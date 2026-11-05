// d3d11_shaders corpus: target cs_5_0, entry main
RWTexture2D<float4> img : register(u0);
[numthreads(4, 4, 1)] void main(uint3 id : SV_DispatchThreadID) { img[id.xy] = float4(1, 0, 0, 1); }
