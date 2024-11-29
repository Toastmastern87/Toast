#inputlayout
#type vertex
#pragma pack_matrix( row_major )

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float2 texCoord			: TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
	PixelInputType output;

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
	output.texCoord = float2((vID << 1) & 2, vID & 2);
	output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1);

	return output;
}

#type pixel
Texture2D DepthTexture				: register(t9);
Texture2D BaseTexture				: register(t10);

SamplerState DefaultSampler			: register(s1);

#pragma pack_matrix( row_major )

#define PI 3.141592653589793

static const float maxFloat = 3.402823466e+38;

cbuffer Camera : register(b0)
{
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float far;
    float near;
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
};

struct PixelInputType
{
	float4 position			: SV_POSITION0;
	float2 texCoord			: TEXCOORD;
};

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

		if(dstToSphereFar >= 0.0f)
			return float2(dstToSphereNear, dstToSphereFar);
	}

	return float2(maxFloat, 0.0f);
}

float3 SampleLightRay(float3 rayOrigin)
{
	float2 sunRayAtmoPoints = RaySphere(planetCenter, radius + atmosphereHeight, rayOrigin, -direction.xyz);

	float3 rayOpticalDepth = 0.0f;
	float mieOpticalDepth = 0.0f;

	float time = 0.0f;
	float stepSize = (sunRayAtmoPoints.y - sunRayAtmoPoints.x) / (float)(numOpticalDepthPoints);
	for (int i = 0; i < numOpticalDepthPoints; i++)
	{
		float3 pointInAtmosphere = rayOrigin - direction.xyz * (time + stepSize * 0.5f);
		float height = length(pointInAtmosphere - planetCenter) - radius;

		// Inside the planet, minAltitude is to make sure that the ray is lower then even the lowest point of the planet.
		if (height < minAltitude)
			return 0.0f;

		// Optical depth for the secondary ray
		rayOpticalDepth += exp(-height / rayScaleHeight) * rayBaseScatteringCoefficient * stepSize;
		mieOpticalDepth += exp(-height / mieScaleHeight) * mieBaseScatteringCoefficient * stepSize;

		time += stepSize;
	}

	return exp(-(rayOpticalDepth + mieOpticalDepth));
}

