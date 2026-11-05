// Texture instructions: SampleLevel, SampleBias, SampleGrad, Load, Gather, GetDimensions. Entry: main
Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);
float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    uint w, h, levels;
    tex0.GetDimensions(0, w, h, levels);
    float4 a = tex0.SampleLevel(samp0, uv, 0);
    float4 b = tex0.SampleBias(samp0, uv, 0.5);
    float4 c = tex0.SampleGrad(samp0, uv, float2(0.01, 0), float2(0, 0.01));
    float4 d = tex0.Load(int3(uv * float2(w, h), 0));
    float4 e = tex0.GatherRed(samp0, uv);
    return (a + b + c + d + e) * 0.2 + float4(levels * 0.0, 0, 0, 0);
}
