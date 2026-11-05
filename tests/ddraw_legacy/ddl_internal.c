/* ddraw.dll internal exports (used by d3d8/d3dim/dsound) and d3d8thk.dll thunks. */
#include "dl.h"

static const char *k_thunks[] = {
    "OsThunkD3dContextCreate",
    "OsThunkD3dContextDestroy",
    "OsThunkD3dContextDestroyAll",
    "OsThunkD3dDrawPrimitives2",
    "OsThunkD3dValidateTextureStageState",
    "OsThunkDdAddAttachedSurface",
    "OsThunkDdAlphaBlt",
    "OsThunkDdAttachSurface",
    "OsThunkDdBeginMoCompFrame",
    "OsThunkDdBlt",
    "OsThunkDdCanCreateD3DBuffer",
    "OsThunkDdCanCreateSurface",
    "OsThunkDdColorControl",
    "OsThunkDdCreateD3DBuffer",
    "OsThunkDdCreateDirectDrawObject",
    "OsThunkDdCreateMoComp",
    "OsThunkDdCreateSurface",
    "OsThunkDdCreateSurfaceEx",
    "OsThunkDdCreateSurfaceObject",
    "OsThunkDdDeleteDirectDrawObject",
    "OsThunkDdDeleteSurfaceObject",
    "OsThunkDdDestroyD3DBuffer",
    "OsThunkDdDestroyMoComp",
    "OsThunkDdDestroySurface",
    "OsThunkDdEndMoCompFrame",
    "OsThunkDdFlip",
    "OsThunkDdFlipToGDISurface",
    "OsThunkDdGetAvailDriverMemory",
    "OsThunkDdGetBltStatus",
    "OsThunkDdGetDC",
    "OsThunkDdGetDriverInfo",
    "OsThunkDdGetDriverState",
    "OsThunkDdGetDxHandle",
    "OsThunkDdGetFlipStatus",
    "OsThunkDdGetInternalMoCompInfo",
    "OsThunkDdGetMoCompBuffInfo",
    "OsThunkDdGetMoCompFormats",
    "OsThunkDdGetMoCompGuids",
    "OsThunkDdGetScanLine",
    "OsThunkDdLock",
    "OsThunkDdLockD3D",
    "OsThunkDdQueryDirectDrawObject",
    "OsThunkDdQueryMoCompStatus",
    "OsThunkDdReenableDirectDrawObject",
    "OsThunkDdReleaseDC",
    "OsThunkDdRenderMoComp",
    "OsThunkDdResetVisrgn",
    "OsThunkDdSetColorKey",
    "OsThunkDdSetExclusiveMode",
    "OsThunkDdSetGammaRamp",
    "OsThunkDdSetOverlayPosition",
    "OsThunkDdUnattachSurface",
    "OsThunkDdUnlock",
    "OsThunkDdUnlockD3D",
    "OsThunkDdUpdateOverlay",
    "OsThunkDdWaitForVerticalBlank",
};

