#inputlayout
#type compute

Texture2DArray<float4> HeightCubeArray : register(t0);
RWTexture2DArray<float4> normalOutput : register(u0);
SamplerState linearSampler : register(s0);

struct CubeSample
{
    uint face;
    float2 uv; // [0,1]
};

float3 CubeFaceUVToDir(uint face, float2 uv)
{
    // Match the bake: flip Y
    float2 p = 2.0 * float2(uv.x, 1.0 - uv.y) - 1.0;
    float px = p.x;
    float py = p.y;

    switch (face)
    {
        case 0:
            return normalize(float3(1.0, py, -px)); // +X
        case 1:
            return normalize(float3(-1.0, py, px)); // -X
        case 2:
            return normalize(float3(px, 1.0, -py)); // +Y
        case 3:
            return normalize(float3(px, -1.0, py)); // -Y
        case 4:
            return normalize(float3(px, py, 1.0)); // +Z
        default:
            return normalize(float3(-px, py, -1.0)); // -Z
    }
}

CubeSample DirectionToCube(float3 v)
{
    v = normalize(v);

    float ax = abs(v.x);
    float ay = abs(v.y);
    float az = abs(v.z);

    uint face;
    float2 uvFace;

    if (ax >= ay && ax >= az)
    {
        if (v.x > 0)
        {
            face = 0;
            uvFace = float2(-v.z, v.y) / ax;
        }
        else
        {
            face = 1;
            uvFace = float2(v.z, v.y) / ax;
        }
    }
    else if (ay >= ax && ay >= az)
    {
        if (v.y > 0)
        {
            face = 2;
            uvFace = float2(v.x, -v.z) / ay;
        }
        else
        {
            face = 3;
            uvFace = float2(v.x, v.z) / ay;
        }
    }
    else
    {
        if (v.z > 0)
        {
            face = 4;
            uvFace = float2(v.x, v.y) / az;
        }
        else
        {
            face = 5;
            uvFace = float2(-v.x, v.y) / az;
        }
    }

    CubeSample cs;
    cs.face = face;
    cs.uv = uvFace * 0.5 + 0.5;
    return cs;
}

float SampleHeight(float3 dir)
{
    CubeSample cs = DirectionToCube(dir);
    return HeightCubeArray.SampleLevel(linearSampler, float3(cs.uv, (float) cs.face), 0);
}

// ------------------------------------------------------------
// For a given cubemap texel (x,y,face) return the direction
// that would sample that texel in a standard cubemap layout.
// ------------------------------------------------------------
float3 GetSamplingVector(uint3 tid)
{
    uint width, height, depth;
    normalOutput.GetDimensions(width, height, depth);

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
    normalOutput.GetDimensions(width, height, depth);
    if (ThreadID.x >= width || ThreadID.y >= height || ThreadID.z >= depth)
        return;

    float3 dir = GetSamplingVector(ThreadID);

    float3 up = abs(dir.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 T1 = normalize(cross(up, dir));
    float3 T2 = cross(dir, T1);

    float texelAngle = 2.0f / (float) 2048;

    float hR = SampleHeight(normalize(dir + T1 * texelAngle));
    float hL = SampleHeight(normalize(dir - T1 * texelAngle));
    float hU = SampleHeight(normalize(dir + T2 * texelAngle));
    float hD = SampleHeight(normalize(dir - T2 * texelAngle));

    float stepMeters = texelAngle * 3389500.0f * 2.0f;

    float3 N = normalize(
        dir * stepMeters
        - T1 * (hR - hL)
        - T2 * (hU - hD)
    );

    normalOutput[ThreadID] = float4(N * 0.5f + 0.5f, 1.0f);
}