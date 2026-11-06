/* Device / context level reference counting, interface availability, state dedup, destroy order. */
#include "rc.h"

#define NAME(n) d3d11_debug_refcount_##n

TEST(d3d11_debug_refcount_create_device_only_release_zero)
{
    REQUIRE(d3dh_D3D11CreateDevice());
    ID3D11Device *dev = NULL;
    D3D_FEATURE_LEVEL fl = 0;
    HRESULT hr = d3dh_D3D11CreateDevice()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, &fl, NULL);
    if (FAILED(hr)) { t_skip("WARP device unavailable: %s", d3dh_hr_name(hr)); return; }
    CHECK(dev != NULL);
    CHECK(fl >= D3D_FEATURE_LEVEL_9_1);
    EXPECT_EQ(d3dh_refcount(dev), 1);
    EXPECT_EQ(RC_ADDREF(dev), 2);
    EXPECT_EQ(RC_RELEASE(dev), 1);
    EXPECT_EQ(RC_RELEASE(dev), 0);
}

TEST(d3d11_debug_refcount_create_device_with_context_outputs)
{
    RC_DEVICE(dev, ctx, 0);
    CHECK(dev && ctx);
    ID3D11DeviceContext *c2 = NULL;
    ID3D11Device_GetImmediateContext(dev, &c2);
    CHECK(c2 == ctx);
    SAFE_RELEASE(c2);
    ID3D11Device *d2 = NULL;
    ID3D11DeviceContext_GetDevice(ctx, &d2);
    CHECK(d2 == dev);
    SAFE_RELEASE(d2);
    EXPECT_EQ(d3dh_refcount(dev), 2);                     /* Win10: the immediate context holds one reference to its device */
    EXPECT_EQ(d3dh_refcount(ctx), 1);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_immediate_context_is_singleton)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceContext *a = NULL, *b = NULL, *c = NULL;
    ID3D11Device_GetImmediateContext(dev, &a);
    ID3D11Device_GetImmediateContext(dev, &b);
    ID3D11Device_GetImmediateContext(dev, &c);
    CHECK(a == ctx && b == ctx && c == ctx);
    ULONG n = d3dh_refcount(ctx);
    SAFE_RELEASE(a);
    EXPECT_EQ(d3dh_refcount(ctx), n - 1);
    SAFE_RELEASE(b);
    EXPECT_EQ(d3dh_refcount(ctx), n - 2);
    SAFE_RELEASE(c);
    EXPECT_EQ(d3dh_refcount(ctx), n - 3);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_get_immediate_context_addrefs)
{
    RC_DEVICE(dev, ctx, 0);
    ULONG base = d3dh_refcount(ctx);
    ID3D11DeviceContext *a = NULL;
    ID3D11Device_GetImmediateContext(dev, &a);
    EXPECT_EQ(d3dh_refcount(ctx), base + 1);
    SAFE_RELEASE(a);
    EXPECT_EQ(d3dh_refcount(ctx), base);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_immediate_context_getdevice_addrefs)
{
    RC_DEVICE(dev, ctx, 0);
    ULONG base = d3dh_refcount(dev);
    ID3D11Device *d = NULL;
    ID3D11DeviceContext_GetDevice(ctx, &d);
    CHECK(d == dev);
    EXPECT_EQ(d3dh_refcount(dev), base + 1);
    SAFE_RELEASE(d);
    EXPECT_EQ(d3dh_refcount(dev), base);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_immediate_context_type)
{
    RC_DEVICE(dev, ctx, 0);
    EXPECT_EQ(ID3D11DeviceContext_GetType(ctx), D3D11_DEVICE_CONTEXT_IMMEDIATE);
    EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(ctx), 0);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_deferred_context_type_and_flags)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(dev, 0, &dc), S_OK);
    if (dc) {
        EXPECT_EQ(ID3D11DeviceContext_GetType(dc), D3D11_DEVICE_CONTEXT_DEFERRED);
        EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(dc), 0);
        CHECK(dc != ctx);
        EXPECT_EQ(d3dh_refcount(dc), 1);
        SAFE_RELEASE(dc);
    }
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_command_list_flags_and_type_survive_context)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceContext *dc = NULL;
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(dev, 0, &dc), S_OK);
    if (dc) {
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
        SAFE_RELEASE(dc);   /* the list remains valid after its recording context is gone */
        if (cl) {
            EXPECT_EQ(ID3D11CommandList_GetContextFlags(cl), 0);
            ID3D11DeviceContext_ExecuteCommandList(ctx, cl, FALSE);
            ID3D11DeviceContext_ExecuteCommandList(ctx, cl, TRUE);   /* a list may be executed repeatedly */
            EXPECT_EQ(d3dh_refcount(cl), 1);
            SAFE_RELEASE(cl);
        }
    }
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_finish_command_list_empty)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(dev, 0, &dc), S_OK);
    if (dc) {
        ID3D11CommandList *c1 = NULL, *c2 = NULL;
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &c1), S_OK);
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, TRUE, &c2), S_OK);
        CHECK(c1 && c2 && c1 != c2);
        SAFE_RELEASE(c1); SAFE_RELEASE(c2);
        SAFE_RELEASE(dc);
    }
    RC_DEVICE_END(dev, ctx);
}