/* ---------------- d3d8thk.dll: resolve-only ---------------- */
TEST(ddraw_legacy_d3d8thk_loads) {
    HMODULE m = t_module("d3d8thk.dll");
    if (!m) { t_skip("d3d8thk.dll not loadable"); return; }
    char path[MAX_PATH];
    CHECK(GetModuleFileNameA(m, path, sizeof path) > 0);
    t_note("d3d8thk.dll loaded from %s", path);
}
TEST(ddraw_legacy_d3d8thk_exports_resolve) {
    HMODULE m = t_module("d3d8thk.dll");
    if (!m) { t_skip("d3d8thk.dll not loadable"); return; }
    for (unsigned i = 0; i < sizeof k_thunks / sizeof *k_thunks; i++)
        CHECKF(GetProcAddress(m, k_thunks[i]) != NULL, "export %s", k_thunks[i]);
}
TEST(ddraw_legacy_d3d8thk_exports_inside_module) {
    HMODULE m = t_module("d3d8thk.dll");
    if (!m) { t_skip("d3d8thk.dll not loadable"); return; }
    for (unsigned i = 0; i < sizeof k_thunks / sizeof *k_thunks; i++) {
        FARPROC p = GetProcAddress(m, k_thunks[i]);
        if (!p) continue;
        HMODULE owner = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
        CHECKF(owner == m, "%s resolves outside d3d8thk.dll", k_thunks[i]);
    }
}
TEST(ddraw_legacy_d3d8thk_unknown_export) {
    HMODULE m = t_module("d3d8thk.dll");
    if (!m) { t_skip("d3d8thk.dll not loadable"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, "OsThunkDdNoSuchThing") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    CHECK(GetProcAddress(m, "osthunkddblt") == NULL);        /* case sensitive */
}
TEST(ddraw_legacy_d3d8thk_export_count) {
    HMODULE m = t_module("d3d8thk.dll");
    if (!m) { t_skip("d3d8thk.dll not loadable"); return; }
    /* walk the export directory: the named exports are exactly the documented thunk set */
    BYTE *base = (BYTE *)m;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd.VirtualAddress) { t_skip("no export directory"); return; }
    IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(base + dd.VirtualAddress);
    EXPECT_EQ(ed->NumberOfNames, sizeof k_thunks / sizeof *k_thunks);
    DWORD *names = (DWORD *)(base + ed->AddressOfNames);
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        const char *n = (const char *)(base + names[i]);
        int found = 0;
        for (unsigned k = 0; k < sizeof k_thunks / sizeof *k_thunks; k++) if (!strcmp(n, k_thunks[k])) found = 1;
        CHECKF(found, "unexpected export %s", n);
    }
}
TEST(ddraw_legacy_d3d8thk_forwarders_none) {
    HMODULE m = t_module("d3d8thk.dll");
    if (!m) { t_skip("d3d8thk.dll not loadable"); return; }
    BYTE *base = (BYTE *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd.VirtualAddress) { t_skip("no export directory"); return; }
    IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(base + dd.VirtualAddress);
    DWORD *funcs = (DWORD *)(base + ed->AddressOfFunctions);
    int fwd = 0;
    for (DWORD i = 0; i < ed->NumberOfFunctions; i++)
        if (funcs[i] >= dd.VirtualAddress && funcs[i] < dd.VirtualAddress + dd.Size) fwd++;
    EXPECT_EQ(fwd, 0);
}

/* ---------------- ddraw.dll export directory ---------------- */
TEST(ddraw_legacy_exports_directory_names) {
    HMODULE m = t_module("ddraw.dll");
    if (!m) { t_skip("ddraw.dll not loadable"); return; }
    BYTE *base = (BYTE *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd.VirtualAddress) { t_skip("no export directory"); return; }
    IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(base + dd.VirtualAddress);
    const char *want[] = { "AcquireDDThreadLock", "CompleteCreateSysmemSurface", "D3DParseUnknownCommand", "DDGetAttachedSurfaceLcl",
        "DDInternalLock", "DDInternalUnlock", "DSoundHelp", "DirectDrawCreate", "DirectDrawCreateClipper", "DirectDrawCreateEx",
        "DirectDrawEnumerateA", "DirectDrawEnumerateExA", "DirectDrawEnumerateExW", "DirectDrawEnumerateW", "DllCanUnloadNow",
        "DllGetClassObject", "GetDDSurfaceLocal", "GetOLEThunkData", "GetSurfaceFromDC", "RegisterSpecialCase", "ReleaseDDThreadLock", "SetAppCompatData" };
    EXPECT_EQ(ed->NumberOfNames, sizeof want / sizeof *want);
    DWORD *names = (DWORD *)(base + ed->AddressOfNames);
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        const char *n = (const char *)(base + names[i]);
        int found = 0;
        for (unsigned k = 0; k < sizeof want / sizeof *want; k++) if (!strcmp(n, want[k])) found = 1;
        CHECKF(found, "unexpected ddraw export %s", n);
    }
}
TEST(ddraw_legacy_exports_ordinals_stable) {
    /* ordinals documented by the export table: AcquireDDThreadLock=1 ... SetAppCompatData=22 (alphabetical-ish, 1-based) */
    HMODULE m = t_module("ddraw.dll");
    if (!m) { t_skip("ddraw.dll not loadable"); return; }
    struct { const char *n; WORD ord; } o[] = {
        { "AcquireDDThreadLock", 1 }, { "CompleteCreateSysmemSurface", 2 }, { "D3DParseUnknownCommand", 3 }, { "DDGetAttachedSurfaceLcl", 4 },
        { "DDInternalLock", 5 }, { "DDInternalUnlock", 6 }, { "DSoundHelp", 7 }, { "DirectDrawCreate", 8 }, { "DirectDrawCreateClipper", 9 },
        { "DirectDrawCreateEx", 10 }, { "DirectDrawEnumerateA", 11 }, { "DirectDrawEnumerateExA", 12 }, { "DirectDrawEnumerateExW", 13 },
        { "DirectDrawEnumerateW", 14 }, { "DllCanUnloadNow", 15 }, { "DllGetClassObject", 16 }, { "GetDDSurfaceLocal", 17 }, { "GetOLEThunkData", 18 },
        { "GetSurfaceFromDC", 19 }, { "RegisterSpecialCase", 20 }, { "ReleaseDDThreadLock", 21 }, { "SetAppCompatData", 22 },
    };
    for (unsigned i = 0; i < sizeof o / sizeof *o; i++) {
        FARPROC byname = GetProcAddress(m, o[i].n);
        FARPROC byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)o[i].ord);
        CHECKF(byname != NULL && byname == byord, "%s ordinal %u", o[i].n, o[i].ord);
    }
}

