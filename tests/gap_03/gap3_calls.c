/* Behaviour of internal exports whose body is a verified constant, pure argument validation, or a pure lookup.
 * Anything with global side effects is in tests/dx_gap_03. */
#include "t.h"
#include <stdint.h>

#define E_INVALIDARG_ ((unsigned)0x80070057)
#define E_NOTIMPL_    ((unsigned)0x80004001)
#define E_NOINTERFACE_ ((unsigned)0x80004002)
#define E_FAIL_UNSPEC ((unsigned)0x8000FFFF)

/* ---- d3dscache: argument validation is the first thing every entry does ---- */
typedef unsigned (WINAPI *sc_create_t)(void *desc, void **out, ULONGLONG x);
typedef unsigned (WINAPI *sc_destroy_t)(void *cache, unsigned a, ULONGLONG b);
typedef unsigned (WINAPI *sc_flush_t)(void *cache, ULONGLONG a, ULONGLONG b);
typedef unsigned (WINAPI *sc_free_t)(void *val, ULONGLONG a, ULONGLONG b);
typedef unsigned (WINAPI *sc_add_t)(void *cache, void *keys, ULONGLONG nkeys, void *extra, ULONGLONG data, unsigned flags);
typedef unsigned (WINAPI *sc_find_t)(ULONGLONG cache, void *keys, ULONGLONG nkeys, void *extra, void **out);
static FARPROC sc(const char *n) { return t_proc_in("d3dscache.dll", n); }

TEST(gap3_shadercache_create_null_desc_and_out) {
    sc_create_t f = (sc_create_t)sc("ShaderCache_Create"); REQUIRE(f);
    EXPECT_EQ(f(NULL, NULL, 0), E_INVALIDARG_);
}
TEST(gap3_shadercache_create_null_desc) {
    sc_create_t f = (sc_create_t)sc("ShaderCache_Create"); REQUIRE(f);
    void *out = (void *)(ULONG_PTR)0x5a5a;
    EXPECT_EQ(f(NULL, &out, 0), E_INVALIDARG_);
    EXPECT_EQ(out, 0x5a5a); /* output untouched on argument failure */
}
TEST(gap3_shadercache_create_null_out) {
    sc_create_t f = (sc_create_t)sc("ShaderCache_Create"); REQUIRE(f);
    unsigned desc[32] = { sizeof desc };
    EXPECT_EQ(f(desc, NULL, 0), E_INVALIDARG_);
}
TEST(gap3_shadercache_destroy_null) {
    sc_destroy_t f = (sc_destroy_t)sc("ShaderCache_Destroy"); REQUIRE(f);
    EXPECT_EQ(f(NULL, 0, 0), E_INVALIDARG_);
    EXPECT_EQ(f(NULL, 1, 0), E_INVALIDARG_);
}
TEST(gap3_shadercache_flush_null) {
    sc_flush_t f = (sc_flush_t)sc("ShaderCache_Flush"); REQUIRE(f);
    EXPECT_EQ(f(NULL, 0, 0), E_INVALIDARG_);
}
TEST(gap3_shadercache_freevalue_null) {
    sc_free_t f = (sc_free_t)sc("ShaderCache_FreeValue"); REQUIRE(f);
    EXPECT_EQ(f(NULL, 0, 0), E_INVALIDARG_);
}
TEST(gap3_shadercache_addvalue_null_cache) {
    sc_add_t f = (sc_add_t)sc("ShaderCache_AddValue"); REQUIRE(f);
    EXPECT_EQ(f(NULL, NULL, 0, NULL, 0, 0), E_INVALIDARG_);
}
TEST(gap3_shadercache_addvalue_null_cache_other_args_valid_looking) {
    sc_add_t f = (sc_add_t)sc("ShaderCache_AddValue"); REQUIRE(f);
    unsigned char key[64] = { 0 };
    EXPECT_EQ(f(NULL, key, 1, NULL, 16, 0), E_INVALIDARG_);
    EXPECT_EQ(f(NULL, key, 1, NULL, 16, 1), E_INVALIDARG_);
}
TEST(gap3_shadercache_findvalue_zero_handle) {
    sc_find_t f = (sc_find_t)sc("ShaderCache_FindValue"); REQUIRE(f);
    void *out = (void *)(ULONG_PTR)0x77;
    unsigned char key[64] = { 0 };
    EXPECT_EQ(f(0, NULL, 0, NULL, NULL), E_INVALIDARG_);
    EXPECT_EQ(f(0, key, 1, NULL, &out), E_INVALIDARG_);
}
TEST(gap3_shadercache_all_six_exports_resolve_distinct) {
    const char *n[] = { "ShaderCache_Create", "ShaderCache_Destroy", "ShaderCache_AddValue", "ShaderCache_FindValue", "ShaderCache_FreeValue", "ShaderCache_Flush" };
    FARPROC p[6];
    for (int i = 0; i < 6; i++) { p[i] = sc(n[i]); REQUIRE(p[i]); }
    for (int i = 0; i < 6; i++) for (int j = i + 1; j < 6; j++) CHECKF(p[i] != p[j], "%s vs %s", n[i], n[j]);
}
TEST(gap3_shadercache_ordinals_match_reference) {
    HMODULE m = t_module("d3dscache.dll"); REQUIRE(m);
    const struct { const char *n; unsigned o; } e[] = { {"ShaderCache_Create", 4}, {"ShaderCache_AddValue", 3}, {"ShaderCache_Destroy", 5}, {"ShaderCache_FindValue", 6}, {"ShaderCache_Flush", 7}, {"ShaderCache_FreeValue", 8} };
    for (int i = 0; i < 6; i++) {
        FARPROC byname = GetProcAddress(m, e[i].n);
        if (!byname) { CHECKF(t_is_wine(), "%s missing", e[i].n); continue; }
        CHECKF(GetProcAddress(m, (LPCSTR)(ULONG_PTR)e[i].o) == byname, "%s is ordinal %u", e[i].n, e[i].o);
    }
}

