//#type compute
//#pragma pack_matrix(row_major)

//// ------------------------------------------------------------
//// Debug/toggles
//// ------------------------------------------------------------
//// 0 = store Ψ_ms (normal)
//// 1 = L2_total (vol + ground)
//// 2 = f_ms (grey)
//// 3 = L2_vol only
//// 4 = L2_gnd only
//#ifndef MS_DEBUG_MODE
//#define MS_DEBUG_MODE        0
//#endif

//// Visual flips for DX11 blit conventions (just for preview)
//#ifndef MS_FLIP_X
//#define MS_FLIP_X            0
//#endif
//#ifndef MS_FLIP_Y
//#define MS_FLIP_Y            1
//#endif

//// Include/Exclude terms
//#ifndef MS_ENABLE_GROUND
//#define MS_ENABLE_GROUND     1   // 1 = include ground contribution in L2
//#endif
//#ifndef MS_FORCE_SUNVIS
//#define MS_FORCE_SUNVIS      0   // 1 = ignore horizon gate (sunVis=1)
//#endif

//// Sampling quality (if cb not provided)
//#ifndef MS_MIN_STEPS_DIR
//#define MS_MIN_STEPS_DIR     6   // min per-ray steps for L2 integration
//#endif

//// ------------------------------------------------------------
//// Buffers
//// ------------------------------------------------------------
//cbuffer PlanetFrame : register(b4)
//{
//    float3 PlanetCenterWS;
//    float PlanetRadius; // Rg
//    float3 BasisTanEast;
//    float MaxHeight;
//    float3 BasisTanNorth;
//    float MinHeight;
//    float3 BasisRadUp;
//    float3 BasisLonEast;
//    float3 BasisLonNorth;
//    float3 BasisSpinUp;
//};

//cbuffer Atmosphere : register(b5)
//{
//    float AtmosphereHeight; // Rt - Rg
//    float RayScaleHeight; // Hr
//    float MieScaleHeight; // Hm
//    float MieAnisotropy; // g
//    float3 RayleighScattering; // beta_R (1/m) RGB
//    float3 MieScattering; // beta_Ms (1/m) RGB
//    float3 MieAbsorption; // beta_Ma (1/m) RGB
//    float3 GroundAlbedo; // diffuse ground albedo
//    float OzoneStrength; // (used in TLUT, not here)
//    uint StepsTransmittance; // per-ray steps for L2 (e.g. 16–32)
//    uint StepsMultiScattering; // # directions (e.g. 64)
//    float APFarDynamic; // unused
//};

//Texture2D<float4> TransmittanceLUT : register(t0);
//SamplerState ClampLinear : register(s0);
//RWTexture2D<float4> OutMultiScatter : register(u0);

//// ------------------------------------------------------------
//// Constants / helpers
//// ------------------------------------------------------------
//static const float PI = 3.14159265359f;
//static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

//// Up is +Y in this engine.
//// NOTE: The TLUT only needs (r, mu), independent of world basis,
//// so we just make sure mu = cos with the local radial +Up axis.
//// Here we treat +Y as that Up.
//#define MU_FROM_DIR(wi) ((wi).y)

//// ------------------------------------------------------------
//// Densities & local optical props
//// ------------------------------------------------------------
//float DensityRayleigh(float h)
//{
//    return exp(-max(h, 0.0f) / max(RayScaleHeight, 1e-3f));
//}
//float DensityMie(float h)
//{
//    return exp(-max(h, 0.0f) / max(MieScaleHeight, 1e-3f));
//}
//void OpticalPropsAtHeight(float h, out float3 sigma_s, out float3 sigma_a, out float3 sigma_t)
//{
//    float dR = DensityRayleigh(h);
//    float dM = DensityMie(h);
//    float3 sigR_s = RayleighScattering * dR;
//    float3 sigM_s = MieScattering * dM;
//    float3 sigM_a = MieAbsorption * dM;
//    sigma_s = sigR_s + sigM_s;
//    sigma_a = sigM_a;
//    sigma_t = sigma_s + sigma_a; // ozone omitted here (already baked in TLUT)
//}

