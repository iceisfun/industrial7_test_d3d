/* State-changing exports from the gap_01 group. Run with the destructive areas (tools\run_all.bat /destructive).
 * Process-wide effects are confined to child processes; monitor calls only ever receive invalid handles, so no monitor setting can change. */
#include "d3dh.h"
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <lowlevelmonitorconfigurationapi.h>

#define G1_DXGI_ERROR_INVALID_CALL ((HRESULT)0x887A0001)
#define G1_DXGI_ERROR_ALREADY_EXISTS ((HRESULT)0x887A0036)

UDECL(HRESULT, DXGIDeclareAdapterRemovalSupport, (void));

/* ---- DXGIDeclareAdapterRemovalSupport: process-wide, one shot, so it only runs in a child ---- */
CHILD(dxg1_declare_removal_twice) {
    HRESULT (WINAPI *f)(void) = (HRESULT (WINAPI *)(void))t_proc("DXGIDeclareAdapterRemovalSupport");
    if (!f) return 90;
    HRESULT h1 = f();
    HRESULT h2 = f();
    printf("child: first %s second %s\n", d3dh_hr_name(h1), d3dh_hr_name(h2));
    /* Windows 10: the first declaration succeeds, repeating it is DXGI_ERROR_ALREADY_EXISTS */
    if (h1 != S_OK) return 11;
    if (h2 != G1_DXGI_ERROR_ALREADY_EXISTS) return 13;
    /* a factory can still be created afterwards */
    IDXGIFactory1 *fac = NULL;
    HRESULT hf = d3dh_make_factory1(&fac);
    if (FAILED(hf) || !fac) return 12;
    IDXGIFactory1_Release(fac);
    return 0;
}
TEST(dx_gap_01_declare_removal_support_in_child) {
    REQUIRE(UN(DXGIDeclareAdapterRemovalSupport));
    if (t_is_wine()) { t_skip("emulated dxgi"); return; }
    DWORD rc = t_run_child("dxg1_declare_removal_twice", "", 20000);
    if (rc == 12) { t_skip("no DXGI factory in this environment"); return; }
    EXPECT_EQ(rc, 0);
}
CHILD(dxg1_declare_removal_then_device) {
    HRESULT (WINAPI *f)(void) = (HRESULT (WINAPI *)(void))t_proc("DXGIDeclareAdapterRemovalSupport");
    if (!f) return 90;
    HRESULT h = f();
    printf("child: declare -> %s\n", d3dh_hr_name(h));
    if (h != S_OK) return 22;
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE dt;
    HRESULT hd = d3dh_make_device11(&dev, &ctx, &fl, &dt, 0);
    if (FAILED(hd) || !dev) return 20;
    /* a working device remains usable, and the removed reason is the healthy S_OK */
    HRESULT rr = ID3D11Device_GetDeviceRemovedReason(dev);
    ID3D11DeviceContext_Release(ctx);
    ID3D11Device_Release(dev);
    return rr == S_OK ? 0 : 21;
}
TEST(dx_gap_01_declare_removal_support_then_device) {
    REQUIRE(UN(DXGIDeclareAdapterRemovalSupport));
    if (t_is_wine()) { t_skip("emulated dxgi"); return; }
    DWORD rc = t_run_child("dxg1_declare_removal_then_device", "", 30000);
    if (rc == 20) { t_skip("no D3D11 device in this environment"); return; }
    EXPECT_EQ(rc, 0);
}

/* ---- D3DPERF_SetOptions ---- */
TEST(dx_gap_01_d3dperf_setoptions_does_not_change_status) {
    REQUIRE(UIN("d3d9.dll", D3DPERF_SetOptions)); REQUIRE(UIN("d3d9.dll", D3DPERF_GetStatus));
    DWORD before = UIN("d3d9.dll", D3DPERF_GetStatus)();
    UIN("d3d9.dll", D3DPERF_SetOptions)(0);
    EXPECT_EQ(UIN("d3d9.dll", D3DPERF_GetStatus)(), before);
    UIN("d3d9.dll", D3DPERF_SetOptions)(1);           /* D3DPERF disable request: only meaningful for an attached profiler */
    DWORD after = UIN("d3d9.dll", D3DPERF_GetStatus)();
    if (!before) EXPECT_EQ(after, 0);
    /* markers remain callable afterwards */
    UIN("d3d9.dll", D3DPERF_SetMarker)(0, L"after options");
    UIN("d3d9.dll", D3DPERF_SetRegion)(0, L"after options");
}

