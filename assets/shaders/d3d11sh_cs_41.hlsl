// d3d11_shaders corpus: target cs_4_1, entry main
RWStructuredBuffer<uint> outbuf : register(u0);
[numthreads(8, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { outbuf[id.x] = id.x * 3; }
