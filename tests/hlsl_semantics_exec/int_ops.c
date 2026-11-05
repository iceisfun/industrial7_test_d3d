/* Integer instruction semantics executed in compute shaders on the D3D11 device (WARP preferred). */
#include "hse.h"

#define POP(x) ((uint32_t)__builtin_popcount(x))
static uint32_t ref_rev(uint32_t x) { uint32_t r = 0; for (int i = 0; i < 32; i++) if (x & (1u << i)) r |= 1u << (31 - i); return r; }
static uint32_t ref_fbh(uint32_t x) { return x ? 31u - (uint32_t)__builtin_clz(x) : 0xffffffffu; }
static uint32_t ref_fbs(uint32_t x) { if (x == 0 || x == 0xffffffffu) return 0xffffffffu; if ((int32_t)x < 0) x = ~x; return 31u - (uint32_t)__builtin_clz(x); }
static uint32_t ref_fbl(uint32_t x) { return x ? (uint32_t)__builtin_ctz(x) : 0xffffffffu; }
#define SDIV_PRE "int d = (ib == 0 || (ia == -2147483647 - 1 && ib == -1)) ? 1 : ib;"
static int32_t sden(uint32_t a, uint32_t b) { return (b == 0 || (a == 0x80000000u && b == 0xffffffffu)) ? 1 : (int32_t)b; }

/* ---- arithmetic ------------------------------------------------------------------------------ */
HSE_P(int_uadd, "", "ua + ub", HSE_EXACT, 0, a + b)
HSE_P(int_usub, "", "ua - ub", HSE_EXACT, 0, a - b)
HSE_P(int_umul_low32, "", "ua * ub", HSE_EXACT, 0, a * b)
HSE_P(int_imul_low32, "", "ia * ib", HSE_EXACT, 0, a * b)
HSE_P(int_umad, "", "ua * ub + uc", HSE_EXACT, 0, a * b + c)
HSE_P(int_imad, "", "ia * ib + ic", HSE_EXACT, 0, a * b + c)
HSE_P(int_iadd_wraps, "", "ia + ib", HSE_EXACT, 0, a + b)
HSE_P(int_ineg_wraps, "", "-ia", HSE_EXACT, 0, 0u - a)
HSE_P(int_iabs_int_min_wraps, "", "abs(ia)", HSE_EXACT, 0, ((int32_t)a < 0) ? 0u - a : a)
HSE_P(int_udiv_nonzero, "uint d = ub == 0 ? 1 : ub;", "ua / d", HSE_EXACT, 0, a / (b ? b : 1))
HSE_P(int_umod_nonzero, "uint d = ub == 0 ? 1 : ub;", "ua % d", HSE_EXACT, 0, a % (b ? b : 1))
HSE_P(int_idiv_nonzero, SDIV_PRE, "ia / d", HSE_EXACT, 0, (uint32_t)((int32_t)a / sden(a, b)))
HSE_P(int_imod_nonzero, SDIV_PRE, "ia % d", HSE_EXACT, 0, (uint32_t)((int32_t)a % sden(a, b)))
HSE_P(int_umin, "", "min(ua, ub)", HSE_EXACT, 0, a < b ? a : b)
HSE_P(int_umax, "", "max(ua, ub)", HSE_EXACT, 0, a > b ? a : b)
HSE_P(int_imin, "", "min(ia, ib)", HSE_EXACT, 0, (int32_t)a < (int32_t)b ? a : b)
HSE_P(int_imax, "", "max(ia, ib)", HSE_EXACT, 0, (int32_t)a > (int32_t)b ? a : b)
HSE_P(int_uclamp, "uint lo = min(ub, uc), hi = max(ub, uc);", "clamp(ua, lo, hi)", HSE_EXACT, 0,
      (a < (b < c ? b : c)) ? (b < c ? b : c) : (a > (b > c ? b : c) ? (b > c ? b : c) : a))
