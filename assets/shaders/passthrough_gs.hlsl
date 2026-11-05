// Triangle passthrough geometry shader. Entry: main
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
[maxvertexcount(3)]
void main(triangle V input[3], inout TriangleStream<V> stream)
{
    for (uint i = 0; i < 3; i++)
        stream.Append(input[i]);
    stream.RestartStrip();
}
