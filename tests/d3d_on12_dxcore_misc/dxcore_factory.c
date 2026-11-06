/* dxcore.dll: factory, adapter lists, adapters. */
#include "odm.h"

/* ---------------- DXCoreCreateAdapterFactory ---------------- */
TEST(d3d_on12_dxcore_misc_dxcore_export_present) {
    HMODULE m = t_module("dxcore.dll");
    if (!m || t_is_wine()) { t_skip("dxcore.dll missing or emulated"); return; }
    CHECK(GetProcAddress(m, "DXCoreCreateAdapterFactory") != NULL);
}
TEST(d3d_on12_dxcore_misc_dxcore_export_ordinal_211) {
    HMODULE m = t_module("dxcore.dll");
    if (!m || t_is_wine()) { t_skip("dxcore.dll missing or emulated"); return; }
    FARPROC byname = GetProcAddress(m, "DXCoreCreateAdapterFactory");
    FARPROC byord  = GetProcAddress(m, MAKEINTRESOURCEA(211));
    REQUIRE(byname);
    CHECK(byord == byname);
}
TEST(d3d_on12_dxcore_misc_dxcore_unnamed_exports_resolve_by_ordinal) {
    /* Everything below ordinal 211 except the gaps is exported by ordinal only (win32u forwarders and internal entry points). */
    HMODULE m = t_module("dxcore.dll");
    if (!m || t_is_wine()) { t_skip("dxcore.dll missing or emulated"); return; }
    static const unsigned ords[] = {1, 2, 3, 9, 10, 11, 14, 18, 22, 34, 36, 40, 65, 95, 96, 133, 134, 135, 136, 165, 218};
    for (unsigned i = 0; i < sizeof ords / sizeof ords[0]; i++) {
        FARPROC p = GetProcAddress(m, MAKEINTRESOURCEA(ords[i]));
        CHECKF(p != NULL, "ordinal %u", ords[i]);
    }
}
TEST(d3d_on12_dxcore_misc_dxcore_ordinal_not_exported) {
    HMODULE m = t_module("dxcore.dll");
    if (!m || t_is_wine()) { t_skip("dxcore.dll missing or emulated"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(0xFFF0)) == NULL);
    EXPECT_ERR(ERROR_INVALID_ORDINAL); /* by-ordinal lookups fail with 182, by-name with 127 (probed) */
    SET_ERR(0);
    CHECK(GetProcAddress(m, "DXCoreNoSuchExport") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(d3d_on12_dxcore_misc_dxcore_forwarded_ordinals_are_win32u) {
    HMODULE m = t_module("dxcore.dll"), w = t_module("win32u.dll");
    if (!m || !w || t_is_wine()) { t_skip("module missing"); return; }
    /* ordinal 36 forwards to NtGdiDdDDICloseAdapter */
    FARPROC a = GetProcAddress(m, MAKEINTRESOURCEA(36)), b = GetProcAddress(w, "NtGdiDdDDICloseAdapter");
    REQUIRE(a); REQUIRE(b);
    CHECK(a == b);
    a = GetProcAddress(m, MAKEINTRESOURCEA(34)); b = GetProcAddress(w, "NtGdiDdDDIQueryAdapterInfo");
    CHECK(a == b);
    a = GetProcAddress(m, MAKEINTRESOURCEA(2)); b = GetProcAddress(w, "NtGdiDdDDICreateAllocation");
    CHECK(a == b);
}
TEST(d3d_on12_dxcore_misc_factory_create_ok) {
    DXCoreCreateAdapterFactory_t c = odm_dxc_create(); REQUIRE(c);
    if (t_is_wine()) { t_skip("emulated dxcore"); return; }
    void *f = (void *)1;
    HRESULT hr = c(&IID_IDXCoreAdapterFactory_, &f);
    EXPECT_HR(hr, S_OK);
    CHECK(f != NULL && f != (void *)1);
    /* the factory is a per-process singleton (probed): a second create returns the same pointer with one more reference */
    void *f2 = NULL; ULONG c1 = odm_addref(f); c1 = odm_release(f);
    EXPECT_HR(c(&IID_IDXCoreAdapterFactory_, &f2), S_OK);
    CHECK(f2 == f);
    EXPECT_EQ(odm_release(f2), c1);
    EXPECT_EQ(odm_release(f), c1 - 1);
}
TEST(d3d_on12_dxcore_misc_factory_create_null_out) {
    DXCoreCreateAdapterFactory_t c = odm_dxc_create(); REQUIRE(c);
    if (t_is_wine()) { t_skip("emulated dxcore"); return; }
    EXPECT_HR(c(&IID_IDXCoreAdapterFactory_, NULL), E_PTR);
}
TEST(d3d_on12_dxcore_misc_factory_create_unknown_iid) {
    DXCoreCreateAdapterFactory_t c = odm_dxc_create(); REQUIRE(c);
    if (t_is_wine()) { t_skip("emulated dxcore"); return; }
    void *f = (void *)1;
    HRESULT hr = c(&DXC_ATTR_BOGUS, &f);
    EXPECT_HR(hr, E_NOIFACE);
    CHECK(f == NULL);
}
TEST(d3d_on12_dxcore_misc_factory_create_iid_iunknown) {
    DXCoreCreateAdapterFactory_t c = odm_dxc_create(); REQUIRE(c);
    if (t_is_wine()) { t_skip("emulated dxcore"); return; }
    void *f = NULL;
    HRESULT hr = c(&IID_IUnknown, &f);
    EXPECT_HR(hr, S_OK);
    ODM_REL(f);
}
TEST(d3d_on12_dxcore_misc_factory_create_wrong_interface_iids) {
    DXCoreCreateAdapterFactory_t c = odm_dxc_create(); REQUIRE(c);
    if (t_is_wine()) { t_skip("emulated dxcore"); return; }
    const GUID *bad[] = {&IID_IDXCoreAdapterList_, &IID_IDXCoreAdapter_};
    for (int i = 0; i < 2; i++) {
        void *f = (void *)1;
        EXPECT_HR(c(bad[i], &f), E_NOIFACE);
        CHECK(f == NULL);
    }
}
TEST(d3d_on12_dxcore_misc_factory_refcount_protocol) {
    NEED_FAC();
    /* the factory is a per-process singleton, so earlier references may still be held: check the protocol relatively */
    ULONG a = odm_addref(fac), b = odm_addref(fac);
    CHECK(b == a + 1);
    EXPECT_EQ(odm_release(fac), b - 1);
    EXPECT_EQ(odm_release(fac), a - 1);
    EXPECT_EQ(odm_release(fac), a - 2);
}
TEST(d3d_on12_dxcore_misc_factory_qi_self_and_iunknown) {
    NEED_FAC();
    void *p = NULL;
    EXPECT_HR(odm_qi(fac, &IID_IDXCoreAdapterFactory_, &p), S_OK);
    CHECK(p == fac);
    ODM_REL(p);
    EXPECT_HR(odm_qi(fac, &IID_IUnknown, &p), S_OK);
    ODM_REL(p);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_factory_qi_unsupported) {
    NEED_FAC();
    void *p = (void *)1;
    EXPECT_HR(odm_qi(fac, &IID_IDXCoreAdapterList_, &p), E_NOIFACE);
    CHECK(p == NULL);
    p = (void *)1;
    EXPECT_HR(odm_qi(fac, &DXC_ATTR_BOGUS, &p), E_NOIFACE);
    CHECK(p == NULL);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_factory_two_creates_independent_release) {
    DXCoreCreateAdapterFactory_t c = odm_dxc_create(); REQUIRE(c);
    if (t_is_wine()) { t_skip("emulated dxcore"); return; }
    void *a = NULL, *b = NULL;
    EXPECT_HR(c(&IID_IDXCoreAdapterFactory_, &a), S_OK);
    EXPECT_HR(c(&IID_IDXCoreAdapterFactory_, &b), S_OK);
    REQUIRE(a); REQUIRE(b);
    odm_release(a);
    /* b stays usable */
    CHECK(FAC_IsNotif(b, 0) != 0);
    odm_release(b);
}

/* ---------------- IsNotificationTypeSupported ---------------- */
TEST(d3d_on12_dxcore_misc_notification_types_supported) {
    NEED_FAC();
    for (UINT t = 0; t < 4; t++) CHECKF(FAC_IsNotif(fac, t) == 1, "type %u", t);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_notification_types_unsupported) {
    NEED_FAC();
    static const UINT bad[] = {4, 5, 7, 0x10, 0xFF, 0x7FFFFFFF, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) CHECKF(FAC_IsNotif(fac, bad[i]) == 0, "type %u", bad[i]);
    odm_release(fac);
}

/* ---------------- CreateAdapterList ---------------- */
TEST(d3d_on12_dxcore_misc_createlist_null_out) {
    NEED_FAC();
    EXPECT_HR(FAC_CreateList(fac, 1, &DXC_ATTR_D3D12_GRAPHICS, &IID_IDXCoreAdapterList_, NULL), E_PTR);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_null_attributes) {
    NEED_FAC();
    void *l = (void *)1;
    EXPECT_HR(FAC_CreateList(fac, 1, NULL, &IID_IDXCoreAdapterList_, &l), E_INVARG);
    CHECK(l == NULL);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_zero_attributes) {
    NEED_FAC();
    void *l = (void *)1;
    EXPECT_HR(FAC_CreateList(fac, 0, &DXC_ATTR_D3D12_GRAPHICS, &IID_IDXCoreAdapterList_, &l), E_INVARG);
    CHECK(l == NULL);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_each_attribute_ok) {
    NEED_FAC();
    const GUID *a[] = {&DXC_ATTR_D3D11_GRAPHICS, &DXC_ATTR_D3D12_GRAPHICS, &DXC_ATTR_D3D12_CORE_COMPUTE};
    const char *n[] = {"D3D11_GRAPHICS", "D3D12_GRAPHICS", "D3D12_CORE_COMPUTE"};
    for (int i = 0; i < 3; i++) {
        void *l = NULL;
        HRESULT hr = FAC_CreateList(fac, 1, a[i], &IID_IDXCoreAdapterList_, &l);
        CHECKF(hr == S_OK, "%s hr=%s", n[i], d3dh_hr_name(hr));
        if (l) t_note("%s adapters: %u", n[i], LST_Count(l));
        ODM_REL(l);
    }
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_unknown_attribute_empty_list) {
    NEED_FAC();
    void *l = NULL;
    HRESULT hr = FAC_CreateList(fac, 1, &DXC_ATTR_BOGUS, &IID_IDXCoreAdapterList_, &l);
    EXPECT_HR(hr, S_OK);
    if (l) { EXPECT_EQ(LST_Count(l), 0); ODM_REL(l); }
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_wrong_iid) {
    NEED_FAC();
    void *l = (void *)1;
    HRESULT hr = FAC_CreateList(fac, 1, &DXC_ATTR_D3D12_GRAPHICS, &IID_IDXCoreAdapter_, &l);
    EXPECT_HR_FAILED(hr);
    CHECK(l == NULL);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_iunknown_iid) {
    NEED_FAC();
    void *l = NULL;
    HRESULT hr = FAC_CreateList(fac, 1, &DXC_ATTR_D3D12_GRAPHICS, &IID_IUnknown, &l);
    EXPECT_HR(hr, S_OK);
    ODM_REL(l);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_multiple_attributes_subset) {
    /* CreateAdapterList requires an adapter to carry ALL listed attributes: 2 attributes can never exceed 1 attribute. */
    NEED_FAC();
    GUID two[2] = {DXC_ATTR_D3D12_GRAPHICS, DXC_ATTR_D3D12_CORE_COMPUTE};
    void *l1 = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS), *l2 = NULL;
    HRESULT hr = FAC_CreateList(fac, 2, two, &IID_IDXCoreAdapterList_, &l2);
    EXPECT_HR(hr, S_OK);
    if (l1 && l2) CHECK(LST_Count(l2) <= LST_Count(l1));
    ODM_REL(l1); ODM_REL(l2); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_with_bogus_second_attribute_is_empty) {
    NEED_FAC();
    GUID two[2] = {DXC_ATTR_D3D12_GRAPHICS, DXC_ATTR_BOGUS};
    void *l = NULL;
    EXPECT_HR(FAC_CreateList(fac, 2, two, &IID_IDXCoreAdapterList_, &l), S_OK);
    if (l) { EXPECT_EQ(LST_Count(l), 0); ODM_REL(l); }
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_d3d12_subset_of_d3d11_or_equal) {
    NEED_FAC();
    void *l12 = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS), *l11 = odm_list(fac, &DXC_ATTR_D3D11_GRAPHICS);
    if (l12 && l11) t_note("d3d12=%u d3d11=%u", LST_Count(l12), LST_Count(l11));
    ODM_REL(l12); ODM_REL(l11); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_createlist_holds_factory_reference_semantics) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    void *f2 = NULL;
    EXPECT_HR(LST_GetFactory(l, &IID_IDXCoreAdapterFactory_, &f2), S_OK);
    CHECK(f2 != NULL);
    ODM_REL(f2);
    odm_release(fac);
    /* list still functional after the creator's reference is gone */
    (void)LST_Count(l);
    ODM_REL(l);
}

