/* State objects (blend, depth-stencil, rasterizer, sampler): creation, normalisation, de-duplication, pipeline get/set, ClearState. */
#include "dd.h"

#define T(n) TEST(d3d10_device_state_##n)

static D3D10_BLEND_DESC blend_default(void)
{
    D3D10_BLEND_DESC d; memset(&d, 0, sizeof d);
    d.SrcBlend = D3D10_BLEND_ONE; d.DestBlend = D3D10_BLEND_ZERO; d.BlendOp = D3D10_BLEND_OP_ADD;
    d.SrcBlendAlpha = D3D10_BLEND_ONE; d.DestBlendAlpha = D3D10_BLEND_ZERO; d.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    for (int i = 0; i < 8; i++) d.RenderTargetWriteMask[i] = D3D10_COLOR_WRITE_ENABLE_ALL;
    return d;
}
static D3D10_DEPTH_STENCIL_DESC ds_default(void)
{
    D3D10_DEPTH_STENCIL_DESC d; memset(&d, 0, sizeof d);
    d.DepthEnable = TRUE; d.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D10_COMPARISON_LESS;
    d.StencilReadMask = 0xff; d.StencilWriteMask = 0xff;
    d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = d.FrontFace.StencilPassOp = D3D10_STENCIL_OP_KEEP; d.FrontFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
    d.BackFace = d.FrontFace;
    return d;
}
static D3D10_RASTERIZER_DESC rs_default(void)
{
    D3D10_RASTERIZER_DESC d; memset(&d, 0, sizeof d);
    d.FillMode = D3D10_FILL_SOLID; d.CullMode = D3D10_CULL_BACK; d.DepthClipEnable = TRUE;
    return d;
}
static D3D10_SAMPLER_DESC smp_default(void)
{
    D3D10_SAMPLER_DESC d; memset(&d, 0, sizeof d);
    d.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
    d.AddressU = d.AddressV = d.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
    d.MaxAnisotropy = 1; d.ComparisonFunc = D3D10_COMPARISON_NEVER; d.MinLOD = 0; d.MaxLOD = D3D10_FLOAT32_MAX;
    return d;
}

