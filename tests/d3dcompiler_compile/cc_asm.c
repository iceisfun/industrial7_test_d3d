/* D3DAssemble, D3DDisassemble, D3DDisassembleRegion, D3DGetTraceInstructionOffsets, D3DDisassemble10Effect. */
#include "cc.h"

/* the emulator stubs these exports with an aborting handler */
#define NEED_NATIVE(fn) do { REQUIRE(CCN(fn)); if (t_is_wine()) { t_skip(#fn " is not implemented by the emulator"); return; } } while (0)
#define NEED_ASM() do { REQUIRE(CCN(D3DAssemble)); } while (0)
#define NEED_DIS() do { REQUIRE(CCF(D3DCompile)); REQUIRE(CCF(D3DDisassemble)); } while (0)

static const char *cc_ps20_asm =
    "ps_2_0\n"
    "def c0, 1, 0, 0, 1\n"
    "mov oC0, c0\n";

typedef struct { HRESULT hr; ID3DBlob *code, *errb; char err[4096]; } cc_asm_r;

static HRESULT cc_asm(cc_asm_r *r, const char *src, SIZE_T len, const char *name, const D3D_SHADER_MACRO *d, ID3DInclude *inc, UINT flags)
{
    memset(r, 0, sizeof *r);
    r->hr = CCN(D3DAssemble)(src, len, name, d, inc, flags, &r->code, &r->errb);
    cc_blobstr(r->errb, r->err, sizeof r->err);
    return r->hr;
}
static void cc_asm_free(cc_asm_r *r) { SAFE_RELEASE(r->code); SAFE_RELEASE(r->errb); }

static int cc_has_words(ID3DBlob *b, const DWORD *w, int n)
{
    SIZE_T sz = cc_size(b), i;
    int j;
    const unsigned char *p = cc_ptr(b);
    for (i = 0; i + (SIZE_T)n * 4 <= sz; i += 4) {
        for (j = 0; j < n; j++) if (cc_rd32(p + i + j * 4) != w[j]) break;
        if (j == n) return 1;
    }
    return 0;
}

/* ---------------- D3DAssemble ---------------- */
TEST(d3dcompiler_compile_asm_ps20_tokens)
{
    cc_asm_r r;
    static const DWORD mov[] = { 0x02000001, 0x800F0800, 0xA0E40000 };
    static const DWORD def[] = { 0x05000051, 0xA00F0000, 0x3F800000, 0x00000000, 0x00000000, 0x3F800000 };
    NEED_ASM();
    EXPECT_HR(cc_asm(&r, cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, 0), S_OK);
    if (r.code) {
        SIZE_T n = cc_size(r.code);
        CHECK(n >= 24 && (n % 4) == 0);
        EXPECT_EQ(cc_rd32(cc_ptr(r.code)), 0xFFFF0200);
        EXPECT_EQ(cc_rd32(cc_ptr(r.code) + n - 4), 0x0000FFFF);
        CHECK(cc_has_words(r.code, def, 6));
        CHECK(cc_has_words(r.code, mov, 3));
    }
    CHECK(r.errb == NULL);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_version_tokens)
{
    static const struct { const char *src; DWORD tok; } tab[] = {
        { "vs_1_1\nmov oPos, v0\n", 0xFFFE0101 },
        { "vs_2_0\nmov oPos, v0\n", 0xFFFE0200 },
        { "vs_3_0\ndcl_position v0\ndcl_position o0\nmov o0, v0\n", 0xFFFE0300 },
        { "ps_1_1\nmov r0, c0\n", 0xFFFF0101 },
        { "ps_1_4\nmov r0, c0\n", 0xFFFF0104 },
        { "ps_2_0\nmov oC0, c0\n", 0xFFFF0200 },
        { "ps_3_0\nmov oC0, c0\n", 0xFFFF0300 },
    };
    unsigned i;
    NEED_ASM();
    for (i = 0; i < sizeof tab / sizeof *tab; i++) {
        cc_asm_r r;
        cc_asm(&r, tab[i].src, strlen(tab[i].src), "t.asm", NULL, NULL, 0);
        CHECKF(r.hr == S_OK, "'%.12s' hr=0x%08lx err=%.120s", tab[i].src, (unsigned long)r.hr, r.err);
        if (r.code) {
            EXPECT_EQ(cc_rd32(cc_ptr(r.code)), tab[i].tok);
            EXPECT_EQ(cc_rd32(cc_ptr(r.code) + cc_size(r.code) - 4), 0x0000FFFF);
        }
        cc_asm_free(&r);
    }
}

TEST(d3dcompiler_compile_asm_flags_valid)
{
    static const UINT ok[] = { 0, D3DCOMPILE_DEBUG, D3DCOMPILE_SKIP_VALIDATION, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_VALIDATION };
    unsigned i;
    NEED_ASM();
    for (i = 0; i < 4; i++) {
        cc_asm_r r;
        cc_asm(&r, cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, ok[i]);
        CHECKF(r.hr == S_OK, "flags 0x%x hr=0x%08lx", ok[i], (unsigned long)r.hr);
        CHECK(r.code != NULL);
        cc_asm_free(&r);
    }
}

