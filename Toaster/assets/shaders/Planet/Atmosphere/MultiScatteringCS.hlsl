#type compute
#pragma pack_matrix(row_major)

// --- toggles ---------------------------------------------------------------
// *** NEW: make f_ms proportional to single-scattering albedo (recommended = 1) ***
#ifndef MS_FMS_WEIGHT_RHO
#define MS_FMS_WEIGHT_RHO    0
#endif
// *** NEW: upper bound for f_ms to keep F_ms numerically tame ***
#ifndef MS_FMS_MAX
#define MS_FMS_MAX           0.55f 
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

Texture2D<float4> TransmittanceLUT : register(t0);
SamplerState ClampLinear : register(s0);
RWTexture2D<float4> OutMultiScatter : register(u0);

// --- helpers ---------------------------------------------------------------
static const float PI = 3.14159265359f;
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

#define MU_FROM_DIR(wi) ((wi).y)

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

float2 TransUV(float r, float mu, float Rb, float Rt)
{
    float rNorm = (r - Rb) / max(Rt - Rb, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (Rb * Rb) / (r * r)));
    mu = clamp(mu, muMin + 1.0e-3f, 1.0f - 1.0e-3f); // larger guard than 1e-5
    return float2((mu - muMin) / (1.0f - muMin), saturate(rNorm));
}

float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
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

float3 T_along_ray(float r, float mu, float t, float Rb, float Rt)
{
    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
    rd = clamp(rd, Rb, Rt);
    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
    float3 num = T_to_TOA(r, mu, Rb, Rt);
    float3 den = T_to_TOA(rd, muD, Rb, Rt);
    return saturate(num / max(den, 1e-6.xxx));
}
float3 T_to_boundary(float r, float mu, float d, bool toGround, float Rb, float Rt)
{
    if (toGround)
    {
        float rd = sqrt(d * d + 2.0f * r * mu * d + r * r);
        rd = clamp(rd, Rb, Rt);
        float muD = clamp((r * mu + d) / rd, -1.0f, 1.0f);
        float3 num = T_to_TOA(rd, -muD, Rb, Rt);
        float3 den = T_to_TOA(r, -mu, Rb, Rt);
        return saturate(num / max(den, 1e-6.xxx));
    }
    return T_along_ray(r, mu, d, Rb, Rt);
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

float SunVisibilityAtSample(float r, float muS, float Rb)
{
    const float SunAngularRadius = 0.004675f;
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunAngularRadius,
                       sinThetaH * SunAngularRadius,
                       muS - cosThetaH);
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

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius);

    float thetaS = uv.x * PI;
    float muS = cos(thetaS);
    muS = clamp(muS, -1.0f + 1e-3f, 1.0f - 1e-3f);
    float r = lerp(RbVis, Rt, uv.y);
    float v = saturate((r - RbVis) / max(Rt - RbVis, 1e-6f));
    float h = max(0.0f, r - RbVis); // meters  <-- add this

    // local medium props at LUT cell (used for rho and gBar)
    float3 sigma_s0, sigma_a0, sigma_t0;
    OpticalPropsAtHeight(h, sigma_s0, sigma_a0, sigma_t0);

    // single-scattering albedo ρ in luminance
    float rho_lum = dot(sigma_s0, LUMA) / max(dot(sigma_t0, LUMA), 1e-6f);

    const float3 LoGround = GroundAlbedo / PI;

    const uint Ndirs = max(StepsMultiScattering, 2u);
    
    float dMax = HitsGround(r, 0.0f, RbVis) ? DistToBottom(r, 0.0f, RbVis) : DistToTop(r, 0.0f, Rt);
    float stepMeters = 2000.0f; // ~2 km works well
    uint Nsteps = max(MS_MIN_STEPS_DIR, (uint) ceil(dMax / stepMeters));

    float3 L2_vol = 0.0f;
    float3 L2_gnd = 0.0f;
    float fms = 0.0f;

    [loop]
    for (uint i = 0; i < Ndirs; ++i)
    {
        float3 wi = SampleSphere(i, Ndirs);
        float mu = MU_FROM_DIR(wi);

        bool isGround = HitsGround(r, mu, RbVis);
        float d = isGround ? DistToBottom(r, mu, RbVis) : DistToTop(r, mu, Rt);

        float3 T_out_rgb = T_to_boundary(r, mu, d, isGround, RbVis, Rt);
        float T_out = dot(T_out_rgb, LUMA);

#if MS_FMS_WEIGHT_RHO
        fms += rho_lum * (1.0f - T_out);
#else
        fms += (1.0f - T_out);
#endif

        if (isGround)
            L2_gnd += LoGround * T_out_rgb;

        float dt = d / float(Nsteps);
        float t = 0.5f * dt;
        [loop]
        for (uint s = 0; s < Nsteps; ++s, t += dt)
        {
            float3 Tseg = T_along_ray(r, mu, t, RbVis, Rt);

            float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
            float hh = max(0.0f, rd - RbVis);
            float3 sigma_s_step = RayleighScattering * DensityRayleigh(hh) + MieScattering * DensityMie(hh);

            L2_vol += sigma_s_step * Tseg * dt;
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

    float mieShare = dot(MieScattering * DensityMie(h), LUMA) / max(dot(sigma_s0, LUMA), 1e-6f);
    float gBar = saturate(mieShare) * saturate(MieAnisotropy);

    OutMultiScatter[dtid.xy] = float4(max(PsiMS, 0.0f), gBar);
}

