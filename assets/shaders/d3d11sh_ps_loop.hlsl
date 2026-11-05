// d3d11_shaders corpus: target ps_4_0, entry main
float4 main(float4 p : SV_Position) : SV_Target { float a = 0; [loop] for (int i = 0; i < 16; i++) a += 0.0625f; return a.xxxx; }
