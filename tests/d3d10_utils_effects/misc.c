/* Export sets, shader profile strings, version, level9 journal. */
#include "ue.h"
#include "exports_tbl.h"

#define NELEM(a) ((int)(sizeof(a) / sizeof(*(a))))

static void check_exports(const char *dll, const char *const *tbl, int n, int kmt_soft)
{
    HMODULE m = t_module(dll);
    if (!m) { t_skip("%s not loadable", dll); return; }
    for (int i = 0; i < n; i++) {
        FARPROC p = GetProcAddress(m, tbl[i]);
        if (kmt_soft && !strncmp(tbl[i], "D3DKMT", 6)) { if (!p) t_note("%s lacks %s", dll, tbl[i]); continue; }
        t_check(p != NULL, __FILE__, __LINE__, "GetProcAddress", "%s!%s missing", dll, tbl[i]);
    }
}
TEST(d3d10_utils_effects_exports_d3d10) { check_exports("d3d10.dll", k_d3d10, NELEM(k_d3d10), 0); }
TEST(d3d10_utils_effects_exports_d3d10_1) { check_exports("d3d10_1.dll", k_d3d10_1, NELEM(k_d3d10_1), 0); }
TEST(d3d10_utils_effects_exports_d3d10core) { check_exports("d3d10core.dll", k_core, NELEM(k_core), 1); }
TEST(d3d10_utils_effects_exports_d3d10_1core) { check_exports("d3d10_1core.dll", k_core1, NELEM(k_core1), 1); }
TEST(d3d10_utils_effects_exports_d3d10level9) { check_exports("d3d10level9.dll", k_l9, NELEM(k_l9), 1); }

static void check_ordinals(const char *dll, const char *const *tbl, int n)
{
    if (t_is_wine()) { t_skip("export ordinals of the emulator differ"); return; }
    HMODULE m = t_module(dll);
    if (!m) { t_skip("%s not loadable", dll); return; }
    /* The export directory is sorted by name and the ordinals follow that order, starting at 1. */
    for (int i = 0; i < n; i++) {
        FARPROC byname = GetProcAddress(m, tbl[i]);
        FARPROC byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)(i + 1));
        CHECK(byname != NULL);
        if (byname != byord) t_note("%s!%s is not at name-sorted ordinal %d", dll, tbl[i], i + 1);
    }
}
TEST(d3d10_utils_effects_ordinals_d3d10) { check_ordinals("d3d10.dll", k_d3d10, NELEM(k_d3d10)); }
TEST(d3d10_utils_effects_ordinals_d3d10_1) { check_ordinals("d3d10_1.dll", k_d3d10_1, NELEM(k_d3d10_1)); }

