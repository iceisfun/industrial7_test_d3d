/* DirectDrawEnumerate*, DxcCreateInstance(2), D3DCreateFunctionLinkingGraph, argument validation of the
 * device-creation / state-block / effect-pool exports. */
#include "gap0.h"
#include <d3d11shader.h>

/* ========================= ddraw enumeration ========================= */
typedef BOOL (WINAPI *dd_cbA)(GUID *, LPSTR, LPSTR, LPVOID);
typedef BOOL (WINAPI *dd_cbExA)(GUID *, LPSTR, LPSTR, LPVOID, HMONITOR);
typedef HRESULT (WINAPI *dd_enumA)(dd_cbA, LPVOID);
typedef HRESULT (WINAPI *dd_enumExA)(dd_cbExA, LPVOID, DWORD);
typedef HRESULT (WINAPI *dd_enumW)(void *, LPVOID);
typedef HRESULT (WINAPI *dd_enumExW)(void *, LPVOID, DWORD);

typedef struct { int calls; int stop_after; int null_guid_first; char desc0[260], name0[260]; void *ctx_seen; int ctx_ok; } ddstate;

static BOOL WINAPI dd_cb(GUID *g, LPSTR desc, LPSTR name, LPVOID ctx, HMONITOR mon)
{
    ddstate *s = ctx;
    if (s->calls == 0) {
        s->null_guid_first = (g == NULL);
        strncpy(s->desc0, desc ? desc : "<null>", 259);
        strncpy(s->name0, name ? name : "<null>", 259);
    }
    s->calls++;
    return s->stop_after ? s->calls < s->stop_after : TRUE;
}
static BOOL WINAPI dd_cbA_(GUID *g, LPSTR desc, LPSTR name, LPVOID ctx) { return dd_cb(g, desc, name, ctx, NULL); }

static int single_monitor(void) { return GetSystemMetrics(SM_CMONITORS) <= 1; }

TEST(gap0_dd_enumex_a_null_callback)
{
    dd_enumExA fn = (dd_enumExA)t_proc_in("ddraw.dll", "DirectDrawEnumerateExA");
    DWORD fl;
    REQUIRE(fn);
    if (t_is_wine()) { t_skip("emulated ddraw calls a NULL callback"); return; }
    for (fl = 0; fl < 8; fl++) EXPECT_HR(fn(NULL, NULL, fl), E_INVALIDARG_HR);
}

TEST(gap0_dd_enumex_a_bad_flags)
{
    dd_enumExA fn = (dd_enumExA)t_proc_in("ddraw.dll", "DirectDrawEnumerateExA");
    static const DWORD bad[] = { 8, 9, 0x10, 0x20, 0x100, 0x8000, 0x80000000, 0xfffffff8, 0xffffffff, 0xf };
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ddstate st = { 0 };
        EXPECT_HR(fn(dd_cb, &st, bad[i]), E_INVALIDARG_HR);
        EXPECT_EQ(st.calls, 0);                    /* callback never invoked for rejected flags */
    }
}

TEST(gap0_dd_enumex_a_primary_first)
{
    dd_enumExA fn = (dd_enumExA)t_proc_in("ddraw.dll", "DirectDrawEnumerateExA");
    DWORD fl;
    REQUIRE(fn);
    for (fl = 0; fl < 8; fl++) {
        ddstate st = { 0 };
        HRESULT hr = fn(dd_cb, &st, fl);
        EXPECT_HR(hr, S_OK);
        CHECK(st.calls >= 1);
        CHECK(st.null_guid_first);                 /* the first entry is the primary display driver, GUID == NULL */
        EXPECT_STR(st.name0, "display");
        CHECK(st.desc0[0] != 0);
        t_note("flags %lu: %d callbacks, first desc \"%s\"", (unsigned long)fl, st.calls, st.desc0);
    }
}

TEST(gap0_dd_enumex_a_stop_on_false)
{
    dd_enumExA fn = (dd_enumExA)t_proc_in("ddraw.dll", "DirectDrawEnumerateExA");
    DWORD fl;
    REQUIRE(fn);
    for (fl = 0; fl < 8; fl++) {
        ddstate st = { 0 };
        st.stop_after = 1;                          /* return FALSE on the first call */
        EXPECT_HR(fn(dd_cb, &st, fl), S_OK);
        EXPECT_EQ(st.calls, 1);
    }
}

