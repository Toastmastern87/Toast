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

Texture2D sceneHDR                      : register(t9); // scene + bloom, linear HDR
Texture2D stars                         : register(t10); // StarsRT from step 1
Texture3D<float2> LocalExposureGrid     : register(t11);
StructuredBuffer<float4> SunVisibility  : register(t12);
Texture2D StarburstTex                  : register(t13);
Texture2D<float> SunDiscMask            : register(t14);
Texture2D<float> SunHaloMask            : register(t15);
StructuredBuffer<float> ExposureEV      : register(t17);

SamplerState LinearClamp                : register(s0);
SamplerState ClampPoint                 : register(s1);

#include "LocalExposure.hlsli"

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
    float4x4 lightViewProj[4];
    
    float4 direction; // FROM light -> scene
    
    float4 radiance; // RGB
    
    float SunIntensity;
    float DirectionalLightGain;
    uint CascadeCount;
    float ShadowDistance;
    
    float4 CascadeEnds;
    
    uint CascadeIndex;
    float ConstantBias;
    float SlopeBias;
};

cbuffer StarsParams : register(b7)
{
    float StarNits; // e.g. 600.0 (display-space peak for brightest texel)
    float TwilightStartDeg; // start appearing (e.g. 0.0)    
    float TwilightEndDeg; // fully visible by (e.g. -6.0)
    float SpaceFadeStart; // altitude norm where space visibility starts (0..1), e.g. 0.85
    
    float SpaceFadeEnd; // fully visible by (0..1), e.g. 0.98    
    float3 _pad0;
}

cbuffer Tonemapping : register(b10)
{    
    float StarburstSize; // Sprite diameter as a fraction of the screen heigh
    float StarburstIntensity;
    float GhostIntensity;
    float GhostScale;
    
    float LocalHighlightContrast; 
    float LocalShadowContrast; 
    float LocalDetailStrength; 
    float _pad1;
}

static const float PI = 3.14159265358979323846f;
static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);
static const float AGX_LOOK_POWER = 1.15f;
static const float AGX_LOOK_SATURATION = 1.1f;
static const uint GHOST_COUNT = 4;
static const float GHOST_POSITION[GHOST_COUNT] = { 0.55f, 0.90f, 1.35f, 1.80f };
static const float GHOST_SIZE[GHOST_COUNT] = { 0.025f, 0.045f, 0.070f, 0.030f };
static const float3 GHOST_COLOR[GHOST_COUNT] =
{
    float3(0.0300f, 0.0216f, 0.0126f), // amber
    float3(0.0076f, 0.0153f, 0.0162f), // teal
    float3(0.0086f, 0.0066f, 0.0120f), // violet
    float3(0.0132f, 0.0240f, 0.0144f) // green
};
static const float3 GHOST_CHROMA = float3(1.02f, 1.0f, 0.98f);

float3 AgXContrastApprox(float3 x)
{
    // 6th-order polynomial fit of AgX's default sigmoid (mean error^2 3.67e-6)
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return 15.5f * x4 * x2
         - 40.14f * x4 * x
         + 31.96f * x4
         - 6.868f * x2 * x
         + 0.4298f * x2
         + 0.1191f * x
         - 0.00232f;
}

float3 AgXLook(float3 v, float power, float saturation)
{
    float luma = dot(v, LUMA);
    v = pow(max(v, 0.0f), power);
    return luma + saturation * (v - luma);
}

float3 ToneMapAgX(float3 c)
{
    // "Inset": pulls the primaries slightly toward white, so saturated colours desaturate
    // smoothly as they brighten instead of skewing hue (rows; mul(M, v)).
    const float3x3 AgXInset = float3x3(
        0.842479062253094f, 0.0784335999999992f, 0.0792237451477643f,
        0.0423282422610123f, 0.878468636469772f, 0.0791661274605434f,
        0.0423756549057051f, 0.0784336f, 0.879142973793104f);
    // "Outset": the inverse, applied after the curve
    const float3x3 AgXOutset = float3x3(
        1.19687900512017f, -0.0980208811401368f, -0.0990297440797205f,
        -0.0528968517574562f, 1.15190312990417f, -0.0989611768448433f,
        -0.0529716355144438f, -0.0980434501171241f, 1.15107367264116f);
    const float minEV = -12.47393f;
    const float maxEV = 4.026069f;

    float3 v = mul(AgXInset, max(c, 0.0f));
    v = clamp(log2(max(v, 1e-10f)), minEV, maxEV); // log2 space, 16.5 stops
    v = (v - minEV) / (maxEV - minEV);
    v = AgXContrastApprox(v); // display-encoded (gamma 2.2)
    v = AgXLook(v, AGX_LOOK_POWER, AGX_LOOK_SATURATION); 
    v = mul(AgXOutset, v);
    return pow(saturate(v), 2.2f);
}

