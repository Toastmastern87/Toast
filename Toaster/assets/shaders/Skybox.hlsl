#inputlayout
vertex
vertex
vertex
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

cbuffer SkyboxTransforms : register(b1)
{
    matrix viewMatrix;
    matrix projectionMatrix;
	float4 cameraPosition;
}

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
	float4 svpos		: SV_POSITION;
    float3 pos			: POSITION;
	float3 cameraPos	: POSITION1;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;

	output.pos = input.position;
	float3 viewPos = mul(input.position, (float3x3)viewMatrix);
	float4 finalPos = mul(float4(viewPos, 1.0f), projectionMatrix);
	output.svpos = finalPos.xyww;
	output.cameraPos = mul(cameraPosition, viewMatrix);
	output.cameraPos = mul(output.cameraPos, projectionMatrix);

	return output;
}

#type pixel
static const float PI = 3.141592f;

cbuffer DirectionalLight : register(b0)
{
	float4 direction;
	float4 radiance;
	float multiplier;
	float sunDiscToggle;
};

cbuffer Environment : register(b1)
{
	float environmentStrength;
	float textureLOD;
	float2 padding;
}

struct PixelInputType
{
	float4 svpos		: SV_POSITION;
	float3 pos			: POSITION;
	float3 cameraPos	: POSITION1;
};

TextureCube skybox : register(t3);
SamplerState sampleType;

float4 main(PixelInputType input) : SV_Target
{
	float3 sunColor, skycolor;

	if (sunDiscToggle == 1.0f)
	{
		float sun = 1.0f - acos(dot(normalize(input.pos), normalize(direction)));
		sun = clamp(sun, 0.0f, 1.0f);

		float glow = sun;
		glow = clamp(glow, 0.0f, 1.0f);

		sun = pow(sun, 100.0f);
		sun *= 100.0f;
		sun = clamp(sun, 0.0f, 1.0f);

		glow = pow(glow, 6.0f) * 1.0f;
		glow = pow(glow, 1.0f);
		glow = clamp(glow, 0.0f, 1.0f);

		sun *= pow(1.0f, 1.0f / 1.65f);

		glow *= pow(1.0f, 1.0f / 2.0f);

		sun += glow;

		sunColor = radiance * sun;
	}
	else
	{
		sunColor = 0.0f;
	}

	skycolor = skybox.SampleLevel(sampleType, normalize(input.pos), textureLOD).rgb * environmentStrength;

	return float4(skycolor + sunColor, 1.0f);
}