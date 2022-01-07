#inputlayout
#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
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
	float3 cameraPos		: POSITION1;
	float3 viewVector		: POSITION2;
	float2 texCoord			: TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
	PixelInputType output;

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
	output.texCoord = float2((vID << 1) & 2, vID & 2);
	output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);

	float3 ndc = 2.0f * float3(output.texCoord, 1.0f) - 1.0f;
	// Reversing the projection and view-space transformations.
	float4 tempVector = mul(float4(ndc, 1.0f), inverseProjectionMatrix);
	tempVector = mul(tempVector, inverseViewMatrix);
	float3 position = tempVector.xyz / tempVector.w;

	output.cameraPos = cameraPosition;
	output.viewVector = normalize(position - output.cameraPos);

	//output.worldPos = mul(output.position, inverseProjectionMatrix);
	//output.worldPos = mul(output.worldPos, inverseViewMatrix);
	// 
	//output.viewVector = worldPos - cameraPosition.xyz;

	//float4x4 tempMatrix = (float4x4)inverseViewMatrix;
	//output.viewVector.x = tempMatrix[2][0];
	//output.viewVector.y = tempMatrix[2][1];
	//output.viewVector.z = tempMatrix[2][2];

	return output;
}

#type pixel
static const float maxFloat = 3.402823466e+38;

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float3 cameraPos		: POSITION1;
	float3 viewVector		: POSITION2;
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

bool rayHitSphere(float3 sphereCenter, float3 sphereRadius, float3 rayOrigin, float3 rayDirection)
{
	float3 oneMinusC = rayOrigin - sphereCenter;
	float a = 1.0f;
	float b = 2.0f * dot(rayDirection, oneMinusC);
	float c = dot(oneMinusC, oneMinusC) - (sphereRadius * sphereRadius);
	float discrim = (b * b) - (4.0f * a * c);

	if (discrim < 0.0)
		return false;
	else
	{
		float t1 = (-b - sqrt(discrim)) / (2.0f * a);
		float t2 = (-b + sqrt(discrim)) / (2.0f * a);
		float t = max(t1, t2);

		if (t < 0.0)
			return false;
		else
			return true;
	}
}

float4 main(PixelInputType input) : SV_TARGET
{
	float4x4 inverseProjectionMatrix = { 0.70666, 0.0, 0.0, 0.0,
										 0.0, 0.26795, 0.0, 0.0,
										 0.0, 0.0, 0.0, -99.99995,
										 0.0, 0.0, 1.0, 99.99999 };

	float4x4 inverseViewMatrix = { 1.0, 0.0, 0.0, 0.0,
									0.0, 0.96106, 0.27636, 0.0,
									0.0, -0.27636, 0.96106, 0.0,
									0.0, 2.76356, -9.61055, 1.0 };

	float3 ndc = 2.0f * float3(input.texCoord.xy, 1.0f) - 1.0f;
	// Reversing the projection and view-space transformations.
	float4 tempVector = mul(float4(ndc, 1.0f), inverseProjectionMatrix);
	tempVector = mul(tempVector, inverseViewMatrix);
	float3 position = tempVector.xyz / tempVector.w;

	float3 rayOrigin = input.cameraPos;
	float3 rayDirection = normalize(position - rayOrigin);
	float3 sphereCenter = float3(0.0f, 0.0f, 0.0f);
	float sphereRadius = 0.5f;

	//float3 rd = normalize(float3(input.texCoord, 1.0f));
	//float2 p = (2.0f * input.texCoord - float2(1280.0f, 720.0f)) / 720.0f;

	//float3 rd = normalize(float3(p, -2.0f));
	//float3 rd = ray_dir(45.0f, float2(1280.0f, 720.0f), input.texCoord);

	//float4 ro = float4(0.0, 0.0, 10.f, 1.0);
	//float3 rd = normalize(float3((-1.0f + 2.0f * input.texCoord) * float2(1.778f, 1.0f), -1.0f));

	//float2 hitInfo = RaySphere(float3(0.0f, 0.0f, 0.0f), 1.0f, input.cameraPos, input.viewVector);
	//float2 hitInfo = raySphereIntersection(float3(0.0f, 0.0f, 0.0f), 1.0f, input.cameraPos.xyz, input.viewVector.xyz);
	bool hitInfo = rayHitSphere(sphereCenter, sphereRadius, rayOrigin, rayDirection);
	//float2 hitInfo = RaySphere(float3(0.0f, 0.0f, 0.0f), 1.0f, input.cameraPos.xyz, rd);
	//float hitInfo = raySphereIntersect(float3(0.0f, 0.0f, 0.0f), 1.0f, input.cameraPos.xyz, input.viewVector.xyz);
	//float dstToAtmosphere = hitInfo.x;
	//float dstThroughAtmosphere = hitInfo.y;

	//if (dstToAtmosphere > 9000.0f)
	//	discard;

	////float color = dstThroughAtmosphere / (1.0f * 2.0f);
	////return float4(color, 0.0f, 0.0f, 1.0f); //dstThroughAtmosphere / (1.0f * 2.0f)

	//float color = dstThroughAtmosphere / (1.0f * 2.0f);
	//if (dstToAtmosphere < 9000.0f)
	//	return float4(dstToAtmosphere, 0.0f, 0.0f, 1.0f); //dstThroughAtmosphere / (1.0f * 2.0f)
	//else
	//	discard;

	//return float4(dstToAtmosphere, 0.0f, 0.0f, 1.0f); //dstThroughAtmosphere / (1.0f * 2.0f)
	if (hitInfo)
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	else
		discard;

	return float4(0.0f, 0.0f, 0.0f, 0.0f);
}