/* ---- dxgi / d3d10_1core constant-return stubs ---------------------------- */
typedef unsigned (WINAPI *stub4_t)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
static void stub_ret(const char *dll, const char *name, unsigned want) {
    stub4_t f = (stub4_t)t_proc_in(dll, name);
    if (!f) { t_skip("missing export: %s!%s", dll, name); return; }
    if (t_is_wine()) { t_skip("emulator body differs"); return; }
    EXPECT_EQ(f(0, 0, 0, 0), want);
    EXPECT_EQ(f(1, 2, 3, 4), want);   /* arguments are never dereferenced */
    EXPECT_EQ(f(0, 0, 0, 0), want);   /* stateless */
}
TEST(gap3_dxgid3d10_createdevice_e_notimpl) { stub_ret("dxgi.dll", "DXGID3D10CreateDevice", E_NOTIMPL_); }
TEST(gap3_dxgid3d10_createlayereddevice_e_notimpl) { stub_ret("dxgi.dll", "DXGID3D10CreateLayeredDevice", E_NOTIMPL_); }
TEST(gap3_dxgid3d10_registerlayers_e_notimpl) { stub_ret("dxgi.dll", "DXGID3D10RegisterLayers", E_NOTIMPL_); }
TEST(gap3_pixbegincapture_returns_zero) { stub_ret("dxgi.dll", "PIXBeginCapture", 0); }
TEST(gap3_pixendcapture_returns_zero) { stub_ret("dxgi.dll", "PIXEndCapture", 0); }
TEST(gap3_pixgetcapturestate_returns_zero) { stub_ret("dxgi.dll", "PIXGetCaptureState", 0); }
TEST(gap3_d3d10corecreatedevice1_e_notimpl) { stub_ret("d3d10_1core.dll", "D3D10CoreCreateDevice1", E_NOTIMPL_); }
TEST(gap3_d3d10coregetsupportedversions_e_notimpl) { stub_ret("d3d10_1core.dll", "D3D10CoreGetSupportedVersions", E_NOTIMPL_); }
TEST(gap3_d3d10coreregisterlayers_e_notimpl) { stub_ret("d3d10_1core.dll", "D3D10CoreRegisterLayers", E_NOTIMPL_); }
TEST(gap3_pix_capture_calls_do_not_change_last_error) {
    stub4_t b = (stub4_t)t_proc_in("dxgi.dll", "PIXBeginCapture"); REQUIRE(b);
    SetLastError(0x1234); b(0, 0, 0, 0);
    EXPECT_EQ(GetLastError(), 0x1234);
}

/* ---- d3d11: empty layer registration ------------------------------------ */
typedef int (WINAPI *reg_layers_t)(const void *layers, ULONG_PTR count, BOOL lock);
TEST(gap3_d3d11coreregisterlayers_zero_count) {
    reg_layers_t f = (reg_layers_t)t_proc_in("d3d11.dll", "D3D11CoreRegisterLayers"); REQUIRE(f);
    EXPECT_EQ(f(NULL, 0, FALSE), 0);
    EXPECT_EQ(f(NULL, 0, TRUE), 0);
}

/* ---- d3d10warp -------------------------------------------------------------- */
typedef unsigned (WINAPI *lastcreate_t)(void **out);
typedef unsigned (WINAPI *layeriface_t)(void *dev, const GUID *iid, void **out);
static const GUID G_ID  = {0x8458d731,0x21e7,0x438d,{0x8e,0xda,0xd0,0x4e,0xfc,0x25,0xcf,0xcd}};
static const GUID G_A   = {0x1983311b,0xb10a,0x4b7d,{0x8a,0x07,0x62,0xdd,0xd0,0xc7,0xcd,0xb6}};
static const GUID G_B   = {0x470f15e4,0x7c96,0x4d93,{0xb3,0xa8,0x66,0x17,0x0f,0x97,0xca,0x5c}};
static const GUID G_C   = {0x0e9952c3,0xd228,0x4c1a,{0x81,0xfc,0x36,0x21,0xe7,0x17,0xc4,0xe2}};
static const GUID G_BAD = {0x67676767,0x1234,0x4321,{0x80,0x80,0x01,0x02,0x03,0x04,0x05,0x06}};