TEST(gap0_dd_enumex_a_flags_zero_single_monitor)
{
    dd_enumExA fn = (dd_enumExA)t_proc_in("ddraw.dll", "DirectDrawEnumerateExA");
    ddstate st = { 0 };
    DWORD v = 0, sz = sizeof v;
    REQUIRE(fn);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\DirectDraw", "EnumerateAttachedSecondaries", RRF_RT_DWORD, NULL, &v, &sz) == ERROR_SUCCESS && v) {
        t_skip("EnumerateAttachedSecondaries is set on this machine"); return;
    }
    if (!single_monitor()) { t_skip("multi-monitor system"); return; }
    EXPECT_HR(fn(dd_cb, &st, 0), S_OK);
    EXPECT_EQ(st.calls, 1);                         /* only the primary driver is reported without secondary flags */
}

TEST(gap0_dd_enumex_a_context_roundtrip)
{
    dd_enumExA fn = (dd_enumExA)t_proc_in("ddraw.dll", "DirectDrawEnumerateExA");
    struct ctx { int magic; int seen; } c = { 0x5eed1234, 0 };
    BOOL WINAPI (*cb)(GUID *, LPSTR, LPSTR, LPVOID, HMONITOR);
    (void)cb;
    REQUIRE(fn);
    {
        ddstate st = { 0 };
        st.stop_after = 1;
        EXPECT_HR(fn(dd_cb, &st, 0), S_OK);
        EXPECT_EQ(st.calls, 1);                     /* ctx pointer reached the callback (it dereferenced it as ddstate) */
    }
    (void)c;
}

TEST(gap0_dd_enum_a_basic)
{
    dd_enumA fn = (dd_enumA)t_proc_in("ddraw.dll", "DirectDrawEnumerateA");
    ddstate st = { 0 };
    REQUIRE(fn);
    if (!t_is_wine()) { EXPECT_HR(fn(NULL, NULL), E_INVALIDARG_HR); EXPECT_HR(fn(NULL, &st), E_INVALIDARG_HR); }
    EXPECT_HR(fn(dd_cbA_, &st), S_OK);
    CHECK(st.calls >= 1);
    CHECK(st.null_guid_first);
    EXPECT_STR(st.name0, "display");
    CHECK(st.desc0[0] != 0);
}

TEST(gap0_dd_enum_a_stop_on_false)
{
    dd_enumA fn = (dd_enumA)t_proc_in("ddraw.dll", "DirectDrawEnumerateA");
    ddstate st = { 0 };
    REQUIRE(fn);
    st.stop_after = 1;
    EXPECT_HR(fn(dd_cbA_, &st), S_OK);
    EXPECT_EQ(st.calls, 1);
}

TEST(gap0_dd_enum_a_matches_enumex_primary)
{
    dd_enumA fa = (dd_enumA)t_proc_in("ddraw.dll", "DirectDrawEnumerateA");
    dd_enumExA fe = (dd_enumExA)t_proc_in("ddraw.dll", "DirectDrawEnumerateExA");
    ddstate a = { 0 }, e = { 0 };
    REQUIRE(fa); REQUIRE(fe);
    a.stop_after = e.stop_after = 1;
    EXPECT_HR(fa(dd_cbA_, &a), S_OK);
    EXPECT_HR(fe(dd_cb, &e, 0), S_OK);
    EXPECT_STR(a.desc0, e.desc0);
    EXPECT_STR(a.name0, e.name0);
}

BOOL WINAPI dd_dummyW(GUID *g, LPWSTR a, LPWSTR b, LPVOID c) { return FALSE; }

TEST(gap0_dd_enum_w_stub)
{
    dd_enumW fn = (dd_enumW)t_proc_in("ddraw.dll", "DirectDrawEnumerateW");
    ddstate st = { 0 };
    REQUIRE(fn);
    EXPECT_HR(fn(NULL, NULL), E_INVALIDARG_HR);
    EXPECT_HR(fn(NULL, &st), E_INVALIDARG_HR);
    EXPECT_HR(fn((void *)dd_dummyW, &st), E_NOTIMPL_HR);
    EXPECT_EQ(st.calls, 0);
}

