/* render targets, depth surfaces, offscreen plain surfaces, copy operations */
#include "d9r.h"

static HRESULT mk_off(D9 *d, UINT w, UINT h, D3DFORMAT f, D3DPOOL p, IDirect3DSurface9 **s)
{
    return IDirect3DDevice9_CreateOffscreenPlainSurface(d->dev, w, h, f, p, s, NULL);
}
static HRESULT mk_rt(D9 *d, UINT w, UINT h, D3DFORMAT f, BOOL lockable, IDirect3DSurface9 **s)
{
    return IDirect3DDevice9_CreateRenderTarget(d->dev, w, h, f, D3DMULTISAMPLE_NONE, 0, lockable, s, NULL);
}

/* ---- offscreen plain ---- */
TEST(d3d9_device_resources_off_args)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = (IDirect3DSurface9 *)1;
    EXPECT_HR(mk_off(&d, 0, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s), D3DERR_INVALIDCALL);
    CHECK(s == NULL);
    EXPECT_HR(mk_off(&d, 4, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s), D3DERR_INVALIDCALL);
    EXPECT_HR(mk_off(&d, 4, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(mk_off(&d, 4, 4, D3DFMT_A8R8G8B8, (D3DPOOL)4, &s), D3DERR_INVALIDCALL);
    EXPECT_HR(mk_off(&d, 4, 4, D3DFMT_UNKNOWN, D3DPOOL_SYSTEMMEM, &s), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_off_managed_invalid)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    EXPECT_HR(mk_off(&d, 4, 4, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &s), D3DERR_INVALIDCALL);
    CHECK(s == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_off_pools_desc)
{
    D9_OPEN(d);
    D3DPOOL pools[] = { D3DPOOL_DEFAULT, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH };
    D3DFORMAT fm[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A8, D3DFMT_L8, D3DFMT_A16B16G16R16F };
    for (unsigned p = 0; p < 3; p++)
        for (unsigned i = 0; i < sizeof fm / sizeof *fm; i++) {
            IDirect3DSurface9 *s = NULL;
            HRESULT hr = mk_off(&d, 12, 7, fm[i], pools[p], &s);
            if (FAILED(hr)) { t_note("offscreen pool %d fmt %d: %s", pools[p], fm[i], d3dh_hr_name(hr)); continue; }
            D3DSURFACE_DESC dd;
            EXPECT_HR(IDirect3DSurface9_GetDesc(s, &dd), D3D_OK);
            EXPECT_EQ(dd.Format, fm[i]);
            EXPECT_EQ(dd.Type, D3DRTYPE_SURFACE);
            EXPECT_EQ(dd.Usage, 0);
            EXPECT_EQ(dd.Pool, pools[p]);
            EXPECT_EQ(dd.MultiSampleType, D3DMULTISAMPLE_NONE);
            EXPECT_EQ(dd.Width, 12);
            EXPECT_EQ(dd.Height, 7);
            EXPECT_HR(IDirect3DSurface9_GetDesc(s, NULL), D3DERR_INVALIDCALL);
            IDirect3DSurface9_Release(s);
        }
    D9_END(d);
}

TEST(d3d9_device_resources_off_lock_pitch)
{
    D9_OPEN(d);
    D3DFORMAT fm[] = { D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A8, D3DFMT_A32B32G32R32F, D3DFMT_DXT1, D3DFMT_DXT5 };
    UINT w[] = { 1, 5, 16, 33 };
    for (unsigned i = 0; i < sizeof fm / sizeof *fm; i++)
        for (unsigned j = 0; j < 4; j++) {
            IDirect3DSurface9 *s = NULL;
            if (FAILED(mk_off(&d, w[j], 4, fm[i], D3DPOOL_SCRATCH, &s))) { t_note("scratch off fmt %d w %u unavailable", fm[i], w[j]); continue; }
            D3DLOCKED_RECT lr;
            EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr, NULL, 0), D3D_OK);
            CHECKF(lr.Pitch >= (int)d9_minpitch(fm[i], w[j]), "fmt %d w %u pitch %d", fm[i], w[j], lr.Pitch);
            CHECK(lr.pBits != NULL);
            IDirect3DSurface9_UnlockRect(s);
            IDirect3DSurface9_Release(s);
        }
    D9_END(d);
}

TEST(d3d9_device_resources_surf_lock_rules)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    if (FAILED(mk_off(&d, 16, 16, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s))) { t_skip("no sysmem surface"); D9_END(d); return; }
    D3DLOCKED_RECT lr, full, lr2;
    EXPECT_HR(IDirect3DSurface9_LockRect(s, NULL, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DSurface9_UnlockRect(s), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DSurface9_LockRect(s, &full, NULL, 0), D3D_OK);
    EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr2, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DSurface9_UnlockRect(s), D3D_OK);
    EXPECT_HR(IDirect3DSurface9_UnlockRect(s), D3DERR_INVALIDCALL);
    RECT r = { 2, 3, 10, 12 };
    EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr, &r, 0), D3D_OK);
    EXPECT_EQ(lr.Pitch, full.Pitch);
    EXPECT_EQ((char *)lr.pBits - (char *)full.pBits, 3 * full.Pitch + 2 * 4);
    IDirect3DSurface9_UnlockRect(s);
    RECT bad[] = { {-1,0,4,4}, {0,-1,4,4}, {0,0,17,4}, {0,0,4,17}, {4,4,4,8}, {4,4,8,4}, {8,0,4,4}, {0,8,4,4} };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr = IDirect3DSurface9_LockRect(s, &lr, &bad[i], 0);
        CHECKF(hr == D3DERR_INVALIDCALL, "rect %u got %s", i, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirect3DSurface9_UnlockRect(s);
    }
    IDirect3DSurface9_Release(s);
    D9_END(d);
}

