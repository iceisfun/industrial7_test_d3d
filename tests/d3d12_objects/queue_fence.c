/* d3d12_objects: command queues, command allocators, fences, device-child object protocol. */
#include "d12o.h"

/* ====== command queue ================================================================== */
static const struct { D3D12_COMMAND_LIST_TYPE t; const char *n; } qtypes[] = {
    { D3D12_COMMAND_LIST_TYPE_DIRECT, "DIRECT" }, { D3D12_COMMAND_LIST_TYPE_COMPUTE, "COMPUTE" }, { D3D12_COMMAND_LIST_TYPE_COPY, "COPY" },
};

TEST(d3d12_objects_queue_create_types) {
    NEED_DEV();
    for (unsigned i = 0; i < 3; i++) {
        D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.Type = qtypes[i].t;
        ID3D12CommandQueue *q = NULL;
        HRESULT hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
        EXPECT_HR(hr, S_OK);
        if (q) {
            D3D12_COMMAND_QUEUE_DESC g = d12_queue_desc(q);
            EXPECT_EQ(g.Type, qtypes[i].t);
            EXPECT_EQ(g.Priority, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL);
            EXPECT_EQ(g.Flags, D3D12_COMMAND_QUEUE_FLAG_NONE);
            if (g.NodeMask != 1) t_note("GetDesc().NodeMask = %u for NodeMask 0", g.NodeMask);
            SAFE_RELEASE(q);
        }
    }
}
TEST(d3d12_objects_queue_create_video_types) {
    NEED_DEV();
    static const D3D12_COMMAND_LIST_TYPE v[] = { D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE, D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS, D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE };
    for (unsigned i = 0; i < 3; i++) {
        D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.Type = v[i];
        ID3D12CommandQueue *q = NULL;
        HRESULT hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
        if (g_d12warp) CHECKF(hr == E_INVALIDARG, "video queue type %d on WARP (no video engines): %s", (int)v[i], d3dh_hr_name(hr));
        else t_note("video queue type %d on this adapter: %s", (int)v[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) { D3D12_COMMAND_QUEUE_DESC g = d12_queue_desc(q); EXPECT_EQ(g.Type, v[i]); }
        SAFE_RELEASE(q);
    }
}
TEST(d3d12_objects_queue_create_invalid_type) {
    NEED_DEV();
    static const int bad[] = { D3D12_COMMAND_LIST_TYPE_BUNDLE, 7, 99, -1 };
    for (unsigned i = 0; i < 4; i++) {
        D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.Type = (D3D12_COMMAND_LIST_TYPE)bad[i];
        ID3D12CommandQueue *q = NULL;
        HRESULT hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
        CHECKF((hr) == E_INVALIDARG, "type %d got %s", bad[i], d3dh_hr_name(hr));
        if (hr != E_INVALIDARG) t_note("queue type %d -> %s (E_INVALIDARG expected)", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(q);
    }
}
TEST(d3d12_objects_queue_priorities) {
    NEED_DEV();
    static const D3D12_COMMAND_QUEUE_PRIORITY pr[] = { D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, D3D12_COMMAND_QUEUE_PRIORITY_HIGH };
    for (unsigned i = 0; i < 2; i++) {
        D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.Priority = pr[i];
        ID3D12CommandQueue *q = NULL;
        HRESULT hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
        EXPECT_HR(hr, S_OK);
        if (q) { D3D12_COMMAND_QUEUE_DESC g = d12_queue_desc(q); EXPECT_EQ(g.Priority, pr[i]); SAFE_RELEASE(q); }
    }
}
TEST(d3d12_objects_queue_priority_realtime) {
    NEED_DEV();
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.Priority = D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME;
    ID3D12CommandQueue *q = NULL;
    HRESULT hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
    t_note("GLOBAL_REALTIME queue (needs privilege / hardware support): %s", d3dh_hr_name(hr));
    if (q) { D3D12_COMMAND_QUEUE_DESC g = d12_queue_desc(q); EXPECT_EQ(g.Priority, D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME); }
    SAFE_RELEASE(q);
}
TEST(d3d12_objects_queue_priority_invalid) {
    NEED_DEV();
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.Priority = 55;
    ID3D12CommandQueue *q = NULL;
    HRESULT hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
    CHECKF(hr == E_INVALIDARG, "queue priority 55: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(q);
}
TEST(d3d12_objects_queue_flags) {
    NEED_DEV();
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
    ID3D12CommandQueue *q = NULL;
    HRESULT hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
    CHECKF(hr == S_OK, "DISABLE_GPU_TIMEOUT queue: %s", d3dh_hr_name(hr));
    if (q) { D3D12_COMMAND_QUEUE_DESC g = d12_queue_desc(q); EXPECT_EQ(g.Flags, D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT); }
    SAFE_RELEASE(q);
    d.Flags = 0x80;   /* undefined flag bit */
    hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
    CHECKF(hr == E_INVALIDARG, "undefined queue flag 0x80: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(q);
}
TEST(d3d12_objects_queue_node_mask) {
    NEED_DEV();
    UINT nodes = ID3D12Device_GetNodeCount(dev);
    EXPECT_EQ(nodes, 1);
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.NodeMask = 1;
    ID3D12CommandQueue *q = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q), S_OK);
    if (q) { D3D12_COMMAND_QUEUE_DESC g = d12_queue_desc(q); EXPECT_EQ(g.NodeMask, 1); SAFE_RELEASE(q); }
    d.NodeMask = 2;
    HRESULT hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q);
    CHECKF((hr) == E_INVALIDARG, "node 2 on a one-node adapter: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(q);
}
TEST(d3d12_objects_queue_null_desc_and_ppv) {
    NEED_DEV();
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d);
    ID3D12CommandQueue *q = NULL;
    HRESULT hr = ID3D12Device_CreateCommandQueue(dev, NULL, &IID_ID3D12CommandQueue, (void **)&q);
    CHECKF((hr) == E_INVALIDARG, "NULL desc: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(q);
    /* wrong interface id */
    hr = ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12Fence, (void **)&q);
    CHECKF((hr) == E_NOINTERFACE, "riid=Fence: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(q);
}
TEST(d3d12_objects_queue_iids) {
    NEED_DEV();
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d);
    ID3D12CommandQueue *q = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q), S_OK);
    if (!q) return;
    void *o;
    o = NULL; EXPECT_HR(ID3D12CommandQueue_QueryInterface(q, &IID_IUnknown, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = NULL; EXPECT_HR(ID3D12CommandQueue_QueryInterface(q, &IID_ID3D12Object, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = NULL; EXPECT_HR(ID3D12CommandQueue_QueryInterface(q, &IID_ID3D12DeviceChild, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = NULL; EXPECT_HR(ID3D12CommandQueue_QueryInterface(q, &IID_ID3D12Pageable, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = (void *)1; EXPECT_HR(ID3D12CommandQueue_QueryInterface(q, &IID_ID3D12Fence, &o), E_NOINTERFACE); CHECK(o == NULL);
    o = (void *)1; EXPECT_HR(ID3D12CommandQueue_QueryInterface(q, &IID_ID3D12Resource, &o), E_NOINTERFACE); CHECK(o == NULL);
    o = (void *)1; EXPECT_HR(ID3D12CommandQueue_QueryInterface(q, &IID_ID3D12CommandList, &o), E_NOINTERFACE); CHECK(o == NULL);
    EXPECT_EQ(d3dh_refcount(q), 1);
    SAFE_RELEASE(q);
}
TEST(d3d12_objects_queue_exec_zero_lists) {
    NEED_DEV();
    d12ctx c = {0};
    EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    if (c.q) {
        ID3D12CommandList *dummy = NULL;
        ID3D12CommandQueue_ExecuteCommandLists(c.q, 0, &dummy);
        EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, 1), S_OK);
        EXPECT_HR(d12_wait(&c, 1), S_OK);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 1);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_queue_exec_empty_list_all_types) {
    NEED_DEV();
    for (unsigned i = 0; i < 3; i++) {
        d12ctx c = {0};
        HRESULT hr = d12_ctx_init(&c, dev, qtypes[i].t);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) {
            EXPECT_HR(d12_run(&c), S_OK);
            EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 1);
        }
        d12_ctx_free(&c);
    }
}
TEST(d3d12_objects_queue_exec_multiple_lists) {
    NEED_DEV();
    d12ctx c = {0}; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    if (!c.cl) { d12_ctx_free(&c); return; }
    ID3D12CommandAllocator *al2 = NULL; ID3D12GraphicsCommandList *cl2 = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&al2), S_OK);
    EXPECT_HR(ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, al2, NULL, &IID_ID3D12GraphicsCommandList, (void **)&cl2), S_OK);
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
    EXPECT_HR(ID3D12GraphicsCommandList_Close(cl2), S_OK);
    ID3D12CommandList *ls[2] = { (ID3D12CommandList *)c.cl, (ID3D12CommandList *)cl2 };
    ID3D12CommandQueue_ExecuteCommandLists(c.q, 2, ls);
    EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, 1), S_OK);
    EXPECT_HR(d12_wait(&c, 1), S_OK);
    SAFE_RELEASE(cl2); SAFE_RELEASE(al2);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_queue_timestamp_frequency) {
    NEED_DEV();
    for (unsigned i = 0; i < 3; i++) {
        D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); d.Type = qtypes[i].t;
        ID3D12CommandQueue *q = NULL; UINT64 f = 0xCDCDCDCD;
        if (FAILED(ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q))) continue;
        HRESULT hr = ID3D12CommandQueue_GetTimestampFrequency(q, &f);
        if (qtypes[i].t == D3D12_COMMAND_LIST_TYPE_COPY) {
            CHECKF(hr == S_OK, "COPY queue timestamp frequency: %s", d3dh_hr_name(hr));
            if (g_d12warp) CHECKF(f == 10000000ull, "COPY queue timestamp frequency %llu", (unsigned long long)f);
        } else {
            EXPECT_HR(hr, S_OK);
            CHECK(f > 0);
        }
        SAFE_RELEASE(q);
    }
}
TEST(d3d12_objects_queue_timestamp_frequency_consistent) {
    NEED_DEV();
    d12ctx a = {0}, b = {0}; UINT64 fa = 0, fb = 0;
    if (FAILED(d12_ctx_init(&a, dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) || FAILED(d12_ctx_init(&b, dev, D3D12_COMMAND_LIST_TYPE_COMPUTE))) { t_skip("no queues"); d12_ctx_free(&a); d12_ctx_free(&b); return; }
    EXPECT_HR(ID3D12CommandQueue_GetTimestampFrequency(a.q, &fa), S_OK);
    EXPECT_HR(ID3D12CommandQueue_GetTimestampFrequency(a.q, &fb), S_OK);
    EXPECT_EQ(fa, fb);
    fb = 0;
    HRESULT hr = ID3D12CommandQueue_GetTimestampFrequency(b.q, &fb);
    if (hr == S_OK && fa != fb) t_note("compute and direct queue timestamp frequencies differ: %llu vs %llu", (unsigned long long)fa, (unsigned long long)fb);
    d12_ctx_free(&a); d12_ctx_free(&b);
}
TEST(d3d12_objects_queue_clock_calibration) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no queue"); d12_ctx_free(&c); return; }
    UINT64 gpu1 = 0, cpu1 = 0, gpu2 = 0, cpu2 = 0, freq = 0;
    LARGE_INTEGER qpc, qpf; QueryPerformanceFrequency(&qpf); QueryPerformanceCounter(&qpc);
    HRESULT hr = ID3D12CommandQueue_GetClockCalibration(c.q, &gpu1, &cpu1);
    if (FAILED(hr)) { t_note("GetClockCalibration failed on this adapter: %s", d3dh_hr_name(hr)); d12_ctx_free(&c); return; }
    EXPECT_HR(hr, S_OK);
    CHECK(gpu1 != 0 || cpu1 != 0);
    /* CPU timestamp is in QueryPerformanceCounter units */
    LONGLONG diff = (LONGLONG)cpu1 - qpc.QuadPart; if (diff < 0) diff = -diff;
    CHECKF(diff < 10 * qpf.QuadPart, "cpu clock %llu vs qpc %lld", (unsigned long long)cpu1, (long long)qpc.QuadPart);
    Sleep(20);
    EXPECT_HR(ID3D12CommandQueue_GetClockCalibration(c.q, &gpu2, &cpu2), S_OK);
    CHECK(cpu2 >= cpu1);
    CHECK(gpu2 >= gpu1);
    EXPECT_HR(ID3D12CommandQueue_GetTimestampFrequency(c.q, &freq), S_OK);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_queue_signal_fence_values) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no queue"); d12_ctx_free(&c); return; }
    EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 0);
    for (UINT64 v = 1; v <= 4; v++) {
        EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, v * 10), S_OK);
        EXPECT_HR(d12_wait(&c, v * 10), S_OK);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), v * 10);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_queue_wait_already_signalled) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no queue"); d12_ctx_free(&c); return; }
    ID3D12Fence *f2 = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 5, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f2), S_OK);
    if (f2) {
        EXPECT_HR(ID3D12CommandQueue_Wait(c.q, f2, 5), S_OK);   /* equal value satisfies the wait */
        EXPECT_HR(ID3D12CommandQueue_Wait(c.q, f2, 1), S_OK);
        EXPECT_HR(ID3D12CommandQueue_Wait(c.q, f2, 0), S_OK);
        EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, 1), S_OK);
        EXPECT_HR(d12_wait(&c, 1), S_OK);
        SAFE_RELEASE(f2);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_queue_wait_blocks_until_cpu_signal) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no queue"); d12_ctx_free(&c); return; }
    ID3D12Fence *gate = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&gate), S_OK);
    if (gate) {
        EXPECT_HR(ID3D12CommandQueue_Wait(c.q, gate, 3), S_OK);
        EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, 1), S_OK);
        Sleep(100);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 0);   /* held back by the wait */
        EXPECT_HR(ID3D12Fence_Signal(gate, 2), S_OK);            /* not enough */
        Sleep(50);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 0);
        EXPECT_HR(ID3D12Fence_Signal(gate, 3), S_OK);
        EXPECT_HR(d12_wait(&c, 1), S_OK);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 1);
        SAFE_RELEASE(gate);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_queue_cross_queue_sync) {
    NEED_DEV();
    d12ctx a = {0}, b = {0};
    if (FAILED(d12_ctx_init(&a, dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) || FAILED(d12_ctx_init(&b, dev, D3D12_COMMAND_LIST_TYPE_COMPUTE))) { t_skip("no queues"); d12_ctx_free(&a); d12_ctx_free(&b); return; }
    ID3D12Fence *shared = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&shared), S_OK);
    if (shared) {
        EXPECT_HR(ID3D12CommandQueue_Wait(b.q, shared, 7), S_OK);
        EXPECT_HR(ID3D12CommandQueue_Signal(b.q, b.fence, 1), S_OK);
        Sleep(50);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(b.fence), 0);
        EXPECT_HR(ID3D12CommandQueue_Signal(a.q, shared, 7), S_OK);
        EXPECT_HR(d12_wait(&b, 1), S_OK);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(shared), 7);
        SAFE_RELEASE(shared);
    }
    d12_ctx_free(&a); d12_ctx_free(&b);
}
TEST(d3d12_objects_queue_getdevice) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no queue"); d12_ctx_free(&c); return; }
    ID3D12Device *d2 = NULL;
    ULONG before = d3dh_refcount(dev);
    EXPECT_HR(ID3D12CommandQueue_GetDevice(c.q, &IID_ID3D12Device, (void **)&d2), S_OK);
    CHECK(d2 == dev);
    if (d2) { EXPECT_EQ(d3dh_refcount(dev), before + 1); ID3D12Device_Release(d2); }
    void *o = (void *)1;
    EXPECT_HR(ID3D12CommandQueue_GetDevice(c.q, &IID_ID3D12Fence, &o), E_NOINTERFACE);
    CHECK(o == NULL);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_queue_device_lifetime) {
    ID3D12Device *dv = NULL;
    if (!d3dh_D3D12CreateDevice() || FAILED(d3dh_make_device12(&dv, D3D_FEATURE_LEVEL_11_0, NULL))) { t_skip("no D3D12 device"); return; }
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d);
    ID3D12CommandQueue *q = NULL; ID3D12CommandAllocator *al = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandQueue(dv, &d, &IID_ID3D12CommandQueue, (void **)&q), S_OK);
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dv, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&al), S_OK);
    CHECK(d3dh_refcount(dv) >= 3);   /* every child pins the device */
    ULONG rc = ID3D12Device_Release(dv);
    CHECK(rc >= 2);
    ID3D12Device *d2 = NULL;
    EXPECT_HR(ID3D12CommandAllocator_GetDevice(al, &IID_ID3D12Device, (void **)&d2), S_OK);
    CHECK(d2 == dv);
    D3D12_COMMAND_QUEUE_DESC g = d12_queue_desc(q);
    EXPECT_EQ(g.Type, D3D12_COMMAND_LIST_TYPE_DIRECT);
    EXPECT_HR(ID3D12CommandAllocator_Reset(al), S_OK);
    if (d2) ID3D12Device_Release(d2);
    SAFE_RELEASE(al); SAFE_RELEASE(q);
}

