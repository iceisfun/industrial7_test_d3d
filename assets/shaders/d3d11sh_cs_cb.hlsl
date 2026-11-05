// d3d11_shaders corpus: target cs_5_0, entry main
cbuffer cb0 : register(b0) { uint4 params; };
RWStructuredBuffer<uint> outbuf : register(u0);
[numthreads(1, 1, 1)] void main() { outbuf[0] = params.x + params.y; }
