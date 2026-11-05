/* Depth-stencil and rasterizer state creation. */
#include "so_common.h"

#define SCAN_CHECK(hr, valid, v) t_check((hr) == ((valid) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, #hr, \
    "value %d: got %s want %s", (int)(v), d3dh_hr_name(hr), (valid) ? "S_FALSE" : "E_INVALIDARG")

static int ds_equal(const D3D11_DEPTH_STENCIL_DESC *a, const D3D11_DEPTH_STENCIL_DESC *b)
{
    return a->DepthEnable == b->DepthEnable && a->DepthWriteMask == b->DepthWriteMask && a->DepthFunc == b->DepthFunc &&
           a->StencilEnable == b->StencilEnable && a->StencilReadMask == b->StencilReadMask && a->StencilWriteMask == b->StencilWriteMask &&
           !memcmp(&a->FrontFace, &b->FrontFace, sizeof a->FrontFace) && !memcmp(&a->BackFace, &b->BackFace, sizeof a->BackFace);
}
static D3D11_DEPTH_STENCIL_DESC ds_stencil_on(void)
{
    D3D11_DEPTH_STENCIL_DESC d = so_ds_default();
    d.StencilEnable = TRUE;
    d.StencilReadMask = 0x3C;
    d.StencilWriteMask = 0xA5;
    d.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
    d.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR_SAT;
    d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
    d.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
    d.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
    d.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_INVERT;
    d.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
    d.BackFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
    return d;
}