//// ------------------------------------------------------------
//// TLUT mapping (Bruneton/UE): (r, mu) -> uv
//// ------------------------------------------------------------
//float2 TransUV(float r, float mu, float Rg, float Rt)
//{
//    float rNorm = (r - Rg) / max(Rt - Rg, 1e-6f);
//    float muMin = -sqrt(saturate(1.0f - (Rg * Rg) / (r * r)));
//    float uMu = (mu - muMin) / (1.0f - muMin);
//    return saturate(float2(uMu, rNorm));
//}
//float3 T_to_TOA(float r, float mu, float Rg, float Rt)
//{
//    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rg, Rt), 0).rgb;
//}

//// ------------------------------------------------------------
//// Distances / ground test in spherical shell
//// ------------------------------------------------------------
//float DistToTop(float r, float mu, float Rt)
//{
//    float d = r * r * (mu * mu - 1.0f) + Rt * Rt;
//    return max(-r * mu + sqrt(max(d, 0.0f)), 0.0f);
//}
//float DistToBottom(float r, float mu, float Rg)
//{
//    float d = r * r * (mu * mu - 1.0f) + Rg * Rg;
//    return max(-r * mu - sqrt(max(d, 0.0f)), 0.0f);
//}
//bool HitsGround(float r, float mu, float Rg)
//{
//    return (mu < 0.0f) && (r * r * (mu * mu - 1.0f) + Rg * Rg >= 0.0f);
//}

//// ------------------------------------------------------------
//// Segment transmittance
////  - interior partial segment (always use this for t<boundary)
////  - full segment to first boundary (TOA or ground)
//// ------------------------------------------------------------
//float3 T_along_ray(float r, float mu, float t, float Rg, float Rt)
//{
//    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
//    rd = clamp(rd, Rg, Rt);
//    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
//    float3 num = T_to_TOA(r, mu, Rg, Rt);
//    float3 den = T_to_TOA(rd, muD, Rg, Rt);
//    return saturate(num / max(den, 1e-6.xxx));
//}
//float3 T_to_boundary(float r, float mu, float d, bool toGround, float Rg, float Rt)
//{
//    if (toGround)
//    {
//        float rd = sqrt(d * d + 2.0f * r * mu * d + r * r);
//        rd = clamp(rd, Rg, Rt);
//        float muD = clamp((r * mu + d) / rd, -1.0f, 1.0f);
//        // T(p->ground) = T(ground->TOA) / T(p->TOA), with flipped μ
//        float3 num = T_to_TOA(rd, -muD, Rg, Rt);
//        float3 den = T_to_TOA(r, -mu, Rg, Rt);
//        return saturate(num / max(den, 1e-6.xxx));
//    }
//    // For TOA, interior ratio at t=d is correct
//    return T_along_ray(r, mu, d, Rg, Rt);
//}

//// ------------------------------------------------------------
//// Uniform sphere sampling (Hammersley)
//// ------------------------------------------------------------
//uint ReverseBits32(uint x)
//{
//    x = (x << 16) | (x >> 16);
//    x = ((x & 0x00ff00ffu) << 8) | ((x & 0xff00ff00u) >> 8);
//    x = ((x & 0x0f0f0f0fu) << 4) | ((x & 0xf0f0f0f0u) >> 4);
//    x = ((x & 0x33333333u) << 2) | ((x & 0xccccccccu) >> 2);
//    x = ((x & 0x55555555u) << 1) | ((x & 0xaaaaaaaau) >> 1);
//    return x;
//}
//float RadicalInverse_VdC(uint i)
//{
//    return float(ReverseBits32(i)) * 2.3283064365386963e-10f;
//}
//float3 SampleSphere(uint i, uint n)
//{
//    float u = (float(i) + 0.5f) / float(n);
//    float v = RadicalInverse_VdC(i);
//    float z = 1.0f - 2.0f * v;
//    float r = sqrt(saturate(1.0f - z * z));
//    float phi = 2.0f * PI * u;
//    return float3(r * cos(phi), z, r * sin(phi)); // arrange so .y is "Up"
//    // (x,z) in horizontal plane, y vertical. So MU_FROM_DIR(wi) == wi.y.
//}

