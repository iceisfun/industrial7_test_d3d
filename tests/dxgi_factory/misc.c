/* Window association, IsCurrent, stereo, CheckFeatureSupport, occlusion and adapters-changed registration. */
#include "fh.h"

/* ---------------- MakeWindowAssociation / GetWindowAssociation ---------------- */
TEST(dxgi_factory_mwa_null_window_all_valid_flags) {
    NEED_F1(f);
    for (UINT fl = 0; fl <= DXGI_MWA_VALID; fl++)
        EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, NULL, fl), S_OK);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_mwa_window_all_valid_flags) {
    NEED_F1(f);
    HWND w = d3dh_create_hidden_window();
    if (!w) { t_skip("no window"); IDXGIFactory1_Release(f); return; }
    LONG_PTR proc = GetWindowLongPtrW(w, GWLP_WNDPROC);
    for (UINT fl = 0; fl <= DXGI_MWA_VALID; fl++) {
        EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, w, fl), S_OK);
        CHECKF(GetWindowLongPtrW(w, GWLP_WNDPROC) == proc, "flags %u changed the window procedure", fl);
    }
    IDXGIFactory1_MakeWindowAssociation(f, NULL, 0);
    IDXGIFactory1_Release(f);
    d3dh_destroy_window(w);
}
TEST(dxgi_factory_mwa_invalid_flag_bits) {
    NEED_F1(f);
    HWND w = d3dh_create_hidden_window();
    static const UINT bad[] = { DXGI_MWA_VALID + 1, 0x8, 0x10, 0x20, 0x100, 0x10000, 0x80000000u, 0xffffffffu, 0xfffffff8u };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        CHECKF(IDXGIFactory1_MakeWindowAssociation(f, w, bad[i]) == DXGI_E_INVALID_CALL, "window flags %#x", bad[i]);
        HRESULT hr = IDXGIFactory1_MakeWindowAssociation(f, NULL, bad[i]);
        CHECKF(hr == DXGI_E_INVALID_CALL, "null window flags %#x = %s", bad[i], d3dh_hr_name(hr));
    }
    IDXGIFactory1_Release(f);
    if (w) d3dh_destroy_window(w);
}
TEST(dxgi_factory_mwa_valid_mask_value) {
    CHECK(DXGI_MWA_NO_WINDOW_CHANGES == 1);
    CHECK(DXGI_MWA_NO_ALT_ENTER == 2);
    CHECK(DXGI_MWA_NO_PRINT_SCREEN == 4);
    CHECK(DXGI_MWA_VALID == 7);
}
TEST(dxgi_factory_gwa_null_pointer) {
    NEED_F1(f);
    EXPECT_HR(IDXGIFactory1_GetWindowAssociation(f, NULL), DXGI_E_INVALID_CALL);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_gwa_initial_value) {
    NEED_F1(f);
    HWND h = (HWND)POISON;
    EXPECT_HR(IDXGIFactory1_GetWindowAssociation(f, &h), S_OK);
    CHECK(h != (HWND)POISON);
    t_note("GetWindowAssociation on fresh factory = %p", (void *)h);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_gwa_after_make_observed) {
    NEED_F1(f);
    HWND w = d3dh_create_hidden_window();
    if (!w) { t_skip("no window"); IDXGIFactory1_Release(f); return; }
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, w, 0), S_OK);
    HWND h = (HWND)POISON;
    EXPECT_HR(IDXGIFactory1_GetWindowAssociation(f, &h), S_OK);
    CHECK(h == NULL || h == w);
    t_note("GetWindowAssociation after Make(w) = %s", h == w ? "the window" : (h ? "other" : "NULL"));
    IDXGIFactory1_MakeWindowAssociation(f, NULL, 0);
    IDXGIFactory1_Release(f);
    d3dh_destroy_window(w);
}
TEST(dxgi_factory_gwa_after_null_make) {
    NEED_F1(f);
    HWND w = d3dh_create_hidden_window();
    IDXGIFactory1_MakeWindowAssociation(f, w, DXGI_MWA_NO_ALT_ENTER);
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, NULL, 0), S_OK);
    HWND h = (HWND)POISON;
    EXPECT_HR(IDXGIFactory1_GetWindowAssociation(f, &h), S_OK);
    CHECK(h == NULL);
    IDXGIFactory1_Release(f);
    if (w) d3dh_destroy_window(w);
}
TEST(dxgi_factory_mwa_two_factories_independent) {
    NEED_F1(f);
    IDXGIFactory1 *g = fh_f1();
    HWND w = d3dh_create_hidden_window();
    if (!g || !w) { t_skip("setup"); if (g) IDXGIFactory1_Release(g); IDXGIFactory1_Release(f); if (w) d3dh_destroy_window(w); return; }
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, w, DXGI_MWA_NO_ALT_ENTER), S_OK);
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(g, w, DXGI_MWA_NO_PRINT_SCREEN), S_OK);
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, NULL, 0), S_OK);
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(g, NULL, 0), S_OK);
    IDXGIFactory1_Release(g);
    IDXGIFactory1_Release(f);
    d3dh_destroy_window(w);
}
TEST(dxgi_factory_mwa_two_windows_one_factory) {
    NEED_F1(f);
    HWND a = d3dh_create_hidden_window(), b = d3dh_create_hidden_window();
    if (!a || !b) { t_skip("windows"); IDXGIFactory1_Release(f); if (a) d3dh_destroy_window(a); if (b) d3dh_destroy_window(b); return; }
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, a, 0), S_OK);
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, b, DXGI_MWA_NO_WINDOW_CHANGES), S_OK);
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, a, DXGI_MWA_VALID), S_OK);
    IDXGIFactory1_MakeWindowAssociation(f, NULL, 0);
    IDXGIFactory1_Release(f);
    d3dh_destroy_window(a); d3dh_destroy_window(b);
}
TEST(dxgi_factory_mwa_window_destroyed_before_factory) {
    NEED_F1(f);
    HWND w = d3dh_create_hidden_window();
    if (!w) { t_skip("no window"); IDXGIFactory1_Release(f); return; }
    EXPECT_HR(IDXGIFactory1_MakeWindowAssociation(f, w, 0), S_OK);
    d3dh_destroy_window(w);
    HWND h = (HWND)POISON;
    EXPECT_HR(IDXGIFactory1_GetWindowAssociation(f, &h), S_OK);
    EXPECT_EQ(IDXGIFactory1_Release(f), 0);
}
TEST(dxgi_factory_mwa_invalid_hwnd_observed) {
    NEED_F1(f);
    HRESULT hr = IDXGIFactory1_MakeWindowAssociation(f, (HWND)(ULONG_PTR)0x7fff0, 0);
    t_note("MakeWindowAssociation(non-window handle) = %s", d3dh_hr_name(hr));
    IDXGIFactory1_MakeWindowAssociation(f, NULL, 0);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_mwa_through_every_interface) {
    for (size_t k = 0; k < 4; k++) {
        static const IID *iids[] = { &IID_IDXGIFactory2, &IID_IDXGIFactory3, &IID_IDXGIFactory4, &IID_IDXGIFactory1 };
        IDXGIFactory2 *f = NULL;
        if (FAILED(fh_make(iids[k], (void **)&f))) continue;
        HWND w = d3dh_create_hidden_window();
        EXPECT_HR(IDXGIFactory2_MakeWindowAssociation(f, w, DXGI_MWA_NO_ALT_ENTER), S_OK);
        HWND h = NULL;
        EXPECT_HR(IDXGIFactory2_GetWindowAssociation(f, &h), S_OK);
        EXPECT_HR(IDXGIFactory2_MakeWindowAssociation(f, w, 8), DXGI_E_INVALID_CALL);
        IDXGIFactory2_MakeWindowAssociation(f, NULL, 0);
        EXPECT_EQ(IDXGIFactory2_Release(f), 0);
        if (w) d3dh_destroy_window(w);
    }
}
TEST(dxgi_factory_mwa_cf0_factory) {
    WINE_NOFAC();
    if (!fh_cf0()) { t_skip("no CreateDXGIFactory"); return; }
    IDXGIFactory *f = NULL;
    EXPECT_HR(fh_cf0()(&IID_IDXGIFactory, (void **)&f), S_OK);
    if (!f) return;
    EXPECT_HR(IDXGIFactory_MakeWindowAssociation(f, NULL, DXGI_MWA_VALID), S_OK);
    EXPECT_HR(IDXGIFactory_MakeWindowAssociation(f, NULL, 8), DXGI_E_INVALID_CALL);
    HWND h = NULL;
    EXPECT_HR(IDXGIFactory_GetWindowAssociation(f, &h), S_OK);
    EXPECT_HR(IDXGIFactory_GetWindowAssociation(f, NULL), DXGI_E_INVALID_CALL);
    IDXGIFactory_Release(f);
}

