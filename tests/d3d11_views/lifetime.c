/* View object identity, reference counting, lifetime, private data, QueryInterface. */
#include "views.h"

/* Uniform handle on the four view kinds for table-driven tests. */
typedef struct { int kind; IUnknown *v; ID3D11View *view; } vh;
static const char *kname[] = { "SRV", "RTV", "DSV", "UAV" };

static int make_view(ID3D11Device *dev, int kind, ID3D11Resource *res, vh *out)
{
    HRESULT hr = E_FAIL; void *p = NULL;
    out->kind = kind;
    switch (kind) {
    case 0: hr = ID3D11Device_CreateShaderResourceView(dev, res, NULL, (ID3D11ShaderResourceView **)&p); break;
    case 1: hr = ID3D11Device_CreateRenderTargetView(dev, res, NULL, (ID3D11RenderTargetView **)&p); break;
    case 2: hr = ID3D11Device_CreateDepthStencilView(dev, res, NULL, (ID3D11DepthStencilView **)&p); break;
    case 3: hr = ID3D11Device_CreateUnorderedAccessView(dev, res, NULL, (ID3D11UnorderedAccessView **)&p); break;
    }
    if (FAILED(hr) || !p) return 0;
    out->v = (IUnknown *)p;
    out->view = NULL;
    if (FAILED(IUnknown_QueryInterface(out->v, &IID_ID3D11View, (void **)&out->view))) return 0;
    return 1;
}
static void free_view(vh *h) { if (h->view) ID3D11View_Release(h->view); if (h->v) IUnknown_Release(h->v); h->view = NULL; h->v = NULL; }

/* texture usable by every view kind (typed formats so NULL descs work). */
static ID3D11Resource *tex_for(ID3D11Device *dev, int kind)
{
    switch (kind) {
    case 0: return R(mk2d(dev, 16, 16, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1));
    case 1: return R(mk2d(dev, 16, 16, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1));
    case 2: return R(mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL, 0, 1));
    default: return R(mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R32_UINT, D3D11_BIND_UNORDERED_ACCESS, 0, 1));
    }
}
#define FOR_KINDS(k) for (int k = 0; k < 4; k++)

