#ifndef TRANSMITTANCE_LUT_HLSLI
#define TRANSMITTANCE_LUT_HLSLI

float2 TransUV(float r, float mu, float Rb, float Rt)
{
    float H = sqrt(max(0.0f, (Rt - Rb) * (Rt + Rb)));
    float rho = sqrt(max(0.0f, (r - Rb) * (r + Rb)));
    
    float disc = (Rt - r) * (Rt + r) + r * r * mu * mu;
    float d = max(0.0f, -r * mu + sqrt(max(disc, 0.0f)));

    float dMin = Rt - r;
    float dMax = rho + H;
    
    float xMu = (d - dMin) / max(dMax - dMin, 1e-6f);
    float xR = rho / max(H, 1e-6f);

    return float2(xMu, xR);
}

void TransRMuFromUV(float2 uv, float Rb, float Rt, out float r, out float mu)
{
    float H = sqrt(max(0.0f, (Rt - Rb) * (Rt + Rb)));
    float rho = H * uv.y;
    r = sqrt(rho * rho + Rb * Rb);

    float dMin = Rt - r;
    float dMax = rho + H;
    float d = dMin + uv.x * (dMax - dMin);
    
    mu = (d <= 0.0f) ? 1.0f : ((Rt - r) * (Rt + r) - d * d) / (2.0f * r * d);
    mu = clamp(mu, -1.0f, 1.0f);
}

#endif // TRANSMITTANCE_LUT_HLSLI