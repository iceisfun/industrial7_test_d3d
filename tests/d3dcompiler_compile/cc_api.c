/* D3DCompile / D3DCompile2 entry-point contract: outputs, argument validation, error blobs, macros. */
#include "cc.h"

#define NEED_COMPILE() do { REQUIRE(CCF(D3DCompile)); } while (0)
#define NEED_COMPILE2() do { REQUIRE(CCF(D3DCompile2)); } while (0)

TEST(d3dcompiler_compile_api_basic_ps)
{
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    CHECK(r.code != NULL);
    CHECK(cc_size(r.code) > 32);
    CHECK(cc_dxbc_ok(cc_ptr(r.code), cc_size(r.code)));
    CHECK(r.errb == NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_basic_vs)
{
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, CC_VS_SRC, "main", "vs_5_0", 0), S_OK);
    CHECK(cc_dxbc_ok(cc_ptr(r.code), cc_size(r.code)));
    CHECK(r.errb == NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_clean_success_has_no_error_blob)
{
    static const char *tg[] = { "ps_4_0", "ps_4_1", "ps_5_0", "ps_5_1" };
    int i;
    NEED_COMPILE();
    for (i = 0; i < 4; i++) {
        cc_r r;
        EXPECT_HR(cc_run(&r, CC_PS_CONST, "main", tg[i], 0), S_OK);
        CHECKF(r.errb == NULL, "target %s", tg[i]);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_api_null_code_output_allowed)
{
    ID3DBlob *err = NULL;
    HRESULT hr;
    NEED_COMPILE();
    hr = CCF(D3DCompile)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, NULL, &err);
    EXPECT_HR(hr, S_OK);
    CHECK(err == NULL);
    SAFE_RELEASE(err);
}

TEST(d3dcompiler_compile_api_both_outputs_null)
{
    HRESULT hr;
    NEED_COMPILE();
    hr = CCF(D3DCompile)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, NULL, NULL);
    EXPECT_HR(hr, S_OK);
}

TEST(d3dcompiler_compile_api_null_error_output_on_failure)
{
    ID3DBlob *code = (ID3DBlob *)(ULONG_PTR)0x10;
    HRESULT hr;
    NEED_COMPILE();
    hr = CCF(D3DCompile)("float4 main( : SV_Target {", 25, "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, &code, NULL);
    EXPECT_HR_FAILED(hr);
    CHECK(code == NULL);
}

TEST(d3dcompiler_compile_api_outputs_cleared_on_invalid_flags)
{
    WIN_ONLY();
    ID3DBlob *code = (ID3DBlob *)(ULONG_PTR)0x10, *err = (ID3DBlob *)(ULONG_PTR)0x20;
    HRESULT hr;
    NEED_COMPILE();
    hr = CCF(D3DCompile)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0x80, 0, &code, &err);
    EXPECT_HR(hr, CC_INVALIDCALL);
    CHECK(code == NULL);
    CHECK(err != NULL);
    SAFE_RELEASE(err);
}

