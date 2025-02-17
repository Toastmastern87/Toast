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
    float viewportWidth;
    float viewportHeight;
};

cbuffer SSAO : register(b10)
{
    float4 samples[64]; // Same order as CPU
    float radius;
};

Texture2D positionTexture       : register(t0);
Texture2D normalTexture         : register(t1);
Texture2D noiseTexture          : register(t2);

SamplerState linearSampler      : register(s4);

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
    float3 pixelPosView = positionTexture.Sample(linearSampler, input.texCoord).xyz;
    float3 normalView = normalize(normalTexture.Sample(linearSampler, input.texCoord).xyz);
    normalView = normalize(normalView * 2.0f - 1.0f); // Convert to [-1, 1]

    // If no geometry at this pixel (z=0?), early out
    if (pixelPosView.z == 0.0f)
        return float4(1, 1, 1, 1);

    //------------------------------------------------------------
    // 2. Distance-based fade-out for large scenes
    //------------------------------------------------------------
    // We'll fade from full occlusion at 200m to no occlusion at 250m.
    //float fadeStart = 200.0f;
    //float fadeEnd = 250.0f;

    // +Z is forward, so fragPosView.z = distance from camera in your engine
    float distFromCamera = pixelPosView.z;

    // Compute a fade factor in [0..1], where 1 = full occlusion, 0 = no occlusion
    //float fade = saturate((fadeEnd - distFromCamera) / (fadeEnd - fadeStart));

    // If the fragment is > 250 units away, fade will be 0 → no occlusion
    // If it's <= 200 units, fade will be 1 → full occlusion
    // Between 200 and 250, it transitions smoothly

    //------------------------------------------------------------
    // 3. Accumulate occlusion from nearby samples
    //------------------------------------------------------------
    float occlusion = 0.0f;
    const int KERNEL_SIZE = 64;

    [unroll]
    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        // 3a. Offset in view space
        float3 samplePosView = pixelPosView + (samples[i].xyz * radius);

        // 3b. Re-project to clip space
        float4 clipPos = mul(float4(samplePosView, 1.0f), projectionMatrix);
        if (clipPos.w <= 0.0f)
            continue;

        // 3c. Convert to UV in the position texture
        float2 sampleUV = clipPos.xy / clipPos.w * 0.5f + 0.5f;
        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f ||
            sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            continue;

        // 3d. Get the position of whatever is actually there
        float3 sampleFragPosView = positionTexture.Sample(linearSampler, sampleUV).xyz;
        if (sampleFragPosView.z == 0.0f)
            continue;

        // 3e. Range check to soften occlusion if sample is near
        float dist = distance(pixelPosView, sampleFragPosView);
        float rangeCheck = smoothstep(0.0f, radius, dist);

        // 3f. Normal alignment
        float3 dir = sampleFragPosView - pixelPosView;
        float ndotd = max(dot(normalView, normalize(dir)), 0.0f);

        // 3g. Depth check for +Z forward (larger Z = farther from camera)
        if (sampleFragPosView.z > samplePosView.z)
            occlusion += ndotd * rangeCheck;
    }

    //------------------------------------------------------------
    // 4. Average, invert, and apply distance fade
    //------------------------------------------------------------
    occlusion = 1.0f - (occlusion / KERNEL_SIZE);
    //occlusion *= fade; // Multiply by the distance-based fade factor

    return float4(occlusion, occlusion, occlusion, 1.0f);
}