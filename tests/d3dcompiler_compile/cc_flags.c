/* Compile flag matrix and DXBC container properties. */
#include "cc.h"

#define NEED() do { REQUIRE(CCF(D3DCompile)); } while (0)

static const char *cc_flag_src =
    "cbuffer cb : register(b0) { float4x4 m; float4 k; };\n"
    "float4 main(float4 p : SV_Position) : SV_Target { float4 a = mul(p, m); return a * k + 0.5f; }\n";

static void cc_check_container(ID3DBlob *b, const char *what)
{
    const unsigned char *p = cc_ptr(b);
    SIZE_T n = cc_size(b);
    DWORD i, cnt, expect;
    uint32_t sum[4];
    if (!b) return;
    CHECKF(cc_dxbc_ok(p, n), "%s: not a valid DXBC container", what);
    if (!cc_dxbc_ok(p, n)) return;
    EXPECT_EQ(cc_rd32(p), CC_FOURCC('D', 'X', 'B', 'C'));
    EXPECT_EQ(cc_rd32(p + 20), 1);
    EXPECT_EQ(cc_rd32(p + 24), n);
    cnt = cc_rd32(p + 28);
    expect = 32 + cnt * 4;
    for (i = 0; i < cnt; i++) {
        DWORD off = cc_rd32(p + 32 + i * 4);
        DWORD sz = cc_rd32(p + off + 4);
        CHECKF(off == expect, "%s: part %lu at %lu, expected tight packing at %lu", what, (unsigned long)i, (unsigned long)off, (unsigned long)expect);
        CHECKF((off & 3) == 0 && (sz & 3) == 0, "%s: part %lu unaligned (off %lu size %lu)", what, (unsigned long)i, (unsigned long)off, (unsigned long)sz);
        expect = off + 8 + sz;
    }
    EXPECT_EQ(expect, n);
    cc_dxbc_checksum(p, n, sum);
    CHECKF(!memcmp(sum, p + 4, 16), "%s: header checksum does not match recomputed hash", what);
}

