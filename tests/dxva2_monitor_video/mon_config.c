#include "dxm.h"
/* Monitor configuration (high-level wrappers over the DDC/CI VCP primitives).
 * No test here ever sends a Set* request to a real monitor: Set/restore/degauss calls are only issued with
 * invalid monitor handles or invalid enumeration values. */

#define ERR_INVALID_PARAMETER 87
static const HANDLE kBad[] = { NULL, (HANDLE)(ULONG_PTR)1 };
#define NBAD 2

/* ------- argument validation that happens before the monitor handle is used: ERROR_INVALID_PARAMETER ------- */

/* MC_SIZE_TYPE / MC_POSITION_TYPE accept 0 and 1; MC_DRIVE_TYPE / MC_GAIN_TYPE accept 0..2. */
#define ENUM_TEST_GET(id, fn, ptype, bad) \
TEST(dxva2_monitor_video_cfg_##id) { \
    BOOL (WINAPI *f)(HANDLE, ptype, LPDWORD, LPDWORD, LPDWORD) = U(fn); REQUIRE(f); \
    DWORD a = 0xCCCCCCCC, b = 0xCCCCCCCC, c = 0xCCCCCCCC; \
    for (int i = 0; i < NBAD; i++) { \
        SET_ERR(0); \
        CHECK(f(kBad[i], (ptype)(bad), &a, &b, &c) == FALSE); \
        EXPECT_ERR(ERR_INVALID_PARAMETER); \
    } \
    EXPECT_EQ(a, 0xCCCCCCCC); EXPECT_EQ(b, 0xCCCCCCCC); EXPECT_EQ(c, 0xCCCCCCCC); \
}
ENUM_TEST_GET(size_get_type2, GetMonitorDisplayAreaSize, MC_SIZE_TYPE, 2)
ENUM_TEST_GET(size_get_type_neg, GetMonitorDisplayAreaSize, MC_SIZE_TYPE, -1)
ENUM_TEST_GET(size_get_type_big, GetMonitorDisplayAreaSize, MC_SIZE_TYPE, 0x10000)
ENUM_TEST_GET(pos_get_type2, GetMonitorDisplayAreaPosition, MC_POSITION_TYPE, 2)
ENUM_TEST_GET(pos_get_type_neg, GetMonitorDisplayAreaPosition, MC_POSITION_TYPE, -1)
ENUM_TEST_GET(drive_get_type3, GetMonitorRedGreenOrBlueDrive, MC_DRIVE_TYPE, 3)
ENUM_TEST_GET(drive_get_type_neg, GetMonitorRedGreenOrBlueDrive, MC_DRIVE_TYPE, -1)
ENUM_TEST_GET(gain_get_type3, GetMonitorRedGreenOrBlueGain, MC_GAIN_TYPE, 3)
ENUM_TEST_GET(gain_get_type_neg, GetMonitorRedGreenOrBlueGain, MC_GAIN_TYPE, -1)
ENUM_TEST_GET(gain_get_type_big, GetMonitorRedGreenOrBlueGain, MC_GAIN_TYPE, 0x7FFFFFFF)

#define ENUM_TEST_SET(id, fn, ptype, bad) \
TEST(dxva2_monitor_video_cfg_##id) { \
    BOOL (WINAPI *f)(HANDLE, ptype, DWORD) = U(fn); REQUIRE(f); \
    for (int i = 0; i < NBAD; i++) { \
        SET_ERR(0); \
        CHECK(f(kBad[i], (ptype)(bad), 1) == FALSE); \
        EXPECT_ERR(ERR_INVALID_PARAMETER); \
    } \
}
ENUM_TEST_SET(size_set_type2, SetMonitorDisplayAreaSize, MC_SIZE_TYPE, 2)
ENUM_TEST_SET(size_set_type_neg, SetMonitorDisplayAreaSize, MC_SIZE_TYPE, -1)
ENUM_TEST_SET(pos_set_type2, SetMonitorDisplayAreaPosition, MC_POSITION_TYPE, 2)
ENUM_TEST_SET(pos_set_type_neg, SetMonitorDisplayAreaPosition, MC_POSITION_TYPE, -1)
ENUM_TEST_SET(drive_set_type3, SetMonitorRedGreenOrBlueDrive, MC_DRIVE_TYPE, 3)
ENUM_TEST_SET(drive_set_type_neg, SetMonitorRedGreenOrBlueDrive, MC_DRIVE_TYPE, -1)
ENUM_TEST_SET(gain_set_type3, SetMonitorRedGreenOrBlueGain, MC_GAIN_TYPE, 3)
ENUM_TEST_SET(gain_set_type_neg, SetMonitorRedGreenOrBlueGain, MC_GAIN_TYPE, -1)

