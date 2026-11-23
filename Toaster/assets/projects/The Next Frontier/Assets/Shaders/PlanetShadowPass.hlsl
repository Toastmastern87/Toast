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
    int materialCount;

    float3 planetCenterRelLoWS;
};

cbuffer DirectionalLight : register(b3)
{
    float4x4 lightViewProj[4];
    
    float4 direction; // FROM light -> scene
    
    float4 radiance; // RGB
    
    float SunIntensity;
    float DirectionalLightGain;
    uint CascadeCount;
    float ShadowDistance;
    
    float4 CascadeEnds;
    
    uint CascadeIndex;
    float ConstantBias;
    float SlopeBias;
};

struct VertexInputType
{
    uint2 ij : TEXCOORD0; // per-vertex
    int level : TEXTUREID0; // per-instance

    float3 V0 : POSITION0;
    float3 V1 : POSITION1;
    float3 V2 : POSITION2;

    float3 P2RelHi : TEXCOORD3;
    float3 P0RelHi : TEXCOORD1;
    float3 P1RelHi : TEXCOORD2;
    
    float3 PatchOriginPS : TEXCOORD4;
    
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
};

struct PlanetBakedVertex
{
    float3 NormalPS;
    float Height;
    uint3 PackedDebug;
    uint Pad;
};

StructuredBuffer<PlanetBakedVertex> BakedVertices : register(t5);

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    uint N = 1u << (uint) patchLevels;
    
    uint i = input.ij.x;
    uint j = input.ij.y;
    if (i + j > N)
    {
        i = min(i, N);
        j = N - i;
    }
    uint k = N - i - j;

    float invN = exp2(-(float)patchLevels);
    float w0 = (float) k * invN;
    float w1 = (float) i * invN;
    float w2 = (float) j * invN;
    
    float3 V0 = normalize(input.V0);
    float3 V1 = normalize(input.V1);
    float3 V2 = normalize(input.V2);
    
    precise float3 dir = normalize(w0 * V0 + w1 * V1 + w2 * V2);

    uint verticesPerPatch = (N + 1u) * (N + 2u) / 2u;
    float h = BakedVertices[input.instanceID * verticesPerPatch + input.vertexID].Height;

    precise float3 relPSHi = w0 * input.P0RelHi + w1 * input.P1RelHi + w2 * input.P2RelHi;
    relPSHi += dir * h;  
    float3 relPS = relPSHi - camLoPS;
    float3 relWS = mul(relPS, (float3x3) worldMatrix);

    float4 viewPos = mul(float4(relWS, 1.0f), viewMatrixPlanetRendering);
    float4 posWS = mul(float4(viewPos.xyz, 1.0f), inverseViewMatrix);

    output.position = mul(float4(posWS.xyz, 1.0f), lightViewProj[CascadeIndex]);
    return output;
}

//float4 main() : SV_TARGET
//{
//	return float4(1.0f, 1.0f, 1.0f, 1.0f);
//}