#inputlayout
vertex
vertex
vertex
vertex
vertex

#type vertex
#pragma pack_matrix( row_major )

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

cbuffer Model : register(b1)
{
    matrix worldMatrix;
    float clickable;
    int entityID;
    int noWorldTransform;
    int isInstanced;
};

cbuffer IcospherePlanet : register(b2)
{
    float planetRadius;
    float3 camHiPS;
	
    matrix viewMatrixPlanetRendering; // This includes floating origin translation for planet rendering
	
    int patchLevels;
    float3 camLoPS;
	
    float3 planetCenterRelHiWS;
    int materialCount;

    float3 planetCenterRelLoWS;
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
    float _padPF0;
    float3 BasisSpinUp;
    float _padPF1;
};

cbuffer PlanetRenderingSettings : register(b5)
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

cbuffer PlanetLevel : register(b7)
{
    int OriginX;
    int OriginY;
    int CellSize;
    int GridSize;
    
    int DrawMode;
    float ScatterOriginMetersX;
    float ScatterOriginMetersY;
    float FinestCellSize;
};

// Per-terrain-object-layer settings (bind once per layer)
//cbuffer TerrainObject : register(b13)
//{
//    uint TOSeed;
//    int TOLODActivation;
//    uint TOInstancesPerLevel; // how many instances to draw for THIS level draw
//    float TOMinScale;
    
//    float TOMaxScale;
//    float TOScatterCellSize;
//    uint TOScatterCells;
//};

cbuffer TerrainObject : register(b13)
{
    // 16 bytes
    uint TOSeed;
    int TOLODActivation;
    float TOMinScale;
    float TOMaxScale;

    // 16 bytes
    float TOScatterRadiusMeters;
    uint TOCandidateGridSize;
    float TODensityProb;
    float TOPlayerTangentEast;

    // 16 bytes
    float TOPlayerTangentNorth;
    float3 pad2;
};

struct VertexInputType
{
    float3 position                 : POSITION0;
    float3 normal                   : NORMAL;
    float4 tangent                  : TANGENT;
    float2 texCoord                 : TEXCOORD; 
    float3 color                    : COLOR0;
};