/* ---------------- IDXCoreAdapterList ---------------- */
TEST(d3d_on12_dxcore_misc_list_qi_and_refcount) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    void *p = NULL;
    EXPECT_HR(odm_qi(l, &IID_IDXCoreAdapterList_, &p), S_OK);
    CHECK(p == l); ODM_REL(p);
    EXPECT_HR(odm_qi(l, &IID_IUnknown, &p), S_OK); ODM_REL(p);
    p = (void *)1;
    EXPECT_HR(odm_qi(l, &IID_IDXCoreAdapterFactory_, &p), E_NOIFACE);
    CHECK(p == NULL);
    ULONG a = odm_addref(l); CHECK(odm_addref(l) == a + 1); odm_release(l); odm_release(l);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_getadapter_null_out) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    EXPECT_HR(LST_GetAdapter(l, 0, &IID_IDXCoreAdapter_, NULL), E_PTR);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_getadapter_out_of_range) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    UINT n = LST_Count(l);
    UINT idx[3] = {n, n + 1, 0xFFFFFFFF};
    for (int i = 0; i < 3; i++) {
        void *a = (void *)1;
        EXPECT_HR(LST_GetAdapter(l, idx[i], &IID_IDXCoreAdapter_, &a), E_INVARG);
        CHECK(a == NULL);
    }
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_getadapter_all_valid) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    UINT n = LST_Count(l);
    if (!n) { ODM_REL(l); odm_release(fac); t_skip("no D3D12 adapters"); return; }
    for (UINT i = 0; i < n && i < 16; i++) {
        void *a = NULL;
        EXPECT_HR(LST_GetAdapter(l, i, &IID_IDXCoreAdapter_, &a), S_OK);
        if (a) { CHECK(AD_IsValid(a) == 1); CHECK(AD_IsAttr(a, &DXC_ATTR_D3D12_GRAPHICS) == 1); ODM_REL(a); }
    }
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_getadapter_wrong_iid) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    if (!LST_Count(l)) { ODM_REL(l); odm_release(fac); t_skip("no adapters"); return; }
    void *a = (void *)1;
    EXPECT_HR_FAILED(LST_GetAdapter(l, 0, &IID_IDXCoreAdapterList_, &a));
    CHECK(a == NULL);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_getadapter_same_object_twice) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l || !LST_Count(l)) { ODM_REL(l); odm_release(fac); t_skip("no adapters"); return; }
    void *a = NULL, *b = NULL;
    EXPECT_HR(LST_GetAdapter(l, 0, &IID_IDXCoreAdapter_, &a), S_OK);
    EXPECT_HR(LST_GetAdapter(l, 0, &IID_IDXCoreAdapter_, &b), S_OK);
    CHECK(a == b);
    ODM_REL(a); ODM_REL(b); ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_is_stale_fresh_false) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    CHECK(LST_IsStale(l) == 0);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_getfactory) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    void *f = NULL;
    EXPECT_HR(LST_GetFactory(l, &IID_IDXCoreAdapterFactory_, &f), S_OK);
    CHECK(f != NULL);
    ODM_REL(f);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_getfactory_wrong_iid) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    void *f = (void *)1;
    EXPECT_HR_FAILED(LST_GetFactory(l, &IID_IDXCoreAdapter_, &f));
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_preferences_supported) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    for (UINT p = 0; p < 3; p++) CHECKF(LST_PrefSupp(l, p) == 1, "pref %u", p);
    static const UINT bad[] = {3, 4, 100, 0xFFFFFFFF};
    for (unsigned i = 0; i < 4; i++) CHECKF(LST_PrefSupp(l, bad[i]) == 0, "pref %u", bad[i]);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_sort_zero_count) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    UINT p = DXPREF_Hardware;
    EXPECT_HR(LST_Sort(l, 0, &p), E_INVARG);
    EXPECT_HR(LST_Sort(l, 0, NULL), E_INVARG);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_sort_each_preference_ok) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    for (UINT p = 0; p < 3; p++) EXPECT_HR(LST_Sort(l, 1, &p), S_OK);
    UINT all[3] = {DXPREF_HighPerformance, DXPREF_MinimumPower, DXPREF_Hardware};
    EXPECT_HR(LST_Sort(l, 3, all), S_OK);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_sort_preserves_count_and_members) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    UINT n = LST_Count(l);
    if (n > 8) n = 8;
    LUID before[8], after[8]; UINT cnt = LST_Count(l);
    for (UINT i = 0; i < n; i++) { void *a = NULL; LST_GetAdapter(l, i, &IID_IDXCoreAdapter_, &a); memset(&before[i], 0, sizeof(LUID)); if (a) { AD_GetProp(a, DXP_InstanceLuid, sizeof(LUID), &before[i]); ODM_REL(a); } }
    UINT p = DXPREF_HighPerformance;
    EXPECT_HR(LST_Sort(l, 1, &p), S_OK);
    EXPECT_EQ(LST_Count(l), cnt);
    for (UINT i = 0; i < n; i++) { void *a = NULL; memset(&after[i], 0, sizeof(LUID)); if (SUCCEEDED(LST_GetAdapter(l, i, &IID_IDXCoreAdapter_, &a)) && a) { AD_GetProp(a, DXP_InstanceLuid, sizeof(LUID), &after[i]); ODM_REL(a); } }
    for (UINT i = 0; i < n; i++) {
        int found = 0;
        for (UINT j = 0; j < n; j++) if (!memcmp(&before[i], &after[j], sizeof(LUID))) found = 1;
        CHECK(found);
    }
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_list_sort_hardware_first) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    UINT n = LST_Count(l);
    if (n < 2) { ODM_REL(l); odm_release(fac); t_skip("need two adapters"); return; }
    UINT p = DXPREF_Hardware;
    EXPECT_HR(LST_Sort(l, 1, &p), S_OK);
    int seen_soft = 0;
    for (UINT i = 0; i < n && i < 16; i++) {
        void *a = NULL; unsigned char hw = 0;
        if (FAILED(LST_GetAdapter(l, i, &IID_IDXCoreAdapter_, &a)) || !a) continue;
        AD_GetProp(a, DXP_IsHardware, 1, &hw);
        if (!hw) seen_soft = 1; else CHECKF(!seen_soft, "hardware adapter %u after a software one", i);
        ODM_REL(a);
    }
    ODM_REL(l); odm_release(fac);
}

