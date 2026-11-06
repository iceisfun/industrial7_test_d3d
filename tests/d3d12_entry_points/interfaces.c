#include "ep.h"

/* ---------- D3D12GetDebugInterface ---------- */
static const struct { const GUID *iid; const char *n; } k_dbg[] = {
    {&IID_ID3D12Debug, "ID3D12Debug"}, {&IID_ID3D12Debug1, "ID3D12Debug1"}, {&IID_ID3D12Debug2, "ID3D12Debug2"},
    {&IID_ID3D12Debug3, "ID3D12Debug3"}, {&IID_ID3D12Debug4, "ID3D12Debug4"}, {&IID_ID3D12Debug5, "ID3D12Debug5"},
    {&EP_IID_Debug6, "ID3D12Debug6"},
};

#define DBG_TEST(i) \
TEST(d3d12_entry_points_debug_interface_##i) { \
    REQUIRE(U(D3D12GetDebugInterface)); \
    void *p = (void *)(ULONG_PTR)1; \
    HRESULT hr = U(D3D12GetDebugInterface)(k_dbg[i].iid, &p); \
    if (SUCCEEDED(hr)) { \
        CHECK(p != NULL && p != (void *)1); \
        if (p && p != (void *)1) { \
            IUnknown *u = (IUnknown *)p; \
            EP_EQ(u->lpVtbl->AddRef(u), 2); \
            EP_EQ(u->lpVtbl->Release(u), 1); \
            EP_EQ(u->lpVtbl->Release(u), 0); \
        } \
    } else { \
        if (i == 0) EXPECT_HR_IN(hr, HR_SDKMISSING, HR_NOINTERFACE); \
        /* Win10 22H2 (with the Graphics Tools feature): ID3D12Debug .. Debug3 exist, Debug4/5/6 are not in the inbox layer: E_NOINTERFACE */ \
        else EXPECT_HR_IN(hr, E_NOINTERFACE, HR_SDKMISSING); /* HR_SDKMISSING when the Graphics Tools feature is not installed */ \
        t_note("%s -> %s (debug layer missing or interface too new)", k_dbg[i].n, d3dh_hr_name(hr)); \
    } \
}
DBG_TEST(0) DBG_TEST(1) DBG_TEST(2) DBG_TEST(3) DBG_TEST(4) DBG_TEST(5) DBG_TEST(6)

TEST(d3d12_entry_points_debug_interface_ladder) {
    REQUIRE(U(D3D12GetDebugInterface));
    void *base = NULL;
    HRESULT hr = U(D3D12GetDebugInterface)(&IID_ID3D12Debug, &base);
    if (FAILED(hr)) { EXPECT_HR_IN(hr, HR_SDKMISSING, HR_NOINTERFACE); t_skip("debug layer not installed (%s)", d3dh_hr_name(hr)); return; }
    IUnknown *u = (IUnknown *)base;
    IUnknown *ident = NULL;
    EXPECT_HR(u->lpVtbl->QueryInterface(u, &IID_IUnknown, (void **)&ident), S_OK);
    int missing_seen = 0;
    for (unsigned i = 1; i < sizeof k_dbg / sizeof k_dbg[0]; i++) {
        void *q = NULL;
        HRESULT h = u->lpVtbl->QueryInterface(u, k_dbg[i].iid, &q);
        if (SUCCEEDED(h)) { CHECKF(!missing_seen, "%s present after a lower interface was absent", k_dbg[i].n); ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q); }
        else { EXPECT_HR(h, E_NOINTERFACE); missing_seen = 1; CHECK(q == NULL); }
    }
    CHECK(ident != NULL);
    if (ident) ident->lpVtbl->Release(ident);
    EP_EQ(u->lpVtbl->Release(u), 0);
}

