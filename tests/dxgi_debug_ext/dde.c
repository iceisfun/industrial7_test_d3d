/* dxgi_debug_ext: DXGI interfaces that the other DXGI areas do not call: IDXGIOutputDuplication, IDXGIDebug1, IDXGIInfoQueue,
 * IDXGIDisplayControl, IDXGISwapChainMedia, IDXGIAdapter3 content-protection events, IDXGIOutput::SetDisplaySurface.
 * All values measured on Windows 10 22H2 with the Basic Render Driver and a virtual display. */
#include "d3dh.h"
#include <dxgi1_5.h>
#include <dxgidebug.h>

typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; IDXGIFactory1 *fac; IDXGIAdapter1 *ad; IDXGIOutput *out; D3D_DRIVER_TYPE drv; } DD;
/* Picks the first adapter that owns an output (the hardware-type Basic Render Driver on the reference VM); falls back to WARP. */
static int dd_open(DD *d, UINT flags) {
    memset(d, 0, sizeof *d);
    if (!d3dh_D3D11CreateDevice() || !d3dh_CreateDXGIFactory1()) { t_skip("no d3d11/dxgi"); return 0; }
    if (FAILED(d3dh_make_factory1(&d->fac))) { t_skip("no factory"); return 0; }
    for (UINT i = 0; i < 8 && !d->ad; i++) {
        IDXGIAdapter1 *a = NULL; if (FAILED(IDXGIFactory1_EnumAdapters1(d->fac, i, &a))) break;
        IDXGIOutput *o = NULL;
        if (SUCCEEDED(IDXGIAdapter1_EnumOutputs(a, 0, &o))) { d->ad = a; d->out = o; } else IDXGIAdapter1_Release(a);
    }
    __typeof__(&D3D11CreateDevice) cd = d3dh_D3D11CreateDevice();
    D3D_FEATURE_LEVEL fl;
    if (d->ad) {
        HRESULT hr = cd((IDXGIAdapter *)d->ad, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &d->dev, &fl, &d->ctx);
        d->drv = D3D_DRIVER_TYPE_UNKNOWN; if (FAILED(hr)) { SAFE_RELEASE(d->out); SAFE_RELEASE(d->ad); }
    }
    if (!d->dev) {
        if (FAILED(d3dh_make_device11(&d->dev, &d->ctx, NULL, &d->drv, flags)) || d->drv == D3D_DRIVER_TYPE_NULL) { t_skip("no usable D3D11 device"); return 0; }
        IDXGIDevice *dd = NULL; IDXGIAdapter *ad = NULL;
        if (SUCCEEDED(ID3D11Device_QueryInterface(d->dev, &IID_IDXGIDevice, (void **)&dd))) { IDXGIDevice_GetAdapter(dd, &ad); IDXGIDevice_Release(dd); }
        if (ad) { IDXGIAdapter_QueryInterface(ad, &IID_IDXGIAdapter1, (void **)&d->ad); IDXGIAdapter_Release(ad); }
    }
    return 1;
}
/* Releasing the device straight after a duplication object was released intermittently raised an access violation inside DXGI
 * (measured on Win10 22H2); a short pause before teardown keeps the area deterministic. */
static void dd_close(DD *d) { Sleep(250); SAFE_RELEASE(d->out); SAFE_RELEASE(d->ad); SAFE_RELEASE(d->fac); SAFE_RELEASE(d->ctx); SAFE_RELEASE(d->dev); }


#define ON_BRD(d) ((d)->ad && dd_vendor(d) == 0x1414)
static UINT dd_vendor(DD *d) { DXGI_ADAPTER_DESC1 ad; memset(&ad, 0, sizeof ad); if (d->ad) IDXGIAdapter1_GetDesc1(d->ad, &ad); return ad.VendorId; }
/* Pin a value measured on the Microsoft Basic Render Driver (vendor 0x1414); elsewhere it is only logged. */
#define BRD_EQ(d, a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); if (ON_BRD(d)) EXPECT_EQ(a_, b_); else t_note(#a " = 0x%llx", (unsigned long long)a_); } while (0)

