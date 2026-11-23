#type compute
#pragma pack_matrix(row_major)

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

Texture2DArray<float> HeightCube    : register(t0);
Texture2D<uint> APFarU32            : register(t1);

RWTexture2D<float2> OutShadowHeight : register(u0);

#include "HeightCubeSampling.hlsli"
#include "ShadowHeightMap.hlsli"

static const uint SHADOWHEIGHTMAP_STEPS = 24;

[numthreads(8, 8, 1)]
void main( uint3 tid : SV_DispatchThreadID )
{
    if (tid.x >= SHADOWHEIGHTMAP_SIZE || tid.y >= SHADOWHEIGHTMAP_SIZE)
        return;

    const float R = PlanetRadius;
    const float APFar = asfloat(APFarU32.Load(int3(0, 0, 0)));
    const ShadowHeightFrame frame = BuildShadowHeightFrame(cameraPosition.xyz - PlanetCenterCR, BasisLonEast, BasisSpinUp, BasisLonNorth, APFar);

    const float2 uv = ((float2) tid.xy + 0.5f) / (float) SHADOWHEIGHTMAP_SIZE;
    const float3 upPS = ShadowHeightUVToDir(frame, uv, R);
    const float3 sunPS = normalize(ShadowHeightMapToPlanetSpace(-direction.xyz, BasisLonEast, BasisSpinUp, BasisLonNorth));

    const float sinSun = dot(upPS, sunPS);
    float3 toSunPS = sunPS - upPS * sinSun;
    const float cosSun = length(toSunPS);
    if (cosSun < 1e-4f)
    {
        OutShadowHeight[tid.xy] = float2(SHADOWHEIGHTMAP_UNSHADOWED, 0.0f);
        return;
    }
    toSunPS /= cosSun;
    const float tanSun = sinSun / cosSun;
    
    uint width, height, layers, mipCount;
    HeightCube.GetDimensions(0, width, height, layers, mipCount);
    const uint2 size0 = uint2(width, height);

    const float h0 = LoadHeightBilinear(HeightCube, upPS, 0.0f, size0, mipCount);
    const float metersPerTexel = R * (0.5f * 3.14159265f / (float) width);
    const float dMin = metersPerTexel;
    const float dMax = max(sqrt(2.0f * R * max(MaxHeight - h0, 0.0f)), 2.0f * dMin);
    const float growth = pow(dMax / dMin, 1.0f / (float) (SHADOWHEIGHTMAP_STEPS - 1));

    float hShadow = SHADOWHEIGHTMAP_UNSHADOWED;
    float dBlocker = 0.0f;
    float d = dMin;
    
    [loop]
    for (int i = 0; i < SHADOWHEIGHTMAP_STEPS; ++i)
    {
        const float theta = d / (R + h0);
        float s, c;
        sincos(theta, s, c);
        const float3 dirPS = upPS * c + toSunPS * s;
        
        const float mipF = log2(max(d * (growth - 1.0f), metersPerTexel) / metersPerTexel);
        const float hS = LoadHeightBilinear(HeightCube, dirPS, mipF, size0, mipCount);
        
        const float sh = sin(0.5f * theta);
        const float x = (R + hS) * s;
        const float hLine = hS - (R + hS) * 2.0f * sh * sh - x * tanSun;

        if (hLine > hShadow)
        {
            hShadow = hLine;
            dBlocker = d;
        }
        
        d *= growth;
    }
    
    OutShadowHeight[tid.xy] = float2(hShadow, dBlocker);
}