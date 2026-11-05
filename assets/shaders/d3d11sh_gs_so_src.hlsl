// d3d11_shaders corpus: target gs_4_0, entry main
struct V { float4 pos : SV_Position; float3 nrm : NORMAL; float2 uv : TEXCOORD0; float4 col : COLOR0; };
[maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> s) { for (int k = 0; k < 3; k++) s.Append(i[k]); }
