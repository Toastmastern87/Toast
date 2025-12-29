#inputlayout        // packed 16-bit grid coordinates: uint16 gx, uint16 gy
vertex

#type vertex
#pragma pack_matrix( row_major )

static const float PI = 3.14159265358979323846f;
static const float INV_TWO_PI = 1.0f / (2.0f * PI);
static const float INV_PI = 1.0f / PI;
static const uint EDGE_CELLS = 12; // number of cells in edge blending strip

struct VertexInputType
{
    uint2 grid : POSITION0; // gx,gy 0 … N-1
};

cbuffer Camera : register(b0)
{
    matrix worldTranslationMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float far;
    float near;
    float viewportWidth;
    float viewportHeight;
};

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
    float3 BasisSpinUp;
};

cbuffer PlanetLevel : register(b7) 
{
    int OriginX;
    int OriginY;
    int CellSize;
    int GridSize;
    int DrawMode;
};

struct PixelInputType
{
    float4 pixelPosition        : SV_POSITION;
    float3 viewPosition         : VIEWPOS;
    float3 normalSphereWS       : NORMAL0;
};

struct PlanetPointVS      
{
    float3 posVS; // for SV_POSITION
    float3 nWS; // unit sphere normal in world-space
};

struct DetailSettings
{
    int LODActivation;
    int Octaves;
    float Frequency;
    float Amplitude;
    int PermBase; // index into gPermTables (int4 units)
    float pad0, pad1, pad2;
};

struct RelSample
{
    float3 pRelWS; // camera-relative world space position (what you render)
    float3 nSphereWS; // radial direction used in the spherical branch (and for outward check)
    float h; // height (optional, for debugging)
};

Texture2DArray<float> HeightCubeArray           : register(t0);
    
StructuredBuffer<DetailSettings> Details       : register(t8);
StructuredBuffer<int4> PermTables              : register(t9);

SamplerState HeightMapSampler                   : register(s5);

#include "PerlinNoise.hlsli"    
#include "TerrainHeightCalculations.hlsli"

RelSample SampleRelSurface(float2 offMeters, float edgeW)
{
    RelSample s;

    // Build reference-sphere direction (same as your current VS)
    float3 pSphereLocal = BasisRadUp + BasisTanEast * (offMeters.x / PlanetRadius) + BasisTanNorth * (offMeters.y / PlanetRadius);

    float3 nWS = normalize(pSphereLocal);
    s.nSphereWS = nWS;

    float3 vPlanet = float3(dot(nWS, BasisLonEast), dot(nWS, BasisSpinUp), dot(nWS, BasisLonNorth));

    float3 pNoise = vPlanet * PlanetRadius;

    float h = SampleHeightFromDir(normalize(vPlanet));

    int lodFine = LodFromCellSize(CellSize);
    float detailFine = AccumulateHeightDetails(pNoise, lodFine);
    float detail = detailFine;

    if (DrawMode == 1)
    {
        int lodCoarse = lodFine + 1;
        float detailCoarse = AccumulateHeightDetails(pNoise, lodCoarse);
        detail = lerp(detailCoarse, detailFine, edgeW);
    }

    h += detail;
    s.h = h;

    // MATCH YOUR ACTUAL POSITION MODEL
    float3 pRelWS;

    if (CellSize <= 8)
    {
        float3 pPlaneWS = BasisTanEast * offMeters.x + BasisTanNorth * offMeters.y;
        float heightAboveCamera = h - Altitude;
        pRelWS = pPlaneWS + BasisRadUp * heightAboveCamera;
    }
    else
    {
        float3 dN = nWS - BasisRadUp;
        pRelWS = dN * PlanetRadius + nWS * h - BasisRadUp * Altitude;
    }

    s.pRelWS = pRelWS;
    return s;
}

