#include "dx.h"
/* d3dx10_43.dll / d3dx11_43.dll: image info, texture loading, SH projection, compile, math parity with d3dx9_43. */

/* Declarations are local: the SDK headers for D3DX10/11 are not part of the toolchain. */
typedef struct { UINT Width, Height, Depth, ArraySize, MipLevels, MiscFlags; DXGI_FORMAT Format; UINT ResourceDimension; UINT ImageFileFormat; } XIMAGE_INFO;
typedef struct { UINT Width, Height, Depth, FirstMipLevel, MipLevels; UINT Usage, BindFlags, CpuAccessFlags, MiscFlags; DXGI_FORMAT Format; UINT Filter, MipFilter; XIMAGE_INFO *pSrcInfo; } XLOAD_INFO;
enum { XIFF_BMP = 0, XIFF_JPG = 1, XIFF_PNG = 3, XIFF_DDS = 4 };
#define XDEFAULT ((UINT)-1)
#define XFROM_FILE ((UINT)-3)

typedef HRESULT (WINAPI *X_GetInfoMem)(const void *, SIZE_T, void *pump, XIMAGE_INFO *, HRESULT *);
typedef BOOL (WINAPI *X_CheckVer)(UINT, UINT);
typedef HRESULT (WINAPI *X10_TexMem)(ID3D10Device *, const void *, SIZE_T, XLOAD_INFO *, void *, ID3D10Resource **, HRESULT *);
typedef HRESULT (WINAPI *X11_TexMem)(ID3D11Device *, const void *, SIZE_T, XLOAD_INFO *, void *, ID3D11Resource **, HRESULT *);
typedef HRESULT (WINAPI *X10_SRVMem)(ID3D10Device *, const void *, SIZE_T, XLOAD_INFO *, void *, ID3D10ShaderResourceView **, HRESULT *);
typedef HRESULT (WINAPI *X11_SRVMem)(ID3D11Device *, const void *, SIZE_T, XLOAD_INFO *, void *, ID3D11ShaderResourceView **, HRESULT *);
typedef HRESULT (WINAPI *X10_CompMem)(const char *, SIZE_T, const char *, const void *, void *, const char *, const char *, UINT, UINT, void *, ID3D10Blob **, ID3D10Blob **, HRESULT *);
typedef HRESULT (WINAPI *X10_SHProj)(UINT, ID3D10Texture2D *, float *, float *, float *);
typedef HRESULT (WINAPI *X11_SHProj)(ID3D11DeviceContext *, UINT, ID3D11Texture2D *, float *, float *, float *);
typedef HRESULT (WINAPI *X11_LoadTex)(ID3D11DeviceContext *, ID3D11Resource *, void *, ID3D11Resource *);
typedef HRESULT (WINAPI *X10_LoadTex)(ID3D10Resource *, void *, ID3D10Resource *);
typedef HRESULT (WINAPI *X10_Pump)(UINT, UINT, void **);

#define P10(name, type) ((type)t_proc_in(X10, name))
#define P11(name, type) ((type)t_proc_in(X11, name))
#define E_X10_ERR_FAIL(h) FAILED(h)

static const BYTE RGBA_RGBW[16] = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255 };

/* ---- presence / version ----------------------------------------------------------------- */
TEST(d3dx_redist_optional_x10_check_version) {
    NEED_DLL(X10);
    X_CheckVer f = P10("D3DX10CheckVersion", X_CheckVer); REQUIRE(f);
    CHECK(f(29, 43) == TRUE); CHECK(f(29, 42) == FALSE); CHECK(f(28, 43) == FALSE); CHECK(f(0, 0) == FALSE);
}
TEST(d3dx_redist_optional_x11_check_version) {
    NEED_DLL(X11);
    X_CheckVer f = P11("D3DX11CheckVersion", X_CheckVer); REQUIRE(f);
    CHECK(f(7, 43) == TRUE); CHECK(f(7, 42) == FALSE); CHECK(f(6, 43) == FALSE); CHECK(f(0, 0) == FALSE);
}
TEST(d3dx_redist_optional_x10_x11_export_presence) {
    HMODULE a = LoadLibraryA(X10), b = LoadLibraryA(X11);
    static const char *n10[] = { "D3DX10CreateTextureFromMemory","D3DX10GetImageInfoFromMemory","D3DX10LoadTextureFromTexture","D3DX10SHProjectCubeMap","D3DX10CompileFromMemory","D3DX10CreateThreadPump",
        "D3DX10CreateEffectFromMemory","D3DX10SaveTextureToMemory","D3DX10FilterTexture","D3DXMatrixMultiply","D3DXSHEvalDirection","D3DX10CreateFontW","D3DX10CreateSprite","D3DX10CreateMesh" };
    static const char *n11[] = { "D3DX11CreateTextureFromMemory","D3DX11GetImageInfoFromMemory","D3DX11LoadTextureFromTexture","D3DX11SHProjectCubeMap","D3DX11CompileFromMemory","D3DX11CreateThreadPump",
        "D3DX11CreateShaderResourceViewFromMemory","D3DX11SaveTextureToMemory","D3DX11FilterTexture","D3DX11ComputeNormalMap" };
    for (unsigned i = 0; i < sizeof n10 / sizeof *n10; i++) t_note("%s!%s %s", X10, n10[i], a ? (GetProcAddress(a, n10[i]) ? "exported" : "missing") : "(dll absent)");
    for (unsigned i = 0; i < sizeof n11 / sizeof *n11; i++) t_note("%s!%s %s", X11, n11[i], b ? (GetProcAddress(b, n11[i]) ? "exported" : "missing") : "(dll absent)");
}

