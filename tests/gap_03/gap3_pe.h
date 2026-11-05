/* Tiny PE export-directory reader over a loaded module (no DLL under test is called). */
#ifndef GAP3_PE_H
#define GAP3_PE_H
#include "t.h"
static IMAGE_NT_HEADERS *g3_nt(HMODULE m) {
    IMAGE_DOS_HEADER *d = (IMAGE_DOS_HEADER *)m;
    if (!m || d->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((char *)m + d->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE ? nt : NULL;
}
static IMAGE_EXPORT_DIRECTORY *g3_expdir(HMODULE m, DWORD *size) {
    IMAGE_NT_HEADERS *nt = g3_nt(m);
    if (!nt) return NULL;
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd->VirtualAddress) return NULL;
    if (size) *size = dd->Size;
    return (IMAGE_EXPORT_DIRECTORY *)((char *)m + dd->VirtualAddress);
}
/* returns the biased ordinal of a named export or 0; *rva = function RVA (or forwarder string RVA) */
static unsigned g3_find_named(HMODULE m, const char *name, DWORD *rva) {
    IMAGE_EXPORT_DIRECTORY *e = g3_expdir(m, NULL);
    if (!e) return 0;
    DWORD *names = (DWORD *)((char *)m + e->AddressOfNames);
    WORD *ords = (WORD *)((char *)m + e->AddressOfNameOrdinals);
    DWORD *funcs = (DWORD *)((char *)m + e->AddressOfFunctions);
    for (DWORD i = 0; i < e->NumberOfNames; i++)
        if (!strcmp((char *)m + names[i], name)) { if (rva) *rva = funcs[ords[i]]; return ords[i] + e->Base; }
    return 0;
}
static int g3_is_forwarder(HMODULE m, DWORD rva) {
    DWORD sz = 0; IMAGE_EXPORT_DIRECTORY *e = g3_expdir(m, &sz);
    DWORD base = (DWORD)((char *)e - (char *)m);
    return e && rva >= base && rva < base + sz;
}
/* section characteristics of the section containing rva, or 0 */
static DWORD g3_section_flags(HMODULE m, DWORD rva) {
    IMAGE_NT_HEADERS *nt = g3_nt(m);
    if (!nt) return 0;
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD sz = s[i].Misc.VirtualSize > s[i].SizeOfRawData ? s[i].Misc.VirtualSize : s[i].SizeOfRawData;
        if (rva >= s[i].VirtualAddress && rva < s[i].VirtualAddress + sz) return s[i].Characteristics;
    }
    return 0;
}
#endif
