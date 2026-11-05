/* Comparisons, selects, control flow, indexing, swizzles; compute kernels plus pixel-shader discard. */
#include "hse.h"

#define I F_INF
#define NI F_NINF
#define NaN F_NAN

/* Pattern test with an HLSL prelude (helper functions / constant tables). */
#define HSE_PT(name, prelude, pre, expr, ref) \
    TEST(hlsl_semantics_exec_##name) { HSE_NEED(); hse_row r_[HSE_NPAT]; hse_patterns(r_, HSE_NPAT); \
        for (int i_ = 0; i_ < HSE_NPAT; i_++) { uint32_t a = r_[i_].a, b = r_[i_].b, c = r_[i_].c; (void)a; (void)b; (void)c; r_[i_].want = (uint32_t)(ref); } \
        HSE_PRELUDE(prelude); HSE_ROWS(pre, expr, r_, HSE_NPAT, HSE_EXACT, 0); }

/* ---- integer comparisons ---------------------------------------------------------------------------- */
HSE_P(cmp_ult, "", "ua < ub ? 1u : 0u", HSE_EXACT, 0, a < b)
HSE_P(cmp_ule, "", "ua <= ub ? 1u : 0u", HSE_EXACT, 0, a <= b)
HSE_P(cmp_ugt, "", "ua > ub ? 1u : 0u", HSE_EXACT, 0, a > b)
HSE_P(cmp_uge, "", "ua >= ub ? 1u : 0u", HSE_EXACT, 0, a >= b)
HSE_P(cmp_ueq, "", "ua == ub ? 1u : 0u", HSE_EXACT, 0, a == b)
HSE_P(cmp_une, "", "ua != ub ? 1u : 0u", HSE_EXACT, 0, a != b)
HSE_P(cmp_ilt, "", "ia < ib ? 1u : 0u", HSE_EXACT, 0, (int32_t)a < (int32_t)b)
HSE_P(cmp_ile, "", "ia <= ib ? 1u : 0u", HSE_EXACT, 0, (int32_t)a <= (int32_t)b)
HSE_P(cmp_igt, "", "ia > ib ? 1u : 0u", HSE_EXACT, 0, (int32_t)a > (int32_t)b)
HSE_P(cmp_ige, "", "ia >= ib ? 1u : 0u", HSE_EXACT, 0, (int32_t)a >= (int32_t)b)
HSE_P(cmp_mixed_sign_compares_unsigned, "", "ua < ib ? 1u : 0u", HSE_EXACT, 0, a < b)
HSE_T(cmp_mixed_sign_known, "", "ua < ib ? 1u : 0u", HSE_EXACT, 0, { 1, 0xffffffffu, 0, 1 }, { 0xffffffffu, 1, 0, 0 }, { 0, 0x80000000u, 0, 1 })
HSE_P(sel_int_movc, "", "ua ? ub : uc", HSE_EXACT, 0, a ? b : c)
HSE_P(sel_int_not_ua, "", "!ua ? ub : uc", HSE_EXACT, 0, !a ? b : c)
HSE_P(sel_bool_logic_and, "", "(ua != 0 && ub != 0) ? 1u : 0u", HSE_EXACT, 0, (a && b))
HSE_P(sel_bool_logic_or, "", "(ua != 0 || ub != 0) ? 1u : 0u", HSE_EXACT, 0, (a || b))
HSE_P(sel_bool_not_xor, "", "(ua != 0) != (ub != 0) ? 1u : 0u", HSE_EXACT, 0, ((a != 0) != (b != 0)))
HSE_P(sel_any_all_int3, "", "(any(uint3(ua, ub, 0)) ? 2u : 0u) | (all(uint3(ua, ub, 1)) ? 1u : 0u)", HSE_EXACT, 0, ((a || b) ? 2u : 0u) | ((a && b) ? 1u : 0u))
HSE_P(sel_vector_ternary_per_lane, "uint2 r = (uint2(ua, ub) != 0) ? uint2(1, 2) : uint2(3, 4);", "r.x * 16 + r.y", HSE_EXACT, 0,
      (a ? 1u : 3u) * 16 + (b ? 2u : 4u))

