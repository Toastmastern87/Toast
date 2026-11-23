#type compute
#pragma pack_matrix(row_major)

RWTexture2DArray<float> SrcMip : register(u0);
RWTexture2DArray<float> DstMip : register(u1);

[numthreads(8, 8, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
    uint width, height, slices;
    DstMip.GetDimensions(width, height, slices);
    if (id.x >= width || id.y >= height)
        return;
    
    const uint2 s = id.xy * 2u;
    const float h00 = SrcMip[uint3(s, 0)];
    const float h10 = SrcMip[uint3(s + uint2(1, 0), 0)];
    const float h01 = SrcMip[uint3(s + uint2(0, 1), 0)];
    const float h11 = SrcMip[uint3(s + uint2(1, 1), 0)];
    
    DstMip[uint3(id.xy, 0)] = max(max(h00, h10), max(h01, h11));

}