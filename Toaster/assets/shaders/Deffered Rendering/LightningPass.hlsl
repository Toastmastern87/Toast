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

static const float3 Fdielectric = 0.04f;
static const float Epsilon = 0.00001f;
static const float PI = 3.141592f;

cbuffer Camera : register(b0)
{
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float far;
    float near;
};

cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction;
    float4 radiance;
    float multiplier;
};

cbuffer Atmosphere : register(b4)
{
    float radius;
    float minAltitude;
    float maxAltitude;
    float atmosphereHeight;
    float mieAnisotropy;
    float rayScaleHeight;
    float mieScaleHeight;
    float3 rayBaseScatteringCoefficient;
    float mieBaseScatteringCoefficient;
    float3 planetCenter;
    int atmosphereToggle;
    int numInScatteringPoints;
    int numOpticalDepthPoints;
    int sunDiscToggle;
    float sunDiscRadius;
    float sunGlowIntensity;
    float sunEdgeSoftness;
    float sunGlowSize;
    int useDepth;
};

// G-buffer Textures
Texture2D positionTexture           : register(t0); // View-space position
Texture2D normalTexture             : register(t1); // Encoded normals
Texture2D albedoMetallicTexture     : register(t2); // Albedo RGB and Metallic A
Texture2D roughnessAOTexture        : register(t3); // Roughness R and AO A

// IBL Textures
TextureCube IrradianceTexture       : register(t4);
TextureCube RadianceTexture         : register(t5);
Texture2D SpecularBRDFLUT           : register(t6);

// Shadow Pass Texture
Texture2D ShadowDepthTexture        : register(t12);
Texture2D ObjectMaskTexture         : register(t13);

// Sampler state
SamplerState defaultSampler         : register(s0);
SamplerState spBRDFSampler          : register(s1);

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
            PrefilteredColor += IrradianceTexture.Sample(defaultSampler, L).rgb * NoL;
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

float3 DirectionalLightning(float3 F0, float3 Normal, float3 View, float NdotV, float3 albedo, float roughness, float metalness, float3 sunDir)
{   
    float3 result = float3(0.0f, 0.0f, 0.0f);

    float3 Li = normalize(-sunDir);
    float3 Lradiance = radiance * multiplier;
    float3 Lh = normalize(Li + View);

	// Calculate angles between surface normal and various light vectors.
    float cosLi = max(0.0f, dot(Normal, Li));
    float cosLh = max(0.0f, dot(Normal, Lh));

    float3 F = fresnelSchlick(F0, max(0.0f, dot(Lh, View)));
    float D = ndfGGX(cosLh, roughness);
    float G = gaSchlickGGX(cosLi, NdotV, roughness);

    float3 kd = lerp(float3(1.0f, 1.0f, 1.0f) - F, albedo, metalness);
    float3 diffuseBRDF = (kd * albedo) / PI;

	// Cook-Torrance
    float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * cosLi * NdotV);

    result += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;

    return result;
}

float3 IBL(float3 F0, float3 Lr, float3 Normal, float3 View, float NdotV, float3 albedo, float roughness, float metalness, float cosLo)
{
    float3 irradiance = IrradianceTexture.Sample(defaultSampler, Normal).rgb;
    float3 F = fresnelSchlick(F0, cosLo);
    float3 kd = lerp(float3(1.0f - F), float3(0.0f, 0.0f, 0.0f), (1.0f - metalness));
    float3 diffuseIBL = kd * albedo * irradiance;

    uint specularTextureLevels = queryRadianceTextureLevels();
	//float NoV = clamp(NdotV, 0.0f, 1.0f);
	//float3 R = 2.0f * dot(View, Normal) * Normal - View;
    float3 specularIrradiance = RadianceTexture.SampleLevel(defaultSampler, Lr, roughness * specularTextureLevels).rgb;

    float2 specularBRDF = SpecularBRDFLUT.Sample(spBRDFSampler, float2(cosLo, roughness)).rg;
    float3 specularIBL = specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);

    return diffuseIBL + specularIBL;
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
    float3 albedo = albedoMetallicTexture.Sample(defaultSampler, uv).rgb;
    float3 normal = normalTexture.Sample(defaultSampler, uv).rgb;
    normal = normalize(normal * 2.0f - 1.0f); // Convert to [-1, 1]
    float3 position = positionTexture.Sample(defaultSampler, uv).rgb;
    
    // Reconstruct World Position from View Space
    float4 positionWorld = mul(float4(position, 1.0f), inverseViewMatrix);
    float3 worldPos = positionWorld.xyz / positionWorld.w;
    
    // Metalness and Roughness
    float metalness = albedoMetallicTexture.Sample(defaultSampler, uv).a;
    float roughness = roughnessAOTexture.Sample(defaultSampler, uv).r;
    roughness = max(roughness, 0.05f); // Avoid zero roughness
    
    // Ambient Occlusion to 1.0f but to be implemented in updates in the future.
    float ao = 1.0f;
    
    // In View Space, the camera is at the origin (0, 0, 0)
    float3 V = normalize(-position); 
    float NdotV = max(dot(normal, V), 0.0f);
    
    // Fresnel reflectance at normal incidence (for metals use albedo color).
    float3 F0 = lerp(Fdielectric, albedo, metalness);
    
    // Recalculate sun direction to view space
    float3 directionVS = normalize(mul(direction.xyz, (float3x3)viewMatrix));
    
    // **1. Normal Offset Biasing**
    // Offset the world position along the normal to reduce self-shadowing artifacts
    float normalOffsetScale = 5.0f; // Adjust based on your scene's scale
    float3 offsetPosition = worldPos + normal * normalOffsetScale;

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
        float3 Li = normalize(-directionVS);
        float bias = max(biasMultiplier * (1.0f - dot(normal, Li)), minBias);

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
                    float sampledDepth = ShadowDepthTexture.Sample(defaultSampler, sampleUV).r;

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
    float3 lightContribution = DirectionalLightning(F0, normal, V, NdotV, albedo, roughness, metalness, directionVS) * shadow;
    
    // IBL Contribution
    float3 Lr = reflect(V, normal);
    float3 iblContribution = IBL(F0, Lr, normal, V, NdotV, albedo, roughness, metalness, NdotV);
    
    float3 ambient = float3(0.005f, 0.005f, 0.005f);

    output.color = float4((ambient + lightContribution + iblContribution) * ao, 1.0f);

    return output;
}