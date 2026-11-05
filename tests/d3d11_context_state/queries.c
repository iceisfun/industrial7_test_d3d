/* Query/Begin/End/GetData, predication, annotation, multithread protection. */
#include "csh.h"

static const struct { D3D11_QUERY q; UINT size; const char *name; } QT[] = {
    { D3D11_QUERY_EVENT, 4, "EVENT" },
    { D3D11_QUERY_OCCLUSION, 8, "OCCLUSION" },
    { D3D11_QUERY_TIMESTAMP, 8, "TIMESTAMP" },
    { D3D11_QUERY_TIMESTAMP_DISJOINT, 16, "TIMESTAMP_DISJOINT" },
    { D3D11_QUERY_PIPELINE_STATISTICS, 88, "PIPELINE_STATISTICS" },
    { D3D11_QUERY_OCCLUSION_PREDICATE, 4, "OCCLUSION_PREDICATE" },
    { D3D11_QUERY_SO_STATISTICS, 16, "SO_STATISTICS" },
    { D3D11_QUERY_SO_OVERFLOW_PREDICATE, 4, "SO_OVERFLOW_PREDICATE" },
};
#define NQT (sizeof QT / sizeof QT[0])

static ID3D11Query *mkq(csf *f, D3D11_QUERY t)
{
    D3D11_QUERY_DESC d = { t, 0 };
    ID3D11Query *q = NULL;
    HRESULT hr = ID3D11Device_CreateQuery(f->dev, &d, &q);
    if (FAILED(hr)) { t_note("CreateQuery(%d): %s", t, d3dh_hr_name(hr)); return NULL; }
    return q;
}
static ID3D11Predicate *mkp(csf *f, D3D11_QUERY t)
{
    D3D11_QUERY_DESC d = { t, 0 };
    ID3D11Predicate *q = NULL;
    HRESULT hr = ID3D11Device_CreatePredicate(f->dev, &d, &q);
    if (FAILED(hr)) { t_note("CreatePredicate(%d): %s", t, d3dh_hr_name(hr)); return NULL; }
    return q;
}
/* poll GetData on the immediate context until S_OK (bounded) */
static HRESULT wait_data(csf *f, ID3D11Asynchronous *a, void *buf, UINT size, UINT flags)
{
    HRESULT hr = S_FALSE;
    ULONGLONG t0 = GetTickCount64();
    ID3D11DeviceContext_Flush(f->ctx);
    while (GetTickCount64() - t0 < 4000) {
        hr = ID3D11DeviceContext_GetData(f->ctx, a, buf, size, flags);
        if (hr != S_FALSE) return hr;
        Sleep(1);
    }
    return hr;
}
struct scene { ID3D11Texture2D *t; ID3D11RenderTargetView *rtv; ID3D11VertexShader *vs; ID3D11PixelShader *ps; };
static int scene_open(csf *f, struct scene *s)
{
    memset(s, 0, sizeof *s);
    s->t = cs_tex(f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    s->rtv = s->t ? cs_rtv(f, (ID3D11Resource *)s->t, NULL) : NULL;
    s->vs = mk_VSID(f); s->ps = mk_PS(f);
    if (!s->rtv || !s->vs || !s->ps) { t_skip("scene setup"); return 0; }
    return 1;
}
static void scene_close(struct scene *s)
{
    SAFE_RELEASE(s->vs); SAFE_RELEASE(s->ps); SAFE_RELEASE(s->rtv); SAFE_RELEASE(s->t);
}
static void scene_draw(csf *f, struct scene *s)
{
    D3D11_VIEWPORT vp = { 0, 0, 8, 8, 0, 1 };
    ID3D11DeviceContext_OMSetRenderTargets(f->ctx, 1, &s->rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(f->ctx, 1, &vp);
    ID3D11DeviceContext_IASetPrimitiveTopology(f->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(f->ctx, s->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(f->ctx, s->ps, NULL, 0);
    ID3D11DeviceContext_Draw(f->ctx, 3, 0);
}

/* ---- creation / description ------------------------------------------------ */
TEST(d3d11_context_state_query_data_sizes) {
    OPEN();
    for (unsigned i = 0; i < NQT; i++) {
        ID3D11Query *q = mkq(&f, QT[i].q);
        if (!q) { t_note("%s not creatable", QT[i].name); continue; }
        UINT sz = ID3D11Query_GetDataSize(q);
        CHECKF(sz == QT[i].size, "%s: GetDataSize=%u want %u", QT[i].name, sz, QT[i].size);
        D3D11_QUERY_DESC d = { 0xff, 0xff };
        ID3D11Query_GetDesc(q, &d);
        CHECKF(d.Query == QT[i].q && d.MiscFlags == 0, "%s: desc %d/%u", QT[i].name, d.Query, d.MiscFlags);
        ID3D11Device *dv = NULL; ID3D11Query_GetDevice(q, &dv);
        CHECK(dv == f.dev); SAFE_RELEASE(dv);
        ID3D11Asynchronous *as = NULL;
        EXPECT_HR(ID3D11Query_QueryInterface(q, &IID_ID3D11Asynchronous, (void **)&as), S_OK);
        EXPECT_EQ((ID3D11Query *)as, q);
        SAFE_RELEASE(as);
        EXPECT_EQ(ID3D11Query_Release(q), 0);
    }
    CLOSE();
}
TEST(d3d11_context_state_query_struct_sizes) {
    CHECK(sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT) == 16);
    CHECK(sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS) == 88);
    CHECK(sizeof(D3D11_QUERY_DATA_SO_STATISTICS) == 16);
}
TEST(d3d11_context_state_query_create_invalid) {
    OPEN();
    ID3D11Query *q = (void *)1;
    D3D11_QUERY_DESC d = { (D3D11_QUERY)200, 0 };
    EXPECT_HR(ID3D11Device_CreateQuery(f.dev, &d, &q), E_INVALIDARG);
    d.Query = D3D11_QUERY_EVENT;
    EXPECT_HR(ID3D11Device_CreateQuery(f.dev, NULL, &q), E_INVALIDARG);
    d.MiscFlags = 0x80;
    q = NULL;
    HRESULT hr = ID3D11Device_CreateQuery(f.dev, &d, &q);
    t_note("CreateQuery(EVENT, MiscFlags=0x80) -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(q);
    CLOSE();
}
TEST(d3d11_context_state_query_create_validate_only) {
    OPEN();
    D3D11_QUERY_DESC d = { D3D11_QUERY_EVENT, 0 };
    HRESULT hr = ID3D11Device_CreateQuery(f.dev, &d, NULL);
    t_note("CreateQuery(valid desc, NULL out) -> %s", d3dh_hr_name(hr));
    CHECKF(hr == S_FALSE || hr == S_OK, "got %s", d3dh_hr_name(hr));
    d.Query = (D3D11_QUERY)200;
    hr = ID3D11Device_CreateQuery(f.dev, &d, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    CLOSE();
}
TEST(d3d11_context_state_predicate_create_types) {
    OPEN();
    ID3D11Predicate *p = mkp(&f, D3D11_QUERY_OCCLUSION_PREDICATE);
    CHECK(p != NULL);
    if (p) {
        EXPECT_EQ(ID3D11Predicate_GetDataSize(p), 4);
        D3D11_QUERY_DESC d; ID3D11Predicate_GetDesc(p, &d);
        EXPECT_EQ(d.Query, D3D11_QUERY_OCCLUSION_PREDICATE);
        ID3D11Asynchronous *as = NULL;
        EXPECT_HR(ID3D11Predicate_QueryInterface(p, &IID_ID3D11Asynchronous, (void **)&as), S_OK);
        SAFE_RELEASE(as);
        ID3D11Predicate_Release(p);
    }
    p = mkp(&f, D3D11_QUERY_SO_OVERFLOW_PREDICATE);
    CHECK(p != NULL);
    if (p) { EXPECT_EQ(ID3D11Predicate_GetDataSize(p), 4); ID3D11Predicate_Release(p); }
    CLOSE();
}
TEST(d3d11_context_state_predicate_create_non_predicate_types) {
    OPEN();
    static const D3D11_QUERY bad[] = { D3D11_QUERY_EVENT, D3D11_QUERY_OCCLUSION, D3D11_QUERY_TIMESTAMP,
        D3D11_QUERY_TIMESTAMP_DISJOINT, D3D11_QUERY_PIPELINE_STATISTICS, D3D11_QUERY_SO_STATISTICS };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ID3D11Predicate *p = (void *)1;
        D3D11_QUERY_DESC d = { bad[i], 0 };
        HRESULT hr = ID3D11Device_CreatePredicate(f.dev, &d, &p);
        CHECKF(hr == E_INVALIDARG, "CreatePredicate(type %d): %s", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) ID3D11Predicate_Release(p);
    }
    CLOSE();
}
TEST(d3d11_context_state_query_predicate_hint_flag) {
    OPEN();
    D3D11_QUERY_DESC d = { D3D11_QUERY_OCCLUSION_PREDICATE, D3D11_QUERY_MISC_PREDICATEHINT };
    ID3D11Query *q = NULL;
    HRESULT hr = ID3D11Device_CreateQuery(f.dev, &d, &q);
    EXPECT_HR(hr, S_OK);
    if (q) {
        D3D11_QUERY_DESC g; ID3D11Query_GetDesc(q, &g);
        EXPECT_EQ(g.MiscFlags, D3D11_QUERY_MISC_PREDICATEHINT);
        ID3D11Query_Release(q);
    }
    CLOSE();
}

/* ---- GetData protocol -------------------------------------------------------- */
TEST(d3d11_context_state_event_query_completes) {
    OPEN();
    ID3D11Query *q = mkq(&f, D3D11_QUERY_EVENT);
    if (!q) { t_skip("query"); CLOSE(); return; }
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    BOOL v = FALSE;
    HRESULT hr = ID3D11DeviceContext_GetData(f.ctx, (ID3D11Asynchronous *)q, NULL, 0, 0);
    CHECKF(hr == S_OK || hr == S_FALSE, "first status poll %s", d3dh_hr_name(hr));
    hr = wait_data(&f, (ID3D11Asynchronous *)q, &v, sizeof v, 0);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(v, TRUE);
    /* data stays available, repeatable */
    v = FALSE;
    EXPECT_HR(ID3D11DeviceContext_GetData(f.ctx, (ID3D11Asynchronous *)q, &v, sizeof v, 0), S_OK);
    EXPECT_EQ(v, TRUE);
    EXPECT_HR(ID3D11DeviceContext_GetData(f.ctx, (ID3D11Asynchronous *)q, NULL, 0, 0), S_OK);
    ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_event_query_reissue) {
    OPEN();
    ID3D11Query *q = mkq(&f, D3D11_QUERY_EVENT);
    if (!q) { t_skip("query"); CLOSE(); return; }
    for (int i = 0; i < 5; i++) {
        BOOL v = FALSE;
        ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
        EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &v, sizeof v, 0), S_OK);
        EXPECT_EQ(v, TRUE);
    }
    ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_getdata_do_not_flush) {
    OPEN();
    ID3D11Query *q = mkq(&f, D3D11_QUERY_EVENT);
    if (!q) { t_skip("query"); CLOSE(); return; }
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    BOOL v = 0;
    HRESULT hr = ID3D11DeviceContext_GetData(f.ctx, (ID3D11Asynchronous *)q, &v, sizeof v, D3D11_ASYNC_GETDATA_DONOTFLUSH);
    CHECKF(hr == S_OK || hr == S_FALSE, "DONOTFLUSH poll: %s", d3dh_hr_name(hr));
    if (hr == S_OK) EXPECT_EQ(v, TRUE);
    t_note("GetData(DONOTFLUSH) right after End: %s", d3dh_hr_name(hr));
    ID3D11DeviceContext_Flush(f.ctx);
    v = 0;
    hr = wait_data(&f, (ID3D11Asynchronous *)q, &v, sizeof v, D3D11_ASYNC_GETDATA_DONOTFLUSH);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(v, TRUE);
    ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_getdata_bad_size) {
    OPEN();
    ID3D11Query *q = mkq(&f, D3D11_QUERY_EVENT);
    if (!q) { t_skip("query"); CLOSE(); return; }
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    UINT64 big[2] = { 0 };
    EXPECT_HR(ID3D11DeviceContext_GetData(f.ctx, (ID3D11Asynchronous *)q, big, 2, 0), E_INVALIDARG);
    EXPECT_HR(ID3D11DeviceContext_GetData(f.ctx, (ID3D11Asynchronous *)q, big, 16, 0), E_INVALIDARG);
    EXPECT_HR(ID3D11DeviceContext_GetData(f.ctx, (ID3D11Asynchronous *)q, NULL, 4, 0), E_INVALIDARG);
    EXPECT_EQ(big[0], 0);
    ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_getdata_not_ended_observed) {
    OPEN();
    for (unsigned i = 0; i < NQT; i++) {
        ID3D11Query *q = mkq(&f, QT[i].q);
        if (!q) continue;
        HRESULT hr = ID3D11DeviceContext_GetData(f.ctx, (ID3D11Asynchronous *)q, NULL, 0, 0);
        t_note("%s: GetData before any End -> %s", QT[i].name, d3dh_hr_name(hr));
        ID3D11Query_Release(q);
    }
    CLOSE();
}
TEST(d3d11_context_state_getdata_deferred_invalid) {
    OPEN();
    ID3D11DeviceContext *d = NULL;
    ID3D11Query *q = mkq(&f, D3D11_QUERY_EVENT);
    if (!q || FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d))) { t_skip("setup"); SAFE_RELEASE(q); CLOSE(); return; }
    BOOL v = 0;
    EXPECT_HR(ID3D11DeviceContext_GetData(d, (ID3D11Asynchronous *)q, NULL, 0, 0), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(ID3D11DeviceContext_GetData(d, (ID3D11Asynchronous *)q, &v, sizeof v, 0), DXGI_ERROR_INVALID_CALL);
    ID3D11DeviceContext_End(d, (ID3D11Asynchronous *)q);
    EXPECT_HR(ID3D11DeviceContext_GetData(d, (ID3D11Asynchronous *)q, &v, sizeof v, 0), DXGI_ERROR_INVALID_CALL);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    EXPECT_HR(ID3D11DeviceContext_GetData(d, (ID3D11Asynchronous *)q, &v, sizeof v, 0), DXGI_ERROR_INVALID_CALL);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    EXPECT_HR(ID3D11DeviceContext_GetData(d, (ID3D11Asynchronous *)q, &v, sizeof v, 0), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &v, sizeof v, 0), S_OK);
    EXPECT_EQ(v, TRUE);
    SAFE_RELEASE(cl); ID3D11DeviceContext_Release(d); ID3D11Query_Release(q);
    CLOSE();
}

