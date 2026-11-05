/* private data, priorities and misc resource-interface behaviour across every resource kind */
#include "d9r.h"

typedef struct { IUnknownVtbl *vtbl; LONG ref; } Fake;
static HRESULT WINAPI fk_qi(IUnknown *u, REFIID r, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI fk_addref(IUnknown *u) { return InterlockedIncrement(&((Fake *)u)->ref); }
static ULONG WINAPI fk_release(IUnknown *u) { return InterlockedDecrement(&((Fake *)u)->ref); }
static IUnknownVtbl fk_vtbl = { fk_qi, fk_addref, fk_release };
static void fk_init(Fake *f) { f->vtbl = &fk_vtbl; f->ref = 1; }

static const GUID G1 = { 0x11111111, 0x1111, 0x1111, { 1, 2, 3, 4, 5, 6, 7, 8 } };
static const GUID G2 = { 0x22222222, 0x2222, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };

enum { K_TEX, K_CUBE, K_VOL, K_VB, K_IB, K_SURF_SYS, K_RT, K_TEX_DEF, K_COUNT };
static const char *kname[] = { "tex", "cube", "voltex", "vb", "ib", "sysmem surface", "rt", "default tex" };

static IDirect3DResource9 *mkres(D9 *d, int k)
{
    IDirect3DResource9 *r = NULL;
    switch (k) {
    case K_TEX: { IDirect3DTexture9 *t; if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d->dev, 8, 8, 2, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) r = (IDirect3DResource9 *)t; break; }
    case K_CUBE: { IDirect3DCubeTexture9 *t; if (SUCCEEDED(IDirect3DDevice9_CreateCubeTexture(d->dev, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) r = (IDirect3DResource9 *)t; break; }
    case K_VOL: { IDirect3DVolumeTexture9 *t; if (SUCCEEDED(IDirect3DDevice9_CreateVolumeTexture(d->dev, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) r = (IDirect3DResource9 *)t; break; }
    case K_VB: { IDirect3DVertexBuffer9 *t; if (SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(d->dev, 64, 0, 0, D3DPOOL_MANAGED, &t, NULL))) r = (IDirect3DResource9 *)t; break; }
    case K_IB: { IDirect3DIndexBuffer9 *t; if (SUCCEEDED(IDirect3DDevice9_CreateIndexBuffer(d->dev, 64, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &t, NULL))) r = (IDirect3DResource9 *)t; break; }
    case K_SURF_SYS: { IDirect3DSurface9 *t; if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(d->dev, 8, 8, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &t, NULL))) r = (IDirect3DResource9 *)t; break; }
    case K_RT: { IDirect3DSurface9 *t; if (SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(d->dev, 8, 8, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &t, NULL))) r = (IDirect3DResource9 *)t; break; }
    case K_TEX_DEF: { IDirect3DTexture9 *t; if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d->dev, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t, NULL))) r = (IDirect3DResource9 *)t; break; }
    }
    return r;
}


