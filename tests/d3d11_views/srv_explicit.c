/* CreateShaderResourceView with explicit descriptors: dimensions, ranges, format casts, cube, buffers. */
#include "views.h"

#define UI_ALL ((UINT)-1)

/* ---- dimension vs resource ------------------------------------------------ */
TEST(d3d11_views_srv_dim_tex2d_on_tex2d_ok)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2D.MostDetailedMip = 0; d.Texture2D.MipLevels = 4;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_dim_mismatch_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    static const D3D11_SRV_DIMENSION bad[] = { D3D11_SRV_DIMENSION_BUFFER, D3D11_SRV_DIMENSION_TEXTURE1D, D3D11_SRV_DIMENSION_TEXTURE1DARRAY,
        D3D11_SRV_DIMENSION_TEXTURE3D, D3D11_SRV_DIMENSION_TEXTURE2DMS, D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY, D3D11_SRV_DIMENSION_TEXTURECUBE,
        D3D11_SRV_DIMENSION_TEXTURECUBEARRAY, D3D11_SRV_DIMENSION_BUFFEREX };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(bad[i], DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture1D.MipLevels = 1; /* union: harmless for others, makes ranges non-zero */
        HRESULT hr = TRY_SRV(dev, t, &d);
        t_check(hr == E_INV, __FILE__, __LINE__, "dimension mismatch", "dim %d got %s", bad[i], d3dh_hr_name(hr));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_dim_unknown_fails)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    ID3D11Buffer *b = mkbuf(dev, 64, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(t); REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    d.ViewDimension = (D3D11_SRV_DIMENSION)99;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(b); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_dim_1d_array_on_array)
{
    NEED_DEV();
    ID3D11Texture1D *t = mk1d(dev, 16, 3, 4, DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE1DARRAY, DXGI_FORMAT_R8_UNORM);
    d.Texture1DArray.MipLevels = 3; d.Texture1DArray.FirstArraySlice = 1; d.Texture1DArray.ArraySize = 3;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.Texture1DArray.FirstArraySlice = 2; d.Texture1DArray.ArraySize = 3;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.Texture1DArray.FirstArraySlice = 4; d.Texture1DArray.ArraySize = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_dim_tex3d)
{
    NEED_DEV();
    ID3D11Texture3D *t = mk3d(dev, 16, 16, 16, 5, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE3D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture3D.MostDetailedMip = 1; d.Texture3D.MipLevels = 4;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.Texture3D.MostDetailedMip = 2; d.Texture3D.MipLevels = 4;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY; d.Texture2DArray.MostDetailedMip = 0; d.Texture2DArray.MipLevels = 1; d.Texture2DArray.ArraySize = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}

/* ---- mip / slice ranges ---------------------------------------------------- */
TEST(d3d11_views_srv_mip_range_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 32, 32, 5, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    static const struct { UINT most, count; HRESULT want; } tab[] = {
        {0, 5, S_OK}, {0, 1, S_OK}, {4, 1, S_OK}, {2, 3, S_OK}, {1, 4, S_OK}, {0, UI_ALL, S_OK}, {3, UI_ALL, S_OK}, {4, UI_ALL, S_OK},
        {0, 6, E_INVALIDARG}, {1, 5, E_INVALIDARG}, {4, 2, E_INVALIDARG}, {5, 1, E_INVALIDARG}, {5, UI_ALL, E_INVALIDARG},
        {100, 1, E_INVALIDARG}, {0, 0, E_INVALIDARG}, {3, 0, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture2D.MostDetailedMip = tab[i].most; d.Texture2D.MipLevels = tab[i].count;
        HRESULT hr = TRY_SRV(dev, t, &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "srv mip range", "most=%u count=%u got %s want %s", tab[i].most, tab[i].count, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_mip_minus1_getdesc)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 32, 32, 5, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM), o;
    d.Texture2D.MostDetailedMip = 1; d.Texture2D.MipLevels = UI_ALL;
    ID3D11ShaderResourceView *v = NULL;
    EXPECT_HR(MK_SRV(dev, t, &d, &v), S_OK);
    REQUIRE(v);
    ID3D11ShaderResourceView_GetDesc(v, &o);
    EXPECT_EQ(o.Texture2D.MostDetailedMip, 1);
    t_note("MipLevels=-1 reads back from GetDesc as %u", o.Texture2D.MipLevels);
    EXPECT_EQ(o.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2D);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_array_range_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    static const struct { UINT mip, nmip, first, n; HRESULT want; } tab[] = {
        {0, 3, 0, 6, S_OK}, {0, 1, 0, 1, S_OK}, {0, 3, 5, 1, S_OK}, {1, 2, 2, 3, S_OK}, {2, 1, 3, 3, S_OK},
        {0, 3, 0, 7, E_INVALIDARG}, {0, 3, 5, 2, E_INVALIDARG}, {0, 3, 6, 1, E_INVALIDARG}, {0, 3, 0, 0, E_INVALIDARG},
        {3, 1, 0, 1, E_INVALIDARG}, {0, 4, 0, 1, E_INVALIDARG}, {0, 3, 100, 1, E_INVALIDARG}, {0, 0, 0, 1, E_INVALIDARG},
        {0, UI_ALL, 0, 6, S_OK},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture2DArray.MostDetailedMip = tab[i].mip; d.Texture2DArray.MipLevels = tab[i].nmip;
        d.Texture2DArray.FirstArraySlice = tab[i].first; d.Texture2DArray.ArraySize = tab[i].n;
        HRESULT hr = TRY_SRV(dev, t, &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "srv array range", "mip=%u/%u slice=%u+%u got %s want %s", tab[i].mip, tab[i].nmip, tab[i].first, tab[i].n, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_array_size_minus1)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2DArray.MipLevels = 1; d.Texture2DArray.FirstArraySlice = 2; d.Texture2DArray.ArraySize = UI_ALL;
    HRESULT hr = TRY_SRV(dev, t, &d);
    t_note("ArraySize=-1 (first slice 2 of 6) -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_array_view_on_single_texture)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2DArray.MipLevels = 1; d.Texture2DArray.ArraySize = 1;
    HRESULT hr = TRY_SRV(dev, t, &d);
    t_note("Texture2DArray view of non-array 2D texture -> %s", d3dh_hr_name(hr));
    d.Texture2DArray.ArraySize = 2;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_tex2d_view_on_array_texture)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2D.MipLevels = 1;
    HRESULT hr = TRY_SRV(dev, t, &d);
    t_note("Texture2D view of a 4-slice array texture -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t); DONE();
}

/* ---- cube views --------------------------------------------------------------- */
TEST(d3d11_views_srv_cube_explicit)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 5, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_TEXTURECUBE, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURECUBE, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.TextureCube.MostDetailedMip = 0; d.TextureCube.MipLevels = 5;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.TextureCube.MostDetailedMip = 4; d.TextureCube.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.TextureCube.MostDetailedMip = 4; d.TextureCube.MipLevels = 2;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.TextureCube.MostDetailedMip = 0; d.TextureCube.MipLevels = UI_ALL;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_cube_view_requires_cube_flag)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURECUBE, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.TextureCube.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_cube_texture_as_2d_array)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_TEXTURECUBE, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2DArray.MipLevels = 1; d.Texture2DArray.FirstArraySlice = 1; d.Texture2DArray.ArraySize = 5;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.Texture2DArray.FirstArraySlice = 0; d.Texture2DArray.ArraySize = 6;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.Texture2DArray.FirstArraySlice = 1; d.Texture2DArray.ArraySize = 6;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_cube_array_ranges)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 2, 18, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_TEXTURECUBE, 1);
    if (!t) { DONE(); t_skip("cube array texture not creatable"); return; }
    static const struct { UINT mip, nmip, face, cubes; HRESULT want; } tab[] = {
        {0, 2, 0, 3, S_OK}, {0, 2, 0, 1, S_OK}, {1, 1, 6, 2, S_OK}, {0, 2, 12, 1, S_OK},
        {0, 2, 0, 4, E_INVALIDARG}, {0, 2, 12, 2, E_INVALIDARG}, {0, 2, 18, 1, E_INVALIDARG}, {0, 2, 0, 0, E_INVALIDARG},
        {2, 1, 0, 1, E_INVALIDARG}, {0, 3, 0, 1, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURECUBEARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
        d.TextureCubeArray.MostDetailedMip = tab[i].mip; d.TextureCubeArray.MipLevels = tab[i].nmip;
        d.TextureCubeArray.First2DArrayFace = tab[i].face; d.TextureCubeArray.NumCubes = tab[i].cubes;
        HRESULT hr = TRY_SRV(dev, t, &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "cube array range", "mip=%u/%u face=%u cubes=%u got %s want %s", tab[i].mip, tab[i].nmip, tab[i].face, tab[i].cubes, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_cube_array_view_on_plain_cube)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_TEXTURECUBE, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURECUBEARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.TextureCubeArray.MipLevels = 1; d.TextureCubeArray.NumCubes = 1;
    HRESULT hr = TRY_SRV(dev, t, &d);
    t_note("TextureCubeArray view (1 cube) of a plain cube -> %s", d3dh_hr_name(hr));
    d.TextureCubeArray.NumCubes = 2;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}

/* ---- multisample ------------------------------------------------------------------ */
TEST(d3d11_views_srv_ms_dimension_rules)
{
    NEED_DEV();
    if (!ms_ok(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4)) { DONE(); t_skip("4x MSAA unavailable"); return; }
    ID3D11Texture2D *ms = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 4);
    ID3D11Texture2D *ss = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(ms); REQUIRE(ss);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2DMS, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR(TRY_SRV(dev, ms, &d), S_OK);
    EXPECT_HR(TRY_SRV(dev, ss, &d), E_INV);
    d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, ms, &d), E_INV);
    d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY; d.Texture2DArray.MipLevels = 1; d.Texture2DArray.ArraySize = 1;
    EXPECT_HR(TRY_SRV(dev, ms, &d), E_INV);
    SAFE_RELEASE(ss); SAFE_RELEASE(ms); DONE();
}
TEST(d3d11_views_srv_ms_array_ranges)
{
    NEED_DEV();
    if (!ms_ok(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4)) { DONE(); t_skip("4x MSAA unavailable"); return; }
    ID3D11Texture2D *ms = mk2d(dev, 16, 16, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 4);
    REQUIRE(ms);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2DMSArray.FirstArraySlice = 1; d.Texture2DMSArray.ArraySize = 3;
    EXPECT_HR(TRY_SRV(dev, ms, &d), S_OK);
    d.Texture2DMSArray.FirstArraySlice = 2; d.Texture2DMSArray.ArraySize = 3;
    EXPECT_HR(TRY_SRV(dev, ms, &d), E_INV);
    d.Texture2DMSArray.FirstArraySlice = 0; d.Texture2DMSArray.ArraySize = 0;
    EXPECT_HR(TRY_SRV(dev, ms, &d), E_INV);
    SAFE_RELEASE(ms); DONE();
}

