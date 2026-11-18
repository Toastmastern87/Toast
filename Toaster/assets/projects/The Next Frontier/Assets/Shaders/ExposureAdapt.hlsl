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
};

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterWS;
    float PlanetRadius; // Rg
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
};

cbuffer Atmosphere : register(b5)
{
    float AtmosphereHeight;
};

cbuffer SunDiscSettings : register(b6)
{
    float SunDiscRadius; // rad
};

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
    
    float HighlightPercent; // 64
    float HighlightTarget; // 68
    uint IncidentValid; // 72
    float SpaceFactor; // 76
    
    float AdaptationKneeEV; // 80 
    float DimLightAdaptation; // 84  
    float2 _pad0;
}

StructuredBuffer<uint> Histogram        : register(t0);
Texture2D<float4> TransmittanceLUT      : register(t1);
Texture2D<float4> GroundIrradianceLUT   : register(t2);

SamplerState ClampLinear                : register(s0);

RWStructuredBuffer<float> ExposureEV    : register(u0);

#include "TransmittanceLUT.hlsli"
#include "GroundIrradianceLUT.hlsli"
#include "AtmosphereRays.hlsli"

static const uint BIN_COUNT = 256;
static const float MIN_METERED_SHARE = 0.001f;
static const float PI = 3.14159265358979323846f;
static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

groupshared float gsScan[BIN_COUNT];
groupshared float gsUsed[BIN_COUNT];
groupshared float gsWeighted[BIN_COUNT];
groupshared float gsHighlightLog2;

float BinCentreLog2(uint bin)
{
    float t = ((float) bin - 0.5f) / 254.0f;
    return MinLog2Lum + t / InvLog2LumRange;
}

float SunVisibilityAtR(float r, float muS, float Rb)
{
    float sinThetaH = Rb / r;
    float cosThetaH = -sqrt(saturate(1.0f - sinThetaH * sinThetaH));
    return smoothstep(-sinThetaH * SunDiscRadius, sinThetaH * SunDiscRadius, muS - cosThetaH);
}

float IncidentEV()
{
    const float Rt = PlanetRadius + AtmosphereHeight;
    const float RbPhys = PlanetRadius + min(0.0f, MinHeight);
    const float RbVis = RbPhys + max(1.0f, 2e-6f * PlanetRadius);

    const float3 camRel = cameraPosition.xyz - PlanetCenterWS;
    const float rCam = length(camRel);
    const float3 up = camRel / max(rCam, 1.0f);
    const float r = clamp(rCam, RbPhys + 1.0f, Rt);

    const float3 wSun = -normalize(direction.xyz);
    const float muS = dot(up, wSun);
    const float3 Esun = radiance.rgb * SunIntensity;
    
    const float3 TFlat = TransmittanceLUT.SampleLevel(ClampLinear, TransUV(r, muS, RbPhys, Rt), 0).rgb;
    const float3 flatOverPi = TFlat * max(muS, 0.0f) * (1.0f / PI) + GroundIrradianceLUT.SampleLevel(ClampLinear, GroundIrradianceUV(r, muS, RbPhys, Rt), 0).rgb;
    
    const float3 Tsun = SampleTransmittance_EyeToSun(camRel, wSun, RbPhys, Rt) * SunVisibilityAtR(max(rCam, RbVis + 1.0f), muS, RbVis);
    const float3 facingOverPi = Tsun * (1.0f / PI);

    const float3 cardOverPi = lerp(flatOverPi, facingOverPi, saturate(SpaceFactor));
    const float cardLum = MiddleGrey * dot(cardOverPi * Esun, LUMA);
    return log2(max(cardLum, 1e-10f)) - log2(MiddleGrey) - Compensation;
}

[numthreads(256, 1, 1)]
void main( uint gi : SV_GroupIndex )
{
    if (gi == 0)
        gsHighlightLog2 = -1000.0f;
    
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
    
    float screenPixels = (float) InputSize.x * (float) InputSize.y;
    float highlightRank = total - (1.0f - HighlightPercent) * screenPixels;
    if (count > 0.0f && binStart < highlightRank && highlightRank <= binEnd)
        gsHighlightLog2 = BinCentreLog2(gi);
    
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
        const float incidentEV = (IncidentValid != 0) ? IncidentEV() : 0.0f;
        ExposureEV[1] = incidentEV;
        
        if (LockEV != 0)
        {
            ExposureEV[0] = LockedEV;
            return;
        }
        
        float previousEV = ExposureEV[0];
        
        const bool frameMeterable = (gsUsed[0] >= 1.0f) && (total >= MIN_METERED_SHARE * screenPixels);

        float targetEV;
        if (IncidentValid != 0)
            targetEV = incidentEV;
        
        else if (frameMeterable)
        {
            float avgLog2 = gsWeighted[0] / gsUsed[0];
            targetEV = avgLog2 - log2(MiddleGrey) - Compensation;
        }
        else
        {
            ExposureEV[0] = (ResetAdaptation != 0) ? 0.0f : previousEV;
            return;
        }
        
        if (targetEV < AdaptationKneeEV)
            targetEV = AdaptationKneeEV + DimLightAdaptation * (targetEV - AdaptationKneeEV);
        
        if (frameMeterable && gsHighlightLog2 > -999.0f)
        {
            float highlightEV = gsHighlightLog2 - log2(HighlightTarget);
            targetEV = max(targetEV, highlightEV);
        }

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