TEST(d3d12_entry_points_debug_interface_unrelated_iid) {
    REQUIRE(U(D3D12GetDebugInterface));
    void *p = NULL;
    HRESULT hr = U(D3D12GetDebugInterface)(&EP_RANDOM_GUID, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    if (SUCCEEDED(hr) && p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
}

TEST(d3d12_entry_points_debug_interface_not_a_device) {
    REQUIRE(U(D3D12GetDebugInterface));
    void *p = NULL;
    HRESULT hr = U(D3D12GetDebugInterface)(&IID_ID3D12Device, &p);
    EXPECT_HR(hr, E_NOINTERFACE); CHECK(p == NULL);
}

TEST(d3d12_entry_points_debug_interface_repeatable) {
    REQUIRE(U(D3D12GetDebugInterface));
    void *a = NULL, *b = NULL;
    HRESULT h1 = U(D3D12GetDebugInterface)(&IID_ID3D12Debug, &a);
    HRESULT h2 = U(D3D12GetDebugInterface)(&IID_ID3D12Debug, &b);
    CHECK(h1 == h2);
    if (SUCCEEDED(h1)) { CHECK(a && b); t_note("debug interface objects %s", a == b ? "shared" : "distinct"); }
    if (a) ((IUnknown *)a)->lpVtbl->Release((IUnknown *)a);
    if (b) ((IUnknown *)b)->lpVtbl->Release((IUnknown *)b);
}

TEST(d3d12_entry_points_debug_interface_via_ordinal) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    typeof(&D3D12GetDebugInterface) f = (typeof(&D3D12GetDebugInterface))GetProcAddress(m, (LPCSTR)102);
    REQUIRE(f);
    void *p = NULL;
    HRESULT hr = f(&IID_ID3D12Debug, &p);
    CHECK(hr == S_OK || hr == HR_SDKMISSING || hr == E_NOINTERFACE);
    if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
}

/* ---------- D3D12GetInterface ---------- */
static int have_core(void) { return t_module("d3d12core.dll") != NULL; }

TEST(d3d12_entry_points_getinterface_sdkconfiguration) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    ID3D12SDKConfiguration *c = NULL;
    HRESULT hr = U(D3D12GetInterface)(&CLSID_D3D12SDKConfiguration, &IID_ID3D12SDKConfiguration, (void **)&c);
    /* Win10 22H2: the SDK configuration object is part of the Agility SDK runtime only, the inbox D3D12GetInterface answers E_NOINTERFACE */
    if (FAILED(hr)) { EXPECT_HR(hr, E_NOINTERFACE); CHECK(c == NULL); return; }
    EXPECT_HR(hr, S_OK);
    CHECK(c != NULL);
    EP_EQ(d3dh_refcount(c), 1);
    IUnknown *u = NULL;
    EXPECT_HR(ID3D12SDKConfiguration_QueryInterface(c, &IID_IUnknown, (void **)&u), S_OK);
    CHECK((void *)u == (void *)c);
    SAFE_RELEASE(u);
    EP_EQ(ID3D12SDKConfiguration_Release(c), 0);
}

TEST(d3d12_entry_points_getinterface_sdkconfiguration_null_out) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    ID3D12SDKConfiguration *c = NULL;
    HRESULT probe = U(D3D12GetInterface)(&CLSID_D3D12SDKConfiguration, &IID_ID3D12SDKConfiguration, (void **)&c);
    if (FAILED(probe)) { EXPECT_HR(probe, E_NOINTERFACE); HRESULT hn = U(D3D12GetInterface)(&CLSID_D3D12SDKConfiguration, &IID_ID3D12SDKConfiguration, NULL); EXPECT_HR(hn, E_NOINTERFACE); return; }   /* inbox runtime: NULL out does not change the answer */
    SAFE_RELEASE(c);
    HRESULT hr = U(D3D12GetInterface)(&CLSID_D3D12SDKConfiguration, &IID_ID3D12SDKConfiguration, NULL);
    EXPECT_HR(hr, S_FALSE);
}

TEST(d3d12_entry_points_getinterface_unknown_clsid) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    void *p = NULL;
    HRESULT hr = U(D3D12GetInterface)(&EP_RANDOM_GUID, &IID_IUnknown, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
}

TEST(d3d12_entry_points_getinterface_unknown_clsid_null_out) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    EXPECT_HR(U(D3D12GetInterface)(&EP_RANDOM_GUID, &IID_IUnknown, NULL), E_NOINTERFACE);
}

