// Mesh vertex shader: position/normal/uv with a world-view-projection matrix in b0. Entry: main
cbuffer Transform : register(b0) { float4x4 wvp; };
struct VSIn { float3 pos : POSITION; float3 nrm : NORMAL; float2 uv : TEXCOORD0; };
struct VSOut { float4 pos : SV_Position; float3 nrm : NORMAL; float2 uv : TEXCOORD0; };
VSOut main(VSIn i)
{
    VSOut o;
    o.pos = mul(float4(i.pos, 1), wvp);
    o.nrm = i.nrm;
    o.uv = i.uv;
    return o;
}
