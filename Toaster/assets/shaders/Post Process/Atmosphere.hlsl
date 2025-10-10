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
    float SunDiscRadius; // rad  (e.g. radians(0.2666))
    float SunEdgeSoftness; // rad  (soft rim width)
    int SunDiscToggle; // 0=off, 1=on
    float SpaceDiscBrightnessScale; // unitless scale, e.g. 1.30
    
    float3 SunDiscWhite;
    float AirHaloIntensity; // 0..~0.6 (was HaloStrength_Ground, e.g. 0.28)
    
    float3 WarmTint; // e.g. float3(1.00, 0.92, 0.78)    
    float AirHaloStartFrac; // 0..1   (was InAirStart, e.g. 0.15)
    
    float AirHaloFalloffPow; // curve (was InAirPow, e.g. 1.10)
    float HorizonRefractionDeg; // deg (was RefracCenterDeg, e.g. 0.83)
    float TwilightBlendDeg; // deg (was TwilightExtraDeg, e.g. 1.5)
    float SpaceHaloWidthDeg; // deg (was SpaceHaloSigmaDeg, e.g. 0.8)
    
    float SpaceHaloIntensity; // 0.01..0.10 (was SpaceHaloGain, e.g. 0.04)
    float SpaceHaloCutoffDeg; // deg (was SpaceHaloCutoffDeg, e.g. 6.0)
};

// ===== Textures / Samplers ==================================================
Texture2D<float4> TransmittanceLUT      : register(t0); // (not used in composite)
Texture2D<float4> MultiScatterLUT       : register(t1); // (not used in composite)
Texture2D<float4> SkyViewLUT            : register(t2);
Texture3D<float4> AerialPerspective3D   : register(t3);
Texture2D<float4> positionTexture       : register(t4);
Texture2D<uint> APFarU32                : register(t5);
Texture2D<float> SceneDepth             : register(t9);
Texture2D<float4> SceneColor            : register(t10);

SamplerState ClampLinear    : register(s0);
SamplerState ClampPoint     : register(s1);
SamplerState SkyAniso       : register(s3);

#ifndef AP_Z_GAMMA
#define AP_Z_GAMMA 1.6f   // try 1.5–1.8; 1.6 is a good start
#endif

// ===== Math helpers =========================================================
static const float PI = 3.14159265358979323846f;
static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

// ---------- HELPERS ----------
float2 SunScreenUV()
{
    float3 wSun = -normalize(direction.xyz); // TO sun
    float3 pWS = cameraPosition.xyz + wSun * 1e6;
    float4 pVS = mul(float4(pWS, 1), viewMatrix);
    float4 pCS = mul(pVS, projectionMatrix);
    float2 ndc = pCS.xy / max(pCS.w, 1e-6);
    return 0.5 * (ndc * float2(1, -1) + float2(1, 1));
}

float Starburst(float2 dirNorm, float sharp, float aspect)
{
    float2 dn = (all(dirNorm == 0)) ? float2(1, 0) : normalize(float2(dirNorm.x * (1.0 + aspect), dirNorm.y));
    float2 a0 = float2(1, 0), a1 = float2(0, 1);
    float2 a2 = normalize(float2(1, 1));
    float2 a3 = normalize(float2(1, -1));
    float s = max(max(abs(dot(dn, a0)), abs(dot(dn, a1))),
                   max(abs(dot(dn, a2)), abs(dot(dn, a3))));
    return pow(s, sharp);
}

// Rayleigh and Henyey–Greenstein phase functions (normalized)
float PhaseRayleigh(float cosTheta)
{
    return (3.0f / (16.0f * PI)) * (1.0f + cosTheta * cosTheta);
}
float PhaseMie(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = pow(max(1.0f + g2 - 2.0f * g * cosTheta, 1e-4f), 1.5f);
    return (1.0f / (4.0f * PI)) * ((1.0f - g2) / denom);
}

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

float GroundBiasMeters(float Rg)
{
    return max(1.0f, 2e-6f * Rg);
}

Hit IntersectSphereGrazingSafe(float3 ro, float3 rd, float R)
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

