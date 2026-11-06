/* COM apartment interaction: D3D/DXGI entry points work on threads with no COM, an STA or an MTA,
 * and objects are not bound to the apartment (or thread) that created them. */
#include "cts.h"

/* ---- scaffolding facts about the apartment model (the base every other test relies on) ---- */
static DWORD WINAPI coinit_rules(cts_job *j)
{
    HRESULT a = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT b = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT c = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    j->priv = (void *)(ULONG_PTR)((a == S_OK) | ((b == S_FALSE) << 1) | ((c == (HRESULT)0x80010106) << 2));
    CoUninitialize(); CoUninitialize();
    return 0;
}
T(apartment_coinit_return_values)
{
    cts_job j; memset(&j, 0, sizeof j);
    CHECK(cts_run(1, &j, coinit_rules, 10000));
    EXPECT_EQ((ULONG_PTR)j.priv, 7);
}

/* ---- per-apartment-mode creation ------------------------------------------------ */
typedef struct {
    HRESULT hr_dev, hr_buf, hr_tex, hr_removed, hr_qi_dxgi, hr_factory, hr_adapter;
    HRESULT hr_ctxmap;
    UINT fl; int same_adapter;
} aptres;

static DWORD WINAPI apt_d3d11(cts_job *j)
{
    aptres *r = j->priv; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Buffer *b = NULL; ID3D11Texture2D *t = NULL;
    IDXGIDevice *dd = NULL; IDXGIAdapter *ad = NULL; IDXGIFactory *f = NULL;
    r->hr_dev = cts_dev11(0, &d, &c);
    if (FAILED(r->hr_dev)) return 0;
    r->fl = ID3D11Device_GetFeatureLevel(d);
    r->hr_buf = cts_make_buffer(d, 256, D3D11_BIND_VERTEX_BUFFER, &b);
    r->hr_tex = cts_make_tex(d, 16, 16, &t);
    r->hr_removed = ID3D11Device_GetDeviceRemovedReason(d);
    r->hr_qi_dxgi = ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd);
    if (SUCCEEDED(r->hr_qi_dxgi)) {
        r->hr_adapter = IDXGIDevice_GetAdapter(dd, &ad);
        if (SUCCEEDED(r->hr_adapter)) r->hr_factory = IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory, (void **)&f);
    }
    SAFE_RELEASE(f); SAFE_RELEASE(ad); SAFE_RELEASE(dd); SAFE_RELEASE(t); SAFE_RELEASE(b); SAFE_RELEASE(c); SAFE_RELEASE(d);
    return 0;
}

static void apt_d3d11_mode(int mode)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; aptres r; cts_job j;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
    memset(&r, 0, sizeof r); memset(&j, 0, sizeof j);
    j.apt = mode; j.priv = &r;
    CHECK(cts_run(1, &j, apt_d3d11, 60000));
    if (mode != CTS_APT_NONE) CHECKF(SUCCEEDED(j.coinit_hr), "coinit %s hr=%s", cts_apt_name(mode), d3dh_hr_name(j.coinit_hr));
    EXPECT_HR_OK(r.hr_dev);
    EXPECT_HR_OK(r.hr_buf);
    EXPECT_HR_OK(r.hr_tex);
    EXPECT_HR(r.hr_removed, S_OK);
    EXPECT_HR_OK(r.hr_qi_dxgi);
    EXPECT_HR_OK(r.hr_adapter);
    EXPECT_HR_OK(r.hr_factory);
}
#define APT11(nm, m) T(apartment_d3d11_device_##nm) { apt_d3d11_mode(m); }
APT11(no_com, CTS_APT_NONE)
APT11(sta, CTS_APT_STA)
APT11(sta_nodde, CTS_APT_STA_NODDE)
APT11(mta, CTS_APT_MTA)
APT11(mta_speed, CTS_APT_MTA_SPEED)
APT11(sta_then_uninit, CTS_APT_STA_THEN_UNINIT)
APT11(oleinitialize, CTS_APT_OLE)

T(apartment_d3d11_same_featurelevel_in_every_mode)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; aptres r[3]; cts_job j[3]; int modes[3] = { CTS_APT_NONE, CTS_APT_STA, CTS_APT_MTA };
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
    memset(r, 0, sizeof r); memset(j, 0, sizeof j);
    for (int i = 0; i < 3; i++) { j[i].apt = modes[i]; j[i].priv = &r[i]; }
    for (int i = 0; i < 3; i++) { CHECK(cts_run(1, &j[i], apt_d3d11, 60000)); }
    EXPECT_EQ(r[0].fl, r[1].fl);
    EXPECT_EQ(r[1].fl, r[2].fl);
}

