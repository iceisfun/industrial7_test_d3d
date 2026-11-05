/* d3d12_objects: graphics command list lifecycle, barriers, bundles, copy-queue / compute-queue recording. */
#include "d12o.h"

static const D3D12_COMMAND_LIST_TYPE ltypes[] = { D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_BUNDLE, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY };
static const char *ltn[] = { "DIRECT", "BUNDLE", "COMPUTE", "COPY" };

TEST(d3d12_objects_list_create_matching_types) {
    NEED_DEV();
    for (unsigned i = 0; i < 4; i++) {
        ID3D12CommandAllocator *a = NULL; ID3D12GraphicsCommandList *l = NULL;
        EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, ltypes[i], &IID_ID3D12CommandAllocator, (void **)&a), S_OK);
        if (!a) continue;
        HRESULT hr = ID3D12Device_CreateCommandList(dev, 0, ltypes[i], a, NULL, &IID_ID3D12GraphicsCommandList, (void **)&l);
        CHECKF(hr == S_OK, "%s list: %s", ltn[i], d3dh_hr_name(hr));
        if (l) {
            EXPECT_EQ(ID3D12GraphicsCommandList_GetType(l), ltypes[i]);
            EXPECT_HR(ID3D12GraphicsCommandList_Close(l), S_OK);
        }
        SAFE_RELEASE(l); SAFE_RELEASE(a);
    }
}
TEST(d3d12_objects_list_create_type_mismatch_matrix) {
    NEED_DEV();
    for (unsigned i = 0; i < 4; i++) for (unsigned j = 0; j < 4; j++) {
        if (i == j) continue;
        ID3D12CommandAllocator *a = NULL; ID3D12GraphicsCommandList *l = (ID3D12GraphicsCommandList *)(ULONG_PTR)1;
        if (FAILED(ID3D12Device_CreateCommandAllocator(dev, ltypes[j], &IID_ID3D12CommandAllocator, (void **)&a))) continue;
        HRESULT hr = ID3D12Device_CreateCommandList(dev, 0, ltypes[i], a, NULL, &IID_ID3D12GraphicsCommandList, (void **)&l);
        CHECKF(hr == E_INVALIDARG, "list %s with %s allocator: %s", ltn[i], ltn[j], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(l); else CHECK(l == NULL);
        SAFE_RELEASE(a);
    }
}
TEST(d3d12_objects_list_create_null_allocator) {
    NEED_DEV();
    ID3D12GraphicsCommandList *l = NULL;
    HRESULT hr = ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, NULL, NULL, &IID_ID3D12GraphicsCommandList, (void **)&l);
    CHECKF(FAILED(hr), "NULL allocator: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(l);
}
TEST(d3d12_objects_list_create_invalid_type_and_node) {
    NEED_DEV();
    ID3D12CommandAllocator *a = NULL; ID3D12GraphicsCommandList *l = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a), S_OK);
    if (!a) return;
    HRESULT hr = ID3D12Device_CreateCommandList(dev, 0, (D3D12_COMMAND_LIST_TYPE)42, a, NULL, &IID_ID3D12GraphicsCommandList, (void **)&l);
    CHECKF(FAILED(hr), "type 42: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(l);
    hr = ID3D12Device_CreateCommandList(dev, 2, D3D12_COMMAND_LIST_TYPE_DIRECT, a, NULL, &IID_ID3D12GraphicsCommandList, (void **)&l);
    CHECKF(FAILED(hr), "node mask 2: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(l);
    SAFE_RELEASE(a);
}
TEST(d3d12_objects_list_create_allocator_busy) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12GraphicsCommandList *l2 = NULL;
    HRESULT hr = ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, c.al, NULL, &IID_ID3D12GraphicsCommandList, (void **)&l2);
    CHECKF(FAILED(hr), "allocator in use by a recording list: %s", d3dh_hr_name(hr));
    if (hr != E_INVALIDARG) t_note("busy allocator -> %s (E_INVALIDARG documented for Reset)", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(l2);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_create_ppv_null_validates) {
    NEED_DEV();
    ID3D12CommandAllocator *a = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a), S_OK);
    if (!a) return;
    HRESULT hr = ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, a, NULL, &IID_ID3D12GraphicsCommandList, NULL);
    t_note("CreateCommandList(ppv=NULL): %s", d3dh_hr_name(hr));
    /* either way the allocator must remain usable: no list was left recording */
    CHECK(SUCCEEDED(ID3D12CommandAllocator_Reset(a)) || hr == S_FALSE || hr == S_OK);
    SAFE_RELEASE(a);
}
TEST(d3d12_objects_list_create_with_initial_pso) {
    NEED_DEV();
    ID3DBlob *cs = d12_compile_or_null(d12_cs_iota_src, "cs_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    if (!cs || FAILED(d12_make_cs_rootsig(dev, &rs)) || FAILED(d12_make_cs_pso(dev, rs, cs, &pso))) { t_skip("cannot build compute PSO"); goto out; }
    {
        ID3D12CommandAllocator *a = NULL; ID3D12GraphicsCommandList *l = NULL;
        EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a), S_OK);
        EXPECT_HR(ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, a, pso, &IID_ID3D12GraphicsCommandList, (void **)&l), S_OK);
        if (l) EXPECT_HR(ID3D12GraphicsCommandList_Close(l), S_OK);
        SAFE_RELEASE(l); SAFE_RELEASE(a);
    }
