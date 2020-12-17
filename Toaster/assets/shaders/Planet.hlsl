#inputlayout
vertex
vertex
instance
instance
instance
instance

#type vertex
#pragma pack_matrix( row_major )

#define PI 3.141592653589793

Texture2D heightMap : register(t0);
SamplerState sampleType;

cbuffer Camera : register(b0)
{
	matrix viewMatrix;
	matrix projectionMatrix;
	matrix inverseViewMatrix;
	float4 cameraPosition;
};

cbuffer Model : register(b1)
{
	matrix worldMatrix;
};

cbuffer Morphing : register(b2)
{
	float4 distanceLUT[24];
	float4 morphRange;
};

cbuffer Planet : register(b3)
{
	float4 radius;
	float4 minAltitude;
	float4 maxAltitude;
};

struct VertexInputType
{
	float2 localPosition : TEXCOORD0;
	float2 localMorph : TEXCOORD1;
	int level : TEXTUREID;
	float3 a : POSITION0;
	float3 r : POSITION1;
	float3 s : POSITION2;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float MorphFac(float distance, int level)
{
	float low, high, delta, a;

	low = distanceLUT[level - 1].x;
	high = distanceLUT[level].x;

	delta = high - low;

	a = (distance - low) / delta;

	return (1 - clamp((a / morphRange.x), 0, 1));
}

PixelInputType main(VertexInputType input)
{
	PixelInputType output;
	float2 uv;
	float3 pos;
	float4 finalPos;
	float distance, morphPercentage;

	pos = input.a + input.r * input.localPosition.x + input.s * input.localPosition.y;

	distance = length(mul(pos, worldMatrix) - cameraPosition.xyz);
	morphPercentage = MorphFac(distance, input.level);

	pos += morphPercentage * (input.r * input.localMorph.x + input.s * input.localMorph.y);
	pos = normalize(pos);

	output.uv = float2((0.5f + (atan2(pos.z, pos.x) / (2.0f * PI))), (0.5f - (asin(pos.y) / PI)));
	pos *= 1.0f + ((heightMap.SampleLevel(sampleType, output.uv, 0).r * (maxAltitude.r - minAltitude.r) + minAltitude.r) / radius.r);

	finalPos = float4(pos * 0.5f, 1.0f);

	output.position = mul(finalPos, worldMatrix);
	output.position = mul(output.position, viewMatrix);
	output.position = mul(output.position, projectionMatrix);

	return output;
}

#type pixel

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
};

Texture2D albedo : register(t0);
SamplerState sampleType;

float4 main(PixelInputType input) : SV_TARGET
{
	float4 color = albedo.SampleLevel(sampleType, input.uv, 0).rgba;

	return color;
}