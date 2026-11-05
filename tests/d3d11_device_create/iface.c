/* Device and immediate context object contract: interfaces, identity, refcounts, exception mode, private data,
 * stability, threads, D3D11-on-12 and the other exports of d3d11.dll. */
#include "dc.h"

#define T(n) TEST(d3d11_device_create_##n)
#define NEEDDEV() ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0; \
    REQUIRE(DC_FN()); DC_NEED_WARP(d, c, fl)

/* ---- QueryInterface table ---- */
static void qi_expect(IUnknown *u, const IID *iid, int must, const char *nm)
{
    void *p = (void *)(ULONG_PTR)0;
    HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    if (must) {
        t_check(hr == S_OK, __FILE__, __LINE__, nm, "QI got %s", d3dh_hr_name(hr));
        t_check(p != NULL, __FILE__, __LINE__, nm, "QI returned NULL object");
    } else {
        if (hr != S_OK) {
            t_check(hr == E_NOINTERFACE, __FILE__, __LINE__, nm, "QI got %s want E_NOINTERFACE", d3dh_hr_name(hr));
            t_check(p == NULL, __FILE__, __LINE__, nm, "QI failure left non-NULL out pointer");
        } else t_note("%s supported on this system", nm);
    }
    if (p && hr == S_OK) IUnknown_Release((IUnknown *)p);
}
#define QI_MUST(nm, iid) T(qi_device_##nm) { NEEDDEV(); qi_expect((IUnknown *)d, &iid, 1, #iid); DC_REL(d, c); }
#define QI_OPT(nm, iid)  T(qi_device_##nm) { NEEDDEV(); qi_expect((IUnknown *)d, &iid, 0, #iid); DC_REL(d, c); }
#define QI_NO(nm, iid)   T(qi_device_##nm##_unsupported) { NEEDDEV(); \
    void *p = (void *)(ULONG_PTR)0; HRESULT hr = ID3D11Device_QueryInterface(d, &iid, &p); \
    EXPECT_HR(hr, E_NOINTERFACE); CHECK(p == NULL); DC_REL(d, c); }
QI_MUST(iunknown, IID_IUnknown)
QI_MUST(id3d11device, IID_ID3D11Device)
QI_MUST(idxgiobject, IID_IDXGIObject)
QI_MUST(idxgidevice, IID_IDXGIDevice)
QI_MUST(idxgidevice1, IID_IDXGIDevice1)
QI_MUST(idxgidevice2, IID_IDXGIDevice2)
QI_MUST(idxgidevice3, IID_IDXGIDevice3)
QI_OPT(idxgidevice4, IID_IDXGIDevice4)
QI_MUST(id3d11device1, IID_ID3D11Device1)
QI_MUST(id3d11device2, IID_ID3D11Device2)
QI_MUST(id3d11device3, IID_ID3D11Device3)
QI_OPT(id3d11device4, IID_ID3D11Device4)
QI_OPT(id3d11device5, IID_ID3D11Device5)
QI_NO(texture2d, IID_ID3D11Texture2D)
QI_NO(context, IID_ID3D11DeviceContext)
QI_NO(factory, IID_IDXGIFactory)
QI_NO(adapter, IID_IDXGIAdapter)
QI_NO(swapchain, IID_IDXGISwapChain)
QI_NO(buffer, IID_ID3D11Buffer)
QI_NO(guid_null, GUID_NULL)

T(qi_device_failure_does_not_change_refcount)
{
    NEEDDEV();
    void *p = NULL;
    ULONG r0 = d3dh_refcount(d);
    EXPECT_HR(ID3D11Device_QueryInterface(d, &IID_ID3D11Texture2D, &p), E_NOINTERFACE);
    EXPECT_EQ(d3dh_refcount(d), r0);
    DC_REL(d, c);
}
T(qi_device_success_adds_one_reference)
{
    NEEDDEV();
    IDXGIDevice *dd = NULL; ID3D11Device1 *d1 = NULL;
    ULONG r0 = d3dh_refcount(d);
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    EXPECT_EQ(d3dh_refcount(d), r0 + 1);
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1));
    EXPECT_EQ(d3dh_refcount(d), r0 + 2);
    SAFE_RELEASE(d1);
    SAFE_RELEASE(dd);
    EXPECT_EQ(d3dh_refcount(d), r0);
    DC_REL(d, c);
}
T(qi_device_refcount_shared_across_interfaces)
{
    NEEDDEV();
    IDXGIDevice *dd = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    ULONG a = d3dh_refcount(d), b = d3dh_refcount(dd);
    EXPECT_EQ(a, b);
    SAFE_RELEASE(dd);
    DC_REL(d, c);
}
T(qi_device_iunknown_identity)
{
    NEEDDEV();
    IUnknown *u1 = NULL, *u2 = NULL, *u3 = NULL; IDXGIDevice *dd = NULL; ID3D11Device1 *d1 = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IUnknown, (void **)&u1));
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1));
    if (dd) EXPECT_HR_OK(IDXGIDevice_QueryInterface(dd, &IID_IUnknown, (void **)&u2));
    if (d1) EXPECT_HR_OK(ID3D11Device1_QueryInterface(d1, &IID_IUnknown, (void **)&u3));
    CHECK(u1 && u1 == u2 && u1 == u3);
    SAFE_RELEASE(u1); SAFE_RELEASE(u2); SAFE_RELEASE(u3); SAFE_RELEASE(dd); SAFE_RELEASE(d1);
    DC_REL(d, c);
}
T(qi_device_roundtrip_to_base)
{
    NEEDDEV();
    IDXGIDevice1 *dd = NULL; ID3D11Device *back = NULL; ID3D11Device3 *d3 = NULL, *d3b = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice1, (void **)&dd));
    if (dd) EXPECT_HR_OK(IDXGIDevice1_QueryInterface(dd, &IID_ID3D11Device, (void **)&back));
    CHECK(back == d);
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3));
    if (d3) EXPECT_HR_OK(ID3D11Device3_QueryInterface(d3, &IID_ID3D11Device3, (void **)&d3b));
    CHECK(d3 == d3b);
    SAFE_RELEASE(d3b); SAFE_RELEASE(d3); SAFE_RELEASE(back); SAFE_RELEASE(dd);
    DC_REL(d, c);
}
T(qi_device_same_pointer_for_repeated_query)
{
    NEEDDEV();
    IDXGIDevice *a = NULL, *b = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&a));
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&b));
    CHECK(a == b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DC_REL(d, c);
}
T(qi_device_when_created_for_every_level)
{
    static const D3D_FEATURE_LEVEL lv[] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_11_0 };
    REQUIRE(DC_FN());
    for (int i = 0; i < 4; i++) {
        ID3D11Device *d = NULL; IDXGIDevice1 *dd = NULL; ID3D11Device1 *d1 = NULL;
        if (FAILED(dc_warp(0, &lv[i], 1, &d, NULL, NULL))) { t_skip("no WARP"); return; }
        t_check(SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice1, (void **)&dd)), __FILE__, __LINE__, "IDXGIDevice1", "level %s", dc_fl_name(lv[i]));
        t_check(SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1)), __FILE__, __LINE__, "ID3D11Device1", "level %s", dc_fl_name(lv[i]));
        SAFE_RELEASE(dd); SAFE_RELEASE(d1); SAFE_RELEASE(d);
    }
}

