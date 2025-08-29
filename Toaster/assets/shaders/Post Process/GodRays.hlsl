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

cbuffer DirectionalLight : register(b3)
{
    matrix lightViewProj;
    float4 direction;
    float4 radiance;
    float SunIntensity;
};


cbuffer GodRaySettings : register(b13)
{
    float exposure; // overall brightness (e.g. 0.35)
    float decay; // how quickly the shaft fades (e.g. 0.97)
    float density; // sample spacing factor   (e.g. 0.8)
    float weight; // per-sample contribution (e.g. 0.02)
};

// ---------- resources --------------------------------------------------------
Texture2D DepthTexture : register(t0); // full-res depth
Texture2D SourceTexture : register(t1); // ¼-res bright copy of HDR scene
SamplerState BilinearClamp : register(s0);

#define MAX_SAMPLES 128

struct VSOut
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float LinearEyeDepth(float nonLinearDepth)
{
    return near * far / (near + nonLinearDepth * (far - near));
}

float Random(float2 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * (p.x + p.y));
}

// ---------- pixel ------------------------------------------------------------
float4 main(VSOut input) : SV_TARGET
{
    float3 sunDirWS = -direction; // cam → sun
    float3 sunPosWS = cameraPosition.xyz + sunDirWS * (far * 0.99f);

    // clip-space
    float4 sunClip = mul(float4(sunPosWS, 1.0f),  mul(viewMatrix, projectionMatrix));

    // Sun behind camera?  early out → black pixel
    if (sunClip.w <= 0.0f)
        return 0;

    float2 ndc = sunClip.xy / sunClip.w; // −1 … +1

    // Outside viewport?  early out
    if (abs(ndc.x) > 1.0f || abs(ndc.y) > 1.0f)
        return 0;

    float2 sunUV;
    sunUV.x = ndc.x * 0.5f + 0.5f;
    sunUV.y = -ndc.y * 0.5f + 0.5f;
    
    // vector from the pixel to the sun in UV space
    float2 delta = sunUV - input.texCoord;
    if (length(delta) < 1e-4)                   // pixel = sun centre
        return 0;

    float2 stepUV = delta * (density / MAX_SAMPLES);
    float3 illumination = 0.0;
    
    float jitter = Random(input.texCoord * viewportWidth); // stable per frame
    float illumDecay = 1.0;
    float2 uv = input.texCoord + stepUV * jitter;
    
    float3 sunPosVS = mul(float4(sunPosWS, 1.0f), viewMatrix).xyz;
    float eyeDepthToSun = -sunPosVS.z;
    
    [loop]
    for (int i = 0; i < MAX_SAMPLES; ++i)
    {
        // 1. Sample non-linear depth at this UV
        float depthSample = DepthTexture.Sample(BilinearClamp, uv).r;

        // 2. Reconstruct clip-space and view-space position
        float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f); // Flip Y
        float4 clipPos = float4(ndc, depthSample, 1.0f);
        float4 viewPos = mul(clipPos, inverseProjectionMatrix);
        viewPos /= viewPos.w;

        float sampleEyeDepth = -viewPos.z;
        
        float occlusion = saturate((eyeDepthToSun - sampleEyeDepth) * 0.3);

        float3 col = SourceTexture.Sample(BilinearClamp, uv).rgb * occlusion;

        illumination += col * illumDecay * weight;
        illumDecay *= decay;

        uv += stepUV; // march
    }

    return float4(illumination * exposure, 1.0);
}