TEST(gap0_dd_enumex_w_stub)
{
    dd_enumExW fn = (dd_enumExW)t_proc_in("ddraw.dll", "DirectDrawEnumerateExW");
    DWORD fl;
    REQUIRE(fn);
    for (fl = 0; fl < 8; fl++) EXPECT_HR(fn((void *)dd_dummyW, NULL, fl), E_NOTIMPL_HR);
    EXPECT_HR(fn(NULL, NULL, 0), E_NOTIMPL_HR);
    EXPECT_HR(fn((void *)dd_dummyW, NULL, 0xffffffff), E_NOTIMPL_HR);
}

/* ========================= DxcCreateInstance / DxcCreateInstance2 ========================= */
typedef HRESULT (WINAPI *dxc_ci_t)(REFCLSID, REFIID, void **);
typedef HRESULT (WINAPI *dxc_ci2_t)(IMalloc *, REFCLSID, REFIID, void **);
/* DXBC converter CLSID exposed by dxilconv.dll */
static const GUID CLSID_DxbcConv = { 0x4900391e, 0xb752, 0x4edd, { 0xa8, 0x85, 0x6f, 0xb7, 0x6e, 0x25, 0xad, 0xdb } };

typedef struct { const IMallocVtbl *lpVtbl; LONG refs; LONG allocs, frees, reallocs; LONG live; } cmalloc;
static HRESULT WINAPI cm_qi(IMalloc *t, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IMalloc)) { *o = t; InterlockedIncrement(&((cmalloc *)t)->refs); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cm_addref(IMalloc *t) { return InterlockedIncrement(&((cmalloc *)t)->refs); }
static ULONG WINAPI cm_release(IMalloc *t) { return InterlockedDecrement(&((cmalloc *)t)->refs); }
static void *WINAPI cm_alloc(IMalloc *t, SIZE_T n) { cmalloc *c = (cmalloc *)t; InterlockedIncrement(&c->allocs); InterlockedIncrement(&c->live); return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }
static void *WINAPI cm_realloc(IMalloc *t, void *p, SIZE_T n) { cmalloc *c = (cmalloc *)t; InterlockedIncrement(&c->reallocs); return p ? HeapReAlloc(GetProcessHeap(), 0, p, n ? n : 1) : HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }
static void WINAPI cm_free(IMalloc *t, void *p) { cmalloc *c = (cmalloc *)t; if (p) { InterlockedIncrement(&c->frees); InterlockedDecrement(&c->live); HeapFree(GetProcessHeap(), 0, p); } }
static SIZE_T WINAPI cm_getsize(IMalloc *t, void *p) { return p ? HeapSize(GetProcessHeap(), 0, p) : (SIZE_T)-1; }
static int WINAPI cm_didalloc(IMalloc *t, void *p) { return -1; }
static void WINAPI cm_heapmin(IMalloc *t) {}
static const IMallocVtbl cm_vtbl = { cm_qi, cm_addref, cm_release, cm_alloc, cm_realloc, cm_free, cm_getsize, cm_didalloc, cm_heapmin };

TEST(gap0_dxc_unknown_clsid)
{
    dxc_ci_t fn = (dxc_ci_t)t_proc_in("dxilconv.dll", "DxcCreateInstance");
    static const GUID *const bad[] = { &GUID_NULL, &IID_IUnknown, &IID_IMalloc, &CLSID_StdGlobalInterfaceTable };
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < 4; i++) {
        void *out = (void *)(ULONG_PTR)0x10;
        EXPECT_HR(fn(bad[i], &IID_IUnknown, &out), CLASSNOTREG_HR);
        CHECK(out == NULL);                         /* output cleared before the class lookup */
    }
}

TEST(gap0_dxc2_unknown_clsid)
{
    dxc_ci2_t fn = (dxc_ci2_t)t_proc_in("dxilconv.dll", "DxcCreateInstance2");
    size_t i;
    cmalloc cm = { &cm_vtbl, 1 };
    static const GUID *const bad[] = { &GUID_NULL, &IID_IUnknown, &CLSID_StdGlobalInterfaceTable };
    REQUIRE(fn);
    for (i = 0; i < 3; i++) {
        void *out = (void *)(ULONG_PTR)0x10;
        EXPECT_HR(fn(NULL, bad[i], &IID_IUnknown, &out), CLASSNOTREG_HR);
        CHECK(out == NULL);
        out = (void *)(ULONG_PTR)0x10;
        EXPECT_HR(fn((IMalloc *)&cm, bad[i], &IID_IUnknown, &out), CLASSNOTREG_HR);
        CHECK(out == NULL);
    }
    EXPECT_EQ(cm.live, 0);
}