TEST(d3dcompiler_compile_asm_flags_invalid)
{
    WIN_ONLY();
    static const UINT bad[] = { 0x4, 0x8, 0x10, 0x20, 0x100, 0x10000, 0x80000000, 0xFFFFFFFC, 0xFFFFFFFF, D3DCOMPILE_SKIP_OPTIMIZATION };
    unsigned i;
    NEED_ASM();
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        cc_asm_r r;
        cc_asm(&r, cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, bad[i]);
        CHECKF(r.hr == CC_INVALIDCALL, "flags 0x%x hr=0x%08lx", bad[i], (unsigned long)r.hr);
        CHECKF(r.code == NULL, "flags 0x%x produced code", bad[i]);
        cc_asm_free(&r);
    }
}

TEST(d3dcompiler_compile_asm_outputs_optional)
{
    HRESULT hr;
    ID3DBlob *code = NULL;
    NEED_ASM();
    hr = CCN(D3DAssemble)(cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, 0, NULL, NULL);
    EXPECT_HR(hr, S_OK);
    hr = CCN(D3DAssemble)(cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, 0, &code, NULL);
    EXPECT_HR(hr, S_OK);
    CHECK(code != NULL);
    SAFE_RELEASE(code);
}

TEST(d3dcompiler_compile_asm_output_cleared_on_invalid_flags)
{
    WIN_ONLY();
    ID3DBlob *code = (ID3DBlob *)(ULONG_PTR)0x10, *err = NULL;
    HRESULT hr;
    NEED_ASM();
    hr = CCN(D3DAssemble)(cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, 0x4, &code, &err);
    EXPECT_HR(hr, CC_INVALIDCALL);
    CHECK(code == NULL);
    SAFE_RELEASE(err);
}

TEST(d3dcompiler_compile_asm_null_source_name)
{
    cc_asm_r r;
    NEED_ASM();
    EXPECT_HR(cc_asm(&r, cc_ps20_asm, strlen(cc_ps20_asm), NULL, NULL, NULL, 0), S_OK);
    cc_asm_free(&r);
    cc_asm(&r, "ps_2_0\nbogus_op r0\n", 19, NULL, NULL, NULL, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.errb != NULL);
    t_note("unnamed asm error text: %.100s", r.err);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_unknown_instruction)
{
    WIN_ONLY();
    static const char *src = "ps_2_0\nbogus_op r0, c0\nmov oC0, r0\n";
    cc_asm_r r;
    NEED_ASM();
    cc_asm(&r, src, strlen(src), "bad.asm", NULL, NULL, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    CHECK(r.errb != NULL);
    CHECK(strstr(r.err, "bad.asm(2") != NULL);
    t_note("unknown instruction: hr=0x%08lx %.140s", (unsigned long)r.hr, r.err);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_missing_version)
{
    static const char *src = "mov oC0, c0\n";
    cc_asm_r r;
    NEED_ASM();
    cc_asm(&r, src, strlen(src), "t.asm", NULL, NULL, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    CHECK(r.errb != NULL);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_unknown_version)
{
    static const char *bad[] = { "ps_9_9\nmov oC0, c0\n", "xs_2_0\nmov oC0, c0\n" };
    unsigned i;
    NEED_ASM();
    for (i = 0; i < 2; i++) {
        cc_asm_r r;
        cc_asm(&r, bad[i], strlen(bad[i]), "t.asm", NULL, NULL, 0);
        CHECKF((LONG)r.hr < 0, "'%.12s' hr=0x%08lx", bad[i], (unsigned long)r.hr);
        CHECKF(r.code == NULL, "'%.12s' produced code", bad[i]);
        cc_asm_free(&r);
    }
}

TEST(d3dcompiler_compile_asm_register_out_of_range)
{
    static const char *src = "ps_2_0\nmov r99, c0\nmov oC0, r0\n";
    cc_asm_r r;
    NEED_ASM();
    cc_asm(&r, src, strlen(src), "t.asm", NULL, NULL, 0);
    t_note("r99 in ps_2_0: hr=0x%08lx %.120s", (unsigned long)r.hr, r.err);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_comments_and_blank_lines)
{
    static const char *src = "// leading comment\nps_2_0 ; version\n\n\ndef c0, 0.5, 0.5, 0.5, 1 // define\n   mov   oC0,   c0   \n";
    cc_asm_r r;
    NEED_ASM();
    EXPECT_HR(cc_asm(&r, src, strlen(src), "t.asm", NULL, NULL, 0), S_OK);
    CHECK(r.code != NULL);
    if (r.code) EXPECT_EQ(cc_rd32(cc_ptr(r.code)), 0xFFFF0200);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_case_insensitive_mnemonics)
{
    static const char *src = "PS_2_0\nDEF c0, 1, 1, 1, 1\nMOV oC0, C0\n";
    cc_asm_r r, ref;
    NEED_ASM();
    cc_asm(&r, src, strlen(src), "t.asm", NULL, NULL, 0);
    t_note("upper-case mnemonics: hr=0x%08lx", (unsigned long)r.hr);
    cc_asm(&ref, "ps_2_0\ndef c0, 1, 1, 1, 1\nmov oC0, c0\n", 38, "t.asm", NULL, NULL, 0);
    if (SUCCEEDED(r.hr) && r.code && ref.code) {
        EXPECT_EQ(cc_size(r.code), cc_size(ref.code));
        if (cc_size(r.code) == cc_size(ref.code)) CHECK(!memcmp(cc_ptr(r.code), cc_ptr(ref.code), cc_size(r.code)));
    }
    cc_asm_free(&r); cc_asm_free(&ref);
}

TEST(d3dcompiler_compile_asm_macros_and_defines)
{
    static const D3D_SHADER_MACRO d[] = { { "SRC", "c0" }, { NULL, NULL } };
    static const char *src = "ps_2_0\ndef c0, 1, 0, 0, 1\nmov oC0, SRC\n";
    cc_asm_r a, b;
    NEED_ASM();
    cc_asm(&a, src, strlen(src), "t.asm", d, NULL, 0);
    EXPECT_HR(a.hr, S_OK);
    cc_asm(&b, cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, 0);
    if (a.code && b.code) {
        EXPECT_EQ(cc_size(a.code), cc_size(b.code));
        if (cc_size(a.code) == cc_size(b.code)) CHECK(!memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)));
    }
    cc_asm_free(&a); cc_asm_free(&b);
    cc_asm(&a, src, strlen(src), "t.asm", NULL, NULL, 0);
    CHECK((LONG)a.hr < 0);
    cc_asm_free(&a);
}

