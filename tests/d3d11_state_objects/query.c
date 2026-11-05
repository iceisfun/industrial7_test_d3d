/* Query, predicate and counter creation. */
#include "so_common.h"

static const struct { int type; UINT size; int pred; const char *name; } QT[16] = {
    { D3D11_QUERY_EVENT, 4, 0, "EVENT" },
    { D3D11_QUERY_OCCLUSION, 8, 0, "OCCLUSION" },
    { D3D11_QUERY_TIMESTAMP, 8, 0, "TIMESTAMP" },
    { D3D11_QUERY_TIMESTAMP_DISJOINT, 16, 0, "TIMESTAMP_DISJOINT" },
    { D3D11_QUERY_PIPELINE_STATISTICS, 88, 0, "PIPELINE_STATISTICS" },
    { D3D11_QUERY_OCCLUSION_PREDICATE, 4, 1, "OCCLUSION_PREDICATE" },
    { D3D11_QUERY_SO_STATISTICS, 16, 0, "SO_STATISTICS" },
    { D3D11_QUERY_SO_OVERFLOW_PREDICATE, 4, 1, "SO_OVERFLOW_PREDICATE" },
    { D3D11_QUERY_SO_STATISTICS_STREAM0, 16, 0, "SO_STATISTICS_STREAM0" },
    { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0, 4, 1, "SO_OVERFLOW_PREDICATE_STREAM0" },
    { D3D11_QUERY_SO_STATISTICS_STREAM1, 16, 0, "SO_STATISTICS_STREAM1" },
    { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1, 4, 1, "SO_OVERFLOW_PREDICATE_STREAM1" },
    { D3D11_QUERY_SO_STATISTICS_STREAM2, 16, 0, "SO_STATISTICS_STREAM2" },
    { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2, 4, 1, "SO_OVERFLOW_PREDICATE_STREAM2" },
    { D3D11_QUERY_SO_STATISTICS_STREAM3, 16, 0, "SO_STATISTICS_STREAM3" },
    { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3, 4, 1, "SO_OVERFLOW_PREDICATE_STREAM3" },
};
#define CQ(d, o) ID3D11Device_CreateQuery(so_dev, (d), (o))
#define CP(d, o) ID3D11Device_CreatePredicate(so_dev, (d), (o))
static int type_supported(int t) { return t < 8 || so_fl >= D3D_FEATURE_LEVEL_11_0; }

