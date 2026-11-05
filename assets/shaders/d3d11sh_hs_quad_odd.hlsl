// d3d11_shaders corpus: target hs_5_0, entry main
struct VI { float3 p : POSITION; };
struct PC { float e[4] : SV_TessFactor; float i[2] : SV_InsideTessFactor; };
PC pcf(InputPatch<VI, 4> ip) { PC o; o.e[0] = 2; o.e[1] = 2; o.e[2] = 2; o.e[3] = 2; o.i[0] = 3; o.i[1] = 3; return o; }
[domain("quad")][partitioning("fractional_odd")][outputtopology("triangle_cw")][outputcontrolpoints(4)][patchconstantfunc("pcf")]
VI main(InputPatch<VI, 4> ip, uint i : SV_OutputControlPointID) { return ip[i]; }
