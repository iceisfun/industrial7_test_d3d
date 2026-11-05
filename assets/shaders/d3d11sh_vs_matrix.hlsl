// d3d11_shaders corpus: target vs_4_0, entry main
cbuffer cb0 : register(b0) { float4x4 wvp; };
float4 main(float4 p : POSITION) : SV_Position { return mul(p, wvp); }
