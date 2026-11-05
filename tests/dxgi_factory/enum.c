/* Adapter enumeration: EnumAdapters/EnumAdapters1, EnumWarpAdapter, EnumAdapterByLuid, EnumAdapterByGpuPreference. */
#include "fh.h"

static UINT count_adapters1(IDXGIFactory1 *f) {
    UINT n = 0; IDXGIAdapter1 *a;
    while (n < 64 && SUCCEEDED(IDXGIFactory1_EnumAdapters1(f, n, &a))) { IDXGIAdapter1_Release(a); n++; }
    return n;
}
static UINT count_adapters0(IDXGIFactory *f) {
    UINT n = 0; IDXGIAdapter *a;
    while (n < 64 && SUCCEEDED(IDXGIFactory_EnumAdapters(f, n, &a))) { IDXGIAdapter_Release(a); n++; }
    return n;
}

TEST(dxgi_factory_enum_adapters_end_not_found) {
    NEED_F1(f);
    UINT n = count_adapters0((IDXGIFactory *)f);
    t_note("EnumAdapters count = %u", n);
    IDXGIAdapter *a = POISON;
    EXPECT_HR(IDXGIFactory1_EnumAdapters(f, n, &a), DXGI_E_NOT_FOUND);
    CHECK(a == NULL);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters1_end_not_found) {
    NEED_F1(f);
    UINT n = count_adapters1(f);
    t_note("EnumAdapters1 count = %u", n);
    IDXGIAdapter1 *a = POISON;
    EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, n, &a), DXGI_E_NOT_FOUND);
    CHECK(a == NULL);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters_far_indices_not_found) {
    NEED_F1(f);
    static const UINT idx[] = { 64, 100, 255, 256, 1000, 65535, 0x7fffffff, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    UINT n = count_adapters1(f);
    for (size_t i = 0; i < sizeof(idx) / sizeof(idx[0]); i++) {
        IDXGIAdapter *a = POISON; IDXGIAdapter1 *a1 = POISON;
        EXPECT_HR(IDXGIFactory1_EnumAdapters(f, idx[i], &a), DXGI_E_NOT_FOUND);
        CHECKF(a == NULL, "idx %u out not cleared", idx[i]);
        EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, idx[i], &a1), DXGI_E_NOT_FOUND);
        CHECKF(a1 == NULL, "idx %u out1 not cleared", idx[i]);
    }
    (void)n;
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters_null_out) {
    NEED_F1(f);
    HRESULT hr = IDXGIFactory1_EnumAdapters(f, 0, NULL);
    CHECKF(hr == DXGI_E_INVALID_CALL || hr == E_INVALIDARG || hr == E_POINTER || hr == DXGI_E_NOT_FOUND, "got %s", d3dh_hr_name(hr));
    t_note("EnumAdapters(0, NULL) = %s", d3dh_hr_name(hr));
    hr = IDXGIFactory1_EnumAdapters1(f, 0, NULL);
    CHECKF(hr == DXGI_E_INVALID_CALL || hr == E_INVALIDARG || hr == E_POINTER || hr == DXGI_E_NOT_FOUND, "got %s", d3dh_hr_name(hr));
    t_note("EnumAdapters1(0, NULL) = %s", d3dh_hr_name(hr));
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters_null_out_past_end) {
    NEED_F1(f);
    HRESULT hr = IDXGIFactory1_EnumAdapters(f, 0xffffff, NULL);
    CHECK(FAILED(hr));
    t_note("EnumAdapters(0xffffff, NULL) = %s", d3dh_hr_name(hr));
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters_counts_match) {
    NEED_F1(f);
    UINT n0 = count_adapters0((IDXGIFactory *)f), n1 = count_adapters1(f);
    EXPECT_EQ(n0, n1);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters_stable_between_calls) {
    NEED_F1(f);
    UINT n1 = count_adapters1(f), n2 = count_adapters1(f);
    EXPECT_EQ(n1, n2);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters_indices_contiguous) {
    NEED_F1(f);
    UINT n = count_adapters1(f);
    for (UINT i = n; i < n + 4; i++) {
        IDXGIAdapter1 *a = POISON;
        EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, i, &a), DXGI_E_NOT_FOUND);
    }
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters_refcounts) {
    NEED_F1(f);
    IDXGIAdapter1 *a = NULL;
    if (FAILED(IDXGIFactory1_EnumAdapters1(f, 0, &a))) { t_skip("no adapters"); IDXGIFactory1_Release(f); return; }
    EXPECT_EQ(IDXGIAdapter1_AddRef(a), 2);
    EXPECT_EQ(IDXGIAdapter1_Release(a), 1);
    EXPECT_EQ(IDXGIAdapter1_Release(a), 0);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapters_new_instance_each_time) {
    NEED_F1(f);
    IDXGIAdapter1 *a = NULL, *b = NULL;
    if (FAILED(IDXGIFactory1_EnumAdapters1(f, 0, &a))) { t_skip("no adapters"); IDXGIFactory1_Release(f); return; }
    EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, 0, &b), S_OK);
    if (b) {
        DXGI_ADAPTER_DESC1 da, db;
        IDXGIAdapter1_GetDesc1(a, &da); IDXGIAdapter1_GetDesc1(b, &db);
        CHECK(fh_luid_eq(da.AdapterLuid, db.AdapterLuid));
        CHECK(!wcscmp(da.Description, db.Description));
        t_note("repeated EnumAdapters1(0) %s the same object", a == b ? "returns" : "does not return");
        IDXGIAdapter1_Release(b);
    }
    IDXGIAdapter1_Release(a);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapter_get_parent_factory) {
    NEED_F1(f);
    IUnknown *fid = fh_ident((IUnknown *)f);
    for (UINT i = 0; i < 8; i++) {
        IDXGIAdapter1 *a;
        if (FAILED(IDXGIFactory1_EnumAdapters1(f, i, &a))) break;
        void *par = POISON;
        HRESULT hr = IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory1, &par);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) {
            IUnknown *pid = fh_ident((IUnknown *)par);
            CHECKF(pid == fid, "adapter %u parent is not the enumerating factory", i);
            if (pid) pid->lpVtbl->Release(pid);
            ((IUnknown *)par)->lpVtbl->Release((IUnknown *)par);
        }
        par = POISON;
        EXPECT_HR(IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory, &par), S_OK);
        if (par && par != POISON) ((IUnknown *)par)->lpVtbl->Release((IUnknown *)par);
        par = POISON;
        hr = IDXGIAdapter1_GetParent(a, &IID_IDXGIDevice, &par);
        CHECKF(FAILED(hr), "adapter GetParent(IDXGIDevice) = %s", d3dh_hr_name(hr));
        CHECK(par == NULL);
        IDXGIAdapter1_Release(a);
    }
    if (fid) fid->lpVtbl->Release(fid);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapter_keeps_factory_alive) {
    NEED_F1(f);
    IDXGIAdapter1 *a = NULL;
    if (FAILED(IDXGIFactory1_EnumAdapters1(f, 0, &a))) { t_skip("no adapters"); IDXGIFactory1_Release(f); return; }
    ULONG rc = fh_rc((IUnknown *)f);
    t_note("factory refcount with one live adapter: %lu", rc);
    CHECKF(rc >= 1, "rc %lu", rc);
    IDXGIFactory1_Release(f);
    /* adapter still usable after the application dropped the factory */
    DXGI_ADAPTER_DESC1 d;
    EXPECT_HR(IDXGIAdapter1_GetDesc1(a, &d), S_OK);
    void *par = NULL;
    EXPECT_HR(IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory1, &par), S_OK);
    if (par) IDXGIFactory1_Release((IDXGIFactory1 *)par);
    IDXGIAdapter1_Release(a);
}
TEST(dxgi_factory_enum_adapter_qi_levels) {
    NEED_F1(f);
    IDXGIAdapter1 *a = NULL;
    if (FAILED(IDXGIFactory1_EnumAdapters1(f, 0, &a))) { t_skip("no adapters"); IDXGIFactory1_Release(f); return; }
    static const struct { const char *n; const IID *i; int hard; } t[] = {
        { "IUnknown", &IID_IUnknown, 1 }, { "IDXGIObject", &IID_IDXGIObject, 1 }, { "IDXGIAdapter", &IID_IDXGIAdapter, 1 },
        { "IDXGIAdapter1", &IID_IDXGIAdapter1, 1 }, { "IDXGIAdapter2", &IID_IDXGIAdapter2, 1 },
        { "IDXGIAdapter3", &IID_IDXGIAdapter3, 0 }, { "IDXGIAdapter4", &IID_IDXGIAdapter4, 0 },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        void *p = POISON;
        HRESULT hr = IDXGIAdapter1_QueryInterface(a, t[i].i, &p);
        if (t[i].hard) CHECKF(hr == S_OK, "%s = %s", t[i].n, d3dh_hr_name(hr)); else t_note("adapter QI %s = %s", t[i].n, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p); else CHECK(p == NULL);
    }
    void *p = POISON;
    EXPECT_HR(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIFactory, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = POISON;
    EXPECT_HR(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIDevice, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    IDXGIAdapter1_Release(a);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapter_desc_consistency) {
    NEED_F1(f);
    for (UINT i = 0; i < 16; i++) {
        IDXGIAdapter1 *a;
        if (FAILED(IDXGIFactory1_EnumAdapters1(f, i, &a))) break;
        DXGI_ADAPTER_DESC d0; DXGI_ADAPTER_DESC1 d1;
        EXPECT_HR(IDXGIAdapter1_GetDesc(a, &d0), S_OK);
        EXPECT_HR(IDXGIAdapter1_GetDesc1(a, &d1), S_OK);
        CHECK(!wcscmp(d0.Description, d1.Description));
        CHECK(d0.VendorId == d1.VendorId && d0.DeviceId == d1.DeviceId && d0.SubSysId == d1.SubSysId && d0.Revision == d1.Revision);
        CHECK(d0.DedicatedVideoMemory == d1.DedicatedVideoMemory && d0.DedicatedSystemMemory == d1.DedicatedSystemMemory && d0.SharedSystemMemory == d1.SharedSystemMemory);
        CHECK(fh_luid_eq(d0.AdapterLuid, d1.AdapterLuid));
        CHECK(d1.Description[0] != 0);
        size_t len = wcsnlen(d1.Description, 128);
        CHECK(len < 128);
        t_note("adapter %u: %ls vendor %04x device %04x flags %#x luid %08lx:%08lx", i, d1.Description, d1.VendorId, d1.DeviceId, d1.Flags, (unsigned long)d1.AdapterLuid.HighPart, (unsigned long)d1.AdapterLuid.LowPart);
        CHECK((d1.Flags & ~(DXGI_ADAPTER_FLAG_REMOTE | DXGI_ADAPTER_FLAG_SOFTWARE)) == 0);
        IDXGIAdapter1_Release(a);
    }
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapter_luids_unique) {
    NEED_F1(f);
    LUID l[16]; UINT n = 0;
    for (; n < 16; n++) {
        IDXGIAdapter1 *a; DXGI_ADAPTER_DESC1 d;
        if (FAILED(IDXGIFactory1_EnumAdapters1(f, n, &a))) break;
        IDXGIAdapter1_GetDesc1(a, &d); l[n] = d.AdapterLuid; IDXGIAdapter1_Release(a);
    }
    for (UINT i = 0; i < n; i++) for (UINT j = i + 1; j < n; j++)
        CHECKF(!fh_luid_eq(l[i], l[j]) || (l[i].LowPart == 0 && l[i].HighPart == 0), "adapters %u and %u share a LUID", i, j);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapter_getdesc_null) {
    NEED_F1(f);
    IDXGIAdapter1 *a;
    if (FAILED(IDXGIFactory1_EnumAdapters1(f, 0, &a))) { t_skip("no adapters"); IDXGIFactory1_Release(f); return; }
    HRESULT hr = IDXGIAdapter1_GetDesc(a, NULL);
    CHECKF(hr == DXGI_E_INVALID_CALL || hr == E_INVALIDARG, "GetDesc(NULL) = %s", d3dh_hr_name(hr));
    hr = IDXGIAdapter1_GetDesc1(a, NULL);
    CHECKF(hr == DXGI_E_INVALID_CALL || hr == E_INVALIDARG, "GetDesc1(NULL) = %s", d3dh_hr_name(hr));
    IDXGIAdapter1_Release(a);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_enum_adapter_interface_support_umd) {
    NEED_F1(f);
    IDXGIAdapter1 *a;
    if (FAILED(IDXGIFactory1_EnumAdapters1(f, 0, &a))) { t_skip("no adapters"); IDXGIFactory1_Release(f); return; }
    LARGE_INTEGER v; v.QuadPart = 0x1234567890abcdefLL;
    HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_IDXGIDevice, &v);
    t_note("CheckInterfaceSupport(IDXGIDevice) = %s version %08lx:%08lx", d3dh_hr_name(hr), (unsigned long)v.HighPart, (unsigned long)v.LowPart);
    hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_ID3D11Device, &v);
    CHECKF(hr == DXGI_E_UNSUPPORTED, "CheckInterfaceSupport(ID3D11Device) = %s", d3dh_hr_name(hr));
    hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_IDXGIDevice, NULL);
    t_note("CheckInterfaceSupport(NULL version) = %s", d3dh_hr_name(hr));
    IDXGIAdapter1_Release(a);
    IDXGIFactory1_Release(f);
}

