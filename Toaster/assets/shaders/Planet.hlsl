#inputlayout
vertex
vertex
instance
instance
instance
instance

#type vertex
#pragma pack_matrix( row_major )

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
	float4 distanceLUT[22];
	float4 morphRange;
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
	float3 pos;
	float4 finalPos;
	float distance, morphPercentage;

	pos = input.a + input.r * input.localPosition.x + input.s * input.localPosition.y;

	distance = length(mul(pos, worldMatrix) - cameraPosition.xyz);
	morphPercentage = MorphFac(distance, input.level);

	pos += morphPercentage * (input.r * input.localMorph.x + input.s * input.localMorph.y);

	finalPos = float4(normalize(pos) * 0.5f, 1.0f);

	output.position = mul(finalPos, worldMatrix);
	output.position = mul(output.position, viewMatrix);
	output.position = mul(output.position, projectionMatrix);

	return output;
}

#type pixel
struct PixelInputType
{
	float4 position : SV_POSITION;
};

Texture2D albedo : register(t0);
SamplerState sampleType;

float4 main(PixelInputType input) : SV_TARGET
{
	float4 color = albedo.SampleLevel(sampleType, float2(0,0), 0).rgba;

	return color;
}