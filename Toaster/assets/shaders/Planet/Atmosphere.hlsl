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
Texture2D InputTexture          : register(t9);
SamplerState DefaultSampler     : register(s1);

#pragma pack_matrix( row_major )

static const float maxFloat = 3.402823466e+38;

cbuffer Camera : register(b11)
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

float2 RaySphere(float3 center, float radius, float3 rayOrigin, float3 rayDir) 
{
	float3 offset = rayOrigin - center;
	const float a = 1.0f;
	float b = 2.0f * dot(offset, rayDir);
	float c = dot(offset, offset) - radius * radius;

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
}

float4 main(PixelInputType input) : SV_TARGET
{
	float3 ndc = 2.0f * float3(input.texCoord.xy, 1.0f) - 1.0f;
	// Reversing the projection and view-space transformations.
	float4 tempVector = mul(float4(ndc, 1.0f), inverseProjectionMatrix);
	tempVector = mul(tempVector, inverseViewMatrix);
	float3 position = tempVector.xyz / tempVector.w;

	float3 rayOrigin = cameraPosition;
	float3 rayDirection = normalize(position - rayOrigin);
	float3 sphereCenter = float3(0.0f, 0.0f, 0.0f);
	float sphereRadius = 3389.5f + 1010.8f;

	float2 hitInfo = RaySphere(sphereCenter, sphereRadius, rayOrigin, rayDirection);
	float dstToAtmosphere = hitInfo.x;
	float dstThroughAtmosphere = hitInfo.y;

	float planetTest = InputTexture.Sample(DefaultSampler, input.texCoord).g;

	if (dstToAtmosphere > 90000.0f)
		discard;

	float color = dstThroughAtmosphere / ((3389.5f + 1010.8f) * 2.0f);

	if (planetTest > 0.9f)
		discard;
	
	return float4(color, color, color, color);
}