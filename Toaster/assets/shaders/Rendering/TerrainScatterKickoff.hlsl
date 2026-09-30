#type compute

#include "TerrainObjectCommon.hlsli"

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

RWByteAddressBuffer IndirectArgs : register(u1);

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint argsBase = TOSTypeIndex * TERRAIN_ARGS_STRIDE;
    
    IndirectArgs.Store4(argsBase, uint4(TOSIndexCountPerInstance, 0, TOSStartIndexLocation, 0));
    IndirectArgs.Store(argsBase + TERRAIN_ARGS_START_INSTANCE_OFF, 0);

}