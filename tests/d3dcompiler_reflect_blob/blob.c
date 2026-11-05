/* d3dcompiler_47: exports, ID3DBlob lifetime, D3DCreateBlob, file helpers, shader compression, DebugSetMute. */
#include "rb.h"

static const char *const rb_export_names[] = {
    "D3DAssemble", "D3DCompile", "D3DCompile2", "D3DCompileFromFile", "D3DCompressShaders", "D3DCreateBlob",
    "D3DCreateFunctionLinkingGraph", "D3DCreateLinker", "D3DDecompressShaders", "D3DDisassemble", "D3DDisassemble10Effect",
    "D3DDisassemble11Trace", "D3DDisassembleRegion", "D3DGetBlobPart", "D3DGetDebugInfo", "D3DGetInputAndOutputSignatureBlob",
    "D3DGetInputSignatureBlob", "D3DGetOutputSignatureBlob", "D3DGetTraceInstructionOffsets", "D3DLoadModule", "D3DPreprocess",
    "D3DReadFileToBlob", "D3DReflect", "D3DReflectLibrary", "D3DReturnFailure1", "D3DSetBlobPart", "D3DStripShader",
    "D3DWriteBlobToFile", "DebugSetMute"
};
/* ordinals of the Windows 10 d3dcompiler_47.dll export table */
static const struct { const char *n; unsigned ord; } rb_ordinals[] = {
    { "D3DAssemble", 1 }, { "DebugSetMute", 2 }, { "D3DCompile", 3 }, { "D3DCompile2", 4 }, { "D3DCompileFromFile", 5 },
    { "D3DCompressShaders", 6 }, { "D3DCreateBlob", 7 }, { "D3DCreateFunctionLinkingGraph", 8 }, { "D3DCreateLinker", 9 },
    { "D3DDecompressShaders", 10 }, { "D3DDisassemble", 11 }, { "D3DGetBlobPart", 15 }, { "D3DGetDebugInfo", 16 },
    { "D3DGetInputSignatureBlob", 18 }, { "D3DLoadModule", 21 }, { "D3DReadFileToBlob", 23 }, { "D3DReflect", 24 },
    { "D3DReflectLibrary", 25 }, { "D3DSetBlobPart", 27 }, { "D3DStripShader", 28 }, { "D3DWriteBlobToFile", 29 },
};

static void rb_wpath(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }
static int rb_write_file(const char *path, const void *data, DWORD n)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); DWORD got = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (n) WriteFile(h, data, n, &got, NULL);
    CloseHandle(h); return got == n;
}
static BYTE *rb_read_file(const char *path, DWORD *n)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); BYTE *b; DWORD sz, got = 0;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL); b = malloc(sz ? sz : 1);
    if (sz) ReadFile(h, b, sz, &got, NULL);
    CloseHandle(h); *n = got; return b;
}