/* ---------------- IsCurrent / IsWindowedStereoEnabled ---------------- */
TEST(dxgi_factory_is_current_fresh) {
    NEED_F1(f);
    BOOL cur = IDXGIFactory1_IsCurrent(f);
    CHECKF(cur == TRUE, "fresh factory IsCurrent = %d", cur);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_is_current_repeatable) {
    NEED_F1(f);
    BOOL a = IDXGIFactory1_IsCurrent(f);
    for (int i = 0; i < 50; i++) CHECK(IDXGIFactory1_IsCurrent(f) == a);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_is_current_all_interfaces) {
    NEED_F1(f);
    IDXGIFactory2 *f2 = fh_qi((IUnknown *)f, &IID_IDXGIFactory2);
    IDXGIFactory4 *f4 = fh_qi((IUnknown *)f, &IID_IDXGIFactory4);
    BOOL c1 = IDXGIFactory1_IsCurrent(f);
    if (f2) { CHECK(IDXGIFactory2_IsCurrent(f2) == c1); IDXGIFactory2_Release(f2); }
    if (f4) { CHECK(IDXGIFactory4_IsCurrent(f4) == c1); IDXGIFactory4_Release(f4); }
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_is_current_after_enumeration) {
    NEED_F1(f);
    IDXGIAdapter1 *a;
    for (UINT i = 0; i < 8 && SUCCEEDED(IDXGIFactory1_EnumAdapters1(f, i, &a)); i++) IDXGIAdapter1_Release(a);
    CHECK(IDXGIFactory1_IsCurrent(f) == TRUE);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_is_current_new_factory_also_current) {
    NEED_F1(f);
    IDXGIFactory1 *g = fh_f1();
    if (g) { CHECK(IDXGIFactory1_IsCurrent(g) == TRUE); CHECK(IDXGIFactory1_IsCurrent(f) == TRUE); IDXGIFactory1_Release(g); }
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_stereo_enabled_value) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    BOOL s = IDXGIFactory2_IsWindowedStereoEnabled(f2);
    CHECKF(s == TRUE || s == FALSE, "value %d", s);
    t_note("IsWindowedStereoEnabled = %d", s);
    for (int i = 0; i < 10; i++) CHECK(IDXGIFactory2_IsWindowedStereoEnabled(f2) == s);
    IDXGIFactory2_Release(f2);
}
TEST(dxgi_factory_stereo_consistent_across_factories) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    IDXGIFactory2 *g = NULL;
    if (SUCCEEDED(fh_make(&IID_IDXGIFactory2, (void **)&g))) { CHECK(IDXGIFactory2_IsWindowedStereoEnabled(g) == IDXGIFactory2_IsWindowedStereoEnabled(f2)); IDXGIFactory2_Release(g); }
    IDXGIFactory2_Release(f2);
}

