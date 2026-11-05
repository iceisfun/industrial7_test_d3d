// d3d11_shaders corpus: target gs_4_0, entry main
struct V { float4 p : SV_Position; };
struct O { float4 p : SV_Position; uint vp : SV_ViewportArrayIndex; };
[maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<O> s) { for (int k = 0; k < 3; k++) { O o; o.p = i[k].p; o.vp = 1; s.Append(o); } }
