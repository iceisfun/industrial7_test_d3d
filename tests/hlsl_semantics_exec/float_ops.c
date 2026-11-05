/* Float instruction semantics: IEEE-defined results and D3D special-case rules. */
#include "hse.h"
#include <float.h>

#define I F_INF
#define NI F_NINF
#define NaN F_NAN

/* Same table twice: values hard-checked ignoring the sign of zero, then the zero sign recorded with t_note. */
#define HSE_TZ(name, pre, expr, ...) \
    TEST(hlsl_semantics_exec_##name) { HSE_NEED(); hse_row r_[] = { __VA_ARGS__ }; int n_ = (int)(sizeof r_ / sizeof r_[0]); \
        HSE_ROWS(pre, expr, r_, n_, HSE_FLT, 0); HSE_ROWS(pre, expr, r_, n_, HSE_SIGNED0 | HSE_SOFT, 0); }

/* ---- arithmetic -------------------------------------------------------------------------------- */
HSE_TZ(float_add_basic, "", "fa + fb",
       { FB(1.5f), FB(2.25f), 0, FB(3.75f) }, { FB(1e30f), FB(1e30f), 0, FB(2e30f) }, { FB(-1.0f), FB(1.0f), 0, 0 },
       { FB(0.1f), FB(0.2f), 0, FB(0.1f + 0.2f) }, { FB(16777216.0f), FB(1.0f), 0, FB(16777216.0f) }, { FB(16777216.0f), FB(2.0f), 0, FB(16777218.0f) },
       { FB(3e38f), FB(3e38f), 0, I }, { FB(-3e38f), FB(-3e38f), 0, NI }, { 0, F_NZERO, 0, 0 }, { F_NZERO, F_NZERO, 0, F_NZERO })
HSE_TZ(float_add_specials, "", "fa + fb",
       { I, FB(1.0f), 0, I }, { NI, FB(1.0f), 0, NI }, { I, I, 0, I }, { NI, NI, 0, NI }, { I, NI, 0, NaN }, { NI, I, 0, NaN },
       { NaN, FB(1.0f), 0, NaN }, { FB(1.0f), NaN, 0, NaN }, { NaN, NaN, 0, NaN }, { NaN, I, 0, NaN })
HSE_TZ(float_sub_basic, "", "fa - fb",
       { FB(5.0f), FB(3.0f), 0, FB(2.0f) }, { FB(3.0f), FB(5.0f), 0, FB(-2.0f) }, { FB(1.0f), FB(1.0f), 0, 0 }, { I, I, 0, NaN }, { I, NI, 0, I },
       { NI, I, 0, NI }, { NaN, FB(1.0f), 0, NaN }, { 0, 0, 0, 0 }, { F_NZERO, 0, 0, F_NZERO })
HSE_TZ(float_mul_basic, "", "fa * fb",
       { FB(3.0f), FB(4.0f), 0, FB(12.0f) }, { FB(-3.0f), FB(4.0f), 0, FB(-12.0f) }, { FB(-3.0f), FB(-4.0f), 0, FB(12.0f) }, { FB(0.5f), FB(0.5f), 0, FB(0.25f) },
       { FB(1e20f), FB(1e20f), 0, I }, { FB(-1e20f), FB(1e20f), 0, NI }, { FB(1.5f), 0, 0, 0 }, { FB(-1.5f), 0, 0, F_NZERO },
       { FB(123456.0f), FB(0.0009765625f), 0, FB(123456.0f * 0.0009765625f) })
HSE_TZ(float_mul_specials, "", "fa * fb",
       { I, 0, 0, NaN }, { 0, I, 0, NaN }, { NI, 0, 0, NaN }, { I, FB(2.0f), 0, I }, { I, FB(-2.0f), 0, NI }, { I, NI, 0, NI },
       { NaN, FB(2.0f), 0, NaN }, { NaN, 0, 0, NaN }, { NaN, I, 0, NaN })
HSE_T(float_mad_exact_cases, "", "fa * fb + fc", HSE_FLT, 0,
      { FB(2.0f), FB(3.0f), FB(4.0f), FB(10.0f) }, { FB(-2.0f), FB(3.0f), FB(6.0f), 0 }, { FB(0.5f), FB(0.5f), FB(0.75f), FB(1.0f) },
      { I, FB(2.0f), FB(1.0f), I }, { I, 0, FB(1.0f), NaN }, { NaN, FB(1.0f), FB(1.0f), NaN }, { I, FB(1.0f), NI, NaN })
