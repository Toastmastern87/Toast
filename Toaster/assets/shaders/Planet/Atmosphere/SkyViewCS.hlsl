#type compute
#pragma pack_matrix(row_major)

// ===== Debug / toggles ======================================================
#ifndef SKY_FLIP_Y
#define SKY_FLIP_Y            1
#endif
#ifndef SKY_ENABLE_GROUND
#define SKY_ENABLE_GROUND     1
#endif
#ifndef SKY_STEPS
#define SKY_STEPS             96
#endif
#ifndef SKY_DEBUG_MODE
#define SKY_DEBUG_MODE        0
#endif
#ifndef SKY_DBG_SCALE
#define SKY_DBG_SCALE         1.0f
#endif
#ifndef SKY_USE_MS
#define SKY_USE_MS            1
#endif
#ifndef SKY_USE_MIE
#define SKY_USE_MIE           1
#endif
#ifndef SKY_USE_RAY
#define SKY_USE_RAY           1
#endif

// Match MultiScatteringCS::MS_BAKE_SUNVIS (1 = already gated in LUT, 0 = gate with Vsun at runtime)
#ifndef SKY_MS_BAKED_SUNVIS
#define SKY_MS_BAKED_SUNVIS   1
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
    float4x4 lightViewProj;
    float4 direction; // FROM light -> scene
    float4 radiance; // RGB
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
    float RayScaleHeight;
    float MieScaleHeight;
    float MieAnisotropy;
    float3 RayleighScattering;
    float3 MieScattering;
    float3 MieAbsorption;
    float3 GroundAlbedo;
    float OzoneStrength;
    uint StepsTransmittance;
    uint StepsMultiScattering;
    float APFarDynamic;
};

// ===== LUTs =================================================================
Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatterLUT : register(t1);
Texture1D<uint> HorizonMu : register(t2);

SamplerState ClampLinear : register(s0);

RWTexture2D<float4> OutSkyView : register(u0);

// ===== Constants / helpers ==================================================
static const float PI = 3.14159265358979323846f;
static const float TWO_PI = 6.283185307179586f;
static const float INV4PI = 0.25f / PI;
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

float DecodeMu(uint ox, out bool ok)
{
    if (ox == 0u)
    {
        ok = false;
        return 0.0;
    }
    uint m = (ox & 0x80000000u) ? 0x80000000u : 0xffffffffu;
    ok = true;
    return asfloat(ox ^ m);
}

// === helpers for seam-safe FOV slice ===
struct FovSlice
{
    float uL;
    float uR;
    bool wraps;
};

FovSlice MakeSlice(float uLeft, float uRight)
{
    uLeft = frac(uLeft + 1.0f);
    uRight = frac(uRight + 1.0f);

    float forward = (uRight - uLeft);
    if (forward < 0.0f)
        forward += 1.0f;

    FovSlice s;
    if (forward <= 0.5f)
    {
        s.uL = uLeft;
        s.uR = uRight;
        s.wraps = false;
    }
    else
    {
        s.uL = uRight;
        s.uR = uLeft;
        s.wraps = true;
    }
    return s;
}

bool InSlice(float u, FovSlice s)
{
    if (!s.wraps)
        return (u >= s.uL) && (u <= s.uR);
    return (u >= s.uL) || (u <= s.uR); // wrapped
}

float SliceU(float u, FovSlice s)
{
    if (!s.wraps)
        return (u - s.uL) / max(1e-6f, (s.uR - s.uL));
    float len = (1.0f - s.uL) + s.uR;
    float t = (u >= s.uL) ? (u - s.uL) : ((1.0f - s.uL) + u);
    return t / max(1e-6f, len);
}

// === FOV slice in SkyView space (uses WS basis) ===
FovSlice GetSkyViewFOVSlice(float3 east, float3 north)
{
    float tanHalfFovX = 1.0f / projectionMatrix._11;
    float halfFovX = atan(tanHalfFovX);

    float3 fwd = normalize(inverseViewMatrix[2].xyz);
    float3 right = normalize(inverseViewMatrix[0].xyz);

    float3 leftDir = normalize(fwd * cos(halfFovX) - right * sin(halfFovX));
    float3 rightDir = normalize(fwd * cos(halfFovX) + right * sin(halfFovX));

    float lonL = atan2(dot(leftDir, north), dot(leftDir, east));
    float lonR = atan2(dot(rightDir, north), dot(rightDir, east));

    float uLeft = (lonL + PI) / (2.0f * PI);
    float uRight = (lonR + PI) / (2.0f * PI);

    return MakeSlice(uLeft, uRight);
}

