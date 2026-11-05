// d3d11_shaders corpus: target vs_4_0, entry main
float4 main(float4 p : POSITION) : SV_Position { if (p.x > 0.5f) return p * 2; else return p * 0.5f; }
