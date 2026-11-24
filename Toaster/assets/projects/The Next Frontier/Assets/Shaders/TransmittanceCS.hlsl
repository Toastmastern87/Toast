#type compute
#pragma pack_matrix(row_major)

// ---------- cbuffers ----------
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
    float3 BasisLonEast;
    float3 BasisLonNorth;
    float3 BasisSpinUp;
}

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight; // 0: Rt - Rg (m)
    float RayScaleHeight; // 4
    float MieScaleHeight; // 8
    float OzoneStrength; // 12

    float3 RayleighScattering; // 16
    float MieAnisotropyBroad; // 28

    float3 MieScattering; // 32
    float BakeIBL; // 44

    float3 MieAbsorption; // 48
    uint FaceIndex; // 60

    float3 GroundAlbedo; // 64
    uint StepsTransmittance; // 76

    float3 MieAnisotropy; // 80
    uint StepsMultiScattering; // 92

    float3 MieLobeWeight; // 96
    float _AtmospherePad0; // 108
}

RWTexture2D<float4> OutTransmittance : register(u0);

// ---------- constants ----------
static const float3 O3_COEFF = float3(0.650e-6, 1.881e-6, 0.085e-6); // Bruneton

#include "TransmittanceLUT.hlsli"

// ---------- densities ----------
float DensityExpTapered(float h, float H)
{
    float hTop = AtmosphereHeight - min(0.0f, MinHeight);
    H = max(H, 1e-3f);
    return max(exp(-max(h, 0.0f) / H) - exp(-hTop / H), 0.0f);
}

float DensityRayleigh(float h)
{
    return DensityExpTapered(h, RayScaleHeight);
}
float DensityMie(float h)
{
    return DensityExpTapered(h, MieScaleHeight);
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

// ---------- main ----------
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint W, H;
    OutTransmittance.GetDimensions(W, H);
    if (id.x >= W || id.y >= H)
        return;

    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);

    float2 uv = float2((id.x + 0.5f) / float(W), (id.y + 0.5f) / float(H));

    float r, mu;
    TransRMuFromUV(uv, RbPhys, Rt, r, mu);
   
    float3 x = float3(0.0f, r, 0.0f);
    float s = sqrt(saturate(1.0f - mu * mu));
    // Put ray in X–Y plane; y is "up"
    float3 w = float3(s, mu, 0.0f);

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

