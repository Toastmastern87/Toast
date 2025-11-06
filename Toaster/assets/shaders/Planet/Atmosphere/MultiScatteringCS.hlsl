#type compute
#pragma pack_matrix(row_major)

// --- toggles ---------------------------------------------------------------
// *** NEW: upper bound for f_ms to keep F_ms numerically tame ***
#ifndef MS_FMS_MAX
#define MS_FMS_MAX           0.8f 
#endif
#ifndef MS_MIN_STEPS_DIR
#define MS_MIN_STEPS_DIR     6
#endif

// --- buffers ---------------------------------------------------------------
cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterWS;
    float PlanetRadius; // Rg
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
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
    float Irradiance;
};

Texture2D<float4> TransmittanceLUT : register(t0);

SamplerState ClampLinear : register(s0);

RWTexture2D<float4> OutMultiScatter : register(u0);

// --- helpers ---------------------------------------------------------------
static const float PI = 3.14159265359f;
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);
static const float MU_EPS = 8e-4;

#define MU_FROM_DIR(wi) ((wi).y)

float GroundBiasMeters(float Rg)
{
    return max(1.0f, 2e-6f * Rg);
}

float DensityRayleigh(float h)
{
    return exp(-max(h, 0.0f) / max(RayScaleHeight, 1e-3f));
}
float DensityMie(float h)
{
    return exp(-max(h, 0.0f) / max(MieScaleHeight, 1e-3f));
}