TEST(dde_output_duplication_frame_contract) {
    DD d; if (!dd_open(&d, 0)) return;
    if (!d.out) { t_skip("no adapter with an output"); dd_close(&d); return; }
    IDXGIOutput1 *o1 = NULL; EXPECT_HR(IDXGIOutput_QueryInterface(d.out, &IID_IDXGIOutput1, (void **)&o1), S_OK);
    IDXGIOutputDuplication *od = NULL;
    if (o1) EXPECT_HR(IDXGIOutput1_DuplicateOutput(o1, (IUnknown *)d.dev, &od), S_OK);
    DXGI_OUTPUT_DESC odsc; memset(&odsc, 0, sizeof odsc); IDXGIOutput_GetDesc(d.out, &odsc);
    UINT dw = odsc.DesktopCoordinates.right - odsc.DesktopCoordinates.left, dh = odsc.DesktopCoordinates.bottom - odsc.DesktopCoordinates.top;
    if (od) {
        DXGI_OUTDUPL_DESC dd; memset(&dd, 0xCD, sizeof dd); IDXGIOutputDuplication_GetDesc(od, &dd);
        EXPECT_EQ(dd.ModeDesc.Width, dw); EXPECT_EQ(dd.ModeDesc.Height, dh);
        EXPECT_EQ(dd.ModeDesc.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
        EXPECT_EQ(dd.Rotation, DXGI_MODE_ROTATION_IDENTITY);
        BRD_EQ(&d, dd.DesktopImageInSystemMemory, 1);
        DXGI_MAPPED_RECT mr; memset(&mr, 0, sizeof mr);
        EXPECT_HR(IDXGIOutputDuplication_MapDesktopSurface(od, &mr), DXGI_ERROR_INVALID_CALL);      /* no frame acquired */
        EXPECT_HR(IDXGIOutputDuplication_ReleaseFrame(od), DXGI_ERROR_INVALID_CALL);                /* nothing to release */
        DXGI_OUTDUPL_FRAME_INFO fi; memset(&fi, 0, sizeof fi); IDXGIResource *res = NULL;
        HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(od, 2000, &fi, &res);
        EXPECT_HR(hr, S_OK);   /* the first frame of a new duplication is always available */
        if (SUCCEEDED(hr)) {
            CHECK(fi.AccumulatedFrames >= 1); CHECK(res != NULL);
            EXPECT_EQ(fi.PointerPosition.Visible, 0); EXPECT_EQ(fi.ProtectedContentMaskedOut, 0);
            ID3D11Texture2D *t = NULL; EXPECT_HR(res ? IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&t) : E_POINTER, S_OK);
            if (t) { D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
                EXPECT_EQ(td.Width, dw); EXPECT_EQ(td.Height, dh); EXPECT_EQ(td.Format, DXGI_FORMAT_B8G8R8A8_UNORM); EXPECT_EQ(td.Usage, D3D11_USAGE_DEFAULT); EXPECT_EQ(td.MipLevels, 1); EXPECT_EQ(td.ArraySize, 1);
                BRD_EQ(&d, td.BindFlags, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET); BRD_EQ(&d, td.CPUAccessFlags, 0);
                ID3D11Texture2D_Release(t); }
            UINT need = 0; RECT rc[4]; memset(rc, 0, sizeof rc);
            /* the metadata of the first frame: one dirty rectangle covering the desktop, no moves, no pointer shape */
            EXPECT_HR(IDXGIOutputDuplication_GetFrameDirtyRects(od, 0, NULL, &need), E_INVALIDARG);
            EXPECT_HR(IDXGIOutputDuplication_GetFrameDirtyRects(od, 4, rc, &need), DXGI_ERROR_MORE_DATA); EXPECT_EQ(need, sizeof(RECT));
            EXPECT_HR(IDXGIOutputDuplication_GetFrameDirtyRects(od, sizeof rc, rc, &need), S_OK);
            EXPECT_EQ(need, sizeof(RECT));
            if (need == sizeof(RECT)) { EXPECT_EQ(rc[0].left, odsc.DesktopCoordinates.left); EXPECT_EQ(rc[0].top, odsc.DesktopCoordinates.top); EXPECT_EQ(rc[0].right, odsc.DesktopCoordinates.right); EXPECT_EQ(rc[0].bottom, odsc.DesktopCoordinates.bottom); }
            DXGI_OUTDUPL_MOVE_RECT mv[4]; need = 0xCD; EXPECT_HR(IDXGIOutputDuplication_GetFrameMoveRects(od, sizeof mv, mv, &need), S_OK); EXPECT_EQ(need, 0);
            char pb[1024]; DXGI_OUTDUPL_POINTER_SHAPE_INFO psi; need = 0xCD; EXPECT_HR(IDXGIOutputDuplication_GetFramePointerShape(od, sizeof pb, pb, &need, &psi), S_OK); EXPECT_EQ(need, 0);
            EXPECT_EQ(fi.TotalMetadataBufferSize, sizeof(RECT));
            /* the desktop image is mappable while a frame is held (system-memory duplication on the Basic Render Driver) */
            hr = IDXGIOutputDuplication_MapDesktopSurface(od, &mr);
            if (dd.DesktopImageInSystemMemory) {
                EXPECT_HR(hr, S_OK);
                if (SUCCEEDED(hr)) { CHECK(mr.pBits != NULL); CHECK((UINT)mr.Pitch >= dw * 4); EXPECT_HR(IDXGIOutputDuplication_UnMapDesktopSurface(od), S_OK); EXPECT_HR(IDXGIOutputDuplication_UnMapDesktopSurface(od), DXGI_ERROR_INVALID_CALL); }
            } else t_note("MapDesktopSurface with a video-memory duplication = %s", d3dh_hr_name(hr));
            if (res) IDXGIResource_Release(res);
            EXPECT_HR(IDXGIOutputDuplication_ReleaseFrame(od), S_OK);
            EXPECT_HR(IDXGIOutputDuplication_ReleaseFrame(od), DXGI_ERROR_INVALID_CALL);
            EXPECT_HR(IDXGIOutputDuplication_MapDesktopSurface(od, &mr), DXGI_ERROR_INVALID_CALL);
        }
        /* a NULL resource out-pointer is rejected; with an unchanged desktop a zero timeout times out (the desktop may change: both outcomes are legal, others are not) */
        hr = IDXGIOutputDuplication_AcquireNextFrame(od, 0, &fi, NULL); EXPECT_HR(hr, E_INVALIDARG);
        int ok = 0, to = 0, other = 0;
        for (int i = 0; i < 10; i++) { IDXGIResource *r2 = NULL; hr = IDXGIOutputDuplication_AcquireNextFrame(od, 0, &fi, &r2);
            if (hr == S_OK) { ok++; if (r2) IDXGIResource_Release(r2); IDXGIOutputDuplication_ReleaseFrame(od); } else if (hr == DXGI_ERROR_WAIT_TIMEOUT) to++; else other++; }
        EXPECT_EQ(other, 0); t_note("zero-timeout acquisitions: %d frames, %d timeouts", ok, to);
        IDXGIOutputDuplication_Release(od);
    }
    SAFE_RELEASE(o1); dd_close(&d);
}

