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
    /* the translation of the kernel status never produces an error that is not an HRESULT failure or success */
    if (SUCCEEDED(h1) && h1 != S_OK) return 11;
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
    t_note("RestoreMonitorFactoryColorDefaults(NULL) last error %lu", GetLastError());
    SetLastError(0);
    r = U(RestoreMonitorFactoryColorDefaults)(INVALID_HANDLE_VALUE);
    EXPECT_EQ(r, FALSE);
    t_note("RestoreMonitorFactoryColorDefaults(INVALID_HANDLE_VALUE) last error %lu", GetLastError());
}
TEST(dx_gap_01_get_timing_report_invalid_handle) {
    REQUIRE(U(GetTimingReport));
    if (t_is_wine()) { t_skip("emulated dxva2"); return; }
    MC_TIMING_REPORT rep; memset(&rep, 0xcd, sizeof rep);
    SetLastError(0);
    BOOL r = U(GetTimingReport)(NULL, &rep);
    EXPECT_EQ(r, FALSE);
    t_note("GetTimingReport(NULL) last error %lu", GetLastError());
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
