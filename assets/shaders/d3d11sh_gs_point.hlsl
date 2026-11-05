// d3d11_shaders corpus: target gs_4_0, entry main
struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };
[maxvertexcount(1)] void main(point V i[1], inout PointStream<V> s) { s.Append(i[0]); }
