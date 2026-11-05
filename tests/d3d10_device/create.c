/* D3D10CreateDevice / D3D10CreateDevice1 / D3D10CreateDeviceAndSwapChain(1): argument validation, driver types,
 * feature levels, flags, interfaces of the created device, version exports. */
#include "dd.h"
#include <d3d11_4.h>

#define T(n) TEST(d3d10_device_create_##n)
#define NEED10() REQUIRE(DD_C10())
#define NEED10_1() REQUIRE(DD_C10_1())

/* Call CreateDevice expecting failure; the device output must be cleared. want==0: any failure. */
static void fail10(const char *what, HRESULT want, IDXGIAdapter *ad, UINT drv, HMODULE sw, UINT flags, UINT sdk)
{
    ID3D10Device *d = DD_SENT_DEV;
    HRESULT hr = DD_C10()(ad, (D3D10_DRIVER_TYPE)drv, sw, flags, sdk, &d);
    if (want) t_check(hr == want, __FILE__, __LINE__, what, "got %s want %s", d3dh_hr_name(hr), d3dh_hr_name(want));
    else t_check(FAILED(hr), __FILE__, __LINE__, what, "got %s want failure", d3dh_hr_name(hr));
    if (d != NULL) t_note("%s: device output not cleared (%p)", what, d);
    if (d && d != DD_SENT_DEV) ID3D10Device_Release(d);
}
static void fail10_1(const char *what, HRESULT want, IDXGIAdapter *ad, UINT drv, HMODULE sw, UINT flags, UINT fl, UINT sdk)
{
    ID3D10Device1 *d = (ID3D10Device1 *)DD_SENT_DEV;
    HRESULT hr = DD_C10_1()(ad, (D3D10_DRIVER_TYPE)drv, sw, flags, (D3D10_FEATURE_LEVEL1)fl, sdk, &d);
    if (want) t_check(hr == want, __FILE__, __LINE__, what, "got %s want %s", d3dh_hr_name(hr), d3dh_hr_name(want));
    else t_check(FAILED(hr), __FILE__, __LINE__, what, "got %s want failure", d3dh_hr_name(hr));
    if (d != NULL) t_note("%s: device output not cleared (%p)", what, d);
    if (d && d != (ID3D10Device1 *)DD_SENT_DEV) ID3D10Device1_Release(d);
}
#define DRV() ((UINT)dd_driver())
#define NEED_DRV() do { if (dd_driver() < 0) { t_skip("no D3D10 device can be created"); return; } } while (0)

T(sdk_constants)
{
    EXPECT_EQ(D3D10_SDK_VERSION, 29);
    EXPECT_EQ(D3D10_1_SDK_VERSION, 0x20);
    EXPECT_EQ(D3D10_DRIVER_TYPE_HARDWARE, 0);
    EXPECT_EQ(D3D10_DRIVER_TYPE_REFERENCE, 1);
    EXPECT_EQ(D3D10_DRIVER_TYPE_NULL, 2);
    EXPECT_EQ(D3D10_DRIVER_TYPE_SOFTWARE, 3);
    EXPECT_EQ(D3D10_DRIVER_TYPE_WARP, 5);
    EXPECT_EQ(D3D10_FEATURE_LEVEL_10_0, 0xa000);
    EXPECT_EQ(D3D10_FEATURE_LEVEL_10_1, 0xa100);
    EXPECT_EQ(D3D10_FEATURE_LEVEL_9_1, 0x9100);
    EXPECT_EQ(D3D10_FEATURE_LEVEL_9_2, 0x9200);
    EXPECT_EQ(D3D10_FEATURE_LEVEL_9_3, 0x9300);
}

/* ---- SDK version ---- */
#define SDKV(nm, v) T(sdk_version_##nm) { NEED10(); NEED_DRV(); fail10("D3D10CreateDevice sdk " #v, 0, NULL, DRV(), NULL, 0, v); }
SDKV(0, 0) SDKV(1, 1) SDKV(7, 7) SDKV(28, 28) SDKV(30, 30) SDKV(31, 31) SDKV(32, 32) SDKV(99, 99) SDKV(max, 0xffffffffu)
SDKV(shl, 29u << 16)

T(sdk_version_checked_for_every_driver)
{
    static const UINT d[] = { D3D10_DRIVER_TYPE_HARDWARE, D3D10_DRIVER_TYPE_REFERENCE, D3D10_DRIVER_TYPE_NULL, D3D10_DRIVER_TYPE_WARP };
    NEED10();
    for (int i = 0; i < 4; i++) fail10("bad sdk, all driver types", 0, NULL, d[i], NULL, 0, 6);
}
T(sdk_version_29_succeeds)
{
    DD_DEV(dev);
    CHECK(dev != NULL);
    DD_END(dev);
}
T(sdk_version_loop_all_bad)
{
    NEED10(); NEED_DRV();
    for (UINT v = 0; v < 100; v++) {
        if (v == D3D10_SDK_VERSION) continue;
        fail10("sdk loop", 0, NULL, DRV(), NULL, 0, v);
    }
}
#define SDKV1(nm, v) T(sdk1_version_##nm) { NEED10_1(); NEED_DRV(); fail10_1("D3D10CreateDevice1 sdk " #v, 0, NULL, DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, v); }
SDKV1(0, 0) SDKV1(1, 1) SDKV1(31, 31) SDKV1(33, 33) SDKV1(max, 0xffffffffu) SDKV1(100, 100)
T(sdk1_version_0x20_succeeds)
{
    ID3D10Device1 *d;
    NEED10_1(); NEED_DRV();
    HRESULT hr = dd_make1(0, D3D10_FEATURE_LEVEL_10_0, &d);
    EXPECT_HR_OK(hr);
    if (SUCCEEDED(hr)) { CHECK(d != NULL); EXPECT_EQ(ID3D10Device1_Release(d), 0); }
}
T(sdk1_version_29_observed)
{
    ID3D10Device1 *d = NULL;
    NEED10_1(); NEED_DRV();
    HRESULT hr = DD_C10_1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_SDK_VERSION, &d);
    t_note("D3D10CreateDevice1 with the 10.0 SDK version 29: %s", d3dh_hr_name(hr));
    if (d) ID3D10Device1_Release(d);
}

/* ---- output pointer ---- */
T(null_output_create1)
{
    NEED10_1(); NEED_DRV();
    HRESULT hr = DD_C10_1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
}
T(null_output_create_observed)
{
    NEED10(); NEED_DRV();
    HRESULT hr = DD_C10()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, NULL);
    t_note("D3D10CreateDevice with NULL ppDevice: %s", d3dh_hr_name(hr));
}

