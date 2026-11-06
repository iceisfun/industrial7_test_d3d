/* CheckFormatSupport / FORMAT_SUPPORT2 for every DXGI_FORMAT id: per-format rule sets. */
#include "cf.h"

#define IMPLIES(a, b) (!(a) || (b))

static void fmt_rules(UINT f)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    UINT s, s2; HRESULT hr;
    if (!d) { t_skip("no D3D11 device"); return; }
    hr = cf_support(d, f, &s);
    CHECKF(SUCCEEDED(hr) || !s, "failed query left support %#x (%s)", s, d3dh_hr_name(hr));
    if (hr == S_OK && !s) t_note("%s: S_OK with empty support word", cf_fmt_name(f));
    if (hr != S_OK) s = 0;
    /* repeated query is stable */
    { UINT again; HRESULT h2 = cf_support(d, f, &again); EXPECT_HR(h2, hr); if (SUCCEEDED(h2)) EXPECT_EQ(again, s); }
    if (f == DXGI_FORMAT_NV12) EXPECT_EQ(s & 0xc0000000u, 0xc0000000u);   /* Windows 10 WARP: NV12 alone sets the two undocumented top bits */
    else CHECKF((s & 0x80000000u) == 0, "undefined top bit set: %#x", s);
    /* feature-query path returns exactly the same word */
    { D3D11_FEATURE_DATA_FORMAT_SUPPORT fd; fd.InFormat = (DXGI_FORMAT)f; fd.OutFormatSupport = 0xdeadbeef;
      HRESULT h3 = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_FORMAT_SUPPORT, &fd, sizeof(fd));
      EXPECT_HR(h3, hr); EXPECT_EQ(fd.OutFormatSupport, s); }
    /* implications between bits */
    CHECK(IMPLIES(s & S_RT, s & S_T2D));
    CHECK(IMPLIES(s & S_BLEND, s & S_RT));
    CHECK(IMPLIES(s & S_DS, s & S_T2D));
    CHECK(IMPLIES(s & S_CUBE, s & S_T2D));
    CHECK(IMPLIES(s & S_AUTOGEN, s & S_T2D));
    CHECK(IMPLIES(s & S_MSRT, s & (S_RT | S_DS)));
    CHECK(IMPLIES(s & S_MSRESOLVE, s & S_T2D));
    CHECK(IMPLIES(s & S_DS, !(s & S_RT)));
    if (fl >= D3D_FEATURE_LEVEL_10_0) {
        CHECK(IMPLIES(s & S_DS, !(s & S_BLEND)));
    }
    /* SUPPORT2 */
    s2 = 0xdeadbeef;
    { D3D11_FEATURE_DATA_FORMAT_SUPPORT2 fd; fd.InFormat = (DXGI_FORMAT)f; fd.OutFormatSupport2 = 0xdeadbeef;
      HRESULT h4 = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_FORMAT_SUPPORT2, &fd, sizeof(fd));
      CHECKF(h4 == S_OK || h4 == E_FAIL, "SUPPORT2 hr %s", d3dh_hr_name(h4));
      if (SUCCEEDED(h4)) s2 = fd.OutFormatSupport2; else { EXPECT_EQ(fd.OutFormatSupport2, 0); s2 = 0; } }
    if (s2 & ~0x47ffu) t_note("format %s: unknown SUPPORT2 bits %#x", cf_fmt_name(f), s2 & ~0x47ffu);
    if (s2 & (D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE |
              D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD | D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE))
        CHECKF(s & S_TUAV, "UAV SUPPORT2 bits %#x without TYPED_UNORDERED_ACCESS_VIEW (support %#x)", s2, s);
    /* class rules */
    if (cf_is_typeless(f)) {
        CHECKF(!(s & (S_RT | S_BLEND | S_DISPLAY)), "typeless format with view-only bits: %#x", s);
        if (s & S_DS) t_note("%s: typeless reports DEPTH_STENCIL", cf_fmt_name(f));
        if (s & (S_SAMPLE | S_LOAD)) t_note("%s: typeless reports sample/load bits (%#x)", cf_fmt_name(f), s);
    }
    if (cf_is_bc(f) && fl >= D3D_FEATURE_LEVEL_10_0 && !(f == DXGI_FORMAT_BC6H_TYPELESS || f == DXGI_FORMAT_BC6H_UF16 || f == DXGI_FORMAT_BC6H_SF16 ||
                                                       f == DXGI_FORMAT_BC7_TYPELESS || f == DXGI_FORMAT_BC7_UNORM || f == DXGI_FORMAT_BC7_UNORM_SRGB)) {
        CHECKF(!(s & (S_RT | S_DS | S_BLEND | S_DISPLAY | S_TUAV | S_MSRT)), "BC format with render bits: %#x", s);
        if (!cf_is_typeless(f)) CHECKF((s & (S_T2D | S_SAMPLE | S_LOAD)) == (S_T2D | S_SAMPLE | S_LOAD), "BC format lacks texture bits: %#x", s);
    }
    if (cf_is_bc(f) && fl >= D3D_FEATURE_LEVEL_11_0) {
        CHECKF(!(s & (S_RT | S_DS | S_BLEND | S_DISPLAY | S_TUAV | S_MSRT)), "BC format with render bits: %#x", s);
        if (!cf_is_typeless(f)) CHECKF((s & (S_T2D | S_T3D | S_CUBE | S_SAMPLE | S_LOAD | S_MIP)) == (S_T2D | S_T3D | S_CUBE | S_SAMPLE | S_LOAD | S_MIP), "BC format lacks texture bits: %#x", s);
    }
    if (cf_is_depth(f) && fl >= D3D_FEATURE_LEVEL_10_0) {
        CHECKF(s & S_DS, "depth format lacks DEPTH_STENCIL: %#x", s);
        CHECKF(s & S_T2D, "depth format lacks TEXTURE2D: %#x", s);
        CHECKF(!(s & (S_RT | S_BLEND | S_DISPLAY)), "depth format with color bits: %#x", s);
        if (!(s & S_MSRT)) t_note("%s: depth format without MULTISAMPLE_RENDERTARGET", cf_fmt_name(f));
    }
    if (!cf_is_defined(f) && f != 0) {
        /* ids 116..129 are not formats */
        if (s) t_note("undefined format id %u reports support %#x", f, s);
    }
}
#define GEN_RULES(n) TEST(d3d11_caps_formats_fmt_##n##_rules) { fmt_rules(DXGI_FORMAT_##n); }
CF_FORMATS(GEN_RULES)