/* ---- IDXGIDevice behaviour ---- */
T(dxgi_device_get_adapter_is_warp)
{
    NEEDDEV();
    IDXGIDevice *dd = NULL; IDXGIAdapter *a = NULL; DXGI_ADAPTER_DESC ad;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(dd, &a));
    CHECK(a != NULL);
    if (a) {
        memset(&ad, 0, sizeof ad);
        EXPECT_HR_OK(IDXGIAdapter_GetDesc(a, &ad));
        EXPECT_EQ(ad.VendorId, 0x1414);   /* Microsoft software adapter */
        t_note("WARP adapter: vendor=%x device=%x", ad.VendorId, ad.DeviceId);
        SAFE_RELEASE(a);
    }
    SAFE_RELEASE(dd);
    DC_REL(d, c);
}
T(dxgi_device_get_parent_adapter_matches_get_adapter)
{
    NEEDDEV();
    IDXGIDevice *dd = NULL; IDXGIAdapter *a = NULL, *p = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(dd, &a));
    EXPECT_HR_OK(IDXGIDevice_GetParent(dd, &IID_IDXGIAdapter, (void **)&p));
    CHECK(a && a == p);
    SAFE_RELEASE(a); SAFE_RELEASE(p); SAFE_RELEASE(dd);
    DC_REL(d, c);
}
T(dxgi_device_get_parent_wrong_iid)
{
    NEEDDEV();
    IDXGIDevice *dd = NULL; void *p = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    HRESULT hr = IDXGIDevice_GetParent(dd, &IID_ID3D11Texture2D, &p);
    EXPECT_HR_FAILED(hr);
    CHECK(p == NULL);
    SAFE_RELEASE(dd);
    DC_REL(d, c);
}
T(dxgi_device_adapter_factory_chain)
{
    NEEDDEV();
    IDXGIDevice *dd = NULL; IDXGIAdapter *a = NULL; IDXGIFactory *f = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(dd, &a));
    if (a) EXPECT_HR_OK(IDXGIAdapter_GetParent(a, &IID_IDXGIFactory, (void **)&f));
    CHECK(f != NULL);
    SAFE_RELEASE(f); SAFE_RELEASE(a); SAFE_RELEASE(dd);
    DC_REL(d, c);
}
T(dxgi_device1_frame_latency_default_and_range)
{
    NEEDDEV();
    IDXGIDevice1 *dd = NULL; UINT lat = 99;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice1, (void **)&dd));
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(dd, &lat));
    EXPECT_EQ(lat, 3);
    EXPECT_HR_OK(IDXGIDevice1_SetMaximumFrameLatency(dd, 1));
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(dd, &lat));
    EXPECT_EQ(lat, 1);
    EXPECT_HR_OK(IDXGIDevice1_SetMaximumFrameLatency(dd, 16));
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(dd, &lat));
    EXPECT_EQ(lat, 16);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(dd, 17), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(dd, &lat));
    EXPECT_EQ(lat, 16);
    SAFE_RELEASE(dd);
    DC_REL(d, c);
}
T(dxgi_device_gpu_thread_priority_range)
{
    NEEDDEV();
    IDXGIDevice *dd = NULL; INT pr = 99;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    EXPECT_HR_OK(IDXGIDevice_GetGPUThreadPriority(dd, &pr));
    EXPECT_EQ(pr, 0);
    EXPECT_HR(IDXGIDevice_SetGPUThreadPriority(dd, 8), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIDevice_SetGPUThreadPriority(dd, -8), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR_OK(IDXGIDevice_GetGPUThreadPriority(dd, &pr));
    EXPECT_EQ(pr, 0);
    SAFE_RELEASE(dd);
    DC_REL(d, c);
}

/* ---- device getters ---- */
T(device_get_feature_level_matches_output)
{
    NEEDDEV();
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), fl);
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), ID3D11Device_GetFeatureLevel(d));
    DC_REL(d, c);
}
T(device_get_device_removed_reason_ok)
{
    NEEDDEV();
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    DC_REL(d, c);
}
T(device_creation_flags_default)
{
    NEEDDEV();
    UINT cf = ID3D11Device_GetCreationFlags(d);
    t_note("default GetCreationFlags = 0x%x", cf);
    EXPECT_EQ(cf & D3D11_CREATE_DEVICE_SINGLETHREADED, 0);
    EXPECT_EQ(ID3D11Device_GetCreationFlags(d), cf);
    DC_REL(d, c);
}
T(device_exception_mode_default_zero)
{
    NEEDDEV();
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), 0);
    DC_REL(d, c);
}
T(device_exception_mode_roundtrip)
{
    NEEDDEV();
    EXPECT_HR_OK(ID3D11Device_SetExceptionMode(d, D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR));
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR);
    EXPECT_HR_OK(ID3D11Device_SetExceptionMode(d, 0));
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), 0);
    DC_REL(d, c);
}
T(device_exception_mode_invalid_bits)
{
    NEEDDEV();
    HRESULT hr = ID3D11Device_SetExceptionMode(d, 0x2);
    t_note("SetExceptionMode(0x2) -> %s", d3dh_hr_name(hr));
    if (FAILED(hr)) EXPECT_HR(hr, E_INVALIDARG);
    hr = ID3D11Device_SetExceptionMode(d, 0xfffffffeu);
    t_note("SetExceptionMode(0xfffffffe) -> %s", d3dh_hr_name(hr));
    if (FAILED(hr)) { EXPECT_HR(hr, E_INVALIDARG); EXPECT_EQ(ID3D11Device_GetExceptionMode(d), 0); }
    ID3D11Device_SetExceptionMode(d, 0);
    DC_REL(d, c);
}
T(device_exception_mode_not_shared_between_devices)
{
    NEEDDEV();
    ID3D11Device *d2 = NULL;
    if (FAILED(dc_warp(0, NULL, 0, &d2, NULL, NULL))) { DC_REL(d, c); t_skip("second device"); return; }
    EXPECT_HR_OK(ID3D11Device_SetExceptionMode(d, D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR));
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d2), 0);
    ID3D11Device_SetExceptionMode(d, 0);
    SAFE_RELEASE(d2);
    DC_REL(d, c);
}

