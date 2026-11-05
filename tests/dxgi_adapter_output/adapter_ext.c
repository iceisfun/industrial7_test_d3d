/* IDXGIAdapter::CheckInterfaceSupport, IDXGIAdapter2/4 descriptors, IDXGIAdapter3 video memory management. */
#include "ao.h"

/* ---- CheckInterfaceSupport -------------------------------------------------- */
AO_TEST_WARP(dxgi_adapter_output_cis_dxgidevice) {
    LARGE_INTEGER v; v.QuadPart = 0x5555;
    HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_IDXGIDevice, &v);
    t_note("CheckInterfaceSupport(IDXGIDevice) -> %s", d3dh_hr_name(hr));
    CHECK(hr == S_OK || hr == DXGI_ERROR_UNSUPPORTED);
    if (hr == S_OK) t_note("WARP UMD version %u.%u.%u.%u", (unsigned)(v.HighPart >> 16), (unsigned)(v.HighPart & 0xffff),
                           (unsigned)(v.LowPart >> 16), (unsigned)(v.LowPart & 0xffff));
    if (hr == S_OK) CHECK(v.QuadPart != 0x5555);
}
AO_TEST_WARP(dxgi_adapter_output_cis_d3d10device) {
    LARGE_INTEGER v; v.QuadPart = 0;
    HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_ID3D10Device, &v);
    EXPECT_HR(hr, S_OK);
    CHECK(v.QuadPart != 0);
}
AO_TEST_WARP(dxgi_adapter_output_cis_d3d10device1) {
    LARGE_INTEGER v; v.QuadPart = 0;
    HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_ID3D10Device1, &v);
    t_note("CheckInterfaceSupport(ID3D10Device1) -> %s", d3dh_hr_name(hr));
}
AO_TEST_WARP(dxgi_adapter_output_cis_d3d11device) {
    LARGE_INTEGER v; v.QuadPart = 0;
    HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_ID3D11Device, &v);
    t_note("CheckInterfaceSupport(ID3D11Device) -> %s", d3dh_hr_name(hr));
}
AO_TEST_WARP(dxgi_adapter_output_cis_d3d12device) {
    LARGE_INTEGER v; v.QuadPart = 0;
    HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_ID3D12Device, &v);
    t_note("CheckInterfaceSupport(ID3D12Device) -> %s", d3dh_hr_name(hr));
}
AO_TEST_WARP(dxgi_adapter_output_cis_same_version_dxgi_and_d3d10) {
    LARGE_INTEGER v1, v2; v1.QuadPart = v2.QuadPart = 0;
    if (IDXGIAdapter1_CheckInterfaceSupport(a, &IID_IDXGIDevice, &v1) != S_OK ||
        IDXGIAdapter1_CheckInterfaceSupport(a, &IID_ID3D10Device, &v2) != S_OK) { t_skip("interfaces not supported"); return; }
    t_note("UMD versions %llx and %llx", (unsigned long long)v1.QuadPart, (unsigned long long)v2.QuadPart);
    CHECK(v1.QuadPart != 0 && v2.QuadPart != 0);
}
AO_TEST_ADAPTER(dxgi_adapter_output_cis_unsupported_iid) {
    static const IID *bad[] = { &IID_IUnknown, &IID_IDXGIFactory, &IID_IDXGIAdapter, &IID_IDXGISwapChain, &IID_IDXGIOutput };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        LARGE_INTEGER v; v.QuadPart = 0x7777;
        HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(a, bad[i], &v);
        EXPECT_HR(hr, DXGI_ERROR_UNSUPPORTED);
    }
}
AO_TEST_ADAPTER(dxgi_adapter_output_cis_random_guid) {
    static const GUID g = { 0xdeadbeef, 0xcafe, 0xf00d, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    LARGE_INTEGER v; v.QuadPart = 0;
    EXPECT_HR(IDXGIAdapter1_CheckInterfaceSupport(a, &g, &v), DXGI_ERROR_UNSUPPORTED);
}
AO_TEST_ADAPTER(dxgi_adapter_output_cis_null_version) {
    HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_IDXGIDevice, NULL);
    t_note("CheckInterfaceSupport(IDXGIDevice, NULL version) -> %s", d3dh_hr_name(hr));
}
AO_TEST_ADAPTER(dxgi_adapter_output_cis_each_adapter_dxgidevice) {
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *x = NULL; LARGE_INTEGER v; v.QuadPart = 0;
        if (IDXGIFactory1_EnumAdapters1(f, i, &x) != S_OK) { CHECK(0); continue; }
        HRESULT hr = IDXGIAdapter1_CheckInterfaceSupport(x, &IID_IDXGIDevice, &v);
        t_note("adapter %u IDXGIDevice support %s version %llx", i, d3dh_hr_name(hr), (unsigned long long)v.QuadPart);
        IDXGIAdapter1_Release(x);
    }
}

