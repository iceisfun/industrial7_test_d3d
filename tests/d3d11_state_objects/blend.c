/* Blend state creation: validation, dedup, GetDesc normalisation, logic ops. */
#include "so_common.h"

#define SCAN_CHECK(hr, valid, v) t_check((hr) == ((valid) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, #hr, \
    "value %d: got %s want %s", (int)(v), d3dh_hr_name(hr), (valid) ? "S_FALSE" : "E_INVALIDARG")

static int color_factor_ok(int v, int dual)
{
    return (v >= 1 && v <= 11) || v == 14 || v == 15 || (dual && v >= 16 && v <= 19);
}
static int alpha_factor_ok(int v, int dual)
{
    return v == 1 || v == 2 || (v >= 5 && v <= 8) || v == 11 || v == 14 || v == 15 || (dual && (v == 18 || v == 19));
}
static int blend_eq(const D3D11_RENDER_TARGET_BLEND_DESC *a, const D3D11_RENDER_TARGET_BLEND_DESC *b)
{
    return a->BlendEnable == b->BlendEnable && a->SrcBlend == b->SrcBlend && a->DestBlend == b->DestBlend &&
           a->BlendOp == b->BlendOp && a->SrcBlendAlpha == b->SrcBlendAlpha && a->DestBlendAlpha == b->DestBlendAlpha &&
           a->BlendOpAlpha == b->BlendOpAlpha && a->RenderTargetWriteMask == b->RenderTargetWriteMask;
}
static void enabled_rt0(D3D11_BLEND_DESC *d)
{
    d->RenderTarget[0].BlendEnable = TRUE;
    d->RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    d->RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    d->RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    d->RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    d->RenderTarget[0].RenderTargetWriteMask = 0x0B;
}

/* ---- basics ---------------------------------------------------------------- */
ST(blend_default_accepted)
{
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    SO_ACCEPT_BLEND(&d);
}
ST(blend_null_desc_with_out)
{
    ID3D11BlendState *s = SO_SENTINEL(ID3D11BlendState);
    SO_NEED();
    EXPECT_HR(SO_CBLEND(NULL, &s), E_INVALIDARG);
    CHECK(s == NULL);
}
ST(blend_null_desc_null_out)
{
    SO_NEED();
    EXPECT_HR(SO_CBLEND(NULL, NULL), E_INVALIDARG);
}
ST(blend_zeroed_desc_is_valid_semantically)
{
    /* an all-zero desc has Src/Dest = 0 but BlendEnable is FALSE so factors are not validated; write mask 0 is valid */
    D3D11_BLEND_DESC d;
    ID3D11BlendState *s = NULL;
    SO_NEED();
    memset(&d, 0, sizeof d);
    EXPECT_HR(SO_CBLEND(&d, NULL), S_FALSE);
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (s) {
        D3D11_BLEND_DESC g;
        memset(&g, 0xCC, sizeof g);
        ID3D11BlendState_GetDesc(s, &g);
        EXPECT_EQ(g.RenderTarget[0].RenderTargetWriteMask, 0);
        EXPECT_EQ(g.RenderTarget[0].BlendEnable, 0);
        SAFE_RELEASE(s);
    }
}
ST(blend_validate_only_returns_s_false)
{
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    EXPECT_HR(SO_CBLEND(&d, NULL), S_FALSE);
    enabled_rt0(&d);
    EXPECT_HR(SO_CBLEND(&d, NULL), S_FALSE);
    d.AlphaToCoverageEnable = TRUE;
    EXPECT_HR(SO_CBLEND(&d, NULL), S_FALSE);
    d.AlphaToCoverageEnable = FALSE;
    d.RenderTarget[0].SrcBlend = 0;
    EXPECT_HR(SO_CBLEND(&d, NULL), E_INVALIDARG);
}
ST(blend_create_returns_s_ok_and_object)
{
    D3D11_BLEND_DESC d = so_blend_default();
    ID3D11BlendState *s = NULL;
    SO_NEED();
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    CHECK(s != NULL);
    SAFE_RELEASE(s);
}

