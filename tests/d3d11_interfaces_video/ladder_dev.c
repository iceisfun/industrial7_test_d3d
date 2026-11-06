/* Device interface ladder: QueryInterface behaviour of ID3D11Device and its aggregated interfaces. */
#include "ifv.h"

static const D3D_FEATURE_LEVEL k_fls[] = {
    D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1 };

#define QE(iid, w) { &iid, #iid, w }

T(dev_qi_ladder) {
    IFV_DEV(d, c);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_IUnknown, 1), QE(IID_ID3D11Device, 1), QE(IID_ID3D11Device1, 1), QE(IID_ID3D11Device2, 1),
        QE(IID_ID3D11Device3, 1), QE(IID_ID3D11Device4, 1), QE(IID_ID3D11Device5, 1),
        QE(IID_IDXGIObject, 1), QE(IID_IDXGIDevice, 1), QE(IID_IDXGIDevice1, 1), QE(IID_IDXGIDevice2, 1),
        QE(IID_IDXGIDevice3, 1), QE(IID_IDXGIDevice4, -1),
        QE(IID_ID3D11Multithread, -1), QE(IID_ID3D11VideoDevice, -1), QE(IID_ID3D11VideoDevice1, -1),
        QE(IID_ID3D10Multithread, -1),
    };
    ifv_check_qi_table(d, tab, sizeof(tab) / sizeof(tab[0]), "device");
    SAFE_RELEASE(c); EXPECT_EQ(D3DH_RELEASE_RC(d), 0); d = NULL;
}