HSE_P(int_isign, "", "sign(ia)", HSE_EXACT, 0, (int32_t)a > 0 ? 1u : ((int32_t)a < 0 ? 0xffffffffu : 0u))

/* ---- division by zero (documented udiv behaviour) ---------------------------------------------- */
HSE_T(int_udiv_by_zero_all_ones, "", "ua / ub", HSE_EXACT, 0,
      { 1, 0, 0, 0xffffffffu }, { 0, 0, 0, 0xffffffffu }, { 0xffffffffu, 0, 0, 0xffffffffu }, { 0x80000000u, 0, 0, 0xffffffffu })
HSE_T(int_umod_by_zero_all_ones, "", "ua % ub", HSE_EXACT, 0,
      { 1, 0, 0, 0xffffffffu }, { 0, 0, 0, 0xffffffffu }, { 12345, 0, 0, 0xffffffffu }, { 0xffffffffu, 0, 0, 0xffffffffu })
HSE_T(int_udiv_known_values, "", "ua / ub", HSE_EXACT, 0,
      { 10, 3, 0, 3 }, { 0xffffffffu, 1, 0, 0xffffffffu }, { 0xffffffffu, 0xffffffffu, 0, 1 }, { 0xfffffffeu, 0xffffffffu, 0, 0 },
      { 0x80000000u, 2, 0, 0x40000000u }, { 7, 8, 0, 0 }, { 0, 5, 0, 0 })
HSE_T(int_idiv_signed_known, "", "ia / ib", HSE_EXACT, 0,
      { (uint32_t)-7, 2, 0, (uint32_t)-3 }, { 7, (uint32_t)-2, 0, (uint32_t)-3 }, { (uint32_t)-7, (uint32_t)-2, 0, 3 },
      { 0x7fffffff, (uint32_t)-1, 0, 0x80000001u }, { 0x80000000u, 1, 0, 0x80000000u }, { 0x80000001u, (uint32_t)-1, 0, 0x7fffffffu })
HSE_T(int_imod_sign_follows_dividend, "", "ia % ib", HSE_EXACT, 0,
      { (uint32_t)-7, 3, 0, (uint32_t)-1 }, { 7, (uint32_t)-3, 0, 1 }, { (uint32_t)-7, (uint32_t)-3, 0, (uint32_t)-1 }, { 6, 3, 0, 0 })

/* ---- shifts: only the low five bits of the count are used --------------------------------------- */
HSE_P(int_ishl_mask5, "", "ua << ub", HSE_EXACT, 0, a << (b & 31))
HSE_P(int_ushr_mask5, "", "ua >> ub", HSE_EXACT, 0, a >> (b & 31))
HSE_P(int_ishr_mask5_arith, "", "ia >> ib", HSE_EXACT, 0, (uint32_t)((int32_t)a >> (b & 31)))
HSE_T(int_shift_count_32_is_zero, "", "(ua << ub) ^ (ua >> ub) ^ asuint(ia >> ib)", HSE_EXACT, 0,
      { 0x80000001u, 32, 0, 0x80000001u ^ 0x80000001u ^ 0x80000001u }, { 5, 64, 0, 5 }, { 0x80000000u, 33, 0, (0x80000000u << 1) ^ (0x80000000u >> 1) ^ 0xc0000000u })
HSE_T(int_shift_count_all_ones_is_31, "", "(ua << ub)", HSE_EXACT, 0,
      { 1, 0xffffffffu, 0, 0x80000000u }, { 0xffffffffu, 0xffffffffu, 0, 0x80000000u }, { 3, 0xffffffe0u, 0, 3 })
HSE_T(int_ishr_sign_fill, "", "ia >> ib", HSE_EXACT, 0,
      { 0x80000000u, 31, 0, 0xffffffffu }, { 0x80000000u, 1, 0, 0xc0000000u }, { 0x7fffffff, 31, 0, 0 }, { 0xfffffffeu, 1, 0, 0xffffffffu })

