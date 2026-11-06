/* ID3D12GraphicsCommandList1..7 methods. */
#include "cle.h"

TEST(cle_list_interface_ladder) {
    NEED_DEV();
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    const struct { const char *n; const GUID *i; } v[] = {
        { "List1", &IID_ID3D12GraphicsCommandList1 }, { "List2", &IID_ID3D12GraphicsCommandList2 }, { "List3", &IID_ID3D12GraphicsCommandList3 },
        { "List4", &IID_ID3D12GraphicsCommandList4 }, { "List5", &IID_ID3D12GraphicsCommandList5 }, { "List6", &IID_ID3D12GraphicsCommandList6 },
        { "List7", &IID_ID3D12GraphicsCommandList7 } };
    for (unsigned i = 0; i < 7; i++) {
        void *p = cle_qi_cl(c.cl, v[i].i);
        if (cle_warp()) CHECKF((p != NULL) == (i < 6), "QI %s -> %s (WARP12 on Win10 22H2 implements CommandList1..6, not 7)", v[i].n, p ? "yes" : "no");
        else t_note("QI %s -> %s", v[i].n, p ? "yes" : "no");
        if (p) ID3D12GraphicsCommandList_Release((ID3D12GraphicsCommandList *)p);
    }
    d12_ctx_free(&c);
}

/* WriteBufferImmediate (List2): modes DEFAULT / MARKER_IN / MARKER_OUT, several parameters at once */
TEST(cle_write_buffer_immediate) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 o3; memset(&o3, 0, sizeof o3);
    EXPECT_HR(CFS(dev, D3D12_FEATURE_D3D12_OPTIONS3, o3), S_OK);
    if (cle_warp()) EXPECT_EQ(o3.WriteBufferImmediateSupportFlags, 0x3f);
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    ID3D12GraphicsCommandList2 *l2 = c.cl ? cle_qi_cl(c.cl, &IID_ID3D12GraphicsCommandList2) : NULL;
    if (!l2) { t_note("no CommandList2"); d12_ctx_free(&c); return; }
    ID3D12Resource *b = NULL; EXPECT_HR(cle_uav_buf(dev, 256, &b), S_OK);
    if (b) {
        D3D12_GPU_VIRTUAL_ADDRESS va = ID3D12Resource_GetGPUVirtualAddress(b);
        D3D12_WRITEBUFFERIMMEDIATE_PARAMETER ps[4] = { { va + 0, 0x11111111 }, { va + 4, 0x22222222 }, { va + 8, 0x33333333 }, { va + 12, 0x44444444 } };
        D3D12_WRITEBUFFERIMMEDIATE_MODE modes[4] = { D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT, D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_IN, D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT, D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT };
        ID3D12GraphicsCommandList2_WriteBufferImmediate(l2, 4, ps, modes);
        D3D12_WRITEBUFFERIMMEDIATE_PARAMETER p2 = { va + 16, 0xDEADBEEF };
        ID3D12GraphicsCommandList2_WriteBufferImmediate(l2, 1, &p2, NULL);   /* NULL modes = all DEFAULT */
        EXPECT_HR(d12_run(&c), S_OK);
        size_t n = 0; UINT *r = cle_read(dev, b, D3D12_RESOURCE_STATE_COMMON, &n);
        if (r && n >= 20) { EXPECT_EQ(r[0], 0x11111111); EXPECT_EQ(r[1], 0x22222222); EXPECT_EQ(r[2], 0x33333333); EXPECT_EQ(r[3], 0x44444444); EXPECT_EQ(r[4], 0xDEADBEEF); } else CHECK(0);
        free(r);
    }
    SAFE_RELEASE(b); ID3D12GraphicsCommandList2_Release(l2); d12_ctx_free(&c);
}