TEST(d3d9_device_resources_pd_roundtrip)
{
    D9_OPEN(d);
    for (int k = 0; k < K_COUNT; k++) {
        IDirect3DResource9 *r = mkres(&d, k);
        if (!r) { t_note("kind %s unavailable", kname[k]); continue; }
        BYTE in[7] = { 1, 2, 3, 4, 5, 6, 7 }, out[16];
        DWORD sz;
        HRESULT hr = IDirect3DResource9_SetPrivateData(r, &G1, in, sizeof in, 0);
        CHECKF(hr == D3D_OK, "%s Set got %s", kname[k], d3dh_hr_name(hr));
        sz = sizeof out; memset(out, 0xcc, sizeof out);
        hr = IDirect3DResource9_GetPrivateData(r, &G1, out, &sz);
        CHECKF(hr == D3D_OK, "%s Get got %s", kname[k], d3dh_hr_name(hr));
        CHECKF(sz == 7, "%s size %lu", kname[k], sz);
        CHECK(!memcmp(in, out, 7));
        CHECK(out[7] == 0xcc);
        /* overwrite with a different size */
        BYTE in2[3] = { 9, 8, 7 };
        EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, in2, 3, 0), D3D_OK);
        sz = sizeof out;
        EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G1, out, &sz), D3D_OK);
        EXPECT_EQ(sz, 3);
        CHECK(!memcmp(in2, out, 3));
        /* second GUID is independent */
        EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G2, in, 5, 0), D3D_OK);
        sz = sizeof out;
        EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G2, out, &sz), D3D_OK);
        EXPECT_EQ(sz, 5);
        EXPECT_HR(IDirect3DResource9_FreePrivateData(r, &G1), D3D_OK);
        sz = sizeof out;
        EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G1, out, &sz), D3DERR_NOTFOUND);
        EXPECT_HR(IDirect3DResource9_FreePrivateData(r, &G1), D3DERR_NOTFOUND);
        sz = sizeof out;
        EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G2, out, &sz), D3D_OK);
        IDirect3DResource9_Release(r);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pd_buffer_protocol)
{
    D9_OPEN(d);
    for (int k = 0; k < K_COUNT; k++) {
        IDirect3DResource9 *r = mkres(&d, k);
        if (!r) continue;
        BYTE in[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, out[16];
        EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, in, 10, 0), D3D_OK);
        DWORD sz = 4;
        memset(out, 0xcc, sizeof out);
        HRESULT hr = IDirect3DResource9_GetPrivateData(r, &G1, out, &sz);
        CHECKF(hr == D3DERR_MOREDATA, "%s small buffer got %s", kname[k], d3dh_hr_name(hr));
        CHECKF(sz == 10, "%s needed size %lu", kname[k], sz);
        sz = 9;
        hr = IDirect3DResource9_GetPrivateData(r, &G1, out, &sz);
        CHECKF(hr == D3DERR_MOREDATA && sz == 10, "%s exact-1 got %s %lu", kname[k], d3dh_hr_name(hr), sz);
        sz = 10;
        hr = IDirect3DResource9_GetPrivateData(r, &G1, out, &sz);
        CHECKF(hr == D3D_OK && sz == 10, "%s exact got %s %lu", kname[k], d3dh_hr_name(hr), sz);
        sz = 16;
        hr = IDirect3DResource9_GetPrivateData(r, &G1, out, &sz);
        CHECKF(hr == D3D_OK && sz == 10, "%s larger buffer got %s %lu", kname[k], d3dh_hr_name(hr), sz);
        sz = 0;
        hr = IDirect3DResource9_GetPrivateData(r, &G1, out, &sz);
        CHECKF(hr == D3DERR_MOREDATA && sz == 10, "%s zero size got %s %lu", kname[k], d3dh_hr_name(hr), sz);
        sz = 0;
        hr = IDirect3DResource9_GetPrivateData(r, &G1, NULL, &sz);
        t_note("%s NULL buffer, size 0: %s size %lu", kname[k], d3dh_hr_name(hr), sz);
        sz = 3;
        EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G2, out, &sz), D3DERR_NOTFOUND);
        EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G1, out, NULL), D3DERR_INVALIDCALL);
        IDirect3DResource9_Release(r);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pd_invalid_args)
{
    D9_OPEN(d);
    IDirect3DResource9 *r = mkres(&d, K_TEX);
    if (!r) { t_skip("no texture"); D9_END(d); return; }
    BYTE b[4] = { 0 };
    EXPECT_HR(IDirect3DResource9_SetPrivateData(r, NULL, b, 4, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, NULL, 4, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DResource9_GetPrivateData(r, NULL, b, &(DWORD){ 4 }), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DResource9_FreePrivateData(r, NULL), D3DERR_INVALIDCALL);
    HRESULT hr = IDirect3DResource9_SetPrivateData(r, &G1, b, 4, 0x80);
    t_note("SetPrivateData with unknown flag 0x80: %s", d3dh_hr_name(hr));
    hr = IDirect3DResource9_SetPrivateData(r, &G2, b, 0, 0);
    t_note("SetPrivateData size 0: %s", d3dh_hr_name(hr));
    IDirect3DResource9_Release(r);
    D9_END(d);
}

TEST(d3d9_device_resources_pd_large_blob)
{
    D9_OPEN(d);
    IDirect3DResource9 *r = mkres(&d, K_VB);
    if (!r) { t_skip("no vb"); D9_END(d); return; }
    BYTE *buf = malloc(100000), *out = malloc(100000);
    for (int i = 0; i < 100000; i++) buf[i] = (BYTE)(i * 31);
    EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, buf, 100000, 0), D3D_OK);
    DWORD sz = 100000;
    EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G1, out, &sz), D3D_OK);
    EXPECT_EQ(sz, 100000);
    CHECK(!memcmp(buf, out, 100000));
    free(buf); free(out);
    IDirect3DResource9_Release(r);
    D9_END(d);
}

