// Triangle domain shader: barycentric interpolation of the patch corners. Entry: main
struct CP { float3 pos : POSITION; };
struct PCF { float edges[3] : SV_TessFactor; float inside : SV_InsideTessFactor; };
struct DSOut { float4 pos : SV_Position; };
[domain("tri")]
DSOut main(PCF f, float3 bary : SV_DomainLocation, const OutputPatch<CP, 3> p)
{
    DSOut o;
    float3 w = p[0].pos * bary.x + p[1].pos * bary.y + p[2].pos * bary.z;
    o.pos = float4(w, 1);
    return o;
}