// μ at horizon for a sphere of radius R as seen from r
float MuHorizon(float r, float R)
{
    float s = saturate(R / r);
    return -sqrt(max(1.0f - s * s, 0.0f));
}

// Per-frame μ window that actually produces sky
// mu0 = lower bound (planet/ground horizon), mu1 = upper bound (TOA edge).
// If camera is inside the atmosphere (r <= Rt), every upward μ intersects;
// in that case, set mu1 = 1.
void GetMuWindow(float r, float RbVis, float Rt, out float mu0, out float mu1)
{
    float muG = MuHorizon(r, RbVis); // lower bound (sky starts above ground)
    float muT = (r <= Rt) ? 1.0f : MuHorizon(r, Rt); // upper bound where rays stop hitting TOA
    mu0 = muG;
    mu1 = max(muG + 1e-5f, muT); // keep a tiny span at least
}

// Map μ → v in [0,1] using this window (clamped)
float VFromMuWindowed(float mu, float mu0, float mu1)
{
    return saturate((mu - mu0) / max(mu1 - mu0, 1e-6f));
}

float SunVisibilityAtR(float r, float muS, float Rb, float sunRadius)
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

// Transmittance of the *atmospheric segment* intersected by ray (ro,rd).
// Uses your TLUT T_to_TOA(r, mu, Rb, Rt):
// If the LUT encodes T(p->TOA) = exp(-tau), then T_segment(p->q) = T(p->TOA) / T(q->TOA)
float3 TLUT_SegmentTransmittance(float3 ro, float3 rd, float RbVis, float Rt)
{
    Hit hit = IntersectSphereGrazingSafe(ro, rd, Rt);
    if (!hit.ok || hit.t1 <= max(0.0, hit.t0))
        return 1.0.xxx; // no atmosphere on ray

    float tEnter = max(0.0, hit.t0);
    float tExit = hit.t1;

    float3 pEnter = ro + rd * tEnter;
    float3 pExit = ro + rd * tExit;

    float rEnter = length(pEnter);
    float rExit = length(pExit);

    float3 upEnter = pEnter / rEnter;
    float3 upExit = pExit / rExit;

    float muEnter = dot(rd, upEnter); // cos zenith at entry
    float muExit = dot(rd, upExit); // ...at exit (same rd)

    float3 Tenter = T_to_TOA(rEnter, muEnter, RbVis, Rt);
    float3 Texit = T_to_TOA(rExit, muExit, RbVis, Rt);

    // Avoid divide-by-near-zero (if Texit extremely small, clamp)
    Texit = max(Texit, 1e-5.xxx);

    return saturate(Tenter / Texit);
}

// Eye→Sun transmittance. 100% reusing your helpers.
// camRel  : cameraPosition - PlanetCenterWS
// vDir    : view ray (not used for T here, but keep for future variants)
// wSun    : TO-sun direction (unit)
// RbPhys  : physical ground radius you use elsewhere (PlanetRadius + min(0, MinHeight))
// Rt      : top-of-atmosphere radius
float3 SampleTransmittance_EyeToSun(float3 camRel, float3 vDir, float3 wSun, float RbPhys, float Rt)
{
    float r = length(camRel);
    float3 up = camRel / max(r, 1e-6f);

    // Inside atmosphere: TLUT gives T(point -> TOA) along ray direction (here: toward sun)
    if (r <= Rt + 1e-3f)
    {
        // μ = cos(angle(ray, local up)) for the *ray you’re tracing to the sun*
        float muRay = clamp(dot(wSun, up), -1.0f, 1.0f);
        return T_to_TOA(r, muRay, RbPhys, Rt); // uses TransUV + TransmittanceLUT.SampleLevel(...)
    }

    // In space: only part of the eye→sun ray (if any) crosses the shell.
    // Your TLUT_SegmentTransmittance computes T_segment = T(enter->TOA) / T(exit->TOA).
    // If the ray never hits the shell, it returns 1.
    return TLUT_SegmentTransmittance(camRel, wSun, RbPhys, Rt);
}

// Impact parameter for a ray from radius r with view cosine mu
float ImpactParameter(float r, float mu)
{
    return r * sqrt(saturate(1.0f - mu * mu));
}

