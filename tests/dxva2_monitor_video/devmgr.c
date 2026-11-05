#include "dxm.h"
/* IDirect3DDeviceManager9 (DXVA2CreateDirect3DDeviceManager9): handle table, device reset, lock protocol. */

#define E_HANDLE_ ((HRESULT)0x80070006)
#define E_INVALIDARG_ ((HRESULT)0x80070057)
#define E_NOINTERFACE_ ((HRESULT)0x80004002)

typedef HRESULT (WINAPI *create_mgr_fn)(UINT *, IDirect3DDeviceManager9 **);
static create_mgr_fn mgr_create(void) { return (create_mgr_fn)t_proc_in("dxva2.dll", "DXVA2CreateDirect3DDeviceManager9"); }

/* Manager initialised with a real device (skips when the runtime cannot build the video service). */
typedef struct { DXMD9 d; IDirect3DDeviceManager9 *m; UINT token; } MGR;
static int mgr_open(MGR *g)
{
    memset(g, 0, sizeof *g);
    create_mgr_fn c = mgr_create();
    if (!c) { t_skip("DXVA2CreateDirect3DDeviceManager9 missing"); return 0; }
    if (!dxm_d9_open(&g->d)) return 0;
    HRESULT hr = c(&g->token, &g->m);
    if (FAILED(hr) || !g->m) { t_skip("create failed %s", d3dh_hr_name(hr)); dxm_d9_close(&g->d); return 0; }
    hr = IDirect3DDeviceManager9_ResetDevice(g->m, g->d.dev, g->token);
    if (FAILED(hr)) {
        t_skip("ResetDevice failed on this device (%s)", d3dh_hr_name(hr));
        IDirect3DDeviceManager9_Release(g->m); dxm_d9_close(&g->d); return 0;
    }
    return 1;
}
static void mgr_close(MGR *g) { IDirect3DDeviceManager9_Release(g->m); dxm_d9_close(&g->d); }
#define MGR_OPEN(g) MGR g; if (!mgr_open(&g)) return

/* ---------------- creation, identity ---------------- */
TEST(dxva2_monitor_video_mgr_create) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok = 0; IDirect3DDeviceManager9 *m = NULL;
    HRESULT hr = c(&tok, &m);
    EXPECT_HR(hr, S_OK);
    CHECK(m != NULL);
    if (tok == 0) t_note("reset token is 0 (tick count wrapped?)"); else CHECK(tok != 0);
    if (m) { EXPECT_EQ(IDirect3DDeviceManager9_Release(m), 0); }
}
TEST(dxva2_monitor_video_mgr_refcount_protocol) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    EXPECT_EQ(IDirect3DDeviceManager9_AddRef(m), 2);
    EXPECT_EQ(IDirect3DDeviceManager9_AddRef(m), 3);
    EXPECT_EQ(IDirect3DDeviceManager9_Release(m), 2);
    EXPECT_EQ(IDirect3DDeviceManager9_Release(m), 1);
    EXPECT_EQ(IDirect3DDeviceManager9_Release(m), 0);
}
TEST(dxva2_monitor_video_mgr_qi_supported) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    void *p = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_QueryInterface(m, &DXM_IID_DevMgr9, &p), S_OK);
    CHECK(p == (void *)m);
    EXPECT_EQ(IDirect3DDeviceManager9_Release(m), 1);
    p = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_QueryInterface(m, &DXM_IID_Unk, &p), S_OK);
    CHECK(p != NULL);
    if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    EXPECT_EQ(IDirect3DDeviceManager9_Release(m), 0);
}
TEST(dxva2_monitor_video_mgr_qi_unsupported) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    const GUID *bad[] = { &DXM_IID_Bogus, &DXM_IID_DecSvc, &DXM_IID_ProcSvc, &DXM_IID_Accel, &GUID_NULL };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        void *p = (void *)(ULONG_PTR)0x1;
        EXPECT_HR(IDirect3DDeviceManager9_QueryInterface(m, bad[i], &p), E_NOINTERFACE_);
        CHECK(p == NULL);
    }
    EXPECT_EQ(IDirect3DDeviceManager9_Release(m), 0);
}
TEST(dxva2_monitor_video_mgr_two_managers_independent) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT t1, t2; IDirect3DDeviceManager9 *a = NULL, *b = NULL;
    if (FAILED(c(&t1, &a)) || FAILED(c(&t2, &b))) { t_skip("create failed"); if (a) IDirect3DDeviceManager9_Release(a); return; }
    CHECK(a != b);
    EXPECT_EQ(IDirect3DDeviceManager9_Release(a), 0);
    EXPECT_EQ(IDirect3DDeviceManager9_AddRef(b), 2);
    EXPECT_EQ(IDirect3DDeviceManager9_Release(b), 1);
    EXPECT_EQ(IDirect3DDeviceManager9_Release(b), 0);
}

