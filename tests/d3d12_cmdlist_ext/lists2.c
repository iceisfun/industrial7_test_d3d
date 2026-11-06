/* Render passes (List4), depth-bounds / sample-position effects (List1), shading rate (List5), atomic copy (List1). */
#include "cle.h"

TEST(cle_render_pass_clear_preserve_and_draw) {
    NEED_DEV();
    d12draw dr; if (FAILED(d12_draw_init(dev, &dr))) { t_skip("draw setup failed"); d12_draw_free(&dr); return; }
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5; memset(&o5, 0, sizeof o5); EXPECT_HR(CFS(dev, D3D12_FEATURE_D3D12_OPTIONS5, o5), S_OK);
    ID3D12GraphicsCommandList4 *l4 = c.cl ? cle_qi_cl(c.cl, &IID_ID3D12GraphicsCommandList4) : NULL;
    if (!l4) { t_skip("no CommandList4"); d12_ctx_free(&c); d12_draw_free(&dr); return; }
    if (cle_warp()) EXPECT_EQ(o5.RenderPassesTier, D3D12_RENDER_PASS_TIER_1);
    D3D12_RENDER_PASS_RENDER_TARGET_DESC rp; memset(&rp, 0, sizeof rp);
    rp.cpuDescriptor = dr.rtv;
    rp.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
    rp.BeginningAccess.Clear.ClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rp.BeginningAccess.Clear.ClearValue.Color[1] = 1; rp.BeginningAccess.Clear.ClearValue.Color[3] = 1;
    rp.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    ID3D12GraphicsCommandList4_BeginRenderPass(l4, 1, &rp, NULL, D3D12_RENDER_PASS_FLAG_NONE);
    ID3D12GraphicsCommandList4_EndRenderPass(l4);
    /* second pass preserves the clear and draws the red triangle over it */
    rp.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    D3D12_VIEWPORT vp = { 0, 0, 8, 8, 0, 1 }; D3D12_RECT sc = { 0, 0, 8, 4 };   /* scissor: top half only */
    ID3D12GraphicsCommandList4_SetGraphicsRootSignature(l4, dr.rs); ID3D12GraphicsCommandList4_SetPipelineState(l4, dr.pso);
    ID3D12GraphicsCommandList4_BeginRenderPass(l4, 1, &rp, NULL, D3D12_RENDER_PASS_FLAG_NONE);
    ID3D12GraphicsCommandList4_RSSetViewports(l4, 1, &vp); ID3D12GraphicsCommandList4_RSSetScissorRects(l4, 1, &sc);
    ID3D12GraphicsCommandList4_IASetPrimitiveTopology(l4, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D12GraphicsCommandList4_DrawInstanced(l4, 3, 1, 0, 0);
    ID3D12GraphicsCommandList4_EndRenderPass(l4);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s; memset(&s, 0, sizeof s);
    if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) {
        EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 4, PX_RED), 32);
        EXPECT_EQ(d12_sub_count32(&s, 0, 4, 8, 8, PX_GREEN), 32);
        d12_sub_free(&s);
    } else CHECK(0);
    ID3D12GraphicsCommandList4_Release(l4); d12_ctx_free(&c); d12_draw_free(&dr);
}

