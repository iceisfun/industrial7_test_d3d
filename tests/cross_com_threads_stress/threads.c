/* Bounded multi-thread behaviour: one free-threaded device used from many threads, one device per thread,
 * reference-count atomicity, state-object deduplication under contention, the immediate-context lock. */
#include "cts.h"
#include "shaders.h"

#define NT 8

/* ---- reference counting ------------------------------------------------------------- */
static DWORD WINAPI rc_hammer(cts_job *j)
{
    IUnknown *u = j->shared;
    for (int i = 0; i < j->ops; i++) { u->lpVtbl->AddRef(u); u->lpVtbl->Release(u); }
    j->done = j->ops;
    return 0;
}
static void rc_case(IUnknown *u)
{
    cts_job j[NT]; ULONG before = d3dh_refcount(u);
    memset(j, 0, sizeof j);
    for (int i = 0; i < NT; i++) { j[i].shared = u; j[i].ops = 10000; }
    CHECK(cts_run(NT, j, rc_hammer, 60000));
    EXPECT_EQ(cts_total_done(NT, j), NT * 10000);
    EXPECT_EQ(d3dh_refcount(u), before);
}
T(threads_refcount_hammer_device)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    rc_case((IUnknown *)d);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(threads_refcount_hammer_context)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    rc_case((IUnknown *)c);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(threads_refcount_hammer_buffer_and_texture)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Buffer *b = NULL; ID3D11Texture2D *t = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    EXPECT_HR_OK(cts_make_buffer(d, 64, D3D11_BIND_VERTEX_BUFFER, &b));
    EXPECT_HR_OK(cts_make_tex(d, 8, 8, &t));
    if (b) { EXPECT_EQ(d3dh_refcount(b), 1); rc_case((IUnknown *)b); }
    if (t) { EXPECT_EQ(d3dh_refcount(t), 1); rc_case((IUnknown *)t); }
    SAFE_RELEASE(t); SAFE_RELEASE(b); SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(threads_refcount_hammer_dxgi_factory)
{
    IDXGIFactory1 *f = NULL;
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no factory"); return; }
    EXPECT_EQ(d3dh_refcount(f), 1);
    rc_case((IUnknown *)f);
    SAFE_RELEASE(f);
}

