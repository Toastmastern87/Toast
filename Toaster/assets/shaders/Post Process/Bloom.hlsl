﻿#inputlayout
#type vertex
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

PixelInputType main(uint vID : SV_VertexID)
{
    PixelInputType output;

	//https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
    output.uv = float2((vID << 1) & 2, vID & 2);
    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0.0f, 1);

    return output;
}

#type pixel
#pragma pack_matrix( row_major )

cbuffer BloomParams : register(b11)
{
    float SunSurfaceThreshold;
    float SunSurfaceIntensity;
    float SunSpaceThreshold;
    float SunSpaceIntensity;
    
    float SkySurfaceThreshold;
    float SkySurfaceIntensity;
    float SkySpaceThreshold;
    float SkySpaceIntensity;
    
    float GeometryThreshold;
    float GeometryIntensity ;
    float SunRadius;
    float SkySurfaceRadius;
    
    float SkySpaceRadius; 
    float SoftKnee; 
    float SaturationClamp;
    float spaceFactor;
};

Texture2D sceneBaseTexture      : register(t0);
Texture2D<float> SceneDepth     : register(t1);
Texture2D SunDiscMaskRT         : register(t2);
Texture2D SunHaloMaskRT         : register(t3);

SamplerState ClampPoint         : register(s2);
SamplerState clampSampler       : register(s3);

float3 softKneeBright(float3 color, float threshold, float kneeFrac)
{
    float k = saturate(kneeFrac) * max(threshold, 1e-6);
    float3 over = max(color - threshold, 0.0.xxx);
    float3 soft = max(color - (threshold - k), 0.0.xxx);
    float invDen = (k > 1e-6) ? (1.0 / (4.0 * k)) : 0.0;
    float3 knee = soft * soft * invDen;
    return max(over, knee);
}


struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

struct PixelOutputType
{
    float4 sun      : SV_Target0; // bright: sun (disc+halo)
    float4 sky      : SV_Target1; // bright: sky (no sun)
    float4 geometry : SV_Target2; // bright: geometry
};

PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;
    float2 uv = input.uv;
   
    float3 hdr = sceneBaseTexture.Sample(clampSampler, uv).rgb;
    
    // Masks
    float disc = SunDiscMaskRT.Sample(clampSampler, uv).r;
    float halo = SunHaloMaskRT.Sample(clampSampler, uv).r;
    float sunMask = saturate(disc + halo);
    
    // Depth partition
    float depth = SceneDepth.Sample(ClampPoint, uv);
    float isSky = (depth <= 1e-12f) ? 1.0f : 0.0f;
    float isGeom = 1.0f - isSky;
    
    // Remove sun from sky
    float skyOnlyMask = isSky * (1.0f - disc);
    
    // Surface↔Space thresholds
    float sunThreshold = lerp(SunSurfaceThreshold, SunSpaceThreshold, spaceFactor);
    float skyThreshold = lerp(SkySurfaceThreshold, SkySpaceThreshold, spaceFactor);
    float geoThreshold = (GeometryThreshold > 0.0) ? GeometryThreshold : skyThreshold;

    // Bright contributions
    float3 sunBright = softKneeBright(hdr, sunThreshold, SoftKnee) * sunMask;
    float3 skyBright = softKneeBright(hdr, skyThreshold, SoftKnee) * skyOnlyMask;
    float3 geomBright = softKneeBright(hdr, geoThreshold, SoftKnee) * isGeom;

    output.sun = float4(sunBright, 1.0f);
    output.sky = float4(skyBright, 1.0f);
    output.geometry = float4(geomBright, 1.0f);
    return output;
}