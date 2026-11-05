/* Feature level selection: which level is returned, ordering rules, per-level limits. */
#include "dc.h"

#define T(n) TEST(d3d11_device_create_##n)

/* create with an explicit list on WARP; skips the test when WARP cannot be created at all */
static HRESULT mk(const D3D_FEATURE_LEVEL *l, UINT n, ID3D11Device **d, ID3D11DeviceContext **c, D3D_FEATURE_LEVEL *fl)
{
    return dc_warp(0, l, n, d, c, fl);
}

/* single level that WARP supports on every Windows 10 */
#define SINGLE(nm, lvl) T(level_only_##nm) { \
    D3D_FEATURE_LEVEL l = lvl, fl = 0; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; \
    REQUIRE(DC_FN()); \
    HRESULT hr = mk(&l, 1, &d, &c, &fl); \
    if (FAILED(hr)) { t_skip("no WARP at " #nm ": %s", d3dh_hr_name(hr)); return; } \
    EXPECT_HR_OK(hr); EXPECT_EQ(fl, lvl); CHECK(d && c); \
    if (d) EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), lvl); \
    DC_REL(d, c); }
SINGLE(9_1, D3D_FEATURE_LEVEL_9_1)
SINGLE(9_2, D3D_FEATURE_LEVEL_9_2)
SINGLE(9_3, D3D_FEATURE_LEVEL_9_3)
SINGLE(10_0, D3D_FEATURE_LEVEL_10_0)
SINGLE(10_1, D3D_FEATURE_LEVEL_10_1)
SINGLE(11_0, D3D_FEATURE_LEVEL_11_0)

/* levels whose availability depends on the WARP/driver version: result must be consistent either way */
#define SINGLE_OPT(nm, lvl) T(level_only_##nm) { \
    D3D_FEATURE_LEVEL l = lvl, fl = DC_FL_SENTINEL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; \
    REQUIRE(DC_FN()); \
    HRESULT hr = mk(&l, 1, &d, &c, &fl); \
    if (SUCCEEDED(hr)) { EXPECT_EQ(fl, lvl); CHECK(d && c); if (d) EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), lvl); } \
    else { t_note(#nm " not available on WARP: %s", d3dh_hr_name(hr)); CHECK(!d && !c); EXPECT_EQ(fl, 0); } \
    DC_REL(d, c); }
SINGLE_OPT(11_1, D3D_FEATURE_LEVEL_11_1)
SINGLE_OPT(12_0, D3D_FEATURE_LEVEL_12_0)
SINGLE_OPT(12_1, D3D_FEATURE_LEVEL_12_1)

/* order: the first entry that can be satisfied wins, even if a higher one is available later */
#define ORDER2(nm, a, b, want) T(order_##nm) { \
    D3D_FEATURE_LEVEL l[2] = { a, b }, fl = 0; ID3D11Device *d = NULL; \
    REQUIRE(DC_FN()); \
    HRESULT hr = mk(l, 2, &d, NULL, &fl); \
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; } \
    EXPECT_EQ(fl, want); if (d) EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), want); SAFE_RELEASE(d); }
ORDER2(9_1_then_11_0, D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_9_1)
ORDER2(11_0_then_9_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_11_0)
ORDER2(10_0_then_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0)
ORDER2(10_1_then_10_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1)
ORDER2(9_3_then_9_2, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3)
ORDER2(9_2_then_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2)
ORDER2(dup_10_0, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_0)
ORDER2(9_3_then_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3)
ORDER2(10_0_then_9_3, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_0)

T(order_full_descending_list)
{
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(l, 9, &d, NULL, &fl);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    CHECK(fl >= D3D_FEATURE_LEVEL_11_0);       /* WARP implements at least 11_0 on Windows 10 */
    t_note("full descending list on WARP -> %s", dc_fl_name(fl));
    if (d) EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), fl);
    SAFE_RELEASE(d);
}
T(order_full_ascending_list_returns_lowest)
{
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_12_1 };
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(l, 9, &d, NULL, &fl);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_9_1);
    SAFE_RELEASE(d);
}
T(order_unsupported_first_falls_through)
{
    /* 12_1 may or may not be available; the next entries are tried in order */
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(l, 2, &d, NULL, &fl);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    CHECK(fl == D3D_FEATURE_LEVEL_12_1 || fl == D3D_FEATURE_LEVEL_10_0);
    SAFE_RELEASE(d);
}
T(order_result_always_member_of_list)
{
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_10_1 };
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(l, 2, &d, NULL, &fl);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    CHECK(fl == D3D_FEATURE_LEVEL_9_3 || fl == D3D_FEATURE_LEVEL_10_1);
    SAFE_RELEASE(d);
}