TEST(dde_output_duplication_creation_rules) {
    DD d; if (!dd_open(&d, 0)) return;
    if (!d.out) { t_skip("no adapter with an output"); dd_close(&d); return; }
    IDXGIOutput1 *o1 = NULL; EXPECT_HR(IDXGIOutput_QueryInterface(d.out, &IID_IDXGIOutput1, (void **)&o1), S_OK);
    if (o1) {
        IDXGIOutputDuplication *a = NULL, *b = NULL;
        EXPECT_HR(IDXGIOutput1_DuplicateOutput(o1, (IUnknown *)d.dev, &a), S_OK);
        EXPECT_HR(IDXGIOutput1_DuplicateOutput(o1, (IUnknown *)d.dev, &b), E_INVALIDARG);          /* one live duplication per output and process */
        EXPECT_EQ(b == NULL, 1);
        SAFE_RELEASE(a);
        EXPECT_HR(IDXGIOutput1_DuplicateOutput(o1, (IUnknown *)d.dev, &a), S_OK);                  /* released: it can be created again */
        SAFE_RELEASE(a);
        a = (IDXGIOutputDuplication *)(UINT_PTR)0x1234;
        EXPECT_HR(IDXGIOutput1_DuplicateOutput(o1, NULL, &a), E_INVALIDARG);
        EXPECT_EQ((UINT_PTR)a, 0x1234);                                                           /* the output pointer is left alone on failure */
        EXPECT_HR(IDXGIOutput1_DuplicateOutput(o1, (IUnknown *)d.dev, NULL), E_INVALIDARG);
        IDXGIOutput1_Release(o1);
    }
    dd_close(&d);
}

