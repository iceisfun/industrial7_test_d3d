/* Table-driven per-object-kind identity / refcount / private data tests. */
#include "rc.h"

#define KIND_FIX(K) \
    RC_DEVICE(dev, ctx, 0); \
    ID3D11DeviceChild *obj = NULL; \
    { HRESULT mk_ = rc_make(K, dev, &obj); \
      if (FAILED(mk_)) { t_skip("kind %d unavailable on this device (%s)", K, d3dh_hr_name(mk_)); RC_DEVICE_END(dev, ctx); return; } }
#define KIND_END() do { SAFE_RELEASE(obj); RC_DEVICE_END(dev, ctx); } while (0)

/* ---- the interface matrix ------------------------------------------------------- */
enum { I_UNK, I_CHILD, I_RES, I_BUF, I_T1, I_T2, I_T3, I_VIEW, I_SRV, I_RTV, I_DSV, I_UAV, I_BS, I_RS, I_DSS, I_SS,
       I_VS, I_PS, I_CS, I_IL, I_ASYNC, I_QUERY, I_PRED, I_CLINK, I_CTX, I_CL, I_COUNT };
static const IID *const iid_tab[I_COUNT] = {
    &IID_IUnknown, &IID_ID3D11DeviceChild, &IID_ID3D11Resource, &IID_ID3D11Buffer, &IID_ID3D11Texture1D,
    &IID_ID3D11Texture2D, &IID_ID3D11Texture3D, &IID_ID3D11View, &IID_ID3D11ShaderResourceView,
    &IID_ID3D11RenderTargetView, &IID_ID3D11DepthStencilView, &IID_ID3D11UnorderedAccessView,
    &IID_ID3D11BlendState, &IID_ID3D11RasterizerState, &IID_ID3D11DepthStencilState, &IID_ID3D11SamplerState,
    &IID_ID3D11VertexShader, &IID_ID3D11PixelShader, &IID_ID3D11ComputeShader, &IID_ID3D11InputLayout,
    &IID_ID3D11Asynchronous, &IID_ID3D11Query, &IID_ID3D11Predicate, &IID_ID3D11ClassLinkage,
    &IID_ID3D11DeviceContext, &IID_ID3D11CommandList };
static const char *const iid_names[I_COUNT] = {
    "IUnknown", "DeviceChild", "Resource", "Buffer", "Texture1D", "Texture2D", "Texture3D", "View", "SRV", "RTV", "DSV", "UAV",
    "BlendState", "RasterizerState", "DepthStencilState", "SamplerState", "VertexShader", "PixelShader", "ComputeShader",
    "InputLayout", "Asynchronous", "Query", "Predicate", "ClassLinkage", "DeviceContext", "CommandList" };
#define M(i) (1u << (i))
static unsigned kind_mask(int k)
{
    unsigned base = M(I_UNK) | M(I_CHILD);
    switch (k) {
    case K_BUF: return base | M(I_RES) | M(I_BUF);
    case K_T1D: return base | M(I_RES) | M(I_T1);
    case K_T2D: return base | M(I_RES) | M(I_T2);
    case K_T3D: return base | M(I_RES) | M(I_T3);
    case K_SRV: return base | M(I_VIEW) | M(I_SRV);
    case K_RTV: return base | M(I_VIEW) | M(I_RTV);
    case K_DSV: return base | M(I_VIEW) | M(I_DSV);
    case K_UAV: return base | M(I_VIEW) | M(I_UAV);
    case K_BLEND: return base | M(I_BS);
    case K_RAST: return base | M(I_RS);
    case K_DSS: return base | M(I_DSS);
    case K_SAMP: return base | M(I_SS);
    case K_VS: return base | M(I_VS);
    case K_PS: return base | M(I_PS);
    case K_CS: return base | M(I_CS);
    case K_IL: return base | M(I_IL);
    case K_QUERY: return base | M(I_ASYNC) | M(I_QUERY);
    case K_PRED: return base | M(I_ASYNC) | M(I_QUERY) | M(I_PRED);
    case K_CLINK: return base | M(I_CLINK);
    case K_DEFCTX: return base | M(I_CTX);
    case K_CMDLIST: return base | M(I_CL);
    }
    return base;
}