TEST(d3d9_device_resources_surf_data_roundtrip)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    if (FAILED(mk_off(&d, 7, 5, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s))) { t_skip("no sysmem surface"); D9_END(d); return; }
    D3DLOCKED_RECT lr;
    IDirect3DSurface9_LockRect(s, &lr, NULL, 0);
    for (int y = 0; y < 5; y++) for (int x = 0; x < 7; x++) ((DWORD *)((char *)lr.pBits + y * lr.Pitch))[x] = (y << 16) | (x << 8) | 0xff000000u;
    IDirect3DSurface9_UnlockRect(s);
    IDirect3DSurface9_LockRect(s, &lr, NULL, D3DLOCK_READONLY);
    int bad = 0;
    for (int y = 0; y < 5; y++) for (int x = 0; x < 7; x++) if (((DWORD *)((char *)lr.pBits + y * lr.Pitch))[x] != (DWORD)((y << 16) | (x << 8) | 0xff000000u)) bad++;
    CHECK(bad == 0);
    IDirect3DSurface9_UnlockRect(s);
    IDirect3DSurface9_Release(s);
    D9_END(d);
}

TEST(d3d9_device_resources_surf_getdc)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    if (FAILED(mk_off(&d, 16, 16, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &s))) { t_skip("no sysmem surface"); D9_END(d); return; }
    HDC dc = NULL;
    EXPECT_HR(IDirect3DSurface9_GetDC(s, NULL), D3DERR_INVALIDCALL);
    HRESULT hr = IDirect3DSurface9_GetDC(s, &dc);
    if (FAILED(hr)) { t_note("GetDC on sysmem X8R8G8B8: %s", d3dh_hr_name(hr)); IDirect3DSurface9_Release(s); D9_END(d); return; }
    CHECK(dc != NULL);
    D3DLOCKED_RECT lr;
    EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr, NULL, 0), D3DERR_INVALIDCALL);
    HDC dc2 = NULL;
    EXPECT_HR(IDirect3DSurface9_GetDC(s, &dc2), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DSurface9_ReleaseDC(s, dc), D3D_OK);
    EXPECT_HR(IDirect3DSurface9_ReleaseDC(s, dc), D3DERR_INVALIDCALL);
    /* and the other way round */
    EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr, NULL, 0), D3D_OK);
    EXPECT_HR(IDirect3DSurface9_GetDC(s, &dc), D3DERR_INVALIDCALL);
    IDirect3DSurface9_UnlockRect(s);
    IDirect3DSurface9_Release(s);
    D9_END(d);
}

TEST(d3d9_device_resources_surf_getdc_bad_format)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    if (FAILED(mk_off(&d, 16, 16, D3DFMT_DXT1, D3DPOOL_SCRATCH, &s))) { t_skip("no dxt surface"); D9_END(d); return; }
    HDC dc = NULL;
    EXPECT_HR(IDirect3DSurface9_GetDC(s, &dc), D3DERR_INVALIDCALL);
    IDirect3DSurface9_Release(s);
    D9_END(d);
}

TEST(d3d9_device_resources_surf_type_device_qi)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    if (FAILED(mk_off(&d, 4, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &s))) { t_skip("no sysmem surface"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DSurface9_GetType(s), D3DRTYPE_SURFACE);
    IDirect3DDevice9 *dd = NULL;
    EXPECT_HR(IDirect3DSurface9_GetDevice(s, &dd), D3D_OK);
    CHECK(dd == d.dev);
    SAFE_RELEASE(dd);
    void *c = (void *)1;
    /* a standalone surface has no container other than the device */
    HRESULT hr = IDirect3DSurface9_GetContainer(s, &IID_IDirect3DTexture9, &c);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(c == NULL);
    c = NULL;
    hr = IDirect3DSurface9_GetContainer(s, &IID_IDirect3DDevice9, &c);
    EXPECT_HR(hr, D3D_OK);
    if (c) IDirect3DDevice9_Release((IDirect3DDevice9 *)c);
    c = NULL;
    EXPECT_HR(IDirect3DSurface9_QueryInterface(s, &IID_IDirect3DResource9, &c), S_OK);
    if (c) IDirect3DResource9_Release((IDirect3DResource9 *)c);
    c = (void *)1;
    EXPECT_HR(IDirect3DSurface9_QueryInterface(s, &IID_IDirect3DTexture9, &c), E_NOINTERFACE);
    IDirect3DSurface9_Release(s);
    D9_END(d);
}

