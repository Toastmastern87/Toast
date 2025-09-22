﻿#inputlayout
#type compute
#pragma pack_matrix(row_major)

// ===== Debug / toggles ======================================================
#ifndef AP_MAX_Z_SLICES
#define AP_MAX_Z_SLICES       128u   // clamp work if OutAP has deeper Z
#endif
#ifndef AP_A_EPS
#define AP_A_EPS 1e-4f   // was 0.003 — too high, creates a visible ring
#endif
#ifndef AP_USE_FAST_EXP
#define AP_USE_FAST_EXP       1      // exp2-based fast exp
#endif
#ifndef AP_USE_OZONE
#define AP_USE_OZONE          1
#endif
// Must match MultiScatteringCS/SkyViewCS
#ifndef AP_MS_BAKED_SUNVIS
#define AP_MS_BAKED_SUNVIS    1
#endif

#ifndef AP_Z_GAMMA
#define AP_Z_GAMMA 1.6f   // try 1.5–1.8; 1.6 is a good start
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
    float3 BasisRadUp;
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

// ===== LUTs =================================================================
Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
Texture2D<uint> APFarU32 : register(t2);

SamplerState ClampLinear : register(s0);

// 3D AP output: [x,y]=screen tile, [z]=non-linear distance
// rgb = accumulated in-scatter, a = camera->slice scalar transmittance
RWTexture3D<float4> OutAP : register(u0);

// ===== Constants / helpers ==================================================
static const float PI = 3.14159265358979323846f;
static const float INV4PI = 0.25f / PI;

// Ozone (Bruneton)
static const float3 O3_COEFF = float3(0.650e-6, 1.881e-6, 0.085e-6);

#if AP_USE_FAST_EXP
// exp(x) ~ exp2(x/ln(2))
float3 fexp3(float3 x)
{
    return exp2(x * 1.4426950408889634);
}
float fexp1(float x)
{
    return exp2(x * 1.4426950408889634);
}
#else
float3 fexp3(float3 x) { return exp(x); }
float  fexp1(float  x) { return exp(x); }
#endif

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

// TLUT mapping (Bruneton/UE), identical to SkyView
float2 TransUV(float r, float mu, float Rg, float Rt)
{
    float rNorm = (r - Rg) / max(Rt - Rg, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (Rg * Rg) / (r * r)));
    mu = clamp(mu, muMin + 1e-5f, 1.0f - 1e-5f);
    float uMu = (mu - muMin) / (1.0f - muMin);
    return float2(uMu, saturate(rNorm));
}
float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
}

// MultiScatter LUT sampling: x=theta_s/π, y = 1 - linear altitude (top=TOA)
float4 SamplePsiMS4(float r, float muS, float Rb, float Rt)
{
    float thetaS = acos(clamp(muS, -1.0f, 1.0f));
    float u = thetaS / PI;
    float v = saturate((r - Rb) / max(Rt - Rb, 1e-6f));
    v = 1.0f - v; // 0=TOA, 1=ground (MS_FLIP_Y=1 when baked)
    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0);
}
// MS anisotropy blend using LUT alpha as effective ḡ
float MSPhase(float mu, float gBar)
{
    float pIso = INV4PI;
    float g = saturate(gBar);
    float g2 = g * g;
    float d = 1.0f + g2 - 2.0f * g * mu;
    float pHG = INV4PI * (1.0f - g2) / max(pow(d, 1.5f), 1e-5f);
    return lerp(pIso, pHG, g);
}

// Ray-sphere
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

// Small horizon softening (matches SkyView)
float SunVisibilityAtR(float r, float muS, float Rb)
{
    const float SunAngularRadius = 0.004675f; // ~0.266° CURRENTLY HARDCODED TO EARTH VALUES
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunAngularRadius, sinThetaH * SunAngularRadius, muS - cosThetaH);
}

float3 ViewDirWS_fromUV(float2 uv)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, 1.0f, 1.0f);

    // View-space direction towards far plane
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 dirVS = normalize(vpos.xyz / max(vpos.w, 1e-6f));

    // Rotate into world space (remove translation)
    float3 dirWS = normalize(mul(dirVS, (float3x3) inverseViewMatrix));
    return dirWS;
}