/* ---- driver types ---- */
#define BADDRV(nm, v) T(driver_type_##nm) { NEED10(); NEED_DRV(); fail10("driver type " #v, 0, NULL, v, NULL, 0, D3D10_SDK_VERSION); }
BADDRV(6, 6) BADDRV(7, 7) BADDRV(100, 100) BADDRV(0x7fffffff, 0x7fffffff) BADDRV(max, 0xffffffffu)
#define BADDRV1(nm, v) T(driver_type1_##nm) { NEED10_1(); NEED_DRV(); fail10_1("driver type " #v, 0, NULL, v, NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION); }
BADDRV1(6, 6) BADDRV1(100, 100) BADDRV1(max, 0xffffffffu)
T(driver_type1_invalid_is_einvalidarg)
{
    NEED10_1(); NEED_DRV();
    fail10_1("driver 0xffffffff", E_INVALIDARG, NULL, 0xffffffffu, NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION);
}
T(driver_type_4_observed)
{
    ID3D10Device *d = NULL;
    NEED10();
    HRESULT hr = DD_C10()(NULL, (D3D10_DRIVER_TYPE)4, NULL, 0, D3D10_SDK_VERSION, &d);
    t_note("undefined driver type 4: %s", d3dh_hr_name(hr));
    if (d) ID3D10Device_Release(d);
}
T(software_without_module)
{
    NEED10(); NEED_DRV();
    fail10("SOFTWARE, no module", 0, NULL, D3D10_DRIVER_TYPE_SOFTWARE, NULL, 0, D3D10_SDK_VERSION);
}
T(software_with_non_driver_module)
{
    NEED10(); NEED_DRV();
    fail10("SOFTWARE, kernel32 as rasteriser", 0, NULL, D3D10_DRIVER_TYPE_SOFTWARE, GetModuleHandleA("kernel32.dll"), 0, D3D10_SDK_VERSION);
}
T(software_with_exe_module)
{
    NEED10(); NEED_DRV();
    fail10("SOFTWARE, the exe as rasteriser", 0, NULL, D3D10_DRIVER_TYPE_SOFTWARE, GetModuleHandleA(NULL), 0, D3D10_SDK_VERSION);
}
T(software_module_with_other_driver_observed)
{
    static const UINT d[] = { D3D10_DRIVER_TYPE_HARDWARE, D3D10_DRIVER_TYPE_REFERENCE, D3D10_DRIVER_TYPE_NULL, D3D10_DRIVER_TYPE_WARP };
    NEED10();
    for (int i = 0; i < 4; i++) {
        ID3D10Device *dev = NULL;
        HRESULT hr = DD_C10()(NULL, (D3D10_DRIVER_TYPE)d[i], GetModuleHandleA("kernel32.dll"), 0, D3D10_SDK_VERSION, &dev);
        t_note("driver %u with a software module handle: %s", d[i], d3dh_hr_name(hr));
        if (dev) ID3D10Device_Release(dev);
    }
}

#define DRVTYPE(nm, drv) T(driver_##nm) { ID3D10Device *d = NULL; NEED10(); \
    HRESULT hr = DD_C10()(NULL, drv, NULL, 0, D3D10_SDK_VERSION, &d); \
    if (FAILED(hr)) { CHECK(d == NULL); t_note(#nm " driver unavailable here: %s", d3dh_hr_name(hr)); return; } \
    CHECK(d != NULL); \
    if (d) { IDXGIDevice *dx = NULL; EXPECT_HR_OK(ID3D10Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dx)); SAFE_RELEASE(dx); \
             EXPECT_EQ(ID3D10Device_Release(d), 0); } }
DRVTYPE(warp, D3D10_DRIVER_TYPE_WARP)
DRVTYPE(reference, D3D10_DRIVER_TYPE_REFERENCE)
DRVTYPE(null, D3D10_DRIVER_TYPE_NULL)
DRVTYPE(hardware, D3D10_DRIVER_TYPE_HARDWARE)

T(driver_failure_clears_output)
{
    NEED10();
    ID3D10Device *d = DD_SENT_DEV;
    HRESULT hr = DD_C10()(NULL, D3D10_DRIVER_TYPE_SOFTWARE, NULL, 0, D3D10_SDK_VERSION, &d);
    CHECK(FAILED(hr));
    if (d != NULL) t_note("device output after failure: %p", d);
}

T(adapter_zero_hardware_observed)
{
    IDXGIFactory *f = NULL; IDXGIAdapter *a = NULL; ID3D10Device *d = NULL;
    NEED10();
    REQUIRE(D3DH_FN("dxgi.dll", CreateDXGIFactory));
    if (FAILED(D3DH_FN("dxgi.dll", CreateDXGIFactory)(&IID_IDXGIFactory, (void **)&f))) { t_skip("no factory"); return; }
    if (FAILED(IDXGIFactory_EnumAdapters(f, 0, &a))) { IDXGIFactory_Release(f); t_skip("no adapter"); return; }
    HRESULT hr = DD_C10()(a, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &d);
    t_note("adapter 0 + HARDWARE: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { CHECK(d != NULL); EXPECT_EQ(ID3D10Device_Release(d), 0); }
    IDXGIAdapter_Release(a); IDXGIFactory_Release(f);
}
T(adapter_with_warp_driver_observed)
{
    IDXGIFactory *f = NULL; IDXGIAdapter *a = NULL; ID3D10Device *d = NULL;
    NEED10();
    REQUIRE(D3DH_FN("dxgi.dll", CreateDXGIFactory));
    if (FAILED(D3DH_FN("dxgi.dll", CreateDXGIFactory)(&IID_IDXGIFactory, (void **)&f))) { t_skip("no factory"); return; }
    if (FAILED(IDXGIFactory_EnumAdapters(f, 0, &a))) { IDXGIFactory_Release(f); t_skip("no adapter"); return; }
    HRESULT hr = DD_C10()(a, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_SDK_VERSION, &d);
    t_note("adapter 0 + WARP: %s", d3dh_hr_name(hr));
    if (d) ID3D10Device_Release(d);
    IDXGIAdapter_Release(a); IDXGIFactory_Release(f);
}

/* ---- flags ---- */
#define FLAGOK(nm, fl) T(flag_##nm) { ID3D10Device *d = NULL; NEED10(); NEED_DRV(); \
    HRESULT hr = dd_make(fl, &d); EXPECT_HR_OK(hr); \
    if (SUCCEEDED(hr)) { CHECK(d != NULL); EXPECT_EQ(ID3D10Device_Release(d), 0); } }
FLAGOK(singlethreaded, D3D10_CREATE_DEVICE_SINGLETHREADED)
FLAGOK(prevent_internal_threading, D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS)
FLAGOK(prevent_altering_layer_settings, D3D10_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY)
FLAGOK(singlethreaded_prevent, D3D10_CREATE_DEVICE_SINGLETHREADED | D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS)