/* ---- render targets ---- */
TEST(d3d9_device_resources_rt_args)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = (IDirect3DSurface9 *)1;
    EXPECT_HR(mk_rt(&d, 0, 4, D3DFMT_X8R8G8B8, FALSE, &s), D3DERR_INVALIDCALL);
    CHECK(s == NULL);
    EXPECT_HR(mk_rt(&d, 4, 0, D3DFMT_X8R8G8B8, FALSE, &s), D3DERR_INVALIDCALL);
    EXPECT_HR(mk_rt(&d, 4, 4, D3DFMT_X8R8G8B8, FALSE, NULL), D3DERR_INVALIDCALL);
    D3DFORMAT bad[] = { D3DFMT_UNKNOWN, D3DFMT_D24S8, D3DFMT_DXT1, D3DFMT_INDEX16, D3DFMT_VERTEXDATA };
    for (unsigned i = 0; i < 5; i++) {
        HRESULT hr = mk_rt(&d, 4, 4, bad[i], FALSE, &s);
        CHECKF(hr == D3DERR_INVALIDCALL || hr == D3DERR_NOTAVAILABLE, "fmt %d got %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_rt_desc)
{
    D9_OPEN(d);
    D3DFORMAT fm[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A16B16G16R16F, D3DFMT_A32B32G32R32F, D3DFMT_R32F, D3DFMT_A2R10G10B10, D3DFMT_A8B8G8R8 };
    for (unsigned i = 0; i < sizeof fm / sizeof *fm; i++) {
        IDirect3DSurface9 *s = NULL;
        HRESULT hr = mk_rt(&d, 20, 10, fm[i], FALSE, &s);
        if (FAILED(hr)) { t_note("rt fmt %d: %s", fm[i], d3dh_hr_name(hr)); continue; }
        D3DSURFACE_DESC dd;
        EXPECT_HR(IDirect3DSurface9_GetDesc(s, &dd), D3D_OK);
        EXPECT_EQ(dd.Format, fm[i]);
        EXPECT_EQ(dd.Type, D3DRTYPE_SURFACE);
        EXPECT_EQ(dd.Usage, D3DUSAGE_RENDERTARGET);
        EXPECT_EQ(dd.Pool, D3DPOOL_DEFAULT);
        EXPECT_EQ(dd.Width, 20);
        EXPECT_EQ(dd.Height, 10);
        IDirect3DSurface9_Release(s);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_rt_lockable)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, FALSE, &s))) {
        EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr, NULL, 0), D3DERR_INVALIDCALL);
        EXPECT_HR(IDirect3DSurface9_UnlockRect(s), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(s);
    }
    if (SUCCEEDED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, TRUE, &s))) {
        HRESULT hr = IDirect3DSurface9_LockRect(s, &lr, NULL, 0);
        EXPECT_HR(hr, D3D_OK);
        if (SUCCEEDED(hr)) { CHECK(lr.Pitch >= 64); IDirect3DSurface9_UnlockRect(s); }
        IDirect3DSurface9_Release(s);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_rt_multisample)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    DWORD q = 0;
    HRESULT cm = IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_4_SAMPLES, &q);
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(d.dev, 16, 16, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_4_SAMPLES, 0, FALSE, &s, NULL);
    if (SUCCEEDED(cm)) {
        EXPECT_HR(hr, D3D_OK);
        if (s) {
            D3DSURFACE_DESC dd; IDirect3DSurface9_GetDesc(s, &dd);
            EXPECT_EQ(dd.MultiSampleType, D3DMULTISAMPLE_4_SAMPLES);
            D3DLOCKED_RECT lr;
            EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr, NULL, 0), D3DERR_INVALIDCALL);
            IDirect3DSurface9_Release(s); s = NULL;
            /* lockable multisampled targets are invalid */
            EXPECT_HR(IDirect3DDevice9_CreateRenderTarget(d.dev, 16, 16, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_4_SAMPLES, 0, TRUE, &s, NULL), D3DERR_INVALIDCALL);
            /* quality above the reported level count */
            hr = IDirect3DDevice9_CreateRenderTarget(d.dev, 16, 16, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_4_SAMPLES, q, FALSE, &s, NULL);
            EXPECT_HR(hr, D3DERR_INVALIDCALL);
            SAFE_RELEASE(s);
        }
    } else {
        EXPECT_HR_FAILED(hr);
        SAFE_RELEASE(s);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_rt_set_get)
{
    D9_OPEN(d);
    IDirect3DSurface9 *rt = NULL, *orig = NULL, *got = NULL;
    if (FAILED(mk_rt(&d, 64, 64, D3DFMT_X8R8G8B8, FALSE, &rt))) { t_skip("no rt"); D9_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 0, &orig), D3D_OK);
    ULONG rc0 = d3dh_refcount(rt);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, rt), D3D_OK);
    CHECK(d3dh_refcount(rt) == rc0 + 1);
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 0, &got), D3D_OK);
    CHECK(got == rt);
    SAFE_RELEASE(got);
    D3DVIEWPORT9 vp;
    EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &vp), D3D_OK);
    t_note("viewport after SetRenderTarget: %lux%lu", vp.Width, vp.Height);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, orig), D3D_OK);
    CHECK(d3dh_refcount(rt) == rc0);
    SAFE_RELEASE(orig);
    IDirect3DSurface9_Release(rt);
    D9_END(d);
}

