/* Device + swap-chain creation exports (D3D11CreateDeviceAndSwapChain, D3D10CreateDeviceAndSwapChain1).
 * State-changing: real WARP devices and swap chains on a hidden window. */
#include "dxgap0.h"

typedef HRESULT (WINAPI *cdsc11_t)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
    const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
typedef HRESULT (WINAPI *cdsc10_t)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT,
    DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D10Device1 **);

static void make_desc(DXGI_SWAP_CHAIN_DESC *d, HWND wnd, UINT w, UINT h, DXGI_FORMAT fmt, UINT bufs)
{
    memset(d, 0, sizeof *d);
    d->BufferDesc.Width = w; d->BufferDesc.Height = h; d->BufferDesc.Format = fmt;
    d->BufferDesc.RefreshRate.Numerator = 60; d->BufferDesc.RefreshRate.Denominator = 1;
    d->SampleDesc.Count = 1;
    d->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d->BufferCount = bufs;
    d->OutputWindow = wnd;
    d->Windowed = TRUE;
    d->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
}

/* try WARP, then REFERENCE */
static HRESULT make11(cdsc11_t fn, UINT flags, const D3D_FEATURE_LEVEL *fls, UINT nfl, const DXGI_SWAP_CHAIN_DESC *sd,
                      IDXGISwapChain **sc, ID3D11Device **dev, D3D_FEATURE_LEVEL *fl, ID3D11DeviceContext **ctx)
{
    static const D3D_DRIVER_TYPE types[] = { D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE };
    HRESULT hr = E_FAIL;
    int i;
    for (i = 0; i < 2; i++) {
        hr = fn(NULL, types[i], NULL, flags, fls, nfl, D3D11_SDK_VERSION, sd, sc, dev, fl, ctx);
        if (SUCCEEDED(hr)) return hr;
    }
    return hr;
}

TEST(dxgap0_cdsc11_device_and_swapchain)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    HWND wnd;
    DXGI_SWAP_CHAIN_DESC sd, sd2;
    IDXGISwapChain *sc = NULL;
    ID3D11Device *dev = NULL, *dev2 = NULL;
    ID3D11DeviceContext *ctx = NULL;
    D3D_FEATURE_LEVEL fl = 0;
    HRESULT hr;
    REQUIRE(fn);
    wnd = d3dh_create_hidden_window_sized(128, 96);
    make_desc(&sd, wnd, 128, 96, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    hr = make11(fn, 0, NULL, 0, &sd, &sc, &dev, &fl, &ctx);
    if (FAILED(hr)) { t_skip("no device with swap chain: %s", d3dh_hr_name(hr)); d3dh_destroy_window(wnd); return; }
    CHECK(sc && dev && ctx);
    CHECK(fl >= D3D_FEATURE_LEVEL_9_1);
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(dev), fl);
    /* the swap chain was created on this very device */
    EXPECT_HR(IDXGISwapChain_GetDevice(sc, &IID_ID3D11Device, (void **)&dev2), S_OK);
    CHECK(dev2 == dev);
    SAFE_RELEASE(dev2);
    memset(&sd2, 0, sizeof sd2);
    EXPECT_HR(IDXGISwapChain_GetDesc(sc, &sd2), S_OK);
    EXPECT_EQ(sd2.BufferDesc.Width, 128);
    EXPECT_EQ(sd2.BufferDesc.Height, 96);
    EXPECT_EQ(sd2.BufferDesc.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(sd2.BufferCount, 1);
    EXPECT_EQ(sd2.OutputWindow, wnd);
    EXPECT_EQ(sd2.Windowed, TRUE);
    EXPECT_EQ(sd2.SampleDesc.Count, 1);
    EXPECT_EQ(sd2.SwapEffect, DXGI_SWAP_EFFECT_DISCARD);
    {
        ID3D11Texture2D *bb = NULL;
        D3D11_TEXTURE2D_DESC td;
        EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb), S_OK);
        if (bb) {
            ID3D11Texture2D_GetDesc(bb, &td);
            EXPECT_EQ(td.Width, 128); EXPECT_EQ(td.Height, 96);
            EXPECT_EQ(td.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
            EXPECT_EQ(td.ArraySize, 1); EXPECT_EQ(td.MipLevels, 1);
            EXPECT_EQ(td.SampleDesc.Count, 1);
            EXPECT_EQ(td.Usage, D3D11_USAGE_DEFAULT);
            CHECK(td.BindFlags & D3D11_BIND_RENDER_TARGET);
            SAFE_RELEASE(bb);
        }
        EXPECT_HR_FAILED(IDXGISwapChain_GetBuffer(sc, 1, &IID_ID3D11Texture2D, (void **)&bb));
    }
    {
        ID3D11DeviceContext *c2 = NULL;
        ID3D11Device_GetImmediateContext(dev, &c2);
        CHECK(c2 == ctx);                            /* returned context is the immediate context */
        SAFE_RELEASE(c2);
    }
    SAFE_RELEASE(ctx); SAFE_RELEASE(sc);
    { ULONG rc = D3DH_RELEASE_RC(dev); dev = NULL; EXPECT_EQ(rc, 0); }
    d3dh_destroy_window(wnd);
}

