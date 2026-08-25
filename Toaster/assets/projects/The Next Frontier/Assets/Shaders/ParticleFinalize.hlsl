#type compute
#include "ParticleCommon.hlsli"

// u0: the counters.
RWStructuredBuffer<uint>    Counters        : register(u0);
// u1: The raw indirect arguments buffer, we now write the draw arguments as well
RWByteAddressBuffer         IndirectArgs    : register(u1);

[numthreads(1, 1, 1)]
void main()
{
    // Move this frames survivors to the next frames counter.
    uint aliveCount = Counters[PARTICLECOUNTER_ALIVE_AFTER];
    Counters[PARTICLECOUNTER_ALIVE] = aliveCount;
    
    // Write the DrawIndexedInstancedIndirect arguments.
    //
    //   [0] IndexCountPerInstance   6, the existing quad index buffer
    //   [1] InstanceCount           one instance per living particle
    //   [2] StartIndexLocation      0
    //   [3] BaseVertexLocation      0
    //   [4] StartInstanceLocation   0
    //
    IndirectArgs.Store4(ARGS_OFFSET_DRAW, uint4(PARTICLE_QUAD_INDEX_COUNT, aliveCount, 0, 0));
    IndirectArgs.Store(ARGS_OFFSET_DRAW + 16, 0); // StartInstanceLocation
}