/* ---- immediate context ---- */
T(context_get_immediate_twice_same_pointer)
{
    NEEDDEV();
    ID3D11DeviceContext *a = NULL, *b = NULL;
    ID3D11Device_GetImmediateContext(d, &a);
    ID3D11Device_GetImmediateContext(d, &b);
    CHECK(a != NULL); CHECK(a == b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DC_REL(d, c);
}
T(context_returned_by_create_is_immediate_context)
{
    NEEDDEV();
    ID3D11DeviceContext *a = NULL;
    ID3D11Device_GetImmediateContext(d, &a);
    CHECK(a == c);
    SAFE_RELEASE(a);
    DC_REL(d, c);
}
T(context_get_immediate_adds_one_reference)
{
    NEEDDEV();
    ID3D11DeviceContext *a = NULL, *b = NULL;
    ULONG r0 = d3dh_refcount(c);
    ID3D11Device_GetImmediateContext(d, &a);
    EXPECT_EQ(d3dh_refcount(c), r0 + 1);
    ID3D11Device_GetImmediateContext(d, &b);
    EXPECT_EQ(d3dh_refcount(c), r0 + 2);
    SAFE_RELEASE(b); SAFE_RELEASE(a);
    EXPECT_EQ(d3dh_refcount(c), r0);
    DC_REL(d, c);
}
T(context_get_device_adds_one_device_reference)
{
    NEEDDEV();
    ID3D11Device *g = NULL;
    ULONG r0 = d3dh_refcount(d);
    ID3D11DeviceContext_GetDevice(c, &g);
    CHECK(g == d);
    EXPECT_EQ(d3dh_refcount(d), r0 + 1);
    SAFE_RELEASE(g);
    EXPECT_EQ(d3dh_refcount(d), r0);
    DC_REL(d, c);
}
T(context_type_and_flags)
{
    NEEDDEV();
    EXPECT_EQ(ID3D11DeviceContext_GetType(c), D3D11_DEVICE_CONTEXT_IMMEDIATE);
    EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(c), 0);
    DC_REL(d, c);
}
T(context_interfaces)
{
    NEEDDEV();
    qi_expect((IUnknown *)c, &IID_ID3D11DeviceContext, 1, "ID3D11DeviceContext");
    qi_expect((IUnknown *)c, &IID_ID3D11DeviceContext1, 1, "ID3D11DeviceContext1");
    qi_expect((IUnknown *)c, &IID_ID3D11DeviceContext2, 1, "ID3D11DeviceContext2");
    qi_expect((IUnknown *)c, &IID_ID3D11DeviceContext3, 1, "ID3D11DeviceContext3");
    qi_expect((IUnknown *)c, &IID_ID3D11DeviceContext4, 0, "ID3D11DeviceContext4");
    qi_expect((IUnknown *)c, &IID_ID3D11Multithread, 1, "ID3D11Multithread");
    qi_expect((IUnknown *)c, &IID_ID3DUserDefinedAnnotation, 1, "ID3DUserDefinedAnnotation");
    qi_expect((IUnknown *)c, &IID_ID3D11Device, 0, "ID3D11Device(from context)");
    DC_REL(d, c);
}
T(context_not_device_interfaces)
{
    NEEDDEV();
    void *p = NULL;
    EXPECT_HR(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Device, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_HR(ID3D11DeviceContext_QueryInterface(c, &IID_IDXGIDevice, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    DC_REL(d, c);
}
T(context_multithread_protect_default)
{
    NEEDDEV();
    ID3D11Multithread *mt = NULL;
    EXPECT_HR_OK(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt));
    if (mt) {
        EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), FALSE);
        EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, TRUE), FALSE);
        EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), TRUE);
        EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, FALSE), TRUE);
        SAFE_RELEASE(mt);
    }
    DC_REL(d, c);
}
T(context_singlethreaded_multithread_interface)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Multithread *mt = NULL;
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(D3D11_CREATE_DEVICE_SINGLETHREADED, NULL, 0, &d, &c, NULL))) { t_skip("no WARP"); return; }
    HRESULT hr = ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt);
    t_note("ID3D11Multithread on SINGLETHREADED device: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) CHECK(mt != NULL); else CHECK(mt == NULL);
    SAFE_RELEASE(mt);
    DC_REL(d, c);
}
T(context_flush_and_clear_state_callable)
{
    NEEDDEV();
    ID3D11DeviceContext_ClearState(c);
    ID3D11DeviceContext_Flush(c);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    DC_REL(d, c);
}
T(deferred_context_properties)
{
    NEEDDEV();
    ID3D11DeviceContext *dc = NULL, *dc2 = NULL;
    EXPECT_HR_OK(ID3D11Device_CreateDeferredContext(d, 0, &dc));
    EXPECT_HR_OK(ID3D11Device_CreateDeferredContext(d, 0, &dc2));
    CHECK(dc && dc2 && dc != dc2 && dc != c);
    if (dc) {
        ID3D11Device *g = NULL;
        EXPECT_EQ(ID3D11DeviceContext_GetType(dc), D3D11_DEVICE_CONTEXT_DEFERRED);
        EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(dc), 0);
        ID3D11DeviceContext_GetDevice(dc, &g);
        CHECK(g == d);
        SAFE_RELEASE(g);
    }
    SAFE_RELEASE(dc); SAFE_RELEASE(dc2);
    DC_REL(d, c);
}
T(deferred_context_finish_empty)
{
    NEEDDEV();
    ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR_OK(ID3D11Device_CreateDeferredContext(d, 0, &dc));
    if (dc) {
        EXPECT_HR_OK(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl));
        CHECK(cl != NULL);
        if (cl) {
            ID3D11Device *g = NULL;
            ID3D11CommandList_GetDevice(cl, &g);
            CHECK(g == d);
            SAFE_RELEASE(g);
            EXPECT_EQ(ID3D11CommandList_GetContextFlags(cl), 0);
        }
    }
    SAFE_RELEASE(cl); SAFE_RELEASE(dc);
    DC_REL(d, c);
}
T(deferred_context_invalid_flags)
{
    NEEDDEV();
    ID3D11DeviceContext *dc = NULL;
    HRESULT hr = ID3D11Device_CreateDeferredContext(d, 1, &dc);
    t_note("CreateDeferredContext(flags=1) -> %s", d3dh_hr_name(hr));
    if (FAILED(hr)) { CHECK(dc == NULL); }
    SAFE_RELEASE(dc);
    DC_REL(d, c);
}

