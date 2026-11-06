/* Argument validation, cross-device resources, usages and multisample array views (measured on Windows 10 22H2 / WARP). */
#include "views.h"

TEST(d3d11_views_null_resource_rejected)
{
    NEED_DEV();
    ID3D11ShaderResourceView *s = (void *)1; ID3D11RenderTargetView *r = (void *)1;
    ID3D11DepthStencilView *d = (void *)1; ID3D11UnorderedAccessView *u = (void *)1;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, NULL, NULL, &s), E_INV);
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(dev, NULL, NULL, &r), E_INV);
    EXPECT_HR(ID3D11Device_CreateDepthStencilView(dev, NULL, NULL, &d), E_INV);
    EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(dev, NULL, NULL, &u), E_INV);
    s = NULL; EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, NULL, NULL, &s), E_INV); CHECK(s == NULL);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, NULL, NULL, NULL), E_INV);
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(dev, NULL, NULL, NULL), E_INV);
    EXPECT_HR(ID3D11Device_CreateDepthStencilView(dev, NULL, NULL, NULL), E_INV);
    EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(dev, NULL, NULL, NULL), E_INV);
    DONE();
}
TEST(d3d11_views_null_out_pointer_is_validate_only)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS, 0, 1);
    REQUIRE(t);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, R(t), NULL, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(dev, R(t), NULL, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(dev, R(t), NULL, NULL), S_FALSE);
    /* an invalid description is still diagnosed without an output pointer */
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE3D, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, R(t), &d, NULL), E_INV);
    /* a view the resource cannot back fails even when validate-only: no UAV bind flag */
    ID3D11Texture2D *n = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    if (n) { EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(dev, R(n), NULL, NULL), E_INV); SAFE_RELEASE(n); }
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_resource_from_another_device)
{
    NEED_DEV();
    ID3D11Device *d2 = NULL; ID3D11DeviceContext *c2 = NULL;
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 1);
    if (!t || FAILED(d3dh_make_device11(&d2, &c2, NULL, NULL, 0))) { t_skip("second device"); SAFE_RELEASE(t); DONE(); return; }
    ID3D11ShaderResourceView *v = (void *)1;
    /* Win10: a resource owned by another device is reported as E_OUTOFMEMORY, not E_INVALIDARG */
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(d2, R(t), NULL, &v), E_OUTOFMEMORY);
    ID3D11RenderTargetView *r = (void *)1;
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(d2, R(t), NULL, &r), E_OUTOFMEMORY);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, R(t), NULL, &v), S_OK);
    SAFE_RELEASE(v);
    ID3D11DeviceContext_Release(c2); ID3D11Device_Release(d2);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_usage_staging_and_dynamic)
{
    NEED_DEV();
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = 8; td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *st = NULL, *dy = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture2D(dev, &td, NULL, &st), S_OK);
    if (st) { EXPECT_HR(TRY_SRV(dev, st, NULL), E_INV); SAFE_RELEASE(st); } /* staging resources have no bind flags */
    td.Usage = D3D11_USAGE_DYNAMIC; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    EXPECT_HR(ID3D11Device_CreateTexture2D(dev, &td, NULL, &dy), S_OK);
    if (dy) { EXPECT_HR(TRY_SRV(dev, dy, NULL), S_OK); SAFE_RELEASE(dy); }
    DONE();
}
TEST(d3d11_views_ms_array_rtv_ranges_by_sample_count)
{
    NEED_DEV();
    static const UINT ns[] = { 2, 4, 8, 16 };
    for (unsigned i = 0; i < 4; i++) {
        ID3D11Texture2D *m = mk2d(dev, 16, 16, 1, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, ns[i]);
        if (!m) { t_note("%ux MSAA array texture not creatable", ns[i]); continue; } /* not asserted: WARP supports 2/4/8/16 (probed) but other drivers need not */
        D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
        d.Texture2DMSArray.FirstArraySlice = 1; d.Texture2DMSArray.ArraySize = 2;
        CHECKF(try_rtv(dev, R(m), &d) == S_OK, "%ux msarray(1,2)", ns[i]);
        d.Texture2DMSArray.ArraySize = 3;
        CHECKF(try_rtv(dev, R(m), &d) == E_INV, "%ux msarray(1,3)", ns[i]);
        d.Texture2DMSArray.ArraySize = (UINT)-1;
        CHECKF(try_rtv(dev, R(m), &d) == S_OK, "%ux msarray(1,-1)", ns[i]);
        SAFE_RELEASE(m);
    }
    DONE();
}
