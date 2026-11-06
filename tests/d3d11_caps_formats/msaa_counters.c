/* CheckMultisampleQualityLevels, CheckCounterInfo/CheckCounter, D3D11_REQ_* limits per feature level. */
#include "cf.h"

static HRESULT msq(ID3D11Device *d, UINT f, UINT c, UINT *q)
{
    return ID3D11Device_CheckMultisampleQualityLevels(d, (DXGI_FORMAT)f, c, q);
}

/* ---- sample count handling ---- */
static const UINT cf_msaa_fmts[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R8G8B8A8_TYPELESS };
#define N_MSAA_FMTS (sizeof(cf_msaa_fmts) / sizeof(cf_msaa_fmts[0]))

TEST(d3d11_caps_formats_msaa_count1_is_one_level)
{
    ID3D11Device *d = cf_shared(NULL); UINT q = 0xdeadbeef;
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 1, &q), S_OK);
    EXPECT_EQ(q, 1);
}
TEST(d3d11_caps_formats_msaa_count1_depth_formats)
{
    ID3D11Device *d = cf_shared(NULL); UINT q; size_t i;
    static const UINT f[] = { DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT };
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < 4; i++) { q = 0xdeadbeef; EXPECT_HR(msq(d, f[i], 1, &q), S_OK); EXPECT_EQ(q, 1); }
}
TEST(d3d11_caps_formats_msaa_count0_fails)
{
    ID3D11Device *d = cf_shared(NULL); UINT q = 0xdeadbeef; size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < N_MSAA_FMTS; i++) {
        q = 0xdeadbeef;
        HRESULT hr = msq(d, cf_msaa_fmts[i], 0, &q);
        t_check(FAILED(hr) || q == 0, __FILE__, __LINE__, "count 0", "%s hr %s q %u", cf_fmt_name(cf_msaa_fmts[i]), d3dh_hr_name(hr), q);
    }
}
TEST(d3d11_caps_formats_msaa_count_above_32_fails)
{
    static const UINT counts[] = { 33, 34, 48, 63, 64, 65, 128, 1000, 0xffff, 0x10000, 0x7fffffff, 0xffffffffu };
    ID3D11Device *d = cf_shared(NULL); UINT q; size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
        q = 0xdeadbeef;
        HRESULT hr = msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, counts[i], &q);
        t_check(FAILED(hr) || q == 0, __FILE__, __LINE__, "count > 32", "count %u got %s q %u", counts[i], d3dh_hr_name(hr), q);
    }
}
TEST(d3d11_caps_formats_msaa_non_pow2_counts_unsupported)
{
    static const UINT counts[] = { 3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 17, 24, 31 };
    ID3D11Device *d = cf_shared(NULL); UINT q; size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
        q = 0xdeadbeef;
        HRESULT hr = msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, counts[i], &q);
        t_check(FAILED(hr) || q == 0, __FILE__, __LINE__, "non power of two count", "count %u got %s q=%u", counts[i], d3dh_hr_name(hr), q);
    }
}
TEST(d3d11_caps_formats_msaa_pow2_counts_succeed)
{
    ID3D11Device *d = cf_shared(NULL); UINT q, c, i;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < N_MSAA_FMTS; i++)
        for (c = 1; c <= 32; c <<= 1) {
            q = 0xdeadbeef;
            HRESULT hr = msq(d, cf_msaa_fmts[i], c, &q);
            t_check(hr == S_OK, __FILE__, __LINE__, "pow2 count", "%s x%u got %s", cf_fmt_name(cf_msaa_fmts[i]), c, d3dh_hr_name(hr));
            CHECK(q != 0xdeadbeef);
            t_note("%s x%u: %u quality levels", cf_fmt_name(cf_msaa_fmts[i]), c, q);
        }
}
TEST(d3d11_caps_formats_msaa_unknown_format)
{
    ID3D11Device *d = cf_shared(NULL); UINT q = 0xdeadbeef;
    if (!d) { t_skip("no D3D11 device"); return; }
    HRESULT hr = msq(d, DXGI_FORMAT_UNKNOWN, 2, &q);
    CHECKF(FAILED(hr) || q == 0, "UNKNOWN format has %u levels", q);
    t_note("MSAA(UNKNOWN, 2) = %s", d3dh_hr_name(hr));
}
TEST(d3d11_caps_formats_msaa_huge_format_no_levels)
{
    ID3D11Device *d = cf_shared(NULL); UINT q = 0xdeadbeef;
    if (!d) { t_skip("no D3D11 device"); return; }
    static const UINT bad[] = { 65536, 0xffffffffu, 0x7fffffffu };
    size_t i;
    for (i = 0; i < 3; i++) {
        q = 0xdeadbeef;
        HRESULT hr = msq(d, bad[i], 2, &q);
        CHECKF(FAILED(hr) || q == 0, "format %#x has %u levels", bad[i], q);
        t_note("MSAA(%#x, 2) = %s", bad[i], d3dh_hr_name(hr));
    }
}
TEST(d3d11_caps_formats_msaa_bc_has_no_levels)
{
    ID3D11Device *d = cf_shared(NULL); UINT q; UINT f;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (f = DXGI_FORMAT_BC1_TYPELESS; f <= DXGI_FORMAT_BC7_UNORM_SRGB; f++) {
        if (!cf_is_defined(f) || !cf_is_bc(f)) continue;     /* 85..93 (B5G6R5, B8G8R8A8...) sit between BC5 and BC6H and are multisample capable */
        q = 0xdeadbeef;
        HRESULT hr = msq(d, f, 2, &q);
        CHECKF(FAILED(hr) || q == 0, "%s x2 has %u levels", cf_fmt_name(f), q);
        q = 0xdeadbeef;
        hr = msq(d, f, 4, &q);
        CHECKF(FAILED(hr) || q == 0, "%s x4 has %u levels", cf_fmt_name(f), q);
    }
}
TEST(d3d11_caps_formats_msaa_fl11_requires_4x_for_core_formats)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    static const UINT f[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R32_FLOAT,
        DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D16_UNORM };
    size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    if (fl < D3D_FEATURE_LEVEL_11_0) { t_skip("device below 11_0"); return; }
    for (i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        UINT q4 = cf_msq(d, f[i], 4);
        t_check(q4 >= 1, __FILE__, __LINE__, "4x MSAA mandatory at 11_0", "%s", cf_fmt_name(f[i]));
    }
}
TEST(d3d11_caps_formats_msaa_levels_imply_format_bits)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl); UINT f, c;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (f = 1; f < CF_NFORMATS; f++) {
        UINT s = cf_sup(d, f), any = 0;
        for (c = 2; c <= 16; c <<= 1) any |= cf_msq(d, f, c);
        if (any && !cf_is_typeless(f)) CHECKF(s & (S_MSRT | S_DS | S_MSLOAD | S_MSRESOLVE), "%s has MSAA levels but support word %#x", cf_fmt_name(f), s);
        if ((s & S_MSRT) && !cf_is_typeless(f)) CHECKF(any, "%s advertises MULTISAMPLE_RENDERTARGET but no count 2..16 has levels", cf_fmt_name(f));
        if (!cf_is_defined(f)) CHECKF(!any, "undefined id %u has MSAA levels", f);
    }
}
TEST(d3d11_caps_formats_msaa_count_monotonic_note)
{
    ID3D11Device *d = cf_shared(NULL); UINT f, c;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (f = 1; f < CF_NFORMATS; f++) {
        UINT prev = 1;
        for (c = 2; c <= 32; c <<= 1) { UINT q = cf_msq(d, f, c); if (q && !prev) t_note("%s: x%u supported but a lower count is not", cf_fmt_name(f), c); if (c == 2 || q) prev = q; }
    }
}
TEST(d3d11_caps_formats_msaa_typeless_matches_typed)
{
    ID3D11Device *d = cf_shared(NULL);
    if (!d) { t_skip("no D3D11 device"); return; }
    if ((cf_msq(d, DXGI_FORMAT_R8G8B8A8_TYPELESS, 4) != 0) != (cf_msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 4) != 0)) t_note("typeless and typed RGBA8 disagree on 4x MSAA");
}
TEST(d3d11_caps_formats_msaa_quality_range_usable_for_texture)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl); UINT q4, c;
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *tex;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (c = 2; c <= 8; c <<= 1) {
        q4 = cf_msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, c);
        if (!q4) continue;
        memset(&td, 0, sizeof(td));
        td.Width = 32; td.Height = 32; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = c; td.SampleDesc.Quality = 0; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
        tex = NULL;
        EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &tex), S_OK);
        SAFE_RELEASE(tex);
        td.SampleDesc.Quality = q4 - 1; tex = NULL;
        EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &tex), S_OK);
        SAFE_RELEASE(tex);
        td.SampleDesc.Quality = q4; tex = NULL;
        EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &tex), E_INVALIDARG);
        SAFE_RELEASE(tex);
    }
}
TEST(d3d11_caps_formats_msaa_unsupported_count_rejects_texture)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *tex = NULL; UINT c;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (c = 3; c <= 7; c += 2) {
        memset(&td, 0, sizeof(td));
        td.Width = 32; td.Height = 32; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = c; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
        EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &tex), E_INVALIDARG);
        SAFE_RELEASE(tex);
    }
}
static void msaa_level(D3D_FEATURE_LEVEL want)
{
    ID3D11Device *d = cf_make(want); UINT q;
    if (!d) { t_skip("feature level %s unavailable", cf_fl_name(want)); return; }
    q = 0; EXPECT_HR(msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 1, &q), S_OK); EXPECT_EQ(q, 1);
    q = 0xdeadbeef; { HRESULT h = msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &q); CHECK(FAILED(h) || q == 0); }
    q = 0xdeadbeef; { HRESULT h = msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 64, &q); CHECK(FAILED(h) || q == 0); }
    q = 0xdeadbeef; { HRESULT h = msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 15, &q); CHECK(FAILED(h) || q == 0); }
    q = 0xdeadbeef; { HRESULT h = msq(d, DXGI_FORMAT_BC3_UNORM, 2, &q); CHECK(FAILED(h) || q == 0); }
    if (want >= D3D_FEATURE_LEVEL_10_1) CHECK(cf_msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 4) >= 1);
    t_note("fl %s: RGBA8 x2=%u x4=%u x8=%u", cf_fl_name(want), cf_msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 2), cf_msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 4), cf_msq(d, DXGI_FORMAT_R8G8B8A8_UNORM, 8));
    SAFE_RELEASE(d);
}
#define MLV(n, l) TEST(d3d11_caps_formats_msaa_level_##n) { msaa_level(l); }
MLV(9_1, D3D_FEATURE_LEVEL_9_1) MLV(9_2, D3D_FEATURE_LEVEL_9_2) MLV(9_3, D3D_FEATURE_LEVEL_9_3)
MLV(10_0, D3D_FEATURE_LEVEL_10_0) MLV(10_1, D3D_FEATURE_LEVEL_10_1) MLV(11_0, D3D_FEATURE_LEVEL_11_0) MLV(11_1, D3D_FEATURE_LEVEL_11_1)