/* ---------------- CheckFeatureSupport ---------------- */
TEST(dxgi_factory_feature_tearing_ok) {
    NEED_FAC(f5, IDXGIFactory5, IID_IDXGIFactory5);
    BOOL data = (BOOL)0xdeadbeef;
    EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &data, sizeof(data)), S_OK);
    CHECKF(data == TRUE || data == FALSE, "data = %#x", data);
    t_note("PRESENT_ALLOW_TEARING supported = %d", data);
    IDXGIFactory5_Release(f5);
}
TEST(dxgi_factory_feature_tearing_size_protocol) {
    NEED_FAC(f5, IDXGIFactory5, IID_IDXGIFactory5);
    BYTE buf[32]; memset(buf, 0xcd, sizeof(buf));
    static const UINT bad[] = { 0, 1, 2, 3, 5, 6, 8, 16, 32, 0xffffffffu };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        HRESULT hr = IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, buf, bad[i]);
        CHECKF(hr == DXGI_E_INVALID_CALL, "size %u = %s", bad[i], d3dh_hr_name(hr));
    }
    CHECK(buf[0] == 0xcd && buf[4] == 0xcd);
    IDXGIFactory5_Release(f5);
}
TEST(dxgi_factory_feature_tearing_writes_only_bool) {
    NEED_FAC(f5, IDXGIFactory5, IID_IDXGIFactory5);
    BYTE buf[16]; memset(buf, 0xee, sizeof(buf));
    EXPECT_HR(IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, buf, 4), S_OK);
    for (int i = 4; i < 16; i++) CHECKF(buf[i] == 0xee, "byte %d overwritten", i);
    IDXGIFactory5_Release(f5);
}
TEST(dxgi_factory_feature_unknown_ids) {
    NEED_FAC(f5, IDXGIFactory5, IID_IDXGIFactory5);
    static const int ids[] = { 1, 2, 3, 0x12345678, 0x7fffffff, -1, 100, 0x10000 };
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        BOOL d = (BOOL)0xdeadbeef;
        HRESULT hr = IDXGIFactory5_CheckFeatureSupport(f5, (DXGI_FEATURE)ids[i], &d, sizeof(d));
        CHECKF(hr == DXGI_E_INVALID_CALL, "feature %d = %s", ids[i], d3dh_hr_name(hr));
        CHECKF(d == (BOOL)0xdeadbeef, "feature %d wrote output", ids[i]);
    }
    IDXGIFactory5_Release(f5);
}
TEST(dxgi_factory_feature_repeatable_and_consistent) {
    NEED_FAC(f5, IDXGIFactory5, IID_IDXGIFactory5);
    BOOL a = 7, b = 9;
    IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &a, sizeof(a));
    IDXGIFactory5_CheckFeatureSupport(f5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &b, sizeof(b));
    CHECK(a == b);
    IDXGIFactory5 *g = NULL;
    if (SUCCEEDED(fh_make(&IID_IDXGIFactory5, (void **)&g))) {
        BOOL c = 5; IDXGIFactory5_CheckFeatureSupport(g, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &c, sizeof(c)); CHECK(c == a);
        IDXGIFactory5_Release(g);
    }
    IDXGIFactory5_Release(f5);
}
TEST(dxgi_factory_feature_via_factory7) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    BOOL d = 3;
    EXPECT_HR(IDXGIFactory7_CheckFeatureSupport(f7, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &d, sizeof(d)), S_OK);
    IDXGIFactory7_Release(f7);
}