/* a*b is 1 + 2^-11 + 2^-24: an unfused mad rounds the product first (result 0), a fused one keeps the low bit (2^-24). */
HSE_T(float_mad_fused_or_not_observed, "", "fa * fb + fc", HSE_EXACT | HSE_SOFT, 0,
      { 0x3f800800u, 0x3f800800u, 0xbf801000u, 0 })
HSE_T(float_mad_fused_or_not_one_of_two, "float r = fa * fb + fc;", "(asuint(r) == 0u || asuint(r) == 0x33800000u) ? 1u : 0u", HSE_EXACT, 0,
      { 0x3f800800u, 0x3f800800u, 0xbf801000u, 1 })

/* ---- reciprocal / division ------------------------------------------------------------------------- */
HSE_T(float_rcp_specials, "", "rcp(fa)", HSE_ULP, 1,
      { 0, 0, 0, I }, { F_NZERO, 0, 0, NI }, { I, 0, 0, 0 }, { NI, 0, 0, F_NZERO }, { NaN, 0, 0, NaN },
      { FB(1.0f), 0, 0, FB(1.0f) }, { FB(2.0f), 0, 0, FB(0.5f) }, { FB(4.0f), 0, 0, FB(0.25f) }, { FB(-8.0f), 0, 0, FB(-0.125f) }, { FB(0.5f), 0, 0, FB(2.0f) })
HSE_T(float_rcp_inexact_within_1ulp, "", "rcp(fa)", HSE_ULP, 1,
      { FB(3.0f), 0, 0, FB(1.0f / 3.0f) }, { FB(7.0f), 0, 0, FB(1.0f / 7.0f) }, { FB(10.0f), 0, 0, FB(0.1f) }, { FB(-3.0f), 0, 0, FB(-1.0f / 3.0f) },
      { FB(1e10f), 0, 0, FB(1e-10f) }, { FB(0.1f), 0, 0, FB(1.0f / 0.1f) })
HSE_T(float_div_specials, "", "fa / fb", HSE_ULP, 2,
      { FB(1.0f), 0, 0, I }, { FB(-1.0f), 0, 0, NI }, { FB(1.0f), F_NZERO, 0, NI }, { 0, 0, 0, NaN }, { I, I, 0, NaN }, { 0, I, 0, 0 },
      { FB(1.0f), I, 0, 0 }, { I, FB(2.0f), 0, I }, { NaN, FB(1.0f), 0, NaN }, { FB(1.0f), NaN, 0, NaN },
      { FB(6.0f), FB(3.0f), 0, FB(2.0f) }, { FB(1.0f), FB(4.0f), 0, FB(0.25f) }, { FB(-9.0f), FB(3.0f), 0, FB(-3.0f) }, { FB(1.0f), FB(3.0f), 0, FB(1.0f / 3.0f) })

/* ---- min / max / abs / neg / saturate / sign ----------------------------------------------------- */
HSE_TZ(float_min, "", "min(fa, fb)",
       { FB(1.0f), FB(2.0f), 0, FB(1.0f) }, { FB(2.0f), FB(1.0f), 0, FB(1.0f) }, { FB(-1.0f), FB(1.0f), 0, FB(-1.0f) }, { NI, I, 0, NI },
       { NaN, FB(1.0f), 0, FB(1.0f) }, { FB(1.0f), NaN, 0, FB(1.0f) }, { NaN, NaN, 0, NaN }, { NaN, NI, 0, NI }, { I, NaN, 0, I }, { 0, F_NZERO, 0, F_NZERO })
HSE_TZ(float_max, "", "max(fa, fb)",
       { FB(1.0f), FB(2.0f), 0, FB(2.0f) }, { FB(2.0f), FB(1.0f), 0, FB(2.0f) }, { FB(-1.0f), FB(1.0f), 0, FB(1.0f) }, { NI, I, 0, I },
       { NaN, FB(1.0f), 0, FB(1.0f) }, { FB(1.0f), NaN, 0, FB(1.0f) }, { NaN, NaN, 0, NaN }, { NaN, I, 0, I }, { NI, NaN, 0, NI }, { 0, F_NZERO, 0, 0 })
