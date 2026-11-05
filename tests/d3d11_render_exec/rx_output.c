/* Output merger: write masks, blending, depth/stencil, MSAA, render-target formats, queries. */
#include "rx.h"

#define A(a) RX_N(a)

/* ----------------------------------------------------- colour write ---- */
TEST(d3d11_render_exec_color_write_mask_all_16)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        static const float dst[4] = { 51 / 255.f, 102 / 255.f, 153 / 255.f, 204 / 255.f };
        static const float src[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
        D3D11_BLEND_DESC bd;
        for (UINT mask = 0; mask < 16; mask++) {
            d3dh_image im; uint32_t want = 0;
            const uint8_t d8[4] = { 51, 102, 153, 204 }, s8[4] = { 255, 0, 255, 0 };
            for (int c = 0; c < 4; c++) want |= (uint32_t)((mask >> c) & 1 ? s8[c] : d8[c]) << (8 * c);
            rx_blend_default(&bd); bd.RenderTarget[0].RenderTargetWriteMask = (UINT8)mask;
            rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
            rx_clear(&r, dst[0], dst[1], dst[2], dst[3]);
            rx_full(&r, 0, src);
            if (!rx_read_rt(&r, &im)) break;
            CHECKF(d3dh_image_pixel32(&im, 0, 0) == want && d3dh_image_pixel32(&im, 1, 1) == want, "mask %x: got %08x want %08x", mask, d3dh_image_pixel32(&im, 0, 0), want);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_blend_disabled_overwrites)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        D3D11_BLEND_DESC bd; d3dh_image im;
        rx_blend_default(&bd);
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO; bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;  /* ignored: BlendEnable false */
        rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
        rx_clear(&r, 1, 1, 1, 1);
        rx_full(&r, 0, RX_RED);
        if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, RGBA8(255, 0, 0, 255)); d3dh_image_free(&im); }
    }
    rx_close(&r);
}

/* reference blend arithmetic (floating point), results compared with +-1 on the 8-bit channel */
static float bl_fac(D3D11_BLEND f, int c, const float *s, const float *d, const float *bf)
{
    switch (f) {
    case D3D11_BLEND_ZERO: return 0;
    case D3D11_BLEND_ONE: return 1;
    case D3D11_BLEND_SRC_COLOR: return s[c];
    case D3D11_BLEND_INV_SRC_COLOR: return 1 - s[c];
    case D3D11_BLEND_SRC_ALPHA: return s[3];
    case D3D11_BLEND_INV_SRC_ALPHA: return 1 - s[3];
    case D3D11_BLEND_DEST_ALPHA: return d[3];
    case D3D11_BLEND_INV_DEST_ALPHA: return 1 - d[3];
    case D3D11_BLEND_DEST_COLOR: return d[c];
    case D3D11_BLEND_INV_DEST_COLOR: return 1 - d[c];
    case D3D11_BLEND_SRC_ALPHA_SAT: { if (c == 3) return 1; float v = 1 - d[3]; return s[3] < v ? s[3] : v; }
    case D3D11_BLEND_BLEND_FACTOR: return bf[c];
    case D3D11_BLEND_INV_BLEND_FACTOR: return 1 - bf[c];
    default: return 0;
    }
}
static float bl_op(D3D11_BLEND_OP op, float s, float fs, float d, float fd)
{
    float v;
    switch (op) {
    case D3D11_BLEND_OP_ADD: v = s * fs + d * fd; break;
    case D3D11_BLEND_OP_SUBTRACT: v = s * fs - d * fd; break;
    case D3D11_BLEND_OP_REV_SUBTRACT: v = d * fd - s * fs; break;
    case D3D11_BLEND_OP_MIN: v = s < d ? s : d; break;
    default: v = s > d ? s : d; break;
    }
    return v < 0 ? 0 : v > 1 ? 1 : v;
}
static D3D11_BLEND bl_alpha_of(D3D11_BLEND f)
{
    switch (f) {
    case D3D11_BLEND_SRC_COLOR: return D3D11_BLEND_SRC_ALPHA;
    case D3D11_BLEND_INV_SRC_COLOR: return D3D11_BLEND_INV_SRC_ALPHA;
    case D3D11_BLEND_DEST_COLOR: return D3D11_BLEND_DEST_ALPHA;
    case D3D11_BLEND_INV_DEST_COLOR: return D3D11_BLEND_INV_DEST_ALPHA;
    default: return f;
    }
}

