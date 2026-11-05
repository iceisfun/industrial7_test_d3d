/* device creation, present-parameter validation, caps, creation parameters, Reset */
#include "d9r.h"

TEST(d3d9_device_resources_create_basic)
{
    D9_OPEN(d);
    CHECK(d.dev != NULL);
    IDirect3D9 *got = NULL;
    EXPECT_HR(IDirect3DDevice9_GetDirect3D(d.dev, &got), D3D_OK);
    CHECK(got == d.d3d);
    SAFE_RELEASE(got);
    D9_END(d);
}

TEST(d3d9_device_resources_create_null_args)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9 *dev2 = (IDirect3DDevice9 *)0x1;
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, NULL, &dev2), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, NULL), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_create_bad_adapter)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9 *dev2 = NULL;
    UINT n = IDirect3D9_GetAdapterCount(d.d3d);
    CHECK(n >= 1);
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, n, d.type, d.hwnd, SWVP, &pp, &dev2), D3DERR_INVALIDCALL);
    CHECK(dev2 == NULL);
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0xffffffffu, d.type, d.hwnd, SWVP, &pp, &dev2), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_create_bad_devtype)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9 *dev2 = NULL;
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, (D3DDEVTYPE)0, d.hwnd, SWVP, &pp, &dev2), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, (D3DDEVTYPE)5, d.hwnd, SWVP, &pp, &dev2), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, (D3DDEVTYPE)0x7fffffff, d.hwnd, SWVP, &pp, &dev2), D3DERR_INVALIDCALL);
    CHECK(dev2 == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_create_no_windows)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, NULL);
    IDirect3DDevice9 *dev2 = NULL;
    /* neither focus nor device window */
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, d.type, NULL, SWVP, &pp, &dev2), D3DERR_INVALIDCALL);
    CHECK(dev2 == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_create_invalid_hwnd)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9 *dev2 = NULL;
    HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, (HWND)(ULONG_PTR)0x12345678, SWVP, &pp, &dev2);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    SAFE_RELEASE(dev2);
    D9_END(d);
}

TEST(d3d9_device_resources_create_focus_only)
{
    D9_OPEN(d);
    /* hDeviceWindow NULL: the focus window is used */
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, NULL);
    IDirect3DDevice9 *dev2 = NULL;
    HRESULT hr = d9_create(&d, &pp, SWVP, d.hwnd, &dev2);
    if (FAILED(hr)) { t_note("focus-only create: %s", d3dh_hr_name(hr)); }
    else {
        D3DDEVICE_CREATION_PARAMETERS cp;
        EXPECT_HR(IDirect3DDevice9_GetCreationParameters(dev2, &cp), D3D_OK);
        CHECK(cp.hFocusWindow == d.hwnd);
        IDirect3DDevice9_Release(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pp_zero_size_windowed)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9 *dev2 = NULL;
    RECT rc; GetClientRect(d.hwnd, &rc);
    pp.BackBufferWidth = 0; pp.BackBufferHeight = 0;
    HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) {
        /* documented: pp is updated with the real values */
        EXPECT_EQ(pp.BackBufferWidth, (UINT)(rc.right - rc.left));
        EXPECT_EQ(pp.BackBufferHeight, (UINT)(rc.bottom - rc.top));
        EXPECT_EQ(pp.BackBufferCount, 1);
        CHECK(pp.BackBufferFormat != D3DFMT_UNKNOWN);
        IDirect3DSwapChain9 *sc = NULL;
        if (SUCCEEDED(IDirect3DDevice9_GetSwapChain(dev2, 0, &sc))) {
            D3DPRESENT_PARAMETERS p2;
            EXPECT_HR(IDirect3DSwapChain9_GetPresentParameters(sc, &p2), D3D_OK);
            EXPECT_EQ(p2.BackBufferWidth, pp.BackBufferWidth);
            EXPECT_EQ(p2.BackBufferHeight, pp.BackBufferHeight);
            CHECK(p2.BackBufferFormat != D3DFMT_UNKNOWN);
            EXPECT_EQ(p2.Windowed, TRUE);
            IDirect3DSwapChain9_Release(sc);
        }
        IDirect3DDevice9_Release(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pp_zero_width_only)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9 *dev2 = NULL;
    pp.BackBufferWidth = 0; pp.BackBufferHeight = 32;
    HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
    t_note("width 0 height 32 windowed: %s (w=%u h=%u)", d3dh_hr_name(hr), pp.BackBufferWidth, pp.BackBufferHeight);
    SAFE_RELEASE(dev2);
    D9_END(d);
}

