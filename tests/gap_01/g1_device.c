/* dxgi.dll CreateDXGIFactory2 / DXGIGetDebugInterface1, d3d12.dll D3D12CreateDevice, d3d10 device and swap-chain creation. */
#include "gap1.h"
#include <dxgi1_6.h>
#include <dxgidebug.h>

#define GAP_MAGIC ((void *)(ULONG_PTR)0x1234)

/* ======================= CreateDXGIFactory2 ======================= */
TEST(gap1_factory2_supported_interfaces) {
    SKIP_ON_WINE("wine headless dxgi differs");
    REQUIRE(U(CreateDXGIFactory2));
    static const struct { const IID *iid; const char *name; } ok[] = {
        { &IID_IUnknown, "IUnknown" }, { &IID_IDXGIObject, "IDXGIObject" }, { &IID_IDXGIFactory, "IDXGIFactory" },
        { &IID_IDXGIFactory1, "IDXGIFactory1" }, { &IID_IDXGIFactory2, "IDXGIFactory2" }, { &IID_IDXGIFactory3, "IDXGIFactory3" },
        { &IID_IDXGIFactory4, "IDXGIFactory4" }, { &IID_IDXGIFactory5, "IDXGIFactory5" },
    };
    for (unsigned i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        void *f = NULL;
        HRESULT hr = U(CreateDXGIFactory2)(0, ok[i].iid, &f);
        CHECKF(hr == S_OK && f, "%s -> %s", ok[i].name, d3dh_hr_name(hr));
        if (f) IUnknown_Release((IUnknown *)f);
    }
}
TEST(gap1_factory2_newer_interfaces_observed) {
    REQUIRE(U(CreateDXGIFactory2));
    static const IID *iids[] = { &IID_IDXGIFactory6, &IID_IDXGIFactory7 };
    for (unsigned i = 0; i < 2; i++) {
        void *f = NULL;
        HRESULT hr = U(CreateDXGIFactory2)(0, iids[i], &f);
        t_note("IDXGIFactory%u -> %s", 6 + i, d3dh_hr_name(hr));
        if (f) IUnknown_Release((IUnknown *)f);
    }
}
TEST(gap1_factory2_non_factory_iid_is_nointerface_and_clears_output) {
    SKIP_ON_WINE("wine headless dxgi differs");
    REQUIRE(U(CreateDXGIFactory2));
    static const IID *bad[] = { &IID_IDXGIAdapter, &IID_IDXGIDevice, &IID_ID3D11Device, &IID_IDXGISwapChain, &GUID_NULL };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        void *f = GAP_MAGIC;
        HRESULT hr = U(CreateDXGIFactory2)(0, bad[i], &f);
        CHECKF(hr == G1_E_NOINTERFACE, "%s -> %s", d3dh_guid_str(bad[i]), d3dh_hr_name(hr));
        CHECKF(f == NULL, "%s: output not cleared", d3dh_guid_str(bad[i]));
    }
}
TEST(gap1_factory2_each_call_makes_a_new_object) {
    SKIP_ON_WINE("wine headless dxgi differs");
    REQUIRE(U(CreateDXGIFactory2));
    IDXGIFactory2 *a = NULL, *b = NULL;
    EXPECT_HR(U(CreateDXGIFactory2)(0, &IID_IDXGIFactory2, (void **)&a), S_OK);
    EXPECT_HR(U(CreateDXGIFactory2)(0, &IID_IDXGIFactory2, (void **)&b), S_OK);
    CHECK(a && b && a != b);
    if (a) G1_EQ(IDXGIFactory2_Release(a), 0);
    if (b) G1_EQ(IDXGIFactory2_Release(b), 0);
}
TEST(gap1_factory2_qi_chain_keeps_identity) {
    REQUIRE(U(CreateDXGIFactory2));
    IDXGIFactory2 *f = NULL;
    if (FAILED(U(CreateDXGIFactory2)(0, &IID_IDXGIFactory2, (void **)&f)) || !f) { t_skip("no factory"); return; }
    IUnknown *u1 = NULL, *u2 = NULL; IDXGIFactory *f0 = NULL; IDXGIFactory1 *f1 = NULL;
    EXPECT_HR(IDXGIFactory2_QueryInterface(f, &IID_IUnknown, (void **)&u1), S_OK);
    EXPECT_HR(IDXGIFactory2_QueryInterface(f, &IID_IDXGIFactory, (void **)&f0), S_OK);
    EXPECT_HR(IDXGIFactory2_QueryInterface(f, &IID_IDXGIFactory1, (void **)&f1), S_OK);
    if (f0) EXPECT_HR(IDXGIFactory_QueryInterface(f0, &IID_IUnknown, (void **)&u2), S_OK);
    CHECK(u1 != NULL && u1 == u2);
    CHECK((void *)f0 == (void *)f && (void *)f1 == (void *)f);       /* one object, one vtable chain */
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    if (f0) IDXGIFactory_Release(f0);
    if (f1) IDXGIFactory1_Release(f1);
    G1_EQ(IDXGIFactory2_Release(f), 0);
}
TEST(gap1_factory2_debug_flag_observed) {
    REQUIRE(U(CreateDXGIFactory2));
    IDXGIFactory2 *f = NULL;
    HRESULT hr = U(CreateDXGIFactory2)(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory2, (void **)&f);
    t_note("CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG) -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { CHECK(f != NULL); if (f) IDXGIFactory2_Release(f); } else CHECK(f == NULL);
}
TEST(gap1_factory2_is_current_and_fresh) {
    REQUIRE(U(CreateDXGIFactory2));
    IDXGIFactory1 *f = NULL;
    if (FAILED(U(CreateDXGIFactory2)(0, &IID_IDXGIFactory1, (void **)&f)) || !f) { t_skip("no factory"); return; }
    CHECK(IDXGIFactory1_IsCurrent(f));
    IDXGIFactory1_Release(f);
}
TEST(gap1_factory2_private_data_protocol) {
    REQUIRE(U(CreateDXGIFactory2));
    IDXGIFactory2 *f = NULL;
    if (FAILED(U(CreateDXGIFactory2)(0, &IID_IDXGIFactory2, (void **)&f)) || !f) { t_skip("no factory"); return; }
    static const GUID g = { 0xa1a1a1a1, 0xb2b2, 0xc3c3, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    UINT sz = 8; unsigned char buf[16];
    EXPECT_HR(IDXGIFactory2_GetPrivateData(f, &g, &sz, buf), G1_DXGI_ERROR_NOT_FOUND);
    static const unsigned char data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    EXPECT_HR(IDXGIFactory2_SetPrivateData(f, &g, sizeof data, data), S_OK);
    sz = 4;
    EXPECT_HR(IDXGIFactory2_GetPrivateData(f, &g, &sz, buf), G1_DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 8);                                                  /* required size is reported */
    sz = 8; memset(buf, 0, sizeof buf);
    EXPECT_HR(IDXGIFactory2_GetPrivateData(f, &g, &sz, buf), S_OK);
    CHECK(!memcmp(buf, data, 8));
    sz = 0;
    HRESULT hq = IDXGIFactory2_GetPrivateData(f, &g, &sz, NULL);
    EXPECT_HR(hq, S_OK); EXPECT_EQ(sz, 8);   /* Win10: a NULL buffer is a size query */
    HRESULT hrm = IDXGIFactory2_SetPrivateData(f, &g, 0, NULL);
    EXPECT_HR(hrm, S_OK);   /* Win10: size 0 with a NULL pointer removes the entry */
    sz = 8;
    EXPECT_HR(IDXGIFactory2_GetPrivateData(f, &g, &sz, buf), G1_DXGI_ERROR_NOT_FOUND);
    IDXGIFactory2_Release(f);
}
TEST(gap1_factory2_warp_adapter_identity) {
    REQUIRE(U(CreateDXGIFactory2));
    IDXGIFactory4 *f = NULL;
    if (FAILED(U(CreateDXGIFactory2)(0, &IID_IDXGIFactory4, (void **)&f)) || !f) { t_skip("no IDXGIFactory4"); return; }
    IDXGIAdapter *a = NULL;
    HRESULT hr = IDXGIFactory4_EnumWarpAdapter(f, &IID_IDXGIAdapter, (void **)&a);
    EXPECT_HR(hr, S_OK);
    if (a) {
        DXGI_ADAPTER_DESC d; memset(&d, 0, sizeof d);
        EXPECT_HR(IDXGIAdapter_GetDesc(a, &d), S_OK);
        EXPECT_EQ(d.VendorId, 0x1414);
        EXPECT_EQ(d.DeviceId, 0x8c);
        CHECK(!lstrcmpW(d.Description, L"Microsoft Basic Render Driver"));
        EXPECT_EQ(d.DedicatedVideoMemory, 0);
        IDXGIAdapter1 *a1 = NULL;
        if (SUCCEEDED(IDXGIAdapter_QueryInterface(a, &IID_IDXGIAdapter1, (void **)&a1)) && a1) {
            DXGI_ADAPTER_DESC1 d1; memset(&d1, 0, sizeof d1);
            EXPECT_HR(IDXGIAdapter1_GetDesc1(a1, &d1), S_OK);
            CHECK(d1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
            CHECK(d1.AdapterLuid.LowPart == d.AdapterLuid.LowPart && d1.AdapterLuid.HighPart == d.AdapterLuid.HighPart);
            IDXGIAdapter1_Release(a1);
        }
        void *bad = GAP_MAGIC;
        EXPECT_HR(IDXGIFactory4_EnumWarpAdapter(f, &IID_ID3D11Device, &bad), G1_E_NOINTERFACE);
        IDXGIAdapter_Release(a);
    }
    IDXGIFactory4_Release(f);
}
TEST(gap1_factory2_enum_adapters_ends_with_not_found) {
    REQUIRE(U(CreateDXGIFactory2));
    IDXGIFactory1 *f = NULL;
    if (FAILED(U(CreateDXGIFactory2)(0, &IID_IDXGIFactory1, (void **)&f)) || !f) { t_skip("no factory"); return; }
    UINT i = 0; HRESULT hr;
    for (;; i++) {
        IDXGIAdapter1 *a = (IDXGIAdapter1 *)GAP_MAGIC;
        hr = IDXGIFactory1_EnumAdapters1(f, i, &a);
        if (FAILED(hr)) { CHECK(a == NULL); break; }
        IDXGIAdapter1_Release(a);
        if (i > 64) break;
    }
    EXPECT_HR(hr, G1_DXGI_ERROR_NOT_FOUND);
    /* every index past the end keeps failing the same way */
    IDXGIAdapter1 *a = (IDXGIAdapter1 *)GAP_MAGIC;
    EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, i + 5, &a), G1_DXGI_ERROR_NOT_FOUND);
    CHECK(a == NULL);
    t_note("%u enumerable adapters", i);
    IDXGIFactory1_Release(f);
}

/* ======================= DXGIGetDebugInterface1 ======================= */
TEST(gap1_dxgi_debug_interface_null_output) {
    SKIP_ON_WINE("wine headless dxgi differs");
    REQUIRE(U(DXGIGetDebugInterface1));
    EXPECT_HR(U(DXGIGetDebugInterface1)(0, &IID_IDXGIInfoQueue, NULL), G1_DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(U(DXGIGetDebugInterface1)(0, &IID_IDXGIDebug, NULL), G1_DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(U(DXGIGetDebugInterface1)(1, &IID_IDXGIDebug, NULL), G1_DXGI_ERROR_INVALID_CALL);
}
TEST(gap1_dxgi_debug_interface_sdk_missing_or_available) {
    SKIP_ON_WINE("wine headless dxgi differs");
    REQUIRE(U(DXGIGetDebugInterface1));
    static const IID *iids[] = { &IID_IDXGIInfoQueue, &IID_IDXGIDebug, &IID_IDXGIDebug1 };
    for (unsigned i = 0; i < 3; i++) {
        void *p = GAP_MAGIC;
        HRESULT hr = U(DXGIGetDebugInterface1)(0, iids[i], &p);
        CHECKF(hr == S_OK || hr == G1_DXGI_ERROR_SDK_COMPONENT_MISSING, "%s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        if (FAILED(hr)) CHECK(p == NULL); else { CHECK(p != NULL); if (p) IUnknown_Release((IUnknown *)p); }
        t_note("DXGIGetDebugInterface1(%s) -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
    }
}
TEST(gap1_dxgi_debug_interface_nonzero_flags_rejected) {
    SKIP_ON_WINE("wine headless dxgi differs");
    REQUIRE(U(DXGIGetDebugInterface1));
    static const UINT flags[] = { 1, 2, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < 4; i++) {
        void *p = GAP_MAGIC;
        HRESULT hr = U(DXGIGetDebugInterface1)(flags[i], &IID_IDXGIDebug, &p);
        /* the flags are validated only after the debug component has produced the interface */
        CHECKF(hr == G1_DXGI_ERROR_INVALID_CALL || hr == G1_DXGI_ERROR_SDK_COMPONENT_MISSING, "flags 0x%x -> %s", flags[i], d3dh_hr_name(hr));
        CHECKF(p == NULL, "flags 0x%x: output not cleared", flags[i]);
    }
}
TEST(gap1_dxgi_debug_interface_unknown_iid) {
    SKIP_ON_WINE("wine headless dxgi differs");
    REQUIRE(U(DXGIGetDebugInterface1));
    void *p = GAP_MAGIC;
    HRESULT hr = U(DXGIGetDebugInterface1)(0, &IID_ID3D11Device, &p);
    EXPECT_HR_FAILED(hr);
    CHECK(p == NULL);
    /* unknown IID: E_NOINTERFACE with the Graphics Tools feature installed, DXGI_ERROR_SDK_COMPONENT_MISSING (0x887a002d) without it */
    CHECKF(hr == G1_E_NOINTERFACE || hr == (HRESULT)0x887A002D, "unknown IID: %s", d3dh_hr_name(hr));
}

/* ======================= D3D12CreateDevice ======================= */
static IDXGIAdapter *warp_adapter(void) {
    if (!U(CreateDXGIFactory2)) return NULL;
    IDXGIFactory4 *f = NULL; IDXGIAdapter *a = NULL;
    if (FAILED(U(CreateDXGIFactory2)(0, &IID_IDXGIFactory4, (void **)&f)) || !f) return NULL;
    if (FAILED(IDXGIFactory4_EnumWarpAdapter(f, &IID_IDXGIAdapter, (void **)&a))) a = NULL;
    IDXGIFactory4_Release(f);
    return a;
}
TEST(gap1_d3d12_create_probe_with_null_device_returns_sfalse) {
    REQUIRE(U(D3D12CreateDevice));
    IDXGIAdapter *a = warp_adapter(); if (!a) { t_skip("no WARP adapter"); return; }
    HRESULT hr = U(D3D12CreateDevice)((IUnknown *)a, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL);
    if (FAILED(hr)) { t_skip("D3D12 unavailable on WARP: %s", d3dh_hr_name(hr)); IDXGIAdapter_Release(a); return; }
    EXPECT_HR(hr, S_FALSE);                                  /* documented: capability probe, no device created */
    IDXGIAdapter_Release(a);
}
TEST(gap1_d3d12_create_warp_device_properties) {
    REQUIRE(U(D3D12CreateDevice));
    IDXGIAdapter *a = warp_adapter(); if (!a) { t_skip("no WARP adapter"); return; }
    ID3D12Device *dev = NULL;
    HRESULT hr = U(D3D12CreateDevice)((IUnknown *)a, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&dev);
    if (FAILED(hr) || !dev) { t_skip("D3D12CreateDevice(WARP) -> %s", d3dh_hr_name(hr)); IDXGIAdapter_Release(a); return; }
    DXGI_ADAPTER_DESC d; IDXGIAdapter_GetDesc(a, &d);
    LUID l;
    { typedef LUID *(WINAPI *pfn)(ID3D12Device *, LUID *); pfn f = (pfn)dev->lpVtbl->GetAdapterLuid; f(dev, &l); }
    CHECK(l.LowPart == d.AdapterLuid.LowPart && l.HighPart == d.AdapterLuid.HighPart);
    G1_EQ(ID3D12Device_GetNodeCount(dev), 1);
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(dev), S_OK);
    D3D12_FEATURE_DATA_FEATURE_LEVELS fl; static const D3D_FEATURE_LEVEL req[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_12_1 };
    fl.NumFeatureLevels = 4; fl.pFeatureLevelsRequested = req; fl.MaxSupportedFeatureLevel = 0;
    EXPECT_HR(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FEATURE_LEVELS, &fl, sizeof fl), S_OK);
    EXPECT_EQ(fl.MaxSupportedFeatureLevel, D3D_FEATURE_LEVEL_12_1);   /* Win10 22H2 WARP12 */
    G1_EQ(ID3D12Device_Release(dev), 0);
    IDXGIAdapter_Release(a);
}
TEST(gap1_d3d12_two_devices_on_one_adapter) {
    REQUIRE(U(D3D12CreateDevice));
    IDXGIAdapter *a = warp_adapter(); if (!a) { t_skip("no WARP adapter"); return; }
    ID3D12Device *d1 = NULL, *d2 = NULL;
    HRESULT h1 = U(D3D12CreateDevice)((IUnknown *)a, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d1);
    if (FAILED(h1)) { t_skip("D3D12CreateDevice -> %s", d3dh_hr_name(h1)); IDXGIAdapter_Release(a); return; }
    EXPECT_HR(U(D3D12CreateDevice)((IUnknown *)a, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d2), S_OK);
    CHECK(d1 && d1 == d2);   /* Win10: an adapter has one ID3D12Device; the second create returns the same object with a second reference */
    if (d1 && d1 == d2) { G1_EQ(ID3D12Device_AddRef(d1), 3); G1_EQ(ID3D12Device_Release(d1), 2); }
    if (d1) ID3D12Device_Release(d1);
    if (d2) ID3D12Device_Release(d2);
    IDXGIAdapter_Release(a);
}
TEST(gap1_d3d12_invalid_minimum_feature_level) {
    REQUIRE(U(D3D12CreateDevice));
    IDXGIAdapter *a = warp_adapter(); if (!a) { t_skip("no WARP adapter"); return; }
    ID3D12Device *probe = NULL;
    if (FAILED(U(D3D12CreateDevice)((IUnknown *)a, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&probe))) { t_skip("no D3D12 on WARP"); IDXGIAdapter_Release(a); return; }
    ID3D12Device_Release(probe);
    static const D3D_FEATURE_LEVEL bad[] = { (D3D_FEATURE_LEVEL)0, (D3D_FEATURE_LEVEL)0x1234, D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ID3D12Device *d = NULL;
        HRESULT hr = U(D3D12CreateDevice)((IUnknown *)a, bad[i], &IID_ID3D12Device, (void **)&d);
        CHECKF(hr == G1_E_INVALIDARG, "minimum level 0x%x -> %s", bad[i], d3dh_hr_name(hr));   /* below 11_0 and unknown values: E_INVALIDARG */
        if (d) ID3D12Device_Release(d);
    }
    IDXGIAdapter_Release(a);
}
TEST(gap1_d3d12_non_adapter_object_is_rejected) {
    REQUIRE(U(D3D12CreateDevice));
    IDXGIFactory1 *f = NULL;
    if (FAILED(d3dh_make_factory1(&f)) || !f) { t_skip("no factory"); return; }
    ID3D12Device *d = NULL;
    HRESULT hr = U(D3D12CreateDevice)((IUnknown *)f, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d);
    EXPECT_HR_FAILED(hr);
    CHECK(d == NULL);
    EXPECT_HR(hr, G1_E_INVALIDARG);   /* D3D12CreateDevice(factory as adapter): measured on Win10 */
    IDXGIFactory1_Release(f);
}

/* ======================= D3D10CreateDevice1 ======================= */
TEST(gap1_d3d10_1_warp_levels_roundtrip) {
    REQUIRE(U(D3D10CreateDevice1));
    static const struct { D3D10_FEATURE_LEVEL1 fl; const char *name; int must; } lv[] = {
        { D3D10_FEATURE_LEVEL_10_1, "10_1", 1 }, { D3D10_FEATURE_LEVEL_10_0, "10_0", 1 },
        { D3D10_FEATURE_LEVEL_9_3, "9_3", 0 }, { D3D10_FEATURE_LEVEL_9_2, "9_2", 0 }, { D3D10_FEATURE_LEVEL_9_1, "9_1", 0 },
    };
    int made = 0;
    for (unsigned i = 0; i < sizeof lv / sizeof lv[0]; i++) {
        ID3D10Device1 *d = NULL;
        HRESULT hr = U(D3D10CreateDevice1)(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, lv[i].fl, D3D10_1_SDK_VERSION, &d);
        if (FAILED(hr)) { t_note("WARP at %s -> %s", lv[i].name, d3dh_hr_name(hr)); CHECK(d == NULL); continue; }
        made++;
        CHECK(d != NULL);
        if (d) {
            G1_EQ(ID3D10Device1_GetFeatureLevel(d), lv[i].fl);          /* the requested level is exactly what you get */
            EXPECT_HR(ID3D10Device1_GetDeviceRemovedReason(d), S_OK);
            G1_EQ(ID3D10Device1_Release(d), 0);
        }
    }
    if (!made) t_skip("no WARP D3D10.1 device");
}
TEST(gap1_d3d10_1_creation_flags_roundtrip) {
    REQUIRE(U(D3D10CreateDevice1));
    ID3D10Device1 *d = NULL;
    UINT want = D3D10_CREATE_DEVICE_SINGLETHREADED | D3D10_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = U(D3D10CreateDevice1)(NULL, D3D10_DRIVER_TYPE_WARP, NULL, want, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d);
    if (FAILED(hr) || !d) { t_skip("device -> %s", d3dh_hr_name(hr)); return; }
    UINT got = ID3D10Device1_GetCreationFlags(d);
    CHECK((got & want) == want);
    G1_EQ(ID3D10Device1_GetExceptionMode(d), 0);
    ID3D10Device1_Release(d);
}
TEST(gap1_d3d10_1_device_interfaces_and_adapter) {
    REQUIRE(U(D3D10CreateDevice1));
    ID3D10Device1 *d = NULL;
    HRESULT hr = U(D3D10CreateDevice1)(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d);
    if (FAILED(hr) || !d) { t_skip("device -> %s", d3dh_hr_name(hr)); return; }
    ID3D10Device *d0 = NULL; IDXGIDevice *dx = NULL;
    EXPECT_HR(ID3D10Device1_QueryInterface(d, &IID_ID3D10Device, (void **)&d0), S_OK);
    CHECK((void *)d0 == (void *)d);
    EXPECT_HR(ID3D10Device1_QueryInterface(d, &IID_IDXGIDevice, (void **)&dx), S_OK);
    if (dx) {
        IDXGIAdapter *a = NULL;
        EXPECT_HR(IDXGIDevice_GetAdapter(dx, &a), S_OK);
        if (a) {
            DXGI_ADAPTER_DESC ad; IDXGIAdapter_GetDesc(a, &ad);
            EXPECT_EQ(ad.VendorId, 0x1414);                           /* WARP driver type means the basic render adapter */
            IDXGIAdapter_Release(a);
        }
        IDXGIDevice_Release(dx);
    }
    void *d11 = NULL;
    EXPECT_HR(ID3D10Device1_QueryInterface(d, &IID_ID3D11Device, &d11), S_OK);   /* Win10: a d3d10.1 device is backed by a d3d11 device and exposes it */
    CHECK(d11 != NULL);
    if (d11) { ID3D11Device *x = d11; CHECK(ID3D11Device_GetFeatureLevel(x) >= D3D_FEATURE_LEVEL_10_0); ID3D11Device_Release(x); }
    if (d0) ID3D10Device_Release(d0);
    ID3D10Device1_Release(d);
}
TEST(gap1_d3d10_1_software_driver_needs_module) {
    REQUIRE(U(D3D10CreateDevice1));
    ID3D10Device1 *d = GAP_MAGIC;
    HRESULT hr = U(D3D10CreateDevice1)(NULL, D3D10_DRIVER_TYPE_SOFTWARE, NULL, 0, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d);
    EXPECT_HR_FAILED(hr);
    EXPECT_HR(hr, G1_E_INVALIDARG);   /* D3D10_DRIVER_TYPE_SOFTWARE with a NULL module: measured on Win10 */
    if (d != GAP_MAGIC && d) ID3D10Device1_Release(d);
}

/* ======================= D3D10CreateDeviceAndSwapChain ======================= */
static DXGI_SWAP_CHAIN_DESC sc_desc(HWND hw) {
    DXGI_SWAP_CHAIN_DESC d; memset(&d, 0, sizeof d);
    d.BufferDesc.Width = 64; d.BufferDesc.Height = 48; d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1; d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 1;
    d.OutputWindow = hw; d.Windowed = TRUE; d.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    return d;
}
TEST(gap1_d3d10_swapchain_creation_roundtrip) {
    REQUIRE(U(D3D10CreateDeviceAndSwapChain));
    HWND hw = d3dh_create_hidden_window_sized(64, 48);
    DXGI_SWAP_CHAIN_DESC d = sc_desc(hw);
    IDXGISwapChain *sc = NULL; ID3D10Device *dev = NULL;
    HRESULT hr = U(D3D10CreateDeviceAndSwapChain)(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_SDK_VERSION, &d, &sc, &dev);
    if (FAILED(hr) || !sc || !dev) { t_skip("D3D10CreateDeviceAndSwapChain -> %s", d3dh_hr_name(hr)); SAFE_RELEASE(sc); SAFE_RELEASE(dev); d3dh_destroy_window(hw); return; }
    DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
    EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
    EXPECT_EQ(g.BufferDesc.Width, 64);
    EXPECT_EQ(g.BufferDesc.Height, 48);
    EXPECT_EQ(g.BufferDesc.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(g.BufferCount, 1);
    EXPECT_EQ(g.Windowed, TRUE);
    EXPECT_EQ(g.OutputWindow, hw);
    EXPECT_EQ(g.SampleDesc.Count, 1);
    EXPECT_EQ(g.SwapEffect, DXGI_SWAP_EFFECT_DISCARD);
    ID3D10Texture2D *bb = NULL;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D10Texture2D, (void **)&bb), S_OK);
    if (bb) {
        D3D10_TEXTURE2D_DESC td; ID3D10Texture2D_GetDesc(bb, &td);
        EXPECT_EQ(td.Width, 64);
        EXPECT_EQ(td.Height, 48);
        EXPECT_EQ(td.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
        EXPECT_EQ(td.MipLevels, 1);
        EXPECT_EQ(td.ArraySize, 1);
        CHECK(td.BindFlags & D3D10_BIND_RENDER_TARGET);
        ID3D10Texture2D_Release(bb);
    }
    void *none = GAP_MAGIC;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 1, &IID_ID3D10Texture2D, &none), G1_DXGI_ERROR_INVALID_CALL);   /* only buffer 0 exists */
    ID3D10Device *same = NULL;
    EXPECT_HR(IDXGISwapChain_GetDevice(sc, &IID_ID3D10Device, (void **)&same), S_OK);
    CHECK(same == dev);
    if (same) ID3D10Device_Release(same);
    HRESULT hp = IDXGISwapChain_Present(sc, 0, 0);
    CHECK(SUCCEEDED(hp));
    EXPECT_HR(hp, S_OK);   /* Present on a hidden window: measured on Win10 */
    IDXGISwapChain_Release(sc);
    G1_EQ(ID3D10Device_Release(dev), 0);
    d3dh_destroy_window(hw);
}
TEST(gap1_d3d10_swapchain_device_flags_apply) {
    REQUIRE(U(D3D10CreateDeviceAndSwapChain));
    HWND hw = d3dh_create_hidden_window_sized(64, 48);
    DXGI_SWAP_CHAIN_DESC d = sc_desc(hw);
    IDXGISwapChain *sc = NULL; ID3D10Device *dev = NULL;
    UINT want = D3D10_CREATE_DEVICE_SINGLETHREADED;
    HRESULT hr = U(D3D10CreateDeviceAndSwapChain)(NULL, D3D10_DRIVER_TYPE_WARP, NULL, want, D3D10_SDK_VERSION, &d, &sc, &dev);
    if (FAILED(hr) || !sc || !dev) { t_skip("D3D10CreateDeviceAndSwapChain -> %s", d3dh_hr_name(hr)); SAFE_RELEASE(sc); SAFE_RELEASE(dev); d3dh_destroy_window(hw); return; }
    CHECK((ID3D10Device_GetCreationFlags(dev) & want) == want);
    IDXGISwapChain_Release(sc); ID3D10Device_Release(dev); d3dh_destroy_window(hw);
}
CHILD(g1_d3d10_sc_zero_buffers) {
    if (!U(D3D10CreateDeviceAndSwapChain)) return 90;
    HWND hw = d3dh_create_hidden_window_sized(64, 48);
    DXGI_SWAP_CHAIN_DESC d = sc_desc(hw); d.BufferCount = 0;
    IDXGISwapChain *sc = NULL; ID3D10Device *dev = NULL;
    U(D3D10CreateDeviceAndSwapChain)(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_SDK_VERSION, &d, &sc, &dev);   /* faults */
    return 0;
}
TEST(gap1_d3d10_swapchain_zero_buffer_count_faults) {
    REQUIRE(U(D3D10CreateDeviceAndSwapChain));
    SKIP_ON_WINE("wine d3d10 behaviour differs");
    /* Win10: BufferCount 0 is not rejected with an HRESULT, it raises STATUS_ACCESS_VIOLATION inside d3d10 */
    EXPECT_EQ(t_run_child("g1_d3d10_sc_zero_buffers", "", 60000), 0xC0000005);
}
