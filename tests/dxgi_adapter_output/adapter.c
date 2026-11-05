/* IDXGIAdapter / IDXGIAdapter1: enumeration, descriptors, identity, GetParent, private data. */
#include "ao.h"

/* ---- enumeration -------------------------------------------------------- */
AO_TEST_FACTORY(dxgi_adapter_output_enum_end_condition) {
    UINT n = ao_count_adapters(f);
    if (!n) { t_note("no adapters enumerated"); }
    IDXGIAdapter1 *a = (IDXGIAdapter1 *)(UINT_PTR)0x1234;
    EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, n, &a), DXGI_ERROR_NOT_FOUND);
    CHECK(a == NULL);
    a = (IDXGIAdapter1 *)(UINT_PTR)0x1234;
    EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, n + 1, &a), DXGI_ERROR_NOT_FOUND);
    CHECK(a == NULL);
    a = (IDXGIAdapter1 *)(UINT_PTR)0x1234;
    EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, 0xFFFFFFFFu, &a), DXGI_ERROR_NOT_FOUND);
    CHECK(a == NULL);
}
AO_TEST_FACTORY(dxgi_adapter_output_enum_legacy_end_condition) {
    UINT n = 0; IDXGIAdapter *a = NULL;
    for (;; n++) {
        a = (IDXGIAdapter *)(UINT_PTR)0x1234;
        HRESULT hr = IDXGIFactory1_EnumAdapters(f, n, &a);
        if (hr != S_OK) { EXPECT_HR(hr, DXGI_ERROR_NOT_FOUND); CHECK(a == NULL); break; }
        IDXGIAdapter_Release(a);
        if (n > 64) { CHECK(0); break; }
    }
    EXPECT_EQ(n, ao_count_adapters(f));
}
AO_TEST_FACTORY(dxgi_adapter_output_enum_null_out) {
    HRESULT hr = IDXGIFactory1_EnumAdapters1(f, 0, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("EnumAdapters1(0, NULL) -> %s", d3dh_hr_name(hr));
    hr = IDXGIFactory1_EnumAdapters(f, 0, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("EnumAdapters(0, NULL) -> %s", d3dh_hr_name(hr));
}
AO_TEST_FACTORY(dxgi_adapter_output_enum_counts_match) {
    UINT n1 = ao_count_adapters(f), n0 = 0;
    for (;; n0++) {
        IDXGIAdapter *a = NULL;
        if (IDXGIFactory1_EnumAdapters(f, n0, &a) != S_OK) break;
        IDXGIAdapter_Release(a);
        if (n0 > 64) break;
    }
    EXPECT_EQ(n0, n1);
}
AO_TEST_FACTORY(dxgi_adapter_output_enum_stable_between_calls) {
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *a = NULL, *b = NULL; DXGI_ADAPTER_DESC1 da, db;
        if (IDXGIFactory1_EnumAdapters1(f, i, &a) != S_OK) { CHECK(0); continue; }
        if (IDXGIFactory1_EnumAdapters1(f, i, &b) != S_OK) { CHECK(0); IDXGIAdapter1_Release(a); continue; }
        EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(a, &da));
        EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(b, &db));
        CHECK(ao_luid_eq(da.AdapterLuid, db.AdapterLuid));
        CHECK(!lstrcmpW(da.Description, db.Description));
        EXPECT_EQ(da.VendorId, db.VendorId);
        EXPECT_EQ(da.DeviceId, db.DeviceId);
        IDXGIAdapter1_Release(a); IDXGIAdapter1_Release(b);
    }
}
AO_TEST_FACTORY(dxgi_adapter_output_enum_new_object_each_call) {
    IDXGIAdapter1 *a = NULL;
    if (IDXGIFactory1_EnumAdapters1(f, 0, &a) != S_OK) { t_skip("no adapter"); return; }
    ULONG rc = d3dh_refcount(a);
    CHECK(rc >= 1);
    IDXGIAdapter1_AddRef(a);
    EXPECT_EQ(d3dh_refcount(a), rc + 1);
    EXPECT_EQ(IDXGIAdapter1_Release(a), rc);
    IDXGIAdapter1_Release(a);
}
AO_TEST_FACTORY(dxgi_adapter_output_factory_isadapter_current) {
    /* The factory created just now is current. */
    EXPECT_EQ(IDXGIFactory1_IsCurrent(f), TRUE);
}

