/* Concurrency: resource creation from several threads, ID3D11Multithread protection. */
#include "rc.h"

#define NTHREADS 8

typedef struct {
    ID3D11Device *dev;
    int index;
    int iterations;
    volatile LONG *go;
    int created, failed, bad_desc, bad_rc;
} worker;

static DWORD WINAPI create_worker(LPVOID arg)
{
    worker *w = arg;
    DWORD t0 = GetTickCount();
    while (!*w->go && GetTickCount() - t0 < 5000) Sleep(0);
    for (int i = 0; i < w->iterations; i++) {
        UINT bw = 16 * (1 + (w->index * 31 + i) % 64);
        D3D11_BUFFER_DESC bd = { bw, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
        ID3D11Buffer *b = NULL;
        if (FAILED(ID3D11Device_CreateBuffer(w->dev, &bd, NULL, &b)) || !b) { w->failed++; continue; }
        w->created++;
        D3D11_BUFFER_DESC g; ID3D11Buffer_GetDesc(b, &g);
        if (g.ByteWidth != bw) w->bad_desc++;
        if (d3dh_refcount(b) != 1) w->bad_rc++;
        ID3D11Buffer_Release(b);
    }
    return 0;
}

static void run_workers(LPTHREAD_START_ROUTINE fn, worker *w, int n)
{
    HANDLE h[NTHREADS];
    volatile LONG go = 0;
    int started = 0;
    for (int i = 0; i < n; i++) {
        w[i].go = &go;
        h[i] = CreateThread(NULL, 0, fn, &w[i], 0, NULL);
        if (h[i]) started++;
    }
    CHECK(started == n);
    InterlockedExchange(&go, 1);
    for (int i = 0; i < n; i++) if (h[i]) {
        DWORD r = WaitForSingleObject(h[i], 20000);
        CHECKF(r == WAIT_OBJECT_0, "worker %d did not finish", i);
        if (r != WAIT_OBJECT_0) TerminateThread(h[i], 1);
        CloseHandle(h[i]);
    }
}

TEST(d3d11_debug_refcount_threads_create_buffers_concurrently)
{
    RC_DEVICE(dev, ctx, 0);
    worker w[NTHREADS]; memset(w, 0, sizeof(w));
    for (int i = 0; i < NTHREADS; i++) { w[i].dev = dev; w[i].index = i; w[i].iterations = 200; }
    run_workers(create_worker, w, NTHREADS);
    for (int i = 0; i < NTHREADS; i++) {
        EXPECT_EQ(w[i].created, 200); EXPECT_EQ(w[i].failed, 0); EXPECT_EQ(w[i].bad_desc, 0); EXPECT_EQ(w[i].bad_rc, 0);
    }
    RC_DEVICE_END(dev, ctx);
}

/* every worker creates an object kind and keeps it live until the end, to stress concurrent registration */
typedef struct { worker base; int kind; ID3D11DeviceChild *keep[40]; int nkeep; } kworker;
static DWORD WINAPI kind_worker(LPVOID arg)
{
    kworker *w = arg;
    DWORD t0 = GetTickCount();
    while (!*w->base.go && GetTickCount() - t0 < 5000) Sleep(0);
    for (int i = 0; i < 40; i++) {
        ID3D11DeviceChild *o = NULL;
        if (FAILED(rc_make(w->kind, w->base.dev, &o))) { w->base.failed++; continue; }
        w->base.created++;
        w->keep[w->nkeep++] = o;
    }
    return 0;
}
static void kind_concurrent(int kind)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceChild *probe = NULL;
    if (FAILED(rc_make(kind, dev, &probe))) { RC_DEVICE_END(dev, ctx); t_skip("kind %d unavailable", kind); return; }
    RC_RELEASE(probe);
    kworker w[NTHREADS]; memset(w, 0, sizeof(w));
    HANDLE h[NTHREADS]; volatile LONG go = 0;
    for (int i = 0; i < NTHREADS; i++) { w[i].base.dev = dev; w[i].base.go = &go; w[i].kind = kind; h[i] = CreateThread(NULL, 0, kind_worker, &w[i], 0, NULL); }
    InterlockedExchange(&go, 1);
    for (int i = 0; i < NTHREADS; i++) if (h[i]) {
        DWORD r = WaitForSingleObject(h[i], 20000);
        CHECKF(r == WAIT_OBJECT_0, "worker %d did not finish", i);
        if (r != WAIT_OBJECT_0) TerminateThread(h[i], 1);
        CloseHandle(h[i]);
    }
    int total = 0;
    for (int i = 0; i < NTHREADS; i++) {
        EXPECT_EQ(w[i].base.failed, 0);
        total += w[i].nkeep;
        for (int j = 0; j < w[i].nkeep; j++) {
            if (rc_kind_is_state(kind)) CHECK(d3dh_refcount(w[i].keep[j]) >= 1);
            else EXPECT_EQ(d3dh_refcount(w[i].keep[j]), 1);
        }
    }
    if (rc_kind_is_state(kind)) {
        /* deduplicated: all threads share one object holding one reference per creation */
        CHECK(w[0].nkeep > 0);
        if (w[0].nkeep > 0) {
            for (int i = 0; i < NTHREADS; i++) for (int j = 0; j < w[i].nkeep; j++) CHECK(w[i].keep[j] == w[0].keep[0]);
            EXPECT_EQ(d3dh_refcount(w[0].keep[0]), (ULONG)total);
        }
    }
    for (int i = 0; i < NTHREADS; i++) for (int j = 0; j < w[i].nkeep; j++) RC_RELEASE(w[i].keep[j]);
    RC_DEVICE_END(dev, ctx);
}
#define X(k, n) TEST(d3d11_debug_refcount_threads_create_##n##_concurrently) { kind_concurrent(k); }
X(K_BUF, buffer) X(K_T2D, tex2d) X(K_SRV, srv) X(K_RTV, rtv) X(K_BLEND, blend) X(K_RAST, rasterizer) X(K_DSS, depthstencil) X(K_SAMP, sampler)
X(K_VS, vertexshader) X(K_PS, pixelshader) X(K_IL, inputlayout) X(K_QUERY, query)
#undef X