/* ---- results ------------------------------------------------------------------ */
TEST(d3d11_context_state_occlusion_counts_pixels) {
    OPEN();
    struct scene s; ID3D11Query *q = mkq(&f, D3D11_QUERY_OCCLUSION);
    if (!q || !scene_open(&f, &s)) { SAFE_RELEASE(q); CLOSE(); return; }
    UINT64 n = 0xdead;
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    scene_draw(&f, &s);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &n, sizeof n, 0), S_OK);
    EXPECT_EQ(n, 64);
    /* the same query object can be reused; an empty interval counts 0 */
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    n = 0xdead;
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &n, sizeof n, 0), S_OK);
    EXPECT_EQ(n, 0);
    /* and two draws accumulate */
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    scene_draw(&f, &s); scene_draw(&f, &s);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &n, sizeof n, 0), S_OK);
    EXPECT_EQ(n, 128);
    ID3D11DeviceContext_ClearState(f.ctx);
    scene_close(&s); ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_occlusion_scissored_count) {
    OPEN();
    struct scene s; ID3D11Query *q = mkq(&f, D3D11_QUERY_OCCLUSION);
    ID3D11RasterizerState *rs = cs_rs(&f, D3D11_CULL_NONE, TRUE);
    if (!q || !rs || !scene_open(&f, &s)) { SAFE_RELEASE(q); SAFE_RELEASE(rs); CLOSE(); return; }
    D3D11_RECT r = { 2, 2, 6, 5 }; /* 4 x 3 */
    ID3D11DeviceContext_RSSetState(f.ctx, rs);
    UINT64 n = 0;
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    scene_draw(&f, &s);   /* sets viewport; scissor set after so it persists */
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_RSSetScissorRects(f.ctx, 1, &r);
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_Draw(f.ctx, 3, 0);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &n, sizeof n, 0), S_OK);
    EXPECT_EQ(n, 12);
    ID3D11DeviceContext_ClearState(f.ctx);
    scene_close(&s); ID3D11Query_Release(q); ID3D11RasterizerState_Release(rs);
    CLOSE();
}
TEST(d3d11_context_state_occlusion_predicate_value) {
    OPEN();
    struct scene s; ID3D11Query *q = mkq(&f, D3D11_QUERY_OCCLUSION_PREDICATE);
    if (!q || !scene_open(&f, &s)) { SAFE_RELEASE(q); CLOSE(); return; }
    BOOL v = 0x55;
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &v, sizeof v, 0), S_OK);
    EXPECT_EQ(v, FALSE);
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    scene_draw(&f, &s);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &v, sizeof v, 0), S_OK);
    EXPECT_EQ(v, TRUE);
    ID3D11DeviceContext_ClearState(f.ctx);
    scene_close(&s); ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_timestamp_disjoint) {
    OPEN();
    ID3D11Query *dj = mkq(&f, D3D11_QUERY_TIMESTAMP_DISJOINT), *t1 = mkq(&f, D3D11_QUERY_TIMESTAMP), *t2 = mkq(&f, D3D11_QUERY_TIMESTAMP);
    if (!dj || !t1 || !t2) { t_skip("queries"); goto out; }
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)dj);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)t1);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)t2);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)dj);
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dd; memset(&dd, 0xcc, sizeof dd);
    UINT64 a = 0, b = 0;
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)dj, &dd, sizeof dd, 0), S_OK);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)t1, &a, sizeof a, 0), S_OK);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)t2, &b, sizeof b, 0), S_OK);
    CHECK(dd.Disjoint == TRUE || dd.Disjoint == FALSE);
    t_note("timestamp frequency=%llu disjoint=%d t1=%llu t2=%llu", (unsigned long long)dd.Frequency, dd.Disjoint, (unsigned long long)a, (unsigned long long)b);
    if (!dd.Disjoint) { CHECK(dd.Frequency != 0); CHECK(b >= a); }
