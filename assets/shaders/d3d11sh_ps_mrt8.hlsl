// d3d11_shaders corpus: target ps_4_0, entry main
struct O { float4 c0 : SV_Target0; float4 c1 : SV_Target1; float4 c2 : SV_Target2; float4 c3 : SV_Target3; float4 c4 : SV_Target4; float4 c5 : SV_Target5; float4 c6 : SV_Target6; float4 c7 : SV_Target7; };
O main(float4 p : SV_Position) { O o; o.c0 = 0; o.c1 = 1; o.c2 = 2; o.c3 = 3; o.c4 = 4; o.c5 = 5; o.c6 = 6; o.c7 = 7; return o; }