TEST(dde_set_display_surface) {
    DD d; if (!dd_open(&d, 0)) return;
    if (!d.out) { t_skip("no adapter with an output"); dd_close(&d); return; }
    for (int i = 0; i < 20; i++) { HRESULT hr = IDXGIOutput_SetDisplaySurface(d.out, NULL); if (hr != S_OK) { CHECKF(0, "SetDisplaySurface(NULL) #%d = %s", i, d3dh_hr_name(hr)); break; } }
    DXGI_OUTPUT_DESC od; memset(&od, 0, sizeof od); IDXGIOutput_GetDesc(d.out, &od);
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td); td.Width = od.DesktopCoordinates.right - od.DesktopCoordinates.left; td.Height = od.DesktopCoordinates.bottom - od.DesktopCoordinates.top;
    td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *t = NULL; EXPECT_HR(ID3D11Device_CreateTexture2D(d.dev, &td, NULL, &t), S_OK);
    if (t) { IDXGISurface *sf = NULL; EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&sf), S_OK);
        if (sf) { for (int i = 0; i < 20; i++) { HRESULT hr = IDXGIOutput_SetDisplaySurface(d.out, sf); if (hr != S_OK) { CHECKF(0, "SetDisplaySurface(surface) #%d = %s", i, d3dh_hr_name(hr)); break; } }
            IDXGIOutput_SetDisplaySurface(d.out, NULL); IDXGISurface_Release(sf); }
        ID3D11Texture2D_Release(t); }
    dd_close(&d);
}

TEST(dde_debug_interfaces_defaults) {
    HMODULE dl = LoadLibraryA("dxgidebug.dll");
    __typeof__(&DXGIGetDebugInterface1) g = (__typeof__(&DXGIGetDebugInterface1))d3dh_getproc("dxgi.dll", "DXGIGetDebugInterface1");
    if (!g || !dl) { t_skip("dxgidebug.dll (Graphics Tools) is not installed"); return; }
    IDXGIDebug1 *dbg = NULL; EXPECT_HR(g(0, &IID_IDXGIDebug1, (void **)&dbg), S_OK);
    if (dbg) {
        EXPECT_EQ(IDXGIDebug1_IsLeakTrackingEnabledForThread(dbg), TRUE);   /* tracking is on by default */
        IDXGIDebug1_DisableLeakTrackingForThread(dbg); EXPECT_EQ(IDXGIDebug1_IsLeakTrackingEnabledForThread(dbg), FALSE);
        IDXGIDebug1_EnableLeakTrackingForThread(dbg); EXPECT_EQ(IDXGIDebug1_IsLeakTrackingEnabledForThread(dbg), TRUE);
        EXPECT_HR(IDXGIDebug1_ReportLiveObjects(dbg, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY), S_OK);
        EXPECT_HR(IDXGIDebug1_ReportLiveObjects(dbg, DXGI_DEBUG_DXGI, DXGI_DEBUG_RLO_ALL), S_OK);
        IDXGIDebug1_Release(dbg);
    }
    IDXGIInfoQueue *iq = NULL; EXPECT_HR(g(0, &IID_IDXGIInfoQueue, (void **)&iq), S_OK);
    if (iq) {
        EXPECT_EQ(IDXGIInfoQueue_GetNumStoredMessages(iq, DXGI_DEBUG_ALL), 0); EXPECT_EQ(IDXGIInfoQueue_GetMessageCountLimit(iq, DXGI_DEBUG_ALL), 1024);
        EXPECT_EQ(IDXGIInfoQueue_GetNumStoredMessagesAllowedByRetrievalFilters(iq, DXGI_DEBUG_ALL), 0); EXPECT_EQ(IDXGIInfoQueue_GetMuteDebugOutput(iq, DXGI_DEBUG_ALL), FALSE);
        EXPECT_EQ(IDXGIInfoQueue_GetStorageFilterStackSize(iq, DXGI_DEBUG_ALL), 0); EXPECT_EQ(IDXGIInfoQueue_GetRetrievalFilterStackSize(iq, DXGI_DEBUG_ALL), 0);
        EXPECT_HR(IDXGIInfoQueue_PushDenyAllStorageFilter(iq, DXGI_DEBUG_ALL), S_OK); EXPECT_EQ(IDXGIInfoQueue_GetStorageFilterStackSize(iq, DXGI_DEBUG_ALL), 1);
        EXPECT_HR(IDXGIInfoQueue_PushDenyAllRetrievalFilter(iq, DXGI_DEBUG_ALL), S_OK); EXPECT_EQ(IDXGIInfoQueue_GetRetrievalFilterStackSize(iq, DXGI_DEBUG_ALL), 1);
        IDXGIInfoQueue_PopStorageFilter(iq, DXGI_DEBUG_ALL); IDXGIInfoQueue_PopRetrievalFilter(iq, DXGI_DEBUG_ALL);
        EXPECT_EQ(IDXGIInfoQueue_GetStorageFilterStackSize(iq, DXGI_DEBUG_ALL), 0); EXPECT_EQ(IDXGIInfoQueue_GetRetrievalFilterStackSize(iq, DXGI_DEBUG_ALL), 0);
        IDXGIInfoQueue_Release(iq);
    }
}