TEST(d3d9_device_resources_rt_set_invalid)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    /* sysmem surface */
    if (SUCCEEDED(mk_off(&d, 64, 64, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &s))) {
        EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, s), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(s);
    }
    /* default-pool plain surface without the render target usage */
    if (SUCCEEDED(mk_off(&d, 64, 64, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &s))) {
        EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, s), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(s);
    }
    D3DCAPS9 c; IDirect3DDevice9_GetDeviceCaps(d.dev, &c);
    if (SUCCEEDED(mk_rt(&d, 64, 64, D3DFMT_X8R8G8B8, FALSE, &s))) {
        EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, c.NumSimultaneousRTs, s), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(s);
    }
    /* clearing a non-zero slot is fine */
    if (c.NumSimultaneousRTs > 1) EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 1, NULL), D3D_OK);
    D9_END(d);
}

/* ---- depth stencil ---- */
TEST(d3d9_device_resources_ds_args)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = (IDirect3DSurface9 *)1;
    HRESULT hr;
    hr = IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 0, 16, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &s, NULL);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    CHECK(s == NULL);
    EXPECT_HR(IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 16, 0, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &s, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 16, 16, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, NULL, NULL), D3DERR_INVALIDCALL);
    D3DFORMAT bad[] = { D3DFMT_UNKNOWN, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_DXT1, D3DFMT_INDEX16 };
    for (unsigned i = 0; i < 5; i++) {
        hr = IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 16, 16, bad[i], D3DMULTISAMPLE_NONE, 0, TRUE, &s, NULL);
        CHECKF(hr == D3DERR_INVALIDCALL || hr == D3DERR_NOTAVAILABLE, "fmt %d got %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_ds_desc_and_set)
{
    D9_OPEN(d);
    D3DFORMAT fm[] = { D3DFMT_D24S8, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D32, D3DFMT_D15S1, D3DFMT_D24X4S4, D3DFMT_D16_LOCKABLE, D3DFMT_D32F_LOCKABLE, D3DFMT_D24FS8 };
    for (unsigned i = 0; i < sizeof fm / sizeof *fm; i++) {
        IDirect3DSurface9 *s = NULL;
        HRESULT hr = IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 32, 32, fm[i], D3DMULTISAMPLE_NONE, 0, FALSE, &s, NULL);
        if (FAILED(hr)) { t_note("ds fmt %d: %s", fm[i], d3dh_hr_name(hr)); continue; }
        D3DSURFACE_DESC dd;
        EXPECT_HR(IDirect3DSurface9_GetDesc(s, &dd), D3D_OK);
        EXPECT_EQ(dd.Format, fm[i]);
        EXPECT_EQ(dd.Usage, D3DUSAGE_DEPTHSTENCIL);
        EXPECT_EQ(dd.Pool, D3DPOOL_DEFAULT);
        EXPECT_EQ(dd.Type, D3DRTYPE_SURFACE);
        IDirect3DSurface9 *got = NULL;
        EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, s), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &got), D3D_OK);
        CHECK(got == s);
        SAFE_RELEASE(got);
        EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, NULL), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &got), D3DERR_NOTFOUND);
        IDirect3DSurface9_Release(s);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_ds_set_invalid)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    if (SUCCEEDED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, FALSE, &s))) {
        EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, s), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(s);
    }
    EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, NULL), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_ds_lock)
{
    D9_OPEN(d);
    IDirect3DSurface9 *s = NULL;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 16, 16, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &s, NULL))) {
        EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr, NULL, 0), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(s);
    }
    if (SUCCEEDED(IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 16, 16, D3DFMT_D16_LOCKABLE, D3DMULTISAMPLE_NONE, 0, FALSE, &s, NULL))) {
        HRESULT hr = IDirect3DSurface9_LockRect(s, &lr, NULL, D3DLOCK_READONLY);
        EXPECT_HR(hr, D3D_OK);
        if (SUCCEEDED(hr)) { CHECK(lr.Pitch >= 32); IDirect3DSurface9_UnlockRect(s); }
        IDirect3DSurface9_Release(s);
    }
    D9_END(d);
}

/* ---- UpdateSurface ---- */
TEST(d3d9_device_resources_updatesurface_args)
{
    D9_OPEN(d);
    IDirect3DSurface9 *src = NULL, *dst = NULL;
    if (FAILED(mk_off(&d, 16, 16, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &src)) || FAILED(mk_off(&d, 16, 16, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dst))) {
        t_skip("surfaces unavailable"); SAFE_RELEASE(src); D9_END(d); return;
    }
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, NULL, NULL, dst, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, NULL, NULL, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, NULL, dst, NULL), D3D_OK);
    RECT sr = { 0, 0, 8, 8 };
    POINT pt = { 8, 8 };
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, &sr, dst, &pt), D3D_OK);
    pt.x = 9;
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, &sr, dst, &pt), D3DERR_INVALIDCALL);
    RECT big = { 0, 0, 17, 8 };
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, &big, dst, NULL), D3DERR_INVALIDCALL);
    RECT neg = { -1, 0, 8, 8 };
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, &neg, dst, NULL), D3DERR_INVALIDCALL);
    /* wrong pool direction */
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, dst, NULL, src, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, dst, NULL, dst, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, NULL, src, NULL), D3DERR_INVALIDCALL);
    IDirect3DSurface9_Release(src);
    IDirect3DSurface9_Release(dst);
    D9_END(d);
}

