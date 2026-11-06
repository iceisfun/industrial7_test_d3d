/* Argument validation of D3D11CreateDevice: SDK version, adapter/driver type/software module combinations,
 * feature level array validation, output pointer handling. */
#include "dc.h"

#define T(n) TEST(d3d11_device_create_##n)
#define NEED() REQUIRE(DC_FN())

/* ---- SDK version ---- */
#define SDKV(nm, v) T(sdk_version_##nm) { NEED(); \
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, v, "sdk version " #v); }
SDKV(0, 0) SDKV(1, 1) SDKV(6, 6) SDKV(8, 8) SDKV(9, 9) SDKV(0x1c, 0x1c) SDKV(max, 0xffffffffu) SDKV(7_shl, 0x70000)

T(sdk_version_7_succeeds)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0;
    NEED();
    EXPECT_EQ(D3D11_SDK_VERSION, 7);
    HRESULT hr = dc_warp(0, NULL, 0, &d, &c, &fl);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    CHECK(d && c); CHECK(dc_fl_known(fl));
    DC_REL(d, c);
}

T(sdk_version_checked_before_driver_type)
{
    /* an SDK mismatch is reported even for a combination that is also invalid for another reason */
    NEED();
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, 6, "bad sdk + unknown w/o adapter");
}

/* ---- driver type / adapter / software module ---- */
T(unknown_without_adapter)
{
    NEED();
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, "UNKNOWN, no adapter");
}
T(unknown_without_adapter_with_levels)
{
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    NEED();
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, l, 2, D3D11_SDK_VERSION, "UNKNOWN, no adapter, levels");
}
T(unknown_without_adapter_with_software_module)
{
    NEED();
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_UNKNOWN, GetModuleHandleA("kernel32.dll"), 0, NULL, 0,
                           D3D11_SDK_VERSION, "UNKNOWN, no adapter, software");
}

#define ADAPTER_DRV(nm, drv) T(adapter_with_##nm) { IDXGIAdapter *a; NEED(); \
    if (!dc_get_adapter0(&a)) { t_skip("no DXGI adapter"); return; } \
    dc_expect_fail_cleared(E_INVALIDARG, a, drv, NULL, 0, NULL, 0, D3D11_SDK_VERSION, "adapter + " #drv); \
    IDXGIAdapter_Release(a); }
ADAPTER_DRV(hardware, D3D_DRIVER_TYPE_HARDWARE)
ADAPTER_DRV(reference, D3D_DRIVER_TYPE_REFERENCE)
ADAPTER_DRV(null, D3D_DRIVER_TYPE_NULL)
ADAPTER_DRV(software, D3D_DRIVER_TYPE_SOFTWARE)
ADAPTER_DRV(warp, D3D_DRIVER_TYPE_WARP)

T(adapter_with_warp_levels_given)
{
    IDXGIAdapter *a; static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_11_0 };
    NEED();
    if (!dc_get_adapter0(&a)) { t_skip("no DXGI adapter"); return; }
    dc_expect_fail_cleared(E_INVALIDARG, a, D3D_DRIVER_TYPE_WARP, NULL, 0, l, 1, D3D11_SDK_VERSION, "adapter + WARP + levels");
    IDXGIAdapter_Release(a);
}
T(adapter_unknown_with_software_module)
{
    IDXGIAdapter *a;
    NEED();
    if (!dc_get_adapter0(&a)) { t_skip("no DXGI adapter"); return; }
    dc_expect_fail_cleared(E_INVALIDARG, a, D3D_DRIVER_TYPE_UNKNOWN, GetModuleHandleA("kernel32.dll"), 0, NULL, 0,
                           D3D11_SDK_VERSION, "adapter + UNKNOWN + software module");
    IDXGIAdapter_Release(a);
}