/* shared state object created and released by many threads */
static DWORD WINAPI sampler_churn(LPVOID arg)
{
    worker *w = arg;
    DWORD t0 = GetTickCount();
    while (!*w->go && GetTickCount() - t0 < 5000) Sleep(0);
    D3D11_SAMPLER_DESC d; memset(&d, 0, sizeof(d));
    d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    d.MaxAnisotropy = 1; d.ComparisonFunc = D3D11_COMPARISON_NEVER; d.MaxLOD = D3D11_FLOAT32_MAX;
    for (int i = 0; i < w->iterations; i++) {
        ID3D11SamplerState *s = NULL;
        if (FAILED(ID3D11Device_CreateSamplerState(w->dev, &d, &s)) || !s) { w->failed++; continue; }
        w->created++;
        ID3D11SamplerState_AddRef(s);
        ID3D11SamplerState_Release(s);
        ID3D11SamplerState_Release(s);
    }
    return 0;
}
TEST(d3d11_debug_refcount_threads_shared_state_object_churn)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11SamplerState *anchor = NULL;
    D3D11_SAMPLER_DESC d; memset(&d, 0, sizeof(d));
    d.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    d.MaxAnisotropy = 1; d.ComparisonFunc = D3D11_COMPARISON_NEVER; d.MaxLOD = D3D11_FLOAT32_MAX;
    EXPECT_HR(ID3D11Device_CreateSamplerState(dev, &d, &anchor), S_OK);
    worker w[NTHREADS]; memset(w, 0, sizeof(w));
    for (int i = 0; i < NTHREADS; i++) { w[i].dev = dev; w[i].index = i; w[i].iterations = 300; }
    run_workers(sampler_churn, w, NTHREADS);
    for (int i = 0; i < NTHREADS; i++) { EXPECT_EQ(w[i].created, 300); EXPECT_EQ(w[i].failed, 0); }
    if (anchor) EXPECT_EQ(d3dh_refcount(anchor), 1);   /* all concurrent references balanced out */
    SAFE_RELEASE(anchor);
    RC_DEVICE_END(dev, ctx);
}

/* AddRef/Release on one object from many threads */
static DWORD WINAPI addref_worker(LPVOID arg)
{
    worker *w = arg;
    ID3D11Buffer *b = (ID3D11Buffer *)w->dev;   /* the object is smuggled through the device slot */
    DWORD t0 = GetTickCount();
    while (!*w->go && GetTickCount() - t0 < 5000) Sleep(0);
    for (int i = 0; i < w->iterations; i++) { ID3D11Buffer_AddRef(b); }
    for (int i = 0; i < w->iterations; i++) { ID3D11Buffer_Release(b); }
    return 0;
}
TEST(d3d11_debug_refcount_threads_addref_release_balance)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceChild *o = NULL;
    if (FAILED(rc_make(K_BUF, dev, &o))) { RC_DEVICE_END(dev, ctx); t_skip("buffer unavailable"); return; }
    worker w[NTHREADS]; memset(w, 0, sizeof(w));
    for (int i = 0; i < NTHREADS; i++) { w[i].dev = (ID3D11Device *)o; w[i].iterations = 5000; }
    run_workers(addref_worker, w, NTHREADS);
    EXPECT_EQ(d3dh_refcount(o), 1);
    EXPECT_EQ(RC_RELEASE(o), 0);
    RC_DEVICE_END(dev, ctx);
}

