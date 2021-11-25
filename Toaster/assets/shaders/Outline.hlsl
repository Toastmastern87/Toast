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
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}

#type pixel
Texture2D InputTexture          : register(t9);
SamplerState DefaultSampler     : register(s1);

struct PixelInputType
{
    float4 position   : SV_POSITION;
    float2 texCoord   : TEXCOORD;
};

float SampleTexture(float2 uv, float2 pixeloffset)
{
    float samples[9];

    uint width, height;
    InputTexture.GetDimensions(width, height);
    float2 pixelSize = 1.0f / float2(width, height);

    float p = 3.0f; // Outline width
    samples[0] = InputTexture.Sample(DefaultSampler, uv + (float2(-p, -p) + pixeloffset) * pixelSize).r;
    samples[1] = InputTexture.Sample(DefaultSampler, uv + (float2(0.0f, -p) + pixeloffset) * pixelSize).r;
    samples[2] = InputTexture.Sample(DefaultSampler, uv + (float2(p, -p) + pixeloffset) * pixelSize).r;
    samples[3] = InputTexture.Sample(DefaultSampler, uv + (float2(-p, 0.0f) + pixeloffset) * pixelSize).r;
    samples[4] = InputTexture.Sample(DefaultSampler, uv + (float2(0.0f, 0.0f) + pixeloffset) * pixelSize).r;
    samples[5] = InputTexture.Sample(DefaultSampler, uv + (float2(p, 0.0f) + pixeloffset) * pixelSize).r;
    samples[6] = InputTexture.Sample(DefaultSampler, uv + (float2(-p, p) + pixeloffset) * pixelSize).r;
    samples[7] = InputTexture.Sample(DefaultSampler, uv + (float2(0.0f, p) + pixeloffset) * pixelSize).r;
    samples[8] = InputTexture.Sample(DefaultSampler, uv + (float2(p, p) + pixeloffset) * pixelSize).r;

    float maxVal = 0.0f;
    for (int i = 0; i < 9; i++)
        maxVal = max(maxVal, samples[i]);

    return maxVal;
}

float4 main(PixelInputType input) : SV_TARGET
{
    static float val = 0.0f;
    val += SampleTexture(input.texCoord, 0.0f.xx);

    float multiplier = 1.0f - InputTexture.Sample(DefaultSampler, input.texCoord).r;
    val *= multiplier;

    if (val < 0.2f)
        discard;

    float3 outlineColor = float3(122.0f/255.0f, 173.0f/255.0f, 71.0f/255.0f);
    return float4(outlineColor, val);
}