/* ---------------- AcquireDDThreadLock / ReleaseDDThreadLock ---------------- */
typedef void (WINAPI *lock_fn)(void);
TEST(ddraw_legacy_threadlock_pairing) {
    lock_fn acq = (lock_fn)dl_export("AcquireDDThreadLock"), rel = (lock_fn)dl_export("ReleaseDDThreadLock");
    REQUIRE(acq); REQUIRE(rel);
    acq(); rel();
    acq(); acq(); acq(); rel(); rel(); rel();     /* recursive lock */
}
struct tl { lock_fn acq, rel; HANDLE got, go; };
static DWORD WINAPI tl_thread(LPVOID p) {
    struct tl *t = p;
    t->acq();
    SetEvent(t->got);
    t->rel();
    return 0;
}
TEST(ddraw_legacy_threadlock_excludes_other_threads) {
    struct tl t;
    t.acq = (lock_fn)dl_export("AcquireDDThreadLock"); t.rel = (lock_fn)dl_export("ReleaseDDThreadLock");
    REQUIRE(t.acq); REQUIRE(t.rel);
    t.got = CreateEventA(NULL, TRUE, FALSE, NULL);
    t.acq();
    HANDLE th = CreateThread(NULL, 0, tl_thread, &t, 0, NULL);
    DWORD r = WaitForSingleObject(t.got, 300);
    EXPECT_EQ(r, WAIT_TIMEOUT);                    /* the other thread cannot get the lock while we hold it */
    t.rel();
    r = WaitForSingleObject(t.got, 5000);
    EXPECT_EQ(r, WAIT_OBJECT_0);                   /* ... and gets it as soon as we let go */
    WaitForSingleObject(th, 5000);
    CloseHandle(th); CloseHandle(t.got);
}
TEST(ddraw_legacy_threadlock_recursive_hold_blocks_until_all_released) {
    struct tl t;
    t.acq = (lock_fn)dl_export("AcquireDDThreadLock"); t.rel = (lock_fn)dl_export("ReleaseDDThreadLock");
    REQUIRE(t.acq); REQUIRE(t.rel);
    t.got = CreateEventA(NULL, TRUE, FALSE, NULL);
    t.acq(); t.acq();
    HANDLE th = CreateThread(NULL, 0, tl_thread, &t, 0, NULL);
    t.rel();
    EXPECT_EQ(WaitForSingleObject(t.got, 300), WAIT_TIMEOUT);   /* one release of two is not enough */
    t.rel();
    EXPECT_EQ(WaitForSingleObject(t.got, 5000), WAIT_OBJECT_0);
    WaitForSingleObject(th, 5000);
    CloseHandle(th); CloseHandle(t.got);
}
TEST(ddraw_legacy_threadlock_blocks_api_calls) {
    lock_fn acq = (lock_fn)dl_export("AcquireDDThreadLock"), rel = (lock_fn)dl_export("ReleaseDDThreadLock");
    REQUIRE(acq); REQUIRE(rel);
    dl_createclipper_t cc = DL_PROC(dl_createclipper_t, "DirectDrawCreateClipper"); REQUIRE(cc);
    /* the same thread may enter public entry points while holding the lock (the CS is recursive) */
    acq();
    LPDIRECTDRAWCLIPPER c = NULL;
    HRESULT hr = cc(0, &c, NULL);
    rel();
    EXPECT_HR(hr, DD_OK);
    if (c) IDirectDrawClipper_Release(c);
}

