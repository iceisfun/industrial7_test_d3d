#include "dxm.h"
/* Physical monitor enumeration: GetNumberOf/GetPhysicalMonitors (HMONITOR and IDirect3DDevice9), Destroy*.
 * Hard checks are limited to argument-validation paths that do not depend on the attached monitors. */

static HMONITOR primary(void) { return MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY); }

#define ERR_INVALID_MONITOR_HANDLE 1461

TEST(dxva2_monitor_video_mon_num_null_hmonitor) {
    BOOL (WINAPI *f)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR); REQUIRE(f);
    DWORD n = 0xABCDABCD;
    SET_ERR(0);
    BOOL r = f(NULL, &n);
    CHECK(r == FALSE);
    EXPECT_ERR(ERR_INVALID_MONITOR_HANDLE);   /* GetMonitorInfoW(NULL) failure code is passed through HRESULT_FROM_WIN32 and back */
    EXPECT_EQ(n, 0xABCDABCD);                 /* output untouched on failure */
}
TEST(dxva2_monitor_video_mon_num_bogus_hmonitor) {
    BOOL (WINAPI *f)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR); REQUIRE(f);
    DWORD n = 77;
    SET_ERR(0);
    CHECK(f((HMONITOR)(ULONG_PTR)0x1, &n) == FALSE);
    EXPECT_ERR(ERR_INVALID_MONITOR_HANDLE);
    EXPECT_EQ(n, 77);
}
TEST(dxva2_monitor_video_mon_num_primary) {
    BOOL (WINAPI *f)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR); REQUIRE(f);
    HMONITOR hm = primary();
    if (!hm) { t_skip("no primary HMONITOR"); return; }
    DWORD n = 0xFFFF;
    SET_ERR(0);
    BOOL r = f(hm, &n);
    if (r) {
        CHECKF(n < 64, "implausible monitor count %lu", n);
        t_note("primary HMONITOR has %lu physical monitor(s)", n);
    } else {
        t_note("GetNumberOfPhysicalMonitorsFromHMONITOR(primary) failed, GLE=%lu (no monitor driver on a virtual display is normal)", GetLastError());
        EXPECT_EQ(n, 0xFFFF);
    }
}
TEST(dxva2_monitor_video_mon_num_stable) {
    BOOL (WINAPI *f)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR); REQUIRE(f);
    HMONITOR hm = primary(); if (!hm) { t_skip("no primary HMONITOR"); return; }
    DWORD a = 0, b = 0;
    BOOL ra = f(hm, &a), rb = f(hm, &b);
    CHECK(ra == rb);
    if (ra && rb) EXPECT_EQ(a, b);
}
struct enumctx { BOOL (WINAPI *f)(HMONITOR, LPDWORD); int n, ok; };
static BOOL CALLBACK enum_cb(HMONITOR hm, HDC dc, LPRECT rc, LPARAM lp)
{
    struct enumctx *x = (struct enumctx *)lp; DWORD n = 0;
    x->n++;
    if (x->f(hm, &n)) x->ok++;
    return TRUE;
}
TEST(dxva2_monitor_video_mon_num_each_monitor) {
    BOOL (WINAPI *f)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR); REQUIRE(f);
    struct enumctx c = { f, 0, 0 };
    EnumDisplayMonitors(NULL, NULL, enum_cb, (LPARAM)&c);
    t_note("%d HMONITOR(s), %d answered GetNumberOfPhysicalMonitorsFromHMONITOR", c.n, c.ok);
}
/* A monitor without DDC/CI (the oracle's virtual "Generic PnP Monitor") comes back as a physical-monitor entry with a NULL handle: the
 * enumeration succeeds, nothing can be configured through it (low-level calls fail with ERROR_NOT_SUPPORTED) and DestroyPhysicalMonitors
 * accepts it. A real DDC/CI monitor has a non-NULL handle. */
