// d3d11_shaders corpus: target cs_5_0, entry main
ConsumeStructuredBuffer<uint> inbuf : register(u0);
RWStructuredBuffer<uint> outbuf : register(u1);
[numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { outbuf[0] = inbuf.Consume(); }
