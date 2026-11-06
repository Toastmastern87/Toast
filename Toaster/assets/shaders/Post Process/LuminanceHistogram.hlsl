#type compute
#pragma pack_matrix(row_major)

cbuffer AutoExposure : register(b8)
{
    uint2 InputSize;        // 0
    float MinLog2Lum;       // 8
    float InvLog2LumRange;  // 12
    
    float LowPercent;       // 16
    float HighPercent;      // 20
    float Compensation;     // 24
    float MiddleGrey;       // 28
    
    float MinEV;            // 32
    float MaxEV;            // 36
    float SpeedToBright;    // 40 
    float SpeedToDark;      // 44
    
    float DeltaTime;        // 48
    uint ResetAdaptation;   // 52
    uint LockEV;            // 56
    float LockedEV;         // 60
}

Texture2D<float4> SceneHDR          : register(t0);
RWStructuredBuffer<uint> Histogram  : register(u0);

static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);
static const uint BIN_COUNT = 256;

groupshared uint gsBins[BIN_COUNT];

uint LuminanceToBin(float3 hdr)
{
    float lum = dot(hdr, LUMA);

    if (!(lum >= exp2(MinLog2Lum)))
        return 0;

    float t = saturate((log2(lum) - MinLog2Lum) * InvLog2LumRange);
    return (uint) (t * 254.0f + 1.0f);
}

[numthreads(16, 16, 1)]
void main( uint3 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex )
{
    gsBins[gi] = 0;
    GroupMemoryBarrierWithGroupSync();
    
    if (tid.x < InputSize.x && tid.y < InputSize.y)
    {
        float3 hdr = SceneHDR.Load(int3(tid.xy, 0)).rgb;
        InterlockedAdd(gsBins[LuminanceToBin(hdr)], 1);
    }
    GroupMemoryBarrierWithGroupSync();
    
    InterlockedAdd(Histogram[gi], gsBins[gi]);
}