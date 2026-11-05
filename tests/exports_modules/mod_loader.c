/* Loader behaviour on the D3D module family: handle lookups, GetModuleHandleEx, GetModuleFileName buffer
 * protocol, data-file loads, reference counts, pinning, static dependencies. */
#include "xm.h"

#define NEED(i) HMODULE h = xm_load(i); do { if (!h) { t_skip("%s not present", g_xm[i].dll); return; } } while (0)
#define ORD(o) ((LPCSTR)(ULONG_PTR)(o))

static void widen(const char *s, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, s, -1, w, cap); }
static void stem(const char *dll, char *out, size_t cap) { snprintf(out, cap, "%s", dll); char *d = strrchr(out, '.'); if (d) *d = 0; }

static void p_lookup(int i) {
    NEED(i);
    WCHAR w[128], up[128], full[MAX_PATH * 2];
    widen(g_xm[i].dll, w, 128);
    EXPECT_EQ(GetModuleHandleW(w), h);
    EXPECT_EQ(GetModuleHandleA(g_xm[i].dll), h);
    wcscpy(up, w); CharUpperW(up);
    XC(GetModuleHandleW(up) == h, "upper-case name resolves");
    char s[128]; WCHAR ws[128]; stem(g_xm[i].dll, s, sizeof s); widen(s, ws, 128);
    XC(GetModuleHandleW(ws) == h, "name without extension resolves (.dll appended)");
    char sa[128]; snprintf(sa, sizeof sa, "%s.", s);
    WCHAR wd[128]; widen(sa, wd, 128);
    SetLastError(0);
    XC(GetModuleHandleW(wd) == NULL, "trailing dot means no extension");
    XERR(ERROR_MOD_NOT_FOUND);
    GetModuleFileNameW(h, full, MAX_PATH * 2);
    XC(GetModuleHandleW(full) == h, "full path resolves");
    SetLastError(0);
    XC(GetModuleHandleA("") == NULL, "empty name");
    { DWORD e = GetLastError(); t_note("GetModuleHandle(\"\") last error %lu", e); }
}
XM_FOR_ALL(handle_lookup_forms, p_lookup)

static void p_handle_ex(int i) {
    NEED(i);
    HMODULE m = (HMODULE)(ULONG_PTR)1;
    WCHAR w[128]; widen(g_xm[i].dll, w, 128);
    BOOL ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, w, &m);
    XC(ok && m == h, "UNCHANGED_REFCOUNT by name");
    /* by address */
    FARPROC p = NULL;
    for (int k = 0; k < g_xm[i].n && !p; k++) if (g_xm[i].e[k].name && !g_xm[i].e[k].fwd) p = GetProcAddress(h, g_xm[i].e[k].name);
    if (p) {
        m = NULL;
        ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)p, &m);
        XC(ok && m == h, "FROM_ADDRESS inside the image gives the image base");
    }
    /* an address that belongs to no module */
    m = (HMODULE)(ULONG_PTR)1;
    char stackv; SetLastError(0);
    ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&stackv, &m);
    XC(!ok && m == NULL, "stack address is in no module; output zeroed");
    XERR(ERROR_MOD_NOT_FOUND);
    /* invalid flag combinations fail with ERROR_INVALID_PARAMETER and still zero the output */
    static const DWORD bad[] = { 3, 8, 0x10, 0x80, 0x100, 0x80000000u };
    for (unsigned b = 0; b < sizeof bad / sizeof bad[0]; b++) {
        m = (HMODULE)(ULONG_PTR)1; SetLastError(0);
        ok = GetModuleHandleExW(bad[b], w, &m);
        XC(!ok && m == NULL, "flags 0x%lx rejected", bad[b]);
        XERR(ERROR_INVALID_PARAMETER);
    }
    m = (HMODULE)(ULONG_PTR)1; SetLastError(0);
    ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, NULL, &m);
    XC(!ok && m == NULL, "FROM_ADDRESS with NULL");
    XERR(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, w, NULL);
    XC(!ok, "NULL phModule");
    XERR(ERROR_INVALID_PARAMETER);
    /* NULL name: the process image */
    m = NULL;
    ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL, &m);
    XC(ok && m == GetModuleHandleW(NULL), "NULL name is the exe");
    /* not-loaded module */
    m = (HMODULE)(ULONG_PTR)1; SetLastError(0);
    ok = GetModuleHandleExW(0, L"no_such_module_for_d3d.dll", &m);
    XC(!ok && m == NULL, "unknown module");
    XERR(ERROR_MOD_NOT_FOUND);
}
XM_FOR_ALL(getmodulehandleex_matrix, p_handle_ex)

