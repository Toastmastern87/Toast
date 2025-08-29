//#type compute
//#pragma pack_matrix(row_major)

//// ---------- preview/debug ----------
//#ifndef SKY_FLIP_Y
//#define SKY_FLIP_Y 1          // just for preview: horizon in the middle, zenith at top
//#endif
//#ifndef SKY_ENABLE_GROUND
//#define SKY_ENABLE_GROUND 1
//#endif
//#ifndef SKY_STEPS
//#define SKY_STEPS 32          // 24–48 is plenty
//#endif

//// ---------- cbuffers ----------
//cbuffer Camera : register(b0)
//{
//    float4x4 worldTranslationMatrix;
//    float4x4 viewMatrix;
//    float4x4 projectionMatrix;
//    float4x4 inverseViewMatrix;
//    float4x4 inverseProjectionMatrix;
//    float4 cameraPosition; // .xyz used
//    float farZ;
//    float nearZ;
//    float viewportWidth;
//    float viewportHeight;
//};

//cbuffer DirectionalLight : register(b3)
//{
//    float4x4 lightViewProj;
//    float4 direction; // convention: FROM light -> scene
//    float4 radiance; // RGB intensity scale
//    float multiplier;
//};

//cbuffer PlanetFrame : register(b4)
//{
//    float3 PlanetCenterWS;
//    float PlanetRadius; // Rg
//    float3 BasisTanEast; // local tangent East
//    float MaxHeight; // (unused)
//    float3 BasisTanNorth; // local tangent North
//    float MinHeight; // (unused)
//    float3 BasisRadUp; // local Up at camera (tangent frame)
//};

//cbuffer Atmosphere : register(b5)
//{
//    float AtmosphereHeight; // Rt - Rg
//    float RayScaleHeight; // Hr
//    float MieScaleHeight; // Hm
//    float MieAnisotropy; // g
//    float3 RayleighScattering; // beta_R (1/m)
//    float3 MieScattering; // beta_Ms (1/m)
//    float3 MieAbsorption; // beta_Ma (1/m)
//    float3 GroundAlbedo;
//    float OzoneStrength; // (unused here)
//    uint StepsTransmittance; // (unused here)
//    uint StepsMultiScattering; // (unused here)
//    float APFarDynamic; // (unused)
//};

//// ---------- LUTs ----------
//Texture2D<float4> TransmittanceLUT : register(t0);
//Texture2D<float4> MultiScatterLUT : register(t1);
//SamplerState ClampLinear : register(s0);
//RWTexture2D<float4> OutSkyView : register(u0);

//// ---------- constants ----------
//static const float PI = 3.14159265358979323846f;
//static const float INV4PI = 0.25f / PI;
//static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

//// ---------- densities ----------
//float DensityRayleigh(float h)
//{
//    return exp(-max(h, 0.0f) / max(RayScaleHeight, 1e-3f));
//}
//float DensityMie(float h)
//{
//    return exp(-max(h, 0.0f) / max(MieScaleHeight, 1e-3f));
//}

//// ---------- phases ----------
//float PhaseRayleigh(float mu)
//{
//    // 3/(16π) * (1 + mu^2)
//    return (3.0f * INV4PI) * 0.25f * (1.0f + mu * mu);
//}
//float PhaseMieHG(float mu, float g)
//{
//    float g2 = g * g;
//    float d = 1.0f + g2 - 2.0f * g * mu;
//    return INV4PI * (1.0f - g2) / max(pow(d, 1.5f), 1e-5f);
//}

//// ---------- TLUT mapping ----------
//float2 TransUV(float r, float mu, float Rg, float Rt)
//{
//    float rNorm = (r - Rg) / max(Rt - Rg, 1e-6f);
//    float muMin = -sqrt(saturate(1.0f - (Rg * Rg) / (r * r)));
//    float uMu = (mu - muMin) / (1.0f - muMin);
//    return saturate(float2(uMu, rNorm));
//}
//float3 T_to_TOA(float r, float mu, float Rg, float Rt)
//{
//    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rg, Rt), 0).rgb;
//}

//// segment transmittance camera->point at distance t (ratio trick)
//float3 T_along_ray(float r, float mu, float t, float Rg, float Rt)
//{
//    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
//    rd = clamp(rd, Rg, Rt);
//    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
//    float3 num = T_to_TOA(r, mu, Rg, Rt);
//    float3 den = T_to_TOA(rd, muD, Rg, Rt);
//    return saturate(num / max(den, float3(1e-6f, 1e-6f, 1e-6f)));
//}

//// distances in spherical shell
//float DistToTop(float r, float mu, float Rt)
//{
//    float d = r * r * (mu * mu - 1.0f) + Rt * Rt;
//    return max(-r * mu + sqrt(max(d, 0.0f)), 0.0f);
//}
//float DistToBottom(float r, float mu, float Rg)
//{
//    float d = r * r * (mu * mu - 1.0f) + Rg * Rg;
//    return max(-r * mu - sqrt(max(d, 0.0f)), 0.0f);
//}
//bool HitsGround(float r, float mu, float Rg)
//{
//    return (mu < 0.0f) && (r * r * (mu * mu - 1.0f) + Rg * Rg >= 0.0f);
//}

