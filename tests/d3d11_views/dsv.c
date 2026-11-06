/* CreateDepthStencilView */
#include "views.h"
#define UI_ALL ((UINT)-1)

static int get_null_dsv(ID3D11Device *dev, void *res, D3D11_DEPTH_STENCIL_VIEW_DESC *out, HRESULT *hr_out)
{
    ID3D11DepthStencilView *v = NULL; HRESULT hr = MK_DSV(dev, res, NULL, &v);
    *hr_out = hr; memset(out, 0xAB, sizeof *out);
    if (FAILED(hr)) return 0;
    ID3D11DepthStencilView_GetDesc(v, out);
    SAFE_RELEASE(v);
    return 1;
}
TEST(d3d11_views_dsv_null_typed_formats)
{
    NEED_DEV(); D3D11_DEPTH_STENCIL_VIEW_DESC d; HRESULT hr;
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT };
    for (unsigned i = 0; i < 4; i++) {
        ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, f[i], D3D11_BIND_DEPTH_STENCIL, 0, 1);
        if (!t) { CHECKF(0, "depth format %d not creatable", f[i]); continue; }
        CHECK(get_null_dsv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
        EXPECT_EQ(d.Format, f[i]);
        EXPECT_EQ(d.ViewDimension, D3D11_DSV_DIMENSION_TEXTURE2D);
        EXPECT_EQ(d.Flags, 0);
        EXPECT_EQ(d.Texture2D.MipSlice, 0);
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_dsv_null_array)
{
    NEED_DEV(); D3D11_DEPTH_STENCIL_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 4, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    CHECK(get_null_dsv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_DSV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(d.Flags, 0);
    EXPECT_EQ(d.Texture2DArray.MipSlice, 0);
    EXPECT_EQ(d.Texture2DArray.FirstArraySlice, 0);
    EXPECT_EQ(d.Texture2DArray.ArraySize, 4);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_null_ms)
{
    NEED_DEV(); D3D11_DEPTH_STENCIL_VIEW_DESC d; HRESULT hr;
    if (!ms_ok(dev, DXGI_FORMAT_D24_UNORM_S8_UINT, 4)) { DONE(); t_skip("4x MSAA depth unavailable"); return; }
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, 0, 4);
    ID3D11Texture2D *ta = mk2d(dev, 16, 16, 1, 2, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, 0, 4);
    REQUIRE(t); REQUIRE(ta);
    CHECK(get_null_dsv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_DSV_DIMENSION_TEXTURE2DMS);
    CHECK(get_null_dsv(dev, ta, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY);
    EXPECT_EQ(d.Texture2DMSArray.ArraySize, 2);
    SAFE_RELEASE(ta); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_null_tex1d)
{
    NEED_DEV(); D3D11_DEPTH_STENCIL_VIEW_DESC d; HRESULT hr;
    ID3D11Texture1D *t = mk1d(dev, 32, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL);
    ID3D11Texture1D *ta = mk1d(dev, 32, 1, 3, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL);
    if (!t || !ta) { SAFE_RELEASE(t); SAFE_RELEASE(ta); DONE(); t_skip("1D depth texture not creatable"); return; }
    CHECK(get_null_dsv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_DSV_DIMENSION_TEXTURE1D);
    CHECK(get_null_dsv(dev, ta, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_DSV_DIMENSION_TEXTURE1DARRAY);
    EXPECT_EQ(d.Texture1DArray.ArraySize, 3);
    SAFE_RELEASE(ta); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_null_typeless_fails)
{
    NEED_DEV(); D3D11_DEPTH_STENCIL_VIEW_DESC d; HRESULT hr;
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R32G8X24_TYPELESS };
    for (unsigned i = 0; i < 4; i++) {
        ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, f[i], D3D11_BIND_DEPTH_STENCIL, 0, 1);
        if (!t) { CHECKF(0, "typeless depth %d not creatable", f[i]); continue; }
        get_null_dsv(dev, t, &d, &hr);
        EXPECT_HR(hr, E_INV);
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_dsv_requires_bind_depth_stencil)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    ID3D11Texture2D *rt = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t); REQUIRE(rt);
    D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT, 0);
    EXPECT_HR(try_dsv(dev, R(t), &d), E_INV);
    EXPECT_HR(try_dsv(dev, R(rt), NULL), E_INV);
    EXPECT_HR(val_dsv(dev, R(t), &d), E_INV);
    SAFE_RELEASE(rt); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_color_format_on_depth_texture_fails)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    static const DXGI_FORMAT bad[] = { DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT,
        DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, bad[i], 0);
        HRESULT hr = try_dsv(dev, R(t), &d);
        t_check(hr == E_INV, __FILE__, __LINE__, "dsv bad fmt", "fmt %d got %s", bad[i], d3dh_hr_name(hr));
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC ok = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT, 0);
    EXPECT_HR(try_dsv(dev, R(t), &ok), S_OK);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_typeless_cast_table)
{
    NEED_DEV();
    static const struct { DXGI_FORMAT res, view; HRESULT want; } tab[] = {
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, S_OK},
        {DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, S_OK},
        {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_D16_UNORM, S_OK},
        {DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D16_UNORM, E_INVALIDARG},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, E_INVALIDARG},
        {DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D32_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_D32_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, E_INVALIDARG},
        {DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D16_UNORM, E_INVALIDARG},
        {DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D32_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D32_FLOAT, S_OK},
        {DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_UNKNOWN, S_OK},
        {DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_UNKNOWN, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, tab[i].res, D3D11_BIND_DEPTH_STENCIL, 0, 1);
        if (!t) { CHECKF(0, "depth res %d not creatable", tab[i].res); continue; }
        D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, tab[i].view, 0);
        HRESULT hr = try_dsv(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "dsv cast", "res %d view %d got %s want %s", tab[i].res, tab[i].view, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_dsv_read_only_flags_stencil_formats)
{
    NEED_DEV();
    static const UINT fl_tab[] = { 0, D3D11_DSV_READ_ONLY_DEPTH, D3D11_DSV_READ_ONLY_STENCIL, D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL };
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT };
    for (unsigned k = 0; k < 2; k++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, f[k], D3D11_BIND_DEPTH_STENCIL, 0, 1);
        if (!t) { CHECKF(0, "format %d not creatable", f[k]); continue; }
        for (unsigned i = 0; i < 4; i++) {
            D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, f[k], fl_tab[i]), o;
            ID3D11DepthStencilView *v = NULL;
            HRESULT hr = MK_DSV(dev, t, &d, &v);
            t_check(hr == S_OK, __FILE__, __LINE__, "dsv ro flags", "fmt %d flags %u got %s", f[k], fl_tab[i], d3dh_hr_name(hr));
            if (v) { memset(&o, 0, sizeof o); ID3D11DepthStencilView_GetDesc(v, &o); EXPECT_EQ(o.Flags, fl_tab[i]); EXPECT_EQ(o.Format, f[k]); }
            SAFE_RELEASE(v);
        }
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_dsv_read_only_flags_depth_only_formats)
{
    NEED_DEV();
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D16_UNORM };
    for (unsigned k = 0; k < 2; k++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, f[k], D3D11_BIND_DEPTH_STENCIL, 0, 1);
        if (!t) { CHECKF(0, "depth format %d not creatable", f[k]); continue; }
        D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, f[k], D3D11_DSV_READ_ONLY_DEPTH);
        EXPECT_HR(try_dsv(dev, R(t), &d), S_OK);
        d.Flags = D3D11_DSV_READ_ONLY_STENCIL;
        HRESULT hr = try_dsv(dev, R(t), &d);
        CHECKF(hr == E_INV, "READ_ONLY_STENCIL on format %d (no stencil) -> %s", f[k], d3dh_hr_name(hr));
        d.Flags = 0x4;
        EXPECT_HR(try_dsv(dev, R(t), &d), E_INV);          /* undefined flag bits are rejected for DSVs */
        d.Flags = 0x80000000u;
        EXPECT_HR(try_dsv(dev, R(t), &d), E_INV);
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_dsv_read_only_array_and_ms)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 3, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_DSV_READ_ONLY_DEPTH), o;
    d.Texture2DArray.ArraySize = 3;
    ID3D11DepthStencilView *v = NULL;
    EXPECT_HR(MK_DSV(dev, t, &d, &v), S_OK);
    if (v) { ID3D11DepthStencilView_GetDesc(v, &o); EXPECT_EQ(o.Flags, D3D11_DSV_READ_ONLY_DEPTH); EXPECT_EQ(o.Texture2DArray.ArraySize, 3); }
    SAFE_RELEASE(v); SAFE_RELEASE(t);
    if (ms_ok(dev, DXGI_FORMAT_D24_UNORM_S8_UINT, 4)) {
        ID3D11Texture2D *m = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, 0, 4);
        if (m) {
            D3D11_DEPTH_STENCIL_VIEW_DESC dm = dsvd(D3D11_DSV_DIMENSION_TEXTURE2DMS, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_DSV_READ_ONLY_STENCIL);
            EXPECT_HR(try_dsv(dev, R(m), &dm), S_OK);
            SAFE_RELEASE(m);
        }
    }
    DONE();
}
/* Win10: Texture2DMS is accepted for a single-sample depth texture; Texture2DMSArray is not. */
TEST(d3d11_views_dsv_ms_dim_accepted_on_single_sample_texture)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2DMS, DXGI_FORMAT_D32_FLOAT, 0);
    EXPECT_HR(try_dsv(dev, R(t), &d), S_OK);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_dimension_mismatch_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    static const D3D11_DSV_DIMENSION bad[] = { D3D11_DSV_DIMENSION_UNKNOWN, D3D11_DSV_DIMENSION_TEXTURE1D, D3D11_DSV_DIMENSION_TEXTURE1DARRAY,
        D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY, (D3D11_DSV_DIMENSION)7 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(bad[i], DXGI_FORMAT_D32_FLOAT, 0);
        d.Texture2DArray.ArraySize = 1;
        HRESULT hr = try_dsv(dev, R(t), &d);
        t_check(hr == E_INV, __FILE__, __LINE__, "dsv dim mismatch", "dim %d got %s", bad[i], d3dh_hr_name(hr));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_tex3d_and_buffer_fail)
{
    NEED_DEV();
    ID3D11Buffer *b = mkbuf(dev, 64, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(b);
    D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT, 0);
    EXPECT_HR(try_dsv(dev, R(b), &d), E_INV);
    EXPECT_HR(try_dsv(dev, R(b), NULL), E_INV);
    ID3D11Texture3D *t3 = mk3d(dev, 8, 8, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    if (t3) { EXPECT_HR(try_dsv(dev, R(t3), &d), E_INV); EXPECT_HR(try_dsv(dev, R(t3), NULL), E_INV); SAFE_RELEASE(t3); }
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_dsv_range_tables)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 4, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    static const struct { UINT mip, first, n; HRESULT want; } tab[] = {
        {0, 0, 4, S_OK}, {2, 3, 1, S_OK}, {1, 1, 3, S_OK},
        {3, 0, 1, E_INVALIDARG}, {0, 0, 5, E_INVALIDARG}, {0, 4, 1, E_INVALIDARG}, {0, 2, 3, E_INVALIDARG}, {0, 0, 0, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_D32_FLOAT, 0);
        d.Texture2DArray.MipSlice = tab[i].mip; d.Texture2DArray.FirstArraySlice = tab[i].first; d.Texture2DArray.ArraySize = tab[i].n;
        HRESULT hr = try_dsv(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "dsv range", "mip %u slice %u+%u got %s want %s", tab[i].mip, tab[i].first, tab[i].n, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC d2 = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT, 0);
    d2.Texture2D.MipSlice = 3;
    EXPECT_HR(try_dsv(dev, R(t), &d2), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_array_size_minus1)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 4, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_D32_FLOAT, 0);
    d.Texture2DArray.FirstArraySlice = 1; d.Texture2DArray.ArraySize = UI_ALL;
    HRESULT hr = try_dsv(dev, R(t), &d);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_desc_roundtrip)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 6, DXGI_FORMAT_R24G8_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_DSV_READ_ONLY_STENCIL), o;
    d.Texture2DArray.MipSlice = 1; d.Texture2DArray.FirstArraySlice = 2; d.Texture2DArray.ArraySize = 3;
    ID3D11DepthStencilView *v = NULL;
    EXPECT_HR(MK_DSV(dev, t, &d, &v), S_OK);
    REQUIRE(v);
    memset(&o, 0, sizeof o);
    ID3D11DepthStencilView_GetDesc(v, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_D24_UNORM_S8_UINT);
    EXPECT_EQ(o.ViewDimension, D3D11_DSV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(o.Flags, D3D11_DSV_READ_ONLY_STENCIL);
    EXPECT_EQ(o.Texture2DArray.MipSlice, 1);
    EXPECT_EQ(o.Texture2DArray.FirstArraySlice, 2);
    EXPECT_EQ(o.Texture2DArray.ArraySize, 3);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_shares_typeless_with_srv)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_DEPTH_STENCIL_VIEW_DESC dd = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT, 0);
    D3D11_SHADER_RESOURCE_VIEW_DESC sd = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
    sd.Texture2D.MipLevels = 1;
    ID3D11DepthStencilView *dv = NULL; ID3D11ShaderResourceView *sv = NULL;
    EXPECT_HR(MK_DSV(dev, t, &dd, &dv), S_OK);
    EXPECT_HR(MK_SRV(dev, t, &sd, &sv), S_OK);
    CHECK(dv && sv);
    SAFE_RELEASE(sv); SAFE_RELEASE(dv); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_validate_only_semantics)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    D3D11_DEPTH_STENCIL_VIEW_DESC d = dsvd(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT, 0);
    EXPECT_HR(val_dsv(dev, R(t), &d), S_FALSE);
    EXPECT_HR(val_dsv(dev, R(t), NULL), S_FALSE);
    d.Texture2D.MipSlice = 1;
    EXPECT_HR(val_dsv(dev, R(t), &d), E_INV);
    EXPECT_HR(val_dsv(dev, NULL, NULL), E_INV);
    SAFE_RELEASE(t); DONE();
}