/* ---- image info (both DLLs, table driven) ----------------------------------------------------- */
static const struct { const char *name; const BYTE *data; UINT size; UINT w, h, mips, iff; int has_fmt; DXGI_FORMAT fmt; } IMGS[] = {
    { "png",     dxa_png_2x2,           sizeof dxa_png_2x2,           2, 2, 1, XIFF_PNG, 1, DXGI_FORMAT_R8G8B8A8_UNORM },
    { "bmp",     dxa_bmp24_2x2,         sizeof dxa_bmp24_2x2,         2, 2, 1, XIFF_BMP, 0, DXGI_FORMAT_UNKNOWN },
    { "dxt1",    dxa_dds_dxt1_4x4,      sizeof dxa_dds_dxt1_4x4,      4, 4, 1, XIFF_DDS, 1, DXGI_FORMAT_BC1_UNORM },
    { "dxt1mip", dxa_dds_dxt1_8x8_mips, sizeof dxa_dds_dxt1_8x8_mips, 8, 8, 4, XIFF_DDS, 1, DXGI_FORMAT_BC1_UNORM },
    { "argb",    dxa_dds_argb_4x4,      sizeof dxa_dds_argb_4x4,      4, 4, 1, XIFF_DDS, 0, DXGI_FORMAT_B8G8R8A8_UNORM },
};
static void imginfo_table(const char *dll, const char *fn) {
    X_GetInfoMem f = (X_GetInfoMem)t_proc_in(dll, fn); if (!f) { t_skip("%s missing", fn); return; }
    for (unsigned i = 0; i < sizeof IMGS / sizeof *IMGS; i++) {
        XIMAGE_INFO info; memset(&info, 0xCD, sizeof info);
        HRESULT hr = f(IMGS[i].data, IMGS[i].size, NULL, &info, NULL);
        if (hr == E_NOTIMPL && t_is_wine()) { t_skip("%s is a stub in this emulator", fn); return; }
        CHECKF(hr == S_OK, "%s %s hr %s", fn, IMGS[i].name, d3dh_hr_name(hr));
        if (FAILED(hr)) continue;
        CHECKF(info.Width == IMGS[i].w && info.Height == IMGS[i].h, "%s %ux%u", IMGS[i].name, info.Width, info.Height);
        CHECKF(info.Depth == 1, "%s depth %u", IMGS[i].name, info.Depth);
        CHECKF(info.ArraySize == 1, "%s array %u", IMGS[i].name, info.ArraySize);
        CHECKF(info.MipLevels == IMGS[i].mips, "%s mips %u", IMGS[i].name, info.MipLevels);
        CHECKF(info.ImageFileFormat == IMGS[i].iff, "%s iff %u", IMGS[i].name, info.ImageFileFormat);
        CHECKF(info.ResourceDimension == 3, "%s dimension %u (TEXTURE2D=3)", IMGS[i].name, info.ResourceDimension);
        CHECKF(info.MiscFlags == 0, "%s misc 0x%x", IMGS[i].name, info.MiscFlags);
        if (IMGS[i].has_fmt) CHECKF(info.Format == IMGS[i].fmt, "%s format %d want %d", IMGS[i].name, info.Format, IMGS[i].fmt);
        else t_note("%s format %d", IMGS[i].name, info.Format);
    }
}
TEST(d3dx_redist_optional_x10_imageinfo_table) { NEED_DLL(X10); imginfo_table(X10, "D3DX10GetImageInfoFromMemory"); }
TEST(d3dx_redist_optional_x11_imageinfo_table) { NEED_DLL(X11); imginfo_table(X11, "D3DX11GetImageInfoFromMemory"); }

static void imginfo_invalid(const char *dll, const char *fn) {
    X_GetInfoMem f = (X_GetInfoMem)t_proc_in(dll, fn); if (!f) { t_skip("%s missing", fn); return; }
    XIMAGE_INFO info; HRESULT hr;
    static const BYTE junk[64] = { 1, 2, 3, 4, 5, 6 };
    hr = f(junk, sizeof junk, NULL, &info, NULL); CHECK(FAILED(hr)); t_note("%s junk -> %s", fn, d3dh_hr_name(hr));
    hr = f(NULL, 0, NULL, &info, NULL); CHECK(FAILED(hr)); t_note("%s NULL/0 -> %s", fn, d3dh_hr_name(hr));
    hr = f(dxa_png_2x2, 0, NULL, &info, NULL); CHECK(FAILED(hr)); t_note("%s size 0 -> %s", fn, d3dh_hr_name(hr));
    hr = f(dxa_dds_dxt1_4x4, 20, NULL, &info, NULL); CHECK(FAILED(hr)); t_note("%s truncated dds -> %s", fn, d3dh_hr_name(hr));
    hr = f(dxa_png_2x2, 20, NULL, &info, NULL); CHECK(FAILED(hr)); t_note("%s truncated png -> %s", fn, d3dh_hr_name(hr));
}
TEST(d3dx_redist_optional_x10_imageinfo_invalid) { NEED_DLL(X10); imginfo_invalid(X10, "D3DX10GetImageInfoFromMemory"); }
TEST(d3dx_redist_optional_x11_imageinfo_invalid) { NEED_DLL(X11); imginfo_invalid(X11, "D3DX11GetImageInfoFromMemory"); }

static void imginfo_phresult(const char *dll, const char *fn) {
    X_GetInfoMem f = (X_GetInfoMem)t_proc_in(dll, fn); if (!f) { t_skip("%s missing", fn); return; }
    XIMAGE_INFO info; HRESULT out = 0x12345678;
    HRESULT hr = f(dxa_png_2x2, sizeof dxa_png_2x2, NULL, &info, &out);
    t_note("%s with a non-NULL pHResult and no pump: ret %s, *pHResult %s", fn, d3dh_hr_name(hr), d3dh_hr_name(out));
}
TEST(d3dx_redist_optional_x10_imageinfo_phresult) { NEED_DLL(X10); imginfo_phresult(X10, "D3DX10GetImageInfoFromMemory"); }
TEST(d3dx_redist_optional_x11_imageinfo_phresult) { NEED_DLL(X11); imginfo_phresult(X11, "D3DX11GetImageInfoFromMemory"); }