#define FLAGOBS(nm, fl) T(flag_##nm##_observed) { ID3D10Device *d = NULL; NEED10(); NEED_DRV(); \
    HRESULT hr = dd_make(fl, &d); t_note(#nm ": %s", d3dh_hr_name(hr)); \
    if (SUCCEEDED(hr)) { CHECK(d != NULL); EXPECT_EQ(ID3D10Device_Release(d), 0); } else CHECK(d == NULL); }
FLAGOBS(bgra, D3D10_CREATE_DEVICE_BGRA_SUPPORT)
FLAGOBS(allow_null_from_map, D3D10_CREATE_DEVICE_ALLOW_NULL_FROM_MAP)
FLAGOBS(strict_validation, D3D10_CREATE_DEVICE_STRICT_VALIDATION)
FLAGOBS(debuggable, D3D10_CREATE_DEVICE_DEBUGGABLE)
FLAGOBS(debug, D3D10_CREATE_DEVICE_DEBUG)
FLAGOBS(switch_to_ref, D3D10_CREATE_DEVICE_SWITCH_TO_REF)
FLAGOBS(unknown_0x40, 0x40)
FLAGOBS(unknown_0x100, 0x100)
FLAGOBS(unknown_0x80000000, 0x80000000u)

T(flag_debug_failure_code)
{
    ID3D10Device *d = NULL;
    NEED10(); NEED_DRV();
    HRESULT hr = dd_make(D3D10_CREATE_DEVICE_DEBUG, &d);
    if (SUCCEEDED(hr)) {
        ID3D10InfoQueue *iq = NULL; ID3D10Debug *dbg = NULL;
        EXPECT_HR_OK(ID3D10Device_QueryInterface(d, &IID_ID3D10InfoQueue, (void **)&iq));
        EXPECT_HR_OK(ID3D10Device_QueryInterface(d, &IID_ID3D10Debug, (void **)&dbg));
        SAFE_RELEASE(iq); SAFE_RELEASE(dbg);
        ID3D10Device_Release(d);
        return;
    }
    CHECK(d == NULL);
    /* Graphics Tools missing: documented failure is DXGI_ERROR_SDK_COMPONENT_MISSING */
    t_note("debug layer device not created: %s", d3dh_hr_name(hr));
    CHECK(hr == (HRESULT)0x887A002D || hr == E_FAIL || hr == E_INVALIDARG || hr == DXGI_ERROR_UNSUPPORTED || hr == E_NOINTERFACE);
}
T(flag_bgra_format_support)
{
    ID3D10Device *d = NULL; UINT s = 0;
    NEED10(); NEED_DRV();
    if (FAILED(dd_make(D3D10_CREATE_DEVICE_BGRA_SUPPORT, &d))) { t_skip("BGRA flag not accepted"); return; }
    EXPECT_HR_OK(ID3D10Device_CheckFormatSupport(d, DXGI_FORMAT_B8G8R8A8_UNORM, &s));
    CHECK(s & D3D10_FORMAT_SUPPORT_RENDER_TARGET);
    CHECK(s & D3D10_FORMAT_SUPPORT_TEXTURE2D);
    ID3D10Device_Release(d);
}
T(flag_singlethreaded_multithread_observed)
{
    ID3D10Device *d = NULL; ID3D10Multithread *mt = NULL;
    NEED10(); NEED_DRV();
    if (FAILED(dd_make(D3D10_CREATE_DEVICE_SINGLETHREADED, &d))) { t_skip("no device"); return; }
    if (SUCCEEDED(ID3D10Device_QueryInterface(d, &IID_ID3D10Multithread, (void **)&mt)))
        { t_note("SINGLETHREADED device: GetMultithreadProtected=%d", ID3D10Multithread_GetMultithreadProtected(mt)); ID3D10Multithread_Release(mt); }
    else t_note("SINGLETHREADED device: no ID3D10Multithread");
    ID3D10Device_Release(d);
}

