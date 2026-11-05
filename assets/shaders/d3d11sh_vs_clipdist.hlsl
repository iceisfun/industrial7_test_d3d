// d3d11_shaders corpus: target vs_4_0, entry main
struct O { float4 p : SV_Position; float c : SV_ClipDistance0; };
O main(float4 p : POSITION) { O o; o.p = p; o.c = p.x; return o; }
