/* Target profile matrix for D3DCompile. */
#include "cc.h"

#define NEED() do { REQUIRE(CCF(D3DCompile)); } while (0)

enum { K_VS4, K_PS4, K_GS, K_HS, K_DS, K_CS, K_VS3, K_PS3, K_LIB, K_FX2, K_FX4, K_FX5, K_TX };

static const char *cc_gs_src =
    "struct GO { float4 p : SV_Position; };\n"
    "[maxvertexcount(3)] void main(triangle float4 v[3] : SV_Position, inout TriangleStream<GO> s) {\n"
    "  for (int i = 0; i < 3; i++) { GO o; o.p = v[i]; s.Append(o); }\n}\n";

static const char *cc_hs_src =
    "struct VO { float4 p : POSITION; };\nstruct CP { float4 p : POSITION; };\n"
    "struct PC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };\n"
    "PC pcf(InputPatch<VO, 3> ip) { PC o; o.e[0] = 1; o.e[1] = 1; o.e[2] = 1; o.i = 1; return o; }\n"
    "[domain(\"tri\")][partitioning(\"integer\")][outputtopology(\"triangle_cw\")][outputcontrolpoints(3)][patchconstantfunc(\"pcf\")]\n"
    "CP main(InputPatch<VO, 3> ip, uint i : SV_OutputControlPointID) { CP o; o.p = ip[i].p; return o; }\n";

static const char *cc_ds_src =
    "struct CP { float4 p : POSITION; };\n"
    "struct PC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };\n"
    "struct DO { float4 p : SV_Position; };\n"
    "[domain(\"tri\")] DO main(PC pc, float3 loc : SV_DomainLocation, const OutputPatch<CP, 3> patch) {\n"
    "  DO o; o.p = patch[0].p * loc.x + patch[1].p * loc.y + patch[2].p * loc.z; return o; }\n";

static const char *cc_cs_src =
    "RWByteAddressBuffer b : register(u0);\n"
    "[numthreads(4, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { b.Store(id.x * 4, id.x); }\n";

static const char *cc_lib_src = "export float4 scale(float4 a, float k) { return a * k; }\n";

static const char *cc_fx2_src =
    "float4 psmain() : COLOR0 { return float4(1, 0, 0, 1); }\n"
    "technique T { pass P0 { PixelShader = compile ps_2_0 psmain(); } }\n";

static const char *cc_fx4_src =
    "float4 vsmain(float4 p : POSITION) : SV_Position { return p; }\n"
    "float4 psmain() : SV_Target { return float4(1, 0, 0, 1); }\n"
    "technique10 T { pass P0 { SetVertexShader(CompileShader(vs_4_0, vsmain())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, psmain())); } }\n";

static const char *cc_fx5_src =
    "float4 vsmain(float4 p : POSITION) : SV_Position { return p; }\n"
    "float4 psmain() : SV_Target { return float4(1, 0, 0, 1); }\n"
    "technique11 T { pass P0 { SetVertexShader(CompileShader(vs_5_0, vsmain())); SetPixelShader(CompileShader(ps_5_0, psmain())); } }\n";

static const char *cc_tx_src = "float4 tex(float2 pos : POSITION, float2 psize : PSIZE) : COLOR { return float4(pos, 0, 1); }\n";

static const char *cc_kind_src(int k, const char **entry)
{
    *entry = "main";
    switch (k) {
    case K_VS4: return CC_VS_SRC;
    case K_PS4: return CC_PS_SRC;
    case K_GS:  return cc_gs_src;
    case K_HS:  return cc_hs_src;
    case K_DS:  return cc_ds_src;
    case K_CS:  return cc_cs_src;
    case K_VS3: return CC_VS_LEGACY;
    case K_PS3: return CC_PS_LEGACY;
    case K_LIB: *entry = NULL; return cc_lib_src;
    case K_FX2: *entry = NULL; return cc_fx2_src;
    case K_FX4: *entry = NULL; return cc_fx4_src;
    case K_FX5: *entry = NULL; return cc_fx5_src;
    case K_TX:  *entry = "tex"; return cc_tx_src;
    }
    return "";
}

