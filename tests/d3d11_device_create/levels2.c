/* Per-feature-level resource limits and capability values of the WARP rasteriser (Win10 22H2). Every value below was
 * measured on the reference VM; WARP is the device under test here (d3dh prefers it), so the table is WARP specific. */
#include "dc.h"

#define T(n) TEST(d3d11_device_create_##n)

static HRESULT lv_mk(D3D_FEATURE_LEVEL l, ID3D11Device **d, ID3D11DeviceContext **c)
{
    D3D_FEATURE_LEVEL got = 0;
    HRESULT hr = dc_warp(0, &l, 1, d, c, &got);
    return hr;
}

static HRESULT v1(ID3D11Device *d, UINT w, UINT mips, UINT arr)
{
    D3D11_TEXTURE1D_DESC t = { w, mips, arr, DXGI_FORMAT_R8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    return ID3D11Device_CreateTexture1D(d, &t, NULL, NULL);
}
static HRESULT v2(ID3D11Device *d, UINT w, UINT h, UINT mips, UINT arr, UINT misc)
{
    D3D11_TEXTURE2D_DESC t = { w, h, mips, arr, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, misc };
    return ID3D11Device_CreateTexture2D(d, &t, NULL, NULL);
}
static HRESULT v3(ID3D11Device *d, UINT w, UINT h, UINT dp, UINT mips)
{
    D3D11_TEXTURE3D_DESC t = { w, h, dp, mips, DXGI_FORMAT_R8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    return ID3D11Device_CreateTexture3D(d, &t, NULL, NULL);
}

/* max1d == 0: 1D textures are not supported at the level (9_x) */
static void limits(D3D_FEATURE_LEVEL lvl, UINT max1d, UINT maxarr, UINT max3d, UINT maxcube, UINT maxarr1d)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(DC_FN());
    if (FAILED(lv_mk(lvl, &d, &c))) { t_skip("no WARP at feature level %#x", (unsigned)lvl); return; }
    if (max1d) {
        EXPECT_HR(v1(d, max1d, 1, 1), S_FALSE);
        EXPECT_HR(v1(d, max1d + 1, 1, 1), E_INVALIDARG);
        EXPECT_HR(v1(d, 8, 1, maxarr1d), S_FALSE);
        EXPECT_HR(v1(d, 8, 1, maxarr1d + 1), E_INVALIDARG);
    } else {
        EXPECT_HR(v1(d, 8, 1, 1), E_INVALIDARG);
        EXPECT_HR(v1(d, 1, 1, 1), E_INVALIDARG);
    }
    EXPECT_HR(v2(d, 4, 4, 1, maxarr, 0), S_FALSE);
    EXPECT_HR(v2(d, 4, 4, 1, maxarr + 1, 0), E_INVALIDARG);
    EXPECT_HR(v2(d, maxcube, maxcube, 1, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), S_FALSE);
    EXPECT_HR(v2(d, maxcube + 1, maxcube + 1, 1, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), E_INVALIDARG);
    EXPECT_HR(v3(d, max3d, 1, 1, 1), S_FALSE);
    EXPECT_HR(v3(d, max3d + 1, 1, 1, 1), E_INVALIDARG);
    EXPECT_HR(v3(d, 1, max3d, 1, 1), S_FALSE);
    EXPECT_HR(v3(d, 1, max3d + 1, 1, 1), E_INVALIDARG);
    EXPECT_HR(v3(d, 1, 1, max3d, 1), S_FALSE);
    EXPECT_HR(v3(d, 1, 1, max3d + 1, 1), E_INVALIDARG);
    /* mip counts: the full chain is accepted, one more level is not */
    EXPECT_HR(v2(d, 16, 16, 5, 1, 0), S_FALSE);
    EXPECT_HR(v2(d, 16, 16, 6, 1, 0), E_INVALIDARG);
    EXPECT_HR(v3(d, 8, 8, 8, 4), S_FALSE);
    EXPECT_HR(v3(d, 8, 8, 8, 5), E_INVALIDARG);
    DC_REL(d, c);
}
/* lvl, 1D width, 2D array, 3D dim, cube side, 1D array.  (9_x: array textures are not supported, so ArraySize 1 is the maximum.) */
T(limits_9_1) { limits(D3D_FEATURE_LEVEL_9_1, 0, 1, 256, 512, 1); }
T(limits_9_2) { limits(D3D_FEATURE_LEVEL_9_2, 0, 1, 256, 512, 1); }
T(limits_9_3) { limits(D3D_FEATURE_LEVEL_9_3, 0, 1, 256, 4096, 1); }
T(limits_10_0) { limits(D3D_FEATURE_LEVEL_10_0, 8192, 512, 2048, 8192, 512); }
T(limits_10_1) { limits(D3D_FEATURE_LEVEL_10_1, 8192, 512, 2048, 8192, 512); }
T(limits_11_0) { limits(D3D_FEATURE_LEVEL_11_0, 16384, 2048, 2048, 16384, 2048); }
T(limits_11_1) { limits(D3D_FEATURE_LEVEL_11_1, 16384, 2048, 2048, 16384, 2048); }

/* Cube arrays (TextureCubeArray, 12 faces) need 10_1. */
/* Without cube-array support (below 10_1) ArraySize must be exactly 6; from 10_1 any size >= 6 is accepted (7 included). */
static void cube_array(D3D_FEATURE_LEVEL lvl, HRESULT want)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(DC_FN());
    if (FAILED(lv_mk(lvl, &d, &c))) { t_skip("no WARP at feature level %#x", (unsigned)lvl); return; }
    EXPECT_HR(v2(d, 4, 4, 1, 6, D3D11_RESOURCE_MISC_TEXTURECUBE), S_FALSE);
    EXPECT_HR(v2(d, 4, 4, 1, 12, D3D11_RESOURCE_MISC_TEXTURECUBE), want);
    EXPECT_HR(v2(d, 4, 4, 1, 7, D3D11_RESOURCE_MISC_TEXTURECUBE), want);
    EXPECT_HR(v2(d, 4, 4, 1, 5, D3D11_RESOURCE_MISC_TEXTURECUBE), E_INVALIDARG);
    DC_REL(d, c);
}
T(cube_array_9_3) { cube_array(D3D_FEATURE_LEVEL_9_3, E_INVALIDARG); }
T(cube_array_10_0) { cube_array(D3D_FEATURE_LEVEL_10_0, E_INVALIDARG); }
T(cube_array_10_1) { cube_array(D3D_FEATURE_LEVEL_10_1, S_FALSE); }
T(cube_array_11_0) { cube_array(D3D_FEATURE_LEVEL_11_0, S_FALSE); }

