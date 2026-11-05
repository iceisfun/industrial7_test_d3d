// Samples Texture2D t0 with sampler s0. Entry: main
Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);
float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    return tex0.Sample(samp0, uv);
}
