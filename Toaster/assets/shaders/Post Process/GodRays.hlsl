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
    float4x4 lightViewProj;
    
    float4 direction; // FROM light -> scene
    
    float4 radiance; // RGB
    
    float SunIntensity;
    float DirectionalLightGain;
};

cbuffer GodRaySettings : register(b13)
{
    float exposure; // overall brightness (e.g. 0.35)
    float decay; // how quickly the shaft fades (e.g. 0.97)
    float density; // sample spacing factor   (e.g. 0.8)
    float weight; // per-sample contribution (e.g. 0.02)
    
    float kHalo;
    float HaloPower;
    float FogRangeMeters;
};

// ---------- resources --------------------------------------------------------
Texture2D DepthTexture          : register(t0); // full-res depth
Texture2D SunDiscMask           : register(t1); // ¼-res bright copy of HDR scene
Texture2D SunHaloMask           : register(t2);

SamplerState BilinearClamp      : register(s0);
SamplerState ClampPoint         : register(s1);

#define MAX_SAMPLES 160

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

float ViewDistanceFromDepth(float2 uv, float depth)
{
    // Guard: if nothing was written to depth (sky), it will be 0 with reversed-Z.
    if (depth <= 1e-12f)
        return 0.0f;

    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, depth, 1.0f);
    float4 vpos = mul(clip, inverseProjectionMatrix);
    float3 posVS = vpos.xyz / max(vpos.w, 1e-12f);
    return length(posVS); // meters
}

static const float3 LUMA = float3(0.2126f, 0.7152f, 0.0722f);

// ---------- pixel ------------------------------------------------------------
float4 main(VSOut input) : SV_TARGET
{
    float kHalo = 0.6f; // move to cbuffer
    
    float3 sunDirWS = -direction.xyz; // use .xyz explicitly
    float3 sunPosWS = cameraPosition.xyz + sunDirWS * (far * 0.99f);

    float4 sunClip = mul(float4(sunPosWS, 1.0f), mul(viewMatrix, projectionMatrix));

    // Fade out when the sun goes behind the camera (prevents popping)
    float behindFade = smoothstep(0.0f, 0.10f, sunClip.w);
    
    // If fully behind, no contribution.
    if (behindFade <= 0.0f)
        return 0;
    
    float2 ndc = sunClip.xy / sunClip.w;
    
    // Edge fade (avoid popping when sun goes off-screen)
    float edge = max(abs(ndc.x), abs(ndc.y)); // 0 center, 1 at edge
    float edgeFade = 1.0f - smoothstep(0.75f, 0.98f, edge);

    float sunVis = edgeFade * behindFade;
    if (sunVis <= 0.0f)
        return 0;

    float2 sunUV = float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
    float2 sunUVc = saturate(sunUV); // clamp target to keep marching stable near edges
    
    // Radial blur march using stable lerp-to-sun formulation
    float jitter = Random(input.texCoord * viewportWidth);
    float illumination = 0.0f;
    float decayAcc = 1.0f;

    // Optional: reduce ray length as sun approaches edge (prevents "stretchy" feel)
    // density acts like a global ray-length scale in this formulation
    //float tMax = saturate(density) * edgeFade; // keep density ~0.7..1.2
    float tMax = density * edgeFade;
    
    [loop]
    for (int i = 0; i < MAX_SAMPLES; ++i)
    {
        float t = ((float) i + jitter) / (float) MAX_SAMPLES; // 0..1
        t *= tMax;

        float2 uv = lerp(input.texCoord, sunUVc, t);

        float disc = SunDiscMask.Sample(BilinearClamp, uv).r;
        float halo = SunHaloMask.Sample(BilinearClamp, uv).r;

        // Tighten halo so it doesn't "inflate" via accumulation
        halo = pow(halo, HaloPower);

        float source = saturate(disc + kHalo * halo);

        illumination += source * decayAcc * weight;
        decayAcc *= decay;
    }

    // Receiver weighting: push shafts into haze over terrain (KSA-like look)
    float depth = DepthTexture.Sample(ClampPoint, input.texCoord).r;
    float viewDist = ViewDistanceFromDepth(input.texCoord, depth);
    
    // Fog amount: 0 near camera / sky, 1 far away in haze
    float fogAmt = 1.0f - exp(-viewDist / FogRangeMeters);
    fogAmt = saturate(0.25f + 0.75f * fogAmt);
    
    // If you still want a small contribution in sky, keep it; otherwise set to 0.
    bool isSky = (depth <= 1e-12f);
    float skyFactor = isSky ? 0.15f : 1.0f; // tune: 0.0 for terrain-only
    
    float rayFog = illumination * exposure * sunVis * fogAmt * skyFactor;
    rayFog = pow(max(rayFog, 0.0f), 0.65f);
    rayFog = rayFog / (1.0f + rayFog);

    float3 tint = radiance.rgb;
    float Y = max(dot(tint, LUMA), 1e-6);
    tint /= Y; // normalize to chroma (unit luminance)

    float sunScale = SunIntensity * 0.25f; // start small, tune
    return float4(rayFog * tint * sunScale, 1.0f);
}