TEST(dxva2_monitor_video_cfg_colortemp_set_invalid_values) {
    BOOL (WINAPI *f)(HANDLE, MC_COLOR_TEMPERATURE) = U(SetMonitorColorTemperature); REQUIRE(f);
    /* MC_COLOR_TEMPERATURE_UNKNOWN (0) cannot be set; valid values are 1..8 (4000K..11500K) */
    static const int bad[] = { 0, 9, 10, 100, -1, 0x7FFFFFFF };
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++)
        for (int i = 0; i < NBAD; i++) {
            SET_ERR(0);
            CHECKF(f(kBad[i], (MC_COLOR_TEMPERATURE)bad[k]) == FALSE, "value %d", bad[k]);
            EXPECT_ERR(ERR_INVALID_PARAMETER);
        }
}
TEST(dxva2_monitor_video_cfg_colortemp_set_valid_values_reach_handle_check) {
    BOOL (WINAPI *f)(HANDLE, MC_COLOR_TEMPERATURE) = U(SetMonitorColorTemperature); REQUIRE(f);
    BOOL (WINAPI *vcp)(HANDLE, BYTE, DWORD) = U(SetVCPFeature); REQUIRE(vcp);
    /* a valid temperature is forwarded to VCP code 0x14 with value 3..10, so with a NULL handle it fails the same way */
    for (int v = 1; v <= 8; v++) {
        SET_ERR(0);
        CHECK(f(NULL, (MC_COLOR_TEMPERATURE)v) == FALSE);
        DWORD e1 = GetLastError();
        SET_ERR(0);
        CHECK(vcp(NULL, 0x14, (DWORD)(v + 2)) == FALSE);
        DWORD e2 = GetLastError();
        CHECKF(e1 == e2, "temperature %d: GLE %lu vs raw VCP %lu", v, e1, e2);
    }
}

