/* Legacy Direct3D (IDirect3D / 2 / 3 / 7) reached through QueryInterface on DirectDraw objects.
 * The legacy d3d.h declarations clash with the DXGI headers used by the shared helpers, so the few
 * interface layouts needed here are declared locally (vtable slot order is the documented one). */
#include "dl.h"

#define D3DENUMRET_CANCEL 0
#define D3DENUMRET_OK 1
static const GUID L_IID_IDirect3D  = { 0x3BBA0080, 0x2421, 0x11CF, { 0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56 } };
static const GUID L_IID_IDirect3D2 = { 0x6aae1ec1, 0x662a, 0x11d0, { 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a } };
static const GUID L_IID_IDirect3D3 = { 0xbb223240, 0xe72b, 0x11d0, { 0xa9, 0xb4, 0x00, 0xaa, 0x00, 0xc0, 0x99, 0x3e } };
static const GUID L_IID_IDirect3D7 = { 0xf5049e77, 0x4861, 0x11d2, { 0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8 } };
static const GUID L_IID_RGBDevice  = { 0xA4665C60, 0x2673, 0x11CF, { 0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56 } };
static const GUID L_IID_HALDevice  = { 0x84E63dE0, 0x46AA, 0x11CF, { 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E } };

typedef struct { HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *); } L_unk_vtbl;
typedef struct { const L_unk_vtbl *lpVtbl; } L_unk;
typedef HRESULT (WINAPI *L_enumdev7_cb)(LPSTR desc, LPSTR name, void *desc7, LPVOID ctx);
typedef HRESULT (WINAPI *L_enumdev_cb)(GUID *guid, LPSTR desc, LPSTR name, void *hw, void *hel, LPVOID ctx);

/* IDirect3D7: 0 QI, 1 AddRef, 2 Release, 3 EnumDevices, 4 CreateDevice, 5 CreateVertexBuffer, 6 EnumZBufferFormats, 7 EvictManagedTextures */
typedef struct {
    L_unk_vtbl unk;
    HRESULT (WINAPI *EnumDevices)(void *, L_enumdev7_cb, LPVOID);
    HRESULT (WINAPI *CreateDevice)(void *, REFCLSID, IDirectDrawSurface7 *, void **);
    HRESULT (WINAPI *CreateVertexBuffer)(void *, void *, void **, DWORD);
    HRESULT (WINAPI *EnumZBufferFormats)(void *, REFCLSID, void *, LPVOID);
    HRESULT (WINAPI *EvictManagedTextures)(void *);
} L_d3d7_vtbl;
typedef struct { const L_d3d7_vtbl *lpVtbl; } L_d3d7;
/* IDirect3D (v1) : QI, AddRef, Release, Initialize, EnumDevices, CreateLight, CreateMaterial, CreateViewport, FindDevice */
typedef struct {
    L_unk_vtbl unk;
    HRESULT (WINAPI *Initialize)(void *, REFIID);
    HRESULT (WINAPI *EnumDevices)(void *, L_enumdev_cb, LPVOID);
    HRESULT (WINAPI *CreateLight)(void *, void **, IUnknown *);
    HRESULT (WINAPI *CreateMaterial)(void *, void **, IUnknown *);
    HRESULT (WINAPI *CreateViewport)(void *, void **, IUnknown *);
    HRESULT (WINAPI *FindDevice)(void *, void *, void *);
} L_d3d1_vtbl;
typedef struct { const L_d3d1_vtbl *lpVtbl; } L_d3d1;
/* IDirect3DDevice7 slots used */
typedef struct {
    L_unk_vtbl unk;
    HRESULT (WINAPI *GetCaps)(void *, void *);                         /* 3 */
    HRESULT (WINAPI *EnumTextureFormats)(void *, void *, LPVOID);      /* 4 */
    HRESULT (WINAPI *BeginScene)(void *);                              /* 5 */
    HRESULT (WINAPI *EndScene)(void *);                                /* 6 */
    HRESULT (WINAPI *GetDirect3D)(void *, void **);                    /* 7 */
    HRESULT (WINAPI *SetRenderTarget)(void *, IDirectDrawSurface7 *, DWORD); /* 8 */
    HRESULT (WINAPI *GetRenderTarget)(void *, IDirectDrawSurface7 **); /* 9 */
    HRESULT (WINAPI *Clear)(void *, DWORD, RECT *, DWORD, DWORD, float, DWORD); /* 10 */
} L_dev7_vtbl;
typedef struct { const L_dev7_vtbl *lpVtbl; } L_dev7;