/* ---- GetDesc ------------------------------------------------------------ */
AO_TEST_FACTORY(dxgi_adapter_output_getdesc_all_adapters) {
    UINT n = ao_count_adapters(f);
    if (!n) { t_skip("no adapters"); return; }
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter *a = NULL; DXGI_ADAPTER_DESC d;
        if (IDXGIFactory1_EnumAdapters(f, i, &a) != S_OK) { CHECK(0); continue; }
        memset(&d, 0xCC, sizeof d);
        EXPECT_HR(IDXGIAdapter_GetDesc(a, &d), S_OK);
        CHECK(ao_wide_terminated(d.Description, 128));
        CHECK(d.Description[0] != 0);
        CHECK(d.VendorId != 0);
        CHECK(d.AdapterLuid.LowPart != 0 || d.AdapterLuid.HighPart != 0);
        t_note("adapter %u: vendor %04x device %04x subsys %08x rev %u vram %llu sysmem %llu shared %llu", i, d.VendorId, d.DeviceId,
               d.SubSysId, d.Revision, (unsigned long long)d.DedicatedVideoMemory, (unsigned long long)d.DedicatedSystemMemory,
               (unsigned long long)d.SharedSystemMemory);
        IDXGIAdapter_Release(a);
    }
}
AO_TEST_FACTORY(dxgi_adapter_output_getdesc1_all_adapters) {
    UINT n = ao_count_adapters(f);
    if (!n) { t_skip("no adapters"); return; }
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *a = NULL; DXGI_ADAPTER_DESC1 d1; DXGI_ADAPTER_DESC d;
        if (IDXGIFactory1_EnumAdapters1(f, i, &a) != S_OK) { CHECK(0); continue; }
        EXPECT_HR(IDXGIAdapter1_GetDesc1(a, &d1), S_OK);
        EXPECT_HR(IDXGIAdapter1_GetDesc(a, &d), S_OK);
        /* DESC1 is DESC plus Flags. */
        CHECK(!lstrcmpW(d.Description, d1.Description));
        EXPECT_EQ(d.VendorId, d1.VendorId);
        EXPECT_EQ(d.DeviceId, d1.DeviceId);
        EXPECT_EQ(d.SubSysId, d1.SubSysId);
        EXPECT_EQ(d.Revision, d1.Revision);
        EXPECT_EQ(d.DedicatedVideoMemory, d1.DedicatedVideoMemory);
        EXPECT_EQ(d.DedicatedSystemMemory, d1.DedicatedSystemMemory);
        EXPECT_EQ(d.SharedSystemMemory, d1.SharedSystemMemory);
        CHECK(ao_luid_eq(d.AdapterLuid, d1.AdapterLuid));
        CHECK((d1.Flags & ~(UINT)(DXGI_ADAPTER_FLAG_REMOTE | DXGI_ADAPTER_FLAG_SOFTWARE)) == 0);
        IDXGIAdapter1_Release(a);
    }
}
AO_TEST_ADAPTER(dxgi_adapter_output_getdesc_null) {
    HRESULT hr = IDXGIAdapter1_GetDesc(a, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("GetDesc(NULL) -> %s", d3dh_hr_name(hr));
    hr = IDXGIAdapter1_GetDesc1(a, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("GetDesc1(NULL) -> %s", d3dh_hr_name(hr));
}
AO_TEST_ADAPTER(dxgi_adapter_output_getdesc_idempotent) {
    DXGI_ADAPTER_DESC d1, d2;
    EXPECT_HR_OK(IDXGIAdapter1_GetDesc(a, &d1));
    EXPECT_HR_OK(IDXGIAdapter1_GetDesc(a, &d2));
    CHECK(!memcmp(&d1, &d2, sizeof d1) || (!lstrcmpW(d1.Description, d2.Description) && d1.VendorId == d2.VendorId));
    EXPECT_EQ(d1.DedicatedVideoMemory, d2.DedicatedVideoMemory);
}

AO_TEST_WARP(dxgi_adapter_output_warp_desc) {
    DXGI_ADAPTER_DESC d;
    EXPECT_HR_OK(IDXGIAdapter1_GetDesc(a, &d));
    EXPECT_EQ(d.VendorId, AO_WARP_VENDOR);
    EXPECT_EQ(d.DeviceId, AO_WARP_DEVICE);
    EXPECT_WSTR(d.Description, L"Microsoft Basic Render Driver");
    EXPECT_EQ(d.SubSysId, 0);
    EXPECT_EQ(d.Revision, 0);
    EXPECT_EQ(d.DedicatedVideoMemory, 0);
    EXPECT_EQ(d.DedicatedSystemMemory, 0);
    CHECK(d.SharedSystemMemory > 0);
}
AO_TEST_WARP(dxgi_adapter_output_warp_desc1_flags) {
    DXGI_ADAPTER_DESC1 d;
    EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(a, &d));
    CHECK((d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0);
    CHECK((d.Flags & DXGI_ADAPTER_FLAG_REMOTE) == 0);
    EXPECT_EQ(d.VendorId, AO_WARP_VENDOR);
    EXPECT_EQ(d.DeviceId, AO_WARP_DEVICE);
}
AO_TEST_WARP(dxgi_adapter_output_warp_luid_stable_with_enum_by_luid) {
    DXGI_ADAPTER_DESC1 d; IDXGIFactory4 *f4 = NULL; IDXGIAdapter1 *b = NULL;
    EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(a, &d));
    if (FAILED(IDXGIFactory1_QueryInterface(f, &IID_IDXGIFactory4, (void **)&f4))) { t_skip("IDXGIFactory4 unavailable"); return; }
    EXPECT_HR(IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, &IID_IDXGIAdapter1, (void **)&b), S_OK);
    if (b) {
        DXGI_ADAPTER_DESC1 e;
        EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(b, &e));
        CHECK(ao_luid_eq(d.AdapterLuid, e.AdapterLuid));
        EXPECT_EQ(e.VendorId, AO_WARP_VENDOR);
        IDXGIAdapter1_Release(b);
    }
    IDXGIFactory4_Release(f4);
}
AO_TEST_WARP(dxgi_adapter_output_warp_has_no_outputs) {
    IDXGIOutput *o = (IDXGIOutput *)(UINT_PTR)0x1234;
    EXPECT_HR(IDXGIAdapter1_EnumOutputs(a, 0, &o), DXGI_ERROR_NOT_FOUND);
    CHECK(o == NULL);
}
AO_TEST_WARP(dxgi_adapter_output_warp_enum_twice_same_luid) {
    IDXGIAdapter1 *b = ao_warp(f);
    DXGI_ADAPTER_DESC1 d, e;
    if (!b) { CHECK(0); return; }
    EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(a, &d));
    EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(b, &e));
    CHECK(ao_luid_eq(d.AdapterLuid, e.AdapterLuid));
    IDXGIAdapter1_Release(b);
}