T(default_list_ceiling_is_11_0)
{
    /* NULL list = {11_0, 10_1, 10_0, 9_3, 9_2, 9_1}; 11_1 and above are never selected implicitly */
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(NULL, 0, &d, NULL, &fl);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    CHECK(fl <= D3D_FEATURE_LEVEL_11_0);
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_11_0);
    SAFE_RELEASE(d);
}
T(default_list_equals_explicit_default_list)
{
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    D3D_FEATURE_LEVEL a = 0, b = 0; ID3D11Device *d1 = NULL, *d2 = NULL;
    REQUIRE(DC_FN());
    HRESULT h1 = mk(NULL, 0, &d1, NULL, &a), h2 = mk(l, 6, &d2, NULL, &b);
    if (FAILED(h1) || FAILED(h2)) { SAFE_RELEASE(d1); SAFE_RELEASE(d2); t_skip("no WARP"); return; }
    EXPECT_EQ(a, b);
    SAFE_RELEASE(d1); SAFE_RELEASE(d2);
}
T(default_list_with_flags_zero_device_feature_level_matches)
{
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(NULL, 0, &d, NULL, &fl);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), fl);
    SAFE_RELEASE(d);
}

T(level_11_1_not_in_default_even_if_supported)
{
    /* explicit request for 11_1 may succeed, the implicit one never reports it */
    D3D_FEATURE_LEVEL l = D3D_FEATURE_LEVEL_11_1, fl = 0, fl2 = 0; ID3D11Device *d = NULL, *d2 = NULL;
    REQUIRE(DC_FN());
    if (FAILED(mk(NULL, 0, &d, NULL, &fl2))) { t_skip("no WARP"); return; }
    HRESULT hr = mk(&l, 1, &d2, NULL, &fl);
    if (SUCCEEDED(hr)) EXPECT_EQ(fl, D3D_FEATURE_LEVEL_11_1);
    CHECK(fl2 != D3D_FEATURE_LEVEL_11_1);
    SAFE_RELEASE(d); SAFE_RELEASE(d2);
}

T(levels_unsupported_returns_failure_and_clears)
{
    /* nothing supported can be requested on WARP at 12_x on old systems; when it fails the outputs must be cleared */
    D3D_FEATURE_LEVEL l = D3D_FEATURE_LEVEL_12_1, fl = DC_FL_SENTINEL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(&l, 1, &d, &c, &fl);
    if (FAILED(hr)) {
        t_note("12_1 only on WARP -> %s", d3dh_hr_name(hr));
        CHECK(d == NULL); CHECK(c == NULL); EXPECT_EQ(fl, 0);
    } else {
        EXPECT_EQ(fl, D3D_FEATURE_LEVEL_12_1);
    }
    DC_REL(d, c);
}
T(levels_12_x_request_on_unsupported_not_invalid_arg)
{
    /* 12_0 and 12_1 are valid enum values for the entry point (validation passes); lack of support is not E_INVALIDARG */
    D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_12_0 };
    ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(l, 1, &d, NULL, NULL);
    t_note("12_0 only on WARP -> %s", d3dh_hr_name(hr));
    CHECK(hr != E_INVALIDARG);
    SAFE_RELEASE(d);
}
T(levels_11_1_request_not_invalid_arg)
{
    D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    HRESULT hr = mk(l, 2, &d, NULL, &fl);
    if (FAILED(hr)) { t_skip("no WARP: %s", d3dh_hr_name(hr)); return; }
    CHECK(fl == D3D_FEATURE_LEVEL_11_1 || fl == D3D_FEATURE_LEVEL_11_0);
    SAFE_RELEASE(d);
}