/* ---- textures: D3D11 ------------------------------------------------------------------------ */
static int readback11(ID3D11DeviceContext *ctx, ID3D11Resource *res, UINT w, UINT h, BYTE *out /* w*h*4 */) {
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(ID3D11DeviceContext_Map(ctx, res, 0, D3D11_MAP_READ, 0, &ms))) return 0;
    for (UINT y = 0; y < h; y++) memcpy(out + y * w * 4, (BYTE *)ms.pData + y * ms.RowPitch, w * 4);
    ID3D11DeviceContext_Unmap(ctx, res, 0); return 1;
}
static void staging_info11(XLOAD_INFO *li) {
    memset(li, 0xFF, sizeof *li);      /* every field = D3DX11_DEFAULT */
    li->Usage = D3D11_USAGE_STAGING; li->BindFlags = 0; li->CpuAccessFlags = D3D11_CPU_ACCESS_READ; li->MiscFlags = 0;
    li->MipLevels = 1; li->Format = DXGI_FORMAT_R8G8B8A8_UNORM; li->Filter = 1 /* NONE */; li->MipFilter = 1; li->pSrcInfo = NULL;
}
typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; } D11;
static int d11_open(D11 *d) {
    memset(d, 0, sizeof *d); D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE dt;
    HRESULT hr = d3dh_make_device11(&d->dev, &d->ctx, &fl, &dt, 0);
    if (FAILED(hr) || !d->dev) { t_skip("no D3D11 device (%s)", d3dh_hr_name(hr)); return 0; }
    return 1;
}
static void d11_close(D11 *d) { SAFE_RELEASE(d->ctx); SAFE_RELEASE(d->dev); }