struct PixelInputType
{
    float4 pixelPosition    : SV_POSITION;
    float3 viewPosition     : VIEWPOS;
    float3 viewNormal       : NORMAL;
    float2 texCoord         : TEXCOORD;
    float3x3 TBN            : TBASIS;
    int entityID            : TEXTUREID; 
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

Texture2DArray<float> HeightCubeArray           : register(t0);
StructuredBuffer<MaterialData> Materials        : register(t1);
StructuredBuffer<NoiseLayerData> NoiseLayers    : register(t2);
StructuredBuffer<int4> PermTables               : register(t3);
Texture2DArray<float4> AlbedoCubeArray          : register(t4);
    
StructuredBuffer<DetailSettings> Details        : register(t8);
StructuredBuffer<int4> PermTablesDetails        : register(t9);

SamplerState HeightMapSampler                   : register(s5);
SamplerState UWrapVClampLinearSampler           : register(s6);

static const uint EDGE_CELLS = 12;

#define MAX_MATERIALS 8

#include "DirectionToCube.hlsli"
#include "PerlinNoise.hlsli"
#include "PlanetTerrainHelpers.hlsli"

struct InstSurfaceSample
{
    float3 pRelWS; // camera-relative world space position of the surface
    float3 nSphereWS; // reference-sphere radial direction (good enough as a normal for now)
    float h; // height for debug / later
};

uint2 ComputeLocalGridCoordFromOff(float2 offMeters)
{
    float cells = (float) (GridSize - 1);
    float halfExtent = 0.5f * cells * (float) CellSize;

    // Map meters -> [0..cells] in float
    float2 g = (offMeters + halfExtent) / (float) CellSize;

    // Clamp and convert
    g = clamp(g, 0.0f, cells);
    return (uint2) g;
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
    int currentLOD = 0; // not used by terrain objects, planet uses it for noise layer LOD activation
    uint matCount = MaterialCount; // however you pass it — likely a cbuffer field
    float3 baseNormal = ComputeBaseNormalPS(dir);
    float slope = 1.0 - saturate(dot(normalize(baseNormal), dir));
    float colorAvg = SampleColorAvg(dir);
    float3 normalPS = nWS; // not actually used inside SampleTerrainHeight per the code above

    // 4) Out parameters we don't care about (debug-only)
    float wallDebug, wallMaskDebug, erosionMaskDebug, erosionPatternDebug, erosionDeltaDebug;

    return SampleTerrainHeight(dir, worldPos, currentLOD, matCount, slope, colorAvg, normalPS, wallDebug, wallMaskDebug, erosionMaskDebug, erosionPatternDebug, erosionDeltaDebug);
}

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

void BuildONB(float3 n, out float3 b1, out float3 b2)
{
    // Orthonormal basis around n
    float3 up = (abs(n.y) < 0.999f) ? float3(0, 1, 0) : float3(1, 0, 0);
    b1 = normalize(cross(up, n));
    b2 = cross(n, b1);
}

float3 SampleDirInCap(float3 centerDir, float alphaMax, float2 u)
{
    // u in [0,1]^2, uniform on spherical cap
    // cos(theta) in [cos(alphaMax), 1]
    float cosMin = cos(alphaMax);
    float cosT = lerp(cosMin, 1.0f, u.x);
    float sinT = sqrt(saturate(1.0f - cosT * cosT));
    float phi = u.y * 6.2831853f;

    float3 b1, b2;
    BuildONB(centerDir, b1, b2);

    return normalize(centerDir * cosT + (b1 * cos(phi) + b2 * sin(phi)) * sinT);
}

float4x4 CreateRotationMatrix(float3 rotationAngles)
{
    // Rotation matrix around the X axis
    float cosX = cos(rotationAngles.x);
    float sinX = sin(rotationAngles.x);
    float4x4 rotationX = float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, cosX, -sinX, 0.0f,
        0.0f, sinX, cosX, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // Rotation matrix around the Y axis
    float cosY = cos(rotationAngles.y);
    float sinY = sin(rotationAngles.y);
    float4x4 rotationY = float4x4(
        cosY, 0.0f, sinY, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sinY, 0.0f, cosY, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // Rotation matrix around the Z axis
    float cosZ = cos(rotationAngles.z);
    float sinZ = sin(rotationAngles.z);
    float4x4 rotationZ = float4x4(
        cosZ, -sinZ, 0.0f, 0.0f,
        sinZ, cosZ, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // Combine all rotations
    return mul(mul(rotationX, rotationY), rotationZ);
}

PixelInputType main(VertexInputType input, uint instanceID : SV_InstanceID)
{
    PixelInputType output;

    float4 worldPosition;
    float3 worldNormal;
    float3 worldTangent;
    
    // CURRENTLY THIS WILL ONLY RENDER TERRAIN OBJECTS!
    if (isInstanced)
    {       
        uint M = TOCandidateGridSize;
        uint candidateCount = M * M;

        if (instanceID >= candidateCount)
        {
            output.pixelPosition = float4(2, 2, 2, 1);
            return output;
        }

        // Candidate index → 2D position in candidate grid
        uint ix = instanceID % M;
        uint iy = instanceID / M;

        // Each cell's size in tangent meters
        float candidateCellSize = (2.0f * TOScatterRadiusMeters) / (float) M;

        // Local offset in tangent meters (centered on player)
        float2 offMeters = (float2((float) ix + 0.5f, (float) iy + 0.5f) * candidateCellSize)
                     - float2(TOScatterRadiusMeters, TOScatterRadiusMeters);

        // Convert to world-stable position for hashing
        float2 globalMeters = float2(TOPlayerTangentEast, TOPlayerTangentNorth) + offMeters;

        // World cell ID — stable as the player moves
        int worldCX = (int) floor(globalMeters.x / candidateCellSize);
        int worldCY = (int) floor(globalMeters.y / candidateCellSize);

        // Stable world-space hash key
        uint key = Hash_u32(TOSeed
              ^ Hash_u32((uint) worldCX)
              ^ (Hash_u32((uint) worldCY) * 0x85ebca6bu));

        // Density-based cull
        if (Hash01(key) > TODensityProb)
        {
            output.pixelPosition = float4(2, 2, 2, 1);
            return output;
        }

        // Stable randoms for this stone
        float2 r2 = Hash02(key);
        float3 r3 = Hash03(key ^ 0x68bc21ebu);

        // Jitter within the world cell (stable)
        float2 jitteredGlobalMeters = (float2((float) worldCX, (float) worldCY) + r2) * candidateCellSize;
        offMeters = jitteredGlobalMeters - float2(TOPlayerTangentEast, TOPlayerTangentNorth);

        // Cull beyond scatter radius (after jitter could push slightly out)
        float distFromCenter = length(offMeters);
        if (distFromCenter > TOScatterRadiusMeters)
        {
            output.pixelPosition = float4(2, 2, 2, 1);
            return output;
        }

        // Sample terrain height at this offset
        float h = EvaluateTerrainHeightMeters(offMeters);

        // Place on tangent plane
        float3 pPlaneCR = BasisTanEast * offMeters.x + BasisTanNorth * offMeters.y;
        float3 baseCR = pPlaneCR + BasisRadUp * (h - Altitude);

        // Per-instance scale and rotation
        float scale = lerp(TOMinScale, TOMaxScale, r3.x);
        float3 rotationAngles = r3 * 6.2831853f;
        float4x4 rotM = CreateRotationMatrix(rotationAngles);

        float3 localPos = input.position * scale;
        float3 localN = input.normal;
        float3 localT = input.tangent.xyz;

        float3 rotatedPos = mul(float4(localPos, 1.0f), rotM).xyz;
        float3 rotatedN = mul(localN, (float3x3) rotM);
        float3 rotatedT = mul(localT, (float3x3) rotM);

        float3 pCR = baseCR + rotatedPos;

        // Output
        float4 worldPosition = float4(pCR, 1.0f);
        float4 viewPosition = mul(worldPosition, viewMatrix);
        output.pixelPosition = mul(viewPosition, projectionMatrix);
        output.viewPosition = viewPosition.xyz;

        worldNormal = rotatedN;
        worldTangent = float4(rotatedT, input.tangent.w);

        float3 viewNormal = normalize(mul(worldNormal, (float3x3) viewMatrix));
        float3 viewTangent = normalize(mul(worldTangent.xyz, (float3x3) viewMatrix));
        float3 viewBitangent = cross(viewNormal, viewTangent) * input.tangent.w;

        float3x3 TBN = float3x3(viewTangent, viewBitangent, viewNormal);

        output.TBN = TBN;
        output.viewNormal = viewNormal;
        output.texCoord = input.texCoord;
        output.entityID = -1;

        return output;
    }
    else
    {
        if (noWorldTransform == 1)
        {
            worldPosition = float4(input.position, 1.0f);
            worldPosition = mul(worldPosition, worldTranslationMatrix);
            worldNormal = input.normal;
            worldTangent = input.tangent;
        }
        else
        {
            worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
            worldPosition = mul(worldPosition, worldTranslationMatrix);
            worldNormal = mul(input.normal, (float3x3) worldMatrix);
            worldTangent = mul(input.tangent.xyz, (float3x3) worldMatrix);
        }
    }

    float4 viewPosition = mul(worldPosition, viewMatrix);
    output.pixelPosition = mul(viewPosition, projectionMatrix);
    output.viewPosition = viewPosition.xyz;

    float3 viewNormal = normalize(mul(worldNormal, (float3x3) viewMatrix));
    float3 viewTangent = normalize(mul(worldTangent, (float3x3) viewMatrix));
    
    float3 viewBitangent = cross(viewNormal, viewTangent) * input.tangent.w;
    
    float3x3 TBN = float3x3(viewTangent, viewBitangent, viewNormal);
    
    output.TBN = TBN;
    output.viewNormal = viewNormal;

    output.texCoord = input.texCoord;

    if (clickable > 0)
        output.entityID = entityID;
    else
        output.entityID = -1;

    return output;
}

#type pixel
#pragma pack_matrix( row_major )

struct PixelInputType
{
    float4 pixelPosition        : SV_POSITION;
    float3 viewPosition         : VIEWPOS;
    float3 viewNormal           : NORMAL;
    float2 texCoord             : TEXCOORD;
    float3x3 TBN                : TBASIS;
    int entityID                : TEXTUREID;
};

struct PixelOutputType
{
    float4 position             : SV_Target0;
    float4 normal               : SV_Target1;
    float4 albedoMetallic       : SV_Target2;
    float4 roughnessAO          : SV_Target3;
    int entityID                : SV_Target4;
    float4 planetMaterialDebug  : SV_Target5;
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

Texture2D AlbedoTexture         : register(t3);
Texture2D NormalTexture         : register(t4);
Texture2D MetalRoughTexture     : register(t5);

SamplerState defaultSampler : register(s0);

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

PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;
    PBRParameters params;
    
    // Sample input textures to get shading model params.
    params.Albedo = AlbedoTexToggle > 0 ? AlbedoTexture.Sample(defaultSampler, input.texCoord).rgb : Albedo.rgb;
    params.Metalness = MetalRoughTexToggle > 0 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).r : Metalness;
    params.Roughness = MetalRoughTexToggle > 0 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).g : Roughness;
    params.Roughness = max(params.Roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight
    
    // TODO MIGHT NEED TO BE FIXED AT A LATER STAGE TO GET CORRECT ALBEDO MAPPING
    if (AlbedoTexToggle > 0)
        params.Albedo = LinearToSRGB(params.Albedo);
          
    // Position
        output.position = float4(input.viewPosition, 1.0f);
	
    // Entity ID
    if (input.entityID > -1)
        output.entityID = input.entityID + 1;
    
    // Handle Normal Mapping
    float3 N;
    
    if (NormalTexToggle > 0)
    {
        // Sample the normal map
        float3 sampledNormal = NormalTexture.Sample(defaultSampler, input.texCoord).rgb;
        
        // Decode the normal from [0,1] to [-1,1]
        sampledNormal = sampledNormal * 2.0f - 1.0f;
        sampledNormal = normalize(sampledNormal);
        
        // Transform the sampled normal to view space
        N = normalize(mul(sampledNormal, input.TBN));
    }
    else
    {
        // Use the default view normal
        N = normalize(input.viewNormal);
    }
    
    // Encode Normal  
    float3 encodedNormal = N * 0.5 + 0.5;

    if (input.entityID > -1)
        output.normal = float4(encodedNormal, 1.0);
    else
        output.normal = float4(encodedNormal, (float)input.entityID);
    
    // Albedo & Metallic
    output.albedoMetallic.rgb = params.Albedo;
    output.albedoMetallic.a = params.Metalness;
    
    // RoughnessAO
    output.roughnessAO = float4(params.Roughness, 0.0f, 0.0f, 1.0f);
    
    return output;
}