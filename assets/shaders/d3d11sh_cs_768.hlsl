// d3d11_shaders corpus: target cs_4_0, entry main
RWByteAddressBuffer outbuf : register(u0);
[numthreads(768, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { outbuf.Store(id.x * 4, id.x); }