static void tex11_pixels(const void *data, UINT n, const BYTE *want, UINT w, UINT h, const char *what) {
    X11_TexMem f = P11("D3DX11CreateTextureFromMemory", X11_TexMem); if (!f) { t_skip("export missing"); return; }
    D11 d; if (!d11_open(&d)) return;
    XLOAD_INFO li; staging_info11(&li); li.Width = w; li.Height = h;
    ID3D11Resource *r = NULL; HRESULT hr = f(d.dev, data, n, &li, NULL, &r, NULL);
    CHECKF(hr == S_OK, "%s hr %s", what, d3dh_hr_name(hr));
    if (r) {
        ID3D11Texture2D *t = NULL; EXPECT_HR(ID3D11Resource_QueryInterface(r, &IID_ID3D11Texture2D, (void **)&t), S_OK);
        if (t) {
            D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
            EXPECT_EQ(td.Width, w); EXPECT_EQ(td.Height, h); EXPECT_EQ(td.MipLevels, 1); EXPECT_EQ(td.ArraySize, 1);
            EXPECT_EQ(td.Format, DXGI_FORMAT_R8G8B8A8_UNORM); EXPECT_EQ(td.Usage, D3D11_USAGE_STAGING);
            BYTE px[64]; CHECK(readback11(d.ctx, r, w, h, px));
            CHECKF(!memcmp(px, want, w * h * 4), "%s pixels differ: %02x%02x%02x%02x %02x%02x%02x%02x ...", what, px[0], px[1], px[2], px[3], px[4], px[5], px[6], px[7]);
            ID3D11Texture2D_Release(t);
        }
        ID3D11Resource_Release(r);
    }
    d11_close(&d);
}
TEST(d3dx_redist_optional_x11_texture_png_pixels) { NEED_DLL(X11); tex11_pixels(dxa_png_2x2, sizeof dxa_png_2x2, RGBA_RGBW, 2, 2, "png"); }
TEST(d3dx_redist_optional_x11_texture_bmp_pixels) { NEED_DLL(X11); tex11_pixels(dxa_bmp24_2x2, sizeof dxa_bmp24_2x2, RGBA_RGBW, 2, 2, "bmp"); }
TEST(d3dx_redist_optional_x11_texture_dds_argb_pixels) {
    NEED_DLL(X11);
    BYTE want[64]; for (int i = 0; i < 16; i++) { want[i*4+0] = 0x33; want[i*4+1] = 0x66; want[i*4+2] = 0x99; want[i*4+3] = 0xFF; }
    /* ARGB 0xFF336699: R=0x33 G=0x66 B=0x99 A=0xFF -> RGBA byte order 33 66 99 FF */
    tex11_pixels(dxa_dds_argb_4x4, sizeof dxa_dds_argb_4x4, want, 4, 4, "dds-argb");
}
TEST(d3dx_redist_optional_x11_texture_dds_dxt1_decompressed) {
    NEED_DLL(X11);
    BYTE want[64]; memset(want, 0xFF, sizeof want);
    tex11_pixels(dxa_dds_dxt1_4x4, sizeof dxa_dds_dxt1_4x4, want, 4, 4, "dds-dxt1");
}
TEST(d3dx_redist_optional_x11_texture_default_info) {
    NEED_DLL(X11);
    X11_TexMem f = P11("D3DX11CreateTextureFromMemory", X11_TexMem); REQUIRE(f);
    D11 d; if (!d11_open(&d)) return;
    ID3D11Resource *r = NULL; XIMAGE_INFO si; XLOAD_INFO li; memset(&li, 0xFF, sizeof li); li.pSrcInfo = &si;
    HRESULT hr = f(d.dev, dxa_dds_dxt1_8x8_mips, sizeof dxa_dds_dxt1_8x8_mips, &li, NULL, &r, NULL);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(si.Width, 8); EXPECT_EQ(si.MipLevels, 4);
    if (r) {
        ID3D11Texture2D *t = NULL; ID3D11Resource_QueryInterface(r, &IID_ID3D11Texture2D, (void **)&t);
        if (t) { D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
            EXPECT_EQ(td.Width, 8); EXPECT_EQ(td.Format, DXGI_FORMAT_BC1_UNORM); EXPECT_EQ(td.Usage, D3D11_USAGE_DEFAULT);
            t_note("default load: mips %u bind 0x%x cpu 0x%x", td.MipLevels, td.BindFlags, td.CPUAccessFlags);
            CHECK((td.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0);
            ID3D11Texture2D_Release(t); }
        ID3D11Resource_Release(r);
    }
    d11_close(&d);
}
TEST(d3dx_redist_optional_x11_texture_null_loadinfo) {
    NEED_DLL(X11);
    X11_TexMem f = P11("D3DX11CreateTextureFromMemory", X11_TexMem); REQUIRE(f);
    D11 d; if (!d11_open(&d)) return;
    ID3D11Resource *r = NULL; HRESULT hr = f(d.dev, dxa_png_2x2, sizeof dxa_png_2x2, NULL, NULL, &r, NULL);
    EXPECT_HR(hr, S_OK);
    if (r) {
        ID3D11Texture2D *t = NULL; ID3D11Resource_QueryInterface(r, &IID_ID3D11Texture2D, (void **)&t);
        if (t) { D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
            EXPECT_EQ(td.Width, 2); EXPECT_EQ(td.Height, 2); EXPECT_EQ(td.Format, DXGI_FORMAT_R8G8B8A8_UNORM); EXPECT_EQ(td.Usage, D3D11_USAGE_DEFAULT); EXPECT_EQ(td.BindFlags, D3D11_BIND_SHADER_RESOURCE);
            t_note("default png load: mips %u (2x2 full chain would be 2)", td.MipLevels);
            ID3D11Texture2D_Release(t); }
        ID3D11Resource_Release(r);
    }
    d11_close(&d);
}
TEST(d3dx_redist_optional_x11_texture_invalid) {
    NEED_DLL(X11);
    X11_TexMem f = P11("D3DX11CreateTextureFromMemory", X11_TexMem); REQUIRE(f);
    D11 d; if (!d11_open(&d)) return;
    ID3D11Resource *r = (ID3D11Resource *)1; static const BYTE junk[32] = { 7, 7, 7 };
    HRESULT hr = f(d.dev, junk, sizeof junk, NULL, NULL, &r, NULL); CHECK(FAILED(hr)); t_note("junk -> %s", d3dh_hr_name(hr));
    r = NULL; hr = f(d.dev, NULL, 0, NULL, NULL, &r, NULL); CHECK(FAILED(hr)); t_note("NULL data -> %s", d3dh_hr_name(hr));
    hr = f(NULL, dxa_png_2x2, sizeof dxa_png_2x2, NULL, NULL, &r, NULL); CHECK(FAILED(hr)); t_note("NULL device -> %s", d3dh_hr_name(hr));
    if (r && r != (ID3D11Resource *)1) ID3D11Resource_Release(r);
    d11_close(&d);
}
TEST(d3dx_redist_optional_x11_srv_from_memory) {
    NEED_DLL(X11);
    X11_SRVMem f = P11("D3DX11CreateShaderResourceViewFromMemory", X11_SRVMem); REQUIRE(f);
    D11 d; if (!d11_open(&d)) return;
    ID3D11ShaderResourceView *v = NULL;
    EXPECT_HR(f(d.dev, dxa_png_2x2, sizeof dxa_png_2x2, NULL, NULL, &v, NULL), S_OK);
    if (v) {
        D3D11_SHADER_RESOURCE_VIEW_DESC vd; ID3D11ShaderResourceView_GetDesc(v, &vd);
        EXPECT_EQ(vd.Format, DXGI_FORMAT_R8G8B8A8_UNORM); EXPECT_EQ(vd.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2D); EXPECT_EQ(vd.Texture2D.MostDetailedMip, 0);
        ID3D11Resource *r = NULL; ID3D11ShaderResourceView_GetResource(v, &r); CHECK(r != NULL); if (r) ID3D11Resource_Release(r);
        CHECK((ULONG)ID3D11ShaderResourceView_Release(v) == 0);
    }
    d11_close(&d);
}
TEST(d3dx_redist_optional_x11_load_texture_from_texture) {
    NEED_DLL(X11);
    X11_TexMem f = P11("D3DX11CreateTextureFromMemory", X11_TexMem); X11_LoadTex lt = P11("D3DX11LoadTextureFromTexture", X11_LoadTex); REQUIRE(f); REQUIRE(lt);
    D11 d; if (!d11_open(&d)) return;
    XLOAD_INFO li; staging_info11(&li); li.Width = 2; li.Height = 2;
    ID3D11Resource *src = NULL;
    HRESULT hr = f(d.dev, dxa_png_2x2, sizeof dxa_png_2x2, &li, NULL, &src, NULL); EXPECT_HR(hr, S_OK);
    ID3D11Texture2D *dst = NULL;
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td); td.Width = 2; td.Height = 2; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    if (src && SUCCEEDED(ID3D11Device_CreateTexture2D(d.dev, &td, NULL, &dst))) {
        hr = lt(d.ctx, src, NULL, (ID3D11Resource *)dst);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) { BYTE px[16]; CHECK(readback11(d.ctx, (ID3D11Resource *)dst, 2, 2, px)); CHECK(!memcmp(px, RGBA_RGBW, 16)); }
    } else if (src) t_note("staging destination could not be created");
    if (dst) ID3D11Texture2D_Release(dst); if (src) ID3D11Resource_Release(src);
    d11_close(&d);
}
TEST(d3dx_redist_optional_x11_sh_project_constant_cube) {
    NEED_DLL(X11);
    X11_SHProj f = P11("D3DX11SHProjectCubeMap", X11_SHProj); REQUIRE(f);
    D11 d; if (!d11_open(&d)) return;
    DWORD white[16 * 6]; for (int i = 0; i < 16 * 6; i++) white[i] = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA sd[6]; for (int i = 0; i < 6; i++) { sd[i].pSysMem = white + i * 16; sd[i].SysMemPitch = 16; sd[i].SysMemSlicePitch = 0; }
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td); td.Width = td.Height = 4; td.MipLevels = 1; td.ArraySize = 6; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    ID3D11Texture2D *cube = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(d.dev, &td, sd, &cube))) { t_skip("cube texture creation failed"); d11_close(&d); return; }
    float r[9], g[9], b[9]; for (int i = 0; i < 9; i++) r[i] = g[i] = b[i] = -77.f;
    HRESULT hr = f(d.ctx, 3, cube, r, g, b); EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        CHECKNE(r[0], 3.5449077f, 0.03f); CHECKNE(g[0], 3.5449077f, 0.03f); CHECKNE(b[0], 3.5449077f, 0.03f);
        for (int i = 1; i < 9; i++) CHECKF(fabsf(r[i]) < 0.05f, "r[%d] = %g", i, r[i]);
    }
    ID3D11Texture2D_Release(cube); d11_close(&d);
}