/* ---- DXGI factory / adapter enumeration in each mode ---------------------------- */
static DWORD WINAPI apt_dxgi(cts_job *j)
{
    aptres *r = j->priv; IDXGIFactory1 *f = NULL; IDXGIAdapter1 *a = NULL; DXGI_ADAPTER_DESC1 desc; UINT i = 0, n = 0;
    HRESULT (WINAPI *cf)(REFIID, void **) = (void *)d3dh_getproc("dxgi.dll", "CreateDXGIFactory1");
    r->hr_factory = cf(&IID_IDXGIFactory1, (void **)&f);
    if (FAILED(r->hr_factory)) return 0;
    while (IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK) { if (SUCCEEDED(IDXGIAdapter1_GetDesc1(a, &desc))) n++; SAFE_RELEASE(a); i++; }
    r->fl = n; r->hr_adapter = IDXGIFactory1_EnumAdapters1(f, i, &a);
    r->hr_dev = a == NULL ? S_OK : E_FAIL;
    SAFE_RELEASE(f);
    return 0;
}
static void apt_dxgi_mode(int mode)
{
    aptres r; cts_job j;
    IDXGIFactory1 *f0 = NULL;
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (FAILED(d3dh_make_factory1(&f0))) { t_skip("no DXGI factory"); return; }
    SAFE_RELEASE(f0);
    memset(&r, 0, sizeof r); memset(&j, 0, sizeof j); j.apt = mode; j.priv = &r;
    CHECK(cts_run(1, &j, apt_dxgi, 30000));
    EXPECT_HR_OK(r.hr_factory);
    EXPECT_HR(r.hr_adapter, DXGI_ERROR_NOT_FOUND);
    EXPECT_HR_OK(r.hr_dev);              /* the out pointer is NULL after NOT_FOUND */
}
#define APTDX(nm, m) T(apartment_dxgi_factory_##nm) { apt_dxgi_mode(m); }
APTDX(no_com, CTS_APT_NONE) APTDX(sta, CTS_APT_STA) APTDX(mta, CTS_APT_MTA) APTDX(sta_then_uninit, CTS_APT_STA_THEN_UNINIT) APTDX(oleinitialize, CTS_APT_OLE)

/* ---- D3D12 in each mode ----------------------------------------------------------- */
static DWORD WINAPI apt_d3d12(cts_job *j)
{
    aptres *r = j->priv; ID3D12Device *d = NULL; ID3D12CommandAllocator *ca = NULL; int w = 0;
    r->hr_dev = d3dh_make_device12(&d, D3D_FEATURE_LEVEL_11_0, &w);
    if (FAILED(r->hr_dev)) return 0;
    r->hr_buf = ID3D12Device_CreateCommandAllocator(d, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&ca);
    r->hr_removed = ID3D12Device_GetDeviceRemovedReason(d);
    SAFE_RELEASE(ca); SAFE_RELEASE(d);
    return 0;
}
static void apt_d3d12_mode(int mode)
{
    ID3D12Device *d = NULL; aptres r; cts_job j; int w;
    REQUIRE(d3dh_D3D12CreateDevice());
    if (FAILED(d3dh_make_device12(&d, D3D_FEATURE_LEVEL_11_0, &w))) { t_skip("no d3d12 device"); return; }
    SAFE_RELEASE(d);
    memset(&r, 0, sizeof r); memset(&j, 0, sizeof j); j.apt = mode; j.priv = &r;
    CHECK(cts_run(1, &j, apt_d3d12, 60000));
    EXPECT_HR_OK(r.hr_dev);
    EXPECT_HR_OK(r.hr_buf);
    EXPECT_HR(r.hr_removed, S_OK);
}
#define APT12(nm, m) T(apartment_d3d12_device_##nm) { apt_d3d12_mode(m); }
APT12(no_com, CTS_APT_NONE) APT12(sta, CTS_APT_STA) APT12(mta, CTS_APT_MTA)