/* ---- D3D10CreateDevice1 feature levels ---- */
static const UINT g_levels[] = { 0xa100, 0xa000, 0x9300, 0x9200, 0x9100 };
#define LEVEL(nm, v) T(level1_##nm) { ID3D10Device1 *d = NULL; NEED10_1(); NEED_DRV(); \
    HRESULT hr = dd_make1(0, (D3D10_FEATURE_LEVEL1)v, &d); \
    if (FAILED(hr)) { CHECK(d == NULL); t_note("level " #nm " not creatable: %s", d3dh_hr_name(hr)); return; } \
    EXPECT_EQ(ID3D10Device1_GetFeatureLevel(d), v); \
    ID3D10Device *base = NULL; EXPECT_HR_OK(ID3D10Device1_QueryInterface(d, &IID_ID3D10Device, (void **)&base)); SAFE_RELEASE(base); \
    EXPECT_EQ(ID3D10Device1_Release(d), 0); }
LEVEL(10_1, 0xa100) LEVEL(10_0, 0xa000) LEVEL(9_3, 0x9300) LEVEL(9_2, 0x9200) LEVEL(9_1, 0x9100)

T(level1_10_0_always_works_when_any_works)
{
    ID3D10Device1 *d = NULL;
    NEED10_1(); NEED_DRV();
    EXPECT_HR_OK(dd_make1(0, D3D10_FEATURE_LEVEL_10_0, &d));
    if (d) ID3D10Device1_Release(d);
}
#define BADFL(nm, v) T(level1_invalid_##nm) { NEED10_1(); NEED_DRV(); \
    fail10_1("feature level " #v, 0, NULL, DRV(), NULL, 0, v, D3D10_1_SDK_VERSION); }
BADFL(zero, 0) BADFL(one, 1) BADFL(11_0, 0xb000) BADFL(9_0, 0x9000) BADFL(10_2, 0xa200) BADFL(garbage, 0xdeadbeef)
BADFL(0x8000, 0x8000) BADFL(max, 0xffffffffu) BADFL(9_4, 0x9400) BADFL(10_0plus1, 0xa001)
T(level1_zero_is_einvalidarg)
{
    NEED10_1(); NEED_DRV();
    fail10_1("feature level 0", E_INVALIDARG, NULL, DRV(), NULL, 0, 0, D3D10_1_SDK_VERSION);
}
T(level1_invalid_for_each_driver)
{
    static const UINT d[] = { D3D10_DRIVER_TYPE_HARDWARE, D3D10_DRIVER_TYPE_REFERENCE, D3D10_DRIVER_TYPE_NULL, D3D10_DRIVER_TYPE_WARP };
    NEED10_1();
    for (int i = 0; i < 4; i++) fail10_1("feature level 0, all drivers", E_INVALIDARG, NULL, d[i], NULL, 0, 0, D3D10_1_SDK_VERSION);
}
T(level1_is_independent_of_create_device)
{
    /* D3D10CreateDevice yields a 10.0 device: through ID3D11Device (if present) the level is 10_0 */
    ID3D11Device *d11 = NULL;
    DD_DEV(dev);
    if (SUCCEEDED(ID3D10Device_QueryInterface(dev, &IID_ID3D11Device, (void **)&d11))) {
        EXPECT_EQ(ID3D11Device_GetFeatureLevel(d11), D3D_FEATURE_LEVEL_10_0);
        ID3D11Device_Release(d11);
    } else t_note("D3D10 device does not expose ID3D11Device");
    DD_END(dev);
}
T(level1_device_from_create_device_qi_device1)
{
    ID3D10Device1 *d1 = NULL;
    DD_DEV(dev);
    HRESULT hr = ID3D10Device_QueryInterface(dev, &IID_ID3D10Device1, (void **)&d1);
    t_note("QI ID3D10Device1 on D3D10CreateDevice device: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { EXPECT_EQ(ID3D10Device1_GetFeatureLevel(d1), D3D10_FEATURE_LEVEL_10_0); ID3D10Device1_Release(d1); }
    DD_END(dev);
}

/* ---- interfaces of the device ---- */
static void qi_expect(IUnknown *u, const IID *iid, int want, const char *name)
{
    void *p = NULL;
    HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    if (want) t_check(hr == S_OK && p, __FILE__, __LINE__, name, "QI got %s", d3dh_hr_name(hr));
    else t_check(hr == E_NOINTERFACE && !p, __FILE__, __LINE__, name, "QI got %s p=%p", d3dh_hr_name(hr), p);
    if (p) IUnknown_Release((IUnknown *)p);
}
static void qi_note(IUnknown *u, const IID *iid, const char *name)
{
    void *p = NULL;
    HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    t_note("QI %s: %s", name, d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
}
T(iface_matrix_core)
{
    DD_DEV(dev);
    IUnknown *u = (IUnknown *)dev;
    qi_expect(u, &IID_IUnknown, 1, "IUnknown");
    qi_expect(u, &IID_ID3D10Device, 1, "ID3D10Device");
    qi_expect(u, &IID_IDXGIObject, 1, "IDXGIObject");
    qi_expect(u, &IID_IDXGIDevice, 1, "IDXGIDevice");
    qi_expect(u, &IID_ID3D10Multithread, 1, "ID3D10Multithread");
    DD_END(dev);
}
T(iface_matrix_negative)
{
    DD_DEV(dev);
    IUnknown *u = (IUnknown *)dev;
    qi_expect(u, &IID_ID3D10InfoQueue, 0, "ID3D10InfoQueue (non-debug)");
    qi_expect(u, &IID_ID3D10Debug, 0, "ID3D10Debug (non-debug)");
    qi_expect(u, &IID_IDXGISwapChain, 0, "IDXGISwapChain");
    qi_expect(u, &IID_IDXGIFactory, 0, "IDXGIFactory");
    qi_expect(u, &IID_IDXGIAdapter, 0, "IDXGIAdapter");
    qi_expect(u, &IID_ID3D10Texture2D, 0, "ID3D10Texture2D");
    qi_expect(u, &IID_ID3D10Buffer, 0, "ID3D10Buffer");
    qi_expect(u, &IID_ID3D10Resource, 0, "ID3D10Resource");
    qi_expect(u, &IID_ID3D10Asynchronous, 0, "ID3D10Asynchronous");
    qi_expect(u, &IID_ID3D10DeviceChild, 0, "ID3D10DeviceChild");
    DD_END(dev);
}
T(iface_matrix_observed)
{
    DD_DEV(dev);
    IUnknown *u = (IUnknown *)dev;
    qi_note(u, &IID_IDXGIDevice1, "IDXGIDevice1");
    qi_note(u, &IID_ID3D10Device1, "ID3D10Device1");
    qi_note(u, &IID_ID3D11Device, "ID3D11Device");
    qi_note(u, &IID_ID3D10SwitchToRef, "ID3D10SwitchToRef");
    qi_note(u, &IID_IDXGIDevice2, "IDXGIDevice2");
    DD_END(dev);
}
T(iface_null_out_pointer)
{
    DD_DEV(dev);
    HRESULT hr = ID3D10Device_QueryInterface(dev, &IID_ID3D10Device, NULL);
    EXPECT_HR(hr, E_POINTER);
    DD_END(dev);
}
T(iface_qi_zero_guid)
{
    void *p = (void *)1;
    DD_DEV(dev);
    HRESULT hr = ID3D10Device_QueryInterface(dev, &GUID_NULL, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    DD_END(dev);
}
T(iface_refcount_qi_shares_count)
{
    DD_DEV(dev);
    IDXGIDevice *dx = NULL; ID3D10Multithread *mt = NULL;
    ULONG base = d3dh_refcount(dev);
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dx));
    EXPECT_EQ(d3dh_refcount(dev), base + 1);
    EXPECT_EQ(d3dh_refcount(dx), base + 1);
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_ID3D10Multithread, (void **)&mt));
    EXPECT_EQ(d3dh_refcount(dev), base + 2);
    ID3D10Multithread_Release(mt);
    IDXGIDevice_Release(dx);
    EXPECT_EQ(d3dh_refcount(dev), base);
    DD_END(dev);
}
T(iface_iunknown_identity)
{
    IUnknown *u1 = NULL, *u2 = NULL; IDXGIDevice *dx = NULL;
    DD_DEV(dev);
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_IUnknown, (void **)&u1));
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dx));
    EXPECT_HR_OK(IDXGIDevice_QueryInterface(dx, &IID_IUnknown, (void **)&u2));
    CHECK(u1 == u2);
    SAFE_RELEASE(u1); SAFE_RELEASE(u2); SAFE_RELEASE(dx);
    DD_END(dev);
}
T(iface_dxgi_parent_chain)
{
    IDXGIDevice *dx = NULL; IDXGIAdapter *ad = NULL; IDXGIFactory *fa = NULL;
    DD_DEV(dev);
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dx));
    if (dx) {
        EXPECT_HR_OK(IDXGIDevice_GetParent(dx, &IID_IDXGIAdapter, (void **)&ad));
        if (ad) {
            EXPECT_HR_OK(IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory, (void **)&fa));
            SAFE_RELEASE(fa);
            DXGI_ADAPTER_DESC desc;
            EXPECT_HR_OK(IDXGIAdapter_GetDesc(ad, &desc));
            t_note("adapter: vendor %04x device %04x", desc.VendorId, desc.DeviceId);
            IDXGIAdapter_Release(ad);
        }
        IDXGIDevice_Release(dx);
    }
    DD_END(dev);
}
T(iface_dxgi_get_adapter)
{
    IDXGIDevice *dx = NULL; IDXGIAdapter *ad = NULL;
    DD_DEV(dev);
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dx));
    if (dx) {
        EXPECT_HR_OK(IDXGIDevice_GetAdapter(dx, &ad));
        CHECK(ad != NULL);
        EXPECT_HR(IDXGIDevice_GetAdapter(dx, NULL), DXGI_ERROR_INVALID_CALL);
        SAFE_RELEASE(ad);
        IDXGIDevice_Release(dx);
    }
    DD_END(dev);
}
T(iface_multithread_default_protected)
{
    ID3D10Multithread *mt = NULL;
    DD_DEV(dev);
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_ID3D10Multithread, (void **)&mt));
    if (mt) {
        EXPECT_EQ(ID3D10Multithread_GetMultithreadProtected(mt), TRUE);
        EXPECT_EQ(ID3D10Multithread_SetMultithreadProtected(mt, FALSE), TRUE);
        EXPECT_EQ(ID3D10Multithread_GetMultithreadProtected(mt), FALSE);
        EXPECT_EQ(ID3D10Multithread_SetMultithreadProtected(mt, TRUE), FALSE);
        EXPECT_EQ(ID3D10Multithread_SetMultithreadProtected(mt, TRUE), TRUE);
        EXPECT_EQ(ID3D10Multithread_GetMultithreadProtected(mt), TRUE);
        ID3D10Multithread_Enter(mt);
        ID3D10Multithread_Enter(mt);
        ID3D10Multithread_Leave(mt);
        ID3D10Multithread_Leave(mt);
        ID3D10Multithread_Release(mt);
    }
    DD_END(dev);
}
T(iface_multithread_nonbool_value)
{
    ID3D10Multithread *mt = NULL;
    DD_DEV(dev);
    if (SUCCEEDED(ID3D10Device_QueryInterface(dev, &IID_ID3D10Multithread, (void **)&mt))) {
        ID3D10Multithread_SetMultithreadProtected(mt, 5);
        BOOL v = ID3D10Multithread_GetMultithreadProtected(mt);
        t_note("SetMultithreadProtected(5) then Get: %d", v);
        CHECK(v != 0);
        ID3D10Multithread_SetMultithreadProtected(mt, TRUE);
        ID3D10Multithread_Release(mt);
    }
    DD_END(dev);
}
T(iface_multithread_shared_with_d3d11_context)
{
    ID3D11Device *d11 = NULL; ID3D10Multithread *mt = NULL; ID3D11DeviceContext *ctx = NULL; ID3D11Multithread *m11 = NULL;
    DD_DEV(dev);
    if (FAILED(ID3D10Device_QueryInterface(dev, &IID_ID3D11Device, (void **)&d11))) { DD_END(dev); t_skip("no ID3D11Device"); return; }
    ID3D11Device_GetImmediateContext(d11, &ctx);
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_ID3D10Multithread, (void **)&mt));
    if (SUCCEEDED(ID3D11DeviceContext_QueryInterface(ctx, &IID_ID3D11Multithread, (void **)&m11)) && mt) {
        EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(m11), ID3D10Multithread_GetMultithreadProtected(mt));
        ID3D10Multithread_SetMultithreadProtected(mt, FALSE);
        EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(m11), FALSE);
        ID3D10Multithread_SetMultithreadProtected(mt, TRUE);
        ID3D11Multithread_Release(m11);
    }
    SAFE_RELEASE(mt); SAFE_RELEASE(ctx); ID3D11Device_Release(d11);
    DD_END(dev);
}

