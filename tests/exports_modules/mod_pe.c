/* PE image properties of the D3D module family: headers, sections, memory protections, on-disk image,
 * version resource, 32-bit counterparts. */
#include "xm.h"

static void stem(const char *dll, char *out, size_t cap) { snprintf(out, cap, "%s", dll); char *d = strrchr(out, '.'); if (d) *d = 0; }
#define NEED(i) HMODULE h = xm_load(i); do { if (!h) { t_skip("%s not present", g_xm[i].dll); return; } } while (0)

static void p_headers(int i) {
    NEED(i);
    const IMAGE_DOS_HEADER *d = (const IMAGE_DOS_HEADER *)h;
    XC(d->e_magic == IMAGE_DOS_SIGNATURE, "MZ");
    const IMAGE_NT_HEADERS64 *nt = xm_nt(h);
    XC(nt != NULL, "PE signature");
    if (!nt) return;
    const IMAGE_FILE_HEADER *fh = &nt->FileHeader;
    const IMAGE_OPTIONAL_HEADER64 *oh = &nt->OptionalHeader;
    XC(fh->Machine == IMAGE_FILE_MACHINE_AMD64, "machine 0x%x", fh->Machine);
    XC(oh->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC, "PE32+ magic 0x%x", oh->Magic);
    XC(fh->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64), "optional header size %u", fh->SizeOfOptionalHeader);
    XC(fh->Characteristics & IMAGE_FILE_DLL, "DLL flag");
    XC(fh->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE, "executable image");
    XC(fh->Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE, "large address aware");
    XC(!(fh->Characteristics & IMAGE_FILE_32BIT_MACHINE), "not a 32-bit machine image");
    XC(oh->Subsystem == g_xm[i].subsystem, "subsystem %u want %d", oh->Subsystem, g_xm[i].subsystem);
    XC(oh->AddressOfEntryPoint != 0, "has a DllMain entry point");
    XC(oh->AddressOfEntryPoint < oh->SizeOfImage, "entry inside image");
    XC(xm_is_exec(h, (const char *)h + oh->AddressOfEntryPoint), "entry point in an executable section");
    XC(oh->SectionAlignment == 0x1000, "section alignment 0x%lx", oh->SectionAlignment);
    XC(oh->SizeOfImage % oh->SectionAlignment == 0, "image size aligned");
    XC(oh->FileAlignment >= 0x200 && oh->FileAlignment <= 0x1000, "file alignment 0x%lx", oh->FileAlignment);
    XC(fh->NumberOfSections >= 5 && fh->NumberOfSections <= 12, "%u sections", fh->NumberOfSections);
    XC(oh->SizeOfHeaders >= (DWORD)((const char *)(IMAGE_FIRST_SECTION(nt) + fh->NumberOfSections) - (const char *)h), "headers fit SizeOfHeaders");
    XC(oh->DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE, "ASLR (DYNAMIC_BASE)");
    XC(oh->DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT, "NX compatible");
    XC(oh->NumberOfRvaAndSizes >= 16, "data directory count %lu", oh->NumberOfRvaAndSizes);
    XC(oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress != 0, "export directory");
    XC(oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress != 0, "import directory");
    XC(oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress != 0, "resource directory");
    XC(oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress != 0, ".pdata exception directory");
    XC(oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress != 0, "relocation directory");
    XC(oh->MajorOperatingSystemVersion >= 10 || oh->MajorOperatingSystemVersion == 6, "OS version %u.%u", oh->MajorOperatingSystemVersion, oh->MinorOperatingSystemVersion);
    if (!(oh->DllCharacteristics & 0x4000)) t_note("%s: no Control Flow Guard flag", g_xm[i].dll);
    if (!(oh->DllCharacteristics & IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA)) t_note("%s: no HIGH_ENTROPY_VA", g_xm[i].dll);
    if (oh->ImageBase != 0x180000000ULL) t_note("%s: preferred base 0x%llx", g_xm[i].dll, (unsigned long long)oh->ImageBase);
    t_note("%s: SizeOfImage 0x%lx, TimeDateStamp 0x%lx", g_xm[i].dll, oh->SizeOfImage, fh->TimeDateStamp);
    XC(fh->TimeDateStamp != 0, "time stamp is not zero");
}
XM_FOR_ALL(pe_headers, p_headers)