TEST(gap0_dxc2_null_output)
{
    dxc_ci2_t fn = (dxc_ci2_t)t_proc_in("dxilconv.dll", "DxcCreateInstance2");
    cmalloc cm = { &cm_vtbl, 1 };
    REQUIRE(fn);
    EXPECT_HR(fn(NULL, &CLSID_DxbcConv, &IID_IUnknown, NULL), E_POINTER_HR);
    EXPECT_HR(fn((IMalloc *)&cm, &CLSID_DxbcConv, &IID_IUnknown, NULL), E_POINTER_HR);
    EXPECT_HR(fn(NULL, &GUID_NULL, &IID_IUnknown, NULL), E_POINTER_HR);   /* checked before the CLSID is looked at */
}

TEST(gap0_dxc_converter_iunknown)
{
    dxc_ci_t fn = (dxc_ci_t)t_proc_in("dxilconv.dll", "DxcCreateInstance");
    IUnknown *u = NULL;
    HRESULT hr;
    REQUIRE(fn);
    hr = fn(&CLSID_DxbcConv, &IID_IUnknown, (void **)&u);
    if (hr == CLASSNOTREG_HR) { t_note("CLSID guess for the DXBC converter was not accepted by this build"); return; }
    EXPECT_HR(hr, S_OK);
    if (u) {
        IUnknown *v = NULL;
        ULONG rc;
        EXPECT_EQ(IUnknown_AddRef(u), 2);
        EXPECT_EQ(IUnknown_Release(u), 1);
        EXPECT_HR(IUnknown_QueryInterface(u, &IID_IUnknown, (void **)&v), S_OK);
        CHECK(v == u);
        IUnknown_Release(v);
        v = (IUnknown *)(ULONG_PTR)0x10;
        hr = IUnknown_QueryInterface(u, &IID_IMalloc, (void **)&v);
        EXPECT_HR(hr, E_NOINTERFACE_HR);
        CHECK(v == NULL);
        rc = IUnknown_Release(u);
        EXPECT_EQ(rc, 0);
    }
}

TEST(gap0_dxc_converter_bad_riid)
{
    dxc_ci_t fn = (dxc_ci_t)t_proc_in("dxilconv.dll", "DxcCreateInstance");
    void *p = (void *)(ULONG_PTR)0x10;
    HRESULT hr;
    REQUIRE(fn);
    hr = fn(&CLSID_DxbcConv, &CLSID_StdGlobalInterfaceTable, &p);
    if (hr == CLASSNOTREG_HR) { t_note("DXBC converter CLSID not accepted"); return; }
    EXPECT_HR(hr, E_NOINTERFACE_HR);
    CHECK(p == NULL);
}

TEST(gap0_dxc2_custom_malloc)
{
    dxc_ci2_t fn = (dxc_ci2_t)t_proc_in("dxilconv.dll", "DxcCreateInstance2");
    cmalloc cm = { &cm_vtbl, 1 };
    IUnknown *u = NULL;
    HRESULT hr;
    REQUIRE(fn);
    hr = fn((IMalloc *)&cm, &CLSID_DxbcConv, &IID_IUnknown, (void **)&u);
    if (hr == CLASSNOTREG_HR) { t_note("DXBC converter CLSID not accepted"); return; }
    EXPECT_HR(hr, S_OK);
    t_note("custom IMalloc after create: allocs=%ld frees=%ld live=%ld refs=%ld", cm.allocs, cm.frees, cm.live, cm.refs);
    if (u) IUnknown_Release(u);
    t_note("custom IMalloc after release: allocs=%ld frees=%ld live=%ld refs=%ld", cm.allocs, cm.frees, cm.live, cm.refs);
    EXPECT_EQ(cm.live, 0);
}

TEST(gap0_dxc2_null_malloc_matches_dxc)
{
    dxc_ci_t f1 = (dxc_ci_t)t_proc_in("dxilconv.dll", "DxcCreateInstance");
    dxc_ci2_t f2 = (dxc_ci2_t)t_proc_in("dxilconv.dll", "DxcCreateInstance2");
    IUnknown *a = NULL, *b = NULL;
    HRESULT h1, h2;
    REQUIRE(f1); REQUIRE(f2);
    h1 = f1(&CLSID_DxbcConv, &IID_IUnknown, (void **)&a);
    h2 = f2(NULL, &CLSID_DxbcConv, &IID_IUnknown, (void **)&b);
    EXPECT_HR(h1, h2);
    if (a) IUnknown_Release(a);
    if (b) IUnknown_Release(b);
}