TEST(d3d9_device_resources_pd_iunknown)
{
    D9_OPEN(d);
    for (int k = 0; k < K_COUNT; k++) {
        IDirect3DResource9 *r = mkres(&d, k);
        if (!r) continue;
        Fake f; fk_init(&f);
        IUnknown *u = (IUnknown *)&f;
        HRESULT hr = IDirect3DResource9_SetPrivateData(r, &G1, &u, sizeof u, D3DSPD_IUNKNOWN);
        CHECKF(hr == D3D_OK, "%s Set got %s", kname[k], d3dh_hr_name(hr));
        CHECKF(f.ref == 2, "%s ref after set %ld", kname[k], f.ref);
        /* getting it back gives an AddRef'd pointer */
        IUnknown *got = NULL;
        DWORD sz = sizeof got;
        hr = IDirect3DResource9_GetPrivateData(r, &G1, &got, &sz);
        CHECKF(hr == D3D_OK && got == u && sz == sizeof u, "%s Get got %s %p %lu", kname[k], d3dh_hr_name(hr), got, sz);
        CHECKF(f.ref == 3, "%s ref after get %ld", kname[k], f.ref);
        f.ref--;
        /* replacing releases the old object */
        BYTE b[4] = { 1, 2, 3, 4 };
        EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, b, 4, 0), D3D_OK);
        CHECKF(f.ref == 1, "%s ref after replace %ld", kname[k], f.ref);
        /* free releases it too */
        EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, &u, sizeof u, D3DSPD_IUNKNOWN), D3D_OK);
        CHECKF(f.ref == 2, "%s ref %ld", kname[k], f.ref);
        EXPECT_HR(IDirect3DResource9_FreePrivateData(r, &G1), D3D_OK);
        CHECKF(f.ref == 1, "%s ref after free %ld", kname[k], f.ref);
        /* destroying the resource releases it */
        EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G2, &u, sizeof u, D3DSPD_IUNKNOWN), D3D_OK);
        CHECK(f.ref == 2);
        IDirect3DResource9_Release(r);
        CHECKF(f.ref == 1, "%s ref after resource destruction %ld", kname[k], f.ref);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_pd_iunknown_wrong_size)
{
    D9_OPEN(d);
    IDirect3DResource9 *r = mkres(&d, K_TEX);
    if (!r) { t_skip("no texture"); D9_END(d); return; }
    Fake f; fk_init(&f);
    IUnknown *u = (IUnknown *)&f;
    DWORD sizes[] = { 0, 1, 4, 16, sizeof u - 1, sizeof u + 1 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        HRESULT hr = IDirect3DResource9_SetPrivateData(r, &G1, &u, sizes[i], D3DSPD_IUNKNOWN);
        CHECKF(hr == D3DERR_INVALIDCALL, "size %lu got %s", sizes[i], d3dh_hr_name(hr));
        CHECK(f.ref == 1);
    }
    IDirect3DResource9_Release(r);
    D9_END(d);
}

TEST(d3d9_device_resources_pd_iunknown_same_object_twice)
{
    D9_OPEN(d);
    IDirect3DResource9 *r = mkres(&d, K_TEX);
    if (!r) { t_skip("no texture"); D9_END(d); return; }
    Fake f; fk_init(&f);
    IUnknown *u = (IUnknown *)&f;
    EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, &u, sizeof u, D3DSPD_IUNKNOWN), D3D_OK);
    EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, &u, sizeof u, D3DSPD_IUNKNOWN), D3D_OK);
    CHECKF(f.ref == 2, "ref after setting the same object twice %ld", f.ref);
    EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G2, &u, sizeof u, D3DSPD_IUNKNOWN), D3D_OK);
    CHECK(f.ref == 3);
    IDirect3DResource9_Release(r);
    CHECKF(f.ref == 1, "ref after destruction %ld", f.ref);
    D9_END(d);
}

TEST(d3d9_device_resources_pd_iunknown_get_small_buffer)
{
    D9_OPEN(d);
    IDirect3DResource9 *r = mkres(&d, K_IB);
    if (!r) { t_skip("no ib"); D9_END(d); return; }
    Fake f; fk_init(&f);
    IUnknown *u = (IUnknown *)&f;
    IDirect3DResource9_SetPrivateData(r, &G1, &u, sizeof u, D3DSPD_IUNKNOWN);
    IUnknown *got = NULL;
    DWORD sz = 2;
    EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G1, &got, &sz), D3DERR_MOREDATA);
    EXPECT_EQ(sz, sizeof u);
    CHECK(got == NULL);
    CHECKF(f.ref == 2, "failed get must not AddRef (%ld)", f.ref);
    IDirect3DResource9_Release(r);
    D9_END(d);
}

