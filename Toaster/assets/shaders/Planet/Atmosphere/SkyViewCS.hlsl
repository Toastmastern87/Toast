#type compute
#pragma pack_matrix(row_major)

// ===== Debug / toggles ======================================================
#ifndef SKY_ENABLE_GROUND
#define SKY_ENABLE_GROUND     1
#endif
#ifndef SKY_STEPS
#define SKY_STEPS             96
#endif
#ifndef SKY_USE_MS
#define SKY_USE_MS            1
#endif
#ifndef SKY_USE_MIE
#define SKY_USE_MIE           1
#endif
#ifndef SKY_USE_RAY
#define SKY_USE_RAY           1
#endif
#ifndef SKY_FEATHER_ROWS
#define SKY_FEATHER_ROWS 4.0f   // rows to blend across (try 4–6)
#endif
#ifndef SKY_MU_SOFT_EPS
#define SKY_MU_SOFT_EPS 0.0015f // ~0.086°, tiny lift above horizon
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
    float MieAnisotropy;
    float3 RayleighScattering;
    float3 MieScattering;
    float3 MieAbsorption;
    float3 GroundAlbedo;
    float OzoneStrength;
    uint StepsTransmittance;
    uint StepsMultiScattering;
    float APFarDynamic;
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

// Optional focus near the horizon (gives extra rows right where it matters).
// Set k≈0.75..0.9. k=1 → linear.
float FocusT(float t, float k)
{
    return pow(saturate(t), k);
}

//float VFromMu(float mu)
//{
//    mu = clamp(mu, -1.0f, 1.0f);
//    float lat = asin(mu);
//    float a = sqrt(2.0f * abs(lat) / PI);
//    float v = 0.5f + 0.5f * (lat >= 0 ? +a : -a);
//#if SKY_FLIP_Y
//    v = 1.0f - v;
//#endif
//    return saturate(v);
//}

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

float2 TransUV(float r, float mu, float Rb, float Rt)
{
    float rNorm = (r - Rb) / max(Rt - Rb, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (Rb * Rb) / (r * r)));
    mu = clamp(mu, muMin + 1.0e-5f, 1.0f - 1.0e-5f);
    float uMu = (mu - muMin) / (1.0f - muMin);
    return float2(uMu, saturate(rNorm));
}

float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
}

