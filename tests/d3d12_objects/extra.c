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