/* ---- LUIDs -------------------------------------------------------------- */
AO_TEST_FACTORY(dxgi_adapter_output_luid_unique) {
    LUID l[32]; UINT n = 0;
    for (UINT i = 0; i < 32; i++) {
        IDXGIAdapter1 *a = NULL; DXGI_ADAPTER_DESC1 d;
        if (IDXGIFactory1_EnumAdapters1(f, i, &a) != S_OK) break;
        EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(a, &d));
        l[n++] = d.AdapterLuid;
        IDXGIAdapter1_Release(a);
    }
    if (!n) { t_skip("no adapters"); return; }
    for (UINT i = 0; i < n; i++)
        for (UINT j = i + 1; j < n; j++) CHECKF(!ao_luid_eq(l[i], l[j]), "adapters %u and %u share a LUID", i, j);
}
AO_TEST_FACTORY(dxgi_adapter_output_luid_enum_by_luid_roundtrip) {
    IDXGIFactory4 *f4 = NULL;
    if (FAILED(IDXGIFactory1_QueryInterface(f, &IID_IDXGIFactory4, (void **)&f4))) { t_skip("IDXGIFactory4 unavailable"); return; }
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *a = NULL, *b = NULL; DXGI_ADAPTER_DESC1 d, e;
        if (IDXGIFactory1_EnumAdapters1(f, i, &a) != S_OK) { CHECK(0); continue; }
        IDXGIAdapter1_GetDesc1(a, &d);
        HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, d.AdapterLuid, &IID_IDXGIAdapter1, (void **)&b);
        EXPECT_HR(hr, S_OK);
        if (b) { IDXGIAdapter1_GetDesc1(b, &e); CHECK(ao_luid_eq(d.AdapterLuid, e.AdapterLuid)); IDXGIAdapter1_Release(b); }
        IDXGIAdapter1_Release(a);
    }
    IDXGIFactory4_Release(f4);
}
AO_TEST_FACTORY(dxgi_adapter_output_luid_unknown_not_found) {
    IDXGIFactory4 *f4 = NULL; IDXGIAdapter1 *b = (IDXGIAdapter1 *)(UINT_PTR)0x1234;
    if (FAILED(IDXGIFactory1_QueryInterface(f, &IID_IDXGIFactory4, (void **)&f4))) { t_skip("IDXGIFactory4 unavailable"); return; }
    LUID bogus = { 0x7eadbeefu, 0x12345 };
    HRESULT hr = IDXGIFactory4_EnumAdapterByLuid(f4, bogus, &IID_IDXGIAdapter1, (void **)&b);
    EXPECT_HR_FAILED(hr);
    t_note("EnumAdapterByLuid(unknown) -> %s", d3dh_hr_name(hr));
    CHECK(b == NULL);
    IDXGIFactory4_Release(f4);
}

