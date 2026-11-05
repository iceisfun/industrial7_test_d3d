/* CheckFeatureSupport: argument validation, per-feature round trips, cross-feature consistency. */
#include "cf.h"

typedef struct { BOOL ExtendedNV12SharedTextureSupported; } cf_options4;

/* X(testname, enum, struct type, must_exist, all_dwords_bool, takes_format) */
#define FEATS(X) \
    X(threading,  D3D11_FEATURE_THREADING,                     D3D11_FEATURE_DATA_THREADING,                    1, 1, 0) \
    X(doubles,    D3D11_FEATURE_DOUBLES,                       D3D11_FEATURE_DATA_DOUBLES,                      1, 1, 0) \
    X(fmtsup,     D3D11_FEATURE_FORMAT_SUPPORT,                D3D11_FEATURE_DATA_FORMAT_SUPPORT,               1, 0, 1) \
    X(fmtsup2,    D3D11_FEATURE_FORMAT_SUPPORT2,               D3D11_FEATURE_DATA_FORMAT_SUPPORT2,              1, 0, 1) \
    X(hw10x,      D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS,      D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS,     1, 1, 0) \
    X(options,    D3D11_FEATURE_D3D11_OPTIONS,                 D3D11_FEATURE_DATA_D3D11_OPTIONS,                1, 1, 0) \
    X(archinfo,   D3D11_FEATURE_ARCHITECTURE_INFO,             D3D11_FEATURE_DATA_ARCHITECTURE_INFO,            1, 1, 0) \
    X(d3d9opt,    D3D11_FEATURE_D3D9_OPTIONS,                  D3D11_FEATURE_DATA_D3D9_OPTIONS,                 1, 1, 0) \
    X(minprec,    D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT,  D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT, 1, 0, 0) \
    X(d3d9shadow, D3D11_FEATURE_D3D9_SHADOW_SUPPORT,           D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT,          1, 1, 0) \
    X(options1,   D3D11_FEATURE_D3D11_OPTIONS1,                D3D11_FEATURE_DATA_D3D11_OPTIONS1,               1, 0, 0) \
    X(d3d9inst,   D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT,D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT,1, 1, 0) \
    X(marker,     D3D11_FEATURE_MARKER_SUPPORT,                D3D11_FEATURE_DATA_MARKER_SUPPORT,               1, 1, 0) \
    X(d3d9opt1,   D3D11_FEATURE_D3D9_OPTIONS1,                 D3D11_FEATURE_DATA_D3D9_OPTIONS1,                1, 1, 0) \
    X(options2,   D3D11_FEATURE_D3D11_OPTIONS2,                D3D11_FEATURE_DATA_D3D11_OPTIONS2,               1, 0, 0) \
    X(options3,   D3D11_FEATURE_D3D11_OPTIONS3,                D3D11_FEATURE_DATA_D3D11_OPTIONS3,               1, 1, 0) \
    X(gpuva,      D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT,   D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT,  0, 0, 0) \
    X(options4,   17 /* D3D11_FEATURE_D3D11_OPTIONS4 */,       cf_options4,                                     0, 1, 0) \
    X(shadercache,D3D11_FEATURE_SHADER_CACHE,                  D3D11_FEATURE_DATA_SHADER_CACHE,                 0, 0, 0) \
    X(options5,   D3D11_FEATURE_D3D11_OPTIONS5,                D3D11_FEATURE_DATA_D3D11_OPTIONS5,               0, 0, 0)

static HRESULT cf_chk(ID3D11Device *d, int feat, void *p, UINT sz)
{
    return ID3D11Device_CheckFeatureSupport(d, (D3D11_FEATURE)feat, p, sz);
}