TEST(d3d12_entry_points_getinterface_iid_as_clsid) {
    /* an interface id is not a class id */
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    void *p = NULL;
    EXPECT_HR(U(D3D12GetInterface)(&IID_ID3D12SDKConfiguration, &IID_ID3D12SDKConfiguration, &p), E_NOINTERFACE);
    EXPECT_HR(U(D3D12GetInterface)(&IID_ID3D12Device, &IID_ID3D12Device, &p), E_NOINTERFACE);
    CHECK(p == NULL);
}

TEST(d3d12_entry_points_getinterface_null_clsid_guid) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    void *p = NULL;
    EXPECT_HR(U(D3D12GetInterface)(&GUID_NULL, &IID_IUnknown, &p), E_NOINTERFACE);
}

TEST(d3d12_entry_points_getinterface_tools) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    if (!have_core()) { t_skip("d3d12core.dll absent"); return; }
    ID3D12Tools *t = NULL;
    HRESULT hr = U(D3D12GetInterface)(&CLSID_D3D12Tools, &IID_ID3D12Tools, (void **)&t);
    if (FAILED(hr)) { EXPECT_HR(hr, E_NOINTERFACE); CHECK(t == NULL); return; }   /* Win10 22H2: the Tools interface is Agility-SDK only */
    EXPECT_HR(hr, S_OK); CHECK(t != NULL);
    EP_EQ(d3dh_refcount(t), 1);
    IUnknown *u = NULL;
    EXPECT_HR(ID3D12Tools_QueryInterface(t, &IID_IUnknown, (void **)&u), S_OK);
    CHECK((void *)u == (void *)t);
    SAFE_RELEASE(u);
    EP_EQ(ID3D12Tools_Release(t), 0);
}

TEST(d3d12_entry_points_getinterface_tools_null_out) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    if (!have_core()) { t_skip("d3d12core.dll absent"); return; }
    HRESULT hr = U(D3D12GetInterface)(&CLSID_D3D12Tools, &IID_ID3D12Tools, NULL);
    /* Win10 22H2: the Tools interface is not provided by the inbox runtime at all (see getinterface_tools), so NULL out gives the same E_NOINTERFACE */
    t_note("Tools with NULL out -> %s", d3dh_hr_name(hr));
    EXPECT_HR_IN(hr, S_FALSE, E_INVALIDARG, E_POINTER, E_NOINTERFACE);
}

TEST(d3d12_entry_points_getinterface_dred_settings) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    if (!have_core()) { t_skip("d3d12core.dll absent"); return; }
    ID3D12DeviceRemovedExtendedDataSettings *s = NULL;
    HRESULT hr = U(D3D12GetInterface)(&CLSID_D3D12DeviceRemovedExtendedData, &IID_ID3D12DeviceRemovedExtendedDataSettings, (void **)&s);
    if (FAILED(hr)) { EXPECT_HR(hr, E_NOINTERFACE); return; }
    EXPECT_HR(hr, S_OK);
    CHECK(s != NULL);
    EP_EQ(d3dh_refcount(s), 1);
    ID3D12DeviceRemovedExtendedDataSettings1 *s1 = NULL;
    HRESULT h1 = ID3D12DeviceRemovedExtendedDataSettings_QueryInterface(s, &IID_ID3D12DeviceRemovedExtendedDataSettings1, (void **)&s1);
    EXPECT_HR(h1, S_OK);   /* Win10 22H2 provides the Settings1 extension */
    SAFE_RELEASE(s1);
    EP_EQ(ID3D12DeviceRemovedExtendedDataSettings_Release(s), 0);
}

TEST(d3d12_entry_points_getinterface_device_factory_observed) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    void *p = NULL;
    HRESULT hr = U(D3D12GetInterface)(&EP_CLSID_DeviceFactory, &EP_IID_DeviceFactory, &p);
    EXPECT_HR(hr, E_NOINTERFACE);   /* the device factory is Agility-SDK only (Win10 22H2) */
    if (SUCCEEDED(hr) && p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    CHECK(SUCCEEDED(hr) == (p != NULL));
}

