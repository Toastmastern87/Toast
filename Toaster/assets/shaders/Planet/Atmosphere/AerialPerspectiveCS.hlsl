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
    float3 PlanetCenterWS;
    float PlanetRadius;
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
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
    uint StepsMultiScattering; // (unused)
    float APFarDynamic;
};

// ---- resources ------------------------------------------------------------
Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
SamplerState ClampLinear : register(s0);

// OPTIONAL: downsampled min depth to skip sky pixels for AP
//   0..1 depth; values near 1 mean sky only
// If you don’t have this yet, comment the three lines that reference it.
Texture2D<float> MinDepthMask : register(t8);

RWTexture3D<float4> OutAP : register(u0);

// ---- config / fast-math ---------------------------------------------------
static const float PI = 3.14159265359;

#ifndef SUN_DIR_NEGATE
#define SUN_DIR_NEGATE 1
#endif

// Early-exit when transmittance A is already tiny; rest of slices won’t change the image.
#ifndef AP_A_EPS
#define AP_A_EPS 0.003f
#endif

// If you want to clamp Z for perf experiments without reallocating the 3D tex:
#ifndef AP_MAX_Z_SLICES
#define AP_MAX_Z_SLICES 96u   // try 64–128 instead of 256
#endif

// Use exp2(x) ~ 2^x (faster) instead of exp(x)
float3 fastExp3(float3 x)
{
    return exp2(x * 1.4426950408889634);
} // 1/ln(2)

// ---- helpers --------------------------------------------------------------
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

// ---- kernel ---------------------------------------------------------------
[numthreads(8, 8, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint W, H, Dfull;
    OutAP.GetDimensions(W, H, Dfull);
    if (tid.x >= W || tid.y >= H)
        return;

    // Optional: skip XY where screen is sky-only (no geometry needs AP)
    // Comment out if you don't bind MinDepthMask
    float minDepth = MinDepthMask.Load(int3(tid, 0));
    if (minDepth >= 0.999f) // all sky
    {
        float4 v = float4(0, 0, 0, 1);
        [loop]
        for (uint z = 0; z < Dfull; ++z)
            OutAP[uint3(tid.x, tid.y, z)] = v;
        return;
    }

    // Clamp the number of Z slices we actually compute this frame
    uint D = min(Dfull, AP_MAX_Z_SLICES);

    // ---- reconstruct view ray for this screen pixel ----
    float2 uv = (tid + 0.5) / float2(W, H);
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0); // D3D
    float4 clip = float4(ndc, 1.0, 1.0);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 dirVS = normalize(vpos.xyz / vpos.w);
    float3 wWS = normalize(mul(dirVS, (float3x3) inverseViewMatrix));

    // Planet-centered ray
    float3 ro = cameraPosition.xyz - PlanetCenterWS;
    float3 w = wWS;

    // Intersections with TOA and ground
    float Rt = PlanetRadius + AtmosphereHeight;
    RayHit hitAtm = RaySphereIntersect(ro, w, Rt);
    if (!hitAtm.hit || APFarDynamic <= 1e-3f)
    {
        float4 v = float4(0, 0, 0, 1);
        [loop]
        for (uint z = 0; z < Dfull; ++z)
            OutAP[uint3(tid.x, tid.y, z)] = v;
        return;
    }

    float tEnter = max(0.0, hitAtm.t0);
    float tExit = max(0.0, hitAtm.t1);

    RayHit hitG = RaySphereIntersect(ro, w, PlanetRadius);
    if (hitG.hit && hitG.t0 > 0.0)
        tExit = min(tExit, hitG.t0);

    if (tExit <= tEnter)
    {
        float4 v = float4(0, 0, 0, 1);
        [loop]
        for (uint z = 0; z < Dfull; ++z)
            OutAP[uint3(tid.x, tid.y, z)] = v;
        return;
    }

    // cumulative integrals from camera to current slice end
    float3 Lcum = 0.0;
    float3 tauCum = 0.0;

    // consts
    const float3 betaO3 = float3(0.650e-6, 1.881e-6, 0.085e-6);
    const float3 betaExtM = MieScattering + MieAbsorption;
    float3 sdir = GetSunDirWS();
    float3 SunE = GetSunIlluminance();

    // Z mapping: d(z) = APFarDynamic * (z/D)^2  -> incremental to avoid pow
    float invD = 1.0 / max(1.0, (float) D);
    float invD2 = invD * invD;
    float dStart = 0.0;
    float dDelta = APFarDynamic * (1.0 * invD2); // (2*0 + 1)/D^2 * APFarDynamic

    [loop]
    for (uint z = 0; z < D; ++z)
    {
        float d0 = dStart;
        float d1 = dStart + dDelta; // next slice end
        dStart += dDelta; // advance start for next slice
        dDelta += APFarDynamic * (2.0 * invD2); // (2z+3 - (2z+1)) * APFarDynamic/D^2 = 2/D^2 * APFarDynamic

        float segA = max(d0, tEnter);
        float segB = min(d1, tExit);

        if (segB > segA)
        {
            // slice midpoint
            float len = segB - segA;
            float tMid = 0.5f * (segA + segB);
            float3 pMid = ro + w * tMid;
            float rMid = length(pMid);
            float hMid = max(0.0, rMid - PlanetRadius);

            float dR = exp2(-hMid / max(1e-3, RayScaleHeight) * 1.44269504); // fast exp
            float dM = exp2(-hMid / max(1e-3, MieScaleHeight) * 1.44269504);
            float dO = DensityOzone(hMid);

            float3 sigmaExt = RayleighScattering * dR + betaExtM * dM + betaO3 * dO;
            float c = dot(w, sdir);
            float pR = PhaseRayleigh(c);
            float pM = PhaseMie(c, MieAnisotropy);
            float3 sigmaSca = RayleighScattering * dR * pR + MieScattering * dM * pM;

            float muS = dot(normalize(pMid), sdir);
            float3 Tsun = 0.0;
            RayHit gHit = RaySphereIntersect(pMid, sdir, PlanetRadius);
            if (!(gHit.hit && gHit.t0 > 0.0))
                Tsun = LookupTransSafe(rMid, muS) * SunE;

            float3 Ms = LookupMS(rMid, muS) * SunE;

            float3 dTau = sigmaExt * len;
            float3 wI = (1.0.xxx - fastExp3(-dTau)) / max(dTau, 1e-6.xxx);

            float3 TtoA = fastExp3(-tauCum);
            float3 J = sigmaSca * (Tsun + Ms) * wI;

            Lcum += TtoA * J;
            tauCum += dTau;
        }

        float3 Tcum = fastExp3(-tauCum);
        float A = saturate((Tcum.r + Tcum.g + Tcum.b) * (1.0 / 3.0));
        OutAP[uint3(tid.x, tid.y, z)] = float4(Lcum, A);

        // Early fill if we’re effectively opaque already or past tExit
        if (A <= AP_A_EPS || dStart >= tExit)
        {
            [loop]
            for (uint zz = z + 1; zz < Dfull; ++zz)
                OutAP[uint3(tid.x, tid.y, zz)] = float4(Lcum, A);
            return;
        }
    }

    // If we computed fewer Z than the texture depth, replicate the last slice
    if (D < Dfull)
    {
        float4 last = OutAP[uint3(tid.x, tid.y, D - 1)];
        [loop]
        for (uint zz = D; zz < Dfull; ++zz)
            OutAP[uint3(tid.x, tid.y, zz)] = last;
    }
}