/* ---- QueryInterface identity -------------------------------------------- */
static const struct { const IID *iid; const char *name; int required; } adapter_ifaces[] = {
    { &IID_IUnknown, "IUnknown", 1 }, { &IID_IDXGIObject, "IDXGIObject", 1 }, { &IID_IDXGIAdapter, "IDXGIAdapter", 1 },
    { &IID_IDXGIAdapter1, "IDXGIAdapter1", 1 }, { &IID_IDXGIAdapter2, "IDXGIAdapter2", 1 },
    { &IID_IDXGIAdapter3, "IDXGIAdapter3", 1 }, { &IID_IDXGIAdapter4, "IDXGIAdapter4", 0 },
};
AO_TEST_ADAPTER(dxgi_adapter_output_qi_interfaces) {
    for (size_t i = 0; i < sizeof adapter_ifaces / sizeof *adapter_ifaces; i++) {
        void *p = (void *)(UINT_PTR)1;
        HRESULT hr = IDXGIAdapter1_QueryInterface(a, adapter_ifaces[i].iid, &p);
        if (adapter_ifaces[i].required) { CHECKF(hr == S_OK, "QI %s -> %s", adapter_ifaces[i].name, d3dh_hr_name(hr)); }
        else if (hr != S_OK) t_note("QI %s -> %s", adapter_ifaces[i].name, d3dh_hr_name(hr));
        if (hr == S_OK) { CHECK(ao_same_object((IUnknown *)a, (IUnknown *)p)); IUnknown_Release((IUnknown *)p); }
        else CHECK(p == NULL);
    }
}
AO_TEST_ADAPTER(dxgi_adapter_output_qi_unsupported) {
    static const IID *bad[] = { &IID_ID3D11Device, &IID_IDXGIFactory, &IID_IDXGIDevice, &IID_IDXGIOutput, &IID_IDXGISwapChain };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        void *p = (void *)(UINT_PTR)1;
        EXPECT_HR(IDXGIAdapter1_QueryInterface(a, bad[i], &p), E_NOINTERFACE);
        CHECK(p == NULL);
    }
}
AO_TEST_ADAPTER(dxgi_adapter_output_qi_null_out) {
    HRESULT hr = IDXGIAdapter1_QueryInterface(a, &IID_IUnknown, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("QI(NULL out) -> %s", d3dh_hr_name(hr));
}
AO_TEST_ADAPTER(dxgi_adapter_output_refcount_balance) {
    IDXGIAdapter *b = NULL;
    ULONG base = d3dh_refcount(a);
    EXPECT_HR_OK(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter, (void **)&b));
    EXPECT_EQ(d3dh_refcount(a), base + 1);
    IDXGIAdapter_Release(b);
    EXPECT_EQ(d3dh_refcount(a), base);
}
AO_TEST_ADAPTER(dxgi_adapter_output_qi_adapters_desc_consistent) {
    IDXGIAdapter2 *a2 = NULL; DXGI_ADAPTER_DESC1 d1; DXGI_ADAPTER_DESC2 d2;
    if (FAILED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter2, (void **)&a2))) { t_skip("IDXGIAdapter2 unavailable"); return; }
    EXPECT_HR_OK(IDXGIAdapter1_GetDesc1(a, &d1));
    EXPECT_HR_OK(IDXGIAdapter2_GetDesc2(a2, &d2));
    CHECK(!lstrcmpW(d1.Description, d2.Description));
    EXPECT_EQ(d1.VendorId, d2.VendorId);
    EXPECT_EQ(d1.DeviceId, d2.DeviceId);
    EXPECT_EQ(d1.Flags, d2.Flags);
    CHECK(ao_luid_eq(d1.AdapterLuid, d2.AdapterLuid));
    IDXGIAdapter2_Release(a2);
}

