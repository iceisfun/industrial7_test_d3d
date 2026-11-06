/* More SDK-layer behaviour measured on Win10 22H2 (WARP, Graphics Tools): object lifetime messages, live-object reports,
 * draw validation text, concurrency of the message queue. Every test skips when the debug device cannot be created. */
#include "rc.h"

typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; ID3D11Debug *dbg; ID3D11InfoQueue *iq; } d2_fix;
static int d2_open(d2_fix *f)
{
    memset(f, 0, sizeof(*f));
    if (!d3dh_D3D11CreateDevice()) { t_skip("missing export: D3D11CreateDevice"); return 0; }
    HRESULT hr = d3dh_make_device11(&f->dev, &f->ctx, NULL, NULL, D3D11_CREATE_DEVICE_DEBUG);
    if (FAILED(hr)) { t_skip("debug device unavailable (%s): SDK layers not installed?", d3dh_hr_name(hr)); return 0; }
    if (FAILED(RC_QI(f->dev, IID_ID3D11Debug, &f->dbg)) || FAILED(RC_QI(f->dev, IID_ID3D11InfoQueue, &f->iq))) {
        SAFE_RELEASE(f->dbg); SAFE_RELEASE(f->iq); SAFE_RELEASE(f->ctx); SAFE_RELEASE(f->dev); t_skip("debug interfaces missing"); return 0;
    }
    ID3D11InfoQueue_SetMuteDebugOutput(f->iq, TRUE);
    ID3D11InfoQueue_ClearStorageFilter(f->iq);      /* the default filter hides INFO */
    ID3D11InfoQueue_ClearStoredMessages(f->iq);
    return 1;
}
static void d2_close(d2_fix *f) { SAFE_RELEASE(f->iq); SAFE_RELEASE(f->dbg); SAFE_RELEASE(f->ctx); SAFE_RELEASE(f->dev); }
#define D2(f) d2_fix f; if (!d2_open(&f)) return

static D3D11_MESSAGE *d2_msg(ID3D11InfoQueue *iq, UINT64 idx)
{
    SIZE_T len = 0;
    if (ID3D11InfoQueue_GetMessage(iq, idx, NULL, &len) != S_FALSE || !len) return NULL;
    D3D11_MESSAGE *m = malloc(len);
    if (ID3D11InfoQueue_GetMessage(iq, idx, m, &len) != S_OK) { free(m); return NULL; }
    return m;
}
/* index of the first stored message with the given id, or -1 */
static int d2_find(ID3D11InfoQueue *iq, D3D11_MESSAGE_ID id, const char *needle)
{
    UINT64 n = ID3D11InfoQueue_GetNumStoredMessages(iq);
    for (UINT64 i = 0; i < n; i++) {
        D3D11_MESSAGE *m = d2_msg(iq, i);
        int hit = m && m->ID == id && (!needle || (m->pDescription && strstr(m->pDescription, needle)));
        free(m);
        if (hit) return (int)i;
    }
    return -1;
}