static DWORD WINAPI qi_hammer(cts_job *j)
{
    ID3D11Device *d = j->shared;
    for (int i = 0; i < j->ops; i++) {
        IDXGIDevice *dd = NULL; ID3D11Device *d2 = NULL; IUnknown *u = NULL; ID3D11Debug *dbg = NULL;
        HRESULT h = ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd);
        if (FAILED(h)) CTS_FAIL(j, h); else IDXGIDevice_Release(dd);
        h = ID3D11Device_QueryInterface(d, &IID_ID3D11Device, (void **)&d2);
        if (FAILED(h)) CTS_FAIL(j, h); else { if ((void *)d2 != (void *)d) CTS_FAIL(j, E_FAIL); ID3D11Device_Release(d2); }
        h = ID3D11Device_QueryInterface(d, &IID_IUnknown, (void **)&u);
        if (FAILED(h)) CTS_FAIL(j, h); else { if (u != (IUnknown *)d) CTS_FAIL(j, E_FAIL); u->lpVtbl->Release(u); }
        h = ID3D11Device_QueryInterface(d, &IID_IDXGISurface, (void **)&dbg);   /* unsupported interface */
        if (h != E_NOINTERFACE || dbg) CTS_FAIL(j, h);
        j->done++;
    }
    return 0;
}
T(threads_queryinterface_concurrent_device)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[NT]; ULONG before;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    before = d3dh_refcount(d);
    memset(j, 0, sizeof j);
    for (int i = 0; i < NT; i++) { j[i].shared = d; j[i].ops = 300; }
    CHECK(cts_run(NT, j, qi_hammer, 60000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    EXPECT_EQ(cts_total_done(NT, j), NT * 300);
    EXPECT_EQ(d3dh_refcount(d), before);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- create / destroy on one device from NT threads ---------------------------------------- */
static DWORD WINAPI create_buffers(cts_job *j)
{
    ID3D11Device *d = j->shared;
    for (int i = 0; i < j->ops; i++) {
        ID3D11Buffer *b = NULL; D3D11_BUFFER_DESC bd, got;
        UINT sz = 16 * (1 + (i + j->idx) % 16);
        HRESULT h = cts_make_buffer(d, sz, D3D11_BIND_VERTEX_BUFFER, &b);
        if (FAILED(h) || !b) { CTS_FAIL(j, h); continue; }
        memset(&got, 0, sizeof got); ID3D11Buffer_GetDesc(b, &got);
        if (got.ByteWidth != sz || got.BindFlags != D3D11_BIND_VERTEX_BUFFER) CTS_FAIL(j, E_FAIL);
        if (ID3D11Buffer_Release(b) != 0) CTS_FAIL(j, E_FAIL);
        (void)bd;
        j->done++;
    }
    return 0;
}
T(threads_create_destroy_buffers_8x200)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[NT]; ULONG before;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    before = d3dh_refcount(d);
    memset(j, 0, sizeof j);
    for (int i = 0; i < NT; i++) { j[i].shared = d; j[i].ops = 200; }
    CHECK(cts_run(NT, j, create_buffers, 120000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    EXPECT_EQ(cts_total_done(NT, j), NT * 200);
    EXPECT_EQ(d3dh_refcount(d), before);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

static DWORD WINAPI create_textures(cts_job *j)
{
    ID3D11Device *d = j->shared;
    for (int i = 0; i < j->ops; i++) {
        ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *rtv = NULL; D3D11_TEXTURE2D_DESC td;
        UINT w = 4 << ((i + j->idx) % 4);
        HRESULT h = cts_make_tex(d, w, w, &t);
        if (FAILED(h) || !t) { CTS_FAIL(j, h); continue; }
        memset(&td, 0, sizeof td); ID3D11Texture2D_GetDesc(t, &td);
        if (td.Width != w || td.Height != w || td.Format != DXGI_FORMAT_R8G8B8A8_UNORM) CTS_FAIL(j, E_FAIL);
        h = ID3D11Device_CreateShaderResourceView(d, (ID3D11Resource *)t, NULL, &srv);
        if (FAILED(h)) CTS_FAIL(j, h);
        h = ID3D11Device_CreateRenderTargetView(d, (ID3D11Resource *)t, NULL, &rtv);
        if (FAILED(h)) CTS_FAIL(j, h);
        SAFE_RELEASE(rtv); SAFE_RELEASE(srv);
        if (ID3D11Texture2D_Release(t) != 0) CTS_FAIL(j, E_FAIL);
        j->done++;
    }
    return 0;
}
T(threads_create_destroy_textures_with_views_8x200)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[NT];
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    memset(j, 0, sizeof j);
    for (int i = 0; i < NT; i++) { j[i].shared = d; j[i].ops = 200; }
    CHECK(cts_run(NT, j, create_textures, 120000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    EXPECT_EQ(cts_total_done(NT, j), NT * 200);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* Objects created by threads that then exit are still valid; releasing them later restores the device refcount. */
static DWORD WINAPI create_and_keep(cts_job *j)
{
    ID3D11Device *d = j->shared; ID3D11Buffer **keep = j->priv;
    for (int i = 0; i < j->ops; i++) {
        HRESULT h = cts_make_buffer(d, 32 + i, D3D11_BIND_VERTEX_BUFFER, &keep[i]);
        if (FAILED(h)) CTS_FAIL(j, h);
    }
    return 0;
}
T(threads_objects_outlive_creating_threads)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[NT]; ID3D11Buffer *keep[NT][50]; ULONG before;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    before = d3dh_refcount(d);
    memset(j, 0, sizeof j); memset(keep, 0, sizeof keep);
    for (int i = 0; i < NT; i++) { j[i].shared = d; j[i].ops = 50; j[i].priv = keep[i]; }
    CHECK(cts_run(NT, j, create_and_keep, 60000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    for (int i = 0; i < NT; i++) for (int k = 0; k < 50; k++) if (keep[i][k]) {
        D3D11_BUFFER_DESC bd; ID3D11Buffer_GetDesc(keep[i][k], &bd);
        EXPECT_EQ(bd.ByteWidth, 32 + k);
        EXPECT_EQ(ID3D11Buffer_Release(keep[i][k]), 0);
    } else CHECK(0);
    EXPECT_EQ(d3dh_refcount(d), before);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- state object identity under contention ---------------------------------------------- */
static void samp_desc(D3D11_SAMPLER_DESC *s)
{
    memset(s, 0, sizeof *s);
    s->Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; s->AddressU = s->AddressV = s->AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    s->MaxAnisotropy = 1; s->ComparisonFunc = D3D11_COMPARISON_NEVER; s->MinLOD = 0; s->MaxLOD = D3D11_FLOAT32_MAX;
}
typedef struct { ID3D11SamplerState *s[NT]; ID3D11BlendState *b[NT]; ID3D11RasterizerState *r[NT]; ID3D11DepthStencilState *z[NT]; } stateptrs;
static DWORD WINAPI create_states(cts_job *j)
{
    ID3D11Device *d = j->shared; stateptrs *p = j->priv; HRESULT h;
    D3D11_SAMPLER_DESC sd; D3D11_BLEND_DESC bd; D3D11_RASTERIZER_DESC rd; D3D11_DEPTH_STENCIL_DESC zd;
    samp_desc(&sd);
    memset(&bd, 0, sizeof bd); bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    memset(&rd, 0, sizeof rd); rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_BACK; rd.DepthClipEnable = TRUE;
    memset(&zd, 0, sizeof zd); zd.DepthEnable = TRUE; zd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; zd.DepthFunc = D3D11_COMPARISON_LESS;
    for (int i = 0; i < j->ops; i++) {
        ID3D11SamplerState *s = NULL; ID3D11BlendState *b = NULL; ID3D11RasterizerState *r = NULL; ID3D11DepthStencilState *z = NULL;
        h = ID3D11Device_CreateSamplerState(d, &sd, &s); if (FAILED(h)) CTS_FAIL(j, h);
        h = ID3D11Device_CreateBlendState(d, &bd, &b); if (FAILED(h)) CTS_FAIL(j, h);
        h = ID3D11Device_CreateRasterizerState(d, &rd, &r); if (FAILED(h)) CTS_FAIL(j, h);
        h = ID3D11Device_CreateDepthStencilState(d, &zd, &z); if (FAILED(h)) CTS_FAIL(j, h);
        if (i == 0) { p->s[j->idx] = s; p->b[j->idx] = b; p->r[j->idx] = r; p->z[j->idx] = z; }
        else { SAFE_RELEASE(s); SAFE_RELEASE(b); SAFE_RELEASE(r); SAFE_RELEASE(z); }
        j->done++;
    }
    return 0;
}
T(threads_identical_state_objects_are_shared)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[NT]; stateptrs p;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    memset(j, 0, sizeof j); memset(&p, 0, sizeof p);
    for (int i = 0; i < NT; i++) { j[i].shared = d; j[i].priv = &p; j[i].ops = 50; }
    CHECK(cts_run(NT, j, create_states, 60000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    for (int i = 1; i < NT; i++) {
        CHECK(p.s[i] == p.s[0]); CHECK(p.b[i] == p.b[0]); CHECK(p.r[i] == p.r[0]); CHECK(p.z[i] == p.z[0]);
    }
    CHECK(p.s[0] && p.b[0] && p.r[0] && p.z[0]);
    for (int i = 0; i < NT; i++) { SAFE_RELEASE(p.s[i]); SAFE_RELEASE(p.b[i]); SAFE_RELEASE(p.r[i]); SAFE_RELEASE(p.z[i]); }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(threads_distinct_state_descs_give_distinct_objects)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11SamplerState *a = NULL, *b = NULL, *a2 = NULL; D3D11_SAMPLER_DESC sd;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    samp_desc(&sd);
    EXPECT_HR_OK(ID3D11Device_CreateSamplerState(d, &sd, &a));
    EXPECT_HR_OK(ID3D11Device_CreateSamplerState(d, &sd, &a2));
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    EXPECT_HR_OK(ID3D11Device_CreateSamplerState(d, &sd, &b));
    CHECK(a == a2); CHECK(a != b);
    SAFE_RELEASE(a); SAFE_RELEASE(a2); SAFE_RELEASE(b); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- shader creation from several threads -------------------------------------------------- */
static DWORD WINAPI create_shaders(cts_job *j)
{
    ID3D11Device *d = j->shared; ID3DBlob *bl = j->priv; HRESULT h;
    for (int i = 0; i < j->ops; i++) {
        ID3D11PixelShader *ps = NULL;
        h = ID3D11Device_CreatePixelShader(d, d3dh_blob_ptr(bl), d3dh_blob_size(bl), NULL, &ps);
        if (FAILED(h) || !ps) CTS_FAIL(j, h);
        SAFE_RELEASE(ps);
        j->done++;
    }
    return 0;
}
T(threads_create_pixel_shaders_concurrently)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3DBlob *bl = NULL; cts_job j[NT];
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    if (FAILED(d3dh_compile(SH_FLAT_PS, "main", "ps_4_0", 0, &bl))) { t_skip("no shader compiler"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    memset(j, 0, sizeof j);
    for (int i = 0; i < NT; i++) { j[i].shared = d; j[i].priv = bl; j[i].ops = 50; }
    CHECK(cts_run(NT, j, create_shaders, 60000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    EXPECT_EQ(cts_total_done(NT, j), NT * 50);
    SAFE_RELEASE(bl); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- deferred contexts from many threads ---------------------------------------------------- */
static DWORD WINAPI deferred_stress(cts_job *j)
{
    ID3D11Device *d = j->shared;
    for (int i = 0; i < j->ops; i++) {
        ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL; D3D11_VIEWPORT vp = { 0, 0, 16.0f + i, 16, 0, 1 };
        HRESULT h = ID3D11Device_CreateDeferredContext(d, 0, &dc);
        if (FAILED(h)) { CTS_FAIL(j, h); continue; }
        ID3D11DeviceContext_RSSetViewports(dc, 1, &vp);
        if (ID3D11DeviceContext_GetType(dc) != D3D11_DEVICE_CONTEXT_DEFERRED) CTS_FAIL(j, E_FAIL);
        h = ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl);
        if (FAILED(h) || !cl) CTS_FAIL(j, h);
        SAFE_RELEASE(cl); SAFE_RELEASE(dc);
        j->done++;
    }
    return 0;
}
T(threads_deferred_contexts_8x50)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[NT]; D3D_FEATURE_LEVEL fl = 0;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(d3dh_make_device11(&d, &c, &fl, NULL, 0))) { t_skip("no 11 device"); return; }
    memset(j, 0, sizeof j);
    for (int i = 0; i < NT; i++) { j[i].shared = d; j[i].ops = 50; }
    CHECK(cts_run(NT, j, deferred_stress, 120000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    EXPECT_EQ(cts_total_done(NT, j), NT * 50);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- immediate context data integrity with a protected (default) context --------------------- */
typedef struct { ID3D11DeviceContext *c; ID3D11Device *d; } ctxshared;
static DWORD WINAPI ctx_roundtrip(cts_job *j)
{
    ctxshared *s = j->shared; ID3D11Buffer *def = NULL, *st = NULL; D3D11_BUFFER_DESC bd; UINT32 data[16];
    HRESULT h = cts_make_buffer(s->d, 64, D3D11_BIND_VERTEX_BUFFER, &def);
    if (FAILED(h)) { CTS_FAIL(j, h); return 0; }
    memset(&bd, 0, sizeof bd); bd.ByteWidth = 64; bd.Usage = D3D11_USAGE_STAGING; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    h = ID3D11Device_CreateBuffer(s->d, &bd, NULL, &st);
    if (FAILED(h)) { CTS_FAIL(j, h); SAFE_RELEASE(def); return 0; }
    for (int i = 0; i < j->ops; i++) {
        D3D11_MAPPED_SUBRESOURCE m;
        for (int k = 0; k < 16; k++) data[k] = (j->idx << 24) | (i << 8) | k;
        ID3D11DeviceContext_UpdateSubresource(s->c, (ID3D11Resource *)def, 0, NULL, data, 0, 0);
        ID3D11DeviceContext_CopyResource(s->c, (ID3D11Resource *)st, (ID3D11Resource *)def);
        h = ID3D11DeviceContext_Map(s->c, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m);
        if (FAILED(h)) { CTS_FAIL(j, h); continue; }
        if (memcmp(m.pData, data, 64)) CTS_FAIL(j, E_FAIL);
        ID3D11DeviceContext_Unmap(s->c, (ID3D11Resource *)st, 0);
        j->done++;
    }
    SAFE_RELEASE(st); SAFE_RELEASE(def);
    return 0;
}
T(threads_protected_immediate_context_roundtrip_4x50)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Multithread *mt = NULL; cts_job j[4]; ctxshared s;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt))) { t_skip("no ID3D11Multithread"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    if (!ID3D11Multithread_GetMultithreadProtected(mt)) { t_skip("context not protected"); SAFE_RELEASE(mt); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    s.c = c; s.d = d;
    memset(j, 0, sizeof j);
    for (int i = 0; i < 4; i++) { j[i].shared = &s; j[i].ops = 50; }
    CHECK(cts_run(4, j, ctx_roundtrip, 120000));
    EXPECT_EQ(cts_total_fails(4, j), 0);
    EXPECT_EQ(cts_total_done(4, j), 200);
    SAFE_RELEASE(mt); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- ID3D11Multithread ------------------------------------------------------------------------ */
T(threads_multithread_default_protected)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Multithread *mt = NULL, *mt2 = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    EXPECT_HR_OK(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt));
    if (mt) {
        EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), TRUE);
        /* the interface is also reachable from the device and is the same object state */
        if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_ID3D11Multithread, (void **)&mt2))) {
            EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt2), TRUE);
            SAFE_RELEASE(mt2);
        } else t_note("device QI(ID3D11Multithread) failed");
    }
    SAFE_RELEASE(mt); SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(threads_multithread_set_returns_previous)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Multithread *mt = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt))) { t_skip("no ID3D11Multithread"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, FALSE), TRUE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), FALSE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, FALSE), FALSE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, TRUE), FALSE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), TRUE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, TRUE), TRUE);
    SAFE_RELEASE(mt); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

