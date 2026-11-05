// d3d11_shaders corpus: target ds_5_0, entry main
struct HC { float3 p : POSITION; };
struct PC { float e[4] : SV_TessFactor; float i[2] : SV_InsideTessFactor; };
struct O { float4 pos : SV_Position; };
[domain("quad")] O main(PC pc, float2 uv : SV_DomainLocation, const OutputPatch<HC, 4> patch) { O o; float3 a = lerp(patch[0].p, patch[1].p, uv.x); float3 b = lerp(patch[2].p, patch[3].p, uv.x); o.pos = float4(lerp(a, b, uv.y), 1); return o; }