/* GetDevice from many threads */
static DWORD WINAPI getdevice_worker(LPVOID arg)
{
    worker *w = arg;
    ID3D11DeviceChild *o = (ID3D11DeviceChild *)w->dev;
    DWORD t0 = GetTickCount();
    while (!*w->go && GetTickCount() - t0 < 5000) Sleep(0);
    for (int i = 0; i < w->iterations; i++) {
        ID3D11Device *d = NULL;
        o->lpVtbl->GetDevice(o, &d);
        if (!d) w->failed++; else ID3D11Device_Release(d);
    }
    return 0;
}
TEST(d3d11_debug_refcount_threads_getdevice_balance)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceChild *o = NULL;
    if (FAILED(rc_make(K_BUF, dev, &o))) { RC_DEVICE_END(dev, ctx); t_skip("buffer unavailable"); return; }
    ULONG base = d3dh_refcount(dev);
    worker w[NTHREADS]; memset(w, 0, sizeof(w));
    for (int i = 0; i < NTHREADS; i++) { w[i].dev = (ID3D11Device *)o; w[i].iterations = 2000; }
    run_workers(getdevice_worker, w, NTHREADS);
    for (int i = 0; i < NTHREADS; i++) EXPECT_EQ(w[i].failed, 0);
    EXPECT_EQ(d3dh_refcount(dev), base);
    RC_RELEASE(o);
    RC_DEVICE_END(dev, ctx);
}

/* ---- ID3D11Multithread ----------------------------------------------------------------------- */
TEST(d3d11_debug_refcount_multithread_default_and_toggle)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11Multithread *mt = NULL;
    HRESULT hr = RC_QI(ctx, IID_ID3D11Multithread, &mt);
    if (FAILED(hr)) { RC_DEVICE_END(dev, ctx); t_skip("ID3D11Multithread unavailable: %s", d3dh_hr_name(hr)); return; }
    BOOL def = ID3D11Multithread_GetMultithreadProtected(mt);
    EXPECT_EQ(def, FALSE);                                /* Win10: a new context is not multithread protected */
    BOOL prev = ID3D11Multithread_SetMultithreadProtected(mt, TRUE);
    EXPECT_EQ(prev, def);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), TRUE);
    prev = ID3D11Multithread_SetMultithreadProtected(mt, TRUE);
    EXPECT_EQ(prev, TRUE);
    prev = ID3D11Multithread_SetMultithreadProtected(mt, FALSE);
    EXPECT_EQ(prev, TRUE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), FALSE);
    ID3D11Multithread_SetMultithreadProtected(mt, def);
    ID3D11Multithread_Release(mt);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_multithread_default_is_off)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11Multithread *mt = NULL;
    if (FAILED(RC_QI(ctx, IID_ID3D11Multithread, &mt))) { RC_DEVICE_END(dev, ctx); t_skip("ID3D11Multithread unavailable"); return; }
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), FALSE);
    ID3D11Multithread_Release(mt);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_multithread_device_and_context_share_state)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11Multithread *a = NULL, *b = NULL;
    if (FAILED(RC_QI(ctx, IID_ID3D11Multithread, &a)) || FAILED(RC_QI(dev, IID_ID3D11Multithread, &b))) {
        SAFE_RELEASE(a); SAFE_RELEASE(b); RC_DEVICE_END(dev, ctx); t_skip("ID3D11Multithread unavailable"); return;
    }
    ID3D11Multithread_SetMultithreadProtected(a, TRUE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(b), TRUE);
    ID3D11Multithread_SetMultithreadProtected(b, FALSE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(a), FALSE);
    ID3D11Multithread_Release(a); ID3D11Multithread_Release(b);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_multithread_enter_leave_nesting)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11Multithread *mt = NULL;
    if (FAILED(RC_QI(ctx, IID_ID3D11Multithread, &mt))) { RC_DEVICE_END(dev, ctx); t_skip("ID3D11Multithread unavailable"); return; }
    ID3D11Multithread_SetMultithreadProtected(mt, TRUE);
    for (int i = 0; i < 5; i++) ID3D11Multithread_Enter(mt);
    ID3D11DeviceContext_Flush(ctx);   /* the owning thread may keep using the context while holding the lock */
    for (int i = 0; i < 5; i++) ID3D11Multithread_Leave(mt);
    ID3D11Multithread_SetMultithreadProtected(mt, FALSE);
    ID3D11Multithread_Release(mt);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_multithread_interface_refcount)
{
    RC_DEVICE(dev, ctx, 0);
    ULONG base = d3dh_refcount(ctx);
    ID3D11Multithread *mt = NULL;
    if (FAILED(RC_QI(ctx, IID_ID3D11Multithread, &mt))) { RC_DEVICE_END(dev, ctx); t_skip("ID3D11Multithread unavailable"); return; }
    EXPECT_EQ(d3dh_refcount(ctx), base + 1);              /* the interface is the context object itself */
    IUnknown *u0 = NULL, *u1 = NULL;
    RC_QI(ctx, IID_IUnknown, &u0); RC_QI(mt, IID_IUnknown, &u1);
    CHECK(u0 == u1);
    SAFE_RELEASE(u0); SAFE_RELEASE(u1);
    ID3D11Multithread_Release(mt);
    EXPECT_EQ(d3dh_refcount(ctx), base);
    RC_DEVICE_END(dev, ctx);
}

