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

Texture2D sceneHDR                  : register(t9); // scene + bloom, linear HDR
Texture2D stars                     : register(t10); // StarsRT from step 1
Texture2D<float> SceneDepth         : register(t12);
Texture2D<float> SunDiscMask        : register(t14);
Texture2D<float> SunHaloMask        : register(t15);
Texture2D scenePreBloomHDR          : register(t16);
StructuredBuffer<float> ExposureEV  : register(t17);

SamplerState LinearClamp        : register(s0);
SamplerState ClampPoint         : register(s1);

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
    
    float3 SunsetTint;
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
    float2 SunUV; // [0..1] screen UV of sun center
    float SpaceFactor; // 0=surface, 1=space (planet->GetSpaceFactor)
    float LensStrength; // overall multiplier (e.g. 1.0)
    
    uint SunSpikes; // number of diffraction spikes (4,6,8)
    float SunSpikeSharpness; // higher=thinner (e.g. 24.0)
    float SunSpikeRadiusSurface;
    float SunSpikeRadiusSpace;
    
    float SunSpikeFallOff; // how quickly spikes fade with altitude
    float SunSpikeStrengthSurface;
    float SunSpikeStrengthSpace;
    float SunGlareStrengthSurface;
    
    float SunGlareStrengthSpace;
    float SunGlareRadiusSurface;
    float SunGlareRadiusSpace;
    float LensAltStart; // altitude norm where lens effects start (0..1), e.g. 0.5
    
    float LensAltEnd; // fully on by (0..1), e.g. 0.8
    float GhostStrength;
    float GhostSpacing;
    float GhostFalloff;
    
    float GhostSizeSurface; 
    float GhostSizeSpace; 
    float GhostAirSuppression; 
    float _pad1;
}

static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

float IsSkyDepth(float d)  // reversed-Z: sky is ~0
{
    return (d <= 1e-12f) ? 1.0f : 0.0f;
}

bool SunUVOnScreen(float2 uv)
{
    // small margin to avoid edge tap clamping
    const float m = 2.0f; // pixels
    float2 texel = float2(1.0 / viewportWidth, 1.0 / viewportHeight);
    float2 lo = 0.0.xx + m * texel;
    float2 hi = 1.0.xx - m * texel;
    return all(uv >= lo) && all(uv <= hi);
}

// Returns 0..1 where 1 means "sun is visible in screen space"
float SunVisibilityKernel(float2 sunUV, int radius)
{
    float2 texel = float2(1.0 / viewportWidth, 1.0 / viewportHeight);
    float v = 0.0;
    int count = 0;

    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            float2 uv = clamp(sunUV + float2(x, y) * texel * 2.0f, 0.0.xx, 1.0.xx);
            float d = SceneDepth.SampleLevel(ClampPoint, uv, 0);
            v += IsSkyDepth(d);
            count++;
        }
    }
    return v / max(count, 1);
}

// ---------- Sun altitude (deg) from a position (camera or pixel) ------------
float SunAltitudeDegAt(float3 posWS, float3 planetCenterWS, float3 lightDirFromLight)
{
    float3 wSun = -normalize(lightDirFromLight); // TO sun
    float3 up = normalize(posWS - planetCenterWS);
    float mu = clamp(dot(up, wSun), -1.0f, 1.0f);
    return degrees(asin(mu)); // +90 zenith, 0 horizon, negative at night
}

// Horizon dip (deg) for a viewer at distance r from center, occluder radius R
float HorizonDipDeg(float r, float R)
{
    return degrees(acos(saturate(R / max(r, R + 1e-6f))));
}

// Effective sun altitude for the *camera* (adds horizon dip + small refraction)
float SunAltDegEffectiveCamera(float3 camWS, float3 planetCenterWS, float planetRadius, float3 lightDirFromLight)
{
    float3 rel = camWS - planetCenterWS;
    float r = max(planetRadius, length(rel));
    
    float alt = SunAltitudeDegAt(camWS, planetCenterWS, lightDirFromLight);
    float dip = HorizonDipDeg(r, planetRadius);
    // add ~0.83° refraction so disc remains “above” a touch at sunset
    return alt + dip + 0.83;
}