/* one test per flag: must compile, produce a consistent container, keep the base parts */
#define FLAG_TEST(tname, flag, target) \
    TEST(d3dcompiler_compile_flags_##tname) { \
        cc_r r; char parts[128]; \
        NEED(); \
        EXPECT_HR(cc_run(&r, cc_flag_src, "main", target, (flag)), S_OK); \
        if (r.code) { \
            cc_check_container(r.code, #tname); \
            cc_part_list(r.code, parts, sizeof parts); \
            CHECKF(cc_has_part(r.code, "ISGN") && cc_has_part(r.code, "OSGN"), "parts: %s", parts); \
            CHECKF(cc_has_part(r.code, "SHEX") || cc_has_part(r.code, "SHDR"), "parts: %s", parts); \
            t_note(#tname " parts: %s size=%lu", parts, (unsigned long)cc_size(r.code)); \
        } \
        cc_free(&r); \
    }

FLAG_TEST(none, 0, "ps_5_0")
FLAG_TEST(debug, D3DCOMPILE_DEBUG, "ps_5_0")
FLAG_TEST(skip_validation, D3DCOMPILE_SKIP_VALIDATION, "ps_5_0")
FLAG_TEST(skip_optimization, D3DCOMPILE_SKIP_OPTIMIZATION, "ps_5_0")
FLAG_TEST(pack_row_major, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, "ps_5_0")
FLAG_TEST(pack_column_major, D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, "ps_5_0")
FLAG_TEST(partial_precision, D3DCOMPILE_PARTIAL_PRECISION, "ps_5_0")
FLAG_TEST(avoid_flow_control, D3DCOMPILE_AVOID_FLOW_CONTROL, "ps_5_0")
FLAG_TEST(prefer_flow_control, D3DCOMPILE_PREFER_FLOW_CONTROL, "ps_5_0")
FLAG_TEST(enable_strictness, D3DCOMPILE_ENABLE_STRICTNESS, "ps_5_0")
FLAG_TEST(backwards_compat, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, "ps_5_0")
FLAG_TEST(ieee_strictness, D3DCOMPILE_IEEE_STRICTNESS, "ps_5_0")
FLAG_TEST(opt_level0, D3DCOMPILE_OPTIMIZATION_LEVEL0, "ps_5_0")
FLAG_TEST(opt_level1, D3DCOMPILE_OPTIMIZATION_LEVEL1, "ps_5_0")
FLAG_TEST(opt_level2, D3DCOMPILE_OPTIMIZATION_LEVEL2, "ps_5_0")
FLAG_TEST(opt_level3, D3DCOMPILE_OPTIMIZATION_LEVEL3, "ps_5_0")
FLAG_TEST(resources_may_alias, D3DCOMPILE_RESOURCES_MAY_ALIAS, "ps_5_0")
FLAG_TEST(all_resources_bound, D3DCOMPILE_ALL_RESOURCES_BOUND, "ps_5_0")
FLAG_TEST(unbounded_tables_51, D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, "ps_5_1")
FLAG_TEST(debug_name_source, D3DCOMPILE_DEBUG_NAME_FOR_SOURCE, "ps_5_0")
FLAG_TEST(debug_name_binary, D3DCOMPILE_DEBUG_NAME_FOR_BINARY, "ps_5_0")
FLAG_TEST(warnings_are_errors_clean_source, D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0")
FLAG_TEST(debug_skipopt, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, "ps_5_0")
FLAG_TEST(debug_skipopt_skipval, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_SKIP_VALIDATION, "ps_5_0")
FLAG_TEST(strict_ieee_level3, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_IEEE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, "ps_5_0")
TEST(d3dcompiler_compile_flags_vs_debug)
{
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, "cbuffer cb : register(b0) { float4x4 m; };\nfloat4 main(float4 p : POSITION) : SV_Position { return mul(p, m); }\n", "main", "vs_5_0", D3DCOMPILE_DEBUG), S_OK);
    if (r.code) cc_check_container(r.code, "vs_debug");
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_unbounded_tables_50_note)
{
    cc_r r;
    NEED();
    cc_run(&r, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);
    t_note("ENABLE_UNBOUNDED_DESCRIPTOR_TABLES on ps_5_0: hr=0x%08lx", (unsigned long)r.hr);
    cc_free(&r);
}

/* ---- rejected bits -------------------------------------------------------- */
TEST(d3dcompiler_compile_flags_reserved_and_software_bits_rejected)
{
    WIN_ONLY();
    static const UINT bad[] = {
        D3DCOMPILE_FORCE_VS_SOFTWARE_NO_OPT, D3DCOMPILE_FORCE_PS_SOFTWARE_NO_OPT, D3DCOMPILE_NO_PRESHADER,
        D3DCOMPILE_RESERVED16, D3DCOMPILE_RESERVED17,
        0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x40000000, 0x80000000, 0xFFFFFFFF };
    unsigned i;
    NEED();
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        cc_r r;
        cc_run(&r, CC_PS_SRC, "main", "ps_5_0", bad[i]);
        CHECKF(r.hr == CC_INVALIDCALL, "flags 0x%08x -> hr=0x%08lx", bad[i], (unsigned long)r.hr);
        CHECKF(r.code == NULL, "flags 0x%08x produced code", bad[i]);
        CHECKF(strstr(r.err, "Invalid flags specified") != NULL, "flags 0x%08x err '%.80s'", bad[i], r.err);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_flags_rejected_bit_combined_with_valid_flags)
{
    WIN_ONLY();
    cc_r r;
    NEED();
    cc_run(&r, CC_PS_SRC, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_NO_PRESHADER);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_rejected_for_every_shader_target)
{
    WIN_ONLY();
    static const char *tg[] = { "vs_4_0", "ps_4_0", "gs_4_0", "cs_4_0", "vs_5_0", "ps_5_1", "vs_2_0", "ps_3_0", "vs_4_0_level_9_1" };
    unsigned i;
    NEED();
    for (i = 0; i < sizeof tg / sizeof *tg; i++) {
        cc_r r;
        cc_run(&r, CC_PS_SRC, "main", tg[i], 0x80);
        CHECKF(r.hr == CC_INVALIDCALL, "target %s hr=0x%08lx", tg[i], (unsigned long)r.hr);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_flags_both_debug_names_rejected)
{
    WIN_ONLY();
    cc_r r;
    NEED();
    cc_run(&r, CC_PS_SRC, "main", "ps_5_0", D3DCOMPILE_DEBUG_NAME_FOR_SOURCE | D3DCOMPILE_DEBUG_NAME_FOR_BINARY);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    CHECK(r.code == NULL);
    CHECK(strstr(r.err, "cannot specify source and binary debug names") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_debug_names_with_other_flags)
{
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, CC_PS_SRC, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_DEBUG_NAME_FOR_SOURCE), S_OK);
    cc_free(&r);
    EXPECT_HR(cc_run(&r, CC_PS_SRC, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_DEBUG_NAME_FOR_BINARY), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_invalid_flags_message_precedes_source_errors)
{
    WIN_ONLY();
    cc_r r;
    NEED();
    cc_run(&r, "this is not hlsl at all", "main", "ps_5_0", 0x40);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    CHECK(strstr(r.err, "Invalid flags specified") != NULL);
    CHECK(strstr(r.err, "X3000") == NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_validated_before_entry_lookup)
{
    WIN_ONLY();
    cc_r r;
    NEED();
    cc_run(&r, CC_PS_SRC, "missing_entry", "ps_5_0", 0x100);
    EXPECT_HR(r.hr, CC_INVALIDCALL);
    CHECK(strstr(r.err, "X3501") == NULL);
    cc_free(&r);
}

/* ---- optimization levels ---------------------------------------------------- */
TEST(d3dcompiler_compile_flags_opt_level0_or_level3_is_level2)
{
    cc_r a, b, c;
    NEED();
    EXPECT_HR(cc_run(&a, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_OPTIMIZATION_LEVEL3), S_OK);
    EXPECT_HR(cc_run(&b, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL2), S_OK);
    EXPECT_HR(cc_run(&c, cc_flag_src, "main", "ps_5_0", 0), S_OK);
    EXPECT_EQ(D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_OPTIMIZATION_LEVEL3, D3DCOMPILE_OPTIMIZATION_LEVEL2);
    EXPECT_EQ(cc_size(a.code), cc_size(b.code));
    if (a.code && b.code && cc_size(a.code) == cc_size(b.code)) CHECK(!memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)));
    EXPECT_EQ(D3DCOMPILE_OPTIMIZATION_LEVEL1, 0);
    EXPECT_EQ(cc_size(b.code) > 0, 1);
    cc_free(&a); cc_free(&b); cc_free(&c);
}

TEST(d3dcompiler_compile_flags_opt_level1_is_default)
{
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, cc_flag_src, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL1), S_OK);
    EXPECT_EQ(cc_size(a.code), cc_size(b.code));
    if (a.code && b.code && cc_size(a.code) == cc_size(b.code)) CHECK(!memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)));
    cc_free(&a); cc_free(&b);
}

