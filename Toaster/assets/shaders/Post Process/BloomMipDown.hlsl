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

Texture2D SrcTex : register(t0);
Texture2D<float> SunDiscMask : register(t1);

SamplerState LinearClamp : register(s3);

cbuffer BloomMipParams : register(b13)
{
    float2 SrcTexelSize;
    float FirstLevel;
    float _Pad0;
}

static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

// The 13 taps, in source texels from the destination texel's centre.
// The destination centre falls on a corner between 4 source texels, so each bilinear tap
// averages a 2x2 block: the taps cover a 6x6 source area.
static const float2 TAP[13] =
{
    float2(-2.0f, -2.0f), float2(0.0f, -2.0f), float2(2.0f, -2.0f),
    float2(-2.0f, 0.0f), float2(0.0f, 0.0f), float2(2.0f, 0.0f),
    float2(-2.0f, 2.0f), float2(0.0f, 2.0f), float2(2.0f, 2.0f),
    float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, 1.0f)
};

static const uint4 BOX[5] =
{
    uint4(0, 1, 3, 4), uint4(1, 2, 4, 5), uint4(3, 4, 6, 7), uint4(4, 5, 7, 8), uint4(9, 10, 11, 12),
};
static const float BOX_WEIGHT[5] = { 0.125f, 0.125f, 0.125f, 0.125f, 0.5f};

float3 BoxAverage(float3 c[13], uint4 i)
{
    return 0.25f * (c[i.x] + c[i.y] + c[i.z] + c[i.w]);
}

float3 FiniteOrZero(float3 c)
{
    return all(isfinite(c)) ? c : 0.0f.xxx;
}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float3 c[13];
    [unroll]
    for (int t = 0; t < 13; ++t)
        c[t] = FiniteOrZero(SrcTex.SampleLevel(LinearClamp, uv + TAP[t] * SrcTexelSize, 0).rgb);

    if (FirstLevel < 0.5f)
    {
        float3 sum = 0.0f.xxx;
        [unroll]
        for (int b = 0; b < 5; ++b)
            sum += BOX_WEIGHT[b] * BoxAverage(c, BOX[b]);
        return float4(sum, 1.0f);
    }

    float3 cDisc[13];
    float3 cRest[13];
    [unroll]
    for (int k = 0; k < 13; ++k)
    {
        float isDisc = (SunDiscMask.SampleLevel(LinearClamp, uv + TAP[k] * SrcTexelSize, 0) > 0.0f) ? 1.0f : 0.0f;
        cRest[k] = c[k] * (1.0f - isDisc);
    }

    float3 restSum = 0.0f.xxx;
    float restWeight = 0.0f;
    [unroll]
    for (int b = 0; b < 5; ++b)
    {
        float3 boxRest = BoxAverage(cRest, BOX[b]);
        float w = BOX_WEIGHT[b] / (1.0f + dot(boxRest, LUMA));
        restSum += w * boxRest;
        restWeight += w;
    }

    return float4(restSum / max(restWeight, 1e-6f), 1.0f);
}