/* ---- private data (device and immediate context) ---- */
static const GUID G1 = { 0x11111111, 0x1111, 0x1111, { 1, 1, 1, 1, 1, 1, 1, 1 } };
static const GUID G2 = { 0x22222222, 0x2222, 0x2222, { 2, 2, 2, 2, 2, 2, 2, 2 } };

typedef struct { const IUnknownVtbl *v; LONG ref; } tobj;
static HRESULT WINAPI to_qi(IUnknown *s, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown)) { *o = s; InterlockedIncrement(&((tobj *)s)->ref); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI to_addref(IUnknown *s) { return InterlockedIncrement(&((tobj *)s)->ref); }
static ULONG WINAPI to_release(IUnknown *s) { return InterlockedDecrement(&((tobj *)s)->ref); }
static const IUnknownVtbl to_vtbl = { to_qi, to_addref, to_release };

static int pd_kind;   /* 1 = ID3D11Device, 0 = ID3D11DeviceContext */
static HRESULT pd_set_(void *o, const GUID *g, UINT n, const void *p)
{ return pd_kind ? ID3D11Device_SetPrivateData((ID3D11Device *)o, g, n, p) : ID3D11DeviceContext_SetPrivateData((ID3D11DeviceContext *)o, g, n, p); }
static HRESULT pd_get_(void *o, const GUID *g, UINT *n, void *p)
{ return pd_kind ? ID3D11Device_GetPrivateData((ID3D11Device *)o, g, n, p) : ID3D11DeviceContext_GetPrivateData((ID3D11DeviceContext *)o, g, n, p); }
static HRESULT pd_seti_(void *o, const GUID *g, const IUnknown *u)
{ return pd_kind ? ID3D11Device_SetPrivateDataInterface((ID3D11Device *)o, g, u) : ID3D11DeviceContext_SetPrivateDataInterface((ID3D11DeviceContext *)o, g, u); }

#define PD_BOTH(nm, ...) \
    static void pd_##nm(void *o) __VA_ARGS__ \
    T(pd_device_##nm) { NEEDDEV(); pd_kind = 1; pd_##nm(d); DC_REL(d, c); } \
    T(pd_context_##nm) { NEEDDEV(); pd_kind = 0; pd_##nm(c); DC_REL(d, c); }

#define SPD(o, g, n, p)  pd_set_(o, g, n, p)
#define GPD(o, g, n, p)  pd_get_(o, g, n, p)
#define SPDI(o, g, u)    pd_seti_(o, g, u)

PD_BOTH(roundtrip, {
    static const BYTE data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    BYTE out[8] = { 0 }; UINT sz = 8;
    EXPECT_HR_OK(SPD(o, &G1, 8, data));
    EXPECT_HR_OK(GPD(o, &G1, &sz, out));
    EXPECT_EQ(sz, 8); CHECK(!memcmp(data, out, 8));
})
PD_BOTH(size_query_with_null_buffer, {
    static const BYTE data[5] = { 9, 8, 7, 6, 5 };
    UINT sz = 0;
    EXPECT_HR_OK(SPD(o, &G1, 5, data));
    EXPECT_HR_OK(GPD(o, &G1, &sz, NULL));
    EXPECT_EQ(sz, 5);
})
PD_BOTH(buffer_too_small, {
    static const BYTE data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    BYTE out[8]; UINT sz = 3;
    memset(out, 0xcc, 8);
    EXPECT_HR_OK(SPD(o, &G1, 8, data));
    EXPECT_HR(GPD(o, &G1, &sz, out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 8);
    EXPECT_EQ(out[0], 0xcc);
    sz = 0;
    EXPECT_HR(GPD(o, &G1, &sz, out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 8);
})
PD_BOTH(buffer_larger_than_data, {
    static const BYTE data[4] = { 1, 2, 3, 4 };
    BYTE out[16]; UINT sz = 16;
    memset(out, 0xcc, 16);
    EXPECT_HR_OK(SPD(o, &G1, 4, data));
    EXPECT_HR_OK(GPD(o, &G1, &sz, out));
    EXPECT_EQ(sz, 4); CHECK(!memcmp(data, out, 4)); EXPECT_EQ(out[4], 0xcc);
})
PD_BOTH(unknown_guid_not_found, {
    BYTE out[4]; UINT sz = 4;
    EXPECT_HR(GPD(o, &G2, &sz, out), DXGI_ERROR_NOT_FOUND);
    sz = 0;
    EXPECT_HR(GPD(o, &G2, &sz, NULL), DXGI_ERROR_NOT_FOUND);
})
PD_BOTH(replace_same_guid, {
    static const BYTE a[4] = { 1, 2, 3, 4 }, b[6] = { 9, 8, 7, 6, 5, 4 };
    BYTE out[8]; UINT sz = 8;
    EXPECT_HR_OK(SPD(o, &G1, 4, a));
    EXPECT_HR_OK(SPD(o, &G1, 6, b));
    EXPECT_HR_OK(GPD(o, &G1, &sz, out));
    EXPECT_EQ(sz, 6); CHECK(!memcmp(b, out, 6));
})
PD_BOTH(two_guids_independent, {
    static const BYTE a[2] = { 1, 2 }, b[3] = { 7, 8, 9 };
    BYTE out[4]; UINT sz = 4;
    EXPECT_HR_OK(SPD(o, &G1, 2, a));
    EXPECT_HR_OK(SPD(o, &G2, 3, b));
    EXPECT_HR_OK(GPD(o, &G1, &sz, out)); EXPECT_EQ(sz, 2); CHECK(!memcmp(a, out, 2));
    sz = 4;
    EXPECT_HR_OK(GPD(o, &G2, &sz, out)); EXPECT_EQ(sz, 3); CHECK(!memcmp(b, out, 3));
})
PD_BOTH(delete_with_size_zero, {
    static const BYTE a[4] = { 1, 2, 3, 4 };
    UINT sz = 4; BYTE out[4];
    EXPECT_HR_OK(SPD(o, &G1, 4, a));
    EXPECT_HR_OK(SPD(o, &G1, 0, NULL));
    EXPECT_HR(GPD(o, &G1, &sz, out), DXGI_ERROR_NOT_FOUND);
})
PD_BOTH(debug_object_name, {
    static const char nm[] = "d3d11_device_create_name";
    char out[64]; UINT sz = sizeof out;
    EXPECT_HR_OK(SPD(o, &WKPDID_D3DDebugObjectName, sizeof nm - 1, nm));
    EXPECT_HR_OK(GPD(o, &WKPDID_D3DDebugObjectName, &sz, out));
    EXPECT_EQ(sz, sizeof nm - 1); CHECK(!memcmp(out, nm, sizeof nm - 1));
})
PD_BOTH(data_is_copied, {
    BYTE a[4] = { 1, 2, 3, 4 }, out[4]; UINT sz = 4;
    EXPECT_HR_OK(SPD(o, &G1, 4, a));
    a[0] = 99;
    EXPECT_HR_OK(GPD(o, &G1, &sz, out));
    EXPECT_EQ(out[0], 1);
})
PD_BOTH(interface_refcounting, {
    tobj ob = { &to_vtbl, 1 };
    IUnknown *u = (IUnknown *)&ob, *got = NULL; UINT sz = sizeof got;
    EXPECT_HR_OK(SPDI(o, &G1, u));
    EXPECT_EQ(ob.ref, 2);
    EXPECT_HR_OK(GPD(o, &G1, &sz, &got));
    EXPECT_EQ(sz, sizeof(void *)); CHECK(got == u); EXPECT_EQ(ob.ref, 3);
    IUnknown_Release(got);
    EXPECT_HR_OK(SPD(o, &G1, 0, NULL));              /* removal releases the interface */
    EXPECT_EQ(ob.ref, 1);
})
PD_BOTH(interface_replaced_by_data_releases, {
    tobj ob = { &to_vtbl, 1 };
    static const BYTE a[2] = { 5, 6 };
    EXPECT_HR_OK(SPDI(o, &G1, (IUnknown *)&ob));
    EXPECT_EQ(ob.ref, 2);
    EXPECT_HR_OK(SPD(o, &G1, 2, a));
    EXPECT_EQ(ob.ref, 1);
})
PD_BOTH(interface_replaced_by_interface, {
    tobj o1 = { &to_vtbl, 1 }, o2 = { &to_vtbl, 1 };
    EXPECT_HR_OK(SPDI(o, &G1, (IUnknown *)&o1));
    EXPECT_HR_OK(SPDI(o, &G1, (IUnknown *)&o2));
    EXPECT_EQ(o1.ref, 1); EXPECT_EQ(o2.ref, 2);
    SPD(o, &G1, 0, NULL);
    EXPECT_EQ(o2.ref, 1);
})
PD_BOTH(interface_size_query, {
    tobj ob = { &to_vtbl, 1 };
    UINT sz = 0;
    EXPECT_HR_OK(SPDI(o, &G1, (IUnknown *)&ob));
    EXPECT_HR_OK(GPD(o, &G1, &sz, NULL));
    EXPECT_EQ(sz, sizeof(void *));
    EXPECT_EQ(ob.ref, 2);                            /* size query must not AddRef */
    SPD(o, &G1, 0, NULL);
    EXPECT_EQ(ob.ref, 1);
})

T(pd_interface_released_when_device_destroyed)
{
    NEEDDEV();
    tobj ob = { &to_vtbl, 1 };
    EXPECT_HR_OK(ID3D11Device_SetPrivateDataInterface(d, &G1, (IUnknown *)&ob));
    EXPECT_EQ(ob.ref, 2);
    DC_REL(d, c);
    EXPECT_EQ(ob.ref, 1);
}
T(pd_device_private_data_not_shared_with_context)
{
    NEEDDEV();
    static const BYTE a[2] = { 1, 2 };
    BYTE out[2]; UINT sz = 2;
    EXPECT_HR_OK(ID3D11Device_SetPrivateData(d, &G1, 2, a));
    EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &G1, &sz, out), DXGI_ERROR_NOT_FOUND);
    DC_REL(d, c);
}
T(pd_device_private_data_visible_via_other_interface)
{
    NEEDDEV();
    static const BYTE a[3] = { 1, 2, 3 };
    BYTE out[3]; UINT sz = 3; IDXGIDevice *dd = NULL;
    EXPECT_HR_OK(ID3D11Device_SetPrivateData(d, &G1, 3, a));
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    if (dd) {
        HRESULT hr = IDXGIDevice_GetPrivateData(dd, &G1, &sz, out);
        t_note("D3D11 private data seen through IDXGIObject: %s", d3dh_hr_name(hr));
    }
    SAFE_RELEASE(dd);
    DC_REL(d, c);
}
T(pd_dxgi_object_private_data_roundtrip)
{
    NEEDDEV();
    static const BYTE a[3] = { 4, 5, 6 };
    BYTE out[3]; UINT sz = 3; IDXGIDevice *dd = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    if (dd) {
        EXPECT_HR_OK(IDXGIDevice_SetPrivateData(dd, &G2, 3, a));
        EXPECT_HR_OK(IDXGIDevice_GetPrivateData(dd, &G2, &sz, out));
        EXPECT_EQ(sz, 3); CHECK(!memcmp(a, out, 3));
    }
    SAFE_RELEASE(dd);
    DC_REL(d, c);
}

/* ---- stability ---- */
T(stress_create_destroy_100)
{
    REQUIRE(DC_FN());
    D3D_FEATURE_LEVEL first = 0;
    for (int i = 0; i < 100; i++) {
        ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0;
        HRESULT hr = dc_warp(0, NULL, 0, &d, &c, &fl);
        if (FAILED(hr)) { if (i == 0) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; } EXPECT_HR_OK(hr); break; }
        if (i == 0) first = fl;
        if (fl != first) { CHECK(fl == first); }
        if (ID3D11Device_GetFeatureLevel(d) != fl) CHECK(0);
        DC_REL(d, c);
    }
}
T(stress_create_destroy_device_only_100)
{
    REQUIRE(DC_FN());
    for (int i = 0; i < 100; i++) {
        ID3D11Device *d = NULL;
        HRESULT hr = dc_warp(i & 1 ? D3D11_CREATE_DEVICE_SINGLETHREADED : 0, NULL, 0, &d, NULL, NULL);
        if (FAILED(hr)) { if (i == 0) { t_skip("no WARP"); return; } EXPECT_HR_OK(hr); break; }
        SAFE_RELEASE(d);
    }
}
T(stress_many_live_devices)
{
    ID3D11Device *ds[16]; int n = 0;
    REQUIRE(DC_FN());
    for (; n < 16; n++) {
        if (FAILED(dc_warp(0, NULL, 0, &ds[n], NULL, NULL))) break;
    }
    if (n == 0) { t_skip("no WARP"); return; }
    EXPECT_EQ(n, 16);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) if (ds[i] == ds[j]) CHECK(0);
    for (int i = 0; i < n; i++) SAFE_RELEASE(ds[i]);
}
T(stress_create_with_all_levels_cycle)
{
    static const D3D_FEATURE_LEVEL lv[] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0 };
    REQUIRE(DC_FN());
    for (int i = 0; i < 60; i++) {
        ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0;
        HRESULT hr = dc_warp(0, &lv[i % 6], 1, &d, NULL, &fl);
        if (FAILED(hr)) { if (i == 0) { t_skip("no WARP"); return; } EXPECT_HR_OK(hr); break; }
        if (fl != lv[i % 6]) CHECK(fl == lv[i % 6]);
        SAFE_RELEASE(d);
    }
}
T(stress_buffer_create_after_context_dropped)
{
    NEEDDEV();
    ID3D11Buffer *b = NULL; D3D11_BUFFER_DESC bd = { 16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    SAFE_RELEASE(c);
    EXPECT_HR_OK(ID3D11Device_CreateBuffer(d, &bd, NULL, &b));
    SAFE_RELEASE(b); SAFE_RELEASE(d);
}

static DWORD WINAPI worker_use(LPVOID p)
{
    ID3D11Device *d = (ID3D11Device *)p; ID3D11Buffer *b = NULL;
    D3D11_BUFFER_DESC bd = { 16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    HRESULT hr = ID3D11Device_CreateBuffer(d, &bd, NULL, &b);
    SAFE_RELEASE(b);
    return (DWORD)hr;
}
T(thread_device_used_on_other_thread)
{
    NEEDDEV();
    HANDLE h = CreateThread(NULL, 0, worker_use, d, 0, NULL);
    CHECK(h != NULL);
    if (h) {
        EXPECT_EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0);
        DWORD code = 1; GetExitCodeThread(h, &code);
        EXPECT_EQ(code, 0);
        CloseHandle(h);
    }
    DC_REL(d, c);
}
static DWORD WINAPI worker_create(LPVOID p)
{
    ID3D11Device **out = (ID3D11Device **)p;
    return (DWORD)dc_warp(0, NULL, 0, out, NULL, NULL);
}
T(thread_device_created_on_other_thread_released_here)
{
    ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HANDLE h = CreateThread(NULL, 0, worker_create, &d, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 20000), WAIT_OBJECT_0);
    DWORD code = 1; GetExitCodeThread(h, &code); CloseHandle(h);
    if ((HRESULT)code < 0) { t_skip("no WARP: %s", d3dh_hr_name((HRESULT)code)); return; }
    CHECK(d != NULL);
    if (d) EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(d);
}

