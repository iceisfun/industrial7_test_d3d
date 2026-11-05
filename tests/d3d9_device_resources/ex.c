/* IDirect3D9Ex / IDirect3DDevice9Ex */
#include "d9r.h"

TEST(d3d9_device_resources_ex_create_bad_sdk_version)
{
    REQUIRE(d3dh_Direct3DCreate9Ex());
    /* versions whose low 31 bits are below 0x1f or within 0x21..0x83 are rejected */
    UINT bad[] = { 0, 1, 0x1e, 0x21, 0x22, 0x50, 0x83, 0x80000000u | 0x1e, 0x80000000u | 0x40 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        IDirect3D9Ex *ex = (IDirect3D9Ex *)1;
        HRESULT hr = d3dh_Direct3DCreate9Ex()(bad[i], &ex);
        CHECKF(hr == D3DERR_NOTAVAILABLE, "version 0x%x got %s", bad[i], d3dh_hr_name(hr));
        if (!t_is_wine()) CHECKF(ex == NULL, "version 0x%x left pointer %p", bad[i], ex);   /* real d3d9 clears the output first */
    }
}

TEST(d3d9_device_resources_create9_bad_sdk_version)
{
    REQUIRE(d3dh_Direct3DCreate9());
    UINT bad[] = { 0, 1, 0x1e, 0x21, 0x83 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        IDirect3D9 *d3d = d3dh_Direct3DCreate9()(bad[i]);
        CHECKF(d3d == NULL, "version 0x%x gave an object", bad[i]);
        if (d3d) IDirect3D9_Release(d3d);
    }
}

TEST(d3d9_device_resources_ex_create_object)
{
    REQUIRE(d3dh_Direct3DCreate9Ex());
    IDirect3D9Ex *ex = NULL;
    HRESULT hr = d3dh_Direct3DCreate9Ex()(D3D_SDK_VERSION, &ex);
    if (hr == D3DERR_NOTAVAILABLE) { t_skip("Direct3DCreate9Ex not available"); return; }
    EXPECT_HR(hr, D3D_OK);
    if (!ex) return;
    CHECK(IDirect3D9Ex_GetAdapterCount(ex) >= 1);
    /* the Ex object also answers for the base interface */
    IDirect3D9 *base = NULL;
    EXPECT_HR(IDirect3D9Ex_QueryInterface(ex, &IID_IDirect3D9, (void **)&base), S_OK);
    SAFE_RELEASE(base);
    IDirect3D9Ex *again = NULL;
    EXPECT_HR(IDirect3D9Ex_QueryInterface(ex, &IID_IDirect3D9Ex, (void **)&again), S_OK);
    CHECK(again == ex);
    SAFE_RELEASE(again);
    EXPECT_EQ(IDirect3D9Ex_Release(ex), 0);
}

TEST(d3d9_device_resources_ex_plain_object_not_ex)
{
    REQUIRE(d3dh_Direct3DCreate9());
    IDirect3D9 *d3d = d3dh_Direct3DCreate9()(D3D_SDK_VERSION);
    if (!d3d) { t_skip("Direct3DCreate9 failed"); return; }
    void *p = (void *)1;
    HRESULT hr = IDirect3D9_QueryInterface(d3d, &IID_IDirect3D9Ex, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    IDirect3D9_Release(d3d);
}

TEST(d3d9_device_resources_ex_adapter_funcs)
{
    REQUIRE(d3dh_Direct3DCreate9Ex());
    IDirect3D9Ex *ex = NULL;
    if (FAILED(d3dh_Direct3DCreate9Ex()(D3D_SDK_VERSION, &ex)) || !ex) { t_skip("no 9Ex"); return; }
    UINT n = IDirect3D9Ex_GetAdapterCount(ex);
    LUID luid;
    EXPECT_HR(IDirect3D9Ex_GetAdapterLUID(ex, 0, &luid), D3D_OK);
    EXPECT_HR(IDirect3D9Ex_GetAdapterLUID(ex, n, &luid), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_GetAdapterLUID(ex, 0, NULL), D3DERR_INVALIDCALL);
    D3DDISPLAYMODEEX mode; D3DDISPLAYROTATION rot;
    memset(&mode, 0, sizeof mode); mode.Size = sizeof mode;
    HRESULT hr = IDirect3D9Ex_GetAdapterDisplayModeEx(ex, 0, &mode, &rot);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) { CHECK(mode.Width > 0 && mode.Height > 0); CHECK(rot >= D3DDISPLAYROTATION_IDENTITY && rot <= D3DDISPLAYROTATION_270); EXPECT_EQ(mode.Size, sizeof mode); }
    EXPECT_HR(IDirect3D9Ex_GetAdapterDisplayModeEx(ex, n, &mode, &rot), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_GetAdapterDisplayModeEx(ex, 0, NULL, &rot), D3DERR_INVALIDCALL);
    D3DDISPLAYMODEFILTER flt = { sizeof flt, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_UNKNOWN };
    UINT cnt = IDirect3D9Ex_GetAdapterModeCountEx(ex, 0, &flt);
    t_note("mode count X8R8G8B8: %u", cnt);
    if (cnt) {
        memset(&mode, 0, sizeof mode); mode.Size = sizeof mode;
        EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(ex, 0, &flt, 0, &mode), D3D_OK);
        EXPECT_EQ(mode.Format, D3DFMT_X8R8G8B8);
        EXPECT_HR(IDirect3D9Ex_EnumAdapterModesEx(ex, 0, &flt, cnt, &mode), D3DERR_INVALIDCALL);
    }
    EXPECT_EQ(IDirect3D9Ex_GetAdapterModeCountEx(ex, n, &flt), 0);
    IDirect3D9Ex_Release(ex);
}