out:
    SAFE_RELEASE(dj); SAFE_RELEASE(t1); SAFE_RELEASE(t2);
    CLOSE();
}
TEST(d3d11_context_state_timestamp_monotonic_pair) {
    OPEN();
    ID3D11Query *dj = mkq(&f, D3D11_QUERY_TIMESTAMP_DISJOINT), *t[4] = { 0 };
    int ok = dj != NULL;
    for (int i = 0; i < 4; i++) { t[i] = mkq(&f, D3D11_QUERY_TIMESTAMP); if (!t[i]) ok = 0; }
    if (!ok) { t_skip("queries"); goto out; }
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)dj);
    for (int i = 0; i < 4; i++) { ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)t[i]); ID3D11DeviceContext_Flush(f.ctx); }
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)dj);
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dd;
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)dj, &dd, sizeof dd, 0), S_OK);
    UINT64 v[4];
    for (int i = 0; i < 4; i++) EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)t[i], &v[i], sizeof v[i], 0), S_OK);
    if (!dd.Disjoint) for (int i = 1; i < 4; i++) CHECKF(v[i] >= v[i - 1], "timestamp %d went backwards", i);
out:
    SAFE_RELEASE(dj); for (int i = 0; i < 4; i++) SAFE_RELEASE(t[i]);
    CLOSE();
}
TEST(d3d11_context_state_pipeline_statistics_draw) {
    OPEN();
    struct scene s; ID3D11Query *q = mkq(&f, D3D11_QUERY_PIPELINE_STATISTICS);
    if (!q || !scene_open(&f, &s)) { SAFE_RELEASE(q); CLOSE(); return; }
    D3D11_QUERY_DATA_PIPELINE_STATISTICS st; memset(&st, 0xcc, sizeof st);
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    scene_draw(&f, &s);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &st, sizeof st, 0), S_OK);
    EXPECT_EQ(st.IAVertices, 3);
    EXPECT_EQ(st.IAPrimitives, 1);
    EXPECT_EQ(st.GSInvocations, 0); EXPECT_EQ(st.GSPrimitives, 0);
    EXPECT_EQ(st.HSInvocations, 0); EXPECT_EQ(st.DSInvocations, 0); EXPECT_EQ(st.CSInvocations, 0);
    t_note("stats: VS=%llu CIn=%llu CPrim=%llu PS=%llu", (unsigned long long)st.VSInvocations, (unsigned long long)st.CInvocations,
           (unsigned long long)st.CPrimitives, (unsigned long long)st.PSInvocations);
    ID3D11DeviceContext_ClearState(f.ctx);
    scene_close(&s); ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_pipeline_statistics_empty) {
    OPEN();
    ID3D11Query *q = mkq(&f, D3D11_QUERY_PIPELINE_STATISTICS);
    if (!q) { t_skip("query"); CLOSE(); return; }
    D3D11_QUERY_DATA_PIPELINE_STATISTICS st; memset(&st, 0xcc, sizeof st);
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &st, sizeof st, 0), S_OK);
    const UINT64 *p = (const UINT64 *)&st;
    for (int i = 0; i < 11; i++) CHECKF(p[i] == 0, "counter %d = %llu", i, (unsigned long long)p[i]);
    ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_pipeline_statistics_dispatch) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Query *q = mkq(&f, D3D11_QUERY_PIPELINE_STATISTICS);
    ID3D11ComputeShader *cs = mk_CS(&f);
    if (!q || !cs) { t_skip("setup"); SAFE_RELEASE(q); SAFE_RELEASE(cs); CLOSE(); return; }
    D3D11_QUERY_DATA_PIPELINE_STATISTICS st; memset(&st, 0xcc, sizeof st);
    ID3D11DeviceContext_CSSetShader(f.ctx, cs, NULL, 0);
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_Dispatch(f.ctx, 3, 2, 1);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &st, sizeof st, 0), S_OK);
    EXPECT_EQ(st.CSInvocations, 6);
    EXPECT_EQ(st.IAVertices, 0); EXPECT_EQ(st.VSInvocations, 0); EXPECT_EQ(st.PSInvocations, 0);
    ID3D11DeviceContext_ClearState(f.ctx);
    ID3D11ComputeShader_Release(cs); ID3D11Query_Release(q);
    CLOSE();
}
TEST(d3d11_context_state_so_statistics_empty) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_10_0);
    ID3D11Query *q = mkq(&f, D3D11_QUERY_SO_STATISTICS), *p = mkq(&f, D3D11_QUERY_SO_OVERFLOW_PREDICATE);
    if (!q || !p) { t_skip("queries"); goto out; }
    D3D11_QUERY_DATA_SO_STATISTICS st; memset(&st, 0xcc, sizeof st);
    BOOL ov = 0x55;
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)p);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)p);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &st, sizeof st, 0), S_OK);
    EXPECT_EQ(st.NumPrimitivesWritten, 0); EXPECT_EQ(st.PrimitivesStorageNeeded, 0);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)p, &ov, sizeof ov, 0), S_OK);
    EXPECT_EQ(ov, FALSE);