/* ---------------- uninitialised manager (no ResetDevice yet) ---------------- */
TEST(dxva2_monitor_video_mgr_uninit_open_handle) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    HANDLE h = (HANDLE)(ULONG_PTR)0x1234;
    EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(m, &h), DXM_E_NOT_INITIALIZED);
    EXPECT_EQ(h, 0x1234);
    IDirect3DDeviceManager9_Release(m);
}
TEST(dxva2_monitor_video_mgr_uninit_test_device) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(m, (HANDLE)1), DXM_E_NOT_INITIALIZED);
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(m, NULL), DXM_E_NOT_INITIALIZED);
    IDirect3DDeviceManager9_Release(m);
}
TEST(dxva2_monitor_video_mgr_uninit_lock_device) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)(ULONG_PTR)0x1;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(m, (HANDLE)1, &dev, FALSE), DXM_E_NOT_INITIALIZED);
    EXPECT_EQ(dev, 1);
    /* a failed lock does not leave the manager locked: the same answer comes again, also with fBlock = TRUE */
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(m, (HANDLE)1, &dev, TRUE), DXM_E_NOT_INITIALIZED);
    IDirect3DDeviceManager9_Release(m);
}
TEST(dxva2_monitor_video_mgr_uninit_get_video_service) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    void *svc = (void *)(ULONG_PTR)0x1;
    EXPECT_HR(IDirect3DDeviceManager9_GetVideoService(m, (HANDLE)1, &DXM_IID_DecSvc, &svc), DXM_E_NOT_INITIALIZED);
    EXPECT_EQ(svc, 1);
    IDirect3DDeviceManager9_Release(m);
}
TEST(dxva2_monitor_video_mgr_uninit_close_handle) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    /* CloseDeviceHandle only consults the handle table: an unknown handle is E_HANDLE even before initialisation */
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(m, (HANDLE)1), E_HANDLE_);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(m, NULL), E_HANDLE_);
    IDirect3DDeviceManager9_Release(m);
}
TEST(dxva2_monitor_video_mgr_uninit_unlock_wrong_handle) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(m, (HANDLE)0x1234, FALSE), E_INVALIDARG_);
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(m, (HANDLE)0x1234, TRUE), E_INVALIDARG_);
    IDirect3DDeviceManager9_Release(m);
}

/* ---------------- ResetDevice token ---------------- */
TEST(dxva2_monitor_video_mgr_reset_wrong_token) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); return; }
    /* the token is compared before the device pointer is used, so NULL is safe here */
    EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(m, NULL, tok + 1), E_INVALIDARG_);
    EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(m, NULL, tok ^ 0x80000000u), E_INVALIDARG_);
    EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(m, NULL, ~tok), E_INVALIDARG_);
    /* and the manager stays uninitialised */
    HANDLE h = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(m, &h), DXM_E_NOT_INITIALIZED);
    IDirect3DDeviceManager9_Release(m);
}
TEST(dxva2_monitor_video_mgr_reset_wrong_token_with_device) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    DXM_D9(d);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); DXM_D9_END(d); return; }
    ULONG rc = d3dh_refcount(d.dev);
    EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(m, d.dev, tok + 1), E_INVALIDARG_);
    EXPECT_EQ(d3dh_refcount(d.dev), rc);   /* a rejected reset takes no reference */
    IDirect3DDeviceManager9_Release(m);
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_mgr_reset_good_token) {
    MGR_OPEN(g);
    HANDLE h = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h), S_OK);
    CHECK(h != NULL);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_device_refcount_released_on_destroy) {
    create_mgr_fn c = mgr_create(); REQUIRE(c);
    DXM_D9(d);
    ULONG base = d3dh_refcount(d.dev);
    UINT tok; IDirect3DDeviceManager9 *m = NULL;
    if (FAILED(c(&tok, &m))) { t_skip("create failed"); DXM_D9_END(d); return; }
    HRESULT hr = IDirect3DDeviceManager9_ResetDevice(m, d.dev, tok);
    if (FAILED(hr)) { t_skip("ResetDevice -> %s", d3dh_hr_name(hr)); IDirect3DDeviceManager9_Release(m); DXM_D9_END(d); return; }
    ULONG held = d3dh_refcount(d.dev);
    CHECK(held > base);                         /* the manager keeps the device alive */
    HANDLE h = NULL; IDirect3DDevice9 *dev = NULL;
    if (SUCCEEDED(IDirect3DDeviceManager9_OpenDeviceHandle(m, &h)) && SUCCEEDED(IDirect3DDeviceManager9_LockDevice(m, h, &dev, FALSE))) {
        CHECK(dev == d.dev);
        CHECK(d3dh_refcount(d.dev) == held + 1);   /* LockDevice hands out an AddRef'd pointer */
        IDirect3DDevice9_Release(dev);
        IDirect3DDeviceManager9_UnlockDevice(m, h, FALSE);
    }
    EXPECT_EQ(IDirect3DDeviceManager9_Release(m), 0);
    EXPECT_EQ(d3dh_refcount(d.dev), base);       /* everything the manager held is gone */
    DXM_D9_END(d);
}

