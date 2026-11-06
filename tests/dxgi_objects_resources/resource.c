/* IDXGIResource / IDXGIResource1 over D3D11 resources: usage, eviction priority, interface identity,
 * shared handles, subresource surfaces. */
#include "objs.h"


/* ---- GetUsage ------------------------------------------------------------------------------- */
static DXGI_USAGE expected_usage(UINT bind)
{
    DXGI_USAGE u = 0;
    if (bind & D3D11_BIND_RENDER_TARGET) u |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
    if (bind & D3D11_BIND_SHADER_RESOURCE) u |= DXGI_USAGE_SHADER_INPUT;
    if (bind & D3D11_BIND_UNORDERED_ACCESS) u |= DXGI_USAGE_UNORDERED_ACCESS;
    return u;
}
static void check_usage(IUnknown *res, UINT bind, const char *what)
{
    IDXGIResource *r = NULL; DXGI_USAGE u = 0xdeadbeef;
    HRESULT hr = IUnknown_QueryInterface(res, &IID_IDXGIResource, (void **)&r);
    CHECKF(hr == S_OK, "%s bind %#x: QI IDXGIResource %s", what, bind, d3dh_hr_name(hr));
    if (!r) return;
    hr = IDXGIResource_GetUsage(r, &u);
    CHECKF(hr == S_OK, "%s bind %#x: GetUsage %s", what, bind, d3dh_hr_name(hr));
    CHECKF(u == expected_usage(bind), "%s bind %#x: usage %#x want %#x", what, bind, u, expected_usage(bind));
    IDXGIResource_Release(r);
}

static const UINT tex_binds[] = {
    0, D3D11_BIND_SHADER_RESOURCE, D3D11_BIND_RENDER_TARGET, D3D11_BIND_UNORDERED_ACCESS,
    D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
    D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS,
    D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
};
TEST(objs_res_usage_texture2d)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(tex_binds); i++) {
        ID3D11Texture2D *t = NULL;
        if ((tex_binds[i] & D3D11_BIND_UNORDERED_ACCESS) && c.fl < D3D_FEATURE_LEVEL_11_0) continue;
        if (FAILED(objs_tex2d(c.dev, 64, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, tex_binds[i], 0, 0, &t))) {
            t_note("cannot create texture with bind %#x", tex_binds[i]); continue;
        }
        check_usage((IUnknown *)t, tex_binds[i], "tex2d");
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_usage_texture2d_mips)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(tex_binds); i++) {
        ID3D11Texture2D *t = NULL;
        if ((tex_binds[i] & D3D11_BIND_UNORDERED_ACCESS) && c.fl < D3D_FEATURE_LEVEL_11_0) continue;
        if (FAILED(objs_tex2d(c.dev, 64, 64, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, tex_binds[i], 0, 0, &t))) continue;
        check_usage((IUnknown *)t, tex_binds[i], "tex2d-mips");
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_usage_texture2d_depth)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL;
    if (FAILED(objs_tex2d(c.dev, 64, 64, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0, 0, &t))) {
        t_skip("no depth texture"); objs_close(&c); return;
    }
    check_usage((IUnknown *)t, D3D11_BIND_DEPTH_STENCIL, "tex2d-ds");
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_usage_texture1d)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(tex_binds); i++) {
        D3D11_TEXTURE1D_DESC d; ID3D11Texture1D *t = NULL;
        if ((tex_binds[i] & D3D11_BIND_UNORDERED_ACCESS) && c.fl < D3D_FEATURE_LEVEL_11_0) continue;
        memset(&d, 0, sizeof(d)); d.Width = 64; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = tex_binds[i];
        if (FAILED(ID3D11Device_CreateTexture1D(c.dev, &d, NULL, &t))) continue;
        check_usage((IUnknown *)t, tex_binds[i], "tex1d");
        ID3D11Texture1D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_usage_texture3d)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(tex_binds); i++) {
        D3D11_TEXTURE3D_DESC d; ID3D11Texture3D *t = NULL;
        if ((tex_binds[i] & D3D11_BIND_UNORDERED_ACCESS) && c.fl < D3D_FEATURE_LEVEL_11_0) continue;
        memset(&d, 0, sizeof(d)); d.Width = 16; d.Height = 16; d.Depth = 4; d.MipLevels = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = tex_binds[i];
        if (FAILED(ID3D11Device_CreateTexture3D(c.dev, &d, NULL, &t))) continue;
        check_usage((IUnknown *)t, tex_binds[i], "tex3d");
        ID3D11Texture3D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_usage_buffer)
{
    NEED_DEV(c);
    static const UINT binds[] = { D3D11_BIND_VERTEX_BUFFER, D3D11_BIND_INDEX_BUFFER, D3D11_BIND_CONSTANT_BUFFER,
                                  D3D11_BIND_SHADER_RESOURCE, D3D11_BIND_STREAM_OUTPUT, D3D11_BIND_UNORDERED_ACCESS };
    int i;
    for (i = 0; i < OBJS_ARRAY(binds); i++) {
        ID3D11Buffer *b = NULL;
        if ((binds[i] & D3D11_BIND_UNORDERED_ACCESS) && c.fl < D3D_FEATURE_LEVEL_11_0) continue;
        if (FAILED(objs_buf(c.dev, 256, D3D11_USAGE_DEFAULT, binds[i], 0, 0, &b))) { t_note("buffer bind %#x not creatable", binds[i]); continue; }
        check_usage((IUnknown *)b, binds[i], "buffer");
        ID3D11Buffer_Release(b);
    }
    objs_close(&c);
}
TEST(objs_res_usage_staging_dynamic)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; DXGI_USAGE u = 0xdead;
    if (SUCCEEDED(objs_staging(c.dev, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, &t))) {
        EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
        EXPECT_HR_OK(IDXGIResource_GetUsage(r, &u));
        EXPECT_EQ(u, DXGI_CPU_ACCESS_SCRATCH);   /* a staging texture reports the "scratch" CPU-access field (3) */
        IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    }
    if (SUCCEEDED(objs_tex2d(c.dev, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE, 0, &t))) {
        EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
        EXPECT_HR_OK(IDXGIResource_GetUsage(r, &u));
        EXPECT_EQ(u, DXGI_USAGE_SHADER_INPUT | DXGI_CPU_ACCESS_DYNAMIC);   /* dynamic: CPU-access field 1 */
        IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_usage_surface_createsurface)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL; IDXGIResource *r = NULL; DXGI_USAGE u = 0xdead;
    memset(&d, 0, sizeof(d)); d.Width = 16; d.Height = 16; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
    EXPECT_HR_OK(IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s));
    if (s) {
        EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGIResource, (void **)&r));
        EXPECT_HR_OK(IDXGIResource_GetUsage(r, &u));
        CHECKF(u & DXGI_USAGE_RENDER_TARGET_OUTPUT, "usage %#x", u);
        IDXGIResource_Release(r); IDXGISurface_Release(s);
    }
    objs_close(&c);
}