static IDirectDraw7 *setup(HWND *w) {
    IDirectDraw7 *dd = dl_create7();
    if (!dd) return NULL;
    *w = dl_window();
    HRESULT hr = IDirectDraw7_SetCooperativeLevel(dd, *w, DDSCL_NORMAL);
    if (FAILED(hr)) { t_skip("SetCooperativeLevel(NORMAL) -> %s", d3dh_hr_name(hr)); IDirectDraw7_Release(dd); dl_window_free(*w); return NULL; }
    return dd;
}
static void teardown(IDirectDraw7 *dd, HWND w) { IDirectDraw7_Release(dd); dl_window_free(w); }

static L_d3d7 *get_d3d7(IDirectDraw7 *dd) {
    L_d3d7 *d3d = NULL;
    HRESULT hr = IDirectDraw7_QueryInterface(dd, &L_IID_IDirect3D7, (void **)&d3d);
    if (FAILED(hr) || !d3d) { t_skip("IDirect3D7 not available (%s): the %s-bit ddraw %s legacy Direct3D (d3dim*.dll exist in SysWOW64 only); run ddraw_legacy_x86", d3dh_hr_name(hr), DL_X64 ? "64" : "32", DL_X64 ? "has no" : "unexpectedly lacks"); return NULL; }
    return d3d;
}

TEST(ddraw_legacy_d3d_qi_each_version) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw2 *dd2 = NULL; IDirectDraw4 *dd4 = NULL; IDirectDraw7 *dd7 = NULL;
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw2, (void **)&dd2);
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd4);
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw7, (void **)&dd7);
    struct { void *obj; const GUID *iid; const char *n; } c[] = {
        { dd1, &L_IID_IDirect3D, "IDirect3D from IDirectDraw" }, { dd2, &L_IID_IDirect3D2, "IDirect3D2 from IDirectDraw2" },
        { dd4, &L_IID_IDirect3D3, "IDirect3D3 from IDirectDraw4" }, { dd7, &L_IID_IDirect3D7, "IDirect3D7 from IDirectDraw7" },
    };
    int any = 0;
    for (unsigned i = 0; i < 4; i++) {
        if (!c[i].obj) continue;
        L_unk *q = NULL;
        HRESULT hr = ((IUnknown *)c[i].obj)->lpVtbl->QueryInterface((IUnknown *)c[i].obj, c[i].iid, (void **)&q);
        /* x64: no legacy Direct3D at all. x86: v1/v2/v3 are available from every DirectDraw interface; IDirect3D7 is NOT available from an IDirectDraw7
         * obtained by QI from a DirectDrawCreate (v1) object (only from a DirectDrawCreateEx object) */
        HRESULT want = (!DL_X64 && i < 3) ? S_OK : E_NOINTERFACE;
        CHECKF(hr == want, "%s -> %s", c[i].n, d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && q) { any = 1; q->lpVtbl->Release(q); }
    }
    if (dd2) IDirectDraw2_Release(dd2);
    if (dd4) IDirectDraw4_Release(dd4);
    if (dd7) IDirectDraw7_Release(dd7);
    IDirectDraw_Release(dd1);
}
TEST(ddraw_legacy_d3d_mismatched_version_iids) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw7 *dd7 = NULL;
    if (FAILED(IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw7, (void **)&dd7)) || !dd7) { IDirectDraw_Release(dd1); return; }
    L_unk *q = NULL;
    HRESULT hr = IDirectDraw7_QueryInterface(dd7, &L_IID_IDirect3D2, (void **)&q);
    EXPECT_HR(hr, DL_X64 ? E_NOINTERFACE : S_OK);   /* x86: older d3d versions are reachable from any DirectDraw interface */
    if (SUCCEEDED(hr) && q) q->lpVtbl->Release(q);
    IDirectDraw7_Release(dd7);
    IDirectDraw_Release(dd1);
}
TEST(ddraw_legacy_d3d7_refcount_symmetry) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (d3d) {
        ULONG a = d3d->lpVtbl->unk.AddRef(d3d);
        ULONG b = d3d->lpVtbl->unk.AddRef(d3d);
        CHECK(b == a + 1);
        ULONG c = d3d->lpVtbl->unk.Release(d3d);
        CHECK(c == a);
        d3d->lpVtbl->unk.Release(d3d);
        d3d->lpVtbl->unk.Release(d3d);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_d3d7_qi_back_to_ddraw) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (d3d) {
        void *x = NULL;
        HRESULT hr = d3d->lpVtbl->unk.QueryInterface(d3d, &IID_IDirectDraw7, &x);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr) && x) ((IUnknown *)x)->lpVtbl->Release((IUnknown *)x);
        hr = d3d->lpVtbl->unk.QueryInterface(d3d, &IID_IUnknown, &x);
        CHECKF(hr == S_OK, "IUnknown -> %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && x) ((IUnknown *)x)->lpVtbl->Release((IUnknown *)x);
        d3d->lpVtbl->unk.Release(d3d);
    }
    teardown(dd, w);
}