TEST(d3d10_utils_effects_exports_missing_name)
{
    HMODULE m = t_module("d3d10.dll");
    if (!m) { t_skip("no d3d10"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, "D3D10NoSuchExport") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0x7ff0) == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(d3d10_utils_effects_exports_case_sensitive)
{
    HMODULE m = t_module("d3d10.dll");
    if (!m) { t_skip("no d3d10"); return; }
    CHECK(GetProcAddress(m, "d3d10createblob") == NULL);
    CHECK(GetProcAddress(m, "D3D10CREATEBLOB") == NULL);
    CHECK(GetProcAddress(m, "D3D10CreateBlob") != NULL);
}
TEST(d3d10_utils_effects_exports_no_core_in_d3d10)
{
    /* The D3D10Core* entry points live only in the core DLLs; d3d10.dll merely imports them. */
    HMODULE m = t_module("d3d10.dll");
    if (!m) { t_skip("no d3d10"); return; }
    CHECK(GetProcAddress(m, "D3D10CoreGetVersion") == NULL);
    CHECK(GetProcAddress(m, "D3D10CoreCreateDevice") == NULL);
    CHECK(GetProcAddress(m, "OpenAdapter10") == NULL);
}
TEST(d3d10_utils_effects_exports_d3d10_1_has_1_suffix_entries)
{
    HMODULE m = t_module("d3d10_1.dll");
    if (!m) { t_skip("no d3d10_1"); return; }
    CHECK(GetProcAddress(m, "D3D10CreateDevice1") != NULL);
    CHECK(GetProcAddress(m, "D3D10CreateDeviceAndSwapChain1") != NULL);
    CHECK(GetProcAddress(m, "D3D10CreateDevice") == NULL);
    CHECK(GetProcAddress(m, "D3D10CreateDeviceAndSwapChain") == NULL);
    CHECK(GetProcAddress(m, "D3D10StateBlockMaskEnableAll") != NULL);
}
TEST(d3d10_utils_effects_exports_d3d10_has_unsuffixed_device_entries)
{
    HMODULE m = t_module("d3d10.dll");
    if (!m) { t_skip("no d3d10"); return; }
    CHECK(GetProcAddress(m, "D3D10CreateDevice") != NULL);
    CHECK(GetProcAddress(m, "D3D10CreateDeviceAndSwapChain") != NULL);
    CHECK(GetProcAddress(m, "D3D10CreateDevice1") == NULL);
    CHECK(GetProcAddress(m, "D3D10StateBlockMaskEnableAll") != NULL);
}
TEST(d3d10_utils_effects_exports_core_dlls_share_names)
{
    HMODULE a = t_module("d3d10core.dll"), b = t_module("d3d10_1core.dll");
    if (!a || !b) { t_skip("core dlls missing"); return; }
    static const char *const common[] = { "D3D10CoreGetVersion", "D3D10CoreRegisterLayers", "D3D10CoreGetSupportedVersions", "OpenAdapter10", "OpenAdapter10_2" };
    for (int i = 0; i < NELEM(common); i++) {
        CHECKF(GetProcAddress(a, common[i]) != NULL, "d3d10core %s", common[i]);
        CHECKF(GetProcAddress(b, common[i]) != NULL, "d3d10_1core %s", common[i]);
    }
    CHECK(GetProcAddress(a, "D3D10CoreCreateDevice") != NULL);
    CHECK(GetProcAddress(b, "D3D10CoreCreateDevice1") != NULL);
    CHECK(GetProcAddress(a, "D3D10CoreCreateDevice1") == NULL);
    CHECK(GetProcAddress(b, "D3D10CoreCreateDevice") == NULL);
}
TEST(d3d10_utils_effects_exports_level9_entries)
{
    HMODULE m = t_module("d3d10level9.dll");
    if (!m) { t_skip("no d3d10level9"); return; }
    CHECK(GetProcAddress(m, "D3D10CheckLevel9Hardware") != NULL);
    CHECK(GetProcAddress(m, "D3D10Level9DumpJournal") != NULL);
    CHECK(GetProcAddress(m, "OpenAdapter10") != NULL);
    CHECK(GetProcAddress(m, "OpenAdapter10_2") != NULL);
    CHECK(GetProcAddress(m, "RetrieveFilteredOpenAdapter") != NULL);
    CHECK(GetProcAddress(m, "D3D11CreateDeviceExternalImplementation") != NULL);
    CHECK(GetProcAddress(m, "LogMarkerStringTable") != NULL);
}

/* ---- profiles ---- */
typedef const char *(WINAPI *prof_fn)(ID3D10Device *);
typedef struct { const char *name; const char *want; } profrow;
static const profrow k_prof[] = {
    { "D3D10GetVertexShaderProfile", "vs_4_0" },
    { "D3D10GetPixelShaderProfile", "ps_4_0" },
    { "D3D10GetGeometryShaderProfile", "gs_4_0" },
};
static void prof_dll(const char *dll)
{
    for (int i = 0; i < NELEM(k_prof); i++) {
        prof_fn f = (prof_fn)t_proc_in(dll, k_prof[i].name);
        if (!f) { t_skip("%s!%s missing", dll, k_prof[i].name); return; }
        const char *s = f(NULL);
        t_check(s != NULL, __FILE__, __LINE__, "profile != NULL", "%s!%s", dll, k_prof[i].name);
        if (s) { EXPECT_STR(s, k_prof[i].want); }
    }
}
TEST(d3d10_utils_effects_profile_strings_d3d10) { prof_dll("d3d10.dll"); }
TEST(d3d10_utils_effects_profile_strings_d3d10_1) { prof_dll("d3d10_1.dll"); }
TEST(d3d10_utils_effects_profile_returns_static_pointer)
{
    prof_fn f = (prof_fn)t_proc_in("d3d10.dll", "D3D10GetVertexShaderProfile");
    REQUIRE(f);
    const char *a = f(NULL), *b = f(NULL);
    EXPECT_EQ(a, b);
}
TEST(d3d10_utils_effects_profile_independent_of_device_argument)
{
    prof_fn f = (prof_fn)t_proc_in("d3d10.dll", "D3D10GetPixelShaderProfile");
    REQUIRE(f);
    ID3D10Device *dev = NULL;
    D3D10_DRIVER_TYPE dt;
    if (FAILED(d3dh_make_device10(&dev, &dt, 0)) || !dev) { t_skip("no D3D10 device"); return; }
    EXPECT_STR(f(dev), "ps_4_0");
    EXPECT_STR(f(NULL), "ps_4_0");
    SAFE_RELEASE(dev);
}
TEST(d3d10_utils_effects_profile_with_10_1_device)
{
    ID3D10Device1 *dev = NULL;
    D3D10_FEATURE_LEVEL1 fl;
    D3D10_DRIVER_TYPE dt;
    if (FAILED(d3dh_make_device10_1(&dev, &fl, &dt, 0)) || !dev) { t_skip("no D3D10.1 device"); return; }
    prof_fn v = (prof_fn)t_proc_in("d3d10_1.dll", "D3D10GetVertexShaderProfile");
    prof_fn p = (prof_fn)t_proc_in("d3d10_1.dll", "D3D10GetPixelShaderProfile");
    prof_fn g = (prof_fn)t_proc_in("d3d10_1.dll", "D3D10GetGeometryShaderProfile");
    if (v && p && g) {
        /* the entry points are forwarded to d3d10.dll, which does not look at the device */
        EXPECT_STR(v((ID3D10Device *)dev), "vs_4_0");
        EXPECT_STR(p((ID3D10Device *)dev), "ps_4_0");
        EXPECT_STR(g((ID3D10Device *)dev), "gs_4_0");
    }
    t_note("10.1 device feature level 0x%x", (unsigned)fl);
    SAFE_RELEASE(dev);
}
TEST(d3d10_utils_effects_profile_usable_as_compile_target)
{
    prof_fn v = (prof_fn)t_proc_in("d3d10.dll", "D3D10GetVertexShaderProfile");
    prof_fn p = (prof_fn)t_proc_in("d3d10.dll", "D3D10GetPixelShaderProfile");
    REQUIRE(v && p && U(D3D10CompileShader));
    ID3D10Blob *b = NULL;
    HRESULT hr = ue_compile_vs(UE_VS_SRC, "main", v(NULL), &b);
    EXPECT_HR(hr, S_OK);
    UE_REL(b);
    hr = ue_compile_vs(UE_PS_SRC, "main", p(NULL), &b);
    EXPECT_HR(hr, S_OK);
    UE_REL(b);
}

/* ---- version ---- */
typedef UINT64 (WINAPI *ver_fn)(void);
TEST(d3d10_utils_effects_version_value)
{
    NOWINE("D3D10GetVersion is a stub");
    ver_fn f = (ver_fn)t_proc_in("d3d10.dll", "D3D10GetVersion");
    REQUIRE(f);
    UINT64 v = f();
    EXPECT_EQ(v >> 48, 10);
    EXPECT_EQ((v >> 32) & 0xffff, 1);
    t_note("D3D10GetVersion = 0x%016llx", (unsigned long long)v);
}
TEST(d3d10_utils_effects_version_full_value)
{
    NOWINE("D3D10GetVersion is a stub");
    ver_fn f = (ver_fn)t_proc_in("d3d10.dll", "D3D10GetVersion");
    REQUIRE(f);
    UINT64 v = f();
    if (v != 0x000a000100041770ull) t_note("D3D10GetVersion differs from the Win10 19041 value: 0x%016llx", (unsigned long long)v);
    CHECK((v >> 48) == 10);
}
TEST(d3d10_utils_effects_version_matches_core)
{
    NOWINE("D3D10GetVersion is a stub");
    ver_fn a = (ver_fn)t_proc_in("d3d10.dll", "D3D10GetVersion");
    ver_fn b = (ver_fn)t_proc_in("d3d10core.dll", "D3D10CoreGetVersion");
    ver_fn c = (ver_fn)t_proc_in("d3d10_1.dll", "D3D10GetVersion");
    REQUIRE(a && b && c);
    UINT64 va = a(), vb = b(), vc = c();
    EXPECT_EQ(va, vb);
    EXPECT_EQ(vc, vb);
}
TEST(d3d10_utils_effects_version_core1_matches)
{
    NOWINE("D3D10GetVersion is a stub");
    ver_fn b = (ver_fn)t_proc_in("d3d10core.dll", "D3D10CoreGetVersion");
    ver_fn d = (ver_fn)t_proc_in("d3d10_1core.dll", "D3D10CoreGetVersion");
    REQUIRE(b && d);
    UINT64 vb = b(), vd = d();
    if (vb != vd) t_note("core 0x%llx vs 1core 0x%llx", (unsigned long long)vb, (unsigned long long)vd);
    EXPECT_EQ(vd >> 48, 10);
}
TEST(d3d10_utils_effects_version_stable_across_calls)
{
    NOWINE("D3D10GetVersion is a stub");
    ver_fn a = (ver_fn)t_proc_in("d3d10.dll", "D3D10GetVersion");
    REQUIRE(a);
    UINT64 v0 = a();
    for (int i = 0; i < 10; i++) EXPECT_EQ(a(), v0);
}

/* ---- level9 journal ---- */
static int g_journal_calls;
static int g_journal_bad;
static void __cdecl journal_cb(const char *s)
{
    g_journal_calls++;
    static const char pre[] = "D3D10Level9 Error ";
    size_t n = s ? strlen(s) : 0;
    if (!s || strncmp(s, pre, sizeof pre - 1) || n < 2 || s[n - 2] != '\r' || s[n - 1] != '\n') g_journal_bad++;
}
TEST(d3d10_utils_effects_level9_dump_journal_format)
{
    typedef void (__cdecl *dj_fn)(void (__cdecl *)(const char *));
    dj_fn f = (dj_fn)t_proc_in("d3d10level9.dll", "D3D10Level9DumpJournal");
    REQUIRE(f);
    g_journal_calls = g_journal_bad = 0;
    f(journal_cb);
    EXPECT_EQ(g_journal_bad, 0);
    t_note("journal entries reported: %d", g_journal_calls);
    /* a second dump reports the same entries (the journal is not consumed) */
    int first = g_journal_calls;
    g_journal_calls = 0;
    f(journal_cb);
    EXPECT_EQ(g_journal_calls, first);
}