/* ---------------- EnumWarpAdapter ---------------- */
TEST(dxgi_factory_warp_null_out) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    EXPECT_HR(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, NULL), DXGI_E_INVALID_CALL);
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_warp_adapter_desc) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    IDXGIAdapter1 *a = POISON;
    HRESULT hr = IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&a);
    if (FAILED(hr)) { t_skip("no WARP adapter: %s", d3dh_hr_name(hr)); IDXGIFactory4_Release(f4); return; }
    DXGI_ADAPTER_DESC1 d;
    EXPECT_HR(IDXGIAdapter1_GetDesc1(a, &d), S_OK);
    EXPECT_EQ(d.Flags, DXGI_ADAPTER_FLAG_SOFTWARE);
    EXPECT_EQ(d.VendorId, 0x1414);
    EXPECT_EQ(d.DeviceId, 0x008c);
    t_note("WARP adapter: %ls luid %08lx:%08lx", d.Description, (unsigned long)d.AdapterLuid.HighPart, (unsigned long)d.AdapterLuid.LowPart);
    t_note("WARP dedicated video memory %llu", (unsigned long long)d.DedicatedVideoMemory);
    EXPECT_EQ(IDXGIAdapter1_Release(a), 0);
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_warp_adapter_iids) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    static const struct { const char *n; const IID *i; } t[] = {
        { "IUnknown", &IID_IUnknown }, { "IDXGIObject", &IID_IDXGIObject }, { "IDXGIAdapter", &IID_IDXGIAdapter },
        { "IDXGIAdapter1", &IID_IDXGIAdapter1 }, { "IDXGIAdapter2", &IID_IDXGIAdapter2 },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        void *p = POISON;
        HRESULT hr = IDXGIFactory4_EnumWarpAdapter(f4, t[i].i, &p);
        if (FAILED(hr) && i == 0) { t_skip("no WARP adapter"); break; }
        CHECKF(hr == S_OK, "%s = %s", t[i].n, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) EXPECT_EQ(((IUnknown *)p)->lpVtbl->Release((IUnknown *)p), 0);
    }
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_warp_adapter_bad_iid) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    static const IID *bad[] = { &IID_IDXGIFactory, &IID_IDXGIDevice, &IID_IDXGISwapChain, &GUID_NULL, &IID_IDXGIOutput };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        void *p = POISON;
        HRESULT hr = IDXGIFactory4_EnumWarpAdapter(f4, bad[i], &p);
        CHECKF(FAILED(hr), "bad iid %zu returned %s", i, d3dh_hr_name(hr));
        CHECKF(hr == E_NOINTERFACE, "bad iid %zu: %s", i, d3dh_hr_name(hr));
        CHECK(p == NULL);
    }
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_warp_adapter_luid_stable) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    IDXGIAdapter1 *a = NULL, *b = NULL;
    if (FAILED(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&a))) { t_skip("no WARP"); IDXGIFactory4_Release(f4); return; }
    IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&b);
    DXGI_ADAPTER_DESC1 da, db;
    IDXGIAdapter1_GetDesc1(a, &da);
    if (b) { IDXGIAdapter1_GetDesc1(b, &db); CHECK(fh_luid_eq(da.AdapterLuid, db.AdapterLuid)); IDXGIAdapter1_Release(b); }
    IDXGIAdapter1_Release(a);
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_warp_adapter_parent_is_factory) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    IDXGIAdapter1 *a = NULL;
    if (FAILED(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&a))) { t_skip("no WARP"); IDXGIFactory4_Release(f4); return; }
    void *par = NULL;
    HRESULT hr = IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory4, &par);
    t_note("WARP adapter GetParent(IDXGIFactory4) = %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        IUnknown *x = fh_ident((IUnknown *)par), *y = fh_ident((IUnknown *)f4);
        t_note("WARP adapter parent %s the enumerating factory", x == y ? "is" : "is not");
        if (x) x->lpVtbl->Release(x);
        if (y) y->lpVtbl->Release(y);
        ((IUnknown *)par)->lpVtbl->Release((IUnknown *)par);
    }
    IDXGIAdapter1_Release(a);
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_warp_adapter_has_no_outputs) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    IDXGIAdapter1 *a = NULL;
    if (FAILED(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&a))) { t_skip("no WARP"); IDXGIFactory4_Release(f4); return; }
    IDXGIOutput *o = POISON;
    EXPECT_HR(IDXGIAdapter1_EnumOutputs(a, 0, &o), DXGI_E_NOT_FOUND);
    CHECK(o == NULL);
    IDXGIAdapter1_Release(a);
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_warp_adapter_in_enumeration_observed) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    IDXGIAdapter1 *w = NULL;
    if (FAILED(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&w))) { t_skip("no WARP"); IDXGIFactory4_Release(f4); return; }
    DXGI_ADAPTER_DESC1 wd; IDXGIAdapter1_GetDesc1(w, &wd);
    int found = 0;
    for (UINT i = 0; i < 16; i++) {
        IDXGIAdapter1 *a; DXGI_ADAPTER_DESC1 d;
        if (FAILED(IDXGIFactory4_EnumAdapters1(f4, i, &a))) break;
        IDXGIAdapter1_GetDesc1(a, &d);
        if (fh_luid_eq(d.AdapterLuid, wd.AdapterLuid)) found = 1;
        IDXGIAdapter1_Release(a);
    }
    t_note("WARP adapter %s in EnumAdapters1", found ? "listed" : "not listed");
    IDXGIAdapter1_Release(w);
    IDXGIFactory4_Release(f4);
}

