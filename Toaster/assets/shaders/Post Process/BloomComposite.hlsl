﻿#inputlayout
#type vertex
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
    PixelInputType output;

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    output.uv = float2((vID << 1) & 2, vID & 2);
    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0.0f, 1);

    return output;
}

#type pixel
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
    float4x4 worldTranslationMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 inverseViewMatrix;
    float4x4 inverseProjectionMatrix;
    float4 cameraPosition;
    float farZ;
    float nearZ;
    float viewportWidth;
    float viewportHeight;
};

cbuffer DirectionalLight : register(b3)
{
    float4x4 lightViewProj;
    float4 direction; // FROM light -> scene
    float4 radiance; // RGB
    float SunIntensity;
};

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterWS;
    float PlanetRadius; // Rg
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
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
};

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius;
    float SunEdgeSoftness; // rad  (soft rim width)
    int SunDiscToggle; // 0=off, 1=on
    float SpaceDiscBrightnessScale; // unitless scale, e.g. 1.30
    
    float3 SunDiscWhite;
    float AirHaloIntensity; // 0..~0.6 (was HaloStrength_Ground, e.g. 0.28)
    
    float3 WarmTint; // e.g. float3(1.00, 0.92, 0.78)    
    float AirHaloStartFrac; // 0..1   (was InAirStart, e.g. 0.15)
    
    float AirHaloFalloffPow; // curve (was InAirPow, e.g. 1.10)
    float HorizonRefractionDeg; // deg (was RefracCenterDeg, e.g. 0.83)
    float TwilightBlendDeg; // deg (was TwilightExtraDeg, e.g. 1.5)
    float SpaceHaloWidthDeg; // deg (was SpaceHaloSigmaDeg, e.g. 0.8)
    
    float SpaceHaloIntensity; // 0.01..0.10 (was SpaceHaloGain, e.g. 0.04)
    float SpaceHaloCutoffDeg; // deg (was SpaceHaloCutoffDeg, e.g. 6.0)
};

cbuffer FloatingOrigin : register(b7)
{
    float3 WorldOffsetWS;
};


cbuffer BloomParams : register(b11)
{
    float SunSurfaceThreshold;
    float SunSurfaceIntensity;
    float SunSpaceThreshold;
    float SunSpaceIntensity;
    
    float SkySurfaceThreshold;
    float SkySurfaceIntensity;
    float SkySpaceThreshold;
    float SkySpaceIntensity;
    
    float GeometryThreshold;
    float GeometryIntensity;
    float SunRadius;
    float SkySurfaceRadius;
    
    float SkySpaceRadius;
    float SoftKnee;
    float SaturationClamp;
    float spaceFactor;
};

Texture2D sceneBaseTexture          : register(t0);
Texture2D bloomSunTexture           : register(t1);
Texture2D bloomSkyTexture           : register(t2);
Texture2D bloomGeomTexture          : register(t3);
Texture2D<float4> TransmittanceLUT  : register(t4);

SamplerState defaultSampler         : register(s0);
SamplerState ClampLinear            : register(s1);

static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);
static const float MU_EPS = 8e-4;

float3 clampSaturation(float3 c, float clampAmt)
{
    float y = dot(c, LUMA);
    return lerp(y.xxx, c, saturate(clampAmt));
}

float GroundBiasMeters(float Rg)
{
    return max(1.0f, 2e-6f * Rg);
}

struct Hit
{
    bool ok;
    float t0, t1;
};

Hit IntersectSphereGrazingSafe(float3 ro, float3 rd, float R)
{
    Hit H;
    H.ok = false;
    H.t0 = H.t1 = 0.0f;
    float Rabs = abs(R);
    if (Rabs <= 0.0f)
        return H;

    // Normalize direction for stable geometry form
    float a = dot(rd, rd);
    if (a <= 0.0f)
        return H;
    float invDirLen = rsqrt(max(a, 1e-30));
    float3 nrd = rd * invDirLen; // |nrd| = 1

    // Use cross-product form in unit-sphere space
    float3 roU = ro / Rabs; // O(1)
    float d2 = dot(cross(nrd, roU), cross(nrd, roU)); // <= ~1 when intersecting

    // Robust tangency handling: allow a tiny overshoot
    // NOTE: keep this the *same value everywhere you use this function*
    const float grazeTol = 5e-5; // ~1e-6..2e-4 are reasonable
    if (d2 > 1.0f + grazeTol)
        return H;

    float tca = -dot(roU, nrd); // along-ray to closest approach (radius units)
    float thc = sqrt(max(1.0f - d2, 0.0f)); // 0 at tangency

    // Convert back to world meters and original rd scale
    float t0 = (tca - thc) * Rabs * invDirLen;
    float t1 = (tca + thc) * Rabs * invDirLen;

    if (t0 > t1)
    {
        float tmp = t0;
        t0 = t1;
        t1 = tmp;
    }
    H.ok = true;
    H.t0 = t0;
    H.t1 = t1;
    return H;
}

// μ at horizon for a sphere of radius R as seen from r
float MuHorizon(float r, float R)
{
    float s = saturate(R / r);
    return -sqrt(max(1.0f - s * s, 0.0f));
}

