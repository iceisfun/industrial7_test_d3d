/* Driver types, D3D11_CREATE_DEVICE_* flags, GetCreationFlags, swap chain creation. */
#include "dc.h"

#define T(n) TEST(d3d11_device_create_##n)

/* ---- driver types ---- */
static void drv_probe(D3D_DRIVER_TYPE dt, const char *name)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    HRESULT hr = DC_FN()(NULL, dt, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, &c);
    if (FAILED(hr)) {
        t_note("%s not available: %s", name, d3dh_hr_name(hr));
        CHECK(d == NULL); CHECK(c == NULL); EXPECT_EQ(fl, 0);
        t_skip("%s device cannot be created here", name);
        return;
    }
    CHECK(d != NULL); CHECK(c != NULL); CHECK(dc_fl_known(fl)); CHECK(fl <= D3D_FEATURE_LEVEL_11_0);
    if (d) {
        EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), fl);
        EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    }
    DC_REL(d, c);
}
T(driver_warp) { REQUIRE(DC_FN()); drv_probe(D3D_DRIVER_TYPE_WARP, "WARP"); }
T(driver_hardware) { REQUIRE(DC_FN()); drv_probe(D3D_DRIVER_TYPE_HARDWARE, "HARDWARE"); }
T(driver_reference) { REQUIRE(DC_FN()); drv_probe(D3D_DRIVER_TYPE_REFERENCE, "REFERENCE"); }
T(driver_null)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0;
    REQUIRE(DC_FN());
    HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_NULL, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, &c);
    if (FAILED(hr)) { t_skip("NULL device unavailable: %s", d3dh_hr_name(hr)); return; }
    CHECK(d && c); CHECK(dc_fl_known(fl));
    DC_REL(d, c);
}
T(driver_unknown_with_warp_adapter)
{
    IDXGIAdapter *a; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0;
    REQUIRE(DC_FN());
    if (!dc_get_warp_adapter(&a)) { t_skip("no WARP adapter"); return; }
    HRESULT hr = DC_FN()(a, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, &c);
    if (FAILED(hr)) { IDXGIAdapter_Release(a); t_skip("device on WARP adapter failed: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR_OK(hr);
    CHECK(d && c); CHECK(dc_fl_known(fl));
    IDXGIDevice *dd = NULL;
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd))) {
        IDXGIAdapter *got = NULL;
        EXPECT_HR_OK(IDXGIDevice_GetAdapter(dd, &got));
        if (got) {
            DXGI_ADAPTER_DESC d1, d2;
            EXPECT_HR_OK(IDXGIAdapter_GetDesc(a, &d1));
            EXPECT_HR_OK(IDXGIAdapter_GetDesc(got, &d2));
            EXPECT_EQ(d1.AdapterLuid.LowPart, d2.AdapterLuid.LowPart);
            EXPECT_EQ(d1.AdapterLuid.HighPart, d2.AdapterLuid.HighPart);
            IDXGIAdapter_Release(got);
        }
        IDXGIDevice_Release(dd);
    }
    DC_REL(d, c);
    IDXGIAdapter_Release(a);
}
T(driver_unknown_with_adapter0)
{
    IDXGIAdapter *a; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0;
    REQUIRE(DC_FN());
    if (!dc_get_adapter0(&a)) { t_skip("no DXGI adapter"); return; }
    HRESULT hr = DC_FN()(a, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, NULL);
    if (FAILED(hr)) { t_note("device on adapter 0 failed: %s", d3dh_hr_name(hr)); CHECK(d == NULL); EXPECT_EQ(fl, 0); }
    else { CHECK(d != NULL); CHECK(dc_fl_known(fl)); }
    SAFE_RELEASE(d);
    IDXGIAdapter_Release(a);
}
T(driver_unknown_adapter_not_released_by_call)
{
    IDXGIAdapter *a; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    if (!dc_get_warp_adapter(&a)) { t_skip("no WARP adapter"); return; }
    ULONG before = d3dh_refcount(a);
    HRESULT hr = DC_FN()(a, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, NULL, NULL);
    SAFE_RELEASE(d);
    if (SUCCEEDED(hr)) {
        ULONG after = d3dh_refcount(a);
        CHECK(after >= 1);
        t_note("adapter refcount before=%lu after=%lu", before, after);
    }
    IDXGIAdapter_Release(a);
}
T(driver_unknown_levels_restricted_by_list)
{
    IDXGIAdapter *a; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0; D3D_FEATURE_LEVEL l = D3D_FEATURE_LEVEL_10_0;
    REQUIRE(DC_FN());
    if (!dc_get_warp_adapter(&a)) { t_skip("no WARP adapter"); return; }
    HRESULT hr = DC_FN()(a, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, &l, 1, D3D11_SDK_VERSION, &d, &fl, NULL);
    if (FAILED(hr)) { IDXGIAdapter_Release(a); t_skip("failed: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_10_0);
    SAFE_RELEASE(d);
    IDXGIAdapter_Release(a);
}
T(driver_warp_twice_independent_devices)
{
    ID3D11Device *a = NULL, *b = NULL;
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(0, NULL, 0, &a, NULL, NULL))) { t_skip("no WARP"); return; }
    EXPECT_HR_OK(dc_warp(0, NULL, 0, &b, NULL, NULL));
    CHECK(a != b); CHECK(a && b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

/* ---- flags ---- */
#define FLAG_OK(nm, f) T(flag_##nm) { \
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0; \
    REQUIRE(DC_FN()); \
    HRESULT hr = dc_warp(f, NULL, 0, &d, &c, &fl); \
    if (FAILED(hr)) { t_skip("WARP with " #f ": %s", d3dh_hr_name(hr)); return; } \
    CHECK(d && c); CHECK(dc_fl_known(fl)); \
    UINT cf = ID3D11Device_GetCreationFlags(d); \
    t_note(#nm ": GetCreationFlags=0x%x", cf); \
    DC_REL(d, c); }
FLAG_OK(singlethreaded, D3D11_CREATE_DEVICE_SINGLETHREADED)
FLAG_OK(prevent_internal_threading, D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS)
FLAG_OK(bgra_support, D3D11_CREATE_DEVICE_BGRA_SUPPORT)
FLAG_OK(prevent_altering_layer_settings, D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY)
FLAG_OK(disable_gpu_timeout, D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT)
FLAG_OK(video_support, D3D11_CREATE_DEVICE_VIDEO_SUPPORT)
FLAG_OK(debuggable, D3D11_CREATE_DEVICE_DEBUGGABLE)
FLAG_OK(combined_common, D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY)

#define FLAG_STICKS(nm, f) T(creation_flags_##nm) { \
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0; \
    REQUIRE(DC_FN()); \
    HRESULT hr = dc_warp(f, NULL, 0, &d, NULL, &fl); \
    if (FAILED(hr)) { t_skip("WARP with " #f ": %s", d3dh_hr_name(hr)); return; } \
    UINT cf = ID3D11Device_GetCreationFlags(d); \
    EXPECT_EQ(cf & (f), (f)); \
    SAFE_RELEASE(d); }
FLAG_STICKS(singlethreaded, D3D11_CREATE_DEVICE_SINGLETHREADED)
FLAG_STICKS(bgra, D3D11_CREATE_DEVICE_BGRA_SUPPORT)
FLAG_STICKS(single_bgra, D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT)

T(creation_flags_zero_has_no_requested_bits)
{
    ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(0, NULL, 0, &d, NULL, NULL))) { t_skip("no WARP"); return; }
    UINT cf = ID3D11Device_GetCreationFlags(d);
    EXPECT_EQ(cf & (D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG), 0);
    SAFE_RELEASE(d);
}
T(creation_flags_same_through_context_device)
{
    ID3D11Device *d = NULL, *d2 = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, &d, &c, NULL))) { t_skip("no WARP"); return; }
    ID3D11DeviceContext_GetDevice(c, &d2);
    CHECK(d2 == d);
    if (d2) EXPECT_EQ(ID3D11Device_GetCreationFlags(d2), ID3D11Device_GetCreationFlags(d));
    SAFE_RELEASE(d2);
    DC_REL(d, c);
}