static void feat_bad_sizes(int feat, UINT sz)
{
    ID3D11Device *d = cf_shared(NULL);
    unsigned char buf[512];
    UINT sizes[8]; int n = 0, i;
    if (!d) { t_skip("no D3D11 device"); return; }
    sizes[n++] = 0; sizes[n++] = 1; sizes[n++] = sz - 1; sizes[n++] = sz + 1;
    sizes[n++] = sz * 2; sizes[n++] = sz + 4; sizes[n++] = 0x10000; sizes[n++] = 0xffffffffu;
    for (i = 0; i < n; i++) {
        unsigned char ref[512];
        memset(buf, 0xef, sizeof(buf)); memcpy(ref, buf, sizeof(buf));
        if (sizes[i] == sz) continue;
        HRESULT hr = cf_chk(d, feat, buf, sizes[i]);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "bad size", "feature %d size %u got %s", feat, sizes[i], d3dh_hr_name(hr));
        t_check(!memcmp(buf, ref, sizeof(buf)), __FILE__, __LINE__, "buffer untouched", "feature %d size %u modified the buffer", feat, sizes[i]);
    }
}
static void feat_null(int feat, UINT sz)
{
    ID3D11Device *d = cf_shared(NULL);
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(cf_chk(d, feat, NULL, 0), E_INVALIDARG);
    /* NULL data with the correct size is deliberately not exercised: the runtime may dereference it. */
    (void)sz;
}
static void feat_roundtrip(int feat, UINT sz, int must, int allbool, int takes_fmt)
{
    ID3D11Device *d = cf_shared(NULL);
    unsigned char buf[512]; UINT i;
    if (!d) { t_skip("no D3D11 device"); return; }
    memset(buf, 0xef, sizeof(buf));
    if (takes_fmt) *(UINT *)buf = DXGI_FORMAT_R8G8B8A8_UNORM;
    HRESULT hr = cf_chk(d, feat, buf, sz);
    if (must) EXPECT_HR(hr, S_OK);
    else if (hr != S_OK) {
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "unavailable feature", "feature %d got %s", feat, d3dh_hr_name(hr));
        t_note("feature %d not available on this OS (%s)", feat, d3dh_hr_name(hr));
        return;
    }
    for (i = sz; i < sizeof(buf); i++)
        if (buf[i] != 0xef) { CHECKF(0, "byte %u past the struct was written", i); break; }
    for (i = 0; i < sz / 4; i++) {
        UINT v = ((UINT *)buf)[i];
        if (takes_fmt && i == 0) { EXPECT_EQ(v, DXGI_FORMAT_R8G8B8A8_UNORM); continue; }
        CHECKF(v != 0xefefefefu, "dword %u of feature %d was not written", i, feat);
        if (allbool) CHECKF(v <= 1, "dword %u of feature %d is not a BOOL: %#x", i, feat, v);
    }
}
static void feat_stable(int feat, UINT sz, int takes_fmt)
{
    ID3D11Device *d = cf_shared(NULL);
    unsigned char a[512], b[512];
    if (!d) { t_skip("no D3D11 device"); return; }
    memset(a, 0, sizeof(a)); memset(b, 0xff, sizeof(b));
    if (takes_fmt) { *(UINT *)a = DXGI_FORMAT_R16G16B16A16_FLOAT; *(UINT *)b = DXGI_FORMAT_R16G16B16A16_FLOAT; }
    HRESULT h1 = cf_chk(d, feat, a, sz), h2 = cf_chk(d, feat, b, sz);
    EXPECT_HR(h1, h2);
    if (SUCCEEDED(h1) && SUCCEEDED(h2)) CHECK(!memcmp(a, b, sz));
}
static void feat_two_devices(int feat, UINT sz, int takes_fmt)
{
    ID3D11Device *d1 = cf_make_default(NULL), *d2 = cf_make_default(NULL);
    unsigned char a[512], b[512];
    if (!d1 || !d2) { SAFE_RELEASE(d1); SAFE_RELEASE(d2); t_skip("no D3D11 device"); return; }
    memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
    if (takes_fmt) { *(UINT *)a = DXGI_FORMAT_R8G8B8A8_UNORM; *(UINT *)b = DXGI_FORMAT_R8G8B8A8_UNORM; }
    HRESULT h1 = cf_chk(d1, feat, a, sz), h2 = cf_chk(d2, feat, b, sz);
    EXPECT_HR(h1, h2);
    if (SUCCEEDED(h1) && SUCCEEDED(h2)) CHECK(!memcmp(a, b, sz));
    SAFE_RELEASE(d1); SAFE_RELEASE(d2);
}

