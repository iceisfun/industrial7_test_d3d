#include "d3dh.h"
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <lowlevelmonitorconfigurationapi.h>

/* STATE-CHANGING: talks to a real monitor over DDC/CI. Run only with /destructive.
 * Every Set call writes back the value just read, so the monitor ends up unchanged. */

static int first_phys(PHYSICAL_MONITOR *pm)
{
    BOOL (WINAPI *num)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *get)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR);
    if (!num || !get) return 0;
    HMONITOR hm = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    DWORD n = 0;
    if (!hm || !num(hm, &n) || n != 1) return 0;
    memset(pm, 0, sizeof *pm);
    return get(hm, 1, pm) != 0;
}
static void rel(PHYSICAL_MONITOR *pm) { BOOL (WINAPI *d)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors); if (d) d(1, pm); }

typedef BOOL (WINAPI *get_fn)(HANDLE, LPDWORD, LPDWORD, LPDWORD);
typedef BOOL (WINAPI *set_fn)(HANDLE, DWORD);

static void roundtrip(get_fn g, set_fn s, const char *nm)
{
    PHYSICAL_MONITOR pm;
    if (!g || !s) { t_skip("missing export"); return; }
    if (!first_phys(&pm)) { t_skip("no single physical monitor"); return; }
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
    PHYSICAL_MONITOR pm;
    REQUIRE(a); REQUIRE(b);
    if (!first_phys(&pm)) { t_skip("no single physical monitor"); return; }
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
