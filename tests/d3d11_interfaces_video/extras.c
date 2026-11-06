/* Direct3D 11-on-12 device ladder, device-removed events, annotation helpers and object lifetime ordering. */
#include "ifv.h"

#define QE(iid, w) { &iid, #iid, w }

static int on12_open(ID3D12Device **d12, ID3D12CommandQueue **q, ID3D11Device **d, ID3D11DeviceContext **c, D3D_FEATURE_LEVEL *fl) {
    __typeof__(&D3D11On12CreateDevice) create = U(D3D11On12CreateDevice);
    if (!create) { t_skip("D3D11On12CreateDevice not exported"); return 0; }
    HRESULT hr = d3dh_make_device12(d12, D3D_FEATURE_LEVEL_11_0, NULL);
    if (FAILED(hr) || !*d12) { t_skip("no Direct3D 12 device (%s)", d3dh_hr_name(hr)); return 0; }
    D3D12_COMMAND_QUEUE_DESC qd; memset(&qd, 0, sizeof(qd));
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = ID3D12Device_CreateCommandQueue(*d12, &qd, &IID_ID3D12CommandQueue, (void **)q);
    if (FAILED(hr) || !*q) { t_skip("no command queue (%s)", d3dh_hr_name(hr)); ID3D12Device_Release(*d12); *d12 = NULL; return 0; }
    IUnknown *qs[1] = { (IUnknown *)*q };
    hr = create((IUnknown *)*d12, 0, NULL, 0, qs, 1, 0, d, c, fl);
    if (FAILED(hr) || !*d) {
        t_skip("D3D11On12CreateDevice failed (%s)", d3dh_hr_name(hr));
        ID3D12CommandQueue_Release(*q); ID3D12Device_Release(*d12); *q = NULL; *d12 = NULL;
        return 0;
    }
    return 1;
}
static void on12_close(ID3D12Device *d12, ID3D12CommandQueue *q, ID3D11Device *d, ID3D11DeviceContext *c) {
    SAFE_RELEASE(c); SAFE_RELEASE(d); SAFE_RELEASE(q); SAFE_RELEASE(d12);
}

T(on12_device_qi_ladder) {
    ID3D12Device *d12 = NULL; ID3D12CommandQueue *q = NULL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0;
    if (!on12_open(&d12, &q, &d, &c, &fl)) return;
    static const struct ifv_qi_ent tab[] = {
        QE(IID_ID3D11Device, 1), QE(IID_ID3D11Device1, 1), QE(IID_ID3D11Device2, 1), QE(IID_ID3D11Device3, 1),
        QE(IID_ID3D11Device4, 1), QE(IID_ID3D11Device5, 1), QE(IID_IDXGIDevice, 1), QE(IID_IDXGIDevice1, 1),
        QE(IID_ID3D11On12Device, 1), QE(IID_ID3D11On12Device1, -1), QE(IID_ID3D11On12Device2, -1),
        QE(IID_ID3D11Debug, 0), QE(IID_ID3D11InfoQueue, 0), QE(IID_ID3D12Device, 0), QE(IID_ID3D11DeviceContext, 0),
    };
    ifv_check_qi_table(d, tab, sizeof(tab) / sizeof(tab[0]), "11on12 device");
    CHECK(fl >= D3D_FEATURE_LEVEL_11_0);
    CHECK(ID3D11Device_GetFeatureLevel(d) == fl);
    on12_close(d12, q, d, c);
}

T(on12_get_d3d12_device_identity) {
    ID3D12Device *d12 = NULL; ID3D12CommandQueue *q = NULL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0;
    if (!on12_open(&d12, &q, &d, &c, &fl)) return;
    ID3D11On12Device1 *o = NULL;
    HRESULT qh = ID3D11Device_QueryInterface(d, &IID_ID3D11On12Device1, (void **)&o);
    t_note("QI ID3D11On12Device1 -> %s", d3dh_hr_name(qh));
    if (o) {
        ID3D12Device *g = NULL;
        ULONG rc = d3dh_refcount(d12);
        EXPECT_HR(ID3D11On12Device1_GetD3D12Device(o, &IID_ID3D12Device, &g), S_OK);
        CHECK(g != NULL && ifv_identity(g) == ifv_identity(d12));
        EXPECT_EQ(d3dh_refcount(d12), rc + 1);
        SAFE_RELEASE(g);
        void *bad = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D11On12Device1_GetD3D12Device(o, &ifv_IID_Random, (ID3D12Device **)&bad);
        CHECKF(FAILED(hr), "GetD3D12Device(random IID) = %s", d3dh_hr_name(hr));
        CHECK(bad == NULL);
        if (SUCCEEDED(hr) && bad) IUnknown_Release((IUnknown *)bad);
        SAFE_RELEASE(o);
    }
    on12_close(d12, q, d, c);
}

