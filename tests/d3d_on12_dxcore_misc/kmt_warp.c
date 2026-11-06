/* User-mode D3DKMT shims in d3d11/d3d10core/d3d10_1core, d3d10warp exports, OpenAdapter10, gdi32 D3DKMT read-only queries. */
#include "odm.h"

typedef LONG (WINAPI *kmt_fn)(void *);
typedef struct { UINT hAdapter; UINT Type; void *pData; UINT Size; } odm_qai;
typedef struct { UINT Version; WCHAR Name[MAX_PATH]; } odm_umdname;
typedef struct { UINT hDevice; UINT StateType; UINT Data[10]; } odm_devstate;
typedef struct { HDC hDc; UINT hAdapter; LUID Luid; UINT VidPnSourceId; } odm_openhdc;
typedef struct { UINT hAdapter; UINT hDevice; UINT Type; UINT Flags; void *pData; UINT Size; } odm_escape;

static const char *k_stub_names[] = {
    "D3DKMTCloseAdapter", "D3DKMTDestroyAllocation", "D3DKMTDestroyContext", "D3DKMTDestroyDevice", "D3DKMTDestroySynchronizationObject",
    "D3DKMTPresent", "D3DKMTSetDisplayPrivateDriverFormat", "D3DKMTSignalSynchronizationObject", "D3DKMTUnlock", "D3DKMTWaitForSynchronizationObject",
    "D3DKMTCreateAllocation", "D3DKMTCreateContext", "D3DKMTCreateDevice", "D3DKMTCreateSynchronizationObject", "D3DKMTGetContextSchedulingPriority",
    "D3DKMTGetDisplayModeList", "D3DKMTGetMultisampleMethodList", "D3DKMTGetRuntimeData", "D3DKMTGetSharedPrimaryHandle", "D3DKMTLock",
    "D3DKMTQueryAllocationResidency", "D3DKMTRender", "D3DKMTSetAllocationPriority", "D3DKMTSetContextSchedulingPriority", "D3DKMTSetDisplayMode",
    "D3DKMTSetGammaRamp", "D3DKMTSetVidPnSourceOwner", "D3DKMTWaitForVerticalBlankEvent",
};
#define NSTUB ((int)(sizeof k_stub_names / sizeof k_stub_names[0]))
static const char *k_mods[] = {"d3d11.dll", "d3d10core.dll", "d3d10_1core.dll"};

static HMODULE kmod(const char *n) { HMODULE m = t_is_wine() ? NULL : LoadLibraryA(n); return m; }

