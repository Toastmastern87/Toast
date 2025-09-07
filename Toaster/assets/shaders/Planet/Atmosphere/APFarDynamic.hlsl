#type compute
#pragma pack_matrix(row_major)

// ---------- toggles ----------
#ifndef TLUT_USE_OZONE
#define TLUT_USE_OZONE    1      // set 0 to prove ozone is the warm source
#endif
#ifndef TLUT_SAFE_EPS
#define TLUT_SAFE_EPS     1e-5f  // keep μ strictly inside its domain
#endif
#ifndef TLUT_DEBUG_MODE
#define TLUT_DEBUG_MODE   0      // 0=T rgb, 1=tau rgb, 2=vis: μ_min, 3=vis: r
#endif

// ---------- cbuffers ----------
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

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterWS;
    float PlanetRadius; // Rg
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
    float3 BasisLonEast;
    float3 BasisLonNorth;
    float3 BasisSpinUp;
};

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight; // Rt - Rg
    float RayScaleHeight; // Hr
    float MieScaleHeight; // Hm
    float MieAnisotropy; // g
    float3 RayleighScattering; // beta_R (1/m) RGB
    float3 MieScattering; // beta_Ms (1/m) RGB
    float3 MieAbsorption; // beta_Ma (1/m) RGB
    float3 GroundAlbedo;
    float OzoneStrength;
    uint StepsTransmittance; // (unused here)
    uint StepsMultiScattering; // (unused here)
    float APFarDynamic; // camera->max distance for AP (meters)
};

Texture2D<float> SceneDepth : register(t0);
SamplerState ClampPoint : register(s1);

RWTexture2D<uint> APFarU32 : register(u0); // 1x1 UAV

static const float ReliefMeters = 3000.0f; // e.g. 3000.0
static const float Safety = 1.15f; // e.g. 1.08
static const float MinAP = 32000.0f; // 32000
static const float MaxAP = 2000000.0f; // 2000000

struct Hit
{
    bool ok;
    float t0, t1;
};
Hit RaySphere(float3 ro, float3 rd, float R)
{
    float b = dot(ro, rd), c = dot(ro, ro) - R * R;
    float h = b * b - c;
    Hit H;
    H.ok = (h >= 0);
    if (!H.ok)
    {
        H.t0 = H.t1 = 0;
        return H;
    }
    float s = sqrt(h);
    H.t0 = -b - s;
    H.t1 = -b + s;
    return H;
}
float3 ViewDirWS_fromUV(float2 uv)
{
    float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
    float4 clip = float4(ndc, 1, 1);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 dirVS = normalize(vpos.xyz / max(vpos.w, 1e-6));
    return normalize(mul(dirVS, (float3x3) inverseViewMatrix));
}
float ViewDistanceFromDepth(float2 uv, float depth)
{
    if (depth <= 1e-12)
        return 0;
    float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
    float4 clip = float4(ndc, depth, 1);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    return length(vpos.xyz / max(vpos.w, 1e-12));
}
float HorizonDistance(float Rg, float h)
{
    return sqrt(max(0.0, h * h + 2.0 * Rg * h));
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    // Use your AP XY size (match the 3D AP volume’s width/height)
    uint W = 192, H = 108;
    if (tid.x >= W || tid.y >= H)
        return;

    float2 base = float2(tid.xy);

    // 2×2 taps inside the tile (centers of four quadrants)
    static const float2 OFFS[4] =
    {
        float2(0.25, 0.25), float2(0.75, 0.25),
        float2(0.25, 0.75), float2(0.75, 0.75)
    };

    float maxCand = 0.0;

    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        float2 uv = (base + OFFS[k]) / float2(W, H);

        float3 ro = cameraPosition.xyz - PlanetCenterWS;
        float3 rd = ViewDirWS_fromUV(uv);
        float Rg = PlanetRadius;
        float Rt = PlanetRadius + AtmosphereHeight;

        Hit hatm = RaySphere(ro, rd, Rt);
        if (!hatm.ok)
            continue;

        float tEnter = max(0.0, hatm.t0);
        float tExitA = max(0.0, hatm.t1);
        Hit hg = RaySphere(ro, rd, Rg);
        if (hg.ok && hg.t0 > 0.0)
            tExitA = min(tExitA, hg.t0);

        float depth = SceneDepth.SampleLevel(ClampPoint, uv, 0);
        if (depth > 1e-12)
        {
            float tSurf = ViewDistanceFromDepth(uv, depth);
            float cand = clamp(tSurf, tEnter, tExitA);
            maxCand = max(maxCand, cand);
        }
    }

    float target = Safety * maxCand;
    target = min(target, MaxAP);

    // atomic max into 1×1 R32_UINT UAV cleared to 0 at frame start
    InterlockedMax(APFarU32[uint2(0, 0)], asuint(target));
}