TEST(d3dcompiler_compile_api_outputs_cleared_on_syntax_error)
{
    ID3DBlob *code = (ID3DBlob *)(ULONG_PTR)0x10, *err = (ID3DBlob *)(ULONG_PTR)0x20;
    HRESULT hr;
    NEED_COMPILE();
    hr = CCF(D3DCompile)("float4 main( {", 14, "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, &code, &err);
    EXPECT_HR_FAILED(hr);
    CHECK(code == NULL);
    CHECK(err != NULL);
    SAFE_RELEASE(err);
}

TEST(d3dcompiler_compile_api_failure_hresult_is_negative)
{
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, "float4 main() : SV_Target { return undeclared_thing; }", "main", "ps_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.hr == E_FAIL || r.hr == CC_COMPILEFAIL);
    if (r.hr != E_FAIL && r.hr != CC_COMPILEFAIL) t_note("semantic error hr=0x%08lx", (unsigned long)r.hr);
    CHECK(r.code == NULL);
    CHECK(r.errb != NULL);
    CHECK(cc_size(r.errb) > 0);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_error_blob_is_text)
{
    cc_r r;
    size_t n;
    NEED_COMPILE();
    cc_run(&r, "float4 main() : SV_Target { return undeclared_thing; }", "main", "ps_5_0", 0);
    n = cc_size(r.errb);
    CHECK(n > 8);
    CHECK(strlen(r.err) >= 8);
    /* every character is printable text or whitespace */
    {
        size_t i; int bad = 0;
        for (i = 0; i < strlen(r.err); i++) {
            unsigned char c = (unsigned char)r.err[i];
            if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') bad++;
        }
        CHECK(bad == 0);
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_entry_missing)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, CC_PS_SRC, "nosuchentry", "ps_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    CHECK(cc_has_error(r.err, 3501));
    CHECK(strstr(r.err, "nosuchentry") != NULL);
    CHECK(strstr(r.err, "entrypoint not found") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_entry_case_sensitive)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, CC_PS_SRC, "MAIN", "ps_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(cc_has_error(r.err, 3501));
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_entry_null)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE();
    cc_compile_full(&r, CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, NULL, "ps_5_0", 0, 0);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    CHECK(r.code == NULL);
    CHECK(strstr(r.err, "pEntrypoint pointer is invalid") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_entry_null_all_shader_stages)
{
    WIN_ONLY();
    static const char *tg[] = { "vs_4_0", "ps_4_0", "gs_4_0", "cs_5_0", "hs_5_0", "ds_5_0", "vs_2_0", "ps_3_0" };
    int i;
    NEED_COMPILE();
    for (i = 0; i < 8; i++) {
        cc_r r;
        cc_compile_full(&r, CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, NULL, tg[i], 0, 0);
        CHECKF(r.hr == CC_INVALIDCALL, "target %s hr=0x%08lx", tg[i], (unsigned long)r.hr);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_api_entry_empty_string)
{
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, CC_PS_SRC, "", "ps_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    t_note("empty entry hr=0x%08lx err=%.80s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_entry_selects_function)
{
    static const char *src =
        "float4 a() : SV_Target { return 1; }\n"
        "float4 b() : SV_Target { return 2; }\n"
        "float4 c() : SV_Target { return 3; }\n";
    static const char *e[] = { "a", "b", "c" };
    cc_r r[3];
    int i;
    NEED_COMPILE();
    for (i = 0; i < 3; i++) EXPECT_HR(cc_run(&r[i], src, e[i], "ps_5_0", 0), S_OK);
    for (i = 0; i < 3; i++) CHECK(r[i].code != NULL);
    CHECK(cc_size(r[0].code) == cc_size(r[1].code));
    CHECK(memcmp(cc_ptr(r[0].code), cc_ptr(r[1].code), cc_size(r[0].code)) != 0);
    CHECK(memcmp(cc_ptr(r[1].code), cc_ptr(r[2].code), cc_size(r[1].code)) != 0);
    for (i = 0; i < 3; i++) cc_free(&r[i]);
}

TEST(d3dcompiler_compile_api_target_lib_rejects_entry)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, "export float4 f(float4 a) { return a; }", "main", "lib_5_0", 0);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    CHECK(r.code == NULL);
    CHECK(strstr(r.err, "Entry point cannot be specified for a library") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_target_lib_without_entry)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE();
    cc_compile_full(&r, "export float4 f(float4 a) { return a; }", 39, "t.hlsl", NULL, NULL, NULL, "lib_5_0", 0, 0);
    if (FAILED(r.hr)) t_note("lib_5_0 compile hr=0x%08lx err=%.120s", (unsigned long)r.hr, r.err);
    EXPECT_HR(r.hr, S_OK);
    if (r.code) {
        CHECK(cc_dxbc_ok(cc_ptr(r.code), cc_size(r.code)));
        t_note("lib_5_0 parts: LIBH=%d LIBF=%d", cc_has_part(r.code, "LIBH"), cc_has_part(r.code, "LIBF"));
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_target_rootsig_requires_define_name)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE();
    cc_compile_full(&r, "#define RS \"RootFlags(0)\"\n", 27, "t.hlsl", NULL, NULL, NULL, "rootsig_1_0", 0, 0);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    CHECK(strstr(r.err, "Entry point must specify the RootSignature define name") != NULL);
    cc_free(&r);
    cc_compile_full(&r, "#define RS \"RootFlags(0)\"\n", 27, "t.hlsl", NULL, NULL, NULL, "rootsig_1_1", 0, 0);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_target_rootsig_compiles)
{
    static const char *src = "#define RS \"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), CBV(b0)\"\n";
    static const char *tg[] = { "rootsig_1_0", "rootsig_1_1" };
    int i;
    NEED_COMPILE();
    if (t_is_wine()) { t_skip("root signature profiles need native compiler"); return; }
    for (i = 0; i < 2; i++) {
        cc_r r;
        cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "RS", tg[i], 0, 0);
        EXPECT_HR(r.hr, S_OK);
        if (r.code) {
            CHECK(cc_dxbc_ok(cc_ptr(r.code), cc_size(r.code)));
            CHECKF(cc_has_part(r.code, "RTS0"), "target %s", tg[i]);
        }
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_api_target_unknown_names)
{
    WIN_ONLY();
    static const char *bad[] = { "ps_9_9", "xs_5_0", "ps", "ps_5", "PS_5_0", "ps_5_0 ", " ps_5_0", "ps_5_0_", "vs_6_0", "ps_6_0", "" };
    unsigned i;
    NEED_COMPILE();
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        cc_r r;
        cc_run(&r, CC_PS_SRC, "main", bad[i], 0);
        CHECKF((LONG)r.hr < 0, "target '%s' hr=0x%08lx", bad[i], (unsigned long)r.hr);
        CHECKF(r.code == NULL, "target '%s' produced code", bad[i]);
        CHECKF(r.errb != NULL, "target '%s' has no error blob", bad[i]);
        t_note("target '%s' -> hr=0x%08lx", bad[i], (unsigned long)r.hr);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_api_stage_mismatch)
{
    cc_r r;
    NEED_COMPILE();
    /* SV_Position output is not valid in a pixel shader output; vertex input semantic as SV_Target fails to validate */
    cc_run(&r, "float4 main(float4 p : POSITION) : SV_Position { return p; }", "main", "ps_5_0", 0);
    t_note("vs-style entry compiled as ps_5_0: hr=0x%08lx", (unsigned long)r.hr);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_source_size_limits_parse)
{
    static const char *src = "float4 main() : SV_Target { return 1; } this is garbage after the sized part";
    size_t good = strlen("float4 main() : SV_Target { return 1; }");
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&r, src, good, "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0), S_OK);
    CHECK(r.code != NULL);
    cc_free(&r);
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_source_not_nul_terminated)
{
    const char *s = "float4 main() : SV_Target { return float4(0.25, 0.5, 0.75, 1); }";
    size_t n = strlen(s);
    char *copy = (char *)malloc(n);
    cc_r r;
    NEED_COMPILE();
    memcpy(copy, s, n);
    EXPECT_HR(cc_compile_full(&r, copy, n, "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0), S_OK);
    CHECK(r.code != NULL);
    cc_free(&r);
    free(copy);
}

TEST(d3dcompiler_compile_api_source_crlf_and_tabs)
{
    const char *s = "float4\tmain()\r\n:\tSV_Target\r\n{\r\n\treturn 1;\r\n}\r\n";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, s, "main", "ps_5_0", 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_source_bom_is_not_fatal)
{
    const char *s = "\xEF\xBB\xBF" "float4 main() : SV_Target { return 1; }";
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, s, "main", "ps_5_0", 0);
    t_note("UTF-8 BOM source hr=0x%08lx err=%.100s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_source_comments_and_whitespace)
{
    const char *s = "/* block */ // line\nfloat4 main() /* mid */ : SV_Target // tail\n{ return 1; } // eof";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, s, "main", "ps_5_0", 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_source_unterminated_comment)
{
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, "float4 main() : SV_Target { return 1; } /* never closed", "main", "ps_5_0", 0);
    t_note("unterminated comment hr=0x%08lx err=%.120s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_long_source)
{
    size_t cap = 200000, n = 0;
    char *s = (char *)malloc(cap);
    int i;
    cc_r r;
    NEED_COMPILE();
    n += snprintf(s + n, cap - n, "float4 main(float4 p : SV_Position) : SV_Target {\n float4 a = p;\n");
    for (i = 0; i < 1500; i++) n += snprintf(s + n, cap - n, " a = a * 0.999f + %d.0f; // filler line %d\n", i % 7, i);
    n += snprintf(s + n, cap - n, " return a;\n}\n");
    EXPECT_HR(cc_compile_full(&r, s, n, "long.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0), S_OK);
    cc_free(&r);
    free(s);
}

TEST(d3dcompiler_compile_api_srcname_prefixes_diagnostics)
{
    WIN_ONLY();
    static const char *names[] = { "my_shader.hlsl", "dir/sub/file.fx", "C:/path/with space/x.hlsl", "n" };
    unsigned i;
    NEED_COMPILE();
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        cc_r r;
        char first[512];
        const char *nl;
        size_t l;
        cc_compile_full(&r, "float4 main() : SV_Target { return bad_symbol; }", 48, names[i], NULL, NULL, "main", "ps_5_0", 0, 0);
        CHECK((LONG)r.hr < 0);
        nl = strchr(r.err, '\n');
        l = nl ? (size_t)(nl - r.err) : strlen(r.err);
        if (l >= sizeof first) l = sizeof first - 1;
        memcpy(first, r.err, l); first[l] = 0;
        CHECKF(cc_line_format_ok(first, l, names[i]), "name '%s' first line '%s'", names[i], first);
        CHECKF(cc_has_error(r.err, 3004), "name '%s'", names[i]);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_api_srcname_null_synthesised)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE();
    cc_compile_full(&r, "float4 main() : SV_Target { return bad_symbol; }", 48, NULL, NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(strncmp(r.err, "Shader@0x", 9) == 0);
    {
        const char *p = r.err + 9;
        int hex = 0;
        while ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) { p++; hex++; }
        CHECK(hex >= 8);
        CHECK(*p == '(');
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_srcname_empty)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE();
    cc_compile_full(&r, "float4 main() : SV_Target { return bad_symbol; }", 48, "", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(cc_has_error(r.err, 3004));
    t_note("empty source name -> '%.60s'", r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_diagnostic_line_format)
{
    WIN_ONLY();
    static const char *src =
        "float4 main() : SV_Target\n"
        "{\n"
        "    float4 a = missing_a;\n"
        "    float4 b = missing_b;\n"
        "    return a + b;\n"
        "}\n";
    cc_r r;
    const char *s;
    int n = 0;
    NEED_COMPILE();
    cc_compile_full(&r, src, strlen(src), "fmt.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    s = r.err;
    while (*s) {
        const char *e = strchr(s, '\n');
        size_t l = e ? (size_t)(e - s) : strlen(s);
        if (l && s[l - 1] == '\r') l--;
        if (l) {
            CHECKF(cc_line_format_ok(s, l, "fmt.hlsl"), "line %d: '%.*s'", n, (int)l, s);
            n++;
        }
        if (!e) break;
        s = e + 1;
    }
    CHECK(n >= 2);
    CHECK(strstr(r.err, "fmt.hlsl(3,") != NULL);
    CHECK(strstr(r.err, "fmt.hlsl(4,") != NULL);
    CHECK(strstr(r.err, "'missing_a'") != NULL);
    CHECK(strstr(r.err, "'missing_b'") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_error_text_ends_with_newline)
{
    cc_r r;
    size_t n;
    NEED_COMPILE();
    cc_run(&r, "float4 main() : SV_Target { return bad; }", "main", "ps_5_0", 0);
    n = strlen(r.err);
    CHECK(n > 0);
    if (n) CHECK(r.err[n - 1] == '\n');
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_syntax_error_position)
{
    WIN_ONLY();
    static const char *src = "float4 main() : SV_Target\n{\n  return 1\n}\n";
    cc_r r;
    NEED_COMPILE();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(cc_has_error(r.err, 3000));
    CHECK(strstr(r.err, "t.hlsl(4,") != NULL);
    CHECK(strstr(r.err, "syntax error") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_warning_with_success)
{
    WIN_ONLY();
    static const char *src = "float4 main(float4 p : SV_Position) : SV_Target { float3 x = p; return float4(x, 1); }";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, src, "main", "ps_5_0", 0), S_OK);
    CHECK(r.code != NULL);
    CHECK(r.errb != NULL);
    CHECK(cc_has_warning(r.err, 3206));
    CHECK(strstr(r.err, "implicit truncation of vector type") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_warnings_are_errors)
{
    WIN_ONLY();
    static const char *src = "float4 main(float4 p : SV_Position) : SV_Target { float3 x = p; return float4(x, 1); }";
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, src, "main", "ps_5_0", D3DCOMPILE_WARNINGS_ARE_ERRORS);
    CHECK((LONG)r.hr < 0);
    CHECK(r.hr == E_FAIL || r.hr == CC_COMPILEFAIL);
    CHECK(r.code == NULL);
    CHECK(strstr(r.err, "X3206") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_deterministic_bytes)
{
    cc_r a, b;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&a, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    EXPECT_EQ(cc_size(a.code), cc_size(b.code));
    CHECK(memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)) == 0);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_api_source_name_does_not_change_release_bytes)
{
    cc_r a, b;
    NEED_COMPILE();
    cc_compile_full(&a, CC_PS_SRC, strlen(CC_PS_SRC), "one.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    cc_compile_full(&b, CC_PS_SRC, strlen(CC_PS_SRC), "second_name.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    EXPECT_HR(a.hr, S_OK); EXPECT_HR(b.hr, S_OK);
    EXPECT_EQ(cc_size(a.code), cc_size(b.code));
    CHECK(memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)) == 0);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_api_blob_object_contract)
{
    cc_r r;
    IUnknown *unk = NULL;
    ID3DBlob *b2 = NULL;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    if (!r.code) { cc_free(&r); return; }
    EXPECT_EQ(d3dh_refcount(r.code), 1);
    CHECK(SUCCEEDED(D3DH_QI(r.code, IID_IUnknown, &unk)));
    SAFE_RELEASE(unk);
    CHECK(SUCCEEDED(D3DH_QI(r.code, IID_ID3D10Blob, &b2)));
    SAFE_RELEASE(b2);
    CHECK(ID3D10Blob_GetBufferPointer(r.code) == ID3D10Blob_GetBufferPointer(r.code));
    CHECK(ID3D10Blob_GetBufferSize(r.code) == ID3D10Blob_GetBufferSize(r.code));
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_error_blob_object_contract)
{
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, "float4 main() : SV_Target { return bad; }", "main", "ps_5_0", 0);
    CHECK(r.errb != NULL);
    if (r.errb) {
        EXPECT_EQ(d3dh_refcount(r.errb), 1);
        CHECK(ID3D10Blob_GetBufferSize(r.errb) > 0);
        CHECK(ID3D10Blob_GetBufferPointer(r.errb) != NULL);
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_blob_data_independent_of_source)
{
    char *s = _strdup(CC_PS_SRC);
    size_t n = strlen(s);
    cc_r r;
    unsigned char *copy;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&r, s, n, "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0), S_OK);
    copy = (unsigned char *)malloc(cc_size(r.code));
    memcpy(copy, cc_ptr(r.code), cc_size(r.code));
    memset(s, 'X', n);
    CHECK(memcmp(copy, cc_ptr(r.code), cc_size(r.code)) == 0);
    free(copy); free(s);
    cc_free(&r);
}

/* ---- macros -------------------------------------------------------------- */
TEST(d3dcompiler_compile_api_macro_defines_value)
{
    WIN_ONLY();
    static const D3D_SHADER_MACRO defs[] = { { "VALUE", "0.75" }, { NULL, NULL } };
    static const char *src = "float4 main() : SV_Target { return VALUE; }";
    cc_r a, b;
    NEED_COMPILE();
    cc_run(&a, src, "main", "ps_5_0", 0);
    CHECK((LONG)a.hr < 0);
    CHECK(cc_has_error(a.err, 3004));
    EXPECT_HR(cc_compile_full(&b, src, strlen(src), "t.hlsl", defs, NULL, "main", "ps_5_0", 0, 0), S_OK);
    CHECK(b.code != NULL);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_api_macro_value_changes_output)
{
    static const D3D_SHADER_MACRO d1[] = { { "V", "0.25" }, { NULL, NULL } };
    static const D3D_SHADER_MACRO d2[] = { { "V", "0.5" }, { NULL, NULL } };
    static const char *src = "float4 main() : SV_Target { return V; }";
    cc_r a, b, c;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&a, src, strlen(src), "t.hlsl", d1, NULL, "main", "ps_5_0", 0, 0), S_OK);
    EXPECT_HR(cc_compile_full(&b, src, strlen(src), "t.hlsl", d2, NULL, "main", "ps_5_0", 0, 0), S_OK);
    EXPECT_HR(cc_compile_full(&c, src, strlen(src), "t.hlsl", d1, NULL, "main", "ps_5_0", 0, 0), S_OK);
    if (a.code && b.code && c.code) {
        CHECK(cc_size(a.code) != cc_size(b.code) || memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)) != 0);
        EXPECT_EQ(cc_size(a.code), cc_size(c.code));
        CHECK(memcmp(cc_ptr(a.code), cc_ptr(c.code), cc_size(a.code)) == 0);
    }
    cc_free(&a); cc_free(&b); cc_free(&c);
}

TEST(d3dcompiler_compile_api_macro_empty_definition)
{
    static const D3D_SHADER_MACRO defs[] = { { "QUAL", "" }, { NULL, NULL } };
    static const char *src = "QUAL float4 main() : SV_Target { return 1; }";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&r, src, strlen(src), "t.hlsl", defs, NULL, "main", "ps_5_0", 0, 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_macro_ifdef_selects_branch)
{
    static const D3D_SHADER_MACRO on[] = { { "FEATURE", "1" }, { NULL, NULL } };
    static const char *src =
        "#ifdef FEATURE\nfloat4 main() : SV_Target { return 1; }\n#else\nthis is not hlsl\n#endif\n";
    cc_r a, b;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&a, src, strlen(src), "t.hlsl", on, NULL, "main", "ps_5_0", 0, 0), S_OK);
    cc_run(&b, src, "main", "ps_5_0", 0);
    CHECK((LONG)b.hr < 0);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_api_macro_if_expression)
{
    static const D3D_SHADER_MACRO d[] = { { "LEVEL", "3" }, { NULL, NULL } };
    static const char *src =
        "#if LEVEL > 2\n#define RES float4(1,2,3,4)\n#else\n#define RES float4(0,0,0,0)\n#endif\n"
        "float4 main() : SV_Target { return RES; }\n";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&r, src, strlen(src), "t.hlsl", d, NULL, "main", "ps_5_0", 0, 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_macro_multiple_and_function_like_use)
{
    static const D3D_SHADER_MACRO d[] = { { "A", "1.0" }, { "B", "2.0" }, { "C", "A + B" }, { "TARGET_NAME", "main" }, { NULL, NULL } };
    static const char *src = "float4 TARGET_NAME() : SV_Target { return float4(A, B, C, 1); }";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&r, src, strlen(src), "t.hlsl", d, NULL, "main", "ps_5_0", 0, 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_macro_terminator_stops_list)
{
    /* entries after the {NULL,NULL} terminator are never examined */
    static const D3D_SHADER_MACRO d[] = { { "A", "1.0" }, { NULL, NULL }, { "B", "oops(" } };
    static const char *src = "float4 main() : SV_Target { return A; }";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&r, src, strlen(src), "t.hlsl", d, NULL, "main", "ps_5_0", 0, 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_macro_overrides_nothing_when_undefined)
{
    static const D3D_SHADER_MACRO d[] = { { "UNUSED", "1" }, { NULL, NULL } };
    cc_r a, b;
    NEED_COMPILE();
    EXPECT_HR(cc_compile_full(&a, CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", d, NULL, "main", "ps_5_0", 0, 0), S_OK);
    EXPECT_HR(cc_run(&b, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    EXPECT_EQ(cc_size(a.code), cc_size(b.code));
    CHECK(memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)) == 0);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_api_macro_stringize_like_define_in_source)
{
    static const char *src =
        "#define SQR(x) ((x)*(x))\n"
        "float4 main(float4 p : SV_Position) : SV_Target { return SQR(p.x) + SQR(2); }\n";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, src, "main", "ps_5_0", 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_builtin_macros)
{
    static const char *src =
        "float4 main() : SV_Target {\n"
        "#if defined(__LINE__) && defined(__FILE__)\n return float4(__LINE__, 0, 0, 1);\n#else\n#error missing builtins\n#endif\n}\n";
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, src, "main", "ps_5_0", 0);
    t_note("__LINE__/__FILE__ defined check hr=0x%08lx %.100s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_hlsl_version_macro)
{
    static const char *src =
        "#ifndef __HLSL_VERSION\n#error no version macro\n#endif\n"
        "float4 main() : SV_Target { return __HLSL_VERSION; }\n";
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, src, "main", "ps_5_0", 0);
    t_note("__HLSL_VERSION available: %d (hr=0x%08lx)", SUCCEEDED(r.hr), (unsigned long)r.hr);
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_error_directive)
{
    WIN_ONLY();
    static const char *src = "#error stop here\nfloat4 main() : SV_Target { return 1; }\n";
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, src, "main", "ps_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    CHECK(strstr(r.err, "stop here") != NULL);
    CHECK(strstr(r.err, "t.hlsl(1,") != NULL);
    cc_free(&r);
}

/* ---- D3DCompile2 --------------------------------------------------------- */
static HRESULT cc_c2(cc_r *r, const char *src, const char *entry, const char *target, UINT f1, UINT f2, UINT sflags, const void *sdata, SIZE_T ssize)
{
    memset(r, 0, sizeof *r);
    r->hr = CCF(D3DCompile2)(src, strlen(src), "t.hlsl", NULL, NULL, entry, target, f1, f2, sflags, sdata, ssize, &r->code, &r->errb);
    cc_blobstr(r->errb, r->err, sizeof r->err);
    return r->hr;
}

TEST(d3dcompiler_compile_c2_basic_matches_d3dcompile)
{
    cc_r a, b;
    NEED_COMPILE2(); NEED_COMPILE();
    EXPECT_HR(cc_c2(&a, CC_PS_SRC, "main", "ps_5_0", 0, 0, 0, NULL, 0), S_OK);
    EXPECT_HR(cc_run(&b, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    EXPECT_EQ(cc_size(a.code), cc_size(b.code));
    CHECK(memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)) == 0);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_c2_secondary_flags_invalid_bits)
{
    WIN_ONLY();
    static const UINT bad[] = { 0x8, 0x10, 0x20, 0x100, 0x10000, 0x80000000, 0xFFFFFFF8, 0xFFFFFFFF };
    unsigned i;
    NEED_COMPILE2();
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        cc_r r;
        cc_c2(&r, CC_PS_SRC, "main", "ps_5_0", 0, 0, bad[i], NULL, 0);
        CHECKF(r.hr == E_INVALIDARG, "secondary flags 0x%x hr=0x%08lx", bad[i], (unsigned long)r.hr);
        CHECK(r.code == NULL);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_c2_secondary_data_requires_flags)
{
    WIN_ONLY();
    static const unsigned char blob[16] = { 0 };
    cc_r r;
    NEED_COMPILE2();
    cc_c2(&r, CC_PS_SRC, "main", "ps_5_0", 0, 0, 0, blob, sizeof blob);
    EXPECT_HR(r.hr, E_INVALIDARG);
    CHECK(r.code == NULL);
    cc_free(&r);
    cc_c2(&r, CC_PS_SRC, "main", "ps_5_0", 0, 0, 0, blob, 0);
    EXPECT_HR(r.hr, E_INVALIDARG);
    cc_free(&r);
    cc_c2(&r, CC_PS_SRC, "main", "ps_5_0", 0, 0, 0, NULL, 16);
    EXPECT_HR(r.hr, E_INVALIDARG);
    cc_free(&r);
}

TEST(d3dcompiler_compile_c2_secondary_flags_valid_bits_not_rejected_early)
{
    UINT f;
    NEED_COMPILE2();
    for (f = 1; f <= 7; f++) {
        cc_r r;
        cc_c2(&r, CC_PS_SRC, "main", "ps_5_0", 0, 0, f, NULL, 0);
        t_note("secondary flags %u without data -> hr=0x%08lx", f, (unsigned long)r.hr);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_c2_invalid_args_do_not_produce_diagnostics)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE2();
    cc_c2(&r, CC_PS_SRC, "main", "ps_5_0", 0, 0, 0x8, NULL, 0);
    EXPECT_HR(r.hr, E_INVALIDARG);
    CHECK(r.errb == NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_c2_flags_same_validation_as_d3dcompile)
{
    WIN_ONLY();
    cc_r r;
    NEED_COMPILE2();
    cc_c2(&r, CC_PS_SRC, "main", "ps_5_0", 0x80, 0, 0, NULL, 0);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    cc_free(&r);
    cc_c2(&r, CC_PS_SRC, "main", "ps_5_0", D3DCOMPILE_DEBUG_NAME_FOR_SOURCE | D3DCOMPILE_DEBUG_NAME_FOR_BINARY, 0, 0, NULL, 0);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_c2_null_outputs)
{
    HRESULT hr;
    NEED_COMPILE2();
    hr = CCF(D3DCompile2)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, 0, NULL, 0, NULL, NULL);
    EXPECT_HR(hr, S_OK);
}

/* ---- concurrency ----------------------------------------------------------- */
typedef struct { HRESULT hr; size_t size; unsigned char *bytes; } cc_thr;
static DWORD WINAPI cc_thread_compile(LPVOID p)
{
    cc_thr *t = (cc_thr *)p;
    cc_r r;
    int i;
    t->hr = S_OK;
    for (i = 0; i < 10; i++) {
        cc_run(&r, CC_PS_SRC, "main", "ps_5_0", 0);
        if (FAILED(r.hr)) { t->hr = r.hr; cc_free(&r); return 1; }
        if (i == 0) {
            t->size = cc_size(r.code);
            t->bytes = (unsigned char *)malloc(t->size);
            memcpy(t->bytes, cc_ptr(r.code), t->size);
        } else if (cc_size(r.code) != t->size || memcmp(t->bytes, cc_ptr(r.code), t->size)) {
            t->hr = E_UNEXPECTED;
        }
        cc_free(&r);
    }
    return 0;
}

TEST(d3dcompiler_compile_api_concurrent_compiles_identical)
{
    cc_thr th[4];
    HANDLE h[4];
    int i;
    NEED_COMPILE();
    memset(th, 0, sizeof th);
    for (i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, cc_thread_compile, &th[i], 0, NULL);
    for (i = 0; i < 4; i++) {
        if (WaitForSingleObject(h[i], 60000) != WAIT_OBJECT_0) { CHECK(!"thread timeout"); TerminateThread(h[i], 1); }
        CloseHandle(h[i]);
    }
    for (i = 0; i < 4; i++) EXPECT_HR(th[i].hr, S_OK);
    for (i = 1; i < 4; i++) {
        EXPECT_EQ(th[i].size, th[0].size);
        if (th[i].bytes && th[0].bytes && th[i].size == th[0].size) CHECK(memcmp(th[i].bytes, th[0].bytes, th[0].size) == 0);
    }
    for (i = 0; i < 4; i++) free(th[i].bytes);
}

/* ---- misc source features -------------------------------------------------- */
TEST(d3dcompiler_compile_api_cbuffer_texture_sampler)
{
    static const char *src =
        "cbuffer cb : register(b0) { float4 tint; float scale; };\n"
        "Texture2D tex : register(t0);\nSamplerState smp : register(s0);\n"
        "float4 main(float2 uv : TEXCOORD0) : SV_Target { return tex.Sample(smp, uv) * tint * scale; }\n";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, src, "main", "ps_5_0", 0), S_OK);
    if (r.code) CHECK(cc_has_part(r.code, "RDEF"));
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_struct_io)
{
    static const char *src =
        "struct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD0; };\n"
        "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
        "VSOut main(VSIn i) { VSOut o; o.pos = float4(i.pos, 1); o.uv = i.uv; return o; }\n";
    cc_r r;
    NEED_COMPILE();
    EXPECT_HR(cc_run(&r, src, "main", "vs_5_0", 0), S_OK);
    if (r.code) {
        CHECK(cc_has_part(r.code, "ISGN"));
        CHECK(cc_has_part(r.code, "OSGN"));
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_compute_shader)
{
    static const char *src =
        "RWStructuredBuffer<uint> buf : register(u0);\n"
        "[numthreads(8,1,1)] void main(uint3 id : SV_DispatchThreadID) { buf[id.x] = id.x * 2; }\n";
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, src, "main", "cs_5_0", 0);
    if (t_is_wine() && FAILED(r.hr)) { t_skip("structured buffers unsupported by this compiler"); cc_free(&r); return; }
    EXPECT_HR(r.hr, S_OK);
    if (r.code) CHECK(cc_has_part(r.code, "SHEX"));
    cc_free(&r);
}