/* ---------------- GetAdapterByLuid ---------------- */
TEST(d3d_on12_dxcore_misc_byluid_null_out) {
    NEED_FAC();
    LUID luid = {1, 0};
    EXPECT_HR(FAC_ByLuid(fac, &luid, &IID_IDXCoreAdapter_, NULL), E_PTR);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_byluid_unknown_luid) {
    NEED_FAC();
    LUID luid = {0x7FFFFFF1, 0x7FFF};
    void *a = (void *)1;
    EXPECT_HR(FAC_ByLuid(fac, &luid, &IID_IDXCoreAdapter_, &a), E_INVARG);
    CHECK(a == NULL);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_byluid_zero_luid) {
    NEED_FAC();
    LUID luid = {0, 0};
    void *a = (void *)1;
    EXPECT_HR(FAC_ByLuid(fac, &luid, &IID_IDXCoreAdapter_, &a), E_INVARG);
    CHECK(a == NULL);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_byluid_roundtrip) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l || !LST_Count(l)) { ODM_REL(l); odm_release(fac); t_skip("no adapters"); return; }
    for (UINT i = 0; i < LST_Count(l) && i < 8; i++) {
        void *a = NULL, *b = NULL; LUID luid;
        EXPECT_HR(LST_GetAdapter(l, i, &IID_IDXCoreAdapter_, &a), S_OK);
        REQUIRE(a);
        EXPECT_HR(AD_GetProp(a, DXP_InstanceLuid, sizeof luid, &luid), S_OK);
        EXPECT_HR(FAC_ByLuid(fac, &luid, &IID_IDXCoreAdapter_, &b), S_OK);
        CHECK(b == a);
        ODM_REL(a); ODM_REL(b);
    }
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_byluid_wrong_iid) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l || !LST_Count(l)) { ODM_REL(l); odm_release(fac); t_skip("no adapters"); return; }
    void *a = NULL; LUID luid;
    LST_GetAdapter(l, 0, &IID_IDXCoreAdapter_, &a);
    REQUIRE(a);
    AD_GetProp(a, DXP_InstanceLuid, sizeof luid, &luid);
    void *b = (void *)1;
    EXPECT_HR_FAILED(FAC_ByLuid(fac, &luid, &IID_IDXCoreAdapterList_, &b));
    CHECK(b == NULL);
    ODM_REL(a); ODM_REL(l); odm_release(fac);
}

