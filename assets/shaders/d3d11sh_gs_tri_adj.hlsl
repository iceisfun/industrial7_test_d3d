// d3d11_shaders corpus: target gs_4_0, entry main
struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };
[maxvertexcount(3)] void main(triangleadj V i[6], inout TriangleStream<V> s) { s.Append(i[0]); s.Append(i[2]); s.Append(i[4]); }