typedef struct { ID3D11Multithread *mt; HANDLE entered; } lockjob;
static DWORD WINAPI lock_waiter(LPVOID p)
{
    lockjob *l = p;
    ID3D11Multithread_Enter(l->mt);
    SetEvent(l->entered);
    ID3D11Multithread_Leave(l->mt);
    return 0;
}
T(threads_multithread_enter_blocks_other_threads)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Multithread *mt = NULL; lockjob l; HANDLE th;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt))) { t_skip("no ID3D11Multithread"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    ID3D11Multithread_SetMultithreadProtected(mt, TRUE);
    l.mt = mt; l.entered = CreateEventA(NULL, TRUE, FALSE, NULL);
    ID3D11Multithread_Enter(mt);
    ID3D11Multithread_Enter(mt);                    /* recursive on the owning thread */
    th = CreateThread(NULL, 0, lock_waiter, &l, 0, NULL);
    CHECK(th != NULL);
    EXPECT_EQ(WaitForSingleObject(l.entered, 300), WAIT_TIMEOUT);   /* still held by this thread */
    ID3D11Multithread_Leave(mt);
    EXPECT_EQ(WaitForSingleObject(l.entered, 300), WAIT_TIMEOUT);   /* one recursion level remains */
    ID3D11Multithread_Leave(mt);
    EXPECT_EQ(WaitForSingleObject(l.entered, 10000), WAIT_OBJECT_0);
    if (th) { WaitForSingleObject(th, 10000); CloseHandle(th); }
    CloseHandle(l.entered);
    SAFE_RELEASE(mt); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(threads_singlethreaded_flag_roundtrip)
{
    ID3D11Device *d = NULL, *d2 = NULL; ID3D11DeviceContext *c = NULL, *c2 = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(D3D11_CREATE_DEVICE_SINGLETHREADED, &d, &c))) { t_skip("no 11 device"); return; }
    EXPECT_EQ(ID3D11Device_GetCreationFlags(d) & D3D11_CREATE_DEVICE_SINGLETHREADED, D3D11_CREATE_DEVICE_SINGLETHREADED);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    if (SUCCEEDED(cts_dev11(0, &d2, &c2))) {
        EXPECT_EQ(ID3D11Device_GetCreationFlags(d2) & D3D11_CREATE_DEVICE_SINGLETHREADED, 0);
    }
    SAFE_RELEASE(c2); SAFE_RELEASE(d2); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- one device per thread -------------------------------------------------------------------- */
typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; } owndev;
static DWORD WINAPI own_device(cts_job *j)
{
    owndev *o = j->priv; ID3D11Buffer *def = NULL, *st = NULL; D3D11_BUFFER_DESC bd; UINT32 data[32]; D3D11_MAPPED_SUBRESOURCE m; HRESULT h;
    h = cts_dev11(j->ops ? D3D11_CREATE_DEVICE_SINGLETHREADED : 0, &o->dev, &o->ctx);
    if (FAILED(h)) { CTS_FAIL(j, h); return 0; }
    for (int k = 0; k < 32; k++) data[k] = 0xA5000000u | (j->idx << 16) | k;
    memset(&bd, 0, sizeof bd); bd.ByteWidth = sizeof data; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    { D3D11_SUBRESOURCE_DATA sd = { data, 0, 0 }; h = ID3D11Device_CreateBuffer(o->dev, &bd, &sd, &def); }
    if (FAILED(h)) { CTS_FAIL(j, h); return 0; }
    memset(&bd, 0, sizeof bd); bd.ByteWidth = sizeof data; bd.Usage = D3D11_USAGE_STAGING; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    h = ID3D11Device_CreateBuffer(o->dev, &bd, NULL, &st);
    if (FAILED(h)) { CTS_FAIL(j, h); SAFE_RELEASE(def); return 0; }
    for (int i = 0; i < 20; i++) {
        ID3D11DeviceContext_CopyResource(o->ctx, (ID3D11Resource *)st, (ID3D11Resource *)def);
        h = ID3D11DeviceContext_Map(o->ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m);
        if (FAILED(h)) { CTS_FAIL(j, h); break; }
        if (memcmp(m.pData, data, sizeof data)) CTS_FAIL(j, E_FAIL);
        ID3D11DeviceContext_Unmap(o->ctx, (ID3D11Resource *)st, 0);
        j->done++;
    }
    SAFE_RELEASE(st); SAFE_RELEASE(def);
    return 0;
}
static void own_device_case(int singlethreaded)
{
    cts_job j[4]; owndev o[4]; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
    memset(j, 0, sizeof j); memset(o, 0, sizeof o);
    for (int i = 0; i < 4; i++) { j[i].priv = &o[i]; j[i].ops = singlethreaded; }
    CHECK(cts_run(4, j, own_device, 120000));
    EXPECT_EQ(cts_total_fails(4, j), 0);
    EXPECT_EQ(cts_total_done(4, j), 80);
    for (int i = 0; i < 4; i++) {
        CHECK(o[i].dev != NULL);
        for (int k = i + 1; k < 4; k++) CHECK(o[i].dev != o[k].dev);
        if (o[i].dev) EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(o[i].dev), S_OK);
        SAFE_RELEASE(o[i].ctx); SAFE_RELEASE(o[i].dev);
    }
}
T(threads_four_threads_own_device) { own_device_case(0); }
T(threads_four_threads_own_singlethreaded_device) { own_device_case(1); }

