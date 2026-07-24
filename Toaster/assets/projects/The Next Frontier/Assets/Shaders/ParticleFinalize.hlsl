#type compute
#include "ParticleCommon.hlsli"

// u0: the counters.
RWStructuredBuffer<uint> Counters : register(u0);

[numthreads(1, 1, 1)]
void main()
{
    // Move this frames survivors to the next frames counter.
    Counters[PARTICLECOUNTER_ALIVE] = Counters[PARTICLECOUNTER_ALIVE_AFTER];
    
    // We will add more stuff here in the next step
}