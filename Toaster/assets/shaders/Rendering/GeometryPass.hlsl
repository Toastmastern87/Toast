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
};

// Per-terrain-object-layer settings (bind once per layer)
cbuffer TerrainObject : register(b13)
{
    uint TOSeed;
    int TOLODActivation;
    uint TOInstancesPerLevel; // how many instances to draw for THIS level draw
    float TOMinScale;
    
    float TOMaxScale;
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

struct CubeSample
{
    uint face;
    float2 uv; // [0,1]
};

struct InstSurfaceSample
{
    float3 pRelWS; // camera-relative world space position of the surface
    float3 nSphereWS; // reference-sphere radial direction (good enough as a normal for now)
    float h; // height for debug / later
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

// If uv is outside [0,1], wrap to the correct neighbor face via direction space.
CubeSample RemapFaceUV(uint face, float2 uv)
{
    if (all(uv >= 0.0f) && all(uv <= 1.0f))
    {
        CubeSample cs;
        cs.face = face;
        cs.uv = uv;
        return cs;
    }
    float3 dir = CubeFaceUVToDir(face, uv);
    return DirectionToCube(dir);
}

// Manual bilinear sampler that crosses cube-face edges using Load().
float SampleCubeBilinearLoad(float3 dir, uint2 dims, uint mip)
{
    CubeSample cs = DirectionToCube(dir);
    uint face = cs.face;
    float2 uv = cs.uv;

    float2 p = uv * dims - 0.5f;
    float2 fxy = frac(p);
    int2 i0 = int2(floor(p));
    int2 i1 = i0 + 1;

    float2 uv00 = (float2(i0) + 0.5f) / dims;
    float2 uv10 = (float2(i1.x, i0.y) + 0.5f) / dims;
    float2 uv01 = (float2(i0.x, i1.y) + 0.5f) / dims;
    float2 uv11 = (float2(i1) + 0.5f) / dims;

    CubeSample c00 = RemapFaceUV(face, uv00);
    CubeSample c10 = RemapFaceUV(face, uv10);
    CubeSample c01 = RemapFaceUV(face, uv01);
    CubeSample c11 = RemapFaceUV(face, uv11);

    int2 wh = int2(dims);
    int2 ij00 = clamp(int2(c00.uv * wh), int2(0, 0), wh - 1);
    int2 ij10 = clamp(int2(c10.uv * wh), int2(0, 0), wh - 1);
    int2 ij01 = clamp(int2(c01.uv * wh), int2(0, 0), wh - 1);
    int2 ij11 = clamp(int2(c11.uv * wh), int2(0, 0), wh - 1);

    float v00 = HeightCubeArray.Load(int4(ij00, c00.face, mip));
    float v10 = HeightCubeArray.Load(int4(ij10, c10.face, mip));
    float v01 = HeightCubeArray.Load(int4(ij01, c01.face, mip));
    float v11 = HeightCubeArray.Load(int4(ij11, c11.face, mip));

    float vx0 = lerp(v00, v10, fxy.x);
    float vx1 = lerp(v01, v11, fxy.x);
    return lerp(vx0, vx1, fxy.y);
}

uint2 ComputeLocalGridCoord(float2 offMeters)
{
    // offMeters = gWorld * CellSize  => gWorld ≈ offMeters / CellSize
    // Use round so we land on the nearest grid line for stable banding
    int2 gWorld = (int2) round(offMeters / (float) CellSize);

    int2 gLocalI = gWorld - int2(OriginX, OriginY);

    uint cells = (uint) (GridSize - 1);
    int2 clamped = clamp(gLocalI, int2(0, 0), int2((int) cells, (int) cells));
    return (uint2) clamped;
}

/*──────────────────────── Use it in vertex sampling ───────────────────────────────*/

float SampleHeightFromDir(float3 dirPlanet)
{
    // Mip 0; if you use mips, compute dims for that mip.
    uint W, H, L;
    HeightCubeArray.GetDimensions(W, H, L);
    return SampleCubeBilinearLoad(normalize(dirPlanet), uint2(W, H), /*mip*/0);
}

uint EdgeDistanceToBorder(uint2 gLocal, uint cells)
{
    uint dx = min(gLocal.x, cells - gLocal.x);
    uint dy = min(gLocal.y, cells - gLocal.y);
    return min(dx, dy); // 0 on outer border, 1..EDGE_CELLS inward
}

float EdgeBlendWeight(uint2 gLocal, uint cells)
{
    // 0 -> coarse, 1 -> fine
    uint d = EdgeDistanceToBorder(gLocal, cells);

    // We only care inside the band [0..EDGE_CELLS]
    float t = saturate((float) d / (float) EDGE_CELLS);

    // smoother transition (optional but recommended)
    return t * t * (3.0f - 2.0f * t); // smoothstep(0,1,t)
}

int LodFromCellSize(int cellSize)
{
    // cellSize: 1,2,4,8,... (must be power of two)
    int lod = 0;
    int v = cellSize;
    while (v > 1)
    {
        v >>= 1;
        lod++;
    }
    
    return lod;
}

float AccumulateHeightDetails(float3 offMeters, int lod)
{
    float sum = 0.0f;

    [loop]
    for (int i = 0; i < NumHeightDetails; ++i)
    {
        DetailSettings d = Details[i];
        if (lod <= d.LODActivation)
        {
            sum += FractalPerlin3D(d.PermBase, offMeters, d.Octaves, d.Frequency, d.Amplitude);
        }
    }
    return sum;
}

float EvaluateTerrainHeightMeters(float2 offMeters, int lod /*unused but keep signature*/)
{
    // 1) Reference-sphere direction (same as planet VS)
    float3 pSphereLocal =
        BasisRadUp +
        BasisTanEast * (offMeters.x / PlanetRadius) +
        BasisTanNorth * (offMeters.y / PlanetRadius);

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
        // Decide which LOD we are currently drawing for this instanced pass.
        // You are dispatching draws per level on the CPU, so use that level’s CellSize/GridSize.
        // If you need the numeric lod index, derive it from CellSize:
        int lod = 0;
        int cs = CellSize;
        while (cs > 1)
        {
            cs >>= 1;
            lod++;
        } // CellSize = 2^lod

        // Deterministic RNG per instance & level
        uint base = Hash_u32(TOSeed ^ instanceID ^ (uint) (lod * 0x9e3779b9u));
        float2 r2 = Hash02(base);
        float3 r3 = Hash03(base ^ 0x68bc21ebu);

        // Patch half-extent in meters for this level’s grid
        float cells = (float) (GridSize - 1);
        float halfExtent = 0.5f * cells * (float) CellSize;

        // Random point in tangent plane
        float2 offMeters = (r2 * 2.0f - 1.0f) * halfExtent;

        // Evaluate terrain height at this point (meters above reference radius)
        float h = EvaluateTerrainHeightMeters(offMeters, lod);

        // Build base position in camera-relative space (same as your terrain tangent-plane path)
        float3 pPlaneCR = BasisTanEast * offMeters.x + BasisTanNorth * offMeters.y;
        float heightAboveCamera = h - Altitude;
        float3 baseCR = pPlaneCR + BasisRadUp * heightAboveCamera;

        // Optional slope alignment normal
        // TODO
       // float3 surfNCR = ComputeSurfaceNormalCR(offMeters, lod);

        // Random uniform scale
        float scale = lerp(TOMinScale, TOMaxScale, r3.x);

        // Random rotation (use trig here; acceptable)
        float3 rotationAngles = float3(r3.x, r3.y, r3.z) * 6.2831853f;
        float4x4 rotM = CreateRotationMatrix(rotationAngles);

        // Apply local mesh transform (scale + rotation)
        float3 localPos = input.position * scale;
        float3 localN = input.normal;
        float3 localT = input.tangent.xyz;

        float3 rotatedPos = mul(float4(localPos, 1.0f), rotM).xyz;
        float3 rotatedN = mul(localN, (float3x3) rotM);
        float3 rotatedT = mul(localT, (float3x3) rotM);

        // Align to slope: rotate from +Z (or +Y) to surf normal.
        // Choose your mesh “up axis” here. If your stone meshes are authored with +Y as up:
        float3 meshUp = float3(0, 1, 0);

        // Build an orthonormal basis from surf normal for alignment
        // This creates a frame where 'up' = surfNCR. We then express rotatedPos in that frame.
        float3 up = float3(0, 1, 0);
        float3 east = normalize(cross(BasisSpinUp, up));
        if (all(abs(east) < 1e-6))
            east = normalize(cross(float3(1, 0, 0), up));
        float3 north = normalize(cross(up, east));

        float3x3 alignM = float3x3(east, up, north); // columns

        // If mesh up is +Y, this works when we treat rotatedPos.y as "up".
        // So interpret rotatedPos in mesh local axes (x,right; y,up; z,forward) and map to world:
        float3 alignedPos = alignM[0] * rotatedPos.x + alignM[1] * rotatedPos.y + alignM[2] * rotatedPos.z;
        float3 alignedN = normalize(alignM[0] * rotatedN.x + alignM[1] * rotatedN.y + alignM[2] * rotatedN.z);
        float3 alignedT = normalize(alignM[0] * rotatedT.x + alignM[1] * rotatedT.y + alignM[2] * rotatedT.z);

        // Final camera-relative position
        float3 pCR = baseCR + alignedPos;

        // IMPORTANT: pCR is already camera-relative. Do NOT apply worldTranslationMatrix here.
        worldPosition = float4(pCR, 1.0f);
        worldNormal = alignedN;
        worldTangent = alignedT;
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

Texture2D AlbedoTexture : register(t3);
Texture2D NormalTexture : register(t4);
Texture2D MetalRoughTexture : register(t5);

SamplerState defaultSampler : register(s0);

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
	
    // Sample input textures to get shading model params.
    params.Albedo = AlbedoTexToggle > 0 ? AlbedoTexture.Sample(defaultSampler, input.texCoord).rgb : Albedo.rgb;
    params.Metalness = MetalRoughTexToggle > 0 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).b : Metalness;
    params.Roughness = MetalRoughTexToggle > 0 ? MetalRoughTexture.Sample(defaultSampler, input.texCoord).r : Roughness;
    params.Roughness = max(params.Roughness, 0.05f); // Minimum roughness of 0.05 to keep specular highlight
    
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