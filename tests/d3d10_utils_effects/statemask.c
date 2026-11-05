/* D3D10_STATE_BLOCK_MASK helpers: pure CPU functions of d3d10.dll / d3d10_1.dll. */
#include "ue.h"

#define E_INVALIDARG_ ((HRESULT)0x80070057u)
#define MASKSZ 76

typedef struct { int type; const char *name; size_t off; size_t bytes; } stype;
#define OFS(f) offsetof(D3D10_STATE_BLOCK_MASK, f)
static const stype k_types[] = {
    { D3D10_DST_SO_BUFFERS, "SO_BUFFERS", OFS(SOBuffers), 1 },
    { D3D10_DST_OM_RENDER_TARGETS, "OM_RENDER_TARGETS", OFS(OMRenderTargets), 1 },
    { D3D10_DST_OM_DEPTH_STENCIL_STATE, "OM_DEPTH_STENCIL_STATE", OFS(OMDepthStencilState), 1 },
    { D3D10_DST_OM_BLEND_STATE, "OM_BLEND_STATE", OFS(OMBlendState), 1 },
    { D3D10_DST_VS, "VS", OFS(VS), 1 },
    { D3D10_DST_VS_SAMPLERS, "VS_SAMPLERS", OFS(VSSamplers), 2 },
    { D3D10_DST_VS_SHADER_RESOURCES, "VS_SHADER_RESOURCES", OFS(VSShaderResources), 16 },
    { D3D10_DST_VS_CONSTANT_BUFFERS, "VS_CONSTANT_BUFFERS", OFS(VSConstantBuffers), 2 },
    { D3D10_DST_GS, "GS", OFS(GS), 1 },
    { D3D10_DST_GS_SAMPLERS, "GS_SAMPLERS", OFS(GSSamplers), 2 },
    { D3D10_DST_GS_SHADER_RESOURCES, "GS_SHADER_RESOURCES", OFS(GSShaderResources), 16 },
    { D3D10_DST_GS_CONSTANT_BUFFERS, "GS_CONSTANT_BUFFERS", OFS(GSConstantBuffers), 2 },
    { D3D10_DST_PS, "PS", OFS(PS), 1 },
    { D3D10_DST_PS_SAMPLERS, "PS_SAMPLERS", OFS(PSSamplers), 2 },
    { D3D10_DST_PS_SHADER_RESOURCES, "PS_SHADER_RESOURCES", OFS(PSShaderResources), 16 },
    { D3D10_DST_PS_CONSTANT_BUFFERS, "PS_CONSTANT_BUFFERS", OFS(PSConstantBuffers), 2 },
    { D3D10_DST_IA_VERTEX_BUFFERS, "IA_VERTEX_BUFFERS", OFS(IAVertexBuffers), 2 },
    { D3D10_DST_IA_INDEX_BUFFER, "IA_INDEX_BUFFER", OFS(IAIndexBuffer), 1 },
    { D3D10_DST_IA_INPUT_LAYOUT, "IA_INPUT_LAYOUT", OFS(IAInputLayout), 1 },
    { D3D10_DST_IA_PRIMITIVE_TOPOLOGY, "IA_PRIMITIVE_TOPOLOGY", OFS(IAPrimitiveTopology), 1 },
    { D3D10_DST_RS_VIEWPORTS, "RS_VIEWPORTS", OFS(RSViewports), 1 },
    { D3D10_DST_RS_SCISSOR_RECTS, "RS_SCISSOR_RECTS", OFS(RSScissorRects), 1 },
    { D3D10_DST_RS_RASTERIZER_STATE, "RS_RASTERIZER_STATE", OFS(RSRasterizerState), 1 },
    { D3D10_DST_PREDICATION, "PREDICATION", OFS(Predication), 1 },
};
#define NTYPES ((int)(sizeof k_types / sizeof *k_types))

/* known-exact capacities (slot counts) of the multi-slot types */
static int known_count(int type)
{
    switch (type) {
    case D3D10_DST_VS: case D3D10_DST_GS: case D3D10_DST_PS: return 1;
    case D3D10_DST_VS_SAMPLERS: case D3D10_DST_GS_SAMPLERS: case D3D10_DST_PS_SAMPLERS: return 16;
    case D3D10_DST_VS_SHADER_RESOURCES: case D3D10_DST_GS_SHADER_RESOURCES: case D3D10_DST_PS_SHADER_RESOURCES: return 128;
    case D3D10_DST_VS_CONSTANT_BUFFERS: case D3D10_DST_GS_CONSTANT_BUFFERS: case D3D10_DST_PS_CONSTANT_BUFFERS: return 14;
    case D3D10_DST_IA_VERTEX_BUFFERS: return 16;
    }
    return 0;
}

