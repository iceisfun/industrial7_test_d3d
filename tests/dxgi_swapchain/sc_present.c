/* Present / Present1, present counts, statistics, window-only target resize, fullscreen exit error codes. */
#include "sc.h"

#define OPEN2(s) SC s; if (!sc_open2(&s, 0)) return

static const int k_effects[] = { DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD };
static const char *k_effn[] = { "DISCARD", "SEQUENTIAL", "FLIP_SEQUENTIAL", "FLIP_DISCARD" };

/* Run fn for every swap effect with a fresh chain. */
typedef void (*effect_fn)(SC *s, IDXGISwapChain1 *sc, int eff);
static void for_each_effect(SC *s, effect_fn fn, UINT count, DXGI_FORMAT fmt, UINT flags)
{
    for (unsigned i = 0; i < 4; i++) {
        HWND w = sc_window(64, 48);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, fmt, count, k_effects[i]);
        d.Flags = flags;
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(s, w, &d, &sc);
        if (!sc_okish(hr)) { CHECKF(0, "%s: create -> %s", k_effn[i], d3dh_hr_name(hr)); DestroyWindow(w); continue; }
        fn(s, sc, k_effects[i]);
        SC_REL(sc);
        DestroyWindow(w);
    }
}

static void fn_sync_range(SC *s, IDXGISwapChain1 *sc, int eff)
{
    for (UINT i = 0; i < 10; i++) {
        HRESULT hr = IDXGISwapChain1_Present(sc, i, 0);
        if (i <= 4) CHECKF(sc_okish(hr), "eff %d sync %u -> %s", eff, i, d3dh_hr_name(hr));
        else CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d sync %u -> %s", eff, i, d3dh_hr_name(hr));
    }
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0xFFFFFFFF, 0), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
}
TEST(dxgi_swapchain_present_sync_interval_range) { OPEN2(s); for_each_effect(&s, fn_sync_range, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static void fn_sync_range_p1(SC *s, IDXGISwapChain1 *sc, int eff)
{
    DXGI_PRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    for (UINT i = 0; i < 8; i++) {
        HRESULT hr = IDXGISwapChain1_Present1(sc, i, 0, &pp);
        if (i <= 4) CHECKF(sc_okish(hr), "eff %d Present1 sync %u -> %s", eff, i, d3dh_hr_name(hr));
        else CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d Present1 sync %u -> %s", eff, i, d3dh_hr_name(hr));
    }
    EXPECT_HR(IDXGISwapChain1_Present1(sc, 0, 0, NULL), S_OK);
}
TEST(dxgi_swapchain_present1_sync_interval_range) { OPEN2(s); for_each_effect(&s, fn_sync_range_p1, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static void fn_count(SC *s, IDXGISwapChain1 *sc, int eff)
{
    UINT c = ~0u, e;
    EXPECT_HR(IDXGISwapChain1_GetLastPresentCount(sc, &c), S_OK);
    CHECKF(c == 0, "eff %d initial count %u", eff, c);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    e = c + 1;
    IDXGISwapChain1_GetLastPresentCount(sc, &c);
    CHECKF(c == e, "eff %d after Present: %u want %u", eff, c, e);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 10, 0), DXGI_ERROR_INVALID_CALL);
    IDXGISwapChain1_GetLastPresentCount(sc, &c);
    CHECKF(c == e, "eff %d after failed Present: %u want %u", eff, c, e);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_TEST), S_OK);
    IDXGISwapChain1_GetLastPresentCount(sc, &c);
    CHECKF(c == e, "eff %d after TEST present: %u want %u", eff, c, e);
    EXPECT_HR(IDXGISwapChain1_Present1(sc, 0, DXGI_PRESENT_TEST, NULL), S_OK);
    IDXGISwapChain1_GetLastPresentCount(sc, &c);
    CHECKF(c == e, "eff %d after Present1 TEST: %u want %u", eff, c, e);
    EXPECT_HR(IDXGISwapChain1_Present1(sc, 0, 0, NULL), S_OK);
    e++;
    IDXGISwapChain1_GetLastPresentCount(sc, &c);
    CHECKF(c == e, "eff %d after Present1: %u want %u", eff, c, e);
    for (int i = 0; i < 5; i++) IDXGISwapChain1_Present(sc, 0, 0);
    e += 5;
    IDXGISwapChain1_GetLastPresentCount(sc, &c);
    CHECKF(c == e, "eff %d after 5 more: %u want %u", eff, c, e);
}
TEST(dxgi_swapchain_present_count_semantics) { OPEN2(s); for_each_effect(&s, fn_count, 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

TEST(dxgi_swapchain_present_count_per_chain)
{
    OPEN2(s);
    HWND w2 = sc_window(32, 32);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(32, 32, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *a, *b;
    if (!sc_okish(sc_create1(&s, s.hwnd, &d, &a))) { CHECK(0); DestroyWindow(w2); sc_close(&s); return; }
    if (!sc_okish(sc_create1(&s, w2, &d, &b))) { CHECK(0); SC_REL(a); DestroyWindow(w2); sc_close(&s); return; }
    for (int i = 0; i < 3; i++) IDXGISwapChain1_Present(a, 0, 0);
    IDXGISwapChain1_Present(b, 0, 0);
    UINT ca = 0, cb = 0;
    IDXGISwapChain1_GetLastPresentCount(a, &ca); IDXGISwapChain1_GetLastPresentCount(b, &cb);
    EXPECT_EQ(ca, 3); EXPECT_EQ(cb, 1);
    SC_REL(a); SC_REL(b);
    DestroyWindow(w2);
    sc_close(&s);
}

static void fn_flags_test(SC *s, IDXGISwapChain1 *sc, int eff)
{
    static const UINT ok_flags[] = { 0, DXGI_PRESENT_TEST, DXGI_PRESENT_DO_NOT_SEQUENCE, DXGI_PRESENT_RESTART,
        DXGI_PRESENT_TEST | DXGI_PRESENT_DO_NOT_SEQUENCE };
    for (unsigned i = 0; i < ARRAY_SIZE_(ok_flags); i++) {
        HRESULT hr = IDXGISwapChain1_Present(sc, 0, ok_flags[i]);
        CHECKF(sc_okish(hr), "eff %d Present flags 0x%x -> %s", eff, ok_flags[i], d3dh_hr_name(hr));
    }
}
TEST(dxgi_swapchain_present_flags_basic) { OPEN2(s); for_each_effect(&s, fn_flags_test, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static void fn_do_not_wait(SC *s, IDXGISwapChain1 *sc, int eff)
{
    for (int i = 0; i < 4; i++) {
        HRESULT hr = IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_DO_NOT_WAIT);
        CHECKF(sc_okish(hr) || hr == DXGI_ERROR_WAS_STILL_DRAWING, "eff %d DO_NOT_WAIT -> %s", eff, d3dh_hr_name(hr));
    }
    HRESULT hr = IDXGISwapChain1_Present(sc, 1, DXGI_PRESENT_DO_NOT_WAIT | DXGI_PRESENT_TEST);
    CHECKF(sc_okish(hr) || hr == DXGI_ERROR_WAS_STILL_DRAWING, "eff %d DO_NOT_WAIT|TEST -> %s", eff, d3dh_hr_name(hr));
}
TEST(dxgi_swapchain_present_do_not_wait) { OPEN2(s); for_each_effect(&s, fn_do_not_wait, 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

TEST(dxgi_swapchain_present_test_does_not_unbind)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *rtv = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t);
    ID3D11Device_CreateRenderTargetView(s.dev, (ID3D11Resource *)t, NULL, &rtv);
    ID3D11DeviceContext_OMSetRenderTargets(s.ctx, 1, &rtv, NULL);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_TEST), S_OK);
    ID3D11RenderTargetView *bound = NULL;
    ID3D11DeviceContext_OMGetRenderTargets(s.ctx, 1, &bound, NULL);
    CHECK(bound == rtv);
    SAFE_RELEASE(bound);
    ID3D11DeviceContext_OMSetRenderTargets(s.ctx, 0, NULL, NULL);
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    SC_REL(sc);
    sc_close(&s);
}

static void fn_unbind(SC *s, IDXGISwapChain1 *sc, int eff)
{
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *rtv = NULL, *bound = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t);
    ID3D11Device_CreateRenderTargetView(s->dev, (ID3D11Resource *)t, NULL, &rtv);
    ID3D11DeviceContext_OMSetRenderTargets(s->ctx, 1, &rtv, NULL);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    ID3D11DeviceContext_OMGetRenderTargets(s->ctx, 1, &bound, NULL);
    if (sc_is_flip(eff)) CHECKF(bound == NULL, "eff %d: back buffer 0 still bound after flip present", eff);
    else CHECKF(bound == rtv, "eff %d: bitblt present unbound the render target", eff);
    SAFE_RELEASE(bound);
    /* DO_NOT_SEQUENCE keeps the binding in every model */
    ID3D11DeviceContext_OMSetRenderTargets(s->ctx, 1, &rtv, NULL);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_DO_NOT_SEQUENCE), S_OK);
    ID3D11DeviceContext_OMGetRenderTargets(s->ctx, 1, &bound, NULL);
    t_note("eff %d: after DO_NOT_SEQUENCE present RTV is %s", eff, bound ? "still bound" : "unbound");
    SAFE_RELEASE(bound);
    ID3D11DeviceContext_OMSetRenderTargets(s->ctx, 0, NULL, NULL);
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
}
TEST(dxgi_swapchain_present_flip_unbinds_backbuffer) { OPEN2(s); for_each_effect(&s, fn_unbind, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static void fn_dirty(SC *s, IDXGISwapChain1 *sc, int eff)
{
    RECT r[2] = { { 0, 0, 16, 16 }, { 20, 10, 60, 40 } };
    DXGI_PRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    pp.DirtyRectsCount = 1; pp.pDirtyRects = r;
    HRESULT hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(sc_okish(hr), "eff %d one dirty rect -> %s", eff, d3dh_hr_name(hr));
    pp.DirtyRectsCount = 2;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(sc_okish(hr), "eff %d two dirty rects -> %s", eff, d3dh_hr_name(hr));
    RECT full = { 0, 0, 64, 48 };
    pp.DirtyRectsCount = 1; pp.pDirtyRects = &full;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(sc_okish(hr), "eff %d full-surface dirty rect -> %s", eff, d3dh_hr_name(hr));
    pp.DirtyRectsCount = 0; pp.pDirtyRects = NULL;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(sc_okish(hr), "eff %d empty parameters -> %s", eff, d3dh_hr_name(hr));
    /* TEST with dirty rects is still a no-op success */
    pp.DirtyRectsCount = 1; pp.pDirtyRects = r;
    hr = IDXGISwapChain1_Present1(sc, 0, DXGI_PRESENT_TEST, &pp);
    CHECKF(sc_okish(hr), "eff %d TEST with dirty rect -> %s", eff, d3dh_hr_name(hr));
}
TEST(dxgi_swapchain_present1_dirty_rects) { OPEN2(s); for_each_effect(&s, fn_dirty, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static void fn_dirty_bad(SC *s, IDXGISwapChain1 *sc, int eff)
{
    DXGI_PRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    pp.DirtyRectsCount = 1; pp.pDirtyRects = NULL;
    HRESULT hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    t_note("eff %d: DirtyRectsCount=1 with NULL array -> %s", eff, d3dh_hr_name(hr));
    RECT big = { 0, 0, 5000, 5000 };
    pp.pDirtyRects = &big;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    t_note("eff %d: dirty rect larger than the buffer -> %s", eff, d3dh_hr_name(hr));
    RECT inv = { 30, 30, 10, 10 };
    pp.pDirtyRects = &inv;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    t_note("eff %d: inverted dirty rect -> %s", eff, d3dh_hr_name(hr));
    /* the chain stays usable */
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
}
TEST(dxgi_swapchain_present1_dirty_rects_invalid) { OPEN2(s); for_each_effect(&s, fn_dirty_bad, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static void fn_scroll(SC *s, IDXGISwapChain1 *sc, int eff)
{
    RECT scroll = { 0, 0, 32, 32 }; POINT off = { 8, 8 };
    RECT dirty = { 0, 0, 40, 40 };
    DXGI_PRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    pp.pScrollRect = &scroll; pp.pScrollOffset = &off;
    pp.DirtyRectsCount = 1; pp.pDirtyRects = &dirty;
    HRESULT hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    t_note("eff %d: scroll rect + offset + dirty -> %s", eff, d3dh_hr_name(hr));
    pp.DirtyRectsCount = 0; pp.pDirtyRects = NULL;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    t_note("eff %d: scroll rect + offset only -> %s", eff, d3dh_hr_name(hr));
    pp.pScrollOffset = NULL;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    t_note("eff %d: scroll rect without offset -> %s", eff, d3dh_hr_name(hr));
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
}
TEST(dxgi_swapchain_present1_scroll) { OPEN2(s); for_each_effect(&s, fn_scroll, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

TEST(dxgi_swapchain_present_tearing_requires_flag)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    HRESULT hr = IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_ALLOW_TEARING);
    CHECKF(FAILED(hr), "ALLOW_TEARING without the swap chain flag succeeded (%s)", d3dh_hr_name(hr));
    t_note("Present(ALLOW_TEARING) without chain flag -> %s", d3dh_hr_name(hr));
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_present_tearing_with_flag)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    d.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = sc_create1(&s, s.hwnd, &d, &sc);
    if (!sc_okish(hr)) { t_note("ALLOW_TEARING chain -> %s", d3dh_hr_name(hr)); t_skip("tearing not supported"); sc_close(&s); return; }
    hr = IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_ALLOW_TEARING);
    CHECKF(sc_okish(hr), "ALLOW_TEARING, sync 0 -> %s", d3dh_hr_name(hr));
    hr = IDXGISwapChain1_Present(sc, 1, DXGI_PRESENT_ALLOW_TEARING);
    CHECKF(FAILED(hr), "ALLOW_TEARING with sync interval 1 succeeded (%s)", d3dh_hr_name(hr));
    t_note("Present(1, ALLOW_TEARING) -> %s", d3dh_hr_name(hr));
    hr = IDXGISwapChain1_Present(sc, 0, 0);
    CHECK(sc_okish(hr));
    IDXGIFactory5 *f5 = NULL;
    if (SUCCEEDED(IDXGIFactory2_QueryInterface(s.fac2, &IID_IDXGIFactory5, (void **)&f5))) {
        BOOL sup = 0xFF;
        hr = IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &sup, sizeof sup);
        EXPECT_HR(hr, S_OK);
        t_note("PRESENT_ALLOW_TEARING feature support -> %d", sup);
        EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &sup, 1), E_INVALIDARG);
        EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, NULL, sizeof sup), E_INVALIDARG);
        EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, (DXGI_FEATURE)99, &sup, sizeof sup), E_INVALIDARG);
        SAFE_RELEASE(f5);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_present_hidden_window)
{
    OPEN2(s);
    CHECK(!IsWindowVisible(s.hwnd));
    for (unsigned i = 0; i < 4; i++) {
        HWND w = sc_window(40, 40);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(40, 40, DXGI_FORMAT_R8G8B8A8_UNORM, 2, k_effects[i]);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, w, &d, &sc);
        if (sc) {
            hr = IDXGISwapChain1_Present(sc, 0, 0);
            CHECKF(sc_okish(hr), "%s hidden window Present -> %s", k_effn[i], d3dh_hr_name(hr));
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

static void fn_frame_stats(SC *s, IDXGISwapChain1 *sc, int eff)
{
    DXGI_FRAME_STATISTICS st; memset(&st, 0xCC, sizeof st);
    HRESULT hr = IDXGISwapChain1_GetFrameStatistics(sc, &st);
    t_note("eff %d: GetFrameStatistics before any Present -> %s", eff, d3dh_hr_name(hr));
    if (eff == DXGI_SWAP_EFFECT_DISCARD || eff == DXGI_SWAP_EFFECT_SEQUENTIAL)
        CHECKF(FAILED(hr), "windowed bitblt chain returned statistics (%s)", d3dh_hr_name(hr));
    for (int i = 0; i < 3; i++) IDXGISwapChain1_Present(sc, 1, 0);
    memset(&st, 0xCC, sizeof st);
    hr = IDXGISwapChain1_GetFrameStatistics(sc, &st);
    t_note("eff %d: GetFrameStatistics after presents -> %s (PresentCount %u PresentRefreshCount %u SyncRefreshCount %u)",
           eff, d3dh_hr_name(hr), st.PresentCount, st.PresentRefreshCount, st.SyncRefreshCount);
    if (hr == S_OK) CHECK(st.PresentCount <= 3 + 1);
    if (eff == DXGI_SWAP_EFFECT_DISCARD || eff == DXGI_SWAP_EFFECT_SEQUENTIAL) CHECK(FAILED(hr));
}
TEST(dxgi_swapchain_frame_statistics) { OPEN2(s); for_each_effect(&s, fn_frame_stats, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

TEST(dxgi_swapchain_getcurrentbackbufferindex_d3d11)
{
    OPEN2(s);
    for (unsigned i = 0; i < 4; i++) {
        HWND w = sc_window(40, 40);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(40, 40, DXGI_FORMAT_R8G8B8A8_UNORM, 3, k_effects[i]);
        IDXGISwapChain1 *sc = NULL; IDXGISwapChain3 *s3 = NULL;
        sc_create1(&s, w, &d, &sc);
        if (sc && SUCCEEDED(IDXGISwapChain1_QueryInterface(sc, &IID_IDXGISwapChain3, (void **)&s3))) {
            for (int j = 0; j < 8; j++) {
                UINT idx = IDXGISwapChain3_GetCurrentBackBufferIndex(s3);
                CHECKF(idx == 0, "%s: D3D11 flip/bitblt chain index %u after %d presents", k_effn[i], idx, j);
                IDXGISwapChain3_Present(s3, 0, 0);
            }
            SAFE_RELEASE(s3);
        }
        if (sc) SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_resizetarget_windowed)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    static const struct { UINT w, h; } sz[] = { { 200, 120 }, { 100, 100 }, { 320, 64 } };
    for (unsigned i = 0; i < ARRAY_SIZE_(sz); i++) {
        DXGI_MODE_DESC m; memset(&m, 0, sizeof m);
        m.Width = sz[i].w; m.Height = sz[i].h;
        HRESULT hr = IDXGISwapChain1_ResizeTarget(sc, &m);
        CHECKF(hr == S_OK, "ResizeTarget %ux%u -> %s", sz[i].w, sz[i].h, d3dh_hr_name(hr));
        RECT r = { 0 }; GetClientRect(s.hwnd, &r);
        CHECKF((UINT)r.right == sz[i].w && (UINT)r.bottom == sz[i].h, "client %ldx%ld want %ux%u", r.right, r.bottom, sz[i].w, sz[i].h);
    }
    HRESULT hr = IDXGISwapChain1_ResizeTarget(sc, NULL);
    CHECK(FAILED(hr));
    t_note("ResizeTarget(NULL) -> %s", d3dh_hr_name(hr));
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resizetarget_does_not_resize_buffers)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    DXGI_SWAP_CHAIN_DESC1 before, after;
    IDXGISwapChain1_GetDesc1(sc, &before);
    DXGI_MODE_DESC m; memset(&m, 0, sizeof m); m.Width = 210; m.Height = 130;
    EXPECT_HR(IDXGISwapChain1_ResizeTarget(sc, &m), S_OK);
    IDXGISwapChain1_GetDesc1(sc, &after);
    /* buffers are only resized once the window message is processed by the app calling ResizeBuffers */
    CHECK(before.Width == after.Width || after.Width == 210);
    t_note("buffer width %u -> %u after ResizeTarget(210x130) without message pump", before.Width, after.Width);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_setfullscreenstate_false)
{
    OPEN2(s);
    for (unsigned i = 0; i < 4; i++) {
        HWND w = sc_window(40, 40);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(40, 40, DXGI_FORMAT_R8G8B8A8_UNORM, 2, k_effects[i]);
        IDXGISwapChain1 *sc = NULL;
        sc_create1(&s, w, &d, &sc);
        if (sc) {
            EXPECT_HR(IDXGISwapChain1_SetFullscreenState(sc, FALSE, NULL), S_OK);
            EXPECT_HR(IDXGISwapChain1_SetFullscreenState(sc, FALSE, NULL), S_OK);
            BOOL fs = 1; IDXGIOutput *o = (IDXGIOutput *)(ULONG_PTR)1;
            EXPECT_HR(IDXGISwapChain1_GetFullscreenState(sc, &fs, &o), S_OK);
            EXPECT_EQ(fs, FALSE); CHECK(o == NULL);
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_getcontainingoutput)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    HRESULT hr = IDXGISwapChain1_GetContainingOutput(sc, NULL);
    CHECK(FAILED(hr));
    t_note("GetContainingOutput(NULL) -> %s", d3dh_hr_name(hr));
    IDXGIOutput *o = NULL;
    hr = IDXGISwapChain1_GetContainingOutput(sc, &o);
    t_note("GetContainingOutput on a hidden window -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        CHECK(o != NULL);
        DXGI_OUTPUT_DESC od; memset(&od, 0, sizeof od);
        if (o) EXPECT_HR(IDXGIOutput_GetDesc(o, &od), S_OK);
        SAFE_RELEASE(o);
    } else CHECK(o == NULL);
    SC_REL(sc);
    sc_close(&s);
}