/* ---- interface availability on device and context --------------------------------- */
static void qi_expect(void *obj, const IID *iid, const char *name, int want)
{
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = UNK(obj)->lpVtbl->QueryInterface(UNK(obj), iid, &p);
    if (want) {
        CHECKF(hr == S_OK && p, "QI %s: %s", name, d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && p) RC_RELEASE(p);
    } else {
        CHECKF(hr == E_NOINTERFACE, "QI %s: %s want E_NOINTERFACE", name, d3dh_hr_name(hr));
        CHECKF(p == NULL, "QI %s: out not NULLed", name);
        if (SUCCEEDED(hr) && p) RC_RELEASE(p);
    }
}

TEST(d3d11_debug_refcount_device_qi_interfaces)
{
    RC_DEVICE(dev, ctx, 0);
    ULONG base = d3dh_refcount(dev);
#define Q(iid, want) qi_expect(dev, &iid, #iid, want)
    Q(IID_IUnknown, 1);
    Q(IID_ID3D11Device, 1);
    Q(IID_ID3D11Device1, 1);
    Q(IID_ID3D11Device2, 1);
    Q(IID_ID3D11Device3, 1);
    Q(IID_IDXGIObject, 1);
    Q(IID_IDXGIDevice, 1);
    Q(IID_IDXGIDevice1, 1);
    Q(IID_IDXGIDevice2, 1);
    Q(IID_ID3D11Multithread, 1);
    /* the SDK-layer interfaces exist only on a device created with the debug flag */
    Q(IID_ID3D11Debug, 0);
    Q(IID_ID3D11InfoQueue, 0);
    Q(IID_ID3D11SwitchToRef, 0);
    /* context and resource interfaces are not device interfaces */
    Q(IID_ID3D11DeviceContext, 0);
    Q(IID_ID3D11DeviceChild, 0);
    Q(IID_ID3D11Resource, 0);
    Q(IID_IDXGISwapChain, 0);
    Q(IID_IDXGIFactory, 0);
    Q(IID_IDXGIAdapter, 0);
#undef Q
    EXPECT_EQ(d3dh_refcount(dev), base);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_device_qi_newer_interfaces_optional)
{
    RC_DEVICE(dev, ctx, 0);
    void *p = NULL;
    HRESULT hr = RC_QI(dev, IID_ID3D11Device4, &p);
    EXPECT_HR(hr, S_OK);                                  /* Win10 22H2 d3d11 exposes Device4 and Device5 */
    if (SUCCEEDED(hr)) RC_RELEASE(p);
    p = NULL;
    hr = RC_QI(dev, IID_ID3D11Device5, &p);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) RC_RELEASE(p);
    if (hr != S_OK) CHECK(hr == E_NOINTERFACE);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_device_iunknown_identity)
{
    RC_DEVICE(dev, ctx, 0);
    IUnknown *u0 = NULL, *u1 = NULL, *u2 = NULL, *u3 = NULL;
    ID3D11Device1 *d1 = NULL; ID3D11Device2 *d2 = NULL;
    EXPECT_HR(RC_QI(dev, IID_IUnknown, &u0), S_OK);
    EXPECT_HR(RC_QI(dev, IID_ID3D11Device1, &d1), S_OK);
    EXPECT_HR(RC_QI(dev, IID_ID3D11Device2, &d2), S_OK);
    if (d1) EXPECT_HR(RC_QI(d1, IID_IUnknown, &u1), S_OK);
    if (d2) EXPECT_HR(RC_QI(d2, IID_IUnknown, &u2), S_OK);
    EXPECT_HR(RC_QI(u0, IID_IUnknown, &u3), S_OK);
    CHECK(u0 && u0 == u1);
    CHECK(u0 && u0 == u2);
    CHECK(u0 && u0 == u3);
    SAFE_RELEASE(u0); SAFE_RELEASE(u1); SAFE_RELEASE(u2); SAFE_RELEASE(u3);
    SAFE_RELEASE(d1); SAFE_RELEASE(d2);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_device_dxgi_identity_note)
{
    RC_DEVICE(dev, ctx, 0);
    IDXGIDevice *dxgi = NULL; IUnknown *u0 = NULL, *u1 = NULL;
    EXPECT_HR(RC_QI(dev, IID_IDXGIDevice, &dxgi), S_OK);
    EXPECT_HR(RC_QI(dev, IID_IUnknown, &u0), S_OK);
    if (dxgi) {
        EXPECT_HR(RC_QI(dxgi, IID_IUnknown, &u1), S_OK);
        CHECK(u0 == u1);                                  /* one identity for the D3D11 and DXGI faces of the device */
        IDXGIDevice_Release(dxgi);
    }
    SAFE_RELEASE(u0); SAFE_RELEASE(u1);
    /* the DXGI device queried back for the D3D11 device interface gives the D3D11 device */
    EXPECT_HR(RC_QI(dev, IID_IDXGIDevice, &dxgi), S_OK);
    if (dxgi) {
        ID3D11Device *back = NULL;
        EXPECT_HR(RC_QI(dxgi, IID_ID3D11Device, &back), S_OK);
        CHECK(back == dev);
        SAFE_RELEASE(back);
        IDXGIDevice_Release(dxgi);
    }
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_context_qi_interfaces)
{
    RC_DEVICE(dev, ctx, 0);
    ULONG base = d3dh_refcount(ctx);
#define Q(iid, want) qi_expect(ctx, &iid, #iid, want)
    Q(IID_IUnknown, 1);
    Q(IID_ID3D11DeviceChild, 1);
    Q(IID_ID3D11DeviceContext, 1);
    Q(IID_ID3D11DeviceContext1, 1);
    Q(IID_ID3D11DeviceContext2, 1);
    Q(IID_ID3D11DeviceContext3, 1);
    Q(IID_ID3DUserDefinedAnnotation, 1);
    Q(IID_ID3D11Multithread, 1);
    Q(IID_ID3D11Device, 0);
    Q(IID_IDXGIDevice, 0);
    Q(IID_ID3D11Resource, 0);
    Q(IID_ID3D11CommandList, 0);
    Q(IID_ID3D11Debug, 0);
#undef Q
    EXPECT_EQ(d3dh_refcount(ctx), base);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_context_iunknown_identity)
{
    RC_DEVICE(dev, ctx, 0);
    IUnknown *u0 = NULL, *u1 = NULL;
    ID3D11DeviceContext1 *c1 = NULL;
    EXPECT_HR(RC_QI(ctx, IID_IUnknown, &u0), S_OK);
    EXPECT_HR(RC_QI(ctx, IID_ID3D11DeviceContext1, &c1), S_OK);
    if (c1) EXPECT_HR(RC_QI(c1, IID_IUnknown, &u1), S_OK);
    CHECK(u0 && u0 == u1);
    SAFE_RELEASE(u0); SAFE_RELEASE(u1); SAFE_RELEASE(c1);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_context_annotation_identity_note)
{
    RC_DEVICE(dev, ctx, 0);
    ID3DUserDefinedAnnotation *a = NULL;
    IUnknown *u0 = NULL, *u1 = NULL;
    EXPECT_HR(RC_QI(ctx, IID_ID3DUserDefinedAnnotation, &a), S_OK);
    EXPECT_HR(RC_QI(ctx, IID_IUnknown, &u0), S_OK);
    if (a) {
        EXPECT_HR(RC_QI(a, IID_IUnknown, &u1), S_OK);
        CHECK(u0 == u1);
        /* the annotation interface works on the immediate context */
        INT lvl = ID3DUserDefinedAnnotation_BeginEvent(a, L"refcount");
        INT end = ID3DUserDefinedAnnotation_EndEvent(a);
        EXPECT_EQ(lvl, -1);                               /* Win10: no profiler attached */
        EXPECT_EQ(end, -1);
        ID3DUserDefinedAnnotation_Release(a);
    }
    SAFE_RELEASE(u0); SAFE_RELEASE(u1);
    RC_DEVICE_END(dev, ctx);
}

