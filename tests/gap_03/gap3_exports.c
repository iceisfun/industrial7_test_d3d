/* Non-public / internal exports: presence, ordinals, section placement, shared code, lookup errors.
 * Nothing here calls the exports; behaviour tests live in gap3_calls.c and tests/dx_gap_03. */
#include "t.h"
#include "gap3_pe.h"
#include "gap3_table.h"
#define NTAB (sizeof(g3_tab) / sizeof(g3_tab[0]))

static HMODULE g3_mod(const char *dll) {
    HMODULE m = t_module(dll);
    if (!m) t_skip("%s not loadable", dll);
    return m;
}
/* data export, not code */
static int g3_is_data(const char *n) { return !strcmp(n, "D3D12DeviceRemovedExtendedData"); }

#define FOR_DLL(dll) for (unsigned i = 0; i < NTAB; i++) if (!strcmp(g3_tab[i].dll, dll))

static void present(const char *dll) {
    HMODULE m = g3_mod(dll); if (!m) return;
    int n = 0;
    FOR_DLL(dll) {
        FARPROC p = GetProcAddress(m, g3_tab[i].name);
        if (t_is_wine() && !p) { t_note("%s!%s absent (emulator)", dll, g3_tab[i].name); continue; }
        CHECKF(p != NULL, "%s!%s", dll, g3_tab[i].name);
        n++;
    }
    if (!n && !t_is_wine()) CHECK(n > 0);
}
static void by_ordinal(const char *dll) {
    HMODULE m = g3_mod(dll); if (!m) return;
    FOR_DLL(dll) {
        FARPROC byname = GetProcAddress(m, g3_tab[i].name);
        if (!byname) { if (!t_is_wine()) CHECKF(0, "%s!%s missing", dll, g3_tab[i].name); continue; }
        DWORD rva = 0; unsigned ord = g3_find_named(m, g3_tab[i].name, &rva);
        CHECKF(ord != 0, "%s!%s in export directory", dll, g3_tab[i].name);
        if (ord != g3_tab[i].ord) {
            typedef LONG (WINAPI *rtl_t)(OSVERSIONINFOW *);
            rtl_t rv = (rtl_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
            OSVERSIONINFOW vi; memset(&vi, 0, sizeof vi); vi.dwOSVersionInfoSize = sizeof vi;
            if (rv && rv(&vi) == 0 && vi.dwBuildNumber == 19045) CHECKF(0, "%s!%s ordinal %u, reference build 19045 has %u", dll, g3_tab[i].name, ord, g3_tab[i].ord);
            else t_note("%s!%s ordinal %u (reference build 19045: %u)", dll, g3_tab[i].name, ord, g3_tab[i].ord);
        }
        FARPROC byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)ord);
        CHECKF(byord == byname, "%s!%s: GetProcAddress by ordinal %u equals by name", dll, g3_tab[i].name, ord);
    }
}
static void placement(const char *dll) {
    HMODULE m = g3_mod(dll); if (!m) return;
    FOR_DLL(dll) {
        DWORD rva = 0;
        if (!g3_find_named(m, g3_tab[i].name, &rva)) continue;
        if (t_is_wine() && g3_is_forwarder(m, rva)) { t_note("%s!%s forwarded (emulator)", dll, g3_tab[i].name); continue; }
        CHECKF(!g3_is_forwarder(m, rva), "%s!%s is not a forwarder", dll, g3_tab[i].name);
        DWORD fl = g3_section_flags(m, rva);
        CHECKF(fl != 0, "%s!%s RVA inside a section", dll, g3_tab[i].name);
        if (g3_is_data(g3_tab[i].name)) CHECKF(!(fl & IMAGE_SCN_MEM_EXECUTE), "%s!%s data export is not in an executable section", dll, g3_tab[i].name);
        else CHECKF((fl & IMAGE_SCN_MEM_EXECUTE) != 0, "%s!%s code export is in an executable section", dll, g3_tab[i].name);
        FARPROC p = GetProcAddress(m, g3_tab[i].name);
        CHECKF(p == (FARPROC)((char *)m + rva), "%s!%s address == base + RVA", dll, g3_tab[i].name);
    }
}
static void sorted(const char *dll) {
    HMODULE m = g3_mod(dll); if (!m) return;
    IMAGE_EXPORT_DIRECTORY *e = g3_expdir(m, NULL);
    if (!e) { t_skip("no export directory"); return; }
    DWORD *names = (DWORD *)((char *)m + e->AddressOfNames);
    for (DWORD i = 1; i < e->NumberOfNames; i++)
        CHECKF(strcmp((char *)m + names[i - 1], (char *)m + names[i]) < 0, "%s export names strictly ascending at %lu", dll, i);
    CHECK(e->NumberOfNames <= e->NumberOfFunctions);
}
static void lookup_errors(const char *dll) {
    HMODULE m = g3_mod(dll); if (!m) return;
    FOR_DLL(dll) {
        char buf[200];
        SetLastError(0);
        FARPROC p = GetProcAddress(m, "NoSuchInternalExport_gap3");
        CHECK(p == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
        /* export lookup is case sensitive */
        snprintf(buf, sizeof buf, "%s", g3_tab[i].name);
        for (char *c = buf; *c; c++) if (*c >= 'a' && *c <= 'z') *c = *c - 32; else if (*c >= 'A' && *c <= 'Z') *c = *c + 32;
        if (strcmp(buf, g3_tab[i].name)) { SetLastError(0); CHECKF(GetProcAddress(m, buf) == NULL, "case-swapped %s", buf); EXPECT_ERR(ERROR_PROC_NOT_FOUND); }
        /* trailing garbage / prefix are different names */
        snprintf(buf, sizeof buf, "%sX", g3_tab[i].name);
        SetLastError(0); CHECK(GetProcAddress(m, buf) == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
        snprintf(buf, sizeof buf, "%s", g3_tab[i].name); buf[strlen(buf) - 1] = 0;
        if (!GetProcAddress(m, buf)) { SetLastError(0); CHECK(GetProcAddress(m, buf) == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND); }
        break; /* the table-wide loop is done by the other tests; one name per module is enough here */
    }
    /* Windows 10: ordinal 0 is an invalid parameter; an ordinal outside the export table (or in a hole of it) is ERROR_INVALID_ORDINAL; names are PROC_NOT_FOUND */
    SetLastError(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    IMAGE_EXPORT_DIRECTORY *ed = g3_expdir(m, NULL);
    if (ed) {
        unsigned top = (unsigned)(ed->Base + ed->NumberOfFunctions);
        SetLastError(0);
        CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)top) == NULL); EXPECT_ERR(ERROR_INVALID_ORDINAL);
        SetLastError(0);
        CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)(top + 1)) == NULL); EXPECT_ERR(ERROR_INVALID_ORDINAL);
        if (ed->Base > 1) {
            SetLastError(0);
            CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)(ed->Base - 1)) == NULL); EXPECT_ERR(ERROR_INVALID_ORDINAL);
        }
        SetLastError(0);
        CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)(ed->Base)) != NULL); EXPECT_EQ(GetLastError(), 0);
    }
    SetLastError(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0xFFFF) == NULL); EXPECT_ERR(ERROR_INVALID_ORDINAL);
    SetLastError(0);
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0x8000) == NULL); EXPECT_ERR(ERROR_INVALID_ORDINAL);
    SetLastError(0);
    CHECK(GetProcAddress(m, "") == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
static void in_image(const char *dll) {
    HMODULE m = g3_mod(dll); if (!m) return;
    IMAGE_NT_HEADERS *nt = g3_nt(m);
    if (!nt) { t_skip("not a PE image"); return; }
    ULONG_PTR lo = (ULONG_PTR)m, hi = lo + nt->OptionalHeader.SizeOfImage;
    FOR_DLL(dll) {
        FARPROC p = GetProcAddress(m, g3_tab[i].name);
        if (!p) continue;
        if (t_is_wine() && ((ULONG_PTR)p < lo || (ULONG_PTR)p >= hi)) { t_note("%s!%s outside image (emulator forwarder)", dll, g3_tab[i].name); continue; }
        CHECKF((ULONG_PTR)p >= lo && (ULONG_PTR)p < hi, "%s!%s inside its own image", dll, g3_tab[i].name);
    }
}

#define PER_DLL(id, dll) \
    TEST(gap3_present_##id)       { present(dll); } \
    TEST(gap3_ordinal_##id)       { by_ordinal(dll); } \
    TEST(gap3_placement_##id)     { placement(dll); } \
    TEST(gap3_sorted_names_##id)  { sorted(dll); } \
    TEST(gap3_lookup_errors_##id) { lookup_errors(dll); } \
    TEST(gap3_in_image_##id)      { in_image(dll); }
PER_DLL(d3d8thk, "d3d8thk.dll")
PER_DLL(d3d9, "d3d9.dll")
PER_DLL(d3d9on12, "d3d9on12.dll")
PER_DLL(d3d10_1core, "d3d10_1core.dll")
PER_DLL(d3d10core, "d3d10core.dll")
PER_DLL(d3d10level9, "d3d10level9.dll")
PER_DLL(d3d10warp, "d3d10warp.dll")
PER_DLL(d3d11, "d3d11.dll")
PER_DLL(d3d11on12, "d3d11on12.dll")
PER_DLL(d3d12, "d3d12.dll")
PER_DLL(d3dcompiler_47, "d3dcompiler_47.dll")
PER_DLL(d3dscache, "d3dscache.dll")
PER_DLL(ddraw, "ddraw.dll")
PER_DLL(dxgi, "dxgi.dll")

/* ---- relations between modules ---------------------------------------- */
static int has(const char *dll, const char *name) { for (unsigned i = 0; i < NTAB; i++) if (!strcmp(g3_tab[i].dll, dll) && !strcmp(g3_tab[i].name, name)) return 1; return 0; }

TEST(gap3_kmt_thunks_same_set_d3d11_d3d10core_d3d10_1core) {
    const char *others[] = { "d3d10core.dll", "d3d10_1core.dll" };
    for (unsigned i = 0; i < NTAB; i++) {
        if (strcmp(g3_tab[i].dll, "d3d11.dll") || strncmp(g3_tab[i].name, "D3DKMT", 6)) continue;
        for (int k = 0; k < 2; k++) CHECKF(has(others[k], g3_tab[i].name), "%s has %s", others[k], g3_tab[i].name);
    }
    for (unsigned i = 0; i < NTAB; i++) {
        if (strncmp(g3_tab[i].name, "D3DKMT", 6)) continue;
        if (!strcmp(g3_tab[i].dll, "d3d10core.dll") || !strcmp(g3_tab[i].dll, "d3d10_1core.dll"))
            CHECKF(has("d3d11.dll", g3_tab[i].name), "d3d11 has %s", g3_tab[i].name);
    }
}
TEST(gap3_kmt_thunks_distinct_code_per_module) {
    HMODULE a = t_module("d3d11.dll"), b = t_module("d3d10core.dll"), c = t_module("d3d10_1core.dll");
    REQUIRE(a); REQUIRE(b); REQUIRE(c);
    FARPROC pa = GetProcAddress(a, "D3DKMTCloseAdapter"), pb = GetProcAddress(b, "D3DKMTCloseAdapter"), pc = GetProcAddress(c, "D3DKMTCloseAdapter");
    if (t_is_wine() && !(pa && pb && pc)) { t_skip("emulator"); return; }
    CHECK(pa && pb && pc);
    CHECK(pa != pb); CHECK(pb != pc); CHECK(pa != pc);
}
TEST(gap3_kmt_thunks_d3d10level9_larger_set) {
    int n = 0;
    for (unsigned i = 0; i < NTAB; i++) if (!strcmp(g3_tab[i].dll, "d3d10level9.dll") && !strncmp(g3_tab[i].name, "D3DKMT", 6)) n++;
    CHECK(n >= 100);
    HMODULE m = t_module("d3d10level9.dll"); REQUIRE(m);
    const char *extra[] = { "D3DKMTAcquireKeyedMutex", "D3DKMTWaitForVerticalBlankEvent2", "D3DKMTCreateKeyedMutex", "D3DKMTEnumAdapters" };
    for (int k = 0; k < 4; k++) if (has("d3d10level9.dll", extra[k])) CHECK(GetProcAddress(m, extra[k]) != NULL);
}
TEST(gap3_d3d8thk_only_osthunk_exports) {
    HMODULE m = t_module("d3d8thk.dll"); REQUIRE(m);
    IMAGE_EXPORT_DIRECTORY *e = g3_expdir(m, NULL); REQUIRE(e);
    DWORD *names = (DWORD *)((char *)m + e->AddressOfNames);
    CHECK(e->NumberOfNames == 56);
    for (DWORD i = 0; i < e->NumberOfNames; i++) CHECKF(!strncmp((char *)m + names[i], "OsThunkD", 8), "%s", (char *)m + names[i]);
    CHECK(e->NumberOfNames == e->NumberOfFunctions);
}
TEST(gap3_d3d8thk_osthunk_d3d_vs_dd_counts) {
    int d3 = 0, dd = 0;
    for (unsigned i = 0; i < NTAB; i++) {
        if (strcmp(g3_tab[i].dll, "d3d8thk.dll")) continue;
        if (!strncmp(g3_tab[i].name, "OsThunkD3d", 10)) d3++; else if (!strncmp(g3_tab[i].name, "OsThunkDd", 9)) dd++;
    }
    EXPECT_EQ(d3, 5); EXPECT_EQ(dd, 51);
}
TEST(gap3_d3d8thk_no_dxgi_symbols) {
    HMODULE m = t_module("d3d8thk.dll"); REQUIRE(m);
    SetLastError(0);
    CHECK(GetProcAddress(m, "D3DKMTCloseAdapter") == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    CHECK(GetProcAddress(m, "OsThunkD3dContextCreate") != NULL || t_is_wine());
}
TEST(gap3_open_adapter10_2_in_five_modules) {
    const char *mods[] = { "d3d10_1core.dll", "d3d10core.dll", "d3d10warp.dll", "d3d11.dll", "d3d10level9.dll" };
    FARPROC p[5];
    for (int i = 0; i < 5; i++) {
        HMODULE m = t_module(mods[i]); if (!m) { t_skip("%s", mods[i]); return; }
        p[i] = GetProcAddress(m, "OpenAdapter10_2");
        if (!p[i]) { if (t_is_wine()) { t_skip("emulator"); return; } CHECKF(0, "%s!OpenAdapter10_2", mods[i]); }
    }
    for (int i = 0; i < 5; i++) for (int j = i + 1; j < 5; j++) CHECK(p[i] != p[j]);
}
TEST(gap3_open_adapter_in_two_modules) {
    HMODULE a = t_module("d3d9on12.dll"), b = t_module("d3d10warp.dll");
    REQUIRE(a); REQUIRE(b);
    FARPROC pa = GetProcAddress(a, "OpenAdapter"), pb = GetProcAddress(b, "OpenAdapter");
    if (t_is_wine() && !(pa && pb)) { t_skip("emulator"); return; }
    CHECK(pa && pb); CHECK(pa != pb);
}
/* identical-code folding visible in the export tables (verified on the reference Windows 10 build) */
static void same_addr(const char *dll, const char *a, const char *b, const char *c) {
    HMODULE m = t_module(dll); if (!m) { t_skip("%s", dll); return; }
    FARPROC pa = GetProcAddress(m, a), pb = GetProcAddress(m, b), pc = GetProcAddress(m, c);
    if (t_is_wine() && !(pa && pb && pc)) { t_skip("emulator"); return; }
    CHECK(pa != NULL); CHECK(pa == pb); CHECK(pb == pc);
}
TEST(gap3_dxgid3d10_stubs_share_one_body) { same_addr("dxgi.dll", "DXGID3D10CreateDevice", "DXGID3D10CreateLayeredDevice", "DXGID3D10RegisterLayers"); }
TEST(gap3_dxgi_pix_capture_stubs_share_one_body) { same_addr("dxgi.dll", "PIXBeginCapture", "PIXEndCapture", "PIXGetCaptureState"); }
TEST(gap3_d3d10_1core_stubs_share_one_body) { same_addr("d3d10_1core.dll", "D3D10CoreCreateDevice1", "D3D10CoreGetSupportedVersions", "D3D10CoreRegisterLayers"); }
TEST(gap3_dxgid3d10_stub_differs_from_pix_stub) {
    HMODULE m = t_module("dxgi.dll"); REQUIRE(m);
    FARPROC a = GetProcAddress(m, "DXGID3D10CreateDevice"), b = GetProcAddress(m, "PIXBeginCapture");
    REQUIRE(a); REQUIRE(b);
    CHECK(a != b);
}
TEST(gap3_d3d12_data_export_is_not_code) {
    HMODULE m = t_module("d3d12.dll"); REQUIRE(m);
    DWORD rva = 0; unsigned ord = g3_find_named(m, "D3D12DeviceRemovedExtendedData", &rva);
    if (!ord) { t_skip("export absent"); return; }
    DWORD fl = g3_section_flags(m, rva);
    CHECK(fl != 0); CHECK(!(fl & IMAGE_SCN_MEM_EXECUTE));
    CHECK(fl & IMAGE_SCN_MEM_READ);
}
TEST(gap3_d3d12_pix_exports_are_code) {
    HMODULE m = t_module("d3d12.dll"); REQUIRE(m);
    const char *n[] = { "D3D12PIXEventsReplaceBlock", "D3D12PIXGetThreadInfo", "D3D12PIXNotifyWakeFromFenceSignal", "D3D12PIXReportCounter" };
    for (int i = 0; i < 4; i++) {
        DWORD rva; if (!g3_find_named(m, n[i], &rva)) { t_skip("%s absent", n[i]); return; }
        CHECKF(g3_section_flags(m, rva) & IMAGE_SCN_MEM_EXECUTE, "%s", n[i]);
    }
}
TEST(gap3_d3d12_core_layer_exports_present) {
    HMODULE m = t_module("d3d12.dll"); REQUIRE(m);
    const char *n[] = { "D3D12CoreCreateLayeredDevice", "D3D12CoreGetLayeredDeviceSize", "D3D12CoreRegisterLayers" };
    for (int i = 0; i < 3; i++) { if (t_is_wine() && !GetProcAddress(m, n[i])) continue; CHECKF(GetProcAddress(m, n[i]) != NULL, "%s", n[i]); }
}
TEST(gap3_d3d11_core_layer_exports_present) {
    HMODULE m = t_module("d3d11.dll"); REQUIRE(m);
    const char *n[] = { "D3D11CoreCreateLayeredDevice", "D3D11CoreGetLayeredDeviceSize", "D3D11CoreRegisterLayers", "D3D11CreateDeviceForD3D12", "OpenAdapter10_2" };
    for (int i = 0; i < 5; i++) { if (t_is_wine() && !GetProcAddress(m, n[i])) continue; CHECKF(GetProcAddress(m, n[i]) != NULL, "%s", n[i]); }
}
TEST(gap3_ddraw_internal_exports_present) {
    HMODULE m = t_module("ddraw.dll"); REQUIRE(m);
    const char *n[] = { "CompleteCreateSysmemSurface", "DDGetAttachedSurfaceLcl", "DDInternalLock", "DDInternalUnlock", "DSoundHelp", "GetDDSurfaceLocal" };
    for (int i = 0; i < 6; i++) { if (t_is_wine() && !GetProcAddress(m, n[i])) continue; CHECKF(GetProcAddress(m, n[i]) != NULL, "%s", n[i]); }
}
TEST(gap3_exports_not_in_wrong_module) {
    /* a few internal names live in exactly one module of the family */
    struct { const char *dll, *name; } no[] = {
        { "d3d11.dll", "ShaderCache_Create" }, { "dxgi.dll", "OpenAdapter10_2" }, { "d3d12.dll", "PIXBeginCapture" },
        { "d3dscache.dll", "D3D11RefGetLastCreation" }, { "d3d9.dll", "OpenAdapter" }, { "ddraw.dll", "D3DKMTCloseAdapter" } };
    for (unsigned i = 0; i < sizeof no / sizeof no[0]; i++) {
        HMODULE m = t_module(no[i].dll); if (!m) continue;
        SetLastError(0);
        CHECKF(GetProcAddress(m, no[i].name) == NULL, "%s!%s", no[i].dll, no[i].name);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