//// MS LUT sampling (u = theta_s/π, v = (r-Rg)/(Rt-Rg))
//float3 SamplePsiMS(float r, float muS, float Rg, float Rt)
//{
//    float thetaS = acos(clamp(muS, -1.0f, 1.0f));
//    float u = thetaS / PI;
//    float v = saturate((r - Rg) / max(Rt - Rg, 1e-6f));
//    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0).rgb;
//}

//// small horizon softening for sun visibility
//float SunVisibilityAtR(float r, float muS, float Rg)
//{
//    const float SunAngularRadius = 0.00935f; // ~0.535°
//    float sinThetaH = Rg / r;
//    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
//    return smoothstep(-sinThetaH * SunAngularRadius,
//                       sinThetaH * SunAngularRadius,
//                       muS - cosThetaH);
//}

//// inverse non-linear latitude mapping (paper)
//float LatitudeFromV(float v)
//{
//#if SKY_FLIP_Y
//    v = 1.0f - v; // preview flip only
//#endif
//    float s = 2.0f * (v - 0.5f); // [-1,1]
//    float l_abs = (PI * 0.5f) * (abs(s) * abs(s));
//    return (s >= 0.0f) ? l_abs : -l_abs; // [-π/2, +π/2]
//}
//float LongitudeFromU(float u)
//{
//    return lerp(-PI, PI, saturate(u));
//}

//// build view direction from lon/lat in the camera-aligned ground frame
//float3 DirFromLonLat(float lon, float lat, float3 east, float3 north, float3 up)
//{
//    float cl = cos(lat), sl = sin(lat);
//    float ce = cos(lon), se = sin(lon);
//    return normalize(cl * (ce * east + se * north) + sl * up);
//}

//// ---------- main ----------
//[numthreads(8, 8, 1)]
//void main(uint3 tid : SV_DispatchThreadID)
//{
//    uint W, H;
//    OutSkyView.GetDimensions(W, H);
//    if (tid.x >= W || tid.y >= H)
//        return;

//    // Radii
//    const float Rg = PlanetRadius;
//    const float Rt = PlanetRadius + AtmosphereHeight;

//    // Camera & local frame (planet-centered coordinates)
//    const float3 camWS = cameraPosition.xyz;
//    const float3 camRel = camWS - PlanetCenterWS; // <- FIX: planet-centered
//    float rCam = length(camRel);
//    float3 upCam = (rCam > 0.0f) ? camRel / rCam : normalize(BasisRadUp);

//    // Use the provided tangent frame (should be orthonormal to upCam)
//    float3 east = normalize(BasisTanEast);
//    float3 north = normalize(BasisTanNorth);

//    // Sun (FROM point -> sun)
//    float3 wSun = -normalize(direction.xyz);

//    // Texture param (longitude, latitude)
//    float u = (tid.x + 0.5f) / float(W);
//    float v = (tid.y + 0.5f) / float(H);
//    float lon = LongitudeFromU(u); // [-π, π]
//    float lat = LatitudeFromV(v); // [-π/2, π/2], 0=horizon

//    // View direction in planet frame
//    float3 wView = DirFromLonLat(lon, lat, east, north, upCam);

//    // cos with Up at camera
//    float muV = dot(wView, upCam);

//    // Ray bounds (to ground or to TOA)
//    bool groundHit = HitsGround(rCam, muV, Rg);
//    float dExit = groundHit ? DistToBottom(rCam, muV, Rg)
//                                : DistToTop(rCam, muV, Rt);

//    float3 L = 0.0f; // final radiance

//#if SKY_ENABLE_GROUND
//    // Ground branch: shade lambert ground if we look below the horizon
//    if (groundHit && lat < 0.0f)
//    {
//        // camera->ground transmittance
//        float3 Tcg = T_along_ray(rCam, muV, dExit, Rg, Rt);

//        // ground point in planet-centered frame
//        float3 pGRel = camRel + wView * dExit;
//        float rG = max(length(pGRel), Rg);
//        float3 upG = pGRel / rG; // <- FIX: define upG

//        // sun at ground (direct)
//        float muSg = dot(upG, wSun);
//        float Vsg = SunVisibilityAtR(rG, muSg, Rg);
//        float3 Tsg = T_to_TOA(rG, muSg, Rg, Rt) * Vsg;

//        float cosNL = max(muSg, 0.0f);
//        float3 LoG = (GroundAlbedo / PI) * Tsg * cosNL;

//        // MS ambient at ground (do NOT gate with Vsg again)
//        float3 PsiMSg = SamplePsiMS(rG, muSg, Rg, Rt);