/* ---- IDXGIAdapter2::GetDesc2 -------------------------------------------------- */
static int ao_gran_ok(int g) { return g >= 0 && g <= 4; }
AO_TEST_ADAPTER(dxgi_adapter_output_desc2_fields) {
    IDXGIAdapter2 *a2 = NULL; DXGI_ADAPTER_DESC2 d;
    if (FAILED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter2, (void **)&a2))) { t_skip("IDXGIAdapter2 unavailable"); return; }
    memset(&d, 0xCC, sizeof d);
    EXPECT_HR(IDXGIAdapter2_GetDesc2(a2, &d), S_OK);
    CHECK(ao_wide_terminated(d.Description, 128));
    CHECK((d.Flags & ~(UINT)(DXGI_ADAPTER_FLAG_REMOTE | DXGI_ADAPTER_FLAG_SOFTWARE)) == 0);
    CHECKF(ao_gran_ok(d.GraphicsPreemptionGranularity), "graphics granularity %d", d.GraphicsPreemptionGranularity);
    CHECKF(ao_gran_ok(d.ComputePreemptionGranularity), "compute granularity %d", d.ComputePreemptionGranularity);
    t_note("desc2 granularity graphics %d compute %d", d.GraphicsPreemptionGranularity, d.ComputePreemptionGranularity);
    IDXGIAdapter2_Release(a2);
}
AO_TEST_ADAPTER(dxgi_adapter_output_desc2_null) {
    IDXGIAdapter2 *a2 = NULL;
    if (FAILED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter2, (void **)&a2))) { t_skip("IDXGIAdapter2 unavailable"); return; }
    HRESULT hr = IDXGIAdapter2_GetDesc2(a2, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("GetDesc2(NULL) -> %s", d3dh_hr_name(hr));
    IDXGIAdapter2_Release(a2);
}
AO_TEST_WARP(dxgi_adapter_output_desc2_warp) {
    IDXGIAdapter2 *a2 = NULL; DXGI_ADAPTER_DESC2 d;
    if (FAILED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter2, (void **)&a2))) { t_skip("IDXGIAdapter2 unavailable"); return; }
    EXPECT_HR_OK(IDXGIAdapter2_GetDesc2(a2, &d));
    EXPECT_WSTR(d.Description, L"Microsoft Basic Render Driver");
    EXPECT_EQ(d.VendorId, AO_WARP_VENDOR);
    EXPECT_EQ(d.DeviceId, AO_WARP_DEVICE);
    CHECK((d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0);
    IDXGIAdapter2_Release(a2);
}

