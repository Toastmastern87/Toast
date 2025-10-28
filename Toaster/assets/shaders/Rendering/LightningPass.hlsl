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
static const float MU_EPS = 8e-4;

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
    matrix lightViewProj;
    float4 direction;
    float4 radiance;
    float SunIntensity;
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
};

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight; // Rt - Rg
    float RayScaleHeight;
    float MieScaleHeight;
    float3 RayleighScattering;
    float3 MieScattering;
    float3 MieAbsorption;
    float3 GroundAlbedo;
    float3 MieAnisotropy;
    float OzoneStrength;
    uint StepsTransmittance;
    uint StepsMultiScattering;
    float APFarDynamic;
};

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius;
    float SunEdgeSoftness; // rad  (soft rim width)
    int SunDiscToggle; // 0=off, 1=on
    float SpaceDiscBrightnessScale; // unitless scale, e.g. 1.30
    
    float3 SunDiscWhite;
    float AirHaloIntensity; // 0..~0.6 (was HaloStrength_Ground, e.g. 0.28)
    
    float3 WarmTint; // e.g. float3(1.00, 0.92, 0.78)    
    float AirHaloStartFrac; // 0..1   (was InAirStart, e.g. 0.15)
    
    float AirHaloFalloffPow; // curve (was InAirPow, e.g. 1.10)
    float HorizonRefractionDeg; // deg (was RefracCenterDeg, e.g. 0.83)
    float TwilightBlendDeg; // deg (was TwilightExtraDeg, e.g. 1.5)
    float SpaceHaloWidthDeg; // deg (was SpaceHaloSigmaDeg, e.g. 0.8)
    
    float SpaceHaloIntensity; // 0.01..0.10 (was SpaceHaloGain, e.g. 0.04)
    float SpaceHaloCutoffDeg; // deg (was SpaceHaloCutoffDeg, e.g. 6.0)
    float SunIrradiance;
};

cbuffer StarsParams : register(b7)
{
    float StarNits; // e.g. 600.0 (display-space peak for brightest texel)
    float DayFadeStartDeg; // start hiding stars above horizon (e.g. +2.0)
    float DayFadeEndDeg; // fully hidden by (e.g. 0.0 or -2.0)
    float TwilightStartDeg; // start appearing (e.g. 0.0)
    
    float TwilightEndDeg; // fully visible by (e.g. -6.0)
    float SpaceFadeStart; // altitude norm where space visibility starts (0..1), e.g. 0.85
    float SpaceFadeEnd; // fully visible by (0..1), e.g. 0.98
    float GlareInnerDeg; // sun glare inner angle (e.g. 5.0)
    
    float GlareOuterDeg; // sun glare outer angle (e.g. 12.0)
    float3 NightAmbient;
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
Texture2D<float4> MultiScatterLUT       : register(t8);

// SSAO Textures
Texture2D SSAOTexture                   : register(t10);

// Shadow Pass Texture
Texture2D ShadowDepthTexture            : register(t12);
Texture2D ObjectMaskTexture             : register(t13);

// Sampler state
SamplerState DefaultSampler             : register(s0);
SamplerState SPBRDFSampler              : register(s1);
SamplerState PointSampler               : register(s2);
SamplerState LinearSampler              : register(s3);

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

float GeometrySchlickGGX(float NdotV, float roughness)

{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float nom = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
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

// ---------------------------------------------------------------------------------------------------
// The following code (from Unreal Engine 4's paper) shows how to filter the environment map
// for different roughnesses. This is mean to be computed offline and stored in cube map mips,
// so turning this on online will cause poor performance
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N)
{
    float a = roughness * roughness;
    float Phi = 2.0f * PI * Xi.x;
    float CosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float SinTheta = sqrt(1.0f - CosTheta * CosTheta);
    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    float3 UpVector = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
	// Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

float3 PrefilterEnvMap(float roughness, float3 R)
{
    float TotalWeight = 0.0;
    float3 N = R;
    float3 V = R;
    float3 PrefilteredColor = float3(0.0f, 0.0f, 0.0f);
    int NumSamples = 1024;
    for (int i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = 2.0f * dot(V, H) * H - V;
        float NoL = clamp(dot(N, L), 0.0f, 1.0f);
        if (NoL > 0)
        {
            PrefilteredColor += IrradianceTexture.Sample(DefaultSampler, L).rgb * NoL;
            TotalWeight += NoL;
        }
    }
    return PrefilteredColor / TotalWeight;
}

// Returns number of mipmap levels for specular IBL environment map.
uint queryRadianceTextureLevels()
{
    uint width, height, levels;
    RadianceTexture.GetDimensions(0, width, height, levels);
    return levels;
}

float2 TransUV(float r, float mu, float RbPhys, float Rt)
{
    float rNorm = (r - RbPhys) / max(Rt - RbPhys, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (RbPhys * RbPhys) / (r * r)));
    mu = clamp(mu, muMin + MU_EPS, 1.0f - MU_EPS);
    return float2((mu - muMin) / (1.0f - muMin), saturate(rNorm));
}

float SunVisibilityAtR(float r, float muS, float Rb)
{
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunDiscRadius, sinThetaH * SunDiscRadius, muS - cosThetaH);
}

