/* IDirectDraw / IDirectDraw2 / IDirectDraw4 / IDirectDraw7 object methods. */
#include "dl.h"

/* ---------------- QueryInterface ladder and reference counts ---------------- */
TEST(ddraw_legacy_obj_struct_sizes_x64) {
    /* the 64-bit layouts the DLL validates dwSize against */
    EXPECT_EQ(sizeof(DDSURFACEDESC), 120);
    EXPECT_EQ(sizeof(DDSURFACEDESC2), 136);
    EXPECT_EQ(sizeof(DDPIXELFORMAT), 32);
    EXPECT_EQ(sizeof(DDCAPS_DX3), 316);
    EXPECT_EQ(sizeof(DDCAPS_DX5), 364);
    EXPECT_EQ(sizeof(DDCAPS_DX6), 380);
    EXPECT_EQ(sizeof(DDCAPS_DX7), 380);
    EXPECT_EQ(sizeof(DDDEVICEIDENTIFIER), 1064);
    EXPECT_EQ(sizeof(DDDEVICEIDENTIFIER2), 1072);
}
TEST(ddraw_legacy_obj_qi_ladder_from_dd1) {
    DL_SKIP_IF_NO_DD1(dd);
    const IID *iids[] = { &IID_IUnknown, &IID_IDirectDraw, &IID_IDirectDraw2, &IID_IDirectDraw4, &IID_IDirectDraw7 };
    for (unsigned i = 0; i < 5; i++) {
        IUnknown *q = NULL;
        HRESULT hr = IDirectDraw_QueryInterface(dd, iids[i], (void **)&q);
        CHECKF(hr == S_OK && q, "QI %s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        if (q) IUnknown_Release(q);
    }
    EXPECT_RET(IDirectDraw_Release(dd), 0);
}
TEST(ddraw_legacy_obj_qi_ladder_from_dd7) {
    DL_SKIP_IF_NO_DD7(dd);
    const IID *iids[] = { &IID_IUnknown, &IID_IDirectDraw, &IID_IDirectDraw2, &IID_IDirectDraw4, &IID_IDirectDraw7 };
    for (unsigned i = 0; i < 5; i++) {
        IUnknown *q = NULL;
        HRESULT hr = IDirectDraw7_QueryInterface(dd, iids[i], (void **)&q);
        CHECKF(hr == S_OK && q, "QI %s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        if (q) IUnknown_Release(q);
    }
    EXPECT_RET(IDirectDraw7_Release(dd), 0);
}
TEST(ddraw_legacy_obj_qi_null_iid) {
    DL_SKIP_IF_NO_DD7(dd);
    void *p = (void *)0x1;
    if (t_is_wine()) { t_skip("emulator dereferences NULL IID"); IDirectDraw7_Release(dd); return; }
    EXPECT_HR(dd->lpVtbl->QueryInterface(dd, NULL, &p), E_INVALIDARG);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_obj_qi_unsupported_iids) {
    DL_SKIP_IF_NO_DD7(dd);
    const IID *iids[] = { &IID_IDirectDrawSurface, &IID_IDirectDrawSurface7, &IID_IDirectDrawClipper,
                          &IID_IDirectDrawPalette, &IID_IClassFactory, &IID_IDXGIFactory, &GUID_NULL };
    for (unsigned i = 0; i < 7; i++) {
        void *q = NULL;
        HRESULT hr = IDirectDraw7_QueryInterface(dd, iids[i], &q);
        CHECKF(hr == E_NOINTERFACE, "QI %s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && q) IUnknown_Release((IUnknown *)q);
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_obj_qi_new_interface_has_own_refcount) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw7 *dd7 = NULL;
    EXPECT_HR(IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw7, (void **)&dd7), S_OK);
    if (!dd7) { IDirectDraw_Release(dd1); return; }
    EXPECT_RET(IDirectDraw7_AddRef(dd7), 2);     /* a freshly created interface starts at 1 */
    EXPECT_RET(IDirectDraw7_Release(dd7), 1);
    EXPECT_RET(IDirectDraw_AddRef(dd1), 2);      /* the original interface keeps its own count */
    EXPECT_RET(IDirectDraw_Release(dd1), 1);
    EXPECT_RET(IDirectDraw_Release(dd1), 0);     /* releasing it does not destroy the object ... */
    DDCAPS caps; memset(&caps, 0, sizeof caps); caps.dwSize = sizeof caps;
    EXPECT_HR(IDirectDraw7_GetCaps(dd7, &caps, NULL), DD_OK);   /* ... while dd7 is alive */
    EXPECT_RET(IDirectDraw7_Release(dd7), 0);
}
TEST(ddraw_legacy_obj_qi_same_iid_adds_ref) {
    DL_SKIP_IF_NO_DD7(dd);
    IDirectDraw7 *q = NULL;
    EXPECT_HR(IDirectDraw7_QueryInterface(dd, &IID_IDirectDraw7, (void **)&q), S_OK);
    CHECK(q == dd);                                 /* same interface: same pointer */
    EXPECT_RET(IDirectDraw7_Release(dd), 1);
    EXPECT_RET(IDirectDraw7_Release(q), 0);
}
TEST(ddraw_legacy_obj_qi_repeated_iid_same_pointer) {
    DL_SKIP_IF_NO_DD1(dd);
    IDirectDraw4 *a = NULL, *b = NULL;
    EXPECT_HR(IDirectDraw_QueryInterface(dd, &IID_IDirectDraw4, (void **)&a), S_OK);
    EXPECT_HR(IDirectDraw_QueryInterface(dd, &IID_IDirectDraw4, (void **)&b), S_OK);
    if (a && b) {
        t_note("second QI(IDirectDraw4): %s pointer", a == b ? "same" : "different");
        if (a == b) { EXPECT_RET(IDirectDraw4_AddRef(a), 3); IDirectDraw4_Release(a); }
    }
    if (a) IDirectDraw4_Release(a);
    if (b) IDirectDraw4_Release(b);
    IDirectDraw_Release(dd);
}
TEST(ddraw_legacy_obj_all_versions_share_state) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw2 *dd2 = NULL; IDirectDraw4 *dd4 = NULL; IDirectDraw7 *dd7 = NULL;
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw2, (void **)&dd2);
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd4);
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw7, (void **)&dd7);
    CHECK(dd2 && dd4 && dd7);
    if (dd2 && dd4 && dd7) {
        HWND w = dl_window();
        EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd7, w, DDSCL_NORMAL), DD_OK);
        DDCAPS c1, c2, c4, c7; memset(&c1, 0, sizeof c1);
        c1.dwSize = sizeof c1; c2 = c4 = c7 = c1;
        EXPECT_HR(IDirectDraw_GetCaps(dd1, &c1, NULL), DD_OK);
        EXPECT_HR(IDirectDraw2_GetCaps(dd2, &c2, NULL), DD_OK);
        EXPECT_HR(IDirectDraw4_GetCaps(dd4, &c4, NULL), DD_OK);
        EXPECT_HR(IDirectDraw7_GetCaps(dd7, &c7, NULL), DD_OK);
        EXPECT_EQ(c1.dwCaps, c7.dwCaps);
        EXPECT_EQ(c2.dwCaps, c7.dwCaps);
        EXPECT_EQ(c4.dwCaps, c7.dwCaps);
        EXPECT_EQ(c1.dwCKeyCaps, c7.dwCKeyCaps);
        dl_window_free(w);
    }
    if (dd2) IDirectDraw2_Release(dd2);
    if (dd4) IDirectDraw4_Release(dd4);
    if (dd7) IDirectDraw7_Release(dd7);
    IDirectDraw_Release(dd1);
}
TEST(ddraw_legacy_obj_initialize_already_initialized) {
    DL_SKIP_IF_NO_DD1(dd);
    EXPECT_HR(IDirectDraw_Initialize(dd, NULL), DDERR_ALREADYINITIALIZED);
    IDirectDraw_Release(dd);
}
TEST(ddraw_legacy_obj_initialize_already_initialized_dd7) {
    DL_SKIP_IF_NO_DD7(dd);
    EXPECT_HR(IDirectDraw7_Initialize(dd, NULL), DDERR_ALREADYINITIALIZED);
    IDirectDraw7_Release(dd);
}

