/* Texture instruction semantics (Load / Sample* / Gather / GetDimensions / LOD) and pixel-shader derivatives. */
#include "hse.h"

#define T_ENDIAN_NOTE
typedef struct { ID3D11Texture2D *tex; ID3D11ShaderResourceView *srv; } tx;
static void tx_free(tx *t) { SAFE_RELEASE(t->srv); SAFE_RELEASE(t->tex); }

/* 4x4 R32_FLOAT, 3 mips: level 0 = y*4+x+1 (1..16); level 1 = 101..104; level 2 = 200 */
static tx make_ramp_tex(void) {
    static float l0[16], l1[4], l2[1];
    for (int i = 0; i < 16; i++) l0[i] = (float)(i + 1);
    for (int i = 0; i < 4; i++) l1[i] = (float)(101 + i);
    l2[0] = 200.0f;
    D3D11_SUBRESOURCE_DATA sd[3] = { { l0, 16, 0 }, { l1, 8, 0 }, { l2, 4, 0 } };
    tx t = { 0 };
    t.tex = hse_tex2d(4, 4, 3, 1, DXGI_FORMAT_R32_FLOAT, D3D11_BIND_SHADER_RESOURCE, sd);
    if (t.tex) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)t.tex, NULL, &t.srv);
    return t;
}
static float ramp0(int x, int y) { return (float)(y * 4 + x + 1); }

/* Generic query kernel: row i = p (float4) in, EXPR evaluated -> float4 out. `decl` declares T (t1) and samplers (s0, s1). */
static int tex_query_at(const char *file, int line, const char *decl, const char *expr, ID3D11ShaderResourceView *srv, ID3D11SamplerState *s0, ID3D11SamplerState *s1,
                        const float in[][4], int n, float out[][4]) {
    char src[3072];
    snprintf(src, sizeof src, "%s\nByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) { uint gi = id.x; float4 p = asfloat(I.Load4(gi * 16)); float4 r = %s; O.Store4(gi * 16, asuint(r)); }", decl, expr);
    int groups = (n + 63) / 64;
    float (*inb)[4] = calloc(groups * 64, 16), (*outb)[4] = calloc(groups * 64, 16);
    memcpy(inb, in, n * 16);
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = src; c.in = inb; c.inbytes = groups * 64 * 16; c.outbytes = groups * 64 * 16; c.gx = groups; c.out = outb;
    c.srv[0] = srv; c.smp[0] = s0; c.smp[1] = s1;
    int ok = hse_dispatch_at(file, line, &c);
    if (ok) memcpy(out, outb, n * 16);
    free(inb); free(outb);
    return ok;
}
#define TQ(decl, expr, srv, s0, s1, in, n, out) tex_query_at(__FILE__, __LINE__, decl, expr, srv, s0, s1, in, n, out)
#define DECL_F "Texture2D<float4> T : register(t1); SamplerState S : register(s0); SamplerState S1 : register(s1);"
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

TEST(hlsl_semantics_exec_tex_load_every_texel) {
    HSE_NEED();
    tx t = make_ramp_tex();
    float in[16][4], out[16][4];
    for (int i = 0; i < 16; i++) { in[i][0] = (float)(i & 3); in[i][1] = (float)(i >> 2); in[i][2] = 0; in[i][3] = 0; }
    if (TQ(DECL_F, "T.Load(int3(p.x, p.y, p.z))", t.srv, NULL, NULL, in, 16, out))
        for (int i = 0; i < 16; i++) {
            CHECKF(out[i][0] == ramp0(i & 3, i >> 2), "texel %d got %g", i, out[i][0]);
            CHECKF(out[i][1] == 0.0f && out[i][2] == 0.0f && out[i][3] == 1.0f, "missing components of an R32_FLOAT load must be (0,0,1): %g %g %g", out[i][1], out[i][2], out[i][3]);
        }
    tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_load_mip_levels) {
    HSE_NEED();
    tx t = make_ramp_tex();
    float in[5][4] = { { 0, 0, 1, 0 }, { 1, 0, 1, 0 }, { 0, 1, 1, 0 }, { 1, 1, 1, 0 }, { 0, 0, 2, 0 } }, out[5][4];
    static const float want[5] = { 101, 102, 103, 104, 200 };
    if (TQ(DECL_F, "T.Load(int3(p.x, p.y, p.z))", t.srv, NULL, NULL, in, 5, out))
        for (int i = 0; i < 5; i++) CHECKF(out[i][0] == want[i], "row %d got %g want %g", i, out[i][0], want[i]);
    tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_load_out_of_bounds_returns_zero) {
    HSE_NEED();
    tx t = make_ramp_tex();
    float in[8][4] = { { 4, 0, 0, 0 }, { 0, 4, 0, 0 }, { -1, 0, 0, 0 }, { 0, -1, 0, 0 }, { 100, 100, 0, 0 }, { 0, 0, 3, 0 }, { 2, 0, 1, 0 }, { 0, 2, 1, 0 } }, out[8][4];
    if (TQ(DECL_F, "T.Load(int3(p.x, p.y, p.z))", t.srv, NULL, NULL, in, 8, out))
        for (int i = 0; i < 8; i++) CHECKF(out[i][0] == 0.0f, "row %d (%g,%g,mip %g) got %g, out-of-range loads return 0", i, in[i][0], in[i][1], in[i][2], out[i][0]);
    tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_load_with_offset) {
    HSE_NEED();
    tx t = make_ramp_tex();
    float in[4][4] = { { 0, 0, 0, 0 }, { 1, 1, 0, 0 }, { 2, 2, 0, 0 }, { 3, 3, 0, 0 } }, out[4][4];
    if (TQ(DECL_F, "T.Load(int3(p.x, p.y, p.z), int2(1, -1))", t.srv, NULL, NULL, in, 4, out)) {
        CHECK(out[0][0] == 0.0f);                       /* (1,-1) is outside */
        CHECK(out[1][0] == ramp0(2, 0)); CHECK(out[2][0] == ramp0(3, 1)); CHECK(out[3][0] == 0.0f);   /* (4,2) outside */
    }
    tx_free(&t);
}