TEST(dxgap0_cdsc11_device_only_variants)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    HWND wnd;
    DXGI_SWAP_CHAIN_DESC sd;
    IDXGISwapChain *sc = NULL;
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    HRESULT hr;
    REQUIRE(fn);
    wnd = d3dh_create_hidden_window_sized(64, 64);
    make_desc(&sd, wnd, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2);
    /* device only: no swap chain description, no swap chain output */
    hr = make11(fn, 0, NULL, 0, NULL, NULL, &dev, NULL, &ctx);
    if (FAILED(hr)) { t_skip("no device: %s", d3dh_hr_name(hr)); d3dh_destroy_window(wnd); return; }
    CHECK(dev && ctx);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    /* a description but no place to return the swap chain: the device is still created */
    hr = make11(fn, 0, NULL, 0, &sd, NULL, &dev, NULL, NULL);
    EXPECT_HR(hr, S_OK);
    CHECK(dev != NULL);
    SAFE_RELEASE(dev);
    /* swap chain requested but no description */
    sc = (IDXGISwapChain *)(ULONG_PTR)0x10;
    hr = make11(fn, 0, NULL, 0, NULL, &sc, &dev, NULL, NULL);
    t_note("ppSwapChain without pSwapChainDesc: hr=%s sc=%p dev=%p", d3dh_hr_name(hr), (void *)sc, (void *)dev);
    if (SUCCEEDED(hr)) CHECK(sc == NULL);
    if (sc && sc != (IDXGISwapChain *)(ULONG_PTR)0x10) SAFE_RELEASE(sc);
    SAFE_RELEASE(dev);
    /* every output NULL */
    hr = fn(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL, NULL, NULL, NULL);
    t_note("all outputs NULL: hr=%s", d3dh_hr_name(hr));
    d3dh_destroy_window(wnd);
}

TEST(dxgap0_cdsc11_feature_level_requests)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    static const D3D_FEATURE_LEVEL single[] = { D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_1 };
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < 5; i++) {
        ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL;
        D3D_FEATURE_LEVEL fl = 0;
        HRESULT hr = make11(fn, 0, &single[i], 1, NULL, NULL, &dev, &fl, &ctx);
        if (FAILED(hr)) { t_note("feature level 0x%x alone: %s", single[i], d3dh_hr_name(hr)); continue; }
        EXPECT_EQ(fl, single[i]);                    /* a single-entry list yields exactly that level */
        EXPECT_EQ(ID3D11Device_GetFeatureLevel(dev), single[i]);
        SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    }
    {   /* ordered list: first supported entry wins */
        static const D3D_FEATURE_LEVEL order[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
        ID3D11Device *dev = NULL; D3D_FEATURE_LEVEL fl = 0;
        HRESULT hr = make11(fn, 0, order, 3, NULL, NULL, &dev, &fl, NULL);
        if (SUCCEEDED(hr)) { CHECK(fl == order[0] || fl == order[1] || fl == order[2]); SAFE_RELEASE(dev); }
    }
    {   /* a reversed list picks the lowest first */
        static const D3D_FEATURE_LEVEL rev[] = { D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_11_0 };
        ID3D11Device *dev = NULL; D3D_FEATURE_LEVEL fl = 0;
        HRESULT hr = make11(fn, 0, rev, 2, NULL, NULL, &dev, &fl, NULL);
        if (SUCCEEDED(hr)) { EXPECT_EQ(fl, D3D_FEATURE_LEVEL_10_0); SAFE_RELEASE(dev); }
    }
}