/* ---- D3D11-on-12 and other exports ---- */
UDECL(HRESULT, D3D11On12CreateDevice_t, (IUnknown *, UINT, const D3D_FEATURE_LEVEL *, UINT, IUnknown *const *, UINT, UINT,
                                          ID3D11Device **, ID3D11DeviceContext **, D3D_FEATURE_LEVEL *));
static HRESULT make_on12(UINT flags, const D3D_FEATURE_LEVEL *l, UINT n, ID3D12Device **dev12, ID3D12CommandQueue **q,
                         ID3D11Device **d, ID3D11DeviceContext **c, D3D_FEATURE_LEVEL *fl)
{
    int warp = 0;
    HRESULT hr = d3dh_make_device12(dev12, D3D_FEATURE_LEVEL_11_0, &warp);
    if (FAILED(hr)) return hr;
    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0, 0 };
    hr = ID3D12Device_CreateCommandQueue(*dev12, &qd, &IID_ID3D12CommandQueue, (void **)q);
    if (FAILED(hr)) { ID3D12Device_Release(*dev12); *dev12 = NULL; return hr; }
    HRESULT (WINAPI *f)(IUnknown *, UINT, const D3D_FEATURE_LEVEL *, UINT, IUnknown *const *, UINT, UINT,
                        ID3D11Device **, ID3D11DeviceContext **, D3D_FEATURE_LEVEL *) =
        (void *)t_proc_in("d3d11.dll", "D3D11On12CreateDevice");
    if (!f) return E_NOTIMPL;
    IUnknown *qs[1] = { (IUnknown *)*q };
    return f((IUnknown *)*dev12, flags, l, n, qs, 1, 0, d, c, fl);
}
#define ON12_BEGIN() ID3D12Device *d12 = NULL; ID3D12CommandQueue *q = NULL; ID3D11Device *d = NULL; \
    ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL; HRESULT hr; \
    REQUIRE(t_proc_in("d3d11.dll", "D3D11On12CreateDevice")); \
    if (t_is_wine()) { t_skip("D3D12 under emulation"); return; } \
    hr = make_on12(0, NULL, 0, &d12, &q, &d, &c, &fl); \
    if (FAILED(hr)) { SAFE_RELEASE(q); SAFE_RELEASE(d12); t_skip("D3D11On12CreateDevice setup: %s", d3dh_hr_name(hr)); return; }