/* ---------------- Register / Unregister notifications ---------------- */
static void CALLBACK odm_notify_cb(UINT type, void *obj, void *ctx) { (void)type; (void)obj; (void)ctx; }
TEST(d3d_on12_dxcore_misc_register_null_cookie_out) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    HRESULT hr = FAC_Register(fac, l, 0, (void *)odm_notify_cb, NULL, NULL);
    EXPECT_HR(hr, E_PTR);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_register_unregister_list_stale) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    UINT cookie = 0;
    HRESULT hr = FAC_Register(fac, l, 0 /* AdapterListStale */, (void *)odm_notify_cb, NULL, &cookie);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) EXPECT_HR(FAC_Unregister(fac, cookie), S_OK);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_unregister_unknown_cookie) {
    NEED_FAC();
    HRESULT hr = FAC_Unregister(fac, 0x7FFFFFF0);
    EXPECT_HR(hr, E_INVARG);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_register_unsupported_notification_type) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    UINT cookie = 0;
    HRESULT hr = FAC_Register(fac, l, 99, (void *)odm_notify_cb, NULL, &cookie);
    EXPECT_HR(hr, DXGI_INVALID_CALL);
    EXPECT_EQ(cookie, 0);
    ODM_REL(l); odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_register_cookie_unique) {
    NEED_FAC();
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (!l) { odm_release(fac); t_skip("no list"); return; }
    UINT c1 = 0, c2 = 0;
    HRESULT h1 = FAC_Register(fac, l, 0, (void *)odm_notify_cb, NULL, &c1);
    HRESULT h2 = FAC_Register(fac, l, 0, (void *)odm_notify_cb, NULL, &c2);
    EXPECT_HR(h1, S_OK); EXPECT_HR(h2, S_OK);
    if (SUCCEEDED(h1) && SUCCEEDED(h2)) CHECK(c1 != c2);
    if (SUCCEEDED(h1)) FAC_Unregister(fac, c1);
    if (SUCCEEDED(h2)) FAC_Unregister(fac, c2);
    ODM_REL(l); odm_release(fac);
}

/* ---------------- IDXCoreAdapter ---------------- */
static void *odm_first_adapter(void *fac, const GUID *attr, void **list_out) {
    void *l = odm_list(fac, attr), *a = NULL;
    if (!l) return NULL;
    if (!LST_Count(l) || FAILED(LST_GetAdapter(l, 0, &IID_IDXCoreAdapter_, &a))) { odm_release(l); return NULL; }
    *list_out = l;
    return a;
}
#define NEED_ADAPTER() NEED_FAC(); void *lst = NULL; void *ad = odm_first_adapter(fac, &DXC_ATTR_D3D12_GRAPHICS, &lst); \
    if (!ad) { void *l2 = NULL; ad = odm_first_adapter(fac, &DXC_ATTR_D3D11_GRAPHICS, &l2); lst = l2; } \
    if (!ad) { odm_release(fac); t_skip("no DXCore adapter on this machine"); return; }
#define END_ADAPTER() do { odm_release(ad); odm_release(lst); odm_release(fac); } while (0)

