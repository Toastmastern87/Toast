#type compute
#pragma pack_matrix(row_major)

// ---------------- debug switches -------------------------------------------
// Choose ONE at a time.
#define MS_DEBUG_MODE      3
// 0 = Ψms (normal)
// 1 = L2_total (volumetric + ground)
// 2 = fms (grey)
// 3 = L2_volumetric only
// 4 = L2_ground only
// 5 = <T_to_TOA(r,muS)> over muS      (diagnose TLUT row)
// 6 = <T_out_avg> over directions      (mean boundary transmittance)
// 7 = <Tseg_first_avg>                 (mean T over a short segment)
// 8 = tone-mapped Ψms (for shape)
// 9 = Ψms * MS_DBG_SCALE, clamped

#define MS_DBG_SCALE       50.0f   // used by modes 8–9
#define MS_FLIP_X          0       // visual flip only
#define MS_FLIP_Y          0
#define MS_DISABLE_GROUND  0       // ignore ground bounce in L2
#define MS_FORCE_SUNVIS    1       // sunVis = 1 (ignore horizon gate)

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
    float RayScaleHeight; // Hr
    float MieScaleHeight; // Hm
    float MieAnisotropy; // g
    float3 RayleighScattering; // beta_R (1/m)
    float3 MieScattering; // beta_Ms (1/m)
    float3 MieAbsorption; // beta_Ma (1/m)
    float3 GroundAlbedo; // diffuse albedo
    float OzoneStrength; // (TLUT only)
    uint StepsTransmittance; // per-ray steps for L2
    uint StepsMultiScattering; // directions on the sphere
    float APFarDynamic;
};

Texture2D<float4> TransmittanceLUT : register(t0);
SamplerState ClampLinear : register(s0);
RWTexture2D<float4> OutMultiScatter : register(u0);

// ---------------- constants / helpers --------------------------------------
static const float PI = 3.14159265359f;
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

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
    sigma_t = sigma_s + sigma_a; // (no ozone here)
}

float2 TransUV(float r, float mu, float Rg, float Rt)
{
    float rNorm = (r - Rg) / max(Rt - Rg, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (Rg * Rg) / (r * r)));
    float uMu = (mu - muMin) / (1.0f - muMin);
    return saturate(float2(uMu, rNorm));
}
float3 T_to_TOA(float r, float mu, float Rg, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rg, Rt), 0).rgb;
}

float DistToTop(float r, float mu, float Rt)
{
    float d = r * r * (mu * mu - 1.0f) + Rt * Rt;
    return max(-r * mu + sqrt(max(d, 0.0f)), 0.0f);
}
float DistToBottom(float r, float mu, float Rg)
{
    float d = r * r * (mu * mu - 1.0f) + Rg * Rg;
    return max(-r * mu - sqrt(max(d, 0.0f)), 0.0f);
}
bool HitsGround(float r, float mu, float Rg)
{
    return (mu < 0.0f) && (r * r * (mu * mu - 1.0f) + Rg * Rg >= 0.0f);
}

// interior (partial) segment T
float3 T_along_ray(float r, float mu, float t, float Rg, float Rt)
{
    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
    rd = clamp(rd, Rg, Rt);
    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
    float3 num = T_to_TOA(r, mu, Rg, Rt);
    float3 den = T_to_TOA(rd, muD, Rg, Rt);
    return saturate(num / max(den, 1e-6.xxx));
}

// full segment to first boundary
float3 T_to_boundary(float r, float mu, float d, bool toGround, float Rg, float Rt)
{
    if (toGround)
    {
        float rd = sqrt(d * d + 2.0f * r * mu * d + r * r);
        rd = clamp(rd, Rg, Rt);
        float muD = clamp((r * mu + d) / rd, -1.0f, 1.0f);
        float3 num = T_to_TOA(rd, -muD, Rg, Rt);
        float3 den = T_to_TOA(r, -mu, Rg, Rt);
        return saturate(num / max(den, 1e-6.xxx));
    }
    else
    {
        return T_along_ray(r, mu, d, Rg, Rt);
    }
}

// sampling
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
    return float3(r * cos(phi), r * sin(phi), z); // z = μ
}

float SunVisibilityAtSample(float r, float muS, float Rg)
{
    const float SunAngularRadius = 0.00935f;
    float sinThetaH = Rg / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunAngularRadius, sinThetaH * SunAngularRadius,
                      muS - cosThetaH);
}

// ---------------- main ------------------------------------------------------
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint W, H;
    OutMultiScatter.GetDimensions(W, H);
    if (dtid.x >= W || dtid.y >= H)
        return;

    float2 uv = (float2(dtid.xy) + 0.5f) / float2(W, H);
#if MS_FLIP_X
    uv.x = 1.0f - uv.x;
#endif
#if MS_FLIP_Y
    uv.y = 1.0f - uv.y;