T(software_without_module)
{
    NEED();
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_SOFTWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, "SOFTWARE, no module");
}
T(software_without_module_with_flags)
{
    NEED();
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_SOFTWARE, NULL, D3D11_CREATE_DEVICE_SINGLETHREADED, NULL, 0,
                           D3D11_SDK_VERSION, "SOFTWARE, no module, flags");
}
T(software_with_non_driver_module)
{
    NEED();
    /* kernel32 exports no rasteriser entry point: the loader's GetProcAddress failure is returned (Win10 22H2) */
    dc_expect_fail_cleared(HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND), NULL, D3D_DRIVER_TYPE_SOFTWARE, GetModuleHandleA("kernel32.dll"), 0, NULL, 0,
                           D3D11_SDK_VERSION, "SOFTWARE, kernel32 as rasteriser");
    dc_expect_fail_cleared(HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND), NULL, D3D_DRIVER_TYPE_SOFTWARE, t_module("dxgi.dll"), 0, NULL, 0,
                           D3D11_SDK_VERSION, "SOFTWARE, dxgi as rasteriser");
}
T(software_with_exe_module)
{
    NEED();
    dc_expect_fail_cleared(HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND), NULL, D3D_DRIVER_TYPE_SOFTWARE, GetModuleHandleA(NULL), 0, NULL, 0, D3D11_SDK_VERSION,
                           "SOFTWARE, the exe as rasteriser");
}
/* Any module that exports the user-mode-driver entry point works as the "software rasteriser". Win10 22H2: d3d11.dll and
 * d3d10warp.dll both give a feature-level 11_0 device, d3d10core.dll one limited to feature level 10_1. */
static void sw_module_ok(const char *dll, D3D_FEATURE_LEVEL want_fl)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    HMODULE m = t_module(dll);
    if (!m) { t_skip("%s not loadable", dll); return; }
    HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_SOFTWARE, m, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, &c);
    t_check(hr == S_OK && d && c, __FILE__, __LINE__, "SOFTWARE with module", "%s: %s", dll, d3dh_hr_name(hr));
    t_check(fl == want_fl, __FILE__, __LINE__, "SOFTWARE feature level", "%s: got %#x want %#x", dll, (unsigned)fl, (unsigned)want_fl);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(software_with_d3d11_module) { NEED(); sw_module_ok("d3d11.dll", D3D_FEATURE_LEVEL_11_0); }
T(software_with_d3d10warp_module) { NEED(); sw_module_ok("d3d10warp.dll", D3D_FEATURE_LEVEL_11_0); }
T(software_with_d3d10core_module) { NEED(); sw_module_ok("d3d10core.dll", D3D_FEATURE_LEVEL_10_1); }
T(software_with_missing_module_handle_value)
{
    NEED();
    /* a module handle that is not a loaded module: the GetProcAddress inside the runtime fails */
    dc_expect_fail_cleared(0, NULL, D3D_DRIVER_TYPE_SOFTWARE, (HMODULE)(ULONG_PTR)0x10000, 0, NULL, 0, D3D11_SDK_VERSION, "SOFTWARE, bogus module handle");
}
T(hardware_and_warp_with_software_module_rejected)
{
    NEED();
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_HARDWARE, GetModuleHandleA(NULL), 0, NULL, 0, D3D11_SDK_VERSION, "HARDWARE + module");
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, GetModuleHandleA(NULL), 0, NULL, 0, D3D11_SDK_VERSION, "WARP + module");
}

#define SWMOD(nm, drv) T(software_module_with_##nm) { NEED(); \
    dc_expect_fail_cleared(E_INVALIDARG, NULL, drv, GetModuleHandleA("kernel32.dll"), 0, NULL, 0, D3D11_SDK_VERSION, \
                           "software module + " #drv); }
SWMOD(hardware, D3D_DRIVER_TYPE_HARDWARE)
SWMOD(reference, D3D_DRIVER_TYPE_REFERENCE)
SWMOD(null, D3D_DRIVER_TYPE_NULL)
SWMOD(warp, D3D_DRIVER_TYPE_WARP)

#define BADDRV(nm, v) T(driver_type_##nm) { NEED(); \
    dc_expect_fail_cleared(0, NULL, (D3D_DRIVER_TYPE)(v), NULL, 0, NULL, 0, D3D11_SDK_VERSION, "driver type " #v); }
BADDRV(6, 6) BADDRV(7, 7) BADDRV(100, 100) BADDRV(0x7fffffff, 0x7fffffff) BADDRV(minus1, -1)

/* ---- feature level array validation ---- */
T(levels_null_with_count)
{
    static const UINT counts[] = { 1, 2, 7, 9, 10, 100, 0xffffffffu };
    NEED();
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++)
        dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, counts[i], D3D11_SDK_VERSION, "NULL array with count");
}

