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

// ----- constants (keep local; no new uniforms) -----
static const float PaperWhiteNits = 200.0f; // must match your tonemapper
static const float DiscNitsPerIntensity = 3200.0f; // 1.0 intensity -> ~3200 nits
static const float3 SunTint = float3(1.00, 0.985, 0.960);

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
            return float4(0.0f, 0.0f, 0.0f, 0.0f);
        
        uint Wsv, Hsv;
        SkyViewLUT.GetDimensions(Wsv, Hsv);
        float dv = 1.0f / Hsv;
       
        float3 upCam = camRel / rCam;

        // pick the window 
        float mu0, mu1;
        GetMuWindow(rCam, RbVis, Rt, mu0, mu1);
        
        // Basis consistent with SkyViewCS
        float3 up, east, north;
        BuildSkyBasis(cameraPosition.xyz, PlanetCenterWS, BasisSpinUp, up, east, north);
        
        float xE = dot(wView, east);
        float xN = dot(wView, north);
        float mu = dot(wView, up);
        
        float uSky = frac((atan2(xN, xE) + PI) / (2.0f * PI));
        float vSky = VFromMuWindowed(mu, mu0, mu1);
        vSky = 1.0f - vSky;

        // sample with aniso
        float3 sky = SkyViewLUT.Sample(SkyAniso, float2(uSky, vSky)).rgb;

        float muV = dot(wView, normalize(camRel));
        float3 Tcam = T_to_TOA(rCam, muV, RbVis, Rt);
        
        if (SunDiscToggle != 0)
        {
    // ----- directions & angles -----
            float3 wSun = -normalize(direction.xyz); // TO sun
            float muViewSun = dot(wView, wSun); // cos(theta)
            float theta = acos(clamp(muViewSun, -1.0f, 1.0f)); // radians
            float rNorm = theta / max(SunDiscRadius, 1e-6f);

            float3 camRel = cameraPosition.xyz - PlanetCenterWS;
            float rCam = max(PlanetRadius, length(camRel));
            float3 upCam = camRel / rCam;
            float muS_up = dot(upCam, wSun);

    // ===== HORIZON / TWILIGHT GATING =====
            const float RefracCenterDeg = 0.83f; // apparent sunset lift
            float VdiscH = SunVisibilityAtR(
        rCam, muS_up, RbVis, SunDiscRadius + radians(RefracCenterDeg));

            float sunAlt = asin(clamp(muS_up, -1.0f, 1.0f)); // radians
            float twilight = smoothstep(radians(-6.0f), radians(0.0f), sunAlt);
            float haloGate = max(VdiscH, twilight * twilight); // halo lingers into civil twilight

    // ---------- brightness scaling ----------
            const float PaperWhiteNits = 200.0f;
            const float DiscNitsPerIntensity = 3200.0f; // 1.0 -> 3200 nits
            const float3 SunTint = float3(1.00, 0.985, 0.960);
            float discLinear = (SunIntensity * DiscNitsPerIntensity) / PaperWhiteNits;

    // ===== View-ray transmittance (consistent basis for disc & halo) =====
            float muV = dot(wView, normalize(camRel)); // view vs upCam
            float3 Tview = saturate(T_to_TOA(rCam, muV, RbVis, Rt));

    // Air-mass proxy from Tview (0 at zenith → larger near horizon)
            float TvY = dot(Tview, 0.3333.xxx);
            float airmass = saturate(-log(max(TvY, 1e-6)));
            float zenith = saturate(1.0 - 0.6 * airmass); // ~1 at noon, ~0 near horizon

    // ---------- disc shape ----------
            float featherFracBase = clamp(SunEdgeSoftness * 0.25f, 0.02f, 0.06f);
            float featherFrac = saturate(featherFracBase + (1.0 - zenith) * 0.02); // slightly softer near horizon
            float discSoft = 1.0f - smoothstep(1.0f - featherFrac, 1.0f, rNorm);
            float limb = LimbDarken(rNorm);
            float discMask = discSoft * limb * VdiscH; // disc uses visibility only

    // ===== DISC COLOR: achromatic extinction + warm shift (prevents blue disc) =====
            float TviewY = dot(Tview, float3(0.2126, 0.7152, 0.0722)); // luminance
            float TdiscSc = max(pow(saturate(TviewY), 0.80), 0.005); // soften & floor (no black hole)
            float warmAmt = saturate(1.0 - zenith); // 0 noon → 1 horizon
            float3 warmTint = normalize(lerp(float3(1, 1, 1), float3(1.06, 0.96, 0.84), 0.85 * warmAmt));

    // Subtle disc EV bias (dim a bit toward horizon)
            float discEVBias = lerp(-0.5, 0.0, zenith);
            float discGain = exp2(discEVBias);

            float3 discTintFinal = normalize(SunTint * warmTint); // slightly warm, never blue
            float3 sunDisc = discTintFinal * (discLinear * discGain) * TdiscSc * discMask;

    // ===== HALO (sky-tinted, path-length & altitude gated) =====

    // Basis for sky tints
            float3 up, east, north;
            BuildSkyBasis(cameraPosition.xyz, PlanetCenterWS, BasisSpinUp, up, east, north);

    // Sunward sky sample (for tint near disc)
            float xEs = dot(wSun, east);
            float xNs = dot(wSun, north);
            float muS = dot(wSun, up);

    // Recompute μ-window for current camera height
            float mu0, mu1;
            GetMuWindow(rCam, RbVis, Rt, mu0, mu1);

            float uSun = frac((atan2(xNs, xEs) + PI) / (2.0f * PI));
            float vSun = 1.0f - VFromMuWindowed(muS, mu0, mu1);

            float3 skyTowardSun = SkyViewLUT.Sample(SkyAniso, float2(uSun, vSun)).rgb;
            float3 sunSkyTint = skyTowardSun / max(dot(skyTowardSun, LUMA), 1e-3);
            float3 skyTintLocal = sky / max(dot(sky, LUMA), 1e-3);

    // Tint evolves from sun-side sky near the disc to local sky farther out;
    // also nudge toward a near-white Mie color at high sun (avoid icy cyan at noon)
            float tAngle = smoothstep(0.0f, radians(6.0f), theta);
            float3 haloTint = normalize(lerp(sunSkyTint, skyTintLocal, tAngle));
            float3 mieWhite = float3(1.00, 0.98, 0.95);
            haloTint = normalize(lerp(haloTint, mieWhite, 0.45 * zenith));

    // If sun is below horizon, bias back toward local sky to avoid over-red halo
            float below = saturate(-sunAlt / radians(6.0f));
            haloTint = normalize(lerp(haloTint, skyTintLocal, 0.6f * below));

    // ---- Path-length: how much of the view ray is inside atmosphere?
            Hit hitAtm = IntersectSphereGrazingSafe(camRel, wView, Rt);
            float L_atmo = (hitAtm.ok && hitAtm.t1 > max(0.0, hitAtm.t0))
                 ? (max(0.0, hitAtm.t1) - max(0.0, hitAtm.t0)) : 0.0;

    // Convert to [0..1] gate (0 if no intersection, →1 for long paths)
            const float L0 = 12000.0; // ~12 km; tweak 8–20 km to taste
            float mPath = 1.0 - exp(-L_atmo / L0);

    // Altitude gate: fade halo as camera climbs to space
            float hCam = max(0.0f, rCam - RbVis);
            float densR = exp(-hCam / max(RayScaleHeight, 1.0f));
            float densM = exp(-hCam / max(MieScaleHeight, 1.0f));
            float mAlt = saturate(0.35 * densR + 0.65 * densM);

    // Angular shaping: tight core + soft outer fade (no hard cutoff)
            float haloHalfDegBase = lerp(2.0, 6.0, saturate(SunGlowSize));
            float haloHalfDeg = lerp(max(1.2, haloHalfDegBase * 0.7), haloHalfDegBase, 1.0 - 0.85 * zenith);
            float theta50 = radians(haloHalfDeg);
            float haloCore = exp(-(theta * theta) / (2.0f * theta50 * theta50)); // inner Gaussian

            float thetaOuterOn = radians(8.0); // start fading after ~8°
            float thetaOuterOff = radians(18.0); // fully gone by ~18°
            float outerGate = smoothstep(thetaOuterOff, thetaOuterOn, theta);

            float haloAngular = haloCore * outerGate;

    // Keep mild chroma for atmospheric halo; slightly brighter at zenith to avoid over-dimming
            float3 Thalo = pow(Tview, 0.35);
            float3 ThaloZenith = lerp(Thalo, max(Thalo, pow(Thalo, 0.25)), 0.5 * zenith);

    // Final halo intensity with air-mass scaling + gates
            float haloGain = SunGlowIntensity * lerp(0.35, 1.2, 1.0 - 0.85 * zenith);

            float3 halo = haloTint * (discLinear * haloGain) * haloAngular * haloGate * ThaloZenith;
            halo *= (mPath * mAlt); // kill in space / very short paths

    // ---------- compact dazzle veil (optional glare cone) ----------
            float coneDeg = lerp(10.0f, 16.0f, saturate(SunGlowSize));
            coneDeg = lerp(coneDeg * 0.6, coneDeg, 1.0 - 0.8 * zenith); // smaller at noon
            float cone = radians(coneDeg);
            if (SunGlowIntensity > 0.0f && theta <= cone)
            {
                float theta0 = 3.0f * SunDiscRadius;
                float veil = (discLinear / (1.0 + (theta / theta0) * (theta / theta0))) * (0.6f * SunGlowIntensity);
                veil = min(veil, 0.30f * discLinear);
                float edge = smoothstep(cone, 0.0f, theta);

                float3 veilColor = lerp(1.0.xxx, haloTint, 0.20 * (1.0 - zenith)); // whiter at noon
                float3 veilTerm = (veil * edge) * veilColor * haloGate * ThaloZenith;
                halo += veilTerm * (mPath * mAlt); // same atmosphere gates
            }

    // Accumulate (pre-tonemap)
            sky += sunDisc + halo;

    // // DEBUG: visualize true disc edge (uncomment to verify apparent size)
    // float ring = smoothstep(0.985, 0.995, rNorm) - smoothstep(1.005, 1.015, rNorm);
    // sky += ring.xxx;
        }
               
        float3 outSky = sky;
        return float4(max(outSky, 0.0f), 1.0f);
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
        return float4(outRGB, max(Trgb.r, max(Trgb.g, Trgb.b)));
    }
}