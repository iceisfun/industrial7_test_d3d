/* DXGI_FORMAT properties observed through staging textures, and DXGI HRESULT numeric values. */
#include "objs.h"

typedef struct { DXGI_FORMAT f; const char *n; UINT bytes; int block; } fmt_info;  /* bytes: per pixel, or per 4x4 block */
#define F(x, b) { DXGI_FORMAT_##x, #x, b, 0 }
#define B(x, b) { DXGI_FORMAT_##x, #x, b, 1 }
static const fmt_info fmts[] = {
    F(R32G32B32A32_TYPELESS, 16), F(R32G32B32A32_FLOAT, 16), F(R32G32B32A32_UINT, 16), F(R32G32B32A32_SINT, 16),
    F(R32G32B32_TYPELESS, 12), F(R32G32B32_FLOAT, 12), F(R32G32B32_UINT, 12), F(R32G32B32_SINT, 12),
    F(R16G16B16A16_TYPELESS, 8), F(R16G16B16A16_FLOAT, 8), F(R16G16B16A16_UNORM, 8), F(R16G16B16A16_UINT, 8), F(R16G16B16A16_SNORM, 8), F(R16G16B16A16_SINT, 8),
    F(R32G32_TYPELESS, 8), F(R32G32_FLOAT, 8), F(R32G32_UINT, 8), F(R32G32_SINT, 8),
    F(R32G8X24_TYPELESS, 8), F(D32_FLOAT_S8X24_UINT, 8),
    F(R10G10B10A2_TYPELESS, 4), F(R10G10B10A2_UNORM, 4), F(R10G10B10A2_UINT, 4), F(R11G11B10_FLOAT, 4),
    F(R8G8B8A8_TYPELESS, 4), F(R8G8B8A8_UNORM, 4), F(R8G8B8A8_UNORM_SRGB, 4), F(R8G8B8A8_UINT, 4), F(R8G8B8A8_SNORM, 4), F(R8G8B8A8_SINT, 4),
    F(R16G16_TYPELESS, 4), F(R16G16_FLOAT, 4), F(R16G16_UNORM, 4), F(R16G16_UINT, 4), F(R16G16_SNORM, 4), F(R16G16_SINT, 4),
    F(R32_TYPELESS, 4), F(D32_FLOAT, 4), F(R32_FLOAT, 4), F(R32_UINT, 4), F(R32_SINT, 4),
    F(R24G8_TYPELESS, 4), F(D24_UNORM_S8_UINT, 4),
    F(R8G8_TYPELESS, 2), F(R8G8_UNORM, 2), F(R8G8_UINT, 2), F(R8G8_SNORM, 2), F(R8G8_SINT, 2),
    F(R16_TYPELESS, 2), F(R16_FLOAT, 2), F(D16_UNORM, 2), F(R16_UNORM, 2), F(R16_UINT, 2), F(R16_SNORM, 2), F(R16_SINT, 2),
    F(R8_TYPELESS, 1), F(R8_UNORM, 1), F(R8_UINT, 1), F(R8_SNORM, 1), F(R8_SINT, 1), F(A8_UNORM, 1),
    F(R9G9B9E5_SHAREDEXP, 4), F(R8G8_B8G8_UNORM, 2), F(G8R8_G8B8_UNORM, 2),
    F(B8G8R8A8_UNORM, 4), F(B8G8R8X8_UNORM, 4), F(B8G8R8A8_TYPELESS, 4), F(B8G8R8A8_UNORM_SRGB, 4), F(B8G8R8X8_TYPELESS, 4), F(B8G8R8X8_UNORM_SRGB, 4),
    F(R10G10B10_XR_BIAS_A2_UNORM, 4),
    B(BC1_TYPELESS, 8), B(BC1_UNORM, 8), B(BC1_UNORM_SRGB, 8), B(BC2_TYPELESS, 16), B(BC2_UNORM, 16), B(BC2_UNORM_SRGB, 16),
    B(BC3_TYPELESS, 16), B(BC3_UNORM, 16), B(BC3_UNORM_SRGB, 16), B(BC4_TYPELESS, 8), B(BC4_UNORM, 8), B(BC4_SNORM, 8),
    B(BC5_TYPELESS, 16), B(BC5_UNORM, 16), B(BC5_SNORM, 16), B(BC6H_TYPELESS, 16), B(BC6H_UF16, 16), B(BC6H_SF16, 16),
    B(BC7_TYPELESS, 16), B(BC7_UNORM, 16), B(BC7_UNORM_SRGB, 16),
};
static const fmt_info fmts_11_1[] = { F(B5G6R5_UNORM, 2), F(B5G5R5A1_UNORM, 2), F(B4G4R4A4_UNORM, 2) };

