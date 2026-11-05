/* Behaviour of the small exports whose bodies are fully known: shared no-op thunks, E_NOTIMPL stubs, version
 * constants, profile strings, performance-marker entry points and the D3DKMT compatibility shims. */
#include "xm.h"

#define WINE_SKIP() do { if (t_is_wine()) { t_skip("calls an export the emulator leaves unimplemented"); return; } } while (0)
#define LOADED(dll) HMODULE hm_ = LoadLibraryA(dll); WINE_SKIP(); do { if (!hm_) { t_skip("%s not present", dll); return; } } while (0)
#define FN(dll, name, type) type name##_fn = (type)t_proc_in(dll, #name); do { if (!name##_fn) { t_skip("missing export %s!%s", dll, #name); return; } } while (0)
#define STATUS_INVALID_PARAMETER_ ((LONG)0xC000000D)
#define E_NOTIMPL_ ((LONG)0x80004001)

typedef LONG (WINAPI *pfn_ret_p)(void *);
typedef LONG (WINAPI *pfn_ret_ppp)(void *, void *, void *);

static int find_idx(const char *dll) { for (int i = 0; i < XM_COUNT; i++) if (!strcmp(g_xm[i].dll, dll)) return i; return -1; }
static const xexp *find_exp(int mod, const char *name) {
    for (int k = 0; k < g_xm[mod].n; k++) if (g_xm[mod].e[k].name && !strcmp(g_xm[mod].e[k].name, name)) return &g_xm[mod].e[k];
    return NULL;
}

/* --- the shared "return STATUS_SUCCESS" thunk behind most D3DKMT exports of d3d11/d3d10core/d3d10_1core --- */
static void stub_group(const char *dll, const char *anchor) {
    LOADED(dll);
    int mi = find_idx(dll);
    const xexp *a = find_exp(mi, anchor);
    FARPROC pa = GetProcAddress(hm_, anchor);
    CHECK(a != NULL && pa != NULL);
    if (!a || !pa) return;
    int members = 0;
    for (int k = 0; k < g_xm[mi].n; k++) {
        const xexp *e = &g_xm[mi].e[k];
        if (!e->name || e->rva != a->rva) continue;
        FARPROC p = GetProcAddress(hm_, e->name);
        members++;
        CHECKF(p == pa, "%s!%s shares one code address with %s", dll, e->name, anchor);
        if (p == pa) {
            /* xor eax,eax / ret: arguments are never read, the status is STATUS_SUCCESS */
            LONG r = ((pfn_ret_p)p)(NULL);
            CHECKF(r == 0, "%s!%s(NULL) returned 0x%lx", dll, e->name, r);
            r = ((pfn_ret_ppp)p)((void *)(ULONG_PTR)1, NULL, NULL);
            CHECKF(r == 0, "%s!%s returns 0 regardless of arguments", dll, e->name);
        }
    }
    t_note("%s: %d exports share the success thunk", dll, members);
    CHECK(members >= 20);
}
TEST(exports_modules_d3d11_d3dkmt_success_thunk) { stub_group("d3d11.dll", "D3DKMTCloseAdapter"); }
TEST(exports_modules_d3d10core_d3dkmt_success_thunk) { stub_group("d3d10core.dll", "D3DKMTCloseAdapter"); }
TEST(exports_modules_d3d10_1core_d3dkmt_success_thunk) { stub_group("d3d10_1core.dll", "D3DKMTCloseAdapter"); }

static void invalid_param_group(const char *dll) {
    LOADED(dll);
    static const char *const names[] = { "D3DKMTOpenResource", "D3DKMTQueryResourceInfo" };
    FARPROC first = NULL;
    for (int k = 0; k < 2; k++) {
        FARPROC p = GetProcAddress(hm_, names[k]);
        CHECKF(p != NULL, "%s!%s", dll, names[k]);
        if (!p) continue;
        if (!first) first = p; else CHECK(p == first);
        LONG r = ((pfn_ret_p)p)(NULL);
        CHECKF(r == STATUS_INVALID_PARAMETER_, "%s!%s returned 0x%lx", dll, names[k], r);
        FARPROC succ = GetProcAddress(hm_, "D3DKMTCloseAdapter");
        CHECK(p != succ);
    }
}
TEST(exports_modules_d3d11_d3dkmt_openresource_rejects) { invalid_param_group("d3d11.dll"); }
TEST(exports_modules_d3d10core_d3dkmt_openresource_rejects) { invalid_param_group("d3d10core.dll"); }
TEST(exports_modules_d3d10_1core_d3dkmt_openresource_rejects) { invalid_param_group("d3d10_1core.dll"); }

