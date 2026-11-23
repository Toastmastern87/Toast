﻿#inputlayout
#type compute
#pragma pack_matrix(row_major)

// ===== toggles ======================================================
#ifndef AP_MAX_Z_SLICES
#define AP_MAX_Z_SLICES       128u   // clamp work if OutAP has deeper Z
#endif
#ifndef AP_USE_FAST_EXP
#define AP_USE_FAST_EXP       1      // exp2-based fast exp
#endif
#ifndef AP_Z_GAMMA
#define AP_Z_GAMMA 1.6f   // try 1.5–1.8; 1.6 is a good start
#endif
#ifndef AP_VOLUMETRIC_SHADOWS
#define AP_VOLUMETRIC_SHADOWS 1
#endif
#ifndef AP_SHADOW_TAPS
#define AP_SHADOW_TAPS 4u
#endif

// ===== CBuffers =============================================================
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
    float3 PlanetCenterCR;
    float PlanetRadius;
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
    float Altitude;
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
    float BakeIBL;
    
    float3 MieLobeWeight;
    float MieAnisotropyBroad;
};

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius; // rad  (e.g. radians(0.2666))
    float SunEdgeSoftness; // rad  (soft rim width)
    int SunDiscToggle; // 0=off, 1=on
    float SpaceDiscBrightnessScale; // unitless scale, e.g. 1.30
    
    float3 SunDiscPad0;
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

cbuffer APTemporal : register(b7)
{
    float4x4 PrevViewProj;  // 0
    
    float3 PrevCameraPos;   // 64
    uint FrameIndex;        // 76
    
    float HistoryWeight;    // 80
    uint HistoryValid;      // 84
    float2 _pad0;
}

// ===== LUTs =================================================================
Texture2D<float4> TransmittanceLUT          : register(t0);
Texture2D<float4> MultiScatterLUT           : register(t1);
Texture2D<uint> APFarU32                    : register(t2);
Texture2DArray<float> ShadowDepthTexture    : register(t3);
Texture3D<float> VisHistory                 : register(t4);
Texture2D<float> SceneDepth                 : register(t5);
Texture2D<float2> ShadowHeightMap           : register(t6);

SamplerState ClampLinear : register(s0);
SamplerState ClampPoint : register(s1);

// 3D AP output: [x,y]=screen tile, [z]=non-linear distance
// rgb = accumulated in-scatter, a = camera->slice scalar transmittance
RWTexture3D<float4> OutAP       : register(u0);
RWTexture3D<float> OutVis       : register(u1);
RWTexture2D<float4> OutSkyShadow : register(u2);

// ===== Constants / helpers ==================================================
static const float PI = 3.14159265358979323846f;
static const float INV4PI = 0.25f / PI;
static const float MU_EPS = 2e-3;
static const bool AP_TERRAIN_SHADOW_MAP = true;

// Ozone (Bruneton)
static const float3 O3_COEFF = float3(0.650e-6, 1.881e-6, 0.085e-6);

#include "TransmittanceLUT.hlsli"
#include "ShadowHeightMap.hlsli"

#if AP_USE_FAST_EXP
// exp(x) ~ exp2(x/ln(2))
float3 fexp3(float3 x)
{
    return exp2(x * 1.4426950408889634);
}
float fexp1(float x)
{
    return exp2(x * 1.4426950408889634);
}
#else
float3 fexp3(float3 x) { return exp(x); }
float  fexp1(float  x) { return exp(x); }
#endif

float DensityExpTapered(float h, float H)
{
    float hTop = AtmosphereHeight - min(0.0f, MinHeight);
    H = max(H, 1e-3f);
    return max(exp(-max(h, 0.0f) / H) - exp(-hTop / H), 0.0f);
}

float DensityRayleigh(float h)
{
    return DensityExpTapered(h, RayScaleHeight);
}
float DensityMie(float h)
{
    return DensityExpTapered(h, MieScaleHeight);
}
float DensityOzone(float hMeters)
{
    // Triangle 10–40 km peaking at 25 km.
    // Normalize so that the column integral equals OzoneStrength (area = 15000 m).
    float km = hMeters * 1e-3f;
    float tri = saturate(1.0f - abs((km - 25.0f) / 15.0f));
    return tri * (OzoneStrength / 15000.0f);
}

