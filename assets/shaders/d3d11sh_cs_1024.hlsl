// d3d11_shaders corpus: target cs_5_0, entry main
RWStructuredBuffer<uint> outbuf : register(u0);
[numthreads(1024, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { outbuf[id.x] = id.x; }