HSE_T(float_abs_clears_sign_bit, "", "abs(fa)", HSE_SIGNED0, 0,
      { F_NZERO, 0, 0, 0 }, { FB(-1.5f), 0, 0, FB(1.5f) }, { FB(1.5f), 0, 0, FB(1.5f) }, { NI, 0, 0, I }, { I, 0, 0, I }, { FB(-3e38f), 0, 0, FB(3e38f) })
HSE_T(float_neg_zero_sign_observed, "", "-fa", HSE_SIGNED0 | HSE_SOFT, 0, { 0, 0, 0, F_NZERO }, { F_NZERO, 0, 0, 0 })
HSE_T(float_neg_flips_sign_bit, "", "-fa", HSE_SIGNED0, 0,
      { FB(1.5f), 0, 0, FB(-1.5f) }, { FB(-1.5f), 0, 0, FB(1.5f) }, { I, 0, 0, NI }, { NI, 0, 0, I })
HSE_T(float_abs_nan_is_nan, "", "isnan(abs(fa)) ? 1u : 0u", HSE_EXACT, 0, { NaN, 0, 0, 1 }, { 0xffc00000u, 0, 0, 1 }, { 0x7f800001u, 0, 0, 1 }, { I, 0, 0, 0 })
HSE_T(float_saturate, "", "saturate(fa)", HSE_FLT, 0,
      { FB(-1.0f), 0, 0, 0 }, { FB(0.5f), 0, 0, FB(0.5f) }, { FB(2.0f), 0, 0, FB(1.0f) }, { NaN, 0, 0, 0 }, { I, 0, 0, FB(1.0f) },
      { NI, 0, 0, 0 }, { F_NZERO, 0, 0, 0 }, { FB(1.0f), 0, 0, FB(1.0f) }, { FB(0.99999994f), 0, 0, FB(0.99999994f) }, { FB(1.0000001f), 0, 0, FB(1.0f) },
      { FB(-1e-30f), 0, 0, 0 }, { 0xffc00000u, 0, 0, 0 })
HSE_T(float_clamp, "", "clamp(fa, fb, fc)", HSE_FLT, 0,
      { FB(5.0f), FB(0.0f), FB(3.0f), FB(3.0f) }, { FB(-5.0f), FB(-2.0f), FB(3.0f), FB(-2.0f) }, { FB(1.0f), FB(-2.0f), FB(3.0f), FB(1.0f) }, { I, 0, FB(3.0f), FB(3.0f) },
      { NI, FB(-7.0f), FB(3.0f), FB(-7.0f) })
HSE_T(float_sign_returns_int, "", "sign(fa)", HSE_EXACT, 0,
      { FB(-3.5f), 0, 0, 0xffffffffu }, { FB(3.5f), 0, 0, 1 }, { 0, 0, 0, 0 }, { F_NZERO, 0, 0, 0 }, { I, 0, 0, 1 }, { NI, 0, 0, 0xffffffffu }, { FB(1e-30f), 0, 0, 1 })

/* ---- rounding family on a shared value set ------------------------------------------------------------- */
static const uint32_t FV[] = {
    0, F_NZERO, 0x3e800000u, 0x3f000000u, 0x3f400000u, 0x3f800000u, 0x3fa00000u, 0x3fc00000u, 0x40000000u, 0x40200000u, 0x40600000u,
    0xbe800000u, 0xbf000000u, 0xbf800000u, 0xbfc00000u, 0xc0200000u, 0xc0600000u, 0x3efffffeu, 0x3f000001u, 0x42c9999au, 0xc2c9999au,
    0x4b7fffffu, 0x4b800000u, 0xcb800000u, 0x501502f9u, 0xd01502f9u, 0x7f7e0000u, I, NI, NaN, 0x4b000001u, 0x3f800001u, 0x3f7fffffu, 0xbf7fffffu, 0xbf800001u,
    0x40a00000u, 0x40c00000u, 0x40e00000u, 0xc0a00000u, 0x3dcccccdu };
