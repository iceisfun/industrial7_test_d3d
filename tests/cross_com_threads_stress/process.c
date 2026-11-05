/* Process-level behaviour: every DLL loads and sits in System32, device creation works in detached/no-console
 * processes, with odd environments and no window, processes exit cleanly with live devices, DLLs can be
 * loaded/freed repeatedly, DXGI output rectangles under each DPI awareness mode. All checks that need isolation
 * run in a child of this executable. */
#include "cts.h"

#define SKIPCODE 0x4000

static WCHAR *widen(const char *s, WCHAR *out, int cap) { MultiByteToWideChar(CP_ACP, 0, s, -1, out, cap); return out; }

typedef HRESULT (WINAPI *create11_fn)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
                                      ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);

/* Create a WARP device with raw LoadLibrary/GetProcAddress (children run without the harness module cache). */
static HRESULT child_make11(ID3D11Device **d, ID3D11DeviceContext **c, D3D_FEATURE_LEVEL *fl)
{
    HMODULE m = LoadLibraryW(L"d3d11.dll"); create11_fn f;
    if (!m) return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    f = (create11_fn)GetProcAddress(m, "D3D11CreateDevice");
    if (!f) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    return f(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, d, fl, c);
}

/* ---- running children with custom creation flags / environment ----------------------------------------- */
static DWORD run_ex(const char *name, const char *args, DWORD flags, const char *env, int nul_stdio, DWORD tmo)
{
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000; HANDLE nul = INVALID_HANDLE_VALUE, h;
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    if (nul_stdio) nul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    h = t_spawn_child(name, args, flags, nul_stdio ? nul : NULL, nul_stdio ? nul : NULL, nul_stdio ? nul : NULL, nul_stdio ? TRUE : FALSE, env, &pi);
    if (!h) { if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul); return 0xDEAD0000; }
    if (WaitForSingleObject(h, tmo) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    return code;
}
/* environment block = current environment + extra "A=B\0C=D\0" strings (double-NUL terminated) */
static char *env_with(const char *extra, size_t extra_len)
{
    char *cur = GetEnvironmentStringsA(), *p = cur, *out; size_t n;
    while (*p) p += strlen(p) + 1;
    n = (size_t)(p - cur);
    out = malloc(n + extra_len + 2);
    memcpy(out, cur, n); memcpy(out + n, extra, extra_len); out[n + extra_len] = 0; out[n + extra_len + 1] = 0;
    FreeEnvironmentStringsA(cur);
    return out;
}

/* ---- load every module in a fresh process ------------------------------------------------------------------- */
CHILD(load_one)
{
    WCHAR w[128], path[MAX_PATH], sys[MAX_PATH]; HMODULE m, m2; DWORD e;
    if (argc < 2) return 99;
    widen(argv[1], w, 128);
    m = LoadLibraryW(w);
    if (!m) { printf("  NOTE LoadLibrary(%s) error %lu\n", argv[1], GetLastError()); return 1; }
    m2 = GetModuleHandleW(w);
    if (m2 != m) return 4;
    if (argc > 2 && strcmp(argv[2], "-")) {
        if (!GetProcAddress(m, argv[2])) return 2;
    }
    e = GetModuleFileNameW(m, path, MAX_PATH);
    GetSystemDirectoryW(sys, MAX_PATH);
    if (!e || _wcsnicmp(path, sys, wcslen(sys)) != 0) { printf("  NOTE %s loaded from %ls\n", argv[1], path); return 3; }
    return FreeLibrary(m) ? 0 : 5;
}
static void load_case(const char *dll, const char *exp, int hard)
{
    char a[200]; DWORD rc;
    snprintf(a, sizeof a, "%s %s", dll, exp);
    rc = t_run_child("load_one", a, 30000);
    if (hard) { EXPECT_EQ(rc, 0); }
    else {
        t_note("%s load child result %lu (0 = loaded from System32 with export %s)", dll, rc, exp);
        CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    }
}
#define LOADT(nm, dll, exp, hard) T(process_load_##nm) { load_case(dll, exp, hard); }
LOADT(d3d11, "d3d11.dll", "D3D11CreateDevice", 1)
LOADT(dxgi, "dxgi.dll", "CreateDXGIFactory1", 1)
LOADT(d3d9, "d3d9.dll", "Direct3DCreate9", 1)
LOADT(d3d10, "d3d10.dll", "D3D10CreateDevice", 1)
LOADT(d3d10_1, "d3d10_1.dll", "D3D10CreateDevice1", 1)
LOADT(d3d10core, "d3d10core.dll", "D3D10CoreCreateDevice", 1)
LOADT(d3d12, "d3d12.dll", "D3D12CreateDevice", 1)
LOADT(d3dcompiler_47, "d3dcompiler_47.dll", "D3DCompile", 1)
LOADT(ddraw, "ddraw.dll", "DirectDrawCreate", 1)
LOADT(dxva2, "dxva2.dll", "DXVA2CreateVideoService", 1)
LOADT(d3d10_1core, "d3d10_1core.dll", "-", 0)
LOADT(d3d10warp, "d3d10warp.dll", "-", 0)
LOADT(d3d10level9, "d3d10level9.dll", "-", 0)
LOADT(d3d11on12, "d3d11on12.dll", "-", 0)
LOADT(d3d12core, "d3d12core.dll", "D3D12GetInterface", 0)
LOADT(d3d9on12, "d3d9on12.dll", "-", 0)
LOADT(dxcore, "dxcore.dll", "DXCoreCreateAdapterFactory", 0)
LOADT(d3dscache, "d3dscache.dll", "-", 0)
LOADT(d3d8thk, "d3d8thk.dll", "-", 0)
LOADT(dxilconv, "dxilconv.dll", "-", 0)