static void p_fname(int i) {
    NEED(i);
    WCHAR full[MAX_PATH * 2], buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(h, full, MAX_PATH * 2);
    XC(n > 8 && n < MAX_PATH * 2, "length");
    XC(full[1] == L':' || full[0] == L'\\', "absolute path, no \\\\?\\ prefix: %ls", full);
    XC(n == lstrlenW(full), "return value is the length without the terminator");
    XC(GetModuleFileNameW(h, full, MAX_PATH * 2) == n, "repeatable");
    /* nSize == 0 */
    SetLastError(0);
    XC(GetModuleFileNameW(h, buf, 0) == 0, "nSize 0 returns 0");
    XERR(ERROR_INSUFFICIENT_BUFFER);
    /* too small: truncated, NUL-terminated, returns nSize, ERROR_INSUFFICIENT_BUFFER */
    static const DWORD sizes[] = { 1, 2, 5, 11 };
    for (unsigned k = 0; k < 4; k++) {
        DWORD sz = sizes[k];
        if (sz >= n + 1) continue;
        for (int c = 0; c < 20; c++) buf[c] = 0x7777;
        SetLastError(0);
        DWORD r = GetModuleFileNameW(h, buf, sz);
        XC(r == sz, "nSize %lu: returned %lu", sz, r);
        XERR(ERROR_INSUFFICIENT_BUFFER);
        XC(buf[sz - 1] == 0, "nSize %lu: NUL terminated", sz);
        XC(sz < 2 || !wcsncmp(buf, full, sz - 1), "nSize %lu: prefix kept", sz);
        XC(buf[sz] == 0x7777, "nSize %lu: no write past the buffer", sz);
    }
    /* exact-1 (truncated by one) and exact (n+1 fits) */
    for (int c = 0; c < 20; c++) buf[c] = 0;
    SetLastError(0);
    DWORD r = GetModuleFileNameW(h, buf, n);
    XC(r == n, "nSize == length: truncated, returns nSize (%lu)", r);
    XERR(ERROR_INSUFFICIENT_BUFFER);
    XC(buf[n - 1] == 0, "terminator in last slot");
    SetLastError(0);
    r = GetModuleFileNameW(h, buf, n + 1);
    XC(r == n, "nSize == length+1 fits (%lu)", r);
    XERR(0);
    XC(!wcscmp(buf, full), "full string");
    /* A variant */
    char a[MAX_PATH * 2], b[16];
    DWORD na = GetModuleFileNameA(h, a, sizeof a);
    XC(na > 0 && na == (DWORD)strlen(a), "A length");
    SetLastError(0);
    r = GetModuleFileNameA(h, b, sizeof b);
    if (na >= sizeof b) { XC(r == sizeof b, "A truncation returns nSize"); XERR(ERROR_INSUFFICIENT_BUFFER); }
    SetLastError(0);
    XC(GetModuleFileNameA(h, b, 0) == 0, "A nSize 0");
    /* a misaligned handle is not a module */
    SetLastError(0);
    XC(GetModuleFileNameW((HMODULE)((char *)h + 1), buf, 100) == 0, "handle with low bit set");
    XERR(ERROR_MOD_NOT_FOUND);
    XC(GetModuleFileNameW((HMODULE)((char *)h + 2), buf, 100) == 0, "handle with bit 1 set");
    XERR(ERROR_MOD_NOT_FOUND);
}
XM_FOR_ALL(getmodulefilename_buffer_protocol, p_fname)