#define NFV ((int)(sizeof FV / sizeof FV[0]))
/* x: the input. Table is checked ignoring zero sign, and again with zero sign recorded. */
#define HSE_F1(name, pre, expr, ulps, refx) \
    TEST(hlsl_semantics_exec_##name) { HSE_NEED(); hse_row r_[NFV]; \
        for (int i_ = 0; i_ < NFV; i_++) { float x = BF(FV[i_]); r_[i_].a = FV[i_]; r_[i_].b = r_[i_].c = 0; r_[i_].want = FB(refx); } \
        HSE_ROWS(pre, expr, r_, NFV, ulps ? HSE_ULP : HSE_FLT, ulps); \
        HSE_ROWS(pre, expr, r_, NFV, HSE_SIGNED0 | HSE_SOFT, 0); }
HSE_F1(float_floor, "", "floor(fa)", 0, floorf(x))
HSE_F1(float_ceil, "", "ceil(fa)", 0, ceilf(x))
HSE_F1(float_round_ties_even, "", "round(fa)", 0, rintf(x))
HSE_F1(float_trunc, "", "trunc(fa)", 0, truncf(x))
HSE_F1(float_frac, "", "frac(fa)", 0, x - floorf(x))
HSE_F1(float_isnan_via_select, "", "isnan(fa) ? 1.0 : 0.0", 0, (x != x) ? 1.0f : 0.0f)
HSE_F1(float_isinf_via_select, "", "isinf(fa) ? 1.0 : 0.0", 0, (x == INFINITY || x == -INFINITY) ? 1.0f : 0.0f)
HSE_F1(float_isfinite_via_select, "", "isfinite(fa) ? 1.0 : 0.0", 0, (x == x && x != INFINITY && x != -INFINITY) ? 1.0f : 0.0f)
HSE_F1(float_sqrt_table, "", "sqrt(fa)", 1, (x < 0) ? NAN : sqrtf(x))
HSE_F1(float_rsqrt_table, "", "rsqrt(fa)", 1, (x != x) ? NAN : x < 0 ? NAN : (x == 0 ? (signbit(x) ? -INFINITY : INFINITY) : 1.0f / sqrtf(x)))
HSE_T(float_round_half_cases, "", "round(fa)", HSE_FLT, 0,
      { FB(0.5f), 0, 0, 0 }, { FB(1.5f), 0, 0, FB(2.0f) }, { FB(2.5f), 0, 0, FB(2.0f) }, { FB(3.5f), 0, 0, FB(4.0f) }, { FB(-0.5f), 0, 0, 0 },
      { FB(-1.5f), 0, 0, FB(-2.0f) }, { FB(-2.5f), 0, 0, FB(-2.0f) }, { FB(0.49999997f), 0, 0, 0 }, { FB(0.50000006f), 0, 0, FB(1.0f) }, { FB(8388607.5f), 0, 0, FB(8388608.0f) })
HSE_T(float_sqrt_exact_squares, "", "sqrt(fa)", HSE_ULP, 1,
      { FB(0.0f), 0, 0, 0 }, { FB(1.0f), 0, 0, FB(1.0f) }, { FB(4.0f), 0, 0, FB(2.0f) }, { FB(9.0f), 0, 0, FB(3.0f) }, { FB(16.0f), 0, 0, FB(4.0f) },
      { FB(0.25f), 0, 0, FB(0.5f) }, { FB(2.25f), 0, 0, FB(1.5f) }, { FB(1e10f), 0, 0, FB(100000.0f) }, { FB(1024.0f), 0, 0, FB(32.0f) }, { FB(0.0625f), 0, 0, FB(0.25f) },
      { F_NZERO, 0, 0, 0 }, { I, 0, 0, I }, { FB(-1.0f), 0, 0, NaN }, { NI, 0, 0, NaN }, { NaN, 0, 0, NaN })
HSE_T(float_rsqrt_exact_squares, "", "rsqrt(fa)", HSE_ULP, 1,
      { FB(1.0f), 0, 0, FB(1.0f) }, { FB(4.0f), 0, 0, FB(0.5f) }, { FB(16.0f), 0, 0, FB(0.25f) }, { FB(0.25f), 0, 0, FB(2.0f) }, { FB(0.0625f), 0, 0, FB(4.0f) },
      { 0, 0, 0, I }, { F_NZERO, 0, 0, NI }, { I, 0, 0, 0 }, { FB(-1.0f), 0, 0, NaN }, { NI, 0, 0, NaN }, { NaN, 0, 0, NaN })