T(process_load_missing_module_error)
{
    HMODULE m; DWORD e;
    SetLastError(0);
    m = LoadLibraryW(L"d3dt_no_such_module_4711.dll");
    e = GetLastError();
    CHECK(m == NULL);
    EXPECT_EQ(e, ERROR_MOD_NOT_FOUND);
}

/* ---- device creation in unusual processes ---------------------------------------------------------------------- */
static BOOL CALLBACK count_wnd(HWND h, LPARAM l)
{
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId()) (*(int *)l)++;
    return TRUE;
}
CHILD(create11)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0; HRESULT hr; int wnd = 0; ID3D11Buffer *b = NULL;
    hr = child_make11(&d, &c, &fl);
    if (hr == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND)) return 3;
    if (FAILED(hr)) { printf("  NOTE child D3D11CreateDevice = %s\n", d3dh_hr_name(hr)); return SKIPCODE; }
    if (!d || !c || fl < D3D_FEATURE_LEVEL_9_1) return 2;
    if (FAILED(cts_make_buffer(d, 64, D3D11_BIND_VERTEX_BUFFER, &b))) return 4;
    EnumWindows(count_wnd, (LPARAM)&wnd);
    printf("  NOTE child top-level windows owned by process after device creation: %d\n", wnd);
    ID3D11Buffer_Release(b); ID3D11DeviceContext_Release(c); ID3D11Device_Release(d);
    return 0;
}
static DWORD baseline11(void) { return t_run_child("create11", "", 60000); }
#define CREATE_CASE(nm, flags, env, nul) T(process_create11_##nm) { \
    DWORD rc; REQUIRE(d3dh_D3D11CreateDevice()); \
    if (baseline11() != 0) { t_skip("baseline child cannot create a device"); return; } \
    rc = run_ex("create11", "", flags, env, nul, 60000); EXPECT_EQ(rc, 0); }
CREATE_CASE(plain, 0, NULL, 0)
CREATE_CASE(detached_process, DETACHED_PROCESS, NULL, 0)
CREATE_CASE(no_window, CREATE_NO_WINDOW, NULL, 0)
CREATE_CASE(new_process_group, CREATE_NEW_PROCESS_GROUP, NULL, 0)
CREATE_CASE(detached_with_nul_stdio, DETACHED_PROCESS, NULL, 1)
CREATE_CASE(below_normal_priority, BELOW_NORMAL_PRIORITY_CLASS, NULL, 0)
CREATE_CASE(idle_priority_no_window, IDLE_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, 0)