/* ---- SampleLevel with point filtering ----------------------------------------------------------------------------------- */
static ID3D11SamplerState *smp_point(D3D11_TEXTURE_ADDRESS_MODE am) { return hse_sampler(D3D11_FILTER_MIN_MAG_MIP_POINT, am, 0, 0); }
static ID3D11SamplerState *smp_linear(D3D11_TEXTURE_ADDRESS_MODE am) { return hse_sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, am, 0, 0); }
TEST(hlsl_semantics_exec_tex_samplelevel_point_texel_centers) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    float in[16][4], out[16][4];
    for (int i = 0; i < 16; i++) { in[i][0] = ((i & 3) + 0.5f) / 4.0f; in[i][1] = ((i >> 2) + 0.5f) / 4.0f; in[i][2] = 0; in[i][3] = 0; }
    if (TQ(DECL_F, "T.SampleLevel(S, p.xy, p.z)", t.srv, s, NULL, in, 16, out))
        for (int i = 0; i < 16; i++) CHECKF(out[i][0] == ramp0(i & 3, i >> 2), "texel %d got %g", i, out[i][0]);
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_samplelevel_point_mip_selection) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    float in[8][4] = { { .125f, .125f, 0 }, { .125f, .125f, 1 }, { .125f, .125f, 2 }, { .875f, .875f, 1 }, { .125f, .125f, 0.4f }, { .125f, .125f, 0.6f }, { .125f, .125f, 5 }, { .125f, .125f, 1.6f } }, out[8][4];
    static const float want[8] = { 1, 101, 200, 104, 1, 101, 200, 200 };
    if (TQ(DECL_F, "T.SampleLevel(S, p.xy, p.z)", t.srv, s, NULL, in, 8, out))
        for (int i = 0; i < 8; i++) CHECKF(out[i][0] == want[i], "lod %g got %g want %g", in[i][2], out[i][0], want[i]);
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_samplelevel_linear_midpoints) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_linear(D3D11_TEXTURE_ADDRESS_CLAMP);
    /* u=.5 is the boundary between texel columns 1 and 2; v=.5 between rows 1 and 2 */
    float in[5][4] = { { .5f, .375f, 0 }, { .375f, .5f, 0 }, { .5f, .5f, 0 }, { .5f, .5f, 1 }, { .5f, .5f, 0.5f } }, out[5][4];
    if (TQ(DECL_F, "T.SampleLevel(S, p.xy, p.z)", t.srv, s, NULL, in, 5, out)) {
        CHECKF(NEAR_(out[0][0], (ramp0(1, 1) + ramp0(2, 1)) / 2, 0.02f), "horizontal midpoint got %g", out[0][0]);
        CHECKF(NEAR_(out[1][0], (ramp0(1, 1) + ramp0(1, 2)) / 2, 0.02f), "vertical midpoint got %g", out[1][0]);
        CHECKF(NEAR_(out[2][0], (ramp0(1, 1) + ramp0(2, 1) + ramp0(1, 2) + ramp0(2, 2)) / 4, 0.02f), "bilinear centre got %g", out[2][0]);
        CHECKF(NEAR_(out[3][0], 102.5f, 0.1f), "mip 1 bilinear centre got %g", out[3][0]);
        CHECKF(NEAR_(out[4][0], (8.5f + 102.5f) / 2, 0.2f), "trilinear halfway got %g", out[4][0]);
        t_note("linear filter results %g %g %g %g %g", out[0][0], out[1][0], out[2][0], out[3][0], out[4][0]);
    }
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_address_modes_point) {
    HSE_NEED();
    tx t = make_ramp_tex();
    D3D11_SAMPLER_DESC d; memset(&d, 0, sizeof d);
    d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; d.MaxAnisotropy = 1; d.ComparisonFunc = D3D11_COMPARISON_NEVER; d.MaxLOD = D3D11_FLOAT32_MAX;
    d.BorderColor[0] = 77.0f; d.BorderColor[1] = 0; d.BorderColor[2] = 0; d.BorderColor[3] = 0;
    static const D3D11_TEXTURE_ADDRESS_MODE modes[5] = { D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_MIRROR_ONCE };
    /* u values (v fixed in row 0): 1.125 -> wrap texel 0, mirror texel 3, clamp 3, border 77, mirror_once 3;  -0.125: wrap 3, mirror 0, clamp 0, border 77, mirror_once 0 */
    static const float us[2] = { 1.125f, -0.125f };
    static const int want_tex[2][5] = { { 0, 3, 3, -1, 3 }, { 3, 0, 0, -1, 0 } };
    for (int m = 0; m < 5; m++) {
        ID3D11SamplerState *s = NULL;
        d.AddressU = d.AddressV = d.AddressW = modes[m];
        if (hse.dev) ID3D11Device_CreateSamplerState(hse.dev, &d, &s);
        float in[2][4] = { { us[0], .125f, 0, 0 }, { us[1], .125f, 0, 0 } }, out[2][4];
        if (TQ(DECL_F, "T.SampleLevel(S, p.xy, 0)", t.srv, s, NULL, in, 2, out))
            for (int k = 0; k < 2; k++) {
                float want = want_tex[k][m] < 0 ? 77.0f : ramp0(want_tex[k][m], 0);
                CHECKF(out[k][0] == want, "mode %d u=%g got %g want %g", modes[m], us[k], out[k][0], want);
            }
        SAFE_RELEASE(s);
    }
    tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_sample_offset_shifts_texels) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    float in[3][4] = { { .125f, .125f, 0 }, { .375f, .375f, 0 }, { .875f, .125f, 0 } }, out[3][4];
    if (TQ(DECL_F, "T.SampleLevel(S, p.xy, p.z, int2(1, 2))", t.srv, s, NULL, in, 3, out)) {
        CHECK(out[0][0] == ramp0(1, 2)); CHECK(out[1][0] == ramp0(2, 3)); CHECK(out[2][0] == ramp0(3, 2));   /* clamped at the right edge */
    }
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_samplegrad_selects_mip) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    /* gradient of g uv units per pixel on a 4-texel texture: lod = log2(4 g) */
    float in[5][4] = { { .125f, .125f, 0.25f, 0 }, { .125f, .125f, 0.5f, 0 }, { .125f, .125f, 1.0f, 0 }, { .125f, .125f, 8.0f, 0 }, { .125f, .125f, 0.0625f, 0 } }, out[5][4];
    static const float want[5] = { 1, 101, 200, 200, 1 };
    if (TQ(DECL_F, "T.SampleGrad(S, p.xy, float2(p.z, 0), float2(0, p.z))", t.srv, s, NULL, in, 5, out))
        for (int i = 0; i < 5; i++) CHECKF(out[i][0] == want[i], "gradient %g got %g want %g", in[i][2], out[i][0], want[i]);
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_samplegrad_uses_larger_axis) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    float in[2][4] = { { .125f, .125f, 0.5f, 0.0625f }, { .125f, .125f, 0.0625f, 0.5f } }, out[2][4];
    if (TQ(DECL_F, "T.SampleGrad(S, p.xy, float2(p.z, 0), float2(0, p.w))", t.srv, s, NULL, in, 2, out)) { CHECK(out[0][0] == 101.0f); CHECK(out[1][0] == 101.0f); }
    SAFE_RELEASE(s); tx_free(&t);
}

