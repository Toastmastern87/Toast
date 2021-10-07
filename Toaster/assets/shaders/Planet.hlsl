#inputlayout
vertex
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
	float4 cameraPosition;
};

cbuffer Model : register(b1)
{
	matrix worldMatrix;
	int entityID;
};

cbuffer Planet : register(b3)
{
	float4 radius;
	float4 minAltitude;
	float4 maxAltitude;
};

struct VertexInputType
{
	float2 localPosition : TEXCOORD0;
	float2 localMorph : TEXCOORD1;
	int level : TEXTUREID;
	float3 a : POSITION0;
	float3 r : POSITION1;
	float3 s : POSITION2;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float2 texcoord			: TEXCOORD0;
	float3 cameraPos		: POSITION1;
	int entityID			: TEXTUREID;
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
	float2 uv;
	float3 pos;
	float4 finalPos;
	float distance, morphPercentage;

	pos = input.a + input.r * input.localPosition.x + input.s * input.localPosition.y;

	distance = length(mul(pos, worldMatrix) - cameraPosition.xyz);

	pos = normalize(pos);

	output.texcoord = float2((0.5f + (atan2(pos.z, pos.x) / (2.0f * PI))), (0.5f - (asin(pos.y) / PI)));
	pos *= 1.0f + ((HeightMapTexture.SampleLevel(defaultSampler, output.texcoord, 0).r * (maxAltitude.r - minAltitude.r) + minAltitude.r) / radius.r);

	float craterDetected = CraterMapTexture.SampleLevel(defaultSampler, output.texcoord, 0).r;
	if (craterDetected == 0.0f)
	{
		float craterHeightDetail = SimplexNoise(float3(float2(8192.0f, 4096.0f) * output.texcoord, 1.0f), 15.0f, 0.5f, 0.5f);
		//Min and max altitude of the details are 30 and -30. check base height for information on how to change these
		pos *= 1.0f + craterHeightDetail * (0.03f / radius.r);
	}

	finalPos = float4(pos * 0.5f, 1.0f);

	output.pixelPosition = mul(finalPos, worldMatrix);
	output.pixelPosition = mul(output.pixelPosition, viewMatrix);
	output.pixelPosition = mul(output.pixelPosition, projectionMatrix);

	output.worldPosition = mul(finalPos, worldMatrix).xyz;
	output.cameraPos = cameraPosition.xyz;

	output.entityID = entityID;

	return output;
}

#type pixel
// Constant normal incidence Fresnel factor for all dielectrics.
static const float3 Fdielectric = 0.04f;
static const float Epsilon = 0.00001f;
static const float PI = 3.141592f;

cbuffer DirectionalLight : register(b0)
{
	float4 direction;
	float4 radiance;
	float multiplier;
	float sunDiscToggle;
};

cbuffer PlanetPS : register(b1)
{
	float4 radius;
	float4 minAltitude;
	float4 maxAltitude;
};

#pragma pack_matrix( row_major )
	
struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float2 texcoord			: TEXCOORD0;
	float3 cameraPos		: POSITION1;
	int entityID			: TEXTUREID;
};

struct PixelOutputType
{
	float4 Color			: SV_Target0;
	int EntityID			: SV_Target1;
};

TextureCube IrradianceTexture	: register(t0);
TextureCube RadianceTexture		: register(t1);
Texture2D specularBRDFLUT		: register(t2);
Texture2D AlbedoTexture			: register(t3);
Texture2D HeightMapTexture		: register(t4);
Texture2D CraterMapTexture		: register(t5);

SamplerState defaultSampler		: register(s0);
SamplerState spBRDFSampler		: register(s1);

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

float GetHeight(float2 uv)
{
	float finalHeight;

	float baseHeight = (HeightMapTexture.SampleLevel(defaultSampler, uv, 0).r * (maxAltitude.x + minAltitude.x) + minAltitude.x);

	float craterDetected = CraterMapTexture.SampleLevel(defaultSampler, uv, 0).r;
	if (craterDetected == 0.0f)
	{
		float craterHeightDetail = SimplexNoise(float3(float2(8192.0f, 4096.0f) * uv, 1.0f), 15.0f, 0.5f, 0.5f);
		//Min and max altitude of the details are 30 and -30. check base height for information on how to change these
		baseHeight *= 1.0f + (((craterHeightDetail + 1.0f) * 0.5f) * (0.06f) - 0.03f);
	}

	finalHeight = baseHeight;

	return finalHeight; 
}

