// d3d11_shaders corpus: target ps_5_0, entry main
RWStructuredBuffer<uint> outbuf : register(u1);
float4 main(float4 p : SV_Position) : SV_Target { uint n; InterlockedAdd(outbuf[0], 1, n); return float4(1, 1, 1, 1); }