TEST(d3d9_device_resources_pp_backbuffer_count_matrix)
{
    D9_OPEN(d);
    struct { UINT count; D3DSWAPEFFECT se; int want_ok; } t[] = {
        { 0, D3DSWAPEFFECT_DISCARD, 1 }, { 1, D3DSWAPEFFECT_DISCARD, 1 }, { 2, D3DSWAPEFFECT_DISCARD, 1 }, { 3, D3DSWAPEFFECT_DISCARD, 1 },
        { 4, D3DSWAPEFFECT_DISCARD, 0 }, { 100, D3DSWAPEFFECT_DISCARD, 0 },
        { 1, D3DSWAPEFFECT_FLIP, 1 }, { 2, D3DSWAPEFFECT_FLIP, 1 },
        { 1, D3DSWAPEFFECT_COPY, 1 }, { 2, D3DSWAPEFFECT_COPY, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
        pp.BackBufferCount = t[i].count; pp.SwapEffect = t[i].se;
        IDirect3DDevice9 *dev2 = NULL;
        HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
        if (t[i].want_ok) {
            if (FAILED(hr)) t_note("row %u (count %u se %d): %s", i, t[i].count, t[i].se, d3dh_hr_name(hr));
            else if (t[i].count == 0) EXPECT_EQ(pp.BackBufferCount, 1);
        } else
            CHECKF(hr == D3DERR_INVALIDCALL, "row %u count %u se %d got %s", i, t[i].count, t[i].se, d3dh_hr_name(hr));
        SAFE_RELEASE(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pp_swapeffect_invalid)
{
    D9_OPEN(d);
    int bad[] = { 0, 5, 6, 100, -1 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
        pp.SwapEffect = (D3DSWAPEFFECT)bad[i];
        IDirect3DDevice9 *dev2 = NULL;
        HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
        CHECKF(hr == D3DERR_INVALIDCALL, "swapeffect %d got %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pp_multisample_swapeffect)
{
    D9_OPEN(d);
    D3DSWAPEFFECT se[] = { D3DSWAPEFFECT_FLIP, D3DSWAPEFFECT_COPY };
    for (int i = 0; i < 2; i++) {
        D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
        pp.SwapEffect = se[i]; pp.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
        IDirect3DDevice9 *dev2 = NULL;
        HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
        /* multisampling requires SWAPEFFECT_DISCARD */
        CHECKF(hr == D3DERR_INVALIDCALL, "se %d got %s", se[i], d3dh_hr_name(hr));
        SAFE_RELEASE(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pp_multisample_quality)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    pp.MultiSampleType = D3DMULTISAMPLE_NONE; pp.MultiSampleQuality = 1;
    IDirect3DDevice9 *dev2 = NULL;
    HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    SAFE_RELEASE(dev2);
    /* a non-discard swap with MULTISAMPLE_NONMASKABLE is still a multisample request */
    d9_pp(&pp, d.hwnd); pp.MultiSampleType = (D3DMULTISAMPLE_TYPE)17;
    hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(dev2);
    D9_END(d);
}

TEST(d3d9_device_resources_pp_autodepth)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9 *dev2 = NULL;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2), D3DERR_INVALIDCALL);
    SAFE_RELEASE(dev2);
    /* a colour format is not a depth format */
    d9_pp(&pp, d.hwnd); pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_A8R8G8B8;
    HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(dev2);
    /* a usable depth format creates a depth surface */
    D3DFORMAT fm[] = { D3DFMT_D24S8, D3DFMT_D16, D3DFMT_D24X8, D3DFMT_D32 };
    int made = 0;
    for (int i = 0; i < 4 && !made; i++) {
        d9_pp(&pp, d.hwnd); pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = fm[i];
        hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
        if (SUCCEEDED(hr)) {
            IDirect3DSurface9 *ds = NULL;
            EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(dev2, &ds), D3D_OK);
            if (ds) {
                D3DSURFACE_DESC desc; IDirect3DSurface9_GetDesc(ds, &desc);
                EXPECT_EQ(desc.Format, fm[i]);
                EXPECT_EQ(desc.Type, D3DRTYPE_SURFACE);
                EXPECT_EQ(desc.Usage, D3DUSAGE_DEPTHSTENCIL);
                EXPECT_EQ(desc.Pool, D3DPOOL_DEFAULT);
                EXPECT_EQ(desc.Width, 64);
                EXPECT_EQ(desc.Height, 64);
                IDirect3DSurface9_Release(ds);
            }
            IDirect3DDevice9_Release(dev2);
            made = 1;
        }
    }
    if (!made) t_note("no auto depth format creatable");
    D9_END(d);
}

TEST(d3d9_device_resources_no_autodepth_has_no_ds)
{
    D9_OPEN(d);
    IDirect3DSurface9 *ds = (IDirect3DSurface9 *)1;
    HRESULT hr = IDirect3DDevice9_GetDepthStencilSurface(d.dev, &ds);
    EXPECT_HR(hr, D3DERR_NOTFOUND);
    CHECK(ds == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_pp_windowed_refresh)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    pp.FullScreen_RefreshRateInHz = 60;
    IDirect3DDevice9 *dev2 = NULL;
    /* windowed mode requires a zero refresh rate */
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2), D3DERR_INVALIDCALL);
    SAFE_RELEASE(dev2);
    D9_END(d);
}

TEST(d3d9_device_resources_pp_windowed_interval)
{
    D9_OPEN(d);
    /* windowed devices only accept DEFAULT / ONE ... presentation intervals (not TWO..FOUR) */
    UINT ok[] = { D3DPRESENT_INTERVAL_DEFAULT, D3DPRESENT_INTERVAL_ONE, D3DPRESENT_INTERVAL_IMMEDIATE };
    for (unsigned i = 0; i < 3; i++) {
        D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
        pp.PresentationInterval = ok[i];
        IDirect3DDevice9 *dev2 = NULL;
        HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
        if (FAILED(hr)) t_note("interval 0x%x windowed: %s", ok[i], d3dh_hr_name(hr));
        SAFE_RELEASE(dev2);
    }
    UINT bad[] = { 0x3, 0x5, 0x10, 0x20, 0x12345678 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
        pp.PresentationInterval = bad[i];
        IDirect3DDevice9 *dev2 = NULL;
        HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
        CHECKF(FAILED(hr), "interval 0x%x got %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pp_flags)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    IDirect3DDevice9 *dev2 = NULL;
    HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
    if (SUCCEEDED(hr)) {
        IDirect3DSurface9 *bb = NULL;
        EXPECT_HR(IDirect3DDevice9_GetBackBuffer(dev2, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
        if (bb) {
            D3DLOCKED_RECT lr;
            HRESULT lh = IDirect3DSurface9_LockRect(bb, &lr, NULL, D3DLOCK_READONLY);
            EXPECT_HR(lh, D3D_OK);
            if (SUCCEEDED(lh)) { CHECK(lr.pBits != NULL); CHECK(lr.Pitch >= 64 * 4 || lr.Pitch >= 64 * 2); IDirect3DSurface9_UnlockRect(bb); }
            IDirect3DSurface9_Release(bb);
        }
        IDirect3DDevice9_Release(dev2);
    } else t_note("lockable backbuffer: %s", d3dh_hr_name(hr));
    d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2), D3D_OK);
    if (dev2) {
        IDirect3DSurface9 *bb = NULL;
        EXPECT_HR(IDirect3DDevice9_GetBackBuffer(dev2, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
        if (bb) {
            D3DLOCKED_RECT lr;
            /* a back buffer without LOCKABLE_BACKBUFFER cannot be locked */
            EXPECT_HR(IDirect3DSurface9_LockRect(bb, &lr, NULL, 0), D3DERR_INVALIDCALL);
            IDirect3DSurface9_Release(bb);
        }
        IDirect3DDevice9_Release(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pp_backbuffer_format)
{
    D9_OPEN(d);
    D3DFORMAT bad[] = { D3DFMT_DXT1, D3DFMT_DXT5, D3DFMT_D16, D3DFMT_D24S8, D3DFMT_L8, D3DFMT_A8, D3DFMT_V8U8, D3DFMT_INDEX16, D3DFMT_VERTEXDATA, D3DFMT_R32F };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
        pp.BackBufferFormat = bad[i];
        IDirect3DDevice9 *dev2 = NULL;
        HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
        CHECKF(hr == D3DERR_INVALIDCALL || hr == D3DERR_NOTAVAILABLE, "fmt %d got %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pp_backbuffer_format_roundtrip)
{
    D9_OPEN(d);
    D3DFORMAT ok[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
    for (unsigned i = 0; i < 4; i++) {
        D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
        pp.BackBufferFormat = ok[i];
        IDirect3DDevice9 *dev2 = NULL;
        HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP, &pp, &dev2);
        if (SUCCEEDED(hr)) {
            IDirect3DSurface9 *bb = NULL;
            if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev2, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) {
                D3DSURFACE_DESC desc; IDirect3DSurface9_GetDesc(bb, &desc);
                EXPECT_EQ(desc.Format, ok[i]);
                EXPECT_EQ(desc.Type, D3DRTYPE_SURFACE);
                EXPECT_EQ(desc.Usage, D3DUSAGE_RENDERTARGET);
                EXPECT_EQ(desc.Pool, D3DPOOL_DEFAULT);
                EXPECT_EQ(desc.Width, 64);
                EXPECT_EQ(desc.Height, 64);
                EXPECT_EQ(desc.MultiSampleType, D3DMULTISAMPLE_NONE);
                IDirect3DSurface9_Release(bb);
            }
            IDirect3DDevice9_Release(dev2);
        } else t_note("backbuffer fmt %d: %s", ok[i], d3dh_hr_name(hr));
    }
    D9_END(d);
}

TEST(d3d9_device_resources_behavior_flags)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    IDirect3DDevice9 *dev2 = NULL;
    /* SW+HW together is contradictory */
    HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev2);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    SAFE_RELEASE(dev2);
    hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MIXED_VERTEXPROCESSING, &pp, &dev2);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    SAFE_RELEASE(dev2);
    /* no vertex-processing flag at all */
    d9_pp(&pp, d.hwnd);
    hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, 0, &pp, &dev2);
    t_note("behavior flags 0: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(dev2);
    /* flags are recorded verbatim */
    d9_pp(&pp, d.hwnd);
    DWORD fl = SWVP | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;
    hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, fl, &pp, &dev2);
    EXPECT_HR(hr, D3D_OK);
    if (dev2) {
        D3DDEVICE_CREATION_PARAMETERS cp;
        EXPECT_HR(IDirect3DDevice9_GetCreationParameters(dev2, &cp), D3D_OK);
        EXPECT_EQ(cp.BehaviorFlags & fl, fl);
        IDirect3DDevice9_Release(dev2);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_creation_parameters)
{
    D9_OPEN(d);
    D3DDEVICE_CREATION_PARAMETERS cp;
    memset(&cp, 0xcc, sizeof cp);
    EXPECT_HR(IDirect3DDevice9_GetCreationParameters(d.dev, &cp), D3D_OK);
    EXPECT_EQ(cp.AdapterOrdinal, 0);
    EXPECT_EQ(cp.DeviceType, d.type);
    CHECK(cp.hFocusWindow == d.hwnd);
    CHECK(cp.BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING);
    EXPECT_HR(IDirect3DDevice9_GetCreationParameters(d.dev, NULL), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_getcaps_basic)
{
    D9_OPEN(d);
    D3DCAPS9 c, c2;
    memset(&c, 0, sizeof c); memset(&c2, 0, sizeof c2);
    EXPECT_HR(IDirect3DDevice9_GetDeviceCaps(d.dev, &c), D3D_OK);
    EXPECT_EQ(c.DeviceType, d.type);
    EXPECT_EQ(c.AdapterOrdinal, 0);
    EXPECT_HR(IDirect3D9_GetDeviceCaps(d.d3d, 0, d.type, &c2), D3D_OK);
    EXPECT_EQ(c2.DeviceType, d.type);
    CHECK(c.MaxTextureWidth >= 256 && c.MaxTextureHeight >= 256);
    CHECK(c.MaxSimultaneousTextures >= 1);
    CHECK(c.NumSimultaneousRTs >= 1);
    CHECK(c.MaxStreams >= 1);
    CHECK(c.MaxVertexIndex >= 0xffff);
    CHECK(c.MaxPrimitiveCount >= 0xffff);
    CHECK((c.Caps2 & D3DCAPS2_CANMANAGERESOURCE) || 1);
    CHECK(c.PresentationIntervals & D3DPRESENT_INTERVAL_ONE);
    EXPECT_EQ(c.MaxVertexShaderConst > 0, c.VertexShaderVersion >= D3DVS_VERSION(1, 0));
    EXPECT_HR(IDirect3DDevice9_GetDeviceCaps(d.dev, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_GetDeviceCaps(d.d3d, 0, d.type, NULL), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_getcaps_invalid)
{
    D9_OPEN(d);
    D3DCAPS9 c;
    UINT n = IDirect3D9_GetAdapterCount(d.d3d);
    EXPECT_HR(IDirect3D9_GetDeviceCaps(d.d3d, n, d.type, &c), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_GetDeviceCaps(d.d3d, 0, (D3DDEVTYPE)0, &c), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_GetDeviceCaps(d.d3d, 0, (D3DDEVTYPE)9, &c), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_caps_consistency)
{
    D9_OPEN(d);
    D3DCAPS9 c;
    EXPECT_HR(IDirect3DDevice9_GetDeviceCaps(d.dev, &c), D3D_OK);
    /* all D3D9 devices expose at least shader model 0 fixed function, and consistent version fields */
    CHECK((c.VertexShaderVersion & 0xffff0000u) == 0xfffe0000u);
    CHECK((c.PixelShaderVersion & 0xffff0000u) == 0xffff0000u);
    if (c.VertexShaderVersion >= D3DVS_VERSION(3, 0)) CHECK(c.PixelShaderVersion >= D3DPS_VERSION(3, 0) || 1);
    CHECK(c.MaxVolumeExtent >= 1 || !(c.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP));
    CHECK(c.MaxAnisotropy >= 1);
    t_note("caps: type %d textures %ux%u vs %lx ps %lx rts %lu", c.DeviceType, c.MaxTextureWidth, c.MaxTextureHeight, c.VertexShaderVersion, c.PixelShaderVersion, c.NumSimultaneousRTs);
    D9_END(d);
}

TEST(d3d9_device_resources_displaymode)
{
    D9_OPEN(d);
    D3DDISPLAYMODE m;
    memset(&m, 0xcc, sizeof m);
    EXPECT_HR(IDirect3DDevice9_GetDisplayMode(d.dev, 0, &m), D3D_OK);
    CHECK(m.Width > 0 && m.Height > 0);
    CHECK(m.Format == D3DFMT_X8R8G8B8 || m.Format == D3DFMT_A2R10G10B10 || m.Format == D3DFMT_R5G6B5 || m.Format == D3DFMT_X1R5G5B5);
    EXPECT_HR(IDirect3DDevice9_GetDisplayMode(d.dev, 0, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetDisplayMode(d.dev, 1, &m), D3DERR_INVALIDCALL);
    D3DDISPLAYMODE m2;
    EXPECT_HR(IDirect3D9_GetAdapterDisplayMode(d.d3d, 0, &m2), D3D_OK);
    EXPECT_EQ(m2.Width, m.Width);
    EXPECT_EQ(m2.Height, m.Height);
    EXPECT_EQ(m2.Format, m.Format);
    EXPECT_HR(IDirect3D9_GetAdapterDisplayMode(d.d3d, 0, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_GetAdapterDisplayMode(d.d3d, 100, &m2), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_cooperative_level)
{
    D9_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_TestCooperativeLevel(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_TestCooperativeLevel(d.dev), D3D_OK);
    D9_END(d);
}

TEST(d3d9_device_resources_swapchain_count)
{
    D9_OPEN(d);
    EXPECT_EQ(IDirect3DDevice9_GetNumberOfSwapChains(d.dev), 1);
    IDirect3DSwapChain9 *sc = (IDirect3DSwapChain9 *)1;
    EXPECT_HR(IDirect3DDevice9_GetSwapChain(d.dev, 1, &sc), D3DERR_INVALIDCALL);
    CHECK(sc == NULL);
    EXPECT_HR(IDirect3DDevice9_GetSwapChain(d.dev, 0, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetSwapChain(d.dev, 0, &sc), D3D_OK);
    if (sc) {
        IDirect3DDevice9 *dd = NULL;
        EXPECT_HR(IDirect3DSwapChain9_GetDevice(sc, &dd), D3D_OK);
        CHECK(dd == d.dev);
        SAFE_RELEASE(dd);
        IDirect3DSwapChain9_Release(sc);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_backbuffer_args)
{
    D9_OPEN(d);
    IDirect3DSurface9 *bb = NULL;
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
    SAFE_RELEASE(bb);
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 1, D3DBACKBUFFER_TYPE_MONO, &bb), D3DERR_INVALIDCALL);
    CHECK(bb == NULL);
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 1, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3DERR_INVALIDCALL);
    t_note("GetBackBuffer type 3: %s", d3dh_hr_name(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, (D3DBACKBUFFER_TYPE)3, &bb)));
    SAFE_RELEASE(bb);
    D9_END(d);
}

TEST(d3d9_device_resources_rendertarget_default)
{
    D9_OPEN(d);
    IDirect3DSurface9 *rt = NULL, *bb = NULL;
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 0, &rt), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
    CHECK(rt != NULL && rt == bb);
    SAFE_RELEASE(rt); SAFE_RELEASE(bb);
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 0, NULL), D3DERR_INVALIDCALL);
    D3DCAPS9 c; IDirect3DDevice9_GetDeviceCaps(d.dev, &c);
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, c.NumSimultaneousRTs, &rt), D3DERR_INVALIDCALL);
    if (c.NumSimultaneousRTs > 1) {
        rt = (IDirect3DSurface9 *)1;
        EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 1, &rt), D3DERR_NOTFOUND);
        CHECK(rt == NULL);
    }
    /* the primary RT may not be removed */
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, NULL), D3DERR_INVALIDCALL);
    D9_END(d);
}

/* ---- Reset ---- */
TEST(d3d9_device_resources_reset_basic)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    pp.BackBufferWidth = 96; pp.BackBufferHeight = 48;
    HRESULT hr = IDirect3DDevice9_Reset(d.dev, &pp);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) {
        IDirect3DSurface9 *bb = NULL;
        EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
        if (bb) { D3DSURFACE_DESC dd; IDirect3DSurface9_GetDesc(bb, &dd); EXPECT_EQ(dd.Width, 96); EXPECT_EQ(dd.Height, 48); IDirect3DSurface9_Release(bb); }
        EXPECT_HR(IDirect3DDevice9_TestCooperativeLevel(d.dev), D3D_OK);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_reset_null)
{
    D9_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, NULL), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_reset_invalid_pp)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    pp.BackBufferCount = 4;
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3DERR_INVALIDCALL);
    d9_pp(&pp, d.hwnd); pp.SwapEffect = (D3DSWAPEFFECT)99;
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3DERR_INVALIDCALL);
    d9_pp(&pp, d.hwnd); pp.SwapEffect = D3DSWAPEFFECT_COPY; pp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3DERR_INVALIDCALL);
    d9_pp(&pp, d.hwnd); pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3DERR_INVALIDCALL);
    /* after a failed Reset the device is lost until a valid Reset succeeds */
    HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(d.dev);
    t_note("TestCooperativeLevel after failed Reset: %s", d3dh_hr_name(hr));
    d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_TestCooperativeLevel(d.dev), D3D_OK);
    D9_END(d);
}

