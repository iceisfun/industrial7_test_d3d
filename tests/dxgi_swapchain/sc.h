/* Shared helpers for the dxgi_swapchain area. Everything is reached through the d3dh loaders
 * (LoadLibrary -> GetProcAddress); DXGI/D3D11/D3D12 are never linked. */
#ifndef SC_H
#define SC_H
#include "d3dh.h"
#include <dxgi1_5.h>
#include <stdarg.h>

#ifndef DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
#define DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING 0x800
#endif

/* All flags a swap chain description may carry (DXGI 1.5). */
#define SC_ALL_FLAGS 0x1FFF
/* A value measured on Windows 10 and pinned: fails when the HRESULT differs. */
#define PIN_HR(text, hr, want) do { HRESULT ph_ = (HRESULT)(hr), pw_ = (HRESULT)(want); \
    t_check(ph_ == pw_, __FILE__, __LINE__, text, "%s -> %s want %s", text, d3dh_hr_name(ph_), d3dh_hr_name(pw_)); } while (0)
/* Golden HRESULTs for formatted descriptions (sc_pins.inc, measured on Win10 22H2): PINF(hr, "scaling %d effect %d", a, b) must equal the table entry. */
static const struct { const char *k, *v; } k_sc_pins[] = {
#include "sc_pins.inc"
};
static void sc_pinf(const char *file, int line, HRESULT hr, const char *fmt, ...)
{
    char key[256]; va_list ap; unsigned i;
    va_start(ap, fmt); vsnprintf(key, sizeof key, fmt, ap); va_end(ap);
    for (i = 0; i < sizeof k_sc_pins / sizeof *k_sc_pins; i++)
        if (!strcmp(key, k_sc_pins[i].k)) { t_check(!strcmp(d3dh_hr_name(hr), k_sc_pins[i].v), file, line, key, "%s -> %s want %s", key, d3dh_hr_name(hr), k_sc_pins[i].v); return; }
    t_check(0, file, line, key, "no pinned HRESULT for \"%s\" (got %s)", key, d3dh_hr_name(hr));
}
#define PINF(hr, ...) sc_pinf(__FILE__, __LINE__, (hr), __VA_ARGS__)
#define ARRAY_SIZE_(a) (sizeof(a) / sizeof((a)[0]))

typedef struct SC {
    ID3D11Device        *dev;
    ID3D11DeviceContext *ctx;
    D3D_DRIVER_TYPE      drv;
    IDXGIFactory        *fac;
    IDXGIFactory2       *fac2;
    IUnknown            *unk;      /* the device as IUnknown (what CreateSwapChain takes) */
    HWND                 hwnd;
} SC;

/* Opens a D3D11 device (WARP first) and its factory. Returns 1 on success; otherwise calls t_skip and returns 0. */
static int sc_open(SC *s, UINT devflags)
{
    memset(s, 0, sizeof *s);
    if (!d3dh_D3D11CreateDevice() || !d3dh_CreateDXGIFactory1()) { t_skip("d3d11/dxgi not available"); return 0; }
    HRESULT hr = d3dh_make_device11(&s->dev, &s->ctx, NULL, &s->drv, devflags);
    if (FAILED(hr)) { t_skip("no D3D11 device (%s)", d3dh_hr_name(hr)); return 0; }
    if (s->drv == D3D_DRIVER_TYPE_NULL) {
        t_skip("only the NULL driver is available");
        SAFE_RELEASE(s->ctx); SAFE_RELEASE(s->dev);
        return 0;
    }
    IDXGIDevice *dd = NULL; IDXGIAdapter *ad = NULL;
    hr = ID3D11Device_QueryInterface(s->dev, &IID_IDXGIDevice, (void **)&dd);
    if (SUCCEEDED(hr)) hr = IDXGIDevice_GetAdapter(dd, &ad);
    if (SUCCEEDED(hr)) hr = IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory, (void **)&s->fac);
    SAFE_RELEASE(ad); SAFE_RELEASE(dd);
    if (FAILED(hr)) { t_skip("no factory (%s)", d3dh_hr_name(hr)); SAFE_RELEASE(s->ctx); SAFE_RELEASE(s->dev); return 0; }
    IDXGIFactory_QueryInterface(s->fac, &IID_IDXGIFactory2, (void **)&s->fac2);
    ID3D11Device_QueryInterface(s->dev, &IID_IUnknown, (void **)&s->unk);
    s->hwnd = d3dh_create_hidden_window_sized(128, 96);
    if (!s->hwnd) {
        t_skip("cannot create a window");
        SAFE_RELEASE(s->unk); SAFE_RELEASE(s->fac2); SAFE_RELEASE(s->fac); SAFE_RELEASE(s->ctx); SAFE_RELEASE(s->dev);
        return 0;
    }
    return 1;
}

