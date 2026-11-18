#type compute
#pragma pack_matrix(row_major)

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterCR;
    float PlanetRadius;
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
    float Altitude;
}

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight;
    float RayScaleHeight;
    float MieScaleHeight;
    float MSGain;
    
    float3 RayleighScattering;
    float SGain;
    
    float3 MieScattering;
    
    float3 MieAbsorption;
    
    float3 GroundAlbedo;
    uint FaceIndex;
    
    float3 MieAnisotropy;
    float OzoneStrength;
    
    uint StepsTransmittance;
    uint StepsMultiScattering;
    float APFarDynamic;
    
    float3 SunsetTint;
    float BakeIBL;
    
    float3 MieLobeWeight;
    float MieAnisotropyBroad;
};

Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);

SamplerState ClampLinear : register(s0);

RWTexture2D<float4> OutGroundIrradiance : register(u0);

static const float PI = 3.14159265358979323846f;
static const float INV4PI = 0.25f / PI;
static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);
static const float3 O3_COEFF = float3(0.650e-6, 1.881e-6, 0.085e-6); // Bruneton

static const uint GI_DIRS = 64u;
static const float GI_TAU_STEP = 0.05f;
static const uint GI_NCAP = 32u;

#include "TransmittanceLUT.hlsli"
#include "GroundIrradianceLUT.hlsli"

float DensityExpTapered(float h, float H)
{
    float hTop = AtmosphereHeight - min(0.0f, MinHeight);
    H = max(H, 1e-3f);
    return max(exp(-max(h, 0.0f) / H) - exp(-hTop / H), 0.0f);
}

float DensityOzone(float hMeters)
{
    float km = hMeters * 1e-3;
    float tri = saturate(1.0f - abs((km - 25.0f) / 15.0f));
    return tri * (OzoneStrength / 15000.0f);
}

float PhaseRayleigh(float cosTheta)
{
    return (3.0f / (16.0f * PI)) * (1.0f + cosTheta * cosTheta);
}

float PhaseMieCS(float mu, float g) // Cornette–Shanks
{
    g = saturate(g);
    float g2 = g * g;
    float denom = pow(1.0 + g2 - 2.0 * g * mu, 1.5);
    float cs = (3.0 * (1.0 + mu * mu)) / (2.0 * (2.0 + g2)) * ((1.0 - g2) / max(denom, 1e-5));
    return INV4PI * cs;
}

float3 PhaseMieTwoLobe(float mu)
{
    float3 w = saturate(MieLobeWeight);
    float3 narrow = float3(PhaseMieCS(mu, MieAnisotropy.r), PhaseMieCS(mu, MieAnisotropy.g), PhaseMieCS(mu, MieAnisotropy.b));
    return w * narrow + (1.0f - w) * PhaseMieCS(mu, MieAnisotropyBroad);
}

float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
}

float SunVisibleHard(float r, float muS, float Rb)
{
    float s = saturate(Rb / r);
    return (muS > -sqrt(max(1.0f - s * s, 0.0f))) ? 1.0f : 0.0f;
}

float3 SamplePsiMS(float r, float muS, float Rb, float Rt)
{
    float u = acos(clamp(muS, -1.0f, 1.0f)) / PI;
    float v = 1.0f - saturate((r - Rb) / max(Rt - Rb, 1e-6f));
    v = clamp(v, 1.0e-3f, 1.0f - 1.0e-3f);
    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0).rgb;
}

float DistToTop(float r, float mu, float Rt)
{
    float d = r * r * (mu * mu - 1.0f) + Rt * Rt;
    return max(-r * mu + sqrt(max(d, 0.0f)), 0.0f);
}

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float3 SkyRadiance(float3 ro, float3 rd, float3 wSun, float tEnd, float Rb, float Rt)
{
    float muPhase = clamp(dot(wSun, rd), -1.0f, 1.0f);
    float PR = PhaseRayleigh(muPhase);
    float3 PM = PhaseMieTwoLobe(muPhase);

    float3 L = 0.0f.xxx;
    float3 T = 1.0f.xxx;
    float t = 0.0f;
    uint iter = 0;

    [loop]
    while (t < tEnd - 1e-3f && iter < 4u * GI_NCAP)
    {
        float hE = max(0.0f, length(ro + rd * t) - Rb);
        float3 sigE = RayleighScattering * DensityExpTapered(hE, RayScaleHeight) + (MieScattering + MieAbsorption) * DensityExpTapered(hE, MieScaleHeight);
        float dtTau = GI_TAU_STEP / max(dot(sigE, LUMA), 1e-9f);
        float dtGeo = (tEnd - t) / (float) max(1u, GI_NCAP - min(iter, GI_NCAP - 1u));
        float dt = clamp(min(dtTau, dtGeo), 1e-3f, tEnd - t);

        float3 p = ro + rd * (t + 0.5f * dt);
        float r = length(p);
        float h = max(0.0f, r - Rb);
        float dR = DensityExpTapered(h, RayScaleHeight);
        float dM = DensityExpTapered(h, MieScaleHeight);
        float3 sigR_s = RayleighScattering * dR;
        float3 sigM_s = MieScattering * dM;
        float3 sigmaExt = sigR_s + (MieScattering + MieAbsorption) * dM + O3_COEFF * DensityOzone(h);

        float muS = dot(p / max(r, 1e-3f), wSun);
        float3 Tsun = T_to_TOA(r, muS, Rb, Rt) * SunVisibleHard(r, muS, Rb);

        float3 S1 = (sigR_s * PR + sigM_s * PM) * Tsun;
        float3 SMS = (sigR_s + sigM_s) * SamplePsiMS(r, muS, Rb, Rt);

        float3 stepT = exp(-sigmaExt * dt);
        L += T * (S1 + SMS) * (1.0f.xxx - stepT) / max(sigmaExt, 1e-12f.xxx);
        T *= stepT;

        t += dt;
        iter++;
    }
    return L;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint W, H;
    OutGroundIrradiance.GetDimensions(W, H);
    if (id.x >= W || id.y >= H)
        return;

    const float Rt = PlanetRadius + AtmosphereHeight;
    const float Rb = PlanetRadius + min(0.0f, MinHeight);

    float2 uv = (float2(id.xy) + 0.5f) / float2(W, H);
    float r, muS;
    GroundIrradianceRMuFromUV(uv, Rb, Rt, r, muS);

    // Local frame: +Y is up at the surface point, the sun in the XY plane (any azimuth: symmetric).
    float3 ro = float3(0.0f, r, 0.0f);
    float3 wSun = float3(sqrt(saturate(1.0f - muS * muS)), muS, 0.0f);

    float3 sum = 0.0f.xxx;
    [loop]
    for (uint i = 0; i < GI_DIRS; ++i)
    {
        // Cosine-weighted hemisphere direction (pdf = cos/pi), Hammersley points.
        float2 xi = float2((float(i) + 0.5f) / float(GI_DIRS), RadicalInverse_VdC(i));
        float cosT = sqrt(1.0f - xi.y);
        float sinT = sqrt(xi.y);
        float phi = 2.0f * PI * xi.x;
        float3 rd = float3(sinT * cos(phi), cosT, sinT * sin(phi));

        sum += SkyRadiance(ro, rd, wSun, DistToTop(r, rd.y, Rt), Rb, Rt);
    }

    // With cosine-weighted samples, E = pi * mean(L), so the mean radiance is E/pi (the IBL's unit).
    OutGroundIrradiance[id.xy] = float4(sum / float(GI_DIRS), 1.0f);
}