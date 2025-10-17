#type compute
#pragma pack_matrix(row_major)

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

cbuffer FloatingOrigin : register(b7)
{
    float3 WorldOffsetWS;
}

Texture2D<float> SceneDepth : register(t0);
SamplerState ClampPoint : register(s1);

RWTexture2D<uint> APFarU32 : register(u0); // 1x1 UAV

static const float Safety = 1.05f; // e.g. 1.08

// 2×2 taps inside the tile (centers of four quadrants)
static const float2 OFFS[4] =
{
    float2(0.25, 0.25), float2(0.75, 0.25),
    float2(0.25, 0.75), float2(0.75, 0.75)
};
float GroundBiasMeters(float Rg)
{
    return max(1.0f, 2e-6f * Rg);
}

// Ray-sphere intersection in a numerically stable way.
// ro: ray origin (translated-space, meters, relative to planet center)
// rd: ray direction (any length; normalized internally)
// R : sphere radius in meters (e.g., Rt or RbHit; sign ignored)
struct Hit
{
    bool ok;
    float t0, t1;
};

Hit IntersectSphereGrazingSafe(float3 ro, float3 rd, float R)
{
    Hit H = (Hit) 0;

    float Rabs = abs(R);
    if (Rabs <= 0.0f)
        return H;

    // Normalize direction for stable quadratic
    float a = dot(rd, rd);
    if (a <= 0.0f)
        return H;
    float invDirLen = rsqrt(max(a, 1e-30));
    float3 nrd = rd * invDirLen; // |nrd| = 1

    // Scale origin into unit-sphere space: |roU + t*nrd|^2 = 1
    float3 roU = ro / Rabs;

    // Solve t^2 + 2 b t + c = 0, where:
    float b = dot(roU, nrd);
    float c = dot(roU, roU) - 1.0f;

    // Discriminant (with tiny negative allowed for grazing)
    float disc = b * b - c;
    const float grazeTol = 2e-4; // allow slight negatives from FP error
    if (disc < -grazeTol)
        return H;
    disc = max(disc, 0.0f);

    float s = sqrt(disc);
    float t0u = -b - s; // unit-sphere param
    float t1u = -b + s;

    if (t0u > t1u)
    {
        float tmp = t0u;
        t0u = t1u;
        t1u = tmp;
    }

    // Convert back to meters and original rd scale
    H.ok = true;
    H.t0 = t0u * Rabs * invDirLen;
    H.t1 = t1u * Rabs * invDirLen;

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
    
    float3 ro = (cameraPosition.xyz - WorldOffsetWS) - PlanetCenterWS;
    float rCam = length(ro);
    float Rt = PlanetRadius + AtmosphereHeight;
    
    bool cameraInsideAtmosphere = (rCam <= Rt);
    
    float maxFarInsideAtmosphere = 0.0f;

    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        float2 uv = (base + OFFS[k]) / float2(W, H);
        
        float depth = SceneDepth.SampleLevel(ClampPoint, uv, 0);
        if (depth < 1e-12)
            continue;

        float tSurf = ViewDistanceFromDepth(uv, depth);
        
        float3 rd = ViewDirWSFromUV(uv);
        
        Hit hatm = IntersectSphereGrazingSafe(ro, rd, Rt);
        if (!hatm.ok)
            continue;

        float tEnter = max(0.0, hatm.t0);

        float lengthInAtmosphere = cameraInsideAtmosphere ? tSurf : max(0.0f, tSurf - tEnter);
        
        maxFarInsideAtmosphere = max(maxFarInsideAtmosphere, lengthInAtmosphere);
    }

    float target = Safety * maxFarInsideAtmosphere;

    // atomic max into 1×1 R32_UINT UAV cleared to 0 at frame start
    InterlockedMax(APFarU32[uint2(0, 0)], asuint(target));
}