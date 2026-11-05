// d3d11_shaders corpus: target cs_5_0, entry main
RWStructuredBuffer<uint> outbuf : register(u0);
[numthreads(32, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { uint o; InterlockedAdd(outbuf[0], 1, o); InterlockedMax(outbuf[1], id.x); }
