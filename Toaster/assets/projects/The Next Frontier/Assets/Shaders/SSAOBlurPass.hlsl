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

Texture2D SSAOInput : register(t0);

SamplerState linearSampler : register(s4);

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
    // Query the texture dimensions.
    uint width, height;
    SSAOInput.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float result = 0.0f;

    // Loop over a 4x4 kernel (x: -2 to 1, y: -2 to 1)
    [unroll]
    for (int x = -2; x < 2; ++x)
    {
        [unroll]
        for (int y = -2; y < 2; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            result += SSAOInput.Sample(linearSampler, input.texCoord + offset).r;
        }
    }
    result /= 16.0f; // Average over 16 samples

    return float4(result, result, result, 1.0f);
}