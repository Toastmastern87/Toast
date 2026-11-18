#inputlayout
#type vertex
#pragma pack_matrix(row_major)

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
    PixelInputType output;

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    output.texCoord = float2((vID << 1) & 2, vID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1);

    return output;
}

#type pixel
#pragma pack_matrix(row_major)

#include "BloomFilters.hlsli"

Texture2D SceneHDR : register(t0);
Texture2D BloomTex : register(t1);

SamplerState LinearClamp : register(s3);

cbuffer BloomBlendParams : register(b13)
{
    float2 BloomTexelSize;
    float Intensity;
    float _Pad0;
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float3 scene = SceneHDR.SampleLevel(LinearClamp, uv, 0).rgb;
    float3 bloom = BloomTent(BloomTex, LinearClamp, uv, BloomTexelSize);
    
    return float4(lerp(scene, bloom, Intensity), 1.0f);
}