#inputlayout
vertex
vertex
vertex
vertex
vertex
instance

#type vertex
#pragma pack_matrix( row_major )

#include "ParticleCommon.hlsli"

// Quad corner offsets
static const float2 offsets[4] =
{
    float2(-0.5, 0.5), // Top-left
    float2(0.5, 0.5), // Top-right
    float2(-0.5, -0.5), // Bottom-left
    float2(0.5, -0.5) // Bottom-right
};

// Structured buffer for particle data
StructuredBuffer<GPUParticle> ParticleBuffer : register(t0);
// t1: the alive list. Packed. Maps instance index -> pool slot.
StructuredBuffer<uint> AliveList : register(t1);

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

struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float lifeRatio : TEXCOORD1; // Pass particle age as a fraction of lifetime
    float3 viewPos : TEXCOORD2; // VIEW space, matches the G-buffer
    float softFade : TEXCOORD3; // per-particle fade distance
};

PixelInputType main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    PixelInputType output;
    
    // Get the particle to draw
    // SV_InstanceID is a position in the PACKED alive list, not a pool
    uint particleIndex = AliveList[instanceID];
    GPUParticle p = ParticleBuffer[particleIndex];
    
    // Position is simulated on the GPU now in the new particle system, so we use it directly.
    float3 worldPos = p.Position;
    
    // Floating origin: Position is origin-relative; this applies the
    worldPos = mul(float4(worldPos, 1.0f), worldTranslationMatrix).xyz;
    
    float lifeRatio = p.Age / p.Lifetime;
    float scaledSize = p.Size * (1.0f + p.GrowRate * lifeRatio);
    float alpha = lerp(1.0f, 0.0f, lifeRatio);
    
    float adjustedBlend = lerp(lifeRatio, 1.0, p.ColorBlendFactor);
    float3 lerpedColor = lerp(p.StartColor, p.EndColor, adjustedBlend);
    
    // View-space billboarding: offsetting after the view transform makes the
    // quad automatically face the camera.
    float4 viewPos = mul(float4(worldPos, 1.0f), viewMatrix);
    float3 right = float3(1.0f, 0.0f, 0.0f);
    float3 up = float3(0.0f, 1.0f, 0.0f);
    //
    float2 cornerOffset = offsets[vertexID] * scaledSize;
    float3 viewOffset = (cornerOffset.x * right) + (cornerOffset.y * up);
    viewPos.xyz += viewOffset;
    
    output.viewPos = viewPos.xyz;
    output.softFade = p.SoftFadeDistance;
    output.position = mul(viewPos, projectionMatrix);
    
    float intensityT = pow(saturate(lifeRatio), max(p.IntensityFalloff, 0.001f));
    float intensity = lerp(p.StartIntensity, p.EndIntensity, intensityT);
    
    // Base color and alpha computed over lifetime
    output.color = float4(lerpedColor * intensity, alpha);
    
    // Map quad offsets (-0.5 to 0.5) to UV space (0 to 1)
    output.uv = offsets[vertexID] + float2(0.5, 0.5);
    
    output.lifeRatio = lifeRatio;
    return output;
}

#type pixel
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

struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float lifeRatio : TEXCOORD1; // Pass particle age as a fraction of lifetime
    float3 viewPos : TEXCOORD2; // VIEW space, matches the G-buffer
    float softFade : TEXCOORD3; // per-particle fade distance
};

Texture2D MaskTexture   : register(t0);
Texture2D GPassPosition : register(t1);

SamplerState defaultSampler : register(s0);

float4 main(PixelInputType input) : SV_TARGET
{   
    // Sample the mask texture using the provided UV coordinates.
    float4 texColor = MaskTexture.Sample(defaultSampler, input.uv);

    // Multiply the particle's color by the texture sample.
    // This will tint the particle with the texture's RGB and modulate the alpha.
    float4 finalColor = input.color * texColor;
    //
    // Soft particles: fade out as the billboard approaches whatever is behind it,
    // so it dissolves into the surface instead of showing a hard intersection.
    if (input.softFade > 0.0f)
    {
        // Screen-space UV from SV_POSITION (already in pixels).
        float2 screenUV = input.position.xy / float2(viewportWidth, viewportHeight);

        float3 scenePosVS = GPassPosition.Sample(defaultSampler, screenUV).xyz;

        float sceneDepth = (abs(scenePosVS.z) < 1e-6f) ? 1e9f : scenePosVS.z;
        float particleDepth = input.viewPos.z;

        float fade = saturate((sceneDepth - particleDepth) / input.softFade);

        finalColor.a *= fade;
    }
    
    return finalColor;
}

