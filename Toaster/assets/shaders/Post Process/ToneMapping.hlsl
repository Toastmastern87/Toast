#type pixel
#pragma pack_matrix(row_major)

Texture2D sceneHDR          : register(t9); // scene + bloom, linear HDR
Texture2D stars             : register(t10); // StarsRT from step 1
Texture2D SSAO              : register(t11);
Texture2D<float> SceneDepth : register(t12);

SamplerState LinearClamp    : register(s1);
SamplerState ClampPoint : register(s2);

cbuffer Camera : register(b0)
{
    matrix worldTranslationMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float far;
    float near;
    float viewportWidth;
    float viewportHeight;
};

cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction;
    float4 radiance;
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

cbuffer StarsParams : register(b7)
{
    float StarNits; // e.g. 600.0 (display-space peak for brightest texel)
    float DayFadeStartDeg; // start hiding stars above horizon (e.g. +2.0)
    float DayFadeEndDeg; // fully hidden by (e.g. 0.0 or -2.0)
    float TwilightStartDeg; // start appearing (e.g. 0.0)
    
    float TwilightEndDeg; // fully visible by (e.g. -6.0)
    float SpaceFadeStart; // altitude norm where space visibility starts (0..1), e.g. 0.85
    float SpaceFadeEnd; // fully visible by (0..1), e.g. 0.98
    float GlareInnerDeg; // sun glare inner angle (e.g. 5.0)
    
    float GlareOuterDeg; // sun glare outer angle (e.g. 12.0)
    float3 NightAmbient;
}

cbuffer Tonemapping : register(b10)
{
    float EV;
}

static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

float SoftKnee(float x, float x0, float k)
{
    // x0 = knee start (e.g. 0.8), k = knee width (e.g. 0.2)
    // Maps [x0, x0+k] smoothly into a slope change toward 1.0..peak
    float t = saturate((x - x0) / max(k, 1e-6));
    // cubic ease for continuity of slope at x0
    return lerp(x, x0 + k * (1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t)), t);
}

float3 ToneMap_HDR_scRGB_Soft(float3 c, float peakLin, float kneeStart, float kneeWidth)
{
    float Y = max(dot(c, LUMA), 1e-6);
    // First do a soft knee around paper white:
    float Yk = SoftKnee(Y, kneeStart, kneeWidth); // e.g., kneeStart=0.85, kneeWidth=0.35
    // Then the classic highlight knee to the display peak:
    float over = max(Yk - 1.0, 0.0);
    float k = max(peakLin - 1.0, 1e-6);
    float Yt = min(Yk, 1.0) + over / (1.0 + over / k);
    return c * (Yt / Y);
}

float MaxEVAllowedAtAltitude(float sunAltDeg)
{
    // t = 0 at -2°, 1 at +2°
    float t = saturate((sunAltDeg + 2.0) / 4.0);
    // At night (t=0), cap positive EV at 0.0; by day (t=1), effectively "no cap".
    // Use a large value for the day cap to avoid affecting daytime EV.
    return lerp(0.0, 100.0, t); // 100 stops is effectively unbounded
}

// Compute sun altitude (degrees) from camera "up" and TO-sun direction
float SunAltitudeDeg(float3 camPosWS, float3 planetCenterWS, float3 lightDirFromLight)
{
    float3 wSun = -normalize(lightDirFromLight); // TO sun
    float3 up = normalize(camPosWS - planetCenterWS); // radial up at camera
    float mu = clamp(dot(up, wSun), -1.0, 1.0);
    return degrees(asin(mu)); // +90 at zenith, 0 at horizon, negative at night
}

// Scalar smoothstep
float sstep(float a, float b, float x)
{
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0 - 2.0 * t);
}

// Night bias curve (stops) vs sun altitude: 0 @ day → down to ~-6 @ -18°
float EVBiasFromSunAltitude(float sunAltDeg)
{
    const float EV_atDay = 0.0; // ≥ +2°
    const float EV_atHorizon = -1.0; //   0°
    const float EV_atCivil = -3.0; //  −6°
    const float EV_atNautical = -4.5; // −12°
    const float EV_atAstro = -6.0; // ≤ −18°

    // chain smooth segments
    if (sunAltDeg >= 0.0)
        return lerp(EV_atDay, EV_atHorizon, sstep(+2.0, 0.0, sunAltDeg));
    if (sunAltDeg >= -6.0)
        return lerp(EV_atHorizon, EV_atCivil, sstep(0.0, -6.0, sunAltDeg));
    if (sunAltDeg >= -12.0)
        return lerp(EV_atCivil, EV_atNautical, sstep(-6.0, -12.0, sunAltDeg));
    return lerp(EV_atNautical, EV_atAstro, sstep(-12.0, -18.0, sunAltDeg));
}

// Ambient light term (fade day→night with a pre-twilight shoulder)
float3 ComputeAmbientLight()
{
    float sunAltDeg = SunAltitudeDeg(cameraPosition.xyz, PlanetCenterWS, direction.xyz);
    
    // Base fade: 0 at/above startDeg → 1 by endDeg (e.g. +2° → −6°)
    float f = sstep(DayFadeStartDeg, DayFadeEndDeg, sunAltDeg);

    // Keep some ambient exactly at the horizon so you don't dip to black
    // Apply only within a small band around 0° to avoid affecting midday/night
    const float MinAtHorizon = 0.15; // 0..1 of target ambient at 0°
    float nearH = 1.0 - sstep(-2.0, +2.0, abs(sunAltDeg));
    f = saturate(lerp(f, max(f, MinAtHorizon), nearH));

    // Optional softening
    float fade = pow(f, 0.90);
    
    float3 nightAmbientPW = NightAmbient * fade;
    
    return nightAmbientPW;
}

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSIn input) : SV_Target
{
    float3 hdr = sceneHDR.Sample(LinearClamp, input.uv).rgb;
    
    // --- Compute sun altitude and the allowed EV cap (uniform across the frame)
    float sunAltDeg = SunAltitudeDeg(cameraPosition.xyz, PlanetCenterWS, direction.xyz);
    float maxEVAllow = MaxEVAllowedAtAltitude(sunAltDeg);
    float EVbias = EVBiasFromSunAltitude(sunAltDeg);
    
    // --- Clamp *positive* exposure at night
    float EVtotal = min(EV, maxEVAllow) + EVbias;
    
    // Apply exposure
    float3 color = max(hdr * exp2(EVtotal), 0.0.xxx);

     // Tone map (paper-white roll-off with soft knee)
    const float peakLinear = 2000.0f / 200.0f; // 10-bit HDR ~1000–2000 nits typical
    color = ToneMap_HDR_scRGB_Soft(color, peakLinear, 0.85f, 0.35f);
    
    // Add stars AFTER tonemap in display/scRGB
    float3 starsPW = stars.Sample(LinearClamp, input.uv).rgb; // your stars RT stores paper-white layer
    // If your stars RT is normalized (0..1), scale by StarNits/PW to convert to display units:
    float starGain = StarNits / max(200.0f, 1e-3);
    float3 starsColor = starsPW * starGain;
    
    float depth = SceneDepth.Sample(ClampPoint, input.uv);
    
    float3 postAmbient = float3(0.0f, 0.0f, 0.0f);
    if (depth > 1e-12f)
    {
        float ao = SSAO.Sample(LinearClamp, input.uv).r;
        postAmbient = ComputeAmbientLight() * ao;
    }

    color = color + starsColor + postAmbient;
    
    return float4(color, 1.0f);
}