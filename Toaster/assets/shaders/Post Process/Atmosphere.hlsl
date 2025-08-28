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

// ===== Inputs / CBuffers (as provided) ======================================
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
    float4 radiance; // RGB energy
    float multiplier;
};

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterWS;
    float PlanetRadius; // Rg
    float3 BasisTanEast;
    float MaxHeight; // (unused)
    float3 BasisTanNorth;
    float MinHeight; // (unused)
    float3 BasisRadUp;
};

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight; // Rt - Rg
    float RayScaleHeight; // (unused here)
    float MieScaleHeight; // (unused here)
    float MieAnisotropy; // (unused here)
    float3 RayleighScattering; // (unused here)
    float3 MieScattering; // (unused here)
    float3 MieAbsorption; // (unused here)
    float3 GroundAlbedo; // (unused here)
    float OzoneStrength; // (unused here)
    uint StepsTransmittance; // (unused here)
    uint StepsMultiScattering; // (unused here)
    float APFarDynamic; // meters
};

// ===== Textures / Samplers ==================================================
Texture2D<float4> SceneColor : register(t10);
Texture2D<float> SceneDepth : register(t9);
Texture2D<float4> TransmittanceLUT : register(t0); // (not used in composite)
Texture2D<float4> MultiScatterLUT : register(t1); // (not used in composite)
Texture2D<float4> SkyViewLUT : register(t2);
Texture3D<float4> AerialPerspective3D : register(t3);

SamplerState ClampLinear : register(s0);
SamplerState ClampPoint : register(s1);

// ===== Options to match your LUT packing ===================================
#ifndef SKY_FLIP_Y
#define SKY_FLIP_Y 1
#endif

// ===== Fullscreen triangle VS ==============================================
struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// (Re-use all cbuffers/textures/samplers from the vertex section)

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
float ViewDistanceFromDepth(float2 uv, float depth01)
{
    // Guard: if nothing was written to depth (sky), it will be 0 with reversed-Z.
    if (depth01 <= 1e-6f)
        return 0.0f;

    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, depth01, 1.0f);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 posVS = vpos.xyz / max(vpos.w, 1e-6f);
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

#define AP_DEPTH_SLICES 128.0   // must match the 3D AP texture’s Z resolution

float AP_W_Tex_Correct(float d, float APFar, float D)
{
    float s = sqrt(saturate(d / max(APFar, 1e-6f)));
    float z = D * s - 1.0f; // continuous slice index
    float w = (z + 0.5f) / D; // sample at center between neighbors
    return saturate(w);
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
    float4 scene = SceneColor.Sample(ClampPoint, uv);
    float depth = SceneDepth.Sample(ClampPoint, uv); // hardware depth, reversed-Z (sky -> 0)

    bool isSky = depth < 1e-12f ? true : false;
    
    // If no geometry wrote to depth, draw SKY using the precomputed SkyView LUT
    if (isSky)
    {
        float3 wView = ViewDirWS_fromUV(uv);
        float2 skyUV = SkyUVFromViewDir(wView);

        // SkyView already contains radiance * multiplier per your SkyViewCS
        float3 sky = SkyViewLUT.SampleLevel(ClampLinear, skyUV, 0).rgb;
        return float4(max(sky, 0.0f), 1.0f);
    }
    else
    {
        // Geometry pixel: apply aerial perspective up to the actual distance
        float dMeters = ViewDistanceFromDepth(uv, depth); // metric along the ray

        // 3D AP volume at (screenUV, slice)
        float wAP = AP_W_Tex_Correct(dMeters, APFarDynamic, AP_DEPTH_SLICES);
        float4 ap = AerialPerspective3D.SampleLevel(ClampLinear, float3(uv, wAP), 0);
        float3 L_ap = ap.rgb;
        float T_s = saturate(ap.a);

        // Composite in linear HDR:
        // NOTE: let your post chain handle exposure/tonemap later.
        float3 outRGB = scene.rgb * T_s + L_ap;

        return float4(max(outRGB, 0.0f), 1.0f);
    }
}