TEST(d3d9_device_resources_reset_default_pool_blocks)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL);
    if (FAILED(hr)) { t_skip("no default pool RT texture: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_TestCooperativeLevel(d.dev), D3DERR_DEVICENOTRESET);
    IDirect3DTexture9_Release(tex);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_TestCooperativeLevel(d.dev), D3D_OK);
    D9_END(d);
}

TEST(d3d9_device_resources_reset_default_surface_blocks)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(d.dev, 16, 16, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &s, NULL);
    if (FAILED(hr)) { t_skip("no RT: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3DERR_INVALIDCALL);
    IDirect3DSurface9_Release(s);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3D_OK);
    D9_END(d);
}

TEST(d3d9_device_resources_reset_managed_survives)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    if (FAILED(hr)) { t_skip("no managed tex: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    D3DLOCKED_RECT lr;
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0), D3D_OK);
    ((DWORD *)lr.pBits)[0] = 0xdeadbeef;
    IDirect3DTexture9_UnlockRect(tex, 0);
    IDirect3DTexture9 *sys = NULL;
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3D_OK);
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, D3DLOCK_READONLY), D3D_OK);
    EXPECT_EQ(((DWORD *)lr.pBits)[0], 0xdeadbeef);
    IDirect3DTexture9_UnlockRect(tex, 0);
    IDirect3DTexture9_Release(tex);
    (void)sys;
    D9_END(d);
}