/* ---- format casting -------------------------------------------------------------------- */
static const struct { DXGI_FORMAT typeless; DXGI_FORMAT view; HRESULT want; } cast_tab[] = {
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, S_OK},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, S_OK},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT, S_OK},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SNORM, S_OK},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SINT, S_OK},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_TYPELESS, E_INVALIDARG},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_FLOAT, E_INVALIDARG},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, E_INVALIDARG},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM, E_INVALIDARG},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R16G16_FLOAT, E_INVALIDARG},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8_UNORM, E_INVALIDARG},
    {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R16G16B16A16_FLOAT, E_INVALIDARG},
    {DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM, S_OK},
    {DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, S_OK},
    {DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, E_INVALIDARG},
    {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, S_OK},
    {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_UINT, S_OK},
    {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_SINT, S_OK},
    {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, E_INVALIDARG},
    {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R16G16_FLOAT, E_INVALIDARG},
    {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UNORM, S_OK},
    {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_FLOAT, S_OK},
    {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UINT, S_OK},
    {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_SNORM, S_OK},
    {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_SINT, S_OK},
    {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R8G8_UNORM, E_INVALIDARG},
    {DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_FLOAT, S_OK},
    {DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_UNORM, S_OK},
    {DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R32_FLOAT, E_INVALIDARG},
    {DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_FLOAT, S_OK},
    {DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_UINT, S_OK},
    {DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R32G32_FLOAT, E_INVALIDARG},
    {DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_R32G32B32A32_FLOAT, S_OK},
    {DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_R32G32B32A32_UINT, S_OK},
    {DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_R32G32B32_FLOAT, E_INVALIDARG},
    {DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UNORM, S_OK},
    {DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UINT, S_OK},
    {DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, E_INVALIDARG},
    {DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_UNORM, S_OK},
    {DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_UINT, S_OK},
    {DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_A8_UNORM, E_INVALIDARG},
    {DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_UNORM, S_OK},
    {DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R16_UNORM, E_INVALIDARG},
};
TEST(d3d11_views_srv_format_cast_typeless_table)
{
    NEED_DEV();
    for (unsigned i = 0; i < sizeof cast_tab / sizeof *cast_tab; i++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, cast_tab[i].typeless, D3D11_BIND_SHADER_RESOURCE, 0, 1);
        if (!t) { t_note("typeless %d not creatable as SRV texture", cast_tab[i].typeless); continue; }
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, cast_tab[i].view);
        d.Texture2D.MipLevels = 1;
        HRESULT hr = TRY_SRV(dev, t, &d);
        t_check(hr == cast_tab[i].want, __FILE__, __LINE__, "srv cast", "typeless %d as %d got %s want %s", cast_tab[i].typeless, cast_tab[i].view, d3dh_hr_name(hr), d3dh_hr_name(cast_tab[i].want));
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_srv_format_cast_typed_resource_same_family)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    d.Texture2D.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.Format = DXGI_FORMAT_R8G8B8A8_UINT;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.Format = DXGI_FORMAT_R32_FLOAT;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_format_unknown_inherits_typed)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R16G16_FLOAT, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_UNKNOWN), o;
    d.Texture2D.MipLevels = 1;
    ID3D11ShaderResourceView *v = NULL;
    HRESULT hr = MK_SRV(dev, t, &d, &v);
    EXPECT_HR(hr, S_OK);
    if (v) { ID3D11ShaderResourceView_GetDesc(v, &o); EXPECT_EQ(o.Format, DXGI_FORMAT_R16G16_FLOAT); }
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_format_unknown_typeless_fails)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_UNKNOWN);
    d.Texture2D.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_format_invalid_enum)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, (DXGI_FORMAT)0x7fff);
    d.Texture2D.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.Format = (DXGI_FORMAT)200;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_depth_typeless_formats)
{
    NEED_DEV();
    static const struct { DXGI_FORMAT res, view; HRESULT want; } tab[] = {
        {DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, S_OK},
        {DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_X24_TYPELESS_G8_UINT, S_OK},
        {DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, E_INVALIDARG},
        {DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_R32_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UNORM, S_OK},
        {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_D16_UNORM, E_INVALIDARG},
        {DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, S_OK},
        {DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_X32_TYPELESS_G8X24_UINT, S_OK},
        {DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, tab[i].res, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL, 0, 1);
        if (!t) { t_note("depth typeless %d not creatable", tab[i].res); continue; }
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, tab[i].view);
        d.Texture2D.MipLevels = 1;
        HRESULT hr = TRY_SRV(dev, t, &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "depth srv", "res %d view %d got %s want %s", tab[i].res, tab[i].view, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
        SAFE_RELEASE(t);
    }
    DONE();
}

