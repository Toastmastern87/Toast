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

    float3 planetCenterRelLoWS;
};

struct VertexInputType
{
    uint2 ij        : TEXCOORD0; // per-vertex
    int level       : TEXTUREID0; // per-instance

    float3 V0       : POSITION0;
    float3 V1       : POSITION1;
    float3 V2       : POSITION2;

    float3 P0RelHi  : TEXCOORD1;
    float3 P1RelHi  : TEXCOORD2;
    float3 P2RelHi  : TEXCOORD3;
};

struct PixelInputType
{
	float4 pixelPosition	: SV_POSITION;
	float3 viewPosition		: VIEWPOS;
	float3 normalWS	        : NORMAL0;
    float3 dirPS            : TEXCOORD0; // planet-space unit direction
};

Texture2DArray<float> HeightCubeArray : register(t0);
SamplerState HeightMapSampler : register(s5);

#include "DirectionToCube.hlsli"

float SampleCubeBilinear(float3 dir, uint2 dims, uint mip)
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

// ── height sampling using manual bilinear ────────────────────────
float SampleHeightMetres(float3 dir)
{
    uint W, H, L;
    HeightCubeArray.GetDimensions(W, H, L);
    return SampleCubeBilinear(normalize(dir), uint2(W, H), 0);
}

// ── compute terrain normal via finite differences ────────────────
float3 ComputeTerrainNormalPS(float3 dir)
{
    // Tangent frame on the unit sphere
    float3 up = (abs(dir.y) < 0.999) ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tanU = normalize(cross(up, dir));
    float3 tanV = cross(dir, tanU);

    // Angular step: one texel of the cubemap face
    uint width, height, layers;
    HeightCubeArray.GetDimensions(width, height, layers);
    float angularStep = (2.0 / (float) width) * (3.14159265 * 0.5);

    float3 dirU = normalize(dir + tanU * angularStep);
    float3 dirV = normalize(dir + tanV * angularStep);

    float hC = SampleHeightMetres(dir);
    float hU = SampleHeightMetres(dirU);
    float hV = SampleHeightMetres(dirV);

    // Displaced positions on the surface
    float3 pC = dir * (planetRadius + hC);
    float3 pU = dirU * (planetRadius + hU);
    float3 pV = dirV * (planetRadius + hV);

    float3 N = normalize(cross(pU - pC, pV - pC));

    // Ensure outward-facing
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

    float h = SampleHeightMetres(dir);

    // High-precision relative position in planet space meters:
    precise float3 relPSHi = w0 * input.P0RelHi + w1 * input.P1RelHi + w2 * input.P2RelHi;
    relPSHi += dir * h;
    
    // Final view-relative = (hi-relative) - camLo
    float3 relPS = relPSHi - camLoPS;

    float3 normalPS = ComputeTerrainNormalPS(dir);
    
    // World rotation only
    float3x3 R = (float3x3) worldMatrix;
    float3 relWS = mul(relPS, R);
    float3 nWS = normalize(mul(normalPS, R));
    o.normalWS = nWS;

    float4 viewPos = mul(float4(relWS, 1.0f), viewMatrixPlanetRendering);
    o.viewPosition = viewPos.xyz;
    o.pixelPosition = mul(viewPos, projectionMatrix);
    o.dirPS = dir;
    return o;
}

#type pixel
#pragma pack_matrix( row_major )

struct PixelInputType
{
	float4 pixelPosition		: SV_POSITION;
	float3 viewPosition			: VIEWPOS;
	float3 normalWS     		: NORMAL0;
    float3 dirPS                : TEXCOORD0; // planet-space unit direction
};

struct PixelOutputType
{
	float4 position				: SV_Target0;
	float4 normal				: SV_Target1;
	float4 albedoMetallic		: SV_Target2;
	float4 roughnessAO			: SV_Target3;
	int entityID				: SV_Target4;
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
    float SlopeSensitivity;
    float SlopeThreshold;
    float SlopeDarkening;
};

Texture2DArray<float4> NormalCubeArray      : register(t2);
Texture2DArray<float4> AlbedoCubeArray      : register(t3);

SamplerState defaultSampler                 : register(s0);

#include "DirectionToCube.hlsli"

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

PixelOutputType main(PixelInputType input)
{
	PixelOutputType output;
	PBRParameters params;
    
    /*--------------------------------------------------------------*/
    /* 1) Get cubemap UVs from direction                            */
    /*--------------------------------------------------------------*/   
    float3 dirPS = normalize(input.dirPS);
    
    CubeSample cs = DirectionToCube(dirPS);
    float3 coord = float3(cs.uv, (float) cs.face);
    
    /*--------------------------------------------------------------*/
    /* 2) position + normal                                         */
    /*--------------------------------------------------------------*/   
    float3 nWS = normalize(input.normalWS);

    float3x3 R = (float3x3) worldMatrix;
    
    float camToSurface = length(input.viewPosition);
    float cubeNormalStrength = smoothstep(1000.0f, 5000.0f, camToSurface);
    
    float3 normalPS = normalize(mul(nWS, transpose(R)));
    
    if (cubeNormalStrength > 0.001)
    {
        float3 cubeNPS = SampleCubeBilinearLoadFloat4(NormalCubeArray, dirPS).rgb * 2.0 - 1.0;
        normalPS = normalize(lerp(normalPS, cubeNPS, cubeNormalStrength));
        nWS = normalize(mul(normalPS, R));
    }
    
    float3 nVS = normalize(mul(nWS, (float3x3) viewMatrix));
    
	output.position = float4(input.viewPosition, 1.0f);
	output.normal = float4(nVS * 0.5f + 0.5f, 1.0f);

    /*--------------------------------------------------------------*/
    /* 3) albedo + metallic                                         */
    /*--------------------------------------------------------------*/
    
    float3 albedo = AlbedoTexToggle > 0 ? SampleCubeBilinearLoadFloat4(AlbedoCubeArray, dirPS).rgb : Albedo.rgb;
    
    // ── slope darkening ──────────────────────────────────────────
    // Both normalWS and dirPS are in planet space before the VS
    // rotates normalWS to world space — but we need the planet-space
    // normal for slope. Undo the world rotation:
    float slope = 1.0 - saturate(dot(normalPS, dirPS));
    slope = saturate(slope * SlopeSensitivity - SlopeThreshold);
    albedo *= lerp(1.0, SlopeDarkening, slope);
    
    if (AlbedoTexToggle > 0)
        albedo = LinearToSRGB(albedo);
    
    output.albedoMetallic = float4(albedo, Metalness);

    /*--------------------------------------------------------------*/
    /* 4) roughness + ambient occlusion                             */
    /*--------------------------------------------------------------*/
	output.roughnessAO = float4(Roughness, 0.0f, 0.0, 1.0);

	output.entityID = 0;
   
	return output;
}