/* D3D11RefGetLastCreation runs the module's lazy initialisation; do it before the layer-interface helper */
static int warp_init(void) {
    lastcreate_t f = (lastcreate_t)t_proc_in("d3d10warp.dll", "D3D11RefGetLastCreation");
    if (!f) return 0;
    void *o = NULL; unsigned hr = f(&o);
    return hr == 0;
}
TEST(gap3_d3d11refgetlastcreation_null_out) {
    lastcreate_t f = (lastcreate_t)t_proc_in("d3d10warp.dll", "D3D11RefGetLastCreation"); REQUIRE(f);
    unsigned hr = f(NULL);
    if (hr == E_FAIL_UNSPEC && t_is_wine()) { t_note("module initialisation failed in the emulator (0x8000FFFF)"); return; }
    EXPECT_EQ(hr, E_INVALIDARG_);
}
TEST(gap3_d3d11refgetlastcreation_out_is_written) {
    lastcreate_t f = (lastcreate_t)t_proc_in("d3d10warp.dll", "D3D11RefGetLastCreation"); REQUIRE(f);
    void *o = (void *)(ULONG_PTR)0x1; unsigned hr = f(&o);
    if (hr == E_FAIL_UNSPEC && t_is_wine()) { t_note("module initialisation failed in the emulator"); return; }
    EXPECT_EQ(hr, 0);
    EXPECT_EQ(o, 0);    /* Windows 10: no WARP device has been created yet in this process */
    void *o2 = (void *)(ULONG_PTR)0x2; EXPECT_EQ(f(&o2), 0);
    EXPECT_EQ(o2, o);   /* no device is created between the two calls */
}
TEST(gap3_d3dlayergetinterface_unknown_iid) {
    layeriface_t f = (layeriface_t)t_proc_in("d3d10warp.dll", "D3DLayerGetInterface"); REQUIRE(f);
    if (!warp_init()) { t_skip("warp module did not initialise"); return; }
    void *out = (void *)(ULONG_PTR)0x99; int dummy;
    EXPECT_EQ(f(&dummy, &G_BAD, &out), E_NOINTERFACE_);
    EXPECT_EQ(out, 0);   /* cleared first */
}
TEST(gap3_d3dlayergetinterface_identity_iid) {
    layeriface_t f = (layeriface_t)t_proc_in("d3d10warp.dll", "D3DLayerGetInterface"); REQUIRE(f);
    if (!warp_init()) { t_skip("warp module did not initialise"); return; }
    void *out = NULL; int dummy;
    EXPECT_EQ(f(&dummy, &G_ID, &out), 0);
    EXPECT_EQ(out, &dummy);
    out = NULL;
    EXPECT_EQ(f(NULL, &G_ID, &out), 0);
    EXPECT_EQ(out, 0);
}
TEST(gap3_d3dlayergetinterface_device_relative_iids_null_device) {
    layeriface_t f = (layeriface_t)t_proc_in("d3d10warp.dll", "D3DLayerGetInterface"); REQUIRE(f);
    if (!warp_init()) { t_skip("warp module did not initialise"); return; }
    const GUID *g[] = { &G_A, &G_B, &G_C };
    for (int i = 0; i < 3; i++) {
        void *out = (void *)(ULONG_PTR)0x99;
        EXPECT_EQ(f(NULL, g[i], &out), 0);   /* success, interface pointer is NULL without a device */
        EXPECT_EQ(out, 0);
    }
}
TEST(gap3_d3dlayergetinterface_device_slot_null_pointer) {
    layeriface_t f = (layeriface_t)t_proc_in("d3d10warp.dll", "D3DLayerGetInterface"); REQUIRE(f);
    if (!warp_init()) { t_skip("warp module did not initialise"); return; }
    void *devslot = NULL;   /* the first pointer-sized member of the handle is the device */
    const GUID *g[] = { &G_A, &G_B, &G_C };
    for (int i = 0; i < 3; i++) {
        void *out = (void *)(ULONG_PTR)0x99;
        EXPECT_EQ(f(&devslot, g[i], &out), 0);
        EXPECT_EQ(out, 0);
    }
}
TEST(gap3_d3dlayergetinterface_repeatable) {
    layeriface_t f = (layeriface_t)t_proc_in("d3d10warp.dll", "D3DLayerGetInterface"); REQUIRE(f);
    if (!warp_init()) { t_skip("warp module did not initialise"); return; }
    int dummy;
    for (int i = 0; i < 100; i++) { void *o = NULL; EXPECT_EQ(f(&dummy, &G_ID, &o), 0); EXPECT_EQ(o, &dummy); }
}