/* ---------------- D3DParseUnknownCommand (pure) ---------------- */
typedef HRESULT (WINAPI *parse_fn)(LPVOID cmd, LPVOID *next);
#define D3DERR_COMMAND_UNPARSED_VAL ((HRESULT)0x88760bb8)
TEST(ddraw_legacy_parse_unknown_always_sets_output) {
    parse_fn f = (parse_fn)dl_export("D3DParseUnknownCommand"); REQUIRE(f);
    for (int op = 0; op < 256; op++) {
        BYTE cmd[16] = { (BYTE)op, 1, 0, 0 };
        LPVOID out = (LPVOID)(ULONG_PTR)0x11;
        f(cmd, &out);
        CHECKF(op == 13 || op == 28 || op == 29 || out == (LPVOID)cmd, "opcode %d: output not initialised to the input pointer", op);
    }
}
TEST(ddraw_legacy_parse_unknown_error_table) {
    parse_fn f = (parse_fn)dl_export("D3DParseUnknownCommand"); REQUIRE(f);
    for (int op = 0; op < 256; op++) {
        HRESULT want;
        if (op < 4) want = E_FAIL;
        else if (op < 8 || op == 9) want = D3DERR_COMMAND_UNPARSED_VAL;
        else if (op == 8) want = E_FAIL;
        else if (op >= 10 && op <= 12) want = D3DERR_COMMAND_UNPARSED_VAL;
        else if (op == 13 || op == 28 || op == 29) want = S_OK;
        else if (op == 14) want = D3DERR_COMMAND_UNPARSED_VAL;
        else want = E_FAIL;
        BYTE cmd[32] = { (BYTE)op, 2, 3, 0 };
        LPVOID out = NULL;
        HRESULT hr = f(cmd, &out);
        CHECKF(hr == want, "opcode %d -> %s, want %s", op, d3dh_hr_name(hr), d3dh_hr_name(want));
    }
}
TEST(ddraw_legacy_parse_unknown_skip_distances) {
    parse_fn f = (parse_fn)dl_export("D3DParseUnknownCommand"); REQUIRE(f);
    BYTE cmd[512];
    LPVOID out;
    /* opcode 13 (D3DOP_STATERENDER-like): next = cmd + 4 + wCount * bSize */
    memset(cmd, 0, sizeof cmd); cmd[0] = 13; cmd[1] = 12; *(WORD *)(cmd + 2) = 5;
    out = NULL; EXPECT_HR(f(cmd, &out), S_OK); CHECK(out == (LPVOID)(cmd + 4 + 5 * 12));
    memset(cmd, 0, sizeof cmd); cmd[0] = 13; cmd[1] = 0; *(WORD *)(cmd + 2) = 9;
    out = NULL; EXPECT_HR(f(cmd, &out), S_OK); CHECK(out == (LPVOID)(cmd + 4));
    memset(cmd, 0, sizeof cmd); cmd[0] = 13; cmd[1] = 4; *(WORD *)(cmd + 2) = 0;
    out = NULL; EXPECT_HR(f(cmd, &out), S_OK); CHECK(out == (LPVOID)(cmd + 4));
    /* opcode 28: next = cmd + 4 + wCount * 16 */
    memset(cmd, 0, sizeof cmd); cmd[0] = 28; *(WORD *)(cmd + 2) = 3;
    out = NULL; EXPECT_HR(f(cmd, &out), S_OK); CHECK(out == (LPVOID)(cmd + 4 + 48));
    /* opcode 29: next = cmd + 4 + wCount * 8 */
    memset(cmd, 0, sizeof cmd); cmd[0] = 29; *(WORD *)(cmd + 2) = 7;
    out = NULL; EXPECT_HR(f(cmd, &out), S_OK); CHECK(out == (LPVOID)(cmd + 4 + 56));
    memset(cmd, 0, sizeof cmd); cmd[0] = 29; *(WORD *)(cmd + 2) = 0;
    out = NULL; EXPECT_HR(f(cmd, &out), S_OK); CHECK(out == (LPVOID)(cmd + 4));
    /* large counts: 16 bit count times 16 stays well inside 64-bit arithmetic */
    static BYTE big[4 + 65535 * 16];
    memset(big, 0, 4); big[0] = 28; *(WORD *)(big + 2) = 65535;
    out = NULL; EXPECT_HR(f(big, &out), S_OK); CHECK(out == (LPVOID)(big + 4 + 65535 * 16));
}
TEST(ddraw_legacy_parse_unknown_failure_keeps_input_pointer) {
    parse_fn f = (parse_fn)dl_export("D3DParseUnknownCommand"); REQUIRE(f);
    BYTE cmd[8] = { 1, 0, 0, 0 };
    LPVOID out = NULL;
    EXPECT_HR(f(cmd, &out), E_FAIL);
    CHECK(out == (LPVOID)cmd);
    cmd[0] = 5;
    out = NULL;
    EXPECT_HR(f(cmd, &out), D3DERR_COMMAND_UNPARSED_VAL);
    CHECK(out == (LPVOID)cmd);
}

