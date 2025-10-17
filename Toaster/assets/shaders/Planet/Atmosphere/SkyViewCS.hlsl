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
    float RayScaleHeight;
    float MieScaleHeight;
    float3 RayleighScattering;
    float3 MieScattering;
    float3 MieAbsorption;
    float3 GroundAlbedo;
    float3 MieAnisotropy;
    float OzoneStrength;
    uint StepsTransmittance;
    uint StepsMultiScattering;
    float APFarDynamic;
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

cbuffer FloatingOrigin : register(b7)
{
    float3 WorldOffsetWS;
};

// ===== LUTs =================================================================
Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
Texture1D<uint> HorizonMu : register(t2);

SamplerState ClampLinear : register(s0);

RWTexture2D<float4> OutSkyView : register(u0);

// ===== Constants / helpers ==================================================
static const float PI = 3.14159265358979323846f;
static const float TWO_PI = 6.283185307179586f;
static const float INV4PI = 0.25f / PI;
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);
static const float MU_EPS = 8e-4;

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
    float muG = MuHorizon(r, RbVis);
    float muT = (r <= Rt) ? 1.0f : MuHorizon(r, Rt);
    mu0 = muG + MU_EPS; // lift off horizon
    mu1 = max(mu0 + 1e-5f, muT); // keep span > 0
}