/* ---- IDXGIAdapter4::GetDesc3 -------------------------------------------------- */
AO_TEST_ADAPTER(dxgi_adapter_output_desc3_matches_desc2) {
    IDXGIAdapter4 *a4 = NULL; IDXGIAdapter2 *a2 = NULL; DXGI_ADAPTER_DESC3 d3; DXGI_ADAPTER_DESC2 d2;
    if (FAILED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter4, (void **)&a4))) { t_skip("IDXGIAdapter4 unavailable"); return; }
    memset(&d3, 0xCC, sizeof d3);
    EXPECT_HR(IDXGIAdapter4_GetDesc3(a4, &d3), S_OK);
    CHECK(ao_wide_terminated(d3.Description, 128));
    if (SUCCEEDED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter2, (void **)&a2))) {
        IDXGIAdapter2_GetDesc2(a2, &d2);
        CHECK(!lstrcmpW(d2.Description, d3.Description));
        EXPECT_EQ(d2.VendorId, d3.VendorId);
        EXPECT_EQ(d2.DeviceId, d3.DeviceId);
        EXPECT_EQ(d2.SubSysId, d3.SubSysId);
        EXPECT_EQ(d2.Revision, d3.Revision);
        CHECK(ao_luid_eq(d2.AdapterLuid, d3.AdapterLuid));
        EXPECT_EQ(d2.DedicatedVideoMemory, d3.DedicatedVideoMemory);
        EXPECT_EQ(d2.SharedSystemMemory, d3.SharedSystemMemory);
        EXPECT_EQ(d2.GraphicsPreemptionGranularity, d3.GraphicsPreemptionGranularity);
        EXPECT_EQ(d2.ComputePreemptionGranularity, d3.ComputePreemptionGranularity);
        /* Flags3: REMOTE=1, SOFTWARE=2 are shared bit positions. */
        EXPECT_EQ((UINT)d2.Flags, (UINT)d3.Flags & 3u);
        IDXGIAdapter2_Release(a2);
    }
    /* Flags3 defined bits: REMOTE 1, SOFTWARE 2, ACG_COMPATIBLE 4, SUPPORT_MONITORED_FENCES 8, SUPPORT_NON_MONITORED_FENCES 16, KEYED_MUTEX_CONFORMANCE 32 */
    t_note("desc3 flags 0x%x", (UINT)d3.Flags);
    IDXGIAdapter4_Release(a4);
}
AO_TEST_ADAPTER(dxgi_adapter_output_desc3_null) {
    IDXGIAdapter4 *a4 = NULL;
    if (FAILED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter4, (void **)&a4))) { t_skip("IDXGIAdapter4 unavailable"); return; }
    HRESULT hr = IDXGIAdapter4_GetDesc3(a4, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("GetDesc3(NULL) -> %s", d3dh_hr_name(hr));
    IDXGIAdapter4_Release(a4);
}
AO_TEST_WARP(dxgi_adapter_output_desc3_warp) {
    IDXGIAdapter4 *a4 = NULL; DXGI_ADAPTER_DESC3 d;
    if (FAILED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter4, (void **)&a4))) { t_skip("IDXGIAdapter4 unavailable"); return; }
    EXPECT_HR_OK(IDXGIAdapter4_GetDesc3(a4, &d));
    EXPECT_WSTR(d.Description, L"Microsoft Basic Render Driver");
    EXPECT_EQ(d.VendorId, AO_WARP_VENDOR);
    EXPECT_EQ(d.DeviceId, AO_WARP_DEVICE);
    CHECK((d.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0);
    CHECK((d.Flags & DXGI_ADAPTER_FLAG3_REMOTE) == 0);
    EXPECT_EQ(d.DedicatedVideoMemory, 0);
    t_note("WARP desc3 flags 0x%x", (UINT)d.Flags);
    IDXGIAdapter4_Release(a4);
}
AO_TEST_FACTORY(dxgi_adapter_output_desc3_all_adapters) {
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *x = NULL; IDXGIAdapter4 *x4 = NULL; DXGI_ADAPTER_DESC3 d3; DXGI_ADAPTER_DESC1 d1;
        if (IDXGIFactory1_EnumAdapters1(f, i, &x) != S_OK) { CHECK(0); continue; }
        if (SUCCEEDED(IDXGIAdapter1_QueryInterface(x, &IID_IDXGIAdapter4, (void **)&x4))) {
            EXPECT_HR_OK(IDXGIAdapter4_GetDesc3(x4, &d3));
            IDXGIAdapter1_GetDesc1(x, &d1);
            CHECK(ao_luid_eq(d1.AdapterLuid, d3.AdapterLuid));
            EXPECT_EQ((UINT)d1.Flags, (UINT)d3.Flags & 3u);
            IDXGIAdapter4_Release(x4);
        }
        IDXGIAdapter1_Release(x);
    }
}