//        L = Tcg * (LoG + PsiMSg);
//        L *= radiance.rgb * multiplier; // apply light scaling once
//        OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);
//        return;
//    }
//#endif

//    // Integrate sky in-scattering along the ray
//    float tEnd = dExit;
//    float3 Ls = 0.0f; // single scattering
//    float3 Lms = 0.0f; // multiple scattering

//    [loop]
//    for (uint i = 0; i < SKY_STEPS; ++i)
//    {
//        float a0 = float(i) / float(SKY_STEPS);
//        float a1 = float(i + 1) / float(SKY_STEPS);
//        float t0 = a0 * a0 * tEnd;
//        float t1 = a1 * a1 * tEnd;
//        float ti = 0.5f * (t0 + t1);
//        float dt = (t1 - t0);

//        // sample point in planet-centered coords
//        float3 pRel = camRel + wView * ti; // <- FIX: use camRel, not 'pc'
//        float rS = length(pRel);
//        float3 upS = (rS > 0.0f) ? (pRel / rS) : upCam;
//        float hS = max(0.0f, rS - Rg);

//        // camera -> sample transmittance
//        float3 Tvp = T_along_ray(rCam, muV, ti, Rg, Rt);

//        // local optical properties
//        float3 sigR_s = RayleighScattering * DensityRayleigh(hS);
//        float3 sigM_s = MieScattering * DensityMie(hS);

//        // sun at the sample (per-sample!)
//        float muS = dot(upS, wSun);
//        float Vsun = SunVisibilityAtR(rS, muS, Rg); // occlusion by planet
//        float3 Tps = T_to_TOA(rS, muS, Rg, Rt) * Vsun;

//        // scattering angles (sun->point, point->camera)
//        float muPhase = dot(wSun, -wView);
//        float PR = PhaseRayleigh(muPhase);
//        float PM = PhaseMieHG(muPhase, saturate(MieAnisotropy));

//        // single scattering (gate by Vsun)
//        float3 S1 = (sigR_s * PR + sigM_s * PM) * Tps;

//        // multiple scattering from LUT — DO NOT gate again (twilight!)
//        float3 Psi = SamplePsiMS(rS, muS, Rg, Rt);

//        Ls += Tvp * S1 * dt;
//        Lms += Tvp * Psi * dt;
//    }

//    L = (Ls + Lms) * radiance.rgb * multiplier;
//    OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);
//}

//#type compute
//#pragma pack_matrix(row_major)

//// ===== Debug toggles =======================================================
//#ifndef SKY_FLIP_Y
//#define SKY_FLIP_Y            1     // preview flip only (does not change mapping)
//#endif
//#ifndef SKY_ENABLE_GROUND
//#define SKY_ENABLE_GROUND     1
//#endif
//#ifndef SKY_STEPS
//#define SKY_STEPS             32    // per-ray steps
//#endif
//#ifndef SKY_DEBUG_MODE
//#define SKY_DEBUG_MODE        0     // see table in the message
//#endif
//#ifndef SKY_DBG_SCALE
//#define SKY_DBG_SCALE         1.0f  // extra scale after tone-map in modes 1–2
//#endif
//#ifndef SKY_USE_MS
//#define SKY_USE_MS        1   // 0 = disable multi-scatter term
//#endif
//#ifndef SKY_USE_MIE
//#define SKY_USE_MIE       1   // 0 = disable Mie single scatter
//#endif
//#ifndef SKY_USE_RAY
//#define SKY_USE_RAY       1   // 0 = disable Rayleigh single scatter
//#endif

//// ===== CBuffers ============================================================
//cbuffer Camera : register(b0)
//{
//    float4x4 worldTranslationMatrix;
//    float4x4 viewMatrix;
//    float4x4 projectionMatrix;
//    float4x4 inverseViewMatrix;
//    float4x4 inverseProjectionMatrix;
//    float4 cameraPosition; // .xyz used
//    float farZ;
//    float nearZ;
//    float viewportWidth;
//    float viewportHeight;
//};

//cbuffer DirectionalLight : register(b3)
//{
//    float4x4 lightViewProj;
//    float4 direction; // FROM light -> scene
//    float4 radiance; // RGB
//    float multiplier; // scalar intensity
//};

//cbuffer PlanetFrame : register(b4)
//{
//    float3 PlanetCenterWS;
//    float PlanetRadius; // Rg
//    float3 BasisTanEast;
//    float MaxHeight; // (unused)
//    float3 BasisTanNorth;
//    float MinHeight; // (unused)
//    float3 BasisRadUp; // local Up (radial) at camera
//};

