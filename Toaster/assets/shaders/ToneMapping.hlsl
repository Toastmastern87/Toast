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

#define PI 3.141592653589793

#pragma pack_matrix( row_major )

struct PixelInputType
{
	float4 position			: SV_POSITION;
	float2 texCoord			: TEXCOORD;
};

static const float3x3 ACESInputMat =
{
	{0.59719, 0.35458, 0.04823},
	{0.07600, 0.90834, 0.01566},
	{0.02840, 0.13383, 0.83777}
};

static const float3x3 ACESOutputMat =
{
	{ 1.60475, -0.53108, -0.07367},
	{-0.10208,  1.10813, -0.00605},
	{-0.00327, -0.07276,  1.07602}
};

float3 RRTAndODTFit(float3 v)
{
	float3 a = v * (v + 0.0245786f) - 0.000090537f;
	float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}

float3 AcesTonemap(float3 color) {

	float3 v = mul(ACESInputMat, color);
	float3 a = v * (v + 0.0245786f) - 0.000090537f;
	float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return pow(clamp(mul(ACESOutputMat, (a / b)), 0.0f, 1.0f), float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
}

float3 LinearTosRGB(float3 color)
{
	float3 x = color * 12.92f;
	float3 y = 1.055f * pow(saturate(color), 1.0f / 2.4f) - 0.055f;

	float3 clr = color;
	clr.r = color.r < 0.0031308f ? x.r : y.r;
	clr.g = color.g < 0.0031308f ? x.g : y.g;
	clr.b = color.b < 0.0031308f ? x.b : y.b;

	return clr;
}

float3 SRGBToLinear(float3 color)
{
	float3 x = color / 12.92f;
	float3 y = pow(max((color + 0.055f) / 1.055f, 0.0f), 2.4f);

	float3 clr = color;
	clr.r = color.r <= 0.04045f ? x.r : y.r;
	clr.g = color.g <= 0.04045f ? x.g : y.g;
	clr.b = color.b <= 0.04045f ? x.b : y.b;

	return clr;
}

float4 main(PixelInputType input) : SV_TARGET
{
	float4 color = BaseTexture.Sample(DefaultSampler, input.texCoord);

	float3 colorToned = mul(ACESInputMat, color.rgb);

	colorToned = RRTAndODTFit(colorToned);

	colorToned = mul(ACESOutputMat, colorToned);

	colorToned = saturate(colorToned);

	float lumBefore = 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;

	//Dolkar solution
	float lumAfter = (log(lumBefore + 0.5f) - log(0.5f)) / (log(10.0f + 0.5f) - log(0.5f));

	float3 colorAfter = color.rgb * (lumAfter / lumBefore);

	return float4(colorAfter, 1.0f);
	//return float4(AcesTonemap(color.rgb), 1.0f);
}