/* ---- Gather -------------------------------------------------------------------------------------------------------------------- */
TEST(hlsl_semantics_exec_tex_gather_component_order) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    float in[2][4] = { { .5f, .5f, 0, 0 }, { .75f, .25f, 0, 0 } }, out[2][4];
    if (TQ(DECL_F, "T.Gather(S, p.xy)", t.srv, s, NULL, in, 2, out)) {
        /* x = (i0,j1) y = (i1,j1) z = (i1,j0) w = (i0,j0) */
        CHECK(out[0][0] == ramp0(1, 2) && out[0][1] == ramp0(2, 2) && out[0][2] == ramp0(2, 1) && out[0][3] == ramp0(1, 1));
        CHECK(out[1][0] == ramp0(2, 1) && out[1][1] == ramp0(3, 1) && out[1][2] == ramp0(3, 0) && out[1][3] == ramp0(2, 0));
        t_note("gather(.5,.5) = %g %g %g %g", out[0][0], out[0][1], out[0][2], out[0][3]);
    }
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_gather_ignores_filter_mode) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_linear(D3D11_TEXTURE_ADDRESS_CLAMP);
    float in[1][4] = { { .5f, .5f, 0, 0 } }, out[1][4];
    if (TQ(DECL_F, "T.Gather(S, p.xy)", t.srv, s, NULL, in, 1, out))
        CHECK(out[0][0] == ramp0(1, 2) && out[0][1] == ramp0(2, 2) && out[0][2] == ramp0(2, 1) && out[0][3] == ramp0(1, 1));
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_gather_offset) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    float in[1][4] = { { .5f, .5f, 0, 0 } }, out[1][4];
    if (TQ(DECL_F, "T.Gather(S, p.xy, int2(1, 0))", t.srv, s, NULL, in, 1, out))
        CHECK(out[0][0] == ramp0(2, 2) && out[0][1] == ramp0(3, 2) && out[0][2] == ramp0(3, 1) && out[0][3] == ramp0(2, 1));
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_gather_edge_clamp_vs_wrap) {
    HSE_NEED();
    tx t = make_ramp_tex(); ID3D11SamplerState *c = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP), *w = smp_point(D3D11_TEXTURE_ADDRESS_WRAP);
    float in[1][4] = { { .9375f, .5f, 0, 0 } }, oc[1][4], ow[1][4];
    if (TQ(DECL_F, "T.Gather(S, p.xy)", t.srv, c, NULL, in, 1, oc)) CHECK(oc[0][0] == ramp0(3, 2) && oc[0][1] == ramp0(3, 2) && oc[0][2] == ramp0(3, 1) && oc[0][3] == ramp0(3, 1));
    if (TQ(DECL_F, "T.Gather(S, p.xy)", t.srv, w, NULL, in, 1, ow)) CHECK(ow[0][0] == ramp0(3, 2) && ow[0][1] == ramp0(0, 2) && ow[0][2] == ramp0(0, 1) && ow[0][3] == ramp0(3, 1));
    SAFE_RELEASE(c); SAFE_RELEASE(w); tx_free(&t);
}

/* ---- comparison sampling ----------------------------------------------------------------------------------------------------------- */
static tx make_cmp_tex(void) {
    float l0[16];
    for (int i = 0; i < 16; i++) l0[i] = (float)(i + 1);
    D3D11_SUBRESOURCE_DATA sd = { l0, 16, 0 };
    tx t = { 0 };
    t.tex = hse_tex2d(4, 4, 1, 1, DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_SHADER_RESOURCE, &sd);
    if (t.tex) {
        D3D11_SHADER_RESOURCE_VIEW_DESC v; memset(&v, 0, sizeof v);
        v.Format = DXGI_FORMAT_R32_FLOAT; v.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; v.Texture2D.MipLevels = 1;
        ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)t.tex, &v, &t.srv);
    }
    return t;
}
static int cmp_supported(void) {
    UINT s = 0;
    if (FAILED(ID3D11Device_CheckFormatSupport(hse.dev, DXGI_FORMAT_R32_FLOAT, &s))) return 0;
    return !!(s & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON);
}
#define DECL_C "Texture2D<float> T : register(t1); SamplerComparisonState S : register(s0);"
TEST(hlsl_semantics_exec_tex_samplecmp_point_less_equal) {
    HSE_NEED();
    if (hse.dev && !cmp_supported()) { t_skip("R32_FLOAT comparison sampling unsupported"); return; }
    tx t = make_cmp_tex(); ID3D11SamplerState *s = hse_sampler(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_COMPARISON_LESS_EQUAL, 0);
    /* texel (1,1) holds 6: pass when reference <= 6 */
    float in[5][4] = { { .375f, .375f, 5.0f, 0 }, { .375f, .375f, 6.0f, 0 }, { .375f, .375f, 6.5f, 0 }, { .375f, .375f, 0.0f, 0 }, { .375f, .375f, 1000.0f, 0 } }, out[5][4];
    static const float want[5] = { 1, 1, 0, 1, 0 };
    if (TQ(DECL_C, "T.SampleCmpLevelZero(S, p.xy, p.z)", t.srv, s, NULL, in, 5, out))
        for (int i = 0; i < 5; i++) CHECKF(out[i][0] == want[i], "ref %g got %g want %g", in[i][2], out[i][0], want[i]);
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_samplecmp_all_functions) {
    HSE_NEED();
    if (hse.dev && !cmp_supported()) { t_skip("R32_FLOAT comparison sampling unsupported"); return; }
    tx t = make_cmp_tex();
    static const D3D11_COMPARISON_FUNC fn[8] = { D3D11_COMPARISON_NEVER, D3D11_COMPARISON_LESS, D3D11_COMPARISON_EQUAL, D3D11_COMPARISON_LESS_EQUAL,
                                                 D3D11_COMPARISON_GREATER, D3D11_COMPARISON_NOT_EQUAL, D3D11_COMPARISON_GREATER_EQUAL, D3D11_COMPARISON_ALWAYS };
    /* texel value 6; references 5, 6, 7 -> pass pattern of (ref OP 6) */
    float in[3][4] = { { .375f, .375f, 5.0f, 0 }, { .375f, .375f, 6.0f, 0 }, { .375f, .375f, 7.0f, 0 } }, out[3][4];
    for (int f = 0; f < 8; f++) {
        ID3D11SamplerState *s = hse_sampler(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, fn[f], 0);
        if (TQ(DECL_C, "T.SampleCmpLevelZero(S, p.xy, p.z)", t.srv, s, NULL, in, 3, out))
            for (int k = 0; k < 3; k++) {
                float r = in[k][2], v = 6.0f; int pass;
                switch (fn[f]) { case D3D11_COMPARISON_NEVER: pass = 0; break; case D3D11_COMPARISON_LESS: pass = r < v; break; case D3D11_COMPARISON_EQUAL: pass = r == v; break;
                    case D3D11_COMPARISON_LESS_EQUAL: pass = r <= v; break; case D3D11_COMPARISON_GREATER: pass = r > v; break; case D3D11_COMPARISON_NOT_EQUAL: pass = r != v; break;
                    case D3D11_COMPARISON_GREATER_EQUAL: pass = r >= v; break; default: pass = 1; }
                CHECKF(out[k][0] == (float)pass, "func %d ref %g vs texel 6: got %g want %d", fn[f], r, out[k][0], pass);
            }
        SAFE_RELEASE(s);
    }
    tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_samplecmp_linear_percentage_closer) {
    HSE_NEED();
    if (hse.dev && !cmp_supported()) { t_skip("R32_FLOAT comparison sampling unsupported"); return; }
    tx t = make_cmp_tex(); ID3D11SamplerState *s = hse_sampler(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_COMPARISON_LESS_EQUAL, 0);
    /* footprint (1,1)=6 (2,1)=7 (1,2)=10 (2,2)=11 around (.5,.5) */
    float in[5][4] = { { .5f, .5f, 8.5f, 0 }, { .5f, .5f, 5.0f, 0 }, { .5f, .5f, 12.0f, 0 }, { .5f, .5f, 6.5f, 0 }, { .5f, .5f, 10.5f, 0 } }, out[5][4];
    static const float want[5] = { 0.5f, 1.0f, 0.0f, 0.75f, 0.25f };
    if (TQ(DECL_C, "T.SampleCmpLevelZero(S, p.xy, p.z)", t.srv, s, NULL, in, 5, out))
        for (int i = 0; i < 5; i++) CHECKF(NEAR_(out[i][0], want[i], 0.01f), "ref %g got %g want %g", in[i][2], out[i][0], want[i]);
    SAFE_RELEASE(s); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_gathercmp_returns_four_results) {
    HSE_NEED();
    if (hse.dev && !cmp_supported()) { t_skip("R32_FLOAT comparison sampling unsupported"); return; }
    tx t = make_cmp_tex(); ID3D11SamplerState *s = hse_sampler(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_COMPARISON_LESS_EQUAL, 0);
    float in[1][4] = { { .5f, .5f, 8.5f, 0 } }, out[1][4];
    /* footprint order x=(1,2)=10 y=(2,2)=11 z=(2,1)=7 w=(1,1)=6; 8.5 <= texel passes for 10 and 11 only */
    if (TQ(DECL_C, "T.GatherCmp(S, p.xy, p.z)", t.srv, s, NULL, in, 1, out)) CHECK(out[0][0] == 1 && out[0][1] == 1 && out[0][2] == 0 && out[0][3] == 0);
    SAFE_RELEASE(s); tx_free(&t);
}