T(flag_debug_layer_present_or_missing)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    REQUIRE(DC_FN());
    HRESULT hr = dc_warp(D3D11_CREATE_DEVICE_DEBUG, NULL, 0, &d, &c, &fl);
    if (SUCCEEDED(hr)) {
        t_note("debug layer installed");
        CHECK(d && c); CHECK(dc_fl_known(fl));
        CHECK(ID3D11Device_GetCreationFlags(d) & D3D11_CREATE_DEVICE_DEBUG);
        ID3D11Debug *dbg = NULL; ID3D11InfoQueue *iq = NULL;
        EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_ID3D11Debug, (void **)&dbg));
        EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_ID3D11InfoQueue, (void **)&iq));
        SAFE_RELEASE(dbg); SAFE_RELEASE(iq);
    } else {
        t_note("debug device creation: %s", d3dh_hr_name(hr));
        EXPECT_HR(hr, DXGI_ERROR_SDK_COMPONENT_MISSING);
        CHECK(d == NULL); CHECK(c == NULL); EXPECT_EQ(fl, 0);
    }
    DC_REL(d, c);
}
T(flag_debug_missing_layer_outputs_cleared)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    REQUIRE(DC_FN());
    HRESULT hr = dc_warp(D3D11_CREATE_DEVICE_DEBUG, NULL, 0, &d, &c, &fl);
    if (SUCCEEDED(hr)) { DC_REL(d, c); t_skip("debug layer installed"); return; }
    EXPECT_HR(hr, DXGI_ERROR_SDK_COMPONENT_MISSING);
    CHECK(d == NULL); CHECK(c == NULL); EXPECT_EQ(fl, 0);
}
T(flag_debug_not_requested_no_debug_interfaces)
{
    ID3D11Device *d = NULL; ID3D11Debug *dbg = (ID3D11Debug *)(ULONG_PTR)0; ID3D11InfoQueue *iq = NULL;
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(0, NULL, 0, &d, NULL, NULL))) { t_skip("no WARP"); return; }
    HRESULT h1 = ID3D11Device_QueryInterface(d, &IID_ID3D11Debug, (void **)&dbg);
    HRESULT h2 = ID3D11Device_QueryInterface(d, &IID_ID3D11InfoQueue, (void **)&iq);
    EXPECT_HR(h1, E_NOINTERFACE); EXPECT_HR(h2, E_NOINTERFACE);
    CHECK(dbg == NULL); CHECK(iq == NULL);
    SAFE_RELEASE(d);
}
T(flag_debug_with_levels)
{
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_10_0 };
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0;
    REQUIRE(DC_FN());
    HRESULT hr = dc_warp(D3D11_CREATE_DEVICE_DEBUG, l, 1, &d, NULL, &fl);
    if (FAILED(hr)) { EXPECT_HR(hr, DXGI_ERROR_SDK_COMPONENT_MISSING); CHECK(d == NULL); return; }
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_10_0);
    SAFE_RELEASE(d);
}
T(flag_debug_with_invalid_level_is_invalidarg_first)
{
    D3D_FEATURE_LEVEL l = D3D_FEATURE_LEVEL_1_0_CORE;
    REQUIRE(DC_FN());
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_DEBUG, &l, 1, D3D11_SDK_VERSION, "debug flag + invalid level");
}
T(flag_increased_priority)
{
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    REQUIRE(DC_FN());
    HRESULT hr = dc_warp(0x1000, NULL, 0, &d, NULL, &fl);
    if (FAILED(hr)) {
        /* without SeIncBasePriority the call is rejected as an invalid argument */
        t_note("INCREASED_PRIORITY: %s", d3dh_hr_name(hr));
        EXPECT_HR(hr, E_INVALIDARG);   /* decomp: missing SeIncBasePriority -> 0x80070057 */
        CHECK(d == NULL); EXPECT_EQ(fl, 0);
    } else {
        t_note("INCREASED_PRIORITY accepted (privilege held)");
    }
    SAFE_RELEASE(d);
}
T(flag_switch_to_ref_reserved)
{
    ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = dc_warp(D3D11_CREATE_DEVICE_SWITCH_TO_REF, NULL, 0, &d, NULL, NULL);
    t_note("SWITCH_TO_REF (reserved) -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(d);
}
T(flag_unknown_high_bits)
{
    ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = dc_warp(0x00400000, NULL, 0, &d, NULL, NULL);
    t_note("undefined flag bit 0x400000 -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(d);
}

T(flag_bgra_format_support)
{
    ID3D11Device *d = NULL; UINT s = 0;
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, &d, NULL, NULL))) { t_skip("no WARP"); return; }
    EXPECT_HR_OK(ID3D11Device_CheckFormatSupport(d, DXGI_FORMAT_B8G8R8A8_UNORM, &s));
    CHECK(s & D3D11_FORMAT_SUPPORT_TEXTURE2D);
    CHECK(s & D3D11_FORMAT_SUPPORT_RENDER_TARGET);
    SAFE_RELEASE(d);
}
T(flag_bgra_texture_creates)
{
    ID3D11Device *d = NULL; ID3D11Texture2D *t = NULL;
    D3D11_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, &d, NULL, NULL))) { t_skip("no WARP"); return; }
    EXPECT_HR_OK(ID3D11Device_CreateTexture2D(d, &td, NULL, &t));
    SAFE_RELEASE(t); SAFE_RELEASE(d);
}
T(flag_single_threaded_device_usable)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Buffer *b = NULL;
    D3D11_BUFFER_DESC bd = { 64, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(D3D11_CREATE_DEVICE_SINGLETHREADED, NULL, 0, &d, &c, NULL))) { t_skip("no WARP"); return; }
    EXPECT_HR_OK(ID3D11Device_CreateBuffer(d, &bd, NULL, &b));
    CHECK(b != NULL);
    SAFE_RELEASE(b);
    DC_REL(d, c);
}
T(flag_video_support_video_device_interface)
{
    ID3D11Device *d = NULL; ID3D11VideoDevice *v = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = dc_warp(D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0, &d, NULL, NULL);
    if (FAILED(hr)) { t_skip("video support: %s", d3dh_hr_name(hr)); return; }
    hr = ID3D11Device_QueryInterface(d, &IID_ID3D11VideoDevice, (void **)&v);
    t_note("QI ID3D11VideoDevice with VIDEO_SUPPORT -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(v); SAFE_RELEASE(d);
}

/* ---- swap chain creation ---- */
static DXGI_SWAP_CHAIN_DESC sc_desc(HWND h, UINT w, UINT ht)
{
    DXGI_SWAP_CHAIN_DESC sd;
    memset(&sd, 0, sizeof sd);
    sd.BufferDesc.Width = w; sd.BufferDesc.Height = ht; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 1;
    sd.OutputWindow = h; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    return sd;
}
static HRESULT sc_create(const DXGI_SWAP_CHAIN_DESC *sd, IDXGISwapChain **sc, ID3D11Device **d, ID3D11DeviceContext **c,
                         D3D_FEATURE_LEVEL *fl, UINT flags)
{
    return DC_SC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, NULL, 0, D3D11_SDK_VERSION, sd, sc, d, fl, c);
}