typedef struct { const char *target; int kind; } cc_tgt;

static void cc_target_table_run(const cc_tgt *tab, unsigned n, int hard)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        const char *entry;
        const char *src = cc_kind_src(tab[i].kind, &entry);
        cc_r r;
        cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, entry, tab[i].target, 0, 0);
        if (hard) {
            CHECKF(r.hr == S_OK, "target %s hr=0x%08lx err=%.150s", tab[i].target, (unsigned long)r.hr, r.err);
            CHECKF(r.code != NULL && cc_size(r.code) > 0, "target %s: no code", tab[i].target);
        } else {
            t_note("target %s hr=0x%08lx size=%lu", tab[i].target, (unsigned long)r.hr, (unsigned long)cc_size(r.code));
        }
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_targets_vs_sm4_sm5)
{
    static const cc_tgt t[] = { { "vs_4_0", K_VS4 }, { "vs_4_1", K_VS4 }, { "vs_5_0", K_VS4 }, { "vs_5_1", K_VS4 } };
    NEED();
    cc_target_table_run(t, 4, 1);
}

TEST(d3dcompiler_compile_targets_ps_sm4_sm5)
{
    static const cc_tgt t[] = { { "ps_4_0", K_PS4 }, { "ps_4_1", K_PS4 }, { "ps_5_0", K_PS4 }, { "ps_5_1", K_PS4 } };
    NEED();
    cc_target_table_run(t, 4, 1);
}

TEST(d3dcompiler_compile_targets_gs)
{
    static const cc_tgt t[] = { { "gs_4_0", K_GS }, { "gs_4_1", K_GS }, { "gs_5_0", K_GS }, { "gs_5_1", K_GS } };
    NEED();
    if (t_is_wine()) { t_skip("geometry shader HLSL front end unsupported by the emulator compiler"); return; }
    cc_target_table_run(t, 4, 1);
}

TEST(d3dcompiler_compile_targets_hs)
{
    static const cc_tgt t[] = { { "hs_5_0", K_HS }, { "hs_5_1", K_HS } };
    NEED();
    if (t_is_wine()) { t_skip("hull shader HLSL front end unsupported by the emulator compiler"); return; }
    cc_target_table_run(t, 2, 1);
}

TEST(d3dcompiler_compile_targets_ds)
{
    static const cc_tgt t[] = { { "ds_5_0", K_DS }, { "ds_5_1", K_DS } };
    NEED();
    if (t_is_wine()) { t_skip("domain shader HLSL front end unsupported by the emulator compiler"); return; }
    cc_target_table_run(t, 2, 1);
}

TEST(d3dcompiler_compile_targets_cs)
{
    static const cc_tgt t[] = { { "cs_4_0", K_CS }, { "cs_4_1", K_CS }, { "cs_5_0", K_CS }, { "cs_5_1", K_CS } };
    NEED();
    cc_target_table_run(t, 4, 1);
}

TEST(d3dcompiler_compile_targets_sm3_sm2)
{
    static const cc_tgt t[] = {
        { "vs_2_0", K_VS3 }, { "vs_2_a", K_VS3 }, { "vs_3_0", K_VS3 },
        { "ps_2_0", K_PS3 }, { "ps_2_a", K_PS3 }, { "ps_2_b", K_PS3 }, { "ps_3_0", K_PS3 } };
    NEED();
    cc_target_table_run(t, 7, 1);
}

TEST(d3dcompiler_compile_targets_software_profiles_note)
{
    static const cc_tgt t[] = { { "vs_2_sw", K_VS3 }, { "vs_3_sw", K_VS3 }, { "ps_2_sw", K_PS3 }, { "ps_3_sw", K_PS3 },
                                { "vs_1_1", K_VS3 }, { "ps_1_1", K_PS3 }, { "ps_1_4", K_PS3 } };
    NEED();
    cc_target_table_run(t, 7, 0);
}