float3 T_along_ray(float r, float mu, float t, float Rb, float Rt)
{
    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
    rd = clamp(rd, Rb, Rt);
    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
    float3 num = T_to_TOA(r, mu, Rb, Rt);
    float3 den = T_to_TOA(rd, muD, Rb, Rt);
    return saturate(num / max(den, 1e-6.xxx));
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

float SunVisibilityAtR(float r, float muS, float Rb)
{
    const float SunAngularRadius = 0.004675f;
    float sH = Rb / r;
    float cH = -sqrt(saturate(1.0f - sH * sH));
    return smoothstep(-sH * SunAngularRadius, sH * SunAngularRadius, muS - cH);
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

void BuildSkyBasis(float3 camWS, float3 planetCenter, float3 spinUpWS, out float3 up, out float3 east, out float3 north)
{
    up = normalize(camWS - planetCenter); // true radial up at camera
    float3 spinT = spinUpWS - up * dot(spinUpWS, up); // remove vertical
    float len2 = max(dot(spinT, spinT), 1e-20f);
    float3 northHint = spinT * rsqrt(len2); // tangent north hint

    east = normalize(cross(northHint, up)); // exact orthonormal
    north = normalize(cross(up, east)); // re-derive north
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
    
    // keep the MS halo thin: ~2x–3x solar radius (tweak)
    const float SUN_RAD = 0.004675f;
    const float HALO_WIDTH = 6.0f * SUN_RAD;
    
    float3 wSun = -normalize(direction.xyz);
    float3 Esun = radiance.rgb * SunIntensity;
    
    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius);
    const float RbHit = RbPhys + GroundBiasMeters(RbPhys);
    
    // μ at the analytic horizon, then row index of horizon
    float3 camWS = cameraPosition.xyz;
    float3 camRel = camWS - PlanetCenterWS;
    float rCam = max(Rg, length(camRel));
    
    // pick the window 
    float mu0, mu1;
    GetMuWindow(rCam, RbVis, Rt, mu0, mu1);
    
    // v→μ with optional horizon focus
    float v = (tid.y + 0.5f) / float(H);
    v = 1.0f - v;
    float t = FocusT(v, 0.85f); // tweak 0.8–0.9 if you like
    float mu = lerp(mu0, mu1, t);
    
    // azimuth from u
    float phi = lerp(-PI, PI, saturate(u));

    // reconstruct direction at this μ and azimuth
    float3 up, east, north;
    BuildSkyBasis(cameraPosition.xyz, PlanetCenterWS, BasisSpinUp, up, east, north);
    float sphi = sin(phi), cphi = cos(phi);
    float sinTh = sqrt(saturate(1.0f - mu * mu));
    float3 wView = normalize(mu * up + sinTh * (cphi * east + sphi * north));
    float muV = dot(wView, up);
    
    Hit hatm = IntersectSphereGrazingSafe(camRel, wView, Rt);
    if (!hatm.ok)
    {
        OutSkyView[tid.xy] = float4(0, 0, 0, 1);
        return;
    }

    float tEnter = max(0.0f, hatm.t0);
    float tExit = max(0.0f, hatm.t1);

    Hit hg = IntersectSphereGrazingSafe(camRel, wView, RbHit);
    if (hg.ok && hg.t0 > 0.0f)
        tExit = min(tExit, hg.t0);

    // tiny writer pad to be conservative after filtering
    tExit += 2.0f;

    // Length to integrate
    float L = max(tExit - tEnter, 1e-6f);

    float3 Ls = 0, Lms = 0;
    
    // entry point on the ray where we hit the atmosphere
    float3 pEntry = camRel + wView * tEnter;

    // keep rEntry strictly inside [RbVis, Rt] for stable TLUT lookups
    float rEntry = clamp(length(pEntry), RbVis + 1e-3f, Rt - 1e-3f);
    float3 upEntry = pEntry / rEntry;
    float muEntry = dot(wView, upEntry);

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
        float3 Tvp = T_along_ray(rEntry, muEntry, tLocal, RbVis, Rt);
        float muPh = clamp(dot(wSun, wView), -0.9995f, 0.9995f);
        
        float3 pRel = camRel + wView * ti;
        float rp = length(pRel);
        float h = max(0.0f, rp - RbPhys); // << height above blocking radius
            
        float dR = DensityRayleigh(h);
        float dM = DensityMie(h);
        float3 sigR_s = RayleighScattering * dR;
        float3 sigM_s = MieScattering * dM;
        float3 sigS = sigR_s + sigM_s;

        float3 upS = (rp > 0.0f) ? (pRel / rp) : up;
        float muS = dot(upS, wSun);
        float Vsun = SunVisibilityAtR(rp, muS, RbVis);      
           
        float3 Tsun = T_to_TOA(rp, muS, RbVis, Rt) * Vsun;

        float PR = PhaseRayleigh(muPh);
        float PM = PhaseMieHG(muPh, saturate(MieAnisotropy));

        // single scattering
        if (SKY_USE_RAY)
            Ls += Tvp * (sigR_s * PR * Tsun) * dt;
        if (SKY_USE_MIE)
            Ls += Tvp * (sigM_s * PM * Tsun) * dt;

        // multiple scattering: use Rb in the LUT sampling too
        float4 Psi4 = SamplePsiMS4(rp, muS, RbPhys, Rt); // << Rb
        float pMS = MSPhase(muPh, Psi4.a);
               
        float3 PsiMS_rgb = Psi4.rgb;
        
        if (SKY_USE_MS)
            Lms += Tvp * ((sigS * PsiMS_rgb) * pMS) * dt;
    }
    float fadeStart = cos(radians(85.0)); // ~+0.087 : a few degrees above horizon
    float fadeEnd = cos(radians(100.0)); // ~-0.174 : ~10° below horizon
    float3 upCam = (rCam > 0) ? camRel / rCam : float3(0, 1, 0);
    float muSunAtCam = dot(upCam, -normalize(direction.xyz));
    float fNight = smoothstep(fadeEnd, fadeStart, muSunAtCam);

    float nightEVBias = lerp(-1.0f, 0.0f, fNight); // -3 EV in deep night → 0 EV near horizon
    float nightMul = exp2(nightEVBias);
    float3 skyRGB = (Ls + Lms) * Esun * nightMul;

    OutSkyView[tid.xy] = float4(skyRGB, 1.0f);
    return;  
}