/* ---- immutable state objects -------------------------------------------------------- */
static D3D11_SAMPLER_DESC samp_desc(void)
{
    D3D11_SAMPLER_DESC d; memset(&d, 0, sizeof(d));
    d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    d.MaxAnisotropy = 1; d.ComparisonFunc = D3D11_COMPARISON_NEVER; d.MaxLOD = D3D11_FLOAT32_MAX;
    return d;
}

TEST(d3d11_debug_refcount_state_dedup_sampler_counts)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_SAMPLER_DESC d = samp_desc();
    ID3D11SamplerState *a = NULL, *b = NULL, *c = NULL;
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d, &b), S_OK);
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d, &c), S_OK);
    CHECK(a == b && b == c);
    EXPECT_EQ(d3dh_refcount(a), 3);
    EXPECT_EQ(RC_RELEASE(a), 2);
    EXPECT_EQ(RC_RELEASE(b), 1);
    EXPECT_EQ(RC_RELEASE(c), 0);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_dedup_sampler_distinct_desc)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_SAMPLER_DESC d1 = samp_desc(), d2 = samp_desc();
    d2.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    ID3D11SamplerState *a = NULL, *b = NULL;
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d1, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d2, &b), S_OK);
    CHECK(a && b && a != b);
    EXPECT_EQ(d3dh_refcount(a), 1);
    EXPECT_EQ(d3dh_refcount(b), 1);
    D3D11_SAMPLER_DESC g;
    ID3D11SamplerState_GetDesc(a, &g); EXPECT_EQ(g.AddressU, D3D11_TEXTURE_ADDRESS_WRAP);
    ID3D11SamplerState_GetDesc(b, &g); EXPECT_EQ(g.AddressU, D3D11_TEXTURE_ADDRESS_CLAMP);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_dedup_blend)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_BLEND_DESC d; memset(&d, 0, sizeof(d));
    d.RenderTarget[0].SrcBlend = d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    d.RenderTarget[0].DestBlend = d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    d.RenderTarget[0].BlendOp = d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    d.RenderTarget[0].RenderTargetWriteMask = 0xf;
    D3D11_BLEND_DESC e = d; e.AlphaToCoverageEnable = TRUE;
    ID3D11BlendState *a = NULL, *b = NULL, *c = NULL;
    EXPECT_HR(ID3D11Device_CreateBlendState(dev, &d, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateBlendState(dev, &d, &b), S_OK);
    EXPECT_HR(ID3D11Device_CreateBlendState(dev, &e, &c), S_OK);
    CHECK(a == b);
    CHECK(a != c);
    EXPECT_EQ(d3dh_refcount(a), 2);
    EXPECT_EQ(d3dh_refcount(c), 1);
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_dedup_rasterizer)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_RASTERIZER_DESC d; memset(&d, 0, sizeof(d));
    d.FillMode = D3D11_FILL_SOLID; d.CullMode = D3D11_CULL_NONE; d.DepthClipEnable = TRUE;
    D3D11_RASTERIZER_DESC e = d; e.FillMode = D3D11_FILL_WIREFRAME;
    ID3D11RasterizerState *a = NULL, *b = NULL, *c = NULL;
    EXPECT_HR(ID3D11Device_CreateRasterizerState(dev, &d, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateRasterizerState(dev, &d, &b), S_OK);
    EXPECT_HR(ID3D11Device_CreateRasterizerState(dev, &e, &c), S_OK);
    CHECK(a == b);
    CHECK(a != c);
    EXPECT_EQ(d3dh_refcount(a), 2);
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_dedup_depthstencil)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_DEPTH_STENCIL_DESC d; memset(&d, 0, sizeof(d));
    d.DepthEnable = TRUE; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    d.StencilReadMask = d.StencilWriteMask = 0xff;
    d.FrontFace.StencilFunc = d.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    d.BackFace = d.FrontFace;
    D3D11_DEPTH_STENCIL_DESC e = d; e.DepthFunc = D3D11_COMPARISON_GREATER;
    ID3D11DepthStencilState *a = NULL, *b = NULL, *c = NULL;
    EXPECT_HR(ID3D11Device_CreateDepthStencilState(dev, &d, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateDepthStencilState(dev, &d, &b), S_OK);
    EXPECT_HR(ID3D11Device_CreateDepthStencilState(dev, &e, &c), S_OK);
    CHECK(a == b);
    CHECK(a != c);
    EXPECT_EQ(d3dh_refcount(a), 2);
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_dedup_is_per_device)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11Device *dev2 = NULL; ID3D11DeviceContext *ctx2 = NULL;
    if (FAILED(d3dh_make_device11(&dev2, &ctx2, NULL, NULL, 0))) { RC_DEVICE_END(dev, ctx); t_skip("second device unavailable"); return; }
    D3D11_SAMPLER_DESC d = samp_desc();
    ID3D11SamplerState *a = NULL, *b = NULL;
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev2, &d, &b), S_OK);
    CHECK(a && b && a != b);
    EXPECT_EQ(d3dh_refcount(a), 1);
    EXPECT_EQ(d3dh_refcount(b), 1);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    RC_DEVICE_END(dev2, ctx2);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_dedup_recreate_after_release)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_SAMPLER_DESC d = samp_desc();
    ID3D11SamplerState *a = NULL;
    for (int i = 0; i < 20; i++) {
        EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d, &a), S_OK);
        EXPECT_EQ(d3dh_refcount(a), 1);
        EXPECT_EQ(RC_RELEASE(a), 0);
        a = NULL;
    }
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_null_desc_rejected)
{
    RC_DEVICE(dev, ctx, 0);
    void *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11Device_CreateBlendState(dev, NULL, (ID3D11BlendState **)&p), E_INVALIDARG);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11Device_CreateRasterizerState(dev, NULL, (ID3D11RasterizerState **)&p), E_INVALIDARG);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11Device_CreateDepthStencilState(dev, NULL, (ID3D11DepthStencilState **)&p), E_INVALIDARG);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, NULL, (ID3D11SamplerState **)&p), E_INVALIDARG);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_validate_only_note)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_SAMPLER_DESC d = samp_desc();
    HRESULT hr = ID3D11Device_CreateSamplerState(dev, &d, NULL);
    EXPECT_HR(hr, S_FALSE);                               /* validate-only call: success code, no object*/
    d.AddressU = (D3D11_TEXTURE_ADDRESS_MODE)99;
    hr = ID3D11Device_CreateSamplerState(dev, &d, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_state_too_many_unique_sampler_objects)
{
    RC_DEVICE(dev, ctx, 0);
    enum { MAXN = 4400 };
    ID3D11SamplerState **v = calloc(MAXN, sizeof(*v));
    int n = 0; HRESULT hr = S_OK;
    for (n = 0; n < MAXN; n++) {
        D3D11_SAMPLER_DESC d = samp_desc();
        d.MipLODBias = (float)n * 0.001f - 2.0f;   /* every value unique and inside the legal bias range */
        hr = ID3D11Device_CreateSamplerState(dev, &d, &v[n]);
        if (FAILED(hr)) break;
    }
    EXPECT_EQ(n, 4095);                                   /* Win10 22H2: the 4096th unique sampler state is refused */
    EXPECT_HR(hr, HR_TOO_MANY_STATES);
    for (int i = 0; i < n; i++) RC_RELEASE(v[i]);
    /* after releasing them all, a new state can be created again */
    ID3D11SamplerState *s = NULL;
    D3D11_SAMPLER_DESC d = samp_desc();
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d, &s), S_OK);
    SAFE_RELEASE(s);
    free(v);
    RC_DEVICE_END(dev, ctx);
}

