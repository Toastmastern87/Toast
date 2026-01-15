#inputlayout
vertex
instance
instance
instance
instance

#type vertex
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
	matrix worldTranslationMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
	matrix inverseViewMatrix;
	matrix inverseProjectionMatrix;
	float4 cameraPosition;
	float far;
	float near;
	float viewportWidth;
	float viewportHeight;
};

struct VertexInputType
{
	float2 localPosition	: TEXCOORD0;
	int level				: TEXTUREID;
	float3 a				: POSITION0;
	float3 r				: POSITION1;
	float3 s				: POSITION2;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 viewPosition		: VIEWPOS;
	float3 normalSphereWS	: NORMAL0;
};

PixelInputType main(VertexInputType input)
{
}

#type pixel
#pragma pack_matrix( row_major )

struct PixelInputType
{
	float4 pixelPosition		: SV_POSITION;
	float3 viewPosition			: VIEWPOS;
	float3 normalSphereWS		: NORMAL0;
};

struct PixelOutputType
{
	float4 position				: SV_Target0;
	float4 normal				: SV_Target1;
	float4 albedoMetallic		: SV_Target2;
	float4 roughnessAO			: SV_Target3;
	int entityID				: SV_Target4;
};

cbuffer Camera : register(b0)
{
	matrix worldTranslationMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
	matrix inverseViewMatrix;
	matrix inverseProjectionMatrix;
	float4 cameraPosition;
	float far;
	float near;
	float viewportWidth;
	float viewportHeight;
};

cbuffer Material : register(b2)
{
	float4 Albedo;
	float Emission;
	float Metalness;
	float Roughness;
	int AlbedoTexToggle;
	int NormalTexToggle;
	int MetalRoughTexToggle;
};

struct PBRParameters
{
	float3 Albedo;
	float Metalness;
	float Roughness;
	float AO;
};

PixelOutputType main(PixelInputType input)
{
	PixelOutputType output;
	PBRParameters params;
    
    /*--------------------------------------------------------------*/
    /* 1) position + normal                                         */
    /*--------------------------------------------------------------*/   
	float3 nVS = normalize(mul(input.normalSphereWS, (float3x3) viewMatrix));
    
	output.position = float4(input.viewPosition, 1.0f);
	output.normal = float4(nVS * 0.5f + 0.5f, 1.0f);

    /*--------------------------------------------------------------*/
    /* 2) albedo + metallic                                         */
    /*--------------------------------------------------------------*/
    
	params.Albedo = Albedo.rgb; /* later:   if(AlbedoTexToggle) … */
    
	output.albedoMetallic.rgb = params.Albedo;
	output.albedoMetallic.a = 1.0f; //    Metalness;

    /*--------------------------------------------------------------*/
    /* 3) roughness + ambient occlusion                             */
    /*--------------------------------------------------------------*/
	output.roughnessAO = float4(Roughness, 0.0f, 0.0, 1.0);

	output.entityID = 0;
   
	return output;
}