/* ---- scans over single fields ----------------------------------------------- */
ST(blend_scan_srcblend_rt0)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 24; v++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].SrcBlend = (D3D11_BLEND)v;
        hr = SO_CBLEND(&d, NULL);
        SCAN_CHECK(hr, color_factor_ok(v, 1), v);
    }
}
ST(blend_scan_destblend_rt0)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 24; v++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].DestBlend = (D3D11_BLEND)v;
        hr = SO_CBLEND(&d, NULL);
        SCAN_CHECK(hr, color_factor_ok(v, 1), v);
    }
}
ST(blend_scan_srcblendalpha_rt0)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 24; v++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].SrcBlendAlpha = (D3D11_BLEND)v;
        hr = SO_CBLEND(&d, NULL);
        SCAN_CHECK(hr, alpha_factor_ok(v, 1), v);
    }
}
ST(blend_scan_destblendalpha_rt0)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 24; v++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].DestBlendAlpha = (D3D11_BLEND)v;
        hr = SO_CBLEND(&d, NULL);
        SCAN_CHECK(hr, alpha_factor_ok(v, 1), v);
    }
}
ST(blend_scan_blendop_rt0)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 9; v++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)v;
        hr = SO_CBLEND(&d, NULL);
        SCAN_CHECK(hr, v >= 1 && v <= 5, v);
    }
}
ST(blend_scan_blendopalpha_rt0)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 9; v++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].BlendOpAlpha = (D3D11_BLEND_OP)v;
        hr = SO_CBLEND(&d, NULL);
        SCAN_CHECK(hr, v >= 1 && v <= 5, v);
    }
}
ST(blend_scan_writemask_rt0)
{
    int v;
    SO_NEED();
    for (v = 0; v <= 255; v++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.RenderTarget[0].RenderTargetWriteMask = (UINT8)v;
        hr = SO_CBLEND(&d, NULL);
        SCAN_CHECK(hr, v <= 0x0F, v);
    }
}
ST(blend_scan_writemask_rt0_blend_enabled)
{
    int v;
    SO_NEED();
    for (v = 0; v <= 255; v += 5) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        enabled_rt0(&d);
        d.RenderTarget[0].RenderTargetWriteMask = (UINT8)v;
        hr = SO_CBLEND(&d, NULL);
        SCAN_CHECK(hr, v <= 0x0F, v);
    }
}
ST(blend_disabled_rt_factors_not_validated)
{
    /* BlendEnable == FALSE: the blend factors/ops are never inspected */
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    d.RenderTarget[0].SrcBlend = 0;
    d.RenderTarget[0].DestBlend = 99;
    d.RenderTarget[0].BlendOp = 0;
    d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_COLOR;
    d.RenderTarget[0].DestBlendAlpha = -5;
    d.RenderTarget[0].BlendOpAlpha = 77;
    SO_ACCEPT_BLEND(&d);
}
ST(blend_disabled_rt_mask_still_validated)
{
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    d.RenderTarget[0].RenderTargetWriteMask = 0x10;
    SO_REJECT_BLEND(&d);
    d.RenderTarget[0].RenderTargetWriteMask = 0xFF;
    SO_REJECT_BLEND(&d);
}
ST(blend_color_factors_rejected_as_alpha_factors)
{
    static const int bad[] = { D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_DEST_COLOR,
                               D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_SRC1_COLOR, D3D11_BLEND_INV_SRC1_COLOR };
    unsigned i;
    SO_NEED();
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        D3D11_BLEND_DESC d = so_blend_default();
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].SrcBlendAlpha = bad[i];
        SO_REJECT_BLEND(&d);
        d = so_blend_default();
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].DestBlendAlpha = bad[i];
        SO_REJECT_BLEND(&d);
    }
}
ST(blend_reserved_enum_values_12_13)
{
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].SrcBlend = (D3D11_BLEND)12;
    SO_REJECT_BLEND(&d);
    d.RenderTarget[0].SrcBlend = (D3D11_BLEND)13;
    SO_REJECT_BLEND(&d);
    d = so_blend_default();
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].DestBlend = (D3D11_BLEND)12;
    SO_REJECT_BLEND(&d);
}
ST(blend_src_alpha_sat_as_dest_color_factor)
{
    /* SRC_ALPHA_SAT is a legal enum for any of the four factors */
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA_SAT;
    d.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_ALPHA_SAT;
    d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA_SAT;
    d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA_SAT;
    SO_ACCEPT_BLEND(&d);
}
ST(blend_blend_factor_enums_accepted)
{
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
    d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_BLEND_FACTOR;
    d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_BLEND_FACTOR;
    d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_BLEND_FACTOR;
    SO_ACCEPT_BLEND(&d);
}
ST(blend_all_valid_color_pairs)
{
    int s, dd;
    SO_NEED();
    for (s = 1; s <= 15; s++) {
        for (dd = 1; dd <= 15; dd++) {
            D3D11_BLEND_DESC d = so_blend_default();
            HRESULT hr;
            d.RenderTarget[0].BlendEnable = TRUE;
            d.RenderTarget[0].SrcBlend = (D3D11_BLEND)s;
            d.RenderTarget[0].DestBlend = (D3D11_BLEND)dd;
            hr = SO_CBLEND(&d, NULL);
            t_check(hr == ((color_factor_ok(s, 0) && color_factor_ok(dd, 0)) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__,
                    "pair", "src %d dest %d got %s", s, dd, d3dh_hr_name(hr));
        }
    }
}
ST(blend_all_valid_ops_pairs)
{
    int a, b;
    SO_NEED();
    for (a = 1; a <= 5; a++)
        for (b = 1; b <= 5; b++) {
            D3D11_BLEND_DESC d = so_blend_default();
            d.RenderTarget[0].BlendEnable = TRUE;
            d.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)a;
            d.RenderTarget[0].BlendOpAlpha = (D3D11_BLEND_OP)b;
            SO_ACCEPT_BLEND(&d);
        }
}

