/* additional depth for resources on a HAL device: render + readback, clear validation, priority on surfaces,
 * adapter format/depth/multisample capability matrices, typed private-data entry points, volumes, threading */
#include "d9r.h"

static int more_hal(D9 *d)
{
    if (d->type == D3DDEVTYPE_NULLREF) { t_skip("null reference device does not render"); return 0; }
    return 1;
}

TEST(d3d9_device_resources_more_clear_present_readback)
{
    D9_OPEN(d);
    if (!more_hal(&d)) { D9_END(d); return; }
    IDirect3DSurface9 *bb = NULL, *sys = NULL;
    D3DSURFACE_DESC sd;
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
    if (!bb) { D9_END(d); return; }
    IDirect3DSurface9_GetDesc(bb, &sd);
    if (FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(d.dev, sd.Width, sd.Height, sd.Format, D3DPOOL_SYSTEMMEM, &sys, NULL))) { t_skip("no sysmem surface"); SAFE_RELEASE(bb); D9_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff336699u, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, bb, sys), D3D_OK);
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
        int bad = 0;
        for (UINT y = 0; y < sd.Height; y++) for (UINT x = 0; x < sd.Width; x++) if ((((DWORD *)((char *)lr.pBits + y * lr.Pitch))[x] & 0xffffff) != 0x336699) bad++;
        CHECKF(bad == 0, "%d of %ux%u pixels are not the clear colour", bad, sd.Width, sd.Height);
        IDirect3DSurface9_UnlockRect(sys);
    }
    /* a sub-rectangle clear touches only the rectangle */
    D3DRECT rc = { 8, 8, 24, 24 };
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 1, &rc, D3DCLEAR_TARGET, 0xffcc0000u, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderTargetData(d.dev, bb, sys), D3D_OK);
    if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
        DWORD in = ((DWORD *)((char *)lr.pBits + 10 * lr.Pitch))[10] & 0xffffff, out = ((DWORD *)((char *)lr.pBits + 4 * lr.Pitch))[4] & 0xffffff, edge = ((DWORD *)((char *)lr.pBits + 24 * lr.Pitch))[24] & 0xffffff;
        EXPECT_EQ(in, 0xcc0000); EXPECT_EQ(out, 0x336699); EXPECT_EQ(edge, 0x336699);   /* the rectangle is half open */
        IDirect3DSurface9_UnlockRect(sys);
    }
    EXPECT_HR(IDirect3DDevice9_Present(d.dev, NULL, NULL, NULL, NULL), D3D_OK);
    SAFE_RELEASE(sys); SAFE_RELEASE(bb);
    D9_END(d);
}

TEST(d3d9_device_resources_more_clear_validation)
{
    D9_OPEN(d);
    if (!more_hal(&d)) { D9_END(d); return; }
    D3DRECT rc = { 0, 0, 8, 8 };
    /* no depth buffer on this device: clearing depth/stencil is invalid */
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, 0, 0, 1.0f, 0), D3DERR_INVALIDCALL);   /* Win10: no flags is an error */
    /* Win10 retail: a count with a NULL rectangle array, a rectangle array with count 0, inverted and oversized rectangles are all accepted */
    EXPECT_HRN(IDirect3DDevice9_Clear(d.dev, 1, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, &rc, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, 0x80000000u, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    D3DRECT bad = { 20, 20, 10, 10 };
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 1, &bad, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    D3DRECT huge = { -10, -10, 1000, 1000 };
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 1, &huge, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    /* scene state machine */
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_more_surface_priority)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL; IDirect3DSurface9 *lvl = NULL, *off = NULL, *rt = NULL;
    if (FAILED(IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 2, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL))) { t_skip("no managed texture"); D9_END(d); return; }
    IDirect3DTexture9_GetSurfaceLevel(tex, 0, &lvl);
    IDirect3DDevice9_CreateOffscreenPlainSurface(d.dev, 8, 8, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &off, NULL);
    IDirect3DDevice9_CreateRenderTarget(d.dev, 8, 8, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL);
    struct { IDirect3DSurface9 *s; const char *n; } t[] = { { lvl, "managed level" }, { off, "sysmem offscreen" }, { rt, "render target" } };
    for (int i = 0; i < 3; i++) {
        if (!t[i].s) continue;
        DWORD p0 = IDirect3DSurface9_GetPriority(t[i].s), prev = IDirect3DSurface9_SetPriority(t[i].s, 7), p1 = IDirect3DSurface9_GetPriority(t[i].s);
        t_note("%s: priority get %lu, set(7) returned %lu, get %lu", t[i].n, (unsigned long)p0, (unsigned long)prev, (unsigned long)p1);
        IDirect3DSurface9_PreLoad(t[i].s);
    }
    /* a level shares the priority of its texture */
    if (lvl) {
        IDirect3DTexture9_SetPriority(tex, 11);
        t_note("texture priority 11, level priority %lu", (unsigned long)IDirect3DSurface9_GetPriority(lvl));
    }
    SAFE_RELEASE(rt); SAFE_RELEASE(off); SAFE_RELEASE(lvl); IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_more_check_depth_stencil_match)
{
    D9_OPEN(d);
    static const D3DFORMAT ds[] = { D3DFMT_D16, D3DFMT_D32, D3DFMT_D24S8, D3DFMT_D24X8, D3DFMT_D15S1, D3DFMT_D24X4S4, D3DFMT_D16_LOCKABLE, D3DFMT_D32F_LOCKABLE, D3DFMT_D24FS8, D3DFMT_A8R8G8B8 };
    static const char *const nm[] = { "D16", "D32", "D24S8", "D24X8", "D15S1", "D24X4S4", "D16_LOCKABLE", "D32F_LOCKABLE", "D24FS8", "A8R8G8B8" };
    for (unsigned i = 0; i < sizeof ds / sizeof *ds; i++) {
        HRESULT hr = IDirect3D9_CheckDepthStencilMatch(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, ds[i]);
        t_note("CheckDepthStencilMatch(X8R8G8B8 adapter/rt, %s) = %s", nm[i], d3dh_hr_name(hr));
        /* a colour format is never a depth format */
        if (ds[i] == D3DFMT_A8R8G8B8) CHECK(FAILED(hr));
    }
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d.d3d, 99, d.type, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_D24S8), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d.d3d, 0, (D3DDEVTYPE)0, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_D24S8), D3DERR_INVALIDCALL);   /* unlike GetDeviceCaps (INVALIDDEVICE) */
    D9_END(d);
}

