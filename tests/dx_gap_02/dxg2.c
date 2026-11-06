#include "d3dh.h"
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <lowlevelmonitorconfigurationapi.h>

/* STATE-CHANGING: talks to a real monitor over DDC/CI. Run only with /destructive.
 * Every Set call writes back the value just read, so the monitor ends up unchanged. */

/* Returns 1 when the primary monitor has exactly one physical monitor and it is safe to talk to.
 * *live is 1 when the physical monitor has a DDC/CI handle. A monitor without a handle (e.g. the "Generic PnP Monitor" of a virtual display, hPhysicalMonitor == NULL)
 * cannot reach any hardware, so it is always safe; live DDC/CI writes happen only when explicitly requested (TP_ALLOW_MONITOR_WRITE=1). */
static void rel(PHYSICAL_MONITOR *pm);
static int first_phys(PHYSICAL_MONITOR *pm, int *live)
{
    char ev[8];
    BOOL (WINAPI *num)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *get)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR);
    if (!num || !get) return 0;
    HMONITOR hm = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    DWORD n = 0;
    if (!hm || !num(hm, &n) || n != 1) return 0;
    memset(pm, 0, sizeof *pm);
    if (!get(hm, 1, pm)) return 0;
    *live = pm->hPhysicalMonitor != NULL;
    if (*live && (!GetEnvironmentVariableA("TP_ALLOW_MONITOR_WRITE", ev, sizeof ev) || ev[0] != '1')) return 0;
    return 1;
}
static void rel(PHYSICAL_MONITOR *pm) { BOOL (WINAPI *d)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors); if (d) d(1, pm); }

typedef BOOL (WINAPI *get_fn)(HANDLE, LPDWORD, LPDWORD, LPDWORD);
typedef BOOL (WINAPI *set_fn)(HANDLE, DWORD);

static void roundtrip(get_fn g, set_fn s, const char *nm)
{
    PHYSICAL_MONITOR pm; int live;
    if (!g || !s) { t_skip("missing export"); return; }
    if (!first_phys(&pm, &live)) { t_skip("DDC/CI monitor present but live writes disabled (set TP_ALLOW_MONITOR_WRITE=1), or not exactly one physical monitor"); return; }
    if (!live) {
        /* Measured on Win10 22H2 in a VM: the monitor has no DDC/CI handle; Get and Set both fail with ERROR_NOT_SUPPORTED */
        DWORD a = 0, b = 0, c = 0;
        SET_ERR(0); BOOL rg = g(pm.hPhysicalMonitor, &a, &b, &c); EXPECT_FAIL(!rg, ERROR_NOT_SUPPORTED);
        SET_ERR(0); BOOL rs = s(pm.hPhysicalMonitor, 50); EXPECT_FAIL(!rs, ERROR_NOT_SUPPORTED);
        rel(&pm); return;
    }
    DWORD mn, cu, mx;
    if (!g(pm.hPhysicalMonitor, &mn, &cu, &mx)) { t_skip("%s not readable (GLE=%lu)", nm, GetLastError()); rel(&pm); return; }
    SET_ERR(0);
    BOOL r = s(pm.hPhysicalMonitor, cu);
    DWORD e = GetLastError();
    if (!r) { t_note("%s set failed GLE=%lu", nm, e); rel(&pm); return; }
    DWORD mn2, cu2, mx2;
    if (g(pm.hPhysicalMonitor, &mn2, &cu2, &mx2)) {
        EXPECT_EQ(mn2, mn); EXPECT_EQ(mx2, mx);
        if (cu2 != cu) t_note("%s read back %lu after writing %lu", nm, cu2, cu);
    }
    rel(&pm);
}
TEST(dxgap2_brightness_write_current_value_roundtrip) { roundtrip(U(GetMonitorBrightness), U(SetMonitorBrightness), "brightness"); }
TEST(dxgap2_contrast_write_current_value_roundtrip) { roundtrip(U(GetMonitorContrast), U(SetMonitorContrast), "contrast"); }

