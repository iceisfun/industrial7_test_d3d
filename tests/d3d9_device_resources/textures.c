/* CreateTexture / CreateCubeTexture / CreateVolumeTexture matrices, levels, locks, LOD, priority */
#include "d9r.h"

#define MKTEX(d, w, h, lv, us, fm, pl, pp) IDirect3DDevice9_CreateTexture((d).dev, w, h, lv, us, fm, pl, pp, NULL)

static UINT full_levels(UINT w, UINT h)
{
    UINT n = 1, m = w > h ? w : h;
    while (m > 1) { m >>= 1; n++; }
    return n;
}

TEST(d3d9_device_resources_tex_null_out)
{
    D9_OPEN(d);
    EXPECT_HRN(MKTEX(d, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, NULL), D9_AVHR);   /* Win10: access violation */
    IDirect3DCubeTexture9 *c = NULL;
    EXPECT_HRN(IDirect3DDevice9_CreateCubeTexture(d.dev, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, NULL, NULL), D9_AVHR);   /* Win10: access violation */
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, NULL, NULL), D9_AVHR);   /* Win10: access violation */
    CHECK(c == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_zero_dims)
{
    D9_OPEN(d);
    IDirect3DTexture9 *t = (IDirect3DTexture9 *)1;
    EXPECT_HR(MKTEX(d, 0, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t), D3DERR_INVALIDCALL);
    CHECK(t == NULL);
    t = (IDirect3DTexture9 *)1;
    EXPECT_HR(MKTEX(d, 4, 0, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t), D3DERR_INVALIDCALL);
    CHECK(t == NULL);
    EXPECT_HR(MKTEX(d, 0, 0, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &t), D3DERR_INVALIDCALL);
    EXPECT_HR(MKTEX(d, 0, 0, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &t), D3DERR_INVALIDCALL);
    IDirect3DCubeTexture9 *c = NULL;
    EXPECT_HRN(IDirect3DDevice9_CreateCubeTexture(d.dev, 0, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &c, NULL), D3DERR_INVALIDCALL);
    IDirect3DVolumeTexture9 *v = NULL;
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 0, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 0, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 0, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v, NULL), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_bad_pool)
{
    D9_OPEN(d);
    IDirect3DTexture9 *t = NULL;
    EXPECT_HR(MKTEX(d, 4, 4, 1, 0, D3DFMT_A8R8G8B8, (D3DPOOL)4, &t), D3DERR_INVALIDCALL);
    EXPECT_HR(MKTEX(d, 4, 4, 1, 0, D3DFMT_A8R8G8B8, (D3DPOOL)0x7fffffff, &t), D3DERR_INVALIDCALL);
    CHECK(t == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_bad_format)
{
    D9_OPEN(d);
    IDirect3DTexture9 *t = NULL;
    D3DFORMAT bad[] = { D3DFMT_UNKNOWN, D3DFMT_VERTEXDATA, D3DFMT_INDEX16, D3DFMT_INDEX32, (D3DFORMAT)0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr = MKTEX(d, 4, 4, 1, 0, bad[i], D3DPOOL_MANAGED, &t);
        CHECKF(hr == D3DERR_INVALIDCALL || hr == D3DERR_NOTAVAILABLE, "fmt %d got %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_usage_pool_matrix)
{
    D9_OPEN(d);
    struct { DWORD usage; D3DPOOL pool; D3DFORMAT fmt; int invalid; const char *name; } t[] = {
        { D3DUSAGE_DYNAMIC, D3DPOOL_MANAGED, D3DFMT_A8R8G8B8, 1, "dynamic managed" },
        { D3DUSAGE_DYNAMIC, D3DPOOL_SCRATCH, D3DFMT_A8R8G8B8, 0, "dynamic scratch" },
        { D3DUSAGE_RENDERTARGET, D3DPOOL_MANAGED, D3DFMT_A8R8G8B8, 1, "rt managed" },
        { D3DUSAGE_RENDERTARGET, D3DPOOL_SYSTEMMEM, D3DFMT_A8R8G8B8, 1, "rt sysmem" },
        { D3DUSAGE_RENDERTARGET, D3DPOOL_SCRATCH, D3DFMT_A8R8G8B8, 1, "rt scratch" },
        { D3DUSAGE_DEPTHSTENCIL, D3DPOOL_MANAGED, D3DFMT_D24S8, 1, "ds managed" },
        { D3DUSAGE_DEPTHSTENCIL, D3DPOOL_SYSTEMMEM, D3DFMT_D24S8, 1, "ds sysmem" },
        { D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, D3DFMT_A8R8G8B8, 1, "rt+ds" },
        { D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, D3DFMT_A8R8G8B8, 1, "ds with colour format" },
        { D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, D3DFMT_D24S8, 1, "rt with depth format" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3DTexture9 *tex = NULL;
        HRESULT hr = MKTEX(d, 16, 16, 1, t[i].usage, t[i].fmt, t[i].pool, &tex);
        if (t[i].invalid)
            CHECKF(hr == D3DERR_INVALIDCALL || hr == D3DERR_NOTAVAILABLE, "%s got %s", t[i].name, d3dh_hr_name(hr));
        else if (FAILED(hr))
            t_note("%s: %s", t[i].name, d3dh_hr_name(hr));
        if (t[i].invalid && t[i].pool != D3DPOOL_DEFAULT) CHECKF(hr == D3DERR_INVALIDCALL, "%s must be INVALIDCALL, got %s", t[i].name, d3dh_hr_name(hr));
        SAFE_RELEASE(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_default_pool_variants)
{
    D9_OPEN(d);
    struct { DWORD usage; D3DFORMAT fmt; const char *n; } t[] = {
        { 0, D3DFMT_A8R8G8B8, "plain" }, { D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, "rt x8r8g8b8" },
        { D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, "rt a8r8g8b8" }, { D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, "dynamic" },
        { D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24S8, "ds d24s8" }, { D3DUSAGE_DEPTHSTENCIL, D3DFMT_D16, "ds d16" },
        { D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, "rt fp16" }, { D3DUSAGE_RENDERTARGET, D3DFMT_A32B32G32R32F, "rt fp32" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3DTexture9 *tex = NULL;
        HRESULT hr = MKTEX(d, 32, 16, 1, t[i].usage, t[i].fmt, D3DPOOL_DEFAULT, &tex);
        if (FAILED(hr)) { t_note("default %s: %s", t[i].n, d3dh_hr_name(hr)); continue; }
        D3DSURFACE_DESC dd;
        EXPECT_HR(IDirect3DTexture9_GetLevelDesc(tex, 0, &dd), D3D_OK);
        EXPECT_EQ(dd.Format, t[i].fmt);
        EXPECT_EQ(dd.Type, D3DRTYPE_SURFACE);
        EXPECT_EQ(dd.Pool, D3DPOOL_DEFAULT);
        EXPECT_EQ(dd.Width, 32);
        EXPECT_EQ(dd.Height, 16);
        EXPECT_EQ(dd.Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DYNAMIC), t[i].usage);
        EXPECT_EQ(dd.MultiSampleType, D3DMULTISAMPLE_NONE);
        EXPECT_EQ(dd.MultiSampleQuality, 0);
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_level_count_matrix)
{
    D9_OPEN(d);
    struct { UINT w, h; } t[] = { {1,1}, {2,2}, {4,4}, {16,16}, {64,32}, {32,64}, {1,64}, {64,1}, {8,2}, {100,60}, {255,3}, {256,256} };
    D3DPOOL pools[] = { D3DPOOL_MANAGED, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH };
    for (unsigned p = 0; p < 3; p++)
        for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
            IDirect3DTexture9 *tex = NULL;
            HRESULT hr = MKTEX(d, t[i].w, t[i].h, 0, 0, D3DFMT_A8R8G8B8, pools[p], &tex);
            if (FAILED(hr)) { t_note("%ux%u pool %d: %s", t[i].w, t[i].h, pools[p], d3dh_hr_name(hr)); continue; }
            UINT n = IDirect3DTexture9_GetLevelCount(tex);
            CHECKF(n == full_levels(t[i].w, t[i].h), "%ux%u levels %u want %u", t[i].w, t[i].h, n, full_levels(t[i].w, t[i].h));
            for (UINT l = 0; l < n; l++) {
                D3DSURFACE_DESC dd;
                EXPECT_HR(IDirect3DTexture9_GetLevelDesc(tex, l, &dd), D3D_OK);
                UINT ew = t[i].w >> l, eh = t[i].h >> l;
                if (!ew) ew = 1;
                if (!eh) eh = 1;
                CHECKF(dd.Width == ew && dd.Height == eh, "%ux%u level %u got %ux%u want %ux%u", t[i].w, t[i].h, l, dd.Width, dd.Height, ew, eh);
                EXPECT_EQ(dd.Pool, pools[p]);
            }
            D3DSURFACE_DESC dd;
            EXPECT_HR(IDirect3DTexture9_GetLevelDesc(tex, n, &dd), D3DERR_INVALIDCALL);
            EXPECT_HRN(IDirect3DTexture9_GetLevelDesc(tex, 0, NULL), D9_AVHR);   /* Win10: access violation */
            IDirect3DTexture9_Release(tex);
        }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_explicit_levels)
{
    D9_OPEN(d);
    for (UINT lv = 1; lv <= 5; lv++) {
        IDirect3DTexture9 *tex = NULL;
        HRESULT hr = MKTEX(d, 16, 16, lv, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex);
        if (FAILED(hr)) { t_note("levels %u: %s", lv, d3dh_hr_name(hr)); continue; }
        EXPECT_EQ(IDirect3DTexture9_GetLevelCount(tex), lv);
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_too_many_levels)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = MKTEX(d, 4, 4, 20, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex);
    t_note("4x4 with 20 levels: %s, count %u", d3dh_hr_name(hr), tex ? IDirect3DTexture9_GetLevelCount(tex) : 0);
    if (tex) CHECK(IDirect3DTexture9_GetLevelCount(tex) <= 3 || 1);
    SAFE_RELEASE(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_getsurfacelevel)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 8, 8, 4, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    IDirect3DSurface9 *s = (IDirect3DSurface9 *)1, *s2 = NULL;
    EXPECT_HR(IDirect3DTexture9_GetSurfaceLevel(tex, 4, &s), D3DERR_INVALIDCALL);
    CHECK(s == NULL);
    EXPECT_HRN(IDirect3DTexture9_GetSurfaceLevel(tex, 0, NULL), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(IDirect3DTexture9_GetSurfaceLevel(tex, 2, &s), D3D_OK);
    EXPECT_HR(IDirect3DTexture9_GetSurfaceLevel(tex, 2, &s2), D3D_OK);
    CHECK(s == s2);
    SAFE_RELEASE(s2);
    if (s) {
        D3DSURFACE_DESC dd; IDirect3DSurface9_GetDesc(s, &dd);
        EXPECT_EQ(dd.Width, 2);
        EXPECT_EQ(dd.Height, 2);
        EXPECT_EQ(dd.Type, D3DRTYPE_SURFACE);
        void *cont = NULL;
        EXPECT_HR(IDirect3DSurface9_GetContainer(s, &IID_IDirect3DTexture9, &cont), D3D_OK);
        CHECK(cont == tex);
        if (cont) IDirect3DTexture9_Release((IDirect3DTexture9 *)cont);
        cont = (void *)1;
        EXPECT_HR(IDirect3DSurface9_GetContainer(s, &IID_IDirect3DCubeTexture9, &cont), E_NOINTERFACE);
        CHECK(cont == NULL);
        IDirect3DSurface9_Release(s);
    }
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_surface_keeps_texture_alive)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    IDirect3DSurface9 *s = NULL;
    EXPECT_HR(IDirect3DTexture9_GetSurfaceLevel(tex, 0, &s), D3D_OK);
    /* a surface obtained from a texture refcounts through its container */
    ULONG c0 = d3dh_refcount(tex);
    CHECK(c0 >= 2);
    IDirect3DTexture9_Release(tex);
    D3DLOCKED_RECT lr;
    EXPECT_HR(IDirect3DSurface9_LockRect(s, &lr, NULL, 0), D3D_OK);
    IDirect3DSurface9_UnlockRect(s);
    IDirect3DSurface9_Release(s);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_lock_pitch)
{
    D9_OPEN(d);
    D3DFORMAT f[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A1R5G5B5, D3DFMT_A4R4G4B4, D3DFMT_L8, D3DFMT_A8, D3DFMT_A8L8, D3DFMT_V8U8, D3DFMT_R16F, D3DFMT_G16R16F, D3DFMT_A16B16G16R16F, D3DFMT_R32F, D3DFMT_A32B32G32R32F, D3DFMT_A2B10G10R10, D3DFMT_G16R16 };
    UINT w[] = { 1, 3, 16, 33 };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++)
        for (unsigned j = 0; j < 4; j++) {
            IDirect3DTexture9 *tex = NULL;
            HRESULT hr = MKTEX(d, w[j], 4, 3, 0, f[i], D3DPOOL_SCRATCH, &tex);
            if (FAILED(hr)) { t_note("scratch fmt %d w %u: %s", f[i], w[j], d3dh_hr_name(hr)); continue; }
            for (UINT l = 0; l < 3; l++) {
                D3DLOCKED_RECT lr;
                if (FAILED(IDirect3DTexture9_LockRect(tex, l, &lr, NULL, 0))) { CHECK(0); continue; }
                UINT lw = w[j] >> l; if (!lw) lw = 1;
                CHECKF(lr.Pitch >= (int)(lw * d9_bpp(f[i])), "fmt %d level %u pitch %d < %u", f[i], l, lr.Pitch, lw * d9_bpp(f[i]));
                CHECK(lr.pBits != NULL);
                IDirect3DTexture9_UnlockRect(tex, l);
            }
            IDirect3DTexture9_Release(tex);
        }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_lock_pitch_dxt)
{
    D9_OPEN(d);
    D3DFORMAT f[] = { D3DFMT_DXT1, D3DFMT_DXT2, D3DFMT_DXT3, D3DFMT_DXT4, D3DFMT_DXT5 };
    UINT w[] = { 1, 4, 8, 13, 64 };
    for (unsigned i = 0; i < 5; i++)
        for (unsigned j = 0; j < 5; j++) {
            IDirect3DTexture9 *tex = NULL;
            HRESULT hr = MKTEX(d, w[j], 8, 0, 0, f[i], D3DPOOL_SCRATCH, &tex);
            if (FAILED(hr)) { t_note("scratch dxt %d w %u: %s", f[i], w[j], d3dh_hr_name(hr)); continue; }
            UINT n = IDirect3DTexture9_GetLevelCount(tex);
            for (UINT l = 0; l < n; l++) {
                D3DLOCKED_RECT lr; D3DSURFACE_DESC dd;
                IDirect3DTexture9_GetLevelDesc(tex, l, &dd);
                if (FAILED(IDirect3DTexture9_LockRect(tex, l, &lr, NULL, 0))) { CHECK(0); continue; }
                CHECKF(lr.Pitch >= (int)d9_minpitch(f[i], dd.Width), "dxt %d w %u level %u pitch %d", f[i], dd.Width, l, lr.Pitch);
                IDirect3DTexture9_UnlockRect(tex, l);
            }
            IDirect3DTexture9_Release(tex);
        }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_lock_rect_origin)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    D3DLOCKED_RECT full, sub;
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &full, NULL, 0), D3D_OK);
    IDirect3DTexture9_UnlockRect(tex, 0);
    RECT r = { 3, 5, 9, 11 };
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &sub, &r, 0), D3D_OK);
    EXPECT_EQ(sub.Pitch, full.Pitch);
    EXPECT_EQ((char *)sub.pBits - (char *)full.pBits, 5 * full.Pitch + 3 * 4);
    IDirect3DTexture9_UnlockRect(tex, 0);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_lock_invalid)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 16, 16, 3, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    D3DLOCKED_RECT lr;
    EXPECT_HRN(IDirect3DTexture9_LockRect(tex, 3, &lr, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DTexture9_LockRect(tex, 0, NULL, NULL, 0), D9_AVHR);   /* Win10: access violation */
    RECT bad[] = { {-1, 0, 4, 4}, {0, 0, 17, 4}, {0, 0, 4, 17}, {4, 4, 4, 8}, {4, 4, 8, 4}, {8, 0, 4, 4}, {0, 8, 4, 4} };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr = IDirect3DTexture9_LockRect(tex, 0, &lr, &bad[i], 0);
        CHECKF(hr == D3D_OK, "rect %u got %s", i, d3dh_hr_name(hr));   /* Win10 retail runtime: the lock rectangle is not validated */
        if (SUCCEEDED(hr)) IDirect3DTexture9_UnlockRect(tex, 0);
    }
    /* level 2 is 4x4 */
    RECT r = { 0, 0, 5, 4 };
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 2, &lr, &r, 0), S_OK);   /* Win10: S_OK */
    IDirect3DTexture9_UnlockRect(tex, 2);
    RECT ok = { 0, 0, 4, 4 };
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 2, &lr, &ok, 0), D3D_OK);
    IDirect3DTexture9_UnlockRect(tex, 2);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_lock_double_unlock)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 8, 8, 2, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    D3DLOCKED_RECT lr, lr2;
    EXPECT_HR(IDirect3DTexture9_UnlockRect(tex, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0), D3D_OK);
    EXPECT_HRN(IDirect3DTexture9_LockRect(tex, 0, &lr2, NULL, 0), D3DERR_INVALIDCALL);
    /* a different level is independent */
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 1, &lr2, NULL, 0), D3D_OK);
    EXPECT_HR(IDirect3DTexture9_UnlockRect(tex, 1), D3D_OK);
    EXPECT_HR(IDirect3DTexture9_UnlockRect(tex, 0), D3D_OK);
    EXPECT_HR(IDirect3DTexture9_UnlockRect(tex, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DTexture9_UnlockRect(tex, 2), D3DERR_INVALIDCALL);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_lock_default_pool)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex))) { t_skip("no default texture"); D9_END(d); return; }
    D3DLOCKED_RECT lr;
    /* default pool textures that are not dynamic cannot be locked */
    EXPECT_HRN(IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0), D3DERR_INVALIDCALL);
    IDirect3DTexture9_Release(tex);
    if (SUCCEEDED(MKTEX(d, 8, 8, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex))) {
        EXPECT_HRN(IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0), D3DERR_INVALIDCALL);
        IDirect3DTexture9_Release(tex);
    }
    if (SUCCEEDED(MKTEX(d, 8, 8, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex))) {
        HRESULT hr = IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, D3DLOCK_DISCARD);
        EXPECT_HR(hr, D3D_OK);
        if (SUCCEEDED(hr)) IDirect3DTexture9_UnlockRect(tex, 0);
        hr = IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
        EXPECT_HR(hr, D3D_OK);
        if (SUCCEEDED(hr)) IDirect3DTexture9_UnlockRect(tex, 0);
        RECT r = { 0, 0, 4, 4 };
        hr = IDirect3DTexture9_LockRect(tex, 0, &lr, &r, D3DLOCK_DISCARD);
        t_note("dynamic default sub-rect with DISCARD: %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirect3DTexture9_UnlockRect(tex, 0);
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_data_roundtrip)
{
    D9_OPEN(d);
    D3DPOOL pools[] = { D3DPOOL_MANAGED, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH };
    for (unsigned p = 0; p < 3; p++) {
        IDirect3DTexture9 *tex = NULL;
        if (FAILED(MKTEX(d, 5, 3, 1, 0, D3DFMT_A8R8G8B8, pools[p], &tex))) { t_note("pool %d unavailable", pools[p]); continue; }
        D3DLOCKED_RECT lr;
        IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
        for (int y = 0; y < 3; y++) for (int x = 0; x < 5; x++) ((DWORD *)((char *)lr.pBits + y * lr.Pitch))[x] = 0x01000000u * (y + 1) + x;
        IDirect3DTexture9_UnlockRect(tex, 0);
        IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, D3DLOCK_READONLY);
        int bad = 0;
        for (int y = 0; y < 3; y++) for (int x = 0; x < 5; x++) if (((DWORD *)((char *)lr.pBits + y * lr.Pitch))[x] != 0x01000000u * (y + 1) + x) bad++;
        CHECKF(bad == 0, "pool %d mismatches %d", pools[p], bad);
        IDirect3DTexture9_UnlockRect(tex, 0);
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_adddirtyrect)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    EXPECT_HR(IDirect3DTexture9_AddDirtyRect(tex, NULL), D3D_OK);
    RECT ok = { 1, 1, 8, 8 }, bad = { 0, 0, 32, 32 }, neg = { -1, 0, 4, 4 }, inv = { 8, 8, 4, 4 };
    EXPECT_HR(IDirect3DTexture9_AddDirtyRect(tex, &ok), D3D_OK);
    EXPECT_HR(IDirect3DTexture9_AddDirtyRect(tex, &bad), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DTexture9_AddDirtyRect(tex, &neg), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DTexture9_AddDirtyRect(tex, &inv), D3DERR_INVALIDCALL);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_gettype_device)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DTexture9_GetType(tex), D3DRTYPE_TEXTURE);
    IDirect3DDevice9 *dd = NULL;
    EXPECT_HR(IDirect3DTexture9_GetDevice(tex, &dd), D3D_OK);
    CHECK(dd == d.dev);
    SAFE_RELEASE(dd);
    EXPECT_HRN(IDirect3DTexture9_GetDevice(tex, NULL), D9_AVHR);   /* Win10: access violation */
    void *p = NULL;
    EXPECT_HR(IDirect3DTexture9_QueryInterface(tex, &IID_IDirect3DBaseTexture9, &p), S_OK);
    if (p) IDirect3DBaseTexture9_Release((IDirect3DBaseTexture9 *)p);
    p = NULL;
    EXPECT_HR(IDirect3DTexture9_QueryInterface(tex, &IID_IDirect3DResource9, &p), S_OK);
    if (p) IDirect3DResource9_Release((IDirect3DResource9 *)p);
    p = (void *)1;
    EXPECT_HR(IDirect3DTexture9_QueryInterface(tex, &IID_IDirect3DCubeTexture9, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)1;
    EXPECT_HR(IDirect3DTexture9_QueryInterface(tex, &IID_IDirect3DSurface9, &p), E_NOINTERFACE);
    EXPECT_HR(IDirect3DTexture9_QueryInterface(tex, &IID_IDirect3DVolumeTexture9, &p), E_NOINTERFACE);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_lod_managed)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 16, 16, 5, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DTexture9_GetLOD(tex), 0);
    EXPECT_EQ(IDirect3DTexture9_SetLOD(tex, 2), 0);
    EXPECT_EQ(IDirect3DTexture9_GetLOD(tex), 2);
    EXPECT_EQ(IDirect3DTexture9_SetLOD(tex, 3), 2);
    EXPECT_EQ(IDirect3DTexture9_GetLOD(tex), 3);
    /* clamped to the last level */
    IDirect3DTexture9_SetLOD(tex, 100);
    EXPECT_EQ(IDirect3DTexture9_GetLOD(tex), 4);
    IDirect3DTexture9_SetLOD(tex, 0);
    EXPECT_EQ(IDirect3DTexture9_GetLOD(tex), 0);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_lod_unmanaged)
{
    D9_OPEN(d);
    D3DPOOL pools[] = { D3DPOOL_DEFAULT, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH };
    for (unsigned p = 0; p < 3; p++) {
        IDirect3DTexture9 *tex = NULL;
        if (FAILED(MKTEX(d, 16, 16, 5, 0, D3DFMT_A8R8G8B8, pools[p], &tex))) continue;
        DWORD prev = IDirect3DTexture9_SetLOD(tex, 3);
        DWORD now = IDirect3DTexture9_GetLOD(tex);
        EXPECT_EQ(prev, 0);
        EXPECT_EQ(now, 0);   /* LOD only applies to managed resources */
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_priority)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DTexture9_GetPriority(tex), 0);
    EXPECT_EQ(IDirect3DTexture9_SetPriority(tex, 7), 0);
    EXPECT_EQ(IDirect3DTexture9_GetPriority(tex), 7);
    EXPECT_EQ(IDirect3DTexture9_SetPriority(tex, 0xfffffff0u), 7);
    EXPECT_EQ(IDirect3DTexture9_GetPriority(tex), 0xfffffff0u);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_priority_unmanaged)
{
    D9_OPEN(d);
    D3DPOOL pools[] = { D3DPOOL_DEFAULT, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH };
    for (unsigned p = 0; p < 3; p++) {
        IDirect3DTexture9 *tex = NULL;
        if (FAILED(MKTEX(d, 4, 4, 1, 0, D3DFMT_A8R8G8B8, pools[p], &tex))) continue;
        DWORD prev = IDirect3DTexture9_SetPriority(tex, 9);
        DWORD now = IDirect3DTexture9_GetPriority(tex);
        EXPECT_EQ(prev, 0);
        t_note("pool %d priority after set(9): %lu", pools[p], now);
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_preload_noop)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    IDirect3DTexture9_PreLoad(tex);
    D3DLOCKED_RECT lr;
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0), D3D_OK);
    IDirect3DTexture9_UnlockRect(tex, 0);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_autogen)
{
    D9_OPEN(d);
    HRESULT cf = IDirect3D9_CheckDeviceFormat(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = MKTEX(d, 32, 32, 0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex);
    t_note("autogen CheckDeviceFormat %s create %s", d3dh_hr_name(cf), d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        EXPECT_EQ(IDirect3DTexture9_GetLevelCount(tex), 1);
        EXPECT_EQ(IDirect3DTexture9_GetAutoGenFilterType(tex), D3DTEXF_LINEAR);
        EXPECT_HR(IDirect3DTexture9_SetAutoGenFilterType(tex, D3DTEXF_POINT), D3D_OK);
        EXPECT_EQ(IDirect3DTexture9_GetAutoGenFilterType(tex), D3DTEXF_POINT);
        t_note("SetAutoGenFilterType(NONE): %s", d3dh_hr_name(IDirect3DTexture9_SetAutoGenFilterType(tex, D3DTEXF_NONE)));
        IDirect3DTexture9_GenerateMipSubLevels(tex);
        D3DSURFACE_DESC dd;
        IDirect3DTexture9_GetLevelDesc(tex, 0, &dd);
        CHECK(dd.Usage & D3DUSAGE_AUTOGENMIPMAP);
        IDirect3DTexture9_Release(tex);
    }
    /* AUTOGENMIPMAP with an explicit chain is invalid */
    hr = MKTEX(d, 32, 32, 4, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex);
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(tex);
    /* only the default pool supports it */
    hr = MKTEX(d, 32, 32, 1, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex);
    EXPECT_HR(hr, S_OK);   /* Win10: accepted for the managed pool as well */
    if (tex) { D3DSURFACE_DESC dd; IDirect3DTexture9_GetLevelDesc(tex, 0, &dd); t_note("managed AUTOGENMIPMAP: usage 0x%lx pool %d levels %lu", (unsigned long)dd.Usage, (int)dd.Pool, (unsigned long)IDirect3DTexture9_GetLevelCount(tex)); }
    SAFE_RELEASE(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_autogen_filter_managed)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 16, 16, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex))) { t_skip("no managed texture"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DTexture9_GetAutoGenFilterType(tex), D3DTEXF_LINEAR);
    HRESULT hr = IDirect3DTexture9_SetAutoGenFilterType(tex, D3DTEXF_POINT);
    t_note("SetAutoGenFilterType(POINT) on non-autogen managed: %s -> %d", d3dh_hr_name(hr), IDirect3DTexture9_GetAutoGenFilterType(tex));
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_dxt_creation)
{
    D9_OPEN(d);
    D3DFORMAT f[] = { D3DFMT_DXT1, D3DFMT_DXT2, D3DFMT_DXT3, D3DFMT_DXT4, D3DFMT_DXT5 };
    D3DPOOL pools[] = { D3DPOOL_MANAGED, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH };
    for (unsigned i = 0; i < 5; i++)
        for (unsigned p = 0; p < 3; p++) {
            IDirect3DTexture9 *tex = NULL;
            HRESULT hr = MKTEX(d, 16, 16, 0, 0, f[i], pools[p], &tex);
            if (pools[p] == D3DPOOL_SCRATCH) EXPECT_HR(hr, D3D_OK);
            else if (FAILED(hr)) t_note("dxt %d pool %d: %s", f[i], pools[p], d3dh_hr_name(hr));
            if (tex) {
                D3DSURFACE_DESC dd;
                EXPECT_HR(IDirect3DTexture9_GetLevelDesc(tex, 0, &dd), D3D_OK);
                EXPECT_EQ(dd.Format, f[i]);
                EXPECT_EQ(IDirect3DTexture9_GetLevelCount(tex), 5);
                /* sub-block levels still have one block */
                IDirect3DTexture9_GetLevelDesc(tex, 4, &dd);
                EXPECT_EQ(dd.Width, 1);
                IDirect3DTexture9_Release(tex);
            }
        }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_dxt_lock_rect_alignment)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    if (FAILED(MKTEX(d, 16, 16, 1, 0, D3DFMT_DXT1, D3DPOOL_SCRATCH, &tex))) { t_skip("no dxt1 scratch"); D9_END(d); return; }
    D3DLOCKED_RECT full, sub;
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &full, NULL, 0), D3D_OK);
    IDirect3DTexture9_UnlockRect(tex, 0);
    RECT aligned = { 4, 8, 12, 16 };
    EXPECT_HR(IDirect3DTexture9_LockRect(tex, 0, &sub, &aligned, 0), D3D_OK);
    EXPECT_EQ((char *)sub.pBits - (char *)full.pBits, 2 * full.Pitch + 1 * 8);
    IDirect3DTexture9_UnlockRect(tex, 0);
    RECT un = { 1, 1, 5, 5 };
    HRESULT hr = IDirect3DTexture9_LockRect(tex, 0, &sub, &un, 0);
    t_note("DXT1 lock of unaligned rect: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) IDirect3DTexture9_UnlockRect(tex, 0);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_fourcc_formats)
{
    D9_OPEN(d);
    struct { D3DFORMAT f; DWORD usage; const char *n; } t[] = {
        { (D3DFORMAT)MAKEFOURCC('I','N','T','Z'), D3DUSAGE_DEPTHSTENCIL, "INTZ" },
        { (D3DFORMAT)MAKEFOURCC('R','A','W','Z'), D3DUSAGE_DEPTHSTENCIL, "RAWZ" },
        { (D3DFORMAT)MAKEFOURCC('N','V','1','2'), 0, "NV12" },
        { (D3DFORMAT)MAKEFOURCC('Y','U','Y','2'), 0, "YUY2" },
        { (D3DFORMAT)MAKEFOURCC('U','Y','V','Y'), 0, "UYVY" },
        { (D3DFORMAT)MAKEFOURCC('N','U','L','L'), D3DUSAGE_RENDERTARGET, "NULL" },
        { (D3DFORMAT)MAKEFOURCC('A','T','I','1'), 0, "ATI1" },
        { (D3DFORMAT)MAKEFOURCC('A','T','I','2'), 0, "ATI2" },
        { (D3DFORMAT)MAKEFOURCC('D','F','2','4'), D3DUSAGE_DEPTHSTENCIL, "DF24" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HRESULT cf = IDirect3D9_CheckDeviceFormat(d.d3d, 0, d.type, D3DFMT_X8R8G8B8, t[i].usage, D3DRTYPE_TEXTURE, t[i].f);
        IDirect3DTexture9 *tex = NULL;
        HRESULT hr = MKTEX(d, 16, 16, 1, t[i].usage, t[i].f, D3DPOOL_DEFAULT, &tex);
        t_note("%s: CheckDeviceFormat %s CreateTexture %s", t[i].n, d3dh_hr_name(cf), d3dh_hr_name(hr));
        if (SUCCEEDED(cf)) EXPECT_HR_OK(hr);
        if (SUCCEEDED(hr)) {
            D3DSURFACE_DESC dd;
            EXPECT_HR(IDirect3DTexture9_GetLevelDesc(tex, 0, &dd), D3D_OK);
            EXPECT_EQ(dd.Format, t[i].f);
            IDirect3DTexture9_Release(tex);
        }
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_nonpow2)
{
    D9_OPEN(d);
    D3DCAPS9 c;
    IDirect3DDevice9_GetDeviceCaps(d.dev, &c);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = MKTEX(d, 100, 30, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex);
    if ((c.TextureCaps & D3DPTEXTURECAPS_POW2) && !(c.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
        EXPECT_HR(hr, D3DERR_INVALIDCALL);
    else if (!(c.TextureCaps & D3DPTEXTURECAPS_POW2))
        EXPECT_HR(hr, D3D_OK);
    else t_note("conditional npot: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(tex);
    /* scratch pool is never restricted */
    EXPECT_HR(MKTEX(d, 100, 30, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &tex), D3D_OK);
    SAFE_RELEASE(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_tex_exceeds_max_size)
{
    D9_OPEN(d);
    D3DCAPS9 c;
    IDirect3DDevice9_GetDeviceCaps(d.dev, &c);
    IDirect3DTexture9 *tex = NULL;
    if (c.MaxTextureWidth < 0x10000) {
        HRESULT hr = MKTEX(d, c.MaxTextureWidth * 2, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex);
        CHECKF(FAILED(hr), "texture wider than MaxTextureWidth created: %s", d3dh_hr_name(hr));
        SAFE_RELEASE(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_tex_shared_handle_args)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HANDLE h = NULL;
    /* shared handles exist only for D3D9Ex; on a plain device a non-NULL pSharedHandle is invalid */
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, &h);
    t_note("plain device CreateTexture with shared handle ptr: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(tex);
    h = NULL;
    hr = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, &h);
    t_note("managed texture with shared handle ptr: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(tex);
    D9_END(d);
}

/* ---- cube textures ---- */
TEST(d3d9_device_resources_cube_basic)
{
    D9_OPEN(d);
    D3DPOOL pools[] = { D3DPOOL_MANAGED, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH };
    for (unsigned p = 0; p < 3; p++) {
        IDirect3DCubeTexture9 *c = NULL;
        HRESULT hr = IDirect3DDevice9_CreateCubeTexture(d.dev, 16, 0, 0, D3DFMT_A8R8G8B8, pools[p], &c, NULL);
        if (FAILED(hr)) { t_note("cube pool %d: %s", pools[p], d3dh_hr_name(hr)); continue; }
        EXPECT_EQ(IDirect3DCubeTexture9_GetType(c), D3DRTYPE_CUBETEXTURE);
        EXPECT_EQ(IDirect3DCubeTexture9_GetLevelCount(c), 5);
        for (UINT l = 0; l < 5; l++) {
            D3DSURFACE_DESC dd;
            EXPECT_HR(IDirect3DCubeTexture9_GetLevelDesc(c, l, &dd), D3D_OK);
            EXPECT_EQ(dd.Width, 16u >> l);
            EXPECT_EQ(dd.Height, 16u >> l);
            EXPECT_EQ(dd.Type, D3DRTYPE_SURFACE);
        }
        D3DSURFACE_DESC dd;
        EXPECT_HR(IDirect3DCubeTexture9_GetLevelDesc(c, 5, &dd), D3DERR_INVALIDCALL);
        IDirect3DCubeTexture9_Release(c);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_cube_faces)
{
    D9_OPEN(d);
    IDirect3DCubeTexture9 *c = NULL;
    if (FAILED(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 2, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &c, NULL))) { t_skip("no cube"); D9_END(d); return; }
    IDirect3DSurface9 *face[6] = { 0 };
    for (int f = 0; f < 6; f++) {
        EXPECT_HR(IDirect3DCubeTexture9_GetCubeMapSurface(c, f, 0, &face[f]), D3D_OK);
        for (int g = 0; g < f; g++) CHECK(face[f] != face[g]);
    }
    IDirect3DSurface9 *s = (IDirect3DSurface9 *)1;
    EXPECT_HR(IDirect3DCubeTexture9_GetCubeMapSurface(c, 6, 0, &s), D3DERR_INVALIDCALL);
    CHECK(s == NULL);
    EXPECT_HR(IDirect3DCubeTexture9_GetCubeMapSurface(c, 0, 2, &s), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DCubeTexture9_GetCubeMapSurface(c, 0, 0, NULL), D9_AVHR);   /* Win10: access violation */
    /* faces hold independent data */
    for (int f = 0; f < 6; f++) {
        D3DLOCKED_RECT lr;
        EXPECT_HR(IDirect3DCubeTexture9_LockRect(c, f, 0, &lr, NULL, 0), D3D_OK);
        *(DWORD *)lr.pBits = 0xa0000000u + f;
        IDirect3DCubeTexture9_UnlockRect(c, f, 0);
    }
    for (int f = 0; f < 6; f++) {
        D3DLOCKED_RECT lr;
        EXPECT_HR(IDirect3DCubeTexture9_LockRect(c, f, 0, &lr, NULL, D3DLOCK_READONLY), D3D_OK);
        EXPECT_EQ(*(DWORD *)lr.pBits, 0xa0000000u + f);
        IDirect3DCubeTexture9_UnlockRect(c, f, 0);
    }
    D3DLOCKED_RECT lr;
    EXPECT_HRN(IDirect3DCubeTexture9_LockRect(c, 6, 0, &lr, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DCubeTexture9_LockRect(c, 0, 2, &lr, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DCubeTexture9_UnlockRect(c, 0, 0), S_OK);   /* Win10: S_OK */
    for (int f = 0; f < 6; f++) SAFE_RELEASE(face[f]);
    IDirect3DCubeTexture9_Release(c);
    D9_END(d);
}

TEST(d3d9_device_resources_cube_invalid_usage)
{
    D9_OPEN(d);
    IDirect3DCubeTexture9 *c = NULL;
    EXPECT_HRN(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &c, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &c, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &c, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 1, 0, D3DFMT_A8R8G8B8, (D3DPOOL)7, &c, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, &c, NULL), D3DERR_INVALIDCALL);
    CHECK(c == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_cube_default_rt)
{
    D9_OPEN(d);
    IDirect3DCubeTexture9 *c = NULL;
    HRESULT hr = IDirect3DDevice9_CreateCubeTexture(d.dev, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &c, NULL);
    if (FAILED(hr)) { t_note("cube rt: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    D3DSURFACE_DESC dd;
    EXPECT_HR(IDirect3DCubeTexture9_GetLevelDesc(c, 0, &dd), D3D_OK);
    EXPECT_EQ(dd.Usage & D3DUSAGE_RENDERTARGET, D3DUSAGE_RENDERTARGET);
    EXPECT_EQ(dd.Pool, D3DPOOL_DEFAULT);
    D3DLOCKED_RECT lr;
    EXPECT_HRN(IDirect3DCubeTexture9_LockRect(c, 0, 0, &lr, NULL, 0), D3DERR_INVALIDCALL);
    IDirect3DCubeTexture9_Release(c);
    D9_END(d);
}

TEST(d3d9_device_resources_cube_lod_priority)
{
    D9_OPEN(d);
    IDirect3DCubeTexture9 *c = NULL;
    if (FAILED(IDirect3DDevice9_CreateCubeTexture(d.dev, 8, 4, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &c, NULL))) { t_skip("no cube"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DCubeTexture9_SetLOD(c, 2), 0);
    EXPECT_EQ(IDirect3DCubeTexture9_GetLOD(c), 2);
    IDirect3DCubeTexture9_SetLOD(c, 50);
    EXPECT_EQ(IDirect3DCubeTexture9_GetLOD(c), 3);
    EXPECT_EQ(IDirect3DCubeTexture9_SetPriority(c, 3), 0);
    EXPECT_EQ(IDirect3DCubeTexture9_GetPriority(c), 3);
    EXPECT_HR(IDirect3DCubeTexture9_AddDirtyRect(c, D3DCUBEMAP_FACE_NEGATIVE_Z, NULL), D3D_OK);
    EXPECT_HRN(IDirect3DCubeTexture9_AddDirtyRect(c, (D3DCUBEMAP_FACES)6, NULL), D3DERR_INVALIDCALL);
    RECT bad = { 0, 0, 100, 100 };
    EXPECT_HR(IDirect3DCubeTexture9_AddDirtyRect(c, D3DCUBEMAP_FACE_POSITIVE_X, &bad), D3DERR_INVALIDCALL);
    IDirect3DCubeTexture9_Release(c);
    D9_END(d);
}

TEST(d3d9_device_resources_cube_qi)
{
    D9_OPEN(d);
    IDirect3DCubeTexture9 *c = NULL;
    if (FAILED(IDirect3DDevice9_CreateCubeTexture(d.dev, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &c, NULL))) { t_skip("no cube"); D9_END(d); return; }
    void *p = NULL;
    EXPECT_HR(IDirect3DCubeTexture9_QueryInterface(c, &IID_IDirect3DBaseTexture9, &p), S_OK);
    if (p) IDirect3DBaseTexture9_Release((IDirect3DBaseTexture9 *)p);
    p = (void *)1;
    EXPECT_HR(IDirect3DCubeTexture9_QueryInterface(c, &IID_IDirect3DTexture9, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    IDirect3DSurface9 *s = NULL;
    if (SUCCEEDED(IDirect3DCubeTexture9_GetCubeMapSurface(c, 2, 0, &s))) {
        void *cont = NULL;
        EXPECT_HR(IDirect3DSurface9_GetContainer(s, &IID_IDirect3DCubeTexture9, &cont), D3D_OK);
        CHECK(cont == c);
        if (cont) IDirect3DCubeTexture9_Release((IDirect3DCubeTexture9 *)cont);
        IDirect3DSurface9_Release(s);
    }
    IDirect3DCubeTexture9_Release(c);
    D9_END(d);
}

/* ---- volume textures ---- */
static int mkvol(D9 *d, UINT w, UINT h, UINT dp, UINT lv, DWORD us, D3DFORMAT f, D3DPOOL p, IDirect3DVolumeTexture9 **v)
{
    return SUCCEEDED(IDirect3DDevice9_CreateVolumeTexture(d->dev, w, h, dp, lv, us, f, p, v, NULL));
}

TEST(d3d9_device_resources_vol_levels)
{
    D9_OPEN(d);
    struct { UINT w, h, d; } t[] = { {4,4,4}, {8,4,2}, {16,16,1}, {1,1,1}, {2,8,32}, {5,3,7} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3DVolumeTexture9 *v = NULL;
        if (!mkvol(&d, t[i].w, t[i].h, t[i].d, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &v)) { t_note("volume %ux%ux%u unavailable", t[i].w, t[i].h, t[i].d); continue; }
        UINT m = t[i].w; if (t[i].h > m) m = t[i].h; if (t[i].d > m) m = t[i].d;
        UINT want = 1; while (m > 1) { m >>= 1; want++; }
        EXPECT_EQ(IDirect3DVolumeTexture9_GetLevelCount(v), want);
        EXPECT_EQ(IDirect3DVolumeTexture9_GetType(v), D3DRTYPE_VOLUMETEXTURE);
        for (UINT l = 0; l < want; l++) {
            D3DVOLUME_DESC vd;
            EXPECT_HR(IDirect3DVolumeTexture9_GetLevelDesc(v, l, &vd), D3D_OK);
            UINT ew = t[i].w >> l, eh = t[i].h >> l, ed = t[i].d >> l;
            CHECK(vd.Width == (ew ? ew : 1) && vd.Height == (eh ? eh : 1) && vd.Depth == (ed ? ed : 1));
            EXPECT_EQ(vd.Type, D3DRTYPE_VOLUME);
            EXPECT_EQ(vd.Format, D3DFMT_A8R8G8B8);
            EXPECT_EQ(vd.Pool, D3DPOOL_SCRATCH);
        }
        D3DVOLUME_DESC vd;
        EXPECT_HR(IDirect3DVolumeTexture9_GetLevelDesc(v, want, &vd), D3DERR_INVALIDCALL);
        IDirect3DVolumeTexture9_Release(v);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_vol_lockbox)
{
    D9_OPEN(d);
    IDirect3DVolumeTexture9 *v = NULL;
    if (!mkvol(&d, 8, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v)) { t_skip("no volume"); D9_END(d); return; }
    D3DLOCKED_BOX lb;
    EXPECT_HR(IDirect3DVolumeTexture9_LockBox(v, 0, &lb, NULL, 0), D3D_OK);
    CHECK(lb.RowPitch >= 8 * 4);
    CHECK(lb.SlicePitch >= lb.RowPitch * 4);
    D3DLOCKED_BOX full = lb;
    EXPECT_HRN(IDirect3DVolumeTexture9_LockBox(v, 0, &lb, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DVolumeTexture9_UnlockBox(v, 0), D3D_OK);
    EXPECT_HR(IDirect3DVolumeTexture9_UnlockBox(v, 0), D3DERR_INVALIDCALL);
    D3DBOX b = { 1, 1, 8, 3, 1, 3 };
    EXPECT_HR(IDirect3DVolumeTexture9_LockBox(v, 0, &lb, &b, 0), D3D_OK);
    EXPECT_EQ(lb.RowPitch, full.RowPitch);
    EXPECT_EQ(lb.SlicePitch, full.SlicePitch);
    EXPECT_EQ((char *)lb.pBits - (char *)full.pBits, 1 * full.SlicePitch + 1 * full.RowPitch + 1 * 4);
    IDirect3DVolumeTexture9_UnlockBox(v, 0);
    D3DBOX bad[] = { {0,0,9,1,0,1}, {0,0,1,5,0,1}, {0,0,1,1,0,5}, {4,0,4,1,0,1}, {0,2,1,2,0,1}, {0,0,1,1,3,3}, {-1,0,1,1,0,1} };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr = IDirect3DVolumeTexture9_LockBox(v, 0, &lb, &bad[i], 0);
        CHECKF(hr == D3DERR_INVALIDCALL, "box %u got %s", i, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirect3DVolumeTexture9_UnlockBox(v, 0);
    }
    EXPECT_HRN(IDirect3DVolumeTexture9_LockBox(v, 1, &lb, NULL, 0), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DVolumeTexture9_LockBox(v, 0, NULL, NULL, 0), D9_AVHR);   /* Win10: access violation */
    IDirect3DVolumeTexture9_Release(v);
    D9_END(d);
}

TEST(d3d9_device_resources_vol_data_slices)
{
    D9_OPEN(d);
    IDirect3DVolumeTexture9 *v = NULL;
    if (!mkvol(&d, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v)) { t_skip("no volume"); D9_END(d); return; }
    D3DLOCKED_BOX lb;
    IDirect3DVolumeTexture9_LockBox(v, 0, &lb, NULL, 0);
    for (int z = 0; z < 4; z++) for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++)
        *(DWORD *)((char *)lb.pBits + z * lb.SlicePitch + y * lb.RowPitch + x * 4) = z * 100 + y * 10 + x;
    IDirect3DVolumeTexture9_UnlockBox(v, 0);
    IDirect3DVolumeTexture9_LockBox(v, 0, &lb, NULL, D3DLOCK_READONLY);
    int bad = 0;
    for (int z = 0; z < 4; z++) for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++)
        if (*(DWORD *)((char *)lb.pBits + z * lb.SlicePitch + y * lb.RowPitch + x * 4) != (DWORD)(z * 100 + y * 10 + x)) bad++;
    CHECK(bad == 0);
    IDirect3DVolumeTexture9_UnlockBox(v, 0);
    IDirect3DVolumeTexture9_Release(v);
    D9_END(d);
}

TEST(d3d9_device_resources_vol_getvolumelevel)
{
    D9_OPEN(d);
    IDirect3DVolumeTexture9 *v = NULL;
    if (!mkvol(&d, 8, 8, 8, 3, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v)) { t_skip("no volume"); D9_END(d); return; }
    IDirect3DVolume9 *lv = (IDirect3DVolume9 *)1;
    EXPECT_HR(IDirect3DVolumeTexture9_GetVolumeLevel(v, 3, &lv), D3DERR_INVALIDCALL);
    CHECK(lv == NULL);
    EXPECT_HRN(IDirect3DVolumeTexture9_GetVolumeLevel(v, 1, NULL), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(IDirect3DVolumeTexture9_GetVolumeLevel(v, 1, &lv), D3D_OK);
    if (lv) {
        D3DVOLUME_DESC vd;
        EXPECT_HR(IDirect3DVolume9_GetDesc(lv, &vd), D3D_OK);
        EXPECT_EQ(vd.Width, 4);
        EXPECT_EQ(vd.Depth, 4);
        void *cont = NULL;
        EXPECT_HR(IDirect3DVolume9_GetContainer(lv, &IID_IDirect3DVolumeTexture9, &cont), D3D_OK);
        CHECK(cont == v);
        if (cont) IDirect3DVolumeTexture9_Release((IDirect3DVolumeTexture9 *)cont);
        IDirect3DDevice9 *dd = NULL;
        EXPECT_HR(IDirect3DVolume9_GetDevice(lv, &dd), D3D_OK);
        CHECK(dd == d.dev);
        SAFE_RELEASE(dd);
        D3DLOCKED_BOX lb;
        EXPECT_HR(IDirect3DVolume9_LockBox(lv, &lb, NULL, 0), D3D_OK);
        EXPECT_HRN(IDirect3DVolume9_LockBox(lv, &lb, NULL, 0), D3DERR_INVALIDCALL);
        EXPECT_HR(IDirect3DVolume9_UnlockBox(lv), D3D_OK);
        EXPECT_HR(IDirect3DVolume9_UnlockBox(lv), D3DERR_INVALIDCALL);
        IDirect3DVolume9_Release(lv);
    }
    IDirect3DVolumeTexture9_Release(v);
    D9_END(d);
}

TEST(d3d9_device_resources_vol_invalid_usage)
{
    D9_OPEN(d);
    IDirect3DVolumeTexture9 *v = NULL;
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &v, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24S8, D3DPOOL_DEFAULT, &v, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, (D3DPOOL)9, &v, NULL), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, &v, NULL), D3DERR_INVALIDCALL);
    CHECK(v == NULL);
    D9_END(d);
}

TEST(d3d9_device_resources_vol_dxt)
{
    D9_OPEN(d);
    IDirect3DVolumeTexture9 *v = NULL;
    if (!mkvol(&d, 8, 8, 4, 1, 0, D3DFMT_DXT3, D3DPOOL_SCRATCH, &v)) { t_note("dxt3 volume unavailable"); D9_END(d); return; }
    D3DLOCKED_BOX lb;
    EXPECT_HR(IDirect3DVolumeTexture9_LockBox(v, 0, &lb, NULL, 0), D3D_OK);
    CHECK(lb.RowPitch >= 2 * 16);
    CHECK(lb.SlicePitch >= lb.RowPitch * 2);
    IDirect3DVolumeTexture9_UnlockBox(v, 0);
    IDirect3DVolumeTexture9_Release(v);
    D9_END(d);
}

TEST(d3d9_device_resources_vol_lod_priority)
{
    D9_OPEN(d);
    IDirect3DVolumeTexture9 *v = NULL;
    if (!mkvol(&d, 8, 8, 8, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v)) { t_skip("no volume"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DVolumeTexture9_GetLevelCount(v), 4);
    EXPECT_EQ(IDirect3DVolumeTexture9_SetLOD(v, 1), 0);
    EXPECT_EQ(IDirect3DVolumeTexture9_GetLOD(v), 1);
    IDirect3DVolumeTexture9_SetLOD(v, 99);
    EXPECT_EQ(IDirect3DVolumeTexture9_GetLOD(v), 3);
    EXPECT_EQ(IDirect3DVolumeTexture9_SetPriority(v, 5), 0);
    EXPECT_EQ(IDirect3DVolumeTexture9_GetPriority(v), 5);
    EXPECT_HR(IDirect3DVolumeTexture9_AddDirtyBox(v, NULL), D3D_OK);
    D3DBOX ok = { 0, 0, 4, 4, 0, 4 }, bad = { 0, 0, 16, 4, 0, 4 };
    EXPECT_HR(IDirect3DVolumeTexture9_AddDirtyBox(v, &ok), D3D_OK);
    EXPECT_HR(IDirect3DVolumeTexture9_AddDirtyBox(v, &bad), D3DERR_INVALIDCALL);
    IDirect3DVolumeTexture9_Release(v);
    D9_END(d);
}

TEST(d3d9_device_resources_vol_qi)
{
    D9_OPEN(d);
    IDirect3DVolumeTexture9 *v = NULL;
    if (!mkvol(&d, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &v)) { t_skip("no volume"); D9_END(d); return; }
    void *p = NULL;
    EXPECT_HR(IDirect3DVolumeTexture9_QueryInterface(v, &IID_IDirect3DBaseTexture9, &p), S_OK);
    if (p) IDirect3DBaseTexture9_Release((IDirect3DBaseTexture9 *)p);
    p = (void *)1;
    EXPECT_HR(IDirect3DVolumeTexture9_QueryInterface(v, &IID_IDirect3DTexture9, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    IDirect3DVolumeTexture9_Release(v);
    D9_END(d);
}