/* ---- blend ---- */
T(blend_null_desc)
{
    DD_DEV(dev);
    ID3D10BlendState *b = (void *)1;
    EXPECT_HR(ID3D10Device_CreateBlendState(dev, NULL, &b), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateBlendState(dev, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(blend_null_out_is_s_false)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC d = blend_default();
    EXPECT_HR(ID3D10Device_CreateBlendState(dev, &d, NULL), S_FALSE);
    d.SrcBlend = 0; d.BlendEnable[0] = TRUE;
    EXPECT_HR(ID3D10Device_CreateBlendState(dev, &d, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(blend_dedup_same_object)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC d = blend_default();
    ID3D10BlendState *a = NULL, *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &d, &a));
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &d, &b));
    CHECK(a == b);
    if (a) {
        EXPECT_EQ(d3dh_refcount(a), 2);
        ID3D10BlendState *o = NULL; ID3D10Device *got = NULL;
        ID3D10BlendState_GetDevice(a, &got); CHECK(got == dev); SAFE_RELEASE(got);
        (void)o;
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DD_END(dev);
}
T(blend_different_desc_different_object)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC d = blend_default(), e;
    ID3D10BlendState *a = NULL, *b = NULL;
    e = d; e.AlphaToCoverageEnable = TRUE;
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &d, &a));
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &e, &b));
    if (a && b) CHECK(a != b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DD_END(dev);
}
T(blend_default_roundtrip)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC d, g; ID3D10BlendState *s = NULL;
    memset(&d, 0, sizeof d);
    for (int i = 0; i < 8; i++) d.RenderTargetWriteMask[i] = D3D10_COLOR_WRITE_ENABLE_ALL;
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &d, &s));
    if (s) {
        memset(&g, 0xcc, sizeof g);
        ID3D10BlendState_GetDesc(s, &g);
        EXPECT_EQ(g.AlphaToCoverageEnable, FALSE);
        EXPECT_EQ(g.SrcBlend, D3D10_BLEND_ONE); EXPECT_EQ(g.DestBlend, D3D10_BLEND_ZERO); EXPECT_EQ(g.BlendOp, D3D10_BLEND_OP_ADD);
        EXPECT_EQ(g.SrcBlendAlpha, D3D10_BLEND_ONE); EXPECT_EQ(g.DestBlendAlpha, D3D10_BLEND_ZERO); EXPECT_EQ(g.BlendOpAlpha, D3D10_BLEND_OP_ADD);
        for (int i = 0; i < 8; i++) { EXPECT_EQ(g.BlendEnable[i], FALSE); EXPECT_EQ(g.RenderTargetWriteMask[i], D3D10_COLOR_WRITE_ENABLE_ALL); }
        ID3D10BlendState_Release(s);
    }
    DD_END(dev);
}
T(blend_full_roundtrip)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC d = blend_default(), g; ID3D10BlendState *s = NULL;
    d.AlphaToCoverageEnable = TRUE;
    d.SrcBlend = D3D10_BLEND_SRC_ALPHA; d.DestBlend = D3D10_BLEND_INV_SRC_ALPHA; d.BlendOp = D3D10_BLEND_OP_SUBTRACT;
    d.SrcBlendAlpha = D3D10_BLEND_ONE; d.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA; d.BlendOpAlpha = D3D10_BLEND_OP_MAX;
    d.BlendEnable[0] = TRUE; d.BlendEnable[3] = TRUE;
    d.RenderTargetWriteMask[1] = D3D10_COLOR_WRITE_ENABLE_RED; d.RenderTargetWriteMask[2] = D3D10_COLOR_WRITE_ENABLE_GREEN | D3D10_COLOR_WRITE_ENABLE_ALPHA; d.RenderTargetWriteMask[7] = 0;
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &d, &s));
    if (s) {
        ID3D10BlendState_GetDesc(s, &g);
        CHECK(!memcmp(&g, &d, sizeof d));
        ID3D10BlendState_Release(s);
    }
    DD_END(dev);
}
T(blend_disabled_state_is_normalised)
{
    /* with all BlendEnable[] false the blend factors/ops are not validated and read back as the defaults */
    DD_DEV(dev);
    D3D10_BLEND_DESC d, g; ID3D10BlendState *s = NULL;
    memset(&d, 0, sizeof d);
    for (int i = 0; i < 8; i++) d.RenderTargetWriteMask[i] = D3D10_COLOR_WRITE_ENABLE_ALL;
    d.SrcBlend = d.DestBlend = d.SrcBlendAlpha = d.DestBlendAlpha = 0; d.BlendOp = d.BlendOpAlpha = 0;
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &d, &s));
    if (s) { ID3D10BlendState_GetDesc(s, &g); EXPECT_EQ(g.SrcBlend, D3D10_BLEND_ONE); EXPECT_EQ(g.DestBlend, D3D10_BLEND_ZERO); EXPECT_EQ(g.BlendOp, D3D10_BLEND_OP_ADD); ID3D10BlendState_Release(s); }
    DD_END(dev);
}
T(blend_invalid_enums_when_enabled)
{
    static const UINT bad[] = { 0, 20, 21, 100, 0xffffffffu };
    DD_DEV(dev);
    for (unsigned i = 0; i < 5; i++) {
        D3D10_BLEND_DESC d = blend_default(); ID3D10BlendState *s = (void *)1; HRESULT hr;
        d.BlendEnable[0] = TRUE; d.SrcBlend = (D3D10_BLEND)bad[i];
        hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "SrcBlend", "value %u: %s", bad[i], d3dh_hr_name(hr));
        d = blend_default(); d.BlendEnable[0] = TRUE; d.DestBlend = (D3D10_BLEND)bad[i];
        hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "DestBlend", "value %u: %s", bad[i], d3dh_hr_name(hr));
        d = blend_default(); d.BlendEnable[0] = TRUE; d.SrcBlendAlpha = (D3D10_BLEND)bad[i];
        hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "SrcBlendAlpha", "value %u: %s", bad[i], d3dh_hr_name(hr));
        d = blend_default(); d.BlendEnable[0] = TRUE; d.DestBlendAlpha = (D3D10_BLEND)bad[i];
        hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "DestBlendAlpha", "value %u: %s", bad[i], d3dh_hr_name(hr));
    }
    DD_END(dev);
}
T(blend_invalid_ops_when_enabled)
{
    static const UINT bad[] = { 0, 6, 7, 100, 0xffffffffu };
    DD_DEV(dev);
    for (unsigned i = 0; i < 5; i++) {
        D3D10_BLEND_DESC d = blend_default(); ID3D10BlendState *s = (void *)1; HRESULT hr;
        d.BlendEnable[0] = TRUE; d.BlendOp = (D3D10_BLEND_OP)bad[i];
        hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "BlendOp", "value %u: %s", bad[i], d3dh_hr_name(hr));
        d = blend_default(); d.BlendEnable[0] = TRUE; d.BlendOpAlpha = (D3D10_BLEND_OP)bad[i];
        hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "BlendOpAlpha", "value %u: %s", bad[i], d3dh_hr_name(hr));
    }
    DD_END(dev);
}
T(blend_all_valid_factor_values)
{
    DD_DEV(dev);
    for (UINT f = 1; f <= 19; f++) {
        D3D10_BLEND_DESC d = blend_default(); ID3D10BlendState *s = NULL; HRESULT hr;
        d.BlendEnable[0] = TRUE; d.SrcBlend = (D3D10_BLEND)f; d.DestBlend = D3D10_BLEND_ZERO;
        hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        if ((f >= 1 && f <= 11) || (f >= 16 && f <= 19)) t_check(SUCCEEDED(hr), __FILE__, __LINE__, "src factor", "value %u: %s", f, d3dh_hr_name(hr));
        else t_note("SrcBlend %u: %s", f, d3dh_hr_name(hr));
        SAFE_RELEASE(s);
        d = blend_default(); d.BlendEnable[0] = TRUE; d.SrcBlend = D3D10_BLEND_ONE; d.DestBlend = (D3D10_BLEND)f;
        hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        if ((f >= 1 && f <= 10) || (f >= 16 && f <= 19)) t_check(SUCCEEDED(hr), __FILE__, __LINE__, "dest factor", "value %u: %s", f, d3dh_hr_name(hr));
        else t_note("DestBlend %u: %s", f, d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    DD_END(dev);
}
T(blend_alpha_color_factors_observed)
{
    DD_DEV(dev);
    static const D3D10_BLEND f[] = { D3D10_BLEND_SRC_COLOR, D3D10_BLEND_INV_SRC_COLOR, D3D10_BLEND_DEST_COLOR, D3D10_BLEND_INV_DEST_COLOR };
    for (unsigned i = 0; i < 4; i++) {
        D3D10_BLEND_DESC d = blend_default(); ID3D10BlendState *s = NULL;
        d.BlendEnable[0] = TRUE; d.SrcBlendAlpha = f[i];
        HRESULT hr = ID3D10Device_CreateBlendState(dev, &d, &s);
        t_note("SrcBlendAlpha=%u: %s", f[i], d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    DD_END(dev);
}
T(blend_write_mask_high_bits_observed)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC d = blend_default(); ID3D10BlendState *s = NULL;
    d.RenderTargetWriteMask[0] = 0x1f;
    HRESULT hr = ID3D10Device_CreateBlendState(dev, &d, &s);
    t_note("write mask 0x1f: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(s);
    DD_END(dev);
}
T(blend_qi_d3d10_1_observed)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC d = blend_default(); ID3D10BlendState *s = NULL; void *p = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &d, &s));
    if (s) {
        HRESULT hr = ID3D10BlendState_QueryInterface(s, &IID_ID3D10BlendState1, &p);
        t_note("blend state QI BlendState1: %s", d3dh_hr_name(hr)); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; hr = ID3D10BlendState_QueryInterface(s, &IID_ID3D11BlendState, &p);
        t_note("blend state QI ID3D11BlendState: %s", d3dh_hr_name(hr)); if (p) IUnknown_Release((IUnknown *)p);
        ID3D10BlendState_Release(s);
    }
    DD_END(dev);
}
T(blend1_independent)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_1);
    D3D10_BLEND_DESC1 d, g; ID3D10BlendState1 *s = NULL;
    memset(&d, 0, sizeof d);
    d.IndependentBlendEnable = TRUE;
    for (int i = 0; i < 8; i++) {
        d.RenderTarget[i].SrcBlend = D3D10_BLEND_ONE; d.RenderTarget[i].DestBlend = D3D10_BLEND_ZERO; d.RenderTarget[i].BlendOp = D3D10_BLEND_OP_ADD;
        d.RenderTarget[i].SrcBlendAlpha = D3D10_BLEND_ONE; d.RenderTarget[i].DestBlendAlpha = D3D10_BLEND_ZERO; d.RenderTarget[i].BlendOpAlpha = D3D10_BLEND_OP_ADD;
        d.RenderTarget[i].RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_ALL;
    }
    d.RenderTarget[0].BlendEnable = TRUE; d.RenderTarget[0].SrcBlend = D3D10_BLEND_SRC_ALPHA; d.RenderTarget[0].DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
    d.RenderTarget[1].BlendEnable = TRUE; d.RenderTarget[1].SrcBlend = D3D10_BLEND_ONE; d.RenderTarget[1].DestBlend = D3D10_BLEND_ONE;
    d.RenderTarget[2].RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_BLUE;
    EXPECT_HR_OK(ID3D10Device1_CreateBlendState1(dev, &d, &s));
    if (s) {
        memset(&g, 0, sizeof g); ID3D10BlendState1_GetDesc1(s, &g);
        EXPECT_EQ(g.IndependentBlendEnable, TRUE);
        for (int i = 0; i < 8; i++) {
            t_check(!memcmp(&g.RenderTarget[i], &d.RenderTarget[i], sizeof d.RenderTarget[i]), __FILE__, __LINE__, "rt desc", "render target %d differs", i);
        }
        D3D10_BLEND_DESC old; ID3D10BlendState1_GetDesc(s, &old);
        t_note("GetDesc on independent blend: SrcBlend %u BlendEnable[1] %d", old.SrcBlend, old.BlendEnable[1]);
        ID3D10BlendState1_Release(s);
    }
    ID3D10Device1_Release(dev);
}
T(blend1_null_and_dedup)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_1);
    D3D10_BLEND_DESC1 d; ID3D10BlendState1 *a = NULL, *b = NULL;
    memset(&d, 0, sizeof d);
    for (int i = 0; i < 8; i++) {
        d.RenderTarget[i].SrcBlend = D3D10_BLEND_ONE; d.RenderTarget[i].DestBlend = D3D10_BLEND_ZERO; d.RenderTarget[i].BlendOp = D3D10_BLEND_OP_ADD;
        d.RenderTarget[i].SrcBlendAlpha = D3D10_BLEND_ONE; d.RenderTarget[i].DestBlendAlpha = D3D10_BLEND_ZERO; d.RenderTarget[i].BlendOpAlpha = D3D10_BLEND_OP_ADD;
        d.RenderTarget[i].RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_ALL;
    }
    EXPECT_HR(ID3D10Device1_CreateBlendState1(dev, NULL, &a), E_INVALIDARG);
    EXPECT_HR(ID3D10Device1_CreateBlendState1(dev, &d, NULL), S_FALSE);
    EXPECT_HR_OK(ID3D10Device1_CreateBlendState1(dev, &d, &a));
    EXPECT_HR_OK(ID3D10Device1_CreateBlendState1(dev, &d, &b));
    CHECK(a == b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    ID3D10Device1_Release(dev);
}

