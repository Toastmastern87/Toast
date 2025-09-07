#inputlayout
#type vertex
#pragma pack_matrix(row_major)

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};
VSOut main(uint vID : SV_VertexID)
{
    VSOut o;
    o.uv = float2((vID << 1) & 2, vID & 2);
    o.pos = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

#type pixel
#pragma pack_matrix(row_major)

// ===== CBuffers =============================================================
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
    float4x4 lightViewProj;
    float4 direction; // FROM light -> scene
    float4 radiance; // RGB
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

// ===== Textures / Samplers ==================================================
Texture2D<float4> TransmittanceLUT : register(t0); // (not used in composite)
Texture2D<float4> MultiScatterLUT : register(t1); // (not used in composite)
Texture2D<float4> SkyViewLUT : register(t2);
Texture3D<float4> AerialPerspective3D : register(t3);
Texture2D<float4> positionTexture : register(t4);
Texture2D<float> SceneDepth : register(t9);
Texture2D<float4> SceneColor : register(t10);

SamplerState ClampLinear : register(s0);
SamplerState ClampPoint : register(s1);

// ===== Options to match your LUT packing ===================================
#ifndef SKY_FLIP_Y
#define SKY_FLIP_Y 1
#endif

#ifndef AP_Z_GAMMA
#define AP_Z_GAMMA 1.6f   // try 1.5–1.8; 1.6 is a good start
#endif

// ===== Math helpers =========================================================
static const float PI = 3.14159265358979323846f;

float3 OrthonormalizeUpEastNorth(float3 upIn, float3 eastIn, float3 northIn,
                                 out float3 up, out float3 east, out float3 north)
{
    up = normalize(upIn);
    east = normalize(eastIn - up * dot(eastIn, up));
    north = normalize(northIn - up * dot(northIn, up));
    // Rebuild exact orthonormal right-handed basis
    east = normalize(cross(north, up));
    north = normalize(cross(up, east));
    return up; // (returning up just to avoid warnings)
}

// Unproject: view ray direction in WORLD space (unit length)
float3 ViewDirWS_fromUV(float2 uv)
{
    // NDC in D3D: x,y in [-1,1], z in [0,1]
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, 1.0f, 1.0f);

    // To view space (point on far plane direction)
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 dirVS = normalize(vpos.xyz / max(vpos.w, 1e-6f));

    // To world space (rotation only)
    float3 dirWS = normalize(mul(dirVS, (float3x3) inverseViewMatrix));
    return dirWS;
}

// Reconstruct metric distance along the camera ray from hardware depth.
// Works for reversed-Z because the inverseProjectionMatrix encodes that mapping.
float ViewDistanceFromDepth(float2 uv, float depth)
{
    // Guard: if nothing was written to depth (sky), it will be 0 with reversed-Z.
    if (depth <= 1e-12f)
        return 0.0f;

    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, depth, 1.0f);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 posVS = vpos.xyz / max(vpos.w, 1e-12f);
    return length(posVS); // meters
}

// Inverse of SkyViewCS's LatitudeFromV() packing.
// Given true latitude in [-pi/2, pi/2], return v in [0,1] with optional flip.
float VFromLatitude(float lat)
{
    float s = (lat >= 0.0f) ? 1.0f : -1.0f;
    float a = sqrt(saturate((2.0f * abs(lat)) / PI)); // a in [0,1]
    float v = 0.5f + 0.5f * s * a;
#if SKY_FLIP_Y
    v = 1.0f - v;
#endif
    return saturate(v);
}

// Inverse of SkyViewCS's LongitudeFromU(): u in [0,1] from lon in [-pi, pi].
float UFromLongitude(float lon)
{
    float u = (lon + PI) / (2.0f * PI);
    // Wrap safely to [0,1)
    return frac(u);
}

// Convert world-space view direction to (u,v) for SkyViewLUT
float2 SkyUVFromViewDir(float3 wView)
{
    // Build the same basis SkyViewCS used
    float3 up, east, north;
    OrthonormalizeUpEastNorth(BasisRadUp, BasisTanEast, BasisTanNorth, up, east, north);

    // True spherical angles relative to that local frame
    float xE = dot(wView, east);
    float xN = dot(wView, north);
    float xU = dot(wView, up);

    float lon = atan2(xN, xE); // [-pi, pi]
    float lat = asin(clamp(xU, -1.0f, 1.0f)); // [-pi/2, pi/2]

    float u = UFromLongitude(lon);
    float v = VFromLatitude(lat);
    return float2(u, v);
}

struct Hit
{
    bool ok;
    float t0, t1;
};
Hit IntersectSphere(float3 ro, float3 rd, float R)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - R * R;
    float h = b * b - c;
    Hit H;
    H.ok = (h >= 0.0f);
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

