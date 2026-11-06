/* SDK-layer interfaces: ID3D11Debug, ID3D11InfoQueue, ID3D11SwitchToRef. They exist only when the Graphics Tools
 * feature is installed; every test skips (never fails) when the debug device cannot be created. */
#include "rc.h"

typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; ID3D11Debug *dbg; ID3D11InfoQueue *iq; } dbg_fix;

static int dbg_open(dbg_fix *f, int want_iq)
{
    memset(f, 0, sizeof(*f));
    if (!d3dh_D3D11CreateDevice()) { t_skip("missing export: D3D11CreateDevice"); return 0; }
    HRESULT hr = d3dh_make_device11(&f->dev, &f->ctx, NULL, NULL, D3D11_CREATE_DEVICE_DEBUG);
    if (FAILED(hr)) { t_skip("debug device unavailable (%s): SDK layers not installed?", d3dh_hr_name(hr)); return 0; }
    hr = RC_QI(f->dev, IID_ID3D11Debug, &f->dbg);
    if (FAILED(hr)) { t_skip("debug device without ID3D11Debug (%s)", d3dh_hr_name(hr)); SAFE_RELEASE(f->ctx); SAFE_RELEASE(f->dev); return 0; }
    if (want_iq) {
        hr = RC_QI(f->dev, IID_ID3D11InfoQueue, &f->iq);
        if (FAILED(hr)) { t_skip("debug device without ID3D11InfoQueue (%s)", d3dh_hr_name(hr)); SAFE_RELEASE(f->dbg); SAFE_RELEASE(f->ctx); SAFE_RELEASE(f->dev); return 0; }
        ID3D11InfoQueue_SetMuteDebugOutput(f->iq, TRUE);
        /* Win10: the default storage filter (stack size 1) denies INFO severity, so AddApplicationMessage(INFO) returns S_FALSE and
           stores nothing. Mode 2 clears it so mechanics tests can use any severity; mode 1 keeps the default filter. */
        if (want_iq == 2) ID3D11InfoQueue_ClearStorageFilter(f->iq);
        ID3D11InfoQueue_ClearStoredMessages(f->iq);
    }
    return 1;
}
static void dbg_close(dbg_fix *f)
{
    if (f->iq) ID3D11InfoQueue_SetMuteDebugOutput(f->iq, FALSE);
    SAFE_RELEASE(f->iq); SAFE_RELEASE(f->dbg); SAFE_RELEASE(f->ctx); SAFE_RELEASE(f->dev);
}
#define DBG(f, iq) dbg_fix f; if (!dbg_open(&f, iq)) return
#define DBGQ(f) dbg_fix f; if (!dbg_open(&f, 2)) return
#define IQ (f.iq)

/* ---- device creation with the debug flag --------------------------------------------- */
TEST(d3d11_debug_refcount_create_device_debug_flag_result)
{
    REQUIRE(d3dh_D3D11CreateDevice());
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL;
    HRESULT hr = d3dh_D3D11CreateDevice()(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, 0, D3D11_SDK_VERSION, &dev, NULL, &ctx);
    if (SUCCEEDED(hr)) {
        t_note("SDK layers present: debug device created (%s)", d3dh_hr_name(hr));
        EXPECT_EQ(ID3D11Device_GetCreationFlags(dev) & D3D11_CREATE_DEVICE_DEBUG, D3D11_CREATE_DEVICE_DEBUG);
        SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    } else {
        t_note("debug device creation failed: %s", d3dh_hr_name(hr));
        if (!t_is_wine()) EXPECT_HR(hr, HR_SDK_COMPONENT_MISSING);
        CHECK(dev == NULL);
        CHECK(ctx == NULL);
    }
}

