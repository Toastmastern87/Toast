﻿#inputlayout
#type compute
#pragma pack_matrix(row_major)

// ===== toggles ======================================================
#ifndef AP_MAX_Z_SLICES
#define AP_MAX_Z_SLICES       128u   // clamp work if OutAP has deeper Z
#endif
#ifndef AP_USE_FAST_EXP
#define AP_USE_FAST_EXP       1      // exp2-based fast exp
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

cbuffer FloatingOrigin : register(b7)
{
    float3 WorldOffsetWS;
}

// ===== LUTs =================================================================
Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
Texture2D<uint> APFarU32 : register(t2);

SamplerState ClampLinear : register(s0);

// 3D AP output: [x,y]=screen tile, [z]=non-linear distance
// rgb = accumulated in-scatter, a = camera->slice scalar transmittance
RWTexture3D<float4> OutAP       : register(u0);

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

// Small horizon softening (matches SkyView)
float SunVisibilityAtR(float r, float muS, float Rb)
{
    const float SunAngularRadius = 0.004675f; // ~0.266° CURRENTLY HARDCODED TO EARTH VALUES
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunAngularRadius, sinThetaH * SunAngularRadius, muS - cosThetaH);
}

float3 ViewDirWSFromUV(float2 uv)
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
    float APNear = 0.0f;
    
    // Planet & radii
    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbHit = RbPhys + GroundBiasMeters(Rg);
    
    float3 ro = (cameraPosition.xyz - WorldOffsetWS) - PlanetCenterWS;
    float2 uvC = (float2(tid.xy) + 0.5f) / float2(W, H);
    float3 wView = ViewDirWSFromUV(uvC);
    
    Hit ha = IntersectSphereGrazingSafe(ro, wView, Rt);
    if (!ha.ok)
    { // nothing to accumulate for this texel
        float4 zero = float4(0, 0, 0, 0);
        [loop]
        for (uint z = 0; z < D; ++z)
            OutAP[uint3(tid.x, tid.y, z)] = zero;
        return;
    }
    
    // Entry/exit in absolute meters
    float tEnter = max(0.0f, ha.t0); // if camera inside, this becomes 0
    float tExitA = max(0.0f, ha.t1);
    
    Hit hg = IntersectSphereGrazingSafe(ro, wView, RbHit);
    if (hg.ok && hg.t0 > 0.0f)
        tExitA = min(tExitA, hg.t0);
    
    float Lray = max(0.0f, tExitA - tEnter);
    
    float Lcap = min(APFar, Lray);
    
    // Early out: if this ray has no air in front, write zeros
    if (Lcap <= 1e-6f)
    {
        float4 zero = float4(0, 0, 0, 0);
        [loop]
        for (uint z = 0; z < D; ++z)
            OutAP[uint3(tid.x, tid.y, z)] = zero;
        return;
    }
     
    // Accumulators to current slice end
    float3 Lcum = 0.0f; // in-scattered radiance
    float3 tauCum = 0.0f; // camera->current extinction

    const float3 wSun = -normalize(direction.xyz);
    float3 Esun = radiance.rgb * SunIntensity;
    
    [loop]
    for (uint z = 0; z < D; ++z)
    {
        float u0 = (float) z / (float) D;
        float u1 = (float) (z + 1) / (float) D;
        
        // place slices at fixed distance from entry only
        float sA = APFar * pow(u0, AP_Z_GAMMA);
        float sB = APFar * pow(u1, AP_Z_GAMMA);

        // Clamp this slice to the actual ray length Lcap
        float segA = clamp(sA, 0.0f, Lcap);
        float segB = clamp(sB, 0.0f, Lcap);
        if (segB <= segA)
        {
            // store current cumulative so Z filtering has defined values
            float3 Tcum = exp(-tauCum);
            float Tmean = max((Tcum.r + Tcum.g + Tcum.b) * (1.0 / 3.0), 1e-6f);
            float tauMean = -log(Tmean);
            OutAP[uint3(tid.x, tid.y, z)] = float4(Lcum, tauMean);
            continue;
        }

        float len = segB - segA;
        float sMid = 0.5f * (segA + segB);

        // Convert S (from entry) to absolute t
        float tMid = tEnter + sMid;

        // Sample medium at midpoint
        float3 p = ro + wView * tMid;
        float rMid = length(p);
        float hMid = max(0.0f, rMid - RbPhys);

        float dR = DensityRayleigh(hMid);
        float dM = DensityMie(hMid);
        float dO = DensityOzone(hMid);

        float3 sigmaExt = RayleighScattering * dR
                        + (MieScattering + MieAbsorption) * dM
                        + O3_COEFF * dO;

        float3 sigR_s = RayleighScattering * dR;
        float3 sigM_s = MieScattering * dM;

        float3 upS = (rMid > 0.0f) ? (p / rMid) : BasisRadUp;
        float muS = dot(upS, wSun);
        float Vsun = SunVisibilityAtR(rMid, muS, Rg);
        float3 Tsun = T_to_TOA(rMid, muS, Rg, Rt) * Vsun;

        float muPhase = clamp(dot(wSun, wView), -0.9995f, 0.9995f);
        float PR = PhaseRayleigh(muPhase);
        float PM = PhaseMieHG(muPhase, saturate(MieAnisotropy));

        float3 S1 = (sigR_s * PR + sigM_s * PM) * Tsun * Esun;

        float4 Psi4 = SamplePsiMS4(rMid, muS, Rg, Rt) * Vsun;
        float3 S_MS = (sigR_s + sigM_s) * MSPhase(muPhase, Psi4.a) * Psi4.rgb * Esun;

        // Midpoint integral over this slice
        float3 dTau = sigmaExt * len;
        float3 wInt = (1.0.xxx - fexp3(-dTau)) / max(sigmaExt, 1e-8.xxx);

        float3 Tcam = fexp3(-tauCum);
        Lcum += Tcam * (S1 + S_MS) * wInt;
        tauCum += dTau;

        // write cumulative for this z
        float3 Tcum = fexp3(-tauCum);
        float Tmean = max((Tcum.r + Tcum.g + Tcum.b) * (1.0 / 3.0), 1e-6f);
        float tauMean = -log(Tmean);
        OutAP[uint3(tid.x, tid.y, z)] = float4(Lcum, tauMean);
    }

    // replicate last to the tail if D < Dfull
    if (D < Dfull)
    {
        float4 last = OutAP[uint3(tid.x, tid.y, D - 1)];
        [loop]
        for (uint zz = D; zz < Dfull; ++zz)
            OutAP[uint3(tid.x, tid.y, zz)] = last;
    }
}