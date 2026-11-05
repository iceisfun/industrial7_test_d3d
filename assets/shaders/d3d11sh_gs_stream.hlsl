// d3d11_shaders corpus: target gs_5_0, entry main
struct A { float4 p : SV_Position; };
struct B { float4 c : COLOR0; };
[maxvertexcount(2)] void main(point A i[1], inout PointStream<A> s0, inout PointStream<B> s1) { s0.Append(i[0]); B b; b.c = i[0].p; s1.Append(b); }
