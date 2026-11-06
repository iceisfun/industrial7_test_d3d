#include "d12o.h"

TEST(d3d12_objects_extra_increment_sizes_stable) {
    NEED_DEV();
    for (int t = 0; t < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; t++) {
        UINT a = ID3D12Device_GetDescriptorHandleIncrementSize(dev, (D3D12_DESCRIPTOR_HEAP_TYPE)t);
        UINT b = ID3D12Device_GetDescriptorHandleIncrementSize(dev, (D3D12_DESCRIPTOR_HEAP_TYPE)t);
        CHECKF(a != 0 && a == b, "type %d: %u / %u", t, a, b);
    }
}
TEST(d3d12_objects_extra_buffer_gpu_va_distinct) {
    NEED_DEV();
    ID3D12Resource *a = NULL, *b = NULL;
    if (FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, 0, &a)) || FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, 0, &b))) { t_skip("no buffers"); SAFE_RELEASE(a); SAFE_RELEASE(b); return; }
    D3D12_GPU_VIRTUAL_ADDRESS va = ID3D12Resource_GetGPUVirtualAddress(a), vb = ID3D12Resource_GetGPUVirtualAddress(b);
    CHECK(va != 0 && vb != 0 && va != vb);
    CHECKF(va % 256 == 0 && vb % 256 == 0, "va alignment %llx %llx", (unsigned long long)va, (unsigned long long)vb);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
TEST(d3d12_objects_extra_upload_map_roundtrip) {
    NEED_DEV();
    ID3D12Resource *r = NULL; unsigned char *p = NULL, *p2 = NULL;
    if (FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, 4096, 0, &r))) { t_skip("no buffer"); return; }
    EXPECT_HR(ID3D12Resource_Map(r, 0, NULL, (void **)&p), S_OK);
    if (p) {
        for (int i = 0; i < 4096; i++) p[i] = (unsigned char)(i * 7);
        /* nested Map returns the same pointer */
        EXPECT_HR(ID3D12Resource_Map(r, 0, NULL, (void **)&p2), S_OK);
        CHECK(p2 == p);
        ID3D12Resource_Unmap(r, 0, NULL); ID3D12Resource_Unmap(r, 0, NULL);
        EXPECT_HR(ID3D12Resource_Map(r, 0, NULL, (void **)&p2), S_OK);
        if (p2) { int bad = 0; for (int i = 0; i < 4096; i++) bad += p2[i] != (unsigned char)(i * 7); CHECKF(bad == 0, "%d bytes differ after remap", bad); ID3D12Resource_Unmap(r, 0, NULL); }
    }
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_extra_fence_initial_value) {
    NEED_DEV();
    static const UINT64 v[] = { 0, 1, 0x100000000ull, 0xFFFFFFFFFFFFFFF0ull };
    for (unsigned i = 0; i < 4; i++) {
        ID3D12Fence *f = NULL;
        EXPECT_HR(ID3D12Device_CreateFence(dev, v[i], D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
        if (f) { UINT64 got = ID3D12Fence_GetCompletedValue(f); CHECKF(got == v[i], "initial %llx got %llx", (unsigned long long)v[i], (unsigned long long)got); SAFE_RELEASE(f); }
    }
}
TEST(d3d12_objects_extra_fence_signal_backwards_allowed) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    if (FAILED(ID3D12Device_CreateFence(dev, 5, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f))) { t_skip("no fence"); return; }
    EXPECT_HR(ID3D12Fence_Signal(f, 9), S_OK);
    EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 9);
    EXPECT_HR(ID3D12Fence_Signal(f, 2), S_OK);   /* CPU signal may lower the value */
    EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 2);
    SAFE_RELEASE(f);
}
TEST(d3d12_objects_extra_empty_execute_then_signal) {
    NEED_DEV();
    d12ctx c = {0};
    if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no queue"); d12_ctx_free(&c); return; }
    ID3D12CommandQueue_ExecuteCommandLists(c.q, 0, NULL);
    EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, 1), S_OK);
    EXPECT_HR(d12_wait(&c, 1), S_OK);
    d12_ctx_free(&c);
}

