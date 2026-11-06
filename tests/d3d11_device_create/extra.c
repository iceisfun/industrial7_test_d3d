/* Additional exports of d3d11.dll (kernel-thunk stubs, performance markers, feature level upgrade, WinRT interop),
 * immediate context identity through the versioned device interfaces, and concurrent creation. */
#include "dc.h"
#include <inspectable.h>

#define T(n) TEST(d3d11_device_create_##n)
#define NEEDDEV() ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0; \
    REQUIRE(DC_FN()); DC_NEED_WARP(d, c, fl)

/* ---- exports with fixed ordinals ---- */
static const struct { const char *n; unsigned o; } ords[] = {
    { "D3D11CreateDeviceForD3D12", 1 }, { "EnableFeatureLevelUpgrade", 13 }, { "OpenAdapter10", 14 },
    { "OpenAdapter10_2", 15 }, { "CreateDirect3D11DeviceFromDXGIDevice", 16 }, { "CreateDirect3D11SurfaceFromDXGISurface", 17 },
    { "D3D11CoreCreateDevice", 18 }, { "D3D11CoreCreateLayeredDevice", 19 }, { "D3D11CoreGetLayeredDeviceSize", 20 },
    { "D3D11CoreRegisterLayers", 21 }, { "D3D11CreateDevice", 22 }, { "D3D11CreateDeviceAndSwapChain", 23 },
    { "D3D11On12CreateDevice", 24 }, { "D3DPerformance_BeginEvent", 48 }, { "D3DPerformance_EndEvent", 49 },
    { "D3DPerformance_GetStatus", 50 }, { "D3DPerformance_SetMarker", 51 } };

#define ORD(i, nm) T(export_ordinal_##nm) { HMODULE m = t_module("d3d11.dll"); REQUIRE(m); \
    FARPROC a = GetProcAddress(m, ords[i].n), b = GetProcAddress(m, (LPCSTR)(ULONG_PTR)ords[i].o); \
    CHECKF(a != NULL, "%s missing", ords[i].n); CHECKF(a == b, "%s: ordinal %u differs from name lookup", ords[i].n, ords[i].o); }
ORD(0, forD3D12) ORD(1, feature_level_upgrade) ORD(2, open_adapter10) ORD(3, open_adapter10_2)
ORD(4, interop_device) ORD(5, interop_surface) ORD(6, core_create_device) ORD(7, core_create_layered)
ORD(8, core_layered_size) ORD(9, core_register_layers) ORD(10, create_device) ORD(11, create_device_swapchain)
ORD(12, on12) ORD(13, perf_begin) ORD(14, perf_end) ORD(15, perf_status) ORD(16, perf_marker)

T(export_unassigned_ordinals_absent)
{
    HMODULE m = t_module("d3d11.dll");
    static const unsigned absent[] = { 52, 53, 100, 0x3e8, 0xffff };
    REQUIRE(m);
    for (unsigned i = 0; i < sizeof absent / sizeof *absent; i++)
        CHECKF(GetProcAddress(m, (LPCSTR)(ULONG_PTR)absent[i]) == NULL, "ordinal %u unexpectedly exported", absent[i]);
}

/* ---- D3DKMT* names exported by d3d11.dll are local thunks, not forwarders to gdi32 ---- */
static const char *kmt_empty[] = {   /* one shared body: `xor eax,eax; ret` */
    "D3DKMTCloseAdapter", "D3DKMTDestroyAllocation", "D3DKMTDestroyContext", "D3DKMTDestroyDevice",
    "D3DKMTDestroySynchronizationObject", "D3DKMTPresent", "D3DKMTSetDisplayPrivateDriverFormat",
    "D3DKMTSignalSynchronizationObject", "D3DKMTUnlock", "D3DKMTWaitForSynchronizationObject",
    "D3DKMTCreateAllocation", "D3DKMTCreateContext", "D3DKMTCreateDevice", "D3DKMTCreateSynchronizationObject",
    "D3DKMTGetContextSchedulingPriority", "D3DKMTGetDisplayModeList", "D3DKMTGetMultisampleMethodList",
    "D3DKMTGetRuntimeData", "D3DKMTGetSharedPrimaryHandle", "D3DKMTLock", "D3DKMTQueryAllocationResidency",
    "D3DKMTRender", "D3DKMTSetAllocationPriority", "D3DKMTSetContextSchedulingPriority", "D3DKMTSetDisplayMode",
    "D3DKMTSetGammaRamp", "D3DKMTSetVidPnSourceOwner", "D3DKMTWaitForVerticalBlankEvent" };
