#inputlayout
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
    float2 uv = float2((vID << 1) & 2, vID & 2); // {0,2}
    output.texCoord = uv * 0.5;
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1);

    return output;
}

#type pixel
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

Texture2D Src : register(t0); // whatever you want to copy
SamplerState LinearClamp : register(s0);

float4 main(PixelInputType input) : SV_TARGET
{   
    return Src.Sample(LinearClamp, input.texCoord); // just pass it through
}