/* two threads map/unmap the same staging buffer through the immediate context, serialised by Enter/Leave */
typedef struct {
    ID3D11DeviceContext *ctx; ID3D11Multithread *mt; ID3D11Buffer *buf;
    volatile LONG *go; int id, iterations, map_fail, torn, protect;
} mapworker;
static DWORD WINAPI map_worker(LPVOID arg)
{
    mapworker *w = arg;
    DWORD t0 = GetTickCount();
    while (!*w->go && GetTickCount() - t0 < 5000) Sleep(0);
    for (int i = 0; i < w->iterations; i++) {
        D3D11_MAPPED_SUBRESOURCE m;
        if (w->protect) ID3D11Multithread_Enter(w->mt);
        HRESULT hr = ID3D11DeviceContext_Map(w->ctx, (ID3D11Resource *)w->buf, 0, D3D11_MAP_READ_WRITE, 0, &m);
        if (FAILED(hr)) { w->map_fail++; if (w->protect) ID3D11Multithread_Leave(w->mt); continue; }
        DWORD *p = m.pData;
        DWORD tag = (DWORD)(w->id << 16) | (DWORD)i;
        for (int k = 0; k < 16; k++) p[k] = tag;
        Sleep(0);   /* give the other thread a chance to collide if the lock were missing */
        for (int k = 0; k < 16; k++) if (p[k] != tag) { w->torn++; break; }
        ID3D11DeviceContext_Unmap(w->ctx, (ID3D11Resource *)w->buf, 0);
        if (w->protect) ID3D11Multithread_Leave(w->mt);
    }
    return 0;
}

