/* More D3D11CreateDeviceAndSwapChain coverage: driver-type argument matrix, swap-chain description validation
 * and repeated create/destroy. State-changing (real devices, swap chains on hidden windows). */
#include "dxgap0.h"

typedef HRESULT (WINAPI *cdsc11_t)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
    const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);

static void mk_desc(DXGI_SWAP_CHAIN_DESC *d, HWND wnd, UINT w, UINT h, DXGI_FORMAT fmt, UINT bufs)
{
    memset(d, 0, sizeof *d);
    d->BufferDesc.Width = w; d->BufferDesc.Height = h; d->BufferDesc.Format = fmt;
    d->BufferDesc.RefreshRate.Numerator = 60; d->BufferDesc.RefreshRate.Denominator = 1;
    d->SampleDesc.Count = 1; d->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d->BufferCount = bufs;
    d->OutputWindow = wnd; d->Windowed = TRUE; d->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
}
static void drop(IDXGISwapChain *sc, ID3D11Device *dev, ID3D11DeviceContext *ctx)
{
    if (sc && sc != SENT_SC) IDXGISwapChain_Release(sc);
    if (ctx && ctx != SENT_CTX) ID3D11DeviceContext_Release(ctx);
    if (dev && dev != SENT_DEV) ID3D11Device_Release(dev);
}

/* Driver type argument rules that do not depend on the installed adapters. */
TEST(dxgap0_cdsc11_driver_type_argument_rules)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    HWND wnd; DXGI_SWAP_CHAIN_DESC sd; IDXGISwapChain *sc; ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl; HRESULT hr;
    IDXGIFactory1 *fac = NULL; IDXGIAdapter *ad = NULL;
    REQUIRE(fn);
    wnd = d3dh_create_hidden_window_sized(64, 64); mk_desc(&sd, wnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
#define TRY(drv, adapter, mod, what) do { sc = SENT_SC; dev = SENT_DEV; ctx = SENT_CTX; fl = 0; \
        hr = fn(adapter, drv, mod, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx); } while (0)
    TRY(D3D_DRIVER_TYPE_UNKNOWN, NULL, NULL, "unknown");
    EXPECT_HR(hr, E_INVALIDARG); CHECK(sc == NULL && dev == NULL && ctx == NULL); drop(sc, dev, ctx);
    TRY(D3D_DRIVER_TYPE_SOFTWARE, NULL, NULL, "software");
    EXPECT_HR(hr, E_INVALIDARG); drop(sc, dev, ctx);
    TRY((D3D_DRIVER_TYPE)99, NULL, NULL, "bad");
    EXPECT_HR(hr, E_INVALIDARG); drop(sc, dev, ctx);
    TRY(D3D_DRIVER_TYPE_NULL, NULL, NULL, "null");
    EXPECT_HR(hr, S_OK); drop(sc, dev, ctx);   /* Win10: the NULL driver creates a non-rendering device */
    /* an explicit adapter requires D3D_DRIVER_TYPE_UNKNOWN; every other type rejects it */
    if (d3dh_CreateDXGIFactory1() && SUCCEEDED(d3dh_CreateDXGIFactory1()(&IID_IDXGIFactory1, (void **)&fac)) && SUCCEEDED(IDXGIFactory1_EnumAdapters(fac, 0, &ad))) {
        TRY(D3D_DRIVER_TYPE_HARDWARE, ad, NULL, "hw+adapter");
        EXPECT_HR(hr, E_INVALIDARG); drop(sc, dev, ctx);
        TRY(D3D_DRIVER_TYPE_WARP, ad, NULL, "warp+adapter");
        EXPECT_HR(hr, E_INVALIDARG); drop(sc, dev, ctx);
        TRY(D3D_DRIVER_TYPE_UNKNOWN, ad, NULL, "unknown+adapter");
        EXPECT_HR(hr, S_OK); CHECK(dev != NULL && dev != SENT_DEV); drop(sc, dev, ctx);
        IDXGIAdapter_Release(ad);
    } else t_skip("no DXGI adapter");
    if (fac) IDXGIFactory1_Release(fac);
    TRY(D3D_DRIVER_TYPE_WARP, NULL, NULL, "warp");
    EXPECT_HR(hr, S_OK); if (SUCCEEDED(hr)) { CHECK(fl >= D3D_FEATURE_LEVEL_10_0); t_note("WARP feature level 0x%x", fl); } drop(sc, dev, ctx);
    TRY(D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, "hardware");
    t_note("DRIVER_TYPE_HARDWARE: %s fl=0x%x", d3dh_hr_name(hr), fl); drop(sc, dev, ctx);
    TRY(D3D_DRIVER_TYPE_REFERENCE, NULL, NULL, "reference");
    t_note("DRIVER_TYPE_REFERENCE: %s", d3dh_hr_name(hr)); drop(sc, dev, ctx);
