// d3d11_shaders corpus: target ps_4_0, entry main
Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);
float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { return tex.Sample(smp, uv); }