/* --- d3d11 own D3DKMT emulations ------------------------------------------------------------ */
TEST(exports_modules_d3d11_d3dkmt_getdevicestate) {
    LOADED("d3d11.dll");
    FN("d3d11.dll", D3DKMTGetDeviceState, pfn_ret_p);
    CHECK((void *)D3DKMTGetDeviceState_fn != (void *)GetProcAddress(hm_, "D3DKMTCloseAdapter"));
    BYTE b[0x48];
    /* StateType 1 (execution): state = 1 (active) */
    memset(b, 0xCC, sizeof b); *(DWORD *)(b + 4) = 1;
    EXPECT_EQ(D3DKMTGetDeviceState_fn(b), 0);
    EXPECT_EQ(*(DWORD *)(b + 8), 1);
    EXPECT_EQ(b[0x0c], 0xCC);                          /* only the first dword of the union is written */
    /* StateType 2 (present): 0x28 bytes cleared */
    memset(b, 0xCC, sizeof b); *(DWORD *)(b + 4) = 2;
    EXPECT_EQ(D3DKMTGetDeviceState_fn(b), 0);
    for (int k = 8; k < 0x30; k++) if (b[k] != 0) { CHECKF(0, "byte +0x%x not cleared", k); break; }
    EXPECT_EQ(b[0x30], 0xCC);
    EXPECT_EQ(b[0x07], 0xCC);
    /* any other type is unsupported: STATUS_UNSUCCESSFUL, buffer untouched */
    static const DWORD others[] = { 0, 3, 4, 5, 100 };
    for (int k = 0; k < 5; k++) {
        memset(b, 0xCC, sizeof b); *(DWORD *)(b + 4) = others[k];
        LONG r = D3DKMTGetDeviceState_fn(b);
        CHECKF(r == (LONG)0xC0000001, "type %lu returned 0x%lx", others[k], r);
        CHECK(b[8] == 0xCC && b[0x20] == 0xCC);
    }
}
TEST(exports_modules_d3d11_d3dkmt_escape_type_21) {
    LOADED("d3d11.dll");
    FN("d3d11.dll", D3DKMTEscape, pfn_ret_p);
    BYTE flag = 0; BYTE b[0x40]; memset(b, 0, sizeof b);
    *(DWORD *)(b + 8) = 0x15;
    *(void **)(b + 0x10) = &flag;
    EXPECT_EQ(D3DKMTEscape_fn(b), 0);
    EXPECT_EQ(flag, 1);
    CHECK((void *)D3DKMTEscape_fn != (void *)GetProcAddress(hm_, "D3DKMTCloseAdapter"));
}
TEST(exports_modules_d3d11_d3dkmt_openadapterfromhdc) {
    LOADED("d3d11.dll");
    FN("d3d11.dll", D3DKMTOpenAdapterFromHdc, pfn_ret_p);
    BYTE b1[0x20], b2[0x20];
    memset(b1, 0, sizeof b1); memset(b2, 0, sizeof b2);
    EXPECT_EQ(D3DKMTOpenAdapterFromHdc_fn(b1), 0);
    EXPECT_EQ(D3DKMTOpenAdapterFromHdc_fn(b2), 0);
    EXPECT_EQ(*(DWORD *)(b1 + 8), 0xf0f);               /* fixed pseudo adapter handle */
    EXPECT_EQ(*(DWORD *)(b2 + 8), 0xf0f);
    const LUID *l1 = (const LUID *)(b1 + 0xc), *l2 = (const LUID *)(b2 + 0xc);
    CHECK(l1->LowPart != 0 || l1->HighPart != 0);       /* a freshly allocated locally unique id */
    CHECK(l1->LowPart != l2->LowPart || l1->HighPart != l2->HighPart);
    EXPECT_EQ(*(DWORD *)(b1 + 0), 0);                   /* hDc untouched */
}
TEST(exports_modules_d3d11_openadapter10_fills_function_table) {
    LOADED("d3d11.dll");
    FN("d3d11.dll", OpenAdapter10, pfn_ret_p);
    void *tbl[4] = { 0 };
    BYTE a[0x30]; memset(a, 0, sizeof a);
    *(void ***)(a + 0x20) = tbl;                        /* D3D10DDIARG_OPENADAPTER::pAdapterFuncs */
    *(DWORD *)(a + 0x10) = 0x1234;                      /* Interface: left alone */
    *(DWORD *)(a + 0x14) = 0x5678;                      /* Version: left alone */
    EXPECT_EQ(OpenAdapter10_fn(a), 0);
    EXPECT_EQ(*(ULONGLONG *)(a + 8), 0xf07);            /* hAdapter */
    EXPECT_EQ(*(DWORD *)(a + 0x10), 0x1234);
    EXPECT_EQ(*(DWORD *)(a + 0x14), 0x5678);
    for (int k = 0; k < 3; k++) {
        CHECKF(tbl[k] != NULL && xm_in_image(hm_, tbl[k]) && xm_is_exec(hm_, tbl[k]), "function %d points into d3d11 code (%p)", k, tbl[k]);
    }
    EXPECT_EQ(tbl[3], NULL);                            /* exactly three callbacks */
    if (tbl[2] != (void *)GetProcAddress(hm_, "D3DKMTCloseAdapter")) t_note("OpenAdapter10 callback 2 is not the D3DKMTCloseAdapter thunk");
}