float3 ComputeVertexNormalWS(float2 offMeters, float edgeW)
{
    float step = (float) CellSize;

    RelSample c = SampleRelSurface(offMeters, edgeW);
    RelSample xp = SampleRelSurface(offMeters + float2(step, 0), edgeW);
    RelSample xm = SampleRelSurface(offMeters - float2(step, 0), edgeW);
    RelSample yp = SampleRelSurface(offMeters + float2(0, step), edgeW);
    RelSample ym = SampleRelSurface(offMeters - float2(0, step), edgeW);

    float3 dPx = xp.pRelWS - xm.pRelWS;
    float3 dPy = yp.pRelWS - ym.pRelWS;

    float3 N = normalize(cross(dPy, dPx));

    // Outward consistency check using the radial direction at center
    if (dot(N, c.nSphereWS) < 0.0f)
        N = -N;

    return N;
}
PlanetPointVS CalulatePlanetPosVS(int2 gWorld)
{
    PlanetPointVS p;
    
    // 1. Local tangent-plane coordinates in meters
    //    (gWorld is already small: around [-128,128] in your setup)
    float2 off = (float2) gWorld * (float) CellSize;

    // 2. Build a direction on the reference sphere (still using your old logic)
    //    We keep this so height sampling behaves identically.
    float3 pSphereLocal = BasisRadUp + BasisTanEast * (off.x / PlanetRadius) + BasisTanNorth * (off.y / PlanetRadius);

    // Direction from planet center in world space
    float3 nWS = normalize(pSphereLocal);

    // Convert world-space normal to planet-local coordinates for cubemap sampling
    float3 vPlanet;
    vPlanet.x = dot(nWS, BasisLonEast); // "east" axis of planet
    vPlanet.y = dot(nWS, BasisSpinUp); // spin axis
    vPlanet.z = dot(nWS, BasisLonNorth); // "north" axis
    
    float3 pNoise = vPlanet * PlanetRadius;

    float h = SampleHeightFromDir(normalize(vPlanet)); // height in meters
    
    int lodFine = LodFromCellSize(CellSize);
    int lodCoarse = lodFine + 1;
    
    float detailFine = AccumulateHeightDetails(pNoise, lodFine);
    float detailCoarse = AccumulateHeightDetails(pNoise, lodCoarse);
    
    float detail = detailFine;
    
    float edgeW = 1.0f;
    if (DrawMode == 1)
    {
        int2 gLocalI = int2(gWorld) - int2(OriginX, OriginY);
        uint2 gLocal = (uint2) gLocalI;

        edgeW = EdgeBlendWeight(gLocal, GridSize - 1);
        
        // Outer edge (w=0): coarse. Inner edge (w=1): fine.
        detail = lerp(detailCoarse, detailFine, edgeW);

        // Optional: force the very outer border to be exactly coarse
        // (helps if any numerical jitter exists)
        if (EdgeDistanceToBorder(gLocal, GridSize - 1) == 0) 
            detail = detailCoarse;
    }
    
    h += detail;

    // 3. Choose geometry model:
    //    - For L0–L3 (CellSize <= 8): use tangent-plane around the camera.
    //    - For L3+          : use the exact spherical expression.
    float3 pRelWS;
    
    if (CellSize <= 8)   // L0=1, L1=2, L2=4, L3=8  → tangent-plane
    {
        // Tangent-plane offset in world space (meters)
        float3 pPlaneWS = BasisTanEast * off.x + BasisTanNorth * off.y;

        // Camera is at radius + Altitude along BasisRadUp.
        // So the vertical difference between surface and camera is:
        float heightAboveCamera = h - Altitude;

        // Final camera-relative position:
        //   pRelWS = (horizontal offset on tangent plane)
        //          + (vertical offset along radial up)
        pRelWS = pPlaneWS + BasisRadUp * heightAboveCamera;
    }
    else
    {
        // Original exact spherical expression you had:
        //
        // Surface point: pWS   = nWS * (PlanetRadius + h)
        // Camera:        camWS = BasisRadUp * (PlanetRadius + Altitude)
        //
        // pRel = pWS - camWS
        //      = (nWS - BasisRadUp) * PlanetRadius + nWS*h - BasisRadUp*Altitude

        float3 dN = nWS - BasisRadUp;
        pRelWS = dN * PlanetRadius + nWS * h - BasisRadUp * Altitude;
    }

    // 4. View-space position
    float3 posVS = mul(float4(pRelWS, 1.0f), viewMatrix).xyz;

    p.posVS = posVS;
    p.nWS = ComputeVertexNormalWS(off, edgeW);
    return p;
}

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    // unpack & scroll to **planet metres** on the tangent plane
    int2 gWorld = int2(OriginX, OriginY) + int2(input.grid);

    PlanetPointVS C = CalulatePlanetPosVS(gWorld);
    
    output.pixelPosition = mul(float4(C.posVS, 1.0f), projectionMatrix);
    output.viewPosition = C.posVS;
    output.normalSphereWS = C.nWS;

    return output;
}

#type pixel
#pragma pack_matrix( row_major )

static const float PI = 3.14159265358979323846f;
static const float INV_TWO_PI = 1.0f / (2.0f * PI);
static const float INV_PI = 1.0f / PI;

struct PixelInputType
{
    float4 pixelPosition    : SV_POSITION;
    float3 viewPosition     : VIEWPOS;
    float3 normalSphereWS   : NORMAL0;
};

struct PixelOutputType
{
    float4 position         : SV_Target0;
    float4 normal           : SV_Target1;
    float4 albedoMetallic   : SV_Target2;
    float4 roughnessAO      : SV_Target3;
    int entityID            : SV_Target4;
};

cbuffer Camera : register(b0)
{
    matrix worldTranslationMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix inverseViewMatrix;
    matrix inverseProjectionMatrix;
    float4 cameraPosition;
    float far;
    float near;
    float viewportWidth;
    float viewportHeight;
};

cbuffer Material : register(b2)
{
    float4 Albedo;
    float Emission;
    float Metalness;
    float Roughness;
    int AlbedoTexToggle;
    int NormalTexToggle;
    int MetalRoughTexToggle;
};

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
    float3 BasisSpinUp;
};

cbuffer PlanetLevel : register(b7)
{
    int OriginX;
    int OriginY;
    int CellSize;
    int GridSize;
    
    int DrawMode;
    int ScatterOriginX;
    int ScatterOriginY;
};

cbuffer HeightDetail : register(b8)
{
    int Perm[256]; // 1024 bytes
    
    int LODActivation;
    int Octaves;
    float Frequency;
    float Amplitude;
};

struct PBRParameters
{
    float3 Albedo;
    float Metalness;
    float Roughness;
    float AO;
};

PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;
    PBRParameters params;
    
    /*--------------------------------------------------------------*/
    /* 1) position + normal                                         */
    /*--------------------------------------------------------------*/
    float3 nVS = normalize(mul(input.normalSphereWS, (float3x3) viewMatrix));
    
    output.position = float4(input.viewPosition, 1.0f);
    output.normal = float4(nVS * 0.5f + 0.5f, 1.0f);

    /*--------------------------------------------------------------*/
    /* 2) albedo + metallic                                         */
    /*--------------------------------------------------------------*/
    
    params.Albedo = Albedo.rgb; /* later:   if(AlbedoTexToggle) … */
    
    output.albedoMetallic.rgb = params.Albedo;
    output.albedoMetallic.a = 1.0f;//    Metalness;

    /*--------------------------------------------------------------*/
    /* 3) roughness + ambient occlusion                             */
    /*--------------------------------------------------------------*/
    output.roughnessAO = float4(Roughness, 0.0f, 0.0, 1.0);

    output.entityID = 0;
   
    return output;
}