/* Additional instruction-semantics kernels: constant divisors, vector intrinsics, legacy helpers, matrix intrinsics, minimum precision. */
#include "hse.h"

#define POP(x) ((uint32_t)__builtin_popcount(x))
static uint32_t x_rev(uint32_t x) { uint32_t r = 0; for (int i = 0; i < 32; i++) if (x & (1u << i)) r |= 1u << (31 - i); return r; }
static uint32_t x_fbh(uint32_t x) { return x ? 31u - (uint32_t)__builtin_clz(x) : 0xffffffffu; }
static uint32_t x_fbl(uint32_t x) { return x ? (uint32_t)__builtin_ctz(x) : 0xffffffffu; }

/* ---- constant divisors take a different code path than variable ones ------------------------------- */
HSE_P(x_udiv_const_pow2, "", "ua / 8u", HSE_EXACT, 0, a / 8u)
HSE_P(x_umod_const_pow2, "", "ua % 16u", HSE_EXACT, 0, a % 16u)
HSE_P(x_udiv_const_3, "", "ua / 3u", HSE_EXACT, 0, a / 3u)
HSE_P(x_umod_const_10, "", "ua % 10u", HSE_EXACT, 0, a % 10u)
HSE_P(x_udiv_const_large, "", "ua / 0x80000001u", HSE_EXACT, 0, a / 0x80000001u)
HSE_P(x_idiv_const_pow2_truncates_to_zero, "", "ia / 4", HSE_EXACT, 0, (uint32_t)((int32_t)a / 4))
HSE_P(x_imod_const_pow2_sign_of_dividend, "", "ia % 4", HSE_EXACT, 0, (uint32_t)((int32_t)a % 4))
HSE_P(x_idiv_const_negative, "", "ia / -3", HSE_EXACT, 0, (uint32_t)((int32_t)a / -3))
HSE_P(x_imod_const_7, "", "ia % 7", HSE_EXACT, 0, (uint32_t)((int32_t)a % 7))
HSE_T(x_idiv_const_known, "", "ia / 4", HSE_EXACT, 0,
      { (uint32_t)-1, 0, 0, 0 }, { (uint32_t)-3, 0, 0, 0 }, { (uint32_t)-4, 0, 0, (uint32_t)-1 }, { (uint32_t)-5, 0, 0, (uint32_t)-1 }, { 0x80000000u, 0, 0, 0xe0000000u }, { 7, 0, 0, 1 })

/* ---- vector forms of the bit intrinsics ---------------------------------------------------------------- */
HSE_P(x_countbits_uint4, "uint4 v = countbits(uint4(ua, ub, uc, ua ^ ub));", "v.x + v.y * 100u + v.z * 10000u + v.w * 1000000u", HSE_EXACT, 0,
      POP(a) + POP(b) * 100u + POP(c) * 10000u + POP(a ^ b) * 1000000u)
HSE_P(x_firstbithigh_uint2, "uint2 v = firstbithigh(uint2(ua, ub));", "v.x ^ (v.y * 7u)", HSE_EXACT, 0, x_fbh(a) ^ (x_fbh(b) * 7u))
HSE_P(x_firstbitlow_uint2, "uint2 v = firstbitlow(uint2(ua, ub));", "v.x ^ (v.y * 7u)", HSE_EXACT, 0, x_fbl(a) ^ (x_fbl(b) * 7u))
HSE_P(x_reversebits_uint3, "uint3 v = reversebits(uint3(ua, ub, uc));", "v.x ^ (v.y * 3u) ^ (v.z * 5u)", HSE_EXACT, 0, x_rev(a) ^ (x_rev(b) * 3u) ^ (x_rev(c) * 5u))
HSE_P(x_asuint_asint_identity, "", "asuint(asint(ua)) ^ asuint(ib)", HSE_EXACT, 0, a ^ b)
HSE_P(x_asfloat_roundtrip_bits_observed, "", "asuint(asfloat(ua))", HSE_WARPHARD, 0, a)
HSE_P(x_bool_to_int_is_zero_or_one, "", "(uint)(ua < ub) + (uint)(ua == ub) * 2u + (uint)(int)(ua > ub) * 4u", HSE_EXACT, 0,
      (a < b ? 1u : 0u) + (a == b ? 2u : 0u) + (a > b ? 4u : 0u))