//cbuffer Atmosphere : register(b5)
//{
//    float AtmosphereHeight; // Rt - Rg
//    float RayScaleHeight; // Hr
//    float MieScaleHeight; // Hm
//    float MieAnisotropy; // g
//    float3 RayleighScattering; // beta_R (1/m) RGB
//    float3 MieScattering; // beta_Ms (1/m) RGB
//    float3 MieAbsorption; // beta_Ma (1/m) RGB
//    float3 GroundAlbedo; // constant ground albedo
//    float OzoneStrength; // (unused here)
//    uint StepsTransmittance; // (unused here)
//    uint StepsMultiScattering;
//    float APFarDynamic;
//};

//// ===== LUTs ================================================================
//Texture2D<float4> TransmittanceLUT : register(t0);
//Texture2D<float4> MultiScatterLUT : register(t1);
//SamplerState ClampLinear : register(s0);
//RWTexture2D<float4> OutSkyView : register(u0);

//// ===== Constants / helpers =================================================
//static const float PI = 3.14159265358979323846f;
//static const float INV4PI = 0.25f / PI;
//static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

//float DensityRayleigh(float h)
//{
//    return exp(-max(h, 0.0f) / max(RayScaleHeight, 1e-3f));
//}
//float DensityMie(float h)
//{
//    return exp(-max(h, 0.0f) / max(MieScaleHeight, 1e-3f));
//}

//float PhaseRayleigh(float mu)
//{
//    return (3.0f * INV4PI) * 0.25f * (1.0f + mu * mu);
//}
//float PhaseMieHG(float mu, float g)
//{
//    float g2 = g * g;
//    float d = 1.0f + g2 - 2.0f * g * mu;
//    return INV4PI * (1.0f - g2) / max(pow(d, 1.5f), 1e-5f);
//}

//// --- TLUT mapping (Bruneton/UE) -------------------------------------------
//float2 TransUV(float r, float mu, float Rg, float Rt)
//{
//    float rNorm = (r - Rg) / max(Rt - Rg, 1e-6f);
//    float muMin = -sqrt(saturate(1.0f - (Rg * Rg) / (r * r)));
//    float uMu = (mu - muMin) / (1.0f - muMin);
//    return saturate(float2(uMu, rNorm));
//}
//float3 T_to_TOA(float r, float mu, float Rg, float Rt)
//{
//    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rg, Rt), 0).rgb;
//}

//// segment transmittance to a point at parametric distance t
//float3 T_along_ray(float r, float mu, float t, float Rg, float Rt)
//{
//    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
//    rd = clamp(rd, Rg, Rt);
//    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
//    float3 num = T_to_TOA(r, mu, Rg, Rt);
//    float3 den = T_to_TOA(rd, muD, Rg, Rt);
//    return saturate(num / max(den, float3(1e-6f, 1e-6f, 1e-6f)));
//}

//// distances in spherical shell
//float DistToTop(float r, float mu, float Rt)
//{
//    float d = r * r * (mu * mu - 1.0f) + Rt * Rt;
//    return max(-r * mu + sqrt(max(d, 0.0f)), 0.0f);
//}
//float DistToBottom(float r, float mu, float Rg)
//{
//    float d = r * r * (mu * mu - 1.0f) + Rg * Rg;
//    return max(-r * mu - sqrt(max(d, 0.0f)), 0.0f);
//}
//bool HitsGround(float r, float mu, float Rg)
//{
//    return (mu < 0.0f) && (r * r * (mu * mu - 1.0f) + Rg * Rg >= 0.0f);
//}

//// MS LUT sampling: u = theta_s/π, v = linear altitude
//float3 SamplePsiMS(float r, float muS, float Rg, float Rt)
//{
//    float thetaS = acos(clamp(muS, -1.0f, 1.0f));
//    float u = thetaS / PI;
//    float v = saturate((r - Rg) / max(Rt - Rg, 1e-6f));
//    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0).rgb;
//}

//// small horizon softening
//float SunVisibilityAtR(float r, float muS, float Rg)
//{
//    const float SunAngularRadius = 0.00935f; // ~0.535°
//    float sinThetaH = Rg / r;
//    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
//    return smoothstep(-sinThetaH * SunAngularRadius, sinThetaH * SunAngularRadius,
//                      muS - cosThetaH);
//}

//// paper's non-linear latitude mapping (inverse: v -> l)
//float LatitudeFromV(float v)
//{
//#if SKY_FLIP_Y
//    v = 1.0f - v;
//#endif
//    float s = 2.0f * (v - 0.5f); // [-1,1]
//    float a = abs(s);
//    float l_abs = (PI * 0.5f) * (a * a);
//    return (s >= 0.0f) ? l_abs : -l_abs;
//}
//float LongitudeFromU(float u)
//{
//    return lerp(-PI, PI, saturate(u));
//}

//// direction from (lon,lat) in local tangent frame
//float3 DirFromLonLat(float lon, float lat, float3 east, float3 north, float3 up)
//{
//    float cl = cos(lat), sl = sin(lat);
//    float ce = cos(lon), se = sin(lon);
//    return normalize(cl * (ce * east + se * north) + sl * up);
//}

