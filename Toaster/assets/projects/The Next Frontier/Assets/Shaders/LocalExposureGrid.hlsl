#type compute
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

Texture2D sceneHDR                  : register(t0);
StructuredBuffer<float> ExposureEV  : register(t1);

RWTexture3D<float2> GridOut         : register(u0);

static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

#include "LocalExposure.hlsli" 

groupshared int gsSum[LE_BINS];
groupshared uint gsCount[LE_BINS];

[numthreads(16, 16, 1)]
void main( uint3 cell : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint gi : SV_GroupIndex )
{
    if (gi < LE_BINS)
    {
        gsSum[gi] = 0;
        gsCount[gi] = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    
    uint gridW, gridH, gridD;
    GridOut.GetDimensions(gridW, gridH, gridD);

    const uint2 screen = uint2((uint) viewportWidth, (uint) viewportHeight);
    const uint2 p0 = (cell.xy * screen) / uint2(gridW, gridH);
    const uint2 p1 = ((cell.xy + 1) * screen) / uint2(gridW, gridH);
    
    const float exposure = exp2(-ExposureEV[0]);

    for (uint y = p0.y + gtid.y; y < p1.y; y += 16)
    {
        for (uint x = p0.x + gtid.x; x < p1.x; x += 16)
        {
            const float lum = max(dot(sceneHDR.Load(int3(x, y, 0)).rgb, LUMA) * exposure, 1e-8f);
            const float logLum = clamp(log2(lum), LE_LOG_MIN, LE_LOG_MAX);
            const uint bin = min((uint) (LocalExposureBin01(logLum) * LE_BINS), LE_BINS - 1);

            InterlockedAdd(gsSum[bin], (int) round(logLum * 256.0f));
            InterlockedAdd(gsCount[bin], 1u);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (gi < LE_BINS)
        GridOut[uint3(cell.xy, gi)] = float2((float) gsSum[gi] / 256.0f, (float)gsCount[gi]);
}