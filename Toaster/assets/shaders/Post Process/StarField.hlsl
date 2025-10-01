#type pixel
#pragma pack_matrix(row_major)

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
    float4 direction; // FROM light -> scene
    float4 radiance;
    float SunIntensity;
};

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
    float StarNits; // keep if you premultiply here (we'll leave it as 1.0 in RT)
    float DayFadeStartDeg; // unused now
    float DayFadeEndDeg; // unused now
    float TwilightStartDeg; // e.g. 0.0
    float TwilightEndDeg; // e.g. -6.0
    float SpaceFadeStart; // 0.85
    float SpaceFadeEnd; // 0.98
    float GlareInnerDeg; // 5.0
    float GlareOuterDeg; // 12.0
    float3 NightAmbient;
}

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

TextureCube radianceTexture : register(t5); // starfield cubemap
Texture2D<float> SceneDepth : register(t9); // reversed-Z depth

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

static const float PI = 3.14159265359f;

float sstep(float a, float b, float x)
{
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0 - 2.0 * t);
}

float SunAltitudeDeg(float3 camPosWS, float3 planetCenterWS, float3 lightDirFromLight)
{
    float3 wSun = -normalize(lightDirFromLight); // TO sun
    float3 up = normalize(camPosWS - planetCenterWS);
    float mu = clamp(dot(up, wSun), -1.0, 1.0);
    return degrees(asin(mu)); // +90 zenith, 0 horizon, negative at night
}

float4 main(PixelInputType input) : SV_Target
{
    // Discard where geometry exists (reversed-Z: sky depth ~ 0)
    float depth = SceneDepth.Sample(PointSampler, input.texCoord);
    if (depth > 1e-12f)
        discard;

    // Reconstruct world ray
    float2 ndc = input.texCoord * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    float4 clipPos = float4(ndc, 0.0f, 1.0f);
    float4 viewPos = mul(clipPos, inverseProjectionMatrix);
    viewPos /= max(viewPos.w, 1e-6);
    float3 viewDir = normalize(viewPos.xyz);
    float3 worldDir = normalize(mul(viewDir, (float3x3) inverseViewMatrix));

    // Camera & planet
    float3 planetCenterWS = mul(float4(PlanetCentreVS, 1.0f), worldTranslationMatrix).xyz;
    float3 camWS = cameraPosition.xyz;
    float3 upCam = normalize(camWS - planetCenterWS);
    float rCam = length(camWS - planetCenterWS);
    float h = max(0.0, rCam - PlanetRadius);

    // Sun geometry
    float3 wSun = -normalize(direction.xyz); // TO sun
    float sunAlt = SunAltitudeDeg(camWS, planetCenterWS, direction.xyz);

    // ---------- VISIBILITY FACTORS ----------
    // Surface night factor: 0 at/above TwilightStartDeg, 1 by TwilightEndDeg (e.g. 0 → -6°)
    float surfaceNight = sstep(TwilightStartDeg, TwilightEndDeg, sunAlt);

    // Space visibility from altitude
    float spaceFadeStartAlt = PlanetRadius + SpaceFadeStart * AtmosphereHeight;
    float spaceFadeEndAlt = PlanetRadius + SpaceFadeEnd * AtmosphereHeight;
    float spaceVis = sstep(spaceFadeStartAlt, spaceFadeEndAlt, rCam);

    // Sun glare: reduce near sun
    float thetaSun = degrees(acos(clamp(dot(worldDir, wSun), -1.0, 1.0)));
    float glareCut = sstep(GlareInnerDeg, GlareOuterDeg, thetaSun);

    // Final visibility: surface night OR space, then apply horizon & glare
    float visibility = saturate(max(surfaceNight, spaceVis) * glareCut);

    // Sample stars (normalized 0..1)
    float3 starTex = radianceTexture.SampleLevel(LinearSampler, worldDir, 0.0f).rgb;

    // Output normalized stars; scale to nits later in tonemapper
    float3 outStars = starTex * visibility;

    return float4(outStars, 1.0);
}