float3 CalculateLightScattering(float3 rayOrigin, float3 rayDir, float tEntryPoint, float tDistanceThroughAtmo, out float3 returnTransmittance)
{
	float time = tEntryPoint;
	float stepSize = tDistanceThroughAtmo / (float)(numInScatteringPoints);

	float3 rayTotalScattering = float3(0.0f, 0.0f, 0.0f);
	float mieTotalScattering = 0.0f;
	float3 totalTransmittance = 1.0f;

	for (int i = 0; i < numInScatteringPoints; i++)
	{
		float3 pointInAtmosphere = rayOrigin + rayDir * (time + stepSize * 0.5f);
		float height = length(pointInAtmosphere - planetCenter) - radius;

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

	returnTransmittance = totalTransmittance;

    float cosTheta = dot(rayDir, -direction.xyz);
	float cos2Theta = cosTheta * cosTheta;
	float g = mieAnisotropy;
	float g2 = g * g;
	float rayPhase = (3.0f / (16.0f * PI)) * (1.0f + cos2Theta);
	float miePhase = (3.0f / (8.0f * PI)) * ((1.0f - g2) * (1.0f + cos2Theta)) / (pow(1.0f + g2 - 2.0f * g * cosTheta, 1.5f) * (2.0f + g2));

	//Rename multiplier to intensity
 	return multiplier * (rayPhase * rayTotalScattering + miePhase * mieTotalScattering);	                 
}

float3 ComputeScatteringAlongRay(float3 rayOrigin, float3 rayDir)
{
    float2 atmoHitInfo = RaySphere(planetCenter, radius + atmosphereHeight, rayOrigin, rayDir);
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
        float height = length(pointInAtmosphere - planetCenter) - radius;

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
    float cosTheta = dot(rayDir, -direction.xyz); // direction.xyz is the sun direction
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

float Remap(float value, float inputMin, float inputMax, float outputMin, float outputMax) {
	return (value - inputMin) / (inputMax - inputMin) * (outputMax - outputMin) + outputMin;
}

float4 main(PixelInputType input) : SV_TARGET
{
    float4 originalColor = BaseTexture.Sample(DefaultSampler, input.texCoord);

    if (atmosphereToggle)
    {
        float3 ndc = float3(float2(input.texCoord.x, 1.0f - input.texCoord.y) * 2.0f - 1.0f, 0.0f);
        float4 tempVector = mul(float4(ndc, 1.0f), inverseProjectionMatrix);
        tempVector = mul(tempVector, inverseViewMatrix);
        float3 worldPosPixel = tempVector.xyz / tempVector.w;
        float3 rayOrigin = cameraPosition.xyz;

        float sceneDepthNonLinear = DepthTexture.Sample(DefaultSampler, input.texCoord).r;
        float sceneDepth = LinearEyeDepth(sceneDepthNonLinear);
        sceneDepth = (1.0f - Remap(sceneDepth, far, near, 0.0f, 1.0f)) * length(worldPosPixel - rayOrigin);
		
        float3 rayDir = normalize(worldPosPixel - rayOrigin);

        float2 atmoHitInfo = RaySphere(planetCenter, radius + atmosphereHeight, rayOrigin, rayDir);
        float dstToAtmosphere = atmoHitInfo.x;
        float dstThroughAtmosphere = min(atmoHitInfo.y - atmoHitInfo.x, sceneDepth - dstToAtmosphere);

		// Initialize contributions
        float3 scatteringColor = 0.0f;
        float3 transmittance = 1.0f;

		// Atmosphere scattering contribution (only if inside atmosphere)
        if (dstThroughAtmosphere > 0.0f)
        {
            scatteringColor = CalculateLightScattering(rayOrigin, rayDir, dstToAtmosphere, dstThroughAtmosphere, transmittance);
        }

		// Sun disc logic (always render sun disc)
        float3 sunDir = normalize(-direction.xyz);
        float3 sunColor = 0.0f;

        if (sunDiscToggle > 0)
        {  
            // Compute the angle between the view direction and the sun direction
            float cosAngle = dot(rayDir, sunDir);
            cosAngle = clamp(cosAngle, -1.0f, 1.0f);
            float angle = acos(cosAngle); // angle in radians

            // Sun disc parameters
            float sunAngularRadius = sunDiscRadius; // in radians (e.g., 0.00465f for Earth)
            float edgeSoftness = sunEdgeSoftness; // Small value for sharp edge
            float glowSize = sunGlowSize; // Glow size in radians
            float sunGlowFactor = sunGlowIntensity; // Glow intensity factor from Atmosphere cbuffer

            // Sun disc intensity
            float sunDiscIntensity = 1.0f - smoothstep(sunAngularRadius - edgeSoftness, sunAngularRadius + edgeSoftness, angle);
            sunDiscIntensity *= 10.0f; // Boost intensity for HDR

            // Sun glow intensity using Gaussian falloff
            float angleFromSunCenter = angle;
            float sunGlowIntensity = sunGlowFactor * exp(-pow(angleFromSunCenter / glowSize, 2.0f));

            // Compute atmospheric scattering along the sun's direction
            float3 sunScatteringColor = ComputeScatteringAlongRay(rayOrigin, sunDir);

            // Compute the viewer's height above the planet surface
            float viewerHeight = length(rayOrigin - planetCenter) - radius;

            // Normalize viewer height to a range [0, 1] for blending
            float atmosphereTransitionHeight = atmosphereHeight * 0.1f; // Adjust as needed
            float blendFactor = saturate((viewerHeight - (atmosphereHeight - atmosphereTransitionHeight)) / atmosphereTransitionHeight);

            // Adjust sun scattering color brightness for the glow
            float glowBrightness = 1.1f; // Adjust as needed

            // Compute the glow color by blending based on viewer position
            float3 atmosphericGlowColor = sunScatteringColor * glowBrightness;
            float3 spaceGlowColor = radiance.rgb;

            float3 sunGlowColor = lerp(atmosphericGlowColor, spaceGlowColor, blendFactor);
            sunGlowColor = max(sunGlowColor, float3(0.0f, 0.0f, 0.0f));

            // Boost glow color intensity for HDR
            sunGlowColor *= 10.0f;

            // Final sun color: combine inner disc and glow
            sunColor = (radiance.rgb * sunDiscIntensity) + (sunGlowColor * sunGlowIntensity);

            // Respect depth: Sun should fade behind geometry
            float epsilon = 1e-15f;
            if (sceneDepthNonLinear > epsilon)
            {
                sunColor = float3(0.0f, 0.0f, 0.0f); // Sun is behind geometry
            }
        }

		// Combine with original color and atmospheric transmittance
        float4 finalColor = (originalColor * float4(transmittance, 1.0f)) + float4(scatteringColor + sunColor, 0.0f);
        
        return finalColor;
		
    }
	
    return originalColor;
}