/* ---------------- occlusion status ---------------- */
#define WM_OCC (WM_USER + 7)
TEST(dxgi_factory_occlusion_window_register_unregister) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HWND w = d3dh_create_hidden_window();
    if (!w) { t_skip("no window"); IDXGIFactory2_Release(f2); return; }
    DWORD cookie = 0xdeadbeef;
    HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusWindow(f2, w, WM_OCC, &cookie);
    CHECKF(hr == S_OK || hr == DXGI_E_NOT_CURRENTLY_AVAILABLE, "got %s", d3dh_hr_name(hr));
    t_note("RegisterOcclusionStatusWindow = %s cookie %#lx", d3dh_hr_name(hr), (unsigned long)cookie);
    if (SUCCEEDED(hr)) IDXGIFactory2_UnregisterOcclusionStatus(f2, cookie);
    IDXGIFactory2_Release(f2);
    d3dh_destroy_window(w);
}
TEST(dxgi_factory_occlusion_window_null_cookie) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HWND w = d3dh_create_hidden_window();
    HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusWindow(f2, w, WM_OCC, NULL);
    CHECKF(fh_inv_ok(hr), "got %s", d3dh_hr_name(hr));
    IDXGIFactory2_Release(f2);
    if (w) d3dh_destroy_window(w);
}
TEST(dxgi_factory_occlusion_window_null_hwnd) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    DWORD cookie = 0xdeadbeef;
    HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusWindow(f2, NULL, WM_OCC, &cookie);
    t_note("RegisterOcclusionStatusWindow(NULL hwnd) = %s", d3dh_hr_name(hr));
    CHECK(FAILED(hr) || cookie != 0xdeadbeef);
    if (SUCCEEDED(hr)) IDXGIFactory2_UnregisterOcclusionStatus(f2, cookie);
    IDXGIFactory2_Release(f2);
}
TEST(dxgi_factory_occlusion_window_cookies_distinct) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HWND w = d3dh_create_hidden_window();
    DWORD c[4] = { 0 }; int ok = 0;
    for (int i = 0; i < 4; i++) {
        HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusWindow(f2, w, WM_OCC + i, &c[i]);
        if (hr == S_OK) ok++; else break;
    }
    for (int i = 0; i < ok; i++) for (int j = i + 1; j < ok; j++) CHECKF(c[i] != c[j], "cookies %d and %d equal", i, j);
    for (int i = 0; i < ok; i++) IDXGIFactory2_UnregisterOcclusionStatus(f2, c[i]);
    if (!ok) t_skip("occlusion registration unavailable");
    IDXGIFactory2_Release(f2);
    if (w) d3dh_destroy_window(w);
}
TEST(dxgi_factory_occlusion_event_register_unregister) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD cookie = 0xdeadbeef;
    HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, &cookie);
    CHECKF(hr == S_OK || hr == DXGI_E_NOT_CURRENTLY_AVAILABLE, "got %s", d3dh_hr_name(hr));
    t_note("RegisterOcclusionStatusEvent = %s cookie %#lx", d3dh_hr_name(hr), (unsigned long)cookie);
    if (SUCCEEDED(hr)) IDXGIFactory2_UnregisterOcclusionStatus(f2, cookie);
    CloseHandle(ev);
    IDXGIFactory2_Release(f2);
}
TEST(dxgi_factory_occlusion_event_null_cookie) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, NULL);
    CHECKF(fh_inv_ok(hr), "got %s", d3dh_hr_name(hr));
    CloseHandle(ev);
    IDXGIFactory2_Release(f2);
}
TEST(dxgi_factory_occlusion_event_null_handle) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    DWORD cookie = 0xdeadbeef;
    HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusEvent(f2, NULL, &cookie);
    t_note("RegisterOcclusionStatusEvent(NULL event) = %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) IDXGIFactory2_UnregisterOcclusionStatus(f2, cookie);
    IDXGIFactory2_Release(f2);
}
TEST(dxgi_factory_occlusion_event_not_signalled_by_register) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD cookie = 0;
    if (FAILED(IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, &cookie))) { t_skip("unavailable"); CloseHandle(ev); IDXGIFactory2_Release(f2); return; }
    DWORD w = WaitForSingleObject(ev, 50);
    t_note("occlusion event after register: %s", w == WAIT_OBJECT_0 ? "signalled" : "not signalled");
    IDXGIFactory2_UnregisterOcclusionStatus(f2, cookie);
    CloseHandle(ev);
    IDXGIFactory2_Release(f2);
}
TEST(dxgi_factory_occlusion_unregister_then_release_factory) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD cookie = 0;
    if (SUCCEEDED(IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, &cookie))) IDXGIFactory2_UnregisterOcclusionStatus(f2, cookie);
    EXPECT_EQ(IDXGIFactory2_Release(f2), 0);
    CloseHandle(ev);
}
TEST(dxgi_factory_occlusion_registered_then_release_factory) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD cookie = 0;
    HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusEvent(f2, ev, &cookie);
    (void)hr;
    EXPECT_EQ(IDXGIFactory2_Release(f2), 0);   /* registrations do not keep the factory alive */
    CloseHandle(ev);
}
TEST(dxgi_factory_occlusion_window_destroyed_after_register) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HWND w = d3dh_create_hidden_window();
    DWORD cookie = 0;
    HRESULT hr = IDXGIFactory2_RegisterOcclusionStatusWindow(f2, w, WM_OCC, &cookie);
    if (w) d3dh_destroy_window(w);
    if (SUCCEEDED(hr)) IDXGIFactory2_UnregisterOcclusionStatus(f2, cookie);
    EXPECT_EQ(IDXGIFactory2_Release(f2), 0);
}