TEST(d3dcompiler_compile_asm_preprocessor_directives)
{
    static const char *src = "#define OUT oC0\n#if 1\nps_2_0\n#endif\ndef c0, 1, 0, 0, 1\nmov OUT, c0\n";
    cc_asm_r a, b;
    NEED_ASM();
    EXPECT_HR(cc_asm(&a, src, strlen(src), "t.asm", NULL, NULL, 0), S_OK);
    cc_asm(&b, cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, 0);
    if (a.code && b.code) {
        EXPECT_EQ(cc_size(a.code), cc_size(b.code));
        if (cc_size(a.code) == cc_size(b.code)) CHECK(!memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)));
    }
    cc_asm_free(&a); cc_asm_free(&b);
}

TEST(d3dcompiler_compile_asm_include_callback)
{
    static const char *src = "ps_2_0\n#include \"consts.inc\"\nmov oC0, c0\n";
    cc_inc inc;
    cc_asm_r r;
    NEED_ASM();
    cc_inc_init(&inc);
    cc_inc_add(&inc, "consts.inc", "def c0, 1, 0, 0, 1\n");
    EXPECT_HR(cc_asm(&r, src, strlen(src), "t.asm", NULL, (ID3DInclude *)&inc, 0), S_OK);
    EXPECT_EQ(inc.n_open, 1);
    EXPECT_EQ(inc.n_close, 1);
    EXPECT_EQ(inc.types[0], D3D_INCLUDE_LOCAL);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_deterministic)
{
    cc_asm_r a, b;
    NEED_ASM();
    cc_asm(&a, cc_ps20_asm, strlen(cc_ps20_asm), "one.asm", NULL, NULL, 0);
    cc_asm(&b, cc_ps20_asm, strlen(cc_ps20_asm), "two.asm", NULL, NULL, 0);
    if (a.code && b.code) {
        EXPECT_EQ(cc_size(a.code), cc_size(b.code));
        if (cc_size(a.code) == cc_size(b.code)) CHECK(!memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)));
    }
    cc_asm_free(&a); cc_asm_free(&b);
}