// Average Rayleigh phase over a small symmetric box in μ of half-width dmu.
// ⟨μ²⟩ = μ0² + dmu²/3  → exact for (1 + μ²) under a box filter in μ.
float PhaseRayleigh_Band(float mu0, float dmu)
{
    dmu = saturate(dmu); // keep sane
    float mu2_avg = mu0 * mu0 + (dmu * dmu) * (1.0f / 3.0f);
    return (3.0f * INV4PI) * 0.25f * (1.0f + mu2_avg);
}

// Safer HG (avoid spike near μ→1 for large g)
float PhaseMieHG_Safe(float mu, float g)
{
    g = clamp(g, -0.999f, 0.999f);
    mu = clamp(mu, -0.999f, 0.999f);
    float g2 = g * g;
    float d = 1.0f + g2 - 2.0f * g * mu;
    d = max(d, 1e-2f);
    return INV4PI * (1.0f - g2) / (d * sqrt(d));
}

// Box-filter HG by sampling at μ±dmu (cheap and stable)
float PhaseMieHG_Band(float mu0, float g, float dmu)
{
    dmu = saturate(dmu);
    float muA = clamp(mu0 - dmu, -0.999f, 0.999f);
    float muB = clamp(mu0 + dmu, -0.999f, 0.999f);
    return 0.5f * (PhaseMieHG_Safe(muA, g) + PhaseMieHG_Safe(muB, g));
}

// Spatial interleaved gradient noise in [0,1)
float IGN(uint2 p)
{
    // simple, stable hash – no uniforms needed
    float n = dot(float2(p), float2(12.9898, 78.233));
    return frac(sin(n) * 43758.5453);
}

static const float2 OFFS[4] =
{
    float2(0.25, 0.25), float2(0.75, 0.25),
    float2(0.25, 0.75), float2(0.75, 0.75)
};