/* ---------------- EnumAdapterByLuid ---------------- */
TEST(dxgi_factory_by_luid_roundtrip) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    UINT seen = 0;
    for (UINT i = 0; i < 16; i++) {
        IDXGIAdapter1 *a; DXGI_ADAPTER_DESC1 d;
        if (FAILED(IDXGIFactory4_EnumAdapters1(f4, i, &a))) break;
        IDXGIAdapter1_GetDesc1(a, &d);
        IDXGIAdapter1 *b = POISON;
        HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, &IID_IDXGIAdapter1, (void **)&b);
        CHECKF(hr == S_OK, "adapter %u by luid = %s", i, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            DXGI_ADAPTER_DESC1 d2; IDXGIAdapter1_GetDesc1(b, &d2);
            CHECK(fh_luid_eq(d.AdapterLuid, d2.AdapterLuid));
            CHECK(!wcscmp(d.Description, d2.Description));
            CHECK(b != a);
            IDXGIAdapter1_Release(b);
        }
        IDXGIAdapter1_Release(a);
        seen++;
    }
    if (!seen) t_skip("no adapters");
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_by_luid_unknown_not_found) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    static const LUID l[] = { { 0x7e57beef, 0x1337 }, { 0xffffffff, -1 }, { 0x12345678, 0x7fffffff }, { 0xdeadbeef, 0 } };
    for (size_t i = 0; i < sizeof(l) / sizeof(l[0]); i++) {
        IDXGIAdapter *a = POISON;
        HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, l[i], &IID_IDXGIAdapter, (void **)&a);
        CHECKF(hr == DXGI_E_NOT_FOUND, "luid %zu = %s", i, d3dh_hr_name(hr));
        CHECK(a == NULL);
    }
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_by_luid_null_out) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    IDXGIAdapter1 *a;
    LUID l = { 1, 2 };
    if (SUCCEEDED(IDXGIFactory4_EnumAdapters1(f4, 0, &a))) {
        DXGI_ADAPTER_DESC1 d; IDXGIAdapter1_GetDesc1(a, &d); l = d.AdapterLuid; IDXGIAdapter1_Release(a);
        EXPECT_HR(IDXGIFactory4_EnumAdapterByLuid(f4, l, &IID_IDXGIAdapter1, NULL), DXGI_E_INVALID_CALL);
    } else {
        HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, l, &IID_IDXGIAdapter1, NULL);
        CHECK(FAILED(hr));
        t_note("no adapters; EnumAdapterByLuid(NULL out) = %s", d3dh_hr_name(hr));
    }
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_by_luid_bad_iid) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    IDXGIAdapter1 *a;
    if (FAILED(IDXGIFactory4_EnumAdapters1(f4, 0, &a))) { t_skip("no adapters"); IDXGIFactory4_Release(f4); return; }
    DXGI_ADAPTER_DESC1 d; IDXGIAdapter1_GetDesc1(a, &d); IDXGIAdapter1_Release(a);
    static const IID *bad[] = { &IID_IDXGIFactory, &IID_IDXGIDevice, &GUID_NULL };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        void *p = POISON;
        HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, bad[i], &p);
        CHECKF(hr == E_NOINTERFACE, "bad iid %zu = %s", i, d3dh_hr_name(hr));
        CHECK(p == NULL);
    }
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_by_luid_adapter_iids) {
    NEED_FAC(f4, IDXGIFactory4, IID_IDXGIFactory4);
    IDXGIAdapter1 *a;
    if (FAILED(IDXGIFactory4_EnumAdapters1(f4, 0, &a))) { t_skip("no adapters"); IDXGIFactory4_Release(f4); return; }
    DXGI_ADAPTER_DESC1 d; IDXGIAdapter1_GetDesc1(a, &d); IDXGIAdapter1_Release(a);
    static const struct { const char *n; const IID *i; } t[] = {
        { "IUnknown", &IID_IUnknown }, { "IDXGIObject", &IID_IDXGIObject }, { "IDXGIAdapter", &IID_IDXGIAdapter },
        { "IDXGIAdapter1", &IID_IDXGIAdapter1 }, { "IDXGIAdapter2", &IID_IDXGIAdapter2 },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        void *p = POISON;
        HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, t[i].i, &p);
        CHECKF(hr == S_OK, "%s = %s", t[i].n, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
    IDXGIFactory4_Release(f4);
}
TEST(dxgi_factory_by_luid_via_factory2_and_5) {
    IDXGIFactory4 *f4 = NULL;
    if (FAILED(fh_make(&IID_IDXGIFactory4, (void **)&f4))) { t_skip("no factory4"); return; }
    IDXGIFactory5 *f5 = fh_qi((IUnknown *)f4, &IID_IDXGIFactory5);
    if (!f5) { t_skip("no factory5"); IDXGIFactory4_Release(f4); return; }
    IDXGIAdapter1 *w = NULL;
    if (SUCCEEDED(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&w))) {
        DXGI_ADAPTER_DESC1 d; IDXGIAdapter1_GetDesc1(w, &d);
        IDXGIAdapter1 *b = NULL;
        HRESULT hr = IDXGIFactory5_EnumAdapterByLuid(f5, d.AdapterLuid, &IID_IDXGIAdapter1, (void **)&b);
        t_note("EnumAdapterByLuid(WARP luid) = %s", d3dh_hr_name(hr));
        if (b) IDXGIAdapter1_Release(b);
        IDXGIAdapter1_Release(w);
    }
    IDXGIFactory5_Release(f5);
    IDXGIFactory4_Release(f4);
}

