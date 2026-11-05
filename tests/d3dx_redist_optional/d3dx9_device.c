#include "dx.h"
/* d3dx9_43.dll functionality that needs an IDirect3DDevice9 (NULLREF/HAL/REF via d3dh_make_device9); only
   SCRATCH / SYSTEMMEM resources are used, nothing is ever presented. */

#define INVALIDCALL ((HRESULT)0x8876086C)
#define INVALIDDATA ((HRESULT)0x88760B59)

typedef struct { IDirect3D9 *d3d; IDirect3DDevice9 *dev; HWND hwnd; } DEV9;
static int dev9_open(DEV9 *d) {
    memset(d, 0, sizeof *d);
    D3DDEVTYPE ty;
    HRESULT hr = d3dh_make_device9(&d->d3d, &d->dev, NULL, &d->hwnd, &ty);
    if (FAILED(hr) || !d->dev) { SAFE_RELEASE(d->dev); SAFE_RELEASE(d->d3d); d3dh_destroy_window(d->hwnd); t_skip("no D3D9 device (%s)", d3dh_hr_name(hr)); return 0; }
    return 1;
}
static void dev9_close(DEV9 *d) { SAFE_RELEASE(d->dev); SAFE_RELEASE(d->d3d); d3dh_destroy_window(d->hwnd); }
#define SETUP9(d) DEV9 d; NEED_DLL(X9); if (!dev9_open(&d)) return

/* read a pixel DWORD of a locked level-0 32bpp texture */
static int read_px32(IDirect3DTexture9 *t, UINT lvl, UINT w, UINT h, DWORD *out) {
    D3DLOCKED_RECT lr;
    if (FAILED(IDirect3DTexture9_LockRect(t, lvl, &lr, NULL, D3DLOCK_READONLY))) return 0;
    for (UINT y = 0; y < h; y++) for (UINT x = 0; x < w; x++) out[y * w + x] = ((DWORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x];
    IDirect3DTexture9_UnlockRect(t, lvl);
    return 1;
}

/* ---- D3DXCreateTexture ----------------------------------------------------------- */
TEST(d3dx_redist_optional_x9_create_texture_basic) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = NULL;
    EXPECT_HR(F9(D3DXCreateTexture)(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &t), S_OK);
    if (t) {
        D3DSURFACE_DESC sd; EXPECT_HR(IDirect3DTexture9_GetLevelDesc(t, 0, &sd), S_OK);
        EXPECT_EQ(sd.Width, 4); EXPECT_EQ(sd.Height, 4); EXPECT_EQ(sd.Format, D3DFMT_A8R8G8B8); EXPECT_EQ(sd.Pool, D3DPOOL_SCRATCH);
        EXPECT_EQ(IDirect3DTexture9_GetLevelCount(t), 1);
        CHECK((ULONG)IDirect3DTexture9_Release(t) == 0);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_texture_full_mip_chain) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTexture));
    static const struct { UINT w, h, levels; } t[] = { { 4, 4, 3 }, { 16, 16, 5 }, { 8, 2, 4 }, { 1, 1, 1 }, { 64, 32, 7 } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3DTexture9 *tex = NULL;
        EXPECT_HR(F9(D3DXCreateTexture)(d.dev, t[i].w, t[i].h, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &tex), S_OK);
        if (tex) { CHECKF(IDirect3DTexture9_GetLevelCount(tex) == t[i].levels, "%ux%u levels %lu want %u", t[i].w, t[i].h, IDirect3DTexture9_GetLevelCount(tex), t[i].levels); IDirect3DTexture9_Release(tex); }
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_texture_level_dims) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *tex = NULL;
    EXPECT_HR(F9(D3DXCreateTexture)(d.dev, 16, 4, 0, 0, D3DFMT_X8R8G8B8, D3DPOOL_SCRATCH, &tex), S_OK);
    if (tex) {
        static const UINT w[] = { 16, 8, 4 }, h[] = { 4, 2, 1 };
        for (UINT l = 0; l < 3; l++) { D3DSURFACE_DESC sd; EXPECT_HR(IDirect3DTexture9_GetLevelDesc(tex, l, &sd), S_OK); EXPECT_EQ(sd.Width, w[l]); EXPECT_EQ(sd.Height, h[l]); }
        IDirect3DTexture9_Release(tex);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_texture_formats) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTexture));
    static const D3DFORMAT f[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A1R5G5B5, D3DFMT_A4R4G4B4, D3DFMT_A8, D3DFMT_L8, D3DFMT_A8L8, D3DFMT_R16F, D3DFMT_A16B16G16R16F, D3DFMT_A32B32G32R32F, D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5, D3DFMT_V8U8, D3DFMT_G16R16 };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        IDirect3DTexture9 *tex = NULL;
        HRESULT hr = F9(D3DXCreateTexture)(d.dev, 8, 8, 1, 0, f[i], D3DPOOL_SCRATCH, &tex);   /* scratch pool is not limited by device caps */
        CHECKF(SUCCEEDED(hr), "format %d hr %s", f[i], d3dh_hr_name(hr));
        if (tex) { D3DSURFACE_DESC sd; IDirect3DTexture9_GetLevelDesc(tex, 0, &sd); CHECKF(sd.Format == f[i], "format %d got %d", f[i], sd.Format); IDirect3DTexture9_Release(tex); }
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_texture_invalid) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = NULL;
    EXPECT_HR(F9(D3DXCreateTexture)(NULL, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &t), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateTexture)(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, NULL), INVALIDCALL);
    CHECK(t == NULL);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_texture_non_pow2) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = NULL;
    HRESULT hr = F9(D3DXCreateTexture)(d.dev, 5, 3, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &t);
    CHECK(SUCCEEDED(hr));
    if (t) { D3DSURFACE_DESC sd; IDirect3DTexture9_GetLevelDesc(t, 0, &sd); t_note("5x3 scratch texture created as %ux%u", sd.Width, sd.Height); IDirect3DTexture9_Release(t); }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_texture_zero_size) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = NULL;
    HRESULT hr = F9(D3DXCreateTexture)(d.dev, 0, 0, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &t);
    t_note("D3DXCreateTexture(0x0) -> %s", d3dh_hr_name(hr));
    if (t) { D3DSURFACE_DESC sd; IDirect3DTexture9_GetLevelDesc(t, 0, &sd); t_note("... gave %ux%u", sd.Width, sd.Height); IDirect3DTexture9_Release(t); }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_cube_texture) {
    SETUP9(d); REQUIRE(F9(D3DXCreateCubeTexture));
    IDirect3DCubeTexture9 *c = NULL;
    EXPECT_HR(F9(D3DXCreateCubeTexture)(d.dev, 8, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &c), S_OK);
    if (c) { EXPECT_EQ(IDirect3DCubeTexture9_GetLevelCount(c), 4); D3DSURFACE_DESC sd; IDirect3DCubeTexture9_GetLevelDesc(c, 0, &sd); EXPECT_EQ(sd.Width, 8); EXPECT_EQ(sd.Height, 8); IDirect3DCubeTexture9_Release(c); }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_volume_texture) {
    SETUP9(d); REQUIRE(F9(D3DXCreateVolumeTexture));
    IDirect3DVolumeTexture9 *v = NULL;
    EXPECT_HR(F9(D3DXCreateVolumeTexture)(d.dev, 4, 4, 4, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &v), S_OK);
    if (v) { EXPECT_EQ(IDirect3DVolumeTexture9_GetLevelCount(v), 3); D3DVOLUME_DESC vd; IDirect3DVolumeTexture9_GetLevelDesc(v, 0, &vd); EXPECT_EQ(vd.Depth, 4); IDirect3DVolumeTexture9_Release(v); }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_check_texture_requirements) {
    SETUP9(d); REQUIRE(F9(D3DXCheckTextureRequirements));
    UINT w = 16, h = 8, m = 0; D3DFORMAT f = D3DFMT_A8R8G8B8;
    EXPECT_HR(F9(D3DXCheckTextureRequirements)(d.dev, &w, &h, &m, 0, &f, D3DPOOL_SCRATCH), S_OK);
    EXPECT_EQ(w, 16); EXPECT_EQ(h, 8); EXPECT_EQ(m, 5); EXPECT_EQ(f, D3DFMT_A8R8G8B8);
    w = 16; h = 8; m = 1; f = D3DFMT_A8R8G8B8;
    EXPECT_HR(F9(D3DXCheckTextureRequirements)(d.dev, &w, &h, &m, 0, &f, D3DPOOL_SCRATCH), S_OK);
    EXPECT_EQ(m, 1);
    w = 16; h = 16; m = 99; f = D3DFMT_A8R8G8B8;   /* a request for more levels than exist is clamped */
    EXPECT_HR(F9(D3DXCheckTextureRequirements)(d.dev, &w, &h, &m, 0, &f, D3DPOOL_SCRATCH), S_OK);
    EXPECT_EQ(m, 5);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_check_texture_requirements_defaults) {
    SETUP9(d); REQUIRE(F9(D3DXCheckTextureRequirements));
    UINT w = D3DX_DEFAULT, h = D3DX_DEFAULT, m = D3DX_DEFAULT; D3DFORMAT f = D3DFMT_UNKNOWN;
    HRESULT hr = F9(D3DXCheckTextureRequirements)(d.dev, &w, &h, &m, 0, &f, D3DPOOL_SCRATCH);
    t_note("all-default requirements: %s w=%u h=%u mips=%u fmt=%d", d3dh_hr_name(hr), w, h, m, f);
    dev9_close(&d);
}