ST(query_equal_types_numeric_values)
{
    /* the enumerators are part of the ABI */
    int i;
    for (i = 0; i < 16; i++) CHECKF(QT[i].type == i, "type %s", QT[i].name);
}
ST(query_null_desc)
{
    ID3D11Query *q = SO_SENTINEL(ID3D11Query);
    SO_NEED();
    EXPECT_HR(CQ(NULL, NULL), E_INVALIDARG);
    EXPECT_HR(CQ(NULL, &q), E_INVALIDARG);
    CHECK(q == NULL);
}
ST(query_create_every_type)
{
    int i;
    SO_NEED();
    for (i = 0; i < 16; i++) {
        D3D11_QUERY_DESC d;
        ID3D11Query *q = NULL;
        HRESULT hr;
        d.Query = (D3D11_QUERY)i;
        d.MiscFlags = 0;
        hr = CQ(&d, NULL);
        t_check(hr == (type_supported(i) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "validate", "%s got %s", QT[i].name, d3dh_hr_name(hr));
        hr = CQ(&d, &q);
        t_check(hr == (type_supported(i) ? S_OK : E_INVALIDARG), __FILE__, __LINE__, "create", "%s got %s", QT[i].name, d3dh_hr_name(hr));
        SAFE_RELEASE(q);
    }
}
ST(query_out_of_range_types)
{
    static const int bad[] = { -1, 16, 17, 100, 0x7FFFFFFF, INT_MIN };
    unsigned i;
    SO_NEED();
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        D3D11_QUERY_DESC d;
        ID3D11Query *q = SO_SENTINEL(ID3D11Query);
        HRESULT hr;
        d.Query = (D3D11_QUERY)bad[i];
        d.MiscFlags = 0;
        hr = CQ(&d, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "validate", "type %d got %s", bad[i], d3dh_hr_name(hr));
        hr = CQ(&d, &q);
        t_check(hr == E_INVALIDARG && q == NULL, __FILE__, __LINE__, "create", "type %d got %s ptr %p", bad[i], d3dh_hr_name(hr), (void *)q);
    }
}
ST(query_getdesc_roundtrip_every_type)
{
    int i;
    SO_NEED();
    for (i = 0; i < 16; i++) {
        D3D11_QUERY_DESC d, g;
        ID3D11Query *q = NULL;
        if (!type_supported(i)) continue;
        d.Query = (D3D11_QUERY)i;
        d.MiscFlags = 0;
        EXPECT_HR(CQ(&d, &q), S_OK);
        if (!q) continue;
        memset(&g, 0x77, sizeof g);
        ID3D11Query_GetDesc(q, &g);
        t_check(g.Query == i && g.MiscFlags == 0, __FILE__, __LINE__, "desc", "%s got %d/%u", QT[i].name, g.Query, g.MiscFlags);
        SAFE_RELEASE(q);
    }
}
ST(query_getdatasize_every_type)
{
    int i;
    SO_NEED();
    for (i = 0; i < 16; i++) {
        D3D11_QUERY_DESC d;
        ID3D11Query *q = NULL;
        UINT sz;
        if (!type_supported(i)) continue;
        d.Query = (D3D11_QUERY)i;
        d.MiscFlags = 0;
        EXPECT_HR(CQ(&d, &q), S_OK);
        if (!q) continue;
        sz = ID3D11Query_GetDataSize(q);
        t_check(sz == QT[i].size, __FILE__, __LINE__, "size", "%s got %u want %u", QT[i].name, sz, QT[i].size);
        SAFE_RELEASE(q);
    }
}
ST(query_data_size_matches_sdk_structs)
{
    CHECK(sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS) == 88);
    CHECK(sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT) == 16);
    CHECK(sizeof(D3D11_QUERY_DATA_SO_STATISTICS) == 16);
}
ST(query_qi_predicate_only_for_predicate_types)
{
    int i;
    SO_NEED();
    for (i = 0; i < 16; i++) {
        D3D11_QUERY_DESC d;
        ID3D11Query *q = NULL;
        ID3D11Predicate *p = SO_SENTINEL(ID3D11Predicate);
        HRESULT hr;
        if (!type_supported(i)) continue;
        d.Query = (D3D11_QUERY)i;
        d.MiscFlags = 0;
        EXPECT_HR(CQ(&d, &q), S_OK);
        if (!q) continue;
        hr = ID3D11Query_QueryInterface(q, &IID_ID3D11Predicate, (void **)&p);
        t_check(hr == (QT[i].pred ? S_OK : E_NOINTERFACE), __FILE__, __LINE__, "predicate qi", "%s got %s", QT[i].name, d3dh_hr_name(hr));
        if (hr == S_OK) SAFE_RELEASE(p); else CHECK(p == NULL);
        SAFE_RELEASE(q);
    }
}
ST(query_each_create_returns_new_object)
{
    D3D11_QUERY_DESC d;
    ID3D11Query *a = NULL, *b = NULL;
    SO_NEED();
    d.Query = D3D11_QUERY_EVENT;
    d.MiscFlags = 0;
    EXPECT_HR(CQ(&d, &a), S_OK);
    EXPECT_HR(CQ(&d, &b), S_OK);
    CHECK(a != NULL && b != NULL);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(query_misc_flags_on_non_predicate_types)
{
    int i;
    UINT mf;
    SO_NEED();
    for (i = 0; i < 16; i++) {
        if (i == D3D11_QUERY_OCCLUSION_PREDICATE || !type_supported(i)) continue;
        for (mf = 1; mf <= 3; mf++) {
            D3D11_QUERY_DESC d;
            HRESULT hr;
            d.Query = (D3D11_QUERY)i;
            d.MiscFlags = mf;
            hr = CQ(&d, NULL);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "misc", "%s misc %u got %s", QT[i].name, mf, d3dh_hr_name(hr));
        }
    }
}
ST(query_occlusion_predicate_predicatehint_flag)
{
    D3D11_QUERY_DESC d, g;
    ID3D11Query *q = NULL;
    UINT mf;
    SO_NEED();
    d.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
    for (mf = 0; mf <= 4; mf++) {
        HRESULT hr;
        d.MiscFlags = mf;
        hr = CQ(&d, NULL);
        t_check(hr == (mf <= 1 ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "misc", "misc %u got %s", mf, d3dh_hr_name(hr));
    }
    d.MiscFlags = D3D11_QUERY_MISC_PREDICATEHINT;
    EXPECT_HR(CQ(&d, &q), S_OK);
    if (q) {
        ID3D11Query_GetDesc(q, &g);
        EXPECT_EQ(g.Query, D3D11_QUERY_OCCLUSION_PREDICATE);
        EXPECT_EQ(g.MiscFlags, D3D11_QUERY_MISC_PREDICATEHINT);
        SAFE_RELEASE(q);
    }
    d.MiscFlags = 0x80000000u;
    EXPECT_HR(CQ(&d, NULL), E_INVALIDARG);
}
ST(query_occlusion_predicate_misc_flags_high_bits)
{
    D3D11_QUERY_DESC d;
    SO_NEED();
    d.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
    d.MiscFlags = 0xFFFFFFFFu;
    EXPECT_HR(CQ(&d, NULL), E_INVALIDARG);
    d.MiscFlags = 0xFFFFFFFEu;
    EXPECT_HR(CQ(&d, NULL), E_INVALIDARG);
}
ST(query_event_roundtrip_with_context)
{
    D3D11_QUERY_DESC d;
    ID3D11Query *q = NULL;
    BOOL done = FALSE;
    HRESULT hr = S_FALSE;
    DWORD t0;
    SO_NEED();
    d.Query = D3D11_QUERY_EVENT;
    d.MiscFlags = 0;
    EXPECT_HR(CQ(&d, &q), S_OK);
    if (!q) return;
    ID3D11DeviceContext_End(so_ctx, (ID3D11Asynchronous *)q);
    ID3D11DeviceContext_Flush(so_ctx);
    t0 = GetTickCount();
    while (GetTickCount() - t0 < 3000) {
        hr = ID3D11DeviceContext_GetData(so_ctx, (ID3D11Asynchronous *)q, &done, sizeof done, 0);
        if (hr == S_OK) break;
        Sleep(1);
    }
    EXPECT_HR(hr, S_OK);
    if (hr == S_OK) EXPECT_EQ(done, TRUE);
    SAFE_RELEASE(q);
}
ST(query_getdata_before_end_observation)
{
    /* before End, an event query has produced nothing */
    D3D11_QUERY_DESC d;
    ID3D11Query *q = NULL;
    BOOL v = 0x55;
    HRESULT hr;
    SO_NEED();
    d.Query = D3D11_QUERY_EVENT;
    d.MiscFlags = 0;
    EXPECT_HR(CQ(&d, &q), S_OK);
    if (!q) return;
    hr = ID3D11DeviceContext_GetData(so_ctx, (ID3D11Asynchronous *)q, &v, sizeof v, 0);
    t_note("GetData on an event query that was never ended -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(q);
}

/* ---- predicates ---------------------------------------------------------------------- */
ST(predicate_null_desc)
{
    ID3D11Predicate *p = NULL;
    SO_NEED();
    EXPECT_HR(CP(NULL, NULL), E_INVALIDARG);
    EXPECT_HR(CP(NULL, &p), E_INVALIDARG);
    CHECK(p == NULL);
}
ST(predicate_only_two_base_types)
{
    int i;
    SO_NEED();
    for (i = -1; i <= 17; i++) {
        D3D11_QUERY_DESC d;
        HRESULT hr, want;
        d.Query = (D3D11_QUERY)i;
        d.MiscFlags = 0;
        hr = CP(&d, NULL);
        want = (i == D3D11_QUERY_OCCLUSION_PREDICATE || i == D3D11_QUERY_SO_OVERFLOW_PREDICATE) ? S_FALSE : E_INVALIDARG;
        t_check(hr == want, __FILE__, __LINE__, "predicate", "type %d got %s want %s", i, d3dh_hr_name(hr), d3dh_hr_name(want));
    }
}
ST(predicate_create_occlusion_predicate)
{
    D3D11_QUERY_DESC d, g;
    ID3D11Predicate *p = NULL;
    ID3D11Query *q = NULL;
    SO_NEED();
    d.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
    d.MiscFlags = 0;
    EXPECT_HR(CP(&d, &p), S_OK);
    if (!p) return;
    ID3D11Predicate_GetDesc(p, &g);
    EXPECT_EQ(g.Query, D3D11_QUERY_OCCLUSION_PREDICATE);
    EXPECT_EQ(g.MiscFlags, 0);
    EXPECT_EQ(ID3D11Predicate_GetDataSize(p), 4);
    EXPECT_HR(ID3D11Predicate_QueryInterface(p, &IID_ID3D11Query, (void **)&q), S_OK);
    SAFE_RELEASE(q);
    SAFE_RELEASE(p);
}
ST(predicate_create_so_overflow_predicate)
{
    D3D11_QUERY_DESC d, g;
    ID3D11Predicate *p = NULL;
    SO_NEED();
    d.Query = D3D11_QUERY_SO_OVERFLOW_PREDICATE;
    d.MiscFlags = 0;
    EXPECT_HR(CP(&d, &p), S_OK);
    if (!p) return;
    ID3D11Predicate_GetDesc(p, &g);
    EXPECT_EQ(g.Query, D3D11_QUERY_SO_OVERFLOW_PREDICATE);
    EXPECT_EQ(ID3D11Predicate_GetDataSize(p), 4);
    SAFE_RELEASE(p);
}
ST(predicate_stream_variants_rejected_by_createpredicate)
{
    static const int t[] = { D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0, D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1,
                             D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2, D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3 };
    unsigned i;
    SO_NEED();
    for (i = 0; i < 4; i++) {
        D3D11_QUERY_DESC d;
        HRESULT hr;
        d.Query = (D3D11_QUERY)t[i];
        d.MiscFlags = 0;
        hr = CP(&d, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "stream predicate", "type %d got %s", t[i], d3dh_hr_name(hr));
    }
}
ST(predicate_misc_flags)
{
    D3D11_QUERY_DESC d;
    SO_NEED();
    d.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
    d.MiscFlags = D3D11_QUERY_MISC_PREDICATEHINT;
    EXPECT_HR(CP(&d, NULL), S_FALSE);
    d.MiscFlags = 2;
    EXPECT_HR(CP(&d, NULL), E_INVALIDARG);
    d.Query = D3D11_QUERY_SO_OVERFLOW_PREDICATE;
    d.MiscFlags = D3D11_QUERY_MISC_PREDICATEHINT;
    EXPECT_HR(CP(&d, NULL), E_INVALIDARG);
}
ST(predicate_each_create_new_object)
{
    D3D11_QUERY_DESC d;
    ID3D11Predicate *a = NULL, *b = NULL;
    SO_NEED();
    d.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
    d.MiscFlags = 0;
    EXPECT_HR(CP(&d, &a), S_OK);
    EXPECT_HR(CP(&d, &b), S_OK);
    EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
ST(predicate_set_predication_roundtrip)
{
    D3D11_QUERY_DESC d;
    ID3D11Predicate *p = NULL, *g = NULL;
    BOOL val = 7;
    SO_NEED();
    d.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
    d.MiscFlags = 0;
    EXPECT_HR(CP(&d, &p), S_OK);
    if (!p) return;
    ID3D11DeviceContext_SetPredication(so_ctx, p, TRUE);
    ID3D11DeviceContext_GetPredication(so_ctx, &g, &val);
    EXPECT_EQ(g, p);
    EXPECT_EQ(val, TRUE);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_SetPredication(so_ctx, NULL, FALSE);
    ID3D11DeviceContext_GetPredication(so_ctx, &g, &val);
    CHECK(g == NULL);
    EXPECT_EQ(val, FALSE);
    SAFE_RELEASE(p);
}
ST(predicate_qi_from_query_object_same_identity)
{
    D3D11_QUERY_DESC d;
    ID3D11Query *q = NULL;
    ID3D11Predicate *p = NULL;
    IUnknown *u1 = NULL, *u2 = NULL;
    SO_NEED();
    d.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
    d.MiscFlags = 0;
    EXPECT_HR(CQ(&d, &q), S_OK);
    if (!q) return;
    EXPECT_HR(ID3D11Query_QueryInterface(q, &IID_ID3D11Predicate, (void **)&p), S_OK);
    if (p) {
        ID3D11Query_QueryInterface(q, &IID_IUnknown, (void **)&u1);
        ID3D11Predicate_QueryInterface(p, &IID_IUnknown, (void **)&u2);
        EXPECT_EQ(u1, u2);
        SAFE_RELEASE(u1); SAFE_RELEASE(u2); SAFE_RELEASE(p);
    }
    SAFE_RELEASE(q);
}

/* ---- Query1 / context types ------------------------------------------------------------ */
ST(query1_null_desc)
{
    SO_NEED3();
    EXPECT_HR(ID3D11Device3_CreateQuery1(so_dev3(), NULL, NULL), E_INVALIDARG);
}
ST(query1_default_context_type_all)
{
    D3D11_QUERY_DESC1 d, g;
    ID3D11Query1 *q = NULL;
    SO_NEED3();
    d.Query = D3D11_QUERY_OCCLUSION;
    d.MiscFlags = 0;
    d.ContextType = D3D11_CONTEXT_TYPE_ALL;
    EXPECT_HR(ID3D11Device3_CreateQuery1(so_dev3(), &d, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device3_CreateQuery1(so_dev3(), &d, &q), S_OK);
    if (!q) return;
    memset(&g, 0x44, sizeof g);
    ID3D11Query1_GetDesc1(q, &g);
    EXPECT_EQ(g.Query, D3D11_QUERY_OCCLUSION);
    EXPECT_EQ(g.MiscFlags, 0);
    EXPECT_EQ(g.ContextType, D3D11_CONTEXT_TYPE_ALL);
    SAFE_RELEASE(q);
}
ST(query1_context_type_beyond_video_rejected)
{
    int v;
    SO_NEED3();
    for (v = 5; v < 9; v++) {
        D3D11_QUERY_DESC1 d;
        HRESULT hr;
        d.Query = D3D11_QUERY_EVENT;
        d.MiscFlags = 0;
        d.ContextType = (D3D11_CONTEXT_TYPE)v;
        hr = ID3D11Device3_CreateQuery1(so_dev3(), &d, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "ctx", "context %d got %s", v, d3dh_hr_name(hr));
    }
}
ST(query1_simple_types_accept_all_context_types)
{
    static const int types[] = { D3D11_QUERY_EVENT, D3D11_QUERY_OCCLUSION, D3D11_QUERY_TIMESTAMP, D3D11_QUERY_TIMESTAMP_DISJOINT };
    unsigned i;
    int c;
    SO_NEED3();
    for (i = 0; i < 4; i++)
        for (c = 0; c <= 4; c++) {
            D3D11_QUERY_DESC1 d;
            HRESULT hr;
            d.Query = (D3D11_QUERY)types[i];
            d.MiscFlags = 0;
            d.ContextType = (D3D11_CONTEXT_TYPE)c;
            hr = ID3D11Device3_CreateQuery1(so_dev3(), &d, NULL);
            t_check(hr == S_FALSE, __FILE__, __LINE__, "ctx", "type %d context %d got %s", types[i], c, d3dh_hr_name(hr));
        }
}
ST(query1_statistics_types_only_all_or_3d_context)
{
    static const int types[] = { D3D11_QUERY_PIPELINE_STATISTICS, D3D11_QUERY_OCCLUSION_PREDICATE, D3D11_QUERY_SO_STATISTICS, D3D11_QUERY_SO_OVERFLOW_PREDICATE };
    unsigned i;
    int c;
    SO_NEED3();
    for (i = 0; i < 4; i++)
        for (c = 0; c <= 4; c++) {
            D3D11_QUERY_DESC1 d;
            HRESULT hr;
            d.Query = (D3D11_QUERY)types[i];
            d.MiscFlags = 0;
            d.ContextType = (D3D11_CONTEXT_TYPE)c;
            hr = ID3D11Device3_CreateQuery1(so_dev3(), &d, NULL);
            t_check(hr == (c <= 1 ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "ctx", "type %d context %d got %s", types[i], c, d3dh_hr_name(hr));
        }
}
ST(query1_getdesc_downlevel_matches)
{
    D3D11_QUERY_DESC1 d;
    D3D11_QUERY_DESC g;
    ID3D11Query1 *q = NULL;
    SO_NEED3();
    d.Query = D3D11_QUERY_TIMESTAMP;
    d.MiscFlags = 0;
    d.ContextType = D3D11_CONTEXT_TYPE_3D;
    EXPECT_HR(ID3D11Device3_CreateQuery1(so_dev3(), &d, &q), S_OK);
    if (!q) return;
    ID3D11Query1_GetDesc(q, &g);
    EXPECT_EQ(g.Query, D3D11_QUERY_TIMESTAMP);
    EXPECT_EQ(g.MiscFlags, 0);
    SAFE_RELEASE(q);
}

/* ---- counters ------------------------------------------------------------------------- */
ST(counter_checkcounterinfo_consistent)
{
    D3D11_COUNTER_INFO ci;
    SO_NEED();
    memset(&ci, 0xEE, sizeof ci);
    ID3D11Device_CheckCounterInfo(so_dev, &ci);
    t_note("counters: LastDeviceDependentCounter=%d NumSimultaneousCounters=%u NumDetectableParallelUnits=%u",
           (int)ci.LastDeviceDependentCounter, ci.NumSimultaneousCounters, ci.NumDetectableParallelUnits);
    CHECK(ci.LastDeviceDependentCounter == 0 || ci.LastDeviceDependentCounter >= D3D11_COUNTER_DEVICE_DEPENDENT_0);
}
ST(counter_create_without_device_counters_fails)
{
    D3D11_COUNTER_INFO ci;
    D3D11_COUNTER_DESC d;
    ID3D11Counter *c = SO_SENTINEL(ID3D11Counter);
    HRESULT hr;
    SO_NEED();
    memset(&ci, 0, sizeof ci);
    ID3D11Device_CheckCounterInfo(so_dev, &ci);
    d.Counter = D3D11_COUNTER_DEVICE_DEPENDENT_0;
    d.MiscFlags = 0;
    if (ci.LastDeviceDependentCounter != 0) { t_skip("device exposes counters"); return; }
    hr = ID3D11Device_CreateCounter(so_dev, &d, &c);
    EXPECT_HR_FAILED(hr);
    t_note("CreateCounter(DEVICE_DEPENDENT_0) without counters -> %s", d3dh_hr_name(hr));
    hr = ID3D11Device_CreateCounter(so_dev, &d, NULL);
    EXPECT_HR_FAILED(hr);
}
ST(counter_create_invalid_counter_value_fails)
{
    static const int vals[] = { 0, 1, 0x3FFFFFFF, 0x7FFFFFFF, (int)0xFFFFFFFF };
    unsigned i;
    SO_NEED();
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        D3D11_COUNTER_DESC d;
        ID3D11Counter *c = NULL;
        HRESULT hr;
        d.Counter = (D3D11_COUNTER)vals[i];
        d.MiscFlags = 0;
        hr = ID3D11Device_CreateCounter(so_dev, &d, &c);
        t_check(FAILED(hr) && c == NULL, __FILE__, __LINE__, "counter", "counter 0x%x got %s", vals[i], d3dh_hr_name(hr));
        SAFE_RELEASE(c);
    }
}
ST(counter_check_counter_unsupported)
{
    D3D11_COUNTER_DESC d;
    D3D11_COUNTER_TYPE t;
    UINT active = 0, nl = 0, nu = 0, nd = 0;
    HRESULT hr;
    SO_NEED();
    d.Counter = (D3D11_COUNTER)0x12345;
    d.MiscFlags = 0;
    t = (D3D11_COUNTER_TYPE)99;
    hr = ID3D11Device_CheckCounter(so_dev, &d, &t, &active, NULL, &nl, NULL, &nu, NULL, &nd);
    EXPECT_HR_FAILED(hr);
    t_note("CheckCounter(0x12345) -> %s", d3dh_hr_name(hr));
}
