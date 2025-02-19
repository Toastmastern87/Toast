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
    float age;
    float lifetime;
    float size;
    float growRate;
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
};

PixelInputType main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    PixelInputType output;

    ParticleInstance p = particleBuffer[instanceID];

    // Apply gravity (particles fall over time)
    float3 worldPos = p.position + p.velocity * p.age;
    worldPos.y -= 3.73f * p.age;
    
    // Compute size scaling over lifetime
    float lifeRatio = p.age / p.lifetime;
    float scaledSize = p.size * (1.0f + p.growRate * lifeRatio);
    float alpha = lerp(1.0, 0.0, lifeRatio);

    float4 viewPos = mul(float4(worldPos, 1.0f), viewMatrix);
    
    float3 right = float3(1.0f, 0.0f, 0.0f);
    float3 up = float3(0.0f, 1.0f, 0.0f);

    float2 cornerOffset = offsets[vertexID] * scaledSize;
    float3 viewOffset = (cornerOffset.x * right) + (cornerOffset.y * up);
    viewPos.xyz += viewOffset;
    
    viewPos.z += instanceID * 0.0001;

    output.position = mul(viewPos, projectionMatrix);

    // Always output a red color (ignoring texture)
    output.color = float4(1.0f, 0.0f, 0.0f, alpha); // Red color

    return output;
}

#type pixel
struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PixelInputType input) : SV_TARGET
{
    return input.color; // Solid red output
}