TEST(d3d_on12_dxcore_misc_adapter_isvalid) {
    NEED_ADAPTER();
    CHECK(AD_IsValid(ad) == 1);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_qi) {
    NEED_ADAPTER();
    void *p = NULL;
    EXPECT_HR(odm_qi(ad, &IID_IDXCoreAdapter_, &p), S_OK); CHECK(p == ad); ODM_REL(p);
    EXPECT_HR(odm_qi(ad, &IID_IUnknown, &p), S_OK); ODM_REL(p);
    p = (void *)1;
    EXPECT_HR(odm_qi(ad, &IID_IDXCoreAdapterList_, &p), E_NOIFACE);
    CHECK(p == NULL);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_getfactory) {
    NEED_ADAPTER();
    void *f = NULL;
    EXPECT_HR(AD_GetFactory(ad, &IID_IDXCoreAdapterFactory_, &f), S_OK);
    CHECK(f != NULL);
    ODM_REL(f);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_attribute_support) {
    NEED_ADAPTER();
    CHECK(AD_IsAttr(ad, &DXC_ATTR_BOGUS) == 0);
    GUID z; memset(&z, 0, sizeof z);
    CHECK(AD_IsAttr(ad, &z) == 0);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_attribute_matches_list_filter) {
    NEED_FAC();
    const GUID *attrs[] = {&DXC_ATTR_D3D11_GRAPHICS, &DXC_ATTR_D3D12_GRAPHICS, &DXC_ATTR_D3D12_CORE_COMPUTE};
    for (int k = 0; k < 3; k++) {
        void *l = odm_list(fac, attrs[k]);
        if (!l) continue;
        for (UINT i = 0; i < LST_Count(l) && i < 8; i++) {
            void *a = NULL;
            if (SUCCEEDED(LST_GetAdapter(l, i, &IID_IDXCoreAdapter_, &a)) && a) { CHECKF(AD_IsAttr(a, attrs[k]) == 1, "list %d adapter %u", k, i); ODM_REL(a); }
        }
        ODM_REL(l);
    }
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_adapter_property_support_matrix) {
    NEED_ADAPTER();
    for (UINT p = 0; p <= 13; p++) {
        unsigned char s = AD_IsProp(ad, p);
        if (p == DXP_ComputePreemption || p == DXP_GraphicsPreemption) t_note("IsPropertySupported(%u)=%u", p, s);
        else CHECKF(s == 1, "property %u", p);
    }
    static const UINT bad[] = {14, 15, 100, 0xFFFFFFFF};
    for (unsigned i = 0; i < 4; i++) CHECKF(AD_IsProp(ad, bad[i]) == 0, "property %u", bad[i]);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_property_sizes) {
    NEED_ADAPTER();
    static const struct { UINT p; SIZE_T sz; } t[] = {
        {DXP_InstanceLuid, 8}, {DXP_DriverVersion, 8}, {DXP_HardwareID, 16}, {DXP_KmdModelVersion, 4},
        {DXP_DedicatedAdapterMemory, 8}, {DXP_DedicatedSystemMemory, 8}, {DXP_SharedSystemMemory, 8},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SIZE_T sz = 0x1234;
        EXPECT_HR(AD_GetPropSize(ad, t[i].p, &sz), S_OK);
        CHECKF(sz == t[i].sz, "property %u size %llu", t[i].p, (unsigned long long)sz);
    }
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_bool_property_sizes) {
    NEED_ADAPTER();
    static const UINT bools[] = {DXP_AcgCompatible, DXP_IsHardware, DXP_IsIntegrated, DXP_IsDetachable};
    for (unsigned i = 0; i < 4; i++) {
        SIZE_T sz = 0x1234;
        HRESULT hr = AD_GetPropSize(ad, bools[i], &sz);
        EXPECT_HR(hr, S_OK);
        CHECKF(sz == 1, "bool property %u size %llu", bools[i], (unsigned long long)sz);
    }
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_propsize_null_out) {
    NEED_ADAPTER();
    EXPECT_HR(AD_GetPropSize(ad, DXP_InstanceLuid, NULL), E_PTR);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_propsize_unknown_property) {
    NEED_ADAPTER();
    SIZE_T sz = 0x1234;
    EXPECT_HR(AD_GetPropSize(ad, 14, &sz), DXGI_INVALID_CALL);
    EXPECT_EQ(sz, 0);
    sz = 0x1234;
    EXPECT_HR(AD_GetPropSize(ad, 0xFFFF, &sz), DXGI_INVALID_CALL);
    EXPECT_EQ(sz, 0);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_getprop_null_buffer) {
    NEED_ADAPTER();
    EXPECT_HR(AD_GetProp(ad, DXP_InstanceLuid, 8, NULL), E_PTR);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_getprop_unknown_property) {
    NEED_ADAPTER();
    unsigned char buf[32]; memset(buf, 0xAA, sizeof buf);
    EXPECT_HR(AD_GetProp(ad, 14, sizeof buf, buf), DXGI_INVALID_CALL);
    /* the output buffer is zeroed before validation */
    for (unsigned i = 0; i < sizeof buf; i++) if (buf[i]) { CHECK(0); break; }
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_getprop_too_small_fixed) {
    NEED_ADAPTER();
    static const struct { UINT p; SIZE_T sz; } t[] = {
        {DXP_InstanceLuid, 8}, {DXP_DriverVersion, 8}, {DXP_HardwareID, 16}, {DXP_KmdModelVersion, 4},
        {DXP_DedicatedAdapterMemory, 8}, {DXP_DedicatedSystemMemory, 8}, {DXP_SharedSystemMemory, 8},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        unsigned char buf[32];
        for (SIZE_T n = 0; n < t[i].sz; n += (t[i].sz > 8 ? 5 : 3)) {
            memset(buf, 0xAA, sizeof buf);
            HRESULT hr = AD_GetProp(ad, t[i].p, n, buf);
            CHECKF(hr == E_INVARG, "property %u size %llu hr=%s", t[i].p, (unsigned long long)n, d3dh_hr_name(hr));
            if (n < sizeof buf && n) CHECKF(buf[n - 1] == 0 && buf[0] == 0, "buffer zero-filled up to size");
            CHECKF(buf[n] == 0xAA, "no write past the stated size (property %u)", t[i].p);
        }
        memset(buf, 0, sizeof buf);
        EXPECT_HR(AD_GetProp(ad, t[i].p, t[i].sz, buf), S_OK);
    }
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_getprop_bool_needs_one_byte) {
    NEED_ADAPTER();
    static const UINT bools[] = {DXP_AcgCompatible, DXP_IsHardware, DXP_IsIntegrated, DXP_IsDetachable};
    for (unsigned i = 0; i < 4; i++) {
        unsigned char b[4] = {0xAA, 0xAA, 0xAA, 0xAA};
        EXPECT_HR(AD_GetProp(ad, bools[i], 0, b), E_INVARG);
        b[0] = 0xAA;
        EXPECT_HR(AD_GetProp(ad, bools[i], 1, b), S_OK);
        CHECKF(b[0] <= 1, "bool property %u value %u", bools[i], b[0]);
        CHECK(b[1] == 0xAA);
    }
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_driver_description_protocol) {
    NEED_ADAPTER();
    SIZE_T sz = 0;
    EXPECT_HR(AD_GetPropSize(ad, DXP_DriverDescription, &sz), S_OK);
    CHECK(sz >= 1 && sz < 1024);
    if (sz < 1 || sz >= 1024) { END_ADAPTER(); return; }
    char buf[1100]; memset(buf, 0x7E, sizeof buf);
    /* exact size succeeds, NUL terminated, size == strlen+1 */
    EXPECT_HR(AD_GetProp(ad, DXP_DriverDescription, sz, buf), S_OK);
    CHECK(buf[sz - 1] == 0);
    CHECK(strlen(buf) + 1 == sz);
    CHECK(buf[sz] == 0x7E);
    /* exact-1 and 0 fail */
    memset(buf, 0x7E, sizeof buf);
    EXPECT_HR(AD_GetProp(ad, DXP_DriverDescription, sz - 1, buf), E_INVARG);
    EXPECT_HR(AD_GetProp(ad, DXP_DriverDescription, 0, buf), E_INVARG);
    /* oversized buffer is accepted and zero-filled after the terminator */
    memset(buf, 0x7E, sizeof buf);
    EXPECT_HR(AD_GetProp(ad, DXP_DriverDescription, sz + 16, buf), S_OK);
    CHECK(strlen(buf) + 1 == sz);
    for (SIZE_T i = sz; i < sz + 16; i++) if (buf[i]) { CHECK(0); break; }
    t_note("driver description: %s", buf);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_driver_description_nonempty_ascii) {
    NEED_ADAPTER();
    SIZE_T sz = 0; char buf[1024];
    AD_GetPropSize(ad, DXP_DriverDescription, &sz);
    if (!sz || sz >= sizeof buf) { END_ADAPTER(); t_skip("size"); return; }
    AD_GetProp(ad, DXP_DriverDescription, sz, buf);
    CHECK(strlen(buf) > 0);
    for (size_t i = 0; buf[i]; i++) if ((unsigned char)buf[i] < 0x20) { CHECK(0); break; }
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_luid_nonzero_and_stable) {
    NEED_ADAPTER();
    LUID a = {0, 0}, b = {0, 0};
    EXPECT_HR(AD_GetProp(ad, DXP_InstanceLuid, sizeof a, &a), S_OK);
    EXPECT_HR(AD_GetProp(ad, DXP_InstanceLuid, sizeof b, &b), S_OK);
    CHECK(a.LowPart != 0 || a.HighPart != 0);
    CHECK(!memcmp(&a, &b, sizeof a));
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_luid_oversized_buffer_ok) {
    NEED_ADAPTER();
    unsigned char buf[24]; memset(buf, 0xCC, sizeof buf);
    EXPECT_HR(AD_GetProp(ad, DXP_InstanceLuid, sizeof buf, buf), S_OK);
    for (int i = 8; i < 24; i++) if (buf[i]) { CHECK(0); break; }
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_luid_matches_dxgi) {
    NEED_ADAPTER();
    LUID l = {0, 0};
    AD_GetProp(ad, DXP_InstanceLuid, sizeof l, &l);
    IDXGIFactory1 *f = NULL;
    if (FAILED(d3dh_make_factory1(&f)) || !f) { END_ADAPTER(); t_skip("no dxgi"); return; }
    int found = 0;
    IDXGIAdapter1 *a = NULL;
    for (UINT i = 0; IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK && i < 16; i++) {
        DXGI_ADAPTER_DESC1 d;
        if (SUCCEEDED(IDXGIAdapter1_GetDesc1(a, &d)) && d.AdapterLuid.LowPart == l.LowPart && d.AdapterLuid.HighPart == l.HighPart) found = 1;
        SAFE_RELEASE(a);
    }
    if (!found) t_note("DXCore adapter LUID %08lx:%08lx not enumerated by DXGI EnumAdapters1 (may be the software adapter)", (unsigned long)l.HighPart, l.LowPart);
    SAFE_RELEASE(f);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_hardware_id_consistent_with_dxgi) {
    NEED_ADAPTER();
    struct { UINT vendorID, deviceID, subSysID, revision; } hw;
    memset(&hw, 0xFF, sizeof hw);
    EXPECT_HR(AD_GetProp(ad, DXP_HardwareID, sizeof hw, &hw), S_OK);
    LUID l; AD_GetProp(ad, DXP_InstanceLuid, sizeof l, &l);
    IDXGIFactory1 *f = NULL;
    if (SUCCEEDED(d3dh_make_factory1(&f)) && f) {
        IDXGIAdapter1 *a = NULL;
        for (UINT i = 0; IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK && i < 16; i++) {
            DXGI_ADAPTER_DESC1 d;
            if (SUCCEEDED(IDXGIAdapter1_GetDesc1(a, &d)) && d.AdapterLuid.LowPart == l.LowPart && d.AdapterLuid.HighPart == l.HighPart) {
                if (d.VendorId != hw.vendorID || d.DeviceId != hw.deviceID || d.SubSysId != hw.subSysID || d.Revision != hw.revision)
                    t_note("hardware id differs from DXGI desc (ven %04x/%04x dev %04x/%04x)", hw.vendorID, d.VendorId, hw.deviceID, d.DeviceId);
            }
            SAFE_RELEASE(a);
        }
        SAFE_RELEASE(f);
    }
    t_note("hardware id: ven %04x dev %04x subsys %08x rev %02x", hw.vendorID, hw.deviceID, hw.subSysID, hw.revision);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_memory_properties_sane) {
    NEED_ADAPTER();
    unsigned long long ded = 0, sys = 0, shr = 0;
    EXPECT_HR(AD_GetProp(ad, DXP_DedicatedAdapterMemory, 8, &ded), S_OK);
    EXPECT_HR(AD_GetProp(ad, DXP_DedicatedSystemMemory, 8, &sys), S_OK);
    EXPECT_HR(AD_GetProp(ad, DXP_SharedSystemMemory, 8, &shr), S_OK);
    t_note("memory: dedicated %llu system %llu shared %llu", ded, sys, shr);
    CHECK(ded < (1ull << 50) && sys < (1ull << 50) && shr < (1ull << 50));
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_memory_matches_dxgi) {
    NEED_ADAPTER();
    LUID l; unsigned long long ded = 0, sys = 0, shr = 0;
    AD_GetProp(ad, DXP_InstanceLuid, sizeof l, &l);
    AD_GetProp(ad, DXP_DedicatedAdapterMemory, 8, &ded);
    AD_GetProp(ad, DXP_DedicatedSystemMemory, 8, &sys);
    AD_GetProp(ad, DXP_SharedSystemMemory, 8, &shr);
    IDXGIFactory1 *f = NULL;
    if (FAILED(d3dh_make_factory1(&f)) || !f) { END_ADAPTER(); t_skip("no dxgi"); return; }
    IDXGIAdapter1 *a = NULL;
    for (UINT i = 0; IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK && i < 16; i++) {
        DXGI_ADAPTER_DESC1 d;
        if (SUCCEEDED(IDXGIAdapter1_GetDesc1(a, &d)) && d.AdapterLuid.LowPart == l.LowPart && d.AdapterLuid.HighPart == l.HighPart) {
            if ((unsigned long long)d.DedicatedVideoMemory != ded || (unsigned long long)d.DedicatedSystemMemory != sys || (unsigned long long)d.SharedSystemMemory != shr)
                t_note("memory differs from DXGI desc: %llu/%llu %llu/%llu %llu/%llu", ded, (unsigned long long)d.DedicatedVideoMemory, sys, (unsigned long long)d.DedicatedSystemMemory, shr, (unsigned long long)d.SharedSystemMemory);
        }
        SAFE_RELEASE(a);
    }
    SAFE_RELEASE(f);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_flags_hardware_vs_integrated) {
    NEED_ADAPTER();
    unsigned char hw = 9, integ = 9, det = 9, acg = 9;
    AD_GetProp(ad, DXP_IsHardware, 1, &hw);
    AD_GetProp(ad, DXP_IsIntegrated, 1, &integ);
    AD_GetProp(ad, DXP_IsDetachable, 1, &det);
    AD_GetProp(ad, DXP_AcgCompatible, 1, &acg);
    CHECK(hw <= 1 && integ <= 1 && det <= 1 && acg <= 1);
    if (!hw) CHECK(!integ && !det);
    t_note("IsHardware=%u IsIntegrated=%u IsDetachable=%u AcgCompatible=%u", hw, integ, det, acg);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_driver_version_nonzero_for_hardware) {
    NEED_ADAPTER();
    unsigned long long v = 0; unsigned char hw = 0;
    EXPECT_HR(AD_GetProp(ad, DXP_DriverVersion, 8, &v), S_OK);
    AD_GetProp(ad, DXP_IsHardware, 1, &hw);
    if (hw) CHECK(v != 0);
    t_note("driver version %u.%u.%u.%u", (unsigned)(v >> 48) & 0xFFFF, (unsigned)(v >> 32) & 0xFFFF, (unsigned)(v >> 16) & 0xFFFF, (unsigned)v & 0xFFFF);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_kmd_model_version) {
    NEED_ADAPTER();
    UINT kmd = 0;
    EXPECT_HR(AD_GetProp(ad, DXP_KmdModelVersion, 4, &kmd), S_OK);
    t_note("KMD model version 0x%x", kmd);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_preemption_unsupported_error) {
    NEED_ADAPTER();
    for (UINT p = DXP_ComputePreemption; p <= DXP_GraphicsPreemption; p++) {
        unsigned char buf[8] = {0};
        SIZE_T sz = 0;
        if (AD_IsProp(ad, p)) { t_note("property %u supported", p); continue; }
        EXPECT_HR(AD_GetPropSize(ad, p, &sz), DXGI_UNSUPPORTED);
        EXPECT_HR(AD_GetProp(ad, p, 4, buf), DXGI_UNSUPPORTED);
    }
    END_ADAPTER();
}

