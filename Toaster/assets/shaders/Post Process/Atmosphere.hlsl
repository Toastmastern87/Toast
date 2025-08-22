#inputlayout 
#type vertex
#pragma pack_matrix( row_major ) 
    
struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
    PixelInputType output;
    output.texCoord = float2((vID << 1) & 2, vID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1);
    return output;
}

#type pixel
#pragma pack_matrix(row_major)

Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScattLUT : register(t1);
Texture2D<float4> SkyViewLUT : register(t2);
Texture3D<float4> AerialPerspective : register(t3);
Texture2D<float> DepthTex : register(t9);
Texture2D<float4> BaseColor : register(t10);

SamplerState ClampLinear : register(s0);
SamplerState ClampPoint : register(s1);
SamplerState UWrapVClampLinear : register(s2);

// ------------------- CONTROLS -------------------
#ifndef ATMO_GAIN
#define ATMO_GAIN 8.0f
#endif
#ifndef APPLY_GAMMA_OUT
#define APPLY_GAMMA_OUT 0
#endif
#ifndef STAR_LUMA_K
#define STAR_LUMA_K 360.0f
#endif
#ifndef STAR_SUN_PROX_SCALE
#define STAR_SUN_PROX_SCALE 12.0f
#endif
#ifndef SPACE_SKY_HSCALE_MULT
#define SPACE_SKY_HSCALE_MULT 6.0f
#endif
#ifndef SPACE_SKY_TRANSITION_H
#define SPACE_SKY_TRANSITION_H 2.0f
#endif
#ifndef SPACE_SKY_SCALE
#define SPACE_SKY_SCALE 0.6f
#endif
// Optional cosmetic tweak to reduce “neon-blue” limb from orbit.
// 0.0 = off (fully physical). Try 0.10–0.25 for a subtle desat.
#ifndef SPACE_LIMB_DESAT
#define SPACE_LIMB_DESAT 0.0f
#endif
// ------------------------------------------------

#define BYPASS_SKYVIEW 0
#define BYPASS_AP      0
#define DBG            0

static const float PI = 3.14159265359f;

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

cbuffer SunParamsCB : register(b6)
{
    float SunDiscRadius;
    float SunEdgeSoftness;
    float SunGlowSize;
    float SunGlowIntensity;
    uint SunDiscToggle;
};

#ifndef VISUAL_H_MULT
#define VISUAL_H_MULT 6.0f // same as CS
#endif

#ifndef SUN_DIR_NEGATE
#define SUN_DIR_NEGATE 1
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

float3 ReconstructViewPos(float2 uv, float depth01)
{
    float2 ndcXY = float2(uv.x, 1.0 - uv.y) * 2.0 - 1.0;
    float4 clip = float4(ndcXY, depth01, 1.0);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    return vpos.xyz / vpos.w;
}
float3 ViewRayDirVS(float2 uv)
{
    float2 ndcXY = float2(uv.x, 1.0 - uv.y) * 2.0 - 1.0;
    float4 clip = float4(ndcXY, 0.0, 1.0);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    return normalize(vpos.xyz / vpos.w);
}

struct RayHit
{
    bool hit;
    float t0;
    float t1;
};
RayHit RaySphereWS(float3 ro, float3 rd, float rad)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - rad * rad;
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

void SkyParamsFromDir(float3 dirWS, out float azim01, out float muV)
{
    float3 camWS = cameraPosition.xyz;
    float3 upWS = normalize(camWS - PlanetCenterWS);

    float3 eastWS = normalize(BasisTanEast - upWS * dot(BasisTanEast, upWS));
    float3 northWS = normalize(BasisTanNorth - upWS * dot(BasisTanNorth, upWS));

    muV = dot(dirWS, upWS); // [-1..1]
    float sin2 = max(0.0, 1.0 - muV * muV);

    if (sin2 < 1e-8)
    {
        azim01 = 0.0;
        return;
    }

    float invSin = rsqrt(sin2);
    float3 h = (dirWS - upWS * muV) * invSin;

    float cosP = dot(h, eastWS);
    float sinP = dot(h, northWS);
    float phi = atan2(sinP, cosP);
    if (phi < 0.0)
        phi += 2.0 * PI;

    azim01 = phi * (1.0 / (2.0 * PI));
}