static void phys_handle_rules(HANDLE h, const WCHAR *desc)
{
    if (h) return;
    {
        BOOL (WINAPI *caps)(HANDLE, LPDWORD, LPDWORD) = U(GetMonitorCapabilities);
        DWORD c = 0xdead, col = 0xdead;
        EXPECT_WSTR(desc, L"Generic PnP Monitor");
        if (caps) {
            SET_ERR(0);
            CHECK(caps(NULL, &c, &col) == FALSE);
            EXPECT_ERR(ERROR_NOT_SUPPORTED);
        }
    }
}
TEST(dxva2_monitor_video_mon_get_null_hmonitor) {
    BOOL (WINAPI *f)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR); REQUIRE(f);
    PHYSICAL_MONITOR pm[2]; memset(pm, 0xCC, sizeof pm);
    SET_ERR(0);
    CHECK(f(NULL, 1, pm) == FALSE);
    EXPECT_ERR(ERR_INVALID_MONITOR_HANDLE);
}
TEST(dxva2_monitor_video_mon_get_null_hmonitor_zero_count) {
    BOOL (WINAPI *f)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR); REQUIRE(f);
    PHYSICAL_MONITOR pm;
    SET_ERR(0);
    CHECK(f(NULL, 0, &pm) == FALSE);   /* the HMONITOR is resolved before the count is looked at */
    EXPECT_ERR(ERR_INVALID_MONITOR_HANDLE);
}
TEST(dxva2_monitor_video_mon_get_bogus_hmonitor) {
    BOOL (WINAPI *f)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR); REQUIRE(f);
    PHYSICAL_MONITOR pm;
    SET_ERR(0);
    CHECK(f((HMONITOR)(ULONG_PTR)0x3, 1, &pm) == FALSE);
    EXPECT_ERR(ERR_INVALID_MONITOR_HANDLE);
}
TEST(dxva2_monitor_video_mon_get_roundtrip) {
    BOOL (WINAPI *num)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *get)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *destroy)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors);
    REQUIRE(num); REQUIRE(get); REQUIRE(destroy);
    HMONITOR hm = primary(); if (!hm) { t_skip("no primary HMONITOR"); return; }
    DWORD n = 0;
    if (!num(hm, &n) || n == 0 || n > 16) { t_skip("no physical monitor on primary HMONITOR (n=%lu, GLE=%lu)", n, GetLastError()); return; }
    PHYSICAL_MONITOR pm[16]; memset(pm, 0, sizeof pm);
    BOOL r = get(hm, n, pm);
    CHECKF(r, "GetPhysicalMonitorsFromHMONITOR failed GLE=%lu", GetLastError());
    if (!r) return;
    for (DWORD i = 0; i < n; i++) {
        phys_handle_rules(pm[i].hPhysicalMonitor, pm[i].szPhysicalMonitorDescription);
        size_t len = wcsnlen(pm[i].szPhysicalMonitorDescription, PHYSICAL_MONITOR_DESCRIPTION_SIZE);
        CHECK(len < PHYSICAL_MONITOR_DESCRIPTION_SIZE);   /* NUL terminated within 128 WCHARs */
        t_note("physical monitor %lu: handle %p, \"%ls\"", i, pm[i].hPhysicalMonitor, pm[i].szPhysicalMonitorDescription);
    }
    SET_ERR(0);
    CHECK(destroy(n, pm) == TRUE);
    for (DWORD i = 0; i < n; i++) CHECK(pm[i].szPhysicalMonitorDescription[0] == 0);   /* description is cleared */
}
TEST(dxva2_monitor_video_mon_get_wrong_count) {
    BOOL (WINAPI *num)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *get)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR);
    REQUIRE(num); REQUIRE(get);
    HMONITOR hm = primary(); if (!hm) { t_skip("no primary HMONITOR"); return; }
    DWORD n = 0;
    if (!num(hm, &n) || n == 0 || n > 15) { t_skip("no physical monitor available"); return; }
    PHYSICAL_MONITOR pm[17]; memset(pm, 0, sizeof pm);
    /* the array size must equal the monitor count exactly: a larger request fails (the surplus is not silently accepted) */
    SET_ERR(0);
    BOOL r = get(hm, n + 1, pm);
    CHECK(r == FALSE);
    if (!r) t_note("GetPhysicalMonitorsFromHMONITOR(count+1) GLE=%lu", GetLastError());
    else { BOOL (WINAPI *d)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors); if (d) d(n + 1, pm); }
}
TEST(dxva2_monitor_video_mon_destroy_zero_array) {
    BOOL (WINAPI *f)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors); REQUIRE(f);
    SET_ERR(0x1234);
    CHECK(f(0, NULL) == TRUE);          /* an empty array is a successful no-op, the array pointer is not touched */
    EXPECT_EQ(GetLastError(), 0x1234);  /* and last-error is left alone */
}
TEST(dxva2_monitor_video_mon_destroy_invalid_handle_clears_description) {
    BOOL (WINAPI *f)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors); REQUIRE(f);
    PHYSICAL_MONITOR pm; memset(&pm, 0, sizeof pm);
    pm.hPhysicalMonitor = NULL;
    wcscpy(pm.szPhysicalMonitorDescription, L"stale description");
    SET_ERR(0);
    BOOL r = f(1, &pm);
    CHECK(r == FALSE);                                   /* destroying a NULL physical monitor handle fails ... */
    CHECK(GetLastError() != 0);
    CHECK(pm.szPhysicalMonitorDescription[0] == 0);      /* ... but the description is cleared regardless */
}
TEST(dxva2_monitor_video_mon_destroy_array_reports_first_failure) {
    BOOL (WINAPI *f)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors); REQUIRE(f);
    PHYSICAL_MONITOR pm[3]; memset(pm, 0, sizeof pm);
    for (int i = 0; i < 3; i++) wcscpy(pm[i].szPhysicalMonitorDescription, L"x");
    SET_ERR(0);
    CHECK(f(3, pm) == FALSE);
    CHECK(GetLastError() != 0);
    for (int i = 0; i < 3; i++) CHECK(pm[i].szPhysicalMonitorDescription[0] == 0);   /* every entry is processed */
}
TEST(dxva2_monitor_video_mon_destroy_single_null) {
    BOOL (WINAPI *f)(HANDLE) = U(DestroyPhysicalMonitor); REQUIRE(f);
    SET_ERR(0);
    CHECK(f(NULL) == FALSE);
    CHECK(GetLastError() != 0);
}
TEST(dxva2_monitor_video_mon_destroy_single_invalid_values) {
    BOOL (WINAPI *f)(HANDLE) = U(DestroyPhysicalMonitor); REQUIRE(f);
    HANDLE vals[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)1 };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        BOOL r = f(vals[i]);
        CHECKF(r == FALSE, "value %d unexpectedly destroyed", i);
        CHECK(GetLastError() != 0);
    }
}
TEST(dxva2_monitor_video_mon_destroy_twice) {
    BOOL (WINAPI *num)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *get)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *d1)(HANDLE) = U(DestroyPhysicalMonitor);
    REQUIRE(num); REQUIRE(get); REQUIRE(d1);
    HMONITOR hm = primary(); if (!hm) { t_skip("no primary HMONITOR"); return; }
    DWORD n = 0;
    if (!num(hm, &n) || n != 1) { t_skip("needs exactly one physical monitor (n=%lu)", n); return; }
    PHYSICAL_MONITOR pm; memset(&pm, 0, sizeof pm);
    if (!get(hm, 1, &pm)) { t_skip("GetPhysicalMonitorsFromHMONITOR failed"); return; }
    CHECK(d1(pm.hPhysicalMonitor) == TRUE);
    SET_ERR(0);
    CHECK(d1(pm.hPhysicalMonitor) == FALSE);   /* the handle is dead after the first destroy */
    CHECK(GetLastError() != 0);
}
TEST(dxva2_monitor_video_mon_handles_distinct_between_calls) {
    BOOL (WINAPI *num)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *get)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *ds)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors);
    REQUIRE(num); REQUIRE(get); REQUIRE(ds);
    HMONITOR hm = primary(); if (!hm) { t_skip("no primary HMONITOR"); return; }
    DWORD n = 0;
    if (!num(hm, &n) || n != 1) { t_skip("needs exactly one physical monitor"); return; }
    PHYSICAL_MONITOR a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    if (!get(hm, 1, &a)) { t_skip("get failed"); return; }
    if (!get(hm, 1, &b)) { ds(1, &a); t_skip("second get failed"); return; }
    CHECK(a.hPhysicalMonitor != b.hPhysicalMonitor);   /* each call hands out its own handle */
    CHECK(lstrcmpW(a.szPhysicalMonitorDescription, b.szPhysicalMonitorDescription) == 0);
    CHECK(ds(1, &a) == TRUE);
    CHECK(ds(1, &b) == TRUE);
}