static const char *kmt_special[] = {   /* own bodies; the first two reject without looking at the argument */
    "D3DKMTOpenResource", "D3DKMTQueryResourceInfo", "D3DKMTQueryAdapterInfo", "D3DKMTEscape",
    "D3DKMTGetDeviceState", "D3DKMTOpenAdapterFromHdc" };
#define NKMT_EMPTY (sizeof kmt_empty / sizeof *kmt_empty)

T(export_d3dkmt_names_present)
{
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    for (unsigned i = 0; i < NKMT_EMPTY; i++)
        CHECKF(GetProcAddress(m, kmt_empty[i]) != NULL, "%s missing from d3d11.dll", kmt_empty[i]);
    for (unsigned i = 0; i < sizeof kmt_special / sizeof *kmt_special; i++)
        CHECKF(GetProcAddress(m, kmt_special[i]) != NULL, "%s missing from d3d11.dll", kmt_special[i]);
}
T(export_d3dkmt_not_forwarded)
{
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    for (unsigned i = 0; i < NKMT_EMPTY; i++) {
        HMODULE owner = NULL;
        FARPROC p = GetProcAddress(m, kmt_empty[i]);
        if (p && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner))
            CHECKF(owner == m, "%s resolves outside d3d11.dll", kmt_empty[i]);
    }
}
T(export_d3dkmt_empty_thunks_share_one_body)
{
    HMODULE m = t_module("d3d11.dll");
    FARPROC first;
    REQUIRE(m);
    first = GetProcAddress(m, kmt_empty[0]);
    REQUIRE(first);
    for (unsigned i = 1; i < NKMT_EMPTY; i++)
        CHECKF(GetProcAddress(m, kmt_empty[i]) == first, "%s is not the shared thunk", kmt_empty[i]);
}
T(export_d3dkmt_empty_thunks_return_success_ignoring_arguments)
{
    /* the body never touches its arguments (NULL is safe) and reports STATUS_SUCCESS */
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    if (t_is_wine()) { t_skip("unimplemented stubs abort under emulation"); return; }
    for (unsigned i = 0; i < NKMT_EMPTY; i++) {
        LONG (WINAPI *f)(void *) = (void *)GetProcAddress(m, kmt_empty[i]);
        if (!f) { CHECKF(0, "%s missing", kmt_empty[i]); continue; }
        CHECKF(f(NULL) == 0, "%s(NULL) != STATUS_SUCCESS", kmt_empty[i]);
    }
}
T(export_d3dkmt_open_resource_rejects)
{
    /* both entry points are `mov eax, STATUS_INVALID_PARAMETER; ret` */
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    if (t_is_wine()) { t_skip("unimplemented stubs abort under emulation"); return; }
    for (unsigned i = 0; i < 2; i++) {
        LONG (WINAPI *f)(void *) = (void *)GetProcAddress(m, kmt_special[i]);
        if (!f) { CHECKF(0, "%s missing", kmt_special[i]); continue; }
        CHECKF(f(NULL) == (LONG)0xC000000D, "%s(NULL) != STATUS_INVALID_PARAMETER", kmt_special[i]);
    }
    CHECK(GetProcAddress(m, kmt_special[0]) == GetProcAddress(m, kmt_special[1]));
}

/* ---- D3DPerformance_* : forward to a context object, NULL means no profiler attached ---- */
T(perf_begin_event_null_object)
{
    int (WINAPI *f)(void *, const WCHAR *) = (void *)t_proc_in("d3d11.dll", "D3DPerformance_BeginEvent");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("stub export under emulation"); return; }
    EXPECT_EQ(f(NULL, L"event"), -1);
    EXPECT_EQ(f(NULL, NULL), -1);
}
T(perf_end_event_null_object)
{
    int (WINAPI *f)(void *) = (void *)t_proc_in("d3d11.dll", "D3DPerformance_EndEvent");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("stub export under emulation"); return; }
    EXPECT_EQ(f(NULL), -1);
    EXPECT_EQ(f(NULL), -1);
}
T(perf_get_status_null_object)
{
    DWORD (WINAPI *f)(void *) = (void *)t_proc_in("d3d11.dll", "D3DPerformance_GetStatus");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("stub export under emulation"); return; }
    EXPECT_EQ(f(NULL), 0);
}
T(perf_set_marker_null_object)
{
    void (WINAPI *f)(void *, const WCHAR *) = (void *)t_proc_in("d3d11.dll", "D3DPerformance_SetMarker");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("stub export under emulation"); return; }
    f(NULL, L"marker");
    f(NULL, NULL);
    CHECK(1);   /* must return without touching the arguments */
}

T(enable_feature_level_upgrade_returns_true)
{
    int (WINAPI *f)(void) = (void *)t_proc_in("d3d11.dll", "EnableFeatureLevelUpgrade");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("stub export under emulation"); return; }
    EXPECT_EQ(f(), 1);
    EXPECT_EQ(f(), 1);
}

