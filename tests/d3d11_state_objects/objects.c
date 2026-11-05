/* Properties shared by all state objects: device child behaviour, private data, QI, binding lifetime. */
#include "so_common.h"

typedef IUnknown *(*mk_fn)(void);
static IUnknown *mk_blend(void) { D3D11_BLEND_DESC d = so_blend_default(); ID3D11BlendState *s = NULL; d.RenderTarget[0].RenderTargetWriteMask = 0x0D; SO_CBLEND(&d, &s); return (IUnknown *)s; }
static IUnknown *mk_ds(void) { D3D11_DEPTH_STENCIL_DESC d = so_ds_default(); ID3D11DepthStencilState *s = NULL; d.StencilReadMask = 0x71; SO_CDS(&d, &s); return (IUnknown *)s; }
static IUnknown *mk_rast(void) { D3D11_RASTERIZER_DESC d = so_rast_default(); ID3D11RasterizerState *s = NULL; d.DepthBias = 31337; SO_CRAST(&d, &s); return (IUnknown *)s; }
static IUnknown *mk_samp(void) { D3D11_SAMPLER_DESC d = so_samp_default(); ID3D11SamplerState *s = NULL; d.MipLODBias = 0.3125f; SO_CSAMP(&d, &s); return (IUnknown *)s; }
static IUnknown *mk_query(void) { D3D11_QUERY_DESC d; ID3D11Query *q = NULL; d.Query = D3D11_QUERY_OCCLUSION; d.MiscFlags = 0; ID3D11Device_CreateQuery(so_dev, &d, &q); return (IUnknown *)q; }
static IUnknown *mk_pred(void) { D3D11_QUERY_DESC d; ID3D11Predicate *q = NULL; d.Query = D3D11_QUERY_OCCLUSION_PREDICATE; d.MiscFlags = 0; ID3D11Device_CreatePredicate(so_dev, &d, &q); return (IUnknown *)q; }
static IUnknown *mk_layout(void)
{
    static const char vs[] = "float4 main(float4 p : POSITION) : SV_Position { return p; }\n";
    D3D11_INPUT_ELEMENT_DESC e = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    ID3DBlob *b = so_compile_vs(vs);
    ID3D11InputLayout *l = NULL;
    if (b) ID3D11Device_CreateInputLayout(so_dev, &e, 1, d3dh_blob_ptr(b), d3dh_blob_size(b), &l);
    return (IUnknown *)l;
}
static const struct { const char *name; mk_fn mk; const IID *iid; } KINDS[] = {
    { "blend", mk_blend, &IID_ID3D11BlendState },
    { "depthstencil", mk_ds, &IID_ID3D11DepthStencilState },
    { "rasterizer", mk_rast, &IID_ID3D11RasterizerState },
    { "sampler", mk_samp, &IID_ID3D11SamplerState },
    { "query", mk_query, &IID_ID3D11Query },
    { "predicate", mk_pred, &IID_ID3D11Predicate },
    { "inputlayout", mk_layout, &IID_ID3D11InputLayout },
};
#define NKINDS (sizeof KINDS / sizeof KINDS[0])
static const GUID PD_GUID = { 0x6f1d3a20, 0x1b0c, 0x4d7e, { 0x9a, 0x11, 0x5e, 0x77, 0x31, 0x02, 0xc4, 0x09 } };
static const GUID PD_GUID2 = { 0x6f1d3a21, 0x1b0c, 0x4d7e, { 0x9a, 0x11, 0x5e, 0x77, 0x31, 0x02, 0xc4, 0x0a } };

#define FOREACH_KIND(...) do { unsigned k_; for (k_ = 0; k_ < NKINDS; k_++) { \
    IUnknown *o = KINDS[k_].mk(); const char *kn = KINDS[k_].name; ID3D11DeviceChild *dc = NULL; \
    if (!o) { t_note("cannot create %s", kn); continue; } \
    if (FAILED(IUnknown_QueryInterface(o, &IID_ID3D11DeviceChild, (void **)&dc))) { CHECKF(0, "%s: no ID3D11DeviceChild", kn); IUnknown_Release(o); continue; } \
    __VA_ARGS__; \
    SAFE_RELEASE(dc); IUnknown_Release(o); } } while (0)