/* ---- interface support per resource kind ------------------------------------------------------------- */
static int supports(IUnknown *o, const GUID *iid)
{
    IUnknown *p = NULL; HRESULT hr = IUnknown_QueryInterface(o, iid, (void **)&p);
    if (SUCCEEDED(hr)) IUnknown_Release(p); else if (p) t_check(0, __FILE__, __LINE__, "out set on failure", NULL);
    return SUCCEEDED(hr);
}
TEST(objs_res_iface_texture2d_single)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (t) {
        IUnknown *u = (IUnknown *)t;
        CHECK(supports(u, &IID_IDXGIObject));
        CHECK(supports(u, &IID_IDXGIDeviceSubObject));
        CHECK(supports(u, &IID_IDXGIResource));
        CHECK(supports(u, &IID_IDXGISurface));
        CHECK(supports(u, &IID_ID3D11Resource));
        CHECK(supports(u, &IID_ID3D11DeviceChild));
        CHECK(supports(u, &IID_IUnknown));
        CHECK(!supports(u, &IID_ID3D11Buffer));
        CHECK(!supports(u, &IID_ID3D11Texture1D));
        CHECK(!supports(u, &IID_IDXGIDevice));
        CHECK(!supports(u, &IID_IDXGIKeyedMutex));
        t_note("tex2d IDXGIResource1: %d, IDXGISurface1: %d, IDXGISurface2: %d", supports(u, &IID_IDXGIResource1), supports(u, &IID_IDXGISurface1), supports(u, &IID_IDXGISurface2));
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_iface_texture2d_resource1)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (t) {
        CHECK(supports((IUnknown *)t, &IID_IDXGIResource1));
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_iface_texture2d_surface1)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (t) {
        CHECK(supports((IUnknown *)t, &IID_IDXGISurface1));
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_iface_texture2d_mipped_no_surface)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL;
    if (FAILED(objs_tex2d(c.dev, 64, 64, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0, &t))) { t_skip("no texture"); objs_close(&c); return; }
    CHECK(supports((IUnknown *)t, &IID_IDXGIResource));
    CHECK(!supports((IUnknown *)t, &IID_IDXGISurface));
    CHECK(!supports((IUnknown *)t, &IID_IDXGISurface1));
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_iface_texture2d_array_no_surface)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL;
    if (FAILED(objs_tex2d(c.dev, 64, 64, 1, 2, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0, &t))) { t_skip("no texture"); objs_close(&c); return; }
    CHECK(supports((IUnknown *)t, &IID_IDXGIResource));
    CHECK(!supports((IUnknown *)t, &IID_IDXGISurface));
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_iface_texture3d)
{
    NEED_DEV(c);
    D3D11_TEXTURE3D_DESC d; ID3D11Texture3D *t = NULL;
    memset(&d, 0, sizeof(d)); d.Width = 8; d.Height = 8; d.Depth = 8; d.MipLevels = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(ID3D11Device_CreateTexture3D(c.dev, &d, NULL, &t))) { t_skip("no 3d texture"); objs_close(&c); return; }
    CHECK(supports((IUnknown *)t, &IID_IDXGIResource));
    CHECK(supports((IUnknown *)t, &IID_IDXGIObject));
    CHECK(!supports((IUnknown *)t, &IID_IDXGISurface));
    ID3D11Texture3D_Release(t);
    objs_close(&c);
}
TEST(objs_res_iface_buffer)
{
    NEED_DEV(c);
    ID3D11Buffer *b = NULL;
    EXPECT_HR_OK(objs_buf(c.dev, 128, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, &b));
    if (b) {
        CHECK(supports((IUnknown *)b, &IID_IDXGIResource));
        CHECK(supports((IUnknown *)b, &IID_IDXGIObject));
        CHECK(supports((IUnknown *)b, &IID_IDXGIDeviceSubObject));
        CHECK(supports((IUnknown *)b, &IID_IDXGISurface));     /* Win10 exposes a one-row byte surface on buffers */
        CHECK(supports((IUnknown *)b, &IID_IDXGISurface1));
        CHECK(!supports((IUnknown *)b, &IID_IDXGIKeyedMutex));
        ID3D11Buffer_Release(b);
    }
    objs_close(&c);
}
TEST(objs_res_iface_texture1d)
{
    NEED_DEV(c);
    D3D11_TEXTURE1D_DESC d; ID3D11Texture1D *t = NULL;
    memset(&d, 0, sizeof(d)); d.Width = 64; d.MipLevels = 0; d.ArraySize = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(ID3D11Device_CreateTexture1D(c.dev, &d, NULL, &t))) { t_skip("no 1d texture"); objs_close(&c); return; }
    CHECK(supports((IUnknown *)t, &IID_IDXGIResource));
    CHECK(!supports((IUnknown *)t, &IID_IDXGISurface));
    d.MipLevels = 1;
    ID3D11Texture1D_Release(t);
    if (SUCCEEDED(ID3D11Device_CreateTexture1D(c.dev, &d, NULL, &t))) {
        CHECK(supports((IUnknown *)t, &IID_IDXGIResource));
        t_note("1D texture (1 mip) IDXGISurface: %d", supports((IUnknown *)t, &IID_IDXGISurface));
        ID3D11Texture1D_Release(t);
    }
    objs_close(&c);
}

