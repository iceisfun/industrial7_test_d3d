/* CreateUnorderedAccessView */
#include "views.h"
#define UI_ALL ((UINT)-1)
#define UB D3D11_BIND_UNORDERED_ACCESS

static int get_null_uav(ID3D11Device *dev, void *res, D3D11_UNORDERED_ACCESS_VIEW_DESC *out, HRESULT *hr_out)
{
    ID3D11UnorderedAccessView *v = NULL; HRESULT hr = MK_UAV(dev, res, NULL, &v);
    *hr_out = hr; memset(out, 0xAB, sizeof *out);
    if (FAILED(hr)) return 0;
    ID3D11UnorderedAccessView_GetDesc(v, out);
    SAFE_RELEASE(v);
    return 1;
}

TEST(d3d11_views_uav_null_tex2d)
{
    NEED_DEV(); NEED_FL11(); D3D11_UNORDERED_ACCESS_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 1, DXGI_FORMAT_R32_UINT, UB, 0, 1);
    REQUIRE(t);
    CHECK(get_null_uav(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.Format, DXGI_FORMAT_R32_UINT);
    EXPECT_EQ(d.ViewDimension, D3D11_UAV_DIMENSION_TEXTURE2D);
    EXPECT_EQ(d.Texture2D.MipSlice, 0);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_null_tex2d_array)
{
    NEED_DEV(); NEED_FL11(); D3D11_UNORDERED_ACCESS_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 4, DXGI_FORMAT_R32_FLOAT, UB, 0, 1);
    REQUIRE(t);
    CHECK(get_null_uav(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_UAV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(d.Texture2DArray.MipSlice, 0);
    EXPECT_EQ(d.Texture2DArray.FirstArraySlice, 0);
    EXPECT_EQ(d.Texture2DArray.ArraySize, 4);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_null_tex1d_and_array)
{
    NEED_DEV(); NEED_FL11(); D3D11_UNORDERED_ACCESS_VIEW_DESC d; HRESULT hr;
    ID3D11Texture1D *t = mk1d(dev, 32, 1, 1, DXGI_FORMAT_R32_UINT, UB);
    ID3D11Texture1D *ta = mk1d(dev, 32, 1, 3, DXGI_FORMAT_R32_UINT, UB);
    REQUIRE(t); REQUIRE(ta);
    CHECK(get_null_uav(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_UAV_DIMENSION_TEXTURE1D);
    CHECK(get_null_uav(dev, ta, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_UAV_DIMENSION_TEXTURE1DARRAY);
    EXPECT_EQ(d.Texture1DArray.ArraySize, 3);
    SAFE_RELEASE(ta); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_null_tex3d)
{
    NEED_DEV(); NEED_FL11(); D3D11_UNORDERED_ACCESS_VIEW_DESC d; HRESULT hr;
    ID3D11Texture3D *t = mk3d(dev, 8, 8, 4, 1, DXGI_FORMAT_R32_UINT, UB);
    REQUIRE(t);
    CHECK(get_null_uav(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_UAV_DIMENSION_TEXTURE3D);
    EXPECT_EQ(d.Texture3D.MipSlice, 0);
    EXPECT_EQ(d.Texture3D.FirstWSlice, 0);
    t_note("NULL-desc 3D UAV WSize = %u (depth is 4)", d.Texture3D.WSize);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_null_typeless_fails)
{
    NEED_DEV(); NEED_FL11(); D3D11_UNORDERED_ACCESS_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R32_TYPELESS, UB, 0, 1);
    REQUIRE(t);
    get_null_uav(dev, t, &d, &hr);
    EXPECT_HR(hr, E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_requires_bind_flag)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R32_UINT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 1);
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(t); REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_UINT);
    EXPECT_HR(try_uav(dev, R(t), &d), E_INV);
    EXPECT_HR(try_uav(dev, R(t), NULL), E_INV);
    D3D11_UNORDERED_ACCESS_VIEW_DESC bd = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_UINT);
    bd.Buffer.NumElements = 64;
    EXPECT_HR(try_uav(dev, R(b), &bd), E_INV);
    EXPECT_HR(val_uav(dev, R(b), &bd), E_INV);
    SAFE_RELEASE(b); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_ms_texture_fails)
{
    NEED_DEV(); NEED_FL11();
    if (!ms_ok(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4)) { DONE(); t_skip("4x MSAA unavailable"); return; }
    /* Multisampled textures cannot carry BIND_UNORDERED_ACCESS, so creation of the texture itself is refused. */
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t = NULL; HRESULT hr;
    memset(&td, 0, sizeof td);
    td.Width = td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R32_UINT; td.SampleDesc.Count = 4;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = UB;
    hr = ID3D11Device_CreateTexture2D(dev, &td, NULL, &t);
    EXPECT_HR(hr, E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_dimension_mismatch_table)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R32_UINT, UB, 0, 1);
    REQUIRE(t);
    static const D3D11_UAV_DIMENSION bad[] = { D3D11_UAV_DIMENSION_UNKNOWN, D3D11_UAV_DIMENSION_BUFFER, D3D11_UAV_DIMENSION_TEXTURE1D,
        D3D11_UAV_DIMENSION_TEXTURE1DARRAY, D3D11_UAV_DIMENSION_TEXTURE3D, (D3D11_UAV_DIMENSION)9 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(bad[i], DXGI_FORMAT_R32_UINT);
        d.Texture3D.WSize = 1; d.Buffer.NumElements = 1;
        HRESULT hr = try_uav(dev, R(t), &d);
        t_check(hr == E_INV, __FILE__, __LINE__, "uav dim mismatch", "dim %d got %s", bad[i], d3dh_hr_name(hr));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_texture_range_tables)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 4, DXGI_FORMAT_R32_UINT, UB, 0, 1);
    REQUIRE(t);
    static const struct { UINT mip, first, n; HRESULT want; } tab[] = {
        {0, 0, 4, S_OK}, {2, 3, 1, S_OK}, {1, 1, 2, S_OK},
        {3, 0, 1, E_INVALIDARG}, {0, 0, 5, E_INVALIDARG}, {0, 4, 1, E_INVALIDARG}, {0, 3, 2, E_INVALIDARG}, {0, 0, 0, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R32_UINT);
        d.Texture2DArray.MipSlice = tab[i].mip; d.Texture2DArray.FirstArraySlice = tab[i].first; d.Texture2DArray.ArraySize = tab[i].n;
        HRESULT hr = try_uav(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "uav range", "mip %u slice %u+%u got %s want %s", tab[i].mip, tab[i].first, tab[i].n, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    D3D11_UNORDERED_ACCESS_VIEW_DESC d2 = uavd(D3D11_UAV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_UINT);
    d2.Texture2D.MipSlice = 2;
    EXPECT_HR(try_uav(dev, R(t), &d2), S_OK);
    d2.Texture2D.MipSlice = 3;
    EXPECT_HR(try_uav(dev, R(t), &d2), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_tex3d_wslice_table)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture3D *t = mk3d(dev, 16, 16, 8, 3, DXGI_FORMAT_R32_UINT, UB);
    REQUIRE(t);
    static const struct { UINT mip, first, n; HRESULT want; } tab[] = {
        {0, 0, 8, S_OK}, {0, 7, 1, S_OK}, {1, 0, 4, S_OK}, {2, 1, 1, S_OK},
        {0, 0, 9, E_INVALIDARG}, {0, 8, 1, E_INVALIDARG}, {1, 0, 5, E_INVALIDARG}, {3, 0, 1, E_INVALIDARG}, {0, 0, 0, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_TEXTURE3D, DXGI_FORMAT_R32_UINT);
        d.Texture3D.MipSlice = tab[i].mip; d.Texture3D.FirstWSlice = tab[i].first; d.Texture3D.WSize = tab[i].n;
        HRESULT hr = try_uav(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "uav 3d", "mip %u w %u+%u got %s want %s", tab[i].mip, tab[i].first, tab[i].n, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_1d_ranges)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture1D *t = mk1d(dev, 32, 3, 4, DXGI_FORMAT_R32_UINT, UB);
    REQUIRE(t);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_TEXTURE1DARRAY, DXGI_FORMAT_R32_UINT);
    d.Texture1DArray.MipSlice = 2; d.Texture1DArray.FirstArraySlice = 0; d.Texture1DArray.ArraySize = 4;
    EXPECT_HR(try_uav(dev, R(t), &d), S_OK);
    d.Texture1DArray.MipSlice = 3;
    EXPECT_HR(try_uav(dev, R(t), &d), E_INV);
    d.Texture1DArray.MipSlice = 0; d.Texture1DArray.ArraySize = 5;
    EXPECT_HR(try_uav(dev, R(t), &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_format_cast_table)
{
    NEED_DEV(); NEED_FL11();
    static const struct { DXGI_FORMAT res, view; HRESULT want; } tab[] = {
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_UINT, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_SINT, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_TYPELESS, E_INVALIDARG},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R16G16_UINT, E_INVALIDARG},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, E_INVALIDARG},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, S_OK},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT, S_OK},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, E_INVALIDARG},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R16G16_FLOAT, E_INVALIDARG},
        {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UINT, S_OK},
        {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_FLOAT, S_OK},
        {DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_FLOAT, S_OK},
        {DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, S_OK},
        {DXGI_FORMAT_R32_UINT, DXGI_FORMAT_UNKNOWN, S_OK},
        {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_UNKNOWN, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, tab[i].res, UB, 0, 1);
        if (!t) { t_note("format %d not creatable as UAV texture", tab[i].res); continue; }
        D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_TEXTURE2D, tab[i].view);
        HRESULT hr = try_uav(dev, R(t), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "uav cast", "res %d view %d got %s want %s", tab[i].res, tab[i].view, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_uav_texture_desc_roundtrip)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 5, DXGI_FORMAT_R32_TYPELESS, UB, 0, 1);
    REQUIRE(t);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R32_FLOAT), o;
    d.Texture2DArray.MipSlice = 1; d.Texture2DArray.FirstArraySlice = 2; d.Texture2DArray.ArraySize = 3;
    ID3D11UnorderedAccessView *v = NULL;
    EXPECT_HR(MK_UAV(dev, t, &d, &v), S_OK);
    REQUIRE(v);
    memset(&o, 0, sizeof o);
    ID3D11UnorderedAccessView_GetDesc(v, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_R32_FLOAT);
    EXPECT_EQ(o.ViewDimension, D3D11_UAV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(o.Texture2DArray.MipSlice, 1);
    EXPECT_EQ(o.Texture2DArray.FirstArraySlice, 2);
    EXPECT_EQ(o.Texture2DArray.ArraySize, 3);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}