/* ---- textures from memory ---------------------------------------------------------- */
static IDirect3DTexture9 *tex_from(DEV9 *d, const void *data, UINT n, UINT w, UINT h, UINT mips, D3DFORMAT fmt, D3DXIMAGE_INFO *info, HRESULT *hr_out) {
    IDirect3DTexture9 *t = NULL;
    HRESULT hr = F9(D3DXCreateTextureFromFileInMemoryEx)(d->dev, data, n, w, h, mips, 0, fmt, D3DPOOL_SCRATCH, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, info, NULL, &t);
    if (hr_out) *hr_out = hr;
    return t;
}
static const DWORD RGBW[4] = { 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFFFF };
static void check_rgbw(const void *data, UINT n, const char *what) {
    DEV9 d; if (!dev9_open(&d)) return;
    HRESULT hr; D3DXIMAGE_INFO info;
    IDirect3DTexture9 *t = tex_from(&d, data, n, 2, 2, 1, D3DFMT_A8R8G8B8, &info, &hr);
    CHECKF(SUCCEEDED(hr) && t, "%s load hr %s", what, d3dh_hr_name(hr));
    if (t) {
        D3DSURFACE_DESC sd; IDirect3DTexture9_GetLevelDesc(t, 0, &sd); EXPECT_EQ(sd.Width, 2); EXPECT_EQ(sd.Height, 2); EXPECT_EQ(sd.Format, D3DFMT_A8R8G8B8);
        DWORD px[4] = {0};
        CHECK(read_px32(t, 0, 2, 2, px));
        for (int i = 0; i < 4; i++) CHECKF(px[i] == RGBW[i], "%s pixel %d got 0x%08lx want 0x%08lx", what, i, px[i], RGBW[i]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_texture_from_memory_png) { NEED_DLL(X9); REQUIRE(F9(D3DXCreateTextureFromFileInMemoryEx)); check_rgbw(dxa_png_2x2, sizeof dxa_png_2x2, "png"); }
TEST(d3dx_redist_optional_x9_texture_from_memory_bmp) { NEED_DLL(X9); REQUIRE(F9(D3DXCreateTextureFromFileInMemoryEx)); check_rgbw(dxa_bmp24_2x2, sizeof dxa_bmp24_2x2, "bmp"); }
TEST(d3dx_redist_optional_x9_texture_from_memory_tga) { NEED_DLL(X9); REQUIRE(F9(D3DXCreateTextureFromFileInMemoryEx)); check_rgbw(dxa_tga32_2x2, sizeof dxa_tga32_2x2, "tga"); }
TEST(d3dx_redist_optional_x9_texture_from_memory_dds_argb) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTextureFromFileInMemoryEx));
    HRESULT hr; IDirect3DTexture9 *t = tex_from(&d, dxa_dds_argb_4x4, sizeof dxa_dds_argb_4x4, D3DX_FROM_FILE, D3DX_FROM_FILE, D3DX_FROM_FILE, D3DFMT_FROM_FILE, NULL, &hr);
    EXPECT_HR(hr, S_OK);
    if (t) {
        D3DSURFACE_DESC sd; IDirect3DTexture9_GetLevelDesc(t, 0, &sd);
        EXPECT_EQ(sd.Width, 4); EXPECT_EQ(sd.Height, 4); EXPECT_EQ(sd.Format, D3DFMT_A8R8G8B8); EXPECT_EQ(IDirect3DTexture9_GetLevelCount(t), 1);
        DWORD px[16]; CHECK(read_px32(t, 0, 4, 4, px));
        for (int i = 0; i < 16; i++) CHECKF(px[i] == 0xFF336699, "pixel %d = 0x%08lx", i, px[i]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_texture_from_memory_dds_dxt1_native) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTextureFromFileInMemoryEx));
    D3DXIMAGE_INFO info; HRESULT hr;
    IDirect3DTexture9 *t = tex_from(&d, dxa_dds_dxt1_8x8_mips, sizeof dxa_dds_dxt1_8x8_mips, D3DX_FROM_FILE, D3DX_FROM_FILE, D3DX_FROM_FILE, D3DFMT_FROM_FILE, &info, &hr);
    EXPECT_HR(hr, S_OK);
    if (t) {
        D3DSURFACE_DESC sd; IDirect3DTexture9_GetLevelDesc(t, 0, &sd);
        EXPECT_EQ(sd.Format, D3DFMT_DXT1); EXPECT_EQ(sd.Width, 8); EXPECT_EQ(IDirect3DTexture9_GetLevelCount(t), 4);
        EXPECT_EQ(info.MipLevels, 4); EXPECT_EQ(info.ImageFileFormat, D3DXIFF_DDS);
        IDirect3DTexture9_GetLevelDesc(t, 3, &sd); EXPECT_EQ(sd.Width, 1); EXPECT_EQ(sd.Height, 1);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_texture_from_memory_dds_dxt1_decompress) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTextureFromFileInMemoryEx));
    HRESULT hr; IDirect3DTexture9 *t = tex_from(&d, dxa_dds_dxt1_4x4, sizeof dxa_dds_dxt1_4x4, 4, 4, 1, D3DFMT_A8R8G8B8, NULL, &hr);
    EXPECT_HR(hr, S_OK);
    if (t) {
        DWORD px[16]; CHECK(read_px32(t, 0, 4, 4, px));
        for (int i = 0; i < 16; i++) CHECKF(px[i] == 0xFFFFFFFF, "pixel %d = 0x%08lx", i, px[i]);   /* block: color0 = white, all indices 0 */
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_texture_from_memory_info_out) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTextureFromFileInMemoryEx));
    D3DXIMAGE_INFO info; HRESULT hr;
    IDirect3DTexture9 *t = tex_from(&d, dxa_png_2x2, sizeof dxa_png_2x2, 4, 4, 1, D3DFMT_X8R8G8B8, &info, &hr);   /* source info describes the file, not the texture */
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(info.Width, 2); EXPECT_EQ(info.Height, 2); EXPECT_EQ(info.ImageFileFormat, D3DXIFF_PNG);
    if (t) { D3DSURFACE_DESC sd; IDirect3DTexture9_GetLevelDesc(t, 0, &sd); EXPECT_EQ(sd.Width, 4); EXPECT_EQ(sd.Format, D3DFMT_X8R8G8B8); IDirect3DTexture9_Release(t); }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_texture_from_memory_invalid) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTextureFromFileInMemory));
    IDirect3DTexture9 *t = NULL; static const unsigned char junk[32] = { 9, 8, 7 };
    EXPECT_HR(F9(D3DXCreateTextureFromFileInMemory)(d.dev, junk, sizeof junk, &t), INVALIDDATA);
    CHECK(t == NULL);
    EXPECT_HR(F9(D3DXCreateTextureFromFileInMemory)(d.dev, NULL, 0, &t), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateTextureFromFileInMemory)(NULL, dxa_png_2x2, sizeof dxa_png_2x2, &t), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateTextureFromFileInMemory)(d.dev, dxa_png_2x2, sizeof dxa_png_2x2, NULL), INVALIDCALL);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_texture_from_memory_default_pool) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTextureFromFileInMemory));
    IDirect3DTexture9 *t = NULL;
    HRESULT hr = F9(D3DXCreateTextureFromFileInMemory)(d.dev, dxa_png_2x2, sizeof dxa_png_2x2, &t);
    t_note("D3DXCreateTextureFromFileInMemory (defaults) -> %s", d3dh_hr_name(hr));
    if (t) { D3DSURFACE_DESC sd; IDirect3DTexture9_GetLevelDesc(t, 0, &sd); t_note("default texture: %ux%u fmt %d pool %d levels %lu", sd.Width, sd.Height, sd.Format, sd.Pool, IDirect3DTexture9_GetLevelCount(t)); IDirect3DTexture9_Release(t); }
    dev9_close(&d);
}