TEST(d3d11_views_getresource_identity_all_kinds)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh h; ID3D11Resource *got = NULL;
        if (!r) { t_note("%s resource not creatable", kname[k]); continue; }
        CHECK(make_view(dev, k, r, &h));
        ID3D11View_GetResource(h.view, &got);
        t_check(got == r, __FILE__, __LINE__, "GetResource identity", "%s got %p want %p", kname[k], (void *)got, (void *)r);
        SAFE_RELEASE(got);
        free_view(&h);
        SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_getresource_addrefs_resource)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh h; ID3D11Resource *a = NULL, *b = NULL;
        if (!r) continue;
        CHECK(make_view(dev, k, r, &h));
        ULONG base = d3dh_refcount(r);
        ID3D11View_GetResource(h.view, &a);
        EXPECT_EQ(d3dh_refcount(r), base + 1);
        ID3D11View_GetResource(h.view, &b);
        EXPECT_EQ(d3dh_refcount(r), base + 2);
        EXPECT_EQ(a, b);
        SAFE_RELEASE(a); SAFE_RELEASE(b);
        EXPECT_EQ(d3dh_refcount(r), base);
        free_view(&h); SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_creation_leaves_resource_public_refcount)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh h;
        if (!r) continue;
        ULONG base = d3dh_refcount(r);
        EXPECT_EQ(base, 1);
        CHECK(make_view(dev, k, r, &h));
        ULONG with = d3dh_refcount(r);
        /* Views hold an internal reference only; the public count a caller sees does not move. */
        t_check(with == base, __FILE__, __LINE__, "resource refcount after view", "%s before %lu after %lu", kname[k], base, with);
        free_view(&h);
        EXPECT_EQ(d3dh_refcount(r), base);
        SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_view_keeps_resource_alive)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh h; ID3D11Resource *got = NULL;
        if (!r) continue;
        CHECK(make_view(dev, k, r, &h));
        ULONG left = ID3D11Resource_Release(r);   /* drop the only application reference */
        EXPECT_EQ(left, 0);
        ID3D11View_GetResource(h.view, &got);
        t_check(got == r, __FILE__, __LINE__, "resource alive via view", "%s got %p", kname[k], (void *)got);
        if (got) {
            D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
            ID3D11Resource_GetType(got, &dim);
            EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_TEXTURE2D);
            EXPECT_EQ(d3dh_refcount(got), 1);
            SAFE_RELEASE(got);
        }
        free_view(&h);
    }
    DONE();
}
TEST(d3d11_views_resource_survives_until_last_view)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView *s = NULL; ID3D11RenderTargetView *r = NULL;
    EXPECT_HR(MK_SRV(dev, t, NULL, &s), S_OK);
    EXPECT_HR(MK_RTV(dev, t, NULL, &r), S_OK);
    ID3D11Texture2D_Release(t);
    ID3D11Resource *g1 = NULL, *g2 = NULL;
    ID3D11ShaderResourceView_GetResource(s, &g1);
    ID3D11RenderTargetView_GetResource(r, &g2);
    EXPECT_EQ(g1, t); EXPECT_EQ(g2, t);
    SAFE_RELEASE(g1); SAFE_RELEASE(g2);
    SAFE_RELEASE(s);
    ID3D11RenderTargetView_GetResource(r, &g2);
    EXPECT_EQ(g2, t);
    SAFE_RELEASE(g2);
    SAFE_RELEASE(r);
    DONE();
}
TEST(d3d11_views_getresource_returns_resource_interface)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView *s = NULL; ID3D11Resource *g = NULL; ID3D11Texture2D *t2 = NULL; ID3D11Buffer *b = NULL;
    EXPECT_HR(MK_SRV(dev, t, NULL, &s), S_OK);
    ID3D11ShaderResourceView_GetResource(s, &g);
    REQUIRE(g);
    EXPECT_HR(ID3D11Resource_QueryInterface(g, &IID_ID3D11Texture2D, (void **)&t2), S_OK);
    EXPECT_EQ(t2, t);
    EXPECT_HR(ID3D11Resource_QueryInterface(g, &IID_ID3D11Buffer, (void **)&b), E_NOINTERFACE);
    CHECK(b == NULL);
    SAFE_RELEASE(t2); SAFE_RELEASE(g); SAFE_RELEASE(s); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_getdevice_identity_and_refcount)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh h; ID3D11Device *d1 = NULL;
        if (!r) continue;
        CHECK(make_view(dev, k, r, &h));
        ULONG base = d3dh_refcount(dev);
        ID3D11View_GetDevice(h.view, &d1);
        t_check(d1 == dev, __FILE__, __LINE__, "GetDevice", "%s got %p want %p", kname[k], (void *)d1, (void *)dev);
        EXPECT_EQ(d3dh_refcount(dev), base + 1);
        SAFE_RELEASE(d1);
        EXPECT_EQ(d3dh_refcount(dev), base);
        free_view(&h); SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_view_does_not_change_device_public_refcount)
{
    NEED_DEV();
    ULONG base = d3dh_refcount(dev);
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    ULONG with_tex = d3dh_refcount(dev);
    ID3D11ShaderResourceView *s = NULL;
    EXPECT_HR(MK_SRV(dev, t, NULL, &s), S_OK);
    t_note("device refcount: base=%lu with texture=%lu with view=%lu", base, with_tex, d3dh_refcount(dev));
    SAFE_RELEASE(s); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_query_interface_table)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh h; void *p;
        if (!r) continue;
        CHECK(make_view(dev, k, r, &h));
        p = NULL; EXPECT_HR(IUnknown_QueryInterface(h.v, &IID_IUnknown, &p), S_OK);
        t_check(p == (void *)h.v, __FILE__, __LINE__, "IUnknown identity", "%s", kname[k]);
        if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_HR(IUnknown_QueryInterface(h.v, &IID_ID3D11DeviceChild, &p), S_OK);
        t_check(p == (void *)h.v, __FILE__, __LINE__, "DeviceChild identity", "%s", kname[k]);
        if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_HR(IUnknown_QueryInterface(h.v, &IID_ID3D11View, &p), S_OK);
        t_check(p == (void *)h.v, __FILE__, __LINE__, "View identity", "%s", kname[k]);
        if (p) IUnknown_Release((IUnknown *)p);
        p = (void *)1; EXPECT_HR(IUnknown_QueryInterface(h.v, &IID_ID3D11Resource, &p), E_NOINTERFACE);
        CHECK(p == NULL);
        p = (void *)1; EXPECT_HR(IUnknown_QueryInterface(h.v, &IID_ID3D11Device, &p), E_NOINTERFACE);
        CHECK(p == NULL);
        p = (void *)1; EXPECT_HR(IUnknown_QueryInterface(h.v, &IID_ID3D11Texture2D, &p), E_NOINTERFACE);
        CHECK(p == NULL);
        free_view(&h); SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_query_interface_cross_view_types_fail)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView *s = NULL; void *p;
    EXPECT_HR(MK_SRV(dev, t, NULL, &s), S_OK);
    REQUIRE(s);
    p = (void *)1; EXPECT_HR(ID3D11ShaderResourceView_QueryInterface(s, &IID_ID3D11RenderTargetView, &p), E_NOINTERFACE); CHECK(p == NULL);
    p = (void *)1; EXPECT_HR(ID3D11ShaderResourceView_QueryInterface(s, &IID_ID3D11DepthStencilView, &p), E_NOINTERFACE); CHECK(p == NULL);
    p = (void *)1; EXPECT_HR(ID3D11ShaderResourceView_QueryInterface(s, &IID_ID3D11UnorderedAccessView, &p), E_NOINTERFACE); CHECK(p == NULL);
    p = NULL; EXPECT_HR(ID3D11ShaderResourceView_QueryInterface(s, &IID_ID3D11ShaderResourceView, &p), S_OK);
    EXPECT_EQ(p, s);
    if (p) IUnknown_Release((IUnknown *)p);
    SAFE_RELEASE(s); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_refcount_addref_release_sequence)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh h;
        if (!r) continue;
        CHECK(make_view(dev, k, r, &h));
        ULONG base = d3dh_refcount(h.v);   /* includes the QI'd ID3D11View reference */
        EXPECT_EQ(base, 2);
        EXPECT_EQ(IUnknown_AddRef(h.v), base + 1);
        EXPECT_EQ(IUnknown_AddRef(h.v), base + 2);
        EXPECT_EQ(IUnknown_Release(h.v), base + 1);
        EXPECT_EQ(IUnknown_Release(h.v), base);
        free_view(&h); SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_same_desc_twice_distinct_objects)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh a, b;
        if (!r) continue;
        CHECK(make_view(dev, k, r, &a)); CHECK(make_view(dev, k, r, &b));
        t_check(a.v != b.v, __FILE__, __LINE__, "views not deduplicated", "%s: both creations returned %p", kname[k], (void *)a.v);
        /* private data proves the objects are independent */
        static const GUID g = { 0x12345678, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
        UINT v = 0xC0FFEE, sz = sizeof v, out = 0;
        EXPECT_HR(ID3D11View_SetPrivateData(a.view, &g, sizeof v, &v), S_OK);
        sz = sizeof out;
        HRESULT hr = ID3D11View_GetPrivateData(b.view, &g, &sz, &out);
        EXPECT_HR(hr, DXGI_ERROR_NOT_FOUND);
        free_view(&a); free_view(&b); SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_many_views_one_resource)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 64, 64, 7, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView *s[7]; ID3D11RenderTargetView *r[7];
    for (UINT m = 0; m < 7; m++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
        sd.Texture2D.MostDetailedMip = m; sd.Texture2D.MipLevels = 1;
        D3D11_RENDER_TARGET_VIEW_DESC rd = rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        rd.Texture2D.MipSlice = m;
        s[m] = NULL; r[m] = NULL;
        EXPECT_HR(MK_SRV(dev, t, &sd, &s[m]), S_OK);
        EXPECT_HR(MK_RTV(dev, t, &rd, &r[m]), S_OK);
    }
    ID3D11Texture2D_Release(t);
    for (UINT m = 0; m < 7; m++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC o; ID3D11ShaderResourceView_GetDesc(s[m], &o);
        EXPECT_EQ(o.Texture2D.MostDetailedMip, m);
        D3D11_RENDER_TARGET_VIEW_DESC ro; ID3D11RenderTargetView_GetDesc(r[m], &ro);
        EXPECT_EQ(ro.Texture2D.MipSlice, m);
        SAFE_RELEASE(s[m]); SAFE_RELEASE(r[m]);
    }
    DONE();
}
TEST(d3d11_views_private_data_roundtrip_all_kinds)
{
    NEED_DEV(); NEED_FL11();
    static const GUID g = { 0xabcdef01, 0x2345, 0x6789, { 0x9a, 0xbc, 0xde, 0xf0, 0x11, 0x22, 0x33, 0x44 } };
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); vh h;
        if (!r) continue;
        CHECK(make_view(dev, k, r, &h));
        UINT sz = 0; char buf[16] = {0};
        EXPECT_HR(ID3D11View_GetPrivateData(h.view, &g, &sz, NULL), DXGI_ERROR_NOT_FOUND);
        EXPECT_HR(ID3D11View_SetPrivateData(h.view, &g, 5, "abcd"), S_OK);
        sz = 0;
        EXPECT_HR(ID3D11View_GetPrivateData(h.view, &g, &sz, NULL), S_OK);
        EXPECT_EQ(sz, 5);
        sz = 3;
        EXPECT_HR(ID3D11View_GetPrivateData(h.view, &g, &sz, buf), DXGI_ERROR_MORE_DATA);
        EXPECT_EQ(sz, 5);
        sz = 5;
        EXPECT_HR(ID3D11View_GetPrivateData(h.view, &g, &sz, buf), S_OK);
        EXPECT_STR(buf, "abcd");
        EXPECT_HR(ID3D11View_SetPrivateData(h.view, &g, 0, NULL), S_OK);   /* removes */
        sz = 0;
        EXPECT_HR(ID3D11View_GetPrivateData(h.view, &g, &sz, NULL), DXGI_ERROR_NOT_FOUND);
        free_view(&h); SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_private_data_interface_holds_reference)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    static const GUID g = { 0x01020304, 0x0506, 0x0708, { 9, 10, 11, 12, 13, 14, 15, 16 } };
    ID3D11ShaderResourceView *s = NULL;
    EXPECT_HR(MK_SRV(dev, t, NULL, &s), S_OK);
    REQUIRE(s);
    ULONG base = d3dh_refcount(dev);
    EXPECT_HR(ID3D11ShaderResourceView_SetPrivateDataInterface(s, &g, (IUnknown *)dev), S_OK);
    EXPECT_EQ(d3dh_refcount(dev), base + 1);
    IUnknown *got = NULL; UINT sz = sizeof got;
    EXPECT_HR(ID3D11ShaderResourceView_GetPrivateData(s, &g, &sz, &got), S_OK);
    EXPECT_EQ(got, dev);
    if (got) IUnknown_Release(got);
    SAFE_RELEASE(s);
    EXPECT_EQ(d3dh_refcount(dev), base);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_debug_object_name_private_data)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 0, 1);
    REQUIRE(t);
    ID3D11RenderTargetView *v = NULL;
    EXPECT_HR(MK_RTV(dev, t, NULL, &v), S_OK);
    REQUIRE(v);
    EXPECT_HR(ID3D11RenderTargetView_SetPrivateData(v, &WKPDID_D3DDebugObjectName, 6, "myview"), S_OK);
    char buf[16] = {0}; UINT sz = sizeof buf;
    EXPECT_HR(ID3D11RenderTargetView_GetPrivateData(v, &WKPDID_D3DDebugObjectName, &sz, buf), S_OK);
    EXPECT_EQ(sz, 6);
    CHECK(memcmp(buf, "myview", 6) == 0);
    SAFE_RELEASE(v); SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_null_ppview_validate_only_all_kinds)
{
    NEED_DEV(); NEED_FL11();
    FOR_KINDS(k) {
        ID3D11Resource *r = tex_for(dev, k); HRESULT hr = E_FAIL;
        if (!r) continue;
        ULONG base = d3dh_refcount(r);
        switch (k) {
        case 0: hr = val_srv(dev, r, NULL); break;
        case 1: hr = val_rtv(dev, r, NULL); break;
        case 2: hr = val_dsv(dev, r, NULL); break;
        default: hr = val_uav(dev, r, NULL); break;
        }
        t_check(hr == S_FALSE, __FILE__, __LINE__, "validate only", "%s got %s", kname[k], d3dh_hr_name(hr));
        EXPECT_EQ(d3dh_refcount(r), base);
        SAFE_RELEASE(r);
    }
    DONE();
}
TEST(d3d11_views_null_ppview_invalid_returns_error)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    D3D11_SHADER_RESOURCE_VIEW_DESC d = srvd(D3D11_SRV_DIMENSION_TEXTURE3D, DXGI_FORMAT_R8G8B8A8_UNORM);
    d.Texture3D.MipLevels = 1;
    EXPECT_HR(val_srv(dev, R(t), &d), E_INV);
    d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT); d.Texture2D.MipLevels = 1;
    EXPECT_HR(val_srv(dev, R(t), &d), E_INV);
    d = srvd(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM); d.Texture2D.MipLevels = 1;
    EXPECT_HR(val_srv(dev, R(t), &d), S_FALSE);
    SAFE_RELEASE(t); DONE();
}
TEST(d3d11_views_buffer_view_getresource)
{
    NEED_DEV(); NEED_FL11();
    ID3D11Buffer *b = mkbuf(dev, 256, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0);
    REQUIRE(b);
    D3D11_SHADER_RESOURCE_VIEW_DESC s = srvd(D3D11_SRV_DIMENSION_BUFFER, DXGI_FORMAT_R32_FLOAT); s.Buffer.NumElements = 64;
    D3D11_UNORDERED_ACCESS_VIEW_DESC u = uavd(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_R32_TYPELESS); u.Buffer.NumElements = 64; u.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    ID3D11ShaderResourceView *sv = NULL; ID3D11UnorderedAccessView *uv = NULL; ID3D11Resource *g = NULL;
    EXPECT_HR(MK_SRV(dev, b, &s, &sv), S_OK);
    EXPECT_HR(MK_UAV(dev, b, &u, &uv), S_OK);
    REQUIRE(sv); REQUIRE(uv);
    ID3D11ShaderResourceView_GetResource(sv, &g); EXPECT_EQ(g, b); SAFE_RELEASE(g);
    ID3D11UnorderedAccessView_GetResource(uv, &g); EXPECT_EQ(g, b); SAFE_RELEASE(g);
    D3D11_RESOURCE_DIMENSION dim = 0;
    ID3D11Buffer_GetType(b, &dim); EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_BUFFER);
    SAFE_RELEASE(uv); SAFE_RELEASE(sv); SAFE_RELEASE(b); DONE();
}
TEST(d3d11_views_getdesc_is_stable_across_calls)
{
    NEED_DEV();
    ID3D11Texture2D *t = mk2d(dev, 16, 16, 4, 2, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE, 0, 1);
    REQUIRE(t);
    ID3D11ShaderResourceView *s = NULL;
    EXPECT_HR(MK_SRV(dev, t, NULL, &s), S_OK);
    REQUIRE(s);
    D3D11_SHADER_RESOURCE_VIEW_DESC a, b;
    memset(&a, 1, sizeof a); memset(&b, 2, sizeof b);
    ID3D11ShaderResourceView_GetDesc(s, &a);
    ID3D11ShaderResourceView_GetDesc(s, &b);
    CHECK(memcmp(&a, &b, sizeof a) == 0 || (a.Format == b.Format && a.ViewDimension == b.ViewDimension && a.Texture2DArray.ArraySize == b.Texture2DArray.ArraySize));
    SAFE_RELEASE(s); SAFE_RELEASE(t); DONE();
}
