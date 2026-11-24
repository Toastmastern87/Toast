#type compute
#pragma pack_matrix(row_major)

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
};

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterWS;
    float PlanetRadius; // Rg
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
};

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight;
};

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius; // rad  (e.g. radians(0.2666))
};

Texture2D<float> SceneDepth                 : register(t0);
Texture2D<float4> TransmittanceLUT          : register(t1);

SamplerState ClampLinear                    : register(s0);

RWStructuredBuffer<float4> SunVisibility    : register(u0);

static const float SUN_LIMB_U = 0.55f;
static const uint SAMPLE_COUNT = 64;
static const float GOLDEN_ANGLE = 2.39996323f; // radians

#include "TransmittanceLUT.hlsli"
#include "AtmosphereRays.hlsli"

groupshared float4 gsLight[SAMPLE_COUNT];
groupshared float2 gsScreen[SAMPLE_COUNT];

float2 DiscPoint(uint i)
{
    float r = sqrt(((float) i + 0.5f) / (float)SAMPLE_COUNT);
    float a = (float) i * GOLDEN_ANGLE;
    return r * float2(cos(a), sin(a));
}

float2 DirectionsToUV(float3 dirWS, out bool inFront)
{
    float3 dirVS = mul(dirWS, (float3x3)viewMatrix);
    float4 clip = mul(float4(dirVS, 0.0f), projectionMatrix);
    inFront = clip.w > 1e-6f;
    float2 ndc = clip.xy / max(clip.w, 1e-6f);
    return float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
}

[numthreads(SAMPLE_COUNT, 1, 1)]
void main( uint gi : SV_GroupIndex)
{
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius);
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float3 camRel = cameraPosition.xyz - PlanetCenterWS;
    
    const float3 wSun = -normalize(direction.xyz);
    const float3 helper = (abs(wSun.y) < 0.99f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    const float3 t1 = normalize(cross(helper, wSun));
    const float3 t2 = cross(wSun, t1);
    const float2 p = DiscPoint(gi);
    const float3 dir = normalize(wSun + tan(SunDiscRadius) * (p.x * t1 + p.y * t2));

    const float w = 1.0f - SUN_LIMB_U * (1.0f - sqrt(saturate(1.0f - dot(p, p))));
    
    bool inFront;
    const float2 uv = DirectionsToUV(dir, inFront);
    const bool onScreen = inFront && all(uv >= 0.0f) && all(uv <= 1.0f);
    float visible = 1.0f;
    if (onScreen)
    {
        const uint2 size = uint2((uint) viewportWidth, (uint)viewportHeight);
        const uint2 px = min((uint2) (uv * float2(size)), size - 1u);
        visible = (SceneDepth.Load(int3(px, 0)) <= 1e-12f) ? 1.0f : 0.0f;
    }

    const Hit hPlanet = IntersectSphereGrazingSafe(camRel, dir, RbVis);
    if (hPlanet.ok && hPlanet.t0 > 0.0f)
        visible = 0.0f;

    const float3 T = SampleTransmittance_EyeToSun(camRel, dir, RbPhys, Rt);
    
    gsLight[gi] = float4(w * visible * T, w * visible);
    gsScreen[gi] = float2(w * (onScreen ? 1.0f : 0.0f), w);
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint s = SAMPLE_COUNT / 2; s > 0; s >>= 1)
    {
        if (gi < s)
        {
            gsLight[gi] += gsLight[gi + s];
            gsScreen[gi] += gsScreen[gi + s];
        }
        GroupMemoryBarrierWithGroupSync();

    }
    
    if (gi == 0)
    {
        const float wSum = max(gsScreen[0].y, 1e-6f);
        bool centerInFront;
        const float2 centerUV = DirectionsToUV(wSun, centerInFront);
        SunVisibility[0] = float4(gsLight[0].rgb / wSum, gsLight[0].a / wSum);
        SunVisibility[1] = float4(gsScreen[0].x / wSum, centerUV, centerInFront ? 1.0f : 0.0f);
    }
}