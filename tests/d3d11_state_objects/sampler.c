/* Sampler state creation. */
#include "so_common.h"

#define SCAN_CHECK(hr, valid, v) t_check((hr) == ((valid) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, #hr, \
    "value %d: got %s want %s", (int)(v), d3dh_hr_name(hr), (valid) ? "S_FALSE" : "E_INVALIDARG")

/* 'base' filter encodings: min bit 4, mag bit 2, mip bit 0, anisotropic 0x55; comparison adds 0x80, minimum 0x100, maximum 0x180 */
static int base_filter_ok(int f)
{
    int b = f & 0x7F;
    return b == 0 || b == 1 || b == 4 || b == 5 || b == 0x10 || b == 0x11 || b == 0x14 || b == 0x15 || b == 0x55;
}
static int filter_is_comparison(int f) { return f >= 0x80 && f < 0x100; }
static int filter_is_aniso(int f) { return (f & 0x7F) == 0x55; }

static int samp_eq(const D3D11_SAMPLER_DESC *a, const D3D11_SAMPLER_DESC *b)
{
    return a->Filter == b->Filter && a->AddressU == b->AddressU && a->AddressV == b->AddressV && a->AddressW == b->AddressW &&
           a->MipLODBias == b->MipLODBias && a->MaxAnisotropy == b->MaxAnisotropy && a->ComparisonFunc == b->ComparisonFunc &&
           !memcmp(a->BorderColor, b->BorderColor, sizeof a->BorderColor) && a->MinLOD == b->MinLOD && a->MaxLOD == b->MaxLOD;
}