TEST(dde_display_control_adapter3_swapchain_media) {
    DD d; if (!dd_open(&d, 0)) return;
    IDXGIDisplayControl *dc = NULL; EXPECT_HR(IDXGIFactory1_QueryInterface(d.fac, &IID_IDXGIDisplayControl, (void **)&dc), S_OK);
    if (dc) { EXPECT_EQ(IDXGIDisplayControl_IsStereoEnabled(dc), FALSE); IDXGIDisplayControl_SetStereoEnabled(dc, FALSE); EXPECT_EQ(IDXGIDisplayControl_IsStereoEnabled(dc), FALSE); IDXGIDisplayControl_Release(dc); }
    IDXGIAdapter3 *a3 = NULL; EXPECT_HR(IDXGIAdapter1_QueryInterface(d.ad, &IID_IDXGIAdapter3, (void **)&a3), S_OK);
    if (a3) {
        HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL); DWORD cookie = 0xCD;
        EXPECT_HR(IDXGIAdapter3_RegisterHardwareContentProtectionTeardownStatusEvent(a3, ev, &cookie), S_OK); CHECK(cookie != 0xCD);
        EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_TIMEOUT);                         /* no teardown happened */
        IDXGIAdapter3_UnregisterHardwareContentProtectionTeardownStatus(a3, cookie);
        CloseHandle(ev); IDXGIAdapter3_Release(a3);
    }
    HWND w = d3dh_create_hidden_window(); IDXGIFactory2 *f2 = NULL;
    if (SUCCEEDED(IDXGIFactory1_QueryInterface(d.fac, &IID_IDXGIFactory2, (void **)&f2))) {
        IDXGIDevice *dxd = NULL; IDXGIAdapter *sca = NULL; IDXGIFactory2 *sf2 = NULL;
        ID3D11Device_QueryInterface(d.dev, &IID_IDXGIDevice, (void **)&dxd); if (dxd) IDXGIDevice_GetAdapter(dxd, &sca); if (sca) IDXGIAdapter_GetParent(sca, &IID_IDXGIFactory2, (void **)&sf2);
        DXGI_SWAP_CHAIN_DESC1 sd; memset(&sd, 0, sizeof sd); sd.Width = 64; sd.Height = 64; sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM; sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        IDXGISwapChain1 *sc = NULL; EXPECT_HR(IDXGIFactory2_CreateSwapChainForHwnd(sf2 ? sf2 : f2, (IUnknown *)d.dev, w, &sd, NULL, NULL, &sc), S_OK);
        if (sc) {
            IDXGISwapChainMedia *m = NULL; EXPECT_HR(IDXGISwapChain1_QueryInterface(sc, &IID_IDXGISwapChainMedia, (void **)&m), S_OK);
            if (m) {
                DXGI_FRAME_STATISTICS_MEDIA fs; memset(&fs, 0, sizeof fs); EXPECT_HR(IDXGISwapChainMedia_GetFrameStatisticsMedia(m, &fs), DXGI_ERROR_FRAME_STATISTICS_DISJOINT);
                UINT lo = 0xCD, hi = 0xCD; EXPECT_HR(IDXGISwapChainMedia_CheckPresentDurationSupport(m, 166667, &lo, &hi), S_OK); EXPECT_EQ(lo, 0); EXPECT_EQ(hi, 0);
                EXPECT_HR(IDXGISwapChainMedia_SetPresentDuration(m, 166667), S_OK); EXPECT_HR(IDXGISwapChainMedia_SetPresentDuration(m, 0), S_OK);
                IDXGISwapChainMedia_Release(m);
            }
            /* a rejected description must not be reported to the DXGI info queue unless the debug layer is on */
            IDXGISwapChain1_Release(sc);
        }
        SAFE_RELEASE(sf2); SAFE_RELEASE(sca); SAFE_RELEASE(dxd); IDXGIFactory2_Release(f2);
    }
    d3dh_destroy_window(w); dd_close(&d);
}