static void p_datafile(int i) {
    NEED(i);
    WCHAR full[MAX_PATH * 2];
    GetModuleFileNameW(h, full, MAX_PATH * 2);
    HMODULE d = LoadLibraryExW(full, NULL, LOAD_LIBRARY_AS_DATAFILE);
    /* the image is already loaded normally: a data-file load of the same file still yields a data-file handle */
    XC(d != NULL, "data-file load");
    if (!d) return;
    XC(((ULONG_PTR)d & 1) == 1, "AS_DATAFILE handle has bit 0 set (%p)", d);
    XC(d != h, "distinct from the image handle");
    HRSRC r = FindResourceW(d, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16));
    XC(r != NULL, "version resource id 1 present");
    if (r) { XC(SizeofResource(d, r) > 0x40, "version resource size"); XC(LoadResource(d, r) != NULL, "LoadResource"); }
    SetLastError(0);
    XC(GetProcAddress(d, g_xm[i].e[0].name ? g_xm[i].e[0].name : "x") == NULL, "GetProcAddress on a data-file handle fails");
    XC(FreeLibrary(d), "FreeLibrary(data file)");
    HMODULE r2 = LoadLibraryExW(full, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    XC(r2 != NULL, "image-resource load");
    if (r2) {
        XC(((ULONG_PTR)r2 & 3) != 0, "AS_IMAGE_RESOURCE handle is tagged (%p)", r2);
        if (((ULONG_PTR)r2 & 3) != 2) t_note("AS_IMAGE_RESOURCE handle tag is %lu", (unsigned long)((ULONG_PTR)r2 & 3));
        XC(FindResourceW(r2, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16)) != NULL, "version resource via image mapping");
        XC(FreeLibrary(r2), "FreeLibrary(image resource)");
    }
}
XM_FOR_ALL(datafile_load, p_datafile)

/* ---- child-process scenarios (clean module list) ---------------------------------------------- */
static int idx_arg(int argc, char **argv) { return argc > 1 ? atoi(argv[1]) : -1; }

CHILD(xm_refcount) {
    int i = idx_arg(argc, argv); if (i < 0 || i >= XM_COUNT) return 90;
    const char *n = g_xm[i].dll;
    if (GetModuleHandleA(n)) return 91;                    /* fresh process: not loaded */
    HMODULE a = LoadLibraryA(n);
    if (!a) return 77;                                       /* absent: skip */
    HMODULE b = LoadLibraryA(n), c = LoadLibraryA(n);
    if (a != b || b != c) return 1;
    if (!FreeLibrary(c)) return 2;
    if (!FreeLibrary(b)) return 3;
    if (GetModuleHandleA(n) != a) return 4;                  /* still loaded: one reference left */
    HMODULE x = NULL;
    if (!GetModuleHandleExA(0, n, &x) || x != a) return 5;   /* adds a reference */
    if (!FreeLibrary(a)) return 6;
    if (GetModuleHandleA(n) != a) return 7;                  /* the Ex reference keeps it */
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, n, &x) || x != a) return 8;
    if (!FreeLibrary(a)) return 9;                           /* last reference */
    SetLastError(0);
    if (GetModuleHandleA(n) != NULL) return 10;              /* stayed resident (a module may pin itself) */
    if (GetLastError() != ERROR_MOD_NOT_FOUND) return 11;
    return 0;
}
CHILD(xm_pin) {
    int i = idx_arg(argc, argv); if (i < 0 || i >= XM_COUNT) return 90;
    const char *n = g_xm[i].dll;
    HMODULE a = LoadLibraryA(n);
    if (!a) return 77;
    HMODULE x = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, n, &x) || x != a) return 1;
    if (!FreeLibrary(a)) return 2;                           /* FreeLibrary still succeeds on a pinned module */
    if (GetModuleHandleA(n) != a) return 3;                  /* but it stays resident */
    for (int k = 0; k < 4; k++) if (!FreeLibrary(a)) return 4;
    if (GetModuleHandleA(n) != a) return 5;
    return 0;
}
CHILD(xm_deps) {
    int i = idx_arg(argc, argv); if (i < 0 || i >= XM_COUNT) return 90;
    if (!LoadLibraryA(g_xm[i].dll)) return 77;
    char list[512]; snprintf(list, sizeof list, "%s", g_xm[i].deps);
    int k = 0;
    int soft = 0;
    for (char *tok = strtok(list, ","); tok; tok = strtok(NULL, ","), k++)
        if (!GetModuleHandleA(tok)) {
            if (!strncmp(tok, "d3d", 3) || !strncmp(tok, "dxgi", 4) || !strncmp(tok, "dxil", 4)) return 100 + k;
            soft = 200 + k;
        }
    return soft;
}
CHILD(xm_datafile_hidden) {
    int i = idx_arg(argc, argv); if (i < 0 || i >= XM_COUNT) return 90;
    if (GetModuleHandleA(g_xm[i].dll)) return 91;
    HMODULE d = LoadLibraryExA(g_xm[i].dll, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!d) return 77;
    SetLastError(0);
    if (GetModuleHandleA(g_xm[i].dll) != NULL) return 1;     /* data-file mappings are not in the loader list */
    if (GetLastError() != ERROR_MOD_NOT_FOUND) return 2;
    if (!FreeLibrary(d)) return 3;
    return 0;
}
CHILD(xm_no_resolve) {
    int i = idx_arg(argc, argv); if (i < 0 || i >= XM_COUNT) return 90;
    HMODULE m = LoadLibraryExA(g_xm[i].dll, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!m) return 77;
    if (GetModuleHandleA(g_xm[i].dll) != m) return 1;
    char list[512]; snprintf(list, sizeof list, "%s", g_xm[i].deps);
    for (char *tok = strtok(list, ","); tok; tok = strtok(NULL, ","))
        if ((!strncmp(tok, "d3d", 3) || !strncmp(tok, "dxgi", 4) || !strncmp(tok, "dxil", 4)) && GetModuleHandleA(tok)) return 2;
    int named = 0;
    for (int k = 0; k < g_xm[i].n; k++) if (g_xm[i].e[k].name && !g_xm[i].e[k].fwd) { named = 1; if (!GetProcAddress(m, g_xm[i].e[k].name)) return 3; break; }
    (void)named;
    if (!FreeLibrary(m)) return 4;
    if (GetModuleHandleA(g_xm[i].dll) != NULL) return 5;
    return 0;
}
CHILD(xm_dep_unload) {
    /* d3d10.dll statically imports d3d10core.dll: both are loaded by one LoadLibrary and the import is released with it */
    HMODULE m = LoadLibraryA("d3d10.dll");
    if (!m) return 77;
    if (!GetModuleHandleA("d3d10core.dll")) return 1;
    if (!GetModuleHandleA("d3d11.dll")) return 2;
    if (!FreeLibrary(m)) return 3;
    if (GetModuleHandleA("d3d10.dll")) return 4;
    return GetModuleHandleA("d3d10core.dll") ? 5 : 0;
}