TEST(d3dcompiler_compile_asm_source_size_respected)
{
    static const char *src = "ps_2_0\ndef c0, 1, 0, 0, 1\nmov oC0, c0\nthis garbage is beyond the sized part\n";
    cc_asm_r r;
    NEED_ASM();
    EXPECT_HR(cc_asm(&r, src, strlen(cc_ps20_asm), "t.asm", NULL, NULL, 0), S_OK);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_sm4_text_note)
{
    static const char *src = "ps_4_0\ndcl_output o0.xyzw\nmov o0.xyzw, l(1,0,0,1)\nret\n";
    cc_asm_r r;
    NEED_ASM();
    cc_asm(&r, src, strlen(src), "t.asm", NULL, NULL, 0);
    t_note("SM4 text assembly: hr=0x%08lx size=%lu", (unsigned long)r.hr, (unsigned long)cc_size(r.code));
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_empty_source_note)
{
    cc_asm_r r;
    NEED_ASM();
    cc_asm(&r, "", 0, "t.asm", NULL, NULL, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_texture_sampling)
{
    static const char *src = "ps_2_0\ndcl t0.xy\ndcl_2d s0\ntexld r0, t0, s0\nmov oC0, r0\n";
    cc_asm_r r;
    NEED_ASM();
    cc_asm(&r, src, strlen(src), "t.asm", NULL, NULL, 0);
    EXPECT_HR(r.hr, S_OK);
    if (r.code) {
        EXPECT_EQ(cc_rd32(cc_ptr(r.code)), 0xFFFF0200);
        CHECK(cc_size(r.code) > 24);
    }
    cc_asm_free(&r);
}

TEST(d3dcompiler_compile_asm_roundtrip_through_disassembler)
{
    WIN_ONLY();
    cc_asm_r r;
    ID3DBlob *d = NULL;
    char txt[2048];
    HRESULT hr;
    NEED_ASM(); REQUIRE(CCF(D3DDisassemble));
    EXPECT_HR(cc_asm(&r, cc_ps20_asm, strlen(cc_ps20_asm), "t.asm", NULL, NULL, 0), S_OK);
    if (!r.code) { cc_asm_free(&r); return; }
    hr = CCF(D3DDisassemble)(cc_ptr(r.code), cc_size(r.code), 0, NULL, &d);
    EXPECT_HR(hr, S_OK);
    cc_blobstr(d, txt, sizeof txt);
    CHECK(strstr(txt, "ps_2_0") != NULL);
    CHECK(strstr(txt, "def c0, 1, 0, 0, 1") != NULL);
    CHECK(strstr(txt, "mov oC0, c0") != NULL);
    SAFE_RELEASE(d);
    cc_asm_free(&r);
}

/* ---------------- D3DDisassemble ---------------- */
static const char *cc_dis_src =
    "cbuffer cb : register(b0) { float4 k; };\n"
    "float4 main(float4 p : SV_Position) : SV_Target { return p * k + float4(1, 0, 0, 1); }\n";

static HRESULT cc_dis(cc_r *c, UINT flags, const char *comments, char *out, size_t cap)
{
    ID3DBlob *d = NULL;
    HRESULT hr = CCF(D3DDisassemble)(cc_ptr(c->code), cc_size(c->code), flags, comments, &d);
    cc_blobstr(d, out, cap);
    SAFE_RELEASE(d);
    return hr;
}

TEST(d3dcompiler_compile_disasm_basic)
{
    WIN_ONLY();
    static char txt[32768];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, 0, NULL, txt, sizeof txt), S_OK);
        CHECK(strstr(txt, "ps_5_0") != NULL);
        CHECK(strstr(txt, "dcl_output o0.xyzw") != NULL);
        CHECK(strstr(txt, "ret") != NULL);
        CHECK(strstr(txt, "dcl_constantbuffer") != NULL);
        t_note("disasm starts: %.70s", txt);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_header_lists_signatures)
{
    WIN_ONLY();
    static char txt[32768];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, 0, NULL, txt, sizeof txt), S_OK);
        CHECK(strstr(txt, "Input signature:") != NULL);
        CHECK(strstr(txt, "Output signature:") != NULL);
        CHECK(strstr(txt, "SV_Position") != NULL);
        CHECK(strstr(txt, "SV_Target") != NULL);
        CHECK(strstr(txt, "cbuffer cb") != NULL);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_profile_names)
{
    static const struct { const char *t; const char *src; const char *want; } tab[] = {
        { "ps_4_0", CC_PS_SRC, "ps_4_0" }, { "ps_4_1", CC_PS_SRC, "ps_4_1" }, { "ps_5_0", CC_PS_SRC, "ps_5_0" },
        { "vs_4_0", CC_VS_SRC, "vs_4_0" }, { "vs_5_0", CC_VS_SRC, "vs_5_0" },
    };
    static char txt[32768];
    unsigned i;
    NEED_DIS();
    for (i = 0; i < sizeof tab / sizeof *tab; i++) {
        cc_r c;
        EXPECT_HR(cc_run(&c, tab[i].src, "main", tab[i].t, 0), S_OK);
        if (c.code) {
            EXPECT_HR(cc_dis(&c, 0, NULL, txt, sizeof txt), S_OK);
            CHECKF(strstr(txt, tab[i].want) != NULL, "%s: %.80s", tab[i].t, txt);
        }
        cc_free(&c);
    }
}