static void blend_case(rx *r, D3D11_BLEND sf, D3D11_BLEND df, D3D11_BLEND_OP op, D3D11_BLEND_OP aop, int *fails)
{
    static const float dstc[4] = { 51 / 255.f, 102 / 255.f, 153 / 255.f, 204 / 255.f };
    static const float srcc[4] = { 102 / 255.f, 51 / 255.f, 204 / 255.f, 153 / 255.f };
    static const float bf[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    D3D11_BLEND_DESC bd; d3dh_image im;
    rx_blend_default(&bd);
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = sf; bd.RenderTarget[0].DestBlend = df; bd.RenderTarget[0].BlendOp = op;
    bd.RenderTarget[0].SrcBlendAlpha = bl_alpha_of(sf); bd.RenderTarget[0].DestBlendAlpha = bl_alpha_of(df); bd.RenderTarget[0].BlendOpAlpha = aop;
    rx_set_bs(r, &bd, bf, 0xFFFFFFFFu);
    rx_clear(r, dstc[0], dstc[1], dstc[2], dstc[3]);
    rx_full(r, 0, srcc);
    if (!rx_read_rt(r, &im)) { (*fails)++; return; }
    uint32_t got = d3dh_image_pixel32(&im, 0, 0);
    uint32_t want = 0;
    for (int c = 0; c < 4; c++) {
        D3D11_BLEND fs = c < 3 ? sf : bl_alpha_of(sf), fd = c < 3 ? df : bl_alpha_of(df);
        D3D11_BLEND_OP o = c < 3 ? op : aop;
        float v = bl_op(o, srcc[c], bl_fac(fs, c, srcc, dstc, bf), dstc[c], bl_fac(fd, c, srcc, dstc, bf));
        want |= (uint32_t)(int)(v * 255.0f + 0.5f) << (8 * c);
    }
    int ok = rx_close32(got, want, 1) && got == d3dh_image_pixel32(&im, 1, 1);
    if (!ok) (*fails)++;
    CHECKF(ok, "src %d dst %d op %d/%d: got %08x want %08x (+-1)", (int)sf, (int)df, (int)op, (int)aop, got, want);
    d3dh_image_free(&im);
}

static const D3D11_BLEND BL_ALL[] = {
    D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
    D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_SRC_ALPHA_SAT,
    D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR };

TEST(d3d11_render_exec_blend_factor_matrix_add)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        int fails = 0;
        for (unsigned s = 0; s < A(BL_ALL) && fails < 20; s++) for (unsigned d = 0; d < A(BL_ALL) && fails < 20; d++)
            blend_case(&r, BL_ALL[s], BL_ALL[d], D3D11_BLEND_OP_ADD, D3D11_BLEND_OP_ADD, &fails);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_blend_ops_table)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        int fails = 0;
        static const D3D11_BLEND fac[] = { D3D11_BLEND_ONE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_ZERO };
        for (int op = D3D11_BLEND_OP_ADD; op <= D3D11_BLEND_OP_MAX; op++) for (int aop = D3D11_BLEND_OP_ADD; aop <= D3D11_BLEND_OP_MAX; aop += 2)
            for (unsigned i = 0; i < A(fac); i++) for (unsigned j = 0; j < A(fac); j++) {
                if (op >= D3D11_BLEND_OP_MIN && (i || j)) continue;  /* factors are ignored for MIN/MAX: ONE/ONE only */
                blend_case(&r, fac[i], fac[j], (D3D11_BLEND_OP)op, (D3D11_BLEND_OP)(aop > 5 ? 5 : aop), &fails);
            }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_blend_min_max_ignore_factors)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        int fails = 0;
        static const D3D11_BLEND odd[] = { D3D11_BLEND_ZERO, D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_SRC_ALPHA_SAT };
        for (int op = D3D11_BLEND_OP_MIN; op <= D3D11_BLEND_OP_MAX; op++) for (unsigned i = 0; i < A(odd); i++) {
            D3D11_BLEND_DESC bd; d3dh_image im;
            static const float dstc[4] = { 51 / 255.f, 102 / 255.f, 153 / 255.f, 204 / 255.f };
            static const float srcc[4] = { 102 / 255.f, 51 / 255.f, 204 / 255.f, 153 / 255.f };
            rx_blend_default(&bd);
            bd.RenderTarget[0].BlendEnable = TRUE; bd.RenderTarget[0].BlendOp = bd.RenderTarget[0].BlendOpAlpha = (D3D11_BLEND_OP)op;
            bd.RenderTarget[0].SrcBlend = odd[i]; bd.RenderTarget[0].DestBlend = odd[(i + 1) % A(odd)];
            bd.RenderTarget[0].SrcBlendAlpha = bl_alpha_of(odd[i]); bd.RenderTarget[0].DestBlendAlpha = bl_alpha_of(odd[(i + 1) % A(odd)]);
            rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
            rx_clear(&r, dstc[0], dstc[1], dstc[2], dstc[3]);
            rx_full(&r, 0, srcc);
            if (!rx_read_rt(&r, &im)) continue;
            uint32_t want = op == D3D11_BLEND_OP_MIN ? RGBA8(51, 51, 153, 153) : RGBA8(102, 102, 204, 204);
            uint32_t got = d3dh_image_pixel32(&im, 0, 0);
            CHECKF(got == want, "op %d factor set %u: got %08x want %08x", op, i, got, want);
            fails += got != want;
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_blend_classic_alpha_over)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        D3D11_BLEND_DESC bd; d3dh_image im;
        rx_blend_default(&bd);
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
        /* white over black, alpha 0.2 -> 51; then red alpha 0 -> unchanged; alpha 1 -> replaced */
        static const float c1[4] = { 1, 1, 1, 51 / 255.f }, c2[4] = { 1, 0, 0, 0 }, c3[4] = { 0, 0, 1, 1 };
        rx_clear(&r, 0, 0, 0, 0);
        rx_full(&r, 0, c1);
        if (rx_read_rt(&r, &im)) { CHECK(rx_close32(d3dh_image_pixel32(&im, 0, 0), RGBA8(51, 51, 51, 51), 1)); d3dh_image_free(&im); }
        rx_full(&r, 0, c2);
        if (rx_read_rt(&r, &im)) { CHECK(rx_close32(d3dh_image_pixel32(&im, 1, 1), RGBA8(51, 51, 51, 51), 1)); d3dh_image_free(&im); }
        rx_full(&r, 0, c3);
        if (rx_read_rt(&r, &im)) { CHECK(rx_close32(d3dh_image_pixel32(&im, 1, 0), RGBA8(0, 0, 255, 255), 0)); d3dh_image_free(&im); }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_blend_factor_roundtrip_and_use)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        D3D11_BLEND_DESC bd; d3dh_image im; float bf[4] = { 0.25f, 0.5f, 0.0f, 1.0f }, got[4] = { 9, 9, 9, 9 }; UINT mask = 0; ID3D11BlendState *bs = NULL;
        rx_blend_default(&bd);
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_BLEND_FACTOR; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR; bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
        rx_set_bs(&r, &bd, bf, 0xFFFFFFFFu);
        ID3D11DeviceContext_OMGetBlendState(r.ctx, &bs, got, &mask);
        CHECK(bs == r.bs);
        CHECKF(mask == 0xFFFFFFFFu, "sample mask %08x", mask);
        CHECK(got[0] == 0.25f && got[1] == 0.5f && got[2] == 0.0f && got[3] == 1.0f);
        SAFE_RELEASE(bs);
        rx_clear(&r, 0, 0, 0, 0);
        static const float white[4] = { 1, 1, 1, 1 };
        rx_full(&r, 0, white);
        if (rx_read_rt(&r, &im)) { CHECK(rx_close32(d3dh_image_pixel32(&im, 0, 0), RGBA8(64, 128, 0, 255), 1)); d3dh_image_free(&im); }
        /* passing NULL factor array resets to (1,1,1,1) */
        ID3D11DeviceContext_OMSetBlendState(r.ctx, r.bs, NULL, 0xFFFFFFFFu);
        ID3D11DeviceContext_OMGetBlendState(r.ctx, &bs, got, &mask);
        CHECK(got[0] == 1.0f && got[1] == 1.0f && got[2] == 1.0f && got[3] == 1.0f);
        SAFE_RELEASE(bs);
        /* NULL state resets to the default state */
        ID3D11DeviceContext_OMSetBlendState(r.ctx, NULL, bf, 0xFFFFFFFFu);
        ID3D11DeviceContext_OMGetBlendState(r.ctx, &bs, got, &mask);
        if (bs) t_note("OMGetBlendState after setting a NULL state returns an object");
        CHECK(got[0] == 0.25f && got[3] == 1.0f);
        SAFE_RELEASE(bs);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_sample_mask_single_sample)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        D3D11_BLEND_DESC bd; d3dh_image im;
        static const struct { UINT mask; unsigned want; } tab[] = { { 0, 0 }, { 1, 16 }, { 0xFFFFFFFFu, 16 }, { 0xFFFFFFFEu, 0 } };
        rx_blend_default(&bd);
        for (unsigned i = 0; i < A(tab); i++) {
            rx_set_bs(&r, &bd, RX_ZERO, tab[i].mask);
            rx_clear(&r, 0, 0, 0, 0);
            rx_full(&r, 0, RX_WHITE);
            if (!rx_read_rt(&r, &im)) break;
            unsigned n = rx_count32(&im, 0xFFFFFFFFu);
            if (tab[i].mask == 0xFFFFFFFEu) { if (n != tab[i].want) t_note("sample mask 0xFFFFFFFE on a 1x target drew %u pixels", n); }
            else CHECKF(n == tab[i].want, "mask %08x: %u pixels, want %u", tab[i].mask, n, tab[i].want);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

static const char PS_MRT[] = HL(
    struct O { float4 a : SV_Target0; float4 b : SV_Target1; float4 c : SV_Target2; };
    O main(float4 p : SV_Position) { O o; o.a = float4(1, 0, 0, 1); o.b = float4(0, 1, 0, 1); o.c = float4(0, 0, 1, 1); return o; });

TEST(d3d11_render_exec_multiple_render_targets_null_slot)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11PixelShader *ps = rx_ps(&r, PS_MRT);
    ID3D11Texture2D *t[3] = { NULL, NULL, NULL }; ID3D11RenderTargetView *v[3] = { NULL, NULL, NULL };
    int ok = ps != NULL;
    for (int i = 0; i < 3 && ok; i++) {
        t[i] = rx_tex2d(&r, 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_RENDER_TARGET, 0, NULL);
        if (!t[i] || FAILED(ID3D11Device_CreateRenderTargetView(r.dev, (ID3D11Resource *)t[i], NULL, &v[i]))) ok = 0;
    }
    if (ok) {
        r.w = r.h = 4;
        for (int pass = 0; pass < 2; pass++) {
            ID3D11RenderTargetView *set[3] = { v[0], pass ? NULL : v[1], v[2] };
            for (int i = 0; i < 3; i++) ID3D11DeviceContext_ClearRenderTargetView(r.ctx, v[i], RX_ZERO);
            ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 3, set, NULL);
            rx_viewport(&r, 0, 0, 4, 4, 0, 1);
            rx_full_vs(&r, 0);
            ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
            ID3D11DeviceContext_Draw(r.ctx, 4, 0);
            static const uint32_t want[3] = { RGBA8(255, 0, 0, 255), RGBA8(0, 255, 0, 255), RGBA8(0, 0, 255, 255) };
            for (int i = 0; i < 3; i++) {
                d3dh_image im;
                if (!rx_read(&r, t[i], 0, &im)) continue;
                uint32_t w = (pass && i == 1) ? 0u : want[i];
                rx_expect_all32(&im, w);
                d3dh_image_free(&im);
            }
        }
    }
    for (int i = 0; i < 3; i++) { SAFE_RELEASE(v[i]); SAFE_RELEASE(t[i]); }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_independent_blend)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        struct O { float4 a : SV_Target0; float4 b : SV_Target1; };
        O main(float4 p : SV_Position) { O o; o.a = float4(0.4, 0.4, 0.4, 0.4); o.b = float4(0.4, 0.4, 0.4, 0.4); return o; });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    ID3D11Texture2D *t[2] = { NULL, NULL }; ID3D11RenderTargetView *v[2] = { NULL, NULL };
    int ok = ps != NULL;
    for (int i = 0; i < 2 && ok; i++) {
        t[i] = rx_tex2d(&r, 2, 2, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_RENDER_TARGET, 0, NULL);
        if (!t[i] || FAILED(ID3D11Device_CreateRenderTargetView(r.dev, (ID3D11Resource *)t[i], NULL, &v[i]))) ok = 0;
    }
    if (ok) {
        r.w = r.h = 2;
        for (int indep = 0; indep < 2; indep++) {
            D3D11_BLEND_DESC bd; rx_blend_default(&bd);
            bd.IndependentBlendEnable = indep;
            bd.RenderTarget[0].BlendEnable = TRUE; bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
            bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
            /* RT1 left at its defaults (blend disabled): only honoured when IndependentBlendEnable */
            rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
            static const float dc[4] = { 51 / 255.f, 51 / 255.f, 51 / 255.f, 51 / 255.f };
            for (int i = 0; i < 2; i++) ID3D11DeviceContext_ClearRenderTargetView(r.ctx, v[i], dc);
            ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 2, v, NULL);
            rx_viewport(&r, 0, 0, 2, 2, 0, 1);
            rx_full_vs(&r, 0);
            ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
            ID3D11DeviceContext_Draw(r.ctx, 4, 0);
            d3dh_image a, b;
            if (rx_read(&r, t[0], 0, &a)) {
                CHECK(rx_close32(d3dh_image_pixel32(&a, 0, 0), RGBA8(153, 153, 153, 153), 1));  /* 0.2 + 0.4 */
                d3dh_image_free(&a);
            }
            if (rx_read(&r, t[1], 0, &b)) {
                uint32_t want = indep ? RGBA8(102, 102, 102, 102) : RGBA8(153, 153, 153, 153);
                CHECKF(rx_close32(d3dh_image_pixel32(&b, 1, 1), want, 1), "independent %d: RT1 %08x want %08x", indep, d3dh_image_pixel32(&b, 1, 1), want);
                d3dh_image_free(&b);
            }
        }
    }
    for (int i = 0; i < 2; i++) { SAFE_RELEASE(v[i]); SAFE_RELEASE(t[i]); }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_dual_source_blend)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        struct O { float4 c0 : SV_Target0; float4 c1 : SV_Target1; };
        O main(float4 p : SV_Position) { O o; o.c0 = float4(0.2, 0.2, 0.2, 0.2); o.c1 = float4(0.5, 0.25, 1.0, 0.5); return o; });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (ps && rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        static const struct { D3D11_BLEND s, d; } tab[] = {
            { D3D11_BLEND_ONE, D3D11_BLEND_SRC1_COLOR }, { D3D11_BLEND_SRC1_COLOR, D3D11_BLEND_ZERO },
            { D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC1_COLOR }, { D3D11_BLEND_SRC1_ALPHA, D3D11_BLEND_INV_SRC1_ALPHA },
        };
        const float dstc[4] = { 0.8f, 0.8f, 0.8f, 0.8f };
        for (unsigned i = 0; i < A(tab); i++) {
            D3D11_BLEND_DESC bd; d3dh_image im;
            const float s0 = 0.2f, s1[4] = { 0.5f, 0.25f, 1.0f, 0.5f };
            float want[4];
            rx_blend_default(&bd);
            bd.RenderTarget[0].BlendEnable = TRUE; bd.RenderTarget[0].SrcBlend = tab[i].s; bd.RenderTarget[0].DestBlend = tab[i].d;
            bd.RenderTarget[0].SrcBlendAlpha = tab[i].s == D3D11_BLEND_SRC1_COLOR ? D3D11_BLEND_SRC1_ALPHA : tab[i].s;
            bd.RenderTarget[0].DestBlendAlpha = tab[i].d == D3D11_BLEND_SRC1_COLOR ? D3D11_BLEND_SRC1_ALPHA : tab[i].d == D3D11_BLEND_INV_SRC1_COLOR ? D3D11_BLEND_INV_SRC1_ALPHA : tab[i].d;
            rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
            rx_clear(&r, dstc[0], dstc[1], dstc[2], dstc[3]);
            rx_full_vs(&r, 0);
            ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
            ID3D11DeviceContext_Draw(r.ctx, 4, 0);
            for (int c = 0; c < 4; c++) {
                float a = c < 3 ? s1[c] : s1[3];
                float fs = tab[i].s == D3D11_BLEND_ONE ? 1 : tab[i].s == D3D11_BLEND_SRC1_COLOR ? a : s1[3];
                float fd = tab[i].d == D3D11_BLEND_ZERO ? 0 : tab[i].d == D3D11_BLEND_SRC1_COLOR ? a : tab[i].d == D3D11_BLEND_INV_SRC1_COLOR ? 1 - a : 1 - s1[3];
                float v = s0 * fs + dstc[c] * fd; want[c] = v > 1 ? 1 : v;
            }
            if (!rx_read_rt(&r, &im)) break;
            uint32_t w = 0, got = d3dh_image_pixel32(&im, 0, 0);
            for (int c = 0; c < 4; c++) w |= (uint32_t)(int)(want[c] * 255 + 0.5f) << (8 * c);
            CHECKF(rx_close32(got, w, 1), "dual source case %u: got %08x want %08x", i, got, w);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

/* -------------------------------------------------------- RT formats ---- */
TEST(d3d11_render_exec_ps_output_integer_formats)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { DXGI_FORMAT f; const char *ps; unsigned bpp; uint8_t want[16]; } tab[] = {
        { DXGI_FORMAT_R32G32B32A32_UINT, HL(uint4 main(float4 p : SV_Position) : SV_Target { return uint4(0xDEADBEEFu, 1u, 0x80000000u, 0xFFFFFFFFu); }), 16,
          { 0xEF, 0xBE, 0xAD, 0xDE, 1, 0, 0, 0, 0, 0, 0, 0x80, 0xFF, 0xFF, 0xFF, 0xFF } },
        { DXGI_FORMAT_R32G32B32A32_SINT, HL(int4 main(float4 p : SV_Position) : SV_Target { return int4(-1, -2147483647 - 1, 2147483647, 5); }), 16,
          { 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0x80, 0xFF, 0xFF, 0xFF, 0x7F, 5, 0, 0, 0 } },
        { DXGI_FORMAT_R8G8B8A8_UINT, HL(uint4 main(float4 p : SV_Position) : SV_Target { return uint4(1, 2, 3, 255); }), 4, { 1, 2, 3, 255 } },
        { DXGI_FORMAT_R8G8B8A8_SINT, HL(int4 main(float4 p : SV_Position) : SV_Target { return int4(-1, 127, -128, 0); }), 4, { 0xFF, 0x7F, 0x80, 0 } },
        { DXGI_FORMAT_R16_UINT, HL(uint main(float4 p : SV_Position) : SV_Target { return 65535u; }), 2, { 0xFF, 0xFF } },
        { DXGI_FORMAT_R16G16_SINT, HL(int2 main(float4 p : SV_Position) : SV_Target { return int2(-2, 300); }), 4, { 0xFE, 0xFF, 0x2C, 0x01 } },
        { DXGI_FORMAT_R32_UINT, HL(uint main(float4 p : SV_Position) : SV_Target { return 123456789u; }), 4, { 0x15, 0xCD, 0x5B, 0x07 } },
        { DXGI_FORMAT_R32_FLOAT, HL(float main(float4 p : SV_Position) : SV_Target { return 3.5; }), 4, { 0, 0, 0x60, 0x40 } },
        { DXGI_FORMAT_R32_SINT, HL(int main(float4 p : SV_Position) : SV_Target { return -7; }), 4, { 0xF9, 0xFF, 0xFF, 0xFF } },
        { DXGI_FORMAT_R8G8_UINT, HL(uint2 main(float4 p : SV_Position) : SV_Target { return uint2(9, 200); }), 2, { 9, 200 } },
        { DXGI_FORMAT_R8_UINT, HL(uint main(float4 p : SV_Position) : SV_Target { return 77; }), 1, { 77 } },
        { DXGI_FORMAT_R16G16B16A16_UINT, HL(uint4 main(float4 p : SV_Position) : SV_Target { return uint4(1, 0xFFFF, 0x1234, 0); }), 8, { 1, 0, 0xFF, 0xFF, 0x34, 0x12, 0, 0 } },
        { DXGI_FORMAT_R8G8B8A8_SNORM, HL(float4 main(float4 p : SV_Position) : SV_Target { return float4(1, -1, 0, 0); }), 4, { 0x7F, 0x81, 0, 0 } },
        { DXGI_FORMAT_R16G16_SNORM, HL(float2 main(float4 p : SV_Position) : SV_Target { return float2(1, -1); }), 4, { 0xFF, 0x7F, 0x01, 0x80 } },
        { DXGI_FORMAT_R16G16_FLOAT, HL(float2 main(float4 p : SV_Position) : SV_Target { return float2(1, -2); }), 4, { 0x00, 0x3C, 0x00, 0xC0 } },
        { DXGI_FORMAT_R8G8B8A8_UNORM, HL(float4 main(float4 p : SV_Position) : SV_Target { return float4(1, 2, -3, 0.25098039); }), 4, { 255, 255, 0, 64 } },
    };
    for (unsigned i = 0; i < A(tab); i++) {
        ID3D11PixelShader *ps = rx_ps(&r, tab[i].ps);
        if (!ps) continue;
        if (rx_target_ms(&r, 3, 2, tab[i].f, 1, 0)) {
            d3dh_image im; unsigned bad = 0;
            ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 1, &r.rtv, NULL);
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
            rx_full_vs(&r, 0);
            ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
            ID3D11DeviceContext_Draw(r.ctx, 4, 0);
            if (rx_read_rt(&r, &im)) {
                for (UINT p = 0; p < 6; p++) if (memcmp(im.data + (size_t)(p / 3) * im.pitch + (size_t)(p % 3) * tab[i].bpp, tab[i].want, tab[i].bpp)) bad++;
                CHECKF(bad == 0, "format %d: %u of 6 pixels differ from the expected bytes (first byte %02x want %02x)", (int)tab[i].f, bad, im.data[0], tab[i].want[0]);
                d3dh_image_free(&im);
            }
        }
        SAFE_RELEASE(ps);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_srgb_target_encoding)
{
    rx r; if (!rx_open(&r)) return;
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB };
    for (unsigned f = 0; f < 2; f++) {
        if (!rx_target(&r, 2, 2, fm[f])) break;
        static const struct { float v; int b; int tol; } tab[] = { { 0, 0, 0 }, { 1, 255, 0 }, { 0.5f, 188, 1 }, { 0.2f, 124, 1 }, { 0.0031308f, 10, 1 }, { 0.001f, 3, 1 } };
        for (unsigned i = 0; i < A(tab); i++) {
            float c[4] = { tab[i].v, tab[i].v, tab[i].v, 64 / 255.f }; d3dh_image im;
            rx_clear(&r, 0, 0, 0, 0);
            rx_full(&r, 0, c);
            if (!rx_read_rt(&r, &im)) break;
            uint32_t p = d3dh_image_pixel32(&im, 0, 0);
            int b0 = p & 255, b1 = (p >> 8) & 255, b2 = (p >> 16) & 255, a = p >> 24;
            CHECKF(abs(b0 - tab[i].b) <= tab[i].tol && abs(b1 - tab[i].b) <= tab[i].tol && abs(b2 - tab[i].b) <= tab[i].tol,
                   "fmt %d linear %g: %02x %02x %02x want %d", (int)fm[f], tab[i].v, b0, b1, b2, tab[i].b);
            CHECKF(a == 64, "alpha is stored linearly: got %d", a);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_float_target_blend_exact)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        D3D11_BLEND_DESC bd; d3dh_image im;
        rx_blend_default(&bd);
        bd.RenderTarget[0].BlendEnable = TRUE; bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
        static const float c[4] = { 0.5f, 2.0f, -1.0f, 100.0f };
        rx_clear(&r, 0.25f, 0.25f, 0.25f, 0.25f);
        rx_full(&r, 0, c);
        if (rx_read_rt(&r, &im)) {
            static const uint16_t want[4] = { 0x3A00, 0x4080, 0xBA00, 0x5644 };  /* .75 2.25 -.75 100.25, all exact in half */
            const uint16_t *p = (const uint16_t *)im.data;
            CHECKF(p[0] == want[0], "R %04x want %04x", p[0], want[0]);
            CHECKF(p[1] == want[1], "G %04x want %04x", p[1], want[1]);
            CHECKF(p[2] == want[2], "B %04x want %04x", p[2], want[2]);
            CHECKF(p[3] == want[3], "A %04x want %04x", p[3], want[3]);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_rtv_array_slice_and_mip_selection)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11Texture2D *ta = rx_tex2d(&r, 8, 8, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_RENDER_TARGET, 0, NULL);
    ID3D11Texture2D *tm = rx_tex2d(&r, 8, 8, 3, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_RENDER_TARGET, 0, NULL);
    if (ta && tm) {
        ID3D11RenderTargetView *v = NULL; D3D11_RENDER_TARGET_VIEW_DESC rd; d3dh_image im;
        memset(&rd, 0, sizeof rd);
        rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rd.Texture2DArray.MipSlice = 0; rd.Texture2DArray.FirstArraySlice = 2; rd.Texture2DArray.ArraySize = 1;
        EXPECT_HR_OK(ID3D11Device_CreateRenderTargetView(r.dev, (ID3D11Resource *)ta, &rd, &v));
        if (v) {
            r.w = r.h = 8;
            for (int s = 0; s < 4; s++) {
                ID3D11RenderTargetView *tmp = NULL; rd.Texture2DArray.FirstArraySlice = s;
                if (SUCCEEDED(ID3D11Device_CreateRenderTargetView(r.dev, (ID3D11Resource *)ta, &rd, &tmp))) { ID3D11DeviceContext_ClearRenderTargetView(r.ctx, tmp, RX_ZERO); SAFE_RELEASE(tmp); }
            }
            ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 1, &v, NULL);
            rx_viewport(&r, 0, 0, 8, 8, 0, 1);
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
            rx_full(&r, 0, RX_WHITE);
            for (UINT s = 0; s < 4; s++) {
                if (!rx_read(&r, ta, s, &im)) continue;
                unsigned n = rx_count32(&im, 0xFFFFFFFFu);
                CHECKF(n == (s == 2 ? 64u : 0u), "slice %u: %u white pixels", s, n);
                d3dh_image_free(&im);
            }
            SAFE_RELEASE(v);
        }
        D3D11_RENDER_TARGET_VIEW_DESC md; memset(&md, 0, sizeof md);
        md.Format = DXGI_FORMAT_R8G8B8A8_UNORM; md.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; md.Texture2D.MipSlice = 1;
        EXPECT_HR_OK(ID3D11Device_CreateRenderTargetView(r.dev, (ID3D11Resource *)tm, &md, &v));
        if (v) {
            for (UINT m = 0; m < 3; m += 2) {
                ID3D11RenderTargetView *tmp = NULL; md.Texture2D.MipSlice = m;
                if (SUCCEEDED(ID3D11Device_CreateRenderTargetView(r.dev, (ID3D11Resource *)tm, &md, &tmp))) { ID3D11DeviceContext_ClearRenderTargetView(r.ctx, tmp, RX_ZERO); SAFE_RELEASE(tmp); }
            }
            ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 1, &v, NULL);
            r.w = r.h = 4;
            rx_viewport(&r, 0, 0, 4, 4, 0, 1);
            rx_full(&r, 0, RX_RED);
            static const unsigned wantn[3] = { 0, 16, 0 };
            for (UINT m = 0; m < 3; m++) {
                if (!rx_read(&r, tm, m, &im)) continue;
                unsigned n = rx_count32(&im, RGBA8(255, 0, 0, 255));
                CHECKF(im.width == (8u >> m) && n == wantn[m], "mip %u: width %u, %u red pixels", m, im.width, n);
                d3dh_image_free(&im);
            }
            SAFE_RELEASE(v);
        }
    }
    SAFE_RELEASE(ta); SAFE_RELEASE(tm);
    rx_close(&r);
}