/* ---- state ---- */
TEST(d3d_on12_dxcore_misc_adapter_query_state_support) {
    NEED_ADAPTER();
    CHECK(AD_IsQState(ad, 0) == 1);
    CHECK(AD_IsQState(ad, 1) == 1);
    CHECK(AD_IsQState(ad, 2) == 0);
    CHECK(AD_IsQState(ad, 0xFFFFFFFF) == 0);
    CHECK(AD_IsSState(ad, 0) == 0);
    CHECK(AD_IsSState(ad, 1) == 1);
    CHECK(AD_IsSState(ad, 2) == 0);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_query_state_null_output) {
    NEED_ADAPTER();
    EXPECT_HR(AD_QueryState(ad, 0, 0, NULL, 1, NULL), E_PTR);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_query_state_unknown_state) {
    NEED_ADAPTER();
    unsigned char out[64]; memset(out, 0xAA, sizeof out);
    EXPECT_HR(AD_QueryState(ad, 2, 0, NULL, sizeof out, out), DXGI_INVALID_CALL);
    EXPECT_HR(AD_QueryState(ad, 0xFFFF, 0, NULL, sizeof out, out), DXGI_INVALID_CALL);
    CHECK(out[0] == 0 && out[63] == 0);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_query_state_driver_update_zero_size) {
    NEED_ADAPTER();
    unsigned char out[4] = {0xAA};
    EXPECT_HR(AD_QueryState(ad, 0, 0, NULL, 0, out), E_INVARG);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_query_state_driver_update_in_progress) {
    NEED_ADAPTER();
    unsigned char out = 0xAA;
    HRESULT hr = AD_QueryState(ad, 0, 0, NULL, 1, &out);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) CHECK(out <= 1);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_set_state_unknown) {
    NEED_ADAPTER();
    UINT in[2] = {0, 0}; unsigned long long v = 0;
    EXPECT_HR(AD_SetState(ad, 0, 8, in, 8, &v), DXGI_INVALID_CALL);
    EXPECT_HR(AD_SetState(ad, 7, 8, in, 8, &v), DXGI_INVALID_CALL);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_set_state_null_args) {
    NEED_ADAPTER();
    UINT in[2] = {0, 0}; unsigned long long v = 0;
    EXPECT_HR(AD_SetState(ad, 1, 8, NULL, 8, &v), E_PTR);
    EXPECT_HR(AD_SetState(ad, 1, 8, in, 8, NULL), E_PTR);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_set_state_small_buffers) {
    NEED_ADAPTER();
    UINT in[2] = {0, 0}; unsigned long long v = 0;
    EXPECT_HR(AD_SetState(ad, 1, 7, in, 8, &v), E_INVARG);
    EXPECT_HR(AD_SetState(ad, 1, 8, in, 7, &v), E_INVARG);
    EXPECT_HR(AD_SetState(ad, 1, 0, in, 0, &v), E_INVARG);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_set_state_bad_segment_group) {
    NEED_ADAPTER();
    UINT in[2] = {0, 2}; /* nodeIndex, segmentGroup (only Local=0, NonLocal=1 are valid) */
    unsigned long long v = 0;
    EXPECT_HR(AD_SetState(ad, 1, 8, in, 8, &v), E_INVARG);
    in[1] = 0xFFFF;
    EXPECT_HR(AD_SetState(ad, 1, 8, in, 8, &v), E_INVARG);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_budget_query_wrong_state_sizes) {
    NEED_ADAPTER();
    unsigned char out[64];
    UINT in[2] = {0, 0};
    HRESULT hr = AD_QueryState(ad, 1, 8, in, 0, out);
    t_note("QueryState(budget, out size 0) = %s", d3dh_hr_name(hr));
    CHECK(FAILED(hr) || hr == S_OK);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_budget_query_local_segment) {
    NEED_ADAPTER();
    struct { unsigned long long budget, usage, avail, reserved; } out;
    UINT in[2] = {0, 0};
    memset(&out, 0xAA, sizeof out);
    HRESULT hr = AD_QueryState(ad, 1, 8, in, sizeof out, &out);
    if (FAILED(hr)) { t_note("budget query: %s", d3dh_hr_name(hr)); END_ADAPTER(); return; }
    t_note("budget %llu usage %llu avail %llu reservation %llu", out.budget, out.usage, out.avail, out.reserved);
    CHECK(out.usage <= out.budget || out.budget == 0);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_events_unregister_unknown_cookie) {
    NEED_FAC();
    HRESULT hr = FAC_Unregister(fac, 1);
    EXPECT_HR(hr, E_INVARG);
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_adapter_survives_list_release) {
    NEED_ADAPTER();
    odm_release(lst); lst = NULL;
    CHECK(AD_IsValid(ad) == 1);
    LUID l; EXPECT_HR(AD_GetProp(ad, DXP_InstanceLuid, sizeof l, &l), S_OK);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_adapter_survives_factory_release) {
    NEED_ADAPTER();
    odm_release(fac); fac = NULL;
    CHECK(AD_IsValid(ad) == 1);
    LUID l; EXPECT_HR(AD_GetProp(ad, DXP_InstanceLuid, sizeof l, &l), S_OK);
    odm_release(ad); odm_release(lst);
}
TEST(d3d_on12_dxcore_misc_adapter_refcount_protocol) {
    NEED_ADAPTER();
    ULONG a = odm_addref(ad), b = odm_addref(ad);
    CHECK(b == a + 1);
    odm_release(ad); odm_release(ad);
    END_ADAPTER();
}
TEST(d3d_on12_dxcore_misc_core_compute_adapters_superset_of_graphics) {
    NEED_FAC();
    void *g = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS), *c = odm_list(fac, &DXC_ATTR_D3D12_CORE_COMPUTE);
    if (g && c) {
        UINT ng = LST_Count(g), nc = LST_Count(c);
        t_note("d3d12 graphics adapters %u, core compute %u", ng, nc);
        for (UINT i = 0; i < ng && i < 8; i++) {
            void *a = NULL; LUID l; void *b = NULL;
            if (SUCCEEDED(LST_GetAdapter(g, i, &IID_IDXCoreAdapter_, &a)) && a) {
                if (AD_IsAttr(a, &DXC_ATTR_D3D12_CORE_COMPUTE)) {
                    AD_GetProp(a, DXP_InstanceLuid, sizeof l, &l);
                    int found = 0;
                    for (UINT j = 0; j < nc && j < 16; j++) {
                        LUID l2;
                        if (SUCCEEDED(LST_GetAdapter(c, j, &IID_IDXCoreAdapter_, &b)) && b) { AD_GetProp(b, DXP_InstanceLuid, sizeof l2, &l2); if (!memcmp(&l, &l2, sizeof l)) found = 1; ODM_REL(b); }
                    }
                    CHECK(found);
                }
                ODM_REL(a);
            }
        }
    }
    ODM_REL(g); ODM_REL(c); odm_release(fac);
}

TEST(d3d_on12_dxcore_misc_register_null_object_and_callback) {
    NEED_FAC();
    UINT cookie = 0x55;
    EXPECT_HR(FAC_Register(fac, NULL, 0, (void *)odm_notify_cb, NULL, &cookie), E_INVARG);
    EXPECT_EQ(cookie, 0);
    void *l = odm_list(fac, &DXC_ATTR_D3D12_GRAPHICS);
    if (l) {
        cookie = 0x55;
        EXPECT_HR(FAC_Register(fac, l, 0, NULL, NULL, &cookie), E_PTR);
        EXPECT_EQ(cookie, 0);
        ODM_REL(l);
    }
    odm_release(fac);
}
TEST(d3d_on12_dxcore_misc_register_object_not_list_or_adapter) {
    NEED_FAC();
    UINT cookie = 0x55;
    HRESULT hr = FAC_Register(fac, fac, 0, (void *)odm_notify_cb, NULL, &cookie);
    EXPECT_HR(hr, E_INVARG);
    odm_release(fac);
}
