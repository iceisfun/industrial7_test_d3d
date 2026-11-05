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
    t_note("ID3D11SwitchToRef on debug device -> %s", d3dh_hr_name(hr));
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
    t_note("initial feature mask 0x%x", orig);
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
        HRESULT hr = ID3D11Debug_SetFeatureMask(f.dbg, bits[i]);
        t_note("SetFeatureMask(0x%x) -> %s, GetFeatureMask -> 0x%x", bits[i], d3dh_hr_name(hr), ID3D11Debug_GetFeatureMask(f.dbg));
    }
    ID3D11Debug_SetFeatureMask(f.dbg, orig);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_present_delay_note)
{
    DBG(f, 0);
    UINT orig = ID3D11Debug_GetPresentPerRenderOpDelay(f.dbg);
    HRESULT hr = ID3D11Debug_SetPresentPerRenderOpDelay(f.dbg, 0);
    t_note("initial delay %u, SetPresentPerRenderOpDelay(0) -> %s, Get -> %u", orig, d3dh_hr_name(hr), ID3D11Debug_GetPresentPerRenderOpDelay(f.dbg));
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
        EXPECT_HR(ID3D11Debug_SetSwapChain(f.dbg, sc), S_OK);
        t_note("swap chain refcount %lu -> %lu after SetSwapChain", base, d3dh_refcount(sc));
        IDXGISwapChain *got = NULL;
        EXPECT_HR(ID3D11Debug_GetSwapChain(f.dbg, &got), S_OK);
        CHECK(got == sc);
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
    t_note("ReportLiveDeviceObjects(0) -> %s", d3dh_hr_name(hr));
    hr = ID3D11Debug_ReportLiveDeviceObjects(f.dbg, (D3D11_RLDO_FLAGS)0x80);
    t_note("ReportLiveDeviceObjects(0x80) -> %s", d3dh_hr_name(hr));
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

TEST(d3d11_debug_refcount_debug_validate_context_note)
{
    DBG(f, 1);
    HRESULT hr = ID3D11Debug_ValidateContext(f.dbg, f.ctx);
    t_note("ValidateContext(immediate, empty pipeline) -> %s", d3dh_hr_name(hr));
    hr = ID3D11Debug_ValidateContextForDispatch(f.dbg, f.ctx);
    t_note("ValidateContextForDispatch(immediate, empty pipeline) -> %s", d3dh_hr_name(hr));
    ID3D11DeviceContext *dc = NULL;
    if (SUCCEEDED(ID3D11Device_CreateDeferredContext(f.dev, 0, &dc))) {
        hr = ID3D11Debug_ValidateContext(f.dbg, dc);
        t_note("ValidateContext(deferred) -> %s", d3dh_hr_name(hr));
        SAFE_RELEASE(dc);
    }
    ID3D11InfoQueue_ClearStoredMessages(f.iq);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_debug_validate_context_foreign_device_note)
{
    DBG(f, 1);
    ID3D11Device *d2 = NULL; ID3D11DeviceContext *c2 = NULL;
    if (FAILED(d3dh_make_device11(&d2, &c2, NULL, NULL, 0))) { dbg_close(&f); t_skip("second device unavailable"); return; }
    HRESULT hr = ID3D11Debug_ValidateContext(f.dbg, c2);
    t_note("ValidateContext(context of another device) -> %s", d3dh_hr_name(hr));
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
    if (FAILED(hr)) { dbg_close(&f); t_skip("ID3D11SwitchToRef not exposed (%s)", d3dh_hr_name(hr)); return; }
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
    if (FAILED(hr)) { dbg_close(&f); t_skip("ID3D11SwitchToRef not exposed (%s)", d3dh_hr_name(hr)); return; }
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
    t_note("message count limit %llu, storage stack %u, retrieval stack %u",
           (unsigned long long)ID3D11InfoQueue_GetMessageCountLimit(IQ), ID3D11InfoQueue_GetStorageFilterStackSize(IQ), ID3D11InfoQueue_GetRetrievalFilterStackSize(IQ));
    CHECK(ID3D11InfoQueue_GetMessageCountLimit(IQ) > 0);
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
    DBG(f, 1);
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
        CHECK(m->DescriptionByteLength >= strlen(text));
        CHECK(m->pDescription && !strncmp(m->pDescription, text, strlen(text)));
        CHECK(len >= sizeof(D3D11_MESSAGE) + strlen(text));
        t_note("DescriptionByteLength %llu, GetMessage length %llu, sizeof(D3D11_MESSAGE) %u", (unsigned long long)m->DescriptionByteLength, (unsigned long long)len, (unsigned)sizeof(D3D11_MESSAGE));
        free(m);
    }
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_add_message_fields_roundtrip)
{
    DBG(f, 1);
    static const struct { D3D11_MESSAGE_CATEGORY c; D3D11_MESSAGE_SEVERITY s; D3D11_MESSAGE_ID id; } t[] = {
        { D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED, D3D11_MESSAGE_SEVERITY_MESSAGE, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION },
        { D3D11_MESSAGE_CATEGORY_MISCELLANEOUS, D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION },
        { D3D11_MESSAGE_CATEGORY_INITIALIZATION, D3D11_MESSAGE_SEVERITY_WARNING, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION },
        { D3D11_MESSAGE_CATEGORY_EXECUTION, D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_ID_DEVICE_DRAW_VERTEX_BUFFER_NOT_SET },
        { D3D11_MESSAGE_CATEGORY_SHADER, D3D11_MESSAGE_SEVERITY_MESSAGE, D3D11_MESSAGE_ID_UNKNOWN },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        char desc[32]; sprintf(desc, "entry %u", (unsigned)i);
        HRESULT hr = ID3D11InfoQueue_AddMessage(IQ, t[i].c, t[i].s, t[i].id, desc);
        t_note("AddMessage(cat %d, sev %d, id %d) -> %s", t[i].c, t[i].s, t[i].id, d3dh_hr_name(hr));
    }
    UINT64 n = ID3D11InfoQueue_GetNumStoredMessages(IQ);
    t_note("%llu of %u messages stored", (unsigned long long)n, (unsigned)(sizeof(t) / sizeof(t[0])));
    CHECK(n >= 1);
    /* the first message is the first one added and keeps its fields */
    D3D11_MESSAGE *m = get_msg(IQ, 0, NULL);
    if (m) {
        EXPECT_EQ(m->Category, t[0].c); EXPECT_EQ(m->Severity, t[0].s); EXPECT_EQ(m->ID, t[0].id);
        CHECK(m->pDescription && !strcmp(m->pDescription, "entry 0"));
        free(m);
    } else CHECK(m != NULL);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_messages_in_insertion_order)
{
    DBG(f, 1);
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
    DBG(f, 1);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "size protocol");
    SIZE_T len = 0;
    HRESULT hr = ID3D11InfoQueue_GetMessage(IQ, 0, NULL, &len);
    EXPECT_HR(hr, S_OK);
    CHECK(len > sizeof(D3D11_MESSAGE));
    /* a buffer that is one byte too small must not be accepted */
    D3D11_MESSAGE *m = malloc(len + 16);
    memset(m, 0xcc, len + 16);
    SIZE_T small = len - 1;
    hr = ID3D11InfoQueue_GetMessage(IQ, 0, m, &small);
    t_note("GetMessage with length-1 buffer -> %s, length now %llu", d3dh_hr_name(hr), (unsigned long long)small);
    CHECK(hr != S_OK);
    small = 0;
    hr = ID3D11InfoQueue_GetMessage(IQ, 0, m, &small);
    t_note("GetMessage with length 0 buffer -> %s", d3dh_hr_name(hr));
    CHECK(hr != S_OK);
    /* exact size and larger size succeed and report the real length */
    SIZE_T exact = len;
    EXPECT_HR(ID3D11InfoQueue_GetMessage(IQ, 0, m, &exact), S_OK);
    EXPECT_EQ(exact, len);
    SIZE_T big = len + 16;
    EXPECT_HR(ID3D11InfoQueue_GetMessage(IQ, 0, m, &big), S_OK);
    EXPECT_EQ(big, len);
    free(m);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_get_message_out_of_range)
{
    DBG(f, 1);
    SIZE_T len = 0;
    HRESULT hr = ID3D11InfoQueue_GetMessage(IQ, 0, NULL, &len);
    t_note("GetMessage(0) on an empty queue -> %s", d3dh_hr_name(hr));
    CHECK(hr != S_OK);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "one");
    len = 0;
    hr = ID3D11InfoQueue_GetMessage(IQ, 1, NULL, &len);
    t_note("GetMessage(1) with one stored -> %s", d3dh_hr_name(hr));
    CHECK(hr != S_OK);
    len = 0;
    hr = ID3D11InfoQueue_GetMessage(IQ, (UINT64)-1, NULL, &len);
    CHECK(hr != S_OK);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_clear_stored_messages)
{
    DBG(f, 1);
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
    DBG(f, 1);
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
    DBG(f, 1);
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
    DBG(f, 1);
    UINT64 orig = ID3D11InfoQueue_GetMessageCountLimit(IQ);
    for (int i = 0; i < 6; i++) ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "p");
    HRESULT hr = ID3D11InfoQueue_SetMessageCountLimit(IQ, 2);
    t_note("shrinking the limit below the stored count -> %s, stored now %llu", d3dh_hr_name(hr), (unsigned long long)ID3D11InfoQueue_GetNumStoredMessages(IQ));
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
    DBG(f, 1);
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
    DBG(f, 1);
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
    DBG(f, 1);
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
    DBG(f, 1);
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
    DBG(f, 1);
    EXPECT_HR(ID3D11InfoQueue_PushEmptyStorageFilter(IQ), S_OK);
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "i");
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_WARNING, "w");
    ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_MESSAGE, "m");
    t_note("empty storage filter stored %llu of 3 messages", (unsigned long long)ID3D11InfoQueue_GetNumStoredMessages(IQ));
    CHECK(ID3D11InfoQueue_GetNumStoredMessages(IQ) == 3);
    ID3D11InfoQueue_PopStorageFilter(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_get_storage_filter_size_protocol)
{
    DBG(f, 1);
    D3D11_MESSAGE_SEVERITY sevs[2] = { D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_SEVERITY_MESSAGE };
    D3D11_INFO_QUEUE_FILTER fl; memset(&fl, 0, sizeof(fl));
    fl.DenyList.NumSeverities = 2; fl.DenyList.pSeverityList = sevs;
    EXPECT_HR(ID3D11InfoQueue_PushStorageFilter(IQ, &fl), S_OK);
    SIZE_T len = 0;
    HRESULT hr = ID3D11InfoQueue_GetStorageFilter(IQ, NULL, &len);
    EXPECT_HR(hr, S_OK);
    CHECK(len >= sizeof(D3D11_INFO_QUEUE_FILTER) + 2 * sizeof(D3D11_MESSAGE_SEVERITY));
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
    DBG(f, 1);
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
    DBG(f, 1);
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
    DBG(f, 1);
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
    DBG(f, 1);
    static const D3D11_MESSAGE_SEVERITY s[] = { D3D11_MESSAGE_SEVERITY_CORRUPTION, D3D11_MESSAGE_SEVERITY_ERROR, D3D11_MESSAGE_SEVERITY_WARNING,
        D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_SEVERITY_MESSAGE };
    for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); i++) {
        BOOL orig = ID3D11InfoQueue_GetBreakOnSeverity(IQ, s[i]);
        t_note("default break-on-severity %d = %d", s[i], (int)orig);
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
    DBG(f, 1);
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
    DBG(f, 1);
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
    UINT64 n = ID3D11InfoQueue_GetNumStoredMessages(IQ);
    CHECK(n >= 1);
    for (UINT64 i = 0; i < n && i < 8; i++) {
        D3D11_MESSAGE *m = get_msg(IQ, i, NULL);
        if (!m) continue;
        t_note("message %llu: cat %d sev %d id %d: %s", (unsigned long long)i, m->Category, m->Severity, m->ID, m->pDescription ? m->pDescription : "");
        if (i == 0) {
            EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_ERROR);
            t_note("first message category %d", m->Category);
        }
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
    DBG(f, 1);
    HRESULT hr = ID3D11InfoQueue_AddMessage(IQ, D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED, D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION, NULL);
    t_note("AddMessage(NULL description) -> %s", d3dh_hr_name(hr));
    hr = ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, NULL);
    t_note("AddApplicationMessage(NULL) -> %s", d3dh_hr_name(hr));
    hr = ID3D11InfoQueue_AddApplicationMessage(IQ, (D3D11_MESSAGE_SEVERITY)99, "bad severity");
    t_note("AddApplicationMessage(severity 99) -> %s", d3dh_hr_name(hr));
    hr = ID3D11InfoQueue_AddMessage(IQ, (D3D11_MESSAGE_CATEGORY)99, D3D11_MESSAGE_SEVERITY_INFO, D3D11_MESSAGE_ID_STRING_FROM_APPLICATION, "bad category");
    t_note("AddMessage(category 99) -> %s", d3dh_hr_name(hr));
    ID3D11InfoQueue_ClearStoredMessages(IQ);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_empty_description)
{
    DBG(f, 1);
    HRESULT hr = ID3D11InfoQueue_AddApplicationMessage(IQ, D3D11_MESSAGE_SEVERITY_INFO, "");
    t_note("AddApplicationMessage(\"\") -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(IQ), 1);
    dbg_close(&f);
}

TEST(d3d11_debug_refcount_infoqueue_long_description)
{
    DBG(f, 1);
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
    DBG(f, 1);
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
    DBG(f, 1);
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