/* ---- bit ops --------------------------------------------------------------------------------- */
HSE_P(int_and, "", "ua & ub", HSE_EXACT, 0, a & b)
HSE_P(int_or, "", "ua | ub", HSE_EXACT, 0, a | b)
HSE_P(int_xor, "", "ua ^ ub", HSE_EXACT, 0, a ^ b)
HSE_P(int_not, "", "~ua", HSE_EXACT, 0, ~a)
HSE_P(int_countbits_uint, "", "countbits(ua)", HSE_EXACT, 0, POP(a))
HSE_P(int_countbits_int, "", "countbits(ia)", HSE_EXACT, 0, POP(a))
HSE_P(int_reversebits, "", "reversebits(ua)", HSE_EXACT, 0, ref_rev(a))
HSE_P(int_reversebits_involution, "", "reversebits(reversebits(ua))", HSE_EXACT, 0, a)
HSE_P(int_firstbithigh_uint, "", "firstbithigh(ua)", HSE_EXACT, 0, ref_fbh(a))
HSE_P(int_firstbithigh_int, "", "firstbithigh(ia)", HSE_EXACT, 0, ref_fbs(a))
HSE_P(int_firstbitlow_uint, "", "firstbitlow(ua)", HSE_EXACT, 0, ref_fbl(a))
HSE_P(int_firstbitlow_int, "", "firstbitlow(ia)", HSE_EXACT, 0, ref_fbl(a))
HSE_T(int_firstbit_known, "", "firstbithigh(ua)", HSE_EXACT, 0,
      { 0, 0, 0, 0xffffffffu }, { 1, 0, 0, 0 }, { 2, 0, 0, 1 }, { 0x10000000u, 0, 0, 28 }, { 0x80000000u, 0, 0, 31 }, { 0xffffffffu, 0, 0, 31 })
HSE_T(int_firstbitlow_known, "", "firstbitlow(ua)", HSE_EXACT, 0,
      { 0, 0, 0, 0xffffffffu }, { 1, 0, 0, 0 }, { 8, 0, 0, 3 }, { 0x80000000u, 0, 0, 31 }, { 0xfffffff0u, 0, 0, 4 })
HSE_T(int_firstbithigh_signed_known, "", "firstbithigh(ia)", HSE_EXACT, 0,
      { 0, 0, 0, 0xffffffffu }, { 0xffffffffu, 0, 0, 0xffffffffu }, { 1, 0, 0, 0 }, { 0xfffffffeu, 0, 0, 0 },
      { 0x80000000u, 0, 0, 30 }, { 0x7fffffffu, 0, 0, 30 }, { 0xfffffff0u, 0, 0, 3 }, { 0xc0000000u, 0, 0, 29 })

/* ---- bitfield patterns (values identical whether or not the compiler picks ubfe/ibfe/bfi) ------- */
HSE_P(int_bitfield_extract_unsigned, "uint o = ub % 24; uint w = uc % 8 + 1;", "(ua >> o) & ((1u << w) - 1u)", HSE_EXACT, 0,
      (a >> (b % 24)) & ((1u << (c % 8 + 1)) - 1u))
HSE_P(int_bitfield_extract_signed, "uint o = ub % 24; uint w = uc % 8 + 1;", "(ia << (32 - w - o)) >> (32 - w)", HSE_EXACT, 0,
      (uint32_t)((int32_t)(a << (32 - (c % 8 + 1) - (b % 24))) >> (32 - (c % 8 + 1))))
HSE_P(int_bitfield_insert, "uint o = ub % 24; uint w = uc % 8 + 1; uint m = ((1u << w) - 1u) << o;", "(ua & ~m) | ((ua * 3u << o) & m)", HSE_EXACT, 0,
      (a & ~((((1u << (c % 8 + 1)) - 1u)) << (b % 24))) | (((a * 3u) << (b % 24)) & (((1u << (c % 8 + 1)) - 1u) << (b % 24))))

