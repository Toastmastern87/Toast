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
    output.texCoord = float2((vID << 1) & 2, vID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.0f, 1);

    return output;
}

#type pixel
// G-buffer textures
Texture2D positionTexture           : register(t0); // View-space position
Texture2D normalTexture             : register(t1); // Encoded normals
Texture2D albedoMetallicTexture     : register(t2); // Albedo RGB and Metallic A
Texture2D roughnessAOTexture        : register(t3); // Roughness R and AO A

// Sampler state
SamplerState samplerState           : register(s0);

// Input structure from vertex shader
struct PixelInputType
{
    float4 position : SV_POSITION; // Clip-space position
    float2 texCoord : TEXCOORD0; // Texture coordinates
};

// Pixel shader main function
float4 main(PixelInputType input) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}