/* ---- D3D10 / D3D9 / compiler / ddraw: no apartment requirements ---------------------- */
static DWORD WINAPI apt_misc(cts_job *j)
{
    aptres *r = j->priv; ID3D10Device *d = NULL; ID3DBlob *blob = NULL;
    r->hr_dev = d3dh_make_device10(&d, NULL, 0);
    SAFE_RELEASE(d);
    if (d3dh_D3DCompile()) {
        static const char src[] = "float4 main(float4 p : POSITION) : SV_Position { return p; }";
        r->hr_buf = d3dh_D3DCompile()(src, sizeof src - 1, "t", NULL, NULL, "main", "vs_4_0", 0, 0, &blob, NULL);
        SAFE_RELEASE(blob);
    } else r->hr_buf = S_OK;
    return 0;
}
static void apt_misc_mode(int mode)
{
    aptres r; cts_job j; ID3D10Device *d = NULL;
    if (!d3dh_D3D10CreateDevice() || FAILED(d3dh_make_device10(&d, NULL, 0))) { t_skip("no d3d10 device"); return; }
    SAFE_RELEASE(d);
    memset(&r, 0, sizeof r); memset(&j, 0, sizeof j); j.apt = mode; j.priv = &r;
    CHECK(cts_run(1, &j, apt_misc, 60000));
    EXPECT_HR_OK(r.hr_dev);
    if (d3dh_D3DCompile()) { EXPECT_HR_OK(r.hr_buf); }
}
#define APTMS(nm, m) T(apartment_d3d10_and_compiler_##nm) { apt_misc_mode(m); }
APTMS(no_com, CTS_APT_NONE) APTMS(sta, CTS_APT_STA) APTMS(mta, CTS_APT_MTA)

static DWORD WINAPI apt_d3d9(cts_job *j)
{
    aptres *r = j->priv; IDirect3D9 *d3d = NULL;
    d3d = d3dh_Direct3DCreate9() ? d3dh_Direct3DCreate9()(D3D_SDK_VERSION) : NULL;
    r->hr_dev = d3d ? S_OK : E_FAIL;
    if (d3d) {
        r->fl = IDirect3D9_GetAdapterCount(d3d);
        IDirect3D9_Release(d3d);
    }
    return 0;
}
static void apt_d3d9_mode(int mode)
{
    aptres r; cts_job j;
    REQUIRE(d3dh_Direct3DCreate9());
    { IDirect3D9 *p = d3dh_Direct3DCreate9()(D3D_SDK_VERSION); if (!p) { t_skip("Direct3DCreate9 fails here"); return; } IDirect3D9_Release(p); }
    memset(&r, 0, sizeof r); memset(&j, 0, sizeof j); j.apt = mode; j.priv = &r;
    CHECK(cts_run(1, &j, apt_d3d9, 30000));
    EXPECT_HR_OK(r.hr_dev);
}
#define APT9(nm, m) T(apartment_d3d9_create_##nm) { apt_d3d9_mode(m); }
APT9(no_com, CTS_APT_NONE) APT9(sta, CTS_APT_STA) APT9(mta, CTS_APT_MTA)

/* ---- objects are not apartment-affine ------------------------------------------------- */
typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; ID3D11Buffer *buf; HRESULT hr[8]; ID3D11Buffer *newbuf; } xfer;