out:
    SAFE_RELEASE(q); SAFE_RELEASE(p);
    CLOSE();
}
TEST(d3d11_context_state_query_independent_queries) {
    OPEN();
    struct scene s; ID3D11Query *a = mkq(&f, D3D11_QUERY_OCCLUSION), *b = mkq(&f, D3D11_QUERY_OCCLUSION);
    if (!a || !b || !scene_open(&f, &s)) { SAFE_RELEASE(a); SAFE_RELEASE(b); CLOSE(); return; }
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)a);
    scene_draw(&f, &s);
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)b);
    ID3D11DeviceContext_Draw(f.ctx, 3, 0);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)b);
    ID3D11DeviceContext_Draw(f.ctx, 3, 0);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)a);
    UINT64 na = 0, nb = 0;
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)a, &na, sizeof na, 0), S_OK);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)b, &nb, sizeof nb, 0), S_OK);
    EXPECT_EQ(na, 192); EXPECT_EQ(nb, 64);
    ID3D11DeviceContext_ClearState(f.ctx);
    scene_close(&s); ID3D11Query_Release(a); ID3D11Query_Release(b);
    CLOSE();
}
TEST(d3d11_context_state_query_refcount_during_flight) {
    OPEN();
    ID3D11Query *q = mkq(&f, D3D11_QUERY_EVENT);
    if (!q) { t_skip("query"); CLOSE(); return; }
    ULONG base = RC(q);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)q);
    ULONG after = RC(q);
    t_note("query refcount base=%lu after End=%lu", base, after);
    ID3D11Query_Release(q); /* released while in flight: must not crash, nothing left to observe */
    ID3D11DeviceContext_Flush(f.ctx);
    CLOSE();
}
TEST(d3d11_context_state_query_deferred_begin_end) {
    OPEN();
    struct scene s; ID3D11DeviceContext *d = NULL;
    ID3D11Query *q = mkq(&f, D3D11_QUERY_OCCLUSION);
    if (!q || !scene_open(&f, &s) || FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d))) { t_skip("setup"); SAFE_RELEASE(q); CLOSE(); return; }
    D3D11_VIEWPORT vp = { 0, 0, 8, 8, 0, 1 };
    ID3D11DeviceContext_OMSetRenderTargets(d, 1, &s.rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(d, 1, &vp);
    ID3D11DeviceContext_IASetPrimitiveTopology(d, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(d, s.vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(d, s.ps, NULL, 0);
    ID3D11DeviceContext_Begin(d, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_Draw(d, 3, 0);
    ID3D11DeviceContext_End(d, (ID3D11Asynchronous *)q);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    UINT64 n = 0;
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &n, sizeof n, 0), S_OK);
    EXPECT_EQ(n, 64);
    /* the list can be executed again, re-running the query */
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    n = 0;
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)q, &n, sizeof n, 0), S_OK);
    EXPECT_EQ(n, 64);
    SAFE_RELEASE(cl); ID3D11DeviceContext_Release(d);
    scene_close(&s); ID3D11Query_Release(q);
    CLOSE();
}