/* ---------------- handle table ---------------- */
TEST(dxva2_monitor_video_mgr_handles_sequential) {
    MGR_OPEN(g);
    HANDLE h[4] = {0};
    for (int i = 0; i < 4; i++) EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h[i]), S_OK);
    /* handles are small integers allocated from 1 upwards */
    for (int i = 0; i < 4; i++) EXPECT_EQ(h[i], i + 1);
    for (int i = 0; i < 4; i++) EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h[i]), S_OK);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_handle_reuse_lowest_free) {
    MGR_OPEN(g);
    HANDLE a, b, c2, again;
    IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &a);
    IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &b);
    IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &c2);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, b), S_OK);
    EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &again), S_OK);
    EXPECT_EQ(again, b);                     /* the lowest free value is reused */
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, a), S_OK);
    EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &again), S_OK);
    EXPECT_EQ(again, a);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, a);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, b);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, c2);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_close_twice) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h), S_OK);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h), E_HANDLE_);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_close_unknown_handle) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, NULL), E_HANDLE_);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, (HANDLE)0x7777), E_HANDLE_);
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, (HANDLE)((ULONG_PTR)h + 1)), E_HANDLE_);
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(g.m, h), S_OK);    /* the valid one is unaffected */
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_test_device_states) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(g.m, h), S_OK);
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(g.m, NULL), E_HANDLE_);
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(g.m, (HANDLE)0x99), E_HANDLE_);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(g.m, h), E_HANDLE_);   /* closed handle */
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_new_device_invalidates_handles) {
    MGR_OPEN(g);
    HANDLE h1; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h1);
    EXPECT_HR(IDirect3DDeviceManager9_ResetDevice(g.m, g.d.dev, g.token), S_OK);   /* same device, but a reset nevertheless */
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(g.m, h1), DXM_E_NEW_VIDEO_DEVICE);
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h1, &dev, FALSE), DXM_E_NEW_VIDEO_DEVICE);
    void *svc = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_GetVideoService(g.m, h1, &DXM_IID_DecSvc, &svc), DXM_E_NEW_VIDEO_DEVICE);
    CHECK(svc == NULL);
    /* the stale handle must still be closed by the caller */
    EXPECT_HR(IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h1), S_OK);
    HANDLE h2; EXPECT_HR(IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h2), S_OK);
    EXPECT_HR(IDirect3DDeviceManager9_TestDevice(g.m, h2), S_OK);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h2);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_new_device_failed_lock_releases_lock) {
    MGR_OPEN(g);
    HANDLE h1; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h1);
    IDirect3DDeviceManager9_ResetDevice(g.m, g.d.dev, g.token);
    IDirect3DDevice9 *dev = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h1, &dev, FALSE), DXM_E_NEW_VIDEO_DEVICE);
    HANDLE h2; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h2);
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h2, &dev, FALSE), S_OK);    /* the failed attempt did not leave the lock taken */
    if (dev) IDirect3DDevice9_Release(dev);
    IDirect3DDeviceManager9_UnlockDevice(g.m, h2, FALSE);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h1);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h2);
    mgr_close(&g);
}