static DWORD WINAPI xfer_create(cts_job *j)
{
    xfer *x = j->priv;
    x->hr[0] = cts_dev11(0, &x->dev, &x->ctx);
    if (SUCCEEDED(x->hr[0])) x->hr[1] = cts_make_buffer(x->dev, 1024, D3D11_BIND_CONSTANT_BUFFER, &x->buf);
    return 0;
}
static DWORD WINAPI xfer_use(cts_job *j)
{
    xfer *x = j->priv; D3D11_BUFFER_DESC bd; IDXGIDevice *dd = NULL;
    x->hr[2] = cts_make_buffer(x->dev, 64, D3D11_BIND_VERTEX_BUFFER, &x->newbuf);
    memset(&bd, 0, sizeof bd);
    ID3D11Buffer_GetDesc(x->buf, &bd);
    x->hr[3] = bd.ByteWidth == 1024 ? S_OK : E_FAIL;
    x->hr[4] = ID3D11Device_QueryInterface(x->dev, &IID_IDXGIDevice, (void **)&dd);
    SAFE_RELEASE(dd);
    x->hr[5] = ID3D11Device_GetDeviceRemovedReason(x->dev);
    {   /* immediate context used from the second thread */
        D3D11_BOX box = { 0, 0, 0, 16, 1, 1 }; BYTE data[16] = { 1 };
        ID3D11DeviceContext_UpdateSubresource(x->ctx, (ID3D11Resource *)x->buf, 0, &box, data, 0, 0);
        ID3D11DeviceContext_Flush(x->ctx);
        x->hr[6] = S_OK;
    }
    return 0;
}
static void xfer_case(int apt_a, int apt_b)
{
    xfer x; cts_job a, b;
    REQUIRE(d3dh_D3D11CreateDevice());
    memset(&x, 0, sizeof x); memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    a.apt = apt_a; a.priv = &x; b.apt = apt_b; b.priv = &x;
    CHECK(cts_run(1, &a, xfer_create, 60000));      /* creating thread has exited (and left its apartment) */
    if (FAILED(x.hr[0])) { t_skip("no 11 device"); return; }
    EXPECT_HR_OK(x.hr[1]);
    CHECK(cts_run(1, &b, xfer_use, 60000));
    EXPECT_HR_OK(x.hr[2]);
    EXPECT_HR_OK(x.hr[3]);
    EXPECT_HR_OK(x.hr[4]);
    EXPECT_HR(x.hr[5], S_OK);
    EXPECT_HR_OK(x.hr[6]);
    SAFE_RELEASE(x.newbuf); SAFE_RELEASE(x.buf); SAFE_RELEASE(x.ctx); SAFE_RELEASE(x.dev);
}
#define XFER(nm, a, b) T(apartment_object_survives_creator_thread_##nm) { xfer_case(a, b); }
XFER(none_to_none, CTS_APT_NONE, CTS_APT_NONE)
XFER(sta_to_mta, CTS_APT_STA, CTS_APT_MTA)
XFER(mta_to_sta, CTS_APT_MTA, CTS_APT_STA)
XFER(sta_to_sta, CTS_APT_STA, CTS_APT_STA)
XFER(mta_to_mta, CTS_APT_MTA, CTS_APT_MTA)
XFER(sta_to_none, CTS_APT_STA, CTS_APT_NONE)
XFER(none_to_sta, CTS_APT_NONE, CTS_APT_STA)

/* A device kept alive by the main thread, objects created on threads in different apartments at the same time. */
static DWORD WINAPI shared_dev_create(cts_job *j)
{
    ID3D11Device *d = j->shared; ID3D11Buffer *b = NULL; ID3D11Texture2D *t = NULL;
    for (int i = 0; i < 20; i++) {
        HRESULT h1 = cts_make_buffer(d, 128 + i, D3D11_BIND_INDEX_BUFFER, &b);
        HRESULT h2 = cts_make_tex(d, 8 + i, 8, &t);
        if (FAILED(h1)) CTS_FAIL(j, h1);
        if (FAILED(h2)) CTS_FAIL(j, h2);
        SAFE_RELEASE(b); SAFE_RELEASE(t);
        InterlockedIncrement(&j->done);
    }
    return 0;
}
T(apartment_shared_device_mixed_apartments)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[6]; int modes[6] = { CTS_APT_NONE, CTS_APT_STA, CTS_APT_MTA, CTS_APT_NONE, CTS_APT_STA, CTS_APT_MTA };
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    memset(j, 0, sizeof j);
    for (int i = 0; i < 6; i++) { j[i].apt = modes[i]; j[i].shared = d; }
    CHECK(cts_run(6, j, shared_dev_create, 60000));
    EXPECT_EQ(cts_total_fails(6, j), 0);
    EXPECT_EQ(cts_total_done(6, j), 120);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* Reference counting has no apartment affinity: AddRef on one thread, Release on another. */
static DWORD WINAPI addref_thread(cts_job *j) { IUnknown *u = j->shared; for (int i = 0; i < 100; i++) u->lpVtbl->AddRef(u); return 0; }
static DWORD WINAPI release_thread(cts_job *j) { IUnknown *u = j->shared; for (int i = 0; i < 100; i++) u->lpVtbl->Release(u); return 0; }
T(apartment_addref_release_on_different_threads)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j; ULONG before, mid, after;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    before = d3dh_refcount(d);
    memset(&j, 0, sizeof j); j.shared = d; j.apt = CTS_APT_STA;
    CHECK(cts_run(1, &j, addref_thread, 10000));
    mid = d3dh_refcount(d);
    EXPECT_EQ(mid, before + 100);
    memset(&j, 0, sizeof j); j.shared = d; j.apt = CTS_APT_MTA;
    CHECK(cts_run(1, &j, release_thread, 10000));
    after = d3dh_refcount(d);
    EXPECT_EQ(after, before);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* Immediate context handed from thread to thread, one thread at a time. */