/* --- performance markers --------------------------------------------------------------------- */
typedef int (WINAPI *pfn_begin)(void *, const WCHAR *);
typedef int (WINAPI *pfn_end)(void *);
typedef void (WINAPI *pfn_marker)(void *, const WCHAR *);
typedef int (WINAPI *pfn_status)(void *);
TEST(exports_modules_d3d11_performance_markers_null_context) {
    LOADED("d3d11.dll");
    FN("d3d11.dll", D3DPerformance_BeginEvent, pfn_begin);
    FN("d3d11.dll", D3DPerformance_EndEvent, pfn_end);
    FN("d3d11.dll", D3DPerformance_SetMarker, pfn_marker);
    FN("d3d11.dll", D3DPerformance_GetStatus, pfn_status);
    EXPECT_EQ(D3DPerformance_BeginEvent_fn(NULL, L"event"), -1);   /* no annotation interface: negative level */
    EXPECT_EQ(D3DPerformance_EndEvent_fn(NULL), -1);
    EXPECT_EQ(D3DPerformance_GetStatus_fn(NULL), 0);               /* no profiler attached */
    D3DPerformance_SetMarker_fn(NULL, L"marker");                   /* silently ignored */
    D3DPerformance_SetMarker_fn(NULL, NULL);
    EXPECT_EQ(D3DPerformance_BeginEvent_fn(NULL, NULL), -1);
}
TEST(exports_modules_d3d9_d3dperf_without_profiler) {
    LOADED("d3d9.dll");
    typedef int (WINAPI *pb)(DWORD, const WCHAR *);
    typedef int (WINAPI *pe)(void);
    typedef void (WINAPI *ps)(DWORD, const WCHAR *);
    typedef DWORD (WINAPI *pg)(void);
    typedef BOOL (WINAPI *pq)(void);
    typedef void (WINAPI *po)(DWORD);
    FN("d3d9.dll", D3DPERF_BeginEvent, pb);
    FN("d3d9.dll", D3DPERF_EndEvent, pe);
    FN("d3d9.dll", D3DPERF_SetMarker, ps);
    FN("d3d9.dll", D3DPERF_SetRegion, ps);
    FN("d3d9.dll", D3DPERF_GetStatus, pg);
    FN("d3d9.dll", D3DPERF_QueryRepeatFrame, pq);
    FN("d3d9.dll", D3DPERF_SetOptions, po);
    DWORD st = D3DPERF_GetStatus_fn() & 0xff;   /* returns bool: only AL is defined */
    t_note("D3DPERF_GetStatus = %lu", st);
    int b = D3DPERF_BeginEvent_fn(0xff00ff00, L"scope");
    if (st == 0) CHECKF(b == -1, "BeginEvent without a profiler returns -1, got %d", b);
    int e = D3DPERF_EndEvent_fn();
    if (st == 0) t_note("EndEvent returned %d", e);
    D3DPERF_SetMarker_fn(0xffffffff, L"marker");
    D3DPERF_SetRegion_fn(0xffffffff, L"region");
    CHECK(D3DPERF_QueryRepeatFrame_fn() == FALSE);
    D3DPERF_SetOptions_fn(0);
    CHECK(D3DPERF_QueryRepeatFrame_fn() == FALSE);
    typedef void (WINAPI *pv)(DWORD);
    pv dm = (pv)GetProcAddress(hm_, "DebugSetMute"), dl = (pv)GetProcAddress(hm_, "DebugSetLevel");
    CHECK(dm && dl);
    if (dm) dm(1);
    if (dl) dl(0);
}

