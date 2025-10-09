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

SamplerState clampSampler : register(s3);

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
    // HDR color (post-exposure, pre-tonemap)
    float3 colorHDR = sceneBaseTexture.Sample(clampSampler, input.texCoord).rgb;

    // Space/atmo thresholds
    float t = lerp(thresholdAtmosphere, thresholdSpace, spaceFactor);
    float k = 0.5 * t; // knee width ~ half the threshold (good starting point)

    // Unreal-style soft-knee bright pass, PER CHANNEL (keeps sun’s color)
    float3 over = max(colorHDR - t, 0.0.xxx);
    float3 soft = max(colorHDR - (t - k), 0.0.xxx);
    float invDen = 1.0 / max(4.0 * k, 1e-6);
    float3 knee = soft * soft * invDen;
    float3 bright = max(over, knee);

    // Output the BRIGHT COLOR, not a luma mask
    return float4(bright, 1.0);
}