// v in [0,1]  <->  p in [RbPhys, Rt]  (GROUND ↔ TOA)
float VFromP(float p, float RbPhys, float Rt)
{
    float t = (p - RbPhys) / max(Rt - RbPhys, 1.0e-6f);
    return saturate(t);
}
float PFromV(float v, float RbPhys, float Rt)
{
    return lerp(RbPhys, Rt, saturate(v));
}

// Reader: μ -> v using rEval and **RbPhys**
float VFromMu_AirArc(float mu, float rEval, float RbPhys, float Rt)
{
    float p = ImpactParameter(rEval, mu);
    return VFromP(p, RbPhys, Rt);
}

// ===== PS ===================================================================
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PSOut
{
    float4 color : SV_Target0; // sky color
    float disc : SV_Target1; // SunDiscMask [0..1]
    float halo : SV_Target2; // SunHaloMask [0..1]
};

PSOut main(PSIn i)
{
    PSOut output;
    const float2 uv = i.uv;

    // Fetch scene
    float3 colorPreAtmos = SceneColor.Sample(ClampPoint, uv);
    float depth = SceneDepth.Sample(ClampPoint, uv); // hardware depth, reversed-Z (sky -> 0)

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight); // physical floor used for densities
    const float RbHit = RbPhys + GroundBiasMeters(Rg); // use ONLY for intersections
    const float RbVis = (PlanetRadius + min(0.0f, MinHeight)) + max(1.0f, 2e-6f * PlanetRadius);
    
    // If no geometry wrote to depth, draw SKY using the precomputed SkyView LUT
    if (depth <= 1e-12f)
    {
        float3 camWS = cameraPosition.xyz;
        float3 camRel = cameraPosition.xyz - PlanetCenterWS;
        float rCam = max(PlanetRadius, length(camRel));
        float3 wView = ViewDirWS_fromUV(uv); // unit
        
        Hit h = IntersectSphereGrazingSafe(camRel, wView, RbHit);
        
        if (h.ok && h.t1 > 0.0f)
        {
            // fully occluded by ground: no sky, no sun, no halo
            output.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
            output.disc = float4(0.0f, 0.0f, 0.0f, 0.0f);
            output.halo = float4(0.0f, 0.0f, 0.0f, 0.0f);
            return output;
        }
        
        uint Wsv, Hsv;
        SkyViewLUT.GetDimensions(Wsv, Hsv);
        float dv = 1.0f / Hsv;
       
        float3 upCam = camRel / rCam;
        float rWin = min(rCam, Rt - 1.0f);
       
        // Basis consistent with SkyViewCS
        float3 up, east, north;
        BuildSkyBasis(cameraPosition.xyz, PlanetCenterWS, BasisSpinUp, up, east, north);
        
        float xE = dot(wView, east);
        float xN = dot(wView, north);
        float mu = dot(wView, up);
       
        // same stable window as writer (from rWin)
        float mu0_ref, mu1_ref;
        GetMuWindow(rWin, RbVis, Rt, mu0_ref, mu1_ref); // mu1_ref==1.0, constant
        float S_ref = max(mu1_ref - mu0_ref, 1.0e-6f);

        // actual ground horizon at the real camera radius rCam
        float mu0_cam = MuHorizon(rCam, RbVis);

        // map μ using the actual horizon as the offset, but the stable span S_ref
        float vPhys = (mu - mu0_cam) / S_ref; // <-- anchor to real horizon
        vPhys = saturate(vPhys);

        // inverse focus to get row
        float uSky = frac((atan2(xN, xE) + PI) / (2.0f * PI));
        const float k = 0.85f; // must match writer
        float vRow = pow(vPhys, 1.0f / k);
        float vSky = 1.0f - vRow;

        // sample with aniso
        float3 sky = SkyViewLUT.Sample(SkyAniso, float2(uSky, vSky)).rgb;
        
        float3 sunColor = float3(0.0f, 0.0f, 0.0f);
        float discMask_out = 0.0f; // <- will go to SV_Target1
        float haloMask_out = 0.0f; // <- will go to SV_Target2
        if (SunDiscToggle != 0)
        {
            // ===============================
            // Geometry
            // ===============================
            float3 vDir = normalize(wView); // cam → pixel
            float3 wSun = -normalize(direction.xyz); // TO sun
            float mu = clamp(dot(vDir, wSun), -1.0, 1.0);
            
            float theta = acos(mu);
            float edgePxR = 1.5f;
            float dmu = length(float2(ddx(mu), ddy(mu))) + 1e-7f;
            float dtheta = dmu / max(sin(theta), 1e-4f);
            float wEdge = edgePxR * dtheta;

            float rDiscRad = SunDiscRadius; // from cbuffer (radians)
            float rSoftRad = SunEdgeSoftness; // from cbuffer (radians)
            float sunDiscRadiusDeg = degrees(SunDiscRadius);
            float sunEdgeSoftDeg = degrees(SunEdgeSoftness);
            float muDisc = cos(rDiscRad);
            float denom = max(1.0 - muDisc, 1e-12);
            float rNorm = sqrt(max((1.0 - mu) / denom, 0.0)); // 0 center, 1 edge, >1 outside

            float discMask = 1.0f - smoothstep(rDiscRad, rDiscRad + wEdge, theta);

            float3 camRel = cameraPosition.xyz - PlanetCenterWS;
            float rCam = max(1e-6, length(camRel));
            float3 upCam = camRel / rCam;

            float altUpDeg = degrees(asin(clamp(dot(upCam, wSun), -1.0, 1.0)));
            float dipGroundDeg = degrees(acos(saturate(RbPhys / rCam)));
            float altRelGroundDeg = altUpDeg + dipGroundDeg;

            float shellSpan = max(Rt - RbPhys, 1.0e-6);
            float rClamp = clamp(rCam, RbPhys, Rt);
            float airFrac = (Rt - rClamp) / shellSpan;
            float inAirHalo = pow(saturate(airFrac), AirHaloFalloffPow) * smoothstep(0.0, AirHaloStartFrac, airFrac);

            // ===============================
            // Base disc tint (rim warming, luminance preserved)
            // ===============================
            float3 T_eye_sun = SampleTransmittance_EyeToSun(camRel, vDir, wSun, RbPhys, Rt);
            float3 discRGB = SunIntensity * SunDiscWhite * T_eye_sun;
            

            // Ground occlusion           
            float3 oc = camRel;
            float3 rd = normalize(wSun);
            float b = dot(oc, rd);
            float c = dot(oc, oc) - (RbPhys * RbPhys);
            float h = b * b - c;
            bool frontHit = (h >= 0.0f) && ((-b - sqrt(max(h, 0.0f))) >= 0.0f);
            
            // --- refraction-aware visibility (inflate only the *test*, not the drawn radius) ---
            float altEffDeg = altRelGroundDeg + HorizonRefractionDeg; // your variables
            float visSoft = smoothstep(-sunDiscRadiusDeg, +sunDiscRadiusDeg, altEffDeg);

            // Keep your robust hit test (frontHit / hitGround)
            float discVis = frontHit ? visSoft : 1.0f;

            //// ===============================
            //// Sky color from LUT
            //// ===============================
            //uint Wsv, Hsv;
            //SkyViewLUT.GetDimensions(Wsv, Hsv);
            //float mu0, mu1;
            ////GetMuWindow(rCam, RbPhys, Rt, mu0, mu1);

            //float3 up, east, north;
            //BuildSkyBasis(cameraPosition.xyz, PlanetCenterWS, BasisSpinUp, up, east, north);

            //float xE = dot(vDir, east);
            //float xN = dot(vDir, north);
            //float muV = dot(vDir, up);

            //float uSky = frac((atan2(xN, xE) + PI) / (2.0f * PI));
            //float vSky = 1.0f - VFromMuWindowed(muV, mu0, mu1);
            ////float3 sky = SkyViewLUT.Sample(SkyAniso, float2(uSky, vSky)).rgb;
            
            // --- rim color match to sky near sunset ---
            float sunAltDeg = altUpDeg;
            float sunset = saturate(1.0f - saturate((sunAltDeg - 2.0f) / 6.0f)); // 1 near horizon → 0 by ~+8°
            float rim01 = smoothstep(0.7f, 1.0f, saturate(theta / rDiscRad));
            
            // Luma-preserving blend toward sky chroma at the rim when low
            float Yd = max(dot(discRGB, LUMA), 1e-6);
            float Ys = max(dot(sky, LUMA), 1e-6);
            float3 edgeRGB = lerp(discRGB, (sky / Ys) * Yd, 0.30 * sunset * rim01);

            discRGB = lerp(discRGB, edgeRGB, rim01);
            
            // ===============================
            // SPACE-ONLY tint logic with tangent altitude
            // ===============================
            float3 discTint = discRGB;
            float3 discAttn = 1.0.xxx;

            bool cameraAboveAtmo = (rCam >= Rt + 1e-3);
            bool throughAtmo_SPACE = false;
            float h_tan = 1e9;

            if (cameraAboveAtmo)
            {
                // tangent altitude of ray to sun
                float dMin = length(cross(camRel, vDir)) / length(vDir); // closest approach
                h_tan = dMin - PlanetRadius;

                // ray ∩ outer sphere?
                float bRt = dot(camRel, vDir);
                float cRt = dot(camRel, camRel) - Rt * Rt;
                float hRt = bRt * bRt - cRt;
                float sRt = sqrt(max(hRt, 0.0));
                float t1Rt = -bRt + sRt;
                bool hitsRt = (hRt >= 0.0) && (t1Rt > 0.0);

                // only consider tint if tangent altitude low enough
                const float HtintMax = 35000.0; // 35 km
                throughAtmo_SPACE = hitsRt && (h_tan <= HtintMax);
            }

            float dipAtmoDeg = cameraAboveAtmo ? degrees(acos(saturate(Rt / rCam))) : 0.0;
            float altRelAtmoDeg = altUpDeg + dipAtmoDeg;
            float cover = saturate((sunDiscRadiusDeg - altRelAtmoDeg) / (2.0 * sunDiscRadiusDeg));

            float depthWeight = saturate((70000.0 - h_tan) / 70000.0);
            float tintStrength = saturate(depthWeight * cover);

            if (throughAtmo_SPACE)
            {
                float Ysky = max(dot(sky, LUMA), 1e-6);
                float3 skyChroma = sky / Ysky;

                float3 targetTint = normalize(lerp(discTint, skyChroma, 0.30 * tintStrength));
                float Ysrc = max(dot(discTint, LUMA), 1e-6);
                discTint = targetTint * (Ysrc / max(dot(targetTint, LUMA), 1e-6));

                float3 extBase = float3(0.92, 0.88, 0.84);
                discAttn = lerp(1.0.xxx, extBase, 0.5 * tintStrength);
            }
            else
            {
                float spaceGain = lerp(SpaceDiscBrightnessScale, 1.0, saturate(inAirHalo));
                discAttn *= spaceGain;
            }

            // ===============================
            // Halo (AIR)
            // ===============================
            float x = rNorm;
            float deg = x * sunDiscRadiusDeg; // angular distance from center (deg)

            float tight = exp(-pow(deg / 1.8, 2.0));
            float shoulder = 1.0 / (1.0 + pow(deg / 4.5, 2.2));
            float tailMain = 1.0 / pow(1.0 + (deg / 10.0), 1.25);
            float tailUltra = 1.0 / pow(1.0 + (deg / 25.0), 1.35);

            float haloCore = 0.70 * tight + 0.55 * shoulder;
            float haloOuter = (0.28 * tailMain + 0.06 * tailUltra) * 0.85;
            float haloMaskAirShape = haloCore + haloOuter;

            float bleedAmt = saturate((deg - 0.35) / 2.6) * inAirHalo;
            float3 baseHalo = lerp(SunDiscWhite, WarmTint, 0.32);
            float3 haloTint = lerp(baseHalo, sky, 0.75 * bleedAmt);

            float haloVisAir = smoothstep(-HorizonRefractionDeg - TwilightBlendDeg, +HorizonRefractionDeg, altRelGroundDeg);
            float3 haloRadianceAir = SunIntensity * (AirHaloIntensity * inAirHalo) * haloTint * haloMaskAirShape * haloVisAir;

            // ===============================
            // Halo (SPACE) — tiny, always-on shoulder
            // ===============================
            float edgeFromCenterDeg = max(deg - sunDiscRadiusDeg, 0.0);
            float sigma = max(SpaceHaloWidthDeg, 1e-3);

            float spaceGauss = exp(-0.5 * (edgeFromCenterDeg / sigma) * (edgeFromCenterDeg / sigma));
            spaceGauss *= step(edgeFromCenterDeg, SpaceHaloCutoffDeg);

            float haloVisSpace = discVis;
            float3 spaceHaloTint = SunDiscWhite;

            float3 haloRadianceSpace = SunIntensity * (SpaceHaloIntensity) * spaceHaloTint * spaceGauss.xxx * haloVisSpace;

            // ===============================
            // Core boost (existing)
            // ===============================           
            float thetaInner = 0.60 * rDiscRad; // inner 60% of radius
            float m_inner = 1.0 - smoothstep(thetaInner, thetaInner + wEdge, theta);

            const float PW = 200.0;
            const float SunHDRBoostNits = 6000.0; // 3000–8000
            float boostLin = SunHDRBoostNits / PW;

            // Boost center without fattening the rim
            float3 discRGB_boosted = discRGB + (boostLin * m_inner).xxx;

            // Final disc (mask + visibility)
            float3 discRadiance = discRGB_boosted * discMask * discVis;

            // ===============================
            // Compose
            // ===============================
            sunColor += discRadiance + haloRadianceAir + haloRadianceSpace;

            // ====== WRITE MASKS ======
            discMask_out = saturate(discMask * discVis);

            float haloMaskAir = saturate(haloMaskAirShape * haloVisAir);
            float haloMaskSpace = saturate(spaceGauss * haloVisSpace);
            haloMask_out = saturate(haloMaskAir + haloMaskSpace);
        }
               
        float3 outSky = sky + sunColor;
        output.color = float4(max(outSky, 0.0f), 1.0f);
        output.disc = discMask_out;
        output.halo = haloMask_out;
        return output;
    }
    else
    {       
        float3 camWS = cameraPosition.xyz;
        float3 ro = camWS - PlanetCenterWS;
        float rCam = length(ro);              
        float3 wView = ViewDirWS_fromUV(uv); // unit
        
        // TOA segment
        Hit hitAtm = IntersectSphereGrazingSafe(ro, wView, Rt);

        float tEnter = max(0.0f, hitAtm.t0);
        float tSurf = ViewDistanceFromDepth(uv, depth);
        
        float lengthInAtmosphere = (rCam <= Rt) ? tSurf : max(0.0f, tSurf - tEnter);
        
        float APFar = asfloat(APFarU32.Load(int3(0, 0, 0)));
        float d = (APFar > 1e-6f) ? saturate(lengthInAtmosphere / APFar) : 0.0f;
        float u = pow(d, 1.0f / AP_Z_GAMMA);
             
        // address slice **centers** then (optionally) jitter
        uint Wd, Hd, Dd;
        AerialPerspective3D.GetDimensions(Wd, Hd, Dd);
        float wAP = u * ((Dd - 1.0f) / Dd) + (0.5f / Dd);

        // final sample: TRILINEAR
        float4 ap = AerialPerspective3D.SampleLevel(ClampLinear, float3(uv, wAP), 0);
        float tau = max(ap.a, 0.0f);
        
        float3 betaExt = RayleighScattering + MieScattering + MieAbsorption; // 1/m
        float betaAvg = (betaExt.r + betaExt.g + betaExt.b) * (1.0f / 3.0f);
        float3 k = betaExt / max(betaAvg, 1e-9);

        // Trgb ≈ A^(betaExt / betaAvg)
        float3 Trgb = exp(-tau * k);

        float3 outRGB = colorPreAtmos * Trgb + ap.rgb;
        output.color = float4(outRGB, max(Trgb.r, max(Trgb.g, Trgb.b)));
        output.disc = 0.0f; // no sun over geometry pass here
        output.halo = 0.0f; // no halo mask over geometry pixels
        return output;
    }
}