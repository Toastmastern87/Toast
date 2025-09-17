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
Hit IntersectSphereRobust(float3 ro, float3 rd, float R)
{
    // normalize by radius => sphere becomes unit radius
    float invR = rcp(R);
    float3 roN = ro * invR; // O(1)
    float3 rdN = rd; // assume |rd|=1
    
    Hit H;

    // closest approach to center
    float tca = -dot(roN, rdN);
    float d2 = dot(roN, roN) - tca * tca; // O(1)

    // treat tiny overshoot above 1.0 as grazing hit (fp noise)
    if (d2 > 1.0f + 1e-5f)
    {
        H.ok = false;
        H.t0 = H.t1 = 0.0f;
        return H;
    }

    float m = max(1.0f - d2, 0.0f);
    float thc = sqrt(m);

    float t0N = tca - thc; // in "radius units"
    float t1N = tca + thc;

    float Rscale = R; // back to meters
    H.ok = true;
    H.t0 = t0N * Rscale;
    H.t1 = t1N * Rscale;
    return H;
}

float GroundBiasMeters(float Rg)
{
    return max(1.0f, 2e-6f * Rg);
}

Hit IntersectSphere_GrazingSafe(float3 ro, float3 rd, float R)
{
    Hit H;
    H.ok = false;
    H.t0 = H.t1 = 0.0f;
    float Rabs = abs(R);
    if (Rabs <= 0.0f)
        return H;

    // Normalize direction for stable geometry form
    float a = dot(rd, rd);
    if (a <= 0.0f)
        return H;
    float invDirLen = rsqrt(max(a, 1e-30));
    float3 nrd = rd * invDirLen; // |nrd| = 1

    // Use cross-product form in unit-sphere space
    float3 roU = ro / Rabs; // O(1)
    float d2 = dot(cross(nrd, roU), cross(nrd, roU)); // <= ~1 when intersecting

    // Robust tangency handling: allow a tiny overshoot
    // NOTE: keep this the *same value everywhere you use this function*
    const float grazeTol = 5e-5; // ~1e-6..2e-4 are reasonable
    if (d2 > 1.0f + grazeTol)
        return H;

    float tca = -dot(roU, nrd); // along-ray to closest approach (radius units)
    float thc = sqrt(max(1.0f - d2, 0.0f)); // 0 at tangency

    // Convert back to world meters and original rd scale
    float t0 = (tca - thc) * Rabs * invDirLen;
    float t1 = (tca + thc) * Rabs * invDirLen;

    if (t0 > t1)
    {
        float tmp = t0;
        t0 = t1;
        t1 = tmp;
    }
    H.ok = true;
    H.t0 = t0;
    H.t1 = t1;
    return H;
}

float3 ViewDirWSFromUV(float2 uv)
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
        float3 rd = ViewDirWSFromUV(uv);
        float Rg = PlanetRadius;
        float Rt = PlanetRadius + AtmosphereHeight;
        const float RbPhys = PlanetRadius + min(0.0f, MinHeight); // physical floor used for densities
        const float RbHit = RbPhys + GroundBiasMeters(Rg); // use ONLY for intersections

        Hit hatm = IntersectSphere_GrazingSafe(ro, rd, Rt);
        if (!hatm.ok)
            continue;

        float tEnter = max(0.0, hatm.t0);
        float tExitA = max(0.0, hatm.t1);
        Hit hg = IntersectSphere_GrazingSafe(ro, rd, RbHit);
        if (hg.ok && hg.t0 > 0.0)
            tExitA = min(tExitA, hg.t0);

        float depth = SceneDepth.SampleLevel(ClampPoint, uv, 0);
        if (depth > 1e-12)
        {
            float tSurf = ViewDistanceFromDepth(uv, depth);
            maxCand = max(maxCand, tSurf);
        }
    }

    float target = Safety * maxCand;
    target = min(target, MaxAP);

    // atomic max into 1×1 R32_UINT UAV cleared to 0 at frame start
    InterlockedMax(APFarU32[uint2(0, 0)], asuint(target));
}