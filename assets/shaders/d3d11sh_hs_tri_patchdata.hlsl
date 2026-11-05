// d3d11_shaders corpus: target hs_5_0, entry main
struct VI { float3 p : POSITION; };
struct PC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; float4 extra : TEXCOORD0; };
PC pcf(InputPatch<VI, 3> ip, uint pid : SV_PrimitiveID) { PC o; o.e[0] = 1; o.e[1] = 1; o.e[2] = 1; o.i = 1; o.extra = float4((float)pid, 0, 0, 0); return o; }
[domain("tri")][partitioning("integer")][outputtopology("triangle_cw")][outputcontrolpoints(3)][patchconstantfunc("pcf")][maxtessfactor(8.0)]
VI main(InputPatch<VI, 3> ip, uint i : SV_OutputControlPointID) { return ip[i]; }
