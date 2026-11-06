/* ddraw.dll entry points and class objects, d3dscache.dll DllGetClassObject, dxva2.dll device manager / DXVAHD, dxcore.dll factory, d3d9.dll PIX markers. */
#include "gap1.h"
#include <dxva2api.h>
#include <dxvahd.h>

#define GAP_MAGIC ((void *)(ULONG_PTR)0x1234)
static const GUID G1_IID_DecoderService = { 0xfc51a551, 0xd5e7, 0x11d9, { 0xaf, 0x55, 0x00, 0x05, 0x4e, 0x43, 0xff, 0x02 } };
static const GUID G1_IID_ProcessorService = { 0xfc51a552, 0xd5e7, 0x11d9, { 0xaf, 0x55, 0x00, 0x05, 0x4e, 0x43, 0xff, 0x02 } };

/* ======================= ddraw.dll ======================= */
TEST(gap1_ddraw_create_rejects_aggregation) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("ddraw.dll", DirectDrawCreate));
    g1_unk outer = { &g1_unk_vtbl, 0 };
    LPDIRECTDRAW dd = GAP_MAGIC;
    EXPECT_HR(UIN("ddraw.dll", DirectDrawCreate)(NULL, &dd, (IUnknown *)&outer), G1_CLASS_E_NOAGGREGATION);
    CHECK(dd == GAP_MAGIC);                                   /* not touched on this path */
    EXPECT_EQ(outer.calls, 0);                                /* the outer object is never consulted */
}
TEST(gap1_ddraw_create_null_output) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("ddraw.dll", DirectDrawCreate));
    EXPECT_HR(UIN("ddraw.dll", DirectDrawCreate)(NULL, NULL, NULL), G1_E_INVALIDARG);
}
TEST(gap1_ddraw_create_unknown_guid_fails_and_clears_output) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("ddraw.dll", DirectDrawCreate));
    static GUID bogus = { 0x1a2b3c4d, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    LPDIRECTDRAW dd = GAP_MAGIC;
    HRESULT hr = UIN("ddraw.dll", DirectDrawCreate)(&bogus, &dd, NULL);
    EXPECT_HR(hr, (HRESULT)DDERR_INVALIDDIRECTDRAWGUID);
    CHECK(dd == NULL);
}
TEST(gap1_ddraw_create_default_object_is_initialized) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("ddraw.dll", DirectDrawCreate));
    LPDIRECTDRAW dd = NULL;
    HRESULT hr = UIN("ddraw.dll", DirectDrawCreate)(NULL, &dd, NULL);
    if (FAILED(hr) || !dd) { t_skip("DirectDrawCreate -> %s (no usable display)", d3dh_hr_name(hr)); return; }
    EXPECT_HR(IDirectDraw_Initialize(dd, NULL), DDERR_ALREADYINITIALIZED);
    IUnknown *u = NULL;
    EXPECT_HR(IDirectDraw_QueryInterface(dd, &IID_IUnknown, (void **)&u), S_OK);
    if (u) IUnknown_Release(u);
    LPDIRECTDRAW7 d7 = NULL;
    HRESULT h7 = IDirectDraw_QueryInterface(dd, &IID_IDirectDraw7, (void **)&d7);
    EXPECT_HR(h7, S_OK);
    if (d7) IDirectDraw7_Release(d7);
    void *bad = GAP_MAGIC;
    EXPECT_HR(IDirectDraw_QueryInterface(dd, &IID_ID3D11Device, &bad), E_NOINTERFACE);
    CHECK(bad == NULL);
    G1_EQ(IDirectDraw_Release(dd), 0);
}
CHILD(g1_emu) {
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    typedef HRESULT (WINAPI *cr_t)(GUID *, LPDIRECTDRAW *, IUnknown *);
    cr_t f = (cr_t)t_proc_in("ddraw.dll", "DirectDrawCreate"); if (!f) return 0xC0DE0001;
    IDirectDraw *a = NULL, *b = NULL;
    if (mode >= 1) { if (FAILED(f(NULL, &a, NULL))) return 0xC0DE0002; if (mode == 2) { IDirectDraw_Release(a); a = NULL; } }
    HRESULT h = f((GUID *)DDCREATE_EMULATIONONLY, &b, NULL);
    if (b) { DDCAPS hal, hel; memset(&hal, 0, sizeof hal); memset(&hel, 0, sizeof hel); hal.dwSize = hel.dwSize = sizeof hal; if (IDirectDraw_GetCaps(b, &hal, &hel) != DD_OK) return 0xC0DE0003; IDirectDraw_Release(b); }
    return (int)h;
}
TEST(gap1_ddraw_create_emulation_only_flag) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    /* Win10 22H2: the emulation-only (HEL) object can be created only while no DirectDraw object of the HAL driver is alive in the process */
    DWORD r0 = t_run_child("g1_emu", "0", 60000), r1 = t_run_child("g1_emu", "1", 60000), r2 = t_run_child("g1_emu", "2", 60000);
    EXPECT_EQ(r0, 0);                  /* first call in the process */
    EXPECT_EQ(r1, 0x80004005);         /* E_FAIL while a DirectDrawCreate(NULL) object is alive */
    EXPECT_EQ(r2, 0);                  /* S_OK again after that object was released */
}
TEST(gap1_ddraw_createex_rejects_aggregation_before_iid_check) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("ddraw.dll", DirectDrawCreateEx));
    g1_unk outer = { &g1_unk_vtbl, 0 };
    void *p = GAP_MAGIC;
    EXPECT_HR(UIN("ddraw.dll", DirectDrawCreateEx)(NULL, &p, &IID_IDirectDraw7, (IUnknown *)&outer), G1_CLASS_E_NOAGGREGATION);
    EXPECT_HR(UIN("ddraw.dll", DirectDrawCreateEx)(NULL, &p, &IID_IDirectDraw, (IUnknown *)&outer), G1_CLASS_E_NOAGGREGATION);
    EXPECT_EQ(outer.calls, 0);
}
TEST(gap1_ddraw_createex_only_accepts_idirectdraw7) {
    REQUIRE(UIN("ddraw.dll", DirectDrawCreateEx));
    static const IID *bad[] = { &IID_IDirectDraw, &IID_IDirectDraw2, &IID_IDirectDraw4, &IID_IUnknown, &IID_IDirectDrawClipper, &GUID_NULL };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        void *p = NULL;
        HRESULT hr = UIN("ddraw.dll", DirectDrawCreateEx)(NULL, &p, bad[i], NULL);
        CHECKF(hr == G1_E_INVALIDARG, "iid %s -> %s", d3dh_guid_str(bad[i]), d3dh_hr_name(hr));
        CHECKF(p == NULL, "iid %u: object returned", i);
    }
}
TEST(gap1_ddraw_createex_default_object) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("ddraw.dll", DirectDrawCreateEx));
    LPDIRECTDRAW7 d7 = NULL;
    HRESULT hr = UIN("ddraw.dll", DirectDrawCreateEx)(NULL, (void **)&d7, &IID_IDirectDraw7, NULL);
    if (FAILED(hr) || !d7) { t_skip("DirectDrawCreateEx -> %s (no usable display)", d3dh_hr_name(hr)); return; }
    EXPECT_HR(IDirectDraw7_Initialize(d7, NULL), DDERR_ALREADYINITIALIZED);
    IUnknown *u1 = NULL, *u2 = NULL;
    IDirectDraw7_QueryInterface(d7, &IID_IUnknown, (void **)&u1);
    IDirectDraw7_QueryInterface(d7, &IID_IUnknown, (void **)&u2);
    CHECK(u1 != NULL && u1 == u2);                            /* identity */
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    G1_EQ(IDirectDraw7_Release(d7), 0);
}

