// d3d11_shaders corpus: target ps_5_0, entry main
struct O { float4 c : SV_Target; uint cov : SV_Coverage; };
O main(float4 p : SV_Position) { O o; o.c = float4(1, 1, 1, 1); o.cov = 1; return o; }