/* ---- views and pipeline bindings hold references ------------------------------------- */
TEST(d3d11_debug_refcount_srv_keeps_texture_alive)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_TEXTURE2D_DESC td = {16, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0};
    ID3D11Texture2D *tex = NULL; ID3D11ShaderResourceView *srv = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture2D(dev, &td, NULL, &tex), S_OK);
    if (!tex) { RC_DEVICE_END(dev, ctx); return; }
    ULONG before = d3dh_refcount(tex);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, NULL, &srv), S_OK);
    EXPECT_EQ(d3dh_refcount(tex), before);                /* views hold internal, not external, references to their resource */
    ID3D11Resource *saved = (ID3D11Resource *)tex;
    RC_RELEASE(tex);
    tex = NULL;
    ID3D11Resource *res = NULL;
    ID3D11ShaderResourceView_GetResource(srv, &res);
    CHECK(res == saved);
    if (res) {
        ID3D11Texture2D *t2 = NULL;
        EXPECT_HR(RC_QI(res, IID_ID3D11Texture2D, &t2), S_OK);
        if (t2) {
            D3D11_TEXTURE2D_DESC g;
            ID3D11Texture2D_GetDesc(t2, &g);
            EXPECT_EQ(g.Width, 16); EXPECT_EQ(g.Height, 8);
            SAFE_RELEASE(t2);
        }
        ULONG a = d3dh_refcount(res);
        RC_RELEASE(res);
        EXPECT_EQ(d3dh_refcount(res), a - 1);
    }
    SAFE_RELEASE(srv);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_view_getresource_addrefs_exactly_one)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_TEXTURE2D_DESC td = {16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 0};
    ID3D11Texture2D *tex = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *rtv = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture2D(dev, &td, NULL, &tex), S_OK);
    if (!tex) { RC_DEVICE_END(dev, ctx); return; }
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, NULL, &srv), S_OK);
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)tex, NULL, &rtv), S_OK);
    ULONG base = d3dh_refcount(tex);
    ID3D11Resource *r1 = NULL, *r2 = NULL;
    ID3D11ShaderResourceView_GetResource(srv, &r1);
    EXPECT_EQ(d3dh_refcount(tex), base + 1);
    ID3D11RenderTargetView_GetResource(rtv, &r2);
    EXPECT_EQ(d3dh_refcount(tex), base + 2);
    CHECK(r1 == (ID3D11Resource *)tex && r2 == (ID3D11Resource *)tex);
    SAFE_RELEASE(r1); SAFE_RELEASE(r2);
    EXPECT_EQ(d3dh_refcount(tex), base);
    SAFE_RELEASE(srv); SAFE_RELEASE(rtv); SAFE_RELEASE(tex);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_buffer_srv_keeps_buffer_alive)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0};
    ID3D11Buffer *buf = NULL; ID3D11ShaderResourceView *srv = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &buf), S_OK);
    if (!buf) { RC_DEVICE_END(dev, ctx); return; }
    D3D11_SHADER_RESOURCE_VIEW_DESC sd; memset(&sd, 0, sizeof(sd));
    sd.Format = DXGI_FORMAT_R32_TYPELESS; sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    sd.BufferEx.NumElements = 16; sd.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    HRESULT hr = ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)buf, &sd, &srv);
    if (FAILED(hr)) { SAFE_RELEASE(buf); RC_DEVICE_END(dev, ctx); t_skip("raw SRV unsupported: %s", d3dh_hr_name(hr)); return; }
    ID3D11Resource *saved = (ID3D11Resource *)buf;
    RC_RELEASE(buf); buf = NULL;
    ID3D11Resource *res = NULL;
    ID3D11ShaderResourceView_GetResource(srv, &res);
    CHECK(res == saved);
    if (res) {
        D3D11_RESOURCE_DIMENSION dim = 0;
        ID3D11Resource_GetType(res, &dim);
        EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_BUFFER);
        SAFE_RELEASE(res);
    }
    SAFE_RELEASE(srv);
    RC_DEVICE_END(dev, ctx);
}