out:
    SAFE_RELEASE(pso); SAFE_RELEASE(rs); if (cs) ID3D10Blob_Release(cs);
}
TEST(d3d12_objects_list_create_initial_pso_wrong_list_type) {
    NEED_DEV();
    /* a graphics PSO can't be the initial state of a COPY list: expect failure or tolerance, record observed */
    ID3DBlob *cs = d12_compile_or_null(d12_cs_iota_src, "cs_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    if (!cs || FAILED(d12_make_cs_rootsig(dev, &rs)) || FAILED(d12_make_cs_pso(dev, rs, cs, &pso))) { t_skip("cannot build compute PSO"); goto out; }
    {
        ID3D12CommandAllocator *a = NULL; ID3D12GraphicsCommandList *l = NULL;
        ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_COPY, &IID_ID3D12CommandAllocator, (void **)&a);
        if (a) {
            HRESULT hr = ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_COPY, a, pso, &IID_ID3D12GraphicsCommandList, (void **)&l);
            t_note("COPY list with initial compute PSO: %s", d3dh_hr_name(hr));
            SAFE_RELEASE(l); SAFE_RELEASE(a);
        }
    }
out:
    SAFE_RELEASE(pso); SAFE_RELEASE(rs); if (cs) ID3D10Blob_Release(cs);
}
TEST(d3d12_objects_list_close_twice) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), E_FAIL);
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), E_FAIL);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_close_twice_all_types) {
    NEED_DEV();
    for (unsigned i = 0; i < 4; i++) {
        ID3D12CommandAllocator *a = NULL; ID3D12GraphicsCommandList *l = NULL;
        if (FAILED(ID3D12Device_CreateCommandAllocator(dev, ltypes[i], &IID_ID3D12CommandAllocator, (void **)&a))) continue;
        if (SUCCEEDED(ID3D12Device_CreateCommandList(dev, 0, ltypes[i], a, NULL, &IID_ID3D12GraphicsCommandList, (void **)&l))) {
            HRESULT h1 = ID3D12GraphicsCommandList_Close(l), h2 = ID3D12GraphicsCommandList_Close(l);
            CHECKF(h1 == S_OK && h2 == E_FAIL, "%s: %s then %s", ltn[i], d3dh_hr_name(h1), d3dh_hr_name(h2));
        }
        SAFE_RELEASE(l); SAFE_RELEASE(a);
    }
}
TEST(d3d12_objects_list_reset_while_recording) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    EXPECT_HR(ID3D12GraphicsCommandList_Reset(c.cl, c.al, NULL), E_FAIL);
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);   /* still recording after the failed Reset */
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_reset_after_close) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    for (int i = 0; i < 4; i++) {
        EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
        EXPECT_HR(ID3D12CommandAllocator_Reset(c.al), S_OK);
        EXPECT_HR(ID3D12GraphicsCommandList_Reset(c.cl, c.al, NULL), S_OK);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_reset_without_allocator_reset) {
    NEED_DEV();
    /* Reset of a list may reuse an allocator without resetting it first (the allocator keeps growing) */
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
    EXPECT_HR(ID3D12GraphicsCommandList_Reset(c.cl, c.al, NULL), S_OK);
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_reset_other_allocator) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12CommandAllocator *a2 = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a2), S_OK);
    if (a2) {
        EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
        EXPECT_HR(ID3D12GraphicsCommandList_Reset(c.cl, a2, NULL), S_OK);
        EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
        /* the first allocator is free again */
        EXPECT_HR(ID3D12CommandAllocator_Reset(c.al), S_OK);
        SAFE_RELEASE(a2);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_reset_wrong_allocator_type) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12CommandAllocator *a2 = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_COMPUTE, &IID_ID3D12CommandAllocator, (void **)&a2), S_OK);
    if (a2) {
        EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
        EXPECT_HR(ID3D12GraphicsCommandList_Reset(c.cl, a2, NULL), E_INVALIDARG);
        SAFE_RELEASE(a2);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_reset_allocator_in_use_by_other_list) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12GraphicsCommandList *l2 = NULL; ID3D12CommandAllocator *a2 = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a2), S_OK);
    if (a2) {
        EXPECT_HR(ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, a2, NULL, &IID_ID3D12GraphicsCommandList, (void **)&l2), S_OK);
        if (l2) {
            EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
            EXPECT_HR(ID3D12GraphicsCommandList_Reset(c.cl, a2, NULL), E_INVALIDARG);   /* a2 is recording on l2 */
            ID3D12GraphicsCommandList_Close(l2);
        }
        SAFE_RELEASE(l2); SAFE_RELEASE(a2);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_reset_null_allocator) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
    HRESULT hr = ID3D12GraphicsCommandList_Reset(c.cl, NULL, NULL);
    CHECKF(FAILED(hr), "Reset(NULL allocator): %s", d3dh_hr_name(hr));
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_reset_while_executing) {
    NEED_DEV();
    /* ID3D12GraphicsCommandList::Reset may be called immediately after ExecuteCommandLists (with an allocator that is not in flight). */
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12CommandAllocator *a2 = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a2), S_OK);
    if (a2) {
        EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
        ID3D12CommandList *l = (ID3D12CommandList *)c.cl;
        ID3D12CommandQueue_ExecuteCommandLists(c.q, 1, &l);
        EXPECT_HR(ID3D12GraphicsCommandList_Reset(c.cl, a2, NULL), S_OK);
        EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
        EXPECT_HR(ID3D12CommandQueue_Signal(c.q, c.fence, 1), S_OK);
        EXPECT_HR(d12_wait(&c, 1), S_OK);
        SAFE_RELEASE(a2);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_execute_twice) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12Resource *up = NULL, *dst = NULL; static const unsigned char pat[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    EXPECT_HR(d12_make_upload(dev, pat, 16, 256, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_NONE, &dst), S_OK);
    if (up && dst) {
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, dst, 0, up, 0, 16);
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);   /* leaves the state as found: re-executable */
        EXPECT_HR(ID3D12GraphicsCommandList_Close(c.cl), S_OK);
        ID3D12CommandList *l = (ID3D12CommandList *)c.cl;
        for (int i = 1; i <= 2; i++) {
            ID3D12CommandQueue_ExecuteCommandLists(c.q, 1, &l);
            ID3D12CommandQueue_Signal(c.q, c.fence, i);
            EXPECT_HR(d12_wait(&c, i), S_OK);
        }
        void *data = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, dst, D3D12_RESOURCE_STATE_COMMON, &data, &sz), S_OK);
        if (data) { CHECK(!memcmp(data, pat, 16)); free(data); }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(dst);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_qi_interfaces) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    void *o; HRESULT hr;
    o = NULL; EXPECT_HR(ID3D12GraphicsCommandList_QueryInterface(c.cl, &IID_ID3D12CommandList, &o), S_OK); if (o) { CHECK(o == (void *)c.cl); ((IUnknown *)o)->lpVtbl->Release(o); }
    o = NULL; EXPECT_HR(ID3D12GraphicsCommandList_QueryInterface(c.cl, &IID_ID3D12DeviceChild, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = NULL; EXPECT_HR(ID3D12GraphicsCommandList_QueryInterface(c.cl, &IID_ID3D12Object, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = (void *)1; EXPECT_HR(ID3D12GraphicsCommandList_QueryInterface(c.cl, &IID_ID3D12Fence, &o), E_NOINTERFACE); CHECK(o == NULL);
    o = (void *)1; EXPECT_HR(ID3D12GraphicsCommandList_QueryInterface(c.cl, &IID_ID3D12CommandQueue, &o), E_NOINTERFACE); CHECK(o == NULL);
    static const struct { const GUID *g; const char *n; } ext[] = {
        { &IID_ID3D12GraphicsCommandList1, "List1" }, { &IID_ID3D12GraphicsCommandList2, "List2" }, { &IID_ID3D12GraphicsCommandList3, "List3" },
        { &IID_ID3D12GraphicsCommandList4, "List4" }, { &IID_ID3D12GraphicsCommandList5, "List5" }, { &IID_ID3D12GraphicsCommandList6, "List6" },
    };
    for (unsigned i = 0; i < 6; i++) {
        o = NULL; hr = ID3D12GraphicsCommandList_QueryInterface(c.cl, ext[i].g, &o);
        t_note("QI ID3D12Graphics%s: %s", ext[i].n, d3dh_hr_name(hr));
        if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    }
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_getdevice) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_COMPUTE))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12Device *d = NULL;
    EXPECT_HR(ID3D12GraphicsCommandList_GetDevice(c.cl, &IID_ID3D12Device, (void **)&d), S_OK);
    CHECK(d == dev); if (d) ID3D12Device_Release(d);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_create_closed_list1) {
    NEED_DEV();
    ID3D12Device4 *d4 = NULL;
    if (FAILED(ID3D12Device_QueryInterface(dev, &IID_ID3D12Device4, (void **)&d4)) || !d4) { t_skip("ID3D12Device4 unavailable"); return; }
    ID3D12GraphicsCommandList *l = NULL;
    HRESULT hr = ID3D12Device4_CreateCommandList1(d4, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, &IID_ID3D12GraphicsCommandList, (void **)&l);
    EXPECT_HR(hr, S_OK);
    if (l) {
        EXPECT_HR(ID3D12GraphicsCommandList_Close(l), E_FAIL);   /* created in the closed state */
        { HRESULT rh = ID3D12GraphicsCommandList_Reset(l, NULL, NULL); CHECKF(FAILED(rh), "Reset(NULL allocator): %s", d3dh_hr_name(rh)); }
        ID3D12CommandAllocator *a = NULL;
        ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&a);
        if (a) { EXPECT_HR(ID3D12GraphicsCommandList_Reset(l, a, NULL), S_OK); EXPECT_HR(ID3D12GraphicsCommandList_Close(l), S_OK); SAFE_RELEASE(a); }
        SAFE_RELEASE(l);
    }
    ID3D12Device4_Release(d4);
}

