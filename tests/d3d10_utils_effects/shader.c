/* D3D10CompileShader, D3D10DisassembleShader, signature blobs, debug info, preprocessor. */
#include "ue.h"

#define E_INVALIDARG_ ((HRESULT)0x80070057u)
#define E_FAIL_       ((HRESULT)0x80004005u)

typedef HRESULT (WINAPI *cs_fn)(const char *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, const char *, const char *, UINT, ID3D10Blob **, ID3D10Blob **);
#define CS(dll) ((cs_fn)t_proc_in(dll, "D3D10CompileShader"))

static const char *text_of(ID3D10Blob *b, char *buf, size_t cap)
{
    size_t n = b ? ID3D10Blob_GetBufferSize(b) : 0;
    if (n >= cap) n = cap - 1;
    if (n) memcpy(buf, ID3D10Blob_GetBufferPointer(b), n);
    buf[n] = 0;
    return buf;
}
static int blob_has(ID3D10Blob *b, const char *needle)
{
    size_t n = b ? ID3D10Blob_GetBufferSize(b) : 0;
    const char *p = b ? ID3D10Blob_GetBufferPointer(b) : NULL;
    size_t m = strlen(needle);
    for (size_t i = 0; p && i + m <= n; i++) if (!memcmp(p + i, needle, m)) return 1;
    return 0;
}

/* The same shader from the two compilers that can produce D3D10 bytecode. */
static const char *k_srcs[3] = { NULL, NULL, NULL };
static HRESULT make_shader(int who, int stage, ID3D10Blob **out)
{
    static const char *const prof[2][3] = { { "vs_4_0", "ps_4_0", "gs_4_0" }, { "vs_4_0", "ps_4_0", "gs_4_0" } };
    const char *src = stage == 0 ? UE_VS_SRC : stage == 1 ? UE_PS_SRC : UE_GS_SRC;
    *out = NULL;
    if (who == 0) return ue_compile_vs(src, "main", prof[0][stage], out);
    return ue_d3dc(src, "main", prof[1][stage], 0, out);
}
static const char *const k_who[2] = { "d3d10", "d3dcompiler_47" };
static const char *const k_stage[3] = { "vs", "ps", "gs" };

