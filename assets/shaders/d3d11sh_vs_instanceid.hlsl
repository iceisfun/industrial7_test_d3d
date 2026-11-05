// d3d11_shaders corpus: target vs_4_0, entry main
float4 main(float4 p : POSITION, uint inst : SV_InstanceID) : SV_Position { return p + float4((float)inst, 0, 0, 0); }