/* ---- textures: D3D10 ------------------------------------------------------------------------ */
typedef struct { ID3D10Device *dev; } D10;
static int d10_open(D10 *d) {
    memset(d, 0, sizeof *d); D3D10_DRIVER_TYPE dt;
    HRESULT hr = d3dh_make_device10(&d->dev, &dt, 0);
    if (FAILED(hr) || !d->dev) { t_skip("no D3D10 device (%s)", d3dh_hr_name(hr)); return 0; }
    return 1;
}
TEST(d3dx_redist_optional_x10_texture_png_pixels) {
    NEED_DLL(X10);
    X10_TexMem f = P10("D3DX10CreateTextureFromMemory", X10_TexMem); REQUIRE(f);
    D10 d; if (!d10_open(&d)) return;
    XLOAD_INFO li; memset(&li, 0xFF, sizeof li);
    li.Usage = D3D10_USAGE_STAGING; li.BindFlags = 0; li.CpuAccessFlags = D3D10_CPU_ACCESS_READ; li.MiscFlags = 0; li.MipLevels = 1; li.Format = DXGI_FORMAT_R8G8B8A8_UNORM; li.Filter = 1; li.MipFilter = 1; li.pSrcInfo = NULL;
    ID3D10Resource *r = NULL; HRESULT hr = f(d.dev, dxa_png_2x2, sizeof dxa_png_2x2, &li, NULL, &r, NULL);
    EXPECT_HR(hr, S_OK);
    if (r) {
        ID3D10Texture2D *t = NULL; EXPECT_HR(ID3D10Resource_QueryInterface(r, &IID_ID3D10Texture2D, (void **)&t), S_OK);
        if (t) {
            D3D10_TEXTURE2D_DESC td; ID3D10Texture2D_GetDesc(t, &td);
            EXPECT_EQ(td.Width, 2); EXPECT_EQ(td.Height, 2); EXPECT_EQ(td.Format, DXGI_FORMAT_R8G8B8A8_UNORM); EXPECT_EQ(td.Usage, D3D10_USAGE_STAGING);
            D3D10_MAPPED_TEXTURE2D mt;
            if (SUCCEEDED(ID3D10Texture2D_Map(t, 0, D3D10_MAP_READ, 0, &mt))) {
                for (int y = 0; y < 2; y++) CHECK(!memcmp((BYTE *)mt.pData + y * mt.RowPitch, RGBA_RGBW + y * 8, 8));
                ID3D10Texture2D_Unmap(t, 0);
            }
            ID3D10Texture2D_Release(t);
        }
        ID3D10Resource_Release(r);
    }
    SAFE_RELEASE(d.dev);
}
TEST(d3dx_redist_optional_x10_texture_default_dxt1) {
    NEED_DLL(X10);
    X10_TexMem f = P10("D3DX10CreateTextureFromMemory", X10_TexMem); REQUIRE(f);
    D10 d; if (!d10_open(&d)) return;
    ID3D10Resource *r = NULL; HRESULT hr = f(d.dev, dxa_dds_dxt1_8x8_mips, sizeof dxa_dds_dxt1_8x8_mips, NULL, NULL, &r, NULL);
    EXPECT_HR(hr, S_OK);
    if (r) {
        ID3D10Texture2D *t = NULL; ID3D10Resource_QueryInterface(r, &IID_ID3D10Texture2D, (void **)&t);
        if (t) { D3D10_TEXTURE2D_DESC td; ID3D10Texture2D_GetDesc(t, &td); EXPECT_EQ(td.Width, 8); EXPECT_EQ(td.Format, DXGI_FORMAT_BC1_UNORM); EXPECT_EQ(td.MipLevels, 4); EXPECT_EQ(td.BindFlags, D3D10_BIND_SHADER_RESOURCE); ID3D10Texture2D_Release(t); }
        ID3D10Resource_Release(r);
    }
    SAFE_RELEASE(d.dev);
}
TEST(d3dx_redist_optional_x10_texture_invalid) {
    NEED_DLL(X10);
    X10_TexMem f = P10("D3DX10CreateTextureFromMemory", X10_TexMem); REQUIRE(f);
    D10 d; if (!d10_open(&d)) return;
    ID3D10Resource *r = NULL; static const BYTE junk[32] = { 5, 5, 5 };
    HRESULT hr = f(d.dev, junk, sizeof junk, NULL, NULL, &r, NULL); CHECK(FAILED(hr)); CHECK(r == NULL); t_note("junk -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(d.dev);
}
TEST(d3dx_redist_optional_x10_srv_from_memory) {
    NEED_DLL(X10);
    X10_SRVMem f = P10("D3DX10CreateShaderResourceViewFromMemory", X10_SRVMem); REQUIRE(f);
    D10 d; if (!d10_open(&d)) return;
    ID3D10ShaderResourceView *v = NULL;
    EXPECT_HR(f(d.dev, dxa_png_2x2, sizeof dxa_png_2x2, NULL, NULL, &v, NULL), S_OK);
    if (v) { D3D10_SHADER_RESOURCE_VIEW_DESC vd; ID3D10ShaderResourceView_GetDesc(v, &vd); EXPECT_EQ(vd.Format, DXGI_FORMAT_R8G8B8A8_UNORM); EXPECT_EQ(vd.ViewDimension, D3D10_SRV_DIMENSION_TEXTURE2D); CHECK((ULONG)ID3D10ShaderResourceView_Release(v) == 0); }
    SAFE_RELEASE(d.dev);
}
TEST(d3dx_redist_optional_x10_sh_project_constant_cube) {
    NEED_DLL(X10);
    X10_SHProj f = P10("D3DX10SHProjectCubeMap", X10_SHProj); REQUIRE(f);
    D10 d; if (!d10_open(&d)) return;
    DWORD white[16 * 6]; for (int i = 0; i < 16 * 6; i++) white[i] = 0xFFFFFFFF;
    D3D10_SUBRESOURCE_DATA sd[6]; for (int i = 0; i < 6; i++) { sd[i].pSysMem = white + i * 16; sd[i].SysMemPitch = 16; sd[i].SysMemSlicePitch = 0; }
    D3D10_TEXTURE2D_DESC td; memset(&td, 0, sizeof td); td.Width = td.Height = 4; td.MipLevels = 1; td.ArraySize = 6; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D10_USAGE_DEFAULT; td.BindFlags = D3D10_BIND_SHADER_RESOURCE; td.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
    ID3D10Texture2D *cube = NULL;
    if (FAILED(ID3D10Device_CreateTexture2D(d.dev, &td, sd, &cube))) { t_skip("cube texture creation failed"); SAFE_RELEASE(d.dev); return; }
    float r[16], g[16], b[16]; for (int i = 0; i < 16; i++) r[i] = g[i] = b[i] = -77.f;
    HRESULT hr = f(4, cube, r, g, b); EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        CHECKNE(r[0], 3.5449077f, 0.03f); CHECKNE(g[0], 3.5449077f, 0.03f); CHECKNE(b[0], 3.5449077f, 0.03f);
        for (int i = 1; i < 16; i++) CHECKF(fabsf(r[i]) < 0.05f, "r[%d] = %g", i, r[i]);
    }
    ID3D10Texture2D_Release(cube); SAFE_RELEASE(d.dev);
}
TEST(d3dx_redist_optional_x10_sh_project_order_bounds) {
    NEED_DLL(X10);
    X10_SHProj f = P10("D3DX10SHProjectCubeMap", X10_SHProj); REQUIRE(f);
    D10 d; if (!d10_open(&d)) return;
    D3D10_TEXTURE2D_DESC td; memset(&td, 0, sizeof td); td.Width = td.Height = 2; td.MipLevels = 1; td.ArraySize = 6; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D10_USAGE_DEFAULT; td.BindFlags = D3D10_BIND_SHADER_RESOURCE; td.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
    DWORD px[4 * 6] = {0}; D3D10_SUBRESOURCE_DATA sd[6]; for (int i = 0; i < 6; i++) { sd[i].pSysMem = px + i * 4; sd[i].SysMemPitch = 8; sd[i].SysMemSlicePitch = 0; }
    ID3D10Texture2D *cube = NULL;
    if (FAILED(ID3D10Device_CreateTexture2D(d.dev, &td, sd, &cube))) { t_skip("cube creation failed"); SAFE_RELEASE(d.dev); return; }
    float r[36], g[36], b[36];
    for (UINT ord = 2; ord <= 6; ord++) { HRESULT hr = f(ord, cube, r, g, b); CHECKF(SUCCEEDED(hr), "order %u hr %s", ord, d3dh_hr_name(hr)); if (SUCCEEDED(hr)) CHECKNE(r[0], 0.f, 1e-5f); }
    HRESULT h1 = f(1, cube, r, g, b), h7 = f(7, cube, r, g, b);
    t_note("order 1 -> %s, order 7 -> %s", d3dh_hr_name(h1), d3dh_hr_name(h7));
    ID3D10Texture2D_Release(cube); SAFE_RELEASE(d.dev);
}