TEST(rb_blob_all_named_exports_resolve)
{
    HMODULE m = t_module("d3dcompiler_47.dll");
    if (!m) { t_skip("d3dcompiler_47.dll not loadable"); return; }
    for (size_t i = 0; i < sizeof rb_export_names / sizeof *rb_export_names; i++)
        t_check(GetProcAddress(m, rb_export_names[i]) != NULL, __FILE__, __LINE__, "export present", "%s", rb_export_names[i]);
}
TEST(rb_blob_export_ordinals_match_windows)
{
    HMODULE m = t_module("d3dcompiler_47.dll");
    if (!m) { t_skip("no d3dcompiler_47"); return; }
    if (t_is_wine()) { t_skip("ordinal layout is specific to the Windows binary"); return; }
    for (size_t i = 0; i < sizeof rb_ordinals / sizeof *rb_ordinals; i++) {
        FARPROC a = GetProcAddress(m, rb_ordinals[i].n), b = GetProcAddress(m, (LPCSTR)(ULONG_PTR)rb_ordinals[i].ord);
        t_check(a != NULL && a == b, __FILE__, __LINE__, "ordinal resolves to the named export", "%s ord %u", rb_ordinals[i].n, rb_ordinals[i].ord);
    }
}
TEST(rb_blob_unknown_export_fails)
{
    HMODULE m = t_module("d3dcompiler_47.dll"); if (!m) { t_skip("no d3dcompiler_47"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, "D3DNoSuchFunction") == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    CHECK(GetProcAddress(m, "d3dcreateblob") == NULL); /* exports are case sensitive */
    CHECK(GetProcAddress(m, "D3DCreateBlobA") == NULL);
}
TEST(rb_blob_version_constants)
{
    CHECK(D3D_COMPILER_VERSION == 47);
    EXPECT_STR(D3DCOMPILER_DLL_A, "d3dcompiler_47.dll");
    EXPECT_EQ(D3DCOMPILER_STRIP_REFLECTION_DATA, 1); EXPECT_EQ(D3DCOMPILER_STRIP_DEBUG_INFO, 2); EXPECT_EQ(D3DCOMPILER_STRIP_TEST_BLOBS, 4);
    EXPECT_EQ(D3DCOMPILER_STRIP_PRIVATE_DATA, 8); EXPECT_EQ(D3DCOMPILER_STRIP_ROOT_SIGNATURE, 0x10);
    EXPECT_EQ(D3D_BLOB_INPUT_SIGNATURE_BLOB, 0); EXPECT_EQ(D3D_BLOB_DEBUG_INFO, 5); EXPECT_EQ(D3D_BLOB_PRIVATE_DATA, 10);
    EXPECT_EQ(D3D_BLOB_ROOT_SIGNATURE, 11); EXPECT_EQ(D3D_BLOB_DEBUG_NAME, 12); EXPECT_EQ(D3D_BLOB_TEST_ALTERNATE_SHADER, 0x8000);
    CHECK(IsEqualGUID(&IID_ID3D10Blob, &IID_ID3DBlob));
}
TEST(rb_blob_module_file_name_and_version)
{
    HMODULE m = t_module("d3dcompiler_47.dll"); char path[MAX_PATH]; DWORD n;
    if (!m) { t_skip("no d3dcompiler_47"); return; }
    n = GetModuleFileNameA(m, path, sizeof path);
    CHECK(n > 0 && n < sizeof path);
    CHECK(n >= 18 && !lstrcmpiA(path + n - 18, "d3dcompiler_47.dll"));
    {
        HMODULE v = t_module("version.dll");
        typedef DWORD (WINAPI *gsz_t)(LPCSTR, LPDWORD); typedef BOOL (WINAPI *gi_t)(LPCSTR, DWORD, DWORD, LPVOID); typedef BOOL (WINAPI *vq_t)(LPCVOID, LPCSTR, LPVOID *, PUINT);
        gsz_t gsz = v ? (gsz_t)GetProcAddress(v, "GetFileVersionInfoSizeA") : NULL; gi_t gi = v ? (gi_t)GetProcAddress(v, "GetFileVersionInfoA") : NULL; vq_t vq = v ? (vq_t)GetProcAddress(v, "VerQueryValueA") : NULL;
        if (gsz && gi && vq) {
            DWORD z = gsz(path, NULL); void *buf = z ? malloc(z) : NULL; VS_FIXEDFILEINFO *ffi = NULL; UINT len = 0;
            if (buf && gi(path, 0, z, buf) && vq(buf, "\\", (void **)&ffi, &len) && ffi) {
                t_note("d3dcompiler_47 file version %lu.%lu.%lu.%lu", ffi->dwFileVersionMS >> 16, ffi->dwFileVersionMS & 0xffff, ffi->dwFileVersionLS >> 16, ffi->dwFileVersionLS & 0xffff);
                if (!t_is_wine()) CHECK((ffi->dwFileVersionMS >> 16) == 10);
            }
            free(buf);
        }
    }
}

/* ---- D3DCreateBlob ------------------------------------------------------------------------------- */
TEST(rb_blob_create_null_out_is_invalidcall)
{
    REQUIRE(d3dh_D3DCreateBlob());
    EXPECT_HR(d3dh_D3DCreateBlob()(16, NULL), RB_INVALIDCALL);
    EXPECT_HR(d3dh_D3DCreateBlob()(0, NULL), RB_INVALIDCALL);
}
TEST(rb_blob_create_sizes_roundtrip)
{
    static const SIZE_T sizes[] = { 1, 2, 3, 4, 7, 8, 15, 16, 17, 31, 32, 63, 64, 65, 255, 256, 1000, 4096, 65535, 65536, 1048576 };
    REQUIRE(d3dh_D3DCreateBlob());
    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        ID3DBlob *b = NULL; HRESULT hr = d3dh_D3DCreateBlob()(sizes[i], &b);
        t_check(hr == S_OK && b, __FILE__, __LINE__, "create ok", "size %zu: %s", sizes[i], d3dh_hr_name(hr));
        if (!b) continue;
        t_check(ID3D10Blob_GetBufferSize(b) == sizes[i], __FILE__, __LINE__, "GetBufferSize", "size %zu got %zu", sizes[i], (size_t)ID3D10Blob_GetBufferSize(b));
        BYTE *p = ID3D10Blob_GetBufferPointer(b);
        CHECK(p != NULL);
        if (p) { memset(p, 0xA5, sizes[i]); p[0] = 1; p[sizes[i] - 1] ^= 0xff; CHECK(((BYTE *)ID3D10Blob_GetBufferPointer(b)) == p); }
        RB_REFS(Release, b, 0);
    }
}
TEST(rb_blob_create_size_zero)
{
    ID3DBlob *b = (ID3DBlob *)0x1; HRESULT hr;
    REQUIRE(d3dh_D3DCreateBlob());
    hr = d3dh_D3DCreateBlob()(0, &b);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && b) {
        EXPECT_EQ(ID3D10Blob_GetBufferSize(b), 0);
        t_note("zero-size blob buffer pointer = %p", ID3D10Blob_GetBufferPointer(b));
        RB_REFS(Release, b, 0);
    }
}
TEST(rb_blob_create_large_blob)
{
    ID3DBlob *b = NULL; HRESULT hr; const SIZE_T n = 48u << 20;
    REQUIRE(d3dh_D3DCreateBlob());
    hr = d3dh_D3DCreateBlob()(n, &b);
    EXPECT_HR(hr, S_OK);
    if (b) {
        BYTE *p = ID3D10Blob_GetBufferPointer(b);
        EXPECT_EQ(ID3D10Blob_GetBufferSize(b), n);
        if (p) { p[0] = 1; p[n / 2] = 2; p[n - 1] = 3; CHECK(p[0] == 1 && p[n / 2] == 2 && p[n - 1] == 3); }
        ID3D10Blob_Release(b);
    }
}
TEST(rb_blob_two_blobs_are_independent)
{
    ID3DBlob *a = NULL, *b = NULL;
    REQUIRE(d3dh_D3DCreateBlob());
    EXPECT_HR(d3dh_D3DCreateBlob()(32, &a), S_OK); EXPECT_HR(d3dh_D3DCreateBlob()(32, &b), S_OK);
    if (a && b) {
        CHECK(ID3D10Blob_GetBufferPointer(a) != ID3D10Blob_GetBufferPointer(b));
        CHECK(a != b);
        memset(ID3D10Blob_GetBufferPointer(a), 0x11, 32); memset(ID3D10Blob_GetBufferPointer(b), 0x22, 32);
        CHECK(((BYTE *)ID3D10Blob_GetBufferPointer(a))[31] == 0x11 && ((BYTE *)ID3D10Blob_GetBufferPointer(b))[0] == 0x22);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
TEST(rb_blob_refcount_protocol)
{
    ID3DBlob *b = NULL;
    REQUIRE(d3dh_D3DCreateBlob());
    EXPECT_HR(d3dh_D3DCreateBlob()(8, &b), S_OK);
    if (!b) return;
    RB_REFS(AddRef, b, 2); RB_REFS(AddRef, b, 3);
    RB_REFS(Release, b, 2); RB_REFS(Release, b, 1);
    RB_REFS(Release, b, 0);
}
TEST(rb_blob_query_interface)
{
    ID3DBlob *b = NULL; void *o;
    REQUIRE(d3dh_D3DCreateBlob());
    EXPECT_HR(d3dh_D3DCreateBlob()(8, &b), S_OK);
    if (!b) return;
    o = NULL; EXPECT_HR(ID3D10Blob_QueryInterface(b, &IID_IUnknown, &o), S_OK); CHECK(o == (void *)b); RB_REFS(Release, b, 1);
    o = NULL; EXPECT_HR(ID3D10Blob_QueryInterface(b, &IID_ID3D10Blob, &o), S_OK); CHECK(o == (void *)b); RB_REFS(Release, b, 1);
    o = NULL; EXPECT_HR(ID3D10Blob_QueryInterface(b, &IID_ID3DBlob, &o), S_OK); CHECK(o == (void *)b); RB_REFS(Release, b, 1);
    o = (void *)1; EXPECT_HR(ID3D10Blob_QueryInterface(b, &IID_IDXGIObject, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
    o = (void *)1; EXPECT_HR(ID3D10Blob_QueryInterface(b, &IID_ID3D11Device, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
    o = (void *)1; EXPECT_HR(ID3D10Blob_QueryInterface(b, &IID_IClassFactory, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
    RB_REFS(Release, b, 0);
}
TEST(rb_blob_pointer_stable_across_refs)
{
    ID3DBlob *b = NULL;
    REQUIRE(d3dh_D3DCreateBlob());
    EXPECT_HR(d3dh_D3DCreateBlob()(100, &b), S_OK);
    if (!b) return;
    void *p0 = ID3D10Blob_GetBufferPointer(b); ID3D10Blob_AddRef(b);
    CHECK(ID3D10Blob_GetBufferPointer(b) == p0); CHECK(ID3D10Blob_GetBufferSize(b) == 100);
    ID3D10Blob_Release(b); ID3D10Blob_Release(b);
}
TEST(rb_blob_compiled_shader_blob_properties)
{
    RB_COMPILE(b, RB_PS_CONST, "ps_5_0");
    CHECK(RB_BS(b) >= 0x20 + 4);
    CHECK(!memcmp(RB_BP(b), "DXBC", 4));
    EXPECT_EQ(*(const DWORD *)(RB_BP(b) + 0x18), RB_BS(b));
    EXPECT_EQ(*(const WORD *)(RB_BP(b) + 0x14), 1);
    EXPECT_EQ(*(const WORD *)(RB_BP(b) + 0x16), 0);
    EXPECT_EQ(RB_BS(b) % 4, 0);
    {
        rb_chunk c[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS);
        CHECK(n >= 3);
        CHECK(rb_find(c, n, FCC_SHDR) || rb_find(c, n, FCC_SHEX));
        CHECK(rb_find(c, n, FCC_ISGN) != NULL); CHECK(rb_find(c, n, FCC_OSGN) != NULL);
        CHECK(rb_find(c, n, FCC_RDEF) != NULL);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_blob_container_hash_matches_own_algorithm)
{
    RB_COMPILE(b, RB_PS_CONST, "ps_5_0");
    BYTE *cp = malloc(RB_BS(b));
    memcpy(cp, RB_BP(b), RB_BS(b));
    memset(cp + 4, 0, 16);
    rb_dxbc_rehash(cp, RB_BS(b));
    CHECK(!memcmp(cp + 4, RB_BP(b) + 4, 16));
    free(cp);
    ID3D10Blob_Release(b);
}

/* ---- D3DReadFileToBlob / D3DWriteBlobToFile ------------------------------------------------------ */
TEST(rb_blob_readfile_roundtrip_sizes)
{
    static const DWORD sizes[] = { 0, 1, 3, 4, 100, 4096, 70000 };
    REQUIRE(d3dh_D3DCreateBlob());
    REQUIRE(UIN("d3dcompiler_47.dll", D3DReadFileToBlob));
    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        char path[MAX_PATH]; WCHAR w[MAX_PATH]; BYTE *data = malloc(sizes[i] + 1); ID3DBlob *b = NULL; HRESULT hr;
        for (DWORD k = 0; k < sizes[i]; k++) data[k] = (BYTE)(k * 7 + i);
        t_tmpfile(path, sizeof path, "rf.bin"); rb_wpath(path, w, MAX_PATH);
        CHECK(rb_write_file(path, data, sizes[i]));
        hr = UIN("d3dcompiler_47.dll", D3DReadFileToBlob)(w, &b);
        t_check(hr == S_OK && b, __FILE__, __LINE__, "read ok", "size %lu: %s", sizes[i], d3dh_hr_name(hr));
        if (b) {
            t_check(ID3D10Blob_GetBufferSize(b) == sizes[i], __FILE__, __LINE__, "size", "want %lu got %zu", sizes[i], (size_t)ID3D10Blob_GetBufferSize(b));
            if (sizes[i]) CHECK(!memcmp(ID3D10Blob_GetBufferPointer(b), data, sizes[i]));
            ID3D10Blob_Release(b);
        }
        free(data); DeleteFileA(path);
    }
}
TEST(rb_blob_readfile_missing_file)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = (ID3DBlob *)0x1234;
    REQUIRE(UIN("d3dcompiler_47.dll", D3DReadFileToBlob));
    t_tmpfile(path, sizeof path, "does_not_exist.bin"); rb_wpath(path, w, MAX_PATH);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DReadFileToBlob)(w, &b), HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    CHECK(b == (ID3DBlob *)0x1234); /* output untouched on failure */
}
TEST(rb_blob_readfile_missing_directory)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL;
    REQUIRE(UIN("d3dcompiler_47.dll", D3DReadFileToBlob));
    t_tmpfile(path, sizeof path, "no_such_dir\\x.bin"); rb_wpath(path, w, MAX_PATH);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DReadFileToBlob)(w, &b), HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
    CHECK(b == NULL);
}
TEST(rb_blob_readfile_directory_fails)
{
    WCHAR w[MAX_PATH]; ID3DBlob *b = NULL; HRESULT hr;
    REQUIRE(UIN("d3dcompiler_47.dll", D3DReadFileToBlob));
    rb_wpath(t_tmpdir(), w, MAX_PATH);
    hr = UIN("d3dcompiler_47.dll", D3DReadFileToBlob)(w, &b);
    EXPECT_HR_FAILED(hr); CHECK(b == NULL);
    if (hr != HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) t_note("directory read returned %s", d3dh_hr_name(hr));
    SAFE_RELEASE(b);
}
TEST(rb_blob_readfile_sharing_violation_with_writer)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL; HANDLE h;
    REQUIRE(UIN("d3dcompiler_47.dll", D3DReadFileToBlob));
    t_tmpfile(path, sizeof path, "locked.bin"); rb_wpath(path, w, MAX_PATH);
    CHECK(rb_write_file(path, "abcd", 4));
    h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        /* the reader asks for FILE_SHARE_READ only, which a live writer conflicts with */
        EXPECT_HR(UIN("d3dcompiler_47.dll", D3DReadFileToBlob)(w, &b), HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION));
        CloseHandle(h);
        EXPECT_HR(UIN("d3dcompiler_47.dll", D3DReadFileToBlob)(w, &b), S_OK);
        SAFE_RELEASE(b);
    }
    DeleteFileA(path);
}
TEST(rb_blob_readfile_allows_other_readers)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL; HANDLE h;
    REQUIRE(UIN("d3dcompiler_47.dll", D3DReadFileToBlob));
    t_tmpfile(path, sizeof path, "shared.bin"); rb_wpath(path, w, MAX_PATH);
    CHECK(rb_write_file(path, "abcd", 4));
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DReadFileToBlob)(w, &b), S_OK);
    SAFE_RELEASE(b);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    DeleteFileA(path);
}
TEST(rb_blob_writefile_new_file)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL; DWORD n = 0; BYTE *rd;
    REQUIRE(d3dh_D3DCreateBlob()); REQUIRE(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile));
    t_tmpfile(path, sizeof path, "wr_new.bin"); rb_wpath(path, w, MAX_PATH); DeleteFileA(path);
    EXPECT_HR(d3dh_D3DCreateBlob()(10, &b), S_OK); if (!b) return;
    memcpy(ID3D10Blob_GetBufferPointer(b), "0123456789", 10);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(b, w, FALSE), S_OK);
    rd = rb_read_file(path, &n);
    CHECK(rd && n == 10 && !memcmp(rd, "0123456789", 10)); free(rd);
    ID3D10Blob_Release(b); DeleteFileA(path);
}
TEST(rb_blob_writefile_existing_without_overwrite)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL; DWORD n = 0; BYTE *rd;
    REQUIRE(d3dh_D3DCreateBlob()); REQUIRE(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile));
    t_tmpfile(path, sizeof path, "wr_exist.bin"); rb_wpath(path, w, MAX_PATH);
    CHECK(rb_write_file(path, "OLDOLDOLD", 9));
    EXPECT_HR(d3dh_D3DCreateBlob()(3, &b), S_OK); if (!b) return;
    memcpy(ID3D10Blob_GetBufferPointer(b), "new", 3);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(b, w, FALSE), HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));
    rd = rb_read_file(path, &n); CHECK(rd && n == 9 && !memcmp(rd, "OLDOLDOLD", 9)); free(rd);
    ID3D10Blob_Release(b); DeleteFileA(path);
}
TEST(rb_blob_writefile_overwrite_truncates)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL; DWORD n = 0; BYTE *rd;
    REQUIRE(d3dh_D3DCreateBlob()); REQUIRE(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile));
    t_tmpfile(path, sizeof path, "wr_over.bin"); rb_wpath(path, w, MAX_PATH);
    CHECK(rb_write_file(path, "OLDOLDOLD", 9));
    EXPECT_HR(d3dh_D3DCreateBlob()(3, &b), S_OK); if (!b) return;
    memcpy(ID3D10Blob_GetBufferPointer(b), "new", 3);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(b, w, TRUE), S_OK);
    rd = rb_read_file(path, &n); CHECK(rd && n == 3 && !memcmp(rd, "new", 3)); free(rd);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(b, w, 2), S_OK); /* any non-zero value means overwrite */
    ID3D10Blob_Release(b); DeleteFileA(path);
}
TEST(rb_blob_writefile_empty_blob_creates_empty_file)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL; DWORD n = 99; BYTE *rd;
    REQUIRE(d3dh_D3DCreateBlob()); REQUIRE(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile));
    t_tmpfile(path, sizeof path, "wr_empty.bin"); rb_wpath(path, w, MAX_PATH); DeleteFileA(path);
    EXPECT_HR(d3dh_D3DCreateBlob()(0, &b), S_OK); if (!b) return;
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(b, w, FALSE), S_OK);
    rd = rb_read_file(path, &n); CHECK(rd != NULL && n == 0); free(rd);
    ID3D10Blob_Release(b); DeleteFileA(path);
}
TEST(rb_blob_writefile_bad_directory)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL;
    REQUIRE(d3dh_D3DCreateBlob()); REQUIRE(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile));
    t_tmpfile(path, sizeof path, "missing_dir\\x.bin"); rb_wpath(path, w, MAX_PATH);
    EXPECT_HR(d3dh_D3DCreateBlob()(4, &b), S_OK); if (!b) return;
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(b, w, TRUE), HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(b, w, FALSE), HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
    ID3D10Blob_Release(b);
}
TEST(rb_blob_writefile_read_only_target)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *b = NULL;
    REQUIRE(d3dh_D3DCreateBlob()); REQUIRE(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile));
    t_tmpfile(path, sizeof path, "wr_ro.bin"); rb_wpath(path, w, MAX_PATH);
    CHECK(rb_write_file(path, "keep", 4));
    SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY);
    EXPECT_HR(d3dh_D3DCreateBlob()(4, &b), S_OK); if (!b) { SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL); return; }
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(b, w, TRUE), HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    ID3D10Blob_Release(b); DeleteFileA(path);
}
TEST(rb_blob_write_then_read_shader_roundtrip)
{
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; ID3DBlob *back = NULL;
    RB_COMPILE(sh, RB_PS_CONST, "ps_5_0");
    REQUIRE(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)); REQUIRE(UIN("d3dcompiler_47.dll", D3DReadFileToBlob));
    t_tmpfile(path, sizeof path, "shader.cso"); rb_wpath(path, w, MAX_PATH);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DWriteBlobToFile)(sh, w, TRUE), S_OK);
    EXPECT_HR(UIN("d3dcompiler_47.dll", D3DReadFileToBlob)(w, &back), S_OK);
    if (back) { CHECK(RB_BS(back) == RB_BS(sh)); CHECK(RB_BS(back) == RB_BS(sh) && !memcmp(RB_BP(back), RB_BP(sh), RB_BS(sh))); ID3D10Blob_Release(back); }
    ID3D10Blob_Release(sh); DeleteFileA(path);
}