/* ------- NULL / invalid monitor handles: the call fails, outputs are untouched ------- */
TEST(dxva2_monitor_video_cfg_brightness_get_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPDWORD, LPDWORD, LPDWORD) = U(GetMonitorBrightness); REQUIRE(f);
    for (int i = 0; i < NBAD; i++) {
        DWORD a = 0xDEADBEEF, b = 0xDEADBEEF, c = 0xDEADBEEF;
        SET_ERR(0);
        CHECK(f(kBad[i], &a, &b, &c) == FALSE);
        CHECK(GetLastError() != 0);
        EXPECT_EQ(a, 0xDEADBEEF); EXPECT_EQ(b, 0xDEADBEEF); EXPECT_EQ(c, 0xDEADBEEF);
    }
}
TEST(dxva2_monitor_video_cfg_contrast_get_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPDWORD, LPDWORD, LPDWORD) = U(GetMonitorContrast); REQUIRE(f);
    for (int i = 0; i < NBAD; i++) {
        DWORD a = 0xDEADBEEF, b = 0xDEADBEEF, c = 0xDEADBEEF;
        SET_ERR(0);
        CHECK(f(kBad[i], &a, &b, &c) == FALSE);
        CHECK(GetLastError() != 0);
        EXPECT_EQ(a, 0xDEADBEEF); EXPECT_EQ(b, 0xDEADBEEF); EXPECT_EQ(c, 0xDEADBEEF);
    }
}
TEST(dxva2_monitor_video_cfg_valid_enum_get_bad_handle) {
    BOOL (WINAPI *sz)(HANDLE, MC_SIZE_TYPE, LPDWORD, LPDWORD, LPDWORD) = U(GetMonitorDisplayAreaSize);
    BOOL (WINAPI *ps)(HANDLE, MC_POSITION_TYPE, LPDWORD, LPDWORD, LPDWORD) = U(GetMonitorDisplayAreaPosition);
    BOOL (WINAPI *dr)(HANDLE, MC_DRIVE_TYPE, LPDWORD, LPDWORD, LPDWORD) = U(GetMonitorRedGreenOrBlueDrive);
    BOOL (WINAPI *gn)(HANDLE, MC_GAIN_TYPE, LPDWORD, LPDWORD, LPDWORD) = U(GetMonitorRedGreenOrBlueGain);
    REQUIRE(sz); REQUIRE(ps); REQUIRE(dr); REQUIRE(gn);
    DWORD a, b, c;
    for (int t = 0; t < 3; t++) {
        if (t < 2) {
            a = b = c = 0x5A5A5A5A; SET_ERR(0);
            CHECK(sz(NULL, (MC_SIZE_TYPE)t, &a, &b, &c) == FALSE); CHECK(GetLastError() != ERR_INVALID_PARAMETER);
            EXPECT_EQ(a, 0x5A5A5A5A);
            a = b = c = 0x5A5A5A5A; SET_ERR(0);
            CHECK(ps(NULL, (MC_POSITION_TYPE)t, &a, &b, &c) == FALSE); CHECK(GetLastError() != ERR_INVALID_PARAMETER);
            EXPECT_EQ(a, 0x5A5A5A5A);
        }
        a = b = c = 0x5A5A5A5A; SET_ERR(0);
        CHECK(dr(NULL, (MC_DRIVE_TYPE)t, &a, &b, &c) == FALSE); CHECK(GetLastError() != ERR_INVALID_PARAMETER);
        EXPECT_EQ(a, 0x5A5A5A5A);
        a = b = c = 0x5A5A5A5A; SET_ERR(0);
        CHECK(gn(NULL, (MC_GAIN_TYPE)t, &a, &b, &c) == FALSE); CHECK(GetLastError() != ERR_INVALID_PARAMETER);
        EXPECT_EQ(a, 0x5A5A5A5A);
    }
}
TEST(dxva2_monitor_video_cfg_techtype_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPMC_DISPLAY_TECHNOLOGY_TYPE) = U(GetMonitorTechnologyType); REQUIRE(f);
    for (int i = 0; i < NBAD; i++) {
        MC_DISPLAY_TECHNOLOGY_TYPE t = (MC_DISPLAY_TECHNOLOGY_TYPE)0x77;
        SET_ERR(0);
        CHECK(f(kBad[i], &t) == FALSE);
        CHECK(GetLastError() != 0);
        EXPECT_EQ((int)t, 0x77);
    }
}
TEST(dxva2_monitor_video_cfg_colortemp_get_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPMC_COLOR_TEMPERATURE) = U(GetMonitorColorTemperature); REQUIRE(f);
    for (int i = 0; i < NBAD; i++) {
        MC_COLOR_TEMPERATURE t = (MC_COLOR_TEMPERATURE)0x77;
        SET_ERR(0);
        CHECK(f(kBad[i], &t) == FALSE);
        CHECK(GetLastError() != 0);
        EXPECT_EQ((int)t, 0x77);
    }
}
TEST(dxva2_monitor_video_cfg_capabilities_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPDWORD, LPDWORD) = U(GetMonitorCapabilities); REQUIRE(f);
    for (int i = 0; i < NBAD; i++) {
        DWORD caps = 0xABABABAB, temps = 0xABABABAB;
        SET_ERR(0);
        CHECK(f(kBad[i], &caps, &temps) == FALSE);
        CHECK(GetLastError() != 0);
        EXPECT_EQ(caps, 0xABABABAB); EXPECT_EQ(temps, 0xABABABAB);
    }
}
TEST(dxva2_monitor_video_cfg_capabilities_bad_handle_partial_pointers) {
    BOOL (WINAPI *f)(HANDLE, LPDWORD, LPDWORD) = U(GetMonitorCapabilities); REQUIRE(f);
    DWORD v = 5;
    SET_ERR(0); CHECK(f(NULL, &v, NULL) == FALSE); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(f(NULL, NULL, &v) == FALSE); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(f(NULL, NULL, NULL) == FALSE); CHECK(GetLastError() != 0);
    EXPECT_EQ(v, 5);
}
TEST(dxva2_monitor_video_cfg_capstrlen_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPDWORD) = U(GetCapabilitiesStringLength); REQUIRE(f);
    for (int i = 0; i < NBAD; i++) {
        DWORD n = 0x1111;
        SET_ERR(0);
        CHECK(f(kBad[i], &n) == FALSE);
        CHECK(GetLastError() != 0);
    }
}
TEST(dxva2_monitor_video_cfg_capreply_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPSTR, DWORD) = U(CapabilitiesRequestAndCapabilitiesReply); REQUIRE(f);
    char buf[64]; memset(buf, 'Q', sizeof buf);
    for (int i = 0; i < NBAD; i++) {
        SET_ERR(0);
        CHECK(f(kBad[i], buf, sizeof buf) == FALSE);
        CHECK(GetLastError() != 0);
    }
}
TEST(dxva2_monitor_video_cfg_capreply_zero_length_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPSTR, DWORD) = U(CapabilitiesRequestAndCapabilitiesReply); REQUIRE(f);
    char c = 'Q';
    SET_ERR(0);
    CHECK(f(NULL, &c, 0) == FALSE);
    CHECK(GetLastError() != 0);
    EXPECT_EQ(c, 'Q');
}
TEST(dxva2_monitor_video_cfg_timing_report_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, LPMC_TIMING_REPORT) = U(GetTimingReport); REQUIRE(f);
    for (int i = 0; i < NBAD; i++) {
        MC_TIMING_REPORT r; memset(&r, 0xAA, sizeof r);
        SET_ERR(0);
        CHECK(f(kBad[i], &r) == FALSE);
        CHECK(GetLastError() != 0);
    }
}
TEST(dxva2_monitor_video_cfg_vcp_get_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, BYTE, LPMC_VCP_CODE_TYPE, LPDWORD, LPDWORD) = U(GetVCPFeatureAndVCPFeatureReply); REQUIRE(f);
    static const BYTE codes[] = { 0x00, 0x10, 0x12, 0x14, 0xB6, 0xFF };
    for (size_t k = 0; k < sizeof codes; k++) {
        MC_VCP_CODE_TYPE t = (MC_VCP_CODE_TYPE)9; DWORD cur = 0x42, max = 0x42;
        SET_ERR(0);
        CHECKF(f(NULL, codes[k], &t, &cur, &max) == FALSE, "code 0x%02x", codes[k]);
        CHECK(GetLastError() != 0);
    }
}
TEST(dxva2_monitor_video_cfg_vcp_get_optional_outputs_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, BYTE, LPMC_VCP_CODE_TYPE, LPDWORD, LPDWORD) = U(GetVCPFeatureAndVCPFeatureReply); REQUIRE(f);
    DWORD cur;
    SET_ERR(0); CHECK(f(NULL, 0x10, NULL, &cur, NULL) == FALSE); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(f(NULL, 0x10, NULL, NULL, NULL) == FALSE); CHECK(GetLastError() != 0);
}
TEST(dxva2_monitor_video_cfg_vcp_set_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, BYTE, DWORD) = U(SetVCPFeature); REQUIRE(f);
    for (int i = 0; i < NBAD; i++) {
        SET_ERR(0);
        CHECK(f(kBad[i], 0x10, 0) == FALSE);
        CHECK(GetLastError() != 0);
    }
}
TEST(dxva2_monitor_video_cfg_gle_deterministic) {
    BOOL (WINAPI *f)(HANDLE, BYTE, DWORD) = U(SetVCPFeature); REQUIRE(f);
    SET_ERR(0); f(NULL, 0x10, 0); DWORD a = GetLastError();
    SET_ERR(0); f(NULL, 0x10, 0); DWORD b = GetLastError();
    EXPECT_EQ(a, b);
    t_note("SetVCPFeature(NULL) last error = 0x%lx (%lu)", a, a);
}
TEST(dxva2_monitor_video_cfg_gle_overwritten_not_preserved) {
    BOOL (WINAPI *f)(HANDLE, BYTE, DWORD) = U(SetVCPFeature); REQUIRE(f);
    SET_ERR(0xCAFE);
    CHECK(f(NULL, 0x10, 0) == FALSE);
    CHECK(GetLastError() != 0xCAFE);
}