TEST(dxgap2_save_current_settings_live) {
    BOOL (WINAPI *a)(HANDLE) = U(SaveCurrentMonitorSettings);
    BOOL (WINAPI *b)(HANDLE) = U(SaveCurrentSettings);
    PHYSICAL_MONITOR pm; int live;
    REQUIRE(a); REQUIRE(b);
    if (!first_phys(&pm, &live)) { t_skip("DDC/CI monitor present but live writes disabled (set TP_ALLOW_MONITOR_WRITE=1), or not exactly one physical monitor"); return; }
    if (!live) {
        SET_ERR(0); BOOL r1 = a(pm.hPhysicalMonitor); EXPECT_FAIL(!r1, ERROR_NOT_SUPPORTED);
        SET_ERR(0); BOOL r2 = b(pm.hPhysicalMonitor); EXPECT_FAIL(!r2, ERROR_NOT_SUPPORTED);
        rel(&pm); return;
    }
    SET_ERR(0); BOOL ra = a(pm.hPhysicalMonitor); DWORD ea = GetLastError();
    SET_ERR(0); BOOL rb = b(pm.hPhysicalMonitor); DWORD eb = GetLastError();
    t_note("save: %d/%lu and %d/%lu", ra, ea, rb, eb);
    CHECK((ra != 0) == (rb != 0));
    if (!ra) CHECK(ea == eb);
    rel(&pm);
}
TEST(dxgap2_restore_factory_defaults_not_called) {
    /* RestoreMonitorFactoryDefaults would reset the user's real monitor; deliberately not issued against live hardware. */
    REQUIRE(U(RestoreMonitorFactoryDefaults));
    t_note("RestoreMonitorFactoryDefaults only exercised with invalid handles in gap_02");
}

/* ---- the remaining state-changing and reading entry points against a monitor without DDC/CI ---- */
#define OK_NOT_SUPPORTED(call, name) do { SET_ERR(0); BOOL r_ = (call); DWORD e_ = GetLastError(); \
    t_check(!r_ && e_ == ERROR_NOT_SUPPORTED, __FILE__, __LINE__, name, "%s: ret %d GLE 0x%lx (want FALSE / ERROR_NOT_SUPPORTED)", name, r_, e_); } while (0)