/* ---- CheckFeatureSupport values by feature level ---------------------------------------------------------------------- */
static void caps(D3D_FEATURE_LEVEL lvl, int ge10, int ge11)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(DC_FN());
    if (FAILED(lv_mk(lvl, &d, &c))) { t_skip("no WARP at feature level %#x", (unsigned)lvl); return; }
    {   D3D11_FEATURE_DATA_THREADING th = { 7, 7 };
        EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_THREADING, &th, sizeof th), S_OK);
        EXPECT_EQ(th.DriverConcurrentCreates, 1); EXPECT_EQ(th.DriverCommandLists, 0); }
    {   D3D11_FEATURE_DATA_DOUBLES db; memset(&db, 0xcc, sizeof db);
        EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_DOUBLES, &db, sizeof db), S_OK);
        EXPECT_EQ(db.DoublePrecisionFloatShaderOps, ge11 ? 1 : 0); }
    {   D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hw; memset(&hw, 0xcc, sizeof hw);
        EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hw, sizeof hw), S_OK);
        EXPECT_EQ(hw.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x, ge10 ? 1 : 0); }
    {   D3D11_FEATURE_DATA_D3D11_OPTIONS o; memset(&o, 0xcc, sizeof o);
        EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof o), S_OK);
        EXPECT_EQ(o.OutputMergerLogicOp, 1); EXPECT_EQ(o.UAVOnlyRenderingForcedSampleCount, ge11 ? 1 : 0);
        EXPECT_EQ(o.DiscardAPIsSeenByDriver, 1); EXPECT_EQ(o.FlagsForUpdateAndCopySeenByDriver, 1);
        EXPECT_EQ(o.ClearView, 1); EXPECT_EQ(o.CopyWithOverlap, 1); EXPECT_EQ(o.ConstantBufferPartialUpdate, 1);
        EXPECT_EQ(o.ConstantBufferOffsetting, 1); EXPECT_EQ(o.MapNoOverwriteOnDynamicConstantBuffer, 1); }
    {   D3D11_FEATURE_DATA_ARCHITECTURE_INFO a; memset(&a, 0xcc, sizeof a);
        EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_ARCHITECTURE_INFO, &a, sizeof a), S_OK);
        EXPECT_EQ(a.TileBasedDeferredRenderer, 0); }
    {   D3D11_FEATURE_DATA_D3D9_OPTIONS o9; memset(&o9, 0xcc, sizeof o9);
        EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D9_OPTIONS, &o9, sizeof o9), S_OK);
        EXPECT_EQ(o9.FullNonPow2TextureSupport, 1); }
    {   D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT sh; memset(&sh, 0xcc, sizeof sh);
        EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D9_SHADOW_SUPPORT, &sh, sizeof sh), S_OK);
        EXPECT_EQ(sh.SupportsDepthAsTextureWithLessEqualComparisonFilter, 1); }
    {   D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT mp; memset(&mp, 0xcc, sizeof mp);
        EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &mp, sizeof mp), S_OK);
        EXPECT_EQ(mp.PixelShaderMinPrecision, 3); EXPECT_EQ(mp.AllOtherShaderStagesMinPrecision, 3); }
    DC_REL(d, c);
}
T(caps_values_9_1) { caps(D3D_FEATURE_LEVEL_9_1, 0, 0); }
T(caps_values_9_3) { caps(D3D_FEATURE_LEVEL_9_3, 0, 0); }
T(caps_values_10_0) { caps(D3D_FEATURE_LEVEL_10_0, 1, 0); }
T(caps_values_10_1) { caps(D3D_FEATURE_LEVEL_10_1, 1, 0); }
T(caps_values_11_0) { caps(D3D_FEATURE_LEVEL_11_0, 1, 1); }
T(caps_values_11_1) { caps(D3D_FEATURE_LEVEL_11_1, 1, 1); }