static void p_sections(int i) {
    NEED(i);
    const IMAGE_NT_HEADERS64 *nt = xm_nt(h);
    if (!nt) { t_skip("no PE"); return; }
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    int n = nt->FileHeader.NumberOfSections;
    static const char *const need[] = { ".text", ".rdata", ".data", ".pdata", ".rsrc", ".reloc" };
    for (int k = 0; k < 6; k++) {
        int found = -1;
        for (int j = 0; j < n; j++) if (!strncmp((const char *)s[j].Name, need[k], 8)) found = j;
        XC(found >= 0, "section %s present", need[k]);
        if (found < 0) continue;
        DWORD c = s[found].Characteristics;
        if (!strcmp(need[k], ".text")) XC((c & IMAGE_SCN_MEM_EXECUTE) && (c & IMAGE_SCN_MEM_READ) && !(c & IMAGE_SCN_MEM_WRITE), ".text is R-X (0x%lx)", c);
        if (!strcmp(need[k], ".rdata")) XC((c & IMAGE_SCN_MEM_READ) && !(c & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)), ".rdata is R-- (0x%lx)", c);
        if (!strcmp(need[k], ".data")) XC((c & IMAGE_SCN_MEM_READ) && (c & IMAGE_SCN_MEM_WRITE) && !(c & IMAGE_SCN_MEM_EXECUTE), ".data is RW- (0x%lx)", c);
        if (!strcmp(need[k], ".pdata")) XC((c & IMAGE_SCN_MEM_READ) && !(c & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)), ".pdata is R-- (0x%lx)", c);
        if (!strcmp(need[k], ".rsrc")) XC((c & IMAGE_SCN_MEM_READ) && !(c & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)), ".rsrc is R-- (0x%lx)", c);
        if (!strcmp(need[k], ".reloc")) XC((c & IMAGE_SCN_MEM_DISCARDABLE) && (c & IMAGE_SCN_MEM_READ), ".reloc is discardable (0x%lx)", c);
    }
    /* sections are contiguous, ascending and aligned */
    DWORD prev_end = nt->OptionalHeader.SizeOfHeaders;
    int ok = 1, wx = 0;
    for (int j = 0; j < n; j++) {
        if (s[j].VirtualAddress < prev_end || s[j].VirtualAddress % 0x1000) ok = 0;
        DWORD vs = s[j].Misc.VirtualSize ? s[j].Misc.VirtualSize : s[j].SizeOfRawData;
        prev_end = s[j].VirtualAddress + vs;
        if ((s[j].Characteristics & IMAGE_SCN_MEM_EXECUTE) && (s[j].Characteristics & IMAGE_SCN_MEM_WRITE)) wx = 1;
    }
    XC(ok, "sections ascending, page aligned");
    XC(prev_end <= nt->OptionalHeader.SizeOfImage, "sections end within SizeOfImage");
    XC(!wx, "no writable+executable section");
    /* compare names to the reference build: informative only */
    char got[256] = ""; for (int j = 0; j < n; j++) { char nm[9] = { 0 }; memcpy(nm, s[j].Name, 8); strcat(got, j ? "," : ""); strcat(got, nm); }
    if (strcmp(got, g_xm[i].sections)) t_note("%s: sections [%s], reference build %s has [%s]", g_xm[i].dll, got, g_xm[i].ver, g_xm[i].sections);
}
XM_FOR_ALL(pe_sections, p_sections)

