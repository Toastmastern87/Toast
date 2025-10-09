﻿#inputlayout
#type vertex
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
    PixelInputType output;

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    float2 uv = float2((vID << 1) & 2, vID & 2); // {0,2}
    output.texCoord = uv * 0.5;
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1);
    
    return output;
}

#type pixel
#pragma pack_matrix( row_major )

#define PI 3.141592653589793

static const float maxFloat = 3.402823466e+38;

Texture2D DepthTexture : register(t9);

SamplerState DefaultSampler : register(s1);

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


cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction;
    float4 radiance;
    float multiplier;
};

cbuffer Atmosphere : register(b4)
{
    float radius;
    float minAltitude;
    float maxAltitude;
    float atmosphereHeight;
    float mieAnisotropy;
    float rayScaleHeight;
    float mieScaleHeight;
    float3 rayBaseScatteringCoefficient;
    float mieBaseScatteringCoefficient;
    float3 planetCenter;
    int atmosphereToggle;
    int numInScatteringPoints;
    int numOpticalDepthPoints;
    int sunDiscToggle;
    float sunDiscRadius;
    float sunGlowIntensity;
    float sunEdgeSoftness;
    float sunGlowSize;
    int useDepth;
};

struct SunParams
{
    float discRadius; // rad, phys radius  (e.g. 0.00465)
    float edgeSoftness; // rad, 1–10 % of radius
    float glowSize; // rad, ~3–8 × discRadius
    float glowFalloff; // 1/σ² for gaussian ( >0 )
    float3 discColour; // usually radiance.rgb
    float3 glowColour; // atmospheric tint
    float discHDR; // HDR boost for disc   (10-20)
    float glowHDR; // HDR boost for glow   ( 1-5 )
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

// -----------------------------------------------------------------------------
// Returns inner-disc (RGB) and halo (RGB) separately so caller can decide how
// to combine / mask them.
// angle2     – squared angle between view dir and sun dir (radians²)
// ----------------------------------------------------------------------------- 
void EvaluateSun(in SunParams P,
                 in float angle2,
                 out float3 discOut,
                 out float3 glowOut)
{
    float r2 = P.discRadius * P.discRadius;
    float softR2A = (P.discRadius - P.edgeSoftness);
    float softR2B = (P.discRadius + P.edgeSoftness);
    softR2A *= softR2A;
    softR2B *= softR2B;

    // disc (smoothstep on squared radius avoids expensive sqrt/acos)
    float discMask = 1.0f - smoothstep(softR2A, softR2B, angle2);
    discOut = P.discColour * discMask * P.discHDR;

    // gaussian glow  exp(- (θ/σ)² )
    float glowMask = exp(-angle2 * P.glowFalloff);
    glowOut = P.glowColour * glowMask * P.glowHDR;
}

float2 RaySphere(float3 sphereCenter, float sphereRadius, float3 rayOrigin, float3 rayDir)
{
    rayOrigin -= sphereCenter;
    float a = dot(rayDir, rayDir);
    float b = 2.0f * dot(rayOrigin, rayDir);
    float c = dot(rayOrigin, rayOrigin) - (sphereRadius * sphereRadius);

	// If outside and facing away from sphere
    if (c > 0.0 && b > 0.0)
        return float2(maxFloat, 0); // No hit

    float discriminant = b * b - 4.0f * a * c;
	 //No intersections:  discriminant < 0;
	 //1 intersections:  discriminant == 0;
	 //2 intersections:  discriminant > 0;

    if (discriminant > 0.0f)
    {
        float s = sqrt(discriminant);
        float dstToSphereNear = max(0.0f, (-b - s) / (2.0f * a));
        float dstToSphereFar = (-b + s) / (2.0f * a);

        if (dstToSphereFar >= 0.0f)
            return float2(dstToSphereNear, dstToSphereFar);
    }

    return float2(maxFloat, 0.0f);
}

float3 SampleLightRay(float3 rayOrigin)
{
    float3 planetCenterTranslated = mul(float4(planetCenter, 1.0f), worldTranslationMatrix).xyz;
    
    float2 sunRayAtmoPoints = RaySphere(planetCenterTranslated, radius + atmosphereHeight, rayOrigin, -direction.xyz);

    if (sunRayAtmoPoints.x == maxFloat)
    {
		// No intersection: return full transmittance (no attenuation)
        return float3(1.0f, 1.0f, 1.0f);
    }
    
    float totalDist = sunRayAtmoPoints.y - sunRayAtmoPoints.x;
    
    // If totalDist <= 0, invalid scenario, return full transmittance
    if (totalDist <= 0.0f)
    {
        return float3(1.0f, 1.0f, 1.0f);
    }

    float3 rayOpticalDepth = 0.0f;
    float mieOpticalDepth = 0.0f;

    float time = 0.0f;
    float stepSize = (sunRayAtmoPoints.y - sunRayAtmoPoints.x) / (float) (numOpticalDepthPoints);
    for (int i = 0; i < numOpticalDepthPoints; i++)
    {
        float3 pointInAtmosphere = rayOrigin - direction.xyz * (time + stepSize * 0.5f);
        float height = length(pointInAtmosphere - planetCenterTranslated) - radius;

		// Inside the planet, minAltitude is to make sure that the ray is lower then even the lowest point of the planet.
        if (height < minAltitude)
            return float3(0.0f, 0.0f, 0.0f);

		// Optical depth for the secondary ray
        rayOpticalDepth += exp(-height / rayScaleHeight) * rayBaseScatteringCoefficient * stepSize;
        mieOpticalDepth += exp(-height / mieScaleHeight) * mieBaseScatteringCoefficient * stepSize;

        time += stepSize;
    }

    return exp(-(rayOpticalDepth + mieOpticalDepth));
}

float3 ComputeScatteringAlongRay(float3 rayOrigin, float3 rayDir)
{
    float3 planetCenterTranslated = mul(float4(planetCenter, 1.0f), worldTranslationMatrix).xyz;
    
    float2 atmoHitInfo = RaySphere(planetCenterTranslated, radius + atmosphereHeight, rayOrigin, rayDir);
    float tEntryPoint = atmoHitInfo.x;
    float tExitPoint = atmoHitInfo.y;

    if (tEntryPoint == maxFloat)
        return float3(0.0f, 0.0f, 0.0f);

    float tDistanceThroughAtmo = tExitPoint - tEntryPoint;

    float time = tEntryPoint;
    float stepSize = tDistanceThroughAtmo / numInScatteringPoints;

    float3 rayTotalScattering = float3(0.0f, 0.0f, 0.0f);
    float mieTotalScattering = 0.0f;
    float3 totalTransmittance = float3(1.0f, 1.0f, 1.0f);

    for (int i = 0; i < numInScatteringPoints; i++)
    {
        float3 pointInAtmosphere = rayOrigin + rayDir * (time + stepSize * 0.5f);
        float height = length(pointInAtmosphere - planetCenterTranslated) - radius;

        if (height < minAltitude)
            return float3(0.0f, 0.0f, 0.0f);
        
        // Compute light transmittance from point to sun
        float3 lightTransmittance = SampleLightRay(pointInAtmosphere);

        float rayHeightFallOff = exp(-height / rayScaleHeight);
        float mieHeightFallOff = exp(-height / mieScaleHeight);
        float3 rayOpticalDepth = rayHeightFallOff * rayBaseScatteringCoefficient;
        float mieOpticalDepth = mieHeightFallOff * mieBaseScatteringCoefficient;

        rayTotalScattering += totalTransmittance * rayOpticalDepth * lightTransmittance * stepSize;
        mieTotalScattering += totalTransmittance * mieOpticalDepth * lightTransmittance * stepSize;

        float3 samplePointTransmittance = exp(-(rayOpticalDepth + mieOpticalDepth) * stepSize);
        totalTransmittance *= samplePointTransmittance;
        time += stepSize;
    }

    // Calculate phase functions
    rayDir = normalize(rayDir);
    float3 sunDir = normalize(direction.xyz);
    float cosTheta = dot(rayDir, -sunDir); // direction.xyz is the sun direction
    float cos2Theta = cosTheta * cosTheta;
    float g = mieAnisotropy;
    float g2 = g * g;
    float rayPhase = (3.0f / (16.0f * PI)) * (1.0f + cos2Theta);
    float miePhase = (3.0f / (8.0f * PI)) * ((1.0f - g2) * (1.0f + cos2Theta)) / (pow(1.0f + g2 - 2.0f * g * cosTheta, 1.5f) * (2.0f + g2));

    return multiplier * (rayPhase * rayTotalScattering + miePhase * mieTotalScattering);
}

float LinearEyeDepth(float nonLinearDepth)
{
    return near * far / (near + nonLinearDepth * (far - near));
}

float Remap(float value, float inputMin, float inputMax, float outputMin, float outputMax)
{
    return (value - inputMin) / (inputMax - inputMin) * (outputMax - outputMin) + outputMin;
}

float4 main(PixelInputType input) : SV_TARGET
{
    float3 ndc = float3(float2(input.texCoord.x, 1.0f - input.texCoord.y) * 2.0f - 1.0f, 0.0f);
    float4 tempVector = mul(float4(ndc, 1.0f), inverseProjectionMatrix);
    tempVector = mul(tempVector, inverseViewMatrix);
    float3 worldPosPixel = tempVector.xyz / tempVector.w;
    float3 rayOrigin = cameraPosition.xyz;
		
    float3 planetCenterTranslated = mul(float4(planetCenter, 1.0f), worldTranslationMatrix).xyz;
    
    float3 rayDir = normalize(worldPosPixel - rayOrigin);
    
    float3 sunDir = normalize(direction.xyz);
    float3 sunColor = 0.0f;
    
    float3 sunScatteringColor = ComputeScatteringAlongRay(rayOrigin, sunDir);
    float glowBrightness = 1.1f;
    float3 atmosphericGlowColor = sunScatteringColor * glowBrightness;
            
    float viewerHeight = length(rayOrigin - planetCenterTranslated) - radius;
    float atmosphereTransitionHeight = atmosphereHeight * 0.1f;
    float blendFactor = saturate((viewerHeight - (atmosphereHeight - atmosphereTransitionHeight)) / atmosphereTransitionHeight);
            
    float sceneDepthNonLinear = 0.0f;
    float sceneDepth = 1000000.0f; // A large value that effectively means "no geometry"
        
    if (useDepth == 1)
    {
            // Only use the scene's depth information if requested
        sceneDepthNonLinear = DepthTexture.Sample(DefaultSampler, input.texCoord).r;
        sceneDepth = LinearEyeDepth(sceneDepthNonLinear);
        sceneDepth = (1.0f - Remap(sceneDepth, far, near, 0.0f, 1.0f)) * length(worldPosPixel - rayOrigin);      
    }
    
    SunParams s;
    s.discRadius = sunDiscRadius;
    s.edgeSoftness = sunEdgeSoftness;
    s.glowSize = sunGlowSize; // 3-6 × disc radius
    s.glowFalloff = 1.0 / (s.glowSize * s.glowSize);
    s.discColour = radiance.rgb;
    s.glowColour = lerp(atmosphericGlowColor, radiance.rgb, blendFactor);
    s.discHDR = 12.0;
    s.glowHDR = 2.0;

    float3 discTerm, glowTerm;
    float angle2 = 1.0 - dot(rayDir, -sunDir); // 1-cosθ ≈ θ²/2 for small θ
    EvaluateSun(s, angle2, discTerm, glowTerm);

    sunColor = discTerm + glowTerm;
            
    // Respect depth: Sun should fade behind geometry
    if (useDepth == 1)
    {
        float epsilon = 1e-15f;
        if (sceneDepthNonLinear > epsilon)
        {
            sunColor = float3(0.0f, 0.0f, 0.0f); // Sun is behind geometry
        }
    }
    
    return float4(sunColor, 1.0f);

}