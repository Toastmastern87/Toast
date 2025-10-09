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

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    output.texCoord = float2((vID << 1) & 2, vID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1);

    return output;
}

#type pixel
#pragma pack_matrix(row_major)

Texture2D sceneHDR              : register(t9); // scene + bloom, linear HDR
Texture2D stars                 : register(t10); // StarsRT from step 1
Texture2D SSAO                  : register(t11);
Texture2D<float> SceneDepth     : register(t12);
Texture2D NormalMap             : register(t13);
Texture2D<float> SunDiscMask    : register(t14);
Texture2D<float> SunHaloMask    : register(t15
);

SamplerState LinearClamp : register(s1);
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

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius; // rad  (e.g. radians(0.2666))
    float SunEdgeSoftness; // rad  (soft rim width)
    int SunDiscToggle; // 0=off, 1=on
    float SpaceFactor; // 0=inside atmosphere, 1=space
    
    float3 SunDiscWhite;
    float SpaceDiscBrightnessScale; // unitless scale, e.g. 1.30
    
    float3 WarmTint; // e.g. float3(1.00, 0.92, 0.78)
    float AirHaloIntensity; // 0..~0.6 (was HaloStrength_Ground, e.g. 0.28)
    
    float AirHaloStartFrac; // 0..1   (was InAirStart, e.g. 0.15)
    float AirHaloFalloffPow; // curve (was InAirPow, e.g. 1.10)
    float HorizonRefractionDeg; // deg (was RefracCenterDeg, e.g. 0.83)
    float TwilightBlendDeg; // deg (was TwilightExtraDeg, e.g. 1.5)
    
    float HorizonSoftEdgeDeg; // deg (was LimbSoftDeg, e.g. 0.40)
    float SpaceHaloWidthDeg; // deg (was SpaceHaloSigmaDeg, e.g. 0.8)
    float SpaceHaloIntensity; // 0.01..0.10 (was SpaceHaloGain, e.g. 0.04)
    float SpaceHaloCutoffDeg; // deg (was SpaceHaloCutoffDeg, e.g. 6.0)
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
      
    float3 NightAmbient;
}

cbuffer Tonemapping : register(b10)
{
    float EV;
    float EVSceneOffset;
}

static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

float SoftKnee(float x, float x0, float k)
{
    float t = saturate((x - x0) / max(k, 1e-6));
    return lerp(x, x0 + k * (1.0 - t) * (1.0 - t) * (1.0 - t), t);
}

float3 ToneMap_HDR_scRGB_Soft(float3 c, float peakLin, float kneeStart, float kneeWidth)
{
    float Y = max(dot(c, LUMA), 1e-6);
    float Yk = SoftKnee(Y, kneeStart, kneeWidth);
    float over = max(Yk - 1.0, 0.0);
    float k = max(peakLin - 1.0, 1e-6);
    float Yt = min(Yk, 1.0) + over / (1.0 + over / k);
    return c * (Yt / Y);
}

float sstep(float a, float b, float x)
{
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0 - 2.0 * t);
}

// ---------- Sun altitude (deg) from a position (camera or pixel) ------------
float SunAltitudeDeg_at(float3 posWS, float3 planetCenterWS, float3 lightDirFromLight)
{
    float3 wSun = -normalize(lightDirFromLight); // TO sun
    float3 up = normalize(posWS - planetCenterWS);
    float mu = clamp(dot(up, wSun), -1.0, 1.0);
    return degrees(asin(mu)); // +90 zenith, 0 horizon, negative at night
}

// Horizon dip (deg) for a viewer at distance r from center, occluder radius R
float HorizonDipDeg(float r, float R)
{
    return degrees(acos(saturate(R / max(r, R + 1e-6))));
}

