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
    float4x4 lightViewProj[4];
    
    float4 direction; // FROM light -> scene
    
    float4 radiance; // RGB
    
    float SunIntensity;
    float DirectionalLightGain;
    uint CascadeCount;
    float ShadowDistance;
    
    float4 CascadeEnds;
    
    uint CascadeIndex;
    float ConstantBias;
    float SlopeBias;
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
    float AtmosphereHeight;
    float RayScaleHeight;
    float MieScaleHeight;
    float MSGain;
    
    float3 RayleighScattering;
    float SGain;
    
    float3 MieScattering;
    
    float3 MieAbsorption;
    
    float3 GroundAlbedo;
    uint FaceIndex;
    
    float3 MieAnisotropy;
    float OzoneStrength;
    
    uint StepsTransmittance;
    uint StepsMultiScattering;
    float APFarDynamic;
    
    float3 SunsetTint;
    float BakeIBL;
    
    float3 MieLobeWeight;
    float MieAnisotropyBroad;
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
#define AP_Z_GAMMA 1.6f  
#endif

#ifndef AP_XY_BSPLINE
#define AP_XY_BSPLINE 1 // 1 = cubic B-spline in screen x/y, 0 = plain trilinear
#endif

// ===== Math helpers =========================================================
static const float PI = 3.14159265358979323846f;
static const float INV4PI = 0.25f / PI;
static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);
static const float MU_EPS = 8e-4;

#include "TransmittanceLUT.hlsli"
#include "SkyViewLUT.hlsli"

static const float ORBIT_TAU_STEP = 0.03f; // target optical depth per step(same as SkyViewCS)
static const uint ORBIT_NCAP = 64u; // soft step cap; hard cap is 4x (same scheme as SkyViewCS)
static const float3 O3_COEFF_ORBIT = float3(0.650e-6f, 1.881e-6f, 0.085e-6f); // as TransmittanceCS (Bruneton)
static const float ORBIT_BLEND_M = 20000.0f;

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

float DensityExpTaperedOrbit(float h, float H)
{
    float hTop = AtmosphereHeight - min(0.0f, MinHeight);
    H = max(H, 1e-3f);
    return max(exp(-max(h, 0.0f) / H) - exp(-hTop / H), 0.0f);
}

float DensityRayleighOrbit(float h)
{
    return DensityExpTaperedOrbit(h, RayScaleHeight);
}
float DensityMieOrbit(float h)
{
    return DensityExpTaperedOrbit(h, MieScaleHeight);
}

float DensityOzoneOrbit(float hMeters)
{
    float km = hMeters * 1e-3;
    float tri = saturate(1.0f - abs((km - 25.0f) / 15.0f)); 
    return tri * (OzoneStrength / 15000.0f);
}

float PhaseMieHG_CS(float mu, float g) // Cornette–Shanks
{
    g = saturate(g);
    float g2 = g * g;
    float denom = pow(1.0 + g2 - 2.0 * g * mu, 1.5);
    float cs = (3.0 * (1.0 + mu * mu)) / (2.0 * (2.0 + g2)) * ((1.0 - g2) / max(denom, 1e-5));
    return INV4PI * cs;
}

float MuDiscAvg(float mu)
{
    float theta = acos(clamp(mu, -1.0f, 1.0f));
    float thetaEff = sqrt(theta * theta + SunDiscRadius * SunDiscRadius);
    return cos(thetaEff);
}

float3 PhaseMieTwoLobe(float mu)
{
    float muEff = MuDiscAvg(mu);
    float3 gN = saturate(MieAnisotropy);
    float3 w = saturate(MieLobeWeight);
    float3 narrow = float3(PhaseMieHG_CS(muEff, gN.r), PhaseMieHG_CS(muEff, gN.g), PhaseMieHG_CS(muEff, gN.b));
    float broad = PhaseMieHG_CS(muEff, MieAnisotropyBroad);
    return w * narrow + (1.0f - w) * broad;
}