T(process_create11_unrelated_environment_variables)
{
    static const char extra[] = "D3DT_JUNK=1\0D3D11_NOT_A_REAL_SETTING=yes\0DXGI_NOT_A_REAL_SETTING=0\0";
    char *env; DWORD rc;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (baseline11() != 0) { t_skip("baseline child cannot create a device"); return; }
    env = env_with(extra, sizeof extra - 1);
    rc = run_ex("create11", "", 0, env, 0, 60000);
    free(env);
    EXPECT_EQ(rc, 0);
}
T(process_create11_very_long_environment_value)
{
    char *extra, *env; DWORD rc; size_t n = 20000;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (baseline11() != 0) { t_skip("baseline child cannot create a device"); return; }
    extra = malloc(n + 32);
    memcpy(extra, "D3DT_LONG=", 10); memset(extra + 10, 'x', n); extra[10 + n] = 0; extra[11 + n] = 0;
    env = env_with(extra, n + 11);
    rc = run_ex("create11", "", 0, env, 0, 60000);
    free(env); free(extra);
    EXPECT_EQ(rc, 0);
}
T(process_create11_minimal_environment_note)
{
    char sysroot[MAX_PATH], blk[1024]; size_t n = 0; DWORD rc;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (baseline11() != 0) { t_skip("baseline child cannot create a device"); return; }
    if (!GetEnvironmentVariableA("SystemRoot", sysroot, sizeof sysroot)) { t_skip("no SystemRoot"); return; }
    n += snprintf(blk + n, sizeof blk - n, "SystemRoot=%s", sysroot) + 1;
    n += snprintf(blk + n, sizeof blk - n, "windir=%s", sysroot) + 1;
    blk[n] = 0;
    rc = run_ex("create11", "", 0, blk, 0, 60000);
    t_note("device creation with only SystemRoot/windir in the environment: child result %lu", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
}

CHILD(create_dxgi)
{
    HMODULE m = LoadLibraryW(L"dxgi.dll"); HRESULT (WINAPI *cf)(REFIID, void **); IDXGIFactory1 *f = NULL; IDXGIAdapter1 *a = NULL; HRESULT hr;
    if (!m) return 3;
    cf = (void *)GetProcAddress(m, "CreateDXGIFactory1");
    if (!cf) return 3;
    hr = cf(&IID_IDXGIFactory1, (void **)&f);
    if (FAILED(hr)) { printf("  NOTE child CreateDXGIFactory1 = %s\n", d3dh_hr_name(hr)); return SKIPCODE; }
    hr = IDXGIFactory1_EnumAdapters1(f, 0, &a);
    if (hr != S_OK && hr != DXGI_ERROR_NOT_FOUND) return 2;
    if (a) IDXGIAdapter1_Release(a);
    IDXGIFactory1_Release(f);
    return 0;
}
T(process_dxgi_factory_in_detached_process)
{
    DWORD rc;
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (t_run_child("create_dxgi", "", 30000) != 0) { t_skip("baseline child cannot create a factory"); return; }
    rc = run_ex("create_dxgi", "", DETACHED_PROCESS, NULL, 1, 30000);
    EXPECT_EQ(rc, 0);
    rc = run_ex("create_dxgi", "", CREATE_NO_WINDOW, NULL, 0, 30000);
    EXPECT_EQ(rc, 0);
}

/* ---- exit paths with live objects --------------------------------------------------------------------------------- */
CHILD(exit_live)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Buffer *b[8]; HRESULT hr;
    hr = child_make11(&d, &c, NULL);
    if (FAILED(hr)) return SKIPCODE;
    for (int i = 0; i < 8; i++) cts_make_buffer(d, 1024 << i, D3D11_BIND_VERTEX_BUFFER, &b[i]);
    ID3D11DeviceContext_Flush(c);
    if (argc > 1 && !strcmp(argv[1], "terminate")) TerminateProcess(GetCurrentProcess(), 44);
    if (argc > 1 && !strcmp(argv[1], "crt")) return 43;
    ExitProcess(42);
    return 0;
}
static void exit_case(const char *how, DWORD want)
{
    DWORD rc;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (baseline11() != 0) { t_skip("baseline child cannot create a device"); return; }
    rc = t_run_child("exit_live", how, 60000);
    EXPECT_EQ(rc, want);
}
T(process_exit_process_with_live_device) { exit_case("exit", 42); }
T(process_terminate_self_with_live_device) { exit_case("terminate", 44); }
T(process_crt_return_with_live_device) { exit_case("crt", 43); }