/* ---- dxva2 monitor entry points: invalid handles only ---- */
TEST(dx_gap_01_restore_factory_color_defaults_invalid_handle) {
    REQUIRE(U(RestoreMonitorFactoryColorDefaults));
    if (t_is_wine()) { t_skip("emulated dxva2"); return; }
    SetLastError(0);
    BOOL r = U(RestoreMonitorFactoryColorDefaults)(NULL);
    EXPECT_EQ(r, FALSE);
    EXPECT_ERR(0xC026258Cu);
    SetLastError(0);
    r = U(RestoreMonitorFactoryColorDefaults)(INVALID_HANDLE_VALUE);
    EXPECT_EQ(r, FALSE);
    EXPECT_ERR(0xC026258Cu);
}
TEST(dx_gap_01_get_timing_report_invalid_handle) {
    REQUIRE(U(GetTimingReport));
    if (t_is_wine()) { t_skip("emulated dxva2"); return; }
    MC_TIMING_REPORT rep; memset(&rep, 0xcd, sizeof rep);
    SetLastError(0);
    BOOL r = U(GetTimingReport)(NULL, &rep);
    EXPECT_EQ(r, FALSE);
    EXPECT_ERR(0xC026258Cu);
    SetLastError(0);
    r = U(GetTimingReport)(INVALID_HANDLE_VALUE, &rep);
    EXPECT_EQ(r, FALSE);
}
TEST(dx_gap_01_monitor_calls_are_stable_when_repeated) {
    REQUIRE(U(RestoreMonitorFactoryColorDefaults)); REQUIRE(U(GetTimingReport));
    if (t_is_wine()) { t_skip("emulated dxva2"); return; }
    MC_TIMING_REPORT rep;
    for (int i = 0; i < 50; i++) {
        BOOL a = U(RestoreMonitorFactoryColorDefaults)(NULL);
        BOOL b = U(GetTimingReport)(NULL, &rep);
        if (a || b) { CHECKF(0, "iteration %d: invalid handle accepted (%d %d)", i, a, b); break; }
    }
}

/* The declaration must precede device creation: after a D3D11 device exists it is rejected, but creating only a factory does not lock it. */
CHILD(dxg1_declare_after_factory) {
    HRESULT (WINAPI *f)(void) = (HRESULT (WINAPI *)(void))t_proc("DXGIDeclareAdapterRemovalSupport");
    if (!f) return 90;
    IDXGIFactory1 *fac = NULL;
    if (FAILED(d3dh_make_factory1(&fac)) || !fac) return 12;
    HRESULT h = f();
    IDXGIFactory1_Release(fac);
    return h == S_OK ? 0 : 31;
}
CHILD(dxg1_declare_after_device) {
    HRESULT (WINAPI *f)(void) = (HRESULT (WINAPI *)(void))t_proc("DXGIDeclareAdapterRemovalSupport");
    if (!f) return 90;
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL;
    if (FAILED(d3dh_make_device11(&dev, &ctx, NULL, NULL, 0)) || !dev) return 20;
    HRESULT h = f();
    ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
    return h == DXGI_ERROR_INVALID_CALL ? 0 : 32;
}
TEST(dx_gap_01_declare_removal_support_ordering) {
    REQUIRE(UN(DXGIDeclareAdapterRemovalSupport));
    DWORD rc = t_run_child("dxg1_declare_after_factory", "", 30000);
    if (rc == 12) { t_skip("no DXGI factory in this environment"); return; }
    EXPECT_EQ(rc, 0);
    rc = t_run_child("dxg1_declare_after_device", "", 30000);
    if (rc == 20) { t_skip("no D3D11 device in this environment"); return; }
    EXPECT_EQ(rc, 0);
}

