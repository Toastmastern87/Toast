#inputlayout
#type compute
#pragma pack_matrix(row_major)

// ---- lights ---------------------------------------------------------------
cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction; // xyz dir; negate below if needed
    float4 radiance; // rgb radiance/color (linear)
    float multiplier; // intensity scale
};

// ---- planet/atmos ---------------------------------------------------------
cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCentreVS;
    float PlanetRadius;
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
    float3 _pad0;
    float3 _pad1;
    float3 _pad2;
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
    float APFarDynamic; // NEW: max distance represented on X axis
};

// ---- resources ------------------------------------------------------------
Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
SamplerState ClampLinear : register(s0);
RWTexture3D<float4> OutAP : register(u0);

static const float PI = 3.14159265359;

// ---- config helpers -------------------------------------------------------
#ifndef SUN_DIR_NEGATE
#define SUN_DIR_NEGATE 1
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

// ---- math helpers ---------------------------------------------------------
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

// DX11 V-flipped transmittance lookups
float2 TransUV(float r, float mu)
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

    if (r < PlanetRadius) // extra optical depth from r -> Rg
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

// 3D LUT axes: X=distance, Y=muV (DX11 flipped), Z=altitude slice
float SliceToRadius(uint z, uint D)
{
    float a = (z + 0.5) / D;
    float rMin = PlanetRadius + MinHeight; // supports valleys
    float rMax = PlanetRadius + AtmosphereHeight;
    return lerp(rMin, rMax, a);
}
float YToMuV(uint y, uint H)
{
    float v = (y + 0.5) / H;
    return 1.0 - v; // DX11 flip (horizon->zenith)
}
float XToDistance(uint x, uint W, float dMax)
{
    float a = (x + 0.5) / W;
    return dMax * (a * a); // square mapping = more near-camera res
}

// ---- kernel ---------------------------------------------------------------
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint W, H, D;
    OutAP.GetDimensions(W, H, D);
    if (id.x >= W || id.y >= H || id.z >= D)
        return;

    float3 up = normalize(-PlanetCentreVS);
    float r = SliceToRadius(id.z, D);

    float muV = YToMuV(id.y, H);
    if (muV <= 0.0)
    {
        OutAP[id.xyz] = float4(0, 0, 0, 1);
        return;
    }

    // pick a fixed azimuth (AP is fairly azimuth-smooth)
    float sinTh = sqrt(saturate(1.0 - muV * muV));
    float3 east = normalize(BasisTanEast);
    float3 w = east * sinTh + up * muV;

    // march length: X axis represents [0..APFarDynamic], but clamp to TOA
    float Rt = PlanetRadius + AtmosphereHeight;
    RayHit hitToa = RaySphereIntersect(up * r, w, Rt);
    if (!hitToa.hit)
    {
        OutAP[id.xyz] = float4(0, 0, 0, 1);
        return;
    }
    float tMaxPhys = max(0.0, hitToa.t1);
    float desiredD = XToDistance(id.x, W, APFarDynamic);
    float tEnd = min(desiredD, tMaxPhys);

    uint N = max(8u, StepsMultiScattering);
    float dt = tEnd / max(1.0, (float) N);

    float3 tau = 0.0;
    float3 L = 0.0;
    const float3 betaExtM = MieScattering + MieAbsorption;

    float3 sdir = GetSunDirVS();
    float3 SunE = GetSunIlluminance();

    const float3 betaO3 = float3(0.650e-5, 1.881e-5, 0.085e-5);
    
    [loop]
    for (uint i = 0; i < N && tEnd > 0.0; ++i)
    {
        float t = (i + 0.5) * dt;
        float3 p = up * r + w * t;
        float rp = length(p);
        float h = rp - PlanetRadius;

        // local densities: clamp below ground to avoid >1 explosion in valleys
        float hN = max(0.0, h);
        float dR = exp(-hN / RayScaleHeight);
        float dM = exp(-hN / MieScaleHeight);
        float dO = DensityOzone(hN);
        
        float3 sigmaExt = RayleighScattering * dR + betaExtM * dM + betaO3 * dO;
        float3 T_view = exp(-tau);

        // direct sun (blocked by ground)
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

        if (all(T_view < 1e-4))
            break;
    }

    float3 T_end = exp(-tau); // transmittance to this texel's distance
    OutAP[id.xyz] = float4(L, saturate(T_end.r));
}