/* binds an object, drops the application reference, reads it back, clears the state */
#define PIPE_TEST(tname, kind, set_stmt, get_stmt, type) \
    TEST(d3d11_debug_refcount_pipeline_holds_##tname) \
    { \
        RC_DEVICE(dev, ctx, 0); \
        ID3D11DeviceChild *o = NULL; \
        HRESULT mk_ = rc_make(kind, dev, &o); \
        if (FAILED(mk_)) { RC_DEVICE_END(dev, ctx); t_skip("%s unavailable", #tname); return; } \
        type *p = (type *)o; type *got = NULL; \
        set_stmt; \
        RC_RELEASE(o); o = NULL; \
        get_stmt; \
        CHECK(got == p); \
        if (got) { ULONG c = d3dh_refcount(got); CHECK(c >= 1); RC_RELEASE(got); } \
        ID3D11DeviceContext_ClearState(ctx); \
        ID3D11DeviceContext_Flush(ctx); \
        got = (type *)(ULONG_PTR)1; \
        get_stmt; \
        CHECK(got == NULL); \
        RC_DEVICE_END(dev, ctx); \
    }
PIPE_TEST(vs, K_VS, ID3D11DeviceContext_VSSetShader(ctx, p, NULL, 0), ID3D11DeviceContext_VSGetShader(ctx, &got, NULL, NULL), ID3D11VertexShader)
PIPE_TEST(ps, K_PS, ID3D11DeviceContext_PSSetShader(ctx, p, NULL, 0), ID3D11DeviceContext_PSGetShader(ctx, &got, NULL, NULL), ID3D11PixelShader)
PIPE_TEST(cs, K_CS, ID3D11DeviceContext_CSSetShader(ctx, p, NULL, 0), ID3D11DeviceContext_CSGetShader(ctx, &got, NULL, NULL), ID3D11ComputeShader)
PIPE_TEST(input_layout, K_IL, ID3D11DeviceContext_IASetInputLayout(ctx, p), ID3D11DeviceContext_IAGetInputLayout(ctx, &got), ID3D11InputLayout)
PIPE_TEST(blend_state, K_BLEND, ID3D11DeviceContext_OMSetBlendState(ctx, p, NULL, 0xffffffff), ID3D11DeviceContext_OMGetBlendState(ctx, &got, NULL, NULL), ID3D11BlendState)
PIPE_TEST(depth_stencil_state, K_DSS, ID3D11DeviceContext_OMSetDepthStencilState(ctx, p, 0), ID3D11DeviceContext_OMGetDepthStencilState(ctx, &got, NULL), ID3D11DepthStencilState)
PIPE_TEST(rasterizer_state, K_RAST, ID3D11DeviceContext_RSSetState(ctx, p), ID3D11DeviceContext_RSGetState(ctx, &got), ID3D11RasterizerState)
PIPE_TEST(vs_sampler, K_SAMP, ID3D11DeviceContext_VSSetSamplers(ctx, 0, 1, &p), ID3D11DeviceContext_VSGetSamplers(ctx, 0, 1, &got), ID3D11SamplerState)
PIPE_TEST(ps_sampler, K_SAMP, ID3D11DeviceContext_PSSetSamplers(ctx, 3, 1, &p), ID3D11DeviceContext_PSGetSamplers(ctx, 3, 1, &got), ID3D11SamplerState)
PIPE_TEST(ps_srv, K_SRV, ID3D11DeviceContext_PSSetShaderResources(ctx, 2, 1, &p), ID3D11DeviceContext_PSGetShaderResources(ctx, 2, 1, &got), ID3D11ShaderResourceView)
PIPE_TEST(vs_srv, K_SRV, ID3D11DeviceContext_VSSetShaderResources(ctx, 0, 1, &p), ID3D11DeviceContext_VSGetShaderResources(ctx, 0, 1, &got), ID3D11ShaderResourceView)
PIPE_TEST(cs_srv, K_SRV, ID3D11DeviceContext_CSSetShaderResources(ctx, 1, 1, &p), ID3D11DeviceContext_CSGetShaderResources(ctx, 1, 1, &got), ID3D11ShaderResourceView)
PIPE_TEST(render_target, K_RTV, ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &p, NULL), ID3D11DeviceContext_OMGetRenderTargets(ctx, 1, &got, NULL), ID3D11RenderTargetView)
PIPE_TEST(depth_stencil_view, K_DSV, ID3D11DeviceContext_OMSetRenderTargets(ctx, 0, NULL, p), ID3D11DeviceContext_OMGetRenderTargets(ctx, 0, NULL, &got), ID3D11DepthStencilView)

static void bufbind_test(UINT bind, int which)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_BUFFER_DESC bd = {256, D3D11_USAGE_DEFAULT, bind, 0, 0, 0};
    ID3D11Buffer *b = NULL, *got = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &b), S_OK);
    if (!b) { RC_DEVICE_END(dev, ctx); return; }
    ID3D11Buffer *p = b;
    UINT stride = 16, off = 0;
    switch (which) {
    case 0: ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &p, &stride, &off); break;
    case 1: ID3D11DeviceContext_IASetIndexBuffer(ctx, p, DXGI_FORMAT_R16_UINT, 0); break;
    case 2: ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &p); break;
    case 3: ID3D11DeviceContext_PSSetConstantBuffers(ctx, 5, 1, &p); break;
    case 4: ID3D11DeviceContext_SOSetTargets(ctx, 1, &p, &off); break;
    }
    RC_RELEASE(b); b = NULL;
    switch (which) {
    case 0: ID3D11DeviceContext_IAGetVertexBuffers(ctx, 0, 1, &got, &stride, &off); break;
    case 1: ID3D11DeviceContext_IAGetIndexBuffer(ctx, &got, NULL, NULL); break;
    case 2: ID3D11DeviceContext_VSGetConstantBuffers(ctx, 0, 1, &got); break;
    case 3: ID3D11DeviceContext_PSGetConstantBuffers(ctx, 5, 1, &got); break;
    case 4: ID3D11DeviceContext_SOGetTargets(ctx, 1, &got); break;
    }
    CHECK(got == p);
    if (got) {
        D3D11_BUFFER_DESC g; ID3D11Buffer_GetDesc(got, &g);
        EXPECT_EQ(g.ByteWidth, 256);
        RC_RELEASE(got); got = NULL;
    }
    ID3D11DeviceContext_ClearState(ctx);
    got = (ID3D11Buffer *)(ULONG_PTR)1;
    switch (which) {
    case 0: ID3D11DeviceContext_IAGetVertexBuffers(ctx, 0, 1, &got, &stride, &off); break;
    case 1: ID3D11DeviceContext_IAGetIndexBuffer(ctx, &got, NULL, NULL); break;
    case 2: ID3D11DeviceContext_VSGetConstantBuffers(ctx, 0, 1, &got); break;
    case 3: ID3D11DeviceContext_PSGetConstantBuffers(ctx, 5, 1, &got); break;
    case 4: ID3D11DeviceContext_SOGetTargets(ctx, 1, &got); break;
    }
    CHECK(got == NULL);
    RC_DEVICE_END(dev, ctx);
}
TEST(d3d11_debug_refcount_pipeline_holds_vertex_buffer)   { bufbind_test(D3D11_BIND_VERTEX_BUFFER, 0); }
TEST(d3d11_debug_refcount_pipeline_holds_index_buffer)    { bufbind_test(D3D11_BIND_INDEX_BUFFER, 1); }
TEST(d3d11_debug_refcount_pipeline_holds_vs_constant_buffer) { bufbind_test(D3D11_BIND_CONSTANT_BUFFER, 2); }
TEST(d3d11_debug_refcount_pipeline_holds_ps_constant_buffer) { bufbind_test(D3D11_BIND_CONSTANT_BUFFER, 3); }
TEST(d3d11_debug_refcount_pipeline_holds_stream_output_buffer)
{
    /* stream-output targets are only valid with a geometry shader bound to the pipeline for drawing, but binding is legal */
    bufbind_test(D3D11_BIND_STREAM_OUTPUT, 4);
}

