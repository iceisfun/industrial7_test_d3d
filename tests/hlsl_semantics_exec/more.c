/* Additional intrinsic / system-value / compile-flag semantics measured on Win10 (WARP). */
#include "hse.h"

#define I F_INF
#define NI F_NINF
#define NaN F_NAN

/* ---- scalar intrinsics -------------------------------------------------------------------------------------- */
HSE_T(more_radians, "", "radians(fa)", HSE_ULP, 2,
      { FB(180.0f), 0, 0, FB(3.14159265f) }, { 0, 0, 0, 0 }, { FB(90.0f), 0, 0, FB(1.57079633f) }, { FB(-360.0f), 0, 0, FB(-6.28318531f) })
HSE_T(more_degrees, "", "degrees(fa)", HSE_ULP, 2,
      { FB(3.14159265f), 0, 0, FB(180.0f) }, { 0, 0, 0, 0 }, { FB(1.57079633f), 0, 0, FB(90.0f) }, { FB(-6.28318531f), 0, 0, FB(-360.0f) })
HSE_T(more_sign_float_to_int, "", "(uint)(sign(fa) + 1)", HSE_EXACT, 0,
      { FB(3.0f), 0, 0, 2 }, { FB(-2.0f), 0, 0, 0 }, { 0, 0, 0, 1 }, { F_NZERO, 0, 0, 1 }, { I, 0, 0, 2 }, { NI, 0, 0, 0 })
HSE_T(more_sign_int, "", "(uint)(sign(ia) + 1)", HSE_EXACT, 0, { 5, 0, 0, 2 }, { (uint32_t)-5, 0, 0, 0 }, { 0, 0, 0, 1 }, { 0x80000000u, 0, 0, 0 }, { 0x7fffffffu, 0, 0, 2 })
HSE_T(more_clamp_float, "", "clamp(fa, fb, fc)", HSE_FLT, 0,
      { FB(5.0f), FB(0.0f), FB(1.0f), FB(1.0f) }, { FB(-5.0f), FB(0.0f), FB(1.0f), 0 }, { FB(0.5f), FB(0.0f), FB(1.0f), FB(0.5f) }, { I, FB(-1.0f), FB(1.0f), FB(1.0f) }, { NI, FB(-1.0f), FB(1.0f), FB(-1.0f) })
HSE_T(more_clamp_int, "", "(uint)clamp(ia, ib, ic)", HSE_EXACT, 0,
      { 50, 0, 10, 10 }, { (uint32_t)-50, (uint32_t)-10, 10, (uint32_t)-10 }, { 3, 0, 10, 3 }, { 0x7fffffffu, 0, 100, 100 })
HSE_T(more_clamp_uint, "", "clamp(ua, ub, uc)", HSE_EXACT, 0, { 0xffffffffu, 0, 10, 10 }, { 3, 5, 10, 5 }, { 7, 5, 10, 7 })
HSE_T(more_modf_fraction, "float ip; float fr = modf(fa, ip);", "fr", HSE_FLT, 0,
      { FB(2.5f), 0, 0, FB(0.5f) }, { FB(-2.5f), 0, 0, FB(-0.5f) }, { FB(3.0f), 0, 0, 0 }, { 0, 0, 0, 0 }, { FB(0.25f), 0, 0, FB(0.25f) })
HSE_T(more_modf_integer_part, "float ip; float fr = modf(fa, ip);", "ip", HSE_FLT, 0,
      { FB(2.5f), 0, 0, FB(2.0f) }, { FB(-2.5f), 0, 0, FB(-2.0f) }, { FB(3.0f), 0, 0, FB(3.0f) }, { FB(0.25f), 0, 0, 0 })
HSE_T(more_atan2_axes, "", "atan2(fa, fb)", HSE_ULP, 4000,
      { 0, FB(1.0f), 0, 0 }, { 0, FB(-1.0f), 0, FB(3.14159265f) }, { FB(1.0f), 0, 0, FB(1.57079633f) }, { FB(-1.0f), 0, 0, FB(-1.57079633f) },
      { FB(1.0f), FB(1.0f), 0, FB(0.78539816f) }, { FB(-1.0f), FB(-1.0f), 0, FB(-2.35619449f) })
