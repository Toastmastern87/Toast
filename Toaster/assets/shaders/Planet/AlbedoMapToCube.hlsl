#inputlayout
#type compute
static const float PI = 3.141592653589793f;
static const float TWO_PI = 2.0f * PI;
static const float INV_PI = 1.0f / PI;
static const float INV_2PI = 1.0f / (2.0f * PI);

Texture2D<float4> inputTexture : register(t0);
RWTexture2DArray<float4> outputTexture : register(u0);
SamplerState defaultSampler : register(s0);

float3 GetSamplingVector(uint3 tid)
{
    uint width, height, depth;
    outputTexture.GetDimensions(width, height, depth);
    float2 st = (float2(tid.xy) + 0.5f) / float2(width, height);
    float2 uv = 2.0f * st - 1.0f;
    float3 dir;
    switch (tid.z)
    {
        case 0:
            dir = float3(1.0, uv.y, -uv.x);
            break;
        case 1:
            dir = float3(-1.0, uv.y, uv.x);
            break;
        case 2:
            dir = float3(uv.x, 1.0, -uv.y);
            break;
        case 3:
            dir = float3(uv.x, -1.0, uv.y);
            break;
        case 4:
            dir = float3(uv.x, uv.y, 1.0);
            break;
        default:
            dir = float3(-uv.x, uv.y, -1.0);
            break;
    }
    return normalize(dir);
}

[numthreads(32, 32, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    uint width, height, depth;
    outputTexture.GetDimensions(width, height, depth);
    if (ThreadID.x >= width || ThreadID.y >= height || ThreadID.z >= depth)
        return;

    float3 v = GetSamplingVector(ThreadID);

    // Apply the same coordinate fix as your UV mapping
    float3 adjusted = float3(-v.x, v.y, -v.z);

    float phi = atan2(adjusted.x, adjusted.z);
    float theta = acos(clamp(adjusted.y, -1.0f, 1.0f));

    float u = phi * INV_2PI + 0.5f;
    u = frac(u);
    float vTex = theta * INV_PI;

    float4 sample = inputTexture.SampleLevel(defaultSampler, float2(u, vTex), 0);
    outputTexture[ThreadID] = sample;
}