static void run_child_prop(const char *child, int i, const char *what) {
    HMODULE probe = LoadLibraryExA(g_xm[i].dll, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!probe) { t_skip("%s not present", g_xm[i].dll); return; }
    FreeLibrary(probe);
    char a[16]; snprintf(a, sizeof a, "%d", i);
    DWORD rc = t_run_child(child, a, 20000);
    if (rc == 77) { t_skip("%s cannot be loaded in a plain process", g_xm[i].dll); return; }
    if (rc == 91) { t_skip("%s is already loaded in a fresh process (injected)", g_xm[i].dll); return; }
    if (rc == 10 && !strcmp(child, "xm_refcount")) { t_note("%s stays resident after its last FreeLibrary", g_xm[i].dll); return; }
    if (xm_wine() && rc != 0) { t_note("emulator: %s child %s returned %lu", g_xm[i].dll, what, rc); return; }
    CHECKF(rc == 0, "%s: child step %lu failed (%s)", g_xm[i].dll, rc, what);
}
#define CHILD_PROP(prop, child) \
    static void p_##prop(int i) { run_child_prop(child, i, #prop); } XM_FOR_ALL(prop, p_##prop)
CHILD_PROP(refcount_lifetime, "xm_refcount")
CHILD_PROP(pin_keeps_resident, "xm_pin")
CHILD_PROP(datafile_not_in_module_list, "xm_datafile_hidden")