float2 TransUV(float r, float mu, float RbPhys, float Rt)
{
    float rNorm = (r - RbPhys) / max(Rt - RbPhys, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (RbPhys * RbPhys) / (r * r)));
    mu = clamp(mu, muMin + MU_EPS, 1.0f - MU_EPS);
    return float2((mu - muMin) / (1.0f - muMin), saturate(rNorm));
}


float3 T_to_TOA(float r, float mu, float RbPhys, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, RbPhys, Rt), 0).rgb;
}

float SunVisibilityAtR(float r, float muS, float RbVis)
{
    float sH = RbVis / r;
    float cH = -sqrt(saturate(1.0f - sH * sH));
    return smoothstep(-sH * SunDiscRadius, sH * SunDiscRadius, muS - cH);
}

// Transmittance of the *atmospheric segment* intersected by ray (ro,rd).
// Uses your TLUT T_to_TOA(r, mu, Rb, Rt):
// If the LUT encodes T(p->TOA) = exp(-tau), then T_segment(p->q) = T(p->TOA) / T(q->TOA)
float3 TLUT_SegmentTransmittance(float3 ro, float3 rd, float RbVis, float Rt)
{
    Hit hit = IntersectSphereGrazingSafe(ro, rd, Rt);
    if (!hit.ok || hit.t1 <= max(0.0, hit.t0))
        return 1.0.xxx; // no atmosphere on ray

    float tEnter = max(0.0, hit.t0);
    float tExit = hit.t1;

    float3 pEnter = ro + rd * tEnter;
    float3 pExit = ro + rd * tExit;

    float rEnter = length(pEnter);
    float rExit = length(pExit);

    float3 upEnter = pEnter / rEnter;
    float3 upExit = pExit / rExit;

    float muEnter = dot(rd, upEnter); // cos zenith at entry
    float muExit = dot(rd, upExit); // ...at exit (same rd)

    float3 Tenter = T_to_TOA(rEnter, muEnter, RbVis, Rt);
    float3 Texit = T_to_TOA(rExit, muExit, RbVis, Rt);

    // Avoid divide-by-near-zero (if Texit extremely small, clamp)
    Texit = max(Texit, 1e-5.xxx);

    return saturate(Tenter / Texit);
}

float DayFactor(float3 camRel, float3 wSun, float RbPhys, float RbVis, float Rt)
{
    float r = length(camRel);
    float3 up = camRel / max(r, 1e-6);

    // 1) Geometric “sun altitude” term relative to the true horizon
    float muSun = clamp(dot(up, wSun), -1.0f, 1.0f); // cos(sun zenith)
    float muH = MuHorizon(r, RbVis); // horizon cosine at camera radius (<=0)

    // small safety lift to avoid numerical flicker at the horizon
    float muLo = muH + 0.002; // start of dawn
    float muHi = 0.06; // comfortably “day” (~3.4° altitude)
    float altTerm = saturate((muSun - muLo) / max(muHi - muLo, 1e-6));

    // 2) How much light actually reaches the eye from the sun direction
    float3 TeyeSun = (r <= Rt + 1e-3) ? T_to_TOA(r, muSun, RbPhys, Rt) : TLUT_SegmentTransmittance(camRel, wSun, RbVis, Rt);
    float Y = dot(TeyeSun, LUMA);
    float illum = smoothstep(0.02, 0.25, Y); // tweak these to taste

    // 3) Sun disc visibility at the camera (handles below-horizon = 0)
    float vis = SunVisibilityAtR(r, muSun, RbVis);

    // Final: geometric * illumination * visibility
    return saturate(altTerm * illum) * vis;
}

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
    float3 scene = sceneBaseTexture.Sample(defaultSampler, input.uv).rgb;
    float3 sunB = bloomSunTexture.Sample(defaultSampler, input.uv).rgb;
    float3 skyB = bloomSkyTexture.Sample(defaultSampler, input.uv).rgb;
    float3 geoB = bloomGeomTexture.Sample(defaultSampler, input.uv).rgb;
    
    float dayFactor = DayFactor(cameraPosition.xyz - WorldOffsetWS - PlanetCenterWS, -normalize(direction.xyz), PlanetRadius + min(0.0f, MinHeight), PlanetRadius + GroundBiasMeters(PlanetRadius),
                            PlanetRadius + AtmosphereHeight);
    
    // Intensities (surface↔space)
    float sunIntensity = lerp(SunSurfaceIntensity, SunSpaceIntensity, spaceFactor);
    float skyIntensity = lerp(SkySurfaceIntensity, SkySpaceIntensity, spaceFactor);
    
    skyIntensity *= lerp(0.35f, 1.0f, saturate(dayFactor));
    
    float geomIntensity = GeometryIntensity;
    
    // Mild saturation clamp to prevent color smear
    sunB = clampSaturation(sunB, SaturationClamp);
    skyB = clampSaturation(skyB, SaturationClamp);
    geoB = clampSaturation(geoB, SaturationClamp);
    
    float3 outColor = scene + sunB * sunIntensity + skyB * skyIntensity + geoB * geomIntensity;
    return float4(outColor, 1.0f);
}