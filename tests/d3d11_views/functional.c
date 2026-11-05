/* Views used through the context: clears, counters, sub-resource targeting (WARP-deterministic values only). */
#include "views.h"

static uint32_t px(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *t, UINT sub, UINT x, UINT y, int *ok)
{
    d3dh_image im; uint32_t v = 0;
    *ok = SUCCEEDED(d3dh_readback_texture2d11(dev, ctx, t, sub, &im));
    if (*ok) { v = d3dh_image_pixel32(&im, x, y); d3dh_image_free(&im); }
    return v;
}

TEST(d3d11_views_rtv_clear_and_readback)
{
    NEED_DEV(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11RenderTargetView *v = NULL;
    EXPECT_HR(MK_RTV(dev, t, NULL, &v), S_OK);
    REQUIRE(v);
    float c[4] = { 1, 0, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v, c);
    uint32_t p = px(dev, ctx, t, 0, 3, 3, &ok);
    CHECK(ok); EXPECT_EQ(p, 0xFF0000FFu);
    float c2[4] = { 0, 1, 0, 0 };
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v, c2);
    p = px(dev, ctx, t, 0, 7, 7, &ok);
    CHECK(ok); EXPECT_EQ(p, 0x0000FF00u);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_clear_after_resource_release)
{
    NEED_DEV(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11RenderTargetView *v = NULL; ID3D11Resource *g = NULL;
    EXPECT_HR(MK_RTV(dev, t, NULL, &v), S_OK);
    REQUIRE(v);
    ID3D11Texture2D_Release(t);
    float c[4] = { 0, 0, 1, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v, c);
    ID3D11RenderTargetView_GetResource(v, &g);
    REQUIRE(g);
    ID3D11Texture2D *t2 = NULL;
    EXPECT_HR(ID3D11Resource_QueryInterface(g, &IID_ID3D11Texture2D, (void **)&t2), S_OK);
    if (t2) { uint32_t p = px(dev, ctx, t2, 0, 0, 0, &ok); CHECK(ok); EXPECT_EQ(p, 0xFFFF0000u); SAFE_RELEASE(t2); }
    SAFE_RELEASE(g); SAFE_RELEASE(v); DONE();
}
TEST(d3d11_views_rtv_array_slice_targeting)
{
    NEED_DEV(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 4, 4, 1, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11RenderTargetView *all = NULL, *s1 = NULL;
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture2DArray.FirstArraySlice = 0; d.Texture2DArray.ArraySize = 3;
    EXPECT_HR(MK_RTV(dev, t, &d, &all), S_OK);
    d.Texture2DArray.FirstArraySlice = 1; d.Texture2DArray.ArraySize = 1;
    EXPECT_HR(MK_RTV(dev, t, &d, &s1), S_OK);
    REQUIRE(all); REQUIRE(s1);
    float z[4] = { 0, 0, 0, 0 }, w[4] = { 1, 1, 1, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(ctx, all, z);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, s1, w);
    EXPECT_EQ(px(dev, ctx, t, 0, 1, 1, &ok), 0u); CHECK(ok);
    EXPECT_EQ(px(dev, ctx, t, 1, 1, 1, &ok), 0xFFFFFFFFu); CHECK(ok);
    EXPECT_EQ(px(dev, ctx, t, 2, 1, 1, &ok), 0u); CHECK(ok);
    SAFE_RELEASE(s1); SAFE_RELEASE(all); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_mip_slice_targeting)
{
    NEED_DEV(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 3, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11RenderTargetView *v0 = NULL, *v1 = NULL, *v2 = NULL;
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR(MK_RTV(dev, t, &d, &v0), S_OK);
    d.Texture2D.MipSlice = 1; EXPECT_HR(MK_RTV(dev, t, &d, &v1), S_OK);
    d.Texture2D.MipSlice = 2; EXPECT_HR(MK_RTV(dev, t, &d, &v2), S_OK);
    REQUIRE(v0); REQUIRE(v1); REQUIRE(v2);
    float a[4] = { 1, 0, 0, 1 }, b[4] = { 0, 1, 0, 1 }, c[4] = { 0, 0, 1, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v0, a);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v1, b);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v2, c);
    EXPECT_EQ(px(dev, ctx, t, 0, 7, 7, &ok), 0xFF0000FFu); CHECK(ok);
    EXPECT_EQ(px(dev, ctx, t, 1, 3, 3, &ok), 0xFF00FF00u); CHECK(ok);
    EXPECT_EQ(px(dev, ctx, t, 2, 1, 1, &ok), 0xFFFF0000u); CHECK(ok);
    SAFE_RELEASE(v2); SAFE_RELEASE(v1); SAFE_RELEASE(v0); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_typeless_cast_clear)
{
    NEED_DEV(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    ID3D11RenderTargetView *v = NULL;
    EXPECT_HR(MK_RTV(dev, t, &d, &v), S_OK);
    REQUIRE(v);
    float one[4] = { 1, 1, 1, 1 }, zero[4] = { 0, 0, 0, 0 };
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v, one);
    EXPECT_EQ(px(dev, ctx, t, 0, 0, 0, &ok), 0xFFFFFFFFu); CHECK(ok);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v, zero);
    EXPECT_EQ(px(dev, ctx, t, 0, 0, 0, &ok), 0u); CHECK(ok);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_rtv_uint_view_clear_via_uint_cast)
{
    NEED_DEV(); NEED_FL11(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 4, 4, 1, 1, DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    D3D11_RENDER_TARGET_VIEW_DESC d = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT);
    ID3D11RenderTargetView *v = NULL;
    EXPECT_HR(MK_RTV(dev, t, &d, &v), S_OK);
    REQUIRE(v);
    float c[4] = { 1.0f, 0, 0, 0 };
    ID3D11DeviceContext_ClearRenderTargetView(ctx, v, c);
    EXPECT_EQ(px(dev, ctx, t, 0, 2, 2, &ok), 0x3F800000u); CHECK(ok);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_dsv_clear_depth_readback)
{
    NEED_DEV(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 4, 4, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1);
    REQUIRE(t);
    ID3D11DepthStencilView *v = NULL;
    EXPECT_HR(MK_DSV(dev, t, NULL, &v), S_OK);
    REQUIRE(v);
    ID3D11DeviceContext_ClearDepthStencilView(ctx, v, D3D11_CLEAR_DEPTH, 0.5f, 0);
    uint32_t p = px(dev, ctx, t, 0, 1, 1, &ok);
    if (ok) EXPECT_EQ(p, 0x3F000000u); else t_note("depth readback unsupported");
    ID3D11DeviceContext_ClearDepthStencilView(ctx, v, D3D11_CLEAR_DEPTH, 1.0f, 0);
    p = px(dev, ctx, t, 0, 3, 3, &ok);
    if (ok) EXPECT_EQ(p, 0x3F800000u);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_buffer_clear_uint_range)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 64 * 4, D3D11_BIND_UNORDERED_ACCESS, 0, 0);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC all = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_UINT), part = all;
    all.Buffer.NumElements = 64; part.Buffer.FirstElement = 16; part.Buffer.NumElements = 16;
    ID3D11UnorderedAccessView *va = NULL, *vp = NULL;
    EXPECT_HR(MK_UAV(dev, b, &all, &va), S_OK);
    EXPECT_HR(MK_UAV(dev, b, &part, &vp), S_OK);
    REQUIRE(va); REQUIRE(vp);
    UINT zero[4] = { 0, 0, 0, 0 }, mark[4] = { 0xDEADBEEFu, 0, 0, 0 };
    ID3D11DeviceContext_ClearUnorderedAccessViewUint(ctx, va, zero);
    ID3D11DeviceContext_ClearUnorderedAccessViewUint(ctx, vp, mark);
    UINT *data = NULL; size_t sz = 0;
    if (FAILED(d3dh_readback_buffer11(dev, ctx, b, (void **)&data, &sz))) { t_note("readback failed"); }
    else {
        EXPECT_EQ(sz, 256);
        for (UINT i = 0; i < 64 && i < sz / 4; i++) {
            UINT want = (i >= 16 && i < 32) ? 0xDEADBEEFu : 0;
            if (data[i] != want) { CHECKF(0, "elem %u got %08x want %08x", i, data[i], want); break; }
        }
        free(data);
    }
    SAFE_RELEASE(vp); SAFE_RELEASE(va); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_raw_buffer_clear_uint)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 32 * 4, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_TYPELESS);
    d.Buffer.NumElements = 32; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    ID3D11UnorderedAccessView *v = NULL;
    EXPECT_HR(MK_UAV(dev, b, &d, &v), S_OK);
    REQUIRE(v);
    UINT val[4] = { 0x01020304u, 0x11111111u, 0x22222222u, 0x33333333u };
    ID3D11DeviceContext_ClearUnorderedAccessViewUint(ctx, v, val);
    UINT *data = NULL; size_t sz = 0;
    if (SUCCEEDED(d3dh_readback_buffer11(dev, ctx, b, (void **)&data, &sz))) {
        EXPECT_EQ(sz, 128);
        int bad = 0;
        for (UINT i = 0; i < 32; i++) if (data[i] != 0x01020304u) bad++;
        t_check(bad == 0, __FILE__, __LINE__, "raw clear", "%d words differ", bad);
        free(data);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_texture_clear_uint)
{
    NEED_DEV(); NEED_FL11(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 4, 4, 2, 1, DXGI_FORMAT_R32_UINT, D3D11_BIND_UNORDERED_ACCESS, 0, 1);
    REQUIRE(t);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_UINT);
    ID3D11UnorderedAccessView *v0 = NULL, *v1 = NULL;
    EXPECT_HR(MK_UAV(dev, t, &d, &v0), S_OK);
    d.Texture2D.MipSlice = 1; EXPECT_HR(MK_UAV(dev, t, &d, &v1), S_OK);
    REQUIRE(v0); REQUIRE(v1);
    UINT a[4] = { 7, 7, 7, 7 }, b[4] = { 9, 9, 9, 9 };
    ID3D11DeviceContext_ClearUnorderedAccessViewUint(ctx, v0, a);
    ID3D11DeviceContext_ClearUnorderedAccessViewUint(ctx, v1, b);
    EXPECT_EQ(px(dev, ctx, t, 0, 3, 3, &ok), 7u); CHECK(ok);
    EXPECT_EQ(px(dev, ctx, t, 1, 1, 1, &ok), 9u); CHECK(ok);
    SAFE_RELEASE(v1); SAFE_RELEASE(v0); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_uav_float_clear_r32_float)
{
    NEED_DEV(); NEED_FL11(); int ok;
    ID3D11Texture2D *t = mk2d(dev, 4, 4, 1, 1, DXGI_FORMAT_R32_FLOAT, D3D11_BIND_UNORDERED_ACCESS, 0, 1);
    REQUIRE(t);
    ID3D11UnorderedAccessView *v = NULL;
    EXPECT_HR(MK_UAV(dev, t, NULL, &v), S_OK);
    REQUIRE(v);
    float c[4] = { 2.0f, 0, 0, 0 };
    ID3D11DeviceContext_ClearUnorderedAccessViewFloat(ctx, v, c);
    EXPECT_EQ(px(dev, ctx, t, 0, 1, 2, &ok), 0x40000000u); CHECK(ok);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
static UINT counter_of(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11UnorderedAccessView *v, int *ok)
{
    ID3D11Buffer *dst = mkbuf(dev, 16, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    UINT r = 0xFFFFFFFFu; *ok = 0;
    if (!dst) return r;
    ID3D11DeviceContext_CopyStructureCount(ctx, dst, 0, v);
    UINT *data = NULL; size_t sz = 0;
    if (SUCCEEDED(d3dh_readback_buffer11(dev, ctx, dst, (void **)&data, &sz))) { r = data[0]; *ok = 1; free(data); }
    SAFE_RELEASE(dst);
    return r;
}
TEST(d3d11_views_uav_initial_count_append)
{
    NEED_DEV(); NEED_FL11(); int ok;
    ID3D11Buffer *b = mkbuf(dev, 16 * 16, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
    d.Buffer.NumElements = 16; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
    ID3D11UnorderedAccessView *v = NULL;
    EXPECT_HR(MK_UAV(dev, b, &d, &v), S_OK);
    REQUIRE(v);
    static const UINT counts[] = { 0, 5, 12, 1 };
    for (unsigned i = 0; i < 4; i++) {
        UINT init = counts[i];
        ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, 1, &v, &init);
        UINT c = counter_of(dev, ctx, v, &ok);
        CHECK(ok); t_check(c == counts[i], __FILE__, __LINE__, "initial count", "set %u got %u", counts[i], c);
    }
    UINT keep = (UINT)-1;
    UINT seven = 7;
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, 1, &v, &seven);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, 1, &v, &keep);
    UINT c = counter_of(dev, ctx, v, &ok);
    CHECK(ok); EXPECT_EQ(c, 7);
    ID3D11UnorderedAccessView *nullv = NULL;
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, 1, &nullv, NULL);
    SAFE_RELEASE(v); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_uav_initial_count_counter_flag)
{
    NEED_DEV(); NEED_FL11(); int ok;
    ID3D11Buffer *b = mkbuf(dev, 16 * 8, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 8);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
    d.Buffer.NumElements = 16; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    ID3D11UnorderedAccessView *v = NULL;
    EXPECT_HR(MK_UAV(dev, b, &d, &v), S_OK);
    REQUIRE(v);
    UINT init = 42;
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 3, 1, &v, &init);
    UINT c = counter_of(dev, ctx, v, &ok);
    CHECK(ok); EXPECT_EQ(c, 42);
    ID3D11UnorderedAccessView *nullv = NULL;
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 3, 1, &nullv, NULL);
    SAFE_RELEASE(v); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_cs_get_unordered_access_views_addrefs)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 64, D3D11_BIND_UNORDERED_ACCESS, 0, 0);
    REQUIRE(b);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_UINT); d.Buffer.NumElements = 16;
    ID3D11UnorderedAccessView *v = NULL, *g = NULL;
    EXPECT_HR(MK_UAV(dev, b, &d, &v), S_OK);
    REQUIRE(v);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, 1, &v, NULL);
    ULONG base = d3dh_refcount(v);
    ID3D11DeviceContext_CSGetUnorderedAccessViews(ctx, 0, 1, &g);
    EXPECT_EQ(g, v);
    EXPECT_EQ(d3dh_refcount(v), base + 1);
    SAFE_RELEASE(g);
    ID3D11UnorderedAccessView *nullv = NULL;
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, 1, &nullv, NULL);
    EXPECT_EQ(d3dh_refcount(v), base - 1);
    SAFE_RELEASE(v); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_ps_srv_and_om_rtv_bind_get_roundtrip)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView *s = NULL, *sg = NULL; ID3D11RenderTargetView *r = NULL, *rg = NULL; ID3D11DepthStencilView *dg = (ID3D11DepthStencilView *)1;
    EXPECT_HR(MK_SRV(dev, t, NULL, &s), S_OK);
    EXPECT_HR(MK_RTV(dev, t, NULL, &r), S_OK);
    REQUIRE(s); REQUIRE(r);
    ID3D11DeviceContext_PSSetShaderResources(ctx, 2, 1, &s);
    ID3D11DeviceContext_PSGetShaderResources(ctx, 2, 1, &sg);
    EXPECT_EQ(sg, s); SAFE_RELEASE(sg);
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &r, NULL);
    ID3D11DeviceContext_OMGetRenderTargets(ctx, 1, &rg, &dg);
    EXPECT_EQ(rg, r); EXPECT_EQ(dg, NULL);
    SAFE_RELEASE(rg);
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 0, NULL, NULL);
    ID3D11ShaderResourceView *nv = NULL;
    ID3D11DeviceContext_PSSetShaderResources(ctx, 2, 1, &nv);
    SAFE_RELEASE(r); SAFE_RELEASE(s); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_generate_mips_on_srv)
{
    NEED_DEV(); int ok;
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t = NULL;
    memset(&td, 0, sizeof td);
    td.Width = td.Height = 8; td.MipLevels = 0; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    REQUIRE(SUCCEEDED(ID3D11Device_CreateTexture2D(dev, &td, NULL, &t)));
    ID3D11ShaderResourceView *s = NULL; ID3D11RenderTargetView *r = NULL;
    EXPECT_HR(MK_SRV(dev, t, NULL, &s), S_OK);
    EXPECT_HR(MK_RTV(dev, t, NULL, &r), S_OK);
    REQUIRE(s); REQUIRE(r);
    float c[4] = { 1, 1, 1, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(ctx, r, c);
    ID3D11DeviceContext_GenerateMips(ctx, s);
    uint32_t p = px(dev, ctx, t, 3, 0, 0, &ok);
    CHECK(ok); EXPECT_EQ(p, 0xFFFFFFFFu);
    SAFE_RELEASE(r); SAFE_RELEASE(s); SAFE_RELEASE(t); DONE();
}
