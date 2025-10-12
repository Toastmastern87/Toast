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
    float TwilightStartDeg; // e.g. 0.0
    float TwilightEndDeg; // e.g. -6.0
    float SpaceFadeStart; // 0.85
    float SpaceFadeEnd; // 0.98
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

// Intersect a ray with a sphere of radius R centered at origin (planet space).
// Returns true if there is an intersection; t0 <= t1 on return.
bool RaySphere(float3 ro, float3 rd, float R, out float t0, out float t1)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - R * R;
    float h = b * b - c;
    if (h < 0.0)
    {
        t0 = 0.0;
        t1 = 0.0;
        return false;
    }
    float s = sqrt(h);
    t0 = -b - s;
    t1 = -b + s;
    return true;
}

// Returns whether the forward ray crosses the atmosphere shell [Rg, Rt],
// along with the tangent altitude h_tan (meters) and forward segment length L_shell (meters).
bool AtmosRayInfo(float3 camRel, float3 rd, float Rg, float Rt, out float h_tan, out float L_shell)
{
    // Closest approach to center (|rd| = 1): distance minus ground radius -> tangent altitude
    float d_closest = length(cross(camRel, rd));
    h_tan = d_closest - Rg;

    // Intersections
    float t0o, t1o;
    bool hitOut = RaySphere(camRel, rd, Rt, t0o, t1o);
    if (!hitOut || t1o <= 0.0)
    {
        L_shell = 0.0;
        return false;
    }

    float t0i, t1i;
    bool hitIn = RaySphere(camRel, rd, Rg, t0i, t1i);

    // Forward segment length inside the shell
    float r0 = length(camRel);
    float tEnter, tExit;
    if (r0 >= Rt - 1e-3)
    {
        tEnter = max(t0o, 0.0);
        tExit = t1o;
        if (hitIn && t1i > 0.0)
            tExit = min(tExit, t0i);
    }
    else
    {
        tEnter = 0.0;
        tExit = t1o;
        if (hitIn && t1i > 0.0)
            tExit = min(tExit, t0i);
    }

    L_shell = max(tExit - tEnter, 0.0);
    return (L_shell > 0.0);
}

// ---------- main: star occlusion that respects atmosphere shell -------------
float4 main(PixelInputType input) : SV_Target
{
    // Discard where geometry exists (reversed-Z: sky depth ~ 0)
    float depth = SceneDepth.Sample(PointSampler, input.texCoord);
    if (depth > 1e-12f)
        discard;

    // Reconstruct world ray (row_major: mul(vector, matrix))
    float2 ndc = input.texCoord * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    float4 clipPos = float4(ndc, 0.0f, 1.0f);
    float4 viewPos = mul(clipPos, inverseProjectionMatrix);
    viewPos /= max(viewPos.w, 1e-6);
    float3 viewDir = normalize(viewPos.xyz);
    float3 worldDir = normalize(mul(viewDir, (float3x3) inverseViewMatrix));

    // Camera & planet
    float3 planetCenterTrueWS = mul(float4(PlanetCenterWS, 1.0f), worldTranslationMatrix).xyz;
    float3 camWS = cameraPosition.xyz;
    float3 camRel = camWS - planetCenterTrueWS;
    float rCam = length(camRel);
    float Rg = PlanetRadius;
    float Rt = PlanetRadius + AtmosphereHeight;

    // Sun
    float3 wSun = -normalize(direction.xyz); // TO sun
    float sunAlt = SunAltitudeDeg(camWS, planetCenterTrueWS, direction.xyz);

    // ---------- base visibility ----------
    // Ground night factor (0 at/above TwilightStartDeg → 1 by TwilightEndDeg)
    float surfaceNight = sstep(TwilightStartDeg, TwilightEndDeg, sunAlt);

    // Space visibility from altitude
    float spaceFadeStartAlt = PlanetRadius + SpaceFadeStart * AtmosphereHeight;
    float spaceFadeEndAlt = PlanetRadius + SpaceFadeEnd * AtmosphereHeight;
    float spaceVis = sstep(spaceFadeStartAlt, spaceFadeEndAlt, rCam);

    // Start from night OR space
    float visibility = saturate(max(surfaceNight, spaceVis));

    // ---------- shell occlusion (SPACE ONLY) ----------
    if (rCam >= Rt - 1e-3)  // <-- key fix: do not occlude when inside atmo
    {
        float h_tan, L_shell;
        bool crossesShell = AtmosRayInfo(camRel, worldDir, Rg, Rt, h_tan, L_shell);
        if (crossesShell)
        {
            // If tangent altitude is above the top of the air, don't occlude
            // Otherwise attenuate proportionally to depth and path length.
            float depthWeight = saturate((AtmosphereHeight - max(h_tan, 0.0)) / AtmosphereHeight); // 0 at top, 1 near ground
            const float Ls = 80000.0; // 80 km scale for chord length → weight
            float lenWeight = 1.0 - exp(-L_shell / Ls);
            float atmoOcc = pow(saturate(depthWeight * lenWeight), 0.8); // gentle curve

            // Leave a tiny residual in very thin upper-atmo; stronger near limb
            visibility *= (1.0 - 0.95 * atmoOcc);
        }
    }

    // Sample stars (normalized 0..1)
    float3 starTex = radianceTexture.SampleLevel(LinearSampler, worldDir, 0.0f).rgb;

    // Output normalized stars; scale to nits later in tonemapper
    float3 outStars = starTex * visibility;

    return float4(outStars, 1.0);
}