ST(samp_default_accepted)
{
    D3D11_SAMPLER_DESC d = so_samp_default();
    SO_NEED();
    SO_ACCEPT_SAMP(&d);
}
ST(samp_null_desc)
{
    ID3D11SamplerState *s = NULL;
    SO_NEED();
    EXPECT_HR(SO_CSAMP(NULL, NULL), E_INVALIDARG);
    EXPECT_HR(SO_CSAMP(NULL, &s), E_INVALIDARG);
    CHECK(s == NULL);
}
ST(samp_zeroed_desc_rejected)
{
    D3D11_SAMPLER_DESC d;
    SO_NEED();
    memset(&d, 0, sizeof d);
    /* address modes of 0 are not valid enumerators */
    SO_REJECT_SAMP(&d);
}
ST(samp_scan_filter_low_range)
{
    int f;
    SO_NEED();
    for (f = -1; f < 0x100; f++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.Filter = (D3D11_FILTER)f;
        d.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        d.MaxAnisotropy = 4;
        hr = SO_CSAMP(&d, NULL);
        t_check(hr == (f >= 0 && base_filter_ok(f) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "filter", "filter 0x%x got %s", f, d3dh_hr_name(hr));
    }
}
ST(samp_scan_filter_minmax_range)
{
    int f, nvalid = 0;
    SO_NEED();
    for (f = 0x100; f < 0x200; f++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.Filter = (D3D11_FILTER)f;
        d.MaxAnisotropy = 4;
        hr = SO_CSAMP(&d, NULL);
        if (!base_filter_ok(f)) {
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "minmax filter", "invalid filter 0x%x got %s", f, d3dh_hr_name(hr));
        } else {
            t_check(hr == S_FALSE || hr == E_INVALIDARG, __FILE__, __LINE__, "minmax filter", "filter 0x%x got %s", f, d3dh_hr_name(hr));
            if (hr == S_FALSE) nvalid++;
        }
    }
    t_note("min/max filters accepted: %d of 18 (capability dependent, all-or-nothing expected)", nvalid);
    CHECK(nvalid == 0 || nvalid == 18);
}
ST(samp_filter_named_constants_accepted)
{
    static const int f[] = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
        D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR, D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
        D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC,
        D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC };
    unsigned i;
    SO_NEED();
    for (i = 0; i < sizeof f / sizeof f[0]; i++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        d.Filter = (D3D11_FILTER)f[i];
        d.ComparisonFunc = D3D11_COMPARISON_LESS;
        d.MaxAnisotropy = 8;
        SO_ACCEPT_SAMP(&d);
    }
}
ST(samp_scan_address_u)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 8; v++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.AddressU = (D3D11_TEXTURE_ADDRESS_MODE)v;
        hr = SO_CSAMP(&d, NULL);
        SCAN_CHECK(hr, v >= 1 && v <= 5, v);
    }
}
ST(samp_scan_address_v)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 8; v++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.AddressV = (D3D11_TEXTURE_ADDRESS_MODE)v;
        hr = SO_CSAMP(&d, NULL);
        SCAN_CHECK(hr, v >= 1 && v <= 5, v);
    }
}
ST(samp_scan_address_w)
{
    int v;
    SO_NEED();
    for (v = -2; v <= 8; v++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.AddressW = (D3D11_TEXTURE_ADDRESS_MODE)v;
        hr = SO_CSAMP(&d, NULL);
        SCAN_CHECK(hr, v >= 1 && v <= 5, v);
    }
}
ST(samp_mirror_once_accepted)
{
    D3D11_SAMPLER_DESC d = so_samp_default();
    SO_NEED();
    d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
    SO_ACCEPT_SAMP(&d);
}
ST(samp_miplodbias_range)
{
    static const struct { float v; int ok; } t[] = {
        { 0.f, 1 }, { 1.f, 1 }, { -1.f, 1 }, { -16.f, 1 }, { 15.99f, 1 }, { 15.f, 1 }, { 8.5f, 1 }, { -15.99f, 1 },
        { -16.001f, 0 }, { -17.f, 0 }, { 15.9901f, 0 }, { 16.f, 0 }, { 100.f, 0 }, { -100.f, 0 }, { INFINITY, 0 }, { -INFINITY, 0 }, { NAN, 0 },
    };
    unsigned i;
    SO_NEED();
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.MipLODBias = t[i].v;
        hr = SO_CSAMP(&d, NULL);
        t_check(hr == (t[i].ok ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "bias", "bias %g got %s", t[i].v, d3dh_hr_name(hr));
    }
}
ST(samp_scan_maxanisotropy_anisotropic_filter)
{
    int v;
    SO_NEED();
    for (v = 1; v <= 40; v++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.Filter = D3D11_FILTER_ANISOTROPIC;
        d.MaxAnisotropy = v;
        hr = SO_CSAMP(&d, NULL);
        SCAN_CHECK(hr, v <= 16, v);
    }
}
ST(samp_scan_maxanisotropy_non_anisotropic_filter)
{
    /* the upper bound is applied to every filter type */
    int v;
    SO_NEED();
    for (v = 1; v <= 40; v++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        d.MaxAnisotropy = v;
        hr = SO_CSAMP(&d, NULL);
        SCAN_CHECK(hr, v <= 16, v);
    }
}
ST(samp_maxanisotropy_huge_rejected)
{
    D3D11_SAMPLER_DESC d = so_samp_default();
    SO_NEED();
    d.Filter = D3D11_FILTER_ANISOTROPIC;
    d.MaxAnisotropy = 0xFFFFFFFFu;
    SO_REJECT_SAMP(&d);
    d.MaxAnisotropy = 0x80000000u;
    SO_REJECT_SAMP(&d);
}
ST(samp_maxanisotropy_zero_observation)
{
    D3D11_SAMPLER_DESC d = so_samp_default();
    ID3D11SamplerState *s = NULL;
    HRESULT hr;
    SO_NEED();
    d.Filter = D3D11_FILTER_ANISOTROPIC;
    d.MaxAnisotropy = 0;
    hr = SO_CSAMP(&d, NULL);
    t_note("anisotropic filter with MaxAnisotropy 0 -> %s", d3dh_hr_name(hr));
    hr = SO_CSAMP(&d, &s);
    if (SUCCEEDED(hr) && s) {
        D3D11_SAMPLER_DESC g;
        ID3D11SamplerState_GetDesc(s, &g);
        t_note("... GetDesc reports MaxAnisotropy %u", g.MaxAnisotropy);
    }
    SAFE_RELEASE(s);
}
ST(samp_scan_comparisonfunc_with_comparison_filter)
{
    static const int filters[] = { D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D11_FILTER_COMPARISON_ANISOTROPIC };
    unsigned i;
    int v;
    SO_NEED();
    for (i = 0; i < 3; i++)
        for (v = -2; v <= 11; v++) {
            D3D11_SAMPLER_DESC d = so_samp_default();
            HRESULT hr;
            d.Filter = (D3D11_FILTER)filters[i];
            d.ComparisonFunc = (D3D11_COMPARISON_FUNC)v;
            hr = SO_CSAMP(&d, NULL);
            t_check(hr == ((v >= 1 && v <= 8) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "cmp func", "filter 0x%x func %d got %s", filters[i], v, d3dh_hr_name(hr));
        }
}
ST(samp_comparisonfunc_ignored_for_non_comparison_filter)
{
    static const int filters[] = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_FILTER_ANISOTROPIC };
    unsigned i;
    int v;
    SO_NEED();
    for (i = 0; i < 3; i++)
        for (v = -2; v <= 11; v++) {
            D3D11_SAMPLER_DESC d = so_samp_default();
            HRESULT hr;
            d.Filter = (D3D11_FILTER)filters[i];
            d.ComparisonFunc = (D3D11_COMPARISON_FUNC)v;
            hr = SO_CSAMP(&d, NULL);
            t_check(hr == S_FALSE, __FILE__, __LINE__, "cmp func ignored", "filter 0x%x func %d got %s", filters[i], v, d3dh_hr_name(hr));
        }
}
ST(samp_lod_ordering_not_validated)
{
    D3D11_SAMPLER_DESC d = so_samp_default();
    SO_NEED();
    d.MinLOD = 5.f;
    d.MaxLOD = 2.f;
    SO_ACCEPT_SAMP(&d);
}
ST(samp_lod_nan_rejected)
{
    D3D11_SAMPLER_DESC d = so_samp_default();
    SO_NEED();
    d.MinLOD = NAN;
    SO_REJECT_SAMP(&d);
    d = so_samp_default();
    d.MaxLOD = NAN;
    SO_REJECT_SAMP(&d);
}
ST(samp_lod_extremes_accepted)
{
    static const float vals[] = { 0.f, -1.f, 1.f, 3.5f, 10.f, 1000.f, -1000.f, D3D11_FLOAT32_MAX, -D3D11_FLOAT32_MAX };
    unsigned i, j;
    SO_NEED();
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++)
        for (j = 0; j < sizeof vals / sizeof vals[0]; j++) {
            D3D11_SAMPLER_DESC d = so_samp_default();
            d.MinLOD = vals[i];
            d.MaxLOD = vals[j];
            SO_ACCEPT_SAMP(&d);
        }
}
ST(samp_border_color_values_accepted)
{
    static const float c[][4] = { { 0, 0, 0, 0 }, { 1, 1, 1, 1 }, { 0.25f, 0.5f, 0.75f, 1.f }, { -1, 2, 100, -100 } };
    unsigned i;
    SO_NEED();
    for (i = 0; i < 4; i++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        memcpy(d.BorderColor, c[i], sizeof d.BorderColor);
        SO_ACCEPT_SAMP(&d);
    }
}