/* ---------------- GetOLEThunkData / SetAppCompatData / RegisterSpecialCase / GetSurfaceFromDC ---------------- */
typedef ULONG_PTR (WINAPI *thunk_fn)(ULONG_PTR);
TEST(ddraw_legacy_olethunk_unused_selectors) {
    thunk_fn f = (thunk_fn)dl_export("GetOLEThunkData"); REQUIRE(f);
    ULONG_PTR sel[] = { 0, 4, 7, 8, 9, 100, 0xffff, 0x100000000ull };
    for (unsigned i = 0; i < sizeof sel / sizeof *sel; i++) {
        ULONG_PTR r = f(sel[i]);
        CHECKF(r == 0, "selector %llu -> %llx", (unsigned long long)sel[i], (unsigned long long)r);
    }
}
TEST(ddraw_legacy_olethunk_check_exclusive_mode_pointer) {
    thunk_fn f = (thunk_fn)dl_export("GetOLEThunkData"); REQUIRE(f);
    ULONG_PTR p = f(5);
    CHECK(p != 0);
    HMODULE owner = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
    CHECK(owner == t_module("ddraw.dll"));            /* an internal ddraw routine */
    EXPECT_EQ(f(5), p);                               /* stable */
}
TEST(ddraw_legacy_olethunk_driver_list) {
    thunk_fn f = (thunk_fn)dl_export("GetOLEThunkData"); REQUIRE(f);
    DL_SKIP_IF_NO_DD1(dd);
    CHECK(f(2) != 0);                                 /* lpDriverObjectList is non-empty while a DirectDraw object lives */
    IDirectDraw_Release(dd);
    t_note("driver list after the last release: %p", (void *)f(2));
    t_note("last frame rate selector: %lu, attached processes: %p", (unsigned long)f(1), (void *)f(3));
}
TEST(ddraw_legacy_appcompat_noop_selectors) {
    typedef void (WINAPI *fn)(int, DWORD);
    fn f = (fn)dl_export("SetAppCompatData"); REQUIRE(f);
    int sel[] = { 0, 6, 13, 14, 99, 1000, -1, -100, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof sel / sizeof *sel; i++) f(sel[i], 0x12345678);
    DL_SKIP_IF_NO_DD7(dd);                            /* ddraw still works after unknown selectors */
    DDCAPS caps; memset(&caps, 0, sizeof caps); caps.dwSize = sizeof caps;
    EXPECT_HR(IDirectDraw7_GetCaps(dd, &caps, NULL), DD_OK);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_appcompat_reset_selectors) {
    typedef void (WINAPI *fn)(int, DWORD);
    fn f = (fn)dl_export("SetAppCompatData"); REQUIRE(f);
    f(1, 0); f(2, 0);                                 /* reset the DWM-off hacks: the default state */
    DL_SKIP_IF_NO_DD7(dd);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_register_special_case_unknown_formats) {
    typedef HRESULT (WINAPI *fn)(void *, void *, void *, DWORD);
    fn f = (fn)dl_export("RegisterSpecialCase"); REQUIRE(f);
    BYTE a[32] = { 0 }, b[32] = { 0 };
    EXPECT_HR(f(a, b, NULL, 0), DDERR_NOTFOUND);       /* neither pixel format is in the conversion table */
    memset(a, 0xff, sizeof a);
    EXPECT_HR(f(a, b, NULL, 0), DDERR_NOTFOUND);
    EXPECT_HR(f(b, a, NULL, 0), DDERR_NOTFOUND);
}
TEST(ddraw_legacy_get_surface_from_dc_unknown) {
    typedef HRESULT (WINAPI *fn)(HDC, LPDIRECTDRAWSURFACE *, HDC *);
    fn f = (fn)dl_export("GetSurfaceFromDC"); REQUIRE(f);
    HDC dc = CreateCompatibleDC(NULL);
    LPDIRECTDRAWSURFACE s = (LPDIRECTDRAWSURFACE)(ULONG_PTR)0x55; HDC h = (HDC)(ULONG_PTR)0x66;
    HRESULT hr = f(dc, &s, &h);
    EXPECT_HR(hr, DDERR_NOTFOUND);
    EXPECT_EQ(s, 0);                                  /* outputs are cleared before the lookup */
    EXPECT_EQ(h, 0);
    s = (LPDIRECTDRAWSURFACE)(ULONG_PTR)0x55; h = (HDC)(ULONG_PTR)0x66;
    hr = f(NULL, &s, &h);
    EXPECT_HR(hr, DDERR_NOTFOUND);
    EXPECT_EQ(s, 0);
    DeleteDC(dc);
}
TEST(ddraw_legacy_get_surface_from_dc_roundtrip) {
    DL_SKIP_IF_NO_DD7(dd);
    HWND w = dl_window();
    if (FAILED(IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL))) { t_skip("no coop level"); IDirectDraw7_Release(dd); dl_window_free(w); return; }
    IDirectDrawSurface7 *s = dl_offscreen7(dd, 8, 8, DDSCAPS_SYSTEMMEMORY, NULL);
    HDC dc = NULL;
    if (s && SUCCEEDED(IDirectDrawSurface7_GetDC(s, &dc))) {
        IDirectDrawSurface7 *back = NULL;
        HRESULT hr = IDirectDraw7_GetSurfaceFromDC(dd, dc, &back);
        EXPECT_HR(hr, DD_OK);
        if (SUCCEEDED(hr) && back) {
            CHECK(back == s);                         /* the DC maps back to the surface that produced it */
            IDirectDrawSurface7_Release(back);
        }
        IDirectDrawSurface7_ReleaseDC(s, dc);
        back = NULL;
        hr = IDirectDraw7_GetSurfaceFromDC(dd, dc, &back);
        EXPECT_HR(hr, DDERR_NOTFOUND);                /* the mapping goes away with ReleaseDC */
        if (back) IDirectDrawSurface7_Release(back);
    } else t_skip("no surface DC");
    if (s) IDirectDrawSurface7_Release(s);
    IDirectDraw7_Release(dd);
    dl_window_free(w);
}
TEST(ddraw_legacy_internal_surface_helpers_resolve_only) {
    /* these take internal DDRAW structures: resolve, never call */
    const char *n[] = { "DDInternalLock", "DDInternalUnlock", "DDGetAttachedSurfaceLcl", "GetDDSurfaceLocal",
                        "CompleteCreateSysmemSurface", "DSoundHelp" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECKF(dl_export(n[i]) != NULL, "%s", n[i]);
}
