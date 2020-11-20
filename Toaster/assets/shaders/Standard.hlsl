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
	float4 cameraPosition;
};

cbuffer Model : register(b1)
{
	matrix worldMatrix;
};

struct VertexInputType
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal: BINORMAL;
	float2 texcoord : TEXCOORD;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal: BINORMAL;
	float2 texcoord : TEXCOORD;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = mul(float4(input.position, 1.0f), worldMatrix);
	output.position = mul(output.position, viewMatrix);
	output.position = mul(output.position, projectionMatrix);

	output.normal = input.normal;
	output.tangent = input.tangent;
	output.binormal = input.binormal;
	output.texcoord = input.texcoord;

	return output;
}

#type pixel
struct PixelInputType
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal: BINORMAL;
	float2 texcoord : TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
	float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);

	return color;
}