//// ------------------------------------------------------------
//// Sun visibility (small horizon softening)
//// ------------------------------------------------------------
//float SunVisibilityAtSample(float r, float muS, float Rg)
//{
//    const float SunAngularRadius = 0.00935f; // ~0.535°
//    float sinThetaH = Rg / r;
//    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
//    return smoothstep(-sinThetaH * SunAngularRadius,
//                       sinThetaH * SunAngularRadius,
//                       muS - cosThetaH);
//}

//// ------------------------------------------------------------
//// Main
////  * X = θ_s ∈ [0, π] (left→right)
////  * Y = linear altitude r ∈ [Rg, Rt] (bottom→top)
////
////  L2_vol : ∫ σ_s(t) T(t) dt       (along-path sigma!)
////  L2_gnd : Lo_ground * T_out      (if ray hits ground)
////  f_ms   : ⟨ 1 - T_out ⟩          (proxy, no extra rho)
////  Ψ_ms   : (L2_vol + L2_gnd) * 1/(1 - f_ms)
//// ------------------------------------------------------------
//[numthreads(8, 8, 1)]
//void main(uint3 dtid : SV_DispatchThreadID)
//{
//    uint W, H;
//    OutMultiScatter.GetDimensions(W, H);
//    if (dtid.x >= W || dtid.y >= H)
//        return;

//    float2 uv = (float2(dtid.xy) + 0.5f) / float2(W, H);
//#if MS_FLIP_X
//    uv.x = 1.0f - uv.x;
//#endif
//#if MS_FLIP_Y
//    uv.y = 1.0f - uv.y;
//#endif

//    const float Rg = PlanetRadius;
//    const float Rt = PlanetRadius + AtmosphereHeight;

//    // Param: θ_s and linear altitude
//    float thetaS = uv.x * PI; // θ_s ∈ [0, π]
//    float muS = cos(thetaS); // μ_s
//    float r = lerp(Rg, Rt, uv.y);
//    float h = max(0.0f, r - Rg);

//    // Local properties (still needed for gBar & optional sanity)
//    float3 sigma_s0, sigma_a0, sigma_t0;
//    OpticalPropsAtHeight(h, sigma_s0, sigma_a0, sigma_t0);

//    // Sun gate
//    float sunVis = SunVisibilityAtSample(r, muS, Rg);
//#if MS_FORCE_SUNVIS
//    sunVis = 1.0f;
//#endif
//    const float3 LoGround = GroundAlbedo / PI;

//    // Sample counts
//    const uint Ndirs = max(StepsMultiScattering, 2u);
//    const uint Nsteps = max(MS_MIN_STEPS_DIR, StepsTransmittance / 2u);

//    // Accumulators
//    float3 L2_vol = 0.0f;
//    float3 L2_gnd = 0.0f;
//    float fms = 0.0f;

//    // Directions loop (uniform over 4π)
//    [loop]
//    for (uint i = 0; i < Ndirs; ++i)
//    {
//        float3 wi = SampleSphere(i, Ndirs); // Up = +Y
//        float mu = MU_FROM_DIR(wi); // cos with Up

//        bool g = HitsGround(r, mu, Rg);
//        float d = g ? DistToBottom(r, mu, Rg) : DistToTop(r, mu, Rt);

//        // --- f_ms: proxy as (1 - T_out) without extra rho ---
//        float3 T_out_rgb = T_to_boundary(r, mu, d, g, Rg, Rt);
//        float T_out = dot(T_out_rgb, LUMA);
//        fms += (1.0f - T_out);

//        // --- Ground seed (single hit) ---
//#if MS_ENABLE_GROUND
//        if (g)
//        {
//            // diffuse ground under unit light: Lo = A/π
//            L2_gnd += LoGround * T_out_rgb * sunVis;
//        }
//#endif

//        // --- Volumetric seed with along-path sigma_s(t) ---
//        float dt = d / float(Nsteps);
//        float t = 0.5f * dt;
//        [loop]
//        for (uint s = 0; s < Nsteps; ++s, t += dt)
//        {
//            // transmittance to current sample along the ray
//            float3 Tseg = T_along_ray(r, mu, t, Rg, Rt);

