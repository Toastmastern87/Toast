#inputlayout
#type vertex
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
    PixelInputType output;

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    output.texCoord = float2((vID << 1) & 2, vID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1);

    return output;
}

#type pixel
#pragma pack_matrix( row_major )

static const float3 Fdielectric = float3(0.04f, 0.04f, 0.04f);
static const float Epsilon = 0.00001f;
static const float PI = 3.14159265359f;
static const uint TRACE_STEPS = 24;
static const float TRACE_MIN_PENUMBRA = 0.004f;
static const float TRACE_HANDOFF_BAND = 0.2f;

cbuffer Camera : register(b0)
{
    matrix worldTranslationMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float far;
    float near;
    float viewportWidth;
    float viewportHeight;
};

cbuffer DirectionalLight : register(b3)
{
    float4x4 lightViewProj[4];
    
    float4 direction; // FROM light -> scene
    
    float4 radiance; // RGB
    
    float SunIntensity;
    float DirectionalLightGain;
    uint CascadeCount;
    float ShadowDistance;
    
    float4 CascadeEnds;
    
    uint CascadeIndex;
    float ConstantBias;
    float SlopeBias;
};

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterWS;
    float PlanetRadius; // Rg
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
    float Altitude;
    float3 BasisLonEast; 
    int NumHeightDetails;
    float3 BasisLonNorth; 
    float _PlanetFramePad3;
    float3 BasisSpinUp; 
    float _PlanetFramePad5;
};

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight; // 0: Rt - Rg (m)
    float RayScaleHeight; // 4
    float MieScaleHeight; // 8
    float OzoneStrength; // 12

    float3 RayleighScattering; // 16
    float MieAnisotropyBroad; // 28

    float3 MieScattering; // 32
    float BakeIBL; // 44

    float3 MieAbsorption; // 48
    uint FaceIndex; // 60

    float3 GroundAlbedo; // 64
    uint StepsTransmittance; // 76

    float3 MieAnisotropy; // 80
    uint StepsMultiScattering; // 92

    float3 MieLobeWeight; // 96
    float _AtmospherePad0; // 108
}

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius; // 0
    int SunDiscToggle; // 4
    float2 _SunDiscPad0; // 8
};

cbuffer LightningBufferSettings : register(b8)
{
    float DiffuseIBLGain;
    float SpecularIBLGain;
    float NightSkyLight;
    float _Pad0;
}

// G-buffer Textures
Texture2D positionTexture               : register(t0); // View-space position
Texture2D normalTexture                 : register(t1); // Encoded normals
Texture2D albedoMetallicTexture         : register(t2); // Albedo RGB and Metallic A
Texture2D roughnessAOTexture            : register(t3); // Roughness R and AO A

// IBL Textures
TextureCube IrradianceTexture           : register(t4);
TextureCube RadianceTexture             : register(t5);
Texture2D SpecularBRDFLUT               : register(t6);

// Atmospheric Scattering Textures
Texture2D<float4> TransmittanceLUT      : register(t7);
Texture2D<float4> GroundIrradianceLUT   : register(t9);

// SSAO Textures
Texture2D SSAOTexture                   : register(t10);

// Shadow Pass Texture
Texture2DArray ShadowDepthTexture       : register(t12);

Texture2D<int> ObjectMaskTexture        : register(t13);

Texture2DArray<float> HeightCube        : register(t16);
Texture2D SceneDepth                    : register(t17);

// Sampler state
SamplerState DefaultSampler             : register(s0);
SamplerState SPBRDFSampler              : register(s1);
SamplerState PointSampler               : register(s2);
SamplerState LinearSampler              : register(s3);
SamplerComparisonState ShadowCmpSampler : register(s4);

