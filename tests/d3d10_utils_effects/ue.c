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

/* Local-validation aid, used only when no real device exists AND an emulator runs the tests: a COM object with the ID3D10Device
 * layout that answers AddRef/Release/CreateBuffer and fails everything else. Never used on Windows. */
typedef struct { const void *const *vtbl; LONG rc; } fake_obj;
static HRESULT WINAPI fk_stub(void *self) { return E_NOTIMPL; }
static HRESULT WINAPI fk_qi(void *self, REFIID riid, void **out) { *out = self; ((fake_obj *)self)->rc++; return S_OK; }
static ULONG WINAPI fk_addref(void *self) { return ++((fake_obj *)self)->rc; }
static ULONG WINAPI fk_release(void *self) { return --((fake_obj *)self)->rc; }
static const void *g_fk_dev_vtbl[sizeof(ID3D10DeviceVtbl) / sizeof(void *)];
static const void *g_fk_buf_vtbl[sizeof(ID3D10BufferVtbl) / sizeof(void *)];
static fake_obj g_fk_dev = { g_fk_dev_vtbl, 1 };
static fake_obj g_fk_buf = { g_fk_buf_vtbl, 1 };
static HRESULT WINAPI fk_create_buffer(void *self, const D3D10_BUFFER_DESC *d, const D3D10_SUBRESOURCE_DATA *i, ID3D10Buffer **out) { g_fk_buf.rc++; *out = (ID3D10Buffer *)&g_fk_buf; return S_OK; }
static HRESULT WINAPI fk_create_obj(void *self, const void *d, void **out) { g_fk_buf.rc++; *out = &g_fk_buf; return S_OK; }
static ID3D10Device *fake_device(void)
{
    for (unsigned i = 0; i < sizeof g_fk_dev_vtbl / sizeof(void *); i++) g_fk_dev_vtbl[i] = fk_stub;
    for (unsigned i = 0; i < sizeof g_fk_buf_vtbl / sizeof(void *); i++) g_fk_buf_vtbl[i] = fk_stub;
    g_fk_dev_vtbl[0] = fk_qi; g_fk_dev_vtbl[1] = fk_addref; g_fk_dev_vtbl[2] = fk_release;
    g_fk_buf_vtbl[0] = fk_qi; g_fk_buf_vtbl[1] = fk_addref; g_fk_buf_vtbl[2] = fk_release;
    g_fk_dev_vtbl[offsetof(ID3D10DeviceVtbl, CreateBuffer) / sizeof(void *)] = fk_create_buffer;
    g_fk_dev_vtbl[offsetof(ID3D10DeviceVtbl, CreateBlendState) / sizeof(void *)] = fk_create_obj;
    g_fk_dev_vtbl[offsetof(ID3D10DeviceVtbl, CreateRasterizerState) / sizeof(void *)] = fk_create_obj;
    g_fk_dev_vtbl[offsetof(ID3D10DeviceVtbl, CreateDepthStencilState) / sizeof(void *)] = fk_create_obj;
    g_fk_dev_vtbl[offsetof(ID3D10DeviceVtbl, CreateSamplerState) / sizeof(void *)] = fk_create_obj;
    return (ID3D10Device *)&g_fk_dev;
}
static ID3D10Device *g_dev;
static int g_dev_tried;
static void dev_cleanup(void) { if (g_dev && g_dev != (ID3D10Device *)&g_fk_dev) { ID3D10Device_Release(g_dev); } g_dev = NULL; }
ID3D10Device *ue_shared_device(void)
{
    if (!g_dev_tried) {
        D3D10_DRIVER_TYPE dt;
        g_dev_tried = 1;
        if (SUCCEEDED(d3dh_make_device10(&g_dev, &dt, 0)) && g_dev) { atexit(dev_cleanup); t_note("shared D3D10 device driver type %d", (int)dt); }
        else g_dev = NULL;
        if (!g_dev && t_is_wine()) { g_dev = fake_device(); t_note("using the stand-in device (emulator, no real device)"); }
    }
    return g_dev;
}
int ue_fx_open(ID3D10Device *dev, const char *src, ID3D10Effect **fx)
{
    HRESULT hr = ue_make_effect(src, dev, fx);
    if (hr == D3DH_E_NOCOMPILER) { t_skip("effect entry points missing"); return 0; }
    if ((FAILED(hr) || !*fx) && t_is_wine()) { t_skip("effect not supported by the emulator (%s)", d3dh_hr_name(hr)); return 0; }
    if (FAILED(hr) || !*fx) {
        t_check(0, __FILE__, __LINE__, "effect compiles and loads", "%s", d3dh_hr_name(hr));
        return 0;
    }
    return 1;
}

