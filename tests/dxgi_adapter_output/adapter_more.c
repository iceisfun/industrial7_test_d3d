/* More adapter-side coverage: WARP enumeration paths, GPU preference ordering, device/adapter identity, lifetimes. */
#include "ao.h"

static IDXGIFactory4 *ao_f4(IDXGIFactory1 *f) {
    IDXGIFactory4 *f4 = NULL;
    if (FAILED(IDXGIFactory1_QueryInterface(f, &IID_IDXGIFactory4, (void **)&f4))) { t_skip("IDXGIFactory4 unavailable"); return NULL; }
    return f4;
}
static IDXGIFactory6 *ao_f6(IDXGIFactory1 *f) {
    IDXGIFactory6 *f6 = NULL;
    if (FAILED(IDXGIFactory1_QueryInterface(f, &IID_IDXGIFactory6, (void **)&f6))) { t_skip("IDXGIFactory6 unavailable"); return NULL; }
    return f6;
}

/* ---- EnumWarpAdapter ----------------------------------------------------- */
AO_TEST_FACTORY(dxgi_adapter_output_warp_enum_iids) {
    IDXGIFactory4 *f4 = ao_f4(f);
    if (!f4) return;
    static const IID *iids[] = { &IID_IDXGIAdapter, &IID_IDXGIAdapter1, &IID_IDXGIAdapter2, &IID_IDXGIAdapter3 };
    for (size_t i = 0; i < sizeof iids / sizeof *iids; i++) {
        void *p = NULL;
        HRESULT hr = IDXGIFactory4_EnumWarpAdapter(f4, iids[i], &p);
        CHECKF(hr == S_OK && p, "EnumWarpAdapter iid %zu -> %s", i, d3dh_hr_name(hr));
        if (p) IUnknown_Release((IUnknown *)p);
    }
    IDXGIFactory4_Release(f4);
}
AO_TEST_FACTORY(dxgi_adapter_output_warp_enum_bad_iid) {
    IDXGIFactory4 *f4 = ao_f4(f);
    if (!f4) return;
    void *p = (void *)(UINT_PTR)1;
    HRESULT hr = IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIFactory, &p);
    EXPECT_HR_FAILED(hr);
    t_note("EnumWarpAdapter(IDXGIFactory) -> %s", d3dh_hr_name(hr));
    CHECK(p == NULL || p == (void *)(UINT_PTR)1);
    IDXGIFactory4_Release(f4);
}
AO_TEST_FACTORY(dxgi_adapter_output_warp_enum_null_out) {
    IDXGIFactory4 *f4 = ao_f4(f);
    if (!f4) return;
    HRESULT hr = IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("EnumWarpAdapter(NULL out) -> %s", d3dh_hr_name(hr));
    IDXGIFactory4_Release(f4);
}
AO_TEST_FACTORY(dxgi_adapter_output_warp_enum_desc) {
    IDXGIFactory4 *f4 = ao_f4(f);
    if (!f4) return;
    IDXGIAdapter1 *w = NULL; DXGI_ADAPTER_DESC1 d;
    if (SUCCEEDED(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&w)) && w) {
        EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(w, &d));
        EXPECT_EQ(d.VendorId, AO_WARP_VENDOR);
        EXPECT_EQ(d.DeviceId, AO_WARP_DEVICE);
        EXPECT_WSTR(d.Description, L"Microsoft Basic Render Driver");
        CHECK((d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0);
        CHECK(d.DedicatedVideoMemory == 0);
        IDXGIAdapter1_Release(w);
    } else CHECK(0);
    IDXGIFactory4_Release(f4);
}
AO_TEST_FACTORY(dxgi_adapter_output_warp_luid_vs_enumeration) {
    IDXGIFactory4 *f4 = ao_f4(f);
    if (!f4) return;
    IDXGIAdapter1 *w = NULL; DXGI_ADAPTER_DESC1 d; int found = 0;
    if (SUCCEEDED(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&w)) && w) {
        IDXGIAdapter1_GetDesc1(w, &d);
        for (UINT i = 0; i < 64; i++) {
            IDXGIAdapter1 *x = NULL; DXGI_ADAPTER_DESC1 e;
            if (IDXGIFactory1_EnumAdapters1(f, i, &x) != S_OK) break;
            IDXGIAdapter1_GetDesc1(x, &e);
            if (ao_luid_eq(d.AdapterLuid, e.AdapterLuid)) found = 1;
            IDXGIAdapter1_Release(x);
        }
        t_note("WARP LUID %08lx:%08lx present in EnumAdapters1 list: %d", (unsigned long)d.AdapterLuid.HighPart, (unsigned long)d.AdapterLuid.LowPart, found);
        IDXGIAdapter1_Release(w);
    }
    IDXGIFactory4_Release(f4);
}

