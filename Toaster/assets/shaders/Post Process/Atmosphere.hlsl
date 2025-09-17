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

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius;
    float SunEdgeSoftness;
    float SunGlowSize;
    float SunGlowIntensity;
    int SunDiscToggle; // 1=on, 0=off
};

// ===== Textures / Samplers ==================================================
Texture2D<float4> TransmittanceLUT : register(t0); // (not used in composite)
Texture2D<float4> MultiScatterLUT : register(t1); // (not used in composite)
Texture2D<float4> SkyViewLUT : register(t2);
Texture3D<float4> AerialPerspective3D : register(t3);
Texture2D<float4> positionTexture : register(t4);
Texture2D<uint> APFarU32 : register(t5);
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
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

void BuildSkyBasis(float3 camWS, float3 planetCenter, float3 spinUpWS, out float3 up, out float3 east, out float3 north)
{
    up = normalize(camWS - planetCenter); // true radial up at camera
    float3 spinT = spinUpWS - up * dot(spinUpWS, up); // remove vertical
    float len2 = max(dot(spinT, spinT), 1e-20f);
    float3 northHint = spinT * rsqrt(len2); // tangent north hint

    east = normalize(cross(northHint, up)); // exact orthonormal
    north = normalize(cross(up, east)); // re-derive north
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
    float3 camWS = cameraPosition.xyz;
    
    // Build the same basis SkyViewCS used
    float3 up, east, north;
    BuildSkyBasis(camWS, PlanetCenterWS, BasisSpinUp, up, east, north);

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

struct FovSlice
{
    float uL, uR;
    bool wraps;
};
bool InSlice(float u, FovSlice s)
{
    return s.wraps ? (u >= s.uL || u <= s.uR) : (u >= s.uL && u <= s.uR);
}
float SliceU(float u, FovSlice s)
{
    if (!s.wraps)
        return (u - s.uL) / max(1e-6, (s.uR - s.uL));
    float len = (1 - s.uL) + s.uR;
    float t = (u >= s.uL) ? (u - s.uL) : ((1 - s.uL) + u);
    return t / max(1e-6, len);
}
FovSlice MakeSlice(float uL, float uR)
{
    uL = frac(uL + 1);
    uR = frac(uR + 1);
    float f = uR - uL;
    if (f < 0)
        f += 1;
    FovSlice s;
    if (f <= 0.5)
    {
        s.uL = uL;
        s.uR = uR;
        s.wraps = false;
    }
    else
    {
        s.uL = uR;
        s.uR = uL;
        s.wraps = true;
    }
    return s;
}

FovSlice GetSkyViewFOVSlice(float3 east, float3 north, float4x4 invView, float4x4 proj)
{
    float tanHalfFovX = 1.0f / proj._11;
    float halfFovX = atan(tanHalfFovX);
    float3 fwd = normalize(invView[2].xyz);
    float3 right = normalize(invView[0].xyz);

    float3 L = normalize(fwd * cos(halfFovX) - right * sin(halfFovX));
    float3 R = normalize(fwd * cos(halfFovX) + right * sin(halfFovX));

    float lonL = atan2(dot(L, north), dot(L, east));
    float lonR = atan2(dot(R, north), dot(R, east));

    return MakeSlice(UFromLongitude(lonL), UFromLongitude(lonR));
}

// ---- TLUT helpers (same mapping as your other passes)
float2 TransUV(float r, float mu, float Rb, float Rt)
{
    float rNorm = (r - Rb) / max(Rt - Rb, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (Rb * Rb) / (r * r)));
    mu = clamp(mu, muMin + 1e-5f, 1.0f - 1.0e-5f);
    float uMu = (mu - muMin) / (1.0f - muMin);
    return float2(uMu, saturate(rNorm));
}
float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
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

Hit IntersectSphereRobust(float3 ro, float3 rd, float R)
{
    // normalize by radius => sphere becomes unit radius
    float invR = rcp(R);
    float3 roN = ro * invR;        // O(1)
    float3 rdN = rd;               // assume |rd|=1
    
    Hit H;

    // closest approach to center
    float tca = -dot(roN, rdN);
    float d2  = dot(roN, roN) - tca * tca;   // O(1)

    // treat tiny overshoot above 1.0 as grazing hit (fp noise)
    if (d2 > 1.0f + 1e-5f)
    {
        H.ok = false;
        H.t0 = H.t1 = 0.0f;
        return H;
    }

    float m = max(1.0f - d2, 0.0f);
    float thc = sqrt(m);

    float t0N = tca - thc;         // in "radius units"
    float t1N = tca + thc;

    float Rscale = R;              // back to meters
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

float SunVisibilityAtR_Config(float r, float muS, float Rb, float sunRadius)
{
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * sunRadius, sinThetaH * sunRadius, muS - cosThetaH);
}

