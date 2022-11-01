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
	float2 texCoord			: TEXCOORD;
};

struct PixelInputType
{
	float4 position			: SV_POSITION;
	int entityID : TEXTUREID;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = mul(float4(input.position.xy, 1.0f, 1.0f), worldMatrix);
	output.position.x = ((output.position.x / width) * 2.0f) - 1.0f;
	output.position.y = ((output.position.y / height) * 2.0f) + 1.0f;
	output.position.z = 1.0f;
	output.position.w = 1.0f;

	output.entityID = entityID;

	return output;
}

#type pixel
struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	int entityID			: TEXTUREID;
};

struct PixelOutputType
{
	int EntityID : SV_Target;
};

PixelOutputType main(PixelInputType input) : SV_TARGET
{
	PixelOutputType output;

	output.EntityID = input.entityID + 1;

	return output;
}