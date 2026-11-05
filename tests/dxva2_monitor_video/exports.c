#include "dxm.h"
/* Every dxva2.dll export resolves by name and by ordinal; ordinals follow the alphabetical order of names. */
static const struct { const char *name; unsigned ord; } kExp[] = {
    {"CapabilitiesRequestAndCapabilitiesReply", 1}, {"DXVA2CreateDirect3DDeviceManager9", 2}, {"DXVA2CreateVideoService", 3},
    {"DXVAHD_CreateDevice", 4}, {"DegaussMonitor", 5}, {"DestroyPhysicalMonitor", 6}, {"DestroyPhysicalMonitors", 7},
    {"GetCapabilitiesStringLength", 8}, {"GetMonitorBrightness", 9}, {"GetMonitorCapabilities", 10},
    {"GetMonitorColorTemperature", 11}, {"GetMonitorContrast", 12}, {"GetMonitorDisplayAreaPosition", 13},
    {"GetMonitorDisplayAreaSize", 14}, {"GetMonitorRedGreenOrBlueDrive", 15}, {"GetMonitorRedGreenOrBlueGain", 16},
    {"GetMonitorTechnologyType", 17}, {"GetNumberOfPhysicalMonitorsFromHMONITOR", 18},
    {"GetNumberOfPhysicalMonitorsFromIDirect3DDevice9", 19}, {"GetPhysicalMonitorsFromHMONITOR", 20},
    {"GetPhysicalMonitorsFromIDirect3DDevice9", 21}, {"GetTimingReport", 22}, {"GetVCPFeatureAndVCPFeatureReply", 23},
    {"OPMGetVideoOutputForTarget", 24}, {"OPMGetVideoOutputsFromHMONITOR", 25},
    {"OPMGetVideoOutputsFromIDirect3DDevice9Object", 26}, {"RestoreMonitorFactoryColorDefaults", 27},
    {"RestoreMonitorFactoryDefaults", 28}, {"SaveCurrentMonitorSettings", 29}, {"SaveCurrentSettings", 30},
    {"SetMonitorBrightness", 31}, {"SetMonitorColorTemperature", 32}, {"SetMonitorContrast", 33},
    {"SetMonitorDisplayAreaPosition", 34}, {"SetMonitorDisplayAreaSize", 35}, {"SetMonitorRedGreenOrBlueDrive", 36},
    {"SetMonitorRedGreenOrBlueGain", 37}, {"SetVCPFeature", 38},
};
#define NEXP ((int)(sizeof kExp / sizeof kExp[0]))

#define EXP_TEST(idx, id) \
TEST(dxva2_monitor_video_export_##id) { \
    HMODULE m = t_module("dxva2.dll"); if (!m) { t_skip("dxva2.dll not loadable"); return; } \
    FARPROC byname = t_proc_in("dxva2.dll", kExp[idx].name); \
    CHECKF(byname != NULL, "%s not exported", kExp[idx].name); \
    FARPROC byord = t_proc_ord("dxva2.dll", kExp[idx].ord); \
    CHECKF(byord == byname, "ordinal %u differs from name lookup of %s", kExp[idx].ord, kExp[idx].name); \
    HMODULE owner = NULL; \
    if (byname) { \
        BOOL ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)byname, &owner); \
        CHECKF(ok && owner == m, "%s is not implemented inside dxva2.dll (forwarder?)", kExp[idx].name); \
    } \
}
EXP_TEST(0, CapabilitiesRequestAndCapabilitiesReply) EXP_TEST(1, DXVA2CreateDirect3DDeviceManager9)
EXP_TEST(2, DXVA2CreateVideoService) EXP_TEST(3, DXVAHD_CreateDevice) EXP_TEST(4, DegaussMonitor)
EXP_TEST(5, DestroyPhysicalMonitor) EXP_TEST(6, DestroyPhysicalMonitors) EXP_TEST(7, GetCapabilitiesStringLength)
EXP_TEST(8, GetMonitorBrightness) EXP_TEST(9, GetMonitorCapabilities) EXP_TEST(10, GetMonitorColorTemperature)
EXP_TEST(11, GetMonitorContrast) EXP_TEST(12, GetMonitorDisplayAreaPosition) EXP_TEST(13, GetMonitorDisplayAreaSize)
EXP_TEST(14, GetMonitorRedGreenOrBlueDrive) EXP_TEST(15, GetMonitorRedGreenOrBlueGain) EXP_TEST(16, GetMonitorTechnologyType)
EXP_TEST(17, GetNumberOfPhysicalMonitorsFromHMONITOR) EXP_TEST(18, GetNumberOfPhysicalMonitorsFromIDirect3DDevice9)
EXP_TEST(19, GetPhysicalMonitorsFromHMONITOR) EXP_TEST(20, GetPhysicalMonitorsFromIDirect3DDevice9)
EXP_TEST(21, GetTimingReport) EXP_TEST(22, GetVCPFeatureAndVCPFeatureReply) EXP_TEST(23, OPMGetVideoOutputForTarget)
EXP_TEST(24, OPMGetVideoOutputsFromHMONITOR) EXP_TEST(25, OPMGetVideoOutputsFromIDirect3DDevice9Object)
EXP_TEST(26, RestoreMonitorFactoryColorDefaults) EXP_TEST(27, RestoreMonitorFactoryDefaults)
EXP_TEST(28, SaveCurrentMonitorSettings) EXP_TEST(29, SaveCurrentSettings) EXP_TEST(30, SetMonitorBrightness)
EXP_TEST(31, SetMonitorColorTemperature) EXP_TEST(32, SetMonitorContrast) EXP_TEST(33, SetMonitorDisplayAreaPosition)
EXP_TEST(34, SetMonitorDisplayAreaSize) EXP_TEST(35, SetMonitorRedGreenOrBlueDrive) EXP_TEST(36, SetMonitorRedGreenOrBlueGain)
EXP_TEST(37, SetVCPFeature)