static void k_matrix(int K)
{
    KIND_FIX(K);
    unsigned want = kind_mask(K);
    for (int i = 0; i < I_COUNT; i++) {
        void *p = (void *)(ULONG_PTR)1;
        ULONG before = d3dh_refcount(obj);
        HRESULT hr = obj->lpVtbl->QueryInterface(obj, iid_tab[i], &p);
        if (want & M(i)) {
            CHECKF(hr == S_OK && p, "QI %s: hr=%s", iid_names[i], d3dh_hr_name(hr));
            if (SUCCEEDED(hr) && p) {
                CHECKF(d3dh_refcount(obj) == before + 1, "QI %s must add exactly one reference", iid_names[i]);
                RC_RELEASE(p);
            }
        } else {
            CHECKF(hr == E_NOINTERFACE, "QI %s: hr=%s want E_NOINTERFACE", iid_names[i], d3dh_hr_name(hr));
            CHECKF(p == NULL, "QI %s failed but *ppv not NULLed", iid_names[i]);
            if (SUCCEEDED(hr) && p) RC_RELEASE(p);
        }
        CHECKF(d3dh_refcount(obj) == before, "QI %s leaked a reference", iid_names[i]);
    }
    KIND_END();
}

static void k_initial(int K)
{
    KIND_FIX(K);
    EXPECT_EQ(RC_ADDREF(obj), 2);
    EXPECT_EQ(RC_ADDREF(obj), 3);
    EXPECT_EQ(RC_RELEASE(obj), 2);
    EXPECT_EQ(RC_RELEASE(obj), 1);
    EXPECT_EQ(RC_RELEASE(obj), 0);   /* the object is destroyed here; obj must not be touched again */
    obj = NULL;
    RC_DEVICE_END(dev, ctx);
}

static void k_chain(int K)
{
    KIND_FIX(K);
    for (int i = 0; i < 64; i++) EXPECT_EQ(RC_ADDREF(obj), (ULONG)(i + 2));
    for (int i = 63; i >= 0; i--) EXPECT_EQ(RC_RELEASE(obj), (ULONG)(i + 1));
    KIND_END();
}

static void k_identity(int K)
{
    KIND_FIX(K);
    IUnknown *u1 = NULL, *u2 = NULL, *u3 = NULL;
    ID3D11DeviceChild *c = NULL;
    EXPECT_HR(RC_QI(obj, IID_IUnknown, &u1), S_OK);
    EXPECT_HR(RC_QI(obj, IID_ID3D11DeviceChild, &c), S_OK);
    EXPECT_HR(RC_QI(c, IID_IUnknown, &u2), S_OK);
    EXPECT_HR(u1->lpVtbl->QueryInterface(u1, &IID_IUnknown, (void **)&u3), S_OK);
    CHECK(u1 && u1 == u2);
    CHECK(u1 && u1 == u3);
    if (c != obj) t_note("kind %d: QI(DeviceChild) returned a different pointer than the creation pointer", K);
    EXPECT_EQ(d3dh_refcount(obj), 5);   // 1 + u1 + c + u2 + u3
    SAFE_RELEASE(u1); SAFE_RELEASE(u2); SAFE_RELEASE(u3); SAFE_RELEASE(c);
    EXPECT_EQ(d3dh_refcount(obj), 1);
    KIND_END();
}