/* ---- WinRT interop ---- */
T(interop_device_from_dxgi_device)
{
    HRESULT (WINAPI *f)(IDXGIDevice *, IInspectable **) = (void *)t_proc_in("d3d11.dll", "CreateDirect3D11DeviceFromDXGIDevice");
    IDXGIDevice *dd = NULL; IInspectable *insp = NULL;
    NEEDDEV();
    REQUIRE(f);
    EXPECT_HR_OK(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd));
    if (dd) {
        HRESULT hr = f(dd, &insp);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) {
            CHECK(insp != NULL);
            if (insp) {
                void *access = NULL;
                /* IDXGIInterfaceAccess {A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1} */
                static const GUID iid_access = { 0xA9B3D012, 0x3DF2, 0x4EE3, { 0xB8, 0xD1, 0x86, 0x95, 0xF4, 0x57, 0xD3, 0xC1 } };
                HRESULT h2 = IUnknown_QueryInterface((IUnknown *)insp, &iid_access, &access);
                EXPECT_HR(h2, S_OK);
                if (SUCCEEDED(h2)) IUnknown_Release((IUnknown *)access);
                IUnknown_Release((IUnknown *)insp);
            }
        } else CHECK(insp == NULL);
    }
    SAFE_RELEASE(dd);
    DC_REL(d, c);
}

/* ---- immediate context through the versioned device interfaces ---- */
T(device1_get_immediate_context1_same_object)
{
    ID3D11Device1 *d1 = NULL; ID3D11DeviceContext1 *c1 = NULL; IUnknown *u1 = NULL, *u0 = NULL;
    NEEDDEV();
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1)) && d1) {
        ID3D11Device1_GetImmediateContext1(d1, &c1);
        CHECK(c1 != NULL);
        if (c1) {
            IUnknown_QueryInterface((IUnknown *)c1, &IID_IUnknown, (void **)&u1);
            IUnknown_QueryInterface((IUnknown *)c, &IID_IUnknown, (void **)&u0);
            CHECK(u0 && u0 == u1);
            SAFE_RELEASE(u1); SAFE_RELEASE(u0);
            SAFE_RELEASE(c1);
        }
    } else CHECKF(0, "ID3D11Device1 not available");
    SAFE_RELEASE(d1);
    DC_REL(d, c);
}
T(device2_get_immediate_context2_same_object)
{
    ID3D11Device2 *d2 = NULL; ID3D11DeviceContext2 *c2 = NULL; IUnknown *u1 = NULL, *u0 = NULL;
    NEEDDEV();
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device2, (void **)&d2)) && d2) {
        ID3D11Device2_GetImmediateContext2(d2, &c2);
        CHECK(c2 != NULL);
        if (c2) {
            IUnknown_QueryInterface((IUnknown *)c2, &IID_IUnknown, (void **)&u1);
            IUnknown_QueryInterface((IUnknown *)c, &IID_IUnknown, (void **)&u0);
            CHECK(u0 && u0 == u1);
            SAFE_RELEASE(u1); SAFE_RELEASE(u0);
            SAFE_RELEASE(c2);
        }
    } else CHECKF(0, "ID3D11Device2 not available");
    SAFE_RELEASE(d2);
    DC_REL(d, c);
}
T(device3_get_immediate_context3_same_object)
{
    ID3D11Device3 *d3 = NULL; ID3D11DeviceContext3 *c3 = NULL; IUnknown *u1 = NULL, *u0 = NULL;
    NEEDDEV();
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3)) && d3) {
        ID3D11Device3_GetImmediateContext3(d3, &c3);
        CHECK(c3 != NULL);
        if (c3) {
            IUnknown_QueryInterface((IUnknown *)c3, &IID_IUnknown, (void **)&u1);
            IUnknown_QueryInterface((IUnknown *)c, &IID_IUnknown, (void **)&u0);
            CHECK(u0 && u0 == u1);
            SAFE_RELEASE(u1); SAFE_RELEASE(u0);
            SAFE_RELEASE(c3);
        }
    } else CHECKF(0, "ID3D11Device3 not available");
    SAFE_RELEASE(d3);
    DC_REL(d, c);
}
T(context_pointer_stable_after_release_and_reget)
{
    ID3D11DeviceContext *again = NULL;
    NEEDDEV();
    ID3D11DeviceContext *orig = c;
    SAFE_RELEASE(c);                       /* the device keeps the immediate context alive */
    ID3D11Device_GetImmediateContext(d, &again);
    CHECK(again == orig);
    SAFE_RELEASE(again);
    SAFE_RELEASE(d);
}
T(context_survives_after_device_pointer_released)
{
    ID3D11Device *g = NULL;
    NEEDDEV();
    SAFE_RELEASE(d);                       /* the context holds the device alive */
    ID3D11DeviceContext_GetDevice(c, &g);
    CHECK(g != NULL);
    if (g) {
        EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(g), S_OK);
        SAFE_RELEASE(g);
    }
    SAFE_RELEASE(c);
}
T(deferred_context1_properties)
{
    ID3D11Device1 *d1 = NULL; ID3D11DeviceContext1 *dc1 = NULL;
    NEEDDEV();
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1)) && d1) {
        EXPECT_HR_OK(ID3D11Device1_CreateDeferredContext1(d1, 0, &dc1));
        if (dc1) {
            EXPECT_EQ(ID3D11DeviceContext1_GetType(dc1), D3D11_DEVICE_CONTEXT_DEFERRED);
            CHECK((void *)dc1 != (void *)c);
        }
        SAFE_RELEASE(dc1);
    }
    SAFE_RELEASE(d1);
    DC_REL(d, c);
}