/* ====================== depth stencil ===================================== */
ST(ds_default_accepted)
{
    D3D11_DEPTH_STENCIL_DESC d = so_ds_default();
    SO_NEED();
    SO_ACCEPT_DS(&d);
}
ST(ds_null_desc)
{
    ID3D11DepthStencilState *s = SO_SENTINEL(ID3D11DepthStencilState);
    SO_NEED();
    EXPECT_HR(SO_CDS(NULL, NULL), E_INVALIDARG);
    EXPECT_HR(SO_CDS(NULL, &s), E_INVALIDARG);
}
ST(ds_scan_depthfunc_enabled)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 11; v++) {
        D3D11_DEPTH_STENCIL_DESC d = so_ds_default();
        HRESULT hr;
        d.DepthFunc = (D3D11_COMPARISON_FUNC)v;
        hr = SO_CDS(&d, NULL);
        SCAN_CHECK(hr, v >= 1 && v <= 8, v);
    }
}
ST(ds_depthfunc_ignored_when_depth_disabled)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 11; v++) {
        D3D11_DEPTH_STENCIL_DESC d = so_ds_default();
        HRESULT hr;
        d.DepthEnable = FALSE;
        d.DepthFunc = (D3D11_COMPARISON_FUNC)v;
        hr = SO_CDS(&d, NULL);
        SCAN_CHECK(hr, 1, v);
    }
}
ST(ds_scan_depthwritemask)
{
    int v;
    SO_NEED();
    for (v = -3; v <= 4; v++) {
        D3D11_DEPTH_STENCIL_DESC d = so_ds_default();
        HRESULT hr;
        d.DepthWriteMask = (D3D11_DEPTH_WRITE_MASK)v;
        hr = SO_CDS(&d, NULL);
        SCAN_CHECK(hr, v == 0 || v == 1, v);
    }
}
ST(ds_writemask_validated_even_when_depth_disabled)
{
    D3D11_DEPTH_STENCIL_DESC d = so_ds_default();
    SO_NEED();
    d.DepthEnable = FALSE;
    d.DepthWriteMask = (D3D11_DEPTH_WRITE_MASK)2;
    SO_REJECT_DS(&d);
}
ST(ds_stencil_fields_validated_when_enabled)
{
    /* every stencil op / func field on both faces, one at a time */
    int field, face, v;
    SO_NEED();
    for (face = 0; face < 2; face++)
        for (field = 0; field < 4; field++)
            for (v = -1; v <= 10; v++) {
                D3D11_DEPTH_STENCIL_DESC d = ds_stencil_on();
                D3D11_DEPTH_STENCILOP_DESC *f = face ? &d.BackFace : &d.FrontFace;
                HRESULT hr;
                switch (field) {
                case 0: f->StencilFailOp = (D3D11_STENCIL_OP)v; break;
                case 1: f->StencilDepthFailOp = (D3D11_STENCIL_OP)v; break;
                case 2: f->StencilPassOp = (D3D11_STENCIL_OP)v; break;
                default: f->StencilFunc = (D3D11_COMPARISON_FUNC)v; break;
                }
                hr = SO_CDS(&d, NULL);
                t_check(hr == ((v >= 1 && v <= 8) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "stencil field", "face %d field %d value %d got %s", face, field, v, d3dh_hr_name(hr));
            }
}
ST(ds_stencil_fields_ignored_when_disabled)
{
    D3D11_DEPTH_STENCIL_DESC d = so_ds_default();
    SO_NEED();
    memset(&d.FrontFace, 0, sizeof d.FrontFace);
    memset(&d.BackFace, 0xFF, sizeof d.BackFace);
    d.StencilEnable = FALSE;
    SO_ACCEPT_DS(&d);
}
ST(ds_zeroed_desc_accepted)
{
    /* DepthEnable 0, DepthWriteMask 0 (ZERO) and StencilEnable 0 are all valid: nothing else is inspected */
    D3D11_DEPTH_STENCIL_DESC d;
    SO_NEED();
    memset(&d, 0, sizeof d);
    SO_ACCEPT_DS(&d);
}
ST(ds_dedup_same_pointer_and_refcount)
{
    D3D11_DEPTH_STENCIL_DESC d = ds_stencil_on();
    ID3D11DepthStencilState *a = NULL, *b = NULL;
    ULONG rc;
    SO_NEED();
    d.StencilReadMask = 0x11;
    EXPECT_HR(SO_CDS(&d, &a), S_OK);
    rc = d3dh_refcount(a);
    EXPECT_HR(SO_CDS(&d, &b), S_OK);
    EXPECT_EQ(a, b);
    EXPECT_EQ(d3dh_refcount(a), rc + 1);
    SAFE_RELEASE(b);
    EXPECT_EQ(d3dh_refcount(a), rc);
    SAFE_RELEASE(a);
}
ST(ds_dedup_distinguishes_every_field)
{
    D3D11_DEPTH_STENCIL_DESC base = ds_stencil_on(), v[12];
    ID3D11DepthStencilState *s0 = NULL, *si[12];
    int i, n = 0;
    SO_NEED();
    EXPECT_HR(SO_CDS(&base, &s0), S_OK);
    for (i = 0; i < 12; i++) v[i] = base;
    v[n++].DepthEnable = FALSE;
    v[n++].DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    v[n++].DepthFunc = D3D11_COMPARISON_GREATER;
    v[n++].StencilReadMask = 0x3D;
    v[n++].StencilWriteMask = 0xA4;
    v[n++].FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    v[n++].FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
    v[n++].BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    v[n++].BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    v[n++].BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    v[n++].FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
    v[n++].FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
    for (i = 0; i < n; i++) {
        si[i] = NULL;
        EXPECT_HR(SO_CDS(&v[i], &si[i]), S_OK);
        t_check(si[i] != s0, __FILE__, __LINE__, "distinct", "variant %d shares the base object", i);
    }
    for (i = 0; i < n; i++) SAFE_RELEASE(si[i]);
    SAFE_RELEASE(s0);
}
ST(ds_getdesc_default_roundtrip)
{
    D3D11_DEPTH_STENCIL_DESC d = so_ds_default(), g;
    ID3D11DepthStencilState *s = NULL;
    SO_NEED();
    EXPECT_HR(SO_CDS(&d, &s), S_OK);
    if (!s) return;
    memset(&g, 0xCD, sizeof g);
    ID3D11DepthStencilState_GetDesc(s, &g);
    CHECK(ds_equal(&g, &d));
    EXPECT_EQ(g.DepthEnable, 1);
    EXPECT_EQ(g.DepthWriteMask, D3D11_DEPTH_WRITE_MASK_ALL);
    EXPECT_EQ(g.DepthFunc, D3D11_COMPARISON_LESS);
    EXPECT_EQ(g.StencilReadMask, 0xFF);
    EXPECT_EQ(g.StencilWriteMask, 0xFF);
    SAFE_RELEASE(s);
}
ST(ds_getdesc_stencil_roundtrip)
{
    D3D11_DEPTH_STENCIL_DESC d = ds_stencil_on(), g;
    ID3D11DepthStencilState *s = NULL;
    SO_NEED();
    EXPECT_HR(SO_CDS(&d, &s), S_OK);
    if (!s) return;
    ID3D11DepthStencilState_GetDesc(s, &g);
    CHECK(ds_equal(&g, &d));
    SAFE_RELEASE(s);
}
ST(ds_getdesc_masks_every_byte_value)
{
    int v;
    SO_NEED();
    for (v = 0; v < 256; v += 17) {
        D3D11_DEPTH_STENCIL_DESC d = ds_stencil_on(), g;
        ID3D11DepthStencilState *s = NULL;
        d.StencilReadMask = (UINT8)v;
        d.StencilWriteMask = (UINT8)(255 - v);
        EXPECT_HR(SO_CDS(&d, &s), S_OK);
        if (!s) continue;
        ID3D11DepthStencilState_GetDesc(s, &g);
        t_check(g.StencilReadMask == v && g.StencilWriteMask == 255 - v, __FILE__, __LINE__, "mask", "v %d got %d/%d", v, g.StencilReadMask, g.StencilWriteMask);
        SAFE_RELEASE(s);
    }
}
ST(ds_getdesc_each_comparison_func_roundtrips)
{
    int v;
    SO_NEED();
    for (v = 1; v <= 8; v++) {
        D3D11_DEPTH_STENCIL_DESC d = ds_stencil_on(), g;
        ID3D11DepthStencilState *s = NULL;
        d.DepthFunc = (D3D11_COMPARISON_FUNC)v;
        d.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)v;
        d.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)(9 - v);
        EXPECT_HR(SO_CDS(&d, &s), S_OK);
        if (!s) continue;
        ID3D11DepthStencilState_GetDesc(s, &g);
        t_check(ds_equal(&g, &d), __FILE__, __LINE__, "func roundtrip", "func %d", v);
        SAFE_RELEASE(s);
    }
}
ST(ds_getdesc_each_stencil_op_roundtrips)
{
    int v;
    SO_NEED();
    for (v = 1; v <= 8; v++) {
        D3D11_DEPTH_STENCIL_DESC d = ds_stencil_on(), g;
        ID3D11DepthStencilState *s = NULL;
        d.FrontFace.StencilFailOp = d.BackFace.StencilPassOp = (D3D11_STENCIL_OP)v;
        d.FrontFace.StencilDepthFailOp = d.BackFace.StencilFailOp = (D3D11_STENCIL_OP)(9 - v);
        d.FrontFace.StencilPassOp = d.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)(1 + v % 8);
        EXPECT_HR(SO_CDS(&d, &s), S_OK);
        if (!s) continue;
        ID3D11DepthStencilState_GetDesc(s, &g);
        t_check(ds_equal(&g, &d), __FILE__, __LINE__, "op roundtrip", "op %d", v);
        SAFE_RELEASE(s);
    }
}
ST(ds_getdesc_nonzero_bool_reports_one)
{
    D3D11_DEPTH_STENCIL_DESC d = ds_stencil_on(), g;
    ID3D11DepthStencilState *s = NULL;
    SO_NEED();
    d.DepthEnable = 5;
    d.StencilEnable = 3;
    EXPECT_HR(SO_CDS(&d, &s), S_OK);
    if (!s) return;
    ID3D11DepthStencilState_GetDesc(s, &g);
    EXPECT_EQ(g.DepthEnable, 1);
    EXPECT_EQ(g.StencilEnable, 1);
    SAFE_RELEASE(s);
}
ST(ds_disabled_depth_dedup_observation)
{
    D3D11_DEPTH_STENCIL_DESC d = so_ds_default(), e;
    ID3D11DepthStencilState *a = NULL, *b = NULL;
    SO_NEED();
    d.DepthEnable = FALSE;
    e = d;
    e.DepthFunc = D3D11_COMPARISON_GREATER;
    EXPECT_HR(SO_CDS(&d, &a), S_OK);
    EXPECT_HR(SO_CDS(&e, &b), S_OK);
    t_note("depth disabled, different DepthFunc: %s object", a == b ? "same" : "different");
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

/* ====================== rasterizer ======================================== */
static int rast_equal(const D3D11_RASTERIZER_DESC *a, const D3D11_RASTERIZER_DESC *b)
{
    return a->FillMode == b->FillMode && a->CullMode == b->CullMode && a->FrontCounterClockwise == b->FrontCounterClockwise &&
           a->DepthBias == b->DepthBias && a->DepthBiasClamp == b->DepthBiasClamp && a->SlopeScaledDepthBias == b->SlopeScaledDepthBias &&
           a->DepthClipEnable == b->DepthClipEnable && a->ScissorEnable == b->ScissorEnable &&
           a->MultisampleEnable == b->MultisampleEnable && a->AntialiasedLineEnable == b->AntialiasedLineEnable;
}
ST(rast_default_accepted)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    SO_NEED();
    SO_ACCEPT_RAST(&d);
}
ST(rast_null_desc)
{
    ID3D11RasterizerState *s = NULL;
    SO_NEED();
    EXPECT_HR(SO_CRAST(NULL, NULL), E_INVALIDARG);
    EXPECT_HR(SO_CRAST(NULL, &s), E_INVALIDARG);
    CHECK(s == NULL);
}
ST(rast_zeroed_desc_rejected)
{
    /* FillMode 0 and CullMode 0 are not valid enumerators */
    D3D11_RASTERIZER_DESC d;
    SO_NEED();
    memset(&d, 0, sizeof d);
    SO_REJECT_RAST(&d);
}
ST(rast_scan_fillmode)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 6; v++) {
        D3D11_RASTERIZER_DESC d = so_rast_default();
        HRESULT hr;
        d.FillMode = (D3D11_FILL_MODE)v;
        hr = SO_CRAST(&d, NULL);
        SCAN_CHECK(hr, v == 2 || v == 3, v);
    }
}
ST(rast_scan_cullmode)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 6; v++) {
        D3D11_RASTERIZER_DESC d = so_rast_default();
        HRESULT hr;
        d.CullMode = (D3D11_CULL_MODE)v;
        hr = SO_CRAST(&d, NULL);
        SCAN_CHECK(hr, v >= 1 && v <= 3, v);
    }
}
ST(rast_all_fill_cull_combinations)
{
    int f, c;
    SO_NEED();
    for (f = 2; f <= 3; f++)
        for (c = 1; c <= 3; c++) {
            D3D11_RASTERIZER_DESC d = so_rast_default(), g;
            ID3D11RasterizerState *s = NULL;
            d.FillMode = (D3D11_FILL_MODE)f;
            d.CullMode = (D3D11_CULL_MODE)c;
            EXPECT_HR(SO_CRAST(&d, &s), S_OK);
            if (!s) continue;
            ID3D11RasterizerState_GetDesc(s, &g);
            t_check(g.FillMode == f && g.CullMode == c, __FILE__, __LINE__, "fill/cull", "%d/%d", f, c);
            SAFE_RELEASE(s);
        }
}
ST(rast_depth_bias_int_extremes_accepted)
{
    static const int vals[] = { 0, 1, -1, 100, -100, 1000000, -1000000, INT_MAX, INT_MIN };
    unsigned i;
    SO_NEED();
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        D3D11_RASTERIZER_DESC d = so_rast_default(), g;
        ID3D11RasterizerState *s = NULL;
        d.DepthBias = vals[i];
        EXPECT_HR(SO_CRAST(&d, &s), S_OK);
        if (!s) continue;
        ID3D11RasterizerState_GetDesc(s, &g);
        t_check(g.DepthBias == vals[i], __FILE__, __LINE__, "bias roundtrip", "%d got %d", vals[i], g.DepthBias);
        SAFE_RELEASE(s);
    }
}
ST(rast_depth_bias_clamp_and_slope_values)
{
    static const float vals[] = { 0.f, 1.f, -1.f, 0.5f, -0.5f, 1e-6f, 1e6f, -1e6f, 123.456f };
    unsigned i;
    SO_NEED();
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        D3D11_RASTERIZER_DESC d = so_rast_default(), g;
        ID3D11RasterizerState *s = NULL;
        d.DepthBiasClamp = vals[i];
        d.SlopeScaledDepthBias = vals[(i + 3) % 9];
        EXPECT_HR(SO_CRAST(&d, &s), S_OK);
        if (!s) continue;
        ID3D11RasterizerState_GetDesc(s, &g);
        t_check(g.DepthBiasClamp == d.DepthBiasClamp && g.SlopeScaledDepthBias == d.SlopeScaledDepthBias, __FILE__, __LINE__, "float roundtrip",
                "clamp %g slope %g got %g/%g", d.DepthBiasClamp, d.SlopeScaledDepthBias, g.DepthBiasClamp, g.SlopeScaledDepthBias);
        SAFE_RELEASE(s);
    }
}
ST(rast_nan_depth_bias_clamp_rejected)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    SO_NEED();
    d.DepthBiasClamp = NAN;
    SO_REJECT_RAST(&d);
}
ST(rast_nan_slope_scaled_bias_rejected)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    SO_NEED();
    d.SlopeScaledDepthBias = NAN;
    SO_REJECT_RAST(&d);
}
ST(rast_boolean_flags_all_combinations)
{
    int m;
    SO_NEED();
    for (m = 0; m < 16; m++) {
        D3D11_RASTERIZER_DESC d = so_rast_default(), g;
        ID3D11RasterizerState *s = NULL;
        d.FrontCounterClockwise = m & 1;
        d.DepthClipEnable = (m >> 1) & 1;
        d.ScissorEnable = (m >> 2) & 1;
        d.MultisampleEnable = (m >> 3) & 1;
        d.AntialiasedLineEnable = (m & 3) == 3;
        EXPECT_HR(SO_CRAST(&d, &s), S_OK);
        if (!s) continue;
        ID3D11RasterizerState_GetDesc(s, &g);
        t_check(rast_equal(&g, &d), __FILE__, __LINE__, "flags", "mask %d", m);
        SAFE_RELEASE(s);
    }
}
ST(rast_dedup_same_pointer_and_refcount)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    ID3D11RasterizerState *a = NULL, *b = NULL;
    ULONG rc;
    SO_NEED();
    d.DepthBias = 4321;
    EXPECT_HR(SO_CRAST(&d, &a), S_OK);
    rc = d3dh_refcount(a);
    EXPECT_HR(SO_CRAST(&d, &b), S_OK);
    EXPECT_EQ(a, b);
    EXPECT_EQ(d3dh_refcount(a), rc + 1);
    SAFE_RELEASE(b);
    EXPECT_EQ(d3dh_refcount(a), rc);
    SAFE_RELEASE(a);
}
ST(rast_dedup_distinguishes_every_field)
{
    D3D11_RASTERIZER_DESC base = so_rast_default(), v[10];
    ID3D11RasterizerState *s0 = NULL, *si[10];
    int i, n = 0;
    SO_NEED();
    EXPECT_HR(SO_CRAST(&base, &s0), S_OK);
    for (i = 0; i < 10; i++) v[i] = base;
    v[n++].FillMode = D3D11_FILL_WIREFRAME;
    v[n++].CullMode = D3D11_CULL_NONE;
    v[n++].FrontCounterClockwise = TRUE;
    v[n++].DepthBias = 1;
    v[n++].DepthBiasClamp = 0.25f;
    v[n++].SlopeScaledDepthBias = 2.f;
    v[n++].DepthClipEnable = FALSE;
    v[n++].ScissorEnable = TRUE;
    v[n++].MultisampleEnable = TRUE;
    v[n++].AntialiasedLineEnable = TRUE;
    for (i = 0; i < n; i++) {
        si[i] = NULL;
        EXPECT_HR(SO_CRAST(&v[i], &si[i]), S_OK);
        t_check(si[i] != s0, __FILE__, __LINE__, "distinct", "variant %d shares the base object", i);
    }
    for (i = 0; i < n; i++) SAFE_RELEASE(si[i]);
    SAFE_RELEASE(s0);
}
ST(rast_getdesc_default_values)
{
    D3D11_RASTERIZER_DESC d = so_rast_default(), g;
    ID3D11RasterizerState *s = NULL;
    SO_NEED();
    EXPECT_HR(SO_CRAST(&d, &s), S_OK);
    if (!s) return;
    memset(&g, 0x77, sizeof g);
    ID3D11RasterizerState_GetDesc(s, &g);
    CHECK(rast_equal(&g, &d));
    EXPECT_EQ(g.FillMode, D3D11_FILL_SOLID);
    EXPECT_EQ(g.CullMode, D3D11_CULL_BACK);
    EXPECT_EQ(g.DepthClipEnable, 1);
    EXPECT_EQ(g.ScissorEnable, 0);
    SAFE_RELEASE(s);
}
ST(rast_getdesc_nonzero_bool_reports_one)
{
    D3D11_RASTERIZER_DESC d = so_rast_default(), g;
    ID3D11RasterizerState *s = NULL;
    SO_NEED();
    d.ScissorEnable = 9;
    d.FrontCounterClockwise = 2;
    EXPECT_HR(SO_CRAST(&d, &s), S_OK);
    if (!s) return;
    ID3D11RasterizerState_GetDesc(s, &g);
    EXPECT_EQ(g.ScissorEnable, 1);
    EXPECT_EQ(g.FrontCounterClockwise, 1);
    SAFE_RELEASE(s);
}
ST(rast_qi_rasterizerstate1_and_getdesc1)
{
    D3D11_RASTERIZER_DESC d = so_rast_default(), g0;
    D3D11_RASTERIZER_DESC1 g1;
    ID3D11RasterizerState *s = NULL;
    ID3D11RasterizerState1 *s1 = NULL;
    HRESULT hr;
    SO_NEED1();
    d.DepthBias = -77;
    d.CullMode = D3D11_CULL_FRONT;
    EXPECT_HR(SO_CRAST(&d, &s), S_OK);
    if (!s) return;
    hr = ID3D11RasterizerState_QueryInterface(s, &IID_ID3D11RasterizerState1, (void **)&s1);
    EXPECT_HR(hr, S_OK);
    if (s1) {
        ID3D11RasterizerState_GetDesc(s, &g0);
        memset(&g1, 0x55, sizeof g1);
        ID3D11RasterizerState1_GetDesc1(s1, &g1);
        CHECK(rast_equal(&g0, (D3D11_RASTERIZER_DESC *)&g1));
        EXPECT_EQ(g1.ForcedSampleCount, 0);
        SAFE_RELEASE(s1);
    }
    SAFE_RELEASE(s);
}
ST(rast_qi_rasterizerstate2_and_getdesc2)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    D3D11_RASTERIZER_DESC2 g2;
    ID3D11RasterizerState *s = NULL;
    ID3D11RasterizerState2 *s2 = NULL;
    SO_NEED3();
    d.FillMode = D3D11_FILL_WIREFRAME;
    EXPECT_HR(SO_CRAST(&d, &s), S_OK);
    if (!s) return;
    EXPECT_HR(ID3D11RasterizerState_QueryInterface(s, &IID_ID3D11RasterizerState2, (void **)&s2), S_OK);
    if (s2) {
        memset(&g2, 0x55, sizeof g2);
        ID3D11RasterizerState2_GetDesc2(s2, &g2);
        EXPECT_EQ(g2.FillMode, D3D11_FILL_WIREFRAME);
        EXPECT_EQ(g2.ForcedSampleCount, 0);
        EXPECT_EQ(g2.ConservativeRaster, D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF);
        SAFE_RELEASE(s2);
    }
    SAFE_RELEASE(s);
}