float StarburstN(float angle, int spikes, float sharpness)
{
    // spikes=4 -> cos(2a); spikes=6 -> cos(3a); spikes=8 -> cos(4a)
    float k = 0.5f * spikes;
    float s = abs(cos(k * angle));
    return pow(s, sharpness);
}

// Smooth radial falloff for lens effects (r in UV space)
float RadialFalloff(float r, float r0, float power)
{
    // r0 = radius where it starts fading; power controls tail
    float t = saturate(r / max(r0, 1e-6f));
    return pow(1.0f - t, power);
}

float3 Starburst(float2 pixel, float2 sunPixel)
{
    const float sizePx = max(StarburstSize * viewportHeight, 1.0f);
    const float2 uv = (pixel - sunPixel) / sizePx + 0.5f;
    if (any(uv < 0.0f) || any(uv > 1.0f))
        return 0.0.xxx;

    const float3 enc = StarburstTex.SampleLevel(LinearClamp, uv, 0).rgb;
    return (enc * enc) * (enc * enc);
}

float HexagonSDF(float2 p, float R)
{
    const float sector = PI / 3.0f;
    const float a = atan2(p.y, p.x);
    const float local = a - sector * round(a / sector);
    return length(p) * cos(local) - R * 0.8660254f;
}

float GhostShape(float d, float R)
{
    const float soft = 0.04f * R;
    const float body = saturate(0.5f - d / soft);
    const float rimWidth = 0.06f * R;
    const float rim = (d < soft) ? 0.6f * exp(-(d * d) / (rimWidth * rimWidth)) : 0.0f;
    return 0.6f * body + rim;
}

float3 LensGhosts(float2 pixel, float2 sunPixel, float2 centerPixel)
{
    float3 sum = 0.0.xxx;
    
    [unroll]
    for (uint i = 0; i < GHOST_COUNT; ++i)
    {
        const float2 ghostPixel = sunPixel + (centerPixel - sunPixel) * GHOST_POSITION[i];
        const float R = GHOST_SIZE[i] * GhostScale * viewportHeight;
        const float d = HexagonSDF(pixel - ghostPixel, R);

        const float3 dc = d + R * 0.8660254f * (1.0f - GHOST_CHROMA);
        sum += GHOST_COLOR[i] * float3(GhostShape(dc.r, R * GHOST_CHROMA.r), GhostShape(dc.g, R), GhostShape(dc.b, R * GHOST_CHROMA.b));
    }
    
    return sum;
}

float3 ApplyLocalExposure(float3 colorLinear, float2 uv)
{
    const float lum = max(dot(colorLinear, LUMA), 1e-8f);
    const float logLum = clamp(log2(lum), LE_LOG_MIN, LE_LOG_MAX);

    const float2 grid = LocalExposureGrid.SampleLevel(LinearClamp, float3(uv, LocalExposureBin01(logLum)), 0);
    if (grid.y < 1e-4f)
        return colorLinear;

    const float base = grid.x / grid.y;
    const float middleGrey = log2(0.18f);
    const float delta = base - middleGrey;
    const float contrast = (delta > 0.0f) ? LocalHighlightContrast : LocalShadowContrast;
    const float newLog = middleGrey + delta * contrast + (logLum - base) * LocalDetailStrength;

    return colorLinear * exp2(newLog - logLum);

}

