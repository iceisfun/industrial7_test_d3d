// Structured buffer compute: out[i] = in[i] * scale + bias. t0 = input, u0 = output. Entry: main
struct Elem { float4 v; uint id; float3 pad; };
StructuredBuffer<Elem> src : register(t0);
RWStructuredBuffer<Elem> dst : register(u0);
cbuffer Params : register(b0) { float scale; float bias; uint count; uint pad; };
[numthreads(16, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x >= count) return;
    Elem e = src[dtid.x];
    e.v = e.v * scale + bias;
    e.id = dtid.x;
    dst[dtid.x] = e;
}