/* ---- compile ---- */
static void check_container(ID3D10Blob *b, const char *what)
{
    const unsigned char *p = ue_bp(b);
    SIZE_T sz = ue_bs(b);
    t_check(sz >= 36 && !memcmp(p, "DXBC", 4), __FILE__, __LINE__, "DXBC magic", "%s", what);
    if (sz < 36) return;
    t_check(*(const DWORD *)(p + 20) == 1, __FILE__, __LINE__, "container version 1", "%s", what);
    t_check(*(const DWORD *)(p + 24) == sz, __FILE__, __LINE__, "container size == blob size", "%s: %u vs %u", what, *(const DWORD *)(p + 24), (unsigned)sz);
    DWORD n = *(const DWORD *)(p + 28);
    t_check(n >= 2 && n < 32, __FILE__, __LINE__, "chunk count sane", "%s: %u", what, n);
    int allzero = 1;
    for (int i = 4; i < 20; i++) if (p[i]) allzero = 0;
    t_check(!allzero, __FILE__, __LINE__, "checksum non-zero", "%s", what);
}
TEST(d3d10_utils_effects_shader_compile_stages_ok)
{
    REQUIRE(CS("d3d10.dll"));
    for (int st = 0; st < 3; st++) {
        ID3D10Blob *b = NULL;
        if (st == 2 && t_is_wine()) { t_note("gs compile skipped: emulator HLSL front end lacks geometry shaders"); continue; }
        HRESULT hr = make_shader(0, st, &b);
        t_check(hr == S_OK && b, __FILE__, __LINE__, "compile ok", "%s_4_0: %s", k_stage[st], d3dh_hr_name(hr));
        if (b) check_container(b, k_stage[st]);
        UE_REL(b);
    }
}
TEST(d3d10_utils_effects_shader_compile_version_tokens)
{
    REQUIRE(CS("d3d10.dll"));
    static const DWORD want[3] = { 0x00010040, 0x00000040, 0x00020040 };
    for (int who = 0; who < 2; who++) for (int st = 0; st < 3; st++) {
        if (st == 2 && t_is_wine()) continue;
        ID3D10Blob *b = NULL;
        if (FAILED(make_shader(who, st, &b))) { CHECKF(0, "%s %s failed", k_who[who], k_stage[st]); continue; }
        DWORD csz = 0;
        const unsigned char *c = ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('S','H','D','R'), &csz);
        if (!c) c = ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('S','H','E','X'), &csz);
        t_check(c && csz >= 8, __FILE__, __LINE__, "SHDR chunk", "%s %s", k_who[who], k_stage[st]);
        if (c) {
            t_check(*(const DWORD *)c == want[st], __FILE__, __LINE__, "version token", "%s %s: %08x", k_who[who], k_stage[st], *(const DWORD *)c);
            t_check(*(const DWORD *)(c + 4) * 4 == csz, __FILE__, __LINE__, "length token matches chunk", "%s %s", k_who[who], k_stage[st]);
        }
        UE_REL(b);
    }
}
TEST(d3d10_utils_effects_shader_compile_has_signature_chunks)
{
    REQUIRE(CS("d3d10.dll"));
    for (int who = 0; who < 2; who++) for (int st = 0; st < 3; st++) {
        if (st == 2 && t_is_wine()) continue;
        ID3D10Blob *b = NULL;
        if (FAILED(make_shader(who, st, &b))) { CHECKF(0, "%s %s failed", k_who[who], k_stage[st]); continue; }
        CHECK(ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('I','S','G','N'), NULL) != NULL);
        CHECK(ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('O','S','G','N'), NULL) != NULL);
        CHECK(ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('S','H','D','R'), NULL) != NULL);
        if (st == 1) CHECK(ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('R','D','E','F'), NULL) != NULL);
        UE_REL(b);
    }
}
TEST(d3d10_utils_effects_shader_compile_input_signature_content)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL;
    if (FAILED(make_shader(0, 0, &b))) { CHECK(0); return; }
    DWORD csz = 0;
    const unsigned char *c = ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('I','S','G','N'), &csz);
    if (c) {
        EXPECT_EQ(*(const DWORD *)c, 3);
        static const char *const names[3] = { "POSITION", "TEXCOORD", "NORMAL" };
        for (int i = 0; i < 3; i++) {
            const DWORD *e = (const DWORD *)(c + 8 + 24 * i);
            const char *nm = (const char *)c + e[0];
            t_check(!strcmp(nm, names[i]), __FILE__, __LINE__, "input element name", "%d: %s", i, nm);
            EXPECT_EQ(e[1], 0);
            EXPECT_EQ(e[4], i);
        }
    }
    UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_output_signature_content)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL;
    if (FAILED(make_shader(0, 0, &b))) { CHECK(0); return; }
    DWORD csz = 0;
    const unsigned char *c = ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('O','S','G','N'), &csz);
    if (c) {
        EXPECT_EQ(*(const DWORD *)c, 3);
        const DWORD *e0 = (const DWORD *)(c + 8);
        CHECK(!strcmp((const char *)c + e0[0], "SV_Position"));
        EXPECT_EQ(e0[2], 1);   /* D3D_NAME_POSITION */
    }
    UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_null_entry)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1, *e = NULL;
    HRESULT hr = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, NULL, "vs_4_0", 0, &b, &e);
    EXPECT_HR(hr, E_D3DERR_INVALIDCALL);
    EXPECT_EQ(b, NULL);
    UE_REL(e);
}
TEST(d3d10_utils_effects_shader_compile_output_cleared_on_failure)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1, *e = (ID3D10Blob *)(ULONG_PTR)1;
    HRESULT hr = CS("d3d10.dll")("float4 main() : SV_Target { return syntax error; }", 49, "bad.hlsl", NULL, NULL, "main", "ps_4_0", 0, &b, &e);
    EXPECT_HR(hr < 0, 1);
    EXPECT_EQ(b, NULL);
    if (e == (ID3D10Blob *)(ULONG_PTR)1) { CHECKF(0, "error out left untouched"); return; }
    UE_REL(e);
}
TEST(d3d10_utils_effects_shader_compile_syntax_error_text)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL, *e = NULL;
    const char *src = "float4 main() : SV_Target\n{\n    return float4(1,2,3,4)\n}\n";
    HRESULT hr = CS("d3d10.dll")(src, strlen(src), "bad.hlsl", NULL, NULL, "main", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr));
    EXPECT_EQ(b, NULL);
    CHECK(e != NULL);
    if (e) {
        char txt[1024];
        text_of(e, txt, sizeof txt);
        CHECK(strstr(txt, "error") != NULL);
        CHECK(strstr(txt, "bad.hlsl") != NULL);
        t_note("syntax error hr=%s text: %.160s", d3dh_hr_name(hr), txt);
    }
    UE_REL(e); UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_missing_entry_point)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL, *e = NULL;
    HRESULT hr = CS("d3d10.dll")(UE_PS_SRC, strlen(UE_PS_SRC), "ps.hlsl", NULL, NULL, "nosuchentry", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr));
    EXPECT_EQ(b, NULL);
    CHECK(e != NULL);
    if (e) { char txt[512]; t_note("missing entry hr=%s: %.200s", d3dh_hr_name(hr), text_of(e, txt, sizeof txt)); }
    UE_REL(e); UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_undeclared_identifier)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL, *e = NULL;
    const char *src = "float4 main() : SV_Target { return undeclared_value; }\n";
    HRESULT hr = CS("d3d10.dll")(src, strlen(src), "undecl.hlsl", NULL, NULL, "main", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr));
    CHECK(e != NULL);
    if (e) {
        char txt[1024];
        text_of(e, txt, sizeof txt);
        CHECK(strstr(txt, "undeclared_value") != NULL);
        CHECK(strstr(txt, "undecl.hlsl") != NULL);
    }
    UE_REL(e); UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_error_line_number)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL, *e = NULL;
    const char *src = "// line 1\n// line 2\nfloat4 main() : SV_Target { return nope; }\n";
    HRESULT hr = CS("d3d10.dll")(src, strlen(src), "ln.hlsl", NULL, NULL, "main", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr));
    if (e) {
        char txt[1024];
        text_of(e, txt, sizeof txt);
        CHECK(strstr(txt, "ln.hlsl(3,") != NULL);
    } else CHECK(0);
    UE_REL(e); UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_null_shader_out_allowed)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *e = NULL;
    HRESULT hr = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", "vs_4_0", 0, NULL, &e);
    EXPECT_HR(hr, S_OK);
    UE_REL(e);
}
TEST(d3d10_utils_effects_shader_compile_null_error_out_allowed)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL;
    HRESULT hr = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", "vs_4_0", 0, &b, NULL);
    EXPECT_HR(hr, S_OK);
    UE_REL(b);
    hr = CS("d3d10.dll")("garbage garbage", 15, "bad.hlsl", NULL, NULL, "main", "vs_4_0", 0, &b, NULL);
    CHECK(FAILED(hr));
    UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_invalid_flags)
{
    REQUIRE(CS("d3d10.dll"));
    static const UINT bad[] = { 0x40, 0x80, 0x100, 0x10000, 0x20000, 0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x40000000, 0x80000000, 0xdf0301c0 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1, *e = NULL;
        HRESULT hr = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", "vs_4_0", bad[i], &b, &e);
        t_check(hr == E_D3DERR_INVALIDCALL, __FILE__, __LINE__, "invalid flag bits", "flags 0x%x: %s", bad[i], d3dh_hr_name(hr));
        EXPECT_EQ(b, NULL);
        UE_REL(e); UE_REL(b);
    }
}
TEST(d3d10_utils_effects_shader_compile_valid_flags)
{
    REQUIRE(CS("d3d10.dll"));
    static const UINT good[] = { 0, D3D10_SHADER_SKIP_VALIDATION, D3D10_SHADER_SKIP_OPTIMIZATION, D3D10_SHADER_PACK_MATRIX_ROW_MAJOR,
        D3D10_SHADER_PACK_MATRIX_COLUMN_MAJOR, D3D10_SHADER_PARTIAL_PRECISION, D3D10_SHADER_AVOID_FLOW_CONTROL, D3D10_SHADER_PREFER_FLOW_CONTROL,
        D3D10_SHADER_ENABLE_STRICTNESS, D3D10_SHADER_IEEE_STRICTNESS, D3D10_SHADER_WARNINGS_ARE_ERRORS };
    for (unsigned i = 0; i < sizeof good / sizeof *good; i++) {
        ID3D10Blob *b = NULL, *e = NULL;
        HRESULT hr = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", "vs_4_0", good[i], &b, &e);
        t_check(SUCCEEDED(hr) && b, __FILE__, __LINE__, "flag accepted", "flags 0x%x: %s", good[i], d3dh_hr_name(hr));
        UE_REL(e); UE_REL(b);
    }
}
TEST(d3d10_utils_effects_shader_compile_row_vs_column_major_differ)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *a = NULL, *b = NULL, *e = NULL;
    HRESULT h1 = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", "vs_4_0", D3D10_SHADER_PACK_MATRIX_ROW_MAJOR, &a, &e);
    UE_REL(e);
    HRESULT h2 = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", "vs_4_0", D3D10_SHADER_PACK_MATRIX_COLUMN_MAJOR, &b, &e);
    UE_REL(e);
    EXPECT_HR(h1, S_OK); EXPECT_HR(h2, S_OK);
    if (a && b) {
        /* the matrix is multiplied as mul(v, M): the two packings must produce different code */
        int same = ue_bs(a) == ue_bs(b) && !memcmp(ue_bp(a), ue_bp(b), ue_bs(a));
        CHECK(!same);
    }
    UE_REL(a); UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_deterministic)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *a = NULL, *b = NULL;
    EXPECT_HR(ue_compile_vs(UE_PS_SRC, "main", "ps_4_0", &a), S_OK);
    EXPECT_HR(ue_compile_vs(UE_PS_SRC, "main", "ps_4_0", &b), S_OK);
    if (a && b) {
        EXPECT_EQ(ue_bs(a), ue_bs(b));
        CHECK(ue_bs(a) == ue_bs(b) && !memcmp(ue_bp(a), ue_bp(b), ue_bs(a)));
    }
    UE_REL(a); UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_macro_defines)
{
    REQUIRE(CS("d3d10.dll"));
    const char *src = "#ifndef MUST_DEFINE\n#error MUST_DEFINE is required\n#endif\nfloat4 main() : SV_Target { return float4(VALUE, 0, 0, 1); }\n";
    D3D10_SHADER_MACRO defs[] = { { "MUST_DEFINE", "1" }, { "VALUE", "0.5" }, { NULL, NULL } };
    ID3D10Blob *b = NULL, *e = NULL;
    HRESULT hr = CS("d3d10.dll")(src, strlen(src), "m.hlsl", defs, NULL, "main", "ps_4_0", 0, &b, &e);
    EXPECT_HR(hr, S_OK);
    UE_REL(b); UE_REL(e);
    hr = CS("d3d10.dll")(src, strlen(src), "m.hlsl", NULL, NULL, "main", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr));
    CHECK(b == NULL);
    if (e) { char t[512]; CHECK(strstr(text_of(e, t, sizeof t), "MUST_DEFINE") != NULL); }
    UE_REL(b); UE_REL(e);
}
TEST(d3d10_utils_effects_shader_compile_macro_changes_output)
{
    REQUIRE(CS("d3d10.dll"));
    const char *src = "float4 main() : SV_Target { return float4(VALUE, 0, 0, 1); }\n";
    D3D10_SHADER_MACRO d1[] = { { "VALUE", "0.25" }, { NULL, NULL } };
    D3D10_SHADER_MACRO d2[] = { { "VALUE", "0.75" }, { NULL, NULL } };
    ID3D10Blob *a = NULL, *b = NULL;
    EXPECT_HR(CS("d3d10.dll")(src, strlen(src), "m.hlsl", d1, NULL, "main", "ps_4_0", 0, &a, NULL), S_OK);
    EXPECT_HR(CS("d3d10.dll")(src, strlen(src), "m.hlsl", d2, NULL, "main", "ps_4_0", 0, &b, NULL), S_OK);
    if (a && b) CHECK(!(ue_bs(a) == ue_bs(b) && !memcmp(ue_bp(a), ue_bp(b), ue_bs(a))));
    UE_REL(a); UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_null_filename)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL, *e = NULL;
    HRESULT hr = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), NULL, NULL, NULL, "main", "vs_4_0", 0, &b, &e);
    EXPECT_HR(hr, S_OK);
    UE_REL(b); UE_REL(e);
    /* diagnostics refer to the anonymous source as Shader@0x<ptr> */
    const char *bad = "float4 main() : SV_Target { return oops; }";
    hr = CS("d3d10.dll")(bad, strlen(bad), NULL, NULL, NULL, "main", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr));
    if (e) { char t[512]; CHECK(strstr(text_of(e, t, sizeof t), "Shader@0x") != NULL); }
    UE_REL(b); UE_REL(e);
}
TEST(d3d10_utils_effects_shader_compile_entry_name_selects_function)
{
    REQUIRE(CS("d3d10.dll"));
    const char *src = "float4 first() : SV_Target { return 1; }\nfloat4 second() : SV_Target { return float4(0,1,0,1); }\n";
    ID3D10Blob *a = NULL, *b = NULL;
    EXPECT_HR(CS("d3d10.dll")(src, strlen(src), "two.hlsl", NULL, NULL, "first", "ps_4_0", 0, &a, NULL), S_OK);
    EXPECT_HR(CS("d3d10.dll")(src, strlen(src), "two.hlsl", NULL, NULL, "second", "ps_4_0", 0, &b, NULL), S_OK);
    if (a && b) CHECK(!(ue_bs(a) == ue_bs(b) && !memcmp(ue_bp(a), ue_bp(b), ue_bs(a))));
    UE_REL(a); UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_profile_mismatch_stage)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL, *e = NULL;
    /* a pixel shader entry compiled as vertex shader: no SV_Target in vs_4_0 */
    HRESULT hr = CS("d3d10.dll")(UE_PS_SRC, strlen(UE_PS_SRC), "ps.hlsl", NULL, NULL, "main", "vs_4_0", 0, &b, &e);
    CHECK(FAILED(hr));
    EXPECT_EQ(b, NULL);
    UE_REL(b); UE_REL(e);
}
TEST(d3d10_utils_effects_shader_compile_unknown_profile)
{
    REQUIRE(CS("d3d10.dll"));
    static const char *const bad[] = { "xs_4_0", "vs_9_9", "ps_4_9", "garbage", "", "vs_2_0x" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        ID3D10Blob *b = NULL, *e = NULL;
        HRESULT hr = CS("d3d10.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", bad[i], 0, &b, &e);
        t_check(FAILED(hr) && !b, __FILE__, __LINE__, "unknown profile fails", "'%s': %s", bad[i], d3dh_hr_name(hr));
        UE_REL(b); UE_REL(e);
    }
}
TEST(d3d10_utils_effects_shader_compile_sm4_1)
{
    REQUIRE(CS("d3d10.dll"));
    for (int st = 0; st < 2; st++) {
        ID3D10Blob *b = NULL;
        HRESULT hr = ue_compile_vs(st ? UE_PS_SRC : UE_VS_SRC, "main", st ? "ps_4_1" : "vs_4_1", &b);
        if (FAILED(hr)) { t_note("4_1 profile compile failed: %s", d3dh_hr_name(hr)); continue; }
        DWORD csz;
        const unsigned char *c = ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('S','H','D','R'), &csz);
        if (c) EXPECT_EQ(*(const DWORD *)c, st ? 0x41u : 0x10041u);
        UE_REL(b);
    }
}
TEST(d3d10_utils_effects_shader_compile_sm5_rejected)
{
    REQUIRE(CS("d3d10.dll"));
    ID3D10Blob *b = NULL;
    HRESULT hr = ue_compile_vs(UE_VS_SRC, "main", "vs_5_0", &b);
    if (SUCCEEDED(hr)) t_note("vs_5_0 accepted by D3D10CompileShader");
    else CHECK(b == NULL);
    UE_REL(b);
}
TEST(d3d10_utils_effects_shader_compile_via_d3d10_1)
{
    REQUIRE(CS("d3d10_1.dll"));
    ID3D10Blob *b = NULL, *e = NULL;
    HRESULT hr = CS("d3d10_1.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", "vs_4_0", 0, &b, &e);
    EXPECT_HR(hr, S_OK);
    if (b) check_container(b, "d3d10_1 thunk");
    UE_REL(b); UE_REL(e);
    EXPECT_HR(CS("d3d10_1.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, NULL, "vs_4_0", 0, &b, &e), E_D3DERR_INVALIDCALL);
    UE_REL(b); UE_REL(e);
    EXPECT_HR(CS("d3d10_1.dll")(UE_VS_SRC, strlen(UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", "vs_4_0", 0x40, &b, &e), E_D3DERR_INVALIDCALL);
    UE_REL(b); UE_REL(e);
}
TEST(d3d10_utils_effects_shader_compile_matches_between_dlls)
{
    REQUIRE(CS("d3d10.dll") && CS("d3d10_1.dll"));
    ID3D10Blob *a = NULL, *b = NULL;
    EXPECT_HR(CS("d3d10.dll")(UE_PS_SRC, strlen(UE_PS_SRC), "ue.hlsl", NULL, NULL, "main", "ps_4_0", 0, &a, NULL), S_OK);
    EXPECT_HR(CS("d3d10_1.dll")(UE_PS_SRC, strlen(UE_PS_SRC), "ue.hlsl", NULL, NULL, "main", "ps_4_0", 0, &b, NULL), S_OK);
    if (a && b) CHECK(ue_bs(a) == ue_bs(b) && !memcmp(ue_bp(a), ue_bp(b), ue_bs(a)));
    UE_REL(a); UE_REL(b);
}

/* ---- ID3D10Include ---- */
typedef struct {
    const ID3DIncludeVtbl *lpVtbl;
    int opens, closes;
    D3D_INCLUDE_TYPE type;
    char name[64];
    const void *parent;
    const void *opened_ptr, *closed_ptr;
    const char *content;
    HRESULT open_hr;
} inc_t;
static HRESULT WINAPI inc_open(ID3DInclude *This, D3D_INCLUDE_TYPE type, LPCSTR name, LPCVOID parent, LPCVOID *data, UINT *bytes)
{
    inc_t *s = (inc_t *)This;
    s->opens++;
    s->type = type;
    strncpy(s->name, name, sizeof s->name - 1);
    s->parent = parent;
    if (FAILED(s->open_hr)) return s->open_hr;
    *data = s->content;
    *bytes = (UINT)strlen(s->content);
    s->opened_ptr = s->content;
    return S_OK;
}
static HRESULT WINAPI inc_close(ID3DInclude *This, LPCVOID data)
{
    inc_t *s = (inc_t *)This;
    s->closes++;
    s->closed_ptr = data;
    return S_OK;
}
static const ID3DIncludeVtbl k_incvtbl = { inc_open, inc_close };

TEST(d3d10_utils_effects_shader_include_local)
{
    REQUIRE(CS("d3d10.dll"));
    inc_t inc = { &k_incvtbl, 0, 0, (D3D_INCLUDE_TYPE)-1, "", (void *)(ULONG_PTR)1, NULL, NULL, "float4 helper() { return float4(1,0,0,1); }\n", S_OK };
    const char *src = "#include \"helper.hlsli\"\nfloat4 main() : SV_Target { return helper(); }\n";
    ID3D10Blob *b = NULL, *e = NULL;
    HRESULT hr = CS("d3d10.dll")(src, strlen(src), "inc.hlsl", NULL, (ID3D10Include *)&inc, "main", "ps_4_0", 0, &b, &e);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(inc.opens, 1);
    EXPECT_EQ(inc.closes, 1);
    EXPECT_EQ(inc.type, D3D_INCLUDE_LOCAL);
    EXPECT_STR(inc.name, "helper.hlsli");
    EXPECT_EQ(inc.closed_ptr, inc.opened_ptr);
    EXPECT_EQ(inc.parent, NULL);
    UE_REL(b); UE_REL(e);
}
TEST(d3d10_utils_effects_shader_include_system)
{
    REQUIRE(CS("d3d10.dll"));
    inc_t inc = { &k_incvtbl, 0, 0, (D3D_INCLUDE_TYPE)-1, "", NULL, NULL, NULL, "float4 helper() { return float4(0,1,0,1); }\n", S_OK };
    const char *src = "#include <sysheader.h>\nfloat4 main() : SV_Target { return helper(); }\n";
    ID3D10Blob *b = NULL, *e = NULL;
    HRESULT hr = CS("d3d10.dll")(src, strlen(src), "inc.hlsl", NULL, (ID3D10Include *)&inc, "main", "ps_4_0", 0, &b, &e);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(inc.type, D3D_INCLUDE_SYSTEM);
    EXPECT_STR(inc.name, "sysheader.h");
    UE_REL(b); UE_REL(e);
}
TEST(d3d10_utils_effects_shader_include_open_failure)
{
    REQUIRE(CS("d3d10.dll"));
    inc_t inc = { &k_incvtbl, 0, 0, (D3D_INCLUDE_TYPE)-1, "", NULL, NULL, NULL, "", E_FAIL };
    const char *src = "#include \"missing.hlsli\"\nfloat4 main() : SV_Target { return 0; }\n";
    ID3D10Blob *b = NULL, *e = NULL;
    HRESULT hr = CS("d3d10.dll")(src, strlen(src), "inc.hlsl", NULL, (ID3D10Include *)&inc, "main", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr));
    EXPECT_EQ(b, NULL);
    EXPECT_EQ(inc.opens, 1);
    EXPECT_EQ(inc.closes, 0);
    if (e) { char t[512]; t_note("include failure hr=%s: %.200s", d3dh_hr_name(hr), text_of(e, t, sizeof t)); }
    UE_REL(b); UE_REL(e);
}
TEST(d3d10_utils_effects_shader_include_nested_parent_data)
{
    REQUIRE(CS("d3d10.dll"));
    inc_t inc = { &k_incvtbl, 0, 0, (D3D_INCLUDE_TYPE)-1, "", NULL, NULL, NULL, "float4 helper() { return 1; }\n", S_OK };
    /* the include handler is called once per distinct #include */
    const char *src = "#include \"a.h\"\n#include \"a.h\"\nfloat4 main() : SV_Target { return helper(); }\n";
    ID3D10Blob *b = NULL, *e = NULL;
    HRESULT hr = CS("d3d10.dll")(src, strlen(src), "inc.hlsl", NULL, (ID3D10Include *)&inc, "main", "ps_4_0", 0, &b, &e);
    /* redefining helper() twice is an error, but each open must still be paired with a close */
    t_note("double include hr=%s opens=%d closes=%d", d3dh_hr_name(hr), inc.opens, inc.closes);
    EXPECT_EQ(inc.opens, inc.closes);
    CHECK(inc.opens >= 1);
    UE_REL(b); UE_REL(e);
}

/* ---- disassembly ---- */
typedef HRESULT (WINAPI *dis_fn)(const void *, SIZE_T, BOOL, const char *, ID3D10Blob **);
#define DIS(dll) ((dis_fn)t_proc_in(dll, "D3D10DisassembleShader"))

TEST(d3d10_utils_effects_disasm_null_args)
{
    NOWINE("emulator dereferences NULL bytecode");
    REQUIRE(DIS("d3d10.dll"));
    ID3D10Blob *b = NULL, *sh = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    EXPECT_HR(DIS("d3d10.dll")(NULL, 100, FALSE, NULL, &b), E_D3DERR_INVALIDCALL);
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, NULL), E_D3DERR_INVALIDCALL);
    EXPECT_HR(DIS("d3d10.dll")(NULL, 0, FALSE, NULL, NULL), E_D3DERR_INVALIDCALL);
    EXPECT_EQ(b, NULL);
    UE_REL(sh);
}
TEST(d3d10_utils_effects_disasm_size_must_match_container)
{
    REQUIRE(DIS("d3d10.dll"));
    for (int who = 0; who < 2; who++) {
        ID3D10Blob *sh = NULL, *d = NULL;
        if (FAILED(make_shader(who, 1, &sh))) { CHECK(0); continue; }
        SIZE_T n = ue_bs(sh);
        void *copy = malloc(n + 64);
        memset(copy, 0, n + 64);
        memcpy(copy, ue_bp(sh), n);
        EXPECT_HR(DIS("d3d10.dll")(copy, n - 1, FALSE, NULL, &d), E_D3DERR_INVALIDCALL);
        EXPECT_HR(DIS("d3d10.dll")(copy, n + 1, FALSE, NULL, &d), E_D3DERR_INVALIDCALL);
        EXPECT_HR(DIS("d3d10.dll")(copy, 0, FALSE, NULL, &d), E_D3DERR_INVALIDCALL);
        EXPECT_HR(DIS("d3d10.dll")(copy, 4, FALSE, NULL, &d), E_D3DERR_INVALIDCALL);
        EXPECT_EQ(d, NULL);
        free(copy);
        UE_REL(sh);
    }
}
TEST(d3d10_utils_effects_disasm_header_and_profile)
{
    REQUIRE(DIS("d3d10.dll"));
    for (int who = 0; who < 2; who++) for (int st = 0; st < 3; st++) {
        if (st == 2 && t_is_wine()) continue;
        ID3D10Blob *sh = NULL, *d = NULL;
        if (FAILED(make_shader(who, st, &sh))) { CHECK(0); continue; }
        HRESULT hr = DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, &d);
        t_check(hr == S_OK && d, __FILE__, __LINE__, "disassemble ok", "%s %s: %s", k_who[who], k_stage[st], d3dh_hr_name(hr));
        if (d) {
            static const char *const prof[3] = { "vs_4_0", "ps_4_0", "gs_4_0" };
            CHECK(blob_has(d, "//\n// Generated by "));
            CHECK(blob_has(d, prof[st]));
            CHECK(blob_has(d, "ret"));
            CHECK(!blob_has(d, "<pre>"));
        }
        UE_REL(d); UE_REL(sh);
    }
}
TEST(d3d10_utils_effects_disasm_mentions_declarations)
{
    REQUIRE(DIS("d3d10.dll"));
    ID3D10Blob *sh = NULL, *d = NULL;
    if (FAILED(make_shader(0, 1, &sh))) { CHECK(0); return; }
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, &d), S_OK);
    if (d) {
        CHECK(blob_has(d, "dcl_constantbuffer"));
        CHECK(blob_has(d, "dcl_sampler"));
        CHECK(blob_has(d, "dcl_resource_texture2d"));
        CHECK(blob_has(d, "dcl_output o0.xyzw"));
        CHECK(blob_has(d, "sample"));
        CHECK(blob_has(d, "cb0"));
    }
    UE_REL(d); UE_REL(sh);
}
TEST(d3d10_utils_effects_disasm_resource_table_comments)
{
    REQUIRE(DIS("d3d10.dll"));
    ID3D10Blob *sh = NULL, *d = NULL;
    if (FAILED(make_shader(1, 1, &sh))) { CHECK(0); return; }
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, &d), S_OK);
    if (d) {
        CHECK(blob_has(d, "cbuffer cb0"));
        CHECK(blob_has(d, "float4 tint;"));
        CHECK(blob_has(d, "tex0"));
        CHECK(blob_has(d, "smp0"));
        CHECK(blob_has(d, "SV_Position"));
        CHECK(blob_has(d, "TEXCOORD"));
        CHECK(blob_has(d, "SV_Target"));
    }
    UE_REL(d); UE_REL(sh);
}
TEST(d3d10_utils_effects_disasm_comment_string)
{
    REQUIRE(DIS("d3d10.dll"));
    ID3D10Blob *sh = NULL, *d1 = NULL, *d2 = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, "// my marker comment\n", &d1), S_OK);
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, &d2), S_OK);
    CHECK(blob_has(d1, "// my marker comment"));
    CHECK(!blob_has(d2, "my marker"));
    UE_REL(d1); UE_REL(d2); UE_REL(sh);
}
TEST(d3d10_utils_effects_disasm_color_code_html)
{
    REQUIRE(DIS("d3d10.dll"));
    ID3D10Blob *sh = NULL, *d = NULL;
    if (FAILED(make_shader(0, 1, &sh))) { CHECK(0); return; }
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), TRUE, NULL, &d), S_OK);
    if (d) {
        const char *p = ue_bp(d);
        CHECK(ue_bs(d) > 5 && !strncmp(p, "<pre>", 5));
        CHECK(blob_has(d, "<body bgcolor=\"#000000\">"));
        CHECK(blob_has(d, "<font color"));
    }
    UE_REL(d); UE_REL(sh);
}
TEST(d3d10_utils_effects_disasm_deterministic)
{
    REQUIRE(DIS("d3d10.dll"));
    ID3D10Blob *sh = NULL, *a = NULL, *b = NULL;
    if (FAILED(make_shader(1, 0, &sh))) { CHECK(0); return; }
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, &a), S_OK);
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, &b), S_OK);
    if (a && b) CHECK(ue_bs(a) == ue_bs(b) && !memcmp(ue_bp(a), ue_bp(b), ue_bs(a)));
    UE_REL(a); UE_REL(b); UE_REL(sh);
}
TEST(d3d10_utils_effects_disasm_via_d3d10_1)
{
    REQUIRE(DIS("d3d10_1.dll"));
    ID3D10Blob *sh = NULL, *d = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    EXPECT_HR(DIS("d3d10_1.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, &d), S_OK);
    CHECK(blob_has(d, "vs_4_0"));
    EXPECT_HR(DIS("d3d10_1.dll")(NULL, 0, FALSE, NULL, &d), E_D3DERR_INVALIDCALL);
    UE_REL(d); UE_REL(sh);
}
TEST(d3d10_utils_effects_disasm_same_as_d3dcompiler_shape)
{
    /* both disassemblers print the same instruction stream for a simple shader */
    REQUIRE(DIS("d3d10.dll") && d3dh_D3DDisassemble());
    ID3D10Blob *sh = NULL, *a = NULL;
    ID3DBlob *c = NULL;
    if (FAILED(make_shader(1, 0, &sh))) { CHECK(0); return; }
    EXPECT_HR(DIS("d3d10.dll")(ue_bp(sh), ue_bs(sh), FALSE, NULL, &a), S_OK);
    HRESULT hr = d3dh_D3DDisassemble()(ue_bp(sh), ue_bs(sh), 0, NULL, &c);
    EXPECT_HR(hr, S_OK);
    if (a && c) {
        char ta[8192], tc[8192];
        text_of(a, ta, sizeof ta); text_of((ID3D10Blob *)c, tc, sizeof tc);
        const char *ia = strstr(ta, "vs_4_0"), *ic = strstr(tc, "vs_4_0");
        CHECK(ia && ic);
        if (ia && ic) t_note("d3d10 and d3dcompiler disassembly tails %s", strcmp(ia, ic) ? "differ" : "match");
    }
    UE_REL(a); SAFE_RELEASE(c); UE_REL(sh);
}

/* ---- signature blobs ---- */
typedef HRESULT (WINAPI *sig_fn)(const void *, SIZE_T, ID3D10Blob **);
static const char *const k_sig_names[3] = { "D3D10GetInputSignatureBlob", "D3D10GetOutputSignatureBlob", "D3D10GetInputAndOutputSignatureBlob" };

static void sig_check(const char *dll, int which)
{
    sig_fn f = (sig_fn)t_proc_in(dll, k_sig_names[which]);
    if (!f) { t_skip("%s missing", k_sig_names[which]); return; }
    for (int who = 0; who < 2; who++) for (int st = 0; st < 3; st++) {
        if (st == 2 && t_is_wine()) continue;
        ID3D10Blob *sh = NULL, *sig = (ID3D10Blob *)(ULONG_PTR)1;
        if (FAILED(make_shader(who, st, &sh))) { CHECK(0); continue; }
        HRESULT hr = f(ue_bp(sh), ue_bs(sh), &sig);
        t_check(hr == S_OK && sig, __FILE__, __LINE__, "signature blob", "%s %s %s: %s", k_sig_names[which], k_who[who], k_stage[st], d3dh_hr_name(hr));
        if (hr == S_OK && sig) {
            const unsigned char *p = ue_bp(sig);
            SIZE_T n = ue_bs(sig);
            t_check(n > 32 && !memcmp(p, "DXBC", 4) && *(const DWORD *)(p + 24) == n, __FILE__, __LINE__, "stand-alone DXBC container", "%s %s", k_who[who], k_stage[st]);
            DWORD nc = n >= 32 ? *(const DWORD *)(p + 28) : 0;
            t_check(nc == (which == 2 ? 2u : 1u), __FILE__, __LINE__, "chunk count", "%u", nc);
            int hasi = ue_dxbc_chunk(p, n, FOURCC('I','S','G','N'), NULL) != NULL;
            int haso = ue_dxbc_chunk(p, n, FOURCC('O','S','G','N'), NULL) != NULL;
            t_check(hasi == (which != 1) && haso == (which != 0), __FILE__, __LINE__, "chunks present", "isgn=%d osgn=%d", hasi, haso);
            /* the signature chunk is a byte copy of the original */
            if (hasi) {
                DWORD a, b;
                const unsigned char *x = ue_dxbc_chunk(p, n, FOURCC('I','S','G','N'), &a);
                const unsigned char *y = ue_dxbc_chunk(ue_bp(sh), ue_bs(sh), FOURCC('I','S','G','N'), &b);
                CHECK(x && y && a == b && !memcmp(x, y, a));
            }
        }
        UE_REL(sig); UE_REL(sh);
    }
}
TEST(d3d10_utils_effects_sigblob_input) { sig_check("d3d10.dll", 0); }
TEST(d3d10_utils_effects_sigblob_output) { sig_check("d3d10.dll", 1); }
TEST(d3d10_utils_effects_sigblob_input_output) { sig_check("d3d10.dll", 2); }
TEST(d3d10_utils_effects_sigblob_input_d3d10_1) { sig_check("d3d10_1.dll", 0); }
TEST(d3d10_utils_effects_sigblob_output_d3d10_1) { sig_check("d3d10_1.dll", 1); }
TEST(d3d10_utils_effects_sigblob_input_output_d3d10_1) { sig_check("d3d10_1.dll", 2); }

TEST(d3d10_utils_effects_sigblob_null_data)
{
    for (int i = 0; i < 3; i++) {
        sig_fn f = (sig_fn)t_proc_in("d3d10.dll", k_sig_names[i]);
        REQUIRE(f);
        ID3D10Blob *s = (ID3D10Blob *)(ULONG_PTR)1;
        HRESULT hr = f(NULL, 100, &s);
        t_check(hr == E_FAIL_, __FILE__, __LINE__, "NULL data", "%s: %s", k_sig_names[i], d3dh_hr_name(hr));
    }
}
TEST(d3d10_utils_effects_sigblob_garbage_data)
{
    unsigned char junk[128];
    for (int i = 0; i < 128; i++) junk[i] = (unsigned char)(i * 3 + 1);
    for (int i = 0; i < 3; i++) {
        sig_fn f = (sig_fn)t_proc_in("d3d10.dll", k_sig_names[i]);
        REQUIRE(f);
        ID3D10Blob *s = NULL;
        HRESULT hr = f(junk, sizeof junk, &s);
        t_check(FAILED(hr) && !s, __FILE__, __LINE__, "garbage rejected", "%s: %s", k_sig_names[i], d3dh_hr_name(hr));
        hr = f(junk, 0, &s);
        t_check(FAILED(hr) && !s, __FILE__, __LINE__, "zero size rejected", "%s: %s", k_sig_names[i], d3dh_hr_name(hr));
    }
}
TEST(d3d10_utils_effects_sigblob_truncated)
{
    ID3D10Blob *sh = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    sig_fn f = (sig_fn)t_proc_in("d3d10.dll", "D3D10GetInputSignatureBlob");
    REQUIRE(f);
    SIZE_T n = ue_bs(sh);
    static const SIZE_T cuts[] = { 0, 4, 31, 32, 40, 100 };
    for (unsigned i = 0; i < sizeof cuts / sizeof *cuts; i++) {
        ID3D10Blob *s = NULL;
        HRESULT hr = f(ue_bp(sh), cuts[i] < n ? cuts[i] : n - 1, &s);
        t_check(FAILED(hr) && !s, __FILE__, __LINE__, "truncated container", "size %u: %s", (unsigned)cuts[i], d3dh_hr_name(hr));
    }
    ID3D10Blob *s = NULL;
    HRESULT hr = f(ue_bp(sh), n - 1, &s);
    CHECK(FAILED(hr) && !s);
    UE_REL(s); UE_REL(sh);
}
TEST(d3d10_utils_effects_sigblob_null_out)
{
    ID3D10Blob *sh = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    sig_fn f = (sig_fn)t_proc_in("d3d10.dll", "D3D10GetInputSignatureBlob");
    REQUIRE(f);
    HRESULT hr = f(ue_bp(sh), ue_bs(sh), NULL);
    t_note("signature blob with NULL output: %s", d3dh_hr_name(hr));
    UE_REL(sh);
}
TEST(d3d10_utils_effects_sigblob_usable_for_input_layout)
{
    /* the stand-alone container is a valid bytecode argument for CreateInputLayout-style consumers: re-extracting from it works */
    ID3D10Blob *sh = NULL, *sig = NULL, *sig2 = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    sig_fn f = (sig_fn)t_proc_in("d3d10.dll", "D3D10GetInputSignatureBlob");
    REQUIRE(f);
    EXPECT_HR(f(ue_bp(sh), ue_bs(sh), &sig), S_OK);
    if (sig) {
        HRESULT hr = f(ue_bp(sig), ue_bs(sig), &sig2);
        EXPECT_HR(hr, S_OK);
        if (sig2) CHECK(ue_bs(sig2) == ue_bs(sig) && !memcmp(ue_bp(sig), ue_bp(sig2), ue_bs(sig)));
    }
    UE_REL(sig2); UE_REL(sig); UE_REL(sh);
}
TEST(d3d10_utils_effects_sigblob_output_of_input_container_missing)
{
    /* an input-only container has no OSGN to extract */
    ID3D10Blob *sh = NULL, *sig = NULL, *sig2 = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    sig_fn fi = (sig_fn)t_proc_in("d3d10.dll", "D3D10GetInputSignatureBlob");
    sig_fn fo = (sig_fn)t_proc_in("d3d10.dll", "D3D10GetOutputSignatureBlob");
    REQUIRE(fi && fo);
    EXPECT_HR(fi(ue_bp(sh), ue_bs(sh), &sig), S_OK);
    if (sig) {
        HRESULT hr = fo(ue_bp(sig), ue_bs(sig), &sig2);
        EXPECT_HR(hr, E_FAIL_);
        EXPECT_EQ(sig2, NULL);
    }
    UE_REL(sig2); UE_REL(sig); UE_REL(sh);
}

/* ---- debug info ---- */
typedef HRESULT (WINAPI *dbg_fn)(const void *, SIZE_T, ID3D10Blob **);
TEST(d3d10_utils_effects_debuginfo_null_data)
{
    dbg_fn f = (dbg_fn)t_proc_in("d3d10.dll", "D3D10GetShaderDebugInfo");
    REQUIRE(f);
    ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1;
    EXPECT_HR(f(NULL, 64, &b), E_D3DERR_INVALIDCALL);
    EXPECT_EQ(b, NULL);
    EXPECT_HR(f(NULL, 0, &b), E_D3DERR_INVALIDCALL);
}
TEST(d3d10_utils_effects_debuginfo_null_out)
{
    dbg_fn f = (dbg_fn)t_proc_in("d3d10.dll", "D3D10GetShaderDebugInfo");
    REQUIRE(f);
    ID3D10Blob *sh = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    EXPECT_HR(f(ue_bp(sh), ue_bs(sh), NULL), E_D3DERR_INVALIDCALL);
    UE_REL(sh);
}
TEST(d3d10_utils_effects_debuginfo_bad_magic)
{
    dbg_fn f = (dbg_fn)t_proc_in("d3d10.dll", "D3D10GetShaderDebugInfo");
    REQUIRE(f);
    unsigned char junk[64];
    memset(junk, 0x41, sizeof junk);
    ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1;
    EXPECT_HR(f(junk, sizeof junk, &b), E_FAIL_);
    EXPECT_EQ(b, NULL);
    memcpy(junk, "DXBD", 4);
    EXPECT_HR(f(junk, sizeof junk, &b), E_FAIL_);
    memcpy(junk, "dxbc", 4);
    EXPECT_HR(f(junk, sizeof junk, &b), E_FAIL_);
}
TEST(d3d10_utils_effects_debuginfo_shader_without_debug_chunk)
{
    dbg_fn f = (dbg_fn)t_proc_in("d3d10.dll", "D3D10GetShaderDebugInfo");
    REQUIRE(f);
    ID3D10Blob *sh = NULL, *b = NULL;
    if (FAILED(make_shader(0, 0, &sh))) { CHECK(0); return; }
    CHECK(ue_dxbc_chunk(ue_bp(sh), ue_bs(sh), FOURCC('S','D','B','G'), NULL) == NULL);
    HRESULT hr = f(ue_bp(sh), ue_bs(sh), &b);
    t_note("debug info of a shader without SDBG: %s, blob %p size %u", d3dh_hr_name(hr), (void *)b, (unsigned)ue_bs(b));
    UE_REL(b); UE_REL(sh);
}
TEST(d3d10_utils_effects_debuginfo_via_d3d10_1)
{
    dbg_fn f = (dbg_fn)t_proc_in("d3d10_1.dll", "D3D10GetShaderDebugInfo");
    REQUIRE(f);
    ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1;
    EXPECT_HR(f(NULL, 64, &b), E_D3DERR_INVALIDCALL);
    EXPECT_EQ(b, NULL);
}

/* ---- preprocessor ---- */
typedef HRESULT (WINAPI *pp_fn)(const char *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, ID3D10Blob **, ID3D10Blob **);
#define PP(dll) ((pp_fn)t_proc_in(dll, "D3D10PreprocessShader"))
TEST(d3d10_utils_effects_preprocess_null_args)
{
    NOWINE("D3D10PreprocessShader is a stub");
    REQUIRE(PP("d3d10.dll"));
    ID3D10Blob *t = NULL, *e = NULL;
    EXPECT_HR(PP("d3d10.dll")(NULL, 10, "x.hlsl", NULL, NULL, &t, &e), E_INVALIDARG_);
    EXPECT_HR(PP("d3d10.dll")("int a;", 6, "x.hlsl", NULL, NULL, NULL, &e), E_INVALIDARG_);
    EXPECT_HR(PP("d3d10.dll")(NULL, 0, NULL, NULL, NULL, NULL, NULL), E_INVALIDARG_);
    EXPECT_EQ(t, NULL);
}
TEST(d3d10_utils_effects_preprocess_macro_expansion)
{
    NOWINE("D3D10PreprocessShader is a stub");
    REQUIRE(PP("d3d10.dll"));
    const char *src = "#define TWICE(x) ((x)+(x))\nfloat v = TWICE(3);\n#define K 42\nint w = K;\n";
    ID3D10Blob *t = NULL, *e = NULL;
    HRESULT hr = PP("d3d10.dll")(src, strlen(src), "pp.hlsl", NULL, NULL, &t, &e);
    EXPECT_HR(hr, S_OK);
    CHECK(blob_has(t, "((3)+(3))"));
    CHECK(blob_has(t, "int w = 42;"));
    CHECK(!blob_has(t, "#define"));
    UE_REL(t); UE_REL(e);
}
TEST(d3d10_utils_effects_preprocess_conditionals)
{
    NOWINE("D3D10PreprocessShader is a stub");
    REQUIRE(PP("d3d10.dll"));
    const char *src = "#if A_VALUE > 2\nint taken = 1;\n#else\nint skipped = 2;\n#endif\n#ifdef NOT_DEFINED\nint hidden;\n#endif\n";
    D3D10_SHADER_MACRO defs[] = { { "A_VALUE", "5" }, { NULL, NULL } };
    ID3D10Blob *t = NULL, *e = NULL;
    EXPECT_HR(PP("d3d10.dll")(src, strlen(src), "pp.hlsl", defs, NULL, &t, &e), S_OK);
    CHECK(blob_has(t, "int taken = 1;"));
    CHECK(!blob_has(t, "skipped"));
    CHECK(!blob_has(t, "hidden"));
    UE_REL(t); UE_REL(e);
}
TEST(d3d10_utils_effects_preprocess_error_directive)
{
    NOWINE("D3D10PreprocessShader is a stub");
    REQUIRE(PP("d3d10.dll"));
    const char *src = "int a;\n#error stop here please\nint b;\n";
    ID3D10Blob *t = NULL, *e = NULL;
    HRESULT hr = PP("d3d10.dll")(src, strlen(src), "pp.hlsl", NULL, NULL, &t, &e);
    CHECK(FAILED(hr));
    CHECK(e != NULL);
    if (e) CHECK(blob_has(e, "stop here please"));
    UE_REL(t); UE_REL(e);
}
TEST(d3d10_utils_effects_preprocess_include)
{
    NOWINE("D3D10PreprocessShader is a stub");
    REQUIRE(PP("d3d10.dll"));
    inc_t inc = { &k_incvtbl, 0, 0, (D3D_INCLUDE_TYPE)-1, "", NULL, NULL, NULL, "int from_include = 7;\n", S_OK };
    const char *src = "#include \"x.h\"\nint after;\n";
    ID3D10Blob *t = NULL, *e = NULL;
    EXPECT_HR(PP("d3d10.dll")(src, strlen(src), "pp.hlsl", NULL, (ID3D10Include *)&inc, &t, &e), S_OK);
    CHECK(blob_has(t, "int from_include = 7;"));
    CHECK(blob_has(t, "int after;"));
    EXPECT_EQ(inc.opens, 1);
    EXPECT_EQ(inc.closes, 1);
    UE_REL(t); UE_REL(e);
}
TEST(d3d10_utils_effects_preprocess_null_error_out_ok)
{
    NOWINE("D3D10PreprocessShader is a stub");
    REQUIRE(PP("d3d10.dll"));
    ID3D10Blob *t = NULL;
    EXPECT_HR(PP("d3d10.dll")("int a;\n", 7, "pp.hlsl", NULL, NULL, &t, NULL), S_OK);
    CHECK(blob_has(t, "int a;"));
    UE_REL(t);
}
TEST(d3d10_utils_effects_preprocess_via_d3d10_1)
{
    NOWINE("D3D10PreprocessShader is a stub");
    NOWINE("D3D10PreprocessShader of d3d10_1 is a stub");
    REQUIRE(PP("d3d10_1.dll"));
    ID3D10Blob *t = NULL, *e = NULL;
    EXPECT_HR(PP("d3d10_1.dll")("#define Q 9\nint z = Q;\n", 23, "pp.hlsl", NULL, NULL, &t, &e), S_OK);
    CHECK(blob_has(t, "int z = 9;"));
    UE_REL(t); UE_REL(e);
    EXPECT_HR(PP("d3d10_1.dll")(NULL, 0, "pp.hlsl", NULL, NULL, &t, &e), E_INVALIDARG_);
}