TEST(d3d12_entry_points_getinterface_via_ordinal) {
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    typeof(&D3D12GetInterface) f = (typeof(&D3D12GetInterface))GetProcAddress(m, (LPCSTR)111);
    if (!f) { t_skip("ordinal 111 absent"); return; }
    CHECK(f == (typeof(&D3D12GetInterface))GetProcAddress(m, "D3D12GetInterface"));
    void *p = NULL;
    EXPECT_HR(f(&EP_RANDOM_GUID, &IID_IUnknown, &p), E_NOINTERFACE);
}

TEST(d3d12_entry_points_getinterface_core_direct) {
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    HMODULE c = t_module("d3d12core.dll"); if (!c) { t_skip("no d3d12core.dll"); return; }
    typeof(&D3D12GetInterface) f = (typeof(&D3D12GetInterface))GetProcAddress(c, "D3D12GetInterface");
    REQUIRE(f);
    void *p = NULL;
    EXPECT_HR(f(&EP_RANDOM_GUID, &IID_IUnknown, &p), E_NOINTERFACE);
    ID3D12Tools *t = NULL;
    HRESULT hr = f(&CLSID_D3D12Tools, &IID_ID3D12Tools, (void **)&t);
    if (SUCCEEDED(hr)) { EXPECT_HR(hr, S_OK); EP_EQ(ID3D12Tools_Release(t), 0); }
    else EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(f(&CLSID_D3D12Tools, &IID_ID3D12Tools, NULL) == S_FALSE || FAILED(f(&CLSID_D3D12Tools, &IID_ID3D12Tools, NULL)));
}

/* ---------- ID3D12SDKConfiguration::SetSDKVersion (isolated: it changes process-wide loader state) ---------- */
CHILD(sdkcfg_bad_path) {
    if (!U(D3D12GetInterface)) return 100;
    ID3D12SDKConfiguration *c = NULL;
    HRESULT hr = U(D3D12GetInterface)(&CLSID_D3D12SDKConfiguration, &IID_ID3D12SDKConfiguration, (void **)&c);
    if (FAILED(hr) || !c) return 101;
    hr = ID3D12SDKConfiguration_SetSDKVersion(c, 9999, ".\\d3dt_no_such_dir\\");
    printf("sdkconfig bad path hr=0x%08lx\n", (unsigned long)hr);
    ID3D12SDKConfiguration_Release(c);
    return hr == S_OK ? 0 : (hr == (HRESULT)0x887A0004 ? 1 : (hr == E_INVALIDARG ? 2 : (SUCCEEDED(hr) ? 3 : 4)));
}
TEST(d3d12_entry_points_sdkconfig_setsdkversion_bad_path) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    DWORD rc = t_run_child("sdkcfg_bad_path", "", 20000);
    /* Win10 22H2: no SDK configuration object -> the child reports 100 (D3D12GetInterface failed) */
    if (rc == 100 || rc == 101) return;   /* child could not obtain the SDK configuration object (inbox runtime) */
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    t_note("SetSDKVersion(9999, nonexistent dir) class=%lu (0 ok,1 unsupported,2 invalidarg,3 other success,4 other failure)", rc);
}

CHILD(sdkcfg_refcount) {
    ID3D12SDKConfiguration *c = NULL;
    if (!U(D3D12GetInterface) || FAILED(U(D3D12GetInterface)(&CLSID_D3D12SDKConfiguration, &IID_ID3D12SDKConfiguration, (void **)&c)) || !c) return 100;
    ULONG a = ID3D12SDKConfiguration_AddRef(c);
    ULONG b = ID3D12SDKConfiguration_Release(c);
    ULONG r = ID3D12SDKConfiguration_Release(c);
    return (a == 2 && b == 1 && r == 0) ? 0 : 1;
}
TEST(d3d12_entry_points_sdkconfig_refcounts) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    DWORD rc = t_run_child("sdkcfg_refcount", "", 20000);
    if (rc == 100) return;   /* inbox runtime: no SDK configuration object (same E_NOINTERFACE as getinterface_sdkconfiguration) */
    EXPECT_EQ(rc, 0);
}

