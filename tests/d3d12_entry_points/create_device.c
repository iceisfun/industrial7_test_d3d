#include "ep.h"

/* ---------- exports and ordinals ---------- */
static const struct { const char *name; unsigned ord; } k_d3d12_exports[] = {
    {"GetBehaviorValue", 100}, {"D3D12CreateDevice", 101}, {"D3D12GetDebugInterface", 102},
    {"SetAppCompatStringPointer", 103}, {"D3D12CoreCreateLayeredDevice", 104},
    {"D3D12CoreGetLayeredDeviceSize", 105}, {"D3D12CoreRegisterLayers", 106},
    {"D3D12CreateRootSignatureDeserializer", 107}, {"D3D12CreateVersionedRootSignatureDeserializer", 108},
    {"D3D12DeviceRemovedExtendedData", 109}, {"D3D12EnableExperimentalFeatures", 110},
    {"D3D12GetInterface", 111}, {"D3D12PIXEventsReplaceBlock", 112}, {"D3D12PIXGetThreadInfo", 113},
    {"D3D12PIXNotifyWakeFromFenceSignal", 114}, {"D3D12PIXReportCounter", 115},
    {"D3D12SerializeRootSignature", 116}, {"D3D12SerializeVersionedRootSignature", 117},
};
#define NEXP (sizeof k_d3d12_exports / sizeof k_d3d12_exports[0])

TEST(d3d12_entry_points_dll_loads) {
    HMODULE m = LoadLibraryA("d3d12.dll");
    if (!m) { t_skip("d3d12.dll not present"); return; }
    CHECK(m == t_module("d3d12.dll"));
    char path[MAX_PATH]; DWORD n = GetModuleFileNameA(m, path, sizeof path);
    CHECK(n > 0 && n < sizeof path);
    FreeLibrary(m);
}

TEST(d3d12_entry_points_export_names_resolve) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    if (t_is_wine()) { t_skip("emulator does not export the full Windows 10 surface"); return; }
    for (unsigned i = 0; i < NEXP; i++) {
        FARPROC p = GetProcAddress(m, k_d3d12_exports[i].name);
        CHECKF(p != NULL, "export %s", k_d3d12_exports[i].name);
    }
}

TEST(d3d12_entry_points_export_ordinals_match_names) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    if (t_is_wine()) { t_skip("emulator does not export the full Windows 10 surface"); return; }
    for (unsigned i = 0; i < NEXP; i++) {
        FARPROC byname = GetProcAddress(m, k_d3d12_exports[i].name);
        FARPROC byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)k_d3d12_exports[i].ord);
        CHECKF(byord == byname && byord != NULL, "ordinal %u vs %s", k_d3d12_exports[i].ord, k_d3d12_exports[i].name);
    }
}

TEST(d3d12_entry_points_export_distinct_addresses) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    if (t_is_wine()) { t_skip("emulator does not export the full Windows 10 surface"); return; }
    for (unsigned i = 0; i < NEXP; i++)
        for (unsigned j = i + 1; j < NEXP; j++) {
            FARPROC a = GetProcAddress(m, k_d3d12_exports[i].name), b = GetProcAddress(m, k_d3d12_exports[j].name);
            /* the PIX stubs may be folded by the linker; only the core entry points must differ */
            if (i < 12 && j < 12) CHECKF(a != b, "%s == %s", k_d3d12_exports[i].name, k_d3d12_exports[j].name);
        }
}

TEST(d3d12_entry_points_export_missing_name_fails) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, "D3D12CreateDevice_nonexistent") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(GetProcAddress(m, "d3d12createdevice") == NULL); /* export names are case sensitive */
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}

TEST(d3d12_entry_points_export_ordinal_out_of_range_fails) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)1) == NULL);
    EXPECT_ERR(ERROR_INVALID_ORDINAL);   /* Win10 22H2: a missing ordinal is ERROR_INVALID_ORDINAL (182), a missing name ERROR_PROC_NOT_FOUND (127) */
    SET_ERR(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0xFFFE) == NULL);
    EXPECT_ERR(ERROR_INVALID_ORDINAL);
}

TEST(d3d12_entry_points_d3d12core_exports) {
    HMODULE m = t_module("d3d12core.dll");
    if (!m) { t_skip("d3d12core.dll not present"); return; }
    FARPROC gi = GetProcAddress(m, "D3D12GetInterface");
    FARPROC sv = GetProcAddress(m, "D3D12SDKVersion");
    CHECK(gi != NULL); CHECK(sv != NULL);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)1) == gi);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)2) == sv);
}