/* ====== recording commands & verifying results =============================================== */
static void fill_pattern(unsigned char *p, size_t n, unsigned seed) { for (size_t i = 0; i < n; i++) p[i] = (unsigned char)((i * 7 + seed) ^ (i >> 3)); }

TEST(d3d12_objects_copy_buffer_region_basic) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    unsigned char src[1024]; fill_pattern(src, sizeof src, 3);
    ID3D12Resource *up = NULL, *dst = NULL;
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, sizeof src, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 2048, D3D12_RESOURCE_FLAG_NONE, &dst), S_OK);
    if (up && dst) {
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, dst, 512, up, 128, 400);
        EXPECT_HR(d12_run(&c), S_OK);
        unsigned char *rb = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, dst, D3D12_RESOURCE_STATE_COPY_DEST, (void **)&rb, &sz), S_OK);
        if (rb) {
            CHECK(sz == 2048);
            CHECK(!memcmp(rb + 512, src + 128, 400));
            int zero = 1; for (int i = 0; i < 512; i++) if (rb[i]) zero = 0;
            CHECK(zero);   /* untouched head (buffers are zero-initialised) */
            zero = 1; for (int i = 912; i < 2048; i++) if (rb[i]) zero = 0;
            CHECK(zero);
            free(rb);
        }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(dst); d12_ctx_free(&c);
}
TEST(d3d12_objects_copy_buffer_implicit_promotion) {
    NEED_DEV();
    /* Buffers in COMMON are promoted to COPY_DEST / COPY_SOURCE on first use and decay back after ExecuteCommandLists. */
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    unsigned char src[256]; fill_pattern(src, sizeof src, 11);
    ID3D12Resource *up = NULL, *mid = NULL, *rb = NULL;
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, sizeof src, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_NONE, &mid), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, 256, D3D12_RESOURCE_FLAG_NONE, &rb), S_OK);
    if (up && mid && rb) {
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, mid, 0, up, 0, 256);   /* mid: COMMON -> COPY_DEST implicitly */
        EXPECT_HR(d12_run(&c), S_OK);
        EXPECT_HR(d12_rerecord(&c), S_OK);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, rb, 0, mid, 0, 256);   /* mid decayed to COMMON, promoted to COPY_SOURCE */
        EXPECT_HR(d12_run(&c), S_OK);
        void *p = NULL; D3D12_RANGE rr = { 0, 256 };
        EXPECT_HR(ID3D12Resource_Map(rb, 0, &rr, &p), S_OK);
        if (p) { CHECK(!memcmp(p, src, 256)); D3D12_RANGE wr = { 0, 0 }; ID3D12Resource_Unmap(rb, 0, &wr); }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(mid); SAFE_RELEASE(rb); d12_ctx_free(&c);
}
TEST(d3d12_objects_copy_resource_buffer) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    unsigned char src[4096]; fill_pattern(src, sizeof src, 5);
    ID3D12Resource *up = NULL, *dst = NULL;
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, sizeof src, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, sizeof src, D3D12_RESOURCE_FLAG_NONE, &dst), S_OK);
    if (up && dst) {
        ID3D12GraphicsCommandList_CopyResource(c.cl, dst, up);
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        EXPECT_HR(d12_run(&c), S_OK);
        void *rb = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, dst, D3D12_RESOURCE_STATE_COPY_SOURCE, &rb, &sz), S_OK);
        if (rb) { CHECK(sz == sizeof src && !memcmp(rb, src, sizeof src)); free(rb); }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(dst); d12_ctx_free(&c);
}
TEST(d3d12_objects_copy_on_copy_queue) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_COPY))) { t_skip("no copy queue"); d12_ctx_free(&c); return; }
    unsigned char src[512]; fill_pattern(src, sizeof src, 21);
    ID3D12Resource *up = NULL, *dst = NULL;
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, sizeof src, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, sizeof src, D3D12_RESOURCE_FLAG_NONE, &dst), S_OK);
    if (up && dst) {
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, dst, 0, up, 0, sizeof src);   /* COMMON -> COPY_DEST promotion on a copy queue */
        EXPECT_HR(d12_run(&c), S_OK);
        void *rb = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, dst, D3D12_RESOURCE_STATE_COMMON, &rb, &sz), S_OK);
        if (rb) { CHECK(!memcmp(rb, src, sizeof src)); free(rb); }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(dst); d12_ctx_free(&c);
}
TEST(d3d12_objects_copy_on_compute_queue) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_COMPUTE))) { t_skip("no compute queue"); d12_ctx_free(&c); return; }
    unsigned char src[512]; fill_pattern(src, sizeof src, 31);
    ID3D12Resource *up = NULL, *dst = NULL;
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, sizeof src, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, sizeof src, D3D12_RESOURCE_FLAG_NONE, &dst), S_OK);
    if (up && dst) {
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, dst, 0, up, 0, sizeof src);
        EXPECT_HR(d12_run(&c), S_OK);
        void *rb = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, dst, D3D12_RESOURCE_STATE_COMMON, &rb, &sz), S_OK);
        if (rb) { CHECK(!memcmp(rb, src, sizeof src)); free(rb); }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(dst); d12_ctx_free(&c);
}
TEST(d3d12_objects_copy_to_readback_directly) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    unsigned char src[128]; fill_pattern(src, sizeof src, 77);
    ID3D12Resource *up = NULL, *rb = NULL;
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, sizeof src, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, sizeof src, D3D12_RESOURCE_FLAG_NONE, &rb), S_OK);
    if (up && rb) {
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, rb, 0, up, 0, sizeof src);
        EXPECT_HR(d12_run(&c), S_OK);
        void *p = NULL; D3D12_RANGE rr = { 0, sizeof src };
        EXPECT_HR(ID3D12Resource_Map(rb, 0, &rr, &p), S_OK);
        if (p) { CHECK(!memcmp(p, src, sizeof src)); D3D12_RANGE wr = { 0, 0 }; ID3D12Resource_Unmap(rb, 0, &wr); }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(rb); d12_ctx_free(&c);
}