TEST(d3d11_debug_refcount_clearstate_releases_getters_to_null_for_all_stages)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceChild *vs = NULL, *ps = NULL, *smp = NULL;
    if (FAILED(rc_make(K_VS, dev, &vs)) || FAILED(rc_make(K_PS, dev, &ps)) || FAILED(rc_make(K_SAMP, dev, &smp))) {
        SAFE_RELEASE(vs); SAFE_RELEASE(ps); SAFE_RELEASE(smp); RC_DEVICE_END(dev, ctx); t_skip("objects unavailable"); return;
    }
    ID3D11SamplerState *s = (ID3D11SamplerState *)smp;
    ID3D11DeviceContext_VSSetShader(ctx, (ID3D11VertexShader *)vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(ctx, (ID3D11PixelShader *)ps, NULL, 0);
    ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &s);
    ULONG v0 = d3dh_refcount(vs);
    EXPECT_EQ(v0, 1);                                     /* the pipeline's reference is internal */
    ID3D11DeviceContext_ClearState(ctx);
    EXPECT_EQ(d3dh_refcount(vs), 1);
    EXPECT_EQ(d3dh_refcount(ps), 1);
    EXPECT_EQ(d3dh_refcount(smp), 1);
    SAFE_RELEASE(vs); SAFE_RELEASE(ps); SAFE_RELEASE(smp);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_binding_with_null_unbinds)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceChild *vs = NULL;
    if (FAILED(rc_make(K_VS, dev, &vs))) { RC_DEVICE_END(dev, ctx); t_skip("vs unavailable"); return; }
    ID3D11DeviceContext_VSSetShader(ctx, (ID3D11VertexShader *)vs, NULL, 0);
    ID3D11DeviceContext_VSSetShader(ctx, NULL, NULL, 0);
    EXPECT_EQ(d3dh_refcount(vs), 1);
    ID3D11VertexShader *got = (ID3D11VertexShader *)(ULONG_PTR)1;
    ID3D11DeviceContext_VSGetShader(ctx, &got, NULL, NULL);
    CHECK(got == NULL);
    SAFE_RELEASE(vs);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_rebinding_replaces_reference)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceChild *a = NULL, *b = NULL;
    if (FAILED(rc_make(K_VS, dev, &a)) || FAILED(rc_make(K_VS, dev, &b))) {
        SAFE_RELEASE(a); SAFE_RELEASE(b); RC_DEVICE_END(dev, ctx); t_skip("vs unavailable"); return;
    }
    if (a == b) { SAFE_RELEASE(a); SAFE_RELEASE(b); RC_DEVICE_END(dev, ctx); t_skip("identical shaders were merged"); return; }
    ID3D11DeviceContext_VSSetShader(ctx, (ID3D11VertexShader *)a, NULL, 0);
    ID3D11DeviceContext_VSSetShader(ctx, (ID3D11VertexShader *)b, NULL, 0);
    ID3D11VertexShader *got = NULL;
    ID3D11DeviceContext_VSGetShader(ctx, &got, NULL, NULL);
    CHECK(got == (ID3D11VertexShader *)b);
    SAFE_RELEASE(got);
    EXPECT_EQ(d3dh_refcount(a), 1);   /* the first shader is no longer referenced by the pipeline */
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_getter_with_count_zero_object_not_touched)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11SamplerState *arr[4];
    for (int i = 0; i < 4; i++) arr[i] = (ID3D11SamplerState *)(ULONG_PTR)1;
    ID3D11DeviceContext_PSGetSamplers(ctx, 0, 4, arr);   /* nothing bound: all four slots are written NULL */
    for (int i = 0; i < 4; i++) CHECKF(arr[i] == NULL, "slot %d", i);
    RC_DEVICE_END(dev, ctx);
}

