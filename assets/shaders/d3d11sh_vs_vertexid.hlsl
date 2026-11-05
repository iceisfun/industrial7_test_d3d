// d3d11_shaders corpus: target vs_4_0, entry main
float4 main(uint id : SV_VertexID) : SV_Position { return float4((float)id, 0, 0, 1); }
