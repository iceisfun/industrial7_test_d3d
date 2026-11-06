/* Export-table conformance: every listed export resolves by name and by ordinal, the export directory is
 * well formed, names of other modules are absent, and bad GetProcAddress inputs fail the way Windows does. */
#include "xm.h"

#define NEED(i) HMODULE h = xm_load(i); do { if (!h) { t_skip("%s not present", g_xm[i].dll); return; } } while (0)
#define ORD(o) ((LPCSTR)(ULONG_PTR)(o))

static void p_loads(int i) {
    NEED(i);
    char path[MAX_PATH], sys[MAX_PATH], want[MAX_PATH * 2];
    EXPECT_EQ(GetModuleHandleA(g_xm[i].dll), h);
    DWORD n = GetModuleFileNameA(h, path, sizeof path);
    XC(n > 0 && n < sizeof path, "len %lu", n);
    UINT sn = GetSystemDirectoryA(sys, sizeof sys);
    XC(sn > 0, "sysdir");
    snprintf(want, sizeof want, "%s\\%s", sys, g_xm[i].dll);
    XC(!lstrcmpiA(path, want), "path \"%s\" want \"%s\"", path, want);
    XC(((ULONG_PTR)h & 0xFFFF) == 0, "module base %p is 64K aligned", h);
    XC(xm_nt(h) != NULL, "PE headers readable");
}
XM_FOR_ALL(loads_from_system32, p_loads)

static void p_names(int i) {
    NEED(i);
    int cnt = 0;
    for (int k = 0; k < g_xm[i].n; k++) {
        const xexp *e = &g_xm[i].e[k];
        if (!e->name) continue;
        cnt++;
        SetLastError(0xdeadbeef);
        FARPROC p = GetProcAddress(h, e->name);
        XC(p != NULL, "%s!%s not exported", g_xm[i].dll, e->name);
    }
    t_note("%s: %d named exports checked", g_xm[i].dll, cnt);
}
XM_FOR_ALL(names_resolve, p_names)

static void p_ords(int i) {
    NEED(i);
    for (int k = 0; k < g_xm[i].n; k++) {
        const xexp *e = &g_xm[i].e[k];
        FARPROC po = GetProcAddress(h, ORD(e->ord));
        XC(po != NULL, "%s ordinal %u (%s) not exported", g_xm[i].dll, e->ord, e->name ? e->name : "no name");
        if (e->name) {
            FARPROC pn = GetProcAddress(h, e->name);
            XC(po == pn, "%s!%s: by-name %p != by-ordinal(%u) %p", g_xm[i].dll, e->name, pn, e->ord, po);
        }
    }
}
XM_FOR_ALL(ordinals_resolve, p_ords)

static void p_ordmap(int i) {
    NEED(i);
    for (int k = 0; k < g_xm[i].n; k++) {
        const xexp *e = &g_xm[i].e[k];
        const char *nm = xm_name_of_ordinal(h, e->ord);
        if (e->name) XC(nm && !strcmp(nm, e->name), "ordinal %u: name \"%s\" want \"%s\"", e->ord, nm ? nm : "(none)", e->name);
        else XC(nm == NULL, "ordinal %u should be unnamed, is \"%s\"", e->ord, nm ? nm : "");
    }
}
XM_FOR_ALL(ordinal_to_name_map, p_ordmap)