static const UINT bad_levels[] = {
    0, 1, 0x1000 /* 1_0_CORE */, 0x8000, 0x9000, 0x9101, 0x9400, 0xa001, 0xa200, 0xafff, 0xb001, 0xb200, 0xc001,
    0xc200 /* 12_2 */, 0xd000, 0xffff, 0x10000, 0x7fffffff, 0x80000000u, 0xffffffffu };

T(levels_invalid_single)
{
    NEED();
    for (unsigned i = 0; i < sizeof bad_levels / sizeof *bad_levels; i++) {
        D3D_FEATURE_LEVEL l = (D3D_FEATURE_LEVEL)bad_levels[i];
        dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, &l, 1, D3D11_SDK_VERSION, "invalid level alone");
        if (t_is_wine()) break;
    }
}
#define BADFL(nm, v) T(level_invalid_##nm) { D3D_FEATURE_LEVEL l = (D3D_FEATURE_LEVEL)(v); NEED(); \
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, &l, 1, D3D11_SDK_VERSION, "level " #v); }
BADFL(1_0_core, 0x1000) BADFL(zero, 0) BADFL(12_2, 0xc200) BADFL(garbage, 0xdeadbeef) BADFL(9_0, 0x9000) BADFL(10_2, 0xa200)

T(levels_invalid_after_valid)
{
    /* every element is validated, not just the one that would be selected */
    D3D_FEATURE_LEVEL l[3] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_1, 0 };
    NEED();
    for (unsigned i = 0; i < sizeof bad_levels / sizeof *bad_levels; i++) {
        l[2] = (D3D_FEATURE_LEVEL)bad_levels[i];
        dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, l, 3, D3D11_SDK_VERSION, "invalid level after valid ones");
        if (t_is_wine()) break;
    }
}
T(levels_invalid_before_valid)
{
    D3D_FEATURE_LEVEL l[2] = { D3D_FEATURE_LEVEL_1_0_CORE, D3D_FEATURE_LEVEL_11_0 };
    NEED();
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, l, 2, D3D11_SDK_VERSION, "1_0_CORE then 11_0");
    l[0] = D3D_FEATURE_LEVEL_12_2;
    dc_expect_fail_cleared(E_INVALIDARG, NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, l, 2, D3D11_SDK_VERSION, "12_2 then 11_0");
}
T(levels_invalid_checked_for_every_driver_type)
{
    D3D_FEATURE_LEVEL l = (D3D_FEATURE_LEVEL)0x1234;
    static const D3D_DRIVER_TYPE dts[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_NULL, D3D_DRIVER_TYPE_WARP };
    NEED();
    for (unsigned i = 0; i < 4; i++)
        dc_expect_fail_cleared(E_INVALIDARG, NULL, dts[i], NULL, 0, &l, 1, D3D11_SDK_VERSION, "invalid level, various drivers");
}
T(levels_invalid_with_adapter)
{
    IDXGIAdapter *a; D3D_FEATURE_LEVEL l = (D3D_FEATURE_LEVEL)0x1234;
    NEED();
    if (!dc_get_adapter0(&a)) { t_skip("no DXGI adapter"); return; }
    dc_expect_fail_cleared(E_INVALIDARG, a, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, &l, 1, D3D11_SDK_VERSION, "invalid level with adapter");
    IDXGIAdapter_Release(a);
}

