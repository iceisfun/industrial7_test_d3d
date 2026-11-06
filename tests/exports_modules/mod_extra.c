/* Extra loader behaviour of the graphics DLLs: KnownDLLs membership, LoadLibraryEx flag handling on loaded modules,
 * concurrent export resolution. */
#include "xm.h"
#include <winternl.h>

#define NEED(i) HMODULE h = xm_load(i); do { if (!h) { t_skip("%s not present", g_xm[i].dll); return; } } while (0)

typedef NTSTATUS (NTAPI *pNtOpenSection)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
typedef VOID (NTAPI *pRtlInitUnicodeString)(PUNICODE_STRING, PCWSTR);

/* None of the graphics DLLs is in the KnownDLLs cache on Win10 22H2: the section does not exist
 * (STATUS_OBJECT_NAME_NOT_FOUND), so they are always mapped from disk by the normal search. */
static void p_known(int i) {
    NEED(i);
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    pNtOpenSection open = (pNtOpenSection)GetProcAddress(nt, "NtOpenSection");
    pRtlInitUnicodeString init = (pRtlInitUnicodeString)GetProcAddress(nt, "RtlInitUnicodeString");
    WCHAR nm[128]; UNICODE_STRING us; OBJECT_ATTRIBUTES oa; HANDLE sec = NULL;
    if (!open || !init) { t_skip("ntdll entry points"); return; }
    swprintf(nm, 128, L"\\KnownDlls\\%S", g_xm[i].dll);
    init(&us, nm);
    InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);
    NTSTATUS st = open(&sec, SECTION_QUERY, &oa);
    XC(st == (NTSTATUS)0xC0000034, "NtOpenSection(\\KnownDlls\\%s) = 0x%08lx, want STATUS_OBJECT_NAME_NOT_FOUND", g_xm[i].dll, (unsigned long)st);
    if (st == 0 && sec) CloseHandle(sec);
}
XM_FOR_ALL(not_a_knowndll, p_known)

/* Re-loading an already loaded module with flags that do not change what is mapped returns the same handle, and every
 * successful LoadLibraryEx is balanced by one FreeLibrary without unloading the module the process already holds. */
static void p_flags(int i) {
    NEED(i);
    static const DWORD flags[] = { 0, LOAD_LIBRARY_SEARCH_SYSTEM32, LOAD_LIBRARY_AS_DATAFILE, DONT_RESOLVE_DLL_REFERENCES, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32 };
    static const char *const nm[] = { "0", "SEARCH_SYSTEM32", "AS_DATAFILE", "DONT_RESOLVE_DLL_REFERENCES", "SEARCH_DEFAULT_DIRS|SEARCH_SYSTEM32" };
    for (int k = 0; k < 5; k++) {
        HMODULE m = LoadLibraryExA(g_xm[i].dll, NULL, flags[k]);
        XC(m == h, "LoadLibraryEx(%s, %s) = %p, loaded handle %p", g_xm[i].dll, nm[k], m, h);
        if (m) XC(FreeLibrary(m), "FreeLibrary after %s", nm[k]);
    }
    XC(GetModuleHandleA(g_xm[i].dll) == h, "module still loaded");
    /* an absolute path to the same file also yields the same handle */
    char path[MAX_PATH]; GetModuleFileNameA(h, path, sizeof path);
    HMODULE m = LoadLibraryA(path);
    XC(m == h, "LoadLibrary(full path) = %p", m);
    if (m) FreeLibrary(m);
    /* a different case of the name is the same module */
    char up[64]; strcpy(up, g_xm[i].dll); for (char *c = up; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    m = LoadLibraryA(up);
    XC(m == h, "LoadLibrary(%s) = %p", up, m);
    if (m) FreeLibrary(m);
}
XM_FOR_ALL(loadlibraryex_flags_on_loaded_module, p_flags)

typedef struct { HMODULE h; int mod; FARPROC *want; volatile LONG bad; } Cg;
static DWORD WINAPI cg_thread(LPVOID p) {
    Cg *c = p; int n = g_xm[c->mod].n;
    for (int round = 0; round < 20; round++)
        for (int k = 0; k < n; k++) {
            const xexp *e = &g_xm[c->mod].e[k];
            FARPROC byname = e->name ? GetProcAddress(c->h, e->name) : c->want[k];
            FARPROC byord = GetProcAddress(c->h, (LPCSTR)(ULONG_PTR)e->ord);
            if (byname != c->want[k] || byord != c->want[k]) InterlockedIncrement(&c->bad);
        }
    return 0;
}
/* Six threads resolve every export by name and ordinal at the same time; results equal the single-threaded ones. */
static void p_conc(int i) {
    NEED(i);
    Cg c; HANDLE th[6]; int n = g_xm[i].n;
    c.h = h; c.mod = i; c.bad = 0; c.want = calloc((size_t)n, sizeof(FARPROC));
    for (int k = 0; k < n; k++) c.want[k] = GetProcAddress(h, (LPCSTR)(ULONG_PTR)g_xm[i].e[k].ord);
    for (int t = 0; t < 6; t++) th[t] = CreateThread(NULL, 0, cg_thread, &c, 0, NULL);
    for (int t = 0; t < 6; t++) { WaitForSingleObject(th[t], 60000); CloseHandle(th[t]); }
    XC(c.bad == 0, "%ld inconsistent resolutions across threads", (long)c.bad);
    free(c.want);
}
XM_FOR_ALL(getprocaddress_concurrent, p_conc)