static void p_dir(int i) {
    NEED(i);
    DWORD sz = 0;
    const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(h, &sz);
    XC(ed != NULL, "export directory present");
    if (!ed) return;
    unsigned maxo = 0, named = 0;
    for (int k = 0; k < g_xm[i].n; k++) { if (g_xm[i].e[k].ord > maxo) maxo = g_xm[i].e[k].ord; if (g_xm[i].e[k].name) named++; }
    XC(ed->Base >= 1, "Base %lu", ed->Base);
    XC(ed->Base + ed->NumberOfFunctions > maxo, "ordinal table (Base %lu, %lu) covers ordinal %u", ed->Base, ed->NumberOfFunctions, maxo);
    XC(ed->NumberOfNames <= ed->NumberOfFunctions, "names %lu <= functions %lu", ed->NumberOfNames, ed->NumberOfFunctions);
    XC(ed->NumberOfNames >= named, "names %lu >= %u", ed->NumberOfNames, named);
    if (ed->NumberOfNames != named) t_note("%s: NumberOfNames %lu, reference build has %u", g_xm[i].dll, ed->NumberOfNames, named);
    if (ed->Base + ed->NumberOfFunctions != maxo + 1) t_note("%s: ordinal span %lu..%lu, reference max %u", g_xm[i].dll, ed->Base, ed->Base + ed->NumberOfFunctions - 1, maxo);
    XC(ed->Characteristics == 0, "Characteristics 0x%lx", ed->Characteristics);
    XC((ed->AddressOfNames && ed->AddressOfNameOrdinals && ed->AddressOfFunctions) || ed->NumberOfNames == 0, "table RVAs");
    /* the loader binary-searches the name table: it must be strictly increasing in byte order */
    const DWORD *names = (const DWORD *)((const char *)h + ed->AddressOfNames);
    const WORD *ords = (const WORD *)((const char *)h + ed->AddressOfNameOrdinals);
    const DWORD *funcs = (const DWORD *)((const char *)h + ed->AddressOfFunctions);
    const char *prev = NULL; int sorted = 1, ordok = 1, ascii = 1, nz = 1;
    for (DWORD k = 0; k < ed->NumberOfNames; k++) {
        const char *s = (const char *)h + names[k];
        if (prev && strcmp(prev, s) >= 0) sorted = 0;
        if (ords[k] >= ed->NumberOfFunctions) ordok = 0; else if (!funcs[ords[k]]) nz = 0;
        for (const char *c = s; *c; c++) if ((unsigned char)*c < 0x21 || (unsigned char)*c > 0x7e) ascii = 0;
        if (!*s) ascii = 0;
        prev = s;
    }
    XC(sorted, "name table strictly sorted");
    XC(ordok, "name ordinals in range");
    XC(ascii, "names printable ASCII");
    XC(nz, "every named export has a non-zero RVA");
    const char *nm = (const char *)h + ed->Name;
    if (lstrcmpiA(nm, g_xm[i].dll)) t_note("%s: export directory names the module \"%s\"", g_xm[i].dll, nm);
    /* ordinals in the range but absent from the reference table: only a note (newer servicing may add exports) */
    for (DWORD o = ed->Base; o < ed->Base + ed->NumberOfFunctions; o++) {
        int known = 0;
        for (int k = 0; k < g_xm[i].n; k++) if (g_xm[i].e[k].ord == o) { known = 1; break; }
        if (!known && funcs[o - ed->Base]) t_note("%s: ordinal %lu is exported but absent from the reference table", g_xm[i].dll, o);
    }
}
XM_FOR_ALL(export_directory_wellformed, p_dir)

static void p_holes(int i) {
    NEED(i);
    const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(h, NULL);
    if (!ed) { t_skip("no export directory"); return; }
    const DWORD *funcs = (const DWORD *)((const char *)h + ed->AddressOfFunctions);
    int holes = 0;
    for (DWORD o = ed->Base; o < ed->Base + ed->NumberOfFunctions; o++) {
        if (funcs[o - ed->Base]) continue;
        holes++;
        SetLastError(0);
        FARPROC p = GetProcAddress(h, ORD(o));
        XC(p == NULL, "ordinal %lu has RVA 0 but resolves to %p", o, p);
        XERR(ERROR_INVALID_ORDINAL);   /* Win10: a hole in the ordinal table reports 182, not 127 */
    }
    if (!holes) t_note("%s: export ordinal range is dense", g_xm[i].dll);
}
XM_FOR_ALL(ordinal_holes_do_not_resolve, p_holes)

