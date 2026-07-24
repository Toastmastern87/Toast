#type compute
#include "ParticleCommon.hlsli"

// u0: the counters. We READ alive, and RESET the after-sim tally.
RWStructuredBuffer<uint>    Counters        : register(u0);
// u1: the raw indirect-args buffer. We WRITE the dispatch thread-group 
RWByteAddressBuffer         IndirectArgs    : register(u1);

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint aliveCount = Counters[PARTICLECOUNTER_ALIVE];
    
    // How many groups of 64 threads cover 'aliveCount' particles, rounded up.
    // If aliveCount is 0 this is 0, and DispatchIndirect(0,1,1) is a legal no-op.
    uint groupCount = (aliveCount + PARTICLE_THREADGROUP_SIZE - 1) / PARTICLE_THREADGROUP_SIZE;
    
    // Write the DispatchIndirect args: (ThreadGroupCountX, Y, Z) = (groupCount, 1, 1).
    // Store3 writes three consecutive uints starting at the given BYTE offset.
    IndirectArgs.Store3(ARGS_OFFSET_DISPATCH, uint3(groupCount, 1, 1));

    // Reset the survivor tally. Simulate counts up from here.
    Counters[PARTICLECOUNTER_ALIVE_AFTER] = 0;
}