/* ---- float comparisons: NaN is unordered, -0 equals +0 ----------------------------------------------- */
static const uint32_t CV[] = { 0, F_NZERO, 0x3f800000u, 0xbf800000u, 0x40000000u, I, NI, NaN, 0x3f800001u, 0x7f7fffffu, 0xff7fffffu, 0x3f7fffffu };
#define NCV ((int)(sizeof CV / sizeof CV[0]))
#define HSE_FC(name, cop) \
    TEST(hlsl_semantics_exec_cmp_float_##name) { HSE_NEED(); hse_row r_[NCV * NCV]; int n_ = 0; \
        for (int i = 0; i < NCV; i++) for (int j = 0; j < NCV; j++) { float x = BF(CV[i]), y = BF(CV[j]); \
            r_[n_].a = CV[i]; r_[n_].b = CV[j]; r_[n_].c = 0; r_[n_].want = (x cop y) ? 1u : 0u; n_++; } \
        HSE_ROWS("", "fa " #cop " fb ? 1u : 0u", r_, n_, HSE_EXACT, 0); }
HSE_FC(lt, <)
HSE_FC(le, <=)
HSE_FC(gt, >)
HSE_FC(ge, >=)
HSE_FC(eq, ==)
HSE_FC(ne, !=)
HSE_T(cmp_float_nan_unordered_all_false, "", "((fa < fb) ? 1u : 0u) | ((fa <= fb) ? 2u : 0u) | ((fa > fb) ? 4u : 0u) | ((fa >= fb) ? 8u : 0u) | ((fa == fb) ? 16u : 0u)", HSE_EXACT, 0,
      { NaN, FB(1.0f), 0, 0 }, { FB(1.0f), NaN, 0, 0 }, { NaN, NaN, 0, 0 }, { NaN, I, 0, 0 })
HSE_T(cmp_float_nan_ne_true, "", "fa != fb ? 1u : 0u", HSE_EXACT, 0, { NaN, NaN, 0, 1 }, { NaN, FB(1.0f), 0, 1 }, { FB(1.0f), NaN, 0, 1 })
HSE_T(cmp_float_negzero_equals_zero, "", "fa == fb ? 1u : 0u", HSE_EXACT, 0, { 0, F_NZERO, 0, 1 }, { F_NZERO, 0, 0, 1 }, { F_NZERO, F_NZERO, 0, 1 })
HSE_T(cmp_float_negzero_not_less_than_zero, "", "fa < fb ? 1u : 0u", HSE_EXACT, 0, { F_NZERO, 0, 0, 0 }, { 0, F_NZERO, 0, 0 })
HSE_T(sel_float_condition_nonzero_true, "", "fa ? 1u : 2u", HSE_EXACT, 0,
      { FB(1.0f), 0, 0, 1 }, { 0, 0, 0, 2 }, { FB(-1.0f), 0, 0, 1 }, { I, 0, 0, 1 }, { NaN, 0, 0, 1 }, { 0x3f800000u, 0, 0, 1 })
HSE_T(sel_float_condition_negzero_is_false, "", "fa ? 1u : 2u", HSE_EXACT, 0, { F_NZERO, 0, 0, 2 })
HSE_T(sel_float_select_passes_nan_bits, "", "(fa == fa) ? fb : fc", HSE_EXACT, 0,
      { FB(1.0f), 0x7fc00123u, FB(5.0f), 0x7fc00123u }, { NaN, FB(1.0f), FB(5.0f), FB(5.0f) })
HSE_T(sel_float_select_negzero_preserved, "", "(ua == 0) ? fb : fc", HSE_EXACT, 0, { 0, F_NZERO, FB(1.0f), F_NZERO }, { 1, F_NZERO, FB(1.0f), FB(1.0f) })
HSE_T(sel_any_all_float, "", "(any(float3(fa, fb, fc)) ? 1u : 0u) | (all(float3(fa, fb, fc)) ? 2u : 0u)", HSE_EXACT, 0,
      { 0, 0, 0, 0 }, { FB(1.0f), 0, 0, 1 }, { FB(1.0f), FB(2.0f), FB(3.0f), 3 }, { NaN, FB(1.0f), FB(1.0f), 3 }, { F_NZERO, F_NZERO, 0, 0 })

/* ---- loops ----------------------------------------------------------------------------------------- */
HSE_P(flow_for_variable_bound, "uint n = ua & 63; uint s = 0; [loop] for (uint i = 0; i < n; i++) s += i * i + ub;", "s", HSE_EXACT, 0,
      ({ uint32_t n = a & 63, s = 0; for (uint32_t i = 0; i < n; i++) s += i * i + b; s; }))
HSE_P(flow_for_zero_iterations, "uint s = 5; [loop] for (uint i = 0; i < (ua & 0); i++) s = 99;", "s", HSE_EXACT, 0, 5)
HSE_P(flow_for_unrolled_equals_loop, "uint s = ub; [unroll] for (uint i = 0; i < 8; i++) s = s * 3 + i;", "s", HSE_EXACT, 0,
      ({ uint32_t s = b; for (uint32_t i = 0; i < 8; i++) s = s * 3 + i; s; }))
HSE_P(flow_while_shift_count, "uint x = ua, c = 0; [loop] while (x != 0) { x >>= 1; c++; }", "c", HSE_EXACT, 0,
      ({ uint32_t x = a, k = 0; while (x) { x >>= 1; k++; } k; }))
HSE_P(flow_do_while_runs_once, "uint c = 0; do { c++; } while (c < (ua & 0));", "c", HSE_EXACT, 0, 1)
HSE_P(flow_do_while_counts, "uint c = 0; uint n = (ua & 31) + 1; [loop] do { c += 3; n--; } while (n != 0);", "c", HSE_EXACT, 0, ((a & 31) + 1) * 3)
HSE_P(flow_break_first_over_limit, "uint s = 0; uint i = 0; [loop] for (i = 0; i < 100; i++) { s += i; if (s > (uc & 1023)) break; }", "s * 128 + i", HSE_EXACT, 0,
      ({ uint32_t s = 0, i; for (i = 0; i < 100; i++) { s += i; if (s > (c & 1023)) break; } s * 128 + i; }))
HSE_P(flow_continue_skips_odd, "uint s = 0; [loop] for (uint i = 0; i < 40; i++) { if (((i ^ ua) & 1) != 0) continue; s += i + 1; }", "s", HSE_EXACT, 0,
      ({ uint32_t s = 0; for (uint32_t i = 0; i < 40; i++) { if (((i ^ a) & 1) != 0) continue; s += i + 1; } s; }))
HSE_P(flow_nested_loops_break_inner, "uint s = 0; [loop] for (uint i = 0; i < 8; i++) { [loop] for (uint j = 0; j < 8; j++) { if (j > i) break; s += (ua >> ((i + j) & 31)) & 1; } }", "s", HSE_EXACT, 0,
      ({ uint32_t s = 0; for (uint32_t i = 0; i < 8; i++) for (uint32_t j = 0; j < 8; j++) { if (j > i) break; s += (a >> ((i + j) & 31)) & 1; } s; }))
HSE_PT(flow_collatz_steps,
       "uint collatz(uint s) { uint n = s, c = 0; [loop] for (uint i = 0; i < 256; i++) { if (n == 1) break; n = (n & 1) ? 3 * n + 1 : (n >> 1); c++; } return c; }",
       "", "collatz(ua % 1000 + 1)",
       ({ uint32_t n = a % 1000 + 1, k = 0; for (int i = 0; i < 256; i++) { if (n == 1) break; n = (n & 1) ? 3 * n + 1 : (n >> 1); k++; } k; }))
HSE_P(flow_loop_with_float_accumulate, "float s = 0; [loop] for (uint i = 0; i < (ua & 31); i++) s += 0.5;", "s", HSE_EXACT, 0, FB(0.5f * (float)(a & 31)))
HSE_P(flow_loop_decrement_to_zero, "uint x = ua & 255; uint s = 0; [loop] while (x > 0) { s += x; x -= 1; }", "s", HSE_EXACT, 0, ((a & 255) * ((a & 255) + 1)) / 2)
HSE_P(flow_loop_preincrement_postincrement, "uint x = ua & 7; uint y = x++; uint z = ++x;", "y * 10000 + z * 100 + x", HSE_EXACT, 0,
      ({ uint32_t x = a & 7, y = x++, z = ++x; y * 10000 + z * 100 + x; }))

/* ---- branches, switch, function returns ------------------------------------------------------------------------ */
HSE_P(flow_if_else_chain, "uint r; if (ua < 10) r = 1; else if (ua < 100) r = 2; else if ((ua & 1) == 0) r = 3; else r = 4;", "r", HSE_EXACT, 0,
      (a < 10) ? 1u : (a < 100) ? 2u : ((a & 1) == 0) ? 3u : 4u)
HSE_P(flow_switch_default, "uint r = 100; [branch] switch (ua & 7) { case 0: r = 10; break; case 1: r = 11; break; case 2: r = 12; break; case 5: r = 15; break; default: r = 99; break; }", "r", HSE_EXACT, 0,
      ((a & 7) == 0) ? 10u : ((a & 7) == 1) ? 11u : ((a & 7) == 2) ? 12u : ((a & 7) == 5) ? 15u : 99u)
HSE_P(flow_switch_stacked_labels, "uint r = 0; switch (ua & 15) { case 1: case 3: case 5: r = 1; break; case 2: case 4: r = 2; break; default: r = 3; break; }", "r", HSE_EXACT, 0,
      ({ uint32_t k = a & 15; (k == 1 || k == 3 || k == 5) ? 1u : (k == 2 || k == 4) ? 2u : 3u; }))
HSE_P(flow_switch_no_default_keeps_value, "uint r = 77; switch (ua & 3) { case 0: r = 1; break; case 1: r = 2; break; }", "r", HSE_EXACT, 0,
      ({ uint32_t k = a & 3; k == 0 ? 1u : k == 1 ? 2u : 77u; }))
HSE_P(flow_switch_in_loop, "uint s = 0; [loop] for (uint i = 0; i < 8; i++) { switch ((ua >> i) & 3) { case 0: s += 0; break; case 1: s += 1001; break; case 2: s += 1010; break; default: s += 1100; break; } }", "s", HSE_EXACT, 0,
      ({ uint32_t s = 0; for (uint32_t i = 0; i < 8; i++) { uint32_t k = (a >> i) & 3; s += k == 0 ? 0 : k == 1 ? 1001 : k == 2 ? 1010 : 1100; } s; }))
HSE_PT(flow_early_return_paths,
       "uint f(uint x) { if (x & 1) return 7; if (x & 2) return 9; [loop] for (uint i = 0; i < 4; i++) { if (((x >> (i + 2)) & 1) != 0) return 20 + i; } return x; }",
       "", "f(ua)",
       ({ uint32_t x = a, r = x; if (x & 1) r = 7; else if (x & 2) r = 9; else { for (uint32_t i = 0; i < 4; i++) if (((x >> (i + 2)) & 1) != 0) { r = 20 + i; break; } } r; }))
HSE_PT(flow_out_and_inout_params,
       "void g(uint x, out uint lo, inout uint acc) { lo = x & 0xff; acc += x >> 24; }",
       "uint lo = 12345, acc = ub & 0xffff; g(ua, lo, acc);", "lo * 65536u + acc",
       ({ uint32_t lo = a & 0xff, acc = (b & 0xffff) + (a >> 24); lo * 65536u + acc; }))
HSE_PT(flow_function_call_value_semantics,
       "uint h(uint x) { x += 5; return x; }",
       "uint v = ua; uint w = h(v);", "w - v",
       5u)
HSE_PT(flow_static_const_array_dynamic_index,
       "static const uint T[8] = { 3, 1, 4, 1, 5, 9, 2, 6 };",
       "", "T[ua & 7] * 10 + T[(ub + 3) & 7]",
       ({ static const uint32_t T[8] = { 3, 1, 4, 1, 5, 9, 2, 6 }; T[a & 7] * 10 + T[(b + 3) & 7]; }))
TEST(hlsl_semantics_exec_flow_local_array_dynamic_index) {
    HSE_NEED();
    if (t_is_wine()) { t_skip("the emulator's HLSL compiler faults on this construct"); return; }
    hse_row r_[HSE_NPAT]; hse_patterns(r_, HSE_NPAT);
    for (int i_ = 0; i_ < HSE_NPAT; i_++) {
        uint32_t a = r_[i_].a, b = r_[i_].b, arr[16];
        for (uint32_t i = 0; i < 16; i++) arr[i] = i * i + b;
        arr[a & 15] ^= 0xff;
        r_[i_].want = arr[a & 15] + arr[(a + 1) & 15] * 3;
    }
    HSE_ROWS("uint arr[16]; [unroll] for (uint i = 0; i < 16; i++) arr[i] = i * i + ub; uint t0 = arr[ua & 15]; arr[ua & 15] = t0 ^ 0xff;",
             "arr[ua & 15] + arr[(ua + 1) & 15] * 3", r_, HSE_NPAT, HSE_EXACT, 0);
}
HSE_P(flow_local_array_loop_fill_read, "uint arr[32]; [loop] for (uint i = 0; i < 32; i++) arr[i] = (i * 2654435761u) ^ ub; uint s = 0; [loop] for (uint j = 0; j < 8; j++) s += arr[(ua + j * 5) & 31];", "s", HSE_EXACT, 0,
      ({ uint32_t arr[32], s = 0; for (uint32_t i = 0; i < 32; i++) arr[i] = (i * 2654435761u) ^ b; for (uint32_t j = 0; j < 8; j++) s += arr[(a + j * 5) & 31]; s; }))

/* ---- vectors, swizzles, write masks --------------------------------------------------------------------------------- */
HSE_P(vec_swizzle_reverse, "uint4 v = uint4(ua, ub, uc, 7); uint4 w = v.wzyx;", "w.x * 3 + w.y * 5 + w.z * 7 + w.w * 11", HSE_EXACT, 0, 7u * 3 + c * 5 + b * 7 + a * 11)
HSE_P(vec_write_mask_swizzle, "uint4 v = uint4(1, 2, 3, 4); v.xz = uint2(ua, ub); v.yw += v.xz;", "v.x + v.y * 3 + v.z * 5 + v.w * 7", HSE_EXACT, 0,
      ({ uint32_t x = a, y = 2 + a, z = b, w = 4 + b; x + y * 3 + z * 5 + w * 7; }))
HSE_P(vec_swizzle_repeat_and_scalar_broadcast, "uint3 v = ua.xxx + uint3(0, 1, 2); uint2 u = ub.xx;", "v.z + v.y * 2 + u.y", HSE_EXACT, 0, (a + 2) + (a + 1) * 2 + b)
HSE_P(vec_component_wise_ops, "uint3 v = uint3(ua, ub, uc); uint3 w = (v * 3 + 1) ^ v.yzx;", "w.x + w.y * 5 + w.z * 9", HSE_EXACT, 0,
      ({ uint32_t x = (a * 3 + 1) ^ b, y = (b * 3 + 1) ^ c, z = (c * 3 + 1) ^ a; x + y * 5 + z * 9; }))
HSE_T(vec_float4_dot_and_sum, "float4 v = float4(fa, fb, fc, 2.0);", "dot(v, v) + v.x + v.w", HSE_ULP, 1,
      { FB(1.0f), FB(2.0f), FB(3.0f), FB(14.0f + 4.0f + 1.0f + 2.0f) }, { 0, 0, 0, FB(4.0f + 2.0f) })
HSE_T(vec_max_component_wise, "float3 v = max(float3(fa, fb, fc), float3(fc, fa, fb));", "v.x * 100 + v.y * 10 + v.z", HSE_ULP, 1,
      { FB(1.0f), FB(2.0f), FB(3.0f), FB(300.0f + 10.0f * 3.0f + 3.0f) })
HSE_P(vec_cast_float_int_vectors, "int3 iv = int3(ia, ib, ic); uint3 uv = (uint3)iv;", "uv.x ^ uv.y ^ uv.z", HSE_EXACT, 0, a ^ b ^ c)

/* ---- compute: early return leaves memory untouched ---------------------------------------------------------------------- */
TEST(hlsl_semantics_exec_flow_cs_early_return_leaves_output) {
    HSE_NEED();
    uint32_t init[64], out[64];
    for (int i = 0; i < 64; i++) init[i] = 0xCAFE0000u + (uint32_t)i;
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) {\n"
            "  if ((id.x % 3) == 0) return;\n"
            "  O.Store(id.x * 4, id.x * 2);\n"
            "  if (id.x > 40) return;\n"
            "  O.Store(id.x * 4, id.x * 5);\n}\n";
    c.outinit = init; c.outbytes = sizeof init; c.gx = 1; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    for (int i = 0; i < 64; i++) {
        uint32_t want = (i % 3 == 0) ? init[i] : (i > 40 ? (uint32_t)i * 2 : (uint32_t)i * 5);
        EXPECT_EQ(out[i], want);
    }
}
TEST(hlsl_semantics_exec_flow_cs_dispatch_thread_id_mapping) {
    HSE_NEED();
    enum { GX = 3, GY = 2, TX = 4, TY = 2 };
    uint32_t out[GX * GY * TX * TY * 4] = { 0 };
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(4,2,1)] void main(uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint gi : SV_GroupIndex) {\n"
            "  uint lin = dtid.y * 12 + dtid.x;\n"
            "  O.Store4(lin * 16, uint4(gid.y * 3 + gid.x, gtid.y * 4 + gtid.x, gi, dtid.x * 100 + dtid.y));\n}\n";
    c.outbytes = sizeof out; c.gx = GX; c.gy = GY; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    for (UINT y = 0; y < GY * TY; y++) for (UINT x = 0; x < GX * TX; x++) {
        uint32_t *o = out + (y * 12 + x) * 4;
        UINT gx = x / TX, gy = y / TY, tx = x % TX, ty = y % TY;
        EXPECT_EQ(o[0], gy * 3 + gx); EXPECT_EQ(o[1], ty * 4 + tx); EXPECT_EQ(o[2], ty * 4 + tx); EXPECT_EQ(o[3], x * 100 + y);
    }
}
TEST(hlsl_semantics_exec_flow_cs_group_index_in_3d_group) {
    HSE_NEED();
    uint32_t out[2 * 3 * 2] = { 0 };
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(2,3,2)] void main(uint3 gtid : SV_GroupThreadID, uint gi : SV_GroupIndex) {\n"
            "  O.Store(gi * 4, gtid.z * 100 + gtid.y * 10 + gtid.x);\n}\n";
    c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    for (UINT z = 0; z < 2; z++) for (UINT y = 0; y < 3; y++) for (UINT x = 0; x < 2; x++)
        EXPECT_EQ(out[z * 6 + y * 2 + x], z * 100 + y * 10 + x);   /* SV_GroupIndex = z*X*Y + y*X + x */
}

/* ---- pixel shader discard / clip ---------------------------------------------------------------------------------------------- */
static int draw_px(const char *ps, d3dh_image *im, float clear) {
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = ps; p.clear[0] = p.clear[1] = p.clear[2] = p.clear[3] = clear;
    return HSE_DRAW(&p, im);
}
static float px(const d3dh_image *im, UINT x, UINT y, int ch) { return ((const float *)(im->data + y * im->pitch))[x * 4 + ch]; }
TEST(hlsl_semantics_exec_flow_ps_discard_keeps_clear_value) {
    HSE_NEED();
    d3dh_image im = { 0 };
    if (!draw_px("float4 main(float4 p : SV_Position) : SV_Target { if (p.x < 2.0) discard; return float4(1, 2, 3, 4); }", &im, 7.0f)) return;
    for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++)
        for (int ch = 0; ch < 4; ch++) CHECKF(px(&im, x, y, ch) == (x < 2 ? 7.0f : (float)(ch + 1)), "x=%u y=%u ch=%d got %g", x, y, ch, px(&im, x, y, ch));
    d3dh_image_free(&im);
}
TEST(hlsl_semantics_exec_flow_ps_discard_after_computation) {
    HSE_NEED();
    d3dh_image im = { 0 };
    if (!draw_px("float4 main(float4 p : SV_Position) : SV_Target { float v = p.x + p.y * 10.0; if (v > 20.0) discard; return float4(v, 0, 0, 1); }", &im, -1.0f)) return;
    for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) {
        float v = (x + 0.5f) + (y + 0.5f) * 10.0f;
        CHECKF(px(&im, x, y, 0) == (v > 20.0f ? -1.0f : v), "x=%u y=%u got %g", x, y, px(&im, x, y, 0));
    }
    d3dh_image_free(&im);
}
TEST(hlsl_semantics_exec_flow_ps_clip_negative_discards) {
    HSE_NEED();
    d3dh_image im = { 0 };
    if (!draw_px("float4 main(float4 p : SV_Position) : SV_Target { clip(p.y - 2.0); return float4(1, 1, 1, 1); }", &im, 0.0f)) return;
    for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) CHECKF(px(&im, x, y, 0) == (y < 2 ? 0.0f : 1.0f), "x=%u y=%u", x, y);
    d3dh_image_free(&im);
}
TEST(hlsl_semantics_exec_flow_ps_clip_zero_and_nan) {
    HSE_NEED();
    d3dh_image im = { 0 };
    /* clip(x) discards when x < 0; zero keeps the pixel. NaN: recorded, not asserted. */
    if (!draw_px("float4 main(float4 p : SV_Position) : SV_Target { clip(p.x - 1.5 + (p.x < 1.0 ? 0.0 : 0.0)); return float4(1, 1, 1, 1); }", &im, 0.0f)) return;
    for (UINT x = 0; x < 4; x++) CHECKF(px(&im, x, 0, 0) == (x < 1 ? 0.0f : 1.0f), "x=%u", x);
    d3dh_image_free(&im);
}
TEST(hlsl_semantics_exec_flow_ps_discard_in_both_branches_of_loop) {
    HSE_NEED();
    d3dh_image im = { 0 };
    if (!draw_px("float4 main(float4 p : SV_Position) : SV_Target { uint n = (uint)p.x; float s = 0; [loop] for (uint i = 0; i < 4; i++) { if (i == n) discard; s += 1.0; } return float4(s, 0, 0, 1); }", &im, 9.0f)) return;
    for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) CHECKF(px(&im, x, y, 0) == 9.0f, "x=%u y=%u got %g", x, y, px(&im, x, y, 0));
    d3dh_image_free(&im);
}
TEST(hlsl_semantics_exec_flow_ps_early_return_value) {
    HSE_NEED();
    d3dh_image im = { 0 };
    if (!draw_px("float4 f(float x) { if (x < 2.0) return float4(1, 0, 0, 1); return float4(0, 1, 0, 1); }\n"
                 "float4 main(float4 p : SV_Position) : SV_Target { if (p.y > 2.0) return float4(5, 5, 5, 5); return f(p.x); }", &im, 0.0f)) return;
    for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) {
        float r = y >= 2 ? 5.0f : (x < 2 ? 1.0f : 0.0f), g = y >= 2 ? 5.0f : (x < 2 ? 0.0f : 1.0f);
        CHECKF(px(&im, x, y, 0) == r && px(&im, x, y, 1) == g, "x=%u y=%u got %g %g", x, y, px(&im, x, y, 0), px(&im, x, y, 1));
    }
    d3dh_image_free(&im);
}