/* ---- compile (D3DX10/11CompileFromMemory) ----------------------------------------------------- */
static const char CS_SRC[] = "float4 c; float4 main(float4 p : POSITION) : SV_Position { return p + c; }\n";
static void compile_ok(const char *dll, const char *fn) {
    X10_CompMem f = (X10_CompMem)t_proc_in(dll, fn); if (!f) { t_skip("%s missing", fn); return; }
    ID3D10Blob *sh = NULL, *err = NULL;
    HRESULT hr = f(CS_SRC, sizeof CS_SRC - 1, "t.hlsl", NULL, NULL, "main", "vs_4_0", 0, 0, NULL, &sh, &err, NULL);
    EXPECT_HR(hr, S_OK);
    if (sh) {
        const DWORD *p = ID3D10Blob_GetBufferPointer(sh); SIZE_T n = ID3D10Blob_GetBufferSize(sh);
        CHECK(n > 32); EXPECT_EQ(p[0], MAKEFOURCC('D','X','B','C')); EXPECT_EQ(p[6], n);   /* DXBC total size field at offset 24 */
        ID3D10Blob_Release(sh);
    }
    if (err) ID3D10Blob_Release(err);
}
TEST(d3dx_redist_optional_x10_compile_from_memory) { NEED_DLL(X10); compile_ok(X10, "D3DX10CompileFromMemory"); }
TEST(d3dx_redist_optional_x11_compile_from_memory) { NEED_DLL(X11); compile_ok(X11, "D3DX11CompileFromMemory"); }
static void compile_bad(const char *dll, const char *fn) {
    X10_CompMem f = (X10_CompMem)t_proc_in(dll, fn); if (!f) { t_skip("%s missing", fn); return; }
    static const char bad[] = "float4 main( : SV_Position { return 1 }";
    ID3D10Blob *sh = NULL, *err = NULL;
    HRESULT hr = f(bad, sizeof bad - 1, "bad.hlsl", NULL, NULL, "main", "vs_4_0", 0, 0, NULL, &sh, &err, NULL);
    CHECK(FAILED(hr)); CHECK(sh == NULL); CHECK(err != NULL); t_note("%s syntax error -> %s", fn, d3dh_hr_name(hr));
    if (err) { const char *m = ID3D10Blob_GetBufferPointer(err); CHECK(strstr(m, "bad.hlsl") != NULL); t_note("message: %.100s", m); ID3D10Blob_Release(err); err = NULL; }
    hr = f(CS_SRC, sizeof CS_SRC - 1, "t.hlsl", NULL, NULL, "nosuchentry", "vs_4_0", 0, 0, NULL, &sh, &err, NULL);
    CHECK(FAILED(hr)); if (err) ID3D10Blob_Release(err);
    hr = f(CS_SRC, sizeof CS_SRC - 1, "t.hlsl", NULL, NULL, "main", "vs_9_9", 0, 0, NULL, &sh, &err, NULL);
    CHECK(FAILED(hr)); if (err) ID3D10Blob_Release(err);
}
TEST(d3dx_redist_optional_x10_compile_errors) { NEED_DLL(X10); compile_bad(X10, "D3DX10CompileFromMemory"); }
TEST(d3dx_redist_optional_x11_compile_errors) { NEED_DLL(X11); compile_bad(X11, "D3DX11CompileFromMemory"); }
TEST(d3dx_redist_optional_x11_compile_reflects_like_d3dcompiler) {
    NEED_DLL(X11);
    X10_CompMem f = (X10_CompMem)t_proc_in(X11, "D3DX11CompileFromMemory"); REQUIRE(f); REQUIRE(d3dh_D3DReflect());
    ID3D10Blob *sh = NULL, *err = NULL;
    if (FAILED(f(CS_SRC, sizeof CS_SRC - 1, "t.hlsl", NULL, NULL, "main", "vs_4_0", 0, 0, NULL, &sh, &err, NULL))) { t_skip("compile failed"); return; }
    ID3D11ShaderReflection *rf = NULL;
    HRESULT hr = d3dh_D3DReflect()(ID3D10Blob_GetBufferPointer(sh), ID3D10Blob_GetBufferSize(sh), &IID_ID3D11ShaderReflection, (void **)&rf);
    t_note("D3DReflect (compiler 47) on a 43-compiled blob: %s", d3dh_hr_name(hr));
    if (rf) {
        D3D11_SHADER_DESC sd; rf->lpVtbl->GetDesc(rf, &sd);
        EXPECT_EQ(sd.ConstantBuffers, 1); EXPECT_EQ(sd.InputParameters, 1); EXPECT_EQ(sd.OutputParameters, 1);
        rf->lpVtbl->Release(rf);
    }
    ID3D10Blob_Release(sh); if (err) ID3D10Blob_Release(err);
}

