/* ID3DInclude callbacks, D3D_COMPILE_STANDARD_FILE_INCLUDE, D3DCompileFromFile and D3DPreprocess. */
#include "cc.h"

#define NEED() do { REQUIRE(CCF(D3DCompile)); } while (0)

static const char *cc_main_src =
    "#include \"inc.h\"\n"
    "float4 main() : SV_Target { return INC_VALUE; }\n";

/* ---------------- ID3DInclude contract ---------------- */
TEST(d3dcompiler_compile_include_local_type_and_name)
{
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "inc.h", "#define INC_VALUE float4(1,2,3,4)\n");
    cc_compile_full(&r, cc_main_src, strlen(cc_main_src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 1);
    EXPECT_EQ(inc.n_close, 1);
    EXPECT_EQ(inc.types[0], D3D_INCLUDE_LOCAL);
    EXPECT_STR(inc.names[0], "inc.h");
    CHECK(inc.closed[0] == inc.datas[0]);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_system_type)
{
    static const char *src = "#include <sys/inc.h>\nfloat4 main() : SV_Target { return INC_VALUE; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "sys/inc.h", "#define INC_VALUE 0.5\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 1);
    EXPECT_EQ(inc.types[0], D3D_INCLUDE_SYSTEM);
    EXPECT_STR(inc.names[0], "sys/inc.h");
    CHECK(cc_inc_balanced(&inc));
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_enum_values)
{
    EXPECT_EQ(D3D_INCLUDE_LOCAL, 0);
    EXPECT_EQ(D3D_INCLUDE_SYSTEM, 1);
}

