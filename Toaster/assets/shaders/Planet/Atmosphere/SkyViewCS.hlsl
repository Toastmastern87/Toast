#inputlayout
#type compute
#pragma pack_matrix(row_major)

cbuffer Camera : register(b0)
{
    matrix worldTranslationMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float farZ;
    float nearZ;
    float viewportWidth;
    float viewportHeight;
};

cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction;
    float4 radiance;
    float multiplier;
};

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterWS; // unused here
    float PlanetRadius; // Rg
    float3 BasisTanEast; // unused here
    float MaxHeight; // unused here
    float3 BasisTanNorth; // unused here
    float MinHeight; // unused here
    float3 BasisRadUp; // unused here
    float3 BasisLonEast; // unused here
    float3 BasisLonNorth; // unused here
    float3 BasisSpinUp; // unused here
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

#ifndef SUN_DIR_NEGATE
#define SUN_DIR_NEGATE 1
#endif

#ifndef VISUAL_H_MULT
#define VISUAL_H_MULT 6.0f   // 5–7 works well; increase -> earlier sunset, decrease -> later
#endif

float3 GetSunDirWS()
{
    float3 d = normalize(direction.xyz);
    return SUN_DIR_NEGATE ? -d : d;
}
float3 GetSunIlluminance()
{
    return radiance.rgb * multiplier;
}

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
    return saturate(1.0 - abs((km - 25.0) / 15.0)) * OzoneStrength;
}

// -------- horizon-aware μ mapping --------
// Surface horizon μ (negative = below +Z/up) for rCam > Rg
float MuSurfaceHorizon(float rCam, float Rg)
{
    if (rCam <= Rg + 1.0f)
        return 0.0f;
    float ratio = Rg / rCam; // (0,1)
    return -sqrt(saturate(1.0f - ratio * ratio)); // μ = -sin(depression)
}

// v∈[0,1] (top=zenith after DX flip) -> μ using horizon as the centerline (v=0.5)
float MuFromV_H(float v, float muH)
{
    return (v >= 0.5f)
        ? lerp(muH, 1.0, (v - 0.5f) * 2.0f)
        : lerp(-1.0, muH, v * 2.0f);
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint W, H;
    OutSkyView.GetDimensions(W, H);
    if (id.x >= W || id.y >= H)
        return;

    float3 ro = cameraPosition.xyz - PlanetCenterWS;
    float3 up = normalize(ro);
    float rCam = length(ro);

    float3 east = normalize(BasisTanEast - up * dot(BasisTanEast, up));
    float3 north = normalize(cross(up, east));
    east = normalize(cross(north, up));

    // decode azimuth + horizon-relative v
    float u = (id.x + 0.5) / W;
    float v = 1.0 - (id.y + 0.5) / H; // DX flip (top = zenith)
    float azim = u * (2.0 * PI);

    float muH = MuSurfaceHorizon(rCam, PlanetRadius); // << use surface horizon
    float muV = (v >= 0.5f) ? lerp(muH, 1.0, (v - 0.5f) * 2.0) // MuFromV_H with muH
                        : lerp(-1.0, muH, v * 2.0f);

    float sin2 = saturate(1.0 - muV * muV);
    float3 w = (sin2 < 1e-8)
             ? ((muV >= 0.0) ? up : -up)
             : normalize(up * muV + (cos(azim) * east + sin(azim) * north) * sqrt(sin2));

    float Rt = PlanetRadius + AtmosphereHeight;
    RayHit hitToa = RaySphereIntersect(ro, w, Rt);
    if (!hitToa.hit)
    {
        OutSkyView[id.xy] = 0;
        return;
    }

    float tEnter = max(0.0, hitToa.t0);
    float tExit = max(0.0, hitToa.t1);
    float segLen = tExit - tEnter;
    if (segLen <= 0.0)
    {
        OutSkyView[id.xy] = 0;
        return;
    }

    uint N = max(8u, StepsMultiScattering);
    float dt = segLen / N;

    float3 tau = 0.0, L = 0.0;
    const float3 betaExtM = MieScattering + MieAbsorption;
    float3 sdir = GetSunDirWS();
    float3 SunE = GetSunIlluminance();
    const float3 betaO3 = float3(0.650e-6, 1.881e-6, 0.085e-6);

    [loop]
    for (uint i = 0; i < N; ++i)
    {
        float t = tEnter + (i + 0.5) * dt;
        float3 p = ro + w * t;
        float rp = length(p);
        float h = rp - PlanetRadius;

        float hN = max(0.0, h);
        float dR = exp(-hN / RayScaleHeight);
        float dM = exp(-hN / MieScaleHeight);
        float dO = DensityOzone(hN);

        float3 sigmaExt = RayleighScattering * dR + betaExtM * dM + betaO3 * dO;
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

    OutSkyView[id.xy] = float4(L, 1);
}
