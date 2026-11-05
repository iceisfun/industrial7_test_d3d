/* D3D11 typeless families, depth/SRV aliasing, format support invariants, size limits and slot counts. */
#include "fpt.h"

#define F(n) DXGI_FORMAT_##n

static HRESULT mk_tex2d(ID3D11Device *dev, DXGI_FORMAT fmt, UINT w, UINT h, UINT bind, ID3D11Texture2D **out)
{
    D3D11_TEXTURE2D_DESC t = {0};
    t.Width = w; t.Height = h; t.MipLevels = 1; t.ArraySize = 1; t.Format = fmt; t.SampleDesc.Count = 1;
    t.Usage = D3D11_USAGE_DEFAULT; t.BindFlags = bind;
    *out = NULL;
    return ID3D11Device_CreateTexture2D(dev, &t, NULL, out);
}
static HRESULT mk_srv(ID3D11Device *dev, ID3D11Texture2D *tex, DXGI_FORMAT fmt)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC d = {0}; ID3D11ShaderResourceView *v = NULL; HRESULT hr;
    d.Format = fmt; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1;
    hr = ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, &d, &v);
    SAFE_RELEASE(v);
    return hr;
}
static HRESULT mk_dsv(ID3D11Device *dev, ID3D11Texture2D *tex, DXGI_FORMAT fmt)
{
    D3D11_DEPTH_STENCIL_VIEW_DESC d = {0}; ID3D11DepthStencilView *v = NULL; HRESULT hr;
    d.Format = fmt; d.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    hr = ID3D11Device_CreateDepthStencilView(dev, (ID3D11Resource *)tex, &d, &v);
    SAFE_RELEASE(v);
    return hr;
}
static HRESULT mk_rtv(ID3D11Device *dev, ID3D11Texture2D *tex, DXGI_FORMAT fmt)
{
    D3D11_RENDER_TARGET_VIEW_DESC d = {0}; ID3D11RenderTargetView *v = NULL; HRESULT hr;
    d.Format = fmt; d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)tex, &d, &v);
    SAFE_RELEASE(v);
    return hr;
}

/* ---- depth families ------------------------------------------------------ */
typedef struct { DXGI_FORMAT typeless, dsv; DXGI_FORMAT srv[2]; const char *name; } depth_fam;
static const depth_fam dfam[] = {
    { F(R32_TYPELESS),       F(D32_FLOAT),             { F(R32_FLOAT), F(UNKNOWN) }, "R32" },
    { F(R16_TYPELESS),       F(D16_UNORM),             { F(R16_UNORM), F(UNKNOWN) }, "R16" },
    { F(R24G8_TYPELESS),     F(D24_UNORM_S8_UINT),     { F(R24_UNORM_X8_TYPELESS), F(X24_TYPELESS_G8_UINT) }, "R24G8" },
    { F(R32G8X24_TYPELESS),  F(D32_FLOAT_S8X24_UINT),  { F(R32_FLOAT_X8X24_TYPELESS), F(X32_TYPELESS_G8X24_UINT) }, "R32G8X24" },
};
#define NDF (sizeof dfam / sizeof *dfam)
static const DXGI_FORMAT all_dsv[] = { F(D32_FLOAT), F(D16_UNORM), F(D24_UNORM_S8_UINT), F(D32_FLOAT_S8X24_UINT) };
static const DXGI_FORMAT all_srv[] = { F(R32_FLOAT), F(R16_UNORM), F(R24_UNORM_X8_TYPELESS), F(X24_TYPELESS_G8_UINT), F(R32_FLOAT_X8X24_TYPELESS), F(X32_TYPELESS_G8X24_UINT) };

static int in_srv(const depth_fam *d, DXGI_FORMAT f) { return d->srv[0] == f || d->srv[1] == f; }

