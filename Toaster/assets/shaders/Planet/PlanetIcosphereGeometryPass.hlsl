#inputlayout
vertex
instance
instance
instance
instance
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

cbuffer Model : register(b1)
{
    matrix worldMatrix; // planet rotation + translation (translation should already be floating-origin safe)
    float clickable;
    int entityID;
    int noWorldTransform;
    int isInstanced;
};

cbuffer IcospherePlanet : register(b2)
{
    float planetRadius; 
    float3 camHiPS;
	
    matrix viewMatrixPlanetRendering; // This includes floating origin translation for planet rendering
	
    int patchLevels;
    float3 camLoPS;
	
    float3 planetCenterRelHiWS;

    float3 planetCenterRelLoWS;
};

struct VertexInputType
{
    uint2 ij        : TEXCOORD0; // per-vertex
    int level       : TEXTUREID0; // per-instance

    float3 V0       : POSITION0;
    float3 V1       : POSITION1;
    float3 V2       : POSITION2;

    float3 P0RelHi  : TEXCOORD1;
    float3 P1RelHi  : TEXCOORD2;
    float3 P2RelHi  : TEXCOORD3;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 viewPosition		: VIEWPOS;
	float3 normalSphereWS	: NORMAL0;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType o;

    // Triangular grid barycentrics
    uint N = 1u << (uint) patchLevels;

    uint i = input.ij.x;
    uint j = input.ij.y;
    if (i + j > N)
    {
        i = min(i, N);
        j = N - i;
    }
    uint k = N - i - j;

    float invN = exp2(-(float) patchLevels);
    float wi = (float) i * invN;
    float wj = (float) j * invN;
    float wk = (float) k * invN;

    // IMPORTANT: map weights consistently to corners.
    // You must ensure (i,j,k) correspond to (V1,V2,V0) or similar consistently.
    // Pick ONE mapping and keep it everywhere.
    float w0 = wk; // for V0
    float w1 = wi; // for V1
    float w2 = wj; // for V2

    float3 V0 = normalize(input.V0);
    float3 V1 = normalize(input.V1);
    float3 V2 = normalize(input.V2);

    // Edge-consistent direction
    precise float3 dir = normalize(w0 * V0 + w1 * V1 + w2 * V2);

    // Normal in planet space (same as dir for sphere)
    float3 nPS = dir;

    // High-precision relative position in planet space meters:
    precise float3 relPSHi = w0 * input.P0RelHi + w1 * input.P1RelHi + w2 * input.P2RelHi;

    // Final view-relative = (hi-relative) - camLo
    float3 relPS = relPSHi - camLoPS;

    // World rotation only
    float3x3 R = (float3x3) worldMatrix;
    float3 relWS = mul(relPS, R);
    float3 nWS = mul(nPS, R);
    o.normalSphereWS = normalize(nWS);

    float4 viewPos = mul(float4(relWS, 1.0f), viewMatrixPlanetRendering);
    o.viewPosition = viewPos.xyz;
    o.pixelPosition = mul(viewPos, projectionMatrix);
    return o;
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