//// ===== Main =================================================================
//[numthreads(8, 8, 1)]
//void main(uint3 tid : SV_DispatchThreadID)
//{
//    uint W, H;
//    OutSkyView.GetDimensions(W, H);
//    if (tid.x >= W || tid.y >= H)
//        return;

//    // radii and local frame
//    const float Rg = PlanetRadius;
//    const float Rt = PlanetRadius + AtmosphereHeight;

//    float3 up = normalize(BasisRadUp);
//    float3 east = normalize(BasisTanEast - up * dot(BasisTanEast, up)); // reproject
//    float3 north = normalize(cross(up, east)); // make it orthonormal

//    // camera in planet-centered frame
//    float3 camWS = cameraPosition.xyz;
//    float3 camRel = camWS - PlanetCenterWS;
//    float rCam = max(Rg, length(camRel));

//    // sun dir FROM point -> sun
//    float3 wSun = -normalize(direction.xyz);

//    // (u,v) -> (lon,lat)
//    float u = (tid.x + 0.5f) / float(W);
//    float v = (tid.y + 0.5f) / float(H);
//    float lon = LongitudeFromU(u);
//    float lat = LatitudeFromV(v);

//    // view direction
//    float3 wView = DirFromLonLat(lon, lat, east, north, up);

//    // cosines
//    float muV = dot(wView, up); // camera up
//    float muS0 = dot(wSun, up); // row-wise sun zenith cosine (for reference)

//    // intersections
//    bool groundHit = HitsGround(rCam, muV, Rg);
//    float dExit = groundHit ? DistToBottom(rCam, muV, Rg)
//                                : DistToTop(rCam, muV, Rt);

//#if SKY_ENABLE_GROUND
//    // simple ground shading (early return for ground half)
//    if (groundHit && lat < 0.0f)
//    {
//        float3 Tcg = T_along_ray(rCam, muV, dExit, Rg, Rt);

//        float3 pG = camRel + wView * dExit;
//        float rG = max(Rg, length(pG));
//        float3 upG = pG / rG;
//        float muSg = dot(upG, wSun);
//        float Vsg = SunVisibilityAtR(rG, muSg, Rg);
//        float3 Tsg = T_to_TOA(rG, muSg, Rg, Rt) * Vsg;

//        float3 Lo = (GroundAlbedo / PI) * Tsg;
//        float3 PsiG = SamplePsiMS(rG, muSg, Rg, Rt);

//        float3 Lg = Tcg * (Lo + PsiG) * radiance.rgb * multiplier;
//        OutSkyView[tid.xy] = float4(max(Lg, 0.0f), 1.0f);
//        return;
//    }
//#endif

//    // integrate sky toward TOA
//    float tEnd = dExit;
//    float3 Ls = 0.0f; // single scattering
//    float3 Lms = 0.0f; // multi scattering

//    // --- debug accumulators (do not affect lighting) ---
//    float accW = 0.0f;
//    float dbg_Vsun = 0.0f;
//    float dbg_muS = 0.0f;
//    float dbg_muPh = 0.0f;
//    float dbg_TvpLum = 0.0f;

//    [loop]
//    for (uint i = 0; i < SKY_STEPS; ++i)
//    {
//        float a0 = float(i) / float(SKY_STEPS);
//        float a1 = float(i + 1) / float(SKY_STEPS);
//        float t0 = a0 * a0 * tEnd;
//        float t1 = a1 * a1 * tEnd;
//        float ti = 0.5f * (t0 + t1);
//        float dt = (t1 - t0);

//        float3 pRel = camRel + wView * ti;
//        float rp = max(Rg, length(pRel));
//        float h = max(0.0f, rp - Rg);

//        float3 Tvp = T_along_ray(rCam, muV, ti, Rg, Rt);

//        float dR = DensityRayleigh(h);
//        float dM = DensityMie(h);
//        float3 sigR_s = RayleighScattering * dR;
//        float3 sigM_s = MieScattering * dM;

//        float3 upS = pRel / rp;
//        float muS = dot(upS, wSun);
//        float Vsun = SunVisibilityAtR(rp, muS, Rg);
//        float3 Tps = T_to_TOA(rp, muS, Rg, Rt) * Vsun;

//        float muPhase = dot(wSun, -wView);
//        float PR = PhaseRayleigh(muPhase);
//        float PM = PhaseMieHG(muPhase, saturate(MieAnisotropy));

//        float3 S1 = (sigR_s * PR + sigM_s * PM) * Tps;
//        float3 Psi = SamplePsiMS(rp, muS, Rg, Rt);
//        float3 Sms = Psi * Vsun;

//        float3 S_R = sigR_s * PR * Tps; // Rayleigh single
//        float3 S_M = sigM_s * PM * Tps; // Mie single
//        float3 S_MS = SamplePsiMS(rp, muS, Rg, Rt) * Vsun; // multi-scatter (no extra Tps)
        