// AP UVW with μv in [-1..1] → v in [0..1]
float3 AP_UVW(float dist, float muV, float rCam)
{
    float u = sqrt(saturate(dist / max(1e-3, APFarDynamic)));
    float v = 0.5 * (1.0 - muV);
    float rMin = PlanetRadius + MinHeight;
    float rMax = PlanetRadius + AtmosphereHeight;
    float w = saturate((rCam - rMin) / max(1e-3, (rMax - rMin)));
    return float3(u, v, w);
}

// Closest altitude inside TOA (meters)
float ClosestAltitudeWS(float3 camWS, float3 dirWS)
{
    float3 ro = camWS - PlanetCenterWS;
    float Rt = PlanetRadius + AtmosphereHeight;
    RayHit h = RaySphereWS(ro, dirWS, Rt);
    if (!h.hit)
        return 1e9;
    float tEnter = max(0.0, h.t0);
    float tExit = max(0.0, h.t1);
    float tMid = 0.5 * (tEnter + tExit);
    float3 pMin = ro + dirWS * tMid;
    float rMin = length(pMin);
    return max(0.0, rMin - PlanetRadius);
}

// Surface horizon μ (negative = below +Z/up) for rCam > Rg
float MuSurfaceHorizon(float rCam, float Rg)
{
    if (rCam <= Rg + 1.0f)
        return 0.0f;
    float ratio = Rg / rCam; // (0,1)
    return -sqrt(saturate(1.0f - ratio * ratio)); // μ = -sin(depression)
}
float VFromMu_H(float mu, float muH)
{
    return (mu >= muH)
         ? (0.5f + 0.5f * (mu - muH) / max(1e-6f, (1.0f - muH)))
         : (0.5f * (mu + 1.0f) / max(1e-6f, (muH + 1.0f)));
}