TEST(d3dcompiler_compile_disasm_each_flag_accepted)
{
    static const UINT ok[] = { 0, D3D_DISASM_ENABLE_COLOR_CODE, D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING,
                               D3D_DISASM_DISABLE_DEBUG_INFO, D3D_DISASM_ENABLE_INSTRUCTION_OFFSET, D3D_DISASM_INSTRUCTION_ONLY,
                               D3D_DISASM_PRINT_HEX_LITERALS, 0xF7 };
    static char txt[65536];
    cc_r c;
    unsigned i;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    for (i = 0; i < sizeof ok / sizeof *ok && c.code; i++) {
        HRESULT hr = cc_dis(&c, ok[i], NULL, txt, sizeof txt);
        CHECKF(hr == S_OK, "flags 0x%x hr=0x%08lx", ok[i], (unsigned long)hr);
        CHECKF(txt[0] != 0, "flags 0x%x empty text", ok[i]);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_invalid_flags)
{
    WIN_ONLY();
    static const UINT bad[] = { 0x8, 0x100, 0x200, 0x10000, 0x80000000, 0xFFFFFFFF, 0x1F7 };
    ID3DBlob *d;
    cc_r c;
    unsigned i;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    for (i = 0; i < sizeof bad / sizeof *bad && c.code; i++) {
        d = (ID3DBlob *)(ULONG_PTR)0x10;
        HRESULT hr = CCF(D3DDisassemble)(cc_ptr(c.code), cc_size(c.code), bad[i], NULL, &d);
        CHECKF(hr == CC_INVALIDCALL, "flags 0x%x hr=0x%08lx", bad[i], (unsigned long)hr);
        CHECKF(d == NULL || d == (ID3DBlob *)(ULONG_PTR)0x10, "flags 0x%x blob %p", bad[i], d);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_null_and_oversize_source)
{
    WIN_ONLY();
    ID3DBlob *d = NULL;
    HRESULT hr;
    cc_r c;
    NEED_DIS();
    hr = CCF(D3DDisassemble)(NULL, 0, 0, NULL, &d);
    EXPECT_HR(hr, CC_INVALIDCALL);
    CHECK(d == NULL);
    hr = CCF(D3DDisassemble)(NULL, 100, 0, NULL, &d);
    EXPECT_HR(hr, CC_INVALIDCALL);
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        hr = CCF(D3DDisassemble)(cc_ptr(c.code), (SIZE_T)1 << 32, 0, NULL, &d);
        EXPECT_HR(hr, CC_INVALIDCALL);
        hr = CCF(D3DDisassemble)(cc_ptr(c.code), (SIZE_T)0x100000000ull + 4, 0, NULL, &d);
        EXPECT_HR(hr, CC_INVALIDCALL);
        CHECK(d == NULL);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_garbage_input)
{
    static const unsigned char junk[64] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    static const unsigned char zeros[4] = { 0 };
    ID3DBlob *d = NULL;
    HRESULT hr;
    NEED_DIS();
    hr = CCF(D3DDisassemble)(junk, sizeof junk, 0, NULL, &d);
    EXPECT_HR_FAILED(hr);
    CHECK(d == NULL);
    hr = CCF(D3DDisassemble)(zeros, sizeof zeros, 0, NULL, &d);
    EXPECT_HR_FAILED(hr);
    CHECK(d == NULL);
    hr = CCF(D3DDisassemble)(junk, 0, 0, NULL, &d);
    EXPECT_HR_FAILED(hr);
    t_note("garbage disassembly hr=0x%08lx", (unsigned long)hr);
}

TEST(d3dcompiler_compile_disasm_truncated_container)
{
    ID3DBlob *d = NULL;
    HRESULT hr;
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        hr = CCF(D3DDisassemble)(cc_ptr(c.code), 16, 0, NULL, &d);
        EXPECT_HR_FAILED(hr);
        CHECK(d == NULL);
        hr = CCF(D3DDisassemble)(cc_ptr(c.code), cc_size(c.code) / 2, 0, NULL, &d);
        t_note("half container: hr=0x%08lx blob=%p", (unsigned long)hr, d);
        SAFE_RELEASE(d);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_color_code_html)
{
    WIN_ONLY();
    static char txt[65536];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, D3D_DISASM_ENABLE_COLOR_CODE, NULL, txt, sizeof txt), S_OK);
        CHECK(strncmp(txt, "<pre>", 5) == 0);
        CHECK(strstr(txt, "</pre>") != NULL);
        EXPECT_HR(cc_dis(&c, 0, NULL, txt, sizeof txt), S_OK);
        CHECK(strstr(txt, "<pre>") == NULL);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_instruction_only)
{
    WIN_ONLY();
    static char full[65536], only[65536];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, 0, NULL, full, sizeof full), S_OK);
        EXPECT_HR(cc_dis(&c, D3D_DISASM_INSTRUCTION_ONLY, NULL, only, sizeof only), S_OK);
        CHECK(strlen(only) < strlen(full));
        CHECK(strstr(only, "Input signature") == NULL);
        CHECK(strstr(only, "ret") != NULL);
        CHECK(strstr(only, "mad") != NULL || strstr(only, "mul") != NULL);
        t_note("instruction-only output: %.120s", only);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_instruction_numbering)
{
    WIN_ONLY();
    static char num[65536], plain[65536];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, NULL, num, sizeof num), S_OK);
        EXPECT_HR(cc_dis(&c, 0, NULL, plain, sizeof plain), S_OK);
        CHECK(strlen(num) > strlen(plain));
        CHECK(strstr(num, "0: ") != NULL);
        CHECK(strstr(num, "1: ") != NULL);
        CHECK(strstr(plain, "   0: ") == NULL);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_instruction_offsets)
{
    WIN_ONLY();
    static char off[65536], plain[65536];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, D3D_DISASM_ENABLE_INSTRUCTION_OFFSET, NULL, off, sizeof off), S_OK);
        EXPECT_HR(cc_dis(&c, 0, NULL, plain, sizeof plain), S_OK);
        CHECK(strlen(off) > strlen(plain));
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_hex_literals)
{
    WIN_ONLY();
    static char hex[65536], plain[65536];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, "float4 main() : SV_Target { return float4(1.0, 0.5, 2.0, 0.0); }", "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, D3D_DISASM_PRINT_HEX_LITERALS, NULL, hex, sizeof hex), S_OK);
        EXPECT_HR(cc_dis(&c, 0, NULL, plain, sizeof plain), S_OK);
        CHECK(strstr(hex, "0x3f800000") != NULL || strstr(hex, "0x3F800000") != NULL);
        CHECK(strstr(plain, "0x3f800000") == NULL && strstr(plain, "0x3F800000") == NULL);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_comment_string)
{
    WIN_ONLY();
    static char txt[65536];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, 0, "unique-comment-marker-123", txt, sizeof txt), S_OK);
        CHECK(strstr(txt, "unique-comment-marker-123") != NULL);
        EXPECT_HR(cc_dis(&c, 0, NULL, txt, sizeof txt), S_OK);
        CHECK(strstr(txt, "unique-comment-marker-123") == NULL);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_debug_info_flag)
{
    static char with[131072], without[131072];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION), S_OK);
    if (c.code) {
        EXPECT_HR(cc_dis(&c, 0, NULL, with, sizeof with), S_OK);
        EXPECT_HR(cc_dis(&c, D3D_DISASM_DISABLE_DEBUG_INFO, NULL, without, sizeof without), S_OK);
        t_note("debug disassembly %lu chars, without debug info %lu chars", (unsigned long)strlen(with), (unsigned long)strlen(without));
        CHECK(strlen(without) <= strlen(with));
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_text_is_nul_terminated_blob)
{
    WIN_ONLY();
    ID3DBlob *d = NULL;
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(CCF(D3DDisassemble)(cc_ptr(c.code), cc_size(c.code), 0, NULL, &d), S_OK);
        if (d) {
            const char *p = (const char *)cc_ptr(d);
            SIZE_T n = cc_size(d);
            CHECK(n > 0);
            CHECK(memchr(p, 0, n) != NULL);
            t_note("disassembly blob size %lu, strlen %lu", (unsigned long)n, (unsigned long)strlen(p));
            EXPECT_EQ(d3dh_refcount(d), 1);
        }
        SAFE_RELEASE(d);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_does_not_modify_input)
{
    static char txt[65536];
    cc_r c;
    unsigned char *copy;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        copy = (unsigned char *)malloc(cc_size(c.code));
        memcpy(copy, cc_ptr(c.code), cc_size(c.code));
        cc_dis(&c, 0xF7, "c", txt, sizeof txt);
        CHECK(!memcmp(copy, cc_ptr(c.code), cc_size(c.code)));
        free(copy);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_deterministic)
{
    static char a[65536], b[65536];
    cc_r c;
    NEED_DIS();
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        cc_dis(&c, 0, NULL, a, sizeof a);
        cc_dis(&c, 0, NULL, b, sizeof b);
        CHECK(!strcmp(a, b));
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_disasm_stripped_shader)
{
    static char txt[65536];
    cc_r c;
    ID3DBlob *stripped = NULL;
    NEED_DIS(); REQUIRE(U(D3DStripShader));
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        HRESULT hr = U(D3DStripShader)(cc_ptr(c.code), cc_size(c.code), D3DCOMPILER_STRIP_REFLECTION_DATA, &stripped);
        EXPECT_HR(hr, S_OK);
        if (stripped) {
            ID3DBlob *d = NULL;
            hr = CCF(D3DDisassemble)(cc_ptr(stripped), cc_size(stripped), 0, NULL, &d);
            EXPECT_HR(hr, S_OK);
            cc_blobstr(d, txt, sizeof txt);
            CHECK(strstr(txt, "ps_5_0") != NULL);
            CHECK(strstr(txt, "ret") != NULL);
            SAFE_RELEASE(d);
        }
    }
    SAFE_RELEASE(stripped);
    cc_free(&c);
}