/* ---- GetDesc -------------------------------------------------------------------- */
ST(samp_getdesc_default_roundtrip)
{
    D3D11_SAMPLER_DESC d = so_samp_default(), g;
    ID3D11SamplerState *s = NULL;
    SO_NEED();
    EXPECT_HR(SO_CSAMP(&d, &s), S_OK);
    if (!s) return;
    memset(&g, 0x99, sizeof g);
    ID3D11SamplerState_GetDesc(s, &g);
    EXPECT_EQ(g.Filter, D3D11_FILTER_MIN_MAG_MIP_LINEAR);
    EXPECT_EQ(g.AddressU, D3D11_TEXTURE_ADDRESS_WRAP);
    EXPECT_EQ(g.AddressV, D3D11_TEXTURE_ADDRESS_WRAP);
    EXPECT_EQ(g.AddressW, D3D11_TEXTURE_ADDRESS_WRAP);
    CHECK(g.MipLODBias == 0.f);
    CHECK(g.MinLOD == -D3D11_FLOAT32_MAX);
    CHECK(g.MaxLOD == D3D11_FLOAT32_MAX);
    SAFE_RELEASE(s);
}
ST(samp_getdesc_every_valid_filter_roundtrips)
{
    int f;
    SO_NEED();
    for (f = 0; f < 0x100; f++) {
        D3D11_SAMPLER_DESC d = so_samp_default(), g;
        ID3D11SamplerState *s = NULL;
        if (!base_filter_ok(f)) continue;
        d.Filter = (D3D11_FILTER)f;
        d.ComparisonFunc = D3D11_COMPARISON_GREATER_EQUAL;
        d.MaxAnisotropy = 6;
        EXPECT_HR(SO_CSAMP(&d, &s), S_OK);
        if (!s) continue;
        ID3D11SamplerState_GetDesc(s, &g);
        t_check(g.Filter == f, __FILE__, __LINE__, "filter roundtrip", "filter 0x%x got 0x%x", f, g.Filter);
        if (filter_is_aniso(f)) t_check(g.MaxAnisotropy == 6, __FILE__, __LINE__, "aniso roundtrip", "got %u", g.MaxAnisotropy);
        if (filter_is_comparison(f)) t_check(g.ComparisonFunc == D3D11_COMPARISON_GREATER_EQUAL, __FILE__, __LINE__, "cmp roundtrip", "filter 0x%x got %d", f, g.ComparisonFunc);
        SAFE_RELEASE(s);
    }
}
ST(samp_getdesc_address_modes_roundtrip)
{
    int u, v, w;
    SO_NEED();
    for (u = 1; u <= 5; u++)
        for (v = 1; v <= 5; v++) {
            D3D11_SAMPLER_DESC d = so_samp_default(), g;
            ID3D11SamplerState *s = NULL;
            w = 1 + (u + v) % 5;
            d.AddressU = (D3D11_TEXTURE_ADDRESS_MODE)u;
            d.AddressV = (D3D11_TEXTURE_ADDRESS_MODE)v;
            d.AddressW = (D3D11_TEXTURE_ADDRESS_MODE)w;
            d.BorderColor[0] = 0.5f; d.BorderColor[3] = 1.f;
            EXPECT_HR(SO_CSAMP(&d, &s), S_OK);
            if (!s) continue;
            ID3D11SamplerState_GetDesc(s, &g);
            t_check(g.AddressU == u && g.AddressV == v && g.AddressW == w, __FILE__, __LINE__, "address", "%d %d %d got %d %d %d", u, v, w, g.AddressU, g.AddressV, g.AddressW);
            SAFE_RELEASE(s);
        }
}
ST(samp_getdesc_float_fields_roundtrip)
{
    D3D11_SAMPLER_DESC d = so_samp_default(), g;
    ID3D11SamplerState *s = NULL;
    SO_NEED();
    d.Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
    d.MipLODBias = -3.25f;
    d.MaxAnisotropy = 16;
    d.ComparisonFunc = D3D11_COMPARISON_NOT_EQUAL;
    d.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    d.BorderColor[0] = 0.125f; d.BorderColor[1] = 0.25f; d.BorderColor[2] = 0.5f; d.BorderColor[3] = 0.75f;
    d.MinLOD = 1.5f;
    d.MaxLOD = 7.25f;
    EXPECT_HR(SO_CSAMP(&d, &s), S_OK);
    if (!s) return;
    ID3D11SamplerState_GetDesc(s, &g);
    CHECK(samp_eq(&g, &d));
    SAFE_RELEASE(s);
}
ST(samp_getdesc_swapped_lods_roundtrip)
{
    D3D11_SAMPLER_DESC d = so_samp_default(), g;
    ID3D11SamplerState *s = NULL;
    SO_NEED();
    d.MinLOD = 6.f;
    d.MaxLOD = 1.f;
    EXPECT_HR(SO_CSAMP(&d, &s), S_OK);
    if (!s) return;
    ID3D11SamplerState_GetDesc(s, &g);
    t_note("MinLOD 6 / MaxLOD 1 reported as %g / %g", g.MinLOD, g.MaxLOD);
    SAFE_RELEASE(s);
}
ST(samp_getdesc_bias_values_roundtrip)
{
    static const float b[] = { 0.f, 0.5f, -0.5f, 2.25f, -16.f, 15.f };
    unsigned i;
    SO_NEED();
    for (i = 0; i < sizeof b / sizeof b[0]; i++) {
        D3D11_SAMPLER_DESC d = so_samp_default(), g;
        ID3D11SamplerState *s = NULL;
        d.MipLODBias = b[i];
        EXPECT_HR(SO_CSAMP(&d, &s), S_OK);
        if (!s) continue;
        ID3D11SamplerState_GetDesc(s, &g);
        t_check(g.MipLODBias == b[i], __FILE__, __LINE__, "bias", "bias %g got %g", b[i], g.MipLODBias);
        SAFE_RELEASE(s);
    }
}
ST(samp_getdesc_all_comparison_funcs)
{
    int v;
    SO_NEED();
    for (v = 1; v <= 8; v++) {
        D3D11_SAMPLER_DESC d = so_samp_default(), g;
        ID3D11SamplerState *s = NULL;
        d.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        d.ComparisonFunc = (D3D11_COMPARISON_FUNC)v;
        EXPECT_HR(SO_CSAMP(&d, &s), S_OK);
        if (!s) continue;
        ID3D11SamplerState_GetDesc(s, &g);
        t_check(g.ComparisonFunc == v && g.Filter == d.Filter, __FILE__, __LINE__, "cmp", "func %d got %d", v, g.ComparisonFunc);
        SAFE_RELEASE(s);
    }
}
ST(samp_getdesc_inactive_fields_observation)
{
    D3D11_SAMPLER_DESC d = so_samp_default(), g;
    ID3D11SamplerState *s = NULL;
    SO_NEED();
    d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    d.ComparisonFunc = D3D11_COMPARISON_GREATER;
    d.MaxAnisotropy = 9;
    d.BorderColor[0] = d.BorderColor[1] = d.BorderColor[2] = d.BorderColor[3] = 0.3f;
    EXPECT_HR(SO_CSAMP(&d, &s), S_OK);
    if (!s) return;
    ID3D11SamplerState_GetDesc(s, &g);
    EXPECT_EQ(g.Filter, D3D11_FILTER_MIN_MAG_MIP_POINT);
    t_note("point filter, wrap: ComparisonFunc %d (set 5), MaxAnisotropy %u (set 9), border %g (set 0.3)", g.ComparisonFunc, g.MaxAnisotropy, g.BorderColor[0]);
    SAFE_RELEASE(s);
}

