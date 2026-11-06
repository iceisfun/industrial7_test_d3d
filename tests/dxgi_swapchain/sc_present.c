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
    EXPECT_HR(IDXGISwapChain1_Present1(sc, 0, 0, &pp), S_OK);
}
TEST(dxgi_swapchain_present1_sync_interval_range) { OPEN2(s); for_each_effect(&s, fn_sync_range_p1, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

/* IDXGISwapChain1::Present1 dereferences pPresentParameters unconditionally: NULL is an access violation, not an error code. */
static IDXGISwapChain1 *g_nullp_sc;
static void present1_null_params(void *ctx) { IDXGISwapChain1_Present1(g_nullp_sc, 0, (UINT)(ULONG_PTR)ctx, NULL); }
static void fn_null_params(SC *s, IDXGISwapChain1 *sc, int eff)
{
    g_nullp_sc = sc;
    CHECKF(t_try(present1_null_params, (void *)0) == 0xC0000005u, "eff %d Present1(NULL parameters) did not fault", eff);
    CHECKF(t_try(present1_null_params, (void *)(ULONG_PTR)DXGI_PRESENT_TEST) == 0xC0000005u, "eff %d Present1(TEST, NULL parameters) did not fault", eff);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);   /* the chain survives the fault */
}
TEST(dxgi_swapchain_present1_null_params_faults) { OPEN2(s); for_each_effect(&s, fn_null_params, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

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
    DXGI_PRESENT_PARAMETERS pp0; memset(&pp0, 0, sizeof pp0);
    EXPECT_HR(IDXGISwapChain1_Present1(sc, 0, DXGI_PRESENT_TEST, &pp0), S_OK);
    IDXGISwapChain1_GetLastPresentCount(sc, &c);
    CHECKF(c == e, "eff %d after Present1 TEST: %u want %u", eff, c, e);
    EXPECT_HR(IDXGISwapChain1_Present1(sc, 0, 0, &pp0), S_OK);
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

/* Present flag validity, measured on Win10 22H2 for 3-buffer chains after a first plain Present.
 * Defined bits TEST(1) DO_NOT_SEQUENCE(2) RESTART(4) DO_NOT_WAIT(8) USE_DURATION(0x100) are accepted on every model; STEREO_PREFER_RIGHT(0x10),
 * STEREO_TEMPORARY_MONO(0x20), RESTRICT_TO_OUTPUT(0x40) and ALLOW_TEARING(0x200, chain flag missing) are rejected on every model.
 * The bit-blt models also accept combinations (TEST with others) and the undefined bits 0x80, 0x400, 0x800; flip models reject all of those. */
static const UINT k_present_flags[] = { 0, 1, 2, 3, 4, 5, 8, 9, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800 };
static int present_flag_ok(int eff, UINT f)
{
    switch (f) {
    case 0: case 1: case 2: case 4: case 8: case 0x100: return 1;
    case 3: case 5: case 9: case 0x80: case 0x400: case 0x800: return !sc_is_flip((DXGI_SWAP_EFFECT)eff);
    default: return 0;
    }
}
static void fn_flags_matrix(SC *s, IDXGISwapChain1 *sc, int eff)
{
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    for (unsigned i = 0; i < ARRAY_SIZE_(k_present_flags); i++) for (UINT sync = 0; sync < 2; sync++) {
        HRESULT hr = IDXGISwapChain1_Present(sc, sync, k_present_flags[i]);
        /* DO_NOT_WAIT answers WAS_STILL_DRAWING whenever the chain's queue is full: flip chains with a deep queue do so after a few presents */
        if (k_present_flags[i] == DXGI_PRESENT_DO_NOT_WAIT && hr == DXGI_ERROR_WAS_STILL_DRAWING) continue;
        CHECKF(hr == (present_flag_ok(eff, k_present_flags[i]) ? S_OK : DXGI_ERROR_INVALID_CALL), "eff %d sync %u Present flags 0x%x -> %s", eff, sync, k_present_flags[i], d3dh_hr_name(hr));
    }
}
TEST(dxgi_swapchain_present_flags_matrix) { OPEN2(s); for_each_effect(&s, fn_flags_matrix, 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static void fn_flags_test(SC *s, IDXGISwapChain1 *sc, int eff)
{
    static const UINT ok_flags[] = { 0, DXGI_PRESENT_TEST, DXGI_PRESENT_DO_NOT_SEQUENCE, DXGI_PRESENT_RESTART };
    for (unsigned i = 0; i < ARRAY_SIZE_(ok_flags); i++) {
        HRESULT hr = IDXGISwapChain1_Present(sc, 0, ok_flags[i]);
        CHECKF(hr == S_OK, "eff %d Present flags 0x%x -> %s", eff, ok_flags[i], d3dh_hr_name(hr));
    }
    /* TEST combined with DO_NOT_SEQUENCE is a bit-blt-only combination */
    HRESULT hr = IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_TEST | DXGI_PRESENT_DO_NOT_SEQUENCE);
    CHECKF(hr == (sc_is_flip((DXGI_SWAP_EFFECT)eff) ? DXGI_ERROR_INVALID_CALL : S_OK), "eff %d TEST|DO_NOT_SEQUENCE -> %s", eff, d3dh_hr_name(hr));
}
TEST(dxgi_swapchain_present_flags_basic) { OPEN2(s); for_each_effect(&s, fn_flags_test, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static void fn_do_not_wait(SC *s, IDXGISwapChain1 *sc, int eff)
{
    for (int i = 0; i < 4; i++) {
        HRESULT hr = IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_DO_NOT_WAIT);
        CHECKF(sc_okish(hr) || hr == DXGI_ERROR_WAS_STILL_DRAWING, "eff %d DO_NOT_WAIT -> %s", eff, d3dh_hr_name(hr));
    }
    HRESULT hr = IDXGISwapChain1_Present(sc, 1, DXGI_PRESENT_DO_NOT_WAIT | DXGI_PRESENT_TEST);
    CHECKF(hr == (sc_is_flip((DXGI_SWAP_EFFECT)eff) ? DXGI_ERROR_INVALID_CALL : S_OK), "eff %d DO_NOT_WAIT|TEST -> %s", eff, d3dh_hr_name(hr));
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
    CHECKF((bound != NULL) == !sc_is_flip((DXGI_SWAP_EFFECT)eff), "eff %d: after Present RTV is %s (flip models unbind, bit-blt keeps)", eff, bound ? "still bound" : "unbound");
    SAFE_RELEASE(bound);
    /* DO_NOT_SEQUENCE does not change that on Win10: flip chains unbind on every Present */
    ID3D11DeviceContext_OMSetRenderTargets(s->ctx, 1, &rtv, NULL);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_DO_NOT_SEQUENCE), S_OK);
    ID3D11DeviceContext_OMGetRenderTargets(s->ctx, 1, &bound, NULL);
    CHECKF((bound != NULL) == !sc_is_flip((DXGI_SWAP_EFFECT)eff), "eff %d: after DO_NOT_SEQUENCE present RTV is %s", eff, bound ? "still bound" : "unbound");
    SAFE_RELEASE(bound);
    ID3D11DeviceContext_OMSetRenderTargets(s->ctx, 0, NULL, NULL);
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
}
TEST(dxgi_swapchain_present_flip_unbinds_backbuffer) { OPEN2(s); for_each_effect(&s, fn_unbind, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

/* Dirty rectangles (Win10 22H2): only the SEQUENTIAL and FLIP_SEQUENTIAL models take them; the DISCARD models reject any count > 0. A rectangle must lie
 * inside the buffer and be non-empty, the first Present of a chain may not carry one, and so may not a TEST present. */
static void fn_dirty(SC *s, IDXGISwapChain1 *sc, int eff)
{
    RECT r[2] = { { 0, 0, 16, 16 }, { 20, 10, 60, 40 } };
    DXGI_PRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    int takes = (eff == DXGI_SWAP_EFFECT_SEQUENTIAL || eff == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
    HRESULT ok = takes ? S_OK : DXGI_ERROR_INVALID_CALL;
    pp.DirtyRectsCount = 1; pp.pDirtyRects = r;
    HRESULT hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d dirty rect on the very first Present -> %s", eff, d3dh_hr_name(hr));
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == ok, "eff %d one dirty rect -> %s", eff, d3dh_hr_name(hr));
    pp.DirtyRectsCount = 2;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == ok, "eff %d two dirty rects -> %s", eff, d3dh_hr_name(hr));
    RECT full = { 0, 0, 64, 48 };
    pp.DirtyRectsCount = 1; pp.pDirtyRects = &full;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == ok, "eff %d full-surface dirty rect -> %s", eff, d3dh_hr_name(hr));
    RECT corner = { 0, 0, 1, 1 }, edge = { 63, 47, 64, 48 };
    pp.pDirtyRects = &corner; hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp); CHECKF(hr == ok, "eff %d 1x1 corner rect -> %s", eff, d3dh_hr_name(hr));
    pp.pDirtyRects = &edge;   hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp); CHECKF(hr == ok, "eff %d last-pixel rect -> %s", eff, d3dh_hr_name(hr));
    pp.DirtyRectsCount = 0; pp.pDirtyRects = NULL;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == S_OK, "eff %d empty parameters -> %s", eff, d3dh_hr_name(hr));
    /* a TEST present never accepts dirty rects */
    pp.DirtyRectsCount = 1; pp.pDirtyRects = r;
    hr = IDXGISwapChain1_Present1(sc, 0, DXGI_PRESENT_TEST, &pp);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d TEST with dirty rect -> %s", eff, d3dh_hr_name(hr));
    /* empty rectangle */
    RECT empty = { 0, 0, 0, 0 };
    pp.pDirtyRects = &empty; hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d empty dirty rect -> %s", eff, d3dh_hr_name(hr));
}
TEST(dxgi_swapchain_present1_dirty_rects) { OPEN2(s); for_each_effect(&s, fn_dirty, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

static IDXGISwapChain1 *g_dr_sc; static DXGI_PRESENT_PARAMETERS *g_dr_pp; static HRESULT g_dr_hr;
static void present1_pp(void *c) { g_dr_hr = IDXGISwapChain1_Present1(g_dr_sc, 0, 0, g_dr_pp); }
static void fn_dirty_bad(SC *s, IDXGISwapChain1 *sc, int eff)
{
    DXGI_PRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);   /* past the first Present, where every rectangle is rejected */
    pp.DirtyRectsCount = 1; pp.pDirtyRects = NULL;
    g_dr_sc = sc; g_dr_pp = &pp; g_dr_hr = 0x77777777;
    DWORD x = t_try(present1_pp, NULL);
    /* the models that take rectangles walk the array unchecked (access violation); the DISCARD models reject the count first */
    if (eff == DXGI_SWAP_EFFECT_SEQUENTIAL || eff == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL) CHECKF(x == 0xC0000005u, "eff %d: DirtyRectsCount=1 with NULL array raised 0x%lx", eff, x);
    else CHECKF(x == 0 && g_dr_hr == DXGI_ERROR_INVALID_CALL, "eff %d: DirtyRectsCount=1 with NULL array: exception 0x%lx hr %s", eff, x, d3dh_hr_name(g_dr_hr));
    HRESULT hr;
    RECT big = { 0, 0, 5000, 5000 };
    pp.pDirtyRects = &big;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d: dirty rect larger than the buffer -> %s", eff, d3dh_hr_name(hr));
    RECT inv = { 30, 30, 10, 10 };
    pp.pDirtyRects = &inv;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d: inverted dirty rect -> %s", eff, d3dh_hr_name(hr));
    RECT neg = { -4, 0, 16, 16 };
    pp.pDirtyRects = &neg;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d: dirty rect with negative left -> %s", eff, d3dh_hr_name(hr));
    /* the chain stays usable */
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
}
TEST(dxgi_swapchain_present1_dirty_rects_invalid) { OPEN2(s); for_each_effect(&s, fn_dirty_bad, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0); sc_close(&s); }

/* Scrolling (Win10 22H2): a scroll rectangle with an offset is rejected by every model; without an offset only FLIP_SEQUENTIAL takes it. */
static void fn_scroll(SC *s, IDXGISwapChain1 *sc, int eff)
{
    RECT scroll = { 0, 0, 32, 32 }; POINT off = { 8, 8 };
    RECT dirty = { 0, 0, 40, 40 };
    DXGI_PRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    pp.pScrollRect = &scroll; pp.pScrollOffset = &off;
    pp.DirtyRectsCount = 1; pp.pDirtyRects = &dirty;
    HRESULT hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d: scroll rect + offset + dirty -> %s", eff, d3dh_hr_name(hr));
    pp.DirtyRectsCount = 0; pp.pDirtyRects = NULL;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "eff %d: scroll rect + offset only -> %s", eff, d3dh_hr_name(hr));
    pp.pScrollOffset = NULL;
    hr = IDXGISwapChain1_Present1(sc, 0, 0, &pp);
    CHECKF(hr == (eff == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL ? S_OK : DXGI_ERROR_INVALID_CALL), "eff %d: scroll rect without offset -> %s", eff, d3dh_hr_name(hr));
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
    PIN_HR("Present(ALLOW_TEARING) without chain flag", hr, DXGI_ERROR_INVALID_CALL);
    SC_REL(sc);
    sc_close(&s);
}

