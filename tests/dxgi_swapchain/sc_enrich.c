/* Present rules on D3D12 chains, GetBuffer identity across Present, fullscreen transitions of a hidden window, buffer reference rules.
 * Expected values are pinned in sc_pins.inc (measured on Win10 22H2, WARP). */
#include "sc.h"

static const DXGI_SWAP_EFFECT k_flip[] = { DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD };
static const UINT k_pflags[] = { 0, 1, 2, 3, 4, 5, 8, 9, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800 };

TEST(dxgi_swapchain_d12_present_flags_matrix)
{
    SC12 s; if (!sc12_open(&s)) return;
    for (unsigned e = 0; e < 2; e++) {
        HWND w = sc_window(64, 48);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, 3, k_flip[e]);
        IDXGISwapChain1 *sc;
        if (!sc_okish(sc12_create(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); continue; }
        /* the very first Present refuses DO_NOT_SEQUENCE */
        PINF(IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_DO_NOT_SEQUENCE), "d12 eff %d first Present DO_NOT_SEQUENCE", k_flip[e]);
        PINF(IDXGISwapChain1_Present(sc, 0, 0), "d12 eff %d first plain Present", k_flip[e]);
        for (unsigned i = 0; i < ARRAY_SIZE_(k_pflags); i++) {
            HRESULT hr = IDXGISwapChain1_Present(sc, 0, k_pflags[i]);
            if (k_pflags[i] == DXGI_PRESENT_DO_NOT_WAIT && hr == DXGI_ERROR_WAS_STILL_DRAWING) continue;   /* queue full: legitimate */
            PINF(hr, "d12 eff %d Present flags 0x%x", k_flip[e], k_pflags[i]);
        }
        for (UINT sync = 0; sync <= 5; sync++) PINF(IDXGISwapChain1_Present(sc, sync, 0), "d12 eff %d Present sync %u", k_flip[e], sync);
        PINF(IDXGISwapChain1_Present(sc, 0xFFFFFFFF, 0), "d12 eff %d Present sync 0xffffffff", k_flip[e]);
        d3dh_d12_flush(s.dev, s.q);
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_present1_dirty_rects)
{
    SC12 s; if (!sc12_open(&s)) return;
    for (unsigned e = 0; e < 2; e++) {
        HWND w = sc_window(64, 48);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, 3, k_flip[e]);
        IDXGISwapChain1 *sc; RECT r = { 0, 0, 16, 16 }, big = { 0, 0, 5000, 5000 }, scroll = { 0, 0, 32, 32 }; POINT off = { 8, 8 };
        DXGI_PRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
        if (!sc_okish(sc12_create(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); continue; }
        pp.DirtyRectsCount = 1; pp.pDirtyRects = &r;
        PINF(IDXGISwapChain1_Present1(sc, 0, 0, &pp), "d12 eff %d dirty rect on the first Present", k_flip[e]);
        EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
        PINF(IDXGISwapChain1_Present1(sc, 0, 0, &pp), "d12 eff %d dirty rect", k_flip[e]);
        pp.pDirtyRects = &big;
        PINF(IDXGISwapChain1_Present1(sc, 0, 0, &pp), "d12 eff %d oversize dirty rect", k_flip[e]);
        pp.pDirtyRects = &r; PINF(IDXGISwapChain1_Present1(sc, 0, DXGI_PRESENT_TEST, &pp), "d12 eff %d TEST with dirty rect", k_flip[e]);
        memset(&pp, 0, sizeof pp); pp.pScrollRect = &scroll;
        PINF(IDXGISwapChain1_Present1(sc, 0, 0, &pp), "d12 eff %d scroll rect without offset", k_flip[e]);
        pp.pScrollOffset = &off;
        PINF(IDXGISwapChain1_Present1(sc, 0, 0, &pp), "d12 eff %d scroll rect with offset", k_flip[e]);
        memset(&pp, 0, sizeof pp);
        PINF(IDXGISwapChain1_Present1(sc, 0, 0, &pp), "d12 eff %d empty parameters", k_flip[e]);
        d3dh_d12_flush(s.dev, s.q);
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc12_close(&s);
}

/* D3D11 flip chains hand out the same buffer object for index 0 across Present; bit-blt chains too. */
TEST(dxgi_swapchain_getbuffer_identity_across_present)
{
    SC s; static const DXGI_SWAP_EFFECT effs[] = { DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD };
    if (!sc_open2(&s, 0)) return;
    for (unsigned e = 0; e < 4; e++) for (UINT cnt = 2; cnt <= 3; cnt++) {
        HWND w = sc_window(32, 32); DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(32, 32, DXGI_FORMAT_R8G8B8A8_UNORM, cnt, effs[e]); IDXGISwapChain1 *sc;
        void *b0 = NULL, *b1 = NULL, *b2 = NULL; ULONG rc;
        if (!sc_okish(sc_create1(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); continue; }
        IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, &b0);
        IDXGISwapChain1_Present(sc, 0, 0);
        IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, &b1);
        IDXGISwapChain1_Present(sc, 0, 0);
        IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, &b2);
        PINF(b0 == b1 ? S_OK : S_FALSE, "eff %d count %u buffer 0 object kept after one Present", effs[e], cnt);
        PINF(b0 == b2 ? S_OK : S_FALSE, "eff %d count %u buffer 0 object kept after two Presents", effs[e], cnt);
        /* GetBuffer adds one reference per call on the same object (three calls, the same pointer) */
        rc = ((IUnknown *)b2)->lpVtbl->AddRef((IUnknown *)b2); ((IUnknown *)b2)->lpVtbl->Release((IUnknown *)b2);
        CHECKF(rc - 1 == 3, "eff %d count %u: buffer 0 holds %lu refs with three GetBuffer references outstanding", effs[e], cnt, rc - 1);
        ((IUnknown *)b0)->lpVtbl->Release((IUnknown *)b0); ((IUnknown *)b1)->lpVtbl->Release((IUnknown *)b1); ((IUnknown *)b2)->lpVtbl->Release((IUnknown *)b2);
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}