#define FN_UNION       UIN("d3d10.dll", D3D10StateBlockMaskUnion)
#define FN_INTERSECT   UIN("d3d10.dll", D3D10StateBlockMaskIntersect)
#define FN_DIFFERENCE  UIN("d3d10.dll", D3D10StateBlockMaskDifference)
#define FN_ENABLEALL   UIN("d3d10.dll", D3D10StateBlockMaskEnableAll)
#define FN_DISABLEALL  UIN("d3d10.dll", D3D10StateBlockMaskDisableAll)
#define FN_ENABLE      UIN("d3d10.dll", D3D10StateBlockMaskEnableCapture)
#define FN_DISABLE     UIN("d3d10.dll", D3D10StateBlockMaskDisableCapture)
#define FN_GET         UIN("d3d10.dll", D3D10StateBlockMaskGetSetting)

static unsigned char *B(D3D10_STATE_BLOCK_MASK *m) { return (unsigned char *)m; }
static int popcount_mask(const D3D10_STATE_BLOCK_MASK *m)
{
    int n = 0;
    for (int i = 0; i < MASKSZ; i++) for (int b = 0; b < 8; b++) n += (((const unsigned char *)m)[i] >> b) & 1;
    return n;
}
static void fill_rand(D3D10_STATE_BLOCK_MASK *m, d3dh_rng *r)
{
    for (int i = 0; i < MASKSZ; i++) B(m)[i] = (unsigned char)d3dh_rng_u32(r);
}

