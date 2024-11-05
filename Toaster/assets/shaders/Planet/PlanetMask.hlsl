#inputlayout
vertex
instance
instance
instance
instance

#type vertex
#pragma pack_matrix( row_major )

#define PI 3.141592653589793

Texture2D HeightMapTexture		: register(t4);
Texture2D CraterMapTexture		: register(t5);

SamplerState defaultSampler;

cbuffer Camera : register(b0)
{
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

cbuffer Planet : register(b2)
{
	float4 radius;
	float4 minAltitude;
	float4 maxAltitude;
};

struct VertexInputType
{
	float2 localPosition : TEXCOORD0;
	int level : TEXTUREID;
	float3 a : POSITION0;
	float3 r : POSITION1;
	float3 s : POSITION2;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
};

float3 hash33(float3 p3)
{
	p3 = frac(p3 * float3(0.1031f, 0.11369f, 0.13787f));
	p3 += dot(p3, p3.yxz + 19.19f);
	return -1.0f + 2.0f * frac(float3((p3.x + p3.y) * p3.z, (p3.x + p3.z) * p3.y, (p3.y + p3.z) * p3.x));
}

float SimplexNoiseRaw(float3 pos)
{
	const float K1 = 0.333333333f;
	const float K2 = 0.166666667f;

	float3 i = floor(pos + (pos.x + pos.y + pos.z) * K1);
	float3 d0 = pos - (i - (i.x + i.y + i.z) * K2);

	float3 e = step(float3(0.0f, 0.0f, 0.0f), d0 - d0.yzx);
	float3 i1 = e * (1.0f - e.zxy);
	float3 i2 = 1.0f - e.zxy * (1.0f - e);

	float3 d1 = d0 - (i1 - 1.0f * K2);
	float3 d2 = d0 - (i2 - 2.0f * K2);
	float3 d3 = d0 - (1.0f - 3.0f * K2);

	float4 h = max(0.6f - float4(dot(d0, d0), dot(d1, d1), dot(d2, d2), dot(d3, d3)), 0.0f);
	float4 n = h * h * h * h * float4(dot(d0, hash33(i)), dot(d1, hash33(i + i1)), dot(d2, hash33(i + i2)), dot(d3, hash33(i + 1.0f)));

	return dot(float4(31.316f, 31.316f, 31.316f, 31.316f), n);
}

float SimplexNoise(float3 pos, float octaves, float scale, float persistence)
{
	float final = 0.0f;
	float amplitude = 1.0f;
	float maxAmplitude = 0.0f;

	for (float i = 0.0f; i < octaves; ++i)
	{
		final += SimplexNoiseRaw(pos * scale) * amplitude;
		scale *= 2.0f;
		maxAmplitude += amplitude;
		amplitude *= persistence;
	}

	return (final / maxAmplitude);
}

PixelInputType main(VertexInputType input)
{
	PixelInputType output;
	float2 texcoord;
	float3 pos;
	float4 finalPos;

	pos = input.a + input.r * input.localPosition.x + input.s * input.localPosition.y;

	pos = normalize(pos);

	float theta = atan2(pos.z, pos.x);
	float phi = asin(pos.y);

	texcoord = float2(theta / PI, phi / (PI / 2.0f));
	texcoord.x = texcoord.x * 0.5f + 0.5f;
	texcoord.y = texcoord.y * 0.5f + 0.5f;
	//output.texcoord = float2((0.5f + (atan2(pos.z, pos.x) / (2.0f * PI))), (0.5f - (asin(pos.y) / PI)));
	pos *= 1.0f + ((HeightMapTexture.SampleLevel(defaultSampler, texcoord, 0).r * (maxAltitude - minAltitude) + minAltitude) / radius);

	float craterDetected = CraterMapTexture.SampleLevel(defaultSampler, texcoord, 0).r;
	if (craterDetected == 0.0f)
	{
		float craterHeightDetail = SimplexNoise(float3(float2(8192.0f, 4096.0f) * texcoord, 1.0f), 15.0f, 0.5f, 0.5f);
		//Min and max altitude of the details are 30 and -30. check base height for information on how to change these
		pos *= 1.0f + craterHeightDetail * (0.03f / radius.r);
	}

	output.pixelPosition = mul(pos, worldMatrix);
	output.pixelPosition = mul(output.pixelPosition, viewMatrix);
	output.pixelPosition = mul(output.pixelPosition, projectionMatrix);

	return output;
}