#endif

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;

    // X: θs ∈ [0,π]  ;  Y: linear altitude
    float thetaS = uv.x * PI;
    float muS = cos(thetaS);
    float r = lerp(Rg, Rt, uv.y);
    float h = max(0.0f, r - Rg);

    // local medium
    float3 sigma_s, sigma_a, sigma_t;
    OpticalPropsAtHeight(h, sigma_s, sigma_a, sigma_t);

    float3 rho_rgb = saturate(sigma_s / max(sigma_t, 1e-6f));
    float rho_lum = dot(rho_rgb, LUMA);

    float sunVis = SunVisibilityAtSample(r, muS, Rg);
#if MS_FORCE_SUNVIS
    sunVis = 1.0f;
#endif
    const float3 LoGround = GroundAlbedo / PI;

    const uint Ndirs = max(StepsMultiScattering, 2u);
    const uint Nsteps = max(6u, StepsTransmittance / 2u);

    // accumulators
    float3 L2_vol = 0.0f;
    float3 L2_gnd = 0.0f;
    float fms = 0.0f;
    float Tout_mean = 0.0f;
    float3 Tseg_first_mean = 0.0f;

    [loop]
    for (uint i = 0; i < Ndirs; ++i)
    {
        float3 wi = SampleSphere(i, Ndirs);
        float mu = wi.z;

        bool g = HitsGround(r, mu, Rg);
        float d = g ? DistToBottom(r, mu, Rg) : DistToTop(r, mu, Rt);

        float3 T_out_rgb = T_to_boundary(r, mu, d, g, Rg, Rt);
        float T_out = dot(T_out_rgb, LUMA);
        fms += rho_lum * (1.0f - T_out);
        Tout_mean += T_out;

#if !MS_DISABLE_GROUND
        if (g)
            L2_gnd += LoGround * T_out_rgb * sunVis;
#endif

        float dt = d / float(Nsteps);
        float t = 0.5f * dt;

        // sample #1 stored for diagnostics
        float3 T_first = T_along_ray(r, mu, t, Rg, Rt);
        Tseg_first_mean += T_first;

        [loop]
        for (uint s = 0; s < Nsteps; ++s, t += dt)
        {
            float3 Tseg = (s == 0) ? T_first : T_along_ray(r, mu, t, Rg, Rt);
            L2_vol += sigma_s * Tseg * sunVis * dt;
        }
    }

    // averages
    float invN = 1.0f / float(Ndirs);
    L2_vol *= invN;
    L2_gnd *= invN;
    fms *= invN;
    Tout_mean *= invN;
    Tseg_first_mean *= invN;

    fms = clamp(fms, 0.0f, 0.95f);
    float Fms = 1.0f / (1.0f - fms);
    float3 PsiMS = (L2_vol + L2_gnd) * Fms;

    float mieShare = dot(MieScattering * DensityMie(h), LUMA)
                   / max(dot(sigma_s, LUMA), 1e-6f);
    float gBar = saturate(mieShare) * saturate(MieAnisotropy);

    // ---------------- debug outputs ----------------
#if   MS_DEBUG_MODE == 1
    OutMultiScatter[dtid.xy] = float4(max(L2_vol + L2_gnd, 0.0f), 1.0f);
#elif MS_DEBUG_MODE == 2
    OutMultiScatter[dtid.xy] = float4(fms.xxx, 1.0f);
#elif MS_DEBUG_MODE == 3
    OutMultiScatter[dtid.xy] = float4(max(L2_vol,0.0f), 1.0f);
#elif MS_DEBUG_MODE == 4
    OutMultiScatter[dtid.xy] = float4(max(L2_gnd,0.0f), 1.0f);
#elif MS_DEBUG_MODE == 5
    // TLUT row probe: show T_to_TOA(r, muS)
    OutMultiScatter[dtid.xy] = float4(T_to_TOA(r, muS, Rg, Rt), 1.0f);
#elif MS_DEBUG_MODE == 6
    // mean boundary transmittance over directions
    OutMultiScatter[dtid.xy] = float4(saturate(Tout_mean).xxx, 1.0f);
#elif MS_DEBUG_MODE == 7
    // mean first-step interior segment transmittance
    OutMultiScatter[dtid.xy] = float4(saturate(Tseg_first_mean), 1.0f);
#elif MS_DEBUG_MODE == 8
    {
        float3 C = PsiMS * MS_DBG_SCALE; C = C / (1.0f + C);
        OutMultiScatter[dtid.xy] = float4(saturate(C), gBar);
    }
#elif MS_DEBUG_MODE == 9
    OutMultiScatter[dtid.xy] = float4(saturate(PsiMS * MS_DBG_SCALE), gBar);
#else
    OutMultiScatter[dtid.xy] = float4(max(PsiMS, 0.0f), gBar);
#endif
}