TEST(d3dcompiler_compile_targets_level9)
{
    static const cc_tgt t[] = { { "vs_4_0_level_9_1", K_VS4 }, { "vs_4_0_level_9_3", K_VS4 },
                                { "ps_4_0_level_9_1", K_PS4 }, { "ps_4_0_level_9_3", K_PS4 } };
    NEED();
    cc_target_table_run(t, 4, 1);
}

TEST(d3dcompiler_compile_targets_level9_0_note)
{
    static const cc_tgt t[] = { { "vs_4_0_level_9_0", K_VS4 }, { "ps_4_0_level_9_0", K_PS4 } };
    NEED();
    cc_target_table_run(t, 2, 0);
}

TEST(d3dcompiler_compile_targets_level9_has_aon9_part)
{
    static const char *tg[] = { "vs_4_0_level_9_1", "vs_4_0_level_9_3", "ps_4_0_level_9_1", "ps_4_0_level_9_3" };
    unsigned i;
    NEED();
    if (t_is_wine()) { t_skip("level 9 profiles"); return; }
    for (i = 0; i < 4; i++) {
        cc_r r;
        EXPECT_HR(cc_run(&r, tg[i][0] == 'v' ? CC_VS_SRC : CC_PS_SRC, "main", tg[i], 0), S_OK);
        if (r.code) {
            char parts[128];
            cc_part_list(r.code, parts, sizeof parts);
            CHECKF(cc_has_part(r.code, "Aon9"), "%s parts: %s", tg[i], parts);
            CHECK(cc_dxbc_ok(cc_ptr(r.code), cc_size(r.code)));
        }
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_targets_lib)
{
    static const cc_tgt t[] = { { "lib_5_0", K_LIB } };
    static const cc_tgt n[] = { { "lib_4_0", K_LIB }, { "lib_4_1", K_LIB } };
    NEED();
    if (t_is_wine()) { t_skip("library profiles"); return; }
    cc_target_table_run(t, 1, 1);
    cc_target_table_run(n, 2, 0);
}

TEST(d3dcompiler_compile_targets_effects)
{
    static const cc_tgt t[] = { { "fx_2_0", K_FX2 }, { "fx_4_0", K_FX4 }, { "fx_4_1", K_FX4 }, { "fx_5_0", K_FX5 } };
    unsigned i;
    NEED();
    for (i = 0; i < 4; i++) {
        const char *entry;
        const char *src = cc_kind_src(t[i].kind, &entry);
        cc_r r;
        cc_compile_full(&r, src, strlen(src), "t.fx", NULL, NULL, entry, t[i].target, 0, 0);
        if (t_is_wine() && FAILED(r.hr)) { t_note("effect profile %s unsupported here", t[i].target); cc_free(&r); continue; }
        CHECKF(r.hr == S_OK, "target %s hr=0x%08lx err=%.150s", t[i].target, (unsigned long)r.hr, r.err);
        if (r.code) {
            CHECKF(cc_size(r.code) > 16, "target %s size", t[i].target);
            t_note("%s first dword 0x%08lx size %lu", t[i].target, (unsigned long)cc_rd32(cc_ptr(r.code)), (unsigned long)cc_size(r.code));
        }
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_targets_effect_ignores_entry_point)
{
    cc_r a, b;
    NEED();
    if (t_is_wine()) { t_skip("effect profiles"); return; }
    cc_compile_full(&a, cc_fx5_src, strlen(cc_fx5_src), "t.fx", NULL, NULL, NULL, "fx_5_0", 0, 0);
    cc_compile_full(&b, cc_fx5_src, strlen(cc_fx5_src), "t.fx", NULL, NULL, "this_is_ignored", "fx_5_0", 0, 0);
    EXPECT_HR(a.hr, S_OK);
    EXPECT_HR(b.hr, S_OK);
    if (a.code && b.code) {
        EXPECT_EQ(cc_size(a.code), cc_size(b.code));
        if (cc_size(a.code) == cc_size(b.code)) CHECK(!memcmp(cc_ptr(a.code), cc_ptr(b.code), cc_size(a.code)));
    }
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_targets_texture_shader_note)
{
    static const cc_tgt t[] = { { "tx_1_0", K_TX } };
    NEED();
    cc_target_table_run(t, 1, 0);
}

/* ---- legacy output format ------------------------------------------------------ */
TEST(d3dcompiler_compile_targets_sm3_output_is_raw_bytecode)
{
    static const struct { const char *t; DWORD tok; } tab[] = {
        { "vs_2_0", 0xFFFE0200 }, { "vs_3_0", 0xFFFE0300 }, { "ps_2_0", 0xFFFF0200 }, { "ps_3_0", 0xFFFF0300 } };
    unsigned i;
    NEED();
    for (i = 0; i < 4; i++) {
        cc_r r;
        EXPECT_HR(cc_run(&r, tab[i].t[0] == 'v' ? CC_VS_LEGACY : CC_PS_LEGACY, "main", tab[i].t, 0), S_OK);
        if (r.code) {
            SIZE_T n = cc_size(r.code);
            CHECKF(n >= 8 && (n % 4) == 0, "%s size %lu", tab[i].t, (unsigned long)n);
            if (n >= 8) {
                EXPECT_EQ(cc_rd32(cc_ptr(r.code)), tab[i].tok);
                EXPECT_EQ(cc_rd32(cc_ptr(r.code) + n - 4), 0x0000FFFF);
            }
            CHECK(!cc_dxbc_ok(cc_ptr(r.code), n));
        }
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_targets_sw_version_tokens)
{
    static const struct { const char *t; DWORD tok; } tab[] = {
        { "vs_2_sw", 0xFFFE02FF }, { "vs_3_sw", 0xFFFE03FF }, { "ps_2_sw", 0xFFFF02FF }, { "ps_3_sw", 0xFFFF03FF } };
    unsigned i;
    NEED();
    for (i = 0; i < 4; i++) {
        cc_r r;
        cc_run(&r, tab[i].t[0] == 'v' ? CC_VS_LEGACY : CC_PS_LEGACY, "main", tab[i].t, 0);
        if (SUCCEEDED(r.hr) && r.code && cc_size(r.code) >= 4) {
            CHECKF(cc_rd32(cc_ptr(r.code)) == tab[i].tok, "%s token 0x%08lx", tab[i].t, (unsigned long)cc_rd32(cc_ptr(r.code)));
        } else {
            t_note("%s did not compile: hr=0x%08lx", tab[i].t, (unsigned long)r.hr);
        }
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_targets_sm2_a_b_same_version_token)
{
    static const char *tg[] = { "ps_2_0", "ps_2_a", "ps_2_b" };
    DWORD tok[3] = { 0 };
    int i;
    NEED();
    for (i = 0; i < 3; i++) {
        cc_r r;
        EXPECT_HR(cc_run(&r, CC_PS_LEGACY, "main", tg[i], 0), S_OK);
        if (r.code) tok[i] = cc_rd32(cc_ptr(r.code));
        cc_free(&r);
    }
    EXPECT_EQ(tok[0], 0xFFFF0200);
    EXPECT_EQ(tok[1], 0xFFFF0200);
    EXPECT_EQ(tok[2], 0xFFFF0200);
}

/* ---- profile/stage and profile/feature mismatches ------------------------------- */
TEST(d3dcompiler_compile_targets_pixel_source_as_vertex_shader)
{
    cc_r r;
    NEED();
    cc_run(&r, CC_PS_SRC, "main", "vs_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    CHECK(r.errb != NULL);
    t_note("ps entry as vs_5_0: hr=0x%08lx %.120s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_vertex_source_as_pixel_shader_note)
{
    cc_r r;
    NEED();
    cc_run(&r, "float4 main(float4 p : POSITION) : SV_Position { return p; }", "main", "ps_5_0", 0);
    t_note("SV_Position output as ps_5_0: hr=0x%08lx %.120s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_texture_objects_need_sm4)
{
    static const char *src =
        "Texture2D t : register(t0); SamplerState s : register(s0);\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR0 { return t.Sample(s, uv); }\n";
    static const char *tg[] = { "ps_2_0", "ps_3_0" };
    int i;
    NEED();
    for (i = 0; i < 2; i++) {
        cc_r r;
        cc_run(&r, src, "main", tg[i], 0);
        CHECKF((LONG)r.hr < 0, "%s hr=0x%08lx", tg[i], (unsigned long)r.hr);
        CHECK(r.code == NULL);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_targets_texture_objects_ok_in_sm4)
{
    static const char *src =
        "Texture2D t : register(t0); SamplerState s : register(s0);\n"
        "float4 main(float2 uv : TEXCOORD0) : SV_Target { return t.Sample(s, uv); }\n";
    static const char *tg[] = { "ps_4_0", "ps_4_1", "ps_5_0", "ps_4_0_level_9_1", "ps_4_0_level_9_3" };
    int i;
    NEED();
    for (i = 0; i < 5; i++) {
        cc_r r;
        EXPECT_HR(cc_run(&r, src, "main", tg[i], 0), S_OK);
        cc_free(&r);
    }
}

TEST(d3dcompiler_compile_targets_ps20_arithmetic_slot_limit)
{
    static char src[8192];
    size_t n = 0;
    int i;
    cc_r a, b;
    NEED();
    if (t_is_wine()) { t_skip("legacy instruction slot limits are not modelled"); return; }
    n += snprintf(src + n, sizeof src - n, "float4 main(float4 c : COLOR0) : COLOR0 {\n float4 r = c;\n");
    for (i = 0; i < 40; i++) n += snprintf(src + n, sizeof src - n, " r = sin(r) * cos(r.wzyx) + r;\n");
    n += snprintf(src + n, sizeof src - n, " return r;\n}\n");
    cc_run(&a, src, "main", "ps_2_0", 0);
    CHECK((LONG)a.hr < 0);
    CHECK(a.code == NULL);
    CHECK(strstr(a.err, "ps_2_0") != NULL);
    CHECK(strstr(a.err, "error X5608") != NULL || strstr(a.err, "X5") != NULL);
    t_note("ps_2_0 limit message: %.160s", a.err);
    cc_run(&b, src, "main", "ps_3_0", 0);
    t_note("same shader on ps_3_0: hr=0x%08lx", (unsigned long)b.hr);
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_targets_sm4_integer_ops)
{
    static const char *src =
        "uint4 g;\nfloat4 main(uint id : SV_VertexID) : SV_Position { uint v = (id << 3) ^ g.x; return float4(v & 0xff, v >> 2, 0, 1); }\n";
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, src, "main", "vs_4_0", 0), S_OK);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_system_value_semantics_need_sm4)
{
    static const char *src = "float4 main(uint id : SV_VertexID) : POSITION { return id; }";
    cc_r r;
    NEED();
    cc_run(&r, src, "main", "vs_3_0", 0);
    CHECK((LONG)r.hr < 0);
    t_note("SV_VertexID in vs_3_0: hr=0x%08lx %.120s", (unsigned long)r.hr, r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_multiple_render_targets_level9)
{
    static const char *src = "void main(out float4 a : SV_Target0, out float4 b : SV_Target1) { a = 1; b = 2; }";
    cc_r r;
    NEED();
    EXPECT_HR(cc_run(&r, src, "main", "ps_4_0", 0), S_OK);
    cc_free(&r);
    cc_run(&r, src, "main", "ps_4_0_level_9_1", 0);
    t_note("2 render targets on level_9_1: hr=0x%08lx %.100s", (unsigned long)r.hr, r.err);
    cc_free(&r);
    cc_run(&r, src, "main", "ps_4_0_level_9_3", 0);
    t_note("2 render targets on level_9_3: hr=0x%08lx", (unsigned long)r.hr);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_cs_requires_numthreads_attribute)
{
    static const char *src = "RWByteAddressBuffer b : register(u0);\nvoid main(uint3 id : SV_DispatchThreadID) { b.Store(0, id.x); }\n";
    cc_r r;
    NEED();
    cc_run(&r, src, "main", "cs_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_cs_thread_group_limits)
{
    cc_r r;
    NEED();
    cc_run(&r, "RWByteAddressBuffer b : register(u0);\n[numthreads(1025,1,1)] void main(uint3 id : SV_DispatchThreadID) { b.Store(0, id.x); }\n", "main", "cs_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    cc_free(&r);
    cc_run(&r, "RWByteAddressBuffer b : register(u0);\n[numthreads(1024,1,1)] void main(uint3 id : SV_DispatchThreadID) { b.Store(0, id.x); }\n", "main", "cs_5_0", 0);
    EXPECT_HR(r.hr, S_OK);
    cc_free(&r);
    cc_run(&r, "RWByteAddressBuffer b : register(u0);\n[numthreads(2,2,65)] void main(uint3 id : SV_DispatchThreadID) { b.Store(0, id.x); }\n", "main", "cs_5_0", 0);
    CHECK((LONG)r.hr < 0);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_cs_40_thread_limit_512)
{
    cc_r r;
    NEED();
    cc_run(&r, "RWByteAddressBuffer b : register(u0);\n[numthreads(1024,1,1)] void main(uint3 id : SV_DispatchThreadID) { b.Store(0, id.x); }\n", "main", "cs_4_0", 0);
    t_note("cs_4_0 1024 threads: hr=0x%08lx", (unsigned long)r.hr);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_gs_requires_maxvertexcount)
{
    static const char *src =
        "struct GO { float4 p : SV_Position; };\n"
        "void main(triangle float4 v[3] : SV_Position, inout TriangleStream<GO> s) { GO o; o.p = v[0]; s.Append(o); }\n";
    cc_r r;
    NEED();
    if (t_is_wine()) { t_skip("geometry shaders"); return; }
    cc_run(&r, src, "main", "gs_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    t_note("gs without maxvertexcount: %.120s", r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_hs_requires_patch_constant_function)
{
    static const char *src =
        "struct VO { float4 p : POSITION; };\nstruct CP { float4 p : POSITION; };\n"
        "[domain(\"tri\")][partitioning(\"integer\")][outputtopology(\"triangle_cw\")][outputcontrolpoints(3)]\n"
        "CP main(InputPatch<VO, 3> ip, uint i : SV_OutputControlPointID) { CP o; o.p = ip[i].p; return o; }\n";
    cc_r r;
    NEED();
    if (t_is_wine()) { t_skip("hull shaders"); return; }
    cc_run(&r, src, "main", "hs_5_0", 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    t_note("hs without patchconstantfunc: %.120s", r.err);
    cc_free(&r);
}

TEST(d3dcompiler_compile_targets_hs_domain_attribute_values)
{
    static const char *doms[] = { "tri", "quad", "isoline" };
    int i;
    NEED();
    if (t_is_wine()) { t_skip("hull shaders"); return; }
    for (i = 0; i < 3; i++) {
        char src[1024];
        cc_r r;
        snprintf(src, sizeof src,
            "struct VO { float4 p : POSITION; };\nstruct CP { float4 p : POSITION; };\n"
            "struct PC { float e[%d] : SV_TessFactor; %s };\n"
            "PC pcf(InputPatch<VO, 4> ip) { PC o; o.e[0] = 1; o.e[1] = 1; %s %s return o; }\n"
            "[domain(\"%s\")][partitioning(\"integer\")][outputtopology(\"%s\")][outputcontrolpoints(4)][patchconstantfunc(\"pcf\")]\n"
            "CP main(InputPatch<VO, 4> ip, uint i : SV_OutputControlPointID) { CP o; o.p = ip[i].p; return o; }\n",
            i == 0 ? 3 : (i == 1 ? 4 : 2),
            i == 0 ? "float i : SV_InsideTessFactor;" : (i == 1 ? "float i[2] : SV_InsideTessFactor;" : ""),
            i == 0 ? "o.e[2] = 1; o.i = 1;" : (i == 1 ? "o.e[2] = 1; o.e[3] = 1; o.i[0] = 1; o.i[1] = 1;" : ""),
            "",
            doms[i], i == 2 ? "line" : "triangle_cw");
        cc_run(&r, src, "main", "hs_5_0", 0);
        if (i < 2) EXPECT_HR(r.hr, S_OK);
        if (FAILED(r.hr)) t_note("domain %s: %.200s", doms[i], r.err);
        cc_free(&r);
    }
}