TEST(gap1_ddraw_clipper_rejects_aggregation) {
    REQUIRE(UIN("ddraw.dll", DirectDrawCreateClipper));
    g1_unk outer = { &g1_unk_vtbl, 0 };
    LPDIRECTDRAWCLIPPER c = GAP_MAGIC;
    EXPECT_HR(UIN("ddraw.dll", DirectDrawCreateClipper)(0, &c, (IUnknown *)&outer), G1_CLASS_E_NOAGGREGATION);
    CHECK(c == GAP_MAGIC);
    EXPECT_EQ(outer.calls, 0);
}
TEST(gap1_ddraw_clipper_flags_must_be_zero) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("ddraw.dll", DirectDrawCreateClipper));
    static const DWORD bad[] = { 1, 2, 0x10, 0x8000, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        LPDIRECTDRAWCLIPPER c = GAP_MAGIC;
        HRESULT hr = UIN("ddraw.dll", DirectDrawCreateClipper)(bad[i], &c, NULL);
        CHECKF(hr == G1_E_INVALIDARG, "flags 0x%lx -> %s", bad[i], d3dh_hr_name(hr));
        CHECKF(c == NULL, "flags 0x%lx: output not cleared", bad[i]);
    }
}
TEST(gap1_ddraw_clipper_create_and_lifetime) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("ddraw.dll", DirectDrawCreateClipper));
    LPDIRECTDRAWCLIPPER c = NULL, c2 = NULL;
    HRESULT hr = UIN("ddraw.dll", DirectDrawCreateClipper)(0, &c, NULL);
    if (FAILED(hr)) { t_skip("DirectDrawCreateClipper -> %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(hr, S_OK);
    CHECK(c != NULL);
    if (!c) return;
    G1_EQ(IDirectDrawClipper_AddRef(c), 2);
    G1_EQ(IDirectDrawClipper_Release(c), 1);
    EXPECT_HR(IDirectDrawClipper_Initialize(c, NULL, 0), DDERR_ALREADYINITIALIZED);
    EXPECT_HR(UIN("ddraw.dll", DirectDrawCreateClipper)(0, &c2, NULL), S_OK);
    CHECK(c2 != NULL && c2 != c);
    IUnknown *u = NULL;
    EXPECT_HR(IDirectDrawClipper_QueryInterface(c, &IID_IUnknown, (void **)&u), S_OK);
    if (u) IUnknown_Release(u);
    if (c2) IDirectDrawClipper_Release(c2);
    G1_EQ(IDirectDrawClipper_Release(c), 0);
}

typedef HRESULT (WINAPI *dgco_fn)(REFCLSID, REFIID, LPVOID *);
static dgco_fn dgco(const char *dll) { return (dgco_fn)t_proc_in(dll, "DllGetClassObject"); }

TEST(gap1_ddraw_dllgetclassobject_null_arguments) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    dgco_fn f = dgco("ddraw.dll"); REQUIRE(f);
    void *p = GAP_MAGIC;
    EXPECT_HR(f(&CLSID_DirectDraw, &IID_IClassFactory, NULL), G1_E_INVALIDARG);
    EXPECT_HR(f(NULL, &IID_IClassFactory, &p), G1_E_INVALIDARG);
    CHECK(p == NULL);                                         /* the output is cleared before the CLSID is examined */
    p = GAP_MAGIC;
    EXPECT_HR(f(&CLSID_DirectDraw, NULL, &p), G1_E_INVALIDARG);
    CHECK(p == NULL);
}
TEST(gap1_ddraw_dllgetclassobject_unknown_clsid) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    dgco_fn f = dgco("ddraw.dll"); REQUIRE(f);
    static const GUID bogus = { 0x5ab0c1d2, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };
    void *p = GAP_MAGIC;
    EXPECT_HR(f(&bogus, &IID_IClassFactory, &p), G1_E_FAIL);
    CHECK(p == NULL);
    p = GAP_MAGIC;
    EXPECT_HR(f(&GUID_NULL, &IID_IUnknown, &p), G1_E_FAIL);
    CHECK(p == NULL);
}
TEST(gap1_ddraw_dllgetclassobject_only_iunknown_or_iclassfactory) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    dgco_fn f = dgco("ddraw.dll"); REQUIRE(f);
    static const IID *bad[] = { &IID_IDirectDraw, &IID_IDirectDraw7, &IID_IClassFactory2, &GUID_NULL };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        void *p = GAP_MAGIC;
        HRESULT hr = f(&CLSID_DirectDraw, bad[i], &p);
        CHECKF(hr == G1_E_NOINTERFACE, "riid %s -> %s", d3dh_guid_str(bad[i]), d3dh_hr_name(hr));
        CHECK(p == NULL);
    }
}
CHILD(g1_lockserver_nocom) {
    dgco_fn f = dgco("ddraw.dll"); if (!f) return 90;
    const GUID *ids[] = { &CLSID_DirectDraw, &CLSID_DirectDraw7, &CLSID_DirectDrawClipper };
    int bad = 0;
    for (int i = 0; i < 3; i++) {
        IClassFactory *cf = NULL;
        if (FAILED(f(ids[i], &IID_IClassFactory, (void **)&cf)) || !cf) return 91;
        if (IClassFactory_LockServer(cf, TRUE) != (HRESULT)0x800401f0) bad |= 1 << i;
        if (IClassFactory_LockServer(cf, FALSE) != (HRESULT)0x800401f0) bad |= 8 << i;
        IClassFactory_Release(cf);
    }
    return bad;
}
TEST(gap1_ddraw_factory_lockserver_needs_com) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    EXPECT_EQ(t_run_child("g1_lockserver_nocom", "", 30000), 0);   /* CO_E_NOTINITIALIZED for all three factories, TRUE and FALSE */
}
TEST(gap1_ddraw_dllgetclassobject_factories_for_each_clsid) {
    dgco_fn f = dgco("ddraw.dll"); REQUIRE(f);
    const GUID *ids[] = { &CLSID_DirectDraw, &CLSID_DirectDraw7, &CLSID_DirectDrawClipper };
    for (unsigned i = 0; i < 3; i++) {
        IClassFactory *cf = NULL;
        HRESULT hr = f(ids[i], &IID_IClassFactory, (void **)&cf);
        CHECKF(hr == S_OK && cf, "clsid %u -> %s", i, d3dh_hr_name(hr));
        if (!cf) continue;
        /* Win10: LockServer needs COM on the calling thread (without it CO_E_NOTINITIALIZED, see gap1_ddraw_factory_lockserver_needs_com) */
        HRESULT ci = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (SUCCEEDED(ci)) {
            EXPECT_HR(IClassFactory_LockServer(cf, TRUE), S_OK);
            EXPECT_HR(IClassFactory_LockServer(cf, FALSE), S_OK);
            CoUninitialize();
        }
        IUnknown *u = NULL;
        EXPECT_HR(IClassFactory_QueryInterface(cf, &IID_IUnknown, (void **)&u), S_OK);
        if (u) IUnknown_Release(u);
        IClassFactory_Release(cf);
    }
    /* IUnknown is accepted as the requested interface too */
    IUnknown *u = NULL;
    EXPECT_HR(f(&CLSID_DirectDraw, &IID_IUnknown, (void **)&u), S_OK);
    if (u) IUnknown_Release(u);
}
TEST(gap1_ddraw_clipper_factory_instance_needs_initialize) {
    dgco_fn f = dgco("ddraw.dll"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDrawClipper, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no clipper factory"); return; }
    LPDIRECTDRAWCLIPPER c = NULL;
    HRESULT hr = IClassFactory_CreateInstance(cf, NULL, &IID_IDirectDrawClipper, (void **)&c);
    EXPECT_HR(hr, S_OK);
    if (c) {
        EXPECT_HR(IDirectDrawClipper_Initialize(c, NULL, 0), S_OK);
        EXPECT_HR(IDirectDrawClipper_Initialize(c, NULL, 0), DDERR_ALREADYINITIALIZED);
        EXPECT_HR(IDirectDrawClipper_Initialize(c, NULL, 1), DDERR_ALREADYINITIALIZED);
        IDirectDrawClipper_Release(c);
    }
    IClassFactory_Release(cf);
}
TEST(gap1_ddraw_factory_instance_needs_initialize) {
    dgco_fn f = dgco("ddraw.dll"); REQUIRE(f);
    IClassFactory *cf = NULL;
    if (FAILED(f(&CLSID_DirectDraw, &IID_IClassFactory, (void **)&cf)) || !cf) { t_skip("no factory"); return; }
    LPDIRECTDRAW dd = NULL;
    HRESULT hr = IClassFactory_CreateInstance(cf, NULL, &IID_IDirectDraw, (void **)&dd);
    if (FAILED(hr) || !dd) { t_note("CreateInstance(IID_IDirectDraw) -> %s", d3dh_hr_name(hr)); IClassFactory_Release(cf); return; }
    HRESULT hi = IDirectDraw_Initialize(dd, NULL);
    if (FAILED(hi)) t_note("Initialize on factory object -> %s (no usable display?)", d3dh_hr_name(hi));
    else EXPECT_HR(IDirectDraw_Initialize(dd, NULL), DDERR_ALREADYINITIALIZED);
    IDirectDraw_Release(dd);
    IClassFactory_Release(cf);
}

/* ======================= d3dscache.dll ======================= */
TEST(gap1_scache_dllgetclassobject_unknown_clsid) {
    dgco_fn f = dgco("d3dscache.dll"); REQUIRE(f);
    static const GUID bogus = { 0x5ab0c1d2, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };
    void *p = GAP_MAGIC;
    EXPECT_HR(f(&bogus, &IID_IClassFactory, &p), G1_CLASS_E_CLASSNOTAVAILABLE);
    CHECK(p == NULL);
    p = GAP_MAGIC;
    EXPECT_HR(f(&GUID_NULL, &IID_IUnknown, &p), G1_CLASS_E_CLASSNOTAVAILABLE);
    CHECK(p == NULL);
}
TEST(gap1_scache_dllgetclassobject_null_output) {
    dgco_fn f = dgco("d3dscache.dll"); REQUIRE(f);
    static const GUID bogus = { 0x5ab0c1d2, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };
    EXPECT_HR(f(&bogus, &IID_IClassFactory, NULL), G1_E_POINTER);
}

/* ======================= dxva2.dll ======================= */
static void make_manager(IDirect3DDeviceManager9 **m, UINT *tok) {
    *m = NULL; *tok = 0;
    if (FAILED(U(DXVA2CreateDirect3DDeviceManager9)(tok, m))) *m = NULL;
}
TEST(gap1_dxva2_manager_create_and_identity) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(U(DXVA2CreateDirect3DDeviceManager9));
    UINT t1 = 0, t2 = 0;
    IDirect3DDeviceManager9 *a = NULL, *b = NULL;
    EXPECT_HR(U(DXVA2CreateDirect3DDeviceManager9)(&t1, &a), S_OK);
    EXPECT_HR(U(DXVA2CreateDirect3DDeviceManager9)(&t2, &b), S_OK);
    CHECK(a != NULL && b != NULL && a != b);
    if (a) {
        G1_EQ(IDirect3DDeviceManager9_AddRef(a), 2);
        G1_EQ(IDirect3DDeviceManager9_Release(a), 1);
        IUnknown *u = NULL;
        EXPECT_HR(IDirect3DDeviceManager9_QueryInterface(a, &IID_IUnknown, (void **)&u), S_OK);
        if (u) IUnknown_Release(u);
        void *bad = GAP_MAGIC;
        EXPECT_HR(IDirect3DDeviceManager9_QueryInterface(a, &IID_ID3D11Device, &bad), E_NOINTERFACE);
        CHECK(bad == NULL);
    }
    t_note("reset tokens: %u %u", t1, t2);
    if (a) G1_EQ(IDirect3DDeviceManager9_Release(a), 0);
    if (b) G1_EQ(IDirect3DDeviceManager9_Release(b), 0);
}
TEST(gap1_dxva2_manager_uninitialized_returns_not_initialized) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(U(DXVA2CreateDirect3DDeviceManager9));
    IDirect3DDeviceManager9 *m; UINT tok; make_manager(&m, &tok);
    if (!m) { t_skip("no device manager"); return; }
    HANDLE h = GAP_MAGIC;
    EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(m, &h), DXVA2_E_NOT_INITIALIZED);
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(m, (HANDLE)(ULONG_PTR)1), DXVA2_E_NOT_INITIALIZED);
    void *svc = GAP_MAGIC;
    EXPECT_HR(IDirect3DDeviceManager9_GetVideoService(m, (HANDLE)(ULONG_PTR)1, &G1_IID_DecoderService, &svc), DXVA2_E_NOT_INITIALIZED);
    IDirect3DDeviceManager9_Release(m);
}
TEST(gap1_dxva2_manager_close_unknown_handle) {
    REQUIRE(U(DXVA2CreateDirect3DDeviceManager9));
    IDirect3DDeviceManager9 *m; UINT tok; make_manager(&m, &tok);
    if (!m) { t_skip("no device manager"); return; }
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(m, (HANDLE)(ULONG_PTR)1), G1_E_HANDLE);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(m, NULL), G1_E_HANDLE);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(m, (HANDLE)(ULONG_PTR)0xdeadbeef), G1_E_HANDLE);
    IDirect3DDeviceManager9_Release(m);
}
TEST(gap1_dxva2_manager_reset_with_wrong_token) {
    REQUIRE(U(DXVA2CreateDirect3DDeviceManager9));
    IDirect3DDeviceManager9 *m; UINT tok; make_manager(&m, &tok);
    if (!m) { t_skip("no device manager"); return; }
    /* the token is compared before the device is looked at */
    EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(m, NULL, tok + 1), G1_E_INVALIDARG);
    EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(m, NULL, ~tok), G1_E_INVALIDARG);
    EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(m, NULL, tok ^ 0x80000000u), G1_E_INVALIDARG);
    HANDLE h;
    EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(m, &h), DXVA2_E_NOT_INITIALIZED);   /* still uninitialized */
    IDirect3DDeviceManager9_Release(m);
}
TEST(gap1_dxva2_manager_handle_lifecycle_with_device) {
    REQUIRE(U(DXVA2CreateDirect3DDeviceManager9)); REQUIRE(U(DXVA2CreateVideoService));
    IDirect3D9 *d3d = NULL; IDirect3DDevice9 *dev = NULL; HWND hw = NULL; D3DDEVTYPE ty;
    if (FAILED(d3dh_make_device9(&d3d, &dev, NULL, &hw, &ty)) || !dev) { SAFE_RELEASE(d3d); d3dh_destroy_window(hw); t_skip("no D3D9 device"); return; }
    IDirect3DDeviceManager9 *m; UINT tok; make_manager(&m, &tok);
    if (!m) { SAFE_RELEASE(dev); SAFE_RELEASE(d3d); d3dh_destroy_window(hw); t_skip("no device manager"); return; }
    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(m, dev, tok);
    if (FAILED(hr)) {
        t_note("ResetDevice with a %d device -> %s (video acceleration service unavailable)", (int)ty, d3dh_hr_name(hr));
    } else {
        HANDLE h1 = NULL, h2 = NULL;
        EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(m, &h1), S_OK);
        EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(m, &h2), S_OK);
        CHECK(h1 != NULL && h2 != NULL && h1 != h2);
        EXPECT_HR(IDirect3DDeviceManager9_TestDevice(m, h1), S_OK);
        EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(m, h1), S_OK);
        EXPECT_HR(IDirect3DDeviceManager9_TestDevice(m, h1), G1_E_HANDLE);                /* closed handles are unknown */
        EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(m, h1), G1_E_HANDLE);
        /* resetting the device invalidates the handles that are still open */
        EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(m, dev, tok), S_OK);
        EXPECT_HR(IDirect3DDeviceManager9_TestDevice(m, h2), (HRESULT)0x80041001);        /* DXVA2_E_NEW_VIDEO_DEVICE */
        EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(m, h2), S_OK);
        IDirect3DDevice9 *lk = NULL; HANDLE h3 = NULL;
        EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(m, &h3), S_OK);
        if (h3) {
            EXPECT_HR(IDirect3DDeviceManager9_LockDevice(m, h3, &lk, TRUE), S_OK);
            CHECK(lk == dev);
            if (lk) { IDirect3DDeviceManager9_UnlockDevice(m, h3, FALSE); IDirect3DDevice9_Release(lk); }
            IDirect3DDeviceManager9_CloseDeviceHandle(m, h3);
        }
    }
    IDirect3DDeviceManager9_Release(m);
    SAFE_RELEASE(dev); SAFE_RELEASE(d3d); d3dh_destroy_window(hw);
}
TEST(gap1_dxva2_video_service_for_device) {
    REQUIRE(U(DXVA2CreateVideoService));
    IDirect3D9 *d3d = NULL; IDirect3DDevice9 *dev = NULL; HWND hw = NULL; D3DDEVTYPE ty;
    if (FAILED(d3dh_make_device9(&d3d, &dev, NULL, &hw, &ty)) || !dev) { SAFE_RELEASE(d3d); d3dh_destroy_window(hw); t_skip("no D3D9 device"); return; }
    void *svc = GAP_MAGIC;
    HRESULT hr = U(DXVA2CreateVideoService)(dev, &G1_IID_ProcessorService, &svc);
    EXPECT_HR(hr, S_OK);   /* Win10: the processor service is available (also on devtype 4) */
    if (SUCCEEDED(hr)) {
        CHECK(svc != NULL && svc != GAP_MAGIC);
        if (svc) IUnknown_Release((IUnknown *)svc);
        svc = GAP_MAGIC;
        IDirectXVideoDecoderService *ds = NULL;
        EXPECT_HR(U(DXVA2CreateVideoService)(dev, &G1_IID_DecoderService, (void **)&ds), S_OK);
        if (ds) {
            UINT n = 0xdead; GUID *g = NULL;
            HRESULT h2 = IDirectXVideoDecoderService_GetDecoderDeviceGuids(ds, &n, &g);
            if (SUCCEEDED(h2)) { t_note("decoder guid count %u", n); if (g) CoTaskMemFree(g); }
            IDirectXVideoDecoderService_Release(ds);
        }
        void *no = GAP_MAGIC;
        HRESULT h3 = U(DXVA2CreateVideoService)(dev, &IID_ID3D11Device, &no);
        EXPECT_HR_FAILED(h3);
        CHECK(no == NULL || no == GAP_MAGIC);
    }
    SAFE_RELEASE(dev); SAFE_RELEASE(d3d); d3dh_destroy_window(hw);
}

