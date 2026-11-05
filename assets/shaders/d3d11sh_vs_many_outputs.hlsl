// d3d11_shaders corpus: target vs_4_0, entry main
struct O { float4 p : SV_Position; float4 t0 : TEXCOORD0; float4 t1 : TEXCOORD1; float4 t2 : TEXCOORD2; float4 t3 : TEXCOORD3; float4 t4 : TEXCOORD4; float4 t5 : TEXCOORD5; float4 t6 : TEXCOORD6; float4 t7 : TEXCOORD7; };
O main(float4 p : POSITION) { O o; o.p = p; o.t0 = p; o.t1 = p.yzwx; o.t2 = p.zwxy; o.t3 = p.wxyz; o.t4 = p; o.t5 = p; o.t6 = p; o.t7 = p; return o; }
