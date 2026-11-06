/* IDXGIFactory2 stereo-status registration and IDXGIFactoryMedia (measured on Windows 10 22H2, Basic Render Driver). */
#include "fh.h"

#define WM_STEREO (WM_USER + 11)

TEST(dxgi_factory_stereo_window_register_unregister) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HWND w = d3dh_create_hidden_window();
    if (!w) { t_skip("no window"); IDXGIFactory2_Release(f2); return; }
    DWORD c1 = 0xdeadbeef, c2 = 0xdeadbeef;
    EXPECT_HR(IDXGIFactory2_RegisterStereoStatusWindow(f2, w, WM_STEREO, &c1), S_OK);
    EXPECT_HR(IDXGIFactory2_RegisterStereoStatusWindow(f2, w, WM_STEREO + 1, &c2), S_OK);
    EXPECT_EQ(c1, 1); CHECK(c2 != c1 && c2 != 0xdeadbeef);
    IDXGIFactory2_UnregisterStereoStatus(f2, c1);
    IDXGIFactory2_UnregisterStereoStatus(f2, c2);
    IDXGIFactory2_UnregisterStereoStatus(f2, c2);      /* unregistering twice / a bogus cookie is silent (void) */
    IDXGIFactory2_UnregisterStereoStatus(f2, 0x7e57);
    IDXGIFactory2_Release(f2);
    d3dh_destroy_window(w);
}
TEST(dxgi_factory_stereo_window_bad_args) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HWND w = d3dh_create_hidden_window();
    DWORD c = 0xdeadbeef;
    EXPECT_HR(IDXGIFactory2_RegisterStereoStatusWindow(f2, NULL, WM_STEREO, &c), DXGI_E_INVALID_CALL);
    EXPECT_EQ(c, 0xdeadbeef);
    EXPECT_HR(IDXGIFactory2_RegisterStereoStatusWindow(f2, w, WM_STEREO, NULL), DXGI_E_INVALID_CALL);
    IDXGIFactory2_Release(f2);
    if (w) d3dh_destroy_window(w);
}
TEST(dxgi_factory_stereo_event_register_signals_immediately) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD c = 0xdeadbeef;
    EXPECT_HR(IDXGIFactory2_RegisterStereoStatusEvent(f2, ev, &c), S_OK);
    CHECK(c != 0xdeadbeef);
    EXPECT_EQ(WaitForSingleObject(ev, 50), WAIT_OBJECT_0); /* Win10 delivers the current (non-stereo) status on registration */
    EXPECT_EQ(WaitForSingleObject(ev, 20), WAIT_TIMEOUT);  /* and does not repeat it */
    IDXGIFactory2_UnregisterStereoStatus(f2, c);
    EXPECT_HR(IDXGIFactory2_RegisterStereoStatusEvent(f2, NULL, &c), DXGI_E_INVALID_CALL);
    EXPECT_HR(IDXGIFactory2_RegisterStereoStatusEvent(f2, ev, NULL), DXGI_E_INVALID_CALL);
    CloseHandle(ev);
    IDXGIFactory2_Release(f2);
}
TEST(dxgi_factory_stereo_and_occlusion_cookie_sequences_independent) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD a = 0, b = 0;
    EXPECT_HR(IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, &a), S_OK);
    EXPECT_HR(IDXGIFactory2_RegisterStereoStatusEvent(f2, ev, &b), S_OK);
    EXPECT_EQ(a, 1); EXPECT_EQ(b, 1);                    /* each kind has its own sequence per factory */
    IDXGIFactory2_UnregisterOcclusionStatus(f2, a);
    IDXGIFactory2_UnregisterStereoStatus(f2, b);
    CloseHandle(ev);
    IDXGIFactory2_Release(f2);
}

/* ---- IDXGIFactoryMedia ---- */
typedef struct { IDXGIFactoryMedia *fm; IUnknown *dev; HANDLE h; int with_desc; } dec_ctx;
static void dec_create(void *p) {
    dec_ctx *c = p; void *out = NULL; DXGI_DECODE_SWAP_CHAIN_DESC dd; memset(&dd, 0, sizeof dd);
    IDXGIFactoryMedia_CreateDecodeSwapChainForCompositionSurfaceHandle(c->fm, c->dev, c->h, c->with_desc ? &dd : NULL, NULL, NULL, (IDXGIDecodeSwapChain **)&out);
}
TEST(dxgi_factory_media_composition_surface_handle) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    IDXGIFactoryMedia *fm = fh_qi((IUnknown *)f2, &IID_IDXGIFactoryMedia);
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL;
    if (!fm || FAILED(d3dh_make_device11(&dev, &ctx, NULL, NULL, 0))) { t_skip("no IDXGIFactoryMedia or device"); if (fm) IDXGIFactoryMedia_Release(fm); IDXGIFactory2_Release(f2); return; }
    DXGI_SWAP_CHAIN_DESC1 d; memset(&d, 0, sizeof d);
    d.Width = 64; d.Height = 64; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1; d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount = 2; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; d.Scaling = DXGI_SCALING_STRETCH;
    IDXGISwapChain1 *sc = (void *)1;
    IUnknown *du = (IUnknown *)dev;
    EXPECT_HR(IDXGIFactoryMedia_CreateSwapChainForCompositionSurfaceHandle(fm, NULL, (HANDLE)1, &d, NULL, &sc), DXGI_E_INVALID_CALL);
    CHECK(sc == (void *)1); /* argument errors leave the out pointer untouched */
    EXPECT_HR(IDXGIFactoryMedia_CreateSwapChainForCompositionSurfaceHandle(fm, du, (HANDLE)1, NULL, NULL, &sc), DXGI_E_INVALID_CALL);
    EXPECT_HR(IDXGIFactoryMedia_CreateSwapChainForCompositionSurfaceHandle(fm, du, (HANDLE)0x1234, &d, NULL, NULL), DXGI_E_INVALID_CALL);
    /* a made-up surface handle fails (E_FAIL), a NULL handle is accepted */
    sc = NULL;
    EXPECT_HR(IDXGIFactoryMedia_CreateSwapChainForCompositionSurfaceHandle(fm, du, (HANDLE)0x1234, &d, NULL, &sc), E_FAIL);
    CHECK(sc == NULL);
    EXPECT_HR(IDXGIFactoryMedia_CreateSwapChainForCompositionSurfaceHandle(fm, du, NULL, &d, NULL, &sc), S_OK);
    if (sc) { DXGI_SWAP_CHAIN_DESC1 g; EXPECT_HR(IDXGISwapChain1_GetDesc1(sc, &g), S_OK); EXPECT_EQ(g.Width, 64); IDXGISwapChain1_Release(sc); }
    /* decode swap chain: NULL arguments are diagnosed; a zeroed description faults inside dxgi */
    void *dsc = (void *)1;
    EXPECT_HR(IDXGIFactoryMedia_CreateDecodeSwapChainForCompositionSurfaceHandle(fm, du, NULL, NULL, NULL, NULL, (IDXGIDecodeSwapChain **)&dsc), DXGI_E_INVALID_CALL);
    dec_ctx dc = { fm, du, (HANDLE)0x1234, 1 };
    EXPECT_EQ(t_try(dec_create, &dc), 0xc0000005);
    IDXGIFactoryMedia_Release(fm);
    ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
    IDXGIFactory2_Release(f2);
}
