// d3d11_shaders corpus: target vs_4_0, entry main
Buffer<float4> buf : register(t0);
float4 main(uint id : SV_VertexID) : SV_Position { return buf[id]; }
