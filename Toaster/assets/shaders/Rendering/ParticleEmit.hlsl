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

cbuffer ParticleEmitCB : register(b1)
{
    uint EmitCount;     // how many particles THIS dispatch should spawn
    uint EmitterIndex;  // which entry of Emitters[] to use
    uint FrameSeed;     // changes per frame so spawns differ frame to frame
    float EmitDeltaTime;
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
    if (deadCountBefore == 0 || deadCountBefore > PARTICLE_MAX_PARTICLES)
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

    // Sub-frame emission. Without this all particles within this frame will be spawning on the same position
    // causing "blocks" to appear at lower frame rate and overall making the particle system look less natural
    float u = (DTid.x + 0.5f) / (float) EmitCount;
    float3 emitterPos = lerp(e.PrevSpawnPosition, e.SpawnPosition, u);
    
    float3 spawnPos = emitterPos;
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
        spawnPos = RandomPointInBox(emitterPos, e.SpawnSize, e.BiasExponent, rng);
    }
    else if (e.EmitFunction == EMITFUNCTION_DISC)
    {
        float3 radialDir;
        float3 offset = RandomPointOnDisc(e.SpawnSize.x, rng, radialDir);
        
        spawnPos = emitterPos + offset;
        
        spawnVel = radialDir * e.Velocity.x + float3(0.0f, e.Velocity.y, 0.0f);
    }
    
    spawnVel *= lerp(1.0f - e.SpeedJitter, 1.0f + e.SpeedJitter, NextFloat(rng));
    
    if (e.DirectionalJitter > 0.0f)
        spawnVel = RandomVelocityInCone(spawnVel, e.DirectionalJitter, rng);
    
    GPUParticle p;
    p.Position = spawnPos - spawnVel * (u * EmitDeltaTime);
    p.Age = -u * EmitDeltaTime;
    p.Velocity = spawnVel;
    p.Lifetime = e.MaxLifeTime * lerp(1.0f - e.LifetimeJitter, 1.0f + e.LifetimeJitter, NextFloat(rng));
    p.Size = e.Size * lerp(1.0f - e.SizeJitter, 1.0f + e.SizeJitter, NextFloat(rng));
    p.StartColor = e.StartColor;
    p.ColorBlendFactor = e.ColorBlendFactor;
    p.EndColor = e.EndColor;
    p.GrowRate = e.GrowRate;
    p.BurstInitial = e.BurstInitial;
    p.BurstDecay = e.BurstDecay;
    p.EmitterIndex = EmitterIndex;
    p.StartIntensity = e.StartIntensity;
    p.EndIntensity = e.EndIntensity;
    p.IntensityFalloff = e.IntensityFalloff;
    p.SoftFadeDistance = e.SoftFadeDistance;
    p.Drag = e.Drag;
    p.TurbulenceStrength = e.TurbulenceStrength;
    p.MaskSlice = e.MaskSlice;
    p.BlendMode = e.BlendMode;
    p.Rotation = NextFloat(rng) * 2.0f * PARTICLE_PI;
    p.AlphaScale = e.AlphaScale;
    p._pad1 = 0.0f;
    p._pad2 = 0.0f;

    // Add particle
    ParticleBuffer[particleIndex] = p;
    
    // Last step, add it to the alive list
    uint aliveIndex;
    InterlockedAdd(Counters[PARTICLECOUNTER_ALIVE], 1u, aliveIndex);
    AliveList[aliveIndex] = particleIndex;
}