#include "TransmittanceLUT.hlsli"
#include "GroundIrradianceLUT.hlsli"
#include "HeightCubeSampling.hlsli"

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2
float ndfGGX(float cosLh, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;

    float denom = (cosLh * cosLh) * (alphaSq - 1.0f) + 1.0f;
    return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
    return cosTheta / (cosTheta * (1.0f - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f; // Epic suggests using this roughness remapping for analytic lights.
    return gaSchlickG1(cosLi, k) * gaSchlickG1(NdotV, k);
}

// Shlick's approximation of the Fresnel factor.
float3 fresnelSchlick(float3 F0, float cosTheta)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float3 fresnelSchlickRoughness(float3 F0, float cosTheta, float roughness)
{
    return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

// Returns number of mipmap levels for specular IBL environment map.
uint queryRadianceTextureLevels()
{
    uint width, height, levels;
    RadianceTexture.GetDimensions(0, width, height, levels);
    return levels;
}

float SunVisibilityAtR(float r, float muS, float Rb)
{
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunDiscRadius, sinThetaH * SunDiscRadius, muS - cosThetaH);
}
//
float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(LinearSampler, TransUV(r, mu, Rb, Rt), 0).rgb;
}

uint SelectCascade(float viewDepth)
{
    // viewDepth should be positive forward distance in view space
    // pick first i where viewDepth <= CascadeEnds[i]
    [unroll]
    for (uint i = 0; i < CascadeCount; ++i)
    {
        if (viewDepth <= CascadeEnds[i])
            return i;
    }
    return CascadeCount - 1;
}

float SampleShadowPCF(uint ci, float2 uv, float depth01, float receiverBias)
{
    // outside = lit
    if (any(uv < 0.0f) || any(uv > 1.0f))
        return 1.0f;

    // Bias the RECEIVER depth (recommended mental model)
    float d = depth01 - receiverBias;

    uint width, height, elements;
    ShadowDepthTexture.GetDimensions(width, height, elements);
    
    float2 texel = float2(1.0f / (float) width, 1.0f / (float) height); // (1.0f/4096.0f, 1.0f/4096.0f) passed in

    // 3x3 PCF
    float sum = 0.0f; // Stars are hidden over the sun's disc and its halo (masks written by the atmosphere pass).
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 o = float2(x, y) * texel;
            sum += ShadowDepthTexture.SampleCmpLevelZero(ShadowCmpSampler, float3(uv + o, (float) ci), d);
        }
    }
    return sum * (1.0f / 9.0f);
}

float3 WorldToPlanetDir(float3 dirWS)
{
    return float3(dot(dirWS, BasisLonEast), dot(dirWS, BasisSpinUp), dot(dirWS, BasisLonNorth));
}

float TerrainHorizonShadow(float3 posWS, float3 planetCenterWS, float3 wSun)
{
    uint width, height, slices, mipCount;
    HeightCube.GetDimensions(0, width, height, slices, mipCount);
    const uint2 size0 = uint2(width, height);

    const float3 pRel = posWS - planetCenterWS;
    const float3 up = pRel / max(length(pRel), 1.0f);

    const float sinSun = dot(up, wSun);
    const float sunElevation = asin(clamp(sinSun, -1.0f, 1.0f));
    float3 toSun = wSun - up * sinSun;
    const float toSunLen = length(toSun);
    if (toSunLen < 1e-4f)
        return 1.0f;
    toSun /= toSunLen;
    
    const float3 upPS = WorldToPlanetDir(up);
    const float3 toSunPS = WorldToPlanetDir(toSun);

    const float R = PlanetRadius;
    const float h0 = LoadHeightBilinear(HeightCube, upPS, 0.0f, size0, mipCount);
    const float meterPerPixel = R * (0.5f * PI / (float)width);

    const float dMin = meterPerPixel;
    const float dMax = max(sqrt(2.0f * R * max(MaxHeight - h0, 0.0f)), 2.0f * dMin);
    const float growth = pow(dMax / dMin, 1.0f / (float) (TRACE_STEPS - 1));

    const float penumbra = max(SunDiscRadius, TRACE_MIN_PENUMBRA);

    float horizon = -1.0f;
    float d = dMin;
    
    [loop]
    for (uint i = 0; i < TRACE_STEPS; ++i)
    {
        const float theta = d / (R + h0);
        float s, c;
        sincos(theta, s, c);
        const float3 dirPS = upPS * c + toSunPS * s;
        
        const float mipF = log2(max(d * (growth - 1.0f), meterPerPixel) / meterPerPixel);
        const float hS = LoadHeightBilinear(HeightCube, dirPS, mipF, size0, mipCount);

        const float sh = sin(0.5f * theta);
        const float x = (R + hS) * s;
        const float z = (hS - h0) - (R + hS) * 2.0f * sh * sh;
        horizon = max(horizon, atan2(z, x));

        if (horizon > sunElevation + penumbra)
            return 0.0f;
        
        d *= growth;
    }
    
    return smoothstep(-penumbra, penumbra, sunElevation - horizon);
}