float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(PointSampler, TransUV(r, mu, Rb, Rt), 0).rgb;
}

float sstep(float a, float b, float x)
{
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0 - 2.0 * t);
}

// Compute sun altitude in degrees
float SunAltitudeDeg(float3 camPosWS, float3 planetCenterWS, float3 lightDirFromLight)
{
    float3 wSun = -normalize(lightDirFromLight); // TO sun
    float3 up = normalize(camPosWS - planetCenterWS); // camera "up"
    float mu = clamp(dot(up, wSun), -1.0, 1.0);
    return degrees(asin(mu)); // +90 zenith, 0 horizon, negative at night
}

float4 SamplePsiMS4(float r, float muS, float Rg, float Rt)
{
    float thetaS = acos(clamp(muS, -1.0f, 1.0f));
    float u = thetaS / PI;
    float v = 1.0f - saturate((r - Rg) / max(Rt - Rg, 1e-6f)); // MS_FLIP_Y=1
    return MultiScatterLUT.SampleLevel(LinearSampler, float2(u, v), 0);
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

float3 IBL(float3 F0, float3 Lr, float3 NormalWorldSpace, float3 albedo, float roughness, float metalness, float NdotV)
{
    float3 irradiance = IrradianceTexture.Sample(SPBRDFSampler, NormalWorldSpace).rgb;

    // Correct Fresnel term using NdotV
    float3 F = fresnelSchlickRoughness(F0, NdotV, roughness);

    // Correct kd calculation
    float3 kd = (1.0f - F) * (1.0f - metalness);
    float3 diffuseIBL = kd * albedo * irradiance;

    uint specularTextureLevels = queryRadianceTextureLevels();
    float mipLevel = roughness * (float) (specularTextureLevels - 1);
    float3 specularIrradiance = RadianceTexture.SampleLevel(SPBRDFSampler, Lr, mipLevel).rgb;

    // Use NdotV in BRDF LUT sampling
    float2 specularBRDF = SpecularBRDFLUT.Sample(SPBRDFSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = specularIrradiance * (F * specularBRDF.x + specularBRDF.y);

    return specularIBL + diffuseIBL;
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
    float3 V = normalize(-posWS.xyz);
    float3 VWorld = normalize(cameraPosition.xyz - posWS.xyz);
    float NdotV = max(dot(normalWorld, VWorld), 0.05f);
    
    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    float3 pRel = posWS.xyz - PlanetCenterWS;
      
    float r_true = length(pRel);
    float3 up = (r_true > 0.0f) ? (pRel / r_true) : BasisRadUp;

    float3 Esun = radiance.rgb * SunIntensity; // radiance
    
    float3 wSun = normalize(-direction.xyz); // point -> sun
    float muS = dot(up, wSun);
    
    float4 Psi4 = SamplePsiMS4(r_true, muS, RbPhys, Rt);
    float3 msIrr = Psi4.rgb * Esun;
    
    // Fresnel reflectance at normal incidence (for metals use albedo color).
    float3 F0 = lerp(Fdielectric, albedo, metalness);
    float3 Fv = fresnelSchlick(F0, NdotV);
    float3 kd = (1.0f - Fv) * (1.0f - metalness);
    
    float3 Lo_sky = kd * (albedo / PI) * msIrr;
    Lo_sky *= ao;
    
    // Recalculate sun direction to view space
    float3 directionVS = normalize(mul(direction.xyz, (float3x3)viewMatrix));
    
    // **1. Normal Offset Biasing**
    // Offset the world position along the normal to reduce self-shadowing artifacts
    float normalOffsetScale = 5.0f; // Adjust based on your scene's scale
    float3 offsetPosition = posWS.xyz + normalWorld * normalOffsetScale;

    // Transform the offset position to Light's Clip Space
    float4 pixelPosLightSpace = mul(float4(offsetPosition, 1.0f), lightViewProj);
    pixelPosLightSpace /= pixelPosLightSpace.w; // Perspective divide

    // Convert from Clip Space [-1,1] to UV Space [0,1]
    float2 shadowUV = pixelPosLightSpace.xy * 0.5f + 0.5f;
    shadowUV.y = 1.0f - shadowUV.y; // Flip Y-coordinate
    float currentDepth = pixelPosLightSpace.z * 0.5f + 0.5f;

    // Check if 'shadowUV' is within [0,1]
    bool outsideShadowMap = (shadowUV.x < 0.0f || shadowUV.x > 1.0f || shadowUV.y < 0.0f || shadowUV.y > 1.0f);

    // Initialize shadow factor
    float shadow = 1.0f;

    if (!outsideShadowMap)
    {
        // **2. Dynamic Bias Based on Surface Slope**
        // Calculate bias based on normal and light direction to reduce self-shadowing
        float biasMultiplier = 0.5f; // Adjust based on your scene's scale
        float minBias = 0.1f; // Minimum bias to prevent bias from being too small
        float3 Li = normalize(-direction.xyz);
        float bias = max(biasMultiplier * (1.0f - dot(normalWorld, Li)), minBias);

        // **3. Optimized Percentage Closer Filtering (PCF)**
        int samples = 4; // 4x4 samples for a balance between quality and performance
        float2 texelSize = 1.0f / float2(8192.0f, 8192.0f); // Shadow map resolution

        float shadowSum = 0.0f;
        int sampleCount = 0;

        // PCF sampling loop
        for (int x = -samples / 2; x <= samples / 2; x++)
        {
            for (int y = -samples / 2; y <= samples / 2; y++)
            {
                float2 offset = float2(x, y) * texelSize;
                float2 sampleUV = shadowUV + offset;

            // Check if 'sampleUV' is within [0,1]
                if (sampleUV.x >= 0.0f && sampleUV.x <= 1.0f && sampleUV.y >= 0.0f && sampleUV.y <= 1.0f)
                {
                    float sampledDepth = ShadowDepthTexture.Sample(DefaultSampler, sampleUV).r;

                // Adjusted depth comparison with bias
                    if (currentDepth <= sampledDepth + bias || sampledDepth == 0.0f)
                    {
                        shadowSum += 1.0f;
                    }

                    sampleCount++;
                }
            }
        }

        // Average the shadow factor
        shadow = shadowSum / sampleCount;
    }
    
    // Directional Light Contribution
    float3 lightContribution = DirectionalLightning(F0, normalWorld, VWorld, NdotV, albedo, roughness, metalness, posWS.xyz, direction.xyz, r_true, muS) * shadow;
    
    // IBL Contribution
    float3 Lr = reflect(-VWorld, normalWorld);
    float3 iblContribution = IBL(F0, Lr, normalWorld, albedo, roughness, metalness, NdotV);

    float3 finalShading = (lightContribution + iblContribution + Lo_sky);

    // Output the final color
    output.color = float4(finalShading, 1.0f);
    
    return output;
}