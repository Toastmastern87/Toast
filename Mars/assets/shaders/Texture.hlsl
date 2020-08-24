#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
	matrix viewProjectionMatrix;
};

struct VertexInputType
{
	float3 position : POSITION;
	float4 color : COLOR;
	float2 texcoord : TEXCOORD;
	float texindex : PSIZE0;
	float tilingfactor : PSIZE1;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 texcoord : TEXCOORD;
	float texindex : PSIZE0;
	float tilingfactor : PSIZE1;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.position = mul(float4(input.position, 1.0f), viewProjectionMatrix);

	output.color = input.color;

	output.texcoord = input.texcoord;
	output.texindex = input.texindex;
	output.tilingfactor = input.tilingfactor;

	return output;
}

#type pixel
struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 texcoord : TEXCOORD;
	float texindex : PSIZE0;
	float tilingfactor : PSIZE1;
};

cbuffer Color : register(b0)
{
	float4 cbColor;
	float tilingFactor;
};

Texture2D shaderTexture0 : register(t0);
Texture2D shaderTexture1 : register(t1);
Texture2D shaderTexture2 : register(t2);
Texture2D shaderTexture3 : register(t3);
Texture2D shaderTexture4 : register(t4);
SamplerState sampleType;

float4 main(PixelInputType input) : SV_TARGET
{

	switch ((int)input.texindex)
	{
		case 0:
			return shaderTexture0.SampleLevel(sampleType, input.texcoord * input.tilingfactor, 0) * input.color;
		case 1:
			return shaderTexture1.SampleLevel(sampleType, input.texcoord * input.tilingfactor, 0) * input.color;
		case 2:
			return shaderTexture2.SampleLevel(sampleType, input.texcoord * input.tilingfactor, 0) * input.color;
		default:
			return shaderTexture0.SampleLevel(sampleType, input.texcoord * input.tilingfactor, 0) * input.color;
	}
}