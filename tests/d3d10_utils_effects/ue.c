#include "ue.h"

const char UE_VS_SRC[] =
    "struct VSI { float4 pos : POSITION; float2 uv : TEXCOORD0; float3 nrm : NORMAL; };\n"
    "struct VSO { float4 pos : SV_Position; float2 uv : TEXCOORD0; float3 nrm : TEXCOORD1; };\n"
    "float4x4 wvp;\n"
    "VSO main(VSI i) { VSO o; o.pos = mul(i.pos, wvp); o.uv = i.uv; o.nrm = i.nrm; return o; }\n";
const char UE_PS_SRC[] =
    "Texture2D tex0 : register(t0);\n"
    "SamplerState smp0 : register(s0);\n"
    "cbuffer cb0 : register(b0) { float4 tint; float scale; int count; };\n"
    "float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0, float3 nrm : TEXCOORD1) : SV_Target\n"
    "{ return tex0.Sample(smp0, uv) * tint * scale + count; }\n";
const char UE_GS_SRC[] =
    "struct GI { float4 pos : SV_Position; };\n"
    "[maxvertexcount(3)]\n"
    "void main(triangle GI i[3], inout TriangleStream<GI> s) { for (int k = 0; k < 3; k++) s.Append(i[k]); }\n";

const void *ue_bp(ID3D10Blob *b) { return b ? ID3D10Blob_GetBufferPointer(b) : NULL; }
SIZE_T ue_bs(ID3D10Blob *b) { return b ? ID3D10Blob_GetBufferSize(b) : 0; }

HRESULT ue_compile_fx(const char *src, UINT hf, UINT ff, ID3D10Blob **out)
{
    ue_CompileEffect_t f = (ue_CompileEffect_t)t_proc_in("d3d10.dll", "D3D10CompileEffectFromMemory");
    ID3D10Blob *err = NULL;
    *out = NULL;
    if (!f) return D3DH_E_NOCOMPILER;
    HRESULT hr = f((void *)src, strlen(src), "ue.fx", NULL, NULL, hf, ff, out, &err);
    if (FAILED(hr) && err) t_note("fx compile errors: %.300s", (const char *)ID3D10Blob_GetBufferPointer(err));
    SAFE_RELEASE(err);
    return hr;
}
HRESULT ue_compile_vs(const char *src, const char *entry, const char *profile, ID3D10Blob **out)
{
    ue_CompileShader_t f = (ue_CompileShader_t)t_proc_in("d3d10.dll", "D3D10CompileShader");
    ID3D10Blob *err = NULL;
    *out = NULL;
    if (!f) return D3DH_E_NOCOMPILER;
    HRESULT hr = f(src, strlen(src), "ue.hlsl", NULL, NULL, entry, profile, 0, out, &err);
    if (FAILED(hr) && err) t_note("shader compile errors: %.300s", (const char *)ID3D10Blob_GetBufferPointer(err));
    SAFE_RELEASE(err);
    return hr;
}
HRESULT ue_d3dc(const char *src, const char *entry, const char *target, UINT flags, ID3D10Blob **out)
{
    return d3dh_compile(src, entry, target, flags, out);
}
HRESULT ue_make_effect(const char *src, ID3D10Device *dev, ID3D10Effect **fx)
{
    ID3D10Blob *b = NULL;
    typedef HRESULT (WINAPI *cr_t)(void *, SIZE_T, UINT, ID3D10Device *, ID3D10EffectPool *, ID3D10Effect **);
    cr_t cr = (cr_t)t_proc_in("d3d10.dll", "D3D10CreateEffectFromMemory");
    *fx = NULL;
    if (!cr) return D3DH_E_NOCOMPILER;
    HRESULT hr = ue_compile_fx(src, 0, 0, &b);
    if (FAILED(hr)) return hr;
    hr = cr((void *)ue_bp(b), ue_bs(b), 0, dev, NULL, fx);
    SAFE_RELEASE(b);
    return hr;
}
const unsigned char *ue_dxbc_chunk(const void *dxbc, SIZE_T size, DWORD fourcc, DWORD *chunk_size)
{
    const unsigned char *p = dxbc;
    if (size < 32 || memcmp(p, "DXBC", 4)) return NULL;
    DWORD n = *(const DWORD *)(p + 28);
    for (DWORD i = 0; i < n && 32 + 4 * i + 4 <= size; i++) {
        DWORD off = *(const DWORD *)(p + 32 + 4 * i);
        if (off + 8 > size) return NULL;
        if (*(const DWORD *)(p + off) == fourcc) { if (chunk_size) *chunk_size = *(const DWORD *)(p + off + 4); return p + off + 8; }
    }
    return NULL;
}
DWORD ue_dxbc_nchunks(const void *dxbc) { return *(const DWORD *)((const unsigned char *)dxbc + 28); }