/* ---------------- D3DDisassembleRegion ---------------- */
TEST(d3dcompiler_compile_region_full_equals_disassemble)
{
    static char full[65536], reg[65536];
    ID3DBlob *d = NULL;
    SIZE_T finish = 0x1234;
    cc_r c;
    HRESULT hr;
    NEED_DIS(); NEED_NATIVE(D3DDisassembleRegion);
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        cc_dis(&c, 0, NULL, full, sizeof full);
        hr = CCN(D3DDisassembleRegion)(cc_ptr(c.code), cc_size(c.code), 0, NULL, 0, 0xFFFFFFFF, &finish, &d);
        EXPECT_HR(hr, S_OK);
        cc_blobstr(d, reg, sizeof reg);
        CHECK(!strcmp(full, reg));
        t_note("finish byte offset for full region: %lu", (unsigned long)finish);
        SAFE_RELEASE(d);
        hr = CCN(D3DDisassembleRegion)(cc_ptr(c.code), cc_size(c.code), 0, NULL, 0, 0xFFFFFFFF, NULL, &d);
        EXPECT_HR(hr, S_OK);
        cc_blobstr(d, reg, sizeof reg);
        CHECK(!strcmp(full, reg));
        SAFE_RELEASE(d);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_region_validates_like_disassemble)
{
    ID3DBlob *d = NULL;
    SIZE_T fin = 0;
    cc_r c;
    NEED_DIS(); NEED_NATIVE(D3DDisassembleRegion);
    EXPECT_HR(CCN(D3DDisassembleRegion)(NULL, 0, 0, NULL, 0, 1, &fin, &d), CC_INVALIDCALL);
    CHECK(d == NULL);
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(CCN(D3DDisassembleRegion)(cc_ptr(c.code), cc_size(c.code), 0x8, NULL, 0, 1, &fin, &d), CC_INVALIDCALL);
        EXPECT_HR(CCN(D3DDisassembleRegion)(cc_ptr(c.code), cc_size(c.code), 0x100, NULL, 0, 1, &fin, &d), CC_INVALIDCALL);
        EXPECT_HR(CCN(D3DDisassembleRegion)(cc_ptr(c.code), (SIZE_T)1 << 32, 0, NULL, 0, 1, &fin, &d), CC_INVALIDCALL);
        CHECK(d == NULL);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_region_single_instruction)
{
    static char one[65536], full[65536];
    ID3DBlob *d = NULL;
    SIZE_T fin = 0;
    cc_r c;
    HRESULT hr;
    NEED_DIS(); NEED_NATIVE(D3DDisassembleRegion);
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        cc_dis(&c, D3D_DISASM_INSTRUCTION_ONLY, NULL, full, sizeof full);
        hr = CCN(D3DDisassembleRegion)(cc_ptr(c.code), cc_size(c.code), D3D_DISASM_INSTRUCTION_ONLY, NULL, 0, 1, &fin, &d);
        t_note("region(0,1): hr=0x%08lx finish=%lu", (unsigned long)hr, (unsigned long)fin);
        EXPECT_HR(hr, S_OK);
        cc_blobstr(d, one, sizeof one);
        CHECK(strlen(one) <= strlen(full));
        t_note("region text: %.120s", one);
        SAFE_RELEASE(d);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_region_zero_instructions)
{
    ID3DBlob *d = NULL;
    SIZE_T fin = 0;
    cc_r c;
    HRESULT hr;
    NEED_DIS(); NEED_NATIVE(D3DDisassembleRegion);
    EXPECT_HR(cc_run(&c, cc_dis_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        hr = CCN(D3DDisassembleRegion)(cc_ptr(c.code), cc_size(c.code), 0, NULL, 0, 0, &fin, &d);
        t_note("region with zero instructions: hr=0x%08lx", (unsigned long)hr);
        SAFE_RELEASE(d);
    }
    cc_free(&c);
}

/* ---------------- D3DGetTraceInstructionOffsets ---------------- */
static const char *cc_trace_src =
    "cbuffer cb : register(b0) { float4 k; };\n"
    "Texture2D t : register(t0); SamplerState s : register(s0);\n"
    "float4 main(float2 uv : TEXCOORD0) : SV_Target {\n"
    "  float4 a = t.Sample(s, uv) * k;\n"
    "  a = a * a + k;\n"
    "  return sqrt(a) + a.wzyx;\n}\n";

TEST(d3dcompiler_compile_trace_invalid_flags)
{
    SIZE_T total = 0x77;
    static const UINT bad[] = { 0x2, 0x4, 0x10, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    cc_r c;
    unsigned i;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    for (i = 0; i < sizeof bad / sizeof *bad && c.code; i++) {
        HRESULT hr = CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), bad[i], 0, 4, NULL, &total);
        CHECKF(hr == E_INVALIDARG, "flags 0x%x hr=0x%08lx", bad[i], (unsigned long)hr);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_count_only)
{
    SIZE_T total0 = 0, total1 = 0;
    cc_r c;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 0, 0, NULL, &total0), S_OK);
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 1, 0, 0, NULL, &total1), S_OK);
        CHECK(total0 > 0);
        CHECK(total1 > total0);       /* flag 1 also counts declarations */
        CHECK(total0 < 64);
        t_note("executable instructions %lu, with declarations %lu", (unsigned long)total0, (unsigned long)total1);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_no_outputs_requested)
{
    cc_r c;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 0, 0, NULL, NULL), S_OK);
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_offsets_monotonic_and_aligned)
{
    SIZE_T total = 0, off[128];
    cc_r c;
    SIZE_T i;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        SIZE_T shex_size = 0;
        const unsigned char *shex;
        memset(off, 0xFF, sizeof off);
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 0, 128, off, &total), S_OK);
        CHECK(total > 0 && total <= 128);
        for (i = 0; i < total && i < 128; i++) {
            CHECKF((off[i] & 3) == 0, "offset %lu = %lu unaligned", (unsigned long)i, (unsigned long)off[i]);
            if (i) CHECKF(off[i] > off[i - 1], "offset %lu not increasing", (unsigned long)i);
        }
        if (total < 128) CHECK(off[total] == (SIZE_T)-1 || off[total] == 0xFFFFFFFFFFFFFFFFull);
        shex = cc_part(cc_ptr(c.code), cc_size(c.code), CC_FOURCC('S', 'H', 'E', 'X'), (DWORD *)&shex_size);
        CHECK(shex != NULL);
        if (shex && total) CHECK(off[total - 1] < (shex_size & 0xFFFFFFFF) + 16);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_window)
{
    SIZE_T total = 0, all[128], win[8];
    cc_r c;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        memset(all, 0, sizeof all);
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 0, 128, all, &total), S_OK);
        if (total >= 4) {
            SIZE_T t2 = 0;
            memset(win, 0xAA, sizeof win);
            EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 1, 2, win, &t2), S_OK);
            EXPECT_EQ(t2, total);
            EXPECT_EQ(win[0], all[1]);
            EXPECT_EQ(win[1], all[2]);
            CHECK(win[2] == 0xAAAAAAAAAAAAAAAAull);     /* nothing beyond the requested window is written */
        }
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_zero_count_window_writes_nothing)
{
    SIZE_T total = 0, win[4];
    cc_r c;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        memset(win, 0xAA, sizeof win);
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 0, 0, win, &total), S_OK);
        CHECK(win[0] == 0xAAAAAAAAAAAAAAAAull);
        CHECK(total > 0);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_window_past_end)
{
    SIZE_T total = 0, win[4];
    cc_r c;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        memset(win, 0xAA, sizeof win);
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 1000, 4, win, &total), S_OK);
        CHECK(win[0] == 0xAAAAAAAAAAAAAAAAull);
        CHECK(total > 0 && total < 1000);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_include_declarations_offsets_start_earlier)
{
    SIZE_T tot0 = 0, tot1 = 0, a[128], b[128];
    cc_r c;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 0, 128, a, &tot0), S_OK);
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 1, 0, 128, b, &tot1), S_OK);
        if (tot0 && tot1 > tot0) CHECK(b[0] < a[0]);
        if (tot1 && tot0) CHECK(b[tot1 - 1] == a[tot0 - 1]);   /* the last instruction (ret) is common */
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_matches_disassembly_instruction_count)
{
    static char txt[65536];
    SIZE_T total = 0;
    cc_r c;
    int lines = 0;
    const char *p;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 0, 0, NULL, &total), S_OK);
        cc_dis(&c, D3D_DISASM_INSTRUCTION_ONLY, NULL, txt, sizeof txt);
        for (p = txt; *p; ) {
            const char *e = strchr(p, '\n');
            size_t l = e ? (size_t)(e - p) : strlen(p);
            if (l > 0) {
                const char *q = p;
                while (q < p + l && (*q == ' ' || *q == '\r')) q++;
                if (q < p + l && strncmp(q, "ps_", 3) && strncmp(q, "dcl_", 4) && strncmp(q, "//", 2)) lines++;
            }
            if (!e) break;
            p = e + 1;
        }
        t_note("executable instructions %lu, instruction lines %d", (unsigned long)total, lines);
        if ((SIZE_T)lines != total) t_note("instruction line count differs from trace count");
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_vertex_shader)
{
    SIZE_T total = 0;
    cc_r c;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    EXPECT_HR(cc_run(&c, CC_VS_SRC, "main", "vs_5_0", 0), S_OK);
    if (c.code) {
        EXPECT_HR(CCN(D3DGetTraceInstructionOffsets)(cc_ptr(c.code), cc_size(c.code), 0, 0, 0, NULL, &total), S_OK);
        CHECK(total >= 2);
    }
    cc_free(&c);
}