/* undefined ids inside 0..132 */
TEST(d3d11_caps_formats_fmt_undefined_ids_consistent)
{
    ID3D11Device *d = cf_shared(NULL); UINT f, s;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (f = 0; f < CF_NFORMATS; f++) {
        if (cf_is_defined(f)) continue;
        HRESULT hr = cf_support(d, f, &s);
        CHECKF(SUCCEEDED(hr) || !s, "id %u: hr %s support %#x", f, d3dh_hr_name(hr), s);
        if (hr == S_OK) t_note("undefined format id %u reports support %#x", f, s);
    }
}
TEST(d3d11_caps_formats_fmt_out_of_range_ids)
{
    static const UINT ids[] = { 133, 134, 150, 189, 190, 255, 256, 1000, 0x10000, 0x7fffffff, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    ID3D11Device *d = cf_shared(NULL); size_t i; UINT s;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        HRESULT hr = cf_support(d, ids[i], &s);
        CHECKF(SUCCEEDED(hr) || !s, "id %#x: hr %s support %#x", ids[i], d3dh_hr_name(hr), s);
        if (ids[i] >= 0x10000) { CHECKF(FAILED(hr) || !s, "id %#x reports support %#x", ids[i], s); t_note("id %#x -> %s", ids[i], d3dh_hr_name(hr)); }
    }
}
/* Support word agrees with real resource creation (positive direction is hard). */
static void fmt_create(UINT f)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *d = cf_shared(&fl);
    UINT s; int video = cf_is_video(f);
    if (!d) { t_skip("no D3D11 device"); return; }
    s = cf_sup(d, f);
    if (!s || cf_is_typeless(f) || f == 0) { t_skip("not a typed supported format"); return; }
    if (f == DXGI_FORMAT_X24_TYPELESS_G8_UINT || f == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT || f == DXGI_FORMAT_R1_UNORM) { t_skip("view-only format"); return; }
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *tex; HRESULT hr;
    memset(&td, 0, sizeof(td));
    td.Width = 16; td.Height = 16; td.MipLevels = 1; td.ArraySize = 1; td.Format = (DXGI_FORMAT)f;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    if ((s & (S_T2D | S_RT)) == (S_T2D | S_RT)) {
        td.BindFlags = D3D11_BIND_RENDER_TARGET; tex = NULL;
        hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &tex);
        if (video) { if (FAILED(hr)) t_note("%s: RT texture failed %s", cf_fmt_name(f), d3dh_hr_name(hr)); }
        else EXPECT_HR(hr, S_OK);
        if (tex) { D3D11_RENDER_TARGET_VIEW_DESC rd; ID3D11RenderTargetView *rtv = NULL; memset(&rd, 0, sizeof(rd));
            rd.Format = (DXGI_FORMAT)f; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            hr = ID3D11Device_CreateRenderTargetView(d, (ID3D11Resource *)tex, &rd, &rtv);
            if (!video) EXPECT_HR(hr, S_OK);
            SAFE_RELEASE(rtv); SAFE_RELEASE(tex); }
    }
    if ((s & (S_T2D | S_DS)) == (S_T2D | S_DS)) {
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL; tex = NULL;
        hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &tex);
        EXPECT_HR(hr, S_OK);
        if (tex) { ID3D11DepthStencilView *dsv = NULL; D3D11_DEPTH_STENCIL_VIEW_DESC dd; memset(&dd, 0, sizeof(dd));
            dd.Format = (DXGI_FORMAT)f; dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            EXPECT_HR(ID3D11Device_CreateDepthStencilView(d, (ID3D11Resource *)tex, &dd, &dsv), S_OK);
            SAFE_RELEASE(dsv); SAFE_RELEASE(tex); }
    }
    if ((s & (S_T2D | S_SAMPLE)) == (S_T2D | S_SAMPLE) && !(s & S_DS) && !video) {
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE; tex = NULL;
        hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &tex);
        EXPECT_HR(hr, S_OK);
        if (tex) { ID3D11ShaderResourceView *srv = NULL; D3D11_SHADER_RESOURCE_VIEW_DESC sd; memset(&sd, 0, sizeof(sd));
            sd.Format = (DXGI_FORMAT)f; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
            EXPECT_HR(ID3D11Device_CreateShaderResourceView(d, (ID3D11Resource *)tex, &sd, &srv), S_OK);
            SAFE_RELEASE(srv); SAFE_RELEASE(tex); }
    }
    if (fl >= D3D_FEATURE_LEVEL_11_0 && (s & (S_T2D | S_TUAV)) == (S_T2D | S_TUAV) && !video) {
        td.BindFlags = D3D11_BIND_UNORDERED_ACCESS; tex = NULL;
        hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &tex);
        EXPECT_HR(hr, S_OK);
        if (tex) { ID3D11UnorderedAccessView *uav = NULL; D3D11_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof(ud));
            ud.Format = (DXGI_FORMAT)f; ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(d, (ID3D11Resource *)tex, &ud, &uav), S_OK);
            SAFE_RELEASE(uav); SAFE_RELEASE(tex); }
    }
    if ((s & S_BUFFER) && !(s & S_DS) && fl >= D3D_FEATURE_LEVEL_10_0 && !video && !cf_is_bc(f)) {
        D3D11_BUFFER_DESC bd; ID3D11Buffer *buf = NULL; memset(&bd, 0, sizeof(bd));
        bd.ByteWidth = 256; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        EXPECT_HR(ID3D11Device_CreateBuffer(d, &bd, NULL, &buf), S_OK);
        if (buf) { ID3D11ShaderResourceView *srv = NULL; D3D11_SHADER_RESOURCE_VIEW_DESC sd; memset(&sd, 0, sizeof(sd));
            sd.Format = (DXGI_FORMAT)f; sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; sd.Buffer.FirstElement = 0; sd.Buffer.NumElements = 4;
            hr = ID3D11Device_CreateShaderResourceView(d, (ID3D11Resource *)buf, &sd, &srv);
            if (s & S_LOAD) EXPECT_HR(hr, S_OK);
            SAFE_RELEASE(srv); SAFE_RELEASE(buf); }
    }
}
#define GEN_CREATE(n) TEST(d3d11_caps_formats_fmt_##n##_create) { fmt_create(DXGI_FORMAT_##n); }
CF_FORMATS(GEN_CREATE)