/* ---- device created via CreateDevice1: interface matrix ---- */
T(iface1_matrix)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_0);
    IUnknown *u = (IUnknown *)dev;
    qi_expect(u, &IID_IUnknown, 1, "IUnknown");
    qi_expect(u, &IID_ID3D10Device, 1, "ID3D10Device");
    qi_expect(u, &IID_ID3D10Device1, 1, "ID3D10Device1");
    qi_expect(u, &IID_IDXGIObject, 1, "IDXGIObject");
    qi_expect(u, &IID_IDXGIDevice, 1, "IDXGIDevice");
    qi_expect(u, &IID_ID3D10InfoQueue, 0, "ID3D10InfoQueue (non-debug)");
    qi_note(u, &IID_IDXGIDevice1, "IDXGIDevice1");
    qi_note(u, &IID_ID3D10Multithread, "ID3D10Multithread");
    qi_note(u, &IID_ID3D11Device, "ID3D11Device");
    EXPECT_EQ(ID3D10Device1_Release(dev), 0);
}
T(iface1_featurelevel_d3d11_view)
{
    ID3D11Device *d11 = NULL;
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_1);
    if (SUCCEEDED(ID3D10Device1_QueryInterface(dev, &IID_ID3D11Device, (void **)&d11))) {
        EXPECT_EQ(ID3D11Device_GetFeatureLevel(d11), D3D_FEATURE_LEVEL_10_1);
        ID3D11Device_Release(d11);
    }
    ID3D10Device1_Release(dev);
}
T(iface1_levels_9_via_d3d11)
{
    static const UINT l10[] = { 0x9100, 0x9200, 0x9300, 0xa000, 0xa100 };
    static const D3D_FEATURE_LEVEL l11[] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1 };
    NEED10_1(); NEED_DRV();
    for (int i = 0; i < 5; i++) {
        ID3D10Device1 *d = NULL; ID3D11Device *d11 = NULL;
        if (FAILED(dd_make1(0, (D3D10_FEATURE_LEVEL1)l10[i], &d))) continue;
        if (SUCCEEDED(ID3D10Device1_QueryInterface(d, &IID_ID3D11Device, (void **)&d11))) {
            EXPECT_EQ(ID3D11Device_GetFeatureLevel(d11), l11[i]);
            ID3D11Device_Release(d11);
        }
        ID3D10Device1_Release(d);
    }
}
T(iface1_reports_same_level_repeatedly)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_0);
    for (int i = 0; i < 3; i++) EXPECT_EQ(ID3D10Device1_GetFeatureLevel(dev), D3D10_FEATURE_LEVEL_10_0);
    ID3D10Device1_Release(dev);
}

