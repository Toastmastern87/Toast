#inputlayout
#type vertex
#pragma pack_matrix(row_major)

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PixelInputType main(uint vID : SV_VERTEXID)
{
    PixelInputType output;

    //https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    output.texCoord = float2((vID << 1) & 2, vID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 1, 1);
    return output;
}

#type pixel
Texture2D InputTexture          : register(t11);

SamplerState DefaultSampler     : register(s1);

struct PixelInputType
{
    float4 position   : SV_POSITION;
    float2 texCoord   : TEXCOORD;
};

cbuffer Outline : register(b8)
{
    float4 OutlineColor;
    
    float OutlineThickness;
    float OutlineSoftness; // added
    float2 pad0; // pad to 16-byte byte boundery
};

struct PSOut
{
    float4 hdr : SV_Target0; // -> FinalRT  (HDR / scRGB)
    float4 sdr : SV_Target1; // -> FinalEditorRT (SDR)
};

float SampleTexture(float2 uv, float thickness)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    float2 pixelSize = 1.0f / float2(width, height);

    float maxVal = 0.0f;
    int p = (int) ceil(thickness);

    // Sample every texel within the thickness radius (filled disc, not a ring)
    for (int y = -p; y <= p; y++)
    {
        for (int x = -p; x <= p; x++)
        {
            // circular mask so the outline is round, not square
            if (x * x + y * y > p * p)
                continue;
            float s = InputTexture.Sample(DefaultSampler, uv + float2(x, y) * pixelSize).r;
            maxVal = max(maxVal, s);
        }
    }
    return maxVal;
}

PSOut main(PixelInputType input) 
{
    float val = 0.0f;
    val += SampleTexture(input.texCoord, OutlineThickness);
    float multiplier = 1.0f - InputTexture.Sample(DefaultSampler, input.texCoord).r;
    val *= multiplier;

    float edge = 0.2f; // detection threshold (fixed)
    float soft = OutlineSoftness * 0.1f; // 0..0.8 for slider 0..8
    // Ramp from the threshold upward; softness widens the ramp but the floor stays at 'edge'
    float alpha = smoothstep(edge, edge + max(soft, 1e-4f), val) * OutlineColor.a;

    if (alpha <= 0.0f)
        discard;

    PSOut output;
    output.sdr = float4(OutlineColor.rgb, alpha);
    output.hdr = float4(OutlineColor.rgb, alpha);
    return output;
}