void BuildSkyBasisAnchored(float3 camWS, float3 planetCenterWS, float3 basisEastWS, float3 basisNorthWS, float3 spinUpWS, out float3 up, out float3 east, out float3 north)
{
    // 1) Radial up (center → camera)
    float3 rel = camWS - planetCenterWS;
    float len2 = max(dot(rel, rel), 1e-20f);
    up = rel * rsqrt(len2);

    // 2) Project planet-frame east onto the tangent plane
    float3 eProj = basisEastWS - up * dot(basisEastWS, up);
    float e2 = dot(eProj, eProj);

    // If degenerate (near poles or bad input), derive east from spinUp×up
    if (e2 < 1e-10f)
    {
        float3 sProj = spinUpWS - up * dot(spinUpWS, up);
        float s2 = dot(sProj, sProj);
        if (s2 < 1e-10f)
        {
            // Final fallback: pick any axis not colinear with up
            float3 a = (abs(up.y) < 0.99f) ? float3(0, 1, 0) : float3(1, 0, 0);
            eProj = a - up * dot(a, up);
        }
        else
        {
            eProj = sProj;
        }
    }

    east = eProj * rsqrt(max(dot(eProj, eProj), 1e-20f));
    north = normalize(cross(up, east)); // ensure RHS

    // 3) Enforce orientation to be consistent with basisNorthWS
    // If our computed 'north' points opposite the provided north, flip (east,north).
    if (dot(north, basisNorthWS) < 0.0f)
    {
        east = -east;
        north = -north;
    }
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

float3 CubemapDirFromFaceUV(int face, float2 uv)
{
    float2 a = uv * 2.0f - 1.0f; // [-1,1]
    // Note: depending on your RT UV origin, you may need a.y = -a.y
    float x = a.x;
    float y = a.y;

    switch (face)
    {
        case 0:
            return normalize(float3(1, y, -x)); // +X
        case 1:
            return normalize(float3(-1, y, x)); // -X
        case 2:
            return normalize(float3(x, 1, -y)); // +Y
        case 3:
            return normalize(float3(x, -1, y)); // -Y
        case 4:
            return normalize(float3(x, y, 1)); // +Z
        default:
            return normalize(float3(-x, y, -1)); // -Z
    }
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
//
float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
}

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
//void GetMuWindow(float r, float RbVis, float Rt, out float mu0, out float mu1)
//{
//    float muG = MuHorizon(r, RbVis);
//    float muT = MuHorizon(r, Rt); //(r <= Rt) ? 1.0f : 
//    mu0 = muG + MU_EPS; // lift off horizon
//    mu1 = max(mu0 + 1e-5f, muT); // keep span > 0
//}
void GetMuWindow(float r, float RbVis, float Rt, out float mu0, out float mu1)
{
    float muG = MuHorizon(r, RbVis);
    mu0 = muG + MU_EPS;

    // Critical bit: inside => mu1 = 1, outside => mu1 = MuHorizon(r, Rt)
    mu1 = (r <= Rt) ? 1.0f : MuHorizon(r, Rt);

    // keep span > 0
    mu1 = max(mu0 + 1e-5f, mu1);
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

float3 SamplePsiMSOrbit(float r, float muS, float Rb, float Rt)
{
    float u = acos(clamp(muS, -1.0f, 1.0f)) / PI;
    float v = 1.0f - saturate((r - Rb) / max(Rt - Rb, 1e-6f));
    v = clamp(v, 1.0e-3f, 1.0f - 1.0e-3f);
    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0).rgb;
}

struct OrbitMarch
{
    float3 L; // in-scattered radiance along the segment
    float3 T; // RGB transmittance over the segment
};

OrbitMarch IntegrateOrbit(float3 ro, float3 rd, float tStart, float tEnd, float RbPhys, float RbVis, float Rt)
{
    OrbitMarch o;
    o.L = 0.0f.xxx;
    o.T = 1.0f.xxx;
    if (tEnd <= tStart)
        return o;

    const float3 wSun = -normalize(direction.xyz);
    const float3 Esun = radiance.rgb * SunIntensity;
    const float muPhase = clamp(dot(wSun, rd), -1.0f, 1.0f);
    const float PR = PhaseRayleigh(muPhase);
    const float PM = PhaseMieTwoLobe(muPhase);
    
    float t = tStart;
    uint iter = 0;   
    [loop]
    while (t < tEnd - 1e-3f && iter < 4u * ORBIT_NCAP)
    {
        float hE = max(0.0f, length(ro + rd * t) - RbPhys);
        float3 sigE = RayleighScattering * DensityRayleighOrbit(hE) + (MieScattering + MieAbsorption) * DensityMieOrbit(hE);
        float dtTau = ORBIT_TAU_STEP / max(dot(sigE, LUMA), 1e-9f);
        float dtGeo = (tEnd - t) / (float) max(1u, ORBIT_NCAP - min(iter, ORBIT_NCAP - 1u));
        float dt = clamp(min(dtTau, dtGeo), 1e-3f, tEnd - t);

        float3 p = ro + rd * (t + 0.5f * dt);
        float r = length(p);
        float h = max(0.0f, r - RbPhys);
        float dR = DensityRayleighOrbit(h);
        float dM = DensityMieOrbit(h);
        float3 sigR_s = RayleighScattering * dR;
        float3 sigM_s = MieScattering * dM;
        float3 sigmaExt = sigR_s + (MieScattering + MieAbsorption) * dM + O3_COEFF_ORBIT * DensityOzoneOrbit(h);

        float3 upS = p / max(r, 1e-3f);
        float muS = dot(upS, wSun);
        float3 Tsun = T_to_TOA(r, muS, RbPhys, Rt) * SunVisibilityAtR(r, muS, RbVis, SunDiscRadius);

        float3 S1 = (sigR_s * PR + sigM_s * PM) * Tsun * Esun;
        float3 SMS = (sigR_s + sigM_s) * SamplePsiMSOrbit(r, muS, RbPhys, Rt) * Esun;

        float3 stepT = exp(-sigmaExt * dt);
        float3 wInt = (1.0f.xxx - stepT) / max(sigmaExt, 1e-12f.xxx);
        o.L += o.T * (S1 + SMS) * wInt;
        o.T *= stepT;
        
        t += dt;
        iter++;
    }
    
    return o;
}

// Optional: subtle limb darkening so the disc isn't a flat sticker.
// Tweak the constant or lift it into your cbuffer later if you want.
float LimbDarken(float rNorm) // rNorm = theta / SunDiscRadius, [0..∞)
{
    const float u = 0.55f; // 0 = none, ~0.5–0.7 looks nice
    float mu = sqrt(saturate(1.0f - saturate(rNorm) * saturate(rNorm)));
    return 1.0f - u * (1.0f - mu);
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

float2 OctEncodeHemi(float3 n)
{
    n = normalize(n);
    n.z = max(n.z, 0.0);
    n /= (abs(n.x) + abs(n.y) + n.z + 1e-8);
    return n.xy * 0.5 + 0.5;
}
float3 OctDecodeHemi(float2 uv)
{
    float2 f = uv * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0)
    { // unfold fold onto the rim
        float2 s = (f >= 0.0) ? 1.0.xx : -1.0.xx;
        n.x = (1.0 - abs(n.y)) * s.x;
        n.y = (1.0 - abs(n.x)) * s.y;
        n.z = 0.0;
    }
    return normalize(n);
}

// Fold UV back into the octa diamond (hemi, z>=0)
float2 OctFoldHemiUV(float2 uv) // uv in [0,1]^2
{
    float2 f = uv * 2.0 - 1.0; // [-1,1]
    float2 a = abs(f);
    if (a.x + a.y > 1.0)
    {
        // reflect across the diagonal
        f = (1.0 - float2(a.y, a.x)) * sign(f);
    }
    return f * 0.5 + 0.5; // back to [0,1]
}

// Cubic B-spline filtering of the AP volume in screen x/y, linear in depth.
// Four hardware trilinear taps replace sixteen point taps (Sigg & Hadwiger, GPU Gems 2, ch. 20).
// B-spline weights are never negative, so there's no overshoot: haze stays >= 0 and tau stays >= 0.
float4 SampleAP_BSplineXY(float3 uvw)
{
    uint W, H, D;
    AerialPerspective3D.GetDimensions(W, H, D);
    float2 texSize = float2(W, H);
    float2 invTexSize = 1.0f / texSize;
    
    float2 coord = uvw.xy * texSize - 0.5f;
    float2 i = floor(coord);
    float2 f = coord - i;
    
    float2 f2 = f * f;
    float2 f3 = f2 * f;
    float2 w0 = (1.0f / 6.0f) * (1.0f - 3.0f * f + 3.0f * f2 - f3);
    float2 w1 = (1.0f / 6.0f) * (4.0f - 6.0f * f2 + 3.0f * f3);
    float2 w2 = (1.0f / 6.0f) * (1.0f + 3.0f * f + 3.0f * f2 - 3.0f * f3);
    float2 w3 = (1.0f / 6.0f) * f3;

    float2 g0 = w0 + w1;
    float2 g1 = w2 + w3;
    float2 p0 = (i - 1.0f + w1 / g0 + 0.5f) * invTexSize;
    float2 p1 = (i + 1.0f + w3 / g1 + 0.5f) * invTexSize;
    
    float4 s00 = AerialPerspective3D.SampleLevel(ClampLinear, float3(p0.x, p0.y, uvw.z), 0);
    float4 s10 = AerialPerspective3D.SampleLevel(ClampLinear, float3(p1.x, p0.y, uvw.z), 0);
    float4 s01 = AerialPerspective3D.SampleLevel(ClampLinear, float3(p0.x, p1.y, uvw.z), 0);
    float4 s11 = AerialPerspective3D.SampleLevel(ClampLinear, float3(p1.x, p1.y, uvw.z), 0);
    
    return g0.y * (g0.x * s00 + g1.x * s10) + g1.y * (g0.x * s01 + g1.x * s11);
}

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

    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius);
    const float RbHit = RbPhys + GroundBiasMeters(RbPhys);
    
    float3 camWS = cameraPosition.xyz;
    float3 camRel = camWS - PlanetCenterWS;
    float rCam = max(RbPhys, length(camRel));
    float heightCam = rCam - RbPhys;

    bool bake = (BakeIBL > 0.5f);

    float3 wView;
    
    // If baking, do NOT sample SceneDepth/SceneColor and do NOT do ground-occlusion early-outs.
    if (bake)
    {
        wView = CubemapDirFromFaceUV(FaceIndex, uv); // unit
        
        // Same μ window as writer (with the corrected GetMuWindow):
        float mu0, mu1;
        
        float3 spinUp = normalize(BasisSpinUp);
        float3 east0 = normalize(BasisTanEast);
        float3 north0 = normalize(BasisTanNorth);

        // Build local basis (as you already do)
        float3 up, east, north;
        BuildSkyBasisAnchored(camWS, PlanetCenterWS, east0, north0, spinUp, up, east, north);
        
        GetMuWindow(rCam, RbVis, Rt, mu0, mu1);
        
        // Sky-view LUT lookup (shared mapping, Utilities/SkyViewLUT.hlsli).
        // Directions below the horizon read the horizon row,
        uint Wsv, Hsv;
        SkyViewLUT.GetDimensions(Wsv, Hsv);
        float3 fSun, sSun;
        SkyViewSunFrame(up, -normalize(direction.xyz), east, fSun, sSun);
        float2 uvSky = SkyViewUV(wView, up, fSun, sSun, mu0, mu1, float2(Wsv, Hsv));
        float3 sky = SkyViewLUT.SampleLevel(ClampLinear, uvSky, 0).rgb;

        // From space, directions above the top-of-atmosphere horizon miss the atmosphere.
        if (rCam > Rt && dot(wView, up) > mu1)
            sky = 0.0f.xxx;
        
        // Optional: for IBL you usually exclude sun disc (keep analytic sun in lighting),
        // or include it only in radiance, not in irradiance.
        float3 outSky = sky; // + maybe sunColor

        output.color = float4(max(outSky, 0.0f), 1.0f);
        output.disc = 0.0f;
        output.halo = 0.0f;
        return output;
    }
    
    wView = ViewDirWS_fromUV(uv);
    
    // If no geometry wrote to depth, draw SKY using the precomputed SkyView LUT
    if (depth <= 1e-12f)
    {        
        Hit h = IntersectSphereGrazingSafe(camRel, wView, RbHit);
        
       // Same μ window as writer (with the corrected GetMuWindow):
        float mu0, mu1;
        
        float3 spinUp = normalize(BasisSpinUp);
        float3 east0 = normalize(BasisTanEast);
        float3 north0 = normalize(BasisTanNorth);

        // Build local basis (as you already do)
        float3 up, east, north;
        BuildSkyBasisAnchored(camWS, PlanetCenterWS, east0, north0, spinUp, up, east, north);

        // Local components & physical mu
        float mu = dot(wView, up);
              
        if (h.ok && h.t1 > 0.0f)
        {
            // fully occluded by ground: no sky, no sun, no halo
            output.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
            output.disc = 0.0f;
            output.halo = 0.0f;
            return output;
        }
        
        uint Wsv, Hsv;
        SkyViewLUT.GetDimensions(Wsv, Hsv);
       
        float3 upCam = camRel / rCam;
        float rWin = rCam;     

        GetMuWindow(rCam, RbVis, Rt, mu0, mu1);
        
        // Early reject: if mu < mu0 the ground occludes (you already do a ground ray test;
        // this is a cheap extra guard that also helps at grazing angles).
        if (mu <= mu0)
        {
            // fully occluded by ground
            output.color = 0;
            output.disc = 0;
            output.halo = 0;
            return output;
        }

        float3 fSun, sSun;
        SkyViewSunFrame(up, -normalize(direction.xyz), east, fSun, sSun);
        float2 uvSky = SkyViewUV(wView, up, fSun, sSun, mu0, mu1, float2(Wsv, Hsv));
        float3 sky = SkyViewLUT.SampleLevel(ClampLinear, uvSky, 0).rgb;
        
        // From space, directions above the top-of-atmosphere horizon miss the atmosphere:
        // no sky light (the paper's ray march adds nothing when there is no intersection).
        if (rCam > Rt && mu > mu1)
            sky = 0.0f.xxx;
        
        float wOrbitSky = smoothstep(Rt - ORBIT_BLEND_M, Rt, rCam);
        if (wOrbitSky > 0.0f)
        {
            Hit hA = IntersectSphereGrazingSafe(camRel, wView, Rt);
            float3 skyMarch = 0.0f.xxx;
            if (hA.ok && hA.t1 > 0.0f)
                skyMarch = IntegrateOrbit(camRel, wView, max(0.0f, hA.t0), hA.t1, RbPhys, RbVis, Rt).L;
            sky = lerp(sky, skyMarch, wOrbitSky);
        }
 
        float3 sunColor = float3(0.0f, 0.0f, 0.0f);
        float discMask_out = 0.0f; // SV_Target1
        float haloMask_out = 0.0f; // SV_Target2

        if (SunDiscToggle != 0)
        {
            // ===============================
            // Geometry
            // ===============================
            float3 vDir = normalize(wView); // cam → pixel
            float3 wSun = -normalize(direction.xyz); // TO sun

            float mu = clamp(dot(vDir, wSun), -1.0, 1.0);
            float theta = acos(mu); // radians, center→pixel angular distance

            float edgePxR = 1.5f;
            float dmu = length(float2(ddx(mu), ddy(mu))) + 1e-7f;
            float dtheta = dmu / max(sin(theta), 1e-4f);
            float wEdge = edgePxR * dtheta; // radians

            float rDiscRad = SunDiscRadius; // radians
            float sunDiscRadiusDeg = degrees(rDiscRad);

            // Disc mask (AA in theta-space)
            float discMask = 1.0f - smoothstep(rDiscRad, rDiscRad + wEdge, theta);

            // ===============================
            // Visibility: share of the disc above the geometric horizon.
            // No refraction term: on Mars it is ~0.01° (the air is ~1/60 as dense as Earth's).
            // ===============================
            float altUpDeg = degrees(asin(clamp(dot(upCam, wSun), -1.0, 1.0)));
            float dipGroundDeg = degrees(acos(saturate(RbPhys / rCam)));
            float altRelGroundDeg = altUpDeg + dipGroundDeg;

            // Ground occlusion (frontHit)
            float3 oc = camRel;
            float3 rd = normalize(wSun);
            float b = dot(oc, rd);
            float c = dot(oc, oc) - (RbPhys * RbPhys);
            float h = b * b - c;
            bool frontHit = (h >= 0.0f) && ((-b - sqrt(max(h, 0.0f))) >= 0.0f);

            float visSoft = smoothstep(-sunDiscRadiusDeg, +sunDiscRadiusDeg, altRelGroundDeg);
            float discVis = frontHit ? visSoft : 1.0f;

            // ===============================
            // Disc radiance: sunlight through the real atmosphere along the eye→sun ray
            // (from the ground or from space). Its colour comes only from T_eye_sun.
            // ===============================
            float3 T_eye_sun = SampleTransmittance_EyeToSun(camRel, vDir, wSun, RbPhys, Rt);
            float3 discRGB = SunIntensity * 10.0f * SunDiscWhite * T_eye_sun;
            
            // Core boost for the HDR display (a display decision: revisited with exposure in 16c)
            float thetaInner = 0.60f * rDiscRad;
            float mInner = 1.0f - smoothstep(thetaInner, thetaInner + wEdge, theta);
            
            const float PW = 200.0f;
            const float SunHDRBoostNits = 6000.0f;
            float boostLin = SunHDRBoostNits / PW;
            
            float3 discRGBBoosted = discRGB + (boostLin * mInner).xxx;
            float3 discRadiance = discRGBBoosted * discMask * discVis;
            
            sunColor += discRadiance;
                  
            // ===============================
            // Halo MASKS only: no light is added here any more. The aureole is the real one
            // from the scattering (Fix 11). These masks still tell the god-ray, bloom and
            // tone-mapping passes where the sun is, until those passes are reworked.
            // ===============================
            
            // Air mask shape (angle from the sun's centre)
            float muDisc = cos(rDiscRad);
            float denom = max(1.0f - muDisc, 1e-12f);
            float rNorm = sqrt(max((1.0f - mu) / denom, 0.0f)); // 0 center, 1 edge, >1 outside
            float degAir = rNorm * sunDiscRadiusDeg;
            
            float tight = exp(-pow(degAir / 1.8, 2.0));
            float shoulder = 1.0 / (1.0 + pow(degAir / 4.5, 2.2));
            float tailMain = 1.0 / pow(1.0 + (degAir / 10.0), 1.25);
            float tailUltra = 1.0 / pow(1.0 + (degAir / 25.0), 1.35);
            
            float haloCore = 0.70f * tight + 0.55f * shoulder;
            float haloOuter = (0.28f * tailMain + 0.06f * tailUltra) * 0.85f;
            float haloMaskAirShape = haloCore + haloOuter;
            
            // Space mask shape
            float degTheta = degrees(theta);
            const float MaskInnerSigmaDeg = 0.14f;
            const float MaskOuterSigmaDeg = 1.80f;
            const float MaskOuterWeight = 0.35f;
            const float MaskCutoffDeg = 7.0f;
            const float MaskCutoffFeather = 1.25f;

            // Distance outside disc (deg)
            float xSpace = max(degTheta - sunDiscRadiusDeg, 0.0f);
            float tInner = xSpace / MaskInnerSigmaDeg;
            float gInner = exp2(-0.7213475f * tInner * tInner);
            float eOuter = exp2(-1.442695f * (xSpace / MaskOuterSigmaDeg));
            float cutoffFade = 1.0f - smoothstep(MaskCutoffDeg - MaskCutoffFeather, MaskCutoffDeg, xSpace);
            float spaceHaloShape = cutoffFade * (gInner + MaskOuterWeight * eOuter);
            
            // ====== WRITE MASKS ======
            discMask_out = saturate(discMask * discVis);

            float haloMaskAir = saturate(haloMaskAirShape * discVis);
            float haloMaskSpace = saturate(spaceHaloShape * discVis); 
            
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
        // TOA segment
        Hit hitAtm = IntersectSphereGrazingSafe(camRel, wView, Rt);

        float tEnter = max(0.0f, hitAtm.t0);
        float tSurf = ViewDistanceFromDepth(uv, depth);
        
        float lengthInAtmosphere = (rCam <= Rt) ? tSurf : max(0.0f, tSurf - tEnter);
        
        float APFar = asfloat(APFarU32.Load(int3(0, 0, 0)));
        float d = (APFar > 1e-6f) ? saturate(lengthInAtmosphere / APFar) : 0.0f;
        float u = pow(d, 1.0f / AP_Z_GAMMA);
             
        // Slice z holds the haze accumulated from the camera to the END of slice z,
        // at distance APFar * ((z + 1) / D)^AP_Z_GAMMA (see AerialPerspectiveCS).
        // The continuous slice index for this pixel's distance is therefore u * D - 1.
        uint Wd, Hd, Dd;
        AerialPerspective3D.GetDimensions(Wd, Hd, Dd);
        float DdF = (float) Dd;
        float sliceF = u * DdF - 1.0f;
        //
        // Closer than the end of slice 0 there is nothing nearer to interpolate towards:
        // read slice 0 and fade it linearly to zero at the camera (Hillaire 2020 reference does the same).
        // d * D^gamma = distance / (end of slice 0).
        float weight = saturate(d * pow(DdF, AP_Z_GAMMA));
        
        float wAP = (max(sliceF, 0.0f) + 0.5f) / DdF; // texel centre of the continuous slice index
        
        // final sample: TRILINEAR
#if AP_XY_BSPLINE
        float4 ap = SampleAP_BSplineXY(float3(uv, wAP));
#else
        float4 ap = AerialPerspective3D.SampleLevel(ClampLinear, float3(uv, wAP), 0);
#endif
        ap.rgb *= weight;
        float tau = max(ap.a, 0.0f) * weight;
        
        float3 betaExt = RayleighScattering + MieScattering + MieAbsorption; // 1/m
        float betaAvg = (betaExt.r + betaExt.g + betaExt.b) * (1.0f / 3.0f);
        float3 k = betaExt / max(betaAvg, 1e-9);
        
        float Tmean = exp(-tau);
        float3 Trgb = pow(Tmean.xxx, k);
        
        float3 outRGB = colorPreAtmos * Trgb + ap.rgb;
        float alphaOut = max(Trgb.r, max(Trgb.g, Trgb.b));

        float wOrbit = smoothstep(Rt - ORBIT_BLEND_M, Rt, rCam);
        if (wOrbit > 0.0f && hitAtm.ok)
        {
            OrbitMarch m = IntegrateOrbit(camRel, wView, tEnter, min(tSurf, hitAtm.t1), RbPhys, RbVis, Rt);
            outRGB = lerp(outRGB, colorPreAtmos * m.T + m.L, wOrbit);
            alphaOut = lerp(alphaOut, max(m.T.r, max(Trgb.g, Trgb.b)), wOrbit);
        }

        output.color = float4(outRGB, alphaOut);

        output.disc = 0.0f; // no sun over geometry pass here
        output.halo = 0.0f; // no halo mask over geometry pixels
        return output;
    }
}