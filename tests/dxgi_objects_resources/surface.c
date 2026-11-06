/* IDXGISurface / IDXGISurface1: GetDesc, Map/Unmap flag rules, GetDC/ReleaseDC. */
#include "objs.h"

static IDXGISurface *surf_of(IUnknown *res)
{
    IDXGISurface *s = NULL;
    if (FAILED(IUnknown_QueryInterface(res, &IID_IDXGISurface, (void **)&s))) return NULL;
    return s;
}
/* staging 32x16 BGRA, CPU read+write */
static IDXGISurface *make_staging_surface(objs_ctx *c, ID3D11Texture2D **tex)
{
    IDXGISurface *s;
    *tex = NULL;
    if (FAILED(objs_staging(c->dev, 32, 16, DXGI_FORMAT_B8G8R8A8_UNORM, tex))) return NULL;
    s = surf_of((IUnknown *)*tex);
    if (!s) { ID3D11Texture2D_Release(*tex); *tex = NULL; }
    return s;
}
#define NEED_STAGING(c, t, s) NEED_DEV(c); ID3D11Texture2D *t = NULL; IDXGISurface *s = make_staging_surface(&c, &t); \
    if (!s) { t_skip("no staging surface"); objs_close(&c); return; }
#define DONE_STAGING(c, t, s) do { IDXGISurface_Release(s); ID3D11Texture2D_Release(t); objs_close(&c); } while (0)

/* ---- GetDesc ------------------------------------------------------------------------------------ */
TEST(objs_surf_getdesc_table)
{
    NEED_DEV(c);
    static const struct { UINT w, h; DXGI_FORMAT f; D3D11_USAGE u; UINT bind, cpu; } tab[] = {
        { 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0 },
        { 33, 17, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0 },
        { 1, 1, DXGI_FORMAT_R32_FLOAT, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0 },
        { 128, 4, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0 },
        { 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ },
        { 16, 16, DXGI_FORMAT_R10G10B10A2_UNORM, D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE },
        { 256, 256, DXGI_FORMAT_R8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS, 0 },
        { 64, 64, DXGI_FORMAT_R24G8_TYPELESS, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0 },
    };
    int i;
    for (i = 0; i < OBJS_ARRAY(tab); i++) {
        ID3D11Texture2D *t = NULL; IDXGISurface *s; DXGI_SURFACE_DESC d;
        if ((tab[i].bind & D3D11_BIND_UNORDERED_ACCESS) && c.fl < D3D_FEATURE_LEVEL_11_0) continue;
        if (FAILED(objs_tex2d(c.dev, tab[i].w, tab[i].h, 1, 1, tab[i].f, tab[i].u, tab[i].bind, tab[i].cpu, 0, &t))) { t_note("case %d not creatable", i); continue; }
        s = surf_of((IUnknown *)t);
        CHECKF(s != NULL, "case %d: no IDXGISurface", i);
        if (s) {
            memset(&d, 0xcc, sizeof(d));
            EXPECT_HR_OK(IDXGISurface_GetDesc(s, &d));
            CHECKF(d.Width == tab[i].w && d.Height == tab[i].h && d.Format == tab[i].f, "case %d: %ux%u fmt %d", i, d.Width, d.Height, d.Format);
            CHECKF(d.SampleDesc.Count == 1 && d.SampleDesc.Quality == 0, "case %d: sample %u/%u", i, d.SampleDesc.Count, d.SampleDesc.Quality);
            IDXGISurface_Release(s);
        }
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_surf_getdesc_multisample)
{
    NEED_DEV(c);
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t = NULL; UINT q = 0; IDXGISurface *s; DXGI_SURFACE_DESC d;
    if (FAILED(ID3D11Device_CheckMultisampleQualityLevels(c.dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4, &q)) || !q) { t_skip("4x MSAA unsupported"); objs_close(&c); return; }
    memset(&td, 0, sizeof(td)); td.Width = 32; td.Height = 32; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 4; td.SampleDesc.Quality = 0; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(ID3D11Device_CreateTexture2D(c.dev, &td, NULL, &t))) { t_skip("cannot create"); objs_close(&c); return; }
    s = surf_of((IUnknown *)t);
    if (s) {
        EXPECT_HR_OK(IDXGISurface_GetDesc(s, &d));
        EXPECT_EQ(d.SampleDesc.Count, 4);
        EXPECT_EQ(d.SampleDesc.Quality, 0);
        IDXGISurface_Release(s);
    }
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}

