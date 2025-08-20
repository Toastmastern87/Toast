#inputlayout
#type compute
#pragma pack_matrix( row_major )

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCentreVS;
    float PlanetRadius;
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
    float AtmosphereHeight;
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

RWTexture2D<float4> OutTransmittance : register(u0);

static const float PI = 3.14159265359;

struct RaySphereHit
{
    bool hit;
    float t0;
    float t1;
};

RaySphereHit RaySphereIntersect(float3 ro, float3 rd, float radius)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float h = b * b - c;
    RaySphereHit r;
    r.hit = (h >= 0.0);
    if (!r.hit)
    {
        r.t0 = r.t1 = 0.0;
        return r;
    }
    h = sqrt(h);
    r.t0 = -b - h;
    r.t1 = -b + h;
    return r;
}

float RadiusFromV(float v)     // v in [0,1] -> r in [Rg,Rt]
{
    float Rg2 = PlanetRadius * PlanetRadius;
    float Rt2 = (PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight);
    return sqrt(lerp(Rg2, Rt2, saturate(v)));
}

float MuFromU(float u)
{
    return lerp(-1.0, 1.0, saturate(u));
}

float DensityRayleigh(float h)
{
    return exp(-h / RayScaleHeight);
}

float DensityMie(float h)
{
    return exp(-h / MieScaleHeight);
}

float DensityOzone(float hMeters)
{
    float km = hMeters * 1e-3;
    float tri = saturate(1.0 - abs((km - 25.0) / 15.0)); // 10..40 km peak ~25
    return tri * OzoneStrength;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint texWidth, texHeight;
    OutTransmittance.GetDimensions(texWidth, texHeight);
    
    if (id.x >= texWidth || id.y >= texHeight)
        return;

    // Map pixel -> (r, mu)
    float u = (id.x + 0.5) / texWidth; // mu in [-1,1]
    float v = (id.y + 0.5) / texHeight; // r^2 in [Rg^2,Rt^2]
    float r = RadiusFromV(v);
    float mu = MuFromU(u);

    // Place point on +Z at radius r, shoot in local (mu) direction
    float3 x = float3(0.0, 0.0, r);
    float sinTheta = sqrt(saturate(1.0 - mu * mu));
    float3 w = float3(sinTheta, 0.0, mu);

    // Exit to TOA
    RaySphereHit hitToa = RaySphereIntersect(x, w, (PlanetRadius + AtmosphereHeight));
    if (!hitToa.hit)
    {
        OutTransmittance[id.xy] = float4(1, 1, 1, 1);
        return;
    }
    float tEnd = hitToa.t1;

    // If we hit ground on the way out, transmittance-to-space is zero
    RaySphereHit hitG = RaySphereIntersect(x, w, PlanetRadius);
    if (hitG.hit && hitG.t0 > 0.0 && hitG.t0 < tEnd)
    {
        OutTransmittance[id.xy] = float4(0, 0, 0, 1);
        return;
    }

    // Integrate extinction along the ray
    float3 tau = 0.0;
    uint N = max(1u, StepsTransmittance);
    float dt = tEnd / N;
    float t = 0.0;

    // (Optional) ozone spectrum term (rgb); 0 for Mars
    const float3 betaO3 = float3(0.650e-6, 1.881e-6, 0.085e-6);

    [loop]
    for (uint i = 0; i < N; ++i)
    {
        float ti = t + 0.5 * dt;
        float3 p = x + w * ti;
        float rp = length(p);
        float h = max(0.0, rp - PlanetRadius);

        float dR = DensityRayleigh(h);
        float dM = DensityMie(h);
        float dO = DensityOzone(h);
        
        float3 sigmaExt = RayleighScattering * dR + (MieScattering + MieAbsorption) * dM + betaO3 * dO;
        tau += sigmaExt * dt;

        t += dt;
    }

    float3 T = exp(-tau);
    OutTransmittance[id.xy] = float4(T, 1.0);
}