TEST(d3d11_debug_refcount_create_device_debug_flag_failure_nulls_outputs)
{
    REQUIRE(d3dh_D3D11CreateDevice());
    ID3D11Device *dev = (ID3D11Device *)(ULONG_PTR)1; ID3D11DeviceContext *ctx = (ID3D11DeviceContext *)(ULONG_PTR)1;
    D3D_FEATURE_LEVEL fl = (D3D_FEATURE_LEVEL)0x1234;
    HRESULT hr = d3dh_D3D11CreateDevice()(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    if (SUCCEEDED(hr)) { SAFE_RELEASE(ctx); SAFE_RELEASE(dev); t_skip("SDK layers are installed"); return; }
    CHECK(dev == NULL);
    CHECK(ctx == NULL);
    t_note("failed debug creation leaves feature level 0x%x", (unsigned)fl);
}

TEST(d3d11_debug_refcount_create_device_without_flag_has_no_debug_interfaces)
{
    RC_DEVICE(dev, ctx, 0);
    EXPECT_EQ(ID3D11Device_GetCreationFlags(dev) & D3D11_CREATE_DEVICE_DEBUG, 0);
    void *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(RC_QI(dev, IID_ID3D11Debug, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(RC_QI(dev, IID_ID3D11InfoQueue, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(RC_QI(dev, IID_ID3D11SwitchToRef, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    RC_DEVICE_END(dev, ctx);
}

TEST(d3d11_debug_refcount_debug_device_interfaces_present)
{
    DBG(f, 1);
    ID3D11SwitchToRef *sw = NULL;
    HRESULT hr = RC_QI(f.dev, IID_ID3D11SwitchToRef, &sw);
    /* Win10 22H2, WARP + Graphics Tools: the debug device does not expose ID3D11SwitchToRef (needs the reference rasteriser). */
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(sw == NULL);
    if (SUCCEEDED(hr)) SAFE_RELEASE(sw);
    EXPECT_EQ(ID3D11Device_GetCreationFlags(f.dev) & D3D11_CREATE_DEVICE_DEBUG, D3D11_CREATE_DEVICE_DEBUG);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_interface_refcount)
{
    DBG(f, 1);
    ULONG base = d3dh_refcount(f.dev);
    ID3D11Debug *d2 = NULL; ID3D11InfoQueue *q2 = NULL;
    EXPECT_HR(RC_QI(f.dev, IID_ID3D11Debug, &d2), S_OK);
    EXPECT_EQ(d3dh_refcount(f.dev), base + 1);
    EXPECT_HR(RC_QI(f.dev, IID_ID3D11InfoQueue, &q2), S_OK);
    EXPECT_EQ(d3dh_refcount(f.dev), base + 2);
    CHECK(d2 == f.dbg);   /* the interfaces are stable per device */
    CHECK(q2 == f.iq);
    SAFE_RELEASE(d2); SAFE_RELEASE(q2);
    EXPECT_EQ(d3dh_refcount(f.dev), base);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_iunknown_identity)
{
    DBG(f, 1);
    IUnknown *u0 = NULL, *u1 = NULL, *u2 = NULL;
    EXPECT_HR(RC_QI(f.dev, IID_IUnknown, &u0), S_OK);
    EXPECT_HR(RC_QI(f.dbg, IID_IUnknown, &u1), S_OK);
    EXPECT_HR(RC_QI(f.iq, IID_IUnknown, &u2), S_OK);
    CHECK(u0 == u1);
    CHECK(u0 == u2);
    SAFE_RELEASE(u0); SAFE_RELEASE(u1); SAFE_RELEASE(u2);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_qi_unsupported_iid)
{
    DBG(f, 1);
    void *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(RC_QI(f.dbg, IID_ID3D11Buffer, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(RC_QI(f.iq, IID_ID3D11Texture2D, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(RC_QI(f.iq, GUID_NULL, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_interface_addref_release_values)
{
    DBG(f, 1);
    ULONG a = RC_ADDREF(f.dbg);
    ULONG b = RC_ADDREF(f.dbg);
    EXPECT_EQ(b, a + 1);
    EXPECT_EQ(RC_RELEASE(f.dbg), a);
    EXPECT_EQ(RC_RELEASE(f.dbg), a - 1);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_children_hold_device_with_layer)
{
    DBG(f, 0);
    ID3D11DeviceChild *o = NULL;
    HRESULT hr = rc_make(K_BUF, f.dev, &o);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(f.dbg);
    SAFE_RELEASE(f.ctx);
    SAFE_RELEASE(f.dev);
    if (o) {
        ID3D11Device *d = NULL;
        o->lpVtbl->GetDevice(o, &d);
        CHECK(d != NULL);
        ID3D11Debug *dbg = NULL;
        if (d) EXPECT_HR(RC_QI(d, IID_ID3D11Debug, &dbg), S_OK);   /* the debug layer lives as long as the device */
        SAFE_RELEASE(dbg);
        SAFE_RELEASE(d);
        EXPECT_EQ(RC_RELEASE(o), 0);
    }
}

/* ---- ID3D11Debug -------------------------------------------------------------------------- */
TEST(d3d11_debug_refcount_debug_feature_mask_roundtrip)
{
    DBG(f, 0);
    UINT orig = ID3D11Debug_GetFeatureMask(f.dbg);
    EXPECT_EQ(orig, 0);
    static const UINT masks[] = { 0, D3D11_DEBUG_FEATURE_FLUSH_PER_RENDER_OP, D3D11_DEBUG_FEATURE_FINISH_PER_RENDER_OP,
        D3D11_DEBUG_FEATURE_FLUSH_PER_RENDER_OP | D3D11_DEBUG_FEATURE_FINISH_PER_RENDER_OP };
    for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); i++) {
        EXPECT_HR(ID3D11Debug_SetFeatureMask(f.dbg, masks[i]), S_OK);
        EXPECT_EQ(ID3D11Debug_GetFeatureMask(f.dbg), masks[i]);
    }
    EXPECT_HR(ID3D11Debug_SetFeatureMask(f.dbg, 0), S_OK);
    EXPECT_EQ(ID3D11Debug_GetFeatureMask(f.dbg), 0);
    ID3D11Debug_SetFeatureMask(f.dbg, orig);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_feature_mask_other_bits_note)
{
    DBG(f, 0);
    UINT orig = ID3D11Debug_GetFeatureMask(f.dbg);
    static const UINT bits[] = { D3D11_DEBUG_FEATURE_ALWAYS_DISCARD_OFFERED_RESOURCE, D3D11_DEBUG_FEATURE_NEVER_DISCARD_OFFERED_RESOURCE,
        D3D11_DEBUG_FEATURE_AVOID_BEHAVIOR_CHANGING_DEBUG_AIDS, D3D11_DEBUG_FEATURE_DISABLE_TILED_RESOURCE_MAPPING_TRACKING_AND_VALIDATION, 0x20, 0x100, 0x80000000 };
    for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); i++) {
        /* Win10: the defined bits (0x8, 0x10, 0x40, 0x80) are accepted; undefined bits are E_INVALIDARG and leave the mask unchanged */
        UINT before = ID3D11Debug_GetFeatureMask(f.dbg);
        HRESULT hr = ID3D11Debug_SetFeatureMask(f.dbg, bits[i]);
        if (i < 4) { EXPECT_HR(hr, S_OK); EXPECT_EQ(ID3D11Debug_GetFeatureMask(f.dbg), bits[i]); }
        else { EXPECT_HR(hr, E_INVALIDARG); EXPECT_EQ(ID3D11Debug_GetFeatureMask(f.dbg), before); }
    }
    ID3D11Debug_SetFeatureMask(f.dbg, orig);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_present_delay_note)
{
    DBG(f, 0);
    UINT orig = ID3D11Debug_GetPresentPerRenderOpDelay(f.dbg);
    HRESULT hr = ID3D11Debug_SetPresentPerRenderOpDelay(f.dbg, 0);
    EXPECT_EQ(orig, 10);                                   /* Win10 default */
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(ID3D11Debug_GetPresentPerRenderOpDelay(f.dbg), 0);
    EXPECT_HR(ID3D11Debug_SetPresentPerRenderOpDelay(f.dbg, 250), S_OK);
    EXPECT_EQ(ID3D11Debug_GetPresentPerRenderOpDelay(f.dbg), 250);
    ID3D11Debug_SetPresentPerRenderOpDelay(f.dbg, orig);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_swapchain_null)
{
    DBG(f, 0);
    EXPECT_HR(ID3D11Debug_SetSwapChain(f.dbg, NULL), S_OK);
    IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)1;
    HRESULT hr = ID3D11Debug_GetSwapChain(f.dbg, &sc);
    EXPECT_HR(hr, S_OK);
    CHECK(sc == NULL);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_swapchain_roundtrip_refcount)
{
    DBG(f, 0);
    IDXGIFactory1 *fac = NULL;
    if (FAILED(d3dh_make_factory1(&fac))) { dbg_close(&f); t_skip("no DXGI factory"); return; }
    HWND hwnd = d3dh_create_hidden_window();
    DXGI_SWAP_CHAIN_DESC sd; memset(&sd, 0, sizeof(sd));
    sd.BufferDesc.Width = 64; sd.BufferDesc.Height = 64; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 1; sd.OutputWindow = hwnd; sd.Windowed = TRUE;
    IDXGISwapChain *sc = NULL;
    HRESULT hr = IDXGIFactory1_CreateSwapChain(fac, (IUnknown *)f.dev, &sd, &sc);
    if (FAILED(hr) || !sc) {
        t_note("CreateSwapChain failed: %s", d3dh_hr_name(hr));
    } else {
        ULONG base = d3dh_refcount(sc);
        /* Win10 22H2: SetSwapChain is not implemented by the debug layer (E_NOTIMPL) and does not retain the swap chain */
        EXPECT_HR(ID3D11Debug_SetSwapChain(f.dbg, sc), E_NOTIMPL);
        EXPECT_EQ(d3dh_refcount(sc), base);
        IDXGISwapChain *got = (IDXGISwapChain *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11Debug_GetSwapChain(f.dbg, &got), S_OK);
        CHECK(got == NULL);
        SAFE_RELEASE(got);
        EXPECT_HR(ID3D11Debug_SetSwapChain(f.dbg, NULL), S_OK);
        EXPECT_EQ(d3dh_refcount(sc), base);
        IDXGISwapChain_Release(sc);
    }
    IDXGIFactory1_Release(fac);
    d3dh_destroy_window(hwnd);
    dbg_close(&f);
}

static void report_live(UINT flags, const char *name)
{
    DBG(f, 1);
    ID3D11DeviceChild *o = NULL;
    rc_make(K_BUF, f.dev, &o);
    HRESULT hr = ID3D11Debug_ReportLiveDeviceObjects(f.dbg, (D3D11_RLDO_FLAGS)flags);
    EXPECT_HR(hr, S_OK);
    t_note("ReportLiveDeviceObjects(%s) -> %s, stored messages %llu", name, d3dh_hr_name(hr), (unsigned long long)ID3D11InfoQueue_GetNumStoredMessages(f.iq));
    if (o) EXPECT_EQ(d3dh_refcount(o), 1);   /* reporting does not touch the objects */
    SAFE_RELEASE(o);
    dbg_close(&f);
}
TEST(d3d11_debug_refcount_debug_report_live_summary) { report_live(D3D11_RLDO_SUMMARY, "SUMMARY"); }
TEST(d3d11_debug_refcount_debug_report_live_detail) { report_live(D3D11_RLDO_DETAIL, "DETAIL"); }
TEST(d3d11_debug_refcount_debug_report_live_summary_detail) { report_live(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL, "SUMMARY|DETAIL"); }
TEST(d3d11_debug_refcount_debug_report_live_ignore_internal) { report_live(4 | D3D11_RLDO_SUMMARY, "IGNORE_INTERNAL|SUMMARY"); }
TEST(d3d11_debug_refcount_debug_report_live_detail_ignore_internal) { report_live(4 | D3D11_RLDO_DETAIL, "IGNORE_INTERNAL|DETAIL"); }

TEST(d3d11_debug_refcount_debug_report_live_zero_flags_note)
{
    DBG(f, 1);
    HRESULT hr = ID3D11Debug_ReportLiveDeviceObjects(f.dbg, (D3D11_RLDO_FLAGS)0);
    EXPECT_HR(hr, S_OK);
    hr = ID3D11Debug_ReportLiveDeviceObjects(f.dbg, (D3D11_RLDO_FLAGS)0x80);
    EXPECT_HR(hr, E_INVALIDARG);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_report_live_after_objects_released)
{
    DBG(f, 1);
    for (int i = 0; i < 10; i++) {
        ID3D11DeviceChild *o = NULL;
        if (SUCCEEDED(rc_make(i % 2 ? K_T2D : K_BUF, f.dev, &o))) EXPECT_EQ(RC_RELEASE(o), 0);
    }
    EXPECT_HR(ID3D11Debug_ReportLiveDeviceObjects(f.dbg, D3D11_RLDO_DETAIL | D3D11_RLDO_SUMMARY), S_OK);
    dbg_close(&f);
}

/* Win10 22H2 (20/20 fresh devices): an empty pipeline fails ValidateContext (no shaders) but passes the dispatch validation */
TEST(d3d11_debug_refcount_debug_validate_context_empty_pipeline)
{
    DBG(f, 1);
    EXPECT_HR(ID3D11Debug_ValidateContext(f.dbg, f.ctx), E_FAIL);
    EXPECT_HR(ID3D11Debug_ValidateContextForDispatch(f.dbg, f.ctx), S_OK);
    ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(f.dev, 0, &dc), S_OK);
    if (dc) {
        EXPECT_HR(ID3D11Debug_ValidateContext(f.dbg, dc), E_FAIL);
        EXPECT_HR(ID3D11Debug_ValidateContextForDispatch(f.dbg, dc), S_OK);
        SAFE_RELEASE(dc);
    }
    ID3D11InfoQueue_ClearStoredMessages(f.iq);
    dbg_close(&f);
}

typedef struct { ID3D11Debug *dbg; ID3D11DeviceContext *ctx; HRESULT hr; } vc_args;
static void vc_call(void *p) { vc_args *a = p; a->hr = ID3D11Debug_ValidateContext(a->dbg, a->ctx); }
/* Win10 22H2: no argument validation: a context of another device and a NULL context raise an access violation (20/20) */
TEST(d3d11_debug_refcount_debug_validate_context_foreign_device_faults)
{
    DBG(f, 1);
    ID3D11Device *d2 = NULL; ID3D11DeviceContext *c2 = NULL;
    if (FAILED(d3dh_make_device11(&d2, &c2, NULL, NULL, 0))) { dbg_close(&f); t_skip("second device unavailable"); return; }
    vc_args a = { f.dbg, c2, S_OK };
    EXPECT_EQ(t_try(vc_call, &a), 0xC0000005u);
    a.ctx = NULL;
    EXPECT_EQ(t_try(vc_call, &a), 0xC0000005u);
    SAFE_RELEASE(c2); SAFE_RELEASE(d2);
    ID3D11InfoQueue_ClearStoredMessages(f.iq);
    dbg_close(&f);
}

/* ---- ID3D11SwitchToRef ------------------------------------------------------------------------- */
TEST(d3d11_debug_refcount_switch_to_ref_set_false)
{
    DBG(f, 0);
    ID3D11SwitchToRef *sw = NULL;
    HRESULT hr = RC_QI(f.dev, IID_ID3D11SwitchToRef, &sw);
    if (FAILED(hr)) { dbg_close(&f); t_skip("ID3D11SwitchToRef not exposed (%s); Win10 22H2 WARP debug device: E_NOINTERFACE", d3dh_hr_name(hr)); return; }
    BOOL cur = ID3D11SwitchToRef_GetUseRef(sw);
    t_note("initial GetUseRef = %d", (int)cur);
    BOOL prev = ID3D11SwitchToRef_SetUseRef(sw, FALSE);
    t_note("SetUseRef(FALSE) returned previous %d", (int)prev);
    EXPECT_EQ(ID3D11SwitchToRef_GetUseRef(sw), FALSE);
    prev = ID3D11SwitchToRef_SetUseRef(sw, FALSE);
    EXPECT_EQ(prev, FALSE);
    SAFE_RELEASE(sw);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_switch_to_ref_toggle_note)
{
    DBG(f, 0);
    ID3D11SwitchToRef *sw = NULL;
    HRESULT hr = RC_QI(f.dev, IID_ID3D11SwitchToRef, &sw);
    if (FAILED(hr)) { dbg_close(&f); t_skip("ID3D11SwitchToRef not exposed (%s); Win10 22H2 WARP debug device: E_NOINTERFACE", d3dh_hr_name(hr)); return; }
    BOOL p1 = ID3D11SwitchToRef_SetUseRef(sw, TRUE);
    BOOL now = ID3D11SwitchToRef_GetUseRef(sw);
    t_note("SetUseRef(TRUE): previous %d, now %d (the reference rasteriser may be absent)", (int)p1, (int)now);
    BOOL p2 = ID3D11SwitchToRef_SetUseRef(sw, FALSE);
    t_note("SetUseRef(FALSE): previous %d", (int)p2);
    EXPECT_EQ(ID3D11SwitchToRef_GetUseRef(sw), FALSE);
    SAFE_RELEASE(sw);
    dbg_close(&f);
}

/* ---- ID3D11InfoQueue ------------------------------------------------------------------------------ */
static D3D11_MESSAGE *get_msg(ID3D11InfoQueue *iq, UINT64 idx, SIZE_T *len_out)
{
    SIZE_T len = 0;
    HRESULT hr = ID3D11InfoQueue_GetMessage(iq, idx, NULL, &len);
    if (FAILED(hr) || !len) return NULL;
    D3D11_MESSAGE *m = malloc(len);
    hr = ID3D11InfoQueue_GetMessage(iq, idx, m, &len);
    if (hr != S_OK) { free(m); return NULL; }
    if (len_out) *len_out = len;
    return m;
}

TEST(d3d11_debug_refcount_infoqueue_initial_state)
{
    DBG(f, 1);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessagesAllowedByRetrievalFilter(IQ), 0);
    EXPECT_EQ(ID3D11InfoQueue_GetMessageCountLimit(IQ), 1024);          /* Win10 default */
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), 1);        /* the default filter */
    EXPECT_EQ(ID3D11InfoQueue_GetRetrievalFilterStackSize(IQ), 0);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_mute_roundtrip)
{
    DBG(f, 1);
    ID3D11InfoQueue_SetMuteDebugOutput(IQ, FALSE);
    EXPECT_EQ(ID3D11InfoQueue_GetMuteDebugOutput(IQ), FALSE);
    ID3D11InfoQueue_SetMuteDebugOutput(IQ, TRUE);
    EXPECT_EQ(ID3D11InfoQueue_GetMuteDebugOutput(IQ), TRUE);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_application_message_stored)
{
    DBGQ(f);
    static const char text[] = "hello from the refcount test";
    EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, text), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    SIZE_T len = 0;
    D3D11_MESSAGE *m = get_msg(IQ, 0, &len);
    CHECK(m != NULL);
    if (m) {
        EXPECT_EQ(m->Category, D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED);
        EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_INFO);
        EXPECT_EQ(m->ID, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION);
        EXPECT_EQ(m->DescriptionByteLength, strlen(text) + 1);                 /* Win10: includes the terminating NUL */
        CHECK(m->pDescription && !strcmp(m->pDescription, text));
        EXPECT_EQ(len, sizeof(D3D11_MESSAGE) + strlen(text) + 1);              /* header + description, nothing else */
        free(m);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_add_message_fields_roundtrip)
{
    DBGQ(f);
    static const struct { D3D11_MESSAGE_CATEGORY c; D3D11_MESSAGE_SEVERITY s; D3D11_MESSAGE_ID id; } t[] = {
        { D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED, D3D11_MESSAGE_SEVERITY_MESSAGE, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION },
        { D3D11_MESSAGE_CATEGORY_MISCELLANEOUS, D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION },
        { D3D11_MESSAGE_CATEGORY_INITIALIZATION, D3D11_MESSAGE_SEVERITY_WARNING, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION },
        { D3D11_MESSAGE_CATEGORY_EXECUTION, D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_ID_DEVICE_DRAW_VERTEX_BUFFER_NOT_SET },
        { D3D11_MESSAGE_CATEGORY_SHADER, D3D11_MESSAGE_SEVERITY_MESSAGE, D3D11_MESSAGE_ID_UNKNOWN },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        char desc[32]; sprintf(desc, "entry %u", (unsigned)i);
        EXPECT_HR(ID3D11InfoQueue_AddMessage(IQ, t[i].c, t[i].s, t[i].id, desc), S_OK);   /* no filter: every severity/category/id is stored */
    }
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), sizeof(t) / sizeof(t[0]));
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        D3D11_MESSAGE *m = get_msg(IQ, i, NULL);
        CHECKF(m != NULL, "message %u", (unsigned)i);
        if (!m) continue;
        char desc[32]; sprintf(desc, "entry %u", (unsigned)i);
        EXPECT_EQ(m->Category, t[i].c); EXPECT_EQ(m->Severity, t[i].s); EXPECT_EQ(m->ID, t[i].id);
        CHECK(m->pDescription && !strcmp(m->pDescription, desc));
        free(m);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_messages_in_insertion_order)
{
    DBGQ(f);
    char d[32];
    for (int i = 0; i < 5; i++) { sprintf(d, "msg-%d", i); EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_MESSAGE, d), S_OK); }
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 5);
    for (int i = 0; i < 5; i++) {
        D3D11_MESSAGE *m = get_msg(IQ, i, NULL);
        CHECK(m != NULL);
        if (!m) continue;
        sprintf(d, "msg-%d", i);
        CHECKF(m->pDescription && !strcmp(m->pDescription, d), "message %d is \"%s\"", i, m->pDescription ? m->pDescription : "(null)");
        free(m);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_get_message_size_protocol)
{
    DBGQ(f);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "size protocol");
    SIZE_T len = 0;
    HRESULT hr = ID3D11InfoQueue_GetMessage(IQ, 0, NULL, &len);
    EXPECT_HR(hr, S_FALSE);                                   /* Win10: size query with a NULL buffer returns S_FALSE */
    EXPECT_EQ(len, sizeof(D3D11_MESSAGE) + sizeof("size protocol"));
    /* a buffer that is one byte too small must not be accepted */
    D3D11_MESSAGE *m = malloc(len + 16);
    memset(m, 0xcc, len + 16);
    SIZE_T small = len - 1;
    hr = ID3D11InfoQueue_GetMessage(IQ, 0, m, &small);
    EXPECT_HR(hr, E_INVALIDARG);
    EXPECT_EQ(small, len - 1);                                 /* length is left untouched on failure */
    small = 0;
    hr = ID3D11InfoQueue_GetMessage(IQ, 0, m, &small);
    EXPECT_HR(hr, E_INVALIDARG);
    EXPECT_EQ(small, 0);
    /* exact size and larger size succeed and report the real length */
    SIZE_T exact = len;
    EXPECT_HR(ID3D11InfoQueue_GetMessage(IQ, 0, m, &exact), S_OK);
    EXPECT_EQ(exact, len);
    SIZE_T big = len + 16;
    EXPECT_HR(ID3D11InfoQueue_GetMessage(IQ, 0, m, &big), S_OK);
    EXPECT_EQ(big, len + 16);                                  /* Win10: the length is an input only, it is not rewritten on success */
    free(m);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_get_message_out_of_range)
{
    DBGQ(f);
    SIZE_T len = 0;
    HRESULT hr = ID3D11InfoQueue_GetMessage(IQ, 0, NULL, &len);
    EXPECT_HR(hr, E_INVALIDARG);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "one");
    len = 0;
    hr = ID3D11InfoQueue_GetMessage(IQ, 1, NULL, &len);
    EXPECT_HR(hr, E_INVALIDARG);
    len = 0;
    hr = ID3D11InfoQueue_GetMessage(IQ, (UINT64)-1, NULL, &len);
    EXPECT_HR(hr, E_INVALIDARG);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_clear_stored_messages)
{
    DBGQ(f);
    for (int i = 0; i < 4; i++) ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "x");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 4);
    ID3D11InfoQueue_ClearStoredMessages(IQ);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessagesAllowedByRetrievalFilter(IQ), 0);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "y");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_message_count_limit)
{
    DBGQ(f);
    UINT64 orig = ID3D11InfoQueue_GetMessageCountLimit(IQ);
    EXPECT_HR(ID3D11InfoQueue_SetMessageCountLimit(IQ, 5), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetMessageCountLimit(IQ), 5);
    UINT64 disc0 = ID3D11InfoQueue_GetNumMessagesDiscardedByMessageCountLimit(IQ);
    for (int i = 0; i < 8; i++) ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "limit");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 5);
    EXPECT_EQ(ID3D11InfoQueue_GetNumMessagesDiscardedByMessageCountLimit(IQ) - disc0, 3);
    ID3D11InfoQueue_SetMessageCountLimit(IQ, orig);
    EXPECT_EQ(ID3D11InfoQueue_GetMessageCountLimit(IQ), orig);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_message_count_limit_unlimited)
{
    DBGQ(f);
    UINT64 orig = ID3D11InfoQueue_GetMessageCountLimit(IQ);
    HRESULT hr = ID3D11InfoQueue_SetMessageCountLimit(IQ, (UINT64)-1);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetMessageCountLimit(IQ), (UINT64)-1);
    for (int i = 0; i < 2000; i++) ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "u");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 2000);
    ID3D11InfoQueue_ClearStoredMessages(IQ);
    ID3D11InfoQueue_SetMessageCountLimit(IQ, orig);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_limit_smaller_than_stored)
{
    DBGQ(f);
    UINT64 orig = ID3D11InfoQueue_GetMessageCountLimit(IQ);
    for (int i = 0; i < 6; i++) ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "p");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 6);
    HRESULT hr = ID3D11InfoQueue_SetMessageCountLimit(IQ, 2);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 2);                       /* Win10: oldest messages are dropped to fit */
    EXPECT_EQ(ID3D11InfoQueue_GetNumMessagesDiscardedByMessageCountLimit(IQ), 4);
    ID3D11InfoQueue_SetMessageCountLimit(IQ, orig);
    dbg_close(&f);
}