/* object lifetime announcements: Create (INFO / STATE_CREATION, ExtRef=1 IntRef=0) and Destroy */
static const struct { int kind; const char *iface; D3D11_MESSAGE_ID create, destroy; } k_life[] = {
    { K_BUF, "ID3D11Buffer", 2097228, 2097230 },          { K_T1D, "ID3D11Texture1D", 2097231, 2097233 },
    { K_T2D, "ID3D11Texture2D", 2097234, 2097236 },       { K_T3D, "ID3D11Texture3D", 2097237, 2097239 },
    { K_SRV, "ID3D11ShaderResourceView", 2097240, 2097242 }, { K_RTV, "ID3D11RenderTargetView", 2097243, 2097245 },
    { K_DSV, "ID3D11DepthStencilView", 2097246, 2097248 }, { K_UAV, "ID3D11UnorderedAccessView", 2097301, 2097303 },
    { K_SAMP, "ID3D11Sampler", 2097267, 2097269 },        { K_VS, "ID3D11VertexShader", 2097249, 2097251 },
    { K_PS, "ID3D11PixelShader", 2097261, 2097263 },      { K_CS, "ID3D11ComputeShader", 2097298, 2097300 },
    { K_IL, "ID3D11InputLayout", 2097264, 2097266 },      { K_QUERY, "ID3D11Query", 2097279, 2097281 },
    { K_PRED, "ID3D11Predicate", 2097282, 2097284 },      { K_CLINK, "ID3D11ClassLinkage", 2097293, 2097295 },
    { K_DEFCTX, "ID3D11Context", 2097225, 2097227 },      { K_CMDLIST, "ID3D11CommandList", 2097287, 2097289 },
};
TEST(d3d11_debug_refcount_debug_object_lifetime_messages)
{
    D2(f);
    for (size_t i = 0; i < sizeof k_life / sizeof k_life[0]; i++) {
        char tx[64];
        ID3D11DeviceChild *o = NULL;
        ID3D11InfoQueue_ClearStoredMessages(f.iq);
        HRESULT hr = rc_make(k_life[i].kind, f.dev, &o);
        if (FAILED(hr) || !o) { CHECKF(0, "%s: create %s", k_life[i].iface, d3dh_hr_name(hr)); continue; }
        sprintf(tx, "Create %s: Name=\"unnamed\"", k_life[i].iface);
        int ix = d2_find(f.iq, k_life[i].create, tx);
        CHECKF(ix >= 0, "%s: no create message id %d", k_life[i].iface, (int)k_life[i].create);
        if (ix >= 0) {
            D3D11_MESSAGE *m = d2_msg(f.iq, ix);
            if (m) {
                EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_INFO); EXPECT_EQ(m->Category, D3D11_MESSAGE_CATEGORY_STATE_CREATION);
                CHECKF(strstr(m->pDescription, "ExtRef=1, IntRef=0") != NULL, "%s: %s", k_life[i].iface, m->pDescription);
                free(m);
            }
        }
        ID3D11InfoQueue_ClearStoredMessages(f.iq);
        EXPECT_EQ(RC_RELEASE(o), 0);
        sprintf(tx, "Destroy %s: Name=\"unnamed\"", k_life[i].iface);
        CHECKF(d2_find(f.iq, k_life[i].destroy, tx) >= 0, "%s: no destroy message id %d", k_life[i].iface, (int)k_life[i].destroy);
    }
    d2_close(&f);
}

TEST(d3d11_debug_refcount_debug_state_objects_are_silent)
{
    D2(f);
    static const int st[] = { K_BLEND, K_RAST, K_DSS };
    for (int i = 0; i < 3; i++) {
        ID3D11DeviceChild *o = NULL;
        ID3D11InfoQueue_ClearStoredMessages(f.iq);
        EXPECT_HR(rc_make(st[i], f.dev, &o), S_OK);
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(f.iq), 0);       /* Win10: default-state objects come from the device cache, no create message */
        if (o) EXPECT_EQ(RC_RELEASE(o), 0);
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(f.iq), 0);
    }
    d2_close(&f);
}

TEST(d3d11_debug_refcount_debug_destroy_message_carries_debug_name)
{
    D2(f);
    ID3D11DeviceChild *o = NULL;
    EXPECT_HR(rc_make(K_BUF, f.dev, &o), S_OK);
    if (o) {
        EXPECT_HR(o->lpVtbl->SetPrivateData(o, &WKPDID_D3DDebugObjectName, 5, "mybuf"), S_OK);
        ID3D11InfoQueue_ClearStoredMessages(f.iq);
        RC_RELEASE(o);
        CHECK(d2_find(f.iq, 2097230, "Destroy ID3D11Buffer: Name=\"mybuf\"") >= 0);
    }
    d2_close(&f);
}