float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD) : SV_Target
{
    float depth = DepthTex.Sample(ClampPoint, uv);

    float3 dirVS = ViewRayDirVS(uv);
    float3 dirWS = normalize(mul(dirVS, (float3x3) inverseViewMatrix));
    float3 camWS = cameraPosition.xyz;
    float3 upWS = normalize(camWS - PlanetCenterWS);
    float rCam = length(camWS - PlanetCenterWS);

    float3 baseRGB = BaseColor.Sample(ClampLinear, uv).rgb;
    bool isSky = (depth <= 1e-16);

    // ---------------- SKY ----------------
    if (isSky && BYPASS_SKYVIEW == 0)
    {
        float azim01, muV;
        SkyParamsFromDir(dirWS, azim01, muV);

        // ✅ CORRECT mapping for SkyView LUT: μv ∈ [-1..1] → v ∈ [0..1]
        uint lutW, lutH;
        SkyViewLUT.GetDimensions(lutW, lutH);
        float u = azim01;
        // In Atmosphere PS (sky path), when building (u,v) for SkyViewLUT:
        float muH = MuSurfaceHorizon(rCam, PlanetRadius); // << match the CS
        float v = (muV >= muH)
           ? (0.5f + 0.5f * (muV - muH) / max(1e-6f, (1.0f - muH)))
           : (0.5f * (muV + 1.0f) / max(1e-6f, (muH + 1.0f)));

        float2 dx_uv = float2(ddx(u), ddx(v));
        float2 dy_uv = float2(ddy(u), ddy(v));
        float uDanger = (abs(dx_uv.x) + abs(dy_uv.x)) * lutW;
        float poleFix = saturate(uDanger);
        float2 dx_fix = lerp(dx_uv, float2(0.0, dx_uv.y), poleFix);
        float2 dy_fix = lerp(dy_uv, float2(0.0, dy_uv.y), poleFix);

        float3 skyMain = SkyViewLUT.SampleGrad(UWrapVClampLinear, float2(u, v), dx_fix, dy_fix).rgb;
        float3 skyAvg = 0;
        [unroll]
        for (int i = 0; i < 4; ++i)
            skyAvg += SkyViewLUT.Sample(UWrapVClampLinear, float2(u + 0.25f * i, v)).rgb;
        skyAvg *= 0.25f;
        float3 skyL = lerp(skyMain, skyAvg, poleFix); // linear HDR

        // Optional: subtle space-limb desaturation to avoid “fake neon blue”
        if (SPACE_LIMB_DESAT > 0.0f)
        {
            float Rt = PlanetRadius + AtmosphereHeight;
            if (rCam >= Rt)
            {
                float limb = saturate(-muV); // 0 outward, 1 toward planet/limb
                float k = SPACE_LIMB_DESAT * sqrt(limb);
                float grey = dot(skyL, float3(0.2126, 0.7152, 0.0722));
                skyL = lerp(skyL, grey.xxx, k);
            }
        }

        // Sun disc (unchanged)
        float3 sunDir = GetSunDirWS();
        float muToSun = saturate(dot(dirWS, sunDir));
        float ang = acos(muToSun);
        float dMu = abs(ddx(muToSun)) + abs(ddy(muToSun));
        float dAng = dMu / max(1e-3, sqrt(1.0 - muToSun * muToSun));
        float edgeAA = 1.5 * dAng;
        float edge = max(SunEdgeSoftness, edgeAA);
        float discMask = smoothstep(SunDiscRadius + edge, SunDiscRadius - edge, ang);

        float muS_for_edge = saturate(dot(upWS, sunDir));
        float rimHarden = lerp(2.0, 1.0, muS_for_edge);
        discMask = pow(discMask, rimHarden);

        RayHit gSun = RaySphereWS(camWS - PlanetCenterWS, sunDir, PlanetRadius);
        bool sunBlocked = (gSun.hit && gSun.t0 > 0.0);

        float3 discHDR = 0.0;
        if (SunDiscToggle != 0 && !sunBlocked)
        {
            float3 TcamSun = LookupTransSafe(rCam, muS_for_edge);
            float kHorizon = lerp(0.55, 1.0, smoothstep(0.0, 0.25, muS_for_edge));
            float3 Tvis = max(pow(TcamSun, kHorizon), 0.002.xxx);
            float3 SunRadiance = GetSunIlluminance();
            discHDR = discMask * SunRadiance * Tvis;
        }

        // Space gating by closest altitude (same as before)
        float Rt = PlanetRadius + AtmosphereHeight;
        float H_R = max(1e-3, RayScaleHeight);
        float h_min = ClosestAltitudeWS(camWS, dirWS);
        float h_cut = min(AtmosphereHeight, SPACE_SKY_HSCALE_MULT * H_R);
        float h_band = max(500.0, SPACE_SKY_TRANSITION_H * H_R);
        float skyGate = (rCam >= Rt) ? smoothstep(h_cut + h_band, h_cut - h_band, h_min) : 1.0;

        float spaceScale = (rCam >= Rt) ? SPACE_SKY_SCALE : 1.0;
        skyL *= (spaceScale * skyGate);

        // Background stars (no TLUT loss in space)
        RayHit gBg = RaySphereWS(camWS - PlanetCenterWS, dirWS, PlanetRadius);
        bool bgBlocked = (gBg.hit && gBg.t0 > 0.0);
        float3 T_space = (rCam >= Rt) ? 1.0.xxx : LookupTransSafe(rCam, muV);

        const float3 LUMA = float3(0.2126, 0.7152, 0.0722);
        float skyY = dot(skyL, LUMA);
        float discY = dot(discHDR, LUMA);
        float starsLumaDim = exp(-STAR_LUMA_K * (skyY + discY));
        float proxRadius = STAR_SUN_PROX_SCALE * SunDiscRadius;
        float sunProxMask = smoothstep(0.0, proxRadius, ang);
        float starsDim = lerp(1.0, starsLumaDim * sunProxMask, skyGate);

        float3 bgTerm = bgBlocked ? 0.0.xxx : (baseRGB * T_space * starsDim);

        float3 outHDR = bgTerm + ATMO_GAIN * (skyL + discHDR);
#if APPLY_GAMMA_OUT
        outHDR = pow(saturate(outHDR), 1.0/2.2);
#endif
        return float4(outHDR, 1.0);
    }

    // ---------------- GEOMETRY + AP ----------------
    float3 posVS = ReconstructViewPos(uv, depth);
    float dist = length(posVS);
    float muVg = dot(dirWS, upWS);
    float3 uvw = AP_UVW(min(dist, APFarDynamic), muVg, rCam);
    float4 ap = (BYPASS_AP == 0) ? AerialPerspective.Sample(ClampLinear, uvw)
                                    : float4(0, 1, 0, 1);

    float3 outRGB = ATMO_GAIN * ap.rgb + ap.a * baseRGB;

#if APPLY_GAMMA_OUT
    outRGB = pow(saturate(outRGB), 1.0/2.2);
#endif
    return float4(outRGB, 1.0);
}