/* ---- Map flag rules ---------------------------------------------------------------------------------- */
TEST(objs_surf_map_read)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    EXPECT_HR_OK(IDXGISurface_Map(s, &m, DXGI_MAP_READ));
    CHECK(m.pBits != NULL);
    CHECKF(m.Pitch >= 32 * 4, "pitch %d", m.Pitch);
    EXPECT_HR_OK(IDXGISurface_Unmap(s));
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_write)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    EXPECT_HR_OK(IDXGISurface_Map(s, &m, DXGI_MAP_WRITE));
    CHECK(m.pBits != NULL);
    EXPECT_HR_OK(IDXGISurface_Unmap(s));
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_readwrite)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    EXPECT_HR_OK(IDXGISurface_Map(s, &m, DXGI_MAP_READ | DXGI_MAP_WRITE));
    CHECK(m.pBits != NULL);
    EXPECT_HR_OK(IDXGISurface_Unmap(s));
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_flags_zero)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    EXPECT_HR(IDXGISurface_Map(s, &m, 0), E_INVALIDARG);
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_discard_alone)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    EXPECT_HR(IDXGISurface_Map(s, &m, DXGI_MAP_DISCARD), E_INVALIDARG);
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_read_discard)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    EXPECT_HR(IDXGISurface_Map(s, &m, DXGI_MAP_READ | DXGI_MAP_DISCARD), E_INVALIDARG);
    EXPECT_HR(IDXGISurface_Map(s, &m, DXGI_MAP_READ | DXGI_MAP_WRITE | DXGI_MAP_DISCARD), E_INVALIDARG);
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_write_discard_on_staging)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    /* staging resources (CPU read and write) cannot be mapped with DISCARD */
    EXPECT_HR(IDXGISurface_Map(s, &m, DXGI_MAP_WRITE | DXGI_MAP_DISCARD), E_INVALIDARG);
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_unknown_flag_bits)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    HRESULT hr = IDXGISurface_Map(s, &m, DXGI_MAP_READ | 0x8);
    t_note("Map(READ|0x8) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) IDXGISurface_Unmap(s);
    hr = IDXGISurface_Map(s, &m, 0x8);
    t_note("Map(0x8) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) IDXGISurface_Unmap(s);
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_default_usage_rejected)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGISurface *s; DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    s = surf_of((IUnknown *)t);
    if (s) {
        EXPECT_HR(IDXGISurface_Map(s, &m, DXGI_MAP_READ), E_INVALIDARG);
        EXPECT_HR(IDXGISurface_Map(s, &m, DXGI_MAP_WRITE), E_INVALIDARG);
        EXPECT_HR(IDXGISurface_Map(s, &m, DXGI_MAP_READ | DXGI_MAP_WRITE), E_INVALIDARG);
        IDXGISurface_Release(s);
    }
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_surf_map_createsurface_no_cpu_access)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL; DXGI_MAPPED_RECT m; HRESULT hr; memset(&m, 0, sizeof(m));
    memset(&d, 0, sizeof(d)); d.Width = 16; d.Height = 16; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
    EXPECT_HR_OK(IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s));
    if (s) {
        hr = IDXGISurface_Map(s, &m, DXGI_MAP_READ);
        t_note("Map(READ) on CreateSurface(RENDER_TARGET_OUTPUT) surface -> %s", d3dh_hr_name(hr));
        EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
        if (SUCCEEDED(hr)) IDXGISurface_Unmap(s);
        IDXGISurface_Release(s);
    }
    objs_close(&c);
}
TEST(objs_surf_map_dynamic)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGISurface *s; DXGI_MAPPED_RECT m; HRESULT hr; memset(&m, 0, sizeof(m));
    if (FAILED(objs_tex2d(c.dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE, 0, &t))) { t_skip("no dynamic texture"); objs_close(&c); return; }
    s = surf_of((IUnknown *)t);
    if (s) {
        hr = IDXGISurface_Map(s, &m, DXGI_MAP_READ);
        t_note("dynamic texture: Map(READ) -> %s", d3dh_hr_name(hr)); if (SUCCEEDED(hr)) IDXGISurface_Unmap(s);
        EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
        hr = IDXGISurface_Map(s, &m, DXGI_MAP_WRITE);
        t_note("dynamic texture: Map(WRITE) -> %s", d3dh_hr_name(hr)); if (SUCCEEDED(hr)) IDXGISurface_Unmap(s);
        EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
        hr = IDXGISurface_Map(s, &m, DXGI_MAP_WRITE | DXGI_MAP_DISCARD);
        t_note("dynamic texture: Map(WRITE|DISCARD) -> %s", d3dh_hr_name(hr)); if (SUCCEEDED(hr)) IDXGISurface_Unmap(s);
        EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
        IDXGISurface_Release(s);
    }
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_surf_map_twice)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m1, m2; HRESULT hr;
    memset(&m1, 0, sizeof(m1)); memset(&m2, 0, sizeof(m2));
    EXPECT_HR_OK(IDXGISurface_Map(s, &m1, DXGI_MAP_READ));
    hr = IDXGISurface_Map(s, &m2, DXGI_MAP_READ);
    t_note("second Map without Unmap -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) IDXGISurface_Unmap(s);
    IDXGISurface_Unmap(s);
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_unmap_without_map)
{
    NEED_STAGING(c, t, s);
    HRESULT hr = IDXGISurface_Unmap(s);
    t_note("Unmap without Map -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_remap_cycle)
{
    NEED_STAGING(c, t, s);
    int i;
    for (i = 0; i < 20; i++) {
        DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
        HRESULT hr = IDXGISurface_Map(s, &m, (i & 1) ? DXGI_MAP_WRITE : DXGI_MAP_READ);
        CHECKF(hr == S_OK && m.pBits, "cycle %d: %s", i, d3dh_hr_name(hr));
        hr = IDXGISurface_Unmap(s);
        CHECKF(hr == S_OK, "cycle %d unmap: %s", i, d3dh_hr_name(hr));
    }
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_data_roundtrip_dxgi)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; UINT x, y, bad = 0;
    EXPECT_HR_OK(IDXGISurface_Map(s, &m, DXGI_MAP_WRITE));
    for (y = 0; y < 16; y++) for (x = 0; x < 32; x++) ((UINT *)(m.pBits + y * m.Pitch))[x] = 0xff000000u | (y << 8) | x;
    EXPECT_HR_OK(IDXGISurface_Unmap(s));
    EXPECT_HR_OK(IDXGISurface_Map(s, &m, DXGI_MAP_READ));
    for (y = 0; y < 16; y++) for (x = 0; x < 32; x++) if (((UINT *)(m.pBits + y * m.Pitch))[x] != (0xff000000u | (y << 8) | x)) bad++;
    EXPECT_EQ(bad, 0);
    EXPECT_HR_OK(IDXGISurface_Unmap(s));
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_data_visible_to_d3d11)
{
    NEED_STAGING(c, t, s);
    DXGI_MAPPED_RECT m; D3D11_MAPPED_SUBRESOURCE ms; UINT x, y, bad = 0;
    EXPECT_HR_OK(IDXGISurface_Map(s, &m, DXGI_MAP_WRITE));
    for (y = 0; y < 16; y++) for (x = 0; x < 32; x++) ((UINT *)(m.pBits + y * m.Pitch))[x] = 0xff112233u + (y * 32 + x);
    EXPECT_HR_OK(IDXGISurface_Unmap(s));
    if (SUCCEEDED(ID3D11DeviceContext_Map(c.ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &ms))) {
        for (y = 0; y < 16; y++) for (x = 0; x < 32; x++) if (((UINT *)((BYTE *)ms.pData + y * ms.RowPitch))[x] != 0xff112233u + (y * 32 + x)) bad++;
        ID3D11DeviceContext_Unmap(c.ctx, (ID3D11Resource *)t, 0);
        EXPECT_EQ(bad, 0);
    } else t_note("D3D11 Map(READ) after DXGI Map failed");
    DONE_STAGING(c, t, s);
}
TEST(objs_surf_map_copy_from_gpu)
{
    NEED_DEV(c);
    ID3D11Texture2D *rt = NULL, *st = NULL; ID3D11RenderTargetView *rtv = NULL; IDXGISurface *s = NULL; DXGI_MAPPED_RECT m;
    static const float col[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    EXPECT_HR_OK(objs_rt(c.dev, &rt));
    EXPECT_HR_OK(objs_staging(c.dev, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &st));
    if (!rt || !st || FAILED(ID3D11Device_CreateRenderTargetView(c.dev, (ID3D11Resource *)rt, NULL, &rtv))) { SAFE_RELEASE(rt); SAFE_RELEASE(st); objs_close(&c); return; }
    ID3D11DeviceContext_ClearRenderTargetView(c.ctx, rtv, col);
    ID3D11DeviceContext_CopyResource(c.ctx, (ID3D11Resource *)st, (ID3D11Resource *)rt);
    ID3D11DeviceContext_Flush(c.ctx);
    s = surf_of((IUnknown *)st);
    if (s) {
        memset(&m, 0, sizeof(m));
        HRESULT hr = IDXGISurface_Map(s, &m, DXGI_MAP_READ);
        EXPECT_HR_OK(hr);
        if (SUCCEEDED(hr)) {
            UINT px = *(UINT *)(m.pBits + 10 * m.Pitch + 10 * 4);
            EXPECT_EQ(px, 0xffff0000u);   /* B8G8R8A8: opaque red */
            IDXGISurface_Unmap(s);
        }
        IDXGISurface_Release(s);
    }
    ID3D11RenderTargetView_Release(rtv); ID3D11Texture2D_Release(rt); ID3D11Texture2D_Release(st);
    objs_close(&c);
}
TEST(objs_surf_map_pitch_formats)
{
    NEED_DEV(c);
    static const struct { DXGI_FORMAT f; UINT bpp; } tab[] = {
        { DXGI_FORMAT_R8_UNORM, 1 }, { DXGI_FORMAT_R8G8_UNORM, 2 }, { DXGI_FORMAT_R16_FLOAT, 2 }, { DXGI_FORMAT_R8G8B8A8_UNORM, 4 },
        { DXGI_FORMAT_R32_FLOAT, 4 }, { DXGI_FORMAT_R16G16B16A16_FLOAT, 8 }, { DXGI_FORMAT_R32G32_FLOAT, 8 }, { DXGI_FORMAT_R32G32B32A32_FLOAT, 16 },
    };
    int i;
    for (i = 0; i < OBJS_ARRAY(tab); i++) {
        ID3D11Texture2D *t = NULL; IDXGISurface *s; DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
        if (FAILED(objs_staging(c.dev, 24, 8, tab[i].f, &t))) { t_note("format %d not creatable", tab[i].f); continue; }
        s = surf_of((IUnknown *)t);
        if (s && SUCCEEDED(IDXGISurface_Map(s, &m, DXGI_MAP_READ))) {
            CHECKF(m.Pitch >= (INT)(24 * tab[i].bpp), "format %d: pitch %d", tab[i].f, m.Pitch);
            CHECKF(m.pBits != NULL, "format %d: pBits", tab[i].f);
            IDXGISurface_Unmap(s);
        } else CHECKF(0, "format %d: map failed", tab[i].f);
        SAFE_RELEASE(s); ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_surf_map_releasing_mapped_surface)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGISurface *s; DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
    if (FAILED(objs_staging(c.dev, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, &t))) { t_skip("no staging"); objs_close(&c); return; }
    s = surf_of((IUnknown *)t);
    if (s) {
        EXPECT_HR_OK(IDXGISurface_Map(s, &m, DXGI_MAP_READ));
        /* dropping every reference while mapped must not crash or hang */
        IDXGISurface_Release(s);
    }
    ID3D11Texture2D_Release(t);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(c.dev), S_OK);
    objs_close(&c);
}

/* ---- IDXGISurface1 GetDC / ReleaseDC ----------------------------------------------------------------------- */
static IDXGISurface1 *make_gdi_surface(objs_ctx *c, ID3D11Texture2D **t, UINT misc, DXGI_FORMAT f)
{
    IDXGISurface1 *s = NULL;
    *t = NULL;
    if (FAILED(objs_tex2d(c->dev, 64, 64, 1, 1, f, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, misc, t))) return NULL;
    if (FAILED(ID3D11Texture2D_QueryInterface(*t, &IID_IDXGISurface1, (void **)&s))) { ID3D11Texture2D_Release(*t); *t = NULL; }
    return s;
}
#define NEED_GDI(c, t, s) NEED_DEV(c); ID3D11Texture2D *t = NULL; \
    IDXGISurface1 *s = make_gdi_surface(&c, &t, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, DXGI_FORMAT_B8G8R8A8_UNORM); \
    if (!s) { t_skip("no GDI-compatible surface"); objs_close(&c); return; }
#define DONE_GDI(c, t, s) do { IDXGISurface1_Release(s); ID3D11Texture2D_Release(t); objs_close(&c); } while (0)

TEST(objs_gdi_getdc_basic)
{
    NEED_GDI(c, t, s);
    HDC dc = (HDC)(ULONG_PTR)0x1234;
    EXPECT_HR_OK(IDXGISurface1_GetDC(s, FALSE, &dc));
    CHECK(dc != NULL && dc != (HDC)(ULONG_PTR)0x1234);
    if (dc) EXPECT_EQ(GetObjectType(dc), OBJ_MEMDC);
    EXPECT_HR_OK(IDXGISurface1_ReleaseDC(s, NULL));
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_getdc_discard)
{
    NEED_GDI(c, t, s);
    HDC dc = NULL;
    EXPECT_HR_OK(IDXGISurface1_GetDC(s, TRUE, &dc));
    CHECK(dc != NULL);
    EXPECT_HR_OK(IDXGISurface1_ReleaseDC(s, NULL));
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_getdc_twice)
{
    NEED_GDI(c, t, s);
    HDC dc = NULL, dc2 = (HDC)(ULONG_PTR)0x1234;
    EXPECT_HR_OK(IDXGISurface1_GetDC(s, FALSE, &dc));
    EXPECT_HR(IDXGISurface1_GetDC(s, FALSE, &dc2), DXGI_ERROR_INVALID_CALL);
    EXPECT_EQ(dc2, (HDC)(ULONG_PTR)0x1234);
    EXPECT_HR_OK(IDXGISurface1_ReleaseDC(s, NULL));
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_releasedc_without_getdc)
{
    NEED_GDI(c, t, s);
    EXPECT_HR(IDXGISurface1_ReleaseDC(s, NULL), DXGI_ERROR_INVALID_CALL);
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_releasedc_twice)
{
    NEED_GDI(c, t, s);
    HDC dc = NULL;
    EXPECT_HR_OK(IDXGISurface1_GetDC(s, FALSE, &dc));
    EXPECT_HR_OK(IDXGISurface1_ReleaseDC(s, NULL));
    EXPECT_HR(IDXGISurface1_ReleaseDC(s, NULL), DXGI_ERROR_INVALID_CALL);
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_getdc_cycle)
{
    NEED_GDI(c, t, s);
    int i;
    for (i = 0; i < 10; i++) {
        HDC dc = NULL; HRESULT hr = IDXGISurface1_GetDC(s, (i & 1) ? TRUE : FALSE, &dc);
        CHECKF(hr == S_OK && dc != NULL, "cycle %d: %s", i, d3dh_hr_name(hr));
        if (FAILED(hr)) break;
        hr = IDXGISurface1_ReleaseDC(s, NULL);
        CHECKF(hr == S_OK, "cycle %d release: %s", i, d3dh_hr_name(hr));
    }
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_getdc_null_out)
{
    NEED_GDI(c, t, s);
    EXPECT_HR(IDXGISurface1_GetDC(s, FALSE, NULL), E_INVALIDARG);
    /* a rejected call must not leave the surface in the acquired state */
    EXPECT_HR(IDXGISurface1_ReleaseDC(s, NULL), DXGI_ERROR_INVALID_CALL);
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_not_gdi_compatible)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGISurface1 *s = make_gdi_surface(&c, &t, 0, DXGI_FORMAT_B8G8R8A8_UNORM); HDC dc = (HDC)(ULONG_PTR)0x1234;
    if (!s) { t_skip("no surface"); objs_close(&c); return; }
    EXPECT_HR(IDXGISurface1_GetDC(s, FALSE, &dc), DXGI_ERROR_INVALID_CALL);
    EXPECT_EQ(dc, (HDC)(ULONG_PTR)0x1234);
    EXPECT_HR(IDXGISurface1_ReleaseDC(s, NULL), DXGI_ERROR_INVALID_CALL);
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_createsurface_not_compatible)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL; IDXGISurface1 *s1 = NULL; HDC dc = NULL;
    memset(&d, 0, sizeof(d)); d.Width = 16; d.Height = 16; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
    EXPECT_HR_OK(IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s));
    if (s && SUCCEEDED(IDXGISurface_QueryInterface(s, &IID_IDXGISurface1, (void **)&s1))) {
        HRESULT hr = IDXGISurface1_GetDC(s1, FALSE, &dc);
        t_note("GetDC on CreateSurface surface -> %s", d3dh_hr_name(hr));
        EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);   /* Win10 22H2 */
        if (SUCCEEDED(hr)) IDXGISurface1_ReleaseDC(s1, NULL);
        IDXGISurface1_Release(s1);
    }
    SAFE_RELEASE(s);
    objs_close(&c);
}
TEST(objs_gdi_releasedc_rect)
{
    NEED_GDI(c, t, s);
    static const RECT ok_rects[] = { {0, 0, 64, 64}, {0, 0, 1, 1}, {10, 10, 20, 20}, {5, 5, 5, 5}, {0, 0, 64, 1} };
    int i; HDC dc = NULL;
    for (i = 0; i < OBJS_ARRAY(ok_rects); i++) {
        HRESULT hr = IDXGISurface1_GetDC(s, FALSE, &dc);
        CHECKF(hr == S_OK, "rect %d GetDC: %s", i, d3dh_hr_name(hr));
        if (FAILED(hr)) break;
        hr = IDXGISurface1_ReleaseDC(s, (RECT *)&ok_rects[i]);
        CHECKF(hr == S_OK, "rect %d ReleaseDC: %s", i, d3dh_hr_name(hr));
    }
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_releasedc_bad_rect)
{
    NEED_GDI(c, t, s);
    static const RECT bad[] = { {10, 10, 5, 20}, {10, 10, 20, 5}, {0, 0, 65, 64}, {0, 0, 64, 65}, {0, 0, 1000, 1000} };
    HDC dc = NULL; int i;
    for (i = 0; i < OBJS_ARRAY(bad); i++) {
        EXPECT_HR_OK(IDXGISurface1_GetDC(s, FALSE, &dc));
        HRESULT hr = IDXGISurface1_ReleaseDC(s, (RECT *)&bad[i]);
        CHECKF(hr == E_INVALIDARG, "bad rect %d: %s", i, d3dh_hr_name(hr));
        /* the rejected release leaves the DC acquired */
        hr = IDXGISurface1_ReleaseDC(s, NULL);
        CHECKF(hr == S_OK, "rect %d: follow-up release: %s", i, d3dh_hr_name(hr));
    }
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_dc_bitmap_properties)
{
    NEED_GDI(c, t, s);
    HDC dc = NULL; DIBSECTION dib; HGDIOBJ bmp; int sz;
    EXPECT_HR_OK(IDXGISurface1_GetDC(s, FALSE, &dc));
    if (dc) {
        bmp = GetCurrentObject(dc, OBJ_BITMAP);
        EXPECT_EQ(GetObjectType(bmp), OBJ_BITMAP);
        memset(&dib, 0, sizeof(dib));
        sz = GetObjectA(bmp, sizeof(dib), &dib);
        CHECKF(sz == sizeof(dib) || sz == sizeof(dib.dsBm), "GetObject size %d", sz);
        EXPECT_EQ(dib.dsBm.bmType, 0);
        EXPECT_EQ(dib.dsBm.bmWidth, 64);
        EXPECT_EQ(dib.dsBm.bmHeight, 64);
        EXPECT_EQ(dib.dsBm.bmWidthBytes, 64 * 4);
        EXPECT_EQ(dib.dsBm.bmPlanes, 1);
        EXPECT_EQ(dib.dsBm.bmBitsPixel, 32);
        if (sz == sizeof(dib)) {
            CHECK(dib.dsBm.bmBits != NULL);
            EXPECT_EQ(dib.dsBmih.biSize, sizeof(BITMAPINFOHEADER));
            EXPECT_EQ(dib.dsBmih.biBitCount, 32);
            EXPECT_EQ(dib.dsBmih.biCompression, BI_RGB);
            EXPECT_EQ(dib.dsBmih.biPlanes, 1);
            EXPECT_EQ(dib.dshSection, NULL);
        }
        EXPECT_HR_OK(IDXGISurface1_ReleaseDC(s, NULL));
    }
    DONE_GDI(c, t, s);
}
TEST(objs_gdi_preserves_contents)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGISurface1 *s = make_gdi_surface(&c, &t, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, DXGI_FORMAT_B8G8R8A8_UNORM);
    ID3D11RenderTargetView *rtv = NULL; static const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f }; HDC dc = NULL;
    if (!s) { t_skip("no GDI surface"); objs_close(&c); return; }
    if (SUCCEEDED(ID3D11Device_CreateRenderTargetView(c.dev, (ID3D11Resource *)t, NULL, &rtv))) {
        ID3D11DeviceContext_ClearRenderTargetView(c.ctx, rtv, green);
        EXPECT_HR_OK(IDXGISurface1_GetDC(s, FALSE, &dc));
        if (dc) {
            COLORREF px = GetPixel(dc, 5, 5);
            EXPECT_EQ(px, RGB(0, 255, 0));
            IDXGISurface1_ReleaseDC(s, NULL);
        }
        ID3D11RenderTargetView_Release(rtv);
    }
    IDXGISurface1_Release(s); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_gdi_draw_reaches_texture)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL, *st = NULL; IDXGISurface1 *s = make_gdi_surface(&c, &t, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, DXGI_FORMAT_B8G8R8A8_UNORM);
    ID3D11RenderTargetView *rtv = NULL; static const float black[4] = { 0, 0, 0, 1 }; HDC dc = NULL; D3D11_MAPPED_SUBRESOURCE ms;
    if (!s) { t_skip("no GDI surface"); objs_close(&c); return; }
    if (FAILED(objs_staging(c.dev, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, &st)) || FAILED(ID3D11Device_CreateRenderTargetView(c.dev, (ID3D11Resource *)t, NULL, &rtv))) {
        t_skip("setup failed"); IDXGISurface1_Release(s); ID3D11Texture2D_Release(t); SAFE_RELEASE(st); objs_close(&c); return;
    }
    ID3D11DeviceContext_ClearRenderTargetView(c.ctx, rtv, black);
    EXPECT_HR_OK(IDXGISurface1_GetDC(s, FALSE, &dc));
    if (dc) {
        RECT r = { 8, 8, 24, 24 };
        HBRUSH br = CreateSolidBrush(RGB(0x12, 0x34, 0x56));
        FillRect(dc, &r, br);
        DeleteObject(br);
        GdiFlush();
        EXPECT_HR_OK(IDXGISurface1_ReleaseDC(s, NULL));
        ID3D11DeviceContext_CopyResource(c.ctx, (ID3D11Resource *)st, (ID3D11Resource *)t);
        if (SUCCEEDED(ID3D11DeviceContext_Map(c.ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &ms))) {
            UINT in = *(UINT *)((BYTE *)ms.pData + 12 * ms.RowPitch + 12 * 4) & 0x00ffffff;
            UINT out = *(UINT *)((BYTE *)ms.pData + 40 * ms.RowPitch + 40 * 4) & 0x00ffffff;
            EXPECT_EQ(in, 0x00123456u);
            EXPECT_EQ(out, 0x00000000u);
            ID3D11DeviceContext_Unmap(c.ctx, (ID3D11Resource *)st, 0);
        }
    }
    ID3D11RenderTargetView_Release(rtv); ID3D11Texture2D_Release(st);
    IDXGISurface1_Release(s); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_gdi_formats_creation)
{
    NEED_DEV(c);
    static const struct { DXGI_FORMAT f; int ok; const char *n; } tab[] = {
        { DXGI_FORMAT_B8G8R8A8_UNORM, 1, "B8G8R8A8_UNORM" }, { DXGI_FORMAT_B8G8R8A8_TYPELESS, 1, "B8G8R8A8_TYPELESS" },
        { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 1, "B8G8R8A8_UNORM_SRGB" }, { DXGI_FORMAT_B8G8R8X8_UNORM, 0, "B8G8R8X8_UNORM" },
        { DXGI_FORMAT_B8G8R8X8_TYPELESS, 0, "B8G8R8X8_TYPELESS" }, { DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, 0, "B8G8R8X8_UNORM_SRGB" },
    };
    int i;
    for (i = 0; i < OBJS_ARRAY(tab); i++) {
        ID3D11Texture2D *t = NULL;
        HRESULT hr = objs_tex2d(c.dev, 64, 64, 1, 1, tab[i].f, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, &t);
        if (tab[i].ok) CHECKF(hr == S_OK, "%s: %s", tab[i].n, d3dh_hr_name(hr));
        else CHECKF(FAILED(hr), "%s: %s", tab[i].n, d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
    objs_close(&c);
}
TEST(objs_gdi_staging_not_allowed)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL;
    HRESULT hr = objs_tex2d(c.dev, 64, 64, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, &t);
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(t);
    objs_close(&c);
}
TEST(objs_gdi_other_formats_observed)
{
    NEED_DEV(c);
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R32_FLOAT };
    int i;
    for (i = 0; i < OBJS_ARRAY(f); i++) {
        ID3D11Texture2D *t = NULL;
        HRESULT hr = objs_tex2d(c.dev, 64, 64, 1, 1, f[i], D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, &t);
        t_note("GDI_COMPATIBLE texture of format %d -> %s", f[i], d3dh_hr_name(hr));
        EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
        SAFE_RELEASE(t);
    }
    objs_close(&c);
}
TEST(objs_gdi_mips_array_observed)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL;
    HRESULT hr = objs_tex2d(c.dev, 64, 64, 4, 1, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, &t);
    t_note("GDI_COMPATIBLE with 4 mips -> %s", d3dh_hr_name(hr)); SAFE_RELEASE(t);
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
    hr = objs_tex2d(c.dev, 64, 64, 1, 2, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, &t);
    t_note("GDI_COMPATIBLE with array 2 -> %s", d3dh_hr_name(hr)); SAFE_RELEASE(t);
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    objs_close(&c);
}
TEST(objs_gdi_surface1_without_gdi_flag)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGISurface1 *s1 = NULL; IDXGISurface *s0 = NULL;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface1, (void **)&s1));
    if (s1) {
        EXPECT_HR_OK(IDXGISurface1_QueryInterface(s1, &IID_IDXGISurface, (void **)&s0));
        EXPECT_EQ(s0, (IDXGISurface *)s1);
        SAFE_RELEASE(s0);
        IDXGISurface1_Release(s1);
    }
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