/* ---- D3DPERF without an attached profiler (Windows 10): every event call reports -1, status 0, no repeat frame ---- */
TEST(dx_gap_01_d3dperf_calls_without_profiler) {
    typedef int (WINAPI *be_t)(DWORD, LPCWSTR); typedef int (WINAPI *ee_t)(void); typedef void (WINAPI *mk_t)(DWORD, LPCWSTR);
    typedef BOOL (WINAPI *qr_t)(void); typedef DWORD (WINAPI *gs_t)(void); typedef void (WINAPI *so_t)(DWORD);
    be_t be = (be_t)t_proc_in("d3d9.dll", "D3DPERF_BeginEvent"); ee_t ee = (ee_t)t_proc_in("d3d9.dll", "D3DPERF_EndEvent");
    mk_t sm = (mk_t)t_proc_in("d3d9.dll", "D3DPERF_SetMarker"), sr = (mk_t)t_proc_in("d3d9.dll", "D3DPERF_SetRegion");
    qr_t qr = (qr_t)t_proc_in("d3d9.dll", "D3DPERF_QueryRepeatFrame"); gs_t gs = (gs_t)t_proc_in("d3d9.dll", "D3DPERF_GetStatus");
    so_t so = (so_t)t_proc_in("d3d9.dll", "D3DPERF_SetOptions");
    REQUIRE(be); REQUIRE(ee); REQUIRE(sm); REQUIRE(sr); REQUIRE(qr); REQUIRE(gs); REQUIRE(so);
    EXPECT_EQ(gs(), 0);
    EXPECT_EQ((int)be(0xff00ff00u, L"outer"), -1);
    EXPECT_EQ((int)be(0, L"inner"), -1);
    EXPECT_EQ((int)ee(), -1);
    EXPECT_EQ((int)ee(), -1);
    EXPECT_EQ((int)ee(), -1);                      /* unbalanced EndEvent is harmless */
    EXPECT_EQ((int)be(0, NULL), -1);               /* NULL names are accepted */
    ee();
    sm(0, NULL); sr(0, NULL);
    EXPECT_EQ(qr(), FALSE);
    so(1);                                          /* disable request: still no profiler attached */
    EXPECT_EQ(gs(), 0);
    EXPECT_EQ((int)be(0, L"x"), -1);
    ee();
    so(0);
    EXPECT_EQ(gs(), 0);
}

/* ---- dxva2 monitor entry points: every one rejects a NULL or INVALID_HANDLE_VALUE physical monitor handle with the same last error ---- */
#define G1_MON_ERR 0xC026258Cu
#define MONCASE(name, call) do { \
    HANDLE hs_[2] = { NULL, INVALID_HANDLE_VALUE }; \
    for (int k_ = 0; k_ < 2; k_++) { \
        HANDLE h = hs_[k_]; DWORD a = 0xaaaa, b = 0xbbbb, c = 0xcccc; char buf[64] = { 0 }; MC_TIMING_REPORT tr; MC_VCP_CODE_TYPE vt; \
        (void)h; (void)a; (void)b; (void)c; (void)buf; (void)tr; (void)vt; \
        SetLastError(0); \
        BOOL r_ = (BOOL)(call); \
        DWORD e_ = GetLastError(); \
        t_check(r_ == FALSE && e_ == G1_MON_ERR, __FILE__, __LINE__, name, "%s: returned %d, last error %#lx (want FALSE, %#x)", k_ ? "INVALID_HANDLE_VALUE" : "NULL", (int)r_, (unsigned long)e_, G1_MON_ERR); \
    } } while (0)
