// d3d11_shaders corpus: target ps_4_1, entry main
float4 main(float4 p : SV_Position, uint s : SV_SampleIndex) : SV_Target { return float4((float)s, 0, 0, 1); }