static void map_two_threads(int via_enter_leave)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11Multithread *mt = NULL;
    if (FAILED(RC_QI(ctx, IID_ID3D11Multithread, &mt))) { RC_DEVICE_END(dev, ctx); t_skip("ID3D11Multithread unavailable"); return; }
    ID3D11Multithread_SetMultithreadProtected(mt, TRUE);
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0};
    ID3D11Buffer *buf = NULL;
    if (FAILED(ID3D11Device_CreateBuffer(dev, &bd, NULL, &buf))) { ID3D11Multithread_Release(mt); RC_DEVICE_END(dev, ctx); t_skip("staging buffer unavailable"); return; }
    volatile LONG go = 0;
    mapworker w[2]; HANDLE h[2];
    for (int i = 0; i < 2; i++) {
        memset(&w[i], 0, sizeof(w[i]));
        w[i].ctx = ctx; w[i].mt = mt; w[i].buf = buf; w[i].go = &go; w[i].id = i + 1; w[i].iterations = 300; w[i].protect = via_enter_leave;
        h[i] = CreateThread(NULL, 0, map_worker, &w[i], 0, NULL);
    }
    InterlockedExchange(&go, 1);
    for (int i = 0; i < 2; i++) if (h[i]) {
        DWORD r = WaitForSingleObject(h[i], 20000);
        CHECKF(r == WAIT_OBJECT_0, "map worker %d did not finish", i);
        if (r != WAIT_OBJECT_0) TerminateThread(h[i], 1);
        CloseHandle(h[i]);
    }
    for (int i = 0; i < 2; i++) { EXPECT_EQ(w[i].map_fail, 0); EXPECT_EQ(w[i].torn, 0); }
    /* the resource is fully unmapped afterwards: it can be mapped once more */
    D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)buf, 0, D3D11_MAP_READ, 0, &m), S_OK);
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)buf, 0);
    SAFE_RELEASE(buf);
    ID3D11Multithread_SetMultithreadProtected(mt, FALSE);
    ID3D11Multithread_Release(mt);
    RC_DEVICE_END(dev, ctx);
}
TEST(d3d11_debug_refcount_multithread_map_unmap_two_threads_enter_leave) { map_two_threads(1); }
TEST(d3d11_debug_refcount_multithread_map_two_resources_protected)
{
    /* protection on, no explicit Enter/Leave bracket: each call is atomic, but a Map held across the other
       thread's Map of the same subresource is a usage error, so only the reachable outcome classes are checked */
    RC_DEVICE(dev, ctx, 0);
    ID3D11Multithread *mt = NULL;
    if (FAILED(RC_QI(ctx, IID_ID3D11Multithread, &mt))) { RC_DEVICE_END(dev, ctx); t_skip("ID3D11Multithread unavailable"); return; }
    ID3D11Multithread_SetMultithreadProtected(mt, TRUE);
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0};
    ID3D11Buffer *a = NULL, *b = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &b), S_OK);
    if (a && b) {
        /* two different resources mapped at once from the one context are independent */
        D3D11_MAPPED_SUBRESOURCE ma, mb;
        EXPECT_HR(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)a, 0, D3D11_MAP_READ_WRITE, 0, &ma), S_OK);
        EXPECT_HR(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)b, 0, D3D11_MAP_READ_WRITE, 0, &mb), S_OK);
        CHECK(ma.pData != mb.pData);
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)a, 0);
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)b, 0);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    ID3D11Multithread_SetMultithreadProtected(mt, FALSE);
    ID3D11Multithread_Release(mt);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_multithread_staging_double_map_succeeds)
{
    RC_DEVICE(dev, ctx, 0);
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0};
    ID3D11Buffer *buf = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &buf), S_OK);
    if (buf) {
        D3D11_MAPPED_SUBRESOURCE m1, m2;
        EXPECT_HR(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)buf, 0, D3D11_MAP_READ_WRITE, 0, &m1), S_OK);
        HRESULT hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)buf, 0, D3D11_MAP_READ_WRITE, 0, &m2);
        EXPECT_HR(hr, S_OK);   /* Win10 22H2: the runtime does not refuse a second Map of a mapped staging subresource (no debug layer) */
        CHECK(m1.pData == m2.pData);
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)buf, 0);
        EXPECT_HR(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)buf, 0, D3D11_MAP_READ_WRITE, 0, &m1), S_OK);
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)buf, 0);
        SAFE_RELEASE(buf);
    }
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_threads_deferred_contexts_one_per_thread)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceContext *dc[4] = {0};
    int ok = 1;
    for (int i = 0; i < 4; i++) if (FAILED(ID3D11Device_CreateDeferredContext(dev, 0, &dc[i]))) ok = 0;
    if (ok) {
        for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) CHECK(dc[i] != dc[j]);
        for (int i = 0; i < 4; i++) EXPECT_EQ(d3dh_refcount(dc[i]), 1);
    }
    for (int i = 0; i < 4; i++) SAFE_RELEASE(dc[i]);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_threads_device_released_by_other_thread)
{
    RC_DEVICE(dev, ctx, 0);
    ID3D11DeviceChild *o = NULL;
    if (FAILED(rc_make(K_BUF, dev, &o))) { RC_DEVICE_END(dev, ctx); t_skip("buffer unavailable"); return; }
    SAFE_RELEASE(ctx);
    SAFE_RELEASE(dev);
    worker w; memset(&w, 0, sizeof(w));
    volatile LONG go = 1;
    w.dev = (ID3D11Device *)o; w.go = &go; w.iterations = 10;
    HANDLE h = CreateThread(NULL, 0, getdevice_worker, &w, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 20000); CloseHandle(h); }
    EXPECT_EQ(w.failed, 0);
    EXPECT_EQ(RC_RELEASE(o), 0);   /* last reference dropped on a different thread than the creator */
}
