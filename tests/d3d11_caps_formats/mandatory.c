/* Formats and bits that feature levels guarantee. */
#include "cf.h"

typedef struct { UINT fmt; UINT need; } req_t;

static void check_table(ID3D11Device *d, const req_t *t, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        UINT s = cf_sup(d, t[i].fmt);
        CHECKF((s & t[i].need) == t[i].need, "%s: support %#x lacks %#x", cf_fmt_name(t[i].fmt), s, t[i].need & ~s);
    }
}

TEST(d3d11_caps_formats_mandatory_color_targets)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    static const req_t t[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM,      S_T2D | S_RT | S_BLEND | S_SAMPLE | S_LOAD | S_MIP },
        { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, S_T2D | S_RT | S_BLEND | S_SAMPLE | S_LOAD | S_MIP },
        { DXGI_FORMAT_B8G8R8A8_UNORM,      S_T2D | S_RT | S_BLEND | S_SAMPLE | S_LOAD | S_MIP },
        { DXGI_FORMAT_R16G16B16A16_FLOAT,  S_T2D | S_RT | S_BLEND | S_SAMPLE | S_LOAD | S_MIP },
        { DXGI_FORMAT_R10G10B10A2_UNORM,   S_T2D | S_RT | S_BLEND | S_SAMPLE | S_LOAD | S_MIP },
        { DXGI_FORMAT_R16G16_FLOAT,        S_T2D | S_RT | S_BLEND | S_SAMPLE | S_LOAD | S_MIP },
        { DXGI_FORMAT_R8_UNORM,            S_T2D | S_RT | S_BLEND | S_SAMPLE | S_LOAD | S_MIP },
        { DXGI_FORMAT_R32G32B32A32_FLOAT,  S_T2D | S_RT | S_LOAD | S_MIP },
        { DXGI_FORMAT_R32_FLOAT,           S_T2D | S_RT | S_LOAD | S_MIP },
    };
    if (!d) { t_skip("no D3D11 device"); return; }
    if (fl < D3D_FEATURE_LEVEL_10_0) { t_skip("below 10_0"); return; }
    check_table(d, t, sizeof(t) / sizeof(t[0]));
}
TEST(d3d11_caps_formats_mandatory_depth)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    static const req_t t[] = {
        { DXGI_FORMAT_D32_FLOAT,             S_T2D | S_DS },
        { DXGI_FORMAT_D24_UNORM_S8_UINT,     S_T2D | S_DS },
        { DXGI_FORMAT_D16_UNORM,             S_T2D | S_DS },
        { DXGI_FORMAT_D32_FLOAT_S8X24_UINT,  S_T2D | S_DS },
    };
    if (!d) { t_skip("no D3D11 device"); return; }
    if (fl < D3D_FEATURE_LEVEL_10_0) { t_skip("below 10_0"); return; }
    check_table(d, t, sizeof(t) / sizeof(t[0]));
}
TEST(d3d11_caps_formats_mandatory_input_assembler)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    static const req_t t[] = {
        { DXGI_FORMAT_R32G32B32A32_FLOAT, S_VB }, { DXGI_FORMAT_R32G32B32_FLOAT, S_VB },
        { DXGI_FORMAT_R32G32_FLOAT, S_VB },       { DXGI_FORMAT_R32_FLOAT, S_VB },
        { DXGI_FORMAT_R8G8B8A8_UNORM, S_VB },     { DXGI_FORMAT_R16G16B16A16_FLOAT, S_VB },
        { DXGI_FORMAT_R16_UINT, S_IB },           { DXGI_FORMAT_R32_UINT, S_IB },
    };
    if (!d) { t_skip("no D3D11 device"); return; }
    if (fl < D3D_FEATURE_LEVEL_10_0) { t_skip("below 10_0"); return; }
    check_table(d, t, sizeof(t) / sizeof(t[0]));
}
TEST(d3d11_caps_formats_mandatory_index_formats_only)
{
    ID3D11Device *d = cf_shared(NULL);
    UINT f;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (f = 1; f < CF_NFORMATS; f++) {
        if (f == DXGI_FORMAT_R16_UINT || f == DXGI_FORMAT_R32_UINT) continue;
        if (cf_sup(d, f) & S_IB) t_note("%s reports INDEX_BUFFER", cf_fmt_name(f));
    }
    CHECK(!(cf_sup(d, DXGI_FORMAT_R8_UINT) & S_IB));
    CHECK(!(cf_sup(d, DXGI_FORMAT_R32G32B32A32_FLOAT) & S_IB));
}
TEST(d3d11_caps_formats_mandatory_fl11_msaa_bits)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    static const req_t t[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM, S_MSRT | S_MSRESOLVE }, { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, S_MSRT | S_MSRESOLVE },
        { DXGI_FORMAT_B8G8R8A8_UNORM, S_MSRT | S_MSRESOLVE }, { DXGI_FORMAT_R16G16B16A16_FLOAT, S_MSRT | S_MSRESOLVE },
        { DXGI_FORMAT_D32_FLOAT, S_MSRT }, { DXGI_FORMAT_D24_UNORM_S8_UINT, S_MSRT }, { DXGI_FORMAT_D16_UNORM, S_MSRT },
    };
    if (!d) { t_skip("no D3D11 device"); return; }
    if (fl < D3D_FEATURE_LEVEL_11_0) { t_skip("below 11_0"); return; }
    check_table(d, t, sizeof(t) / sizeof(t[0]));
}
TEST(d3d11_caps_formats_mandatory_fl11_typed_uav)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    static const UINT f[] = { DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_SINT };
    size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    if (fl < D3D_FEATURE_LEVEL_11_0) { t_skip("below 11_0"); return; }
    for (i = 0; i < 3; i++) {
        UINT s = cf_sup(d, f[i]), s2 = cf_sup2(d, f[i]);
        CHECKF(s & S_TUAV, "%s lacks TYPED_UNORDERED_ACCESS_VIEW", cf_fmt_name(f[i]));
        CHECKF(s2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE, "%s lacks UAV_TYPED_STORE", cf_fmt_name(f[i]));
        CHECKF(s2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD, "%s lacks UAV_TYPED_LOAD", cf_fmt_name(f[i]));
    }
    {
        UINT atomic = D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD | D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS |
                      D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE | D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE |
                      D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX | D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX;
        UINT u = cf_sup2(d, DXGI_FORMAT_R32_UINT), si = cf_sup2(d, DXGI_FORMAT_R32_SINT);
        CHECKF((u & atomic) == atomic, "R32_UINT atomics %#x", u & atomic);
        CHECKF((si & atomic) == atomic, "R32_SINT atomics %#x", si & atomic);
    }
}
TEST(d3d11_caps_formats_additional_typed_uav_load_list)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2;
    static const UINT f[] = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_SINT,
        DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_R16G16B16A16_SINT,
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_SINT,
        DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_SINT, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8_SINT };
    size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    if (FAILED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2)))) { t_skip("OPTIONS2 unavailable"); return; }
    if (!o2.TypedUAVLoadAdditionalFormats) { t_skip("TypedUAVLoadAdditionalFormats is FALSE"); return; }
    for (i = 0; i < sizeof(f) / sizeof(f[0]); i++)
        CHECKF(cf_sup2(d, f[i]) & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD, "%s lacks UAV_TYPED_LOAD", cf_fmt_name(f[i]));
}
TEST(d3d11_caps_formats_support_matches_feature_level_floor)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2;
    if (!d) { t_skip("no D3D11 device"); return; }
    /* a reported feature level is one of the defined ones */
    CHECK(fl == D3D_FEATURE_LEVEL_9_1 || fl == D3D_FEATURE_LEVEL_9_2 || fl == D3D_FEATURE_LEVEL_9_3 || fl == D3D_FEATURE_LEVEL_10_0 ||
          fl == D3D_FEATURE_LEVEL_10_1 || fl == D3D_FEATURE_LEVEL_11_0 || fl == D3D_FEATURE_LEVEL_11_1);
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), fl);
    if (fl < D3D_FEATURE_LEVEL_11_1 && SUCCEEDED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2))))
        t_note("fl %s: ROV=%d conservative=%d", cf_fl_name(fl), o2.ROVsSupported, o2.ConservativeRasterizationTier);
}
TEST(d3d11_caps_formats_cast_bit_typeless_family)
{
    ID3D11Device *d = cf_shared(NULL);
    static const UINT f[] = { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT };
    size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        UINT s = cf_sup(d, f[i]);
        if (!(s & S_CAST)) t_note("%s lacks CAST_WITHIN_BIT_LAYOUT (%#x)", cf_fmt_name(f[i]), s);
    }
    CHECK(cf_sup(d, DXGI_FORMAT_R8G8B8A8_TYPELESS) & S_T2D);
    CHECK(cf_sup(d, DXGI_FORMAT_R16G16B16A16_TYPELESS) & S_T2D);
}
TEST(d3d11_caps_formats_srgb_variants_agree)
{
    ID3D11Device *d = cf_shared(NULL);
    static const UINT pairs[][2] = { { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB }, { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
        { DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM_SRGB }, { DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_UNORM_SRGB },
        { DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_UNORM_SRGB }, { DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_BC7_UNORM_SRGB } };
    size_t i; const UINT tex = S_T2D | S_T3D | S_CUBE | S_SAMPLE | S_LOAD | S_RT;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < 6; i++) {
        UINT a = cf_sup(d, pairs[i][0]), b = cf_sup(d, pairs[i][1]);
        CHECKF((a & S_T2D) == (b & S_T2D) && (a & S_SAMPLE) == (b & S_SAMPLE) && (a & S_RT) == (b & S_RT),
               "%s %#x vs %s %#x", cf_fmt_name(pairs[i][0]), a, cf_fmt_name(pairs[i][1]), b);
        (void)tex;
    }
}
TEST(d3d11_caps_formats_every_format_msaa_count1)
{
    ID3D11Device *d = cf_shared(NULL); UINT f;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (f = 1; f < CF_NFORMATS; f++) {
        UINT q = 0xdeadbeef;
        HRESULT hr = ID3D11Device_CheckMultisampleQualityLevels(d, (DXGI_FORMAT)f, 1, &q);
        if (SUCCEEDED(hr)) CHECKF(q <= 1 || q == 0, "%s count 1 has %u levels", cf_fmt_name(f), q);
        if ((cf_sup(d, f) & (S_RT | S_DS)) && !cf_is_typeless(f)) CHECKF(SUCCEEDED(hr) && q == 1, "%s renderable but count-1 answer %s q=%u", cf_fmt_name(f), d3dh_hr_name(hr), q);
    }
}
