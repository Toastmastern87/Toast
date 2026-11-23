#ifndef HEIGHT_CUBE_SAMPLING_HLSLI
#define HEIGHT_CUBE_SAMPLING_HLSLI

#include "DirectionToCube.hlsli"

float LoadHeightBilinear(Texture2DArray<float> heightCube, float3 dirPS, float mipF, uint2 size0, uint mipCount)
{
    const uint mip = (uint) clamp(ceil(mipF), 0.0f, (float) (mipCount - 1));
    const uint2 size = max(size0 >> mip, uint2(1, 1));
    const int2 maxIJ = int2(size) - 1;
    const CubeSample cs = DirectionToCube(dirPS);

    const float2 p = cs.uv * (float2) size - 0.5f;
    const float2 f = frac(p);
    const int2 i0 = clamp(int2(floor(p)), int2(0, 0), maxIJ);
    const int2 i1 = min(i0 + 1, maxIJ);

    const float h00 = heightCube.Load(int4(i0.x, i0.y, cs.face, mip));
    const float h10 = heightCube.Load(int4(i1.x, i0.y, cs.face, mip));
    const float h01 = heightCube.Load(int4(i0.x, i1.y, cs.face, mip));
    const float h11 = heightCube.Load(int4(i1.x, i1.y, cs.face, mip));

    return lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
}

#endif