T(dev_qi_unsupported_interfaces) {
    IFV_DEV(d, c);
    /* Interfaces that belong to contexts, resources, other APIs or the debug layer are not implemented by a plain device. */
    static const struct ifv_qi_ent tab[] = {
        QE(IID_ID3D11DeviceContext, 0), QE(IID_ID3D11DeviceContext1, 0), QE(IID_ID3D11DeviceContext2, 0),
        QE(IID_ID3D11DeviceContext3, 0), QE(IID_ID3D11DeviceContext4, 0),
        QE(IID_ID3D11DeviceChild, 0), QE(IID_ID3D11Resource, 0), QE(IID_ID3D11Texture2D, 0), QE(IID_ID3D11Buffer, 0),
        QE(IID_ID3D11Asynchronous, 0), QE(IID_ID3D11CommandList, 0), QE(IID_ID3D11Fence, 0),
        QE(IID_ID3D11VideoContext, 0), QE(IID_ID3D11VideoContext1, 0), QE(IID_ID3D11VideoContext2, 0),
        QE(IID_ID3DUserDefinedAnnotation, 0), QE(IID_ID3DDeviceContextState, 0),
        QE(IID_IDXGIFactory, 0), QE(IID_IDXGIAdapter, 0), QE(IID_IDXGISwapChain, 0), QE(IID_IDXGIResource, 0),
        QE(IID_ID3D12Device, 0), QE(IID_ID3D11On12Device, 0), QE(IID_ID3D11On12Device1, 0), QE(IID_ID3D11On12Device2, 0),
        QE(IID_ID3D10Device, 0), QE(IID_ID3D10Device1, 0),
    };
    ifv_check_qi_table(d, tab, sizeof(tab) / sizeof(tab[0]), "device");
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_qi_debug_interfaces_absent_without_layer) {
    IFV_DEV(d, c);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_ID3D11Debug, 0), QE(IID_ID3D11InfoQueue, 0), QE(IID_ID3D11SwitchToRef, 0),
    };
    ifv_check_qi_table(d, tab, 3, "non-debug device");
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_qi_null_and_random_iid) {
    IFV_DEV(d, c);
    void *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11Device_QueryInterface(d, &GUID_NULL, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11Device_QueryInterface(d, &ifv_IID_Random, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11DeviceContext_QueryInterface(c, &GUID_NULL, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11DeviceContext_QueryInterface(c, &ifv_IID_Random, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_qi_per_feature_level) {
    int tested = 0;
    for (unsigned i = 0; i < sizeof(k_fls) / sizeof(k_fls[0]); i++) {
        ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
        if (!ifv_dev_at(&d, &c, k_fls[i], 0)) { t_note("feature level 0x%x not available", k_fls[i]); continue; }
        tested++;
        char what[48];
        sprintf(what, "device FL 0x%x", k_fls[i]);
        CHECK(ID3D11Device_GetFeatureLevel(d) == k_fls[i]);
        static const struct ifv_qi_ent tab[] = {
            QE(IID_ID3D11Device1, 1), QE(IID_ID3D11Device2, 1), QE(IID_ID3D11Device3, 1), QE(IID_ID3D11Device4, 1),
            QE(IID_ID3D11Device5, 1), QE(IID_IDXGIDevice, 1), QE(IID_IDXGIDevice1, 1), QE(IID_IDXGIDevice2, 1),
            QE(IID_IDXGIDevice3, 1), QE(IID_ID3D11VideoDevice, -1), QE(IID_ID3D11On12Device, 0),
            QE(IID_ID3D11Debug, 0), QE(IID_ID3D11InfoQueue, 0),
        };
        ifv_check_qi_table(d, tab, sizeof(tab) / sizeof(tab[0]), what);
        static const struct ifv_qi_ent ctab[] = {
            QE(IID_ID3D11DeviceContext1, 1), QE(IID_ID3D11DeviceContext2, 1), QE(IID_ID3D11DeviceContext3, 1),
            QE(IID_ID3D11DeviceContext4, 1), QE(IID_ID3D11Multithread, 1), QE(IID_ID3DUserDefinedAnnotation, 1),
            QE(IID_ID3D11VideoContext, -1), QE(IID_ID3D11Device, 0),
        };
        sprintf(what, "context FL 0x%x", k_fls[i]);
        ifv_check_qi_table(c, ctab, sizeof(ctab) / sizeof(ctab[0]), what);
        SAFE_RELEASE(c); SAFE_RELEASE(d);
    }
    if (!tested) t_skip("no feature level could be created");
}

T(dev_qi_identity_across_interfaces) {
    IFV_DEV(d, c);
    static const GUID *iids[] = { &IID_ID3D11Device, &IID_ID3D11Device1, &IID_ID3D11Device2, &IID_ID3D11Device3,
        &IID_ID3D11Device4, &IID_ID3D11Device5, &IID_IDXGIObject, &IID_IDXGIDevice, &IID_IDXGIDevice1, &IID_IDXGIDevice2 };
    void *id0 = ifv_identity(d);
    CHECK(id0 != NULL);
    for (unsigned i = 0; i < sizeof(iids) / sizeof(iids[0]); i++) {
        IUnknown *u = NULL;
        HRESULT hr = ID3D11Device_QueryInterface(d, iids[i], (void **)&u);
        if (FAILED(hr)) { t_note("QI %s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr)); continue; }
        CHECKF(ifv_identity(u) == id0, "identity of %s differs", d3dh_guid_str(iids[i]));
        /* QI from the derived interface back to the device interface returns the same object. */
        ID3D11Device *back = NULL;
        EXPECT_HR(IUnknown_QueryInterface(u, &IID_ID3D11Device, (void **)&back), S_OK);
        CHECK(back == d);
        SAFE_RELEASE(back);
        IUnknown_Release(u);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_qi_refcount_delta) {
    IFV_DEV(d, c);
    static const GUID *iids[] = { &IID_ID3D11Device1, &IID_ID3D11Device3, &IID_ID3D11Device5, &IID_IDXGIDevice, &IID_IDXGIDevice1 };
    for (unsigned i = 0; i < sizeof(iids) / sizeof(iids[0]); i++) {
        ULONG before = d3dh_refcount(d);
        IUnknown *u = NULL;
        if (FAILED(ID3D11Device_QueryInterface(d, iids[i], (void **)&u))) continue;
        ULONG during = d3dh_refcount(d);
        EXPECT_EQ(during, before + 1);
        EXPECT_EQ(d3dh_refcount(u), during);   /* one shared count for the aggregated object */
        IUnknown_Release(u);
        EXPECT_EQ(d3dh_refcount(d), before);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_addref_release_sequence) {
    IFV_DEV(d, c);
    ULONG base = d3dh_refcount(d);
    EXPECT_EQ(ID3D11Device_AddRef(d), base + 1);
    EXPECT_EQ(ID3D11Device_AddRef(d), base + 2);
    EXPECT_EQ(ID3D11Device_Release(d), base + 1);
    EXPECT_EQ(ID3D11Device_Release(d), base);
    SAFE_RELEASE(c);
    EXPECT_EQ(D3DH_RELEASE_RC(d), 0);
}

T(dev_immediate_context_identity) {
    IFV_DEV(d, c);
    ID3D11DeviceContext *c2 = NULL, *c3 = (void *)(ULONG_PTR)1;
    ULONG rc = d3dh_refcount(c);
    ID3D11Device_GetImmediateContext(d, &c2);
    CHECK(c2 == c);
    EXPECT_EQ(d3dh_refcount(c), rc + 1);
    ID3D11DeviceContext_Release(c2);
    EXPECT_EQ(d3dh_refcount(c), rc);
    CHECK(ID3D11DeviceContext_GetType(c) == D3D11_DEVICE_CONTEXT_IMMEDIATE);
    CHECK(ID3D11DeviceContext_GetContextFlags(c) == 0);
    /* the context keeps no reference to itself: repeated Get/Release is balanced */
    for (int i = 0; i < 10; i++) { ID3D11Device_GetImmediateContext(d, &c3); ID3D11DeviceContext_Release(c3); }
    EXPECT_EQ(d3dh_refcount(c), rc);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_immediate_context_variants) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL; ID3D11Device2 *d2 = NULL; ID3D11Device3 *d3 = NULL;
    void *ident = ifv_identity(c);
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1))) {
        ID3D11DeviceContext1 *c1 = NULL;
        ID3D11Device1_GetImmediateContext1(d1, &c1);
        CHECK(c1 != NULL);
        CHECK(ifv_identity(c1) == ident);
        SAFE_RELEASE(c1);
        SAFE_RELEASE(d1);
    }
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device2, (void **)&d2))) {
        ID3D11DeviceContext2 *c2 = NULL;
        ID3D11Device2_GetImmediateContext2(d2, &c2);
        CHECK(c2 != NULL);
        CHECK(ifv_identity(c2) == ident);
        SAFE_RELEASE(c2);
        SAFE_RELEASE(d2);
    }
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) {
        ID3D11DeviceContext3 *c3 = NULL;
        ID3D11Device3_GetImmediateContext3(d3, &c3);
        CHECK(c3 != NULL);
        CHECK(ifv_identity(c3) == ident);
        SAFE_RELEASE(c3);
        SAFE_RELEASE(d3);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_context_getdevice) {
    IFV_DEV(d, c);
    ID3D11Device *g = NULL;
    ULONG rc = d3dh_refcount(d);
    ID3D11DeviceContext_GetDevice(c, &g);
    CHECK(g == d);
    EXPECT_EQ(d3dh_refcount(d), rc + 1);
    SAFE_RELEASE(g);
    EXPECT_EQ(d3dh_refcount(d), rc);
    /* a device child (a buffer) keeps the device alive and reports it */
    D3D11_BUFFER_DESC bd = { 64, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    ID3D11Buffer *b = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(d, &bd, NULL, &b), S_OK);
    if (b) {
        EXPECT_EQ(d3dh_refcount(d), rc + 1);
        ID3D11Buffer_GetDevice(b, &g);
        CHECK(g == d);
        SAFE_RELEASE(g);
        SAFE_RELEASE(b);
        EXPECT_EQ(d3dh_refcount(d), rc);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_getfeaturelevel_matches_creation) {
    IFV_DEV(d, c);
    CHECK(ID3D11Device_GetFeatureLevel(d) == fl_);
    CHECK(fl_ >= D3D_FEATURE_LEVEL_9_1 && fl_ <= D3D_FEATURE_LEVEL_11_1);
    ID3D11Device1 *d1 = NULL;
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1))) {
        CHECK(ID3D11Device1_GetFeatureLevel(d1) == fl_);
        SAFE_RELEASE(d1);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_creation_flags_roundtrip) {
    static const UINT flags[] = { 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT, D3D11_CREATE_DEVICE_SINGLETHREADED,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED };
    int made = 0;
    for (unsigned i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
        ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
        HRESULT hr = d3dh_make_device11(&d, &c, NULL, NULL, flags[i]);
        if (FAILED(hr)) { t_note("flags 0x%x: %s", flags[i], d3dh_hr_name(hr)); continue; }
        made++;
        UINT got = ID3D11Device_GetCreationFlags(d);
        CHECKF((got & flags[i]) == flags[i], "flags 0x%x -> GetCreationFlags 0x%x", flags[i], got);
        if (got != flags[i]) t_note("flags 0x%x reported as 0x%x", flags[i], got);
        if (!flags[i]) EXPECT_EQ(got, 0);
        SAFE_RELEASE(c); SAFE_RELEASE(d);
    }
    if (!made) t_skip("no device");
}