/* ---------------- SetCooperativeLevel flag matrix ---------------- */
struct coop_case { DWORD flags; const char *name; };
static const struct coop_case k_coop_bad[] = {
    { 0,                                         "0" },
    { 0x10000,                                   "bit16 only" },
    { DDSCL_NORMAL | 0x10000,                    "NORMAL|bit16" },
    { DDSCL_NORMAL | 0x80000000,                 "NORMAL|bit31" },
    { DDSCL_FULLSCREEN,                          "FULLSCREEN alone" },
    { DDSCL_EXCLUSIVE,                           "EXCLUSIVE alone" },
    { DDSCL_ALLOWREBOOT,                         "ALLOWREBOOT alone" },
    { DDSCL_NOWINDOWCHANGES,                     "NOWINDOWCHANGES alone" },
    { DDSCL_ALLOWMODEX,                          "ALLOWMODEX alone" },
    { DDSCL_MULTITHREADED,                       "MULTITHREADED alone" },
    { DDSCL_NORMAL | DDSCL_EXCLUSIVE,            "NORMAL|EXCLUSIVE" },
    { DDSCL_NORMAL | DDSCL_ALLOWMODEX,           "NORMAL|ALLOWMODEX" },
    { DDSCL_NORMAL | DDSCL_FPUSETUP | DDSCL_FPUPRESERVE, "NORMAL|FPUSETUP|FPUPRESERVE" },
    { DDSCL_SETFOCUSWINDOW | DDSCL_NORMAL,       "SETFOCUSWINDOW|NORMAL" },
    { DDSCL_SETFOCUSWINDOW | DDSCL_SETDEVICEWINDOW, "SETFOCUSWINDOW|SETDEVICEWINDOW" },
    { DDSCL_NORMAL | DDSCL_CREATEDEVICEWINDOW,   "NORMAL|CREATEDEVICEWINDOW" },
};
static void coop_bad_table(IDirectDraw7 *dd, HWND w) {
    for (unsigned i = 0; i < sizeof k_coop_bad / sizeof *k_coop_bad; i++) {
        HRESULT hr = IDirectDraw7_SetCooperativeLevel(dd, w, k_coop_bad[i].flags);
        CHECKF(hr == DDERR_INVALIDPARAMS, "%s (0x%lx) -> %s", k_coop_bad[i].name, k_coop_bad[i].flags, d3dh_hr_name(hr));
    }
}
TEST(ddraw_legacy_coop_invalid_flag_matrix_dd7) {
    DL_SKIP_IF_NO_DD7(dd);
    HWND w = dl_window();
    coop_bad_table(dd, w);
    dl_window_free(w);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_coop_invalid_flag_matrix_dd1) {
    DL_SKIP_IF_NO_DD1(dd);
    HWND w = dl_window();
    for (unsigned i = 0; i < sizeof k_coop_bad / sizeof *k_coop_bad; i++) {
        HRESULT hr = IDirectDraw_SetCooperativeLevel(dd, w, k_coop_bad[i].flags);
        CHECKF(hr == DDERR_INVALIDPARAMS, "%s (0x%lx) -> %s", k_coop_bad[i].name, k_coop_bad[i].flags, d3dh_hr_name(hr));
    }
    dl_window_free(w);
    IDirectDraw_Release(dd);
}
TEST(ddraw_legacy_coop_invalid_flag_matrix_dd4) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw4 *dd = NULL;
    if (FAILED(IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd)) || !dd) { IDirectDraw_Release(dd1); return; }
    HWND w = dl_window();
    for (unsigned i = 0; i < sizeof k_coop_bad / sizeof *k_coop_bad; i++) {
        HRESULT hr = IDirectDraw4_SetCooperativeLevel(dd, w, k_coop_bad[i].flags);
        CHECKF(hr == DDERR_INVALIDPARAMS, "%s (0x%lx) -> %s", k_coop_bad[i].name, k_coop_bad[i].flags, d3dh_hr_name(hr));
    }
    dl_window_free(w);
    IDirectDraw4_Release(dd);
    IDirectDraw_Release(dd1);
}
TEST(ddraw_legacy_coop_invalid_flags_with_null_window) {
    DL_SKIP_IF_NO_DD7(dd);
    /* exclusive + fullscreen needs a window */
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, NULL, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN), DDERR_INVALIDPARAMS);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, NULL, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWMODEX), DDERR_INVALIDPARAMS);
    coop_bad_table(dd, NULL);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_coop_window_handle_must_be_window) {
    DL_SKIP_IF_NO_DD7(dd);
    HWND bogus[] = { (HWND)(ULONG_PTR)0x1, (HWND)(ULONG_PTR)0xfffe, (HWND)(ULONG_PTR)0x7fffffff };
    for (unsigned i = 0; i < 3; i++) {
        if (IsWindow(bogus[i])) continue;
        HRESULT hr = IDirectDraw7_SetCooperativeLevel(dd, bogus[i], DDSCL_NORMAL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "hwnd %p -> %s", (void *)bogus[i], d3dh_hr_name(hr));
    }
    HWND w = dl_window();
    DestroyWindow(w);                               /* a destroyed handle is no longer a window */
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL), DDERR_INVALIDPARAMS);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_coop_exclusive_rejects_child_window) {
    DL_SKIP_IF_NO_DD7(dd);
    HWND parent = dl_window();
    HWND child = CreateWindowExA(0, "STATIC", "c", WS_CHILD, 0, 0, 8, 8, parent, NULL, GetModuleHandleA(NULL), NULL);
    if (!child) { t_skip("cannot create child window"); dl_window_free(parent); IDirectDraw7_Release(dd); return; }
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, child, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN), DDERR_INVALIDPARAMS);
    DestroyWindow(child);
    dl_window_free(parent);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_coop_setdevicewindow_needs_focus_window) {
    DL_SKIP_IF_NO_DD7(dd);
    HWND w = dl_window();
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_SETDEVICEWINDOW | DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN), DDERR_NOFOCUSWINDOW);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_SETDEVICEWINDOW | DDSCL_SETFOCUSWINDOW | DDSCL_NORMAL), DDERR_INVALIDPARAMS);
    dl_window_free(w);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_coop_normal_succeeds_repeatedly) {
    DL_SKIP_IF_NO_DD7(dd);
    HWND w = dl_window();
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL), DD_OK);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL), DD_OK);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL | DDSCL_MULTITHREADED), DD_OK);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL | DDSCL_FPUPRESERVE), DD_OK);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL | DDSCL_FPUSETUP), DD_OK);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL | DDSCL_NOWINDOWCHANGES), DD_OK);
    dl_window_free(w);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_coop_normal_two_windows) {
    DL_SKIP_IF_NO_DD7(dd);
    HWND a = dl_window(), b = d3dh_create_hidden_window_sized(32, 32);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, a, DDSCL_NORMAL), DD_OK);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, b, DDSCL_NORMAL), DD_OK);   /* windowed mode may move between windows */
    dl_window_free(a); dl_window_free(b);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_coop_normal_dd1_dd2_dd4) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw2 *dd2 = NULL; IDirectDraw4 *dd4 = NULL;
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw2, (void **)&dd2);
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd4);
    HWND w = dl_window();
    EXPECT_HR(IDirectDraw_SetCooperativeLevel(dd1, w, DDSCL_NORMAL), DD_OK);
    if (dd2) EXPECT_HR(IDirectDraw2_SetCooperativeLevel(dd2, w, DDSCL_NORMAL), DD_OK);
    if (dd4) EXPECT_HR(IDirectDraw4_SetCooperativeLevel(dd4, w, DDSCL_NORMAL), DD_OK);
    dl_window_free(w);
    if (dd2) IDirectDraw2_Release(dd2);
    if (dd4) IDirectDraw4_Release(dd4);
    IDirectDraw_Release(dd1);
}
TEST(ddraw_legacy_coop_test_cooperative_level) {
    DL_SKIP_IF_NO_DD7(dd);
    HWND w = dl_window();
    HRESULT before = IDirectDraw7_TestCooperativeLevel(dd);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL), DD_OK);
    HRESULT after = IDirectDraw7_TestCooperativeLevel(dd);
    /* S_OK, DDERR_WRONGMODE or DDERR_EXCLUSIVEMODEALREADYSET depending on the desktop state */
    CHECKF(after == DD_OK || after == DDERR_WRONGMODE || after == DDERR_EXCLUSIVEMODEALREADYSET, "after: %s", d3dh_hr_name(after));
    CHECKF(before == DD_OK || before == DDERR_WRONGMODE || before == DDERR_EXCLUSIVEMODEALREADYSET || before == DDERR_NOEXCLUSIVEMODE,
           "before: %s", d3dh_hr_name(before));
    t_note("TestCooperativeLevel before=%s after=%s", d3dh_hr_name(before), d3dh_hr_name(after));
    dl_window_free(w);
    IDirectDraw7_Release(dd);
}

