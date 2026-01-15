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
    float4x4 lightViewProj;
    
    float4 direction; // FROM light -> scene
    
    float4 radiance; // RGB
    
    float SunIntensity;
    float DirectionalLightGain;
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

// Optional focus near the horizon (gives extra rows right where it matters).
// Set k≈0.75..0.9. k=1 → linear.
float FocusT(float t, float k)
{
    return pow(saturate(t), k);
}

float DensityRayleigh(float h)
{
    return exp(-max(h, 0.0f) / max(RayScaleHeight, 1e-3f));
}
float DensityMie(float h)
{
    return exp(-max(h, 0.0f) / max(MieScaleHeight, 1e-3f));
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
float PhaseMieHG(float mu, float g)
{
    float g2 = g * g;
    float d = 1.0f + g2 - 2.0f * g * mu;
    return INV4PI * (1.0f - g2) / max(pow(d, 1.5f), 1e-5f);
}

float2 TransUV(float r, float mu, float RbPhys, float Rt)
{
    float rNorm = (r - RbPhys) / max(Rt - RbPhys, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (RbPhys * RbPhys) / (r * r)));
    mu = clamp(mu, muMin + MU_EPS, 1.0f - MU_EPS); 
    return float2((mu - muMin) / (1.0f - muMin), saturate(rNorm));
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
float3 SamplePsiMS(float r, float muS, float Rg, float Rt)
{
    return SamplePsiMS4(r, muS, Rg, Rt).rgb;
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
    if (b < 0.0f)
        return 1.0f;

    float disc = b * b - c;
    // If ray intersects planet (disc > 0), sun is occluded
    return (disc > 1e-6f) ? 0.0f : 1.0f;
}

// MS anisotropy blend using LUT alpha as gBar
float MSPhase(float mu, float gBar)
{
    float pIso = INV4PI;
    float g = saturate(gBar);
    float g2 = g * g;
    float d = 1.0f + g2 - 2.0f * g * mu;
    float pHG = INV4PI * (1.0f - g2) / max(pow(d, 1.5f), 1e-5f);
    return lerp(pIso, pHG, g);
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

// Small-angle disc average by inflating the scattering angle.
float PhaseMie_DiscAvg(float mu, float g)
{
    // theta_eff ≈ sqrt(theta^2 + sunRad^2)
    float theta = acos(clamp(mu, -0.9995, 0.9995));
    float thetaEff = sqrt(theta * theta + SunDiscRadius * SunDiscRadius);
    float muEff = cos(thetaEff);
    return PhaseMieHG_CS(muEff, g);
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
    
    // Pixel in the LUT
    float2 uvSky = (float2(tid.xy) + 0.5f) / float2(W, H);

    // Stable per-frame μ window
    float mu0, mu1; // mu1==1
    GetMuWindow(rWin, RbVis, Rt, mu0, mu1);

    // local basis (unchanged)
    float3 up, east, north;
    BuildSkyBasisAnchored(camWS, normalize(BasisTanEast), normalize(BasisTanNorth), normalize(BasisSpinUp), up, east, north);

    // 1) decode hemi-oct to temporary local vector m (z' in [0,1])
    float3 m = OctDecodeHemi(uvSky);

    // 2) recover azimuth and the *warped* cosine
    float phi = atan2(m.y, m.x); // [-π, π]
    float mu_p = saturate(m.z); // z'  in [0,1]

    // 3) UN-warp μ: map z' back to physical cosine in [mu0, 1]
    float mu = lerp(mu0, mu1, mu_p);

    // 4) rebuild the true local direction from (mu, phi)
    float sinTh = sqrt(saturate(1.0f - mu * mu));
    float3 wView = normalize(mu * up + sinTh * (cos(phi) * east + sin(phi) * north));

    Hit hatm = IntersectSphereGrazingSafe(camRelWin, wView, Rt);
    if (!hatm.ok)
    {
        OutSkyView[tid.xy] = float4(0, 0, 0, 1);
        return;
    }

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

    float3 g = saturate(MieAnisotropy);
    float3 f = g * g;
    float3 g_p = (g - f) / max(1.0.xxx - f, 1e-6.xxx);

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
        float muPhase = clamp(dot(wSun, wView), -0.9995f, 0.9995f);

        //float3 upS = (rp > 0.0f) ? (pRel / rp) : up;
        float muS = dot(upS, wSun);
        
        //float Vsun = SunVisibilityAtR(rp, muS, RbVis); // smooth visibility
        float Vsun = SunVisibleRayTest(pRel, wSun, RbPhys);
        float3 Tsun = T_to_TOA(rpLUT, muS, RbPhys, Rt) * Vsun; // keep using TLUT, just gate it smoothly

        float PR = PhaseRayleigh(muPhase);
        float3 PMrgb = float3(PhaseMie_DiscAvg(muPhase, g_p.r), PhaseMie_DiscAvg(muPhase, g_p.g), PhaseMie_DiscAvg(muPhase, g_p.b));

        // Remove delta peak for single scattering
        float3 sigM_s_single = sigM_s * (1.0.xxx - f);       
        
        // --- use the *physical* horizon for all horizon/elevation logic -------------
        float cH_phys = MuHorizon(rpTrue, RbPhys); // real horizon (PlanetRadius + MinHeight)

        // Sun elevation above the *physical* horizon: 0 at horizon, 1 at zenith
        float elevS = saturate((muS - cH_phys) / (1.0 - cH_phys));

        // Low-sun factor (broad ramp that peaks near the horizon)
        float fLowSun = 1.0 - smoothstep(0.35, 0.85, elevS);

        // View elevation above the *physical* horizon: 0 at horizon, 1 at zenith
        float muView = dot(wView, upS);
        muView = clamp(muView, -0.9995f, 0.9995f);
        float elevV = saturate((muView - cH_phys) / (1.0 - cH_phys));

        // Horizon band that *peaks above the rim* (≈6–25° up)
        // Strong near elevV≈0.10..0.40, fades out as you look higher
        float fBandUp = (1.0 - smoothstep(0.10, 0.40, elevV)); // tune 0.10..0.40

        // Gentle sunward bias so the far anti-sun horizon doesn’t turn blue
        float fSunward = smoothstep(0.20, 0.80, muPhase);

        // Final weight: low sun × above-horizon band × mild sunward bias
        float fBlue = saturate(fLowSun * fBandUp * fSunward);

        // --- single-scatter tint/gain (unchanged otherwise) -------------------------
        float3 Tint = float3(0.0f, 0.0f, 0.0f); //       lerp(1.0.xxx, SunsetTint, fBlue);
        float LsGain = lerp(1.0, SGain, fBlue);

        // single scattering
        Ls += Tmid * ((sigR_s * PR * Tsun) + (sigM_s_single * PMrgb * Tsun)) * dt * Tint * LsGain;

        // --- multiple scattering (near-isotropic w/ tiny bias, energy-preserving) ---
        float4 Psi4 = SamplePsiMS4(rpLUT, muS, RbPhys, Rt);
        float gBar = saturate(Psi4.a);
        float alt01 = saturate((rpLUT - RbPhys) / max(Rt - RbPhys, 1e-6));
        
        // very low effective g for multi-scatter
        float gEff = min(gBar, lerp(0.35, 0.45, alt01));
        float pHG_e1 = 4.0f * PI * MSPhase(muPhase, gEff); // avg = 1
        float wAniso = 0.20; // 20% of the lobe
        float pMS_e1 = 1.0 + wAniso * (pHG_e1 - 1.0);
        
        // altitude equalizer (flatten vertical contrast; keep MSGain ≈ 1.0)
        float baseBoost = 1.0 + 0.18 * saturate(1.0 - (MSGain - 1.0) / 0.3);
        float gainAlt = lerp(baseBoost, 1.0, alt01 * alt01);
        
        float3 PsiMS_dir = (MSGain * gainAlt) * Psi4.rgb * pMS_e1;
        
        // optional: 20–30% pull toward per-altitude mean to avoid dark anti-sun
       // float3 PsiMS_iso = (MSGain * gainAlt) * SamplePsiMS4(rp, 0.0, RbPhys, Rt).rgb;
        float3 PsiMS_iso = (MSGain * gainAlt) * SamplePsiMS4(rpLUT, muS, RbPhys, Rt).rgb;
        float even = 0.30f * Vsun; // no iso pull when sun is occluded
        float3 PsiMS_rgb = lerp(PsiMS_dir, PsiMS_iso, even);

        float3 w0M = sigM_s / max(sigM_s + sigM_a, 1e-6.xxx);
        float3 sigS_ms = sigR_s + sigM_s * w0M;

        Lms += Tmid * (sigS_ms * PsiMS_rgb) * dt;

        // advance cumulative transmittance to next edge
        Tacc *= exp(-sigmaExt * dt);
        t += dt;
    }

    float3 skyRGB = (Ls + Lms) * Esun;
    OutSkyView[tid.xy] = float4(skyRGB, 1.0f);
}