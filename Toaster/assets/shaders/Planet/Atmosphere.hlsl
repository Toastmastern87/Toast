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
	output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);

	return output;
}

#type pixel
Texture2D DepthTexture				: register(t9);
Texture2D BaseTexture				: register(t10);

SamplerState DefaultSampler			: register(s1);

#pragma pack_matrix( row_major )

static const float maxFloat = 3.402823466e+38;

cbuffer DirectionalLight	: register(b0)
{
	float4 direction;
	float4 radiance;
	float multiplier;
	float sunDiscToggle;
};

cbuffer Planet				: register(b4)
{
	float radius;
	float minAltitude;
	float maxAltitude;
	float atmosphereHeight;
	float densityFalloff;
	int atmosphereToggle;
	int numInScatteringPoints;
	int numOpticalDepthPoints;
};

cbuffer Camera				: register(b11)
{
	matrix viewMatrix;
	matrix projectionMatrix;
	matrix inverseViewMatrix;
	matrix inverseProjectionMatrix;
	float4 cameraPosition;
};

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float2 texCoord			: TEXCOORD;
};

float2 RaySphere(float3 sphereCenter, float sphereRadius, float3 rayOrigin, float3 rayDir) 
{
	float3 offset = rayOrigin - sphereCenter;
	const float a = 1.0f;
	float b = 2.0f * dot(offset, rayDir);
	float c = dot(offset, offset) - sphereRadius * sphereRadius;

	float discriminant = b * b - 4.0f * a * c;
	// No intersections:  discriminant < 0;
	// 1 intersections:  discriminant == 0;
	// 2 intersections:  discriminant > 0;
	if (discriminant > 0.0f)
	{
		float s = sqrt(discriminant);
		float dstToSphereNear = max(0.0f, (-b - s) / (2.0f * a));
		float dstToSphereFar = (-b + s) / (2.0f * a);

		if(dstToSphereFar >= 0.0f)
			return float2(dstToSphereNear, dstToSphereFar - dstToSphereNear);
	}

	return float2(maxFloat, 0.0f);

	//float3 o_minus_c = rayOrigin - sphereCenter;

	//float p = dot(rayDir, o_minus_c);
	//float q = dot(o_minus_c, o_minus_c) - (sphereRadius * sphereRadius);

	//float discriminant = (p * p) - q;
	//if (discriminant < 0.0f)
	//{
	//	return float2(maxFloat, 0.0f);
	//}

	//float dRoot = sqrt(discriminant);
	//float dist1 = -p - dRoot;
	//float dist2 = -p + dRoot;

	//return float2(dist1, dist2 - dist1);
}

float DensityAtPoint(float densitySamplePoint, float atmosphereRadius)
{
	float heightAboveSurface = length(densitySamplePoint) - radius;
	float height01 = heightAboveSurface / (atmosphereRadius - radius);
	float localDensity = exp(-height01 * densityFalloff) * (1.0f - height01);

	return localDensity;
}

float OpticalDepth(float3 rayOrigin, float3 rayDir, float rayLength, float atmosphereRadius)
{
	float3 densitySamplePoint = rayOrigin;
	float stepSize = rayLength / (numOpticalDepthPoints - 1);
	float opticalDepth = 0.0f;

	for (int i = 0; i < numOpticalDepthPoints; i++) 
	{
		float localDensity = DensityAtPoint(densitySamplePoint, atmosphereRadius);
		opticalDepth += localDensity * stepSize;
		densitySamplePoint += rayDir * stepSize;
	}

	return opticalDepth;
}

float LinearEyeDepth(float nonLinearDepth)
{ 
	float far = 3000.0f;
	float near = 0.1f;

	//return ((2.0f * near) / (far + near - nonLinearDepth * (far - near)));
	//Themp
	return ((near * far) / (far - (far - near) * nonLinearDepth)); 
	 
	//Manpat
	//float4 view_pos = mul(inverseProjectionMatrix, float4(0.0f, 0.0f, nonLinearDepth, 1.0f)); //inverse(perspective)* vec4(0.0, 0.0, depth, 1.0);
	//float lineardepth = view_pos.z / view_pos.w;
	//return lineardepth;

	//Markusa
	//return (- far * near / (nonLinearDepth * far - nonLinearDepth * near - far));
}