/* ---- counters ---- */
TEST(d3d11_caps_formats_counter_info_ranges)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_COUNTER_INFO ci;
    if (!d) { t_skip("no D3D11 device"); return; }
    memset(&ci, 0xef, sizeof(ci));
    ID3D11Device_CheckCounterInfo(d, &ci);
    CHECKF(ci.LastDeviceDependentCounter == 0 || ci.LastDeviceDependentCounter >= D3D11_COUNTER_DEVICE_DEPENDENT_0,
           "LastDeviceDependentCounter %#x", ci.LastDeviceDependentCounter);
    CHECK(ci.NumSimultaneousCounters != 0xefefefefu);
    t_note("counters: last=%#x simultaneous=%u parallel units=%u", ci.LastDeviceDependentCounter, ci.NumSimultaneousCounters, ci.NumDetectableParallelUnits);
}
TEST(d3d11_caps_formats_counter_info_stable)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_COUNTER_INFO a, b;
    if (!d) { t_skip("no D3D11 device"); return; }
    memset(&a, 0, sizeof(a)); memset(&b, 0xff, sizeof(b));
    ID3D11Device_CheckCounterInfo(d, &a); ID3D11Device_CheckCounterInfo(d, &b);
    EXPECT_EQ(a.LastDeviceDependentCounter, b.LastDeviceDependentCounter);
    EXPECT_EQ(a.NumSimultaneousCounters, b.NumSimultaneousCounters);
    EXPECT_EQ(a.NumDetectableParallelUnits, b.NumDetectableParallelUnits);
}
TEST(d3d11_caps_formats_counter_info_no_standard_counters)
{
    /* D3D11 defines no standard counters: with no device-dependent counters the last counter is 0. */
    ID3D11Device *d = cf_shared(NULL); D3D11_COUNTER_INFO ci;
    if (!d) { t_skip("no D3D11 device"); return; }
    ID3D11Device_CheckCounterInfo(d, &ci);
    if (ci.LastDeviceDependentCounter == 0) {
        if (ci.NumSimultaneousCounters || ci.NumDetectableParallelUnits)
            t_note("no counters but simultaneous=%u units=%u", ci.NumSimultaneousCounters, ci.NumDetectableParallelUnits);
    } else t_note("device-dependent counters exist (last %#x)", ci.LastDeviceDependentCounter);
}
TEST(d3d11_caps_formats_counter_check_invalid_desc)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_COUNTER_DESC cd; D3D11_COUNTER_TYPE t; UINT active, nl = 0, ul = 0, dl = 0;
    D3D11_COUNTER_INFO ci;
    if (!d) { t_skip("no D3D11 device"); return; }
    ID3D11Device_CheckCounterInfo(d, &ci);
    cd.Counter = (D3D11_COUNTER)0; cd.MiscFlags = 0;
    HRESULT hr = ID3D11Device_CheckCounter(d, &cd, &t, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    CHECKF(FAILED(hr), "counter 0 (below DEVICE_DEPENDENT_0) gave %s", d3dh_hr_name(hr));
    t_note("CheckCounter(0) = %s", d3dh_hr_name(hr));
    cd.Counter = (D3D11_COUNTER)0x7fffffff;
    hr = ID3D11Device_CheckCounter(d, &cd, &t, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    CHECKF(FAILED(hr), "counter 0x7fffffff gave %s", d3dh_hr_name(hr));
    cd.Counter = (D3D11_COUNTER)0x40000000;
    cd.MiscFlags = 1;
    hr = ID3D11Device_CheckCounter(d, &cd, &t, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    if (ci.LastDeviceDependentCounter == 0) CHECKF(FAILED(hr), "DEVICE_DEPENDENT_0 without device counters gave %s", d3dh_hr_name(hr));
    t_note("CheckCounter(DEVICE_DEPENDENT_0) = %s", d3dh_hr_name(hr));
}
TEST(d3d11_caps_formats_counter_check_first_device_counter)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_COUNTER_INFO ci; D3D11_COUNTER_DESC cd;
    D3D11_COUNTER_TYPE t = (D3D11_COUNTER_TYPE)99; UINT active = 0xdeadbeef, nl = 0, ul = 0, dl = 0;
    char name[128], units[128], desc[256];
    if (!d) { t_skip("no D3D11 device"); return; }
    ID3D11Device_CheckCounterInfo(d, &ci);
    if (ci.LastDeviceDependentCounter < D3D11_COUNTER_DEVICE_DEPENDENT_0) {
        /* Windows 10 Basic Render Driver: no device counters, so the first one is rejected */
        cd.Counter = D3D11_COUNTER_DEVICE_DEPENDENT_0; cd.MiscFlags = 0;
        EXPECT_EQ(ci.LastDeviceDependentCounter, 0);
        EXPECT_HR(ID3D11Device_CheckCounter(d, &cd, &t, &active, NULL, &nl, NULL, &ul, NULL, &dl), E_INVALIDARG);
        return;
    }
    cd.Counter = D3D11_COUNTER_DEVICE_DEPENDENT_0; cd.MiscFlags = 0;
    /* size query: lengths in, no buffers */
    HRESULT hr = ID3D11Device_CheckCounter(d, &cd, &t, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    EXPECT_HR(hr, S_OK);
    CHECK(nl > 0 && nl < sizeof(name) && ul < sizeof(units) && dl < sizeof(desc));
    CHECK(t <= D3D11_COUNTER_TYPE_UINT64);
    CHECK(active >= 1);
    if (nl && nl < sizeof(name) && ul < sizeof(units) && dl < sizeof(desc)) {
        UINT nl2 = nl, ul2 = ul, dl2 = dl;
        hr = ID3D11Device_CheckCounter(d, &cd, &t, &active, name, &nl2, units, &ul2, desc, &dl2);
        EXPECT_HR(hr, S_OK);
        EXPECT_EQ(strlen(name) + 1, nl);
    }
}

/* ---- D3D11_REQ_* limits ---- */
TEST(d3d11_caps_formats_req_constants)
{
    EXPECT_EQ(D3D11_REQ_TEXTURE1D_U_DIMENSION, 16384);
    EXPECT_EQ(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION, 16384);
    EXPECT_EQ(D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION, 2048);
    EXPECT_EQ(D3D11_REQ_TEXTURECUBE_DIMENSION, 16384);
    EXPECT_EQ(D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION, 2048);
    EXPECT_EQ(D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION, 2048);
    EXPECT_EQ(D3D11_REQ_MAXANISOTROPY, 16);
    EXPECT_EQ(D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES, 2048);
    EXPECT_EQ(D3D11_REQ_RENDER_TO_BUFFER_WINDOW_WIDTH, 16384);
    EXPECT_EQ(D3D11_REQ_MIP_LEVELS, 15);
    EXPECT_EQ(D3D11_REQ_BLEND_OBJECT_COUNT_PER_DEVICE, 4096);
    EXPECT_EQ(D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT, 4096);
    EXPECT_EQ(D3D11_REQ_DRAW_VERTEX_COUNT_2_TO_EXP, 32);
    EXPECT_EQ(D3D11_REQ_DRAWINDEXED_INDEX_COUNT_2_TO_EXP, 32);
    EXPECT_EQ(D3D11_REQ_FILTERING_HW_ADDRESSABLE_RESOURCE_DIMENSION, 16384);
    EXPECT_EQ(D3D11_REQ_RASTERIZER_OBJECT_COUNT_PER_DEVICE, 4096);
    EXPECT_EQ(D3D11_REQ_SAMPLER_OBJECT_COUNT_PER_DEVICE, 4096);
    EXPECT_EQ(D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION, 2048);
}
static D3D11_TEXTURE2D_DESC t2d(UINT w, UINT h, UINT arr, DXGI_FORMAT f, UINT misc)
{
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof(td));
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = arr; td.Format = f;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.MiscFlags = misc;
    return td;
}
static HRESULT mk2d(ID3D11Device *d, UINT w, UINT h, UINT arr, UINT misc)
{
    D3D11_TEXTURE2D_DESC td = t2d(w, h, arr, DXGI_FORMAT_R8_UNORM, misc); ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &t);
    SAFE_RELEASE(t);
    return hr;
}
static HRESULT mk1d(ID3D11Device *d, UINT w, UINT arr)
{
    D3D11_TEXTURE1D_DESC td; ID3D11Texture1D *t = NULL; HRESULT hr;
    memset(&td, 0, sizeof(td)); td.Width = w; td.MipLevels = 1; td.ArraySize = arr; td.Format = DXGI_FORMAT_R8_UNORM;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    hr = ID3D11Device_CreateTexture1D(d, &td, NULL, &t); SAFE_RELEASE(t); return hr;
}
static HRESULT mk3d(ID3D11Device *d, UINT w, UINT h, UINT z)
{
    D3D11_TEXTURE3D_DESC td; ID3D11Texture3D *t = NULL; HRESULT hr;
    memset(&td, 0, sizeof(td)); td.Width = w; td.Height = h; td.Depth = z; td.MipLevels = 1; td.Format = DXGI_FORMAT_R8_UNORM;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    hr = ID3D11Device_CreateTexture3D(d, &td, NULL, &t); SAFE_RELEASE(t); return hr;
}
/* limits per level: tex1d/2d max, cube max, 3d max, array max */
static void limits_at(D3D_FEATURE_LEVEL fl, UINT max2d, UINT maxcube, UINT max3d, UINT maxarr)
{
    ID3D11Device *d = cf_make(fl);
    if (!d) { t_skip("feature level %s unavailable", cf_fl_name(fl)); return; }
    EXPECT_HR(mk2d(d, max2d, 16, 1, 0), S_OK);
    EXPECT_HR(mk2d(d, 16, max2d, 1, 0), S_OK);
    if (fl < D3D_FEATURE_LEVEL_10_0) {
        /* Windows 10: 9_x devices reject anything above 4096 (also at 9_1/9_2, whose documented limit is 2048) */
        EXPECT_HR(mk2d(d, max2d + 1, 16, 1, 0), E_INVALIDARG);
        EXPECT_HR(mk2d(d, 16, max2d + 1, 1, 0), E_INVALIDARG);
    } else {
        /* Windows 10 (WARP and the basic render driver): the per-dimension 2D limit is not enforced for a 16-wide R8 texture
           (1M wide succeeds); only the total size limits it.  Tex1D, cube and arrays are enforced. */
        HRESULT hr = mk2d(d, max2d + 1, 16, 1, 0);
        if (hr != S_OK) t_note("2D width %u above the %s limit: %s (Win10 oracle: S_OK)", max2d + 1, cf_fl_name(fl), d3dh_hr_name(hr));
        EXPECT_HR(mk2d(d, 1u << 20, 16, 1, 0), S_OK);
    }
    EXPECT_HR(mk2d(d, 0, 16, 1, 0), E_INVALIDARG);
    EXPECT_HR(mk2d(d, 16, 0, 1, 0), E_INVALIDARG);
    EXPECT_HR(mk2d(d, 16, 16, 0, 0), E_INVALIDARG);
    if (fl >= D3D_FEATURE_LEVEL_10_0) {
        EXPECT_HR(mk1d(d, max2d, 1), S_OK);
        EXPECT_HR(mk1d(d, max2d + 1, 1), E_INVALIDARG);
        EXPECT_HR(mk1d(d, 16, maxarr), S_OK);
        EXPECT_HR(mk1d(d, 16, maxarr + 1), E_INVALIDARG);
        EXPECT_HR(mk2d(d, 16, 16, maxarr, 0), S_OK);
        EXPECT_HR(mk2d(d, 16, 16, maxarr + 1, 0), E_INVALIDARG);
    }
    EXPECT_HR(mk3d(d, max3d, 1, 1), S_OK);
    EXPECT_HR(mk3d(d, 1, max3d, 1), S_OK);
    EXPECT_HR(mk3d(d, 1, 1, max3d), S_OK);
    EXPECT_HR(mk3d(d, max3d + 1, 1, 1), E_INVALIDARG);
    EXPECT_HR(mk3d(d, 1, 1, max3d + 1), E_INVALIDARG);
    if (fl >= D3D_FEATURE_LEVEL_10_0 && maxcube > 2048) {
        if (maxcube <= 8192) EXPECT_HR(mk2d(d, maxcube, maxcube, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), S_OK);
        EXPECT_HR(mk2d(d, maxcube + 1, maxcube + 1, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), E_INVALIDARG);
    }
    if (maxcube <= 2048) {
        EXPECT_HR(mk2d(d, maxcube, maxcube, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), S_OK);
        EXPECT_HR(mk2d(d, maxcube + 1, maxcube + 1, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), E_INVALIDARG);
    }
    EXPECT_HR(mk2d(d, 16, 16, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), S_OK);
    EXPECT_HR(mk2d(d, 16, 16, 5, D3D11_RESOURCE_MISC_TEXTURECUBE), E_INVALIDARG);
    EXPECT_HR(mk2d(d, 16, 32, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), E_INVALIDARG);
    SAFE_RELEASE(d);
}
/* 9_x arrays are not available; pass maxarr 1 there (array tests are skipped below 10_0) */
TEST(d3d11_caps_formats_limits_fl9_1)  { limits_at(D3D_FEATURE_LEVEL_9_1, 4096, 512, 256, 1); }
TEST(d3d11_caps_formats_limits_fl9_2)  { limits_at(D3D_FEATURE_LEVEL_9_2, 4096, 512, 256, 1); }
TEST(d3d11_caps_formats_limits_fl9_3)  { limits_at(D3D_FEATURE_LEVEL_9_3, 4096, 4096, 256, 1); }
TEST(d3d11_caps_formats_limits_fl10_0) { limits_at(D3D_FEATURE_LEVEL_10_0, 8192, 8192, 2048, 512); }
TEST(d3d11_caps_formats_limits_fl10_1) { limits_at(D3D_FEATURE_LEVEL_10_1, 8192, 8192, 2048, 512); }
TEST(d3d11_caps_formats_limits_fl11_0) { limits_at(D3D_FEATURE_LEVEL_11_0, 16384, 16384, 2048, 2048); }
TEST(d3d11_caps_formats_limits_fl11_1) { limits_at(D3D_FEATURE_LEVEL_11_1, 16384, 16384, 2048, 2048); }