/* ---- thread pump ----------------------------------------------------------------------------- */
TEST(d3dx_redist_optional_x10_thread_pump) {
    NEED_DLL(X10);
    X10_Pump f = P10("D3DX10CreateThreadPump", X10_Pump); REQUIRE(f);
    void *pump = NULL; HRESULT hr = f(0, 0, &pump);
    t_note("D3DX10CreateThreadPump(0,0) -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && pump) { IUnknown *u = pump; CHECK(IUnknown_Release(u) == 0); }
    pump = NULL; hr = f(1, 1, &pump);
    CHECK(SUCCEEDED(hr)); t_note("D3DX10CreateThreadPump(1,1) -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && pump) {
        typedef struct { void **vt; } OBJ; OBJ *o = pump;
        /* ID3DX10ThreadPump: 3 IUnknown slots, AddWorkItem, GetWorkItemCount(UINT) at slot 4 */
        typedef UINT (WINAPI *cnt_fn)(void *);
        UINT n = ((cnt_fn)o->vt[4])(pump);
        EXPECT_EQ(n, 0);
        IUnknown_Release((IUnknown *)pump);
    }
}

/* ---- math parity between d3dx9_43 and d3dx10_43 -------------------------------------------------- */
static int bits_equal(const void *a, const void *b, size_t n) { return !memcmp(a, b, n); }
TEST(d3dx_redist_optional_x10_math_matches_x9_matrices) {
    NEED_DLL(X9); NEED_DLL(X10);
    typedef D3DXMATRIX *(WINAPI *m_rot)(D3DXMATRIX *, float);
    typedef D3DXMATRIX *(WINAPI *m_mul)(D3DXMATRIX *, const D3DXMATRIX *, const D3DXMATRIX *);
    typedef D3DXMATRIX *(WINAPI *m_inv)(D3DXMATRIX *, float *, const D3DXMATRIX *);
    typedef D3DXMATRIX *(WINAPI *m_pfov)(D3DXMATRIX *, float, float, float, float);
    typedef D3DXMATRIX *(WINAPI *m_look)(D3DXMATRIX *, const D3DXVECTOR3 *, const D3DXVECTOR3 *, const D3DXVECTOR3 *);
    static const char *rots[] = { "D3DXMatrixRotationX", "D3DXMatrixRotationY", "D3DXMatrixRotationZ" };
    for (int i = 0; i < 3; i++) {
        m_rot a = (m_rot)t_proc_in(X9, rots[i]), b = (m_rot)t_proc_in(X10, rots[i]); if (!a || !b) { t_note("%s missing", rots[i]); continue; }
        for (float ang = -3.f; ang <= 3.f; ang += 0.5f) { D3DXMATRIX m1, m2; a(&m1, ang); b(&m2, ang); CHECKF(dx_mat_near(&m1, (const float *)&m2, 1e-6f), "%s(%g) differs", rots[i], ang); }
    }
    m_mul mul9 = (m_mul)t_proc_in(X9, "D3DXMatrixMultiply"), mul10 = (m_mul)t_proc_in(X10, "D3DXMatrixMultiply");
    m_inv inv9 = (m_inv)t_proc_in(X9, "D3DXMatrixInverse"), inv10 = (m_inv)t_proc_in(X10, "D3DXMatrixInverse");
    m_pfov pf9 = (m_pfov)t_proc_in(X9, "D3DXMatrixPerspectiveFovLH"), pf10 = (m_pfov)t_proc_in(X10, "D3DXMatrixPerspectiveFovLH");
    m_look lk9 = (m_look)t_proc_in(X9, "D3DXMatrixLookAtLH"), lk10 = (m_look)t_proc_in(X10, "D3DXMatrixLookAtLH");
    if (mul9 && mul10) { D3DXMATRIX a = { { { 1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16 } } }, b = { { { 2,0,1,0, 0,3,0,1, 1,0,4,0, 0,1,0,5 } } }, r1, r2; mul9(&r1, &a, &b); mul10(&r2, &a, &b); CHECK(dx_mat_near(&r1, (const float *)&r2, 1e-6f)); }
    if (inv9 && inv10) { D3DXMATRIX a = { { { 2,0,1,0, 0,3,0,1, 1,0,4,0, 0,1,0,5 } } }, r1, r2; float d1, d2; inv9(&r1, &d1, &a); inv10(&r2, &d2, &a); CHECK(dx_mat_near(&r1, (const float *)&r2, 1e-5f)); CHECKNE(d1, d2, 1e-4f); }
    if (pf9 && pf10) { D3DXMATRIX r1, r2; pf9(&r1, 1.f, 1.5f, 0.1f, 50.f); pf10(&r2, 1.f, 1.5f, 0.1f, 50.f); CHECK(dx_mat_near(&r1, (const float *)&r2, 1e-6f)); }
    if (lk9 && lk10) { D3DXVECTOR3 e = {1,2,3}, a = {0,0,0}, u = {0,1,0}; D3DXMATRIX r1, r2; lk9(&r1, &e, &a, &u); lk10(&r2, &e, &a, &u); CHECK(dx_mat_near(&r1, (const float *)&r2, 1e-6f)); }
}
TEST(d3dx_redist_optional_x10_math_matches_x9_float16) {
    NEED_DLL(X9); NEED_DLL(X10);
    typedef D3DXFLOAT16 *(WINAPI *to16)(D3DXFLOAT16 *, const float *, UINT);
    typedef float *(WINAPI *to32)(float *, const D3DXFLOAT16 *, UINT);
    to16 a16 = (to16)t_proc_in(X9, "D3DXFloat32To16Array"), b16 = (to16)t_proc_in(X10, "D3DXFloat32To16Array");
    to32 a32 = (to32)t_proc_in(X9, "D3DXFloat16To32Array"), b32 = (to32)t_proc_in(X10, "D3DXFloat16To32Array");
    REQUIRE(a16); REQUIRE(b16); REQUIRE(a32); REQUIRE(b32);
    static WORD h[65536], r1[65536], r2[65536]; static float f1[65536], f2[65536];
    for (int i = 0; i < 65536; i++) h[i] = (WORD)i;
    a32(f1, (const D3DXFLOAT16 *)h, 65536); b32(f2, (const D3DXFLOAT16 *)h, 65536);
    int diff = 0; for (int i = 0; i < 65536; i++) if (memcmp(&f1[i], &f2[i], 4)) diff++;
    CHECKF(diff == 0, "%d half->float conversions differ between the two DLLs", diff);
    a16((D3DXFLOAT16 *)r1, f1, 65536); b16((D3DXFLOAT16 *)r2, f1, 65536);
    diff = 0; for (int i = 0; i < 65536; i++) if (r1[i] != r2[i]) diff++;
    CHECKF(diff == 0, "%d float->half conversions differ", diff);
}
TEST(d3dx_redist_optional_x10_math_x9_x10_sh_parity) {
    NEED_DLL(X9); NEED_DLL(X10);
    typedef float *(WINAPI *ev)(float *, UINT, const D3DXVECTOR3 *);
    ev a = (ev)t_proc_in(X9, "D3DXSHEvalDirection"), b = (ev)t_proc_in(X10, "D3DXSHEvalDirection"); REQUIRE(a); REQUIRE(b);
    D3DXVECTOR3 dirs[] = { {0,0,1}, {1,0,0}, {0.36f,0.48f,0.8f}, {-0.6f,0.0f,0.8f} };
    for (int d = 0; d < 4; d++) for (UINT ord = 2; ord <= 6; ord++) { float o1[36], o2[36]; a(o1, ord, &dirs[d]); b(o2, ord, &dirs[d]); CHECK(bits_equal(o1, o2, ord * ord * 4)); }
}
TEST(d3dx_redist_optional_x10_math_matrix_identity_like_x9) {
    NEED_DLL(X10);
    typedef D3DXMATRIX *(WINAPI *mt)(D3DXMATRIX *, float, float, float);
    mt t = (mt)t_proc_in(X10, "D3DXMatrixTranslation"); REQUIRE(t);
    D3DXMATRIX m; t(&m, 1, 2, 3); CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,1);
}