TEST(d3d10_utils_effects_mask_struct_layout)
{
    EXPECT_EQ(sizeof(D3D10_STATE_BLOCK_MASK), MASKSZ);
    EXPECT_EQ(OFS(VS), 0);
    EXPECT_EQ(OFS(VSSamplers), 1);
    EXPECT_EQ(OFS(VSShaderResources), 3);
    EXPECT_EQ(OFS(VSConstantBuffers), 19);
    EXPECT_EQ(OFS(GS), 21);
    EXPECT_EQ(OFS(PS), 42);
    EXPECT_EQ(OFS(IAVertexBuffers), 63);
    EXPECT_EQ(OFS(Predication), 75);
}
TEST(d3d10_utils_effects_mask_enable_all)
{
    REQUIRE(FN_ENABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    memset(&m, 0x5a, sizeof m);
    EXPECT_HR(FN_ENABLEALL(&m), S_OK);
    for (int i = 0; i < MASKSZ; i++) { if (B(&m)[i] != 0xff) { CHECKF(0, "byte %d = %02x", i, B(&m)[i]); break; } }
    EXPECT_EQ(popcount_mask(&m), MASKSZ * 8);
}
TEST(d3d10_utils_effects_mask_enable_all_null)
{
    REQUIRE(FN_ENABLEALL);
    EXPECT_HR(FN_ENABLEALL(NULL), E_INVALIDARG_);
}
TEST(d3d10_utils_effects_mask_disable_all)
{
    REQUIRE(FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    memset(&m, 0xff, sizeof m);
    EXPECT_HR(FN_DISABLEALL(&m), S_OK);
    EXPECT_EQ(popcount_mask(&m), 0);
}
TEST(d3d10_utils_effects_mask_disable_all_null)
{
    REQUIRE(FN_DISABLEALL);
    EXPECT_HR(FN_DISABLEALL(NULL), E_INVALIDARG_);
}
TEST(d3d10_utils_effects_mask_disable_all_does_not_overrun)
{
    REQUIRE(FN_DISABLEALL);
    unsigned char buf[MASKSZ + 16];
    memset(buf, 0xcd, sizeof buf);
    EXPECT_HR(FN_DISABLEALL((D3D10_STATE_BLOCK_MASK *)buf), S_OK);
    for (int i = 0; i < MASKSZ; i++) if (buf[i] != 0) { CHECKF(0, "byte %d", i); break; }
    for (int i = MASKSZ; i < MASKSZ + 16; i++) if (buf[i] != 0xcd) { CHECKF(0, "overrun at %d", i); break; }
}
TEST(d3d10_utils_effects_mask_enable_all_does_not_overrun)
{
    REQUIRE(FN_ENABLEALL);
    unsigned char buf[MASKSZ + 16];
    memset(buf, 0x00, sizeof buf);
    EXPECT_HR(FN_ENABLEALL((D3D10_STATE_BLOCK_MASK *)buf), S_OK);
    for (int i = 0; i < MASKSZ; i++) if (buf[i] != 0xff) { CHECKF(0, "byte %d", i); break; }
    for (int i = MASKSZ; i < MASKSZ + 16; i++) if (buf[i] != 0) { CHECKF(0, "overrun at %d", i); break; }
}

/* ---- binary operations ---- */
typedef HRESULT (WINAPI *binop)(D3D10_STATE_BLOCK_MASK *, D3D10_STATE_BLOCK_MASK *, D3D10_STATE_BLOCK_MASK *);
static void binop_semantics(binop f, int (*ref)(int, int), const char *what)
{
    d3dh_rng r;
    d3dh_rng_seed(&r, 0x10e0 + (uintptr_t)what[0]);
    for (int iter = 0; iter < 64; iter++) {
        D3D10_STATE_BLOCK_MASK x, y, res;
        fill_rand(&x, &r); fill_rand(&y, &r);
        memset(&res, 0xee, sizeof res);
        HRESULT hr = f(&x, &y, &res);
        if (FAILED(hr)) { EXPECT_HR(hr, S_OK); return; }
        int bad = 0;
        for (int i = 0; i < MASKSZ; i++) if (B(&res)[i] != (unsigned char)ref(B(&x)[i], B(&y)[i])) bad++;
        t_check(bad == 0, __FILE__, __LINE__, "per-byte result", "%s: %d bytes differ", what, bad);
        if (bad) return;
    }
}
static int r_or(int a, int b) { return a | b; }
static int r_and(int a, int b) { return a & b; }
static int r_xor(int a, int b) { return a ^ b; }
TEST(d3d10_utils_effects_mask_union_semantics) { REQUIRE(FN_UNION); binop_semantics(FN_UNION, r_or, "union"); }
TEST(d3d10_utils_effects_mask_intersect_semantics) { REQUIRE(FN_INTERSECT); binop_semantics(FN_INTERSECT, r_and, "intersect"); }
TEST(d3d10_utils_effects_mask_difference_is_xor) { REQUIRE(FN_DIFFERENCE); binop_semantics(FN_DIFFERENCE, r_xor, "difference"); }

static void binop_null_args(binop f)
{
    D3D10_STATE_BLOCK_MASK x, y, res;
    memset(&x, 0x0f, sizeof x); memset(&y, 0xf0, sizeof y); memset(&res, 0x77, sizeof res);
    EXPECT_HR(f(NULL, &y, &res), E_INVALIDARG_);
    EXPECT_HR(f(&x, NULL, &res), E_INVALIDARG_);
    EXPECT_HR(f(&x, &y, NULL), E_INVALIDARG_);
    EXPECT_HR(f(NULL, NULL, NULL), E_INVALIDARG_);
    EXPECT_HR(f(NULL, NULL, &res), E_INVALIDARG_);
    int untouched = 1;
    for (int i = 0; i < MASKSZ; i++) if (B(&res)[i] != 0x77) untouched = 0;
    CHECK(untouched);
}
TEST(d3d10_utils_effects_mask_union_null_args) { REQUIRE(FN_UNION); binop_null_args(FN_UNION); }
TEST(d3d10_utils_effects_mask_intersect_null_args) { REQUIRE(FN_INTERSECT); binop_null_args(FN_INTERSECT); }
TEST(d3d10_utils_effects_mask_difference_null_args) { REQUIRE(FN_DIFFERENCE); binop_null_args(FN_DIFFERENCE); }

static void binop_alias(binop f, int (*ref)(int, int))
{
    d3dh_rng r;
    d3dh_rng_seed(&r, 99);
    D3D10_STATE_BLOCK_MASK x, y, x0, y0;
    fill_rand(&x, &r); fill_rand(&y, &r);
    x0 = x; y0 = y;
    /* result aliases the first operand */
    EXPECT_HR(f(&x, &y, &x), S_OK);
    int bad = 0;
    for (int i = 0; i < MASKSZ; i++) if (B(&x)[i] != (unsigned char)ref(B(&x0)[i], B(&y0)[i])) bad++;
    EXPECT_EQ(bad, 0);
    /* result aliases the second operand */
    x = x0; y = y0;
    EXPECT_HR(f(&x, &y, &y), S_OK);
    bad = 0;
    for (int i = 0; i < MASKSZ; i++) if (B(&y)[i] != (unsigned char)ref(B(&x0)[i], B(&y0)[i])) bad++;
    EXPECT_EQ(bad, 0);
    /* both operands the same object */
    x = x0;
    D3D10_STATE_BLOCK_MASK res;
    EXPECT_HR(f(&x, &x, &res), S_OK);
    bad = 0;
    for (int i = 0; i < MASKSZ; i++) if (B(&res)[i] != (unsigned char)ref(B(&x0)[i], B(&x0)[i])) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(d3d10_utils_effects_mask_union_aliasing) { REQUIRE(FN_UNION); binop_alias(FN_UNION, r_or); }
TEST(d3d10_utils_effects_mask_intersect_aliasing) { REQUIRE(FN_INTERSECT); binop_alias(FN_INTERSECT, r_and); }
TEST(d3d10_utils_effects_mask_difference_aliasing) { REQUIRE(FN_DIFFERENCE); binop_alias(FN_DIFFERENCE, r_xor); }

TEST(d3d10_utils_effects_mask_difference_self_is_empty)
{
    REQUIRE(FN_DIFFERENCE && FN_ENABLEALL);
    D3D10_STATE_BLOCK_MASK a, r;
    FN_ENABLEALL(&a);
    EXPECT_HR(FN_DIFFERENCE(&a, &a, &r), S_OK);
    EXPECT_EQ(popcount_mask(&r), 0);
}
TEST(d3d10_utils_effects_mask_difference_with_empty_is_identity)
{
    REQUIRE(FN_DIFFERENCE && FN_DISABLEALL);
    d3dh_rng rg; d3dh_rng_seed(&rg, 5);
    D3D10_STATE_BLOCK_MASK a, z, r;
    fill_rand(&a, &rg);
    FN_DISABLEALL(&z);
    EXPECT_HR(FN_DIFFERENCE(&a, &z, &r), S_OK);
    EXPECT_EQ(memcmp(&a, &r, MASKSZ), 0);
}
TEST(d3d10_utils_effects_mask_difference_does_not_clear_common_bits_only)
{
    /* the "difference" of the API is a symmetric difference: bits only in the second operand survive */
    REQUIRE(FN_DIFFERENCE && FN_ENABLE && FN_DISABLEALL && FN_GET);
    D3D10_STATE_BLOCK_MASK a, b, r;
    FN_DISABLEALL(&a); FN_DISABLEALL(&b);
    FN_ENABLE(&b, D3D10_DST_PS, 0, 1);
    EXPECT_HR(FN_DIFFERENCE(&a, &b, &r), S_OK);
    CHECK(FN_GET(&r, D3D10_DST_PS, 0) != 0);
}
TEST(d3d10_utils_effects_mask_union_idempotent)
{
    REQUIRE(FN_UNION);
    d3dh_rng rg; d3dh_rng_seed(&rg, 6);
    D3D10_STATE_BLOCK_MASK a, r;
    fill_rand(&a, &rg);
    EXPECT_HR(FN_UNION(&a, &a, &r), S_OK);
    EXPECT_EQ(memcmp(&a, &r, MASKSZ), 0);
}
TEST(d3d10_utils_effects_mask_intersect_with_empty)
{
    REQUIRE(FN_INTERSECT && FN_DISABLEALL);
    d3dh_rng rg; d3dh_rng_seed(&rg, 7);
    D3D10_STATE_BLOCK_MASK a, z, r;
    fill_rand(&a, &rg);
    FN_DISABLEALL(&z);
    EXPECT_HR(FN_INTERSECT(&a, &z, &r), S_OK);
    EXPECT_EQ(popcount_mask(&r), 0);
}
TEST(d3d10_utils_effects_mask_union_with_full)
{
    REQUIRE(FN_UNION && FN_ENABLEALL);
    d3dh_rng rg; d3dh_rng_seed(&rg, 8);
    D3D10_STATE_BLOCK_MASK a, f, r;
    fill_rand(&a, &rg);
    FN_ENABLEALL(&f);
    EXPECT_HR(FN_UNION(&a, &f, &r), S_OK);
    EXPECT_EQ(popcount_mask(&r), MASKSZ * 8);
}
TEST(d3d10_utils_effects_mask_binops_commutative)
{
    REQUIRE(FN_UNION && FN_INTERSECT && FN_DIFFERENCE);
    d3dh_rng rg; d3dh_rng_seed(&rg, 9);
    binop fs[3] = { FN_UNION, FN_INTERSECT, FN_DIFFERENCE };
    for (int k = 0; k < 3; k++) {
        D3D10_STATE_BLOCK_MASK a, b, r1, r2;
        fill_rand(&a, &rg); fill_rand(&b, &rg);
        fs[k](&a, &b, &r1); fs[k](&b, &a, &r2);
        EXPECT_EQ(memcmp(&r1, &r2, MASKSZ), 0);
    }
}
TEST(d3d10_utils_effects_mask_binops_read_all_76_bytes)
{
    REQUIRE(FN_UNION);
    /* every byte participates, including the last one */
    for (int pos = 0; pos < MASKSZ; pos += 5) {
        D3D10_STATE_BLOCK_MASK a, b, r;
        memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); memset(&r, 0, sizeof r);
        B(&a)[pos] = 0x81;
        B(&b)[MASKSZ - 1 - pos] = 0x42;
        FN_UNION(&a, &b, &r);
        CHECK(B(&r)[pos] == 0x81 || pos == MASKSZ - 1 - pos);
        CHECK(B(&r)[MASKSZ - 1 - pos] & 0x42);
    }
    D3D10_STATE_BLOCK_MASK a, b, r;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    B(&a)[MASKSZ - 1] = 1;
    FN_UNION(&a, &b, &r);
    EXPECT_EQ(B(&r)[MASKSZ - 1], 1);
    EXPECT_EQ(popcount_mask(&r), 1);
}

/* ---- EnableCapture / DisableCapture / GetSetting ---- */
TEST(d3d10_utils_effects_mask_enable_capture_bit0_each_type)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    for (int i = 0; i < NTYPES; i++) {
        D3D10_STATE_BLOCK_MASK m;
        FN_DISABLEALL(&m);
        HRESULT hr = FN_ENABLE(&m, k_types[i].type, 0, 1);
        t_check(hr == S_OK, __FILE__, __LINE__, "enable bit 0", "%s: %s", k_types[i].name, d3dh_hr_name(hr));
        EXPECT_EQ(popcount_mask(&m), 1);
        t_check(B(&m)[k_types[i].off] == 1, __FILE__, __LINE__, "bit 0 at field offset", "%s", k_types[i].name);
    }
}
TEST(d3d10_utils_effects_mask_enable_capture_last_slot_known_types)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    for (int i = 0; i < NTYPES; i++) {
        int cnt = known_count(k_types[i].type);
        if (!cnt) continue;
        D3D10_STATE_BLOCK_MASK m;
        FN_DISABLEALL(&m);
        HRESULT hr = FN_ENABLE(&m, k_types[i].type, cnt - 1, 1);
        t_check(hr == S_OK, __FILE__, __LINE__, "last slot", "%s slot %d: %s", k_types[i].name, cnt - 1, d3dh_hr_name(hr));
        int byte = (cnt - 1) / 8, bit = (cnt - 1) % 8;
        t_check(B(&m)[k_types[i].off + byte] == (1 << bit) && popcount_mask(&m) == 1, __FILE__, __LINE__, "bit location", "%s", k_types[i].name);
    }
}
TEST(d3d10_utils_effects_mask_enable_capture_one_past_known_end_fails)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    for (int i = 0; i < NTYPES; i++) {
        int cnt = known_count(k_types[i].type);
        if (!cnt) continue;
        D3D10_STATE_BLOCK_MASK m;
        FN_DISABLEALL(&m);
        HRESULT hr = FN_ENABLE(&m, k_types[i].type, cnt, 1);
        t_check(hr == E_INVALIDARG_, __FILE__, __LINE__, "slot == count", "%s: %s", k_types[i].name, d3dh_hr_name(hr));
        hr = FN_ENABLE(&m, k_types[i].type, 0, cnt + 1);
        t_check(hr == E_INVALIDARG_, __FILE__, __LINE__, "count+1 slots", "%s: %s", k_types[i].name, d3dh_hr_name(hr));
        hr = FN_ENABLE(&m, k_types[i].type, cnt - 1, 2);
        t_check(hr == E_INVALIDARG_, __FILE__, __LINE__, "straddling the end", "%s: %s", k_types[i].name, d3dh_hr_name(hr));
        EXPECT_EQ(popcount_mask(&m), 0);
    }
}
TEST(d3d10_utils_effects_mask_enable_capture_full_range_known_types)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    for (int i = 0; i < NTYPES; i++) {
        int cnt = known_count(k_types[i].type);
        if (!cnt) continue;
        D3D10_STATE_BLOCK_MASK m;
        FN_DISABLEALL(&m);
        HRESULT hr = FN_ENABLE(&m, k_types[i].type, 0, cnt);
        t_check(hr == S_OK, __FILE__, __LINE__, "whole range", "%s: %s", k_types[i].name, d3dh_hr_name(hr));
        EXPECT_EQ(popcount_mask(&m), cnt);
        for (int s = 0; s < cnt; s++) {
            if (!(B(&m)[k_types[i].off + s / 8] & (1 << (s % 8)))) { CHECKF(0, "%s slot %d clear", k_types[i].name, s); break; }
        }
    }
}
TEST(d3d10_utils_effects_mask_enable_capture_partial_range)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_DISABLEALL(&m);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_SHADER_RESOURCES, 5, 20), S_OK);
    EXPECT_EQ(popcount_mask(&m), 20);
    const unsigned char *f = B(&m) + OFS(PSShaderResources);
    EXPECT_EQ(f[0], 0xe0);
    EXPECT_EQ(f[1], 0xff);
    EXPECT_EQ(f[2], 0xff);
    EXPECT_EQ(f[3], 0x01);
    EXPECT_EQ(f[4], 0);
}
TEST(d3d10_utils_effects_mask_enable_capture_is_additive)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_DISABLEALL(&m);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_VS_SAMPLERS, 0, 2), S_OK);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_VS_SAMPLERS, 1, 3), S_OK);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_VS_SAMPLERS, 10, 1), S_OK);
    EXPECT_EQ(B(&m)[OFS(VSSamplers)], 0x0f);
    EXPECT_EQ(B(&m)[OFS(VSSamplers) + 1], 0x04);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_VS_SAMPLERS, 0, 1), S_OK);
    EXPECT_EQ(popcount_mask(&m), 5);
}
TEST(d3d10_utils_effects_mask_enable_capture_other_fields_untouched)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_DISABLEALL(&m);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_GS_CONSTANT_BUFFERS, 0, 14), S_OK);
    for (int i = 0; i < MASKSZ; i++) {
        if (i >= (int)OFS(GSConstantBuffers) && i < (int)OFS(GSConstantBuffers) + 2) continue;
        if (B(&m)[i]) { CHECKF(0, "stray byte %d", i); break; }
    }
    EXPECT_EQ(B(&m)[OFS(GSConstantBuffers)], 0xff);
    EXPECT_EQ(B(&m)[OFS(GSConstantBuffers) + 1], 0x3f);
}
static const int k_bad_types[] = { 0, 25, 26, 100, -1, 0x7fffffff, (int)0x80000000 };
TEST(d3d10_utils_effects_mask_enable_capture_invalid_type)
{
    REQUIRE(FN_ENABLE);
    for (unsigned i = 0; i < sizeof k_bad_types / sizeof *k_bad_types; i++) {
        D3D10_STATE_BLOCK_MASK m;
        memset(&m, 0, sizeof m);
        HRESULT hr = FN_ENABLE(&m, (D3D10_DEVICE_STATE_TYPES)k_bad_types[i], 0, 1);
        t_check(hr == E_INVALIDARG_, __FILE__, __LINE__, "bad type", "type %d: %s", k_bad_types[i], d3dh_hr_name(hr));
        EXPECT_EQ(popcount_mask(&m), 0);
    }
}
TEST(d3d10_utils_effects_mask_enable_capture_null_mask)
{
    REQUIRE(FN_ENABLE);
    EXPECT_HR(FN_ENABLE(NULL, D3D10_DST_VS, 0, 1), E_INVALIDARG_);
}
TEST(d3d10_utils_effects_mask_enable_capture_zero_count)
{
    REQUIRE(FN_ENABLE);
    D3D10_STATE_BLOCK_MASK m;
    memset(&m, 0, sizeof m);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_SAMPLERS, 0, 0), E_INVALIDARG_);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_SAMPLERS, 5, 0), E_INVALIDARG_);
    EXPECT_EQ(popcount_mask(&m), 0);
}
TEST(d3d10_utils_effects_mask_enable_capture_start_overflow)
{
    REQUIRE(FN_ENABLE);
    D3D10_STATE_BLOCK_MASK m;
    memset(&m, 0, sizeof m);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_SHADER_RESOURCES, 0xffffffffu, 2), E_INVALIDARG_);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_SHADER_RESOURCES, 2, 0xffffffffu), E_INVALIDARG_);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_SHADER_RESOURCES, 0xfffffff0u, 0x20), E_INVALIDARG_);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_SHADER_RESOURCES, 0x80000000u, 0x80000000u), E_INVALIDARG_);
    EXPECT_EQ(popcount_mask(&m), 0);
}
TEST(d3d10_utils_effects_mask_enable_capture_srv_boundaries)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_DISABLEALL(&m);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 120, 8), S_OK);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 120, 9), E_INVALIDARG_);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 127, 1), S_OK);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 128, 1), E_INVALIDARG_);
    EXPECT_EQ(B(&m)[OFS(VSShaderResources) + 15], 0xff);
    EXPECT_EQ(popcount_mask(&m), 8);
}
TEST(d3d10_utils_effects_mask_enable_capture_cb_boundaries)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_DISABLEALL(&m);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_CONSTANT_BUFFERS, 13, 1), S_OK);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_CONSTANT_BUFFERS, 14, 1), E_INVALIDARG_);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_CONSTANT_BUFFERS, 15, 1), E_INVALIDARG_);
    EXPECT_EQ(B(&m)[OFS(PSConstantBuffers) + 1], 0x20);
}
TEST(d3d10_utils_effects_mask_enable_capture_failure_leaves_mask)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_DISABLEALL(&m);
    EXPECT_HR(FN_ENABLE(&m, D3D10_DST_PS_SAMPLERS, 8, 100), E_INVALIDARG_);
    EXPECT_EQ(popcount_mask(&m), 0);
}
TEST(d3d10_utils_effects_mask_probe_single_slot_capacities)
{
    REQUIRE(FN_ENABLE && FN_DISABLEALL);
    for (int i = 0; i < NTYPES; i++) {
        int maxn = 0;
        for (int n = 1; n <= 256; n++) {
            D3D10_STATE_BLOCK_MASK m;
            FN_DISABLEALL(&m);
            if (FN_ENABLE(&m, k_types[i].type, 0, n) == S_OK) maxn = n; else break;
        }
        int known = known_count(k_types[i].type);
        if (known) t_check(maxn == known, __FILE__, __LINE__, "slot count", "%s: %d want %d", k_types[i].name, maxn, known);
        else t_note("%s capacity %d slots", k_types[i].name, maxn);
        t_check(maxn >= 1 && maxn <= (int)k_types[i].bytes * 8, __FILE__, __LINE__, "capacity fits the mask field", "%s: %d", k_types[i].name, maxn);
    }
}

