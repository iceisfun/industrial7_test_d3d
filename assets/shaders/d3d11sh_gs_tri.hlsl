// d3d11_shaders corpus: target gs_4_0, entry main
struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };
[maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> s) { for (int k = 0; k < 3; k++) s.Append(i[k]); s.RestartStrip(); }
