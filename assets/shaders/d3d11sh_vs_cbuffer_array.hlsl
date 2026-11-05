// d3d11_shaders corpus: target vs_4_0, entry main
cbuffer cb0 : register(b0) { float4 c[16]; };
float4 main(float4 p : POSITION, uint id : SV_VertexID) : SV_Position { return p + c[id & 15]; }