/* ---- Direct3D 9 device variants (HRESULT returning) ---- */
TEST(dxva2_monitor_video_mon_d3d9_num) {
    HRESULT (WINAPI *f)(IDirect3DDevice9 *, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromIDirect3DDevice9); REQUIRE(f);
    DXM_D9(d);
    DWORD n = 0xFFFF;
    HRESULT hr = f(d.dev, &n);
    if (SUCCEEDED(hr)) { CHECK(n < 64); t_note("D3D9 device (type %d) drives %lu physical monitor(s)", d.type, n); }
    else t_note("GetNumberOfPhysicalMonitorsFromIDirect3DDevice9 -> %s", d3dh_hr_name(hr));
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_mon_d3d9_num_matches_hmonitor) {
    HRESULT (WINAPI *f)(IDirect3DDevice9 *, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromIDirect3DDevice9);
    BOOL (WINAPI *g)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR);
    REQUIRE(f); REQUIRE(g);
    DXM_D9(d);
    HMONITOR hm = IDirect3D9_GetAdapterMonitor(d.d3d, 0);
    DWORD n1 = 0, n2 = 0;
    HRESULT hr = f(d.dev, &n1);
    BOOL r = hm ? g(hm, &n2) : FALSE;
    if (SUCCEEDED(hr) && r) EXPECT_EQ(n1, n2);
    else t_note("device variant %s, HMONITOR variant %d", d3dh_hr_name(hr), r);
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_mon_d3d9_get_roundtrip) {
    HRESULT (WINAPI *num)(IDirect3DDevice9 *, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromIDirect3DDevice9);
    HRESULT (WINAPI *get)(IDirect3DDevice9 *, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromIDirect3DDevice9);
    BOOL (WINAPI *ds)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors);
    REQUIRE(num); REQUIRE(get); REQUIRE(ds);
    DXM_D9(d);
    DWORD n = 0;
    HRESULT hr = num(d.dev, &n);
    if (FAILED(hr) || n == 0 || n > 16) { t_skip("no physical monitor for the device (%s, n=%lu)", d3dh_hr_name(hr), n); DXM_D9_END(d); return; }
    PHYSICAL_MONITOR pm[16]; memset(pm, 0, sizeof pm);
    hr = get(d.dev, n, pm);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        for (DWORD i = 0; i < n; i++) { phys_handle_rules(pm[i].hPhysicalMonitor, pm[i].szPhysicalMonitorDescription); CHECK(wcsnlen(pm[i].szPhysicalMonitorDescription, 128) < 128); }
        CHECK(ds(n, pm) == TRUE);
    }
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_mon_d3d9_get_wrong_count) {
    HRESULT (WINAPI *num)(IDirect3DDevice9 *, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromIDirect3DDevice9);
    HRESULT (WINAPI *get)(IDirect3DDevice9 *, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromIDirect3DDevice9);
    REQUIRE(num); REQUIRE(get);
    DXM_D9(d);
    DWORD n = 0;
    if (FAILED(num(d.dev, &n)) || n == 0 || n > 15) { t_skip("no physical monitor for the device"); DXM_D9_END(d); return; }
    PHYSICAL_MONITOR pm[17]; memset(pm, 0, sizeof pm);
    HRESULT hr = get(d.dev, n + 1, pm);
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr)) { BOOL (WINAPI *ds)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors); if (ds) ds(n + 1, pm); }
    else EXPECT_HR(hr, E_FAIL);
    DXM_D9_END(d);
}