/* ---------------- GetCaps ---------------- */
TEST(ddraw_legacy_caps_both_null) {
    DL_SKIP_IF_NO_DD7(dd);
    EXPECT_HR(IDirectDraw7_GetCaps(dd, NULL, NULL), DDERR_INVALIDPARAMS);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_caps_bad_dwsize) {
    DL_SKIP_IF_NO_DD7(dd);
    DWORD bad[] = { 0, 4, 8, 100, 171, 173, 315, 317, 363, 365, 379, 381, 384, 0x1000 };
    BYTE buf[0x1100];
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        memset(buf, 0xA5, sizeof buf);
        ((DDCAPS *)buf)->dwSize = bad[i];
        HRESULT hr = IDirectDraw7_GetCaps(dd, (DDCAPS *)buf, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "HAL dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
        CHECKF(buf[sizeof buf - 1] == 0xA5 && buf[16] == 0xA5, "buffer touched for size %lu", bad[i]);
        memset(buf, 0xA5, sizeof buf);
        ((DDCAPS *)buf)->dwSize = bad[i];
        hr = IDirectDraw7_GetCaps(dd, NULL, (DDCAPS *)buf);
        CHECKF(hr == DDERR_INVALIDPARAMS, "HEL dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_caps_accepted_sizes_write_exact_prefix) {
    DL_SKIP_IF_NO_DD7(dd);
    DWORD ok[] = { 172, 316, 364, 380 };
    for (unsigned i = 0; i < 4; i++) {
        BYTE buf[512], ref[512];
        memset(buf, 0xA5, sizeof buf); memset(ref, 0xA5, sizeof ref);
        ((DDCAPS *)buf)->dwSize = ok[i];
        HRESULT hr = IDirectDraw7_GetCaps(dd, (DDCAPS *)buf, NULL);
        CHECKF(hr == DD_OK, "size %lu -> %s", ok[i], d3dh_hr_name(hr));
        EXPECT_EQ(((DDCAPS *)buf)->dwSize, ok[i]);   /* the caller's size is preserved */
        CHECKF(!memcmp(buf + ok[i], ref + ok[i], sizeof buf - ok[i]), "bytes beyond dwSize=%lu were modified", ok[i]);
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_caps_hel_size_independent) {
    DL_SKIP_IF_NO_DD7(dd);
    DWORD ok[] = { 172, 316, 364, 380 };
    for (unsigned i = 0; i < 4; i++) {
        BYTE buf[512], ref[512];
        memset(buf, 0xA5, sizeof buf); memset(ref, 0xA5, sizeof ref);
        ((DDCAPS *)buf)->dwSize = ok[i];
        HRESULT hr = IDirectDraw7_GetCaps(dd, NULL, (DDCAPS *)buf);
        CHECKF(hr == DD_OK, "HEL size %lu -> %s", ok[i], d3dh_hr_name(hr));
        EXPECT_EQ(((DDCAPS *)buf)->dwSize, ok[i]);
        CHECKF(!memcmp(buf + ok[i], ref + ok[i], sizeof buf - ok[i]), "HEL bytes beyond dwSize=%lu modified", ok[i]);
        EXPECT_EQ(((DDCAPS *)buf)->dwVidMemFree, 0);  /* the emulation layer never reports free video memory */
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_caps_bad_hel_size_with_good_hal) {
    DL_SKIP_IF_NO_DD7(dd);
    DDCAPS hal, hel; memset(&hal, 0, sizeof hal); memset(&hel, 0, sizeof hel);
    hal.dwSize = sizeof hal; hel.dwSize = 100;
    EXPECT_HR(IDirectDraw7_GetCaps(dd, &hal, &hel), DDERR_INVALIDPARAMS);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_caps_hal_and_hel_together) {
    DL_SKIP_IF_NO_DD7(dd);
    DDCAPS hal, hel; memset(&hal, 0, sizeof hal); memset(&hel, 0, sizeof hel);
    hal.dwSize = sizeof hal; hel.dwSize = sizeof hel;
    EXPECT_HR(IDirectDraw7_GetCaps(dd, &hal, &hel), DD_OK);
    EXPECT_EQ(hal.dwSize, sizeof hal);
    EXPECT_EQ(hel.dwSize, sizeof hel);
    EXPECT_EQ(hel.dwVidMemFree, 0);
    CHECK(hal.dwVidMemFree <= hal.dwVidMemTotal);
    CHECK((hal.dwCaps & DDCAPS_OVERLAY) == 0);       /* overlays are masked unless the app-compat shim enables them */
    t_note("HAL: caps=0x%lx caps2=0x%lx vidmem total=%lu free=%lu; HEL: caps=0x%lx caps2=0x%lx",
           hal.dwCaps, hal.dwCaps2, hal.dwVidMemTotal, hal.dwVidMemFree, hel.dwCaps, hel.dwCaps2);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_caps_stable_between_calls) {
    DL_SKIP_IF_NO_DD7(dd);
    DDCAPS a, b; memset(&a, 0, sizeof a); a.dwSize = sizeof a; b = a;
    EXPECT_HR(IDirectDraw7_GetCaps(dd, &a, NULL), DD_OK);
    EXPECT_HR(IDirectDraw7_GetCaps(dd, &b, NULL), DD_OK);
    EXPECT_EQ(a.dwCaps, b.dwCaps);
    EXPECT_EQ(a.dwCaps2, b.dwCaps2);
    EXPECT_EQ(a.dwCKeyCaps, b.dwCKeyCaps);
    EXPECT_EQ(a.dwFXCaps, b.dwFXCaps);
    EXPECT_EQ(a.dwMaxVisibleOverlays, b.dwMaxVisibleOverlays);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_caps_dd1_dd2_dd4_same_validation) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw2 *dd2 = NULL; IDirectDraw4 *dd4 = NULL;
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw2, (void **)&dd2);
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd4);
    DDCAPS c; memset(&c, 0, sizeof c); c.dwSize = 100;
    EXPECT_HR(IDirectDraw_GetCaps(dd1, &c, NULL), DDERR_INVALIDPARAMS);
    EXPECT_HR(IDirectDraw_GetCaps(dd1, NULL, NULL), DDERR_INVALIDPARAMS);
    if (dd2) { EXPECT_HR(IDirectDraw2_GetCaps(dd2, &c, NULL), DDERR_INVALIDPARAMS); IDirectDraw2_Release(dd2); }
    if (dd4) { EXPECT_HR(IDirectDraw4_GetCaps(dd4, &c, NULL), DDERR_INVALIDPARAMS); IDirectDraw4_Release(dd4); }
    c.dwSize = sizeof(DDCAPS_DX3);
    EXPECT_HR(IDirectDraw_GetCaps(dd1, &c, NULL), DD_OK);
    IDirectDraw_Release(dd1);
}

/* ---------------- display modes ---------------- */
TEST(ddraw_legacy_mode_getdisplaymode_dd7) {
    DL_SKIP_IF_NO_DD7(dd);
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    EXPECT_HR(IDirectDraw7_GetDisplayMode(dd, &d), DD_OK);
    EXPECT_EQ(d.dwSize, sizeof d);
    EXPECT_EQ(d.dwFlags, DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_REFRESHRATE);
    CHECK(d.dwWidth > 0 && d.dwHeight > 0);
    EXPECT_EQ(d.ddpfPixelFormat.dwSize, sizeof(DDPIXELFORMAT));
    DWORD bpp = d.ddpfPixelFormat.dwRGBBitCount;
    CHECKF(bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32, "bpp %lu", bpp);
    EXPECT_EQ(d.lPitch, d.dwWidth * bpp / 8);
    if (bpp == 8) CHECK(d.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8);
    else CHECK(d.ddpfPixelFormat.dwFlags & DDPF_RGB);
    if (bpp == 32) { EXPECT_EQ(d.ddpfPixelFormat.dwRBitMask, 0xff0000); EXPECT_EQ(d.ddpfPixelFormat.dwGBitMask, 0xff00); EXPECT_EQ(d.ddpfPixelFormat.dwBBitMask, 0xff); }
    t_note("display mode %lux%lu %lubpp %luHz", d.dwWidth, d.dwHeight, bpp, d.dwRefreshRate);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_getdisplaymode_bad_args) {
    DL_SKIP_IF_NO_DD7(dd);
    EXPECT_HR(IDirectDraw7_GetDisplayMode(dd, NULL), DDERR_INVALIDPARAMS);
    DWORD bad[] = { 0, 4, 108, 112, 124, 135, 137, 0x1000 };
    BYTE buf[0x1100];
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        memset(buf, 0xA5, sizeof buf);
        ((DDSURFACEDESC2 *)buf)->dwSize = bad[i];
        HRESULT hr = IDirectDraw7_GetDisplayMode(dd, (DDSURFACEDESC2 *)buf);
        CHECKF(hr == DDERR_INVALIDPARAMS, "dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
        CHECKF(buf[200] == 0xA5, "buffer touched for dwSize %lu", bad[i]);
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_getdisplaymode_dd1_dd2) {
    DL_SKIP_IF_NO_DD1(dd);
    DDSURFACEDESC d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    EXPECT_HR(IDirectDraw_GetDisplayMode(dd, &d), DD_OK);
    EXPECT_EQ(d.dwSize, sizeof d);
    EXPECT_EQ(d.dwFlags, DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_REFRESHRATE);
    EXPECT_EQ(d.ddpfPixelFormat.dwSize, sizeof(DDPIXELFORMAT));
    IDirectDraw2 *dd2 = NULL;
    if (SUCCEEDED(IDirectDraw_QueryInterface(dd, &IID_IDirectDraw2, (void **)&dd2)) && dd2) {
        DDSURFACEDESC e; memset(&e, 0, sizeof e); e.dwSize = sizeof e;
        EXPECT_HR(IDirectDraw2_GetDisplayMode(dd2, &e), DD_OK);
        EXPECT_EQ(e.dwWidth, d.dwWidth);
        EXPECT_EQ(e.dwHeight, d.dwHeight);
        EXPECT_EQ(e.ddpfPixelFormat.dwRGBBitCount, d.ddpfPixelFormat.dwRGBBitCount);
        d.dwSize = 108;
        EXPECT_HR(IDirectDraw_GetDisplayMode(dd, &d), DDERR_INVALIDPARAMS);
        IDirectDraw2_Release(dd2);
    }
    IDirectDraw_Release(dd);
}
TEST(ddraw_legacy_mode_getdisplaymode_dd4_matches_dd7) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw4 *dd4 = NULL; IDirectDraw7 *dd7 = NULL;
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd4);
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw7, (void **)&dd7);
    if (dd4 && dd7) {
        DDSURFACEDESC2 a, b; memset(&a, 0, sizeof a); a.dwSize = sizeof a; b = a;
        EXPECT_HR(IDirectDraw4_GetDisplayMode(dd4, &a), DD_OK);
        EXPECT_HR(IDirectDraw7_GetDisplayMode(dd7, &b), DD_OK);
        EXPECT_EQ(a.dwWidth, b.dwWidth);
        EXPECT_EQ(a.dwHeight, b.dwHeight);
        EXPECT_EQ(a.lPitch, b.lPitch);
        EXPECT_EQ(a.ddpfPixelFormat.dwRGBBitCount, b.ddpfPixelFormat.dwRGBBitCount);
    }
    if (dd4) IDirectDraw4_Release(dd4);
    if (dd7) IDirectDraw7_Release(dd7);
    IDirectDraw_Release(dd1);
}
TEST(ddraw_legacy_mode_setdisplaymode_bad_flags) {
    if (t_is_wine()) { t_skip("emulator accepts the flags and really switches the mode"); return; }
    DL_SKIP_IF_NO_DD7(dd);
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    if (FAILED(IDirectDraw7_GetDisplayMode(dd, &d))) { IDirectDraw7_Release(dd); return; }
    /* flags other than 0 / DDSDM_STANDARDVGAMODE are rejected before anything touches the display */
    DWORD bad[] = { 2, 4, 0x10, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < 5; i++) {
        HRESULT hr = IDirectDraw7_SetDisplayMode(dd, d.dwWidth, d.dwHeight, d.ddpfPixelFormat.dwRGBBitCount, d.dwRefreshRate, bad[i]);
        CHECKF(hr == DDERR_INVALIDPARAMS, "flags 0x%lx -> %s", bad[i], d3dh_hr_name(hr));
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_setdisplaymode_bad_flags_dd4) {
    if (t_is_wine()) { t_skip("emulator accepts the flags and really switches the mode"); return; }
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw4 *dd = NULL;
    if (FAILED(IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd)) || !dd) { IDirectDraw_Release(dd1); return; }
    EXPECT_HR(IDirectDraw4_SetDisplayMode(dd, 640, 480, 32, 0, 2), DDERR_INVALIDPARAMS);
    EXPECT_HR(IDirectDraw4_SetDisplayMode(dd, 640, 480, 32, 0, 0xffffffff), DDERR_INVALIDPARAMS);
    IDirectDraw4_Release(dd);
    IDirectDraw_Release(dd1);
}