/* ---- the do-nothing thunks ---- */
TEST(d3d_on12_dxcore_misc_kmt_stubs_return_success_for_null) {
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        for (int i = 0; i < NSTUB; i++) {
            kmt_fn f = (kmt_fn)GetProcAddress(m, k_stub_names[i]);
            if (!f) { CHECKF(0, "%s!%s missing", k_mods[mi], k_stub_names[i]); continue; }
            if (!strcmp(k_mods[mi], "d3d11.dll") && !strcmp(k_stub_names[i], "D3DKMTEscape")) continue;
            CHECKF(f(NULL) == 0, "%s!%s(NULL)", k_mods[mi], k_stub_names[i]);
        }
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_stubs_share_one_body) {
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        FARPROC ref = GetProcAddress(m, "D3DKMTCloseAdapter");
        for (int i = 1; i < NSTUB; i++) {
            if (!strcmp(k_mods[mi], "d3d11.dll") && !strcmp(k_stub_names[i], "D3DKMTEscape")) continue;
            if (GetProcAddress(m, k_stub_names[i]) != ref) t_note("%s!%s not folded with CloseAdapter (linker dependent)", k_mods[mi], k_stub_names[i]);
        }
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_stubs_ignore_garbage_struct) {
    unsigned char buf[256]; memset(buf, 0xFF, sizeof buf);
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        for (int i = 0; i < NSTUB; i++) {
            kmt_fn f = (kmt_fn)GetProcAddress(m, k_stub_names[i]);
            if (!f || (!strcmp(k_mods[mi], "d3d11.dll") && !strcmp(k_stub_names[i], "D3DKMTEscape"))) continue;
            unsigned char before[16]; memcpy(before, buf, 16);
            CHECKF(f(buf) == 0, "%s!%s", k_mods[mi], k_stub_names[i]);
            CHECK(!memcmp(before, buf, 16));
        }
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_d3d11_ordinals_match_names) {
    HMODULE m = kmod("d3d11.dll"); if (!m) { t_skip("d3d11 unavailable"); return; }
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(2)) == GetProcAddress(m, "D3DKMTCloseAdapter"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(8)) == GetProcAddress(m, "D3DKMTQueryAdapterInfo"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(14)) == GetProcAddress(m, "OpenAdapter10"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(15)) == GetProcAddress(m, "OpenAdapter10_2"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(37)) == GetProcAddress(m, "D3DKMTOpenAdapterFromHdc"));
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_kmt_open_resource_and_query_resource_info_fail) {
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        kmt_fn o = (kmt_fn)GetProcAddress(m, "D3DKMTOpenResource"), q = (kmt_fn)GetProcAddress(m, "D3DKMTQueryResourceInfo");
        REQUIRE(o); REQUIRE(q);
        CHECKF((ULONG)o(NULL) == 0xC000000Du, "%s OpenResource", k_mods[mi]);
        CHECKF((ULONG)q(NULL) == 0xC000000Du, "%s QueryResourceInfo", k_mods[mi]);
        if (o != q) t_note("OpenResource/QueryResourceInfo not folded");
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_open_adapter_from_hdc) {
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTOpenAdapterFromHdc"); REQUIRE(f);
        odm_openhdc a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
        EXPECT_EQ(f(&a), 0);
        EXPECT_EQ(f(&b), 0);
        CHECKF(a.hAdapter == 0xf0f && b.hAdapter == 0xf0f, "%s handle 0x%x", k_mods[mi], a.hAdapter);
        CHECK(a.Luid.LowPart != 0 || a.Luid.HighPart != 0);
        CHECK(a.Luid.LowPart != b.Luid.LowPart || a.Luid.HighPart != b.Luid.HighPart);
        CHECK(a.hDc == NULL && a.VidPnSourceId == 0);
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_open_adapter_from_hdc_ignores_hdc) {
    HMODULE m = kmod("d3d10core.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTOpenAdapterFromHdc"); REQUIRE(f);
    odm_openhdc a; memset(&a, 0, sizeof a);
    a.hDc = (HDC)(ULONG_PTR)0xDEADBEEF;
    EXPECT_EQ(f(&a), 0);
    CHECK(a.hAdapter == 0xf0f);
    CHECK(a.hDc == (HDC)(ULONG_PTR)0xDEADBEEF);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_kmt_luids_increase) {
    HMODULE m = kmod("d3d11.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTOpenAdapterFromHdc"); REQUIRE(f);
    unsigned long long prev = 0;
    for (int i = 0; i < 8; i++) {
        odm_openhdc a; memset(&a, 0, sizeof a);
        f(&a);
        unsigned long long v = ((unsigned long long)a.Luid.HighPart << 32) | a.Luid.LowPart;
        if (i) CHECKF(v > prev, "luid %llx !> %llx", v, prev);
        prev = v;
    }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_kmt_get_device_state) {
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTGetDeviceState"); REQUIRE(f);
        odm_devstate s; memset(&s, 0x5A, sizeof s);
        s.hDevice = 0; s.StateType = 1;
        EXPECT_EQ(f(&s), 0);
        CHECKF(s.Data[0] == 1, "%s execution state %u", k_mods[mi], s.Data[0]);
        memset(&s, 0x5A, sizeof s); s.StateType = 2;
        EXPECT_EQ(f(&s), 0);
        int nz = 0; for (int i = 0; i < 10; i++) if (s.Data[i]) nz++;
        CHECKF(nz == 0, "%s present state not zeroed", k_mods[mi]);
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_get_device_state_other_types) {
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTGetDeviceState"); REQUIRE(f);
        static const UINT types[] = {0, 3, 4, 5, 100, 0xFFFFFFFF};
        for (unsigned i = 0; i < sizeof types / sizeof types[0]; i++) {
            odm_devstate s; memset(&s, 0x5A, sizeof s); s.StateType = types[i];
            CHECKF((ULONG)f(&s) == 0xC0000001u, "%s type %u", k_mods[mi], types[i]);
            CHECK(s.Data[0] == 0x5A5A5A5A);
        }
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_d3d11_escape) {
    HMODULE m = kmod("d3d11.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTEscape"); REQUIRE(f);
    unsigned char flag = 0;
    odm_escape e; memset(&e, 0, sizeof e);
    e.Type = 0; e.pData = &flag;
    EXPECT_EQ(f(&e), 0);
    CHECK(flag == 0);
    e.Type = 0x15;
    EXPECT_EQ(f(&e), 0);
    CHECK(flag == 1);
    flag = 0; e.Type = 0x14;
    EXPECT_EQ(f(&e), 0);
    CHECK(flag == 0);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_kmt_d3d10core_escape_is_noop) {
    HMODULE m = kmod("d3d10core.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTEscape"); REQUIRE(f);
    unsigned char flag = 0; odm_escape e; memset(&e, 0, sizeof e); e.Type = 0x15; e.pData = &flag;
    EXPECT_EQ(f(&e), 0);
    CHECK(flag == 0);
    EXPECT_EQ(f(NULL), 0);
    FreeLibrary(m);
}