/* ---- ldexp / frexp / fmod / lerp / step ------------------------------------------------------------------ */
HSE_T(float_ldexp, "", "ldexp(fa, fb)", HSE_ULP, 1,
      { FB(1.0f), FB(3.0f), 0, FB(8.0f) }, { FB(3.0f), FB(-1.0f), 0, FB(1.5f) }, { FB(1.0f), 0, 0, FB(1.0f) }, { 0, FB(5.0f), 0, 0 },
      { FB(-0.75f), FB(4.0f), 0, FB(-12.0f) }, { FB(1.0f), FB(10.0f), 0, FB(1024.0f) }, { FB(1.0f), FB(-10.0f), 0, FB(0.0009765625f) })
HSE_T(float_frexp_mantissa, "float e; float m = frexp(fa, e);", "m", HSE_FLT, 0,
      { FB(8.0f), 0, 0, FB(0.5f) }, { FB(1.0f), 0, 0, FB(0.5f) }, { FB(0.75f), 0, 0, FB(0.75f) }, { FB(-3.0f), 0, 0, FB(-0.75f) }, { 0, 0, 0, 0 },
      { FB(1024.0f), 0, 0, FB(0.5f) }, { FB(0.125f), 0, 0, FB(0.5f) }, { FB(3.0f), 0, 0, FB(0.75f) }, { FB(0.5f), 0, 0, FB(0.5f) }, { FB(1.5f), 0, 0, FB(0.75f) })
HSE_T(float_frexp_exponent, "float e; float m = frexp(fa, e);", "e", HSE_FLT, 0,
      { FB(8.0f), 0, 0, FB(4.0f) }, { FB(1.0f), 0, 0, FB(1.0f) }, { FB(0.75f), 0, 0, 0 }, { FB(-3.0f), 0, 0, FB(2.0f) }, { 0, 0, 0, 0 },
      { FB(1024.0f), 0, 0, FB(11.0f) }, { FB(0.125f), 0, 0, FB(-2.0f) }, { FB(3.0f), 0, 0, FB(2.0f) }, { FB(0.5f), 0, 0, 0 }, { FB(1.5f), 0, 0, FB(1.0f) })
HSE_T(float_fmod, "", "fmod(fa, fb)", HSE_ULP, 1,
      { FB(5.5f), FB(2.0f), 0, FB(1.5f) }, { FB(-5.5f), FB(2.0f), 0, FB(-1.5f) }, { FB(5.5f), FB(-2.0f), 0, FB(1.5f) }, { FB(6.0f), FB(3.0f), 0, 0 },
      { FB(1.0f), FB(4.0f), 0, FB(1.0f) }, { FB(10.0f), FB(4.0f), 0, FB(2.0f) })
HSE_T(float_lerp_exact, "", "lerp(fa, fb, fc)", HSE_ULP, 1,
      { 0, FB(10.0f), FB(0.5f), FB(5.0f) }, { FB(2.0f), FB(4.0f), FB(0.25f), FB(2.5f) }, { FB(3.0f), FB(3.0f), FB(0.7f), FB(3.0f) }, { FB(1.0f), FB(5.0f), 0, FB(1.0f) },
      { FB(1.0f), FB(5.0f), FB(1.0f), FB(5.0f) }, { FB(-4.0f), FB(4.0f), FB(0.5f), 0 })
HSE_T(float_step, "", "step(fa, fb)", HSE_EXACT, 0,
      { FB(1.0f), FB(2.0f), 0, FB(1.0f) }, { FB(1.0f), FB(1.0f), 0, FB(1.0f) }, { FB(1.0f), FB(0.99999994f), 0, 0 }, { FB(-1.0f), FB(-2.0f), 0, 0 },
      { FB(0.0f), F_NZERO, 0, FB(1.0f) }, { NaN, FB(1.0f), 0, 0 }, { FB(1.0f), NaN, 0, 0 }, { NI, NI, 0, FB(1.0f) })