static volatile LONG g_stop;
static DWORD WINAPI churn(LPVOID p)
{
    ID3D11Device *d = p;
    while (!g_stop) {
        ID3D11Buffer *b = NULL;
        if (SUCCEEDED(cts_make_buffer(d, 256, D3D11_BIND_VERTEX_BUFFER, &b))) ID3D11Buffer_Release(b);
    }
    return 0;
}
CHILD(exit_busy)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    if (FAILED(child_make11(&d, &c, NULL))) return SKIPCODE;
    for (int i = 0; i < 4; i++) CloseHandle(CreateThread(NULL, 0, churn, d, 0, NULL));
    Sleep(300);
    ExitProcess(77);
    return 0;
}
T(process_exit_while_threads_use_device)
{
    DWORD rc;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (baseline11() != 0) { t_skip("baseline child cannot create a device"); return; }
    rc = t_run_child("exit_busy", "", 60000);
    EXPECT_EQ(rc, 77);
}

CHILD(exit_live12)
{
    HMODULE m = LoadLibraryW(L"d3d12.dll"); HRESULT (WINAPI *cd)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
    ID3D12Device *d = NULL; ID3D12CommandQueue *q = NULL; D3D12_COMMAND_QUEUE_DESC qd;
    if (!m) return SKIPCODE;
    cd = (void *)GetProcAddress(m, "D3D12CreateDevice");
    if (!cd || FAILED(cd(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d))) return SKIPCODE;
    memset(&qd, 0, sizeof qd);
    ID3D12Device_CreateCommandQueue(d, &qd, &IID_ID3D12CommandQueue, (void **)&q);
    ExitProcess(45);
    return 0;
}
T(process_exit_process_with_live_d3d12_device)
{
    DWORD rc;
    REQUIRE(d3dh_D3D12CreateDevice());
    rc = t_run_child("exit_live12", "", 60000);
    if (rc == SKIPCODE) { t_skip("no d3d12 device"); return; }
    EXPECT_EQ(rc, 45);
}