TEST(dxgap2_all_ddc_apis_unsupported_without_ddc_handle) {
    PHYSICAL_MONITOR pm; int live;
    if (!first_phys(&pm, &live)) { t_skip("DDC/CI monitor present but live writes disabled, or not exactly one physical monitor"); return; }
    if (live) { t_skip("monitor has a live DDC/CI handle: write-back coverage is in the roundtrip tests"); rel(&pm); return; }
    HANDLE h = pm.hPhysicalMonitor;    /* NULL */
    DWORD a = 0, b = 0, c = 0; MC_TIMING_REPORT tr; MC_COLOR_TEMPERATURE ct; MC_DISPLAY_TECHNOLOGY_TYPE tt; MC_VCP_CODE_TYPE vt; char cs[64];
    OK_NOT_SUPPORTED(U(DegaussMonitor)(h), "DegaussMonitor");
    OK_NOT_SUPPORTED(U(RestoreMonitorFactoryColorDefaults)(h), "RestoreMonitorFactoryColorDefaults");
    OK_NOT_SUPPORTED(U(RestoreMonitorFactoryDefaults)(h), "RestoreMonitorFactoryDefaults");
    OK_NOT_SUPPORTED(U(SetMonitorColorTemperature)(h, MC_COLOR_TEMPERATURE_6500K), "SetMonitorColorTemperature");
    OK_NOT_SUPPORTED(U(GetMonitorColorTemperature)(h, &ct), "GetMonitorColorTemperature");
    OK_NOT_SUPPORTED(U(SetMonitorDisplayAreaPosition)(h, MC_HORIZONTAL_POSITION, 10), "SetMonitorDisplayAreaPosition");
    OK_NOT_SUPPORTED(U(GetMonitorDisplayAreaPosition)(h, MC_HORIZONTAL_POSITION, &a, &b, &c), "GetMonitorDisplayAreaPosition");
    OK_NOT_SUPPORTED(U(SetMonitorDisplayAreaSize)(h, MC_WIDTH, 10), "SetMonitorDisplayAreaSize");
    OK_NOT_SUPPORTED(U(GetMonitorDisplayAreaSize)(h, MC_WIDTH, &a, &b, &c), "GetMonitorDisplayAreaSize");
    OK_NOT_SUPPORTED(U(SetMonitorRedGreenOrBlueDrive)(h, MC_RED_DRIVE, 10), "SetMonitorRedGreenOrBlueDrive");
    OK_NOT_SUPPORTED(U(GetMonitorRedGreenOrBlueDrive)(h, MC_RED_DRIVE, &a, &b, &c), "GetMonitorRedGreenOrBlueDrive");
    OK_NOT_SUPPORTED(U(SetMonitorRedGreenOrBlueGain)(h, MC_RED_GAIN, 10), "SetMonitorRedGreenOrBlueGain");
    OK_NOT_SUPPORTED(U(GetMonitorRedGreenOrBlueGain)(h, MC_RED_GAIN, &a, &b, &c), "GetMonitorRedGreenOrBlueGain");
    OK_NOT_SUPPORTED(U(SetVCPFeature)(h, 0x10, 50), "SetVCPFeature");
    OK_NOT_SUPPORTED(U(GetVCPFeatureAndVCPFeatureReply)(h, 0x10, &vt, &a, &b), "GetVCPFeatureAndVCPFeatureReply");
    OK_NOT_SUPPORTED(U(GetCapabilitiesStringLength)(h, &a), "GetCapabilitiesStringLength");
    OK_NOT_SUPPORTED(U(CapabilitiesRequestAndCapabilitiesReply)(h, cs, sizeof cs), "CapabilitiesRequestAndCapabilitiesReply");
    OK_NOT_SUPPORTED(U(GetTimingReport)(h, &tr), "GetTimingReport");
    OK_NOT_SUPPORTED(U(GetMonitorTechnologyType)(h, &tt), "GetMonitorTechnologyType");
    OK_NOT_SUPPORTED(U(GetMonitorCapabilities)(h, &a, &b), "GetMonitorCapabilities");
    OK_NOT_SUPPORTED(U(SetMonitorBrightness)(NULL, 50), "SetMonitorBrightness(NULL)");
    rel(&pm);
}

TEST(dxgap2_set_calls_with_bogus_handles) {
    /* Measured on Win10 22H2: a NULL handle is ERROR_NOT_SUPPORTED, a non-NULL handle that is not a physical monitor fails with the driver status 0xC026258C,
     * and the monitor-handle query rejects a NULL HMONITOR with ERROR_INVALID_MONITOR_HANDLE. Nothing here reaches hardware. */
    REQUIRE(U(SetMonitorBrightness)); REQUIRE(U(SetMonitorContrast)); REQUIRE(U(SaveCurrentSettings)); REQUIRE(U(RestoreMonitorFactoryDefaults));
    HANDLE bogus[2] = { (HANDLE)(ULONG_PTR)0x1234, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0); BOOL r = U(SetMonitorBrightness)(bogus[i], 50); DWORD e = GetLastError(); CHECKF(!r && e == 0xC026258C, "brightness bogus %d: %d / 0x%lx", i, r, e);
        SET_ERR(0); r = U(SetMonitorContrast)(bogus[i], 50); e = GetLastError(); CHECKF(!r && e == 0xC026258C, "contrast bogus %d: %d / 0x%lx", i, r, e);
        SET_ERR(0); r = U(SaveCurrentSettings)(bogus[i]); e = GetLastError(); CHECKF(!r && e == 0xC026258C, "save bogus %d: %d / 0x%lx", i, r, e);
        SET_ERR(0); r = U(RestoreMonitorFactoryDefaults)(bogus[i]); e = GetLastError(); CHECKF(!r && e == 0xC026258C, "restore bogus %d: %d / 0x%lx", i, r, e);
    }
    DWORD n = 7; SET_ERR(0);
    BOOL r = U(GetNumberOfPhysicalMonitorsFromHMONITOR)(NULL, &n);
    CHECK(!r); EXPECT_ERR(1461);   /* ERROR_INVALID_MONITOR_HANDLE */
}