TEST(d3d9_device_resources_reset_sysmem_survives)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(d.dev, 8, 8, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s, NULL);
    if (FAILED(hr)) { t_skip("no sysmem surface: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3D_OK);
    D3DSURFACE_DESC dd; IDirect3DSurface9_GetDesc(s, &dd);
    EXPECT_EQ(dd.Width, 8);
    IDirect3DSurface9_Release(s);
    D9_END(d);
}

TEST(d3d9_device_resources_reset_clears_backbuffer_refs)
{
    D9_OPEN(d);
    /* holding a back buffer reference makes Reset fail until it is released */
    IDirect3DSurface9 *bb = NULL;
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    HRESULT hr = IDirect3DDevice9_Reset(d.dev, &pp);
    t_note("Reset with outstanding backbuffer ref: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(bb);
    hr = IDirect3DDevice9_Reset(d.dev, &pp);
    EXPECT_HR(hr, D3D_OK);
    D9_END(d);
}

TEST(d3d9_device_resources_reset_state_defaults)
{
    D9_OPEN(d);
    DWORD v = 0;
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ZENABLE, D3DZB_FALSE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_NONE), D3D_OK);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v), D3D_OK);
    EXPECT_EQ(v, D3DCULL_CCW);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ZENABLE, &v), D3D_OK);
    EXPECT_EQ(v, D3DZB_FALSE);   /* no auto depth stencil => default is FALSE */
    D9_END(d);
}