/* depth-bounds test: the bounds are compared with the value stored in the depth buffer */
typedef struct { DXGI_FORMAT RTFormats[8]; UINT NumRenderTargets; } cle_rtfa;
#define CLE_ALIGN __attribute__((aligned(8)))
typedef struct { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t0 CLE_ALIGN; ID3D12RootSignature *rs;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t1 CLE_ALIGN; D3D12_SHADER_BYTECODE vs;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t2 CLE_ALIGN; D3D12_SHADER_BYTECODE ps;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t3 CLE_ALIGN; D3D12_BLEND_DESC bl;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t4 CLE_ALIGN; D3D12_RASTERIZER_DESC rs_;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t5 CLE_ALIGN; D3D12_DEPTH_STENCIL_DESC1 ds;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t6 CLE_ALIGN; cle_rtfa rtv;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t7 CLE_ALIGN; DXGI_FORMAT dsv;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t8 CLE_ALIGN; D3D12_PRIMITIVE_TOPOLOGY_TYPE topo;
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t9 CLE_ALIGN; DXGI_SAMPLE_DESC sd; } cle_stream;
static void cle_fill_stream(cle_stream *s, ID3D12RootSignature *rs, ID3DBlob *vs, ID3DBlob *ps, DXGI_FORMAT dsv, int bounds, UINT samples, BOOL depth_enable) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gd; d12_fill_gfx_desc(&gd, rs, vs, ps, DXGI_FORMAT_R8G8B8A8_UNORM);
    memset(s, 0, sizeof *s);
    s->t0 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE; s->rs = rs;
    s->t1 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS; s->vs = gd.VS;
    s->t2 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS; s->ps = gd.PS;
    s->t3 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND; s->bl = gd.BlendState;
    s->t4 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER; s->rs_ = gd.RasterizerState;
    s->t5 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1;
    s->ds.DepthEnable = depth_enable; s->ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; s->ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    s->ds.FrontFace = gd.DepthStencilState.FrontFace; s->ds.BackFace = gd.DepthStencilState.BackFace; s->ds.DepthBoundsTestEnable = bounds;
    s->t6 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS; s->rtv.NumRenderTargets = 1; s->rtv.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    s->t7 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT; s->dsv = dsv;
    s->t8 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY; s->topo = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    s->t9 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC; s->sd.Count = samples;
}
/* Measured on WARP (Win10 22H2): the bounds test only takes effect when DepthEnable is TRUE; with depth testing off every draw passes. */
static void depth_bounds_case(ID3D12Device *dev, BOOL depth_enable) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS2 o2; memset(&o2, 0, sizeof o2); EXPECT_HR(CFS(dev, D3D12_FEATURE_D3D12_OPTIONS2, o2), S_OK);
    ID3D12Device2 *d2 = cle_qi(dev, &IID_ID3D12Device2);
    if (!o2.DepthBoundsTestSupported || !d2) { t_skip("depth bounds unsupported"); if (d2) ID3D12Device2_Release(d2); return; }
    d12draw dr; if (FAILED(d12_draw_init(dev, &dr))) { t_skip("draw setup failed"); d12_draw_free(&dr); ID3D12Device2_Release(d2); return; }
    cle_stream st; cle_fill_stream(&st, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_D32_FLOAT, 1, 1, depth_enable);
    D3D12_PIPELINE_STATE_STREAM_DESC sd = { sizeof st, &st };
    ID3D12PipelineState *pso = NULL; EXPECT_HR(ID3D12Device2_CreatePipelineState(d2, &sd, &IID_ID3D12PipelineState, (void **)&pso), S_OK);
    ID3D12Resource *ds = NULL; ID3D12DescriptorHeap *dsvh = NULL;
    D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_D32_FLOAT; cv.DepthStencil.Depth = 0.5f;
    EXPECT_HR(d12_make_tex2d(dev, 8, 8, DXGI_FORMAT_D32_FLOAT, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, &ds), S_OK);
    EXPECT_HR(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, 0, &dsvh), S_OK);
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    ID3D12GraphicsCommandList1 *l1 = c.cl ? cle_qi_cl(c.cl, &IID_ID3D12GraphicsCommandList1) : NULL;
    if (pso && ds && dsvh && l1) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3dh_d12_heap_cpu_start(dsvh); ID3D12Device_CreateDepthStencilView(dev, ds, NULL, dsv);
        d12_draw_setup(c.cl, &dr);
        ID3D12GraphicsCommandList_SetPipelineState(c.cl, pso);
        ID3D12GraphicsCommandList_OMSetRenderTargets(c.cl, 1, &dr.rtv, FALSE, &dsv);
        ID3D12GraphicsCommandList_ClearDepthStencilView(c.cl, dsv, D3D12_CLEAR_FLAG_DEPTH, 0.5f, 0, 0, NULL);
        d12_clear_rt(c.cl, &dr, 0, 0, 1, 1);
        static const struct { D3D12_RECT r; float lo, hi; } q[4] = {
            { { 0, 0, 4, 4 }, 0.4f, 0.6f }, { { 4, 0, 8, 4 }, 0.6f, 0.9f }, { { 0, 4, 4, 8 }, 0.0f, 0.4f }, { { 4, 4, 8, 8 }, 0.5f, 0.5f } };
        for (int i = 0; i < 4; i++) {
            ID3D12GraphicsCommandList1_OMSetDepthBounds(l1, q[i].lo, q[i].hi);
            ID3D12GraphicsCommandList_RSSetScissorRects(c.cl, 1, &q[i].r);
            ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 0, 0);
        }
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s; memset(&s, 0, sizeof s);
        if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) {
            /* stored depth 0.5: [0.4,0.6] passes, [0.6,0.9] and [0,0.4] reject, the degenerate [0.5,0.5] passes (bounds are inclusive) */
            UINT want[4] = { PX_RED, PX_BLUE, PX_BLUE, PX_RED };
            if (!depth_enable) for (int i = 0; i < 4; i++) want[i] = PX_RED;
            for (int i = 0; i < 4; i++) {
                UINT n = d12_sub_count32(&s, q[i].r.left, q[i].r.top, q[i].r.right, q[i].r.bottom, want[i]);
                CHECKF(n == 16, "depth_enable=%d bounds [%.1f,%.1f]: %u/16 pixels 0x%08x (px 0x%08x)", depth_enable, q[i].lo, q[i].hi, n, want[i], d12_sub_px32(&s, q[i].r.left, q[i].r.top, 0));
            }
            d12_sub_free(&s);
        } else CHECK(0);
    }
    if (l1) ID3D12GraphicsCommandList1_Release(l1);
    d12_ctx_free(&c); SAFE_RELEASE(ds); SAFE_RELEASE(dsvh); SAFE_RELEASE(pso); d12_draw_free(&dr); ID3D12Device2_Release(d2);
}
TEST(cle_depth_bounds_test_rejects_outside_stored_depth) { NEED_DEV(); depth_bounds_case(dev, TRUE); }
TEST(cle_depth_bounds_ignored_without_depth_enable) { NEED_DEV(); if (!cle_warp()) { t_skip("pin is WARP behaviour"); return; } depth_bounds_case(dev, FALSE); }