/* Like sc_open but also requires IDXGIFactory2 (Win8+ / always present on Win10). */
static int sc_open2(SC *s, UINT devflags)
{
    if (!sc_open(s, devflags)) return 0;
    if (!s->fac2) {
        t_skip("IDXGIFactory2 missing");
        d3dh_destroy_window(s->hwnd);
        SAFE_RELEASE(s->unk); SAFE_RELEASE(s->fac); SAFE_RELEASE(s->ctx); SAFE_RELEASE(s->dev);
        return 0;
    }
    return 1;
}

static void sc_close(SC *s)
{
    if (s->ctx) ID3D11DeviceContext_ClearState(s->ctx);
    if (s->ctx) ID3D11DeviceContext_Flush(s->ctx);
    SAFE_RELEASE(s->unk); SAFE_RELEASE(s->fac2); SAFE_RELEASE(s->fac);
    SAFE_RELEASE(s->ctx); SAFE_RELEASE(s->dev);
    d3dh_destroy_window(s->hwnd);
    s->hwnd = NULL;
}

/* Fresh hidden window (caller destroys). */
static HWND sc_window(int w, int h) { return d3dh_create_hidden_window_sized(w, h); }

static DXGI_SWAP_CHAIN_DESC sc_desc0(HWND hwnd, UINT w, UINT h, DXGI_FORMAT f, UINT count, DXGI_SWAP_EFFECT eff)
{
    DXGI_SWAP_CHAIN_DESC d; memset(&d, 0, sizeof d);
    d.BufferDesc.Width = w; d.BufferDesc.Height = h; d.BufferDesc.Format = f;
    d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount = count; d.OutputWindow = hwnd; d.Windowed = TRUE; d.SwapEffect = eff;
    return d;
}

static DXGI_SWAP_CHAIN_DESC1 sc_desc1(UINT w, UINT h, DXGI_FORMAT f, UINT count, DXGI_SWAP_EFFECT eff)
{
    DXGI_SWAP_CHAIN_DESC1 d; memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.Format = f; d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = count;
    d.Scaling = DXGI_SCALING_STRETCH; d.SwapEffect = eff; d.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    return d;
}

static int sc_is_flip(DXGI_SWAP_EFFECT e) { return e == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL || e == DXGI_SWAP_EFFECT_FLIP_DISCARD; }

/* An S_OK or an S_OK-like status (windowed creation can report DXGI_STATUS_OCCLUDED on some hosts). */
static int sc_okish(HRESULT hr) { return hr == S_OK || hr == DXGI_STATUS_OCCLUDED; }

/* Create a legacy swap chain; returns hr. */
static HRESULT sc_create0(SC *s, const DXGI_SWAP_CHAIN_DESC *d, IDXGISwapChain **out)
{
    DXGI_SWAP_CHAIN_DESC copy = *d;
    *out = NULL;
    return IDXGIFactory_CreateSwapChain(s->fac, s->unk, &copy, out);
}

/* Create a SwapChain1 for the HWND; returns hr. */
static HRESULT sc_create1(SC *s, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1 *d, IDXGISwapChain1 **out)
{
    *out = NULL;
    return IDXGIFactory2_CreateSwapChainForHwnd(s->fac2, s->unk, hwnd, d, NULL, NULL, out);
}

/* Convenience: default flip/bitblt chain on s->hwnd. Returns NULL (with a failed CHECK) on failure. */
static IDXGISwapChain1 *sc_mk(SC *s, DXGI_FORMAT f, UINT count, DXGI_SWAP_EFFECT eff, UINT flags)
{
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(0, 0, f, count, eff);
    d.Flags = flags;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = sc_create1(s, s->hwnd, &d, &sc);
    if (!sc_okish(hr)) { CHECKF(0, "swap chain creation failed: %s", d3dh_hr_name(hr)); return NULL; }
    return sc;
}

