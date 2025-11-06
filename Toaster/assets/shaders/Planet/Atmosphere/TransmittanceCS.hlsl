#type compute
#pragma pack_matrix(row_major)

// ---------- cbuffers ----------
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

RWTexture2D<float4> OutTransmittance : register(u0);

// ---------- constants ----------
static const float3 O3_COEFF = float3(0.650e-6, 1.881e-6, 0.085e-6); // Bruneton
static const float PI = 3.14159265358979323846f;
static const float MU_EPS = 8e-4;

// ---------- densities ----------
float DensityRayleigh(float h)
{
    return exp(-max(h, 0.0f) / max(RayScaleHeight, 1e-3f));
}
float DensityMie(float h)
{
    return exp(-max(h, 0.0f) / max(MieScaleHeight, 1e-3f));
}

// Ozone: triangle 10–40 km peaking at 25 km. Normalized so that ∫density dh = OzoneStrength.
float DensityOzone(float hMeters)
{
    float km = hMeters * 1e-3;
    float tri = saturate(1.0f - abs((km - 25.0f) / 15.0f)); // base 30 km, peak 1
    // triangle area = 0.5 * base * height = 0.5 * 30000 * 1 = 15000 m
    // multiply by (OzoneStrength / 15000) so the column integral equals OzoneStrength
    return tri * (OzoneStrength / 15000.0f);
}

// ---------- TLUT domain mapping ----------
float RadiusFromV(float v, float RbPhys, float Rt)
{
    return lerp(RbPhys, Rt, saturate(v));
}

float MuFromU(float u, float r, float RbPhys)
{
    float muMin = -sqrt(saturate(1.0f - (RbPhys * RbPhys) / (r * r)));
    float mu = muMin + saturate(u) * (1.0f - muMin);
    return clamp(mu, muMin + MU_EPS, 1.0f - MU_EPS);
}

// ---------- main ----------
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint W, H;
    OutTransmittance.GetDimensions(W, H);
    if (id.x >= W || id.y >= H)
        return;

    const float R_BIAS = max(1.0f, 2e-6f * PlanetRadius);
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + R_BIAS;
    const float RbHit = RbPhys + R_BIAS;

    float u = (id.x + 0.5f) / float(W); // maps to μ in [μmin(r),1]
    float v = (id.y + 0.5f) / float(H); // maps to r in [Rg,Rt]

    float r = RadiusFromV(v, RbPhys, Rt);
    float mu = MuFromU(u, r, RbPhys);

    // Ray (planet-centered), Up = +Z
    float3 x = float3(0.0f, 0.0f, r);
    float s = sqrt(saturate(1.0f - mu * mu));
    float3 w = float3(s, 0.0f, mu);

    // Exit to TOA
    float b = dot(x, w);
    float c = dot(x, x) - Rt * Rt;
    float h = b * b - c;
    float tEnd = (h > 0.0f) ? (-b + sqrt(h)) : 0.0f;

    uint N = max(1u, StepsTransmittance);
    float3 tau = 0.0f;

    [loop]
    for (uint i = 0; i < N; ++i)
    {
        float a0 = float(i) / float(N);
        float a1 = float(i + 1) / float(N);
        float t0 = a0 * a0 * tEnd;
        float t1 = a1 * a1 * tEnd;
        float ti = 0.5f * (t0 + t1);
        float dt = (t1 - t0);

        float3 p = x + w * ti;
        float rp = length(p);
        float h = max(0.0f, rp - RbPhys);

        float dR = DensityRayleigh(h);
        float dM = DensityMie(h);
        float dO = DensityOzone(h);

        // extinction coefficients (1/m)
        float3 sigmaExt = RayleighScattering * dR + (MieScattering + MieAbsorption) * dM + O3_COEFF * dO;

        tau += sigmaExt * dt;
    }

    float3 T = exp(-tau);
    OutTransmittance[id.xy] = float4(T, 1.0);
}

