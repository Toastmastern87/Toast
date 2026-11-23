#inputlayout
#type vertex
#pragma pack_matrix(row_major)

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
#pragma pack_matrix(row_major)

#include "BloomFilters.hlsli"

Texture2D SceneHDR : register(t0);
Texture2D BloomTex : register(t1);
StructuredBuffer<float4> SunVisibility : register(t2);

SamplerState LinearClamp : register(s3);

cbuffer Camera : register(b0)
{
    float4x4 worldTranslationMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 inverseViewMatrix;
    float4x4 inverseProjectionMatrix;
    float4 cameraPosition;
    float farZ;
    float nearZ;
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

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius; // rad (only this is read here; the rest of the cbuffer follows it)
};

cbuffer BloomBlendParams : register(b13)
{
    float2 BloomTexelSize;
    float Intensity;
    float Falloff;
};

static const float PI = 3.14159265f;
static const uint BLOOM_LEVELS = 6;
static const float BLOOM_LEVEL_SIGMA[BLOOM_LEVELS] = { 2.12f, 6.84f, 14.70f, 29.89f, 60.01f, 120.14f };
static const float SUN_FP16_SAFE_MAX = 60000.0f;

float3 SunGlare(float2 pixel)
{
    const float4 sunLight = SunVisibility[0];
    const float4 sunCentre = SunVisibility[1];
    if (sunCentre.w < 0.5f || sunLight.a <= 1e-6f)
        return 0.0f.xxx;

    const float2 viewport = float2(viewportWidth, viewportHeight);
    const float2 d = pixel - sunCentre.yz * viewport;
    const float r2 = dot(d, d);
    const float sigmaMax = BLOOM_LEVEL_SIGMA[BLOOM_LEVELS - 1];
    if (r2 > 25.0f * sigmaMax * sigmaMax)
        return 0.0f.xxx;

    const float sinHalf = sin(0.5f * SunDiscRadius);
    const float omegaSun = 4.0f * PI * sinHalf * sinHalf;
    const float3 T = sunLight.rgb / sunLight.a;
    float3 discRGB = radiance.rgb * SunIntensity / omegaSun * T;
    discRGB *= min(1.0f, SUN_FP16_SAFE_MAX / max(max(discRGB.r, max(discRGB.g, discRGB.b)), 1e-6f));

    const float3 forward = normalize(inverseViewMatrix[2].xyz);
    const float cosT = saturate(dot(-normalize(direction.xyz), forward));
    const float pixelAngle = 2.0f / (projectionMatrix[1][1] * viewportHeight); // at the centre of the screen
    const float omegaPixel = pixelAngle * pixelAngle * cosT * cosT * cosT;
    const float3 energy = discRGB * (omegaSun / max(omegaPixel, 1e-12f)) * sunLight.a;

    float psf = 0.0f;
    float weightSum = 0.0f;
    float w = 1.0f;
    [unroll]
    for (uint k = 0; k < BLOOM_LEVELS; ++k)
    {
        const float s2 = BLOOM_LEVEL_SIGMA[k] * BLOOM_LEVEL_SIGMA[k];
        psf += w * exp(-r2 / (2.0f * s2)) / (2.0f * PI * s2);
        weightSum += w;
        w *= Falloff;
    }

    return energy * (psf / weightSum);
}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float3 scene = SceneHDR.SampleLevel(LinearClamp, uv, 0).rgb;
    float3 bloom = BloomTent(BloomTex, LinearClamp, uv, BloomTexelSize);
    
    return float4(lerp(scene, bloom, Intensity) + Intensity * SunGlare(pos.xy), 1.0f);
}