/* --- E_NOTIMPL stubs and version constants ---------------------------------------------------- */
static void notimpl3(const char *dll, const char *a, const char *b, const char *c) {
    LOADED(dll);
    const char *n[3] = { a, b, c };
    FARPROC p[3];
    for (int k = 0; k < 3; k++) { p[k] = GetProcAddress(hm_, n[k]); CHECKF(p[k] != NULL, "%s!%s", dll, n[k]); }
    CHECK(p[0] == p[1] && p[1] == p[2]);
    for (int k = 0; k < 3; k++) {
        if (!p[k]) continue;
        LONG r = ((pfn_ret_ppp)p[k])(NULL, NULL, NULL);
        CHECKF(r == E_NOTIMPL_, "%s!%s returned 0x%lx", dll, n[k], r);
    }
}
TEST(exports_modules_d3d10core_core_entry_points_not_implemented) { notimpl3("d3d10core.dll", "D3D10CoreCreateDevice", "D3D10CoreGetSupportedVersions", "D3D10CoreRegisterLayers"); }
TEST(exports_modules_d3d10_1core_core_entry_points_not_implemented) { notimpl3("d3d10_1core.dll", "D3D10CoreCreateDevice1", "D3D10CoreGetSupportedVersions", "D3D10CoreRegisterLayers"); }
TEST(exports_modules_dxgi_layered_device_entry_points_not_implemented) { notimpl3("dxgi.dll", "DXGID3D10CreateDevice", "DXGID3D10CreateLayeredDevice", "DXGID3D10RegisterLayers"); }
TEST(exports_modules_dxgi_pix_capture_thunk_returns_zero) {
    LOADED("dxgi.dll");
    static const char *const n[] = { "PIXBeginCapture", "PIXEndCapture", "PIXGetCaptureState", "DXGID3D10GetLayeredDeviceSize" };
    FARPROC first = NULL;
    for (int k = 0; k < 4; k++) {
        FARPROC p = GetProcAddress(hm_, n[k]);
        CHECKF(p != NULL, "%s", n[k]);
        if (!p) continue;
        if (!first) first = p; else CHECKF(p == first, "%s shares the thunk", n[k]);
        CHECKF(((pfn_ret_ppp)p)(NULL, NULL, NULL) == 0, "%s", n[k]);
    }
    FARPROC ni = GetProcAddress(hm_, "DXGID3D10CreateDevice");
    CHECK(ni != first);
}

typedef ULONGLONG (WINAPI *pfn_ver)(void);
TEST(exports_modules_d3d10core_version_constant) {
    LOADED("d3d10core.dll");
    FN("d3d10core.dll", D3D10CoreGetVersion, pfn_ver);
    EXPECT_EQ(D3D10CoreGetVersion_fn(), 0x000a000100041770ULL);
    EXPECT_EQ(D3D10CoreGetVersion_fn(), D3D10CoreGetVersion_fn());
}
TEST(exports_modules_d3d10_1core_version_constant) {
    LOADED("d3d10_1core.dll");
    FN("d3d10_1core.dll", D3D10CoreGetVersion, pfn_ver);
    EXPECT_EQ(D3D10CoreGetVersion_fn(), 0x000a000100041770ULL);
}
TEST(exports_modules_d3d10_getversion_forwards_to_core) {
    WINE_SKIP();
    LOADED("d3d10.dll");
    FN("d3d10.dll", D3D10GetVersion, pfn_ver);
    HMODULE core = LoadLibraryA("d3d10core.dll");
    if (!core) { t_skip("d3d10core missing"); return; }
    pfn_ver cv = (pfn_ver)GetProcAddress(core, "D3D10CoreGetVersion");
    CHECK(cv != NULL);
    if (cv) EXPECT_EQ(D3D10GetVersion_fn(), cv());
    EXPECT_EQ(D3D10GetVersion_fn(), 0x000a000100041770ULL);
    FN("d3d10.dll", D3D10RegisterLayers, pfn_ret_ppp);
    EXPECT_EQ(D3D10RegisterLayers_fn(NULL, NULL, NULL), E_NOTIMPL_);
}
TEST(exports_modules_d3d10_1_getversion_forwards_to_core) {
    WINE_SKIP();
    LOADED("d3d10_1.dll");
    FN("d3d10_1.dll", D3D10GetVersion, pfn_ver);
    EXPECT_EQ(D3D10GetVersion_fn(), 0x000a000100041770ULL);
    FN("d3d10_1.dll", D3D10RegisterLayers, pfn_ret_ppp);
    EXPECT_EQ(D3D10RegisterLayers_fn(NULL, NULL, NULL), E_NOTIMPL_);
}
typedef const char *(WINAPI *pfn_profile)(void *);
TEST(exports_modules_d3d10_shader_profile_strings) {
    LOADED("d3d10.dll");
    FN("d3d10.dll", D3D10GetPixelShaderProfile, pfn_profile);
    FN("d3d10.dll", D3D10GetVertexShaderProfile, pfn_profile);
    FN("d3d10.dll", D3D10GetGeometryShaderProfile, pfn_profile);
    const char *p = D3D10GetPixelShaderProfile_fn(NULL), *v = D3D10GetVertexShaderProfile_fn(NULL), *g = D3D10GetGeometryShaderProfile_fn(NULL);
    CHECK(p && v && g);
    if (!p || !v || !g) return;
    EXPECT_STR(p, "ps_4_0");
    EXPECT_STR(v, "vs_4_0");
    EXPECT_STR(g, "gs_4_0");
    CHECK(xm_in_image(hm_, p) && xm_in_image(hm_, v) && xm_in_image(hm_, g));   /* constant data of d3d10.dll */
    CHECK(!xm_is_exec(hm_, p));
}