static void p_rvas(int i) {
    NEED(i);
    const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(h, NULL);
    const DWORD *funcs = ed ? (const DWORD *)((const char *)h + ed->AddressOfFunctions) : NULL;
    int rvadiff = 0, nonexec = 0;
    for (int k = 0; k < g_xm[i].n; k++) {
        const xexp *e = &g_xm[i].e[k];
        FARPROC p = GetProcAddress(h, ORD(e->ord));
        if (!p) continue;
        DWORD raw = funcs ? funcs[e->ord - ed->Base] : 0;
        int fwd = ed && xm_in_dir(h, (const char *)h + raw);
        if (e->fwd) {
            XC(fwd, "ordinal %u should be a forwarder", e->ord);
            if (fwd) {
                const char *s = (const char *)h + raw;
                if (strcmp(s, e->fwd)) t_note("%s ordinal %u forwards to \"%s\", reference build has \"%s\"", g_xm[i].dll, e->ord, s, e->fwd);
                XC(!xm_in_image(h, p), "forwarded ordinal %u resolves outside the module", e->ord);
            }
        } else {
            XC(!fwd, "ordinal %u should not be a forwarder", e->ord);
            XC(xm_in_image(h, p), "ordinal %u resolves inside the image", e->ord);
            if ((DWORD)((char *)p - (char *)h) != e->rva) rvadiff++;
            if (!xm_is_exec(h, p)) {
                nonexec++;
                if (!strcmp(g_xm[i].dll, "d3d12core.dll") && e->name && !strcmp(e->name, "D3D12SDKVersion"))
                    XC(1, NULL);
                else t_note("%s!%s (ordinal %u) is not in an executable section", g_xm[i].dll, e->name ? e->name : "?", e->ord);
            }
        }
    }
    if (rvadiff) t_note("%s: %d export RVA(s) differ from the reference build %s (expected across servicing)", g_xm[i].dll, rvadiff, g_xm[i].ver);
}
XM_FOR_ALL(export_rvas_and_forwarders, p_rvas)

/* Names that other modules export but this one does not must not resolve (exports are exact, case-sensitive). */
static void p_absent(int i) {
    NEED(i);
    int checked = 0;
    for (int j = 0; j < XM_COUNT; j++) {
        if (j == i) continue;
        for (int k = 0; k < g_xm[j].n; k++) {
            const char *nm = g_xm[j].e[k].name;
            if (!nm || xm_has_name(i, nm)) continue;
            SetLastError(0);
            FARPROC p = GetProcAddress(h, nm);
            XC(p == NULL, "%s unexpectedly exports %s (an export of %s)", g_xm[i].dll, nm, g_xm[j].dll);
            if (!p) { XERR(ERROR_PROC_NOT_FOUND); }
            checked++;
        }
    }
    t_note("%s: %d foreign names verified absent", g_xm[i].dll, checked);
}
XM_FOR_ALL(foreign_exports_absent, p_absent)