float Bayer8x8(uint2 p)
{
    // 8x8 Bayer matrix normalized to [0,1)
    static const uint B[64] =
    {
        0, 32, 8, 40, 2, 34, 10, 42,
        48, 16, 56, 24, 50, 18, 58, 26,
        12, 44, 4, 36, 14, 46, 6, 38,
        60, 28, 52, 20, 62, 30, 54, 22,
        3, 35, 11, 43, 1, 33, 9, 41,
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47, 7, 39, 13, 45, 5, 37,
        63, 31, 55, 23, 61, 29, 53, 21
    };
    uint idx = (p.x & 7u) + ((p.y & 7u) << 3);
    return (B[idx] + 0.5f) / 64.0f;
}

// ===== Pixel shader ==========================================================
struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PSOut
{
    float4 hdr : SV_Target0; // for your HDR swapchain (R16G16B16A16_FLOAT)
    float4 sdr : SV_Target1; // for ImGui viewport (R8G8B8A8_UNORM_SRGB recommended)
};

PSOut main(PSIn input)
{
    PSOut output;
    
    // Sample inputs
    float3 hdr = sceneHDR.Sample(LinearClamp, input.uv).rgb;        
    
    // ---------- Exposure: adapted EV from ExposureAdapt -------------
    float EVCam = ExposureEV[0];

    // ---------- Apply exposure + tonemap -------------------------------------
    float3 colorLinear = max(hdr * exp2(-EVCam), 0.0.xxx);
    colorLinear = ApplyLocalExposure(colorLinear, input.uv);

    // ---------------- Lens effects: starburst and ghosts ----------------
    float4 sunLight = SunVisibility[0];
    float4 sunCenter = SunVisibility[1];
    float3 lensAdd = 0.0.xxx;
    if (sunCenter.w > 0.5f)
    {
        const float2 viewport = float2(viewportWidth, viewportHeight);
        const float2 pixel = input.uv * viewport;
        const float2 sunPixel = sunCenter.yz * viewport;
        const float3 sunAtLens = radiance.rgb * sunLight.rgb * sunCenter.x;
        lensAdd = sunAtLens * (Starburst(pixel, sunPixel) * StarburstIntensity + LensGhosts(pixel, sunPixel, 0.5f * viewport) * GhostIntensity);
    }
    
    // Add to colorLinear BEFORE tonemap:
    colorLinear += lensAdd;
    
    // Tone map to scRGB paper-white space
    const float peakLinear = 2000.0f / 200.0f; // assuming PW=200 nits
    float3 colorHDR = ToneMapAgX(colorLinear);

    // --- NEW: local tonemap bypass for sun (disc + halo)
    
    float discMask = saturate(SunDiscMask.Sample(LinearClamp, input.uv));
    float haloMask = saturate(SunHaloMask.Sample(LinearClamp, input.uv));
    
    // optional: tame the very faint halo from fully bypassing
    float haloTame = saturate(haloMask * 1.25f - 0.05f); // small threshold + gain
    float occ = saturate(discMask * 1.0f + haloTame * 0.85f);

    // ---------- Stars (added in display space), fade by camera twilight ------
    float3 starsPW = stars.Sample(LinearClamp, input.uv).rgb;
    float starGain = StarNits / max(200.0f, 1e-3);
    float3 starsHDR = starsPW * starGain * (1.0 - occ);

    float3 hdrOut = colorHDR + starsHDR;
    output.hdr = float4(hdrOut, 1.0);

    // ------------- HDR output (your current path) -------------
    float3 sdrDisp = colorHDR;

    float n = Bayer8x8((uint2) input.pos.xy);
    float amp = 0.25f; // ordered can run a bit higher, but start low
    float d = (n - 0.5f) * (2.0f * amp / 255.0f);

    // Combine with luma-only rescale (best result)
    float Y = max(dot(sdrDisp, LUMA), 1e-6f);
    float Y2 = saturate(Y + d);
    sdrDisp *= (Y2 / Y);

    output.sdr = float4(saturate(sdrDisp), 1.0f);

    float starGainSDR = StarNits / max(100.0f, 1e-3);
    float3 starsSDR = starsPW * starGainSDR * (1.0 - occ);

    sdrDisp += starsSDR;
    output.sdr = float4(sdrDisp, 1.0);

    return output;
}
