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
    float intensityAtmosphere;
    float intensitySpace;
    float spaceFactor;
    float thresholdAtmosphere;
    float thresholdSpace;
};

Texture2D sceneBaseTexture : register(t0);
Texture2D blurredBloomTexture : register(t1);

SamplerState defaultSampler : register(s0);

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
    float3 sceneColor = sceneBaseTexture.Sample(defaultSampler, input.texCoord).rgb;
    float3 bloomColor = blurredBloomTexture.Sample(defaultSampler, input.texCoord).rgb;
    
    float intensity = lerp(intensityAtmosphere, intensitySpace, spaceFactor);
    // Combine the scene with the bloom contribution
    return float4(sceneColor + bloomColor * intensity, 1.0f);
}