// ===== Main =================================================================
[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint W, H, Dfull;
    OutAP.GetDimensions(W, H, Dfull);
    if (tid.x >= W || tid.y >= H)
        return;
    uint D = min(Dfull, AP_MAX_Z_SLICES);

    float APFar = asfloat(APFarU32.Load(int3(0, 0, 0)));

    // Planet & radii
    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbHit = RbPhys + GroundBiasMeters(Rg);

    float3 ro = cameraPosition.xyz - PlanetCenterWS;

    // --- NEW: 2x2 per-tile rays to build a conservative segment ---
    float tNearTile = 1e30f;
    float tClampTile = 0.0f;
    bool any = false;

    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        float2 uv = (float2(tid.xy) + OFFS[k]) / float2(W, H);
    // build wView for uv (same as you already do)
        float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
        float4 clip = float4(ndc, 1, 1);
        float4 vpos = mul(clip, inverseProjectionMatrix);
        float3 dirVS = normalize(vpos.xyz / max(vpos.w, 1e-6));
        float3 wView = normalize(mul(dirVS, (float3x3) inverseViewMatrix));

        Hit ha = IntersectSphereGrazingSafe(ro, wView, Rt);
        if (!ha.ok)
            continue;

        float tNear = max(0.0f, ha.t0);

        Hit hg = IntersectSphereGrazingSafe(ro, wView, RbHit);
        float tGnd = (hg.ok && hg.t0 > 0.0f) ? hg.t0 : 1e30f;

        float tFar_k = min(tNear + APFar, tGnd); // ground-clamped far for this sub-ray

        any = true;
        tNearTile = min(tNearTile, tNear);
        tClampTile = max(tClampTile, tFar_k);
    }

    if (!any)
    {
    // write zeros and return (same as your early-out)
        float4 zero = float4(0, 0, 0, 0);
        [loop]
        for (uint z = 0; z < D; ++z)
            OutAP[uint3(tid.x, tid.y, z)] = zero;
        return;
    }
    
    // (optional) tiny writer-only safety to avoid under-coverage after filtering
    tClampTile += 1.0f; // meters
     
    // Accumulators to current slice end
    float3 Lcum = 0.0f; // in-scattered radiance
    float3 tauCum = 0.0f; // camera->current extinction

    float2 uvC = (float2(tid.xy) + 0.5f) / float2(W, H);
    float3 wView = ViewDirWS_fromUV(uvC);
    const float3 wSun = -normalize(direction.xyz);
    float3 Lsun = radiance.rgb * SunIntensity; // radiance [W·m⁻2·sr⁻1 in your units] 
    float3 Esun = Lsun;
    
    [loop]
    for (uint z = 0; z < D; ++z)
    {
        float u0 = (float) z / (float) D;
        float u1 = (float) (z + 1u) / (float) D;
        
        // place slices at fixed distance from entry only
        float tA = tNearTile + APFar * pow(u0, AP_Z_GAMMA);
        float tB = tNearTile + APFar * pow(u1, AP_Z_GAMMA);

        // clamp each slice’s contribution to valid air segment
        float segA = clamp(tA, tNearTile, tClampTile);
        float segB = clamp(tB, tNearTile, tClampTile);

        if (segB > segA)
        {
            float len = segB - segA;
            float tMid = 0.5f * (segA + segB);

            float3 p = ro + wView * tMid;
            float rMid = length(p);
            float hMid = max(0.0f, rMid - RbPhys);

            float dR = DensityRayleigh(hMid);
            float dM = DensityMie(hMid);
            float dO = DensityOzone(hMid);

            float3 sigmaExt = RayleighScattering * dR + (MieScattering + MieAbsorption) * dM + O3_COEFF * dO;

            float3 sigR_s = RayleighScattering * dR;
            float3 sigM_s = MieScattering * dM;

            float3 upS = (rMid > 0.0f) ? (p / rMid) : BasisRadUp;
            float muS = dot(upS, wSun);
            float Vsun = SunVisibilityAtR(rMid, muS, RbHit);
            float3 Tsun = T_to_TOA(rMid, muS, RbHit, Rt) * Vsun;

            float muPhase = clamp(dot(wSun, wView), -0.9995f, 0.9995f);
            float PR = PhaseRayleigh(muPhase);
            float PM = PhaseMieHG(muPhase, saturate(MieAnisotropy));

            float3 S1 = (sigR_s * PR + sigM_s * PM) * Tsun * Esun;

            float4 Psi4 = SamplePsiMS4(rMid, muS, RbHit, Rt) * Vsun;
            float pMS = MSPhase(muPhase, Psi4.a);
            float3 S_MS = (sigR_s + sigM_s) * MSPhase(muPhase, Psi4.a) * Psi4.rgb * Esun;

            // Midpoint slice integral
            float3 dTau = sigmaExt * len;
            float3 wInt = (1.0.xxx - fexp3(-dTau)) / max(sigmaExt, 1e-8.xxx);

            float3 Tcam = fexp3(-tauCum);
            Lcum += Tcam * (S1 + S_MS) * wInt;

            tauCum += dTau;
        }

        // Store cumulative (alpha = tauMean)
        float3 Tcum = fexp3(-tauCum);
        float Tmean = max((Tcum.r + Tcum.g + Tcum.b) * (1.0 / 3.0), 1e-6f);
        float tauMean = -log(Tmean);
        OutAP[uint3(tid.x, tid.y, z)] = float4(Lcum, tauMean);

        if (Tmean <= AP_A_EPS || tB >= tClampTile)
        {
        [loop]
            for (uint zz = z + 1; zz < Dfull; ++zz)
                OutAP[uint3(tid.x, tid.y, zz)] = float4(Lcum, tauMean);
            return;
        }
    }

    // If D < Dfull, replicate last slice
    if (D < Dfull)
    {
        float4 last = OutAP[uint3(tid.x, tid.y, D - 1)];
    [loop]
        for (uint zz = D; zz < Dfull; ++zz)
            OutAP[uint3(tid.x, tid.y, zz)] = last;
    }
}