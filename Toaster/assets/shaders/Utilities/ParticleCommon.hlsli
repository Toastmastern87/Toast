#ifndef PARTICLE_COMMON_HLSLI
#define PARTICLE_COMMON_HLSLI

//  Shared particle definitions for the GPU side. Need to match the CPU side
#define PARTICLE_THREADGROUP_SIZE 64

#define PARTICLE_MAX_PARTICLES 262144

#define PARTICLE_PI 3.14159265359f

// Spawn shape. MUST match enum class EmitFunction in ParticleCommon.h.
#define EMITFUNCTION_NONE 0
#define EMITFUNCTION_CONE 1
#define EMITFUNCTION_BOX  2

// Fixes a bug in the old system
#define CONE_PRESERVE_SPEED 1

// 1 = uniform over the cone's solid angle.
// 0 = the old systems existing axis-clustered look (denser core; good for plumes).
#define CONE_UNIFORM_DISTRIBUTION 0

// Must stay indentical to GPUParticle in ParticleCommon
// In food terms this can be seen as a dish that uses the recipe(EmitterParams) to produce a particle.
struct GPUParticle
{
    float3  Position;
    float   Age;
    float3  Velocity;
    float   Lifetime;
    float3  StartColor;
    float   ColorBlendFactor;
    float3  EndColor;
    float   Size;
    float   GrowRate;
    float    BurstInitial;
    float   BurstDecay;
    uint    EmitterIndex;
};

// In food terms this can be seen as the recipe
struct EmitterParams
{
    float3  SpawnPosition;
    float   _pad0;
    float3  SpawnSize;
    float   BiasExponent;
    float3  Velocity;
    float   ConeAngleDegrees;
    float3  StartColor;
    float   ColorBlendFactor;
    float3  EndColor;
    float   MaxLifeTime;
    float   Size;
    float   GrowRate;
    float   BurstInitial;
    float   BurstDecay;
    uint    EmitFunction;
    uint    _pad1;
    uint    _pad2;
    uint    _pad3;
};

// Indices into the Counter buffer (RWStructuredBuffer<uint>, 4 elements).
// ALIVE + DEAD == MAX_PARTICLES, always.
#define PARTICLECOUNTER_ALIVE       0
#define PARTICLECOUNTER_DEAD        1
#define PARTICLECOUNTER_EMIT        2
#define PARTICLECOUNTER_ALIVE_AFTER 3

// Byte offsets inside the raw IndirectArgs buffer
#define ARGS_OFFSET_DISPATCH    0   // uint3: ThreadGroupCountX/Y/Z      (DispatchIndirect)
#define ARGS_OFFSET_DRAW        16  // uint5: IndexCountPerInstance, InstanceCount,
                                    //        StartIndexLocation, BaseVertexLocation,
                                    //        StartInstanceLocation
                                    //                        (DrawIndexedInstancedIndirect)

//  Random number generation
uint WangHash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

struct RNG
{
    uint State;
};

RNG MakeRNG(uint particleIndex, uint frameSeed, uint emitterIndex)
{
    RNG rng;

    rng.State = WangHash(particleIndex * 747796405u
                       + frameSeed * 719393u
                       + emitterIndex * 9781u + 1u);
    return rng;
}

uint NextUInt(inout RNG rng)
{
    rng.State = WangHash(rng.State);
    return rng.State;
}

// Uniform float in [0, 1).
float NextFloat(inout RNG rng)
{
    return NextUInt(rng) * (1.0f / 4294967296.0f);
}

// --------------------------------------------------------------------------------------------
// Spawn shape helpers - direct ports of my CPU implementations in the old Particle System
// --------------------------------------------------------------------------------------------

// Port of ParticleSystem::RandomVelocityInCone() in my old Particle System
float3 RandomVelocityInCone(float3 baseDir, float coneAngleDegrees, inout RNG rng)
{
    float coneAngleRadians = radians(coneAngleDegrees);

    float u = NextFloat(rng);
    float v = NextFloat(rng);

#if CONE_UNIFORM_DISTRIBUTION
    float cosTheta = lerp(1.0f, cos(coneAngleRadians), u);
    float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));
#else
    float theta = u * coneAngleRadians; // your original: clusters toward the axis
    float sinTheta, cosTheta;
    sincos(theta, sinTheta, cosTheta);
#endif

    float phi = v * 2.0f * PARTICLE_PI;

    // Local-space direction, cone aligned to +Z.
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float z = cosTheta;

    float speed = length(baseDir);

    // A zero-length base direction has no meaningful orientation. Your CPU
    // version would divide by zero here; we bail instead.
    if (speed < 1e-6f)
        return float3(0.0f, 0.0f, 0.0f);

    float3 base = baseDir / speed;

    // Pick any 'up' that isn't parallel to base, else the cross product
    // degenerates to a zero vector.
    float3 up = float3(0.0f, 1.0f, 0.0f);
    if (abs(dot(base, up)) > 0.99f)
        up = float3(1.0f, 0.0f, 0.0f);

    float3 right = normalize(cross(up, base));
    float3 newUp = cross(base, right);

    float3 dir = normalize(right * x + newUp * y + base * z);

#if CONE_PRESERVE_SPEED
    return dir * speed; // fixes Bug 1
#else
    return dir;            // old behaviour: always unit length
#endif
}

// Port of ParticleSystem::BiasedRandomValue() in my old Particle System
float BiasedRandomValue(float halfExtent, float biasExponent, inout RNG rng)
{
    float r = NextFloat(rng);
    float value = r * 2.0f - 1.0f; // map to [-1, 1]
    float biased = (value < 0.0f ? -1.0f : 1.0f) * pow(abs(value), biasExponent);
    return biased * halfExtent;
}

// Port of ParticleSystem::RandomPointInBox() in my old Particle System
float3 RandomPointInBox(float3 boxCenter, float3 boxSize, float biasExponent, inout RNG rng)
{
    return boxCenter + float3(BiasedRandomValue(boxSize.x, biasExponent, rng), BiasedRandomValue(boxSize.y, biasExponent, rng), BiasedRandomValue(boxSize.z, biasExponent, rng));
}

#endif // PARTICLE_COMMON_HLSLI