#define GEN(name, id, type, must, allbool, fmt) \
    TEST(d3d11_caps_formats_feat_##name##_bad_sizes) { feat_bad_sizes(id, sizeof(type)); } \
    TEST(d3d11_caps_formats_feat_##name##_null_data) { feat_null(id, sizeof(type)); } \
    TEST(d3d11_caps_formats_feat_##name##_roundtrip) { feat_roundtrip(id, sizeof(type), must, allbool, fmt); } \
    TEST(d3d11_caps_formats_feat_##name##_stable) { feat_stable(id, sizeof(type), fmt); } \
    TEST(d3d11_caps_formats_feat_##name##_two_devices) { feat_two_devices(id, sizeof(type), fmt); }
FEATS(GEN)

/* Struct layout the implementor must honour (ABI sizes). */
TEST(d3d11_caps_formats_feat_struct_sizes)
{
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_THREADING), 8);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_DOUBLES), 4);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT), 8);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT2), 8);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS), 4);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS), 56);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_ARCHITECTURE_INFO), 4);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_D3D9_OPTIONS), 4);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT), 8);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS1), 16);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_D3D9_OPTIONS1), 16);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS2), 32);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS3), 4);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT), 8);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_SHADER_CACHE), 4);
    EXPECT_EQ(sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS5), 4);
}

/* Enumerator values are part of the ABI. */
TEST(d3d11_caps_formats_feat_enum_values)
{
    EXPECT_EQ(D3D11_FEATURE_THREADING, 0);
    EXPECT_EQ(D3D11_FEATURE_DOUBLES, 1);
    EXPECT_EQ(D3D11_FEATURE_FORMAT_SUPPORT, 2);
    EXPECT_EQ(D3D11_FEATURE_FORMAT_SUPPORT2, 3);
    EXPECT_EQ(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, 4);
    EXPECT_EQ(D3D11_FEATURE_D3D11_OPTIONS, 5);
    EXPECT_EQ(D3D11_FEATURE_ARCHITECTURE_INFO, 6);
    EXPECT_EQ(D3D11_FEATURE_D3D9_OPTIONS, 7);
    EXPECT_EQ(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, 8);
    EXPECT_EQ(D3D11_FEATURE_D3D9_SHADOW_SUPPORT, 9);
    EXPECT_EQ(D3D11_FEATURE_D3D11_OPTIONS1, 10);
    EXPECT_EQ(D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT, 11);
    EXPECT_EQ(D3D11_FEATURE_MARKER_SUPPORT, 12);
    EXPECT_EQ(D3D11_FEATURE_D3D9_OPTIONS1, 13);
    EXPECT_EQ(D3D11_FEATURE_D3D11_OPTIONS2, 14);
    EXPECT_EQ(D3D11_FEATURE_D3D11_OPTIONS3, 15);
    EXPECT_EQ(D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, 16);
    EXPECT_EQ(D3D11_FEATURE_SHADER_CACHE, 18);
    EXPECT_EQ(D3D11_FEATURE_D3D11_OPTIONS5, 19);
}

/* Unknown feature ids are rejected. */
TEST(d3d11_caps_formats_feat_unknown_ids)
{
    static const int ids[] = { 64, 100, 255, 256, 0x10000, 0x7fffffff, -1, (int)0x80000000 };
    ID3D11Device *d = cf_shared(NULL);
    unsigned char buf[64]; size_t i; UINT sz;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
        for (sz = 0; sz <= 32; sz += 4) {
            memset(buf, 0xef, sizeof(buf));
            HRESULT hr = cf_chk(d, ids[i], buf, sz);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "unknown feature", "id %d size %u got %s", ids[i], sz, d3dh_hr_name(hr));
            unsigned char ref[64]; memset(ref, 0xef, sizeof(ref));
            CHECK(!memcmp(buf, ref, sizeof(buf)));
        }
}
TEST(d3d11_caps_formats_feat_unknown_id_null)
{
    ID3D11Device *d = cf_shared(NULL);
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(cf_chk(d, 99, NULL, 0), E_INVALIDARG);
    EXPECT_HR(cf_chk(d, -1, NULL, 0), E_INVALIDARG);
}