float CalculateLight(float3 rayOrigin, float3 rayDir, float rayLength, float atmosphereRadius)
{
	float3 inScatterPoint = rayOrigin;
	float stepSize = rayLength / (numInScatteringPoints - 1);
	float inScatteredLight = 0.0f;

	for (int i = 0; i < numInScatteringPoints; i++)
	{
		float sunRayLength = RaySphere(float3(0.0f, 0.0f, 0.0f), atmosphereRadius, inScatterPoint, direction).y;
		float sunRayOpticalDepth = OpticalDepth(inScatterPoint, direction, sunRayLength, atmosphereRadius);
		float viewRayOpticalDepth = OpticalDepth(inScatterPoint, -rayDir, stepSize * i, atmosphereRadius);
		float transmittance = exp(-(sunRayOpticalDepth + viewRayOpticalDepth));
		float localDensity = DensityAtPoint(inScatterPoint, atmosphereRadius);

		inScatteredLight += localDensity * transmittance * stepSize;
		inScatterPoint += rayDir * stepSize;
	}

	return inScatteredLight;
}

float4 main(PixelInputType input) : SV_TARGET
{
	float4 originalColor = BaseTexture.Sample(DefaultSampler, input.texCoord);

	//if (atmosphereToggle)
	//{
		float3 ndc = float3(input.texCoord.xy, 1.0f) * 2.0f - 1.0f;
		// Reversing the projection and view-space transformations.
		float4 tempVector = mul(float4(ndc, 1.0f), inverseProjectionMatrix);
		tempVector = mul(tempVector, inverseViewMatrix);
		float3 position = tempVector.xyz / tempVector.w;
		float3 rayOrigin = cameraPosition;

		float sceneDepthNonLinear = DepthTexture.Sample(DefaultSampler, input.texCoord).r;
		float sceneDepth = LinearEyeDepth(sceneDepthNonLinear);// *length(position - rayOrigin);

		float3 rayDirection = normalize(position - rayOrigin);
		float3 sphereCenter = float3(0.0f, 0.0f, 0.0f);
		float atmosphereRadius = radius + atmosphereHeight;

		float2 atmoHitInfo = RaySphere(sphereCenter, atmosphereRadius, rayOrigin, rayDirection);
		float dstToAtmosphere = atmoHitInfo.x;
		float dstThroughAtmosphere = atmoHitInfo.y;// min(atmoHitInfo.y, sceneDepth - dstToAtmosphere);

		//float2 planetHitInfo = RaySphere(sphereCenter, radius, rayOrigin, rayDirection);
		//float dstToPlanet = planetHitInfo.x;
		////float dstThroughAtmosphere;
		////if (dstToPlanet > 0.0f)
		////	dstThroughAtmosphere = dstToPlanet - dstToAtmosphere;
		////else
		////	dstThroughAtmosphere = atmoHitInfo.y - dstToAtmosphere;

		//float planetTest = InputTexture.Sample(DefaultSampler, input.texCoord).g;

		//if (planetTest > 0.9f)
		//	return originalColor;

		//if (dstThroughAtmosphere > 0.0f)
		//{
		//	const float epsilon = 0.0001f;
		//	float3 pointInAtmosphere = rayOrigin + rayDirection * (dstToAtmosphere + epsilon);
		//	float light = CalculateLight(pointInAtmosphere, rayDirection, (dstThroughAtmosphere - epsilon * 2.0f), atmosphereRadius); //

		//	return originalColor * (1.0f - light) + light;

		//	//return float4(1.0f, 0.0f, 0.0f, 1.0f);
		//}
		//else
		//
		//if (dstThroughAtmosphere <= 0.0f)
		//	return originalColor;
		//else
		//	return dstThroughAtmosphere / (atmosphereRadius * 2.0f);
		// 
		return float4(sceneDepth, sceneDepth, sceneDepth, 1.0f);
	//}

	//return originalColor;

		//float3 ndc = 2.0f * float3(input.texCoord.xy, 1.0f) - 1.0f;
		// Reversing the projection and view-space transformations.
		//float4 tempVector = mul(float4(ndc, 1.0f), inverseProjectionMatrix);
		//tempVector = mul(tempVector, inverseViewMatrix);
		//float3 position = tempVector.xyz / tempVector.w;

		//float3 rayOrigin = cameraPosition;
		//float3 rayDirection = normalize(position - rayOrigin);
		//float3 sphereCenter = float3(0.0f, 0.0f, 0.0f);
		//float sphereRadius = 3389.5f + 1010.8f;

		//float2 hitInfo = RaySphere(sphereCenter, sphereRadius, rayOrigin, rayDirection);
		//float dstToAtmosphere = hitInfo.x;
		//float dstThroughAtmosphere = hitInfo.y;

		//float planetTest = InputTexture.Sample(DefaultSampler, input.texCoord).g;

		//if (dstToAtmosphere > 90000.0f)
		//	return originalColor;

		//float color = dstThroughAtmosphere / ((radius + atmosphereHeight) * 2.0f);

		//if (planetTest > 0.9f)
		//	return originalColor;

		//return float4(color, color, color, color);
}