//        if (SKY_USE_RAY)
//            Ls += Tvp * S_R * dt;
//        if (SKY_USE_MIE)
//            Ls += Tvp * S_M * dt;
//        if (SKY_USE_MS)
//            Lms += Tvp * S_MS * dt;
        
//        // ---- collect debug stats ----
//        accW += dt;
//        dbg_Vsun += Vsun * dt;
//        dbg_muS += muS * dt;
//        dbg_muPh += muPhase * dt;
//        dbg_TvpLum += dot(Tvp, LUMA) * dt;
//    }

//    float3 L = (Ls + Lms) * radiance.rgb * multiplier;

//    // ---- choose debug output (no effect on normal path) -------------------
//#if   SKY_DEBUG_MODE == 0
//    OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);

//#elif SKY_DEBUG_MODE == 1
//    // single scattering (tone-mapped)
//    float3 C = Ls * radiance.rgb * multiplier * SKY_DBG_SCALE;
//    C = C / (1.0f + C);
//    OutSkyView[tid.xy] = float4(saturate(C), 1.0f);

//#elif SKY_DEBUG_MODE == 2
//    // multi scattering (tone-mapped)
//    float3 C = Lms * radiance.rgb * multiplier * SKY_DBG_SCALE;
//    C = C / (1.0f + C);
//    OutSkyView[tid.xy] = float4(saturate(C), 1.0f);

//#elif SKY_DEBUG_MODE == 3
//    // T(camera -> boundary) luminance
//    float3 Tend = T_along_ray(rCam, muV, dExit, Rg, Rt);
//    float  g = saturate(dot(Tend, LUMA));
//    OutSkyView[tid.xy] = float4(g, g, g, 1.0f);

//#elif SKY_DEBUG_MODE == 4
//    // path-avg sun visibility
//    float g = (accW > 0.0f) ? dbg_Vsun / accW : 0.0f;
//    OutSkyView[tid.xy] = float4(g, g, g, 1.0f);

//#elif SKY_DEBUG_MODE == 5
//    // path-avg mu_s mapped to [0,1]
//    float g = (accW > 0.0f) ? 0.5f * (dbg_muS / accW + 1.0f) : 0.0f;
//    OutSkyView[tid.xy] = float4(g, g, g, 1.0f);

//#elif SKY_DEBUG_MODE == 6
//    // path-avg phase cosine (sun vs -view) mapped to [0,1]
//    float g = (accW > 0.0f) ? 0.5f * (dbg_muPh / accW + 1.0f) : 0.0f;
//    OutSkyView[tid.xy] = float4(g, g, g, 1.0f);

//#elif SKY_DEBUG_MODE == 7
//    // path-avg T(view->sample) luminance
//    float g = (accW > 0.0f) ? saturate(dbg_TvpLum / accW) : 0.0f;
//    OutSkyView[tid.xy] = float4(g, g, g, 1.0f);

//#elif SKY_DEBUG_MODE == 8
//    // visualize latitude mapping (white=zenith, 0.5=horizon, black=nadir)
//    float latVis = 0.5f + 0.5f * (lat / (PI * 0.5f));
//    OutSkyView[tid.xy] = float4(saturate(latVis).xxx, 1.0f);

//#elif SKY_DEBUG_MODE == 9
//    // visualize longitude (wrap pattern)
//    float lon01 = (lon + PI) / (2.0f * PI);
//    float g = frac(lon01 * 8.0f) < 0.5f ? 0.25f : 0.75f;
//    OutSkyView[tid.xy] = float4(g, g, g, 1.0f);

//#else
//    OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);
//#endif
//}

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
    float3 BasisRadUp;
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
SamplerState ClampLinear : register(s0);
RWTexture2D<float4> OutSkyView : register(u0);

// ===== Constants / helpers ==================================================
static const float PI = 3.14159265358979323846f;
static const float INV4PI = 0.25f / PI;
static const float3 LUMA = float3(0.2126, 0.7152, 0.0722);

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

float2 TransUV(float r, float mu, float Rg, float Rt)
{
    float rNorm = (r - Rg) / max(Rt - Rg, 1e-6f);
    float muMin = -sqrt(saturate(1.0f - (Rg * Rg) / (r * r)));
    mu = clamp(mu, muMin + 1e-5f, 1.0f - 1e-5f);
    float uMu = (mu - muMin) / (1.0f - muMin);
    return float2(uMu, saturate(rNorm));
}
float3 T_to_TOA(float r, float mu, float Rg, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rg, Rt), 0).rgb;
}

float3 T_along_ray(float r, float mu, float t, float Rg, float Rt)
{
    float rd = sqrt(t * t + 2.0f * r * mu * t + r * r);
    rd = clamp(rd, Rg, Rt);
    float muD = clamp((r * mu + t) / rd, -1.0f, 1.0f);
    float3 num = T_to_TOA(r, mu, Rg, Rt);
    float3 den = T_to_TOA(rd, muD, Rg, Rt);
    return saturate(num / max(den, float3(1e-6f, 1e-6f, 1e-6f)));
}

