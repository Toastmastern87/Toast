#inputlayout
vertex
vertex
vertex
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

struct StarInstance
{
    float3 Dir;
    float Lum;
    float3 RGB;
    float _pad;
};

static const float2 offsets[4] =
{
    float2(-0.5, 0.5), // Top-left
    float2(0.5, 0.5), // Top-right
    float2(-0.5, -0.5), // Bottom-left
    float2(0.5, -0.5) // Bottom-right
};

StructuredBuffer<StarInstance> stars : register(t0);

cbuffer Camera : register(b0)
{
    matrix worldTranslationMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float far;
    float near;
    float viewportWidth;
    float viewportHeight;
};

float SizeFromLum(float L)               // tweak to taste
{
    return saturate(log2(L * 0.4 + 1.0)) * 4.0 + 0.5;
} // 0.5–4.5 px

struct PixelInputType
{
    float4 position     : SV_POSITION;
    float2 uv           : TEXCOORD0;
    float3 color        : COLOR;
};

PixelInputType main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    PixelInputType output;
    StarInstance star = stars[instanceID];
    
    float3 viewDir = mul(star.Dir, (float3x3) viewMatrix);
    float4 viewPos = float4(viewDir, 0.0f);
    float4 clipPos = mul(viewPos, projectionMatrix);

    clipPos.z = 0.0;
    
    float pxSize = SizeFromLum(star.Lum);
    float2 halfClip = float2(2.0f / viewportWidth, 2.0f / viewportHeight) * pxSize * 0.5f;

    clipPos.xy += offsets[vertexID] * halfClip * clipPos.w;
    
    output.position = clipPos;
    output.uv = offsets[vertexID] * 0.5f + 0.5f;
    output.color = star.RGB;
    
    return output;
}

#type pixel
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 position         : SV_POSITION;
    float2 uv               : TEXCOORD0;
    float3 color            : COLOR;
};

float4 main(PixelInputType input) : SV_TARGET
{
    float2 d = input.uv - 0.5f;
    float alpha = exp(-dot(d, d) * 32.0f);
    float3 col = input.color * alpha; // pre-multiplied

    return float4(input.color * alpha * 15.0, alpha);
}