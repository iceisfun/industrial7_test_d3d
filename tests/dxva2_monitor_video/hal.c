/* The same services on a HAL device (here the Basic Render Driver): behaviour differs from the NULLREF device the rest of the area uses. */
#include "dxm.h"

typedef HRESULT (WINAPI *hd_create_fn)(IDirect3DDevice9Ex *, const DXVAHD_CONTENT_DESC *, DXVAHD_DEVICE_USAGE, PDXVAHDSW_Plugin, IDXVAHD_Device **);

typedef struct {
    IDirect3D9 *d3d; IDirect3DDevice9 *dev; HWND hw; IDirect3DDeviceManager9 *mgr; HANDLE h;
    IDirectXVideoDecoderService *dec; IDirectXVideoProcessorService *proc;
} hal_t;

static void hal_close(hal_t *x)
{
    if (x->dec) IDirectXVideoDecoderService_Release(x->dec);
    if (x->proc) IDirectXVideoProcessorService_Release(x->proc);
    if (x->mgr) { if (x->h) IDirect3DDeviceManager9_CloseDeviceHandle(x->mgr, x->h); IDirect3DDeviceManager9_Release(x->mgr); }
    if (x->dev) IDirect3DDevice9_Release(x->dev);
    if (x->d3d) IDirect3D9_Release(x->d3d);
    d3dh_destroy_window(x->hw);
    memset(x, 0, sizeof *x);
}

static int hal_open(hal_t *x)
{
    HRESULT (WINAPI *mk)(UINT *, IDirect3DDeviceManager9 **) = (void *)t_proc("DXVA2CreateDirect3DDeviceManager9");
    UINT tok = 0;
    memset(x, 0, sizeof *x);
    if (!mk || !d3dh_Direct3DCreate9()) { t_skip("no DXVA2 / D3D9"); return 0; }
    if (FAILED(dxm_make9(&x->d3d, &x->dev, &x->hw, D3DDEVTYPE_HAL)) || !x->dev) { t_skip("no D3D9 HAL device"); hal_close(x); return 0; }
    if (FAILED(mk(&tok, &x->mgr)) || FAILED(IDirect3DDeviceManager9_ResetDevice(x->mgr, x->dev, tok)) ||
        FAILED(IDirect3DDeviceManager9_OpenDeviceHandle(x->mgr, &x->h))) { t_skip("no device manager"); hal_close(x); return 0; }
    if (FAILED(IDirect3DDeviceManager9_GetVideoService(x->mgr, x->h, &DXM_IID_DecSvc, (void **)&x->dec))) x->dec = NULL;
    if (FAILED(IDirect3DDeviceManager9_GetVideoService(x->mgr, x->h, &DXM_IID_ProcSvc, (void **)&x->proc))) x->proc = NULL;
    return 1;
}
#define HAL(x) hal_t x; if (!hal_open(&x)) return

