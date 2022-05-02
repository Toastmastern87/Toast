#inputlayout
#type vertex
#pragma pack_matrix( row_major )

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float2 texCoord			: TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
	PixelInputType output;

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
	output.texCoord = float2((vID << 1) & 2, vID & 2);
	output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);

	return output;
}

#type pixel
Texture2D BaseTexture				: register(t10);

SamplerState DefaultSampler			: register(s1);

#pragma pack_matrix( row_major )

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float2 texCoord			: TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
	float4 color = BaseTexture.Sample(DefaultSampler, input.texCoord);

	return color;
}
