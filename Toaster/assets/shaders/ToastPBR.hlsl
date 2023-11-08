#inputlayout
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
	float3 position			: POSITION;
	float3 normal			: NORMAL;
	float4 tangent			: TANGENT;
	float2 texcoord			: TEXCOORD;
	float3 color			: COLOR0;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD;
	float3 cameraPos		: POSITION1;
	float3x3 tangentBasis	: TBASIS;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.pixelPosition = mul(float4(input.position, 1.0f), worldMatrix);
	output.pixelPosition = mul(output.pixelPosition, viewMatrix);
	output.pixelPosition = mul(output.pixelPosition, projectionMatrix);

	output.worldPosition = mul(float4(input.position, 1.0f), worldMatrix).xyz;

	float3 worldNormal = mul(input.normal, (float3x3)worldMatrix);
	float3 worldTangent = mul(input.tangent.xyz, (float3x3)worldMatrix);
	float3 worldBitangent = cross(worldNormal, worldTangent) * input.tangent.w;
	float3x3 TBN = float3x3(worldTangent, worldBitangent, worldNormal);
	output.tangentBasis = mul((float3x3)worldMatrix, TBN);

	output.normal = worldNormal;
	output.texcoord = input.texcoord;
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

cbuffer Planet : register(b2)
{
	float radius;
	float minAltitude;
	float maxAltitude;
	float atmosphereHeight;
	float mieAnisotropy;
	float rayScaleHeight;
	float mieScaleHeight;
	float3 rayBaseScatteringCoefficient;
	float mieBaseScatteringCoefficient;
	float3 planetCenter;
	int atmosphereToggle;
	int numInScatteringPoints;
	int numOpticalDepthPoints;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD;
	float3 cameraPos		: POSITION1;
	float3x3 tangentBasis	: TBASIS;
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
Texture2D NormalTexture			: register(t4);
Texture2D MetalRoughTexture		: register(t5);

SamplerState defaultSampler		: register(s0);
SamplerState spBRDFSampler		: register(s1);

float3 CalculateNormalFromMap(float3 normal, float3 tangent, float3 bitangent, float2 texCoords)
{
	float3 Normal = normalize(normal);
	float3 Tangent = normalize(tangent);
	float3 Bitangent = normalize(bitangent);
	float3 BumpMapNormal = NormalTexture.Sample(defaultSampler, texCoords).xyz;
	BumpMapNormal = 2.0f * BumpMapNormal - 1.0f;

	float3 NewNormal;
	float3x3 TBN = float3x3(Tangent, Bitangent, Normal);
	NewNormal = mul(transpose(TBN), BumpMapNormal);
	NewNormal = normalize(NewNormal);
	return NewNormal;
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

float3 DirectionalLightning(float3 F0, float3 Normal, float3 View, float cosLo, float3 albedo, float roughness, float metalness)
{
	//float3 result = float3(0.0f, 0.0f, 0.0f);

	//float3 Li = direction;
	//float3 Lradiance = radiance * multiplier;
	//float3 Lh = normalize(Li + View);

	//// Calculate angles between surface normal and various light vectors.
	//float cosLi = max(0.0f, dot(Normal, Li));
	//float cosLh = max(0.0f, dot(Normal, Lh));

	//float3 F = fresnelSchlick(F0, max(0.0f, dot(Lh, View)));
	//float D = ndfGGX(cosLh, roughness);
	//float G = gaSchlickGGX(cosLi, cosLo, roughness);

	//float3 kd = lerp(float3(1.0f, 1.0f, 1.0f) - F, float3(0.0f, 0.0f, 0.0f), (1.0f - metalness));
	//float3 diffuseBRDF = (kd * albedo) / PI;

	//// Cook-Torrance
	//float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * cosLi * cosLo);

	//result += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;

	//return result;

	float3 result = float3(0.0f, 0.0f, 0.0f);

	float3 Li = direction;
	float3 Lradiance = radiance * multiplier;
	float3 Lh = normalize(-Li + View);

	// Calculate angles between surface normal and various light vectors.
	float cosLi = max(0.0f, dot(Normal, Li));
	float cosLh = max(0.0f, dot(Normal, Lh));

	float3 F = fresnelSchlick(F0, max(0.0f, dot(Normal, Lh)));
	float D = ndfGGX(cosLh, roughness);
	float G = gaSchlickGGX(cosLi, cosLo, roughness);

	float3 kd = lerp(float3(1.0f, 1.0f, 1.0f) - F, albedo, metalness);
	float3 diffuseBRDF = (kd * albedo) / PI;

	// Cook-Torrance
	float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0f * cosLi * cosLo);

	result += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;

	return result;
}

// Ambient Lightning
float3 IBL(float3 F0, float3 Lr, float3 Normal, float3 View, float NdotV, float3 albedo, float roughness, float metalness, float cosLo)
{
	float3 irradiance = IrradianceTexture.Sample(defaultSampler, Normal).rgb;
	float3 F = fresnelSchlick(F0, cosLo);
	float3 kd = lerp(float3(1.0f - F), float3(0.0f, 0.0f, 0.0f), (1.0f - metalness));
	float3 diffuseIBL = kd * albedo * irradiance;

	uint specularTextureLevels = queryRadianceTextureLevels();
	//float NoV = clamp(NdotV, 0.0f, 1.0f);
	//float3 R = 2.0f * dot(View, Normal) * Normal - View;
	float3 specularIrradiance = RadianceTexture.SampleLevel(defaultSampler, Lr, roughness * specularTextureLevels).rgb;

	float2 specularBRDF = SpecularBRDFLUT.Sample(spBRDFSampler, float2(cosLo, roughness)).rg;
	float3 specularIBL = specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);

	return diffuseIBL + specularIBL;
}