struct devs { int n; char names[8][64]; int has_rgb; };
static HRESULT WINAPI enum7_cb(LPSTR desc, LPSTR name, void *d7, LPVOID ctx) {
    struct devs *d = ctx;
    if (d->n < 8) lstrcpynA(d->names[d->n], name ? name : "(null)", 64);
    d->n++;
    return D3DENUMRET_OK;
}
TEST(ddraw_legacy_d3d7_enum_devices) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (d3d) {
        struct devs d; memset(&d, 0, sizeof d);
        HRESULT hr = d3d->lpVtbl->EnumDevices(d3d, enum7_cb, &d);
        EXPECT_HR(hr, DD_OK);
        EXPECT_EQ(d.n, 3);   /* Win10 x86, Basic Display Adapter: software RGB plus the two HAL devices */
        if (d.n == 3) { EXPECT_STR(d.names[0], "RGB Emulation"); EXPECT_STR(d.names[1], "Direct3D HAL"); EXPECT_STR(d.names[2], "Direct3D T&L HAL"); }
        d3d->lpVtbl->unk.Release(d3d);
    }
    teardown(dd, w);
}
static HRESULT WINAPI enum7_stop(LPSTR desc, LPSTR name, void *d7, LPVOID ctx) { (*(int *)ctx)++; return D3DENUMRET_CANCEL; }
TEST(ddraw_legacy_d3d7_enum_devices_cancel) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (d3d) {
        int n = 0;
        EXPECT_HR(d3d->lpVtbl->EnumDevices(d3d, enum7_stop, &n), DD_OK);
        EXPECT_EQ(n, 1);
        d3d->lpVtbl->unk.Release(d3d);
    }
    teardown(dd, w);
}
static HRESULT WINAPI enum1_cb(GUID *g, LPSTR desc, LPSTR name, void *hw, void *hel, LPVOID ctx) {
    struct devs *d = ctx;
    if (d->n < 8) lstrcpynA(d->names[d->n], name ? name : "(null)", 64);
    if (g && !memcmp(g, &L_IID_RGBDevice, sizeof(GUID))) d->has_rgb = 1;
    d->n++;
    return D3DENUMRET_OK;
}
TEST(ddraw_legacy_d3d1_enum_devices) {
    DL_SKIP_IF_NO_DD1(dd1);
    L_d3d1 *d3d = NULL;
    HRESULT hr = IDirectDraw_QueryInterface(dd1, &L_IID_IDirect3D, (void **)&d3d);
    if (FAILED(hr) || !d3d) { t_skip("IDirect3D not available (%s): 64-bit ddraw has no legacy Direct3D; run ddraw_legacy_x86", d3dh_hr_name(hr)); IDirectDraw_Release(dd1); return; }
    struct devs d; memset(&d, 0, sizeof d);
    hr = d3d->lpVtbl->EnumDevices(d3d, enum1_cb, &d);
    EXPECT_HR(hr, DD_OK);
    EXPECT_EQ(d.n, 3);
    CHECK(d.has_rgb);
    if (d.n == 3) { EXPECT_STR(d.names[0], "Ramp Emulation"); EXPECT_STR(d.names[1], "RGB Emulation"); EXPECT_STR(d.names[2], "Direct3D HAL"); }
    d3d->lpVtbl->unk.Release(d3d);
    IDirectDraw_Release(dd1);
}