ST(obj_all_kinds_are_device_children)
{
    SO_NEED();
    FOREACH_KIND({ CHECKF(dc != NULL, "%s", kn); });
}
ST(obj_getdevice_returns_creating_device)
{
    SO_NEED();
    FOREACH_KIND({
        ID3D11Device *d = NULL;
        ULONG rc = d3dh_refcount(so_dev);
        ID3D11DeviceChild_GetDevice(dc, &d);
        t_check(d == so_dev, __FILE__, __LINE__, "GetDevice", "%s returned %p want %p", kn, (void *)d, (void *)so_dev);
        t_check(d3dh_refcount(so_dev) == rc + 1, __FILE__, __LINE__, "GetDevice refcount", "%s", kn);
        SAFE_RELEASE(d);
    });
}
ST(obj_qi_unknown_identity)
{
    SO_NEED();
    FOREACH_KIND({
        IUnknown *a = NULL, *b = NULL;
        IUnknown_QueryInterface(o, &IID_IUnknown, (void **)&a);
        ID3D11DeviceChild_QueryInterface(dc, &IID_IUnknown, (void **)&b);
        t_check(a == b && a != NULL, __FILE__, __LINE__, "identity", "%s", kn);
        SAFE_RELEASE(a); SAFE_RELEASE(b);
    });
}
ST(obj_qi_own_interface)
{
    SO_NEED();
    FOREACH_KIND({
        IUnknown *a = NULL;
        HRESULT hr = IUnknown_QueryInterface(o, KINDS[k_].iid, (void **)&a);
        t_check(hr == S_OK && a != NULL, __FILE__, __LINE__, "own iid", "%s got %s", kn, d3dh_hr_name(hr));
        SAFE_RELEASE(a);
    });
}
ST(obj_qi_foreign_state_interfaces_fail)
{
    unsigned j;
    SO_NEED();
    FOREACH_KIND({
        for (j = 0; j < NKINDS; j++) {
            void *p = (void *)(ULONG_PTR)0x10;
            HRESULT hr;
            if (j == k_) continue;
            if (!strcmp(kn, "query") && !strcmp(KINDS[j].name, "predicate")) continue;
            if (!strcmp(kn, "predicate") && !strcmp(KINDS[j].name, "query")) continue;
            hr = IUnknown_QueryInterface(o, KINDS[j].iid, &p);
            t_check(hr == E_NOINTERFACE && p == NULL, __FILE__, __LINE__, "foreign iid", "%s as %s got %s", kn, KINDS[j].name, d3dh_hr_name(hr));
            if (SUCCEEDED(hr)) IUnknown_Release((IUnknown *)p);
        }
    });
}
ST(obj_addref_release_return_counts)
{
    SO_NEED();
    FOREACH_KIND({
        ULONG a, b, c;
        a = IUnknown_AddRef(o);
        b = IUnknown_AddRef(o);
        c = IUnknown_Release(o);
        t_check(b == a + 1 && c == a, __FILE__, __LINE__, "refcounts", "%s: %lu %lu %lu", kn, a, b, c);
        IUnknown_Release(o);
    });
}
ST(obj_private_data_not_found)
{
    SO_NEED();
    FOREACH_KIND({
        UINT sz = 77;
        char buf[8];
        HRESULT hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, buf);
        t_check(hr == DXGI_ERROR_NOT_FOUND && sz == 0, __FILE__, __LINE__, "not found", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        sz = 77;
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, NULL);
        t_check(hr == DXGI_ERROR_NOT_FOUND && sz == 0, __FILE__, __LINE__, "not found, size query", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
    });
}
ST(obj_private_data_null_size_pointer)
{
    SO_NEED();
    FOREACH_KIND({
        char buf[8];
        HRESULT hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, NULL, buf);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "null size", "%s got %s", kn, d3dh_hr_name(hr));
    });
}
ST(obj_private_data_set_get_roundtrip)
{
    SO_NEED();
    FOREACH_KIND({
        static const char data[] = "state-object-private";
        char out[64];
        UINT sz;
        HRESULT hr = ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, sizeof data, data);
        t_check(hr == S_OK, __FILE__, __LINE__, "set", "%s got %s", kn, d3dh_hr_name(hr));
        memset(out, 0, sizeof out);
        sz = sizeof out;
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, out);
        t_check(hr == S_OK && sz == sizeof data && !memcmp(out, data, sizeof data), __FILE__, __LINE__, "get", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, 0, NULL);
    });
}
ST(obj_private_data_size_query_and_truncation)
{
    SO_NEED();
    FOREACH_KIND({
        static const char data[16] = "0123456789abcde";
        char out[32];
        UINT sz;
        HRESULT hr;
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, sizeof data, data);
        sz = 0;
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, NULL);
        t_check(hr == S_OK && sz == 16, __FILE__, __LINE__, "size query", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        memset(out, 0x7E, sizeof out);
        sz = 15;
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, out);
        t_check(hr == DXGI_ERROR_MORE_DATA && sz == 16 && out[0] == 0x7E, __FILE__, __LINE__, "too small", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        sz = 0;
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, out);
        t_check(hr == DXGI_ERROR_MORE_DATA && sz == 16, __FILE__, __LINE__, "zero buffer", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        sz = 16;
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, out);
        t_check(hr == S_OK && sz == 16 && !memcmp(out, data, 16), __FILE__, __LINE__, "exact", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        sz = 32;
        memset(out, 0x7E, sizeof out);
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, out);
        t_check(hr == S_OK && sz == 16 && out[16] == 0x7E, __FILE__, __LINE__, "larger buffer", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, 0, NULL);
    });
}
ST(obj_private_data_overwrite_and_remove)
{
    SO_NEED();
    FOREACH_KIND({
        static const char a[] = "AAAA", b[] = "BBBBBBBB";
        char out[16];
        UINT sz = sizeof out;
        HRESULT hr;
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, sizeof a, a);
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, sizeof b, b);
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, out);
        t_check(hr == S_OK && sz == sizeof b && !memcmp(out, b, sizeof b), __FILE__, __LINE__, "overwrite", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        hr = ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, 0, NULL);
        t_check(hr == S_OK, __FILE__, __LINE__, "remove", "%s got %s", kn, d3dh_hr_name(hr));
        sz = sizeof out;
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, out);
        t_check(hr == DXGI_ERROR_NOT_FOUND && sz == 0, __FILE__, __LINE__, "after remove", "%s got %s", kn, d3dh_hr_name(hr));
    });
}
ST(obj_private_data_two_guids_independent)
{
    SO_NEED();
    FOREACH_KIND({
        int x = 1, y = 2, gx = 0, gy = 0;
        UINT sz = sizeof gx;
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, sizeof x, &x);
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID2, sizeof y, &y);
        ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, &gx);
        sz = sizeof gy;
        ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID2, &sz, &gy);
        t_check(gx == 1 && gy == 2, __FILE__, __LINE__, "two guids", "%s got %d %d", kn, gx, gy);
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, 0, NULL);
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID2, 0, NULL);
    });
}
ST(obj_private_data_interface_holds_reference)
{
    SO_NEED();
    FOREACH_KIND({
        ID3D11BlendState *holder = NULL;
        D3D11_BLEND_DESC d = so_blend_default();
        ULONG base;
        d.RenderTarget[0].RenderTargetWriteMask = 0x03;
        d.AlphaToCoverageEnable = TRUE;
        SO_CBLEND(&d, &holder);
        if (!holder) continue;
        base = d3dh_refcount(holder);
        EXPECT_HR(ID3D11DeviceChild_SetPrivateDataInterface(dc, &PD_GUID, (IUnknown *)holder), S_OK);
        t_check(d3dh_refcount(holder) == base + 1, __FILE__, __LINE__, "iface refcount", "%s", kn);
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, 0, NULL);
        t_check(d3dh_refcount(holder) == base, __FILE__, __LINE__, "iface released", "%s", kn);
        SAFE_RELEASE(holder);
    });
}
ST(obj_private_data_interface_size_is_pointer)
{
    SO_NEED();
    FOREACH_KIND({
        ID3D11BlendState *holder = NULL;
        D3D11_BLEND_DESC d = so_blend_default();
        UINT sz = 0;
        HRESULT hr;
        d.RenderTarget[0].RenderTargetWriteMask = 0x03;
        d.AlphaToCoverageEnable = TRUE;
        SO_CBLEND(&d, &holder);
        if (!holder) continue;
        ID3D11DeviceChild_SetPrivateDataInterface(dc, &PD_GUID, (IUnknown *)holder);
        hr = ID3D11DeviceChild_GetPrivateData(dc, &PD_GUID, &sz, NULL);
        t_check(hr == S_OK && sz == sizeof(void *), __FILE__, __LINE__, "iface size", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        ID3D11DeviceChild_SetPrivateData(dc, &PD_GUID, 0, NULL);
        SAFE_RELEASE(holder);
    });
}
ST(obj_debug_object_name_private_data)
{
    /* WKPDID_D3DDebugObjectName: the name is stored as ordinary private data */
    static const GUID WK = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 } };
    SO_NEED();
    FOREACH_KIND({
        static const char name[] = "so-test-object";
        char out[32];
        UINT sz = sizeof out;
        HRESULT hr = ID3D11DeviceChild_SetPrivateData(dc, &WK, sizeof name - 1, name);
        t_check(hr == S_OK, __FILE__, __LINE__, "set name", "%s got %s", kn, d3dh_hr_name(hr));
        memset(out, 0, sizeof out);
        hr = ID3D11DeviceChild_GetPrivateData(dc, &WK, &sz, out);
        t_check(hr == S_OK && sz == sizeof name - 1 && !memcmp(out, name, sz), __FILE__, __LINE__, "get name", "%s got %s size %u", kn, d3dh_hr_name(hr), sz);
        ID3D11DeviceChild_SetPrivateData(dc, &WK, 0, NULL);
    });
}

