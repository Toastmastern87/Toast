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

Texture2D sceneHDR : register(t9); // scene + bloom, linear HDR
Texture2D stars : register(t10); // StarsRT from step 1
Texture2D SSAO : register(t11);
Texture2D<float> SceneDepth : register(t12);
Texture2D NormalMap : register(t13);
Texture2D<float> SunDiscMask : register(t14);
Texture2D<float> SunHaloMask : register(t15);

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

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight; // Rt - Rg
    float RayScaleHeight; // Hr
    float MieScaleHeight; // Hm
    float MieAnisotropy; // g
    float3 RayleighScattering; // beta_R (1/m) RGB
    float3 MieScattering; // beta_Ms (1/m) RGB
    float3 MieAbsorption; // beta_Ma (1/m) RGB
    float3 GroundAlbedo;
    float OzoneStrength;
    uint StepsTransmittance; // (unused here)
    uint StepsMultiScattering; // (unused here)
    float APFarDynamic; // camera->max distance for AP (meters)
};

cbuffer StarsParams : register(b7)
{
    float StarNits; // e.g. 600.0 (display-space peak for brightest texel)
    float TwilightStartDeg; // start appearing (e.g. 0.0)    
    float TwilightEndDeg; // fully visible by (e.g. -6.0)
    float SpaceFadeStart; // altitude norm where space visibility starts (0..1), e.g. 0.85
    
    float SpaceFadeEnd; // fully visible by (0..1), e.g. 0.98      
    float3 NightAmbient;
}

cbuffer Tonemapping : register(b10)
{
    float EVGeometrySurface;
    float EVGeometrySpace;
    float EVGeometryNight;
    float EVSkySurface;
    
    float EVSkySpace;
    float EVSkySurfaceNight;
    float EVSkySpaceNight;
    float AltFadeStartFrac;
    
    float AltFadeEndFrac;
    float SunFadeStartDeg;
    float SunFadeEndDeg;
}

static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

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

float ArcDistanceOnGround(float3 pWS, float3 camWS, float3 planetCenter, float Rg)
{
    float3 upP = normalize(pWS - planetCenter);
    float3 upC = normalize(camWS - planetCenter);
    float cosA = saturate(dot(upP, upC));
    float ang = acos(cosA); // radians
    return Rg * ang; // meters along the surface
}

