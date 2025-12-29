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