TEST(d3d9_device_resources_pd_device_unaffected_by_resource_release)
{
    D9_OPEN(d);
    IDirect3DResource9 *r = mkres(&d, K_TEX);
    if (!r) { t_skip("no texture"); D9_END(d); return; }
    /* private data is per resource: a surface level has its own store */
    IDirect3DTexture9 *t = (IDirect3DTexture9 *)r;
    IDirect3DSurface9 *s = NULL;
    IDirect3DTexture9_GetSurfaceLevel(t, 0, &s);
    BYTE b = 5; DWORD sz = 1; BYTE o;
    EXPECT_HR(IDirect3DResource9_SetPrivateData(r, &G1, &b, 1, 0), D3D_OK);
    if (s) {
        EXPECT_HR(IDirect3DSurface9_GetPrivateData(s, &G1, &o, &sz), D3DERR_NOTFOUND);
        EXPECT_HR(IDirect3DSurface9_SetPrivateData(s, &G1, &b, 1, 0), D3D_OK);
        EXPECT_HR(IDirect3DSurface9_FreePrivateData(s, &G1), D3D_OK);
        sz = 1;
        EXPECT_HR(IDirect3DResource9_GetPrivateData(r, &G1, &o, &sz), D3D_OK);
        IDirect3DSurface9_Release(s);
    }
    IDirect3DResource9_Release(r);
    D9_END(d);
}

TEST(d3d9_device_resources_res_gettype_all)
{
    D9_OPEN(d);
    D3DRESOURCETYPE want[] = { D3DRTYPE_TEXTURE, D3DRTYPE_CUBETEXTURE, D3DRTYPE_VOLUMETEXTURE, D3DRTYPE_VERTEXBUFFER, D3DRTYPE_INDEXBUFFER, D3DRTYPE_SURFACE, D3DRTYPE_SURFACE, D3DRTYPE_TEXTURE };
    for (int k = 0; k < K_COUNT; k++) {
        IDirect3DResource9 *r = mkres(&d, k);
        if (!r) continue;
        CHECKF(IDirect3DResource9_GetType(r) == want[k], "%s type %d", kname[k], IDirect3DResource9_GetType(r));
        IDirect3DDevice9 *dd = (IDirect3DDevice9 *)1;
        EXPECT_HR(IDirect3DResource9_GetDevice(r, &dd), D3D_OK);
        CHECK(dd == d.dev);
        SAFE_RELEASE(dd);
        EXPECT_HR(IDirect3DResource9_GetDevice(r, NULL), D3DERR_INVALIDCALL);
        IUnknown *u = NULL;
        EXPECT_HR(IDirect3DResource9_QueryInterface(r, &IID_IUnknown, (void **)&u), S_OK);
        SAFE_RELEASE(u);
        IDirect3DResource9_Release(r);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_res_priority_by_kind)
{
    D9_OPEN(d);
    for (int k = 0; k < K_COUNT; k++) {
        IDirect3DResource9 *r = mkres(&d, k);
        if (!r) continue;
        D3DPOOL pool = (k == K_SURF_SYS) ? D3DPOOL_SYSTEMMEM : (k == K_RT || k == K_TEX_DEF) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
        DWORD prev = IDirect3DResource9_SetPriority(r, 5);
        DWORD now = IDirect3DResource9_GetPriority(r);
        if (pool == D3DPOOL_MANAGED && k != K_SURF_SYS) {
            CHECKF(prev == 0 && now == 5, "%s managed prev %lu now %lu", kname[k], prev, now);
        } else {
            CHECKF(prev == 0, "%s unmanaged prev %lu", kname[k], prev);
            t_note("%s (pool %d) priority after Set(5): %lu", kname[k], pool, now);
        }
        IDirect3DResource9_PreLoad(r);
        IDirect3DResource9_Release(r);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_res_refcount_stable)
{
    D9_OPEN(d);
    for (int k = 0; k < K_COUNT; k++) {
        IDirect3DResource9 *r = mkres(&d, k);
        if (!r) continue;
        EXPECT_EQ(IDirect3DResource9_AddRef(r), 2);
        EXPECT_EQ(IDirect3DResource9_AddRef(r), 3);
        EXPECT_EQ(IDirect3DResource9_Release(r), 2);
        EXPECT_EQ(IDirect3DResource9_Release(r), 1);
        EXPECT_EQ(IDirect3DResource9_Release(r), 0);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_res_survive_device_release)
{
    D9 d;
    if (!d9_open(&d)) return;
    IDirect3DTexture9 *t = NULL;
    if (FAILED(IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) { t_skip("no tex"); d9_close(&d); return; }
    /* the application's device reference is released first; the child resource keeps the device alive */
    IDirect3DDevice9_AddRef(d.dev);
    ULONG rc = IDirect3DDevice9_Release(d.dev);
    CHECK(rc >= 1);
    D3DLOCKED_RECT lr;
    EXPECT_HR(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, 0), D3D_OK);
    IDirect3DTexture9_UnlockRect(t, 0);
    IDirect3DDevice9 *dd = NULL;
    EXPECT_HR(IDirect3DTexture9_GetDevice(t, &dd), D3D_OK);
    SAFE_RELEASE(dd);
    IDirect3DTexture9_Release(t);
    d9_close(&d);
}