/* ---- depth stencil ---- */
T(ds_null_desc)
{
    DD_DEV(dev);
    ID3D10DepthStencilState *s = (void *)1;
    EXPECT_HR(ID3D10Device_CreateDepthStencilState(dev, NULL, &s), E_INVALIDARG);
    DD_END(dev);
}
T(ds_null_out_is_s_false)
{
    DD_DEV(dev);
    D3D10_DEPTH_STENCIL_DESC d = ds_default();
    EXPECT_HR(ID3D10Device_CreateDepthStencilState(dev, &d, NULL), S_FALSE);
    DD_END(dev);
}
T(ds_dedup)
{
    DD_DEV(dev);
    D3D10_DEPTH_STENCIL_DESC d = ds_default(); ID3D10DepthStencilState *a = NULL, *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &d, &a));
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &d, &b));
    CHECK(a == b);
    if (a) EXPECT_EQ(d3dh_refcount(a), 2);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DD_END(dev);
}
T(ds_full_roundtrip)
{
    DD_DEV(dev);
    D3D10_DEPTH_STENCIL_DESC d = ds_default(), g; ID3D10DepthStencilState *s = NULL;
    d.DepthEnable = TRUE; d.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO; d.DepthFunc = D3D10_COMPARISON_GREATER_EQUAL;
    d.StencilEnable = TRUE; d.StencilReadMask = 0x0f; d.StencilWriteMask = 0xf0;
    d.FrontFace.StencilFailOp = D3D10_STENCIL_OP_INCR; d.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_DECR_SAT; d.FrontFace.StencilPassOp = D3D10_STENCIL_OP_REPLACE; d.FrontFace.StencilFunc = D3D10_COMPARISON_EQUAL;
    d.BackFace.StencilFailOp = D3D10_STENCIL_OP_ZERO; d.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_INVERT; d.BackFace.StencilPassOp = D3D10_STENCIL_OP_INCR_SAT; d.BackFace.StencilFunc = D3D10_COMPARISON_NOT_EQUAL;
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &d, &s));
    if (s) { ID3D10DepthStencilState_GetDesc(s, &g); CHECK(!memcmp(&g, &d, sizeof d)); ID3D10DepthStencilState_Release(s); }
    DD_END(dev);
}
T(ds_disabled_is_normalised)
{
    DD_DEV(dev);
    D3D10_DEPTH_STENCIL_DESC d, g; ID3D10DepthStencilState *s = NULL;
    memset(&d, 0, sizeof d);
    d.DepthEnable = FALSE; d.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO; d.DepthFunc = D3D10_COMPARISON_NEVER; d.StencilEnable = FALSE;
    d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = d.FrontFace.StencilPassOp = D3D10_STENCIL_OP_ZERO; d.FrontFace.StencilFunc = D3D10_COMPARISON_NEVER;
    d.BackFace = d.FrontFace;
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &d, &s));
    if (s) {
        memset(&g, 0, sizeof g); ID3D10DepthStencilState_GetDesc(s, &g);
        EXPECT_EQ(g.DepthEnable, FALSE); EXPECT_EQ(g.StencilEnable, FALSE);
        EXPECT_EQ(g.DepthWriteMask, D3D10_DEPTH_WRITE_MASK_ALL); EXPECT_EQ(g.DepthFunc, D3D10_COMPARISON_LESS);
        EXPECT_EQ(g.StencilReadMask, 0xff); EXPECT_EQ(g.StencilWriteMask, 0xff);
        EXPECT_EQ(g.FrontFace.StencilFailOp, D3D10_STENCIL_OP_KEEP); EXPECT_EQ(g.FrontFace.StencilDepthFailOp, D3D10_STENCIL_OP_KEEP);
        EXPECT_EQ(g.FrontFace.StencilPassOp, D3D10_STENCIL_OP_KEEP); EXPECT_EQ(g.FrontFace.StencilFunc, D3D10_COMPARISON_ALWAYS);
        EXPECT_EQ(g.BackFace.StencilFailOp, D3D10_STENCIL_OP_KEEP); EXPECT_EQ(g.BackFace.StencilDepthFailOp, D3D10_STENCIL_OP_KEEP);
        EXPECT_EQ(g.BackFace.StencilPassOp, D3D10_STENCIL_OP_KEEP); EXPECT_EQ(g.BackFace.StencilFunc, D3D10_COMPARISON_ALWAYS);
        ID3D10DepthStencilState_Release(s);
    }
    DD_END(dev);
}
T(ds_disabled_same_object_as_defaults)
{
    DD_DEV(dev);
    D3D10_DEPTH_STENCIL_DESC a = ds_default(), b = ds_default(); ID3D10DepthStencilState *sa = NULL, *sb = NULL;
    a.DepthEnable = FALSE;
    b.DepthEnable = FALSE; b.DepthFunc = D3D10_COMPARISON_NEVER; b.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &a, &sa));
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &b, &sb));
    t_note("normalised-equal depth states share an object: %d", sa == sb);
    SAFE_RELEASE(sa); SAFE_RELEASE(sb);
    DD_END(dev);
}
T(ds_stencil_disabled_normalises_masks_only)
{
    DD_DEV(dev);
    D3D10_DEPTH_STENCIL_DESC d = ds_default(), g; ID3D10DepthStencilState *s = NULL;
    d.StencilEnable = FALSE; d.StencilReadMask = 0x12; d.StencilWriteMask = 0x34; d.FrontFace.StencilFunc = D3D10_COMPARISON_NEVER;
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &d, &s));
    if (s) {
        ID3D10DepthStencilState_GetDesc(s, &g);
        EXPECT_EQ(g.DepthEnable, TRUE);
        t_note("stencil disabled: masks %02x/%02x front func %u", g.StencilReadMask, g.StencilWriteMask, g.FrontFace.StencilFunc);
        ID3D10DepthStencilState_Release(s);
    }
    DD_END(dev);
}
T(ds_invalid_enums)
{
    static const UINT badfunc[] = { 0, 9, 100, 0xffffffffu };
    DD_DEV(dev);
    for (unsigned i = 0; i < 4; i++) {
        D3D10_DEPTH_STENCIL_DESC d = ds_default(); ID3D10DepthStencilState *s = (void *)1; HRESULT hr;
        d.DepthFunc = (D3D10_COMPARISON_FUNC)badfunc[i];
        hr = ID3D10Device_CreateDepthStencilState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "DepthFunc", "value %u: %s", badfunc[i], d3dh_hr_name(hr));
        d = ds_default(); d.StencilEnable = TRUE; d.FrontFace.StencilFunc = (D3D10_COMPARISON_FUNC)badfunc[i];
        hr = ID3D10Device_CreateDepthStencilState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "FrontFace.StencilFunc", "value %u: %s", badfunc[i], d3dh_hr_name(hr));
        d = ds_default(); d.StencilEnable = TRUE; d.BackFace.StencilFunc = (D3D10_COMPARISON_FUNC)badfunc[i];
        hr = ID3D10Device_CreateDepthStencilState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "BackFace.StencilFunc", "value %u: %s", badfunc[i], d3dh_hr_name(hr));
    }
    static const UINT badop[] = { 0, 9, 100, 0xffffffffu };
    for (unsigned i = 0; i < 4; i++) {
        D3D10_DEPTH_STENCIL_DESC d = ds_default(); ID3D10DepthStencilState *s = (void *)1; HRESULT hr;
        d.StencilEnable = TRUE; d.FrontFace.StencilFailOp = (D3D10_STENCIL_OP)badop[i];
        hr = ID3D10Device_CreateDepthStencilState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "StencilFailOp", "value %u: %s", badop[i], d3dh_hr_name(hr));
        d = ds_default(); d.StencilEnable = TRUE; d.BackFace.StencilPassOp = (D3D10_STENCIL_OP)badop[i];
        hr = ID3D10Device_CreateDepthStencilState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "StencilPassOp", "value %u: %s", badop[i], d3dh_hr_name(hr));
        d = ds_default(); d.StencilEnable = TRUE; d.FrontFace.StencilDepthFailOp = (D3D10_STENCIL_OP)badop[i];
        hr = ID3D10Device_CreateDepthStencilState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "StencilDepthFailOp", "value %u: %s", badop[i], d3dh_hr_name(hr));
    }
    D3D10_DEPTH_STENCIL_DESC d = ds_default(); ID3D10DepthStencilState *s = (void *)1;
    d.DepthWriteMask = (D3D10_DEPTH_WRITE_MASK)2;
    EXPECT_HR(ID3D10Device_CreateDepthStencilState(dev, &d, &s), E_INVALIDARG);
    DD_END(dev);
}
T(ds_all_comparison_funcs_valid)
{
    DD_DEV(dev);
    for (UINT f = 1; f <= 8; f++) {
        D3D10_DEPTH_STENCIL_DESC d = ds_default(); ID3D10DepthStencilState *s = NULL;
        d.DepthFunc = (D3D10_COMPARISON_FUNC)f; d.StencilEnable = TRUE; d.FrontFace.StencilFunc = (D3D10_COMPARISON_FUNC)f; d.BackFace.StencilFunc = (D3D10_COMPARISON_FUNC)f;
        HRESULT hr = ID3D10Device_CreateDepthStencilState(dev, &d, &s);
        t_check(hr == S_OK, __FILE__, __LINE__, "func", "value %u: %s", f, d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    for (UINT o = 1; o <= 8; o++) {
        D3D10_DEPTH_STENCIL_DESC d = ds_default(); ID3D10DepthStencilState *s = NULL;
        d.StencilEnable = TRUE; d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = d.FrontFace.StencilPassOp = (D3D10_STENCIL_OP)o; d.BackFace = d.FrontFace;
        HRESULT hr = ID3D10Device_CreateDepthStencilState(dev, &d, &s);
        t_check(hr == S_OK, __FILE__, __LINE__, "op", "value %u: %s", o, d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    DD_END(dev);
}

/* ---- rasterizer ---- */
T(rs_null_desc)
{
    DD_DEV(dev);
    ID3D10RasterizerState *s = (void *)1;
    EXPECT_HR(ID3D10Device_CreateRasterizerState(dev, NULL, &s), E_INVALIDARG);
    DD_END(dev);
}
T(rs_null_out_is_s_false)
{
    DD_DEV(dev);
    D3D10_RASTERIZER_DESC d = rs_default();
    EXPECT_HR(ID3D10Device_CreateRasterizerState(dev, &d, NULL), S_FALSE);
    DD_END(dev);
}
T(rs_dedup)
{
    DD_DEV(dev);
    D3D10_RASTERIZER_DESC d = rs_default(); ID3D10RasterizerState *a = NULL, *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateRasterizerState(dev, &d, &a));
    EXPECT_HR_OK(ID3D10Device_CreateRasterizerState(dev, &d, &b));
    CHECK(a == b);
    if (a) EXPECT_EQ(d3dh_refcount(a), 2);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DD_END(dev);
}
T(rs_roundtrip)
{
    DD_DEV(dev);
    D3D10_RASTERIZER_DESC d = rs_default(), g; ID3D10RasterizerState *s = NULL;
    d.FillMode = D3D10_FILL_WIREFRAME; d.CullMode = D3D10_CULL_FRONT; d.FrontCounterClockwise = TRUE; d.DepthBias = -37; d.DepthBiasClamp = 0.5f;
    d.SlopeScaledDepthBias = 2.25f; d.DepthClipEnable = FALSE; d.ScissorEnable = TRUE; d.MultisampleEnable = TRUE; d.AntialiasedLineEnable = TRUE;
    EXPECT_HR_OK(ID3D10Device_CreateRasterizerState(dev, &d, &s));
    if (s) { memset(&g, 0xcc, sizeof g); ID3D10RasterizerState_GetDesc(s, &g); CHECK(!memcmp(&g, &d, sizeof d)); ID3D10RasterizerState_Release(s); }
    DD_END(dev);
}
T(rs_all_fill_cull_combinations)
{
    DD_DEV(dev);
    for (UINT f = 2; f <= 3; f++) for (UINT c = 1; c <= 3; c++) {
        D3D10_RASTERIZER_DESC d = rs_default(), g; ID3D10RasterizerState *s = NULL;
        d.FillMode = (D3D10_FILL_MODE)f; d.CullMode = (D3D10_CULL_MODE)c;
        HRESULT hr = ID3D10Device_CreateRasterizerState(dev, &d, &s);
        t_check(hr == S_OK, __FILE__, __LINE__, "fill/cull", "%u/%u: %s", f, c, d3dh_hr_name(hr));
        if (s) { ID3D10RasterizerState_GetDesc(s, &g); EXPECT_EQ(g.FillMode, f); EXPECT_EQ(g.CullMode, c); ID3D10RasterizerState_Release(s); }
    }
    DD_END(dev);
}
T(rs_invalid_enums)
{
    DD_DEV(dev);
    static const UINT fill[] = { 0, 1, 4, 100, 0xffffffffu }, cull[] = { 0, 4, 100, 0xffffffffu };
    for (unsigned i = 0; i < 5; i++) {
        D3D10_RASTERIZER_DESC d = rs_default(); ID3D10RasterizerState *s = (void *)1;
        d.FillMode = (D3D10_FILL_MODE)fill[i];
        HRESULT hr = ID3D10Device_CreateRasterizerState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "FillMode", "value %u: %s", fill[i], d3dh_hr_name(hr));
    }
    for (unsigned i = 0; i < 4; i++) {
        D3D10_RASTERIZER_DESC d = rs_default(); ID3D10RasterizerState *s = (void *)1;
        d.CullMode = (D3D10_CULL_MODE)cull[i];
        HRESULT hr = ID3D10Device_CreateRasterizerState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "CullMode", "value %u: %s", cull[i], d3dh_hr_name(hr));
    }
    DD_END(dev);
}
T(rs_depth_bias_extremes)
{
    DD_DEV(dev);
    static const int bias[] = { 0, 1, -1, 16777215, -16777216, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < 7; i++) {
        D3D10_RASTERIZER_DESC d = rs_default(), g; ID3D10RasterizerState *s = NULL;
        d.DepthBias = bias[i];
        HRESULT hr = ID3D10Device_CreateRasterizerState(dev, &d, &s);
        t_note("DepthBias %d: %s", bias[i], d3dh_hr_name(hr));
        if (s) { ID3D10RasterizerState_GetDesc(s, &g); EXPECT_EQ(g.DepthBias, bias[i]); ID3D10RasterizerState_Release(s); }
    }
    DD_END(dev);
}
T(rs_different_desc_different_object)
{
    DD_DEV(dev);
    D3D10_RASTERIZER_DESC a = rs_default(), b = rs_default(); ID3D10RasterizerState *sa = NULL, *sb = NULL;
    b.ScissorEnable = TRUE;
    EXPECT_HR_OK(ID3D10Device_CreateRasterizerState(dev, &a, &sa));
    EXPECT_HR_OK(ID3D10Device_CreateRasterizerState(dev, &b, &sb));
    if (sa && sb) CHECK(sa != sb);
    SAFE_RELEASE(sa); SAFE_RELEASE(sb);
    DD_END(dev);
}

/* ---- sampler ---- */
T(smp_null_desc)
{
    DD_DEV(dev);
    ID3D10SamplerState *s = (void *)1;
    EXPECT_HR(ID3D10Device_CreateSamplerState(dev, NULL, &s), E_INVALIDARG);
    DD_END(dev);
}
T(smp_null_out_is_s_false)
{
    DD_DEV(dev);
    D3D10_SAMPLER_DESC d = smp_default();
    EXPECT_HR(ID3D10Device_CreateSamplerState(dev, &d, NULL), S_FALSE);
    DD_END(dev);
}
T(smp_dedup)
{
    DD_DEV(dev);
    D3D10_SAMPLER_DESC d = smp_default(); ID3D10SamplerState *a = NULL, *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateSamplerState(dev, &d, &a));
    EXPECT_HR_OK(ID3D10Device_CreateSamplerState(dev, &d, &b));
    CHECK(a == b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DD_END(dev);
}
T(smp_roundtrip)
{
    DD_DEV(dev);
    D3D10_SAMPLER_DESC d = smp_default(), g; ID3D10SamplerState *s = NULL;
    d.Filter = D3D10_FILTER_ANISOTROPIC; d.AddressU = D3D10_TEXTURE_ADDRESS_WRAP; d.AddressV = D3D10_TEXTURE_ADDRESS_MIRROR; d.AddressW = D3D10_TEXTURE_ADDRESS_BORDER;
    d.MipLODBias = 1.5f; d.MaxAnisotropy = 8; d.ComparisonFunc = D3D10_COMPARISON_LESS_EQUAL;
    d.BorderColor[0] = 0.25f; d.BorderColor[1] = 0.5f; d.BorderColor[2] = 0.75f; d.BorderColor[3] = 1.0f; d.MinLOD = 1.0f; d.MaxLOD = 6.0f;
    EXPECT_HR_OK(ID3D10Device_CreateSamplerState(dev, &d, &s));
    if (s) {
        memset(&g, 0xcc, sizeof g); ID3D10SamplerState_GetDesc(s, &g);
        EXPECT_EQ(g.Filter, d.Filter); EXPECT_EQ(g.AddressU, d.AddressU); EXPECT_EQ(g.AddressV, d.AddressV); EXPECT_EQ(g.AddressW, d.AddressW);
        CHECK(g.MipLODBias == 1.5f); EXPECT_EQ(g.MaxAnisotropy, 8); EXPECT_EQ(g.ComparisonFunc, d.ComparisonFunc);
        CHECK(g.BorderColor[0] == 0.25f && g.BorderColor[1] == 0.5f && g.BorderColor[2] == 0.75f && g.BorderColor[3] == 1.0f);
        CHECK(g.MinLOD == 1.0f && g.MaxLOD == 6.0f);
        ID3D10SamplerState_Release(s);
    }
    DD_END(dev);
}
T(smp_all_address_modes)
{
    DD_DEV(dev);
    for (UINT m = 1; m <= 5; m++) {
        D3D10_SAMPLER_DESC d = smp_default(); ID3D10SamplerState *s = NULL;
        d.AddressU = d.AddressV = d.AddressW = (D3D10_TEXTURE_ADDRESS_MODE)m;
        HRESULT hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_check(hr == S_OK, __FILE__, __LINE__, "address mode", "%u: %s", m, d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    static const UINT bad[] = { 0, 6, 100, 0xffffffffu };
    for (unsigned i = 0; i < 4; i++) {
        D3D10_SAMPLER_DESC d = smp_default(); ID3D10SamplerState *s = (void *)1; HRESULT hr;
        d.AddressU = (D3D10_TEXTURE_ADDRESS_MODE)bad[i];
        hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "AddressU", "%u: %s", bad[i], d3dh_hr_name(hr));
        d = smp_default(); d.AddressV = (D3D10_TEXTURE_ADDRESS_MODE)bad[i];
        hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "AddressV", "%u: %s", bad[i], d3dh_hr_name(hr));
        d = smp_default(); d.AddressW = (D3D10_TEXTURE_ADDRESS_MODE)bad[i];
        hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "AddressW", "%u: %s", bad[i], d3dh_hr_name(hr));
    }
    DD_END(dev);
}
T(smp_all_valid_filters)
{
    static const UINT f[] = { 0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15, 0x55, 0x80, 0x81, 0x84, 0x85, 0x90, 0x91, 0x94, 0x95, 0xd5 };
    DD_DEV(dev);
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        D3D10_SAMPLER_DESC d = smp_default(), g; ID3D10SamplerState *s = NULL;
        d.Filter = (D3D10_FILTER)f[i];
        if (f[i] & 0x80) d.ComparisonFunc = D3D10_COMPARISON_LESS;
        if (f[i] == 0x55) d.MaxAnisotropy = 4;
        HRESULT hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_check(hr == S_OK, __FILE__, __LINE__, "filter", "0x%x: %s", f[i], d3dh_hr_name(hr));
        if (s) { ID3D10SamplerState_GetDesc(s, &g); EXPECT_EQ(g.Filter, f[i]); ID3D10SamplerState_Release(s); }
    }
    DD_END(dev);
}
T(smp_invalid_filters)
{
    static const UINT f[] = { 0x02, 0x03, 0x06, 0x07, 0x08, 0xffffffffu };
    DD_DEV(dev);
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        D3D10_SAMPLER_DESC d = smp_default(); ID3D10SamplerState *s = (void *)1;
        d.Filter = (D3D10_FILTER)f[i];
        HRESULT hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "bad filter", "0x%x: %s", f[i], d3dh_hr_name(hr));
    }
    DD_END(dev);
}
T(smp_anisotropy_range)
{
    DD_DEV(dev);
    for (UINT a = 0; a <= 20; a++) {
        D3D10_SAMPLER_DESC d = smp_default(); ID3D10SamplerState *s = NULL;
        d.Filter = D3D10_FILTER_ANISOTROPIC; d.MaxAnisotropy = a;
        HRESULT want = (a >= 1 && a <= 16) ? S_OK : E_INVALIDARG;
        HRESULT hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_check(hr == want, __FILE__, __LINE__, "aniso", "MaxAnisotropy %u: %s", a, d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    DD_END(dev);
}
T(smp_comparison_func_range_with_comparison_filter)
{
    DD_DEV(dev);
    for (UINT c = 0; c <= 9; c++) {
        D3D10_SAMPLER_DESC d = smp_default(); ID3D10SamplerState *s = NULL;
        d.Filter = D3D10_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; d.ComparisonFunc = (D3D10_COMPARISON_FUNC)c;
        HRESULT want = (c >= 1 && c <= 8) ? S_OK : E_INVALIDARG;
        HRESULT hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_check(hr == want, __FILE__, __LINE__, "cmp func", "%u: %s", c, d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    DD_END(dev);
}
T(smp_lod_bounds_observed)
{
    DD_DEV(dev);
    static const float bias[] = { 0.f, 15.99f, -16.f, 16.f, -17.f, 100.f };
    for (unsigned i = 0; i < 6; i++) {
        D3D10_SAMPLER_DESC d = smp_default(); ID3D10SamplerState *s = NULL;
        d.MipLODBias = bias[i];
        HRESULT hr = ID3D10Device_CreateSamplerState(dev, &d, &s);
        t_note("MipLODBias %g: %s", bias[i], d3dh_hr_name(hr));
        SAFE_RELEASE(s);
    }
    D3D10_SAMPLER_DESC d = smp_default(); ID3D10SamplerState *s = NULL;
    d.MinLOD = 5; d.MaxLOD = 1;
    t_note("MinLOD > MaxLOD: %s", d3dh_hr_name(ID3D10Device_CreateSamplerState(dev, &d, &s)));
    SAFE_RELEASE(s);
    DD_END(dev);
}
T(smp_lod_zero_range_valid)
{
    DD_DEV(dev);
    D3D10_SAMPLER_DESC d = smp_default(), g; ID3D10SamplerState *s = NULL;
    d.MinLOD = 2.0f; d.MaxLOD = 2.0f;
    EXPECT_HR_OK(ID3D10Device_CreateSamplerState(dev, &d, &s));
    if (s) { ID3D10SamplerState_GetDesc(s, &g); CHECK(g.MinLOD == 2.0f && g.MaxLOD == 2.0f); ID3D10SamplerState_Release(s); }
    DD_END(dev);
}
T(smp_different_desc_different_object)
{
    DD_DEV(dev);
    D3D10_SAMPLER_DESC a = smp_default(), b = smp_default(); ID3D10SamplerState *sa = NULL, *sb = NULL;
    b.MipLODBias = 0.5f;
    EXPECT_HR_OK(ID3D10Device_CreateSamplerState(dev, &a, &sa));
    EXPECT_HR_OK(ID3D10Device_CreateSamplerState(dev, &b, &sb));
    if (sa && sb) CHECK(sa != sb);
    SAFE_RELEASE(sa); SAFE_RELEASE(sb);
    DD_END(dev);
}
T(smp_state_object_survives_device_release_order)
{
    ID3D10Device *dev = NULL; ID3D10SamplerState *s = NULL;
    REQUIRE(DD_C10());
    if (FAILED(dd_make(0, &dev))) { t_skip("no device"); return; }
    D3D10_SAMPLER_DESC d = smp_default();
    EXPECT_HR_OK(ID3D10Device_CreateSamplerState(dev, &d, &s));
    ID3D10Device_Release(dev);
    if (s) { ID3D10Device *got = NULL; ID3D10SamplerState_GetDevice(s, &got); CHECK(got != NULL); SAFE_RELEASE(got); EXPECT_EQ(ID3D10SamplerState_Release(s), 0); }
}

/* ---- pipeline state get/set defaults ---- */
T(pipeline_defaults_blend)
{
    DD_DEV(dev);
    ID3D10BlendState *s = (void *)1; FLOAT f[4] = { 9, 9, 9, 9 }; UINT mask = 0;
    ID3D10Device_OMGetBlendState(dev, &s, f, &mask);
    CHECK(s == NULL);
    CHECK(f[0] == 1.0f && f[1] == 1.0f && f[2] == 1.0f && f[3] == 1.0f);
    EXPECT_EQ(mask, 0xffffffffu);
    DD_END(dev);
}
T(pipeline_defaults_depth_stencil)
{
    DD_DEV(dev);
    ID3D10DepthStencilState *s = (void *)1; UINT ref = 77;
    ID3D10Device_OMGetDepthStencilState(dev, &s, &ref);
    CHECK(s == NULL); EXPECT_EQ(ref, 0);
    DD_END(dev);
}
T(pipeline_defaults_rasterizer)
{
    DD_DEV(dev);
    ID3D10RasterizerState *s = (void *)1;
    ID3D10Device_RSGetState(dev, &s);
    CHECK(s == NULL);
    UINT nv = 99, nr = 99;
    ID3D10Device_RSGetViewports(dev, &nv, NULL);
    ID3D10Device_RSGetScissorRects(dev, &nr, NULL);
    EXPECT_EQ(nv, 0); EXPECT_EQ(nr, 0);
    DD_END(dev);
}
T(pipeline_defaults_ia)
{
    DD_DEV(dev);
    D3D10_PRIMITIVE_TOPOLOGY topo = 0xff; ID3D10InputLayout *il = (void *)1; ID3D10Buffer *ib = (void *)1; DXGI_FORMAT f = 99; UINT off = 99;
    ID3D10Device_IAGetPrimitiveTopology(dev, &topo); EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED);
    ID3D10Device_IAGetInputLayout(dev, &il); CHECK(il == NULL);
    ID3D10Device_IAGetIndexBuffer(dev, &ib, &f, &off); CHECK(ib == NULL); EXPECT_EQ(f, DXGI_FORMAT_UNKNOWN); EXPECT_EQ(off, 0);
    ID3D10Buffer *vb[16]; UINT st[16], of[16];
    for (int i = 0; i < 16; i++) { vb[i] = (void *)1; st[i] = 77; of[i] = 77; }
    ID3D10Device_IAGetVertexBuffers(dev, 0, 16, vb, st, of);
    for (int i = 0; i < 16; i++) { CHECK(vb[i] == NULL); EXPECT_EQ(st[i], 0); EXPECT_EQ(of[i], 0); }
    DD_END(dev);
}
T(pipeline_defaults_om_targets)
{
    DD_DEV(dev);
    ID3D10RenderTargetView *rt[8]; ID3D10DepthStencilView *ds = (void *)1;
    for (int i = 0; i < 8; i++) rt[i] = (void *)1;
    ID3D10Device_OMGetRenderTargets(dev, 8, rt, &ds);
    for (int i = 0; i < 8; i++) CHECK(rt[i] == NULL);
    CHECK(ds == NULL);
    DD_END(dev);
}
T(pipeline_defaults_so_targets)
{
    DD_DEV(dev);
    ID3D10Buffer *b[4]; UINT o[4];
    for (int i = 0; i < 4; i++) { b[i] = (void *)1; o[i] = 5; }
    ID3D10Device_SOGetTargets(dev, 4, b, o);
    for (int i = 0; i < 4; i++) CHECK(b[i] == NULL);
    DD_END(dev);
}
T(pipeline_blend_set_get_roundtrip)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC d = blend_default(); ID3D10BlendState *s = NULL, *g = NULL;
    d.AlphaToCoverageEnable = TRUE;
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &d, &s));
    if (s) {
        const FLOAT bf[4] = { 0.25f, 0.5f, 0.75f, 1.0f }; FLOAT f[4]; UINT mask = 0; ULONG rc = d3dh_refcount(s);
        ID3D10Device_OMSetBlendState(dev, s, bf, 0x0f0f0f0fu);
        CHECK(d3dh_refcount(s) > rc);
        ID3D10Device_OMGetBlendState(dev, &g, f, &mask);
        CHECK(g == s);
        CHECK(!memcmp(f, bf, sizeof f)); EXPECT_EQ(mask, 0x0f0f0f0fu);
        SAFE_RELEASE(g);
        ID3D10Device_OMSetBlendState(dev, NULL, NULL, 0xffffffffu);
        EXPECT_EQ(d3dh_refcount(s), rc);
        ID3D10Device_OMGetBlendState(dev, &g, f, &mask);
        CHECK(g == NULL); CHECK(f[0] == 1.0f && f[3] == 1.0f); EXPECT_EQ(mask, 0xffffffffu);
        ID3D10BlendState_Release(s);
    }
    DD_END(dev);
}
T(pipeline_blend_factor_null_resets_to_one)
{
    DD_DEV(dev);
    const FLOAT bf[4] = { 0.1f, 0.2f, 0.3f, 0.4f }; FLOAT f[4]; UINT mask; ID3D10BlendState *g = (void *)1;
    ID3D10Device_OMSetBlendState(dev, NULL, bf, 1);
    ID3D10Device_OMGetBlendState(dev, &g, f, &mask);
    CHECK(!memcmp(f, bf, sizeof f)); EXPECT_EQ(mask, 1);
    ID3D10Device_OMSetBlendState(dev, NULL, NULL, 2);
    ID3D10Device_OMGetBlendState(dev, &g, f, &mask);
    CHECK(f[0] == 1.0f && f[1] == 1.0f && f[2] == 1.0f && f[3] == 1.0f); EXPECT_EQ(mask, 2);
    ID3D10Device_OMSetBlendState(dev, NULL, NULL, 0xffffffffu);
    DD_END(dev);
}
T(pipeline_ds_set_get_roundtrip)
{
    DD_DEV(dev);
    D3D10_DEPTH_STENCIL_DESC d = ds_default(); ID3D10DepthStencilState *s = NULL, *g = NULL; UINT ref = 0;
    d.DepthFunc = D3D10_COMPARISON_GREATER;
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &d, &s));
    if (s) {
        ID3D10Device_OMSetDepthStencilState(dev, s, 0x42);
        ID3D10Device_OMGetDepthStencilState(dev, &g, &ref);
        CHECK(g == s); EXPECT_EQ(ref, 0x42); SAFE_RELEASE(g);
        ID3D10Device_OMSetDepthStencilState(dev, NULL, 0);
        ID3D10DepthStencilState_Release(s);
    }
    DD_END(dev);
}
T(pipeline_rs_state_and_viewports)
{
    DD_DEV(dev);
    D3D10_RASTERIZER_DESC d = rs_default(); ID3D10RasterizerState *s = NULL, *g = NULL;
    d.ScissorEnable = TRUE;
    EXPECT_HR_OK(ID3D10Device_CreateRasterizerState(dev, &d, &s));
    if (s) { ID3D10Device_RSSetState(dev, s); ID3D10Device_RSGetState(dev, &g); CHECK(g == s); SAFE_RELEASE(g); ID3D10Device_RSSetState(dev, NULL); ID3D10RasterizerState_Release(s); }
    D3D10_VIEWPORT vp[3] = { { 1, 2, 30, 40, 0.1f, 0.9f }, { 3, 4, 50, 60, 0.0f, 1.0f }, { 0, 0, 8, 8, 0.25f, 0.5f } }, got[16]; UINT n = 3;
    ID3D10Device_RSSetViewports(dev, 3, vp);
    ID3D10Device_RSGetViewports(dev, &n, NULL);
    EXPECT_EQ(n, 3);
    n = 16; memset(got, 0, sizeof got);
    ID3D10Device_RSGetViewports(dev, &n, got);
    EXPECT_EQ(n, 3);
    CHECK(!memcmp(got, vp, sizeof vp));
    n = 2;
    ID3D10Device_RSGetViewports(dev, &n, got);
    EXPECT_EQ(n, 2);
    ID3D10Device_RSSetViewports(dev, 0, NULL);
    n = 5; ID3D10Device_RSGetViewports(dev, &n, NULL); EXPECT_EQ(n, 0);
    DD_END(dev);
}
T(pipeline_scissor_rects_roundtrip)
{
    DD_DEV(dev);
    D3D10_RECT r[2] = { { 1, 2, 3, 4 }, { 10, 20, 30, 40 } }, g[16]; UINT n = 0;
    ID3D10Device_RSSetScissorRects(dev, 2, r);
    ID3D10Device_RSGetScissorRects(dev, &n, NULL); EXPECT_EQ(n, 2);
    n = 16; memset(g, 0, sizeof g);
    ID3D10Device_RSGetScissorRects(dev, &n, g); EXPECT_EQ(n, 2);
    CHECK(!memcmp(g, r, sizeof r));
    ID3D10Device_RSSetScissorRects(dev, 0, NULL);
    DD_END(dev);
}
T(pipeline_viewport_set_replaces_count)
{
    DD_DEV(dev);
    D3D10_VIEWPORT vp = { 0, 0, 4, 4, 0, 1 }; UINT n = 0;
    ID3D10Device_RSSetViewports(dev, 1, &vp);
    ID3D10Device_RSGetViewports(dev, &n, NULL);
    EXPECT_EQ(n, 1);
    ID3D10Device_RSSetViewports(dev, 0, NULL);
    DD_END(dev);
}
T(pipeline_topology_roundtrip)
{
    DD_DEV(dev);
    static const D3D10_PRIMITIVE_TOPOLOGY t[] = { D3D10_PRIMITIVE_TOPOLOGY_POINTLIST, D3D10_PRIMITIVE_TOPOLOGY_LINELIST, D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP,
        D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, D3D10_PRIMITIVE_TOPOLOGY_LINELIST_ADJ, D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
        D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        D3D10_PRIMITIVE_TOPOLOGY g = 0;
        ID3D10Device_IASetPrimitiveTopology(dev, t[i]);
        ID3D10Device_IAGetPrimitiveTopology(dev, &g);
        t_check(g == t[i], __FILE__, __LINE__, "topology", "set %u got %u", t[i], g);
    }
    EXPECT_EQ(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST, 1); EXPECT_EQ(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 5);
    EXPECT_EQ(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ, 12);
    DD_END(dev);
}
T(pipeline_vertex_buffer_roundtrip)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC bd = { 256, D3D10_USAGE_DEFAULT, D3D10_BIND_VERTEX_BUFFER, 0, 0 }; ID3D10Buffer *b[2] = { NULL, NULL };
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &bd, NULL, &b[0]));
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &bd, NULL, &b[1]));
    if (b[0] && b[1]) {
        UINT st[2] = { 16, 32 }, of[2] = { 4, 8 }; ID3D10Buffer *g[2] = { NULL, NULL }; UINT gs[2] = { 0, 0 }, go[2] = { 0, 0 };
        ID3D10Device_IASetVertexBuffers(dev, 2, 2, b, st, of);
        ID3D10Device_IAGetVertexBuffers(dev, 2, 2, g, gs, go);
        CHECK(g[0] == b[0] && g[1] == b[1]); EXPECT_EQ(gs[0], 16); EXPECT_EQ(gs[1], 32); EXPECT_EQ(go[0], 4); EXPECT_EQ(go[1], 8);
        SAFE_RELEASE(g[0]); SAFE_RELEASE(g[1]);
        ID3D10Buffer *none[2] = { NULL, NULL }; UINT z[2] = { 0, 0 };
        ID3D10Device_IASetVertexBuffers(dev, 2, 2, none, z, z);
        ID3D10Device_IAGetVertexBuffers(dev, 2, 2, g, gs, go);
        CHECK(!g[0] && !g[1]);
    }
    SAFE_RELEASE(b[0]); SAFE_RELEASE(b[1]);
    DD_END(dev);
}
T(pipeline_index_buffer_roundtrip)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC bd = { 64, D3D10_USAGE_DEFAULT, D3D10_BIND_INDEX_BUFFER, 0, 0 }; ID3D10Buffer *b = NULL, *g = NULL; DXGI_FORMAT f = 0; UINT off = 0;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &bd, NULL, &b));
    if (b) {
        ID3D10Device_IASetIndexBuffer(dev, b, DXGI_FORMAT_R32_UINT, 12);
        ID3D10Device_IAGetIndexBuffer(dev, &g, &f, &off);
        CHECK(g == b); EXPECT_EQ(f, DXGI_FORMAT_R32_UINT); EXPECT_EQ(off, 12);
        SAFE_RELEASE(g);
        ID3D10Device_IASetIndexBuffer(dev, NULL, DXGI_FORMAT_UNKNOWN, 0);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(pipeline_constant_buffer_roundtrip)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC bd = { 64, D3D10_USAGE_DEFAULT, D3D10_BIND_CONSTANT_BUFFER, 0, 0 }; ID3D10Buffer *b = NULL, *g[3] = { NULL, NULL, NULL };
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &bd, NULL, &b));
    if (b) {
        ID3D10Device_VSSetConstantBuffers(dev, 1, 1, &b);
        ID3D10Device_VSGetConstantBuffers(dev, 0, 3, g);
        CHECK(!g[0] && g[1] == b && !g[2]);
        SAFE_RELEASE(g[1]);
        ID3D10Device_PSSetConstantBuffers(dev, 13, 1, &b);
        ID3D10Device_PSGetConstantBuffers(dev, 13, 1, g);
        CHECK(g[0] == b); SAFE_RELEASE(g[0]);
        ID3D10Device_GSSetConstantBuffers(dev, 0, 1, &b);
        ID3D10Device_GSGetConstantBuffers(dev, 0, 1, g);
        CHECK(g[0] == b); SAFE_RELEASE(g[0]);
        ID3D10Device_ClearState(dev);
        ID3D10Device_VSGetConstantBuffers(dev, 1, 1, g); CHECK(!g[0]);
        ID3D10Device_PSGetConstantBuffers(dev, 13, 1, g); CHECK(!g[0]);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(pipeline_sampler_and_srv_roundtrip)
{
    DD_DEV(dev);
    D3D10_SAMPLER_DESC sd = smp_default(); ID3D10SamplerState *s = NULL, *gs = NULL;
    D3D10_TEXTURE2D_DESC td = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 };
    ID3D10Texture2D *t = NULL; ID3D10ShaderResourceView *v = NULL, *gv = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateSamplerState(dev, &sd, &s));
    EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &td, NULL, &t));
    if (t) EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, (ID3D10Resource *)t, NULL, &v));
    if (s) {
        ID3D10Device_PSSetSamplers(dev, 15, 1, &s); ID3D10Device_PSGetSamplers(dev, 15, 1, &gs); CHECK(gs == s); SAFE_RELEASE(gs);
        ID3D10Device_VSSetSamplers(dev, 0, 1, &s); ID3D10Device_VSGetSamplers(dev, 0, 1, &gs); CHECK(gs == s); SAFE_RELEASE(gs);
        ID3D10Device_GSSetSamplers(dev, 3, 1, &s); ID3D10Device_GSGetSamplers(dev, 3, 1, &gs); CHECK(gs == s); SAFE_RELEASE(gs);
    }
    if (v) {
        ID3D10Device_PSSetShaderResources(dev, 127, 1, &v); ID3D10Device_PSGetShaderResources(dev, 127, 1, &gv); CHECK(gv == v); SAFE_RELEASE(gv);
        ID3D10Device_VSSetShaderResources(dev, 0, 1, &v); ID3D10Device_VSGetShaderResources(dev, 0, 1, &gv); CHECK(gv == v); SAFE_RELEASE(gv);
        ID3D10Device_GSSetShaderResources(dev, 5, 1, &v); ID3D10Device_GSGetShaderResources(dev, 5, 1, &gv); CHECK(gv == v); SAFE_RELEASE(gv);
        ID3D10Device_ClearState(dev);
        ID3D10Device_PSGetShaderResources(dev, 127, 1, &gv); CHECK(!gv);
    }
    SAFE_RELEASE(s); SAFE_RELEASE(v); SAFE_RELEASE(t);
    DD_END(dev);
}
T(pipeline_clear_state_resets_all)
{
    DD_DEV(dev);
    D3D10_BLEND_DESC bd = blend_default(); D3D10_DEPTH_STENCIL_DESC dd = ds_default(); D3D10_RASTERIZER_DESC rd = rs_default();
    ID3D10BlendState *b = NULL; ID3D10DepthStencilState *ds = NULL; ID3D10RasterizerState *rs = NULL;
    bd.AlphaToCoverageEnable = TRUE; dd.DepthFunc = D3D10_COMPARISON_ALWAYS; rd.ScissorEnable = TRUE;
    EXPECT_HR_OK(ID3D10Device_CreateBlendState(dev, &bd, &b));
    EXPECT_HR_OK(ID3D10Device_CreateDepthStencilState(dev, &dd, &ds));
    EXPECT_HR_OK(ID3D10Device_CreateRasterizerState(dev, &rd, &rs));
    const FLOAT bf[4] = { 0, 0, 0, 0 };
    ID3D10Device_OMSetBlendState(dev, b, bf, 3);
    ID3D10Device_OMSetDepthStencilState(dev, ds, 9);
    ID3D10Device_RSSetState(dev, rs);
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D10_VIEWPORT vp = { 0, 0, 4, 4, 0, 1 }; ID3D10Device_RSSetViewports(dev, 1, &vp);
    ID3D10Device_ClearState(dev);
    ID3D10BlendState *gb = (void *)1; ID3D10DepthStencilState *gd = (void *)1; ID3D10RasterizerState *gr = (void *)1; FLOAT f[4]; UINT mask = 0, ref = 5, n = 5;
    D3D10_PRIMITIVE_TOPOLOGY topo = 0xff;
    ID3D10Device_OMGetBlendState(dev, &gb, f, &mask); CHECK(!gb); EXPECT_EQ(mask, 0xffffffffu); CHECK(f[0] == 1.0f && f[3] == 1.0f);
    ID3D10Device_OMGetDepthStencilState(dev, &gd, &ref); CHECK(!gd); EXPECT_EQ(ref, 0);
    ID3D10Device_RSGetState(dev, &gr); CHECK(!gr);
    ID3D10Device_IAGetPrimitiveTopology(dev, &topo); EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED);
    ID3D10Device_RSGetViewports(dev, &n, NULL); EXPECT_EQ(n, 0);
    EXPECT_EQ(d3dh_refcount(b), 1); EXPECT_EQ(d3dh_refcount(ds), 1); EXPECT_EQ(d3dh_refcount(rs), 1);
    SAFE_RELEASE(b); SAFE_RELEASE(ds); SAFE_RELEASE(rs);
    DD_END(dev);
}
T(pipeline_getters_addref_outputs)
{
    DD_DEV(dev);
    D3D10_RASTERIZER_DESC rd = rs_default(); ID3D10RasterizerState *rs = NULL, *g = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateRasterizerState(dev, &rd, &rs));
    if (rs) {
        ID3D10Device_RSSetState(dev, rs);
        ULONG rc = d3dh_refcount(rs);
        ID3D10Device_RSGetState(dev, &g);
        EXPECT_EQ(d3dh_refcount(rs), rc + 1);
        SAFE_RELEASE(g);
        ID3D10Device_RSSetState(dev, NULL);
        ID3D10RasterizerState_Release(rs);
    }
    DD_END(dev);
}
T(pipeline_flush_and_clearstate_are_callable)
{
    DD_DEV(dev);
    ID3D10Device_Flush(dev);
    ID3D10Device_ClearState(dev);
    ID3D10Device_Flush(dev);
    DD_END(dev);
}
