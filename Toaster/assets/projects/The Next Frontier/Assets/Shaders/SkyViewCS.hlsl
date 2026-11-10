#type compute
#pragma pack_matrix(row_major)

// ===== Debug / toggles ======================================================
#ifndef SKY_STEPS
#define SKY_STEPS             96
#endif

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
    float3 PlanetCenterCR;
    float PlanetRadius;
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
    float Altitude;
    float3 BasisLonEast;
    float3 BasisLonNorth;
    float3 BasisSpinUp;
};

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight; // Rt - Rg
    float RayScaleHeight;
    float MieScaleHeight;
    float MSGain;
    
    float3 RayleighScattering;
    float SGain;
    
    float3 MieScattering;
    
    float3 MieAbsorption;
    
    float3 GroundAlbedo;
    
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
    float SunDiscRadius;
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

// ===== LUTs =================================================================
Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
Texture1D<uint> HorizonMu : register(t2);

SamplerState ClampLinear    : register(s0);
SamplerState ClampPoint     : register(s1);

RWTexture2D<float4> OutSkyView : register(u0);

// ===== Constants / helpers ==================================================
static const float PI = 3.14159265358979323846f;
static const float TWO_PI = 6.283185307179586f;
static const float INV4PI = 0.25f / PI;
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);
static const float MU_EPS = 8e-4;

// Ozone (Bruneton)
static const float3 O3_COEFF = float3(0.650e-6, 1.881e-6, 0.085e-6);
//
#include "TransmittanceLUT.hlsli"
#include "SkyViewLUT.hlsli"

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

void GetMuWindow(float r, float RbVis, float Rt, out float mu0, out float mu1)
{
    float muG = MuHorizon(r, RbVis);
    mu0 = muG + MU_EPS;

    // Critical bit: inside => mu1 = 1, outside => mu1 = MuHorizon(r, Rt)
    mu1 = (r <= Rt) ? 1.0f : MuHorizon(r, Rt);

    // keep span > 0
    mu1 = max(mu0 + 1e-5f, mu1);
}

float DensityExpTapered(float h, float H)
{
    float hTop = AtmosphereHeight - min(0.0f, MinHeight);
    H = max(H, 1e-3f);
    return max(exp(-max(h, 0.0f) / H) - exp(-hTop / H), 0.0f);
}

float DensityRayleigh(float h)
{
    return DensityExpTapered(h, RayScaleHeight);
}
float DensityMie(float h)
{
    return DensityExpTapered(h, MieScaleHeight);
}
float DensityOzone(float hMeters)
{
    // Triangle 10–40 km peaking at 25 km.
    // Normalize so that the column integral equals OzoneStrength (area = 15000 m).
    float km = hMeters * 1e-3f;
    float tri = saturate(1.0f - abs((km - 25.0f) / 15.0f));
    return tri * (OzoneStrength / 15000.0f);
}

float PhaseRayleigh(float mu)
{
    return (3.0f * INV4PI) * 0.25f * (1.0f + mu * mu);
}

float3 T_to_TOA(float r, float mu, float RbPhys, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, RbPhys, Rt), 0).rgb;
}

float3 T_along_ray(float r, float mu, float t, float RbVis, float RbPhys, float Rt)
{
    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
    rd = clamp(rd, RbVis + 5e-4f, Rt - 5e-4f);
    float muD = clamp((r * mu + t) / rd, -1.0f + MU_EPS, 1.0f - MU_EPS);
    float3 num = T_to_TOA(r, mu, RbPhys, Rt);
    float3 den = T_to_TOA(rd, muD, RbPhys, Rt);
    return saturate(num / max(den, float3(1e-6f, 1e-6f, 1e-6f))); // fix 1e-6.xxx
}

// MultiScatter LUT sampling: x=theta_s/π, y = 1 - linear altitude (top=TOA)
float4 SamplePsiMS4(float r, float muS, float Rb, float Rt)
{
    float thetaS = acos(clamp(muS, -1.0f, 1.0f));
    float u = thetaS / PI;
    float v = 1.0f - saturate((r - Rb) / max(Rt - Rb, 1e-6f));

    // NEW: keep away from the very first/last row to prevent seams
    v = clamp(v, 1.0e-3f, 1.0f - 1.0e-3f);

    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0);
}

