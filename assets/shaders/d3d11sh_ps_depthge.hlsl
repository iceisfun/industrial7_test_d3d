// d3d11_shaders corpus: target ps_5_0, entry main
struct O { float4 c : SV_Target; float d : SV_DepthGreaterEqual; };
O main(float4 p : SV_Position) { O o; o.c = 1; o.d = p.z + 0.25f; return o; }