/* ---- barriers ---------------------------------------------------------------------------- */
TEST(d3d12_objects_barrier_transition_all_subresources) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12Resource *t = NULL;
    EXPECT_HR(d12_make_tex2d(dev, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 2, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, NULL, &t), S_OK);
    if (t) {
        d12_transition(c.cl, t, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        d12_transition(c.cl, t, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        d12_transition(c.cl, t, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        EXPECT_HR(d12_run(&c), S_OK);
    }
    SAFE_RELEASE(t); d12_ctx_free(&c);
}
TEST(d3d12_objects_barrier_transition_per_subresource) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12Resource *t = NULL;
    EXPECT_HR(d12_make_tex2d(dev, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 2, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, NULL, &t), S_OK);
    if (t) {
        for (UINT s = 0; s < 6; s++) d12_transition(c.cl, t, s, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
        for (UINT s = 0; s < 6; s += 2) d12_transition(c.cl, t, s, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        for (UINT s = 0; s < 6; s++) d12_transition(c.cl, t, s, (s % 2) ? D3D12_RESOURCE_STATE_COPY_SOURCE : D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        EXPECT_HR(d12_run(&c), S_OK);
    }
    SAFE_RELEASE(t); d12_ctx_free(&c);
}
TEST(d3d12_objects_barrier_batch_array) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12Resource *a = NULL, *b = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &a), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &b), S_OK);
    if (a && b) {
        D3D12_RESOURCE_BARRIER br[4]; memset(br, 0, sizeof br);
        br[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br[0].Transition.pResource = a; br[0].Transition.Subresource = 0;
        br[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; br[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        br[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br[1].Transition.pResource = b; br[1].Transition.Subresource = 0;
        br[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; br[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        br[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; br[2].UAV.pResource = a;
        br[3].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; br[3].UAV.pResource = NULL;   /* NULL = all UAV accesses */
        ID3D12GraphicsCommandList_ResourceBarrier(c.cl, 4, br);
        ID3D12GraphicsCommandList_ResourceBarrier(c.cl, 0, NULL);   /* zero barriers is a valid no-op */
        EXPECT_HR(d12_run(&c), S_OK);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); d12_ctx_free(&c);
}
TEST(d3d12_objects_barrier_split) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12Resource *t = NULL;
    EXPECT_HR(d12_make_tex2d(dev, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON, NULL, &t), S_OK);
    if (t) {
        D3D12_RESOURCE_BARRIER b; memset(&b, 0, sizeof b);
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = t; b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
        ID3D12GraphicsCommandList_ResourceBarrier(c.cl, 1, &b);
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
        ID3D12GraphicsCommandList_ResourceBarrier(c.cl, 1, &b);
        d12_transition(c.cl, t, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
        EXPECT_HR(d12_run(&c), S_OK);
    }
    SAFE_RELEASE(t); d12_ctx_free(&c);
}
TEST(d3d12_objects_barrier_aliasing) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    D3D12_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.SizeInBytes = 64 * KB; hd.Properties = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT); hd.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    ID3D12Heap *h = NULL; ID3D12Resource *a = NULL, *b = NULL;
    if (FAILED(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h))) { t_skip("no heap"); d12_ctx_free(&c); return; }
    D3D12_RESOURCE_DESC rd = d12_buf_desc(4096, D3D12_RESOURCE_FLAG_NONE);
    EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &rd, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&a), S_OK);
    EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &rd, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&b), S_OK);
    if (a && b) {
        unsigned char src[64]; fill_pattern(src, sizeof src, 9);
        ID3D12Resource *up = NULL;
        EXPECT_HR(d12_make_upload(dev, src, sizeof src, 256, &up), S_OK);
        D3D12_RESOURCE_BARRIER br; memset(&br, 0, sizeof br);
        br.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING; br.Aliasing.pResourceBefore = NULL; br.Aliasing.pResourceAfter = a;
        ID3D12GraphicsCommandList_ResourceBarrier(c.cl, 1, &br);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, a, 0, up, 0, 64);
        d12_transition(c.cl, a, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        br.Aliasing.pResourceBefore = a; br.Aliasing.pResourceAfter = b;
        ID3D12GraphicsCommandList_ResourceBarrier(c.cl, 1, &br);
        br.Aliasing.pResourceBefore = NULL; br.Aliasing.pResourceAfter = NULL;   /* NULL/NULL aliasing barrier is legal */
        ID3D12GraphicsCommandList_ResourceBarrier(c.cl, 1, &br);
        EXPECT_HR(d12_run(&c), S_OK);
        SAFE_RELEASE(up);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(h); d12_ctx_free(&c);
}
TEST(d3d12_objects_barrier_same_state_note) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12Resource *a = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_NONE, &a), S_OK);
    if (a) {
        d12_transition(c.cl, a, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);
        HRESULT hr = ID3D12GraphicsCommandList_Close(c.cl);
        t_note("Close after a COMMON->COMMON transition barrier: %s", d3dh_hr_name(hr));
    }
    SAFE_RELEASE(a); d12_ctx_free(&c);
}
TEST(d3d12_objects_barrier_on_copy_list) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_COPY))) { t_skip("no copy queue"); d12_ctx_free(&c); return; }
    ID3D12Resource *a = NULL, *up = NULL;
    unsigned char src[64]; fill_pattern(src, sizeof src, 5);
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, 64, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 64, D3D12_RESOURCE_FLAG_NONE, &a), S_OK);
    if (a && up) {
        d12_transition(c.cl, a, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, a, 0, up, 0, 64);
        d12_transition(c.cl, a, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        EXPECT_HR(d12_run(&c), S_OK);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(up); d12_ctx_free(&c);
}

/* ---- markers / misc recording calls ------------------------------------------------------------ */
TEST(d3d12_objects_list_markers_and_events) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    static const char ascii[] = "marker"; static const WCHAR wide[] = L"wide marker";
    ID3D12GraphicsCommandList_SetMarker(c.cl, 0, NULL, 0);
    ID3D12GraphicsCommandList_SetMarker(c.cl, 1, ascii, sizeof ascii);
    ID3D12GraphicsCommandList_BeginEvent(c.cl, 1, ascii, sizeof ascii);
    ID3D12GraphicsCommandList_BeginEvent(c.cl, 2, wide, sizeof wide);
    ID3D12GraphicsCommandList_EndEvent(c.cl);
    ID3D12GraphicsCommandList_EndEvent(c.cl);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_clearstate_and_state_calls) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    D3D12_VIEWPORT vp = { 0, 0, 16, 16, 0, 1 }; D3D12_RECT sc = { 0, 0, 16, 16 }; float bf[4] = { 1, 1, 1, 1 };
    ID3D12GraphicsCommandList_RSSetViewports(c.cl, 1, &vp);
    ID3D12GraphicsCommandList_RSSetScissorRects(c.cl, 1, &sc);
    ID3D12GraphicsCommandList_OMSetBlendFactor(c.cl, bf);
    ID3D12GraphicsCommandList_OMSetStencilRef(c.cl, 3);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(c.cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D12GraphicsCommandList_SetPredication(c.cl, NULL, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
    ID3D12GraphicsCommandList_ClearState(c.cl, NULL);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_ctx_free(&c);
}
TEST(d3d12_objects_list_set_descriptor_heaps) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12DescriptorHeap *hv = NULL, *hs = NULL;
    EXPECT_HR(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, 1, &hv), S_OK);
    EXPECT_HR(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 4, 1, &hs), S_OK);
    if (hv && hs) {
        ID3D12DescriptorHeap *hh[2] = { hv, hs };
        ID3D12GraphicsCommandList_SetDescriptorHeaps(c.cl, 2, hh);
        ID3D12GraphicsCommandList_SetDescriptorHeaps(c.cl, 1, &hv);
        ID3D12GraphicsCommandList_SetDescriptorHeaps(c.cl, 0, NULL);
        EXPECT_HR(d12_run(&c), S_OK);
    }
    SAFE_RELEASE(hv); SAFE_RELEASE(hs); d12_ctx_free(&c);
}