/* ---- predication ------------------------------------------------------------- */
TEST(d3d11_context_state_predication_roundtrip) {
    OPEN();
    ID3D11Predicate *p = mkp(&f, D3D11_QUERY_OCCLUSION_PREDICATE);
    if (!p) { t_skip("predicate"); CLOSE(); return; }
    ULONG base = RC(p);
    ID3D11Predicate *g = NULL; BOOL v = 0x55;
    ID3D11DeviceContext_SetPredication(f.ctx, p, TRUE);
    CHECK(RC(p) > base);
    ID3D11DeviceContext_GetPredication(f.ctx, &g, &v);
    EXPECT_EQ(g, p); EXPECT_EQ(v, TRUE);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_SetPredication(f.ctx, p, FALSE);
    ID3D11DeviceContext_GetPredication(f.ctx, &g, &v);
    EXPECT_EQ(g, p); EXPECT_EQ(v, FALSE);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_GetPredication(f.ctx, &g, NULL); EXPECT_EQ(g, p); SAFE_RELEASE(g);
    v = 0x55; ID3D11DeviceContext_GetPredication(f.ctx, NULL, &v); EXPECT_EQ(v, FALSE);
    ID3D11DeviceContext_SetPredication(f.ctx, NULL, FALSE);
    EXPECT_EQ(RC(p), base);
    ID3D11Predicate_Release(p);
    CLOSE();
}
TEST(d3d11_context_state_predication_null_preserves_value) {
    OPEN();
    ID3D11Predicate *g = (void *)1; BOOL v = 0;
    ID3D11DeviceContext_SetPredication(f.ctx, NULL, TRUE);
    ID3D11DeviceContext_GetPredication(f.ctx, &g, &v);
    CHECK(g == NULL); EXPECT_EQ(v, TRUE);
    ID3D11DeviceContext_SetPredication(f.ctx, NULL, FALSE);
    v = 1; ID3D11DeviceContext_GetPredication(f.ctx, &g, &v);
    CHECK(g == NULL); EXPECT_EQ(v, FALSE);
    CLOSE();
}
TEST(d3d11_context_state_predication_so_overflow_predicate_bind) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_10_0);
    ID3D11Predicate *p = mkp(&f, D3D11_QUERY_SO_OVERFLOW_PREDICATE);
    if (!p) { t_skip("predicate"); CLOSE(); return; }
    ID3D11DeviceContext_SetPredication(f.ctx, p, TRUE);
    ID3D11Predicate *g = NULL; BOOL v = 0;
    ID3D11DeviceContext_GetPredication(f.ctx, &g, &v);
    EXPECT_EQ(g, p); EXPECT_EQ(v, TRUE); SAFE_RELEASE(g);
    ID3D11DeviceContext_SetPredication(f.ctx, NULL, FALSE);
    ID3D11Predicate_Release(p);
    CLOSE();
}
TEST(d3d11_context_state_predication_deferred_roundtrip) {
    OPEN();
    ID3D11DeviceContext *d = NULL;
    ID3D11Predicate *p = mkp(&f, D3D11_QUERY_OCCLUSION_PREDICATE);
    if (!p || FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d))) { t_skip("setup"); SAFE_RELEASE(p); CLOSE(); return; }
    ID3D11Predicate *g = NULL; BOOL v = 0;
    ID3D11DeviceContext_SetPredication(d, p, TRUE);
    ID3D11DeviceContext_GetPredication(d, &g, &v);
    EXPECT_EQ(g, p); EXPECT_EQ(v, TRUE); SAFE_RELEASE(g);
    g = (void *)1; ID3D11DeviceContext_GetPredication(f.ctx, &g, &v);
    CHECK(g == NULL);
    ID3D11DeviceContext_SetPredication(d, NULL, FALSE);
    ID3D11DeviceContext_Release(d); ID3D11Predicate_Release(p);
    CLOSE();
}
static void predicated_draw(csf *f, struct scene *s, ID3D11Predicate *pred, BOOL pv, uint32_t *px)
{
    FLOAT black[4] = { 0, 0, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f->ctx, s->rtv, black);
    ID3D11DeviceContext_SetPredication(f->ctx, pred, pv);
    scene_draw(f, s);
    ID3D11DeviceContext_SetPredication(f->ctx, NULL, FALSE);
    if (!cs_read_px(f, s->t, 4, 4, px)) *px = 0xbad;
}
TEST(d3d11_context_state_predication_skips_draw) {
    OPEN();
    struct scene s; ID3D11Predicate *p = mkp(&f, D3D11_QUERY_OCCLUSION_PREDICATE);
    if (!p || !scene_open(&f, &s)) { SAFE_RELEASE(p); CLOSE(); return; }
    BOOL r = 0;
    /* make the predicate TRUE (samples passed) */
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)p);
    scene_draw(&f, &s);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)p);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)p, &r, sizeof r, 0), S_OK);
    EXPECT_EQ(r, TRUE);
    uint32_t px;
    /* commands are NOT performed when the predicate data equals PredicateValue */
    predicated_draw(&f, &s, p, TRUE, &px);  EXPECT_EQ(px, 0xff000000u);
    predicated_draw(&f, &s, p, FALSE, &px); EXPECT_EQ(px, 0xffffffffu);
    /* now make the predicate FALSE (nothing drawn) */
    ID3D11DeviceContext_Begin(f.ctx, (ID3D11Asynchronous *)p);
    ID3D11DeviceContext_End(f.ctx, (ID3D11Asynchronous *)p);
    EXPECT_HR(wait_data(&f, (ID3D11Asynchronous *)p, &r, sizeof r, 0), S_OK);
    EXPECT_EQ(r, FALSE);
    predicated_draw(&f, &s, p, FALSE, &px); EXPECT_EQ(px, 0xff000000u);
    predicated_draw(&f, &s, p, TRUE, &px);  EXPECT_EQ(px, 0xffffffffu);
    ID3D11DeviceContext_ClearState(f.ctx);
    scene_close(&s); ID3D11Predicate_Release(p);
    CLOSE();
}
TEST(d3d11_context_state_predication_null_draws) {
    OPEN();
    struct scene s;
    if (!scene_open(&f, &s)) { CLOSE(); return; }
    uint32_t px;
    predicated_draw(&f, &s, NULL, TRUE, &px);
    EXPECT_EQ(px, 0xffffffffu);
    predicated_draw(&f, &s, NULL, FALSE, &px);
    EXPECT_EQ(px, 0xffffffffu);
    ID3D11DeviceContext_ClearState(f.ctx);
    scene_close(&s);
    CLOSE();
}

