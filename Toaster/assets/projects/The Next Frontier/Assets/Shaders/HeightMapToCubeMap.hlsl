#inputlayout
#type compute

static const float PI = 3.141592653589793f;
static const float TWO_PI = 2.0f * PI;
static const float INV_PI = 1.0f / PI;
static const float INV_2PI = 1.0f / (2.0f * PI);

// Input: equirectangular texture (lon 0..360, lat 90..-90)
Texture2D<float4> inputTexture : register(t0);

// Output: cubemap stored as Texture2DArray (6 slices)
RWTexture2DArray<float> outputTexture : register(u0);

SamplerState defaultSampler : register(s0);

// ------------------------------------------------------------
// For a given cubemap texel (x,y,face) return the direction
// that would sample that texel in a standard cubemap layout.
// ------------------------------------------------------------
float3 GetSamplingVector(uint3 tid)
{
    uint width, height, depth;
    outputTexture.GetDimensions(width, height, depth);

    // Normalized coords in [0,1], sample at texel centers
    float2 st = (float2(tid.xy) + 0.5f) / float2(width, height);

    // Map to [-1,1] WITHOUT Y flip so it matches runtime CubeFaceUVToDir
    float2 uv = 2.0f * st - 1.0f; // uv.x, uv.y in [-1,1]

    float3 dir;
    switch (tid.z)
    {
        case 0:
            dir = float3(1.0, uv.y, -uv.x);
            break; // +X
        case 1:
            dir = float3(-1.0, uv.y, uv.x);
            break; // -X
        case 2:
            dir = float3(uv.x, 1.0, -uv.y);
            break; // +Y
        case 3:
            dir = float3(uv.x, -1.0, uv.y);
            break; // -Y
        case 4:
            dir = float3(uv.x, uv.y, 1.0);
            break; // +Z
        default:
            dir = float3(-uv.x, uv.y, -1.0);
            break; // -Z
    }

    return normalize(dir);
}

[numthreads(32, 32, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    uint width, height, depth;
    outputTexture.GetDimensions(width, height, depth);

    // Guard against over-dispatch
    if (ThreadID.x >= width || ThreadID.y >= height || ThreadID.z >= depth)
        return;

    float3 v = GetSamplingVector(ThreadID);

    // Cartesian -> spherical
    float phi = atan2(v.z, v.x); // [-PI, PI]
    float theta = acos(clamp(v.y, -1.0f, 1.0f)); // [0, PI], 0 = north pole

    // Equirectangular UV
    float u = phi * INV_2PI + 0.5f;
    u = frac(u);
    float vTex = theta * INV_PI;

    float sample = inputTexture.SampleLevel(defaultSampler, float2(u, vTex), 0);
    outputTexture[ThreadID] = sample;
}