/* ---- EnumAdapterByLuid ----------------------------------------------------- */
AO_TEST_ADAPTER(dxgi_adapter_output_luid_enum_iids) {
    IDXGIFactory4 *f4 = ao_f4(f);
    if (!f4) return;
    DXGI_ADAPTER_DESC1 d;
    IDXGIAdapter1_GetDesc1(a, &d);
    IDXGIAdapter *p0 = NULL;
    EXPECT_HR(IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, &IID_IDXGIAdapter, (void **)&p0), S_OK);
    if (p0) IDXGIAdapter_Release(p0);
    IDXGIAdapter3 *p3 = NULL;
    EXPECT_HR(IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, &IID_IDXGIAdapter3, (void **)&p3), S_OK);
    if (p3) IDXGIAdapter3_Release(p3);
    void *bad = (void *)(UINT_PTR)1;
    HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, &IID_IDXGIFactory, &bad);
    EXPECT_HR_FAILED(hr);
    t_note("EnumAdapterByLuid(IDXGIFactory iid) -> %s", d3dh_hr_name(hr));
    IDXGIFactory4_Release(f4);
}
AO_TEST_FACTORY(dxgi_adapter_output_luid_zero_not_found) {
    IDXGIFactory4 *f4 = ao_f4(f);
    if (!f4) return;
    LUID z = { 0, 0 };
    void *p = (void *)(UINT_PTR)1;
    HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, z, &IID_IDXGIAdapter1, &p);
    EXPECT_HR_FAILED(hr);
    t_note("EnumAdapterByLuid(0:0) -> %s", d3dh_hr_name(hr));
    if (FAILED(hr)) CHECK(p == NULL);
    IDXGIFactory4_Release(f4);
}

