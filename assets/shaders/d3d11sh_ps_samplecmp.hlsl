// d3d11_shaders corpus: target ps_4_0, entry main
Texture2D<float> tex : register(t0);
SamplerComparisonState smp : register(s0);
float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { return tex.SampleCmp(smp, uv, 0.5f).xxxx; }
