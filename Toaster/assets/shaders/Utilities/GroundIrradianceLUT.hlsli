#ifndef GROUND_IRRADIANCE_LUT_HLSLI
#define GROUND_IRRADIANCE_LUT_HLSLI

static const float GI_MU_MIN = -0.35f;

float2 GroundIrradianceUV(float r, float muS, float Rb, float Rt)
{
    float u = saturate((muS - GI_MU_MIN) / (1.0f - GI_MU_MIN));
    float v = sqrt(saturate((r - Rb) / max(Rt - Rb, 1e-3f)));
    return float2(u, v);
}

void GroundIrradianceRMuFromUV(float2 uv, float Rb, float Rt, out float r, out float muS)
{
    muS = GI_MU_MIN + uv.x * (1.0f - GI_MU_MIN);
    r = Rb + uv.y * uv.y * (Rt - Rb);
}

#endif // GROUND_IRRADIANCE_LUT_HLSLI