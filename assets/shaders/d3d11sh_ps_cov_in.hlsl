// d3d11_shaders corpus: target ps_5_0, entry main
float4 main(float4 p : SV_Position, uint cov : SV_Coverage) : SV_Target { return float4((float)cov, 0, 0, 1); }
