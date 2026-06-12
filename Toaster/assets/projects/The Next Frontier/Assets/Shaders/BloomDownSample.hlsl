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
#pragma pack_matrix(row_major)

Texture2D srcTex : register(t0);
SamplerState clampSampler : register(s3);

cbuffer DownsampleParams : register(b13)
{
    float2 srcTexelSize; // 1/srcWidth, 1/srcHeight
};

static const float2 POISSON[12] =
{
    float2(0.0, -1.0), float2(0.87, -0.5), float2(0.87, 0.5),
    float2(0.0, 1.0), float2(-0.87, 0.5), float2(-0.87, -0.5),
    float2(0.0, -2.0), float2(1.73, -1.0), float2(1.73, 1.0),
    float2(0.0, 2.0), float2(-1.73, 1.0), float2(-1.73, -1.0)
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_Target
{
    const float wc = 0.08, w1 = 0.06, w2 = 0.035;
    float3 c = srcTex.Sample(clampSampler, uv).rgb * wc;

    [unroll]
    for (int k = 0; k < 6; ++k)
    {
        float2 o1 = POISSON[k] * srcTexelSize;
        float2 o2 = POISSON[k + 6] * srcTexelSize;
        c += srcTex.Sample(clampSampler, uv + o1).rgb * w1;
        c += srcTex.Sample(clampSampler, uv + o2).rgb * w2;
    }
    return float4(c, 1);
}