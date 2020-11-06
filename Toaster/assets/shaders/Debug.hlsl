#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
	matrix viewMatrix;
	matrix projectionMatrix;
};

struct VertexInputType
{
	float3 position : POSITION;
	float4 color : COLOR;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = mul(float4(input.position, 1.0f), viewMatrix);
	output.position = mul(output.position, projectionMatrix);

	output.color = input.color;

	return output;
}

#type pixel
struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

float4 main(PixelInputType input) : SV_TARGET
{
	return input.color;
}