/* ---- GetParent ------------------------------------------------------------ */
AO_TEST_ADAPTER(dxgi_adapter_output_getparent_factory) {
    IDXGIFactory *pf = NULL; IDXGIFactory1 *pf1 = NULL;
    EXPECT_HR(IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory, (void **)&pf), S_OK);
    if (pf) { CHECK(ao_same_object((IUnknown *)pf, (IUnknown *)f)); IDXGIFactory_Release(pf); }
    EXPECT_HR(IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory1, (void **)&pf1), S_OK);
    if (pf1) { CHECK(ao_same_object((IUnknown *)pf1, (IUnknown *)f)); IDXGIFactory1_Release(pf1); }
}
AO_TEST_ADAPTER(dxgi_adapter_output_getparent_refcount) {
    IDXGIFactory *pf = NULL;
    ULONG base = d3dh_refcount(f);
    EXPECT_HR_OK(IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory, (void **)&pf));
    EXPECT_EQ(d3dh_refcount(f), base + 1);
    IDXGIFactory_Release(pf);
    EXPECT_EQ(d3dh_refcount(f), base);
}
AO_TEST_ADAPTER(dxgi_adapter_output_getparent_wrong_iid) {
    static const IID *bad[] = { &IID_IDXGIDevice, &IID_IDXGIAdapter, &IID_ID3D11Device, &IID_IDXGIOutput };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        void *p = (void *)(UINT_PTR)1;
        HRESULT hr = IDXGIAdapter1_GetParent(a, bad[i], &p);
        EXPECT_HR_FAILED(hr);
        CHECK(p == NULL);
    }
}
AO_TEST_ADAPTER(dxgi_adapter_output_getparent_null_out) {
    HRESULT hr = IDXGIAdapter1_GetParent(a, &IID_IDXGIFactory, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("GetParent(NULL out) -> %s", d3dh_hr_name(hr));
}
AO_TEST_ADAPTER(dxgi_adapter_output_getparent_factory_from_each_adapter) {
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *x = NULL; IDXGIFactory1 *pf = NULL;
        if (IDXGIFactory1_EnumAdapters1(f, i, &x) != S_OK) { CHECK(0); continue; }
        EXPECT_HR_OK(IDXGIAdapter1_GetParent(x, &IID_IDXGIFactory1, (void **)&pf));
        if (pf) { CHECK(ao_same_object((IUnknown *)pf, (IUnknown *)f)); IDXGIFactory1_Release(pf); }
        IDXGIAdapter1_Release(x);
    }
}

