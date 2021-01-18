#inputlayout
vertex
vertex
vertex
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

cbuffer SkyboxTransforms : register(b0)
{
    matrix viewMatrix;
    matrix projectionMatrix;
}

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
	float4 svpos : SV_POSITION;
    float3 pos : POSITION;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;

	output.pos = input.position;
	float3 viewPos = mul(input.position, (float3x3)viewMatrix);
	float4 finalPos = mul(float4(viewPos, 1.0f), projectionMatrix);
	output.svpos = finalPos.xyww;

	return output;
}

#type pixel
cbuffer Environment : register(b0)
{
	float4 environmentStrength;
}

struct PixelInputType
{
	float4 svpos : SV_POSITION;
	float3 pos : POSITION;
};

TextureCube skybox : register(t0);
SamplerState sampleType;

float4 main(PixelInputType input) : SV_Target
{
	float3 skycolor = skybox.SampleLevel(sampleType, normalize(input.pos), 0).rgb * environmentStrength.x;

	return float4(skycolor, 1.0);
}