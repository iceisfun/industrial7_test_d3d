/* COM-server style exports (DllGetClassObject / DllCanUnloadNow) of ddraw and d3dscache, and the dxcore
 * ordinal forwarders to win32u. */
#include "xm.h"
#include <unknwn.h>
#include <ddraw.h>

typedef HRESULT (WINAPI *pfn_gco)(const GUID *, const GUID *, void **);
typedef HRESULT (WINAPI *pfn_cun)(void);

#define E_FAIL_ ((HRESULT)0x80004005)
#define E_NOINTERFACE_ ((HRESULT)0x80004002)
#define E_POINTER_ ((HRESULT)0x80004003)
#define E_INVALIDARG_ ((HRESULT)0x80070057)
#define WINE_SKIP() do { if (t_is_wine()) { t_skip("emulated COM server differs"); return; } } while (0)
#define CLASS_E_CLASSNOTAVAILABLE_ ((HRESULT)0x80040111)

static pfn_gco ddraw_gco(void) { if (t_is_wine()) return NULL; HMODULE m = LoadLibraryA("ddraw.dll"); return m ? (pfn_gco)GetProcAddress(m, "DllGetClassObject") : NULL; }

TEST(exports_modules_ddraw_getclassobject_null_arguments) {
    pfn_gco gco = ddraw_gco(); REQUIRE(gco);
    void *p = (void *)(ULONG_PTR)1;
    EXPECT_EQ(gco(&CLSID_DirectDraw, &IID_IClassFactory, NULL), E_INVALIDARG_);
    EXPECT_EQ(gco(NULL, &IID_IClassFactory, &p), E_INVALIDARG_);
    EXPECT_EQ(p, NULL);                                   /* *ppv is cleared before validation continues */
    p = (void *)(ULONG_PTR)1;
    EXPECT_EQ(gco(&CLSID_DirectDraw, NULL, &p), E_INVALIDARG_);
    EXPECT_EQ(p, NULL);
}
TEST(exports_modules_ddraw_getclassobject_unknown_class) {
    pfn_gco gco = ddraw_gco(); REQUIRE(gco);
    static const GUID *const bad[] = { &IID_NULL, &IID_IUnknown, &IID_IDirectDraw, &IID_IDirectDraw7 };
    for (int k = 0; k < 4; k++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = gco(bad[k], &IID_IClassFactory, &p);
        CHECKF(hr == E_FAIL_, "clsid #%d: 0x%lx", k, hr);
        EXPECT_EQ(p, NULL);
    }
}
TEST(exports_modules_ddraw_getclassobject_unsupported_interface) {
    pfn_gco gco = ddraw_gco(); REQUIRE(gco);
    static const GUID *const iids[] = { &IID_IDirectDraw, &IID_IDirectDraw7, &IID_NULL, &CLSID_DirectDraw };
    for (int k = 0; k < 4; k++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = gco(&CLSID_DirectDraw, iids[k], &p);
        CHECKF(hr == E_NOINTERFACE_, "iid #%d: 0x%lx", k, hr);
        EXPECT_EQ(p, NULL);
    }
}
TEST(exports_modules_ddraw_getclassobject_factories) {
    pfn_gco gco = ddraw_gco(); REQUIRE(gco);
    static const GUID *const cl[] = { &CLSID_DirectDraw, &CLSID_DirectDraw7, &CLSID_DirectDrawClipper };
    static const GUID *const ii[] = { &IID_IClassFactory, &IID_IUnknown };
    for (int c = 0; c < 3; c++) for (int i = 0; i < 2; i++) {
        void *p = NULL;
        HRESULT hr = gco(cl[c], ii[i], &p);
        CHECKF(hr == S_OK && p != NULL, "clsid %d iid %d: 0x%lx", c, i, hr);
        if (hr != S_OK || !p) continue;
        IUnknown *u = (IUnknown *)p;
        /* the factory answers IUnknown and IClassFactory only */
        void *q = NULL;
        EXPECT_EQ(u->lpVtbl->QueryInterface(u, &IID_IClassFactory, &q), S_OK);
        if (q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q);
        q = (void *)(ULONG_PTR)1;
        HRESULT hr2 = u->lpVtbl->QueryInterface(u, &IID_IDirectDraw, &q);
        CHECKF(hr2 == E_NOINTERFACE_, "factory QI(IDirectDraw) = 0x%lx", hr2);
        EXPECT_EQ(q, NULL);
        ULONG r = u->lpVtbl->Release(u);
        CHECKF(r == 0, "release returns %lu", r);
    }
}
TEST(exports_modules_ddraw_getclassobject_holds_module_lock) {
    pfn_gco gco = ddraw_gco(); REQUIRE(gco);
    HMODULE m = GetModuleHandleA("ddraw.dll");
    pfn_cun cun = (pfn_cun)GetProcAddress(m, "DllCanUnloadNow"); REQUIRE(cun);
    void *p = NULL;
    HRESULT hr = gco(&CLSID_DirectDraw, &IID_IClassFactory, &p);
    EXPECT_EQ(hr, S_OK);
    if (hr != S_OK) return;
    EXPECT_EQ(cun(), S_FALSE);                            /* an outstanding class factory pins the DLL */
    ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    HRESULT after = cun();
    t_note("ddraw DllCanUnloadNow after releasing the factory: 0x%lx", after);
}

