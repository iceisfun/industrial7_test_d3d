/* ResizeBuffers / ResizeBuffers1 rules (windowed, hidden window). */
#include "sc.h"

#define OPEN2(s) SC s; if (!sc_open2(&s, 0)) return

static IDXGISwapChain1 *mkr(SC *s, DXGI_SWAP_EFFECT eff, UINT count, UINT flags, DXGI_FORMAT f)
{
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, f, count, eff);
    d.Flags = flags;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = sc_create1(s, s->hwnd, &d, &sc);
    if (!sc_okish(hr)) { CHECKF(0, "create: %s", d3dh_hr_name(hr)); return NULL; }
    return sc;
}

static const int k_effs[] = { DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD };

TEST(dxgi_swapchain_resize_changes_desc_and_buffers)
{
    OPEN2(s);
    for (unsigned i = 0; i < 4; i++) {
        IDXGISwapChain1 *sc = mkr(&s, k_effs[i], 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        if (!sc) continue;
        HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 3, 100, 80, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        CHECKF(hr == S_OK, "effect %d: %s", k_effs[i], d3dh_hr_name(hr));
        DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
        IDXGISwapChain1_GetDesc1(sc, &g);
        CHECKF(g.Width == 100 && g.Height == 80, "effect %d: size %ux%u", k_effs[i], g.Width, g.Height);
        CHECKF(g.Format == DXGI_FORMAT_B8G8R8A8_UNORM, "effect %d: format %d", k_effs[i], g.Format);
        CHECKF(g.BufferCount == 3, "effect %d: count %u", k_effs[i], g.BufferCount);
        EXPECT_EQ(g.SwapEffect, k_effs[i]);
        ID3D11Texture2D *t = NULL;
        EXPECT_HR(IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t), S_OK);
        if (t) {
            D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
            CHECKF(td.Width == 100 && td.Height == 80 && td.Format == DXGI_FORMAT_B8G8R8A8_UNORM, "effect %d texture %ux%u fmt %d", k_effs[i], td.Width, td.Height, td.Format);
            SAFE_RELEASE(t);
        }
        if (k_effs[i] == DXGI_SWAP_EFFECT_SEQUENTIAL || k_effs[i] == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL) {
            void *p = NULL;
            EXPECT_HR(IDXGISwapChain1_GetBuffer(sc, 2, &IID_ID3D11Texture2D, &p), S_OK);
            if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
        }
        void *q = (void *)(ULONG_PTR)1;
        EXPECT_HR(IDXGISwapChain1_GetBuffer(sc, 3, &IID_ID3D11Texture2D, &q), DXGI_ERROR_INVALID_CALL);
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_zero_args_keep_format_and_count)
{
    OPEN2(s);
    for (unsigned i = 0; i < 4; i++) {
        IDXGISwapChain1 *sc = mkr(&s, k_effs[i], 2, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
        if (!sc) continue;
        HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        CHECKF(hr == S_OK, "effect %d: %s", k_effs[i], d3dh_hr_name(hr));
        RECT r = { 0 }; GetClientRect(s.hwnd, &r);
        DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
        IDXGISwapChain1_GetDesc1(sc, &g);
        CHECKF(g.Width == (UINT)r.right && g.Height == (UINT)r.bottom, "effect %d: %ux%u client %ldx%ld", k_effs[i], g.Width, g.Height, r.right, r.bottom);
        CHECKF(g.Format == DXGI_FORMAT_B8G8R8A8_UNORM, "format changed to %d", g.Format);
        CHECKF(g.BufferCount == 2, "count changed to %u", g.BufferCount);
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_outstanding_reference_fails)
{
    OPEN2(s);
    for (unsigned i = 0; i < 4; i++) {
        IDXGISwapChain1 *sc = mkr(&s, k_effs[i], 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        if (!sc) continue;
        ID3D11Texture2D *t = NULL;
        IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t);
        HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        CHECKF(hr == DXGI_ERROR_INVALID_CALL, "effect %d with texture ref held: %s", k_effs[i], d3dh_hr_name(hr));
        DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
        IDXGISwapChain1_GetDesc1(sc, &g);
        CHECKF(g.Width == 64 && g.Height == 48 && g.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "failed resize modified the desc (%ux%u fmt %d)", g.Width, g.Height, g.Format);
        SAFE_RELEASE(t);
        hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        CHECKF(hr == S_OK, "effect %d after release: %s", k_effs[i], d3dh_hr_name(hr));
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_outstanding_view_fails)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_SEQUENTIAL, 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *rtv = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t);
    ID3D11Device_CreateRenderTargetView(s.dev, (ID3D11Resource *)t, NULL, &rtv);
    SAFE_RELEASE(t);
    /* the view keeps the resource alive */
    HRESULT hrv = IDXGISwapChain1_ResizeBuffers(sc, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    CHECKF(hrv == DXGI_ERROR_INVALID_CALL || hrv == S_OK, "resize with live view -> %s", d3dh_hr_name(hrv));
    PIN_HR("resize with a live RTV", hrv, DXGI_ERROR_INVALID_CALL);
    SAFE_RELEASE(rtv);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0), S_OK);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_outstanding_surface_fails)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    IDXGISurface *su = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_IDXGISurface, (void **)&su);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0), DXGI_ERROR_INVALID_CALL);
    SAFE_RELEASE(su);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0), S_OK);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_does_not_resize_window)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    RECT a, b; GetClientRect(s.hwnd, &a);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 2, 17, 9, DXGI_FORMAT_UNKNOWN, 0), S_OK);
    GetClientRect(s.hwnd, &b);
    CHECK(EqualRect(&a, &b));
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_invalid_arguments)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 17, 32, 32, DXGI_FORMAT_UNKNOWN, 0);
    PIN_HR("ResizeBuffers(count 17)", hr, E_INVALIDARG);
    hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_D32_FLOAT, 0);
    PIN_HR("ResizeBuffers(D32_FLOAT)", hr, E_INVALIDARG);
    hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0);
    PIN_HR("ResizeBuffers(SRGB format on flip chain)", hr, E_INVALIDARG);
    hr = IDXGISwapChain1_ResizeBuffers(sc, 1, 32, 32, DXGI_FORMAT_UNKNOWN, 0);
    PIN_HR("ResizeBuffers(count 1 on flip chain)", hr, E_INVALIDARG);
    hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_UNKNOWN, 0x80000000);
    PIN_HR("ResizeBuffers(undefined flag bits)", hr, DXGI_ERROR_INVALID_CALL);
    DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
    IDXGISwapChain1_GetDesc1(sc, &g);
    CHECKF(g.Width == 64 && g.Height == 48 && g.BufferCount == 2, "failed calls modified the desc (%ux%u count %u)", g.Width, g.Height, g.BufferCount);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 2, 64, 48, DXGI_FORMAT_UNKNOWN, 0), S_OK);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_bitblt_count_range)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_DISCARD, 1, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    for (UINT n = 1; n <= 16; n += 5) {
        HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, n, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        CHECKF(hr == S_OK, "ResizeBuffers(count %u) -> %s", n, d3dh_hr_name(hr));
        DXGI_SWAP_CHAIN_DESC1 g; IDXGISwapChain1_GetDesc1(sc, &g);
        CHECKF(g.BufferCount == n, "count %u reported as %u", n, g.BufferCount);
    }
    HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 17, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    PIN_HR("bitblt ResizeBuffers(count 17)", hr, E_INVALIDARG);
    SC_REL(sc);
    sc_close(&s);
}