/* ResolveSubresourceRegion (List1): 4x MSAA RT cleared to a colour, resolved into a sub-rectangle of a larger destination */
TEST(cle_resolve_subresource_region) {
    NEED_DEV();
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ql; memset(&ql, 0, sizeof ql); ql.Format = DXGI_FORMAT_R8G8B8A8_UNORM; ql.SampleCount = 4;
    if (FAILED(CFS(dev, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, ql)) || !ql.NumQualityLevels) { t_skip("4x MSAA unsupported"); return; }
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    ID3D12GraphicsCommandList1 *l1 = c.cl ? cle_qi_cl(c.cl, &IID_ID3D12GraphicsCommandList1) : NULL;
    if (!l1) { t_skip("no CommandList1"); d12_ctx_free(&c); return; }
    D3D12_RESOURCE_DESC md = d12_tex2d_desc(16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET); md.SampleDesc.Count = 4;
    D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_R8G8B8A8_UNORM; cv.Color[0] = 1; cv.Color[2] = 1; cv.Color[3] = 1;
    ID3D12Resource *ms = NULL, *dst = NULL, *dh = NULL; ID3D12DescriptorHeap *rtvh = NULL;
    EXPECT_HR(d12_create_committed(dev, D3D12_HEAP_TYPE_DEFAULT, &md, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, &ms), S_OK);
    EXPECT_HR(d12_make_tex2d(dev, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &dst), S_OK);
    EXPECT_HR(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, 0, &rtvh), S_OK);
    (void)dh;
    if (ms && dst && rtvh) {
        UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE r0 = d3dh_d12_heap_cpu_start(rtvh), r1 = r0; r1.ptr += inc;
        ID3D12Device_CreateRenderTargetView(dev, ms, NULL, r0); ID3D12Device_CreateRenderTargetView(dev, dst, NULL, r1);
        float z[4] = { 0, 0, 0, 0 }, mc[4] = { 1, 0, 1, 1 };
        ID3D12GraphicsCommandList1_ClearRenderTargetView(l1, r1, z, 0, NULL);
        ID3D12GraphicsCommandList1_ClearRenderTargetView(l1, r0, mc, 0, NULL);
        d12_transition(c.cl, ms, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST);
        D3D12_RECT src = { 2, 2, 6, 6 };
        ID3D12GraphicsCommandList1_ResolveSubresourceRegion(l1, dst, 0, 8, 4, ms, 0, &src, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOLVE_MODE_AVERAGE);
        d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s; memset(&s, 0, sizeof s);
        if (SUCCEEDED(d12_readback_sub(dev, dst, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, &s))) {
            EXPECT_EQ(d12_sub_count32(&s, 8, 4, 12, 8, 0xFFFF00FFu), 16);
            EXPECT_EQ(d12_sub_count32(&s, 0, 0, 16, 16, 0xFFFF00FFu), 16);
            d12_sub_free(&s);
        } else CHECK(0);
    }
    SAFE_RELEASE(ms); SAFE_RELEASE(dst); SAFE_RELEASE(rtvh); ID3D12GraphicsCommandList1_Release(l1); d12_ctx_free(&c);
}

/* methods gated by a capability: called only when reported; Close() must succeed */
TEST(cle_list1_capability_gated_calls) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS2 o2; memset(&o2, 0, sizeof o2); EXPECT_HR(CFS(dev, D3D12_FEATURE_D3D12_OPTIONS2, o2), S_OK);
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 o3; memset(&o3, 0, sizeof o3); EXPECT_HR(CFS(dev, D3D12_FEATURE_D3D12_OPTIONS3, o3), S_OK);
    if (cle_warp()) { EXPECT_EQ(o2.DepthBoundsTestSupported, 1); EXPECT_EQ(o2.ProgrammableSamplePositionsTier, 2); EXPECT_EQ(o3.ViewInstancingTier, 1); }
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    ID3D12GraphicsCommandList1 *l1 = c.cl ? cle_qi_cl(c.cl, &IID_ID3D12GraphicsCommandList1) : NULL;
    if (!l1) { t_skip("no CommandList1"); d12_ctx_free(&c); return; }
    if (o2.DepthBoundsTestSupported) ID3D12GraphicsCommandList1_OMSetDepthBounds(l1, 0.25f, 0.75f);
    if (o2.ProgrammableSamplePositionsTier >= 1) {
        D3D12_SAMPLE_POSITION sp[4] = { { -6, -2 }, { 2, -6 }, { -2, 6 }, { 6, 2 } };
        ID3D12GraphicsCommandList1_SetSamplePositions(l1, 4, 1, sp);
    }
    if (o3.ViewInstancingTier != D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED) ID3D12GraphicsCommandList1_SetViewInstanceMask(l1, 0x3);
    EXPECT_HR(d12_run(&c), S_OK);
    ID3D12GraphicsCommandList1_Release(l1); d12_ctx_free(&c);
}