TEST(d3d12_entry_points_d3d12core_no_createdevice_export) {
    HMODULE m = t_module("d3d12core.dll");
    if (!m) { t_skip("d3d12core.dll not present"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, "D3D12CreateDevice") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}

TEST(d3d12_entry_points_sdkversion_is_data_export) {
    HMODULE m = t_module("d3d12core.dll");
    if (!m) { t_skip("d3d12core.dll not present"); return; }
    const DWORD *p = (const DWORD *)GetProcAddress(m, "D3D12SDKVersion");
    if (!p || t_is_wine()) { t_skip("no genuine D3D12SDKVersion data export"); return; }
    /* A data export: the address lies inside the image but not in the (executable) code section. */
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)m;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const BYTE *)m + dos->e_lfanew);
    const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    DWORD rva = (DWORD)((const BYTE *)p - (const BYTE *)m);
    CHECK(rva < nt->OptionalHeader.SizeOfImage);
    int found = 0;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + sec[i].Misc.VirtualSize) {
            found = 1; CHECKF(!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE), "section %.8s is executable", sec[i].Name);
        }
    CHECK(found);
    t_note("inbox D3D12SDKVersion = %lu", (unsigned long)*p);
}

TEST(d3d12_entry_points_sdkversion_stable) {
    HMODULE m = t_module("d3d12core.dll");
    if (!m) { t_skip("d3d12core.dll not present"); return; }
    const DWORD *p = (const DWORD *)GetProcAddress(m, "D3D12SDKVersion");
    if (!p) { t_skip("no D3D12SDKVersion"); return; }
    DWORD a = *p, b = *p; CHECK(a == b);
    CHECK(GetProcAddress(m, "D3D12SDKVersion") == (FARPROC)p);
}

TEST(d3d12_entry_points_exe_has_no_agility_exports) {
    /* the test executable does not opt in to an Agility SDK */
    HMODULE self = GetModuleHandleA(NULL);
    SET_ERR(0);
    CHECK(GetProcAddress(self, "D3D12SDKVersion") == NULL);
    CHECK(GetProcAddress(self, "D3D12SDKPath") == NULL);
}

TEST(d3d12_entry_points_dll_exports_no_sdkversion) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    CHECK(GetProcAddress(m, "D3D12SDKVersion") == NULL);
    CHECK(GetProcAddress(m, "D3D12SDKPath") == NULL);
}

/* ---------- D3D12CreateDevice: feature level matrix ---------- */
static const struct { D3D_FEATURE_LEVEL fl; const char *name; } k_fls[] = {
    {D3D_FEATURE_LEVEL_11_0, "11_0"}, {D3D_FEATURE_LEVEL_11_1, "11_1"}, {D3D_FEATURE_LEVEL_12_0, "12_0"},
    {D3D_FEATURE_LEVEL_12_1, "12_1"}, {EP_FL_12_2, "12_2"},
};

/* Returns hr of creating (or validating) a device at a minimum feature level on the WARP adapter. */
static HRESULT create_fl(IDXGIAdapter *ad, D3D_FEATURE_LEVEL fl, const IID *riid, void **out) {
    return d3dh_D3D12CreateDevice()((IUnknown *)ad, fl, riid, out);
}

#define FL_TEST(idx, label) \
TEST(d3d12_entry_points_create_warp_fl_##label) { \
    EP_NEED_WARP_USABLE(ad); \
    ID3D12Device *d = NULL; \
    HRESULT hr = create_fl(ad, k_fls[idx].fl, &IID_ID3D12Device, (void **)&d); \
    if (idx == 0) EXPECT_HR(hr, S_OK); \
    else if (idx < 4) EXPECT_HR(hr, S_OK); \
    else EXPECT_HR(hr, E_INVALIDARG); /* 12_2 (0xC200) is not accepted by the Win10 22H2 runtime */ \
    CHECK(SUCCEEDED(hr) == (d != NULL)); \
    if (d) EP_EQ(ID3D12Device_Release(d), 0); \
    SAFE_RELEASE(ad); \
}
FL_TEST(0, 11_0)
FL_TEST(1, 11_1)
FL_TEST(2, 12_0)
FL_TEST(3, 12_1)
FL_TEST(4, 12_2)