TEST(d3d9_device_resources_reset_auto_depth_toggle)
{
    D9_OPEN(d);
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D16;
    HRESULT hr = IDirect3DDevice9_Reset(d.dev, &pp);
    if (FAILED(hr)) { t_note("reset with D16 auto depth: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    IDirect3DSurface9 *ds = NULL;
    EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &ds), D3D_OK);
    SAFE_RELEASE(ds);
    DWORD z = 0;
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ZENABLE, &z);
    EXPECT_EQ(z, D3DZB_TRUE);
    d9_pp(&pp, d.hwnd);
    EXPECT_HR(IDirect3DDevice9_Reset(d.dev, &pp), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &ds), D3DERR_NOTFOUND);
    D9_END(d);
}

TEST(d3d9_device_resources_evict_managed)
{
    D9_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_EvictManagedResources(d.dev), D3D_OK);
    IDirect3DTexture9 *tex = NULL;
    if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL))) {
        D3DLOCKED_RECT lr;
        IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
        ((DWORD *)lr.pBits)[3] = 0x01020304;
        IDirect3DTexture9_UnlockRect(tex, 0);
        EXPECT_HR(IDirect3DDevice9_EvictManagedResources(d.dev), D3D_OK);
        EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, D3DLOCK_READONLY), D3D_OK);
        EXPECT_EQ(((DWORD *)lr.pBits)[3], 0x01020304);
        IDirect3DTexture9_UnlockRect(tex, 0);
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_available_texture_mem)
{
    D9_OPEN(d);
    UINT m1 = IDirect3DDevice9_GetAvailableTextureMem(d.dev);
    CHECK(m1 > 0);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 1024, 1024, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL);
    if (SUCCEEDED(hr)) {
        UINT m2 = IDirect3DDevice9_GetAvailableTextureMem(d.dev);
        CHECK(m2 <= m1);
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_device_refcount_children)
{
    D9_OPEN(d);
    ULONG base = d3dh_refcount(d.dev);
    IDirect3DSurface9 *s = NULL;
    HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(d.dev, 4, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s, NULL);
    if (SUCCEEDED(hr)) {
        /* child resources hold a reference to the device only through the internal count; GetDevice adds one */
        IDirect3DDevice9 *dd = NULL;
        EXPECT_HR(IDirect3DSurface9_GetDevice(s, &dd), D3D_OK);
        CHECK(dd == d.dev);
        ULONG with = d3dh_refcount(d.dev);
        CHECK(with >= base + 1);
        SAFE_RELEASE(dd);
        IDirect3DSurface9_Release(s);
    }
    EXPECT_EQ(d3dh_refcount(d.dev), base);
    D9_END(d);
}