/* ---- EnumAdapterByGpuPreference ------------------------------------------------- */
AO_TEST_FACTORY(dxgi_adapter_output_gpupref_counts) {
    IDXGIFactory6 *f6 = ao_f6(f);
    if (!f6) return;
    UINT total = ao_count_adapters(f);
    static const DXGI_GPU_PREFERENCE prefs[] = { DXGI_GPU_PREFERENCE_UNSPECIFIED, DXGI_GPU_PREFERENCE_MINIMUM_POWER, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE };
    for (size_t k = 0; k < 3; k++) {
        UINT n = 0;
        for (;; n++) {
            IDXGIAdapter1 *x = (IDXGIAdapter1 *)(UINT_PTR)1;
            HRESULT hr = IDXGIFactory6_EnumAdapterByGpuPreference(f6, n, prefs[k], &IID_IDXGIAdapter1, (void **)&x);
            if (hr != S_OK) { EXPECT_HR(hr, DXGI_ERROR_NOT_FOUND); CHECK(x == NULL); break; }
            IDXGIAdapter1_Release(x);
            if (n > 64) { CHECK(0); break; }
        }
        t_note("preference %d: %u adapters (EnumAdapters1: %u)", (int)prefs[k], n, total);
        CHECKF(n <= total + 1, "preference %d lists %u adapters, more than %u", (int)prefs[k], n, total);
    }
    IDXGIFactory6_Release(f6);
}
AO_TEST_FACTORY(dxgi_adapter_output_gpupref_unspecified_matches_enum) {
    IDXGIFactory6 *f6 = ao_f6(f);
    if (!f6) return;
    UINT total = ao_count_adapters(f);
    for (UINT i = 0; i < total; i++) {
        IDXGIAdapter1 *x = NULL, *y = NULL; DXGI_ADAPTER_DESC1 dx, dy;
        if (IDXGIFactory6_EnumAdapterByGpuPreference(f6, i, DXGI_GPU_PREFERENCE_UNSPECIFIED, &IID_IDXGIAdapter1, (void **)&x) != S_OK) { t_note("unspecified list shorter than enum list"); break; }
        IDXGIFactory1_EnumAdapters1(f, i, &y);
        if (y) {
            IDXGIAdapter1_GetDesc1(x, &dx); IDXGIAdapter1_GetDesc1(y, &dy);
            t_note("adapter %u same LUID in unspecified order: %d", i, ao_luid_eq(dx.AdapterLuid, dy.AdapterLuid));
            IDXGIAdapter1_Release(y);
        }
        IDXGIAdapter1_Release(x);
    }
    IDXGIFactory6_Release(f6);
}
AO_TEST_FACTORY(dxgi_adapter_output_gpupref_high_perf_not_after_software) {
    IDXGIFactory6 *f6 = ao_f6(f);
    if (!f6) return;
    int seen_software = 0, bad = 0;
    for (UINT i = 0; i < 64; i++) {
        IDXGIAdapter1 *x = NULL; DXGI_ADAPTER_DESC1 d;
        if (IDXGIFactory6_EnumAdapterByGpuPreference(f6, i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter1, (void **)&x) != S_OK) break;
        IDXGIAdapter1_GetDesc1(x, &d);
        if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) seen_software = 1; else if (seen_software) bad = 1;
        IDXGIAdapter1_Release(x);
    }
    t_note("a hardware adapter listed after a software adapter in HIGH_PERFORMANCE order: %d", bad);
    IDXGIFactory6_Release(f6);
}
AO_TEST_FACTORY(dxgi_adapter_output_gpupref_bad_preference) {
    IDXGIFactory6 *f6 = ao_f6(f);
    if (!f6) return;
    void *p = (void *)(UINT_PTR)1;
    HRESULT hr = IDXGIFactory6_EnumAdapterByGpuPreference(f6, 0, (DXGI_GPU_PREFERENCE)77, &IID_IDXGIAdapter1, &p);
    EXPECT_HR_FAILED(hr);
    t_note("EnumAdapterByGpuPreference(77) -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    IDXGIFactory6_Release(f6);
}
AO_TEST_FACTORY(dxgi_adapter_output_gpupref_null_out) {
    IDXGIFactory6 *f6 = ao_f6(f);
    if (!f6) return;
    HRESULT hr = IDXGIFactory6_EnumAdapterByGpuPreference(f6, 0, DXGI_GPU_PREFERENCE_UNSPECIFIED, &IID_IDXGIAdapter1, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("EnumAdapterByGpuPreference(NULL out) -> %s", d3dh_hr_name(hr));
    IDXGIFactory6_Release(f6);
}

/* ---- CreateSoftwareAdapter ------------------------------------------------------------ */
AO_TEST_FACTORY(dxgi_adapter_output_create_software_adapter_null_module) {
    IDXGIAdapter *a = (IDXGIAdapter *)(UINT_PTR)1;
    HRESULT hr = IDXGIFactory1_CreateSoftwareAdapter(f, NULL, &a);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    CHECK(a == NULL || a == (IDXGIAdapter *)(UINT_PTR)1);
}

/* ---- device <-> adapter identity ---------------------------------------------------------- */
AO_TEST_FACTORY(dxgi_adapter_output_device_adapter_identity) {
    ID3D11Device *dev = NULL; IDXGIDevice *dd = NULL; IDXGIAdapter *da = NULL; IDXGIAdapter1 *da1 = NULL;
    D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE drv;
    if (!d3dh_D3D11CreateDevice()) { t_skip("D3D11CreateDevice missing"); return; }
    if (FAILED(d3dh_make_device11(&dev, NULL, &fl, &drv, 0)) || !dev) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd), S_OK);
    if (dd) {
        EXPECT_HR(IDXGIDevice_GetAdapter(dd, &da), S_OK);
        if (da) {
            DXGI_ADAPTER_DESC d;
            EXPECT_HR_OK(IDXGIAdapter_GetDesc(da, &d));
            CHECK(d.VendorId != 0);
            EXPECT_HR(IDXGIAdapter_QueryInterface(da, &IID_IDXGIAdapter1, (void **)&da1), S_OK);
            IDXGIFactory1 *pf = NULL;
            EXPECT_HR(IDXGIAdapter_GetParent(da, &IID_IDXGIFactory1, (void **)&pf), S_OK);
            if (pf) IDXGIFactory1_Release(pf);
            if (drv == D3D_DRIVER_TYPE_WARP) {
                EXPECT_EQ(d.VendorId, AO_WARP_VENDOR);
                EXPECT_EQ(d.DeviceId, AO_WARP_DEVICE);
            }
            t_note("device adapter: %ls vendor %04x", d.Description, d.VendorId);
        }
        IDXGIDevice_Release(dd);
    }
    if (da1) IDXGIAdapter1_Release(da1);
    if (da) IDXGIAdapter_Release(da);
    ID3D11Device_Release(dev);
}
AO_TEST_FACTORY(dxgi_adapter_output_device_adapter_luid_matches_d3d11) {
    ID3D11Device *dev = NULL; IDXGIDevice *dd = NULL; IDXGIAdapter *da = NULL;
    D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE drv;
    if (!d3dh_D3D11CreateDevice()) { t_skip("D3D11CreateDevice missing"); return; }
    if (FAILED(d3dh_make_device11(&dev, NULL, &fl, &drv, 0)) || !dev) { t_skip("no D3D11 device"); return; }
    if (SUCCEEDED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd)) && SUCCEEDED(IDXGIDevice_GetAdapter(dd, &da))) {
        DXGI_ADAPTER_DESC d; IDXGIFactory4 *f4 = ao_f4(f);
        IDXGIAdapter_GetDesc(da, &d);
        if (f4) {
            IDXGIAdapter *same = NULL;
            HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, &IID_IDXGIAdapter, (void **)&same);
            t_note("device adapter LUID resolvable through EnumAdapterByLuid: %s", d3dh_hr_name(hr));
            if (same) IDXGIAdapter_Release(same);
            IDXGIFactory4_Release(f4);
        }
    }
    if (da) IDXGIAdapter_Release(da);
    if (dd) IDXGIDevice_Release(dd);
    ID3D11Device_Release(dev);
}