/* programmable sample positions: a quad covering the left half of one pixel is fully covered with all-left positions and not at all with all-right ones */
static const char *quad_vs =
    "float4 main(uint v : SV_VertexID) : SV_Position {\n"
    "  static const float2 P[6] = { float2(-0.5,0), float2(-0.5,0.5), float2(-0.25,0), float2(-0.25,0), float2(-0.5,0.5), float2(-0.25,0.5) };\n"
    "  return float4(P[v], 0, 1); }\n";
static void sp_run(ID3D12Device *dev, const D3D12_SAMPLE_POSITION *sp, UINT *px_out, int *ran) {
    *ran = 0; *px_out = 0;
    ID3DBlob *vs = d12_compile_or_null(quad_vs, "vs_5_0"), *ps = d12_compile_or_null(d12_ps_red_src, "ps_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    d12ctx c; memset(&c, 0, sizeof c);
    ID3D12Resource *ms = NULL, *dst = NULL; ID3D12DescriptorHeap *rtvh = NULL;
    if (!vs || !ps || FAILED(d12_make_rootsig(dev, 0, NULL, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE, &rs))) goto out;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gd; d12_fill_gfx_desc(&gd, rs, vs, ps, DXGI_FORMAT_R8G8B8A8_UNORM); gd.SampleDesc.Count = 4;
    if (FAILED(ID3D12Device_CreateGraphicsPipelineState(dev, &gd, &IID_ID3D12PipelineState, (void **)&pso))) goto out;
    D3D12_RESOURCE_DESC md = d12_tex2d_desc(4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET); md.SampleDesc.Count = 4;
    D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (FAILED(d12_create_committed(dev, D3D12_HEAP_TYPE_DEFAULT, &md, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, &ms))) goto out;
    if (FAILED(d12_make_tex2d(dev, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &dst))) goto out;
    if (FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, 0, &rtvh))) goto out;
    if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) goto out;
    ID3D12GraphicsCommandList1 *l1 = cle_qi_cl(c.cl, &IID_ID3D12GraphicsCommandList1); if (!l1) goto out;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3dh_d12_heap_cpu_start(rtvh); ID3D12Device_CreateRenderTargetView(dev, ms, NULL, rtv);
    D3D12_VIEWPORT vp = { 0, 0, 4, 4, 0, 1 }; D3D12_RECT sc = { 0, 0, 4, 4 };
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(c.cl, rs); ID3D12GraphicsCommandList_SetPipelineState(c.cl, pso);
    ID3D12GraphicsCommandList_OMSetRenderTargets(c.cl, 1, &rtv, FALSE, NULL);
    ID3D12GraphicsCommandList_RSSetViewports(c.cl, 1, &vp); ID3D12GraphicsCommandList_RSSetScissorRects(c.cl, 1, &sc);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(c.cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    float z[4] = { 0, 0, 0, 0 }; ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, rtv, z, 0, NULL);
    ID3D12GraphicsCommandList1_SetSamplePositions(l1, 4, 1, (D3D12_SAMPLE_POSITION *)sp);
    ID3D12GraphicsCommandList_DrawInstanced(c.cl, 6, 1, 0, 0);
    ID3D12GraphicsCommandList1_SetSamplePositions(l1, 0, 0, NULL);
    d12_transition(c.cl, ms, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    ID3D12GraphicsCommandList_ResolveSubresource(c.cl, dst, 0, ms, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    d12_transition(c.cl, dst, 0, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    ID3D12GraphicsCommandList1_Release(l1);
    if (FAILED(d12_run(&c))) goto out;
    d12_sub s; memset(&s, 0, sizeof s);
    if (SUCCEEDED(d12_readback_sub(dev, dst, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, &s))) { *px_out = d12_sub_px32(&s, 1, 1, 0); *ran = 1; d12_sub_free(&s); }
out:
    d12_ctx_free(&c); SAFE_RELEASE(ms); SAFE_RELEASE(dst); SAFE_RELEASE(rtvh); SAFE_RELEASE(pso); SAFE_RELEASE(rs);
    if (vs) ID3D10Blob_Release(vs); if (ps) ID3D10Blob_Release(ps);
}
TEST(cle_set_sample_positions_changes_coverage) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS2 o2; memset(&o2, 0, sizeof o2); EXPECT_HR(CFS(dev, D3D12_FEATURE_D3D12_OPTIONS2, o2), S_OK);
    if (!o2.ProgrammableSamplePositionsTier) { t_skip("programmable sample positions unsupported"); return; }
    D3D12_SAMPLE_POSITION left[4] = { { -6, -2 }, { -2, -6 }, { -4, 0 }, { -1, 3 } }, right[4] = { { 1, 1 }, { 6, -2 }, { 3, 5 }, { 7, 7 } };
    UINT a = 0, b = 0; int ra = 0, rb = 0;
    sp_run(dev, left, &a, &ra); sp_run(dev, right, &b, &rb);
    CHECK(ra && rb);
    EXPECT_EQ(a, PX_RED); EXPECT_EQ(b, PX_ZERO);
}

TEST(cle_list5_shading_rate_calls) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS6 o6; memset(&o6, 0, sizeof o6); EXPECT_HR(CFS(dev, D3D12_FEATURE_D3D12_OPTIONS6, o6), S_OK);
    if (cle_warp()) EXPECT_EQ(o6.VariableShadingRateTier, 1);
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    ID3D12GraphicsCommandList5 *l5 = c.cl ? cle_qi_cl(c.cl, &IID_ID3D12GraphicsCommandList5) : NULL;
    if (l5 && o6.VariableShadingRateTier) {
        ID3D12GraphicsCommandList5_Release(l5); d12_ctx_free(&c);
        for (int k = 0; k < 6; k++) {
            d12ctx cc; if (FAILED(d12_ctx_init(&cc, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) continue;
            ID3D12GraphicsCommandList5 *m = cle_qi_cl(cc.cl, &IID_ID3D12GraphicsCommandList5);
            D3D12_SHADING_RATE_COMBINER cb[2] = { D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_OVERRIDE };
            D3D12_SHADING_RATE_COMBINER cp[2] = { D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH };
            if (k == 0) ID3D12GraphicsCommandList5_RSSetShadingRate(m, D3D12_SHADING_RATE_2X2, cb);
            if (k == 1) ID3D12GraphicsCommandList5_RSSetShadingRate(m, D3D12_SHADING_RATE_1X1, NULL);
            if (k == 2) ID3D12GraphicsCommandList5_RSSetShadingRate(m, D3D12_SHADING_RATE_2X2, cp);
            if (k == 3) ID3D12GraphicsCommandList5_RSSetShadingRateImage(m, NULL);
            if (k == 4) ID3D12GraphicsCommandList5_RSSetShadingRate(m, D3D12_SHADING_RATE_4X4, cp);
            if (k == 5) ID3D12GraphicsCommandList5_RSSetShadingRate(m, D3D12_SHADING_RATE_2X1, cp);
            HRESULT hr = d12_run(&cc);
            /* variant 3 (a NULL shading-rate image on a tier-1 device) invalidates the list: Close fails with E_FAIL (Win10 22H2 WARP) */
            WARP_HR(hr, k == 3 ? E_FAIL : S_OK);
            ID3D12GraphicsCommandList5_Release(m); d12_ctx_free(&cc);
        }
        return;
    } else t_skip("no List5 or no VRS");
    if (l5) ID3D12GraphicsCommandList5_Release(l5); d12_ctx_free(&c);
}
TEST(cle_list1_atomic_copy_buffer) {
    NEED_DEV();
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    ID3D12GraphicsCommandList1 *l1 = c.cl ? cle_qi_cl(c.cl, &IID_ID3D12GraphicsCommandList1) : NULL;
    ID3D12Resource *a = NULL, *b = NULL, *up = NULL; EXPECT_HR(cle_uav_buf(dev, 256, &a), S_OK); EXPECT_HR(cle_uav_buf(dev, 256, &b), S_OK);
    UINT src[4] = { 0x01020304, 0x0A0B0C0D, 0x55AA55AA, 0xCAFEF00D };
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, 256, &up), S_OK);
    if (l1 && a && b && up) {
        d12_transition(c.cl, b, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, b, 0, up, 0, 16);
        d12_transition(c.cl, b, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        d12_transition(c.cl, a, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        ID3D12GraphicsCommandList1_AtomicCopyBufferUINT(l1, a, 4, b, 8, 0, NULL, NULL);
        ID3D12GraphicsCommandList1_AtomicCopyBufferUINT(l1, a, 16, b, 12, 0, NULL, NULL);
        d12_transition(c.cl, a, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        WARP_HR(d12_run(&c), S_OK);
        size_t n = 0; UINT *r = cle_read(dev, a, D3D12_RESOURCE_STATE_COMMON, &n);
        if (r && n >= 20) { WARP_EQ(r[1], 0x55AA55AA); WARP_EQ(r[4], 0xCAFEF00D); WARP_EQ(r[0], 0); WARP_EQ(r[2], 0); } else CHECK(0);
        free(r);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(up); if (l1) ID3D12GraphicsCommandList1_Release(l1); d12_ctx_free(&c);
}