/* ---- GetDimensions -------------------------------------------------------------------------------------------------------------------- */
static int dims_run_at(const char *file, int line, const char *decl, const char *call_args, ID3D11ShaderResourceView *srv, uint32_t *out, int nout) {
    char src[1024];
    snprintf(src, sizeof src, "%s\nRWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { uint w = 0, h = 0, d = 0, e = 0, l = 0, m = %s; O.Store4(0, uint4(w, h, d, l)); O.Store(16, e); }", decl, call_args);
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = src; c.srv[0] = srv; c.outbytes = 32; c.gx = 1; c.out = out;
    (void)nout;
    return hse_dispatch_at(file, line, &c);
}
TEST(hlsl_semantics_exec_tex_getdimensions_2d_mips) {
    HSE_NEED();
    tx t = make_ramp_tex();
    uint32_t o[8];
    static const struct { const char *call; uint32_t w, h, l; } c[] = {
        { "0; T.GetDimensions(0, w, h, l)", 4, 4, 3 }, { "0; T.GetDimensions(1, w, h, l)", 2, 2, 3 }, { "0; T.GetDimensions(2, w, h, l)", 1, 1, 3 } };
    for (int i = 0; i < 3; i++)
        if (dims_run_at(__FILE__, __LINE__, "Texture2D<float4> T : register(t1);", c[i].call, t.srv, o, 8)) { EXPECT_EQ(o[0], c[i].w); EXPECT_EQ(o[1], c[i].h); EXPECT_EQ(o[3], c[i].l); }
    if (dims_run_at(__FILE__, __LINE__, "Texture2D<float4> T : register(t1);", "0; T.GetDimensions(w, h)", t.srv, o, 8)) { EXPECT_EQ(o[0], 4); EXPECT_EQ(o[1], 4); }
    tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_getdimensions_non_power_of_two_chain) {
    HSE_NEED();
    ID3D11Texture2D *tex = hse_tex2d(5, 3, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL);
    ID3D11ShaderResourceView *srv = NULL;
    if (tex) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)tex, NULL, &srv);
    uint32_t o[8];
    static const struct { const char *call; uint32_t w, h; } c[] = { { "0; T.GetDimensions(0, w, h, l)", 5, 3 }, { "0; T.GetDimensions(1, w, h, l)", 2, 1 }, { "0; T.GetDimensions(2, w, h, l)", 1, 1 } };
    for (int i = 0; i < 3; i++)
        if (dims_run_at(__FILE__, __LINE__, "Texture2D<float4> T : register(t1);", c[i].call, srv, o, 8)) { EXPECT_EQ(o[0], c[i].w); EXPECT_EQ(o[1], c[i].h); EXPECT_EQ(o[3], 3); }
    if (hse.dev && tex) { D3D11_TEXTURE2D_DESC d; ID3D11Texture2D_GetDesc(tex, &d); EXPECT_EQ(d.MipLevels, 3); }
    SAFE_RELEASE(srv); SAFE_RELEASE(tex);
}
TEST(hlsl_semantics_exec_tex_getdimensions_view_is_relative_to_most_detailed_mip) {
    HSE_NEED();
    tx t = make_ramp_tex();
    ID3D11ShaderResourceView *v = NULL;
    if (t.tex) {
        D3D11_SHADER_RESOURCE_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_FLOAT; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MostDetailedMip = 1; d.Texture2D.MipLevels = 2;
        ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)t.tex, &d, &v);
    }
    uint32_t o[8];
    if (dims_run_at(__FILE__, __LINE__, "Texture2D<float4> T : register(t1);", "0; T.GetDimensions(0, w, h, l)", v, o, 8)) { EXPECT_EQ(o[0], 2); EXPECT_EQ(o[1], 2); EXPECT_EQ(o[3], 2); }
    float in[3][4] = { { 0, 0, 0, 0 }, { 0, 0, 1, 0 }, { 1, 1, 1, 0 } }, out[3][4];
    if (TQ(DECL_F, "T.Load(int3(p.x, p.y, p.z))", v, NULL, NULL, in, 3, out)) { CHECK(out[0][0] == 101.0f); CHECK(out[1][0] == 200.0f); CHECK(out[2][0] == 0.0f); }   /* view mip 1 is the 1x1 level */
    SAFE_RELEASE(v); tx_free(&t);
}
TEST(hlsl_semantics_exec_tex_getdimensions_array_and_cube) {
    HSE_NEED();
    ID3D11Texture2D *arr = hse_tex2d(8, 4, 4, 3, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL);
    ID3D11ShaderResourceView *sa = NULL;
    if (arr) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)arr, NULL, &sa);
    uint32_t o[8];
    if (dims_run_at(__FILE__, __LINE__, "Texture2DArray<float4> T : register(t1);", "0; T.GetDimensions(0, w, h, e, l)", sa, o, 8)) { EXPECT_EQ(o[0], 8); EXPECT_EQ(o[1], 4); EXPECT_EQ(o[4], 3); EXPECT_EQ(o[3], 4); }
    if (dims_run_at(__FILE__, __LINE__, "Texture2DArray<float4> T : register(t1);", "0; T.GetDimensions(2, w, h, e, l)", sa, o, 8)) { EXPECT_EQ(o[0], 2); EXPECT_EQ(o[1], 1); EXPECT_EQ(o[4], 3); }
    SAFE_RELEASE(sa); SAFE_RELEASE(arr);
    D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
    ID3D11Texture2D *cube = NULL; ID3D11ShaderResourceView *sc = NULL;
    if (hse.dev) {
        d.Width = d.Height = 16; d.MipLevels = 5; d.ArraySize = 6; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE; d.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        if (SUCCEEDED(ID3D11Device_CreateTexture2D(hse.dev, &d, NULL, &cube))) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)cube, NULL, &sc);
    }
    if (dims_run_at(__FILE__, __LINE__, "TextureCube<float4> T : register(t1);", "0; T.GetDimensions(1, w, h, l)", sc, o, 8)) { EXPECT_EQ(o[0], 8); EXPECT_EQ(o[1], 8); EXPECT_EQ(o[3], 5); }
    SAFE_RELEASE(sc); SAFE_RELEASE(cube);
}
TEST(hlsl_semantics_exec_tex_getdimensions_3d) {
    HSE_NEED();
    ID3D11Texture3D *t3 = NULL; ID3D11ShaderResourceView *sv = NULL;
    if (hse.dev) {
        D3D11_TEXTURE3D_DESC d; memset(&d, 0, sizeof d);
        d.Width = 8; d.Height = 4; d.Depth = 2; d.MipLevels = 4; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (SUCCEEDED(ID3D11Device_CreateTexture3D(hse.dev, &d, NULL, &t3))) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)t3, NULL, &sv);
    }
    uint32_t o[8];
    if (dims_run_at(__FILE__, __LINE__, "Texture3D<float4> T : register(t1);", "0; T.GetDimensions(0, w, h, d, l)", sv, o, 8)) { EXPECT_EQ(o[0], 8); EXPECT_EQ(o[1], 4); EXPECT_EQ(o[2], 2); EXPECT_EQ(o[3], 4); }
    if (dims_run_at(__FILE__, __LINE__, "Texture3D<float4> T : register(t1);", "0; T.GetDimensions(1, w, h, d, l)", sv, o, 8)) { EXPECT_EQ(o[0], 4); EXPECT_EQ(o[1], 2); EXPECT_EQ(o[2], 1); }
    if (dims_run_at(__FILE__, __LINE__, "Texture3D<float4> T : register(t1);", "0; T.GetDimensions(3, w, h, d, l)", sv, o, 8)) { EXPECT_EQ(o[0], 1); EXPECT_EQ(o[1], 1); EXPECT_EQ(o[2], 1); }
    SAFE_RELEASE(sv); SAFE_RELEASE(t3);
}

