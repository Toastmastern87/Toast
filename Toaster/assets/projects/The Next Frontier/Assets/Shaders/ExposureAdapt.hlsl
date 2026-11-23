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
    float Compensation; // 0
    float MinEV; // 4
    float MaxEV; // 8
    float SpeedToBright; // 12
    
    float SpeedToDark; // 16
    float DeltaTime; // 20
    uint ResetAdaptation; // 24
    uint LockEV; // 28
    
    float LockedEV; // 32
    uint IncidentValid; // 36
    float SpaceFactor; // 40
    float AdaptationKneeEV; // 44 
    
    float DimLightAdaptation; // 48  
    float3 _pad0;
}

StructuredBuffer<uint> Histogram        : register(t0);
Texture2D<float4> TransmittanceLUT      : register(t1);
Texture2D<float4> GroundIrradianceLUT   : register(t2);

SamplerState ClampLinear                : register(s0);

RWStructuredBuffer<float> ExposureEV    : register(u0);

#include "TransmittanceLUT.hlsli"
#include "GroundIrradianceLUT.hlsli"
#include "AtmosphereRays.hlsli"

static const float PI = 3.14159265358979323846f;
static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

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
    return log2(max(dot(cardOverPi * Esun, LUMA), 1e-10f)) - Compensation;
}

[numthreads(1, 1, 1)]
void main( uint gi : SV_GroupIndex )
{   
    // No planet with an atmosphere (menu-type scenes): a fixed, authored exposure.
    if (IncidentValid == 0)
    {
        ExposureEV[0] = LockedEV;
        ExposureEV[1] = 0.0f;
        return;
    }

    const float incidentEV = IncidentEV();
    ExposureEV[1] = incidentEV;

    if (LockEV != 0)
    {
        ExposureEV[0] = LockedEV;
        return;
    }

    // Below the knee the exposure only follows part of the drop in light (dusk stays dusk).
    float targetEV = incidentEV;
    if (targetEV < AdaptationKneeEV)
        targetEV = AdaptationKneeEV + DimLightAdaptation * (targetEV - AdaptationKneeEV);

    targetEV = clamp(targetEV, MinEV, MaxEV);

    const float previousEV = ExposureEV[0];

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