/* Fullscreen transition of a chain on a hidden window: no output can be taken exclusively on the VM. */
TEST(dxgi_swapchain_setfullscreenstate_true_hidden_window)
{
    SC s; static const DXGI_SWAP_EFFECT effs[] = { DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_FLIP_DISCARD };
    if (!sc_open2(&s, 0)) return;
    for (unsigned e = 0; e < 2; e++) {
        HWND w = sc_window(64, 48); DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, 2, effs[e]); IDXGISwapChain1 *sc;
        BOOL fs = 7; IDXGIOutput *o = (IDXGIOutput *)(ULONG_PTR)1;
        d.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        if (!sc_okish(sc_create1(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); continue; }
        PINF(IDXGISwapChain1_GetFullscreenState(sc, &fs, &o), "eff %d GetFullscreenState windowed", effs[e]);
        CHECKF(fs == FALSE && o == NULL, "eff %d windowed: fullscreen=%d output=%p", effs[e], fs, (void *)o);
        { /* 45 of 46 observations: UNSUPPORTED; the transient "not currently available" was seen once and is tolerated */
          HRESULT hr = IDXGISwapChain1_SetFullscreenState(sc, TRUE, NULL);
          if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) t_note("eff %d SetFullscreenState(TRUE) returned the transient NOT_CURRENTLY_AVAILABLE", effs[e]);
          else PINF(hr, "eff %d SetFullscreenState(TRUE) on hidden window", effs[e]); }
        fs = 7; o = (IDXGIOutput *)(ULONG_PTR)1;
        PINF(IDXGISwapChain1_GetFullscreenState(sc, &fs, &o), "eff %d GetFullscreenState after failed switch", effs[e]);
        CHECKF(fs == FALSE && o == NULL, "eff %d after failed switch: fullscreen=%d output=%p", effs[e], fs, (void *)o);
        SAFE_RELEASE(o);
        PINF(IDXGISwapChain1_SetFullscreenState(sc, FALSE, NULL), "eff %d SetFullscreenState(FALSE) after", effs[e]);
        PINF(IDXGISwapChain1_GetFullscreenState(sc, NULL, NULL), "eff %d GetFullscreenState(NULL, NULL)", effs[e]);
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}
