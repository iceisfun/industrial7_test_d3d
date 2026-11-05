// d3d11_shaders corpus: target vs_5_0, entry main
float4 main(uint4 v : BLENDINDICES) : SV_Position { return float4((float)countbits(v.x), (float)firstbithigh(v.y), (float)reversebits(v.z), (float)v.w); }