/* ---- int/uint <-> float -------------------------------------------------------------------- */
HSE_T(int_itof_exact, "", "(float)ia", HSE_EXACT, 0,
      { 0, 0, 0, 0 }, { 1, 0, 0, 0x3f800000u }, { (uint32_t)-1, 0, 0, 0xbf800000u }, { 16777216, 0, 0, 0x4b800000u },
      { (uint32_t)-16777216, 0, 0, 0xcb800000u }, { 0x80000000u, 0, 0, 0xcf000000u }, { 255, 0, 0, 0x437f0000u }, { 1 << 30, 0, 0, 0x4e800000u })
HSE_T(int_utof_exact, "", "(float)ua", HSE_EXACT, 0,
      { 0, 0, 0, 0 }, { 1, 0, 0, 0x3f800000u }, { 0x80000000u, 0, 0, 0x4f000000u }, { 16777216, 0, 0, 0x4b800000u }, { 0xffff0000u, 0, 0, 0x4f7fff00u })
HSE_P(int_itof_pattern_within_1ulp, "", "(float)ia", HSE_ULP, 1, FB((float)(int32_t)a))
HSE_P(int_utof_pattern_within_1ulp, "", "(float)ua", HSE_ULP, 1, FB((float)a))
HSE_P(int_itof_round_nearest_even_observed, "", "(float)ia", HSE_EXACT | HSE_SOFT, 0, FB((float)(int32_t)a))
HSE_P(int_utof_round_nearest_even_observed, "", "(float)ua", HSE_EXACT | HSE_SOFT, 0, FB((float)a))
HSE_T(int_ftoi_truncates_and_saturates, "", "(int)fa", HSE_EXACT, 0,
      { FB(0.9f), 0, 0, 0 }, { FB(-0.9f), 0, 0, 0 }, { FB(1.5f), 0, 0, 1 }, { FB(-1.5f), 0, 0, (uint32_t)-1 }, { FB(-2.99f), 0, 0, (uint32_t)-2 },
      { FB(2147483648.0f), 0, 0, 0x7fffffffu }, { FB(-2147483904.0f), 0, 0, 0x80000000u }, { FB(-2147483648.0f), 0, 0, 0x80000000u },
      { FB(3e9f), 0, 0, 0x7fffffffu }, { FB(-3e9f), 0, 0, 0x80000000u }, { F_INF, 0, 0, 0x7fffffffu }, { F_NINF, 0, 0, 0x80000000u },
      { 0x4effffffu, 0, 0, 0x7fffff80u }, { F_NAN, 0, 0, 0 }, { 0xffc00000u, 0, 0, 0 }, { F_NZERO, 0, 0, 0 })
HSE_T(int_ftou_truncates_and_saturates, "", "(uint)fa", HSE_EXACT, 0,
      { FB(0.9f), 0, 0, 0 }, { FB(1.5f), 0, 0, 1 }, { FB(-1.0f), 0, 0, 0 }, { FB(-0.5f), 0, 0, 0 }, { F_NINF, 0, 0, 0 },
      { FB(2147483648.0f), 0, 0, 0x80000000u }, { FB(4294967296.0f), 0, 0, 0xffffffffu }, { 0x4f7fffffu, 0, 0, 0xffffff00u },
      { FB(1e20f), 0, 0, 0xffffffffu }, { F_INF, 0, 0, 0xffffffffu }, { F_NAN, 0, 0, 0 }, { FB(16777217.0f), 0, 0, 16777216u }, { FB(100.99f), 0, 0, 100 })
HSE_T(int_float_to_int_roundtrip, "", "(uint)(float)ua", HSE_EXACT, 0,
      { 0, 0, 0, 0 }, { 1, 0, 0, 1 }, { 1000, 0, 0, 1000 }, { 16777216, 0, 0, 16777216 }, { 8388607, 0, 0, 8388607 })