TEST(fpt_d3d11_depth_typeless_create_with_ds_srv)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT i;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < NDF; i++) {
        ID3D11Texture2D *t;
        HRESULT hr = mk_tex2d(dev, dfam[i].typeless, 32, 32, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, &t);
        EXPECT_HR(hr, S_OK);
        SAFE_RELEASE(t);
        hr = mk_tex2d(dev, dfam[i].typeless, 32, 32, D3D11_BIND_DEPTH_STENCIL, &t);
        EXPECT_HR(hr, S_OK);
        SAFE_RELEASE(t);
    }
}
TEST(fpt_d3d11_depth_typed_cannot_be_shader_resource)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT i;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof all_dsv / sizeof *all_dsv; i++) {
        ID3D11Texture2D *t;
        HRESULT hr = mk_tex2d(dev, all_dsv[i], 32, 32, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, &t);
        EXPECT_HR(hr, E_INVALIDARG);
        SAFE_RELEASE(t);
        hr = mk_tex2d(dev, all_dsv[i], 32, 32, D3D11_BIND_DEPTH_STENCIL, &t);
        EXPECT_HR(hr, S_OK);
        SAFE_RELEASE(t);
    }
}
TEST(fpt_d3d11_depth_dsv_matrix)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT i, j;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < NDF; i++) {
        ID3D11Texture2D *t;
        if (FAILED(mk_tex2d(dev, dfam[i].typeless, 32, 32, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, &t))) { t_skip("depth family %s", dfam[i].name); continue; }
        for (j = 0; j < sizeof all_dsv / sizeof *all_dsv; j++) {
            HRESULT hr = mk_dsv(dev, t, all_dsv[j]);
            if (all_dsv[j] == dfam[i].dsv) EXPECT_HR(hr, S_OK);
            else CHECKF(FAILED(hr), "family %s: DSV format %u unexpectedly created (%s)", dfam[i].name, all_dsv[j], d3dh_hr_name(hr));
        }
        /* The SRV-only formats are not valid DSV formats. */
        for (j = 0; j < sizeof all_srv / sizeof *all_srv; j++) CHECKF(FAILED(mk_dsv(dev, t, all_srv[j])), "family %s: DSV with SRV format %u", dfam[i].name, all_srv[j]);
        SAFE_RELEASE(t);
    }
}
TEST(fpt_d3d11_depth_srv_matrix)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT i, j;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < NDF; i++) {
        ID3D11Texture2D *t;
        if (FAILED(mk_tex2d(dev, dfam[i].typeless, 32, 32, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, &t))) { t_skip("depth family %s", dfam[i].name); continue; }
        for (j = 0; j < sizeof all_srv / sizeof *all_srv; j++) {
            HRESULT hr = mk_srv(dev, t, all_srv[j]);
            if (in_srv(&dfam[i], all_srv[j])) EXPECT_HR(hr, S_OK);
            else CHECKF(FAILED(hr), "family %s: SRV format %u unexpectedly created (%s)", dfam[i].name, all_srv[j], d3dh_hr_name(hr));
        }
        /* Typed depth formats cannot be sampled. */
        for (j = 0; j < sizeof all_dsv / sizeof *all_dsv; j++) CHECKF(FAILED(mk_srv(dev, t, all_dsv[j])), "family %s: SRV with depth format %u", dfam[i].name, all_dsv[j]);
        SAFE_RELEASE(t);
    }
}
TEST(fpt_d3d11_depth_srv_unknown_format_inherits)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); ID3D11Texture2D *t; HRESULT hr;
    if (!dev) { t_skip("no D3D11 device"); return; }
    /* A typeless resource has no format to inherit, so UNKNOWN must be rejected for views on it. */
    hr = mk_tex2d(dev, F(R32_TYPELESS), 16, 16, D3D11_BIND_SHADER_RESOURCE, &t);
    EXPECT_HR(hr, S_OK);
    if (t) { hr = mk_srv(dev, t, F(UNKNOWN)); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(t); }
    /* A typed resource accepts UNKNOWN (inherit). */
    hr = mk_tex2d(dev, F(R32_FLOAT), 16, 16, D3D11_BIND_SHADER_RESOURCE, &t);
    EXPECT_HR(hr, S_OK);
    if (t) { hr = mk_srv(dev, t, F(UNKNOWN)); EXPECT_HR(hr, S_OK); SAFE_RELEASE(t); }
    hr = mk_tex2d(dev, F(D32_FLOAT), 16, 16, D3D11_BIND_DEPTH_STENCIL, &t);
    EXPECT_HR(hr, S_OK);
    if (t) { hr = mk_dsv(dev, t, F(UNKNOWN)); EXPECT_HR(hr, S_OK); SAFE_RELEASE(t); }
}

/* ---- colour typeless families ------------------------------------------------- */
typedef struct { DXGI_FORMAT typeless; DXGI_FORMAT m[5]; int rt; DXGI_FORMAT foreign; } color_fam;
static const color_fam cfam[] = {
    { F(R32G32B32A32_TYPELESS), { F(R32G32B32A32_FLOAT), F(R32G32B32A32_UINT), F(R32G32B32A32_SINT) }, 1, F(R8_UNORM) },
    { F(R32G32B32_TYPELESS),    { F(R32G32B32_FLOAT), F(R32G32B32_UINT), F(R32G32B32_SINT) }, 0, F(R8_UNORM) },
    { F(R16G16B16A16_TYPELESS), { F(R16G16B16A16_FLOAT), F(R16G16B16A16_UNORM), F(R16G16B16A16_UINT), F(R16G16B16A16_SNORM), F(R16G16B16A16_SINT) }, 1, F(R8_UNORM) },
    { F(R32G32_TYPELESS),       { F(R32G32_FLOAT), F(R32G32_UINT), F(R32G32_SINT) }, 1, F(R8_UNORM) },
    { F(R10G10B10A2_TYPELESS),  { F(R10G10B10A2_UNORM), F(R10G10B10A2_UINT) }, 1, F(R8_UNORM) },
    { F(R8G8B8A8_TYPELESS),     { F(R8G8B8A8_UNORM), F(R8G8B8A8_UNORM_SRGB), F(R8G8B8A8_UINT), F(R8G8B8A8_SNORM), F(R8G8B8A8_SINT) }, 1, F(R16_UNORM) },
    { F(R16G16_TYPELESS),       { F(R16G16_FLOAT), F(R16G16_UNORM), F(R16G16_UINT), F(R16G16_SNORM), F(R16G16_SINT) }, 1, F(R8_UNORM) },
    { F(R32_TYPELESS),          { F(R32_FLOAT), F(R32_UINT), F(R32_SINT) }, 1, F(R8_UNORM) },
    { F(R8G8_TYPELESS),         { F(R8G8_UNORM), F(R8G8_UINT), F(R8G8_SNORM), F(R8G8_SINT) }, 1, F(R32_FLOAT) },
    { F(R16_TYPELESS),          { F(R16_FLOAT), F(R16_UNORM), F(R16_UINT), F(R16_SNORM), F(R16_SINT) }, 1, F(R32_FLOAT) },
    { F(R8_TYPELESS),           { F(R8_UNORM), F(R8_UINT), F(R8_SNORM), F(R8_SINT) }, 1, F(R32_FLOAT) },
    { F(B8G8R8A8_TYPELESS),     { F(B8G8R8A8_UNORM), F(B8G8R8A8_UNORM_SRGB) }, 1, F(R16_UNORM) },
    { F(B8G8R8X8_TYPELESS),     { F(B8G8R8X8_UNORM), F(B8G8R8X8_UNORM_SRGB) }, 1, F(R16_UNORM) },
    { F(BC1_TYPELESS),          { F(BC1_UNORM), F(BC1_UNORM_SRGB) }, 0, F(R8_UNORM) },
    { F(BC2_TYPELESS),          { F(BC2_UNORM), F(BC2_UNORM_SRGB) }, 0, F(R8_UNORM) },
    { F(BC3_TYPELESS),          { F(BC3_UNORM), F(BC3_UNORM_SRGB) }, 0, F(R8_UNORM) },
    { F(BC4_TYPELESS),          { F(BC4_UNORM), F(BC4_SNORM) }, 0, F(R32_FLOAT) },
    { F(BC5_TYPELESS),          { F(BC5_UNORM), F(BC5_SNORM) }, 0, F(R32_FLOAT) },
    { F(BC6H_TYPELESS),         { F(BC6H_UF16), F(BC6H_SF16) }, 0, F(R8_UNORM) },
    { F(BC7_TYPELESS),          { F(BC7_UNORM), F(BC7_UNORM_SRGB) }, 0, F(R8_UNORM) },
};
#define NCF (sizeof cfam / sizeof *cfam)