/* ---- bundles ---------------------------------------------------------------------------------- */
TEST(d3d12_objects_bundle_lifecycle) {
    NEED_DEV();
    ID3D12CommandAllocator *a = NULL; ID3D12GraphicsCommandList *b = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_BUNDLE, &IID_ID3D12CommandAllocator, (void **)&a), S_OK);
    if (!a) return;
    EXPECT_HR(ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_BUNDLE, a, NULL, &IID_ID3D12GraphicsCommandList, (void **)&b), S_OK);
    if (b) {
        EXPECT_EQ(ID3D12GraphicsCommandList_GetType(b), D3D12_COMMAND_LIST_TYPE_BUNDLE);
        EXPECT_HR(ID3D12GraphicsCommandList_Close(b), S_OK);
        EXPECT_HR(ID3D12GraphicsCommandList_Close(b), E_FAIL);
        EXPECT_HR(ID3D12CommandAllocator_Reset(a), S_OK);
        EXPECT_HR(ID3D12GraphicsCommandList_Reset(b, a, NULL), S_OK);
        EXPECT_HR(ID3D12GraphicsCommandList_Close(b), S_OK);
    }
    SAFE_RELEASE(b); SAFE_RELEASE(a);
}
TEST(d3d12_objects_bundle_execute_draw) {
    NEED_DEV();
    d12draw dr; d12ctx c = {0}; d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (!c.cl || FAILED(d12_draw_init(dev, &dr))) { t_skip("cannot build draw state"); d12_draw_free(&dr); d12_ctx_free(&c); return; }
    ID3D12CommandAllocator *ba = NULL; ID3D12GraphicsCommandList *bl = NULL;
    EXPECT_HR(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_BUNDLE, &IID_ID3D12CommandAllocator, (void **)&ba), S_OK);
    EXPECT_HR(ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_BUNDLE, ba, dr.pso, &IID_ID3D12GraphicsCommandList, (void **)&bl), S_OK);
    if (ba && bl) {
        ID3D12GraphicsCommandList_SetGraphicsRootSignature(bl, dr.rs);
        ID3D12GraphicsCommandList_IASetPrimitiveTopology(bl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D12GraphicsCommandList_DrawInstanced(bl, 3, 1, 0, 0);
        EXPECT_HR(ID3D12GraphicsCommandList_Close(bl), S_OK);
        d12_draw_setup(c.cl, &dr);
        d12_clear_rt(c.cl, &dr, 0, 0, 1, 1);
        ID3D12GraphicsCommandList_ExecuteBundle(c.cl, bl);
        d12_transition(c.cl, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s;
        if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, &s))) {
            EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_RED), 64);
            d12_sub_free(&s);
        }
        /* the same bundle can be executed repeatedly, from several submissions */
        EXPECT_HR(d12_rerecord(&c), S_OK);
        d12_transition(c.cl, dr.rt, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        d12_draw_setup(c.cl, &dr);
        d12_clear_rt(c.cl, &dr, 0, 1, 0, 1);
        ID3D12GraphicsCommandList_ExecuteBundle(c.cl, bl);
        ID3D12GraphicsCommandList_ExecuteBundle(c.cl, bl);
        d12_transition(c.cl, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        EXPECT_HR(d12_run(&c), S_OK);
        if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, &s))) {
            EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_RED), 64);
            d12_sub_free(&s);
        }
    }
    SAFE_RELEASE(bl); SAFE_RELEASE(ba); d12_draw_free(&dr); d12_ctx_free(&c);
}
TEST(d3d12_objects_bundle_not_executable_on_queue_note) {
    NEED_DEV();
    d12ctx c = {0}; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    /* A bundle can only be executed from a direct list. Submitting it to the queue directly is invalid and only recorded as a note. */
    ID3D12GraphicsCommandList *bl = NULL; ID3D12CommandAllocator *ba = NULL;
    ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_BUNDLE, &IID_ID3D12CommandAllocator, (void **)&ba);
    if (ba && SUCCEEDED(ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_BUNDLE, ba, NULL, &IID_ID3D12GraphicsCommandList, (void **)&bl))) {
        ID3D12GraphicsCommandList_Close(bl);
        t_note("bundle list type = %d (BUNDLE=1); ExecuteCommandLists of bundles is not attempted", (int)ID3D12GraphicsCommandList_GetType(bl));
    }
    SAFE_RELEASE(bl); SAFE_RELEASE(ba); d12_ctx_free(&c);
}

