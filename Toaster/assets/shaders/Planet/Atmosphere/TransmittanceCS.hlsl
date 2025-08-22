#inputlayout
#type compute
#pragma pack_matrix(row_major)

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
    float3 RayleighScattering; // beta_R at sea level (1/m), RGB
    float3 MieScattering; // beta_Ms at sea level (1/m), RGB
    float3 MieAbsorption; // beta_Ma at sea level (1/m), RGB
    float3 GroundAlbedo;
    float OzoneStrength;
    uint StepsTransmittance; // suggest 64–128 for TLUT
    uint StepsMultiScattering; // unused here
    float APFarDynamic;
};

RWTexture2D<float4> OutTransmittance : register(u0);

static const float PI = 3.14159265359;

// ---------------- densities ----------------
float DensityRayleigh(float h)
{
    return exp(-max(h, 0.0) / max(RayScaleHeight, 1e-3));
}
float DensityMie(float h)
{
    return exp(-max(h, 0.0) / max(MieScaleHeight, 1e-3));
}

float DensityOzone(float hMeters)
{
    float km = hMeters * 1e-3;
    float tri = saturate(1.0 - abs((km - 25.0) / 15.0)); // 10..40 km, peak ~25
    return tri * OzoneStrength;
}

// ------------- mapping helpers -------------
float RadiusFromV(float v, float Rg, float Rt)
{
    return lerp(Rg, Rt, saturate(v));
}

// Horizon-aware μ (no ground hit); push off the tangent a hair.
float MuFromU(float u, float r, float Rg)
{
    float muMin = -sqrt(saturate(1.0 - (Rg * Rg) / (r * r)));
    const float eps = 1e-5f;
    return lerp(muMin + eps, 1.0f - eps, saturate(u));
}

// --------------- main ----------------------
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint W, H;
    OutTransmittance.GetDimensions(W, H);
    if (id.x >= W || id.y >= H)
        return;

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;

    float u = (id.x + 0.5f) / float(W); // μ in [μmin(r),1]
    float v = (id.y + 0.5f) / float(H); // r in [Rg,Rt]

    float r = RadiusFromV(v, Rg, Rt);
    float mu = MuFromU(u, r, Rg);

    // Ray origin & dir in planet frame (centered)
    float3 x = float3(0.0, 0.0, r);
    float sinTheta = sqrt(saturate(1.0 - mu * mu));
    float3 w = float3(sinTheta, 0.0, mu);

    // Intersect TOA only (no ground early-out)
    // (This stays finite by construction of μ)
    float b = dot(x, w);
    float c = dot(x, x) - Rt * Rt;
    float h = b * b - c;
    float tEnd = 0.0;
    if (h > 0.0)
        tEnd = -b + sqrt(h); // forward exit

    // Integrate extinction with a start-biased partition (denser near camera)
    // We integrate over variable steps: [t_i, t_{i+1}] with t=a^2 * tEnd
    uint N = max(1u, StepsTransmittance);
    float3 tau = 0.0;
    float tPrev = 0.0;

    [loop]
    for (uint i = 0; i < N; ++i)
    {
        float a0 = (float(i) / float(N));
        float a1 = (float(i + 1) / float(N));
        float t0 = a0 * a0 * tEnd;
        float t1 = a1 * a1 * tEnd;
        float ti = 0.5f * (t0 + t1);
        float dt = (t1 - t0);

        float3 p = x + w * ti;
        float rp = length(p);
        float alt = max(0.0, rp - Rg);

        float dR = DensityRayleigh(alt);
        float dM = DensityMie(alt);
        float dO = DensityOzone(alt);

        float3 sigmaExt = RayleighScattering * dR
                        + (MieScattering + MieAbsorption) * dM
                        + float3(0.650e-6, 1.881e-6, 0.085e-6) * dO;

        tau += sigmaExt * dt;
    }

    float3 T = exp(-tau);
    OutTransmittance[id.xy] = float4(T, 1.0);
}