/* ---- arrays and cube maps -------------------------------------------------------------------------------------------------------------------- */
TEST(hlsl_semantics_exec_tex_array_slice_selection_rounds_to_nearest) {
    HSE_NEED();
    float s0[4] = { 10, 10, 10, 10 }, s1[4] = { 20, 20, 20, 20 }, s2[4] = { 30, 30, 30, 30 };
    D3D11_SUBRESOURCE_DATA sd[3] = { { s0, 8, 0 }, { s1, 8, 0 }, { s2, 8, 0 } };
    ID3D11Texture2D *tex = hse_tex2d(2, 2, 1, 3, DXGI_FORMAT_R32_FLOAT, D3D11_BIND_SHADER_RESOURCE, sd);
    ID3D11ShaderResourceView *srv = NULL; ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    if (tex) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)tex, NULL, &srv);
    float in[9][4] = { { .25f, .25f, 0 }, { .25f, .25f, 1 }, { .25f, .25f, 2 }, { .25f, .25f, 1.4f }, { .25f, .25f, 1.6f }, { .25f, .25f, 10 }, { .25f, .25f, -3 }, { .25f, .25f, 0.4f }, { .25f, .25f, 2.4f } }, out[9][4];
    static const float want[9] = { 10, 20, 30, 20, 30, 30, 10, 10, 30 };
    if (TQ("Texture2DArray<float4> T : register(t1); SamplerState S : register(s0); SamplerState S1 : register(s1);", "T.SampleLevel(S, p.xyz, 0)", srv, s, NULL, in, 9, out))
        for (int i = 0; i < 9; i++) CHECKF(out[i][0] == want[i], "slice coordinate %g got %g want %g", in[i][2], out[i][0], want[i]);
    if (TQ("Texture2DArray<float4> T : register(t1); SamplerState S : register(s0); SamplerState S1 : register(s1);", "T.Load(int4(p.x, p.y, p.z, 0))", srv, s, NULL, in, 3, out))
        for (int i = 0; i < 3; i++) CHECKF(out[i][0] == want[i], "Load slice %g got %g", in[i][2], out[i][0]);
    SAFE_RELEASE(s); SAFE_RELEASE(srv); SAFE_RELEASE(tex);
}
TEST(hlsl_semantics_exec_tex_cube_face_selection) {
    HSE_NEED();
    float face[6][4]; D3D11_SUBRESOURCE_DATA sd[6];
    for (int f = 0; f < 6; f++) { for (int i = 0; i < 4; i++) face[f][i] = (float)(10 + f); sd[f].pSysMem = face[f]; sd[f].SysMemPitch = 8; sd[f].SysMemSlicePitch = 0; }
    ID3D11Texture2D *tex = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    if (hse.dev) {
        D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
        d.Width = d.Height = 2; d.MipLevels = 1; d.ArraySize = 6; d.Format = DXGI_FORMAT_R32_FLOAT; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE; d.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        if (SUCCEEDED(ID3D11Device_CreateTexture2D(hse.dev, &d, sd, &tex))) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)tex, NULL, &srv);
    }
    float in[9][4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 1, .5f, -.3f }, { -.2f, -3, .9f }, { .1f, .2f, -5 } }, out[9][4];
    static const float want[9] = { 10, 11, 12, 13, 14, 15, 10, 13, 15 };
    if (TQ("TextureCube<float4> T : register(t1); SamplerState S : register(s0); SamplerState S1 : register(s1);", "T.SampleLevel(S, p.xyz, 0)", srv, s, NULL, in, 9, out))
        for (int i = 0; i < 9; i++) CHECKF(out[i][0] == want[i], "direction (%g,%g,%g) got %g want %g", in[i][0], in[i][1], in[i][2], out[i][0], want[i]);
    SAFE_RELEASE(s); SAFE_RELEASE(srv); SAFE_RELEASE(tex);
}

