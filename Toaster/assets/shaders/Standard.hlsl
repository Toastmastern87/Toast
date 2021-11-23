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
	int entityID;
};

struct VertexInputType
{
	float3 position		: POSITION;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
	float3 binormal		: BINORMAL;
	float2 texcoord		: TEXCOORD;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD;
	float3 cameraPos		: POSITION1;
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

	return output;
}

#type pixel
cbuffer DirectionalLight : register(b0)
{
	float4 direction;
	float4 radiance;
	float multiplier;
	float sunDiscToggle;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD;
	float3 cameraPos		: POSITION1;
};

struct PixelOutputType
{
	float4 Color			: SV_Target;
};

PixelOutputType main(PixelInputType input) : SV_TARGET
{
	PixelOutputType output;

	//output.Color = float4(1.0f * multiplier, 1.0f * multiplier, 1.0f * multiplier, 1.0f);
	output.Color = float4(1.0f, 0.0f, 0.0f, 1.0f);
	//output.Color = float4(0.0f, 0.0f, 1.0f, 1.0f);

	return output;
}