/* ------- wrappers are thin: they behave exactly like the VCP primitive they map to ------- */
static DWORD gle_of_vcp(BYTE code, DWORD value, HANDLE h)
{
    BOOL (WINAPI *f)(HANDLE, BYTE, DWORD) = U(SetVCPFeature);
    SET_ERR(0); BOOL r = f(h, code, value); DWORD e = GetLastError();
    return r ? 0 : e;
}
TEST(dxva2_monitor_video_cfg_degauss_equals_vcp01) {
    BOOL (WINAPI *f)(HANDLE) = U(DegaussMonitor); REQUIRE(f); REQUIRE(U(SetVCPFeature));
    for (int i = 0; i < NBAD; i++) {
        SET_ERR(0); CHECK(f(kBad[i]) == FALSE); DWORD e = GetLastError();
        EXPECT_EQ(e, gle_of_vcp(0x01, 1, kBad[i]));
    }
}
TEST(dxva2_monitor_video_cfg_restore_defaults_equals_vcp04) {
    BOOL (WINAPI *f)(HANDLE) = U(RestoreMonitorFactoryDefaults); REQUIRE(f); REQUIRE(U(SetVCPFeature));
    for (int i = 0; i < NBAD; i++) {
        SET_ERR(0); CHECK(f(kBad[i]) == FALSE); DWORD e = GetLastError();
        EXPECT_EQ(e, gle_of_vcp(0x04, 1, kBad[i]));
    }
}
TEST(dxva2_monitor_video_cfg_restore_color_defaults_equals_vcp08) {
    BOOL (WINAPI *f)(HANDLE) = U(RestoreMonitorFactoryColorDefaults); REQUIRE(f); REQUIRE(U(SetVCPFeature));
    for (int i = 0; i < NBAD; i++) {
        SET_ERR(0); CHECK(f(kBad[i]) == FALSE); DWORD e = GetLastError();
        EXPECT_EQ(e, gle_of_vcp(0x08, 1, kBad[i]));
    }
}
TEST(dxva2_monitor_video_cfg_brightness_set_equals_vcp10) {
    BOOL (WINAPI *f)(HANDLE, DWORD) = U(SetMonitorBrightness); REQUIRE(f); REQUIRE(U(SetVCPFeature));
    SET_ERR(0); CHECK(f(NULL, 50) == FALSE); DWORD e = GetLastError();
    EXPECT_EQ(e, gle_of_vcp(0x10, 50, NULL));
}
TEST(dxva2_monitor_video_cfg_contrast_set_equals_vcp12) {
    BOOL (WINAPI *f)(HANDLE, DWORD) = U(SetMonitorContrast); REQUIRE(f); REQUIRE(U(SetVCPFeature));
    SET_ERR(0); CHECK(f(NULL, 50) == FALSE); DWORD e = GetLastError();
    EXPECT_EQ(e, gle_of_vcp(0x12, 50, NULL));
}
TEST(dxva2_monitor_video_cfg_save_settings_aliases) {
    BOOL (WINAPI *a)(HANDLE) = U(SaveCurrentMonitorSettings);
    BOOL (WINAPI *b)(HANDLE) = U(SaveCurrentSettings);
    REQUIRE(a); REQUIRE(b);
    for (int i = 0; i < NBAD; i++) {
        SET_ERR(0); BOOL ra = a(kBad[i]); DWORD ea = GetLastError();
        SET_ERR(0); BOOL rb = b(kBad[i]); DWORD eb = GetLastError();
        CHECK(ra == FALSE); CHECK(rb == FALSE);
        EXPECT_EQ(ea, eb);
        CHECK(ea != 0);
    }
}
TEST(dxva2_monitor_video_cfg_setters_valid_enum_bad_handle) {
    BOOL (WINAPI *sz)(HANDLE, MC_SIZE_TYPE, DWORD) = U(SetMonitorDisplayAreaSize);
    BOOL (WINAPI *ps)(HANDLE, MC_POSITION_TYPE, DWORD) = U(SetMonitorDisplayAreaPosition);
    BOOL (WINAPI *dr)(HANDLE, MC_DRIVE_TYPE, DWORD) = U(SetMonitorRedGreenOrBlueDrive);
    BOOL (WINAPI *gn)(HANDLE, MC_GAIN_TYPE, DWORD) = U(SetMonitorRedGreenOrBlueGain);
    REQUIRE(sz); REQUIRE(ps); REQUIRE(dr); REQUIRE(gn);
    for (int t = 0; t < 3; t++) {
        if (t < 2) {
            SET_ERR(0); CHECK(sz(NULL, (MC_SIZE_TYPE)t, 10) == FALSE); CHECK(GetLastError() != ERR_INVALID_PARAMETER);
            SET_ERR(0); CHECK(ps(NULL, (MC_POSITION_TYPE)t, 10) == FALSE); CHECK(GetLastError() != ERR_INVALID_PARAMETER);
        }
        SET_ERR(0); CHECK(dr(NULL, (MC_DRIVE_TYPE)t, 10) == FALSE); CHECK(GetLastError() != ERR_INVALID_PARAMETER);
        SET_ERR(0); CHECK(gn(NULL, (MC_GAIN_TYPE)t, 10) == FALSE); CHECK(GetLastError() != ERR_INVALID_PARAMETER);
    }
}

