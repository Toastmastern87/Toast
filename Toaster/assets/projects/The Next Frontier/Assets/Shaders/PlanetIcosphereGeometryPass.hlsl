#inputlayout
vertex
instance
instance
instance
instance
instance
instance
instance
instance

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
    matrix worldMatrix; // planet rotation + translation (translation should already be floating-origin safe)
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

struct VertexInputType
{
    uint2 ij                : TEXCOORD0; // per-vertex
    int level               : TEXTUREID0; // per-instance

    float3 V0               : POSITION0;
    float3 V1               : POSITION1;
    float3 V2               : POSITION2;

    float3 P2RelHi          : TEXCOORD3;
    float3 P0RelHi          : TEXCOORD1;
    float3 P1RelHi          : TEXCOORD2;
    
    float3 PatchOriginPS    : TEXCOORD4;
};

struct PixelInputType
{
	float4 pixelPosition	    : SV_POSITION;
	float3 viewPosition		    : VIEWPOS;
	float3 normalWS	            : NORMAL0;
    float3 dirPS                : TEXCOORD0; // planet-space unit direction
    float3 worldPosPS           : TEXCOORD1;
    float3 TriplanarPos         : TEXCOORD2;
    float WallDebug             : TEXCOORD3;
    float WallMaskDebug         : TEXCOORD4;
    float ErosionMaskDebug      : TEXCOORD5;
    float ErosionPatternDebug   : TEXCOORD6;
    float ErosionDeltaDebug     : TEXCOORD7;
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

SamplerState HeightMapSampler                   : register(s5);

#include "DirectionToCube.hlsli"
#include "PerlinNoise.hlsli"
#include "PlanetTerrainHelpers.hlsli"

float SampleNormalHeight(float3 dir)
{
    float h = SampleHeightMetres(dir);

    float wallMask = 0.0;
    float wallBoost = ComputeWallSteepenBoost(dir, h, wallMask);
    h += wallBoost;

#if TERRAIN_NORMAL_INCLUDES_EROSION

    float erosionMask = 0.0;
    float erosionPattern = 0.0;

    float erosionDelta = ComputeRuneStyleErosion(dir, h, erosionMask, erosionPattern);

    h += erosionDelta;

#endif

    return h;
}

float3 ComputeTerrainNormalPS(float3 dir, int currentLOD, uint matCount, float slope, float colorAvg, float3 baseNormal)
{
    float3 tanU, tanV;
    BuildSphereTangents(dir, tanU, tanV);

    float normalStepMeters = max(TerrainNormalStepMeters, 10.0);
    float angularStep = normalStepMeters / planetRadius;

    float3 dirU = normalize(dir + tanU * angularStep);
    float3 dirV = normalize(dir + tanV * angularStep);

    float hC = SampleNormalHeight(dir);
    float hU = SampleNormalHeight(dirU);
    float hV = SampleNormalHeight(dirV);

    float3 pC = dir * (planetRadius + hC);
    float3 pU = dirU * (planetRadius + hU);
    float3 pV = dirV * (planetRadius + hV);

    float3 N = normalize(cross(pU - pC, pV - pC));

    if (dot(N, dir) < 0.0)
        N = -N;

    return N;
}

PixelInputType main(VertexInputType input)
{
    PixelInputType o;

    // Triangular grid barycentrics
    uint N = 1u << (uint) patchLevels;

    uint i = input.ij.x;
    uint j = input.ij.y;
    if (i + j > N)
    {
        i = min(i, N);
        j = N - i;
    }
    uint k = N - i - j;

    float invN = exp2(-(float) patchLevels);
    float wi = (float) i * invN;
    float wj = (float) j * invN;
    float wk = (float) k * invN;

    // IMPORTANT: map weights consistently to corners.
    // You must ensure (i,j,k) correspond to (V1,V2,V0) or similar consistently.
    // Pick ONE mapping and keep it everywhere.
    float w0 = wk; // for V0
    float w1 = wi; // for V1
    float w2 = wj; // for V2

    float3 V0 = normalize(input.V0);
    float3 V1 = normalize(input.V1);
    float3 V2 = normalize(input.V2);

    // Edge-consistent direction
    precise float3 dir = normalize(w0 * V0 + w1 * V1 + w2 * V2);
    
    // Compute slope from base heightmap only (no noise)
    float3 baseNormal = ComputeBaseNormalPS(dir);
    float slope = 1.0 - saturate(dot(normalize(baseNormal), dir));
    float colorAvg = SampleColorAvg(dir);
    
    float3 normalPS = ComputeTerrainNormalPS(dir, input.level, materialCount, slope, colorAvg, baseNormal);

    float3 worldPos = dir * planetRadius;

    float wallDebug = 0.0;
    float wallMaskDebug = 0.0;
    float erosionMaskDebug = 0.0, erosionPatternDebug = 0.0, erosionDeltaDebug = 0.0;
    
    float h = SampleTerrainHeight(dir, worldPos, input.level, materialCount, slope, colorAvg, normalPS, wallDebug, wallMaskDebug, erosionMaskDebug, erosionPatternDebug, erosionDeltaDebug);

    // High-precision relative position in planet space meters:
    precise float3 relPSHi = w0 * input.P0RelHi + w1 * input.P1RelHi + w2 * input.P2RelHi;
    relPSHi += dir * h;
    
    // Final view-relative = (hi-relative) - camLo
    float3 relPS = relPSHi - camLoPS;
    
    float3 relPatchPS = relPSHi + camHiPS - input.PatchOriginPS;
    o.TriplanarPos = relPatchPS;
    
    // World rotation only
    float3x3 R = (float3x3) worldMatrix;
    float3 relWS = mul(relPS, R);
    float3 nWS = normalize(mul(normalPS, R));
    o.normalWS = nWS;

    float4 viewPos = mul(float4(relWS, 1.0f), viewMatrixPlanetRendering);
    o.viewPosition = viewPos.xyz;
    o.pixelPosition = mul(viewPos, projectionMatrix);
    o.dirPS = dir;
    o.worldPosPS = relPSHi;
    o.WallDebug = wallDebug;
    o.WallMaskDebug = wallMaskDebug;
    o.ErosionMaskDebug = erosionMaskDebug;
    o.ErosionPatternDebug = erosionPatternDebug;
    o.ErosionDeltaDebug = erosionDeltaDebug;
    
    return o;
}

#type pixel
#pragma pack_matrix( row_major )

#define MAX_MATERIALS 8

struct PixelInputType
{
    float4 pixelPosition        : SV_POSITION;
    float3 viewPosition         : VIEWPOS;
    float3 normalWS             : NORMAL0;
    float3 dirPS                : TEXCOORD0; // planet-space unit direction
    float3 worldPosPS           : TEXCOORD1;
    float3 TriplanarPos         : TEXCOORD2;
    float WallDebug             : TEXCOORD3;
    float WallMaskDebug         : TEXCOORD4;
    float ErosionMaskDebug      : TEXCOORD5;
    float ErosionPatternDebug   : TEXCOORD6;
    float ErosionDeltaDebug     : TEXCOORD7;
};


struct PixelOutputType
{
	float4 position				: SV_Target0;
	float4 normal				: SV_Target1;
	float4 albedoMetallic		: SV_Target2;
	float4 roughnessAO			: SV_Target3;
	int entityID				: SV_Target4;
    float4 planetMaterialDebug  : SV_Target5;
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

cbuffer Model : register(b1)
{
    matrix worldMatrix; // planet rotation + translation (translation should already be floating-origin safe)
    float clickable;
    int entityID;
    int noWorldTransform;
    int isInstanced;
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

struct PBRSample
{
    float3 Albedo;
    float3 Normal;
    float Roughness;
    float AO;
};

StructuredBuffer<MaterialData> Materials    : register(t1);
Texture2DArray<float4> NormalCubeArray      : register(t2);
Texture2DArray<float4> AlbedoCubeArray      : register(t3);
StructuredBuffer<int4> PermTables           : register(t4);
Texture2DArray<float4> PBRAlbedoArray       : register(t6);
Texture2DArray<float4> PBRNormalArray       : register(t7);
Texture2DArray<float4> PBRRoughnessArray    : register(t8);
Texture2DArray<float4> PBRAOArray           : register(t9);
Texture2DArray<float4> PBRDisplacementArray : register(t10);

SamplerState defaultSampler                 : register(s0);
SamplerState PBRSampler                     : register(s1);

#include "PerlinNoise.hlsli"
#include "DirectionToCube.hlsli"

float2 HashUV(float2 p)
{
    p = float2(dot(p, float2(127.1, 311.7)),
               dot(p, float2(269.5, 183.3)));
    return frac(sin(p) * 43758.5453);
}

float4 HashCell(float2 cellId)
{
    float2 h = HashUV(cellId);
    return float4(h.x * 6.28318530, h.y, HashUV(cellId + 17.0).x, 0);
}

struct HexSample
{
    float2 cellIds[3];
    float weights[3];
    float2 localUVs[3];
};

HexSample GetHexSample(float2 uv)
{
    float2 s = float2(1.0, 1.73205080757);
    float2 p = uv;

    float2 hC = floor(p / s) + 0.5; 
    float4 h = float4(hC, hC + 0.5);
    float4 a = float4(p - h.xy * s, p - (h.zw + 0.5) * s);

    float4 closest = (dot(a.xy, a.xy) < dot(a.zw, a.zw))
        ? float4(a.xy, h.xy)
        : float4(a.zw, h.zw + 0.5);

    HexSample hs;

    float2 edge = closest.xy / float2(1.0, 0.86602540378);
    float2 sign2 = sign(edge);
    float2 absEdge = abs(edge);

    float2 neighbor1, neighbor2;
    if (absEdge.x > absEdge.y)
    {
        neighbor1 = float2(sign2.x, 0);
        neighbor2 = float2(sign2.x, sign2.y);
    }
    else
    {
        neighbor1 = float2(0, sign2.y);
        neighbor2 = float2(sign2.x, sign2.y);
    }

    hs.cellIds[0] = closest.zw;
    hs.cellIds[1] = closest.zw + neighbor1;
    hs.cellIds[2] = closest.zw + neighbor2;

    hs.localUVs[0] = p - hs.cellIds[0] * s;
    hs.localUVs[1] = p - hs.cellIds[1] * s;
    hs.localUVs[2] = p - hs.cellIds[2] * s;

    float3 dists = float3(length(hs.localUVs[0]), length(hs.localUVs[1]), length(hs.localUVs[2]));

    float3 w = 1.0 - smoothstep(0.0, 1.0, dists);
    w = w * w * w;
    float totalW = w.x + w.y + w.z;
    hs.weights[0] = w.x / totalW;
    hs.weights[1] = w.y / totalW;
    hs.weights[2] = w.z / totalW;

    return hs;
}

float4 SampleHexTiled(Texture2DArray<float4> tex, SamplerState samp, float2 uv, float slice)
{
    HexSample hs = GetHexSample(uv);
    float4 result = float4(0, 0, 0, 0);

    [unroll]
    for (int i = 0; i < 3; i++)
    {
        float4 hash = HashCell(hs.cellIds[i]);
        float rot = hash.x;
        float2 offset = hash.yz;

        float cosR = cos(rot);
        float sinR = sin(rot);
        float2 rotatedUV = float2(hs.localUVs[i].x * cosR - hs.localUVs[i].y * sinR, hs.localUVs[i].x * sinR + hs.localUVs[i].y * cosR);

        float2 sampleUV = rotatedUV + offset;
        result += tex.Sample(samp, float3(sampleUV, slice)) * hs.weights[i];
    }

    return result;
}

PBRSample SamplePBRTriplanar(uint materialIndex, float3 worldPos, float3 normalPS, float tilingScale)
{
    float3 blendWeights = abs(normalPS);
    blendWeights = pow(blendWeights, 4.0);
    blendWeights /= dot(blendWeights, 1.0.xxx);

    float3 p = worldPos * (1.0 / tilingScale);
    float slice = (float) materialIndex;

    // Three planar projections, each hex-tiled
    float4 cx = SampleHexTiled(PBRAlbedoArray, PBRSampler, p.yz, slice);
    float4 cy = SampleHexTiled(PBRAlbedoArray, PBRSampler, p.xz, slice);
    float4 cz = SampleHexTiled(PBRAlbedoArray, PBRSampler, p.xy, slice);

    float4 nx = SampleHexTiled(PBRNormalArray, PBRSampler, p.yz, slice);
    float4 ny = SampleHexTiled(PBRNormalArray, PBRSampler, p.xz, slice);
    float4 nz = SampleHexTiled(PBRNormalArray, PBRSampler, p.xy, slice);

    float rx = SampleHexTiled(PBRRoughnessArray, PBRSampler, p.yz, slice).r;
    float ry = SampleHexTiled(PBRRoughnessArray, PBRSampler, p.xz, slice).r;
    float rz = SampleHexTiled(PBRRoughnessArray, PBRSampler, p.xy, slice).r;

    float ax = SampleHexTiled(PBRAOArray, PBRSampler, p.yz, slice).r;
    float ay = SampleHexTiled(PBRAOArray, PBRSampler, p.xz, slice).r;
    float az = SampleHexTiled(PBRAOArray, PBRSampler, p.xy, slice).r;

    PBRSample s;
    s.Albedo = cx.rgb * blendWeights.x + cy.rgb * blendWeights.y + cz.rgb * blendWeights.z;
    s.Normal = nx.rgb * blendWeights.x + ny.rgb * blendWeights.y + nz.rgb * blendWeights.z;
    s.Normal = s.Normal * 2.0 - 1.0;
    s.Roughness = rx * blendWeights.x + ry * blendWeights.y + rz * blendWeights.z;
    s.AO = ax * blendWeights.x + ay * blendWeights.y + az * blendWeights.z;
    return s;
}

float3 LinearToSRGB(float3 x)
{
    float3 lo = x * 12.92;
    float3 hi = 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;
    return lerp(hi, lo, step(x, 0.0031308));
}

float4 SampleCubeBilinearLoadFloat4(Texture2DArray<float4> tex, float3 dir)
{
    CubeSample cs = DirectionToCube(dir);
    uint face = cs.face;
    float2 uv = cs.uv;

    uint W, H, L;
    tex.GetDimensions(W, H, L);
    uint2 dims = uint2(W, H);

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

    float4 v00 = tex.Load(int4(ij00, c00.face, 0));
    float4 v10 = tex.Load(int4(ij10, c10.face, 0));
    float4 v01 = tex.Load(int4(ij01, c01.face, 0));
    float4 v11 = tex.Load(int4(ij11, c11.face, 0));

    float4 vx0 = lerp(v00, v10, fxy.x);
    float4 vx1 = lerp(v01, v11, fxy.x);
    return lerp(vx0, vx1, fxy.y);
}

// ── Material weight computation (same as vertex shader) ──────────
float ComputeMaterialWeight(MaterialData mat, float slope, float colorAvg)
{
    float slopeScore = 1.0;
    if (slope < mat.SlopeMin)
        slopeScore = saturate(1.0 - (mat.SlopeMin - slope) * mat.BlendSharpness);
    else if (slope > mat.SlopeMax)
        slopeScore = saturate(1.0 - (slope - mat.SlopeMax) * mat.BlendSharpness);

    float colorScore = 1.0;
    if (mat.UseAlbedo > 0.5)
    {
        if (colorAvg < mat.ColorAvgMin)
            colorScore = saturate(1.0 - (mat.ColorAvgMin - colorAvg) * mat.BlendSharpness);
        else if (colorAvg > mat.ColorAvgMax)
            colorScore = saturate(1.0 - (colorAvg - mat.ColorAvgMax) * mat.BlendSharpness);
    }

    return slopeScore * colorScore;
}

void ComputeAllMaterialWeights(float displacedSlope, float colorAvg, uint matCount, out float weights[MAX_MATERIALS])
{
    float totalWeight = 0.0;
    for (uint m = 0; m < matCount; m++)
    {
        weights[m] = ComputeMaterialWeight(Materials[m], displacedSlope, colorAvg);
        totalWeight += weights[m];
    }
    float invTotal = (totalWeight > 0.001) ? (1.0 / totalWeight) : 0.0;
    for (uint m2 = 0; m2 < matCount; m2++)
        weights[m2] *= invTotal;
}

PixelOutputType main(PixelInputType input)
{
	PixelOutputType output;
	PBRSample params;
    
    float3x3 R = (float3x3) worldMatrix;
    
    float camToSurface = length(input.viewPosition);
    float cubeNormalStrength = smoothstep(20000.0f, 80000.0f, camToSurface);
    float3 nWS = normalize(input.normalWS);
    float3 normalPS = normalize(mul(nWS, transpose(R)));
    float3 dirPS = normalize(input.dirPS);
    
    if (cubeNormalStrength > 0.001)
    {
        float3 cubeNPS = SampleCubeBilinearLoadFloat4(NormalCubeArray, dirPS).rgb * 2.0 - 1.0;
        normalPS = normalize(lerp(normalPS, cubeNPS, cubeNormalStrength));
        nWS = normalize(mul(normalPS, R));
    }  
    
    /*--------------------------------------------------------------*/
    /* 1) Get cubemap UVs from direction                            */
    /*--------------------------------------------------------------*/       
    CubeSample cs = DirectionToCube(dirPS);
    float3 coord = float3(cs.uv, (float) cs.face);
    
    /*--------------------------------------------------------------*/
    /* 2) Samling Albedo                                            */
    /*--------------------------------------------------------------*/   
    float3 albedoSample = AlbedoTexToggle > 0 ? SampleCubeBilinearLoadFloat4(AlbedoCubeArray, dirPS).rgb : Albedo.rgb;
    
    if (MaterialsEnabled == 0)
    {
        // Bypass the entire material system
        float3 finalAlbedo = albedoSample;

        //if (AlbedoTexToggle > 0)
        //    finalAlbedo = LinearToSRGB(finalAlbedo);

        output.albedoMetallic = float4(finalAlbedo, Metalness);
        output.roughnessAO = float4(Roughness, 0.0, 0.0, 1.0);
        output.normal = float4(normalize(mul(nWS, (float3x3) viewMatrix)) * 0.5 + 0.5, 1.0);
        output.position = float4(input.viewPosition, 1.0);
        output.entityID = 0;
        output.planetMaterialDebug = float4(0, 0, 0, 1);
        return output;
    }
    
	output.position = float4(input.viewPosition, 1.0f);
      
    if (WallDebugEnabled > 0.5)
    {
        float debugValue = 0.0;

        // 0 = actual wall boost magnitude
        // 1 = wall slope mask
        // 2 = final normal slope
        if (WallDebugMode < 0.5)
        {
            debugValue = input.WallDebug;
            output.albedoMetallic = float4(debugValue, debugValue * 0.5, 0.0, 1.0);
        }
        else if (WallDebugMode < 1.5)
        {
            debugValue = input.WallMaskDebug;
            output.albedoMetallic = float4(debugValue, 0.0, 0.0, 1.0);
        }
        else
        {
            float slopeDebug = 1.0 - saturate(dot(normalPS, dirPS));
            output.albedoMetallic = float4(slopeDebug, slopeDebug, slopeDebug, 1.0);
        }

        output.position = float4(input.viewPosition, 1.0);
        output.roughnessAO = float4(1.0, 1.0, 0.0, 1.0);
        output.normal = float4(normalize(mul(nWS, (float3x3) viewMatrix)) * 0.5 + 0.5, 1.0);
        output.entityID = 0;
        output.planetMaterialDebug = output.albedoMetallic;

        return output;
    }
    
    if (ErosionDebugEnabled > 0.5)
    {
        // 0 = erosion height delta magnitude
        // 1 = erosion slope mask
        // 2 = erosion raw pattern
        // 3 = erosion affected pattern
        if (ErosionDebugMode < 0.5)
        {
            float d = input.ErosionDeltaDebug;
            output.albedoMetallic = float4(d, d * 0.5, 0.0, 1.0);
        }
        else if (ErosionDebugMode < 1.5)
        {
            float d = input.ErosionMaskDebug;
            output.albedoMetallic = float4(d, 0.0, 0.0, 1.0);
        }
        else if (ErosionDebugMode < 2.5)
        {
            float d = input.ErosionPatternDebug;
            output.albedoMetallic = float4(d, d, d, 1.0);
        }
        else
        {
            float d = saturate(input.ErosionMaskDebug * abs(input.ErosionPatternDebug * 2.0 - 1.0));
            output.albedoMetallic = float4(d, d * 0.4, 0.0, 1.0);
        }

        output.position = float4(input.viewPosition, 1.0);
        output.roughnessAO = float4(1.0, 1.0, 0.0, 1.0);
        output.normal = float4(normalize(mul(nWS, (float3x3) viewMatrix)) * 0.5 + 0.5, 1.0);
        output.entityID = 0;
        output.planetMaterialDebug = output.albedoMetallic;

        return output;
    }
    
    // Compute materials
    float colorAvg = (albedoSample.r + albedoSample.g + albedoSample.b) / 3.0;
    float weights[MAX_MATERIALS];
    float displacedSlope = 1.0 - saturate(dot(normalPS, dirPS)); // from final normal
    ComputeAllMaterialWeights(displacedSlope, colorAvg, MaterialCount, weights);
    
    // Blend all PBR samples weighted by material contribution
    float3 blendedAlbedo = float3(0, 0, 0);
    float3 blendedNormal = float3(0, 0, 0);
    float blendedRoughness = 0.0;
    float blendedAO = 0.0;
    
    for (uint m = 0; m < MaterialCount; m++)
    {
        if (weights[m] < 0.001)
            continue;

        PBRSample pbr = SamplePBRTriplanar(m, input.TriplanarPos, normalPS, Materials[m].UVTilingScale);
        blendedAlbedo += pbr.Albedo * weights[m];
        blendedNormal += pbr.Normal * weights[m];
        blendedRoughness += pbr.Roughness * weights[m];
        blendedAO += pbr.AO * weights[m];
    }

    /*--------------------------------------------------------------*/
    /* 3) albedo + metallic                                         */
    /*--------------------------------------------------------------*/
    
    // Base albedo from cubemap carries the broad color zones.
    // PBR albedo from tiled textures carries the high-frequency detail.
    // Multiply them so both contributions are always visible at every distance.
    // The 0.5 + pbrLum keeps mid-gray PBR textures from darkening the base:
    //   - pbrLum = 0.5 (average brightness) -> multiplier = 1.0 (no change)
    //   - pbrLum = 1.0 (bright details)     -> multiplier = 1.5 (brighten)
    //   - pbrLum = 0.0 (dark details)       -> multiplier = 0.5 (darken)
    float pbrLum = (blendedAlbedo.r + blendedAlbedo.g + blendedAlbedo.b) / 3.0;
    float baseLum = dot(albedoSample, float3(0.299, 0.587, 0.114));
    float3 baseBlend = albedoSample * (0.5 + pbrLum); // old style
    float3 pbrBlend = blendedAlbedo * (0.6 + baseLum * 0.8); // new style
    float3 finalAlbedo = lerp(baseBlend, pbrBlend, PBRColorDominance);
    
    float3 colorNoisePos = dirPS * 3389500.0f;

    float rVar = FractalPerlin3D(0, colorNoisePos, ColorNoiseOctaves, ColorNoiseFrequency, 1.0, 2.0, 0.5);
    float gVar = FractalPerlin3D(0, colorNoisePos + float3(1000, 0, 0), ColorNoiseOctaves, ColorNoiseFrequency, 1.0, 2.0, 0.5);
    float bVar = FractalPerlin3D(0, colorNoisePos + float3(0, 1000, 0), ColorNoiseOctaves, ColorNoiseFrequency, 1.0, 2.0, 0.5);
    
    float3 colorVariation = 1.0f + float3(rVar, gVar, bVar) * ColorNoiseStrength;
    finalAlbedo *= colorVariation;
    
    if (AlbedoTexToggle > 0)
        finalAlbedo = LinearToSRGB(finalAlbedo);
    
    output.albedoMetallic = float4(finalAlbedo, 1.0f);

    /*--------------------------------------------------------------*/
    /* 4) roughness + ambient occlusion                             */
    /*--------------------------------------------------------------*/
    output.roughnessAO = float4(0.954f, blendedAO, 0.0, 1.0);
    
    /*--------------------------------------------------------------*/
    /* 5) Normal                                                    */
    /*--------------------------------------------------------------*/   
    float3 perturbedNormalPS = normalize(normalPS + blendedNormal.xyz * 0.5);
    float3 perturbedNormalWS = normalize(mul(perturbedNormalPS, R));
    float3 perturbedNormalVS = normalize(mul(perturbedNormalWS, (float3x3) viewMatrix));
    output.normal = float4(perturbedNormalVS * 0.5 + 0.5, 1.0);
    
    /*--------------------------------------------------------------*/
    /* 6) Material debug visualization                              */
    /*--------------------------------------------------------------*/
    float3 debugColor = float3(0, 0, 0);
    for (uint dm = 0; dm < MaterialCount; dm++)
        debugColor += Materials[dm].DebugColor * weights[dm];

    output.planetMaterialDebug = float4(debugColor, 1.0);
    
    output.entityID = 0;
   
	return output;
}