static void p_deps(int i) {
    HMODULE probe = LoadLibraryExA(g_xm[i].dll, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!probe) { t_skip("%s not present", g_xm[i].dll); return; }
    FreeLibrary(probe);
    char a[16]; snprintf(a, sizeof a, "%d", i);
    DWORD rc = t_run_child("xm_deps", a, 20000);
    if (rc == 77) { t_skip("cannot load"); return; }
    if (rc >= 200 && rc < 230) {
        char list[512], *tok; int k = 0; snprintf(list, sizeof list, "%s", g_xm[i].deps);
        for (tok = strtok(list, ","); tok && k < (int)rc - 200; tok = strtok(NULL, ","), k++) ;
        t_note("%s: reference-build dependency %s was not loaded", g_xm[i].dll, tok ? tok : "?");
        return;
    }
    if (rc >= 100 && rc < 130) {
        char list[512], *tok; int k = 0; snprintf(list, sizeof list, "%s", g_xm[i].deps);
        for (tok = strtok(list, ","); tok && k < (int)rc - 100; tok = strtok(NULL, ","), k++) ;
        if (xm_wine()) t_note("emulator: static dependency %s not loaded", tok ? tok : "?");
        else CHECKF(0, "%s: static dependency %s not loaded after LoadLibrary", g_xm[i].dll, tok ? tok : "?");
        return;
    }
    if (xm_wine() && rc) { t_note("emulator: deps child %lu", rc); return; }
    EXPECT_EQ(rc, 0);
}
XM_FOR_ALL(static_dependencies_loaded, p_deps)

CHILD_PROP(dont_resolve_references, "xm_no_resolve")
TEST(exports_modules_free_releases_static_imports) {
    DWORD rc = t_run_child("xm_dep_unload", "", 20000);
    if (rc == 77) { t_skip("d3d10.dll missing"); return; }
    if (xm_wine() && rc) { t_note("emulator: child %lu", rc); return; }
    CHECKF(rc == 0 || rc == 5, "child step %lu", rc);
    if (rc == 5) t_note("d3d10core.dll stayed loaded after d3d10.dll was freed");
}

/* ---- global argument validation -------------------------------------------------------------- */
TEST(exports_modules_loadlibrary_invalid_arguments) {
    xm_begin();
    SetLastError(0); XCK(LoadLibraryW(NULL) == NULL); XERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); XCK(LoadLibraryA(NULL) == NULL); XERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); XCK(LoadLibraryW(L"") == NULL); XERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); XCK(LoadLibraryW(L"   ") == NULL); XERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); XCK(LoadLibraryExW(L"d3d11.dll", (HANDLE)(ULONG_PTR)4, 0) == NULL); XERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); XCK(LoadLibraryExW(L"d3d11.dll", NULL, 0x10000) == NULL); XERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); XCK(LoadLibraryExW(L"d3d11.dll", NULL, 0x80000000u) == NULL); XERR(ERROR_INVALID_PARAMETER);
    /* AS_DATAFILE (0x2) together with AS_DATAFILE_EXCLUSIVE (0x40) is rejected */
    SetLastError(0); XCK(LoadLibraryExW(L"d3d11.dll", NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) == NULL); XERR(ERROR_INVALID_PARAMETER);
    xm_end();
}
TEST(exports_modules_loadlibrary_missing_modules) {
    static const char *const bad[] = { "d3d11_does_not_exist.dll", "dxgi_nonexistent_module", "d3d12core_x.dll", "nonexistent_directx.dll" };
    for (int k = 0; k < 4; k++) {
        SetLastError(0);
        CHECKF(LoadLibraryA(bad[k]) == NULL, "%s", bad[k]);
        XERR(ERROR_MOD_NOT_FOUND);
    }
    SetLastError(0);
    HMODULE m = LoadLibraryA("C:\\no_such_dir_d3d\\d3d11.dll");
    CHECK(m == NULL);
    { DWORD e = GetLastError(); CHECKF(e == ERROR_MOD_NOT_FOUND || e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "last error %lu", e); t_note("absolute missing path last error %lu", e); }
}
TEST(exports_modules_loadlibrary_not_a_dll) {
    /* a file that exists but is not a PE image: ERROR_BAD_EXE_FORMAT (193) */
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "notadll.dll");
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD w; char junk[4096]; memset(junk, 0x41, sizeof junk);
    WriteFile(f, junk, sizeof junk, &w, NULL); CloseHandle(f);
    SetLastError(0);
    CHECK(LoadLibraryA(p) == NULL);
    XERR(ERROR_BAD_EXE_FORMAT);
    SetLastError(0);
    CHECK(LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE) == NULL);
    { DWORD e = GetLastError(); t_note("data-file load of a non-PE file last error %lu", e); }
    DeleteFileA(p);
}
TEST(exports_modules_getmodulehandle_exe) {
    HMODULE exe = GetModuleHandleW(NULL);
    CHECK(exe != NULL);
    EXPECT_EQ(GetModuleHandleA(NULL), exe);
    char p[MAX_PATH]; GetModuleFileNameA(NULL, p, sizeof p);
    EXPECT_EQ(GetModuleHandleA(p), exe);
    CHECK(xm_nt(exe) != NULL);
    SetLastError(0);
    CHECK(GetModuleHandleW(L"no_such_module_xyz.dll") == NULL);
    XERR(ERROR_MOD_NOT_FOUND);
}
TEST(exports_modules_freelibrary_invalid) {
    SetLastError(0);
    CHECK(FreeLibrary(NULL) == FALSE);
    { DWORD e = GetLastError(); CHECK(e != 0); t_note("FreeLibrary(NULL) last error %lu", e); }
    SetLastError(0);
    CHECK(FreeLibrary((HMODULE)(ULONG_PTR)0x10000) == FALSE);
    { DWORD e = GetLastError(); CHECK(e != 0); }
}
TEST(exports_modules_getmodulefilename_exe_and_null) {
    char a[MAX_PATH], b[MAX_PATH];
    DWORD n1 = GetModuleFileNameA(NULL, a, sizeof a);
    DWORD n2 = GetModuleFileNameA(GetModuleHandleA(NULL), b, sizeof b);
    CHECK(n1 > 0 && n1 == n2);
    EXPECT_STR(a, b);
    SetLastError(0);
    CHECK(GetModuleFileNameA((HMODULE)(ULONG_PTR)0x10000, a, sizeof a) == 0);
    XERR(ERROR_MOD_NOT_FOUND);
}