/* ---------- D3D12EnableExperimentalFeatures (each call runs in its own process) ---------- */
CHILD(expfeat) {
    if (argc < 2 || !U(D3D12EnableExperimentalFeatures)) return 0x7000;
    int mode = atoi(argv[1]);
    HRESULT hr;
    UINT sz4 = 4; UINT zero = 0; int cfg = 0;
    GUID two[2] = { EP_EXP_ShaderModels, EP_RANDOM_GUID };
    UINT szs2[2] = { 0, 0 };
    switch (mode) {
    case 0: hr = U(D3D12EnableExperimentalFeatures)(1, &EP_RANDOM_GUID, NULL, NULL); break;
    case 1: hr = U(D3D12EnableExperimentalFeatures)(1, &EP_EXP_ShaderModels, NULL, NULL); break;
    case 2: hr = U(D3D12EnableExperimentalFeatures)(1, &EP_EXP_TiledTier4, NULL, NULL); break;
    case 3: hr = U(D3D12EnableExperimentalFeatures)(1, &EP_EXP_MetaCommand, NULL, NULL); break;
    case 4: hr = U(D3D12EnableExperimentalFeatures)(0, NULL, NULL, NULL); break;
    case 5: hr = U(D3D12EnableExperimentalFeatures)(2, two, NULL, szs2); break;
    case 6: hr = U(D3D12EnableExperimentalFeatures)(1, &EP_EXP_ShaderModels, &cfg, &sz4); break;
    case 7: hr = U(D3D12EnableExperimentalFeatures)(1, &EP_EXP_ShaderModels, NULL, &zero); break;
    case 8: hr = U(D3D12EnableExperimentalFeatures)(1, &GUID_NULL, NULL, NULL); break;
    case 9: hr = U(D3D12EnableExperimentalFeatures)(1, &EP_RANDOM_GUID, &cfg, &sz4); break;
    default: return 0x7001;
    }
    printf("expfeat mode %d hr=0x%08lx\n", mode, (unsigned long)hr);
    return (int)hr;
}
static HRESULT run_exp(int mode, int *ran) {
    char arg[16]; snprintf(arg, sizeof arg, "%d", mode);
    DWORD rc = t_run_child("expfeat", arg, 20000);
    *ran = !(rc == 0xDEAD0000 || rc == 0xDEAD0001 || rc == 0x7000 || rc == 0x7001);
    return (HRESULT)rc;
}

#define EXP_TEST(name, mode, ...) \
TEST(d3d12_entry_points_experimental_##name) { \
    REQUIRE(U(D3D12EnableExperimentalFeatures)); \
    int ran; HRESULT hr = run_exp(mode, &ran); \
    if (!ran) { t_skip("child did not run (0x%lx)", (unsigned long)hr); return; } \
    t_note("mode %d -> %s", mode, d3dh_hr_name(hr)); \
    /* Win10 22H2 with developer mode off: every experimental feature request fails with E_NOINTERFACE */ \
    EXPECT_HR(hr, E_NOINTERFACE); \
}
EXP_TEST(unknown_guid, 0, E_NOINTERFACE, HR_UNSUPPORTED)
EXP_TEST(shader_models, 1, S_OK, HR_UNSUPPORTED, E_NOINTERFACE)
EXP_TEST(tiled_tier4, 2, S_OK, HR_UNSUPPORTED, E_NOINTERFACE)
EXP_TEST(meta_command, 3, S_OK, HR_UNSUPPORTED, E_NOINTERFACE)
EXP_TEST(null_guid, 8, E_NOINTERFACE, HR_UNSUPPORTED)
EXP_TEST(unknown_guid_with_config, 9, E_NOINTERFACE, HR_UNSUPPORTED, E_INVALIDARG)

TEST(d3d12_entry_points_experimental_zero_features) {
    REQUIRE(U(D3D12EnableExperimentalFeatures));
    int ran; HRESULT hr = run_exp(4, &ran);
    if (!ran) { t_skip("child did not run"); return; }
    EXPECT_HR(hr, S_OK);   /* nothing to enable */
}

