#ifndef BLOOM_FILTERS_HLSLI
#define BLOOM_FILTERS_HLSLI

float3 BloomTent(Texture2D src, SamplerState s, float2 uv, float2 texel)
{
    float3 c = 0.0f.xxx;
    c += 1.0f * src.SampleLevel(s, uv + float2(-1.0f, -1.0f) * texel, 0).rgb;
    c += 2.0f * src.SampleLevel(s, uv + float2(0.0f, -1.0f) * texel, 0).rgb;
    c += 1.0f * src.SampleLevel(s, uv + float2(1.0f, -1.0f) * texel, 0).rgb;
    c += 2.0f * src.SampleLevel(s, uv + float2(-1.0f, 0.0f) * texel, 0).rgb;
    c += 4.0f * src.SampleLevel(s, uv, 0).rgb;
    c += 2.0f * src.SampleLevel(s, uv + float2(1.0f, 0.0f) * texel, 0).rgb;
    c += 1.0f * src.SampleLevel(s, uv + float2(-1.0f, 1.0f) * texel, 0).rgb;
    c += 2.0f * src.SampleLevel(s, uv + float2(0.0f, 1.0f) * texel, 0).rgb;
    c += 1.0f * src.SampleLevel(s, uv + float2(1.0f, 1.0f) * texel, 0).rgb;
    return c * (1.0f / 16.0f);
}

static const float BLOOM_GAUSS_OFFSET[3] = { 0.0f, 1.3392f, 3.1743f };
static const float BLOOM_GAUSS_WEIGHT[3] = { 1.0f, 1.2118f, 0.1639f };

float3 BloomGaussian(Texture2D src, SamplerState s, float2 uv, float2 texel)
{
    float3 c = 0.0f.xxx;
    float weightSum = 0.0f;
    
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            float2 o = float2(sign(x) * BLOOM_GAUSS_OFFSET[abs(x)], sign(y) * BLOOM_GAUSS_OFFSET[abs(y)]);
            float w = BLOOM_GAUSS_WEIGHT[abs(x)] * BLOOM_GAUSS_WEIGHT[abs(y)];
            c += w * src.SampleLevel(s, uv + o * texel, 0).rgb;
            weightSum += w;
        }
    }

    return c / weightSum;
}

#endif // BLOOM_FILTERS_HLSLI