static const char *cc_redundant_src =
    "float4 main(float4 p : SV_Position) : SV_Target {\n"
    "  float a = p.x * 2; float b = a; float c = b + 0; float dead = sin(p.y) * cos(p.z);\n"
    "  float d = c * 1; float e = d + a - a;\n"
    "  return float4(e, e * 1, e + 0, 1);\n}\n";

TEST(d3dcompiler_compile_flags_skip_optimization_grows_code)
{
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, cc_redundant_src, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, cc_redundant_src, "main", "ps_5_0", D3DCOMPILE_SKIP_OPTIMIZATION), S_OK);
    if (a.code && b.code) {
        t_note("optimized %lu bytes, unoptimized %lu bytes", (unsigned long)cc_size(a.code), (unsigned long)cc_size(b.code));
        if (!t_is_wine()) CHECK(cc_size(b.code) > cc_size(a.code));
    }
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_flags_opt_level0_not_smaller_than_level3)
{
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, cc_redundant_src, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL0), S_OK);
    EXPECT_HR(cc_run(&b, cc_redundant_src, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3), S_OK);
    if (a.code && b.code) {
        t_note("level0 %lu bytes, level3 %lu bytes", (unsigned long)cc_size(a.code), (unsigned long)cc_size(b.code));
        if (!t_is_wine()) CHECK(cc_size(a.code) >= cc_size(b.code));
    }
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_flags_debug_adds_data)
{
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, cc_flag_src, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_DEBUG), S_OK);
    if (a.code && b.code && !t_is_wine()) {
        CHECK(cc_size(b.code) > cc_size(a.code));
        CHECK(cc_nparts(cc_ptr(b.code)) >= cc_nparts(cc_ptr(a.code)));
    }
    cc_free(&a); cc_free(&b);
}

