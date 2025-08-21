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
    PixelInputType output; //https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html 
    output.texCoord = float2((vID << 1) & 2, vID & 2); 
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1); 
    return output; 
} 

#type pixel
#pragma pack_matrix(row_major)

Texture2D<float4> TransmittanceLUT          : register(t0); // for sun disc dimming
Texture2D<float4> MultiScattLUT             : register(t1); // (not used directly here)
Texture2D<float4> SkyViewLUT                : register(t2); // per-frame sky radiance (premultiplied)
Texture3D<float4> AerialPerspective         : register(t3); // RGB=L, A=T_to_dist
Texture2D<float> DepthTex                   : register(t9); // reversed-Z
Texture2D<float4> BaseColor                 : register(t10);

SamplerState ClampLinear                    : register(s0);
SamplerState ClampPoint                     : register(s1);
SamplerState UWrapVClampLinear              : register(s2);

#define BYPASS_SKYVIEW      0
#define BYPASS_AP           0
#define SUN_RGB_DEBUG       0
#define SKY_IS_PREMULT      1
#define DBG                 0   // 0=off, 1=sunUp, 2=TcamSun, 3=azim&muV, 4=APuvw

static const float PI = 3.14159265359f;
static const float ATMO_EXPOSURE = 12.0;

// ---------------- CBuffers ----------------
cbuffer Camera : register(b0)
{
    matrix worldTranslationMatrix; // (unused here)
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition; // world-space
    float farZ; // (unused)
    float nearZ; // (unused)
    float viewportWidth; // (unused)
    float viewportHeight; // (unused)
};

cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj; // (unused)
    float4 direction; // light dir
    float4 radiance; // rgb
    float multiplier; // intensity
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
    uint StepsTransmittance; // (unused here)
    uint StepsMultiScattering; // (unused here)
    float APFarDynamic; // X axis of AP
};

cbuffer SunParamsCB : register(b6)
{
    float SunDiscRadius; // radians (~0.00465 for 0.53°)
    float SunEdgeSoftness; // radians (0.001–0.003)
    float SunGlowSize; // radians (3–8 × disc radius)
    float SunGlowIntensity; // 1–5
    uint SunDiscToggle; // 0/1
};

// -------------- helpers --------------
#ifndef SUN_DIR_NEGATE
#define SUN_DIR_NEGATE 1  // 1 if your 'direction' points from sun -> scene (common)
#endif

float3 GetSunDirWS()
{
    float3 d = normalize(direction.xyz);
    return SUN_DIR_NEGATE ? -d : d; // now points from point -> sun
}
float3 GetSunIlluminance()
{
#if SUN_RGB_DEBUG
        return float3(1,1,1);
#else
    return radiance.rgb * multiplier;
#endif
}

// reconstruct a view-space position from depth (reversed-Z friendly via invProj)
float3 ReconstructViewPos(float2 uv, float depth01)
{
    // DX NDC: x[-1..1], y[1..-1], z[0..1]
    float2 ndcXY = float2(uv.x, 1.0 - uv.y) * 2.0 - 1.0;
    float4 clip = float4(ndcXY, depth01, 1.0);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    return vpos.xyz / vpos.w; // view space
}

