// d3d11_shaders corpus: target ps_4_0, entry main
float4 main(float4 p : SV_Position, uint id : SV_PrimitiveID) : SV_Target { return float4((float)id, 0, 0, 1); }