/* -------------------------------------------------------------- MSAA ---- */
static int rx_ms_ok(rx *r, DXGI_FORMAT f, UINT n)
{
    UINT q = 0;
    HRESULT hr = ID3D11Device_CheckMultisampleQualityLevels(r->dev, f, n, &q);
    return SUCCEEDED(hr) && q > 0;
}
static int rx_resolve_read(rx *r, DXGI_FORMAT f, d3dh_image *im)
{
    ID3D11Texture2D *dst = rx_tex2d(r, r->w, r->h, 1, 1, f, 1, D3D11_BIND_RENDER_TARGET, 0, NULL);
    int ok = 0;
    if (dst) {
        ID3D11DeviceContext_ResolveSubresource(r->ctx, (ID3D11Resource *)dst, 0, (ID3D11Resource *)r->rt, 0, f);
        ok = rx_read(r, dst, 0, im);
        SAFE_RELEASE(dst);
    }
    return ok;
}

TEST(d3d11_render_exec_msaa_full_coverage_resolve_exact)
{
    rx r; if (!rx_open(&r)) return;
    static const UINT counts[] = { 2, 4, 8 };
    for (unsigned i = 0; i < 3; i++) {
        if (!rx_ms_ok(&r, DXGI_FORMAT_R8G8B8A8_UNORM, counts[i])) { t_note("no %ux MSAA for R8G8B8A8_UNORM", counts[i]); continue; }
        if (!rx_target_ms(&r, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, counts[i], 0)) continue;
        d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_clear(&r, 0, 0, 0, 0);
        { static const float c4[4] = { 40 / 255.f, 80 / 255.f, 160 / 255.f, 1 }; rx_full(&r, 0, c4); }
        if (rx_resolve_read(&r, DXGI_FORMAT_R8G8B8A8_UNORM, &im)) { rx_expect_all32(&im, RGBA8(40, 80, 160, 255)); d3dh_image_free(&im); }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_msaa_sample_mask_fraction)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { UINT n, mask, want; } tab[] = {
        { 2, 0x1, 120 }, { 2, 0x2, 120 }, { 2, 0x3, 240 }, { 2, 0, 0 },
        { 4, 0x1, 60 }, { 4, 0x5, 120 }, { 4, 0xA, 120 }, { 4, 0x7, 180 }, { 4, 0xF, 240 }, { 4, 0xFFFFFFF0u, 0 },
        { 8, 0x0F, 120 }, { 8, 0x07, 90 }, { 8, 0x81, 60 }, { 8, 0xFF, 240 },
    };
    D3D11_BLEND_DESC bd; rx_blend_default(&bd);
    for (unsigned i = 0; i < A(tab); i++) {
        if (!rx_ms_ok(&r, DXGI_FORMAT_R8G8B8A8_UNORM, tab[i].n)) continue;
        if (!rx_target_ms(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, tab[i].n, 0)) continue;
        d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_set_bs(&r, &bd, RX_ZERO, tab[i].mask);
        rx_clear(&r, 0, 0, 0, 0);
        { static const float c4[4] = { 240 / 255.f, 240 / 255.f, 240 / 255.f, 240 / 255.f }; rx_full(&r, 0, c4); }
        if (rx_resolve_read(&r, DXGI_FORMAT_R8G8B8A8_UNORM, &im)) {
            uint32_t got = d3dh_image_pixel32(&im, 1, 1);
            CHECKF(rx_close32(got, tab[i].want * 0x01010101u, 1), "%ux mask %x: got %08x want each channel %u", tab[i].n, tab[i].mask, got, tab[i].want);
            rx_expect_all32(&im, got);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_msaa_sv_sampleindex_per_sample_shading)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        float4 main(float4 p : SV_Position, uint si : SV_SampleIndex) : SV_Target { return float4(si * 40 / 255.0, 0, 0, 1); });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (ps && rx_ms_ok(&r, DXGI_FORMAT_R8G8B8A8_UNORM, 4) && rx_target_ms(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 4, 0)) {
        d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_clear(&r, 0, 0, 0, 0);
        rx_full_vs(&r, 0);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);
        if (rx_resolve_read(&r, DXGI_FORMAT_R8G8B8A8_UNORM, &im)) {
            uint32_t p = d3dh_image_pixel32(&im, 2, 2);
            CHECKF(abs((int)(p & 255) - 60) <= 1 && (p >> 24) == 255, "resolved pixel %08x, red should average samples 0,40,80,120 = 60", p);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_msaa_alpha_to_coverage_extremes)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_ms_ok(&r, DXGI_FORMAT_R8G8B8A8_UNORM, 4) && rx_target_ms(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 4, 0)) {
        D3D11_BLEND_DESC bd; rx_blend_default(&bd); bd.AlphaToCoverageEnable = TRUE;
        rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        static const struct { float a; int lo, hi; } tab[] = { { 0.0f, 0, 0 }, { 1.0f, 255, 255 }, { 0.5f, 64, 192 } };
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im; float c[4] = { 1, 0, 0, tab[i].a };
            rx_clear(&r, 0, 0, 0, 0);
            rx_full(&r, 0, c);
            if (rx_resolve_read(&r, DXGI_FORMAT_R8G8B8A8_UNORM, &im)) {
                int red = d3dh_image_pixel32(&im, 1, 1) & 255;
                CHECKF(red >= tab[i].lo && red <= tab[i].hi, "alpha %g: resolved red %d outside [%d,%d]", tab[i].a, red, tab[i].lo, tab[i].hi);
                if (i == 2) t_note("alpha-to-coverage at alpha 0.5 resolves to red %d", red);
                d3dh_image_free(&im);
            }
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_resolve_float_and_srgb_formats)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_ms_ok(&r, DXGI_FORMAT_R16G16B16A16_FLOAT, 4) && rx_target_ms(&r, 4, 4, DXGI_FORMAT_R16G16B16A16_FLOAT, 4, 0)) {
        d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_clear(&r, 0, 0, 0, 0);
        static const float c[4] = { 0.5f, 2.0f, -4.0f, 1.0f };
        rx_full(&r, 0, c);
        if (rx_resolve_read(&r, DXGI_FORMAT_R16G16B16A16_FLOAT, &im)) {
            const uint16_t *p = (const uint16_t *)im.data;
            CHECKF(p[0] == 0x3800 && p[1] == 0x4000 && p[2] == 0xC400 && p[3] == 0x3C00, "%04x %04x %04x %04x", p[0], p[1], p[2], p[3]);
            d3dh_image_free(&im);
        }
    }
    if (rx_ms_ok(&r, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4) && rx_target_ms(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, 0)) {
        d3dh_image im;
        rx_clear(&r, 0, 0, 0, 0);
        rx_full(&r, 0, RX_WHITE);
        if (rx_resolve_read(&r, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, &im)) { rx_expect_all32(&im, 0xFFFFFFFFu); d3dh_image_free(&im); }
    }
    rx_close(&r);
}