TEST(dxva2_monitor_video_hal_services_exist) {
    HAL(x);
    CHECK(x.dec != NULL);     /* both services can be obtained on the HAL device ... */
    CHECK(x.proc != NULL);
    hal_close(&x);
}
TEST(dxva2_monitor_video_hal_no_decoder_devices) {
    HAL(x);
    UINT n = 0xdead; GUID *g = (GUID *)(ULONG_PTR)1;
    if (!x.dec) { CHECK(0); hal_close(&x); return; }
    /* ... but the Basic Render Driver advertises no hardware decoder: the call fails with E_FAIL and reports nothing */
    EXPECT_HR(IDirectXVideoDecoderService_GetDecoderDeviceGuids(x.dec, &n, &g), E_FAIL);
    EXPECT_EQ(n, 0);
    hal_close(&x);
}
TEST(dxva2_monitor_video_hal_processor_guids) {
    HAL(x);
    DXVA2_VideoDesc vd = dxm_desc(64, 64, D3DFMT_X8R8G8B8);
    UINT n = 0; GUID *g = NULL;
    if (!x.proc) { CHECK(0); hal_close(&x); return; }
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids(x.proc, &vd, &n, &g), S_OK);
    EXPECT_EQ(n, 1);                                  /* exactly the software processor */
    if (n == 1 && g) CHECK(IsEqualGUID(&g[0], &DXM_SwProc));
    if (g) CoTaskMemFree(g);
    hal_close(&x);
}
TEST(dxva2_monitor_video_hal_unknown_processor_guid) {
    HAL(x);
    DXVA2_VideoDesc vd = dxm_desc(64, 64, DXM_NV12);
    DXVA2_VideoProcessorCaps caps; DXVA2_ValueRange vr;
    UINT n = 0; D3DFORMAT *f = NULL;
    if (!x.proc) { CHECK(0); hal_close(&x); return; }
    /* A GUID that names no processor: the HAL path answers the caps and proc-amp queries (the NULLREF device rejects them with
     * D3DERR_INVALIDCALL) but fails the format enumerations with E_FAIL. */
    memset(&caps, 0, sizeof caps);
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorCaps(x.proc, &DXM_IID_Bogus, &vd, D3DFMT_X8R8G8B8, &caps), S_OK);
    EXPECT_EQ(caps.DeviceCaps, 1);
    EXPECT_EQ(caps.NumForwardRefSamples, 0);
    EXPECT_HR(IDirectXVideoProcessorService_GetProcAmpRange(x.proc, &DXM_IID_Bogus, &vd, D3DFMT_X8R8G8B8, DXVA2_ProcAmp_Brightness, &vr), S_OK);
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorRenderTargets(x.proc, &DXM_IID_Bogus, &vd, &n, &f), E_FAIL);
    EXPECT_EQ(n, 0);
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorSubStreamFormats(x.proc, &DXM_IID_Bogus, &vd, D3DFMT_X8R8G8B8, &n, &f), E_FAIL);
    hal_close(&x);
}
TEST(dxva2_monitor_video_hal_software_processor_x8r8g8b8) {
    HAL(x);
    DXVA2_VideoDesc vd = dxm_desc(64, 64, DXM_NV12);
    DXVA2_VideoProcessorCaps caps; DXVA2_ValueRange vr;
    UINT n = 0; D3DFORMAT *f = NULL;
    if (!x.proc) { CHECK(0); hal_close(&x); return; }
    /* the software processor GUID with an NV12 stream and an X8R8G8B8 target is refused on the HAL device */
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorCaps(x.proc, &DXM_SwProc, &vd, D3DFMT_X8R8G8B8, &caps), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirectXVideoProcessorService_GetProcAmpRange(x.proc, &DXM_SwProc, &vd, D3DFMT_X8R8G8B8, DXVA2_ProcAmp_Brightness, &vr), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorRenderTargets(x.proc, &DXM_SwProc, &vd, &n, &f), E_FAIL);
    hal_close(&x);
}
TEST(dxva2_monitor_video_hal_dxvahd_unavailable) {
    hd_create_fn c = (hd_create_fn)t_proc("DXVAHD_CreateDevice");
    IDirect3D9Ex *d3d = NULL; IDirect3DDevice9Ex *dev = NULL; HWND hw = NULL; D3DPRESENT_PARAMETERS pp;
    __typeof__(&Direct3DCreate9Ex) c9 = d3dh_Direct3DCreate9Ex();
    DXVAHD_CONTENT_DESC desc;
    int u;
    REQUIRE(c);
    if (!c9) { t_skip("no Direct3DCreate9Ex"); return; }
    hw = d3dh_create_hidden_window();
    if (FAILED(c9(D3D_SDK_VERSION, &d3d))) { t_skip("no D3D9Ex"); d3dh_destroy_window(hw); return; }
    dxm_pp(&pp, hw);
    if (FAILED(IDirect3D9Ex_CreateDeviceEx(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, NULL, &dev))) {
        t_skip("no HAL D3D9Ex device"); IDirect3D9Ex_Release(d3d); d3dh_destroy_window(hw); return;
    }
    memset(&desc, 0, sizeof desc);
    desc.InputFrameFormat = DXVAHD_FRAME_FORMAT_PROGRESSIVE; desc.InputFrameRate.Numerator = 30; desc.InputFrameRate.Denominator = 1;
    desc.InputWidth = 64; desc.InputHeight = 64; desc.OutputFrameRate = desc.InputFrameRate; desc.OutputWidth = 64; desc.OutputHeight = 64;
    for (u = 0; u < 3; u++) {   /* the Basic Render Driver exposes no DXVA-HD capable interface: every usage -> E_NOINTERFACE */
        IDXVAHD_Device *dv = (IDXVAHD_Device *)(ULONG_PTR)1;
        EXPECT_HR(c(dev, &desc, (DXVAHD_DEVICE_USAGE)u, NULL, &dv), E_NOINTERFACE);
        EXPECT_EQ(dv, 1);   /* the out pointer is not written on failure */
    }
    IDirect3DDevice9Ex_Release(dev); IDirect3D9Ex_Release(d3d); d3dh_destroy_window(hw);
}
