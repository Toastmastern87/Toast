#type compute
#include "ParticleCommon.hlsli"

// This is out pool of newborn particles here
RWStructuredBuffer<GPUParticle>     ParticleBuffer  : register(u0);
// Stack of FREE slot indices, we pop from here.
RWStructuredBuffer<uint>            DeadList        : register(u1);
// The CURRENT frame's alive list, here we append the newborn's index from the ParticleBuffer
RWStructuredBuffer<uint>            AliveList       : register(u2);
// The counters
RWStructuredBuffer<uint>            Counters        : register(u3);

// parameters for every emitter in the scene, indexed by EmitterIndex.
StructuredBuffer<EmitterParams> Emitters            : register(t0);

cbuffer ParticleEmitCB : register(b0)
{
    uint EmitCount;     // how many particles THIS dispatch should spawn
    uint EmitterIndex;  // which entry of Emitters[] to use
    uint FrameSeed;     // changes per frame so spawns differ frame to frame
    uint _pad0;
};

[numthreads(PARTICLE_THREADGROUP_SIZE, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    if (DTid.x >= EmitCount)
        return;
    
    // Claim a free slot
    uint deadCountBefore;
    InterlockedAdd(Counters[PARTICLECOUNTER_DEAD], (uint) -1, deadCountBefore);
    
    // Check if the pool is exhausted, more threads tried to spawn than there were free slots.
    if (deadCountBefore == 0)
    {
        InterlockedAdd(Counters[PARTICLECOUNTER_DEAD], 1u);
        return;
    }

    // Stack pop: the live region of DeadList is [0, deadCount), so the top
    // element is at deadCountBefore - 1.
    uint particleIndex = DeadList[deadCountBefore - 1];
    
    // Initialize the particle
    EmitterParams e = Emitters[EmitterIndex];
    RNG rng = MakeRNG(particleIndex, FrameSeed, EmitterIndex);

    float3 spawnPos = e.SpawnPosition;
    float3 spawnVel = e.Velocity;
    
    // This mirrors the switch in the old Particle System OnUpdate on the CPU.
    if (e.EmitFunction == EMITFUNCTION_CONE)
    {
        // CONE spreads the VELOCITY; position stays at the emitter origin.
        spawnVel = RandomVelocityInCone(e.Velocity, e.ConeAngleDegrees, rng);
    }
    else if (e.EmitFunction == EMITFUNCTION_BOX)
    {
        // BOX spreads the POSITION; velocity is used as-is.
        spawnPos = RandomPointInBox(e.SpawnPosition, e.SpawnSize, e.BiasExponent, rng);
    }
    
    GPUParticle p;
    p.Position = spawnPos; // origin-relative (floating-origin space)
    p.Age = 0.0f;
    p.Velocity = spawnVel;
    p.Lifetime = e.MaxLifeTime; // the emitter's setting becomes this particle's own
    p.StartColor = e.StartColor;
    p.ColorBlendFactor = e.ColorBlendFactor;
    p.EndColor = e.EndColor;
    p.Size = e.Size;
    p.GrowRate = e.GrowRate;
    p.BurstInitial = e.BurstInitial;
    p.BurstDecay = e.BurstDecay;
    p.EmitterIndex = EmitterIndex;

    // Add particle
    ParticleBuffer[particleIndex] = p;
    
    // Last step, add it to the alive list
    uint aliveIndex;
    InterlockedAdd(Counters[PARTICLECOUNTER_ALIVE], 1u, aliveIndex);
    AliveList[aliveIndex] = particleIndex;
}