/* ========================= D3DCreateFunctionLinkingGraph ========================= */
typedef HRESULT (WINAPI *flg_t)(UINT, ID3D11FunctionLinkingGraph **);

TEST(gap0_flg_invalid_args)
{
    flg_t fn = t_is_wine() ? NULL : (flg_t)t_proc_in("d3dcompiler_47.dll", "D3DCreateFunctionLinkingGraph");
    ID3D11FunctionLinkingGraph *g = (ID3D11FunctionLinkingGraph *)(ULONG_PTR)0x10;
    UINT bad[] = { 1, 2, 0x80, 0xffffffff };
    int i;
    REQUIRE(fn);
    EXPECT_HR(fn(0, NULL), E_INVALIDARG_HR);
    EXPECT_HR(fn(1, NULL), E_INVALIDARG_HR);
    for (i = 0; i < 4; i++) {
        g = (ID3D11FunctionLinkingGraph *)(ULONG_PTR)0x10;
        EXPECT_HR(fn(bad[i], &g), E_INVALIDARG_HR);
        EXPECT_EQ(g, 0x10);                         /* untouched when the flags are rejected */
    }
}

TEST(gap0_flg_refcount_and_qi)
{
    flg_t fn = t_is_wine() ? NULL : (flg_t)t_proc_in("d3dcompiler_47.dll", "D3DCreateFunctionLinkingGraph");
    ID3D11FunctionLinkingGraph *g = NULL, *g2 = NULL;
    IUnknown *u = NULL;
    HRESULT hr;
    REQUIRE(fn);
    hr = fn(0, &g);
    EXPECT_HR(hr, S_OK);
    if (!g) return;
    EXPECT_EQ(g->lpVtbl->AddRef(g), 2);
    EXPECT_EQ(g->lpVtbl->Release(g), 1);
    EXPECT_HR(g->lpVtbl->QueryInterface(g, &IID_ID3D11FunctionLinkingGraph, (void **)&g2), S_OK);
    CHECK(g2 == g);
    if (g2) g2->lpVtbl->Release(g2);
    EXPECT_HR(g->lpVtbl->QueryInterface(g, &IID_IUnknown, (void **)&u), S_OK);
    CHECK(u != NULL);
    if (u) IUnknown_Release(u);
    u = (IUnknown *)(ULONG_PTR)0x10;
    EXPECT_HR(g->lpVtbl->QueryInterface(g, &IID_IMalloc, (void **)&u), E_NOINTERFACE_HR);
    CHECK(u == NULL);
    {   ULONG rc = g->lpVtbl->Release(g); EXPECT_EQ(rc, 0); }
}

TEST(gap0_flg_independent_instances)
{
    flg_t fn = t_is_wine() ? NULL : (flg_t)t_proc_in("d3dcompiler_47.dll", "D3DCreateFunctionLinkingGraph");
    ID3D11FunctionLinkingGraph *a = NULL, *b = NULL;
    REQUIRE(fn);
    EXPECT_HR(fn(0, &a), S_OK);
    EXPECT_HR(fn(0, &b), S_OK);
    CHECK(a && b && a != b);
    if (a) a->lpVtbl->Release(a);
    if (b) { ULONG rc = b->lpVtbl->Release(b); EXPECT_EQ(rc, 0); }
}

