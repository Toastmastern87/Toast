#inputlayout
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

cbuffer UI : register(b12)
{
	float width;
	float height;
};

cbuffer Model : register(b1)
{
	matrix worldMatrix;
	int entityID;
};

struct VertexInputType
{
	float3 position			: POSITION;
	float4 color			: COLOR;
};

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float4 color			: COLOR;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position.x = ((input.position.x / width) * 2.0f) - 1.0f;
	output.position.y = ((input.position.y / height) * 2.0f) + 1.0f;
	output.position.z = 1.0f;
	output.position.w = 1.0f;

	output.color = input.color;

	return output;
}

#type pixel
struct PixelInputType
{
	float4 position			: SV_POSITION;
	float4 color			: COLOR;
};

float4 main(PixelInputType input) : SV_TARGET
{
	return input.color;
}