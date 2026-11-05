/* IDXGIFactory2::CreateSwapChainForHwnd / ForCoreWindow / ForComposition and the IDXGISwapChain1 getters. */
#include "sc.h"

#define OPEN2(s) SC s; if (!sc_open2(&s, 0)) return

TEST(dxgi_swapchain_hwnd_null_args)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc = (IDXGISwapChain1 *)(ULONG_PTR)1;
    EXPECT_HR(IDXGIFactory2_CreateSwapChainForHwnd(s.fac2, NULL, s.hwnd, &d, NULL, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIFactory2_CreateSwapChainForHwnd(s.fac2, s.unk, NULL, &d, NULL, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIFactory2_CreateSwapChainForHwnd(s.fac2, s.unk, s.hwnd, NULL, NULL, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    HRESULT hr = IDXGIFactory2_CreateSwapChainForHwnd(s.fac2, s.unk, s.hwnd, &d, NULL, NULL, NULL);
    CHECK(FAILED(hr));
    t_note("CreateSwapChainForHwnd(ppSwapChain=NULL) -> %s", d3dh_hr_name(hr));
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_bad_device_object)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = IDXGIFactory2_CreateSwapChainForHwnd(s.fac2, (IUnknown *)s.fac2, s.hwnd, &d, NULL, NULL, &sc);
    CHECK(FAILED(hr));
    t_note("ForHwnd(factory as device) -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_restrict_to_output_wrong_factory)
{
    OPEN2(s);
    /* An output that does not belong to this system: NULL is the only well-defined value here. */
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = IDXGIFactory2_CreateSwapChainForHwnd(s.fac2, s.unk, s.hwnd, &d, NULL, NULL, &sc);
    CHECK(sc_okish(hr));
    if (sc) {
        IDXGIOutput *o = (IDXGIOutput *)(ULONG_PTR)1;
        hr = IDXGISwapChain1_GetRestrictToOutput(sc, &o);
        EXPECT_HR(hr, S_OK);
        t_note("GetRestrictToOutput on an unrestricted chain -> output %p", (void *)o);
        SAFE_RELEASE(o);
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_desc1_roundtrip)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(100, 50, DXGI_FORMAT_B8G8R8A8_UNORM, 3, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL), g;
    IDXGISwapChain1 *sc;
    HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
    if (!sc_okish(hr)) { CHECKF(0, "create: %s", d3dh_hr_name(hr)); sc_close(&s); return; }
    memset(&g, 0xCC, sizeof g);
    EXPECT_HR(IDXGISwapChain1_GetDesc1(sc, &g), S_OK);
    EXPECT_EQ(g.Width, 100); EXPECT_EQ(g.Height, 50);
    EXPECT_EQ(g.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(g.Stereo, FALSE);
    EXPECT_EQ(g.SampleDesc.Count, 1); EXPECT_EQ(g.SampleDesc.Quality, 0);
    EXPECT_EQ(g.BufferUsage, DXGI_USAGE_RENDER_TARGET_OUTPUT);
    EXPECT_EQ(g.BufferCount, 3);
    EXPECT_EQ(g.Scaling, DXGI_SCALING_STRETCH);
    EXPECT_EQ(g.SwapEffect, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
    EXPECT_EQ(g.Flags, 0);
    t_note("GetDesc1.AlphaMode for AlphaMode=UNSPECIFIED input -> %d", g.AlphaMode);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_desc1_matches_legacy_desc)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(70, 40, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc_create1(&s, s.hwnd, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    DXGI_SWAP_CHAIN_DESC g0; memset(&g0, 0, sizeof g0);
    EXPECT_HR(IDXGISwapChain1_GetDesc(sc, &g0), S_OK);
    EXPECT_EQ(g0.BufferDesc.Width, 70); EXPECT_EQ(g0.BufferDesc.Height, 40);
    EXPECT_EQ(g0.BufferDesc.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(g0.BufferCount, 2);
    EXPECT_EQ(g0.OutputWindow, s.hwnd);
    EXPECT_EQ(g0.Windowed, TRUE);
    EXPECT_EQ(g0.SwapEffect, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    EXPECT_EQ(g0.SampleDesc.Count, 1);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_zero_size_uses_client)
{
    OPEN2(s);
    static const struct { int w, h; } t[] = { { 0, 0 }, { 200, 0 }, { 0, 200 }, { 150, 90 } };
    for (unsigned i = 0; i < ARRAY_SIZE_(t); i++) {
        HWND w = sc_window(t[i].w, t[i].h);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD), g;
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, w, &d, &sc);
        CHECKF(sc_okish(hr), "window %dx%d: %s", t[i].w, t[i].h, d3dh_hr_name(hr));
        if (sc) {
            RECT r = { 0 }; GetClientRect(w, &r);
            memset(&g, 0, sizeof g);
            EXPECT_HR(IDXGISwapChain1_GetDesc1(sc, &g), S_OK);
            UINT ew = r.right > 0 ? r.right : 8, eh = r.bottom > 0 ? r.bottom : 8;
            CHECKF(g.Width == ew && g.Height == eh, "client %ldx%ld -> %ux%u want %ux%u", r.right, r.bottom, g.Width, g.Height, ew, eh);
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_getters_null)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc_create1(&s, s.hwnd, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    EXPECT_HR(IDXGISwapChain1_GetDesc1(sc, NULL), E_INVALIDARG);
    EXPECT_HR(IDXGISwapChain1_GetFullscreenDesc(sc, NULL), E_INVALIDARG);
    HRESULT hr = IDXGISwapChain1_GetHwnd(sc, NULL);
    CHECK(FAILED(hr));
    t_note("GetHwnd(NULL) -> %s", d3dh_hr_name(hr));
    hr = IDXGISwapChain1_GetLastPresentCount(sc, NULL);
    CHECK(FAILED(hr));
    t_note("GetLastPresentCount(NULL) -> %s", d3dh_hr_name(hr));
    hr = IDXGISwapChain1_GetFrameStatistics(sc, NULL);
    CHECK(FAILED(hr));
    t_note("GetFrameStatistics(NULL) -> %s", d3dh_hr_name(hr));
    hr = IDXGISwapChain1_GetRestrictToOutput(sc, NULL);
    CHECK(FAILED(hr));
    t_note("GetRestrictToOutput(NULL) -> %s", d3dh_hr_name(hr));
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_gethwnd_value)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc_create1(&s, s.hwnd, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    HWND h = NULL;
    EXPECT_HR(IDXGISwapChain1_GetHwnd(sc, &h), S_OK);
    EXPECT_EQ(h, s.hwnd);
    /* legacy chain, queried through SwapChain1 */
    IDXGISwapChain *leg; HWND w2 = sc_window(40, 40);
    DXGI_SWAP_CHAIN_DESC d0 = sc_desc0(w2, 40, 40, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    if (sc_okish(sc_create0(&s, &d0, &leg))) {
        IDXGISwapChain1 *l1 = NULL;
        if (SUCCEEDED(IDXGISwapChain_QueryInterface(leg, &IID_IDXGISwapChain1, (void **)&l1))) {
            h = NULL;
            EXPECT_HR(IDXGISwapChain1_GetHwnd(l1, &h), S_OK);
            EXPECT_EQ(h, w2);
            SAFE_RELEASE(l1);
        }
        SC_REL(leg);
    }
    DestroyWindow(w2);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_getcorewindow_on_hwnd_chain)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc_create1(&s, s.hwnd, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    void *cw = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDXGISwapChain1_GetCoreWindow(sc, &IID_IUnknown, &cw), DXGI_ERROR_INVALID_CALL);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_fullscreen_desc_default)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc_create1(&s, s.hwnd, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fd; memset(&fd, 0xCC, sizeof fd);
    EXPECT_HR(IDXGISwapChain1_GetFullscreenDesc(sc, &fd), S_OK);
    EXPECT_EQ(fd.Windowed, TRUE);
    t_note("default fullscreen desc: rate %u/%u scanline %d scaling %d", fd.RefreshRate.Numerator, fd.RefreshRate.Denominator, fd.ScanlineOrdering, fd.Scaling);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_fullscreen_desc_roundtrip_windowed)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fd; memset(&fd, 0, sizeof fd);
    fd.RefreshRate.Numerator = 60; fd.RefreshRate.Denominator = 1; fd.Windowed = TRUE;
    fd.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE; fd.Scaling = DXGI_MODE_SCALING_CENTERED;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = IDXGIFactory2_CreateSwapChainForHwnd(s.fac2, s.unk, s.hwnd, &d, &fd, NULL, &sc);
    CHECK(sc_okish(hr));
    if (sc) {
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC g; memset(&g, 0, sizeof g);
        EXPECT_HR(IDXGISwapChain1_GetFullscreenDesc(sc, &g), S_OK);
        EXPECT_EQ(g.Windowed, TRUE);
        EXPECT_EQ(g.RefreshRate.Numerator, 60); EXPECT_EQ(g.RefreshRate.Denominator, 1);
        EXPECT_EQ(g.ScanlineOrdering, DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE);
        EXPECT_EQ(g.Scaling, DXGI_MODE_SCALING_CENTERED);
        BOOL fs = 7; IDXGIOutput *o = NULL;
        EXPECT_HR(IDXGISwapChain1_GetFullscreenState(sc, &fs, &o), S_OK);
        EXPECT_EQ(fs, FALSE); CHECK(o == NULL);
        SC_REL(sc);
    }
    sc_close(&s);
}

static const struct { UINT count; int effect; int ok; } k_h_matrix[] = {
    { 1, DXGI_SWAP_EFFECT_DISCARD, 1 }, { 2, DXGI_SWAP_EFFECT_DISCARD, 1 }, { 16, DXGI_SWAP_EFFECT_DISCARD, 1 },
    { 1, DXGI_SWAP_EFFECT_SEQUENTIAL, 1 }, { 2, DXGI_SWAP_EFFECT_SEQUENTIAL, 1 }, { 16, DXGI_SWAP_EFFECT_SEQUENTIAL, 1 },
    { 0, 2, 0 }, { 2, 2, 0 }, { 2, 5, 0 },
    { 0, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0 }, { 1, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0 },
    { 2, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 1 }, { 3, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 1 }, { 16, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 1 },
    { 17, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0 },
    { 0, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 }, { 1, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 },
    { 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 }, { 3, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 }, { 16, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 },
    { 17, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 },
};

TEST(dxgi_swapchain_hwnd_matrix_count_effect)
{
    OPEN2(s);
    for (unsigned i = 0; i < ARRAY_SIZE_(k_h_matrix); i++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, k_h_matrix[i].count, k_h_matrix[i].effect);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, w, &d, &sc);
        if (k_h_matrix[i].ok) CHECKF(sc_okish(hr), "row %u (count %u effect %d) -> %s", i, k_h_matrix[i].count, k_h_matrix[i].effect, d3dh_hr_name(hr));
        else CHECKF(hr == DXGI_ERROR_INVALID_CALL, "row %u (count %u effect %d) -> %s", i, k_h_matrix[i].count, k_h_matrix[i].effect, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
            IDXGISwapChain1_GetDesc1(sc, &g);
            CHECKF(g.BufferCount == k_h_matrix[i].count, "row %u BufferCount %u", i, g.BufferCount);
            CHECKF(g.SwapEffect == (DXGI_SWAP_EFFECT)k_h_matrix[i].effect, "row %u SwapEffect %d", i, g.SwapEffect);
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_format_matrix)
{
    OPEN2(s);
    static const struct { DXGI_FORMAT f; int eff; int ok; } t[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 }, { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 },
        { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 }, { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 },
        { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 }, { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 },
        { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0 }, { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0 },
        { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_DISCARD, 1 }, { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_SEQUENTIAL, 1 },
        { DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 },
        { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_EFFECT_DISCARD, 1 }, { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_SWAP_EFFECT_SEQUENTIAL, 1 },
    };
    for (unsigned i = 0; i < ARRAY_SIZE_(t); i++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, t[i].f, 4, t[i].eff);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, w, &d, &sc);
        if (t[i].ok) CHECKF(sc_okish(hr), "format %d effect %d -> %s", t[i].f, t[i].eff, d3dh_hr_name(hr));
        else CHECKF(hr == DXGI_ERROR_INVALID_CALL, "format %d effect %d -> %s", t[i].f, t[i].eff, d3dh_hr_name(hr));
        if (sc) SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_format_unknown)
{
    OPEN2(s);
    for (int eff = 0; eff <= 4; eff++) {
        if (eff == 2) continue;
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_UNKNOWN, 2, eff);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
        CHECKF(FAILED(hr), "effect %d accepted FORMAT_UNKNOWN", eff);
        t_note("ForHwnd(FORMAT_UNKNOWN, effect %d) -> %s", eff, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_scaling_modes)
{
    OPEN2(s);
    for (int eff = DXGI_SWAP_EFFECT_DISCARD; eff <= DXGI_SWAP_EFFECT_FLIP_DISCARD; eff++) {
        if (eff == 2) continue;
        for (int sc_ = 0; sc_ <= 2; sc_++) {
            HWND w = sc_window(64, 64);
            DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, eff);
            d.Scaling = sc_;
            IDXGISwapChain1 *sc = NULL;
            HRESULT hr = sc_create1(&s, w, &d, &sc);
            if (sc_ == DXGI_SCALING_STRETCH) CHECKF(sc_okish(hr), "STRETCH effect %d -> %s", eff, d3dh_hr_name(hr));
            else t_note("scaling %d effect %d -> %s", sc_, eff, d3dh_hr_name(hr));
            if (sc) {
                DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
                IDXGISwapChain1_GetDesc1(sc, &g);
                CHECKF(g.Scaling == (DXGI_SCALING)sc_, "scaling %d came back as %d", sc_, g.Scaling);
                SC_REL(sc);
            }
            DestroyWindow(w);
        }
    }
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    d.Scaling = (DXGI_SCALING)3;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    d.Scaling = (DXGI_SCALING)0xFF;
    hr = sc_create1(&s, s.hwnd, &d, &sc);
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_alpha_modes)
{
    OPEN2(s);
    static const struct { int a; const char *n; } t[] = { { 0, "UNSPECIFIED" }, { 1, "PREMULTIPLIED" }, { 2, "STRAIGHT" }, { 3, "IGNORE" } };
    for (unsigned i = 0; i < ARRAY_SIZE_(t); i++) {
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
        d.AlphaMode = t[i].a;
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
        if (t[i].a == DXGI_ALPHA_MODE_UNSPECIFIED || t[i].a == DXGI_ALPHA_MODE_IGNORE) CHECKF(sc_okish(hr), "alpha %s -> %s", t[i].n, d3dh_hr_name(hr));
        else t_note("alpha mode %s on an HWND chain -> %s", t[i].n, d3dh_hr_name(hr));
        if (sc) {
            DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
            IDXGISwapChain1_GetDesc1(sc, &g);
            if (t[i].a == DXGI_ALPHA_MODE_IGNORE) CHECKF(g.AlphaMode == DXGI_ALPHA_MODE_IGNORE, "IGNORE came back %d", g.AlphaMode);
            SC_REL(sc);
        }
    }
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    d.AlphaMode = (DXGI_ALPHA_MODE)4;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_alpha_legacy_default)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC d0 = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *leg;
    if (!sc_okish(sc_create0(&s, &d0, &leg))) { CHECK(0); sc_close(&s); return; }
    IDXGISwapChain1 *sc1 = NULL;
    if (SUCCEEDED(IDXGISwapChain_QueryInterface(leg, &IID_IDXGISwapChain1, (void **)&sc1))) {
        DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0xCC, sizeof g);
        EXPECT_HR(IDXGISwapChain1_GetDesc1(sc1, &g), S_OK);
        EXPECT_EQ(g.Stereo, FALSE);
        EXPECT_EQ(g.Scaling, DXGI_SCALING_STRETCH);
        EXPECT_EQ(g.AlphaMode, DXGI_ALPHA_MODE_IGNORE);
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fd; memset(&fd, 0, sizeof fd);
        EXPECT_HR(IDXGISwapChain1_GetFullscreenDesc(sc1, &fd), S_OK);
        EXPECT_EQ(fd.Windowed, TRUE);
        SAFE_RELEASE(sc1);
    }
    SC_REL(leg);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_stereo)
{
    OPEN2(s);
    BOOL stereo_ok = IDXGIFactory2_IsWindowedStereoEnabled(s.fac2);
    t_note("IsWindowedStereoEnabled -> %d", stereo_ok);
    static const int effs[] = { DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD };
    for (unsigned i = 0; i < 4; i++) {
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, effs[i]);
        d.Stereo = TRUE;
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
        if (!sc_is_flip(effs[i]) || !stereo_ok) CHECKF(FAILED(hr), "stereo with effect %d and no windowed stereo support accepted", effs[i]);
        t_note("stereo effect %d -> %s", effs[i], d3dh_hr_name(hr));
        if (sc) {
            DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
            IDXGISwapChain1_GetDesc1(sc, &g);
            EXPECT_EQ(g.Stereo, TRUE);
            SC_REL(sc);
        }
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_is_windowed_stereo_enabled_stable)
{
    OPEN2(s);
    BOOL a = IDXGIFactory2_IsWindowedStereoEnabled(s.fac2), b = IDXGIFactory2_IsWindowedStereoEnabled(s.fac2);
    EXPECT_EQ(a, b);
    CHECK(a == TRUE || a == FALSE);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_flag_waitable_needs_flip)
{
    OPEN2(s);
    static const struct { int eff; HRESULT hr; } t[] = {
        { DXGI_SWAP_EFFECT_DISCARD, DXGI_ERROR_INVALID_CALL }, { DXGI_SWAP_EFFECT_SEQUENTIAL, DXGI_ERROR_INVALID_CALL },
        { DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, S_OK }, { DXGI_SWAP_EFFECT_FLIP_DISCARD, S_OK },
    };
    for (unsigned i = 0; i < 4; i++) {
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, t[i].eff);
        d.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
        if (t[i].hr == S_OK) CHECKF(sc_okish(hr), "effect %d -> %s", t[i].eff, d3dh_hr_name(hr));
        else CHECKF(hr == t[i].hr, "effect %d -> %s", t[i].eff, d3dh_hr_name(hr));
        if (sc) {
            DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
            IDXGISwapChain1_GetDesc1(sc, &g);
            EXPECT_EQ(g.Flags, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
            SC_REL(sc);
        }
    }
    sc_close(&s);
}

static const struct { UINT flag; const char *n; int ok; } k_flags[] = {
    { DXGI_SWAP_CHAIN_FLAG_NONPREROTATED, "NONPREROTATED", 0 },
    { DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, "ALLOW_MODE_SWITCH", 1 },
    { DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE, "GDI_COMPATIBLE", 1 },
    { DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT, "RESTRICTED_CONTENT", 1 },
    { DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER, "RESTRICT_SHARED_RESOURCE_DRIVER", 1 },
    { DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY, "DISPLAY_ONLY", 0 },
    { DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, "FRAME_LATENCY_WAITABLE_OBJECT", 1 },
    { DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER, "FOREGROUND_LAYER", 0 },
    { DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO, "FULLSCREEN_VIDEO", 0 },
    { DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO, "YUV_VIDEO", 0 },
    { DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING, "ALLOW_TEARING", 1 },
};

TEST(dxgi_swapchain_hwnd_flag_table_flip)
{
    OPEN2(s);
    for (unsigned i = 0; i < ARRAY_SIZE_(k_flags); i++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
        d.Flags = k_flags[i].flag;
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, w, &d, &sc);
        /* flag acceptance on flip chains depends on OS build and driver: record only */
        t_note("flip chain flag %s (expected %s) -> %s", k_flags[i].n, k_flags[i].ok ? "ok" : "rejected", d3dh_hr_name(hr));
        CHECKF(sc_okish(hr) || hr == DXGI_ERROR_INVALID_CALL || hr == E_INVALIDARG || hr == DXGI_ERROR_UNSUPPORTED, "flag %s -> %s", k_flags[i].n, d3dh_hr_name(hr));
        if (sc) {
            DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
            IDXGISwapChain1_GetDesc1(sc, &g);
            CHECKF(g.Flags == k_flags[i].flag, "flag %s came back as 0x%x", k_flags[i].n, g.Flags);
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_flag_hw_protected)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    d.Flags = DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
    t_note("HW_PROTECTED on a WARP device -> %s", d3dh_hr_name(hr));
    if (sc) SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_flag_undefined_bits)
{
    OPEN2(s);
    static const UINT bad[] = { 0x2000, 0x10000, 0x80000000 };
    for (unsigned i = 0; i < ARRAY_SIZE_(bad); i++) {
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
        d.Flags = bad[i];
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
        CHECKF(FAILED(hr), "undefined flag 0x%x accepted", bad[i]);
        t_note("flags 0x%x -> %s", bad[i], d3dh_hr_name(hr));
        if (sc) SAFE_RELEASE(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_ms_rules)
{
    OPEN2(s);
    for (int eff = 0; eff <= 4; eff++) {
        if (eff == 2) continue;
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 4, eff);
        d.SampleDesc.Count = 4;
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
        if (sc_is_flip(eff)) EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
        else t_note("4x MSAA bitblt effect %d -> %s", eff, d3dh_hr_name(hr));
        if (sc) SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_hwnd_usage_values)
{
    OPEN2(s);
    static const DXGI_USAGE u[] = { DXGI_USAGE_RENDER_TARGET_OUTPUT, DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT,
        DXGI_USAGE_SHADER_INPUT, DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_RENDER_TARGET_OUTPUT };
    for (unsigned i = 0; i < ARRAY_SIZE_(u); i++) {
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
        d.BufferUsage = u[i];
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
        t_note("BufferUsage 0x%x -> %s", u[i], d3dh_hr_name(hr));
        if (i < 2) CHECKF(sc_okish(hr), "BufferUsage 0x%x -> %s", u[i], d3dh_hr_name(hr));
        if (sc) {
            DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
            IDXGISwapChain1_GetDesc1(sc, &g);
            CHECKF(g.BufferUsage == u[i], "BufferUsage 0x%x came back 0x%x", u[i], g.BufferUsage);
            SC_REL(sc);
        }
    }
    sc_close(&s);
}

/* ---- composition / core window ------------------------------------------- */
TEST(dxgi_swapchain_composition_null_args)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    d.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    IDXGISwapChain1 *sc = (IDXGISwapChain1 *)(ULONG_PTR)1;
    EXPECT_HR(IDXGIFactory2_CreateSwapChainForComposition(s.fac2, NULL, &d, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIFactory2_CreateSwapChainForComposition(s.fac2, s.unk, NULL, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    HRESULT hr = IDXGIFactory2_CreateSwapChainForComposition(s.fac2, s.unk, &d, NULL, NULL);
    CHECK(FAILED(hr));
    t_note("ForComposition(ppSwapChain=NULL) -> %s", d3dh_hr_name(hr));
    sc_close(&s);
}

TEST(dxgi_swapchain_composition_create)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(96, 48, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    d.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = IDXGIFactory2_CreateSwapChainForComposition(s.fac2, s.unk, &d, NULL, &sc);
    if (FAILED(hr)) { t_note("CreateSwapChainForComposition -> %s (no DirectComposition here)", d3dh_hr_name(hr)); t_skip("composition swap chains unavailable"); sc_close(&s); return; }
    DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
    EXPECT_HR(IDXGISwapChain1_GetDesc1(sc, &g), S_OK);
    EXPECT_EQ(g.Width, 96); EXPECT_EQ(g.Height, 48);
    EXPECT_EQ(g.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(g.BufferCount, 2);
    EXPECT_EQ(g.SwapEffect, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    EXPECT_EQ(g.AlphaMode, DXGI_ALPHA_MODE_PREMULTIPLIED);
    HWND h = (HWND)(ULONG_PTR)1;
    hr = IDXGISwapChain1_GetHwnd(sc, &h);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    void *cw = (void *)(ULONG_PTR)1;
    hr = IDXGISwapChain1_GetCoreWindow(sc, &IID_IUnknown, &cw);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fd;
    hr = IDXGISwapChain1_GetFullscreenDesc(sc, &fd);
    t_note("GetFullscreenDesc on a composition chain -> %s", d3dh_hr_name(hr));
    CHECK(sc_okish(IDXGISwapChain1_Present(sc, 0, 0)));
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_composition_zero_size)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(0, 0, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = IDXGIFactory2_CreateSwapChainForComposition(s.fac2, s.unk, &d, NULL, &sc);
    CHECK(FAILED(hr));   /* a composition surface has no window to take a size from */
    t_note("ForComposition with 0x0 -> %s", d3dh_hr_name(hr));
    if (sc) SAFE_RELEASE(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_composition_bitblt)
{
    OPEN2(s);
    for (int eff = 0; eff <= 1; eff++) {
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, eff);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = IDXGIFactory2_CreateSwapChainForComposition(s.fac2, s.unk, &d, NULL, &sc);
        t_note("ForComposition with bitblt effect %d -> %s", eff, d3dh_hr_name(hr));
        if (sc) SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_corewindow_invalid)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc = (IDXGISwapChain1 *)(ULONG_PTR)1;
    EXPECT_HR(IDXGIFactory2_CreateSwapChainForCoreWindow(s.fac2, NULL, (IUnknown *)s.fac2, &d, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIFactory2_CreateSwapChainForCoreWindow(s.fac2, s.unk, NULL, &d, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIFactory2_CreateSwapChainForCoreWindow(s.fac2, s.unk, (IUnknown *)s.fac2, NULL, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    sc = NULL;
    /* A desktop COM object is not a CoreWindow. */
    HRESULT hr = IDXGIFactory2_CreateSwapChainForCoreWindow(s.fac2, s.unk, (IUnknown *)s.fac2, &d, NULL, &sc);
    CHECK(FAILED(hr));
    t_note("ForCoreWindow(factory as window) -> %s", d3dh_hr_name(hr));
    if (sc) SAFE_RELEASE(sc);
    sc_close(&s);
}

/* ---- Direct3D 12 queue ------------------------------------------------------ */
TEST(dxgi_swapchain_d12_create_basic)
{
    SC12 s; if (!sc12_open(&s)) return;
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD), g;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = sc12_create(&s, s.hwnd, &d, &sc);
    CHECKF(sc_okish(hr), "create: %s", d3dh_hr_name(hr));
    if (sc) {
        memset(&g, 0, sizeof g);
        EXPECT_HR(IDXGISwapChain1_GetDesc1(sc, &g), S_OK);
        EXPECT_EQ(g.Width, 64); EXPECT_EQ(g.Height, 48); EXPECT_EQ(g.BufferCount, 2);
        void *res = NULL;
        EXPECT_HR(IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D12Resource, &res), S_OK);
        if (res) ID3D12Resource_Release((ID3D12Resource *)res);
        res = (void *)(ULONG_PTR)1;
        HRESULT h2 = IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, &res);
        CHECKF(h2 == E_NOINTERFACE, "GetBuffer(ID3D11Texture2D) on a D3D12 chain = %s", d3dh_hr_name(h2));
        res = (void *)(ULONG_PTR)1;
        EXPECT_HR(IDXGISwapChain1_GetBuffer(sc, 2, &IID_ID3D12Resource, &res), DXGI_ERROR_INVALID_CALL);
        SC_REL(sc);
    }
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_device_instead_of_queue)
{
    SC12 s; if (!sc12_open(&s)) return;
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = IDXGIFactory2_CreateSwapChainForHwnd(s.fac2, (IUnknown *)s.dev, s.hwnd, &d, NULL, NULL, &sc);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    CHECK(sc == NULL);
    if (sc) SAFE_RELEASE(sc);
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_effects_and_counts)
{
    SC12 s; if (!sc12_open(&s)) return;
    static const struct { UINT count; int eff; int ok; } t[] = {
        { 2, DXGI_SWAP_EFFECT_DISCARD, 0 }, { 2, DXGI_SWAP_EFFECT_SEQUENTIAL, 0 }, { 1, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 },
        { 1, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0 }, { 0, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 }, { 17, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0 },
        { 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 }, { 3, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 1 }, { 16, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1 },
    };
    for (unsigned i = 0; i < ARRAY_SIZE_(t); i++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, t[i].count, t[i].eff);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc12_create(&s, w, &d, &sc);
        if (t[i].ok) CHECKF(sc_okish(hr), "row %u -> %s", i, d3dh_hr_name(hr));
        else CHECKF(hr == DXGI_ERROR_INVALID_CALL, "row %u (count %u effect %d) -> %s", i, t[i].count, t[i].eff, d3dh_hr_name(hr));
        if (sc) SC_REL(sc);
        DestroyWindow(w);
    }
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_formats)
{
    SC12 s; if (!sc12_open(&s)) return;
    static const struct { DXGI_FORMAT f; int ok; } t[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM, 1 }, { DXGI_FORMAT_B8G8R8A8_UNORM, 1 }, { DXGI_FORMAT_R10G10B10A2_UNORM, 1 },
        { DXGI_FORMAT_R16G16B16A16_FLOAT, 1 }, { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0 }, { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 0 },
        { DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, 0 },
    };
    for (unsigned i = 0; i < ARRAY_SIZE_(t); i++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, t[i].f, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc12_create(&s, w, &d, &sc);
        if (t[i].ok) CHECKF(sc_okish(hr), "format %d -> %s", t[i].f, d3dh_hr_name(hr));
        else CHECKF(hr == DXGI_ERROR_INVALID_CALL, "format %d -> %s", t[i].f, d3dh_hr_name(hr));
        if (sc) SC_REL(sc);
        DestroyWindow(w);
    }
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_getdevice)
{
    SC12 s; if (!sc12_open(&s)) return;
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc12_create(&s, s.hwnd, &d, &sc))) { CHECK(0); sc12_close(&s); return; }
    ID3D12Device *dv = NULL;
    HRESULT hr = IDXGISwapChain1_GetDevice(sc, &IID_ID3D12Device, (void **)&dv);
    t_note("GetDevice(ID3D12Device) on a D3D12 chain -> %s", d3dh_hr_name(hr));
    if (dv) { CHECK(dv == s.dev); SAFE_RELEASE(dv); }
    ID3D12CommandQueue *q = NULL;
    hr = IDXGISwapChain1_GetDevice(sc, &IID_ID3D12CommandQueue, (void **)&q);
    t_note("GetDevice(ID3D12CommandQueue) on a D3D12 chain -> %s", d3dh_hr_name(hr));
    if (q) { CHECK(q == s.q); SAFE_RELEASE(q); }
    void *p = (void *)(ULONG_PTR)1;
    hr = IDXGISwapChain1_GetDevice(sc, &IID_ID3D11Device, &p);
    CHECK(FAILED(hr));
    SC_REL(sc);
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_flags)
{
    SC12 s; if (!sc12_open(&s)) return;
    for (unsigned i = 0; i < ARRAY_SIZE_(k_flags); i++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
        d.Flags = k_flags[i].flag;
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc12_create(&s, w, &d, &sc);
        int ok = k_flags[i].ok;
        if (k_flags[i].flag == DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE || k_flags[i].flag == DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT ||
            k_flags[i].flag == DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER) ok = 0;
        if (ok) CHECKF(sc_okish(hr), "d3d12 flag %s -> %s", k_flags[i].n, d3dh_hr_name(hr));
        else CHECKF(hr == DXGI_ERROR_INVALID_CALL, "d3d12 flag %s -> %s", k_flags[i].n, d3dh_hr_name(hr));
        if (sc) SC_REL(sc);
        DestroyWindow(w);
    }
    sc12_close(&s);
}