// Effective sun altitude for the *camera* (adds horizon dip + small refraction)
float SunAltDegEffectiveCamera(float3 camWS, float3 planetCenterWS, float planetRadius, float3 lightDirFromLight)
{
    float3 rel = camWS - planetCenterWS;
    float r = max(planetRadius, length(rel));
    float alt = SunAltitudeDeg_at(camWS, planetCenterWS, lightDirFromLight);
    float dip = HorizonDipDeg(r, planetRadius);
    // add ~0.83° refraction so disc remains “above” a touch at sunset
    return alt + dip + 0.83;
}

// Night EV bias vs sun altitude (gentle curve that ends at -1.53 at deep night)
float EVBiasFromSunAltitude(float sunAltDeg)
{
    // Targets at key twilight bands (tune to taste)
    const float EV_atDay = 0.0f; // ≥ +2°
    const float EV_atHorizon = -0.60f; //   0°
    const float EV_atCivil = -1.40f; //  −6°
    const float EV_atNautical = -2.10f; // −12°
    const float EV_atAstro = -2.83f; // ≤ −18°

    if (sunAltDeg >= 0.0f)
        return lerp(EV_atDay, EV_atHorizon, sstep(+2.0f, 0.0f, sunAltDeg));
    if (sunAltDeg >= -6.0f)
        return lerp(EV_atHorizon, EV_atCivil, sstep(0.0f, -6.0f, sunAltDeg));
    if (sunAltDeg >= -12.0f)
        return lerp(EV_atCivil, EV_atNautical, sstep(-6.0f, -12.0f, sunAltDeg));
    return lerp(EV_atNautical, EV_atAstro, sstep(-12.0f, -18.0f, sunAltDeg));
}

float3 ReconstructWorldPos(float2 uv, float depth01)
{
    // NDC: x,y in [-1,1], z stays in [0,1] for D3D
    float2 ndcXY = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 ndc = float4(ndcXY, depth01, 1.0f);

    // View space (row_major => mul(vector, matrix))
    float4 v = mul(ndc, inverseProjectionMatrix);
    v /= max(v.w, 1e-6);

    // World space
    float4 w = mul(float4(v.xyz, 1.0f), inverseViewMatrix);
    return w.xyz;
}

// Ray-sphere test (origin at point on/above surface, direction to sun)
bool RayHitsSphereBetween(float3 ro, float3 rd, float R, out float t0, out float t1)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - R * R;
    float h = b * b - c;
    if (h < 0.0)
    {
        t0 = t1 = 0.0;
        return false;
    }
    float s = sqrt(h);
    t0 = -b - s;
    t1 = -b + s;
    return true;
}

// Is the sun occluded from the *pixel* by the planet? (night-side terrain mask)
bool SunOccludedAtPoint(float3 pWS, float3 planetCenterWS, float planetRadius, float3 lightDirFromLight)
{
    float3 wSun = -normalize(lightDirFromLight); // TO sun
    float3 ro = pWS - planetCenterWS;
    float t0, t1;
    bool hit = RayHitsSphereBetween(ro, wSun, planetRadius, t0, t1);
    // If the forward intersection is in front of the point, the planet blocks the sun.
    return hit && (t1 > 0.0);
}

// Night ambient (per sun altitude) with a small horizon shoulder
float3 NightAmbientForAlt(float sunAltDeg, float3 nightAmbientBase)
{
    // Base fade: 0 at start → 1 by end (e.g. +2° → −6°) from your params
    float f = sstep(DayFadeStartDeg, DayFadeEndDeg, sunAltDeg);

    // Keep some ambient exactly at the horizon so you don't dip to black
    const float MinAtHorizon = 0.15;
    float nearH = 1.0 - sstep(-2.0, +2.0, abs(sunAltDeg));
    f = saturate(lerp(f, max(f, MinAtHorizon), nearH));

    float soft = pow(f, 0.90);
    return nightAmbientBase * soft;
}

// ----- SPACE OPTICAL VEIL (angle-based) -------------------------------------
float TanHalfFovY_FromProj(matrix P)
{
    return 1.0f / max(P._22, 1e-6);
}
float TanHalfFovX_FromProj(matrix P)
{
    return 1.0f / max(P._11, 1e-6);
}