TEST(d3d12_entry_points_create_fl_monotonic) {
    EP_NEED_WARP_USABLE(ad);
    int ok[5];
    for (int i = 0; i < 5; i++) {
        ID3D12Device *d = NULL;
        HRESULT hr = create_fl(ad, k_fls[i].fl, &IID_ID3D12Device, (void **)&d);
        ok[i] = SUCCEEDED(hr); SAFE_RELEASE(d);
    }
    /* if a higher minimum level succeeds every lower one must too */
    for (int i = 1; i < 5; i++) if (ok[i]) CHECKF(ok[i - 1], "FL %s ok but %s not", k_fls[i].name, k_fls[i - 1].name);
    SAFE_RELEASE(ad);
}

#define FL_VALIDATE(idx, label) \
TEST(d3d12_entry_points_create_null_out_fl_##label) { \
    EP_NEED_WARP_USABLE(ad); \
    HRESULT full = create_fl(ad, k_fls[idx].fl, &IID_ID3D12Device, NULL); \
    ID3D12Device *d = NULL; \
    HRESULT real = create_fl(ad, k_fls[idx].fl, &IID_ID3D12Device, (void **)&d); \
    SAFE_RELEASE(d); \
    /* validation-only call: S_FALSE where a real call would succeed, same failure otherwise */ \
    if (SUCCEEDED(real)) EXPECT_HR(full, S_FALSE); else CHECKF(FAILED(full), "validate %s -> %s", k_fls[idx].name, d3dh_hr_name(full)); \
    SAFE_RELEASE(ad); \
}
FL_VALIDATE(0, 11_0)
FL_VALIDATE(1, 11_1)
FL_VALIDATE(2, 12_0)
FL_VALIDATE(3, 12_1)
FL_VALIDATE(4, 12_2)

TEST(d3d12_entry_points_create_default_adapter_null) {
    REQUIRE(d3dh_D3D12CreateDevice());
    ID3D12Device *d = NULL;
    HRESULT hr = d3dh_D3D12CreateDevice()(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d);
    if (FAILED(hr)) { t_skip("no default D3D12 adapter (%s)", d3dh_hr_name(hr)); return; }
    CHECK(d != NULL);
    EP_EQ(d3dh_refcount(d), 1);
    HRESULT v = d3dh_D3D12CreateDevice()(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL);
    EXPECT_HR(v, S_FALSE);
    EP_EQ(ID3D12Device_Release(d), 0);
}

TEST(d3d12_entry_points_create_null_out_does_not_leak) {
    EP_NEED_WARP_USABLE(ad);
    ID3D12Device *d = NULL;
    EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
    ULONG before = d3dh_refcount(d);
    for (int i = 0; i < 5; i++) EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL), S_FALSE);
    EP_EQ(d3dh_refcount(d), before);
    ID3D12Device_Release(d); SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_fresh_refcount_is_one) {
    EP_NEED_WARP_USABLE(ad);
    ID3D12Device *d = NULL;
    EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
    EP_EQ(d3dh_refcount(d), 1);
    EP_EQ(ID3D12Device_AddRef(d), 2);
    EP_EQ(ID3D12Device_Release(d), 1);
    EP_EQ(ID3D12Device_Release(d), 0);
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_does_not_consume_adapter_ref) {
    EP_NEED_WARP_USABLE(ad);
    ULONG before = d3dh_refcount(ad);
    ID3D12Device *d = NULL;
    EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
    ULONG with = d3dh_refcount(ad);
    CHECKF(with >= before, "adapter refcount fell from %lu to %lu", before, with);
    ID3D12Device_Release(d);
    ULONG after = d3dh_refcount(ad);
    CHECKF(after >= 1, "adapter refcount %lu", after);
    if (after != before) t_note("adapter refcount before=%lu after=%lu", before, after);
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_twice_same_adapter) {
    EP_NEED_WARP_USABLE(ad);
    ID3D12Device *a = NULL, *b = NULL;
    EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&a), S_OK);
    EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&b), S_OK);
    CHECK(a && b);
    if (a != b) t_note("two devices on one adapter are distinct objects");
    else { t_note("same adapter yields the same device object"); CHECK(d3dh_refcount(a) >= 2); }
    LUID la = ep_luid(a), lb = ep_luid(b);
    CHECK(la.LowPart == lb.LowPart && la.HighPart == lb.HighPart);
    if (b) ID3D12Device_Release(b);
    if (a) ID3D12Device_Release(a);
    SAFE_RELEASE(ad);
}

