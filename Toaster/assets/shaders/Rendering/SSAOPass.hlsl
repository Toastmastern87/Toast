﻿#inputlayout
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

cbuffer SSAO : register(b10)
{
    float4 samples[64]; // Same order as CPU
    float radius;
    float bias;
};

Texture2D positionTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D noiseTexture : register(t2);

SamplerState linearSampler : register(s4);

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PixelInputType input) : SV_TARGET
{
    //------------------------------------------------------------
    // 1. Fetch the fragment's view-space position & normal
    //------------------------------------------------------------
    float3 pixelPos = positionTexture.Sample(linearSampler, input.texCoord).xyz;
    float4 normalViewWithSkipSSAO = normalTexture.Sample(linearSampler, input.texCoord);
    float3 normalView = normalViewWithSkipSSAO.xyz;
    normalView = normalize(normalView * 2.0f - 1.0f); // Convert to [-1, 1]
    
    float skipSSAO = normalViewWithSkipSSAO.w;

    // If no geometry at this pixel (z=0?), early out
    if (pixelPos.z == 0.0f)
        return float4(1, 1, 1, 1);
    
    float2 noiseScale = float2(viewportWidth / 16.0, viewportHeight / 16.0);
    float2 noiseUV = input.texCoord * noiseScale;
    float3 randomVec = normalize(noiseTexture.Sample(linearSampler, noiseUV).xyz);
    
    float3 tangent = normalize(randomVec - normalView * dot(randomVec, normalView));
    float3 bitangent = cross(normalView, tangent);
    
    float3x3 TBN = float3x3(tangent, bitangent, normalView);
   
    float occlusion = 0.0f;
    const int KERNEL_SIZE = 64;
    
    [unroll]
    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        // get sample position
        float3 sampleVec = mul(samples[i].xyz, TBN); // from tangent to view-space
        float3 samplePos = pixelPos + sampleVec * radius;
        
        // project sample position (to sample texture) (to get position on screen/texture)
        float4 offset = mul(float4(samplePos, 1.0f), projectionMatrix); // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5f + 0.5f; // transform to range 0.0 - 1.0
        offset.y = 1.0 - offset.y;
        
        // get sample depth
        float sampleDepth = positionTexture.Sample(linearSampler, offset.xy).z; // get depth value of kernel sample
        
        // range check & accumulate
        float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(pixelPos.z - sampleDepth));
        occlusion += (sampleDepth <= samplePos.z - bias ? 1.0f : 0.0f) * rangeCheck;
    }

    //------------------------------------------------------------
    // 4. Average, invert, and apply distance fade
    //------------------------------------------------------------
    occlusion = 1.0f - (occlusion / KERNEL_SIZE);
    //occlusion *= fade; // Multiply by the distance-based fade factor
    
    return float4(occlusion, occlusion, occlusion, 1.0f);
}