/* ---- read-only queries race freely ---------------------------------------------------------------- */
static DWORD WINAPI query_reads(cts_job *j)
{
    ID3D11Device *d = j->shared; D3D_FEATURE_LEVEL fl0 = (D3D_FEATURE_LEVEL)(ULONG_PTR)j->priv;
    for (int i = 0; i < j->ops; i++) {
        UINT sup = 0; HRESULT h;
        if (ID3D11Device_GetFeatureLevel(d) != fl0) CTS_FAIL(j, E_FAIL);
        if (ID3D11Device_GetDeviceRemovedReason(d) != S_OK) CTS_FAIL(j, E_FAIL);
        h = ID3D11Device_CheckFormatSupport(d, DXGI_FORMAT_R8G8B8A8_UNORM, &sup);
        if (FAILED(h) || !(sup & D3D11_FORMAT_SUPPORT_TEXTURE2D)) CTS_FAIL(j, h);
        { UINT q = 0; h = ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, 1, &q); if (FAILED(h) || q == 0) CTS_FAIL(j, h); }
        j->done++;
    }
    return 0;
}
T(threads_concurrent_device_queries)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[NT]; D3D_FEATURE_LEVEL fl = 0;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(d3dh_make_device11(&d, &c, &fl, NULL, 0))) { t_skip("no 11 device"); return; }
    memset(j, 0, sizeof j);
    for (int i = 0; i < NT; i++) { j[i].shared = d; j[i].priv = (void *)(ULONG_PTR)fl; j[i].ops = 500; }
    CHECK(cts_run(NT, j, query_reads, 60000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    EXPECT_EQ(cts_total_done(NT, j), NT * 500);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- DXGI from many threads ----------------------------------------------------------------------- */
static DWORD WINAPI dxgi_factories(cts_job *j)
{
    LUID *ref = j->shared;
    for (int i = 0; i < j->ops; i++) {
        IDXGIFactory1 *f = NULL; IDXGIAdapter1 *a = NULL; DXGI_ADAPTER_DESC1 ad;
        HRESULT h = d3dh_make_factory1(&f);
        if (FAILED(h) || !f) { CTS_FAIL(j, h); continue; }
        h = IDXGIFactory1_EnumAdapters1(f, 0, &a);
        if (SUCCEEDED(h)) {
            memset(&ad, 0, sizeof ad);
            h = IDXGIAdapter1_GetDesc1(a, &ad);
            if (FAILED(h) || ad.AdapterLuid.LowPart != ref->LowPart || ad.AdapterLuid.HighPart != ref->HighPart) CTS_FAIL(j, h);
            SAFE_RELEASE(a);
        } else if (h != DXGI_ERROR_NOT_FOUND) CTS_FAIL(j, h);
        SAFE_RELEASE(f);
        j->done++;
    }
    return 0;
}
T(threads_dxgi_factories_enumerate_same_adapter)
{
    IDXGIFactory1 *f = NULL; IDXGIAdapter1 *a = NULL; DXGI_ADAPTER_DESC1 ad; LUID luid; cts_job j[NT];
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no factory"); return; }
    if (FAILED(IDXGIFactory1_EnumAdapters1(f, 0, &a))) { t_skip("no adapter"); SAFE_RELEASE(f); return; }
    memset(&ad, 0, sizeof ad); IDXGIAdapter1_GetDesc1(a, &ad); luid = ad.AdapterLuid;
    SAFE_RELEASE(a); SAFE_RELEASE(f);
    memset(j, 0, sizeof j);
    for (int i = 0; i < NT; i++) { j[i].shared = &luid; j[i].ops = 50; }
    CHECK(cts_run(NT, j, dxgi_factories, 60000));
    EXPECT_EQ(cts_total_fails(NT, j), 0);
    EXPECT_EQ(cts_total_done(NT, j), NT * 50);
}

/* ---- D3D12: parallel recording objects ------------------------------------------------------------ */
static DWORD WINAPI d12_objects(cts_job *j)
{
    ID3D12Device *d = j->shared;
    for (int i = 0; i < j->ops; i++) {
        ID3D12CommandAllocator *ca = NULL; ID3D12GraphicsCommandList *cl = NULL; ID3D12Fence *fe = NULL;
        HRESULT h = ID3D12Device_CreateCommandAllocator(d, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&ca);
        if (FAILED(h)) { CTS_FAIL(j, h); continue; }
        h = ID3D12Device_CreateCommandList(d, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, ca, NULL, &IID_ID3D12GraphicsCommandList, (void **)&cl);
        if (FAILED(h)) CTS_FAIL(j, h);
        else { h = ID3D12GraphicsCommandList_Close(cl); if (FAILED(h)) CTS_FAIL(j, h); }
        h = ID3D12Device_CreateFence(d, 7, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fe);
        if (FAILED(h)) CTS_FAIL(j, h);
        else if (ID3D12Fence_GetCompletedValue(fe) != 7) CTS_FAIL(j, E_FAIL);
        SAFE_RELEASE(fe); SAFE_RELEASE(cl); SAFE_RELEASE(ca);
        j->done++;
    }
    return 0;
}
T(threads_d3d12_objects_4x100)
{
    ID3D12Device *d = NULL; cts_job j[4]; int w;
    REQUIRE(d3dh_D3D12CreateDevice());
    if (FAILED(d3dh_make_device12(&d, D3D_FEATURE_LEVEL_11_0, &w))) { t_skip("no d3d12 device"); return; }
    memset(j, 0, sizeof j);
    for (int i = 0; i < 4; i++) { j[i].shared = d; j[i].ops = 100; }
    CHECK(cts_run(4, j, d12_objects, 120000));
    EXPECT_EQ(cts_total_fails(4, j), 0);
    EXPECT_EQ(cts_total_done(4, j), 400);
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(d);
}

typedef struct { ID3D12Fence *f; HANDLE ev; UINT64 v; } fencejob;
static DWORD WINAPI fence_signaller(LPVOID p) { fencejob *f = p; Sleep(50); ID3D12Fence_Signal(f->f, f->v); return 0; }
T(threads_d3d12_fence_signalled_from_other_thread)
{
    ID3D12Device *d = NULL; ID3D12Fence *fe = NULL; fencejob fj; HANDLE th; int w; HRESULT h;
    REQUIRE(d3dh_D3D12CreateDevice());
    if (FAILED(d3dh_make_device12(&d, D3D_FEATURE_LEVEL_11_0, &w))) { t_skip("no d3d12 device"); return; }
    EXPECT_HR_OK(ID3D12Device_CreateFence(d, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fe));
    if (fe) {
        fj.f = fe; fj.v = 5; fj.ev = CreateEventA(NULL, FALSE, FALSE, NULL);
        h = ID3D12Fence_SetEventOnCompletion(fe, 5, fj.ev);
        EXPECT_HR_OK(h);
        th = CreateThread(NULL, 0, fence_signaller, &fj, 0, NULL);
        EXPECT_EQ(WaitForSingleObject(fj.ev, 10000), WAIT_OBJECT_0);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(fe), 5);
        /* an already-reached value fires the event immediately */
        EXPECT_HR_OK(ID3D12Fence_SetEventOnCompletion(fe, 3, fj.ev));
        EXPECT_EQ(WaitForSingleObject(fj.ev, 5000), WAIT_OBJECT_0);
        if (th) { WaitForSingleObject(th, 10000); CloseHandle(th); }
        CloseHandle(fj.ev);
    }
    SAFE_RELEASE(fe); SAFE_RELEASE(d);
}