/* ---------------- EnumAdapterByGpuPreference ---------------- */
static UINT count_pref(IDXGIFactory6 *f6, DXGI_GPU_PREFERENCE p) {
    UINT n = 0; IDXGIAdapter1 *a;
    while (n < 64 && SUCCEEDED(IDXGIFactory6_EnumAdapterByGpuPreference(f6, n, p, &IID_IDXGIAdapter1, (void **)&a))) { IDXGIAdapter1_Release(a); n++; }
    return n;
}
TEST(dxgi_factory_gpupref_end_not_found) {
    NEED_FAC(f6, IDXGIFactory6, IID_IDXGIFactory6);
    static const DXGI_GPU_PREFERENCE p[] = { DXGI_GPU_PREFERENCE_UNSPECIFIED, DXGI_GPU_PREFERENCE_MINIMUM_POWER, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE };
    for (size_t i = 0; i < 3; i++) {
        UINT n = count_pref(f6, p[i]);
        t_note("pref %d count %u", (int)p[i], n);
        IDXGIAdapter1 *a = POISON;
        EXPECT_HR(IDXGIFactory6_EnumAdapterByGpuPreference(f6, n, p[i], &IID_IDXGIAdapter1, (void **)&a), DXGI_E_NOT_FOUND);
        CHECK(a == NULL);
        a = POISON;
        EXPECT_HR(IDXGIFactory6_EnumAdapterByGpuPreference(f6, 0xffffffffu, p[i], &IID_IDXGIAdapter1, (void **)&a), DXGI_E_NOT_FOUND);
        CHECK(a == NULL);
    }
    IDXGIFactory6_Release(f6);
}
TEST(dxgi_factory_gpupref_null_out) {
    NEED_FAC(f6, IDXGIFactory6, IID_IDXGIFactory6);
    HRESULT hr = IDXGIFactory6_EnumAdapterByGpuPreference(f6, 0, DXGI_GPU_PREFERENCE_UNSPECIFIED, &IID_IDXGIAdapter1, NULL);
    CHECKF(hr == DXGI_E_INVALID_CALL || hr == E_INVALIDARG || hr == E_POINTER || hr == DXGI_E_NOT_FOUND, "got %s", d3dh_hr_name(hr));
    t_note("EnumAdapterByGpuPreference(NULL out) = %s", d3dh_hr_name(hr));
    IDXGIFactory6_Release(f6);
}
TEST(dxgi_factory_gpupref_bad_iid) {
    NEED_FAC(f6, IDXGIFactory6, IID_IDXGIFactory6);
    if (!count_pref(f6, DXGI_GPU_PREFERENCE_UNSPECIFIED)) { t_skip("no adapters"); IDXGIFactory6_Release(f6); return; }
    static const IID *bad[] = { &IID_IDXGIFactory, &IID_IDXGIDevice, &GUID_NULL };
    for (size_t i = 0; i < 3; i++) {
        void *p = POISON;
        HRESULT hr = IDXGIFactory6_EnumAdapterByGpuPreference(f6, 0, DXGI_GPU_PREFERENCE_UNSPECIFIED, bad[i], &p);
        CHECKF(hr == E_NOINTERFACE, "bad iid %zu = %s", i, d3dh_hr_name(hr));
        CHECK(p == NULL);
    }
    IDXGIFactory6_Release(f6);
}
TEST(dxgi_factory_gpupref_invalid_enum_observed) {
    NEED_FAC(f6, IDXGIFactory6, IID_IDXGIFactory6);
    static const int bad[] = { 3, 4, 99, -1, 0x7fffffff };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        IDXGIAdapter1 *a = POISON;
        HRESULT hr = IDXGIFactory6_EnumAdapterByGpuPreference(f6, 0, (DXGI_GPU_PREFERENCE)bad[i], &IID_IDXGIAdapter1, (void **)&a);
        t_note("EnumAdapterByGpuPreference(pref=%d) = %s", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDXGIAdapter1_Release(a); else CHECK(a == NULL);
    }
    IDXGIFactory6_Release(f6);
}
TEST(dxgi_factory_gpupref_counts_vs_enum) {
    NEED_FAC(f6, IDXGIFactory6, IID_IDXGIFactory6);
    UINT base = count_adapters1((IDXGIFactory1 *)f6);
    UINT u = count_pref(f6, DXGI_GPU_PREFERENCE_UNSPECIFIED);
    UINT mp = count_pref(f6, DXGI_GPU_PREFERENCE_MINIMUM_POWER);
    UINT hp = count_pref(f6, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE);
    t_note("EnumAdapters1 %u, unspecified %u, min power %u, high perf %u", base, u, mp, hp);
    CHECK(u <= base + 1 && mp <= base + 1 && hp <= base + 1);
    if (u != mp || u != hp) t_note("preference modes enumerate different counts");
    IDXGIFactory6_Release(f6);
}
TEST(dxgi_factory_gpupref_unspecified_matches_enum_order) {
    NEED_FAC(f6, IDXGIFactory6, IID_IDXGIFactory6);
    IDXGIAdapter1 *a = NULL, *b = NULL;
    if (FAILED(IDXGIFactory6_EnumAdapterByGpuPreference(f6, 0, DXGI_GPU_PREFERENCE_UNSPECIFIED, &IID_IDXGIAdapter1, (void **)&a))) { t_skip("no adapters"); IDXGIFactory6_Release(f6); return; }
    IDXGIFactory6_EnumAdapters1(f6, 0, &b);
    if (b) {
        DXGI_ADAPTER_DESC1 da, db; IDXGIAdapter1_GetDesc1(a, &da); IDXGIAdapter1_GetDesc1(b, &db);
        t_note("pref-unspecified[0]=%ls EnumAdapters1[0]=%ls", da.Description, db.Description);
        CHECK(fh_luid_eq(da.AdapterLuid, db.AdapterLuid));
        IDXGIAdapter1_Release(b);
    }
    IDXGIAdapter1_Release(a);
    IDXGIFactory6_Release(f6);
}
TEST(dxgi_factory_gpupref_set_is_same_adapters) {
    NEED_FAC(f6, IDXGIFactory6, IID_IDXGIFactory6);
    DXGI_GPU_PREFERENCE p[3] = { DXGI_GPU_PREFERENCE_UNSPECIFIED, DXGI_GPU_PREFERENCE_MINIMUM_POWER, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE };
    for (int k = 0; k < 3; k++) {
        for (UINT i = 0; i < 16; i++) {
            IDXGIAdapter1 *a; DXGI_ADAPTER_DESC1 d;
            if (FAILED(IDXGIFactory6_EnumAdapterByGpuPreference(f6, i, p[k], &IID_IDXGIAdapter1, (void **)&a))) break;
            IDXGIAdapter1_GetDesc1(a, &d);
            IDXGIAdapter1 *b = NULL;
            HRESULT hr = IDXGIFactory6_EnumAdapterByLuid(f6, d.AdapterLuid, &IID_IDXGIAdapter1, (void **)&b);
            CHECKF(hr == S_OK, "pref %d adapter %u not resolvable by luid: %s", k, i, d3dh_hr_name(hr));
            if (b) IDXGIAdapter1_Release(b);
            IDXGIAdapter1_Release(a);
        }
    }
    IDXGIFactory6_Release(f6);
}
TEST(dxgi_factory_gpupref_iid_adapter4) {
    NEED_FAC(f6, IDXGIFactory6, IID_IDXGIFactory6);
    IDXGIAdapter4 *a = POISON;
    HRESULT hr = IDXGIFactory6_EnumAdapterByGpuPreference(f6, 0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter4, (void **)&a);
    if (hr == DXGI_E_NOT_FOUND) { t_skip("no adapters"); IDXGIFactory6_Release(f6); return; }
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        DXGI_ADAPTER_DESC3 d3;
        EXPECT_HR(IDXGIAdapter4_GetDesc3(a, &d3), S_OK);
        t_note("Desc3 flags %#x graphics preemption %d compute preemption %d", d3.Flags, d3.GraphicsPreemptionGranularity, d3.ComputePreemptionGranularity);
        IDXGIAdapter4_Release(a);
    }
    IDXGIFactory6_Release(f6);
}
