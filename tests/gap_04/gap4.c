#include "t.h"
/* Internal exports: d3dscache!ShaderCache_GetDesc (function) and d3d10warp!VSD3DDebugConnectionBuffer (data). */
UDECL(void, ShaderCache_GetDesc, (void *cache, void *desc));
UDECL(HRESULT, ShaderCache_Create, (void *desc, void **out));
UDECL(HRESULT, ShaderCache_Destroy, (void *cache, unsigned flags));

#define DESC_QWORDS 8
/* Fake object: GetDesc copies 0x40 bytes (8 qwords) from object+0x20 to the output (measured on Win10 22H2: output bytes
 * 0..0x3f are written, nothing beyond). */
static void fill_obj(unsigned char *o, size_t n) { for (size_t i = 0; i < n; i++) o[i] = (unsigned char)(i * 7 + 3); }

TEST(gap4_getdesc_resolves) {
    FARPROC p = t_proc_in("d3dscache.dll", "ShaderCache_GetDesc");
    REQUIRE(p);
    CHECK(p == GetProcAddress(t_module("d3dscache.dll"), "ShaderCache_GetDesc"));
    CHECK(p == (FARPROC)UN(ShaderCache_GetDesc));
}
TEST(gap4_getdesc_not_in_other_modules) {
    HMODULE m = t_module("d3d11.dll");
    REQUIRE(m);
    SET_ERR(0);
    CHECK(GetProcAddress(m, "ShaderCache_GetDesc") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(gap4_getdesc_same_module_family) {
    HMODULE m = t_module("d3dscache.dll");
    REQUIRE(m);
    CHECK(GetProcAddress(m, "ShaderCache_GetDesc") != NULL);
    CHECK(GetProcAddress(m, "ShaderCache_AddValue") != NULL);
    CHECK(GetProcAddress(m, "ShaderCache_Destroy") != NULL);
    CHECK(GetProcAddress(m, "ShaderCache_GetDesc") != GetProcAddress(m, "ShaderCache_Destroy"));
}
TEST(gap4_getdesc_null_cache_leaves_output) {
    ShaderCache_GetDesc_pfn f = (ShaderCache_GetDesc_pfn)t_proc_in("d3dscache.dll", "ShaderCache_GetDesc");
    REQUIRE(f);
    unsigned char out[0x40]; memset(out, 0xAB, sizeof out);
    f(NULL, out);
    for (int i = 0; i < 0x40; i++) CHECK(out[i] == 0xAB);
}
TEST(gap4_getdesc_null_output_no_crash) {
    ShaderCache_GetDesc_pfn f = (ShaderCache_GetDesc_pfn)t_proc_in("d3dscache.dll", "ShaderCache_GetDesc");
    REQUIRE(f);
    unsigned char obj[0x80]; fill_obj(obj, sizeof obj);
    f(obj, NULL);
    f(NULL, NULL);
    CHECK(1);
}
TEST(gap4_getdesc_copies_64_bytes_from_offset_0x20) {
    ShaderCache_GetDesc_pfn f = (ShaderCache_GetDesc_pfn)t_proc_in("d3dscache.dll", "ShaderCache_GetDesc");
    REQUIRE(f);
    unsigned char obj[0x80]; fill_obj(obj, sizeof obj);
    unsigned char out[0x48]; memset(out, 0xEE, sizeof out);
    f(obj, out);
    CHECK(memcmp(out, obj + 0x20, 0x40) == 0);
    for (int i = 0x40; i < 0x48; i++) CHECK(out[i] == 0xEE);   /* nothing written past the 8 qwords */
}
TEST(gap4_getdesc_does_not_modify_object) {
    ShaderCache_GetDesc_pfn f = (ShaderCache_GetDesc_pfn)t_proc_in("d3dscache.dll", "ShaderCache_GetDesc");
    REQUIRE(f);
    unsigned char obj[0x80], ref[0x80], out[0x40];
    fill_obj(obj, sizeof obj); memcpy(ref, obj, sizeof obj);
    f(obj, out);
    CHECK(memcmp(obj, ref, sizeof obj) == 0);
}
TEST(gap4_getdesc_each_qword_position) {
    ShaderCache_GetDesc_pfn f = (ShaderCache_GetDesc_pfn)t_proc_in("d3dscache.dll", "ShaderCache_GetDesc");
    REQUIRE(f);
    for (int q = 0; q < DESC_QWORDS; q++) {
        unsigned long long obj[12] = {0}, out[DESC_QWORDS] = {0};
        obj[4 + q] = 0x1122334455667788ULL + q;       /* offset 0x20 + 8*q */
        f(obj, out);
        for (int k = 0; k < DESC_QWORDS; k++)
            CHECK(out[k] == (k == q ? obj[4 + q] : 0));
    }
}
TEST(gap4_getdesc_fields_before_0x20_ignored) {
    ShaderCache_GetDesc_pfn f = (ShaderCache_GetDesc_pfn)t_proc_in("d3dscache.dll", "ShaderCache_GetDesc");
    REQUIRE(f);
    unsigned long long obj[12], out[DESC_QWORDS];
    memset(obj, 0xFF, sizeof obj); memset(&obj[4], 0, 0x40);
    memset(out, 0x55, sizeof out);
    f(obj, out);
    for (int k = 0; k < DESC_QWORDS; k++) CHECK(out[k] == 0);
}
TEST(gap4_getdesc_repeatable) {
    ShaderCache_GetDesc_pfn f = (ShaderCache_GetDesc_pfn)t_proc_in("d3dscache.dll", "ShaderCache_GetDesc");
    REQUIRE(f);
    unsigned char obj[0x80]; fill_obj(obj, sizeof obj);
    unsigned char a[0x40], b[0x40];
    f(obj, a); f(obj, b);
    CHECK(memcmp(a, b, sizeof a) == 0);
}
TEST(gap4_cache_create_destroy_null_args) {
    ShaderCache_Create_pfn c = (ShaderCache_Create_pfn)t_proc_in("d3dscache.dll", "ShaderCache_Create");
    ShaderCache_Destroy_pfn d = (ShaderCache_Destroy_pfn)t_proc_in("d3dscache.dll", "ShaderCache_Destroy");
    REQUIRE(c); REQUIRE(d);
    void *out = (void *)(ULONG_PTR)1;
    unsigned char desc[0x40] = {0};
    EXPECT_EQ((unsigned)c(NULL, &out), 0x80070057u);
    EXPECT_EQ((unsigned)c(desc, NULL), 0x80070057u);
    EXPECT_EQ((unsigned)c(NULL, NULL), 0x80070057u);
    CHECK(out == (void *)(ULONG_PTR)1);
    EXPECT_EQ((unsigned)d(NULL, 0), 0x80070057u);
}

/* ---- d3d10warp!VSD3DDebugConnectionBuffer: a data symbol (?VSD3DDebugConnectionBuffer@@3UWarpConnectionDebugBuffer@@A) ---- */
static const IMAGE_SECTION_HEADER *sect_of(HMODULE m, void *p) {
    IMAGE_DOS_HEADER *dh = (void *)m; IMAGE_NT_HEADERS *nt = (void *)((char *)m + dh->e_lfanew);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    DWORD rva = (DWORD)((char *)p - (char *)m);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD sz = s[i].Misc.VirtualSize > s[i].SizeOfRawData ? s[i].Misc.VirtualSize : s[i].SizeOfRawData;
        if (rva >= s[i].VirtualAddress && rva < s[i].VirtualAddress + sz) return &s[i];
    }
    return NULL;
}
TEST(gap4_vsd_resolves) {
    HMODULE m = t_module("d3d10warp.dll");
    REQUIRE(m);
    FARPROC p = GetProcAddress(m, "VSD3DDebugConnectionBuffer");
    REQUIRE(p);
    CHECK(p == t_proc_in("d3d10warp.dll", "VSD3DDebugConnectionBuffer"));
}
TEST(gap4_vsd_is_data_not_code) {
    HMODULE m = t_module("d3d10warp.dll");
    REQUIRE(m);
    FARPROC p = GetProcAddress(m, "VSD3DDebugConnectionBuffer");
    REQUIRE(p);
    const IMAGE_SECTION_HEADER *s = sect_of(m, (void *)p);
    REQUIRE(s);
    CHECK((s->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0);
    CHECK((s->Characteristics & IMAGE_SCN_MEM_WRITE) != 0);   /* a mutable global buffer */
    CHECK((s->Characteristics & IMAGE_SCN_CNT_CODE) == 0);
}
TEST(gap4_vsd_inside_module_image_not_forwarded) {
    HMODULE m = t_module("d3d10warp.dll");
    REQUIRE(m);
    FARPROC p = GetProcAddress(m, "VSD3DDebugConnectionBuffer");
    REQUIRE(p);
    CHECK((char *)p > (char *)m);
    CHECK(GetModuleHandleA("d3d10warp.dll") == m);
    HMODULE owner = NULL;
    CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner));
    CHECK(owner == m);
}
TEST(gap4_vsd_memory_is_readable_committed) {
    HMODULE m = t_module("d3d10warp.dll");
    REQUIRE(m);
    FARPROC p = GetProcAddress(m, "VSD3DDebugConnectionBuffer");
    REQUIRE(p);
    MEMORY_BASIC_INFORMATION mbi;
    REQUIRE(VirtualQuery((void *)p, &mbi, sizeof mbi) == sizeof mbi);
    CHECK(mbi.State == MEM_COMMIT);
    CHECK(mbi.Type == MEM_IMAGE);
    CHECK((mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) != 0);
    CHECK((mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0);
}
TEST(gap4_vsd_absent_from_other_modules) {
    const char *mods[] = { "d3d11.dll", "d3d10core.dll", "dxgi.dll", "d3dscache.dll", "d3d10_1.dll" };
    for (unsigned i = 0; i < sizeof mods / sizeof *mods; i++) {
        HMODULE m = t_module(mods[i]);
        if (!m) continue;
        SET_ERR(0);
        CHECK(GetProcAddress(m, "VSD3DDebugConnectionBuffer") == NULL);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(gap4_vsd_neighbour_exports_present) {
    HMODULE m = t_module("d3d10warp.dll");
    REQUIRE(m);
    const char *n[] = { "D3D11RefGetLastCreation", "D3DLayerGetInterface", "OpenAdapter", "OpenAdapter10_2",
                        "OpenAdapter12", "OpenDisplayAdapter1", "QueryDListForApplication1" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECK(GetProcAddress(m, n[i]) != NULL);
}
TEST(gap4_vsd_stable_address) {
    HMODULE m = t_module("d3d10warp.dll");
    REQUIRE(m);
    FARPROC a = GetProcAddress(m, "VSD3DDebugConnectionBuffer");
    FARPROC b = GetProcAddress(m, "VSD3DDebugConnectionBuffer");
    CHECK(a != NULL); CHECK(a == b);
}