#define ON12_END() do { DC_REL(d, c); SAFE_RELEASE(q); SAFE_RELEASE(d12); } while (0)

T(on12_basic)
{
    ON12_BEGIN();
    CHECK(d && c); CHECK(dc_fl_known(fl)); CHECK(fl >= D3D_FEATURE_LEVEL_11_0);
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), fl);
    ON12_END();
}
T(on12_device_interfaces)
{
    ON12_BEGIN();
    qi_expect((IUnknown *)d, &IID_ID3D11On12Device, 1, "ID3D11On12Device");
    qi_expect((IUnknown *)d, &IID_IDXGIDevice, 1, "IDXGIDevice");
    qi_expect((IUnknown *)d, &IID_ID3D11Device1, 1, "ID3D11Device1");
    ON12_END();
}
T(on12_immediate_context_identity)
{
    ON12_BEGIN();
    ID3D11DeviceContext *a = NULL;
    ID3D11Device_GetImmediateContext(d, &a);
    CHECK(a == c);
    SAFE_RELEASE(a);
    EXPECT_EQ(ID3D11DeviceContext_GetType(c), D3D11_DEVICE_CONTEXT_IMMEDIATE);
    ON12_END();
}
T(on12_device_removed_reason_ok)
{
    ON12_BEGIN();
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    ON12_END();
}
T(on12_creation_flags_bgra)
{
    ID3D12Device *d12 = NULL; ID3D12CommandQueue *q = NULL; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0;
    REQUIRE(t_proc_in("d3d11.dll", "D3D11On12CreateDevice"));
    if (t_is_wine()) { t_skip("D3D12 under emulation"); return; }
    HRESULT hr = make_on12(D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, &d12, &q, &d, NULL, &fl);
    if (FAILED(hr)) { SAFE_RELEASE(q); SAFE_RELEASE(d12); t_skip("setup: %s", d3dh_hr_name(hr)); return; }
    CHECK(ID3D11Device_GetCreationFlags(d) & D3D11_CREATE_DEVICE_BGRA_SUPPORT);
    SAFE_RELEASE(d); SAFE_RELEASE(q); SAFE_RELEASE(d12);
}
T(on12_levels_list_respected)
{
    ID3D12Device *d12 = NULL; ID3D12CommandQueue *q = NULL; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0;
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_11_0 };
    REQUIRE(t_proc_in("d3d11.dll", "D3D11On12CreateDevice"));
    if (t_is_wine()) { t_skip("D3D12 under emulation"); return; }
    HRESULT hr = make_on12(0, l, 1, &d12, &q, &d, NULL, &fl);
    if (FAILED(hr)) { SAFE_RELEASE(q); SAFE_RELEASE(d12); t_skip("setup: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_11_0);
    SAFE_RELEASE(d); SAFE_RELEASE(q); SAFE_RELEASE(d12);
}
T(on12_null_device_fails)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    D3D11On12CreateDevice_t_pfn f = (D3D11On12CreateDevice_t_pfn)t_proc_in("d3d11.dll", "D3D11On12CreateDevice");
    REQUIRE(f);
    HRESULT hr = f(NULL, 0, NULL, 0, NULL, 0, 0, &d, &c, &fl);
    EXPECT_HR_FAILED(hr);
    t_note("D3D11On12CreateDevice(NULL device) -> %s", d3dh_hr_name(hr));
    CHECK(d == NULL); CHECK(c == NULL);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(on12_non_d3d12_object_fails)
{
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL; ID3D11Device *notd12 = NULL;
    D3D11On12CreateDevice_t_pfn f = (D3D11On12CreateDevice_t_pfn)t_proc_in("d3d11.dll", "D3D11On12CreateDevice");
    REQUIRE(f); REQUIRE(DC_FN());
    if (FAILED(dc_warp(0, NULL, 0, &notd12, NULL, NULL))) { t_skip("no WARP"); return; }
    HRESULT hr = f((IUnknown *)notd12, 0, NULL, 0, NULL, 0, 0, &d, NULL, &fl);
    EXPECT_HR_FAILED(hr);
    t_note("D3D11On12CreateDevice(11 device) -> %s", d3dh_hr_name(hr));
    CHECK(d == NULL);
    SAFE_RELEASE(d); SAFE_RELEASE(notd12);
}

T(exports_present)
{
    static const char *names[] = { "D3D11CreateDevice", "D3D11CreateDeviceAndSwapChain", "D3D11CreateDeviceForD3D12",
        "D3D11On12CreateDevice", "D3D11CoreCreateDevice", "D3D11CoreCreateLayeredDevice", "D3D11CoreGetLayeredDeviceSize",
        "D3D11CoreRegisterLayers", "CreateDirect3D11DeviceFromDXGIDevice", "CreateDirect3D11SurfaceFromDXGISurface",
        "EnableFeatureLevelUpgrade" };
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        t_check(GetProcAddress(m, names[i]) != NULL, __FILE__, __LINE__, names[i], "export missing from d3d11.dll");
    }
}
T(exports_ordinals)
{
    static const struct { const char *n; unsigned o; } tab[] = {
        { "D3D11CreateDeviceForD3D12", 1 }, { "D3D11CoreCreateDevice", 18 }, { "D3D11CoreCreateLayeredDevice", 19 },
        { "D3D11CoreGetLayeredDeviceSize", 20 }, { "D3D11CoreRegisterLayers", 21 }, { "D3D11CreateDevice", 22 },
        { "D3D11CreateDeviceAndSwapChain", 23 }, { "D3D11On12CreateDevice", 24 } };
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        FARPROC byname = GetProcAddress(m, tab[i].n), byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)tab[i].o);
        t_check(byname != NULL && byname == byord, __FILE__, __LINE__, tab[i].n, "ordinal %u does not resolve to the named export", tab[i].o);
    }
}
T(exports_not_forwarded_to_other_dll)
{
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    FARPROC p = GetProcAddress(m, "D3D11CreateDevice");
    HMODULE owner = NULL;
    CHECK(p != NULL);
    if (p && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner))
        CHECK(owner == m);
}
T(exports_create_device_same_across_lookup)
{
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    CHECK(GetProcAddress(m, "D3D11CreateDevice") == (FARPROC)t_proc("D3D11CreateDevice"));
    CHECK(GetProcAddress(m, "D3D11CreateDevice") != GetProcAddress(m, "D3D11CreateDeviceAndSwapChain"));
}