float DistToTop(float r, float mu, float Rt)
{
    float d = r * r * (mu * mu - 1.0f) + Rt * Rt;
    return max(-r * mu + sqrt(max(d, 0.0f)), 0.0f);
}
float DistToBottom(float r, float mu, float Rg)
{
    float d = r * r * (mu * mu - 1.0f) + Rg * Rg;
    return max(-r * mu - sqrt(max(d, 0.0f)), 0.0f);
}
bool HitsGround(float r, float mu, float Rg)
{
    return (mu < 0.0f) && (r * r * (mu * mu - 1.0f) + Rg * Rg >= 0.0f);
}

// MultiScatter LUT sampling: x=theta_s/π, y = 1 - linear altitude (top=TOA)
float4 SamplePsiMS4(float r, float muS, float Rg, float Rt)
{
    float thetaS = acos(clamp(muS, -1.0f, 1.0f));
    float u = thetaS / PI;
    float v = saturate((r - Rg) / max(Rt - Rg, 1e-6f));
    v = 1.0f - v; // 0=TOA, 1=ground (matches MultiScatteringCS with MS_FLIP_Y=1)
    return MultiScatterLUT.SampleLevel(ClampLinear, float2(u, v), 0);
}
float3 SamplePsiMS(float r, float muS, float Rg, float Rt)
{
    return SamplePsiMS4(r, muS, Rg, Rt).rgb;
}

// sun horizon softening
float SunVisibilityAtR(float r, float muS, float Rg)
{
    const float SunAngularRadius = 0.00935f;
    float sinThetaH = Rg / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunAngularRadius, sinThetaH * SunAngularRadius, muS - cosThetaH);
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

// ===== Main =================================================================
[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint W, H;
    OutSkyView.GetDimensions(W, H);
    if (tid.x >= W || tid.y >= H)
        return;

    const float Rg = PlanetRadius;
    const float Rt = PlanetRadius + AtmosphereHeight;

    float3 up = normalize(BasisRadUp);
    float3 east = normalize(BasisTanEast - up * dot(BasisTanEast, up));
    float3 north = normalize(BasisTanNorth - up * dot(BasisTanNorth, up));
    north = normalize(north);
    east = normalize(cross(north, up));

    float3 camWS = cameraPosition.xyz;
    float3 camRel = camWS - PlanetCenterWS;
    float rCam = max(Rg, length(camRel) + 8200);

    float3 wSun = -normalize(direction.xyz);

    float u = (tid.x + 0.5f) / float(W);
    float v = (tid.y + 0.5f) / float(H);
    float lon = LongitudeFromU(u);
    float lat = LatitudeFromV(v);

    float3 wView = DirFromLonLat(lon, lat, east, north, up);
    float muV = dot(wView, up);

    bool groundHit = HitsGround(rCam, muV, Rg);
    float dExit = groundHit ? DistToBottom(rCam, muV, Rg) : DistToTop(rCam, muV, Rt);

    float tEnd = dExit;
    float3 Ls = 0.0f;
    float3 Lms = 0.0f;

    float accW = 0.0f;
    float dbg_Vsun = 0.0f;
    float dbg_muS = 0.0f;
    float dbg_muPh = 0.0f;
    float dbg_TvpLum = 0.0f;
    float dbg_MSFactor = 0.0f;

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
        float rp = max(Rg, length(pRel));
        float h = max(0.0f, rp - Rg);

        float3 Tvp = T_along_ray(rCam, muV, ti, Rg, Rt);

        float dR = DensityRayleigh(h);
        float dM = DensityMie(h);
        float3 sigR_s = RayleighScattering * dR;
        float3 sigM_s = MieScattering * dM;
        float3 sigS = sigR_s + sigM_s;

        float3 upS = pRel / rp;
        float muS = dot(upS, wSun);
        float Vsun = SunVisibilityAtR(rp, muS, Rg);
        float3 Tsun_NoVis = T_to_TOA(rp, muS, Rg, Rt);
        float3 Tsun = Tsun_NoVis * Vsun;

        // FIX: phase angle is between incoming (sun) and outgoing (-view)
        float muPhase = clamp(dot(wSun, -wView), -0.9995f, 0.9995f);
        float PR = PhaseRayleigh(muPhase);
        float PM = PhaseMieHG(muPhase, saturate(MieAnisotropy));

        // single scattering (keep Tsun)
        if (SKY_USE_RAY)
            Ls += Tvp * (sigR_s * PR * Tsun) * dt;
        if (SKY_USE_MIE)
            Ls += Tvp * (sigM_s * PM * Tsun) * dt;

        // multiple scattering: do NOT multiply by T_to_TOA
        float4 Psi4 = SamplePsiMS4(rp, muS, Rg, Rt);
        float pMS = MSPhase(muPhase, Psi4.a);

        float3 msIrr = Psi4.rgb;
#if !SKY_MS_BAKED_SUNVIS
        msIrr *= Vsun;
#endif

        if (SKY_USE_MS)
        {
            float3 S_MS = sigS * (msIrr) * pMS;
            Lms += Tvp * S_MS * dt;
        }

        // debug
        accW += dt;
        dbg_Vsun += Vsun * dt;
        dbg_muS += muS * dt;
        dbg_muPh += muPhase * dt;
        dbg_TvpLum += dot(Tvp, LUMA) * dt;

#if SKY_MS_BAKED_SUNVIS
        dbg_MSFactor += 1.0f * dt;
#else
        dbg_MSFactor += Vsun * dt;
#endif
    }
    
