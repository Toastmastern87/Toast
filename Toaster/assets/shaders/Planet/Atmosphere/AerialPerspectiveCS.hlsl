﻿#inputlayout
#type compute
#pragma pack_matrix(row_major)

// ===== Debug / toggles ======================================================
#ifndef AP_MAX_Z_SLICES
#define AP_MAX_Z_SLICES       128u   // clamp work if OutAP has deeper Z
#endif
#ifndef AP_A_EPS
#define AP_A_EPS              0.003f // early stop when trans becomes tiny
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
float3 T_to_TOA(float r, float mu, float Rg, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rg, Rt), 0).rgb;
}

// MultiScatter LUT sampling: x=theta_s/π, y = 1 - linear altitude (top=TOA)
float4 SamplePsiMS4(float r, float muS, float Rg, float Rt)
{
    float thetaS = acos(clamp(muS, -1.0f, 1.0f));
    float u = thetaS / PI;
    float v = saturate((r - Rg) / max(Rt - Rg, 1e-6f));
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
Hit IntersectSphere(float3 ro, float3 rd, float R)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - R * R;
    float h = b * b - c;
    Hit h0;
    h0.ok = (h >= 0.0f);
    if (!h0.ok)
    {
        h0.t0 = h0.t1 = 0;
        return h0;
    }
    float s = sqrt(h);
    h0.t0 = -b - s;
    h0.t1 = -b + s;
    return h0;
}

// Small horizon softening (matches SkyView)
float SunVisibilityAtR(float r, float muS, float Rg)
{
    const float SunAngularRadius = 0.00935f; // ~0.535°
    float sinThetaH = Rg / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunAngularRadius, sinThetaH * SunAngularRadius, muS - cosThetaH);
}

// ===== Main =================================================================
[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint W, H, Dfull;
    OutAP.GetDimensions(W, H, Dfull);
    if (tid.x >= W || tid.y >= H)
        return;

    // Clamp Z work if requested
    uint D = min(Dfull, AP_MAX_Z_SLICES);

    // ---- Build view ray (low-res screen aligned) ---------------------------
    float2 uv = (float2(tid.xy) + 0.5f) / float2(W, H);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f); // D3D NDC

    float4 clip = float4(ndc, 1.0f, 1.0f);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 dirVS = normalize(vpos.xyz / max(vpos.w, 1e-6f));
    float3 wView = normalize(mul(dirVS, (float3x3) inverseViewMatrix));

    // Planet-centered camera and radii
    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    float3 SunE = radiance.rgb * SunIntensity;
    const float3 wSun = -normalize(direction.xyz);
    SunE *= 1000.0f;

    float3 roWS = cameraPosition.xyz;
    float3 ro = roWS - PlanetCenterWS;

    // Intersections with atmosphere shell and ground
    Hit hitAtm = IntersectSphere(ro, wView, Rt);
    if (!hitAtm.ok || APFarDynamic <= 1e-3f)
    {
        float4 zero = float4(0, 0, 0, 1); // no in-scatter, fully transmissive
        [loop]
        for (uint z = 0; z < Dfull; ++z)
            OutAP[uint3(tid.x, tid.y, z)] = zero;
        return;
    }

    float tEnter = max(0.0f, hitAtm.t0);
    float tExit = max(0.0f, hitAtm.t1);

    Hit hitG = IntersectSphere(ro, wView, Rg);
    if (hitG.ok && hitG.t0 > 0.0f)
        tExit = min(tExit, hitG.t0);

    if (tExit <= tEnter)
    {
        float4 zero = float4(0, 0, 0, 1);
        [loop]
        for (uint z = 0; z < Dfull; ++z)
            OutAP[uint3(tid.x, tid.y, z)] = zero;
        return;
    }

    // Accumulators to current slice end
    float3 Lcum = 0.0f; // in-scattered radiance
    float3 tauCum = 0.0f; // camera->current extinction

    // Quadratic distance distribution along z: d(z) = APFarDynamic * (z/D)^2
    float invD = 1.0f / max(1.0f, (float) D);
    float invD2 = invD * invD;
    float dStart = 0.0f;
    float dDelta = APFarDynamic * (1.0f * invD2); // z=0 -> (2*0+1)/D^2

    [loop]
    for (uint z = 0; z < D; ++z)
    {
        float d0 = dStart;
        float d1 = dStart + dDelta;
        dStart += dDelta;
        dDelta += APFarDynamic * (2.0f * invD2); // next slice increment

        // Clip to shell segment
        float segA = max(d0, tEnter);
        float segB = min(d1, tExit);

        if (segB > segA)
        {
            float len = segB - segA;
            float tMid = 0.5f * (segA + segB);
            float3 p = ro + wView * tMid;

            float rMid = length(p);
            float hMid = max(0.0f, rMid - Rg);

            // Local densities/coefs
            float dR = DensityRayleigh(hMid);
            float dM = DensityMie(hMid);
            float dO = DensityOzone(hMid);

            float3 sigmaExt = RayleighScattering * dR + (MieScattering + MieAbsorption) * dM + O3_COEFF * dO;

            float3 sigR_s = RayleighScattering * dR;
            float3 sigM_s = MieScattering * dM;

            // Sun at sample
            float3 upS = (rMid > 0.0f) ? (p / rMid) : BasisRadUp;
            float muS = dot(upS, wSun);
            float Vsun = SunVisibilityAtR(rMid, muS, Rg);
            float3 Tsun = T_to_TOA(rMid, muS, Rg, Rt) * Vsun;

            // Phase with incoming wSun and outgoing -wView
            float muPhase = clamp(dot(wSun, -wView), -0.9995f, 0.9995f);
            float PR = PhaseRayleigh(muPhase);
            float PM = PhaseMieHG(muPhase, saturate(MieAnisotropy));

            // Single-scatter source (per color): σ_s * phase * Tsun
            float3 S1 = sigR_s * PR * Tsun + sigM_s * PM * Tsun;

            // Multi-scatter source from LUT
            float4 Psi4 = SamplePsiMS4(rMid, muS, Rg, Rt);
            float pMS = MSPhase(muPhase, Psi4.a);
            float3 msIrr = Psi4.rgb;
#if !AP_MS_BAKED_SUNVIS
            msIrr *= Vsun;
#endif
            float3 S_MS = (sigR_s + sigM_s) * pMS * msIrr;

            // Within-slice attenuation factor: (1 - e^{-Δτ}) / Δτ
            float3 dTau = sigmaExt * len;
            float3 wI = (1.0.xxx - fexp3(-dTau)) / max(dTau, 1e-6.xxx);

            // Camera->sample transmittance from cumulative tau
            float3 Tcam = fexp3(-tauCum);

            // Add slice contribution (apply sun radiance once here)
            Lcum += Tcam * (S1 + S_MS) * wI * SunE;

            // March cumulative tau
            tauCum += dTau;
        }

        // Write cumulative result at this slice end
        float3 Tcum = fexp3(-tauCum);
        // Paper: store scalar transmittance as the mean of RGB channels
        float A = (Tcum.r + Tcum.g + Tcum.b) * (1.0f / 3.0f);
        OutAP[uint3(tid.x, tid.y, z)] = float4(Lcum, saturate(A));

        // Early-out: opaque or past exit
        if (A <= AP_A_EPS || dStart >= tExit)
        {
            [loop]
            for (uint zz = z + 1; zz < Dfull; ++zz)
                OutAP[uint3(tid.x, tid.y, zz)] = float4(Lcum, saturate(A));
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