// d3d11_shaders corpus: target vs_4_0, entry main
Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);
float4 main(float2 uv : TEXCOORD0) : SV_Position { return tex.SampleLevel(smp, uv, 0); }