float SampleHorizonMu_FromUSky(float uSky, FovSlice slice, Texture1D<uint> HorizonMu)
{
    if (!InSlice(uSky, slice))
        return -2.0f; // strictly outside FOV

    float fovU = SliceU(uSky, slice); // [0,1] across frustum arc

    uint N;
    HorizonMu.GetDimensions(N);
    float x = fovU * (N - 1e-4f);

    uint i0 = (uint) floor(x);
    uint i1 = min(i0 + 1, N - 1);
    float f = frac(x);

    bool ok0, ok1;
    float mu0 = DecodeMu(HorizonMu.Load(int2(i0, 0), 0), ok0);
    float mu1 = DecodeMu(HorizonMu.Load(int2(i1, 0), 0), ok1);

    if (!ok0 && !ok1)
        return -3.0f; // column(s) never written
    if (!ok0)
    {
        mu0 = mu1;
        f = 1.0f;
    }
    if (!ok1)
    {
        mu1 = mu0;
        f = 0.0f;
    }

    return lerp(mu0, mu1, f);
}

// invert your LatitudeFromV to get v from μ
float V_fromMu(float mu)
{
    mu = clamp(mu, -1.0f, 1.0f);
    float lat = asin(mu); // [-π/2, π/2]
    float a = sqrt(2.0f * abs(lat) / PI); // a ∈ [0,1]
    float s = (lat >= 0.0f) ? 1.0f : -1.0f;
    float v = 0.5f + 0.5f * s * a;
#if SKY_FLIP_Y
    v = 1.0f - v;
#endif
    return saturate(v);
}

// --- same decode/fallback helpers you already use ---

float SphericalMuH(float rCam, float Rg, float minH)
{
    float Rb = Rg + min(0.0f, minH);
    float s = saturate(Rb / rCam);
    return -sqrt(max(1.0f - s * s, 0.0f));
}

float RblockFromMu(float muH, float rCam, float Rg)
{
    // If μH is the sentinel (< -1.5f), caller should pass a fallback μ first.
    muH = clamp(muH, -1.0f, 1.0f);
    float Rb = rCam * sqrt(saturate(1.0f - muH * muH));
    return max(Rb, Rg);
}

// x in [0,1] if wWorld is inside the current frustum; else -1
float ScreenX01_fromBasis_full(float3 wWorld)
{
    float3 r = normalize(inverseViewMatrix[0].xyz);
    float3 u = normalize(inverseViewMatrix[1].xyz);
    float3 f = normalize(inverseViewMatrix[2].xyz);

    // D3D: +Z forward (LH) → _33 >= 0, -Z forward (RH) → _33 < 0
    float fwdSign = (projectionMatrix._33 >= 0.0f) ? +1.0f : -1.0f;

    float z = dot(wWorld, f) * fwdSign;
    if (z <= 1e-6f)
        return -1.0f; // behind

    float x = dot(wWorld, r);
    float ndcX = (x / z) * projectionMatrix._11; // _11 = 1/tan(fovx/2)
    if (abs(ndcX) > 1.0f)
        return -1.0f; // outside horizontally

    return 0.5f * (ndcX + 1.0f); // 0..1 across the screen
}

float SampleHorizonMu_Screen_NoFallback(float3 wView, uint fovStart, uint fovEnd)
{
    float uX = ScreenX01_fromBasis_full(wView);
    if (uX < 0.0f)
        return -2.0f; // outside FOV

    uint N;
    HorizonMu.GetDimensions(N);

    // Map wView to HorizonLUT index
    uint idx = (uint) (uX * (N - 1));

    // Clamp to current FOV slice
    idx = clamp(idx, fovStart, fovEnd);

    // Remap index → [0,1] across FOV slice
    uint fovWidth = max(1u, fovEnd - fovStart);
    float fovU = float(idx - fovStart) / float(fovWidth);

    // Use fovU to blend HorizonMu entries
    float x = fovU * (N - 1e-4f);
    uint i0 = (uint) floor(x);
    uint i1 = min(i0 + 1, N - 1);
    float f = frac(x);

    bool ok0, ok1;
    float mu0 = DecodeMu(HorizonMu.Load(i0), ok0);
    float mu1 = DecodeMu(HorizonMu.Load(i1), ok1);

    if (!ok0 && !ok1)
        return -3.0f; // no data

    if (!ok0)
    {
        mu0 = mu1;
        f = 1;
    }
    if (!ok1)
    {
        mu1 = mu0;
        f = 0;
    }

    return lerp(mu0, mu1, f);
}

float ord2f(uint ox, out bool ok)
{
    if (ox == 0u)
    {
        ok = false;
        return 0.0;
    }
    uint m = (ox & 0x80000000u) ? 0x80000000u : 0xffffffffu;
    ok = true;
    return asfloat(ox ^ m);
}