/* ---- load / free cycles --------------------------------------------------------------------------------------------- */
typedef struct { const char *dll, *kind; } reload_arg;
static int reload_create(const char *kind, HMODULE m)
{
    if (!strcmp(kind, "d3d11")) {
        create11_fn f = (create11_fn)GetProcAddress(m, "D3D11CreateDevice"); ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
        if (!f) return 2;
        if (FAILED(f(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, NULL, &c))) return SKIPCODE;
        ID3D11DeviceContext_Release(c); ID3D11Device_Release(d);
    } else if (!strcmp(kind, "dxgi")) {
        HRESULT (WINAPI *cf)(REFIID, void **) = (void *)GetProcAddress(m, "CreateDXGIFactory1"); IDXGIFactory1 *f = NULL;
        if (!cf) return 2;
        if (FAILED(cf(&IID_IDXGIFactory1, (void **)&f))) return SKIPCODE;
        IDXGIFactory1_Release(f);
    } else if (!strcmp(kind, "d3d9")) {
        IDirect3D9 *(WINAPI *cf)(UINT) = (void *)GetProcAddress(m, "Direct3DCreate9"); IDirect3D9 *p;
        if (!cf) return 2;
        p = cf(D3D_SDK_VERSION);
        if (!p) return SKIPCODE;
        IDirect3D9_Release(p);
    } else if (!strcmp(kind, "d3d12")) {
        HRESULT (WINAPI *cd)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **) = (void *)GetProcAddress(m, "D3D12CreateDevice"); ID3D12Device *d = NULL;
        if (!cd) return 2;
        if (FAILED(cd(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d))) return SKIPCODE;
        ID3D12Device_Release(d);
    } else if (!strcmp(kind, "compiler")) {
        static const char src[] = "float4 main(float4 p : POSITION) : SV_Position { return p; }";
        HRESULT (WINAPI *cc)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **) =
            (void *)GetProcAddress(m, "D3DCompile");
        ID3DBlob *b = NULL;
        if (!cc) return 2;
        if (FAILED(cc(src, sizeof src - 1, "t", NULL, NULL, "main", "vs_4_0", 0, 0, &b, NULL))) return SKIPCODE;
        ID3D10Blob_Release(b);
    }
    return 0;
}
CHILD(reload)
{
    WCHAR w[128]; const char *kind = argc > 2 ? argv[2] : "none";
    if (argc < 2) return 99;
    widen(argv[1], w, 128);
    for (int cycle = 0; cycle < 3; cycle++) {
        HMODULE h1 = LoadLibraryW(w), h2, hq; int r;
        if (!h1) return 1;
        h2 = LoadLibraryW(w);                       /* second reference */
        if (h2 != h1) return 5;
        r = reload_create(kind, h1);
        if (r == SKIPCODE && cycle == 0) return SKIPCODE;
        if (r) return r == 2 ? 2 : 3;
        if (!FreeLibrary(h2)) return 4;
        if (!GetModuleHandleW(w)) return 7;         /* one reference left: must still be mapped */
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, w, &hq) || hq != h1) return 6;
        if (!FreeLibrary(h1)) return 4;
        printf("  NOTE %s after cycle %d: %s\n", argv[1], cycle, GetModuleHandleW(w) ? "still mapped (other references)" : "unmapped");
    }
    return 0;
}
static void reload_case(const char *dll, const char *kind)
{
    char a[200]; DWORD rc;
    snprintf(a, sizeof a, "%s %s", dll, kind);
    rc = t_run_child("reload", a, 90000);
    if (rc == SKIPCODE) { t_skip("%s: no device/object can be created here", dll); return; }
    EXPECT_EQ(rc, 0);
}
#define RELOAD(nm, dll, kind) T(process_reload_##nm) { reload_case(dll, kind); }
RELOAD(d3d11, "d3d11.dll", "d3d11")
RELOAD(dxgi, "dxgi.dll", "dxgi")
RELOAD(d3d9, "d3d9.dll", "d3d9")
RELOAD(d3d12, "d3d12.dll", "d3d12")
RELOAD(d3dcompiler_47, "d3dcompiler_47.dll", "compiler")
RELOAD(d3d10core, "d3d10core.dll", "none")
RELOAD(ddraw, "ddraw.dll", "none")
RELOAD(dxva2, "dxva2.dll", "none")
RELOAD(d3d10, "d3d10.dll", "none")

