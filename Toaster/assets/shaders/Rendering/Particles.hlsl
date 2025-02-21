#inputlayout
vertex
vertex
vertex
vertex
vertex
instance

#type vertex
#pragma pack_matrix( row_major )

struct ParticleInstance
{
    float3 position;
    float3 velocity;
    float3 startColor;
    float3 endColor;
    float colorBlendFactor;
    float age;
    float lifetime;
    float size;
    float growRate;
    float burstInitial;
    float burstDecay;
};

// Quad corner offsets
static const float2 offsets[4] =
{
    float2(-0.5, 0.5), // Top-left
    float2(0.5, 0.5), // Top-right
    float2(-0.5, -0.5), // Bottom-left
    float2(0.5, -0.5) // Bottom-right
};

// Structured buffer for particle data
StructuredBuffer<ParticleInstance> particleBuffer : register(t0);

cbuffer Camera : register(b0)
{
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

struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float lifeRatio : TEXCOORD1; // Pass particle age as a fraction of lifetime
};

PixelInputType main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    PixelInputType output;
    ParticleInstance p = particleBuffer[instanceID];
    
    float burstFactor = lerp(p.burstInitial, 1.0f, saturate(p.age / p.burstDecay));
    float3 worldPos = p.position + p.velocity * p.age * burstFactor;
    
    float lifeRatio = p.age / p.lifetime;
    float scaledSize = p.size * (1.0f + p.growRate * lifeRatio);
    float alpha = lerp(1.0f, 0.0f, lifeRatio);
    
    float adjustedBlend = lerp(lifeRatio, 1.0, p.colorBlendFactor);
    float3 lerpedColor = lerp(p.startColor, p.endColor, adjustedBlend);

    float4 viewPos = mul(float4(worldPos, 1.0f), viewMatrix);
    float3 right = float3(1.0f, 0.0f, 0.0f);
    float3 up = float3(0.0f, 1.0f, 0.0f);
    
    float2 cornerOffset = offsets[vertexID] * scaledSize;
    float3 viewOffset = (cornerOffset.x * right) + (cornerOffset.y * up);
    viewPos.xyz += viewOffset;
    
    output.position = mul(viewPos, projectionMatrix);
    
    // Base color and alpha computed over lifetime
    output.color = float4(lerpedColor, alpha);
    
    // Map quad offsets (-0.5 to 0.5) to UV space (0 to 1)
    output.uv = offsets[vertexID] + float2(0.5, 0.5);
    
    output.lifeRatio = lifeRatio;
    return output;
}

#type pixel
struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float lifeRatio : TEXCOORD1; // Pass particle age as a fraction of lifetime
};

Texture2D MaskTexture : register(t0);

SamplerState defaultSampler : register(s0);

float4 main(PixelInputType input) : SV_TARGET
{
    //return input.color;
    
    // Sample the mask texture using the provided UV coordinates.
    float4 texColor = MaskTexture.Sample(defaultSampler, input.uv);

    // Multiply the particle's color by the texture sample.
    // This will tint the particle with the texture's RGB and modulate the alpha.
    float4 finalColor = input.color * texColor;
    
    return finalColor;
}

