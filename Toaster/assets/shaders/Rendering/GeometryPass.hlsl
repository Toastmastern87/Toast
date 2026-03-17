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
cbuffer TerrainObject : register(b13)
{
    uint TOSeed;
    int TOLODActivation;
    uint TOInstancesPerLevel; // how many instances to draw for THIS level draw
    float TOMinScale;
    
    float TOMaxScale;
    float TOScatterCellSize;
    uint TOScatterCells;
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

Texture2DArray<float> HeightCubeArray       : register(t0);
    
StructuredBuffer<DetailSettings> Details    : register(t8);
StructuredBuffer<int4> PermTables           : register(t9);

SamplerState UWrapVClampLinearSampler       : register(s5);

static const uint EDGE_CELLS = 12;

#include "PerlinNoise.hlsli"
#include "TerrainHeightCalculations.hlsli"

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

float2 OffMetersFromDirTangentApprox(float3 nWS)
{
    // Project direction onto the tangent basis
    float e = dot(nWS, BasisTanEast);
    float n = dot(nWS, BasisTanNorth);
    float u = dot(nWS, BasisRadUp);

    // Small-angle tangent-plane approximation
    float invU = rcp(max(u, 1e-4f));
    return PlanetRadius * float2(e, n) * invU;
}

float EvaluateTerrainHeightMeters(float2 offMeters)
{
    // 1) Reference-sphere direction (same as planet VS)
    float3 pSphereLocal = BasisRadUp + BasisTanEast * (offMeters.x / PlanetRadius) + BasisTanNorth * (offMeters.y / PlanetRadius);

    float3 nWS = normalize(pSphereLocal);

    // 2) Convert WS direction to planet-local axes for cubemap sampling
    float3 vPlanet;
    vPlanet.x = dot(nWS, BasisLonEast);
    vPlanet.y = dot(nWS, BasisSpinUp);
    vPlanet.z = dot(nWS, BasisLonNorth);

    float3 pNoise = vPlanet * PlanetRadius;

    // 3) Base height from baked cube
    float h = SampleHeightFromDir(normalize(vPlanet));

    // 4) Add procedural details with LOD logic identical to planet
    int lodFine = LodFromCellSize(CellSize);
    float detailFine = AccumulateHeightDetails(pNoise, lodFine);
    float detail = detailFine;

    // Edge strip draw blends coarse/fine details (same as your planet)
    if (DrawMode == 1)
    {
        int lodCoarse = lodFine + 1;
        float detailCoarse = AccumulateHeightDetails(pNoise, lodCoarse);

        uint2 gLocal = ComputeLocalGridCoord(offMeters);
        uint cells = (uint) (GridSize - 1);

        float edgeW = EdgeBlendWeight(gLocal, cells);

        // Outer edge: coarse. Inner edge: fine.
        detail = lerp(detailCoarse, detailFine, edgeW);

        // Force the very outer border to be exactly coarse (planet does this)
        if (EdgeDistanceToBorder(gLocal, cells) == 0)
            detail = detailCoarse;
    }

    h += detail;
    return h;
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
        // ----- Candidate grid for this draw -----
        uint scells = max(1u, TOScatterCells);
        uint candidateCount = scells * scells;

        // If your DrawIndexedInstanced uses candidateCount, this is always true; still keep as guard.
        if (instanceID >= candidateCount)
        {
            output.pixelPosition = float4(2, 2, 2, 1);
            return output;
        }

        uint ix = instanceID % scells;
        uint iy = instanceID / scells;

        // LOD window size in meters (same region you use for terrain for this level)
        float cells = (float) (GridSize - 1);
        float widthM = cells * (float) CellSize;
        float halfExtent = 0.5f * widthM;

        // Candidate cell size in the window (meters)
        // This is NOT TOScatterCellSize; this is just how we sample the window uniformly.
        float candidateCellSize = widthM / (float) scells;

        // Local candidate center in offMeters convention centered at (0,0)
        float2 offMeters = (float2((float) ix + 0.5f, (float) iy + 0.5f) * candidateCellSize) - float2(halfExtent, halfExtent);
        
        float r = max(abs(offMeters.x), abs(offMeters.y));

        if (r > halfExtent)
        {
            output.pixelPosition = float4(2, 2, 2, 1);
            return output;
        }

        float halfWFiner = 0.0f;
        if (CellSize > FinestCellSize) // i.e., L > L0
            halfWFiner = 0.5f * (cells * (0.5f * (float) CellSize));
        
        if (halfWFiner > 0.0f && r < halfWFiner)
        {
            output.pixelPosition = float4(2, 2, 2, 1);
            return output;
        }

        // Convert to world-stable tangent-plane meters
        float2 globalMeters = float2(ScatterOriginMetersX, ScatterOriginMetersY) + offMeters;

        // Quantize to a WORLD scatter cell id (this anchors identity in world space)
        float scatterSize = max(1e-3f, TOScatterCellSize);
        int worldCX = (int) floor(globalMeters.x / scatterSize);
        int worldCY = (int) floor(globalMeters.y / scatterSize);

        // LOD index (same as your code)
        int lod = 0;
        int cs = CellSize;
        while (cs > 1)
        {
            cs >>= 1;
            lod++;
        }

        // Stable world key: DO NOT include camera/window origin
        uint key = Hash_u32(TOSeed
                  ^ Hash_u32((uint) worldCX)
                  ^ (Hash_u32((uint) worldCY) * 0x85ebca6bu)
                  ^ (uint) (lod * 0x9e3779b9u));

        // Density: expected keep fraction ~= desiredCount / candidateCount
        float keepProb = saturate((float) TOInstancesPerLevel / (float) candidateCount);

        // Cull most candidates
        if (Hash01(key) > keepProb)
        {
            output.pixelPosition = float4(2, 2, 2, 1);
            return output;
        }

        // Stable randoms for this world cell
        float2 r2 = Hash02(key);
        float3 r3 = Hash03(key ^ 0x68bc21ebu);

        // Jitter within the WORLD scatter cell (stable)
        float2 jitter = (r2 - 0.5f) * 0.9f * scatterSize;

        // Place at jittered world position, then convert back to local offMeters for your height eval
        float2 jitteredGlobalMeters = (float2((float) worldCX, (float) worldCY) + r2) * scatterSize;
        offMeters = jitteredGlobalMeters - float2(ScatterOriginMetersX, ScatterOriginMetersY);

        // ----- Terrain height + camera-relative base position (your existing path) -----
        float h = EvaluateTerrainHeightMeters(offMeters);

        float3 pPlaneCR = BasisTanEast * offMeters.x + BasisTanNorth * offMeters.y;
        float3 baseCR = pPlaneCR + BasisRadUp * (h - Altitude);

        // ----- Per-instance scale/rotation (your existing path) -----
        float scale = lerp(TOMinScale, TOMaxScale, r3.x);

        float3 rotationAngles = r3 * 6.2831853f;
        float4x4 rotM = CreateRotationMatrix(rotationAngles);

        float3 localPos = input.position * scale;
        float3 localN = input.normal;
        float3 localT = input.tangent.xyz;

        float3 rotatedPos = mul(float4(localPos, 1.0f), rotM).xyz;
        float3 rotatedN = mul(localN, (float3x3) rotM);
        float3 rotatedT = mul(localT, (float3x3) rotM);

        // (Optional) slope alignment: only do this if you compute a real surface normal at offMeters.
        // Otherwise you will introduce artifacts. For now, skip.

        float3 pCR = baseCR + rotatedPos;

        // ----- Output -----
        float4 worldPosition = float4(pCR, 1.0f);

        float4 viewPosition = mul(worldPosition, viewMatrix);
        output.pixelPosition = mul(viewPosition, projectionMatrix);
        output.viewPosition = viewPosition.xyz;
        
        worldNormal = rotatedN;
        worldTangent = float4(rotatedT, input.tangent.w);

        float3 viewNormal = normalize(mul(worldNormal, (float3x3) viewMatrix));
        float3 viewTangent = normalize(mul(worldTangent, (float3x3) viewMatrix));
    
        float3 viewBitangent = cross(viewNormal, viewTangent) * input.tangent.w;
    
        float3x3 TBN = float3x3(viewTangent, viewBitangent, viewNormal);
    
        output.TBN = TBN;
        output.viewNormal = viewNormal;

        output.texCoord = input.texCoord;
        output.entityID = -1;

        // Fill any remaining outputs (uv, material ids, etc.) as your shader requires.
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
    float4 pixelPosition    : SV_POSITION;
    float3 viewPosition     : VIEWPOS;
    float3 viewNormal       : NORMAL;
    float2 texCoord         : TEXCOORD;
    float3x3 TBN            : TBASIS;
    int entityID            : TEXTUREID;
};

struct PixelOutputType
{
    float4 position         : SV_Target0;
    float4 normal           : SV_Target1;
    float4 albedoMetallic   : SV_Target2;
    float4 roughnessAO      : SV_Target3;
    int entityID            : SV_Target4;
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