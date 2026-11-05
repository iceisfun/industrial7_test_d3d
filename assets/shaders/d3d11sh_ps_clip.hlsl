// d3d11_shaders corpus: target ps_4_0, entry main
float4 main(float4 p : SV_Position) : SV_Target { clip(p.y - 1.0f); return float4(1, 0, 0, 1); }