HSE_T(more_pow_special_exponents, "", "pow(fa, fb)", HSE_ULP, 16,
      { FB(2.0f), FB(10.0f), 0, FB(1024.0f) }, { FB(5.0f), 0, 0, FB(1.0f) }, { FB(4.0f), FB(0.5f), 0, FB(2.0f) }, { FB(10.0f), FB(-2.0f), 0, FB(0.01f) },
      { FB(1.0f), FB(100.0f), 0, FB(1.0f) }, { FB(2.0f), FB(-1.0f), 0, FB(0.5f) })
HSE_T(more_exp2_overflow_underflow, "", "exp2(fa)", HSE_FLT, 0, { FB(200.0f), 0, 0, I }, { FB(-200.0f), 0, 0, 0 }, { I, 0, 0, I }, { NI, 0, 0, 0 }, { NaN, 0, 0, NaN })
HSE_T(more_log2_specials, "", "log2(fa)", HSE_FLT, 0, { 0, 0, 0, NI }, { FB(-1.0f), 0, 0, NaN }, { I, 0, 0, I }, { FB(1.0f), 0, 0, 0 }, { NaN, 0, 0, NaN })
HSE_T(more_rcp_exact, "", "rcp(fa)", HSE_ULP, 1, { FB(2.0f), 0, 0, FB(0.5f) }, { FB(-4.0f), 0, 0, FB(-0.25f) }, { FB(0.125f), 0, 0, FB(8.0f) })
HSE_T(more_dot2_dot4, "", "dot(float2(fa, fb), float2(fc, 2.0)) + dot(float4(fa, fb, fc, 1.0), float4(1.0, 1.0, 1.0, 1.0))", HSE_ULP, 2,
      { FB(1.0f), FB(2.0f), FB(3.0f), FB(1.0f * 3.0f + 2.0f * 2.0f + (1.0f + 2.0f + 3.0f + 1.0f)) })
HSE_T(more_reflect_refract_faceforward, "float3 i = float3(fa, fb, 0.0); float3 n = float3(0.0, 1.0, 0.0); float3 r = reflect(i, n);", "r.x", HSE_FLT, 0, { FB(1.0f), FB(-1.0f), 0, FB(1.0f) })
HSE_T(more_reflect_y, "float3 i = float3(fa, fb, 0.0); float3 n = float3(0.0, 1.0, 0.0); float3 r = reflect(i, n);", "r.y", HSE_FLT, 0, { FB(1.0f), FB(-1.0f), 0, FB(1.0f) })
HSE_T(more_faceforward, "float3 r = faceforward(float3(0.0, fa, 0.0), float3(0.0, fb, 0.0), float3(0.0, fc, 0.0));", "r.y", HSE_FLT, 0,
      { FB(1.0f), FB(1.0f), FB(1.0f), FB(-1.0f) }, { FB(1.0f), FB(-1.0f), FB(1.0f), FB(1.0f) }, { FB(1.0f), FB(1.0f), FB(-1.0f), FB(1.0f) })