/* ---- D3DCompressShaders / D3DDecompressShaders --------------------------------------------------- */
static int rb_two_shaders(ID3DBlob **vs, ID3DBlob **ps)
{
    return rb_compile_t("float4 main(float4 p : POSITION, float2 uv : TEXCOORD0) : SV_Position { return p + float4(uv, 0, 0); }", "vs_5_0", 0, vs) &&
           rb_compile_t("Texture2D t : register(t0); SamplerState s : register(s0); float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { return t.Sample(s, uv); }", "ps_5_0", 0, ps);
}
TEST(rb_blob_compress_roundtrip_keep_all)
{
    ID3DBlob *vs, *ps, *packed = NULL; RB_SHADER_DATA sd[2]; ID3DBlob *out[2] = { 0, 0 }; UINT total = 0; HRESULT hr;
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders"); REQUIRE(RB_D3DDecompressShaders());
    if (!rb_two_shaders(&vs, &ps)) return;
    sd[0].pBytecode = RB_BP(vs); sd[0].BytecodeLength = RB_BS(vs); sd[1].pBytecode = RB_BP(ps); sd[1].BytecodeLength = RB_BS(ps);
    hr = RB_D3DCompressShaders()(2, sd, 1, &packed);
    EXPECT_HR(hr, S_OK);
    if (packed) {
        t_note("compressed container %zu bytes for %zu+%zu", (size_t)RB_BS(packed), (size_t)RB_BS(vs), (size_t)RB_BS(ps));
        CHECK(RB_BS(packed) >= 0x20);
        hr = RB_D3DDecompressShaders()(RB_BP(packed), RB_BS(packed), 2, 0, NULL, 0, out, &total);
        EXPECT_HR(hr, S_OK); EXPECT_EQ(total, 2);
        if (out[0]) { CHECK(RB_BS(out[0]) == RB_BS(vs) && !memcmp(RB_BP(out[0]), RB_BP(vs), RB_BS(vs))); ID3D10Blob_Release(out[0]); }
        if (out[1]) { CHECK(RB_BS(out[1]) == RB_BS(ps) && !memcmp(RB_BP(out[1]), RB_BP(ps), RB_BS(ps))); ID3D10Blob_Release(out[1]); }
        ID3D10Blob_Release(packed);
    }
    ID3D10Blob_Release(vs); ID3D10Blob_Release(ps);
}
TEST(rb_blob_compress_default_strips_reflection)
{
    ID3DBlob *vs, *ps, *packed = NULL; RB_SHADER_DATA sd[2]; ID3DBlob *out[2] = { 0, 0 }; UINT total = 0; HRESULT hr;
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders"); REQUIRE(RB_D3DDecompressShaders());
    if (!rb_two_shaders(&vs, &ps)) return;
    sd[0].pBytecode = RB_BP(vs); sd[0].BytecodeLength = RB_BS(vs); sd[1].pBytecode = RB_BP(ps); sd[1].BytecodeLength = RB_BS(ps);
    EXPECT_HR(hr = RB_D3DCompressShaders()(2, sd, 0, &packed), S_OK);
    if (packed) {
        EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), RB_BS(packed), 2, 0, NULL, 0, out, &total), S_OK);
        for (int i = 0; i < 2; i++) {
            rb_chunk c[RB_MAXCHUNKS]; int n;
            if (!out[i]) continue;
            n = rb_dxbc_parse(RB_BP(out[i]), RB_BS(out[i]), c, RB_MAXCHUNKS);
            CHECK(n > 0);
            if (n > 0) {
                CHECK(rb_find(c, n, FCC_SHEX) || rb_find(c, n, FCC_SHDR));
                CHECK(rb_find(c, n, FCC_ISGN) != NULL);
                CHECK(rb_find(c, n, FCC_RDEF) == NULL); CHECK(rb_find(c, n, FCC_STAT) == NULL); CHECK(rb_find(c, n, FCC_SDBG) == NULL);
            }
            ID3D10Blob_Release(out[i]);
        }
        ID3D10Blob_Release(packed);
    }
    ID3D10Blob_Release(vs); ID3D10Blob_Release(ps);
}
TEST(rb_blob_decompress_selects_by_index)
{
    ID3DBlob *vs, *ps, *packed = NULL; RB_SHADER_DATA sd[2]; ID3DBlob *out[2] = { 0, 0 }; UINT total = 0, idx[2] = { 1, 0 };
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders"); REQUIRE(RB_D3DDecompressShaders());
    if (!rb_two_shaders(&vs, &ps)) return;
    sd[0].pBytecode = RB_BP(vs); sd[0].BytecodeLength = RB_BS(vs); sd[1].pBytecode = RB_BP(ps); sd[1].BytecodeLength = RB_BS(ps);
    EXPECT_HR(RB_D3DCompressShaders()(2, sd, 1, &packed), S_OK);
    if (packed) {
        /* explicit index list reverses the order */
        EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), RB_BS(packed), 2, 0, idx, 0, out, &total), S_OK);
        if (out[0]) { CHECK(RB_BS(out[0]) == RB_BS(ps) && !memcmp(RB_BP(out[0]), RB_BP(ps), RB_BS(ps))); ID3D10Blob_Release(out[0]); out[0] = NULL; }
        if (out[1]) { CHECK(RB_BS(out[1]) == RB_BS(vs) && !memcmp(RB_BP(out[1]), RB_BP(vs), RB_BS(vs))); ID3D10Blob_Release(out[1]); out[1] = NULL; }
        /* a start index selects a run of shaders */
        EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), RB_BS(packed), 1, 1, NULL, 0, out, &total), S_OK);
        if (out[0]) { CHECK(RB_BS(out[0]) == RB_BS(ps) && !memcmp(RB_BP(out[0]), RB_BP(ps), RB_BS(ps))); ID3D10Blob_Release(out[0]); }
        ID3D10Blob_Release(packed);
    }
    ID3D10Blob_Release(vs); ID3D10Blob_Release(ps);
}
TEST(rb_blob_decompress_total_pointer_optional)
{
    ID3DBlob *vs, *ps, *packed = NULL; RB_SHADER_DATA sd[1]; ID3DBlob *out[1] = { 0 };
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders"); REQUIRE(RB_D3DDecompressShaders());
    if (!rb_two_shaders(&vs, &ps)) return;
    sd[0].pBytecode = RB_BP(vs); sd[0].BytecodeLength = RB_BS(vs);
    EXPECT_HR(RB_D3DCompressShaders()(1, sd, 1, &packed), S_OK);
    if (packed) {
        EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), RB_BS(packed), 1, 0, NULL, 0, out, NULL), S_OK);
        SAFE_RELEASE(out[0]); ID3D10Blob_Release(packed);
    }
    ID3D10Blob_Release(vs); ID3D10Blob_Release(ps);
}
TEST(rb_blob_compress_invalid_arguments)
{
    ID3DBlob *vs, *ps, *packed = (ID3DBlob *)0x1; RB_SHADER_DATA sd[1];
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders");
    if (!rb_two_shaders(&vs, &ps)) return;
    sd[0].pBytecode = RB_BP(vs); sd[0].BytecodeLength = RB_BS(vs);
    EXPECT_HR(RB_D3DCompressShaders()(1, sd, 1, NULL), RB_E_FAIL);                 /* NULL output */
    EXPECT_HR(RB_D3DCompressShaders()(1, sd, 2, &packed), RB_E_FAIL);              /* only bit 0 is defined */
    CHECK(packed == NULL);                                                          /* output is cleared before the flag check */
    packed = (ID3DBlob *)0x1; EXPECT_HR(RB_D3DCompressShaders()(1, sd, 0x80000000u, &packed), RB_E_FAIL); CHECK(packed == NULL);
    ID3D10Blob_Release(vs); ID3D10Blob_Release(ps);
}
TEST(rb_blob_compress_rejects_non_dxbc_input)
{
    static const BYTE junk[64] = { 1, 2, 3 }; RB_SHADER_DATA sd = { junk, sizeof junk }; ID3DBlob *packed = NULL; HRESULT hr;
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders");
    hr = RB_D3DCompressShaders()(1, &sd, 0, &packed);
    EXPECT_HR_FAILED(hr); CHECK(packed == NULL);
    SAFE_RELEASE(packed);
}
TEST(rb_blob_decompress_invalid_arguments)
{
    ID3DBlob *vs, *ps, *packed = NULL; RB_SHADER_DATA sd[1]; ID3DBlob *out[1] = { 0 }; UINT total = 0; BYTE *cp;
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders"); REQUIRE(RB_D3DDecompressShaders());
    if (!rb_two_shaders(&vs, &ps)) return;
    sd[0].pBytecode = RB_BP(vs); sd[0].BytecodeLength = RB_BS(vs);
    EXPECT_HR(RB_D3DCompressShaders()(1, sd, 1, &packed), S_OK);
    if (!packed) { ID3D10Blob_Release(vs); ID3D10Blob_Release(ps); return; }
    EXPECT_HR(RB_D3DDecompressShaders()(NULL, 100, 1, 0, NULL, 0, out, &total), RB_E_FAIL);
    EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), RB_BS(packed), 1, 0, NULL, 0, NULL, &total), RB_E_FAIL);
    EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), RB_BS(packed), 1, 0, NULL, 1, out, &total), RB_E_FAIL);       /* flags must be 0 */
    EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), 0x1f, 1, 0, NULL, 0, out, &total), RB_E_FAIL);               /* smaller than the header */
    EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), 0x80000000ull, 1, 0, NULL, 0, out, &total), RB_E_FAIL);    /* size above INT_MAX */
    cp = malloc(RB_BS(packed)); memcpy(cp, RB_BP(packed), RB_BS(packed));
    for (int off = 0; off < 0x20; off += 1) {
        BYTE keep = cp[off]; cp[off] ^= 0x01;
        if (off == 0 || off == 1 || off == 2 || off == 3 || off == 4 || off == 6 || off == 8 || off == 9 || off == 10 || off == 11 || (off >= 16 && off < 32)) {
            UINT t2 = 0; HRESULT hr = RB_D3DDecompressShaders()(cp, RB_BS(packed), 1, 0, NULL, 0, out, &t2);
            t_check(hr == RB_E_FAIL, __FILE__, __LINE__, "corrupt header byte rejected", "offset %d got %s", off, d3dh_hr_name(hr));
            if (SUCCEEDED(hr)) SAFE_RELEASE(out[0]);
        }
        cp[off] = keep;
    }
    free(cp);
    ID3D10Blob_Release(packed); ID3D10Blob_Release(vs); ID3D10Blob_Release(ps);
}
TEST(rb_blob_compress_many_shaders)
{
    ID3DBlob *vs, *ps, *packed = NULL; RB_SHADER_DATA sd[8]; ID3DBlob *out[8]; UINT total = 0;
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders"); REQUIRE(RB_D3DDecompressShaders());
    if (!rb_two_shaders(&vs, &ps)) return;
    for (int i = 0; i < 8; i++) { sd[i].pBytecode = (i & 1) ? RB_BP(ps) : RB_BP(vs); sd[i].BytecodeLength = (i & 1) ? RB_BS(ps) : RB_BS(vs); out[i] = NULL; }
    EXPECT_HR(RB_D3DCompressShaders()(8, sd, 1, &packed), S_OK);
    if (packed) {
        EXPECT_HR(RB_D3DDecompressShaders()(RB_BP(packed), RB_BS(packed), 8, 0, NULL, 0, out, &total), S_OK);
        EXPECT_EQ(total, 8);
        for (int i = 0; i < 8; i++) if (out[i]) {
            CHECK(RB_BS(out[i]) == sd[i].BytecodeLength && !memcmp(RB_BP(out[i]), sd[i].pBytecode, sd[i].BytecodeLength));
            ID3D10Blob_Release(out[i]);
        }
        ID3D10Blob_Release(packed);
    }
    ID3D10Blob_Release(vs); ID3D10Blob_Release(ps);
}
TEST(rb_blob_compress_identical_shaders_shrink_container)
{
    ID3DBlob *vs, *ps, *p1 = NULL, *p8 = NULL; RB_SHADER_DATA sd[8];
    REQUIRE(RB_D3DCompressShaders()); RB_NOWINE("D3DCompressShaders");
    if (!rb_two_shaders(&vs, &ps)) return;
    for (int i = 0; i < 8; i++) { sd[i].pBytecode = RB_BP(vs); sd[i].BytecodeLength = RB_BS(vs); }
    EXPECT_HR(RB_D3DCompressShaders()(1, sd, 1, &p1), S_OK); EXPECT_HR(RB_D3DCompressShaders()(8, sd, 1, &p8), S_OK);
    if (p1 && p8) { t_note("1 shader -> %zu bytes, 8 identical -> %zu bytes (raw %zu each)", (size_t)RB_BS(p1), (size_t)RB_BS(p8), (size_t)RB_BS(vs)); CHECK(RB_BS(p8) < 8 * RB_BS(vs)); }
    SAFE_RELEASE(p1); SAFE_RELEASE(p8); ID3D10Blob_Release(vs); ID3D10Blob_Release(ps);
}

/* ---- DebugSetMute --------------------------------------------------------------------------------- */
TEST(rb_blob_debugsetmute_callable)
{
    REQUIRE(RB_DebugSetMute());
    if (t_is_wine()) { t_skip("DebugSetMute is an unimplemented stub under the emulator"); return; }
    RB_DebugSetMute()(TRUE); RB_DebugSetMute()(FALSE); RB_DebugSetMute()(TRUE); RB_DebugSetMute()(TRUE); RB_DebugSetMute()(FALSE);
    CHECK(1);
}
TEST(rb_blob_debugsetmute_does_not_disturb_compiler)
{
    ID3DBlob *b = NULL;
    REQUIRE(RB_DebugSetMute());
    if (t_is_wine()) { t_skip("DebugSetMute is an unimplemented stub under the emulator"); return; }
    RB_DebugSetMute()(TRUE);
    if (rb_compile_t(RB_PS_CONST, "ps_4_0", 0, &b)) ID3D10Blob_Release(b);
    RB_DebugSetMute()(FALSE);
}