/* ---- identity among DXGI views of one texture ----------------------------------------------------------- */
TEST(objs_res_identity_pointers)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL, *r2 = NULL; IDXGISurface *s = NULL, *s2 = NULL; IUnknown *u = NULL; IDXGIObject *o = NULL;
    IDXGIDeviceSubObject *so = NULL;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&s));
    EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGIDeviceSubObject, (void **)&so));
    EXPECT_EQ(so, (IDXGIDeviceSubObject *)r);
    EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGIObject, (void **)&o));
    EXPECT_EQ(o, (IDXGIObject *)r);
    IDXGIObject_Release(o);
    EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGIResource, (void **)&r2));
    EXPECT_EQ(r2, r);
    IDXGIResource_Release(r2);
    EXPECT_HR_OK(IDXGIResource_QueryInterface(r, &IID_IDXGISurface, (void **)&s2));
    EXPECT_EQ(s2, s);
    IDXGISurface_Release(s2);
    EXPECT_HR_OK(IDXGIResource_QueryInterface(r, &IID_IUnknown, (void **)&u));
    {
        IUnknown *u2 = NULL;
        EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IUnknown, (void **)&u2));
        EXPECT_EQ(u, u2);
        SAFE_RELEASE(u2);
    }
    SAFE_RELEASE(u);
    IDXGIDeviceSubObject_Release(so); IDXGISurface_Release(s); IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_identity_private_data_shared)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; IDXGISurface *s = NULL; DWORD v = 123, out = 0; UINT size = sizeof(out);
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&s));
    EXPECT_HR_OK(IDXGIResource_SetPrivateData(r, &IID_IUnknown, sizeof(v), &v));
    EXPECT_HR_OK(IDXGISurface_GetPrivateData(s, &IID_IUnknown, &size, &out));
    EXPECT_EQ(out, 123);
    EXPECT_HR(IDXGISurface_SetPrivateData(s, &IID_IUnknown, 0, NULL), S_OK);
    size = sizeof(out);
    EXPECT_HR(IDXGIResource_GetPrivateData(r, &IID_IUnknown, &size, &out), DXGI_ERROR_NOT_FOUND);
    IDXGISurface_Release(s); IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_identity_resource1)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; IDXGIResource1 *r1 = NULL;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    if (SUCCEEDED(IDXGIResource_QueryInterface(r, &IID_IDXGIResource1, (void **)&r1))) {
        EXPECT_EQ(r1, (IDXGIResource1 *)r);
        IDXGIResource1_Release(r1);
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_refcount_through_views)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; ULONG a, b;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    a = d3dh_refcount(t);
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    b = d3dh_refcount(t);
    EXPECT_EQ(b, a + 1);
    EXPECT_EQ(d3dh_refcount(r), b);
    IDXGIResource_Release(r);
    EXPECT_EQ(d3dh_refcount(t), a);
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}