HSE_P(x_bool_to_float, "", "(float)(ua < ub) + (float)(ua > ub) * 2.0", HSE_EXACT, 0, FB((a < b ? 1.0f : 0.0f) + (a > b ? 2.0f : 0.0f)))
HSE_P(x_int_mul_matrix_vector_wraps, "int2x2 m = int2x2(ia, ib, 2, 3); int2 r = mul(m, int2(ic, 1));", "asuint(r.x) * 31u + asuint(r.y)", HSE_EXACT, 0,
      (a * c + b) * 31u + (2u * c + 3u))
HSE_P(x_int_mul_vector_matrix_wraps, "int2x2 m = int2x2(ia, ib, 2, 3); int2 r = mul(int2(ic, 1), m);", "asuint(r.x) * 31u + asuint(r.y)", HSE_EXACT, 0,
      (c * a + 2u) * 31u + (c * b + 3u))
HSE_P(x_uint_dot_wraps, "", "dot(uint3(ua, ub, uc), uint3(1, 2, 3))", HSE_EXACT, 0, a + 2u * b + 3u * c)

/* ---- half conversions on vectors ------------------------------------------------------------------------ */
HSE_T(x_f32tof16_float2, "uint2 h = f32tof16(float2(fa, fb));", "(h.y << 16) | h.x", HSE_EXACT, 0,
      { FB(1.0f), FB(2.0f), 0, 0x40003c00u }, { FB(-1.0f), FB(0.5f), 0, 0x3800bc00u }, { 0, FB(65504.0f), 0, 0x7bff0000u })
HSE_T(x_f16tof32_uint2, "float2 v = f16tof32(uint2(ua, ub));", "v.x + v.y * 16.0", HSE_ULP, 0,
      { 0x3c00, 0x4000, 0, FB(33.0f) }, { 0xc000, 0x3800, 0, FB(-2.0f + 8.0f) }, { 0, 0, 0, 0 })

/* ---- vector / matrix intrinsics with exactly representable results ------------------------------------------- */
HSE_T(x_dst_legacy, "float4 d = dst(float4(1.0, fa, fb, 4.0), float4(5.0, fc, 7.0, 8.0));", "d.y + d.z * 4.0 + d.w * 16.0 + d.x * 64.0", HSE_ULP, 1,
      { FB(2.0f), FB(3.0f), FB(5.0f), FB(214.0f) }, { FB(1.0f), FB(1.0f), FB(1.0f), FB(1.0f + 4.0f + 128.0f + 64.0f) })
HSE_T(x_lit_diffuse_and_specular, "float4 l = lit(fa, fb, fc);", "l.y + l.z * 16.0 + l.x * 256.0 + l.w * 4096.0", HSE_ULP, 2,
      { FB(0.5f), FB(0.25f), FB(1.0f), FB(0.5f + 4.0f + 256.0f + 4096.0f) },
      { FB(-1.0f), FB(0.5f), FB(2.0f), FB(4352.0f) },
      { FB(0.5f), FB(-0.5f), FB(2.0f), FB(4352.5f) })
HSE_T(x_reflect_axis, "float3 r = reflect(float3(fa, fb, 0.0), float3(0.0, 1.0, 0.0));", "r.x * 1000.0 + r.y", HSE_ULP, 1,
      { FB(1.0f), FB(-1.0f), 0, FB(1001.0f) }, { FB(3.0f), FB(2.0f), 0, FB(2998.0f) }, { 0, FB(5.0f), 0, FB(-5.0f) })
