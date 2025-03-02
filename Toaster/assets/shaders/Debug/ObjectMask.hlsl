#inputlayout
vertex
vertex
vertex
vertex
vertex
instance

#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
    matrix worldMovementMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
	matrix inverseViewMatrix;
	matrix inverseProjectionMatrix;
	float4 cameraPosition;
	float far;
	float near;
    float viewportWidth;
    float viewportHeight;
};

cbuffer Model : register(b1)
{
    matrix worldMatrix;
    int entityID;
    int noWorldTransform;
    int isInstanced;
};

struct VertexInputType
{
    float3 position					: POSITION0;
    float3 normal					: NORMAL;
    float4 tangent					: TANGENT;
    float2 texCoord					: TEXCOORD;
    float3 color					: COLOR0;
    float3 worldInstancePosition	: POSITION1;
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