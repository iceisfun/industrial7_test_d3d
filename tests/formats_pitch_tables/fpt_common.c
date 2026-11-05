#include "fpt.h"

const fpt_fmt fpt_formats[FPT_NFORMATS] = {
#define X(id, nm, grp, bpb, bw, bh) [id] = { id, #nm, grp, bpb, bw, bh },
    FPT_TABLE(X)
#undef X
};
const UINT fpt_widths[FPT_NW] = { 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 64 };

static ID3D11Device *g_d11; static ID3D11DeviceContext *g_c11; static D3D_FEATURE_LEVEL g_fl11; static int g_t11;
static ID3D10Device *g_d10; static int g_t10;
static ID3D12Device *g_d12; static int g_t12;
static IDirect3D9 *g_d3d9; static IDirect3DDevice9 *g_d9; static HWND g_hw9; static int g_t9;
static int g_atexit;

static void fpt_cleanup(void)
{
    SAFE_RELEASE(g_c11); SAFE_RELEASE(g_d11);
    SAFE_RELEASE(g_d10);
    SAFE_RELEASE(g_d12);
    SAFE_RELEASE(g_d9); SAFE_RELEASE(g_d3d9);
    if (g_hw9) { d3dh_destroy_window(g_hw9); g_hw9 = NULL; }
}
static void reg(void) { if (!g_atexit) { g_atexit = 1; atexit(fpt_cleanup); } }

ID3D11Device *fpt_dev11(ID3D11DeviceContext **ctx, D3D_FEATURE_LEVEL *fl)
{
    if (!g_t11) {
        g_t11 = 1; reg();
        if (!d3dh_D3D11CreateDevice()) return NULL;
        if (FAILED(d3dh_make_device11(&g_d11, &g_c11, &g_fl11, NULL, 0))) { g_d11 = NULL; g_c11 = NULL; }
    }
    if (ctx) *ctx = g_c11;
    if (fl) *fl = g_fl11;
    return g_d11;
}
ID3D10Device *fpt_dev10(void)
{
    if (!g_t10) {
        g_t10 = 1; reg();
        if (!d3dh_D3D10CreateDevice()) return NULL;
        if (FAILED(d3dh_make_device10(&g_d10, NULL, 0))) g_d10 = NULL;
    }
    return g_d10;
}
ID3D12Device *fpt_dev12(void)
{
    if (!g_t12) {
        g_t12 = 1; reg();
        if (!d3dh_D3D12CreateDevice()) return NULL;
        if (t_is_wine()) return NULL;
        if (FAILED(d3dh_make_device12(&g_d12, D3D_FEATURE_LEVEL_11_0, NULL))) g_d12 = NULL;
    }
    return g_d12;
}
IDirect3DDevice9 *fpt_dev9(IDirect3D9 **d3d)
{
    if (!g_t9) {
        g_t9 = 1; reg();
        if (!d3dh_Direct3DCreate9()) return NULL;
        if (FAILED(d3dh_make_device9(&g_d3d9, &g_d9, NULL, &g_hw9, NULL))) { g_d9 = NULL; g_d3d9 = NULL; }
    }
    if (d3d) *d3d = g_d3d9;
    return g_d9;
}
UINT fpt_support11(ID3D11Device *dev, UINT fmt, HRESULT *hr_out)
{
    UINT s = 0;
    HRESULT hr = ID3D11Device_CheckFormatSupport(dev, (DXGI_FORMAT)fmt, &s);
    if (hr_out) *hr_out = hr;
    return FAILED(hr) ? 0 : s;
}
