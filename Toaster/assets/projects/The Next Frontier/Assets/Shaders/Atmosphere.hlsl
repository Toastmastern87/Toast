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
    
    float3 SunDiscPad0;
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
Texture2D<float4> GroundIrradianceLUT   : register(t6);
Texture2D<float4> SkyShadowRemoved      : register(t7);
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
#include "GroundIrradianceLUT.hlsli"
#include "AtmosphereRays.hlsli"

static const float ORBIT_TAU_STEP = 0.03f; // target optical depth per step(same as SkyViewCS)
static const uint ORBIT_NCAP = 64u; // soft step cap; hard cap is 4x (same scheme as SkyViewCS)
static const float3 O3_COEFF_ORBIT = float3(0.650e-6f, 1.881e-6f, 0.085e-6f); // as TransmittanceCS (Bruneton)
static const float ORBIT_BLEND_M = 20000.0f;

// Linear limb-darkening law: I(mu) / I(centre) = 1 - u * (1 - mu). u ~ 0.55 in the visible.
// Its average over the disc is 1 - u/3 (used by the disc in main to keep the total energy)
static const float SUN_LIMB_U = 0.55f;

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

float GroundBiasMeters(float Rg)
{
    return max(1.0f, 2e-6f * Rg);
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

float3 GroundRadianceBake(float3 camRel, float3 wView, float tGround, float RbPhys, float RbVis, float Rt)
{
    const float3 wSun = -normalize(direction.xyz);
    const float3 Esun = radiance.rgb * SunIntensity;
    
    float3 pG = camRel + wView * tGround;
    float rG = length(pG);
    float muSG = dot(pG / max(rG, 1e-3f), wSun);
    
    float3 Tsun = T_to_TOA(rG, muSG, RbPhys, Rt) * SunVisibilityAtR(rG, muSG, RbVis, SunDiscRadius);
    float3 EsunOverPi = Tsun * max(muSG, 0.0f) * (1.0f / PI);
    float3 EskyOverPi = GroundIrradianceLUT.SampleLevel(ClampLinear, GroundIrradianceUV(rG, muSG, RbPhys, Rt), 0).rgb;
    float3 Lground = GroundAlbedo * (EsunOverPi + EskyOverPi) * Esun;
    
    Hit hA = IntersectSphereGrazingSafe(camRel, wView, Rt);
    float tEnter = hA.ok ? max(0.0f, hA.t0) : tGround;
    OrbitMarch m = IntegrateOrbit(camRel, wView, tEnter, tGround, RbPhys, RbVis, Rt);
    
    return Lground * m.T + m.L;
}

float LimbDarken(float rNorm) // rNorm = theta / SunDiscRadius, [0..∞)
{
    float mu = sqrt(saturate(1.0f - saturate(rNorm) * saturate(rNorm)));
    return 1.0f - SUN_LIMB_U * (1.0f - mu);
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
        
        Hit hG = IntersectSphereGrazingSafe(camRel, wView, RbHit);
        if (hG.ok && hG.t0 > 0.0f)
        {
            output.color = float4(max(GroundRadianceBake(camRel, wView, hG.t0, RbPhys, RbVis, Rt), 0.0f), 1.0f);
            output.disc = 0.0f;
            output.halo = 0.0f;
            return output;
        }
        
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
        sky = max(sky - SkyShadowRemoved.SampleLevel(ClampLinear, uv, 0).rgb, 0.0f);
        
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
            float discMask = 1.0f - smoothstep(rDiscRad - 0.5f * wEdge, rDiscRad + 0.5f * wEdge, theta);

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
            float3 T_eye_sun = SampleTransmittance_EyeToSun(camRel, vDir, RbPhys, Rt);
            
            float sinHalf = sin(0.5f * rDiscRad);
            float omegaSun = 4.0f * PI * sinHalf * sinHalf; // sr, = 2π(1 - cos r)
            float limb = LimbDarken(theta / rDiscRad) / (1.0f - SUN_LIMB_U / 3.0f); // average 1 over the disc
            
            float3 discRGB = radiance.rgb * SunIntensity / omegaSun * limb * T_eye_sun;
            
            const float FP16_SAFE_MAX = 60000.0f;
            float discPeak = max(discRGB.r, max(discRGB.b, discRGB.g));
            discRGB *= min(1.0f, FP16_SAFE_MAX / max(discPeak, 1e-6f));
            
            sunColor += discRGB * discMask * discVis;
                  
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