#if SKY_ENABLE_GROUND
    if (groundHit && lat < 0.0f)
    {
        float3 Tcg = T_along_ray(rCam, muV, dExit, Rg, Rt);

        float3 pG = camRel + wView * dExit;
        float rG = max(Rg, length(pG));
        float3 upG = pG / rG;

        float muSg = dot(upG, wSun);
        float Vsg = SunVisibilityAtR(rG, muSg, Rg);
        float3 Tsg_NoVis = T_to_TOA(rG, muSg, Rg, Rt);
        float3 Tsg = Tsg_NoVis * Vsg;

        float cosNL = max(muSg, 0.0f);
        float3 Lo = (GroundAlbedo / PI) * Tsg * cosNL;

        float4 PsiG4 = SamplePsiMS4(rG, muSg, Rg, Rt);
        float muPhaseG = clamp(dot(wSun, -wView), -0.9995f, 0.9995f);
        float pMSg = MSPhase(muPhaseG, PsiG4.a);

        float3 msIrrG = PsiG4.rgb;
#if !SKY_MS_BAKED_SUNVIS
    msIrrG *= Vsg;
#endif

        float3 ImsG = (RayleighScattering + MieScattering) * pMSg * msIrrG;

        float3 Lg = Tcg * (Lo + ImsG);
        float3 L = (Ls + Lms + Lg) * radiance.rgb * SunIntensity;
        OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);
        return;
    }
#endif

    float3 L = (Ls + Lms) * radiance.rgb * SunIntensity;

#if   SKY_DEBUG_MODE == 0
    OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);

#elif SKY_DEBUG_MODE == 1
    { float3 C = Ls * radiance.rgb * multiplier * SKY_DBG_SCALE; C = C / (1.0f + C);
      OutSkyView[tid.xy] = float4(saturate(C), 1.0f); }

#elif SKY_DEBUG_MODE == 2
    { float3 C = Lms * radiance.rgb * multiplier * SKY_DBG_SCALE; C = C / (1.0f + C);
      OutSkyView[tid.xy] = float4(saturate(C), 1.0f); }

#elif SKY_DEBUG_MODE == 3
    { float3 Tend = T_along_ray(rCam, muV, dExit, Rg, Rt);
      float g = saturate(dot(Tend, LUMA));
      OutSkyView[tid.xy] = float4(g, g, g, 1.0f); }

#elif SKY_DEBUG_MODE == 4
    { float g = (accW > 0.0f) ? dbg_Vsun / accW : 0.0f;
      OutSkyView[tid.xy] = float4(g, g, g, 1.0f); }

#elif SKY_DEBUG_MODE == 5
    { float g = (accW > 0.0f) ? 0.5f * (dbg_muS / accW + 1.0f) : 0.0f;
      OutSkyView[tid.xy] = float4(g, g, g, 1.0f); }

#elif SKY_DEBUG_MODE == 6
    { float g = (accW > 0.0f) ? 0.5f * (dbg_muPh / accW + 1.0f) : 0.0f;
      OutSkyView[tid.xy] = float4(g, g, g, 1.0f); }

#elif SKY_DEBUG_MODE == 7
    { float g = (accW > 0.0f) ? saturate(dbg_TvpLum / accW) : 0.0f;
      OutSkyView[tid.xy] = float4(g, g, g, 1.0f); }

#elif SKY_DEBUG_MODE == 8
    { float latVis = 0.5f + 0.5f * (lat / (PI * 0.5f));
      OutSkyView[tid.xy] = float4(saturate(latVis).xxx, 1.0f); }

#elif SKY_DEBUG_MODE == 9
    { float lon01 = (lon + PI) / (2.0f * PI);
      float g = frac(lon01 * 8.0f) < 0.5f ? 0.25f : 0.75f;
      OutSkyView[tid.xy] = float4(g, g, g, 1.0f); }

#elif SKY_DEBUG_MODE == 10
    { float g = (accW > 0.0f) ? dbg_MSFactor / accW : 0.0f;
      OutSkyView[tid.xy] = float4(g, g, g, 1.0f); }

#else
    OutSkyView[tid.xy] = float4(max(L, 0.0f), 1.0f);
#endif
}