// fast view ray (view space) to far plane
float3 ViewRayDirVS(float2 uv)
{
    float2 ndcXY = float2(uv.x, 1.0 - uv.y) * 2.0 - 1.0;
    float4 clip = float4(ndcXY, 0.0, 1.0); // reversed-Z far
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

// ---- DX11 V-flipped transmittance lookup (same as compute) ----
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

//// build azimuth (0..1) and muV from a world-space view dir
//void SkyParamsFromDir(float3 dirWS, out float azim01, out float muV)
//{
//    float3 camWS = cameraPosition.xyz;
//    float3 upWS = normalize(camWS - PlanetCenterWS);

//    // ensure orthonormal horizon frame
//    float3 eastWS = normalize(BasisTanEast - upWS * dot(BasisTanEast, upWS));
//    float3 northWS = normalize(BasisTanNorth - upWS * dot(BasisTanNorth, upWS));

//    muV = saturate(dot(dirWS, upWS)); // cos(view zenith)
//    float3 hdir = normalize(dirWS - upWS * muV); // projected to horizon
//    float cosP = dot(hdir, eastWS);
//    float sinP = dot(hdir, northWS);
//    float phi = atan2(sinP, cosP); // [-pi..pi]
//    if (phi < 0.0)
//        phi += 2.0 * PI;
//    // SkyView LUT was written with V flipped (DecodeSkyCoords used v=1-v)
//    azim01 = phi * (1.0 / (2.0 * PI));
//}
void SkyParamsFromDir(float3 dirWS, out float azim01, out float muV)
{
    float3 camWS = cameraPosition.xyz;
    float3 upWS = normalize(camWS - PlanetCenterWS);

    // Orthonormal horizon frame
    float3 eastWS = normalize(BasisTanEast - upWS * dot(BasisTanEast, upWS));
    float3 northWS = normalize(BasisTanNorth - upWS * dot(BasisTanNorth, upWS));

    muV = saturate(dot(dirWS, upWS)); // cos(view zenith)
    float sin2 = max(0.0, 1.0 - muV * muV); // sin^2(theta)

    if (sin2 < 1e-8)
    { // azimuth undefined at the pole
        azim01 = 0.0; // any constant is fine
        return;
    }

    // Normalize horizon component without a fragile normalize()
    float invSin = rsqrt(sin2);
    float3 h = (dirWS - upWS * muV) * invSin;

    float cosP = dot(h, eastWS);
    float sinP = dot(h, northWS);
    float phi = atan2(sinP, cosP); // [-pi..pi]
    if (phi < 0.0)
        phi += 2.0 * PI;

    azim01 = phi * (1.0 / (2.0 * PI));
}

// map world-space to AP-3D UVW
float3 AP_UVW(float dist, float muV, float rCam)
{
    // X: distance ∈ [0..APFarDynamic], compute inverse of a^2 mapping used in compute
    float a = sqrt(saturate(dist / max(1e-3, APFarDynamic)));
    float u = a;

    // Y: muV with DX11 flip
    float v = 1.0 - saturate(muV);

    // Z: altitude slice
    float rMin = PlanetRadius + MinHeight;
    float rMax = PlanetRadius + AtmosphereHeight;
    float w = saturate((rCam - rMin) / max(1e-3, (rMax - rMin)));
    return float3(u, v, w);
}

// ---------------- main ----------------
float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD) : SV_Target
{
    // read depth
    float depth = DepthTex.Sample(ClampPoint, uv);

    // view/world geometry
    float3 dirVS = ViewRayDirVS(uv);
    float3 dirWS = normalize(mul(dirVS, (float3x3) inverseViewMatrix)); // to world
    float3 camWS = cameraPosition.xyz;
    float3 upWS = normalize(camWS - PlanetCenterWS);
    float rCam = length(camWS - PlanetCenterWS);

    // base shaded color
    float3 baseRGB = BaseColor.Sample(ClampLinear, uv).rgb;

    // decide sky vs geometry: reversed-Z clear = 0 → sky
    bool isSky = (depth <= 1e-16);

    // ---------------- SKY ----------------
    if (isSky && BYPASS_SKYVIEW == 0)
    {
        float azim01, muV;
        SkyParamsFromDir(dirWS, azim01, muV);

        // ---- zenith-safe sampling of SkyViewLUT ----
        uint lutW, lutH;
        SkyViewLUT.GetDimensions(lutW, lutH);

        float u = azim01; // [0,1) wraps in U
        float v = 1.0 - muV; // 0 at zenith
        float texelU = 1.0 / max(1.0, (float) lutW);
        
        // 1) derive grads and suppress U near the pole (or when U gradient is huge)
        float2 dx_uv = float2(ddx(u), ddx(v));
        float2 dy_uv = float2(ddy(u), ddy(v));
        
        // “how dangerous” the U gradient is (large at the pole due to azimuth squeeze)
        float uDanger = (abs(dx_uv.x) + abs(dy_uv.x)) * lutW; // unitless
        float poleFix = saturate(uDanger); // 0..1
        
        float2 dx_fix = lerp(dx_uv, float2(0.0, dx_uv.y), poleFix);
        float2 dy_fix = lerp(dy_uv, float2(0.0, dy_uv.y), poleFix);

        // single stable sample (no mip seams; your texture has 1 mip)
        float3 skyMain = SkyViewLUT.SampleGrad(UWrapVClampLinear, float2(u, v), dx_fix, dy_fix).rgb;

        // 2) OPTIONAL: same-v azimuth prefilter (no V shift → no ring)
        float3 skyAvg = 0;
        [unroll]
        for (int i = 0; i < 4; ++i)
            skyAvg += SkyViewLUT.Sample(UWrapVClampLinear, float2(u + 0.25f * i, v)).rgb;
        skyAvg *= 0.25f;

        // blend based on how much we suppressed U (not on v)
        float3 skyL = lerp(skyMain, skyAvg, poleFix);
        
        // --- Sun disc with crisp AA edge ---
        float3 sunDir = GetSunDirWS();
        float mu = saturate(dot(dirWS, sunDir)); // cos(angle to sun)
        float ang = acos(mu);

        // Screen-space AA for the rim: d(acos(mu)) = -dmu / sqrt(1-mu^2)
        float dMu = abs(ddx(mu)) + abs(ddy(mu));
        float dAng = dMu / max(1e-3, sqrt(1.0 - mu * mu));
        float edgeAA = 1.5 * dAng;
        float edge = max(SunEdgeSoftness, edgeAA);

        float discMask = smoothstep(SunDiscRadius + edge, SunDiscRadius - edge, ang);

        // ---- NEW: harden the rim a bit more near the horizon (muS≈0) ----
        float muS_for_edge = saturate(dot(upWS, sunDir)); // sun zenith cosine at camera
        float rimHarden = lerp(2.0, 1.0, muS_for_edge); // harder near horizon, neutral aloft
        discMask = pow(discMask, rimHarden);

        // Planet occlusion (unchanged)
        RayHit g = RaySphereWS(camWS - PlanetCenterWS, sunDir, PlanetRadius);
        bool sunBlocked = (g.hit && g.t0 > 0.0);

        float3 discHDR = 0.0;
        if (SunDiscToggle != 0 && !sunBlocked)
        {
            float muS = muS_for_edge;
            float3 TcamSun = LookupTransSafe(rCam, muS);

            // ---- NEW: horizon visibility lift ----
            // kHorizon  = 0.55 at horizon → 1.0 by muS≈0.25; brightens only low sun
            float kHorizon = lerp(0.55, 1.0, smoothstep(0.0, 0.25, muS));
            float3 Tvis = pow(TcamSun, kHorizon);
            Tvis = max(Tvis, 0.002.xxx); // tiny floor (0.2%) so it never disappears

            float3 SunRadiance = GetSunIlluminance(); // keep in radiance units (no Ω division)
            discHDR = discMask * SunRadiance * Tvis; // HDR disc (no halo)
        }

        // Tone-map separately then overwrite
        float3 skyTM = skyL * ATMO_EXPOSURE / (1.0 + skyL * ATMO_EXPOSURE);
        float3 discTM = discHDR * ATMO_EXPOSURE / (1.0 + discHDR * ATMO_EXPOSURE);
        float3 outTM = lerp(skyTM, discTM, discMask);
        return float4(outTM, 1.0);
    }

    // ---------------- GEOMETRY + AP ----------------
    // reconstruct view-space position → distance along ray
    float3 posVS = ReconstructViewPos(uv, depth);
    float dist = length(posVS); // meters if your matrices are

    // AP LUT sample (RGB=L, A=T_to_dist)
    float muVg = saturate(dot(dirWS, upWS));
    float3 uvw = AP_UVW(min(dist, APFarDynamic), muVg, rCam);
    float4 ap = (BYPASS_AP == 0) ? AerialPerspective.Sample(ClampLinear, uvw) : float4(0, 1, 0, 1); // bypass: no fog
    
#if DBG==1
    float sunUp = dot(normalize(cameraPosition.xyz-PlanetCenterWS), GetSunDirWS());
    return float4(saturate(0.5*sunUp+0.5).xxx,1);
#elif DBG==2
    upWS = normalize(cameraPosition.xyz-PlanetCenterWS);
    float3 TcamSun = LookupTransSafe(length(cameraPosition.xyz-PlanetCenterWS),
                                     dot(upWS, GetSunDirWS()));
    return float4(TcamSun,1);
#elif DBG==3
    float azim01, muV; SkyParamsFromDir(dirWS, azim01, muV);
    return float4(azim01,muV,0,1);
#elif DBG==4
    muVg = saturate(dot(dirWS, normalize(cameraPosition.xyz-PlanetCenterWS)));
    uvw  = AP_UVW( length(ReconstructViewPos(uv, DepthTex.Sample(ClampPoint,uv))),
                          muVg,
                          length(cameraPosition.xyz-PlanetCenterWS) );
    return float4(uvw,1);
#endif
    
    float3 Lfog = 1.0 - exp(-ATMO_EXPOSURE * ap.rgb);
    
    // composite: in-scatter + transmittance * base  
    float3 outRGB = Lfog + ap.a * baseRGB;
    return float4(outRGB, 1.0);
}