float PhaseRayleigh(float mu)
{ //
    return (3.0f * INV4PI) * 0.25f * (1.0f + mu * mu);
}

float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
}

// MultiScatter LUT sampling: x=theta_s/π, y = 1 - linear altitude (top=TOA)
float4 SamplePsiMS4(float r, float muS, float Rb, float Rt)
{
    float mu = clamp(muS, -1.0f + 1.0e-3f, 1.0f - 1.0e-3f);
    float thetaS = acos(mu);
    float u = thetaS / PI;

    float v = saturate((r - Rb) / max(Rt - Rb, 1.0e-6f));
    v = 1.0f - v;
    v = clamp(v, 1.0e-3f, 1.0f - 1.0e-3f); // <- avoid top/bottom rows
    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0);
}
// MS anisotropy blend using LUT alpha as effective ḡ
float MSPhase(float mu, float gBar)
{
    float pIso = INV4PI;
    float g = saturate(gBar);
    float g2 = g * g;
    float d = 1.0f + g2 - 2.0f * g * mu;
    float pHG = INV4PI * (1.0f - g2) / max(pow(d, 1.5f), 1e-5f);
    return lerp(pIso, pHG, g);
}

float GroundBiasMeters(float Rg)
{
    return max(1.0f, 2e-6f * Rg);
}

// Ray-sphere intersection in a numerically stable way.
// ro: ray origin (translated-space, meters, relative to planet center)
// rd: ray direction (any length; normalized internally)
// R : sphere radius in meters (e.g., Rt or RbHit; sign ignored)
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

// Small horizon softening (matches SkyView)
float SunVisibilityAtR(float r, float muS, float Rb)
{
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunDiscRadius, sinThetaH * SunDiscRadius, muS - cosThetaH);
}

float3 ViewDirWSFromUV(float2 uv)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, 1.0f, 1.0f);

    // View-space direction towards far plane
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 dirVS = normalize(vpos.xyz / max(vpos.w, 1e-6f));

    // Rotate into world space (remove translation)
    float3 dirWS = normalize(mul(dirVS, (float3x3) inverseViewMatrix));
    return dirWS;
}

float ViewDistanceFromDepth(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
    float4 clip = float4(ndc, depth, 1);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    return length(vpos.xyz / max(vpos.w, 1e-12));
}

float PhaseMieHG_CS(float mu, float g) // Cornette–Shanks
{
    g = saturate(g);
    float g2 = g * g;
    float denom = pow(1.0 + g2 - 2.0 * g * mu, 1.5);
    float cs = (3.0 * (1.0 + mu * mu)) / (2.0 * (2.0 + g2)) * ((1.0 - g2) / max(denom, 1e-5));
    return INV4PI * cs;
}

float MuDiscAvg(float mu)
{
    float theta = acos(clamp(mu, -1.0f, 1.0f));
    float thetaEff = sqrt(theta * theta + SunDiscRadius * SunDiscRadius);
    return cos(thetaEff);
}

float3 PhaseMieTwoLobe(float mu)
{
    float muEff = MuDiscAvg(mu);
    float3 gN = saturate(MieAnisotropy);
    float3 w = saturate(MieLobeWeight);
    float3 narrow = float3(PhaseMieHG_CS(muEff, gN.r), PhaseMieHG_CS(muEff, gN.g), PhaseMieHG_CS(muEff, gN.b));
    float broad = PhaseMieHG_CS(muEff, MieAnisotropyBroad);
    return w * narrow + (1.0f - w) * broad;
}

// μ at horizon for a sphere of radius R as seen from r
float MuHorizon(float r, float R)
{
    float s = saturate(R / r);
    return -sqrt(max(1.0f - s * s, 0.0f));
}

uint SelectCascade(float viewDepth)
{
    // viewDepth should be positive forward distance in view space
    // pick first i where viewDepth <= CascadeEnds[i]
    [unroll]
    for (uint i = 0; i < CascadeCount; ++i)
    {
        if (viewDepth <= CascadeEnds[i])
            return i;
    }
    return CascadeCount - 1;
}