//            // altitude at this point and local scattering at t
//            float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
//            float h_step = max(0.0f, rd - Rg);
//            float3 sigma_s_step =
//                RayleighScattering * DensityRayleigh(h_step) +
//                MieScattering * DensityMie(h_step);

//            L2_vol += sigma_s_step * Tseg * sunVis * dt;
//        }
//    }

//    // Averages
//    float invN = 1.0f / float(Ndirs);
//    L2_vol *= invN;
//    L2_gnd *= invN;
//    fms *= invN;

//    // Stabilize f_ms to avoid divergence in 1/(1 - f_ms)
//    fms = clamp(fms, 0.0f, 0.95f);

//    // Infinite series amplification and final Ψ_ms
//    float Fms = 1.0f / (1.0f - fms);
//    float3 PsiMS = (L2_vol + L2_gnd) * Fms;

//    // Optional alpha = effective mean cosine for MS (Mie share × g)
//    float mieShare = dot(MieScattering * DensityMie(h), LUMA)
//                   / max(dot(sigma_s0, LUMA), 1e-6f);
//    float gBar = saturate(mieShare) * saturate(MieAnisotropy);

//#if   MS_DEBUG_MODE == 1
//    OutMultiScatter[dtid.xy] = float4(max(L2_vol + L2_gnd, 0.0f), 1.0f);
//#elif MS_DEBUG_MODE == 2
//    OutMultiScatter[dtid.xy] = float4(fms.xxx, 1.0f);
//#elif MS_DEBUG_MODE == 3
//    OutMultiScatter[dtid.xy] = float4(max(L2_vol, 0.0f), 1.0f);
//#elif MS_DEBUG_MODE == 4
//    OutMultiScatter[dtid.xy] = float4(max(L2_gnd, 0.0f), 1.0f);
//#else
//    OutMultiScatter[dtid.xy] = float4(max(PsiMS, 0.0f), gBar);
//#endif
//}

#type compute
#pragma pack_matrix(row_major)

// --- toggles ---------------------------------------------------------------
#ifndef MS_DEBUG_MODE
#define MS_DEBUG_MODE        0         // 0=Ψms, 1=L2_total, 2=f_ms, 3=L2_vol, 4=L2_gnd
#endif
#ifndef MS_FLIP_Y
#define MS_FLIP_Y            1
#endif
#ifndef MS_ENABLE_GROUND
#define MS_ENABLE_GROUND     1         // include ground term in L2
#endif

// *** NEW: keep sun-visibility OUT of the LUT (recommended = 0) ***
#ifndef MS_BAKE_SUNVIS
#define MS_BAKE_SUNVIS       1
#endif

// *** NEW: make f_ms proportional to single-scattering albedo (recommended = 1) ***
#ifndef MS_FMS_WEIGHT_RHO
#define MS_FMS_WEIGHT_RHO    0
#endif

// *** NEW: upper bound for f_ms to keep F_ms numerically tame ***
#ifndef MS_FMS_MAX
#define MS_FMS_MAX           0.7f 
#endif

#ifndef MS_VIS_FLOOR
#define MS_VIS_FLOOR   0.00f   // try 0.02–0.05
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

float2 TransUV(float r, float mu, float Rg, float Rt)
{
    float rNorm = (r - Rg) / max(Rt - Rg, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (Rg * Rg) / (r * r)));
    mu = clamp(mu, muMin + 1e-5f, 1.0f - 1e-5f); // <- important
    float uMu = (mu - muMin) / (1.0f - muMin);
    return float2(uMu, saturate(rNorm));
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

float3 T_along_ray(float r, float mu, float t, float Rg, float Rt)
{
    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
    rd = clamp(rd, Rg, Rt);
    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
    float3 num = T_to_TOA(r, mu, Rg, Rt);
    float3 den = T_to_TOA(rd, muD, Rg, Rt);
    return saturate(num / max(den, 1e-6.xxx));
}
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
    return T_along_ray(r, mu, d, Rg, Rt);
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

