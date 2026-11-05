// d3d11_shaders corpus: target cs_5_0, entry main
AppendStructuredBuffer<uint> outbuf : register(u0);
[numthreads(16, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { outbuf.Append(id.x); }