/* ---- binding to the context --------------------------------------------------------------- */
ST(obj_bound_blend_state_roundtrip_and_lifetime)
{
    D3D11_BLEND_DESC d = so_blend_default();
    ID3D11BlendState *s = NULL, *g = NULL;
    const FLOAT f[4] = { 0.125f, 0.25f, 0.5f, 0.75f };
    FLOAT gf[4] = { 9, 9, 9, 9 };
    UINT mask = 0;
    SO_NEED();
    d.RenderTarget[0].RenderTargetWriteMask = 0x0E;
    d.AlphaToCoverageEnable = TRUE;
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (!s) return;
    ID3D11DeviceContext_OMSetBlendState(so_ctx, s, f, 0x12345678);
    SAFE_RELEASE(s);   /* the context keeps it alive; dedup may also keep it, but Get must still return it */
    ID3D11DeviceContext_OMGetBlendState(so_ctx, &g, gf, &mask);
    CHECK(g != NULL);
    if (g) {
        D3D11_BLEND_DESC gd;
        ID3D11BlendState_GetDesc(g, &gd);
        EXPECT_EQ(gd.RenderTarget[0].RenderTargetWriteMask, 0x0E);
        EXPECT_EQ(gd.AlphaToCoverageEnable, 1);
    }
    CHECK(gf[0] == 0.125f && gf[1] == 0.25f && gf[2] == 0.5f && gf[3] == 0.75f);
    EXPECT_EQ(mask, 0x12345678);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_OMSetBlendState(so_ctx, NULL, NULL, 0xFFFFFFFF);
}
ST(obj_bound_blend_state_null_factor_defaults)
{
    FLOAT gf[4] = { 9, 9, 9, 9 };
    UINT mask = 0;
    ID3D11BlendState *g = NULL;
    SO_NEED();
    ID3D11DeviceContext_OMSetBlendState(so_ctx, NULL, NULL, 0xFFFFFFFF);
    ID3D11DeviceContext_OMGetBlendState(so_ctx, &g, gf, &mask);
    CHECK(gf[0] == 1.f && gf[1] == 1.f && gf[2] == 1.f && gf[3] == 1.f);
    EXPECT_EQ(mask, 0xFFFFFFFF);
    t_note("default blend state pointer after OMSetBlendState(NULL): %p", (void *)g);
    SAFE_RELEASE(g);
}
ST(obj_bound_depth_stencil_roundtrip)
{
    D3D11_DEPTH_STENCIL_DESC d = so_ds_default();
    ID3D11DepthStencilState *s = NULL, *g = NULL;
    UINT ref = 0;
    SO_NEED();
    d.StencilEnable = TRUE;
    d.StencilWriteMask = 0x5A;
    EXPECT_HR(SO_CDS(&d, &s), S_OK);
    if (!s) return;
    ID3D11DeviceContext_OMSetDepthStencilState(so_ctx, s, 0xAB);
    ID3D11DeviceContext_OMGetDepthStencilState(so_ctx, &g, &ref);
    EXPECT_EQ(g, s);
    EXPECT_EQ(ref, 0xAB);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_OMSetDepthStencilState(so_ctx, NULL, 0);
    SAFE_RELEASE(s);
}
ST(obj_bound_rasterizer_roundtrip)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    ID3D11RasterizerState *s = NULL, *g = NULL;
    SO_NEED();
    d.DepthBias = 7070;
    EXPECT_HR(SO_CRAST(&d, &s), S_OK);
    if (!s) return;
    ID3D11DeviceContext_RSSetState(so_ctx, s);
    ID3D11DeviceContext_RSGetState(so_ctx, &g);
    EXPECT_EQ(g, s);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_RSSetState(so_ctx, NULL);
    ID3D11DeviceContext_RSGetState(so_ctx, &g);
    CHECK(g == NULL);
    SAFE_RELEASE(s);
}
ST(obj_bound_samplers_slots_roundtrip)
{
    ID3D11SamplerState *s[3] = { NULL, NULL, NULL }, *g[3] = { NULL, NULL, NULL };
    D3D11_SAMPLER_DESC d = so_samp_default();
    int i;
    SO_NEED();
    for (i = 0; i < 3; i++) {
        d.MipLODBias = (float)i;
        EXPECT_HR(SO_CSAMP(&d, &s[i]), S_OK);
    }
    if (!s[0] || !s[1] || !s[2]) { for (i = 0; i < 3; i++) SAFE_RELEASE(s[i]); return; }
    ID3D11DeviceContext_PSSetSamplers(so_ctx, 13, 3, s);
    ID3D11DeviceContext_PSGetSamplers(so_ctx, 13, 3, g);
    for (i = 0; i < 3; i++) { EXPECT_EQ(g[i], s[i]); SAFE_RELEASE(g[i]); }
    ID3D11DeviceContext_VSSetSamplers(so_ctx, 0, 3, s);
    ID3D11DeviceContext_VSGetSamplers(so_ctx, 0, 3, g);
    for (i = 0; i < 3; i++) { EXPECT_EQ(g[i], s[i]); SAFE_RELEASE(g[i]); }
    ID3D11DeviceContext_ClearState(so_ctx);
    ID3D11DeviceContext_PSGetSamplers(so_ctx, 13, 3, g);
    for (i = 0; i < 3; i++) { CHECK(g[i] == NULL); SAFE_RELEASE(g[i]); }
    for (i = 0; i < 3; i++) SAFE_RELEASE(s[i]);
}
ST(obj_clearstate_unbinds_state_objects)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    ID3D11RasterizerState *s = NULL, *g = NULL;
    SO_NEED();
    d.DepthBias = 99;
    EXPECT_HR(SO_CRAST(&d, &s), S_OK);
    if (!s) return;
    ID3D11DeviceContext_RSSetState(so_ctx, s);
    ID3D11DeviceContext_ClearState(so_ctx);
    ID3D11DeviceContext_RSGetState(so_ctx, &g);
    CHECK(g == NULL);
    SAFE_RELEASE(s);
}