float3 TerrainSkyIrradiance(float r, float muS, float3 normalWorld, float3 up)
{
    const float Rb = PlanetRadius + min(0.0f, MinHeight);
    const float Rt = PlanetRadius + AtmosphereHeight;
    float3 E = GroundIrradianceLUT.SampleLevel(LinearSampler, GroundIrradianceUV(r, muS, Rb, Rt), 0).rgb;
    float skyFraction = saturate(0.5f + 0.5f * dot(normalWorld, up));
    
    return E * (radiance.rgb * SunIntensity) * skyFraction;
}

float3 DirectionalLightning(float3 F0, float3 NormalWorldSpace, float3 View, float NdotV, float3 albedo, float roughness, float metalness, float3 worldPos, float3 sunDir, float r, float muS)
{   
    float3 L = normalize(-sunDir);
    float3 H = normalize(L + View);
    float NoL = max(0.0f, dot(NormalWorldSpace, L));
    if (NoL <= 0.0f)
        return 0;
    float NoH = max(0.0f, dot(NormalWorldSpace, H)); 

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius);
    
    float3 Tsun = T_to_TOA(r, muS, RbPhys, Rt) * SunVisibilityAtR(r, muS, RbVis);
    
    // Sun radiance (same scalar you use in AP/Sky)
    float3 ESun = radiance * SunIntensity;

    float3 Lradiance = ESun * Tsun; // attenuated, spectrally reddened
    
    float3 F = fresnelSchlick(F0, max(0.0f, dot(H, View)));
    float D = ndfGGX(NoH, roughness);
    float G = gaSchlickGGX(NoL, NdotV, roughness);

    float3 kd = (1.0f - F) * (1.0f - metalness);
    float3 diffuseBRDF = kd * albedo / PI;
    
	// Cook-Torrance
    float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * NoL * NdotV);

    float3 result = (diffuseBRDF + specularBRDF) * Lradiance * NoL;

    return result;
}