/* ---- D3DKMTQueryAdapterInfo shim ---- */
TEST(d3d_on12_dxcore_misc_kmt_qai_umdrivername_d3d11) {
    HMODULE m = kmod("d3d11.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTQueryAdapterInfo"); REQUIRE(f);
    odm_umdname n; memset(&n, 0, sizeof n); n.Version = 1;
    odm_qai q = {0xf0f, 1, &n, sizeof n};
    EXPECT_EQ(f(&q), 0);
    WCHAR self[MAX_PATH]; GetModuleFileNameW(m, self, MAX_PATH);
    CHECK(!lstrcmpiW(n.Name, self));
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_kmt_qai_umdrivername_any_nonzero_version_d3d11) {
    HMODULE m = kmod("d3d11.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTQueryAdapterInfo"); REQUIRE(f);
    static const UINT vers[] = {1, 2, 3, 0xFFFFFFFF};
    for (unsigned i = 0; i < 4; i++) {
        odm_umdname n; memset(&n, 0, sizeof n); n.Version = vers[i];
        odm_qai q = {0, 1, &n, sizeof n};
        CHECKF(f(&q) == 0 && n.Name[0], "version %u", vers[i]);
    }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_kmt_qai_umdrivername_version0_d3d11) {
    HMODULE m = kmod("d3d11.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTQueryAdapterInfo"); REQUIRE(f);
    odm_umdname n; memset(&n, 0, sizeof n); n.Version = 0;
    odm_qai q = {0, 1, &n, sizeof n};
    EXPECT_EQ((ULONG)f(&q), 0xC0000001u);
    CHECK(n.Name[0] == 0);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_kmt_qai_umdrivername_d3d10core_version_check) {
    for (int mi = 1; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTQueryAdapterInfo"); REQUIRE(f);
        odm_umdname n; memset(&n, 0, sizeof n); n.Version = 1;
        odm_qai q = {0, 1, &n, sizeof n};
        EXPECT_EQ(f(&q), 0);
        WCHAR self[MAX_PATH]; GetModuleFileNameW(m, self, MAX_PATH);
        CHECKF(!lstrcmpiW(n.Name, self), "%s", k_mods[mi]);
        static const UINT bad[] = {0, 2, 3, 0xFFFFFFFF};
        for (unsigned i = 0; i < 4; i++) {
            memset(&n, 0, sizeof n); n.Version = bad[i];
            CHECKF((ULONG)f(&q) == 0xC0000002u, "%s version %u", k_mods[mi], bad[i]);
        }
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_qai_type11_zeroes_output) {
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTQueryAdapterInfo"); REQUIRE(f);
        UINT v[2] = {0xAAAAAAAA, 0xBBBBBBBB};
        odm_qai q = {0, 11, v, 4};
        EXPECT_EQ(f(&q), 0);
        CHECKF(v[0] == 0 && v[1] == 0xBBBBBBBB, "%s", k_mods[mi]);
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_qai_unknown_types_succeed_untouched) {
    for (int mi = 0; mi < 3; mi++) {
        HMODULE m = kmod(k_mods[mi]); if (!m) continue;
        kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTQueryAdapterInfo"); REQUIRE(f);
        static const UINT types[] = {0, 2, 3, 6, 9, 15, 20, 100, 0xFFFFFFFF};
        for (unsigned i = 0; i < sizeof types / sizeof types[0]; i++) {
            if (types[i] == 24) continue;
            UINT v[4]; memset(v, 0xAB, sizeof v);
            odm_qai q = {0, types[i], v, sizeof v};
            CHECKF(f(&q) == 0, "%s type %u", k_mods[mi], types[i]);
            CHECK(v[0] == 0xABABABAB);
        }
        FreeLibrary(m);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_qai_type24_d3d11) {
    HMODULE m = kmod("d3d11.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "D3DKMTQueryAdapterInfo"); REQUIRE(f);
    UINT v = 0xFFFFFFFF;
    odm_qai q = {0, 24, &v, 4};
    EXPECT_EQ(f(&q), 0);
    CHECK((v & 0x5) == 0x5);
    CHECK((v & 0x2) == 0);
    CHECK(v == 0xFFFFFFFD || v == (0xFFFFFFFFu & ~2u));
    q.Size = 8; v = 0x12345678;
    EXPECT_EQ((ULONG)f(&q), 0xC000000Du);
    CHECK(v == 0x12345678);
    q.Size = 0;
    EXPECT_EQ((ULONG)f(&q), 0xC000000Du);
    FreeLibrary(m);
}

/* ---- OpenAdapter10 ---- */
typedef struct { void *hRTAdapter; void *hAdapter; UINT Interface; UINT Version; const void *pAdapterCallbacks; void **pAdapterFuncs; } odm_openadapter10;
TEST(d3d_on12_dxcore_misc_openadapter10_d3d11_fills_table) {
    HMODULE m = kmod("d3d11.dll"); if (!m) { t_skip("unavailable"); return; }
    kmt_fn f = (kmt_fn)GetProcAddress(m, "OpenAdapter10"); REQUIRE(f);
    void *funcs[8]; memset(funcs, 0, sizeof funcs);
    odm_openadapter10 a; memset(&a, 0, sizeof a); a.pAdapterFuncs = funcs;
    EXPECT_EQ(f(&a), 0);
    CHECK(a.hAdapter == (void *)(ULONG_PTR)0xf07);
    CHECK(funcs[0] && funcs[1] && funcs[2]);
    CHECK(funcs[3] == NULL);
    HMODULE o = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)funcs[1], &o);
    CHECK(o == m);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_openadapter10_d3d10core_defers_to_d3d11) {
    HMODULE c = kmod("d3d10core.dll"), d = kmod("d3d11.dll"); if (!c || !d) { t_skip("unavailable"); return; }
    kmt_fn fc = (kmt_fn)GetProcAddress(c, "OpenAdapter10"), fd = (kmt_fn)GetProcAddress(d, "OpenAdapter10"); REQUIRE(fc); REQUIRE(fd);
    void *t1[8], *t2[8]; memset(t1, 0, sizeof t1); memset(t2, 0, sizeof t2);
    odm_openadapter10 a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); a.pAdapterFuncs = t1; b.pAdapterFuncs = t2;
    EXPECT_EQ(fc(&a), 0);
    EXPECT_EQ(fd(&b), 0);
    CHECK(a.hAdapter == b.hAdapter);
    CHECK(!memcmp(t1, t2, sizeof t1));
    FreeLibrary(c); FreeLibrary(d);
}
TEST(d3d_on12_dxcore_misc_openadapter10_2_d3d10core_defers) {
    HMODULE c = kmod("d3d10core.dll"); if (!c) { t_skip("unavailable"); return; }
    CHECK(GetProcAddress(c, "OpenAdapter10_2") != NULL);
    CHECK(GetProcAddress(c, MAKEINTRESOURCEA(13)) == GetProcAddress(c, "OpenAdapter10_2"));
    CHECK(GetProcAddress(c, MAKEINTRESOURCEA(12)) == GetProcAddress(c, "OpenAdapter10"));
    FreeLibrary(c);
}