TEST(d3d11_caps_formats_limits_mip_levels)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t = NULL;
    if (!d) { t_skip("no D3D11 device"); return; }
    td = t2d(16384, 16384 / 4096, 1, DXGI_FORMAT_R8_UNORM, 0);
    td.MipLevels = 0; /* full chain: 15 levels for 16384 */
    HRESULT hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &t);
    if (SUCCEEDED(hr)) { D3D11_TEXTURE2D_DESC o; ID3D11Texture2D_GetDesc(t, &o); EXPECT_EQ(o.MipLevels, 15); }
    else t_note("16384 wide full mip chain failed: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t);
    td = t2d(16, 16, 1, DXGI_FORMAT_R8_UNORM, 0);
    td.MipLevels = 5; EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &t), S_OK); SAFE_RELEASE(t);
    td.MipLevels = 6; EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &t), E_INVALIDARG); SAFE_RELEASE(t);
    td.MipLevels = 0; EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &t), S_OK);
    if (t) { D3D11_TEXTURE2D_DESC o; ID3D11Texture2D_GetDesc(t, &o); EXPECT_EQ(o.MipLevels, 5); }
    SAFE_RELEASE(t);
}
TEST(d3d11_caps_formats_limits_bc_dimensions)
{
    ID3D11Device *d = cf_shared(NULL); ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC td;
    if (!d) { t_skip("no D3D11 device"); return; }
    td = t2d(5, 5, 1, DXGI_FORMAT_BC1_UNORM, 0);
    HRESULT hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &t);
    t_note("BC1 5x5 single-mip texture: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t);
    td = t2d(8, 8, 1, DXGI_FORMAT_BC1_UNORM, 0);
    EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &t), S_OK); SAFE_RELEASE(t);
    td = t2d(1, 1, 1, DXGI_FORMAT_BC1_UNORM, 0);
    hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &t);
    t_note("BC1 1x1 texture: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t);
}