struct mode_acc { int n; int bad_size; int bad_flags; int bad_refresh; int dup; int cancel_after; DWORD want_flags; DWORD w[512], h[512], bpp[512], hz[512]; int found_cur; DWORD cw, ch, cbpp; DWORD dwsize_expect; };
static HRESULT WINAPI mode_cb7(LPDDSURFACEDESC2 d, LPVOID ctx) {
    struct mode_acc *m = ctx;
    if (d->dwSize != m->dwsize_expect) m->bad_size++;
    if ((d->dwFlags & (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)) != (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)) m->bad_flags++;
    if (m->n < 512) {
        for (int i = 0; i < m->n; i++)
            if (m->w[i] == d->dwWidth && m->h[i] == d->dwHeight && m->bpp[i] == d->ddpfPixelFormat.dwRGBBitCount && m->hz[i] == d->dwRefreshRate) m->dup++;
        m->w[m->n] = d->dwWidth; m->h[m->n] = d->dwHeight; m->bpp[m->n] = d->ddpfPixelFormat.dwRGBBitCount; m->hz[m->n] = d->dwRefreshRate;
    }
    if (d->dwWidth == m->cw && d->dwHeight == m->ch && d->ddpfPixelFormat.dwRGBBitCount == m->cbpp) m->found_cur = 1;
    if (m->want_flags && !(d->dwFlags & m->want_flags)) m->bad_refresh++;
    m->n++;
    return m->cancel_after && m->n >= m->cancel_after ? DDENUMRET_CANCEL : DDENUMRET_OK;
}
static int cur_mode(IDirectDraw7 *dd, struct mode_acc *m) {
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    if (FAILED(IDirectDraw7_GetDisplayMode(dd, &d))) return 0;
    m->cw = d.dwWidth; m->ch = d.dwHeight; m->cbpp = d.ddpfPixelFormat.dwRGBBitCount;
    return 1;
}
TEST(ddraw_legacy_mode_enum_basic) {
    DL_SKIP_IF_NO_DD7(dd);
    struct mode_acc *m = calloc(1, sizeof *m);
    m->dwsize_expect = sizeof(DDSURFACEDESC2);
    cur_mode(dd, m);
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, NULL, m, mode_cb7), DD_OK);
    CHECK(m->n >= 1);
    EXPECT_EQ(m->bad_size, 0);
    EXPECT_EQ(m->bad_flags, 0);
    EXPECT_EQ(m->dup, 0);                            /* each (width,height,bpp) is reported once without DDEDM_REFRESHRATES */
    CHECK(m->found_cur);                             /* the current mode is always enumerated */
    for (int i = 0; i < m->n && i < 512; i++) CHECKF(m->bpp[i] >= 8, "mode %d bpp %lu", i, m->bpp[i]);
    free(m);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_enum_matches_display_settings) {
    DL_SKIP_IF_NO_DD7(dd);
    struct mode_acc *m = calloc(1, sizeof *m);
    m->dwsize_expect = sizeof(DDSURFACEDESC2);
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, NULL, m, mode_cb7), DD_OK);
    /* expected: distinct (w,h,bpp) among EnumDisplaySettings modes with >= 8 bpp, 15bpp counted as 16 */
    DWORD ew[1024], eh[1024], eb[1024]; int en = 0;
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    for (DWORD i = 0; EnumDisplaySettingsA(NULL, i, &dm) && i < 4000; i++) {
        if (dm.dmBitsPerPel < 8) continue;
        DWORD b = dm.dmBitsPerPel == 15 ? 16 : dm.dmBitsPerPel;
        int seen = 0;
        for (int k = 0; k < en; k++) if (ew[k] == dm.dmPelsWidth && eh[k] == dm.dmPelsHeight && eb[k] == b) seen = 1;
        if (!seen && en < 1024) { ew[en] = dm.dmPelsWidth; eh[en] = dm.dmPelsHeight; eb[en] = b; en++; }
    }
    if (!t_is_wine()) EXPECT_EQ(m->n, en);
    else t_note("enumerated %d, EnumDisplaySettings distinct %d", m->n, en);
    free(m);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_enum_refreshrates) {
    DL_SKIP_IF_NO_DD7(dd);
    struct mode_acc *a = calloc(1, sizeof *a), *b = calloc(1, sizeof *b);
    a->dwsize_expect = b->dwsize_expect = sizeof(DDSURFACEDESC2);
    b->want_flags = DDSD_REFRESHRATE;
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, NULL, a, mode_cb7), DD_OK);
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, DDEDM_REFRESHRATES, NULL, b, mode_cb7), DD_OK);
    CHECK(b->n >= a->n);                             /* with rates, modes are only merged when the rate matches too */
    EXPECT_EQ(b->bad_refresh, 0);
    EXPECT_EQ(b->dup, 0);
    EXPECT_EQ(b->bad_flags, 0);
    free(a); free(b);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_enum_cancel) {
    DL_SKIP_IF_NO_DD7(dd);
    struct mode_acc *m = calloc(1, sizeof *m);
    m->dwsize_expect = sizeof(DDSURFACEDESC2); m->cancel_after = 1;
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, NULL, m, mode_cb7), DD_OK);
    EXPECT_EQ(m->n, 1);
    free(m);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_enum_filter_current) {
    DL_SKIP_IF_NO_DD7(dd);
    struct mode_acc *m = calloc(1, sizeof *m);
    m->dwsize_expect = sizeof(DDSURFACEDESC2);
    if (!cur_mode(dd, m)) { free(m); IDirectDraw7_Release(dd); return; }
    DDSURFACEDESC2 f; memset(&f, 0, sizeof f); f.dwSize = sizeof f;
    f.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    f.dwWidth = m->cw; f.dwHeight = m->ch; f.ddpfPixelFormat.dwRGBBitCount = m->cbpp;
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, &f, m, mode_cb7), DD_OK);
    EXPECT_EQ(m->n, 1);                              /* exactly the current mode matches the width/height/bpp filter */
    for (int i = 0; i < m->n; i++) { EXPECT_EQ(m->w[i], m->cw); EXPECT_EQ(m->h[i], m->ch); EXPECT_EQ(m->bpp[i], m->cbpp); }
    free(m);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_enum_filter_no_match) {
    DL_SKIP_IF_NO_DD7(dd);
    struct mode_acc *m = calloc(1, sizeof *m);
    m->dwsize_expect = sizeof(DDSURFACEDESC2);
    DDSURFACEDESC2 f; memset(&f, 0, sizeof f); f.dwSize = sizeof f;
    f.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
    f.dwWidth = 7; f.dwHeight = 3;
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, &f, m, mode_cb7), DD_OK);
    EXPECT_EQ(m->n, 0);
    free(m);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_mode_enum_bad_args) {
    DL_SKIP_IF_NO_DD7(dd);
    struct mode_acc *m = calloc(1, sizeof *m);
    m->dwsize_expect = sizeof(DDSURFACEDESC2);
    if (!t_is_wine()) EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, NULL, m, NULL), DDERR_INVALIDPARAMS);
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 4, NULL, m, mode_cb7), DDERR_INVALIDPARAMS);
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0x80000000, NULL, m, mode_cb7), DDERR_INVALIDPARAMS);
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0xfffffffc, NULL, m, mode_cb7), DDERR_INVALIDPARAMS);
    DDSURFACEDESC2 f; memset(&f, 0, sizeof f); f.dwSize = 124;
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, &f, m, mode_cb7), DDERR_INVALIDPARAMS);
    f.dwSize = 0;
    EXPECT_HR(IDirectDraw7_EnumDisplayModes(dd, 0, &f, m, mode_cb7), DDERR_INVALIDPARAMS);
    EXPECT_EQ(m->n, 0);
    free(m);
    IDirectDraw7_Release(dd);
}
static HRESULT WINAPI mode_cb1(LPDDSURFACEDESC d, LPVOID ctx) {
    struct mode_acc *m = ctx;
    if (d->dwSize != sizeof(DDSURFACEDESC)) m->bad_size++;
    if ((d->dwFlags & (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)) != (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)) m->bad_flags++;
    m->n++;
    return DDENUMRET_OK;
}
TEST(ddraw_legacy_mode_enum_dd1_descriptor_size) {
    DL_SKIP_IF_NO_DD1(dd);
    struct mode_acc *m = calloc(1, sizeof *m);
    EXPECT_HR(IDirectDraw_EnumDisplayModes(dd, 0, NULL, m, mode_cb1), DD_OK);
    CHECK(m->n >= 1);
    EXPECT_EQ(m->bad_size, 0);                       /* DDSURFACEDESC consumers get 120-byte descriptors on x64 */
    EXPECT_EQ(m->bad_flags, 0);
    free(m);
    IDirectDraw_Release(dd);
}