float VolumetricShadow(float3 posWS)
{
    if (CascadeCount == 0)
        return 1.0f;

    const float viewDepth = mul(float4(posWS, 1.0f), viewMatrix).z;
    if (viewDepth <= 0.0f || viewDepth > CascadeEnds[CascadeCount - 1])
        return 1.0f;

    const uint ci = SelectCascade(viewDepth);
    const float4 shadowClip = mul(float4(posWS, 1.0f), lightViewProj[ci]);
    const float3 ndc = shadowClip.xyz / shadowClip.w;
    
    float2 shadowUV = ndc.xy * 0.5f + 0.5f;
    shadowUV.y = 1.0f - shadowUV.y;
    
    if (any(shadowUV < 0.0f) || any(shadowUV > 1.0f) || ndc.z < 0.0f || ndc.z > 1.0f)
        return 1.0f;

    const float occluderDepth = ShadowDepthTexture.SampleLevel(ClampPoint, float3(shadowUV, (float)ci), 0);
    return (ndc.z - ConstantBias <= occluderDepth) ? 1.0f : 0.0f;
}

float ShadowHeightLit(ShadowHeightFrame frame, float3 posFromPlanetWS)
{
    const float r = length(posFromPlanetWS);
    const float3 dirPS = ShadowHeightMapToPlanetSpace(posFromPlanetWS / max(r, 1.0f), BasisLonEast, BasisSpinUp, BasisLonNorth);

    float2 uv;
    if (!ShadowHeightDirToUV(frame, dirPS, PlanetRadius, uv))
        return 1.0f;

    const float2 shm = ShadowHeightMap.SampleLevel(ClampLinear, uv, 0);
    const float w = max(shm.y * max(SunDiscRadius, SHADOWHEIGHTMAP_MIN_PENUMBRA), SHADOWHEIGHTMAP_MIN_PENUMBRA_M);
    return smoothstep(-w, w, (r - PlanetRadius) - shm.x);
}

static const float AP_HANDOFF_BAND = 0.2f;

float VolumetricLit(ShadowHeightFrame frame, float3 posWS, float3 posFromPlanetWS)
{
    if (CascadeCount == 0)
        return 1.0f;
    
    const float csm = VolumetricShadow(posWS);
    if (!AP_TERRAIN_SHADOW_MAP)
        return csm;
    
    const float viewDepth = mul(float4(posWS, 1.0f), viewMatrix).z;
    const float csmWeigth = 1.0f - smoothstep((1.0f - AP_HANDOFF_BAND) * ShadowDistance, ShadowDistance, viewDepth);
    if (csmWeigth >= 1.0f)
        return csm;

    return lerp(ShadowHeightLit(frame, posFromPlanetWS), csm, csmWeigth);
}