/* ---- optional / absent DLLs --------------------------------------------------------------- */
static const char *const kOptional[] = { "d3d8.dll", "d3dx9_43.dll", "d3dx10_43.dll", "d3dx11_43.dll", "d3dcompiler_43.dll",
                                         "d3dcompiler_47.dll", "opengl32.dll", "d3d11_1sdklayers.dll", "d3d11sdklayers.dll",
                                         "d3d12sdklayers.dll", "dxgidebug.dll", "d3d10sdklayers.dll", "D3DX9_42.dll" };
TEST(exports_modules_optional_dlls_presence) {
    for (unsigned k = 0; k < sizeof kOptional / sizeof kOptional[0]; k++) {
        HMODULE m = LoadLibraryExA(kOptional[k], NULL, LOAD_LIBRARY_AS_DATAFILE);
        DWORD e = GetLastError();
        t_note("%s: %s%s", kOptional[k], m ? "present" : "absent", m ? "" : (e == ERROR_MOD_NOT_FOUND ? " (126)" : " (other error)"));
        if (m) FreeLibrary(m);
    }
}
TEST(exports_modules_inbox_graphics_dlls_present) {
    /* d3dcompiler_47 and opengl32 ship with every Windows 10 */
    static const char *const must[] = { "d3dcompiler_47.dll", "opengl32.dll", "d3d11.dll", "dxgi.dll", "d3d9.dll", "ddraw.dll", "dxva2.dll" };
    for (int k = 0; k < 7; k++) {
        HMODULE m = LoadLibraryExA(must[k], NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (xm_wine() && !m) { t_note("emulator: %s absent", must[k]); continue; }
        CHECKF(m != NULL, "%s should ship with Windows 10", must[k]);
        if (m) FreeLibrary(m);
    }
}
TEST(exports_modules_d3dx_absent_from_inbox) {
    /* the D3DX redistributables are not part of the OS image; presence only depends on installed runtimes */
    static const char *const x[] = { "d3dx9_43.dll", "d3dx10_43.dll", "d3dx11_43.dll", "d3dcompiler_43.dll" };
    int present = 0;
    for (int k = 0; k < 4; k++) {
        char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\"); strcat(p, x[k]);
        DWORD a = GetFileAttributesA(p);
        if (a != INVALID_FILE_ATTRIBUTES) { present++; t_note("%s present in System32", x[k]); }
    }
    t_note("%d D3DX redistributable(s) installed", present);
}