/* ---- surface loads / conversions ------------------------------------------------------ */
static IDirect3DTexture9 *mk_tex(DEV9 *d, UINT w, UINT h, D3DFORMAT f) {
    IDirect3DTexture9 *t = NULL; HRESULT hr = F9(D3DXCreateTexture)(d->dev, w, h, 1, 0, f, D3DPOOL_SCRATCH, &t);
    if (FAILED(hr)) { t_note("scratch texture fmt %d: %s", f, d3dh_hr_name(hr)); return NULL; }
    return t;
}
static HRESULT load_surf(IDirect3DTexture9 *t, const void *src, D3DFORMAT sf, UINT pitch, const RECT *srect, const RECT *drect, DWORD filter, D3DCOLOR key) {
    IDirect3DSurface9 *s = NULL; IDirect3DTexture9_GetSurfaceLevel(t, 0, &s);
    HRESULT hr = F9(D3DXLoadSurfaceFromMemory)(s, NULL, drect, src, sf, pitch, NULL, srect, filter, key);
    IDirect3DSurface9_Release(s); return hr;
}
static int read_u16(IDirect3DTexture9 *t, UINT w, UINT h, WORD *out) {
    D3DLOCKED_RECT lr; if (FAILED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, D3DLOCK_READONLY))) return 0;
    for (UINT y = 0; y < h; y++) for (UINT x = 0; x < w; x++) out[y * w + x] = ((WORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x];
    IDirect3DTexture9_UnlockRect(t, 0); return 1;
}
TEST(d3dx_redist_optional_x9_load_surface_argb_copy) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 2, 2, D3DFMT_A8R8G8B8);
    if (t) {
        DWORD src[4] = { 0x80102030, 0xFF405060, 0x00708090, 0x12345678 }, px[4];
        RECT r = { 0, 0, 2, 2 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 8, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_px32(t, 0, 2, 2, px));
        for (int i = 0; i < 4; i++) CHECKF(px[i] == src[i], "pixel %d got 0x%08lx want 0x%08lx", i, px[i], src[i]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_argb_to_x8r8g8b8) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 2, 1, D3DFMT_X8R8G8B8);
    if (t) {
        DWORD src[2] = { 0x80102030, 0x00FFEEDD }, px[2]; RECT r = { 0, 0, 2, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 8, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_px32(t, 0, 2, 1, px));
        EXPECT_EQ(px[0] & 0xFFFFFF, 0x102030); EXPECT_EQ(px[1] & 0xFFFFFF, 0xFFEEDD);   /* top byte is unspecified for X formats */
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_x8r8g8b8_to_argb_alpha_opaque) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 2, 1, D3DFMT_A8R8G8B8);
    if (t) {
        DWORD src[2] = { 0x00102030, 0x00FFEEDD }, px[2]; RECT r = { 0, 0, 2, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_X8R8G8B8, 8, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_px32(t, 0, 2, 1, px));
        EXPECT_EQ(px[0], 0xFF102030); EXPECT_EQ(px[1], 0xFFFFEEDD);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_argb_to_r5g6b5) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 5, 1, D3DFMT_R5G6B5);
    if (t) {
        DWORD src[5] = { 0xFFFFFFFF, 0xFF000000, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF }; WORD px[5]; RECT r = { 0, 0, 5, 1 };
        static const WORD want[5] = { 0xFFFF, 0x0000, 0xF800, 0x07E0, 0x001F };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 20, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_u16(t, 5, 1, px));
        for (int i = 0; i < 5; i++) CHECKF(px[i] == want[i], "pixel %d got 0x%04x want 0x%04x", i, px[i], want[i]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_r5g6b5_to_argb) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 5, 1, D3DFMT_A8R8G8B8);
    if (t) {
        WORD src[5] = { 0xFFFF, 0x0000, 0xF800, 0x07E0, 0x001F }; DWORD px[5]; RECT r = { 0, 0, 5, 1 };
        static const DWORD want[5] = { 0xFFFFFFFF, 0xFF000000, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF };
        EXPECT_HR(load_surf(t, src, D3DFMT_R5G6B5, 10, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_px32(t, 0, 5, 1, px));
        for (int i = 0; i < 5; i++) CHECKF(px[i] == want[i], "pixel %d got 0x%08lx want 0x%08lx", i, px[i], want[i]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_argb_to_a1r5g5b5) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 4, 1, D3DFMT_A1R5G5B5);
    if (t) {
        DWORD src[4] = { 0xFFFFFFFF, 0x00FFFFFF, 0xFFFF0000, 0x00000000 }; WORD px[4]; RECT r = { 0, 0, 4, 1 };
        static const WORD want[4] = { 0xFFFF, 0x7FFF, 0xFC00, 0x0000 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 16, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_u16(t, 4, 1, px));
        for (int i = 0; i < 4; i++) CHECKF(px[i] == want[i], "pixel %d got 0x%04x want 0x%04x", i, px[i], want[i]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_argb_to_a4r4g4b4_extremes) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 4, 1, D3DFMT_A4R4G4B4);
    if (t) {
        DWORD src[4] = { 0xFFFFFFFF, 0x00000000, 0xFFFF0000, 0x00FF00FF }; WORD px[4]; RECT r = { 0, 0, 4, 1 };
        static const WORD want[4] = { 0xFFFF, 0x0000, 0xFF00, 0x0F0F };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 16, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_u16(t, 4, 1, px));
        for (int i = 0; i < 4; i++) CHECKF(px[i] == want[i], "pixel %d got 0x%04x want 0x%04x", i, px[i], want[i]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_argb_to_a8) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 4, 1, D3DFMT_A8);
    if (t) {
        DWORD src[4] = { 0xFF102030, 0x80FFFFFF, 0x00000000, 0x7F112233 }; RECT r = { 0, 0, 4, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 16, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, D3DLOCK_READONLY))) {
            const BYTE *b = lr.pBits; EXPECT_EQ(b[0], 0xFF); EXPECT_EQ(b[1], 0x80); EXPECT_EQ(b[2], 0x00); EXPECT_EQ(b[3], 0x7F);
            IDirect3DTexture9_UnlockRect(t, 0);
        }
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_a8_to_argb) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 2, 1, D3DFMT_A8R8G8B8);
    if (t) {
        BYTE src[2] = { 0x40, 0xFF }; DWORD px[2]; RECT r = { 0, 0, 2, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8, 2, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_px32(t, 0, 2, 1, px));
        EXPECT_EQ(px[0] >> 24, 0x40); EXPECT_EQ(px[1] >> 24, 0xFF);
        t_note("A8 -> A8R8G8B8 colour channels: 0x%06lx 0x%06lx (expected 0)", px[0] & 0xFFFFFF, px[1] & 0xFFFFFF);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_l8_to_argb) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 3, 1, D3DFMT_A8R8G8B8);
    if (t) {
        BYTE src[3] = { 0x00, 0x80, 0xFF }; DWORD px[3]; RECT r = { 0, 0, 3, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_L8, 3, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_px32(t, 0, 3, 1, px));
        EXPECT_EQ(px[0], 0xFF000000); EXPECT_EQ(px[1], 0xFF808080); EXPECT_EQ(px[2], 0xFFFFFFFF);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_argb_to_l8) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 3, 1, D3DFMT_L8);
    if (t) {
        DWORD src[3] = { 0xFF000000, 0xFFFFFFFF, 0xFF808080 }; RECT r = { 0, 0, 3, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 12, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, D3DLOCK_READONLY))) {
            const BYTE *b = lr.pBits; EXPECT_EQ(b[0], 0x00); EXPECT_EQ(b[1], 0xFF); CHECKF(b[2] >= 0x7F && b[2] <= 0x81, "grey -> L8 0x%02x", b[2]);
            IDirect3DTexture9_UnlockRect(t, 0);
        }
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_float_formats) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 1, 1, D3DFMT_A32B32G32R32F);
    if (t) {
        DWORD src[1] = { 0x80FF8000 }; RECT r = { 0, 0, 1, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 4, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, D3DLOCK_READONLY))) {
            const float *f = lr.pBits;   /* component order in memory: R G B A */
            CHECKNE(f[0], 1.f, 1e-6f); CHECKNE(f[1], 128.f / 255, 1e-6f); CHECKNE(f[2], 0.f, 1e-6f); CHECKNE(f[3], 128.f / 255, 1e-6f);
            IDirect3DTexture9_UnlockRect(t, 0);
        }
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_a16b16g16r16f) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 1, 1, D3DFMT_A16B16G16R16F);
    if (t) {
        DWORD src[1] = { 0xFFFF0000 }; RECT r = { 0, 0, 1, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 4, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, D3DLOCK_READONLY))) {
            const WORD *h = lr.pBits; EXPECT_EQ(h[0], 0x3C00); EXPECT_EQ(h[1], 0); EXPECT_EQ(h[2], 0); EXPECT_EQ(h[3], 0x3C00);
            IDirect3DTexture9_UnlockRect(t, 0);
        }
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_point_upscale) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 4, 4, D3DFMT_A8R8G8B8);
    if (t) {
        RECT r = { 0, 0, 2, 2 };
        EXPECT_HR(load_surf(t, RGBW, D3DFMT_A8R8G8B8, 8, &r, NULL, D3DX_FILTER_POINT, 0), S_OK);
        DWORD px[16]; CHECK(read_px32(t, 0, 4, 4, px));
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) CHECKF(px[y * 4 + x] == RGBW[(y / 2) * 2 + x / 2], "(%d,%d) = 0x%08lx", x, y, px[y * 4 + x]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_src_rect) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 1, 1, D3DFMT_A8R8G8B8);
    if (t) {
        RECT r = { 1, 1, 2, 2 };
        EXPECT_HR(load_surf(t, RGBW, D3DFMT_A8R8G8B8, 8, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        DWORD px[1]; CHECK(read_px32(t, 0, 1, 1, px)); EXPECT_EQ(px[0], 0xFFFFFFFF);
        RECT r2 = { 0, 1, 1, 2 };
        EXPECT_HR(load_surf(t, RGBW, D3DFMT_A8R8G8B8, 8, &r2, NULL, D3DX_FILTER_NONE, 0), S_OK);
        CHECK(read_px32(t, 0, 1, 1, px)); EXPECT_EQ(px[0], 0xFF0000FF);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_dest_rect) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 4, 4, D3DFMT_A8R8G8B8);
    if (t) {
        DWORD z[16] = {0}; RECT all = { 0, 0, 4, 4 };
        EXPECT_HR(load_surf(t, z, D3DFMT_A8R8G8B8, 16, &all, NULL, D3DX_FILTER_NONE, 0), S_OK);
        RECT s = { 0, 0, 2, 2 }, dst = { 1, 1, 3, 3 };
        EXPECT_HR(load_surf(t, RGBW, D3DFMT_A8R8G8B8, 8, &s, &dst, D3DX_FILTER_NONE, 0), S_OK);
        DWORD px[16]; CHECK(read_px32(t, 0, 4, 4, px));
        EXPECT_EQ(px[1 * 4 + 1], RGBW[0]); EXPECT_EQ(px[1 * 4 + 2], RGBW[1]); EXPECT_EQ(px[2 * 4 + 1], RGBW[2]); EXPECT_EQ(px[2 * 4 + 2], RGBW[3]);
        EXPECT_EQ(px[0], 0); EXPECT_EQ(px[15], 0); EXPECT_EQ(px[3], 0);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_color_key) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 2, 1, D3DFMT_A8R8G8B8);
    if (t) {
        DWORD src[2] = { 0xFFFF00FF, 0xFF112233 }, px[2]; RECT r = { 0, 0, 2, 1 };
        EXPECT_HR(load_surf(t, src, D3DFMT_A8R8G8B8, 8, &r, NULL, D3DX_FILTER_NONE, 0xFFFF00FF), S_OK);
        CHECK(read_px32(t, 0, 2, 1, px));
        EXPECT_EQ(px[0] >> 24, 0); EXPECT_EQ(px[1], 0xFF112233);   /* keyed pixel becomes transparent */
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_invalid) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 2, 2, D3DFMT_A8R8G8B8);
    if (t) {
        IDirect3DSurface9 *s = NULL; IDirect3DTexture9_GetSurfaceLevel(t, 0, &s);
        RECT r = { 0, 0, 2, 2 };
        EXPECT_HR(F9(D3DXLoadSurfaceFromMemory)(NULL, NULL, NULL, RGBW, D3DFMT_A8R8G8B8, 8, NULL, &r, D3DX_FILTER_NONE, 0), INVALIDCALL);
        EXPECT_HR(F9(D3DXLoadSurfaceFromMemory)(s, NULL, NULL, NULL, D3DFMT_A8R8G8B8, 8, NULL, &r, D3DX_FILTER_NONE, 0), INVALIDCALL);
        EXPECT_HR(F9(D3DXLoadSurfaceFromMemory)(s, NULL, NULL, RGBW, D3DFMT_A8R8G8B8, 8, NULL, NULL, D3DX_FILTER_NONE, 0), INVALIDCALL);   /* source rect is mandatory */
        RECT empty = { 1, 1, 1, 1 }, bad = { 0, 0, 3, 3 }, neg = { 2, 2, 1, 1 };
        HRESULT hr;
        hr = F9(D3DXLoadSurfaceFromMemory)(s, NULL, NULL, RGBW, D3DFMT_A8R8G8B8, 8, NULL, &empty, D3DX_FILTER_NONE, 0); t_note("empty src rect -> %s", d3dh_hr_name(hr));
        hr = F9(D3DXLoadSurfaceFromMemory)(s, NULL, NULL, RGBW, D3DFMT_A8R8G8B8, 8, NULL, &neg, D3DX_FILTER_NONE, 0); CHECK(FAILED(hr)); t_note("inverted src rect -> %s", d3dh_hr_name(hr));
        RECT dbad = { 0, 0, 3, 3 };
        hr = F9(D3DXLoadSurfaceFromMemory)(s, NULL, &dbad, RGBW, D3DFMT_A8R8G8B8, 8, NULL, &r, D3DX_FILTER_NONE, 0); CHECK(FAILED(hr)); t_note("dest rect beyond surface -> %s", d3dh_hr_name(hr));
        (void)bad;
        IDirect3DSurface9_Release(s); IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_from_surface) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromSurface)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *a = mk_tex(&d, 2, 2, D3DFMT_A8R8G8B8), *b = mk_tex(&d, 2, 2, D3DFMT_R5G6B5);
    if (a && b) {
        RECT r = { 0, 0, 2, 2 };
        EXPECT_HR(load_surf(a, RGBW, D3DFMT_A8R8G8B8, 8, &r, NULL, D3DX_FILTER_NONE, 0), S_OK);
        IDirect3DSurface9 *sa = NULL, *sb = NULL; IDirect3DTexture9_GetSurfaceLevel(a, 0, &sa); IDirect3DTexture9_GetSurfaceLevel(b, 0, &sb);
        EXPECT_HR(F9(D3DXLoadSurfaceFromSurface)(sb, NULL, NULL, sa, NULL, NULL, D3DX_FILTER_NONE, 0), S_OK);
        WORD px[4]; CHECK(read_u16(b, 2, 2, px));
        EXPECT_EQ(px[0], 0xF800); EXPECT_EQ(px[1], 0x07E0); EXPECT_EQ(px[2], 0x001F); EXPECT_EQ(px[3], 0xFFFF);
        IDirect3DSurface9_Release(sa); IDirect3DSurface9_Release(sb);
    }
    if (a) IDirect3DTexture9_Release(a); if (b) IDirect3DTexture9_Release(b);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_load_surface_from_file_in_memory) {
    SETUP9(d); REQUIRE(F9(D3DXLoadSurfaceFromFileInMemory)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 2, 2, D3DFMT_A8R8G8B8);
    if (t) {
        IDirect3DSurface9 *s = NULL; IDirect3DTexture9_GetSurfaceLevel(t, 0, &s);
        D3DXIMAGE_INFO info;
        EXPECT_HR(F9(D3DXLoadSurfaceFromFileInMemory)(s, NULL, NULL, dxa_png_2x2, sizeof dxa_png_2x2, NULL, D3DX_FILTER_NONE, 0, &info), S_OK);
        EXPECT_EQ(info.ImageFileFormat, D3DXIFF_PNG);
        DWORD px[4]; CHECK(read_px32(t, 0, 2, 2, px));
        for (int i = 0; i < 4; i++) CHECKF(px[i] == RGBW[i], "pixel %d 0x%08lx", i, px[i]);
        EXPECT_HR(F9(D3DXLoadSurfaceFromFileInMemory)(s, NULL, NULL, "garbage-garbage-garbage", 23, NULL, D3DX_FILTER_NONE, 0, NULL), INVALIDDATA);
        IDirect3DSurface9_Release(s); IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
static int g_fill_calls;
static void WINAPI fill_red(D3DXVECTOR4 *out, const D3DXVECTOR2 *tc, const D3DXVECTOR2 *ts, void *data) {
    g_fill_calls++; out->x = 1.f; out->y = 0.f; out->z = 0.f; out->w = 1.f;
}
TEST(d3dx_redist_optional_x9_fill_texture_callback) {
    SETUP9(d); REQUIRE(F9(D3DXFillTexture)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 4, 4, D3DFMT_A8R8G8B8);
    if (t) {
        g_fill_calls = 0;
        EXPECT_HR(F9(D3DXFillTexture)(t, fill_red, NULL), S_OK);
        EXPECT_EQ(g_fill_calls, 16);
        DWORD px[16]; CHECK(read_px32(t, 0, 4, 4, px));
        for (int i = 0; i < 16; i++) CHECKF(px[i] == 0xFFFF0000, "pixel %d 0x%08lx", i, px[i]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
static void WINAPI fill_coords(D3DXVECTOR4 *out, const D3DXVECTOR2 *tc, const D3DXVECTOR2 *ts, void *data) {
    float *rec = data; rec[0] = tc->x; rec[1] = tc->y; rec[2] = ts->x; rec[3] = ts->y; out->x = out->y = out->z = out->w = 0;
}
TEST(d3dx_redist_optional_x9_fill_texture_coordinates) {
    SETUP9(d); REQUIRE(F9(D3DXFillTexture)); REQUIRE(F9(D3DXCreateTexture));
    IDirect3DTexture9 *t = mk_tex(&d, 4, 2, D3DFMT_A8R8G8B8);
    if (t) {
        float rec[4]; EXPECT_HR(F9(D3DXFillTexture)(t, fill_coords, rec), S_OK);
        CHECKN(rec[2], 0.25f); CHECKN(rec[3], 0.5f);                 /* texel size = 1/width, 1/height */
        t_note("last texcoord (%g, %g) for a 4x2 texture (texel-centre convention would be 0.875, 0.75)", rec[0], rec[1]);
        IDirect3DTexture9_Release(t);
    }
    dev9_close(&d);
}
/* ---- meshes ------------------------------------------------------------------------- */
typedef struct { float x, y, z; } V3;
TEST(d3dx_redist_optional_x9_create_mesh_fvf_basic) {
    SETUP9(d); REQUIRE(F9(D3DXCreateMeshFVF));
    ID3DXMesh *m = NULL;
    EXPECT_HR(F9(D3DXCreateMeshFVF)(2, 4, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ, d.dev, &m), S_OK);
    if (m) {
        EXPECT_EQ(ID3DXMesh_GetNumFaces(m), 2); EXPECT_EQ(ID3DXMesh_GetNumVertices(m), 4);
        EXPECT_EQ(ID3DXMesh_GetFVF(m), D3DFVF_XYZ); EXPECT_EQ(ID3DXMesh_GetNumBytesPerVertex(m), 12);
        t_note("mesh options 0x%lx", ID3DXMesh_GetOptions(m));
        CHECK((ID3DXMesh_GetOptions(m) & D3DXMESH_SYSTEMMEM) == D3DXMESH_SYSTEMMEM);
        IDirect3DDevice9 *dd = NULL; EXPECT_HR(ID3DXMesh_GetDevice(m, &dd), S_OK); EXPECT_EQ(dd, d.dev); if (dd) IDirect3DDevice9_Release(dd);
        V3 *vb = NULL; WORD *ib = NULL;
        EXPECT_HR(ID3DXMesh_LockVertexBuffer(m, 0, (void **)&vb), S_OK);
        if (vb) { vb[0] = (V3){0,0,0}; vb[1] = (V3){1,0,0}; vb[2] = (V3){0,1,0}; vb[3] = (V3){1,1,0}; ID3DXMesh_UnlockVertexBuffer(m); }
        EXPECT_HR(ID3DXMesh_LockIndexBuffer(m, 0, (void **)&ib), S_OK);
        if (ib) { WORD idx[6] = {0,1,2,2,1,3}; memcpy(ib, idx, sizeof idx); ID3DXMesh_UnlockIndexBuffer(m); }
        IDirect3DIndexBuffer9 *ibo = NULL;
        if (SUCCEEDED(ID3DXMesh_GetIndexBuffer(m, &ibo)) && ibo) { D3DINDEXBUFFER_DESC id; IDirect3DIndexBuffer9_GetDesc(ibo, &id); EXPECT_EQ(id.Format, D3DFMT_INDEX16); EXPECT_EQ(id.Size, 12); IDirect3DIndexBuffer9_Release(ibo); }
        CHECK((ULONG)ID3DXMesh_Release(m) == 0);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_mesh_fvf_32bit_indices) {
    SETUP9(d); REQUIRE(F9(D3DXCreateMeshFVF));
    ID3DXMesh *m = NULL;
    HRESULT hr = F9(D3DXCreateMeshFVF)(2, 4, D3DXMESH_SYSTEMMEM | D3DXMESH_32BIT, D3DFVF_XYZ | D3DFVF_NORMAL, d.dev, &m);
    EXPECT_HR(hr, S_OK);
    if (m) {
        CHECK((ID3DXMesh_GetOptions(m) & D3DXMESH_32BIT) != 0); EXPECT_EQ(ID3DXMesh_GetNumBytesPerVertex(m), 24);
        IDirect3DIndexBuffer9 *ibo = NULL;
        if (SUCCEEDED(ID3DXMesh_GetIndexBuffer(m, &ibo)) && ibo) { D3DINDEXBUFFER_DESC id; IDirect3DIndexBuffer9_GetDesc(ibo, &id); EXPECT_EQ(id.Format, D3DFMT_INDEX32); EXPECT_EQ(id.Size, 24); IDirect3DIndexBuffer9_Release(ibo); }
        ID3DXMesh_Release(m);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_mesh_declaration) {
    SETUP9(d); REQUIRE(F9(D3DXCreateMeshFVF));
    ID3DXMesh *m = NULL;
    if (FAILED(F9(D3DXCreateMeshFVF)(1, 3, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1, d.dev, &m))) { t_skip("mesh create failed"); dev9_close(&d); return; }
    D3DVERTEXELEMENT9 decl[MAX_FVF_DECL_SIZE];
    EXPECT_HR(ID3DXMesh_GetDeclaration(m, decl), S_OK);
    EXPECT_EQ(decl[0].Usage, D3DDECLUSAGE_POSITION); EXPECT_EQ(decl[1].Usage, D3DDECLUSAGE_NORMAL); EXPECT_EQ(decl[1].Offset, 12); EXPECT_EQ(decl[2].Usage, D3DDECLUSAGE_TEXCOORD); EXPECT_EQ(decl[3].Stream, 0xFF);
    ID3DXMesh_Release(m); dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_mesh_invalid) {
    SETUP9(d); REQUIRE(F9(D3DXCreateMeshFVF));
    ID3DXMesh *m = NULL; HRESULT hr;
    EXPECT_HR(F9(D3DXCreateMeshFVF)(2, 4, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ, d.dev, NULL), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateMeshFVF)(2, 4, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ, NULL, &m), INVALIDCALL);
    hr = F9(D3DXCreateMeshFVF)(0, 4, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ, d.dev, &m); t_note("0 faces -> %s", d3dh_hr_name(hr)); if (m) { ID3DXMesh_Release(m); m = NULL; }
    hr = F9(D3DXCreateMeshFVF)(2, 0, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ, d.dev, &m); t_note("0 vertices -> %s", d3dh_hr_name(hr)); if (m) { ID3DXMesh_Release(m); m = NULL; }
    hr = F9(D3DXCreateMeshFVF)(2, 4, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ | D3DFVF_XYZRHW, d.dev, &m); CHECK(FAILED(hr)); if (m) ID3DXMesh_Release(m);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_box) {
    SETUP9(d); REQUIRE(F9(D3DXCreateBox));
    ID3DXMesh *m = NULL; ID3DXBuffer *adj = NULL;
    EXPECT_HR(F9(D3DXCreateBox)(d.dev, 1.f, 2.f, 3.f, &m, &adj), S_OK);
    if (m) {
        EXPECT_EQ(ID3DXMesh_GetNumFaces(m), 12); EXPECT_EQ(ID3DXMesh_GetNumVertices(m), 24);
        EXPECT_EQ(ID3DXMesh_GetFVF(m), D3DFVF_XYZ | D3DFVF_NORMAL);
        struct { V3 p, n; } *vb = NULL;
        if (SUCCEEDED(ID3DXMesh_LockVertexBuffer(m, D3DLOCK_READONLY, (void **)&vb)) && vb) {
            float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f, mnz = 1e9f, mxz = -1e9f;
            for (UINT i = 0; i < 24; i++) {
                if (vb[i].p.x < mnx) mnx = vb[i].p.x; if (vb[i].p.x > mxx) mxx = vb[i].p.x;
                if (vb[i].p.y < mny) mny = vb[i].p.y; if (vb[i].p.y > mxy) mxy = vb[i].p.y;
                if (vb[i].p.z < mnz) mnz = vb[i].p.z; if (vb[i].p.z > mxz) mxz = vb[i].p.z;
                float len = sqrtf(vb[i].n.x * vb[i].n.x + vb[i].n.y * vb[i].n.y + vb[i].n.z * vb[i].n.z);
                CHECKF(dx_near(len, 1.f, 1e-5f), "normal %u length %g", i, len);
                float dotp = vb[i].p.x * vb[i].n.x + vb[i].p.y * vb[i].n.y + vb[i].p.z * vb[i].n.z;
                CHECKF(dotp > 0, "normal %u points inwards", i);
            }
            CHECKN(mnx, -0.5f); CHECKN(mxx, 0.5f); CHECKN(mny, -1.f); CHECKN(mxy, 1.f); CHECKN(mnz, -1.5f); CHECKN(mxz, 1.5f);
            ID3DXMesh_UnlockVertexBuffer(m);
        }
        ID3DXMesh_Release(m);
    }
    if (adj) { EXPECT_EQ(ID3DXBuffer_GetBufferSize(adj), 12 * 3 * 4); ID3DXBuffer_Release(adj); }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_box_adjacency_symmetric) {
    SETUP9(d); REQUIRE(F9(D3DXCreateBox));
    ID3DXMesh *m = NULL; ID3DXBuffer *adj = NULL;
    if (FAILED(F9(D3DXCreateBox)(d.dev, 1.f, 1.f, 1.f, &m, &adj)) || !adj) { t_skip("box failed"); dev9_close(&d); return; }
    const DWORD *a = ID3DXBuffer_GetBufferPointer(adj);
    for (DWORD f = 0; f < 12; f++) for (int e = 0; e < 3; e++) {
        DWORD n = a[f * 3 + e];
        CHECKF(n < 12, "face %lu edge %d neighbour 0x%lx", f, e, n);
        if (n < 12) { int back = 0; for (int k = 0; k < 3; k++) if (a[n * 3 + k] == f) back = 1; CHECKF(back, "face %lu -> %lu not symmetric", f, n); }
    }
    ID3DXBuffer_Release(adj); ID3DXMesh_Release(m); dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_sphere_counts) {
    SETUP9(d); REQUIRE(F9(D3DXCreateSphere));
    static const struct { UINT slices, stacks; } t[] = { { 8, 4 }, { 12, 6 }, { 3, 2 }, { 20, 10 } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ID3DXMesh *m = NULL; EXPECT_HR(F9(D3DXCreateSphere)(d.dev, 2.f, t[i].slices, t[i].stacks, &m, NULL), S_OK);
        if (m) {
            CHECKF(ID3DXMesh_GetNumVertices(m) == 2 + t[i].slices * (t[i].stacks - 1), "verts %lu", ID3DXMesh_GetNumVertices(m));
            CHECKF(ID3DXMesh_GetNumFaces(m) == 2 * t[i].slices * (t[i].stacks - 1), "faces %lu", ID3DXMesh_GetNumFaces(m));
            ID3DXMesh_Release(m);
        }
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_sphere_radius) {
    SETUP9(d); REQUIRE(F9(D3DXCreateSphere)); REQUIRE(F9(D3DXComputeBoundingSphere));
    ID3DXMesh *m = NULL;
    if (FAILED(F9(D3DXCreateSphere)(d.dev, 2.5f, 16, 8, &m, NULL))) { t_skip("sphere failed"); dev9_close(&d); return; }
    struct { V3 p, n; } *vb = NULL;
    if (SUCCEEDED(ID3DXMesh_LockVertexBuffer(m, D3DLOCK_READONLY, (void **)&vb)) && vb) {
        for (UINT i = 0; i < ID3DXMesh_GetNumVertices(m); i++) {
            float r = sqrtf(vb[i].p.x * vb[i].p.x + vb[i].p.y * vb[i].p.y + vb[i].p.z * vb[i].p.z);
            CHECKF(dx_near(r, 2.5f, 1e-4f), "vertex %u radius %g", i, r);
        }
        ID3DXMesh_UnlockVertexBuffer(m);
    }
    ID3DXMesh_Release(m); dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_sphere_invalid) {
    SETUP9(d); REQUIRE(F9(D3DXCreateSphere));
    ID3DXMesh *m = NULL;
    EXPECT_HR(F9(D3DXCreateSphere)(d.dev, 1.f, 8, 4, NULL, NULL), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateSphere)(d.dev, -1.f, 8, 4, &m, NULL), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateSphere)(d.dev, 1.f, 1, 4, &m, NULL), INVALIDCALL);   /* slices < 2 */
    EXPECT_HR(F9(D3DXCreateSphere)(d.dev, 1.f, 8, 0, &m, NULL), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateSphere)(NULL, 1.f, 8, 4, &m, NULL), INVALIDCALL);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_torus_counts) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTorus));
    ID3DXMesh *m = NULL;
    EXPECT_HR(F9(D3DXCreateTorus)(d.dev, 0.5f, 1.5f, 6, 8, &m, NULL), S_OK);
    if (m) { EXPECT_EQ(ID3DXMesh_GetNumVertices(m), 6 * 8); EXPECT_EQ(ID3DXMesh_GetNumFaces(m), 2 * 6 * 8); ID3DXMesh_Release(m); }
    EXPECT_HR(F9(D3DXCreateTorus)(d.dev, 0.5f, 1.5f, 2, 8, &m, NULL), INVALIDCALL);   /* sides < 3 */
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_polygon) {
    SETUP9(d); REQUIRE(F9(D3DXCreatePolygon));
    ID3DXMesh *m = NULL;
    EXPECT_HR(F9(D3DXCreatePolygon)(d.dev, 1.f, 5, &m, NULL), S_OK);
    if (m) {
        EXPECT_EQ(ID3DXMesh_GetNumFaces(m), 5);
        t_note("polygon(5): %lu vertices, FVF 0x%lx", ID3DXMesh_GetNumVertices(m), ID3DXMesh_GetFVF(m));
        ID3DXMesh_Release(m);
    }
    EXPECT_HR(F9(D3DXCreatePolygon)(d.dev, 1.f, 2, &m, NULL), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreatePolygon)(d.dev, -1.f, 5, &m, NULL), INVALIDCALL);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_cylinder_counts) {
    SETUP9(d); REQUIRE(F9(D3DXCreateCylinder));
    ID3DXMesh *m = NULL;
    EXPECT_HR(F9(D3DXCreateCylinder)(d.dev, 1.f, 1.f, 2.f, 8, 3, &m, NULL), S_OK);
    if (m) {
        CHECK(ID3DXMesh_GetNumFaces(m) >= 2 * 8 * 3);   /* at least the side quads */
        t_note("cylinder(8,3): %lu faces %lu vertices", ID3DXMesh_GetNumFaces(m), ID3DXMesh_GetNumVertices(m));
        ID3DXMesh_Release(m);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_create_teapot) {
    SETUP9(d); REQUIRE(F9(D3DXCreateTeapot));
    ID3DXMesh *m = NULL;
    EXPECT_HR(F9(D3DXCreateTeapot)(d.dev, &m, NULL), S_OK);
    if (m) {
        CHECK(ID3DXMesh_GetNumFaces(m) > 1000); EXPECT_EQ(ID3DXMesh_GetFVF(m), D3DFVF_XYZ | D3DFVF_NORMAL);
        t_note("teapot: %lu faces %lu vertices", ID3DXMesh_GetNumFaces(m), ID3DXMesh_GetNumVertices(m));
        ID3DXMesh_Release(m);
    }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_mesh_clone_fvf) {
    SETUP9(d); REQUIRE(F9(D3DXCreateBox));
    ID3DXMesh *m = NULL, *c = NULL;
    if (FAILED(F9(D3DXCreateBox)(d.dev, 1.f, 1.f, 1.f, &m, NULL))) { t_skip("box failed"); dev9_close(&d); return; }
    EXPECT_HR(ID3DXMesh_CloneMeshFVF(m, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1, d.dev, &c), S_OK);
    if (c) {
        EXPECT_EQ(ID3DXMesh_GetFVF(c), D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1); EXPECT_EQ(ID3DXMesh_GetNumVertices(c), 24); EXPECT_EQ(ID3DXMesh_GetNumFaces(c), 12); EXPECT_EQ(ID3DXMesh_GetNumBytesPerVertex(c), 32);
        struct { V3 p, n; float u, v; } *vb = NULL; struct { V3 p, n; } *src = NULL;
        if (SUCCEEDED(ID3DXMesh_LockVertexBuffer(c, D3DLOCK_READONLY, (void **)&vb)) && vb) {
            if (SUCCEEDED(ID3DXMesh_LockVertexBuffer(m, D3DLOCK_READONLY, (void **)&src)) && src) {
                for (int i = 0; i < 24; i++) CHECKF(vb[i].p.x == src[i].p.x && vb[i].n.z == src[i].n.z, "vertex %d changed by clone", i);
                ID3DXMesh_UnlockVertexBuffer(m);
            }
            ID3DXMesh_UnlockVertexBuffer(c);
        }
        ID3DXMesh_Release(c);
    }
    ID3DXMesh_Release(m); dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_mesh_compute_normals) {
    SETUP9(d); REQUIRE(F9(D3DXCreateMeshFVF)); REQUIRE(F9(D3DXComputeNormals));
    ID3DXMesh *m = NULL;
    if (FAILED(F9(D3DXCreateMeshFVF)(1, 3, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ | D3DFVF_NORMAL, d.dev, &m))) { t_skip("mesh failed"); dev9_close(&d); return; }
    struct { V3 p, n; } *vb = NULL; WORD *ib = NULL;
    if (SUCCEEDED(ID3DXMesh_LockVertexBuffer(m, 0, (void **)&vb)) && vb) { memset(vb, 0, 3 * sizeof *vb); vb[1].p.x = 1; vb[2].p.y = 1; ID3DXMesh_UnlockVertexBuffer(m); }
    if (SUCCEEDED(ID3DXMesh_LockIndexBuffer(m, 0, (void **)&ib)) && ib) { ib[0] = 0; ib[1] = 1; ib[2] = 2; ID3DXMesh_UnlockIndexBuffer(m); }
    EXPECT_HR(F9(D3DXComputeNormals)((ID3DXBaseMesh *)m, NULL), S_OK);
    if (SUCCEEDED(ID3DXMesh_LockVertexBuffer(m, D3DLOCK_READONLY, (void **)&vb)) && vb) {
        for (int i = 0; i < 3; i++) { CHECKN(vb[i].n.x, 0.f); CHECKN(vb[i].n.y, 0.f); CHECKN(fabsf(vb[i].n.z), 1.f); }
        t_note("normal z of triangle (0,0,0),(1,0,0),(0,1,0): %g (LH, clockwise-front convention would give -1)", vb[0].n.z);
        ID3DXMesh_UnlockVertexBuffer(m);
    }
    ID3DXMesh_Release(m); dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_mesh_attribute_table_default) {
    SETUP9(d); REQUIRE(F9(D3DXCreateBox));
    ID3DXMesh *m = NULL;
    if (FAILED(F9(D3DXCreateBox)(d.dev, 1.f, 1.f, 1.f, &m, NULL))) { t_skip("box failed"); dev9_close(&d); return; }
    DWORD *attr = NULL;
    if (SUCCEEDED(ID3DXMesh_LockAttributeBuffer(m, D3DLOCK_READONLY, &attr)) && attr) { for (int i = 0; i < 12; i++) CHECKF(attr[i] == 0, "face %d subset %lu", i, attr[i]); ID3DXMesh_UnlockAttributeBuffer(m); }
    DWORD n = 99;
    HRESULT hr = ID3DXMesh_GetAttributeTable(m, NULL, &n);
    t_note("attribute table before optimize: %s entries=%lu", d3dh_hr_name(hr), n);
    ID3DXMesh_Release(m); dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_mesh_optimize_inplace) {
    SETUP9(d); REQUIRE(F9(D3DXCreateBox));
    ID3DXMesh *m = NULL; ID3DXBuffer *adj = NULL;
    if (FAILED(F9(D3DXCreateBox)(d.dev, 1.f, 1.f, 1.f, &m, &adj))) { t_skip("box failed"); dev9_close(&d); return; }
    HRESULT hr = ID3DXMesh_OptimizeInplace(m, D3DXMESHOPT_ATTRSORT, ID3DXBuffer_GetBufferPointer(adj), NULL, NULL, NULL);
    EXPECT_HR(hr, S_OK);
    DWORD n = 0; EXPECT_HR(ID3DXMesh_GetAttributeTable(m, NULL, &n), S_OK); EXPECT_EQ(n, 1);
    D3DXATTRIBUTERANGE r[1];
    if (n == 1) { EXPECT_HR(ID3DXMesh_GetAttributeTable(m, r, &n), S_OK); EXPECT_EQ(r[0].AttribId, 0); EXPECT_EQ(r[0].FaceStart, 0); EXPECT_EQ(r[0].FaceCount, 12); }
    ID3DXBuffer_Release(adj); ID3DXMesh_Release(m); dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_mesh_weld_unique) {
    SETUP9(d); REQUIRE(F9(D3DXCreateBox));
    ID3DXMesh *m = NULL, *c = NULL;
    if (FAILED(F9(D3DXCreateBox)(d.dev, 1.f, 1.f, 1.f, &m, NULL))) { t_skip("box failed"); dev9_close(&d); return; }
    EXPECT_HR(ID3DXMesh_CloneMeshFVF(m, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ, d.dev, &c), S_OK);   /* drop normals: duplicated corners now coincide */
    if (c) {
        DWORD *attrs = NULL; (void)attrs;
        EXPECT_EQ(ID3DXMesh_GetNumVertices(c), 24);
        D3DXWELDEPSILONS eps; memset(&eps, 0, sizeof eps);
        DWORD adj[36]; ID3DXMesh_GenerateAdjacency(c, 0.0f, adj);
        HRESULT hr = F9(D3DXWeldVertices)(c, D3DXWELDEPSILONS_WELDALL, &eps, adj, NULL, NULL, NULL);
        t_note("WeldVertices -> %s; vertices after weld %lu (box has 8 corners)", d3dh_hr_name(hr), ID3DXMesh_GetNumVertices(c));
        ID3DXMesh_Release(c);
    }
    ID3DXMesh_Release(m); dev9_close(&d);
}

/* ---- effects --------------------------------------------------------------------------- */
static const char FX_MIN[] = "technique T { pass P { } }";
static const char FX_PARAMS[] = "float4 g = float4(1,2,3,4);\nfloat s = 0.5;\nint n = 7;\ntechnique T0 { pass P0 { } }\ntechnique T1 { pass A { } pass B { } }\n";

TEST(d3dx_redist_optional_x9_effect_minimal) {
    SETUP9(d); REQUIRE(F9(D3DXCreateEffect));
    ID3DXEffect *fx = NULL; ID3DXBuffer *err = NULL;
    HRESULT hr = F9(D3DXCreateEffect)(d.dev, FX_MIN, sizeof FX_MIN - 1, NULL, NULL, 0, NULL, &fx, &err);
    EXPECT_HR(hr, S_OK);
    if (fx) {
        D3DXEFFECT_DESC ed; EXPECT_HR(ID3DXEffect_GetDesc(fx, &ed), S_OK);
        EXPECT_EQ(ed.Techniques, 1); EXPECT_EQ(ed.Parameters, 0); EXPECT_EQ(ed.Functions, 0);
        D3DXHANDLE t = ID3DXEffect_GetTechnique(fx, 0); CHECK(t != NULL);
        D3DXTECHNIQUE_DESC td; memset(&td, 0, sizeof td);
        if (t) { EXPECT_HR(ID3DXEffect_GetTechniqueDesc(fx, t, &td), S_OK); EXPECT_STR(td.Name, "T"); EXPECT_EQ(td.Passes, 1); EXPECT_EQ(td.Annotations, 0); }
        CHECK(ID3DXEffect_GetTechnique(fx, 1) == NULL);
        CHECK(ID3DXEffect_GetTechniqueByName(fx, "T") != NULL); CHECK(ID3DXEffect_GetTechniqueByName(fx, "nope") == NULL);
        CHECK((ULONG)ID3DXEffect_Release(fx) == 0);
    }
    if (err) ID3DXBuffer_Release(err);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_effect_parameters) {
    SETUP9(d); REQUIRE(F9(D3DXCreateEffect));
    ID3DXEffect *fx = NULL; ID3DXBuffer *err = NULL;
    HRESULT hr = F9(D3DXCreateEffect)(d.dev, FX_PARAMS, sizeof FX_PARAMS - 1, NULL, NULL, 0, NULL, &fx, &err);
    EXPECT_HR(hr, S_OK);
    if (fx) {
        D3DXEFFECT_DESC ed; ID3DXEffect_GetDesc(fx, &ed); EXPECT_EQ(ed.Parameters, 3); EXPECT_EQ(ed.Techniques, 2);
        D3DXHANDLE hg = ID3DXEffect_GetParameterByName(fx, NULL, "g"), hs = ID3DXEffect_GetParameterByName(fx, NULL, "s"), hn = ID3DXEffect_GetParameterByName(fx, NULL, "n");
        CHECK(hg && hs && hn); CHECK(ID3DXEffect_GetParameterByName(fx, NULL, "zzz") == NULL);
        D3DXVECTOR4 v = {0}; float f = 0; INT n = 0;
        if (hg) { EXPECT_HR(ID3DXEffect_GetVector(fx, hg, &v), S_OK); CHECKN(v.x, 1.f); CHECKN(v.y, 2.f); CHECKN(v.z, 3.f); CHECKN(v.w, 4.f);
                  D3DXVECTOR4 nv = {5,6,7,8}; EXPECT_HR(ID3DXEffect_SetVector(fx, hg, &nv), S_OK); memset(&v, 0, sizeof v); ID3DXEffect_GetVector(fx, hg, &v); CHECKN(v.w, 8.f); }
        if (hs) { EXPECT_HR(ID3DXEffect_GetFloat(fx, hs, &f), S_OK); CHECKN(f, 0.5f); EXPECT_HR(ID3DXEffect_SetFloat(fx, hs, 2.25f), S_OK); ID3DXEffect_GetFloat(fx, hs, &f); CHECKN(f, 2.25f); }
        if (hn) { EXPECT_HR(ID3DXEffect_GetInt(fx, hn, &n), S_OK); EXPECT_EQ(n, 7); ID3DXEffect_SetInt(fx, hn, -3); ID3DXEffect_GetInt(fx, hn, &n); EXPECT_EQ(n, -3); }
        D3DXPARAMETER_DESC pd;
        if (hg) { EXPECT_HR(ID3DXEffect_GetParameterDesc(fx, hg, &pd), S_OK); EXPECT_STR(pd.Name, "g"); EXPECT_EQ(pd.Class, D3DXPC_VECTOR); EXPECT_EQ(pd.Type, D3DXPT_FLOAT); EXPECT_EQ(pd.Columns, 4); EXPECT_EQ(pd.Rows, 1); EXPECT_EQ(pd.Bytes, 16); }
        if (hn) { ID3DXEffect_GetParameterDesc(fx, hn, &pd); EXPECT_EQ(pd.Type, D3DXPT_INT); EXPECT_EQ(pd.Class, D3DXPC_SCALAR); }
        D3DXTECHNIQUE_DESC td; ID3DXEffect_GetTechniqueDesc(fx, ID3DXEffect_GetTechnique(fx, 1), &td); EXPECT_STR(td.Name, "T1"); EXPECT_EQ(td.Passes, 2);
        ID3DXEffect_Release(fx);
    }
    if (err) ID3DXBuffer_Release(err);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_effect_syntax_error) {
    SETUP9(d); REQUIRE(F9(D3DXCreateEffect));
    static const char bad[] = "technique { pass P { ";
    ID3DXEffect *fx = NULL; ID3DXBuffer *err = NULL;
    HRESULT hr = F9(D3DXCreateEffect)(d.dev, bad, sizeof bad - 1, NULL, NULL, 0, NULL, &fx, &err);
    CHECK(FAILED(hr)); CHECK(fx == NULL); t_note("effect syntax error hr %s", d3dh_hr_name(hr));
    if (err) { t_note("effect compiler message: %.160s", (const char *)ID3DXBuffer_GetBufferPointer(err)); ID3DXBuffer_Release(err); }
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_effect_invalid_args) {
    SETUP9(d); REQUIRE(F9(D3DXCreateEffect));
    ID3DXEffect *fx = NULL;
    EXPECT_HR(F9(D3DXCreateEffect)(NULL, FX_MIN, sizeof FX_MIN - 1, NULL, NULL, 0, NULL, &fx, NULL), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateEffect)(d.dev, NULL, 0, NULL, NULL, 0, NULL, &fx, NULL), INVALIDCALL);
    EXPECT_HR(F9(D3DXCreateEffect)(d.dev, FX_MIN, sizeof FX_MIN - 1, NULL, NULL, 0, NULL, NULL, NULL), INVALIDCALL);
    dev9_close(&d);
}
TEST(d3dx_redist_optional_x9_effect_begin_end_no_passes_state) {
    SETUP9(d); REQUIRE(F9(D3DXCreateEffect));
    ID3DXEffect *fx = NULL;
    if (FAILED(F9(D3DXCreateEffect)(d.dev, FX_MIN, sizeof FX_MIN - 1, NULL, NULL, 0, NULL, &fx, NULL))) { t_skip("effect failed"); dev9_close(&d); return; }
    UINT passes = 0;
    EXPECT_HR(ID3DXEffect_SetTechnique(fx, ID3DXEffect_GetTechnique(fx, 0)), S_OK);
    HRESULT hr = ID3DXEffect_Begin(fx, &passes, 0); EXPECT_HR(hr, S_OK); EXPECT_EQ(passes, 1);
    if (SUCCEEDED(hr)) {
        EXPECT_HR(ID3DXEffect_BeginPass(fx, 0), S_OK); EXPECT_HR(ID3DXEffect_EndPass(fx), S_OK); EXPECT_HR(ID3DXEffect_End(fx), S_OK);
    }
    hr = ID3DXEffect_End(fx); t_note("End without matching Begin -> %s", d3dh_hr_name(hr));
    ID3DXEffect_Release(fx); dev9_close(&d);
}