float SunVisibilityAtR(float r, float muS, float RbVis)
{
    float sH = RbVis / r;
    float cH = -sqrt(saturate(1.0f - sH * sH));
    return smoothstep(-sH * SunDiscRadius, sH * SunDiscRadius, muS - cH);
}

float SunVisibleRayTest(float3 p, float3 sToSun, float Rb)
{
    // p is relative to planet center
    // sToSun is normalized direction toward sun
    float b = dot(p, sToSun);
    float c = dot(p, p) - Rb * Rb;

    // If closest approach is behind origin, ray points away from planet => visible
    if (b >= 0.0f)
        return 1.0f;

    float disc = b * b - c;
    // If ray intersects planet (disc > 0), sun is occluded
    return (disc > 1e-6f) ? 0.0f : 1.0f;
}

void BuildSkyBasisAnchored(float3 camWS, float3 basisEastWS, float3 basisNorthWS, float3 spinUpWS, out float3 up, out float3 east, out float3 north)
{
    // 1) Radial up (center → camera)
    float3 rel = camWS - PlanetCenterCR;
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

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint W, H;
    OutSkyView.GetDimensions(W, H);
    if (tid.x >= W || tid.y >= H)
        return;

    float u = (tid.x + 0.5f) / float(W);

    float3 wSun = -normalize(direction.xyz);
    float3 Esun = radiance.rgb * SunIntensity;

    const float R_BIAS = max(1.0f, 2e-6f * PlanetRadius);
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + R_BIAS;
    const float RbHit = RbPhys + R_BIAS;

    float3 camWS = cameraPosition.xyz;
    float3 camRel = camWS - PlanetCenterCR;
    float rCam = max(RbPhys, length(camRel));

    float rWin = rCam;
    float3 camRelWin = camRel;
    
    // Stable per-frame μ window
    float mu0, mu1; // mu1==1
    GetMuWindow(rWin, RbVis, Rt, mu0, mu1);
    
    // local basis
    float3 up, east, north;
    BuildSkyBasisAnchored(camWS, normalize(BasisTanEast), normalize(BasisTanNorth), normalize(BasisSpinUp), up, east, north);
    
    // LUT texel -> view direction (shared mapping, Utilities/SkyViewLUT.hlsli)
    float3 fSun, sSun;
    SkyViewSunFrame(up, wSun, east, fSun, sSun);
    float3 wView = SkyViewDir(tid.xy, uint2(W, H), up, fSun, sSun, mu0, mu1);
    
    Hit hatm = IntersectSphereGrazingSafe(camRelWin, wView, Rt);
    if (!hatm.ok)
    {
        OutSkyView[tid.xy] = float4(0, 0, 0, 1);
        return;
    }
    //
    float tEnter = max(0.0f, hatm.t0);
    float tExit = max(0.0f, hatm.t1);

    Hit hg = IntersectSphereGrazingSafe(camRelWin, wView, RbHit);
    if (hg.ok && hg.t0 > 0.0f)
        tExit = min(tExit, hg.t0);

    float L = max(tExit - tEnter, 1e-6f);

    // Keep rEntry strictly inside [RbVis, Rt] for stable TLUT lookups (still used for Tsun)
    float3 pEntry = camRelWin + wView * tEnter;
    float rEntry = clamp(length(pEntry), RbVis + 5e-4f, Rt - 5e-4f);
    float3 upEntry = pEntry / rEntry;
    float muEntry = dot(wView, upEntry);

    float3 Ls = 0.0.xxx;
    float3 Lms = 0.0.xxx;
    float3 Tacc = 1.0.xxx;
    
    // Δτ controls
    const float tauStep = 0.03f; // target optical depth per step (0.025..0.035 good)
    const float dtMin = 1e-4f; // clamp against tiny steps
    const uint Ncap = SKY_STEPS; // soft cap on number of steps (same as before)

    float t = tEnter;
    uint iter = 0;

    [loop]
    while (t < tExit - 1e-7f && iter++ < 4u * Ncap)   // safety cap
    {
        // Estimate step size from local extinction (energy in thin air)
        // Use current edge to pick a *candidate* dt, then evaluate at mid
        float3 pEdge = camRelWin + wView * t;
        float rEdge = length(pEdge);
        float hEdge = max(0.0f, rEdge - RbPhys);

        float dR_e = DensityRayleigh(hEdge);
        float dM_e = DensityMie(hEdge);

        float3 sigR_e = RayleighScattering * dR_e;
        float3 sigM_s_e = MieScattering * dM_e;
        float3 sigM_a_e = MieAbsorption * dM_e;

        float3 sigma_t_e = sigR_e + sigM_s_e + sigM_a_e; // (no ozone on Mars)
        float sigmaY = max(dot(sigma_t_e, LUMA), 1e-6);

        float dt_tau = tauStep / sigmaY; // Δτ → Δs
        float dt_geo = (tExit - t) / max(1u, (Ncap - min(iter, Ncap - 1))); // soft geom cap
        float dt = clamp(min(dt_geo, dt_tau), dtMin, tExit - t); // final dt

        // Midpoint sample
        float ti = t + 0.5f * dt;
        //float3 pRel = camRelWin + wView * ti;
        //float rp = clamp(length(pRel), RbVis + 5e-4f, Rt - 5e-4f);
        //float h = max(0.0f, rp - RbPhys);
        
        float3 pRel = camRelWin + wView * ti;
        float rpTrue = length(pRel);
        float3 upS = (rpTrue > 0.0f) ? (pRel / rpTrue) : up;
        // Keep clamped version ONLY for LUT sampling
        float rpLUT = clamp(rpTrue, RbVis + 5e-4f, Rt - 5e-4f);
        float h = max(0.0f, rpLUT - RbPhys);

        float dR = DensityRayleigh(h);
        float dM = DensityMie(h);
        float dO = DensityOzone(h);

        float3 sigR_s = RayleighScattering * dR;
        float3 sigM_s = MieScattering * dM;
        float3 sigM_a = MieAbsorption * dM;
        float3 sigmaExt = sigR_s + sigM_s + sigM_a + O3_COEFF * dO;

        // Transmittance from camera to segment center (use symmetric half-step)
        float3 Tmid = Tacc * exp(-sigmaExt * (0.5.xxx * dt));

        // Prepare phase/lighting
        float muPhase = clamp(dot(wSun, wView), -1.0f, 1.0f);

        //float3 upS = (rp > 0.0f) ? (pRel / rp) : up;
        float muS = dot(upS, wSun);
        
        //float Vsun = SunVisibilityAtR(rp, muS, RbVis); // smooth visibility
        float Vsun = SunVisibleRayTest(pRel, wSun, RbPhys);
        float3 Tsun = T_to_TOA(rpLUT, muS, RbPhys, Rt) * Vsun; // keep using TLUT, just gate it smoothly

        float PR = PhaseRayleigh(muPhase);
        float3 PMrgb = PhaseMieTwoLobe(muPhase);

        // single scattering: sunlight scattered once toward the camera (Rayleigh + Mie).
        // SGain applied uniformly, the same as S1 in AerialPerspectiveCS.
        Ls += Tmid * ((sigR_s * PR * Tsun) + (sigM_s * PMrgb * Tsun)) * dt;

        // --- multiple scattering (Hillaire 2020: isotropic, sigma_s * Psi_ms; identical in AerialPerspectiveCS) ---
        // Scattering coefficient for the multi-scattered light: MieScattering already excludes absorption.
        float3 sigS_ms = sigR_s + sigM_s;
        float3 PsiMS_rgb = SamplePsiMS4(rpLUT, muS, RbPhys, Rt).rgb;

        Lms += Tmid * (sigS_ms * PsiMS_rgb) * dt;

        // advance cumulative transmittance to next edge
        Tacc *= exp(-sigmaExt * dt);
        t += dt;
    }

    float3 skyRGB = (Ls + Lms) * Esun;
    OutSkyView[tid.xy] = float4(skyRGB, 1.0f);
}