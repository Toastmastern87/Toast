#inputlayout
vertex
vertex
vertex
vertex
vertex

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
#pragma pack_matrix( row_major )

cbuffer Camera : register(b0)
{
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float far;
    float near;
};

cbuffer DirectionalLight : register(b3)
{
	float4 direction;
	float4 radiance;
	float multiplier;
};

cbuffer Environment : register(b6)
{
	float environmentStrength;
	float textureLOD;
	float2 padding;
}

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

TextureCube radianceTexture			: register(t5);

// Sampler state
SamplerState defaultSampler			: register(s0);


float4 main(PixelInputType input) : SV_Target
{
    // Reconstruct NDC coordinates from texture coordinates
    float2 ndc = input.texCoord * 2.0f - 1.0f;
    ndc.y = -ndc.y; // Flip Y since texture coordinates are top-left origin

    // Assume a forward direction in clip space
    float4 clipPos = float4(ndc, 0.0f, 1.0f);
    
    // Transform from clip space to view space
    float4 viewPos = mul(clipPos, inverseProjectionMatrix);
    viewPos /= viewPos.w; // Perspective divide

    // Calculate the view direction
    float3 viewDir = normalize(viewPos.xyz);

    // Transform from view space to world space
    float3 worldDir = mul(viewDir, (float3x3) inverseViewMatrix);

    // Sample the cubemap texture
    float3 skyColor = radianceTexture.SampleLevel(defaultSampler, worldDir, textureLOD).rgb * environmentStrength;
    
    return float4(skyColor, 1.0f);
}