float SoftKnee(float x, float x0, float k)
{
    float t = saturate((x - x0) / max(k, 1e-6f));
    return lerp(x, x0 + k * (1.0f - t) * (1.0f - t) * (1.0f - t), t);
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

float3 ToneMapACESFitted(float3 x)
{
    x = max(x, 0.0f);
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
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

float3 Ghosts(float2 uv, float2 sunUV, float3 sunLin, float fSpace)
{
    float2 c = float2(0.5f, 0.5f);
    float2 g = (c - sunUV) * GhostSpacing;

    float3 acc = 0.0.xxx;

    // Tune positions/intensities
    const int N = 4;
    float kBase[N] = { 0.35f, 0.60f, 0.90f, 1.25f };
    float wBase[N] = { 0.25f, 0.18f, 0.12f, 0.08f };
    
    float s = lerp(GhostSizeSurface, GhostSizeSpace, fSpace); // NEW: size knobs
    s = max(s, 1e-6f);

    [unroll]
    for (int i = 0; i < N; ++i)
    {
        float2 p = sunUV + g * kBase[i];
        float2 d = uv - p;
        float rr = dot(d, d);

        // soft disc ghost

        float ghost = exp(-rr / s);
        
        float w = wBase[i] * pow(GhostFalloff, (float) i);

        acc += w * ghost * sunLin;
    }

    // Reduce in air
    acc *= lerp(GhostAirSuppression, 1.0f, fSpace);
    return acc;
}

float OffscreenFade(float2 uv, float2 fadePx)
{
    // fadePx in pixels (e.g. 32..128)
    float2 texel = float2(1.0 / viewportWidth, 1.0 / viewportHeight);
    float2 m = fadePx * texel;

    // distance to each edge (positive when inside)
    float2 d0 = uv - 0.0.xx;
    float2 d1 = 1.0.xx - uv;

    // inside distance to nearest edge
    float2 din = min(d0, d1);

    // fade from 0 at -m (outside by margin) to 1 at 0 (on edge) to 1 inside
    float2 f = saturate((din + m) / m);

    // use min so leaving in either axis fades out
    return min(f.x, f.y);
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

// ---------- Camera altitude and sun altitude (lens effects only) ----------
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    
    float rCam = length(cameraPosition.xyz - PlanetCenterWS);
    float altFrac = saturate((rCam - RbPhys) / max(1e-3f, Rt - RbPhys));
    
    float sunAltCam = SunAltDegEffectiveCamera(cameraPosition.xyz, PlanetCenterWS, PlanetRadius, direction.xyz);
    
    float edgeFade = OffscreenFade(SunUV, float2(160.0, 160.0)); // tweak 64..160 px
    float sunVis3 = SunVisibilityKernel(clamp(SunUV, 0.0.xx, 1.0.xx), 1); // 3x3
    sunVis3 *= edgeFade;
    
    // ---------- Exposure: adapted EV from ExposureAdapt -------------
    float EVCam = ExposureEV[0];

    // ---------- Apply exposure + tonemap -------------------------------------
    float3 colorLinear = max(hdr * exp2(-EVCam), 0.0.xxx);
       
    float2 dUV = input.uv - SunUV;
    float r = length(dUV);

    // Gate: only in space, and only if sun is visible
    float fSpace = saturate(SpaceFactor);
    
    float lensGate = smoothstep(LensAltStart, LensAltEnd, altFrac); // much wider transition
    float vis = sunVis3 * LensStrength * lensGate;

    // Optional: also gate by your disc/halo masks so it doesn't appear when sun is off-screen
    // (disc/halo masks are per-pixel; use a tiny requirement that the sun is “present” in the sky)
    float sunPresent = saturate(SunDiscMask.SampleLevel(ClampPoint, clamp(SunUV, 0.0.xx, 1.0.xx), 0));
    sunPresent *= edgeFade;
    float haloPresent = saturate(SunHaloMask.SampleLevel(ClampPoint, clamp(SunUV, 0.0.xx, 1.0.xx), 0));
    float present = max(sunPresent, haloPresent);
    vis *= smoothstep(0.05f, 0.20f, present);
    
    // Sun altitude in degrees
    // -1° = fully hidden
    // +5° = fully visible
    float LensArticaftsVis = smoothstep(-1.0f, 5.0f, sunAltCam);

    // Shape the growth
    LensArticaftsVis = pow(LensArticaftsVis, 1.8f);

    // ---------------- Veiling glare (large wash) ----------------
    float glareStrength = lerp(SunGlareStrengthSurface, SunGlareStrengthSpace, fSpace); 
    float glareRadius = lerp(SunGlareRadiusSurface, SunGlareRadiusSpace, fSpace);
    float glare = exp(-(r * r) / max(glareRadius * glareRadius, 1e-6f));
    float3 veiling = glareStrength * glare.xxx;
    veiling *= LensArticaftsVis;

    // ---------------- Diffraction spikes (starburst) ----------------
    float angle = atan2(dUV.y, dUV.x);
    float burst = StarburstN(angle, SunSpikes, SunSpikeSharpness);

    // Spike length control: fade with radius
    float spikeSpace = pow(fSpace, 0.35f);
    float spikeRadius = lerp(SunSpikeRadiusSurface, SunSpikeRadiusSpace, spikeSpace); // longer in space
    float spikeFall = RadialFalloff(r, spikeRadius, SunSpikeFallOff);
    float spikeStrength = lerp(SunSpikeStrengthSurface, SunSpikeStrengthSpace, spikeSpace); // essentially off in air
    
    float spikesTerm = spikeStrength * burst * spikeFall;
    spikesTerm *= LensArticaftsVis;
    
    // ---------------- Lens Glare ----------------
    float3 sunSample = scenePreBloomHDR.SampleLevel(LinearClamp, SunUV, 0).rgb;
    sunSample = max(sunSample * exp2(-EVCam), 0.0.xxx);

    // Normalize to avoid insane tinting when saturated
    float Ysun = max(dot(sunSample, LUMA), 1e-6f);
    float3 sunChroma = sunSample / Ysun;
    float Ylens = min(Ysun, 8.0f);

    // Lens artifacts are typically “whiter” than the source; blend toward white
    float3 lensTint = lerp(1.0.xxx, sunChroma, 0.35f);
    
    float3 ghostAdd = Ghosts(input.uv, SunUV, sunSample, fSpace) * lensGate;

    // Final lens contribution in exposure-linear space:
    float3 lensAdd = vis * (veiling + spikesTerm.xxx) * lensTint * Ylens;

    // Add to colorLinear BEFORE tonemap:
    colorLinear += lensAdd;
    
    colorLinear += vis * GhostStrength * ghostAdd * LensArticaftsVis;
    
    // Tone map to scRGB paper-white space
    const float peakLinear = 2000.0f / 200.0f; // assuming PW=200 nits
    float3 colorHDR = ToneMap_HDR_scRGB_Soft(colorLinear, peakLinear, 0.85f, 0.35f);

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
    float3 sdrDisp = ToneMapACESFitted(colorLinear);

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