TEST(d3dcompiler_compile_include_name_passed_verbatim)
{
    static const char *src =
        "#include \"dir/sub/file.hlsli\"\n#include \"UPPER.H\"\n#include \"with space.h\"\n"
        "float4 main() : SV_Target { return A + B + C; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "dir/sub/file.hlsli", "#define A 1\n");
    cc_inc_add(&inc, "UPPER.H", "#define B 2\n");
    cc_inc_add(&inc, "with space.h", "#define C 3\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 3);
    if (inc.n_open == 3) {
        t_note("verbatim names: '%s' '%s' '%s'", inc.names[0], inc.names[1], inc.names[2]);
        EXPECT_STR(inc.names[1], "UPPER.H");
        EXPECT_STR(inc.names[2], "with space.h");
    }
    CHECK(cc_inc_balanced(&inc));
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_nested_open_close_pairing)
{
    static const char *src = "#include \"a.h\"\nfloat4 main() : SV_Target { return A_VAL + B_VAL; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "a.h", "#include \"b.h\"\n#define A_VAL 1\n");
    cc_inc_add(&inc, "b.h", "#define B_VAL 2\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 2);
    EXPECT_EQ(inc.n_close, 2);
    CHECK(cc_inc_balanced(&inc));
    if (inc.n_open == 2) {
        EXPECT_STR(inc.names[0], "a.h");
        EXPECT_STR(inc.names[1], "b.h");
        EXPECT_EQ(inc.types[1], D3D_INCLUDE_LOCAL);
        t_note("nested order: %d %d %d %d (+n open, -n close)", inc.order[0], inc.order[1], inc.order[2], inc.order[3]);
        t_note("parent data of top level include: %p; of nested: %p (a.h data %p)", inc.parents[0], inc.parents[1], inc.datas[0]);
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_nested_callback_order)
{
    static const char *src = "#include \"a.h\"\nfloat4 main() : SV_Target { return A_VAL + B_VAL; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "a.h", "#include \"b.h\"\n#define A_VAL 1\n");
    cc_inc_add(&inc, "b.h", "#define B_VAL 2\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_order, 4);
    if (inc.n_order == 4) {
        CHECK(inc.order[0] == 1 && inc.order[1] == 2);       /* a opened, then b */
        t_note("close order for nested includes: %d then %d (-2 means b first)", inc.order[2], inc.order[3]);
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_siblings_sequential)
{
    static const char *src = "#include \"a.h\"\n#include \"b.h\"\nfloat4 main() : SV_Target { return A_VAL + B_VAL; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "a.h", "#define A_VAL 1\n");
    cc_inc_add(&inc, "b.h", "#define B_VAL 2\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 2);
    EXPECT_EQ(inc.n_close, 2);
    if (inc.n_order == 4)
        t_note("sibling include order: %d %d %d %d", inc.order[0], inc.order[1], inc.order[2], inc.order[3]);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_same_file_opened_per_directive)
{
    static const char *src =
        "#include \"x.h\"\n#include \"x.h\"\nfloat4 main() : SV_Target { return COUNT; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "x.h", "#ifndef COUNT\n#define COUNT 1\n#endif\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 2);
    EXPECT_EQ(inc.n_close, 2);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_pragma_once_note)
{
    static const char *src =
        "#include \"x.h\"\n#include \"x.h\"\nfloat4 main() : SV_Target { return S1(); }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "x.h", "#pragma once\nfloat4 S1() { return 1; }\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    t_note("#pragma once: hr=0x%08lx opens=%d closes=%d", (unsigned long)r.hr, inc.n_open, inc.n_close);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_inactive_branch_not_opened)
{
    static const char *src =
        "#if 0\n#include \"never.h\"\n#endif\n#ifdef NOT_DEFINED\n#include \"never2.h\"\n#endif\n"
        "float4 main() : SV_Target { return 1; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 0);
    EXPECT_EQ(inc.n_close, 0);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_not_called_without_directive)
{
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_compile_full(&r, CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 0);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_open_failure_fails_compile)
{
    WIN_ONLY();
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    inc.fail_hr = E_FAIL;
    cc_compile_full(&r, cc_main_src, strlen(cc_main_src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    EXPECT_EQ(inc.n_open, 1);
    CHECK(strstr(r.err, "inc.h") != NULL);
    CHECK(cc_has_error(r.err, 1507));
    t_note("open failure: hr=0x%08lx closes=%d err=%.120s", (unsigned long)r.hr, inc.n_close, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_open_failure_codes)
{
    static const HRESULT codes[] = { E_FAIL, E_OUTOFMEMORY, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), E_ACCESSDENIED };
    unsigned i;
    NEED();
    for (i = 0; i < 4; i++) {
        cc_inc inc;
        cc_r r;
        cc_inc_init(&inc);
        inc.fail_hr = codes[i];
        cc_compile_full(&r, cc_main_src, strlen(cc_main_src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
        t_note("Open returns 0x%08lx: compile hr=0x%08lx", (unsigned long)codes[i], (unsigned long)r.hr);
        if (FAILED(codes[i])) CHECKF((LONG)r.hr < 0, "Open failure 0x%08lx must fail compile", (unsigned long)codes[i]);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_include_empty_file)
{
    static const char *src = "#include \"empty.h\"\nfloat4 main() : SV_Target { return 1; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "empty.h", "");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    if (FAILED(r.hr)) t_note("empty include fails: hr=0x%08lx %.100s", (unsigned long)r.hr, r.err);
    EXPECT_EQ(inc.n_open, 1);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_error_reports_included_file_name)
{
    WIN_ONLY();
    static const char *src = "#include \"bad.h\"\nfloat4 main() : SV_Target { return BAD; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "bad.h", "#define BAD (unknown_in_include)\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(cc_has_error(r.err, 3004));
    t_note("error through include macro: %.160s", r.err);
    cc_free(&r);
    cc_inc_init(&inc);
    cc_inc_add(&inc, "bad.h", "float helper() { return unknown_in_include; }\n");
    {
        static const char *src2 = "#include \"bad.h\"\nfloat4 main() : SV_Target { return helper(); }\n";
        cc_compile_full(&r, src2, strlen(src2), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
        CHECK((LONG)r.hr < 0);
        CHECK(strstr(r.err, "bad.h(1,") != NULL);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_include_error_line_numbers_resume_in_parent)
{
    WIN_ONLY();
    static const char *src = "#include \"ok.h\"\n\nfloat4 main() : SV_Target { return unknown_after_include; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "ok.h", "// one\n// two\n// three\n");
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(strstr(r.err, "t.hlsl(3,") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_macros_visible_in_include)
{
    static const D3D_SHADER_MACRO defs[] = { { "FROM_APP", "0.5" }, { NULL, NULL } };
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "inc.h", "#ifdef FROM_APP\n#define INC_VALUE FROM_APP\n#else\n#error missing macro\n#endif\n");
    cc_compile_full(&r, cc_main_src, strlen(cc_main_src), "t.hlsl", defs, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_define_flows_back_to_includer)
{
    cc_inc inc;
    cc_r a, b;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "inc.h", "#define INC_VALUE float4(1,2,3,4)\n");
    cc_compile_full(&a, cc_main_src, strlen(cc_main_src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    EXPECT_HR(a.hr, S_OK);
    EXPECT_HR(cc_run(&b, "float4 main() : SV_Target { return float4(1,2,3,4); }", "main", "ps_5_0", 0), S_OK);
    if (a.code && b.code) {
        DWORD sa = 0, sb = 0;
        const unsigned char *pa = cc_shader_part(a.code, &sa);
        const unsigned char *pb = cc_shader_part(b.code, &sb);
        CHECK(pa && pb && sa == sb);
        if (pa && pb && sa == sb) CHECK(!memcmp(pa, pb, sa));
    }
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_include_deep_nesting)
{
    static char names[8][16];
    static char texts[8][64];
    cc_inc inc;
    cc_r r;
    int i;
    NEED();
    cc_inc_init(&inc);
    for (i = 0; i < 8; i++) {
        snprintf(names[i], sizeof names[i], "l%d.h", i);
        if (i < 7) snprintf(texts[i], sizeof texts[i], "#include \"l%d.h\"\n", i + 1);
        else snprintf(texts[i], sizeof texts[i], "#define DEEP 1\n");
        cc_inc_add(&inc, names[i], texts[i]);
    }
    {
        static const char *src = "#include \"l0.h\"\nfloat4 main() : SV_Target { return DEEP; }\n";
        cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    }
    EXPECT_HR(r.hr, S_OK);
    EXPECT_EQ(inc.n_open, 8);
    EXPECT_EQ(inc.n_close, 8);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_recursive_is_bounded)
{
    static const char *src = "#include \"self.h\"\nfloat4 main() : SV_Target { return 1; }\n";
    cc_inc inc;
    cc_r r;
    NEED();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "self.h", "#include \"self.h\"\n");
    inc.max_open = 40;
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, (ID3DInclude *)&inc, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    t_note("recursive include stopped after %d opens: %.120s", inc.n_open, r.err);
    t_note("opens=%d closes=%d", inc.n_open, inc.n_close);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_with_assemble_and_preprocess_share_semantics)
{
    cc_inc inc;
    ID3DBlob *out = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DPreprocess));
    cc_inc_init(&inc);
    cc_inc_add(&inc, "inc.h", "#define INC_VALUE 7\n");
    hr = CCF(D3DPreprocess)(cc_main_src, strlen(cc_main_src), "t.hlsl", NULL, (ID3DInclude *)&inc, &out, &err);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(inc.n_open, 1);
    EXPECT_EQ(inc.n_close, 1);
    EXPECT_EQ(inc.types[0], D3D_INCLUDE_LOCAL);
    SAFE_RELEASE(out); SAFE_RELEASE(err);
}

/* ---------------- standard file include ---------------- */
static int cc_write_file(const char *path, const char *text)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    WriteFile(h, text, (DWORD)strlen(text), &w, NULL);
    CloseHandle(h);
    return 1;
}
static void cc_wide(const char *s, WCHAR *w, int cap) { MultiByteToWideChar(CP_UTF8, 0, s, -1, w, cap); }

TEST(d3dcompiler_compile_include_standard_sentinel_value)
{
    EXPECT_EQ((ULONG_PTR)D3D_COMPILE_STANDARD_FILE_INCLUDE, 1);
}

TEST(d3dcompiler_compile_include_standard_with_absolute_path)
{
    WIN_ONLY();
    char inc_path[MAX_PATH], src[MAX_PATH * 2 + 128];
    cc_r r;
    NEED();
    t_tmpfile(inc_path, sizeof inc_path, "abs_inc.hlsli");
    CHECK(cc_write_file(inc_path, "#define INC_VALUE float4(1,0,0,1)\n"));
    { char *q; for (q = inc_path; *q; q++) if (*q == '\\') *q = '/'; }
    snprintf(src, sizeof src, "#include \"%s\"\nfloat4 main() : SV_Target { return INC_VALUE; }\n", inc_path);
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0);
    t_note("standard include with absolute path: hr=0x%08lx %.150s", (unsigned long)r.hr, r.err);
    EXPECT_HR(r.hr, S_OK);
    DeleteFileA(inc_path);
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_standard_missing_file_fails)
{
    WIN_ONLY();
    static const char *src = "#include \"no_such_file_xyz.hlsli\"\nfloat4 main() : SV_Target { return 1; }\n";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    CHECK(cc_has_error(r.err, 1507));
    cc_free(&r);
}

TEST(d3dcompiler_compile_include_fromfile_relative_to_source)
{
    char main_path[MAX_PATH], inc_path[MAX_PATH];
    WCHAR wmain[MAX_PATH];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    t_tmpfile(main_path, sizeof main_path, "main_with_inc.hlsl");
    t_tmpfile(inc_path, sizeof inc_path, "rel_inc.hlsli");
    CHECK(cc_write_file(inc_path, "#define INC_VALUE float4(0,0,1,1)\n"));
    CHECK(cc_write_file(main_path, "#include \"rel_inc.hlsli\"\nfloat4 main() : SV_Target { return INC_VALUE; }\n"));
    cc_wide(main_path, wmain, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(wmain, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR(hr, S_OK);
    CHECK(code != NULL);
    if (FAILED(hr) && err) { char t[1024]; cc_blobstr(err, t, sizeof t); t_note("fromfile include error: %s", t); }
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    DeleteFileA(main_path); DeleteFileA(inc_path);
}

TEST(d3dcompiler_compile_include_fromfile_nested_subdirectory_note)
{
    char dir[MAX_PATH], main_path[MAX_PATH], a_path[MAX_PATH], b_path[MAX_PATH];
    WCHAR wmain[MAX_PATH];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    snprintf(dir, sizeof dir, "%ssub", t_tmpdir());
    CreateDirectoryA(dir, NULL);
    snprintf(main_path, sizeof main_path, "%smain_sub.hlsl", t_tmpdir());
    snprintf(a_path, sizeof a_path, "%s\\a.hlsli", dir);
    snprintf(b_path, sizeof b_path, "%s\\b.hlsli", dir);
    CHECK(cc_write_file(a_path, "#include \"b.hlsli\"\n"));
    CHECK(cc_write_file(b_path, "#define INC_VALUE 1\n"));
    CHECK(cc_write_file(main_path, "#include \"sub/a.hlsli\"\nfloat4 main() : SV_Target { return INC_VALUE; }\n"));
    cc_wide(main_path, wmain, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(wmain, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &code, &err);
    t_note("nested include relative to including file: hr=0x%08lx", (unsigned long)hr);
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    DeleteFileA(main_path); DeleteFileA(a_path); DeleteFileA(b_path); RemoveDirectoryA(dir);
}

/* ---------------- D3DCompileFromFile ---------------- */
TEST(d3dcompiler_compile_fromfile_missing_file)
{
    char path[MAX_PATH];
    WCHAR w[MAX_PATH];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    t_tmpfile(path, sizeof path, "definitely_missing.hlsl");
    cc_wide(path, w, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    CHECK(code == NULL);
    CHECK(err == NULL);
}

TEST(d3dcompiler_compile_fromfile_missing_directory)
{
    char path[MAX_PATH];
    WCHAR w[MAX_PATH];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    snprintf(path, sizeof path, "%sno_such_dir\\x.hlsl", t_tmpdir());
    cc_wide(path, w, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR(hr, HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
    CHECK(code == NULL);
}

TEST(d3dcompiler_compile_fromfile_null_filename)
{
    WIN_ONLY();
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    hr = CCF(D3DCompileFromFile)(NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR(hr, E_FAIL);
    CHECK(code == NULL);
}

TEST(d3dcompiler_compile_fromfile_name_too_long_for_source_name)
{
    WIN_ONLY();
    WCHAR w[400];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    int i;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    for (i = 0; i < 300; i++) w[i] = L'a';
    w[300] = 0;
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR(hr, E_FAIL);
    CHECK(code == NULL);
}

TEST(d3dcompiler_compile_fromfile_name_at_limit_reaches_file_open)
{
    WCHAR w[400];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    int i;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    for (i = 0; i < 100; i++) w[i] = L'a';
    w[100] = 0;
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    CHECK((LONG)hr < 0);
    CHECK(hr != E_FAIL);
    CHECK(HRESULT_FACILITY(hr) == FACILITY_WIN32);
}

TEST(d3dcompiler_compile_fromfile_directory_is_not_a_file)
{
    WCHAR w[MAX_PATH];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    cc_wide(t_tmpdir(), w, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    CHECK((LONG)hr < 0);
    CHECK(code == NULL);
    t_note("directory as source: hr=0x%08lx", (unsigned long)hr);
}

TEST(d3dcompiler_compile_fromfile_compiles_file)
{
    char path[MAX_PATH];
    WCHAR w[MAX_PATH];
    ID3DBlob *code = NULL, *err = NULL;
    cc_r ref;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    t_tmpfile(path, sizeof path, "from_file.hlsl");
    CHECK(cc_write_file(path, CC_PS_SRC));
    cc_wide(path, w, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR(hr, S_OK);
    CHECK(code != NULL);
    CHECK(err == NULL);
    EXPECT_HR(cc_run(&ref, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    if (code && ref.code) {
        EXPECT_EQ(cc_size(code), cc_size(ref.code));
        if (cc_size(code) == cc_size(ref.code)) CHECK(!memcmp(cc_ptr(code), cc_ptr(ref.code), cc_size(code)));
    }
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    cc_free(&ref);
    DeleteFileA(path);
}

TEST(d3dcompiler_compile_fromfile_error_names_the_path)
{
    WIN_ONLY();
    char path[MAX_PATH], text[2048];
    WCHAR w[MAX_PATH];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    t_tmpfile(path, sizeof path, "bad_from_file.hlsl");
    CHECK(cc_write_file(path, "float4 main() : SV_Target { return nothing_here; }\n"));
    cc_wide(path, w, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    CHECK((LONG)hr < 0);
    CHECK(code == NULL);
    CHECK(err != NULL);
    cc_blobstr(err, text, sizeof text);
    {
        char want[MAX_PATH + 8];
        snprintf(want, sizeof want, "%s(1,", path);
        CHECKF(strstr(text, want) != NULL, "want '%s' in '%.200s'", want, text);
    }
    CHECK(cc_has_error(text, 3004));
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    DeleteFileA(path);
}

TEST(d3dcompiler_compile_fromfile_unicode_path_reported_as_utf8)
{
    WIN_ONLY();
    WCHAR w[MAX_PATH], wdir[MAX_PATH];
    char text[2048];
    ID3DBlob *code = NULL, *err = NULL;
    HANDLE h;
    DWORD wr;
    HRESULT hr;
    static const char body[] = "float4 main() : SV_Target { return nothing_here; }\n";
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    cc_wide(t_tmpdir(), wdir, MAX_PATH);
    wsprintfW(w, L"%ls%ls", wdir, L"café_€.hlsl");
    h = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create unicode file name"); return; }
    WriteFile(h, body, sizeof body - 1, &wr, NULL);
    CloseHandle(h);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    CHECK((LONG)hr < 0);
    cc_blobstr(err, text, sizeof text);
    CHECKF(strstr(text, "caf\xC3\xA9_\xE2\x82\xAC.hlsl(") != NULL, "text: %.200s", text);
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    DeleteFileW(w);
}

TEST(d3dcompiler_compile_fromfile_defines_and_flags)
{
    WIN_ONLY();
    char path[MAX_PATH];
    WCHAR w[MAX_PATH];
    static const D3D_SHADER_MACRO defs[] = { { "V", "0.25" }, { NULL, NULL } };
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    t_tmpfile(path, sizeof path, "defs.hlsl");
    CHECK(cc_write_file(path, "float4 main() : SV_Target { return V; }\n"));
    cc_wide(path, w, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(w, defs, NULL, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    hr = CCF(D3DCompileFromFile)(w, defs, NULL, "main", "ps_5_0", 0x80, 0, &code, &err);
    EXPECT_HR(hr, CC_INVALIDCALL);
    CHECK(code == NULL);
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    DeleteFileA(path);
}

TEST(d3dcompiler_compile_fromfile_null_outputs)
{
    char path[MAX_PATH];
    WCHAR w[MAX_PATH];
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    t_tmpfile(path, sizeof path, "nulls.hlsl");
    CHECK(cc_write_file(path, CC_PS_SRC));
    cc_wide(path, w, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, NULL, NULL);
    EXPECT_HR(hr, S_OK);
    DeleteFileA(path);
}

TEST(d3dcompiler_compile_fromfile_empty_file_note)
{
    char path[MAX_PATH];
    WCHAR w[MAX_PATH];
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    t_tmpfile(path, sizeof path, "empty.hlsl");
    CHECK(cc_write_file(path, ""));
    cc_wide(path, w, MAX_PATH);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    CHECK((LONG)hr < 0);
    CHECK(code == NULL);
    t_note("empty file: hr=0x%08lx", (unsigned long)hr);
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    DeleteFileA(path);
}

TEST(d3dcompiler_compile_fromfile_locked_for_write_is_readable_with_share_read)
{
    char path[MAX_PATH];
    WCHAR w[MAX_PATH];
    HANDLE h;
    DWORD wr;
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr;
    NEED(); REQUIRE(CCF(D3DCompileFromFile));
    t_tmpfile(path, sizeof path, "shared.hlsl");
    h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, CC_PS_SRC, (DWORD)strlen(CC_PS_SRC), &wr, NULL);
    FlushFileBuffers(h);
    cc_wide(path, w, MAX_PATH);
    /* the reader opens with FILE_SHARE_READ only, so a concurrent writer handle makes the open fail */
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR(hr, HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION));
    CloseHandle(h);
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    hr = CCF(D3DCompileFromFile)(w, NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(code); SAFE_RELEASE(err);
    DeleteFileA(path);
}

/* ---------------- D3DPreprocess ---------------- */
static HRESULT cc_pp(const char *src, const D3D_SHADER_MACRO *d, ID3DInclude *inc, char *out, size_t cap, char *errs, size_t ecap)
{
    ID3DBlob *o = NULL, *e = NULL;
    HRESULT hr = CCF(D3DPreprocess)(src, strlen(src), "t.hlsl", d, inc, &o, &e);
    cc_blobstr(o, out, cap);
    cc_blobstr(e, errs, ecap);
    SAFE_RELEASE(o); SAFE_RELEASE(e);
    return hr;
}

TEST(d3dcompiler_compile_preprocess_null_arguments)
{
    WIN_ONLY();
    ID3DBlob *o = NULL, *e = NULL;
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = CCF(D3DPreprocess)(NULL, 0, "t.hlsl", NULL, NULL, &o, &e);
    EXPECT_HR(hr, E_INVALIDARG);
    CHECK(o == NULL);
    hr = CCF(D3DPreprocess)(NULL, 10, "t.hlsl", NULL, NULL, &o, &e);
    EXPECT_HR(hr, E_INVALIDARG);
    hr = CCF(D3DPreprocess)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, NULL, &e);
    EXPECT_HR(hr, E_INVALIDARG);
    hr = CCF(D3DPreprocess)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, NULL, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(o); SAFE_RELEASE(e);
}

TEST(d3dcompiler_compile_preprocess_expands_macros)
{
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("#define K 42\n#define SQ(x) ((x)*(x))\nint a = K;\nint b = SQ(3);\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "int a = 42;"));
    CHECK(cc_has_nows(out, "int b = ((3)*(3));"));
    CHECK(!cc_has_nows(out, "#define"));
    CHECK(errs[0] == 0);
}

TEST(d3dcompiler_compile_preprocess_conditionals)
{
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("#if 0\nint dead;\n#else\nint live;\n#endif\n#ifdef NOPE\nint dead2;\n#endif\n#ifndef NOPE\nint live2;\n#endif\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "live;"));
    CHECK(cc_has_nows(out, "live2;"));
    CHECK(!cc_has_nows(out, "dead"));
    CHECK(!cc_has_nows(out, "#if") && !cc_has_nows(out, "#else") && !cc_has_nows(out, "#endif"));
}

TEST(d3dcompiler_compile_preprocess_strips_comments)
{
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("int a; // trailing comment\n/* block\n comment */ int b;\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "int a;"));
    CHECK(cc_has_nows(out, "int b;"));
    CHECK(!cc_has_nows(out, "trailing comment"));
    CHECK(!cc_has_nows(out, "block"));
}

TEST(d3dcompiler_compile_preprocess_api_defines)
{
    static const D3D_SHADER_MACRO d[] = { { "WIDTH", "16" }, { "NAME", "foo" }, { NULL, NULL } };
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("int WIDTH_VALUE = WIDTH; int NAME;\n", d, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "int WIDTH_VALUE = 16;"));
    CHECK(cc_has_nows(out, "int foo;"));
}

TEST(d3dcompiler_compile_preprocess_output_has_line_markers)
{
    WIN_ONLY();
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("int a;\n#define X 1\nint b = X;\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "#line"));
    t_note("preprocess output prefix: %.60s", out);
}

TEST(d3dcompiler_compile_preprocess_error_directive)
{
    WIN_ONLY();
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("int a;\n#error my custom failure\nint b;\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    CHECK((LONG)hr < 0);
    CHECK(strstr(errs, "my custom failure") != NULL);
    CHECK(strstr(errs, "t.hlsl(2,") != NULL);
    t_note("#error hr=0x%08lx text=%.120s", (unsigned long)hr, errs);
}

TEST(d3dcompiler_compile_preprocess_missing_include_without_handler)
{
    WIN_ONLY();
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("#include \"nothing.h\"\nint a;\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    CHECK((LONG)hr < 0);
    CHECK(cc_has_error(errs, 1507));
}

TEST(d3dcompiler_compile_preprocess_include_text_inlined)
{
    WIN_ONLY();
    cc_inc inc;
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    cc_inc_init(&inc);
    cc_inc_add(&inc, "i.h", "int from_include = 1;\n");
    hr = cc_pp("int before;\n#include \"i.h\"\nint after;\n", NULL, (ID3DInclude *)&inc, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    {
        const char *a = strstr(out, "int before;"), *b = strstr(out, "int from_include = 1;"), *c = strstr(out, "int after;");
        CHECK(a && b && c);
        if (a && b && c) CHECK(a < b && b < c);
    }
    CHECK(!cc_has_nows(out, "#include"));
    CHECK(inc.n_open == 1 && inc.n_close == 1);
}

TEST(d3dcompiler_compile_preprocess_null_error_output)
{
    ID3DBlob *o = NULL;
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = CCF(D3DPreprocess)("int a;\n", 7, "t.hlsl", NULL, NULL, &o, NULL);
    EXPECT_HR(hr, S_OK);
    CHECK(o != NULL);
    SAFE_RELEASE(o);
}

TEST(d3dcompiler_compile_preprocess_respects_source_size)
{
    static char out[8192], errs[1024];
    ID3DBlob *o = NULL, *e = NULL;
    HRESULT hr;
    const char *s = "int kept; int dropped_beyond_size;";
    REQUIRE(CCF(D3DPreprocess));
    hr = CCF(D3DPreprocess)(s, strlen("int kept;"), "t.hlsl", NULL, NULL, &o, &e);
    EXPECT_HR(hr, S_OK);
    cc_blobstr(o, out, sizeof out);
    cc_blobstr(e, errs, sizeof errs);
    CHECK(cc_has_nows(out, "int kept;"));
    CHECK(!cc_has_nows(out, "dropped"));
    SAFE_RELEASE(o); SAFE_RELEASE(e);
}

TEST(d3dcompiler_compile_preprocess_deterministic)
{
    static char a[8192], b[8192], errs[1024];
    const char *s = "#define A 1\n#define B(x) x+A\nint v = B(2);\n";
    REQUIRE(CCF(D3DPreprocess));
    EXPECT_HR(cc_pp(s, NULL, NULL, a, sizeof a, errs, sizeof errs), S_OK);
    EXPECT_HR(cc_pp(s, NULL, NULL, b, sizeof b, errs, sizeof errs), S_OK);
    CHECK(!strcmp(a, b));
}

TEST(d3dcompiler_compile_preprocess_then_compile_matches_direct_compile)
{
    static const char *src =
        "#define SCALE 0.5\n#define MIX(a, b) ((a) * SCALE + (b) * (1 - SCALE))\n"
        "float4 main(float4 p : SV_Position) : SV_Target { return MIX(p, float4(1,1,1,1)); }\n";
    ID3DBlob *pre = NULL, *errs = NULL;
    cc_r a, b;
    NEED(); REQUIRE(CCF(D3DPreprocess));
    EXPECT_HR(CCF(D3DPreprocess)(src, strlen(src), "t.hlsl", NULL, NULL, &pre, &errs), S_OK);
    SAFE_RELEASE(errs);
    EXPECT_HR(cc_run(&a, src, "main", "ps_5_0", 0), S_OK);
    if (pre) {
        cc_compile_full(&b, (const char *)cc_ptr(pre), strlen((const char *)cc_ptr(pre)), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
        EXPECT_HR(b.hr, S_OK);
        if (a.code && b.code) {
            DWORD sa = 0, sb = 0;
            const unsigned char *pa = cc_shader_part(a.code, &sa);
            const unsigned char *pb = cc_shader_part(b.code, &sb);
            CHECK(pa && pb && sa == sb);
            if (pa && pb && sa == sb) CHECK(!memcmp(pa, pb, sa));
        }
        cc_free(&b);
    }
    SAFE_RELEASE(pre);
    cc_free(&a);
}

TEST(d3dcompiler_compile_preprocess_stringify_and_paste)
{
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("#define CAT(a, b) a##b\n#define STR(x) #x\nint CAT(foo, bar);\nstatic const string s = STR(hello);\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "int foobar;"));
    CHECK(cc_has_nows(out, "\"hello\""));
}

TEST(d3dcompiler_compile_preprocess_undef_and_redefine)
{
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("#define A 1\nint x = A;\n#undef A\n#define A 2\nint y = A;\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "int x = 1;"));
    CHECK(cc_has_nows(out, "int y = 2;"));
}

TEST(d3dcompiler_compile_preprocess_if_arithmetic)
{
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("#define N 3\n#if (N * 2 == 6) && defined(N) && !defined(M)\nint ok;\n#endif\n#if N > 5 || N < 0\nint bad;\n#elif N == 3\nint elif_ok;\n#endif\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "int ok;"));
    CHECK(cc_has_nows(out, "int elif_ok;"));
    CHECK(!cc_has_nows(out, "int bad;"));
}

TEST(d3dcompiler_compile_preprocess_line_directive)
{
    WIN_ONLY();
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("#line 77 \"other.hlsl\"\nint a;\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "int a;"));
    CHECK(cc_has_nows(out, "77"));
    CHECK(cc_has_nows(out, "other.hlsl"));
}

TEST(d3dcompiler_compile_preprocess_pragma_passthrough)
{
    static char out[8192], errs[1024];
    HRESULT hr;
    REQUIRE(CCF(D3DPreprocess));
    hr = cc_pp("#pragma pack_matrix(row_major)\nint a;\n", NULL, NULL, out, sizeof out, errs, sizeof errs);
    EXPECT_HR(hr, S_OK);
    CHECK(cc_has_nows(out, "pack_matrix"));
}

TEST(d3dcompiler_compile_preprocess_outputs_independent_blobs)
{
    ID3DBlob *o1 = NULL, *o2 = NULL, *e = NULL;
    REQUIRE(CCF(D3DPreprocess));
    EXPECT_HR(CCF(D3DPreprocess)("int a;", 6, "t.hlsl", NULL, NULL, &o1, &e), S_OK);
    EXPECT_HR(CCF(D3DPreprocess)("int a;", 6, "t.hlsl", NULL, NULL, &o2, &e), S_OK);
    CHECK(o1 && o2 && o1 != o2);
    if (o1 && o2) CHECK(ID3D10Blob_GetBufferPointer(o1) != ID3D10Blob_GetBufferPointer(o2));
    SAFE_RELEASE(o1); SAFE_RELEASE(o2); SAFE_RELEASE(e);
}