/* ---- creation on an explicit adapter with extra options ---- */
T(driver_unknown_warp_adapter_bgra_flag)
{
    IDXGIAdapter *a; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0; UINT cf;
    REQUIRE(DC_FN());
    if (!dc_get_warp_adapter(&a)) { t_skip("no WARP adapter"); return; }
    HRESULT hr = DC_FN()(a, D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &d, &fl, NULL);
    IDXGIAdapter_Release(a);
    if (FAILED(hr)) { t_skip("device on WARP adapter: %s", d3dh_hr_name(hr)); return; }
    cf = ID3D11Device_GetCreationFlags(d);
    EXPECT_EQ(cf & D3D11_CREATE_DEVICE_BGRA_SUPPORT, D3D11_CREATE_DEVICE_BGRA_SUPPORT);
    SAFE_RELEASE(d);
}
T(driver_unknown_adapter_invalid_level_cleared)
{
    IDXGIAdapter *a; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL, bad[2] = { D3D_FEATURE_LEVEL_11_0, (D3D_FEATURE_LEVEL)0x4242 };
    REQUIRE(DC_FN());
    if (!dc_get_warp_adapter(&a)) { t_skip("no WARP adapter"); return; }
    EXPECT_HR(DC_FN()(a, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, bad, 2, D3D11_SDK_VERSION, &d, &fl, NULL), E_INVALIDARG);
    CHECK(d == NULL); EXPECT_EQ(fl, 0);
    IDXGIAdapter_Release(a);
}
T(sdk_version_checked_before_levels)
{
    /* order inside validation: overlap, SDK version, then the level values: both faults give E_INVALIDARG and clear outputs */
    D3D_FEATURE_LEVEL bad = (D3D_FEATURE_LEVEL)0x4242;
    REQUIRE(DC_FN());
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, &bad, 1, 5, "bad sdk and bad level");
}
T(levels_all_nine_valid_values_accepted_by_validation)
{
    /* every documented level value passes validation; a device may still be unavailable, but never E_INVALIDARG */
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    for (unsigned i = 0; i < 9; i++) {
        HRESULT hr = dc_warp(0, &l[i], 1, &d, NULL, NULL);
        CHECKF(hr != E_INVALIDARG, "level %s rejected as invalid argument", dc_fl_name(l[i]));
        SAFE_RELEASE(d);
    }
}

/* ---- concurrent creation ---- */
static DWORD WINAPI creator(LPVOID p)
{
    LONG *fails = (LONG *)p;
    for (int i = 0; i < 4; i++) {
        ID3D11Device *d = NULL;
        HRESULT hr = dc_warp(i & 1 ? D3D11_CREATE_DEVICE_SINGLETHREADED : 0, NULL, 0, &d, NULL, NULL);
        if (FAILED(hr)) { InterlockedIncrement(fails); return (DWORD)hr; }
        SAFE_RELEASE(d);
    }
    return 0;
}
T(concurrent_create_destroy_four_threads)
{
    HANDLE th[4]; LONG fails = 0; ID3D11Device *probe = NULL;
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(0, NULL, 0, &probe, NULL, NULL))) { t_skip("no WARP"); return; }
    SAFE_RELEASE(probe);
    for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, creator, &fails, 0, NULL);
    for (int i = 0; i < 4; i++) {
        if (!th[i]) { CHECK(0); continue; }
        EXPECT_EQ(WaitForSingleObject(th[i], 30000), WAIT_OBJECT_0);
        CloseHandle(th[i]);
    }
    EXPECT_EQ(fails, 0);
}