TEST(d3dcompiler_compile_trace_non_shader_input)
{
    static const unsigned char junk[64] = { 9, 9, 9, 9 };
    SIZE_T total = 0;
    HRESULT hr;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets);
    hr = CCN(D3DGetTraceInstructionOffsets)(junk, sizeof junk, 0, 0, 0, NULL, &total);
    EXPECT_HR_FAILED(hr);
    t_note("trace offsets for junk: hr=0x%08lx", (unsigned long)hr);
}

TEST(d3dcompiler_compile_trace_shader_without_shex_part)
{
    SIZE_T total = 0;
    cc_r c;
    ID3DBlob *no_code = NULL;
    NEED_DIS(); NEED_NATIVE(D3DGetTraceInstructionOffsets); REQUIRE(U(D3DGetBlobPart));
    EXPECT_HR(cc_run(&c, cc_trace_src, "main", "ps_5_0", 0), S_OK);
    if (c.code) {
        /* the input signature container has no SHEX/SHDR part */
        HRESULT hr = U(D3DGetBlobPart)(cc_ptr(c.code), cc_size(c.code), D3D_BLOB_INPUT_AND_OUTPUT_SIGNATURE_BLOB, 0, &no_code);
        if (SUCCEEDED(hr) && no_code) {
            hr = CCN(D3DGetTraceInstructionOffsets)(cc_ptr(no_code), cc_size(no_code), 0, 0, 0, NULL, &total);
            t_note("signature-only container: hr=0x%08lx", (unsigned long)hr);
            EXPECT_HR_FAILED(hr);
        }
    }
    SAFE_RELEASE(no_code);
    cc_free(&c);
}