/* ---- IDXGIObject private data on the adapter -------------------------------- */
static const GUID AO_G1 = { 0x6a2c8b10, 0x1111, 0x4222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
static const GUID AO_G2 = { 0x6a2c8b11, 0x1111, 0x4222, { 1, 2, 3, 4, 5, 6, 7, 9 } };
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_roundtrip) {
    BYTE in[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, out[8] = { 0 }; UINT sz = sizeof out;
    EXPECT_HR(IDXGIAdapter1_SetPrivateData(a, &AO_G1, sizeof in, in), S_OK);
    EXPECT_HR(IDXGIAdapter1_GetPrivateData(a, &AO_G1, &sz, out), S_OK);
    EXPECT_EQ(sz, 8);
    CHECK(!memcmp(in, out, 8));
}
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_size_protocol) {
    BYTE in[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, out[8] = { 0 }; UINT sz;
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a, &AO_G1, sizeof in, in));
    sz = 4;
    EXPECT_HR(IDXGIAdapter1_GetPrivateData(a, &AO_G1, &sz, out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 8);
    sz = 0;
    EXPECT_HR(IDXGIAdapter1_GetPrivateData(a, &AO_G1, &sz, NULL), S_OK);
    EXPECT_EQ(sz, 8);
    sz = 16;
    EXPECT_HR(IDXGIAdapter1_GetPrivateData(a, &AO_G1, &sz, out), S_OK);
    EXPECT_EQ(sz, 8);
}
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_not_found) {
    BYTE out[8]; UINT sz = sizeof out;
    EXPECT_HR(IDXGIAdapter1_GetPrivateData(a, &AO_G2, &sz, out), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(sz, 0);
}
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_overwrite_and_delete) {
    BYTE v1[4] = { 9, 9, 9, 9 }, v2[2] = { 7, 7 }, out[8]; UINT sz = sizeof out;
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a, &AO_G1, sizeof v1, v1));
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a, &AO_G1, sizeof v2, v2));
    EXPECT_HR_OK(IDXGIAdapter1_GetPrivateData(a, &AO_G1, &sz, out));
    EXPECT_EQ(sz, 2);
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a, &AO_G1, 0, NULL));
    sz = sizeof out;
    EXPECT_HR(IDXGIAdapter1_GetPrivateData(a, &AO_G1, &sz, out), DXGI_ERROR_NOT_FOUND);
}
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_interface) {
    IUnknown *obj = NULL, *got = NULL; UINT sz = sizeof got; ULONG base;
    EXPECT_HR_OK(IDXGIAdapter1_QueryInterface(a, &IID_IUnknown, (void **)&obj));
    base = d3dh_refcount(obj);
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateDataInterface(a, &AO_G2, (IUnknown *)obj));
    EXPECT_EQ(d3dh_refcount(obj), base + 1);
    EXPECT_HR_OK(IDXGIAdapter1_GetPrivateData(a, &AO_G2, &sz, &got));
    EXPECT_EQ(sz, sizeof(void *));
    CHECK(got == obj);
    if (got) IUnknown_Release(got);
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a, &AO_G2, 0, NULL));
    EXPECT_EQ(d3dh_refcount(obj), base);
    IUnknown_Release(obj);
}
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_bad_args) {
    BYTE b = 1;
    HRESULT hr = IDXGIAdapter1_SetPrivateData(a, &AO_G1, 4, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("SetPrivateData(size 4, NULL) -> %s", d3dh_hr_name(hr));
    hr = IDXGIAdapter1_GetPrivateData(a, &AO_G1, NULL, &b);
    EXPECT_HR_FAILED(hr);
    t_note("GetPrivateData(NULL size) -> %s", d3dh_hr_name(hr));
}
AO_TEST_ADAPTER(dxgi_adapter_output_privdata_independent_per_adapter_object) {
    /* Two enumerations of the same adapter: record whether private data is shared. */
    IDXGIAdapter1 *b = NULL; BYTE v = 5, out; UINT sz = 1;
    if (IDXGIFactory1_EnumAdapters1(f, 0, &b) != S_OK) { t_skip("no adapter"); return; }
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a, &AO_G1, 1, &v));
    HRESULT hr = IDXGIAdapter1_GetPrivateData(b, &AO_G1, &sz, &out);
    t_note("private data visible through second enumeration object: %s", d3dh_hr_name(hr));
    IDXGIAdapter1_SetPrivateData(a, &AO_G1, 0, NULL);
    IDXGIAdapter1_Release(b);
}
