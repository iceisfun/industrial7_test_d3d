#include "xm.h"

static int g_wmiss;
void xm_begin(void) { g_wmiss = 0; }
void xm_end(void) { if (g_wmiss) t_note("emulator: %d expectation(s) differ from Windows 10", g_wmiss); g_wmiss = 0; }
void xm_check(int ok, const char *file, int line, const char *expr, const char *fmt, ...) {
    char buf[512]; buf[0] = 0;
    if (fmt) { va_list a; va_start(a, fmt); vsnprintf(buf, sizeof buf, fmt, a); va_end(a); }
    if (!ok && t_is_wine()) { g_wmiss++; return; }
    t_check(ok, file, line, expr, buf[0] ? "%s" : NULL, buf);
}

HMODULE xm_load(int i) { return LoadLibraryA(g_xm[i].dll); }
int xm_wine(void) { return t_is_wine(); }

const IMAGE_NT_HEADERS64 *xm_nt(HMODULE m) {
    const IMAGE_DOS_HEADER *d = (const IMAGE_DOS_HEADER *)m;
    if (!m || d->e_magic != IMAGE_DOS_SIGNATURE || d->e_lfanew < 0x40 || d->e_lfanew > 0x1000) return NULL;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)((const char *)m + d->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;
    return nt;
}
const IMAGE_EXPORT_DIRECTORY *xm_expdir(HMODULE m, DWORD *size) {
    const IMAGE_NT_HEADERS64 *nt = xm_nt(m);
    if (!nt || nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) return NULL;
    const IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd->VirtualAddress) return NULL;
    if (size) *size = dd->Size;
    return (const IMAGE_EXPORT_DIRECTORY *)((const char *)m + dd->VirtualAddress);
}
const char *xm_name_of_ordinal(HMODULE m, unsigned ord) {
    const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(m, NULL);
    if (!ed) return NULL;
    const DWORD *names = (const DWORD *)((const char *)m + ed->AddressOfNames);
    const WORD *ords = (const WORD *)((const char *)m + ed->AddressOfNameOrdinals);
    for (DWORD i = 0; i < ed->NumberOfNames; i++)
        if ((unsigned)ords[i] + ed->Base == ord) return (const char *)m + names[i];
    return NULL;
}
int xm_in_dir(HMODULE m, const void *p) {
    DWORD sz = 0; const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(m, &sz);
    if (!ed) return 0;
    const char *b = (const char *)ed;
    return (const char *)p >= b && (const char *)p < b + sz;
}
int xm_in_image(HMODULE m, const void *p) {
    const IMAGE_NT_HEADERS64 *nt = xm_nt(m);
    return nt && (const char *)p >= (const char *)m && (const char *)p < (const char *)m + nt->OptionalHeader.SizeOfImage;
}
int xm_is_exec(HMODULE m, const void *p) {
    const IMAGE_NT_HEADERS64 *nt = xm_nt(m);
    if (!nt || !xm_in_image(m, p)) return 0;
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    DWORD rva = (DWORD)((const char *)p - (const char *)m);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (rva >= s[i].VirtualAddress && rva < s[i].VirtualAddress + (s[i].Misc.VirtualSize ? s[i].Misc.VirtualSize : s[i].SizeOfRawData))
            return (s[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    return 0;
}
int xm_has_name(int mod, const char *name) {
    for (int k = 0; k < g_xm[mod].n; k++) if (g_xm[mod].e[k].name && !strcmp(g_xm[mod].e[k].name, name)) return 1;
    return 0;
}