const char UE_FX_BASIC[] =
    "cbuffer cb {\n"
    "    float4 vecA : MYSEM;\n"
    "    float fA = 1.5;\n"
    "    int iA = 3;\n"
    "    bool bA = true;\n"
    "    row_major float4x4 mA;\n"
    "    float arrA[3];\n"
    "    float3 v3;\n"
    "    int2 iv2;\n"
    "    float4 vArr[2];\n"
    "};\n"
    "technique10 T0 { pass P0 { } pass P1 { } }\n"
    "technique10 T1 { pass P0 { } }\n";
const char UE_FX_OBJECTS[] =
    "Texture2D tex;\n"
    "Texture2DArray texarr;\n"
    "Texture3D tex3;\n"
    "SamplerState smp { Filter = MIN_MAG_MIP_LINEAR; AddressU = Wrap; AddressV = Clamp; };\n"
    "BlendState bs { BlendEnable[0] = TRUE; SrcBlend = SRC_ALPHA; DestBlend = INV_SRC_ALPHA; };\n"
    "RasterizerState rs { CullMode = None; FillMode = Wireframe; };\n"
    "DepthStencilState dss { DepthEnable = FALSE; };\n"
    "RenderTargetView rtv;\n"
    "DepthStencilView dsv;\n"
    "float4 gcol = float4(1, 2, 3, 4);\n"
    "float gscalar = 2.5;\n"
    "technique10 TObj { pass P0 {\n"
    "    SetBlendState(bs, float4(0, 0, 0, 0), 0xFFFFFFFF);\n"
    "    SetRasterizerState(rs);\n"
    "    SetDepthStencilState(dss, 0);\n"
    "} }\n";
const char UE_FX_SHADERS[] =
    "Texture2D tex;\n"
    "SamplerState smp;\n"
    "float4 gcol = float4(1, 2, 3, 4);\n"
    "float4 VS(float4 p : POSITION, float2 uv : TEXCOORD0) : SV_Position { return p + gcol; }\n"
    "float4 PS(float4 p : SV_Position) : SV_Target { return tex.Sample(smp, p.xy) * gcol; }\n"
    "technique10 TSh { pass P0 {\n"
    "    SetVertexShader(CompileShader(vs_4_0, VS()));\n"
    "    SetGeometryShader(NULL);\n"
    "    SetPixelShader(CompileShader(ps_4_0, PS()));\n"
    "} }\n";
const char UE_FX_STRING[] =
    "string title = \"hello\";\n"
    "string names[2] = { \"one\", \"two\" };\n"
    "technique10 T { pass P { } }\n";

const char UE_FX_POOL[] =
    "shared cbuffer shared_cb { float4 shared_vec; float shared_f = 2; };\n"
    "shared Texture2D shared_tex;\n"
    "technique10 TPool { pass P { } }\n";
const char UE_FX_CHILD[] =
    "shared cbuffer shared_cb { float4 shared_vec; float shared_f; };\n"
    "shared Texture2D shared_tex;\n"
    "cbuffer own { float4 own_vec; float own_f = 7; };\n"
    "technique10 TChild { pass P { } }\n";

int ue_device_is_fake(void) { return g_dev == (ID3D10Device *)&g_fk_dev; }