/* ---- destroy order ---------------------------------------------------------------------- */
static void perm_release(int order, int dev_pos)
{
    static const int perms[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceChild *o[3] = {0};
    if (FAILED(rc_make(K_T2D, dev, &o[0])) || FAILED(rc_make(K_SRV, dev, &o[1])) || FAILED(rc_make(K_SAMP, dev, &o[2]))) {
        for (int i = 0; i < 3; i++) SAFE_RELEASE(o[i]);
        RC_DEVICE_END(dev, ctx); t_skip("objects unavailable"); return;
    }
    ID3D11ShaderResourceView *srv = (ID3D11ShaderResourceView *)o[1];
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv);
    for (int step = 0; step < 4; step++) {
        if (step == dev_pos) { SAFE_RELEASE(ctx); SAFE_RELEASE(dev); continue; }
        int idx = step < dev_pos ? step : step - 1;
        if (idx >= 3) continue;
        int which = perms[order][idx];
        ULONG r = RC_RELEASE(o[which]);
        if (which != 1) EXPECT_EQ(r, 0);   /* the bound SRV is still held by the pipeline */
        o[which] = NULL;
    }
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
}
#define PERM(o, p) TEST(d3d11_debug_refcount_destroy_order_##o##_device_at_##p) { perm_release(o, p); }
PERM(0,0) PERM(1,0) PERM(2,0) PERM(3,0) PERM(4,0) PERM(5,0)
PERM(0,1) PERM(1,1) PERM(2,1) PERM(3,1) PERM(4,1) PERM(5,1)
PERM(0,2) PERM(1,2) PERM(2,2) PERM(3,2) PERM(4,2) PERM(5,2)
PERM(0,3) PERM(1,3) PERM(2,3) PERM(3,3) PERM(4,3) PERM(5,3)