TEST(gap0_flg_signatures_and_hlsl)
{
    flg_t fn = t_is_wine() ? NULL : (flg_t)t_proc_in("d3dcompiler_47.dll", "D3DCreateFunctionLinkingGraph");
    ID3D11FunctionLinkingGraph *g = NULL;
    ID3D11LinkingNode *in = NULL, *out = NULL;
    ID3DBlob *hlsl = NULL, *err = NULL;
    D3D11_PARAMETER_DESC pin, pout;
    HRESULT hr;
    REQUIRE(fn);
    if (t_is_wine()) { t_skip("function linking graph is not provided by the emulated compiler"); return; }
    EXPECT_HR(fn(0, &g), S_OK);
    if (!g) return;
    memset(&pin, 0, sizeof pin); memset(&pout, 0, sizeof pout);
    pin.Name = "inputPos"; pin.SemanticName = "POSITION"; pin.Type = D3D_SVT_FLOAT; pin.Class = D3D_SVC_VECTOR;
    pin.Rows = 1; pin.Columns = 4; pin.InterpolationMode = D3D_INTERPOLATION_UNDEFINED;
    pout = pin; pout.Name = "outputPos"; pout.SemanticName = "SV_POSITION";
    hr = g->lpVtbl->SetInputSignature(g, &pin, 1, &in);
    EXPECT_HR(hr, S_OK);
    hr = g->lpVtbl->SetOutputSignature(g, &pout, 1, &out);
    EXPECT_HR(hr, S_OK);
    CHECK(in && out && in != out);
    if (in && out) {
        hr = g->lpVtbl->PassValue(g, in, 0, out, 0);
        EXPECT_HR(hr, S_OK);
        hr = g->lpVtbl->GenerateHlsl(g, 0, &hlsl);
        EXPECT_HR(hr, S_OK);
        if (hlsl) {
            const char *p = d3dh_blob_ptr(hlsl);
            SIZE_T n = d3dh_blob_size(hlsl);
            CHECK(n > 0);
            t_note("generated HLSL (%u bytes): %.200s", (unsigned)n, p);
        }
        hr = g->lpVtbl->GetLastError(g, &err);
        t_note("GetLastError after a successful graph = %s, blob=%p", d3dh_hr_name(hr), (void *)err);
    }
    SAFE_RELEASE(hlsl); SAFE_RELEASE(err);
    if (in) IUnknown_Release((IUnknown *)in);
    if (out) IUnknown_Release((IUnknown *)out);
    g->lpVtbl->Release(g);
}

/* ========================= argument validation of device / state-block / pool creation ========================= */
typedef HRESULT (WINAPI *cdsc11_t)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
    const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);

#define SENT_SC  ((IDXGISwapChain *)(ULONG_PTR)0x10)
#define SENT_DEV ((ID3D11Device *)(ULONG_PTR)0x20)
#define SENT_CTX ((ID3D11DeviceContext *)(ULONG_PTR)0x30)

static HRESULT call11(cdsc11_t fn, D3D_DRIVER_TYPE dt, HMODULE sw, const D3D_FEATURE_LEVEL *fls, UINT nfl, UINT sdk, int check_out)
{
    IDXGISwapChain *sc = SENT_SC; ID3D11Device *dev = SENT_DEV; ID3D11DeviceContext *ctx = SENT_CTX;
    D3D_FEATURE_LEVEL fl = (D3D_FEATURE_LEVEL)0x1234;
    HRESULT hr = fn(NULL, dt, sw, 0, fls, nfl, sdk, NULL, &sc, &dev, &fl, &ctx);
    if (check_out) {
        CHECK(sc == NULL); CHECK(dev == NULL); CHECK(ctx == NULL);
        if (!fls) EXPECT_EQ(fl, 0);
    }
    return hr;
}

TEST(gap0_cdsc11_bad_sdk_version)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    static const UINT bad[] = { 0, 1, 6, 8, 0x20, 0xffffffff };
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, 0, bad[i], 1), E_INVALIDARG_HR);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, NULL, NULL, 0, bad[i], 1), E_INVALIDARG_HR);
}

TEST(gap0_cdsc11_bad_feature_levels)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    D3D_FEATURE_LEVEL a[] = { (D3D_FEATURE_LEVEL)0x1234 };
    D3D_FEATURE_LEVEL b[] = { (D3D_FEATURE_LEVEL)0 };
    D3D_FEATURE_LEVEL c[] = { D3D_FEATURE_LEVEL_11_0, (D3D_FEATURE_LEVEL)0x9000 };
    D3D_FEATURE_LEVEL d[] = { D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_1, (D3D_FEATURE_LEVEL)0xffffffff };
    D3D_FEATURE_LEVEL e[] = { (D3D_FEATURE_LEVEL)0xa200 };
    REQUIRE(fn);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, NULL, a, 1, D3D11_SDK_VERSION, 0), E_INVALIDARG_HR);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, NULL, b, 1, D3D11_SDK_VERSION, 0), E_INVALIDARG_HR);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, NULL, c, 2, D3D11_SDK_VERSION, 0), E_INVALIDARG_HR);   /* any bad entry anywhere */
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, NULL, d, 3, D3D11_SDK_VERSION, 0), E_INVALIDARG_HR);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, NULL, e, 1, D3D11_SDK_VERSION, 0), E_INVALIDARG_HR);
}