static D3D11_INFO_QUEUE_FILTER make_deny_severity(D3D11_MESSAGE_SEVERITY *sev)
{
    D3D11_INFO_QUEUE_FILTER fl; memset(&fl, 0, sizeof(fl));
    fl.DenyList.NumSeverities = 1; fl.DenyList.pSeverityList = sev;
    return fl;
}

TEST(d3d11_debug_refcount_infoqueue_storage_filter_stack_depth)
{
    DBG(f, 1);
    UINT base = ID3D11InfoQueue_GetStorageFilterStackSize(IQ);
    EXPECT_HR(ID3D11InfoQueue_PushEmptyStorageFilter(IQ), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), base + 1);
    EXPECT_HR(ID3D11InfoQueue_PushCopyOfStorageFilter(IQ), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), base + 2);
    D3D11_MESSAGE_SEVERITY sev = D3D11_MESSAGE_SEVERITY_INFO;
    D3D11_INFO_QUEUE_FILTER fl = make_deny_severity(&sev);
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), base + 3);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), base + 2);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), base);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_retrieval_filter_stack_depth)
{
    DBG(f, 1);
    UINT base = ID3D11InfoQueue_GetRetrievalFilterStackSize(IQ);
    EXPECT_HR(ID3D11InfoQueue_PushEmptyRetrievalFilter(IQ), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetRetrievalFilterStackSize(IQ), base + 1);
    EXPECT_HR(ID3D11InfoQueue_PushCopyOfRetrievalFilter(IQ), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetRetrievalFilterStackSize(IQ), base + 2);
    D3D11_MESSAGE_SEVERITY sev = D3D11_MESSAGE_SEVERITY_INFO;
    D3D11_INFO_QUEUE_FILTER fl = make_deny_severity(&sev);
    EXPECT_HR(ID3D11InfoQueue_PushRetrievalFilter(IQ, &fl), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetRetrievalFilterStackSize(IQ), base + 3);
    ID3D11InfoQueue_PopRetrievalFilter(IQ);
    ID3D11InfoQueue_PopRetrievalFilter(IQ);
    ID3D11InfoQueue_PopRetrievalFilter(IQ);
    EXPECT_EQ(ID3D11InfoQueue_GetRetrievalFilterStackSize(IQ), base);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_storage_filter_denies_severity)
{
    DBGQ(f);
    D3D11_MESSAGE_SEVERITY sev = D3D11_MESSAGE_SEVERITY_INFO;
    D3D11_INFO_QUEUE_FILTER fl = make_deny_severity(&sev);
    UINT64 allowed0 = ID3D11InfoQueue_GetNumMessagesAllowedByStorageFilter(IQ);
    UINT64 denied0 = ID3D11InfoQueue_GetNumMessagesDeniedByStorageFilter(IQ);
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "denied");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    EXPECT_EQ(ID3D11InfoQueue_GetNumMessagesDeniedByStorageFilter(IQ) - denied0, 1);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_MESSAGE, "allowed");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    EXPECT_EQ(ID3D11InfoQueue_GetNumMessagesAllowedByStorageFilter(IQ) - allowed0, 1);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "allowed again");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 2);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_storage_filter_allow_list)
{
    DBGQ(f);
    D3D11_MESSAGE_SEVERITY sev = D3D11_MESSAGE_SEVERITY_WARNING;
    D3D11_INFO_QUEUE_FILTER fl; memset(&fl, 0, sizeof(fl));
    fl.AllowList.NumSeverities = 1; fl.AllowList.pSeverityList = &sev;
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "not on the list");
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_MESSAGE, "not on the list either");
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_WARNING, "on the list");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    D3D11_MESSAGE *m = get_msg(IQ, 0, NULL);
    if (m) { EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_WARNING); free(m); } else CHECK(m != NULL);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_storage_filter_deny_id)
{
    DBGQ(f);
    D3D11_MESSAGE_ID id = D3D11_MESSAGE_ID_STRING_FROM_APPLICATION;
    D3D11_INFO_QUEUE_FILTER fl; memset(&fl, 0, sizeof(fl));
    fl.DenyList.NumIDs = 1; fl.DenyList.pIDList = &id;
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_WARNING, "denied by id");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_WARNING, "stored");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_storage_filter_deny_category)
{
    DBGQ(f);
    D3D11_MESSAGE_CATEGORY cat = D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED;
    D3D11_INFO_QUEUE_FILTER fl; memset(&fl, 0, sizeof(fl));
    fl.DenyList.NumCategories = 1; fl.DenyList.pCategoryList = &cat;
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_WARNING, "denied by category");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_empty_storage_filter_allows_all)
{
    DBGQ(f);
    EXPECT_HR(ID3D11InfoQueue_PushEmptyStorageFilter(IQ), S_OK);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "i");
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_WARNING, "w");
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_MESSAGE, "m");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 3);
    CHECK(ID3D11InfoQueue_GetNumStoredMessages(IQ) == 3);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_get_storage_filter_size_protocol)
{
    DBGQ(f);
    D3D11_MESSAGE_SEVERITY sevs[2] = { D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_SEVERITY_MESSAGE };
    D3D11_INFO_QUEUE_FILTER fl; memset(&fl, 0, sizeof(fl));
    fl.DenyList.NumSeverities = 2; fl.DenyList.pSeverityList = sevs;
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    SIZE_T len = 0;
    HRESULT hr = ID3D11InfoQueue_GetStorageFilter(IQ, NULL, &len);
    EXPECT_HR(hr, S_FALSE);                                    /* size query */
    EXPECT_EQ(len, sizeof(D3D11_INFO_QUEUE_FILTER) + 2 * sizeof(D3D11_MESSAGE_SEVERITY));
    D3D11_INFO_QUEUE_FILTER *got = malloc(len);
    SIZE_T l2 = len;
    EXPECT_HR(ID3D11InfoQueue_GetStorageFilter(IQ, got, &l2), S_OK);
    EXPECT_EQ(l2, len);
    EXPECT_EQ(got->DenyList.NumSeverities, 2);
    if (got->DenyList.NumSeverities == 2 && got->DenyList.pSeverityList) {
        EXPECT_EQ(got->DenyList.pSeverityList[0], D3D11_MESSAGE_SEVERITY_INFO);
        EXPECT_EQ(got->DenyList.pSeverityList[1], D3D11_MESSAGE_SEVERITY_MESSAGE);
    }
    EXPECT_EQ(got->AllowList.NumSeverities, 0);
    free(got);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_add_storage_filter_entries)
{
    DBGQ(f);
    ID3D11InfoQueue_PushEmptyStorageFilter(IQ);
    D3D11_MESSAGE_SEVERITY sev = D3D11_MESSAGE_SEVERITY_INFO;
    D3D11_INFO_QUEUE_FILTER fl = make_deny_severity(&sev);
    UINT depth = ID3D11InfoQueue_GetStorageFilterStackSize(IQ);
    EXPECT_HR(ID3D11InfoQueue_AddStorageFilterEntries(IQ, &fl), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), depth);   /* entries are merged into the top filter */
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "denied");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    ID3D11InfoQueue_ClearStorageFilter(IQ);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "allowed");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_pushed_filter_is_copied)
{
    DBGQ(f);
    D3D11_MESSAGE_SEVERITY sev = D3D11_MESSAGE_SEVERITY_INFO;
    D3D11_INFO_QUEUE_FILTER fl = make_deny_severity(&sev);
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    sev = D3D11_MESSAGE_SEVERITY_ERROR;   /* changing the caller's array afterwards must not affect the stack */
    memset(&fl, 0, sizeof(fl));
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "still denied");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_retrieval_filter_hides_messages)
{
    DBGQ(f);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "info");
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_WARNING, "warning");
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "info2");
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 3);
    D3D11_MESSAGE_SEVERITY sev = D3D11_MESSAGE_SEVERITY_INFO;
    D3D11_INFO_QUEUE_FILTER fl = make_deny_severity(&sev);
    EXPECT_HR(ID3D11InfoQueue_PushRetrievalFilter(IQ, &fl), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 3);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessagesAllowedByRetrievalFilter(IQ), 1);
    D3D11_MESSAGE *m = get_msg(IQ, 0, NULL);   /* indices are relative to the retrieval-filtered view */
    if (m) { EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_WARNING); free(m); } else CHECK(m != NULL);
    ID3D11InfoQueue_PopRetrievalFilter(IQ);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessagesAllowedByRetrievalFilter(IQ), 3);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_break_on_severity_roundtrip)
{
    DBGQ(f);
    static const D3D11_MESSAGE_SEVERITY s[] = { D3D11_MESSAGE_SEVERITY_CORRUPTION, D3D11_MESSAGE_SEVERITY_ERROR, D3D11_MESSAGE_SEVERITY_WARNING,
        D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_SEVERITY_MESSAGE };
    for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); i++) {
        BOOL orig = ID3D11InfoQueue_GetBreakOnSeverity(IQ, s[i]);
        EXPECT_EQ(orig, FALSE);                    /* Win10: no break-on-* is armed by default */
        EXPECT_HR(ID3D11InfoQueue_SetBreakOnSeverity(IQ, s[i], TRUE), S_OK);
        EXPECT_EQ(ID3D11InfoQueue_GetBreakOnSeverity(IQ, s[i]), TRUE);
        EXPECT_HR(ID3D11InfoQueue_SetBreakOnSeverity(IQ, s[i], FALSE), S_OK);
        EXPECT_EQ(ID3D11InfoQueue_GetBreakOnSeverity(IQ, s[i]), FALSE);
        ID3D11InfoQueue_SetBreakOnSeverity(IQ, s[i], orig);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_break_on_category_roundtrip)
{
    DBGQ(f);
    for (int c = D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED; c <= D3D11_MESSAGE_CATEGORY_SHADER; c++) {
        BOOL orig = ID3D11InfoQueue_GetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)c);
        EXPECT_HR(ID3D11InfoQueue_SetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)c, TRUE), S_OK);
        EXPECT_EQ(ID3D11InfoQueue_GetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)c), TRUE);
        EXPECT_HR(ID3D11InfoQueue_SetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)c, FALSE), S_OK);
        EXPECT_EQ(ID3D11InfoQueue_GetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)c), FALSE);
        ID3D11InfoQueue_SetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)c, orig);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_break_on_id_roundtrip)
{
    DBGQ(f);
    static const D3D11_MESSAGE_ID ids[] = { D3D11_MESSAGE_ID_STRING_FROM_APPLICATION, D3D11_MESSAGE_ID_CREATEBUFFER_UNRECOGNIZEDFORMAT,
        D3D11_MESSAGE_ID_DEVICE_DRAW_VERTEX_BUFFER_NOT_SET };
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        BOOL orig = ID3D11InfoQueue_GetBreakOnID(IQ, ids[i]);
        EXPECT_HR(ID3D11InfoQueue_SetBreakOnID(IQ, ids[i], TRUE), S_OK);
        EXPECT_EQ(ID3D11InfoQueue_GetBreakOnID(IQ, ids[i]), TRUE);
        EXPECT_HR(ID3D11InfoQueue_SetBreakOnID(IQ, ids[i], FALSE), S_OK);
        EXPECT_EQ(ID3D11InfoQueue_GetBreakOnID(IQ, ids[i]), FALSE);
        ID3D11InfoQueue_SetBreakOnID(IQ, ids[i], orig);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_invalid_call_logs_error)
{
    DBG(f, 1);
    /* an immutable buffer without initial data is rejected by the runtime and the layer reports it */
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0};
    ID3D11Buffer *b = NULL;
    HRESULT hr = ID3D11Device_CreateBuffer(f.dev, &bd, NULL, &b);
    EXPECT_HR(hr, E_INVALIDARG);
    CHECK(b == NULL);
    /* Win10 22H2: exactly two ERROR messages, both STATE_CREATION: the specific cause, then the E_INVALIDARG return report */
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 2);
    static const D3D11_MESSAGE_ID want[2] = { D3D11_MESSAGE_ID_CREATEBUFFER_INVALIDINITIALDATA, D3D11_MESSAGE_ID_CREATEBUFFER_INVALIDARG_RETURN };
    for (UINT64 i = 0; i < 2; i++) {
        D3D11_MESSAGE *m = get_msg(IQ, i, NULL);
        CHECKF(m != NULL, "message %u", (unsigned)i);
        if (!m) continue;
        EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_ERROR);
        EXPECT_EQ(m->Category, D3D11_MESSAGE_CATEGORY_STATE_CREATION);
        EXPECT_EQ(m->ID, want[i]);
        CHECK(m->pDescription && strstr(m->pDescription, "CreateBuffer"));
        free(m);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_valid_calls_add_no_error_messages)
{
    DBG(f, 1);
    ID3D11DeviceChild *o = NULL;
    for (int k = K_BUF; k <= K_DSV; k++) {
        if (SUCCEEDED(rc_make(k, f.dev, &o))) RC_RELEASE(o);
    }
    UINT64 n = ID3D11InfoQueue_GetNumStoredMessages(IQ);
    for (UINT64 i = 0; i < n && i < 16; i++) {
        D3D11_MESSAGE *m = get_msg(IQ, i, NULL);
        if (!m) continue;
        CHECKF(m->Severity != D3D11_MESSAGE_SEVERITY_ERROR && m->Severity != D3D11_MESSAGE_SEVERITY_CORRUPTION, "unexpected %s", m->pDescription ? m->pDescription : "");
        free(m);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_add_message_invalid_args_note)
{
    DBGQ(f);
    HRESULT hr = ID3D11InfoQueue_AddMessage(IQ, D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED, D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    hr = ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    hr = ID3D11InfoQueue_AddApplicationMessage(IQ, (D3D11_MESSAGE_SEVERITY)99, "bad severity");
    EXPECT_HR(hr, S_OK);                      /* Win10: out-of-range enum values are not validated, the message is stored */
    hr = ID3D11InfoQueue_AddMessage(IQ, (D3D11_MESSAGE_CATEGORY)99, D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION, "bad category");
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 2);
    ID3D11InfoQueue_ClearStoredMessages(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_empty_description)
{
    DBGQ(f);
    HRESULT hr = ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "");
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    D3D11_MESSAGE *m = get_msg(IQ, 0, NULL);
    CHECK(m != NULL);
    if (m) { EXPECT_EQ(m->DescriptionByteLength, 1); CHECK(m->pDescription && !m->pDescription[0]); free(m); }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_long_description)
{
    DBGQ(f);
    char *s = malloc(4096);
    memset(s, 'a', 4095); s[4095] = 0;
    HRESULT hr = ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, s);
    EXPECT_HR(hr, S_OK);
    D3D11_MESSAGE *m = get_msg(IQ, 0, NULL);
    CHECK(m != NULL);
    if (m) {
        CHECK(m->DescriptionByteLength >= 4095);
        CHECK(m->pDescription && !strncmp(m->pDescription, s, 4095));
        free(m);
    }
    free(s);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_messages_survive_object_destruction)
{
    DBGQ(f);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "kept");
    ID3D11DeviceChild *o = NULL;
    if (SUCCEEDED(rc_make(K_BUF, f.dev, &o))) RC_RELEASE(o);
    CHECK(ID3D11InfoQueue_GetNumStoredMessages(IQ) >= 1);
    D3D11_MESSAGE *m = get_msg(IQ, 0, NULL);
    CHECK(m != NULL);
    if (m) { CHECK(m->pDescription && !strcmp(m->pDescription, "kept")); free(m); }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_device_release_with_pending_messages)
{
    DBGQ(f);
    for (int i = 0; i < 20; i++) ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "pending");
    ID3D11InfoQueue_SetMuteDebugOutput(IQ, TRUE);
    SAFE_RELEASE(f.iq);   /* mute stays on; releasing everything must not crash */
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_private_data_on_debug_device)
{
    DBG(f, 0);
    static const char name[] = "named device";
    EXPECT_HR(ID3D11Device_SetPrivateData(f.dev, &WKPDID_D3DDebugObjectName, (UINT)strlen(name), name), S_OK);
    UINT sz = 0;
    HRESULT hr = ID3D11Device_GetPrivateData(f.dev, &WKPDID_D3DDebugObjectName, &sz, NULL);
    CHECK(hr == S_OK || hr == HR_MORE_DATA);
    EXPECT_EQ(sz, strlen(name));
    EXPECT_HR(ID3D11Device_SetPrivateData(f.dev, &WKPDID_D3DDebugObjectName, 0, NULL), S_OK);
    dbg_close(&f);
}