/* ---- device removal / basic accessors ---- */
T(device_removed_reason_ok)
{
    DD_DEV(dev);
    EXPECT_HR(ID3D10Device_GetDeviceRemovedReason(dev), S_OK);
    EXPECT_HR(ID3D10Device_GetDeviceRemovedReason(dev), S_OK);
    DD_END(dev);
}
T(device_creation_flags_roundtrip)
{
    ID3D10Device *d = NULL;
    NEED10(); NEED_DRV();
    static const UINT fl[] = { 0, D3D10_CREATE_DEVICE_SINGLETHREADED, D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
        D3D10_CREATE_DEVICE_SINGLETHREADED | D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS };
    for (int i = 0; i < 4; i++) {
        if (FAILED(dd_make(fl[i], &d))) continue;
        UINT got = ID3D10Device_GetCreationFlags(d);
        CHECKF((got & fl[i]) == fl[i], "flags 0x%x -> 0x%x", fl[i], got);
        ID3D10Device_Release(d);
    }
}
T(device_creation_flags_default_zero)
{
    DD_DEV(dev);
    UINT f = ID3D10Device_GetCreationFlags(dev);
    t_note("GetCreationFlags(0) = 0x%x", f);
    CHECK((f & D3D10_CREATE_DEVICE_SINGLETHREADED) == 0);
    DD_END(dev);
}
T(device_creation_flags_debuggable_observed)
{
    DD_DEV(dev);
    t_note("GetCreationFlags = 0x%x", ID3D10Device_GetCreationFlags(dev));
    DD_END(dev);
}
T(device_many_create_release)
{
    NEED10(); NEED_DRV();
    for (int i = 0; i < 8; i++) {
        ID3D10Device *d = NULL;
        EXPECT_HR_OK(dd_make(0, &d));
        if (!d) return;
        EXPECT_EQ(ID3D10Device_Release(d), 0);
    }
}
T(device_two_devices_distinct)
{
    ID3D10Device *a = NULL, *b = NULL;
    NEED10(); NEED_DRV();
    EXPECT_HR_OK(dd_make(0, &a)); EXPECT_HR_OK(dd_make(0, &b));
    if (a && b) CHECK(a != b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

/* ---- device and swap chain ---- */
static void fill_sc(DXGI_SWAP_CHAIN_DESC *d, HWND w)
{
    memset(d, 0, sizeof *d);
    d->BufferDesc.Width = 64; d->BufferDesc.Height = 48;
    d->BufferDesc.RefreshRate.Numerator = 60; d->BufferDesc.RefreshRate.Denominator = 1;
    d->BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d->SampleDesc.Count = 1;
    d->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d->BufferCount = 1;
    d->OutputWindow = w;
    d->Windowed = TRUE;
    d->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
}
#define NEED_SC() do { REQUIRE(DD_C10SC()); NEED_DRV(); } while (0)
#define NEED_SC1() do { REQUIRE(DD_C10SC1()); NEED_DRV(); } while (0)

T(sc_null_everything)
{
    NEED_SC();
    EXPECT_HR(DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, NULL, NULL, NULL), E_INVALIDARG);
}
T(sc_desc_without_outputs)
{
    DXGI_SWAP_CHAIN_DESC d; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, w);
    EXPECT_HR(DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, NULL, NULL), E_INVALIDARG);
    d3dh_destroy_window(w);
}
T(sc_no_desc_device_only)
{
    ID3D10Device *dev = NULL;
    NEED_SC();
    HRESULT hr = DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, NULL, NULL, &dev);
    EXPECT_HR_OK(hr);
    if (dev) EXPECT_EQ(ID3D10Device_Release(dev), 0);
}
T(sc_desc_null_swapchain_ptr_device_only)
{
    ID3D10Device *dev = NULL; DXGI_SWAP_CHAIN_DESC d; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, NULL);
    HRESULT hr = DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, NULL, &dev);
    EXPECT_HR_OK(hr);
    if (dev) EXPECT_EQ(ID3D10Device_Release(dev), 0);
    d3dh_destroy_window(w);
}
T(sc_swapchain_requested_no_device_ptr)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0xdeadbeef; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, w);
    HRESULT hr = DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, &sc, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    CHECK(sc == NULL);
    d3dh_destroy_window(w);
}
T(sc_null_window_with_swapchain)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0xdeadbeef; ID3D10Device *dev = DD_SENT_DEV;
    NEED_SC();
    fill_sc(&d, NULL);
    HRESULT hr = DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, &sc, &dev);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    CHECK(sc == NULL);
    CHECK(dev == NULL);
}
T(sc_bad_format_bc5)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0xdeadbeef; ID3D10Device *dev = DD_SENT_DEV; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, w);
    d.BufferDesc.Format = DXGI_FORMAT_BC5_UNORM;
    HRESULT hr = DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, &sc, &dev);
    EXPECT_HR(hr, E_INVALIDARG);
    CHECK(sc == NULL);
    CHECK(dev == NULL);
    d3dh_destroy_window(w);
}
T(sc_bad_sdk_version)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0xdeadbeef; ID3D10Device *dev = DD_SENT_DEV; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, w);
    HRESULT hr = DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, 30, &d, &sc, &dev);
    EXPECT_HR(hr, E_INVALIDARG);
    if (sc || dev) t_note("outputs after bad sdk: %p %p", sc, dev);
    d3dh_destroy_window(w);
}
T(sc_create_and_roundtrip)
{
    DXGI_SWAP_CHAIN_DESC d, o; IDXGISwapChain *sc = NULL; ID3D10Device *dev = NULL; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, w);
    HRESULT hr = DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, &sc, &dev);
    if (FAILED(hr)) { t_skip("swap chain creation failed (%s)", d3dh_hr_name(hr)); d3dh_destroy_window(w); return; }
    CHECK(sc && dev);
    memset(&o, 0xcc, sizeof o);
    EXPECT_HR_OK(IDXGISwapChain_GetDesc(sc, &o));
    EXPECT_EQ(o.BufferDesc.Width, d.BufferDesc.Width);
    EXPECT_EQ(o.BufferDesc.Height, d.BufferDesc.Height);
    EXPECT_EQ(o.BufferDesc.Format, d.BufferDesc.Format);
    EXPECT_EQ(o.BufferDesc.ScanlineOrdering, 0);
    EXPECT_EQ(o.BufferDesc.Scaling, 0);
    EXPECT_EQ(o.SampleDesc.Count, 1);
    EXPECT_EQ(o.SampleDesc.Quality, 0);
    EXPECT_EQ(o.BufferUsage, d.BufferUsage);
    EXPECT_EQ(o.BufferCount, 1);
    EXPECT_EQ(o.OutputWindow, w);
    EXPECT_EQ(o.Windowed, TRUE);
    EXPECT_EQ(o.SwapEffect, DXGI_SWAP_EFFECT_DISCARD);
    EXPECT_EQ(o.Flags, 0);
    t_note("refresh rate in swap chain desc: %u/%u", o.BufferDesc.RefreshRate.Numerator, o.BufferDesc.RefreshRate.Denominator);
    IDXGISwapChain1 *sc1 = NULL;
    HRESULT h1 = IDXGISwapChain_QueryInterface(sc, &IID_IDXGISwapChain1, (void **)&sc1);
    t_note("swap chain QI IDXGISwapChain1: %s", d3dh_hr_name(h1));
    SAFE_RELEASE(sc1);
    EXPECT_EQ(IDXGISwapChain_Release(sc), 0);
    EXPECT_EQ(ID3D10Device_Release(dev), 0);
    d3dh_destroy_window(w);
}
T(sc_backbuffer_texture)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = NULL; ID3D10Device *dev = NULL; ID3D10Texture2D *bb = NULL; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, w);
    if (FAILED(DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, &sc, &dev))) { t_skip("no swap chain"); d3dh_destroy_window(w); return; }
    EXPECT_HR_OK(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D10Texture2D, (void **)&bb));
    if (bb) {
        D3D10_TEXTURE2D_DESC td;
        ID3D10Texture2D_GetDesc(bb, &td);
        EXPECT_EQ(td.Width, 64); EXPECT_EQ(td.Height, 48);
        EXPECT_EQ(td.MipLevels, 1); EXPECT_EQ(td.ArraySize, 1);
        EXPECT_EQ(td.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
        EXPECT_EQ(td.SampleDesc.Count, 1);
        EXPECT_EQ(td.Usage, D3D10_USAGE_DEFAULT);
        CHECK(td.BindFlags & D3D10_BIND_RENDER_TARGET);
        t_note("backbuffer BindFlags 0x%x MiscFlags 0x%x CPU 0x%x", td.BindFlags, td.MiscFlags, td.CPUAccessFlags);
        ID3D10RenderTargetView *rtv = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, (ID3D10Resource *)bb, NULL, &rtv));
        SAFE_RELEASE(rtv);
        ID3D10Texture2D_Release(bb);
    }
    void *none = (void *)1;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 1, &IID_ID3D10Texture2D, &none), DXGI_ERROR_INVALID_CALL);
    CHECK(none == NULL);
    IDXGISwapChain_Release(sc); ID3D10Device_Release(dev);
    d3dh_destroy_window(w);
}
T(sc_get_device)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = NULL; ID3D10Device *dev = NULL, *got = NULL; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, w);
    if (FAILED(DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, &sc, &dev))) { t_skip("no swap chain"); d3dh_destroy_window(w); return; }
    EXPECT_HR_OK(IDXGISwapChain_GetDevice(sc, &IID_ID3D10Device, (void **)&got));
    CHECK(got == dev);
    SAFE_RELEASE(got);
    IDXGISwapChain_Release(sc); ID3D10Device_Release(dev);
    d3dh_destroy_window(w);
}
T(sc_present_hidden)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = NULL; ID3D10Device *dev = NULL; HWND w = d3dh_create_hidden_window();
    NEED_SC();
    fill_sc(&d, w);
    if (FAILED(DD_C10SC()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_SDK_VERSION, &d, &sc, &dev))) { t_skip("no swap chain"); d3dh_destroy_window(w); return; }
    HRESULT hr = IDXGISwapChain_Present(sc, 0, 0);
    CHECKF(SUCCEEDED(hr), "Present -> %s", d3dh_hr_name(hr));
    IDXGISwapChain_Release(sc); ID3D10Device_Release(dev);
    d3dh_destroy_window(w);
}