TEST(dx_gap_01_monitor_entry_points_reject_invalid_handles) {
    if (t_is_wine()) { t_skip("emulated dxva2"); return; }
    REQUIRE(U(GetMonitorBrightness));
    MONCASE("GetCapabilitiesStringLength", U(GetCapabilitiesStringLength)(h, &a));
    MONCASE("CapabilitiesRequestAndCapabilitiesReply", U(CapabilitiesRequestAndCapabilitiesReply)(h, buf, sizeof buf));
    MONCASE("DegaussMonitor", U(DegaussMonitor)(h));
    MONCASE("SaveCurrentMonitorSettings", U(SaveCurrentMonitorSettings)(h));
    MONCASE("SaveCurrentSettings", U(SaveCurrentSettings)(h));
    MONCASE("RestoreMonitorFactoryDefaults", U(RestoreMonitorFactoryDefaults)(h));
    MONCASE("RestoreMonitorFactoryColorDefaults", U(RestoreMonitorFactoryColorDefaults)(h));
    MONCASE("GetMonitorTechnologyType", U(GetMonitorTechnologyType)(h, (MC_DISPLAY_TECHNOLOGY_TYPE *)&a));
    MONCASE("GetMonitorCapabilities", U(GetMonitorCapabilities)(h, &a, &b));
    MONCASE("GetMonitorBrightness", U(GetMonitorBrightness)(h, &a, &b, &c));
    MONCASE("GetMonitorContrast", U(GetMonitorContrast)(h, &a, &b, &c));
    MONCASE("GetMonitorColorTemperature", U(GetMonitorColorTemperature)(h, (LPMC_COLOR_TEMPERATURE)&a));
    MONCASE("GetMonitorDisplayAreaSize", U(GetMonitorDisplayAreaSize)(h, MC_WIDTH, &a, &b, &c));
    MONCASE("GetMonitorDisplayAreaPosition", U(GetMonitorDisplayAreaPosition)(h, MC_HORIZONTAL_POSITION, &a, &b, &c));
    MONCASE("GetMonitorRedGreenOrBlueDrive", U(GetMonitorRedGreenOrBlueDrive)(h, MC_RED_DRIVE, &a, &b, &c));
    MONCASE("GetMonitorRedGreenOrBlueGain", U(GetMonitorRedGreenOrBlueGain)(h, MC_RED_GAIN, &a, &b, &c));
    MONCASE("GetTimingReport", U(GetTimingReport)(h, &tr));
    MONCASE("GetVCPFeatureAndVCPFeatureReply", U(GetVCPFeatureAndVCPFeatureReply)(h, 0x10, &vt, &a, &b));
    MONCASE("SetVCPFeature", U(SetVCPFeature)(h, 0x10, 1));
    MONCASE("SetMonitorBrightness", U(SetMonitorBrightness)(h, 50));
    MONCASE("SetMonitorContrast", U(SetMonitorContrast)(h, 50));
    MONCASE("SetMonitorColorTemperature", U(SetMonitorColorTemperature)(h, MC_COLOR_TEMPERATURE_6500K));
    MONCASE("SetMonitorDisplayAreaPosition", U(SetMonitorDisplayAreaPosition)(h, MC_HORIZONTAL_POSITION, 1));
    MONCASE("SetMonitorDisplayAreaSize", U(SetMonitorDisplayAreaSize)(h, MC_WIDTH, 1));
    MONCASE("SetMonitorRedGreenOrBlueDrive", U(SetMonitorRedGreenOrBlueDrive)(h, MC_RED_DRIVE, 1));
    MONCASE("SetMonitorRedGreenOrBlueGain", U(SetMonitorRedGreenOrBlueGain)(h, MC_RED_GAIN, 1));
    MONCASE("DestroyPhysicalMonitor", U(DestroyPhysicalMonitor)(h));
}
/* a failing monitor read leaves its output parameters alone */
TEST(dx_gap_01_monitor_failed_reads_leave_outputs) {
    if (t_is_wine()) { t_skip("emulated dxva2"); return; }
    REQUIRE(U(GetMonitorBrightness)); REQUIRE(U(GetMonitorCapabilities));
    DWORD a = 0xaaaa, b = 0xbbbb, c = 0xcccc;
    EXPECT_EQ(U(GetMonitorBrightness)(NULL, &a, &b, &c), FALSE);
    EXPECT_EQ(a, 0xaaaa); EXPECT_EQ(b, 0xbbbb); EXPECT_EQ(c, 0xcccc);
    EXPECT_EQ(U(GetMonitorCapabilities)(NULL, &a, &b), FALSE);
    EXPECT_EQ(a, 0xaaaa); EXPECT_EQ(b, 0xbbbb);
}
/* HMONITOR based entry points: a NULL or bogus HMONITOR is ERROR_INVALID_MONITOR_HANDLE (1461) and *count is untouched; zero handles to destroy succeed. */
TEST(dx_gap_01_hmonitor_entry_points_invalid_handles) {
    if (t_is_wine()) { t_skip("emulated dxva2"); return; }
    REQUIRE(U(GetNumberOfPhysicalMonitorsFromHMONITOR)); REQUIRE(U(GetPhysicalMonitorsFromHMONITOR)); REQUIRE(U(DestroyPhysicalMonitors));
    DWORD n = 0xaa; BOOL r; PHYSICAL_MONITOR pm[2];
    SetLastError(0); r = U(GetNumberOfPhysicalMonitorsFromHMONITOR)(NULL, &n);
    EXPECT_EQ(r, FALSE); EXPECT_ERR(1461); EXPECT_EQ(n, 0xaa);
    n = 0xaa; SetLastError(0); r = U(GetNumberOfPhysicalMonitorsFromHMONITOR)((HMONITOR)(ULONG_PTR)1, &n);
    EXPECT_EQ(r, FALSE); EXPECT_ERR(1461); EXPECT_EQ(n, 0xaa);
    SetLastError(0); r = U(GetPhysicalMonitorsFromHMONITOR)(NULL, 2, pm);
    EXPECT_EQ(r, FALSE); EXPECT_ERR(1461);
    SetLastError(0); r = U(GetPhysicalMonitorsFromHMONITOR)(NULL, 0, pm);
    EXPECT_EQ(r, FALSE); EXPECT_ERR(1461);
    r = U(DestroyPhysicalMonitors)(0, NULL);
    EXPECT_EQ(r, TRUE);
}