static DWORD WINAPI handoff_thread(cts_job *j)
{
    ID3D11DeviceContext *c = j->shared; ID3D11Buffer *b = j->priv; BYTE data[4]; D3D11_BOX box = { 0, 0, 0, 4, 1, 1 };
    memset(data, j->ops, 4);
    ID3D11DeviceContext_UpdateSubresource(c, (ID3D11Resource *)b, 0, &box, data, 0, 0);
    ID3D11DeviceContext_Flush(c);
    j->done = 1;
    return 0;
}
T(apartment_immediate_context_serial_handoff)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Buffer *b = NULL, *st = NULL; D3D11_BUFFER_DESC bd; D3D11_MAPPED_SUBRESOURCE m;
    int modes[4] = { CTS_APT_STA, CTS_APT_MTA, CTS_APT_NONE, CTS_APT_STA };
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    EXPECT_HR_OK(cts_make_buffer(d, 64, D3D11_BIND_VERTEX_BUFFER, &b));
    memset(&bd, 0, sizeof bd); bd.ByteWidth = 64; bd.Usage = D3D11_USAGE_STAGING; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    EXPECT_HR_OK(ID3D11Device_CreateBuffer(d, &bd, NULL, &st));
    if (b && st) {
        for (int i = 0; i < 4; i++) {
            cts_job j; memset(&j, 0, sizeof j); j.ops = 0x20 + i; j.apt = modes[i]; j.shared = c; j.priv = b;
            CHECK(cts_run(1, &j, handoff_thread, 20000));
            EXPECT_EQ(j.done, 1);
        }
        ID3D11DeviceContext_CopyResource(c, (ID3D11Resource *)st, (ID3D11Resource *)b);
        if (SUCCEEDED(ID3D11DeviceContext_Map(c, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m))) {
            BYTE *p = m.pData;
            EXPECT_EQ(p[0], 0x23);   /* the last writer wrote 0x20 + 3 */
            EXPECT_EQ(p[3], 0x23);
            ID3D11DeviceContext_Unmap(c, (ID3D11Resource *)st, 0);
        } else CHECK(0);
    }
    SAFE_RELEASE(st); SAFE_RELEASE(b); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* Deferred context recorded on a worker thread, command list executed on the main thread. */
static DWORD WINAPI deferred_record(cts_job *j)
{
    ID3D11Device *d = j->shared; ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL;
    D3D11_VIEWPORT vp = { 0, 0, 32, 32, 0, 1 };
    HRESULT hr = ID3D11Device_CreateDeferredContext(d, 0, &dc);
    if (FAILED(hr)) { CTS_FAIL(j, hr); return 0; }
    ID3D11DeviceContext_RSSetViewports(dc, 1, &vp);
    hr = ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl);
    if (FAILED(hr)) CTS_FAIL(j, hr);
    j->priv = cl;
    SAFE_RELEASE(dc);
    return 0;
}
T(apartment_deferred_context_cross_thread)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; cts_job j[2]; int modes[2] = { CTS_APT_STA, CTS_APT_MTA };
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    memset(j, 0, sizeof j);
    for (int i = 0; i < 2; i++) { j[i].apt = modes[i]; j[i].shared = d; }
    CHECK(cts_run(2, j, deferred_record, 30000));
    for (int i = 0; i < 2; i++) {
        EXPECT_EQ(j[i].fails, 0);
        if (j[i].priv) {
            ID3D11CommandList *cl = j[i].priv;
            ID3D11DeviceContext_ExecuteCommandList(c, cl, FALSE);
            EXPECT_EQ(ID3D11CommandList_GetContextFlags(cl), 0);
            SAFE_RELEASE(cl);
        }
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* Free-threaded marshaling is an implementation detail: record what Windows does, assert nothing. */
T(apartment_marshal_probe_note)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; IMarshal *m = NULL; HRESULT hr;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    hr = ID3D11Device_QueryInterface(d, &IID_IMarshal, (void **)&m);
    EXPECT_HR(hr, E_NOINTERFACE);       /* Windows 10: the device does not implement IMarshal itself (free-threaded marshaler is not offered) */
    if (SUCCEEDED(hr)) {
        CLSID cls; memset(&cls, 0, sizeof cls);
        HRESULT h2 = IMarshal_GetUnmarshalClass(m, &IID_ID3D11Device, d, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &cls);
        t_note("GetUnmarshalClass = %s %s", d3dh_hr_name(h2), d3dh_guid_str(&cls));
        IMarshal_Release(m);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