TEST(d3d9_device_resources_updatesurface_format_mismatch)
{
    D9_OPEN(d);
    IDirect3DSurface9 *src = NULL, *dst = NULL;
    if (FAILED(mk_off(&d, 16, 16, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &src)) || FAILED(mk_off(&d, 16, 16, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &dst))) {
        t_skip("surfaces unavailable"); SAFE_RELEASE(src); D9_END(d); return;
    }
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, NULL, dst, NULL), D3DERR_INVALIDCALL);
    IDirect3DSurface9_Release(src);
    IDirect3DSurface9_Release(dst);
    D9_END(d);
}

TEST(d3d9_device_resources_updatesurface_size_mismatch)
{
    D9_OPEN(d);
    IDirect3DSurface9 *src = NULL, *dst = NULL;
    if (FAILED(mk_off(&d, 16, 16, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &src)) || FAILED(mk_off(&d, 8, 8, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dst))) {
        t_skip("surfaces unavailable"); SAFE_RELEASE(src); D9_END(d); return;
    }
    /* whole-surface copy of a larger source into a smaller dest does not fit */
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, NULL, dst, NULL), D3DERR_INVALIDCALL);
    RECT r = { 0, 0, 8, 8 };
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, &r, dst, NULL), D3D_OK);
    IDirect3DSurface9_Release(src);
    IDirect3DSurface9_Release(dst);
    D9_END(d);
}

TEST(d3d9_device_resources_updatesurface_scratch_src)
{
    D9_OPEN(d);
    IDirect3DSurface9 *src = NULL, *dst = NULL;
    if (FAILED(mk_off(&d, 8, 8, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &src)) || FAILED(mk_off(&d, 8, 8, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dst))) {
        t_skip("surfaces unavailable"); SAFE_RELEASE(src); D9_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_UpdateSurface(d.dev, src, NULL, dst, NULL), D3DERR_INVALIDCALL);
    IDirect3DSurface9_Release(src);
    IDirect3DSurface9_Release(dst);
    D9_END(d);
}

/* ---- UpdateTexture ---- */
TEST(d3d9_device_resources_updatetexture_args)
{
    D9_OPEN(d);
    IDirect3DTexture9 *src = NULL, *dst = NULL, *man = NULL;
    HRESULT h1 = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 3, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &src, NULL);
    HRESULT h2 = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 3, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dst, NULL);
    HRESULT h3 = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 3, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &man, NULL);
    if (FAILED(h1) || FAILED(h2)) { t_skip("textures unavailable"); SAFE_RELEASE(src); SAFE_RELEASE(dst); SAFE_RELEASE(man); D9_END(d); return; }
    IDirect3DBaseTexture9 *bs = (IDirect3DBaseTexture9 *)src, *bd = (IDirect3DBaseTexture9 *)dst;
    EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, NULL, bd), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, bs, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, bs, bd), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, bd, bs), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, bs, bs), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, bd, bd), D3DERR_INVALIDCALL);
    if (SUCCEEDED(h3)) {
        EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, bs, (IDirect3DBaseTexture9 *)man), D3DERR_INVALIDCALL);
        EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, (IDirect3DBaseTexture9 *)man, bd), D3DERR_INVALIDCALL);
    }
    SAFE_RELEASE(src); SAFE_RELEASE(dst); SAFE_RELEASE(man);
    D9_END(d);
}

TEST(d3d9_device_resources_updatetexture_mismatch)
{
    D9_OPEN(d);
    IDirect3DTexture9 *src = NULL, *dst = NULL;
    HRESULT h1 = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &src, NULL);
    if (FAILED(h1)) { t_skip("no sysmem texture"); D9_END(d); return; }
    IDirect3DBaseTexture9 *bs = (IDirect3DBaseTexture9 *)src;
    struct { UINT w, h, lv; D3DFORMAT f; const char *n; } t[] = {
        { 16, 16, 1, D3DFMT_R5G6B5, "format" }, { 32, 32, 1, D3DFMT_A8R8G8B8, "bigger" }, { 8, 8, 1, D3DFMT_A8R8G8B8, "smaller single level" },
    };
    for (unsigned i = 0; i < 3; i++) {
        HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, t[i].w, t[i].h, t[i].lv, 0, t[i].f, D3DPOOL_DEFAULT, &dst, NULL);
        if (FAILED(hr)) continue;
        hr = IDirect3DDevice9_UpdateTexture(d.dev, bs, (IDirect3DBaseTexture9 *)dst);
        CHECKF(hr == D3DERR_INVALIDCALL, "%s got %s", t[i].n, d3dh_hr_name(hr));
        IDirect3DTexture9_Release(dst); dst = NULL;
    }
    /* texture type mismatch */
    IDirect3DCubeTexture9 *cube = NULL;
    if (SUCCEEDED(IDirect3DDevice9_CreateCubeTexture(d.dev, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &cube, NULL))) {
        EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, bs, (IDirect3DBaseTexture9 *)cube), D3DERR_INVALIDCALL);
        IDirect3DCubeTexture9_Release(cube);
    }
    IDirect3DTexture9_Release(src);
    D9_END(d);
}