static IDirectDrawSurface7 *mk_target(IDirectDraw7 *dd, DWORD w, DWORD h) {
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    d.dwWidth = w; d.dwHeight = h;
    d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | DDSCAPS_SYSTEMMEMORY;
    dl_pf_xrgb32(&d.ddpfPixelFormat);
    IDirectDrawSurface7 *s = NULL;
    if (FAILED(IDirectDraw7_CreateSurface(dd, &d, &s, NULL))) return NULL;
    return s;
}
static L_dev7 *mk_device(L_d3d7 *d3d, IDirectDrawSurface7 *rt, HRESULT *hr_out) {
    L_dev7 *dev = NULL;
    HRESULT hr = d3d->lpVtbl->CreateDevice(d3d, &L_IID_RGBDevice, rt, (void **)&dev);
    if (hr_out) *hr_out = hr;
    return SUCCEEDED(hr) ? dev : NULL;
}
TEST(ddraw_legacy_d3d7_rgb_device_clear) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (!d3d) { teardown(dd, w); return; }
    IDirectDrawSurface7 *rt = mk_target(dd, 32, 32);
    if (!rt) { t_skip("no 3D-capable system-memory surface"); d3d->lpVtbl->unk.Release(d3d); teardown(dd, w); return; }
    HRESULT hr; L_dev7 *dev = mk_device(d3d, rt, &hr);
    if (!dev) { t_skip("RGB device creation -> %s", d3dh_hr_name(hr)); IDirectDrawSurface7_Release(rt); d3d->lpVtbl->unk.Release(d3d); teardown(dd, w); return; }
    EXPECT_HR(dev->lpVtbl->BeginScene(dev), D3D_OK);
    EXPECT_HR(dev->lpVtbl->Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0x00336699, 1.0f, 0), D3D_OK);
    EXPECT_HR(dev->lpVtbl->EndScene(dev), D3D_OK);
    DDSURFACEDESC2 l; memset(&l, 0, sizeof l); l.dwSize = sizeof l;
    if (SUCCEEDED(IDirectDrawSurface7_Lock(rt, NULL, &l, DDLOCK_WAIT | DDLOCK_READONLY, NULL))) {
        int bad = 0;
        for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++)
            if ((*(DWORD *)((BYTE *)l.lpSurface + y * l.lPitch + x * 4) & 0xffffff) != 0x336699) bad++;
        IDirectDrawSurface7_Unlock(rt, NULL);
        EXPECT_EQ(bad, 0);
    }
    IDirectDrawSurface7 *got = NULL;
    EXPECT_HR(dev->lpVtbl->GetRenderTarget(dev, &got), DD_OK);
    CHECK(got == rt);
    if (got) IDirectDrawSurface7_Release(got);
    dev->lpVtbl->unk.Release(dev);
    IDirectDrawSurface7_Release(rt);
    d3d->lpVtbl->unk.Release(d3d);
    teardown(dd, w);
}
TEST(ddraw_legacy_d3d7_clear_rect) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (!d3d) { teardown(dd, w); return; }
    IDirectDrawSurface7 *rt = mk_target(dd, 16, 16);
    HRESULT hr; L_dev7 *dev = rt ? mk_device(d3d, rt, &hr) : NULL;
    if (!dev) { t_skip("RGB device unavailable"); if (rt) IDirectDrawSurface7_Release(rt); d3d->lpVtbl->unk.Release(d3d); teardown(dd, w); return; }
    DDBLTFX fx; memset(&fx, 0, sizeof fx); fx.dwSize = sizeof fx;
    IDirectDrawSurface7_Blt(rt, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    RECT r = { 4, 4, 8, 8 };
    EXPECT_HR(dev->lpVtbl->BeginScene(dev), D3D_OK);
    EXPECT_HR(dev->lpVtbl->Clear(dev, 1, &r, D3DCLEAR_TARGET, 0x00ffffff, 1.0f, 0), D3D_OK);
    EXPECT_HR(dev->lpVtbl->EndScene(dev), D3D_OK);
    DDSURFACEDESC2 l; memset(&l, 0, sizeof l); l.dwSize = sizeof l;
    if (SUCCEEDED(IDirectDrawSurface7_Lock(rt, NULL, &l, DDLOCK_WAIT | DDLOCK_READONLY, NULL))) {
        int bad = 0;
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            int in = x >= 4 && x < 8 && y >= 4 && y < 8;
            DWORD v = *(DWORD *)((BYTE *)l.lpSurface + y * l.lPitch + x * 4) & 0xffffff;
            if (v != (in ? 0xffffffu : 0u)) bad++;
        }
        IDirectDrawSurface7_Unlock(rt, NULL);
        EXPECT_EQ(bad, 0);
    }
    dev->lpVtbl->unk.Release(dev);
    IDirectDrawSurface7_Release(rt);
    d3d->lpVtbl->unk.Release(d3d);
    teardown(dd, w);
}
TEST(ddraw_legacy_d3d7_create_device_needs_3ddevice_surface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (!d3d) { teardown(dd, w); return; }
    IDirectDrawSurface7 *plain = dl_offscreen7(dd, 16, 16, DDSCAPS_SYSTEMMEMORY, NULL);
    if (plain) {
        L_dev7 *dev = NULL;
        HRESULT hr = d3d->lpVtbl->CreateDevice(d3d, &L_IID_RGBDevice, plain, (void **)&dev);
        EXPECT_HR(hr, DDERR_INVALIDCAPS);   /* device on a surface without DDSCAPS_3DDEVICE */
        if (SUCCEEDED(hr) && dev) dev->lpVtbl->unk.Release(dev);
        IDirectDrawSurface7_Release(plain);
    }
    d3d->lpVtbl->unk.Release(d3d);
    teardown(dd, w);
}
TEST(ddraw_legacy_d3d7_create_device_unknown_guid) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (!d3d) { teardown(dd, w); return; }
    IDirectDrawSurface7 *rt = mk_target(dd, 16, 16);
    if (rt) {
        GUID bad = { 0x12345678, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
        L_dev7 *dev = NULL;
        HRESULT hr = d3d->lpVtbl->CreateDevice(d3d, &bad, rt, (void **)&dev);
        EXPECT_HR(hr, E_INVALIDARG);   /* unknown device GUID */
        if (SUCCEEDED(hr) && dev) dev->lpVtbl->unk.Release(dev);
        IDirectDrawSurface7_Release(rt);
    } else t_skip("no 3D surface");
    d3d->lpVtbl->unk.Release(d3d);
    teardown(dd, w);
}
TEST(ddraw_legacy_d3d7_hal_device_on_sysmem_target) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (!d3d) { teardown(dd, w); return; }
    IDirectDrawSurface7 *rt = mk_target(dd, 16, 16);
    if (rt) {
        L_dev7 *dev = NULL;
        HRESULT hr = d3d->lpVtbl->CreateDevice(d3d, &L_IID_HALDevice, rt, (void **)&dev);
        EXPECT_HR(hr, (HRESULT)0x887602eb);   /* Win10 x86, Basic Display Adapter: the HAL device needs a video-memory target (D3DERR_SURFACENOTINVIDMEM) */
        if (SUCCEEDED(hr) && dev) dev->lpVtbl->unk.Release(dev);
        IDirectDrawSurface7_Release(rt);
    } else t_skip("no 3D surface");
    d3d->lpVtbl->unk.Release(d3d);
    teardown(dd, w);
}
static HRESULT WINAPI zfmt_cb(void *pf, LPVOID ctx) { (*(int *)ctx)++; return D3DENUMRET_OK; }
TEST(ddraw_legacy_d3d7_enum_zbuffer_formats) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (d3d) {
        int n = 0;
        HRESULT hr = d3d->lpVtbl->EnumZBufferFormats(d3d, &L_IID_RGBDevice, (void *)zfmt_cb, &n);
        EXPECT_HR(hr, DD_OK); EXPECT_EQ(n, 2);   /* Win10 x86: two z-buffer formats for the RGB device */
        d3d->lpVtbl->unk.Release(d3d);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_d3d7_evict_managed_textures) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (d3d) {
        EXPECT_HR(d3d->lpVtbl->EvictManagedTextures(d3d), DD_OK);
        d3d->lpVtbl->unk.Release(d3d);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_d3d7_texture_surface_create) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    L_d3d7 *d3d = get_d3d7(dd);
    if (!d3d) { teardown(dd, w); return; }
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    d.dwWidth = d.dwHeight = 16; d.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
    dl_pf_xrgb32(&d.ddpfPixelFormat); d.ddpfPixelFormat.dwFlags |= DDPF_ALPHAPIXELS; d.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
    IDirectDrawSurface7 *t = NULL;
    HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &t, NULL);
    EXPECT_HR(hr, DD_OK);
    if (SUCCEEDED(hr) && t) {
        DDSURFACEDESC2 g; memset(&g, 0, sizeof g); g.dwSize = sizeof g;
        EXPECT_HR(IDirectDrawSurface7_GetSurfaceDesc(t, &g), DD_OK);
        CHECK(g.ddsCaps.dwCaps & DDSCAPS_TEXTURE);
        IDirectDrawSurface7_Release(t);
    }
    d3d->lpVtbl->unk.Release(d3d);
    teardown(dd, w);
}