/* ---------------- misc IDirectDraw methods ---------------- */
TEST(ddraw_legacy_misc_fourcc_protocol) {
    DL_SKIP_IF_NO_DD7(dd);
    DWORD n = 0xdeadbeef;
    EXPECT_HR(IDirectDraw7_GetFourCCCodes(dd, &n, NULL), DD_OK);   /* NULL buffer: count query */
    CHECK(n != 0xdeadbeef);
    DWORD total = n;
    DWORD codes[256]; memset(codes, 0xCD, sizeof codes);
    if (total > 200) total = 200;
    DWORD m = total;
    EXPECT_HR(IDirectDraw7_GetFourCCCodes(dd, &m, codes), DD_OK);
    EXPECT_EQ(m, total);
    for (DWORD i = m; i < 256; i++) if (codes[i] != 0xCDCDCDCD) { CHECKF(0, "code buffer overrun at %lu", i); break; }
    if (total > 1) {                                  /* truncation: *n is lowered to the number written */
        DWORD few = 1; DWORD one[2] = { 0xCDCDCDCD, 0xCDCDCDCD };
        EXPECT_HR(IDirectDraw7_GetFourCCCodes(dd, &few, one), DD_OK);
        EXPECT_EQ(few, 1);
        EXPECT_EQ(one[1], 0xCDCDCDCD);
        EXPECT_EQ(one[0], codes[0]);
    }
    DWORD big = total + 50;
    memset(codes, 0xCD, sizeof codes);
    if (big <= 250) {
        EXPECT_HR(IDirectDraw7_GetFourCCCodes(dd, &big, codes), DD_OK);
        EXPECT_EQ(big, total);                        /* *n is lowered to the real count */
    }
    t_note("FourCC code count: %lu", n);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_fourcc_zero_capacity) {
    DL_SKIP_IF_NO_DD7(dd);
    DWORD n = 0; DWORD c = 0xCDCDCDCD;
    EXPECT_HR(IDirectDraw7_GetFourCCCodes(dd, &n, &c), DD_OK);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(c, 0xCDCDCDCD);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_monitor_frequency) {
    DL_SKIP_IF_NO_DD7(dd);
    DWORD hz = 0xdeadbeef;
    HRESULT hr = IDirectDraw7_GetMonitorFrequency(dd, &hz);
    CHECKF(hr == DD_OK || hr == DDERR_UNSUPPORTED, "-> %s", d3dh_hr_name(hr));
    if (hr == DD_OK) CHECK(hz > 0 && hz < 1000);
    t_note("GetMonitorFrequency -> %s, %lu Hz", d3dh_hr_name(hr), hz);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_scanline) {
    DL_SKIP_IF_NO_DD7(dd);
    DWORD line = 0xdeadbeef;
    HRESULT hr = IDirectDraw7_GetScanLine(dd, &line);
    CHECKF(hr == DD_OK || hr == DDERR_UNSUPPORTED || hr == DDERR_VERTICALBLANKINPROGRESS, "-> %s", d3dh_hr_name(hr));
    if (hr == DDERR_UNSUPPORTED) EXPECT_EQ(line, 0);   /* *scanline is zeroed when unsupported */
    t_note("GetScanLine -> %s line=%lu", d3dh_hr_name(hr), line);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_vertical_blank_status) {
    DL_SKIP_IF_NO_DD7(dd);
    BOOL vb = 0x77;
    HRESULT hr = IDirectDraw7_GetVerticalBlankStatus(dd, &vb);
    CHECKF(hr == DD_OK || hr == DDERR_UNSUPPORTED, "-> %s", d3dh_hr_name(hr));
    if (hr == DD_OK) CHECKF(vb == 0 || vb == 1, "value %d", vb);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_waitforvblank_flags) {
    DL_SKIP_IF_NO_DD7(dd);
    /* only DDWAITVB_BLOCKBEGIN (1) and DDWAITVB_BLOCKEND (4) are valid without an event */
    DWORD bad[] = { 0, 8, 16, 0x80000000 };
    for (unsigned i = 0; i < 4; i++) {
        HRESULT hr = IDirectDraw7_WaitForVerticalBlank(dd, bad[i], NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "flags 0x%lx -> %s", bad[i], d3dh_hr_name(hr));
    }
    /* the event variants are not implemented */
    EXPECT_HR(IDirectDraw7_WaitForVerticalBlank(dd, DDWAITVB_BLOCKBEGINEVENT, NULL), DDERR_UNSUPPORTED);
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    EXPECT_HR(IDirectDraw7_WaitForVerticalBlank(dd, DDWAITVB_BLOCKBEGIN, ev), DDERR_UNSUPPORTED);
    EXPECT_HR(IDirectDraw7_WaitForVerticalBlank(dd, DDWAITVB_BLOCKEND, ev), DDERR_UNSUPPORTED);
    CloseHandle(ev);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_compact_requires_exclusive_mode) {
    DL_SKIP_IF_NO_DD7(dd);
    EXPECT_HR(IDirectDraw7_Compact(dd), DDERR_NOEXCLUSIVEMODE);
    HWND w = dl_window();
    IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL);
    EXPECT_HR(IDirectDraw7_Compact(dd), DDERR_NOEXCLUSIVEMODE);
    dl_window_free(w);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_restore_display_mode_noop) {
    DL_SKIP_IF_NO_DD7(dd);
    DDSURFACEDESC2 a, b; memset(&a, 0, sizeof a); a.dwSize = sizeof a; b = a;
    EXPECT_HR(IDirectDraw7_GetDisplayMode(dd, &a), DD_OK);
    HRESULT hr = IDirectDraw7_RestoreDisplayMode(dd);
    t_note("RestoreDisplayMode without a mode change -> %s", d3dh_hr_name(hr));
    EXPECT_HR(IDirectDraw7_GetDisplayMode(dd, &b), DD_OK);
    EXPECT_EQ(a.dwWidth, b.dwWidth);                 /* nothing changed */
    EXPECT_EQ(a.dwHeight, b.dwHeight);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_flip_to_gdi_surface_without_surfaces) {
    DL_SKIP_IF_NO_DD7(dd);
    HRESULT hr = IDirectDraw7_FlipToGDISurface(dd);
    t_note("FlipToGDISurface with no surfaces -> %s", d3dh_hr_name(hr));
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_get_gdi_surface_without_primary) {
    DL_SKIP_IF_NO_DD7(dd);
    IDirectDrawSurface7 *s = (IDirectDrawSurface7 *)(ULONG_PTR)0x1;
    HRESULT hr = IDirectDraw7_GetGDISurface(dd, &s);
    if (FAILED(hr)) EXPECT_EQ(s, 0);
    else if (s) IDirectDrawSurface7_Release(s);
    t_note("GetGDISurface with no primary -> %s", d3dh_hr_name(hr));
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_available_vidmem_args) {
    DL_SKIP_IF_NO_DD7(dd);
    DDSCAPS2 caps; memset(&caps, 0, sizeof caps);
    caps.dwCaps = DDSCAPS_TEXTURE;
    EXPECT_HR(IDirectDraw7_GetAvailableVidMem(dd, &caps, NULL, NULL), DDERR_INVALIDPARAMS);
    DWORD total = 0xdeadbeef, fr = 0xdeadbeef;
    HRESULT hr = IDirectDraw7_GetAvailableVidMem(dd, &caps, &total, &fr);
    if (hr == DD_OK) CHECK(fr <= total);
    t_note("GetAvailableVidMem(TEXTURE) -> %s total=%lu free=%lu", d3dh_hr_name(hr), total, fr);
    total = 0xdeadbeef;
    hr = IDirectDraw7_GetAvailableVidMem(dd, &caps, &total, NULL);
    if (hr == DD_OK) CHECK(total != 0xdeadbeef);
    fr = 0xdeadbeef;
    hr = IDirectDraw7_GetAvailableVidMem(dd, &caps, NULL, &fr);
    if (hr == DD_OK) CHECK(fr != 0xdeadbeef);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_available_vidmem_zeroed_on_failure) {
    DL_SKIP_IF_NO_DD7(dd);
    DDSCAPS2 caps; memset(&caps, 0, sizeof caps);
    caps.dwCaps = DDSCAPS_TEXTURE;
    DWORD total = 0xdeadbeef, fr = 0xdeadbeef;
    HRESULT hr = IDirectDraw7_GetAvailableVidMem(dd, &caps, &total, &fr);
    if (FAILED(hr)) { EXPECT_EQ(total, 0); EXPECT_EQ(fr, 0); }   /* outputs are cleared before the driver is asked */
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_device_identifier7) {
    DL_SKIP_IF_NO_DD7(dd);
    DDDEVICEIDENTIFIER2 id; memset(&id, 0xCD, sizeof id);
    HRESULT hr = IDirectDraw7_GetDeviceIdentifier(dd, &id, 0);
    if (hr == DDERR_NOTFOUND) { t_skip("display device not found in EnumDisplayDevices"); IDirectDraw7_Release(dd); return; }
    EXPECT_HR(hr, DD_OK);
    if (SUCCEEDED(hr)) {
        CHECK(id.szDriver[0] != 0);
        CHECK(id.szDescription[0] != 0);
        CHECK(memchr(id.szDriver, 0, sizeof id.szDriver) != NULL);
        CHECK(memchr(id.szDescription, 0, sizeof id.szDescription) != NULL);
        t_note("driver=\"%s\" desc=\"%s\" vendor=%04lx device=%04lx", id.szDriver, id.szDescription, id.dwVendorId, id.dwDeviceId);
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_device_identifier7_bad_args) {
    DL_SKIP_IF_NO_DD7(dd);
    if (!t_is_wine()) EXPECT_HR(IDirectDraw7_GetDeviceIdentifier(dd, NULL, 0), DDERR_INVALIDPARAMS);
    DDDEVICEIDENTIFIER2 id;
    DWORD bad[] = { 2, 4, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < 4; i++) {
        HRESULT hr = IDirectDraw7_GetDeviceIdentifier(dd, &id, bad[i]);
        CHECKF(hr == DDERR_INVALIDPARAMS, "flags 0x%lx -> %s", bad[i], d3dh_hr_name(hr));
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_device_identifier_dd4_null) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw4 *dd = NULL;
    if (FAILED(IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd)) || !dd) { IDirectDraw_Release(dd1); return; }
    if (!t_is_wine()) EXPECT_HR(IDirectDraw4_GetDeviceIdentifier(dd, NULL, 0), DDERR_INVALIDPARAMS);
    DDDEVICEIDENTIFIER id; memset(&id, 0, sizeof id);
    if (t_is_wine()) { IDirectDraw4_Release(dd); IDirectDraw_Release(dd1); t_skip("emulator dereferences NULL"); return; }
    HRESULT hr = IDirectDraw4_GetDeviceIdentifier(dd, &id, 0);
    if (hr == DD_OK) CHECK(id.szDriver[0] != 0);
    IDirectDraw4_Release(dd);
    IDirectDraw_Release(dd1);
}
TEST(ddraw_legacy_misc_device_identifier_consistent) {
    DL_SKIP_IF_NO_DD7(dd);
    DDDEVICEIDENTIFIER2 a, b;
    if (FAILED(IDirectDraw7_GetDeviceIdentifier(dd, &a, 0))) { IDirectDraw7_Release(dd); return; }
    EXPECT_HR(IDirectDraw7_GetDeviceIdentifier(dd, &b, 0), DD_OK);
    EXPECT_EQ(a.dwVendorId, b.dwVendorId);
    EXPECT_EQ(a.dwDeviceId, b.dwDeviceId);
    EXPECT_STR(a.szDriver, b.szDriver);
    CHECK(!memcmp(&a.guidDeviceIdentifier, &b.guidDeviceIdentifier, sizeof(GUID)));
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_surface_from_dc_null) {
    DL_SKIP_IF_NO_DD7(dd);
    IDirectDrawSurface7 *s = NULL;
    HDC dc = GetDC(NULL);
    HRESULT hr = IDirectDraw7_GetSurfaceFromDC(dd, dc, &s);   /* a screen DC is not a DirectDraw surface DC */
    CHECKF(FAILED(hr), "-> %s", d3dh_hr_name(hr));
    if (s) IDirectDrawSurface7_Release(s);
    ReleaseDC(NULL, dc);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_misc_testcoop_dd1_unavailable_not_crash) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw4 *dd4 = NULL;
    if (SUCCEEDED(IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd4)) && dd4) {
        HRESULT hr = IDirectDraw4_TestCooperativeLevel(dd4);
        t_note("IDirectDraw4::TestCooperativeLevel -> %s", d3dh_hr_name(hr));
        IDirectDraw4_Release(dd4);
    }
    IDirectDraw_Release(dd1);
}