/* Clear back buffer `idx` (0 only for bitblt/flip-discard writes) to the colour and flush. Returns hr. */
static HRESULT sc_clear(SC *s, IDXGISwapChain *sc, UINT idx, const float rgba[4], DXGI_FORMAT view_fmt)
{
    ID3D11Texture2D *tex = NULL; ID3D11RenderTargetView *rtv = NULL;
    HRESULT hr = IDXGISwapChain_GetBuffer(sc, idx, &IID_ID3D11Texture2D, (void **)&tex);
    if (FAILED(hr)) return hr;
    D3D11_RENDER_TARGET_VIEW_DESC rd; memset(&rd, 0, sizeof rd);
    rd.Format = view_fmt; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = ID3D11Device_CreateRenderTargetView(s->dev, (ID3D11Resource *)tex, view_fmt ? &rd : NULL, &rtv);
    if (SUCCEEDED(hr)) {
        ID3D11DeviceContext_ClearRenderTargetView(s->ctx, rtv, rgba);
        ID3D11DeviceContext_Flush(s->ctx);
        SAFE_RELEASE(rtv);
    }
    SAFE_RELEASE(tex);
    return hr;
}

static HRESULT sc_readback(SC *s, IDXGISwapChain *sc, UINT idx, d3dh_image *im)
{
    ID3D11Texture2D *tex = NULL;
    HRESULT hr = IDXGISwapChain_GetBuffer(sc, idx, &IID_ID3D11Texture2D, (void **)&tex);
    if (FAILED(hr)) return hr;
    hr = d3dh_readback_texture2d11(s->dev, s->ctx, tex, 0, im);
    SAFE_RELEASE(tex);
    return hr;
}

/* Release a swap chain and check that the last reference was dropped. */
#define SC_REL(p) do { if (p) { ULONG rc_ = (p)->lpVtbl->Release(p); CHECKF(rc_ == 0, "leaked %lu refs on " #p, rc_); (p) = NULL; } } while (0)

/* ---- D3D12 ---------------------------------------------------------------- */
typedef struct SC12 {
    ID3D12Device       *dev;
    ID3D12CommandQueue *q;
    IDXGIFactory2      *fac2;
    HWND                hwnd;
} SC12;

static int sc12_open(SC12 *s)
{
    memset(s, 0, sizeof *s);
    if (!d3dh_D3D12CreateDevice() || !d3dh_CreateDXGIFactory1()) { t_skip("d3d12 not available"); return 0; }
    HRESULT hr = d3dh_make_device12(&s->dev, D3D_FEATURE_LEVEL_11_0, NULL);
    if (FAILED(hr)) { t_skip("no D3D12 device (%s)", d3dh_hr_name(hr)); return 0; }
    D3D12_COMMAND_QUEUE_DESC qd; memset(&qd, 0, sizeof qd); qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = ID3D12Device_CreateCommandQueue(s->dev, &qd, &IID_ID3D12CommandQueue, (void **)&s->q);
    if (FAILED(hr)) { t_skip("no D3D12 queue"); SAFE_RELEASE(s->dev); return 0; }
    IDXGIFactory1 *f1 = NULL;
    hr = d3dh_CreateDXGIFactory1()(&IID_IDXGIFactory1, (void **)&f1);
    if (SUCCEEDED(hr)) { hr = IDXGIFactory1_QueryInterface(f1, &IID_IDXGIFactory2, (void **)&s->fac2); SAFE_RELEASE(f1); }
    if (FAILED(hr)) { t_skip("no IDXGIFactory2"); SAFE_RELEASE(s->q); SAFE_RELEASE(s->dev); return 0; }
    s->hwnd = d3dh_create_hidden_window_sized(128, 96);
    if (!s->hwnd) { t_skip("no window"); SAFE_RELEASE(s->fac2); SAFE_RELEASE(s->q); SAFE_RELEASE(s->dev); return 0; }
    return 1;
}

static void sc12_close(SC12 *s)
{
    if (s->dev && s->q) d3dh_d12_flush(s->dev, s->q);
    SAFE_RELEASE(s->fac2); SAFE_RELEASE(s->q); SAFE_RELEASE(s->dev);
    d3dh_destroy_window(s->hwnd); s->hwnd = NULL;
}

static HRESULT sc12_create(SC12 *s, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1 *d, IDXGISwapChain1 **out)
{
    *out = NULL;
    return IDXGIFactory2_CreateSwapChainForHwnd(s->fac2, (IUnknown *)s->q, hwnd, d, NULL, NULL, out);
}
#endif
