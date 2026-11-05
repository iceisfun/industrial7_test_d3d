// d3d11_shaders corpus: target ps_4_0, entry main
float4 main(float4 p : SV_Position, bool f : SV_IsFrontFace) : SV_Target { return f ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1); }