/* ---------- invalid feature levels ---------- */
/* 0xB000 and 0xB100 are the valid 11_0 / 11_1 levels; the near misses 0xB001 / 0xB101 / 0xC001 are not. */
static const D3D_FEATURE_LEVEL k_bad_fls[] = { 0, 1, 0x1234, 0x8000, 0xB001, 0xB101, 0xC001, 0xC101, 0xC300, 0xFFFF, 0x7FFFFFFF, (D3D_FEATURE_LEVEL)0xFFFFFFFF };

TEST(d3d12_entry_points_create_invalid_fl_einvalidarg) {
    EP_NEED_WARP_USABLE(ad);
    for (unsigned i = 0; i < sizeof k_bad_fls / sizeof k_bad_fls[0]; i++) {
        ID3D12Device *d = (ID3D12Device *)(ULONG_PTR)0;
        HRESULT hr = create_fl(ad, k_bad_fls[i], &IID_ID3D12Device, (void **)&d);
        CHECKF(hr == E_INVALIDARG, "fl 0x%x -> %s", (unsigned)k_bad_fls[i], d3dh_hr_name(hr));
        CHECK(d == NULL);
    }
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_invalid_fl_null_out) {
    EP_NEED_WARP_USABLE(ad);
    for (unsigned i = 0; i < sizeof k_bad_fls / sizeof k_bad_fls[0]; i++) {
        HRESULT hr = create_fl(ad, k_bad_fls[i], &IID_ID3D12Device, NULL);
        CHECKF(hr == E_INVALIDARG, "validate fl 0x%x -> %s", (unsigned)k_bad_fls[i], d3dh_hr_name(hr));
    }
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_invalid_fl_default_adapter) {
    REQUIRE(d3dh_D3D12CreateDevice());
    ID3D12Device *d = NULL;
    HRESULT hr = d3dh_D3D12CreateDevice()(NULL, (D3D_FEATURE_LEVEL)0, &IID_ID3D12Device, (void **)&d);
    CHECK(FAILED(hr)); CHECK(d == NULL);
    EXPECT_HR(hr, E_INVALIDARG);
}

TEST(d3d12_entry_points_create_legacy_fl_observed) {
    EP_NEED_WARP_USABLE(ad);
    static const D3D_FEATURE_LEVEL legacy[] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1 };
    for (unsigned i = 0; i < 5; i++) {
        ID3D12Device *d = NULL;
        HRESULT hr = create_fl(ad, legacy[i], &IID_ID3D12Device, (void **)&d);
        CHECKF(hr == E_INVALIDARG, "min FL 0x%x -> %s", (unsigned)legacy[i], d3dh_hr_name(hr));   /* 9_x and 10_x are not valid D3D12 minimum levels */
        CHECK(SUCCEEDED(hr) == (d != NULL));
        SAFE_RELEASE(d);
    }
    SAFE_RELEASE(ad);
}

/* ---------- riid matrix ---------- */
#define RIID_TEST(i) \
TEST(d3d12_entry_points_create_riid_device_##i) { \
    EP_NEED_WARP_USABLE(ad); \
    void *p = NULL; \
    HRESULT hr = create_fl(ad, D3D_FEATURE_LEVEL_11_0, EP_DEVICE_IIDS[i], &p); \
    if (SUCCEEDED(hr)) { \
        CHECK(p != NULL); \
        ID3D12Device *base = NULL; \
        EXPECT_HR(((IUnknown *)p)->lpVtbl->QueryInterface((IUnknown *)p, &IID_ID3D12Device, (void **)&base), S_OK); \
        SAFE_RELEASE(base); \
        EP_EQ(((IUnknown *)p)->lpVtbl->Release((IUnknown *)p), 0); \
    } else { \
        t_note("riid ID3D12Device%d -> %s", i, d3dh_hr_name(hr)); CHECK(p == NULL); \
        EXPECT_HR(hr, E_NOINTERFACE); /* Device9/Device10 are not in the Win10 22H2 runtime */ \
    } \
    if (i <= 1) CHECK(SUCCEEDED(hr)); \
    SAFE_RELEASE(ad); \
}
RIID_TEST(0) RIID_TEST(1) RIID_TEST(2) RIID_TEST(3) RIID_TEST(4) RIID_TEST(5)
RIID_TEST(6) RIID_TEST(7) RIID_TEST(8) RIID_TEST(9) RIID_TEST(10)

