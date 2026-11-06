#ifndef SKY_VIEW_LUT_HLSLI
#define SKY_VIEW_LUT_HLSLI

// Sky-view LUT parameterization: Toast's per-frame mu window, with Hillaire 2020's layout inside it.
//   rows (v):    zenith angle across the window [top = mu1, bottom = mu0 (ground horizon)],
//                v = 1 - sqrt(1 - t): rows packed toward the horizon. v = 0 top, v = 1 horizon.
//   columns (u): azimuth from the sun, mirrored (the sky is symmetric about the sun's vertical plane).
//                u = sqrt((1 - cos(phi)) / 2): u = 0 toward the sun, u = 1 away.
// Texel centres sit exactly on the edges: unit = texel / (size - 1). Read with SampleLevel only.

// Tangent frame around the sun: f toward the sun's azimuth, s perpendicular.
// With the sun at the zenith or nadir its azimuth is undefined and the sky is the same in every
// azimuth, so any tangent works.
void SkyViewSunFrame(float3 up, float3 wSun, float3 fallbackTangent, out float3 f, out float3 s)
{
    float3 sunOnPlane = wSun - up * dot(wSun, up);
    float len = length(sunOnPlane);
    float3 fb = fallbackTangent - up * dot(fallbackTangent, up);
    f = (len > 1e-4f) ? sunOnPlane / len : normalize(fb);
    s = cross(up, f);
}

// Unit coordinates [0,1] -> texture uv, with 0 and 1 on the first and last texel centres.
float2 SkyViewUnitToUV(float2 unit, float2 lutSize)
{
    return (unit * (lutSize - 1.0f) + 0.5f) / lutSize;
}

// View direction -> LUT uv (readers).
float2 SkyViewUV(float3 w, float3 up, float3 f, float3 s, float mu0, float mu1, float2 lutSize)
{
    float th = acos(clamp(dot(w, up), -1.0f, 1.0f));
    float th1 = acos(clamp(mu1, -1.0f, 1.0f));
    float th0 = acos(clamp(mu0, -1.0f, 1.0f));
    float t = saturate((th - th1) / max(th0 - th1, 1e-6f));
    float v = 1.0f - sqrt(1.0f - t);//
    
    float xF = dot(w, f);
    float xS = abs(dot(w, s));
    float horiz = sqrt(xF * xF + xS * xS);
    float cosPhi = (horiz > 1e-6f) ? xF / horiz : 1.0f;
    float u = sqrt(saturate(0.5f - 0.5f * cosPhi));

    return SkyViewUnitToUV(float2(u, v), lutSize);
}

// LUT texel -> view direction (writer).
float3 SkyViewDir(uint2 texel, uint2 lutSize, float3 up, float3 f, float3 s, float mu0, float mu1)
{
    float2 unit = float2(texel) / max(float2(lutSize) - 1.0f, 1.0f);

    float th1 = acos(clamp(mu1, -1.0f, 1.0f));
    float th0 = acos(clamp(mu0, -1.0f, 1.0f));
    
    float t = 1.0f - (1.0f - unit.y) * (1.0f - unit.y); // inverse of v = 1 - sqrt(1 - t)
    float th = lerp(th1, th0, t);

    float cosPhi = 1.0f - 2.0f * unit.x * unit.x; // inverse of u = sqrt((1 - cos) / 2)
    float sinPhi = sqrt(saturate(1.0f - cosPhi * cosPhi));

    return normalize(cos(th) * up + sin(th) * (cosPhi * f + sinPhi * s));
}

#endif // SKY_VIEW_LUT_HLSLI