TEST(d3d9_device_resources_more_check_multisample_matrix)
{
    D9_OPEN(d);
    static const D3DFORMAT fm[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_D24S8, D3DFMT_D16 };
    static const char *const nm[] = { "X8R8G8B8", "A8R8G8B8", "R5G6B5", "D24S8", "D16" };
    for (unsigned i = 0; i < sizeof fm / sizeof *fm; i++) {
        char line[256] = ""; DWORD q;
        for (int ms = 0; ms <= 16; ms++) {
            q = 0xdead;
            HRESULT hr = IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, fm[i], TRUE, (D3DMULTISAMPLE_TYPE)ms, &q);
            if (SUCCEEDED(hr)) sprintf(line + strlen(line), " %d(q=%lu)", ms, (unsigned long)q);
        }
        t_note("multisample types accepted windowed for %s:%s", nm[i], line);
    }
    /* NONE is always available for a renderable format, with one quality level */
    DWORD q = 0;
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, &q), D3D_OK);
    EXPECT_EQ(q, 1);
    t_note("MSAA NONMASKABLE on X8R8G8B8: %s q=%lu", d3dh_hr_name(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONMASKABLE, &q)), (unsigned long)q);   /* Basic Render Driver: accepted */
    t_note("MSAA with NULL quality pointer: %s", d3dh_hr_name(IDirect3D9_CheckDeviceMultiSampleType(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, NULL)));
    D9_END(d);
}

TEST(d3d9_device_resources_more_check_format_conversion)
{
    D9_OPEN(d);
    static const D3DFORMAT src[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A16B16G16R16F, D3DFMT_DXT1, D3DFMT_UNKNOWN };
    static const char *const nm[] = { "A8R8G8B8", "X8R8G8B8", "R5G6B5", "A16B16G16R16F", "DXT1", "UNKNOWN" };
    for (unsigned i = 0; i < sizeof src / sizeof *src; i++)
        t_note("CheckDeviceFormatConversion(%s -> X8R8G8B8) = %s", nm[i], d3dh_hr_name(IDirect3D9_CheckDeviceFormatConversion(d.d3d, 0, d.type, src[i], D3DFMT_X8R8G8B8)));
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8), D3D_OK);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d.d3d, 99, d.type, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_more_typed_private_data)
{
    D9_OPEN(d);
    IDirect3DTexture9 *t = NULL; IDirect3DCubeTexture9 *c = NULL; IDirect3DVolumeTexture9 *v = NULL; IDirect3DVertexBuffer9 *vb = NULL; IDirect3DIndexBuffer9 *ib = NULL;
    IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL);
    IDirect3DDevice9_CreateCubeTexture(d.dev, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &c, NULL);
    IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v, NULL);
    IDirect3DDevice9_CreateVertexBuffer(d.dev, 64, 0, 0, D3DPOOL_MANAGED, &vb, NULL);
    IDirect3DDevice9_CreateIndexBuffer(d.dev, 64, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib, NULL);
    static const GUID g = { 0x1234, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };
    BYTE in[5] = { 1, 2, 3, 4, 5 }, out[8]; DWORD sz;
