// d3d11_shaders corpus: target vs_4_0, entry main
struct V { float4 pos : SV_Position; float3 nrm : NORMAL; float2 uv : TEXCOORD0; float4 col : COLOR0; };
V main(float4 p : POSITION) { V o; o.pos = p; o.nrm = p.xyz; o.uv = p.xy; o.col = p; return o; }
