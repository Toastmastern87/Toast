#inputlayout
vertex
vertex
vertex
vertex
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
	float far;
	float near;
};

struct VertexInputType
{
	float3 position			: POSITION;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 bitangent		: BITANGENT;
	float2 texcoord			: TEXCOORD;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	int entityID			: TEXTUREID;
};

cbuffer Model : register(b1)
{
	matrix worldMatrix;
	int entityID;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.pixelPosition = mul(float4(input.position, 1.0f), worldMatrix);
	output.pixelPosition = mul(output.pixelPosition, viewMatrix);
	output.pixelPosition = mul(output.pixelPosition, projectionMatrix);

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
	int EntityID			: SV_Target;
};

PixelOutputType main(PixelInputType input) : SV_TARGET
{
	PixelOutputType output;

	output.EntityID = input.entityID + 1;

	return output;
}