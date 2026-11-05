/* CreateShaderResourceView with pDesc == NULL: dimension/format/ranges inferred from the resource. */
#include "views.h"

static int get_null_srv(ID3D11Device *dev, void *res, D3D11_SHADER_RESOURCE_VIEW_DESC *out, HRESULT *hr_out)
{
    ID3D11ShaderResourceView *v = NULL; HRESULT hr = MK_SRV(dev, res, NULL, &v);
    if (hr_out) *hr_out = hr;
    memset(out, 0xAB, sizeof *out);
    if (FAILED(hr)) return 0;
    ID3D11ShaderResourceView_GetDesc(v, out);
    SAFE_RELEASE(v);
    return 1;
}

TEST(d3d11_views_srv_null_tex2d_single)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 32, 32, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2D);
    EXPECT_EQ(d.Texture2D.MostDetailedMip, 0);
    EXPECT_EQ(d.Texture2D.MipLevels, 1);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_tex2d_full_chain)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 32, 32, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
    EXPECT_EQ(td.MipLevels, 6);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2D);
    EXPECT_EQ(d.Texture2D.MostDetailedMip, 0);
    EXPECT_EQ(d.Texture2D.MipLevels, 6);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_tex2d_array)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 3, 4, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.Format, DXGI_FORMAT_R16G16B16A16_FLOAT);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(d.Texture2DArray.MostDetailedMip, 0);
    EXPECT_EQ(d.Texture2DArray.MipLevels, 3);
    EXPECT_EQ(d.Texture2DArray.FirstArraySlice, 0);
    EXPECT_EQ(d.Texture2DArray.ArraySize, 4);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_tex2d_array_of_two)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 2, DXGI_FORMAT_R32_FLOAT, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
    EXPECT_EQ(d.Texture2DArray.ArraySize, 2);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_cube)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 5, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_TEXTURECUBE, 1);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURECUBE);
    EXPECT_EQ(d.TextureCube.MostDetailedMip, 0);
    EXPECT_EQ(d.TextureCube.MipLevels, 5);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_cube_array)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 12, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_TEXTURECUBE, 1);
    if (!t) { DONE(); t_skip("cube array texture not supported"); return; }
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURECUBEARRAY);
    EXPECT_EQ(d.TextureCubeArray.MostDetailedMip, 0);
    EXPECT_EQ(d.TextureCubeArray.MipLevels, 1);
    EXPECT_EQ(d.TextureCubeArray.First2DArrayFace, 0);
    EXPECT_EQ(d.TextureCubeArray.NumCubes, 2);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_tex2dms)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    if (!ms_ok(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4)) { DONE(); t_skip("4x MSAA unavailable"); return; }
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 4);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2DMS);
    EXPECT_EQ(d.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_tex2dms_array)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    if (!ms_ok(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4)) { DONE(); t_skip("4x MSAA unavailable"); return; }
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 4);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY);
    EXPECT_EQ(d.Texture2DMSArray.FirstArraySlice, 0);
    EXPECT_EQ(d.Texture2DMSArray.ArraySize, 3);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_tex1d)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture1D *t = mk1d(dev, 64, 0, 1, DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.Format, DXGI_FORMAT_R8_UNORM);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE1D);
    EXPECT_EQ(d.Texture1D.MostDetailedMip, 0);
    EXPECT_EQ(d.Texture1D.MipLevels, 7);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_tex1d_array)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture1D *t = mk1d(dev, 16, 2, 3, DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE1DARRAY);
    EXPECT_EQ(d.Texture1DArray.MostDetailedMip, 0);
    EXPECT_EQ(d.Texture1DArray.MipLevels, 2);
    EXPECT_EQ(d.Texture1DArray.FirstArraySlice, 0);
    EXPECT_EQ(d.Texture1DArray.ArraySize, 3);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_tex3d)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture3D *t = mk3d(dev, 16, 16, 8, 0, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    REQUIRE(t);
    CHECK(get_null_srv(dev, t, &d, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE3D);
    EXPECT_EQ(d.Texture3D.MostDetailedMip, 0);
    EXPECT_EQ(d.Texture3D.MipLevels, 5);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_typeless_fails)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R16_TYPELESS,
        DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_R16G16_TYPELESS };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, f[i], D3D11_BIND_SHADER_RESOURCE, 0, 1);
        if (!t) { t_note("typeless format %d not creatable", f[i]); continue; }
        get_null_srv(dev, t, &d, &hr);
        EXPECT_HR(hr, E_INV);
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_srv_null_typeless_depth_fails)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R24G8_TYPELESS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    get_null_srv(dev, t, &d, &hr); EXPECT_HR(hr, E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_srv_null_typed_formats_table)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UINT,
        DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_SINT, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R16_FLOAT,
        DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM,
        DXGI_FORMAT_R32G32B32_FLOAT };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, f[i], D3D11_BIND_SHADER_RESOURCE, 0, 1);
        if (!t) { t_note("format %d texture not creatable", f[i]); continue; }
        CHECK(get_null_srv(dev, t, &d, &hr));
        EXPECT_HR(hr, S_OK);
        EXPECT_EQ(d.Format, f[i]);
        EXPECT_EQ(d.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2D);
        SAFE_RELEASE(t);
    }
    DONE();
}
TEST(d3d11_views_srv_null_buffer)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, 0, 0);
    REQUIRE(b);
    /* A plain (neither structured nor raw) buffer has no format to infer. */
    get_null_srv(dev, b, &d, &hr);
    EXPECT_HR(hr, E_INV);
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_null_structured_buffer)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Buffer *b = mkbuf(dev, 16 * 12, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 12);
    REQUIRE(b);
    if (get_null_srv(dev, b, &d, &hr)) {
        t_note("structured buffer NULL-desc SRV succeeded: fmt=%d dim=%d first=%u num=%u", d.Format, d.ViewDimension, d.Buffer.FirstElement, d.Buffer.NumElements);
    } else t_note("structured buffer NULL-desc SRV hr=%s", d3dh_hr_name(hr));
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_null_raw_buffer)
{
    NEED_DEV(); D3D11_SHADER_RESOURCE_VIEW_DESC d; HRESULT hr;
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    REQUIRE(b);
    if (get_null_srv(dev, b, &d, &hr)) t_note("raw buffer NULL-desc SRV succeeded: fmt=%d dim=%d", d.Format, d.ViewDimension);
    else t_note("raw buffer NULL-desc SRV hr=%s", d3dh_hr_name(hr));
    SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_srv_null_resource)
{
    NEED_DEV(); ID3D11ShaderResourceView *v = (ID3D11ShaderResourceView *)(ULONG_PTR)1;
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2D.MipLevels = 1;
    HRESULT hr = MK_SRV(dev, NULL, &d, &v);
    EXPECT_HR(hr, E_INV);
    hr = MK_SRV(dev, NULL, NULL, NULL);
    EXPECT_HR(hr, E_INV);
    DONE();
}
TEST(d3d11_views_srv_null_desc_not_modified_on_failure)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView *v = NULL;
    HRESULT hr = MK_SRV(dev, t, NULL, &v);
    EXPECT_HR(hr, E_INV);
    CHECK(v == NULL);
    SAFE_RELEASE(t); DONE();
}
