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
cbuffer Hovering : register(b8) // pick a free slot, matching your binding
{
    float4 HoverTint; // rgb + alpha
};

Texture2D MaskTexture : register(t11); // matches the slot you bind the mask to
SamplerState DefaultSampler : register(s1);

struct PSIn
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct PSOut
{
    float4 hdr : SV_Target0;
    float4 sdr : SV_Target1;
};

PSOut main(PSIn input)
{
    float mask = MaskTexture.Sample(DefaultSampler, input.uv).r; // white where hovered mesh is
    float a = mask * HoverTint.a;
    if (a <= 0.001f)
        discard;

    PSOut output;
    output.sdr = float4(HoverTint.rgb, a);
    const float paperWhiteScale = 2000.0f / 200.0f;
    output.hdr = float4(HoverTint.rgb * paperWhiteScale, a);
    return output;
}