/* ---- bind flag prerequisites ----------------------------------------------------------- */
TEST(d3d11_views_srv_requires_bind_shader_resource)
{
    NEED_DEV();
    static const UINT binds[] = { D3D11_BIND_RENDER_TARGET, D3D11_BIND_UNORDERED_ACCESS, D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS };
    for (unsigned i = 0; i < 3; i++) {
        if (binds[i] & D3D11_BIND_UNORDERED_ACCESS && fl < D3D_FEATURE_LEVEL_11_0) continue;
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, binds[i], 0, 1);
        if (!t) continue;
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture2D.MipLevels = 1;
        EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
        EXPECT_HR(val_srv(dev, R(t), &d), E_INV);
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_srv_staging_texture_fails)
{
    NEED_DEV();
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t = NULL;
    memset(&td, 0, sizeof td);
    td.Width = td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    REQUIRE(SUCCEEDED(ID3D11Device_CreateTexture2D(dev, &td, NULL, &t)));
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2D.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    EXPECT_HR(TRY_SRV(dev, t, NULL), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_dynamic_texture_ok)
{
    NEED_DEV();
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t = NULL;
    memset(&td, 0, sizeof td);
    td.Width = td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    REQUIRE(SUCCEEDED(ID3D11Device_CreateTexture2D(dev, &td, NULL, &t)));
    EXPECT_HR(TRY_SRV(dev, t, NULL), S_OK);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_buffer_requires_bind_flag)
{
    NEED_DEV();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_VERTEX_BUFFER, 0, 0);
    REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFER, DXGI_FORMAT_R32_FLOAT);
    d.Buffer.FirstElement = 0; d.Buffer.NumElements = 64;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    SAFE_RELEASE(b); DONE();
}