static void k_qi_junk(int K)
{
    KIND_FIX(K);
    static const GUID junk = {0x12345678, 0x9abc, 0xdef0, {1, 2, 3, 4, 5, 6, 7, 8}};
    void *p = (void *)(ULONG_PTR)0x10;
    HRESULT hr = obj->lpVtbl->QueryInterface(obj, &junk, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)0x10;
    hr = obj->lpVtbl->QueryInterface(obj, &GUID_NULL, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    /* a device interface is never exposed by a child object */
    p = (void *)(ULONG_PTR)0x10;
    hr = RC_QI(obj, IID_ID3D11Device, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)0x10;
    hr = RC_QI(obj, IID_IDXGISwapChain, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_EQ(d3dh_refcount(obj), 1);
    KIND_END();
}

static void k_getdevice(int K)
{
    KIND_FIX(K);
    ULONG base = d3dh_refcount(dev);
    ID3D11Device *d1 = NULL, *d2 = NULL;
    obj->lpVtbl->GetDevice(obj, &d1);
    CHECK(d1 == dev);
    EXPECT_EQ(d3dh_refcount(dev), base + 1);
    obj->lpVtbl->GetDevice(obj, &d2);
    CHECK(d2 == dev);
    EXPECT_EQ(d3dh_refcount(dev), base + 2);
    SAFE_RELEASE(d1);
    EXPECT_EQ(d3dh_refcount(dev), base + 1);
    SAFE_RELEASE(d2);
    EXPECT_EQ(d3dh_refcount(dev), base);
    KIND_END();
}

static void k_dev_first(int K)
{
    KIND_FIX(K);
    ID3D11Device *d = NULL;
    SAFE_RELEASE(ctx);
    SAFE_RELEASE(dev);   /* the application's last reference to the device is gone; the child still works */
    obj->lpVtbl->GetDevice(obj, &d);
    CHECK(d != NULL);
    if (d) {
        CHECK(ID3D11Device_GetFeatureLevel(d) >= D3D_FEATURE_LEVEL_9_1);
        ID3D11Query *q = NULL;
        D3D11_QUERY_DESC qd = { D3D11_QUERY_EVENT, 0 };
        EXPECT_HR(ID3D11Device_CreateQuery(d, &qd, &q), S_OK);
        SAFE_RELEASE(q);
    }
    EXPECT_EQ(RC_RELEASE(obj), 0);   /* the object is released first ... */
    obj = NULL;
    SAFE_RELEASE(d);                         /* ... then the device goes away */
}

static void k_dev_last(int K)
{
    KIND_FIX(K);
    ID3D11Device *d = NULL;
    obj->lpVtbl->GetDevice(obj, &d);
    SAFE_RELEASE(ctx);
    SAFE_RELEASE(dev);
    SAFE_RELEASE(d);          /* device is now kept alive only by the child */
    RC_RELEASE(obj);          /* last release tears everything down; must not crash */
    obj = NULL;
}

static void k_two(int K)
{
    KIND_FIX(K);
    ID3D11DeviceChild *o2 = NULL;
    HRESULT hr = rc_make(K, dev, &o2);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        if (rc_kind_is_state(K)) {
            /* identical immutable state objects are deduplicated: the same pointer comes back with one more reference */
            CHECK(o2 == obj);
            EXPECT_EQ(d3dh_refcount(obj), 2);
            EXPECT_EQ(RC_RELEASE(o2), 1);
        } else if (K == K_VS || K == K_PS || K == K_CS || K == K_IL) {
            if (o2 == obj) t_note("kind %d: identical creation returned the same object (refcount %lu)", K, d3dh_refcount(obj));
            else EXPECT_EQ(d3dh_refcount(obj), 1);
            RC_RELEASE(o2);
        } else {
            CHECK(o2 != obj);
            EXPECT_EQ(d3dh_refcount(obj), 1);
            EXPECT_EQ(d3dh_refcount(o2), 1);
            RC_RELEASE(o2);
        }
    }
    KIND_END();
}

static void k_privdata(int K)
{
    KIND_FIX(K);
    UINT sz; BYTE buf[64]; DWORD v = 0xcafe1234, w;
    /* nothing stored yet */
    sz = sizeof(buf);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, buf), HR_NOT_FOUND);
    sz = 0;
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, NULL), HR_NOT_FOUND);
    /* store, query size, read back */
    EXPECT_HR(obj->lpVtbl->SetPrivateData(obj, &RC_GUID_A, sizeof(v), &v), S_OK);
    sz = 0;
    HRESULT hr = obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, NULL);
    CHECKF(hr == S_OK || hr == HR_MORE_DATA, "size query: %s", d3dh_hr_name(hr));
    EXPECT_EQ(sz, sizeof(v));
    w = 0; sz = sizeof(w);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, &w), S_OK);
    EXPECT_EQ(sz, sizeof(v)); EXPECT_EQ(w, v);
    /* too-small buffer */
    w = 0; sz = 2;
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, &w), HR_MORE_DATA);
    EXPECT_EQ(sz, sizeof(v)); EXPECT_EQ(w, 0);
    /* oversize buffer reports the stored size */
    memset(buf, 0xee, sizeof(buf)); sz = sizeof(buf);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, buf), S_OK);
    EXPECT_EQ(sz, sizeof(v)); EXPECT_EQ(buf[4], 0xee);
    /* a different GUID is independent */
    sz = sizeof(buf);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_B, &sz, buf), HR_NOT_FOUND);
    /* replace with a different size */
    memset(buf, 0x5a, 24);
    EXPECT_HR(obj->lpVtbl->SetPrivateData(obj, &RC_GUID_A, 24, buf), S_OK);
    sz = 0;
    obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, NULL);
    EXPECT_EQ(sz, 24);
    /* delete: zero size and NULL data */
    EXPECT_HR(obj->lpVtbl->SetPrivateData(obj, &RC_GUID_A, 0, NULL), S_OK);
    sz = sizeof(buf);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, buf), HR_NOT_FOUND);
    /* deleting something that does not exist is not an error */
    t_note("kind %d: delete of absent private data -> %s", K, d3dh_hr_name(obj->lpVtbl->SetPrivateData(obj, &RC_GUID_C, 0, NULL)));
    KIND_END();
}