/* flags accepted by ResizeBuffers on a windowed bitblt chain created without flags */
TEST(dxgi_swapchain_resize_flags_table)
{
    OPEN2(s);
    for (UINT flag = DXGI_SWAP_CHAIN_FLAG_NONPREROTATED; flag <= DXGI_SWAP_CHAIN_FLAG_RESTRICTED_TO_ALL_HOLOGRAPHIC_DISPLAYS; flag <<= 1) {
        if (flag == DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED) continue;
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_DISCARD);
        IDXGISwapChain *sc = NULL;
        if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); continue; }
        HRESULT hr = IDXGISwapChain_ResizeBuffers(sc, 2, 32, 24, DXGI_FORMAT_B8G8R8A8_UNORM, flag);
        if (flag == DXGI_SWAP_CHAIN_FLAG_NONPREROTATED || flag == DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY || flag == DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO)
            CHECKF(hr == DXGI_ERROR_INVALID_CALL, "flag 0x%x -> %s", flag, d3dh_hr_name(hr));
        else if (flag == DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT || flag == DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER ||
                 flag == DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO || flag == DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
            CHECKF(hr == E_INVALIDARG, "flag 0x%x -> %s", flag, d3dh_hr_name(hr));
        else {
            CHECKF(hr == S_OK, "flag 0x%x -> %s", flag, d3dh_hr_name(hr));
            DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
            IDXGISwapChain_GetDesc(sc, &g);
            CHECKF(g.Flags == flag, "flag 0x%x reported as 0x%x", flag, g.Flags);
        }
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_flag_cannot_be_dropped)
{
    OPEN2(s);
    /* waitable / tearing chains must keep their flag in every ResizeBuffers call */
    static const UINT fl[] = { DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING };
    for (unsigned i = 0; i < 2; i++) {
        IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, fl[i], DXGI_FORMAT_B8G8R8A8_UNORM);
        if (!sc) continue;
        HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 24, DXGI_FORMAT_B8G8R8A8_UNORM, fl[i]);
        CHECKF(hr == S_OK, "same flag 0x%x -> %s", fl[i], d3dh_hr_name(hr));
        hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 24, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        CHECKF(FAILED(hr), "dropping flag 0x%x succeeded", fl[i]);
        PINF(hr, "dropping flag 0x%x in ResizeBuffers", fl[i]);
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_flag_can_be_dropped)
{
    OPEN2(s);
    static const UINT fl[] = { DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE, DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT,
        DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER };
    for (unsigned i = 0; i < 4; i++) {
        IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_DISCARD, 2, fl[i], DXGI_FORMAT_B8G8R8A8_UNORM);
        if (!sc) continue;
        HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 24, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        CHECKF(hr == S_OK, "dropping flag 0x%x -> %s", fl[i], d3dh_hr_name(hr));
        DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
        IDXGISwapChain1_GetDesc1(sc, &g);
        CHECKF(g.Flags == 0, "flags after drop: 0x%x", g.Flags);
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_resizebuffers1_d3d11)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    IDXGISwapChain3 *s3 = NULL;
    if (FAILED(IDXGISwapChain1_QueryInterface(sc, &IID_IDXGISwapChain3, (void **)&s3))) { t_skip("no IDXGISwapChain3"); SC_REL(sc); sc_close(&s); return; }
    HRESULT hr = IDXGISwapChain3_ResizeBuffers1(s3, 2, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0, NULL, NULL);
    PIN_HR("D3D11 ResizeBuffers1(NULL masks, NULL queues)", hr, DXGI_ERROR_INVALID_CALL);
    UINT mask[2] = { 1, 1 };
    IUnknown *queues[2] = { s.unk, s.unk };
    hr = IDXGISwapChain3_ResizeBuffers1(s3, 2, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0, mask, queues);
    CHECKF(hr == DXGI_ERROR_INVALID_CALL, "D3D11 chain given present queues -> %s", d3dh_hr_name(hr));
    IDXGISwapChain3_Release(s3);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_many_times)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    for (UINT i = 1; i <= 30; i++) {
        UINT w = 8 + i * 5, h = 8 + i * 3;
        HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 2 + (i & 1), w, h, DXGI_FORMAT_UNKNOWN, 0);
        CHECKF(hr == S_OK, "iteration %u: %s", i, d3dh_hr_name(hr));
        CHECK(sc_okish(IDXGISwapChain1_Present(sc, 0, 0)));
        DXGI_SWAP_CHAIN_DESC1 g; IDXGISwapChain1_GetDesc1(sc, &g);
        CHECKF(g.Width == w && g.Height == h, "iteration %u: %ux%u", i, g.Width, g.Height);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_partial_zero_dimension)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    RECT r; GetClientRect(s.hwnd, &r);
    HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 50, 0, DXGI_FORMAT_UNKNOWN, 0);
    DXGI_SWAP_CHAIN_DESC1 g; IDXGISwapChain1_GetDesc1(sc, &g);
    /* a zero in either dimension means "use the client area" for both: the other (non-zero) value is ignored */
    EXPECT_HR(hr, S_OK); CHECKF(g.Width == (UINT)r.right && g.Height == (UINT)r.bottom, "ResizeBuffers(50, 0): buffer %ux%u client %ldx%ld", g.Width, g.Height, r.right, r.bottom);
    hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 0, 40, DXGI_FORMAT_UNKNOWN, 0);
    IDXGISwapChain1_GetDesc1(sc, &g);
    EXPECT_HR(hr, S_OK); CHECKF(g.Width == (UINT)r.right && g.Height == (UINT)r.bottom, "ResizeBuffers(0, 40): buffer %ux%u client %ldx%ld", g.Width, g.Height, r.right, r.bottom);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resize_srgb_bitblt)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = mkr(&s, DXGI_SWAP_EFFECT_SEQUENTIAL, 2, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!sc) { sc_close(&s); return; }
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0), S_OK);
    DXGI_SWAP_CHAIN_DESC1 g; IDXGISwapChain1_GetDesc1(sc, &g);
    EXPECT_EQ(g.Format, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_R16G16B16A16_FLOAT, 0), S_OK);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_R10G10B10A2_UNORM, 0), S_OK);
    SC_REL(sc);
    sc_close(&s);
}