/* ---- format decoding on load ------------------------------------------------------------------------------------------------------------------ */
static void fmt_load_at(const char *file, int line, DXGI_FORMAT fmt, const char *decl, const void *texels, UINT pitch, const char *expr, int nt, float out[][4]) {
    D3D11_SUBRESOURCE_DATA sd = { texels, pitch, 0 };
    ID3D11Texture2D *tex = hse_tex2d(nt, 1, 1, 1, fmt, D3D11_BIND_SHADER_RESOURCE, &sd);
    ID3D11ShaderResourceView *srv = NULL;
    if (tex) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)tex, NULL, &srv);
    float in[16][4]; for (int i = 0; i < nt; i++) { in[i][0] = (float)i; in[i][1] = in[i][2] = in[i][3] = 0; }
    tex_query_at(file, line, decl, expr, srv, NULL, NULL, in, nt, out);
    SAFE_RELEASE(srv); SAFE_RELEASE(tex);
}
#define FMT_LOAD(fmt, decl, texels, pitch, expr, nt, out) fmt_load_at(__FILE__, __LINE__, fmt, decl, texels, pitch, expr, nt, out)
TEST(hlsl_semantics_exec_tex_load_unorm8_decode) {
    HSE_NEED();
    uint8_t px[5][4] = { { 0, 1, 127, 128 }, { 255, 254, 64, 0 }, { 10, 20, 30, 40 }, { 255, 255, 255, 255 }, { 0, 0, 0, 0 } };
    float out[5][4] = { { 0 } };
    FMT_LOAD(DXGI_FORMAT_R8G8B8A8_UNORM, "Texture2D<float4> T : register(t1);", px, 20, "T.Load(int3(p.x, 0, 0))", 5, out);
    if (!hse.dev) return;
    for (int i = 0; i < 5; i++) for (int c = 0; c < 4; c++)
        CHECKF(NEAR_(out[i][c], px[i][c] / 255.0f, 1.2e-7f), "texel %d comp %d: %d/255 decoded as %.9g", i, c, px[i][c], out[i][c]);
}
TEST(hlsl_semantics_exec_tex_load_snorm8_decode) {
    HSE_NEED();
    int8_t px[4][4] = { { -128, -127, -1, 0 }, { 1, 63, 126, 127 }, { 64, -64, 100, -100 }, { 127, 127, -128, -128 } };
    float out[4][4] = { { 0 } };
    FMT_LOAD(DXGI_FORMAT_R8G8B8A8_SNORM, "Texture2D<float4> T : register(t1);", px, 16, "T.Load(int3(p.x, 0, 0))", 4, out);
    if (!hse.dev) return;
    for (int i = 0; i < 4; i++) for (int c = 0; c < 4; c++) {
        float want = px[i][c] / 127.0f; if (want < -1.0f) want = -1.0f;
        CHECKF(NEAR_(out[i][c], want, 1.2e-7f), "texel %d comp %d: %d decoded as %.9g want %.9g", i, c, px[i][c], out[i][c], want);
    }
    CHECK(out[0][0] == -1.0f && out[0][1] == -1.0f);   /* -128 and -127 both decode to -1 */
}
TEST(hlsl_semantics_exec_tex_load_integer_formats) {
    HSE_NEED();
    uint8_t pu[3][4] = { { 0, 1, 200, 255 }, { 7, 8, 9, 10 }, { 255, 0, 255, 0 } };
    int8_t ps[3][4] = { { -128, -1, 0, 127 }, { 5, -5, 6, -6 }, { 1, 2, 3, 4 } };
    float out[3][4] = { { 0 } };
    FMT_LOAD(DXGI_FORMAT_R8G8B8A8_UINT, "Texture2D<uint4> T : register(t1);", pu, 12, "asfloat(T.Load(int3(p.x, 0, 0)))", 3, out);
    if (!hse.dev) return;
    for (int i = 0; i < 3; i++) for (int c = 0; c < 4; c++) CHECKF(hse_fb(out[i][c]) == pu[i][c], "uint texel %d comp %d got %u", i, c, hse_fb(out[i][c]));
    FMT_LOAD(DXGI_FORMAT_R8G8B8A8_SINT, "Texture2D<int4> T : register(t1);", ps, 12, "asfloat(T.Load(int3(p.x, 0, 0)))", 3, out);
    for (int i = 0; i < 3; i++) for (int c = 0; c < 4; c++) CHECKF((int32_t)hse_fb(out[i][c]) == ps[i][c], "sint texel %d comp %d got %d", i, c, (int32_t)hse_fb(out[i][c]));
}
TEST(hlsl_semantics_exec_tex_load_bgra_swizzle_and_missing_channels) {
    HSE_NEED();
    uint8_t px[2][4] = { { 10, 20, 30, 40 }, { 255, 0, 128, 64 } };   /* memory order B G R A */
    float out[2][4] = { { 0 } };
    FMT_LOAD(DXGI_FORMAT_B8G8R8A8_UNORM, "Texture2D<float4> T : register(t1);", px, 8, "T.Load(int3(p.x, 0, 0))", 2, out);
    if (!hse.dev) return;
    for (int i = 0; i < 2; i++) { CHECK(NEAR_(out[i][0], px[i][2] / 255.0f, 1e-7f)); CHECK(NEAR_(out[i][1], px[i][1] / 255.0f, 1e-7f)); CHECK(NEAR_(out[i][2], px[i][0] / 255.0f, 1e-7f)); CHECK(NEAR_(out[i][3], px[i][3] / 255.0f, 1e-7f)); }
    uint8_t one[2] = { 51, 255 };
    FMT_LOAD(DXGI_FORMAT_R8_UNORM, "Texture2D<float4> T : register(t1);", one, 2, "T.Load(int3(p.x, 0, 0))", 2, out);
    CHECK(NEAR_(out[0][0], 0.2f, 1e-7f) && out[0][1] == 0 && out[0][2] == 0 && out[0][3] == 1.0f);
    uint8_t two[4] = { 255, 0, 0, 255 };
    FMT_LOAD(DXGI_FORMAT_R8G8_UNORM, "Texture2D<float4> T : register(t1);", two, 4, "T.Load(int3(p.x, 0, 0))", 2, out);
    CHECK(out[0][0] == 1.0f && out[0][1] == 0 && out[0][2] == 0 && out[0][3] == 1.0f);
    CHECK(out[1][0] == 0 && out[1][1] == 1.0f && out[1][3] == 1.0f);
}
TEST(hlsl_semantics_exec_tex_load_float16_and_unorm16_decode) {
    HSE_NEED();
    uint16_t h[2][4] = { { 0x3c00, 0xc000, 0x0000, 0x3800 }, { 0x7bff, 0x0001, 0x3555, 0x8000 } };
    float out[2][4] = { { 0 } };
    FMT_LOAD(DXGI_FORMAT_R16G16B16A16_FLOAT, "Texture2D<float4> T : register(t1);", h, 16, "T.Load(int3(p.x, 0, 0))", 2, out);
    if (!hse.dev) return;
    CHECK(out[0][0] == 1.0f && out[0][1] == -2.0f && out[0][2] == 0.0f && out[0][3] == 0.5f);
    CHECK(out[1][0] == 65504.0f && out[1][1] == 5.9604645e-08f && out[1][2] == 0.333251953125f && out[1][3] == 0.0f);
    uint16_t u[2][2] = { { 0, 65535 }, { 32768, 1 } };
    FMT_LOAD(DXGI_FORMAT_R16G16_UNORM, "Texture2D<float4> T : register(t1);", u, 8, "T.Load(int3(p.x, 0, 0))", 2, out);
    CHECK(out[0][0] == 0.0f && out[0][1] == 1.0f && NEAR_(out[1][0], 32768.0f / 65535.0f, 1e-7f) && NEAR_(out[1][1], 1.0f / 65535.0f, 1e-10f));
}
TEST(hlsl_semantics_exec_tex_load_rgb10a2_unorm_decode) {
    HSE_NEED();
    uint32_t px[3] = { 1023u | (512u << 10) | (0u << 20) | (3u << 30), 1u | (1022u << 10) | (341u << 20) | (1u << 30), 0 };
    float out[3][4] = { { 0 } };
    FMT_LOAD(DXGI_FORMAT_R10G10B10A2_UNORM, "Texture2D<float4> T : register(t1);", px, 12, "T.Load(int3(p.x, 0, 0))", 3, out);
    if (!hse.dev) return;
    CHECK(out[0][0] == 1.0f && NEAR_(out[0][1], 512.0f / 1023.0f, 1e-7f) && out[0][2] == 0.0f && out[0][3] == 1.0f);
    CHECK(NEAR_(out[1][0], 1.0f / 1023.0f, 1e-9f) && NEAR_(out[1][1], 1022.0f / 1023.0f, 1e-7f) && NEAR_(out[1][2], 341.0f / 1023.0f, 1e-7f) && NEAR_(out[1][3], 1.0f / 3.0f, 1e-7f));
}
TEST(hlsl_semantics_exec_tex_sample_srgb_decodes_to_linear) {
    HSE_NEED();
    uint8_t px[1][4] = { { 0, 128, 255, 128 } };
    D3D11_SUBRESOURCE_DATA sd = { px, 4, 0 };
    ID3D11Texture2D *tex = hse_tex2d(1, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, D3D11_BIND_SHADER_RESOURCE, &sd);
    ID3D11ShaderResourceView *srv = NULL; ID3D11SamplerState *s = smp_point(D3D11_TEXTURE_ADDRESS_CLAMP);
    if (tex) ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)tex, NULL, &srv);
    float in[1][4] = { { .5f, .5f, 0, 0 } }, out[1][4];
    if (TQ(DECL_F, "T.SampleLevel(S, p.xy, 0)", srv, s, NULL, in, 1, out)) {
        CHECK(out[0][0] == 0.0f);
        CHECKF(NEAR_(out[0][1], 0.21586050f, 1e-4f), "sRGB 128 decoded as %.7f (expect about 0.2158605)", out[0][1]);
        CHECK(out[0][2] == 1.0f);
        CHECKF(NEAR_(out[0][3], 128.0f / 255.0f, 1e-6f), "alpha stays linear: %.7f", out[0][3]);
    }
    SAFE_RELEASE(s); SAFE_RELEASE(srv); SAFE_RELEASE(tex);
}

