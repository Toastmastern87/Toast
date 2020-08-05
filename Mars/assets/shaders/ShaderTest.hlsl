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
};

struct PixelInputType
{
	float4 position : SV_POSITION;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = mul(float4(input.position, 1.0f), transform);
	output.position = mul(output.position, viewProjectionMatrix);

	return output;
}

#type pixel
cbuffer Color : register(b0)
{
	float4 color;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
};

float4 main(PixelInputType input) : SV_TARGET
{
	return color;
}