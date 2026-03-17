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
    float ScatterOriginMetersX;
    float ScatterOriginMetersY;
};

struct PixelInputType
{
    float4 pixelPosition        : SV_POSITION;
    float3 viewPosition         : VIEWPOS;
    float3 viewNormal           : NORMAL0;
    float2 uv                   : TEXCOORD0;
};

struct PlanetPointVS
{
    float3 posVS; // for SV_POSITION
    float3 nWS; // unit sphere normal in world-space
    float2 uv;
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

    // 3. Geometry model: compute both, then select/blend
    float3 pPlaneWS = BasisTanEast * offMeters.x + BasisTanNorth * offMeters.y;
    float3 pRelWS_plane = pPlaneWS + BasisRadUp * (h - Altitude);

    float3 dN = nWS - BasisRadUp;
    float3 pRelWS_sphere = dN * PlanetRadius + nWS * h - BasisRadUp * Altitude;

    // Choose which model to use
    float modelW = (CellSize <= 8) ? 1.0f : 0.0f;

    // If last LOD with planar model blend towards spherical to avoid popping
    if (DrawMode == 1 && CellSize == 8)
        modelW = edgeW;

    float3 pRelWS = lerp(pRelWS_sphere, pRelWS_plane, modelW);

    s.pRelWS = pRelWS;
    return s;
}

float GetCubemapTexelStepMeters()
{
    uint width, height, layers;
    HeightCubeArray.GetDimensions(width, height, layers);

    // One texel in face UV space [-1,1]
    float du = 2.0f / (float) width;

    // Convert small angular change to arc length
    return du * PlanetRadius;
}