/* ---- IDXGIAdapter3 video memory ------------------------------------------------ */
static IDXGIAdapter3 *ao_a3(IDXGIAdapter1 *a) {
    IDXGIAdapter3 *a3 = NULL;
    if (FAILED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter3, (void **)&a3))) { t_skip("IDXGIAdapter3 unavailable"); return NULL; }
    return a3;
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_query_local) {
    IDXGIAdapter3 *a3 = ao_a3(a); DXGI_QUERY_VIDEO_MEMORY_INFO m;
    if (!a3) return;
    memset(&m, 0xCC, sizeof m);
    HRESULT hr = IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m);
    EXPECT_HR(hr, S_OK);
    if (hr == S_OK) {
        EXPECT_EQ(m.CurrentReservation, 0);
        t_note("local: budget %llu usage %llu avail %llu reserved %llu", (unsigned long long)m.Budget, (unsigned long long)m.CurrentUsage,
               (unsigned long long)m.AvailableForReservation, (unsigned long long)m.CurrentReservation);
    }
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_query_nonlocal) {
    IDXGIAdapter3 *a3 = ao_a3(a); DXGI_QUERY_VIDEO_MEMORY_INFO m;
    if (!a3) return;
    memset(&m, 0xCC, sizeof m);
    HRESULT hr = IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &m);
    EXPECT_HR(hr, S_OK);
    if (hr == S_OK) {
        EXPECT_EQ(m.CurrentReservation, 0);
        t_note("non-local: budget %llu usage %llu avail %llu", (unsigned long long)m.Budget, (unsigned long long)m.CurrentUsage,
               (unsigned long long)m.AvailableForReservation);
    }
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_query_bad_group) {
    IDXGIAdapter3 *a3 = ao_a3(a); DXGI_QUERY_VIDEO_MEMORY_INFO m;
    if (!a3) return;
    HRESULT hr = IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, (DXGI_MEMORY_SEGMENT_GROUP)2, &m);
    EXPECT_HR_FAILED(hr);
    t_note("QueryVideoMemoryInfo(group 2) -> %s", d3dh_hr_name(hr));
    hr = IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, (DXGI_MEMORY_SEGMENT_GROUP)0x7fffffff, &m);
    EXPECT_HR_FAILED(hr);
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_query_bad_node) {
    IDXGIAdapter3 *a3 = ao_a3(a); DXGI_QUERY_VIDEO_MEMORY_INFO m;
    if (!a3) return;
    HRESULT hr = IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0xFFFF, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m);
    EXPECT_HR_FAILED(hr);
    t_note("QueryVideoMemoryInfo(node 0xffff) -> %s", d3dh_hr_name(hr));
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_query_null_info) {
    IDXGIAdapter3 *a3 = ao_a3(a);
    if (!a3) return;
    HRESULT hr = IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("QueryVideoMemoryInfo(NULL) -> %s", d3dh_hr_name(hr));
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_reserve_zero) {
    IDXGIAdapter3 *a3 = ao_a3(a);
    if (!a3) return;
    EXPECT_HR(IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 0), S_OK);
    EXPECT_HR(IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, 0), S_OK);
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_reserve_roundtrip) {
    IDXGIAdapter3 *a3 = ao_a3(a); DXGI_QUERY_VIDEO_MEMORY_INFO m;
    if (!a3) return;
    if (IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m) != S_OK) { t_skip("query failed"); IDXGIAdapter3_Release(a3); return; }
    UINT64 want = 1u << 20;
    if (m.AvailableForReservation < want) { t_skip("not enough reservable memory (%llu)", (unsigned long long)m.AvailableForReservation); IDXGIAdapter3_Release(a3); return; }
    HRESULT hr = IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, want);
    EXPECT_HR(hr, S_OK);
    if (hr == S_OK) {
        EXPECT_HR_OK(IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m));
        EXPECT_EQ(m.CurrentReservation, want);
        IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 0);
        EXPECT_HR_OK(IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m));
        EXPECT_EQ(m.CurrentReservation, 0);
    }
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_reserve_per_adapter_object) {
    /* Reservation is tracked per adapter object: set on one, query on another enumeration. */
    IDXGIAdapter3 *a3 = ao_a3(a), *b3 = NULL; IDXGIAdapter1 *b = NULL; DXGI_QUERY_VIDEO_MEMORY_INFO m;
    if (!a3) return;
    if (IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m) == S_OK && m.AvailableForReservation >= 65536 &&
        IDXGIFactory1_EnumAdapters1(f, 0, &b) == S_OK && SUCCEEDED(IDXGIAdapter1_QueryInterface(b, &IID_IDXGIAdapter3, (void **)&b3))) {
        if (IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 65536) == S_OK) {
            DXGI_QUERY_VIDEO_MEMORY_INFO m2;
            if (IDXGIAdapter3_QueryVideoMemoryInfo(b3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m2) == S_OK)
                t_note("reservation seen through second adapter object: %llu", (unsigned long long)m2.CurrentReservation);
            IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 0);
        }
    } else t_skip("reservation setup unavailable");
    if (b3) IDXGIAdapter3_Release(b3);
    if (b) IDXGIAdapter1_Release(b);
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_reserve_bad_group) {
    IDXGIAdapter3 *a3 = ao_a3(a);
    if (!a3) return;
    HRESULT hr = IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, (DXGI_MEMORY_SEGMENT_GROUP)2, 0);
    EXPECT_HR_FAILED(hr);
    t_note("SetVideoMemoryReservation(group 2) -> %s", d3dh_hr_name(hr));
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_reserve_over_available) {
    IDXGIAdapter3 *a3 = ao_a3(a); DXGI_QUERY_VIDEO_MEMORY_INFO m;
    if (!a3) return;
    if (IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m) == S_OK && m.AvailableForReservation < (UINT64)1 << 60) {
        HRESULT hr = IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, m.AvailableForReservation + (1ull << 30));
        t_note("SetVideoMemoryReservation(avail + 1GiB) -> %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDXGIAdapter3_SetVideoMemoryReservation(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 0);
    } else t_skip("query failed");
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_notification_register) {
    IDXGIAdapter3 *a3 = ao_a3(a); HANDLE ev; DWORD cookie = 0xFFFFFFFFu;
    if (!a3) return;
    ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    HRESULT hr = IDXGIAdapter3_RegisterVideoMemoryBudgetChangeNotificationEvent(a3, ev, &cookie);
    EXPECT_HR(hr, S_OK);
    if (hr == S_OK) {
        CHECK(cookie != 0xFFFFFFFFu);
        IDXGIAdapter3_UnregisterVideoMemoryBudgetChangeNotification(a3, cookie);
    }
    CloseHandle(ev);
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_notification_two_cookies) {
    IDXGIAdapter3 *a3 = ao_a3(a); HANDLE e1, e2; DWORD c1 = 0, c2 = 0;
    if (!a3) return;
    e1 = CreateEventW(NULL, FALSE, FALSE, NULL); e2 = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (IDXGIAdapter3_RegisterVideoMemoryBudgetChangeNotificationEvent(a3, e1, &c1) == S_OK) {
        HRESULT hr = IDXGIAdapter3_RegisterVideoMemoryBudgetChangeNotificationEvent(a3, e2, &c2);
        EXPECT_HR(hr, S_OK);
        if (hr == S_OK) { CHECK(c1 != c2); IDXGIAdapter3_UnregisterVideoMemoryBudgetChangeNotification(a3, c2); }
        IDXGIAdapter3_UnregisterVideoMemoryBudgetChangeNotification(a3, c1);
    } else t_skip("register failed");
    CloseHandle(e1); CloseHandle(e2);
    IDXGIAdapter3_Release(a3);
}
AO_TEST_ADAPTER(dxgi_adapter_output_vmem_notification_bad_args) {
    IDXGIAdapter3 *a3 = ao_a3(a); HANDLE ev; DWORD cookie = 0;
    if (!a3) return;
    HRESULT hr = IDXGIAdapter3_RegisterVideoMemoryBudgetChangeNotificationEvent(a3, NULL, &cookie);
    EXPECT_HR_FAILED(hr);
    t_note("Register(NULL event) -> %s", d3dh_hr_name(hr));
    ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    hr = IDXGIAdapter3_RegisterVideoMemoryBudgetChangeNotificationEvent(a3, ev, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("Register(NULL cookie) -> %s", d3dh_hr_name(hr));
    hr = IDXGIAdapter3_RegisterVideoMemoryBudgetChangeNotificationEvent(a3, (HANDLE)(UINT_PTR)0xFFFFFFFFFFFFFFFFull, &cookie);
    t_note("Register(INVALID_HANDLE_VALUE) -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) IDXGIAdapter3_UnregisterVideoMemoryBudgetChangeNotification(a3, cookie);
    CloseHandle(ev);
    IDXGIAdapter3_Release(a3);
}
AO_TEST_FACTORY(dxgi_adapter_output_vmem_all_adapters_consistent) {
    UINT n = ao_count_adapters(f);
    for (UINT i = 0; i < n; i++) {
        IDXGIAdapter1 *x = NULL; IDXGIAdapter3 *x3 = NULL; DXGI_QUERY_VIDEO_MEMORY_INFO m;
        if (IDXGIFactory1_EnumAdapters1(f, i, &x) != S_OK) { CHECK(0); continue; }
        if (SUCCEEDED(IDXGIAdapter1_QueryInterface(x, &IID_IDXGIAdapter3, (void **)&x3))) {
            HRESULT hr = IDXGIAdapter3_QueryVideoMemoryInfo(x3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m);
            CHECKF(hr == S_OK, "adapter %u -> %s", i, d3dh_hr_name(hr));
            if (hr == S_OK) CHECK(m.CurrentReservation == 0);
            IDXGIAdapter3_Release(x3);
        }
        IDXGIAdapter1_Release(x);
    }
}
