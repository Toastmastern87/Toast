#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
	matrix viewProjectionMatrix;
};

struct VertexInputType
{
	float3 position : POSITION;
	float4 color : COLOR;
	float2 texcoord : TEXCOORD;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 texcoord : TEXCOORD;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = mul(float4(input.position, 1.0f), viewProjectionMatrix);

	output.color = input.color;

	output.texcoord = input.texcoord;

	return output;
}

#type pixel
struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 texcoord : TEXCOORD;
};

cbuffer Color : register(b0)
{
	float4 cbColor;
	float tilingFactor;
};

Texture2D shaderTexture;
SamplerState sampleType;

float4 main(PixelInputType input) : SV_TARGET
{
	//return shaderTexture.SampleLevel(sampleType, input.texcoord * tilingFactor, 0) * color;
	return input.color;
}