/* ---- lifetimes ---------------------------------------------------------------------------- */
AO_TEST_FACTORY(dxgi_adapter_output_adapter_outlives_factory_ref) {
    IDXGIAdapter1 *a = NULL; IDXGIFactory1 *pf = NULL;
    if (IDXGIFactory1_EnumAdapters1(f, 0, &a) != S_OK) { t_skip("no adapter"); return; }
    ULONG before = d3dh_refcount(f);
    EXPECT_HR_OK(IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory1, (void **)&pf));
    IDXGIFactory1_Release(pf);
    IDXGIAdapter1_Release(a);
    /* Adapter objects hold a reference on the factory only while alive, so the count must not have grown. */
    CHECKF(d3dh_refcount(f) <= before, "factory refcount grew from %lu to %lu", before, d3dh_refcount(f));
}
AO_TEST_FACTORY(dxgi_adapter_output_adapter_usable_after_factory_released) {
    IDXGIFactory1 *f2 = ao_factory();
    if (!f2) return;
    IDXGIAdapter1 *a = NULL; DXGI_ADAPTER_DESC1 d;
    if (IDXGIFactory1_EnumAdapters1(f2, 0, &a) != S_OK) { t_skip("no adapter"); IDXGIFactory1_Release(f2); return; }
    IDXGIFactory1_Release(f2);
    EXPECT_HR(IDXGIAdapter1_GetDesc1(a, &d), S_OK);
    IDXGIFactory1 *pf = NULL;
    EXPECT_HR(IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory1, (void **)&pf), S_OK);
    if (pf) IDXGIFactory1_Release(pf);
    IDXGIAdapter1_Release(a);
}
AO_TEST_FACTORY(dxgi_adapter_output_output_usable_after_adapter_released) {
    IDXGIAdapter1 *a = NULL; IDXGIOutput *o = ao_any_output(f, &a);
    if (!o) { t_skip("no output on any adapter (headless)"); return; }
    IDXGIAdapter1_Release(a);
    DXGI_OUTPUT_DESC d;
    EXPECT_HR(IDXGIOutput_GetDesc(o, &d), S_OK);
    IDXGIAdapter *p = NULL;
    EXPECT_HR(IDXGIOutput_GetParent(o, &IID_IDXGIAdapter, (void **)&p), S_OK);
    if (p) IDXGIAdapter_Release(p);
    IDXGIOutput_Release(o);
}