float SunVisibilityAtSample(float r, float muS, float Rg)
{
    const float SunAngularRadius = 0.00935f;
    float sinThetaH = Rg / r;
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
#if MS_FLIP_Y
    uv.y = 1.0f - uv.y;
#endif

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;

    float thetaS = uv.x * PI;
    float muS = cos(thetaS);
    float r = lerp(Rg, Rt, uv.y);
    float h = max(0.0f, r - Rg);

    // local medium props at LUT cell (used for rho and gBar)
    float3 sigma_s0, sigma_a0, sigma_t0;
    OpticalPropsAtHeight(h, sigma_s0, sigma_a0, sigma_t0);

    // single-scattering albedo ρ in luminance
    float rho_lum = dot(sigma_s0, LUMA) / max(dot(sigma_t0, LUMA), 1e-6f);

    float sunVis = SunVisibilityAtSample(r, muS, Rg);

    // if we keep LUT “light-agnostic”, do not bake sunVis
#if MS_BAKE_SUNVIS
    float sunGate = max(sunVis, MS_VIS_FLOOR); // soften the horizon gate
#else
    float sunGate = 1.0f;
#endif

    const float3 LoGround = GroundAlbedo / PI;

    const uint Ndirs = max(StepsMultiScattering, 2u);
    const uint Nsteps = max(MS_MIN_STEPS_DIR, StepsTransmittance / 2u);

    float3 L2_vol = 0.0f;
    float3 L2_gnd = 0.0f;
    float fms = 0.0f;

    [loop]
    for (uint i = 0; i < Ndirs; ++i)
    {
        float3 wi = SampleSphere(i, Ndirs);
        float mu = MU_FROM_DIR(wi);

        bool g = HitsGround(r, mu, Rg);
        float d = g ? DistToBottom(r, mu, Rg) : DistToTop(r, mu, Rt);

        float3 T_out_rgb = T_to_boundary(r, mu, d, g, Rg, Rt);
        float T_out = dot(T_out_rgb, LUMA);

#if MS_FMS_WEIGHT_RHO
        fms += rho_lum * (1.0f - T_out);
#else
        fms += (1.0f - T_out);
#endif

#if MS_ENABLE_GROUND
        if (g)
            L2_gnd += LoGround * T_out_rgb * sunGate;
#endif

        float dt = d / float(Nsteps);
        float t = 0.5f * dt;
        [loop]
        for (uint s = 0; s < Nsteps; ++s, t += dt)
        {
            float3 Tseg = T_along_ray(r, mu, t, Rg, Rt);

            float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
            float hh = max(0.0f, rd - Rg);
            float3 sigma_s_step = RayleighScattering * DensityRayleigh(hh) + MieScattering * DensityMie(hh);

            L2_vol += sigma_s_step * Tseg * sunGate * dt;
        }
    }

    float invN = 1.0f / float(Ndirs);
    L2_vol *= invN;
    L2_gnd *= invN;
    fms *= invN;

    // keep within a safe range
    fms = clamp(fms, 0.0f, MS_FMS_MAX);

    float Fms = 1.0f / (1.0f - fms);
    float3 PsiMS = (L2_vol + L2_gnd) * Fms;

    float mieShare = dot(MieScattering * DensityMie(h), LUMA) /
                     max(dot(sigma_s0, LUMA), 1e-6f);
    float gBar = saturate(mieShare) * saturate(MieAnisotropy);

#if   MS_DEBUG_MODE == 1
    OutMultiScatter[dtid.xy] = float4(max(L2_vol + L2_gnd,0.0f),1.0f);
#elif MS_DEBUG_MODE == 2
    OutMultiScatter[dtid.xy] = float4(fms.xxx,1.0f);
#elif MS_DEBUG_MODE == 3
    OutMultiScatter[dtid.xy] = float4(max(L2_vol,0.0f),1.0f);
#elif MS_DEBUG_MODE == 4
    OutMultiScatter[dtid.xy] = float4(max(L2_gnd,0.0f),1.0f);
#else
    OutMultiScatter[dtid.xy] = float4(max(PsiMS, 0.0f), gBar);
#endif
}

