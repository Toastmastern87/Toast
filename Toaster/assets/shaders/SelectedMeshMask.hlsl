#inputlayout
vertex

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

cbuffer Model : register(b1)
{
	matrix worldMatrix;
	int entityID;
};

struct VertexInputType
{
	float3 position			: POSITION;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.pixelPosition = mul(float4(input.position, 1.0f), worldMatrix);
	output.pixelPosition = mul(output.pixelPosition, viewMatrix);
	output.pixelPosition = mul(output.pixelPosition, projectionMatrix);

	return output;
}

#type pixel
struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
};

float4 main(PixelInputType input) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}