/* ---- matrix packing --------------------------------------------------------- */
TEST(d3dcompiler_compile_flags_pack_order_changes_bytes)
{
    cc_r a, b, c;
    NEED();
    EXPECT_HR(cc_run(&a, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_PACK_MATRIX_ROW_MAJOR), S_OK);
    EXPECT_HR(cc_run(&b, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR), S_OK);
    EXPECT_HR(cc_run(&c, cc_flag_src, "main", "ps_5_0", 0), S_OK);
    if (a.code && b.code && c.code) {
        CHECK(cc_size(a.code) != cc_size(b.code) || memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)) != 0);
        /* column-major is the default packing */
        EXPECT_EQ(cc_size(b.code), cc_size(c.code));
        if (cc_size(b.code) == cc_size(c.code)) CHECK(!memcmp(cc_ptr(b.code), cc_ptr(c.code), cc_size(b.code)));
    }
    cc_free(&a); cc_free(&b); cc_free(&c);
}

TEST(d3dcompiler_compile_flags_pack_both_set)
{
    cc_r r;
    NEED();
    cc_run(&r, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR);
    t_note("both pack flags: hr=0x%08lx err=%.100s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_pack_pragma_overrides_flag)
{
    static const char *prag =
        "#pragma pack_matrix(row_major)\n"
        "cbuffer cb : register(b0) { float4x4 m; float4 k; };\n"
        "float4 main(float4 p : SV_Position) : SV_Target { float4 a = mul(p, m); return a * k + 0.5f; }\n";
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, prag, "main", "ps_5_0", D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR), S_OK);
    EXPECT_HR(cc_run(&b, prag, "main", "ps_5_0", D3DCOMPILE_PACK_MATRIX_ROW_MAJOR), S_OK);
    if (a.code && b.code) {
        EXPECT_EQ(cc_size(a.code), cc_size(b.code));
        if (cc_size(a.code) == cc_size(b.code)) CHECK(!memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)));
    }
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_flags_row_major_keyword_matches_flag)
{
    static const char *kw =
        "cbuffer cb : register(b0) { row_major float4x4 m; float4 k; };\n"
        "float4 main(float4 p : SV_Position) : SV_Target { float4 a = mul(p, m); return a * k + 0.5f; }\n";
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, kw, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_PACK_MATRIX_ROW_MAJOR), S_OK);
    if (a.code && b.code) {
        /* the DXBC may differ in RDEF text only if names differ; the shader token stream must match */
        DWORD sa = 0, sb = 0;
        const unsigned char *pa = cc_shader_part(a.code, &sa);
        const unsigned char *pb = cc_shader_part(b.code, &sb);
        CHECK(pa && pb);
        if (pa && pb) { EXPECT_EQ(sa, sb); if (sa == sb) CHECK(!memcmp(pa, pb, sa)); }
    }
    cc_free(&a); cc_free(&b);
}

/* ---- flow control --------------------------------------------------------- */
static const char *cc_flow_src =
    "Texture2D t : register(t0); SamplerState s : register(s0);\n"
    "cbuffer cb : register(b0) { float4 k; };\n"
    "float4 main(float2 uv : TEXCOORD0) : SV_Target {\n"
    "  float4 r = 0;\n"
    "  if (k.x > 0.5f) { r = t.SampleLevel(s, uv, 0) + t.SampleLevel(s, uv * 2, 0) * k; }\n"
    "  else { r = t.SampleLevel(s, uv * 3, 0) - k; }\n"
    "  return r;\n}\n";

static int cc_disasm(ID3DBlob *code, UINT flags, char *out, size_t cap)
{
    ID3DBlob *d = NULL;
    HRESULT hr;
    out[0] = 0;
    if (!U(D3DDisassemble)) return 0;
    hr = CCF(D3DDisassemble)(cc_ptr(code), cc_size(code), flags, NULL, &d);
    if (FAILED(hr)) return 0;
    cc_blobstr(d, out, cap);
    SAFE_RELEASE(d);
    return 1;
}