static void fam_srv(UINT i)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); const color_fam *c = &cfam[i]; ID3D11Texture2D *t; UINT j; HRESULT hr;
    if (!dev) { t_skip("no D3D11 device"); return; }
    hr = mk_tex2d(dev, c->typeless, 16, 16, D3D11_BIND_SHADER_RESOURCE, &t);
    if (FAILED(hr)) { t_note("typeless %u texture: %s", c->typeless, d3dh_hr_name(hr)); return; }
    for (j = 0; j < 5 && c->m[j]; j++) {
        hr = mk_srv(dev, t, c->m[j]);
        t_check(SUCCEEDED(hr), __FILE__, __LINE__, "SRV member", "typeless %u -> member %u: %s", c->typeless, c->m[j], d3dh_hr_name(hr));
    }
    hr = mk_srv(dev, t, c->foreign);
    t_check(FAILED(hr), __FILE__, __LINE__, "SRV foreign", "typeless %u accepted foreign view format %u", c->typeless, c->foreign);
    hr = mk_srv(dev, t, c->typeless);
    t_check(FAILED(hr), __FILE__, __LINE__, "SRV typeless", "typeless view format %u accepted (%s)", c->typeless, d3dh_hr_name(hr));
    SAFE_RELEASE(t);
}
static void fam_rtv(UINT i)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); const color_fam *c = &cfam[i]; ID3D11Texture2D *t; UINT j; HRESULT hr;
    if (!dev) { t_skip("no D3D11 device"); return; }
    if (!c->rt) { t_skip("family without render target use"); return; }
    hr = mk_tex2d(dev, c->typeless, 16, 16, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, &t);
    if (FAILED(hr)) { t_note("typeless %u RT texture: %s", c->typeless, d3dh_hr_name(hr)); return; }
    for (j = 0; j < 5 && c->m[j]; j++) {
        UINT sup = fpt_support11(dev, c->m[j], NULL);
        hr = mk_rtv(dev, t, c->m[j]);
        if (sup & D3D11_FORMAT_SUPPORT_RENDER_TARGET) t_check(SUCCEEDED(hr), __FILE__, __LINE__, "RTV member", "typeless %u -> RTV %u: %s", c->typeless, c->m[j], d3dh_hr_name(hr));
        else t_note("typeless %u member %u: no RENDER_TARGET support bit, RTV hr=%s", c->typeless, c->m[j], d3dh_hr_name(hr));
    }
    hr = mk_rtv(dev, t, c->foreign);
    t_check(FAILED(hr), __FILE__, __LINE__, "RTV foreign", "typeless %u accepted foreign RTV format %u", c->typeless, c->foreign);
    SAFE_RELEASE(t);
}
#define FS(i, nm) TEST(fpt_d3d11_family_srv_##nm) { fam_srv(i); } TEST(fpt_d3d11_family_rtv_##nm) { fam_rtv(i); }
FS(0, r32g32b32a32) FS(1, r32g32b32) FS(2, r16g16b16a16) FS(3, r32g32) FS(4, r10g10b10a2) FS(5, r8g8b8a8)
FS(6, r16g16) FS(7, r32) FS(8, r8g8) FS(9, r16) FS(10, r8) FS(11, b8g8r8a8) FS(12, b8g8r8x8)
FS(13, bc1) FS(14, bc2) FS(15, bc3) FS(16, bc4) FS(17, bc5) FS(18, bc6h) FS(19, bc7)