/* ---- d3d10core / d3d10_1core core entry points ---- */
TEST(d3d_on12_dxcore_misc_d3d10core_version_and_stubs) {
    HMODULE m = kmod("d3d10core.dll"); if (!m) { t_skip("unavailable"); return; }
    typedef unsigned long long (WINAPI *getver_t)(void);
    getver_t gv = (getver_t)GetProcAddress(m, "D3D10CoreGetVersion"); REQUIRE(gv);
    EXPECT_EQ(gv(), 0x000a000100041770ull);
    const char *stubs[] = {"D3D10CoreCreateDevice", "D3D10CoreGetSupportedVersions", "D3D10CoreRegisterLayers"};
    for (int i = 0; i < 3; i++) {
        kmt_fn f = (kmt_fn)GetProcAddress(m, stubs[i]); REQUIRE(f);
        CHECKF((ULONG)f(NULL) == 0x80004001u, "%s", stubs[i]);
    }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d10_1core_version_and_stubs) {
    HMODULE m = kmod("d3d10_1core.dll"); if (!m) { t_skip("unavailable"); return; }
    typedef unsigned long long (WINAPI *getver_t)(void);
    getver_t gv = (getver_t)GetProcAddress(m, "D3D10CoreGetVersion"); REQUIRE(gv);
    EXPECT_EQ(gv(), 0x000a000100041770ull);
    const char *stubs[] = {"D3D10CoreCreateDevice1", "D3D10CoreGetSupportedVersions", "D3D10CoreRegisterLayers"};
    for (int i = 0; i < 3; i++) {
        kmt_fn f = (kmt_fn)GetProcAddress(m, stubs[i]); REQUIRE(f);
        CHECKF((ULONG)f(NULL) == 0x80004001u, "%s", stubs[i]);
    }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d10core_ordinal_layout) {
    HMODULE m = kmod("d3d10core.dll"); if (!m) { t_skip("unavailable"); return; }
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(1)) == GetProcAddress(m, "D3DKMTCloseAdapter"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(7)) == GetProcAddress(m, "D3DKMTQueryAdapterInfo"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(14)) == GetProcAddress(m, "D3D10CoreCreateDevice"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(15)) == GetProcAddress(m, "D3D10CoreGetSupportedVersions"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(16)) == GetProcAddress(m, "D3D10CoreGetVersion"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(17)) == GetProcAddress(m, "D3D10CoreRegisterLayers"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(14)) == GetProcAddress(m, "D3D10CoreRegisterLayers"));
    FreeLibrary(m);
}