#define PD(obj, T) do { if (obj) { \
        EXPECT_HR(IDirect3D##T##9_SetPrivateData(obj, &g, in, 5, 0), D3D_OK); sz = 8; memset(out, 0, 8); \
        EXPECT_HR(IDirect3D##T##9_GetPrivateData(obj, &g, out, &sz), D3D_OK); EXPECT_EQ(sz, 5); CHECK(!memcmp(in, out, 5)); \
        sz = 2; EXPECT_HR(IDirect3D##T##9_GetPrivateData(obj, &g, out, &sz), D3DERR_MOREDATA); EXPECT_EQ(sz, 5); \
        EXPECT_HR(IDirect3D##T##9_FreePrivateData(obj, &g), D3D_OK); EXPECT_HR(IDirect3D##T##9_FreePrivateData(obj, &g), D3DERR_NOTFOUND); \
        sz = 8; EXPECT_HR(IDirect3D##T##9_GetPrivateData(obj, &g, out, &sz), D3DERR_NOTFOUND); } } while (0)
    PD(t, Texture); PD(c, CubeTexture); PD(v, VolumeTexture); PD(vb, VertexBuffer); PD(ib, IndexBuffer);
    /* the same GUID on two different objects is independent */
    if (t && c) {
        EXPECT_HR(IDirect3DTexture9_SetPrivateData(t, &g, in, 5, 0), D3D_OK);
        sz = 8; EXPECT_HR(IDirect3DCubeTexture9_GetPrivateData(c, &g, out, &sz), D3DERR_NOTFOUND);
    }
    /* a volume level has a store of its own, separate from its container */
    if (v) {
        IDirect3DVolume9 *lvl = NULL;
        if (SUCCEEDED(IDirect3DVolumeTexture9_GetVolumeLevel(v, 0, &lvl))) {
            IDirect3DDevice9 *dev = NULL;
            EXPECT_HR(IDirect3DVolume9_SetPrivateData(lvl, &g, in, 5, 0), D3D_OK);
            sz = 8; EXPECT_HR(IDirect3DVolumeTexture9_GetPrivateData(v, &g, out, &sz), D3DERR_NOTFOUND);
            sz = 8; EXPECT_HR(IDirect3DVolume9_GetPrivateData(lvl, &g, out, &sz), D3D_OK);
            EXPECT_HR(IDirect3DVolume9_FreePrivateData(lvl, &g), D3D_OK);
            EXPECT_HR(IDirect3DVolume9_GetDevice(lvl, &dev), D3D_OK); CHECK(dev == d.dev); SAFE_RELEASE(dev);
            IDirect3DVolume9_Release(lvl);
        }
    }
    SAFE_RELEASE(ib); SAFE_RELEASE(vb); if (v) IDirect3DVolumeTexture9_Release(v); if (c) IDirect3DCubeTexture9_Release(c); if (t) IDirect3DTexture9_Release(t);
    D9_END(d);
}

/* ---- threading: a MULTITHREADED device creating and destroying resources from several threads ----------- */
typedef struct { IDirect3DDevice9 *dev; volatile LONG fails; int rounds; } MtCtx;
static DWORD WINAPI mt_worker(LPVOID p)
{
    MtCtx *c = p;
    for (int i = 0; i < c->rounds; i++) {
        IDirect3DTexture9 *t = NULL; IDirect3DVertexBuffer9 *vb = NULL; D3DLOCKED_RECT lr; void *vp;
        if (FAILED(IDirect3DDevice9_CreateTexture(c->dev, 8 + (i & 7), 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) { InterlockedIncrement(&c->fails); continue; }
        if (SUCCEEDED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, 0))) { *(DWORD *)lr.pBits = 0xabcd0000u + i; if (*(DWORD *)lr.pBits != 0xabcd0000u + (DWORD)i) InterlockedIncrement(&c->fails); IDirect3DTexture9_UnlockRect(t, 0); } else InterlockedIncrement(&c->fails);
        if (SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(c->dev, 256, 0, 0, D3DPOOL_MANAGED, &vb, NULL))) {
            if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(vb, 0, 0, &vp, 0))) { memset(vp, i, 256); IDirect3DVertexBuffer9_Unlock(vb); } else InterlockedIncrement(&c->fails);
            IDirect3DVertexBuffer9_Release(vb);
        } else InterlockedIncrement(&c->fails);
        if (IDirect3DTexture9_Release(t) != 0) InterlockedIncrement(&c->fails);
    }
    return 0;
}
TEST(d3d9_device_resources_more_multithreaded_resource_churn)
{
    D9_OPEN(d);
    if (!more_hal(&d)) { D9_END(d); return; }
    IDirect3DDevice9 *dev = NULL; D3DPRESENT_PARAMETERS pp; d9_pp(&pp, d.hwnd);
    HRESULT hr = IDirect3D9_CreateDevice(d.d3d, 0, d.type, d.hwnd, SWVP | D3DCREATE_MULTITHREADED, &pp, &dev);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) {
        MtCtx c = { dev, 0, 150 }; HANDLE th[4];
        for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, mt_worker, &c, 0, NULL);
        for (int i = 0; i < 4; i++) { WaitForSingleObject(th[i], 60000); CloseHandle(th[i]); }
        EXPECT_EQ(c.fails, 0);
        IDirect3DDevice9_Release(dev);
    }
    D9_END(d);
}