// Optional: subtle limb darkening so the disc isn't a flat sticker.
// Tweak the constant or lift it into your cbuffer later if you want.
float LimbDarken(float rNorm) // rNorm = theta / SunDiscRadius, [0..∞)
{
    const float u = 0.55f; // 0 = none, ~0.5–0.7 looks nice
    float mu = sqrt(saturate(1.0f - saturate(rNorm) * saturate(rNorm)));
    return 1.0f - u * (1.0f - mu);
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

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight); // physical floor used for densities
    const float RbHit = RbPhys + GroundBiasMeters(Rg); // use ONLY for intersections
    
    // If no geometry wrote to depth, draw SKY using the precomputed SkyView LUT
    if (depth <= 1e-12f)
    {
        float3 wView = ViewDirWS_fromUV(uv);

        // Basis consistent with SkyViewCS
        float3 up, east, north;
        BuildSkyBasis(cameraPosition.xyz, PlanetCenterWS, BasisSpinUp, up, east, north);

        // Map this direction to SkyView (MUST match CS mapping)
        float lon = atan2(dot(wView, north), dot(wView, east));
        float lat = asin(clamp(dot(wView, up), -1.0f, 1.0f));
        float2 skyUV = float2(UFromLongitude(lon), VFromLatitude(lat));
        
        // Sample sky LUT
        float3 sky = SkyViewLUT.SampleLevel(ClampLinear, skyUV, 0).rgb;

        float3 camRel = cameraPosition.xyz - PlanetCenterWS;
        float rCam = max(PlanetRadius, length(camRel));
        float muV = dot(wView, normalize(camRel));
        float3 Tcam = T_to_TOA(rCam, muV, RbHit, Rt);

        float3 stars = SceneColor.Sample(ClampPoint, uv).rgb * Tcam;
        float ySky = dot(sky, LUMA);
        float StarLumaGate = 0.015f; // adjust to taste
        float wStar = saturate(1.0f - ySky / StarLumaGate); 
        stars *= wStar;
        
        if (SunDiscToggle != 0)
        {
            // Direction to sun (your light is FROM light -> scene)
            float3 wSun = -normalize(direction.xyz);

            // Angular distance of current pixel's ray to sun center
            float muViewSun = dot(wView, wSun);
            float theta = acos(clamp(muViewSun, -1.0f, 1.0f));
            float rNorm = theta / max(SunDiscRadius, 1e-6f); // 1.0 at disc edge
            
            // Soft-edged disc mask (feather across the outer rim)
            float edge0 = max(0.0f, 1.0f - SunEdgeSoftness); // inner edge of feather band
            float edge1 = 1.0f; // outer edge (disc radius)
            float discSoft = 1.0f - smoothstep(edge0, edge1, rNorm);

            // Limb darkening (optional but makes it prettier)
            float limb = LimbDarken(rNorm);

            // Horizon visibility at the camera height (lets the disc graze cleanly)
            float3 camRel = cameraPosition.xyz - PlanetCenterWS;
            float rCam = max(PlanetRadius, length(camRel));
            float3 upCam = camRel / rCam;
            float muS_up = dot(upCam, wSun);
            float VsunH = SunVisibilityAtR_Config(rCam, muS_up, RbHit, SunDiscRadius);
    
            // Final disc mask
            float discMask = discSoft * limb * VsunH;

            // Additive glow outside the disc, falling to zero at radius*(1+SunGlowSize)
            // Starts at the rim (rNorm=1) and extends to rNorm=1+SunGlowSize
            float glowOuter = 1.0f + max(SunGlowSize, 0.0f);
            float glowRing = 1.0f - smoothstep(1.0f, glowOuter, rNorm);
            // Mostly keep glow outside the core so the core stays crisp
            float glowMask = glowRing * (1.0f - discSoft) * VsunH;

            // Sun radiance (same units as SkyView) tinted by atmospheric T along the view ray
            float3 Esun = radiance.rgb * SunIntensity; // radiance
            float3 sunDisc = Esun * Tcam * discMask;
            float3 sunGlow = Esun * Tcam * (SunGlowIntensity * glowMask);

            // Accumulate
            sky += sunDisc + sunGlow;
        }

        
        float3 outSky = sky + stars;
        return float4(max(outSky, 0.0f), 1.0f);
    }
    else
    {
        float3 camWS = cameraPosition.xyz;
        float3 ro = camWS - PlanetCenterWS;
        float3 wView = ViewDirWS_fromUV(uv); // unit
        
        // TOA segment
        Hit hitAtm = IntersectSphere_GrazingSafe(ro, wView, Rt);

        float tEnter = max(0.0f, hitAtm.t0);         
        float APFar = asfloat(APFarU32.Load(int3(0, 0, 0)));
              
        float tSurf = ViewDistanceFromDepth(uv, depth);
        
        // distance-from-entry only
        float d = saturate((tSurf - tEnter) / max(APFar, 1e-6f));
        float u = pow(d, 1.0f / AP_Z_GAMMA);

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