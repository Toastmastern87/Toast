#type compute
#include "ParticleCommon.hlsli"

// u0: the pool - read a particle, integrate it, write it back.
RWStructuredBuffer<GPUParticle>     ParticleBuffer  : register(u0);
// u1: THIS frame's alive list (READ). Maps thread -> particle slot.
RWStructuredBuffer<uint>            AliveListIn     : register(u1);
// u2: NEXT frame's alive list (WRITE). Survivors are appended here.
RWStructuredBuffer<uint>            AliveListOut    : register(u2);
// u3: the dead list - dead particles push their slot back here.
RWStructuredBuffer<uint>            DeadList        : register(u3);
// u4: the counters.
RWStructuredBuffer<uint>            Counters        : register(u4);

cbuffer ParticleSimCB : register(b2)
{
    float DeltaTime;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

[numthreads(PARTICLE_THREADGROUP_SIZE, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    // The kickoff shader sized the dispatch to cover ALIVE particles, but we launch
    // whole groups of 64, so the last group has spare threads. Guard against
    uint aliveCount = Counters[PARTICLECOUNTER_ALIVE];
    
    if (DTid.x >= aliveCount)
        return;
    
    // Get the particle to simulate
    uint particleIndex = AliveListIn[DTid.x];
    GPUParticle p = ParticleBuffer[particleIndex];
    
    p.Age += DeltaTime;
    p.Position += p.Velocity * DeltaTime;
    
    if (p.Age >= p.Lifetime)
    {
        // Kills the particle and free its slot so that future emits can reuse it.
        // Interlock is used cause many particles may die on the same frame and they all need a unique
        // position in the dead list.
        // InterlockedAdd(dest, +1, original): 'original' is the count BEFORE
        // the add, which is exactly the index of the new top-of-stack.
        uint deadIndex;
        InterlockedAdd(Counters[PARTICLECOUNTER_DEAD], 1u, deadIndex);
        DeadList[deadIndex] = particleIndex;
        
        return;
    }
    
    // Survived, write back the particle with updated simulated values.
    ParticleBuffer[particleIndex] = p;
    
    uint outIndex;
    InterlockedAdd(Counters[PARTICLECOUNTER_ALIVE_AFTER], 1u, outIndex);
    AliveListOut[outIndex] = particleIndex;
}