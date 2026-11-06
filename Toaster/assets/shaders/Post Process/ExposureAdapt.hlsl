#type compute
#pragma pack_matrix(row_major)

cbuffer AutoExposure : register(b8)
{
    uint2 InputSize; // 0
    float MinLog2Lum; // 8
    float InvLog2LumRange; // 12
    
    float LowPercent; // 16
    float HighPercent; // 20
    float Compensation; // 24
    float MiddleGrey; // 28
    
    float MinEV; // 32
    float MaxEV; // 36
    float SpeedToBright; // 40 
    float SpeedToDark; // 44
    
    float DeltaTime; // 48
    uint ResetAdaptation; // 52
    uint LockEV; // 56
    float LockedEV; // 60
}

StructuredBuffer<uint> Histogram : register(t0);
RWStructuredBuffer<float> ExposureEV : register(u0);

static const uint BIN_COUNT = 256;

groupshared float gsScan[BIN_COUNT];
groupshared float gsUsed[BIN_COUNT];
groupshared float gsWeighted[BIN_COUNT];

float BinCentreLog2(uint bin)
{
    float t = ((float) bin - 0.5f) / 254.0f;
    return MinLog2Lum + t / InvLog2LumRange;
}

[numthreads(256, 1, 1)]
void main( uint gi : SV_GroupIndex )
{
    float count = (gi == 0) ? 0.0f : (float) Histogram[gi];
    gsScan[gi] = count;
    GroupMemoryBarrierWithGroupSync();
    
    [unroll]
    for (uint offset = 1; offset < BIN_COUNT; offset <<= 1)
    {
        float add = (gi >= offset) ? gsScan[gi - offset] : 0.0f;
        GroupMemoryBarrierWithGroupSync();
        gsScan[gi] += add;
        GroupMemoryBarrierWithGroupSync();
    }

    float total = gsScan[BIN_COUNT - 1];
    float low = LowPercent * total;
    float high = HighPercent * total;
    float binEnd = gsScan[gi];
    float binStart = binEnd - count;
    float used = max(0.0f, min(binEnd, high) - max(binStart, low));
    
    gsUsed[gi] = used;
    gsWeighted[gi] = used * BinCentreLog2(gi);
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint stride = BIN_COUNT / 2; stride > 0; stride >>= 1)
    {
        if (gi < stride)
        {
            gsUsed[gi] += gsUsed[gi + stride];
            gsWeighted[gi] += gsWeighted[gi + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    if (gi == 0)
    {
        if (LockEV != 0)
        {
            ExposureEV[0] = LockedEV;
            return;
        }
        
        float previousEV = ExposureEV[0];
        
        if (gsUsed[0] < 1.0f)
        {
            ExposureEV[0] = (ResetAdaptation != 0) ? 0.0f : previousEV;
            return;
        }

        float avgLog2 = gsWeighted[0] / gsUsed[0];
        float targetEV = avgLog2 - log2(MiddleGrey) - Compensation;
        targetEV = clamp(targetEV, MinEV, MaxEV);
        
        float ev;
        if (ResetAdaptation != 0 || !(abs(previousEV) < 100.0f))
            ev = targetEV;
        else
        {
            float speed = (targetEV < previousEV) ? SpeedToBright : SpeedToDark;
            float k = 1.0f - exp(-DeltaTime * speed);
            ev = previousEV + (targetEV - previousEV) * k;
        }
        
        ExposureEV[0] = ev;
    }

}