TEST(d3d9_device_resources_updatetexture_fewer_dst_levels)
{
    D9_OPEN(d);
    IDirect3DTexture9 *src = NULL, *dst = NULL;
    HRESULT h1 = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 5, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &src, NULL);
    HRESULT h2 = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &dst, NULL);
    if (FAILED(h1) || FAILED(h2)) { t_skip("textures unavailable"); SAFE_RELEASE(src); SAFE_RELEASE(dst); D9_END(d); return; }
    HRESULT hr = IDirect3DDevice9_UpdateTexture(d.dev, (IDirect3DBaseTexture9 *)src, (IDirect3DBaseTexture9 *)dst);
    t_note("update 5-level sysmem -> 1-level default (same top size): %s", d3dh_hr_name(hr));
    IDirect3DTexture9_Release(src); IDirect3DTexture9_Release(dst);
    D9_END(d);
}

TEST(d3d9_device_resources_updatetexture_cube_volume)
{
    D9_OPEN(d);
    IDirect3DCubeTexture9 *cs = NULL, *cd = NULL;
    if (SUCCEEDED(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 2, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &cs, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 2, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &cd, NULL)))
        EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, (IDirect3DBaseTexture9 *)cs, (IDirect3DBaseTexture9 *)cd), D3D_OK);
    SAFE_RELEASE(cs); SAFE_RELEASE(cd);
    IDirect3DVolumeTexture9 *vs = NULL, *vd = NULL;
    if (SUCCEEDED(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &vs, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &vd, NULL)))
        EXPECT_HR(IDirect3DDevice9_UpdateTexture(d.dev, (IDirect3DBaseTexture9 *)vs, (IDirect3DBaseTexture9 *)vd), D3D_OK);
    SAFE_RELEASE(vs); SAFE_RELEASE(vd);
    D9_END(d);
}

/* ---- StretchRect ---- */
TEST(d3d9_device_resources_stretchrect_args)
{
    D9_OPEN(d);
    IDirect3DSurface9 *a = NULL, *b = NULL, *sys = NULL;
    if (FAILED(mk_rt(&d, 32, 32, D3DFMT_X8R8G8B8, FALSE, &a)) || FAILED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, FALSE, &b))) {
        t_skip("no rts"); SAFE_RELEASE(a); D9_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, NULL, NULL, b, NULL, D3DTEXF_NONE), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, a, NULL, NULL, NULL, D3DTEXF_NONE), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, a, NULL, b, NULL, D3DTEXF_NONE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, a, NULL, b, NULL, D3DTEXF_POINT), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, a, NULL, b, NULL, D3DTEXF_LINEAR), D3D_OK);
    RECT sr = { 0, 0, 33, 32 };
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, a, &sr, b, NULL, D3DTEXF_NONE), D3DERR_INVALIDCALL);
    RECT dr = { 0, 0, 17, 16 };
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, a, NULL, b, &dr, D3DTEXF_NONE), D3DERR_INVALIDCALL);
    RECT ok = { 4, 4, 12, 12 };
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, a, &ok, b, &ok, D3DTEXF_NONE), D3D_OK);
    /* a surface onto itself with overlapping regions is invalid */
    t_note("StretchRect self: %s", d3dh_hr_name(IDirect3DDevice9_StretchRect(d.dev, a, NULL, a, NULL, D3DTEXF_NONE)));
    if (SUCCEEDED(mk_off(&d, 32, 32, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys))) {
        EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, sys, NULL, b, NULL, D3DTEXF_NONE), D3DERR_INVALIDCALL);
        EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, a, NULL, sys, NULL, D3DTEXF_NONE), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(sys);
    }
    IDirect3DSurface9_Release(a); IDirect3DSurface9_Release(b);
    D9_END(d);
}

TEST(d3d9_device_resources_stretchrect_filter_invalid)
{
    D9_OPEN(d);
    IDirect3DSurface9 *a = NULL, *b = NULL;
    if (FAILED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, FALSE, &a)) || FAILED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, FALSE, &b))) {
        t_skip("no rts"); SAFE_RELEASE(a); D9_END(d); return; }
    HRESULT hr = IDirect3DDevice9_StretchRect(d.dev, a, NULL, b, NULL, D3DTEXF_ANISOTROPIC);
    t_note("StretchRect with ANISOTROPIC filter: %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9_StretchRect(d.dev, a, NULL, b, NULL, (D3DTEXTUREFILTERTYPE)0x7fff);
    t_note("StretchRect with garbage filter: %s", d3dh_hr_name(hr));
    IDirect3DSurface9_Release(a); IDirect3DSurface9_Release(b);
    D9_END(d);
}

TEST(d3d9_device_resources_stretchrect_format_mismatch)
{
    D9_OPEN(d);
    IDirect3DSurface9 *a = NULL, *b = NULL;
    if (FAILED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, FALSE, &a)) || FAILED(mk_rt(&d, 16, 16, D3DFMT_R5G6B5, FALSE, &b))) {
        t_note("rts unavailable"); SAFE_RELEASE(a); D9_END(d); return; }
    HRESULT hr = IDirect3DDevice9_StretchRect(d.dev, a, NULL, b, NULL, D3DTEXF_NONE);
    t_note("StretchRect X8R8G8B8 -> R5G6B5: %s", d3dh_hr_name(hr));
    IDirect3DSurface9_Release(a); IDirect3DSurface9_Release(b);
    D9_END(d);
}