/* D3D11CoreCreateDevice(extensions, adapter, driverType, software, flags, levels, count, <reserved>, ppDevice, pFeatureLevel) */
typedef struct { void *p; UINT n; UINT pad; } core_ext;
typedef HRESULT (WINAPI *core_create_fn)(core_ext *, IUnknown *, UINT, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, ULONG_PTR,
                                         ID3D11Device **, D3D_FEATURE_LEVEL *);
T(core_create_extensions_count_too_large)
{
    core_create_fn f = (core_create_fn)t_proc_in("d3d11.dll", "D3D11CoreCreateDevice");
    REQUIRE(f);
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    static const UINT counts[] = { 0xff, 0x100, 0x1000, 0xffffffffu };
    char dummy[16];
    for (unsigned i = 0; i < 4; i++) {
        core_ext e = { dummy, counts[i], 0 };
        HRESULT hr = f(&e, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, 7, &d, &fl);
        EXPECT_HR(hr, E_INVALIDARG);
        CHECK(d == NULL); EXPECT_EQ(fl, 0);
        fl = DC_FL_SENTINEL;
    }
}
T(core_create_extensions_null_pointer_with_count)
{
    core_create_fn f = (core_create_fn)t_proc_in("d3d11.dll", "D3D11CoreCreateDevice");
    REQUIRE(f);
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    core_ext e = { NULL, 1, 0 };
    EXPECT_HR(f(&e, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, 7, &d, &fl), E_INVALIDARG);
    CHECK(d == NULL); EXPECT_EQ(fl, 0);
}