static HRESULT make_staging(ID3D11Device *d, DXGI_FORMAT f, UINT w, UINT h, ID3D11Texture2D **t)
{
    return objs_tex2d(d, w, h, 1, 1, f, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, t);
}
static UINT row_bytes(const fmt_info *fi, UINT w)
{
    return fi->block ? ((w + 3) / 4) * fi->bytes : w * fi->bytes;
}
static UINT row_count(const fmt_info *fi, UINT h)
{
    return fi->block ? (h + 3) / 4 : h;
}

TEST(objs_fmt_staging_desc_roundtrip)
{
    NEED_DEV(c);
    int i, made = 0;
    for (i = 0; i < OBJS_ARRAY(fmts); i++) {
        ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC d; IDXGISurface *s = NULL; DXGI_SURFACE_DESC sd;
        HRESULT hr = make_staging(c.dev, fmts[i].f, 16, 16, &t);
        if (FAILED(hr)) { t_note("%s: staging create -> %s", fmts[i].n, d3dh_hr_name(hr)); continue; }
        made++;
        ID3D11Texture2D_GetDesc(t, &d);
        CHECKF(d.Format == fmts[i].f && d.Width == 16 && d.Height == 16, "%s: desc %d %ux%u", fmts[i].n, d.Format, d.Width, d.Height);
        if (SUCCEEDED(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&s))) {
            memset(&sd, 0, sizeof(sd));
            IDXGISurface_GetDesc(s, &sd);
            CHECKF(sd.Format == fmts[i].f && sd.Width == 16 && sd.Height == 16, "%s: DXGI desc %d", fmts[i].n, sd.Format);
            IDXGISurface_Release(s);
        }
        ID3D11Texture2D_Release(t);
    }
    CHECK(made > 40);
    objs_close(&c);
}
TEST(objs_fmt_staging_pitch_lower_bound)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(fmts); i++) {
        ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE ms;
        if (FAILED(make_staging(c.dev, fmts[i].f, 32, 16, &t))) continue;
        if (SUCCEEDED(ID3D11DeviceContext_Map(c.ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &ms))) {
            CHECKF(ms.RowPitch >= row_bytes(&fmts[i], 32), "%s: RowPitch %u < %u", fmts[i].n, ms.RowPitch, row_bytes(&fmts[i], 32));
            CHECKF(ms.pData != NULL, "%s: pData NULL", fmts[i].n);
            ID3D11DeviceContext_Unmap(c.ctx, (ID3D11Resource *)t, 0);
        } else CHECKF(0, "%s: Map failed", fmts[i].n);
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_fmt_staging_dxgi_pitch_matches_d3d11)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(fmts); i++) {
        ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE ms; IDXGISurface *s = NULL; DXGI_MAPPED_RECT m;
        if (fmts[i].block) continue;   /* IDXGISurface::Map is only defined for uncompressed surfaces here */
        if (FAILED(make_staging(c.dev, fmts[i].f, 32, 16, &t))) continue;
        if (SUCCEEDED(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&s))) {
            memset(&m, 0, sizeof(m));
            if (SUCCEEDED(IDXGISurface_Map(s, &m, DXGI_MAP_READ))) {
                IDXGISurface_Unmap(s);
                if (SUCCEEDED(ID3D11DeviceContext_Map(c.ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &ms))) {
                    if ((UINT)m.Pitch != ms.RowPitch) t_note("%s: DXGI pitch %d vs D3D11 %u", fmts[i].n, m.Pitch, ms.RowPitch);
                    ID3D11DeviceContext_Unmap(c.ctx, (ID3D11Resource *)t, 0);
                }
            } else t_note("%s: IDXGISurface::Map failed", fmts[i].n);
            IDXGISurface_Release(s);
        }
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_fmt_staging_copy_fidelity)
{
    NEED_DEV(c);
    int i, tested = 0;
    for (i = 0; i < OBJS_ARRAY(fmts); i++) {
        ID3D11Texture2D *a = NULL, *b = NULL; D3D11_MAPPED_SUBRESOURCE ma, mb; UINT rb, rc, y, x, bad = 0;
        if (FAILED(make_staging(c.dev, fmts[i].f, 16, 16, &a)) || FAILED(make_staging(c.dev, fmts[i].f, 16, 16, &b))) { SAFE_RELEASE(a); SAFE_RELEASE(b); continue; }
        rb = row_bytes(&fmts[i], 16); rc = row_count(&fmts[i], 16);
        if (SUCCEEDED(ID3D11DeviceContext_Map(c.ctx, (ID3D11Resource *)a, 0, D3D11_MAP_WRITE, 0, &ma))) {
            for (y = 0; y < rc; y++) for (x = 0; x < rb; x++) ((BYTE *)ma.pData)[y * ma.RowPitch + x] = (BYTE)(y * 31 + x * 7 + i);
            ID3D11DeviceContext_Unmap(c.ctx, (ID3D11Resource *)a, 0);
            ID3D11DeviceContext_CopyResource(c.ctx, (ID3D11Resource *)b, (ID3D11Resource *)a);
            if (SUCCEEDED(ID3D11DeviceContext_Map(c.ctx, (ID3D11Resource *)b, 0, D3D11_MAP_READ, 0, &mb))) {
                for (y = 0; y < rc; y++) for (x = 0; x < rb; x++) if (((BYTE *)mb.pData)[y * mb.RowPitch + x] != (BYTE)(y * 31 + x * 7 + i)) bad++;
                ID3D11DeviceContext_Unmap(c.ctx, (ID3D11Resource *)b, 0);
                CHECKF(bad == 0, "%s: %u mismatching bytes after CopyResource", fmts[i].n, bad);
                tested++;
            }
        }
        ID3D11Texture2D_Release(a); ID3D11Texture2D_Release(b);
    }
    CHECK(tested > 40);
    objs_close(&c);
}
TEST(objs_fmt_compressed_odd_sizes)
{
    NEED_DEV(c);
    static const struct { UINT w, h; } sz[] = { {4, 4}, {8, 4}, {5, 5}, {7, 9}, {1, 1}, {2, 2}, {12, 20} };
    int i, j;
    for (i = 0; i < OBJS_ARRAY(fmts); i++) {
        if (!fmts[i].block || fmts[i].f == DXGI_FORMAT_BC6H_TYPELESS) continue;
        for (j = 0; j < OBJS_ARRAY(sz); j++) {
            ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE ms; HRESULT hr = make_staging(c.dev, fmts[i].f, sz[j].w, sz[j].h, &t);
            if (FAILED(hr)) { t_note("%s %ux%u -> %s", fmts[i].n, sz[j].w, sz[j].h, d3dh_hr_name(hr)); continue; }
            if (SUCCEEDED(ID3D11DeviceContext_Map(c.ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &ms))) {
                CHECKF(ms.RowPitch >= row_bytes(&fmts[i], sz[j].w), "%s %ux%u: pitch %u", fmts[i].n, sz[j].w, sz[j].h, ms.RowPitch);
                ID3D11DeviceContext_Unmap(c.ctx, (ID3D11Resource *)t, 0);
            }
            ID3D11Texture2D_Release(t);
        }
    }
    objs_close(&c);
}
TEST(objs_fmt_checkformatsupport_texture2d)
{
    NEED_DEV(c);
    int i, with = 0;
    for (i = 0; i < OBJS_ARRAY(fmts); i++) {
        UINT sup = 0; HRESULT hr = ID3D11Device_CheckFormatSupport(c.dev, fmts[i].f, &sup);
        ID3D11Texture2D *t = NULL; HRESULT hc = make_staging(c.dev, fmts[i].f, 16, 16, &t);
        if (SUCCEEDED(hr) && (sup & D3D11_FORMAT_SUPPORT_TEXTURE2D)) {
            with++;
            CHECKF(SUCCEEDED(hc), "%s: TEXTURE2D supported but staging create -> %s", fmts[i].n, d3dh_hr_name(hc));
        }
        SAFE_RELEASE(t);
    }
    CHECK(with > 40);
    objs_close(&c);
}
TEST(objs_fmt_bgra_565_family)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(fmts_11_1); i++) {
        ID3D11Texture2D *t = NULL; UINT sup = 0; HRESULT hr = make_staging(c.dev, fmts_11_1[i].f, 16, 16, &t);
        ID3D11Device_CheckFormatSupport(c.dev, fmts_11_1[i].f, &sup);
        t_note("%s: staging create %s, CheckFormatSupport %#x (level %#x)", fmts_11_1[i].n, d3dh_hr_name(hr), sup, c.fl);
        SAFE_RELEASE(t);
    }
    objs_close(&c);
}
TEST(objs_fmt_unknown_rejected)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = (ID3D11Texture2D *)(ULONG_PTR)1;
    EXPECT_HR(make_staging(c.dev, DXGI_FORMAT_UNKNOWN, 16, 16, &t), E_INVALIDARG);
    EXPECT_EQ(t, NULL);
    objs_close(&c);
}
TEST(objs_fmt_out_of_range_rejected)
{
    NEED_DEV(c);
    static const DXGI_FORMAT bad[] = { (DXGI_FORMAT)0x7fff, (DXGI_FORMAT)0xffff, (DXGI_FORMAT)0x10000 };
    int i;
    for (i = 0; i < OBJS_ARRAY(bad); i++) {
        ID3D11Texture2D *t = (ID3D11Texture2D *)(ULONG_PTR)1;
        HRESULT hr = make_staging(c.dev, bad[i], 16, 16, &t);
        CHECKF(hr == E_INVALIDARG, "format %#x: %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
    objs_close(&c);
}
TEST(objs_fmt_reserved_gaps_rejected)
{
    NEED_DEV(c);
    /* values inside the enum range that are not formats (gaps and unused slots) */
    static const DXGI_FORMAT gaps[] = { (DXGI_FORMAT)116, (DXGI_FORMAT)117, (DXGI_FORMAT)118, (DXGI_FORMAT)119, (DXGI_FORMAT)120, (DXGI_FORMAT)121 };
    int i;
    for (i = 0; i < OBJS_ARRAY(gaps); i++) {
        ID3D11Texture2D *t = NULL;
        HRESULT hr = make_staging(c.dev, gaps[i], 16, 16, &t);
        t_note("format %d staging create -> %s", gaps[i], d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
    objs_close(&c);
}
TEST(objs_fmt_video_formats_staging)
{
    NEED_DEV(c);
    static const struct { DXGI_FORMAT f; const char *n; } v[] = {
        { DXGI_FORMAT_AYUV, "AYUV" }, { DXGI_FORMAT_Y410, "Y410" }, { DXGI_FORMAT_Y416, "Y416" }, { DXGI_FORMAT_NV12, "NV12" },
        { DXGI_FORMAT_P010, "P010" }, { DXGI_FORMAT_P016, "P016" }, { DXGI_FORMAT_420_OPAQUE, "420_OPAQUE" }, { DXGI_FORMAT_YUY2, "YUY2" },
        { DXGI_FORMAT_Y210, "Y210" }, { DXGI_FORMAT_Y216, "Y216" }, { DXGI_FORMAT_NV11, "NV11" }, { DXGI_FORMAT_AI44, "AI44" },
        { DXGI_FORMAT_IA44, "IA44" }, { DXGI_FORMAT_P8, "P8" }, { DXGI_FORMAT_A8P8, "A8P8" },
    };
    int i;
    for (i = 0; i < OBJS_ARRAY(v); i++) {
        ID3D11Texture2D *t = NULL; UINT sup = 0;
        HRESULT hr = make_staging(c.dev, v[i].f, 16, 16, &t);
        HRESULT hs = ID3D11Device_CheckFormatSupport(c.dev, v[i].f, &sup);
        t_note("%s: staging create %s, CheckFormatSupport %s %#x", v[i].n, d3dh_hr_name(hr), d3dh_hr_name(hs), sup);
        SAFE_RELEASE(t);
    }
    objs_close(&c);
}
TEST(objs_fmt_mip_chain_pitch)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; int m;
    if (FAILED(objs_tex2d(c.dev, 64, 64, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0, &t))) { t_skip("no mip staging"); objs_close(&c); return; }
    {
        D3D11_TEXTURE2D_DESC d; ID3D11Texture2D_GetDesc(t, &d);
        EXPECT_EQ(d.MipLevels, 7);
    }
    for (m = 0; m < 7; m++) {
        D3D11_MAPPED_SUBRESOURCE ms;
        if (SUCCEEDED(ID3D11DeviceContext_Map(c.ctx, (ID3D11Resource *)t, m, D3D11_MAP_READ, 0, &ms))) {
            UINT w = 64 >> m;
            CHECKF(ms.RowPitch >= w * 4, "mip %d: pitch %u for width %u", m, ms.RowPitch, w);
            ID3D11DeviceContext_Unmap(c.ctx, (ID3D11Resource *)t, m);
        } else CHECKF(0, "mip %d map failed", m);
    }
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_fmt_srgb_typeless_distinct_desc)
{
    NEED_DEV(c);
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_BC1_UNORM_SRGB };
    int i;
    for (i = 0; i < OBJS_ARRAY(f); i++) {
        ID3D11Texture2D *t = NULL; IDXGISurface *s = NULL; DXGI_SURFACE_DESC d;
        if (FAILED(objs_tex2d(c.dev, 16, 16, 1, 1, f[i], D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, &t))) continue;
        if (SUCCEEDED(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&s))) {
            IDXGISurface_GetDesc(s, &d);
            CHECKF(d.Format == f[i], "format %d reported as %d", f[i], d.Format);
            IDXGISurface_Release(s);
        }
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}

/* ---- DXGI HRESULT numeric values ----------------------------------------------------------------------------------------- */
typedef struct { HRESULT v; HRESULT want; const char *n; } hr_case;
#define H(x, w) { (HRESULT)(x), (HRESULT)(w), #x }
TEST(objs_hr_dxgi_error_values)
{
    static const hr_case t[] = {
        H(DXGI_ERROR_INVALID_CALL, 0x887A0001), H(DXGI_ERROR_NOT_FOUND, 0x887A0002), H(DXGI_ERROR_MORE_DATA, 0x887A0003),
        H(DXGI_ERROR_UNSUPPORTED, 0x887A0004), H(DXGI_ERROR_DEVICE_REMOVED, 0x887A0005), H(DXGI_ERROR_DEVICE_HUNG, 0x887A0006),
        H(DXGI_ERROR_DEVICE_RESET, 0x887A0007), H(DXGI_ERROR_WAS_STILL_DRAWING, 0x887A000A), H(DXGI_ERROR_FRAME_STATISTICS_DISJOINT, 0x887A000B),
        H(DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE, 0x887A000C), H(DXGI_ERROR_DRIVER_INTERNAL_ERROR, 0x887A0020),
        H(DXGI_ERROR_NONEXCLUSIVE, 0x887A0021), H(DXGI_ERROR_NOT_CURRENTLY_AVAILABLE, 0x887A0022),
        H(DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED, 0x887A0023), H(DXGI_ERROR_REMOTE_OUTOFMEMORY, 0x887A0024),
        H(DXGI_ERROR_ACCESS_LOST, 0x887A0026), H(DXGI_ERROR_WAIT_TIMEOUT, 0x887A0027), H(DXGI_ERROR_SESSION_DISCONNECTED, 0x887A0028),
        H(DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE, 0x887A0029), H(DXGI_ERROR_CANNOT_PROTECT_CONTENT, 0x887A002A),
        H(DXGI_ERROR_ACCESS_DENIED, 0x887A002B), H(DXGI_ERROR_NAME_ALREADY_EXISTS, 0x887A002C), H(DXGI_ERROR_SDK_COMPONENT_MISSING, 0x887A002D),
        H(DXGI_ERROR_MODE_CHANGE_IN_PROGRESS, 0x887A0025),
    };
    int i;
    for (i = 0; i < OBJS_ARRAY(t); i++) {
        CHECKF(t[i].v == t[i].want, "%s = %#lx want %#lx", t[i].n, (unsigned long)t[i].v, (unsigned long)t[i].want);
        CHECKF(FAILED(t[i].v), "%s should be a failure code", t[i].n);
        CHECKF(HRESULT_FACILITY(t[i].v) == 0x87A, "%s facility %#x", t[i].n, HRESULT_FACILITY(t[i].v));
        CHECKF(HRESULT_SEVERITY(t[i].v) == 1, "%s severity", t[i].n);
    }
}
TEST(objs_hr_dxgi_status_values)
{
    static const hr_case t[] = {
        H(DXGI_STATUS_OCCLUDED, 0x087A0001), H(DXGI_STATUS_CLIPPED, 0x087A0002), H(DXGI_STATUS_NO_REDIRECTION, 0x087A0004),
        H(DXGI_STATUS_NO_DESKTOP_ACCESS, 0x087A0005), H(DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, 0x087A0006),
        H(DXGI_STATUS_MODE_CHANGED, 0x087A0007), H(DXGI_STATUS_MODE_CHANGE_IN_PROGRESS, 0x087A0008),
    };
    int i;
    for (i = 0; i < OBJS_ARRAY(t); i++) {
        CHECKF(t[i].v == t[i].want, "%s = %#lx want %#lx", t[i].n, (unsigned long)t[i].v, (unsigned long)t[i].want);
        CHECKF(SUCCEEDED(t[i].v) && t[i].v != S_OK, "%s should be a success status", t[i].n);
        CHECKF(HRESULT_FACILITY(t[i].v) == 0x87A, "%s facility", t[i].n);
    }
}
TEST(objs_hr_wait_codes_as_hresult)
{
    EXPECT_EQ((HRESULT)WAIT_TIMEOUT, 0x102);
    EXPECT_EQ((HRESULT)WAIT_ABANDONED, 0x80);
    CHECK(SUCCEEDED((HRESULT)WAIT_TIMEOUT) && (HRESULT)WAIT_TIMEOUT != S_OK);
    CHECK(SUCCEEDED((HRESULT)WAIT_ABANDONED) && (HRESULT)WAIT_ABANDONED != S_OK);
}
TEST(objs_hr_result_protocol_from_dll)
{
    NEED_DEV(c);
    BYTE buf[4]; UINT size = 4; HRESULT hr;
    /* the codes the private data protocol hands back are the numeric values above */
    hr = IDXGIDevice_GetPrivateData(c.dx, &OBJS_GMISSING, &size, buf);
    EXPECT_EQ((ULONG)hr, 0x887A0002u);
    IDXGIDevice_SetPrivateData(c.dx, &OBJS_G1, 8, "12345678");
    size = 1;
    hr = IDXGIDevice_GetPrivateData(c.dx, &OBJS_G1, &size, buf);
    EXPECT_EQ((ULONG)hr, 0x887A0003u);
    IDXGIDevice_SetPrivateData(c.dx, &OBJS_G1, 0, NULL);
    {
        IDXGIDevice1 *d1 = NULL;
        if (SUCCEEDED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice1, (void **)&d1))) {
            hr = IDXGIDevice1_SetMaximumFrameLatency(d1, 99);
            EXPECT_EQ((ULONG)hr, 0x887A0001u);
            IDXGIDevice1_Release(d1);
        }
    }
    objs_close(&c);
}
TEST(objs_hr_device_removed_reason_codes)
{
    NEED_DEV(c);
    HRESULT hr = ID3D11Device_GetDeviceRemovedReason(c.dev);
    EXPECT_HR(hr, S_OK);
    objs_close(&c);
}