/* In-process reference bookkeeping on a DLL this test already holds. */
T(process_module_refcount_pairs)
{
    HMODULE base = GetModuleHandleW(L"d3d11.dll"), a, b;
    if (!base) { t_skip("d3d11.dll not loaded"); return; }
    a = LoadLibraryW(L"d3d11.dll"); b = LoadLibraryW(L"d3d11.dll");
    CHECK(a == base); CHECK(b == base);
    CHECK(FreeLibrary(b)); CHECK(FreeLibrary(a));
    CHECK(GetModuleHandleW(L"d3d11.dll") == base);
    { HMODULE x = NULL;
      CHECK(GetModuleHandleExW(0, L"d3d11.dll", &x)); CHECK(x == base);
      CHECK(FreeLibrary(x));
      CHECK(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"d3d11.dll", &x)); CHECK(x == base); }
    CHECK(GetModuleHandleW(L"d3d11.dll") == base);
}
T(process_module_handle_by_address)
{
    HMODULE m = GetModuleHandleW(L"d3d11.dll"), x = NULL; FARPROC p;
    if (!m) { t_skip("d3d11.dll not loaded"); return; }
    p = GetProcAddress(m, "D3D11CreateDevice");
    REQUIRE(p);
    CHECK(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)p, &x));
    CHECK(x == m);
}
T(process_module_file_name_in_system_directory)
{
    static const char *dlls[] = { "d3d11.dll", "dxgi.dll", "d3d12.dll" };
    WCHAR sys[MAX_PATH], path[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
    for (int i = 0; i < 3; i++) {
        WCHAR w[64]; HMODULE m = GetModuleHandleW(widen(dlls[i], w, 64));
        if (!m) continue;
        GetModuleFileNameW(m, path, MAX_PATH);
        CHECKF(!_wcsnicmp(path, sys, wcslen(sys)), "%s at %ls", dlls[i], path);
    }
}

/* ---- DPI awareness and DXGI output rectangles --------------------------------------------------------------------------- */
CHILD(dpi)
{
    int mode = argc > 1 ? atoi(argv[1]) : 0; HMODULE dx, u32 = LoadLibraryW(L"user32.dll");
    HRESULT (WINAPI *cf)(REFIID, void **); IDXGIFactory1 *f = NULL; IDXGIAdapter1 *a = NULL; IDXGIOutput *o = NULL; DXGI_OUTPUT_DESC od;
    MONITORINFOEXW mi; int found = 0, rc = 0; UINT i;
    if (mode == 1) { BOOL (WINAPI *p)(void) = (void *)GetProcAddress(u32, "SetProcessDPIAware"); if (!p || !p()) return 6; }
    if (mode == 2) {
        BOOL (WINAPI *p)(HANDLE) = (void *)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        if (!p) return 6;
        if (!p((HANDLE)(LONG_PTR)-4)) return 6;
    }
    dx = LoadLibraryW(L"dxgi.dll");
    if (!dx) return 6;
    cf = (void *)GetProcAddress(dx, "CreateDXGIFactory1");
    if (!cf || FAILED(cf(&IID_IDXGIFactory1, (void **)&f))) return 4;
    for (i = 0; !found && IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK; i++) {
        if (IDXGIAdapter1_EnumOutputs(a, 0, &o) == S_OK) found = 1; else IDXGIAdapter1_Release(a);
    }
    if (!found) return 4;
    memset(&od, 0, sizeof od);
    if (FAILED(IDXGIOutput_GetDesc(o, &od))) return 8;
    printf("  NOTE dpi mode %d: output %ls rect %ld,%ld,%ld,%ld attached=%d rotation=%d\n", mode, od.DeviceName,
           od.DesktopCoordinates.left, od.DesktopCoordinates.top, od.DesktopCoordinates.right, od.DesktopCoordinates.bottom, od.AttachedToDesktop, od.Rotation);
    if (!od.AttachedToDesktop) rc = 4;
    else if (od.DesktopCoordinates.right <= od.DesktopCoordinates.left || od.DesktopCoordinates.bottom <= od.DesktopCoordinates.top) rc = 5;
    else {
        memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
        if (!GetMonitorInfoW(od.Monitor, (MONITORINFO *)&mi)) rc = 4;
        else if (wcscmp(mi.szDevice, od.DeviceName)) rc = 7;
        else if (!EqualRect(&mi.rcMonitor, &od.DesktopCoordinates)) { printf("  NOTE GetMonitorInfo rect %ld,%ld,%ld,%ld differs\n", mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom); rc = 3; }
    }
    IDXGIOutput_Release(o); IDXGIAdapter1_Release(a); IDXGIFactory1_Release(f);
    return rc;
}
static void dpi_case(int mode)
{
    DWORD rc;
    REQUIRE(d3dh_CreateDXGIFactory1());
    rc = t_run_child("dpi", mode == 0 ? "0" : mode == 1 ? "1" : "2", 60000);
    if (rc == 4) { t_skip("no output attached to the desktop"); return; }
    if (rc == 6) { t_skip("DPI awareness mode %d cannot be set", mode); return; }
    if (rc == 3) t_note("dpi mode %d: DXGI DesktopCoordinates differs from GetMonitorInfo rcMonitor", mode);
    EXPECT_NE(rc, 5);          /* empty DesktopCoordinates */
    EXPECT_NE(rc, 7);          /* device name differs from MONITORINFOEX.szDevice */
    EXPECT_NE(rc, 8);
    CHECK(rc == 0 || rc == 3);
}
T(process_dpi_unaware_output_desc) { dpi_case(0); }
T(process_dpi_system_aware_output_desc) { dpi_case(1); }
T(process_dpi_per_monitor_v2_output_desc) { dpi_case(2); }