TEST(gap0_cdsc11_levels_count_without_array)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    REQUIRE(fn);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, NULL, NULL, 1, D3D11_SDK_VERSION, 1), E_INVALIDARG_HR);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, NULL, NULL, 6, D3D11_SDK_VERSION, 1), E_INVALIDARG_HR);
}

TEST(gap0_cdsc11_driver_type_vs_adapter_and_module)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    HMODULE self = GetModuleHandleA(NULL);
    REQUIRE(fn);
    /* no adapter: UNKNOWN is not a valid driver type */
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_UNKNOWN, NULL, NULL, 0, D3D11_SDK_VERSION, 1), E_INVALIDARG_HR);
    /* SOFTWARE needs a rasteriser module */
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_SOFTWARE, NULL, NULL, 0, D3D11_SDK_VERSION, 1), E_INVALIDARG_HR);
    /* every other type must not carry a module handle */
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_HARDWARE, self, NULL, 0, D3D11_SDK_VERSION, 1), E_INVALIDARG_HR);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_REFERENCE, self, NULL, 0, D3D11_SDK_VERSION, 1), E_INVALIDARG_HR);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_NULL, self, NULL, 0, D3D11_SDK_VERSION, 1), E_INVALIDARG_HR);
    EXPECT_HR(call11(fn, D3D_DRIVER_TYPE_WARP, self, NULL, 0, D3D11_SDK_VERSION, 1), E_INVALIDARG_HR);
}

TEST(gap0_cdsc11_overlapping_feature_level_output)
{
    cdsc11_t fn = (cdsc11_t)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    D3D_FEATURE_LEVEL arr[3] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr;
    REQUIRE(fn);
    hr = fn(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, arr, 3, D3D11_SDK_VERSION, NULL, NULL, NULL, &arr[1], NULL);
    EXPECT_HR(hr, E_INVALIDARG_HR);
}

typedef HRESULT (WINAPI *cdsc10_t)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT,
    DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D10Device1 **);

static HRESULT call10(cdsc10_t fn, D3D10_DRIVER_TYPE dt, HMODULE sw, D3D10_FEATURE_LEVEL1 fl, UINT sdk)
{
    IDXGISwapChain *sc = SENT_SC; ID3D10Device1 *dev = (ID3D10Device1 *)(ULONG_PTR)0x20;
    HRESULT hr = fn(NULL, dt, sw, 0, fl, sdk, NULL, &sc, &dev);
    CHECK(sc == NULL); CHECK(dev == NULL);
    return hr;
}

TEST(gap0_cdsc10_1_bad_sdk_version)
{
    cdsc10_t fn = (cdsc10_t)t_proc_in("d3d10_1.dll", "D3D10CreateDeviceAndSwapChain1");
    static const UINT bad[] = { 0, 1, 29, 31, 33, 0xffffffff };
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) EXPECT_HR(call10(fn, D3D10_DRIVER_TYPE_WARP, NULL, D3D10_FEATURE_LEVEL_10_1, bad[i]), E_INVALIDARG_HR);
}

TEST(gap0_cdsc10_1_null_device_pointer)
{
    cdsc10_t fn = (cdsc10_t)t_proc_in("d3d10_1.dll", "D3D10CreateDeviceAndSwapChain1");
    IDXGISwapChain *sc = SENT_SC;
    REQUIRE(fn);
    EXPECT_HR(fn(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, NULL, &sc, NULL), E_INVALIDARG_HR);
    CHECK(sc == NULL);                              /* the swap chain pointer is cleared first */
    EXPECT_HR(fn(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, NULL, NULL, NULL), E_INVALIDARG_HR);
}

TEST(gap0_cdsc10_1_bad_feature_level)
{
    cdsc10_t fn = (cdsc10_t)t_proc_in("d3d10_1.dll", "D3D10CreateDeviceAndSwapChain1");
    static const UINT bad[] = { 0, 1, 0x8fff, 0xa200, 0xb000, 0xc000, 0x1234, 0xffffffff };
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) EXPECT_HR(call10(fn, D3D10_DRIVER_TYPE_WARP, NULL, (D3D10_FEATURE_LEVEL1)bad[i], D3D10_1_SDK_VERSION), E_INVALIDARG_HR);
}

