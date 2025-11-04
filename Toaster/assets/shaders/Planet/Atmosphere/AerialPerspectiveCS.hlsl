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

cbuffer FloatingOrigin : register(b7)
{
    float3 WorldOffsetWS;
}

// ===== LUTs =================================================================
Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
Texture2D<uint> APFarU32 : register(t2);

SamplerState ClampLinear : register(s0);
SamplerState ClampPoint : register(s1);

// 3D AP output: [x,y]=screen tile, [z]=non-linear distance
// rgb = accumulated in-scatter, a = camera->slice scalar transmittance
RWTexture3D<float4> OutAP       : register(u0);

// ===== Constants / helpers ==================================================
static const float PI = 3.14159265358979323846f;
static const float INV4PI = 0.25f / PI;
static const float MU_EPS = 2e-3;

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
float2 TransUV(float r, float mu, float RbPhys, float Rt)
{
    float rNorm = (r - RbPhys) / max(Rt - RbPhys, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (RbPhys * RbPhys) / (r * r)));
    mu = clamp(mu, muMin + MU_EPS, 1.0f - MU_EPS);
    return float2((mu - muMin) / (1.0f - muMin), saturate(rNorm));
}

float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
}

// MultiScatter LUT sampling: x=theta_s/π, y = 1 - linear altitude (top=TOA)
float4 SamplePsiMS4(float r, float muS, float Rb, float Rt)
{
    float mu = clamp(muS, -1.0f + 1.0e-3f, 1.0f - 1.0e-3f);
    float thetaS = acos(mu);
    float u = thetaS / PI;

    float v = saturate((r - Rb) / max(Rt - Rb, 1.0e-6f));
    v = 1.0f - v;
    v = clamp(v, 1.0e-3f, 1.0f - 1.0e-3f); // <- avoid top/bottom rows
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
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunDiscRadius, sinThetaH * SunDiscRadius, muS - cosThetaH);
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
    float theta = acos(clamp(mu, -0.9995, 0.9995));
    float thetaEff = sqrt(theta * theta + SunDiscRadius * SunDiscRadius); // θ_eff ≈ √(θ² + θ_sun²)
    float muEff = cos(thetaEff);
    return PhaseMieHG_CS(muEff, g);
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
    const float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius);
    const float RbHit = RbPhys + GroundBiasMeters(RbPhys);
    
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
    
    float3 g = saturate(MieAnisotropy);
    float3 f = g * g; // remove delta peak
    float3 g_p = (g - f) / max(1.0.xxx - f, 1e-6.xxx);
    
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

        float3 sigR_s = RayleighScattering * dR;
        float3 sigM_s = MieScattering * dM;
        float3 sigM_a = MieAbsorption * dM;
        
        float3 sigmaExt = RayleighScattering * dR + (MieScattering + MieAbsorption) * dM + O3_COEFF * dO;
        
        float3 sigM_s_single = sigM_s * (1.0.xxx - f);
        
        float3 upS = (rMid > 0.0f) ? (p / rMid) : BasisSpinUp;
        float muS = dot(upS, wSun);
        float Vsun = SunVisibilityAtR(rMid, muS, RbVis);
        float3 Tsun = T_to_TOA(rMid, muS, RbPhys, Rt) * Vsun;

        float muPhase = clamp(dot(wSun, wView), -0.9995f, 0.9995f);
        float PR = SGain * PhaseRayleigh(muPhase);

        // Disc-averaged Cornette–Shanks per-channel
        float3 PMrgb = SGain * float3(PhaseMie_DiscAvg(muPhase, g_p.r), PhaseMie_DiscAvg(muPhase, g_p.g), PhaseMie_DiscAvg(muPhase, g_p.b));

        // --- SINGLE scattering (Rayleigh + Mie) ---
        float3 S1 = (sigR_s * PR + sigM_s_single * PMrgb) * Tsun * Esun;

        // --- MULTI scattering stays reduced by (1 - g) per-channel ---
        float3 w0M = sigM_s / max(sigM_s + MieAbsorption * dM, 1e-6.xxx);
        
        float3 sigS_ms = sigR_s + sigM_s * w0M; // σ'_s for MS

        float4 Psi4 = SamplePsiMS4(rMid, muS, RbPhys, Rt);
        float3 PsiMS_rgb = MSGain * Psi4.rgb;

        float3 S_MS = sigS_ms * PsiMS_rgb * Esun;

        // Midpoint integral over this slice
        float3 dTau = sigmaExt * len;
        float3 wInt = (1.0.xxx - fexp3(-dTau)) / max(sigmaExt, 1e-8.xxx);

        float3 Tcam = fexp3(-tauCum);
        Lcum += Tcam * (S1 + S_MS) * wInt;
        tauCum += dTau;

        // write cumulative for this z
        float3 Tcum = fexp3(-tauCum);
        float Tmean = max((Tcum.r + Tcum.g + Tcum.b) * (1.0f / 3.0f), 1e-6f);
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