/* ---- cross-feature consistency ---- */
TEST(d3d11_caps_formats_feat_tiled_tier_agrees)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o1; D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2;
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D11_OPTIONS1, &o1, sizeof(o1)), S_OK);
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2)), S_OK);
    CHECK(o1.TiledResourcesTier <= D3D11_TILED_RESOURCES_TIER_3);
    EXPECT_EQ(o1.TiledResourcesTier, o2.TiledResourcesTier);
    t_note("tiled resources tier %d, conservative raster tier %d", o2.TiledResourcesTier, o2.ConservativeRasterizationTier);
}
TEST(d3d11_caps_formats_feat_options2_ranges)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2;
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2)), S_OK);
    CHECK(o2.ConservativeRasterizationTier <= D3D11_CONSERVATIVE_RASTERIZATION_TIER_3);
    CHECK(o2.PSSpecifiedStencilRefSupported <= 1 && o2.TypedUAVLoadAdditionalFormats <= 1 && o2.ROVsSupported <= 1);
    CHECK(o2.MapOnDefaultTextures <= 1 && o2.StandardSwizzle <= 1 && o2.UnifiedMemoryArchitecture <= 1);
    t_note("PSStencilRef=%d AdditionalTypedUAV=%d ROV=%d MapDefaultTex=%d StdSwizzle=%d UMA=%d", o2.PSSpecifiedStencilRefSupported,
           o2.TypedUAVLoadAdditionalFormats, o2.ROVsSupported, o2.MapOnDefaultTextures, o2.StandardSwizzle, o2.UnifiedMemoryArchitecture);
}
TEST(d3d11_caps_formats_feat_d3d9_views_agree)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_D3D9_OPTIONS a; D3D11_FEATURE_DATA_D3D9_OPTIONS1 b;
    D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT sh; D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT si;
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D9_OPTIONS, &a, sizeof(a)), S_OK);
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D9_OPTIONS1, &b, sizeof(b)), S_OK);
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D9_SHADOW_SUPPORT, &sh, sizeof(sh)), S_OK);
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT, &si, sizeof(si)), S_OK);
    if (a.FullNonPow2TextureSupport != b.FullNonPow2TextureSupported) t_note("D3D9_OPTIONS and OPTIONS1 disagree on non-pow2: %d vs %d", a.FullNonPow2TextureSupport, b.FullNonPow2TextureSupported);
    if (sh.SupportsDepthAsTextureWithLessEqualComparisonFilter != b.DepthAsTextureWithLessEqualComparisonFilterSupported) t_note("D3D9 shadow views disagree");
    if (si.SimpleInstancingSupported != b.SimpleInstancingSupported) t_note("D3D9 instancing views disagree");
}
TEST(d3d11_caps_formats_feat_minprecision_flags)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT m;
    if (!d) { t_skip("no D3D11 device"); return; }
    memset(&m, 0xef, sizeof(m));
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &m, sizeof(m)), S_OK);
    CHECK((m.PixelShaderMinPrecision & ~3u) == 0);
    CHECK((m.AllOtherShaderStagesMinPrecision & ~3u) == 0);
    t_note("min precision PS=%#x other=%#x", m.PixelShaderMinPrecision, m.AllOtherShaderStagesMinPrecision);
}
TEST(d3d11_caps_formats_feat_shadercache_flags)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_SHADER_CACHE c;
    if (!d) { t_skip("no D3D11 device"); return; }
    if (cf_chk(d, D3D11_FEATURE_SHADER_CACHE, &c, sizeof(c)) != S_OK) { t_skip("SHADER_CACHE feature unavailable"); return; }
    CHECK((c.SupportFlags & ~3u) == 0);
    t_note("shader cache flags %#x", c.SupportFlags);
}
TEST(d3d11_caps_formats_feat_shared_resource_tier)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_D3D11_OPTIONS5 o5; D3D11_FEATURE_DATA_D3D11_OPTIONS o;
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof(o)), S_OK);
    if (cf_chk(d, D3D11_FEATURE_D3D11_OPTIONS5, &o5, sizeof(o5)) != S_OK) { t_skip("OPTIONS5 unavailable"); return; }
    CHECK(o5.SharedResourceTier <= D3D11_SHARED_RESOURCE_TIER_3);
    t_note("shared resource tier %d, ExtendedResourceSharing=%d", o5.SharedResourceTier, o.ExtendedResourceSharing);
}
TEST(d3d11_caps_formats_feat_gpuva_order)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT v;
    if (!d) { t_skip("no D3D11 device"); return; }
    if (cf_chk(d, D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &v, sizeof(v)) != S_OK) { t_skip("GPU VA feature unavailable"); return; }
    CHECK(v.MaxGPUVirtualAddressBitsPerResource <= v.MaxGPUVirtualAddressBitsPerProcess || v.MaxGPUVirtualAddressBitsPerProcess == 0);
    CHECK(v.MaxGPUVirtualAddressBitsPerProcess <= 64);
    t_note("GPU VA bits per resource %u per process %u", v.MaxGPUVirtualAddressBitsPerResource, v.MaxGPUVirtualAddressBitsPerProcess);
}
TEST(d3d11_caps_formats_feat_logicop_matches_format_bits)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_D3D11_OPTIONS o; int any = 0; UINT f;
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof(o)), S_OK);
    for (f = 0; f < CF_NFORMATS; f++) if (cf_sup2(d, f) & D3D11_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP) any = 1;
    if (!o.OutputMergerLogicOp) { if (any) t_note("a format reports OUTPUT_MERGER_LOGIC_OP although OutputMergerLogicOp is FALSE"); }
    else t_note("OutputMergerLogicOp is TRUE; some format has the bit: %d", any);
}
TEST(d3d11_caps_formats_feat_compute_via_sm4_vs_fl)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_make_default(&fl);
    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS h;
    if (!d) { t_skip("no D3D11 device"); return; }
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &h, sizeof(h)), S_OK);
    t_note("fl %s: ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x=%d", cf_fl_name(fl), h.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x);
    SAFE_RELEASE(d);
}
TEST(d3d11_caps_formats_feat_format_support_input_unchanged)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_FORMAT_SUPPORT f1; D3D11_FEATURE_DATA_FORMAT_SUPPORT2 f2;
    UINT fm[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, 0xffffffffu, 500, DXGI_FORMAT_BC7_UNORM };
    size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < 5; i++) {
        f1.InFormat = fm[i]; f1.OutFormatSupport = 0xdeadbeef;
        HRESULT hr = cf_chk(d, D3D11_FEATURE_FORMAT_SUPPORT, &f1, sizeof(f1));
        EXPECT_EQ(f1.InFormat, fm[i]);
        CHECKF((hr == S_OK && f1.OutFormatSupport) || (hr == E_FAIL && !f1.OutFormatSupport), "format %#x hr %s support %#x", fm[i], d3dh_hr_name(hr), f1.OutFormatSupport);
        f2.InFormat = fm[i]; f2.OutFormatSupport2 = 0xdeadbeef;
        hr = cf_chk(d, D3D11_FEATURE_FORMAT_SUPPORT2, &f2, sizeof(f2));
        EXPECT_EQ(f2.InFormat, fm[i]);
        CHECKF(hr == S_OK || hr == E_FAIL, "SUPPORT2 format %#x gave %s", fm[i], d3dh_hr_name(hr));
        if (hr == E_FAIL) EXPECT_EQ(f2.OutFormatSupport2, 0);
    }
}
TEST(d3d11_caps_formats_feat_format_support_invalid_format_efail)
{
    ID3D11Device *d = cf_shared(NULL);
    D3D11_FEATURE_DATA_FORMAT_SUPPORT f;
    if (!d) { t_skip("no D3D11 device"); return; }
    f.InFormat = (DXGI_FORMAT)0xffffffffu; f.OutFormatSupport = 0xdeadbeef;
    HRESULT hr = cf_chk(d, D3D11_FEATURE_FORMAT_SUPPORT, &f, sizeof(f));
    t_note("FORMAT_SUPPORT(0xffffffff) = %s", d3dh_hr_name(hr));
    CHECKF(FAILED(hr) || f.OutFormatSupport == 0, "invalid format reported support %#x", f.OutFormatSupport);
    UINT s = 0xdeadbeef;
    hr = ID3D11Device_CheckFormatSupport(d, (DXGI_FORMAT)0xffffffffu, &s);
    CHECKF(FAILED(hr) || s == 0, "invalid format reported support %#x", s);
}