/* ---- per-level limits through argument-only validation (NULL out pointer on Create* validates only) ---- */
static HRESULT tex2d_validate(ID3D11Device *d, UINT w, UINT h, UINT arr)
{
    D3D11_TEXTURE2D_DESC td = { 0 };
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = arr; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    return ID3D11Device_CreateTexture2D(d, &td, NULL, NULL);
}
#define LIMIT(nm, lvl, maxdim) T(limit_texture2d_##nm) { \
    D3D_FEATURE_LEVEL l = lvl, fl = 0; ID3D11Device *d = NULL; \
    REQUIRE(DC_FN()); \
    if (FAILED(mk(&l, 1, &d, NULL, &fl))) { t_skip("no WARP at " #nm); return; } \
    HRESULT ok = tex2d_validate(d, maxdim, 1, 1), bad = tex2d_validate(d, (maxdim) + 1, 1, 1); \
    CHECK(SUCCEEDED(ok)); EXPECT_HR(bad, E_INVALIDARG); \
    SAFE_RELEASE(d); }
LIMIT(9_1, D3D_FEATURE_LEVEL_9_1, 2048)
LIMIT(9_2, D3D_FEATURE_LEVEL_9_2, 2048)
LIMIT(9_3, D3D_FEATURE_LEVEL_9_3, 4096)
LIMIT(10_0, D3D_FEATURE_LEVEL_10_0, 8192)
LIMIT(10_1, D3D_FEATURE_LEVEL_10_1, 8192)
LIMIT(11_0, D3D_FEATURE_LEVEL_11_0, 16384)

T(limit_texture_array_size_10_0)
{
    D3D_FEATURE_LEVEL l = D3D_FEATURE_LEVEL_10_0, fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    if (FAILED(mk(&l, 1, &d, NULL, &fl))) { t_skip("no WARP"); return; }
    CHECK(SUCCEEDED(tex2d_validate(d, 16, 16, 512)));
    EXPECT_HR(tex2d_validate(d, 16, 16, 513), E_INVALIDARG);
    SAFE_RELEASE(d);
}
T(check_feature_support_bad_size)
{
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    if (FAILED(mk(NULL, 0, &d, NULL, &fl))) { t_skip("no WARP"); return; }
    D3D11_FEATURE_DATA_THREADING th = { 0 };
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_THREADING, &th, 1), E_INVALIDARG);
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_THREADING, &th, sizeof th + 4), E_INVALIDARG);
    CHECK(FAILED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_THREADING, NULL, sizeof th)));
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, (D3D11_FEATURE)0x7777, &th, sizeof th), E_INVALIDARG);
    SAFE_RELEASE(d);
}
T(check_feature_support_threading_warp_values)
{
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL;
    REQUIRE(DC_FN());
    if (FAILED(mk(NULL, 0, &d, NULL, &fl))) { t_skip("no WARP"); return; }
    D3D11_FEATURE_DATA_THREADING th = { 7, 7 };
    EXPECT_HR_OK(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_THREADING, &th, sizeof th));
    CHECK(th.DriverConcurrentCreates == 0 || th.DriverConcurrentCreates == 1);
    CHECK(th.DriverCommandLists == 0 || th.DriverCommandLists == 1);
    SAFE_RELEASE(d);
}
T(check_format_support_baseline)
{
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL; UINT s = 0;
    REQUIRE(DC_FN());
    if (FAILED(mk(NULL, 0, &d, NULL, &fl))) { t_skip("no WARP"); return; }
    EXPECT_HR_OK(ID3D11Device_CheckFormatSupport(d, DXGI_FORMAT_R8G8B8A8_UNORM, &s));
    CHECK((s & D3D11_FORMAT_SUPPORT_TEXTURE2D) && (s & D3D11_FORMAT_SUPPORT_RENDER_TARGET));
    SAFE_RELEASE(d);
}
T(check_multisample_quality_baseline)
{
    D3D_FEATURE_LEVEL fl = 0; ID3D11Device *d = NULL; UINT q = 77;
    REQUIRE(DC_FN());
    if (FAILED(mk(NULL, 0, &d, NULL, &fl))) { t_skip("no WARP"); return; }
    EXPECT_HR_OK(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, 1, &q));
    CHECK(q >= 1);
    q = 77;
    EXPECT_HR_OK(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, 3, &q));
    EXPECT_EQ(q, 0);
    SAFE_RELEASE(d);
}