static void k_name(int K)
{
    KIND_FIX(K);
    static const char name[] = "refcount-test-object";
    UINT sz; char out[64];
    EXPECT_HR(obj->lpVtbl->SetPrivateData(obj, &WKPDID_D3DDebugObjectName, (UINT)strlen(name), name), S_OK);
    sz = sizeof(out); memset(out, 0, sizeof(out));
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &WKPDID_D3DDebugObjectName, &sz, out), S_OK);
    EXPECT_EQ(sz, strlen(name));
    CHECK(!memcmp(out, name, strlen(name)));
    /* renaming replaces the old name */
    EXPECT_HR(obj->lpVtbl->SetPrivateData(obj, &WKPDID_D3DDebugObjectName, 3, "abc"), S_OK);
    sz = sizeof(out);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &WKPDID_D3DDebugObjectName, &sz, out), S_OK);
    EXPECT_EQ(sz, 3);
    /* clearing */
    EXPECT_HR(obj->lpVtbl->SetPrivateData(obj, &WKPDID_D3DDebugObjectName, 0, NULL), S_OK);
    sz = sizeof(out);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &WKPDID_D3DDebugObjectName, &sz, out), HR_NOT_FOUND);
    KIND_END();
}

static void k_privif(int K)
{
    KIND_FIX(K);
    rc_obj *a = rc_obj_new(), *b = rc_obj_new();
    EXPECT_HR(obj->lpVtbl->SetPrivateDataInterface(obj, &RC_GUID_A, &a->iface), S_OK);
    EXPECT_EQ(rc_obj_refs(a), 2);                     /* the object owns one reference */
    /* same GUID again with the same pointer: net count unchanged */
    EXPECT_HR(obj->lpVtbl->SetPrivateDataInterface(obj, &RC_GUID_A, &a->iface), S_OK);
    EXPECT_EQ(rc_obj_refs(a), 2);
    /* replacing with another interface releases the previous one */
    EXPECT_HR(obj->lpVtbl->SetPrivateDataInterface(obj, &RC_GUID_A, &b->iface), S_OK);
    EXPECT_EQ(rc_obj_refs(a), 1);
    EXPECT_EQ(rc_obj_refs(b), 2);
    /* a second slot */
    EXPECT_HR(obj->lpVtbl->SetPrivateDataInterface(obj, &RC_GUID_B, &a->iface), S_OK);
    EXPECT_EQ(rc_obj_refs(a), 2);
    /* the stored value reads back as the pointer */
    IUnknown *got = NULL; UINT sz = sizeof(got);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_A, &sz, &got), S_OK);
    EXPECT_EQ(sz, sizeof(void *));
    CHECK(got == &b->iface);
    LONG extra = rc_obj_refs(b) - 2;
    t_note("kind %d: GetPrivateData of an interface changes its refcount by %ld", K, (long)extra);
    while (extra-- > 0) b->iface.lpVtbl->Release(&b->iface);
    /* overwrite the interface by plain data: the interface is released */
    DWORD dw = 7;
    EXPECT_HR(obj->lpVtbl->SetPrivateData(obj, &RC_GUID_A, sizeof(dw), &dw), S_OK);
    EXPECT_EQ(rc_obj_refs(b), 1);
    /* delete via NULL interface */
    EXPECT_HR(obj->lpVtbl->SetPrivateDataInterface(obj, &RC_GUID_B, NULL), S_OK);
    EXPECT_EQ(rc_obj_refs(a), 1);
    sz = sizeof(got);
    EXPECT_HR(obj->lpVtbl->GetPrivateData(obj, &RC_GUID_B, &sz, &got), HR_NOT_FOUND);
    /* destroying the owner releases whatever it still holds */
    EXPECT_HR(obj->lpVtbl->SetPrivateDataInterface(obj, &RC_GUID_C, &a->iface), S_OK);
    EXPECT_HR(obj->lpVtbl->SetPrivateDataInterface(obj, &RC_GUID_B, &b->iface), S_OK);
    EXPECT_EQ(rc_obj_refs(a), 2); EXPECT_EQ(rc_obj_refs(b), 2);
    EXPECT_EQ(RC_RELEASE(obj), 0);
    obj = NULL;
    EXPECT_EQ(rc_obj_refs(a), 1);
    EXPECT_EQ(rc_obj_refs(b), 1);
    a->iface.lpVtbl->Release(&a->iface); b->iface.lpVtbl->Release(&b->iface);
    CHECK(a->destroyed && b->destroyed);
    rc_obj_delete(a); rc_obj_delete(b);
    RC_DEVICE_END(dev, ctx);
}