TEST(d3d9_device_resources_d3d9_object)
{
    if (!d3dh_Direct3DCreate9()) { t_skip("no Direct3DCreate9"); return; }
    IDirect3D9 *d3d = d3dh_Direct3DCreate9()(D3D_SDK_VERSION);
    if (!d3d) { t_skip("Direct3DCreate9 failed"); return; }
    CHECK(IDirect3D9_GetAdapterCount(d3d) >= 1);
    D3DADAPTER_IDENTIFIER9 id;
    EXPECT_HR(IDirect3D9_GetAdapterIdentifier(d3d, 0, 0, &id), D3D_OK);
    CHECK(id.Driver[0] != 0 || id.Description[0] != 0 || 1);
    EXPECT_HR(IDirect3D9_GetAdapterIdentifier(d3d, 0, 0, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_GetAdapterIdentifier(d3d, 100, 0, &id), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_GetAdapterIdentifier(d3d, 0, 0x2, &id), D3DERR_INVALIDCALL);
    HMONITOR mon = IDirect3D9_GetAdapterMonitor(d3d, 0);
    CHECK(mon != NULL || t_is_wine());
    CHECK(IDirect3D9_GetAdapterMonitor(d3d, 100) == NULL);
    CHECK(IDirect3D9_GetAdapterModeCount(d3d, 100, D3DFMT_X8R8G8B8) == 0);
    EXPECT_EQ(IDirect3D9_Release(d3d), 0);
}

TEST(d3d9_device_resources_d3d9_query_iface)
{
    if (!d3dh_Direct3DCreate9()) { t_skip("no Direct3DCreate9"); return; }
    IDirect3D9 *d3d = d3dh_Direct3DCreate9()(D3D_SDK_VERSION);
    if (!d3d) { t_skip("Direct3DCreate9 failed"); return; }
    IUnknown *u = NULL;
    EXPECT_HR(IDirect3D9_QueryInterface(d3d, &IID_IUnknown, (void **)&u), S_OK);
    SAFE_RELEASE(u);
    void *p = (void *)1;
    EXPECT_HR(IDirect3D9_QueryInterface(d3d, &IID_IDirect3D9, &p), S_OK);
    if (p) IDirect3D9_Release((IDirect3D9 *)p);
    p = (void *)1;
    HRESULT hr = IDirect3D9_QueryInterface(d3d, &IID_IDirect3DDevice9, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    IDirect3D9_Release(d3d);
}

TEST(d3d9_device_resources_checkdevicetype)
{
    D9_OPEN(d);
    EXPECT_HR(IDirect3D9_CheckDeviceType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D3D_OK);
    EXPECT_HR(IDirect3D9_CheckDeviceType(d.d3d, 100, d.type, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceType(d.d3d, 0, (D3DDEVTYPE)0, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceType(d.d3d, 0, d.type, D3DFMT_DXT1, D3DFMT_X8R8G8B8, TRUE), D3DERR_NOTAVAILABLE);
    HRESULT hr = IDirect3D9_CheckDeviceType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, D3DFMT_D24S8, TRUE);
    EXPECT_HR_FAILED(hr);
    D9_END(d);
}

TEST(d3d9_device_resources_checkdeviceformat)
{
    D9_OPEN(d);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D3D_OK);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d.d3d, 100, d.type, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, 0, (D3DRESOURCETYPE)0, D3DFMT_A8R8G8B8), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D3DERR_NOTAVAILABLE);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, D3DFMT_A8R8G8B8), D3D_OK);
    HRESULT hr = IDirect3D9_CheckDeviceFormat(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_DXT1);
    EXPECT_HR(hr, D3DERR_NOTAVAILABLE);
    D9_END(d);
}

