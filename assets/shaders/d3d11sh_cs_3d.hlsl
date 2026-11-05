// d3d11_shaders corpus: target cs_5_0, entry main
RWStructuredBuffer<uint> outbuf : register(u0);
[numthreads(4, 4, 4)] void main(uint gi : SV_GroupIndex, uint3 g : SV_GroupID) { outbuf[g.x * 64 + gi] = gi; }