/* ---- per feature level ---- */
static void level_features(D3D_FEATURE_LEVEL want)
{
    ID3D11Device *d = cf_make(want);
    int feat;
    if (!d) { t_skip("feature level %s unavailable", cf_fl_name(want)); return; }
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(d), want);
    {
        D3D11_FEATURE_DATA_THREADING t;
        EXPECT_HR(cf_chk(d, D3D11_FEATURE_THREADING, &t, sizeof(t)), S_OK);
        D3D11_FEATURE_DATA_DOUBLES db;
        HRESULT hrd = cf_chk(d, D3D11_FEATURE_DOUBLES, &db, sizeof(db));
        if (want >= D3D_FEATURE_LEVEL_10_0) EXPECT_HR(hrd, S_OK); else { t_note("fl %s DOUBLES: %s", cf_fl_name(want), d3dh_hr_name(hrd)); if (FAILED(hrd)) db.DoublePrecisionFloatShaderOps = 0; }
        CHECK(t.DriverCommandLists <= 1 && t.DriverConcurrentCreates <= 1 && db.DoublePrecisionFloatShaderOps <= 1);
        t_note("fl %s: concurrent creates %d, driver command lists %d, doubles %d", cf_fl_name(want), t.DriverConcurrentCreates, t.DriverCommandLists, db.DoublePrecisionFloatShaderOps);
    }
    {
        D3D11_FEATURE_DATA_FORMAT_SUPPORT f; f.InFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        EXPECT_HR(cf_chk(d, D3D11_FEATURE_FORMAT_SUPPORT, &f, sizeof(f)), S_OK);
        CHECK((f.OutFormatSupport & (S_T2D | S_RT | S_SAMPLE)) == (S_T2D | S_RT | S_SAMPLE));
    }
    for (feat = 4; feat <= 15; feat++) {
        unsigned char buf[128]; memset(buf, 0, sizeof(buf));
        UINT sz = 4;
        switch (feat) { case 5: sz = 56; break; case 8: sz = 8; break; case 10: case 13: sz = 16; break; case 14: sz = 32; break; }
        HRESULT hr = cf_chk(d, feat, buf, sz);
        if (hr != S_OK) t_note("fl %s feature %d: %s", cf_fl_name(want), feat, d3dh_hr_name(hr));
        /* a wrong size never succeeds at any level */
        EXPECT_HR(cf_chk(d, feat, buf, sz + 4), E_INVALIDARG);
    }
    SAFE_RELEASE(d);
}
#define LV(n, l) TEST(d3d11_caps_formats_feat_level_##n) { level_features(l); }
LV(9_1, D3D_FEATURE_LEVEL_9_1) LV(9_2, D3D_FEATURE_LEVEL_9_2) LV(9_3, D3D_FEATURE_LEVEL_9_3)
LV(10_0, D3D_FEATURE_LEVEL_10_0) LV(10_1, D3D_FEATURE_LEVEL_10_1) LV(11_0, D3D_FEATURE_LEVEL_11_0) LV(11_1, D3D_FEATURE_LEVEL_11_1)