float2 SunScreenUV(float3 camWS, float3 wSun, matrix V, matrix P)
{
    float3 pW = camWS + wSun * 1e7;
    float4 pV = mul(float4(pW, 1), V);
    float4 pC = mul(pV, P);
    float2 ndc = pC.xy / max(pC.w, 1e-6);
    ndc = ndc * 0.5f + 0.5f;
    ndc.y = 1.0f - ndc.y;
    return ndc;
}

float AngleDegToSun(float2 uv, float2 sunUV, float thfX, float thfY)
{
    float2 dNDC = (uv - sunUV) * 2.0;
    float2 t = float2(dNDC.x * thfX, dNDC.y * thfY);
    return degrees(atan(length(t)));
}

float VeilPSF(float angDeg, float innerDeg, float outerDeg)
{
    float lor = 1.0 / (1.0 + (angDeg / 18.0) * (angDeg / 18.0)); // long tail
    
    float ga = exp(-(angDeg * angDeg) / (2.0 * 6.0 * 6.0)); // mid halo
    float notch = saturate(angDeg / max(1e-3, innerDeg * 0.7)); // avoid white coin
    float gate = 1.0 - smoothstep(innerDeg * 0.35, outerDeg, angDeg);
    return (0.65 * lor + 0.45 * ga) * notch * gate;
}

float Streaks4(float2 uv, float2 sunUV) // optional, tiny cue
{
    float2 d = normalize(uv - sunUV + 1e-6);
    float s0 = pow(abs(d.x), 64.0); // 0/180
    float s1 = pow(abs(d.y), 64.0); // 90/270
    return s0 + 0.7 * s1;
}

float3 ViewDirWS_fromUV(float2 uv)
{
    float2 ndcXY = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);

    // Reconstruct two points along the view ray in *view* space
    float4 p0v = mul(float4(ndcXY, 0.0f, 1.0f), inverseProjectionMatrix);
    p0v /= max(p0v.w, 1e-6f);
    float4 p1v = mul(float4(ndcXY, 1.0, 1.0f), inverseProjectionMatrix);
    p1v /= max(p1v.w, 1e-6f);

    // To world space
    float3 P0 = mul(float4(p0v.xyz, 1.0f), inverseViewMatrix).xyz;
    float3 P1 = mul(float4(p1v.xyz, 1.0f), inverseViewMatrix).xyz;
    return normalize(P1 - P0);
}

float LimbWeightFromMu(float3 wView, float3 camRel)
{
    float3 upCam = normalize(camRel);
    float mu = dot(wView, upCam); // 0 near horizon, +1 zenith, -1 nadir

    // band around horizon: |mu| in [a,b] -> weight 1..0
    const float a = 0.02f; // ~= 1.1°
    const float b = 0.20f; // ~= 11°
    float t = saturate((b - abs(mu)) / max(b - a, 1e-4f));
    return smoothstep(0.0f, 1.0f, t);
}

