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
Texture2D<float4> SceneColor : register(t10);
Texture2D<float> SceneDepth : register(t9);
Texture2D<float4> TransmittanceLUT : register(t0); // (not used in composite)
Texture2D<float4> MultiScatterLUT : register(t1); // (not used in composite)
Texture2D<float4> SkyViewLUT : register(t2);
Texture3D<float4> AerialPerspective3D : register(t3);
Texture2D<float4> positionTexture : register(t4);

SamplerState ClampLinear : register(s0);
SamplerState ClampPoint : register(s1);

// ===== Options to match your LUT packing ===================================
#ifndef SKY_FLIP_Y
#define SKY_FLIP_Y 1
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
float APWFromDistance(float distance)
{
    if (APFarDynamic <= 1e-12f)
        return 1.0f; // degenerate: sample last slice
    return sqrt(saturate(distance / APFarDynamic));
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
        
        // Reconstruct World Position from the G-buffer position texture
        float3 posVS = positionTexture.Sample(ClampPoint, uv).rgb; 
        float4 posWS4 = mul(float4(posVS, 1.0f), inverseViewMatrix);
        float3 posWS = posWS4.xyz;
        
        float tSurf = max(0.0f, dot(posWS - camWS, wView));
        
        float tSample = min(tSurf, tExitA);
        
        float wAP = APWFromDistance(tSample);

        float4 ap = AerialPerspective3D.SampleLevel(ClampLinear, float3(uv, wAP), 0);
        
        float3 betaExt = RayleighScattering + MieScattering + MieAbsorption; // 1/m
        float betaAvg = (betaExt.r + betaExt.g + betaExt.b) * (1.0 / 3.0);
        float3 k = betaExt / max(betaAvg, 1e-9);

        // Trgb ≈ A^(betaExt / betaAvg)
        float3 Trgb = pow(ap.a.xxx, k);

        float3 outRGB = colorPreAtmos * ap.a + ap.rgb;
        return float4(outRGB, ap.a);
    }
}