/* ---- integer / bit intrinsics ---------------------------------------------------------------------------------- */
static uint32_t ref_msad_lane(uint32_t refw, uint64_t src, int lane, uint32_t acc) {
    uint32_t sum = 0;
    for (int j = 0; j < 4; j++) {
        uint32_t rb = (refw >> (8 * j)) & 0xff, sb = (uint32_t)(src >> (8 * (lane + j))) & 0xff;
        if (rb) sum += rb > sb ? rb - sb : sb - rb;
    }
    return acc + sum;
}
TEST(hlsl_semantics_exec_more_msad4_lanes) {
    HSE_NEED();
    static const struct { uint32_t ref; uint32_t s0, s1; uint32_t acc; } c[] = {
        { 0x01020304u, 0x05060708u, 0x090a0b0cu, 0 }, { 0x00ff00ffu, 0x10203040u, 0x50607080u, 7 }, { 0u, 0x11111111u, 0x22222222u, 3 }, { 0xffffffffu, 0u, 0xffffffffu, 100 } };
    hse_row r[4 * 4];
    for (int k = 0; k < 4; k++) for (int lane = 0; lane < 4; lane++) {
        uint64_t src = c[k].s0 | ((uint64_t)c[k].s1 << 32);
        r[k * 4 + lane].a = c[k].ref; r[k * 4 + lane].b = c[k].s0; r[k * 4 + lane].c = c[k].s1;
        r[k * 4 + lane].want = ref_msad_lane(c[k].ref, src, lane, c[k].acc);
    }
    /* one row per (case, lane): lane and accumulator are baked into the prelude through the row index */
    for (int k = 0; k < 4; k++) for (int lane = 0; lane < 4; lane++) {
        char pre[256]; snprintf(pre, sizeof pre, "uint4 m = msad4(ua, uint2(ub, uc), uint4(%u, %u, %u, %u));", c[k].acc, c[k].acc, c[k].acc, c[k].acc);
        char ex[32]; snprintf(ex, sizeof ex, "m[%d]", lane);
        HSE_ROWS(pre, ex, &r[k * 4 + lane], 1, HSE_EXACT, 0);
    }
}
HSE_T(more_countbits_pattern, "", "countbits(ua)", HSE_EXACT, 0, { 0, 0, 0, 0 }, { 0xffffffffu, 0, 0, 32 }, { 0x80000001u, 0, 0, 2 }, { 0x0f0f0f0fu, 0, 0, 16 })
HSE_T(more_firstbitlow_zero_is_minus1, "", "firstbitlow(ua)", HSE_EXACT, 0, { 0, 0, 0, 0xffffffffu }, { 1, 0, 0, 0 }, { 0x80000000u, 0, 0, 31 }, { 0x00f00000u, 0, 0, 20 })
HSE_T(more_firstbithigh_signed, "", "(uint)firstbithigh(ia)", HSE_EXACT, 0, { 0, 0, 0, 0xffffffffu }, { (uint32_t)-1, 0, 0, 0xffffffffu }, { 1, 0, 0, 0 }, { 0x7fffffffu, 0, 0, 30 }, { 0x80000000u, 0, 0, 30 }, { (uint32_t)-2, 0, 0, 0 })
HSE_T(more_reversebits_involution, "", "reversebits(reversebits(ua)) == ua ? 1u : 0u", HSE_EXACT, 0, { 0x12345678u, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0xffffffffu, 0, 0, 1 })
HSE_T(more_int_mod_div_signs, "", "(uint)(ia % ib)", HSE_EXACT, 0, { 7, 3, 0, 1 }, { (uint32_t)-7, 3, 0, (uint32_t)-1 }, { 7, (uint32_t)-3, 0, 1 }, { (uint32_t)-7, (uint32_t)-3, 0, (uint32_t)-1 })
HSE_T(more_int_div_truncates, "", "(uint)(ia / ib)", HSE_EXACT, 0, { 7, 2, 0, 3 }, { (uint32_t)-7, 2, 0, (uint32_t)-3 }, { 7, (uint32_t)-2, 0, (uint32_t)-3 }, { 100, 7, 0, 14 })
HSE_T(more_shift_amount_masked_to_5_bits, "", "(ua << (ub & 31)) | (ub >= 32 ? 0u : 0u)", HSE_EXACT, 0, { 1, 33, 0, 2 }, { 1, 31, 0, 0x80000000u }, { 3, 32, 0, 3 })
HSE_T(more_shift_right_signed_arithmetic, "", "(uint)(ia >> (ub & 31))", HSE_EXACT, 0, { 0x80000000u, 31, 0, 0xffffffffu }, { 0x40000000u, 30, 0, 1 }, { (uint32_t)-8, 1, 0, (uint32_t)-4 })

/* ---- compile flags: optimisation level must not change results ------------------------------------------------- */
HSE_TF(more_skip_optimization_same_arith, D3DCOMPILE_SKIP_OPTIMIZATION, "float r = fa * fb + fc;", "r", HSE_FLT, 0, { FB(1.5f), FB(2.0f), FB(0.25f), FB(3.25f) }, { FB(-3.0f), FB(0.5f), FB(1.0f), FB(-0.5f) })
HSE_TF(more_opt_level3_same_arith, (1 << 15), "float r = fa * fb + fc;", "r", HSE_FLT, 0, { FB(1.5f), FB(2.0f), FB(0.25f), FB(3.25f) }, { FB(-3.0f), FB(0.5f), FB(1.0f), FB(-0.5f) })
HSE_TF(more_precise_keyword_not_fused, HSE_NO_IEEE, "precise float r = fa * fb + fc;", "r", HSE_EXACT, 0, { 0x3f800800u, 0x3f800800u, 0xbf801000u, 0 })
HSE_TF(more_no_ieee_mad_is_unfused_on_warp, HSE_NO_IEEE, "", "fa * fb + fc", HSE_WARPHARD, 0, { 0x3f800800u, 0x3f800800u, 0xbf801000u, 0 })
HSE_TF(more_mad_intrinsic_on_warp, HSE_NO_IEEE, "", "mad(fa, fb, fc)", HSE_WARPHARD, 0, { 0x3f800800u, 0x3f800800u, 0xbf801000u, 0 })