/* D3D11_OPTIONS2/OPTIONS1 depend on the feature level the device was created at (Win10 22H2 WARP). */
static void caps2(D3D_FEATURE_LEVEL lvl, UINT tiled1, UINT tiled2, UINT rov, UINT cons)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(DC_FN());
    if (FAILED(lv_mk(lvl, &d, &c))) { t_skip("no WARP at feature level %#x", (unsigned)lvl); return; }
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o1; D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2;
    memset(&o1, 0xcc, sizeof o1); memset(&o2, 0xcc, sizeof o2);
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS1, &o1, sizeof o1), S_OK);
    EXPECT_EQ(o1.TiledResourcesTier, tiled1); EXPECT_EQ(o1.MinMaxFiltering, 1); EXPECT_EQ(o1.ClearViewAlsoSupportsDepthOnlyFormats, 1);
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof o2), S_OK);
    EXPECT_EQ(o2.TiledResourcesTier, tiled2); EXPECT_EQ(o2.ROVsSupported, rov); EXPECT_EQ(o2.PSSpecifiedStencilRefSupported, rov);
    EXPECT_EQ(o2.ConservativeRasterizationTier, cons);
    EXPECT_EQ(o2.MapOnDefaultTextures, 1); EXPECT_EQ(o2.StandardSwizzle, 1); EXPECT_EQ(o2.UnifiedMemoryArchitecture, 1);
    DC_REL(d, c);
}
T(caps_options12_10_0) { caps2(D3D_FEATURE_LEVEL_10_0, 0, 0, 0, 0); }
T(caps_options12_11_0) { caps2(D3D_FEATURE_LEVEL_11_0, 1, 1, 0, 0); }
T(caps_options12_11_1) { caps2(D3D_FEATURE_LEVEL_11_1, 2, 3, 1, 3); }
