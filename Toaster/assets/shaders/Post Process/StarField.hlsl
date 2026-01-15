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
    float4x4 lightViewProj;
    
    float4 direction; // FROM light -> scene
    
    float4 radiance; // RGB
    
    float SunIntensity;
    float DirectionalLightGain;
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

Texture2D<float4> TransmittanceLUT : register(t0);
TextureCube radianceTexture : register(t5); // starfield cubemap
Texture2D<float> SceneDepth : register(t9); // reversed-Z depth

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

static const float PI = 3.14159265359f;
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);
static const float MU_EPS = 8e-4;

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

float2 TransUV(float r, float mu, float RbPhys, float Rt)
{
    float rNorm = (r - RbPhys) / max(Rt - RbPhys, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (RbPhys * RbPhys) / (r * r)));
    mu = clamp(mu, muMin + MU_EPS, 1.0f - MU_EPS);
    return float2((mu - muMin) / (1.0f - muMin), saturate(rNorm));
}

float3 T_to_TOA(float r, float mu, float RbPhys, float Rt)
{
    return TransmittanceLUT.SampleLevel(LinearSampler, TransUV(r, mu, RbPhys, Rt), 0).rgb;
}

// Transmittance through the atmosphere along 'rd' from the camera.
// Returns scalar attenuation for stars (use luminance of RGB T).
float StarTransmittance(float3 camRel, float3 rd, float rCam, float RbPhys, float RbVis, float Rt)
{
    // Inside atmosphere: straight TLUT to TOA
    if (rCam <= Rt)
    {
        float mu = dot(normalize(camRel), rd);
        float3 T = T_to_TOA(rCam, mu, RbPhys, Rt);
        return dot(T, LUMA); // or min(T) if you want stricter dimming
    }

    // Outside: only attenuate if the view ray crosses the shell
    float t0o, t1o;
    if (!RaySphere(camRel, rd, Rt, t0o, t1o) || t1o <= 0.0)
        return 1.0;

    float tEnter = max(0.0, t0o);
    float tExit = t1o;

    // Stop at ground if hit
    float t0g, t1g;
    if (RaySphere(camRel, rd, RbVis, t0g, t1g) && t1g > 0.0)
        tExit = min(tExit, t0g);

    float Lshell = max(tExit - tEnter, 0.0);
    if (Lshell <= 1e-5)
        return 1.0;

    // Use TLUT ratio along the in-atmosphere segment (same trick as your sky shader)
    float3 pEntry = camRel + rd * (tEnter + 1e-3);
    float rEntry = length(pEntry);
    float3 upEntry = pEntry / rEntry;
    float muEntry = dot(rd, upEntry);

    // End a hair before exit to avoid border artifacts
    float3 pExit = camRel + rd * (tExit - 1e-3);
    float rExit = length(pExit);
    float3 upExit = pExit / rExit;
    float muExit = dot(rd, upExit);

    float3 T_in = T_to_TOA(rEntry, muEntry, RbPhys, Rt);
    float3 T_out = T_to_TOA(rExit, muExit, RbPhys, Rt);
    float3 Tseg = saturate(T_in / max(T_out, 1e-6.xxx));

    return dot(Tseg, LUMA); // scalar attenuation
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
    float3 camWS = cameraPosition.xyz;
    float3 camRel = camWS - PlanetCenterWS;
    float rCam = length(camRel);
    float Rg = PlanetRadius;
    float Rt = PlanetRadius + AtmosphereHeight;

    // Sun
    float3 wSun = -normalize(direction.xyz); // TO sun
    float sunAlt = SunAltitudeDeg(camWS, PlanetCenterWS, direction.xyz);

    // ---------- base visibility ----------
    // Ground night factor (0 at/above TwilightStartDeg → 1 by TwilightEndDeg)
    float surfaceNight = sstep(TwilightStartDeg, TwilightEndDeg, sunAlt);

    // Space visibility from altitude
    float spaceFadeStartAlt = PlanetRadius + SpaceFadeStart * AtmosphereHeight;
    float spaceFadeEndAlt = PlanetRadius + SpaceFadeEnd * AtmosphereHeight;
    float spaceVis = sstep(spaceFadeStartAlt, spaceFadeEndAlt, rCam);

    // Base visibility from sun altitude / altitude fades (your code)
    float visibility = saturate(max(surfaceNight, spaceVis));

    // Atmospheric attenuation that is continuous across Rt
    float RbPhys = PlanetRadius + min(0.0f, MinHeight); // if you have it in this shader
    float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius); // same bias as elsewhere
    float att = StarTransmittance(camRel, worldDir, rCam, RbPhys, RbVis, Rt);

    // Final star visibility
    visibility *= att;

    // Sample stars and output
    float3 starTex = radianceTexture.SampleLevel(LinearSampler, worldDir, 0.0f).rgb;
    float3 outStars = starTex * visibility;
    return float4(outStars, 1.0);
}