float DensityRayleigh(float h)
{
    return exp(-max(h, 0.0f) / max(RayScaleHeight, 1e-3f));
}
float DensityMie(float h)
{
    return exp(-max(h, 0.0f) / max(MieScaleHeight, 1e-3f));
}

float PhaseRayleigh(float mu)
{
    return (3.0f * INV4PI) * 0.25f * (1.0f + mu * mu);
}
float PhaseMieHG(float mu, float g)
{
    float g2 = g * g;
    float d = 1.0f + g2 - 2.0f * g * mu;
    return INV4PI * (1.0f - g2) / max(pow(d, 1.5f), 1e-5f);
}

float2 TransUV(float r, float mu, float Rb, float Rt)
{
    float rNorm = (r - Rb) / max(Rt - Rb, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (Rb * Rb) / (r * r)));
    mu = clamp(mu, muMin + 1.0e-5f, 1.0f - 1.0e-5f);
    float uMu = (mu - muMin) / (1.0f - muMin);
    return float2(uMu, saturate(rNorm));
}

float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
}

float3 T_along_ray(float r, float mu, float t, float Rb, float Rt)
{
    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
    rd = clamp(rd, Rb, Rt);
    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
    float3 num = T_to_TOA(r, mu, Rb, Rt);
    float3 den = T_to_TOA(rd, muD, Rb, Rt);
    return saturate(num / max(den, 1e-6.xxx));
}

float DistToTop(float r, float mu, float Rt)
{
    float d = r * r * (mu * mu - 1) + Rt * Rt;
    return max(-r * mu + sqrt(max(d, 0)), 0);
}

float DistToBottom(float r, float mu, float Rb)
{
    float d = r * r * (mu * mu - 1) + Rb * Rb;
    return max(-r * mu - sqrt(max(d, 0)), 0);
}

bool HitsGround(float r, float mu, float Rb)
{
    return (mu < 0.0f) && (r * r * (mu * mu - 1) + Rb * Rb >= 0.0f);
}

// MultiScatter LUT sampling: x=theta_s/π, y = 1 - linear altitude (top=TOA)
float4 SamplePsiMS4(float r, float muS, float Rb, float Rt)
{
    float thetaS = acos(clamp(muS, -1.0f, 1.0f));
    float u = thetaS / PI;
    float v = 1.0f - saturate((r - Rb) / max(Rt - Rb, 1e-6f)); // MS_FLIP_Y=1
    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0);
}
float3 SamplePsiMS(float r, float muS, float Rg, float Rt)
{
    return SamplePsiMS4(r, muS, Rg, Rt).rgb;
}

float SunVisibilityAtR(float r, float muS, float Rb)
{
    const float SunAngularRadius = 0.004675f;
    float sH = Rb / r;
    float cH = -sqrt(saturate(1.0f - sH * sH));
    return smoothstep(-sH * SunAngularRadius, sH * SunAngularRadius, muS - cH);
}

// lat/lon mapping
float LatitudeFromV(float v)
{
#if SKY_FLIP_Y
    v = 1.0f - v;
#endif
    float s = 2.0f * (v - 0.5f);
    float a = abs(s);
    float l_abs = (PI * 0.5f) * (a * a);
    return (s >= 0.0f) ? l_abs : -l_abs;
}
float LongitudeFromU(float u)
{
    return lerp(-PI, PI, saturate(u));
}

float3 DirFromLonLat(float lon, float lat, float3 east, float3 north, float3 up)
{
    float cl = cos(lat), sl = sin(lat);
    float ce = cos(lon), se = sin(lon);
    return normalize(cl * (ce * east + se * north) + sl * up);
}

// MS anisotropy blend using LUT alpha as gBar
float MSPhase(float mu, float gBar)
{
    float pIso = INV4PI;
    float g = saturate(gBar);
    float g2 = g * g;
    float d = 1.0f + g2 - 2.0f * g * mu;
    float pHG = INV4PI * (1.0f - g2) / max(pow(d, 1.5f), 1e-5f);
    return lerp(pIso, pHG, g);
}

void BuildSkyBasis(float3 camWS, float3 planetCenter, float3 spinUpWS, out float3 up, out float3 east, out float3 north)
{
    up = normalize(camWS - planetCenter); // true radial up at camera
    float3 spinT = spinUpWS - up * dot(spinUpWS, up); // remove vertical
    float len2 = max(dot(spinT, spinT), 1e-20f);
    float3 northHint = spinT * rsqrt(len2); // tangent north hint

    east = normalize(cross(northHint, up)); // exact orthonormal
    north = normalize(cross(up, east)); // re-derive north
}