T(swapchain_basic)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64), got;
    HRESULT hr = sc_create(&sd, &sc, &d, &c, &fl, 0);
    if (FAILED(hr)) { d3dh_destroy_window(h); t_skip("swap chain device: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR_OK(hr);
    CHECK(sc && d && c); CHECK(dc_fl_known(fl));
    if (sc) {
        memset(&got, 0, sizeof got);
        EXPECT_HR_OK(IDXGISwapChain_GetDesc(sc, &got));
        EXPECT_EQ(got.BufferDesc.Width, 64); EXPECT_EQ(got.BufferDesc.Height, 64);
        EXPECT_EQ(got.BufferDesc.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
        EXPECT_EQ(got.BufferCount, 1); EXPECT_EQ(got.Windowed, TRUE);
        EXPECT_EQ(got.OutputWindow, h); EXPECT_EQ(got.SwapEffect, DXGI_SWAP_EFFECT_DISCARD);
        EXPECT_EQ(got.SampleDesc.Count, 1); EXPECT_EQ(got.SampleDesc.Quality, 0);
    }
    SAFE_RELEASE(sc); DC_REL(d, c);
    d3dh_destroy_window(h);
}
T(swapchain_get_device_returns_created_device)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL, *g = NULL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 32, 32);
    if (FAILED(sc_create(&sd, &sc, &d, NULL, NULL, 0))) { d3dh_destroy_window(h); t_skip("no swap chain device"); return; }
    EXPECT_HR_OK(IDXGISwapChain_GetDevice(sc, &IID_ID3D11Device, (void **)&g));
    CHECK(g == d);
    SAFE_RELEASE(g); SAFE_RELEASE(sc); SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_get_buffer)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL; ID3D11Texture2D *t = NULL, *t2 = NULL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 48, 40);
    if (FAILED(sc_create(&sd, &sc, &d, NULL, NULL, 0))) { d3dh_destroy_window(h); t_skip("no swap chain device"); return; }
    EXPECT_HR_OK(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t));
    if (t) {
        D3D11_TEXTURE2D_DESC td;
        ID3D11Texture2D_GetDesc(t, &td);
        EXPECT_EQ(td.Width, 48); EXPECT_EQ(td.Height, 40);
        EXPECT_EQ(td.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
        CHECK(td.BindFlags & D3D11_BIND_RENDER_TARGET);
        EXPECT_EQ(td.ArraySize, 1); EXPECT_EQ(td.MipLevels, 1);
    }
    EXPECT_HR_OK(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t2));
    CHECK(t == t2);
    void *bad = NULL;
    HRESULT hr = IDXGISwapChain_GetBuffer(sc, 1, &IID_ID3D11Texture2D, &bad);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    CHECK(bad == NULL);
    SAFE_RELEASE(t2); SAFE_RELEASE(t); SAFE_RELEASE(sc); SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_zero_size_uses_window_client_area)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL; RECT rc;
    REQUIRE(DC_SC_FN());
    if (t_is_wine()) { t_skip("hidden window geometry under emulation"); return; }
    h = d3dh_create_hidden_window_sized(96, 80);
    GetClientRect(h, &rc);
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 0, 0), got;
    if (FAILED(sc_create(&sd, &sc, &d, NULL, NULL, 0))) { d3dh_destroy_window(h); t_skip("no swap chain device"); return; }
    memset(&got, 0, sizeof got);
    EXPECT_HR_OK(IDXGISwapChain_GetDesc(sc, &got));
    EXPECT_EQ(got.BufferDesc.Width, rc.right - rc.left);
    EXPECT_EQ(got.BufferDesc.Height, rc.bottom - rc.top);
    SAFE_RELEASE(sc); SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_two_buffers_flip_discard)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64);
    sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    HRESULT hr = sc_create(&sd, &sc, &d, NULL, NULL, 0);
    if (FAILED(hr)) { t_note("flip-discard swap chain: %s", d3dh_hr_name(hr)); CHECK(sc == NULL); CHECK(d == NULL); }
    else { DXGI_SWAP_CHAIN_DESC got; IDXGISwapChain_GetDesc(sc, &got); EXPECT_EQ(got.BufferCount, 2); EXPECT_EQ(got.SwapEffect, DXGI_SWAP_EFFECT_FLIP_DISCARD); }
    SAFE_RELEASE(sc); SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_without_output_pointer_creates_device_only)
{
    HWND h; ID3D11Device *d = NULL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64);
    HRESULT hr = sc_create(&sd, NULL, &d, NULL, NULL, 0);
    if (FAILED(hr)) { d3dh_destroy_window(h); t_skip("WARP: %s", d3dh_hr_name(hr)); return; }
    CHECK(d != NULL);
    SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_without_desc_creates_device_only)
{
    ID3D11Device *d = NULL; IDXGISwapChain *sc = NULL;
    REQUIRE(DC_SC_FN());
    HRESULT hr = sc_create(NULL, NULL, &d, NULL, NULL, 0);
    if (FAILED(hr)) { t_skip("WARP: %s", d3dh_hr_name(hr)); return; }
    CHECK(d != NULL);
    SAFE_RELEASE(d);
    hr = sc_create(NULL, &sc, &d, NULL, NULL, 0);
    t_note("NULL desc with non-NULL ppSwapChain -> %s (sc=%p)", d3dh_hr_name(hr), (void *)sc);
    SAFE_RELEASE(sc); SAFE_RELEASE(d);
}
T(swapchain_device_pointer_null_swapchain_keeps_device_alive)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *g = NULL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64);
    HRESULT hr = sc_create(&sd, &sc, NULL, NULL, NULL, 0);
    if (FAILED(hr)) { d3dh_destroy_window(h); t_skip("swap chain without device out: %s", d3dh_hr_name(hr)); return; }
    CHECK(sc != NULL);
    if (sc) {
        EXPECT_HR_OK(IDXGISwapChain_GetDevice(sc, &IID_ID3D11Device, (void **)&g));
        CHECK(g != NULL);
        if (g) EXPECT_HR_OK(ID3D11Device_GetDeviceRemovedReason(g));
    }
    SAFE_RELEASE(g); SAFE_RELEASE(sc);
    d3dh_destroy_window(h);
}
T(swapchain_null_window_fails_and_clears)
{
    IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    REQUIRE(DC_SC_FN());
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(NULL, 64, 64);
    HRESULT hr = sc_create(&sd, &sc, &d, &c, &fl, 0);
    EXPECT_HR_FAILED(hr);
    t_note("windowed swap chain with NULL OutputWindow -> %s", d3dh_hr_name(hr));
    CHECK(sc == NULL); CHECK(d == NULL); CHECK(c == NULL); EXPECT_EQ(fl, 0);
    SAFE_RELEASE(sc); DC_REL(d, c);
}
T(swapchain_invalid_buffer_count_fails_and_clears)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64);
    sd.BufferCount = 1000;
    HRESULT hr = sc_create(&sd, &sc, &d, NULL, &fl, 0);
    EXPECT_HR_FAILED(hr);
    t_note("BufferCount 1000 -> %s", d3dh_hr_name(hr));
    CHECK(sc == NULL); CHECK(d == NULL); EXPECT_EQ(fl, 0);
    SAFE_RELEASE(sc); SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_invalid_format_fails_and_clears)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64);
    sd.BufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
    HRESULT hr = sc_create(&sd, &sc, &d, NULL, NULL, 0);
    EXPECT_HR_FAILED(hr);
    CHECK(sc == NULL); CHECK(d == NULL);
    SAFE_RELEASE(sc); SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_with_flags_and_levels)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0;
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_10_0 };
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64);
    HRESULT hr = DC_SC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, l, 1, D3D11_SDK_VERSION, &sd, &sc, &d, &fl, NULL);
    if (FAILED(hr)) { d3dh_destroy_window(h); t_skip("failed: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_10_0);
    CHECK(ID3D11Device_GetCreationFlags(d) & D3D11_CREATE_DEVICE_BGRA_SUPPORT);
    SAFE_RELEASE(sc); SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_interfaces)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL; IDXGISwapChain1 *s1 = NULL; IDXGIDevice *dd = NULL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64);
    if (FAILED(sc_create(&sd, &sc, &d, NULL, NULL, 0))) { d3dh_destroy_window(h); t_skip("no swap chain device"); return; }
    EXPECT_HR_OK(IDXGISwapChain_QueryInterface(sc, &IID_IDXGISwapChain1, (void **)&s1));
    EXPECT_HR_OK(IDXGISwapChain_GetDevice(sc, &IID_IDXGIDevice, (void **)&dd));
    CHECK(dd != NULL);
    if (dd) {
        ID3D11Device *back = NULL;
        EXPECT_HR_OK(IDXGIDevice_QueryInterface(dd, &IID_ID3D11Device, (void **)&back));
        CHECK(back == d);
        SAFE_RELEASE(back);
    }
    SAFE_RELEASE(dd); SAFE_RELEASE(s1); SAFE_RELEASE(sc); SAFE_RELEASE(d);
    d3dh_destroy_window(h);
}
T(swapchain_release_order_swapchain_last)
{
    HWND h; IDXGISwapChain *sc = NULL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(DC_SC_FN());
    h = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd = sc_desc(h, 64, 64);
    if (FAILED(sc_create(&sd, &sc, &d, &c, NULL, 0))) { d3dh_destroy_window(h); t_skip("no swap chain device"); return; }
    DC_REL(d, c);
    CHECK(sc != NULL);
    if (sc) {
        ID3D11Device *g = NULL;
        EXPECT_HR_OK(IDXGISwapChain_GetDevice(sc, &IID_ID3D11Device, (void **)&g));   /* device kept alive by the swap chain */
        CHECK(g != NULL);
        SAFE_RELEASE(g);
    }
    SAFE_RELEASE(sc);
    d3dh_destroy_window(h);
}
