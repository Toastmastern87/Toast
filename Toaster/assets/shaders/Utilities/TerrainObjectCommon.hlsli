#ifndef TERRATIN_OBJECT_COMMON_HLSLI
#define TERRATIN_OBJECT_COMMON_HLSLI

#define TERRAIN_SCATTER_THREADGROUP_SIZE 64

#define MAX_TERRAIN_INSTANCES_PER_TYPE  16384

#define TERRAIN_ARGS_STRIDE             32
#define TERRAIN_ARGS_INDEX_COUNT_OFF    0
#define TERRAIN_ARGS_INSTANCE_COUNT_OFF 4
#define TERRAIN_ARGS_START_INDEX_OFF    8
#define TERRAIN_ARGS_BASE_VERTEX_OFF    12
#define TERRAIN_ARGS_START_INSTANCE_OFF 16

struct TerrainInstance
{
    float3 PosCR;
    float Scale;
    float4 RotQuat;
};

float4 QuatFromEulerXYZ(float3 e)
{
    float3 h = e * 0.5f;
    float3 s, c;
    sincos(h, s, c);

    float4 q;
    q.x = s.x * c.y * c.z + c.x * s.y * s.z;
    q.y = c.x * s.y * c.z - s.x * c.y * s.z;
    q.z = c.x * c.y * s.z + s.x * s.y * c.z;
    q.w = c.x * c.y * c.z - s.x * s.y * s.z;
    return q;
}

float3 QuatRotate(float4 q, float3 v)
{
    float3 t = 2.0f * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

#endif // TERRATIN_OBJECT_COMMON_HLSLI