TEST(dxgap0_cdsc11_creation_flags)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    static const UINT flagsets[] = { D3D11_CREATE_DEVICE_SINGLETHREADED, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS };
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < 4; i++) {
        ID3D11Device *dev = NULL;
        HRESULT hr = make11(fn, flagsets[i], NULL, 0, NULL, NULL, &dev, NULL, NULL);
        if (FAILED(hr)) { t_note("flags 0x%x: %s", flagsets[i], d3dh_hr_name(hr)); continue; }
        EXPECT_EQ(ID3D11Device_GetCreationFlags(dev) & flagsets[i], flagsets[i]);
        SAFE_RELEASE(dev);
    }
}

TEST(dxgap0_cdsc11_existing_adapter)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    ID3D11Device *dev = NULL, *dev2 = NULL;
    IDXGIDevice *dxdev = NULL;
    IDXGIAdapter *ad = NULL;
    HRESULT hr;
    REQUIRE(fn);
    hr = make11(fn, 0, NULL, 0, NULL, NULL, &dev, NULL, NULL);
    if (FAILED(hr)) { t_skip("no device"); return; }
    EXPECT_HR(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dxdev), S_OK);
    if (!dxdev) { SAFE_RELEASE(dev); return; }
    EXPECT_HR(IDXGIDevice_GetAdapter(dxdev, &ad), S_OK);
    if (ad) {
        dev2 = SENT_DEV;
        /* explicit adapter requires D3D_DRIVER_TYPE_UNKNOWN */
        hr = fn(ad, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL, &dev2, NULL, NULL);
        EXPECT_HR(hr, E_INVALIDARG_HR);
        CHECK(dev2 == NULL);
        hr = fn(ad, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL, &dev2, NULL, NULL);
        EXPECT_HR(hr, E_INVALIDARG_HR);
        CHECK(dev2 == NULL);
        hr = fn(ad, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL, &dev2, NULL, NULL);
        EXPECT_HR(hr, S_OK);
        if (dev2) {
            IDXGIDevice *d2 = NULL; IDXGIAdapter *a2 = NULL;
            if (SUCCEEDED(ID3D11Device_QueryInterface(dev2, &IID_IDXGIDevice, (void **)&d2)) && SUCCEEDED(IDXGIDevice_GetAdapter(d2, &a2))) {
                DXGI_ADAPTER_DESC x, y;
                memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
                IDXGIAdapter_GetDesc(ad, &x); IDXGIAdapter_GetDesc(a2, &y);
                EXPECT_EQ(x.AdapterLuid.LowPart, y.AdapterLuid.LowPart);
                EXPECT_EQ(x.AdapterLuid.HighPart, y.AdapterLuid.HighPart);
            }
            SAFE_RELEASE(a2); SAFE_RELEASE(d2); SAFE_RELEASE(dev2);
        }
    }
    SAFE_RELEASE(ad); SAFE_RELEASE(dxdev); SAFE_RELEASE(dev);
}

TEST(dxgap0_cdsc11_swapchain_failure_cleans_outputs)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    DXGI_SWAP_CHAIN_DESC sd;
    IDXGISwapChain *sc = SENT_SC;
    ID3D11Device *dev = SENT_DEV;
    ID3D11DeviceContext *ctx = SENT_CTX;
    D3D_FEATURE_LEVEL fl = (D3D_FEATURE_LEVEL)0x1234;
    HRESULT hr;
    REQUIRE(fn);
    make_desc(&sd, NULL, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1);   /* windowed without an output window */
    hr = fn(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
    if (SUCCEEDED(hr)) { t_note("NULL OutputWindow unexpectedly accepted"); SAFE_RELEASE(sc); SAFE_RELEASE(ctx); SAFE_RELEASE(dev); return; }
    t_note("NULL OutputWindow: %s", d3dh_hr_name(hr));
    if (hr == DXGI_ERROR_UNSUPPORTED) { t_skip("no WARP device on this system"); return; }
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    CHECK(sc == NULL); CHECK(dev == NULL); CHECK(ctx == NULL);     /* a partially created device is released */
}

/* ---- D3D10.1 ---- */
static HRESULT make10(cdsc10_t fn, D3D10_FEATURE_LEVEL1 lvl, DXGI_SWAP_CHAIN_DESC *sd, IDXGISwapChain **sc, ID3D10Device1 **dev)
{
    static const D3D10_DRIVER_TYPE types[] = { D3D10_DRIVER_TYPE_WARP, D3D10_DRIVER_TYPE_REFERENCE };
    HRESULT hr = E_FAIL;
    int i;
    for (i = 0; i < 2; i++) {
        hr = fn(NULL, types[i], NULL, 0, lvl, D3D10_1_SDK_VERSION, sd, sc, dev);
        if (SUCCEEDED(hr)) return hr;
    }
    return hr;
}