static IDXGIFactory5 *g_f5;
static void cfs_null_data(void *c) { IDXGIFactory5_CheckFeatureSupport(g_f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, NULL, sizeof(BOOL)); }

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
    PIN_HR("Present(1, ALLOW_TEARING)", hr, DXGI_ERROR_INVALID_CALL);
    hr = IDXGISwapChain1_Present(sc, 0, 0);
    CHECK(sc_okish(hr));
    IDXGIFactory5 *f5 = NULL;
    if (SUCCEEDED(IDXGIFactory2_QueryInterface(s.fac2, &IID_IDXGIFactory5, (void **)&f5))) {
        BOOL sup = 0xFF;
        hr = IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &sup, sizeof sup);
        EXPECT_HR(hr, S_OK);
        CHECKF(sup == 1 || sup == 0, "PRESENT_ALLOW_TEARING feature support %d", sup);
        t_note("PRESENT_ALLOW_TEARING feature support -> %d", sup);   /* 1 on the WARP reference; follows the display stack */
        EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &sup, 1), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &sup, 0), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &sup, 8), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, (DXGI_FEATURE)99, &sup, sizeof sup), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, (DXGI_FEATURE)1, &sup, sizeof sup), DXGI_ERROR_INVALID_CALL);
        /* a NULL data pointer is dereferenced: access violation, not an error code */
        g_f5 = f5;
        CHECK(t_try(cfs_null_data, NULL) == 0xC0000005u);
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
    /* bit-blt models have no frame statistics; flip models report DISJOINT until something was presented */
    CHECKF(hr == (sc_is_flip((DXGI_SWAP_EFFECT)eff) ? DXGI_ERROR_FRAME_STATISTICS_DISJOINT : DXGI_ERROR_UNSUPPORTED), "eff %d: GetFrameStatistics before any Present -> %s", eff, d3dh_hr_name(hr));
    for (int i = 0; i < 3; i++) IDXGISwapChain1_Present(sc, 1, 0);
    memset(&st, 0xCC, sizeof st);
    hr = IDXGISwapChain1_GetFrameStatistics(sc, &st);
    CHECKF(hr == (sc_is_flip((DXGI_SWAP_EFFECT)eff) ? S_OK : DXGI_ERROR_UNSUPPORTED), "eff %d: GetFrameStatistics after presents -> %s", eff, d3dh_hr_name(hr));
    t_note("eff %d: GetFrameStatistics after presents (PresentCount %u PresentRefreshCount %u SyncRefreshCount %u; a hidden window gets zero statistics)",
           eff, st.PresentCount, st.PresentRefreshCount, st.SyncRefreshCount);
    if (hr == S_OK) CHECK(st.PresentCount <= 3 + 1);
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
    PIN_HR("ResizeTarget(NULL)", hr, DXGI_ERROR_INVALID_CALL);
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
    CHECKF(before.Width == after.Width, "buffer width %u -> %u after ResizeTarget(210x130) without a message pump", before.Width, after.Width);
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
    PIN_HR("GetContainingOutput(NULL)", hr, DXGI_ERROR_INVALID_CALL);
    IDXGIOutput *o = NULL;
    hr = IDXGISwapChain1_GetContainingOutput(sc, &o);
    PIN_HR("GetContainingOutput on a hidden window", hr, DXGI_ERROR_UNSUPPORTED);
    if (SUCCEEDED(hr)) {
        CHECK(o != NULL);
        DXGI_OUTPUT_DESC od; memset(&od, 0, sizeof od);
        if (o) EXPECT_HR(IDXGIOutput_GetDesc(o, &od), S_OK);
        SAFE_RELEASE(o);
    } else CHECK(o == NULL);
    SC_REL(sc);
    sc_close(&s);
}