TEST(d3d9_device_resources_ex_create_device_args)
{
    D9X_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9Ex *dev2 = NULL;
    EXPECT_HR(IDirect3D9Ex_CreateDeviceEx(d.d3d, 0, d.type, d.hwnd, SWVP, NULL, NULL, &dev2), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_CreateDeviceEx(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, NULL, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9Ex_CreateDeviceEx(d.d3d, 7, d.type, d.hwnd, SWVP, &pp, NULL, &dev2), D3DERR_INVALIDCALL);
    /* a fullscreen mode must not accompany a windowed device */
    D3DDISPLAYMODEEX fm; memset(&fm, 0, sizeof fm);
    fm.Size = sizeof fm; fm.Width = 640; fm.Height = 480; fm.RefreshRate = 60; fm.Format = D3DFMT_X8R8G8B8; fm.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
    HRESULT hr = IDirect3D9Ex_CreateDeviceEx(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &fm, &dev2);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    SAFE_RELEASE(dev2);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_device_queryinterface)
{
    D9X_OPEN(d);
    void *p = NULL;
    EXPECT_HR(IDirect3DDevice9Ex_QueryInterface(d.dev, &IID_IDirect3DDevice9, &p), S_OK);
    if (p) IDirect3DDevice9_Release((IDirect3DDevice9 *)p);
    p = NULL;
    EXPECT_HR(IDirect3DDevice9Ex_QueryInterface(d.dev, &IID_IDirect3DDevice9Ex, &p), S_OK);
    if (p) IDirect3DDevice9Ex_Release((IDirect3DDevice9Ex *)p);
    IDirect3D9 *d3d = NULL;
    EXPECT_HR(IDirect3DDevice9Ex_GetDirect3D(d.dev, &d3d), D3D_OK);
    CHECK(d3d == (IDirect3D9 *)d.d3d);
    SAFE_RELEASE(d3d);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_plain_device_not_ex)
{
    D9_OPEN(d);
    void *p = (void *)1;
    HRESULT hr = IDirect3DDevice9_QueryInterface(d.dev, &IID_IDirect3DDevice9Ex, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_ex_cooperative_level)
{
    D9X_OPEN(d);
    EXPECT_HR(IDirect3DDevice9Ex_TestCooperativeLevel(d.dev), D3D_OK);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_checkdevicestate)
{
    D9X_OPEN(d);
    HRESULT hr = IDirect3DDevice9Ex_CheckDeviceState(d.dev, d.hwnd);
    t_note("CheckDeviceState(hwnd): %s", d3dh_hr_name(hr));
    CHECKF(SUCCEEDED(hr), "got %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9Ex_CheckDeviceState(d.dev, NULL);
    t_note("CheckDeviceState(NULL): %s", d3dh_hr_name(hr));
    CHECKF(SUCCEEDED(hr), "got %s", d3dh_hr_name(hr));
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_gpu_thread_priority)
{
    D9X_OPEN(d);
    INT p = 99;
    EXPECT_HR(IDirect3DDevice9Ex_GetGPUThreadPriority(d.dev, &p), D3D_OK);
    EXPECT_EQ(p, 0);
    EXPECT_HR(IDirect3DDevice9Ex_GetGPUThreadPriority(d.dev, NULL), D3DERR_INVALIDCALL);
    for (INT v = -7; v <= 7; v++) {
        EXPECT_HR(IDirect3DDevice9Ex_SetGPUThreadPriority(d.dev, v), D3D_OK);
        p = 99;
        EXPECT_HR(IDirect3DDevice9Ex_GetGPUThreadPriority(d.dev, &p), D3D_OK);
        CHECKF(p == v, "priority %d read back %d", v, p);
    }
    INT bad[] = { 8, -8, 100, -100, 0x7fffffff, (INT)0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        EXPECT_HR(IDirect3DDevice9Ex_SetGPUThreadPriority(d.dev, bad[i]), D3DERR_INVALIDCALL);
    }
    IDirect3DDevice9Ex_GetGPUThreadPriority(d.dev, &p);
    EXPECT_EQ(p, 7);
    IDirect3DDevice9Ex_SetGPUThreadPriority(d.dev, 0);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_max_frame_latency)
{
    D9X_OPEN(d);
    UINT l = 99;
    EXPECT_HR(IDirect3DDevice9Ex_GetMaximumFrameLatency(d.dev, &l), D3D_OK);
    EXPECT_EQ(l, 3);
    EXPECT_HR(IDirect3DDevice9Ex_GetMaximumFrameLatency(d.dev, NULL), D3DERR_INVALIDCALL);
    for (UINT v = 1; v <= 20; v++) {
        EXPECT_HR(IDirect3DDevice9Ex_SetMaximumFrameLatency(d.dev, v), D3D_OK);
        l = 99;
        IDirect3DDevice9Ex_GetMaximumFrameLatency(d.dev, &l);
        CHECKF(l == v, "latency %u read back %u", v, l);
    }
    EXPECT_HR(IDirect3DDevice9Ex_SetMaximumFrameLatency(d.dev, 21), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9Ex_SetMaximumFrameLatency(d.dev, 1000), D3DERR_INVALIDCALL);
    IDirect3DDevice9Ex_GetMaximumFrameLatency(d.dev, &l);
    EXPECT_EQ(l, 20);
    /* 0 selects the default */
    EXPECT_HR(IDirect3DDevice9Ex_SetMaximumFrameLatency(d.dev, 0), D3D_OK);
    IDirect3DDevice9Ex_GetMaximumFrameLatency(d.dev, &l);
    EXPECT_EQ(l, 3);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_no_managed_pool)
{
    D9X_OPEN(d);
    IDirect3DTexture9 *t = NULL;
    HRESULT hr = IDirect3DDevice9Ex_CreateTexture(d.dev, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    SAFE_RELEASE(t);
    IDirect3DVertexBuffer9 *vb = NULL;
    hr = IDirect3DDevice9Ex_CreateVertexBuffer(d.dev, 64, 0, 0, D3DPOOL_MANAGED, &vb, NULL);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    SAFE_RELEASE(vb);
    IDirect3DIndexBuffer9 *ib = NULL;
    hr = IDirect3DDevice9Ex_CreateIndexBuffer(d.dev, 64, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib, NULL);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    SAFE_RELEASE(ib);
    IDirect3DCubeTexture9 *c = NULL;
    EXPECT_HR(IDirect3DDevice9Ex_CreateCubeTexture(d.dev, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &c, NULL), D3DERR_INVALIDCALL);
    IDirect3DVolumeTexture9 *v = NULL;
    EXPECT_HR(IDirect3DDevice9Ex_CreateVolumeTexture(d.dev, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v, NULL), D3DERR_INVALIDCALL);
    /* other pools stay available */
    hr = IDirect3DDevice9Ex_CreateTexture(d.dev, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &t, NULL);
    EXPECT_HR(hr, D3D_OK);
    SAFE_RELEASE(t);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_surface_ex)
{
    D9X_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    HRESULT hr = IDirect3DDevice9Ex_CreateRenderTargetEx(d.dev, 16, 8, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &s, NULL, 0);
    if (SUCCEEDED(hr)) {
        D3DSURFACE_DESC dd; IDirect3DSurface9_GetDesc(s, &dd);
        EXPECT_EQ(dd.Width, 16); EXPECT_EQ(dd.Height, 8);
        EXPECT_EQ(dd.Usage, D3DUSAGE_RENDERTARGET);
        EXPECT_EQ(dd.Pool, D3DPOOL_DEFAULT);
        IDirect3DSurface9_Release(s);
    } else t_note("CreateRenderTargetEx: %s", d3dh_hr_name(hr));
    EXPECT_HR(IDirect3DDevice9Ex_CreateRenderTargetEx(d.dev, 0, 8, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &s, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9Ex_CreateRenderTargetEx(d.dev, 8, 8, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, NULL, NULL, 0), D3DERR_INVALIDCALL);
    hr = IDirect3DDevice9Ex_CreateOffscreenPlainSurfaceEx(d.dev, 16, 8, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s, NULL, 0);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) {
        D3DSURFACE_DESC dd; IDirect3DSurface9_GetDesc(s, &dd);
        EXPECT_EQ(dd.Pool, D3DPOOL_SYSTEMMEM);
        EXPECT_EQ(dd.Format, D3DFMT_A8R8G8B8);
        IDirect3DSurface9_Release(s);
    }
    EXPECT_HR(IDirect3DDevice9Ex_CreateOffscreenPlainSurfaceEx(d.dev, 16, 8, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &s, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9Ex_CreateOffscreenPlainSurfaceEx(d.dev, 0, 8, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s, NULL, 0), D3DERR_INVALIDCALL);
    hr = IDirect3DDevice9Ex_CreateDepthStencilSurfaceEx(d.dev, 16, 8, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &s, NULL, 0);
    if (SUCCEEDED(hr)) {
        D3DSURFACE_DESC dd; IDirect3DSurface9_GetDesc(s, &dd);
        EXPECT_EQ(dd.Usage, D3DUSAGE_DEPTHSTENCIL);
        EXPECT_EQ(dd.Format, D3DFMT_D24S8);
        IDirect3DSurface9_Release(s);
    } else t_note("CreateDepthStencilSurfaceEx: %s", d3dh_hr_name(hr));
    EXPECT_HR(IDirect3DDevice9Ex_CreateDepthStencilSurfaceEx(d.dev, 16, 8, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &s, NULL, 0), D3DERR_INVALIDCALL);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_resetex)
{
    D9X_OPEN(d);
    EXPECT_HR(IDirect3DDevice9Ex_ResetEx(d.dev, NULL, NULL), D3DERR_INVALIDCALL);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    pp.BackBufferWidth = 80; pp.BackBufferHeight = 40;
    HRESULT hr = IDirect3DDevice9Ex_ResetEx(d.dev, &pp, NULL);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) {
        IDirect3DSurface9 *bb = NULL;
        if (SUCCEEDED(IDirect3DDevice9Ex_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) {
            D3DSURFACE_DESC dd; IDirect3DSurface9_GetDesc(bb, &dd);
            EXPECT_EQ(dd.Width, 80); EXPECT_EQ(dd.Height, 40);
            IDirect3DSurface9_Release(bb);
        }
    }
    /* windowed reset with a fullscreen mode is invalid */
    D3DDISPLAYMODEEX fm; memset(&fm, 0, sizeof fm);
    fm.Size = sizeof fm; fm.Width = 640; fm.Height = 480; fm.RefreshRate = 60; fm.Format = D3DFMT_X8R8G8B8;
    d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9Ex_ResetEx(d.dev, &pp, &fm), D3DERR_INVALIDCALL);
    pp.BackBufferCount = 40;
    EXPECT_HR(IDirect3DDevice9Ex_ResetEx(d.dev, &pp, NULL), D3DERR_INVALIDCALL);
    d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9Ex_ResetEx(d.dev, &pp, NULL), D3D_OK);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_reset_with_default_resources)
{
    D9X_OPEN(d);
    IDirect3DTexture9 *t = NULL;
    if (FAILED(IDirect3DDevice9Ex_CreateTexture(d.dev, 8, 8, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &t, NULL))) { t_skip("no rt texture"); D9X_END(d); return; }
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    HRESULT hr = IDirect3DDevice9Ex_ResetEx(d.dev, &pp, NULL);
    t_note("ResetEx with live default-pool texture: %s", d3dh_hr_name(hr));
    IDirect3DTexture9_Release(t);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_displaymodeex)
{
    D9X_OPEN(d);
    D3DDISPLAYMODEEX m; D3DDISPLAYROTATION rot = (D3DDISPLAYROTATION)99;
    memset(&m, 0, sizeof m); m.Size = sizeof m;
    HRESULT hr = IDirect3DDevice9Ex_GetDisplayModeEx(d.dev, 0, &m, &rot);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) { CHECK(m.Width > 0 && m.Height > 0); CHECK(rot >= 1 && rot <= 4); EXPECT_EQ(m.Size, sizeof m); }
    memset(&m, 0, sizeof m); m.Size = sizeof m;
    EXPECT_HR(IDirect3DDevice9Ex_GetDisplayModeEx(d.dev, 0, &m, NULL), D3D_OK);
    EXPECT_HR(IDirect3DDevice9Ex_GetDisplayModeEx(d.dev, 1, &m, &rot), D3DERR_INVALIDCALL);
    memset(&m, 0, sizeof m);
    hr = IDirect3DDevice9Ex_GetDisplayModeEx(d.dev, 0, &m, &rot);
    t_note("GetDisplayModeEx with Size 0: %s", d3dh_hr_name(hr));
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_presentex)
{
    D9X_OPEN(d);
    HRESULT hr = IDirect3DDevice9Ex_PresentEx(d.dev, NULL, NULL, NULL, NULL, 0);
    CHECKF(SUCCEEDED(hr), "PresentEx got %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9Ex_PresentEx(d.dev, NULL, NULL, NULL, NULL, D3DPRESENT_DONOTWAIT);
    CHECKF(SUCCEEDED(hr) || hr == D3DERR_WASSTILLDRAWING, "DONOTWAIT got %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9Ex_PresentEx(d.dev, NULL, NULL, NULL, NULL, 0x00000100);
    t_note("PresentEx FORCEIMMEDIATE: %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9Ex_PresentEx(d.dev, NULL, NULL, NULL, NULL, 0x00000004);
    t_note("PresentEx DONOTFLIP: %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9Ex_PresentEx(d.dev, NULL, NULL, NULL, NULL, 0x00000200);
    t_note("PresentEx HIDEOVERLAY: %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9Ex_PresentEx(d.dev, NULL, NULL, NULL, NULL, 0x80000000u);
    t_note("PresentEx unknown flag: %s", d3dh_hr_name(hr));
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_present_plain)
{
    D9_OPEN(d);
    HRESULT hr = IDirect3DDevice9_Present(d.dev, NULL, NULL, NULL, NULL);
    CHECKF(SUCCEEDED(hr), "Present got %s", d3dh_hr_name(hr));
    EXPECT_HR(IDirect3DDevice9_Present(d.dev, NULL, NULL, NULL, NULL), D3D_OK);
    IDirect3DSwapChain9 *sc = NULL;
    if (SUCCEEDED(IDirect3DDevice9_GetSwapChain(d.dev, 0, &sc))) {
        hr = IDirect3DSwapChain9_Present(sc, NULL, NULL, NULL, NULL, 0);
        CHECKF(SUCCEEDED(hr), "swapchain Present got %s", d3dh_hr_name(hr));
        hr = IDirect3DSwapChain9_Present(sc, NULL, NULL, NULL, NULL, 0x80000000u);
        t_note("swapchain Present with unknown flag: %s", d3dh_hr_name(hr));
        IDirect3DSwapChain9_Release(sc);
    }
    RECT r = { 0, 0, 16, 16 };
    hr = IDirect3DDevice9_Present(d.dev, &r, NULL, NULL, NULL);
    t_note("Present with source rect on a DISCARD swap chain: %s", d3dh_hr_name(hr));
    D9_END(d);
}

TEST(d3d9_device_resources_ex_wait_for_vblank)
{
    D9X_OPEN(d);
    EXPECT_HR(IDirect3DDevice9Ex_WaitForVBlank(d.dev, 1), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9Ex_WaitForVBlank(d.dev, 0xffffffffu), D3DERR_INVALIDCALL);
    if (t_is_wine()) { t_skip("WaitForVBlank may block without a display"); D9X_END(d); return; }
    HRESULT hr = IDirect3DDevice9Ex_WaitForVBlank(d.dev, 0);
    t_note("WaitForVBlank(0): %s", d3dh_hr_name(hr));
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_check_residency)
{
    D9X_OPEN(d);
    IDirect3DTexture9 *t = NULL;
    if (FAILED(IDirect3DDevice9Ex_CreateTexture(d.dev, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t, NULL))) { t_skip("no default texture"); D9X_END(d); return; }
    IDirect3DResource9 *arr[1] = { (IDirect3DResource9 *)t };
    HRESULT hr = IDirect3DDevice9Ex_CheckResourceResidency(d.dev, arr, 1);
    t_note("CheckResourceResidency(default texture): %s", d3dh_hr_name(hr));
    CHECKF(hr == S_OK || hr == S_RESIDENT_IN_SHARED_MEMORY || hr == S_NOT_RESIDENT || hr == D3DERR_DEVICEHUNG || hr == D3DERR_DEVICEREMOVED, "got %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9Ex_CheckResourceResidency(d.dev, NULL, 1);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    hr = IDirect3DDevice9Ex_CheckResourceResidency(d.dev, arr, 0);
    t_note("CheckResourceResidency(count 0): %s", d3dh_hr_name(hr));
    IDirect3DTexture9_Release(t);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_composerects_args)
{
    D9X_OPEN(d);
    IDirect3DSurface9 *a = NULL, *b = NULL;
    if (FAILED(IDirect3DDevice9Ex_CreateRenderTarget(d.dev, 16, 16, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &a, NULL)) ||
        FAILED(IDirect3DDevice9Ex_CreateRenderTarget(d.dev, 16, 16, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &b, NULL))) {
        t_skip("no render targets"); SAFE_RELEASE(a); D9X_END(d); return; }
    IDirect3DVertexBuffer9 *src = NULL, *dst = NULL;
    IDirect3DDevice9Ex_CreateVertexBuffer(d.dev, 16 * 8, 0, 0, D3DPOOL_DEFAULT, &src, NULL);
    IDirect3DDevice9Ex_CreateVertexBuffer(d.dev, 16 * 16, 0, 0, D3DPOOL_DEFAULT, &dst, NULL);
    HRESULT hr = IDirect3DDevice9Ex_ComposeRects(d.dev, NULL, b, src, 0, dst, D3DCOMPOSERECTS_COPY, 0, 0);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    hr = IDirect3DDevice9Ex_ComposeRects(d.dev, a, NULL, src, 0, dst, D3DCOMPOSERECTS_COPY, 0, 0);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    hr = IDirect3DDevice9Ex_ComposeRects(d.dev, a, b, src, 1, dst, (D3DCOMPOSERECTSOP)99, 0, 0);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    hr = IDirect3DDevice9Ex_ComposeRects(d.dev, a, b, NULL, 1, dst, D3DCOMPOSERECTS_COPY, 0, 0);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    hr = IDirect3DDevice9Ex_ComposeRects(d.dev, a, b, src, 1, NULL, D3DCOMPOSERECTS_COPY, 0, 0);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    hr = IDirect3DDevice9Ex_ComposeRects(d.dev, a, b, src, 0, dst, D3DCOMPOSERECTS_COPY, 0, 0);
    t_note("ComposeRects with zero rects: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(src); SAFE_RELEASE(dst);
    IDirect3DSurface9_Release(a); IDirect3DSurface9_Release(b);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_shared_handle)
{
    D9X_OPEN(d);
    HANDLE h = NULL;
    IDirect3DTexture9 *t = NULL, *t2 = NULL;
    HRESULT hr = IDirect3DDevice9Ex_CreateTexture(d.dev, 16, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t, &h);
    if (FAILED(hr)) { t_note("shared RT texture: %s", d3dh_hr_name(hr)); D9X_END(d); return; }
    CHECK(h != NULL);
    if (h) {
        HANDLE h2 = h;
        hr = IDirect3DDevice9Ex_CreateTexture(d.dev, 16, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t2, &h2);
        t_note("open shared texture on the creating device: %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            D3DSURFACE_DESC a, b;
            IDirect3DTexture9_GetLevelDesc(t, 0, &a);
            IDirect3DTexture9_GetLevelDesc(t2, 0, &b);
            CHECK(a.Width == b.Width && a.Height == b.Height && a.Format == b.Format);
            CHECK(t2 != t);
            IDirect3DTexture9_Release(t2);
        }
        /* mismatching description when opening */
        h2 = h;
        hr = IDirect3DDevice9Ex_CreateTexture(d.dev, 32, 32, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t2, &h2);
        t_note("open shared texture with a different size: %s", d3dh_hr_name(hr));
        SAFE_RELEASE(t2);
    }
    IDirect3DTexture9_Release(t);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_shared_handle_invalid_pool)
{
    D9X_OPEN(d);
    HANDLE h = NULL;
    IDirect3DTexture9 *t = NULL;
    HRESULT hr = IDirect3DDevice9Ex_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &t, &h);
    t_note("shared handle on sysmem texture: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t);
    h = NULL;
    IDirect3DSurface9 *s = NULL;
    hr = IDirect3DDevice9Ex_CreateRenderTarget(d.dev, 16, 16, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &s, &h);
    t_note("shared render target: %s handle %p", d3dh_hr_name(hr), h);
    SAFE_RELEASE(s);
    h = NULL;
    hr = IDirect3DDevice9Ex_CreateOffscreenPlainSurface(d.dev, 16, 16, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s, &h);
    t_note("shared offscreen plain surface: %s handle %p", d3dh_hr_name(hr), h);
    SAFE_RELEASE(s);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_swapchain_ex)
{
    D9X_OPEN(d);
    IDirect3DSwapChain9 *sc = NULL;
    if (FAILED(IDirect3DDevice9Ex_GetSwapChain(d.dev, 0, &sc))) { t_skip("no swapchain"); D9X_END(d); return; }
    IDirect3DSwapChain9Ex *sx = NULL;
    HRESULT hr = IDirect3DSwapChain9_QueryInterface(sc, &IID_IDirect3DSwapChain9Ex, (void **)&sx);
    EXPECT_HR(hr, S_OK);
    if (sx) {
        UINT cnt = 0;
        hr = IDirect3DSwapChain9Ex_GetLastPresentCount(sx, &cnt);
        t_note("GetLastPresentCount: %s %u", d3dh_hr_name(hr), cnt);
        EXPECT_HR(IDirect3DSwapChain9Ex_GetLastPresentCount(sx, NULL), D3DERR_INVALIDCALL);
        D3DPRESENTSTATS st;
        hr = IDirect3DSwapChain9Ex_GetPresentStats(sx, &st);
        t_note("GetPresentStats: %s", d3dh_hr_name(hr));
        D3DDISPLAYMODEEX m; D3DDISPLAYROTATION rot;
        memset(&m, 0, sizeof m); m.Size = sizeof m;
        hr = IDirect3DSwapChain9Ex_GetDisplayModeEx(sx, &m, &rot);
        EXPECT_HR(hr, D3D_OK);
        EXPECT_HR(IDirect3DSwapChain9Ex_GetDisplayModeEx(sx, NULL, &rot), D3DERR_INVALIDCALL);
        IDirect3DSwapChain9Ex_Release(sx);
    }
    IDirect3DSwapChain9_Release(sc);
    D9X_END(d);
}

TEST(d3d9_device_resources_ex_plain_swapchain_not_ex)
{
    D9_OPEN(d);
    IDirect3DSwapChain9 *sc = NULL;
    if (FAILED(IDirect3DDevice9_GetSwapChain(d.dev, 0, &sc))) { t_skip("no swapchain"); D9_END(d); return; }
    void *p = (void *)1;
    EXPECT_HR(IDirect3DSwapChain9_QueryInterface(sc, &IID_IDirect3DSwapChain9Ex, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    IDirect3DSwapChain9_Release(sc);
    D9_END(d);
}

TEST(d3d9_device_resources_ex_creation_params)
{
    D9X_OPEN(d);
    D3DDEVICE_CREATION_PARAMETERS cp;
    EXPECT_HR(IDirect3DDevice9Ex_GetCreationParameters(d.dev, &cp), D3D_OK);
    EXPECT_EQ(cp.DeviceType, d.type);
    CHECK(cp.hFocusWindow == d.hwnd);
    D3DCAPS9 c;
    EXPECT_HR(IDirect3DDevice9Ex_GetDeviceCaps(d.dev, &c), D3D_OK);
    CHECK(c.Caps2 & D3DCAPS2_CANSHARERESOURCE || 1);
    t_note("9Ex caps: Caps2 0x%lx DevCaps2 0x%lx", c.Caps2, c.DevCaps2);
    D9X_END(d);
}