TEST(d3d9_device_resources_stretchrect_depth)
{
    D9_OPEN(d);
    IDirect3DSurface9 *z = NULL, *c = NULL;
    if (FAILED(IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 16, 16, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &z, NULL)) || FAILED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, FALSE, &c))) {
        t_skip("surfaces unavailable"); SAFE_RELEASE(z); D9_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, z, NULL, c, NULL, D3DTEXF_NONE), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_StretchRect(d.dev, c, NULL, z, NULL, D3DTEXF_NONE), D3DERR_INVALIDCALL);
    IDirect3DSurface9_Release(z); IDirect3DSurface9_Release(c);
    D9_END(d);
}

/* ---- ColorFill ---- */
TEST(d3d9_device_resources_colorfill_args)
{
    D9_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, NULL, NULL, 0xffff0000u), D3DERR_INVALIDCALL);
    IDirect3DSurface9 *s = NULL;
    if (SUCCEEDED(mk_off(&d, 16, 16, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &s))) {
        EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, s, NULL, 0xffff0000u), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(s);
    }
    if (SUCCEEDED(mk_off(&d, 16, 16, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &s))) {
        EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, s, NULL, 0xffff0000u), D3D_OK);
        RECT bad = { 0, 0, 17, 16 };
        EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, s, &bad, 0xff00ff00u), D3DERR_INVALIDCALL);
        RECT ok = { 2, 2, 6, 6 };
        EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, s, &ok, 0xff00ff00u), D3D_OK);
        IDirect3DSurface9_Release(s);
    }
    if (SUCCEEDED(IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 16, 16, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &s, NULL))) {
        EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, s, NULL, 0), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(s);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_colorfill_texture_rules)
{
    D9_OPEN(d);
    IDirect3DTexture9 *t = NULL;
    IDirect3DSurface9 *s = NULL;
    /* a default-pool texture level without render-target usage is not fillable */
    if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &t, NULL))) {
        IDirect3DTexture9_GetSurfaceLevel(t, 0, &s);
        HRESULT hr = IDirect3DDevice9_ColorFill(d.dev, s, NULL, 0xff0000ffu);
        t_note("ColorFill on plain default texture level: %s", d3dh_hr_name(hr));
        SAFE_RELEASE(s); IDirect3DTexture9_Release(t); t = NULL;
    }
    if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &t, NULL))) {
        IDirect3DTexture9_GetSurfaceLevel(t, 0, &s);
        EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, s, NULL, 0xff0000ffu), D3D_OK);
        SAFE_RELEASE(s); IDirect3DTexture9_Release(t); t = NULL;
    }
    if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) {
        IDirect3DTexture9_GetSurfaceLevel(t, 0, &s);
        EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, s, NULL, 0xff0000ffu), D3DERR_INVALIDCALL);
        SAFE_RELEASE(s); IDirect3DTexture9_Release(t);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_colorfill_readback)
{
    D9_OPEN(d);
    if (d.type == D3DDEVTYPE_NULLREF) { t_skip("null reference device does not render"); D9_END(d); return; }
    IDirect3DSurface9 *rt = NULL, *sys = NULL;
    if (FAILED(mk_rt(&d, 16, 16, D3DFMT_A8R8G8B8, FALSE, &rt)) || FAILED(mk_off(&d, 16, 16, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sys))) {
        t_skip("surfaces unavailable"); SAFE_RELEASE(rt); D9_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, rt, NULL, 0xff102030u), D3D_OK);
    RECT r = { 4, 4, 8, 8 };
    EXPECT_HR(IDirect3DDevice9_ColorFill(d.dev, rt, &r, 0x80c0b0a0u), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, rt, sys), D3D_OK);
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
        DWORD p0 = *(DWORD *)lr.pBits;
        DWORD p1 = *(DWORD *)((char *)lr.pBits + 5 * lr.Pitch + 5 * 4);
        EXPECT_EQ(p0, 0xff102030u);
        EXPECT_EQ(p1, 0x80c0b0a0u);
        IDirect3DSurface9_UnlockRect(sys);
    }
    IDirect3DSurface9_Release(rt); IDirect3DSurface9_Release(sys);
    D9_END(d);
}

/* ---- GetRenderTargetData / GetFrontBufferData ---- */
TEST(d3d9_device_resources_getrtdata_args)
{
    D9_OPEN(d);
    IDirect3DSurface9 *rt = NULL, *sys = NULL, *sysbig = NULL, *sysfmt = NULL, *def = NULL;
    if (FAILED(mk_rt(&d, 16, 16, D3DFMT_X8R8G8B8, FALSE, &rt)) || FAILED(mk_off(&d, 16, 16, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys))) {
        t_skip("surfaces unavailable"); SAFE_RELEASE(rt); D9_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, NULL, sys), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, rt, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, rt, sys), D3D_OK);
    if (SUCCEEDED(mk_off(&d, 32, 32, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sysbig))) {
        EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, rt, sysbig), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(sysbig);
    }
    if (SUCCEEDED(mk_off(&d, 16, 16, D3DFMT_R5G6B5, D3DPOOL_SYSTEMMEM, &sysfmt))) {
        EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, rt, sysfmt), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(sysfmt);
    }
    if (SUCCEEDED(mk_off(&d, 16, 16, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &def))) {
        EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, rt, def), D3DERR_INVALIDCALL);
        t_note("GetRenderTargetData from a plain default surface: %s", d3dh_hr_name(IDirect3DDevice9_GetRenderTargetData(d.dev, def, sys)));
        IDirect3DSurface9_Release(def);
    }
    /* a system memory source is not a render target */
    EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, sys, sys), D3DERR_INVALIDCALL);
    IDirect3DSurface9_Release(rt); IDirect3DSurface9_Release(sys);
    D9_END(d);
}

