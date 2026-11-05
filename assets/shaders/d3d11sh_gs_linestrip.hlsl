// d3d11_shaders corpus: target gs_4_0, entry main
struct V { float4 p : SV_Position; };
[maxvertexcount(4)] void main(triangle V i[3], inout LineStream<V> s) { s.Append(i[0]); s.Append(i[1]); s.Append(i[2]); s.Append(i[0]); }
