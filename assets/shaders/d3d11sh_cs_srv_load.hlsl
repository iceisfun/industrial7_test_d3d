// d3d11_shaders corpus: target cs_5_0, entry main
Buffer<uint> src : register(t0);
RWBuffer<uint> dst : register(u0);
[numthreads(8, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.x] = src[id.x] + 1; }