T(dev_device_removed_reason_healthy) {
    IFV_DEV(d, c);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_exception_mode) {
    IFV_DEV(d, c);
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), 0);
    static const UINT bad[] = { 2, 3, 4, 0x10, 0x8000, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        HRESULT hr = ID3D11Device_SetExceptionMode(d, bad[i]);
        CHECKF(hr == E_INVALIDARG, "SetExceptionMode(0x%x) = %s", bad[i], d3dh_hr_name(hr));
        EXPECT_EQ(ID3D11Device_GetExceptionMode(d), 0);   /* rejected values leave the mode untouched */
    }
    EXPECT_HR(ID3D11Device_SetExceptionMode(d, D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR), S_OK);
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR);
    EXPECT_HR(ID3D11Device_SetExceptionMode(d, 0), S_OK);
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), 0);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_deferred_context_flags_and_null) {
    IFV_DEV(d, c);
    static const UINT bad[] = { 1, 2, 0x80, 0xffffffffu };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        ID3D11DeviceContext *dc = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D11Device_CreateDeferredContext(d, bad[i], &dc);
        CHECKF(hr == E_INVALIDARG, "CreateDeferredContext(0x%x) = %s", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(dc);
        /* with a NULL out pointer the flag validation still fires first */
        hr = ID3D11Device_CreateDeferredContext(d, bad[i], NULL);
        CHECKF(hr == E_INVALIDARG, "CreateDeferredContext(0x%x, NULL) = %s", bad[i], d3dh_hr_name(hr));
    }
    /* valid flags with NULL out is a validation-only call and returns S_FALSE */
    EXPECT_HR(ID3D11Device_CreateDeferredContext(d, 0, NULL), S_FALSE);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_deferred_context_created) {
    IFV_DEV(d, c);
    ID3D11DeviceContext *dc = NULL;
    ULONG rc = d3dh_refcount(d);
    EXPECT_HR(ID3D11Device_CreateDeferredContext(d, 0, &dc), S_OK);
    if (dc) {
        CHECK(dc != c);
        CHECK(ID3D11DeviceContext_GetType(dc) == D3D11_DEVICE_CONTEXT_DEFERRED);
        EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(dc), 0);
        EXPECT_EQ(d3dh_refcount(dc), 1);
        EXPECT_EQ(d3dh_refcount(d), rc + 1);
        ID3D11Device *g = NULL;
        ID3D11DeviceContext_GetDevice(dc, &g);
        CHECK(g == d);
        SAFE_RELEASE(g);
        EXPECT_EQ(D3DH_RELEASE_RC(dc), 0);
        EXPECT_EQ(d3dh_refcount(d), rc);
    }
    /* a second deferred context is a distinct object */
    ID3D11DeviceContext *a = NULL, *b = NULL;
    ID3D11Device_CreateDeferredContext(d, 0, &a);
    ID3D11Device_CreateDeferredContext(d, 0, &b);
    CHECK(a && b && a != b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_deferred_context_variants) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL; ID3D11Device2 *d2 = NULL; ID3D11Device3 *d3 = NULL;
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1))) {
        ID3D11DeviceContext1 *x = NULL;
        EXPECT_HR(ID3D11Device1_CreateDeferredContext1(d1, 0, &x), S_OK);
        if (x) { CHECK(ID3D11DeviceContext1_GetType(x) == D3D11_DEVICE_CONTEXT_DEFERRED); SAFE_RELEASE(x); }
        EXPECT_HR(ID3D11Device1_CreateDeferredContext1(d1, 1, &x), E_INVALIDARG);
        SAFE_RELEASE(d1);
    }
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device2, (void **)&d2))) {
        ID3D11DeviceContext2 *x = NULL;
        EXPECT_HR(ID3D11Device2_CreateDeferredContext2(d2, 0, &x), S_OK);
        if (x) { CHECK(ID3D11DeviceContext2_GetType(x) == D3D11_DEVICE_CONTEXT_DEFERRED); SAFE_RELEASE(x); }
        EXPECT_HR(ID3D11Device2_CreateDeferredContext2(d2, 1, &x), E_INVALIDARG);
        SAFE_RELEASE(d2);
    }
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) {
        ID3D11DeviceContext3 *x = NULL;
        EXPECT_HR(ID3D11Device3_CreateDeferredContext3(d3, 0, &x), S_OK);
        if (x) { CHECK(ID3D11DeviceContext3_GetType(x) == D3D11_DEVICE_CONTEXT_DEFERRED); SAFE_RELEASE(x); }
        EXPECT_HR(ID3D11Device3_CreateDeferredContext3(d3, 1, &x), E_INVALIDARG);
        SAFE_RELEASE(d3);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_singlethreaded_rejects_deferred) {
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    HRESULT hr = d3dh_make_device11(&d, &c, NULL, NULL, D3D11_CREATE_DEVICE_SINGLETHREADED);
    if (FAILED(hr)) { t_skip("no single threaded device (%s)", d3dh_hr_name(hr)); return; }
    ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(d, 0, &dc), DXGI_ERROR_INVALID_CALL);
    CHECK(dc == NULL);
    EXPECT_HR(ID3D11Device_CreateDeferredContext(d, 0, NULL), DXGI_ERROR_INVALID_CALL);
    SAFE_RELEASE(dc);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_dxgi_device_chain) {
    IFV_DEV(d, c);
    IDXGIDevice *dd = NULL;
    EXPECT_HR(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd), S_OK);
    if (!dd) { SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    IDXGIAdapter *ad = NULL;
    EXPECT_HR(IDXGIDevice_GetAdapter(dd, &ad), S_OK);
    IDXGIAdapter *ad2 = NULL;
    EXPECT_HR(IDXGIDevice_GetParent(dd, &IID_IDXGIAdapter, (void **)&ad2), S_OK);
    CHECK(ad && ad2 && ifv_identity(ad) == ifv_identity(ad2));
    IDXGIFactory *f = NULL;
    if (ad) EXPECT_HR(IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory, (void **)&f), S_OK);
    CHECK(f != NULL);
    /* a factory obtained from the adapter must also be a IDXGIFactory1 on Windows 10 */
    if (f) EXPECT_HR(ifv_qi(f, &IID_IDXGIFactory1, NULL), S_OK);
    /* GetParent of the device with a non-parent IID */
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = IDXGIDevice_GetParent(dd, &IID_ID3D11Device, &p);
    CHECKF(FAILED(hr), "GetParent(ID3D11Device) = %s", d3dh_hr_name(hr));
    CHECK(p == NULL);
    SAFE_RELEASE(f); SAFE_RELEASE(ad2); SAFE_RELEASE(ad); SAFE_RELEASE(dd);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_dxgi_gpu_thread_priority) {
    IFV_DEV(d, c);
    IDXGIDevice *dd = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd))) { SAFE_RELEASE(c); SAFE_RELEASE(d); t_skip("no IDXGIDevice"); return; }
    INT pri = 99;
    EXPECT_HR(IDXGIDevice_GetGPUThreadPriority(dd, &pri), S_OK);
    EXPECT_EQ(pri, 0);
    for (INT p = -7; p <= 7; p += 7) {
        EXPECT_HR(IDXGIDevice_SetGPUThreadPriority(dd, p), S_OK);
        pri = 99;
        EXPECT_HR(IDXGIDevice_GetGPUThreadPriority(dd, &pri), S_OK);
        EXPECT_EQ(pri, p);
    }
    HRESULT hr = IDXGIDevice_SetGPUThreadPriority(dd, 8);
    CHECKF(FAILED(hr), "SetGPUThreadPriority(8) = %s", d3dh_hr_name(hr));
    hr = IDXGIDevice_SetGPUThreadPriority(dd, -8);
    CHECKF(FAILED(hr), "SetGPUThreadPriority(-8) = %s", d3dh_hr_name(hr));
    pri = 99;
    IDXGIDevice_GetGPUThreadPriority(dd, &pri);
    EXPECT_EQ(pri, 7);   /* rejected values do not change the priority */
    IDXGIDevice_SetGPUThreadPriority(dd, 0);
    SAFE_RELEASE(dd); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_dxgi_maximum_frame_latency) {
    IFV_DEV(d, c);
    IDXGIDevice1 *d1 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice1, (void **)&d1))) { SAFE_RELEASE(c); SAFE_RELEASE(d); t_skip("no IDXGIDevice1"); return; }
    UINT lat = 99;
    EXPECT_HR(IDXGIDevice1_GetMaximumFrameLatency(d1, &lat), S_OK);
    EXPECT_EQ(lat, 3);
    for (UINT v = 1; v <= 16; v += 5) {
        EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, v), S_OK);
        lat = 99;
        EXPECT_HR(IDXGIDevice1_GetMaximumFrameLatency(d1, &lat), S_OK);
        EXPECT_EQ(lat, v);
    }
    EXPECT_HR(IDXGIDevice1_GetMaximumFrameLatency(d1, NULL), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, 16), S_OK);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, 17), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, 0xffffffffu), DXGI_ERROR_INVALID_CALL);
    lat = 99;
    EXPECT_HR(IDXGIDevice1_GetMaximumFrameLatency(d1, &lat), S_OK);
    EXPECT_EQ(lat, 16);   /* a rejected value leaves the latency unchanged */
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, 0), S_OK);
    lat = 99;
    EXPECT_HR(IDXGIDevice1_GetMaximumFrameLatency(d1, &lat), S_OK);
    EXPECT_EQ(lat, 0);   /* Set(0) is accepted and read back as 0 */
    SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_dxgi_device3_trim) {
    IFV_DEV(d, c);
    IDXGIDevice3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice3, (void **)&d3))) { SAFE_RELEASE(c); SAFE_RELEASE(d); t_skip("no IDXGIDevice3"); return; }
    IDXGIDevice3_Trim(d3);
    IDXGIDevice3_Trim(d3);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(d3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

struct feat { D3D11_FEATURE f; const char *name; UINT size; int hard; };
#define FE(f, T_, hard) { f, #f, sizeof(T_), hard }
static const struct feat k_feats[] = {
    FE(D3D11_FEATURE_THREADING, D3D11_FEATURE_DATA_THREADING, 1),
    FE(D3D11_FEATURE_DOUBLES, D3D11_FEATURE_DATA_DOUBLES, 1),
    FE(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS, 1),
    FE(D3D11_FEATURE_D3D11_OPTIONS, D3D11_FEATURE_DATA_D3D11_OPTIONS, 1),
    FE(D3D11_FEATURE_ARCHITECTURE_INFO, D3D11_FEATURE_DATA_ARCHITECTURE_INFO, 1),
    FE(D3D11_FEATURE_D3D9_OPTIONS, D3D11_FEATURE_DATA_D3D9_OPTIONS, 1),
    FE(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT, 1),
    FE(D3D11_FEATURE_D3D9_SHADOW_SUPPORT, D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT, 1),
    FE(D3D11_FEATURE_D3D11_OPTIONS1, D3D11_FEATURE_DATA_D3D11_OPTIONS1, 1),
    FE(D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT, D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT, 1),
    FE(D3D11_FEATURE_MARKER_SUPPORT, D3D11_FEATURE_DATA_MARKER_SUPPORT, 1),
    FE(D3D11_FEATURE_D3D9_OPTIONS1, D3D11_FEATURE_DATA_D3D9_OPTIONS1, 1),
    FE(D3D11_FEATURE_D3D11_OPTIONS2, D3D11_FEATURE_DATA_D3D11_OPTIONS2, 1),
    FE(D3D11_FEATURE_D3D11_OPTIONS3, D3D11_FEATURE_DATA_D3D11_OPTIONS3, 1),
    FE(D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT, 1),
    FE(D3D11_FEATURE_D3D11_OPTIONS4, D3D11_FEATURE_DATA_D3D11_OPTIONS4, 1),
    FE(D3D11_FEATURE_SHADER_CACHE, D3D11_FEATURE_DATA_SHADER_CACHE, 1),
    FE(D3D11_FEATURE_D3D11_OPTIONS5, D3D11_FEATURE_DATA_D3D11_OPTIONS5, 1),
};

T(dev_check_feature_support_sizes) {
    IFV_DEV(d, c);
    unsigned char buf[256];
    for (unsigned i = 0; i < sizeof(k_feats) / sizeof(k_feats[0]); i++) {
        const struct feat *f = &k_feats[i];
        memset(buf, 0xcc, sizeof(buf));
        HRESULT hr = ID3D11Device_CheckFeatureSupport(d, f->f, buf, f->size);
        if (f->hard) t_check(hr == S_OK, __FILE__, __LINE__, "CheckFeatureSupport ok", "%s: %s", f->name, d3dh_hr_name(hr));
        else t_note("%s -> %s", f->name, d3dh_hr_name(hr));
        if (FAILED(hr) && !f->hard) continue;
        if (f->size > 1) {
            hr = ID3D11Device_CheckFeatureSupport(d, f->f, buf, f->size - 1);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "size-1", "%s size-1: %s", f->name, d3dh_hr_name(hr));
        }
        /* Win10: only D3D11_FEATURE_D3D11_OPTIONS2 tolerates a size that is 4 bytes too large (older layout); everything else is exact */
        hr = ID3D11Device_CheckFeatureSupport(d, f->f, buf, f->size + 4);
        t_check(hr == (f->f == D3D11_FEATURE_D3D11_OPTIONS2 ? S_OK : E_INVALIDARG), __FILE__, __LINE__, "size+4", "%s size+4: %s", f->name, d3dh_hr_name(hr));
        hr = ID3D11Device_CheckFeatureSupport(d, f->f, buf, f->size + 8);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "size+8", "%s size+8: %s", f->name, d3dh_hr_name(hr));
        hr = ID3D11Device_CheckFeatureSupport(d, f->f, buf, f->size + 1);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "size+1", "%s size+1: %s", f->name, d3dh_hr_name(hr));
        hr = ID3D11Device_CheckFeatureSupport(d, f->f, buf, 0);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "size 0", "%s size 0: %s", f->name, d3dh_hr_name(hr));
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_check_feature_support_invalid_feature) {
    IFV_DEV(d, c);
    unsigned char buf[64] = {0};
    static const int bad[] = { 0x7fffffff, 0x100, 64, 255, -1 };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        HRESULT hr = ID3D11Device_CheckFeatureSupport(d, (D3D11_FEATURE)bad[i], buf, 4);
        CHECKF(hr == E_INVALIDARG, "feature %d -> %s", bad[i], d3dh_hr_name(hr));
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_check_feature_support_defaults) {
    IFV_DEV(d, c);
    D3D11_FEATURE_DATA_THREADING th; memset(&th, 0xff, sizeof(th));
    if (SUCCEEDED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_THREADING, &th, sizeof(th)))) {
        CHECK(th.DriverConcurrentCreates == TRUE || th.DriverConcurrentCreates == FALSE);
        CHECK(th.DriverCommandLists == TRUE || th.DriverCommandLists == FALSE);
        BRD_EQ(th.DriverConcurrentCreates, 1); BRD_EQ(th.DriverCommandLists, 0);
    }
    D3D11_FEATURE_DATA_DOUBLES db; memset(&db, 0xff, sizeof(db));
    if (SUCCEEDED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_DOUBLES, &db, sizeof(db)))) {
        CHECK(db.DoublePrecisionFloatShaderOps == TRUE || db.DoublePrecisionFloatShaderOps == FALSE);
        if (fl_ < D3D_FEATURE_LEVEL_11_0) t_note("doubles at FL<11: %d", db.DoublePrecisionFloatShaderOps);
    }
    D3D11_FEATURE_DATA_ARCHITECTURE_INFO ai; memset(&ai, 0xff, sizeof(ai));
    if (SUCCEEDED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_ARCHITECTURE_INFO, &ai, sizeof(ai)))) {
        CHECK(ai.TileBasedDeferredRenderer == TRUE || ai.TileBasedDeferredRenderer == FALSE);
        BRD_EQ(ai.TileBasedDeferredRenderer, 0);
    }
    D3D11_FEATURE_DATA_D3D11_OPTIONS o; memset(&o, 0xff, sizeof(o));
    if (SUCCEEDED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof(o)))) {
        BRD_EQ(o.ClearView, 1); BRD_EQ(o.ConstantBufferOffsetting, 1); BRD_EQ(o.ConstantBufferPartialUpdate, 1); BRD_EQ(o.OutputMergerLogicOp, 1);
        if (fl_ >= D3D_FEATURE_LEVEL_11_1) CHECK(o.ConstantBufferOffsetting == TRUE || o.ConstantBufferOffsetting == FALSE);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

static void cfs_null_data(void *p)
{
    ID3D11Device_CheckFeatureSupport((ID3D11Device *)p, D3D11_FEATURE_THREADING, NULL, sizeof(D3D11_FEATURE_DATA_THREADING));
}
T(dev_check_feature_support_null_data) {
    IFV_DEV(d, c);
    /* Win10 does not validate the pointer: a NULL pFeatureSupportData with a correct size raises an access violation */
    EXPECT_EQ(t_try(cfs_null_data, d), 0xc0000005);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev_debug_layer_interfaces) {
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl;
    HRESULT hr = d3dh_make_device11(&d, &c, &fl, NULL, D3D11_CREATE_DEVICE_DEBUG);
    if (FAILED(hr)) { t_skip("debug layer unavailable (%s)", d3dh_hr_name(hr)); return; }
    CHECK((ID3D11Device_GetCreationFlags(d) & D3D11_CREATE_DEVICE_DEBUG) != 0);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_ID3D11Debug, 1), QE(IID_ID3D11InfoQueue, 1), QE(IID_ID3D11SwitchToRef, -1),
        QE(IID_ID3D11Device1, 1), QE(IID_ID3D11Device5, 1),
    };
    ifv_check_qi_table(d, tab, sizeof(tab) / sizeof(tab[0]), "debug device");
    ID3D11InfoQueue *iq = NULL;
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11InfoQueue, (void **)&iq))) {
        ID3D11InfoQueue_ClearStoredMessages(iq);
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(iq), 0);
        /* the default storage filter drops INFO (AddApplicationMessage reports S_FALSE and stores nothing) but keeps CORRUPTION, ERROR,
         * WARNING and MESSAGE */
        EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(iq, D3D11_MESSAGE_SEVERITY_INFO, "ifv test message"), S_FALSE);
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(iq), 0);
        EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(iq, D3D11_MESSAGE_SEVERITY_WARNING, "ifv test message"), S_OK);
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(iq), 1);
        SIZE_T len = 0;
        EXPECT_HR(ID3D11InfoQueue_GetMessage(iq, 0, NULL, &len), S_FALSE);   /* size query with a NULL buffer reports S_FALSE */
        CHECK(len >= sizeof(D3D11_MESSAGE));
        ID3D11InfoQueue_ClearStoredMessages(iq);
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(iq), 0);
        UINT64 lim = ID3D11InfoQueue_GetMessageCountLimit(iq);
        EXPECT_EQ(lim, 1024);
        CHECK(ID3D11InfoQueue_GetStorageFilterStackSize(iq) >= 0);
        SAFE_RELEASE(iq);
    }
    SAFE_RELEASE(c); EXPECT_EQ(D3DH_RELEASE_RC(d), 0);
}

T(dev_debug_flag_requires_sdk_layers_error_code) {
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    HRESULT hr = d3dh_make_device11(&d, &c, NULL, NULL, D3D11_CREATE_DEVICE_DEBUG);
    if (SUCCEEDED(hr)) { t_note("debug device created"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    /* Without the Graphics Tools feature the runtime reports the missing component. */
    t_note("debug device creation -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, DXGI_ERROR_SDK_COMPONENT_MISSING);   /* measured on Win10 22H2 without Graphics Tools */
}