/* ---- d3d10warp.dll ---- */
static HMODULE hwarp(void) { return t_is_wine() ? NULL : LoadLibraryA("d3d10warp.dll"); }
/* D3DLayerGetInterface dereferences layer state that only exists once a WARP device has been created in the process: before that it
 * raises an access violation (probed, see the child test below). Create one device and keep it for the life of the exe. */
static HMODULE hwarp_init(void) {
    static ID3D11Device *keep; static ID3D11DeviceContext *kc;
    HMODULE m = hwarp(); if (!m) return NULL;
    if (!keep) { D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE dt; if (FAILED(d3dh_make_device11(&keep, &kc, &fl, &dt, 0))) keep = NULL; }
    return keep ? m : (FreeLibrary(m), NULL);
}
TEST(d3d_on12_dxcore_misc_warp_exports) {
    HMODULE m = hwarp(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    static const char *n[] = {"VSD3DDebugConnectionBuffer", "D3D11RefGetLastCreation", "D3DLayerGetInterface", "OpenAdapter", "OpenAdapter10_2", "OpenAdapter12", "OpenDisplayAdapter1", "QueryDListForApplication1"};
    for (unsigned i = 0; i < 8; i++) CHECKF(GetProcAddress(m, n[i]) != NULL, "%s", n[i]);
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(199)) != NULL);
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(200)) == GetProcAddress(m, "VSD3DDebugConnectionBuffer"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(203)) == GetProcAddress(m, "OpenAdapter"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(204)) == GetProcAddress(m, "OpenAdapter10_2"));
    SET_ERR(0);
    CHECK(GetProcAddress(m, "OpenAdapter10") == NULL);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_no_ordinals_below_199) {
    HMODULE m = hwarp(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    static const unsigned o[] = {1, 100, 198, 208, 300};
    for (unsigned i = 0; i < 5; i++) { SET_ERR(0); CHECKF(GetProcAddress(m, MAKEINTRESOURCEA(o[i])) == NULL, "ordinal %u", o[i]); }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_last_creation_null_out) {
    HMODULE m = hwarp(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    typedef HRESULT (WINAPI *f_t)(void **);
    f_t f = (f_t)GetProcAddress(m, "D3D11RefGetLastCreation"); REQUIRE(f);
    EXPECT_HR(f(NULL), E_INVARG);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_last_creation_writes_output) {
    HMODULE m = hwarp(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    typedef HRESULT (WINAPI *f_t)(void **);
    f_t f = (f_t)GetProcAddress(m, "D3D11RefGetLastCreation"); REQUIRE(f);
    void *p = (void *)(ULONG_PTR)0xDEADBEEF;
    EXPECT_HR(f(&p), S_OK);
    CHECK(p != (void *)(ULONG_PTR)0xDEADBEEF);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_last_creation_tracks_warp_device) {
    HMODULE m = hwarp(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    typedef HRESULT (WINAPI *f_t)(void **);
    f_t f = (f_t)GetProcAddress(m, "D3D11RefGetLastCreation"); REQUIRE(f);
    ID3D11Device *d = NULL; D3D_DRIVER_TYPE dt = 0;
    if (FAILED(d3dh_make_device11(&d, NULL, NULL, &dt, 0)) || !d) { FreeLibrary(m); t_skip("no device"); return; }
    void *p = NULL;
    EXPECT_HR(f(&p), S_OK);
    t_note("driver type %d, last creation pointer %p", dt, p);
    SAFE_RELEASE(d);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_layer_get_interface_unknown_iid) {
    HMODULE m = hwarp_init(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    typedef HRESULT (WINAPI *f_t)(void *, const GUID *, void **);
    f_t f = (f_t)GetProcAddress(m, "D3DLayerGetInterface"); REQUIRE(f);
    void *out = (void *)1;
    EXPECT_HR(f(NULL, &DXC_ATTR_BOGUS, &out), E_NOIFACE);
    CHECK(out == NULL);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_layer_get_interface_null_adapter_ok_iids) {
    HMODULE m = hwarp_init(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    typedef HRESULT (WINAPI *f_t)(void *, const GUID *, void **);
    f_t f = (f_t)GetProcAddress(m, "D3DLayerGetInterface"); REQUIRE(f);
    static const GUID iids[3] = {
        {0x1983311b, 0xb10a, 0x4b7d, {0x8a, 0x07, 0x62, 0xdd, 0xd0, 0xc7, 0xcd, 0xb6}},
        {0x470f15e4, 0x7c96, 0x4d93, {0xb3, 0xa8, 0x66, 0x17, 0x0f, 0x97, 0xca, 0x5c}},
        {0x0e9952c3, 0xd228, 0x4c1a, {0x81, 0xfc, 0x36, 0x21, 0xe7, 0x17, 0xc4, 0xe2}},
    };
    for (int i = 0; i < 3; i++) { void *out = (void *)1; EXPECT_HR(f(NULL, &iids[i], &out), S_OK); CHECK(out == NULL); }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_layer_get_interface_identity_iid) {
    HMODULE m = hwarp_init(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    typedef HRESULT (WINAPI *f_t)(void *, const GUID *, void **);
    f_t f = (f_t)GetProcAddress(m, "D3DLayerGetInterface"); REQUIRE(f);
    static const GUID iid = {0x8458d731, 0x21e7, 0x438d, {0x8e, 0xda, 0xd0, 0x4e, 0xfc, 0x25, 0xcf, 0xcd}};
    void *out = NULL, *in = (void *)(ULONG_PTR)0x1234560;
    EXPECT_HR(f(in, &iid, &out), S_OK);
    CHECK(out == in);
    out = (void *)1;
    EXPECT_HR(f(NULL, &iid, &out), S_OK);
    CHECK(out == NULL);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_layer_get_interface_with_adapter_object) {
    HMODULE m = hwarp_init(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    typedef HRESULT (WINAPI *f_t)(void *, const GUID *, void **);
    f_t f = (f_t)GetProcAddress(m, "D3DLayerGetInterface"); REQUIRE(f);
    static const GUID iid = {0x0e9952c3, 0xd228, 0x4c1a, {0x81, 0xfc, 0x36, 0x21, 0xe7, 0x17, 0xc4, 0xe2}};
    /* the first field is a pointer to an object; a NULL object pointer yields a NULL result without dereferencing */
    void *holder[4] = {0, 0, 0, 0}, *out = (void *)1;
    EXPECT_HR(f(holder, &iid, &out), S_OK);
    CHECK(out == NULL);
    /* with a non-NULL object the result is interior to it */
    unsigned char obj[0x600]; memset(obj, 0, sizeof obj);
    holder[0] = obj; out = NULL;
    EXPECT_HR(f(holder, &iid, &out), S_OK);
    CHECK(out == (void *)(obj + 0x40));
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_warp_openadapter_family_exists_and_distinct) {
    HMODULE m = hwarp(); if (!m) { t_skip("d3d10warp unavailable"); return; }
    FARPROC a = GetProcAddress(m, "OpenAdapter"), b = GetProcAddress(m, "OpenAdapter10_2"), c = GetProcAddress(m, "OpenAdapter12"), d = GetProcAddress(m, "OpenDisplayAdapter1");
    CHECK(a && b && c && d);
    CHECK(a != b && b != c && c != d);
    FreeLibrary(m);
}

/* ---- gdi32 D3DKMT (win32u-backed): read-only queries, invalid arguments ---- */
typedef LONG (WINAPI *g_enum2_t)(void *);
typedef struct { UINT hAdapter; LUID Luid; UINT NumOfSources; BOOL Precise; } odm_adapterinfo;
typedef struct { UINT NumAdapters; odm_adapterinfo *pAdapters; } odm_enum2;
typedef struct { LUID Luid; UINT hAdapter; } odm_openluid;
typedef struct { UINT hAdapter; } odm_close;
static FARPROC gdi(const char *n) { return t_is_wine() ? NULL : t_proc_in("gdi32.dll", n); }

TEST(d3d_on12_dxcore_misc_kmt_gdi32_exports) {
    if (t_is_wine()) { t_skip("emulated"); return; }
    static const char *n[] = {"D3DKMTOpenAdapterFromLuid", "D3DKMTQueryAdapterInfo", "D3DKMTCloseAdapter", "D3DKMTEnumAdapters2", "D3DKMTCheckVidPnExclusiveOwnership"};
    for (unsigned i = 0; i < 5; i++) CHECKF(gdi(n[i]) != NULL, "%s", n[i]);
}
TEST(d3d_on12_dxcore_misc_kmt_gdi32_null_arguments_fail) {
    static const char *n[] = {"D3DKMTOpenAdapterFromLuid", "D3DKMTQueryAdapterInfo", "D3DKMTCloseAdapter", "D3DKMTEnumAdapters2", "D3DKMTCheckVidPnExclusiveOwnership"};
    for (unsigned i = 0; i < 5; i++) {
        kmt_fn f = (kmt_fn)gdi(n[i]);
        if (!f) { t_skip("gdi32 D3DKMT unavailable"); return; }
        LONG st = f(NULL);
        CHECKF(st == (LONG)0xC000000D, "%s(NULL) = 0x%08lx (want STATUS_INVALID_PARAMETER)", n[i], (unsigned long)st);
    }
}
TEST(d3d_on12_dxcore_misc_kmt_gdi32_close_invalid_handle) {
    kmt_fn f = (kmt_fn)gdi("D3DKMTCloseAdapter"); REQUIRE(f);
    odm_close c = {0}; LONG st = f(&c);
    CHECKF(st == (LONG)0xC000000D, "CloseAdapter(0) = 0x%08lx", (unsigned long)st);
    c.hAdapter = 0x7FFFFFF0; st = f(&c);
    CHECKF(st == (LONG)0xC000000D, "CloseAdapter(0x7FFFFFF0) = 0x%08lx", (unsigned long)st);
}
TEST(d3d_on12_dxcore_misc_kmt_gdi32_query_info_invalid_handle) {
    kmt_fn f = (kmt_fn)gdi("D3DKMTQueryAdapterInfo"); REQUIRE(f);
    UINT v = 0; odm_qai q = {0x7FFFFFF0, 15, &v, 4};
    LONG st = f(&q);
    CHECKF(st == (LONG)0xC000000D, "QueryAdapterInfo(bad handle) = 0x%08lx", (unsigned long)st);
}
TEST(d3d_on12_dxcore_misc_kmt_gdi32_open_luid_unknown) {
    kmt_fn f = (kmt_fn)gdi("D3DKMTOpenAdapterFromLuid"); REQUIRE(f);
    odm_openluid o; memset(&o, 0, sizeof o); o.Luid.LowPart = 0x7FFFFF01; o.Luid.HighPart = 0x7FFF;
    LONG st = f(&o);
    CHECKF(st == (LONG)0xC000000D, "OpenAdapterFromLuid(unknown) = 0x%08lx", (unsigned long)st);
    CHECK(o.hAdapter == 0);
}
TEST(d3d_on12_dxcore_misc_kmt_gdi32_enum_adapters_protocol) {
    kmt_fn f = (kmt_fn)gdi("D3DKMTEnumAdapters2"); REQUIRE(f);
    odm_enum2 e; memset(&e, 0, sizeof e);
    LONG st = f(&e);
    if (st != 0) { t_note("EnumAdapters2 count query: 0x%08lx", (unsigned long)st); t_skip("not available"); return; }
    UINT n = e.NumAdapters;
    t_note("adapter count %u", n);
    if (n == 0 || n > 32) return;
    odm_adapterinfo infos[32]; memset(infos, 0, sizeof infos);
    e.pAdapters = infos; e.NumAdapters = n;
    st = f(&e);
    EXPECT_EQ(st, 0);
    CHECK(e.NumAdapters <= n);
    kmt_fn closef = (kmt_fn)gdi("D3DKMTCloseAdapter");
    for (UINT i = 0; i < e.NumAdapters && i < n; i++) {
        CHECK(infos[i].hAdapter != 0);
        CHECK(infos[i].Luid.LowPart != 0 || infos[i].Luid.HighPart != 0);
        if (closef) { odm_close c = {infos[i].hAdapter}; EXPECT_EQ(closef(&c), 0); }
    }
}
TEST(d3d_on12_dxcore_misc_kmt_gdi32_enum_adapters_too_small_buffer) {
    kmt_fn f = (kmt_fn)gdi("D3DKMTEnumAdapters2"); REQUIRE(f);
    odm_enum2 e; memset(&e, 0, sizeof e);
    if (f(&e) != 0 || e.NumAdapters < 2) { t_skip("need >= 2 adapters"); return; }
    odm_adapterinfo one; memset(&one, 0, sizeof one);
    UINT total = e.NumAdapters;
    e.pAdapters = &one; e.NumAdapters = 1;
    LONG st = f(&e);
    CHECKF(st == (LONG)0xC0000023, "EnumAdapters2 with a 1-entry buffer for %u adapters: 0x%08lx", total, (unsigned long)st);   /* STATUS_BUFFER_TOO_SMALL (probed) */
    kmt_fn closef = (kmt_fn)gdi("D3DKMTCloseAdapter");
    if (closef && one.hAdapter) { odm_close c = {one.hAdapter}; closef(&c); }
}
TEST(d3d_on12_dxcore_misc_kmt_gdi32_open_luid_roundtrip_query_close) {
    kmt_fn en = (kmt_fn)gdi("D3DKMTEnumAdapters2"), op = (kmt_fn)gdi("D3DKMTOpenAdapterFromLuid"), qf = (kmt_fn)gdi("D3DKMTQueryAdapterInfo"), cl = (kmt_fn)gdi("D3DKMTCloseAdapter");
    REQUIRE(en); REQUIRE(op); REQUIRE(qf); REQUIRE(cl);
    odm_enum2 e; memset(&e, 0, sizeof e);
    odm_adapterinfo infos[32]; memset(infos, 0, sizeof infos);
    if (en(&e) != 0 || e.NumAdapters == 0 || e.NumAdapters > 32) { t_skip("no kernel adapters"); return; }
    e.pAdapters = infos;
    if (en(&e) != 0) { t_skip("enum failed"); return; }
    for (UINT i = 0; i < e.NumAdapters; i++) { odm_close c = {infos[i].hAdapter}; cl(&c); }
    odm_openluid o; memset(&o, 0, sizeof o); o.Luid = infos[0].Luid;
    LONG st = op(&o);
    EXPECT_EQ(st, 0);
    if (st == 0) {
        CHECK(o.hAdapter != 0);
        UINT type = 0xFFFFFFFF; odm_qai q = {o.hAdapter, 15 /* KMTQAITYPE_ADAPTERTYPE */, &type, 4};
        st = qf(&q);
        CHECKF(st == 0, "ADAPTERTYPE query: 0x%08lx value 0x%x", (unsigned long)st, type);
        CHECK(type != 0xFFFFFFFF);
        odm_close c = {o.hAdapter};
        EXPECT_EQ(cl(&c), 0);
        st = cl(&c);
        CHECKF(st == (LONG)0xC000000D, "double close 0x%08lx", (unsigned long)st);
        st = qf(&q);
        CHECKF(st == (LONG)0xC000000D, "query after close 0x%08lx", (unsigned long)st);
    }
}

/* in a fresh process, before any WARP device exists, D3DLayerGetInterface raises an access violation for every argument shape (probed) */
static void layer_call_null(void *c) { typedef HRESULT (WINAPI *f_t)(void *, const GUID *, void **); void *out = NULL; ((f_t)c)(NULL, &DXC_ATTR_BOGUS, &out); }
CHILD(warp_layer_uninit) {
    HMODULE m = LoadLibraryA("d3d10warp.dll"); if (!m) return 77;
    void *f = (void *)GetProcAddress(m, "D3DLayerGetInterface"); if (!f) return 78;
    return t_try(layer_call_null, f) == 0xc0000005 ? 0 : 1;
}
TEST(d3d_on12_dxcore_misc_warp_layer_get_interface_before_any_device_faults) {
    if (t_is_wine()) { t_skip("emulated d3d10warp"); return; }
    DWORD rc = t_run_child("warp_layer_uninit", "", 30000);
    if (rc == 77) { t_skip("d3d10warp unavailable"); return; }
    EXPECT_EQ(rc, 0);
}