/* CreateDeviceAndSwapChain1 */
T(sc1_null_everything)
{
    NEED_SC1();
    EXPECT_HR(DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, NULL, NULL, NULL), E_INVALIDARG);
}
T(sc1_no_desc_device_only)
{
    ID3D10Device1 *dev = NULL;
    NEED_SC1();
    EXPECT_HR_OK(DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, NULL, NULL, &dev));
    if (dev) EXPECT_EQ(ID3D10Device1_Release(dev), 0);
}
T(sc1_desc_without_outputs)
{
    DXGI_SWAP_CHAIN_DESC d; HWND w = d3dh_create_hidden_window();
    NEED_SC1();
    fill_sc(&d, w);
    EXPECT_HR(DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d, NULL, NULL), E_INVALIDARG);
    d3dh_destroy_window(w);
}
T(sc1_zero_level)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0xdeadbeef; ID3D10Device1 *dev = (ID3D10Device1 *)DD_SENT_DEV; HWND w = d3dh_create_hidden_window();
    NEED_SC1();
    fill_sc(&d, w);
    EXPECT_HR(DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, 0, D3D10_1_SDK_VERSION, &d, &sc, &dev), E_INVALIDARG);
    CHECK(sc == NULL); CHECK(dev == NULL);
    d3dh_destroy_window(w);
}
T(sc1_swapchain_without_device)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0xdeadbeef; HWND w = d3dh_create_hidden_window();
    NEED_SC1();
    fill_sc(&d, w);
    EXPECT_HR(DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d, &sc, NULL), E_INVALIDARG);
    CHECK(sc == NULL);
    d3dh_destroy_window(w);
}
T(sc1_null_window)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0xdeadbeef; ID3D10Device1 *dev = (ID3D10Device1 *)DD_SENT_DEV;
    NEED_SC1();
    fill_sc(&d, NULL);
    EXPECT_HR(DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d, &sc, &dev), DXGI_ERROR_INVALID_CALL);
    CHECK(sc == NULL); CHECK(dev == NULL);
}
T(sc1_null_window_device_only)
{
    DXGI_SWAP_CHAIN_DESC d; ID3D10Device1 *dev = NULL;
    NEED_SC1();
    fill_sc(&d, NULL);
    EXPECT_HR_OK(DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d, NULL, &dev));
    if (dev) EXPECT_EQ(ID3D10Device1_Release(dev), 0);
}
T(sc1_bc5_format)
{
    DXGI_SWAP_CHAIN_DESC d; IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0xdeadbeef; ID3D10Device1 *dev = (ID3D10Device1 *)DD_SENT_DEV; HWND w = d3dh_create_hidden_window();
    NEED_SC1();
    fill_sc(&d, w);
    d.BufferDesc.Format = DXGI_FORMAT_BC5_UNORM;
    EXPECT_HR(DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d, &sc, &dev), E_INVALIDARG);
    CHECK(sc == NULL); CHECK(dev == NULL);
    d3dh_destroy_window(w);
}
T(sc1_create_and_level)
{
    DXGI_SWAP_CHAIN_DESC d, o; IDXGISwapChain *sc = NULL; ID3D10Device1 *dev = NULL; HWND w = d3dh_create_hidden_window();
    NEED_SC1();
    fill_sc(&d, w);
    HRESULT hr = DD_C10SC1()(NULL, (D3D10_DRIVER_TYPE)DRV(), NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d, &sc, &dev);
    if (FAILED(hr)) { t_skip("no swap chain (%s)", d3dh_hr_name(hr)); d3dh_destroy_window(w); return; }
    EXPECT_EQ(ID3D10Device1_GetFeatureLevel(dev), D3D10_FEATURE_LEVEL_10_0);
    EXPECT_HR_OK(IDXGISwapChain_GetDesc(sc, &o));
    EXPECT_EQ(o.BufferDesc.Width, 64); EXPECT_EQ(o.BufferDesc.Height, 48);
    EXPECT_EQ(o.OutputWindow, w);
    IDXGISwapChain1 *sc1 = NULL;
    EXPECT_HR_OK(IDXGISwapChain_QueryInterface(sc, &IID_IDXGISwapChain1, (void **)&sc1));
    SAFE_RELEASE(sc1);
    EXPECT_EQ(IDXGISwapChain_Release(sc), 0);
    EXPECT_EQ(ID3D10Device1_Release(dev), 0);
    d3dh_destroy_window(w);
}