/* ---- default filters and argument rules, measured on Win10 22H2 ---------------------------------------------- */
TEST(d3d11_debug_refcount_infoqueue_default_storage_filter_denies_info_only)
{
    DBG(f, 1);   /* default filter kept */
    SIZE_T len = 0;
    EXPECT_HR(ID3D11InfoQueue_GetStorageFilter(IQ, NULL, &len), S_FALSE);
    EXPECT_EQ(len, 100);
    D3D11_INFO_QUEUE_FILTER *fl = malloc(len);
    SIZE_T l2 = len;
    EXPECT_HR(ID3D11InfoQueue_GetStorageFilter(IQ, fl, &l2), S_OK);
    EXPECT_EQ(fl->AllowList.NumCategories + fl->AllowList.NumSeverities + fl->AllowList.NumIDs, 0);
    EXPECT_EQ(fl->DenyList.NumCategories, 0);
    EXPECT_EQ(fl->DenyList.NumIDs, 0);
    EXPECT_EQ(fl->DenyList.NumSeverities, 1);
    if (fl->DenyList.NumSeverities == 1) EXPECT_EQ(fl->DenyList.pSeverityList[0], D3D11_MESSAGE_SEVERITY_INFO);
    free(fl);
    static const struct { D3D11_MESSAGE_SEVERITY s; HRESULT hr; } t[] = {
        { D3D11_MESSAGE_SEVERITY_CORRUPTION, S_OK }, { D3D11_MESSAGE_SEVERITY_ERROR, S_OK }, { D3D11_MESSAGE_SEVERITY_WARNING, S_OK },
        { D3D11_MESSAGE_SEVERITY_INFO, S_FALSE }, { D3D11_MESSAGE_SEVERITY_MESSAGE, S_OK } };
    UINT64 want = 0;
    for (int i = 0; i < 5; i++) {
        EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(IQ, t[i].s, "default filter"), t[i].hr);
        if (t[i].hr == S_OK) want++;
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), want);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_default_retrieval_filter_empty)
{
    DBG(f, 1);
    SIZE_T len = 0;
    EXPECT_HR(ID3D11InfoQueue_GetRetrievalFilter(IQ, NULL, &len), S_FALSE);
    EXPECT_EQ(len, 96);                                         /* sizeof(D3D11_INFO_QUEUE_FILTER) with empty lists */
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_clear_storage_filter_allows_info)
{
    DBG(f, 1);
    EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "denied by default"), S_FALSE);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 0);
    ID3D11InfoQueue_ClearStorageFilter(IQ);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), 1);   /* Clear empties the top filter, it does not pop it */
    EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "allowed"), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_push_empty_filter_allows_info_over_default)
{
    DBG(f, 1);
    EXPECT_HR(ID3D11InfoQueue_PushEmptyStorageFilter(IQ), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), 2);
    EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "allowed"), S_OK);   /* only the top filter applies */
    ID3D11InfoQueue_PopStorageFilter(IQ);
    EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "denied again"), S_FALSE);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_push_null_filter_invalid)
{
    DBG(f, 1);
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11InfoQueue_PushRetrievalFilter(IQ, NULL), E_INVALIDARG);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), 1);
    EXPECT_EQ(ID3D11InfoQueue_GetRetrievalFilterStackSize(IQ), 0);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_filter_stack_depth_40)
{
    DBG(f, 1);
    for (int i = 0; i < 40; i++) EXPECT_HR(ID3D11InfoQueue_PushEmptyStorageFilter(IQ), S_OK);   /* no small fixed limit */
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), 41);
    for (int i = 0; i < 40; i++) ID3D11InfoQueue_PopStorageFilter(IQ);
    EXPECT_EQ(ID3D11InfoQueue_GetStorageFilterStackSize(IQ), 1);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_stored_filter_roundtrip_all_lists)
{
    DBGQ(f);
    D3D11_MESSAGE_ID ids[2] = { 1, 2 };
    D3D11_MESSAGE_CATEGORY cat = D3D11_MESSAGE_CATEGORY_EXECUTION;
    D3D11_MESSAGE_SEVERITY sev = D3D11_MESSAGE_SEVERITY_MESSAGE;
    D3D11_INFO_QUEUE_FILTER fl; memset(&fl, 0, sizeof fl);
    fl.AllowList.NumIDs = 2; fl.AllowList.pIDList = ids; fl.AllowList.NumSeverities = 1; fl.AllowList.pSeverityList = &sev;
    fl.DenyList.NumCategories = 1; fl.DenyList.pCategoryList = &cat;
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    SIZE_T len = 0;
    EXPECT_HR(ID3D11InfoQueue_GetStorageFilter(IQ, NULL, &len), S_FALSE);
    EXPECT_EQ(len, 112);                                      /* header + 2 ids + 1 severity + 1 category */
    D3D11_INFO_QUEUE_FILTER *g = malloc(len);
    EXPECT_HR(ID3D11InfoQueue_GetStorageFilter(IQ, g, &len), S_OK);
    EXPECT_EQ(g->AllowList.NumIDs, 2); EXPECT_EQ(g->AllowList.NumSeverities, 1); EXPECT_EQ(g->AllowList.NumCategories, 0);
    EXPECT_EQ(g->DenyList.NumCategories, 1); EXPECT_EQ(g->DenyList.NumIDs, 0); EXPECT_EQ(g->DenyList.NumSeverities, 0);
    if (g->AllowList.NumIDs == 2) { EXPECT_EQ(g->AllowList.pIDList[0], 1); EXPECT_EQ(g->AllowList.pIDList[1], 2); }
    if (g->DenyList.NumCategories == 1) EXPECT_EQ(g->DenyList.pCategoryList[0], D3D11_MESSAGE_CATEGORY_EXECUTION);
    free(g);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_break_flags_accept_out_of_range)
{
    DBGQ(f);
    EXPECT_HR(ID3D11InfoQueue_SetBreakOnSeverity(IQ, (D3D11_MESSAGE_SEVERITY)99, TRUE), S_OK);   /* no enum validation on Win10 */
    EXPECT_EQ(ID3D11InfoQueue_GetBreakOnSeverity(IQ, (D3D11_MESSAGE_SEVERITY)99), TRUE);
    EXPECT_HR(ID3D11InfoQueue_SetBreakOnSeverity(IQ, (D3D11_MESSAGE_SEVERITY)99, FALSE), S_OK);
    EXPECT_HR(ID3D11InfoQueue_SetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)99, TRUE), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)99), TRUE);
    EXPECT_HR(ID3D11InfoQueue_SetBreakOnCategory(IQ, (D3D11_MESSAGE_CATEGORY)99, FALSE), S_OK);
    for (int c = 0; c <= 13; c++) EXPECT_HR(ID3D11InfoQueue_AddMessage(IQ, (D3D11_MESSAGE_CATEGORY)c, D3D11_MESSAGE_SEVERITY_MESSAGE, 3, "cat"), S_OK);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 14);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_message_count_limit_zero_invalid)
{
    DBGQ(f);
    UINT64 orig = ID3D11InfoQueue_GetMessageCountLimit(IQ);
    EXPECT_HR(ID3D11InfoQueue_SetMessageCountLimit(IQ, 0), E_INVALIDARG);
    EXPECT_EQ(ID3D11InfoQueue_GetMessageCountLimit(IQ), orig);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_long_description_exact_length)
{
    DBGQ(f);
    char *s = malloc(5000); memset(s, 'a', 4999); s[4999] = 0;
    EXPECT_HR(ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_WARNING, s), S_OK);   /* no truncation up to 5000 bytes */
    SIZE_T len = 0;
    EXPECT_HR(ID3D11InfoQueue_GetMessage(IQ, 0, NULL, &len), S_FALSE);
    EXPECT_EQ(len, sizeof(D3D11_MESSAGE) + 5000);
    free(s);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_second_map_logs_error_message)
{
    DBGQ(f);
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0};
    ID3D11Buffer *buf = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(f.dev, &bd, NULL, &buf), S_OK);
    if (buf) {
        D3D11_MAPPED_SUBRESOURCE m1, m2;
        EXPECT_HR(ID3D11DeviceContext_Map(f.ctx, (ID3D11Resource *)buf, 0, D3D11_MAP_READ_WRITE, 0, &m1), S_OK);
        ID3D11InfoQueue_ClearStoredMessages(IQ);
        /* Win10: the call itself still succeeds and returns the same pointer, but the layer reports a RESOURCE_MANIPULATION error */
        EXPECT_HR(ID3D11DeviceContext_Map(f.ctx, (ID3D11Resource *)buf, 0, D3D11_MAP_READ_WRITE, 0, &m2), S_OK);
        CHECK(m1.pData == m2.pData);
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
        D3D11_MESSAGE *m = get_msg(IQ, 0, NULL);
        CHECK(m != NULL);
        if (m) {
            EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_ERROR);
            EXPECT_EQ(m->Category, D3D11_MESSAGE_CATEGORY_RESOURCE_MANIPULATION);
            EXPECT_EQ(m->ID, 2097213);
            CHECK(m->pDescription && strstr(m->pDescription, "already mapped"));
            free(m);
        }
        ID3D11DeviceContext_Unmap(f.ctx, (ID3D11Resource *)buf, 0);
        SAFE_RELEASE(buf);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_resource_creation_messages_are_info)
{
    DBGQ(f);
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0};
    ID3D11Buffer *b = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(f.dev, &bd, NULL, &b), S_OK);
    /* Win10: with INFO allowed, object creation is announced (STATE_CREATION, INFO) with the live-object bookkeeping text */
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    D3D11_MESSAGE *m = get_msg(IQ, 0, NULL);
    CHECK(m != NULL);
    if (m) {
        EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_INFO);
        EXPECT_EQ(m->Category, D3D11_MESSAGE_CATEGORY_STATE_CREATION);
        EXPECT_EQ(m->ID, 2097228);
        CHECK(m->pDescription && strstr(m->pDescription, "Create ID3D11Buffer"));
        CHECK(m->pDescription && strstr(m->pDescription, "ExtRef=1"));
        free(m);
    }
    ID3D11InfoQueue_ClearStoredMessages(IQ);
    SAFE_RELEASE(b);
    EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);   /* and its destruction */
    m = get_msg(IQ, 0, NULL);
    if (m) { CHECK(m->pDescription && strstr(m->pDescription, "Destroy ID3D11Buffer")); free(m); } else CHECK(m != NULL);
    dbg_close(&f);
}
