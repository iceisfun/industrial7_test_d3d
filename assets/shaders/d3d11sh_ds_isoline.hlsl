// d3d11_shaders corpus: target ds_5_0, entry main
struct HC { float3 p : POSITION; };
struct PC { float e[2] : SV_TessFactor; };
struct O { float4 pos : SV_Position; };
[domain("isoline")] O main(PC pc, float2 uv : SV_DomainLocation, const OutputPatch<HC, 2> patch) { O o; o.pos = float4(lerp(patch[0].p, patch[1].p, uv.x), 1); return o; }