#undef TRY
    d3dh_destroy_window(wnd);
}

/* Swap-chain description validation: each row is created on a WARP device. */
TEST(dxgap0_cdsc11_swapchain_desc_validation)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    HWND wnd; DXGI_SWAP_CHAIN_DESC sd; IDXGISwapChain *sc; ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl; HRESULT hr;
    REQUIRE(fn);
    wnd = d3dh_create_hidden_window_sized(96, 64);
#define ROW(name, want, stmt) { DXGI_SWAP_CHAIN_DESC s_; mk_desc(&s_, wnd, 96, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1); { DXGI_SWAP_CHAIN_DESC *d = &s_; stmt; } \
        sc = SENT_SC; dev = SENT_DEV; ctx = SENT_CTX; \
        hr = GAP0_CALL(fn(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &s_, &sc, &dev, &fl, &ctx)); \
        EXPECT_HR(hr, want); \
        if (FAILED(hr) && hr != GAP0_AVHR) CHECKF(sc == NULL && dev == NULL && ctx == NULL, "'%s': failed creation must clear the outputs", name); \
        drop(sc, dev, ctx); }
    ROW("baseline", S_OK, (void)d);
    ROW("format UNKNOWN", E_INVALIDARG_HR, d->BufferDesc.Format = DXGI_FORMAT_UNKNOWN);
    ROW("BufferCount 0", GAP0_AVHR, d->BufferCount = 0);
    ROW("BufferCount 17", GAP0_AVHR, d->BufferCount = 17);
    ROW("size 0x0 (take from window)", S_OK, d->BufferDesc.Width = 0; d->BufferDesc.Height = 0);
    ROW("SampleDesc.Count 0", E_INVALIDARG_HR, d->SampleDesc.Count = 0);
    ROW("SampleDesc.Count 4 discard", S_OK, d->SampleDesc.Count = 4);
    ROW("invalid window handle", S_OK, d->OutputWindow = (HWND)(ULONG_PTR)0x12345678);
    ROW("usage 0", S_OK, d->BufferUsage = 0);
    ROW("FLIP_SEQUENTIAL 2 buffers", S_OK, d->SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL; d->BufferCount = 2);
    ROW("FLIP_DISCARD windowed", S_OK, d->SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; d->BufferCount = 2);
    ROW("bad swap effect 9", DXGI_ERROR_INVALID_CALL, d->SwapEffect = (DXGI_SWAP_EFFECT)9);
    ROW("B8G8R8A8", S_OK, d->BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM);
    ROW("R16G16B16A16_FLOAT", S_OK, d->BufferDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT);
    ROW("R10G10B10A2", S_OK, d->BufferDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM);
    ROW("depth format D32_FLOAT", E_INVALIDARG_HR, d->BufferDesc.Format = DXGI_FORMAT_D32_FLOAT);
    ROW("compressed BC1", E_INVALIDARG_HR, d->BufferDesc.Format = DXGI_FORMAT_BC1_UNORM);
#undef ROW
    d3dh_destroy_window(wnd);
}

/* A hundred device+swap-chain create/destroy cycles on one window neither fail nor leak handles. */
TEST(dxgap0_cdsc11_create_destroy_cycles)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    HWND wnd; DXGI_SWAP_CHAIN_DESC sd; DWORD h0, h1; int fails = 0;
    REQUIRE(fn);
    wnd = d3dh_create_hidden_window_sized(64, 64);
    for (int i = 0; i < 100; i++) {
        IDXGISwapChain *sc = NULL; ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl;
        mk_desc(&sd, wnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
        if (i == 5) GetProcessHandleCount(GetCurrentProcess(), &h0);       /* after warm-up */
        if (FAILED(fn(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx))) { fails++; continue; }
        if (IDXGISwapChain_Present(sc, 0, 0) != S_OK) fails++;
        if (IDXGISwapChain_Release(sc) != 0) fails++;
        ID3D11DeviceContext_Release(ctx);
        if (ID3D11Device_Release(dev) != 0) fails++;
    }
    GetProcessHandleCount(GetCurrentProcess(), &h1);
    EXPECT_EQ(fails, 0);
    CHECKF(h1 <= h0 + 8, "handle count grew from %lu to %lu over 95 cycles", h0, h1);
    d3dh_destroy_window(wnd);
}