static DXVAHD_CONTENT_DESC good_desc(void) {
    DXVAHD_CONTENT_DESC d;
    memset(&d, 0, sizeof d);
    d.InputFrameFormat = DXVAHD_FRAME_FORMAT_PROGRESSIVE;
    d.InputFrameRate.Numerator = 30; d.InputFrameRate.Denominator = 1;
    d.InputWidth = 64; d.InputHeight = 64;
    d.OutputFrameRate.Numerator = 30; d.OutputFrameRate.Denominator = 1;
    d.OutputWidth = 64; d.OutputHeight = 64;
    return d;
}
TEST(gap1_dxvahd_rejects_invalid_content_description) {
    REQUIRE(U(DXVAHD_CreateDevice));
    IDirect3D9Ex *d3d = NULL; IDirect3DDevice9Ex *dev = NULL; HWND hw = NULL; D3DDEVTYPE ty;
    if (FAILED(d3dh_make_device9ex(&d3d, &dev, NULL, &hw, &ty)) || !dev) { SAFE_RELEASE(d3d); d3dh_destroy_window(hw); t_skip("no D3D9Ex device"); return; }
    struct { const char *what; DXVAHD_CONTENT_DESC d; DXVAHD_DEVICE_USAGE u; } cases[8];
    int n = 0;
#define CASE(w, mut, use) do { cases[n].what = w; cases[n].d = good_desc(); { DXVAHD_CONTENT_DESC *x = &cases[n].d; mut; } cases[n].u = (use); n++; } while (0)
    CASE("frame format 3", x->InputFrameFormat = (DXVAHD_FRAME_FORMAT)3, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL);
    CASE("input width 0", x->InputWidth = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL);
    CASE("input height 0", x->InputHeight = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL);
    CASE("output width 0", x->OutputWidth = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL);
    CASE("output height 0", x->OutputHeight = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL);
    CASE("input rate n/0", x->InputFrameRate.Denominator = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL);
    CASE("output rate n/0", x->OutputFrameRate.Denominator = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL);
    CASE("usage 3", (void)x, (DXVAHD_DEVICE_USAGE)3);
    for (int i = 0; i < n; i++) {
        IDXVAHD_Device *out = GAP_MAGIC;
        HRESULT hr = U(DXVAHD_CreateDevice)(dev, &cases[i].d, cases[i].u, NULL, &out);
        CHECKF(hr == G1_E_INVALIDARG, "%s -> %s", cases[i].what, d3dh_hr_name(hr));
        CHECKF(out == GAP_MAGIC, "%s: output written on failure", cases[i].what);
    }
    SAFE_RELEASE(dev); SAFE_RELEASE(d3d); d3dh_destroy_window(hw);
}
TEST(gap1_dxvahd_valid_description_observed) {
    REQUIRE(U(DXVAHD_CreateDevice));
    IDirect3D9Ex *d3d = NULL; IDirect3DDevice9Ex *dev = NULL; HWND hw = NULL; D3DDEVTYPE ty;
    if (FAILED(d3dh_make_device9ex(&d3d, &dev, NULL, &hw, &ty)) || !dev) { SAFE_RELEASE(d3d); d3dh_destroy_window(hw); t_skip("no D3D9Ex device"); return; }
    DXVAHD_CONTENT_DESC d = good_desc();
    IDXVAHD_Device *out = NULL;
    HRESULT hr = U(DXVAHD_CreateDevice)(dev, &d, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &out);
    /* Win10, Basic Display Adapter / null-ref device (devtype 4): there is no DXVA-HD device, E_NOINTERFACE. Hardware drivers return S_OK. */
    if (FAILED(hr)) EXPECT_HR(hr, G1_E_NOINTERFACE);
    if (SUCCEEDED(hr)) {
        CHECK(out != NULL);
        if (out) {
            DXVAHD_VPDEVCAPS caps; memset(&caps, 0, sizeof caps);
            HRESULT hc = IDXVAHD_Device_GetVideoProcessorDeviceCaps(out, &caps);
            if (SUCCEEDED(hc)) CHECK(caps.InputPool == D3DPOOL_DEFAULT || caps.InputPool == D3DPOOL_SYSTEMMEM || caps.InputPool == D3DPOOL_MANAGED);
            IDXVAHD_Device_Release(out);
        }
    } else CHECK(out == NULL);
    SAFE_RELEASE(dev); SAFE_RELEASE(d3d); d3dh_destroy_window(hw);
}

