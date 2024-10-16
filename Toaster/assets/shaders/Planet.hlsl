#inputlayout
vertex
vertex
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

#define PI 3.141592653589793

//Texture2D HeightMapTexture		: register(t4);
//Texture2D CraterMapTexture		: register(t5);

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
};

struct VertexInputType
{
	float3 globalPosition	: POSITION0;
	float3 normal			: NORMAL;
	float4 tangent			: TANGENT;
	float2 texcoord			: TEXCOORD0;
	float3 color			: COLOR0;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float2 texcoord			: TEXCOORD0;
	float3 cameraPos		: POSITION1;
	float3 color			: COLOR0;
    float3 normal			: NORMAL0;
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
	float3 pos;
	float4 finalPos;

	output.texcoord = input.texcoord;

	output.pixelPosition = float4(input.globalPosition, 1.0f);
	output.worldPosition = input.globalPosition;
	output.normal = input.normal;
	output.pixelPosition = mul(output.pixelPosition, viewMatrix);
	output.pixelPosition = mul(output.pixelPosition, projectionMatrix);
	output.color = input.color;

	output.cameraPos = cameraPosition.xyz;

	return output;
}

#type pixel
// Constant normal incidence Fresnel factor for all dielectrics.
static const float3 Fdielectric = 0.04f;
static const float Epsilon = 0.00001f;
static const float PI = 3.141592f;

cbuffer DirectionalLight	: register(b0)
{
	float4 direction;
	float4 radiance;
	float multiplier;
	float sunDiscToggle;
};

cbuffer Material			: register(b1)
{
	float4 Albedo;
	float Emission;
	float Metalness;
	float Roughness;
	int AlbedoTexToggle;
	int NormalTexToggle;
	int MetalRoughTexToggle;
};

cbuffer RenderSettings : register(b9)
{
    float renderOverlay;
    float3 padding;
};

#pragma pack_matrix( row_major )
	
struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float2 texcoord			: TEXCOORD0;
	float3 cameraPos		: POSITION1;
	float3 color			: COLOR0;
    float3 normal			: NORMAL0;
};

struct PixelOutputType
{
	float4 Color			: SV_Target;
};

struct PBRParameters
{
	float3 Albedo;
	float Metalness;
	float Roughness;
	float AO;
};

TextureCube IrradianceTexture	: register(t0);
TextureCube RadianceTexture		: register(t1);
Texture2D SpecularBRDFLUT		: register(t2);
Texture2D AlbedoTexture			: register(t3);
Texture2D MetalRoughTexture		: register(t5);

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

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r * r) / 8.0f;

	float nom = NdotV;
	float denom = NdotV * (1.0f - k) + k;

	return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
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

// ---------------------------------------------------------------------------------------------------
// The following code (from Unreal Engine 4's paper) shows how to filter the environment map
// for different roughnesses. This is mean to be computed offline and stored in cube map mips,
// so turning this on online will cause poor performance
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N)
{
	float a = roughness * roughness;
	float Phi = 2.0f * PI * Xi.x;
	float CosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
	float SinTheta = sqrt(1.0f - CosTheta * CosTheta);
	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;
	float3 UpVector = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 TangentX = normalize(cross(UpVector, N));
	float3 TangentY = cross(N, TangentX);
	// Tangent to world space
	return TangentX * H.x + TangentY * H.y + N * H.z;
}

float3 PrefilterEnvMap(float roughness, float3 R)
{
	float TotalWeight = 0.0;
	float3 N = R;
	float3 V = R;
	float3 PrefilteredColor = float3(0.0f, 0.0f, 0.0f);
	int NumSamples = 1024;
	for (int i = 0; i < NumSamples; i++)
	{
		float2 Xi = Hammersley(i, NumSamples);
		float3 H = ImportanceSampleGGX(Xi, roughness, N);
		float3 L = 2.0f * dot(V, H) * H - V;
		float NoL = clamp(dot(N, L), 0.0f, 1.0f);
		if (NoL > 0)
		{
			PrefilteredColor += IrradianceTexture.Sample(defaultSampler, L).rgb * NoL;
			TotalWeight += NoL;
		}
	}
	return PrefilteredColor / TotalWeight;
}

// Returns number of mipmap levels for specular IBL environment map.
uint queryRadianceTextureLevels()
{
	uint width, height, levels;
	RadianceTexture.GetDimensions(0, width, height, levels);
	return levels;
}