/* ---- annotation ------------------------------------------------------------------ */
static void annotation_checks(ID3D11DeviceContext *c)
{
    ID3DUserDefinedAnnotation *a = NULL;
    HRESULT hr = ID3D11DeviceContext_QueryInterface(c, &IID_ID3DUserDefinedAnnotation, (void **)&a);
    EXPECT_HR(hr, S_OK);
    if (!a) return;
    /* not running under a profiling tool: levels are -1 and the status is FALSE */
    EXPECT_EQ(ID3DUserDefinedAnnotation_BeginEvent(a, L"outer"), -1);
    EXPECT_EQ(ID3DUserDefinedAnnotation_BeginEvent(a, L"inner"), -1);
    ID3DUserDefinedAnnotation_SetMarker(a, L"marker");
    EXPECT_EQ(ID3DUserDefinedAnnotation_EndEvent(a), -1);
    EXPECT_EQ(ID3DUserDefinedAnnotation_EndEvent(a), -1);
    EXPECT_EQ(ID3DUserDefinedAnnotation_EndEvent(a), -1); /* unbalanced End is harmless */
    EXPECT_EQ(ID3DUserDefinedAnnotation_GetStatus(a), FALSE);
    ID3DUserDefinedAnnotation_Release(a);
}
TEST(d3d11_context_state_annotation_immediate) {
    OPEN();
    annotation_checks(f.ctx);
    CLOSE();
}
TEST(d3d11_context_state_annotation_deferred) {
    OPEN();
    ID3D11DeviceContext *d = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d))) { t_skip("deferred"); CLOSE(); return; }
    annotation_checks(d);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    SAFE_RELEASE(cl);
    ID3D11DeviceContext_Release(d);
    CLOSE();
}
TEST(d3d11_context_state_annotation_shares_context_identity) {
    OPEN();
    ID3DUserDefinedAnnotation *a = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3DUserDefinedAnnotation, (void **)&a))) { t_skip("no annotation"); CLOSE(); return; }
    ULONG rc = RC(f.ctx);
    IUnknown *u = NULL;
    ID3DUserDefinedAnnotation_QueryInterface(a, &IID_ID3D11DeviceContext, (void **)&u);
    t_note("annotation->QI(ID3D11DeviceContext) -> %p (context %p), ctx refcount %lu -> %lu", u, f.ctx, rc, RC(f.ctx));
    if (u) u->lpVtbl->Release(u);
    ID3DUserDefinedAnnotation_Release(a);
    CLOSE();
}