static void p_protect(int i) {
    NEED(i);
    const IMAGE_NT_HEADERS64 *nt = xm_nt(h);
    if (!nt) { t_skip("no PE"); return; }
    MEMORY_BASIC_INFORMATION mbi;
    XC(VirtualQuery(h, &mbi, sizeof mbi) == sizeof mbi, "VirtualQuery header");
    XC(mbi.Type == MEM_IMAGE, "mapped as MEM_IMAGE (0x%lx)", mbi.Type);
    XC(mbi.AllocationBase == (void *)h, "allocation base equals module handle");
    XC(mbi.Protect == PAGE_READONLY, "header page protection 0x%lx", mbi.Protect);
    XC(mbi.State == MEM_COMMIT, "committed");
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int j = 0; j < nt->FileHeader.NumberOfSections; j++) {
        char nm[9] = { 0 }; memcpy(nm, s[j].Name, 8);
        void *p = (char *)h + s[j].VirtualAddress;
        if (VirtualQuery(p, &mbi, sizeof mbi) != sizeof mbi) { XC(0, "VirtualQuery %s", nm); continue; }
        DWORD c = s[j].Characteristics;
        if (!strcmp(nm, ".text")) XC(mbi.Protect == PAGE_EXECUTE_READ, ".text protection 0x%lx", mbi.Protect);
        else if (!strcmp(nm, ".rdata") || !strcmp(nm, ".pdata") || !strcmp(nm, ".rsrc") || !strcmp(nm, ".reloc"))
            XC(mbi.Protect == PAGE_READONLY || !(c & IMAGE_SCN_MEM_DISCARDABLE), "%s protection 0x%lx", nm, mbi.Protect);
        else if (!strcmp(nm, ".data")) XC(mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_WRITECOPY, ".data protection 0x%lx", mbi.Protect);
        XC(mbi.Type == MEM_IMAGE, "%s is MEM_IMAGE", nm);
    }
}
XM_FOR_ALL(pe_memory_protections, p_protect)

static void p_disk(int i) {
    NEED(i);
    char path[MAX_PATH];
    GetModuleFileNameA(h, path, sizeof path);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    XC(f != INVALID_HANDLE_VALUE, "open %s", path);
    if (f == INVALID_HANDLE_VALUE) return;
    BYTE buf[0x400]; DWORD got = 0;
    XC(ReadFile(f, buf, sizeof buf, &got, NULL) && got == sizeof buf, "read headers");
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
    CloseHandle(f);
    const IMAGE_DOS_HEADER *d = (const IMAGE_DOS_HEADER *)buf;
    if (got != sizeof buf || d->e_magic != IMAGE_DOS_SIGNATURE || d->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > sizeof buf) { XC(0, "file headers within first 1K"); return; }
    const IMAGE_NT_HEADERS64 *fnt = (const IMAGE_NT_HEADERS64 *)(buf + d->e_lfanew), *mnt = xm_nt(h);
    XC(fnt->Signature == IMAGE_NT_SIGNATURE, "file PE signature");
    XC(fnt->FileHeader.TimeDateStamp == mnt->FileHeader.TimeDateStamp, "time stamp disk vs memory");
    XC(fnt->OptionalHeader.SizeOfImage == mnt->OptionalHeader.SizeOfImage, "SizeOfImage disk vs memory");
    XC(fnt->OptionalHeader.AddressOfEntryPoint == mnt->OptionalHeader.AddressOfEntryPoint, "entry disk vs memory");
    XC(fnt->OptionalHeader.CheckSum == mnt->OptionalHeader.CheckSum, "checksum disk vs memory");
    XC(fnt->FileHeader.NumberOfSections == mnt->FileHeader.NumberOfSections, "section count disk vs memory");
    XC(sz.QuadPart > fnt->OptionalHeader.SizeOfHeaders && sz.QuadPart <= fnt->OptionalHeader.SizeOfImage + 0x100000, "file size %lld plausible", sz.QuadPart);
    if (fnt->OptionalHeader.CheckSum == 0) t_note("%s: PE checksum is 0 on disk", g_xm[i].dll);
    /* base relocation: loaded base equals the preferred base unless ASLR moved it */
    if ((ULONG_PTR)h != mnt->OptionalHeader.ImageBase) t_note("%s relocated: loaded at %p, header says 0x%llx", g_xm[i].dll, h, (unsigned long long)mnt->OptionalHeader.ImageBase);
}
XM_FOR_ALL(pe_disk_matches_memory, p_disk)