/* ---- D3D12 ------------------------------------------------------------------ */
TEST(dxgi_swapchain_d12_resize)
{
    SC12 s; if (!sc12_open(&s)) return;
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc12_create(&s, s.hwnd, &d, &sc))) { CHECK(0); sc12_close(&s); return; }
    ID3D12Resource *r = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D12Resource, (void **)&r);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0), DXGI_ERROR_INVALID_CALL);
    SAFE_RELEASE(r);
    EXPECT_HR(IDXGISwapChain1_ResizeBuffers(sc, 3, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0), S_OK);
    DXGI_SWAP_CHAIN_DESC1 g; memset(&g, 0, sizeof g);
    IDXGISwapChain1_GetDesc1(sc, &g);
    EXPECT_EQ(g.Width, 40); EXPECT_EQ(g.Height, 30); EXPECT_EQ(g.BufferCount, 3);
    EXPECT_EQ(g.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_HR(IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D12Resource, (void **)&r), S_OK);
    if (r) {
        D3D12_RESOURCE_DESC rd = d3dh_d12_resource_desc(r);
        EXPECT_EQ(rd.Width, 40); EXPECT_EQ(rd.Height, 30); EXPECT_EQ(rd.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
        EXPECT_EQ(rd.Dimension, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
        CHECK(rd.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        SAFE_RELEASE(r);
    }
    d3dh_d12_flush(s.dev, s.q);
    SC_REL(sc);
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_resizebuffers1_masks)
{
    SC12 s; if (!sc12_open(&s)) return;
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc; IDXGISwapChain3 *s3 = NULL;
    if (!sc_okish(sc12_create(&s, s.hwnd, &d, &sc))) { CHECK(0); sc12_close(&s); return; }
    if (FAILED(IDXGISwapChain1_QueryInterface(sc, &IID_IDXGISwapChain3, (void **)&s3))) { t_skip("no SwapChain3"); SC_REL(sc); sc12_close(&s); return; }
    IDXGISwapChain1_Release(sc);
    UINT mask[2] = { 1, 1 };
    IUnknown *queues[2] = { (IUnknown *)s.q, (IUnknown *)s.q };
    EXPECT_HR(IDXGISwapChain3_ResizeBuffers1(s3, 2, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0, mask, queues), S_OK);
    EXPECT_HR(IDXGISwapChain3_ResizeBuffers1(s3, 2, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0, NULL, queues), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain3_ResizeBuffers1(s3, 2, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0, NULL, NULL), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain3_ResizeBuffers1(s3, 0, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0, NULL, NULL), DXGI_ERROR_INVALID_CALL);
    mask[0] = mask[1] = 2;
    EXPECT_HR(IDXGISwapChain3_ResizeBuffers1(s3, 2, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0, mask, queues), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain3_ResizeBuffers1(s3, 0, 40, 30, DXGI_FORMAT_B8G8R8A8_UNORM, 0, mask, queues), DXGI_ERROR_INVALID_CALL);
    d3dh_d12_flush(s.dev, s.q);
    SC_REL(s3);
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_resize_flags)
{
    SC12 s; if (!sc12_open(&s)) return;
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc12_create(&s, s.hwnd, &d, &sc))) { CHECK(0); sc12_close(&s); return; }
    HRESULT hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    EXPECT_HR(hr, S_OK);
    DXGI_SWAP_CHAIN_DESC1 g; IDXGISwapChain1_GetDesc1(sc, &g);
    EXPECT_EQ(g.Flags, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_NONPREROTATED);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    hr = IDXGISwapChain1_ResizeBuffers(sc, 2, 32, 32, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    EXPECT_HR(hr, S_OK);
    d3dh_d12_flush(s.dev, s.q);
    SC_REL(sc);
    sc12_close(&s);
}