TEST(d3d11_debug_refcount_debug_report_live_message_contents)
{
    D2(f);
    ID3D11DeviceChild *o = NULL;
    EXPECT_HR(rc_make(K_BUF, f.dev, &o), S_OK);
    if (o) EXPECT_HR(o->lpVtbl->SetPrivateData(o, &WKPDID_D3DDebugObjectName, 6, "livebf"), S_OK);
    ID3D11InfoQueue_ClearStoredMessages(f.iq);
    EXPECT_HR(ID3D11Debug_ReportLiveDeviceObjects(f.dbg, D3D11_RLDO_SUMMARY), S_OK);
    {   /* Win10: the report opens with the device line (WARNING / STATE_CREATION, id 441); the first report on a device is preceded by a hint (id 422) */
        int ix = d2_find(f.iq, 441, "Live ID3D11Device at 0x");
        CHECK(ix == 0 || ix == 1);
        if (ix == 1) { D3D11_MESSAGE *h = d2_msg(f.iq, 0); CHECK(h && h->ID == 422 && strstr(h->pDescription, "Using ID3D11Debug::ReportLiveDeviceObjects with D3D11_RLDO_DETAIL")); free(h); }
        D3D11_MESSAGE *m = ix >= 0 ? d2_msg(f.iq, ix) : NULL;
        CHECK(m != NULL);
        if (m) { EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_WARNING); EXPECT_EQ(m->Category, D3D11_MESSAGE_CATEGORY_STATE_CREATION); free(m); }
    }
    CHECK(d2_find(f.iq, 422, "ID3D11Buffer :      1") >= 0);              /* per-type tally */
    CHECK(d2_find(f.iq, 423, NULL) < 0);                                 /* SUMMARY alone lists no individual objects */
    ID3D11InfoQueue_ClearStoredMessages(f.iq);
    EXPECT_HR(ID3D11Debug_ReportLiveDeviceObjects(f.dbg, D3D11_RLDO_DETAIL), S_OK);
    CHECK(d2_find(f.iq, 423, "ID3D11Buffer at 0x") >= 0);
    CHECK(d2_find(f.iq, 423, "Name: livebf, Refcount: 1, IntRef: 0") >= 0);
    CHECK(d2_find(f.iq, 422, NULL) < 0);                                 /* DETAIL alone lists no tally */
    ID3D11InfoQueue_ClearStoredMessages(f.iq);
    EXPECT_HR(ID3D11Debug_ReportLiveDeviceObjects(f.dbg, D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL), S_OK);
    CHECK(d2_find(f.iq, 422, "ID3D11Buffer :      1") >= 0);
    CHECK(d2_find(f.iq, 423, "Name: livebf") >= 0);
    if (o) RC_RELEASE(o);
    ID3D11InfoQueue_ClearStoredMessages(f.iq);
    EXPECT_HR(ID3D11Debug_ReportLiveDeviceObjects(f.dbg, D3D11_RLDO_DETAIL), S_OK);
    CHECK(d2_find(f.iq, 423, "Name: livebf") < 0);                       /* released objects are no longer reported */
    d2_close(&f);
}

static void d2_check_draw_messages(d2_fix *f, ID3D11DeviceContext *c, const char *who)
{
    static const struct { D3D11_MESSAGE_ID id; D3D11_MESSAGE_SEVERITY sev; const char *text; } want[4] = {
        { 365, D3D11_MESSAGE_SEVERITY_ERROR, "Current Primitive Topology value (0) is not valid." },
        { 341, D3D11_MESSAGE_SEVERITY_ERROR, "A Vertex Shader is always required when drawing, but none is currently bound." },
        { 3146080, D3D11_MESSAGE_SEVERITY_INFO, "The depth stencil unit or pixel shader expects a Depth Stencil View, but none is bound." },
        { 362, D3D11_MESSAGE_SEVERITY_ERROR, "Rasterization Unit is enabled" },
    };
    ID3D11InfoQueue_ClearStoredMessages(f->iq);
    ID3D11DeviceContext_Draw(c, 3, 0);
    CHECKF(ID3D11InfoQueue_GetNumStoredMessages(f->iq) == 4, "%s: %llu messages", who, (unsigned long long)ID3D11InfoQueue_GetNumStoredMessages(f->iq));
    for (int i = 0; i < 4; i++) {
        D3D11_MESSAGE *m = d2_msg(f->iq, i);
        CHECKF(m != NULL, "%s message %d", who, i);
        if (!m) continue;
        CHECKF(m->ID == want[i].id && m->Severity == want[i].sev && m->Category == D3D11_MESSAGE_CATEGORY_EXECUTION, "%s message %d: id %d sev %d cat %d", who, i, (int)m->ID, (int)m->Severity, (int)m->Category);
        CHECKF(m->pDescription && strstr(m->pDescription, "ID3D11DeviceContext::Draw: ") && strstr(m->pDescription, want[i].text), "%s message %d: %s", who, i, m->pDescription ? m->pDescription : "");
        free(m);
    }
}
TEST(d3d11_debug_refcount_debug_draw_on_empty_pipeline_messages)
{
    D2(f);
    d2_check_draw_messages(&f, f.ctx, "immediate");
    ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(f.dev, 0, &dc), S_OK);
    if (dc) { d2_check_draw_messages(&f, dc, "deferred"); SAFE_RELEASE(dc); }   /* the deferred context validates the same way, at record time */
    d2_close(&f);
}