/* ======================= dxcore.dll ======================= */
UDECL(HRESULT, DXCoreCreateAdapterFactory, (REFIID, void **));
static const GUID IID_DXCoreFactory = { 0x78ee5945, 0xc36e, 0x4b13, { 0xa6, 0x69, 0x00, 0x5d, 0xd1, 0x1c, 0x0f, 0x06 } };

TEST(gap1_dxcore_factory_unknown_interface) {
    REQUIRE(UN(DXCoreCreateAdapterFactory));
    SKIP_ON_WINE("dxcore not provided");
    void *p = GAP_MAGIC;
    EXPECT_HR(UN(DXCoreCreateAdapterFactory)(&IID_ID3D11Device, &p), G1_E_NOINTERFACE);
    CHECK(p == NULL);
    p = GAP_MAGIC;
    EXPECT_HR(UN(DXCoreCreateAdapterFactory)(&GUID_NULL, &p), G1_E_NOINTERFACE);
    CHECK(p == NULL);
}
TEST(gap1_dxcore_factory_iunknown_identity) {
    REQUIRE(UN(DXCoreCreateAdapterFactory));
    SKIP_ON_WINE("dxcore not provided");
    IUnknown *f = NULL, *u1 = NULL, *u2 = NULL;
    HRESULT hr = UN(DXCoreCreateAdapterFactory)(&IID_DXCoreFactory, (void **)&f);
    if (FAILED(hr) || !f) { t_skip("factory -> %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(IUnknown_QueryInterface(f, &IID_IUnknown, (void **)&u1), S_OK);
    EXPECT_HR(IUnknown_QueryInterface(f, &IID_IUnknown, (void **)&u2), S_OK);
    CHECK(u1 != NULL && u1 == u2);
    void *bad = GAP_MAGIC;
    EXPECT_HR(IUnknown_QueryInterface(f, &IID_ID3D11Device, &bad), E_NOINTERFACE);
    CHECK(bad == NULL);
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    IUnknown_Release(f);
}
TEST(gap1_dxcore_factory_refcount_balanced) {
    REQUIRE(UN(DXCoreCreateAdapterFactory));
    SKIP_ON_WINE("dxcore not provided");
    IUnknown *f = NULL;
    if (FAILED(UN(DXCoreCreateAdapterFactory)(&IID_DXCoreFactory, (void **)&f)) || !f) { t_skip("no factory"); return; }
    ULONG a = IUnknown_AddRef(f), r = IUnknown_Release(f);
    CHECK(a == r + 1);
    IUnknown_Release(f);
}

/* ======================= d3d9.dll PIX entry points ======================= */
TEST(gap1_d3dperf_query_repeat_frame_is_false) {
    REQUIRE(UIN("d3d9.dll", D3DPERF_QueryRepeatFrame));
    EXPECT_EQ(UIN("d3d9.dll", D3DPERF_QueryRepeatFrame)(), FALSE);
    EXPECT_EQ(UIN("d3d9.dll", D3DPERF_QueryRepeatFrame)(), FALSE);
}
TEST(gap1_d3dperf_markers_are_harmless_without_a_profiler) {
    REQUIRE(UIN("d3d9.dll", D3DPERF_SetMarker)); REQUIRE(UIN("d3d9.dll", D3DPERF_SetRegion)); REQUIRE(UIN("d3d9.dll", D3DPERF_GetStatus));
    DWORD before = UIN("d3d9.dll", D3DPERF_GetStatus)();
    UIN("d3d9.dll", D3DPERF_SetMarker)(0xffff0000, L"gap1 marker");
    UIN("d3d9.dll", D3DPERF_SetMarker)(0, NULL);
    UIN("d3d9.dll", D3DPERF_SetRegion)(0xff00ff00, L"gap1 region");
    UIN("d3d9.dll", D3DPERF_SetRegion)(0, NULL);
    EXPECT_EQ(UIN("d3d9.dll", D3DPERF_GetStatus)(), before);
}
TEST(gap1_d3dperf_events_without_profiler_return_minus_one) {
    SKIP_ON_WINE("wine ddraw/dxva2/d3d9 behaviour differs");
    REQUIRE(UIN("d3d9.dll", D3DPERF_BeginEvent)); REQUIRE(UIN("d3d9.dll", D3DPERF_EndEvent)); REQUIRE(UIN("d3d9.dll", D3DPERF_GetStatus));
    if (UIN("d3d9.dll", D3DPERF_GetStatus)()) { t_skip("a profiler is attached"); return; }
    EXPECT_EQ(UIN("d3d9.dll", D3DPERF_BeginEvent)(0xff0000ff, L"gap1 event"), -1);
    EXPECT_EQ(UIN("d3d9.dll", D3DPERF_BeginEvent)(0, NULL), -1);
    EXPECT_EQ(UIN("d3d9.dll", D3DPERF_EndEvent)(), -1);
}
