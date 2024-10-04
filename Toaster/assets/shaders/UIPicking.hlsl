#inputlayout
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

cbuffer Model : register(b1)
{
	matrix worldMatrix;
	int entityID;
};

struct VertexInputType
{
    float2 position		: POSITION0;
    float2 size			: POSITION1;
    float2 texCoord		: TEXCOORD;
    uint EntityID		: TEXTUREID;
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
	output.position = mul(output.position, projectionMatrix);
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