/* ------------------------------------------------------------- depth ---- */
static int cmp_pass(D3D11_COMPARISON_FUNC f, float src, float dst)
{
    switch (f) {
    case D3D11_COMPARISON_NEVER: return 0;
    case D3D11_COMPARISON_LESS: return src < dst;
    case D3D11_COMPARISON_EQUAL: return src == dst;
    case D3D11_COMPARISON_LESS_EQUAL: return src <= dst;
    case D3D11_COMPARISON_GREATER: return src > dst;
    case D3D11_COMPARISON_NOT_EQUAL: return src != dst;
    case D3D11_COMPARISON_GREATER_EQUAL: return src >= dst;
    default: return 1;
    }
}

TEST(d3d11_render_exec_depth_func_table_d32)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        static const float zs[] = { 0.25f, 0.5f, 0.75f };
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        for (int f = D3D11_COMPARISON_NEVER; f <= D3D11_COMPARISON_ALWAYS; f++) for (unsigned i = 0; i < 3; i++) {
            D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im, dm;
            rx_dss_default(&dd); dd.DepthFunc = (D3D11_COMPARISON_FUNC)f;
            rx_set_dss(&r, &dd, 0);
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 0.5f, 0);
            rx_full(&r, zs[i], RX_WHITE);
            if (!rx_read_rt(&r, &im)) continue;
            if (!rx_read(&r, r.ds, 0, &dm)) { d3dh_image_free(&im); continue; }
            int pass = cmp_pass((D3D11_COMPARISON_FUNC)f, zs[i], 0.5f);
            unsigned n = rx_count32(&im, 0xFFFFFFFFu);
            CHECKF(n == (pass ? 16u : 0u), "func %d z %g: %u pixels, expected %s", f, zs[i], n, pass ? "all" : "none");
            CHECKF(rx_depth_at(&dm, 2, 2) == (pass ? zs[i] : 0.5f), "func %d z %g: depth %g", f, zs[i], rx_depth_at(&dm, 2, 2));
            d3dh_image_free(&im); d3dh_image_free(&dm);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_depth_func_less_greater_other_depth_formats)
{
    rx r; if (!rx_open(&r)) return;
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT };
    for (unsigned f = 0; f < 3; f++) {
        if (!rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) || !rx_depth(&r, fm[f], 0)) break;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        static const struct { D3D11_COMPARISON_FUNC fn; float clear, z; int pass; } tab[] = {
            { D3D11_COMPARISON_LESS, 1.0f, 0.25f, 1 }, { D3D11_COMPARISON_LESS, 0.5f, 0.75f, 0 }, { D3D11_COMPARISON_LESS, 0.5f, 0.25f, 1 },
            { D3D11_COMPARISON_GREATER, 0.0f, 0.25f, 1 }, { D3D11_COMPARISON_GREATER, 0.5f, 0.25f, 0 }, { D3D11_COMPARISON_LESS, 0.0f, 0.0f, 0 },
            { D3D11_COMPARISON_LESS_EQUAL, 1.0f, 1.0f, 1 }, { D3D11_COMPARISON_GREATER_EQUAL, 0.0f, 0.0f, 1 }, { D3D11_COMPARISON_EQUAL, 1.0f, 1.0f, 1 },
            { D3D11_COMPARISON_EQUAL, 0.0f, 0.0f, 1 }, { D3D11_COMPARISON_NOT_EQUAL, 1.0f, 1.0f, 0 },
        };
        for (unsigned i = 0; i < A(tab); i++) {
            D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im;
            rx_dss_default(&dd); dd.DepthFunc = tab[i].fn;
            rx_set_dss(&r, &dd, 0);
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, tab[i].clear, 0);
            rx_full(&r, tab[i].z, RX_WHITE);
            if (!rx_read_rt(&r, &im)) continue;
            unsigned n = rx_count32(&im, 0xFFFFFFFFu);
            CHECKF(n == (tab[i].pass ? 16u : 0u), "dsv fmt %d case %u: %u pixels, pass=%d", (int)fm[f], i, n, tab[i].pass);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_depth_write_mask_and_disable)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        static const struct { BOOL en; D3D11_DEPTH_WRITE_MASK wm; float want_depth; unsigned want_px; } tab[] = {
            { TRUE, D3D11_DEPTH_WRITE_MASK_ALL, 0.25f, 16 }, { TRUE, D3D11_DEPTH_WRITE_MASK_ZERO, 1.0f, 16 },
            { FALSE, D3D11_DEPTH_WRITE_MASK_ALL, 1.0f, 16 }, { FALSE, D3D11_DEPTH_WRITE_MASK_ZERO, 1.0f, 16 },
        };
        for (unsigned i = 0; i < A(tab); i++) {
            D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im, dm;
            rx_dss_default(&dd); dd.DepthEnable = tab[i].en; dd.DepthWriteMask = tab[i].wm;
            rx_set_dss(&r, &dd, 0);
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
            rx_full(&r, 0.25f, RX_WHITE);
            if (!rx_read_rt(&r, &im)) continue;
            if (!rx_read(&r, r.ds, 0, &dm)) { d3dh_image_free(&im); continue; }
            CHECKF(rx_count32(&im, 0xFFFFFFFFu) == tab[i].want_px, "case %u: wrong pixel count", i);
            CHECKF(rx_depth_at(&dm, 1, 1) == tab[i].want_depth, "case %u: depth %g want %g", i, rx_depth_at(&dm, 1, 1), tab[i].want_depth);
            d3dh_image_free(&im); d3dh_image_free(&dm);
        }
        /* with depth testing disabled a farther draw still lands over a nearer one */
        D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im;
        rx_dss_default(&dd); dd.DepthEnable = FALSE;
        rx_set_dss(&r, &dd, 0);
        rx_clear(&r, 0, 0, 0, 0);
        rx_full(&r, 0.25f, RX_RED);
        rx_full(&r, 0.75f, RX_GREEN);
        if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, RGBA8(0, 255, 0, 255)); d3dh_image_free(&im); }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_depth_overdraw_nearest_wins)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_dss_default(&dd);
        rx_set_dss(&r, &dd, 0);
        rx_clear(&r, 0, 0, 0, 0);
        rx_rect(&r, 0, 0, 8, 8, 0.6f, RX_BLUE);
        rx_rect(&r, 0, 0, 4, 8, 0.3f, RX_RED);     /* nearer: wins on the left half */
        rx_rect(&r, 2, 0, 8, 8, 0.9f, RX_GREEN);   /* farther: loses everywhere */
        rx_rect(&r, 6, 0, 8, 4, 0.1f, RX_WHITE);   /* nearest top-right corner */
        if (rx_read_rt(&r, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
                uint32_t want = (x >= 6 && y < 4) ? 0xFFFFFFFFu : x < 4 ? RGBA8(255, 0, 0, 255) : RGBA8(0, 0, 255, 255);
                if (d3dh_image_pixel32(&im, x, y) != want) bad++;
            }
            CHECKF(bad == 0, "%u pixels wrong", bad);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_ps_depth_output_overrides_z)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        struct O { float4 c : SV_Target; float d : SV_Depth; };
        O main(float4 p : SV_Position) { O o; o.c = float4(1, 1, 1, 1); o.d = 0.3; return o; });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (ps && rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im, dm;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_dss_default(&dd); rx_set_dss(&r, &dd, 0);
        rx_clear(&r, 0, 0, 0, 0);
        ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 0.5f, 0);
        rx_full_vs(&r, 0.9f);      /* geometry depth 0.9 would fail LESS against 0.5 */
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);
        if (rx_read_rt(&r, &im) && rx_read(&r, r.ds, 0, &dm)) {
            unsigned n = rx_count32(&im, 0xFFFFFFFFu);
            CHECKF(n == 16, "%u pixels", n);
            CHECKF(rx_depth_at(&dm, 2, 1) == 0.3f, "depth %.9g want 0.3", rx_depth_at(&dm, 2, 1));
            d3dh_image_free(&im); d3dh_image_free(&dm);
        }
    }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_discard_writes_nothing)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        float4 main(float4 p : SV_Position) : SV_Target { if (p.x < 2.0) discard; return float4(1, 1, 1, 1); });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (ps && rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im, dm;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_dss_default(&dd); rx_set_dss(&r, &dd, 0);
        rx_clear(&r, 0, 0, 0, 0);
        rx_clear(&r, 0, 0, 0, 0);
        ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        rx_full_vs(&r, 0.25f);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);
        if (rx_read_rt(&r, &im) && rx_read(&r, r.ds, 0, &dm)) {
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) {
                int kept = x >= 2;
                CHECKF((d3dh_image_pixel32(&im, x, y) != 0) == kept, "pixel %u,%u colour %08x", x, y, d3dh_image_pixel32(&im, x, y));
                CHECKF(rx_depth_at(&dm, x, y) == (kept ? 0.25f : 1.0f), "pixel %u,%u depth %g", x, y, rx_depth_at(&dm, x, y));
            }
            d3dh_image_free(&im); d3dh_image_free(&dm);
        }
    }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_read_only_dsv_blocks_writes_and_allows_srv)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        Texture2D<float> depthtex : register(t0);
        float4 main(float4 p : SV_Position) : SV_Target { return float4(depthtex.Load(int3(p.xy, 0)), 0, 0, 1); });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (ps && rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_SHADER_RESOURCE)) {
        ID3D11DepthStencilView *ro = NULL; ID3D11ShaderResourceView *srv = NULL;
        D3D11_DEPTH_STENCIL_VIEW_DESC vd; D3D11_SHADER_RESOURCE_VIEW_DESC sd; D3D11_DEPTH_STENCIL_DESC dd;
        memset(&vd, 0, sizeof vd); vd.Format = DXGI_FORMAT_D32_FLOAT; vd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D; vd.Flags = D3D11_DSV_READ_ONLY_DEPTH;
        memset(&sd, 0, sizeof sd); sd.Format = DXGI_FORMAT_R32_FLOAT; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
        EXPECT_HR_OK(ID3D11Device_CreateDepthStencilView(r.dev, (ID3D11Resource *)r.ds, &vd, &ro));
        EXPECT_HR_OK(ID3D11Device_CreateShaderResourceView(r.dev, (ID3D11Resource *)r.ds, &sd, &srv));
        if (ro && srv) {
            d3dh_image im, dm;
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 0.25f, 0);
            rx_dss_default(&dd); dd.DepthFunc = D3D11_COMPARISON_ALWAYS;  /* writes enabled in the state */
            rx_set_dss(&r, &dd, 0);
            ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 1, &r.rtv, ro);
            ID3D11DeviceContext_PSSetShaderResources(r.ctx, 0, 1, &srv);
            rx_full_vs(&r, 0.75f);
            ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
            ID3D11DeviceContext_Draw(r.ctx, 4, 0);
            ID3D11ShaderResourceView *chk = NULL;
            ID3D11DeviceContext_PSGetShaderResources(r.ctx, 0, 1, &chk);
            CHECKF(chk == srv, "the SRV of a read-only depth view must stay bound");
            SAFE_RELEASE(chk);
            if (rx_read_rt(&r, &im)) {
                CHECKF(rx_close32(d3dh_image_pixel32(&im, 1, 1), RGBA8(64, 0, 0, 255), 1), "sampled depth %08x", d3dh_image_pixel32(&im, 1, 1));
                d3dh_image_free(&im);
            }
            if (rx_read(&r, r.ds, 0, &dm)) { CHECKF(rx_depth_at(&dm, 2, 2) == 0.25f, "depth written through a read-only view: %g", rx_depth_at(&dm, 2, 2)); d3dh_image_free(&dm); }
        }
        SAFE_RELEASE(ro); SAFE_RELEASE(srv);
    }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

