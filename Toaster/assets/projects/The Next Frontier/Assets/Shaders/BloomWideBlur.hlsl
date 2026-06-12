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
Texture2D baseTex : register(t0);

SamplerState clampSampler : register(s3);

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer WideBlurParams : register(b12)
{
    float2 quarterTexelSize;
    float sigmaPixels;
};

// Build 13-tap Gaussian weights (same as you had)
void BuildWeights(out float w[7], float sigma)
{
    float twoSigma2 = 2.0 * sigma * sigma + 1e-6;
    [unroll]
    for (int k = 0; k <= 6; ++k)
        w[k] = exp(-(k * k) / twoSigma2);
    float norm = w[0] + 2.0 * (w[1] + w[2] + w[3] + w[4] + w[5] + w[6]);
    [unroll]
    for (int p = 0; p <= 6; ++p)
        w[p] /= norm;
}

float4 main(PSIn i) : SV_Target
{
    float w[7];
    BuildWeights(w, sigmaPixels);

    // 4 directions: X, Y, +45°, -45°
    float2 dirX = float2(1, 0) * quarterTexelSize;
    float2 dirY = float2(0, 1) * quarterTexelSize;
    // normalize diagonal so one “step” moves one pixel diagonally
    float2 dirD1 = normalize(float2(1, 1)) * quarterTexelSize;
    float2 dirD2 = normalize(float2(1, -1)) * quarterTexelSize;

    float3 accX = baseTex.SampleLevel(clampSampler, i.uv, 0).rgb * w[0];
    float3 accY = accX; // start from center for each direction
    float3 accD1 = accX;
    float3 accD2 = accX;

    [unroll]
    for (int k = 1; k <= 6; ++k)
    {
        float2 oX = dirX * k;
        float2 oY = dirY * k;
        float2 oD1 = dirD1 * k;
        float2 oD2 = dirD2 * k;

        accX += baseTex.SampleLevel(clampSampler, i.uv + oX, 0).rgb * w[k];
        accX += baseTex.SampleLevel(clampSampler, i.uv - oX, 0).rgb * w[k];
        accY += baseTex.SampleLevel(clampSampler, i.uv + oY, 0).rgb * w[k];
        accY += baseTex.SampleLevel(clampSampler, i.uv - oY, 0).rgb * w[k];
        accD1 += baseTex.SampleLevel(clampSampler, i.uv + oD1, 0).rgb * w[k];
        accD1 += baseTex.SampleLevel(clampSampler, i.uv - oD1, 0).rgb * w[k];
        accD2 += baseTex.SampleLevel(clampSampler, i.uv + oD2, 0).rgb * w[k];
        accD2 += baseTex.SampleLevel(clampSampler, i.uv - oD2, 0).rgb * w[k];
    }

    // Average axes and diagonals. Slightly bias diagonals to counter any residual squaring.
    float3 acc = (accX + accY) * 0.25f + (accD1 + accD2) * 0.25f;

    return float4(acc, 1.0f);
}
