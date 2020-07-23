#pragma pack_matrix( row_major )

cbuffer Camera
{
	matrix viewProjectionMatrix;
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

	output.position = mul(float4(input.position, 1.0f), viewProjectionMatrix);
	output.color = input.color;

	return output;
}