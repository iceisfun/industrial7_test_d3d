// d3d11_shaders corpus: target ps_5_0, entry main
float4 main(float4 p : SV_Position, uint4 v : TEXCOORD0) : SV_Target { return float4((float)countbits(v.x), (float)firstbitlow(v.y), (float)(v.z ^ v.w), 1); }
