#type compute

#define PLANET_BAKE_THREADGROUP_SIZE 64

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

cbuffer PlanetMeshBake : register(b13)
{
    uint BakePatchCount;
    uint BakeVerticesPerPatch;
    int BakePatchLevels;
    uint BakeMaterialCount;
    
    float planetRadius;
    float3 camHiPS;
};

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

struct PlanetPatch
{
    int Level; // per-instance
    float3 V0;
    float3 V1;
    float3 V2 : POSITION2;
    float3 P2RelHi;
    float3 P0RelHi;
    float3 P1RelHi;
    float3 PatchOriginPS;
};

struct PlanetBakedVertex
{
    float3 NormalPS;
    float Height;
    uint3 PackedDebug;
    uint Pad;
};

Texture2DArray<float> HeightCubeArray : register(t0);
StructuredBuffer<MaterialData> Materials : register(t1);
StructuredBuffer<NoiseLayerData> NoiseLayers : register(t2);
StructuredBuffer<int4> PermTables : register(t3);
Texture2DArray<float4> AlbedoCubeArray : register(t4);
StructuredBuffer<PlanetPatch> Patches : register(t5);

RWStructuredBuffer<PlanetBakedVertex> BakedVertices : register(u0);

#include "DirectionToCube.hlsli"
#include "PerlinNoise.hlsli"
#include "PlanetTerrainHelpers.hlsli"

uint PackHalf2(float a, float b)
{
    return f32tof16(a) | (f32tof16(b) << 16);
}

void GridCoordsFromIndex(uint v, uint N, out uint i, out uint j)
{
    uint rowStart = 0;
    uint rowLength = N + 1;
    j = 0;
    
    [loop]
    while (v >= rowStart + rowLength && rowLength > 1)
    {
        rowStart += rowLength;
        rowLength--;
        j++;
    }
    
    i = v - rowStart;
}

[numthreads(PLANET_BAKE_THREADGROUP_SIZE, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint slot = DTid.x;
    if (slot >= BakePatchCount * BakeVerticesPerPatch)
        return;
    
    uint patchIndex = slot / BakeVerticesPerPatch;
    uint v = slot - patchIndex * BakeVerticesPerPatch;
    
    PlanetPatch patch = Patches[patchIndex];
    
    uint N = 1u << (uint) BakePatchLevels;

    uint i, j;
    GridCoordsFromIndex(v, N, i, j);
    if (i + j > N)
    {
        i = min(i, N);
        j = N - i;
    }
    uint k = N - i - j;

    float invN = exp2(-(float) BakePatchLevels);
    float wi = (float) i * invN;
    float wj = (float) j * invN;
    float wk = (float) k * invN;

    // IMPORTANT: map weights consistently to corners.
    // You must ensure (i,j,k) correspond to (V1,V2,V0) or similar consistently.
    // Pick ONE mapping and keep it everywhere.
    float w0 = wk; // for V0
    float w1 = wi; // for V1
    float w2 = wj; // for V2

    float3 V0 = normalize(patch.V0);
    float3 V1 = normalize(patch.V1);
    float3 V2 = normalize(patch.V2);

    // Edge-consistent direction
    precise float3 dir = normalize(w0 * V0 + w1 * V1 + w2 * V2);
    
    uint matCount = min(BakeMaterialCount, (uint) MAX_MATERIALS);
    
    // Compute slope from base heightmap only (no noise)
    float3 baseNormal = ComputeBaseNormalPS(dir);
    float slope = 1.0 - saturate(dot(normalize(baseNormal), dir));
    float colorAvg = SampleColorAvg(dir);
    
    float3 normalPS = ComputeTerrainNormalPS(dir, patch.Level, matCount, slope, colorAvg, baseNormal);

    float3 worldPos = dir * planetRadius;

    float wallDebug = 0.0;
    float wallMaskDebug = 0.0;
    float erosionMaskDebug = 0.0, erosionPatternDebug = 0.0, erosionDeltaDebug = 0.0;
    
    float h = SampleTerrainHeight(dir, worldPos, patch.Level, matCount, slope, colorAvg, normalPS, wallDebug, wallMaskDebug, erosionMaskDebug, erosionPatternDebug, erosionDeltaDebug);
    
    PlanetBakedVertex output;
    output.NormalPS = normalPS;
    output.Height = h;
    output.PackedDebug = uint3(PackHalf2(wallDebug, wallMaskDebug), PackHalf2(erosionMaskDebug, erosionPatternDebug), PackHalf2(erosionDeltaDebug, 0.0));
    output.Pad = 0.0;

    BakedVertices[slot] = output;
}