HSE_T(x_refract_normal_incidence_unchanged, "float3 r = refract(float3(0.0, -1.0, 0.0), float3(0.0, 1.0, 0.0), fa);", "r.y", HSE_ULP, 1,
      { FB(0.5f), 0, 0, FB(-1.0f) }, { FB(1.0f), 0, 0, FB(-1.0f) }, { FB(2.0f), 0, 0, FB(-1.0f) })
HSE_T(x_refract_total_internal_reflection_is_zero, "float3 r = refract(normalize(float3(fa, -fb, 0.0)), float3(0.0, 1.0, 0.0), 2.0);", "r.x + r.y", HSE_FLT, 0,
      { FB(0.8f), FB(0.6f), 0, 0 }, { FB(1.0f), FB(0.1f), 0, 0 })
HSE_T(x_faceforward_sign, "float3 r = faceforward(float3(0.0, fa, 0.0), float3(0.0, fb, 0.0), float3(0.0, 1.0, 0.0));", "r.y", HSE_FLT, 0,
      { FB(2.0f), FB(-3.0f), 0, FB(2.0f) }, { FB(2.0f), FB(3.0f), 0, FB(-2.0f) }, { FB(-4.0f), FB(-1.0f), 0, FB(-4.0f) })
HSE_T(x_determinant_2x2, "float2x2 m = float2x2(fa, fb, fc, 2.0);", "determinant(m)", HSE_ULP, 1,
      { FB(3.0f), FB(4.0f), FB(5.0f), FB(-14.0f) }, { FB(1.0f), 0, 0, FB(2.0f) }, { 0, FB(1.0f), FB(1.0f), FB(-1.0f) })
HSE_T(x_determinant_3x3_diagonal, "float3x3 m = float3x3(fa, 0, 0, 0, fb, 0, 0, 0, fc);", "determinant(m)", HSE_ULP, 1,
      { FB(2.0f), FB(3.0f), FB(4.0f), FB(24.0f) }, { FB(1.0f), FB(-1.0f), FB(1.0f), FB(-1.0f) }, { FB(5.0f), 0, FB(2.0f), 0 })
HSE_T(x_transpose_indices, "float2x3 m = float2x3(fa, fb, fc, 4.0, 5.0, 6.0); float3x2 t = transpose(m);", "t[0][1] * 100.0 + t[1][0] * 10.0 + t[2][1]", HSE_ULP, 1,
      { FB(1.0f), FB(2.0f), FB(3.0f), FB(426.0f) }, { 0, 0, 0, FB(406.0f) })
HSE_T(x_mul_float2x3_float3x2, "float2x3 A = float2x3(fa, fb, fc, 1.0, 2.0, 3.0); float3x2 B = float3x2(1, 0, 0, 1, 1, 1); float2x2 C = mul(A, B);",
      "C[0][0] * 1000.0 + C[0][1] * 100.0 + C[1][0] * 10.0 + C[1][1]", HSE_ULP, 1,
      { FB(1.0f), FB(2.0f), FB(3.0f), FB(4545.0f) })
HSE_T(x_saturate_vector_lanes, "float3 s = saturate(float3(fa, fb, fc));", "s.x * 100.0 + s.y * 10.0 + s.z", HSE_ULP, 1,
      { FB(2.0f), FB(-1.0f), FB(0.5f), FB(100.5f) }, { FB(0.25f), FB(5.0f), FB(-0.0f), FB(35.0f) })
HSE_T(x_sincos_zero, "float s, c; sincos(fa, s, c);", "s * 10.0 + c", HSE_ULP, 1, { 0, 0, 0, FB(1.0f) })
HSE_T(x_modf_splits_integer_fraction, "float ip; float fp = modf(fa, ip);", "ip * 10.0 + fp", HSE_ULP, 1,
      { FB(3.5f), 0, 0, FB(30.5f) }, { FB(-3.5f), 0, 0, FB(-30.5f) }, { FB(7.0f), 0, 0, FB(70.0f) }, { FB(0.25f), 0, 0, FB(0.25f) })