TEST(d3d10_utils_effects_mask_disable_capture_basic)
{
    REQUIRE(FN_ENABLE && FN_DISABLE && FN_ENABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_ENABLEALL(&m);
    EXPECT_HR(FN_DISABLE(&m, D3D10_DST_PS_SHADER_RESOURCES, 3, 10), S_OK);
    EXPECT_EQ(popcount_mask(&m), MASKSZ * 8 - 10);
    const unsigned char *f = B(&m) + OFS(PSShaderResources);
    EXPECT_EQ(f[0], 0x07);
    EXPECT_EQ(f[1], 0xe0);
    EXPECT_EQ(f[2], 0xff);
}
TEST(d3d10_utils_effects_mask_disable_capture_each_type_bit0)
{
    REQUIRE(FN_DISABLE && FN_ENABLEALL);
    for (int i = 0; i < NTYPES; i++) {
        D3D10_STATE_BLOCK_MASK m;
        FN_ENABLEALL(&m);
        HRESULT hr = FN_DISABLE(&m, k_types[i].type, 0, 1);
        t_check(hr == S_OK, __FILE__, __LINE__, "disable bit 0", "%s: %s", k_types[i].name, d3dh_hr_name(hr));
        EXPECT_EQ(popcount_mask(&m), MASKSZ * 8 - 1);
        t_check(B(&m)[k_types[i].off] == 0xfe, __FILE__, __LINE__, "bit 0 cleared", "%s", k_types[i].name);
    }
}
TEST(d3d10_utils_effects_mask_disable_capture_errors)
{
    REQUIRE(FN_DISABLE && FN_ENABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_ENABLEALL(&m);
    EXPECT_HR(FN_DISABLE(NULL, D3D10_DST_VS, 0, 1), E_INVALIDARG_);
    EXPECT_HR(FN_DISABLE(&m, (D3D10_DEVICE_STATE_TYPES)0, 0, 1), E_INVALIDARG_);
    EXPECT_HR(FN_DISABLE(&m, (D3D10_DEVICE_STATE_TYPES)25, 0, 1), E_INVALIDARG_);
    EXPECT_HR(FN_DISABLE(&m, D3D10_DST_VS_SAMPLERS, 0, 0), E_INVALIDARG_);
    EXPECT_HR(FN_DISABLE(&m, D3D10_DST_VS_SAMPLERS, 16, 1), E_INVALIDARG_);
    EXPECT_HR(FN_DISABLE(&m, D3D10_DST_VS_SAMPLERS, 0xffffffffu, 2), E_INVALIDARG_);
    EXPECT_EQ(popcount_mask(&m), MASKSZ * 8);
}
TEST(d3d10_utils_effects_mask_enable_then_disable_roundtrip)
{
    REQUIRE(FN_ENABLE && FN_DISABLE && FN_DISABLEALL);
    for (int i = 0; i < NTYPES; i++) {
        int cnt = known_count(k_types[i].type);
        if (!cnt) cnt = 1;
        D3D10_STATE_BLOCK_MASK m;
        FN_DISABLEALL(&m);
        EXPECT_HR(FN_ENABLE(&m, k_types[i].type, 0, cnt), S_OK);
        EXPECT_HR(FN_DISABLE(&m, k_types[i].type, 0, cnt), S_OK);
        t_check(popcount_mask(&m) == 0, __FILE__, __LINE__, "roundtrip clears", "%s", k_types[i].name);
    }
}
TEST(d3d10_utils_effects_mask_disable_capture_idempotent)
{
    REQUIRE(FN_DISABLE && FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_DISABLEALL(&m);
    EXPECT_HR(FN_DISABLE(&m, D3D10_DST_VS_SAMPLERS, 0, 16), S_OK);
    EXPECT_EQ(popcount_mask(&m), 0);
}

TEST(d3d10_utils_effects_mask_get_setting_returns_masked_bit)
{
    REQUIRE(FN_ENABLE && FN_GET && FN_DISABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_DISABLEALL(&m);
    FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 0, 1);
    FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 3, 1);
    FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 7, 1);
    FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 9, 1);
    FN_ENABLE(&m, D3D10_DST_VS_SHADER_RESOURCES, 127, 1);
    /* the result is the isolated bit, not a normalised boolean */
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SHADER_RESOURCES, 0), 1);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SHADER_RESOURCES, 3), 8);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SHADER_RESOURCES, 7), 0x80);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SHADER_RESOURCES, 9), 2);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SHADER_RESOURCES, 127), 0x80);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SHADER_RESOURCES, 1), 0);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SHADER_RESOURCES, 8), 0);
}
TEST(d3d10_utils_effects_mask_get_setting_out_of_range)
{
    REQUIRE(FN_GET && FN_ENABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_ENABLEALL(&m);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SHADER_RESOURCES, 128), 0);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SAMPLERS, 16), 0);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_CONSTANT_BUFFERS, 14), 0);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS, 1), 0);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS_SAMPLERS, 0xffffffffu), 0);
    EXPECT_EQ(FN_GET(&m, D3D10_DST_VS, 0), 1);
}
TEST(d3d10_utils_effects_mask_get_setting_invalid_args)
{
    REQUIRE(FN_GET && FN_ENABLEALL);
    D3D10_STATE_BLOCK_MASK m;
    FN_ENABLEALL(&m);
    EXPECT_EQ(FN_GET(NULL, D3D10_DST_VS, 0), 0);
    for (unsigned i = 0; i < sizeof k_bad_types / sizeof *k_bad_types; i++)
        t_check(FN_GET(&m, (D3D10_DEVICE_STATE_TYPES)k_bad_types[i], 0) == 0, __FILE__, __LINE__, "bad type returns 0", "type %d", k_bad_types[i]);
}
TEST(d3d10_utils_effects_mask_get_setting_matches_enable_for_every_slot)
{
    REQUIRE(FN_ENABLE && FN_GET && FN_DISABLEALL);
    for (int i = 0; i < NTYPES; i++) {
        int cnt = known_count(k_types[i].type);
        if (!cnt) cnt = 1;
        for (int s = 0; s < cnt; s++) {
            D3D10_STATE_BLOCK_MASK m;
            FN_DISABLEALL(&m);
            if (FN_ENABLE(&m, k_types[i].type, s, 1) != S_OK) { CHECKF(0, "%s slot %d", k_types[i].name, s); continue; }
            int bad = 0;
            for (int q = 0; q < cnt; q++) if ((FN_GET(&m, k_types[i].type, q) != 0) != (q == s)) bad++;
            if (bad) { CHECKF(0, "%s slot %d: %d mismatches", k_types[i].name, s, bad); break; }
        }
    }
}

