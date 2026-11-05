// d3d11_shaders corpus: target gs_5_0, entry main
struct V { float4 p : SV_Position; };
[instance(4)][maxvertexcount(3)] void main(triangle V i[3], uint inst : SV_GSInstanceID, inout TriangleStream<V> s) { for (int k = 0; k < 3; k++) { V o = i[k]; o.p.x += inst; s.Append(o); } }
