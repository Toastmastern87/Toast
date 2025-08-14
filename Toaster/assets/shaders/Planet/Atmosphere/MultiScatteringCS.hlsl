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
};


Texture2D<float4> TransmittanceLUT : register(t0);
SamplerState ClampLinear : register(s0);
RWTexture2D<float4> OutMultiScatter : register(u0);

static const float PI = 3.14159265359;

// Inverse map
float2 TransUV(float r, float mu)
{
    float Rg2 = PlanetRadius * PlanetRadius;
    float Rt2 = (PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight);
    float v = saturate((r * r - Rg2) / (Rt2 - Rg2));
    float u = saturate((mu + 1.0) * 0.5);
    return float2(u, v);
}

float3 LookupTrans(float r, float mu)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu), 0.0).rgb;
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

float PhaseMie(float cosTheta, float gVal)
{
    float g2 = gVal * gVal;
    float denom = pow(1.0 + g2 - 2.0 * gVal * cosTheta, 1.5);
    return (1.0 - g2) / (4.0 * PI * denom);
}
float PhaseRayleigh(float cosTheta)
{
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

float DensityRayleigh(float h)
{
    return exp(-h / RayScaleHeight);
}

float DensityMie(float h)
{
    return exp(-h / MieScaleHeight);
}

// Short single-scatter integral used to estimate higher orders
float3 SingleScatterShort(float3 x, float3 wi, float3 sdir, float segLen)
{
    float Rt = PlanetRadius + AtmosphereHeight;  
    
    // clamp segment to within atmosphere shell
    float3 roCenter = x; // planet-centered coords not needed for short segment
    float3 rd = wi;

    // simple fixed steps
    uint SAMPLES = max(1u, StepsMultiScattering);
    float ds = segLen / SAMPLES;

    float3 L = 0.0;
    float3 tau = 0.0;

    [loop]
    for (uint i = 0; i < SAMPLES; ++i)
    {
        float t = (i + 0.5) * ds;
        float3 p = x + rd * t;

        // radius/altitude
        float r = length(p);
        // If we drop below ground, stop (rare with small segments above ground)
        if (r < PlanetRadius || r > Rt) 
            break;

        float h = max(0.0, r - PlanetRadius);

        float dR = DensityRayleigh(h);
        float dM = DensityMie(h);
        
        float3 sigmaExt = RayleighScattering * dR + (MieScattering + MieAbsorption) * dM;

        // Transmittance to sun from p (via LUT)
        float muS = dot(normalize(p), sdir);
        float3 T_sun = LookupTrans(r, muS);

        // Phase
        float cosTheta = dot(rd, sdir);
        float pR = PhaseRayleigh(cosTheta);
        float pM = PhaseMie(cosTheta, MieAnisotropy);

        float3 sigmaScat = RayleighScattering * dR * pR + MieScattering * dM * pM;

        float3 T_view = exp(-tau);

        L += T_view * (sigmaScat * T_sun) * ds;
        tau += sigmaExt * ds;
    }
    return L;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint texWidth, texHeight;
    OutMultiScatter.GetDimensions(texWidth, texHeight);
    
    if (id.x >= texWidth || id.y >= texHeight)
        return;

    // Paramization: x = muS (sun zenith), y = altitude
    float u = (id.x + 0.5) / texWidth;
    float v = (id.y + 0.5) / texHeight;
    float muS = MuFromU(u);
    float r = RadiusFromV(v);

    // Position at this altitude along +Z axis
    float3 x = float3(0, 0, r);
    float3 up = normalize(x);

    // Build an orthonormal basis (right, up, forward)
    float3 tmp = (abs(up.z) < 0.999) ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 right = normalize(cross(tmp, up));
    float sinZS = sqrt(saturate(1.0 - muS * muS));

    // Sun direction having the requested zenith (muS) and arbitrary azimuth (right)
    float3 sdir = normalize(right * sinZS + up * muS);

    // Two representative hemisphere directions: up & horizon
    float3 w1 = up;
    float3 t = normalize(abs(up.z) < 0.999 ? cross(up, float3(0, 0, 1)) : cross(up, float3(0, 1, 0)));
    float3 w2 = t; // horizon-ish

    float Rt = PlanetRadius + AtmosphereHeight;
    float segLen = 0.25 * max(1.0, Rt - r);

    float3 L1 = SingleScatterShort(x, w1, sdir, segLen);
    float3 L2 = SingleScatterShort(x, w2, sdir, segLen);

    // Fast trick: average two directions and scale by 2
    float3 Lms = 2.0 * 0.5 * (L1 + L2);

    // Simple ground feedback to lift ambient a bit (tunable)
    const float groundBoost = 0.25;
    Lms += groundBoost * GroundAlbedo * (L1 + L2) * (1.0 / 3.0);

    OutMultiScatter[id.xy] = float4(Lms, 1.0);
}