T(on12_context_ladder) {
    ID3D12Device *d12 = NULL; ID3D12CommandQueue *q = NULL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0;
    if (!on12_open(&d12, &q, &d, &c, &fl)) return;
    static const struct ifv_qi_ent tab[] = {
        QE(IID_ID3D11DeviceContext1, 1), QE(IID_ID3D11DeviceContext2, 1), QE(IID_ID3D11DeviceContext3, 1), QE(IID_ID3D11DeviceContext4, 1),
        QE(IID_ID3DUserDefinedAnnotation, 1), QE(IID_ID3D11Multithread, 1), QE(IID_ID3D11VideoContext, -1), QE(IID_ID3D11On12Device, 0),
    };
    ifv_check_qi_table(c, tab, sizeof(tab) / sizeof(tab[0]), "11on12 context");
    CHECK(ID3D11DeviceContext_GetType(c) == D3D11_DEVICE_CONTEXT_IMMEDIATE);
    on12_close(d12, q, d, c);
}

T(on12_invalid_creation_arguments) {
    __typeof__(&D3D11On12CreateDevice) create = U(D3D11On12CreateDevice);
    REQUIRE(create);
    ID3D11Device *d = (void *)(ULONG_PTR)1; ID3D11DeviceContext *c = (void *)(ULONG_PTR)1; D3D_FEATURE_LEVEL fl = 0;
    HRESULT hr = create(NULL, 0, NULL, 0, NULL, 0, 0, &d, &c, &fl);
    EXPECT_HR(hr, E_INVALIDARG);
    CHECKF(FAILED(hr), "created a device from a NULL D3D12 device");
    if (SUCCEEDED(hr)) { SAFE_RELEASE(c); SAFE_RELEASE(d); }
    ID3D11Device *d11 = NULL; ID3D11DeviceContext *c11 = NULL;
    if (SUCCEEDED(d3dh_make_device11(&d11, &c11, NULL, NULL, 0))) {
        d = NULL; c = NULL;
        hr = create((IUnknown *)d11, 0, NULL, 0, NULL, 0, 0, &d, &c, &fl);
        EXPECT_HR(hr, E_INVALIDARG);
        CHECKF(FAILED(hr), "created a device on top of a D3D11 device");
        if (SUCCEEDED(hr)) { SAFE_RELEASE(c); SAFE_RELEASE(d); }
        SAFE_RELEASE(c11); SAFE_RELEASE(d11);
    }
}