float InterleavedGradientNoise(float2 pixel, uint frame)
{
    pixel += 5.588238f * (float) (frame & 63u);
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float LitHistory(float3 posWS, float apFar)
{
    if (HistoryValid == 0)
        return -1.0f;

    const float4 clip = mul(float4(posWS, 1.0f), PrevViewProj);
    if (clip.w <= 0.0f)
        return -1.0f;

    const float2 ndc = clip.xy / clip.w;
    const float2 uv = float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
    const float w = pow(saturate(length(posWS - PrevCameraPos) / max(apFar, 1.0f)), 1.0f / AP_Z_GAMMA);

    if (any(uv < 0.0f) || any(uv > 1.0f) || w >= 1.0f)
        return -1.0f;

    return VisHistory.SampleLevel(ClampLinear, float3(uv, w), 0);
}

static const float2 OFFS[4] =
{
    float2(0.25, 0.25), float2(0.75, 0.25),
    float2(0.25, 0.75), float2(0.75, 0.75)
};

// ===== Main =================================================================
[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint W, H, Dfull;
    OutAP.GetDimensions(W, H, Dfull);
    if (tid.x >= W || tid.y >= H)
        return;
    uint D = min(Dfull, AP_MAX_Z_SLICES);

    float APFar = asfloat(APFarU32.Load(int3(0, 0, 0)));
    float APNear = 0.0f;
    
    // Planet & radii
    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius);
    const float RbHit = RbPhys + GroundBiasMeters(RbPhys);
    
    float3 ro = cameraPosition.xyz - PlanetCenterCR;
    float2 uvC = (float2(tid.xy) + 0.5f) / float2(W, H);
    float3 wView = ViewDirWSFromUV(uvC);
    
    uint depthW, depthH;
    SceneDepth.GetDimensions(depthW, depthH);
    const float sceneDepth = SceneDepth.Load(int3(min(uint2(uvC * float2(depthW, depthH)), uint2(depthW - 1, depthH - 1)), 0));
    const float tScene = (sceneDepth > 1e-12f) ? ViewDistanceFromDepth(uvC, sceneDepth) : 1e30f;
    const ShadowHeightFrame shmFrame = BuildShadowHeightFrame(ro, BasisLonEast, BasisSpinUp, BasisLonNorth, APFar);
 
    Hit ha = IntersectSphereGrazingSafe(ro, wView, Rt);
    if (!ha.ok)
    { // nothing to accumulate for this texel
        float4 zero = float4(0, 0, 0, 0);
        [loop]
        for (uint z = 0; z < D; ++z)
        {
            OutAP[uint3(tid.x, tid.y, z)] = zero;
#if AP_VOLUMETRIC_SHADOWS
            OutVis[uint3(tid.xy, z)] = 1.0f;
#endif
        }
        OutSkyShadow[tid.xy] = 0.0f;
        return;
    }
    
    // Entry/exit in absolute meters
    float tEnter = max(0.0f, ha.t0); // if camera inside, this becomes 0
    float tExitA = max(0.0f, ha.t1);
    
    Hit hg = IntersectSphereGrazingSafe(ro, wView, RbHit);
    if (hg.ok && hg.t0 > 0.0f)
        tExitA = min(tExitA, hg.t0);
    
    float Lray = max(0.0f, tExitA - tEnter);
    
    float Lcap = min(APFar, Lray);
    
    // Early out: if this ray has no air in front, write zeros
    if (Lcap <= 1e-6f)
    {
        float4 zero = float4(0, 0, 0, 0);
        [loop]
        for (uint z = 0; z < D; ++z)
        {
            OutAP[uint3(tid.x, tid.y, z)] = zero;
#if AP_VOLUMETRIC_SHADOWS
            OutVis[uint3(tid.xy, z)] = 1.0f;
#endif
        }
        
        OutSkyShadow[tid.xy] = 0.0f;
        return;
    }
     
    // Accumulators to current slice end
    float3 Lcum = 0.0f; // in-scattered radiance
    float3 tauCum = 0.0f; // camera->current extinction
    float3 Lremoved = 0.0f;

    const float3 wSun = -normalize(direction.xyz);
    float3 Esun = radiance.rgb * SunIntensity;
    
    [loop]
    for (uint z = 0; z < D; ++z)
    {
        float u0 = (float) z / (float) D;
        float u1 = (float) (z + 1) / (float) D;
        
        // place slices at fixed distance from entry only
        float sA = APFar * pow(u0, AP_Z_GAMMA);
        float sB = APFar * pow(u1, AP_Z_GAMMA);

        // Clamp this slice to the actual ray length Lcap
        float segA = clamp(sA, 0.0f, Lcap);
        float segB = clamp(sB, 0.0f, Lcap);
        if (segB <= segA)
        {
            // store current cumulative so Z filtering has defined values
            float3 Tcum = exp(-tauCum);
            float Tmean = max((Tcum.r + Tcum.g + Tcum.b) * (1.0 / 3.0), 1e-6f);
            float tauMean = -log(Tmean);
            OutAP[uint3(tid.x, tid.y, z)] = float4(Lcum, tauMean);
#if AP_VOLUMETRIC_SHADOWS
            OutVis[uint3(tid.xy, z)] = 1.0f;
#endif
            continue;
        }

        float len = segB - segA;
        float sMid = 0.5f * (segA + segB);

        // Convert S (from entry) to absolute t
        float tMid = tEnter + sMid;

        // Sample medium at midpoint
        float3 p = ro + wView * tMid;
        float rMid = length(p);
        float hMid = max(0.0f, rMid - RbPhys);

        float dR = DensityRayleigh(hMid);
        float dM = DensityMie(hMid);
        float dO = DensityOzone(hMid);

        float3 sigR_s = RayleighScattering * dR;
        float3 sigM_s = MieScattering * dM;
        float3 sigM_a = MieAbsorption * dM;
        
        float3 sigmaExt = RayleighScattering * dR + (MieScattering + MieAbsorption) * dM + O3_COEFF * dO;
        
        float3 upS = (rMid > 0.0f) ? (p / rMid) : BasisSpinUp;
        float muS = dot(upS, wSun);
        float Vsun = SunVisibilityAtR(rMid, muS, RbVis);
        float lit = 1.0f;
#if AP_VOLUMETRIC_SHADOWS
        const float jitter = InterleavedGradientNoise(float2(tid.xy), FrameIndex);
        float litNow = 0.0f;
        [unroll]
        for (uint k = 0; k < AP_SHADOW_TAPS; ++k)
        {
            const float tk = tEnter + segA + len * (((float) k + jitter) / (float) AP_SHADOW_TAPS);
            litNow += VolumetricLit(shmFrame, cameraPosition.xyz + wView * tk, ro + wView * tk);
        }
        litNow *= (1.0f / (float) AP_SHADOW_TAPS);
        
        const float litPrev = LitHistory(cameraPosition.xyz + wView * tMid, APFar);
        lit = (litPrev >= 0.0f) ? lerp(litPrev, litNow, HistoryWeight) : litNow;
        OutVis[uint3(tid.xy, z)] = lit;
#endif
        float3 Tsun = T_to_TOA(rMid, muS, RbPhys, Rt) * Vsun;        
        
        // Phase & disc-averaged Mie (per-channel), same as SkyView
        float muPhase = clamp(dot(wSun, wView), -1.0f, 1.0f);
        float PR = PhaseRayleigh(muPhase);
        float3 PMrgb = PhaseMieTwoLobe(muPhase);

        // --- Horizon-aware tint/gain (same as SkyView) ------------------------------
        // Physical horizon at current altitude
        float cH_phys = MuHorizon(rMid, RbPhys);

        // Sun/view elevations in [0..1] above *physical* horizon
        float elevS = saturate((muS - cH_phys) / (1.0 - cH_phys));
        float muV = clamp(dot(wView, upS), -0.9995f, 0.9995f);
        float elevV = saturate((muV - cH_phys) / (1.0 - cH_phys));

        // --- SINGLE scattering (Rayleigh + Mie, full Cornette-Shanks phase)
        float3 S1Full = ((sigR_s * PR) + (sigM_s * PMrgb)) * (Tsun * Esun);
        float3 S1 = S1Full * lit;
        
        // --- MULTI scattering (Hillaire 2020: isotropic, sigma_s * Psi_ms; identical in SkyViewCS) ---
        // Scattering coefficient for the multi-scattered light: MieScattering already excludes absorption.
        float3 sigS_ms = sigR_s + sigM_s;
        float3 PsiMS_rgb = SamplePsiMS4(rMid, muS, RbPhys, Rt).rgb;

        // Multi-scattered light is not shadowed by the planet here: the multi-scattering LUT
        // already includes the sun's visibility. Hillaire 2020 shadows single scattering only.
        float3 S_MS = sigS_ms * PsiMS_rgb * Esun;
        
        // Midpoint integral over this slice
        float3 dTau = sigmaExt * len;
        float3 wInt = (1.0.xxx - fexp3(-dTau)) / max(sigmaExt, 1e-8.xxx);
        
        float3 Tcam = fexp3(-tauCum);
        Lcum += Tcam * (S1 + S_MS) * wInt;
        const float inFront = saturate((tScene - (tEnter + segA)) / max(len, 1e-3f));
        Lremoved += Tcam * (S1Full - S1) * wInt * inFront;
        tauCum += dTau;

        // write cumulative for this z
        float3 Tcum = fexp3(-tauCum);
        float Tmean = max((Tcum.r + Tcum.g + Tcum.b) * (1.0f / 3.0f), 1e-6f);
        float tauMean = -log(Tmean);
        OutAP[uint3(tid.x, tid.y, z)] = float4(Lcum, tauMean);
    }

    OutSkyShadow[tid.xy] = float4(Lremoved, 0.0f);
    
    // replicate last to the tail if D < Dfull
    if (D < Dfull)
    {
        float4 last = OutAP[uint3(tid.x, tid.y, D - 1)];
        [loop]
        for (uint zz = D; zz < Dfull; ++zz)
            OutAP[uint3(tid.x, tid.y, zz)] = last;
    }
}