HSE_T(float_smoothstep, "", "smoothstep(fa, fb, fc)", HSE_ULP, 2,
      { 0, FB(1.0f), FB(0.5f), FB(0.5f) }, { 0, FB(1.0f), FB(-1.0f), 0 }, { 0, FB(1.0f), FB(2.0f), FB(1.0f) }, { 0, FB(1.0f), 0, 0 },
      { 0, FB(1.0f), FB(1.0f), FB(1.0f) }, { FB(2.0f), FB(4.0f), FB(3.0f), FB(0.5f) }, { 0, FB(1.0f), FB(0.25f), FB(0.15625f) })
HSE_T(float_dot3_exact, "", "dot(float3(fa, fb, fc), float3(1.0, 2.0, 3.0))", HSE_ULP, 1,
      { FB(1.0f), FB(1.0f), FB(1.0f), FB(6.0f) }, { FB(-1.0f), FB(0.5f), FB(2.0f), FB(6.0f) }, { 0, 0, 0, 0 }, { FB(4.0f), FB(-2.0f), FB(0.0f), 0 })
HSE_T(float_length_exact, "", "length(float3(fa, fb, fc))", HSE_ULP, 1,
      { FB(3.0f), FB(4.0f), 0, FB(5.0f) }, { 0, 0, FB(2.0f), FB(2.0f) }, { 0, 0, 0, 0 }, { FB(1.0f), FB(2.0f), FB(2.0f), FB(3.0f) })
HSE_T(float_normalize_axis, "float3 n = normalize(float3(0.0, fa, 0.0));", "n.y", HSE_ULP, 1,
      { 0, FB(5.0f), 0, FB(1.0f) }, { 0, FB(-0.25f), 0, FB(-1.0f) }, { 0, FB(1e10f), 0, FB(1.0f) })
HSE_T(float_cross_axes, "float3 c3 = cross(float3(fa, 0.0, 0.0), float3(0.0, fb, 0.0));", "c3.z", HSE_FLT, 0,
      { FB(1.0f), FB(1.0f), 0, FB(1.0f) }, { FB(2.0f), FB(3.0f), 0, FB(6.0f) }, { FB(-1.0f), FB(1.0f), 0, FB(-1.0f) })
HSE_T(float_distance_exact, "", "distance(float2(fa, fb), float2(0.0, 0.0))", HSE_ULP, 1,
      { FB(3.0f), FB(4.0f), 0, FB(5.0f) }, { FB(-6.0f), FB(8.0f), 0, FB(10.0f) })

/* ---- denormals: arithmetic may flush, moves must not -------------------------------------------------- */
HSE_T(float_denormal_passes_through_move, "", "fa", HSE_EXACT | HSE_SOFT, 0, { 1, 0, 0, 1 }, { 0x007fffffu, 0, 0, 0x007fffffu }, { 0x80000001u, 0, 0, 0x80000001u })
HSE_T(float_denormal_add_zero_observed, "", "fa + fb", HSE_EXACT | HSE_SOFT, 0, { 1, 0, 0, 1 }, { 0x007fffffu, 0, 0, 0x007fffffu })
HSE_T(float_denormal_mul_one_observed, "", "fa * fb", HSE_EXACT | HSE_SOFT, 0, { 1, FB(1.0f), 0, 1 }, { 0x00400000u, FB(2.0f), 0, 0x00800000u })
HSE_T(float_underflow_result_flush_observed, "", "fa * fb", HSE_EXACT | HSE_SOFT, 0, { 0x00800000u, FB(0.5f), 0, 0x00400000u })
HSE_T(float_denormal_is_finite, "", "isfinite(fa) ? 1u : 0u", HSE_EXACT, 0, { 1, 0, 0, 1 }, { 0x007fffffu, 0, 0, 1 })