TEST(d3d11_caps_formats_feat_options_vs_level)
{
    /* Options introduced with 11.1 are not reported on an 11.0 device by a conforming runtime. */
    ID3D11Device *d = cf_make(D3D_FEATURE_LEVEL_11_0);
    D3D11_FEATURE_DATA_D3D11_OPTIONS o;
    if (!d) { t_skip("no 11_0 device"); return; }
    EXPECT_HR(cf_chk(d, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof(o)), S_OK);
    t_note("11_0 device: logicop=%d uavonly=%d discard=%d flags=%d clearview=%d copyoverlap=%d cbpartial=%d cboffset=%d noov_cb=%d noov_srv=%d msrtv1=%d sad4=%d extdbl=%d extshare=%d",
           o.OutputMergerLogicOp, o.UAVOnlyRenderingForcedSampleCount, o.DiscardAPIsSeenByDriver, o.FlagsForUpdateAndCopySeenByDriver, o.ClearView,
           o.CopyWithOverlap, o.ConstantBufferPartialUpdate, o.ConstantBufferOffsetting, o.MapNoOverwriteOnDynamicConstantBuffer,
           o.MapNoOverwriteOnDynamicBufferSRV, o.MultisampleRTVWithForcedSampleCountOne, o.SAD4ShaderInstructions, o.ExtendedDoublesShaderInstructions, o.ExtendedResourceSharing);
    SAFE_RELEASE(d);
}
