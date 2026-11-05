// d3d11_shaders corpus: target gs_4_0, entry main
struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };
[maxvertexcount(2)] void main(lineadj V i[4], inout LineStream<V> s) { s.Append(i[1]); s.Append(i[2]); }
