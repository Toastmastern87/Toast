#ifndef SHADOW_HEIGHT_MAP_HLSLI
#define SHADOW_HEIGHT_MAP_HLSLI

static const uint SHADOWHEIGHTMAP_SIZE = 512;
static const float SHADOWHEIGHTMAP_MIN_HALF_EXTEND = 32000.0f;
static const float SHADOWHEIGHTMAP_MAX_HALF_EXTEND = 1500000.0f;
static const float SHADOWHEIGHTMAP_MIN_PENUMBRA = 0.004f;
static const float SHADOWHEIGHTMAP_MIN_PENUMBRA_M = 25.0f;
static const float SHADOWHEIGHTMAP_UNSHADOWED = -1.0e6f;

struct ShadowHeightFrame
{
    float3 Center;
    float3 East;
    float3 North;
    float HalfExtent;
};

float3 ShadowHeightMapToPlanetSpace(float3 vWS, float3 lonEast, float3 spinUp, float3 lonNorth)
{
    return float3(dot(vWS, lonEast), dot(vWS, spinUp), dot(vWS, lonNorth));
}

ShadowHeightFrame BuildShadowHeightFrame(float3 camFromPlanetWS, float3 lonEast, float3 spinUp, float3 lonNorth, float apFar)
{
    ShadowHeightFrame f;
    
    f.Center = normalize(ShadowHeightMapToPlanetSpace(camFromPlanetWS, lonEast, spinUp, lonNorth));
    const float3 axis = (abs(f.Center.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    f.East = normalize(cross(axis, f.Center));
    f.North = cross(f.Center, f.East);
    f.HalfExtent = clamp(apFar, SHADOWHEIGHTMAP_MIN_HALF_EXTEND, SHADOWHEIGHTMAP_MAX_HALF_EXTEND);
    return f;
}

float3 ShadowHeightUVToDir(ShadowHeightFrame f, float2 uv, float R)
{
    const float2 xy = (uv * 2.0f - 1.00f) * (f.HalfExtent / R);
    return normalize(f.Center + f.East * xy.x + f.North * xy.y);
}

bool ShadowHeightDirToUV(ShadowHeightFrame f, float3 dirPS, float R, out float2 uv)
{
    const float c = dot(dirPS, f.Center);
    const float3 q = dirPS / max(c, 1e-6f);
    const float2 xy = float2(dot(q, f.East), dot(q, f.North)) * (R / f.HalfExtent);
    uv = xy * 0.5f + 0.5f;
    return c > 0.0f && all(abs(xy) < 1.0f);
}

#endif