/* ---------------- adapters-changed events (IDXGIFactory7) ---------------- */
TEST(dxgi_factory_adapters_changed_register_unregister) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD cookie = 0xdeadbeef;
    HRESULT hr = IDXGIFactory7_RegisterAdaptersChangedEvent(f7, ev, &cookie);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        CHECK(cookie != 0xdeadbeef);
        t_note("adapters-changed cookie %#lx", (unsigned long)cookie);
        EXPECT_HR(IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, cookie), S_OK);
    }
    CloseHandle(ev);
    IDXGIFactory7_Release(f7);
}
TEST(dxgi_factory_adapters_changed_null_cookie) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    HRESULT hr = IDXGIFactory7_RegisterAdaptersChangedEvent(f7, ev, NULL);
    CHECKF(fh_inv_ok(hr), "got %s", d3dh_hr_name(hr));
    CloseHandle(ev);
    IDXGIFactory7_Release(f7);
}
TEST(dxgi_factory_adapters_changed_null_event) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    DWORD cookie = 0xdeadbeef;
    HRESULT hr = IDXGIFactory7_RegisterAdaptersChangedEvent(f7, NULL, &cookie);
    t_note("RegisterAdaptersChangedEvent(NULL event) = %s", d3dh_hr_name(hr));
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr)) IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, cookie);
    IDXGIFactory7_Release(f7);
}
TEST(dxgi_factory_adapters_changed_cookies_distinct) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    HANDLE ev[3]; DWORD c[3] = { 0 }; int ok = 0;
    for (int i = 0; i < 3; i++) {
        ev[i] = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (SUCCEEDED(IDXGIFactory7_RegisterAdaptersChangedEvent(f7, ev[i], &c[i]))) ok++;
    }
    EXPECT_EQ(ok, 3);
    for (int i = 0; i < ok; i++) for (int j = i + 1; j < ok; j++) CHECK(c[i] != c[j]);
    for (int i = 0; i < ok; i++) IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, c[i]);
    for (int i = 0; i < 3; i++) CloseHandle(ev[i]);
    IDXGIFactory7_Release(f7);
}
TEST(dxgi_factory_adapters_changed_double_unregister) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD c = 0;
    if (SUCCEEDED(IDXGIFactory7_RegisterAdaptersChangedEvent(f7, ev, &c))) {
        EXPECT_HR(IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, c), S_OK);
        HRESULT hr = IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, c);
        t_note("second UnregisterAdaptersChangedEvent = %s", d3dh_hr_name(hr));
    }
    CloseHandle(ev);
    IDXGIFactory7_Release(f7);
}
TEST(dxgi_factory_adapters_changed_unregister_bogus) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    HRESULT hr = IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, 0x7e57c001);
    t_note("UnregisterAdaptersChangedEvent(bogus cookie) = %s", d3dh_hr_name(hr));
    IDXGIFactory7_Release(f7);
}
TEST(dxgi_factory_adapters_changed_not_signalled_immediately) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD c = 0;
    if (SUCCEEDED(IDXGIFactory7_RegisterAdaptersChangedEvent(f7, ev, &c))) {
        DWORD w = WaitForSingleObject(ev, 20);
        t_note("adapters-changed event right after register: %s", w == WAIT_OBJECT_0 ? "signalled" : "not signalled");
        IDXGIFactory7_UnregisterAdaptersChangedEvent(f7, c);
    }
    CloseHandle(ev);
    IDXGIFactory7_Release(f7);
}
TEST(dxgi_factory_adapters_changed_released_with_factory) {
    NEED_FAC(f7, IDXGIFactory7, IID_IDXGIFactory7);
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    DWORD c = 0;
    IDXGIFactory7_RegisterAdaptersChangedEvent(f7, ev, &c);
    EXPECT_EQ(IDXGIFactory7_Release(f7), 0);
    CloseHandle(ev);
}

