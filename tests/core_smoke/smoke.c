#include "d3dh.h"
/* Smallest possible corpus test: load, resolve, invoke, check. Replace/remove after scaffolding. */
TEST(smoke_module_loads) { CHECK(t_module("d3d11.dll") != NULL); }
TEST(smoke_getprocaddress_negative) {
    /* Valid negative tests: documented/decomp-verified failures an implementor must reproduce. */
    SET_ERR(0);
    FARPROC p = GetProcAddress(t_module("kernel32.dll"), "NoSuchExport_xyz");
    CHECK(p == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
CHILD(echo) { return argc > 1 ? atoi(argv[1]) : 7; }
TEST(smoke_child_roundtrip) { EXPECT_EQ(t_run_child("echo", "42", 20000), 42); }

/* ---- harness self-checks: the corpus' own assertions must behave on the platform that is the authority ---- */

/* EXPECT_EQ / EXPECT_NE / EXPECT_STR evaluate their operands exactly once (they are used with Release() calls). */
static int g_evals;
static int bump(int v) { g_evals++; return v; }
static const char *bump_s(const char *s) { g_evals++; return s; }
TEST(smoke_expect_macros_evaluate_once)
{
    g_evals = 0; EXPECT_EQ(bump(3), bump(3)); CHECK(g_evals == 2);
    g_evals = 0; EXPECT_NE(bump(3), bump(4)); CHECK(g_evals == 2);
    g_evals = 0; EXPECT_STR(bump_s("a"), bump_s("a")); CHECK(g_evals == 2);
}

static void av_write(void *ctx) { *(volatile int *)ctx = 1; }
static void no_fault(void *ctx) { *(int *)ctx = 5; }
TEST(smoke_try_reports_access_violation)
{
    int v = 0;
    EXPECT_EQ(t_try(no_fault, &v), 0); EXPECT_EQ(v, 5);
    EXPECT_EQ(t_try(av_write, NULL), 0xC0000005u);
    EXPECT_EQ(t_try(av_write, (void *)(ULONG_PTR)8), 0xC0000005u);
    EXPECT_EQ(t_try(no_fault, &v), 0);   /* the handler leaves the process usable */
}

CHILD(crash_av) { *(volatile int *)0 = 1; return 0; }
CHILD(exit_code) { return (int)0x80070057; }
CHILD(sleeper) { Sleep(60000); return 0; }
TEST(smoke_child_crash_and_exit_codes)
{
    EXPECT_EQ(t_run_child("crash_av", "", 20000), 0xC0000005u);
    EXPECT_EQ(t_run_child("exit_code", "", 20000), 0x80070057u);
    EXPECT_EQ(t_run_child("echo", "0", 20000), 0);
    EXPECT_EQ(t_run_child("echo", "255", 20000), 255);
    EXPECT_EQ(t_run_child("sleeper", "", 300), 0xDEAD0001u);          /* timeout: the helper kills the child */
    EXPECT_EQ(t_run_child("no_such_child", "", 20000), 0xC0DE0001u);   /* unknown child name: the harness's own exit code */
}

TEST(smoke_list_option)
{
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE r = NULL, w = NULL; PROCESS_INFORMATION pi; STARTUPINFOA si; char cmd[MAX_PATH + 16], buf[16384]; DWORD got = 0, total = 0;
    CHECK(CreatePipe(&r, &w, &sa, 0));
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = w; si.hStdError = w; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    snprintf(cmd, sizeof cmd, "\"%s\" --list", t_self_path());
    CHECK(CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi));
    CloseHandle(w);
    while (total < sizeof buf - 1 && ReadFile(r, buf + total, sizeof buf - 1 - total, &got, NULL) && got) total += got;
    buf[total] = 0;
    WaitForSingleObject(pi.hProcess, 20000);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(r);
    CHECK(strstr(buf, "smoke_list_option\n") != NULL || strstr(buf, "smoke_list_option\r\n") != NULL);
    CHECK(strstr(buf, "smoke_module_loads") != NULL);
    CHECK(strstr(buf, "RUN ") == NULL);   /* --list runs nothing */
}

/* ---- platform facts the whole corpus relies on ---- */
typedef LONG (WINAPI *rtlgetversion_t)(OSVERSIONINFOEXW *);
TEST(smoke_platform_is_windows10)
{
    OSVERSIONINFOEXW v; rtlgetversion_t f = (rtlgetversion_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    REQUIRE(f);
    memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    EXPECT_EQ(f(&v), 0);
    EXPECT_EQ(v.dwMajorVersion, 10); EXPECT_EQ(v.dwMinorVersion, 0);
    CHECKF(v.dwBuildNumber >= 10240, "build %lu", v.dwBuildNumber);
    t_note("Windows %lu.%lu build %lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
    CHECK(!t_is_wine());
    CHECK(sizeof(void *) == 8);
}

TEST(smoke_graphics_modules_load)
{
    static const char *const mods[] = { "dxgi.dll", "d3d9.dll", "d3d10.dll", "d3d10_1.dll", "d3d10core.dll", "d3d10warp.dll", "d3d11.dll", "d3d12.dll",
        "d3dcompiler_47.dll", "ddraw.dll", "dxva2.dll", "d3d10_1core.dll", "dxcore.dll" };
    for (unsigned i = 0; i < sizeof mods / sizeof *mods; i++) {
        HMODULE m = t_module(mods[i]), m2 = t_module(mods[i]);
        CHECKF(m != NULL, "%s does not load (GetLastError %lu)", mods[i], GetLastError());
        CHECKF(m == m2, "%s: two loads gave different handles", mods[i]);
    }
    SET_ERR(0);
    CHECK(LoadLibraryA("d3d_no_such_module.dll") == NULL); EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}

TEST(smoke_getprocaddress_ordinal_and_case)
{
    HMODULE k = t_module("kernel32.dll");
    SET_ERR(0);
    CHECK(GetProcAddress(k, "getlasterror") == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);   /* names are case sensitive */
    CHECK(GetProcAddress(k, "GetLastError") != NULL);
    SET_ERR(0);
    CHECK(GetProcAddress(k, (LPCSTR)(ULONG_PTR)0xFFFF) == NULL); EXPECT_ERR(ERROR_INVALID_ORDINAL);
    CHECK(GetProcAddress(t_module("d3d11.dll"), "D3D11CreateDevice") == t_proc_in("d3d11.dll", "D3D11CreateDevice"));
}

TEST(smoke_scratch_dir_and_files)
{
    char f[MAX_PATH]; DWORD a;
    const char *d = t_tmpdir();
    CHECK(d && d[0] && d[strlen(d) - 1] == '\\');
    a = GetFileAttributesA(d); CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    t_tmpfile(f, sizeof f, "leaf.bin");
    CHECK(strstr(f, d) == f);
    CHECK(strlen(f) > strlen(d));
}

TEST(smoke_window_and_desktop_environment)
{
    DWORD sid = 0; HWND w;
    ProcessIdToSessionId(GetCurrentProcessId(), &sid);
    w = d3dh_create_hidden_window_sized(64, 48);
    CHECKF(w != NULL, "cannot create a top-level window (GetLastError %lu)", GetLastError());
    if (w) {
        RECT r; GetClientRect(w, &r);
        CHECK(r.right == 64 && r.bottom == 48);
        CHECK(!IsWindowVisible(w));
        CHECK(GetParent(w) == NULL);     /* a real top-level window, not the message-only fallback */
        d3dh_destroy_window(w);
    }
    t_note("session %lu, primary monitor %dx%d, monitors %d", sid, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), GetSystemMetrics(SM_CMONITORS));
}