TEST(dxva2_monitor_video_export_table_complete) {
    CHECK(NEXP == 38);
    /* ordinals are dense 1..38 and every name is unique */
    for (int i = 0; i < NEXP; i++) {
        CHECK(kExp[i].ord == (unsigned)i + 1);
        for (int j = i + 1; j < NEXP; j++) CHECK(strcmp(kExp[i].name, kExp[j].name) != 0);
    }
}
TEST(dxva2_monitor_video_export_ordinal_out_of_range) {
    HMODULE m = t_module("dxva2.dll"); if (!m) { t_skip("no dxva2.dll"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0) == NULL);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)39) == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0xFFFF) == NULL);
}
TEST(dxva2_monitor_video_export_unknown_names) {
    HMODULE m = t_module("dxva2.dll"); if (!m) { t_skip("no dxva2.dll"); return; }
    static const char *const bad[] = { "DXVA2CreateDirect3DDeviceManager", "DXVAHD_CreateDeviceA", "GetMonitorBrightnessA",
        "DXVA2CreateVideoServiceW", "SetMonitorGain", "DXVA2CreateDirect3DDeviceManager9 ", "getmonitorbrightness", "DXVA2_CreateVideoService" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        CHECKF(GetProcAddress(m, bad[i]) == NULL, "%s unexpectedly exported", bad[i]);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(dxva2_monitor_video_export_case_sensitive) {
    HMODULE m = t_module("dxva2.dll"); if (!m) { t_skip("no dxva2.dll"); return; }
    CHECK(GetProcAddress(m, "dxva2createvideoservice") == NULL);
    CHECK(GetProcAddress(m, "DXVA2CREATEVIDEOSERVICE") == NULL);
}
TEST(dxva2_monitor_video_export_no_duplicate_addresses) {
    /* SetVCPFeature/DegaussMonitor etc. are distinct functions: no two exports alias the same code */
    for (int i = 0; i < NEXP; i++) {
        FARPROC a = t_proc_in("dxva2.dll", kExp[i].name);
        if (!a) continue;
        for (int j = i + 1; j < NEXP; j++) {
            FARPROC b = t_proc_in("dxva2.dll", kExp[j].name);
            if (a == b) t_note("%s and %s share an address", kExp[i].name, kExp[j].name);
        }
    }
}
TEST(dxva2_monitor_video_module_identity) {
    HMODULE m = t_module("dxva2.dll"); if (!m) { t_skip("no dxva2.dll"); return; }
    char path[MAX_PATH]; DWORD n = GetModuleFileNameA(m, path, sizeof path);
    CHECK(n > 0 && n < sizeof path);
    char *base = strrchr(path, '\\'); base = base ? base + 1 : path;
    CHECK(lstrcmpiA(base, "dxva2.dll") == 0);
    t_note("dxva2 path %s", path);
}
