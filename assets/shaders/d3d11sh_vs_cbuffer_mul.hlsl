// d3d11_shaders corpus: target vs_4_0, entry main
cbuffer cb0 : register(b0) { float4 scale; };
float4 main(float4 p : POSITION) : SV_Position { return p * scale; }
