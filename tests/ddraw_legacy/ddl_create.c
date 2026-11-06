/* ddraw.dll creation entry points, enumeration, class objects. */
#include "dl.h"

static const char *k_exports[] = {
    "AcquireDDThreadLock", "CompleteCreateSysmemSurface", "D3DParseUnknownCommand", "DDGetAttachedSurfaceLcl",
    "DDInternalLock", "DDInternalUnlock", "DSoundHelp", "DirectDrawCreate", "DirectDrawCreateClipper",
    "DirectDrawCreateEx", "DirectDrawEnumerateA", "DirectDrawEnumerateExA", "DirectDrawEnumerateExW",
    "DirectDrawEnumerateW", "DllCanUnloadNow", "DllGetClassObject", "GetDDSurfaceLocal", "GetOLEThunkData",
    "GetSurfaceFromDC", "RegisterSpecialCase", "ReleaseDDThreadLock", "SetAppCompatData",
};

TEST(ddraw_legacy_exports_resolve) {
    HMODULE m = t_module("ddraw.dll");
    if (!m) { t_skip("ddraw.dll not loadable"); return; }
    for (unsigned i = 0; i < sizeof k_exports / sizeof *k_exports; i++) {
        FARPROC p = GetProcAddress(m, k_exports[i]);
        CHECKF(p != NULL, "export %s", k_exports[i]);
    }
}
TEST(ddraw_legacy_exports_in_module_range) {
    HMODULE m = t_module("ddraw.dll");
    if (!m) { t_skip("ddraw.dll not loadable"); return; }
    for (unsigned i = 0; i < sizeof k_exports / sizeof *k_exports; i++) {
        FARPROC p = GetProcAddress(m, k_exports[i]);
        if (!p) continue;
        HMODULE owner = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
        char on[MAX_PATH] = "?"; if (owner) GetModuleFileNameA(owner, on, sizeof on);
        /* 32-bit ddraw: DirectDrawCreate/DirectDrawCreateEx resolve to the apphelp.dll compatibility hook, everything else (and all of x64) is inside ddraw.dll */
        int hooked = !DL_X64 && (!strcmp(k_exports[i], "DirectDrawCreate") || !strcmp(k_exports[i], "DirectDrawCreateEx"));
        if (hooked) { const char *b = strrchr(on, '\\'); CHECKF(b && !lstrcmpiA(b + 1, "apphelp.dll"), "%s owner %s (want apphelp.dll)", k_exports[i], on); }
        else CHECKF(owner == m, "%s is not implemented inside ddraw.dll (forwarder?): owner %s", k_exports[i], on);
    }
}
TEST(ddraw_legacy_export_missing_name) {
    HMODULE m = t_module("ddraw.dll");
    if (!m) { t_skip("no ddraw"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, "DirectDrawCreateExW") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    CHECK(GetProcAddress(m, "directdrawcreate") == NULL);   /* export lookup is case sensitive */
}

/* ---------------- DirectDrawCreate ---------------- */
TEST(ddraw_legacy_create_null_out) {
    dl_create_t f = DL_PROC(dl_create_t, "DirectDrawCreate"); REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator dereferences NULL"); return; }
    EXPECT_HR(f(NULL, NULL, NULL), DDERR_INVALIDPARAMS);
    EXPECT_HR(DDERR_INVALIDPARAMS, E_INVALIDARG);
}
TEST(ddraw_legacy_create_aggregation_refused) {
    dl_create_t f = DL_PROC(dl_create_t, "DirectDrawCreate"); REQUIRE(f);
    IDirectDraw *dd = (IDirectDraw *)(ULONG_PTR)0x1234;
    IUnknown *outer = (IUnknown *)(ULONG_PTR)0x10;   /* only compared against NULL by the DLL */
    HRESULT hr = f(NULL, &dd, outer);
    EXPECT_HR(hr, CLASS_E_NOAGGREGATION);
    EXPECT_EQ(dd, 0x1234);                           /* output not written on the aggregation error */
}
TEST(ddraw_legacy_create_clears_out_on_failure) {
    dl_create_t f = DL_PROC(dl_create_t, "DirectDrawCreate"); REQUIRE(f);
    GUID bad = { 0x12345678, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    IDirectDraw *dd = (IDirectDraw *)(ULONG_PTR)0x1234;
    HRESULT hr = f(&bad, &dd, NULL);
    if (FAILED(hr)) EXPECT_EQ(dd, 0);
    else if (dd) IDirectDraw_Release(dd);
}
TEST(ddraw_legacy_create_unknown_guid) {
    dl_create_t f = DL_PROC(dl_create_t, "DirectDrawCreate"); REQUIRE(f);
    GUID bad = { 0x12345678, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    IDirectDraw *dd = NULL;
    HRESULT hr = f(&bad, &dd, NULL);
    /* Win10 (single monitor, basic display adapter): an unknown driver GUID is rejected, it is not mapped to the primary driver */
    EXPECT_HR(hr, DDERR_INVALIDDIRECTDRAWGUID);
    EXPECT_EQ(dd, 0);
    if (dd) IDirectDraw_Release(dd);
}
TEST(ddraw_legacy_create_primary_release_rc) {
    DL_SKIP_IF_NO_DD1(dd);
    EXPECT_RET(IDirectDraw_AddRef(dd), 2);
    EXPECT_RET(IDirectDraw_Release(dd), 1);
    EXPECT_RET(IDirectDraw_Release(dd), 0);
}
TEST(ddraw_legacy_create_two_objects_independent) {
    DL_SKIP_IF_NO_DD1(a);
    DL_SKIP_IF_NO_DD1(b);
    CHECK(a != b);
    EXPECT_RET(IDirectDraw_Release(a), 0);
    DDCAPS caps; memset(&caps, 0, sizeof caps); caps.dwSize = sizeof caps;
    EXPECT_HR(IDirectDraw_GetCaps(b, &caps, NULL), DD_OK);   /* b unaffected by a's destruction */
    EXPECT_RET(IDirectDraw_Release(b), 0);
}
CHILD(ddl_emu) {
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    typedef HRESULT (WINAPI *cr_t)(GUID *, LPDIRECTDRAW *, IUnknown *);
    cr_t f = (cr_t)t_proc_in("ddraw.dll", "DirectDrawCreate"); if (!f) return 0xC0DE0001;
    IDirectDraw *a = NULL, *b = NULL;
    if (mode >= 1) { if (FAILED(f(NULL, &a, NULL))) return 0xC0DE0002; if (mode == 2) { IDirectDraw_Release(a); a = NULL; } }
    HRESULT h = f((GUID *)DDCREATE_EMULATIONONLY, &b, NULL);
    if (b) { DDCAPS hal, hel; memset(&hal, 0, sizeof hal); memset(&hel, 0, sizeof hel); hal.dwSize = hel.dwSize = sizeof hal; if (IDirectDraw_GetCaps(b, &hal, &hel) != DD_OK) return 0xC0DE0003; IDirectDraw_Release(b); }
    return (int)h;
}
TEST(ddraw_legacy_create_emulation_only) {
    /* Win10 22H2: the emulation-only (HEL) object can be created only while no DirectDraw object of the HAL driver is alive in the process */
    DWORD r0 = t_run_child("ddl_emu", "0", 60000), r1 = t_run_child("ddl_emu", "1", 60000), r2 = t_run_child("ddl_emu", "2", 60000);
    EXPECT_EQ(r0, 0);                  /* first call in the process */
    EXPECT_EQ(r1, 0x80004005);         /* E_FAIL while a DirectDrawCreate(NULL) object is alive */
    EXPECT_EQ(r2, 0);                  /* S_OK again after that object was released */
}
TEST(ddraw_legacy_create_hardware_only) {
    dl_create_t f = DL_PROC(dl_create_t, "DirectDrawCreate"); REQUIRE(f);
    IDirectDraw *dd = NULL;
    HRESULT hr = f((GUID *)DDCREATE_HARDWAREONLY, &dd, NULL);
    EXPECT_HR(hr, DD_OK);   /* Win10, Basic Display Adapter: the hardware-only request still yields the HAL object */
    if (SUCCEEDED(hr) && dd) IDirectDraw_Release(dd);
}

/* ---------------- DirectDrawCreateEx ---------------- */
TEST(ddraw_legacy_createex_wrong_iid) {
    dl_createex_t f = DL_PROC(dl_createex_t, "DirectDrawCreateEx"); REQUIRE(f);
    const IID *iids[] = { &IID_IUnknown, &IID_IDirectDraw, &IID_IDirectDraw2, &IID_IDirectDraw4, &IID_IDirectDrawClipper };
    for (unsigned i = 0; i < 5; i++) {
        LPVOID out = (LPVOID)(ULONG_PTR)0x77;
        HRESULT hr = f(NULL, &out, iids[i], NULL);
        CHECKF(hr == E_INVALIDARG, "iid %s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
    }
}
TEST(ddraw_legacy_createex_aggregation_refused) {
    dl_createex_t f = DL_PROC(dl_createex_t, "DirectDrawCreateEx"); REQUIRE(f);
    LPVOID out = (LPVOID)(ULONG_PTR)0x77;
    HRESULT hr = f(NULL, &out, &IID_IDirectDraw7, (IUnknown *)(ULONG_PTR)0x10);
    EXPECT_HR(hr, CLASS_E_NOAGGREGATION);
    EXPECT_EQ(out, 0x77);
    /* the aggregation check comes before the IID check */
    hr = f(NULL, &out, &IID_IUnknown, (IUnknown *)(ULONG_PTR)0x10);
    EXPECT_HR(hr, CLASS_E_NOAGGREGATION);
}
TEST(ddraw_legacy_createex_roundtrip) {
    DL_SKIP_IF_NO_DD7(dd);
    EXPECT_RET(IDirectDraw7_AddRef(dd), 2);
    EXPECT_RET(IDirectDraw7_Release(dd), 1);
    EXPECT_RET(IDirectDraw7_Release(dd), 0);
}
TEST(ddraw_legacy_createex_failure_clears_out) {
    dl_createex_t f = DL_PROC(dl_createex_t, "DirectDrawCreateEx"); REQUIRE(f);
    GUID bad = { 0x12345678, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    LPVOID out = (LPVOID)(ULONG_PTR)0x77;
    HRESULT hr = f(&bad, &out, &IID_IDirectDraw7, NULL);
    if (FAILED(hr)) EXPECT_EQ(out, 0);
    else if (out) IDirectDraw7_Release((IDirectDraw7 *)out);
}

/* ---------------- DirectDrawCreateClipper ---------------- */
TEST(ddraw_legacy_createclipper_flags_must_be_zero) {
    dl_createclipper_t f = DL_PROC(dl_createclipper_t, "DirectDrawCreateClipper"); REQUIRE(f);
    DWORD bad[] = { 1, 2, 0x80, 0x10000, 0xffffffff };
    for (unsigned i = 0; i < 5; i++) {
        LPDIRECTDRAWCLIPPER c = (LPDIRECTDRAWCLIPPER)(ULONG_PTR)0x55;
        HRESULT hr = f(bad[i], &c, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "flags 0x%lx -> %s", bad[i], d3dh_hr_name(hr));
        EXPECT_EQ(c, 0);
    }
}
TEST(ddraw_legacy_createclipper_aggregation_refused) {
    dl_createclipper_t f = DL_PROC(dl_createclipper_t, "DirectDrawCreateClipper"); REQUIRE(f);
    LPDIRECTDRAWCLIPPER c = (LPDIRECTDRAWCLIPPER)(ULONG_PTR)0x55;
    EXPECT_HR(f(0, &c, (IUnknown *)(ULONG_PTR)0x10), CLASS_E_NOAGGREGATION);
    EXPECT_EQ(c, 0x55);
}
TEST(ddraw_legacy_createclipper_standalone_lifetime) {
    dl_createclipper_t f = DL_PROC(dl_createclipper_t, "DirectDrawCreateClipper"); REQUIRE(f);
    LPDIRECTDRAWCLIPPER c = NULL;
    EXPECT_HR(f(0, &c, NULL), DD_OK);               /* needs no DirectDraw object */
    if (!c) return;
    EXPECT_RET(IDirectDrawClipper_AddRef(c), 2);
    EXPECT_RET(IDirectDrawClipper_Release(c), 1);
    EXPECT_RET(IDirectDrawClipper_Release(c), 0);
}
TEST(ddraw_legacy_createclipper_already_initialized) {
    dl_createclipper_t f = DL_PROC(dl_createclipper_t, "DirectDrawCreateClipper"); REQUIRE(f);
    LPDIRECTDRAWCLIPPER c = NULL;
    if (FAILED(f(0, &c, NULL)) || !c) { t_skip("no clipper"); return; }
    EXPECT_HR(IDirectDrawClipper_Initialize(c, NULL, 0), DDERR_ALREADYINITIALIZED);
    IDirectDrawClipper_Release(c);
}

/* ---------------- enumeration ---------------- */
struct en { int n; int stop_after; int first_guid_null; char name0[128]; char desc0[256]; HMONITOR hm0; };
static BOOL WINAPI enum_cb(GUID *g, LPSTR desc, LPSTR name, LPVOID ctx) {
    struct en *e = ctx;
    if (e->n == 0) {
        e->first_guid_null = (g == NULL);
        lstrcpynA(e->name0, name ? name : "(null)", sizeof e->name0);
        lstrcpynA(e->desc0, desc ? desc : "(null)", sizeof e->desc0);
    }
    e->n++;
    return e->stop_after && e->n >= e->stop_after ? FALSE : TRUE;
}
static BOOL WINAPI enum_cb_ex(GUID *g, LPSTR desc, LPSTR name, LPVOID ctx, HMONITOR hm) {
    struct en *e = ctx;
    if (e->n == 0) {
        e->first_guid_null = (g == NULL);
        lstrcpynA(e->name0, name ? name : "(null)", sizeof e->name0);
        lstrcpynA(e->desc0, desc ? desc : "(null)", sizeof e->desc0);
        e->hm0 = hm;
    }
    e->n++;
    return e->stop_after && e->n >= e->stop_after ? FALSE : TRUE;
}
static BOOL WINAPI enum_cb_w(GUID *g, LPWSTR d, LPWSTR n, LPVOID ctx) { ((struct en *)ctx)->n++; return TRUE; }

TEST(ddraw_legacy_enumA_null_callback) {
    dl_enumA_t f = DL_PROC(dl_enumA_t, "DirectDrawEnumerateA"); REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator calls a NULL callback"); return; }
    EXPECT_HR(f(NULL, NULL), DDERR_INVALIDPARAMS);
}
TEST(ddraw_legacy_enumA_first_is_primary) {
    dl_enumA_t f = DL_PROC(dl_enumA_t, "DirectDrawEnumerateA"); REQUIRE(f);
    struct en e = { 0 };
    HRESULT hr = f(enum_cb, &e);
    EXPECT_HR(hr, DD_OK);
    CHECK(e.n >= 1);
    CHECK(e.first_guid_null);                         /* primary display driver is reported with a NULL GUID */
    EXPECT_STR(e.name0, "display");
    CHECK(e.desc0[0] != 0);
    if (dl_basic_adapter()) EXPECT_STR(e.desc0, "Primary Display Driver");   /* Win10 */
}
TEST(ddraw_legacy_enumA_callback_stop) {
    dl_enumA_t f = DL_PROC(dl_enumA_t, "DirectDrawEnumerateA"); REQUIRE(f);
    struct en e = { 0 }; e.stop_after = 1;
    EXPECT_HR(f(enum_cb, &e), DD_OK);
    EXPECT_EQ(e.n, 1);                                /* FALSE from the callback ends the enumeration, still DD_OK */
}
TEST(ddraw_legacy_enumA_repeatable) {
    dl_enumA_t f = DL_PROC(dl_enumA_t, "DirectDrawEnumerateA"); REQUIRE(f);
    struct en a = { 0 }, b = { 0 };
    EXPECT_HR(f(enum_cb, &a), DD_OK);
    EXPECT_HR(f(enum_cb, &b), DD_OK);
    EXPECT_EQ(a.n, b.n);
}
TEST(ddraw_legacy_enumW_null_callback) {
    dl_enumW_t f = DL_PROC(dl_enumW_t, "DirectDrawEnumerateW"); REQUIRE(f);
    EXPECT_HR(f(NULL, NULL), DDERR_INVALIDPARAMS);
}
TEST(ddraw_legacy_enumW_not_implemented) {
    dl_enumW_t f = DL_PROC(dl_enumW_t, "DirectDrawEnumerateW"); REQUIRE(f);
    struct en e = { 0 };
    EXPECT_HR(f(enum_cb_w, &e), E_NOTIMPL);           /* the wide entry point is a stub */
    EXPECT_EQ(e.n, 0);
}
TEST(ddraw_legacy_enumExW_not_implemented) {
    dl_enumExW_t f = DL_PROC(dl_enumExW_t, "DirectDrawEnumerateExW"); REQUIRE(f);
    struct en e = { 0 };
    EXPECT_HR(f((LPVOID)enum_cb_w, &e, 0), E_NOTIMPL);
    EXPECT_HR(f((LPVOID)enum_cb_w, &e, DDENUM_ATTACHEDSECONDARYDEVICES), E_NOTIMPL);
    EXPECT_EQ(e.n, 0);
}
TEST(ddraw_legacy_enumExA_null_callback) {
    dl_enumExA_t f = DL_PROC(dl_enumExA_t, "DirectDrawEnumerateExA"); REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator calls a NULL callback"); return; }
    EXPECT_HR(f(NULL, NULL, 0), DDERR_INVALIDPARAMS);
    EXPECT_HR(f(NULL, NULL, DDENUM_ATTACHEDSECONDARYDEVICES), DDERR_INVALIDPARAMS);
}
TEST(ddraw_legacy_enumExA_bad_flags) {
    dl_enumExA_t f = DL_PROC(dl_enumExA_t, "DirectDrawEnumerateExA"); REQUIRE(f);
    DWORD bad[] = { 8, 0x10, 0x100, 0x80000000, 0xfffffff8, 0xffffffff, 0x0f };
    for (unsigned i = 0; i < 7; i++) {
        struct en e = { 0 };
        HRESULT hr = f(enum_cb_ex, &e, bad[i]);
        CHECKF(hr == DDERR_INVALIDPARAMS, "flags 0x%lx -> %s", bad[i], d3dh_hr_name(hr));
        EXPECT_EQ(e.n, 0);                            /* nothing is enumerated when the flags are rejected */
    }
}
TEST(ddraw_legacy_enumExA_valid_flag_combinations) {
    dl_enumExA_t f = DL_PROC(dl_enumExA_t, "DirectDrawEnumerateExA"); REQUIRE(f);
    for (DWORD fl = 0; fl < 8; fl++) {
        struct en e = { 0 };
        HRESULT hr = f(enum_cb_ex, &e, fl);
        CHECKF(hr == DD_OK, "flags 0x%lx -> %s", fl, d3dh_hr_name(hr));
        CHECKF(e.n >= 1, "flags 0x%lx enumerated %d", fl, e.n);
        CHECKF(e.first_guid_null, "flags 0x%lx first GUID not NULL", fl);
    }
}
TEST(ddraw_legacy_enumExA_primary_description_and_monitor) {
    dl_enumExA_t f = DL_PROC(dl_enumExA_t, "DirectDrawEnumerateExA"); REQUIRE(f);
    struct en e = { 0 };
    EXPECT_HR(f(enum_cb_ex, &e, 0), DD_OK);
    EXPECT_STR(e.name0, "display");
    CHECK(e.desc0[0] != 0);
    CHECK(e.hm0 == NULL);   /* Win10: without DDENUM_ATTACHEDSECONDARYDEVICES the primary driver is reported with a NULL monitor */
    EXPECT_EQ(e.n, 1);
}
TEST(ddraw_legacy_enumExA_stop) {
    dl_enumExA_t f = DL_PROC(dl_enumExA_t, "DirectDrawEnumerateExA"); REQUIRE(f);
    struct en e = { 0 }; e.stop_after = 1;
    EXPECT_HR(f(enum_cb_ex, &e, DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_NONDISPLAYDEVICES), DD_OK);
    EXPECT_EQ(e.n, 1);
}
TEST(ddraw_legacy_enumExA_nondisplay_superset) {
    dl_enumExA_t f = DL_PROC(dl_enumExA_t, "DirectDrawEnumerateExA"); REQUIRE(f);
    struct en a = { 0 }, b = { 0 };
    EXPECT_HR(f(enum_cb_ex, &a, 0), DD_OK);
    EXPECT_HR(f(enum_cb_ex, &b, DDENUM_NONDISPLAYDEVICES), DD_OK);
    CHECK(b.n >= a.n);
}
TEST(ddraw_legacy_enumA_matches_enumExA_flags4) {
    dl_enumA_t fa = DL_PROC(dl_enumA_t, "DirectDrawEnumerateA");
    dl_enumExA_t fx = DL_PROC(dl_enumExA_t, "DirectDrawEnumerateExA"); REQUIRE(fa); REQUIRE(fx);
    struct en a = { 0 }, x = { 0 };
    EXPECT_HR(fa(enum_cb, &a), DD_OK);
    EXPECT_HR(fx(enum_cb_ex, &x, DDENUM_NONDISPLAYDEVICES), DD_OK);
    EXPECT_EQ(a.n, x.n);                              /* the ANSI non-Ex entry point forwards with DDENUM_NONDISPLAYDEVICES */
}
TEST(ddraw_legacy_enum_primary_guid_creatable) {
    dl_enumA_t f = DL_PROC(dl_enumA_t, "DirectDrawEnumerateA"); REQUIRE(f);
    struct en e = { 0 };
    EXPECT_HR(f(enum_cb, &e), DD_OK);
    CHECK(e.first_guid_null);
    DL_SKIP_IF_NO_DD1(dd);
    IDirectDraw_Release(dd);
}

/* ---------------- DllGetClassObject / DllCanUnloadNow / class factories ---------------- */
TEST(ddraw_legacy_dllgetclassobject_arg_errors) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    void *p = (void *)0x99;
    if (t_is_wine()) { t_skip("emulator dereferences NULL arguments"); return; }
    EXPECT_HR(f(&CLSID_DirectDraw, &IID_IClassFactory, NULL), E_INVALIDARG);
    EXPECT_HR(f(NULL, &IID_IClassFactory, &p), E_INVALIDARG);
    EXPECT_EQ(p, 0);                                  /* *ppv is cleared before the CLSID is validated */
    p = (void *)0x99;
    EXPECT_HR(f(&CLSID_DirectDraw, NULL, &p), E_INVALIDARG);
    EXPECT_EQ(p, 0);
}
TEST(ddraw_legacy_dllgetclassobject_unknown_clsid) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    const GUID *bad[] = { &GUID_NULL, &IID_IUnknown, &IID_IDirectDraw7, &CLSID_StdGlobalInterfaceTable };
    for (unsigned i = 0; i < 4; i++) {
        void *p = (void *)0x99;
        HRESULT hr = f(bad[i], &IID_IClassFactory, &p);
        CHECKF(hr == E_FAIL, "clsid %s -> %s", d3dh_guid_str(bad[i]), d3dh_hr_name(hr));
        EXPECT_EQ(p, 0);
    }
}
TEST(ddraw_legacy_dllgetclassobject_unknown_iid) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    const GUID *clsids[] = { &CLSID_DirectDraw, &CLSID_DirectDraw7, &CLSID_DirectDrawClipper };
    const GUID *iids[] = { &IID_IDirectDraw, &IID_IDirectDraw7, &IID_IClassFactory2, &GUID_NULL };
    for (unsigned c = 0; c < 3; c++) for (unsigned i = 0; i < 4; i++) {
        void *p = (void *)0x99;
        HRESULT hr = f(clsids[c], iids[i], &p);
        CHECKF(hr == E_NOINTERFACE, "%s / %s -> %s", d3dh_guid_str(clsids[c]), d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        EXPECT_EQ(p, 0);
    }
}
TEST(ddraw_legacy_dllgetclassobject_factory_lifetime) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    dl_dllcanunloadnow_t cu = DL_PROC(dl_dllcanunloadnow_t, "DllCanUnloadNow"); REQUIRE(cu);
    const GUID *clsids[] = { &CLSID_DirectDraw, &CLSID_DirectDraw7, &CLSID_DirectDrawClipper };
    const GUID *iids[] = { &IID_IClassFactory, &IID_IUnknown };
    for (unsigned c = 0; c < 3; c++) for (unsigned i = 0; i < 2; i++) {
        IClassFactory *cf = NULL;
        HRESULT hr = f(clsids[c], iids[i], (void **)&cf);
        CHECKF(hr == S_OK && cf, "%s / %s -> %s", d3dh_guid_str(clsids[c]), d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        if (hr != S_OK || !cf) continue;
        EXPECT_RET(IClassFactory_AddRef(cf), 2);
        EXPECT_RET(IClassFactory_Release(cf), 1);
        EXPECT_HR(cu(), S_FALSE);                     /* an outstanding class factory pins the DLL */
        IClassFactory_Release(cf);
    }
}
TEST(ddraw_legacy_classfactory_qi) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDraw, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    IUnknown *u = NULL; void *x = (void *)0x42;
    EXPECT_HR(IClassFactory_QueryInterface(cf, &IID_IUnknown, (void **)&u), S_OK);
    CHECK(u == (IUnknown *)cf);
    if (u) IUnknown_Release(u);
    EXPECT_HR(IClassFactory_QueryInterface(cf, &IID_IClassFactory, (void **)&u), S_OK);
    if (u) IUnknown_Release(u);
    EXPECT_HR(IClassFactory_QueryInterface(cf, &IID_IDirectDraw, &x), E_NOINTERFACE);
    EXPECT_EQ(x, 0);
    EXPECT_HR(IClassFactory_QueryInterface(cf, &IID_IClassFactory2, &x), E_NOINTERFACE);
    IClassFactory_Release(cf);
}
TEST(ddraw_legacy_classfactory_createinstance_args) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDraw, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    void *p = (void *)0x42;
    if (!t_is_wine()) {
        EXPECT_HR(IClassFactory_CreateInstance(cf, NULL, &IID_IDirectDraw, NULL), E_INVALIDARG);
        EXPECT_HR(IClassFactory_CreateInstance(cf, NULL, NULL, &p), E_INVALIDARG);
    }
    EXPECT_EQ(p, 0);
    p = (void *)0x42;
    EXPECT_HR(IClassFactory_CreateInstance(cf, (IUnknown *)(ULONG_PTR)0x10, &IID_IDirectDraw, &p), CLASS_E_NOAGGREGATION);
    EXPECT_EQ(p, 0);
    IClassFactory_Release(cf);
}
TEST(ddraw_legacy_classfactory_createinstance_bad_iid) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDraw, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    void *p = (void *)0x42;
    if (t_is_wine()) { t_skip("emulator crashes on this IID"); IClassFactory_Release(cf); return; }
    HRESULT hr = IClassFactory_CreateInstance(cf, NULL, &IID_IClassFactory, &p);
    if (hr == E_FAIL) { t_skip("DirectDraw not supported on this display"); IClassFactory_Release(cf); return; }
    EXPECT_HR(hr, E_NOINTERFACE);
    EXPECT_EQ(p, 0);
    IClassFactory_Release(cf);
}
TEST(ddraw_legacy_classfactory_dd_uninitialized_until_initialize) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDraw7, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    IDirectDraw7 *dd = NULL;
    HRESULT hr = IClassFactory_CreateInstance(cf, NULL, &IID_IDirectDraw7, (void **)&dd);
    if (FAILED(hr) || !dd) { t_skip("CreateInstance -> %s", d3dh_hr_name(hr)); IClassFactory_Release(cf); return; }
    HWND w = dl_window();
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL), DDERR_NOTINITIALIZED);
    DDCAPS caps; memset(&caps, 0, sizeof caps); caps.dwSize = sizeof caps;
    EXPECT_HR(IDirectDraw7_GetCaps(dd, &caps, NULL), DDERR_NOTINITIALIZED);
    EXPECT_HR(IDirectDraw7_Initialize(dd, NULL), DD_OK);
    EXPECT_HR(IDirectDraw7_Initialize(dd, NULL), DDERR_ALREADYINITIALIZED);
    EXPECT_HR(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL), DD_OK);
    EXPECT_RET(IDirectDraw7_Release(dd), 0);
    dl_window_free(w);
    IClassFactory_Release(cf);
}
TEST(ddraw_legacy_classfactory_dd1_iid_ladder) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDraw, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    const IID *iids[] = { &IID_IUnknown, &IID_IDirectDraw, &IID_IDirectDraw2, &IID_IDirectDraw4, &IID_IDirectDraw7 };
    for (unsigned i = 0; i < 5; i++) {
        IUnknown *u = NULL;
        HRESULT hr = IClassFactory_CreateInstance(cf, NULL, iids[i], (void **)&u);
        if (hr == E_FAIL) { t_skip("DirectDraw not supported"); break; }
        CHECKF(hr == S_OK && u, "%s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        if (u) EXPECT_RET(IUnknown_Release(u), 0);
    }
    IClassFactory_Release(cf);
}
TEST(ddraw_legacy_classfactory_clipper) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDrawClipper, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    IDirectDrawClipper *c = NULL; void *x = (void *)0x42;
    EXPECT_HR(IClassFactory_CreateInstance(cf, NULL, &IID_IDirectDrawClipper, (void **)&c), S_OK);
    if (c) {
        EXPECT_HR(IDirectDrawClipper_Initialize(c, NULL, 0), DD_OK);
        EXPECT_HR(IDirectDrawClipper_Initialize(c, NULL, 0), DDERR_ALREADYINITIALIZED);
        IDirectDrawClipper_Release(c);
    }
    EXPECT_HR(IClassFactory_CreateInstance(cf, NULL, &IID_IDirectDraw, &x), E_NOINTERFACE);
    EXPECT_EQ(x, 0);
    IClassFactory_Release(cf);
}
TEST(ddraw_legacy_classfactory_uninit_clipper_methods) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDrawClipper, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    IDirectDrawClipper *c = NULL;
    if (FAILED(IClassFactory_CreateInstance(cf, NULL, &IID_IDirectDrawClipper, (void **)&c)) || !c) { t_skip("no clipper"); IClassFactory_Release(cf); return; }
    HWND h = NULL; BOOL ch = 0; DWORD sz = 0;
    HRESULT h1 = IDirectDrawClipper_GetHWnd(c, &h);
    HRESULT h2 = IDirectDrawClipper_IsClipListChanged(c, &ch);
    HRESULT h3 = IDirectDrawClipper_GetClipList(c, NULL, NULL, &sz);
    HRESULT h4 = IDirectDrawClipper_SetClipList(c, NULL, 0);
    EXPECT_HR(h1, DDERR_NOTINITIALIZED); EXPECT_HR(h2, DDERR_NOTINITIALIZED); EXPECT_HR(h3, DDERR_NOTINITIALIZED); EXPECT_HR(h4, DDERR_NOTINITIALIZED);
    IDirectDrawClipper_Release(c);
    IClassFactory_Release(cf);
}
TEST(ddraw_legacy_classfactory_lockserver) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    dl_dllcanunloadnow_t cu = DL_PROC(dl_dllcanunloadnow_t, "DllCanUnloadNow"); REQUIRE(cu);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDraw, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); CoUninitialize(); return; }
    HRESULT h1 = IClassFactory_LockServer(cf, TRUE);
    EXPECT_HR(h1, S_OK);   /* LockServer(TRUE): measured on Win10 */
    if (SUCCEEDED(h1)) {
        EXPECT_HR(cu(), S_FALSE);
        EXPECT_HR(IClassFactory_LockServer(cf, FALSE), S_OK);
    }
    IClassFactory_Release(cf);
    CoUninitialize();
}
TEST(ddraw_legacy_canunloadnow_with_object) {
    dl_dllcanunloadnow_t cu = DL_PROC(dl_dllcanunloadnow_t, "DllCanUnloadNow"); REQUIRE(cu);
    DL_SKIP_IF_NO_DD1(dd);
    EXPECT_HR(cu(), S_FALSE);
    IDirectDraw_Release(dd);
}
TEST(ddraw_legacy_canunloadnow_with_clipper) {
    dl_dllcanunloadnow_t cu = DL_PROC(dl_dllcanunloadnow_t, "DllCanUnloadNow"); REQUIRE(cu);
    dl_createclipper_t f = DL_PROC(dl_createclipper_t, "DirectDrawCreateClipper"); REQUIRE(f);
    LPDIRECTDRAWCLIPPER c = NULL;
    if (FAILED(f(0, &c, NULL)) || !c) { t_skip("no clipper"); return; }
    EXPECT_HR(cu(), S_FALSE);
    IDirectDrawClipper_Release(c);
}