/* ------- read-only queries on a real monitor (environment dependent: notes plus internal consistency) ------- */
static int first_phys(HMONITOR *hm_out, PHYSICAL_MONITOR *pm)
{
    BOOL (WINAPI *num)(HMONITOR, LPDWORD) = U(GetNumberOfPhysicalMonitorsFromHMONITOR);
    BOOL (WINAPI *get)(HMONITOR, DWORD, LPPHYSICAL_MONITOR) = U(GetPhysicalMonitorsFromHMONITOR);
    if (!num || !get) return 0;
    HMONITOR hm = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    DWORD n = 0;
    if (!hm || !num(hm, &n) || n != 1) return 0;
    memset(pm, 0, sizeof *pm);
    if (!get(hm, 1, pm)) return 0;
    *hm_out = hm;
    return 1;
}
static void release_phys(PHYSICAL_MONITOR *pm) { BOOL (WINAPI *d)(DWORD, LPPHYSICAL_MONITOR) = U(DestroyPhysicalMonitors); if (d) d(1, pm); }

TEST(dxva2_monitor_video_cfg_live_capabilities) {
    BOOL (WINAPI *f)(HANDLE, LPDWORD, LPDWORD) = U(GetMonitorCapabilities); REQUIRE(f);
    HMONITOR hm; PHYSICAL_MONITOR pm;
    if (!first_phys(&hm, &pm)) { t_skip("no single physical monitor on the primary display"); return; }
    DWORD caps = 0, temps = 0;
    SET_ERR(0);
    BOOL r = f(pm.hPhysicalMonitor, &caps, &temps);
    if (r) {
        t_note("monitor caps 0x%lx temps 0x%lx", caps, temps);
        if (!(caps & MC_CAPS_COLOR_TEMPERATURE)) EXPECT_EQ(temps, 0);
    } else t_note("GetMonitorCapabilities failed GLE=%lu (monitor without DDC/CI)", GetLastError());
    release_phys(&pm);
}
TEST(dxva2_monitor_video_cfg_live_brightness_contrast_ranges) {
    BOOL (WINAPI *b)(HANDLE, LPDWORD, LPDWORD, LPDWORD) = U(GetMonitorBrightness);
    BOOL (WINAPI *c)(HANDLE, LPDWORD, LPDWORD, LPDWORD) = U(GetMonitorContrast);
    REQUIRE(b); REQUIRE(c);
    HMONITOR hm; PHYSICAL_MONITOR pm;
    if (!first_phys(&hm, &pm)) { t_skip("no single physical monitor on the primary display"); return; }
    DWORD mn, cu, mx;
    if (b(pm.hPhysicalMonitor, &mn, &cu, &mx)) {
        EXPECT_EQ(mn, 0);                  /* the minimum is always normalised to 0 */
        CHECK(cu <= mx);
        t_note("brightness %lu/%lu", cu, mx);
    } else t_note("GetMonitorBrightness failed GLE=%lu", GetLastError());
    if (c(pm.hPhysicalMonitor, &mn, &cu, &mx)) { EXPECT_EQ(mn, 0); CHECK(cu <= mx); t_note("contrast %lu/%lu", cu, mx); }
    else t_note("GetMonitorContrast failed GLE=%lu", GetLastError());
    release_phys(&pm);
}
TEST(dxva2_monitor_video_cfg_live_technology_type) {
    BOOL (WINAPI *f)(HANDLE, LPMC_DISPLAY_TECHNOLOGY_TYPE) = U(GetMonitorTechnologyType); REQUIRE(f);
    HMONITOR hm; PHYSICAL_MONITOR pm;
    if (!first_phys(&hm, &pm)) { t_skip("no single physical monitor on the primary display"); return; }
    MC_DISPLAY_TECHNOLOGY_TYPE t = (MC_DISPLAY_TECHNOLOGY_TYPE)-1;
    if (f(pm.hPhysicalMonitor, &t)) { CHECK((int)t >= 0 && (int)t <= 8); t_note("technology type %d", (int)t); }
    else t_note("GetMonitorTechnologyType failed GLE=0x%lx", GetLastError());
    release_phys(&pm);
}
TEST(dxva2_monitor_video_cfg_live_color_temperature) {
    BOOL (WINAPI *f)(HANDLE, LPMC_COLOR_TEMPERATURE) = U(GetMonitorColorTemperature); REQUIRE(f);
    HMONITOR hm; PHYSICAL_MONITOR pm;
    if (!first_phys(&hm, &pm)) { t_skip("no single physical monitor on the primary display"); return; }
    MC_COLOR_TEMPERATURE t = (MC_COLOR_TEMPERATURE)-1;
    if (f(pm.hPhysicalMonitor, &t)) { CHECK((int)t >= 0 && (int)t <= 8); t_note("color temperature %d", (int)t); }
    else t_note("GetMonitorColorTemperature failed GLE=%lu", GetLastError());
    release_phys(&pm);
}
TEST(dxva2_monitor_video_cfg_live_capstring_length_consistency) {
    BOOL (WINAPI *len)(HANDLE, LPDWORD) = U(GetCapabilitiesStringLength);
    BOOL (WINAPI *get)(HANDLE, LPSTR, DWORD) = U(CapabilitiesRequestAndCapabilitiesReply);
    REQUIRE(len); REQUIRE(get);
    HMONITOR hm; PHYSICAL_MONITOR pm;
    if (!first_phys(&hm, &pm)) { t_skip("no single physical monitor on the primary display"); return; }
    DWORD n = 0;
    if (!len(pm.hPhysicalMonitor, &n) || n == 0 || n > 8192) { t_note("no capabilities string (GLE=%lu, n=%lu)", GetLastError(), n); release_phys(&pm); return; }
    char *buf = (char *)LocalAlloc(LPTR, n + 16);
    if (buf) {
        memset(buf, 'Z', n + 16);
        BOOL r = get(pm.hPhysicalMonitor, buf, n);
        if (r) { CHECK(strlen(buf) <= n); t_note("capabilities: %.60s...", buf); }
        else t_note("capabilities reply failed GLE=%lu", GetLastError());
        LocalFree(buf);
    }
    release_phys(&pm);
}