/* ---- dedup ----------------------------------------------------------------------- */
ST(samp_dedup_same_pointer_and_refcount)
{
    D3D11_SAMPLER_DESC d = so_samp_default();
    ID3D11SamplerState *a = NULL, *b = NULL;
    ULONG rc;
    SO_NEED();
    d.MipLODBias = 1.75f;
    EXPECT_HR(SO_CSAMP(&d, &a), S_OK);
    rc = d3dh_refcount(a);
    EXPECT_HR(SO_CSAMP(&d, &b), S_OK);
    EXPECT_EQ(a, b);
    EXPECT_EQ(d3dh_refcount(a), rc + 1);
    SAFE_RELEASE(b);
    EXPECT_EQ(d3dh_refcount(a), rc);
    SAFE_RELEASE(a);
}
ST(samp_dedup_distinguishes_active_fields)
{
    D3D11_SAMPLER_DESC base = so_samp_default(), v[8];
    ID3D11SamplerState *s0 = NULL, *si[8];
    int i, n = 0;
    SO_NEED();
    EXPECT_HR(SO_CSAMP(&base, &s0), S_OK);
    for (i = 0; i < 8; i++) v[i] = base;
    v[n++].Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    v[n++].AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    v[n++].AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
    v[n++].AddressW = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
    v[n++].MipLODBias = 0.5f;
    v[n++].MinLOD = 1.f;
    v[n++].MaxLOD = 100.f;
    v[n].Filter = D3D11_FILTER_ANISOTROPIC; v[n++].MaxAnisotropy = 4;
    for (i = 0; i < n; i++) {
        si[i] = NULL;
        EXPECT_HR(SO_CSAMP(&v[i], &si[i]), S_OK);
        t_check(si[i] != s0, __FILE__, __LINE__, "distinct", "variant %d shares the base object", i);
    }
    for (i = 0; i < n; i++) SAFE_RELEASE(si[i]);
    SAFE_RELEASE(s0);
}
ST(samp_dedup_anisotropy_distinguishes_for_anisotropic_filter)
{
    D3D11_SAMPLER_DESC d = so_samp_default(), e;
    ID3D11SamplerState *a = NULL, *b = NULL;
    SO_NEED();
    d.Filter = D3D11_FILTER_ANISOTROPIC;
    d.MaxAnisotropy = 2;
    e = d;
    e.MaxAnisotropy = 3;
    EXPECT_HR(SO_CSAMP(&d, &a), S_OK);
    EXPECT_HR(SO_CSAMP(&e, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(samp_dedup_comparison_distinguishes_for_comparison_filter)
{
    D3D11_SAMPLER_DESC d = so_samp_default(), e;
    ID3D11SamplerState *a = NULL, *b = NULL;
    SO_NEED();
    d.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    d.ComparisonFunc = D3D11_COMPARISON_LESS;
    e = d;
    e.ComparisonFunc = D3D11_COMPARISON_GREATER;
    EXPECT_HR(SO_CSAMP(&d, &a), S_OK);
    EXPECT_HR(SO_CSAMP(&e, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(samp_dedup_border_color_distinguishes_for_border_address)
{
    D3D11_SAMPLER_DESC d = so_samp_default(), e;
    ID3D11SamplerState *a = NULL, *b = NULL;
    SO_NEED();
    d.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    e = d;
    e.BorderColor[2] = 0.5f;
    EXPECT_HR(SO_CSAMP(&d, &a), S_OK);
    EXPECT_HR(SO_CSAMP(&e, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(samp_dedup_inactive_fields_observation)
{
    D3D11_SAMPLER_DESC d = so_samp_default(), e1, e2, e3;
    ID3D11SamplerState *a = NULL, *b = NULL, *c = NULL, *f = NULL;
    SO_NEED();
    e1 = d; e1.BorderColor[0] = 0.9f;               /* no border addressing */
    e2 = d; e2.ComparisonFunc = D3D11_COMPARISON_ALWAYS; /* non-comparison filter */
    e3 = d; e3.MaxAnisotropy = 7;                   /* non-anisotropic filter */
    EXPECT_HR(SO_CSAMP(&d, &a), S_OK);
    EXPECT_HR(SO_CSAMP(&e1, &b), S_OK);
    EXPECT_HR(SO_CSAMP(&e2, &c), S_OK);
    EXPECT_HR(SO_CSAMP(&e3, &f), S_OK);
    t_note("inactive border colour: %s; inactive comparison func: %s; inactive anisotropy: %s",
           a == b ? "same object" : "distinct", a == c ? "same object" : "distinct", a == f ? "same object" : "distinct");
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c); SAFE_RELEASE(f);
}
ST(samp_many_unique_states)
{
    ID3D11SamplerState *keep[200];
    int i, n = 0;
    SO_NEED();
    memset(keep, 0, sizeof keep);
    for (i = 0; i < 200; i++) {
        D3D11_SAMPLER_DESC d = so_samp_default();
        HRESULT hr;
        d.MipLODBias = (float)(i - 100) * 0.1f;
        hr = SO_CSAMP(&d, &keep[n]);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) n++;
    }
    CHECK(n == 200);
    for (i = 0; i < n; i++) SAFE_RELEASE(keep[i]);
}