TEST(d3d12_entry_points_experimental_mixed_list_fails_if_any_unknown) {
    REQUIRE(U(D3D12EnableExperimentalFeatures));
    int ran; HRESULT hr = run_exp(5, &ran);
    if (!ran) { t_skip("child did not run"); return; }
    EXPECT_HR(hr, E_NOINTERFACE);
}

TEST(d3d12_entry_points_experimental_config_struct_for_shader_models) {
    REQUIRE(U(D3D12EnableExperimentalFeatures));
    int ran; HRESULT hr = run_exp(6, &ran);
    if (!ran) { t_skip("child did not run"); return; }
    EXPECT_HR(hr, E_NOINTERFACE);
}

TEST(d3d12_entry_points_experimental_zero_size_array) {
    REQUIRE(U(D3D12EnableExperimentalFeatures));
    int ran; HRESULT hr = run_exp(7, &ran);
    if (!ran) { t_skip("child did not run"); return; }
    EXPECT_HR(hr, E_NOINTERFACE);
}

TEST(d3d12_entry_points_experimental_deterministic) {
    REQUIRE(U(D3D12EnableExperimentalFeatures));
    int r1, r2; HRESULT a = run_exp(1, &r1), b = run_exp(1, &r2);
    if (!r1 || !r2) { t_skip("child did not run"); return; }
    CHECK(a == b);
}

TEST(d3d12_entry_points_experimental_known_vs_unknown_ordering) {
    REQUIRE(U(D3D12EnableExperimentalFeatures));
    int r1, r2; HRESULT known = run_exp(1, &r1), unk = run_exp(0, &r2);
    if (!r1 || !r2) { t_skip("child did not run"); return; }
    if (known == S_OK) CHECK(FAILED(unk));
    if (known == HR_UNSUPPORTED) t_note("developer mode off: unknown guid -> %s", d3dh_hr_name(unk));
}

UDECL(int, GetBehaviorValue, (const char *, UINT64 *));

/* ---------- undocumented core pass-throughs, run isolated ---------- */
CHILD(getbehavior) {
    if (!UN(GetBehaviorValue)) return 100;
    UINT64 v1 = 0x1122334455667788ull, v2 = 0x1122334455667788ull;
    int r1 = UN(GetBehaviorValue)("D3dtNoSuchBehaviorName", &v1);
    int r2 = UN(GetBehaviorValue)("D3dtNoSuchBehaviorName", &v2);
    printf("GetBehaviorValue unknown -> %d value=0x%llx\n", r1, (unsigned long long)v1);
    return (r1 == r2 && v1 == v2) ? (r1 & 0xff) : 99;
}
TEST(d3d12_entry_points_getbehaviorvalue_unknown_name) {
    REQUIRE(UN(GetBehaviorValue));
    if (t_is_wine()) { t_skip("emulator stubs GetBehaviorValue"); return; }
    DWORD rc = t_run_child("getbehavior", "", 20000);
    if (rc == 100) { t_skip("absent"); return; }
    CHECK(rc != 99 && rc != 0xDEAD0000 && rc != 0xDEAD0001);
    t_note("GetBehaviorValue(unknown) status byte %lu", rc);
}

TEST(d3d12_entry_points_core_layer_exports_present) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    if (t_is_wine()) { t_skip("not exported by the emulator"); return; }
    CHECK(GetProcAddress(m, "D3D12CoreCreateLayeredDevice") != NULL);
    CHECK(GetProcAddress(m, "D3D12CoreGetLayeredDeviceSize") != NULL);
    CHECK(GetProcAddress(m, "D3D12CoreRegisterLayers") != NULL);
    CHECK(GetProcAddress(m, "SetAppCompatStringPointer") != NULL);
}

TEST(d3d12_entry_points_pix_exports_present) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    if (t_is_wine()) { t_skip("not exported by the emulator"); return; }
    CHECK(GetProcAddress(m, "D3D12PIXEventsReplaceBlock") != NULL);
    CHECK(GetProcAddress(m, "D3D12PIXGetThreadInfo") != NULL);
    CHECK(GetProcAddress(m, "D3D12PIXNotifyWakeFromFenceSignal") != NULL);
    CHECK(GetProcAddress(m, "D3D12PIXReportCounter") != NULL);
    CHECK(GetProcAddress(m, "D3D12DeviceRemovedExtendedData") != NULL);
}