/* ----------------------------------------------------------- stencil ---- */
static void st_desc(D3D11_DEPTH_STENCIL_DESC *dd, D3D11_COMPARISON_FUNC fn, D3D11_STENCIL_OP fail, D3D11_STENCIL_OP dfail, D3D11_STENCIL_OP pass)
{
    rx_dss_default(dd);
    dd->DepthFunc = D3D11_COMPARISON_LESS;
    dd->StencilEnable = TRUE;
    dd->FrontFace.StencilFunc = fn; dd->FrontFace.StencilFailOp = fail; dd->FrontFace.StencilDepthFailOp = dfail; dd->FrontFace.StencilPassOp = pass;
    dd->BackFace = dd->FrontFace;
}

TEST(d3d11_render_exec_stencil_func_table)
{
    rx r; if (!rx_open(&r)) return;
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT };
    for (unsigned k = 0; k < 2; k++) {
        if (!rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM) || !rx_depth(&r, fm[k], 0)) break;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        static const UINT refs[] = { 3, 5, 7 };
        for (int f = D3D11_COMPARISON_NEVER; f <= D3D11_COMPARISON_ALWAYS; f++) for (unsigned i = 0; i < 3; i++) {
            D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im;
            st_desc(&dd, (D3D11_COMPARISON_FUNC)f, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);
            rx_set_dss(&r, &dd, refs[i]);
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 5);
            rx_full(&r, 0.5f, RX_WHITE);
            if (!rx_read_rt(&r, &im)) continue;
            int pass = cmp_pass((D3D11_COMPARISON_FUNC)f, (float)refs[i], 5.0f);  /* reference value on the left, stored value on the right */
            CHECKF((rx_count32(&im, 0xFFFFFFFFu) == 4) == pass, "fmt %d func %d ref %u vs stored 5: expected %s", (int)fm[k], f, refs[i], pass ? "pass" : "fail");
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