T(levels_output_overlaps_array)
{
    D3D_FEATURE_LEVEL arr[4] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3 };
    NEED();
    for (int i = 0; i < 4; i++) {
        D3D_FEATURE_LEVEL save[4]; memcpy(save, arr, sizeof arr);
        HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, arr, 4, D3D11_SDK_VERSION, NULL, &arr[i], NULL);
        EXPECT_HR(hr, E_INVALIDARG);
    }
}
T(levels_output_overlaps_array_count_smaller)
{
    /* only the first 'count' elements are the input array: output right behind them is legal */
    struct { D3D_FEATURE_LEVEL in[2]; D3D_FEATURE_LEVEL out; } s = { { D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_1 }, DC_FL_SENTINEL };
    ID3D11Device *d = NULL;
    HRESULT hr;
    NEED();
    hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, s.in, 2, D3D11_SDK_VERSION, &d, &s.out, NULL);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(s.out, D3D_FEATURE_LEVEL_10_0);
    SAFE_RELEASE(d);
}
T(levels_output_before_array_ok)
{
    struct { D3D_FEATURE_LEVEL out; D3D_FEATURE_LEVEL in[2]; } s = { DC_FL_SENTINEL, { D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_1 } };
    ID3D11Device *d = NULL;
    HRESULT hr;
    NEED();
    hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, s.in, 2, D3D11_SDK_VERSION, &d, &s.out, NULL);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(s.out, D3D_FEATURE_LEVEL_10_0);
    EXPECT_EQ(s.in[0], D3D_FEATURE_LEVEL_10_0);
    SAFE_RELEASE(d);
}
T(levels_output_overlap_with_device_failure_still_invalid)
{
    D3D_FEATURE_LEVEL arr[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    ID3D11Device *d = NULL;
    NEED();
    HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, arr, 2, D3D11_SDK_VERSION, &d, &arr[1], NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    CHECK(d == NULL);
}

T(levels_empty_array_nonnull)
{
    D3D_FEATURE_LEVEL l[1] = { D3D_FEATURE_LEVEL_11_0 };
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    NEED();
    HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, l, 0, D3D11_SDK_VERSION, &d, &fl, NULL);
    /* with no level to try no device can be produced */
    EXPECT_HR(hr, DXGI_ERROR_UNSUPPORTED);
    CHECK(d == NULL); EXPECT_EQ(fl, 0);
    SAFE_RELEASE(d);
}

T(levels_many_entries_accepted)
{
    D3D_FEATURE_LEVEL l[16]; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0;
    NEED();
    for (int i = 0; i < 16; i++) l[i] = D3D_FEATURE_LEVEL_10_0;
    HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, l, 16, D3D11_SDK_VERSION, &d, &fl, NULL);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_10_0);
    SAFE_RELEASE(d);
}
T(levels_ten_entries_accepted)
{
    D3D_FEATURE_LEVEL l[10]; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0;
    NEED();
    for (int i = 0; i < 10; i++) l[i] = D3D_FEATURE_LEVEL_9_1;
    HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, l, 10, D3D11_SDK_VERSION, &d, &fl, NULL);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_9_1);
    SAFE_RELEASE(d);
}

/* ---- output pointer combinations ---- */
static void outcombo(unsigned mask)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    HRESULT hr;
    hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
                 (mask & 1) ? &d : NULL, (mask & 2) ? &fl : NULL, (mask & 4) ? &c : NULL);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR_OK(hr);
    if (mask & 1) CHECK(d != NULL);
    if (mask & 4) CHECK(c != NULL);
    if (mask & 2) { CHECK(dc_fl_known(fl)); CHECK(fl <= D3D_FEATURE_LEVEL_11_0); }
    if (!(mask & 1)) CHECK(d == NULL);
    if (!(mask & 4)) CHECK(c == NULL);
    if (!(mask & 2)) EXPECT_EQ(fl, DC_FL_SENTINEL);
    if (c) {
        ID3D11Device *cd = NULL;
        ID3D11DeviceContext_GetDevice(c, &cd);
        CHECK(cd != NULL);
        if (d) CHECK(cd == d);
        SAFE_RELEASE(cd);
    }
    DC_REL(d, c);
}
#define OUTC(m) T(outputs_mask_##m) { REQUIRE(DC_FN()); outcombo(m); }
OUTC(0) OUTC(1) OUTC(2) OUTC(3) OUTC(4) OUTC(5) OUTC(6) OUTC(7)