/* ---- independent blend ------------------------------------------------------ */
ST(blend_nonindependent_ignores_rt1_to_rt7)
{
    D3D11_BLEND_DESC d = so_blend_default();
    int i;
    SO_NEED();
    for (i = 1; i < 8; i++) {
        d.RenderTarget[i].BlendEnable = TRUE;
        d.RenderTarget[i].SrcBlend = 0;
        d.RenderTarget[i].DestBlend = 200;
        d.RenderTarget[i].BlendOp = 0;
        d.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_SRC_COLOR;
        d.RenderTarget[i].RenderTargetWriteMask = 0xFF;
    }
    SO_ACCEPT_BLEND(&d);
}
ST(blend_independent_validates_every_rt_mask)
{
    int k;
    SO_NEED();
    for (k = 0; k < 8; k++) {
        D3D11_BLEND_DESC d = so_blend_default();
        d.IndependentBlendEnable = TRUE;
        SO_ACCEPT_BLEND(&d);
        d.RenderTarget[k].RenderTargetWriteMask = 0x10;
        SO_REJECT_BLEND(&d);
    }
}
ST(blend_independent_validates_every_enabled_rt_factors)
{
    int k;
    SO_NEED();
    for (k = 0; k < 8; k++) {
        D3D11_BLEND_DESC d = so_blend_default();
        d.IndependentBlendEnable = TRUE;
        d.RenderTarget[k].BlendEnable = TRUE;
        d.RenderTarget[k].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        d.RenderTarget[k].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        SO_ACCEPT_BLEND(&d);
        d.RenderTarget[k].SrcBlend = 0;
        SO_REJECT_BLEND(&d);
        d.RenderTarget[k].SrcBlend = D3D11_BLEND_ONE;
        d.RenderTarget[k].BlendOp = 6;
        SO_REJECT_BLEND(&d);
        d.RenderTarget[k].BlendOp = D3D11_BLEND_OP_ADD;
        d.RenderTarget[k].SrcBlendAlpha = D3D11_BLEND_SRC_COLOR;
        SO_REJECT_BLEND(&d);
    }
}
ST(blend_independent_disabled_rt_bad_factors_ok)
{
    D3D11_BLEND_DESC d = so_blend_default();
    int k;
    SO_NEED();
    d.IndependentBlendEnable = TRUE;
    for (k = 1; k < 8; k++) { d.RenderTarget[k].SrcBlend = 0; d.RenderTarget[k].BlendOp = 100; }
    SO_ACCEPT_BLEND(&d);
}
ST(blend_independent_scan_src_per_rt_via_blend1)
{
    int k, v;
    SO_NEED1();
    for (k = 1; k < 8; k++)
        for (v = -1; v <= 21; v++) {
            D3D11_BLEND_DESC1 d = so_blend1_default();
            HRESULT hr;
            d.IndependentBlendEnable = TRUE;
            d.RenderTarget[k].BlendEnable = TRUE;
            d.RenderTarget[k].SrcBlend = (D3D11_BLEND)v;
            hr = ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL);
            t_check(hr == (color_factor_ok(v, 0) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "rt src", "rt %d value %d got %s", k, v, d3dh_hr_name(hr));
        }
}
ST(blend_independent_scan_destalpha_per_rt_via_blend1)
{
    int k, v;
    SO_NEED1();
    for (k = 1; k < 8; k++)
        for (v = -1; v <= 21; v++) {
            D3D11_BLEND_DESC1 d = so_blend1_default();
            HRESULT hr;
            d.IndependentBlendEnable = TRUE;
            d.RenderTarget[k].BlendEnable = TRUE;
            d.RenderTarget[k].DestBlendAlpha = (D3D11_BLEND)v;
            hr = ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL);
            t_check(hr == (alpha_factor_ok(v, 0) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "rt dstalpha", "rt %d value %d got %s", k, v, d3dh_hr_name(hr));
        }
}
ST(blend_dual_source_on_rt0_accepted)
{
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC1_COLOR;
    d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_COLOR;
    d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC1_ALPHA;
    d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
    SO_ACCEPT_BLEND(&d);
}
ST(blend_dual_source_with_minmax_op_rejected)
{
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC1_COLOR;
    d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;
    SO_REJECT_BLEND(&d);
    d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
    SO_REJECT_BLEND(&d);
    d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_SUBTRACT;
    SO_ACCEPT_BLEND(&d);
}
ST(blend_dual_source_on_rt1_rejected_when_rt0_disabled)
{
    D3D11_BLEND_DESC d = so_blend_default();
    SO_NEED();
    d.IndependentBlendEnable = TRUE;
    d.RenderTarget[1].BlendEnable = TRUE;
    d.RenderTarget[1].SrcBlend = D3D11_BLEND_SRC1_ALPHA;
    SO_REJECT_BLEND(&d);
}
ST(blend_dual_source_rt0_plus_second_enabled_rt_rejected_blend1)
{
    D3D11_BLEND_DESC1 d = so_blend1_default();
    ID3D11BlendState1 *s = SO_SENTINEL(ID3D11BlendState1);
    SO_NEED1();
    d.IndependentBlendEnable = TRUE;
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC1_COLOR;
    d.RenderTarget[1].BlendEnable = TRUE;
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, &s), E_INVALIDARG);
    d.RenderTarget[1].BlendEnable = FALSE;
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL), S_FALSE);
}