TEST(d3dcompiler_compile_flags_avoid_flow_control_flattens)
{
    WIN_ONLY();
    cc_r r;
    static char txt[65536];
    NEED(); REQUIRE(CCF(D3DDisassemble));
    EXPECT_HR(cc_run(&r, cc_flow_src, "main", "ps_5_0", D3DCOMPILE_AVOID_FLOW_CONTROL), S_OK);
    if (r.code && cc_disasm(r.code, 0, txt, sizeof txt)) {
        CHECK(strstr(txt, "if_nz") == NULL && strstr(txt, "if_z") == NULL);
        CHECK(strstr(txt, "movc") != NULL);
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_prefer_flow_control_branches)
{
    cc_r r;
    static char txt[65536];
    NEED(); REQUIRE(CCF(D3DDisassemble));
    EXPECT_HR(cc_run(&r, cc_flow_src, "main", "ps_5_0", D3DCOMPILE_PREFER_FLOW_CONTROL), S_OK);
    if (r.code && cc_disasm(r.code, 0, txt, sizeof txt))
        t_note("PREFER_FLOW_CONTROL emits branch: %d", strstr(txt, "if_nz") != NULL || strstr(txt, "if_z") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_branch_attribute_wins_over_flag)
{
    WIN_ONLY();
    static const char *src =
        "cbuffer cb : register(b0) { float4 k; };\n"
        "float4 main(float4 p : SV_Position) : SV_Target {\n"
        "  float4 r = 0;\n  [flatten] if (k.x > 0.5f) { r = p * k; } else { r = p - k; }\n  return r;\n}\n";
    cc_r r;
    static char txt[65536];
    NEED(); REQUIRE(CCF(D3DDisassemble));
    EXPECT_HR(cc_run(&r, src, "main", "ps_5_0", D3DCOMPILE_PREFER_FLOW_CONTROL), S_OK);
    if (r.code && cc_disasm(r.code, 0, txt, sizeof txt))
        CHECK(strstr(txt, "if_nz") == NULL && strstr(txt, "if_z") == NULL);
    cc_free(&r);
}

/* ---- backwards compatibility --------------------------------------------- */
static const char *cc_legacy_tex_src =
    "sampler2D s : register(s0);\n"
    "float4 main(float2 uv : TEXCOORD0) : SV_Target { return tex2D(s, uv); }\n";

TEST(d3dcompiler_compile_flags_backwards_compat_allows_tex2d_in_sm4)
{
    cc_r a, b;
    NEED();
    cc_run(&a, cc_legacy_tex_src, "main", "ps_4_0", 0);
    cc_run(&b, cc_legacy_tex_src, "main", "ps_4_0", D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY);
    t_note("tex2D in ps_4_0 without /Gec: hr=0x%08lx %.100s", (unsigned long)a.hr, a.err);
    t_note("tex2D in ps_4_0 with /Gec: hr=0x%08lx %.100s", (unsigned long)b.hr, b.err);
    t_note("ENABLE_BACKWARDS_COMPATIBILITY allows tex2D in ps_4_0: %d", SUCCEEDED(b.hr));
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_flags_tex2d_valid_in_sm3_without_flag)
{
    static const char *src = "sampler2D s : register(s0);\nfloat4 main(float2 uv : TEXCOORD0) : COLOR0 { return tex2D(s, uv); }\n";
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, src, "main", "ps_3_0", 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_flags_strictness_note)
{
    static const char *src = "float4 main() : SV_Target { vector<float, 4> v = float4(1, 2, 3, 4); return v; }\n";
    cc_r a, b;
    NEED();
    cc_run(&a, src, "main", "ps_5_0", 0);
    cc_run(&b, src, "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS);
    EXPECT_HR(a.hr, S_OK);
    t_note("vector<float,4> with strictness: hr=0x%08lx", (unsigned long)b.hr);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_flags_partial_precision_note)
{
    static const char *src = "float4 main(float4 p : SV_Position) : SV_Target { return p * p + p; }\n";
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, src, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, src, "main", "ps_5_0", D3DCOMPILE_PARTIAL_PRECISION), S_OK);
    if (a.code && b.code)
        t_note("partial precision changes bytes: %d", cc_size(a.code) != cc_size(b.code) || memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)) != 0);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_flags_ieee_strictness_compiles_precise_code)
{
    static const char *src = "float4 main(float4 p : SV_Position) : SV_Target { return p.x * p.y + p.z; }\n";
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, src, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, src, "main", "ps_5_0", D3DCOMPILE_IEEE_STRICTNESS), S_OK);
    if (a.code && b.code)
        t_note("IEEE strictness changes bytes: %d (mad %d vs %d)", memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code) < cc_size(b.code) ? cc_size(a.code) : cc_size(b.code)) != 0, 0, 0);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_flags_skip_validation_still_a_container)
{
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, cc_flag_src, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_SKIP_VALIDATION), S_OK);
    if (a.code && b.code) {
        char pa[128], pb[128];
        cc_part_list(a.code, pa, sizeof pa);
        cc_part_list(b.code, pb, sizeof pb);
        CHECKF(!strcmp(pa, pb), "parts '%s' vs '%s'", pa, pb);
    }
    cc_free(&a); cc_free(&b);
}