float3 ComputeVertexNormalWS(float2 offMeters, float edgeW)
{
    float step;
    if (CellSize > 8)
        step = GetCubemapTexelStepMeters();
    else
        step = (float) CellSize;
    
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

float2 PlanetDirToEquirectUV(float3 dirPlanet)
{
    dirPlanet = normalize(dirPlanet);
    float lon = atan2(dirPlanet.z, dirPlanet.x); // match the height bake
    float lat = asin(clamp(dirPlanet.y, -1.0f, 1.0f));
    float2 uv;
    uv.x = lon * INV_TWO_PI + 0.5f;
    uv.y = 0.5f - lat * INV_PI;
    return uv;
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

    // 3. Geometry model: compute both, then select/blend
    float3 pPlaneWS = BasisTanEast * off.x + BasisTanNorth * off.y;
    float3 pRelWS_plane = pPlaneWS + BasisRadUp * (h - Altitude);

    float3 dN = nWS - BasisRadUp;
    float3 pRelWS_sphere = dN * PlanetRadius + nWS * h - BasisRadUp * Altitude;

    // Choose which model to use
    float modelW = (CellSize <= 8) ? 1.0f : 0.0f;

    // If last LOD with planar model blend towards spherical to avoid popping
    if (DrawMode == 1 && CellSize == 8)
        modelW = edgeW;

    float3 pRelWS = lerp(pRelWS_sphere, pRelWS_plane, modelW);

    // 4. View-space position
    float3 posVS = mul(float4(pRelWS, 1.0f), viewMatrix).xyz;
    
    p.uv = PlanetDirToEquirectUV(float3(-vPlanet.x, vPlanet.y, -vPlanet.z));
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
    output.viewNormal = normalize(mul(C.nWS, (float3x3) viewMatrix));
    output.uv = C.uv;

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
    float3 viewNormal       : NORMAL0;
    float2 uv               : TEXCOORD0;
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

cbuffer PlanetRenderingSettings : register(b5)
{
    float SlopeSensitivity;
    float SlopeThreshold;
    float SlopeDarkening;
}

cbuffer PlanetLevel : register(b7)
{
    int OriginX;
    int OriginY;
    int CellSize;
    int GridSize;
    
    int DrawMode;
    float ScatterOriginMetersX;
    float ScatterOriginMetersY;
};

cbuffer HeightDetail : register(b8)
{
    int Perm[256]; // 1024 bytes
    
    int LODActivation;
    int Octaves;
    float Frequency;
    float Amplitude;
};

Texture2DArray<float4> NormalCubeArray  : register(t2);
Texture2DArray<float4> AlbedoCubeArray  : register(t3);

SamplerState defaultSampler     : register(s0);

struct PBRParameters
{
    float3 Albedo;
    float Metalness;
    float Roughness;
    float AO;
};

float3 LinearToSRGB(float3 x)
{
    float3 lo = x * 12.92;
    float3 hi = 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;
    return lerp(hi, lo, step(x, 0.0031308));
}

struct CubeSample
{
    uint face;
    float2 uv; // [0,1]
};

float3 CubeFaceUVToDir(uint face, float2 uv)
{
    // Match the bake: flip Y
    float2 p = 2.0 * float2(uv.x, 1.0 - uv.y) - 1.0;
    float px = p.x;
    float py = p.y;

    switch (face)
    {
        case 0:
            return normalize(float3(1.0, py, -px)); // +X
        case 1:
            return normalize(float3(-1.0, py, px)); // -X
        case 2:
            return normalize(float3(px, 1.0, -py)); // +Y
        case 3:
            return normalize(float3(px, -1.0, py)); // -Y
        case 4:
            return normalize(float3(px, py, 1.0)); // +Z
        default:
            return normalize(float3(-px, py, -1.0)); // -Z
    }
}

CubeSample DirectionToCube(float3 v)
{
    v = normalize(v);

    float ax = abs(v.x);
    float ay = abs(v.y);
    float az = abs(v.z);

    uint face;
    float2 uvFace;

    if (ax >= ay && ax >= az)
    {
        if (v.x > 0)
        {
            face = 0;
            uvFace = float2(-v.z, v.y) / ax;
        }
        else
        {
            face = 1;
            uvFace = float2(v.z, v.y) / ax;
        }
    }
    else if (ay >= ax && ay >= az)
    {
        if (v.y > 0)
        {
            face = 2;
            uvFace = float2(v.x, -v.z) / ay;
        }
        else
        {
            face = 3;
            uvFace = float2(v.x, v.z) / ay;
        }
    }
    else
    {
        if (v.z > 0)
        {
            face = 4;
            uvFace = float2(v.x, v.y) / az;
        }
        else
        {
            face = 5;
            uvFace = float2(-v.x, v.y) / az;
        }
    }

    CubeSample cs;
    cs.face = face;
    cs.uv = uvFace * 0.5 + 0.5;
    return cs;
}

PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;
    PBRParameters params;
        
    /*--------------------------------------------------------------*/
    /* 1) position + normal                                         */
    /*--------------------------------------------------------------*/ 
    float3 posWS = mul(float4(input.viewPosition, 1.0f), inverseViewMatrix).xyz;
    float camToSurface = length(cameraPosition.xyz - posWS);
    float cubeNormalStrength = smoothstep(1000.0f, 5000.0f, camToSurface);
    
    float3 N = normalize(input.viewNormal);
    
    // Direction from planet center in world space
    float3 dirWS = normalize(posWS - PlanetCenterCR);
    
    // Convert to planet-local space (same basis as your height cubemap)
    float3 vPlanet;
    vPlanet.x = dot(dirWS, BasisLonEast);
    vPlanet.y = dot(dirWS, BasisSpinUp);
    vPlanet.z = dot(dirWS, BasisLonNorth);
    
    CubeSample cs = DirectionToCube(vPlanet);
    
    float3 cubeN = NormalCubeArray.SampleLevel(defaultSampler, float3(cs.uv, (float) cs.face), 0).rgb * 2.0f - 1.0f;
    
    if (cubeNormalStrength > 0.001f)
    {
        // Sample the normal cubemap (stored as packed [0,1], unpack to [-1,1])

        
        // cubeN is in planet space, convert to world space
        float3 cubeNWS = cubeN.x * BasisLonEast + cubeN.y * BasisSpinUp + cubeN.z * BasisLonNorth;
        
        // Convert to view space
        float3 cubeNVS = normalize(mul(cubeNWS, (float3x3) viewMatrix));
        
        // Blend: orbit uses cubemap, surface uses vertex normals
        N = normalize(lerp(N, cubeNVS, cubeNormalStrength));
    }
    
    output.position = float4(input.viewPosition, 1.0f);
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);

    /*--------------------------------------------------------------*/
    /* 2) albedo + metallic                                         */
    /*--------------------------------------------------------------*/
    params.Albedo = AlbedoTexToggle > 0 ? AlbedoCubeArray.SampleLevel(defaultSampler, float3(cs.uv, (float) cs.face), 0).rgb : Albedo.rgb;
    
    float slope = 1.0f - saturate(dot(cubeN, normalize(vPlanet)));
    slope = saturate(slope * SlopeSensitivity - SlopeThreshold); // only steep slopes survive
    float heightTint = lerp(1.0f, SlopeDarkening, slope);
    params.Albedo *= heightTint;
    
    // TODO MIGHT NEED TO BE FIXED AT A LATER STAGE TO GET CORRECT ALBEDO MAPPING
    if (AlbedoTexToggle > 0)
        params.Albedo = LinearToSRGB(params.Albedo);
 
    output.albedoMetallic.rgb = params.Albedo;
    output.albedoMetallic.a = 1.0f;//    Metalness;

    /*--------------------------------------------------------------*/
    /* 3) roughness + ambient occlusion                             */
    /*--------------------------------------------------------------*/
    output.roughnessAO = float4(Roughness, 0.0f, 0.0, 1.0);

    output.entityID = 0;
   
    return output;
}