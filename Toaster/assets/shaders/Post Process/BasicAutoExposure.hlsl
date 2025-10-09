﻿#inputlayout
#type compute
#pragma pack_matrix(row_major)

cbuffer AutoExposureParams : register(b8)
{
    float LogLumMin; // log2 of min metered luminance (e.g. log2(1e-4))
    float LogLumMax; // log2 of max metered luminance (e.g. log2(16.0))
    float RejectBrightNits; // paper-white=1.0; e.g. 4.0 means >4x PW rejected hard
    float RejectBrightSoftNits; // soft knee start; e.g. 2.0
    float RejectDark; // reject very dark (linear) e.g. 0.002 (about -9 stops)
    float CenterWeight; // 0..1 extra weight toward center (e.g. 0.85)
}

Texture2D<float4> SceneHDR : register(t0);

SamplerState LinearClamp : register(s0);

RWTexture2D<float2> AEGroupBuffer : register(u0);

// ---------- Shared ----------
static const uint GROUP_W = 16;
static const uint GROUP_H = 16;

groupshared float sLog[GROUP_W * GROUP_H];
groupshared float sWgt[GROUP_W * GROUP_H];

[numthreads(GROUP_W, GROUP_H, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint W, H;
    SceneHDR.GetDimensions(W, H);

    // Per-thread index in groupshared
    uint li = GTid.y * GROUP_W + GTid.x;

    float logLum = 0.0f;
    float w = 0.0f;

    if (DTid.x < W && DTid.y < H)
    {
        float2 uv = (float2(DTid.xy) + 0.5) / float2(W, H);

        // Center bias (UE-style basic)
        float2 p = uv * 2.0 - 1.0;
        p.x *= (float) W / max(1.0, (float) H); // aspect correction
        float r2 = dot(p, p);
        float wCenter = lerp(1.0, exp(-3.0 * r2), saturate(CenterWeight));

        float3 c = SceneHDR.SampleLevel(LinearClamp, uv, 0).rgb;
        float lumLin = dot(c, float3(0.2126, 0.7152, 0.0722));

        // reject heavy darks (noise/stars)
        if (lumLin <= RejectDark)
            wCenter = 0.0;

        // soft-reject very bright (sun/halo)
        float wHi = 1.0 - smoothstep(RejectBrightSoftNits, RejectBrightNits, lumLin);

        w = wCenter * wHi;

        if (w > 0.0f)
        {
            logLum = clamp(log2(max(lumLin, 1e-8)), LogLumMin, LogLumMax);
        }
        else
        {
            logLum = 0.0f;
        }
    }

    sLog[li] = logLum * w;
    sWgt[li] = w;

    GroupMemoryBarrierWithGroupSync();

    // Parallel reduction in groupshared (power-of-two friendly)
    // Here we do a simple binary-tree reduction.
    for (uint stride = (GROUP_W * GROUP_H) >> 1; stride > 0; stride >>= 1)
    {
        if (li < stride)
        {
            sLog[li] += sLog[li + stride];
            sWgt[li] += sWgt[li + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // One write per group
    if (li == 0)
    {
        // AE_GroupBuffer is sized as (numGroupsX, numGroupsY)
        AEGroupBuffer[uint2(Gid.xy)] = float2(sLog[0], sWgt[0]);
    }
}