/* ---- RasterizerState1 / 2 ------------------------------------------------------ */
static D3D11_RASTERIZER_DESC1 r1_default(void)
{
    D3D11_RASTERIZER_DESC1 d;
    D3D11_RASTERIZER_DESC b = so_rast_default();
    memcpy(&d, &b, sizeof b);
    d.ForcedSampleCount = 0;
    return d;
}
ST(rast1_null_desc)
{
    ID3D11RasterizerState1 *s = NULL;
    SO_NEED1();
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(so_dev1(), NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(so_dev1(), NULL, &s), E_INVALIDARG);
}
ST(rast1_default_accepted_and_s_false)
{
    D3D11_RASTERIZER_DESC1 d = r1_default();
    ID3D11RasterizerState1 *s = NULL;
    SO_NEED1();
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(so_dev1(), &d, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(so_dev1(), &d, &s), S_OK);
    SAFE_RELEASE(s);
}
ST(rast1_forced_sample_count_invalid_values)
{
    static const UINT bad[] = { 2, 3, 5, 6, 7, 9, 12, 15, 17, 32, 64, 0xFFFFFFFF };
    unsigned i;
    SO_NEED1();
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        D3D11_RASTERIZER_DESC1 d = r1_default();
        HRESULT hr;
        d.ForcedSampleCount = bad[i];
        hr = ID3D11Device1_CreateRasterizerState1(so_dev1(), &d, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "forced sample count", "count %u got %s", bad[i], d3dh_hr_name(hr));
    }
}
ST(rast1_forced_sample_count_supported_values_observation)
{
    static const UINT ok[] = { 1, 4, 8, 16 };
    unsigned i;
    SO_NEED1();
    for (i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        D3D11_RASTERIZER_DESC1 d = r1_default();
        HRESULT hr;
        d.ForcedSampleCount = ok[i];
        hr = ID3D11Device1_CreateRasterizerState1(so_dev1(), &d, NULL);
        t_note("ForcedSampleCount %u (no multisample) -> %s", ok[i], d3dh_hr_name(hr));
        CHECK(hr == S_FALSE || hr == E_INVALIDARG);
    }
}
ST(rast1_forced_sample_count_zero_roundtrip)
{
    D3D11_RASTERIZER_DESC1 d = r1_default(), g;
    ID3D11RasterizerState1 *s = NULL;
    SO_NEED1();
    d.DepthBias = 12;
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(so_dev1(), &d, &s), S_OK);
    if (!s) return;
    memset(&g, 0x66, sizeof g);
    ID3D11RasterizerState1_GetDesc1(s, &g);
    CHECK(rast_equal((D3D11_RASTERIZER_DESC *)&g, (D3D11_RASTERIZER_DESC *)&d));
    EXPECT_EQ(g.ForcedSampleCount, 0);
    SAFE_RELEASE(s);
}
ST(rast1_dedup_with_rast0)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    D3D11_RASTERIZER_DESC1 d1 = r1_default();
    ID3D11RasterizerState *a = NULL;
    ID3D11RasterizerState1 *b = NULL;
    SO_NEED1();
    d.DepthBias = d1.DepthBias = 9191;
    EXPECT_HR(SO_CRAST(&d, &a), S_OK);
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(so_dev1(), &d1, &b), S_OK);
    EXPECT_EQ(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(rast1_invalid_fill_cull_same_as_base)
{
    D3D11_RASTERIZER_DESC1 d = r1_default();
    SO_NEED1();
    d.FillMode = (D3D11_FILL_MODE)1;
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(so_dev1(), &d, NULL), E_INVALIDARG);
    d = r1_default();
    d.CullMode = (D3D11_CULL_MODE)4;
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(so_dev1(), &d, NULL), E_INVALIDARG);
}
static D3D11_RASTERIZER_DESC2 r2_default(void)
{
    D3D11_RASTERIZER_DESC2 d;
    memset(&d, 0, sizeof d);
    d.FillMode = D3D11_FILL_SOLID;
    d.CullMode = D3D11_CULL_BACK;
    d.DepthClipEnable = TRUE;
    d.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    return d;
}
ST(rast2_null_desc)
{
    SO_NEED3();
    EXPECT_HR(ID3D11Device3_CreateRasterizerState2(so_dev3(), NULL, NULL), E_INVALIDARG);
}
ST(rast2_default_accepted)
{
    D3D11_RASTERIZER_DESC2 d = r2_default();
    ID3D11RasterizerState2 *s = NULL;
    SO_NEED3();
    EXPECT_HR(ID3D11Device3_CreateRasterizerState2(so_dev3(), &d, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device3_CreateRasterizerState2(so_dev3(), &d, &s), S_OK);
    SAFE_RELEASE(s);
}
ST(rast2_conservative_mode_out_of_range)
{
    static const int bad[] = { 2, 3, -1, 100 };
    unsigned i;
    SO_NEED3();
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        D3D11_RASTERIZER_DESC2 d = r2_default();
        HRESULT hr;
        d.ConservativeRaster = (D3D11_CONSERVATIVE_RASTERIZATION_MODE)bad[i];
        hr = ID3D11Device3_CreateRasterizerState2(so_dev3(), &d, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "conservative", "mode %d got %s", bad[i], d3dh_hr_name(hr));
    }
}
ST(rast2_conservative_on_requires_solid_fill)
{
    D3D11_RASTERIZER_DESC2 d = r2_default();
    SO_NEED3();
    d.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON;
    d.FillMode = D3D11_FILL_WIREFRAME;
    EXPECT_HR(ID3D11Device3_CreateRasterizerState2(so_dev3(), &d, NULL), E_INVALIDARG);
}
ST(rast2_conservative_on_solid_depends_on_tier)
{
    D3D11_RASTERIZER_DESC2 d = r2_default();
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2;
    HRESULT hr;
    SO_NEED3();
    memset(&o2, 0, sizeof o2);
    ID3D11Device_CheckFeatureSupport(so_dev, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof o2);
    d.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON;
    hr = ID3D11Device3_CreateRasterizerState2(so_dev3(), &d, NULL);
    t_note("conservative raster tier %d: ON solid -> %s", (int)o2.ConservativeRasterizationTier, d3dh_hr_name(hr));
    if (o2.ConservativeRasterizationTier == D3D11_CONSERVATIVE_RASTERIZATION_NOT_SUPPORTED) EXPECT_HR(hr, E_INVALIDARG);
    else EXPECT_HR(hr, S_FALSE);
}
ST(rast2_dedup_with_rast0_object)
{
    D3D11_RASTERIZER_DESC d = so_rast_default();
    D3D11_RASTERIZER_DESC2 d2 = r2_default();
    ID3D11RasterizerState *a = NULL;
    ID3D11RasterizerState2 *b = NULL;
    SO_NEED3();
    d.SlopeScaledDepthBias = d2.SlopeScaledDepthBias = 3.5f;
    EXPECT_HR(SO_CRAST(&d, &a), S_OK);
    EXPECT_HR(ID3D11Device3_CreateRasterizerState2(so_dev3(), &d2, &b), S_OK);
    EXPECT_EQ(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(rast2_getdesc2_roundtrip)
{
    D3D11_RASTERIZER_DESC2 d = r2_default(), g;
    ID3D11RasterizerState2 *s = NULL;
    SO_NEED3();
    d.CullMode = D3D11_CULL_NONE;
    d.FrontCounterClockwise = TRUE;
    d.DepthBias = 55;
    d.DepthBiasClamp = 2.f;
    d.SlopeScaledDepthBias = -1.5f;
    d.ScissorEnable = TRUE;
    EXPECT_HR(ID3D11Device3_CreateRasterizerState2(so_dev3(), &d, &s), S_OK);
    if (!s) return;
    ID3D11RasterizerState2_GetDesc2(s, &g);
    CHECK(rast_equal((D3D11_RASTERIZER_DESC *)&g, (D3D11_RASTERIZER_DESC *)&d));
    EXPECT_EQ(g.ConservativeRaster, 0);
    EXPECT_EQ(g.ForcedSampleCount, 0);
    SAFE_RELEASE(s);
}