TEST(d3d9_device_resources_getrtdata_backbuffer)
{
    D9_OPEN(d);
    IDirect3DSurface9 *bb = NULL, *sys = NULL;
    D3DSURFACE_DESC dd;
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
    if (!bb) { D9_END(d); return; }
    IDirect3DSurface9_GetDesc(bb, &dd);
    if (SUCCEEDED(mk_off(&d, dd.Width, dd.Height, dd.Format, D3DPOOL_SYSTEMMEM, &sys))) {
        EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, bb, sys), D3D_OK);
        IDirect3DSurface9_Release(sys);
    }
    IDirect3DSurface9_Release(bb);
    D9_END(d);
}

TEST(d3d9_device_resources_getfrontbufferdata_args)
{
    D9_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_GetFrontBufferData(d.dev, 0, NULL), D3DERR_INVALIDCALL);
    IDirect3DSurface9 *sys = NULL;
    D3DDISPLAYMODE m;
    IDirect3DDevice9_GetDisplayMode(d.dev, 0, &m);
    if (SUCCEEDED(mk_off(&d, m.Width, m.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sys))) {
        EXPECT_HR(IDirect3DDevice9_GetFrontBufferData(d.dev, 1, sys), D3DERR_INVALIDCALL);
        EXPECT_HR(IDirect3DDevice9_GetFrontBufferData(d.dev, 0xffffffffu, sys), D3DERR_INVALIDCALL);
        IDirect3DSurface9_Release(sys);
    }
    IDirect3DSurface9 *small = NULL;
    if (SUCCEEDED(mk_off(&d, 4, 4, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &small))) {
        HRESULT hr = IDirect3DDevice9_GetFrontBufferData(d.dev, 0, small);
        t_note("GetFrontBufferData into 4x4 surface: %s", d3dh_hr_name(hr));
        IDirect3DSurface9_Release(small);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_swapchain_frontbuffer_args)
{
    D9_OPEN(d);
    IDirect3DSwapChain9 *sc = NULL;
    if (FAILED(IDirect3DDevice9_GetSwapChain(d.dev, 0, &sc))) { t_skip("no swapchain"); D9_END(d); return; }
    EXPECT_HR(IDirect3DSwapChain9_GetFrontBufferData(sc, NULL), D3DERR_INVALIDCALL);
    IDirect3DSurface9 *bb = (IDirect3DSurface9 *)1;
    EXPECT_HR(IDirect3DSwapChain9_GetBackBuffer(sc, 1, D3DBACKBUFFER_TYPE_MONO, &bb), D3DERR_INVALIDCALL);
    CHECK(bb == NULL);
    EXPECT_HR(IDirect3DSwapChain9_GetBackBuffer(sc, 0, D3DBACKBUFFER_TYPE_MONO, NULL), D3DERR_INVALIDCALL);
    D3DRASTER_STATUS rs;
    HRESULT hr = IDirect3DSwapChain9_GetRasterStatus(sc, &rs);
    t_note("swapchain raster status: %s", d3dh_hr_name(hr));
    D3DDISPLAYMODE dm;
    hr = IDirect3DSwapChain9_GetDisplayMode(sc, &dm);
    EXPECT_HR(hr, D3D_OK);
    EXPECT_HR(IDirect3DSwapChain9_GetDisplayMode(sc, NULL), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DSwapChain9_GetPresentParameters(sc, NULL), D3DERR_INVALIDCALL);
    IDirect3DSwapChain9_Release(sc);
    D9_END(d);
}

TEST(d3d9_device_resources_createadditionalswapchain_args)
{
    D9_OPEN(d);
    IDirect3DSwapChain9 *sc = (IDirect3DSwapChain9 *)1;
    D3DPRESENT_PARAMETERS pp; d9_pp(&pp, NULL);
    EXPECT_HR(IDirect3DDevice9_CreateAdditionalSwapChain(d.dev, NULL, &sc), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_CreateAdditionalSwapChain(d.dev, &pp, NULL), D3DERR_INVALIDCALL);
    d9_pp(&pp, d.hwnd);
    HRESULT hr = IDirect3DDevice9_CreateAdditionalSwapChain(d.dev, &pp, &sc);
    if (SUCCEEDED(hr)) {
        EXPECT_EQ(IDirect3DDevice9_GetNumberOfSwapChains(d.dev), 2);
        IDirect3DSwapChain9_Release(sc);
        EXPECT_EQ(IDirect3DDevice9_GetNumberOfSwapChains(d.dev), 1);
    } else t_note("additional swapchain on same window: %s", d3dh_hr_name(hr));
    d9_pp(&pp, d.hwnd); pp.BackBufferCount = 4;
    EXPECT_HR(IDirect3DDevice9_CreateAdditionalSwapChain(d.dev, &pp, &sc), D3DERR_INVALIDCALL);
    D9_END(d);
}