PixelOutputType main(PixelInputType input) : SV_TARGET
{
	PixelOutputType output;
	PBRParameters params;

	// Sample input textures to get shading model params.
	params.Albedo = AlbedoTexToggle == 1 ? AlbedoTexture.Sample(defaultSampler, input.texcoord).rgb : Albedo.rgb;
	params.Metalness = MetalRoughTexToggle == 1 ? MetalRoughTexture.Sample(defaultSampler, input.texcoord).b : Metalness;
	params.Roughness = MetalRoughTexToggle == 1 ? MetalRoughTexture.Sample(defaultSampler, input.texcoord).r : Roughness;
	params.Roughness = max(params.Roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight

	// Outgoing light direction (vector from world-space fragment position to the "eye").
	float3 Lo = normalize(input.cameraPos - input.worldPosition);

	// Get current fragment's normal and transform to world space.
	float3 N = normalize(input.normal);
	if (NormalTexToggle == 1)
	{
		float3 N = normalize(2.0f * NormalTexture.Sample(defaultSampler, input.texcoord).rgb - 1.0f);
		N = normalize(mul(input.tangentBasis, N));
	}

	// Angle between surface normal and outgoing light direction.
	float cosLo = max(dot(N, Lo), 0.0f);

	// Specular reflection vector.
	float3 Lr = 2.0f * cosLo * N - Lo;

	// Fresnel reflectance at normal incidence (for metals use albedo color).
	float3 F0 = lerp(Fdielectric, params.Albedo, params.Metalness);

	float3 lightContribution = DirectionalLightning(F0, N, Lo, cosLo, params.Albedo, params.Roughness, params.Metalness) + params.Albedo * Emission;
	float3 iblContribution = IBL(F0, Lr, N, Lo, cosLo, params.Albedo, params.Roughness, params.Metalness, cosLo);

	float sunlightAngle = max(dot(normalize(input.worldPosition - planetCenter), normalize(direction)), 0.0f);
	float objectIntensity = lerp(0.2f, 50.0f, sunlightAngle);

	//The hardcoded float3 i ambient lightning which is just there 
	output.Color = float4((lightContribution + iblContribution + float3(0.001f, 0.001f, 0.001f)) * objectIntensity, 1.0f);

	return output;
}