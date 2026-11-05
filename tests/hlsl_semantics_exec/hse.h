/* Harness for HLSL execution-semantics tests: compile at runtime, run on a D3D11 (WARP) device, compare bits. */
#ifndef HSE_H
#define HSE_H
#include "d3dh.h"
#include <d3d11shader.h>
#include <math.h>

#ifndef D3DCOMPILE_IEEE_STRICTNESS
#define D3DCOMPILE_IEEE_STRICTNESS (1 << 13)
#endif
#ifndef D3DCOMPILE_PACK_MATRIX_ROW_MAJOR
#define D3DCOMPILE_PACK_MATRIX_ROW_MAJOR (1 << 3)
#define D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR (1 << 4)
#endif
#ifndef D3DCOMPILE_SKIP_OPTIMIZATION
#define D3DCOMPILE_SKIP_OPTIMIZATION (1 << 2)
#endif

typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE drv; } hse_t;
extern hse_t hse;
int hse_init(void);   /* 1 when a device exists (created once, released at exit) */
/* hse_need: 1 to go on. Under an emulator with no device it also returns 1 ("compile-only" mode, hse.dev == NULL): resource helpers then return
 * NULL and the dispatch/draw helpers only compile the shader, so the HLSL is syntax-checked. Elsewhere no device means skip. */
int hse_need(void);
#define HSE_NEED() do { if (!hse_need()) { t_skip("no D3D11 device"); return; } } while (0)
/* Use before touching hse.dev/hse.ctx directly (not through the helpers). */
#define HSE_RUN() do { if (!hse.dev) { t_skip("compile-only mode (no device)"); return; } } while (0)
#define HSE_NEED_FL(fl_) do { HSE_NEED(); if (hse.dev && hse.fl < (fl_)) { t_skip("feature level too low"); return; } } while (0)

/* float <-> bits */
static inline uint32_t hse_fb(float f) { union { float f; uint32_t u; } x; x.f = f; return x.u; }
static inline float hse_bf(uint32_t u) { union { float f; uint32_t u; } x; x.u = u; return x.f; }
static inline uint64_t hse_db(double d) { union { double f; uint64_t u; } x; x.f = d; return x.u; }
static inline double hse_bd(uint64_t u) { union { double f; uint64_t u; } x; x.u = u; return x.f; }
#define FB(x) hse_fb(x)
#define BF(x) hse_bf(x)
#define F_INF  0x7f800000u
#define F_NINF 0xff800000u
#define F_NAN  0x7fc00000u
#define F_NZERO 0x80000000u

/* Compile with default flags + IEEE strictness. Failure: CHECK fails (skip under wine). */
ID3DBlob *hse_compile_at(const char *file, int line, const char *src, const char *target, UINT flags);
#define HSE_COMPILE(src, target, flags) hse_compile_at(__FILE__, __LINE__, src, target, flags)

/* One compute dispatch. */
typedef struct {
    const char *src, *target;   /* target default cs_5_0 */
    UINT cflags;                /* extra compile flags */
    const void *in; UINT inbytes;       /* raw buffer bound as t0 (if inbytes) */
    const void *cb; UINT cbbytes;       /* constant buffer b0 (cbbytes multiple of 16) */
    const void *outinit; UINT outbytes; /* raw buffer bound as u0 */
    UINT gx, gy, gz;
    ID3D11ShaderResourceView *srv[6];   /* slots 1.. */
    ID3D11SamplerState *smp[4];
    ID3D11UnorderedAccessView *uav[4];  /* slots 1.. */
    void *out;                  /* receives outbytes bytes */
} hse_cs;
int hse_dispatch_at(const char *file, int line, hse_cs *c);
#define HSE_DISPATCH(c) hse_dispatch_at(__FILE__, __LINE__, c)

/* Generic buffer helpers (raw SRV/UAV views, structured, typed) */
ID3D11Buffer *hse_buf(UINT bytes, UINT bind, UINT misc, UINT stride, const void *init);
ID3D11ShaderResourceView *hse_buf_srv(ID3D11Buffer *b, DXGI_FORMAT fmt, UINT first, UINT num, int raw);
ID3D11UnorderedAccessView *hse_buf_uav(ID3D11Buffer *b, DXGI_FORMAT fmt, UINT first, UINT num, int raw);
int hse_read_buf(ID3D11Buffer *b, void **data, size_t *size);