float3 DirectionalLightning(float3 F0, float3 Normal, float3 View, float NdotV, float3 albedo, float roughness, float metalness)
{
	float3 result = float3(0.0f, 0.0f, 0.0f);

	float3 Li = direction;
	float3 Lradiance = radiance * multiplier;
	float3 Lh = normalize(Li + View);

	// Calculate angles between surface normal and various light vectors.
	float cosLi = max(0.0f, dot(Normal, Li));
	float cosLh = max(0.0f, dot(Normal, Lh));

	float3 F = fresnelSchlick(F0, max(0.0f, dot(Lh, View)));
	float D = ndfGGX(cosLh, roughness);
	float G = gaSchlickGGX(cosLi, NdotV, roughness);

	float3 kd = (1.0f - F) * (1.0f - metalness);
	float3 diffuseBRDF = (kd * albedo) / PI;

	// Cook-Torrance
	float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * cosLi * NdotV);

	result += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;

	return result;
}

// Ambient Lightning
float3 IBL(float3 F0, float3 Lr, float3 Normal, float3 View, float NdotV, float3 albedo, float roughness, float metalness)
{
	float3 irradiance = IrradianceTexture.Sample(defaultSampler, Normal).rgb;
	float3 F = fresnelSchlickRoughness(F0, NdotV, roughness);
	float3 kd = (1.0f - F) * (1.0f - metalness);
	float3 diffuseIBL = albedo * irradiance;

	uint specularTextureLevels = queryRadianceTextureLevels();
	float NoV = clamp(NdotV, 0.0f, 1.0f);
	float3 R = 2.0f * dot(View, Normal) * Normal - View;
	float3 specularIrradiance = RadianceTexture.SampleLevel(defaultSampler, Lr, roughness * specularTextureLevels).rgb;

	// Sample BRDF Lut, 1.0 - roughness for y-coord because texture was generated (in Sparky) for gloss model
	float2 specularBRDF = SpecularBRDFLUT.Sample(spBRDFSampler, float2(NdotV, 1.0f - roughness)).rg;
	float3 specularIBL = specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);

	return kd * diffuseIBL + specularIBL;
}

PixelOutputType main(PixelInputType input) : SV_TARGET
{
	PixelOutputType output;
	PBRParameters params;

	// Sample input textures to get shading model params.
	params.Albedo = AlbedoTexToggle == 1 ? AlbedoTexture.Sample(defaultSampler, input.texcoord).rgb : Albedo.rgb;
	params.Metalness = MetalRoughTexToggle == 1 ? MetalRoughTexture.Sample(defaultSampler, input.texcoord).r : Metalness;
	params.Roughness = MetalRoughTexToggle == 1 ? MetalRoughTexture.Sample(defaultSampler, input.texcoord).b : Roughness;
	params.Roughness = max(params.Roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight

	// Outgoing light direction (vector from world-space fragment position to the "eye").
	float3 Lo = normalize(input.cameraPos - input.worldPosition);

	// Get current fragment's normal and transform to world space.
    float3 N = input.normal; // CalculateNormal(input.worldPosition - planetCenter, input.texcoord);

	// Angle between surface normal and outgoing light direction.
	float cosLo = max(dot(N, Lo), 0.0f);

	// Specular reflection vector.
	float3 Lr = 2.0f * cosLo * N - Lo;

	// Fresnel reflectance at normal incidence (for metals use albedo color).
	float3 F0 = lerp(Fdielectric, params.Albedo, params.Metalness);

	float3 lightContribution = DirectionalLightning(F0, N, Lo, cosLo, params.Albedo, params.Roughness, params.Metalness);
	float3 iblContribution = IBL(F0, Lr, N, Lo, cosLo, params.Albedo, params.Roughness, params.Metalness);

    if (renderOverlay > 0.5f && renderOverlay <= 1.5f)
        output.Color = float4(params.Albedo, 1.0f);
    else if (renderOverlay > 1.5f && renderOverlay <= 2.5f)
        output.Color = float4(N * 0.5 + 0.5, 1.0f);
    else if (renderOverlay > 2.5f && renderOverlay <= 3.5f)
        output.Color = float4(N * 0.5 + 0.5, 1.0f);
    else if (renderOverlay > 3.5f && renderOverlay <= 4.5f)
        output.Color = float4(input.color, 1.0f);
    else
        output.Color = float4(lightContribution + iblContribution, 1.0f);

	return output;
}