/* ---- buffer SRVs ------------------------------------------------------------------------ */
TEST(d3d11_views_srv_buffer_typed_ranges)
{
    NEED_DEV();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(b);
    static const struct { DXGI_FORMAT f; UINT first, n; HRESULT want; } tab[] = {
        {DXGI_FORMAT_R32_FLOAT, 0, 64, S_OK}, {DXGI_FORMAT_R32_FLOAT, 0, 1, S_OK}, {DXGI_FORMAT_R32_FLOAT, 63, 1, S_OK}, {DXGI_FORMAT_R32_FLOAT, 32, 32, S_OK},
        {DXGI_FORMAT_R32_FLOAT, 0, 65, E_INVALIDARG}, {DXGI_FORMAT_R32_FLOAT, 1, 64, E_INVALIDARG}, {DXGI_FORMAT_R32_FLOAT, 64, 1, E_INVALIDARG},
        {DXGI_FORMAT_R32_FLOAT, 0, 0, E_INVALIDARG}, {DXGI_FORMAT_R32_FLOAT, 1000, 1, E_INVALIDARG},
        {DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, S_OK}, {DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 17, E_INVALIDARG}, {DXGI_FORMAT_R32G32B32A32_FLOAT, 15, 1, S_OK},
        {DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 1, E_INVALIDARG},
        {DXGI_FORMAT_R16_FLOAT, 0, 128, S_OK}, {DXGI_FORMAT_R16_FLOAT, 0, 129, E_INVALIDARG},
        {DXGI_FORMAT_R8G8B8A8_UNORM, 0, 64, S_OK}, {DXGI_FORMAT_R8_UNORM, 0, 256, S_OK}, {DXGI_FORMAT_R8_UNORM, 0, 257, E_INVALIDARG},
        {DXGI_FORMAT_R32G32_FLOAT, 0, 32, S_OK}, {DXGI_FORMAT_R32G32_FLOAT, 0, 33, E_INVALIDARG},
        {DXGI_FORMAT_R32G32B32_FLOAT, 0, 21, S_OK}, {DXGI_FORMAT_R32G32B32_FLOAT, 0, 22, E_INVALIDARG},
        {DXGI_FORMAT_R32_FLOAT, 0xFFFFFFFFu, 2, E_INVALIDARG}, {DXGI_FORMAT_R32_FLOAT, 2, 0xFFFFFFFFu, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFER, tab[i].f);
        d.Buffer.FirstElement = tab[i].first; d.Buffer.NumElements = tab[i].n;
        HRESULT hr = TRY_SRV(dev, b, &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "buffer srv range", "fmt=%d first=%u n=%u got %s want %s", tab[i].f, tab[i].first, tab[i].n, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_buffer_format_rules)
{
    NEED_DEV();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
    d.Buffer.NumElements = 4;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);          /* unknown format needs a structured buffer */
    d.Format = DXGI_FORMAT_R32_TYPELESS;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);          /* typeless needs RAW */
    d.Format = DXGI_FORMAT_D32_FLOAT;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    d.Format = DXGI_FORMAT_BC1_UNORM;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    d.Format = DXGI_FORMAT_R32_FLOAT; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_bufferex_flags_basic)
{
    NEED_DEV();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFEREX, DXGI_FORMAT_R32_FLOAT);
    d.BufferEx.FirstElement = 0; d.BufferEx.NumElements = 64; d.BufferEx.Flags = 0;
    EXPECT_HR(TRY_SRV(dev, b, &d), S_OK);
    d.BufferEx.NumElements = 65;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    d.BufferEx.NumElements = 64; d.BufferEx.Flags = 0x2;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);          /* undefined flag bit */
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_raw_buffer_rules)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *raw = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    ID3D11Buffer *plain = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(raw); REQUIRE(plain);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFEREX, DXGI_FORMAT_R32_TYPELESS);
    d.BufferEx.FirstElement = 0; d.BufferEx.NumElements = 64; d.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    EXPECT_HR(TRY_SRV(dev, raw, &d), S_OK);
    EXPECT_HR(TRY_SRV(dev, plain, &d), E_INV);      /* buffer lacks ALLOW_RAW_VIEWS */
    d.BufferEx.NumElements = 65;
    EXPECT_HR(TRY_SRV(dev, raw, &d), E_INV);
    d.BufferEx.NumElements = 64; d.BufferEx.FirstElement = 1;
    EXPECT_HR(TRY_SRV(dev, raw, &d), E_INV);
    d.BufferEx.FirstElement = 63; d.BufferEx.NumElements = 1;
    EXPECT_HR(TRY_SRV(dev, raw, &d), S_OK);
    d.BufferEx.FirstElement = 0; d.BufferEx.NumElements = 64; d.Format = DXGI_FORMAT_R32_FLOAT;
    EXPECT_HR(TRY_SRV(dev, raw, &d), E_INV);        /* RAW requires R32_TYPELESS */
    d.Format = DXGI_FORMAT_UNKNOWN;
    EXPECT_HR(TRY_SRV(dev, raw, &d), E_INV);
    d.Format = DXGI_FORMAT_R16_TYPELESS;
    EXPECT_HR(TRY_SRV(dev, raw, &d), E_INV);
    d.Format = DXGI_FORMAT_R32_TYPELESS; d.BufferEx.Flags = 0;
    EXPECT_HR(TRY_SRV(dev, raw, &d), E_INV);        /* typeless without RAW flag */
    SAFE_RELEASE(plain); SAFE_RELEASE(raw); DONE();
}
TEST(d3d11_views_srv_raw_buffer_as_typed_view_ok)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *raw = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    REQUIRE(raw);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFER, DXGI_FORMAT_R32_FLOAT);
    d.Buffer.NumElements = 64;
    HRESULT hr = TRY_SRV(dev, raw, &d);
    t_note("typed SRV on ALLOW_RAW_VIEWS buffer -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(raw); DONE();
}
TEST(d3d11_views_srv_raw_flag_on_structured_buffer_fails)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16);
    REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFEREX, DXGI_FORMAT_R32_TYPELESS);
    d.BufferEx.NumElements = 16; d.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_structured_buffer_rules)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 16 * 20, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 20);
    REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
    d.Buffer.FirstElement = 0; d.Buffer.NumElements = 16;
    EXPECT_HR(TRY_SRV(dev, b, &d), S_OK);
    d.Buffer.NumElements = 17;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    d.Buffer.FirstElement = 15; d.Buffer.NumElements = 1;
    EXPECT_HR(TRY_SRV(dev, b, &d), S_OK);
    d.Buffer.FirstElement = 16; d.Buffer.NumElements = 1;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);
    d.Buffer.FirstElement = 0; d.Buffer.NumElements = 16; d.Format = DXGI_FORMAT_R32_FLOAT;
    EXPECT_HR(TRY_SRV(dev, b, &d), E_INV);          /* structured buffers need format UNKNOWN */
    d.Format = DXGI_FORMAT_UNKNOWN; d.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX; d.BufferEx.Flags = 0; d.BufferEx.NumElements = 16;
    EXPECT_HR(TRY_SRV(dev, b, &d), S_OK);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_buffer_desc_roundtrip)
{
    NEED_DEV();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFER, DXGI_FORMAT_R32_FLOAT), o;
    d.Buffer.FirstElement = 8; d.Buffer.NumElements = 16;
    ID3D11ShaderResourceView *v = NULL;
    EXPECT_HR(MK_SRV(dev, b, &d, &v), S_OK);
    REQUIRE(v);
    memset(&o, 0, sizeof o);
    ID3D11ShaderResourceView_GetDesc(v, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_R32_FLOAT);
    EXPECT_EQ(o.ViewDimension, D3D11_SRV_DIMENSION_BUFFER);
    EXPECT_EQ(o.Buffer.FirstElement, 8);
    EXPECT_EQ(o.Buffer.NumElements, 16);
    SAFE_RELEASE(v); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_bufferex_desc_roundtrip)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_BUFFEREX, DXGI_FORMAT_R32_TYPELESS), o;
    d.BufferEx.FirstElement = 4; d.BufferEx.NumElements = 20; d.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    ID3D11ShaderResourceView *v = NULL;
    EXPECT_HR(MK_SRV(dev, b, &d, &v), S_OK);
    REQUIRE(v);
    memset(&o, 0, sizeof o);
    ID3D11ShaderResourceView_GetDesc(v, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_R32_TYPELESS);
    EXPECT_EQ(o.ViewDimension, D3D11_SRV_DIMENSION_BUFFEREX);
    EXPECT_EQ(o.BufferEx.FirstElement, 4);
    EXPECT_EQ(o.BufferEx.NumElements, 20);
    EXPECT_EQ(o.BufferEx.Flags, D3D11_BUFFEREX_SRV_FLAG_RAW);
    SAFE_RELEASE(v); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_texture_desc_roundtrips)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 32, 32, 6, 4, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB), o;
    d.Texture2DArray.MostDetailedMip = 1; d.Texture2DArray.MipLevels = 3; d.Texture2DArray.FirstArraySlice = 1; d.Texture2DArray.ArraySize = 2;
    ID3D11ShaderResourceView *v = NULL;
    EXPECT_HR(MK_SRV(dev, t, &d, &v), S_OK);
    REQUIRE(v);
    memset(&o, 0, sizeof o);
    ID3D11ShaderResourceView_GetDesc(v, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    EXPECT_EQ(o.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(o.Texture2DArray.MostDetailedMip, 1);
    EXPECT_EQ(o.Texture2DArray.MipLevels, 3);
    EXPECT_EQ(o.Texture2DArray.FirstArraySlice, 1);
    EXPECT_EQ(o.Texture2DArray.ArraySize, 2);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_depth_view_getdesc)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R24G8_TYPELESS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_X24_TYPELESS_G8_UINT), o;
    d.Texture2D.MipLevels = 1;
    ID3D11ShaderResourceView *v = NULL;
    EXPECT_HR(MK_SRV(dev, t, &d, &v), S_OK);
    REQUIRE(v);
    ID3D11ShaderResourceView_GetDesc(v, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_X24_TYPELESS_G8_UINT);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_1d_ranges)
{
    NEED_DEV();
    ID3D11Texture1D *t = mk1d(dev, 32, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE1D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture1D.MostDetailedMip = 3; d.Texture1D.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    d.Texture1D.MostDetailedMip = 3; d.Texture1D.MipLevels = 2;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.Texture1D.MostDetailedMip = 4; d.Texture1D.MipLevels = 1;
    EXPECT_HR(TRY_SRV(dev, t, &d), E_INV);
    d.Texture1D.MostDetailedMip = 0; d.Texture1D.MipLevels = UI_ALL;
    EXPECT_HR(TRY_SRV(dev, t, &d), S_OK);
    SAFE_RELEASE(t); DONE();
}
