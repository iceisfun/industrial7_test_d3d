// d3d11_shaders corpus: target vs_4_0, entry main
float4 main(uint4 v : BLENDINDICES) : SV_Position { uint a = (v.x << 3) ^ (v.y >> 1) | (v.z & 0xff); return float4((float)a, (float)(v.w % 7u), 0, 1); }