/* ---- multithread ----------------------------------------------------------------- */
TEST(d3d11_context_state_multithread_default_and_toggle) {
    OPEN();
    ID3D11Multithread *m = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Multithread, (void **)&m))) { t_skip("no ID3D11Multithread"); CLOSE(); return; }
    BOOL def = ID3D11Multithread_GetMultithreadProtected(m);
    t_note("default GetMultithreadProtected=%d", def);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(m, TRUE), def);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(m), TRUE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(m, TRUE), TRUE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(m, FALSE), TRUE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(m), FALSE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(m, FALSE), FALSE);
    ID3D11Multithread_SetMultithreadProtected(m, def);
    ID3D11Multithread_Release(m);
    CLOSE();
}
TEST(d3d11_context_state_multithread_default_false) {
    OPEN();
    ID3D11Multithread *m = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Multithread, (void **)&m))) { t_skip("no ID3D11Multithread"); CLOSE(); return; }
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(m), FALSE);
    ID3D11Multithread_Release(m);
    CLOSE();
}
TEST(d3d11_context_state_multithread_shared_between_interfaces) {
    OPEN();
    ID3D11Multithread *a = NULL, *b = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Multithread, (void **)&a))) { t_skip("no ID3D11Multithread"); CLOSE(); return; }
    ID3D11Device_GetImmediateContext(f.dev, (ID3D11DeviceContext **)&b);
    ID3D11DeviceContext_Release((ID3D11DeviceContext *)b); b = NULL;
    ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Multithread, (void **)&b);
    EXPECT_EQ(a, b);
    ID3D11Multithread_SetMultithreadProtected(a, TRUE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(b), TRUE);
    ID3D11Multithread_SetMultithreadProtected(a, FALSE);
    ID3D11Multithread_Release(a); ID3D11Multithread_Release(b);
    CLOSE();
}
TEST(d3d11_context_state_multithread_enter_leave_recursive) {
    OPEN();
    ID3D11Multithread *m = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Multithread, (void **)&m))) { t_skip("no ID3D11Multithread"); CLOSE(); return; }
    ID3D11Multithread_SetMultithreadProtected(m, TRUE);
    ID3D11Multithread_Enter(m);
    ID3D11Multithread_Enter(m);
    ID3D11DeviceContext_Flush(f.ctx);
    ID3D11Multithread_Leave(m);
    ID3D11Multithread_Leave(m);
    ID3D11Multithread_SetMultithreadProtected(m, FALSE);
    ID3D11Multithread_Release(m);
    CLOSE();
}
struct mt_arg { ID3D11Multithread *m; HANDLE ev; };
static DWORD WINAPI mt_thread(void *p)
{
    struct mt_arg *a = p;
    ID3D11Multithread_Enter(a->m);
    ID3D11Multithread_Leave(a->m);
    SetEvent(a->ev);
    return 0;
}
TEST(d3d11_context_state_multithread_enter_blocks_other_thread) {
    OPEN();
    ID3D11Multithread *m = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Multithread, (void **)&m))) { t_skip("no ID3D11Multithread"); CLOSE(); return; }
    ID3D11Multithread_SetMultithreadProtected(m, TRUE);
    struct mt_arg a = { m, CreateEventW(NULL, TRUE, FALSE, NULL) };
    ID3D11Multithread_Enter(m);
    HANDLE th = CreateThread(NULL, 0, mt_thread, &a, 0, NULL);
    if (!th) { t_skip("thread"); ID3D11Multithread_Leave(m); goto out; }
    /* while this thread holds the device lock the other thread cannot get through */
    DWORD w = WaitForSingleObject(a.ev, 300);
    CHECKF(w == WAIT_TIMEOUT, "other thread entered while the lock was held (wait=%lu)", w);
    ID3D11Multithread_Leave(m);
    w = WaitForSingleObject(a.ev, 5000);
    CHECKF(w == WAIT_OBJECT_0, "other thread never got the lock after Leave (wait=%lu)", w);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
out:
    CloseHandle(a.ev);
    ID3D11Multithread_SetMultithreadProtected(m, FALSE);
    ID3D11Multithread_Release(m);
    CLOSE();
}
TEST(d3d11_context_state_multithread_singlethreaded_device_observed) {
    csf f; memset(&f, 0, sizeof f);
    HRESULT hr = d3dh_make_device11(&f.dev, &f.ctx, &f.fl, &f.dt, D3D11_CREATE_DEVICE_SINGLETHREADED);
    if (FAILED(hr)) { t_skip("no singlethreaded device: %s", d3dh_hr_name(hr)); return; }
    ID3D11Multithread *m = NULL;
    hr = ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Multithread, (void **)&m);
    t_note("QI ID3D11Multithread on D3D11_CREATE_DEVICE_SINGLETHREADED device: %s", d3dh_hr_name(hr));
    if (m) {
        t_note("protected default=%d, Set(TRUE) returned %d", ID3D11Multithread_GetMultithreadProtected(m), ID3D11Multithread_SetMultithreadProtected(m, TRUE));
        ID3D11Multithread_SetMultithreadProtected(m, FALSE);
        ID3D11Multithread_Release(m);
    }
    CLOSE();
}