/* ---- version resource ------------------------------------------------------------------------- */
typedef DWORD (WINAPI *pfnVerSizeW)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *pfnVerInfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *pfnVerQueryW)(LPCVOID, LPCWSTR, LPVOID *, PUINT);

static int verstr(const void *blob, const WORD *tr, const char *key, WCHAR *out, size_t cap) {
    pfnVerQueryW q = (pfnVerQueryW)GetProcAddress(LoadLibraryA("version.dll"), "VerQueryValueW");
    WCHAR sub[96], k[48]; LPVOID v = NULL; UINT l = 0;
    MultiByteToWideChar(CP_ACP, 0, key, -1, k, 48);
    swprintf(sub, 96, L"\\StringFileInfo\\%04x%04x\\%ls", tr[0], tr[1], k);
    if (!q(blob, sub, &v, &l) || !v) return 0;
    wcsncpy(out, (WCHAR *)v, cap - 1); out[cap - 1] = 0;
    return 1;
}

static void p_version(int i) {
    NEED(i);
    HMODULE vd = LoadLibraryA("version.dll");
    pfnVerSizeW sz = vd ? (pfnVerSizeW)GetProcAddress(vd, "GetFileVersionInfoSizeW") : NULL;
    pfnVerInfoW inf = vd ? (pfnVerInfoW)GetProcAddress(vd, "GetFileVersionInfoW") : NULL;
    pfnVerQueryW q = vd ? (pfnVerQueryW)GetProcAddress(vd, "VerQueryValueW") : NULL;
    if (!sz || !inf || !q) { t_skip("version.dll unavailable"); return; }
    WCHAR path[MAX_PATH]; GetModuleFileNameW(h, path, MAX_PATH);
    DWORD z = 0, n = sz(path, &z);
    XC(n > 0x80, "version resource size %lu", n);
    if (n <= 0x80) return;
    void *blob = HeapAlloc(GetProcessHeap(), 0, n);
    XC(inf(path, 0, n, blob), "GetFileVersionInfoW");
    VS_FIXEDFILEINFO *fi = NULL; UINT l = 0;
    if (!q(blob, L"\\", (LPVOID *)&fi, &l) || !fi) { XC(0, "fixed info"); HeapFree(GetProcessHeap(), 0, blob); return; }
    XC(l == sizeof *fi, "fixed info size %u", l);
    XC(fi->dwSignature == 0xFEEF04BD, "fixed info signature");
    XC(HIWORD(fi->dwFileVersionMS) == 10 && LOWORD(fi->dwFileVersionMS) == 0, "FileVersion %u.%u.%u.%u", HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS), HIWORD(fi->dwFileVersionLS), LOWORD(fi->dwFileVersionLS));
    XC(HIWORD(fi->dwFileVersionLS) >= 10240, "build number %u", HIWORD(fi->dwFileVersionLS));
    XC(HIWORD(fi->dwProductVersionMS) == 10 && LOWORD(fi->dwProductVersionMS) == 0, "ProductVersion %u.%u", HIWORD(fi->dwProductVersionMS), LOWORD(fi->dwProductVersionMS));
    XC(fi->dwFileOS == VOS_NT_WINDOWS32, "FileOS 0x%lx", fi->dwFileOS);
    XC(fi->dwFileType == VFT_DLL, "FileType %lu", fi->dwFileType);
    XC(fi->dwFileSubtype == 0, "FileSubtype %lu", fi->dwFileSubtype);
    XC(!(fi->dwFileFlags & fi->dwFileFlagsMask & VS_FF_PRIVATEBUILD), "not a private build");
    XC(!(fi->dwFileFlags & fi->dwFileFlagsMask & VS_FF_DEBUG), "not a debug build");
    t_note("%s FileVersion %u.%u.%u.%u (reference %s)", g_xm[i].dll, HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS), HIWORD(fi->dwFileVersionLS), LOWORD(fi->dwFileVersionLS), g_xm[i].ver);
    WORD *tr = NULL; UINT tl = 0;
    if (!q(blob, L"\\VarFileInfo\\Translation", (LPVOID *)&tr, &tl) || tl < 4) { XC(0, "translation table"); HeapFree(GetProcessHeap(), 0, blob); return; }
    if (tr[0] != 0x0409 || tr[1] != 1200) t_note("%s: first translation lang 0x%04x codepage %u (locale dependent)", g_xm[i].dll, tr[0], tr[1]);
    WCHAR s[160];
    char want[64];
    if (verstr(blob, tr, "FileVersion", s, 160)) {
        snprintf(want, sizeof want, "%u.%u.%u.%u", HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS), HIWORD(fi->dwFileVersionLS), LOWORD(fi->dwFileVersionLS));
        WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, want, -1, w, 64);
        XC(!wcsncmp(s, w, wcslen(w)), "FileVersion string \"%ls\" starts with %s", s, want);
    } else XC(0, "FileVersion string present");
    if (verstr(blob, tr, "ProductVersion", s, 160)) {
        XC(!wcsncmp(s, L"10.0.", 5), "ProductVersion string \"%ls\"", s);
    } else XC(0, "ProductVersion string present");
    if (verstr(blob, tr, "CompanyName", s, 160)) XC(!wcscmp(s, L"Microsoft Corporation"), "CompanyName \"%ls\"", s);
    else XC(0, "CompanyName present");
    if (verstr(blob, tr, "ProductName", s, 160)) XC(wcsstr(s, L"Windows") != NULL, "ProductName \"%ls\"", s);
    else XC(0, "ProductName present");
    if (verstr(blob, tr, "OriginalFilename", s, 160)) {
        WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, g_xm[i].dll, -1, w, 64);
        XC(!_wcsicmp(s, w), "OriginalFilename \"%ls\" vs %s", s, g_xm[i].dll);
    } else XC(0, "OriginalFilename present");
    if (verstr(blob, tr, "InternalName", s, 160)) {
        char stm[64]; stem(g_xm[i].dll, stm, sizeof stm);
        WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, stm, -1, w, 64);
        if (_wcsicmp(s, w) && _wcsnicmp(s, w, wcslen(w))) t_note("%s InternalName \"%ls\"", g_xm[i].dll, s);
    } else XC(0, "InternalName present");
    if (verstr(blob, tr, "FileDescription", s, 160)) { XC(wcslen(s) > 3, "FileDescription non-empty"); t_note("%s FileDescription \"%ls\"", g_xm[i].dll, s); }
    else XC(0, "FileDescription present");
    if (verstr(blob, tr, "LegalCopyright", s, 160)) XC(wcsstr(s, L"Microsoft") != NULL, "LegalCopyright \"%ls\"", s);
    else XC(0, "LegalCopyright present");
    HeapFree(GetProcessHeap(), 0, blob);
}
XM_FOR_ALL(version_resource, p_version)

