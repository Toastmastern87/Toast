#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
	matrix viewProjectionMatrix;
};

cbuffer Transform : register(b1)
{
	matrix transform;
};

struct VertexInputType
{
	float3 position : POSITION;
	float2 texcoord : TEXCOORD;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = mul(float4(input.position, 1.0f), transform);
	output.position = mul(output.position, viewProjectionMatrix);

	output.texcoord = input.texcoord;

	return output;
}

#type pixel
struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};


Texture2D shaderTexture;
SamplerState sampleType;

float4 main(PixelInputType input) : SV_TARGET
{
	return shaderTexture.SampleLevel(sampleType, input.texcoord * 10.0f, 0);
}