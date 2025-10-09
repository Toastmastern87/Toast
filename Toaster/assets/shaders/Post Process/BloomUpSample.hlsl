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

Texture2D quarterBlur       : register(t0); // from Pass D (¼ res)

SamplerState clampSampler   : register(s3);

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer UpSampleParams : register(b13)
{
    float2 QuarterTexelSize; // (fullW/quarterW, fullH/quarterH)
    float weightQuarter; // e.g. 0.7..1.0
};

// 12 Poisson taps on two rings (r1 ~ 1, r2 ~ 2)
static const float2 POISSON[12] =
{
    float2(0.0, -1.0), float2(0.87, -0.5), float2(0.87, 0.5),
    float2(0.0, 1.0), float2(-0.87, 0.5), float2(-0.87, -0.5),
    float2(0.0, -2.0), float2(1.73, -1.0), float2(1.73, 1.0),
    float2(0.0, 2.0), float2(-1.73, 1.0), float2(-1.73, -1.0)
};

float4 main(PSIn i) : SV_Target
{
    // weights: gentle Gaussian-ish profile across rings
    const float w1 = 0.060; // inner ring (6 taps)
    const float w2 = 0.035; // outer ring (6 taps)
    const float wc = 0.085; // center

    float3 c = quarterBlur.SampleLevel(clampSampler, i.uv, 0).rgb * wc;

    [unroll]
    for (int k = 0; k < 6; ++k)
    {
        float2 o1 = POISSON[k] * QuarterTexelSize;
        float2 o2 = POISSON[k + 6] * QuarterTexelSize;
        c += quarterBlur.SampleLevel(clampSampler, i.uv + o1, 0).rgb * w1;
        c += quarterBlur.SampleLevel(clampSampler, i.uv + o2, 0).rgb * w2;
    }

    c *= weightQuarter;
    return float4(c, 1.0);
}