TEST(dxgap0_cdsc10_1_device_and_swapchain)
{
    cdsc10_t fn = (cdsc10_t)t_proc_in("d3d10_1.dll", "D3D10CreateDeviceAndSwapChain1");
    HWND wnd;
    DXGI_SWAP_CHAIN_DESC sd, sd2;
    IDXGISwapChain *sc = NULL;
    ID3D10Device1 *dev = NULL, *dev2 = NULL;
    HRESULT hr;
    REQUIRE(fn);
    wnd = d3dh_create_hidden_window_sized(96, 64);
    make_desc(&sd, wnd, 96, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    hr = make10(fn, D3D10_FEATURE_LEVEL_10_0, &sd, &sc, &dev);
    if (FAILED(hr)) { t_skip("no D3D10.1 device: %s", d3dh_hr_name(hr)); d3dh_destroy_window(wnd); return; }
    CHECK(sc && dev);
    EXPECT_EQ(ID3D10Device1_GetFeatureLevel(dev), D3D10_FEATURE_LEVEL_10_0);
    EXPECT_HR(IDXGISwapChain_GetDevice(sc, &IID_ID3D10Device, (void **)&dev2), S_OK);
    CHECK((void *)dev2 == (void *)dev);
    SAFE_RELEASE(dev2);
    memset(&sd2, 0, sizeof sd2);
    EXPECT_HR(IDXGISwapChain_GetDesc(sc, &sd2), S_OK);
    EXPECT_EQ(sd2.BufferDesc.Width, 96); EXPECT_EQ(sd2.BufferDesc.Height, 64);
    EXPECT_EQ(sd2.OutputWindow, wnd);
    {
        ID3D10Texture2D *bb = NULL;
        EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D10Texture2D, (void **)&bb), S_OK);
        SAFE_RELEASE(bb);
    }
    SAFE_RELEASE(sc);
    { ULONG rc = D3DH_RELEASE_RC(dev); dev = NULL; EXPECT_EQ(rc, 0); }
    d3dh_destroy_window(wnd);
}

TEST(dxgap0_cdsc10_1_feature_levels)
{
    cdsc10_t fn = (cdsc10_t)t_proc_in("d3d10_1.dll", "D3D10CreateDeviceAndSwapChain1");
    static const D3D10_FEATURE_LEVEL1 lv[] = { D3D10_FEATURE_LEVEL_10_1, D3D10_FEATURE_LEVEL_10_0, D3D10_FEATURE_LEVEL_9_3, D3D10_FEATURE_LEVEL_9_2, D3D10_FEATURE_LEVEL_9_1 };
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < 5; i++) {
        ID3D10Device1 *dev = NULL;
        HRESULT hr = make10(fn, lv[i], NULL, NULL, &dev);
        if (FAILED(hr)) { t_note("D3D10.1 feature level 0x%x: %s", lv[i], d3dh_hr_name(hr)); continue; }
        EXPECT_EQ(ID3D10Device1_GetFeatureLevel(dev), lv[i]);
        SAFE_RELEASE(dev);
    }
}

TEST(dxgap0_cdsc10_1_no_swapchain)
{
    cdsc10_t fn = (cdsc10_t)t_proc_in("d3d10_1.dll", "D3D10CreateDeviceAndSwapChain1");
    ID3D10Device1 *dev = NULL;
    IDXGISwapChain *sc = SENT_SC;
    HRESULT hr;
    REQUIRE(fn);
    hr = make10(fn, D3D10_FEATURE_LEVEL_10_0, NULL, NULL, &dev);
    if (FAILED(hr)) { t_skip("no device"); return; }
    CHECK(dev != NULL);
    SAFE_RELEASE(dev);
    hr = make10(fn, D3D10_FEATURE_LEVEL_10_0, NULL, &sc, &dev);
    t_note("D3D10.1 ppSwapChain without desc: %s (sc=%p)", d3dh_hr_name(hr), (void *)sc);
    if (SUCCEEDED(hr) && sc != SENT_SC) SAFE_RELEASE(sc);
    SAFE_RELEASE(dev);
}