/* draw once with the op under test, then probe the stencil value with an EQUAL draw */
static int stencil_probe(rx *r, int stage, D3D11_STENCIL_OP op, UINT s0, UINT ref, UINT expect)
{
    D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im; int ok = 0;
    rx_clear(r, 0, 0, 0, 0);
    ID3D11DeviceContext_ClearDepthStencilView(r->ctx, r->dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.5f, (UINT8)s0);
    if (stage == 0) st_desc(&dd, D3D11_COMPARISON_NEVER, op, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);       /* stencil fail */
    else if (stage == 1) st_desc(&dd, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_KEEP, op, D3D11_STENCIL_OP_KEEP); /* depth fail (z 0.9 vs 0.5) */
    else st_desc(&dd, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, op);                  /* pass */
    rx_set_dss(r, &dd, ref);
    rx_full(r, stage == 1 ? 0.9f : 0.1f, RX_RED);
    st_desc(&dd, D3D11_COMPARISON_EQUAL, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);
    dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    rx_set_dss(r, &dd, expect);
    rx_full(r, 0.1f, RX_GREEN);
    if (rx_read_rt(r, &im)) {
        ok = d3dh_image_pixel32(&im, 0, 0) == RGBA8(0, 255, 0, 255);
        d3dh_image_free(&im);
    }
    return ok;
}