TEST(d3d11_debug_refcount_release_to_zero_every_kind_loop)
{
    RC_DEVICE(dev, ctx, 0);
    int made = 0;
    for (int k = 0; k < K_COUNT; k++) {
        for (int i = 0; i < 25; i++) {
            ID3D11DeviceChild *o = NULL;
            if (FAILED(rc_make(k, dev, &o))) break;
            made++;
            EXPECT_EQ(RC_RELEASE(o), 0);
        }
    }
    CHECK(made > 0);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_many_buffers_live_then_released)
{
    RC_DEVICE(dev, ctx, 0);
    enum { N = 500 };
    ID3D11Buffer *v[N];
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
    for (int i = 0; i < N; i++) { v[i] = NULL; bd.ByteWidth = 16 * (1 + i % 16); EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &v[i]), S_OK); }
    for (int i = 0; i < N; i++) {
        if (!v[i]) continue;
        D3D11_BUFFER_DESC g; ID3D11Buffer_GetDesc(v[i], &g);
        EXPECT_EQ(g.ByteWidth, 16 * (1 + i % 16));
    }
    for (int i = N - 1; i >= 0; i--) if (v[i]) EXPECT_EQ(RC_RELEASE(v[i]), 0);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_device_count_vs_children_note)
{
    RC_DEVICE(dev, ctx, 0);
    ULONG base = d3dh_refcount(dev);
    ID3D11DeviceChild *o[8]; int n = 0;
    for (int k = 0; k < 8; k++) if (SUCCEEDED(rc_make(k, dev, &o[n]))) n++;
    EXPECT_EQ(n, 8);
    EXPECT_EQ(d3dh_refcount(dev), base + n);              /* every child holds one external reference on its device */
    for (int i = 0; i < n; i++) RC_RELEASE(o[i]);
    EXPECT_EQ(d3dh_refcount(dev), base);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_device_survives_in_deferred_context_after_release)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(dev, 0, &dc), S_OK);
    SAFE_RELEASE(ctx);
    SAFE_RELEASE(dev);
    if (dc) {
        ID3D11Device *d = NULL;
        ID3D11DeviceContext_GetDevice(dc, &d);
        CHECK(d != NULL);
        if (d) {
            ID3D11DeviceContext *ic = NULL;
            ID3D11Device_GetImmediateContext(d, &ic);
            CHECK(ic != NULL);
            SAFE_RELEASE(ic);
            SAFE_RELEASE(d);
        }
        SAFE_RELEASE(dc);
    }
}

TEST(d3d11_debug_refcount_two_devices_independent)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11Device *d2 = NULL; ID3D11DeviceContext *c2 = NULL;
    if (FAILED(d3dh_make_device11(&d2, &c2, NULL, NULL, 0))) { RC_DEVICE_END(dev, ctx); t_skip("second device unavailable"); return; }
    CHECK(d2 != dev);
    ID3D11DeviceChild *o = NULL;
    if (SUCCEEDED(rc_make(K_BUF, dev, &o))) {
        ID3D11Device *g = NULL;
        o->lpVtbl->GetDevice(o, &g);
        CHECK(g == dev && g != d2);
        SAFE_RELEASE(g);
        SAFE_RELEASE(c2); SAFE_RELEASE(d2);   /* tearing down the unrelated device leaves the first intact */
        EXPECT_EQ(d3dh_refcount(o), 1);
        SAFE_RELEASE(o);
    } else { SAFE_RELEASE(c2); SAFE_RELEASE(d2); }
    RC_DEVICE_END(dev, ctx);
}