// AP 3D volume uses quadratic packing: d(z) = APFarDynamic * (z/D)^2  ->  z/D = sqrt(d/APFarDynamic).
// So normalized W coordinate for sampling is:
float APWFromDistance(float tSample, float t0Seg, float Lseg)
{
    if (Lseg <= 1e-9f)
        return 1.0f;
    
    float w = saturate((tSample - t0Seg) / Lseg);
    return w;
}

// hash-based blue-ish noise in [0,1)
float hash21(uint2 p, uint frameIndex)
{
    uint n = p.x * 0x1f123bb5u ^ p.y * 0x3ad24e1bu ^ frameIndex * 0x9e3779b9u;
    n ^= (n >> 16);
    n *= 0x7feb352du;
    n ^= (n >> 15);
    n *= 0x846ca68bu;
    n ^= (n >> 16);
    return (n & 0x00FFFFFFu) * (1.0 / 16777216.0); // 24-bit to float
}
// cheap 2D hash you already have; reuse hash21
float2 Rand2(uint2 p, uint s)
{
    return float2(hash21(p, s), hash21(p.yx ^ uint2(0x4b1d2fu, 0xa7c5d1u), s));
}

// ===== PS ===================================================================
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    const float2 uv = i.uv;

    // Fetch scene
    float3 colorPreAtmos = SceneColor.Sample(ClampPoint, uv);
    float depth = SceneDepth.Sample(ClampPoint, uv); // hardware depth, reversed-Z (sky -> 0)

    // If no geometry wrote to depth, draw SKY using the precomputed SkyView LUT
    if (depth <= 1e-12f)
    {
        float3 wView = ViewDirWS_fromUV(uv);
        float2 skyUV = SkyUVFromViewDir(wView);

        // SkyView already contains radiance * multiplier per your SkyViewCS
        float3 sky = SkyViewLUT.SampleLevel(ClampLinear, skyUV, 0).rgb;
        return float4(max(sky, 0.0f), 1.0f);
    }
    else
    {
        float3 camWS = cameraPosition.xyz;
        float3 ro = camWS - PlanetCenterWS;
        float3 wView = ViewDirWS_fromUV(uv); // unit
        
        float Rg = PlanetRadius;
        float Rt = PlanetRadius + AtmosphereHeight;
        
        // TOA segment
        Hit hitAtm = IntersectSphere(ro, wView, Rt);
        float tEnter = hitAtm.ok ? max(0.0f, hitAtm.t0) : 1e30f;
        float tExitA = hitAtm.ok ? max(0.0f, hitAtm.t1) : 0.0f;
        
        Hit hitG = IntersectSphere(ro, wView, Rg);
        if (hitG.ok && hitG.t0 > 0.0f)
            tExitA = min(tExitA, hitG.t0);
              
        float tSurf = ViewDistanceFromDepth(uv, depth);
        
        float tSample = clamp(tSurf, tEnter, tExitA);
        
        float t0Seg = max(tEnter, 0.0f);
        float t1Seg = min(tExitA, t0Seg + APFarDynamic);
        float Lseg = max(t1Seg - t0Seg, 1e-6f);
      
        
        // fraction along real distance
        float s = saturate((tSample - t0Seg) / Lseg);

        // invert gamma packing: s → u
        float u = pow(s, 1.0f / AP_Z_GAMMA);

        // address slice **centers** then (optionally) jitter
        uint Wd, Hd, Dd;
        AerialPerspective3D.GetDimensions(Wd, Hd, Dd);
        float wAP = u * ((Dd - 1.0f) / Dd) + (0.5f / Dd);

        // W dither (±½ slice)
        float nW = hash21(uint2(i.pos.xy), 0);
        float wJitter = (nW - 0.5f) / float(Dd);
        float wAPj = clamp(wAP + wJitter, 0.5f / float(Dd), 1.0f - 0.5f / float(Dd));

        // XY dither (±½ texel in AP XY)
        float2 texelAP = 1.0 / float2(Wd, Hd);
        float2 n2 = Rand2(uint2(i.pos.xy), 0);
        float2 uvJ = clamp(uv + (n2 - 0.5) * texelAP, 0.5 * texelAP, 1.0 - 0.5 * texelAP);

        // final sample: TRILINEAR
        float4 ap = AerialPerspective3D.SampleLevel(ClampLinear, float3(uvJ, wAPj), 0);
        float tau = max(ap.a, 0.0f);
        
        float3 betaExt = RayleighScattering + MieScattering + MieAbsorption; // 1/m
        float betaAvg = (betaExt.r + betaExt.g + betaExt.b) * (1.0f / 3.0f);
        float3 k = betaExt / max(betaAvg, 1e-9);

        // Trgb ≈ A^(betaExt / betaAvg)
        float3 Trgb = exp(-tau * k);

        float3 outRGB = colorPreAtmos * Trgb + ap.rgb;
        return float4(outRGB, max(Trgb.r, max(Trgb.g, Trgb.b)));
    }
}