TEST(d3d11_debug_refcount_debug_dispatch_without_shader_is_silent)
{
    D2(f);
    ID3D11DeviceContext_Dispatch(f.ctx, 1, 1, 1);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(f.iq), 0);
    d2_close(&f);
}

/* Win10 22H2 with the debug layer: Unmap(NULL) does not fault, it ends the process (exit code 2173 = 0x87d), so it runs in a child. */
CHILD(dbg_unmap_null)
{
    d2_fix f;
    if (!d2_open(&f)) return 77;
    ID3D11DeviceContext_Unmap(f.ctx, NULL, 0);
    return 0;
}
TEST(d3d11_debug_refcount_debug_unmap_null_resource_ends_process)
{
    D2(f);
    d2_close(&f);
    DWORD rc = t_run_child("dbg_unmap_null", "", 60000);
    EXPECT_EQ(rc, 2173);
}

TEST(d3d11_debug_refcount_debug_deferred_context_has_no_debug_interfaces)
{
    D2(f);
    ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(f.dev, 0, &dc), S_OK);
    if (dc) {
        void *p = (void *)(ULONG_PTR)1;
        EXPECT_HR(RC_QI(dc, IID_ID3D11Debug, &p), E_NOINTERFACE); CHECK(p == NULL);
        p = (void *)(ULONG_PTR)1;
        EXPECT_HR(RC_QI(dc, IID_ID3D11InfoQueue, &p), E_NOINTERFACE); CHECK(p == NULL);
        SAFE_RELEASE(dc);
    }
    d2_close(&f);
}

typedef struct { ID3D11InfoQueue *iq; volatile LONG ok, fail; } msgthr;
static DWORD WINAPI msg_worker(LPVOID p)
{
    msgthr *t = p;
    for (int i = 0; i < 200; i++) {
        if (ID3D11InfoQueue_AddApplicationMessage(t->iq, D3D11_MESSAGE_SEVERITY_MESSAGE, "thread") == S_OK) InterlockedIncrement(&t->ok); else InterlockedIncrement(&t->fail);
    }
    return 0;
}
TEST(d3d11_debug_refcount_infoqueue_concurrent_add_is_atomic)
{
    D2(f);
    msgthr t = { f.iq, 0, 0 };
    HANDLE h[4];
    for (int i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, msg_worker, &t, 0, NULL);
    WaitForMultipleObjects(4, h, TRUE, 30000);
    for (int i = 0; i < 4; i++) if (h[i]) CloseHandle(h[i]);
    EXPECT_EQ(t.ok, 800); EXPECT_EQ(t.fail, 0);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(f.iq), 800);          /* below the 1024 default limit: nothing lost or duplicated */
    EXPECT_EQ(ID3D11InfoQueue_GetNumMessagesDiscardedByMessageCountLimit(f.iq), 0);
    d2_close(&f);
}

TEST(d3d11_debug_refcount_debug_creation_flags_are_preserved)
{
    REQUIRE(d3dh_D3D11CreateDevice());
    static const UINT extra[] = { 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT, D3D11_CREATE_DEVICE_SINGLETHREADED, D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED };
    for (size_t i = 0; i < sizeof extra / sizeof *extra; i++) {
        ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL;
        UINT fl = D3D11_CREATE_DEVICE_DEBUG | extra[i];
        HRESULT hr = d3dh_make_device11(&dev, &ctx, NULL, NULL, fl);
        if (FAILED(hr)) { t_skip("debug device unavailable (%s)", d3dh_hr_name(hr)); return; }
        EXPECT_EQ(ID3D11Device_GetCreationFlags(dev), fl);
        SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    }
}