TEST(d3d12_entry_points_create_riid_validation_only_matches) {
    EP_NEED_WARP_USABLE(ad);
    for (int i = 0; i < 11; i++) {
        void *p = NULL;
        HRESULT real = create_fl(ad, D3D_FEATURE_LEVEL_11_0, EP_DEVICE_IIDS[i], &p);
        if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
        HRESULT v = create_fl(ad, D3D_FEATURE_LEVEL_11_0, EP_DEVICE_IIDS[i], NULL);
        if (SUCCEEDED(real)) CHECKF(v == S_FALSE, "iid %d validate %s", i, d3dh_hr_name(v));
        else t_note("iid %d: real %s validate %s", i, d3dh_hr_name(real), d3dh_hr_name(v));
    }
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_riid_unknown_guid) {
    EP_NEED_WARP_USABLE(ad);
    void *p = (void *)1;
    HRESULT hr = create_fl(ad, D3D_FEATURE_LEVEL_11_0, &EP_RANDOM_GUID, &p);
    CHECK(FAILED(hr));
    EXPECT_HR(hr, E_NOINTERFACE); CHECK(p == NULL);   /* *ppDevice is cleared */
    if (SUCCEEDED(hr) && p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_riid_nondevice_interfaces) {
    EP_NEED_WARP_USABLE(ad);
    static const struct { const GUID *iid; const char *n; } t[] = {
        {&IID_IUnknown, "IUnknown"}, {&IID_ID3D12Object, "ID3D12Object"}, {&IID_ID3D12Resource, "ID3D12Resource"},
        {&IID_ID3D12CommandQueue, "ID3D12CommandQueue"}, {&IID_IDXGIDevice, "IDXGIDevice"}, {&IID_IDXGIFactory, "IDXGIFactory"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        void *p = NULL;
        HRESULT hr = create_fl(ad, D3D_FEATURE_LEVEL_11_0, t[i].iid, &p);
        t_note("riid %s -> %s", t[i].n, d3dh_hr_name(hr));
        if (i >= 2) CHECKF(FAILED(hr), "riid %s must not create a device", t[i].n);
        if (SUCCEEDED(hr) && p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
    SAFE_RELEASE(ad);
}

/* A NULL riid dereferences the pointer inside d3d12: access violation. Observed in a child process because the crash leaves the
 * (process-wide, per-adapter) device object with extra references that would skew every later refcount check. */
CHILD(ep_create_riid_null) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    IDXGIAdapter *ad = ep_warp_adapter(); if (!ad) return 90;
    ID3D12Device *d = NULL;
    d3dh_D3D12CreateDevice()((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, (const IID *)NULL, (void **)&d);
    return 0;   /* not reached */
}
TEST(d3d12_entry_points_create_riid_null) {
    EP_NEED_WARP_USABLE(ad);
    DWORD rc = t_run_child("ep_create_riid_null", "", 30000);
    if (rc == 90) t_skip("child found no WARP adapter"); else EXPECT_EQ(rc, 0xC0000005);
    SAFE_RELEASE(ad);
}

/* ---------- adapter argument ---------- */
TEST(d3d12_entry_points_create_adapter_is_factory_fails) {
    REQUIRE(d3dh_D3D12CreateDevice());
    IDXGIFactory1 *f = NULL;
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no DXGI factory"); return; }
    ID3D12Device *d = NULL;
    HRESULT hr = d3dh_D3D12CreateDevice()((IUnknown *)f, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d);
    CHECKF(FAILED(hr), "factory as adapter -> %s", d3dh_hr_name(hr));
    CHECK(d == NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(d);
    IDXGIFactory1_Release(f);
}

TEST(d3d12_entry_points_create_adapter_is_output_fails) {
    REQUIRE(d3dh_D3D12CreateDevice());
    IDXGIFactory1 *f = NULL;
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no DXGI factory"); return; }
    IDXGIAdapter1 *a = NULL; IDXGIOutput *o = NULL;
    if (FAILED(IDXGIFactory1_EnumAdapters1(f, 0, &a)) || FAILED(IDXGIAdapter1_EnumOutputs(a, 0, &o))) {
        SAFE_RELEASE(a); IDXGIFactory1_Release(f); t_skip("no output to use"); return;
    }
    ID3D12Device *d = NULL;
    HRESULT hr = d3dh_D3D12CreateDevice()((IUnknown *)o, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d);
    CHECK(FAILED(hr)); CHECK(d == NULL);
    SAFE_RELEASE(d); IDXGIOutput_Release(o); IDXGIAdapter1_Release(a); IDXGIFactory1_Release(f);
}

TEST(d3d12_entry_points_create_adapter_interface_variants) {
    EP_NEED_WARP_USABLE(ad);
    IDXGIAdapter1 *a1 = NULL; IDXGIAdapter2 *a2 = NULL; IDXGIAdapter3 *a3 = NULL;
    if (SUCCEEDED(IDXGIAdapter_QueryInterface(ad, &IID_IDXGIAdapter1, (void **)&a1))) {
        ID3D12Device *d = NULL;
        EXPECT_HR(d3dh_D3D12CreateDevice()((IUnknown *)a1, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
        SAFE_RELEASE(d); IDXGIAdapter1_Release(a1);
    }
    if (SUCCEEDED(IDXGIAdapter_QueryInterface(ad, &IID_IDXGIAdapter2, (void **)&a2))) {
        ID3D12Device *d = NULL;
        EXPECT_HR(d3dh_D3D12CreateDevice()((IUnknown *)a2, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
        SAFE_RELEASE(d); IDXGIAdapter2_Release(a2);
    }
    if (SUCCEEDED(IDXGIAdapter_QueryInterface(ad, &IID_IDXGIAdapter3, (void **)&a3))) {
        ID3D12Device *d = NULL;
        EXPECT_HR(d3dh_D3D12CreateDevice()((IUnknown *)a3, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
        SAFE_RELEASE(d); IDXGIAdapter3_Release(a3);
    }
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_all_adapters_validate) {
    REQUIRE(d3dh_D3D12CreateDevice());
    IDXGIFactory1 *f = NULL;
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no DXGI factory"); return; }
    IDXGIAdapter1 *a = NULL;
    int n = 0;
    for (UINT i = 0; i < 16 && IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK; i++) {
        DXGI_ADAPTER_DESC1 desc; IDXGIAdapter1_GetDesc1(a, &desc);
        HRESULT hr = d3dh_D3D12CreateDevice()((IUnknown *)a, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL);
        EXPECT_HR_IN(hr, S_FALSE, HR_UNSUPPORTED);
        t_note("adapter %u flags=0x%x vendor=0x%x -> %s", i, desc.Flags, desc.VendorId, d3dh_hr_name(hr));
        EXPECT_HR(hr, S_FALSE);
        IDXGIAdapter1_Release(a); n++;
    }
    CHECK(n >= 0);
    IDXGIFactory1_Release(f);
}

TEST(d3d12_entry_points_create_warp_adapter_luid_matches) {
    EP_NEED_WARP_USABLE(ad);
    DXGI_ADAPTER_DESC desc; EXPECT_HR(IDXGIAdapter_GetDesc(ad, &desc), S_OK);
    ID3D12Device *d = NULL;
    EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
    LUID l = ep_luid(d);
    EXPECT_EQ(l.LowPart, desc.AdapterLuid.LowPart); EXPECT_EQ(l.HighPart, desc.AdapterLuid.HighPart);
    ID3D12Device_Release(d); SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_create_by_luid_adapter) {
    REQUIRE(d3dh_D3D12CreateDevice());
    EP_NEED_WARP_USABLE(wad);
    DXGI_ADAPTER_DESC desc; IDXGIAdapter_GetDesc(wad, &desc);
    IDXGIFactory4 *f = NULL;
    if (FAILED(d3dh_CreateDXGIFactory1()(&IID_IDXGIFactory4, (void **)&f))) { SAFE_RELEASE(wad); t_skip("no IDXGIFactory4"); return; }
    IDXGIAdapter *ad = NULL;
    EXPECT_HR(IDXGIFactory4_EnumAdapterByLuid(f, desc.AdapterLuid, &IID_IDXGIAdapter, (void **)&ad), S_OK);
    if (ad) {
        ID3D12Device *d = NULL;
        EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
        if (d) { LUID l = ep_luid(d); EP_EQ(l.LowPart, desc.AdapterLuid.LowPart); ID3D12Device_Release(d); }
        IDXGIAdapter_Release(ad);
    }
    IDXGIFactory4_Release(f); SAFE_RELEASE(wad);
}

TEST(d3d12_entry_points_create_warp_adapter_stable_luid) {
    EP_NEED_WARP(a1);
    IDXGIAdapter *a2 = ep_warp_adapter();
    CHECK(a2 != NULL);
    if (a2) {
        DXGI_ADAPTER_DESC d1, d2; IDXGIAdapter_GetDesc(a1, &d1); IDXGIAdapter_GetDesc(a2, &d2);
        CHECK(d1.AdapterLuid.LowPart == d2.AdapterLuid.LowPart && d1.AdapterLuid.HighPart == d2.AdapterLuid.HighPart);
        IDXGIAdapter_Release(a2);
    }
    IDXGIAdapter_Release(a1);
}

/* ---------- loader robustness ---------- */
TEST(d3d12_entry_points_create_via_ordinal) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    PFN_D3D12_CREATE_DEVICE f = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(m, (LPCSTR)101);
    CHECK(f != NULL);
    if (!f) return;
    IDXGIAdapter *ad = ep_warp_adapter();
    if (!ad) { t_skip("no WARP"); return; }
    if (!ep_warp_usable(ad)) { IDXGIAdapter_Release(ad); t_skip("WARP cannot create D3D12 device"); return; }
    HRESULT hr = f((IUnknown *)ad, (D3D_FEATURE_LEVEL)0, &IID_ID3D12Device, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    IDXGIAdapter_Release(ad);
}

TEST(d3d12_entry_points_create_repeated) {
    /* repeated creation while a device is held stays consistent */
    EP_NEED_WARP_USABLE(ad);
    ID3D12Device *held = NULL;
    EXPECT_HR(create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&held), S_OK);
    for (int i = 0; i < 20; i++) {
        ID3D12Device *d = NULL;
        HRESULT hr = create_fl(ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d);
        EXPECT_HR(hr, S_OK); SAFE_RELEASE(d);
    }
    ID3D12Device_Release(held); SAFE_RELEASE(ad);
}

/* The device is a process-wide object per adapter: a second D3D12CreateDevice on the same adapter returns the same pointer with an extra reference,
 * and the object dies with the last Release. Absolute counts are only meaningful in a fresh process, hence the child. */
CHILD(ep_device_singleton) {
    IDXGIAdapter *ad = ep_warp_adapter(); if (!ad) return 90;
    __typeof__(&D3D12CreateDevice) cd = d3dh_D3D12CreateDevice();
    ID3D12Device *a = NULL, *b = NULL, *c = NULL;
    if (FAILED(cd((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&a)) || !a) return 91;
    if (d3dh_refcount(a) != 1) return 92;
    if (FAILED(cd((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&b)) || b != a) return 93;
    if (d3dh_refcount(a) != 2) return 94;
    /* the default adapter (enumeration index 0) is a different adapter than the EnumWarpAdapter one on this system: its device is a separate object */
    if (FAILED(cd(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&c)) || !c) return 95;
    if (c == a || d3dh_refcount(a) != 2 || d3dh_refcount(c) != 1) return 96;
    ID3D12Device1 *d1 = NULL;
    if (FAILED(ID3D12Device_QueryInterface(a, &IID_ID3D12Device1, (void **)&d1)) || (void *)d1 != (void *)a) return 97;
    if (d3dh_refcount(a) != 3) return 98;
    ID3D12Device1_Release(d1);
    if (ID3D12Device_Release(c) != 0) return 99;
    if (ID3D12Device_Release(b) != 1) return 100;
    if (ID3D12Device_Release(a) != 0) return 101;
    return 0;
}
TEST(d3d12_entry_points_device_is_per_adapter_singleton) {
    EP_NEED_WARP_USABLE(ad);
    DWORD rc = t_run_child("ep_device_singleton", "", 60000);
    if (rc == 90) t_skip("child found no WARP adapter"); else CHECKF(rc == 0, "child step %lu failed (91 create, 92 fresh count, 93/94 second create, 95/96 default adapter, 97/98 QI, 99-101 releases)", rc);
    SAFE_RELEASE(ad);
}
