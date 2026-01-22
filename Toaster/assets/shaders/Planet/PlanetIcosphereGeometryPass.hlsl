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
	float3 cameraPosPS;      // camera position in planet space
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
    PixelInputType o;

    // 1) Planar point inside patch triangle (planet local)
    float u = input.localPosition.x;
    float v = input.localPosition.y;
    float3 p = input.a + u * input.r + v * input.s;

    // 2) Project to unit sphere direction
    float lenP = length(p);
    float3 dir = (lenP > 1e-8f) ? (p / lenP) : float3(0, 1, 0);

    // 3) Camera-relative position in planet space (small near camera)
    float3 posRelPS = dir * planetRadius - cameraPosPS;

    // 4) Rotate into world-relative (ignore translation)
    float3 worldRelVec = mul(float4(posRelPS, 0.0f), worldMatrix).xyz;

    // 5) Apply floating origin translation ONLY if viewMatrix expects it.
    // In many floating-origin setups, viewMatrix is rotation-only and worldTranslationMatrix does translation.
    float4 worldRel = mul(float4(worldRelVec, 1.0f), worldTranslationMatrix);

    // 6) View / Projection
    float4 viewPos = mul(worldRel, viewMatrix);
    o.viewPosition = viewPos.xyz;
    o.pixelPosition = mul(viewPos, projectionMatrix);

    // 7) Normal: rotate dir by worldMatrix (ignore translation)
    float3 nWS = mul(float4(dir, 0.0f), worldMatrix).xyz;
    o.normalSphereWS = normalize(nWS);

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