/* Table-driven scalar kernels.  Inputs visible in HLSL: ua ub uc (uint), ia ib ic (int), fa fb fc (float).
 * `pre` is statement text placed before the store, `expr` is any scalar expression stored with asuint(). */
typedef struct { uint32_t a, b, c, want; } hse_row;
enum { HSE_EXACT = 0,   /* bit-exact */
       HSE_FLT,         /* float compare: -0 == +0, any NaN == any NaN */
       HSE_ULP,         /* within `ulps` ULP; NaN must match NaN, inf exact */
       HSE_SIGNED0,     /* bit-exact, except any NaN == any NaN (sign of zero is checked) */
       HSE_SOFT = 0x100 /* OR-ed in: mismatches are reported with t_note, never fail */ };
extern const char *hse_prelude;   /* set by HSE_PRELUDE: HLSL text (functions, constants) placed before the kernel in the next hse_rows_at call */
#define HSE_PRELUDE(s) (hse_prelude = (s))
int hse_rows_at(const char *file, int line, const char *pre, const char *expr, const hse_row *r, int n, int mode, int ulps, UINT cflags);
#define HSE_ROWS(pre, expr, rows, n, mode, ulps) hse_rows_at(__FILE__, __LINE__, pre, expr, rows, n, mode, ulps, 0)

/* Interesting 32-bit patterns: fills n rows (a,b,c) deterministically; a and b mix edge values, c is pseudo-random. */
void hse_patterns(hse_row *r, int n);
#define HSE_NPAT 192
/* Pattern test with a C reference expression over a, b, c (uint32_t). */
#define HSE_P(name, pre, expr, mode, ulps, ref) \
    TEST(hlsl_semantics_exec_##name) { HSE_NEED(); hse_row r_[HSE_NPAT]; hse_patterns(r_, HSE_NPAT); \
        for (int i_ = 0; i_ < HSE_NPAT; i_++) { uint32_t a = r_[i_].a, b = r_[i_].b, c = r_[i_].c; (void)a; (void)b; (void)c; r_[i_].want = (uint32_t)(ref); } \
        HSE_ROWS(pre, expr, r_, HSE_NPAT, mode, ulps); }

/* Accuracy sweep for transcendental functions: n inputs x in [lo,hi] (a), y (b); expected = (float)ref(x,y); only reports (t_note) the observed error. */
int hse_sweep_at(const char *file, int line, const char *name, const char *expr, double (*ref)(double, double), double lo, double hi, double y, int n);
#define HSE_SWEEP(name, expr, ref, lo, hi, y, n) hse_sweep_at(__FILE__, __LINE__, name, expr, ref, lo, hi, y, n)

/* Define a table test. */
#define HSE_T(name, pre, expr, mode, ulps, ...) \
    TEST(hlsl_semantics_exec_##name) { HSE_NEED(); hse_row r_[] = { __VA_ARGS__ }; \
        HSE_ROWS(pre, expr, r_, (int)(sizeof r_ / sizeof r_[0]), mode, ulps); }

/* Pixel shader runs: full-screen triangle into a w*h render target, returns tightly packed pixels. */
typedef struct {
    const char *vs;             /* NULL: SV_VertexID full-screen triangle */
    const char *ps;
    UINT w, h; DXGI_FORMAT fmt; /* default 4x4 R32G32B32A32_FLOAT */
    const void *cb; UINT cbbytes;
    ID3D11ShaderResourceView *srv[6];
    ID3D11SamplerState *smp[4];
    float clear[4];
    UINT cflags;
} hse_ps;
int hse_draw_at(const char *file, int line, hse_ps *p, d3dh_image *out);
#define HSE_DRAW(p, out) hse_draw_at(__FILE__, __LINE__, p, out)

/* Texture helper: w*h, mips, format, bind flags; init = array of subresource data pointers+pitches (NULL: no init). */
ID3D11Texture2D *hse_tex2d(UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT fmt, UINT bind, const D3D11_SUBRESOURCE_DATA *init);
ID3D11SamplerState *hse_sampler(D3D11_FILTER f, D3D11_TEXTURE_ADDRESS_MODE am, D3D11_COMPARISON_FUNC cmp, float lodbias);

/* ULP distance between two finite floats (huge when signs differ and not both zero). */
uint32_t hse_ulp_dist(uint32_t a, uint32_t b);
#endif