TEST(d3d11_render_exec_stencil_ops_results)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { D3D11_STENCIL_OP op; UINT s0, ref, expect; const char *name; } tab[] = {
        { D3D11_STENCIL_OP_KEEP, 5, 7, 5, "KEEP" }, { D3D11_STENCIL_OP_ZERO, 5, 7, 0, "ZERO" }, { D3D11_STENCIL_OP_REPLACE, 5, 7, 7, "REPLACE" },
        { D3D11_STENCIL_OP_INCR_SAT, 5, 7, 6, "INCR_SAT" }, { D3D11_STENCIL_OP_DECR_SAT, 5, 7, 4, "DECR_SAT" }, { D3D11_STENCIL_OP_INVERT, 5, 7, 0xFA, "INVERT" },
        { D3D11_STENCIL_OP_INCR, 5, 7, 6, "INCR" }, { D3D11_STENCIL_OP_DECR, 5, 7, 4, "DECR" },
        { D3D11_STENCIL_OP_INCR_SAT, 255, 7, 255, "INCR_SAT@255" }, { D3D11_STENCIL_OP_INCR, 255, 7, 0, "INCR@255 wraps" },
        { D3D11_STENCIL_OP_DECR_SAT, 0, 7, 0, "DECR_SAT@0" }, { D3D11_STENCIL_OP_DECR, 0, 7, 255, "DECR@0 wraps" },
        { D3D11_STENCIL_OP_INVERT, 0, 7, 255, "INVERT@0" }, { D3D11_STENCIL_OP_REPLACE, 5, 255, 255, "REPLACE 255" }, { D3D11_STENCIL_OP_REPLACE, 5, 0, 0, "REPLACE 0" },
    };
    static const char *stage_name[3] = { "StencilFailOp", "StencilDepthFailOp", "StencilPassOp" };
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D24_UNORM_S8_UINT, 0)) {
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        for (int stage = 0; stage < 3; stage++) for (unsigned i = 0; i < A(tab); i++) {
            int ok = stencil_probe(&r, stage, tab[i].op, tab[i].s0, tab[i].ref, tab[i].expect);
            CHECKF(ok, "%s %s from %u ref %u: stencil is not %u afterwards", stage_name[stage], tab[i].name, tab[i].s0, tab[i].ref, tab[i].expect);
        }
        /* negative control: probing for the wrong value must not match */
        int bad = stencil_probe(&r, 2, D3D11_STENCIL_OP_REPLACE, 5, 7, 8);
        CHECK(!bad);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_stencil_two_sided_ops)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D24_UNORM_S8_UINT, 0)) {
        for (int ccw = 0; ccw < 2; ccw++) for (int rev = 0; rev < 2; rev++) {
            D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im;
            int is_front = rev ? ccw : !ccw;
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, ccw, TRUE, FALSE);
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 20);
            st_desc(&dd, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_INCR);
            dd.BackFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
            rx_set_dss(&r, &dd, 0);
            if (rev) rx_rect(&r, 14, 2, 2, 14, 0.5f, RX_RED); else rx_rect(&r, 2, 2, 14, 14, 0.5f, RX_RED);
            /* probe: front -> 21, back -> 19 */
            st_desc(&dd, D3D11_COMPARISON_EQUAL, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);
            dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
            rx_set_dss(&r, &dd, is_front ? 21 : 19);
            rx_rect(&r, 14, 2, 2, 14, 0.5f, RX_GREEN);
            if (!rx_read_rt(&r, &im)) continue;
            /* the probe quad is reversed; its facing differs, but EQUAL ops are KEEP on both sides so only the value matters */
            CHECKF(d3dh_image_pixel32(&im, 8, 4) == RGBA8(0, 255, 0, 255) && d3dh_image_pixel32(&im, 8, 12) == RGBA8(0, 255, 0, 255),
                   "ccw %d rev %d front %d: %08x %08x", ccw, rev, is_front, d3dh_image_pixel32(&im, 8, 4), d3dh_image_pixel32(&im, 8, 12));
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_stencil_masks)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D24_UNORM_S8_UINT, 0)) {
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im;
        /* write mask: (old & ~mask) | (ref & mask) */
        static const struct { UINT8 wm; UINT s0, ref, expect; } wtab[] = { { 0x0F, 0x55, 0xAA, 0x5A }, { 0xF0, 0x55, 0xAA, 0xA5 }, { 0x00, 0x55, 0xAA, 0x55 }, { 0xFF, 0x55, 0xAA, 0xAA }, { 0x81, 0x7E, 0xFF, 0xFF } };
        for (unsigned i = 0; i < A(wtab); i++) {
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, (UINT8)wtab[i].s0);
            st_desc(&dd, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_REPLACE);
            dd.StencilWriteMask = wtab[i].wm;
            rx_set_dss(&r, &dd, wtab[i].ref);
            rx_full(&r, 0.5f, RX_RED);
            st_desc(&dd, D3D11_COMPARISON_EQUAL, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);
            dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
            rx_set_dss(&r, &dd, wtab[i].expect);
            rx_full(&r, 0.5f, RX_GREEN);
            if (rx_read_rt(&r, &im)) { CHECKF(d3dh_image_pixel32(&im, 0, 0) == RGBA8(0, 255, 0, 255), "write mask %02x: stencil is not %02x", wtab[i].wm, wtab[i].expect); d3dh_image_free(&im); }
        }
        /* read mask: (ref & rm) FUNC (stencil & rm) */
        static const struct { UINT8 rm; UINT s0, ref; int pass; } rtab[] = { { 0x0F, 0x55, 0xA5, 1 }, { 0xF0, 0x55, 0xA5, 0 }, { 0xFF, 0x55, 0x55, 1 }, { 0x00, 0x55, 0xAA, 1 }, { 0x01, 0x55, 0x00, 0 } };
        for (unsigned i = 0; i < A(rtab); i++) {
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, (UINT8)rtab[i].s0);
            st_desc(&dd, D3D11_COMPARISON_EQUAL, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);
            dd.StencilReadMask = rtab[i].rm;
            rx_set_dss(&r, &dd, rtab[i].ref);
            rx_full(&r, 0.5f, RX_WHITE);
            if (rx_read_rt(&r, &im)) { CHECKF((d3dh_image_pixel32(&im, 0, 0) != 0) == rtab[i].pass, "read mask %02x ref %02x stencil %02x", rtab[i].rm, rtab[i].ref, rtab[i].s0); d3dh_image_free(&im); }
        }
        /* StencilEnable FALSE ignores the stencil buffer and leaves it untouched */
        rx_clear(&r, 0, 0, 0, 0);
        ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 9);
        st_desc(&dd, D3D11_COMPARISON_NEVER, D3D11_STENCIL_OP_ZERO, D3D11_STENCIL_OP_ZERO, D3D11_STENCIL_OP_ZERO);
        dd.StencilEnable = FALSE;
        rx_set_dss(&r, &dd, 1);
        rx_full(&r, 0.5f, RX_RED);
        st_desc(&dd, D3D11_COMPARISON_EQUAL, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);
        rx_set_dss(&r, &dd, 9);
        rx_full(&r, 0.1f, RX_GREEN);
        if (rx_read_rt(&r, &im)) { CHECK(d3dh_image_pixel32(&im, 0, 0) == RGBA8(0, 255, 0, 255)); d3dh_image_free(&im); }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_dsv_flags_are_independent)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D24_UNORM_S8_UINT, 0)) {
        D3D11_DEPTH_STENCIL_DESC dd; d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        /* depth cleared to 0.25 and stencil to 9; then clear only stencil (to 4): depth must stay 0.25 */
        ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.25f, 9);
        ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_STENCIL, 0.9f, 4);
        st_desc(&dd, D3D11_COMPARISON_EQUAL, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);
        dd.DepthFunc = D3D11_COMPARISON_GREATER;     /* z 0.5 > 0.25 passes only if depth stayed 0.25 */
        rx_set_dss(&r, &dd, 4);
        rx_clear(&r, 0, 0, 0, 0);
        rx_full(&r, 0.5f, RX_WHITE);
        if (rx_read_rt(&r, &im)) { CHECKF(rx_count32(&im, 0xFFFFFFFFu) == 4, "stencil-only clear must preserve depth and set stencil 4"); d3dh_image_free(&im); }
        /* clear only depth to 0.75 (stencil stays 4) */
        ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 0.75f, 77);
        dd.DepthFunc = D3D11_COMPARISON_LESS;
        rx_set_dss(&r, &dd, 4);
        rx_clear(&r, 0, 0, 0, 0);
        rx_full(&r, 0.5f, RX_WHITE);
        if (rx_read_rt(&r, &im)) { CHECKF(rx_count32(&im, 0xFFFFFFFFu) == 4, "depth-only clear must preserve stencil 4 and set depth 0.75"); d3dh_image_free(&im); }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_dss_state_roundtrip_with_ref)
{
    rx r; if (!rx_open(&r)) return;
    D3D11_DEPTH_STENCIL_DESC dd; ID3D11DepthStencilState *got = NULL; UINT ref = 0;
    rx_dss_default(&dd);
    rx_set_dss(&r, &dd, 0x5A);
    ID3D11DeviceContext_OMGetDepthStencilState(r.ctx, &got, &ref);
    CHECK(got == r.dss);
    EXPECT_EQ(ref, 0x5A);
    SAFE_RELEASE(got);
    ID3D11DeviceContext_OMSetDepthStencilState(r.ctx, NULL, 3);
    ID3D11DeviceContext_OMGetDepthStencilState(r.ctx, &got, &ref);
    if (got) t_note("OMGetDepthStencilState after setting a NULL state returns an object");
    EXPECT_EQ(ref, 3);
    SAFE_RELEASE(got);
    rx_close(&r);
}

