#type compute

#include "TerrainObjectCommon.hlsli"

cbuffer PlanetFrame : register(b4)
{
    float3 PlanetCenterCR;
    float PlanetRadius;
    float3 BasisTanEast;
    float MaxHeight;
    float3 BasisTanNorth;
    float MinHeight;
    float3 BasisRadUp;
    float Altitude;
    float3 BasisLonEast;
    int NumHeightDetails;
    float3 BasisLonNorth;
    float _pad0;
    float3 BasisSpinUp;
    float _pad1;
};

cbuffer PlanetRenderingSettings : register(b11)
{
    int MaterialCount;
    uint MaterialsEnabled;
    float PBRColorDominance;
    float ColorNoiseFrequency;
    
    float ColorNoiseStrength;
    int ColorNoiseOctaves;
    float WallEnhancementEnabled;
    float WallStrength;
    
    float WallStepMeters;
    float WallSlopeStart;
    float WallSlopeEnd;
    float WallSharpStart;
    
    float WallSharpEnd;
    float WallMaxDelta;
    float WallDebugEnabled;
    float WallDebugMode;
    
    float TerrainNormalStepMeters;
    float ErosionEnabled;
    float ErosionStrength;
    float ErosionStepMeters;
    
    float ErosionTilingMeters;
    float ErosionSlopeStart;
    float ErosionSlopeFull;
    float ErosionSlopeEnd;
    
    float ErosionSlopeFadeOut;
    int ErosionOctaves;
    float ErosionLacunarity;
    float ErosionPersistence;
    
    float ErosionDebugEnabled;
    int ErosionDebugMode;
    float ErosionGullyWeight;
    float ErosionDetail;
    
    float ErosionCellScale;
    float ErosionNormalization;
    float ErosionAssumedSlope;
    float ErosionAssumedSlopeBlend;
    
    float ErosionMaxDistance;
    float ErosionFadeStart;
    float pad0, pad1;
};

cbuffer TerrainScatter : register(b12)
{
    uint TOSTypeIndex;
    uint TOSCandidateGridSize;
    uint TOSCandidateCount;
    uint TOSSeed;
    
    float TOSDensityProb;
    float TOSScatterRadiusMeters;
    float TOSMinScale;
    float TOSMaxScale;
    
    float TOSPlayerTangentEast;
    float TOSPlayerTangentNorth;
    uint TOSIndexCountPerInstance;
    uint TOSStartIndexLocation;
    
    float planetRadius;
    float3 camHiPS;
}

struct MaterialData
{
    // 16 bytes
    float SlopeMin;
    float SlopeMax;
    float BlendSharpness;
    int NoiseLayerStart;

    // 16 bytes
    int NoiseLayerCount;
    float UVTilingScale;
    float ColorAvgMin;
    float ColorAvgMax;

    // 16 bytes
    float UseAlbedo;
    float3 DebugColor;
};

struct NoiseLayerData
{
    // 16 bytes
    int Type; // 0=Fractal, 1=Ridged, 2=Turbulence
    int LODActivation;
    int Octaves;
    int PermBase;

    // 16 bytes
    float Frequency;
    float Amplitude;
    float Lacunarity;
    float Persistence;

    // 16 bytes
    float BlendWeight;
    float RadialFrequencyScale;
    float RidgeSharpness;
    float pad0;
};

Texture2DArray<float> HeightCubeArray : register(t0);
StructuredBuffer<MaterialData> Materials : register(t1);
StructuredBuffer<NoiseLayerData> NoiseLayers : register(t2);
StructuredBuffer<int4> PermTables : register(t3);
Texture2DArray<float4> AlbedoCubeArray : register(t4);

RWStructuredBuffer<TerrainInstance> InstanceBuffer : register(u0);
RWByteAddressBuffer IndirectArgs : register(u1);

#define MAX_MATERIALS 8

#include "DirectionToCube.hlsli"
#include "PerlinNoise.hlsli"
#include "PlanetTerrainHelpers.hlsli"