T(dev4_removed_event_registration) {
    IFV_DEV(d, c);
    ID3D11Device4 *d4 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device4, (void **)&d4))) { t_skip("no Device4"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD cookie = 0xdeadbeef, cookie2 = 0xdeadbeef;
    HRESULT hr = ID3D11Device4_RegisterDeviceRemovedEvent(d4, ev, &cookie);
    t_note("RegisterDeviceRemovedEvent -> %s cookie %lu", d3dh_hr_name(hr), cookie);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        /* registering the same event handle twice is refused; a different event gets the next cookie */
        HRESULT hr2 = ID3D11Device4_RegisterDeviceRemovedEvent(d4, ev, &cookie2);
        EXPECT_HR(hr2, DXGI_ERROR_INVALID_CALL);
        {
            HANDLE ev2 = CreateEventA(NULL, TRUE, FALSE, NULL);
            hr2 = ID3D11Device4_RegisterDeviceRemovedEvent(d4, ev2, &cookie2);
            EXPECT_HR(hr2, S_OK);
            if (SUCCEEDED(hr2)) { CHECK(cookie != cookie2); ID3D11Device4_UnregisterDeviceRemoved(d4, cookie2); }
            CloseHandle(ev2);
        }
        EXPECT_HR(ID3D11Device4_RegisterDeviceRemovedEvent(d4, NULL, &cookie2), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(ID3D11Device4_RegisterDeviceRemovedEvent(d4, ev, NULL), DXGI_ERROR_INVALID_CALL);
        EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_TIMEOUT);       /* the device is healthy: nothing fires */
        ID3D11Device4_UnregisterDeviceRemoved(d4, cookie);
        ID3D11Device4_UnregisterDeviceRemoved(d4, cookie);         /* unregistering twice is harmless */
    }
    CloseHandle(ev);
    SAFE_RELEASE(d4); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(dev4_removed_event_invalid_arguments) {
    IFV_DEV(d, c);
    ID3D11Device4 *d4 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device4, (void **)&d4))) { t_skip("no Device4"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    DWORD cookie = 0xdeadbeef;
    HRESULT hr = ID3D11Device4_RegisterDeviceRemovedEvent(d4, NULL, &cookie);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    CHECKF(FAILED(hr), "NULL event accepted");
    hr = ID3D11Device4_RegisterDeviceRemovedEvent(d4, INVALID_HANDLE_VALUE, &cookie);
    EXPECT_HR(hr, S_OK);   /* INVALID_HANDLE_VALUE is not validated */
    if (SUCCEEDED(hr)) ID3D11Device4_UnregisterDeviceRemoved(d4, cookie);
    ID3D11Device4_UnregisterDeviceRemoved(d4, 0xffffff00u);        /* unknown cookie: ignored */
    SAFE_RELEASE(d4); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx2_annotation_helpers) {
    IFV_DEV(d, c);
    ID3D11DeviceContext2 *c2 = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11DeviceContext2, (void **)&c2))) { t_skip("no Context2"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    BOOL en = ID3D11DeviceContext2_IsAnnotationEnabled(c2);
    EXPECT_EQ(en, 0);   /* no profiler attached */
    CHECK(en == FALSE);       /* no graphics debugger attached */
    ID3D11DeviceContext2_SetMarkerInt(c2, L"ifv marker", 1);
    ID3D11DeviceContext2_BeginEventInt(c2, L"ifv event", 2);
    ID3D11DeviceContext2_EndEvent(c2);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(c2); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx3_hardware_protection_state) {
    IFV_DEV(d, c);
    ID3D11DeviceContext3 *c3 = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11DeviceContext3, (void **)&c3))) { t_skip("no Context3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    ID3D11DeviceContext3_Flush1(c3, D3D11_CONTEXT_TYPE_ALL, NULL);
    ID3D11DeviceContext3_Flush1(c3, D3D11_CONTEXT_TYPE_3D, NULL);
    BOOL prot = 0x5a5a;
    ID3D11DeviceContext3_GetHardwareProtectionState(c3, &prot);
    BRD_EQ(prot, 0);
    CHECK(prot == FALSE);
    ID3D11DeviceContext3_SetHardwareProtectionState(c3, FALSE);
    ID3D11DeviceContext3_GetHardwareProtectionState(c3, &prot);
    EXPECT_EQ(prot, FALSE);
    SAFE_RELEASE(c3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(lifetime_release_device_before_children) {
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    if (FAILED(d3dh_make_device11(&d, &c, NULL, NULL, 0))) { t_skip("no device"); return; }
    D3D11_BUFFER_DESC bd = { 128, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    ID3D11Buffer *b = NULL;
    ID3D11Device_CreateBuffer(d, &bd, NULL, &b);
    ID3D11DeviceContext *dc = NULL;
    ID3D11Device_CreateDeferredContext(d, 0, &dc);
    ID3D11Device_AddRef(d);
    ULONG rc = ID3D11Device_Release(d);
    EXPECT_EQ(rc, 4);
    CHECK(rc >= 2);
    /* children remain usable while their device is kept alive by them */
    ID3D11Device *g = NULL;
    if (b) { ID3D11Buffer_GetDevice(b, &g); CHECK(g == d); SAFE_RELEASE(g); }
    SAFE_RELEASE(dc);
    SAFE_RELEASE(b);
    SAFE_RELEASE(c);
    EXPECT_EQ(D3DH_RELEASE_RC(d), 0);
}

T(lifetime_video_objects_keep_device_alive) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    HRESULT hr = ifv_make_enum(&v, 32, 32, &e);
    ULONG rc = d3dh_refcount(v.dev);
    ID3D11Device *dev = v.dev;
    SAFE_RELEASE(v.vc); SAFE_RELEASE(v.vd); SAFE_RELEASE(v.ctx);
    v.dev = NULL;
    if (SUCCEEDED(hr) && e) {
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
        EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorContentDesc(e, &desc), S_OK);
        SAFE_RELEASE(e);
    }
    t_note("device refcount before drop %lu", rc);
    EXPECT_EQ(ID3D11Device_Release(dev), 0);
}