/* Negative direction for classes where the rejection is documented. */
static void fmt_reject(UINT f, UINT bind, UINT need)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *tex = NULL;
    if (!d) { t_skip("no D3D11 device"); return; }
    memset(&td, 0, sizeof(td));
    td.Width = 16; td.Height = 16; td.MipLevels = 1; td.ArraySize = 1; td.Format = (DXGI_FORMAT)f; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = bind;
    if (cf_sup(d, f) & need) { t_skip("format supports the bind flag"); return; }
    EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &tex), E_INVALIDARG);
    SAFE_RELEASE(tex);
}
#define REJ(name, f, bind, need) TEST(d3d11_caps_formats_reject_##name) { fmt_reject(f, bind, need); }
REJ(bc1_rt,  DXGI_FORMAT_BC1_UNORM,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(bc2_rt,  DXGI_FORMAT_BC2_UNORM,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(bc3_rt,  DXGI_FORMAT_BC3_UNORM,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(bc4_rt,  DXGI_FORMAT_BC4_UNORM,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(bc5_rt,  DXGI_FORMAT_BC5_UNORM,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(bc6h_rt, DXGI_FORMAT_BC6H_UF16,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(bc7_rt,  DXGI_FORMAT_BC7_UNORM,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(bc1_ds,  DXGI_FORMAT_BC1_UNORM,  D3D11_BIND_DEPTH_STENCIL, S_DS)
REJ(bc7_ds,  DXGI_FORMAT_BC7_UNORM,  D3D11_BIND_DEPTH_STENCIL, S_DS)
REJ(bc1_uav, DXGI_FORMAT_BC1_UNORM,  D3D11_BIND_UNORDERED_ACCESS, S_TUAV)
REJ(d32_rt,  DXGI_FORMAT_D32_FLOAT,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(d16_rt,  DXGI_FORMAT_D16_UNORM,  D3D11_BIND_RENDER_TARGET, S_RT)
REJ(d24s8_rt,DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_RENDER_TARGET, S_RT)
REJ(rgba8_ds,DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_DEPTH_STENCIL, S_DS)
REJ(rgba16f_ds, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_DEPTH_STENCIL, S_DS)
REJ(unknown_rt, DXGI_FORMAT_UNKNOWN, D3D11_BIND_RENDER_TARGET, 0x80000000u)