/* ---------------- CreateSoftwareAdapter / CreateSwapChain argument checks ---------------- */
TEST(dxgi_factory_create_software_adapter_null_module) {
    NEED_F1(f);
    IDXGIAdapter *a = POISON;
    HRESULT hr = IDXGIFactory1_CreateSoftwareAdapter(f, NULL, &a);
    CHECKF(hr == DXGI_E_INVALID_CALL || hr == E_INVALIDARG, "got %s", d3dh_hr_name(hr));
    CHECK(a == POISON || a == NULL);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_create_swapchain_null_device) {
    NEED_F1(f);
    DXGI_SWAP_CHAIN_DESC d; memset(&d, 0, sizeof(d));
    d.BufferDesc.Width = 64; d.BufferDesc.Height = 64; d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1; d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 1; d.Windowed = TRUE;
    IDXGISwapChain *sc = POISON;
    HRESULT hr = IDXGIFactory1_CreateSwapChain(f, NULL, &d, &sc);
    CHECKF(hr == DXGI_E_INVALID_CALL || hr == E_INVALIDARG, "got %s", d3dh_hr_name(hr));
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_create_swapchain_for_hwnd_null_device) {
    NEED_FAC(f2, IDXGIFactory2, IID_IDXGIFactory2);
    HWND w = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC1 d; memset(&d, 0, sizeof(d));
    d.Width = 64; d.Height = 64; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1 *sc = POISON;
    HRESULT hr = IDXGIFactory2_CreateSwapChainForHwnd(f2, NULL, w, &d, NULL, NULL, &sc);
    CHECKF(fh_inv_ok(hr), "got %s", d3dh_hr_name(hr));
    IDXGIFactory2_Release(f2);
    if (w) d3dh_destroy_window(w);
}
TEST(dxgi_factory_get_shared_resource_adapter_luid_bad_handle) {
    NEED_FAC(f4, IDXGIFactory3, IID_IDXGIFactory3);
    IDXGIFactory4 *g = fh_qi((IUnknown *)f4, &IID_IDXGIFactory4);
    if (g) {
        LUID l = { 0x1234, 0x5678 };
        HRESULT hr = IDXGIFactory4_GetSharedResourceAdapterLuid(g, (HANDLE)NULL, &l);
        CHECK(FAILED(hr));
        t_note("GetSharedResourceAdapterLuid(NULL handle) = %s", d3dh_hr_name(hr));
        IDXGIFactory4_Release(g);
    }
    IDXGIFactory3_Release(f4);
}
TEST(dxgi_factory_cf_multithread_create) {
    WINE_NOFAC();
    /* concurrent creation from several threads must succeed and leak nothing */
    extern int fh_thread_run(void);
    EXPECT_EQ(fh_thread_run(), 0);
}
static DWORD WINAPI fh_thread(LPVOID arg) {
    int bad = 0;
    for (int i = 0; i < 20; i++) {
        IDXGIFactory2 *f = NULL;
        if (FAILED(fh_make(&IID_IDXGIFactory2, (void **)&f))) { bad++; continue; }
        if (!IDXGIFactory2_IsCurrent(f)) bad++;
        IDXGIAdapter1 *a; IDXGIFactory2_EnumAdapters1(f, 0, &a) == S_OK ? (void)IDXGIAdapter1_Release(a) : (void)0;
        if (IDXGIFactory2_Release(f) != 0) bad++;
    }
    return bad;
}
int fh_thread_run(void) {
    HANDLE h[4]; int bad = 0;
    for (int i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, fh_thread, NULL, 0, NULL);
    for (int i = 0; i < 4; i++) {
        if (WaitForSingleObject(h[i], 20000) != WAIT_OBJECT_0) { bad += 100; continue; }
        DWORD c = 0; GetExitCodeThread(h[i], &c); bad += (int)c; CloseHandle(h[i]);
    }
    return bad;
}
