/* CreateRenderTargetView */
#include "views.h"
#define UI_ALL ((UINT)-1)

static int get_null_rtv(ID3D11Device *dev, void *res, D3D11_RENDER_TARGET_VIEW_DESC *out, HRESULT *hr_out)
{
    ID3D11RenderTargetView *v = NULL; HRESULT hr = MK_RTV(dev, res, NULL, &v);
    *hr_out = hr; memset(out, 0xAB, sizeof *out);
    if (FAILED(hr)) return 0;
    ID3D11RenderTargetView_GetDesc(v, out);
    SAFE_RELEASE(v);
    return 1;
}

TEST(d3d11_views_rtv_null_tex2d)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 32, 32, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    CHECK(get_null_rtv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(d.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2D);
    EXPECT_EQ(d.Texture2D.MipSlice, 0);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_null_tex2d_array)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 2, 5, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    CHECK(get_null_rtv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(d.Texture2DArray.MipSlice, 0);
    EXPECT_EQ(d.Texture2DArray.FirstArraySlice, 0);
    EXPECT_EQ(d.Texture2DArray.ArraySize, 5);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_null_cube_texture_is_array)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_TEXTURECUBE, 1);
    REQUIRE(t);
    CHECK(get_null_rtv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(d.Texture2DArray.ArraySize, 6);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_null_tex1d)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    ID3D11Texture1D *t = mk1d(dev, 32, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    REQUIRE(t);
    CHECK(get_null_rtv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE1D);
    EXPECT_EQ(d.Texture1D.MipSlice, 0);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_null_tex1d_array)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    ID3D11Texture1D *t = mk1d(dev, 32, 1, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    REQUIRE(t);
    CHECK(get_null_rtv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE1DARRAY);
    EXPECT_EQ(d.Texture1DArray.FirstArraySlice, 0);
    EXPECT_EQ(d.Texture1DArray.ArraySize, 3);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_null_tex3d)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    ID3D11Texture3D *t = mk3d(dev, 16, 16, 8, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    REQUIRE(t);
    CHECK(get_null_rtv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE3D);
    EXPECT_EQ(d.Texture3D.MipSlice, 0);
    EXPECT_EQ(d.Texture3D.FirstWSlice, 0);
    t_note("NULL-desc 3D RTV WSize = %u (depth is 8)", d.Texture3D.WSize);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_null_ms)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    if (!ms_ok(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4)) { DONE(); t_skip("4x MSAA unavailable"); return; }
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 4);
    ID3D11Texture2D *ta = mk2d(dev, 16, 16, 1, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 4);
    REQUIRE(t); REQUIRE(ta);
    CHECK(get_null_rtv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2DMS);
    CHECK(get_null_rtv(dev, ta, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY);
    EXPECT_EQ(d.Texture2DMSArray.FirstArraySlice, 0);
    EXPECT_EQ(d.Texture2DMSArray.ArraySize, 3);
    SAFE_RELEASE(ta); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_null_typeless_fails)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_B8G8R8A8_TYPELESS };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, f[i], D3D11_BIND_RENDER_TARGET, 0, 1);
        if (!t) { t_note("typeless %d not creatable as RT", f[i]); continue; }
        get_null_rtv(dev, t, &d, &hr);
        EXPECT_HR(hr, E_INV);
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_rtv_null_buffer)
{
    NEED_DEV(); D3D11_RENDER_TARGET_VIEW_DESC d; HRESULT hr;
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_RENDER_TARGET, 0, 0);
    if (!b) { DONE(); t_skip("buffer with BIND_RENDER_TARGET not creatable"); return; }
    get_null_rtv(dev, b, &d, &hr);
    EXPECT_HR(hr, E_INV);   /* a buffer has no format to infer */
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_rtv_requires_bind_render_target)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR(TRY_SRV(dev, t, NULL), S_OK);
    EXPECT_HR(try_rtv(dev, R(t), &d), E_INV);
    EXPECT_HR(try_rtv(dev, R(t), NULL), E_INV);
    EXPECT_HR(val_rtv(dev, R(t), NULL), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_depth_texture_fails)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
    EXPECT_HR(try_rtv(dev, R(t), &d), E_INV);
    EXPECT_HR(try_rtv(dev, R(t), NULL), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_dimension_mismatch_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    static const D3D11_RTV_DIMENSION bad[] = { D3D11_RTV_DIMENSION_UNKNOWN, D3D11_RTV_DIMENSION_BUFFER, D3D11_RTV_DIMENSION_TEXTURE1D,
        D3D11_RTV_DIMENSION_TEXTURE1DARRAY, D3D11_RTV_DIMENSION_TEXTURE2DMS, D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY, D3D11_RTV_DIMENSION_TEXTURE3D };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(bad[i], DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture3D.WSize = 1;
        HRESULT hr = try_rtv(dev, R(t), &d);
        t_check(hr == E_INV, __FILE__, __LINE__, "rtv dim mismatch", "dim %d got %s", bad[i], d3dh_hr_name(hr));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_mip_slice_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 32, 32, 5, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    static const struct { UINT mip; HRESULT want; } tab[] = { {0, S_OK}, {1, S_OK}, {4, S_OK}, {5, E_INVALIDARG}, {6, E_INVALIDARG}, {UI_ALL, E_INVALIDARG} };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture2D.MipSlice = tab[i].mip;
        HRESULT hr = try_rtv(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "rtv mip", "mip %u got %s want %s", tab[i].mip, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_array_range_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    static const struct { UINT mip, first, n; HRESULT want; } tab[] = {
        {0, 0, 4, S_OK}, {2, 0, 1, S_OK}, {1, 3, 1, S_OK}, {0, 1, 2, S_OK},
        {3, 0, 1, E_INVALIDARG}, {0, 0, 5, E_INVALIDARG}, {0, 3, 2, E_INVALIDARG}, {0, 4, 1, E_INVALIDARG}, {0, 0, 0, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture2DArray.MipSlice = tab[i].mip; d.Texture2DArray.FirstArraySlice = tab[i].first; d.Texture2DArray.ArraySize = tab[i].n;
        HRESULT hr = try_rtv(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "rtv array", "mip %u slice %u+%u got %s want %s", tab[i].mip, tab[i].first, tab[i].n, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_array_size_minus1)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2DArray.FirstArraySlice = 1; d.Texture2DArray.ArraySize = UI_ALL;
    ID3D11RenderTargetView *v = NULL;
    HRESULT hr = MK_RTV(dev, t, &d, &v);
    t_note("RTV ArraySize=-1 first=1 of 4 -> %s", d3dh_hr_name(hr));
    if (v) { D3D11_RENDER_TARGET_VIEW_DESC o; ID3D11RenderTargetView_GetDesc(v, &o); t_note("  GetDesc ArraySize=%u", o.Texture2DArray.ArraySize); }
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_tex3d_wslice_table)
{
    NEED_DEV();
    ID3D11Texture3D *t = mk3d(dev, 16, 16, 8, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    REQUIRE(t);
    static const struct { UINT mip, first, n; HRESULT want; } tab[] = {
        {0, 0, 8, S_OK}, {0, 7, 1, S_OK}, {0, 2, 4, S_OK}, {1, 0, 4, S_OK}, {2, 0, 2, S_OK}, {2, 1, 1, S_OK},
        {0, 0, 9, E_INVALIDARG}, {0, 8, 1, E_INVALIDARG}, {0, 7, 2, E_INVALIDARG}, {1, 0, 5, E_INVALIDARG}, {3, 0, 1, E_INVALIDARG}, {0, 0, 0, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE3D, DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture3D.MipSlice = tab[i].mip; d.Texture3D.FirstWSlice = tab[i].first; d.Texture3D.WSize = tab[i].n;
        HRESULT hr = try_rtv(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "rtv 3d", "mip %u w %u+%u got %s want %s", tab[i].mip, tab[i].first, tab[i].n, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_tex3d_wsize_minus1)
{
    NEED_DEV();
    ID3D11Texture3D *t = mk3d(dev, 16, 16, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE3D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture3D.FirstWSlice = 2; d.Texture3D.WSize = UI_ALL;
    ID3D11RenderTargetView *v = NULL;
    HRESULT hr = MK_RTV(dev, t, &d, &v);
    t_note("3D RTV WSize=-1 first=2 depth=8 -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_tex1d_ranges)
{
    NEED_DEV();
    ID3D11Texture1D *t = mk1d(dev, 32, 3, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE1DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture1DArray.MipSlice = 2; d.Texture1DArray.FirstArraySlice = 1; d.Texture1DArray.ArraySize = 3;
    EXPECT_HR(try_rtv(dev, R(t), &d), S_OK);
    d.Texture1DArray.MipSlice = 3;
    EXPECT_HR(try_rtv(dev, R(t), &d), E_INV);
    d.Texture1DArray.MipSlice = 0; d.Texture1DArray.FirstArraySlice = 2; d.Texture1DArray.ArraySize = 3;
    EXPECT_HR(try_rtv(dev, R(t), &d), E_INV);
    d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    EXPECT_HR(try_rtv(dev, R(t), &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_ms_rules)
{
    NEED_DEV();
    if (!ms_ok(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4)) { DONE(); t_skip("4x MSAA unavailable"); return; }
    ID3D11Texture2D *ms = mk2d(dev, 16, 16, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 4);
    REQUIRE(ms);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2DMSArray.FirstArraySlice = 1; d.Texture2DMSArray.ArraySize = 3;
    EXPECT_HR(try_rtv(dev, R(ms), &d), S_OK);
    d.Texture2DMSArray.ArraySize = 4;
    EXPECT_HR(try_rtv(dev, R(ms), &d), E_INV);
    d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY; d.Texture2DArray.MipSlice = 0; d.Texture2DArray.FirstArraySlice = 0; d.Texture2DArray.ArraySize = 1;
    EXPECT_HR(try_rtv(dev, R(ms), &d), E_INV);
    d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; d.Texture2D.MipSlice = 0;
    EXPECT_HR(try_rtv(dev, R(ms), &d), E_INV);
    SAFE_RELEASE(ms); DONE();
}
TEST(d3d11_views_rtv_format_cast_table)
{
    NEED_DEV();
    static const struct { DXGI_FORMAT res, view; HRESULT want; } tab[] = {
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, S_OK},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, S_OK},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT, S_OK},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SNORM, S_OK},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SINT, S_OK},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_TYPELESS, E_INVALIDARG},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, E_INVALIDARG},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM, E_INVALIDARG},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R16G16_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, S_OK},
        {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UINT, S_OK},
        {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R32_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM, S_OK},
        {DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, S_OK},
        {DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, E_INVALIDARG},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_UINT, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_SINT, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R16G16_UNORM, E_INVALIDARG},
        {DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_UNORM, S_OK},
        {DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_FLOAT, S_OK},
        {DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R32G32_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_R32G32B32A32_FLOAT, S_OK},
        {DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UNORM, S_OK},
        {DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UINT, S_OK},
        {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UNORM, S_OK},
        {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_FLOAT, S_OK},
        {DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_UNORM, S_OK},
        {DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8G8_UNORM, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, tab[i].res, D3D11_BIND_RENDER_TARGET, 0, 1);
        if (!t) { t_note("format %d not creatable as render target", tab[i].res); continue; }
        D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, tab[i].view);
        HRESULT hr = try_rtv(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "rtv cast", "res %d view %d got %s want %s", tab[i].res, tab[i].view, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_rtv_format_unknown)
{
    NEED_DEV();
    ID3D11Texture2D *typed = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R16G16_FLOAT, D3D11_BIND_RENDER_TARGET, 0, 1);
    ID3D11Texture2D *tl = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(typed); REQUIRE(tl);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_UNKNOWN), o;
    ID3D11RenderTargetView *v = NULL;
    EXPECT_HR(MK_RTV(dev, typed, &d, &v), S_OK);
    if (v) { ID3D11RenderTargetView_GetDesc(v, &o); EXPECT_EQ(o.Format, DXGI_FORMAT_R16G16_FLOAT); }
    SAFE_RELEASE(v);
    EXPECT_HR(try_rtv(dev, R(tl), &d), E_INV);
    SAFE_RELEASE(tl); SAFE_RELEASE(typed); DONE();
}
TEST(d3d11_views_rtv_unsupported_formats)
{
    NEED_DEV();
    /* Block-compressed and depth formats can never be render targets. */
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT, (DXGI_FORMAT)0x7fff };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, f[i]);
        HRESULT hr = try_rtv(dev, R(t), &d);
        t_check(hr == E_INV, __FILE__, __LINE__, "rtv bad fmt", "fmt %d got %s", f[i], d3dh_hr_name(hr));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_desc_roundtrip_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 32, 32, 4, 6, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB), o;
    d.Texture2DArray.MipSlice = 2; d.Texture2DArray.FirstArraySlice = 3; d.Texture2DArray.ArraySize = 2;
    ID3D11RenderTargetView *v = NULL;
    EXPECT_HR(MK_RTV(dev, t, &d, &v), S_OK);
    REQUIRE(v);
    memset(&o, 0, sizeof o);
    ID3D11RenderTargetView_GetDesc(v, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    EXPECT_EQ(o.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(o.Texture2DArray.MipSlice, 2);
    EXPECT_EQ(o.Texture2DArray.FirstArraySlice, 3);
    EXPECT_EQ(o.Texture2DArray.ArraySize, 2);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_buffer_view_rules)
{
    NEED_DEV();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_RENDER_TARGET, 0, 0);
    if (!b) { DONE(); t_skip("BIND_RENDER_TARGET buffer not creatable"); return; }
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_BUFFER, DXGI_FORMAT_R32_FLOAT);
    d.Buffer.FirstElement = 0; d.Buffer.NumElements = 64;
    HRESULT hr = try_rtv(dev, R(b), &d);
    t_note("buffer RTV R32_FLOAT 64 elems -> %s", d3dh_hr_name(hr));
    d.Buffer.NumElements = 65;
    EXPECT_HR(try_rtv(dev, R(b), &d), E_INV);
    d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    EXPECT_HR(try_rtv(dev, R(b), &d), E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_rtv_validate_only_semantics)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR(val_rtv(dev, R(t), &d), S_FALSE);
    EXPECT_HR(val_rtv(dev, R(t), NULL), S_FALSE);
    d.Texture2D.MipSlice = 2;
    EXPECT_HR(val_rtv(dev, R(t), &d), E_INV);
    EXPECT_HR(val_rtv(dev, NULL, NULL), E_INV);
    EXPECT_EQ(d3dh_refcount(t), 1);
    SAFE_RELEASE(t); DONE();
}
