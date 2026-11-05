// d3d11_shaders corpus: target vs_4_0, entry main
float4 main(float4 p : POSITION) : SV_Position { float4 a = p; [loop] for (int i = 0; i < 8; i++) a += p * 0.125f; return a; }