/* Same-bit-size cross-family casts: observed only. */
TEST(fpt_d3d11_family_cross_cast_observed)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); ID3D11Texture2D *t; HRESULT hr;
    static const struct { DXGI_FORMAT res, view; } c[] = {
        { F(R8G8B8A8_TYPELESS), F(B8G8R8A8_UNORM) }, { F(R32_TYPELESS), F(R10G10B10A2_UNORM) }, { F(R32_FLOAT), F(R32_UINT) },
        { F(R8G8B8A8_UNORM), F(R8G8B8A8_UNORM_SRGB) }, { F(R16G16_UNORM), F(R32_FLOAT) }, { F(BC1_UNORM), F(R32G32_UINT) } };
    UINT i;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof c / sizeof *c; i++) {
        hr = mk_tex2d(dev, c[i].res, 16, 16, D3D11_BIND_SHADER_RESOURCE, &t);
        if (SUCCEEDED(hr)) { t_note("SRV %u on resource %u: %s", c[i].view, c[i].res, d3dh_hr_name(mk_srv(dev, t, c[i].view))); SAFE_RELEASE(t); }
    }
}

/* Which DXGI formats can be created as a texture, derived from CheckFormatSupport. */
TEST(fpt_d3d11_support_table_note)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT id;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        HRESULT hr; UINT s = fpt_support11(dev, id, &hr);
        if (fpt_formats[id].group == G_NONE) continue;
        t_note("support %-30s hr=%s bits=0x%08x", fpt_formats[id].name, d3dh_hr_name(hr), s);
    }
}
TEST(fpt_d3d11_support_invariants)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT id;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id]; HRESULT hr; UINT s = fpt_support11(dev, id, &hr);
        if (f->group == G_NONE || f->group == G_UNKNOWN) { CHECKF(FAILED(hr) || s == 0, "%s id %u: success with bits 0x%x", f->name, id, s); continue; }
        if (FAILED(hr)) continue;
        if (s & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) CHECKF(s & D3D11_FORMAT_SUPPORT_TEXTURE2D, "%s: DEPTH_STENCIL without TEXTURE2D", f->name);
        if (s & D3D11_FORMAT_SUPPORT_BLENDABLE) CHECKF(s & D3D11_FORMAT_SUPPORT_RENDER_TARGET, "%s: BLENDABLE without RENDER_TARGET", f->name);
        if ((s & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET) && !(s & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)) CHECKF(s & D3D11_FORMAT_SUPPORT_RENDER_TARGET, "%s: MSAA RT without RENDER_TARGET", f->name);
        if (s & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN) CHECKF(s & D3D11_FORMAT_SUPPORT_RENDER_TARGET, "%s: MIP_AUTOGEN without RENDER_TARGET", f->name);
        if (fpt_is_bc(f)) CHECKF(!(s & (D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL | D3D11_FORMAT_SUPPORT_BLENDABLE)), "%s: block compressed with RT/DS bits 0x%x", f->name, s);
    }
}
TEST(fpt_d3d11_support_undefined_ids)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); static const UINT ids[] = { 133, 134, 200, 0x10000, 0x7fffffff, 0xffffffffu };
    UINT i;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof ids / sizeof *ids; i++) {
        UINT s = 0xdeadbeef; HRESULT hr = ID3D11Device_CheckFormatSupport(dev, (DXGI_FORMAT)ids[i], &s);
        CHECKF(FAILED(hr) || s == 0, "id 0x%x: hr=%s bits=0x%x", ids[i], d3dh_hr_name(hr), s);
        t_note("CheckFormatSupport(0x%x) = %s bits=0x%x", ids[i], d3dh_hr_name(hr), s);
    }
}
TEST(fpt_d3d11_support_depth_formats_have_ds_bit)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT i;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof all_dsv / sizeof *all_dsv; i++) {
        HRESULT hr; UINT s = fpt_support11(dev, all_dsv[i], &hr);
        EXPECT_HR(hr, S_OK);
        CHECKF(s & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, "format %u lacks DEPTH_STENCIL (0x%x)", all_dsv[i], s);
        CHECKF(s & D3D11_FORMAT_SUPPORT_TEXTURE2D, "format %u lacks TEXTURE2D (0x%x)", all_dsv[i], s);
        CHECKF(!(s & D3D11_FORMAT_SUPPORT_RENDER_TARGET), "format %u has RENDER_TARGET (0x%x)", all_dsv[i], s);
    }
}
TEST(fpt_d3d11_support_required_formats_fl11)
{
    ID3D11Device *dev; D3D_FEATURE_LEVEL fl; UINT i;
    static const struct { DXGI_FORMAT f; UINT need; } req[] = {
        { F(R8G8B8A8_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_BLENDABLE | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(R8G8B8A8_UNORM_SRGB), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(B8G8R8A8_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(R16G16B16A16_FLOAT), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(R32G32B32A32_FLOAT), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_LOAD },
        { F(R32_FLOAT), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_LOAD },
        { F(R8_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(BC1_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_TEXTURE3D | D3D11_FORMAT_SUPPORT_TEXTURECUBE | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(BC2_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_TEXTURE3D | D3D11_FORMAT_SUPPORT_TEXTURECUBE | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(BC3_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_TEXTURE3D | D3D11_FORMAT_SUPPORT_TEXTURECUBE | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(BC4_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(BC5_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(BC6H_UF16), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(BC7_UNORM), D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE },
        { F(D32_FLOAT), D3D11_FORMAT_SUPPORT_DEPTH_STENCIL }, { F(D24_UNORM_S8_UINT), D3D11_FORMAT_SUPPORT_DEPTH_STENCIL },
        { F(D16_UNORM), D3D11_FORMAT_SUPPORT_DEPTH_STENCIL }, { F(D32_FLOAT_S8X24_UINT), D3D11_FORMAT_SUPPORT_DEPTH_STENCIL },
    };
    dev = fpt_dev11(NULL, &fl);
    if (!dev) { t_skip("no D3D11 device"); return; }
    if (fl < D3D_FEATURE_LEVEL_11_0) { t_skip("feature level below 11_0"); return; }
    for (i = 0; i < sizeof req / sizeof *req; i++) {
        HRESULT hr; UINT s = fpt_support11(dev, req[i].f, &hr);
        EXPECT_HR(hr, S_OK);
        CHECKF((s & req[i].need) == req[i].need, "format %u: bits 0x%x missing 0x%x", req[i].f, s, req[i].need & ~s);
    }
}
TEST(fpt_d3d11_support_texture2d_bit_implies_creation)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT id;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (id = 1; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id]; HRESULT hr, hr2; UINT s; ID3D11Texture2D *t;
        if (f->group == G_NONE || f->group == G_PLANAR || f->group == G_VPACK || f->group == G_PAL || f->group == G_R1 || f->group == G_422) continue;
        s = fpt_support11(dev, id, &hr);
        if (FAILED(hr) || !(s & D3D11_FORMAT_SUPPORT_TEXTURE2D)) continue;
        hr2 = mk_tex2d(dev, (DXGI_FORMAT)id, 16, 16, 0, &t);
        CHECKF(SUCCEEDED(hr2), "%s: TEXTURE2D supported but CreateTexture2D hr=%s", f->name, d3dh_hr_name(hr2));
        SAFE_RELEASE(t);
    }
}
TEST(fpt_d3d11_create_texture_invalid_formats)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); ID3D11Texture2D *t; static const UINT ids[] = { 0, 116, 120, 129, 133, 0x7fffffff }; UINT i;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof ids / sizeof *ids; i++) {
        HRESULT hr = mk_tex2d(dev, (DXGI_FORMAT)ids[i], 16, 16, 0, &t);
        CHECKF(FAILED(hr) && !t, "format %u created texture hr=%s", ids[i], d3dh_hr_name(hr));
        if (ids[i] == 0) EXPECT_HR(hr, E_INVALIDARG);
        SAFE_RELEASE(t);
    }
}

/* ---- size limits ------------------------------------------------------------------ */
static ID3D11Device *dev_fl11(D3D_FEATURE_LEVEL *fl)
{
    ID3D11Device *d = fpt_dev11(NULL, fl);
    if (!d) { t_skip("no D3D11 device"); return NULL; }
    if (*fl < D3D_FEATURE_LEVEL_11_0) { t_skip("limits below FL11_0 differ (FL 0x%x)", *fl); return NULL; }
    return d;
}
TEST(fpt_d3d11_limit_texture1d_width)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *dev = dev_fl11(&fl); D3D11_TEXTURE1D_DESC t = {0}; ID3D11Texture1D *tex = NULL; HRESULT hr;
    if (!dev) return;
    t.MipLevels = 1; t.ArraySize = 1; t.Format = F(R8_UNORM); t.Usage = D3D11_USAGE_DEFAULT;
    t.Width = D3D11_REQ_TEXTURE1D_U_DIMENSION;
    EXPECT_HR(ID3D11Device_CreateTexture1D(dev, &t, NULL, &tex), S_OK); SAFE_RELEASE(tex);
    t.Width = D3D11_REQ_TEXTURE1D_U_DIMENSION + 1;
    hr = ID3D11Device_CreateTexture1D(dev, &t, NULL, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
    t.Width = 0;
    hr = ID3D11Device_CreateTexture1D(dev, &t, NULL, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
}
TEST(fpt_d3d11_limit_texture1d_array)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *dev = dev_fl11(&fl); D3D11_TEXTURE1D_DESC t = {0}; ID3D11Texture1D *tex = NULL; HRESULT hr;
    if (!dev) return;
    t.Width = 1; t.MipLevels = 1; t.Format = F(R8_UNORM); t.Usage = D3D11_USAGE_DEFAULT;
    t.ArraySize = D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION;
    EXPECT_HR(ID3D11Device_CreateTexture1D(dev, &t, NULL, &tex), S_OK); SAFE_RELEASE(tex);
    t.ArraySize = D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION + 1;
    hr = ID3D11Device_CreateTexture1D(dev, &t, NULL, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
    t.ArraySize = 0;
    hr = ID3D11Device_CreateTexture1D(dev, &t, NULL, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
}
TEST(fpt_d3d11_limit_texture2d_dimensions)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *dev = dev_fl11(&fl); ID3D11Texture2D *tex; HRESULT hr;
    if (!dev) return;
    hr = mk_tex2d(dev, F(R8_UNORM), D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION, 1, 0, &tex); EXPECT_HR(hr, S_OK); SAFE_RELEASE(tex);
    hr = mk_tex2d(dev, F(R8_UNORM), 1, D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION, 0, &tex); EXPECT_HR(hr, S_OK); SAFE_RELEASE(tex);
    hr = mk_tex2d(dev, F(R8_UNORM), D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1, 1, 0, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
    hr = mk_tex2d(dev, F(R8_UNORM), 1, D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1, 0, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
    hr = mk_tex2d(dev, F(R8_UNORM), 0, 1, 0, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
    hr = mk_tex2d(dev, F(R8_UNORM), 1, 0, 0, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
}
TEST(fpt_d3d11_limit_texture2d_array)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *dev = dev_fl11(&fl); D3D11_TEXTURE2D_DESC t = {0}; ID3D11Texture2D *tex = NULL; HRESULT hr;
    if (!dev) return;
    t.Width = 1; t.Height = 1; t.MipLevels = 1; t.Format = F(R8_UNORM); t.SampleDesc.Count = 1; t.Usage = D3D11_USAGE_DEFAULT;
    t.ArraySize = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    EXPECT_HR(ID3D11Device_CreateTexture2D(dev, &t, NULL, &tex), S_OK); SAFE_RELEASE(tex);
    t.ArraySize = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION + 1;
    hr = ID3D11Device_CreateTexture2D(dev, &t, NULL, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
    t.ArraySize = 0;
    hr = ID3D11Device_CreateTexture2D(dev, &t, NULL, &tex); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(tex);
}
TEST(fpt_d3d11_limit_texture3d_dimensions)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *dev = dev_fl11(&fl); D3D11_TEXTURE3D_DESC t = {0}; ID3D11Texture3D *tex = NULL; HRESULT hr; UINT axis;
    if (!dev) return;
    t.MipLevels = 1; t.Format = F(R8_UNORM); t.Usage = D3D11_USAGE_DEFAULT;
    for (axis = 0; axis < 3; axis++) {
        UINT v[3] = { 1, 1, 1 };
        v[axis] = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        t.Width = v[0]; t.Height = v[1]; t.Depth = v[2];
        hr = ID3D11Device_CreateTexture3D(dev, &t, NULL, &tex); CHECKF(hr == S_OK, "axis %u at limit: %s", axis, d3dh_hr_name(hr)); SAFE_RELEASE(tex);
        v[axis]++;
        t.Width = v[0]; t.Height = v[1]; t.Depth = v[2];
        hr = ID3D11Device_CreateTexture3D(dev, &t, NULL, &tex); CHECKF(hr == E_INVALIDARG, "axis %u over limit: %s", axis, d3dh_hr_name(hr)); SAFE_RELEASE(tex);
        v[axis] = 0;
        t.Width = v[0]; t.Height = v[1]; t.Depth = v[2];
        hr = ID3D11Device_CreateTexture3D(dev, &t, NULL, &tex); CHECKF(hr == E_INVALIDARG, "axis %u zero: %s", axis, d3dh_hr_name(hr)); SAFE_RELEASE(tex);
    }
}
TEST(fpt_d3d11_limit_mip_levels)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *dev = dev_fl11(&fl); D3D11_TEXTURE2D_DESC t = {0}; ID3D11Texture2D *tex = NULL; HRESULT hr; UINT m;
    if (!dev) return;
    t.ArraySize = 1; t.Format = F(R8_UNORM); t.SampleDesc.Count = 1; t.Usage = D3D11_USAGE_DEFAULT;
    /* 2^k wide: exactly k+1 levels are valid; one more is rejected. */
    for (m = 1; m <= D3D11_REQ_MIP_LEVELS; m++) {
        t.Width = 1u << (m - 1); t.Height = 1; t.MipLevels = m;
        hr = ID3D11Device_CreateTexture2D(dev, &t, NULL, &tex); CHECKF(hr == S_OK, "width %u mips %u: %s", t.Width, m, d3dh_hr_name(hr)); SAFE_RELEASE(tex);
        t.MipLevels = m + 1;
        hr = ID3D11Device_CreateTexture2D(dev, &t, NULL, &tex); CHECKF(hr == E_INVALIDARG, "width %u mips %u: %s", t.Width, m + 1, d3dh_hr_name(hr)); SAFE_RELEASE(tex);
    }
}
TEST(fpt_d3d11_limit_buffer_bytewidth)
{
    D3D_FEATURE_LEVEL fl; ID3D11Device *dev = dev_fl11(&fl); D3D11_BUFFER_DESC b = {0}; ID3D11Buffer *buf = NULL; HRESULT hr;
    if (!dev) return;
    b.Usage = D3D11_USAGE_DEFAULT; b.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    b.ByteWidth = 0;
    hr = ID3D11Device_CreateBuffer(dev, &b, NULL, &buf); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(buf);
    b.ByteWidth = 1;
    hr = ID3D11Device_CreateBuffer(dev, &b, NULL, &buf); EXPECT_HR(hr, S_OK); SAFE_RELEASE(buf);
    b.ByteWidth = 3;
    hr = ID3D11Device_CreateBuffer(dev, &b, NULL, &buf); EXPECT_HR(hr, S_OK); SAFE_RELEASE(buf);
}

/* ---- slot counts: set N bindings, read them back ---------------------------------------- */
TEST(fpt_d3d11_slots_samplers_16)
{
    ID3D11DeviceContext *ctx; ID3D11Device *dev = fpt_dev11(&ctx, NULL); ID3D11SamplerState *s = NULL, *in[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT], *out[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT]; D3D11_SAMPLER_DESC d = {0}; UINT i;
    if (!dev) { t_skip("no D3D11 device"); return; }
    CHECK(D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT == 16);
    d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; d.MaxLOD = D3D11_FLOAT32_MAX; d.ComparisonFunc = D3D11_COMPARISON_NEVER;
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d, &s), S_OK);
    if (!s) return;
    for (i = 0; i < 16; i++) in[i] = s;
    ID3D11DeviceContext_PSSetSamplers(ctx, 0, 16, in);
    memset(out, 0, sizeof out);
    ID3D11DeviceContext_PSGetSamplers(ctx, 0, 16, out);
    for (i = 0; i < 16; i++) { CHECKF(out[i] == s, "PS sampler slot %u", i); SAFE_RELEASE(out[i]); }
    ID3D11DeviceContext_VSSetSamplers(ctx, 0, 16, in);
    ID3D11DeviceContext_CSSetSamplers(ctx, 0, 16, in);
    memset(out, 0, sizeof out);
    ID3D11DeviceContext_CSGetSamplers(ctx, 0, 16, out);
    for (i = 0; i < 16; i++) { CHECKF(out[i] == s, "CS sampler slot %u", i); SAFE_RELEASE(out[i]); }
    memset(in, 0, sizeof in);
    ID3D11DeviceContext_PSSetSamplers(ctx, 0, 16, in); ID3D11DeviceContext_VSSetSamplers(ctx, 0, 16, in); ID3D11DeviceContext_CSSetSamplers(ctx, 0, 16, in);
    SAFE_RELEASE(s);
}
TEST(fpt_d3d11_slots_srv_128)
{
    ID3D11DeviceContext *ctx; ID3D11Device *dev = fpt_dev11(&ctx, NULL); ID3D11Texture2D *t; ID3D11ShaderResourceView *v = NULL, *in[128], *out[128]; UINT i;
    D3D11_SHADER_RESOURCE_VIEW_DESC d = {0};
    if (!dev) { t_skip("no D3D11 device"); return; }
    CHECK(D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT == 128);
    EXPECT_HR(mk_tex2d(dev, F(R8G8B8A8_UNORM), 4, 4, D3D11_BIND_SHADER_RESOURCE, &t), S_OK);
    if (!t) return;
    d.Format = F(R8G8B8A8_UNORM); d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)t, &d, &v), S_OK);
    if (v) {
        for (i = 0; i < 128; i++) in[i] = v;
        ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 128, in);
        memset(out, 0, sizeof out);
        ID3D11DeviceContext_PSGetShaderResources(ctx, 0, 128, out);
        for (i = 0; i < 128; i++) { CHECKF(out[i] == v, "PS SRV slot %u", i); SAFE_RELEASE(out[i]); }
        ID3D11DeviceContext_CSSetShaderResources(ctx, 0, 128, in);
        memset(out, 0, sizeof out);
        ID3D11DeviceContext_CSGetShaderResources(ctx, 127, 1, out);
        CHECK(out[0] == v); SAFE_RELEASE(out[0]);
        memset(in, 0, sizeof in);
        ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 128, in); ID3D11DeviceContext_CSSetShaderResources(ctx, 0, 128, in);
        SAFE_RELEASE(v);
    }
    SAFE_RELEASE(t);
}
TEST(fpt_d3d11_slots_constant_buffers_14)
{
    ID3D11DeviceContext *ctx; ID3D11Device *dev = fpt_dev11(&ctx, NULL); ID3D11Buffer *b = NULL, *in[14], *out[14]; D3D11_BUFFER_DESC bd = {0}; UINT i;
    if (!dev) { t_skip("no D3D11 device"); return; }
    CHECK(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT == 14);
    bd.ByteWidth = 64; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &b), S_OK);
    if (!b) return;
    for (i = 0; i < 14; i++) in[i] = b;
    ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 14, in);
    memset(out, 0, sizeof out);
    ID3D11DeviceContext_VSGetConstantBuffers(ctx, 0, 14, out);
    for (i = 0; i < 14; i++) { CHECKF(out[i] == b, "VS CB slot %u", i); SAFE_RELEASE(out[i]); }
    ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 14, in);
    memset(out, 0, sizeof out);
    ID3D11DeviceContext_PSGetConstantBuffers(ctx, 13, 1, out);
    CHECK(out[0] == b); SAFE_RELEASE(out[0]);
    memset(in, 0, sizeof in);
    ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 14, in); ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 14, in);
    SAFE_RELEASE(b);
}
TEST(fpt_d3d11_slots_vertex_buffers_32)
{
    ID3D11DeviceContext *ctx; ID3D11Device *dev = fpt_dev11(&ctx, NULL); ID3D11Buffer *b = NULL, *in[32], *out[32]; D3D11_BUFFER_DESC bd = {0}; UINT i, st[32], of[32], ost[32], oof[32];
    if (!dev) { t_skip("no D3D11 device"); return; }
    CHECK(D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT == 32);
    bd.ByteWidth = 256; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &b), S_OK);
    if (!b) return;
    for (i = 0; i < 32; i++) { in[i] = b; st[i] = 4 + i; of[i] = i; }
    ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 32, in, st, of);
    memset(out, 0, sizeof out);
    ID3D11DeviceContext_IAGetVertexBuffers(ctx, 0, 32, out, ost, oof);
    for (i = 0; i < 32; i++) { CHECKF(out[i] == b && ost[i] == st[i] && oof[i] == of[i], "IA slot %u: %p stride %u offset %u", i, out[i], ost[i], oof[i]); SAFE_RELEASE(out[i]); }
    memset(in, 0, sizeof in); memset(st, 0, sizeof st); memset(of, 0, sizeof of);
    ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 32, in, st, of);
    SAFE_RELEASE(b);
}
TEST(fpt_d3d11_slots_render_targets_8)
{
    ID3D11DeviceContext *ctx; ID3D11Device *dev = fpt_dev11(&ctx, NULL); ID3D11Texture2D *t[8]; ID3D11RenderTargetView *rtv[8], *out[8]; UINT i;
    D3D11_RENDER_TARGET_VIEW_DESC d = {0};
    if (!dev) { t_skip("no D3D11 device"); return; }
    CHECK(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT == 8);
    memset(t, 0, sizeof t); memset(rtv, 0, sizeof rtv);
    d.Format = F(R8G8B8A8_UNORM); d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    for (i = 0; i < 8; i++) {
        if (FAILED(mk_tex2d(dev, F(R8G8B8A8_UNORM), 8, 8, D3D11_BIND_RENDER_TARGET, &t[i]))) break;
        if (FAILED(ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)t[i], &d, &rtv[i]))) break;
    }
    if (i == 8) {
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 8, rtv, NULL);
        memset(out, 0, sizeof out);
        ID3D11DeviceContext_OMGetRenderTargets(ctx, 8, out, NULL);
        for (i = 0; i < 8; i++) { CHECKF(out[i] == rtv[i], "RTV slot %u", i); SAFE_RELEASE(out[i]); }
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 0, NULL, NULL);
    } else t_note("could not create 8 render targets");
    for (i = 0; i < 8; i++) { SAFE_RELEASE(rtv[i]); SAFE_RELEASE(t[i]); }
}
TEST(fpt_d3d11_slots_viewports_scissors_16)
{
    ID3D11DeviceContext *ctx; ID3D11Device *dev = fpt_dev11(&ctx, NULL); D3D11_VIEWPORT vp[16], ov[16]; D3D11_RECT rc[16], orc[16]; UINT i, n;
    if (!dev) { t_skip("no D3D11 device"); return; }
    CHECK(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE == 16);
    for (i = 0; i < 16; i++) {
        vp[i].TopLeftX = (FLOAT)i; vp[i].TopLeftY = (FLOAT)(2 * i); vp[i].Width = 16.0f + i; vp[i].Height = 32.0f + i; vp[i].MinDepth = 0.0f; vp[i].MaxDepth = 1.0f;
        rc[i].left = i; rc[i].top = i + 1; rc[i].right = i + 20; rc[i].bottom = i + 30;
    }
    ID3D11DeviceContext_RSSetViewports(ctx, 16, vp);
    ID3D11DeviceContext_RSSetScissorRects(ctx, 16, rc);
    n = 16; memset(ov, 0, sizeof ov);
    ID3D11DeviceContext_RSGetViewports(ctx, &n, ov);
    CHECKF(n == 16, "viewport count %u", n);
    for (i = 0; i < 16; i++) CHECKF(ov[i].TopLeftX == vp[i].TopLeftX && ov[i].Width == vp[i].Width && ov[i].Height == vp[i].Height, "viewport %u", i);
    n = 16; memset(orc, 0, sizeof orc);
    ID3D11DeviceContext_RSGetScissorRects(ctx, &n, orc);
    CHECKF(n == 16, "scissor count %u", n);
    for (i = 0; i < 16; i++) CHECKF(orc[i].left == rc[i].left && orc[i].bottom == rc[i].bottom, "scissor %u", i);
    ID3D11DeviceContext_RSSetViewports(ctx, 0, NULL);
    ID3D11DeviceContext_RSSetScissorRects(ctx, 0, NULL);
}
TEST(fpt_d3d11_slots_uav_count_by_feature_level)
{
    ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl; ID3D11Device *dev = fpt_dev11(&ctx, &fl); ID3D11Buffer *b[D3D11_1_UAV_SLOT_COUNT]; ID3D11UnorderedAccessView *u[D3D11_1_UAV_SLOT_COUNT], *o[D3D11_1_UAV_SLOT_COUNT];
    UINT n, i, made = 0; UINT init[D3D11_1_UAV_SLOT_COUNT];
    if (!dev) { t_skip("no D3D11 device"); return; }
    if (fl < D3D_FEATURE_LEVEL_11_0) { t_skip("compute UAV slots need FL11_0"); return; }
    n = fl >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
    CHECK(D3D11_1_UAV_SLOT_COUNT == 64 && D3D11_PS_CS_UAV_REGISTER_COUNT == 8);
    memset(b, 0, sizeof b); memset(u, 0, sizeof u);
    for (i = 0; i < n; i++) {
        D3D11_BUFFER_DESC bd = {0}; D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {0};
        bd.ByteWidth = 64; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        ud.Format = F(R32_UINT); ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 16;
        if (FAILED(ID3D11Device_CreateBuffer(dev, &bd, NULL, &b[i]))) break;
        if (FAILED(ID3D11Device_CreateUnorderedAccessView(dev, (ID3D11Resource *)b[i], &ud, &u[i]))) break;
        made++; init[i] = 0xffffffffu;
    }
    t_note("feature level 0x%x: UAV slots tested %u", fl, n);
    if (made == n) {
        ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, n, u, init);
        memset(o, 0, sizeof o);
        ID3D11DeviceContext_CSGetUnorderedAccessViews(ctx, 0, n, o);
        for (i = 0; i < n; i++) { CHECKF(o[i] == u[i], "CS UAV slot %u", i); SAFE_RELEASE(o[i]); }
    } else { t_note("created only %u of %u UAVs", made, n); }
    {
        ID3D11UnorderedAccessView *nulls[D3D11_1_UAV_SLOT_COUNT] = {0};
        ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, n, nulls, init);
    }
    for (i = 0; i < n; i++) { SAFE_RELEASE(u[i]); SAFE_RELEASE(b[i]); }
}