/* ---- buffers ----------------------------------------------------------------- */
TEST(d3d11_views_uav_buffer_typed_ranges)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, UB, 0, 0);
    REQUIRE(b);
    static const struct { DXGI_FORMAT f; UINT first, n; HRESULT want; } tab[] = {
        {DXGI_FORMAT_R32_UINT, 0, 64, S_OK}, {DXGI_FORMAT_R32_UINT, 63, 1, S_OK}, {DXGI_FORMAT_R32_FLOAT, 16, 16, S_OK}, {DXGI_FORMAT_R32_SINT, 0, 1, S_OK},
        {DXGI_FORMAT_R32_UINT, 0, 65, E_INVALIDARG}, {DXGI_FORMAT_R32_UINT, 1, 64, E_INVALIDARG}, {DXGI_FORMAT_R32_UINT, 64, 1, E_INVALIDARG},
        {DXGI_FORMAT_R32_UINT, 0, 0, E_INVALIDARG}, {DXGI_FORMAT_R32_UINT, 0xFFFFFFFFu, 2, E_INVALIDARG},
        {DXGI_FORMAT_R32G32B32A32_UINT, 0, 16, S_OK}, {DXGI_FORMAT_R32G32B32A32_UINT, 0, 17, E_INVALIDARG},
        {DXGI_FORMAT_R16_UINT, 0, 128, S_OK}, {DXGI_FORMAT_R16_UINT, 0, 129, E_INVALIDARG},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, tab[i].f);
        d.Buffer.FirstElement = tab[i].first; d.Buffer.NumElements = tab[i].n;
        HRESULT hr = try_uav(dev, R(b), &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "uav buffer range", "fmt %d first %u n %u got %s want %s", tab[i].f, tab[i].first, tab[i].n, d3dh_hr_name(hr), d3dh_hr_name(tab[i].want));
    }
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_buffer_null_desc_plain)
{
    NEED_DEV(); NEED_FL11(); D3D11_UNORDERED_ACCESS_VIEW_DESC d; HRESULT hr;
    ID3D11Buffer *b = mkbuf(dev, 256, UB, 0, 0);
    REQUIRE(b);
    get_null_uav(dev, b, &d, &hr);
    EXPECT_HR(hr, E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_buffer_format_rules)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, UB, 0, 0);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
    d.Buffer.NumElements = 4;
    EXPECT_HR(try_uav(dev, R(b), &d), E_INV);
    d.Format = DXGI_FORMAT_R32_TYPELESS;
    EXPECT_HR(try_uav(dev, R(b), &d), E_INV);
    d.Format = DXGI_FORMAT_D32_FLOAT;
    EXPECT_HR(try_uav(dev, R(b), &d), E_INV);
    d.Format = DXGI_FORMAT_R32_UINT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    EXPECT_HR(try_uav(dev, R(b), &d), E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_raw_buffer_rules)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *raw = mkbuf(dev, 256, UB, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    ID3D11Buffer *plain = mkbuf(dev, 256, UB, 0, 0);
    REQUIRE(raw); REQUIRE(plain);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_TYPELESS), o;
    d.Buffer.FirstElement = 0; d.Buffer.NumElements = 64; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    ID3D11UnorderedAccessView *v = NULL;
    EXPECT_HR(MK_UAV(dev, raw, &d, &v), S_OK);
    if (v) { memset(&o, 0, sizeof o); ID3D11UnorderedAccessView_GetDesc(v, &o); EXPECT_EQ(o.Format, DXGI_FORMAT_R32_TYPELESS); EXPECT_EQ(o.Buffer.Flags, D3D11_BUFFER_UAV_FLAG_RAW); EXPECT_EQ(o.Buffer.NumElements, 64); }
    SAFE_RELEASE(v);
    EXPECT_HR(try_uav(dev, R(plain), &d), E_INV);
    d.Buffer.NumElements = 65;
    EXPECT_HR(try_uav(dev, R(raw), &d), E_INV);
    d.Buffer.NumElements = 64; d.Format = DXGI_FORMAT_R32_UINT;
    EXPECT_HR(try_uav(dev, R(raw), &d), E_INV);
    d.Format = DXGI_FORMAT_UNKNOWN;
    EXPECT_HR(try_uav(dev, R(raw), &d), E_INV);
    d.Format = DXGI_FORMAT_R32_TYPELESS; d.Buffer.Flags = 0;
    EXPECT_HR(try_uav(dev, R(raw), &d), E_INV);
    SAFE_RELEASE(plain); SAFE_RELEASE(raw); DONE();
}
TEST(d3d11_views_uav_raw_flag_on_structured_buffer_fails)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, UB, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_TYPELESS);
    d.Buffer.NumElements = 16; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    EXPECT_HR(try_uav(dev, R(b), &d), E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_structured_buffer_plain)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 16 * 20, UB, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 20);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
    d.Buffer.NumElements = 16;
    EXPECT_HR(try_uav(dev, R(b), &d), S_OK);
    d.Buffer.NumElements = 17;
    EXPECT_HR(try_uav(dev, R(b), &d), E_INV);
    d.Buffer.FirstElement = 15; d.Buffer.NumElements = 1;
    EXPECT_HR(try_uav(dev, R(b), &d), S_OK);
    d.Buffer.FirstElement = 0; d.Buffer.NumElements = 16; d.Format = DXGI_FORMAT_R32_UINT;
    EXPECT_HR(try_uav(dev, R(b), &d), E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_append_counter_flags)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *st = mkbuf(dev, 16 * 16, UB, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16);
    ID3D11Buffer *plain = mkbuf(dev, 256, UB, 0, 0);
    ID3D11Buffer *raw = mkbuf(dev, 256, UB, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    REQUIRE(st); REQUIRE(plain); REQUIRE(raw);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN), o;
    d.Buffer.NumElements = 16;
    ID3D11UnorderedAccessView *v = NULL;
    d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
    EXPECT_HR(MK_UAV(dev, st, &d, &v), S_OK);
    if (v) { memset(&o, 0, sizeof o); ID3D11UnorderedAccessView_GetDesc(v, &o); EXPECT_EQ(o.Buffer.Flags, D3D11_BUFFER_UAV_FLAG_APPEND); EXPECT_EQ(o.Format, DXGI_FORMAT_UNKNOWN); }
    SAFE_RELEASE(v);
    d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    EXPECT_HR(MK_UAV(dev, st, &d, &v), S_OK);
    if (v) { memset(&o, 0, sizeof o); ID3D11UnorderedAccessView_GetDesc(v, &o); EXPECT_EQ(o.Buffer.Flags, D3D11_BUFFER_UAV_FLAG_COUNTER); }
    SAFE_RELEASE(v);
    d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER;
    EXPECT_HR(try_uav(dev, R(st), &d), E_INV);
    d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
    EXPECT_HR(try_uav(dev, R(plain), &d), E_INV);
    d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    EXPECT_HR(try_uav(dev, R(plain), &d), E_INV);
    d.Format = DXGI_FORMAT_R32_UINT; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    EXPECT_HR(try_uav(dev, R(plain), &d), E_INV);
    d.Format = DXGI_FORMAT_R32_TYPELESS; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW | D3D11_BUFFER_UAV_FLAG_APPEND;
    EXPECT_HR(try_uav(dev, R(raw), &d), E_INV);
    d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW | D3D11_BUFFER_UAV_FLAG_COUNTER;
    EXPECT_HR(try_uav(dev, R(raw), &d), E_INV);
    d.Format = DXGI_FORMAT_UNKNOWN; d.Buffer.Flags = 0x10;
    EXPECT_HR(try_uav(dev, R(st), &d), E_INV);      /* undefined flag bit */
    SAFE_RELEASE(raw); SAFE_RELEASE(plain); SAFE_RELEASE(st); DONE();
}
TEST(d3d11_views_uav_append_with_offset_range)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *st = mkbuf(dev, 16 * 16, UB, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16);
    REQUIRE(st);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
    d.Buffer.FirstElement = 4; d.Buffer.NumElements = 8; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    EXPECT_HR(try_uav(dev, R(st), &d), S_OK);
    d.Buffer.FirstElement = 12; d.Buffer.NumElements = 5;
    EXPECT_HR(try_uav(dev, R(st), &d), E_INV);
    SAFE_RELEASE(st); DONE();
}
TEST(d3d11_views_uav_counter_on_texture_fails)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R32_UINT, UB, 0, 1);
    REQUIRE(t);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
    d.Buffer.NumElements = 4; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    EXPECT_HR(try_uav(dev, R(t), &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_buffer_desc_roundtrip)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, UB, 0, 0);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_FLOAT), o;
    d.Buffer.FirstElement = 10; d.Buffer.NumElements = 20;
    ID3D11UnorderedAccessView *v = NULL;
    EXPECT_HR(MK_UAV(dev, b, &d, &v), S_OK);
    REQUIRE(v);
    memset(&o, 0, sizeof o);
    ID3D11UnorderedAccessView_GetDesc(v, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_R32_FLOAT);
    EXPECT_EQ(o.ViewDimension, D3D11_UAV_DIMENSION_BUFFER);
    EXPECT_EQ(o.Buffer.FirstElement, 10);
    EXPECT_EQ(o.Buffer.NumElements, 20);
    EXPECT_EQ(o.Buffer.Flags, 0);
    SAFE_RELEASE(v); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_validate_only_semantics)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, UB, 0, 0);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_UINT);
    d.Buffer.NumElements = 64;
    EXPECT_HR(val_uav(dev, R(b), &d), S_FALSE);
    d.Buffer.NumElements = 65;
    EXPECT_HR(val_uav(dev, R(b), &d), E_INV);
    EXPECT_HR(val_uav(dev, NULL, &d), E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_buffer_bind_with_srv_both)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, UB | D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC u = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_TYPELESS);
    u.Buffer.NumElements = 64; u.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    D3D11_SHADER_RESOURCE_VIEW_DESC s = srvd(D3D11_SRV_DIMENSION_BUFFEREX, DXGI_FORMAT_R32_TYPELESS);
    s.BufferEx.NumElements = 64; s.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    ID3D11UnorderedAccessView *uv = NULL; ID3D11ShaderResourceView *sv = NULL;
    EXPECT_HR(MK_UAV(dev, b, &u, &uv), S_OK);
    EXPECT_HR(MK_SRV(dev, b, &s, &sv), S_OK);
    CHECK(uv && sv);
    SAFE_RELEASE(sv); SAFE_RELEASE(uv); SAFE_RELEASE(b); DONE();
}
