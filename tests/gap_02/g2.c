#include "d3dh.h"
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <lowlevelmonitorconfigurationapi.h>

/* High-level monitor setters/savers are thin wrappers over SetVCPFeature / DDCCISaveCurrentSettings
 * (SetMonitorBrightness -> VCP 0x10, SetMonitorContrast -> VCP 0x12, RestoreMonitorFactoryDefaults -> VCP 0x04,
 * SaveCurrentMonitorSettings == SaveCurrentSettings -> gdi32!DDCCISaveCurrentSettings; failure => FALSE + last error).
 * Only invalid handles are used here, so no monitor is ever modified. */

static const HANDLE kH[] = { NULL, (HANDLE)(ULONG_PTR)1, (HANDLE)(ULONG_PTR)4, INVALID_HANDLE_VALUE };
#define NH ((int)(sizeof kH / sizeof *kH))
static const DWORD kV[] = { 0, 1, 50, 100, 101, 255, 256, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
#define NV ((int)(sizeof kV / sizeof *kV))

typedef BOOL (WINAPI *set_fn)(HANDLE, DWORD);
typedef BOOL (WINAPI *act_fn)(HANDLE);

static DWORD raw_vcp(HANDLE h, BYTE code, DWORD v, BOOL *ok)
{
    BOOL (WINAPI *f)(HANDLE, BYTE, DWORD) = U(SetVCPFeature);
    SET_ERR(0); *ok = f(h, code, v); return GetLastError();
}
static int need_gle(void) { return !t_is_wine(); }
/* Win10 (probed, identical for every invalid handle value): every failing high-level monitor call leaves the NTSTATUS-style code
 * 0xC026258C in GetLastError, while gdi32!DDCCISaveCurrentSettings itself returns the status 0xC01E058C. */
#define DDCCI_INVALID_HANDLE 0xC026258Cu
#define DDCCI_SAVE_STATUS 0xC01E058Cu

static void set_vs_raw(const char *nm, set_fn f, BYTE code)
{
    BOOL (WINAPI *raw)(HANDLE, BYTE, DWORD) = U(SetVCPFeature);
    if (!f || !raw) { t_skip("missing export"); return; }
    for (int i = 0; i < NH; i++)
        for (int k = 0; k < NV; k++) {
            BOOL rok; DWORD re = raw_vcp(kH[i], code, kV[k], &rok);
            SET_ERR(0); BOOL r = f(kH[i], kV[k]); DWORD e = GetLastError();
            CHECKF(r == rok, "%s h=%p v=%lu", nm, kH[i], kV[k]);
            CHECKF(r == FALSE, "%s h=%p v=%lu succeeded", nm, kH[i], kV[k]);
            CHECKF(e == re, "%s h=%p v=%lu gle %lu raw %lu", nm, kH[i], kV[k], e, re);
            if (need_gle()) CHECKF(e == DDCCI_INVALID_HANDLE, "%s h=%p v=%lu gle 0x%lx", nm, kH[i], kV[k], e);
        }
}
TEST(gap2_brightness_set_matches_vcp10_table) { set_vs_raw("brightness", U(SetMonitorBrightness), 0x10); }
TEST(gap2_contrast_set_matches_vcp12_table) { set_vs_raw("contrast", U(SetMonitorContrast), 0x12); }

static void act_vs_raw(const char *nm, act_fn f, BYTE code)
{
    if (!f || !U(SetVCPFeature)) { t_skip("missing export"); return; }
    for (int i = 0; i < NH; i++) {
        BOOL rok; DWORD re = raw_vcp(kH[i], code, 1, &rok);
        SET_ERR(0); BOOL r = f(kH[i]); DWORD e = GetLastError();
        CHECKF(r == FALSE, "%s h=%p", nm, kH[i]);
        CHECKF(e == re, "%s h=%p gle %lu raw %lu", nm, kH[i], e, re);
        if (need_gle()) CHECKF(e == DDCCI_INVALID_HANDLE, "%s h=%p gle 0x%lx", nm, kH[i], e);
    }
}
TEST(gap2_restore_factory_defaults_matches_vcp04_table) { act_vs_raw("restore", U(RestoreMonitorFactoryDefaults), 0x04); }

TEST(gap2_brightness_set_repeatable) {
    set_fn f = U(SetMonitorBrightness); REQUIRE(f);
    for (int i = 0; i < NH; i++) {
        SET_ERR(0); BOOL a = f(kH[i], 10); DWORD ea = GetLastError();
        SET_ERR(0); BOOL b = f(kH[i], 10); DWORD eb = GetLastError();
        CHECK(a == b); EXPECT_EQ(ea, eb);
    }
}
TEST(gap2_contrast_set_repeatable) {
    set_fn f = U(SetMonitorContrast); REQUIRE(f);
    for (int i = 0; i < NH; i++) {
        SET_ERR(0); BOOL a = f(kH[i], 10); DWORD ea = GetLastError();
        SET_ERR(0); BOOL b = f(kH[i], 10); DWORD eb = GetLastError();
        CHECK(a == b); EXPECT_EQ(ea, eb);
    }
}
TEST(gap2_brightness_and_contrast_same_error_for_same_handle) {
    set_fn b = U(SetMonitorBrightness), c = U(SetMonitorContrast); REQUIRE(b); REQUIRE(c);
    for (int i = 0; i < NH; i++) {
        SET_ERR(0); b(kH[i], 30); DWORD eb = GetLastError();
        SET_ERR(0); c(kH[i], 30); DWORD ec = GetLastError();
        CHECKF(eb == ec, "h=%p brightness gle %lu contrast gle %lu", kH[i], eb, ec);
    }
}
TEST(gap2_brightness_value_does_not_change_error) {
    set_fn f = U(SetMonitorBrightness); REQUIRE(f);
    for (int i = 0; i < NH; i++) {
        SET_ERR(0); f(kH[i], 0); DWORD e0 = GetLastError();
        for (int k = 1; k < NV; k++) {
            SET_ERR(0); f(kH[i], kV[k]); CHECKF(GetLastError() == e0, "h=%p v=%lu", kH[i], kV[k]);
        }
    }
}
TEST(gap2_save_aliases_table) {
    act_fn a = U(SaveCurrentMonitorSettings), b = U(SaveCurrentSettings); REQUIRE(a); REQUIRE(b);
    for (int i = 0; i < NH; i++) {
        SET_ERR(0); BOOL ra = a(kH[i]); DWORD ea = GetLastError();
        SET_ERR(0); BOOL rb = b(kH[i]); DWORD eb = GetLastError();
        CHECKF(ra == FALSE && rb == FALSE, "h=%p", kH[i]);
        CHECKF(ea == eb, "h=%p %lu vs %lu", kH[i], ea, eb);
        if (need_gle()) CHECKF(ea == DDCCI_INVALID_HANDLE, "h=%p gle 0x%lx", kH[i], ea);
    }
}
TEST(gap2_save_matches_gdi32_ddcci) {
    act_fn a = U(SaveCurrentMonitorSettings); REQUIRE(a);
    UDECL(LONG, DDCCISaveCurrentSettings, (HANDLE));
    HMODULE g = LoadLibraryA("gdi32.dll");
    DDCCISaveCurrentSettings_pfn raw = g ? (DDCCISaveCurrentSettings_pfn)GetProcAddress(g, "DDCCISaveCurrentSettings") : NULL;
    if (!raw) { t_skip("gdi32 has no DDCCISaveCurrentSettings"); return; }
    for (int i = 0; i < NH; i++) {
        LONG st = raw(kH[i]);
        SET_ERR(0); BOOL r = a(kH[i]); DWORD e = GetLastError();
        CHECKF((st >= 0) == (r != FALSE), "h=%p status 0x%lx ret %d", kH[i], (unsigned long)st, r);
        if (need_gle()) { EXPECT_EQ((ULONG_PTR)(DWORD)st, DDCCI_SAVE_STATUS); EXPECT_EQ(e, DDCCI_INVALID_HANDLE); }
    }
}
TEST(gap2_save_does_not_touch_other_errors_when_called_twice) {
    act_fn a = U(SaveCurrentSettings); REQUIRE(a);
    SET_ERR(0); BOOL r1 = a(NULL); DWORD e1 = GetLastError();
    SET_ERR(0); BOOL r2 = a(NULL); DWORD e2 = GetLastError();
    CHECK(r1 == r2); EXPECT_EQ(e1, e2);
}
TEST(gap2_exports_resolve_in_dxva2) {
    static const char *const n[] = { "RestoreMonitorFactoryDefaults", "SaveCurrentMonitorSettings", "SaveCurrentSettings",
        "SetMonitorBrightness", "SetMonitorContrast" };
    HMODULE m = t_module("dxva2.dll"); if (!m) { t_skip("no dxva2"); return; }
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        FARPROC p = GetProcAddress(m, n[i]);
        CHECKF(p != NULL, "%s", n[i]);
        CHECKF(p == (FARPROC)t_proc_in("dxva2.dll", n[i]), "%s", n[i]);
    }
}
TEST(gap2_exports_not_decorated) {
    static const char *const n[] = { "SetMonitorBrightnessA", "SetMonitorBrightnessW", "SetMonitorContrastA",
        "SaveCurrentSettingsW", "SaveCurrentMonitorSettingsA", "RestoreMonitorFactoryDefaultsW" };
    HMODULE m = t_module("dxva2.dll"); if (!m) { t_skip("no dxva2"); return; }
    for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        CHECKF(GetProcAddress(m, n[i]) == NULL, "%s", n[i]);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
