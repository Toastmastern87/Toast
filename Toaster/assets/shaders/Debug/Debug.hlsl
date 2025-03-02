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
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;
	
    float4 worldPosition;
    if (noWorldTransform == 1)
    {
        worldPosition = float4(input.position, 1.0f);
    }
    else
    {
        worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
    }

    output.position = mul(worldPosition, viewMatrix);
	output.position = mul(output.position, projectionMatrix);

    output.color = float4(input.color, 1.0f);

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