/* ---- half precision ------------------------------------------------------------------------ */
static uint32_t ref_h2f(uint32_t h) {
    uint32_t s = (h & 0x8000u) << 16, e = (h >> 10) & 31, m = h & 0x3ff;
    if (e == 0) {
        if (!m) return s;
        int sh = 0; while (!(m & 0x400)) { m <<= 1; sh++; }
        return s | ((uint32_t)(113 - sh) << 23) | ((m & 0x3ff) << 13);
    }
    if (e == 31) return s | 0x7f800000u | (m << 13);
    return s | ((e + 112) << 23) | (m << 13);
}
TEST(hlsl_semantics_exec_int_f16tof32_exhaustive) {
    HSE_NEED();
    enum { N = 65536 };
    hse_row *r = calloc(N, sizeof *r);
    for (int i = 0; i < N; i++) { r[i].a = (uint32_t)i; r[i].want = ref_h2f((uint32_t)i); }
    HSE_ROWS("", "f16tof32(ua)", r, N, HSE_FLT, 0);
    free(r);
}
TEST(hlsl_semantics_exec_int_f16tof32_ignores_high_half) {
    HSE_NEED();
    hse_row r[] = { { 0x12343c00u, 0, 0, 0x3f800000u }, { 0xffffc000u, 0, 0, 0xc0000000u }, { 0xabcd0000u, 0, 0, 0 }, { 0x00017bffu, 0, 0, 0x477fe000u } };
    HSE_ROWS("", "f16tof32(ua)", r, 4, HSE_EXACT, 0);
}
HSE_T(int_f16tof32_known, "", "f16tof32(ua)", HSE_EXACT, 0,
      { 0x3c00, 0, 0, 0x3f800000u }, { 0xc000, 0, 0, 0xc0000000u }, { 0x7bff, 0, 0, 0x477fe000u }, { 0x0000, 0, 0, 0 }, { 0x8000, 0, 0, 0x80000000u },
      { 0x7c00, 0, 0, F_INF }, { 0xfc00, 0, 0, F_NINF }, { 0x0400, 0, 0, 0x38800000u }, { 0x3555, 0, 0, 0x3eaaa000u })
HSE_T(int_f16tof32_nan_is_nan, "uint v = asuint(f16tof32(ua));", "((v & 0x7f800000) == 0x7f800000 && (v & 0x7fffff) != 0) ? 1u : 0u", HSE_EXACT, 0,
      { 0x7e00, 0, 0, 1 }, { 0xfe00, 0, 0, 1 }, { 0x7c01, 0, 0, 1 }, { 0x7fff, 0, 0, 1 }, { 0x7c00, 0, 0, 0 })
/* NaN results are normalised so tables stay independent of the payload. */
HSE_T(int_f32tof16_exact_values, "uint h = f32tof16(fa); bool nn = (h & 0x7c00) == 0x7c00 && (h & 0x3ff) != 0;", "nn ? 0x7e00u : h", HSE_EXACT, 0,
      { FB(1.0f), 0, 0, 0x3c00 }, { FB(-2.0f), 0, 0, 0xc000 }, { 0, 0, 0, 0 }, { F_NZERO, 0, 0, 0x8000 }, { FB(65504.0f), 0, 0, 0x7bff },
      { F_INF, 0, 0, 0x7c00 }, { F_NINF, 0, 0, 0xfc00 }, { F_NAN, 0, 0, 0x7e00 }, { FB(0.5f), 0, 0, 0x3800 }, { FB(1.0009765625f), 0, 0, 0x3c01 },
      { 0x33800000u, 0, 0, 0x0001 }, { 0x38800000u, 0, 0, 0x0400 }, { FB(-0.0009765625f), 0, 0, 0x9400 }, { FB(0.333251953125f), 0, 0, 0x3555 })