uint Hash_u32(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float Hash01(uint x)
{
    // 24-bit mantissa -> [0,1)
    return (Hash_u32(x) & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}

float2 Hash02(uint x)
{
    return float2(Hash01(x), Hash01(x ^ 0x9e3779b9u));
}

float3 Hash03(uint x)
{
    return float3(Hash01(x), Hash01(x ^ 0x9e3779b9u), Hash01(x ^ 0x85ebca6bu));
}

float EvaluateTerrainHeightMeters(float2 offMeters)
{
    // 1) Reference-sphere direction at the offset from player
    float3 pSphereLocal = BasisRadUp + BasisTanEast * (offMeters.x / PlanetRadius) + BasisTanNorth * (offMeters.y / PlanetRadius);
    float3 nWS = normalize(pSphereLocal);

    // 2) Convert WS direction to planet-local axes
    float3 vPlanet;
    vPlanet.x = dot(nWS, BasisLonEast);
    vPlanet.y = dot(nWS, BasisSpinUp);
    vPlanet.z = dot(nWS, BasisLonNorth);

    float3 dir = normalize(vPlanet);
    float3 worldPos = dir * PlanetRadius;

    // 3) Inputs needed by SampleTerrainHeight
    int currentLOD = 0;
    uint matCount = min((uint) MaterialCount, (uint) MAX_MATERIALS);
    float3 baseNormal = ComputeBaseNormalPS(dir);
    float slope = 1.0 - saturate(dot(normalize(baseNormal), dir));
    float colorAvg = SampleColorAvg(dir);
    float3 normalPS = nWS; // not actually used inside SampleTerrainHeight per the code above

    // 4) Out parameters we don't care about (debug-only)
    float wallDebug, wallMaskDebug, erosionMaskDebug, erosionPatternDebug, erosionDeltaDebug;

    return SampleTerrainHeight(dir, worldPos, currentLOD, matCount, slope, colorAvg, normalPS, wallDebug, wallMaskDebug, erosionMaskDebug, erosionPatternDebug, erosionDeltaDebug);
}

[numthreads(TERRAIN_SCATTER_THREADGROUP_SIZE, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint candidateIndex = DTid.x;
    if (candidateIndex > TOSCandidateCount)
        return;
    
    uint M = TOSCandidateGridSize;
    
    uint ix = candidateIndex % M;
    uint iy = candidateIndex / M;
    
    float candidateCellSize = (2.0f * TOSScatterRadiusMeters) / (float) M;

    // Local offset in tangent meters (centered on player)
    float2 offMeters = (float2((float) ix + 0.5f, (float) iy + 0.5f) * candidateCellSize) - float2(TOSScatterRadiusMeters, TOSScatterRadiusMeters);

    // Convert to world-stable position for hashing
    float2 globalMeters = float2(TOSPlayerTangentEast, TOSPlayerTangentNorth) + offMeters;

    // World cell ID — stable as the player moves
    int worldCX = (int) floor(globalMeters.x / candidateCellSize);
    int worldCY = (int) floor(globalMeters.y / candidateCellSize);

    // Stable world-space hash key
    uint key = Hash_u32(TOSSeed ^ Hash_u32((uint) worldCX) ^ (Hash_u32((uint) worldCY) * 0x85ebca6bu));

    // Density-based cull
    if (Hash01(key) > TOSDensityProb)
        return;

    // Stable randoms for this stone
    float2 r2 = Hash02(key);
    float3 r3 = Hash03(key ^ 0x68bc21ebu);

    // Jitter within the world cell (stable)
    float2 jitteredGlobalMeters = (float2((float) worldCX, (float) worldCY) + r2) * candidateCellSize;
    offMeters = jitteredGlobalMeters - float2(TOSPlayerTangentEast, TOSPlayerTangentNorth);

    // Cull beyond scatter radius (after jitter could push slightly out)
    if (length(offMeters) > TOSScatterRadiusMeters)
        return;
    
    // Sample terrain height at this offset
    float h = EvaluateTerrainHeightMeters(offMeters);

        // Place on tangent plane
    float3 pPlaneCR = BasisTanEast * offMeters.x + BasisTanNorth * offMeters.y;
    float3 baseCR = pPlaneCR + BasisRadUp * (h - Altitude);
    
    TerrainInstance inst;
    inst.PosCR = baseCR;
    inst.Scale = lerp(TOSMinScale, TOSMaxScale, r3.x);
    inst.RotQuat = QuatFromEulerXYZ(r3 * 6.2831853f);
    
    uint argsBase = TOSTypeIndex * TERRAIN_ARGS_STRIDE;
    
    uint slot;
    IndirectArgs.InterlockedAdd(argsBase + TERRAIN_ARGS_INSTANCE_COUNT_OFF, 1, slot);
    
    if (slot >= MAX_TERRAIN_INSTANCES_PER_TYPE)
    {
        uint ignored;
        IndirectArgs.InterlockedMin(argsBase + TERRAIN_ARGS_INSTANCE_COUNT_OFF, MAX_TERRAIN_INSTANCES_PER_TYPE, ignored);
        
        return;
    }
    
    InstanceBuffer[slot] = inst;

}