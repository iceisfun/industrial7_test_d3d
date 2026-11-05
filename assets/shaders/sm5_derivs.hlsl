// Derivative intrinsics (fine / coarse). Entry: main
float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float2 a = float2(ddx(uv.x), ddy(uv.y));
    float2 b = float2(ddx_coarse(uv.x), ddy_coarse(uv.y));
    float2 c = float2(ddx_fine(uv.x), ddy_fine(uv.y));
    return float4(abs(a) * 100, abs(b + c) * 50);
}
