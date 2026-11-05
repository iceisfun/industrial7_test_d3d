/* Local helpers for the d3d11_caps_formats area. */
#ifndef CF_H
#define CF_H
#include "d3dh.h"
#include "cf_formats.h"

/* Bits of the FORMAT_SUPPORT / FORMAT_SUPPORT2 words (spelled out so the tests read as a table). */
#define S_BUFFER      D3D11_FORMAT_SUPPORT_BUFFER
#define S_VB          D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER
#define S_IB          D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER
#define S_SO          D3D11_FORMAT_SUPPORT_SO_BUFFER
#define S_T1D         D3D11_FORMAT_SUPPORT_TEXTURE1D
#define S_T2D         D3D11_FORMAT_SUPPORT_TEXTURE2D
#define S_T3D         D3D11_FORMAT_SUPPORT_TEXTURE3D
#define S_CUBE        D3D11_FORMAT_SUPPORT_TEXTURECUBE
#define S_LOAD        D3D11_FORMAT_SUPPORT_SHADER_LOAD
#define S_SAMPLE      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE
#define S_CMP         D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON
#define S_MIP         D3D11_FORMAT_SUPPORT_MIP
#define S_AUTOGEN     D3D11_FORMAT_SUPPORT_MIP_AUTOGEN
#define S_RT          D3D11_FORMAT_SUPPORT_RENDER_TARGET
#define S_BLEND       D3D11_FORMAT_SUPPORT_BLENDABLE
#define S_DS          D3D11_FORMAT_SUPPORT_DEPTH_STENCIL
#define S_MSRESOLVE   D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE
#define S_DISPLAY     D3D11_FORMAT_SUPPORT_DISPLAY
#define S_CAST        D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT
#define S_MSRT        D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET
#define S_MSLOAD      D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD
#define S_GATHER      D3D11_FORMAT_SUPPORT_SHADER_GATHER
#define S_BBCAST      D3D11_FORMAT_SUPPORT_BACK_BUFFER_CAST
#define S_TUAV        D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW
#define S_GATHERCMP   D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON

#define CF_NFORMATS 133   /* ids 0..132 */

static const char *cf_fmt_name(UINT f)
{
    switch (f) {
#define X(n) case DXGI_FORMAT_##n: return #n;
    CF_FORMATS(X)
#undef X
    default: return "(undefined)";
    }
}
static int cf_is_typeless(UINT f) { const char *n = cf_fmt_name(f); size_t l = strlen(n); return l > 9 && !strcmp(n + l - 9, "_TYPELESS"); }
static int cf_is_bc(UINT f) { return f >= DXGI_FORMAT_BC1_TYPELESS && f <= DXGI_FORMAT_BC5_SNORM ? 1 : (f >= DXGI_FORMAT_BC6H_TYPELESS && f <= DXGI_FORMAT_BC7_UNORM_SRGB); }
static int cf_is_depth(UINT f) { return f == DXGI_FORMAT_D32_FLOAT || f == DXGI_FORMAT_D24_UNORM_S8_UINT || f == DXGI_FORMAT_D16_UNORM || f == DXGI_FORMAT_D32_FLOAT_S8X24_UINT; }
static int cf_is_video(UINT f)
{
    switch (f) {
    case DXGI_FORMAT_NV12: case DXGI_FORMAT_P010: case DXGI_FORMAT_P016: case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_YUY2: case DXGI_FORMAT_Y210: case DXGI_FORMAT_Y216: case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_AI44: case DXGI_FORMAT_IA44: case DXGI_FORMAT_P8: case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_AYUV: case DXGI_FORMAT_Y410: case DXGI_FORMAT_Y416:
    case 130: case 131: case 132: return 1;
    }
    return 0;
}
static int cf_is_defined(UINT f)
{
    switch (f) {
#define X(n) case DXGI_FORMAT_##n: return 1;
    CF_FORMATS(X)
#undef X
    }
    return 0;
}

/* Device at exactly the wanted feature level (WARP preferred); NULL when not obtainable. */
static ID3D11Device *cf_make(D3D_FEATURE_LEVEL want)
{
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL got = 0;
    HRESULT hr = d3dh_make_device11_fl(&d, NULL, &want, 1, &got, NULL, 0);
    if (FAILED(hr) || !d) return NULL;
    if (got != want) { ID3D11Device_Release(d); return NULL; }
    return d;
}
/* Highest-level default device for a test (caller releases). */
static ID3D11Device *cf_make_default(D3D_FEATURE_LEVEL *fl)
{
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL got = 0;
    HRESULT hr = d3dh_make_device11(&d, NULL, &got, NULL, 0);
    if (FAILED(hr) || !d) return NULL;
    if (fl) *fl = got;
    return d;
}
/* Process-wide shared default device (released at exit) for table-driven per-format tests. */
static ID3D11Device *g_cf_dev; static D3D_FEATURE_LEVEL g_cf_fl; static int g_cf_tried;
static void cf_shared_free(void) { if (g_cf_dev) { ID3D11Device_Release(g_cf_dev); g_cf_dev = NULL; } }
static ID3D11Device *cf_shared(D3D_FEATURE_LEVEL *fl)
{
    if (!g_cf_tried) { g_cf_tried = 1; g_cf_dev = cf_make_default(&g_cf_fl); if (g_cf_dev) atexit(cf_shared_free); }
    if (fl) *fl = g_cf_fl;
    return g_cf_dev;
}
#define NEED_DEV(d, flp) ID3D11Device *d = cf_shared(flp); if (!d) { t_skip("no D3D11 device"); return; }

/* Returns hr; *sup is pre-filled with a sentinel. */
static HRESULT cf_support(ID3D11Device *d, UINT f, UINT *sup)
{
    *sup = 0xdeadbeef;
    return ID3D11Device_CheckFormatSupport(d, (DXGI_FORMAT)f, sup);
}
static UINT cf_sup(ID3D11Device *d, UINT f) { UINT s; HRESULT hr = cf_support(d, f, &s); return SUCCEEDED(hr) ? s : 0; }
static UINT cf_sup2(ID3D11Device *d, UINT f)
{
    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 fd; fd.InFormat = (DXGI_FORMAT)f; fd.OutFormatSupport2 = 0;
    HRESULT hr = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_FORMAT_SUPPORT2, &fd, sizeof(fd));
    return SUCCEEDED(hr) ? fd.OutFormatSupport2 : 0;
}
static UINT cf_msq(ID3D11Device *d, UINT f, UINT count)
{
    UINT q = 0xdeadbeef;
    HRESULT hr = ID3D11Device_CheckMultisampleQualityLevels(d, (DXGI_FORMAT)f, count, &q);
    return SUCCEEDED(hr) ? q : 0;
}
static const D3D_FEATURE_LEVEL cf_levels[] = {
    D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1 };
#define CF_NLEVELS 7
static const char *cf_fl_name(D3D_FEATURE_LEVEL l)
{
    switch (l) { case D3D_FEATURE_LEVEL_9_1: return "9_1"; case D3D_FEATURE_LEVEL_9_2: return "9_2";
    case D3D_FEATURE_LEVEL_9_3: return "9_3"; case D3D_FEATURE_LEVEL_10_0: return "10_0";
    case D3D_FEATURE_LEVEL_10_1: return "10_1"; case D3D_FEATURE_LEVEL_11_0: return "11_0";
    case D3D_FEATURE_LEVEL_11_1: return "11_1"; default: return "?"; }
}
#endif