// Map μ → v in [0,1] using this window (clamped)
float VFromMuWindowed(float mu, float mu0, float mu1)
{
    return saturate((mu - mu0) / max(mu1 - mu0, 1e-6f));
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
    float v = 1.0f - saturate((r - Rb) / max(Rt - Rb, 1e-6f)); // MS_FLIP_Y=1
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

float3 DirFromLonLat(float lon, float lat, float3 east, float3 north, float3 up)
{
    float cl = cos(lat), sl = sin(lat);
    float ce = cos(lon), se = sin(lon);
    return normalize(cl * (ce * east + se * north) + sl * up);
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

float DiscAvgMiePhase(float3 wView, float3 wSun, float sunRad, float g)
{
    // 4 taps inside the sun disc (stratified; cheap and stable)
    static const float2 Xi[4] =
    {
        float2(0.2113, 0.1589), float2(0.7113, 0.6589),
        float2(0.4613, 0.9089), float2(0.9613, 0.4089)
    };
    float3 sx = normalize(abs(wSun.y) < 0.99 ? cross(wSun, float3(0, 1, 0)) : float3(1, 0, 0));
    float3 sy = normalize(cross(sx, wSun));

    float sum = 0.0f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float r = sqrt(Xi[i].x), a = TWO_PI * Xi[i].y;
        float cr = cos(sunRad), sr = sin(sunRad);
        float3 k = normalize(cr * wSun + sr * (r * cos(a) * sx + r * sin(a) * sy));
        float mu = clamp(dot(k, wView), -0.9995f, 0.9995f);
        sum += PhaseMieHG(mu, g);
    }
    return 0.25 * sum;
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
float PhaseMie_DiscAvg(float mu, float g, float sunRad)
{
    // theta_eff ≈ sqrt(theta^2 + sunRad^2)
    float theta = acos(clamp(mu, -0.9995, 0.9995));
    float thetaEff = sqrt(theta * theta + sunRad * sunRad);
    float muEff = cos(thetaEff);
    return PhaseMieHG_CS(muEff, g);
}

// ===== Main =================================================================
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
    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + R_BIAS;
    const float RbHit = RbPhys + R_BIAS;
    
    // μ at the analytic horizon, then row index of horizon
    // Camera-centric coordinates
    float3 camWS = cameraPosition.xyz - WorldOffsetWS;
    float3 camRel = camWS - PlanetCenterWS;
    float rCam = max(RbPhys, length(camRel));
    
    // pick the window 
    float mu0, mu1;
    float rWin = min(rCam, Rt - 1.0f);
    float3 camRelWin = normalize(camRel) * rWin;
    GetMuWindow(rWin, RbVis, Rt, mu0, mu1);
    
    // v→μ with optional horizon focus
    float v = 1.0f - (tid.y + 0.5f) / float(H);
    const float kRows = 4.5f; // ~2–3 rows
    float vmin = kRows / float(H);
    v = lerp(vmin, 1.0f, v); // pushes only the lowest rows up
    float t = FocusT(v, 0.94f);
    float mu = lerp(mu0, mu1, t);
    
    // azimuth from u
    float phi = lerp(-PI, PI, saturate(u));

    // reconstruct direction at this μ and azimuth
    float3 spinUp = normalize(BasisSpinUp);
    float3 east0 = normalize(BasisTanEast);
    float3 north0 = normalize(BasisTanNorth);

    float3 up, east, north;
    BuildSkyBasisAnchored(camWS, PlanetCenterWS, east0, north0, spinUp, up, east, north);
    float sphi = sin(phi), cphi = cos(phi);
    float sinTh = sqrt(saturate(1.0f - mu * mu));
    float3 wView = normalize(mu * up + sinTh * (cphi * east + sphi * north));
    float muV = dot(wView, up);
    
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

    // Length to integrate
    float L = max(tExit - tEnter, 1e-6f);

    float3 Ls = 0, Lms = 0;

    // keep rEntry strictly inside [RbVis, Rt] for stable TLUT lookups
    float3 pEntry = camRelWin + wView * tEnter;
    float rEntry = clamp(length(pEntry), RbVis + 5e-4f, Rt - 5e-4f);
    float3 upEntry = pEntry / rEntry;
    float muEntry = dot(wView, upEntry);

    float3 g = saturate(MieAnisotropy);
    float3 f = g * g; // remove delta peak
    float3 g_p = (g - f) / max(1.0.xxx - f, 1e-6.xxx);
    
    [loop]
    for (uint i = 0; i < SKY_STEPS; ++i)
    {
        float a0 = float(i) / float(SKY_STEPS);
        float a1 = float(i + 1) / float(SKY_STEPS);
        float s0 = a0 * a0;
        float s1 = a1 * a1;
        float t0 = tEnter + L * s0;
        float t1 = tEnter + L * s1;
        float ti = 0.5f * (t0 + t1); // ABSOLUTE distance from camera
        float dt = (t1 - t0);
    
        float tLocal = max(0.0f, ti - tEnter);
        float3 Tvp = T_along_ray(rEntry, muEntry, tLocal, RbVis, RbPhys, Rt);
        float muPh = clamp(dot(wSun, -wView), -0.9995f, 0.9995f);
        
        float3 pRel = camRelWin + wView * ti;
        float rp = length(pRel);
        float h = max(0.0f, rp - RbPhys); // << height above blocking radius
            
        float dR = DensityRayleigh(h);
        float dM = DensityMie(h);
        
        // at height h:
        float3 sigR_s = RayleighScattering * dR; // Rayleigh σ_s
        float3 sigM_s = MieScattering * dM; // Mie σ_s
        float3 sigM_a = MieAbsorption * dM; // Mie σ_a
        
        float3 sig_t = sigM_s + sigM_a;

        float3 sig_t_p = (1.0.xxx - f) * sig_t;
        float3 w0 = sigM_s / max(sig_t, 1e-6.xxx);
        float3 w0_p = ((1.0.xxx - f) * w0) / max(1.0.xxx - f * w0, 1e-6.xxx);
        
        float3 sigM_s_single = w0_p * sig_t_p;
        float g_single = g_p;

        // single-scattering albedos
        float3 w0R = 1.0.xxx; // Rayleigh has no absorption
        float3 w0M = sigM_s / max(sigM_s + sigM_a, 1e-6.xxx);
        
        float3 oneMinusG = max(1e-3.xxx, 1.0.xxx - g);
        float3 sigS_ms = sigR_s * w0R + sigM_s * w0M * oneMinusG;

        float3 upS = (rp > 0.0f) ? (pRel / rp) : up;
        float muS = dot(upS, wSun);
        float Vsun = SunVisibilityAtR(rp, muS, RbVis);
           
        float3 Tsun = T_to_TOA(rp, muS, RbPhys, Rt) * Vsun;

        float PR = PhaseRayleigh(muPh);
        float3 PMrgb = float3(PhaseMie_DiscAvg(muPh, g_p.r, SunDiscRadius), PhaseMie_DiscAvg(muPh, g_p.g, SunDiscRadius), PhaseMie_DiscAvg(muPh, g_p.b, SunDiscRadius));

        // single scattering
        Ls += Tvp * (sigR_s * PR * Tsun) * dt;
        Ls += Tvp * (sigM_s_single * PMrgb * Tsun) * dt;

        // multiple scattering: use Rb in the LUT sampling too
        float4 Psi4 = SamplePsiMS4(rp, muS, RbPhys, Rt);
               
        float3 PsiMS_rgb = Psi4.rgb;
        
        float gEff = saturate(Psi4.a);
        float pMS = MSPhase(muPh, gEff);
        
        Lms += Tvp * (sigS_ms * PsiMS_rgb) * pMS * dt;
    }
    
    float3 skyRGB = (Ls + Lms) * Esun;
    
    OutSkyView[tid.xy] = float4(skyRGB, 1.0f);
    return;
}