static void k_privif_dev_gone(int K)
{
    KIND_FIX(K);
    rc_obj *a = rc_obj_new();
    EXPECT_HR(obj->lpVtbl->SetPrivateDataInterface(obj, &RC_GUID_A, &a->iface), S_OK);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    EXPECT_EQ(rc_obj_refs(a), 2);   /* still held while the object lives */
    RC_RELEASE(obj); obj = NULL;
    EXPECT_EQ(rc_obj_refs(a), 1);
    a->iface.lpVtbl->Release(&a->iface);
    CHECK(a->destroyed);
    rc_obj_delete(a);
}

#define X(k, n) \
    TEST(d3d11_debug_refcount_##n##_initial_counts)   { k_initial(k); } \
    TEST(d3d11_debug_refcount_##n##_addref_chain)     { k_chain(k); } \
    TEST(d3d11_debug_refcount_##n##_iunknown_identity){ k_identity(k); } \
    TEST(d3d11_debug_refcount_##n##_qi_matrix)        { k_matrix(k); } \
    TEST(d3d11_debug_refcount_##n##_qi_junk_iid)      { k_qi_junk(k); } \
    TEST(d3d11_debug_refcount_##n##_getdevice_addref) { k_getdevice(k); } \
    TEST(d3d11_debug_refcount_##n##_device_released_first) { k_dev_first(k); } \
    TEST(d3d11_debug_refcount_##n##_object_outlives_device){ k_dev_last(k); } \
    TEST(d3d11_debug_refcount_##n##_two_instances)    { k_two(k); } \
    TEST(d3d11_debug_refcount_##n##_private_data)     { k_privdata(k); } \
    TEST(d3d11_debug_refcount_##n##_debug_name)       { k_name(k); } \
    TEST(d3d11_debug_refcount_##n##_private_interface){ k_privif(k); } \
    TEST(d3d11_debug_refcount_##n##_private_interface_device_gone) { k_privif_dev_gone(k); }
RC_KINDS(X)
#undef X