/* ---- transcendental accuracy: recorded, never failed ------------------------------------------------------- */
static double r_sin(double x, double y) { return sin(x); }
static double r_cos(double x, double y) { return cos(x); }
static double r_tan(double x, double y) { return tan(x); }
static double r_exp(double x, double y) { return exp(x); }
static double r_exp2(double x, double y) { return exp2(x); }
static double r_log(double x, double y) { return log(x); }
static double r_log2(double x, double y) { return log2(x); }
static double r_log10(double x, double y) { return log10(x); }
static double r_pow(double x, double y) { return pow(x, y); }
static double r_atan(double x, double y) { return atan(x); }
static double r_asin(double x, double y) { return asin(x); }
static double r_acos(double x, double y) { return acos(x); }
static double r_sinh(double x, double y) { return sinh(x); }
static double r_tanh(double x, double y) { return tanh(x); }
static double r_atan2(double x, double y) { return atan2(x, y); }
static double r_rsqrt(double x, double y) { return 1.0 / sqrt(x); }
static double r_sqrt(double x, double y) { return sqrt(x); }
static double r_rcp(double x, double y) { return 1.0 / x; }
#define SWEEP_TEST(n, expr, ref, lo, hi, y) TEST(hlsl_semantics_exec_float_accuracy_##n) { HSE_NEED(); HSE_SWEEP(#n, expr, ref, lo, hi, y, 256); }
SWEEP_TEST(sin, "sin(fa)", r_sin, -3.14159265, 3.14159265, 0)
SWEEP_TEST(cos, "cos(fa)", r_cos, -3.14159265, 3.14159265, 0)
SWEEP_TEST(tan, "tan(fa)", r_tan, -1.5, 1.5, 0)
SWEEP_TEST(exp, "exp(fa)", r_exp, -10, 10, 0)
SWEEP_TEST(exp2, "exp2(fa)", r_exp2, -20, 20, 0)
SWEEP_TEST(log, "log(fa)", r_log, 0.001, 1000, 0)
SWEEP_TEST(log2, "log2(fa)", r_log2, 0.001, 1000, 0)
SWEEP_TEST(log10, "log10(fa)", r_log10, 0.001, 1000, 0)
SWEEP_TEST(pow_y2_5, "pow(fa, fb)", r_pow, 0.01, 100, 2.5)
SWEEP_TEST(pow_y3, "pow(fa, fb)", r_pow, 0.01, 100, 3.0)
SWEEP_TEST(atan, "atan(fa)", r_atan, -50, 50, 0)
SWEEP_TEST(asin, "asin(fa)", r_asin, -1, 1, 0)
SWEEP_TEST(acos, "acos(fa)", r_acos, -1, 1, 0)
SWEEP_TEST(sinh, "sinh(fa)", r_sinh, -5, 5, 0)
SWEEP_TEST(tanh, "tanh(fa)", r_tanh, -5, 5, 0)
SWEEP_TEST(atan2, "atan2(fa, fb)", r_atan2, -4, 4, 1.5)
SWEEP_TEST(rsqrt, "rsqrt(fa)", r_rsqrt, 0.001, 1000, 0)
SWEEP_TEST(sqrt, "sqrt(fa)", r_sqrt, 0.0, 1000, 0)
SWEEP_TEST(rcp, "rcp(fa)", r_rcp, 0.001, 1000, 0)
HSE_T(float_transcendental_special_inputs, "", "exp2(fa)", HSE_ULP, 1,
      { NI, 0, 0, 0 }, { I, 0, 0, I }, { NaN, 0, 0, NaN }, { 0, 0, 0, FB(1.0f) })
HSE_T(float_log2_special_inputs, "", "log2(fa)", HSE_ULP, 1,
      { 0, 0, 0, NI }, { I, 0, 0, I }, { NaN, 0, 0, NaN }, { FB(-1.0f), 0, 0, NaN }, { FB(1.0f), 0, 0, 0 })
HSE_T(float_pow_identities, "", "pow(fa, fb)", HSE_ULP, 1,
      { FB(1.0f), FB(5.0f), 0, FB(1.0f) }, { 0, FB(2.0f), 0, 0 }, { FB(2.0f), 0, 0, FB(1.0f) }, { FB(1.0f), FB(-3.0f), 0, FB(1.0f) })
HSE_T(float_sincos_exact_points, "", "sin(fa)", HSE_EXACT | HSE_SOFT, 0, { 0, 0, 0, 0 }, { F_NZERO, 0, 0, F_NZERO }, { FB(1.5707964f), 0, 0, FB(1.0f) })
HSE_T(float_sin_zero, "", "sin(fa)", HSE_FLT, 0, { 0, 0, 0, 0 })
HSE_T(float_cos_zero, "", "cos(fa)", HSE_ULP, 1, { 0, 0, 0, FB(1.0f) })
HSE_T(float_sin_cos_nan_inf, "", "isnan(sin(fa)) ? 1u : 0u", HSE_EXACT, 0, { I, 0, 0, 1 }, { NI, 0, 0, 1 }, { NaN, 0, 0, 1 })
