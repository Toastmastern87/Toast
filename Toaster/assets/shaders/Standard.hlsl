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
    matrix worldMovementMatrix;
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
    int planet;
};

struct VertexInputType
{
	float3 position			: POSITION;
	float3 normal			: NORMAL;
	float4 tangent			: TANGENT;
	float2 texcoord			: TEXCOORD;
	float3 color			: COLOR0;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD;
	float3 cameraPos		: POSITION1;
    float3 color			: COLOR0;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.pixelPosition = mul(float4(input.position, 1.0f), worldMatrix);
	output.pixelPosition = mul(output.pixelPosition, viewMatrix);
	output.pixelPosition = mul(output.pixelPosition, projectionMatrix);

	output.worldPosition = mul(float4(input.position, 1.0f), worldMatrix).xyz;

	output.normal = mul(input.normal, (float3x3)worldMatrix);
	output.texcoord = float2(input.texcoord.x, 1.0f - input.texcoord.y);
	output.cameraPos = cameraPosition.xyz;
    output.color = input.color;

	return output;
}

#type pixel
cbuffer DirectionalLight : register(b3)
{
	float4 direction;
	float4 radiance;
	float multiplier;
	float sunDiscToggle;
};

cbuffer Material			: register(b2)
{
	float4 Albedo;
	float Emission;
	float Metalness;
	float Roughness;
	int AlbedoTexToggle;
	int NormalTexToggle;
	int MetalRoughTexToggle;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD;
	float3 cameraPos		: POSITION1;
    float3 color			: COLOR0;
};

cbuffer RenderSettings : register(b9)
{
    float renderOverlay;
    float3 padding;
};

float4 main(PixelInputType input) : SV_TARGET
{
    return float4(input.color, 1.0f);
}