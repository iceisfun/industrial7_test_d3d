// d3d11_shaders corpus: target hs_5_0, entry main
struct VI { float3 p : POSITION; };
struct PC { float e[2] : SV_TessFactor; };
PC pcf(InputPatch<VI, 2> ip) { PC o; o.e[0] = 1; o.e[1] = 4; return o; }
[domain("isoline")][partitioning("integer")][outputtopology("line")][outputcontrolpoints(2)][patchconstantfunc("pcf")]
VI main(InputPatch<VI, 2> ip, uint i : SV_OutputControlPointID) { return ip[i]; }