/* ---- multiple threads recording --------------------------------------------------------------- */
typedef struct { ID3D12Device *dev; HRESULT hr; unsigned char *pat; ID3D12Resource *up, *dst; } thr_arg;
static DWORD WINAPI rec_thread(LPVOID p) {
    thr_arg *a = p; d12ctx c = {0};
    a->hr = d12_ctx_init(&c, a->dev, D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (SUCCEEDED(a->hr)) {
        for (int i = 0; i < 20 && SUCCEEDED(a->hr); i++) {
            ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, a->dst, 0, a->up, 0, 256);
            a->hr = d12_run(&c);
            if (SUCCEEDED(a->hr)) a->hr = d12_rerecord(&c);
        }
    }
    d12_ctx_free(&c);
    return 0;
}
TEST(d3d12_objects_list_record_from_threads) {
    NEED_DEV();
    enum { N = 4 };
    thr_arg args[N]; HANDLE th[N]; unsigned char src[256]; fill_pattern(src, sizeof src, 1);
    ID3D12Resource *up = NULL, *dst[N] = { 0 };
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, sizeof src, &up), S_OK);
    if (!up) return;
    for (int i = 0; i < N; i++) {
        d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_NONE, &dst[i]);
        args[i].dev = dev; args[i].up = up; args[i].dst = dst[i]; args[i].hr = E_FAIL;
        th[i] = dst[i] ? CreateThread(NULL, 0, rec_thread, &args[i], 0, NULL) : NULL;
    }
    for (int i = 0; i < N; i++) if (th[i]) {
        CHECK(WaitForSingleObject(th[i], 60000) == WAIT_OBJECT_0);
        EXPECT_HR(args[i].hr, S_OK);
        CloseHandle(th[i]);
    }
    for (int i = 0; i < N; i++) SAFE_RELEASE(dst[i]);
    SAFE_RELEASE(up);
}