/* ---------------- COM registration ---------------- */
static void cocreate_case(const CLSID *clsid, const IID *iid, const char *what) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    void *p = NULL;
    HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, iid, &p);
    if (hr == REGDB_E_CLASSNOTREG) { t_skip("%s not registered", what); }
    else if (hr == E_FAIL) { t_skip("%s: DirectDraw unsupported here", what); }
    else {
        CHECKF(hr == S_OK && p, "%s -> %s", what, d3dh_hr_name(hr));
        if (p) IUnknown_Release((IUnknown *)p);
    }
    CoUninitialize();
}
TEST(ddraw_legacy_cocreate_directdraw) { cocreate_case(&CLSID_DirectDraw, &IID_IDirectDraw, "CLSID_DirectDraw/IDirectDraw"); }
TEST(ddraw_legacy_cocreate_directdraw_dd4) { cocreate_case(&CLSID_DirectDraw, &IID_IDirectDraw4, "CLSID_DirectDraw/IDirectDraw4"); }
TEST(ddraw_legacy_cocreate_directdraw7) { cocreate_case(&CLSID_DirectDraw7, &IID_IDirectDraw7, "CLSID_DirectDraw7/IDirectDraw7"); }
TEST(ddraw_legacy_cocreate_clipper) { cocreate_case(&CLSID_DirectDrawClipper, &IID_IDirectDrawClipper, "CLSID_DirectDrawClipper"); }
TEST(ddraw_legacy_factory_aggregation_refused) {
    dl_dllgetclassobject_t f = DL_PROC(dl_dllgetclassobject_t, "DllGetClassObject"); REQUIRE(f);
    void *p = NULL;
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDraw, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    EXPECT_HR(IClassFactory_CreateInstance(cf, (IUnknown *)cf, &IID_IDirectDraw, &p), CLASS_E_NOAGGREGATION);
    IClassFactory_Release(cf);
}
