// d3d11_shaders corpus: target vs_4_0, entry main
Texture2D<float4> tex : register(t0);
float4 main(uint id : SV_VertexID) : SV_Position { return tex.Load(int3(id & 3, 0, 0)); }
