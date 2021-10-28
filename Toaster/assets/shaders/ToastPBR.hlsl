#inputlayout
vertex
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
	float4 cameraPosition;
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
	float3 tangent			: TANGENT;
	float3 bitangent		: BITANGENT;
	float2 texcoord			: TEXCOORD;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION0;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD;
	float3 cameraPos		: POSITION1;
	float3 tangent			: TANGENT0;
	float3 bitangent		: BITANGENT0;
	int entityID			: TEXTUREID;
};

PixelInputType main(VertexInputType input)
{
	PixelInputType output;

	output.pixelPosition = mul(float4(input.position, 1.0f), worldMatrix);
	output.pixelPosition = mul(output.pixelPosition, viewMatrix);
	output.pixelPosition = mul(output.pixelPosition, projectionMatrix);

	output.worldPosition = mul(float4(input.position, 1.0f), worldMatrix).xyz;

	output.normal = mul(input.normal, (float3x3)worldMatrix);
	output.tangent = mul(input.tangent, (float3x3)worldMatrix);
	output.bitangent = mul(input.bitangent, (float3x3)worldMatrix);

	output.texcoord = float2(input.texcoord.x, input.texcoord.y);
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

cbuffer PBRData : register(b1)
{
	float4 albedoColor;
	float metalnessMulitplier;
	float roughnessMulitplier;
	float emissionMultiplier;
	bool useNormalMap;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 worldPosition	: POSITION;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD;
	float3 cameraPos		: POSITION1;
	float3 tangent			: TANGENT0;
	float3 bitangent		: BITANGENT0;
	int entityID			: TEXTUREID;
};

struct PixelOutputType
{
	float4 Color			: SV_Target0;
	int EntityID			: SV_Target1;
};

TextureCube IrradianceTexture	: register(t0);
TextureCube RadianceTexture		: register(t1);
Texture2D SpecularBRDFLUT		: register(t2);
Texture2D AlbedoTexture			: register(t3);
Texture2D NormalTexture			: register(t4);
Texture2D MetalnessTexture		: register(t5);
Texture2D RoughnessTexture		: register(t6);

SamplerState defaultSampler		: register(s0);
SamplerState spBRDFSampler		: register(s1);

float3 CalculateNormalFromMap(float3 normal, float3 tangent, float3 bitangent, float2 texCoords)
{
	float3 Normal = normalize(normal);
	float3 Tangent = normalize(tangent);
	float3 Bitangent = normalize(bitangent);
	float3 BumpMapNormal = NormalTexture.Sample(defaultSampler, texCoords).xyz;
	BumpMapNormal = 2.0f * BumpMapNormal - float3(1.0f, 1.0f, 1.0f);

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

	// Sample input textures to get shading model params.
	float3 albedo = AlbedoTexture.Sample(defaultSampler, input.texcoord).rgb * albedoColor.rgb;
	float metalness = MetalnessTexture.Sample(defaultSampler, input.texcoord).r * metalnessMulitplier;
	float roughness = RoughnessTexture.Sample(defaultSampler, input.texcoord).r * roughnessMulitplier;
	roughness = max(roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight

	// Outgoing light direction (vector from world-space fragment position to the "eye").
	float3 Lo = normalize(input.cameraPos - input.worldPosition);

	// Get current fragment's normal and transform to world space.
	float3 N = normalize(input.normal);
	if (useNormalMap)
		N = CalculateNormalFromMap(input.normal, input.tangent, input.bitangent, input.texcoord);

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
		float2 specularBRDF = SpecularBRDFLUT.Sample(spBRDFSampler, float2(cosLo, roughness)).rg;

		// Total specular IBL contribution.
		float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;

		// Total ambient lighting contribution.
		ambientLighting = diffuseIBL + specularIBL;
	}

	output.Color = float4(directLighting + ambientLighting, 1.0f);//float4(N, 1.0f);// 
	output.EntityID = input.entityID + 1;

	return output;
}