void OpticalPropsAtHeight(float h, out float3 sigma_s, out float3 sigma_a, out float3 sigma_t)
{
    float dR = DensityRayleigh(h);
    float dM = DensityMie(h);
    float3 sigR_s = RayleighScattering * dR;
    float3 sigM_s = MieScattering * dM;
    float3 sigM_a = MieAbsorption * dM;
    sigma_s = sigR_s + sigM_s;
    sigma_a = sigM_a;
    sigma_t = sigma_s + sigma_a;
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

float DistToTop(float r, float mu, float Rt)
{
    float d = r * r * (mu * mu - 1.0f) + Rt * Rt;
    return max(-r * mu + sqrt(max(d, 0.0f)), 0.0f);
}
float DistToBottom(float r, float mu, float Rb)
{
    float d = r * r * (mu * mu - 1.0f) + Rb * Rb;
    return max(-r * mu - sqrt(max(d, 0.0f)), 0.0f);
}
bool HitsGround(float r, float mu, float Rb)
{
    return (mu < 0.0f) && (r * r * (mu * mu - 1.0f) + Rb * Rb >= 0.0f);
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

float3 T_to_boundary(float r, float mu, float d, bool toGround, float RbVis, float RbPhys, float Rt)
{
    if (toGround)
    {
        float rd = sqrt(d * d + 2.0f * r * mu * d + r * r);
        rd = clamp(rd, RbVis + 5e-4f, Rt - 5e-4f);
        float muD = clamp((r * mu + d) / rd, -1.0f + MU_EPS, 1.0f - MU_EPS);
        float3 num = T_to_TOA(rd, -muD, RbPhys, Rt);
        float3 den = T_to_TOA(r, -mu, RbPhys, Rt);
        return saturate(num / max(den, float3(1e-6f, 1e-6f, 1e-6f)));
    }
    return T_along_ray(r, mu, d, RbVis, RbPhys, Rt);
}

uint ReverseBits32(uint x)
{
    x = (x << 16) | (x >> 16);
    x = ((x & 0x00ff00ffu) << 8) | ((x & 0xff00ff00u) >> 8);
    x = ((x & 0x0f0f0f0fu) << 4) | ((x & 0xf0f0f0f0u) >> 4);
    x = ((x & 0x33333333u) << 2) | ((x & 0xccccccccu) >> 2);
    x = ((x & 0x55555555u) << 1) | ((x & 0xaaaaaaaau) >> 1);
    return x;
}
float RadicalInverse_VdC(uint i)
{
    return float(ReverseBits32(i)) * 2.3283064365386963e-10f;
}
float3 SampleSphere(uint i, uint n)
{
    float u = (float(i) + 0.5f) / float(n);
    float v = RadicalInverse_VdC(i);
    float z = 1.0f - 2.0f * v;
    float r = sqrt(saturate(1.0f - z * z));
    float phi = 2.0f * PI * u;
    return float3(r * cos(phi), z, r * sin(phi)); // y is Up
}

// --- main ------------------------------------------------------------------
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint W, H;
    OutMultiScatter.GetDimensions(W, H);
    if (dtid.x >= W || dtid.y >= H)
        return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(W, H);
    uv.y = 1.0f - uv.y;

    const float R_BIAS = max(1.0f, 2e-6f * PlanetRadius);
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + R_BIAS;
    const float RbHit = RbPhys + R_BIAS;

    float thetaS = uv.x * PI;
    float muS = cos(thetaS);
    muS = clamp(muS, -1.0f + MU_EPS, 1.0f - MU_EPS);
    float r = lerp(RbPhys, Rt, uv.y);
    float h = max(0.0f, r - RbPhys); // meters  <-- add this

    // local medium props at LUT cell (used for rho and gBar)
    float3 sigma_s0, sigma_a0, sigma_t_phys;
    OpticalPropsAtHeight(h, sigma_s0, sigma_a0, sigma_t_phys);
    
    // Build reduced versions for MS only
    float dR = DensityRayleigh(h);
    float dM = DensityMie(h);
    float3 sigR_s = RayleighScattering * dR;
    float3 sigM_s = MieScattering * dM;
    float3 sigM_a = MieAbsorption * dM;

    // TRUE single-scattering albedo (no (1-g) here)
    float3 sigma_s_true = sigR_s + sigM_s; // full scattering
    float3 sigma_t_true = sigma_s_true + sigM_a; // + absorption
    float rho_lum = dot(sigma_s_true, LUMA) / max(dot(sigma_t_true, LUMA), 1e-6f);

    const uint Ndirs = max(StepsMultiScattering, 2u);

    float3 L2_vol = 0.0f;
    float3 L2_gnd = 0.0f;
    float fms = 0.0f;

    [loop]
    for (uint i = 0; i < Ndirs; ++i)
    {
        float3 wi = SampleSphere(i, Ndirs);
        float mu = MU_FROM_DIR(wi);

        bool isGround = HitsGround(r, mu, RbHit);
        float d = isGround ? DistToBottom(r, mu, RbHit) : DistToTop(r, mu, Rt);

        float3 T_out_rgb = T_to_boundary(r, mu, d, isGround, RbVis, RbPhys, Rt);
        float T_out = dot(T_out_rgb, LUMA);

        fms += rho_lum * (1.0f - T_out);

        if (isGround)
            L2_gnd += (GroundAlbedo / PI) * T_out_rgb;
        
        uint stepsMin = MS_MIN_STEPS_DIR; // keep your floor (e.g. 6)
        uint stepsGeo = (uint) ceil(d / 2000.0f); // your geometric heuristic
        uint stepsMax = max(stepsMin, stepsGeo);

        float t = 0.0f;
        for (uint s = 0; s < stepsMax && t < d - 1e-6f; ++s)
        {
            // center of segment estimated after sizing dt
            // compute local extinction to size step by Δτ
            float rd_c = sqrt(t * t + 2.0f * r * mu * t + r * r);
            float hh_c = max(0.0f, rd_c - RbPhys);

            float dR_c = DensityRayleigh(hh_c);
            float dM_c = DensityMie(hh_c);
            float3 sigR_s_c = RayleighScattering * dR_c;
            float3 sigM_s_c = MieScattering * dM_c;
            float3 sigM_a_c = MieAbsorption * dM_c;
            float3 sigma_t_c = sigR_s_c + sigM_s_c + sigM_a_c;

            // luma-weighted magnitude to get a scalar σ
            float sigmaY = max(dot(sigma_t_c, LUMA), 1e-6);

            // target optical-depth per step
            const float tauStep = 0.03; // try 0.02..0.04
            float dt_tau = tauStep / sigmaY;

            // also respect the geometric partition (don’t jump too far)
            float dt_geo = (d - t) / float(stepsMax - s);
            float dt = max(1e-4, min(dt_geo, dt_tau));

            // now evaluate at the segment center
            float ti = t + 0.5f * dt;

            float3 Tseg = T_along_ray(r, mu, ti, RbVis, RbPhys, Rt);

            float rd = sqrt(ti * ti + 2.0f * r * mu * ti + r * r);
            float hh = max(0.0f, rd - RbPhys);

            float dR_step = DensityRayleigh(hh);
            float dM_step = DensityMie(hh);
            float3 sigR_s_step = RayleighScattering * dR_step;
            float3 sigM_s_step = MieScattering * dM_step;
            float3 sigM_a_step = MieAbsorption * dM_step;

            float3 w0M_step = sigM_s_step / max(sigM_s_step + sigM_a_step, 1e-6.xxx);
            float3 sigma_s_step = sigR_s_step + sigM_s_step * w0M_step;

            L2_vol += sigma_s_step * Tseg * dt;

            t += dt;
        }
    }

    float invN = 1.0f / float(Ndirs);
    L2_vol *= invN;
    L2_gnd *= invN;
    fms *= invN;

    // keep within a safe range
// Soft-knee instead of hard cap (optional)
    float soft = 0.5f; // knee position
    fms = soft + (1 - soft) * (1 - exp(-(fms - soft) / max(1e-3f, (MS_FMS_MAX - soft))));
    fms = clamp(fms, 0.0f, MS_FMS_MAX);
  
    float Fms = 1.0f / (1.0f - fms);
    float3 PsiMS = (L2_vol + L2_gnd) * Fms;

    // Anisotropy reduction with increasing scattering order (simple, robust):
    // g_eff ≈ g * (1 - k * fms), k∈[0.6..0.8] works well
    float mieShare = dot(MieScattering * DensityMie(h), LUMA) / max(dot(sigma_s0, LUMA), 1e-6f);
    float3 g_eff = saturate(MieAnisotropy) * (1.0f - 0.7f * fms);
    float gBar = saturate(mieShare * dot(g_eff, LUMA));

    OutMultiScatter[dtid.xy] = float4(max(PsiMS, 0.0f), gBar);
}