float3 IBL(float3 F0, float3 irradiance, float3 specularRadiance, float3 albedo, float roughness, float metalness, float NdotV)
{
    float3 F = fresnelSchlickRoughness(F0, NdotV, roughness);
    float3 kd = (1.0f - F) * (1.0f - metalness);
    float3 diffuseIBL = kd * albedo * irradiance;

    float2 specularBRDF = SpecularBRDFLUT.Sample(SPBRDFSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = specularRadiance * (F * specularBRDF.x + specularBRDF.y);

    return (specularIBL * SpecularIBLGain) + (diffuseIBL * DiffuseIBLGain);
}

struct PixelOutputType
{
    float4 color        : SV_TARGET;
};

// Input structure from vertex shader
struct PixelInputType
{
    float4 position     : SV_POSITION; // Clip-space position
    float2 texCoord     : TEXCOORD0; // Texture coordinates
};

// Pixel shader main function
PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;

    // **1. Sample G-buffer Textures**
    float2 uv = input.texCoord;
    float3 albedo = albedoMetallicTexture.Sample(PointSampler, uv).rgb;
    float3 normal = normalTexture.Sample(PointSampler, uv).rgb;
    normal = normalize(normal * 2.0f - 1.0f); // Convert to [-1, 1]
    float3 posVS = positionTexture.Sample(PointSampler, uv).rgb;
    
    // Reconstruct World Position from View Space
    float4 posWS = mul(float4(posVS, 1.0f), inverseViewMatrix);
    
    // Reconstruct World Normal from View Space
    float3 normalWorld = normalize(mul(normal, (float3x3) inverseViewMatrix));
    
    // Metalness and Roughness
    float metalness = albedoMetallicTexture.Sample(DefaultSampler, uv).a;
    float roughness = roughnessAOTexture.Sample(DefaultSampler, uv).r;
    roughness = max(roughness, 0.05f); // Avoid zero roughness
    
    // Ambient Occlusion from the SSAO texture
    float ao = SSAOTexture.Sample(DefaultSampler, uv).r;
    
    // In View Space, the camera is at the origin (0, 0, 0)
    float3 VWorld = normalize(cameraPosition.xyz - posWS.xyz);
    float NdotV = max(dot(normalWorld, VWorld), 0.05f);

    float3 planetCenterTrueWS = mul(float4(PlanetCenterWS, 1.0f), worldTranslationMatrix).xyz;
    float3 pRel = posWS.xyz - planetCenterTrueWS;
      
    float r_true = length(pRel);
    float3 up = (r_true > 0.0f) ? (pRel / r_true) : BasisRadUp;
    
    float3 wSun = -normalize(direction.xyz); // point -> sun
    float muS = dot(up, wSun);

    // Fresnel reflectance at normal incidence (for metals use albedo color).
    float3 F0 = lerp(Fdielectric, albedo, metalness);
    
    int2 pix = int2(uv * float2(viewportWidth, viewportHeight));
    int isObject = ObjectMaskTexture.Load(int3(pix, 0));
    float depth = SceneDepth.Sample(PointSampler, uv);

    uint ci = SelectCascade(posVS.z);

    // Initialize shadow factor
    float shadowFinal = 1.0f;
    float terrainShadow = 1.0f;
    
    bool isTerrain = (isObject < 0.5f && depth > 1e-12f);

    float3 Lr = normalize(reflect(-VWorld, normalWorld));
    float3 skyIrradiance;
    float3 skySpecular;
    if (isTerrain)
    {
        skyIrradiance = TerrainSkyIrradiance(r_true, muS, normalWorld, up);
        skySpecular = skyIrradiance; // terrain is rough: its reflection is the average sky
    }
    else
    {
        skyIrradiance = IrradianceTexture.Sample(SPBRDFSampler, normalWorld).rgb;
        float mipLevel = roughness * (float) (queryRadianceTextureLevels() - 1);
        skySpecular = RadianceTexture.SampleLevel(SPBRDFSampler, Lr, mipLevel).rgb;
    }

    skyIrradiance += NightSkyLight.xxx;
    skySpecular += NightSkyLight.xxx;
    
    if (isTerrain && dot(normalWorld, wSun) > 0.0f)
        terrainShadow = TerrainHorizonShadow(posWS.xyz, planetCenterTrueWS, wSun);
    
    float4 shadowClip = mul(float4(posWS.xyz, 1.0f), lightViewProj[ci]);
    
    float3 ndc = shadowClip.xyz / shadowClip.w;
    float2 shadowUV = ndc.xy * 0.5f + 0.5f;
    shadowUV.y = 1.0f - shadowUV.y;
    
    float currentDepth = ndc.z;
    
    bool outsideShadow = any(shadowUV < 0.0f) || any(shadowUV > 1.0f) || currentDepth < 0.0f || currentDepth > 1.0f;
    
    float shadow = 1.0f;
    
    if (!outsideShadow)
    {
        float3 Li = normalize(-direction.xyz);
        float NdL = saturate(dot(normalWorld, Li));
        float bias = ConstantBias + SlopeBias * (1.0f - NdL);

        shadow = SampleShadowPCF(ci, shadowUV, currentDepth, bias);
    }
    
    const float csmWeight = 1.0f - smoothstep((1.0f - TRACE_HANDOFF_BAND) * ShadowDistance, ShadowDistance, posVS.z);
    const float csmShadow = lerp(1.0f, shadow, csmWeight);
    const float traceShadow = lerp(1.0f, terrainShadow, 1.0f - csmWeight);
    
    if (CascadeCount == 0) // Shadows disabled
        shadowFinal = 1.0f;
    else
        shadowFinal = min(csmShadow, traceShadow);
    
    // Directional Light Contribution
    float3 directLight = DirectionalLightning(F0, normalWorld, VWorld, NdotV, albedo, roughness, metalness, posWS.xyz, direction.xyz, r_true, muS);
    
    // IBL Contribution
    float3 iblContribution = IBL(F0, skyIrradiance, skySpecular, albedo, roughness, metalness, NdotV);
    
    float3 direct = DirectionalLightGain * directLight * shadowFinal; // no AO

    float3 color = direct + (iblContribution * ao);

    // Output the final color
    output.color = float4(color, 1.0f);
    
    return output;
}