HSE_T(int_f32tof16_high_half_is_zero, "", "f32tof16(fa) >> 16", HSE_EXACT, 0,
      { FB(1.0f), 0, 0, 0 }, { FB(-1.0f), 0, 0, 0 }, { F_NINF, 0, 0, 0 }, { F_NAN, 0, 0, 0 })
/* Rounding of inexact conversions: the instruction may truncate or round; recorded, not asserted. */
HSE_T(int_f32tof16_inexact_rounding_observed, "", "f32tof16(fa)", HSE_EXACT | HSE_SOFT, 0,
      { FB(1.0f + 3.0f / 2048.0f), 0, 0, 0x3c02 }, { FB(65520.0f), 0, 0, 0x7c00 }, { FB(1.0f + 1.0f / 2048.0f), 0, 0, 0x3c00 },
      { FB(1.0f + 3.5f / 2048.0f), 0, 0, 0x3c02 }, { FB(-1.0f - 3.0f / 2048.0f), 0, 0, 0xbc02 })
TEST(hlsl_semantics_exec_int_f32tof16_inexact_truncated_or_rounded) {
    HSE_NEED();
    hse_row r[] = { { FB(1.0f + 3.0f / 2048.0f), 0, 0, 0 }, { FB(1.0f + 3.5f / 2048.0f), 0, 0, 0 }, { FB(3.14159265f), 0, 0, 0 }, { FB(-0.1f), 0, 0, 0 } };
    uint32_t lo[4] = { 0x3c01, 0x3c01, 0x4248, 0xae66 }, hi[4] = { 0x3c02, 0x3c02, 0x4249, 0xae67 };
    char src[1024];
    snprintf(src, sizeof src, "ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n"
        "[numthreads(4,1,1)] void main(uint3 id : SV_DispatchThreadID) { O.Store(id.x * 4, f32tof16(asfloat(I.Load(id.x * 16)))); }");
    uint32_t out[4] = { 0 };
    hse_cs c; memset(&c, 0, sizeof c); c.src = src; c.in = r; c.inbytes = sizeof r; c.outbytes = 16; c.gx = 1; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    for (int i = 0; i < 4; i++) CHECKF(out[i] == lo[i] || out[i] == hi[i], "i=%d got %04x (acceptable %04x or %04x)", i, out[i], lo[i], hi[i]);
    t_note("f32tof16 inexact results: %04x %04x %04x %04x (round-to-nearest-even would be 3c02 3c02 4248 ae66)", out[0], out[1], out[2], out[3]);
}
HSE_T(int_f16_roundtrip_all_normals_sample, "", "f32tof16(f16tof32(ua))", HSE_EXACT, 0,
      { 0x0001, 0, 0, 0x0001 }, { 0x03ff, 0, 0, 0x03ff }, { 0x0400, 0, 0, 0x0400 }, { 0x3c00, 0, 0, 0x3c00 }, { 0x7bff, 0, 0, 0x7bff },
      { 0x8001, 0, 0, 0x8001 }, { 0xfbff, 0, 0, 0xfbff }, { 0x7c00, 0, 0, 0x7c00 }, { 0x1234, 0, 0, 0x1234 }, { 0xabcd, 0, 0, 0xabcd })
TEST(hlsl_semantics_exec_int_f16_roundtrip_exhaustive_finite) {
    HSE_NEED();
    enum { N = 65536 };
    hse_row *r = calloc(N, sizeof *r);
    for (int i = 0; i < N; i++) { r[i].a = (uint32_t)i; r[i].want = (i & 0x7c00) == 0x7c00 ? 0 : (uint32_t)i; }
    /* NaN/inf inputs are excluded by zeroing: pre maps them to 0. */
    HSE_ROWS("uint k = ((ua & 0x7c00) == 0x7c00) ? 0u : ua;", "f32tof16(f16tof32(k))", r, N, HSE_EXACT, 0);
    free(r);
}
