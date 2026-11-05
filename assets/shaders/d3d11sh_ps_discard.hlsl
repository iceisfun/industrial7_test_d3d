// d3d11_shaders corpus: target ps_4_0, entry main
float4 main(float4 p : SV_Position) : SV_Target { if (p.x < 2.0f) discard; return float4(1, 1, 1, 1); }