/* ---------------- D3DDisassemble10Effect ---------------- */
UDECL(HRESULT, D3D10CreateEffectFromMemory, (void *, SIZE_T, UINT, ID3D10Device *, void *, void **));

TEST(d3dcompiler_compile_disasm10effect_roundtrip)
{
    static const char *fx =
        "float4 vsmain(float4 p : POSITION) : SV_Position { return p; }\n"
        "float4 psmain() : SV_Target { return float4(1, 0, 0, 1); }\n"
        "technique10 T { pass P0 { SetVertexShader(CompileShader(vs_4_0, vsmain())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, psmain())); } }\n";
    D3D10CreateEffectFromMemory_pfn create;
    ID3D10Device *dev = NULL;
    D3D10_DRIVER_TYPE drv;
    cc_r c;
    void *effect = NULL;
    ID3DBlob *out = NULL;
    HRESULT hr;
    NEED_DIS(); NEED_NATIVE(D3DDisassemble10Effect);
    create = (D3D10CreateEffectFromMemory_pfn)t_proc_in("d3d10.dll", "D3D10CreateEffectFromMemory");
    REQUIRE(create);
    cc_compile_full(&c, fx, strlen(fx), "t.fx", NULL, NULL, NULL, "fx_4_0", 0, 0);
    if (FAILED(c.hr)) { t_skip("fx_4_0 compile unavailable: 0x%08lx", (unsigned long)c.hr); cc_free(&c); return; }
    if (FAILED(d3dh_make_device10(&dev, &drv, 0))) { t_skip("no D3D10 device"); cc_free(&c); return; }
    hr = create(cc_ptr(c.code) ? (void *)cc_ptr(c.code) : NULL, cc_size(c.code), 0, dev, NULL, &effect);
    if (FAILED(hr) || !effect) { t_skip("effect creation failed 0x%08lx", (unsigned long)hr); SAFE_RELEASE(dev); cc_free(&c); return; }
    hr = CCN(D3DDisassemble10Effect)(effect, 0, &out);
    EXPECT_HR(hr, S_OK);
    if (out) {
        char txt[32768];
        cc_blobstr(out, txt, sizeof txt);
        CHECK(strlen(txt) > 0);
        CHECK(strstr(txt, "vs_4_0") != NULL);
        CHECK(strstr(txt, "ps_4_0") != NULL);
        t_note("effect disassembly starts: %.100s", txt);
    }
    SAFE_RELEASE(out);
    IUnknown_Release((IUnknown *)effect);
    SAFE_RELEASE(dev);
    cc_free(&c);
}
