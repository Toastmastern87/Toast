#inputlayout        // packed 16-bit grid coordinates: uint16 gx, uint16 gy
vertex

#type vertex
#pragma pack_matrix( row_major )

static const float PI = 3.14159265358979323846f;
static const float INV_TWO_PI = 1.0f / (2.0f * PI);
static const float INV_PI = 1.0f / PI;

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
    float3 BasisLonNorth;
    float3 BasisSpinUp;
};

cbuffer PlanetLevel : register(b7) 
{
    int OriginX;
    int OriginY;
    int CellSize;
    int GridSize;
};

cbuffer HeightDetail : register(b8)
{
    int4 Perm[64]; // 1024 bytes
    
    int LODActivation;
    int Octaves;
    float Frequency;
    float Amplitude; 
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


#include "PerlinNoise.hlsli"

Texture2DArray<float> HeightCubeArray           : register(t0);

SamplerState HeightMapSampler                   : register(s5);

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

/*──────────────────────── Use it in vertex sampling ───────────────────────────────*/

float SampleHeightFromDir(float3 dirPlanet)
{
    // Mip 0; if you use mips, compute dims for that mip.
    uint W, H, L;
    HeightCubeArray.GetDimensions(W, H, L);
    return SampleCubeBilinearLoad(normalize(dirPlanet), uint2(W, H), /*mip*/0);
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

PlanetPointVS CalulatePlanetPosVS(int2 gWorld)
{
    // 1. Local tangent-plane coordinates in meters
    //    (gWorld is already small: around [-128,128] in your setup)
    float2 off = (float2) gWorld * (float) CellSize;

    // 2. Build a direction on the reference sphere (still using your old logic)
    //    We keep this so height sampling behaves identically.
    float3 pSphereLocal =
          BasisRadUp
        + BasisTanEast * (off.x / PlanetRadius)
        + BasisTanNorth * (off.y / PlanetRadius);

    // Direction from planet center in world space
    float3 nWS = normalize(pSphereLocal);

    // Convert world-space normal to planet-local coordinates for cubemap sampling
    float3 vPlanet;
    vPlanet.x = dot(nWS, BasisLonEast); // "east" axis of planet
    vPlanet.y = dot(nWS, BasisSpinUp); // spin axis
    vPlanet.z = dot(nWS, BasisLonNorth); // "north" axis

    float h = SampleHeightFromDir(normalize(vPlanet)); // height in meters

    // 3. Choose geometry model:
    //    - For L0–L3 (CellSize <= 8): use tangent-plane around the camera.
    //    - For L3+          : use the exact spherical expression.
    float3 pRelWS;
    
    int patchLod = LodFromCellSize((int) CellSize);
    
    if (CellSize <= 8)   // L0=1, L1=2, L2=4, L3=8  → tangent-plane
    {
        // Tangent-plane offset in world space (meters)
        float3 pPlaneWS = BasisTanEast * off.x + BasisTanNorth * off.y;
        
        if (patchLod >= LODActivation)// L1=2 (and L0=1)
        {
            // Use tangent-plane coordinates in meters (off is meters)
            // Frequency should be in 1/meters (e.g. 0.01 -> ~100m features)
            float2 pNoise = off; // meters

            float noiseMeters = FractalPerlin2D(pNoise, Octaves, Frequency, Amplitude);

            // Add to base height
            h += noiseMeters;
        }

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

    PlanetPointVS p;
    p.posVS = posVS;
    p.nWS = nWS; // still the true spherical normal from planet center
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
    float3 BasisLonNorth;
    float3 BasisSpinUp;
};

cbuffer PlanetLevel : register(b7)
{
    int OriginX;
    int OriginY;
    int CellSize;
    int GridSize;
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

Texture2DArray<float> HeightCubeArray           : register(t0);

SamplerState HeightMapSampler           : register(s5);

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

// Sample height from direction using seamless bilinear Load across faces (mip 0).
float SampleHeightDir(float3 dirPlanet)
{
    uint W, H, L;
    HeightCubeArray.GetDimensions(W, H, L);
    return SampleCubeBilinearLoad(normalize(dirPlanet), uint2(W, H), 0);
}

// Compute analytical normal using longitude/latitude finite differences in planet-local axes,
// with height coming from the cubemap via SampleHeightDir.
float3 AnalyticalNormalFromCube(float3 nSphereWS)
{
    float3 S;
    S.x = dot(nSphereWS, BasisLonEast);
    S.y = dot(nSphereWS, BasisSpinUp);
    S.z = dot(nSphereWS, BasisLonNorth);
    S = normalize(S);

    uint W, H, L;
    HeightCubeArray.GetDimensions(W, H, L);
    float dlon = 2.0f * PI / max(64.0f, (float) W);
    float dlat = PI / max(64.0f, (float) H);

    float lon = atan2(S.z, S.x);
    // BUGFIX: use symmetric clamp, not saturate (which clamps to [0,1])
    float lat = asin(clamp(S.y, -1.0f, 1.0f));

    float cosLat = cos(lat);
    float sinLat = sin(lat);
    float cosLon = cos(lon);
    float sinLon = sin(lon);

    float3 dSdlon = float3(-cosLat * sinLon, 0.0f, cosLat * cosLon);
    float3 dSdlat = float3(-sinLat * cosLon, cosLat, -sinLat * sinLon);

    float3 SWS = S.x * BasisLonEast + S.y * BasisSpinUp + S.z * BasisLonNorth;
    float3 dSdlonWS = dSdlon.x * BasisLonEast + dSdlon.y * BasisSpinUp + dSdlon.z * BasisLonNorth;
    float3 dSdlatWS = dSdlat.x * BasisLonEast + dSdlat.y * BasisSpinUp + dSdlat.z * BasisLonNorth;

    float3 S_lonP = normalize(float3(
        cosLat * cos(lon + dlon),
        sinLat,
        cosLat * sin(lon + dlon)
    ));
    float3 S_latP = normalize(float3(
        cos(lat + dlat) * cosLon,
        sin(lat + dlat),
        cos(lat + dlat) * sinLon
    ));

    float h0 = SampleHeightDir(S);
    float hLonP = SampleHeightDir(S_lonP);
    float hLatP = SampleHeightDir(S_latP);

    float dhdlon = (hLonP - h0) / dlon;
    float dhdlat = (hLatP - h0) / dlat;

    float R = PlanetRadius;
    float3 Pu = (R + h0) * dSdlonWS + dhdlon * SWS;
    float3 Pv = (R + h0) * dSdlatWS + dhdlat * SWS;

    return normalize(cross(Pv, Pu));
}

PixelOutputType main(PixelInputType input)
{
    PixelOutputType output;
    PBRParameters params;
    
    /*--------------------------------------------------------------*/
    /* 1) position + normal                                         */
    /*--------------------------------------------------------------*/
    
    float3 nSphereWS = normalize(input.normalSphereWS);
 
    float3 nWS = AnalyticalNormalFromCube(nSphereWS);
    float3 nVS = normalize(mul(nWS, (float3x3) viewMatrix));
    
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