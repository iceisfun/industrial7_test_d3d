// d3d11_shaders corpus: target ps_4_0, entry main
float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { return float4(ddx(uv), ddy(uv)); }