/* ---- eviction priority ------------------------------------------------------------------------------------ */
static const UINT canon_prio[] = { 0x28000000, 0x50000000, 0x78000000, 0xa0000000, 0xc8000000 };
TEST(objs_res_eviction_default)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; UINT p = 0xdead;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    EXPECT_HR_OK(IDXGIResource_GetEvictionPriority(r, &p));
    EXPECT_EQ(p, DXGI_RESOURCE_PRIORITY_NORMAL);
    EXPECT_EQ(ID3D11Texture2D_GetEvictionPriority(t), p);
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_eviction_canonical_roundtrip)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; int i;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    for (i = 0; i < OBJS_ARRAY(canon_prio); i++) {
        UINT p = 0xdead;
        HRESULT hr = IDXGIResource_SetEvictionPriority(r, canon_prio[i]);
        CHECKF(hr == S_OK, "set %#x: %s", canon_prio[i], d3dh_hr_name(hr));
        hr = IDXGIResource_GetEvictionPriority(r, &p);
        CHECKF(hr == S_OK && p == canon_prio[i], "get after set %#x: %s %#x", canon_prio[i], d3dh_hr_name(hr), p);
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_eviction_d3d11_view)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; int i;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    for (i = 0; i < OBJS_ARRAY(canon_prio); i++) {
        UINT p = 0xdead;
        ID3D11Texture2D_SetEvictionPriority(t, canon_prio[i]);
        IDXGIResource_GetEvictionPriority(r, &p);
        CHECKF(p == canon_prio[i], "D3D11 set %#x, DXGI get %#x", canon_prio[i], p);
        CHECKF(ID3D11Texture2D_GetEvictionPriority(t) == canon_prio[i], "D3D11 get %#x", ID3D11Texture2D_GetEvictionPriority(t));
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_eviction_noncanonical)
{
    NEED_DEV(c);
    static const UINT vals[] = { 0, 1, 0x28000001, 0x4fffffff, 0x60000000, 0x78000000 + 1, 0xc8000001, 0xffffffffu };
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; int i;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    for (i = 0; i < OBJS_ARRAY(vals); i++) {
        UINT p = 0xdead; HRESULT hr = IDXGIResource_SetEvictionPriority(r, vals[i]);
        HRESULT hg = IDXGIResource_GetEvictionPriority(r, &p);
        EXPECT_HR_OK(hr); EXPECT_HR_OK(hg); EXPECT_EQ(p, vals[i]);   /* any value, canonical or not, is stored verbatim */
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_eviction_buffer)
{
    NEED_DEV(c);
    ID3D11Buffer *b = NULL; IDXGIResource *r = NULL; UINT p = 0xdead;
    EXPECT_HR_OK(objs_buf(c.dev, 128, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, &b));
    if (!b) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Buffer_QueryInterface(b, &IID_IDXGIResource, (void **)&r));
    EXPECT_HR_OK(IDXGIResource_SetEvictionPriority(r, 0xa0000000));
    EXPECT_HR_OK(IDXGIResource_GetEvictionPriority(r, &p));
    EXPECT_EQ(p, 0xa0000000);
    IDXGIResource_Release(r); ID3D11Buffer_Release(b);
    objs_close(&c);
}
TEST(objs_res_eviction_independent)
{
    NEED_DEV(c);
    ID3D11Texture2D *a = NULL, *b = NULL; IDXGIResource *ra = NULL, *rb = NULL; UINT pa = 0, pb = 0;
    EXPECT_HR_OK(objs_rt(c.dev, &a)); EXPECT_HR_OK(objs_rt(c.dev, &b));
    if (!a || !b) { SAFE_RELEASE(a); SAFE_RELEASE(b); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(a, &IID_IDXGIResource, (void **)&ra);
    ID3D11Texture2D_QueryInterface(b, &IID_IDXGIResource, (void **)&rb);
    IDXGIResource_GetEvictionPriority(rb, &pb);
    EXPECT_HR_OK(IDXGIResource_SetEvictionPriority(ra, 0xc8000000));
    IDXGIResource_GetEvictionPriority(ra, &pa);
    EXPECT_EQ(pa, 0xc8000000);
    pa = 0;
    IDXGIResource_GetEvictionPriority(rb, &pa);
    EXPECT_EQ(pa, pb);
    IDXGIResource_Release(ra); IDXGIResource_Release(rb); ID3D11Texture2D_Release(a); ID3D11Texture2D_Release(b);
    objs_close(&c);
}

/* ---- GetSharedHandle ------------------------------------------------------------------------------------------ */
TEST(objs_res_sharedhandle_not_shared)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; HANDLE h = (HANDLE)(ULONG_PTR)0xdeadbeef;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    HRESULT hr = IDXGIResource_GetSharedHandle(r, &h);
    EXPECT_HR_OK(hr);
    EXPECT_EQ(h, NULL);
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_sharedhandle_buffer_not_shared)
{
    NEED_DEV(c);
    ID3D11Buffer *b = NULL; IDXGIResource *r = NULL; HANDLE h = (HANDLE)(ULONG_PTR)0xdeadbeef;
    EXPECT_HR_OK(objs_buf(c.dev, 64, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, &b));
    if (!b) { objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Buffer_QueryInterface(b, &IID_IDXGIResource, (void **)&r));
    EXPECT_HR_OK(IDXGIResource_GetSharedHandle(r, &h));
    EXPECT_EQ(h, NULL);
    IDXGIResource_Release(r); ID3D11Buffer_Release(b);
    objs_close(&c);
}