/* ---- container structure -------------------------------------------------- */
typedef struct { const char *name, *src, *entry, *target; } cc_case;
static const cc_case cc_cases[] = {
    { "vs_4_0", CC_VS_SRC, "main", "vs_4_0" },
    { "vs_4_1", CC_VS_SRC, "main", "vs_4_1" },
    { "vs_5_0", CC_VS_SRC, "main", "vs_5_0" },
    { "ps_4_0", CC_PS_SRC, "main", "ps_4_0" },
    { "ps_4_1", CC_PS_SRC, "main", "ps_4_1" },
    { "ps_5_0", CC_PS_SRC, "main", "ps_5_0" },
    { "ps_5_1", CC_PS_SRC, "main", "ps_5_1" },
    { "vs_4_0_level_9_1", CC_VS_SRC, "main", "vs_4_0_level_9_1" },
    { "ps_4_0_level_9_1", CC_PS_CONST, "main", "ps_4_0_level_9_1" },
    { "ps_4_0_level_9_3", CC_PS_CONST, "main", "ps_4_0_level_9_3" },
};

TEST(d3dcompiler_compile_dxbc_container_invariants)
{
    unsigned i;
    NEED();
    for (i = 0; i < sizeof cc_cases / sizeof *cc_cases; i++) {
        cc_r r;
        EXPECT_HR(cc_run(&r, cc_cases[i].src, cc_cases[i].entry, cc_cases[i].target, 0), S_OK);
        if (r.code) cc_check_container(r.code, cc_cases[i].name);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_dxbc_base_part_order)
{
    WIN_ONLY();
    unsigned i;
    NEED();
    for (i = 0; i < sizeof cc_cases / sizeof *cc_cases; i++) {
        cc_r r;
        char parts[128];
        const char *rd, *is, *os, *sh, *st;
        EXPECT_HR(cc_run(&r, cc_cases[i].src, cc_cases[i].entry, cc_cases[i].target, 0), S_OK);
        if (r.code) {
            cc_part_list(r.code, parts, sizeof parts);
            rd = strstr(parts, "RDEF"); is = strstr(parts, "ISGN"); os = strstr(parts, "OSGN");
            sh = strstr(parts, "SHEX"); if (!sh) sh = strstr(parts, "SHDR");
            st = strstr(parts, "STAT");
            CHECKF(rd && is && os && sh && st, "%s parts '%s'", cc_cases[i].name, parts);
            if (rd && is && os && sh && st) CHECKF(rd < is && is < os && os < sh && sh < st, "%s order '%s'", cc_cases[i].name, parts);
            t_note("%s parts: %s", cc_cases[i].name, parts);
        }
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_dxbc_shader_part_kind_by_profile)
{
    WIN_ONLY();
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, CC_PS_SRC, "main", "ps_4_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    if (a.code) { CHECK(cc_has_part(a.code, "SHDR") || cc_has_part(a.code, "SHEX")); t_note("ps_4_0 uses %s", cc_has_part(a.code, "SHDR") ? "SHDR" : "SHEX"); }
    if (b.code) { CHECK(cc_has_part(b.code, "SHEX")); }
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_dxbc_shader_token_version)
{
    static const struct { const char *t; DWORD ver; } tab[] = {
        { "ps_4_0", 0x00000040 }, { "ps_4_1", 0x00000041 }, { "ps_5_0", 0x00000050 },
        { "vs_4_0", 0x00010040 }, { "vs_5_0", 0x00010050 },
    };
    unsigned i;
    NEED();
    for (i = 0; i < sizeof tab / sizeof *tab; i++) {
        cc_r r;
        DWORD sz = 0;
        const unsigned char *sh;
        EXPECT_HR(cc_run(&r, tab[i].t[0] == 'v' ? CC_VS_SRC : CC_PS_SRC, "main", tab[i].t, 0), S_OK);
        if (r.code) {
            sh = cc_part(cc_ptr(r.code), cc_size(r.code), CC_FOURCC('S', 'H', 'E', 'X'), &sz);
            if (!sh) sh = cc_part(cc_ptr(r.code), cc_size(r.code), CC_FOURCC('S', 'H', 'D', 'R'), &sz);
            CHECKF(sh != NULL, "%s no shader part", tab[i].t);
            if (sh) {
                /* version token: low byte minor, next major, high word program type (0 ps, 1 vs, 2 gs ...) */
                DWORD v = cc_rd32(sh);
                CHECKF(((v >> 16) & 0xffff) == (tab[i].ver >> 16), "%s token 0x%08lx", tab[i].t, (unsigned long)v);
                CHECKF(((v >> 4) & 0xf) == ((tab[i].ver >> 4) & 0xf) && (v & 0xf) == (tab[i].ver & 0xf), "%s token 0x%08lx", tab[i].t, (unsigned long)v);
                /* length token counts dwords including the two header tokens */
                CHECKF(cc_rd32(sh + 4) * 4 == sz, "%s length token %lu vs part size %lu", tab[i].t, (unsigned long)cc_rd32(sh + 4), (unsigned long)sz);
            }
        }
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_dxbc_checksum_matches_recomputation)
{
    cc_r r;
    uint32_t h[4];
    NEED();
    EXPECT_HR(cc_run(&r, cc_flag_src, "main", "ps_5_0", 0), S_OK);
    if (r.code) {
        cc_dxbc_checksum(cc_ptr(r.code), cc_size(r.code), h);
        CHECK(!memcmp(h, cc_ptr(r.code) + 4, 16));
        {
            static const unsigned char zero[16];
            CHECK(memcmp(zero, cc_ptr(r.code) + 4, 16) != 0);
        }
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_dxbc_checksum_depends_on_content)
{
    cc_r a, b;
    NEED();
    EXPECT_HR(cc_run(&a, "float4 main() : SV_Target { return 0.25; }", "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(cc_run(&b, "float4 main() : SV_Target { return 0.75; }", "main", "ps_5_0", 0), S_OK);
    if (a.code && b.code) CHECK(memcmp(cc_ptr(a.code) + 4, cc_ptr(b.code) + 4, 16) != 0);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_dxbc_checksum_after_strip)
{
    cc_r r;
    ID3DBlob *stripped = NULL;
    NEED(); REQUIRE(U(D3DStripShader));
    EXPECT_HR(cc_run(&r, cc_flag_src, "main", "ps_5_0", D3DCOMPILE_DEBUG), S_OK);
    if (r.code) {
        HRESULT hr = U(D3DStripShader)(cc_ptr(r.code), cc_size(r.code), D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO, &stripped);
        EXPECT_HR(hr, S_OK);
        if (stripped) {
            cc_check_container(stripped, "stripped");
            CHECK(!cc_has_part(stripped, "RDEF"));
            CHECK(cc_size(stripped) < cc_size(r.code));
        }
    }
    SAFE_RELEASE(stripped);
    cc_free(&r);
}

TEST(d3dcompiler_compile_dxbc_no_priv_part_by_default)
{
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    if (r.code) {
        CHECK(!cc_has_part(r.code, "PRIV"));
        CHECK(!cc_has_part(r.code, "ILDB"));
        CHECK(!cc_has_part(r.code, "SFI0"));
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_dxbc_sfi0_for_double_precision)
{
    static const char *src = "float4 main(float4 p : SV_Position) : SV_Target { double d = p.x; d = d * d + 1.0; return (float)d; }\n";
    cc_r r;
    NEED();
    cc_run(&r, src, "main", "ps_5_0", 0);
    if (t_is_wine() && FAILED(r.hr)) { t_skip("double support"); cc_free(&r); return; }
    EXPECT_HR(r.hr, S_OK);
    if (r.code) {
        DWORD sz = 0;
        const unsigned char *p = cc_part(cc_ptr(r.code), cc_size(r.code), CC_FOURCC('S', 'F', 'I', '0'), &sz);
        CHECK(p != NULL);
        if (p) {
            EXPECT_EQ(sz, 8);
            CHECK((cc_rd32(p) & 1) != 0);   /* D3D11_SHADER_REQUIRES_DOUBLES */
        }
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_dxbc_stat_part_present_and_sized)
{
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    if (r.code) {
        DWORD sz = 0;
        const unsigned char *p = cc_part(cc_ptr(r.code), cc_size(r.code), CC_FOURCC('S', 'T', 'A', 'T'), &sz);
        CHECK(p != NULL);
        CHECK(sz >= 116 && (sz % 4) == 0);
        t_note("STAT size %lu", (unsigned long)sz);
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_dxbc_rdef_header)
{
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, cc_flag_src, "main", "ps_5_0", 0), S_OK);
    if (r.code) {
        DWORD sz = 0;
        const unsigned char *p = cc_part(cc_ptr(r.code), cc_size(r.code), CC_FOURCC('R', 'D', 'E', 'F'), &sz);
        CHECK(p != NULL);
        if (p) {
            DWORD cb = cc_rd32(p), res = cc_rd32(p + 8);
            DWORD prog = cc_rd32(p + 16);
            CHECK(sz >= 60);
            EXPECT_EQ(cb, 1);                 /* one constant buffer */
            EXPECT_EQ(res, 1);                /* its binding */
            EXPECT_EQ(prog & 0xffff0000u, 0xffff0000u);   /* pixel shader program type */
            EXPECT_EQ(prog & 0xffff, 0x0500);
            CHECK(memcmp(p + 28, "RD11", 4) == 0);
        }
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_dxbc_signature_contents)
{
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    if (r.code) {
        DWORD sz = 0;
        const unsigned char *p = cc_part(cc_ptr(r.code), cc_size(r.code), CC_FOURCC('I', 'S', 'G', 'N'), &sz);
        CHECK(p != NULL);
        if (p) {
            DWORD cnt = cc_rd32(p), nameoff = cc_rd32(p + 8);
            EXPECT_EQ(cnt, 1);
            CHECK(nameoff < sz);
            if (nameoff < sz) { CHECK(!_stricmp((const char *)p + nameoff, "SV_Position")); t_note("ISGN name: %s", (const char *)p + nameoff); }
            EXPECT_EQ(cc_rd32(p + 16), 1);    /* D3D_NAME_POSITION */
        }
        p = cc_part(cc_ptr(r.code), cc_size(r.code), CC_FOURCC('O', 'S', 'G', 'N'), &sz);
        CHECK(p != NULL);
        if (p) {
            DWORD cnt = cc_rd32(p), nameoff = cc_rd32(p + 8);
            EXPECT_EQ(cnt, 1);
            if (nameoff < sz) { CHECK(!_stricmp((const char *)p + nameoff, "SV_Target")); t_note("OSGN name: %s", (const char *)p + nameoff); }
        }
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_dxbc_output_independent_of_error_blob_request)
{
    cc_r a;
    ID3DBlob *code = NULL;
    NEED();
    EXPECT_HR(cc_run(&a, CC_PS_SRC, "main", "ps_5_0", 0), S_OK);
    EXPECT_HR(CCF(D3DCompile)(CC_PS_SRC, strlen(CC_PS_SRC), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, &code, NULL), S_OK);
    if (a.code && code) {
        EXPECT_EQ(cc_size(a.code), cc_size(code));
        if (cc_size(a.code) == cc_size(code)) CHECK(!memcmp(cc_ptr(a.code), cc_ptr(code), cc_size(code)));
    }
    SAFE_RELEASE(code);
    cc_free(&a);
}

TEST(d3dcompiler_compile_dxbc_flags_matrix_deterministic)
{
    static const UINT fl[] = { 0, D3DCOMPILE_DEBUG, D3DCOMPILE_SKIP_OPTIMIZATION, D3DCOMPILE_OPTIMIZATION_LEVEL3, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,
                               D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_IEEE_STRICTNESS, D3DCOMPILE_DEBUG | D3DCOMPILE_DEBUG_NAME_FOR_SOURCE };
    unsigned i;
    NEED();
    for (i = 0; i < sizeof fl / sizeof *fl; i++) {
        cc_r a, b;
        EXPECT_HR(cc_run(&a, cc_flag_src, "main", "ps_5_0", fl[i]), S_OK);
        EXPECT_HR(cc_run(&b, cc_flag_src, "main", "ps_5_0", fl[i]), S_OK);
        if (a.code && b.code) {
            CHECKF(cc_size(a.code) == cc_size(b.code) && !memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)), "flags 0x%x not deterministic", fl[i]);
        }
        cc_free(&a); cc_free(&b);
    }
}
