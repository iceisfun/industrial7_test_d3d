// d3d11_shaders corpus: target vs_4_0, entry main
struct I { float3 p : POSITION; float3 n : NORMAL; float2 uv : TEXCOORD0; float4 c : COLOR0; };
struct O { float4 p : SV_Position; float3 n : NORMAL; float2 uv : TEXCOORD0; float4 c : COLOR0; };
O main(I i) { O o; o.p = float4(i.p, 1); o.n = i.n; o.uv = i.uv; o.c = i.c; return o; }