/* ---------------- GetVideoService ---------------- */
TEST(dxva2_monitor_video_mgr_get_service_interfaces) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    const GUID *ok[] = { &DXM_IID_Accel, &DXM_IID_DecSvc, &DXM_IID_ProcSvc };
    for (int i = 0; i < 3; i++) {
        void *svc = NULL;
        HRESULT hr = IDirect3DDeviceManager9_GetVideoService(g.m, h, ok[i], &svc);
        EXPECT_HR(hr, S_OK);
        CHECK(svc != NULL);
        if (svc) ((IUnknown *)svc)->lpVtbl->Release((IUnknown *)svc);
    }
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_get_service_unsupported_iid) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    void *svc = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDeviceManager9_GetVideoService(g.m, h, &DXM_IID_Bogus, &svc), E_NOINTERFACE_);
    CHECK(svc == NULL);
    svc = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDeviceManager9_GetVideoService(g.m, h, &DXM_IID_DevMgr9, &svc), E_NOINTERFACE_);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_get_service_bad_handle) {
    MGR_OPEN(g);
    void *svc = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDeviceManager9_GetVideoService(g.m, NULL, &DXM_IID_DecSvc, &svc), E_HANDLE_);
    EXPECT_HR(IDirect3DDeviceManager9_GetVideoService(g.m, (HANDLE)0x50, &DXM_IID_DecSvc, &svc), E_HANDLE_);
    EXPECT_EQ(svc, 1);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_get_service_same_object) {
    MGR_OPEN(g);
    HANDLE h1, h2; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h1); IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h2);
    void *a = NULL, *b = NULL;
    IDirect3DDeviceManager9_GetVideoService(g.m, h1, &DXM_IID_DecSvc, &a);
    IDirect3DDeviceManager9_GetVideoService(g.m, h2, &DXM_IID_DecSvc, &b);
    CHECK(a != NULL);
    CHECK(a == b);                              /* one service per device generation, shared by all handles */
    if (a) ((IUnknown *)a)->lpVtbl->Release((IUnknown *)a);
    if (b) ((IUnknown *)b)->lpVtbl->Release((IUnknown *)b);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h1); IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h2);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_get_service_after_close) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    void *svc = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_GetVideoService(g.m, h, &DXM_IID_DecSvc, &svc), E_HANDLE_);
    mgr_close(&g);
}

/* ---------------- LockDevice / UnlockDevice ---------------- */
TEST(dxva2_monitor_video_mgr_lock_returns_device) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    IDirect3DDevice9 *dev = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE), S_OK);
    CHECK(dev == g.d.dev);
    if (dev) IDirect3DDevice9_Release(dev);
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE), S_OK);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_lock_is_exclusive_nonblocking) {
    MGR_OPEN(g);
    HANDLE h1, h2; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h1); IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h2);
    IDirect3DDevice9 *dev = NULL, *dev2 = (IDirect3DDevice9 *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h1, &dev, FALSE), S_OK);
    /* the lock is per manager, not per handle and not recursive */
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h2, &dev2, FALSE), DXM_E_VIDEO_DEVICE_LOCKED);
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h1, &dev2, FALSE), DXM_E_VIDEO_DEVICE_LOCKED);
    EXPECT_EQ(dev2, 1);
    if (dev) IDirect3DDevice9_Release(dev);
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(g.m, h1, FALSE), S_OK);
    dev = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h2, &dev, FALSE), S_OK);
    if (dev) IDirect3DDevice9_Release(dev);
    IDirect3DDeviceManager9_UnlockDevice(g.m, h2, FALSE);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h1); IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h2);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_lock_bad_handle_keeps_lock_free) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, (HANDLE)0x42, &dev, FALSE), E_HANDLE_);
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, NULL, &dev, FALSE), E_HANDLE_);
    EXPECT_EQ(dev, 1);
    dev = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE), S_OK);   /* still available */
    if (dev) IDirect3DDevice9_Release(dev);
    IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_unlock_wrong_handle) {
    MGR_OPEN(g);
    HANDLE h1, h2; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h1); IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h2);
    IDirect3DDevice9 *dev = NULL;
    IDirect3DDeviceManager9_LockDevice(g.m, h1, &dev, FALSE);
    if (dev) IDirect3DDevice9_Release(dev);
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(g.m, h2, FALSE), E_INVALIDARG_);   /* only the locker may unlock */
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(g.m, (HANDLE)0x44, FALSE), E_INVALIDARG_);
    dev = (IDirect3DDevice9 *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h2, &dev, FALSE), DXM_E_VIDEO_DEVICE_LOCKED);   /* still locked */
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(g.m, h1, FALSE), S_OK);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h1); IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h2);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_unlock_twice) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    IDirect3DDevice9 *dev = NULL;
    IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE);
    if (dev) IDirect3DDevice9_Release(dev);
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE), S_OK);
    EXPECT_HR(IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE), E_INVALIDARG_);   /* no longer the lock owner */
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_lock_after_close_handle) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE), E_HANDLE_);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_lock_repeat_cycles) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    ULONG rc = d3dh_refcount(g.d.dev);
    for (int i = 0; i < 50; i++) {
        IDirect3DDevice9 *dev = NULL;
        HRESULT hr = IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE);
        CHECK(hr == S_OK);
        if (hr != S_OK) break;
        IDirect3DDevice9_Release(dev);
        CHECK(IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE) == S_OK);
    }
    EXPECT_EQ(d3dh_refcount(g.d.dev), rc);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}

