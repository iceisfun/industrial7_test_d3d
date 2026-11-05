/* Device context interface ladder, ID3D11Multithread, deferred contexts, command lists, context states. */
#include "ifv.h"

#define QE(iid, w) { &iid, #iid, w }

T(ctx_qi_ladder_immediate) {
    IFV_DEV(d, c);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_IUnknown, 1), QE(IID_ID3D11DeviceChild, 1), QE(IID_ID3D11DeviceContext, 1), QE(IID_ID3D11DeviceContext1, 1),
        QE(IID_ID3D11DeviceContext2, 1), QE(IID_ID3D11DeviceContext3, 1), QE(IID_ID3D11DeviceContext4, 1),
        QE(IID_ID3D11Multithread, 1), QE(IID_ID3DUserDefinedAnnotation, 1), QE(IID_ID3D11VideoContext, -1),
        QE(IID_ID3D11VideoContext1, -1), QE(IID_ID3D11VideoContext2, -1), QE(ifv_IID_VideoContext3, -1),
        QE(IID_ID3D10Multithread, -1),
    };
    ifv_check_qi_table(c, tab, sizeof(tab) / sizeof(tab[0]), "immediate context");
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_qi_ladder_deferred) {
    IFV_DEV(d, c);
    ID3D11DeviceContext *dc = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(d, 0, &dc))) { t_skip("no deferred context"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    static const struct ifv_qi_ent tab[] = {
        QE(IID_IUnknown, 1), QE(IID_ID3D11DeviceChild, 1), QE(IID_ID3D11DeviceContext, 1), QE(IID_ID3D11DeviceContext1, 1),
        QE(IID_ID3D11DeviceContext2, 1), QE(IID_ID3D11DeviceContext3, 1), QE(IID_ID3D11DeviceContext4, 1),
        QE(IID_ID3D11Multithread, 0), QE(IID_ID3DUserDefinedAnnotation, 1),
        QE(IID_ID3D11VideoContext, -1), QE(IID_ID3D11VideoContext1, -1),
    };
    ifv_check_qi_table(dc, tab, sizeof(tab) / sizeof(tab[0]), "deferred context");
    SAFE_RELEASE(dc); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_qi_unsupported) {
    IFV_DEV(d, c);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_ID3D11Device, 0), QE(IID_ID3D11Device1, 0), QE(IID_ID3D11Device5, 0), QE(IID_IDXGIDevice, 0),
        QE(IID_ID3D11VideoDevice, 0), QE(IID_ID3D11VideoDevice1, 0), QE(IID_ID3D11Debug, 0), QE(IID_ID3D11InfoQueue, 0),
        QE(IID_ID3D11SwitchToRef, 0), QE(IID_ID3D11On12Device, 0), QE(IID_ID3D11Resource, 0), QE(IID_ID3D11CommandList, 0),
        QE(IID_ID3DDeviceContextState, 0), QE(IID_ID3D11Fence, 0), QE(IID_ID3D11Asynchronous, 0),
    };
    ifv_check_qi_table(c, tab, sizeof(tab) / sizeof(tab[0]), "context");
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_qi_identity_and_refcount) {
    IFV_DEV(d, c);
    static const GUID *iids[] = { &IID_ID3D11DeviceContext1, &IID_ID3D11DeviceContext2, &IID_ID3D11DeviceContext3,
        &IID_ID3D11DeviceContext4, &IID_ID3D11Multithread, &IID_ID3DUserDefinedAnnotation, &IID_ID3D11DeviceChild };
    void *id0 = ifv_identity(c);
    for (unsigned i = 0; i < sizeof(iids) / sizeof(iids[0]); i++) {
        ULONG before = d3dh_refcount(c);
        IUnknown *u = NULL;
        HRESULT hr = ID3D11DeviceContext_QueryInterface(c, iids[i], (void **)&u);
        if (FAILED(hr)) { t_note("QI %s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr)); continue; }
        CHECKF(ifv_identity(u) == id0, "identity differs for %s", d3dh_guid_str(iids[i]));
        EXPECT_EQ(d3dh_refcount(c), before + 1);
        IUnknown_Release(u);
        EXPECT_EQ(d3dh_refcount(c), before);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_context_holds_device_reference) {
    IFV_DEV(d, c);
    ID3D11DeviceContext *dc = NULL;
    ID3D11Device_CreateDeferredContext(d, 0, &dc);
    ULONG with = d3dh_refcount(d);
    SAFE_RELEASE(dc);
    EXPECT_EQ(d3dh_refcount(d), with - 1);
    /* The device stays usable through an extra reference after the app drops the immediate context pointer. */
    ID3D11Device_AddRef(d);
    SAFE_RELEASE(c);
    ID3D11DeviceContext *c2 = NULL;
    ID3D11Device_GetImmediateContext(d, &c2);
    CHECK(c2 != NULL);
    SAFE_RELEASE(c2);
    ID3D11Device_Release(d);
    SAFE_RELEASE(d);
}

T(mt_default_unprotected) {
    IFV_DEV(d, c);
    ID3D11Multithread *mt = NULL;
    EXPECT_HR(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt), S_OK);
    if (mt) {
        EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), FALSE);
        SAFE_RELEASE(mt);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(mt_set_returns_previous) {
    IFV_DEV(d, c);
    ID3D11Multithread *mt = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt))) { t_skip("no ID3D11Multithread"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, TRUE), FALSE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), TRUE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, TRUE), TRUE);
    /* any non-zero value counts as TRUE and is normalised */
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, 5), TRUE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), TRUE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, FALSE), TRUE);
    EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mt), FALSE);
    EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mt, FALSE), FALSE);
    SAFE_RELEASE(mt); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(mt_state_shared_between_interfaces) {
    IFV_DEV(d, c);
    ID3D11Multithread *mc = NULL, *md = NULL;
    ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mc);
    HRESULT hr = ID3D11Device_QueryInterface(d, &IID_ID3D11Multithread, (void **)&md);
    t_note("device QI ID3D11Multithread -> %s", d3dh_hr_name(hr));
    if (mc && md) {
        EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(md, TRUE), FALSE);
        EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(mc), TRUE);
        EXPECT_EQ(ID3D11Multithread_SetMultithreadProtected(mc, FALSE), TRUE);
        EXPECT_EQ(ID3D11Multithread_GetMultithreadProtected(md), FALSE);
    } else t_skip("need ID3D11Multithread on both device and context");
    SAFE_RELEASE(md); SAFE_RELEASE(mc); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(mt_deferred_context_unaffected) {
    IFV_DEV(d, c);
    ID3D11Multithread *mc = NULL;
    ID3D11DeviceContext *dc = NULL;
    ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mc);
    ID3D11Device_CreateDeferredContext(d, 0, &dc);
    if (mc && dc) {
        ID3D11Multithread_SetMultithreadProtected(mc, TRUE);
        void *p = NULL;
        EXPECT_HR(ID3D11DeviceContext_QueryInterface(dc, &IID_ID3D11Multithread, &p), E_NOINTERFACE);
        ID3D11Multithread_SetMultithreadProtected(mc, FALSE);
    } else t_skip("need multithread + deferred context");
    SAFE_RELEASE(dc); SAFE_RELEASE(mc); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(mt_enter_leave_recursive) {
    IFV_DEV(d, c);
    ID3D11Multithread *mt = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt))) { t_skip("no ID3D11Multithread"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    ID3D11Multithread_SetMultithreadProtected(mt, TRUE);
    for (int i = 0; i < 4; i++) ID3D11Multithread_Enter(mt);
    for (int i = 0; i < 4; i++) ID3D11Multithread_Leave(mt);
    /* the device remains usable after balanced Enter/Leave */
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    ID3D11Multithread_SetMultithreadProtected(mt, FALSE);
    SAFE_RELEASE(mt); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

struct mt_arg { ID3D11Multithread *mt; HANDLE started, done; };
static DWORD WINAPI mt_worker(void *p) {
    struct mt_arg *a = p;
    SetEvent(a->started);
    ID3D11Multithread_Enter(a->mt);
    ID3D11Multithread_Leave(a->mt);
    SetEvent(a->done);
    return 0;
}

T(mt_enter_blocks_other_thread) {
    IFV_DEV(d, c);
    ID3D11Multithread *mt = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt))) { t_skip("no ID3D11Multithread"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    ID3D11Multithread_SetMultithreadProtected(mt, TRUE);
    struct mt_arg a = { mt, CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL) };
    ID3D11Multithread_Enter(mt);
    HANDLE th = CreateThread(NULL, 0, mt_worker, &a, 0, NULL);
    CHECK(th != NULL);
    if (th) {
        EXPECT_EQ(WaitForSingleObject(a.started, 5000), WAIT_OBJECT_0);
        /* the worker cannot get through Enter while we hold the device lock */
        DWORD w = WaitForSingleObject(a.done, 300);
        t_check(w == WAIT_TIMEOUT, __FILE__, __LINE__, "worker blocked in Enter", "wait returned 0x%lx", w);
        ID3D11Multithread_Leave(mt);
        w = WaitForSingleObject(a.done, 5000);
        t_check(w == WAIT_OBJECT_0, __FILE__, __LINE__, "worker released after Leave", "wait returned 0x%lx", w);
        WaitForSingleObject(th, 5000);
        CloseHandle(th);
    } else ID3D11Multithread_Leave(mt);
    CloseHandle(a.started); CloseHandle(a.done);
    ID3D11Multithread_SetMultithreadProtected(mt, FALSE);
    SAFE_RELEASE(mt); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(mt_singlethreaded_device_note) {
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    if (FAILED(d3dh_make_device11(&d, &c, NULL, NULL, D3D11_CREATE_DEVICE_SINGLETHREADED))) { t_skip("no single threaded device"); return; }
    ID3D11Multithread *mt = NULL;
    HRESULT hr = ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11Multithread, (void **)&mt);
    t_note("singlethreaded context QI ID3D11Multithread -> %s", d3dh_hr_name(hr));
    if (mt) {
        t_note("protected default = %d", ID3D11Multithread_GetMultithreadProtected(mt));
        SAFE_RELEASE(mt);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_immediate_finish_command_list_fails) {
    IFV_DEV(d, c);
    EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(c), 0);
    ID3D11CommandList *cl = (void *)(ULONG_PTR)1;
    HRESULT hr = ID3D11DeviceContext_FinishCommandList(c, FALSE, &cl);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    hr = ID3D11DeviceContext_FinishCommandList(c, TRUE, &cl);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_deferred_command_list_basics) {
    IFV_DEV(d, c);
    ID3D11DeviceContext *dc = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(d, 0, &dc))) { t_skip("no deferred"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_BUFFER_DESC bd = { 256, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    ID3D11Buffer *a = NULL, *b = NULL;
    ID3D11Device_CreateBuffer(d, &bd, NULL, &a);
    ID3D11Device_CreateBuffer(d, &bd, NULL, &b);
    if (a && b) ID3D11DeviceContext_CopyResource(dc, (ID3D11Resource *)b, (ID3D11Resource *)a);
    ID3D11CommandList *cl = NULL;
    ULONG drc = d3dh_refcount(d);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
    if (cl) {
        EXPECT_EQ(d3dh_refcount(cl), 1);
        EXPECT_EQ(ID3D11CommandList_GetContextFlags(cl), 0);
        ID3D11Device *g = NULL;
        ID3D11CommandList_GetDevice(cl, &g);
        CHECK(g == d);
        SAFE_RELEASE(g);
        EXPECT_HR(ifv_qi(cl, &IID_ID3D11DeviceChild, NULL), S_OK);
        EXPECT_HR(ifv_qi(cl, &IID_ID3D11DeviceContext, NULL), E_NOINTERFACE);
        EXPECT_HR(ifv_qi(cl, &IID_ID3D11Resource, NULL), E_NOINTERFACE);
        /* a command list can be executed repeatedly */
        ID3D11DeviceContext_ExecuteCommandList(c, cl, FALSE);
        ID3D11DeviceContext_ExecuteCommandList(c, cl, TRUE);
        ID3D11DeviceContext_Flush(c);
        EXPECT_EQ(D3DH_RELEASE_RC(cl), 0);
    }
    EXPECT_EQ(d3dh_refcount(d), drc);
    /* the deferred context is reset by FinishCommandList and can be reused */
    cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
    SAFE_RELEASE(cl);
    SAFE_RELEASE(b); SAFE_RELEASE(a); SAFE_RELEASE(dc); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_deferred_finish_null_out) {
    IFV_DEV(d, c);
    ID3D11DeviceContext *dc = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(d, 0, &dc))) { t_skip("no deferred"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    HRESULT hr = ID3D11DeviceContext_FinishCommandList(dc, FALSE, NULL);
    t_note("FinishCommandList(NULL out) = %s", d3dh_hr_name(hr));
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
    SAFE_RELEASE(cl);
    SAFE_RELEASE(dc); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_deferred_restore_state_flag) {
    IFV_DEV(d, c);
    ID3D11DeviceContext *dc = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(d, 0, &dc))) { t_skip("no deferred"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    ID3D11DeviceContext_IASetPrimitiveTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    D3D11_PRIMITIVE_TOPOLOGY t = 0;
    ID3D11DeviceContext_IAGetPrimitiveTopology(dc, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);   /* FALSE: state is cleared */
    ID3D11DeviceContext_IAGetPrimitiveTopology(dc, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    SAFE_RELEASE(cl);
    ID3D11DeviceContext_IASetPrimitiveTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, TRUE, &cl), S_OK);    /* TRUE: state is kept */
    ID3D11DeviceContext_IAGetPrimitiveTopology(dc, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    SAFE_RELEASE(cl);
    SAFE_RELEASE(dc); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_immediate_topology_default) {
    IFV_DEV(d, c);
    D3D11_PRIMITIVE_TOPOLOGY t = 0xdead;
    ID3D11DeviceContext_IAGetPrimitiveTopology(c, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    ID3D11DeviceContext_IASetPrimitiveTopology(c, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    ID3D11DeviceContext_IAGetPrimitiveTopology(c, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    ID3D11DeviceContext_ClearState(c);
    ID3D11DeviceContext_IAGetPrimitiveTopology(c, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_annotation_without_tool) {
    IFV_DEV(d, c);
    ID3DUserDefinedAnnotation *an = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3DUserDefinedAnnotation, (void **)&an))) { t_skip("no annotation"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    /* no graphics debugger attached: the markers are accepted and report "not enabled" */
    EXPECT_EQ(ID3DUserDefinedAnnotation_BeginEvent(an, L"ifv event"), -1);
    EXPECT_EQ(ID3DUserDefinedAnnotation_EndEvent(an), -1);
    ID3DUserDefinedAnnotation_SetMarker(an, L"ifv marker");
    EXPECT_EQ(ID3DUserDefinedAnnotation_GetStatus(an), FALSE);
    SAFE_RELEASE(an);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_state_create_validation) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1))) { t_skip("no Device1"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D_FEATURE_LEVEL fl = fl_ > D3D_FEATURE_LEVEL_11_1 ? D3D_FEATURE_LEVEL_11_1 : fl_, chosen = 0xc0de0000;
    ID3DDeviceContextState *st = (void *)(ULONG_PTR)0xc0de0001;
    EXPECT_HR(ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 1, D3D11_SDK_VERSION, &IID_ID3D11Device1, NULL, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 1, D3D11_SDK_VERSION, &IID_ID3D11Device1, &chosen, NULL), S_FALSE);
    EXPECT_EQ(chosen, fl);
    chosen = 0xc0de0000;
    EXPECT_HR(ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 0, D3D11_SDK_VERSION, &IID_ID3D11Device1, &chosen, &st), E_INVALIDARG);
    EXPECT_EQ(chosen, 0);
    CHECK(st == NULL);
    EXPECT_HR(ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 0, D3D11_SDK_VERSION, &IID_ID3D11Device1, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device1_CreateDeviceContextState(d1, 0, NULL, 0, D3D11_SDK_VERSION, &IID_ID3D11Device1, NULL, NULL), E_INVALIDARG);
    st = NULL;
    EXPECT_HR(ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 1, D3D11_SDK_VERSION, &IID_ID3D11Device1, NULL, &st), S_OK);
    if (st) {
        EXPECT_EQ(d3dh_refcount(st), 1);
        EXPECT_HR(ifv_qi(st, &IID_ID3D11DeviceChild, NULL), S_OK);
        EXPECT_HR(ifv_qi(st, &IID_ID3D11Device, NULL), E_NOINTERFACE);
        ID3D11Device *g = NULL;
        ID3DDeviceContextState_GetDevice(st, &g);
        CHECK(g == d);
        SAFE_RELEASE(g);
        SAFE_RELEASE(st);
    }
    SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_state_create_emulated_interface) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1))) { t_skip("no Device1"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D_FEATURE_LEVEL fl = fl_ > D3D_FEATURE_LEVEL_11_1 ? D3D_FEATURE_LEVEL_11_1 : fl_;
    static const struct { const GUID *iid; const char *n; } e[] = {
        { &IID_ID3D11Device, "ID3D11Device" }, { &IID_ID3D11Device1, "ID3D11Device1" },
        { &IID_ID3D10Device, "ID3D10Device" }, { &IID_ID3D10Device1, "ID3D10Device1" },
        { &IID_ID3D11DeviceContext, "ID3D11DeviceContext" }, { &GUID_NULL, "GUID_NULL" },
    };
    for (unsigned i = 0; i < sizeof(e) / sizeof(e[0]); i++) {
        ID3DDeviceContextState *st = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 1, D3D11_SDK_VERSION, e[i].iid, NULL, &st);
        t_note("CreateDeviceContextState(emulated %s) -> %s", e[i].n, d3dh_hr_name(hr));
        if (FAILED(hr)) CHECKF(st == NULL, "state not NULLed for %s", e[i].n); else SAFE_RELEASE(st);
    }
    ID3DDeviceContextState *st = NULL;
    EXPECT_HR(ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 1, D3D11_SDK_VERSION, &IID_ID3D11Device, NULL, &st), S_OK);
    SAFE_RELEASE(st);
    SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_state_swap_roundtrip) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL; ID3D11DeviceContext1 *c1 = NULL;
    ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1);
    ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11DeviceContext1, (void **)&c1);
    if (!d1 || !c1) { t_skip("need Device1/Context1"); SAFE_RELEASE(c1); SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D_FEATURE_LEVEL fl = fl_ > D3D_FEATURE_LEVEL_11_1 ? D3D_FEATURE_LEVEL_11_1 : fl_;
    ID3DDeviceContextState *st = NULL, *prev = NULL, *back = NULL;
    EXPECT_HR(ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 1, D3D11_SDK_VERSION, &IID_ID3D11Device1, NULL, &st), S_OK);
    if (!st) { SAFE_RELEASE(c1); SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    ID3D11DeviceContext_IASetPrimitiveTopology(c, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, st, &prev);
    CHECK(prev != NULL);
    D3D11_PRIMITIVE_TOPOLOGY t = 0xdead;
    ID3D11DeviceContext_IAGetPrimitiveTopology(c, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);      /* a fresh state is empty */
    ID3D11DeviceContext_IASetPrimitiveTopology(c, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, prev, &back);
    CHECK(back == st);                                      /* swapping out returns the state swapped in */
    ID3D11DeviceContext_IAGetPrimitiveTopology(c, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);       /* original state restored */
    ID3D11DeviceContext1_SwapDeviceContextState(c1, st, NULL);   /* previous state pointer is optional */
    ID3D11DeviceContext_IAGetPrimitiveTopology(c, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);       /* state object kept its own topology */
    ID3D11DeviceContext1_SwapDeviceContextState(c1, prev, NULL);
    SAFE_RELEASE(back); SAFE_RELEASE(prev); SAFE_RELEASE(st);
    SAFE_RELEASE(c1); SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_state_swap_refcounts) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL; ID3D11DeviceContext1 *c1 = NULL;
    ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1);
    ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11DeviceContext1, (void **)&c1);
    if (!d1 || !c1) { t_skip("need Device1/Context1"); SAFE_RELEASE(c1); SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D_FEATURE_LEVEL fl = fl_ > D3D_FEATURE_LEVEL_11_1 ? D3D_FEATURE_LEVEL_11_1 : fl_;
    ID3DDeviceContextState *st = NULL, *prev = NULL;
    ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 1, D3D11_SDK_VERSION, &IID_ID3D11Device1, NULL, &st);
    if (st) {
        ULONG r0 = d3dh_refcount(st);
        ID3D11DeviceContext1_SwapDeviceContextState(c1, st, &prev);
        t_note("state refcount before %lu after swap-in %lu; previous state refcount %lu", r0, d3dh_refcount(st), d3dh_refcount(prev));
        EXPECT_EQ(d3dh_refcount(prev), 1);   /* only the returned reference keeps the old default state */
        ID3D11DeviceContext1_SwapDeviceContextState(c1, prev, NULL);
        SAFE_RELEASE(prev);
        EXPECT_EQ(d3dh_refcount(st), r0);
        SAFE_RELEASE(st);
    }
    SAFE_RELEASE(c1); SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(ctx_state_private_data) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL;
    ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1);
    if (!d1) { t_skip("no Device1"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D_FEATURE_LEVEL fl = fl_ > D3D_FEATURE_LEVEL_11_1 ? D3D_FEATURE_LEVEL_11_1 : fl_;
    ID3DDeviceContextState *st = NULL;
    ID3D11Device1_CreateDeviceContextState(d1, 0, &fl, 1, D3D11_SDK_VERSION, &IID_ID3D11Device1, NULL, &st);
    if (st) {
        static const GUID g = {0xfdb37466, 0x428f, 0x4edf, {0xa3, 0x7f, 0x9b, 0x1d, 0xf4, 0x88, 0xc5, 0xfc}};
        float v[4] = { 1.25f, 2.5f, 3.75f, 5.0f }, got[4] = {0};
        UINT sz = sizeof(got);
        EXPECT_HR(ID3DDeviceContextState_SetPrivateData(st, &g, sizeof(v), v), S_OK);
        EXPECT_HR(ID3DDeviceContextState_GetPrivateData(st, &g, &sz, got), S_OK);
        EXPECT_EQ(sz, sizeof(v));
        CHECK(!memcmp(v, got, sizeof(v)));
        sz = 0;
        EXPECT_HR(ID3DDeviceContextState_GetPrivateData(st, &g, &sz, NULL), S_OK);
        EXPECT_EQ(sz, sizeof(v));
        sz = 4;
        EXPECT_HR(ID3DDeviceContextState_GetPrivateData(st, &g, &sz, got), DXGI_ERROR_MORE_DATA);
        EXPECT_EQ(sz, sizeof(v));
        SAFE_RELEASE(st);
    }
    SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}
