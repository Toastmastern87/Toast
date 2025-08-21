#inputlayout
#type compute
#pragma pack_matrix(row_major)

cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction; // xyz: light dir (see SUN_DIR_NEGATE)
    float4 radiance; // rgb radiance/color
    float multiplier; // intensity scale
};

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCentreVS;
    float PlanetRadius;
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight; // can be negative in valleys
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

Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
SamplerState ClampLinear : register(s0);
RWTexture2D<float4> OutSkyView : register(u0);

static const float PI = 3.14159265359;

// ---------- config ----------
#ifndef SUN_DIR_NEGATE
#define SUN_DIR_NEGATE 1   // set to 0 if 'direction' already points from point -> sun
#endif

float3 GetSunDirVS()
{
    float3 d = normalize(direction.xyz);
    return SUN_DIR_NEGATE ? -d : d;
}
float3 GetSunIlluminance()
{
    return radiance.rgb * multiplier;
}

// ---------- helpers ----------
struct RayHit
{
    bool hit;
    float t0;
    float t1;
};
RayHit RaySphereIntersect(float3 ro, float3 rd, float radius)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float h = b * b - c;
    RayHit r;
    r.hit = (h >= 0.0);
    if (!r.hit)
    {
        r.t0 = r.t1 = 0;
        return r;
    }
    h = sqrt(h);
    r.t0 = -b - h;
    r.t1 = -b + h;
    return r;
}

float2 TransUV(float r, float mu) // DX11 V flipped
{
    float Rg2 = PlanetRadius * PlanetRadius;
    float Rt = PlanetRadius + AtmosphereHeight;
    float Rt2 = Rt * Rt;
    float v = saturate((r * r - Rg2) / (Rt2 - Rg2));
    float u = saturate(0.5 * (mu + 1.0));
    return float2(u, v);
}

float3 LookupTransSafe(float r, float mu)
{
    float rc = max(r, PlanetRadius);
    float3 T = TransmittanceLUT.SampleLevel(ClampLinear, TransUV(rc, mu), 0).rgb;

    if (r < PlanetRadius)
    {
        float h0 = (PlanetRadius - r);
        float3 tauExtra =
            RayleighScattering * (RayScaleHeight * (exp(h0 / RayScaleHeight) - 1.0)) +
            (MieScattering + MieAbsorption) * (MieScaleHeight * (exp(h0 / MieScaleHeight) - 1.0));
        T *= exp(-tauExtra);
    }
    return T;
}

float3 LookupMS(float r, float muS)
{
    float Rg2 = PlanetRadius * PlanetRadius;
    float Rt = PlanetRadius + AtmosphereHeight;
    float Rt2 = Rt * Rt;
    float v = saturate((max(r, PlanetRadius) * max(r, PlanetRadius) - Rg2) / (Rt2 - Rg2));
    float u = saturate(0.5 * (muS + 1.0));
    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0).rgb;
}

float PhaseRayleigh(float c)
{
    return (3.0 / (16.0 * PI)) * (1.0 + c * c);
}
float PhaseMie(float c, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(max(1e-3, 1.0 + g2 - 2.0 * g * c), 1.5));
}

float DensityOzone(float hMeters)
{
    float km = hMeters * 1e-3;
    // triangular 10..40 km, peak at ~25 km
    return saturate(1.0 - abs((km - 25.0) / 15.0)) * OzoneStrength;
}

void DecodeSkyCoords(uint2 px, uint W, uint H, out float azim, out float muV)
{
    float u = (px.x + 0.5) / W; // [0,1] -> azimuth
    float v = (px.y + 0.5) / H; // [0,1] -> muV
    v = 1.0 - v; // DX11 flip to match paper orientation
    azim = u * (2.0 * PI);
    muV = saturate(v); // 0..1 (horizon..zenith)
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint W, H;
    OutSkyView.GetDimensions(W, H);
    if (id.x >= W || id.y >= H)
        return;

    // camera at origin in VS
    float3 up = normalize(-PlanetCentreVS); // center -> camera
    float r = length(PlanetCentreVS); // distance to center

    // --- make an orthonormal horizon frame ---
    float3 east = normalize(BasisTanEast - up * dot(BasisTanEast, up));
    float3 north = normalize(cross(up, east)); // ⟂ to both
    east = normalize(cross(north, up)); // re-orthogonalize east

    float azim, muV;
    DecodeSkyCoords(id.xy, W, H, azim, muV);
    muV = max(muV, 0.0);
    
    // --- decode sky pixel -> direction (zenith-stable) ---
    float muV_clamped = max(muV, 0.0);
    float sin2 = saturate(1.0 - muV_clamped * muV_clamped);

    float3 w;
    if (sin2 < 1e-8)
        w = up; // exactly zenith
    else
    {
        float sinTh = sqrt(sin2);
        float ca = cos(azim), sa = sin(azim);
        float3 h = ca * east + sa * north; // unit in horizon frame
        w = normalize(up * muV_clamped + h * sinTh);
    }

    float Rt = PlanetRadius + AtmosphereHeight;
    RayHit hitToa = RaySphereIntersect(up * r, w, Rt);
    if (!hitToa.hit)
    {
        OutSkyView[id.xy] = 0;
        return;
    }
    float tEnd = max(0.0, hitToa.t1);

    uint N = max(8u, StepsMultiScattering);
    float dt = tEnd / N;

    float3 tau = 0.0;
    float3 L = 0.0;
    const float3 betaExtM = MieScattering + MieAbsorption;
    
    float3 sdir = GetSunDirVS();
    float3 SunE = GetSunIlluminance();
    
    const float3 betaO3 = float3(0.650e-6, 1.881e-6, 0.085e-6);

    [loop]
    for (uint i = 0; i < N; ++i)
    {
        float t = (i + 0.5) * dt;
        float3 p = up * r + w * t;
        float rp = length(p);
        float h = rp - PlanetRadius; // can be negative in valleys

        float hN = max(0.0, h);
        float dR = exp(-hN / RayScaleHeight);
        float dM = exp(-hN / MieScaleHeight);
        float dO = DensityOzone(hN);        
        
        float3 sigmaExt = RayleighScattering * dR + betaExtM * dM + betaO3 * dO;;
        float3 T_view = exp(-tau);

        float muS = dot(normalize(p), sdir);
        float3 T_sun = 0.0;
        {
            RayHit gHit = RaySphereIntersect(p, sdir, PlanetRadius);
            if (!(gHit.hit && gHit.t0 > 0.0))
                T_sun = LookupTransSafe(rp, muS) * SunE;
        }

        float c = dot(w, sdir);
        float pR = PhaseRayleigh(c);
        float pM = PhaseMie(c, MieAnisotropy);
        float3 Ms = LookupMS(rp, muS) * SunE;

        float3 sigmaScat = RayleighScattering * dR * pR + MieScattering * dM * pM;

        L += T_view * sigmaScat * (T_sun + Ms) * dt;
        tau += sigmaExt * dt;
    }

    OutSkyView[id.xy] = float4(L,1);
}