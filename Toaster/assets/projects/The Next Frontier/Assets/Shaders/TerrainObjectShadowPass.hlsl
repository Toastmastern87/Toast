#inputlayout
vertex
vertex
vertex
vertex
vertex
instance

#type vertex
#pragma pack_matrix( row_major )


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
    float3 position : POSITION0;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texCoord : TEXCOORD;
    float3 color : COLOR0;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
};

#include "TerrainObjectCommon.hlsli"

StructuredBuffer<TerrainInstance> TerrainInstances : register(t0);

PixelInputType main( VertexInputType input, uint instanceID : SV_InstanceID)
{
    PixelInputType output;
    
    const TerrainInstance inst = TerrainInstances[instanceID];
    const float3 pCR = inst.PosCR + QuatRotate(inst.RotQuat, input.position * inst.Scale);

    output.position = mul(float4(pCR, 1.0f), lightViewProj[CascadeIndex]);
    return output;
}