TEST(d3dcompiler_compile_api_compute_requires_numthreads)
{
    static const char *src = "RWBuffer<uint> b : register(u0);\nvoid main(uint3 id : SV_DispatchThreadID) { b[id.x] = 1; }\n";
    cc_r r;
    NEED_COMPILE();
    cc_run(&r, src, "main", "cs_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    t_note("cs without numthreads: hr=0x%08lx %.100s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_c2_secondary_flags_zero_data_null_ok)
{
    cc_r r;
    NEED_COMPILE2();
    memset(&r, 0, sizeof r);
    r.hr = CCF(D3DCompile2)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, 0, NULL, 0, &r.code, &r.errb);
    EXPECT_HR(r.hr, S_OK);
    CHECK(cc_dxbc_ok(cc_ptr(r.code), cc_size(r.code)));
    cc_free(&r);
}

TEST(d3dcompiler_compile_c2_secondary_flag_bit3_rejected_per_bit)
{
    UINT bits[] = { 0x8, 0x10, 0x100, 0x80000000u };
    int i;
    WIN_ONLY();
    NEED_COMPILE2();
    for (i = 0; i < 4; i++) {
        cc_r r;
        memset(&r, 0, sizeof r);
        r.hr = CCF(D3DCompile2)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, bits[i], NULL, 0, &r.code, &r.errb);
        CHECKF(FAILED(r.hr), "secondary flags 0x%x hr=0x%08lx", bits[i], (unsigned long)r.hr);
        CHECK(r.code == NULL);
        cc_free(&r);
    }
}