float3 CalculateNormal(float3 normalVector, float2 uv)
{
	float textureWidth, textureHeight, hL, hR, hD, hU;
	float3 texOffset, N;
	float3x3 TBN;

	HeightMapTexture.GetDimensions(textureWidth, textureHeight);

	texOffset = float3((1.0f / (textureWidth)), (1.0f / (textureHeight)), 0.0f);

	if (uv.x >= (1.0f - (1.0f / textureWidth)))
	{
		hL = GetHeight(float2(0.0f, uv.y));
		hR = GetHeight(float2(0.0f, uv.y));
		hD = GetHeight((uv + texOffset.zy));
		hU = GetHeight((uv - texOffset.zy));
	}
	else if (uv.x <= (1.0f / textureWidth))
	{
		hL = GetHeight(float2(textureWidth, uv.y));
		hR = GetHeight(float2(textureWidth, uv.y));
		hD = GetHeight((uv + texOffset.zy));
		hU = GetHeight((uv - texOffset.zy));
	}
	else
	{
		hL = GetHeight((uv - texOffset.xz));
		hR = GetHeight((uv + texOffset.xz));
		hD = GetHeight((uv + texOffset.zy));
		hU = GetHeight((uv - texOffset.zy));
	}

	N = normalize(float3(hL - hR, hU - hD, 2.0f));
	float3 norm = normalize(normalVector);
	float3 up = float3(0.0f, 1.0f, 0.0f) - norm;
	float3 tang = normalize(cross(norm, up));//might need flipping
	float3 biTan = normalize(cross(norm, tang));//same
	float3x3 localAxis = float3x3(tang, biTan, norm);

	return normalize(mul(normalize(N), localAxis));
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2
float ndfGGX(float cosLh, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0f) + 1.0f;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0f - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float NdotV, float roughness)
{
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) * gaSchlickG1(NdotV, k);
}

// Shlick's approximation of the Fresnel factor.
float3 fresnelSchlick(float3 F0, float cosTheta)
{
	return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float3 fresnelSchlickRoughness(float3 F0, float cosTheta, float roughness)
{
	return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

// Returns number of mipmap levels for specular IBL environment map.
uint queryRadianceTextureLevels()
{
	uint width, height, levels;
	RadianceTexture.GetDimensions(0, width, height, levels);
	return levels;
}

PixelOutputType main(PixelInputType input) : SV_TARGET
{
	PixelOutputType output;

	float3 albedo = AlbedoTexture.SampleLevel(defaultSampler, input.texcoord, 0).rgb;
	float metalness = 0.0f;
	float roughness = 0.8f;
	roughness = max(roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight

	// Outgoing light direction (vector from world-space fragment position to the "eye").
	float3 Lo = normalize(input.cameraPos - input.worldPosition);

	float3 n = normalize(input.worldPosition);

	// Get current fragment's normal and transform to world space.
	float3 N = CalculateNormal(input.worldPosition, input.texcoord);

	// Angle between surface normal and outgoing light direction.
	float cosLo = max(0.0f, dot(N, Lo));

	// Specular reflection vector.
	float3 Lr = 2.0f * cosLo * N - Lo;

	// Fresnel reflectance at normal incidence (for metals use albedo color).
	float3 F0 = lerp(Fdielectric, albedo, metalness);

	// Direct lighting calculation for analytical lights.
	float3 directLighting = 0.0f;
	{
		float3 Li = direction;
		float3 Lradiance = radiance * multiplier;

		// Half-vector between Li and Lo.
		float3 Lh = normalize(Li + Lo);

		// Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.0f, dot(N, Li));
		float cosLh = max(0.0f, dot(N, Lh));

		// Calculate Fresnel term for direct lighting. 
		float3 F = fresnelSchlick(F0, max(0.0f, dot(Lh, Lo)));
		// Calculate normal distribution for specular BRDF.
		float D = ndfGGX(cosLh, roughness);
		// Calculate geometric attenuation for specular BRDF.
		float G = gaSchlickGGX(cosLi, cosLo, roughness);

		// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
		// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
		// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
		float3 kd = lerp(float3(1.0f, 1.0f, 1.0f) - F, float3(0.0f, 0.0f, 0.0f), metalness);

		// Lambert diffuse BRDF.
		// We don't scale by 1/PI for lighting & material units to be more convenient.
		// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
		float3 diffuseBRDF = kd * albedo;

		// Cook-Torrance specular microfacet BRDF.
		float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * cosLo);

		// Total contribution for this light.
		directLighting = (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
	}

	// Ambient lighting (IBL).
	float3 ambientLighting;
	{
		// Sample diffuse irradiance at normal direction.
		float3 irradiance = IrradianceTexture.Sample(defaultSampler, N).rgb;

		// Calculate Fresnel term for ambient lighting.
		// Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
		// use cosLo instead of angle with light's half-vector (cosLh above).
		// See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
		float3 F = fresnelSchlick(F0, cosLo);

		// Get diffuse contribution factor (as with direct lighting).
		float3 kd = lerp(1.0 - F, 0.0, metalness);

		// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
		float3 diffuseIBL = kd * albedo * irradiance;

		// Sample pre-filtered specular reflection environment at correct mipmap level.
		uint specularTextureLevels = queryRadianceTextureLevels();
		float3 specularIrradiance = RadianceTexture.SampleLevel(defaultSampler, Lr, roughness * specularTextureLevels).rgb;

		// Split-sum approximation factors for Cook-Torrance specular BRDF.
		float2 specularBRDF = specularBRDFLUT.Sample(spBRDFSampler, float2(cosLo, roughness)).rg;

		// Total specular IBL contribution.
		float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;

		// Total ambient lighting contribution.
		ambientLighting = diffuseIBL + specularIBL;
	}

	output.Color = float4(directLighting + ambientLighting, 1.0f);

	output.EntityID = input.entityID + 1;

	return output;
}