/* ====== threading: independent queues / lists / fences used concurrently from several threads ======================= */
typedef struct { ID3D12Device *dev; int id; int rounds; volatile LONG bad; } thr_arg;
static DWORD WINAPI copy_thread(LPVOID p) {
    thr_arg *a = p; d12ctx c; ID3D12Resource *up = NULL, *dst = NULL, *rb = NULL;
    memset(&c, 0, sizeof c);
    if (FAILED(d12_ctx_init(&c, a->dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) || FAILED(d12_make_upload(a->dev, NULL, 0, 1024, &up)) ||
        FAILED(d12_make_buffer(a->dev, D3D12_HEAP_TYPE_DEFAULT, 1024, 0, &dst)) || FAILED(d12_make_buffer(a->dev, D3D12_HEAP_TYPE_READBACK, 1024, 0, &rb))) {
        InterlockedIncrement(&a->bad); SAFE_RELEASE(up); SAFE_RELEASE(dst); SAFE_RELEASE(rb); d12_ctx_free(&c); return 1;
    }
    for (int r = 0; r < a->rounds; r++) {
        unsigned char *m = NULL; D3D12_RANGE none = { 0, 0 };
        if (FAILED(ID3D12Resource_Map(up, 0, &none, (void **)&m)) || !m) { InterlockedIncrement(&a->bad); break; }
        for (int i = 0; i < 1024; i++) m[i] = (unsigned char)(a->id * 31 + r * 7 + i);
        ID3D12Resource_Unmap(up, 0, NULL);
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, dst, 0, up, 0, 1024);
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, rb, 0, dst, 0, 1024);
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
        if (FAILED(d12_run(&c))) { InterlockedIncrement(&a->bad); break; }
        unsigned char *o = NULL; D3D12_RANGE rr = { 0, 1024 };
        if (FAILED(ID3D12Resource_Map(rb, 0, &rr, (void **)&o)) || !o) { InterlockedIncrement(&a->bad); break; }
        for (int i = 0; i < 1024; i++) if (o[i] != (unsigned char)(a->id * 31 + r * 7 + i)) { InterlockedIncrement(&a->bad); break; }
        D3D12_RANGE wr = { 0, 0 }; ID3D12Resource_Unmap(rb, 0, &wr);
        if (FAILED(d12_rerecord(&c))) { InterlockedIncrement(&a->bad); break; }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(dst); SAFE_RELEASE(rb); d12_ctx_free(&c);
    return 0;
}
TEST(d3d12_objects_threads_independent_queues_copy_roundtrip) {
    NEED_DEV();
    enum { N = 6 };
    thr_arg a[N]; HANDLE h[N];
    for (int i = 0; i < N; i++) { a[i].dev = dev; a[i].id = i + 1; a[i].rounds = 25; a[i].bad = 0; h[i] = CreateThread(NULL, 0, copy_thread, &a[i], 0, NULL); }
    CHECK(WaitForMultipleObjects(N, h, TRUE, 120000) == WAIT_OBJECT_0);
    for (int i = 0; i < N; i++) { CloseHandle(h[i]); CHECKF(a[i].bad == 0, "thread %d saw %ld errors", i, (long)a[i].bad); }
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(dev), S_OK);
}

static DWORD WINAPI fence_waiter(LPVOID p) {
    struct { ID3D12Fence *f; UINT64 v; HANDLE ev; int ok; } *a = p;
    a->ok = SUCCEEDED(ID3D12Fence_SetEventOnCompletion(a->f, a->v, a->ev)) && WaitForSingleObject(a->ev, 20000) == WAIT_OBJECT_0 && ID3D12Fence_GetCompletedValue(a->f) >= a->v;
    return 0;
}
TEST(d3d12_objects_threads_fence_cpu_signal_wakes_waiters) {
    NEED_DEV();
    ID3D12Fence *f = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    if (!f) return;
    struct { ID3D12Fence *f; UINT64 v; HANDLE ev; int ok; } a[4]; HANDLE h[4];
    for (int i = 0; i < 4; i++) { a[i].f = f; a[i].v = (UINT64)(i + 1) * 10; a[i].ev = CreateEventA(NULL, FALSE, FALSE, NULL); a[i].ok = 0; h[i] = CreateThread(NULL, 0, fence_waiter, &a[i], 0, NULL); }
    Sleep(50);
    for (int v = 10; v <= 40; v += 10) { EXPECT_HR(ID3D12Fence_Signal(f, v), S_OK); Sleep(5); }
    CHECK(WaitForMultipleObjects(4, h, TRUE, 30000) == WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) { CHECKF(a[i].ok, "waiter %d for value %llu", i, (unsigned long long)a[i].v); CloseHandle(h[i]); CloseHandle(a[i].ev); }
    EXPECT_EQ(ID3D12Fence_GetCompletedValue(f), 40);
    SAFE_RELEASE(f);
}

TEST(d3d12_objects_device_removed_reason_stays_ok_after_failed_creates) {
    NEED_DEV();
    /* a stream of failing creations must not lose the device */
    D3D12_RESOURCE_DESC bad = d12_buf_desc(0, 0); D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    for (int i = 0; i < 50; i++) {
        ID3D12Resource *r = NULL;
        HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &bad, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
        CHECKF(hr == E_INVALIDARG && r == NULL, "iteration %d: %s", i, d3dh_hr_name(hr));
    }
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(dev), S_OK);
}