/* ---- same API through d3d10_1.dll ---- */
TEST(d3d10_utils_effects_mask_d3d10_1_entry_points)
{
    REQUIRE(UIN("d3d10_1.dll", D3D10StateBlockMaskUnion));
    D3D10_STATE_BLOCK_MASK a, b, r;
    memset(&a, 0x0f, sizeof a); memset(&b, 0xf0, sizeof b);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskUnion)(&a, &b, &r), S_OK);
    EXPECT_EQ(popcount_mask(&r), MASKSZ * 8);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskIntersect)(&a, &b, &r), S_OK);
    EXPECT_EQ(popcount_mask(&r), 0);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskDifference)(&a, &b, &r), S_OK);
    EXPECT_EQ(popcount_mask(&r), MASKSZ * 8);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskDisableAll)(&r), S_OK);
    EXPECT_EQ(popcount_mask(&r), 0);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskEnableCapture)(&r, D3D10_DST_PS, 0, 1), S_OK);
    EXPECT_EQ(UIN("d3d10_1.dll", D3D10StateBlockMaskGetSetting)(&r, D3D10_DST_PS, 0), 1);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskDisableCapture)(&r, D3D10_DST_PS, 0, 1), S_OK);
    EXPECT_EQ(popcount_mask(&r), 0);
}
TEST(d3d10_utils_effects_mask_d3d10_1_error_codes)
{
    REQUIRE(UIN("d3d10_1.dll", D3D10StateBlockMaskUnion));
    D3D10_STATE_BLOCK_MASK a;
    memset(&a, 0, sizeof a);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskUnion)(NULL, &a, &a), E_INVALIDARG_);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskDisableAll)(NULL), E_INVALIDARG_);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskEnableCapture)(&a, (D3D10_DEVICE_STATE_TYPES)0, 0, 1), E_INVALIDARG_);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10StateBlockMaskDisableCapture)(&a, D3D10_DST_VS, 1, 1), E_INVALIDARG_);
    EXPECT_EQ(UIN("d3d10_1.dll", D3D10StateBlockMaskGetSetting)(NULL, D3D10_DST_VS, 0), 0);
}