/* --- d3d10level9 --------------------------------------------------------------------------- */
TEST(exports_modules_d3d10level9_thunk_version_is_four) {
    LOADED("d3d10level9.dll");
    typedef UINT (WINAPI *pv)(void);
    FN("d3d10level9.dll", D3DKMTGetThunkVersion, pv);
    EXPECT_EQ(D3DKMTGetThunkVersion_fn(), 4);
    FN("d3d10level9.dll", D3DKMTGetSharedPrimaryHandle, pfn_ret_p);
    EXPECT_EQ(D3DKMTGetSharedPrimaryHandle_fn(NULL), STATUS_INVALID_PARAMETER_);
}
TEST(exports_modules_d3d10level9_shared_priority_thunks) {
    LOADED("d3d10level9.dll");
    CHECK(GetProcAddress(hm_, "D3DKMTGetContextSchedulingPriority") == GetProcAddress(hm_, "D3DKMTGetDeviceSchedulingPriority"));
    CHECK(GetProcAddress(hm_, "D3DKMTSetContextSchedulingPriority") == GetProcAddress(hm_, "D3DKMTSetDeviceSchedulingPriority"));
    CHECK(GetProcAddress(hm_, "D3DKMTGetContextSchedulingPriority") != GetProcAddress(hm_, "D3DKMTSetContextSchedulingPriority"));
    /* every other D3DKMT thunk has its own address */
    const char *a[] = { "D3DKMTCreateDevice", "D3DKMTDestroyDevice", "D3DKMTLock", "D3DKMTUnlock", "D3DKMTPresent" };
    for (int i = 0; i < 5; i++) for (int j = i + 1; j < 5; j++) CHECK(GetProcAddress(hm_, a[i]) != GetProcAddress(hm_, a[j]));
}

/* --- EnableFeatureLevelUpgrade runs in a child to keep the process pristine --------------------- */
CHILD(xm_enable_fl_upgrade) {
    HMODULE m = LoadLibraryA("d3d11.dll"); if (!m) return 77;
    typedef BOOL (WINAPI *pf)(void);
    pf f = (pf)GetProcAddress(m, "EnableFeatureLevelUpgrade"); if (!f) return 78;
    BOOL a = f(), b = f();
    if (a != 1) return 1;
    if (b != 1) return 2;
    return 0;
}
TEST(exports_modules_d3d11_enable_feature_level_upgrade) {
    DWORD rc = t_run_child("xm_enable_fl_upgrade", "", 20000);
    if (rc == 77 || rc == 78) { t_skip("export missing"); return; }
    if (t_is_wine() && rc) { t_note("emulator: rc %lu", rc); return; }
    EXPECT_EQ(rc, 0);
}

TEST(exports_modules_d3d12core_sdk_version_is_data) {
    LOADED("d3d12core.dll");
    const DWORD *p = (const DWORD *)GetProcAddress(hm_, "D3D12SDKVersion");
    if (!p) { t_skip("D3D12SDKVersion not exported"); return; }
    CHECK(xm_in_image(hm_, p));
    if (t_is_wine()) t_note("emulator: D3D12SDKVersion is %s", xm_is_exec(hm_, p) ? "code" : "data"); else CHECK(!xm_is_exec(hm_, p));
    if (!t_is_wine() || !xm_is_exec(hm_, p)) t_note("d3d12core!D3D12SDKVersion = %lu", *p);
}
