Texture2D shaderTexture;
SamplerState sampleType;

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

float4 main(PixelInputType input) : SV_TARGET
{
	return shaderTexture.SampleLevel(sampleType, input.texcoord, 0);
}