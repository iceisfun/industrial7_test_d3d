// d3d11_shaders corpus: target ds_5_0, entry main
struct HC { float3 p : POSITION; };
struct PC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };
struct O { float4 pos : SV_Position; };
[domain("tri")] O main(PC pc, float3 uvw : SV_DomainLocation, const OutputPatch<HC, 3> patch) { O o; o.pos = float4(patch[0].p * uvw.x + patch[1].p * uvw.y + patch[2].p * uvw.z, 1); return o; }