T(outputs_all_null_with_levels)
{
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_10_0 };
    NEED();
    HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, l, 1, D3D11_SDK_VERSION, NULL, NULL, NULL);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(hr, S_FALSE);   /* validation-only call: success, nothing created */
}
T(outputs_all_null_invalid_args_still_fail)
{
    NEED();
    EXPECT_HR(DC_FN()(NULL, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, 99, NULL, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(DC_FN()(NULL, D3D_DRIVER_TYPE_SOFTWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL, NULL), E_INVALIDARG);
}
T(outputs_failure_clears_preset_levels_only_on_failure)
{
    /* on success *pFeatureLevel is overwritten, on argument failure it is zeroed */
    ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    NEED();
    HRESULT hr = DC_FN()(NULL, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    EXPECT_EQ(fl, 0);
    hr = DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, NULL);
    if (SUCCEEDED(hr)) { CHECK(fl != 0); CHECK(d != NULL); }
    SAFE_RELEASE(d);
}
T(outputs_device_ptr_zeroed_on_failure)
{
    ID3D11Device *d = (ID3D11Device *)(ULONG_PTR)0; ID3D11DeviceContext *c = NULL;
    NEED();
    EXPECT_HR(DC_FN()(NULL, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, NULL, &c), E_INVALIDARG);
    CHECK(d == NULL); CHECK(c == NULL);
}

/* ---- SwapChain entry point shares the validation ---- */
T(sc_sdk_version_invalid)
{
    ID3D11Device *d = NULL; IDXGISwapChain *sc = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    REQUIRE(DC_SC_FN());
    HRESULT hr = DC_SC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, 0, NULL, &sc, &d, &fl, &c);
    EXPECT_HR(hr, E_INVALIDARG);
    CHECK(d == NULL); CHECK(sc == NULL); CHECK(c == NULL); EXPECT_EQ(fl, 0);
}
T(sc_unknown_without_adapter)
{
    ID3D11Device *d = NULL; IDXGISwapChain *sc = NULL;
    REQUIRE(DC_SC_FN());
    EXPECT_HR(DC_SC_FN()(NULL, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, &sc, &d, NULL, NULL), E_INVALIDARG);
    CHECK(d == NULL); CHECK(sc == NULL);
}
T(sc_adapter_with_warp)
{
    IDXGIAdapter *a; ID3D11Device *d = NULL; IDXGISwapChain *sc = NULL;
    REQUIRE(DC_SC_FN());
    if (!dc_get_adapter0(&a)) { t_skip("no DXGI adapter"); return; }
    EXPECT_HR(DC_SC_FN()(a, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, &sc, &d, NULL, NULL), E_INVALIDARG);
    CHECK(d == NULL); CHECK(sc == NULL);
    IDXGIAdapter_Release(a);
}
T(sc_software_without_module)
{
    ID3D11Device *d = NULL; IDXGISwapChain *sc = NULL;
    REQUIRE(DC_SC_FN());
    EXPECT_HR(DC_SC_FN()(NULL, D3D_DRIVER_TYPE_SOFTWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, &sc, &d, NULL, NULL), E_INVALIDARG);
    CHECK(d == NULL); CHECK(sc == NULL);
}
T(sc_software_module_with_warp)
{
    ID3D11Device *d = NULL; IDXGISwapChain *sc = NULL;
    REQUIRE(DC_SC_FN());
    EXPECT_HR(DC_SC_FN()(NULL, D3D_DRIVER_TYPE_WARP, GetModuleHandleA("kernel32.dll"), 0, NULL, 0, D3D11_SDK_VERSION, NULL, &sc, &d, NULL, NULL), E_INVALIDARG);
    CHECK(d == NULL); CHECK(sc == NULL);
}
T(sc_levels_invalid)
{
    D3D_FEATURE_LEVEL l = D3D_FEATURE_LEVEL_1_0_CORE; ID3D11Device *d = NULL; IDXGISwapChain *sc = NULL;
    REQUIRE(DC_SC_FN());
    EXPECT_HR(DC_SC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, &l, 1, D3D11_SDK_VERSION, NULL, &sc, &d, NULL, NULL), E_INVALIDARG);
    CHECK(d == NULL); CHECK(sc == NULL);
}
T(sc_levels_null_with_count)
{
    ID3D11Device *d = NULL;
    REQUIRE(DC_SC_FN());
    EXPECT_HR(DC_SC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 3, D3D11_SDK_VERSION, NULL, NULL, &d, NULL, NULL), E_INVALIDARG);
    CHECK(d == NULL);
}
T(sc_levels_output_overlap)
{
    D3D_FEATURE_LEVEL arr[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    REQUIRE(DC_SC_FN());
    EXPECT_HR(DC_SC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, arr, 2, D3D11_SDK_VERSION, NULL, NULL, NULL, &arr[0], NULL), E_INVALIDARG);
}