/* ---- version / core stubs ---- */
UDECL(UINT64, D3D10GetVersion, (void));
UDECL(UINT64, D3D10CoreGetVersion, (void));
UDECL(HRESULT, D3D10CoreCreateDevice, (void));
UDECL(HRESULT, D3D10CoreGetSupportedVersions, (void));
UDECL(HRESULT, D3D10CoreRegisterLayers, (void));

T(version_nonzero_stable)
{
    if (t_is_wine()) { t_skip("emulator export differs"); return; }
    D3D10GetVersion_pfn f = (D3D10GetVersion_pfn)t_proc_in("d3d10.dll", "D3D10GetVersion");
    REQUIRE(f);
    UINT64 a = f(), b = f();
    t_note("D3D10GetVersion = 0x%llx", (unsigned long long)a);
    CHECK(a != 0);
    EXPECT_EQ(a, b);
}
T(version_matches_core)
{
    if (t_is_wine()) { t_skip("emulator export differs"); return; }
    D3D10GetVersion_pfn f = (D3D10GetVersion_pfn)t_proc_in("d3d10.dll", "D3D10GetVersion");
    D3D10CoreGetVersion_pfn g = (D3D10CoreGetVersion_pfn)t_proc_in("d3d10core.dll", "D3D10CoreGetVersion");
    REQUIRE(f); REQUIRE(g);
    EXPECT_EQ(f(), g());
}
T(version_high_dword)
{
    if (t_is_wine()) { t_skip("emulator export differs"); return; }
    D3D10GetVersion_pfn f = (D3D10GetVersion_pfn)t_proc_in("d3d10.dll", "D3D10GetVersion");
    REQUIRE(f);
    UINT64 v = f();
    /* observed on Windows 10: 0x000a000100041770 -> major 10.0, minor 1 in the high dword; the low part is a build number */
    EXPECT_EQ(v >> 32, 0xa0001);
}
T(core_create_device_stub)
{
    if (t_is_wine()) { t_skip("emulator export differs"); return; }
    D3D10CoreCreateDevice_pfn f = (D3D10CoreCreateDevice_pfn)t_proc_in("d3d10core.dll", "D3D10CoreCreateDevice");
    REQUIRE(f);
    EXPECT_HR(f(), E_NOTIMPL);
}
T(core_supported_versions_stub)
{
    if (t_is_wine()) { t_skip("emulator export differs"); return; }
    D3D10CoreGetSupportedVersions_pfn f = (D3D10CoreGetSupportedVersions_pfn)t_proc_in("d3d10core.dll", "D3D10CoreGetSupportedVersions");
    REQUIRE(f);
    EXPECT_HR(f(), E_NOTIMPL);
}
T(core_register_layers_stub)
{
    if (t_is_wine()) { t_skip("emulator export differs"); return; }
    D3D10CoreRegisterLayers_pfn f = (D3D10CoreRegisterLayers_pfn)t_proc_in("d3d10core.dll", "D3D10CoreRegisterLayers");
    REQUIRE(f);
    EXPECT_HR(f(), E_NOTIMPL);
}
T(core_stub_exports_share_code)
{
    FARPROC a = t_proc_in("d3d10core.dll", "D3D10CoreCreateDevice");
    FARPROC b = t_proc_in("d3d10core.dll", "D3D10CoreGetSupportedVersions");
    FARPROC c = t_proc_in("d3d10core.dll", "D3D10CoreRegisterLayers");
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    t_note("stub addresses %p %p %p", a, b, c);
    CHECK(a == b && b == c);
}
T(exports_present)
{
    static const char *const d3d10[] = { "D3D10CreateDevice", "D3D10CreateDeviceAndSwapChain", "D3D10GetVersion", "D3D10CreateBlob",
        "D3D10RegisterLayers", "D3D10GetVertexShaderProfile", "D3D10GetPixelShaderProfile", "D3D10GetGeometryShaderProfile" };
    static const char *const d3d10_1[] = { "D3D10CreateDevice1", "D3D10CreateDeviceAndSwapChain1", "D3D10CreateBlob" };
    for (unsigned i = 0; i < sizeof d3d10 / sizeof *d3d10; i++)
        CHECKF(t_proc_in("d3d10.dll", d3d10[i]) != NULL, "d3d10.dll!%s", d3d10[i]);
    for (unsigned i = 0; i < sizeof d3d10_1 / sizeof *d3d10_1; i++)
        CHECKF(t_proc_in("d3d10_1.dll", d3d10_1[i]) != NULL, "d3d10_1.dll!%s", d3d10_1[i]);
}
T(d3d10_dll_has_no_createdevice1)
{
    /* CreateDevice1 lives in d3d10_1.dll only */
    REQUIRE(t_module("d3d10.dll"));
    CHECK(GetProcAddress(t_module("d3d10.dll"), "D3D10CreateDevice1") == NULL);
}
T(profile_strings)
{
    typedef LPCSTR (WINAPI *prof_fn)(ID3D10Device *);
    prof_fn v = (prof_fn)t_proc_in("d3d10.dll", "D3D10GetVertexShaderProfile");
    prof_fn p = (prof_fn)t_proc_in("d3d10.dll", "D3D10GetPixelShaderProfile");
    prof_fn g = (prof_fn)t_proc_in("d3d10.dll", "D3D10GetGeometryShaderProfile");
    REQUIRE(v); REQUIRE(p); REQUIRE(g);
    DD_DEV(dev);
    LPCSTR sv = v(dev), sp = p(dev), sg = g(dev);
    t_note("profiles: %s %s %s", sv ? sv : "(null)", sp ? sp : "(null)", sg ? sg : "(null)");
    if (sv) CHECK(!strncmp(sv, "vs_4_", 5));
    if (sp) CHECK(!strncmp(sp, "ps_4_", 5));
    if (sg) CHECK(!strncmp(sg, "gs_4_", 5));
    DD_END(dev);
}