TEST(d3d9_device_resources_checkmultisample)
{
    D9_OPEN(d);
    DWORD q = 0xcc;
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, &q), D3D_OK);
    CHECK(q >= 1);
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, NULL), D3D_OK);
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 100, d.type, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, &q), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, TRUE, (D3DMULTISAMPLE_TYPE)17, &q), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, TRUE, (D3DMULTISAMPLE_TYPE)1, &q), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_DXT1, TRUE, D3DMULTISAMPLE_2_SAMPLES, &q), D3DERR_NOTAVAILABLE);
    D9_END(d);
}

TEST(d3d9_device_resources_enum_adapter_modes)
{
    D9_OPEN(d);
    UINT n = IDirect3D9_GetAdapterModeCount(d.d3d, 0, D3DFMT_X8R8G8B8);
    D3DDISPLAYMODE m;
    if (n) {
        EXPECT_HR(IDirect3D9_EnumAdapterModes(d.d3d, 0, D3DFMT_X8R8G8B8, 0, &m), D3D_OK);
        EXPECT_EQ(m.Format, D3DFMT_X8R8G8B8);
        EXPECT_HR(IDirect3D9_EnumAdapterModes(d.d3d, 0, D3DFMT_X8R8G8B8, n, &m), D3DERR_INVALIDCALL);
    }
    EXPECT_HR(IDirect3D9_EnumAdapterModes(d.d3d, 0, D3DFMT_X8R8G8B8, 0, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_EnumAdapterModes(d.d3d, 100, D3DFMT_X8R8G8B8, 0, &m), D3DERR_INVALIDCALL);
    EXPECT_EQ(IDirect3D9_GetAdapterModeCount(d.d3d, 0, D3DFMT_DXT1), 0);
    EXPECT_EQ(IDirect3D9_GetAdapterModeCount(d.d3d, 0, D3DFMT_A8R8G8B8), 0);
    D9_END(d);
}
