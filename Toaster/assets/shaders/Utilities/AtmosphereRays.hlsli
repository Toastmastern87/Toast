#ifndef ATMOSPHERE_RAYS_HLSLI
#define ATMOSPHERE_RAYS_HLSLI

float3 T_to_TOA(float r, float mu, float Rb, float Rt)
{
    return TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, mu, Rb, Rt), 0).rgb;
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
    Hit H = (Hit) 0;

    float Rabs = abs(R);
    if (Rabs <= 0.0f)
        return H;

    // Normalize direction for stable quadratic
    float a = dot(rd, rd);
    if (a <= 0.0f)
        return H;
    float invDirLen = rsqrt(max(a, 1e-30));
    float3 nrd = rd * invDirLen; // |nrd| = 1

    // Scale origin into unit-sphere space: |roU + t*nrd|^2 = 1
    float3 roU = ro / Rabs;

    // Solve t^2 + 2 b t + c = 0, where:
    float b = dot(roU, nrd);
    float c = dot(roU, roU) - 1.0f;

    // Discriminant (with tiny negative allowed for grazing)
    float disc = b * b - c;
    const float grazeTol = 2e-4; // allow slight negatives from FP error
    if (disc < -grazeTol)
        return H;
    disc = max(disc, 0.0f);

    float s = sqrt(disc);
    float t0u = -b - s; // unit-sphere param
    float t1u = -b + s;

    if (t0u > t1u)
    {
        float tmp = t0u;
        t0u = t1u;
        t1u = tmp;
    }

    // Convert back to meters and original rd scale
    H.ok = true;
    H.t0 = t0u * Rabs * invDirLen;
    H.t1 = t1u * Rabs * invDirLen;

    return H;
}

// Transmittance of the *atmospheric segment* intersected by ray (ro,rd).
// Uses your TLUT T_to_TOA(r, mu, Rb, Rt):
// If the LUT encodes T(p->TOA) = exp(-tau), then T_segment(p->q) = T(p->TOA) / T(q->TOA)
float3 TLUT_SegmentTransmittance(float3 ro, float3 rd, float RbVis, float Rt)
{
    Hit hit = IntersectSphereGrazingSafe(ro, rd, Rt);
    if (!hit.ok || hit.t1 <= max(0.0, hit.t0))
        return 1.0.xxx; // no atmosphere on ray

    float tEnter = max(0.0, hit.t0);
    float tExit = hit.t1;

    float3 pEnter = ro + rd * tEnter;
    float3 pExit = ro + rd * tExit;

    float rEnter = length(pEnter);
    float rExit = length(pExit);

    float3 upEnter = pEnter / rEnter;
    float3 upExit = pExit / rExit;

    float muEnter = dot(rd, upEnter); // cos zenith at entry
    float muExit = dot(rd, upExit); // ...at exit (same rd)

    float3 Tenter = T_to_TOA(rEnter, muEnter, RbVis, Rt);
    float3 Texit = T_to_TOA(rExit, muExit, RbVis, Rt);

    // Avoid divide-by-near-zero (if Texit extremely small, clamp)
    Texit = max(Texit, 1e-5.xxx);

    return saturate(Tenter / Texit);
}

// Eye->sun transmittance along one ray. Called per disc pixel with that pixel's ray,
// so the disc darkens and reddens across its face when it sits behind thick air.
// camRel : cameraPosition - PlanetCenterWS
// rayDir : the ray through this pixel (unit)
// RbPhys : physical ground radius (PlanetRadius + min(0, MinHeight))
// Rt     : top-of-atmosphere radius
float3 SampleTransmittance_EyeToSun(float3 camRel, float3 rayDir, float RbPhys, float Rt)
{
    float r = length(camRel);
    float3 up = camRel / max(r, 1e-6f);

    // Inside atmosphere: TLUT gives T(point -> TOA) along ray direction (here: toward sun)
    if (r <= Rt + 1e-3f)
        return T_to_TOA(r, clamp(dot(rayDir, up), -1.0f, 1.0f), RbPhys, Rt);

    // In space: only part of the eye->sun ray (if any) crosses the shell.
    // Your TLUT_SegmentTransmittance computes T_segment = T(enter->TOA) / T(exit->TOA).
    // If the ray never hits the shell, it returns 1.
    return TLUT_SegmentTransmittance(camRel, rayDir, RbPhys, Rt);
}

#endif // ATMOSPHERE_RAYS_HLSLI