/* ---- per device -------------------------------------------------------------------------------- */
ST(obj_dedup_is_per_device)
{
    ID3D11Device *dev2 = NULL;
    ID3D11DeviceContext *ctx2 = NULL;
    D3D11_BLEND_DESC d = so_blend_default();
    ID3D11BlendState *a = NULL, *b = NULL;
    HRESULT hr;
    SO_NEED();
    hr = d3dh_make_device11(&dev2, &ctx2, NULL, NULL, 0);
    if (FAILED(hr)) { t_skip("second device unavailable"); return; }
    d.RenderTarget[0].RenderTargetWriteMask = 0x0A;
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateBlendState(dev2, &d, &b), S_OK);
    if (a && b) {
        ID3D11Device *owner = NULL;
        EXPECT_NE(a, b);
        ID3D11BlendState_GetDevice(b, &owner);
        EXPECT_EQ(owner, dev2);
        SAFE_RELEASE(owner);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    SAFE_RELEASE(ctx2);
    SAFE_RELEASE(dev2);
}
ST(obj_state_object_keeps_device_alive)
{
    ID3D11Device *dev2 = NULL, *back = NULL;
    ID3D11DeviceContext *ctx2 = NULL;
    D3D11_SAMPLER_DESC d = so_samp_default();
    ID3D11SamplerState *s = NULL;
    HRESULT hr;
    SO_NEED();
    hr = d3dh_make_device11(&dev2, &ctx2, NULL, NULL, 0);
    if (FAILED(hr)) { t_skip("second device unavailable"); return; }
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev2, &d, &s), S_OK);
    SAFE_RELEASE(ctx2);
    SAFE_RELEASE(dev2);
    if (s) {
        ID3D11SamplerState_GetDevice(s, &back);
        CHECK(back != NULL);
        if (back) {
            D3D11_SAMPLER_DESC d2 = so_samp_default();
            ID3D11SamplerState *s2 = NULL;
            d2.MipLODBias = 2.f;
            EXPECT_HR(ID3D11Device_CreateSamplerState(back, &d2, &s2), S_OK);
            SAFE_RELEASE(s2);
            SAFE_RELEASE(back);
        }
        SAFE_RELEASE(s);
    }
}