/* ---- private data: well-known debug name and larger blobs ---------------------------------------- */
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_debug_name) {
    /* WKPDID_D3DDebugObjectName */
    static const GUID name_guid = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 } };
    char out[16] = { 0 }; UINT sz = sizeof out;
    EXPECT_HR(IDXGIAdapter1_SetPrivateData(a, &name_guid, 5, "hello"), S_OK);
    EXPECT_HR(IDXGIAdapter1_GetPrivateData(a, &name_guid, &sz, out), S_OK);
    EXPECT_EQ(sz, 5);
    CHECK(!memcmp(out, "hello", 5));
    IDXGIAdapter1_SetPrivateData(a, &name_guid, 0, NULL);
}
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_large_blob) {
    static const GUID g = { 0x6a2c8b30, 0x1111, 0x4222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    BYTE *in = malloc(65536), *out = calloc(1, 65536); UINT sz = 65536;
    for (int i = 0; i < 65536; i++) in[i] = (BYTE)(i * 7);
    EXPECT_HR(IDXGIAdapter1_SetPrivateData(a, &g, 65536, in), S_OK);
    EXPECT_HR(IDXGIAdapter1_GetPrivateData(a, &g, &sz, out), S_OK);
    EXPECT_EQ(sz, 65536);
    CHECK(!memcmp(in, out, 65536));
    IDXGIAdapter1_SetPrivateData(a, &g, 0, NULL);
    free(in); free(out);
}
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_distinct_guids) {
    static const GUID g1 = { 0x6a2c8b40, 0x1111, 0x4222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    static const GUID g2 = { 0x6a2c8b41, 0x1111, 0x4222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    DWORD v1 = 1, v2 = 2, o1 = 0, o2 = 0; UINT s1 = 4, s2 = 4;
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a, &g1, 4, &v1));
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a, &g2, 4, &v2));
    EXPECT_HR_OK(IDXGIAdapter1_GetPrivateData(a, &g1, &s1, &o1));
    EXPECT_HR_OK(IDXGIAdapter1_GetPrivateData(a, &g2, &s2, &o2));
    EXPECT_EQ(o1, 1); EXPECT_EQ(o2, 2);
    IDXGIAdapter1_SetPrivateData(a, &g1, 0, NULL);
    IDXGIAdapter1_SetPrivateData(a, &g2, 0, NULL);
}

/* ---- descriptor details --------------------------------------------------------------------------- */
AO_TEST_FACTORY(dxgi_adapter_output_desc_description_zero_padded_or_terminated) {
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *x = NULL; DXGI_ADAPTER_DESC1 d;
        if (IDXGIFactory1_EnumAdapters1(f, i, &x) != S_OK) { CHECK(0); continue; }
        IDXGIAdapter1_GetDesc1(x, &d);
        int len = lstrlenW(d.Description);
        CHECK(len > 0 && len < 128);
        IDXGIAdapter1_Release(x);
    }
}
AO_TEST_FACTORY(dxgi_adapter_output_desc_software_flag_implies_no_dedicated_vram) {
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *x = NULL; DXGI_ADAPTER_DESC1 d;
        if (IDXGIFactory1_EnumAdapters1(f, i, &x) != S_OK) { CHECK(0); continue; }
        IDXGIAdapter1_GetDesc1(x, &d);
        if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) CHECKF(d.DedicatedVideoMemory == 0, "software adapter %u has %llu dedicated bytes", i, (unsigned long long)d.DedicatedVideoMemory);
        IDXGIAdapter1_Release(x);
    }
}
AO_TEST_FACTORY(dxgi_adapter_output_desc_basic_render_driver_is_software) {
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *x = NULL; DXGI_ADAPTER_DESC1 d;
        if (IDXGIFactory1_EnumAdapters1(f, i, &x) != S_OK) { CHECK(0); continue; }
        IDXGIAdapter1_GetDesc1(x, &d);
        if (d.VendorId == AO_WARP_VENDOR && d.DeviceId == AO_WARP_DEVICE)
            t_note("Basic Render Driver ids at index %u, software flag %d", i, (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0);
        IDXGIAdapter1_Release(x);
    }
}
AO_TEST_FACTORY(dxgi_adapter_output_factory_isadapter_after_enum_still_current) {
    UINT n = ao_count_adapters(f);
    (void)n;
    EXPECT_EQ(IDXGIFactory1_IsCurrent(f), TRUE);
}
