// d3d11_shaders corpus: target hs_5_0, entry main
struct VI { float3 p : POSITION; };
struct PC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };
PC pcf(InputPatch<VI, 3> ip) { PC o; o.e[0] = 4; o.e[1] = 4; o.e[2] = 4; o.i = 4; return o; }
[domain("tri")][partitioning("pow2")][outputtopology("triangle_ccw")][outputcontrolpoints(3)][patchconstantfunc("pcf")]
VI main(InputPatch<VI, 3> ip, uint i : SV_OutputControlPointID) { return ip[i]; }