HSE_T(x_exp2_integers_observed, "", "exp2(fa)", HSE_WARPHARD, 0,
      { FB(3.0f), 0, 0, FB(8.0f) }, { FB(-2.0f), 0, 0, FB(0.25f) }, { FB(10.0f), 0, 0, FB(1024.0f) }, { FB(0.0f), 0, 0, FB(1.0f) })
HSE_T(x_log2_powers_of_two_observed, "", "log2(fa)", HSE_WARPHARD, 0,
      { FB(8.0f), 0, 0, FB(3.0f) }, { FB(0.25f), 0, 0, FB(-2.0f) }, { FB(1024.0f), 0, 0, FB(10.0f) }, { FB(1.0f), 0, 0, 0 })
HSE_T(x_log2_powers_of_two_within_ulp, "", "log2(fa)", HSE_ULP, 2,
      { FB(8.0f), 0, 0, FB(3.0f) }, { FB(0.25f), 0, 0, FB(-2.0f) }, { FB(1024.0f), 0, 0, FB(10.0f) })
HSE_T(x_exp2_integers_within_ulp, "", "exp2(fa)", HSE_ULP, 2,
      { FB(3.0f), 0, 0, FB(8.0f) }, { FB(-2.0f), 0, 0, FB(0.25f) }, { FB(10.0f), 0, 0, FB(1024.0f) })

/* ---- float -> int rounding helpers ----------------------------------------------------------------------------- */
HSE_T(x_round_pi_ni_via_ceil_floor, "", "(asuint(ceil(fa)) ^ asuint(floor(fa)))", HSE_EXACT, 0,
      { FB(2.0f), 0, 0, 0 }, { FB(2.5f), 0, 0, FB(3.0f) ^ FB(2.0f) }, { FB(-2.5f), 0, 0, FB(-2.0f) ^ FB(-3.0f) })
HSE_T(x_float_to_int_after_floor, "", "(int)floor(fa)", HSE_EXACT, 0,
      { FB(2.5f), 0, 0, 2 }, { FB(-2.5f), 0, 0, (uint32_t)-3 }, { FB(-0.0f), 0, 0, 0 }, { FB(1e10f), 0, 0, 0x7fffffffu })

/* ---- minimum precision types: results need at least 10 mantissa bits / 16 integer bits ------------------------------ */
HSE_TF(x_min16float_add_exact, HSE_NO_IEEE, "min16float x = (min16float)fa + (min16float)fb;", "(float)x", HSE_FLT, 0,
      { FB(1.5f), FB(2.25f), 0, FB(3.75f) }, { FB(-6.0f), FB(2.0f), 0, FB(-4.0f) }, { FB(0.5f), FB(0.25f), 0, FB(0.75f) })
HSE_TF(x_min16float_mul_exact, HSE_NO_IEEE, "min16float x = (min16float)fa * (min16float)fb;", "(float)x", HSE_FLT, 0,
      { FB(0.5f), FB(0.25f), 0, FB(0.125f) }, { FB(-3.0f), FB(4.0f), 0, FB(-12.0f) }, { FB(100.0f), FB(10.0f), 0, FB(1000.0f) })
HSE_TF(x_min16float_within_half_precision, HSE_NO_IEEE, "min16float x = (min16float)fa * (min16float)fb;", "(float)x", HSE_ULP, 20000,
      { FB(3.14159f), FB(2.0f), 0, FB(6.28318f) }, { FB(1.1f), FB(1.1f), 0, FB(1.21f) })
HSE_TF(x_min16int_add, HSE_NO_IEEE, "min16int x = (min16int)ia + (min16int)ib;", "asuint((int)x)", HSE_EXACT, 0,
      { 100, 23, 0, 123 }, { (uint32_t)-100, 40, 0, (uint32_t)-60 }, { 0, 0, 0, 0 })
HSE_TF(x_min16uint_mul, HSE_NO_IEEE, "min16uint x = (min16uint)ua * (min16uint)ub;", "(uint)x", HSE_EXACT, 0,
      { 100, 200, 0, 20000 }, { 255, 255, 0, 65025 }, { 7, 0, 0, 0 })
