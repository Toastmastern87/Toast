﻿#inputlayout
#type vertex
#pragma pack_matrix( row_major )

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
#pragma pack_matrix( row_major )

cbuffer BloomParams : register(b11)
{
    float intensity;
    float threshold; // Brightness threshold
};

Texture2D sceneBaseTexture : register(t0);

SamplerState clampSampler : register(s3);

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
    float4 color = sceneBaseTexture.Sample(clampSampler, input.texCoord);
    
    float knee = threshold * 0.5f;
    float bright = max(dot(color.rgb, float3(0.2126, 0.7152, 0.0722)) - knee, 0.0) / (threshold - knee);
    float mask = saturate(bright * bright);
    return float4(color.rgb * mask, 1.0f);
}