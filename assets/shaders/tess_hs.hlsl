// Triangle hull shader: fixed factor 4 (integer partitioning). Entry: main
struct CP { float3 pos : POSITION; };
struct PCF { float edges[3] : SV_TessFactor; float inside : SV_InsideTessFactor; };
PCF patchconst(InputPatch<CP, 3> p)
{
    PCF f;
    f.edges[0] = 4; f.edges[1] = 4; f.edges[2] = 4;
    f.inside = 4;
    return f;
}
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("patchconst")]
CP main(InputPatch<CP, 3> p, uint i : SV_OutputControlPointID)
{
    return p[i];
}