/* ---- dedup ------------------------------------------------------------------ */
ST(blend_dedup_same_desc_same_pointer)
{
    D3D11_BLEND_DESC d = so_blend_default();
    ID3D11BlendState *a = NULL, *b = NULL;
    ULONG rc1, rc2;
    SO_NEED();
    enabled_rt0(&d);
    d.RenderTarget[0].RenderTargetWriteMask = 0x05;
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    rc1 = d3dh_refcount(a);
    EXPECT_HR(SO_CBLEND(&d, &b), S_OK);
    EXPECT_EQ(a, b);
    rc2 = d3dh_refcount(a);
    EXPECT_EQ(rc2, rc1 + 1);
    SAFE_RELEASE(b);
    EXPECT_EQ(d3dh_refcount(a), rc1);
    SAFE_RELEASE(a);
}
ST(blend_dedup_distinct_for_different_mask)
{
    D3D11_BLEND_DESC d = so_blend_default();
    ID3D11BlendState *a = NULL, *b = NULL;
    SO_NEED();
    d.RenderTarget[0].RenderTargetWriteMask = 0x01;
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    d.RenderTarget[0].RenderTargetWriteMask = 0x02;
    EXPECT_HR(SO_CBLEND(&d, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_dedup_alpha_to_coverage_distinguishes)
{
    D3D11_BLEND_DESC d = so_blend_default();
    ID3D11BlendState *a = NULL, *b = NULL;
    SO_NEED();
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    d.AlphaToCoverageEnable = TRUE;
    EXPECT_HR(SO_CBLEND(&d, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_dedup_independent_flag_distinguishes)
{
    D3D11_BLEND_DESC d = so_blend_default();
    ID3D11BlendState *a = NULL, *b = NULL;
    SO_NEED();
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    d.IndependentBlendEnable = TRUE;
    EXPECT_HR(SO_CBLEND(&d, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_dedup_nonindependent_ignores_rt1_to_rt7)
{
    D3D11_BLEND_DESC d = so_blend_default(), e = so_blend_default();
    ID3D11BlendState *a = NULL, *b = NULL;
    int i;
    SO_NEED();
    enabled_rt0(&d);
    enabled_rt0(&e);
    for (i = 1; i < 8; i++) {
        e.RenderTarget[i].BlendEnable = TRUE;
        e.RenderTarget[i].SrcBlend = D3D11_BLEND_DEST_COLOR;
        e.RenderTarget[i].RenderTargetWriteMask = 0x03;
    }
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    EXPECT_HR(SO_CBLEND(&e, &b), S_OK);
    EXPECT_EQ(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_dedup_disabled_rt_ignores_factors)
{
    D3D11_BLEND_DESC d = so_blend_default(), e = so_blend_default();
    ID3D11BlendState *a = NULL, *b = NULL;
    SO_NEED();
    e.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
    e.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_ALPHA_SAT;
    e.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
    e.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
    e.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    e.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MIN;
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    EXPECT_HR(SO_CBLEND(&e, &b), S_OK);
    EXPECT_EQ(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_dedup_enabled_rt_factors_distinguish)
{
    D3D11_BLEND_DESC d = so_blend_default(), e;
    ID3D11BlendState *a = NULL, *b = NULL;
    SO_NEED();
    enabled_rt0(&d);
    e = d;
    e.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    EXPECT_HR(SO_CBLEND(&e, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_dedup_independent_rt3_distinguishes)
{
    D3D11_BLEND_DESC d = so_blend_default(), e;
    ID3D11BlendState *a = NULL, *b = NULL;
    SO_NEED();
    d.IndependentBlendEnable = TRUE;
    e = d;
    e.RenderTarget[3].RenderTargetWriteMask = 0x07;
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    EXPECT_HR(SO_CBLEND(&e, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_dedup_nonzero_bool_equals_true)
{
    D3D11_BLEND_DESC d = so_blend_default(), e;
    ID3D11BlendState *a = NULL, *b = NULL;
    SO_NEED();
    enabled_rt0(&d);
    e = d;
    e.RenderTarget[0].BlendEnable = 7;
    d.AlphaToCoverageEnable = TRUE;
    e.AlphaToCoverageEnable = 9;
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    EXPECT_HR(SO_CBLEND(&e, &b), S_OK);
    EXPECT_EQ(a, b);
    if (b) {
        D3D11_BLEND_DESC g;
        ID3D11BlendState_GetDesc(b, &g);
        EXPECT_EQ(g.AlphaToCoverageEnable, 1);
        EXPECT_EQ(g.RenderTarget[0].BlendEnable, 1);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_dedup_blend1_and_blend0_share_object)
{
    D3D11_BLEND_DESC d = so_blend_default();
    D3D11_BLEND_DESC1 d1 = so_blend1_default();
    ID3D11BlendState *a = NULL;
    ID3D11BlendState1 *b = NULL;
    SO_NEED1();
    enabled_rt0(&d);
    d1.RenderTarget[0].BlendEnable = TRUE;
    d1.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    d1.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    d1.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    d1.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    d1.RenderTarget[0].RenderTargetWriteMask = 0x0B;
    EXPECT_HR(SO_CBLEND(&d, &a), S_OK);
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d1, &b), S_OK);
    EXPECT_EQ(a, b);
    SAFE_RELEASE(a);
    SAFE_RELEASE(b);
}
ST(blend_dedup_logicop_disabled_ignores_logicop)
{
    D3D11_BLEND_DESC1 d = so_blend1_default(), e = so_blend1_default();
    ID3D11BlendState1 *a = NULL, *b = NULL;
    SO_NEED1();
    e.RenderTarget[0].LogicOp = D3D11_LOGIC_OP_XOR;
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, &a), S_OK);
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &e, &b), S_OK);
    EXPECT_EQ(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend_many_unique_states_succeed)
{
    ID3D11BlendState *keep[300];
    int n = 0, i;
    SO_NEED();
    memset(keep, 0, sizeof keep);
    for (i = 0; i < 300; i++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].SrcBlend = (D3D11_BLEND)(1 + i % 11);
        d.RenderTarget[0].DestBlend = (D3D11_BLEND)(1 + (i / 11) % 11);
        d.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)(1 + (i / 121) % 5);
        d.AlphaToCoverageEnable = (i & 1) && (i % 3 == 0);
        hr = SO_CBLEND(&d, &keep[n]);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) n++;
    }
    for (i = 0; i < n; i++) SAFE_RELEASE(keep[i]);
    CHECK(n == 300);
}
ST(blend_state_limit_observation)
{
    /* The documented limit is 4096 unique blend states per device. Observe, do not require. */
    ID3D11BlendState **keep;
    int n = 0, i, firstfail = -1;
    HRESULT lasthr = S_OK;
    SO_NEED();
    keep = (ID3D11BlendState **)calloc(4200, sizeof *keep);
    CHECK(keep != NULL);
    if (!keep) return;
    for (i = 0; i < 4200; i++) {
        D3D11_BLEND_DESC d = so_blend_default();
        HRESULT hr;
        d.IndependentBlendEnable = TRUE;
        d.RenderTarget[0].RenderTargetWriteMask = (UINT8)(i & 15);
        d.RenderTarget[1].RenderTargetWriteMask = (UINT8)((i >> 4) & 15);
        d.RenderTarget[2].RenderTargetWriteMask = (UINT8)((i >> 8) & 15);
        d.RenderTarget[3].RenderTargetWriteMask = (UINT8)((i >> 12) & 15);
        d.RenderTarget[4].BlendEnable = (i / 16) & 1; /* extra variety */
        hr = SO_CBLEND(&d, &keep[n]);
        if (FAILED(hr)) { if (firstfail < 0) { firstfail = i; lasthr = hr; } keep[n] = NULL; }
        else n++;
    }
    t_note("unique blend states created: %d of 4200 (first failure at %d, hr %s)", n, firstfail, d3dh_hr_name(lasthr));
    CHECK(n >= 1000);
    for (i = 0; i < n; i++) SAFE_RELEASE(keep[i]);
    free(keep);
}

/* ---- GetDesc ----------------------------------------------------------------- */
ST(blend_getdesc_default_roundtrip)
{
    D3D11_BLEND_DESC d = so_blend_default(), g;
    ID3D11BlendState *s = NULL;
    int i;
    SO_NEED();
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (!s) return;
    memset(&g, 0xAB, sizeof g);
    ID3D11BlendState_GetDesc(s, &g);
    EXPECT_EQ(g.AlphaToCoverageEnable, 0);
    EXPECT_EQ(g.IndependentBlendEnable, 0);
    for (i = 0; i < 8; i++) CHECKF(blend_eq(&g.RenderTarget[i], &d.RenderTarget[i]), "rt %d", i);
    SAFE_RELEASE(s);
}
ST(blend_getdesc_enabled_rt0_roundtrip)
{
    D3D11_BLEND_DESC d = so_blend_default(), g;
    ID3D11BlendState *s = NULL;
    SO_NEED();
    enabled_rt0(&d);
    d.AlphaToCoverageEnable = TRUE;
    d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
    d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (!s) return;
    ID3D11BlendState_GetDesc(s, &g);
    EXPECT_EQ(g.AlphaToCoverageEnable, 1);
    CHECK(blend_eq(&g.RenderTarget[0], &d.RenderTarget[0]));
    SAFE_RELEASE(s);
}
ST(blend_getdesc_nonindependent_replicates_rt0)
{
    D3D11_BLEND_DESC d = so_blend_default(), g;
    ID3D11BlendState *s = NULL;
    int i;
    SO_NEED();
    enabled_rt0(&d);
    for (i = 1; i < 8; i++) { d.RenderTarget[i].BlendEnable = TRUE; d.RenderTarget[i].SrcBlend = D3D11_BLEND_DEST_COLOR; }
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (!s) return;
    ID3D11BlendState_GetDesc(s, &g);
    EXPECT_EQ(g.IndependentBlendEnable, 0);
    for (i = 1; i < 8; i++) CHECKF(blend_eq(&g.RenderTarget[i], &g.RenderTarget[0]), "rt %d differs from rt0", i);
    SAFE_RELEASE(s);
}
ST(blend_getdesc_independent_roundtrip)
{
    D3D11_BLEND_DESC d = so_blend_default(), g;
    ID3D11BlendState *s = NULL;
    int i;
    SO_NEED();
    d.IndependentBlendEnable = TRUE;
    for (i = 0; i < 8; i++) {
        d.RenderTarget[i].BlendEnable = (i & 1);
        d.RenderTarget[i].RenderTargetWriteMask = (UINT8)(i + 1);
        if (i & 1) {
            d.RenderTarget[i].SrcBlend = (D3D11_BLEND)(2 + i);
            d.RenderTarget[i].DestBlend = (D3D11_BLEND)(1 + i);
            d.RenderTarget[i].BlendOp = (D3D11_BLEND_OP)(1 + i % 5);
            d.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
            d.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            d.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_SUBTRACT;
        }
    }
    d.RenderTarget[1].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    d.RenderTarget[1].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (!s) return;
    ID3D11BlendState_GetDesc(s, &g);
    EXPECT_EQ(g.IndependentBlendEnable, 1);
    for (i = 0; i < 8; i++) {
        EXPECT_EQ(g.RenderTarget[i].BlendEnable, (i & 1));
        EXPECT_EQ(g.RenderTarget[i].RenderTargetWriteMask, i + 1);
        if (i & 1) CHECKF(blend_eq(&g.RenderTarget[i], &d.RenderTarget[i]), "rt %d", i);
    }
    SAFE_RELEASE(s);
}
ST(blend_getdesc_disabled_rt0_reports_default_factors)
{
    D3D11_BLEND_DESC d = so_blend_default(), e = so_blend_default(), g, h;
    ID3D11BlendState *s = NULL, *t = NULL;
    SO_NEED();
    e.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
    e.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
    e.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    EXPECT_HR(SO_CBLEND(&e, &t), S_OK);
    if (!s || !t) return;
    ID3D11BlendState_GetDesc(s, &g);
    ID3D11BlendState_GetDesc(t, &h);
    CHECK(blend_eq(&g.RenderTarget[0], &h.RenderTarget[0]));
    CHECK(blend_eq(&g.RenderTarget[0], &d.RenderTarget[0]));
    SAFE_RELEASE(s); SAFE_RELEASE(t);
}
ST(blend_getdesc_disabled_independent_rt_reports)
{
    /* what GetDesc reports for a disabled independent RT with unusual factors is not specified here */
    D3D11_BLEND_DESC d = so_blend_default(), g;
    ID3D11BlendState *s = NULL;
    SO_NEED();
    d.IndependentBlendEnable = TRUE;
    d.RenderTarget[2].SrcBlend = D3D11_BLEND_DEST_COLOR;
    d.RenderTarget[2].RenderTargetWriteMask = 0x9;
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (!s) return;
    ID3D11BlendState_GetDesc(s, &g);
    EXPECT_EQ(g.RenderTarget[2].RenderTargetWriteMask, 9);
    EXPECT_EQ(g.RenderTarget[2].BlendEnable, 0);
    t_note("disabled independent rt2 SrcBlend reported as %d (was %d)", (int)g.RenderTarget[2].SrcBlend, (int)d.RenderTarget[2].SrcBlend);
    SAFE_RELEASE(s);
}
ST(blend_getdesc_write_mask_every_value)
{
    int m;
    SO_NEED();
    for (m = 0; m < 16; m++) {
        D3D11_BLEND_DESC d = so_blend_default(), g;
        ID3D11BlendState *s = NULL;
        d.RenderTarget[0].RenderTargetWriteMask = (UINT8)m;
        EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
        if (!s) continue;
        ID3D11BlendState_GetDesc(s, &g);
        t_check(g.RenderTarget[0].RenderTargetWriteMask == m, __FILE__, __LINE__, "mask", "mask %d got %d", m, g.RenderTarget[0].RenderTargetWriteMask);
        SAFE_RELEASE(s);
    }
}
ST(blend_getdesc_every_factor_roundtrips)
{
    int v;
    SO_NEED();
    for (v = 1; v <= 15; v++) {
        D3D11_BLEND_DESC d = so_blend_default(), g;
        ID3D11BlendState *s = NULL;
        if (!color_factor_ok(v, 0)) continue;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].SrcBlend = (D3D11_BLEND)v;
        d.RenderTarget[0].DestBlend = (D3D11_BLEND)(16 - v > 15 ? 1 : (color_factor_ok(16 - v, 0) ? 16 - v : 2));
        EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
        if (!s) continue;
        ID3D11BlendState_GetDesc(s, &g);
        t_check(blend_eq(&g.RenderTarget[0], &d.RenderTarget[0]), __FILE__, __LINE__, "factor roundtrip", "src %d", v);
        SAFE_RELEASE(s);
    }
}
ST(blend_getdesc_dual_source_roundtrip)
{
    D3D11_BLEND_DESC d = so_blend_default(), g;
    ID3D11BlendState *s = NULL;
    SO_NEED();
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC1_COLOR;
    d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_ALPHA;
    d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC1_ALPHA;
    d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (!s) return;
    ID3D11BlendState_GetDesc(s, &g);
    CHECK(blend_eq(&g.RenderTarget[0], &d.RenderTarget[0]));
    SAFE_RELEASE(s);
}

/* ---- BlendState1 / logic ops ------------------------------------------------- */
ST(blend1_null_desc)
{
    ID3D11BlendState1 *s = SO_SENTINEL(ID3D11BlendState1);
    SO_NEED1();
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), NULL, &s), E_INVALIDARG);
}
ST(blend1_default_validate_and_create)
{
    D3D11_BLEND_DESC1 d = so_blend1_default(), g;
    ID3D11BlendState1 *s = NULL;
    int i;
    SO_NEED1();
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, &s), S_OK);
    if (!s) return;
    memset(&g, 0xEE, sizeof g);
    ID3D11BlendState1_GetDesc1(s, &g);
    EXPECT_EQ(g.AlphaToCoverageEnable, 0);
    EXPECT_EQ(g.IndependentBlendEnable, 0);
    for (i = 0; i < 8; i++) {
        EXPECT_EQ(g.RenderTarget[i].LogicOpEnable, 0);
        EXPECT_EQ(g.RenderTarget[i].LogicOp, D3D11_LOGIC_OP_NOOP);
        EXPECT_EQ(g.RenderTarget[i].RenderTargetWriteMask, 0xF);
    }
    SAFE_RELEASE(s);
}
ST(blend1_logicop_enabled_accepted_when_supported)
{
    D3D11_BLEND_DESC1 d = so_blend1_default();
    HRESULT hr;
    SO_NEED1();
    d.RenderTarget[0].LogicOpEnable = TRUE;
    d.RenderTarget[0].LogicOp = D3D11_LOGIC_OP_XOR;
    hr = ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL);
    if (so_logic_ops()) EXPECT_HR(hr, S_FALSE);
    else { t_note("OutputMergerLogicOp unsupported, logic-op blend state gives %s", d3dh_hr_name(hr)); }
}
ST(blend1_logicop_scan_values)
{
    int v;
    SO_NEED1();
    if (!so_logic_ops()) { t_skip("logic ops unsupported"); return; }
    for (v = -2; v <= 18; v++) {
        D3D11_BLEND_DESC1 d = so_blend1_default();
        HRESULT hr;
        d.RenderTarget[0].LogicOpEnable = TRUE;
        d.RenderTarget[0].LogicOp = (D3D11_LOGIC_OP)v;
        hr = ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL);
        SCAN_CHECK(hr, v >= 0 && v <= 15, v);
    }
}
ST(blend1_logicop_with_blend_enable_rejected)
{
    D3D11_BLEND_DESC1 d = so_blend1_default();
    SO_NEED1();
    if (!so_logic_ops()) { t_skip("logic ops unsupported"); return; }
    d.RenderTarget[0].LogicOpEnable = TRUE;
    d.RenderTarget[0].BlendEnable = TRUE;
    d.RenderTarget[0].LogicOp = D3D11_LOGIC_OP_COPY;
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL), E_INVALIDARG);
}
ST(blend1_logicop_with_independent_blend_rejected)
{
    D3D11_BLEND_DESC1 d = so_blend1_default();
    SO_NEED1();
    if (!so_logic_ops()) { t_skip("logic ops unsupported"); return; }
    d.IndependentBlendEnable = TRUE;
    d.RenderTarget[0].LogicOpEnable = TRUE;
    d.RenderTarget[0].LogicOp = D3D11_LOGIC_OP_COPY;
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL), E_INVALIDARG);
}
ST(blend1_logicop_disabled_logicop_value_not_validated)
{
    D3D11_BLEND_DESC1 d = so_blend1_default();
    SO_NEED1();
    d.RenderTarget[0].LogicOp = (D3D11_LOGIC_OP)99;
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL), S_FALSE);
}
ST(blend1_logicop_getdesc_roundtrip)
{
    D3D11_BLEND_DESC1 d = so_blend1_default(), g;
    ID3D11BlendState1 *s = NULL;
    int op;
    SO_NEED1();
    if (!so_logic_ops()) { t_skip("logic ops unsupported"); return; }
    for (op = 0; op <= 15; op++) {
        d.RenderTarget[0].LogicOpEnable = TRUE;
        d.RenderTarget[0].LogicOp = (D3D11_LOGIC_OP)op;
        d.RenderTarget[0].RenderTargetWriteMask = 0x6;
        EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, &s), S_OK);
        if (!s) continue;
        ID3D11BlendState1_GetDesc1(s, &g);
        t_check(g.RenderTarget[0].LogicOpEnable == 1 && g.RenderTarget[0].LogicOp == op && g.RenderTarget[0].BlendEnable == 0 &&
                g.RenderTarget[0].RenderTargetWriteMask == 6, __FILE__, __LINE__, "logic roundtrip", "op %d: en %d op %d mask %d", op,
                g.RenderTarget[0].LogicOpEnable, g.RenderTarget[0].LogicOp, g.RenderTarget[0].RenderTargetWriteMask);
        SAFE_RELEASE(s);
    }
}
ST(blend1_logicop_dedup_ignores_blend_factors)
{
    D3D11_BLEND_DESC1 d = so_blend1_default(), e;
    ID3D11BlendState1 *a = NULL, *b = NULL;
    SO_NEED1();
    if (!so_logic_ops()) { t_skip("logic ops unsupported"); return; }
    d.RenderTarget[0].LogicOpEnable = TRUE;
    d.RenderTarget[0].LogicOp = D3D11_LOGIC_OP_AND;
    e = d;
    e.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
    e.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &d, &a), S_OK);
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &e, &b), S_OK);
    EXPECT_EQ(a, b);
    e.RenderTarget[0].LogicOp = D3D11_LOGIC_OP_OR;
    SAFE_RELEASE(b);
    EXPECT_HR(ID3D11Device1_CreateBlendState1(so_dev1(), &e, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(blend1_getdesc_matches_getdesc_of_base_interface)
{
    D3D11_BLEND_DESC d = so_blend_default(), g0;
    D3D11_BLEND_DESC1 g1;
    ID3D11BlendState *s = NULL;
    ID3D11BlendState1 *s1 = NULL;
    int i;
    SO_NEED1();
    enabled_rt0(&d);
    d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MIN;
    d.IndependentBlendEnable = TRUE;
    d.RenderTarget[4].RenderTargetWriteMask = 3;
    EXPECT_HR(SO_CBLEND(&d, &s), S_OK);
    if (!s) return;
    EXPECT_HR(ID3D11BlendState_QueryInterface(s, &IID_ID3D11BlendState1, (void **)&s1), S_OK);
    if (s1) {
        ID3D11BlendState_GetDesc(s, &g0);
        ID3D11BlendState1_GetDesc1(s1, &g1);
        EXPECT_EQ(g0.AlphaToCoverageEnable, g1.AlphaToCoverageEnable);
        EXPECT_EQ(g0.IndependentBlendEnable, g1.IndependentBlendEnable);
        for (i = 0; i < 8; i++) {
            EXPECT_EQ(g0.RenderTarget[i].BlendEnable, g1.RenderTarget[i].BlendEnable);
            EXPECT_EQ(g0.RenderTarget[i].SrcBlend, g1.RenderTarget[i].SrcBlend);
            EXPECT_EQ(g0.RenderTarget[i].DestBlendAlpha, g1.RenderTarget[i].DestBlendAlpha);
            EXPECT_EQ(g0.RenderTarget[i].RenderTargetWriteMask, g1.RenderTarget[i].RenderTargetWriteMask);
            EXPECT_EQ(g1.RenderTarget[i].LogicOpEnable, 0);
        }
        SAFE_RELEASE(s1);
    }
    SAFE_RELEASE(s);
}
ST(blend1_scan_srcblend_rt0)
{
    int v;
    SO_NEED1();
    for (v = -2; v <= 24; v++) {
        D3D11_BLEND_DESC1 d = so_blend1_default();
        HRESULT hr;
        d.RenderTarget[0].BlendEnable = TRUE;
        d.RenderTarget[0].SrcBlend = (D3D11_BLEND)v;
        hr = ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL);
        SCAN_CHECK(hr, color_factor_ok(v, 1), v);
    }
}
ST(blend1_scan_writemask_all_rts_independent)
{
    int k, v;
    SO_NEED1();
    for (k = 0; k < 8; k++)
        for (v = 0; v < 256; v += 3) {
            D3D11_BLEND_DESC1 d = so_blend1_default();
            HRESULT hr;
            d.IndependentBlendEnable = TRUE;
            d.RenderTarget[k].RenderTargetWriteMask = (UINT8)v;
            hr = ID3D11Device1_CreateBlendState1(so_dev1(), &d, NULL);
            t_check(hr == (v <= 15 ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "mask", "rt %d mask %d got %s", k, v, d3dh_hr_name(hr));
        }
}
