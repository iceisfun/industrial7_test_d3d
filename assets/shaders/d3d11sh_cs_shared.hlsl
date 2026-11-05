// d3d11_shaders corpus: target cs_5_0, entry main
RWStructuredBuffer<uint> outbuf : register(u0);
groupshared uint tile[64];
[numthreads(64, 1, 1)] void main(uint3 gt : SV_GroupThreadID, uint3 id : SV_DispatchThreadID) { tile[gt.x] = id.x; GroupMemoryBarrierWithGroupSync(); outbuf[id.x] = tile[63 - gt.x]; }
