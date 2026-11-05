// d3d11_shaders corpus: target vs_4_0, entry main
float4 main(uint id : SV_VertexID) : SV_Position { return float4(id == 1 ? 3.0f : -1.0f, id == 2 ? 3.0f : -1.0f, 0, 1); }