typedef struct { UINT misc; int valid; int nt; int keyed; } shared_case;
static const shared_case shared_cases[] = {
    { 0, 1, 0, 0 },
    { D3D11_RESOURCE_MISC_SHARED, 1, 0, 0 },
    { D3D11_RESOURCE_MISC_SHARED_NTHANDLE, 0, 1, 0 },
    { D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE, 1, 1, 0 },
    { D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX, 1, 0, 1 },
    { D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE, 1, 1, 1 },
    { D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED, 0, 0, 1 },
    { D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE, 0, 1, 1 },
};
static HRESULT make_shared_tex(objs_ctx *c, UINT misc, ID3D11Texture2D **t)
{
    return objs_tex2d(c->dev, 256, 256, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, misc, t);
}
TEST(objs_res_shared_flag_validity)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(shared_cases); i++) {
        ID3D11Texture2D *t = NULL; HRESULT hr = make_shared_tex(&c, shared_cases[i].misc, &t);
        if (shared_cases[i].valid) CHECKF(hr == S_OK, "misc %#x: %s", shared_cases[i].misc, d3dh_hr_name(hr));
        else CHECKF(hr == E_INVALIDARG, "misc %#x: %s", shared_cases[i].misc, d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
    objs_close(&c);
}
TEST(objs_res_shared_keyedmutex_qi)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(shared_cases); i++) {
        ID3D11Texture2D *t = NULL; IUnknown *km = (IUnknown *)(ULONG_PTR)1; HRESULT hr;
        if (!shared_cases[i].valid || FAILED(make_shared_tex(&c, shared_cases[i].misc, &t))) continue;
        hr = ID3D11Texture2D_QueryInterface(t, &IID_IDXGIKeyedMutex, (void **)&km);
        if (shared_cases[i].keyed) { CHECKF(hr == S_OK, "misc %#x: %s", shared_cases[i].misc, d3dh_hr_name(hr)); SAFE_RELEASE(km); }
        else { CHECKF(hr == E_NOINTERFACE && km == NULL, "misc %#x: %s", shared_cases[i].misc, d3dh_hr_name(hr)); }
        ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_shared_getsharedhandle_matrix)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(shared_cases); i++) {
        ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; HANDLE h = (HANDLE)(ULONG_PTR)0xdeadbeef, h2 = NULL; HRESULT hr;
        if (!shared_cases[i].valid || FAILED(make_shared_tex(&c, shared_cases[i].misc, &t))) continue;
        ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
        hr = IDXGIResource_GetSharedHandle(r, &h);
        if (shared_cases[i].nt) {
            CHECKF(hr == E_INVALIDARG, "misc %#x: %s", shared_cases[i].misc, d3dh_hr_name(hr));
            CHECKF(h == (HANDLE)(ULONG_PTR)0xdeadbeef, "misc %#x: handle modified %p", shared_cases[i].misc, h);
        } else if (shared_cases[i].misc) {
            CHECKF(hr == S_OK, "misc %#x: %s", shared_cases[i].misc, d3dh_hr_name(hr));
            CHECKF(h != NULL && ((ULONG_PTR)h & 0xc0000000), "misc %#x: not a KMT handle %p", shared_cases[i].misc, h);
            IDXGIResource_GetSharedHandle(r, &h2);
            CHECKF(h2 == h, "misc %#x: handle not stable %p vs %p", shared_cases[i].misc, h, h2);
        } else {
            CHECKF(hr == S_OK && h == NULL, "misc %#x: %s %p", shared_cases[i].misc, d3dh_hr_name(hr), h);
        }
        IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_shared_createsharedhandle_matrix)
{
    NEED_DEV(c);
    int i;
    for (i = 0; i < OBJS_ARRAY(shared_cases); i++) {
        ID3D11Texture2D *t = NULL; IDXGIResource1 *r = NULL; HANDLE h = (HANDLE)(ULONG_PTR)0xdeadbeef; HRESULT hr;
        if (!shared_cases[i].valid || !shared_cases[i].misc || FAILED(make_shared_tex(&c, shared_cases[i].misc, &t))) continue;
        if (FAILED(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource1, (void **)&r))) { ID3D11Texture2D_Release(t); continue; }
        hr = IDXGIResource1_CreateSharedHandle(r, NULL, GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &h);
        if (shared_cases[i].nt) {
            CHECKF(hr == S_OK, "misc %#x: %s", shared_cases[i].misc, d3dh_hr_name(hr));
            CHECKF(h != NULL && h != (HANDLE)(ULONG_PTR)0xdeadbeef, "misc %#x: handle %p", shared_cases[i].misc, h);
            if (SUCCEEDED(hr) && h) {
                DWORD flags = 0;
                CHECKF(GetHandleInformation(h, &flags), "misc %#x: not a process handle", shared_cases[i].misc);
                CloseHandle(h);
            }
        } else {
            CHECKF(hr == E_INVALIDARG, "misc %#x: %s", shared_cases[i].misc, d3dh_hr_name(hr));
            CHECKF(h == NULL, "misc %#x: handle %p", shared_cases[i].misc, h);
        }
        IDXGIResource1_Release(r); ID3D11Texture2D_Release(t);
    }
    objs_close(&c);
}
TEST(objs_res_shared_createsharedhandle_unshared)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource1 *r = NULL; HANDLE h = (HANDLE)(ULONG_PTR)0xdeadbeef;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t || FAILED(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource1, (void **)&r))) { SAFE_RELEASE(t); t_skip("no IDXGIResource1"); objs_close(&c); return; }
    HRESULT hr = IDXGIResource1_CreateSharedHandle(r, NULL, GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &h);
    EXPECT_HR(hr, E_INVALIDARG);
    EXPECT_EQ(h, NULL);
    IDXGIResource1_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_shared_open_matrix)
{
    NEED_DEV(c);
    ID3D11Device1 *d1 = NULL; int i;
    if (FAILED(ID3D11Device_QueryInterface(c.dev, &IID_ID3D11Device1, (void **)&d1))) { t_skip("no ID3D11Device1"); objs_close(&c); return; }
    for (i = 0; i < OBJS_ARRAY(shared_cases); i++) {
        ID3D11Texture2D *t = NULL, *o = NULL; IDXGIResource *r = NULL; IDXGIResource1 *r1 = NULL; HANDLE h = NULL; HRESULT hr;
        if (!shared_cases[i].valid || !shared_cases[i].misc || FAILED(make_shared_tex(&c, shared_cases[i].misc, &t))) continue;
        ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
        ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource1, (void **)&r1);
        if (shared_cases[i].nt) {
            if (!r1 || FAILED(IDXGIResource1_CreateSharedHandle(r1, NULL, GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &h))) goto next;
        } else IDXGIResource_GetSharedHandle(r, &h);
        hr = ID3D11Device_OpenSharedResource(c.dev, h, &IID_ID3D11Texture2D, (void **)&o);
        CHECKF(hr == (shared_cases[i].nt ? E_INVALIDARG : S_OK), "misc %#x OpenSharedResource: %s", shared_cases[i].misc, d3dh_hr_name(hr));
        SAFE_RELEASE(o);
        hr = ID3D11Device1_OpenSharedResource1(d1, h, &IID_ID3D11Texture2D, (void **)&o);
        CHECKF(hr == (shared_cases[i].nt ? S_OK : E_INVALIDARG), "misc %#x OpenSharedResource1: %s", shared_cases[i].misc, d3dh_hr_name(hr));
        if (o) {
            D3D11_TEXTURE2D_DESC d; ID3D11Texture2D_GetDesc(o, &d);
            CHECKF(d.Width == 256 && d.Height == 256 && d.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "opened desc %ux%u", d.Width, d.Height);
            ID3D11Texture2D_Release(o);
        }
        if (shared_cases[i].nt) CHECK(CloseHandle(h));
next:
        SAFE_RELEASE(r); SAFE_RELEASE(r1); ID3D11Texture2D_Release(t);
    }
    ID3D11Device1_Release(d1);
    objs_close(&c);
}
TEST(objs_res_shared_open_second_device)
{
    NEED_DEV(c);
    objs_ctx c2; ID3D11Texture2D *t = NULL, *o = NULL; IDXGIResource *r = NULL; HANDLE h = NULL;
    if (!objs_open(&c2)) { t_skip("second device"); objs_close(&c); return; }
    if (FAILED(make_shared_tex(&c, D3D11_RESOURCE_MISC_SHARED, &t))) { t_skip("shared textures unsupported"); objs_close(&c2); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    EXPECT_HR_OK(IDXGIResource_GetSharedHandle(r, &h));
    EXPECT_HR_OK(ID3D11Device_OpenSharedResource(c2.dev, h, &IID_ID3D11Texture2D, (void **)&o));
    if (o) {
        D3D11_TEXTURE2D_DESC a, b; ID3D11Device *dev = NULL;
        ID3D11Texture2D_GetDesc(t, &a); ID3D11Texture2D_GetDesc(o, &b);
        EXPECT_EQ(b.Width, a.Width); EXPECT_EQ(b.Height, a.Height); EXPECT_EQ(b.Format, a.Format);
        EXPECT_EQ(b.MiscFlags & D3D11_RESOURCE_MISC_SHARED, D3D11_RESOURCE_MISC_SHARED);
        ID3D11Texture2D_GetDevice(o, &dev);
        EXPECT_EQ(dev, c2.dev);
        SAFE_RELEASE(dev);
        /* the opened resource reports the same share handle */
        {
            IDXGIResource *ro = NULL; HANDLE h2 = NULL;
            ID3D11Texture2D_QueryInterface(o, &IID_IDXGIResource, (void **)&ro);
            if (ro) { IDXGIResource_GetSharedHandle(ro, &h2); EXPECT_EQ(h2, h); IDXGIResource_Release(ro); }
        }
        ID3D11Texture2D_Release(o);
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c2); objs_close(&c);
}
TEST(objs_res_shared_open_wrong_iid)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; HANDLE h = NULL; IUnknown *p = (IUnknown *)(ULONG_PTR)1;
    if (FAILED(make_shared_tex(&c, D3D11_RESOURCE_MISC_SHARED, &t))) { t_skip("shared textures unsupported"); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    IDXGIResource_GetSharedHandle(r, &h);
    HRESULT hr = ID3D11Device_OpenSharedResource(c.dev, h, &IID_ID3D11Buffer, (void **)&p);
    EXPECT_HR_FAILED(hr);
    t_note("OpenSharedResource(texture handle, IID_ID3D11Buffer) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_NOINTERFACE);   /* Win10 22H2 */
    CHECK(p == NULL);
    hr = ID3D11Device_OpenSharedResource(c.dev, h, &IID_IDXGIFactory, (void **)&p);
    t_note("OpenSharedResource(texture handle, IID_IDXGIFactory) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_NOINTERFACE);   /* Win10 22H2 */
    SAFE_RELEASE(p);
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_res_shared_open_null_handle)
{
    NEED_DEV(c);
    ID3D11Texture2D *o = (ID3D11Texture2D *)(ULONG_PTR)1;
    HRESULT hr = ID3D11Device_OpenSharedResource(c.dev, NULL, &IID_ID3D11Texture2D, (void **)&o);
    EXPECT_HR_FAILED(hr);
    t_note("OpenSharedResource(NULL handle) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
    if (SUCCEEDED(hr) && o) ID3D11Texture2D_Release(o);
    objs_close(&c);
}

/* ---- CreateSubresourceSurface ------------------------------------------------------------------------------------------------ */
typedef struct { int type; UINT w, h, pitch, mips, arr; HRESULT hr; } sub_case;
static const sub_case sub_cases[] = {
    { 0, 512, 1, 512, 1, 1, S_OK },
    { 1, 512, 1, 2048, 1, 1, S_OK },
    { 1, 512, 1, 2048, 4, 4, S_OK },
    { 2, 512, 512, 2048, 1, 1, S_OK },
    { 2, 512, 512, 2048, 4, 4, S_OK },
    { 3, 512, 512, 2048, 1, 1, S_OK },
    { 3, 512, 512, 2048, 2, 1, S_OK },
    { 3, 512, 512, 2048, 1, 2, E_INVALIDARG },
};
static int make_sub(objs_ctx *c, const sub_case *k, IDXGIResource1 **out)
{
    IUnknown *obj = NULL; HRESULT hr = E_FAIL;
    *out = NULL;
    if (k->type == 0) {
        ID3D11Buffer *b = NULL;
        if (SUCCEEDED(objs_buf(c->dev, k->w, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0, &b))) obj = (IUnknown *)b;
    } else if (k->type == 1) {
        D3D11_TEXTURE1D_DESC d; ID3D11Texture1D *t = NULL;
        memset(&d, 0, sizeof(d)); d.Width = k->w; d.MipLevels = k->mips; d.ArraySize = k->arr; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (SUCCEEDED(ID3D11Device_CreateTexture1D(c->dev, &d, NULL, &t))) obj = (IUnknown *)t;
    } else if (k->type == 2) {
        ID3D11Texture2D *t = NULL;
        if (SUCCEEDED(objs_tex2d(c->dev, k->w, k->h, k->mips, k->arr, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0, &t))) obj = (IUnknown *)t;
    } else {
        D3D11_TEXTURE3D_DESC d; ID3D11Texture3D *t = NULL;
        memset(&d, 0, sizeof(d)); d.Width = k->w; d.Height = k->h; d.Depth = k->arr; d.MipLevels = k->mips; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (SUCCEEDED(ID3D11Device_CreateTexture3D(c->dev, &d, NULL, &t))) obj = (IUnknown *)t;
    }
    if (!obj) return 0;
    hr = IUnknown_QueryInterface(obj, &IID_IDXGIResource1, (void **)out);
    IUnknown_Release(obj);
    return SUCCEEDED(hr);
}
static void sub_one(objs_ctx *c, int idx)
{
    const sub_case *k = &sub_cases[idx]; IDXGIResource1 *res = NULL; UINT n = k->mips * k->arr, j;
    if (!make_sub(c, k, &res)) { t_skip("cannot create resource for case %d", idx); return; }
    for (j = 0; j < n; j++) {
        IDXGISurface2 *s = NULL; ULONG r0, r1; HRESULT hr;
        r0 = d3dh_refcount(res);
        hr = IDXGIResource1_CreateSubresourceSurface(res, j, &s);
        CHECKF(hr == k->hr, "case %d sub %u: %s", idx, j, d3dh_hr_name(hr));
        if (FAILED(hr)) { CHECK(s == NULL); continue; }
        r1 = d3dh_refcount(res);
        CHECKF(r1 == r0 + 1, "case %d sub %u: refcount %lu -> %lu", idx, j, r0, r1);
        {
            DXGI_SURFACE_DESC d; UINT shift = j % k->mips, ew = k->w >> shift, eh = (k->type <= 1) ? 1 : (k->h >> shift);
            if (!ew) ew = 1;
            if (!eh) eh = 1;
            memset(&d, 0xcc, sizeof(d));
            EXPECT_HR_OK(IDXGISurface2_GetDesc(s, &d));
            CHECKF(d.Width == ew && d.Height == eh, "case %d sub %u: %ux%u want %ux%u", idx, j, d.Width, d.Height, ew, eh);
            CHECKF(d.Format == (k->type ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_UNKNOWN), "case %d sub %u: fmt %d", idx, j, d.Format);
            CHECKF(d.SampleDesc.Count == 1 && d.SampleDesc.Quality == 0, "case %d sub %u: sample", idx, j);
        }
        {
            DXGI_MAPPED_RECT m; memset(&m, 0, sizeof(m));
            hr = IDXGISurface2_Map(s, &m, DXGI_MAP_READ);
            CHECKF(hr == S_OK, "case %d sub %u: Map %s", idx, j, d3dh_hr_name(hr));
            if (SUCCEEDED(hr)) {
                CHECKF(m.pBits != NULL, "case %d sub %u: pBits NULL", idx, j);
                CHECKF(m.Pitch >= (k->w >> (j % k->mips)) * (k->type ? 4 : 1) || m.Pitch >= 1, "case %d sub %u: pitch %d", idx, j, m.Pitch);
                IDXGISurface2_Unmap(s);
            }
        }
        {
            IDXGIResource1 *parent = NULL; UINT idxout = 0xdead;
            hr = IDXGISurface2_GetResource(s, &IID_IDXGIResource1, (void **)&parent, &idxout);
            CHECKF(hr == S_OK, "case %d sub %u: GetResource %s", idx, j, d3dh_hr_name(hr));
            CHECKF(parent == res, "case %d sub %u: parent %p want %p", idx, j, parent, res);
            CHECKF(idxout == j, "case %d sub %u: index %u", idx, j, idxout);
            SAFE_RELEASE(parent);
        }
        IDXGISurface2_Release(s);
        CHECKF(d3dh_refcount(res) == r0, "case %d sub %u: refcount after release", idx, j);
    }
    if (k->hr == S_OK) {
        IDXGISurface2 *s = (IDXGISurface2 *)(ULONG_PTR)0xdeadbeef;
        HRESULT hr = IDXGIResource1_CreateSubresourceSurface(res, n, &s);
        CHECKF(hr == E_INVALIDARG, "case %d out-of-range: %s", idx, d3dh_hr_name(hr));
        CHECKF(s == (IDXGISurface2 *)(ULONG_PTR)0xdeadbeef, "case %d out-of-range: out written %p", idx, s);
        s = (IDXGISurface2 *)(ULONG_PTR)0xdeadbeef;
        hr = IDXGIResource1_CreateSubresourceSurface(res, 0xffffffffu, &s);
        CHECKF(hr == E_INVALIDARG, "case %d index -1: %s", idx, d3dh_hr_name(hr));
    }
    IDXGIResource1_Release(res);
}
#define SUB_T(i) TEST(objs_res_subsurface_case##i) { NEED_DEV(c); sub_one(&c, i); objs_close(&c); }
SUB_T(0) SUB_T(1) SUB_T(2) SUB_T(3) SUB_T(4) SUB_T(5) SUB_T(6) SUB_T(7)

TEST(objs_res_subsurface_interfaces)
{
    NEED_DEV(c);
    sub_case k = { 2, 64, 64, 256, 3, 2, S_OK }; IDXGIResource1 *res = NULL; IDXGISurface2 *s = NULL;
    if (!make_sub(&c, &k, &res)) { t_skip("no resource"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIResource1_CreateSubresourceSurface(res, 4, &s));
    if (s) {
        IUnknown *u = (IUnknown *)s;
        CHECK(supports(u, &IID_IDXGISurface));
        CHECK(supports(u, &IID_IDXGISurface1));
        CHECK(supports(u, &IID_IDXGIDeviceSubObject));
        CHECK(supports(u, &IID_IDXGIObject));
        CHECK(supports(u, &IID_IUnknown));
        CHECK(!supports(u, &IID_ID3D11Texture2D));
        CHECK(!supports(u, &IID_ID3D11Resource));
        CHECK(!supports(u, &IID_IDXGIResource));
        CHECK(!supports(u, &IID_IDXGIResource1));
        IDXGISurface2_Release(s);
    }
    IDXGIResource1_Release(res);
    objs_close(&c);
}
TEST(objs_res_subsurface_getresource_errors)
{
    NEED_DEV(c);
    sub_case k = { 2, 64, 64, 256, 1, 1, S_OK }; IDXGIResource1 *res = NULL; IDXGISurface2 *s = NULL; UINT idx = 1234; IUnknown *p;
    if (!make_sub(&c, &k, &res)) { t_skip("no resource"); objs_close(&c); return; }
    if (SUCCEEDED(IDXGIResource1_CreateSubresourceSurface(res, 0, &s))) {
        EXPECT_HR(IDXGISurface2_GetResource(s, &IID_IDXGIResource1, NULL, &idx), E_POINTER);
        p = (IUnknown *)(ULONG_PTR)0xdeadbeef;
        EXPECT_HR(IDXGISurface2_GetResource(s, &IID_IDXGIFactory, (void **)&p, &idx), E_NOINTERFACE);
        EXPECT_EQ(p, NULL);
        EXPECT_EQ(idx, 1234);
        IDXGISurface2_Release(s);
    }
    /* a plain (non-subresource) surface view of the same resource */
    if (SUCCEEDED(IDXGIResource1_QueryInterface(res, &IID_IDXGISurface2, (void **)&s))) {
        IDXGIResource1 *parent = NULL; idx = 77;
        EXPECT_HR_OK(IDXGISurface2_GetResource(s, &IID_IDXGIResource1, (void **)&parent, &idx));
        EXPECT_EQ(parent, res);
        EXPECT_EQ(idx, 0);
        SAFE_RELEASE(parent);
        IDXGISurface2_Release(s);
    }
    IDXGIResource1_Release(res);
    objs_close(&c);
}
TEST(objs_res_subsurface_keeps_resource_alive)
{
    NEED_DEV(c);
    sub_case k = { 2, 32, 32, 128, 2, 1, S_OK }; IDXGIResource1 *res = NULL; IDXGISurface2 *s = NULL; DXGI_SURFACE_DESC d;
    if (!make_sub(&c, &k, &res)) { t_skip("no resource"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIResource1_CreateSubresourceSurface(res, 1, &s));
    IDXGIResource1_Release(res);
    if (s) {
        EXPECT_HR_OK(IDXGISurface2_GetDesc(s, &d));
        EXPECT_EQ(d.Width, 16);
        IDXGISurface2_Release(s);
    }
    objs_close(&c);
}
TEST(objs_res_subsurface_nonstaging_default)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource1 *r = NULL; IDXGISurface2 *s = NULL;
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    if (!t || FAILED(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource1, (void **)&r))) { SAFE_RELEASE(t); t_skip("no IDXGIResource1"); objs_close(&c); return; }
    HRESULT hr = IDXGIResource1_CreateSubresourceSurface(r, 0, &s);
    EXPECT_HR_OK(hr);
    if (s) {
        DXGI_SURFACE_DESC d; DXGI_MAPPED_RECT m;
        EXPECT_HR_OK(IDXGISurface2_GetDesc(s, &d));
        EXPECT_EQ(d.Width, 64);
        hr = IDXGISurface2_Map(s, &m, DXGI_MAP_READ);
        EXPECT_HR_FAILED(hr);   /* default-usage resources have no CPU access */
        if (SUCCEEDED(hr)) IDXGISurface2_Unmap(s);
        IDXGISurface2_Release(s);
    }
    IDXGIResource1_Release(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}

/* A buffer is exposed as a Width=ByteWidth, Height=1, DXGI_FORMAT_UNKNOWN surface; only CPU-readable/writable buffers can be mapped through it. */
TEST(objs_res_buffer_as_surface)
{
    NEED_DEV(c);
    static const struct { D3D11_USAGE u; UINT bind, cpu; int mappable; const char *n; } k[] = {
        { D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, "default" },
        { D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 1, "staging" },
        { D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE, 0, "dynamic" },
    };
    unsigned i;
    for (i = 0; i < 3; i++) {
        ID3D11Buffer *b = NULL; IDXGISurface *s = NULL; IDXGISurface1 *s1 = NULL;
        DXGI_SURFACE_DESC d; DXGI_MAPPED_RECT mr; HDC dc = NULL;
        if (FAILED(objs_buf(c.dev, 128, k[i].u, k[i].bind, k[i].cpu, 0, &b))) { CHECKF(0, "%s buffer", k[i].n); continue; }
        EXPECT_HR_OK(ID3D11Buffer_QueryInterface(b, &IID_IDXGISurface, (void **)&s));
        if (s) {
            memset(&d, 0xcc, sizeof d);
            EXPECT_HR_OK(IDXGISurface_GetDesc(s, &d));
            EXPECT_EQ(d.Width, 128); EXPECT_EQ(d.Height, 1); EXPECT_EQ(d.Format, DXGI_FORMAT_UNKNOWN);
            EXPECT_EQ(d.SampleDesc.Count, 1); EXPECT_EQ(d.SampleDesc.Quality, 0);
            memset(&mr, 0, sizeof mr);
            EXPECT_HR(IDXGISurface_Map(s, &mr, DXGI_MAP_READ), k[i].mappable ? S_OK : E_INVALIDARG);
            if (k[i].mappable) { EXPECT_EQ(mr.Pitch, 128); CHECK(mr.pBits != NULL); EXPECT_HR_OK(IDXGISurface_Unmap(s)); }
            EXPECT_HR(IDXGISurface_Map(s, &mr, DXGI_MAP_WRITE), k[i].mappable ? S_OK : E_INVALIDARG);
            if (k[i].mappable) EXPECT_HR_OK(IDXGISurface_Unmap(s));
            EXPECT_HR_OK(IDXGISurface_Unmap(s));   /* an unbalanced Unmap is harmless */
            EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGISurface1, (void **)&s1));
            if (s1) { EXPECT_HR(IDXGISurface1_GetDC(s1, FALSE, &dc), DXGI_ERROR_INVALID_CALL); IDXGISurface1_Release(s1); }
            IDXGISurface_Release(s);
        }
        ID3D11Buffer_Release(b);
    }
    objs_close(&c);
}