// ===== Pixel shader ==========================================================

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSIn input) : SV_Target
{
    // Sample inputs
    float3 hdr = sceneHDR.Sample(LinearClamp, input.uv).rgb;
    float depth = SceneDepth.Sample(ClampPoint, input.uv);

    // Directions / constants
    const float3 wSun = -normalize(direction.xyz); // TO sun

    // ---------- CAMERA-DRIVEN EXPOSURE ---------------------------------------
    float sunAltCamEff = SunAltDegEffectiveCamera(cameraPosition.xyz, PlanetCenterWS, PlanetRadius, direction.xyz);
    float EVBias = EVBiasFromSunAltitude(sunAltCamEff);
    // This is the exposure we would use everywhere if we ignored per-pixel night:
    float EV_cam = EV + EVBias + EVSceneOffset;

    // Default to camera EV; override on night-side terrain
    float EV_final = EV_cam;
    
    float m_disc = pow(saturate(SunDiscMask.Sample(LinearClamp, input.uv)), 1.0f);
    float m_halo = saturate(SunHaloMask.Sample(LinearClamp, input.uv));
    
    if (depth > 1e-12f)
    {
        // ---------- Reconstruct per-pixel data ----------
        float3 pWS = ReconstructWorldPos(input.uv, depth); // world pos
        float3 up_px = normalize(pWS - PlanetCenterWS); // radial up
        float3 wSun = -normalize(direction.xyz); // TO sun

        // Per-pixel sun altitude (deg)
        float sunAlt_px = SunAltitudeDeg_at(pWS, PlanetCenterWS, direction.xyz);

        // Night test: is planet between pixel and sun? (nightside terrain mask)
        float t0, t1;
        bool hit = RayHitsSphereBetween(pWS - PlanetCenterWS, wSun, PlanetRadius, t0, t1);
        bool nightAtPx = hit && (t1 > 0.0);

        // Soften around horizon to avoid hard silhouettes
        float mu = dot(up_px, wSun); // <0 = night hemisphere
        float horizonSoft = 1.0 - sstep(-0.02, +0.02, mu); // ~±1.1°
        float nightMask = max(nightAtPx ? 1.0 : 0.0, horizonSoft * step(mu, 0.0));

        // AO (assume 1=open, 0=occluded; invert if yours is opposite)
        float ao = SSAO.Sample(LinearClamp, input.uv).r;

        // ---------- Tiny hemi ambient for night (linear HDR, pre-exposure) ----------
        // World normal from texture (0..1 → -1..1)
        float3 normal = NormalMap.Sample(LinearClamp, input.uv).rgb;
        normal = normalize(normal * 2.0f - 1.0f);
        float3 N_ws = normalize(mul(normal, (float3x3) inverseViewMatrix));

        // Hemi term: more for upward-facing
        float hemi01 = saturate(0.5f + 0.5f * dot(N_ws, up_px)); // 0..1
        float hemi = lerp(0.5f, 1.0f, hemi01); // 0.5..1
        const float HemiContrast = 0.6f; // 0=flat, 1=strong
        hemi = lerp(1.0f, hemi, HemiContrast);

        // Night weight vs pixel sun altitude (0 by day → ~1 by nautical)
        float wNight_px = sstep(+0.5f, -6.0f, sunAlt_px);

        const float3 NightAmbientRGB = float3(0.30f, 0.32f, 0.36f);
        float3 amb_px = NightAmbient * hemi * ao * wNight_px * nightMask;

        // Add to your accumulator that you later add to hdr
        hdr += amb_px;
    }
    
    // ---------- Apply exposure + tonemap -------------------------------------
    float3 color = max(hdr * exp2(EV_final), 0.0.xxx);

    // Tone map to scRGB paper-white space
    const float peakLinear = 2000.0f / 200.0f; // assuming PW=200 nits
    color = ToneMap_HDR_scRGB_Soft(color, peakLinear, 0.85f, 0.35f);
    
    // Hardcoded for now (tune live, move to cbuffer later):
    const float DiscBoostNits = 350.0; // 200–500 feels good
    const float DiscOccStrength = 1.0; // disc fully blocks stars
    const float HaloOccStrength = 0.85; // halo strongly dims stars
    
    float occ = saturate(m_disc * DiscOccStrength + m_halo * HaloOccStrength);
    
    // ---------- Stars (added in display space), fade by camera twilight ------
    float3 starsPW = stars.Sample(LinearClamp, input.uv).rgb;
    float starGain = StarNits / max(200.0f, 1e-3);
    float starDamp = saturate(1.0f * 0.9f); // reuse 'veil' (angle-based)
    float3 starsColor = starsPW * starGain * (1.0 - occ);

    // ---------- Compose ambient for terrain only -----------------------------
    color += starsColor;

    return float4(color, 1.0f);
}