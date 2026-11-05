/* Additional view-creation contract tests. */
#include "views.h"

TEST(d3d11_views_extra_srv_null_pv_valid_desc_s_false)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2D.MipLevels = 1;
    EXPECT_HR(val_srv(dev, R(t), &d), S_FALSE);
    d.Format = DXGI_FORMAT_R32_FLOAT;
    EXPECT_HR(val_srv(dev, R(t), &d), E_INV);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_extra_srv_cast_incompatible_family_table)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    static const struct { DXGI_FORMAT f; HRESULT want; } tab[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM, S_OK }, { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, S_OK }, { DXGI_FORMAT_R8G8B8A8_UINT, S_OK },
        { DXGI_FORMAT_R8G8B8A8_SNORM, S_OK }, { DXGI_FORMAT_R8G8B8A8_SINT, S_OK },
        { DXGI_FORMAT_R32_FLOAT, E_INVALIDARG }, { DXGI_FORMAT_R16G16_UNORM, E_INVALIDARG }, { DXGI_FORMAT_B8G8R8A8_UNORM, E_INVALIDARG },
        { DXGI_FORMAT_R10G10B10A2_UNORM, E_INVALIDARG }, { DXGI_FORMAT_R16G16B16A16_FLOAT, E_INVALIDARG },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, tab[i].f);
        d.Texture2D.MipLevels = 1;
        HRESULT hr = TRY_SRV(dev, t, &d);
        t_check(hr == tab[i].want, __FILE__, __LINE__, "srv cast", "fmt %d got %s", tab[i].f, d3dh_hr_name(hr));
    }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_extra_rtv_srv_uav_same_resource_independent)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS, 0, 1);
    if (!t) { DONE(); t_skip("typeless R32 with SRV|RTV|UAV not creatable"); return; }
    ID3D11ShaderResourceView *s = NULL; ID3D11RenderTargetView *r = NULL; ID3D11UnorderedAccessView *u = NULL;
    D3D11_SHADER_RESOURCE_VIEW_DESC sd = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT); sd.Texture2D.MipLevels = 1;
    D3D11_RENDER_TARGET_VIEW_DESC rd = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_UINT);
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud = uavd(D3D11_UAV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
    EXPECT_HR(MK_SRV(dev, t, &sd, &s), S_OK);
    EXPECT_HR(MK_RTV(dev, t, &rd, &r), S_OK);
    EXPECT_HR(MK_UAV(dev, t, &ud, &u), S_OK);
    CHECK(s && r && u);
    if (s && r && u) { CHECK((void *)s != (void *)r); CHECK((void *)r != (void *)u); }
    SAFE_RELEASE(s); SAFE_RELEASE(r); SAFE_RELEASE(u); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_extra_failed_creation_keeps_resource_refcount)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    ULONG base = d3dh_refcount(t);
    ID3D11RenderTargetView *v = NULL;
    EXPECT_HR(MK_RTV(dev, t, NULL, &v), E_INV);   /* no BIND_RENDER_TARGET */
    CHECK(v == NULL);
    EXPECT_EQ(d3dh_refcount(t), base);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_extra_getdesc_unknown_format_resolved)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC rd = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_UNKNOWN), o;
    ID3D11RenderTargetView *r = NULL;
    EXPECT_HR(MK_RTV(dev, t, &rd, &r), S_OK);
    REQUIRE(r);
    ID3D11RenderTargetView_GetDesc(r, &o);
    EXPECT_EQ(o.Format, DXGI_FORMAT_R16_FLOAT);
    SAFE_RELEASE(r); SAFE_RELEASE(t); DONE();
}