// ===== Main =================================================================
[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint W, H;
    OutSkyView.GetDimensions(W, H);
    if (tid.x >= W || tid.y >= H)
        return;
    
    // SkyView coords for this pixel
    float uSky = (tid.x + 0.5f) / float(W);
    float vSky = (tid.y + 0.5f) / float(H);

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float Rb = PlanetRadius + min(0.0f, MinHeight);

    float3 camWS = cameraPosition.xyz;
    float3 camRel = camWS - PlanetCenterWS;
    float rCam = max(Rg, length(camRel));
    
    float3 up, east, north;
    BuildSkyBasis(camWS, PlanetCenterWS, BasisSpinUp, up, east, north);

    float lon = lerp(-PI, PI, saturate(uSky));
    vSky = 1.0f - vSky;
    float s = 2.0f * (vSky - 0.5f);
    float a = abs(s);
    float lat = (PI * 0.5f) * (a * a) * (s >= 0 ? 1.0f : -1.0f);

    float3 wView = DirFromLonLat(lon, lat, east, north, up);
    float muV = dot(wView, up);
   
    float3 wSun = -normalize(direction.xyz);
    float3 Esun = radiance.rgb * SunIntensity;

    // Terrain occluded if below horizon
    bool isGround = HitsGround(rCam, muV, Rb);

    if (!isGround)
    {
        float tEnd = DistToTop(rCam, muV, Rt);
        float3 Ls = 0, Lms = 0;

        [loop]
        for (uint i = 0; i < SKY_STEPS; ++i)
        {
            float a0 = float(i) / float(SKY_STEPS);
            float a1 = float(i + 1) / float(SKY_STEPS);
            float t0 = a0 * a0 * tEnd;
            float t1 = a1 * a1 * tEnd;
            float ti = 0.5f * (t0 + t1);
            float dt = (t1 - t0);

            float3 pRel = camRel + wView * ti;
            float rp = max(Rb, length(pRel)); // << clamp by Rb
            float h = max(0.0f, rp - Rb); // << height above blocking radius

            float3 Tvp = T_along_ray(rCam, muV, ti, Rb, Rt); // << Rb
            
            float dR = DensityRayleigh(h);
            float dM = DensityMie(h);
            float3 sigR_s = RayleighScattering * dR;
            float3 sigM_s = MieScattering * dM;
            float3 sigS = sigR_s + sigM_s;

            float3 upS = pRel / rp;
            float muS = dot(upS, wSun);
            float Vsun = SunVisibilityAtR(rp, muS, Rb); // << Rb
            float3 Tsun = T_to_TOA(rp, muS, Rb, Rt) * Vsun; // << Rb

            float muPhase = clamp(dot(wSun, wView), -0.9995f, 0.9995f);
            float PR = PhaseRayleigh(muPhase);
            float PM = PhaseMieHG(muPhase, saturate(MieAnisotropy));

            // single scattering
            if (SKY_USE_RAY)
                Ls += Tvp * (sigR_s * PR * Tsun) * dt;
            if (SKY_USE_MIE)
                Ls += Tvp * (sigM_s * PM * Tsun) * dt;

            // multiple scattering: use Rb in the LUT sampling too
            float4 Psi4 = SamplePsiMS4(rp, muS, Rb, Rt); // << Rb
            float pMS = MSPhase(muPhase, Psi4.a);
            if (SKY_USE_MS)
                Lms += Tvp * ((sigS * Psi4.rgb) * pMS) * dt;
        }

        float3 L = (Ls + Lms) * Esun;
        OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);
        return;
    }
    else
    {
        // Ground pixel: integrate to ground at Rb and light it
        float dHit = DistToBottom(rCam, muV, Rb); // << Rb
        float3 Tcg = T_along_ray(rCam, muV, dHit, Rb, Rt);

        float3 pG = camRel + wView * dHit;
        float rG = max(Rb, length(pG));
        float3 upG = pG / rG;

        float muSg = dot(upG, wSun);
        float Vsg = SunVisibilityAtR(rG, muSg, Rb); // << Rb
        float3 Tsg = T_to_TOA(rG, muSg, Rb, Rt) * Vsg; // << Rb

        float cosNL = max(muSg, 0.0f);
        float3 Lo = (GroundAlbedo / PI) * Tsg * cosNL;

        float4 PsiG4 = SamplePsiMS4(rG, muSg, Rb, Rt); // << Rb
        float pMSg = MSPhase(clamp(dot(wSun, wView), -0.9995f, 0.9995f), PsiG4.a);
        float3 ImsG = (RayleighScattering + MieScattering) * pMSg * PsiG4.rgb;
         
        float3 L = Tcg * (Lo + ImsG) * Esun;
        OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);
        return;
    }
}