/* ---------------------------------------------------------- queries ---- */
TEST(d3d11_render_exec_occlusion_query_counts_samples)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        ID3D11Query *q = rx_make_query(&r, D3D11_QUERY_OCCLUSION), *qp = rx_make_query(&r, D3D11_QUERY_OCCLUSION_PREDICATE);
        D3D11_DEPTH_STENCIL_DESC dd; rx_dss_default(&dd);
        rx_set_dss(&r, &dd, 0);
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        if (q && qp) {
            UINT64 n = 0; BOOL pred = 99; HRESULT hr;
            ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)q);
            ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)qp);
            rx_rect(&r, 4, 4, 12, 12, 0.5f, RX_WHITE);
            ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)q);
            ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)qp);
            hr = rx_query(&r, q, &n, sizeof n); EXPECT_HR(hr, S_OK);
            CHECKF(n == 64, "visible 8x8 rectangle: %llu samples", (unsigned long long)n);
            hr = rx_query(&r, qp, &pred, sizeof pred); EXPECT_HR(hr, S_OK);
            CHECK(pred == TRUE);
            /* same rectangle behind the first: depth test rejects everything */
            ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)q);
            ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)qp);
            rx_rect(&r, 4, 4, 12, 12, 0.75f, RX_WHITE);
            ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)q);
            ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)qp);
            n = 99; pred = 99;
            hr = rx_query(&r, q, &n, sizeof n); EXPECT_HR(hr, S_OK);
            CHECKF(n == 0, "fully occluded draw: %llu samples", (unsigned long long)n);
            hr = rx_query(&r, qp, &pred, sizeof pred); EXPECT_HR(hr, S_OK);
            CHECK(pred == FALSE);
            /* partially visible: new rectangle (8..16, 8..16) at the same depth fails LESS where the first one wrote, 64 - 16 = 48 remain */
            ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)q);
            rx_rect(&r, 8, 8, 16, 16, 0.5f, RX_WHITE);
            ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)q);
            hr = rx_query(&r, q, &n, sizeof n); EXPECT_HR(hr, S_OK);
            CHECKF(n == 48, "partially occluded: %llu samples want 48", (unsigned long long)n);
            /* draws outside Begin/End are not counted; empty bracket yields 0 */
            ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)q);
            ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)q);
            n = 99; hr = rx_query(&r, q, &n, sizeof n); EXPECT_HR(hr, S_OK);
            CHECK(n == 0);
        }
        SAFE_RELEASE(q); SAFE_RELEASE(qp);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_pipeline_statistics_basic_draw)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        ID3D11Query *q = rx_make_query(&r, D3D11_QUERY_PIPELINE_STATISTICS);
        if (q) {
            D3D11_QUERY_DATA_PIPELINE_STATISTICS st; HRESULT hr;
            memset(&st, 0xCD, sizeof st);
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
            ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)q);
            rx_rect(&r, 0, 0, 16, 16, 0, RX_WHITE);
            ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)q);
            hr = rx_query(&r, q, &st, sizeof st);
            EXPECT_HR(hr, S_OK);
            if (hr == S_OK) {
                CHECKF(st.IAVertices == 4, "IAVertices %llu", (unsigned long long)st.IAVertices);
                CHECKF(st.IAPrimitives == 2, "IAPrimitives %llu", (unsigned long long)st.IAPrimitives);
                CHECKF(st.VSInvocations >= 4 || st.VSInvocations == 0, "VSInvocations %llu", (unsigned long long)st.VSInvocations);
                CHECK(st.GSInvocations == 0 && st.GSPrimitives == 0);
                CHECK(st.HSInvocations == 0 && st.DSInvocations == 0 && st.CSInvocations == 0);
                CHECKF(st.PSInvocations >= 256, "PSInvocations %llu", (unsigned long long)st.PSInvocations);
                t_note("stats: VS %llu CInv %llu CPrim %llu PS %llu", (unsigned long long)st.VSInvocations, (unsigned long long)st.CInvocations, (unsigned long long)st.CPrimitives, (unsigned long long)st.PSInvocations);
            }
        }
        SAFE_RELEASE(q);
    }
    rx_close(&r);
}

/* ------------------------------------------------------ SRV hazards ---- */
TEST(d3d11_render_exec_srv_unbound_when_resource_becomes_render_target)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11Texture2D *t = rx_tex2d(&r, 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, NULL);
    ID3D11RenderTargetView *rtv = NULL; ID3D11ShaderResourceView *srv = NULL;
    if (t) {
        EXPECT_HR_OK(ID3D11Device_CreateRenderTargetView(r.dev, (ID3D11Resource *)t, NULL, &rtv));
        EXPECT_HR_OK(ID3D11Device_CreateShaderResourceView(r.dev, (ID3D11Resource *)t, NULL, &srv));
    }
    if (rtv && srv) {
        ID3D11ShaderResourceView *got = NULL;
        ID3D11DeviceContext_PSSetShaderResources(r.ctx, 0, 1, &srv);
        ID3D11DeviceContext_VSSetShaderResources(r.ctx, 3, 1, &srv);
        ID3D11DeviceContext_PSGetShaderResources(r.ctx, 0, 1, &got);
        CHECK(got == srv); SAFE_RELEASE(got);
        ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 1, &rtv, NULL);
        ID3D11DeviceContext_PSGetShaderResources(r.ctx, 0, 1, &got);
        CHECKF(got == NULL, "binding the resource as a render target must unbind it from the PS slot");
        SAFE_RELEASE(got);
        ID3D11DeviceContext_VSGetShaderResources(r.ctx, 3, 1, &got);
        CHECKF(got == NULL, "...and from the VS slot");
        SAFE_RELEASE(got);
        /* other direction: observed only */
        ID3D11DeviceContext_PSSetShaderResources(r.ctx, 1, 1, &srv);
        ID3D11DeviceContext_PSGetShaderResources(r.ctx, 1, 1, &got);
        t_note("binding an SRV of the bound render target afterwards leaves slot %s", got ? "set" : "empty");
        SAFE_RELEASE(got);
    }
    SAFE_RELEASE(rtv); SAFE_RELEASE(srv); SAFE_RELEASE(t);
    rx_close(&r);
}