/* ====== command allocator ============================================================== */
TEST(d3d12_objects_alloc_create_types) {
    NEED_DEV();
    static const D3D12_COMMAND_LIST_TYPE ok[] = { D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_BUNDLE, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY };
    for (unsigned i = 0; i < 4; i++) {
        ID3D12CommandAllocator *a = NULL;
        EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, ok[i], &IID_ID3D12CommandAllocator, (void **)&a), S_OK);
        if (a) { EXPECT_HR(ID3D12CommandAllocator_Reset(a), S_OK); EXPECT_EQ(d3dh_refcount(a), 1); SAFE_RELEASE(a); }
    }
}
TEST(d3d12_objects_alloc_create_invalid_type) {
    NEED_DEV();
    static const int bad[] = { 7, 100, -1 };
    for (unsigned i = 0; i < 3; i++) {
        ID3D12CommandAllocator *a = NULL;
        HRESULT hr = ID3D12Device_CreateCommandAllocator(dev, (D3D12_COMMAND_LIST_TYPE)bad[i], &IID_ID3D12CommandAllocator, (void **)&a);
        CHECKF((hr) == E_INVALIDARG, "type %d: %s", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(a);
    }
}
TEST(d3d12_objects_alloc_reset_idle) {
    NEED_DEV();
    ID3D12CommandAllocator *a = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a), S_OK);
    if (!a) return;
    for (int i = 0; i < 3; i++) EXPECT_HR(ID3D12CommandAllocator_Reset(a), S_OK);
    SAFE_RELEASE(a);
}
TEST(d3d12_objects_alloc_reset_while_list_recording) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    HRESULT hr = ID3D12CommandAllocator_Reset(c.al);   /* the list is still recording */
    EXPECT_HR(hr, E_FAIL);
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
    EXPECT_HR(ID3D12CommandAllocator_Reset(c.al), S_OK);   /* closed list: allocator is free again */
    d12_ctx_free(&c);
}
TEST(d3d12_objects_alloc_reset_after_execute) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    for (int i = 0; i < 5; i++) {
        EXPECT_HR(d12_run(&c), S_OK);
        EXPECT_HR(d12_rerecord(&c), S_OK);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_alloc_getdevice_and_iids) {
    NEED_DEV();
    ID3D12CommandAllocator *a = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_COPY, &IID_ID3D12CommandAllocator, (void **)&a), S_OK);
    if (!a) return;
    ID3D12Device *d = NULL; void *o;
    EXPECT_HR(ID3D12CommandAllocator_GetDevice(a, &IID_ID3D12Device, (void **)&d), S_OK);
    CHECK(d == dev); if (d) ID3D12Device_Release(d);
    o = NULL; EXPECT_HR(ID3D12CommandAllocator_QueryInterface(a, &IID_ID3D12Pageable, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = (void *)1; EXPECT_HR(ID3D12CommandAllocator_QueryInterface(a, &IID_ID3D12CommandQueue, &o), E_NOINTERFACE); CHECK(o == NULL);
    SAFE_RELEASE(a);
}

/* ====== fences ========================================================================= */
TEST(d3d12_objects_fence_create_initial_values) {
    NEED_DEV();
    static const UINT64 init[] = { 0, 1, 7, 0xFFFFFFFFull, 0x100000000ull, 0x7FFFFFFFFFFFFFFFull };
    for (unsigned i = 0; i < 6; i++) {
        ID3D12Fence *f = NULL;
        EXPECT_HR(ID3D12Device_CreateFence(dev, init[i], D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
        if (f) { EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), init[i]); SAFE_RELEASE(f); }
    }
}
TEST(d3d12_objects_fence_flags) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    HRESULT shr = ID3D12Device_CreateFence(dev, 3, D3D12_FENCE_FLAG_SHARED, &IID_ID3D12Fence, (void **)&f);
    CHECKF(shr == S_OK, "SHARED fence: %s", d3dh_hr_name(shr));
    if (f) { EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 3); SAFE_RELEASE(f); }
    HRESULT hr = ID3D12Device_CreateFence(dev, 3, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER, &IID_ID3D12Fence, (void **)&f);
    CHECKF(hr == S_OK, "SHARED|SHARED_CROSS_ADAPTER fence: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(f);
    hr = ID3D12Device_CreateFence(dev, 3, D3D12_FENCE_FLAG_NON_MONITORED, &IID_ID3D12Fence, (void **)&f);
    CHECKF(hr == E_INVALIDARG, "NON_MONITORED fence: %s", d3dh_hr_name(hr));
    if (f) EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 3);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_create_ppv_null_and_bad_iid) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    HRESULT hr = ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Resource, (void **)&f);
    CHECKF((hr) == E_NOINTERFACE, "riid=Resource: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { SAFE_RELEASE(f); }
    f = NULL;
    hr = ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f);
    EXPECT_HR(hr, S_OK);
    /* interface request: an older base interface can be requested */
    ID3D12Object *o = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Object, (void **)&o), S_OK);
    SAFE_RELEASE(o); SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_cpu_signal_increasing) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    if (!f) return;
    UINT64 vals[] = { 1, 2, 100, 0x100000000ull, 0x100000001ull };
    for (unsigned i = 0; i < 5; i++) {
        EXPECT_HR(ID3D12Fence_Signal(f, vals[i]), S_OK);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), vals[i]);
    }
    EXPECT_HR(ID3D12Fence_Signal(f, 0x100000001ull), S_OK);   /* same value again */
    EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 0x100000001ull);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_cpu_signal_decreasing) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 10, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    if (!f) return;
    HRESULT hr = ID3D12Fence_Signal(f, 5);
    /* Win10 22H2: a CPU Signal may move the fence value backwards */
    CHECKF(hr == S_OK, "CPU Signal(5) after value 10: %s", d3dh_hr_name(hr));
    EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 5);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_event_immediate_when_complete) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 5, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (f && ev) {
        static const UINT64 want[] = { 0, 1, 4, 5 };
        for (unsigned i = 0; i < 4; i++) {
            ResetEvent(ev);
            EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, want[i], ev), S_OK);
            EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_OBJECT_0);
        }
    }
    if (ev) CloseHandle(ev);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_event_zero_value_fresh_fence) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (f && ev) {
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 0, ev), S_OK);
        EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_OBJECT_0);
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 0, NULL), S_OK);   /* NULL event = block; already complete returns at once */
    }
    if (ev) CloseHandle(ev);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_event_fires_on_signal) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (f && ev) {
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 3, ev), S_OK);
        EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_TIMEOUT);
        EXPECT_HR(ID3D12Fence_Signal(f, 2), S_OK);
        EXPECT_EQ(WaitForSingleObject(ev, 20), WAIT_TIMEOUT);
        EXPECT_HR(ID3D12Fence_Signal(f, 3), S_OK);
        EXPECT_EQ(WaitForSingleObject(ev, 2000), WAIT_OBJECT_0);
    }
    if (ev) CloseHandle(ev);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_event_signal_jumps_past_value) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (f && ev) {
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 3, ev), S_OK);
        EXPECT_HR(ID3D12Fence_Signal(f, 50), S_OK);
        EXPECT_EQ(WaitForSingleObject(ev, 2000), WAIT_OBJECT_0);
    }
    if (ev) CloseHandle(ev);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_multiple_events_ordering) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    HANDLE e[3];
    for (int i = 0; i < 3; i++) e[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (f) {
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 3, e[2]), S_OK);
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 1, e[0]), S_OK);
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 2, e[1]), S_OK);
        EXPECT_HR(ID3D12Fence_Signal(f, 1), S_OK);
        EXPECT_EQ(WaitForSingleObject(e[0], 2000), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(e[1], 0), WAIT_TIMEOUT);
        EXPECT_EQ(WaitForSingleObject(e[2], 0), WAIT_TIMEOUT);
        EXPECT_HR(ID3D12Fence_Signal(f, 2), S_OK);
        EXPECT_EQ(WaitForSingleObject(e[1], 2000), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(e[2], 0), WAIT_TIMEOUT);
        EXPECT_HR(ID3D12Fence_Signal(f, 3), S_OK);
        EXPECT_EQ(WaitForSingleObject(e[2], 2000), WAIT_OBJECT_0);
    }
    for (int i = 0; i < 3; i++) if (e[i]) CloseHandle(e[i]);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_two_events_same_value) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL), b = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (f) {
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 1, a), S_OK);
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 1, b), S_OK);
        EXPECT_HR(ID3D12Fence_Signal(f, 1), S_OK);
        EXPECT_EQ(WaitForSingleObject(a, 2000), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(b, 2000), WAIT_OBJECT_0);
    }
    CloseHandle(a); CloseHandle(b);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_event_autoreset_consumed) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 1, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);   /* auto-reset */
    if (f) {
        EXPECT_HR(ID3D12Fence_SetEventOnCompletion(f, 1, ev), S_OK);
        EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_TIMEOUT);
    }
    CloseHandle(ev);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_event_signalled_by_gpu) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_HR(ID3D12Fence_SetEventOnCompletion(c.fence, 9, ev), S_OK);
    EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, 9), S_OK);
    EXPECT_EQ(WaitForSingleObject(ev, 10000), WAIT_OBJECT_0);
    EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 9);
    CloseHandle(ev);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_fence_completed_value_only_after_prior_work) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    /* A gate holds back a Signal; completed value must stay at the old value until the queue is released. */
    ID3D12Fence *gate = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&gate), S_OK);
    if (gate) {
        ID3D12CommandQueue_Wait(c.q, gate, 1);
        ID3D12CommandQueue_Signal(c.q, c.fence, 5);
        Sleep(30);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 0);
        ID3D12Fence_Signal(gate, 1);
        EXPECT_HR(d12_wait(&c, 5), S_OK);
        EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 5);
        SAFE_RELEASE(gate);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_fence_getdevice_iids) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    if (!f) return;
    ID3D12Device *d = NULL; void *o;
    EXPECT_HR(ID3D12Fence_GetDevice(f, &IID_ID3D12Device, (void **)&d), S_OK);
    CHECK(d == dev); if (d) ID3D12Device_Release(d);
    o = NULL; EXPECT_HR(ID3D12Fence_QueryInterface(f, &IID_ID3D12Pageable, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = NULL; EXPECT_HR(ID3D12Fence_QueryInterface(f, &IID_ID3D12DeviceChild, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = (void *)1; EXPECT_HR(ID3D12Fence_QueryInterface(f, &IID_ID3D12CommandQueue, &o), E_NOINTERFACE); CHECK(o == NULL);
    ID3D12Fence1 *f1 = NULL;
    HRESULT hr = ID3D12Fence_QueryInterface(f, &IID_ID3D12Fence1, (void **)&f1);
    EXPECT_HR(hr, S_OK);
    if (f1) { D3D12_FENCE_FLAGS fl = ID3D12Fence1_GetCreationFlags(f1); EXPECT_EQ(fl, D3D12_FENCE_FLAG_NONE); ID3D12Fence1_Release(f1); }
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_creation_flags_roundtrip) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    if (FAILED(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_SHARED, &IID_ID3D12Fence, (void **)&f)) || !f) { t_skip("shared fence unsupported"); return; }
    ID3D12Fence1 *f1 = NULL;
    if (FAILED(ID3D12Fence_QueryInterface(f, &IID_ID3D12Fence1, (void **)&f1)) || !f1) { t_skip("no ID3D12Fence1"); SAFE_RELEASE(f); return; }
    EXPECT_EQ(ID3D12Fence1_GetCreationFlags(f1), D3D12_FENCE_FLAG_SHARED);
    ID3D12Fence1_Release(f1); SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_gpu_signal_cpu_event_many) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    for (UINT64 v = 1; v <= 50; v++) {
        EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, v), S_OK);
    }
    EXPECT_HR(d12_wait(&c, 50), S_OK);
    EXPECT_EQ(ID3D12Fence_GetCompletedValue(c.fence), 50);
    d12_ctx_free(&c);
}