CHILD(xm_ddraw_lock) {
    HMODULE m = LoadLibraryA("ddraw.dll"); if (!m) return 77;
    pfn_cun cun = (pfn_cun)GetProcAddress(m, "DllCanUnloadNow");
    pfn_gco gco = (pfn_gco)GetProcAddress(m, "DllGetClassObject");
    if (!cun || !gco) return 78;
    int rc = 0;
    if (cun() != S_OK) rc |= 1;                           /* nothing created yet */
    void *p = NULL;
    if (gco(&CLSID_DirectDrawClipper, &IID_IClassFactory, &p) != S_OK || !p) return 2;
    if (cun() != S_FALSE) rc |= 4;
    ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    if (cun() != S_OK) rc |= 8;
    return rc;
}
TEST(exports_modules_ddraw_canunloadnow_lifecycle) {
    DWORD rc = t_run_child("xm_ddraw_lock", "", 20000);
    if (rc == 77 || rc == 78) { t_skip("ddraw unavailable"); return; }
    if (t_is_wine() && rc) { t_note("emulator: rc %lu", rc); return; }
    CHECKF(rc != 2, "factory creation failed in child");
    CHECKF(!(rc & 4), "S_FALSE expected while a class factory is outstanding");
    if (rc & 1) t_note("fresh-process DllCanUnloadNow was not S_OK");
    if (rc & 8) t_note("DllCanUnloadNow not S_OK after the last factory was released");
}

/* ---- d3dscache ------------------------------------------------------------------------------ */
static HMODULE sc(void) { return t_is_wine() ? NULL : LoadLibraryA("d3dscache.dll"); }
TEST(exports_modules_d3dscache_getclassobject_null_ppv) {
    HMODULE m = sc(); REQUIRE(m);
    pfn_gco gco = (pfn_gco)GetProcAddress(m, "DllGetClassObject"); REQUIRE(gco);
    EXPECT_EQ(gco(&CLSID_DirectDraw, &IID_IClassFactory, NULL), E_POINTER_);
}
TEST(exports_modules_d3dscache_getclassobject_unknown_class) {
    HMODULE m = sc(); REQUIRE(m);
    pfn_gco gco = (pfn_gco)GetProcAddress(m, "DllGetClassObject"); REQUIRE(gco);
    static const GUID *const bad[] = { &IID_NULL, &IID_IUnknown, &CLSID_DirectDraw, &IID_IClassFactory };
    for (int k = 0; k < 4; k++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = gco(bad[k], &IID_IUnknown, &p);
        CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE_, "clsid #%d: 0x%lx", k, hr);
        EXPECT_EQ(p, NULL);
    }
}
CHILD(xm_scache_unload) {
    HMODULE m = LoadLibraryA("d3dscache.dll"); if (!m) return 77;
    pfn_cun cun = (pfn_cun)GetProcAddress(m, "DllCanUnloadNow"); if (!cun) return 78;
    HRESULT hr = cun();
    return hr == S_OK ? 0 : hr == S_FALSE ? 1 : 2;
}
TEST(exports_modules_d3dscache_canunloadnow_fresh_process) {
    DWORD rc = t_run_child("xm_scache_unload", "", 20000);
    if (rc == 77 || rc == 78) { t_skip("d3dscache unavailable"); return; }
    if (t_is_wine() && rc) { t_note("emulator: rc %lu", rc); return; }
    CHECKF(rc <= 1, "DllCanUnloadNow must return S_OK or S_FALSE");
    if (rc == 1) t_note("d3dscache reports S_FALSE (module lock held) in a fresh process");
}

/* ---- dxcore ------------------------------------------------------------------------------------ */
TEST(exports_modules_dxcore_forwarders_match_win32u) {
    WINE_SKIP();
    HMODULE dx = LoadLibraryA("dxcore.dll"), w32 = LoadLibraryA("win32u.dll");
    REQUIRE(dx); REQUIRE(w32);
    int fwd = 0;
    for (int k = 0; k < g_xm[XM_dxcore].n; k++) {
        const xexp *e = &g_xm[XM_dxcore].e[k];
        if (!e->fwd) continue;
        CHECKF(!strncmp(e->fwd, "WIN32U.", 7), "ordinal %u forwarder %s", e->ord, e->fwd);
        FARPROC a = GetProcAddress(dx, (LPCSTR)(ULONG_PTR)e->ord), b = GetProcAddress(w32, e->fwd + 7);
        CHECKF(a != NULL && a == b, "dxcore ordinal %u resolves to win32u!%s (%p vs %p)", e->ord, e->fwd + 7, a, b);
        fwd++;
    }
    CHECK(fwd >= 150);
    t_note("%d dxcore ordinals forward to win32u", fwd);
}
TEST(exports_modules_dxcore_only_one_named_export) {
    WINE_SKIP();
    HMODULE dx = LoadLibraryA("dxcore.dll"); REQUIRE(dx);
    const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(dx, NULL);
    CHECK(ed != NULL);
    if (ed) EXPECT_EQ(ed->NumberOfNames, 1);
    const char *nm = xm_name_of_ordinal(dx, 211);
    CHECK(nm != NULL && !strcmp(nm, "DXCoreCreateAdapterFactory"));
    EXPECT_EQ(GetProcAddress(dx, "DXCoreCreateAdapterFactory"), GetProcAddress(dx, (LPCSTR)(ULONG_PTR)211));
}
TEST(exports_modules_dxcore_unnamed_local_ordinals) {
    WINE_SKIP();
    HMODULE dx = LoadLibraryA("dxcore.dll"); REQUIRE(dx);
    /* ordinals with local code (not forwarders) resolve inside dxcore itself */
    int n = 0;
    for (int k = 0; k < g_xm[XM_dxcore].n; k++) {
        const xexp *e = &g_xm[XM_dxcore].e[k];
        if (e->fwd) continue;
        FARPROC p = GetProcAddress(dx, (LPCSTR)(ULONG_PTR)e->ord);
        CHECKF(p != NULL && xm_in_image(dx, p) && xm_is_exec(dx, p), "ordinal %u", e->ord);
        n++;
    }
    CHECK(n >= 10);
}