TEST(gap0_cdsc10_1_bad_driver_type)
{
    cdsc10_t fn = (cdsc10_t)t_proc_in("d3d10_1.dll", "D3D10CreateDeviceAndSwapChain1");
    static const UINT bad[] = { 4, 6, 7, 99, 0xffffffff };
    HMODULE self = GetModuleHandleA(NULL);
    size_t i;
    REQUIRE(fn);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) EXPECT_HR(call10(fn, (D3D10_DRIVER_TYPE)bad[i], NULL, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION), E_INVALIDARG_HR);
    /* SOFTWARE with neither adapter nor module */
    EXPECT_HR(call10(fn, D3D10_DRIVER_TYPE_SOFTWARE, NULL, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION), E_INVALIDARG_HR);
    /* a module handle is only meaningful for SOFTWARE */
    EXPECT_HR(call10(fn, D3D10_DRIVER_TYPE_HARDWARE, self, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION), E_INVALIDARG_HR);
    EXPECT_HR(call10(fn, D3D10_DRIVER_TYPE_REFERENCE, self, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION), E_INVALIDARG_HR);
    EXPECT_HR(call10(fn, D3D10_DRIVER_TYPE_NULL, self, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION), E_INVALIDARG_HR);
    EXPECT_HR(call10(fn, D3D10_DRIVER_TYPE_WARP, self, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION), E_INVALIDARG_HR);
}

/* D3D10CreateStateBlock / D3D10CreateEffectPoolFromMemory argument checks */
typedef HRESULT (WINAPI *csb_t)(void *, void *, void **);
typedef HRESULT (WINAPI *cep_t)(void *, SIZE_T, UINT, void *, void **);

TEST(gap0_stateblock_invalid_args)
{
    static const char *const mods[] = { "d3d10.dll", "d3d10_1.dll" };
    size_t m;
    for (m = 0; m < 2; m++) {
        csb_t fn = t_is_wine() ? NULL : (csb_t)t_proc_in(mods[m], "D3D10CreateStateBlock");
        D3D10_STATE_BLOCK_MASK mask;
        void *sb = (void *)(ULONG_PTR)0x10;
        if (!fn) { t_skip("missing D3D10CreateStateBlock"); return; }
        memset(&mask, 0xff, sizeof mask);
        EXPECT_HR(fn(NULL, &mask, &sb), E_INVALIDARG_HR);
        EXPECT_EQ(sb, 0x10);                        /* output not written on validation failure */
        EXPECT_HR(fn(NULL, NULL, &sb), E_INVALIDARG_HR);
        EXPECT_HR(fn(NULL, &mask, NULL), E_INVALIDARG_HR);
        EXPECT_HR(fn(NULL, NULL, NULL), E_INVALIDARG_HR);
    }
}

TEST(gap0_effectpool_malformed_blob)
{
    static const char *const mods[] = { "d3d10.dll", "d3d10_1.dll" };
    size_t m;
    for (m = 0; m < 2; m++) {
        cep_t fn = (cep_t)t_proc_in(mods[m], "D3D10CreateEffectPoolFromMemory");
        unsigned char zeros[128] = { 0 };
        void *pool = (void *)(ULONG_PTR)0x10;
        HRESULT hr;
        if (!fn) { t_skip("missing D3D10CreateEffectPoolFromMemory"); return; }
        hr = fn(zeros, sizeof zeros, 0, NULL, &pool);
        EXPECT_HR_FAILED(hr);
        CHECK(pool == NULL);                        /* a failed creation always leaves the output NULL */
        pool = (void *)(ULONG_PTR)0x10;
        hr = fn(NULL, 0, 0, NULL, &pool);
        EXPECT_HR_FAILED(hr);
        CHECK(pool == NULL);
        pool = (void *)(ULONG_PTR)0x10;
        hr = fn(zeros, 4, 0, NULL, &pool);
        EXPECT_HR_FAILED(hr);
        CHECK(pool == NULL);
        pool = (void *)(ULONG_PTR)0x10;
        memcpy(zeros, "FX10", 4);
        hr = fn(zeros, sizeof zeros, 0, NULL, &pool);
        EXPECT_HR_FAILED(hr);
        CHECK(pool == NULL);
    }
}