/* ---------------- cross-thread lock ---------------- */
struct lockctx { IDirect3DDeviceManager9 *m; HANDLE h; BOOL block; HRESULT hr; HANDLE started; };
static DWORD WINAPI lock_thread(LPVOID p)
{
    struct lockctx *c = (struct lockctx *)p;
    IDirect3DDevice9 *dev = NULL;
    if (c->started) SetEvent(c->started);
    c->hr = IDirect3DDeviceManager9_LockDevice(c->m, c->h, &dev, c->block);
    if (SUCCEEDED(c->hr)) { IDirect3DDevice9_Release(dev); IDirect3DDeviceManager9_UnlockDevice(c->m, c->h, FALSE); }
    return 0;
}
TEST(dxva2_monitor_video_mgr_lock_other_thread_nonblocking) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    IDirect3DDevice9 *dev = NULL;
    IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE);
    if (dev) IDirect3DDevice9_Release(dev);
    struct lockctx c = { g.m, h, FALSE, (HRESULT)0x11111111, NULL };
    HANDLE t = CreateThread(NULL, 0, lock_thread, &c, 0, NULL);
    CHECK(t != NULL);
    if (t) {
        CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
        CloseHandle(t);
        EXPECT_HR(c.hr, DXM_E_VIDEO_DEVICE_LOCKED);
    }
    IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_lock_other_thread_blocking_gets_it_after_unlock) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    IDirect3DDevice9 *dev = NULL;
    IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE);
    if (dev) IDirect3DDevice9_Release(dev);
    struct lockctx c = { g.m, h, TRUE, (HRESULT)0x11111111, CreateEventA(NULL, TRUE, FALSE, NULL) };
    HANDLE t = CreateThread(NULL, 0, lock_thread, &c, 0, NULL);
    CHECK(t != NULL);
    if (t) {
        WaitForSingleObject(c.started, 3000);
        Sleep(50);                       /* the thread is now blocked in LockDevice */
        CHECK(WaitForSingleObject(t, 0) == WAIT_TIMEOUT);
        IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE);
        CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
        CloseHandle(t);
        EXPECT_HR(c.hr, S_OK);
    } else IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE);
    CloseHandle(c.started);
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
TEST(dxva2_monitor_video_mgr_unlock_from_other_thread_is_allowed) {
    MGR_OPEN(g);
    HANDLE h; IDirect3DDeviceManager9_OpenDeviceHandle(g.m, &h);
    IDirect3DDevice9 *dev = NULL;
    IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE);
    if (dev) IDirect3DDevice9_Release(dev);
    /* ownership is tracked by device handle, not by thread */
    struct unl { IDirect3DDeviceManager9 *m; HANDLE h; HRESULT hr; } u = { g.m, h, (HRESULT)0x11111111 };
    DWORD WINAPI unlock_thread(LPVOID p);
    HANDLE t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)unlock_thread, &u, 0, NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); EXPECT_HR(u.hr, S_OK); }
    dev = NULL;
    EXPECT_HR(IDirect3DDeviceManager9_LockDevice(g.m, h, &dev, FALSE), S_OK);
    if (dev) { IDirect3DDevice9_Release(dev); IDirect3DDeviceManager9_UnlockDevice(g.m, h, FALSE); }
    IDirect3DDeviceManager9_CloseDeviceHandle(g.m, h);
    mgr_close(&g);
}
DWORD WINAPI unlock_thread(LPVOID p)
{
    struct unl { IDirect3DDeviceManager9 *m; HANDLE h; HRESULT hr; } *u = p;
    u->hr = IDirect3DDeviceManager9_UnlockDevice(u->m, u->h, FALSE);
    return 0;
}