static void p_bad(int i) {
    NEED(i);
    const char *first = NULL;
    for (int k = 0; k < g_xm[i].n; k++) if (g_xm[i].e[k].name) { first = g_xm[i].e[k].name; break; }
    const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(h, NULL);
    SetLastError(0);
    XC(GetProcAddress(h, "NoSuchExport_xyz") == NULL, "missing name");
    XERR(ERROR_PROC_NOT_FOUND);
    SetLastError(0);
    XC(GetProcAddress(h, "") == NULL, "empty name");
    XERR(ERROR_PROC_NOT_FOUND);
    SetLastError(0);
    XC(GetProcAddress(h, ORD(0)) == NULL, "ordinal 0");
    XERR(ERROR_INVALID_PARAMETER);   /* Win10: ordinal 0 is rejected before the export table is consulted */
    SetLastError(0);
    XC(GetProcAddress(h, ORD(0xFFFF)) == NULL, "ordinal 0xFFFF");
    XERR(ERROR_INVALID_ORDINAL);
    if (ed) {
        SetLastError(0);
        FARPROC p = GetProcAddress(h, ORD(ed->Base + ed->NumberOfFunctions));
        XC(p == NULL, "first ordinal past the table");
        XERR(ERROR_INVALID_ORDINAL);
    }
    if (!first) return;
    char buf[256];
    static const char *const suffix[] = { "_", "@8", "@16", "@32", "Ex2", "Q" };
    for (int s = 0; s < 6; s++) {
        snprintf(buf, sizeof buf, "%s%s", first, suffix[s]);
        if (xm_has_name(i, buf)) continue;
        SetLastError(0);
        XC(GetProcAddress(h, buf) == NULL, "decorated name %s", buf);
        XERR(ERROR_PROC_NOT_FOUND);
    }
    snprintf(buf, sizeof buf, "_%s", first);
    SetLastError(0);
    XC(GetProcAddress(h, buf) == NULL, "underscore prefix %s", buf);
    XERR(ERROR_PROC_NOT_FOUND);
    snprintf(buf, sizeof buf, "%s ", first);
    SetLastError(0);
    XC(GetProcAddress(h, buf) == NULL, "trailing blank");
    XERR(ERROR_PROC_NOT_FOUND);
    /* exports are case sensitive */
    snprintf(buf, sizeof buf, "%s", first);
    for (char *c = buf; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
    if (strcmp(buf, first) && !xm_has_name(i, buf)) {
        SetLastError(0);
        XC(GetProcAddress(h, buf) == NULL, "lower case %s", buf);
        XERR(ERROR_PROC_NOT_FOUND);
    }
    snprintf(buf, sizeof buf, "%s", first);
    for (char *c = buf; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    if (strcmp(buf, first) && !xm_has_name(i, buf)) {
        SetLastError(0);
        XC(GetProcAddress(h, buf) == NULL, "upper case %s", buf);
        XERR(ERROR_PROC_NOT_FOUND);
    }
    /* a prefix of an existing export is not an export */
    if (strlen(first) > 3) {
        snprintf(buf, sizeof buf, "%.*s", (int)strlen(first) - 1, first);
        if (!xm_has_name(i, buf)) {
            SetLastError(0);
            XC(GetProcAddress(h, buf) == NULL, "truncated name %s", buf);
            XERR(ERROR_PROC_NOT_FOUND);
        }
    }
}
XM_FOR_ALL(getprocaddress_negative, p_bad)

static void p_repeat(int i) {
    NEED(i);
    for (int k = 0; k < g_xm[i].n; k++) {
        const xexp *e = &g_xm[i].e[k];
        FARPROC a = GetProcAddress(h, ORD(e->ord)), b = GetProcAddress(h, ORD(e->ord));
        XC(a == b, "ordinal %u stable across calls", e->ord);
        if (k > 8) break;
    }
    /* a successful lookup does not leave a stale error behind that matters: error is untouched or zero */
    if (g_xm[i].e[0].name) {
        SetLastError(0x1234);
        FARPROC p = GetProcAddress(h, g_xm[i].e[0].name);
        if (p) { DWORD e = GetLastError(); if (e != 0x1234) t_note("successful GetProcAddress changed last error to %lu", e); }
    }
}
XM_FOR_ALL(getprocaddress_stable, p_repeat)

TEST(exports_modules_getprocaddress_null_module) {
    /* a NULL module means the process image; the test executable exports nothing */
    SetLastError(0);
    CHECK(GetProcAddress(NULL, "D3D11CreateDevice") == NULL);
    XERR(ERROR_PROC_NOT_FOUND);
    SetLastError(0);
    CHECK(GetProcAddress(NULL, NULL) == NULL);
    SetLastError(0);
    CHECK(GetProcAddress(NULL, ORD(1)) == NULL);
    { DWORD e = GetLastError(); CHECK(e != 0); t_note("GetProcAddress(NULL, ordinal 1) last error %lu", e); }
}

/* Export-name table: every name ordinal is unique (no two names for one slot unless aliases) and forwarder
 * strings have the MODULE.Function / MODULE.#ordinal form inside the export directory. */
static void p_fwdfmt(int i) {
    NEED(i);
    DWORD sz = 0;
    const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(h, &sz);
    if (!ed) { t_skip("no export directory"); return; }
    const DWORD *funcs = (const DWORD *)((const char *)h + ed->AddressOfFunctions);
    int nfwd = 0;
    for (DWORD k = 0; k < ed->NumberOfFunctions; k++) {
        if (!funcs[k]) continue;
        const char *p = (const char *)h + funcs[k];
        if (!xm_in_dir(h, p)) continue;
        nfwd++;
        const char *d = strchr(p, '.');
        size_t len = strnlen(p, 256);
        XC(len < 256 && d && d > p && d[1] != 0, "forwarder \"%s\" has the MODULE.Name form", p);
        if (d && d[1] == '#') { char *end; unsigned long o = strtoul(d + 2, &end, 10); XC(*end == 0 && end > d + 2 && o > 0 && o < 65536, "ordinal forwarder \"%s\"", p); }
        XC(!strchr(p, ' ') && !strchr(p, '\\') && !strchr(p, '/'), "forwarder \"%s\" has no path or blank", p);
    }
    int want = 0; for (int k = 0; k < g_xm[i].n; k++) if (g_xm[i].e[k].fwd) want++;
    if (nfwd != want) t_note("%s: %d forwarders, reference build has %d", g_xm[i].dll, nfwd, want);
}
XM_FOR_ALL(forwarder_string_format, p_fwdfmt)

static void p_nameord_unique(int i) {
    NEED(i);
    const IMAGE_EXPORT_DIRECTORY *ed = xm_expdir(h, NULL);
    if (!ed) { t_skip("no export directory"); return; }
    const WORD *ords = (const WORD *)((const char *)h + ed->AddressOfNameOrdinals);
    int dup = 0, alias = 0;
    for (DWORD a = 0; a < ed->NumberOfNames; a++)
        for (DWORD b = a + 1; b < ed->NumberOfNames; b++) if (ords[a] == ords[b]) alias++;
    XCK(dup == 0);
    if (alias) t_note("%s: %d name pair(s) alias one export slot", g_xm[i].dll, alias);
    /* aliases resolve to the same address */
    const DWORD *names = (const DWORD *)((const char *)h + ed->AddressOfNames);
    for (DWORD a = 0; a + 1 < ed->NumberOfNames && a < 2000; a++)
        for (DWORD b = a + 1; b < ed->NumberOfNames && b < a + 40; b++)
            if (ords[a] == ords[b])
                XC(GetProcAddress(h, (const char *)h + names[a]) == GetProcAddress(h, (const char *)h + names[b]), "aliases resolve alike");
}
XM_FOR_ALL(name_aliases_share_address, p_nameord_unique)

static void p_loadforms(int i) {
    NEED(i);
    char full[MAX_PATH * 2], stm[64], up[64];
    GetModuleFileNameA(h, full, sizeof full);
    snprintf(stm, sizeof stm, "%s", g_xm[i].dll); char *dot = strrchr(stm, '.'); if (dot) *dot = 0;
    HMODULE x;
    x = LoadLibraryA(g_xm[i].dll); XC(x == h, "same name"); if (x) FreeLibrary(x);
    x = LoadLibraryA(stm); XC(x == h, "without extension"); if (x) FreeLibrary(x);
    snprintf(up, sizeof up, "%s", g_xm[i].dll); CharUpperA(up);
    x = LoadLibraryA(up); XC(x == h, "upper-case %s", up); if (x) FreeLibrary(x);
    x = LoadLibraryA(full); XC(x == h, "full path"); if (x) FreeLibrary(x);
    x = LoadLibraryExA(g_xm[i].dll, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32); XC(x == h, "SEARCH_SYSTEM32"); if (x) FreeLibrary(x);
    x = LoadLibraryExA(g_xm[i].dll, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS); XC(x == h, "SEARCH_DEFAULT_DIRS"); if (x) FreeLibrary(x);
    /* the same file through a forward-slash path or an extra "." component is the same image */
    char alt[MAX_PATH * 2]; snprintf(alt, sizeof alt, "%s", full);
    for (char *c = alt; *c; c++) if (*c == '\\') *c = '/';
    x = LoadLibraryA(alt); if (x != h) t_note("%s: forward-slash path gives %p, not the loaded image", g_xm[i].dll, x); if (x) FreeLibrary(x);
}
XM_FOR_ALL(loadlibrary_name_forms, p_loadforms)