/* A device from the other adapter (WARP) cannot duplicate the hardware-type adapter's output: E_INVALIDARG. Measured fragility on Win10 22H2:
 * releasing that WARP device afterwards sometimes raises an access violation inside DXGI/D3D11 teardown, so the attempt runs in child processes. */
CHILD(dde_dup_wrong_adapter) {
    DD d; memset(&d, 0, sizeof d);
    if (!d3dh_D3D11CreateDevice() || !d3dh_CreateDXGIFactory1() || FAILED(d3dh_make_factory1(&d.fac))) return 77;
    IDXGIAdapter1 *a0 = NULL; IDXGIOutput *o = NULL;
    for (UINT i = 0; i < 8 && !o; i++) { IDXGIAdapter1 *a = NULL; if (FAILED(IDXGIFactory1_EnumAdapters1(d.fac, i, &a))) break; if (SUCCEEDED(IDXGIAdapter1_EnumOutputs(a, 0, &o))) a0 = a; else IDXGIAdapter1_Release(a); }
    if (!o) return 77;
    ID3D11Device *wd = NULL; ID3D11DeviceContext *wc = NULL; D3D_DRIVER_TYPE dt = 0;
    if (FAILED(d3dh_make_device11(&wd, &wc, NULL, &dt, 0)) || dt != D3D_DRIVER_TYPE_WARP) return 77;
    IDXGIOutput1 *o1 = NULL; IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput1, (void **)&o1);
    IDXGIOutputDuplication *od = NULL; HRESULT hr = IDXGIOutput1_DuplicateOutput(o1, (IUnknown *)wd, &od);
    if (hr != E_INVALIDARG || od) return 90;
    return 0;   /* exiting without releasing: the process teardown is not part of the contract */
}
TEST(dde_output_duplication_wrong_adapter_device) {
    DWORD rc = t_run_child("dde_dup_wrong_adapter", "", 60000);
    if (rc == 77) { t_skip("needs an adapter with an output plus WARP"); return; }
    EXPECT_EQ(rc, 0);
}

/* DuplicateOutput with an object that is not a device (factory, adapter, the output itself): E_NOINTERFACE, the output pointer stays NULL and
 * the reference count of the object is unchanged. Isolated in a child process: following this call with other DuplicateOutput failures in the
 * same process intermittently ended in an access violation inside DXGI (Win10 22H2). */
CHILD(dde_dup_nondevice) {
    DD d; if (!dd_open(&d, 0) || !d.out) return 77;
    IDXGIOutput1 *o1 = NULL; IDXGIOutput_QueryInterface(d.out, &IID_IDXGIOutput1, (void **)&o1);
    IUnknown *objs[3] = { (IUnknown *)d.fac, (IUnknown *)d.ad, (IUnknown *)d.out };
    int bad = 0;
    for (int i = 0; i < 3; i++) {
        ULONG before = d3dh_refcount(objs[i]); IDXGIOutputDuplication *a = NULL;
        HRESULT hr = IDXGIOutput1_DuplicateOutput(o1, objs[i], &a);
        ULONG after = d3dh_refcount(objs[i]);
        printf("  object %d: %s refcount %lu -> %lu out=%p\n", i, d3dh_hr_name(hr), before, after, (void *)a);
        if (hr != E_NOINTERFACE || a != NULL || before != after) bad |= 1 << i;
    }
    fflush(stdout);
    return bad ? 100 + bad : 0;
}
TEST(dde_output_duplication_non_device_object) {
    DWORD rc = t_run_child("dde_dup_nondevice", "", 60000);
    if (rc == 77) { t_skip("no adapter with an output"); return; }
    EXPECT_EQ(rc, 0);
}