/* ---- shared fences ---------------------------------------------------------------------------- */
TEST(d3d12_objects_fence_shared_handle_roundtrip) {
    NEED_DEV();
    if (t_is_wine()) { t_skip("shared handles not validated on this emulator"); return; }
    ID3D12Fence *f = NULL;
    if (FAILED(ID3D12Device_CreateFence(dev, 4, D3D12_FENCE_FLAG_SHARED, &IID_ID3D12Fence, (void **)&f)) || !f) { t_skip("shared fences unsupported"); return; }
    HANDLE h = NULL;
    HRESULT hr = ID3D12Device_CreateSharedHandle(dev, (ID3D12DeviceChild *)f, NULL, GENERIC_ALL, NULL, &h);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && h) {
        ID3D12Fence *g = NULL;
        EXPECT_HR(ID3D12Device_OpenSharedHandle(dev, h, &IID_ID3D12Fence, (void **)&g), S_OK);
        if (g) {
            EXPECT_EQ(ID3D12Fence_GetCompletedValue(g), 4);
            EXPECT_HR(ID3D12Fence_Signal(f, 6), S_OK);
            EXPECT_EQ(ID3D12Fence_GetCompletedValue(g), 6);
            EXPECT_HR(ID3D12Fence_Signal(g, 8), S_OK);
            EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 8);
            CHECK(g != f);
            SAFE_RELEASE(g);
        }
        CloseHandle(h);
    }
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_shared_handle_unshared_fence) {
    NEED_DEV();
    if (t_is_wine()) { t_skip("shared handles not validated on this emulator"); return; }
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    if (!f) return;
    HANDLE h = NULL;
    HRESULT hr = ID3D12Device_CreateSharedHandle(dev, (ID3D12DeviceChild *)f, NULL, GENERIC_ALL, NULL, &h);
    CHECKF((hr) == E_INVALIDARG, "CreateSharedHandle on a non-shared fence: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && h) CloseHandle(h);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_shared_handle_named) {
    NEED_DEV();
    if (t_is_wine()) { t_skip("shared handles not validated on this emulator"); return; }
    ID3D12Fence *f = NULL;
    if (FAILED(ID3D12Device_CreateFence(dev, 2, D3D12_FENCE_FLAG_SHARED, &IID_ID3D12Fence, (void **)&f)) || !f) { t_skip("no shared fence"); return; }
    WCHAR name[64]; wsprintfW(name, L"d3dt_objfence_%lu", GetCurrentProcessId());
    HANDLE h = NULL, h2 = NULL;
    HRESULT hr = ID3D12Device_CreateSharedHandle(dev, (ID3D12DeviceChild *)f, NULL, GENERIC_ALL, name, &h);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        hr = ID3D12Device_OpenSharedHandleByName(dev, name, GENERIC_ALL, &h2);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr) && h2) {
            ID3D12Fence *g = NULL;
            EXPECT_HR(ID3D12Device_OpenSharedHandle(dev, h2, &IID_ID3D12Fence, (void **)&g), S_OK);
            if (g) { EXPECT_EQ(ID3D12Fence_GetCompletedValue(g), 2); SAFE_RELEASE(g); }
            CloseHandle(h2);
        }
        WCHAR bad[64]; wsprintfW(bad, L"d3dt_objfence_missing_%lu", GetCurrentProcessId());
        HANDLE h3 = NULL;
        hr = ID3D12Device_OpenSharedHandleByName(dev, bad, GENERIC_ALL, &h3);
        CHECKF(hr == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE), "unknown name: %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && h3) CloseHandle(h3);
        CloseHandle(h);
    }
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_fence_open_shared_wrong_iid) {
    NEED_DEV();
    if (t_is_wine()) { t_skip("shared handles not validated on this emulator"); return; }
    ID3D12Fence *f = NULL; HANDLE h = NULL;
    if (FAILED(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_SHARED, &IID_ID3D12Fence, (void **)&f)) || !f) { t_skip("no shared fence"); return; }
    if (SUCCEEDED(ID3D12Device_CreateSharedHandle(dev, (ID3D12DeviceChild *)f, NULL, GENERIC_ALL, NULL, &h)) && h) {
        void *o = (void *)1;
        HRESULT hr = ID3D12Device_OpenSharedHandle(dev, h, &IID_ID3D12Resource, &o);
        CHECKF((hr) == E_NOINTERFACE, "fence handle opened as resource: %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && o) ((IUnknown *)o)->lpVtbl->Release(o);
        CloseHandle(h);
    }
    SAFE_RELEASE(f);
}
CHILD(d12_fence_open) {
    /* argv[1] = fence name (ascii). Opens it, verifies the value 7 and signals 9. Exit 0 = ok, 100 = skipped. */
    ID3D12Device *dv = NULL;
    if (!argc || FAILED(d3dh_make_device12(&dv, D3D_FEATURE_LEVEL_11_0, NULL))) return 100;
    WCHAR name[64]; MultiByteToWideChar(CP_ACP, 0, argv[1], -1, name, 64);
    HANDLE h = NULL; int rc = 1;
    if (SUCCEEDED(ID3D12Device_OpenSharedHandleByName(dv, name, GENERIC_ALL, &h)) && h) {
        ID3D12Fence *f = NULL;
        if (SUCCEEDED(ID3D12Device_OpenSharedHandle(dv, h, &IID_ID3D12Fence, (void **)&f)) && f) {
            if (ID3D12Fence_GetCompletedValue(f) == 7) { ID3D12Fence_Signal(f, 9); rc = 0; } else rc = 2;
            ID3D12Fence_Release(f);
        } else rc = 3;
        CloseHandle(h);
    } else rc = 4;
    ID3D12Device_Release(dv);
    return rc;
}
TEST(d3d12_objects_fence_shared_cross_process) {
    NEED_DEV();
    if (t_is_wine()) { t_skip("shared handles not validated on this emulator"); return; }
    ID3D12Fence *f = NULL;
    if (FAILED(ID3D12Device_CreateFence(dev, 7, D3D12_FENCE_FLAG_SHARED, &IID_ID3D12Fence, (void **)&f)) || !f) { t_skip("no shared fence"); return; }
    char an[64]; WCHAR wn[64]; HANDLE h = NULL;
    wsprintfA(an, "d3dt_xproc_fence_%lu", GetCurrentProcessId()); wsprintfW(wn, L"d3dt_xproc_fence_%lu", GetCurrentProcessId());
    if (SUCCEEDED(ID3D12Device_CreateSharedHandle(dev, (ID3D12DeviceChild *)f, NULL, GENERIC_ALL, wn, &h)) && h) {
        DWORD rc = t_run_child("d12_fence_open", an, 60000);
        if (rc == 100) t_skip("child could not create a device");
        else {
            EXPECT_EQ(rc, 0);
            EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 9);
        }
        CloseHandle(h);
    } else t_skip("CreateSharedHandle with a name failed");
    SAFE_RELEASE(f);
}