/* ---- 32-bit counterparts -------------------------------------------------------------------- */
TEST(exports_modules_wow64_counterparts_are_x86) {
    char root[MAX_PATH], p[MAX_PATH * 2];
    UINT n = GetSystemWow64DirectoryA(root, sizeof root);
    if (!n) { t_skip("no SysWOW64 (32-bit Windows or no WOW64)"); return; }
    for (int i = 0; i < XM_COUNT; i++) {
        snprintf(p, sizeof p, "%s\\%s", root, g_xm[i].dll);
        HANDLE f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE) { t_note("%s: no 32-bit counterpart in SysWOW64", g_xm[i].dll); continue; }
        BYTE buf[0x400]; DWORD got = 0;
        ReadFile(f, buf, sizeof buf, &got, NULL); CloseHandle(f);
        const IMAGE_DOS_HEADER *d = (const IMAGE_DOS_HEADER *)buf;
        if (got < 0x200 || d->e_magic != IMAGE_DOS_SIGNATURE || d->e_lfanew + 0x80 > got) { CHECKF(0, "%s: unreadable header", p); continue; }
        const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(buf + d->e_lfanew);
        CHECKF(nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386, "%s machine 0x%x", p, nt->FileHeader.Machine);
        CHECKF(nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC, "%s magic", p);
        CHECKF(nt->FileHeader.Characteristics & IMAGE_FILE_DLL, "%s DLL flag", p);
    }
}