TEST(d3d12_entry_points_dred_export_is_data) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    const void *p = (const void *)GetProcAddress(m, "D3D12DeviceRemovedExtendedData");
    if (!p) { t_skip("absent"); return; }
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)m;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const BYTE *)m + dos->e_lfanew);
    const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    DWORD rva = (DWORD)((const BYTE *)p - (const BYTE *)m);
    int found = 0;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + sec[i].Misc.VirtualSize) {
            found = 1;
            CHECKF(!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE), "section %.8s", sec[i].Name);
            t_note("D3D12DeviceRemovedExtendedData lives in %.8s", sec[i].Name);
        }
    CHECK(found);
}

/* ---------- the undocumented layered-device entry points (one call per child: the arguments are not validated by every path) ---------- */
typedef SIZE_T (WINAPI *layer_size_fn)(const void *, DWORD);
typedef HRESULT (WINAPI *layer_reg_fn)(const void *, DWORD);
typedef HRESULT (WINAPI *layer_create_fn)(const void *, DWORD, const void *, REFIID, void **);
CHILD(ep_layer) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    HMODULE m = t_module("d3d12.dll"); if (!m || argc < 2) return 90;
    int k = atoi(argv[argc - 1]);
    switch (k) {
    case 0: { layer_size_fn f = (layer_size_fn)GetProcAddress(m, "D3D12CoreGetLayeredDeviceSize"); return f && f(NULL, 0) == (SIZE_T)~0ull ? 0 : 1; }
    case 1: { layer_size_fn f = (layer_size_fn)GetProcAddress(m, "D3D12CoreGetLayeredDeviceSize"); return f && f(NULL, 1) == (SIZE_T)~0ull ? 0 : 1; }
    case 2: { layer_reg_fn f = (layer_reg_fn)GetProcAddress(m, "D3D12CoreRegisterLayers"); return f && f(NULL, 0) == S_OK ? 0 : 1; }
    case 3: { layer_reg_fn f = (layer_reg_fn)GetProcAddress(m, "D3D12CoreRegisterLayers"); return f && f(NULL, 1) == E_INVALIDARG ? 0 : 1; }
    case 4: { layer_create_fn f = (layer_create_fn)GetProcAddress(m, "D3D12CoreCreateLayeredDevice"); void *p = (void *)1;
              return f && f(NULL, 0, NULL, &IID_ID3D12Device, &p) == E_INVALIDARG && p == NULL ? 0 : 1; }
    case 5: { layer_create_fn f = (layer_create_fn)GetProcAddress(m, "D3D12CoreCreateLayeredDevice");
              return f && f(NULL, 0, NULL, &IID_ID3D12Device, NULL) == E_UNEXPECTED ? 0 : 1; }
    }
    return 91;
}
TEST(d3d12_entry_points_core_layer_exports_behaviour) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    if (t_is_wine()) { t_skip("not exported by the emulator"); return; }
    /* Measured on Win10 22H2 with all-NULL arguments: GetLayeredDeviceSize -> (SIZE_T)-1 for 0 and 1 layers; RegisterLayers(NULL,0) -> S_OK,
     * RegisterLayers(NULL,1) -> E_INVALIDARG; CreateLayeredDevice(NULL,0,NULL,IID_ID3D12Device,&p) -> E_INVALIDARG with *p cleared,
     * with a NULL output pointer -> E_UNEXPECTED. */
    static const char *const nm[] = { "GetLayeredDeviceSize(NULL,0)", "GetLayeredDeviceSize(NULL,1)", "RegisterLayers(NULL,0)", "RegisterLayers(NULL,1)", "CreateLayeredDevice(NULL args)", "CreateLayeredDevice(NULL ppv)" };
    for (int k = 0; k < 6; k++) {
        char a[8]; wsprintfA(a, "%d", k);
        DWORD rc = t_run_child("ep_layer", a, 30000);
        CHECKF(rc == 0, "%s: child exit 0x%08lx", nm[k], rc);
    }
}
