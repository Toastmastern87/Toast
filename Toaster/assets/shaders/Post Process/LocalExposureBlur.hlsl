#type compute
#pragma pack_matrix(row_major)

Texture3D<float2> GridIn        : register(t0);

RWTexture3D<float2> GridOut     : register(u0);

static const float W5[5] = { 1.0f, 4.0f, 6.0f, 4.0f, 1.0f };

[numthreads(4, 4, 4)]
void main( uint3 id : SV_DispatchThreadID )
{
    uint gridW, gridH, gridD;
    GridIn.GetDimensions(gridW, gridH, gridD);
    
    if (any(id >= uint3(gridW, gridH, gridD)))
        return;

    float2 acc = 0.0f;
    
    [loop]
    for (int dz = -2; dz <= 2; ++dz)
    {
        [loop]
        for (int dy = -2; dy <= 2; ++dy)
        {
            [unroll]
            for (int dx = -2; dx <= 2; ++dx)
            {
                const int3 c = int3(id) + int3(dx, dy, dz);
                if (any(c < 0) || any(c >= int3(gridW, gridH, gridD)))
                    continue;
                acc += (W5[dx + 2] * W5[dy + 2] * W5[dz + 2]) * GridIn.Load(int4(c, 0));
            }
        }
    }

    GridOut[id] = acc * (1.0f / 4096.0f);

}