/* ====== ID3D12Object protocol shared by all child objects ========================================== */
static const GUID g_pd_a = { 0xd3d70001, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
static const GUID g_pd_b = { 0xd3d70002, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };

static void object_protocol(ID3D12Device *dev, ID3D12Object *o, const char *what) {
    HRESULT hr; UINT sz; char buf[32]; static const char payload[8] = "abcdefg";
    hr = ID3D12Object_SetPrivateData(o, &g_pd_a, 8, payload);
    CHECKF(hr == S_OK, "%s SetPrivateData: %s", what, d3dh_hr_name(hr));
    sz = 0; hr = ID3D12Object_GetPrivateData(o, &g_pd_a, &sz, NULL);
    CHECKF(hr == S_OK && sz == 8, "%s size query: %s sz=%u", what, d3dh_hr_name(hr), sz);
    sz = 4; memset(buf, 0, sizeof buf); hr = ID3D12Object_GetPrivateData(o, &g_pd_a, &sz, buf);
    CHECKF(hr == DXGI_ERROR_MORE_DATA && sz == 8, "%s small buffer: %s sz=%u", what, d3dh_hr_name(hr), sz);
    sz = 32; memset(buf, 0, sizeof buf); hr = ID3D12Object_GetPrivateData(o, &g_pd_a, &sz, buf);
    CHECKF(hr == S_OK && sz == 8 && !memcmp(buf, payload, 8), "%s read back: %s sz=%u", what, d3dh_hr_name(hr), sz);
    sz = 99; hr = ID3D12Object_GetPrivateData(o, &g_pd_b, &sz, buf);
    CHECKF(hr == DXGI_ERROR_NOT_FOUND && sz == 0, "%s unknown guid: %s sz=%u", what, d3dh_hr_name(hr), sz);
    hr = ID3D12Object_SetPrivateData(o, &g_pd_a, 3, "xyz");
    sz = 32; hr = ID3D12Object_GetPrivateData(o, &g_pd_a, &sz, buf);
    CHECKF(hr == S_OK && sz == 3 && !memcmp(buf, "xyz", 3), "%s replace: %s sz=%u", what, d3dh_hr_name(hr), sz);
    hr = ID3D12Object_SetPrivateData(o, &g_pd_a, 0, NULL);
    CHECKF(hr == S_OK, "%s clear: %s", what, d3dh_hr_name(hr));
    sz = 32; hr = ID3D12Object_GetPrivateData(o, &g_pd_a, &sz, buf);
    CHECKF(hr == DXGI_ERROR_NOT_FOUND, "%s after clear: %s", what, d3dh_hr_name(hr));
    /* interface payload */
    ID3D12Fence *f = NULL;
    if (SUCCEEDED(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f)) && f) {
        ULONG r0 = d3dh_refcount(f);
        hr = ID3D12Object_SetPrivateDataInterface(o, &g_pd_b, (IUnknown *)f);
        CHECKF(hr == S_OK, "%s SetPrivateDataInterface: %s", what, d3dh_hr_name(hr));
        CHECKF(d3dh_refcount(f) == r0 + 1, "%s interface payload addref %lu->%lu", what, r0, d3dh_refcount(f));
        IUnknown *got = NULL; sz = sizeof got;
        hr = ID3D12Object_GetPrivateData(o, &g_pd_b, &sz, &got);
        CHECKF(hr == S_OK && sz == sizeof(void *) && got == (IUnknown *)f, "%s interface read: %s sz=%u", what, d3dh_hr_name(hr), sz);
        if (got) got->lpVtbl->Release(got);   /* GetPrivateData AddRefs the interface */
        hr = ID3D12Object_SetPrivateDataInterface(o, &g_pd_b, NULL);
        CHECKF(hr == S_OK, "%s clear iface: %s", what, d3dh_hr_name(hr));
        CHECKF(d3dh_refcount(f) == r0, "%s interface payload released %lu->%lu", what, r0, d3dh_refcount(f));
        SAFE_RELEASE(f);
    }
    hr = ID3D12Object_SetName(o, L"d3dt object");
    CHECKF(hr == S_OK, "%s SetName: %s", what, d3dh_hr_name(hr));
    hr = ID3D12Object_SetName(o, L"");
    CHECKF(hr == S_OK, "%s SetName empty: %s", what, d3dh_hr_name(hr));
}
TEST(d3d12_objects_private_data_queue) {
    NEED_DEV();
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0, sizeof d); ID3D12CommandQueue *q = NULL;
    if (FAILED(ID3D12Device_CreateCommandQueue(dev, &d, &IID_ID3D12CommandQueue, (void **)&q))) return;
    object_protocol(dev, (ID3D12Object *)q, "queue"); SAFE_RELEASE(q);
}
TEST(d3d12_objects_private_data_allocator) {
    NEED_DEV();
    ID3D12CommandAllocator *a = NULL;
    if (FAILED(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a))) return;
    object_protocol(dev, (ID3D12Object *)a, "allocator"); SAFE_RELEASE(a);
}
TEST(d3d12_objects_private_data_list) {
    NEED_DEV();
    d12ctx c = {0}; if (SUCCEEDED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) object_protocol(dev, (ID3D12Object *)c.cl, "list");
    d12_ctx_free(&c);
}
TEST(d3d12_objects_private_data_fence) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    if (FAILED(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f))) return;
    object_protocol(dev, (ID3D12Object *)f, "fence"); SAFE_RELEASE(f);
}
TEST(d3d12_objects_private_data_resource) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    if (FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_NONE, &r))) return;
    object_protocol(dev, (ID3D12Object *)r, "resource"); SAFE_RELEASE(r);
}
TEST(d3d12_objects_private_data_descriptor_heap) {
    NEED_DEV();
    ID3D12DescriptorHeap *h = NULL;
    if (FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, 0, &h))) return;
    object_protocol(dev, (ID3D12Object *)h, "descriptor heap"); SAFE_RELEASE(h);
}
TEST(d3d12_objects_private_data_heap) {
    NEED_DEV();
    D3D12_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.SizeInBytes = 64 * KB; hd.Properties = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    hd.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS; ID3D12Heap *h = NULL;
    if (FAILED(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h))) { t_skip("no heap"); return; }
    object_protocol(dev, (ID3D12Object *)h, "heap"); SAFE_RELEASE(h);
}
TEST(d3d12_objects_private_data_rootsig) {
    NEED_DEV();
    ID3D12RootSignature *rs = NULL;
    if (FAILED(d12_make_rootsig(dev, 0, NULL, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE, &rs))) { t_skip("no root signature"); return; }
    object_protocol(dev, (ID3D12Object *)rs, "root signature"); SAFE_RELEASE(rs);
}
TEST(d3d12_objects_private_data_device) {
    NEED_DEV();
    object_protocol(dev, (ID3D12Object *)dev, "device");
}
TEST(d3d12_objects_private_data_independent_between_objects) {
    NEED_DEV();
    ID3D12Fence *a = NULL, *b = NULL;
    ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&a);
    ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&b);
    if (a && b) {
        UINT sz = 4;
        EXPECT_HR(ID3D12Fence_SetPrivateData(a, &g_pd_a, 4, "abcd"), S_OK);
        char buf[8];
        EXPECT_HR(ID3D12Fence_GetPrivateData(b, &g_pd_a, &sz, buf), DXGI_ERROR_NOT_FOUND);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
TEST(d3d12_objects_private_data_released_with_object) {
    NEED_DEV();
    ID3D12Fence *f = NULL, *holder = NULL;
    ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f);
    ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&holder);
    if (f && holder) {
        ULONG r0 = d3dh_refcount(holder);
        EXPECT_HR(ID3D12Fence_SetPrivateDataInterface(f, &g_pd_a, (IUnknown *)holder), S_OK);
        EXPECT_EQ(d3dh_refcount(holder), r0 + 1);
        SAFE_RELEASE(f);   /* destroying the owner drops the stored reference */
        EXPECT_EQ(d3dh_refcount(holder), r0);
    }
    SAFE_RELEASE(f); SAFE_RELEASE(holder);
}
TEST(d3d12_objects_setname_wellknown_guid) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f);
    if (!f) return;
    static const GUID WKPDID_NameW = { 0x4cca5fd8, 0x921f, 0x42c8, { 0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41 } };
    EXPECT_HR(ID3D12Fence_SetName(f, L"nm"), S_OK);
    UINT sz = 0; HRESULT hr = ID3D12Fence_GetPrivateData(f, &WKPDID_NameW, &sz, NULL);
    CHECKF(hr == S_OK && sz == 6, "name private data after SetName: %s size=%u", d3dh_hr_name(hr), sz);   /* "nm" + NUL as WCHARs */
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_refcount_addref_release) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f);
    if (!f) return;
    EXPECT_EQ(ID3D12Fence_AddRef(f), 2);
    EXPECT_EQ(ID3D12Fence_AddRef(f), 3);
    EXPECT_EQ(ID3D12Fence_Release(f), 2);
    ID3D12Object *o = NULL;
    EXPECT_HR(ID3D12Fence_QueryInterface(f, &IID_ID3D12Object, (void **)&o), S_OK);
    EXPECT_EQ(d3dh_refcount(f), 3);
    if (o) ID3D12Object_Release(o);
    EXPECT_EQ(ID3D12Fence_Release(f), 1);
    EXPECT_EQ(ID3D12Fence_Release(f), 0);
}
TEST(d3d12_objects_qi_identity) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f);
    if (!f) return;
    IUnknown *u1 = NULL, *u2 = NULL; ID3D12Object *o = NULL;
    EXPECT_HR(ID3D12Fence_QueryInterface(f, &IID_IUnknown, (void **)&u1), S_OK);
    EXPECT_HR(ID3D12Fence_QueryInterface(f, &IID_ID3D12Object, (void **)&o), S_OK);
    if (o) EXPECT_HR(ID3D12Object_QueryInterface(o, &IID_IUnknown, (void **)&u2), S_OK);
    CHECK(u1 == u2);
    /* Measured on Win10 22H2: the IUnknown identity pointer is a separate sub-object, never the interface pointer itself
     * (fence, resource and device alike), while ID3D12Object/ID3D12DeviceChild/ID3D12Fence1 share the object pointer. */
    CHECK(u1 != (IUnknown *)f);
    { ID3D12DeviceChild *dc = NULL; if (SUCCEEDED(ID3D12Fence_QueryInterface(f, &IID_ID3D12DeviceChild, (void **)&dc)) && dc) { CHECK((void *)dc == (void *)f); CHECK((void *)o == (void *)f); ID3D12DeviceChild_Release(dc); } }
    if (u1) u1->lpVtbl->Release(u1);
    if (u2) u2->lpVtbl->Release(u2);
    if (o) ID3D12Object_Release(o);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_device_removed_reason_ok) {
    NEED_DEV();
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(dev), S_OK);
}
