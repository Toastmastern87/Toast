#inputlayout
#type vertex
#pragma pack_matrix(row_major)

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};
VSOut main(uint vID : SV_VertexID)
{
    VSOut o;
    o.uv = float2((vID << 1) & 2, vID & 2);
    o.pos = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

#type pixel
#pragma pack_matrix(row_major)

// MS LUT bound at t1 in your pipeline
Texture2D<float4> MultiScattLUT : register(t1);
Texture2D<float4> SkyviewLUT : register(t2);
SamplerState ClampLinear : register(s0);

// Debug toggles
#ifndef MS_VIS_MODE
// 0 = tone-mapped RGB Ψ_ms
// 1 = luminance (tone-mapped)
// 2 = raw (no tone-map) — expect very dark unless Ψ exploded
#define MS_VIS_MODE 2
#endif

float3 Tonemap(float3 c)
{
    return c / (1.0f + c);
}

float4 main(float4 svpos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target
{
    // NOTE: We sample the LUT in its native layout: x=theta_s/π, y=altitude.
    // Your MS_CS writes with MS_FLIP_Y=1, so top=TOA, bottom=ground. This blit shows that as-is.
    float4 ms = SkyviewLUT.Sample(ClampLinear, uv);

#if MS_VIS_MODE == 0
    float3 col = Tonemap(max(ms.rgb, 0.0));
    return float4(col, 1);
#elif MS_VIS_MODE == 1
    float Y = dot(max(ms.rgb, 0.0), float3(0.2126, 0.7152, 0.0722));
    float Ytm = Y / (1.0 + Y);
    return float4(Ytm.xxx, 1);
#else
    return float4(ms.rgb, 1);
#endif
}