/* ---- pixel shader: implicit LOD, bias, derivatives ----------------------------------------------------------------------------------------------- */
static float pxf(const d3dh_image *im, UINT x, UINT y, int ch) { return ((const float *)(im->data + y * im->pitch))[x * 4 + ch]; }
static int ps_with_ramp(const char *ps, ID3D11SamplerState *s, d3dh_image *im) {
    tx t = make_ramp_tex();
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = ps; p.srv[0] = t.srv; p.smp[0] = s;
    int ok = HSE_DRAW(&p, im);
    tx_free(&t);
    return ok;
}
#define PS_DECL "Texture2D<float4> T : register(t0); SamplerState S : register(s0);\n"
TEST(hlsl_semantics_exec_ps_sample_implicit_lod_selects_mip) {
    HSE_NEED();
    ID3D11SamplerState *s = hse_sampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP, 0, 0);
    d3dh_image im = { 0 };
    /* uv = pos/2: one pixel step is 0.5 uv = 2 texels -> lod 1 */
    if (ps_with_ramp(PS_DECL "float4 main(float4 p : SV_Position) : SV_Target { return T.Sample(S, p.xy / 2.0); }", s, &im)) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) CHECKF(pxf(&im, x, y, 0) == 101.0f + (y & 1) * 2 + (x & 1), "x=%u y=%u got %g", x, y, pxf(&im, x, y, 0));
        d3dh_image_free(&im);
    }
    /* uv = pos/4: lod 0 */
    if (ps_with_ramp(PS_DECL "float4 main(float4 p : SV_Position) : SV_Target { return T.Sample(S, p.xy / 4.0); }", s, &im)) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) CHECKF(pxf(&im, x, y, 0) == ramp0(x, y), "x=%u y=%u got %g", x, y, pxf(&im, x, y, 0));
        d3dh_image_free(&im);
    }
    /* uv = pos: 4 texels per pixel -> lod 2 */
    if (ps_with_ramp(PS_DECL "float4 main(float4 p : SV_Position) : SV_Target { return T.Sample(S, p.xy); }", s, &im)) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) CHECKF(pxf(&im, x, y, 0) == 200.0f, "x=%u y=%u got %g", x, y, pxf(&im, x, y, 0));
        d3dh_image_free(&im);
    }
    SAFE_RELEASE(s);
}
TEST(hlsl_semantics_exec_ps_samplebias_adds_to_lod) {
    HSE_NEED();
    ID3D11SamplerState *s = hse_sampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 0);
    d3dh_image im = { 0 };
    if (ps_with_ramp(PS_DECL "float4 main(float4 p : SV_Position) : SV_Target { return T.SampleBias(S, p.xy / 4.0, 1.0); }", s, &im)) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) CHECKF(pxf(&im, x, y, 0) == 101.0f + (y / 2) * 2 + (x / 2), "bias +1: x=%u y=%u got %g", x, y, pxf(&im, x, y, 0));
        d3dh_image_free(&im);
    }
    if (ps_with_ramp(PS_DECL "float4 main(float4 p : SV_Position) : SV_Target { return T.SampleBias(S, p.xy / 2.0, -1.0); }", s, &im)) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) {
            /* lod 1 - 1 = 0, uv = pos/2 clamps to the last texels */
            int tx_ = (int)floorf((x + 0.5f) / 2.0f * 4.0f), ty_ = (int)floorf((y + 0.5f) / 2.0f * 4.0f);
            if (tx_ > 3) { tx_ = 3; }
            if (ty_ > 3) { ty_ = 3; }
            CHECKF(pxf(&im, x, y, 0) == ramp0(tx_, ty_), "bias -1: x=%u y=%u got %g", x, y, pxf(&im, x, y, 0));
        }
        d3dh_image_free(&im);
    }
    SAFE_RELEASE(s);
}
TEST(hlsl_semantics_exec_ps_sampler_mip_lod_bias_adds_to_lod) {
    HSE_NEED();
    ID3D11SamplerState *s = hse_sampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1.0f);
    d3dh_image im = { 0 };
    if (ps_with_ramp(PS_DECL "float4 main(float4 p : SV_Position) : SV_Target { return T.Sample(S, p.xy / 4.0); }", s, &im)) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) CHECKF(pxf(&im, x, y, 0) == 101.0f + (y / 2) * 2 + (x / 2), "x=%u y=%u got %g", x, y, pxf(&im, x, y, 0));
        d3dh_image_free(&im);
    }
    SAFE_RELEASE(s);
}
TEST(hlsl_semantics_exec_ps_calculate_lod_values) {
    HSE_NEED();
    ID3D11SamplerState *s = smp_linear(D3D11_TEXTURE_ADDRESS_CLAMP);
    d3dh_image im = { 0 };
    static const struct { const char *scale; float clamped, unclamped; } c[] = { { "p.xy / 4.0", 0.0f, 0.0f }, { "p.xy / 2.0", 1.0f, 1.0f }, { "p.xy", 2.0f, 2.0f }, { "p.xy / 8.0", 0.0f, -1.0f }, { "p.xy * 4.0", 2.0f, 4.0f } };
    for (int k = 0; k < 5; k++) {
        char ps[512];
        snprintf(ps, sizeof ps, PS_DECL "float4 main(float4 p : SV_Position) : SV_Target { float2 uv = %s; return float4(T.CalculateLevelOfDetail(S, uv), T.CalculateLevelOfDetailUnclamped(S, uv), 0, 0); }", c[k].scale);
        if (ps_with_ramp(ps, s, &im)) {
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++)
                CHECKF(NEAR_(pxf(&im, x, y, 0), c[k].clamped, 1e-5f) && NEAR_(pxf(&im, x, y, 1), c[k].unclamped, 1e-5f), "uv=%s x=%u y=%u got %g %g want %g %g", c[k].scale, x, y, pxf(&im, x, y, 0), pxf(&im, x, y, 1), c[k].clamped, c[k].unclamped);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(s);
}
TEST(hlsl_semantics_exec_ps_ddx_ddy_of_position) {
    HSE_NEED();
    d3dh_image im = { 0 };
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 p : SV_Position) : SV_Target { return float4(ddx(p.x), ddy(p.y), ddx(p.y), ddy(p.x)); }";
    if (HSE_DRAW(&p, &im)) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++)
            CHECKF(pxf(&im, x, y, 0) == 1.0f && pxf(&im, x, y, 1) == 1.0f && pxf(&im, x, y, 2) == 0.0f && pxf(&im, x, y, 3) == 0.0f, "x=%u y=%u got %g %g %g %g", x, y, pxf(&im, x, y, 0), pxf(&im, x, y, 1), pxf(&im, x, y, 2), pxf(&im, x, y, 3));
        d3dh_image_free(&im);
    }
}
TEST(hlsl_semantics_exec_ps_ddx_ddy_fine_of_product) {
    HSE_NEED();
    d3dh_image im = { 0 };
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 p : SV_Position) : SV_Target { float f = p.x * p.y; return float4(ddx_fine(f), ddy_fine(f), 0, 1); }";
    if (HSE_DRAW(&p, &im)) {
        /* f = x*y: the fine x derivative is y of the pixel's own row, the fine y derivative is x of the pixel's own column */
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++)
            CHECKF(pxf(&im, x, y, 0) == y + 0.5f && pxf(&im, x, y, 1) == x + 0.5f, "x=%u y=%u got %g %g want %g %g", x, y, pxf(&im, x, y, 0), pxf(&im, x, y, 1), y + 0.5f, x + 0.5f);
        d3dh_image_free(&im);
    }
}
TEST(hlsl_semantics_exec_ps_ddx_ddy_coarse_one_value_per_quad) {
    HSE_NEED();
    d3dh_image im = { 0 };
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 p : SV_Position) : SV_Target { float f = p.x * p.y; return float4(ddx_coarse(f), ddy_coarse(f), 0, 1); }";
    if (HSE_DRAW(&p, &im)) {
        for (UINT qy = 0; qy < 4; qy += 2) for (UINT qx = 0; qx < 4; qx += 2) {
            float dx = pxf(&im, qx, qy, 0), dy = pxf(&im, qx, qy, 1);
            for (UINT j = 0; j < 2; j++) for (UINT i = 0; i < 2; i++)
                CHECKF(pxf(&im, qx + i, qy + j, 0) == dx && pxf(&im, qx + i, qy + j, 1) == dy, "quad (%u,%u) pixel (%u,%u): coarse derivatives differ inside a quad", qx, qy, i, j);
            /* the single value is the difference measured on one row / column of the quad */
            CHECKF(dx == qy + 0.5f || dx == qy + 1.5f, "quad (%u,%u) ddx_coarse %g", qx, qy, dx);
            CHECKF(dy == qx + 0.5f || dy == qx + 1.5f, "quad (%u,%u) ddy_coarse %g", qx, qy, dy);
            t_note("quad (%u,%u): ddx_coarse=%g ddy_coarse=%g (top row / left column would be %g %g)", qx, qy, dx, dy, qy + 0.5f, qx + 0.5f);
        }
        d3dh_image_free(&im);
    }
}
TEST(hlsl_semantics_exec_ps_ddx_plain_is_coarse_or_fine) {
    HSE_NEED();
    d3dh_image im = { 0 };
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 p : SV_Position) : SV_Target { float f = p.x * p.y; return float4(ddx(f), ddy(f), 0, 1); }";
    if (HSE_DRAW(&p, &im)) {
        int fine = 1, coarse_ok = 1;
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) {
            fine &= pxf(&im, x, y, 0) == y + 0.5f && pxf(&im, x, y, 1) == x + 0.5f;
            UINT qx = x & ~1u, qy = y & ~1u;
            coarse_ok &= pxf(&im, x, y, 0) == pxf(&im, qx, qy, 0) && pxf(&im, x, y, 1) == pxf(&im, qx, qy, 1);
        }
        CHECKF(fine || coarse_ok, "ddx/ddy are neither the fine nor the coarse derivative");
        t_note("ddx/ddy of x*y behave as %s", fine ? "fine" : "coarse");
        d3dh_image_free(&im);
    }
}
TEST(hlsl_semantics_exec_ps_derivative_of_linear_function_is_constant) {
    HSE_NEED();
    d3dh_image im = { 0 };
    hse_ps p; memset(&p, 0, sizeof p);
    p.w = 8; p.h = 8;
    p.ps = "float4 main(float4 p : SV_Position) : SV_Target { float f = p.x * 3.0 + p.y * 5.0; return float4(ddx(f), ddy(f), ddx_fine(f), ddy_coarse(f)); }";
    if (HSE_DRAW(&p, &im)) {
        for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++)
            CHECKF(pxf(&im, x, y, 0) == 3.0f && pxf(&im, x, y, 1) == 5.0f && pxf(&im, x, y, 2) == 3.0f && pxf(&im, x, y, 3) == 5.0f, "x=%u y=%u got %g %g %g %g", x, y, pxf(&im, x, y, 0), pxf(&im, x, y, 1), pxf(&im, x, y, 2), pxf(&im, x, y, 3));
        d3dh_image_free(&im);
    }
}
TEST(hlsl_semantics_exec_ps_ddx_fine_quadratic_on_second_quad_column) {
    HSE_NEED();
    d3dh_image im = { 0 };
    hse_ps p; memset(&p, 0, sizeof p);
    p.ps = "float4 main(float4 p : SV_Position) : SV_Target { float f = p.x * p.x; return float4(ddx_fine(f), ddy_fine(p.y * p.y), 0, 1); }";
    if (HSE_DRAW(&p, &im)) {
        /* pairs are (0.5,1.5) -> 2 and (2.5,3.5) -> 6 horizontally; vertically the same on y */
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++)
            CHECKF(pxf(&im, x, y, 0) == (x < 2 ? 2.0f : 6.0f) && pxf(&im, x, y, 1) == (y < 2 ? 2.0f : 6.0f), "x=%u y=%u got %g %g", x, y, pxf(&im, x, y, 0), pxf(&im, x, y, 1));
        d3dh_image_free(&im);
    }
}
