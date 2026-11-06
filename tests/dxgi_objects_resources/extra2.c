/* IDXGIDevice2::EnqueueSetEvent and a few device-level corners not covered elsewhere. */
#include "objs.h"
#include <dxgi1_2.h>

static IDXGIDevice2 *get_dev2(objs_ctx *c)
{
    IDXGIDevice2 *d = NULL;
    if (FAILED(IDXGIDevice_QueryInterface(c->dx, &IID_IDXGIDevice2, (void **)&d))) return NULL;
    return d;
}

TEST(objs_enqueue_event_signals_after_work)
{
    NEED_DEV(c);
    IDXGIDevice2 *d = get_dev2(&c);
    if (!d) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_HR_OK(IDXGIDevice2_EnqueueSetEvent(d, ev));
    ID3D11DeviceContext_Flush(c.ctx);
    EXPECT_EQ(WaitForSingleObject(ev, 5000), WAIT_OBJECT_0);   /* signalled once the queued GPU work has completed */
    CloseHandle(ev);
    IDXGIDevice2_Release(d);
    objs_close(&c);
}
TEST(objs_enqueue_event_two_events)
{
    NEED_DEV(c);
    IDXGIDevice2 *d = get_dev2(&c);
    if (!d) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL), b = CreateEventA(NULL, FALSE, FALSE, NULL);
    EXPECT_HR_OK(IDXGIDevice2_EnqueueSetEvent(d, a));
    EXPECT_HR_OK(IDXGIDevice2_EnqueueSetEvent(d, b));
    ID3D11DeviceContext_Flush(c.ctx);
    EXPECT_EQ(WaitForSingleObject(a, 5000), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(b, 5000), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_TIMEOUT);       /* the auto-reset event fired exactly once */
    CloseHandle(a); CloseHandle(b);
    IDXGIDevice2_Release(d);
    objs_close(&c);
}
TEST(objs_enqueue_event_invalid_handles)
{
    NEED_DEV(c);
    IDXGIDevice2 *d = get_dev2(&c);
    if (!d) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    HANDLE mtx = CreateMutexA(NULL, FALSE, NULL);
    EXPECT_HR(IDXGIDevice2_EnqueueSetEvent(d, NULL), E_INVALIDARG);
    EXPECT_HR(IDXGIDevice2_EnqueueSetEvent(d, INVALID_HANDLE_VALUE), E_INVALIDARG);
    EXPECT_HR(IDXGIDevice2_EnqueueSetEvent(d, (HANDLE)(ULONG_PTR)0x7ffc), E_INVALIDARG);
    EXPECT_HR(IDXGIDevice2_EnqueueSetEvent(d, mtx), E_INVALIDARG);
    CloseHandle(mtx);
    IDXGIDevice2_Release(d);
    objs_close(&c);
}
TEST(objs_enqueue_event_already_signalled_and_closed)
{
    NEED_DEV(c);
    IDXGIDevice2 *d = get_dev2(&c);
    if (!d) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, TRUE, NULL);
    EXPECT_HR_OK(IDXGIDevice2_EnqueueSetEvent(d, ev));
    ID3D11DeviceContext_Flush(c.ctx);
    EXPECT_EQ(WaitForSingleObject(ev, 1000), WAIT_OBJECT_0);
    ResetEvent(ev);
    EXPECT_HR_OK(IDXGIDevice2_EnqueueSetEvent(d, ev));
    CloseHandle(ev);                                          /* the runtime duplicates the handle: closing ours is harmless */
    ID3D11DeviceContext_Flush(c.ctx);
    Sleep(200);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(c.dev), S_OK);
    IDXGIDevice2_Release(d);
    objs_close(&c);
}