/* ---- pixel stage system values ----------------------------------------------------------------------------------- */
static float pxf(const d3dh_image *im, UINT x, UINT y, int ch) { return ((const float *)(im->data + y * im->pitch))[x * 4 + ch]; }
TEST(hlsl_semantics_exec_more_ps_primitive_id_zero_for_first_triangle) {
    HSE_NEED();
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 pos : SV_Position, uint pid : SV_PrimitiveID) : SV_Target { return float4(pid, 7, 0, 0); }";
    d3dh_image im = { 0 };
    if (HSE_DRAW(&p, &im)) { for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) { EXPECT_EQ((int)pxf(&im, x, y, 0), 0); EXPECT_EQ((int)pxf(&im, x, y, 1), 7); } d3dh_image_free(&im); }
}
TEST(hlsl_semantics_exec_more_ps_coverage_input_full_for_1x) {
    HSE_NEED();
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 pos : SV_Position, uint cov : SV_Coverage) : SV_Target { return float4(cov, 0, 0, 0); }";
    d3dh_image im = { 0 };
    if (HSE_DRAW(&p, &im)) { for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) EXPECT_EQ((int)pxf(&im, x, y, 0), 1); d3dh_image_free(&im); }
}
TEST(hlsl_semantics_exec_more_ps_position_pixel_centers) {
    HSE_NEED();
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 pos : SV_Position) : SV_Target { return float4(pos.x, pos.y, pos.z, pos.w); }";
    d3dh_image im = { 0 };
    if (HSE_DRAW(&p, &im)) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) {
            EXPECT_EQ(FB(pxf(&im, x, y, 0)), FB(x + 0.5f)); EXPECT_EQ(FB(pxf(&im, x, y, 1)), FB(y + 0.5f));
            CHECKF(pxf(&im, x, y, 2) == 0.0f, "z %g", pxf(&im, x, y, 2)); CHECKF(pxf(&im, x, y, 3) == 1.0f, "1/w %g", pxf(&im, x, y, 3));
        }
        d3dh_image_free(&im);
    }
}
TEST(hlsl_semantics_exec_more_ps_fwidth_is_abs_dx_plus_abs_dy) {
    HSE_NEED();
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 pos : SV_Position) : SV_Target { float v = pos.x * 2.0 - pos.y * 3.0; return float4(fwidth(v), abs(ddx(v)) + abs(ddy(v)), 0, 0); }";
    d3dh_image im = { 0 };
    if (HSE_DRAW(&p, &im)) { for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) { EXPECT_EQ(FB(pxf(&im, x, y, 0)), FB(5.0f)); EXPECT_EQ(FB(pxf(&im, x, y, 1)), FB(5.0f)); } d3dh_image_free(&im); }
}
TEST(hlsl_semantics_exec_more_vs_instance_id_zero_without_instancing) {
    HSE_NEED();
    hse_ps p; memset(&p, 0, sizeof p);
    p.vs = "struct O { float4 p : SV_Position; nointerpolation uint iid : IID; };\n"
           "O main(uint id : SV_VertexID, uint iid : SV_InstanceID) { O o; float2 q = float2((id << 1) & 2, id & 2); o.p = float4(q * 2.0 - 1.0, 0.0, 1.0); o.iid = iid; return o; }";
    p.ps = "float4 main(float4 pos : SV_Position, nointerpolation uint iid : IID) : SV_Target { return float4(iid, 1, 0, 0); }";
    d3dh_image im = { 0 };
    if (HSE_DRAW(&p, &im)) { for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) { EXPECT_EQ((int)pxf(&im, x, y, 0), 0); EXPECT_EQ((int)pxf(&im, x, y, 1), 1); } d3dh_image_free(&im); }
}
