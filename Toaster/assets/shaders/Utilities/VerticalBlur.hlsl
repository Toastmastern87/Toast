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

Texture2D baseTexture : register(t0);

SamplerState clampSampler : register(s3);

cbuffer BlurParams : register(b12)
{
    float2 texelSize; // 1.0/textureWidth and 1.0/textureHeight
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
    float4 sum = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Sample a series of offsets horizontally
    sum += baseTexture.Sample(clampSampler, input.texCoord + float2(0, -4.0f * texelSize.y)) * 0.05f;
    sum += baseTexture.Sample(clampSampler, input.texCoord + float2(0, -3.0f * texelSize.y)) * 0.09f;
    sum += baseTexture.Sample(clampSampler, input.texCoord + float2(0, -2.0f * texelSize.y)) * 0.12f;
    sum += baseTexture.Sample(clampSampler, input.texCoord + float2(0, -1.0f * texelSize.y)) * 0.15f;
    sum += baseTexture.Sample(clampSampler, input.texCoord) * 0.16f;
    sum += baseTexture.Sample(clampSampler, input.texCoord + float2(0, 1.0f * texelSize.y)) * 0.15f;
    sum += baseTexture.Sample(clampSampler, input.texCoord + float2(0, 2.0f * texelSize.y)) * 0.12f;
    sum += baseTexture.Sample(clampSampler, input.texCoord + float2(0, 3.0f * texelSize.y)) * 0.09f;
    sum += baseTexture.Sample(clampSampler, input.texCoord + float2(0, 4.0f * texelSize.y)) * 0.05f;

    return sum;
}