// Night EV bias vs sun altitude (gentle curve that ends at -1.07 at deep night)
float EVBiasFromSunAltitude(float sunAltDeg)
{
    // Targets at key twilight bands (tuned to keep smooth progression to -1.07)
    const float EV_atDay = 0.00f; // ≥ +2°
    const float EV_atHorizon = -1.26f; //   0°
    const float EV_atCivil = -2.02f; //  −6°
    const float EV_atNautical = -2.69f; // −12°
    const float EV_atAstro = -3.37f; // ≤ −18°

    if (sunAltDeg >= 0.0f)
        return lerp(EV_atDay, EV_atHorizon, sstep(+2.0f, 0.0f, sunAltDeg));
    if (sunAltDeg >= -6.0f)
        return lerp(EV_atHorizon, EV_atCivil, sstep(0.0f, -6.0f, sunAltDeg));
    if (sunAltDeg >= -12.0f)
        return lerp(EV_atCivil, EV_atNautical, sstep(-6.0f, -12.0f, sunAltDeg));
    return lerp(EV_atNautical, EV_atAstro, sstep(-12.0f, -18.0f, sunAltDeg));
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

    // Planet center in world space
    float3 planetCenterTrueWS = mul(float4(PlanetCenterWS, 1.0f), worldTranslationMatrix).xyz;

    // ---------- EXPOSURE SELECTION (ONLY EV LERP CHANGES) --------------------
    // Altitude fraction (0..1) using RbPhys as base
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    float rCam = length(cameraPosition.xyz - planetCenterTrueWS);
    float altFrac = saturate((rCam - RbPhys) / max(1e-3f, Rt - RbPhys));

    const float3 wSun = -normalize(direction.xyz); // TO sun
    
    // Surface <-> Space blend
    float a = sstep(AltFadeStartFrac, AltFadeEndFrac, altFrac);

    // Day EVs by altitude
    float EVGeomDay = lerp(EVGeometrySurface, EVGeometrySpace, a);
    float EVSkyDay = lerp(EVSkySurface, EVSkySpace, a);

    // Night blend by sun altitude at camera (effective)
    float sunAltCamEff = SunAltDegEffectiveCamera(cameraPosition.xyz, mul(float4(planetCenterTrueWS, 1.0f), worldTranslationMatrix).xyz, PlanetRadius, direction.xyz);

    // n = 0 (day) above SunFadeStartDeg, n = 1 (night) below SunFadeEndDeg
    float n = 1.0 - sstep(SunFadeEndDeg, SunFadeStartDeg, sunAltCamEff);

    // Night EV for SKY now depends on altitude (surface vs space)
    float EVSkyNightByAlt = lerp(EVSkySurfaceNight, EVSkySpaceNight, a);
    
    // Final EVs
    float EVGeomFinal = lerp(EVGeomDay, EVGeometryNight, n);
    float EVSkyFinal = lerp(EVSkyDay, EVSkyNightByAlt, n);

    // Pick per-pixel target based on depth (geometry vs sky)
    float EVTarget = (depth > 1e-12f) ? EVGeomFinal : EVSkyFinal;

    // ------------------------------------------------------------------------

    // Geometry-only extras (unchanged)
    if (depth > 1e-12f)
    {
        // ---------- Reconstruct per-pixel data ----------
        float3 pWS = ReconstructWorldPos(input.uv, depth); // world pos
        float3 up_px = normalize(pWS - planetCenterTrueWS); // radial up

        // Per-pixel sun altitude (deg)
        float sunAlt_px = SunAltitudeDeg_at(pWS, planetCenterTrueWS, direction.xyz);
        
        float EVTerrain = EVBiasFromSunAltitude(sunAlt_px) + EVTarget;

        // Night test: is planet between pixel and sun? (nightside terrain mask)
        float t0, t1;
        bool hit = RayHitsSphereBetween(pWS - planetCenterTrueWS, wSun, PlanetRadius, t0, t1);
        bool nightAtPx = hit && (t1 > 0.0);
        
        float mu = dot(up_px, wSun);
        float theta = acos(saturate(mu)); // 0..π (radians)

        const float Rt = PlanetRadius + AtmosphereHeight;
        const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
        
        // Blend width: wide near ground, narrow above the air
        float alt01 = saturate((length(planetCenterTrueWS) - RbPhys) / AtmosphereHeight); // 0=surface,1=TOA+
        float softDeg = lerp(6.0f, 0.8f, alt01); // tune: 6° band at ground → 0.8° in space

        // Build a smooth 0→1 mask centered on the horizon
        float start = radians(90.0f - 0.5f * softDeg);
        float end = radians(90.0f + 0.5f * softDeg);
        float nightMask = smoothstep(start, end, theta);
         
        // --- add a twilight ramp based on *pixel* sun altitude ---
        float nightnessAlt = sstep(+0.5f, -6.0f, sunAlt_px); // 0 at day → ~1 by nautical night
        
        // Final weight to use for per-pixel EV blend:
        float nightness = saturate(max(nightMask, nightnessAlt));
        
        EVTarget = lerp(EVTarget, EVTerrain, nightness);

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

        float3 amb_px = NightAmbient * hemi * ao * wNight_px * nightMask;
        
        // --- Night "eye adaptation" bubble (simple distance fade) ---
        const float EyeRadiusMeters = 10000.0f; // full effect inside 1 km
        const float EyeFadeMeters = 120000.0f; // fade over next 12 km (make this big)
        float d = distance(pWS, cameraPosition.xyz);

        // 0..1 fade after the inner radius
        float t = saturate((d - EyeRadiusMeters) / EyeFadeMeters);

        // softer, longer tail than smoothstep
        float wEye = (1.0 - t) * (1.0 - t); // == pow(1 - t, 2)

        // apply
        amb_px *= wEye;
        
        // Add to your accumulator that you later add to hdr
        hdr += amb_px;
    }

    // ---------- Apply exposure + tonemap -------------------------------------
    float3 colorLinear = max(hdr * exp2(EVTarget), 0.0.xxx);

    // Tone map to scRGB paper-white space
    const float peakLinear = 2000.0f / 200.0f; // assuming PW=200 nits
    float3 color = ToneMap_HDR_scRGB_Soft(colorLinear, peakLinear, 0.85f, 0.35f);

    // --- NEW: local tonemap bypass for sun (disc + halo)
    float discMask = saturate(SunDiscMask.Sample(LinearClamp, input.uv));
    float haloMask = saturate(SunHaloMask.Sample(LinearClamp, input.uv));

    // overlap-safe OR to combine the masks
    float sunMask = saturate(discMask + haloMask - discMask * haloMask);
    
    // optional: tame the very faint halo from fully bypassing
    float haloTame = saturate(haloMask * 1.25 - 0.05); // small threshold + gain
    sunMask = saturate(discMask + haloTame - discMask * haloTame);

// clamp the linear path so bypass doesn’t exceed display peak
    float3 linClamped = min(hdr, peakLinear.xxx);

    // blend: 0 → tonemapped, 1 → linear-clamped (bypass)
    //color = lerp(color, linClamped, sunMask);

    // Hardcoded for now (your originals)
    const float DiscBoostNits = 350.0;
    const float DiscOccStrength = 1.0;
    const float HaloOccStrength = 0.85;

    float occ = saturate(discMask * DiscOccStrength + haloMask * HaloOccStrength);

    // ---------- Stars (added in display space), fade by camera twilight ------
    float3 starsPW = stars.Sample(LinearClamp, input.uv).rgb;
    float starGain = StarNits / max(200.0f, 1e-3);
    float3 starsColor = starsPW * starGain * (1.0 - occ);

    color += starsColor;

    return float4(color, 1.0f);
}
