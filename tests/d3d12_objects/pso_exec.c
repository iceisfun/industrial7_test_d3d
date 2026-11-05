/* d3d12_objects: descriptor heaps, pipeline states, root signatures, dispatch/draw/indirect, queries, predication, device queries. */
#include "d12o.h"

/* ====== descriptor heaps ============================================================================== */
static const D3D12_DESCRIPTOR_HEAP_TYPE dh_types[4] = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
static const char *dh_names[4] = { "CBV_SRV_UAV", "SAMPLER", "RTV", "DSV" };

TEST(d3d12_objects_dheap_increment_sizes) {
    NEED_DEV();
    for (int i = 0; i < 4; i++) {
        UINT a = ID3D12Device_GetDescriptorHandleIncrementSize(dev, dh_types[i]), b = ID3D12Device_GetDescriptorHandleIncrementSize(dev, dh_types[i]);
        CHECKF(a > 0 && a == b, "%s increment %u/%u", dh_names[i], a, b);
        CHECKF(a < 4096, "%s increment %u suspiciously large", dh_names[i], a);
        t_note("descriptor increment %s = %u", dh_names[i], a);
    }
    UINT bad = ID3D12Device_GetDescriptorHandleIncrementSize(dev, (D3D12_DESCRIPTOR_HEAP_TYPE)D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
    t_note("increment for NUM_TYPES = %u", bad);
}
TEST(d3d12_objects_dheap_create_matrix) {
    NEED_DEV();
    for (int i = 0; i < 4; i++) for (int vis = 0; vis < 2; vis++) {
        ID3D12DescriptorHeap *h = NULL;
        HRESULT hr = d12_make_dheap(dev, dh_types[i], 16, vis, &h);
        int visible_ok = dh_types[i] == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || dh_types[i] == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        if (vis && !visible_ok) CHECKF(hr == E_INVALIDARG, "%s shader-visible: %s", dh_names[i], d3dh_hr_name(hr));
        else CHECKF(hr == S_OK, "%s vis=%d: %s", dh_names[i], vis, d3dh_hr_name(hr));
        if (h) {
            D3D12_DESCRIPTOR_HEAP_DESC g = d12_dh_desc(h);
            EXPECT_EQ(g.Type, dh_types[i]); EXPECT_EQ(g.NumDescriptors, 16);
            EXPECT_EQ(g.Flags, vis ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
            EXPECT_EQ(g.NodeMask, 1);   /* NodeMask 0 is reported as 1 */
            SAFE_RELEASE(h);
        }
    }
}
TEST(d3d12_objects_dheap_invalid_type) {
    NEED_DEV();
    D3D12_DESCRIPTOR_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; hd.NumDescriptors = 4;
    ID3D12DescriptorHeap *h = NULL;
    HRESULT hr = ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&h);
    CHECKF(FAILED(hr), "heap type NUM_TYPES: %s", d3dh_hr_name(hr)); SAFE_RELEASE(h);
    hd.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)77;
    hr = ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&h);
    CHECKF(FAILED(hr), "heap type 77: %s", d3dh_hr_name(hr)); SAFE_RELEASE(h);
    hr = ID3D12Device_CreateDescriptorHeap(dev, NULL, &IID_ID3D12DescriptorHeap, (void **)&h);
    CHECKF(FAILED(hr), "NULL desc: %s", d3dh_hr_name(hr)); SAFE_RELEASE(h);
}
TEST(d3d12_objects_dheap_zero_descriptors_note) {
    NEED_DEV();
    D3D12_DESCRIPTOR_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 0;
    ID3D12DescriptorHeap *h = NULL;
    HRESULT hr = ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&h);
    t_note("descriptor heap with NumDescriptors=0: %s", d3dh_hr_name(hr)); SAFE_RELEASE(h);
}
TEST(d3d12_objects_dheap_sampler_limit) {
    NEED_DEV();
    ID3D12DescriptorHeap *h = NULL;
    EXPECT_HR(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, 1, &h), S_OK); SAFE_RELEASE(h);
    HRESULT hr = d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2049, 1, &h);
    CHECKF(hr == E_INVALIDARG, "2049 shader-visible samplers: %s", d3dh_hr_name(hr)); SAFE_RELEASE(h);
}
TEST(d3d12_objects_dheap_handles) {
    NEED_DEV();
    for (int i = 0; i < 4; i++) {
        ID3D12DescriptorHeap *a = NULL, *b = NULL;
        if (FAILED(d12_make_dheap(dev, dh_types[i], 8, 0, &a)) || FAILED(d12_make_dheap(dev, dh_types[i], 8, 0, &b))) { SAFE_RELEASE(a); SAFE_RELEASE(b); continue; }
        D3D12_CPU_DESCRIPTOR_HANDLE ca = d3dh_d12_heap_cpu_start(a), cb = d3dh_d12_heap_cpu_start(b);
        D3D12_GPU_DESCRIPTOR_HANDLE ga = d3dh_d12_heap_gpu_start(a);
        UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, dh_types[i]);
        CHECKF(ca.ptr != 0 && cb.ptr != 0, "%s: CPU start is NULL", dh_names[i]);
        CHECKF(ca.ptr != cb.ptr, "%s: two heaps share a CPU start", dh_names[i]);
        CHECKF(ca.ptr + 8 * inc <= cb.ptr || cb.ptr + 8 * inc <= ca.ptr, "%s: CPU ranges overlap", dh_names[i]);
        CHECKF(ga.ptr == 0, "%s: non-shader-visible heap has GPU handle 0x%llx", dh_names[i], (unsigned long long)ga.ptr);
        EXPECT_EQ(d3dh_d12_heap_cpu_start(a).ptr, ca.ptr);   /* stable */
        SAFE_RELEASE(a); SAFE_RELEASE(b);
    }
    for (int i = 0; i < 2; i++) {
        ID3D12DescriptorHeap *a = NULL;
        if (FAILED(d12_make_dheap(dev, dh_types[i], 8, 1, &a))) continue;
        CHECKF(d3dh_d12_heap_gpu_start(a).ptr != 0, "%s: shader-visible heap has GPU handle 0", dh_names[i]);
        CHECKF(d3dh_d12_heap_cpu_start(a).ptr != 0, "%s: shader-visible heap has CPU handle 0", dh_names[i]);
        SAFE_RELEASE(a);
    }
}
TEST(d3d12_objects_dheap_iids) {
    NEED_DEV();
    ID3D12DescriptorHeap *h = NULL; void *o;
    EXPECT_HR(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, 0, &h), S_OK);
    if (!h) return;
    o = NULL; EXPECT_HR(ID3D12DescriptorHeap_QueryInterface(h, &IID_ID3D12Pageable, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = (void *)1; EXPECT_HR(ID3D12DescriptorHeap_QueryInterface(h, &IID_ID3D12Heap, &o), E_NOINTERFACE); CHECK(o == NULL);
    ID3D12Device *dv = NULL;
    EXPECT_HR(ID3D12DescriptorHeap_GetDevice(h, &IID_ID3D12Device, (void **)&dv), S_OK); CHECK(dv == dev); if (dv) ID3D12Device_Release(dv);
    SAFE_RELEASE(h);
}
TEST(d3d12_objects_dheap_ppv_null_s_false) {
    NEED_DEV();
    D3D12_DESCRIPTOR_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 4;
    EXPECT_HR(ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, NULL), S_FALSE);
}
TEST(d3d12_objects_dheap_view_creation_all_kinds) {
    NEED_DEV();
    ID3D12DescriptorHeap *srv = NULL, *smp = NULL, *rtv = NULL, *dsv = NULL;
    ID3D12Resource *buf = NULL, *tex = NULL, *rt = NULL, *ds = NULL;
    if (FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, 1, &srv)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2, 1, &smp)) ||
        FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, 0, &rtv)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 2, 0, &dsv))) { t_skip("heaps"); goto out; }
    d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 4096, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &buf);
    d12_make_tex2d(dev, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, NULL, &tex);
    d12_make_tex2d(dev, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &rt);
    {
        D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_D32_FLOAT; cv.DepthStencil.Depth = 1;
        d12_make_tex2d(dev, 16, 16, DXGI_FORMAT_D32_FLOAT, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, &ds);
    }
    if (!buf || !tex || !rt || !ds) { t_skip("resources"); goto out; }
    UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv; cbv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(buf); cbv.SizeInBytes = 256;
        ID3D12Device_CreateConstantBufferView(dev, &cbv, d12_dh_cpu(srv, 0, inc));
        D3D12_SHADER_RESOURCE_VIEW_DESC sv; memset(&sv, 0, sizeof sv);
        sv.Format = DXGI_FORMAT_R32_TYPELESS; sv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER; sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Buffer.NumElements = 1024; sv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        ID3D12Device_CreateShaderResourceView(dev, buf, &sv, d12_dh_cpu(srv, 1, inc));
        memset(&sv, 0, sizeof sv);
        sv.Format = DXGI_FORMAT_UNKNOWN; sv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER; sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Buffer.NumElements = 64; sv.Buffer.StructureByteStride = 64;
        ID3D12Device_CreateShaderResourceView(dev, buf, &sv, d12_dh_cpu(srv, 2, inc));
        memset(&sv, 0, sizeof sv);
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = (UINT)-1;
        ID3D12Device_CreateShaderResourceView(dev, tex, &sv, d12_dh_cpu(srv, 3, inc));
        ID3D12Device_CreateShaderResourceView(dev, tex, NULL, d12_dh_cpu(srv, 4, inc));   /* NULL desc: resource's own format */
        D3D12_UNORDERED_ACCESS_VIEW_DESC uv; memset(&uv, 0, sizeof uv);
        uv.Format = DXGI_FORMAT_R32_TYPELESS; uv.ViewDimension = D3D12_UAV_DIMENSION_BUFFER; uv.Buffer.NumElements = 1024; uv.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        ID3D12Device_CreateUnorderedAccessView(dev, buf, NULL, &uv, d12_dh_cpu(srv, 5, inc));
        memset(&uv, 0, sizeof uv);
        uv.Format = DXGI_FORMAT_R8G8B8A8_UNORM; uv.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; uv.Texture2D.MipSlice = 1;
        ID3D12Device_CreateUnorderedAccessView(dev, tex, NULL, &uv, d12_dh_cpu(srv, 6, inc));
        ID3D12Device_CreateUnorderedAccessView(dev, NULL, NULL, &uv, d12_dh_cpu(srv, 7, inc));   /* null UAV descriptor */
        D3D12_SAMPLER_DESC sd; memset(&sd, 0, sizeof sd);
        sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; sd.MaxLOD = D3D12_FLOAT32_MAX; sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        UINT sinc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        ID3D12Device_CreateSampler(dev, &sd, d12_dh_cpu(smp, 0, sinc));
        sd.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT; sd.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
        ID3D12Device_CreateSampler(dev, &sd, d12_dh_cpu(smp, 1, sinc));
        UINT rinc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        ID3D12Device_CreateRenderTargetView(dev, rt, NULL, d12_dh_cpu(rtv, 0, rinc));
        D3D12_RENDER_TARGET_VIEW_DESC rv; memset(&rv, 0, sizeof rv); rv.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        ID3D12Device_CreateRenderTargetView(dev, rt, &rv, d12_dh_cpu(rtv, 1, rinc));
        UINT dinc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        ID3D12Device_CreateDepthStencilView(dev, ds, NULL, d12_dh_cpu(dsv, 0, dinc));
        D3D12_DEPTH_STENCIL_VIEW_DESC dd; memset(&dd, 0, sizeof dd); dd.Format = DXGI_FORMAT_D32_FLOAT; dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; dd.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
        ID3D12Device_CreateDepthStencilView(dev, ds, &dd, d12_dh_cpu(dsv, 1, dinc));
    }
    CHECK(1);   /* the creation calls return void: reaching here without a crash is the contract */
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(smp); SAFE_RELEASE(rtv); SAFE_RELEASE(dsv); SAFE_RELEASE(buf); SAFE_RELEASE(tex); SAFE_RELEASE(rt); SAFE_RELEASE(ds);
}

/* RTV descriptor copies, verified by clearing through the copied descriptors. */
static ID3D12Resource *rt_tex[6];
static int mk_rts(ID3D12Device *dev) {
    for (int i = 0; i < 6; i++) { rt_tex[i] = NULL; if (FAILED(d12_make_tex2d(dev, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &rt_tex[i]))) return 0; }
    return 1;
}
static void free_rts(void) { for (int i = 0; i < 6; i++) SAFE_RELEASE(rt_tex[i]); }
static UINT32 rt_first_px(ID3D12Device *dev, ID3D12Resource *t) {
    d12_sub s; UINT32 v = 0xDEADDEAD;
    if (SUCCEEDED(d12_readback_sub(dev, t, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) { v = d12_sub_px32(&s, 0, 0, 0); d12_sub_free(&s); }
    return v;
}
TEST(d3d12_objects_dheap_copy_descriptors_simple) {
    NEED_DEV();
    ID3D12DescriptorHeap *src = NULL, *dst = NULL; d12ctx c = {0};
    if (!mk_rts(dev) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 6, 0, &src)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 6, 0, &dst)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup"); goto out; }
    {
        UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        for (int i = 0; i < 6; i++) ID3D12Device_CreateRenderTargetView(dev, rt_tex[i], NULL, d12_dh_cpu(src, i, inc));
        /* copy descriptors 1..3 of src to slots 2..4 of dst */
        ID3D12Device_CopyDescriptorsSimple(dev, 3, d12_dh_cpu(dst, 2, inc), d12_dh_cpu(src, 1, inc), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        static const float col[3][4] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 } };
        for (int i = 0; i < 3; i++) ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, d12_dh_cpu(dst, 2 + i, inc), col[i], 0, NULL);
        EXPECT_HR(d12_run(&c), S_OK);
        EXPECT_EQ(rt_first_px(dev, rt_tex[1]), PX_RED);
        EXPECT_EQ(rt_first_px(dev, rt_tex[2]), PX_GREEN);
        EXPECT_EQ(rt_first_px(dev, rt_tex[3]), PX_BLUE);
        UINT32 u0 = rt_first_px(dev, rt_tex[0]), u4 = rt_first_px(dev, rt_tex[4]);
        if (u0 != 0 || u4 != 0) t_note("untouched targets read 0x%08x / 0x%08x", u0, u4);
        CHECK(u0 != PX_RED && u0 != PX_GREEN && u0 != PX_BLUE);
        CHECK(u4 != PX_RED && u4 != PX_GREEN && u4 != PX_BLUE);
    }
out:
    d12_ctx_free(&c); free_rts(); SAFE_RELEASE(src); SAFE_RELEASE(dst);
}
TEST(d3d12_objects_dheap_copy_descriptors_ranges) {
    NEED_DEV();
    ID3D12DescriptorHeap *src = NULL, *dst = NULL; d12ctx c = {0};
    if (!mk_rts(dev) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 6, 0, &src)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 6, 0, &dst)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup"); goto out; }
    {
        UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        for (int i = 0; i < 6; i++) ID3D12Device_CreateRenderTargetView(dev, rt_tex[i], NULL, d12_dh_cpu(src, i, inc));
        /* sources: {0,1} then {4,5} -> destinations: {1,2,3} then {5}  => d1<-s0 d2<-s1 d3<-s4 d5<-s5 */
        D3D12_CPU_DESCRIPTOR_HANDLE ss[2] = { d12_dh_cpu(src, 0, inc), d12_dh_cpu(src, 4, inc) }; UINT ssz[2] = { 2, 2 };
        D3D12_CPU_DESCRIPTOR_HANDLE ds[2] = { d12_dh_cpu(dst, 1, inc), d12_dh_cpu(dst, 5, inc) }; UINT dsz[2] = { 3, 1 };
        ID3D12Device_CopyDescriptors(dev, 2, ds, dsz, 2, ss, ssz, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        static const float col[4][4] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 1, 1, 1, 1 } };
        static const int slot[4] = { 1, 2, 3, 5 }, tex[4] = { 0, 1, 4, 5 };
        for (int i = 0; i < 4; i++) ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, d12_dh_cpu(dst, slot[i], inc), col[i], 0, NULL);
        EXPECT_HR(d12_run(&c), S_OK);
        static const UINT32 want[4] = { PX_RED, PX_GREEN, PX_BLUE, 0xFFFFFFFF };
        for (int i = 0; i < 4; i++) { UINT32 v = rt_first_px(dev, rt_tex[tex[i]]); CHECKF(v == want[i], "texture %d: 0x%08x want 0x%08x", tex[i], v, want[i]); }
    }
out:
    d12_ctx_free(&c); free_rts(); SAFE_RELEASE(src); SAFE_RELEASE(dst);
}
TEST(d3d12_objects_dheap_copy_descriptors_null_dst_sizes) {
    NEED_DEV();
    /* pDestDescriptorRangeSizes == NULL: every range has size 1 */
    ID3D12DescriptorHeap *src = NULL, *dst = NULL; d12ctx c = {0};
    if (!mk_rts(dev) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 6, 0, &src)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 6, 0, &dst)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup"); goto out; }
    {
        UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        for (int i = 0; i < 6; i++) ID3D12Device_CreateRenderTargetView(dev, rt_tex[i], NULL, d12_dh_cpu(src, i, inc));
        D3D12_CPU_DESCRIPTOR_HANDLE ds[2] = { d12_dh_cpu(dst, 4, inc), d12_dh_cpu(dst, 0, inc) };
        D3D12_CPU_DESCRIPTOR_HANDLE ss[2] = { d12_dh_cpu(src, 2, inc), d12_dh_cpu(src, 3, inc) };
        ID3D12Device_CopyDescriptors(dev, 2, ds, NULL, 2, ss, NULL, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        float red[4] = { 1, 0, 0, 1 }, green[4] = { 0, 1, 0, 1 };
        ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, d12_dh_cpu(dst, 4, inc), red, 0, NULL);
        ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, d12_dh_cpu(dst, 0, inc), green, 0, NULL);
        EXPECT_HR(d12_run(&c), S_OK);
        EXPECT_EQ(rt_first_px(dev, rt_tex[2]), PX_RED);
        EXPECT_EQ(rt_first_px(dev, rt_tex[3]), PX_GREEN);
    }
out:
    d12_ctx_free(&c); free_rts(); SAFE_RELEASE(src); SAFE_RELEASE(dst);
}

/* ====== root signatures ===================================================================================== */
TEST(d3d12_objects_rootsig_create_variants) {
    NEED_DEV();
    ID3D12RootSignature *rs = NULL;
    EXPECT_HR(d12_make_rootsig(dev, 0, NULL, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE, &rs), S_OK); SAFE_RELEASE(rs);
    EXPECT_HR(d12_make_rootsig(dev, 0, NULL, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, &rs), S_OK); SAFE_RELEASE(rs);
    EXPECT_HR(d12_make_cs_rootsig(dev, &rs), S_OK);
    if (rs) { EXPECT_EQ(d3dh_refcount(rs), 1); SAFE_RELEASE(rs); }
    D3D12_DESCRIPTOR_RANGE rg[3]; memset(rg, 0, sizeof rg);
    rg[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; rg[0].NumDescriptors = 4; rg[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rg[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; rg[1].NumDescriptors = 2; rg[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rg[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; rg[2].NumDescriptors = 1; rg[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER p[3]; memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; p[0].DescriptorTable.NumDescriptorRanges = 3; p[0].DescriptorTable.pDescriptorRanges = rg;
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; p[1].Descriptor.ShaderRegister = 1; p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[2].Descriptor.ShaderRegister = 5; p[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC ss; memset(&ss, 0, sizeof ss);
    ss.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; ss.MaxLOD = D3D12_FLOAT32_MAX;
    ss.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    EXPECT_HR(d12_make_rootsig(dev, 3, p, 1, &ss, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, &rs), S_OK);
    SAFE_RELEASE(rs);
}
TEST(d3d12_objects_rootsig_create_invalid_blob) {
    NEED_DEV();
    static const unsigned char junk[64] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    ID3D12RootSignature *rs = (ID3D12RootSignature *)(ULONG_PTR)1;
    HRESULT hr = ID3D12Device_CreateRootSignature(dev, 0, junk, sizeof junk, &IID_ID3D12RootSignature, (void **)&rs);
    CHECKF(FAILED(hr), "garbage blob: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(rs); else CHECK(rs == NULL);
    rs = NULL;
    hr = ID3D12Device_CreateRootSignature(dev, 0, junk, 0, &IID_ID3D12RootSignature, (void **)&rs);
    CHECKF(FAILED(hr), "zero-length blob: %s", d3dh_hr_name(hr)); SAFE_RELEASE(rs);
    hr = ID3D12Device_CreateRootSignature(dev, 0, NULL, 16, &IID_ID3D12RootSignature, (void **)&rs);
    CHECKF(FAILED(hr), "NULL blob: %s", d3dh_hr_name(hr)); SAFE_RELEASE(rs);
}
TEST(d3d12_objects_rootsig_node_mask_and_iid) {
    NEED_DEV();
    __typeof__(&D3D12SerializeRootSignature) ser = d3dh_D3D12SerializeRootSignature();
    if (!ser) { t_skip("no serializer"); return; }
    D3D12_ROOT_SIGNATURE_DESC rd; memset(&rd, 0, sizeof rd);
    ID3DBlob *b = NULL, *e = NULL;
    EXPECT_HR(ser(&rd, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, &e), S_OK);
    if (e) ID3D10Blob_Release(e);
    if (!b) return;
    ID3D12RootSignature *rs = NULL;
    HRESULT hr = ID3D12Device_CreateRootSignature(dev, 2, ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &IID_ID3D12RootSignature, (void **)&rs);
    CHECKF(FAILED(hr), "node mask 2: %s", d3dh_hr_name(hr)); SAFE_RELEASE(rs);
    void *o = (void *)1;
    hr = ID3D12Device_CreateRootSignature(dev, 0, ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &IID_ID3D12Fence, &o);
    CHECKF(FAILED(hr), "riid Fence: %s", d3dh_hr_name(hr));
    ID3D12RootSignature *r1 = NULL, *r2 = NULL;
    EXPECT_HR(ID3D12Device_CreateRootSignature(dev, 0, ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &IID_ID3D12RootSignature, (void **)&r1), S_OK);
    EXPECT_HR(ID3D12Device_CreateRootSignature(dev, 0, ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &IID_ID3D12RootSignature, (void **)&r2), S_OK);
    if (r1 && r2 && r1 == r2) t_note("identical root signature blobs yield the same object");
    SAFE_RELEASE(r1); SAFE_RELEASE(r2);
    ID3D10Blob_Release(b);
}

/* ====== pipeline states ========================================================================================= */
TEST(d3d12_objects_pso_compute_create) {
    NEED_DEV();
    ID3DBlob *cs = d12_compile_or_null(d12_cs_iota_src, "cs_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    if (!cs || FAILED(d12_make_cs_rootsig(dev, &rs))) { t_skip("cannot compile / root signature"); goto out; }
    EXPECT_HR(d12_make_cs_pso(dev, rs, cs, &pso), S_OK);
    if (pso) {
        EXPECT_EQ(d3dh_refcount(pso), 1);
        ID3D12Device *dv = NULL; EXPECT_HR(ID3D12PipelineState_GetDevice(pso, &IID_ID3D12Device, (void **)&dv), S_OK); CHECK(dv == dev); if (dv) ID3D12Device_Release(dv);
        void *o = NULL; EXPECT_HR(ID3D12PipelineState_QueryInterface(pso, &IID_ID3D12Pageable, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    }
out:
    SAFE_RELEASE(pso); SAFE_RELEASE(rs); if (cs) ID3D10Blob_Release(cs);
}
TEST(d3d12_objects_pso_compute_invalid) {
    NEED_DEV();
    ID3DBlob *cs = d12_compile_or_null(d12_cs_iota_src, "cs_5_0");
    ID3D12RootSignature *rs = NULL, *rs_empty = NULL; ID3D12PipelineState *pso = NULL;
    if (!cs || FAILED(d12_make_cs_rootsig(dev, &rs)) || FAILED(d12_make_rootsig(dev, 0, NULL, 0, NULL, 0, &rs_empty))) { t_skip("setup"); goto out; }
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC d; memset(&d, 0, sizeof d);
        d.CS.pShaderBytecode = ID3D10Blob_GetBufferPointer(cs); d.CS.BytecodeLength = ID3D10Blob_GetBufferSize(cs);
        HRESULT hr = ID3D12Device_CreateComputePipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&pso);
        CHECKF(FAILED(hr), "no root signature: %s", d3dh_hr_name(hr)); SAFE_RELEASE(pso);
        d.pRootSignature = rs_empty;
        hr = ID3D12Device_CreateComputePipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&pso);
        CHECKF(FAILED(hr), "root signature without the UAV/CBV the shader uses: %s", d3dh_hr_name(hr)); SAFE_RELEASE(pso);
        d.pRootSignature = rs; d.CS.BytecodeLength = 0;
        hr = ID3D12Device_CreateComputePipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&pso);
        CHECKF(FAILED(hr), "empty bytecode: %s", d3dh_hr_name(hr)); SAFE_RELEASE(pso);
        static const unsigned char junk[128] = { 'N', 'O', 'T', 'D', 'X', 'B', 'C' };
        d.CS.pShaderBytecode = junk; d.CS.BytecodeLength = sizeof junk;
        hr = ID3D12Device_CreateComputePipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&pso);
        CHECKF(FAILED(hr), "garbage bytecode: %s", d3dh_hr_name(hr)); SAFE_RELEASE(pso);
        d.CS.pShaderBytecode = ID3D10Blob_GetBufferPointer(cs); d.CS.BytecodeLength = ID3D10Blob_GetBufferSize(cs); d.NodeMask = 2;
        hr = ID3D12Device_CreateComputePipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&pso);
        CHECKF(FAILED(hr), "node mask 2: %s", d3dh_hr_name(hr)); SAFE_RELEASE(pso);
        d.NodeMask = 0;
        void *o = (void *)1;
        hr = ID3D12Device_CreateComputePipelineState(dev, &d, &IID_ID3D12Fence, &o);
        CHECKF(FAILED(hr), "riid Fence: %s", d3dh_hr_name(hr));
    }
out:
    SAFE_RELEASE(pso); SAFE_RELEASE(rs); SAFE_RELEASE(rs_empty); if (cs) ID3D10Blob_Release(cs);
}
TEST(d3d12_objects_pso_compute_wrong_stage_bytecode) {
    NEED_DEV();
    ID3DBlob *ps = d12_compile_or_null(d12_ps_red_src, "ps_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    if (!ps || FAILED(d12_make_rootsig(dev, 0, NULL, 0, NULL, 0, &rs))) { t_skip("setup"); goto out; }
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC d; memset(&d, 0, sizeof d);
        d.pRootSignature = rs; d.CS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps); d.CS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);
        HRESULT hr = ID3D12Device_CreateComputePipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&pso);
        CHECKF(FAILED(hr), "pixel shader bytecode in the CS slot: %s", d3dh_hr_name(hr));
    }
out:
    SAFE_RELEASE(pso); SAFE_RELEASE(rs); if (ps) ID3D10Blob_Release(ps);
}
TEST(d3d12_objects_pso_graphics_create_and_invalid) {
    NEED_DEV();
    d12draw dr;
    if (FAILED(d12_draw_init(dev, &dr))) { t_skip("setup"); d12_draw_free(&dr); return; }
    EXPECT_EQ(d3dh_refcount(dr.pso), 1);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC g; ID3D12PipelineState *p = NULL; HRESULT hr;
    d12_fill_gfx_desc(&g, NULL, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM);
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
    CHECKF(FAILED(hr), "NULL root signature: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM); g.VS.BytecodeLength = 0; g.VS.pShaderBytecode = NULL;
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
    CHECKF(FAILED(hr), "no vertex shader: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM); g.NumRenderTargets = 1; g.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
    CHECKF(FAILED(hr), "render target count 1 with format UNKNOWN: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM); g.NumRenderTargets = 9;
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
    CHECKF(FAILED(hr), "9 render targets: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM); g.VS = g.PS;   /* pixel shader bytecode in the VS slot */
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
    CHECKF(FAILED(hr), "PS bytecode as VS: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM); g.PrimitiveTopologyType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)9;
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
    CHECKF(FAILED(hr), "topology type 9: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM); g.SampleDesc.Count = 0;
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
    t_note("graphics PSO with SampleDesc.Count = 0: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM);
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, NULL, &IID_ID3D12PipelineState, (void **)&p);
    CHECKF(FAILED(hr), "NULL desc: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    d12_draw_free(&dr);
}
TEST(d3d12_objects_pso_graphics_root_signature_mismatch) {
    NEED_DEV();
    d12draw dr;
    if (FAILED(d12_draw_init(dev, &dr))) { t_skip("setup"); d12_draw_free(&dr); return; }
    static const char *ps_tex = "Texture2D t : register(t0); SamplerState s : register(s0); float4 main() : SV_Target { return t.SampleLevel(s, float2(0.5,0.5), 0); }\n";
    ID3DBlob *ps = d12_compile_or_null(ps_tex, "ps_5_0");
    if (ps) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC g; ID3D12PipelineState *p = NULL;
        d12_fill_gfx_desc(&g, dr.rs, dr.vs, ps, DXGI_FORMAT_R8G8B8A8_UNORM);   /* empty root signature, shader needs t0 and s0 */
        HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
        CHECKF(FAILED(hr), "pixel shader needs t0/s0 but the root signature has none: %s", d3dh_hr_name(hr));
        SAFE_RELEASE(p); ID3D10Blob_Release(ps);
    }
    d12_draw_free(&dr);
}
TEST(d3d12_objects_pso_cached_blob_roundtrip) {
    NEED_DEV();
    d12draw dr;
    if (FAILED(d12_draw_init(dev, &dr))) { t_skip("setup"); d12_draw_free(&dr); return; }
    ID3DBlob *blob = NULL;
    HRESULT hr = ID3D12PipelineState_GetCachedBlob(dr.pso, &blob);
    t_note("GetCachedBlob: %s size=%llu", d3dh_hr_name(hr), blob ? (unsigned long long)ID3D10Blob_GetBufferSize(blob) : 0ull);
    if (SUCCEEDED(hr) && blob && ID3D10Blob_GetBufferSize(blob) > 0) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC g; ID3D12PipelineState *p = NULL;
        d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM);
        g.CachedPSO.pCachedBlob = ID3D10Blob_GetBufferPointer(blob); g.CachedPSO.CachedBlobSizeInBytes = ID3D10Blob_GetBufferSize(blob);
        hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
        EXPECT_HR(hr, S_OK); SAFE_RELEASE(p);
        /* damaged blob */
        size_t n = ID3D10Blob_GetBufferSize(blob); unsigned char *copy = malloc(n);
        memcpy(copy, ID3D10Blob_GetBufferPointer(blob), n);
        for (size_t i = 0; i < n; i += 7) copy[i] ^= 0xA5;
        g.CachedPSO.pCachedBlob = copy; g.CachedPSO.CachedBlobSizeInBytes = n;
        hr = ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&p);
        t_note("PSO creation with a damaged cached blob: %s", d3dh_hr_name(hr));
        CHECK(hr == S_OK || hr == D12_E_ADAPTER_NOT_FOUND || hr == D12_E_DRIVER_VERSION_MISM || hr == E_INVALIDARG);
        SAFE_RELEASE(p); free(copy);
    }
    if (blob) ID3D10Blob_Release(blob);
    d12_draw_free(&dr);
}
TEST(d3d12_objects_pso_stream_create) {
    NEED_DEV();
    ID3D12Device2 *d2 = NULL;
    if (FAILED(ID3D12Device_QueryInterface(dev, &IID_ID3D12Device2, (void **)&d2)) || !d2) { t_skip("ID3D12Device2 unavailable"); return; }
    ID3DBlob *cs = d12_compile_or_null(d12_cs_iota_src, "cs_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    if (cs && SUCCEEDED(d12_make_cs_rootsig(dev, &rs))) {
        struct { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t0; ID3D12RootSignature *rs; D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t1; D3D12_SHADER_BYTECODE cs; } st;
        memset(&st, 0, sizeof st);
        st.t0 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE; st.rs = rs;
        st.t1 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS; st.cs.pShaderBytecode = ID3D10Blob_GetBufferPointer(cs); st.cs.BytecodeLength = ID3D10Blob_GetBufferSize(cs);
        D3D12_PIPELINE_STATE_STREAM_DESC sd; sd.SizeInBytes = sizeof st; sd.pPipelineStateSubobjectStream = &st;
        EXPECT_HR(ID3D12Device2_CreatePipelineState(d2, &sd, &IID_ID3D12PipelineState, (void **)&pso), S_OK);
        SAFE_RELEASE(pso);
        st.rs = NULL;
        HRESULT hr = ID3D12Device2_CreatePipelineState(d2, &sd, &IID_ID3D12PipelineState, (void **)&pso);
        CHECKF(FAILED(hr), "stream without root signature: %s", d3dh_hr_name(hr)); SAFE_RELEASE(pso);
    } else t_skip("setup");
    SAFE_RELEASE(rs); if (cs) ID3D10Blob_Release(cs); ID3D12Device2_Release(d2);
}

/* ====== pipeline library ============================================================================================ */
TEST(d3d12_objects_pipeline_library_store_load) {
    NEED_DEV();
    ID3D12Device1 *d1 = NULL;
    if (FAILED(ID3D12Device_QueryInterface(dev, &IID_ID3D12Device1, (void **)&d1)) || !d1) { t_skip("ID3D12Device1 unavailable"); return; }
    ID3DBlob *cs = d12_compile_or_null(d12_cs_iota_src, "cs_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL; ID3D12PipelineLibrary *lib = NULL;
    if (!cs || FAILED(d12_make_cs_rootsig(dev, &rs)) || FAILED(d12_make_cs_pso(dev, rs, cs, &pso))) { t_skip("setup"); goto out; }
    {
        HRESULT hr = ID3D12Device1_CreatePipelineLibrary(d1, NULL, 0, &IID_ID3D12PipelineLibrary, (void **)&lib);
        EXPECT_HR(hr, S_OK);
        if (!lib) goto out;
        D3D12_COMPUTE_PIPELINE_STATE_DESC d; memset(&d, 0, sizeof d);
        d.pRootSignature = rs; d.CS.pShaderBytecode = ID3D10Blob_GetBufferPointer(cs); d.CS.BytecodeLength = ID3D10Blob_GetBufferSize(cs);
        ID3D12PipelineState *ld = (ID3D12PipelineState *)(ULONG_PTR)1;
        hr = ID3D12PipelineLibrary_LoadComputePipeline(lib, L"missing", &d, &IID_ID3D12PipelineState, (void **)&ld);
        EXPECT_HR(hr, E_INVALIDARG);   /* documented: name not found */
        ld = NULL;
        EXPECT_HR(ID3D12PipelineLibrary_StorePipeline(lib, L"iota", pso), S_OK);
        EXPECT_HR(ID3D12PipelineLibrary_StorePipeline(lib, L"iota", pso), E_INVALIDARG);   /* duplicate name */
        hr = ID3D12PipelineLibrary_LoadComputePipeline(lib, L"iota", &d, &IID_ID3D12PipelineState, (void **)&ld);
        EXPECT_HR(hr, S_OK);
        CHECK(ld != NULL);
        SAFE_RELEASE(ld);
        SIZE_T sz = ID3D12PipelineLibrary_GetSerializedSize(lib);
        CHECK(sz > 0);
        if (sz > 0 && sz < 64 * MB) {
            void *buf = malloc(sz);
            EXPECT_HR(ID3D12PipelineLibrary_Serialize(lib, buf, sz), S_OK);
            ID3D12PipelineLibrary *lib2 = NULL;
            hr = ID3D12Device1_CreatePipelineLibrary(d1, buf, sz, &IID_ID3D12PipelineLibrary, (void **)&lib2);
            EXPECT_HR(hr, S_OK);
            if (lib2) {
                hr = ID3D12PipelineLibrary_LoadComputePipeline(lib2, L"iota", &d, &IID_ID3D12PipelineState, (void **)&ld);
                EXPECT_HR(hr, S_OK); SAFE_RELEASE(ld);
                SAFE_RELEASE(lib2);
            }
            /* a truncated blob is rejected */
            hr = ID3D12Device1_CreatePipelineLibrary(d1, buf, sz / 2, &IID_ID3D12PipelineLibrary, (void **)&lib2);
            CHECKF(FAILED(hr), "truncated library blob: %s", d3dh_hr_name(hr));
            if (hr != D12_E_DRIVER_VERSION_MISM && hr != E_INVALIDARG && FAILED(hr)) t_note("truncated library blob -> %s", d3dh_hr_name(hr));
            SAFE_RELEASE(lib2);
            free(buf);
        }
        { char tiny[1]; EXPECT_HR(ID3D12PipelineLibrary_Serialize(lib, tiny, 1), E_INVALIDARG); }   /* buffer too small */
    }
out:
    SAFE_RELEASE(lib); SAFE_RELEASE(pso); SAFE_RELEASE(rs); if (cs) ID3D10Blob_Release(cs); ID3D12Device1_Release(d1);
}
TEST(d3d12_objects_pipeline_library_graphics) {
    NEED_DEV();
    ID3D12Device1 *d1 = NULL;
    if (FAILED(ID3D12Device_QueryInterface(dev, &IID_ID3D12Device1, (void **)&d1)) || !d1) { t_skip("ID3D12Device1 unavailable"); return; }
    d12draw dr; ID3D12PipelineLibrary *lib = NULL;
    if (FAILED(d12_draw_init(dev, &dr)) || FAILED(ID3D12Device1_CreatePipelineLibrary(d1, NULL, 0, &IID_ID3D12PipelineLibrary, (void **)&lib))) { t_skip("setup"); d12_draw_free(&dr); ID3D12Device1_Release(d1); return; }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC g; d12_fill_gfx_desc(&g, dr.rs, dr.vs, dr.ps, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR(ID3D12PipelineLibrary_StorePipeline(lib, L"gfx", dr.pso), S_OK);
    ID3D12PipelineState *ld = NULL;
    EXPECT_HR(ID3D12PipelineLibrary_LoadGraphicsPipeline(lib, L"gfx", &g, &IID_ID3D12PipelineState, (void **)&ld), S_OK);
    SAFE_RELEASE(ld);
    HRESULT hr = ID3D12PipelineLibrary_LoadGraphicsPipeline(lib, L"gfx2", &g, &IID_ID3D12PipelineState, (void **)&ld);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(ld); SAFE_RELEASE(lib); d12_draw_free(&dr); ID3D12Device1_Release(d1);
}

/* ====== compute execution =========================================================================================== */
typedef struct { ID3DBlob *cs; ID3D12RootSignature *rs; ID3D12PipelineState *pso; ID3D12Resource *buf; d12ctx c; } cstest;
static int cs_setup(ID3D12Device *dev, cstest *t, UINT bufsize) {
    memset(t, 0, sizeof *t);
    t->cs = d12_compile_or_null(d12_cs_iota_src, "cs_5_0");
    if (!t->cs || FAILED(d12_make_cs_rootsig(dev, &t->rs)) || FAILED(d12_make_cs_pso(dev, t->rs, t->cs, &t->pso))) return 0;
    if (FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, bufsize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &t->buf))) return 0;
    if (FAILED(d12_ctx_init(&t->c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) return 0;
    return 1;
}
static void cs_free(cstest *t) {
    d12_ctx_free(&t->c); SAFE_RELEASE(t->buf); SAFE_RELEASE(t->pso); SAFE_RELEASE(t->rs); if (t->cs) ID3D10Blob_Release(t->cs);
}
static void cs_bind(cstest *t, UINT k0, UINT k1) {
    ID3D12GraphicsCommandList *cl = t->c.cl;
    d12_transition(cl, t->buf, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ID3D12GraphicsCommandList_SetComputeRootSignature(cl, t->rs);
    ID3D12GraphicsCommandList_SetPipelineState(cl, t->pso);
    UINT k[4] = { k0, k1, 0, 0 };
    ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(cl, 0, 4, k, 0);
    ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(cl, 1, ID3D12Resource_GetGPUVirtualAddress(t->buf));
}
TEST(d3d12_objects_compute_dispatch_iota) {
    NEED_DEV();
    cstest t;
    if (!cs_setup(dev, &t, 4096)) { t_skip("setup"); cs_free(&t); return; }
    cs_bind(&t, 3, 1);
    ID3D12GraphicsCommandList_Dispatch(t.c.cl, 4, 1, 1);   /* 32 threads */
    EXPECT_HR(d12_run(&t.c), S_OK);
    UINT *rb = NULL; size_t sz = 0;
    EXPECT_HR(d3dh_readback_buffer12(dev, t.buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
    if (rb) {
        int ok = 1; for (UINT i = 0; i < 32; i++) if (rb[i] != i * 3 + 1) ok = 0;
        CHECK(ok);
        CHECK(rb[32] == 0 && rb[100] == 0);
        free(rb);
    }
    cs_free(&t);
}
TEST(d3d12_objects_compute_dispatch_zero_groups) {
    NEED_DEV();
    cstest t;
    if (!cs_setup(dev, &t, 4096)) { t_skip("setup"); cs_free(&t); return; }
    cs_bind(&t, 1, 7);
    ID3D12GraphicsCommandList_Dispatch(t.c.cl, 0, 1, 1);
    ID3D12GraphicsCommandList_Dispatch(t.c.cl, 4, 0, 1);
    ID3D12GraphicsCommandList_Dispatch(t.c.cl, 4, 1, 0);
    EXPECT_HR(d12_run(&t.c), S_OK);
    UINT *rb = NULL; size_t sz = 0;
    EXPECT_HR(d3dh_readback_buffer12(dev, t.buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
    if (rb) { int z = 1; for (int i = 0; i < 64; i++) if (rb[i]) z = 0; CHECK(z); free(rb); }
    cs_free(&t);
}
TEST(d3d12_objects_compute_root_constants_overwrite) {
    NEED_DEV();
    cstest t;
    if (!cs_setup(dev, &t, 4096)) { t_skip("setup"); cs_free(&t); return; }
    cs_bind(&t, 100, 100);
    ID3D12GraphicsCommandList_Dispatch(t.c.cl, 1, 1, 1);
    d12_uav_barrier(t.c.cl, t.buf);
    ID3D12GraphicsCommandList_SetComputeRoot32BitConstant(t.c.cl, 0, 5, 1);   /* only k.y changes */
    ID3D12GraphicsCommandList_SetComputeRoot32BitConstant(t.c.cl, 0, 2, 0);   /* k.x */
    ID3D12GraphicsCommandList_Dispatch(t.c.cl, 1, 1, 1);
    EXPECT_HR(d12_run(&t.c), S_OK);
    UINT *rb = NULL; size_t sz = 0;
    EXPECT_HR(d3dh_readback_buffer12(dev, t.buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
    if (rb) { int ok = 1; for (UINT i = 0; i < 8; i++) if (rb[i] != i * 2 + 5) ok = 0; CHECK(ok); free(rb); }
    cs_free(&t);
}
TEST(d3d12_objects_compute_group_ids_3d) {
    NEED_DEV();
    static const char *src =
        "RWByteAddressBuffer u : register(u0);\n"
        "[numthreads(1,1,1)] void main(uint3 g : SV_GroupID) { u.Store((g.x + g.y * 2 + g.z * 6) * 4, g.x | (g.y << 8) | (g.z << 16)); }\n";
    ID3DBlob *cs = d12_compile_or_null(src, "cs_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL; ID3D12Resource *buf = NULL; d12ctx c = {0};
    D3D12_ROOT_PARAMETER p; memset(&p, 0, sizeof p); p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    if (!cs || FAILED(d12_make_rootsig(dev, 1, &p, 0, NULL, 0, &rs)) || FAILED(d12_make_cs_pso(dev, rs, cs, &pso)) ||
        FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 1024, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &buf)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup"); goto out; }
    d12_transition(c.cl, buf, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ID3D12GraphicsCommandList_SetComputeRootSignature(c.cl, rs);
    ID3D12GraphicsCommandList_SetPipelineState(c.cl, pso);
    ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(c.cl, 0, ID3D12Resource_GetGPUVirtualAddress(buf));
    ID3D12GraphicsCommandList_Dispatch(c.cl, 2, 3, 2);
    EXPECT_HR(d12_run(&c), S_OK);
    {
        UINT *rb = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
        if (rb) {
            int ok = 1;
            for (UINT z = 0; z < 2; z++) for (UINT y = 0; y < 3; y++) for (UINT x = 0; x < 2; x++) if (rb[x + y * 2 + z * 6] != (x | (y << 8) | (z << 16))) ok = 0;
            CHECK(ok);
            free(rb);
        }
    }
out:
    d12_ctx_free(&c); SAFE_RELEASE(buf); SAFE_RELEASE(pso); SAFE_RELEASE(rs); if (cs) ID3D10Blob_Release(cs);
}
TEST(d3d12_objects_compute_descriptor_table_uav) {
    NEED_DEV();
    static const char *src = "RWByteAddressBuffer u : register(u0);\n[numthreads(4,1,1)] void main(uint3 id : SV_DispatchThreadID) { u.Store(id.x * 4, 0x1000 + id.x); }\n";
    ID3DBlob *cs = d12_compile_or_null(src, "cs_5_0");
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL; ID3D12Resource *buf = NULL; d12ctx c = {0};
    ID3D12DescriptorHeap *vis = NULL, *stage = NULL;
    D3D12_DESCRIPTOR_RANGE rg; memset(&rg, 0, sizeof rg); rg.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; rg.NumDescriptors = 1; rg.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER p; memset(&p, 0, sizeof p); p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; p.DescriptorTable.NumDescriptorRanges = 1; p.DescriptorTable.pDescriptorRanges = &rg;
    if (!cs || FAILED(d12_make_rootsig(dev, 1, &p, 0, NULL, 0, &rs)) || FAILED(d12_make_cs_pso(dev, rs, cs, &pso)) ||
        FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &buf)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) ||
        FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, 1, &vis)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, 0, &stage))) { t_skip("setup"); goto out; }
    {
        UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
        ud.Format = DXGI_FORMAT_R32_TYPELESS; ud.ViewDimension = D3D12_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 64; ud.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        ID3D12Device_CreateUnorderedAccessView(dev, buf, NULL, &ud, d12_dh_cpu(stage, 2, inc));
        ID3D12Device_CopyDescriptorsSimple(dev, 1, d12_dh_cpu(vis, 1, inc), d12_dh_cpu(stage, 2, inc), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);   /* staging -> shader visible slot 1 */
        D3D12_GPU_DESCRIPTOR_HANDLE gh = d3dh_d12_heap_gpu_start(vis); gh.ptr += inc;
        d12_transition(c.cl, buf, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        ID3D12DescriptorHeap *hh = vis;
        ID3D12GraphicsCommandList_SetDescriptorHeaps(c.cl, 1, &hh);
        ID3D12GraphicsCommandList_SetComputeRootSignature(c.cl, rs);
        ID3D12GraphicsCommandList_SetPipelineState(c.cl, pso);
        ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(c.cl, 0, gh);
        ID3D12GraphicsCommandList_Dispatch(c.cl, 3, 1, 1);
        EXPECT_HR(d12_run(&c), S_OK);
        UINT *rb = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
        if (rb) { int ok = 1; for (UINT i = 0; i < 12; i++) if (rb[i] != 0x1000 + i) ok = 0; CHECK(ok); CHECK(rb[12] == 0); free(rb); }
    }
out:
    d12_ctx_free(&c); SAFE_RELEASE(vis); SAFE_RELEASE(stage); SAFE_RELEASE(buf); SAFE_RELEASE(pso); SAFE_RELEASE(rs); if (cs) ID3D10Blob_Release(cs);
}

/* ====== ExecuteIndirect ============================================================================================ */
static HRESULT buf_from_data(ID3D12Device *dev, const void *data, UINT64 size, D3D12_RESOURCE_STATES final, ID3D12Resource **out) {
    ID3D12Resource *up = NULL; d12ctx c;
    HRESULT hr = d12_make_upload(dev, data, size, size, &up);
    if (SUCCEEDED(hr)) hr = d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, size, D3D12_RESOURCE_FLAG_NONE, out);
    if (SUCCEEDED(hr)) hr = d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT);
    else memset(&c, 0, sizeof c);
    if (SUCCEEDED(hr)) {
        d12_transition(c.cl, *out, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, *out, 0, up, 0, size);
        d12_transition(c.cl, *out, 0, D3D12_RESOURCE_STATE_COPY_DEST, final);
        hr = d12_run(&c);
    }
    d12_ctx_free(&c); SAFE_RELEASE(up);
    return hr;
}
TEST(d3d12_objects_cmdsig_create_valid) {
    NEED_DEV();
    static const D3D12_INDIRECT_ARGUMENT_TYPE ty[] = { D3D12_INDIRECT_ARGUMENT_TYPE_DRAW, D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED, D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH };
    static const UINT stride[] = { 16, 20, 12 };
    for (int i = 0; i < 3; i++) {
        D3D12_INDIRECT_ARGUMENT_DESC a; memset(&a, 0, sizeof a); a.Type = ty[i];
        D3D12_COMMAND_SIGNATURE_DESC d; memset(&d, 0, sizeof d); d.ByteStride = stride[i]; d.NumArgumentDescs = 1; d.pArgumentDescs = &a;
        ID3D12CommandSignature *cs = NULL;
        HRESULT hr = ID3D12Device_CreateCommandSignature(dev, &d, NULL, &IID_ID3D12CommandSignature, (void **)&cs);
        CHECKF(hr == S_OK, "signature type %d: %s", ty[i], d3dh_hr_name(hr));
        if (cs) { EXPECT_EQ(d3dh_refcount(cs), 1); void *o = NULL; EXPECT_HR(ID3D12CommandSignature_QueryInterface(cs, &IID_ID3D12DeviceChild, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o); }
        SAFE_RELEASE(cs);
    }
}
TEST(d3d12_objects_cmdsig_create_invalid) {
    NEED_DEV();
    D3D12_INDIRECT_ARGUMENT_DESC a[2]; memset(a, 0, sizeof a);
    D3D12_COMMAND_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    ID3D12CommandSignature *cs = NULL; HRESULT hr;
    a[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW; d.ByteStride = 16; d.NumArgumentDescs = 0; d.pArgumentDescs = a;
    hr = ID3D12Device_CreateCommandSignature(dev, &d, NULL, &IID_ID3D12CommandSignature, (void **)&cs);
    CHECKF(FAILED(hr), "zero arguments: %s", d3dh_hr_name(hr)); SAFE_RELEASE(cs);
    d.NumArgumentDescs = 1; d.ByteStride = 8;
    hr = ID3D12Device_CreateCommandSignature(dev, &d, NULL, &IID_ID3D12CommandSignature, (void **)&cs);
    CHECKF(FAILED(hr), "stride 8 < 16 bytes of draw args: %s", d3dh_hr_name(hr)); SAFE_RELEASE(cs);
    d.ByteStride = 0;
    hr = ID3D12Device_CreateCommandSignature(dev, &d, NULL, &IID_ID3D12CommandSignature, (void **)&cs);
    CHECKF(FAILED(hr), "stride 0: %s", d3dh_hr_name(hr)); SAFE_RELEASE(cs);
    /* draw must be the last argument */
    a[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW; a[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH; d.NumArgumentDescs = 2; d.ByteStride = 64;
    hr = ID3D12Device_CreateCommandSignature(dev, &d, NULL, &IID_ID3D12CommandSignature, (void **)&cs);
    CHECKF(FAILED(hr), "draw followed by dispatch: %s", d3dh_hr_name(hr)); SAFE_RELEASE(cs);
    /* root-binding arguments need a root signature */
    a[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT; a[0].Constant.RootParameterIndex = 0; a[0].Constant.DestOffsetIn32BitValues = 0; a[0].Constant.Num32BitValuesToSet = 1;
    a[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH; d.ByteStride = 16;
    hr = ID3D12Device_CreateCommandSignature(dev, &d, NULL, &IID_ID3D12CommandSignature, (void **)&cs);
    CHECKF(FAILED(hr), "constant argument without root signature: %s", d3dh_hr_name(hr)); SAFE_RELEASE(cs);
    hr = ID3D12Device_CreateCommandSignature(dev, NULL, NULL, &IID_ID3D12CommandSignature, (void **)&cs);
    CHECKF(FAILED(hr), "NULL desc: %s", d3dh_hr_name(hr)); SAFE_RELEASE(cs);
}
TEST(d3d12_objects_execute_indirect_draw) {
    NEED_DEV();
    d12draw dr; d12ctx c = {0}; ID3D12CommandSignature *sig = NULL; ID3D12Resource *args = NULL;
    if (FAILED(d12_draw_init(dev, &dr)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup"); goto out; }
    {
        D3D12_INDIRECT_ARGUMENT_DESC a; memset(&a, 0, sizeof a); a.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        D3D12_COMMAND_SIGNATURE_DESC d; memset(&d, 0, sizeof d); d.ByteStride = 16; d.NumArgumentDescs = 1; d.pArgumentDescs = &a;
        if (FAILED(ID3D12Device_CreateCommandSignature(dev, &d, NULL, &IID_ID3D12CommandSignature, (void **)&sig))) { t_skip("no signature"); goto out; }
        UINT cmds[8] = { 3, 1, 0, 0,   3, 1, 3, 0 };   /* command 0 draws, command 1 starts at vertex 3 (off-screen) */
        if (FAILED(buf_from_data(dev, cmds, sizeof cmds, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, &args))) { t_skip("no args"); goto out; }
        d12_draw_setup(c.cl, &dr);
        d12_clear_rt(c.cl, &dr, 0, 1, 0, 1);
        ID3D12GraphicsCommandList_ExecuteIndirect(c.cl, sig, 1, args, 16, NULL, 0);   /* second command only: nothing drawn */
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s;
        if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) { EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_GREEN), 64); d12_sub_free(&s); }
        EXPECT_HR(d12_rerecord(&c), S_OK);
        d12_draw_setup(c.cl, &dr);
        d12_clear_rt(c.cl, &dr, 0, 1, 0, 1);
        ID3D12GraphicsCommandList_ExecuteIndirect(c.cl, sig, 2, args, 0, NULL, 0);     /* both: first one covers the target */
        EXPECT_HR(d12_run(&c), S_OK);
        if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) { EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_RED), 64); d12_sub_free(&s); }
    }
out:
    SAFE_RELEASE(args); SAFE_RELEASE(sig); d12_ctx_free(&c); d12_draw_free(&dr);
}
TEST(d3d12_objects_execute_indirect_constants_and_count) {
    NEED_DEV();
    cstest t; ID3D12CommandSignature *sig = NULL; ID3D12Resource *args = NULL, *cnt = NULL;
    if (!cs_setup(dev, &t, 4096)) { t_skip("setup"); goto out; }
    {
        D3D12_INDIRECT_ARGUMENT_DESC a[2]; memset(a, 0, sizeof a);
        a[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT; a[0].Constant.RootParameterIndex = 0; a[0].Constant.DestOffsetIn32BitValues = 0; a[0].Constant.Num32BitValuesToSet = 4;
        a[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC d; memset(&d, 0, sizeof d); d.ByteStride = 28; d.NumArgumentDescs = 2; d.pArgumentDescs = a;
        HRESULT hr = ID3D12Device_CreateCommandSignature(dev, &d, t.rs, &IID_ID3D12CommandSignature, (void **)&sig);
        if (FAILED(hr)) { t_note("signature with constants: %s", d3dh_hr_name(hr)); t_skip("unsupported"); goto out; }
        UINT cmds[28] = { 1, 100, 0, 0, 1, 1, 1,   2, 200, 0, 0, 1, 1, 1,   3, 300, 0, 0, 1, 1, 1,   4, 400, 0, 0, 1, 1, 1 };
        UINT count2[1] = { 2 }, count9[1] = { 9 };
        if (FAILED(buf_from_data(dev, cmds, sizeof cmds, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, &args)) || FAILED(buf_from_data(dev, count2, 4, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, &cnt))) { t_skip("buffers"); goto out; }
        cs_bind(&t, 0, 0);
        ID3D12GraphicsCommandList_ExecuteIndirect(t.c.cl, sig, 4, args, 0, cnt, 0);   /* count buffer says 2 */
        EXPECT_HR(d12_run(&t.c), S_OK);
        UINT *rb = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, t.buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
        if (rb) { int ok = 1; for (UINT i = 0; i < 8; i++) if (rb[i] != i * 2 + 200) ok = 0; CHECKF(ok, "count buffer = 2: rb[3]=%u (want 206)", rb[3]); free(rb); }
        /* a count larger than MaxCommandCount is clamped to it */
        SAFE_RELEASE(cnt);
        EXPECT_HR(buf_from_data(dev, count9, 4, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, &cnt), S_OK);
        EXPECT_HR(d12_rerecord(&t.c), S_OK);
        d12_transition(t.c.cl, t.buf, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);   /* the readback helper left it in COPY_SOURCE */
        cs_bind(&t, 0, 0);
        ID3D12GraphicsCommandList_ExecuteIndirect(t.c.cl, sig, 3, args, 0, cnt, 0);
        EXPECT_HR(d12_run(&t.c), S_OK);
        rb = NULL;
        EXPECT_HR(d3dh_readback_buffer12(dev, t.buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
        if (rb) { int ok = 1; for (UINT i = 0; i < 8; i++) if (rb[i] != i * 3 + 300) ok = 0; CHECKF(ok, "max 3 with count 9: rb[3]=%u (want 309)", rb[3]); free(rb); }
    }
out:
    SAFE_RELEASE(args); SAFE_RELEASE(cnt); SAFE_RELEASE(sig); cs_free(&t);
}
TEST(d3d12_objects_execute_indirect_dispatch_args) {
    NEED_DEV();
    cstest t; ID3D12CommandSignature *sig = NULL; ID3D12Resource *args = NULL;
    if (!cs_setup(dev, &t, 4096)) { t_skip("setup"); goto out; }
    {
        D3D12_INDIRECT_ARGUMENT_DESC a; memset(&a, 0, sizeof a); a.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC d; memset(&d, 0, sizeof d); d.ByteStride = 12; d.NumArgumentDescs = 1; d.pArgumentDescs = &a;
        if (FAILED(ID3D12Device_CreateCommandSignature(dev, &d, NULL, &IID_ID3D12CommandSignature, (void **)&sig))) { t_skip("no signature"); goto out; }
        UINT cmd[3] = { 5, 1, 1 };   /* 40 threads */
        if (FAILED(buf_from_data(dev, cmd, sizeof cmd, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, &args))) { t_skip("no args"); goto out; }
        cs_bind(&t, 1, 9);
        ID3D12GraphicsCommandList_ExecuteIndirect(t.c.cl, sig, 1, args, 0, NULL, 0);
        EXPECT_HR(d12_run(&t.c), S_OK);
        UINT *rb = NULL; size_t sz = 0;
        EXPECT_HR(d3dh_readback_buffer12(dev, t.buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
        if (rb) { int ok = 1; for (UINT i = 0; i < 40; i++) if (rb[i] != i + 9) ok = 0; CHECK(ok); CHECK(rb[40] == 0); free(rb); }
    }
out:
    SAFE_RELEASE(args); SAFE_RELEASE(sig); cs_free(&t);
}

/* ====== draw semantics (SV_VertexID / SV_InstanceID / index buffer) =================================================== */
static int draw_and_count_red(ID3D12Device *dev, int variant) {
    d12draw dr; d12ctx c = {0}; int n = -1;
    ID3D12Resource *ib = NULL;
    if (SUCCEEDED(d12_draw_init(dev, &dr)) && SUCCEEDED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) {
        d12_draw_setup(c.cl, &dr);
        d12_clear_rt(c.cl, &dr, 0, 0, 0, 1);
        switch (variant) {
        case 0: ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 0, 0); break;
        case 1: ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 3, 0); break;     /* SV_VertexID starts at StartVertexLocation: off-screen triangle */
        case 2: ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 0, 1); break;     /* SV_InstanceID does not include StartInstanceLocation */
        case 3: ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 2, 0, 0); break;     /* instance 1 is off-screen, instance 0 covers */
        case 4: ID3D12GraphicsCommandList_DrawInstanced(c.cl, 0, 1, 0, 0); break;     /* no vertices */
        case 5: ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 0, 0, 0); break;     /* no instances */
        case 6: case 7: case 8: {
            static const unsigned short idx[3] = { 0, 1, 2 };
            if (FAILED(d12_make_upload(dev, idx, sizeof idx, 256, &ib))) break;
            D3D12_INDEX_BUFFER_VIEW v; v.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(ib); v.SizeInBytes = sizeof idx; v.Format = DXGI_FORMAT_R16_UINT;
            ID3D12GraphicsCommandList_IASetIndexBuffer(c.cl, &v);
            ID3D12GraphicsCommandList_DrawIndexedInstanced(c.cl, 3, 1, 0, variant == 7 ? 3 : 0, 0);   /* BaseVertexLocation is added to SV_VertexID */
            if (variant == 8) { ID3D12GraphicsCommandList_DrawIndexedInstanced(c.cl, 3, 1, 0, -3, 0); }
            break; }
        }
        if (SUCCEEDED(d12_run(&c))) {
            d12_sub s;
            if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) { n = (int)d12_sub_count32(&s, 0, 0, 8, 8, PX_RED); d12_sub_free(&s); }
        }
    }
    SAFE_RELEASE(ib); d12_ctx_free(&c); d12_draw_free(&dr);
    return n;
}
TEST(d3d12_objects_draw_vertex_instance_ids) {
    NEED_DEV();
    d12draw dr;
    int ok = SUCCEEDED(d12_draw_init(dev, &dr)); d12_draw_free(&dr);
    if (!ok) { t_skip("setup"); return; }
    static const int want[6] = { 64, 0, 64, 64, 0, 0 };
    static const char *nm[6] = { "plain", "StartVertexLocation 3", "StartInstanceLocation 1", "2 instances", "0 vertices", "0 instances" };
    for (int v = 0; v < 6; v++) { int n = draw_and_count_red(dev, v); CHECKF(n == want[v], "%s: %d red pixels, want %d", nm[v], n, want[v]); }
}
TEST(d3d12_objects_draw_indexed_base_vertex) {
    NEED_DEV();
    d12draw dr;
    int ok = SUCCEEDED(d12_draw_init(dev, &dr)); d12_draw_free(&dr);
    if (!ok) { t_skip("setup"); return; }
    int a = draw_and_count_red(dev, 6), b = draw_and_count_red(dev, 7);
    CHECKF(a == 64, "indexed, base vertex 0: %d", a);
    CHECKF(b == 0, "indexed, base vertex 3 moves the vertex ids to the off-screen triangle: %d", b);
    int c8 = draw_and_count_red(dev, 8);
    t_note("indexed draws with BaseVertexLocation 0 then -3: %d red pixels", c8);
}
TEST(d3d12_objects_draw_with_sampled_texture) {
    NEED_DEV();
    static const char *ps_src = "Texture2D t : register(t0); SamplerState s : register(s0);\nfloat4 main() : SV_Target { return t.SampleLevel(s, float2(0.25, 0.25), 0); }\n";
    d12draw dr; d12ctx c = {0}; ID3DBlob *ps = NULL; ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    ID3D12Resource *tex = NULL; ID3D12DescriptorHeap *sv = NULL, *sh = NULL;
    memset(&dr, 0, sizeof dr);
    if (FAILED(d12_draw_init(dev, &dr)) || !(ps = d12_compile_or_null(ps_src, "ps_5_0"))) { t_skip("setup"); goto out; }
    {
        D3D12_DESCRIPTOR_RANGE r0, r1; memset(&r0, 0, sizeof r0); memset(&r1, 0, sizeof r1);
        r0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; r0.NumDescriptors = 1; r0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        r1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; r1.NumDescriptors = 1; r1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        D3D12_ROOT_PARAMETER p[2]; memset(p, 0, sizeof p);
        p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; p[0].DescriptorTable.NumDescriptorRanges = 1; p[0].DescriptorTable.pDescriptorRanges = &r0; p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; p[1].DescriptorTable.NumDescriptorRanges = 1; p[1].DescriptorTable.pDescriptorRanges = &r1; p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC g;
        if (FAILED(d12_make_rootsig(dev, 2, p, 0, NULL, 0, &rs))) { t_skip("root sig"); goto out; }
        d12_fill_gfx_desc(&g, rs, dr.vs, ps, DXGI_FORMAT_R8G8B8A8_UNORM);
        EXPECT_HR(ID3D12Device_CreateGraphicsPipelineState(dev, &g, &IID_ID3D12PipelineState, (void **)&pso), S_OK);
        if (!pso) goto out;
        UINT32 texels[4] = { 0xFF3366CC, 0xFF112233, 0xFF445566, 0xFF778899 };
        if (FAILED(d12_make_tex2d(dev, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &tex)) || FAILED(tex_upload(dev, tex, 0, texels, 8, 2, 1))) { t_skip("texture"); goto out; }
        if (FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, 1, &sv)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2, 1, &sh)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("heaps"); goto out; }
        UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), sinc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        ID3D12Device_CreateShaderResourceView(dev, tex, NULL, d12_dh_cpu(sv, 1, inc));   /* slot 1: table starts at slot 1 */
        D3D12_SAMPLER_DESC sd; memset(&sd, 0, sizeof sd); sd.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sd.MaxLOD = D3D12_FLOAT32_MAX; sd.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; sd.MaxAnisotropy = 1;
        ID3D12Device_CreateSampler(dev, &sd, d12_dh_cpu(sh, 0, sinc));
        d12_transition(c.cl, tex, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        D3D12_VIEWPORT vp = { 0, 0, D12_RT_W, D12_RT_H, 0, 1 }; D3D12_RECT sc = { 0, 0, D12_RT_W, D12_RT_H };
        ID3D12DescriptorHeap *hh[2] = { sv, sh };
        ID3D12GraphicsCommandList_SetDescriptorHeaps(c.cl, 2, hh);
        ID3D12GraphicsCommandList_SetGraphicsRootSignature(c.cl, rs);
        ID3D12GraphicsCommandList_SetPipelineState(c.cl, pso);
        D3D12_GPU_DESCRIPTOR_HANDLE g0 = d3dh_d12_heap_gpu_start(sv), g1 = d3dh_d12_heap_gpu_start(sh); g0.ptr += inc;
        ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(c.cl, 0, g0);
        ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(c.cl, 1, g1);
        ID3D12GraphicsCommandList_RSSetViewports(c.cl, 1, &vp); ID3D12GraphicsCommandList_RSSetScissorRects(c.cl, 1, &sc);
        ID3D12GraphicsCommandList_OMSetRenderTargets(c.cl, 1, &dr.rtv, FALSE, NULL);
        ID3D12GraphicsCommandList_IASetPrimitiveTopology(c.cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d12_clear_rt(c.cl, &dr, 0, 0, 0, 1);
        ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 0, 0);
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s;
        if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) {
            EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, texels[0]), 64);   /* uv (0.25, 0.25) with point sampling hits texel (0,0) */
            d12_sub_free(&s);
        }
    }
out:
    d12_ctx_free(&c); SAFE_RELEASE(sv); SAFE_RELEASE(sh); SAFE_RELEASE(tex); SAFE_RELEASE(pso); SAFE_RELEASE(rs); if (ps) ID3D10Blob_Release(ps); d12_draw_free(&dr);
}

/* ====== queries ======================================================================================================== */
TEST(d3d12_objects_query_heap_create) {
    NEED_DEV();
    static const D3D12_QUERY_HEAP_TYPE ty[] = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, D3D12_QUERY_HEAP_TYPE_TIMESTAMP, D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS, D3D12_QUERY_HEAP_TYPE_SO_STATISTICS };
    static const char *nm[] = { "OCCLUSION", "TIMESTAMP", "PIPELINE_STATISTICS", "SO_STATISTICS" };
    for (int i = 0; i < 4; i++) {
        D3D12_QUERY_HEAP_DESC d; memset(&d, 0, sizeof d); d.Type = ty[i]; d.Count = 8;
        ID3D12QueryHeap *q = NULL;
        HRESULT hr = ID3D12Device_CreateQueryHeap(dev, &d, &IID_ID3D12QueryHeap, (void **)&q);
        CHECKF(hr == S_OK, "%s: %s", nm[i], d3dh_hr_name(hr));
        if (q) { void *o = NULL; EXPECT_HR(ID3D12QueryHeap_QueryInterface(q, &IID_ID3D12Pageable, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o); ID3D12QueryHeap_Release(q); }
    }
    D3D12_QUERY_HEAP_DESC d; memset(&d, 0, sizeof d); d.Type = D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP; d.Count = 2;
    ID3D12QueryHeap *q = NULL; HRESULT hr = ID3D12Device_CreateQueryHeap(dev, &d, &IID_ID3D12QueryHeap, (void **)&q);
    t_note("COPY_QUEUE_TIMESTAMP query heap: %s", d3dh_hr_name(hr)); SAFE_RELEASE(q);
}
TEST(d3d12_objects_query_heap_invalid) {
    NEED_DEV();
    ID3D12QueryHeap *q = NULL; D3D12_QUERY_HEAP_DESC d; memset(&d, 0, sizeof d);
    d.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION; d.Count = 0;
    HRESULT hr = ID3D12Device_CreateQueryHeap(dev, &d, &IID_ID3D12QueryHeap, (void **)&q);
    CHECKF(FAILED(hr), "Count 0: %s", d3dh_hr_name(hr)); SAFE_RELEASE(q);
    d.Type = (D3D12_QUERY_HEAP_TYPE)99; d.Count = 4;
    hr = ID3D12Device_CreateQueryHeap(dev, &d, &IID_ID3D12QueryHeap, (void **)&q);
    CHECKF(FAILED(hr), "type 99: %s", d3dh_hr_name(hr)); SAFE_RELEASE(q);
    hr = ID3D12Device_CreateQueryHeap(dev, NULL, &IID_ID3D12QueryHeap, (void **)&q);
    CHECKF(FAILED(hr), "NULL desc: %s", d3dh_hr_name(hr)); SAFE_RELEASE(q);
}
TEST(d3d12_objects_query_timestamps_monotonic) {
    NEED_DEV();
    d12ctx c = {0}; ID3D12QueryHeap *q = NULL; ID3D12Resource *rb = NULL;
    D3D12_QUERY_HEAP_DESC d; memset(&d, 0, sizeof d); d.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP; d.Count = 3;
    if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) || FAILED(ID3D12Device_CreateQueryHeap(dev, &d, &IID_ID3D12QueryHeap, (void **)&q)) ||
        FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, 64, 0, &rb))) { t_skip("setup"); goto out; }
    {
        UINT64 freq = 0; EXPECT_HR(ID3D12CommandQueue_GetTimestampFrequency(c.q, &freq), S_OK); CHECK(freq > 0);
        ID3D12GraphicsCommandList_EndQuery(c.cl, q, D3D12_QUERY_TYPE_TIMESTAMP, 0);
        ID3D12GraphicsCommandList_EndQuery(c.cl, q, D3D12_QUERY_TYPE_TIMESTAMP, 1);
        ID3D12GraphicsCommandList_EndQuery(c.cl, q, D3D12_QUERY_TYPE_TIMESTAMP, 2);
        ID3D12GraphicsCommandList_ResolveQueryData(c.cl, q, D3D12_QUERY_TYPE_TIMESTAMP, 0, 3, rb, 8);   /* destination offset 8 */
        EXPECT_HR(d12_run(&c), S_OK);
        UINT64 *p = NULL; D3D12_RANGE rr = { 0, 64 };
        EXPECT_HR(ID3D12Resource_Map(rb, 0, &rr, (void **)&p), S_OK);
        if (p) {
            CHECK(p[0] == 0);   /* before the resolve destination offset */
            CHECKF(p[1] != 0 || p[2] != 0 || p[3] != 0, "all timestamps are zero");
            CHECKF(p[1] <= p[2] && p[2] <= p[3], "timestamps decrease: %llu %llu %llu", (unsigned long long)p[1], (unsigned long long)p[2], (unsigned long long)p[3]);
            D3D12_RANGE wr = { 0, 0 }; ID3D12Resource_Unmap(rb, 0, &wr);
        }
    }
out:
    SAFE_RELEASE(rb); SAFE_RELEASE(q); d12_ctx_free(&c);
}
static int run_occlusion(ID3D12Device *dev, D3D12_QUERY_TYPE qt, int vstart, UINT64 *result) {
    d12draw dr; d12ctx c = {0}; ID3D12QueryHeap *q = NULL; ID3D12Resource *rb = NULL; int ok = 0;
    D3D12_QUERY_HEAP_DESC d; memset(&d, 0, sizeof d); d.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION; d.Count = 2;
    memset(&dr, 0, sizeof dr);
    if (SUCCEEDED(d12_draw_init(dev, &dr)) && SUCCEEDED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) &&
        SUCCEEDED(ID3D12Device_CreateQueryHeap(dev, &d, &IID_ID3D12QueryHeap, (void **)&q)) && SUCCEEDED(d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, 16, 0, &rb))) {
        d12_draw_setup(c.cl, &dr);
        d12_clear_rt(c.cl, &dr, 0, 0, 0, 1);
        ID3D12GraphicsCommandList_BeginQuery(c.cl, q, qt, 1);
        ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, vstart, 0);
        ID3D12GraphicsCommandList_EndQuery(c.cl, q, qt, 1);
        ID3D12GraphicsCommandList_ResolveQueryData(c.cl, q, qt, 1, 1, rb, 0);
        if (SUCCEEDED(d12_run(&c))) {
            UINT64 *p = NULL; D3D12_RANGE rr = { 0, 8 };
            if (SUCCEEDED(ID3D12Resource_Map(rb, 0, &rr, (void **)&p)) && p) { *result = p[0]; D3D12_RANGE wr = { 0, 0 }; ID3D12Resource_Unmap(rb, 0, &wr); ok = 1; }
        }
    }
    SAFE_RELEASE(rb); SAFE_RELEASE(q); d12_ctx_free(&c); d12_draw_free(&dr);
    return ok;
}
TEST(d3d12_objects_query_occlusion_counts) {
    NEED_DEV();
    UINT64 r = 0xCD;
    if (!run_occlusion(dev, D3D12_QUERY_TYPE_OCCLUSION, 0, &r)) { t_skip("setup"); return; }
    EXPECT_EQ(r, 64);   /* every pixel of the 8x8 target passes */
    if (run_occlusion(dev, D3D12_QUERY_TYPE_OCCLUSION, 3, &r)) EXPECT_EQ(r, 0);
}
TEST(d3d12_objects_query_binary_occlusion) {
    NEED_DEV();
    UINT64 r = 0xCD;
    if (!run_occlusion(dev, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, &r)) { t_skip("setup"); return; }
    EXPECT_EQ(r, 1);
    if (run_occlusion(dev, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 3, &r)) EXPECT_EQ(r, 0);
}
TEST(d3d12_objects_query_pipeline_statistics) {
    NEED_DEV();
    d12draw dr; d12ctx c = {0}; ID3D12QueryHeap *q = NULL; ID3D12Resource *rb = NULL;
    D3D12_QUERY_HEAP_DESC d; memset(&d, 0, sizeof d); d.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS; d.Count = 1;
    memset(&dr, 0, sizeof dr);
    if (FAILED(d12_draw_init(dev, &dr)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) || FAILED(ID3D12Device_CreateQueryHeap(dev, &d, &IID_ID3D12QueryHeap, (void **)&q)) || FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, 256, 0, &rb))) { t_skip("setup"); goto out; }
    d12_draw_setup(c.cl, &dr);
    ID3D12GraphicsCommandList_BeginQuery(c.cl, q, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
    ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 2, 0, 0);
    ID3D12GraphicsCommandList_EndQuery(c.cl, q, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
    ID3D12GraphicsCommandList_ResolveQueryData(c.cl, q, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0, 1, rb, 0);
    EXPECT_HR(d12_run(&c), S_OK);
    {
        D3D12_QUERY_DATA_PIPELINE_STATISTICS *st = NULL; D3D12_RANGE rr = { 0, sizeof *st };
        EXPECT_EQ(sizeof *st, 88);
        EXPECT_HR(ID3D12Resource_Map(rb, 0, &rr, (void **)&st), S_OK);
        if (st) {
            EXPECT_EQ(st->IAVertices, 6); EXPECT_EQ(st->IAPrimitives, 2);
            EXPECT_EQ(st->PSInvocations, 64);   /* instance 1 is off-screen */
            t_note("pipeline stats: VS=%llu GS=%llu CInv=%llu CPrim=%llu HS=%llu DS=%llu CS=%llu", (unsigned long long)st->VSInvocations, (unsigned long long)st->GSInvocations,
                   (unsigned long long)st->CInvocations, (unsigned long long)st->CPrimitives, (unsigned long long)st->HSInvocations, (unsigned long long)st->DSInvocations, (unsigned long long)st->CSInvocations);
            EXPECT_EQ(st->GSInvocations, 0); EXPECT_EQ(st->HSInvocations, 0); EXPECT_EQ(st->DSInvocations, 0); EXPECT_EQ(st->CSInvocations, 0);
            D3D12_RANGE wr = { 0, 0 }; ID3D12Resource_Unmap(rb, 0, &wr);
        }
    }
out:
    SAFE_RELEASE(rb); SAFE_RELEASE(q); d12_ctx_free(&c); d12_draw_free(&dr);
}

/* ====== predication ================================================================================================= */
static int predicated_draw(ID3D12Device *dev, UINT64 value, D3D12_PREDICATION_OP op) {
    d12draw dr; d12ctx c = {0}; ID3D12Resource *pb = NULL; int n = -1;
    memset(&dr, 0, sizeof dr);
    if (SUCCEEDED(d12_draw_init(dev, &dr)) && SUCCEEDED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) &&
        SUCCEEDED(buf_from_data(dev, &value, 8, D3D12_RESOURCE_STATE_COMMON, &pb))) {
        d12_draw_setup(c.cl, &dr);
        d12_clear_rt(c.cl, &dr, 0, 1, 0, 1);
        d12_transition(c.cl, pb, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PREDICATION);
        ID3D12GraphicsCommandList_SetPredication(c.cl, pb, 0, op);
        ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 0, 0);
        ID3D12GraphicsCommandList_SetPredication(c.cl, NULL, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
        if (SUCCEEDED(d12_run(&c))) {
            d12_sub s;
            if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) { n = (int)d12_sub_count32(&s, 0, 0, 8, 8, PX_RED); d12_sub_free(&s); }
        }
    }
    SAFE_RELEASE(pb); d12_ctx_free(&c); d12_draw_free(&dr);
    return n;
}
TEST(d3d12_objects_predication_ops) {
    NEED_DEV();
    static const struct { UINT64 v; D3D12_PREDICATION_OP op; int want; const char *n; } t[] = {
        { 0, D3D12_PREDICATION_OP_EQUAL_ZERO, 64, "0 EQUAL_ZERO draws" },
        { 1, D3D12_PREDICATION_OP_EQUAL_ZERO, 0, "1 EQUAL_ZERO skips" },
        { 0, D3D12_PREDICATION_OP_NOT_EQUAL_ZERO, 0, "0 NOT_EQUAL_ZERO skips" },
        { 1, D3D12_PREDICATION_OP_NOT_EQUAL_ZERO, 64, "1 NOT_EQUAL_ZERO draws" },
        { 0x100000000ull, D3D12_PREDICATION_OP_EQUAL_ZERO, 0, "high word set, EQUAL_ZERO skips (64-bit compare)" },
        { 0x100000000ull, D3D12_PREDICATION_OP_NOT_EQUAL_ZERO, 64, "high word set, NOT_EQUAL_ZERO draws" },
        { 0xFFFFFFFFFFFFFFFFull, D3D12_PREDICATION_OP_NOT_EQUAL_ZERO, 64, "all ones, NOT_EQUAL_ZERO draws" },
    };
    d12draw dr; int ok = SUCCEEDED(d12_draw_init(dev, &dr)); d12_draw_free(&dr);
    if (!ok) { t_skip("setup"); return; }
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        int n = predicated_draw(dev, t[i].v, t[i].op);
        CHECKF(n == t[i].want, "%s: %d red pixels", t[i].n, n);
    }
}
TEST(d3d12_objects_predication_clear_unaffected_note) {
    NEED_DEV();
    d12draw dr; d12ctx c = {0}; ID3D12Resource *pb = NULL; UINT64 one = 1;
    memset(&dr, 0, sizeof dr);
    if (SUCCEEDED(d12_draw_init(dev, &dr)) && SUCCEEDED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT)) && SUCCEEDED(buf_from_data(dev, &one, 8, D3D12_RESOURCE_STATE_COMMON, &pb))) {
        d12_transition(c.cl, pb, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PREDICATION);
        d12_clear_rt(c.cl, &dr, 0, 1, 0, 1);
        ID3D12GraphicsCommandList_SetPredication(c.cl, pb, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);   /* value 1: predicated work is skipped */
        d12_clear_rt(c.cl, &dr, 0, 0, 1, 1);
        ID3D12GraphicsCommandList_SetPredication(c.cl, NULL, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s;
        if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) { t_note("ClearRenderTargetView under a skipping predicate left pixel 0x%08x (green = not predicated)", d12_sub_px32(&s, 0, 0, 0)); d12_sub_free(&s); }
    } else t_skip("setup");
    SAFE_RELEASE(pb); d12_ctx_free(&c); d12_draw_free(&dr);
}

/* ====== device-level queries ========================================================================================== */
TEST(d3d12_objects_device_check_feature_support) {
    NEED_DEV();
    D3D12_FEATURE_DATA_FEATURE_LEVELS fl; memset(&fl, 0, sizeof fl);
    static const D3D_FEATURE_LEVEL lv[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    fl.NumFeatureLevels = 5; fl.pFeatureLevelsRequested = lv;
    EXPECT_HR(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FEATURE_LEVELS, &fl, sizeof fl), S_OK);
    CHECK(fl.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0);
    D3D12_FEATURE_DATA_SHADER_MODEL sm; sm.HighestShaderModel = D3D_SHADER_MODEL_6_0;
    HRESULT hr = ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_SHADER_MODEL, &sm, sizeof sm);
    t_note("SHADER_MODEL query (6.0 requested): %s highest=0x%x", d3dh_hr_name(hr), sm.HighestShaderModel);
    D3D12_FEATURE_DATA_ARCHITECTURE ar; memset(&ar, 0, sizeof ar);
    EXPECT_HR(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_ARCHITECTURE, &ar, sizeof ar), S_OK);
    t_note("architecture: TileBasedRenderer=%d UMA=%d CacheCoherentUMA=%d", ar.TileBasedRenderer, ar.UMA, ar.CacheCoherentUMA);
    D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT va; memset(&va, 0, sizeof va);
    EXPECT_HR(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &va, sizeof va), S_OK);
    CHECK(va.MaxGPUVirtualAddressBitsPerResource >= 32 && va.MaxGPUVirtualAddressBitsPerProcess >= va.MaxGPUVirtualAddressBitsPerResource);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT fs; memset(&fs, 0, sizeof fs); fs.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    EXPECT_HR(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FORMAT_SUPPORT, &fs, sizeof fs), S_OK);
    CHECK(fs.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
    CHECK(fs.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D);
}
TEST(d3d12_objects_device_check_feature_support_invalid) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS o;
    HRESULT hr = ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_D3D12_OPTIONS, &o, sizeof o - 1);
    EXPECT_HR(hr, E_INVALIDARG);
    hr = ID3D12Device_CheckFeatureSupport(dev, (D3D12_FEATURE)9999, &o, sizeof o);
    EXPECT_HR(hr, E_INVALIDARG);
    hr = ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_D3D12_OPTIONS, NULL, sizeof o);
    CHECKF(FAILED(hr), "NULL data: %s", d3dh_hr_name(hr));
}
TEST(d3d12_objects_device_residency) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 64 * KB, 0, &r), S_OK);
    if (!r) return;
    ID3D12Pageable *pg = (ID3D12Pageable *)r;
    EXPECT_HR(ID3D12Device_Evict(dev, 1, &pg), S_OK);
    EXPECT_HR(ID3D12Device_MakeResident(dev, 1, &pg), S_OK);
    EXPECT_HR(ID3D12Device_MakeResident(dev, 1, &pg), S_OK);   /* already resident */
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_device_node_count_and_luid_consistency) {
    NEED_DEV();
    EXPECT_EQ(ID3D12Device_GetNodeCount(dev), 1);
    ID3D12Device *d2 = NULL;
    EXPECT_HR(ID3D12Device_QueryInterface(dev, &IID_ID3D12Device, (void **)&d2), S_OK);
    CHECK(d2 == dev); if (d2) ID3D12Device_Release(d2);
    static const struct { const GUID *g; const char *n; } ext[] = { { &IID_ID3D12Device1, "Device1" }, { &IID_ID3D12Device2, "Device2" }, { &IID_ID3D12Device3, "Device3" },
        { &IID_ID3D12Device4, "Device4" }, { &IID_ID3D12Device5, "Device5" }, { &IID_ID3D12Device6, "Device6" } };
    for (int i = 0; i < 6; i++) { void *o = NULL; HRESULT hr = ID3D12Device_QueryInterface(dev, ext[i].g, &o); t_note("QI ID3D12%s: %s", ext[i].n, d3dh_hr_name(hr)); if (o) ((IUnknown *)o)->lpVtbl->Release(o); }
}

/* ====== debug interfaces ================================================================================================ */
TEST(d3d12_objects_debug_interface_availability) {
    __typeof__(&D3D12GetDebugInterface) gdi = d3dh_D3D12GetDebugInterface();
    if (!gdi) { t_skip("no D3D12GetDebugInterface"); return; }
    ID3D12Debug *dbg = NULL;
    HRESULT hr = gdi(&IID_ID3D12Debug, (void **)&dbg);
    t_note("D3D12GetDebugInterface(ID3D12Debug): %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { CHECK(dbg != NULL); if (dbg) { EXPECT_EQ(d3dh_refcount(dbg), 1); ID3D12Debug_Release(dbg); } }
    else CHECKF(hr == (HRESULT)0x887A002D || hr == E_NOINTERFACE || hr == E_FAIL || hr == E_INVALIDARG, "unexpected failure %s (the Graphics Tools feature is optional)", d3dh_hr_name(hr));
    void *o = NULL;
    hr = gdi(&IID_ID3D12Fence, &o);
    CHECKF(FAILED(hr), "unrelated IID: %s", d3dh_hr_name(hr));
}
TEST(d3d12_objects_debug_info_queue_without_layer) {
    NEED_DEV();
    ID3D12InfoQueue *iq = NULL;
    HRESULT hr = ID3D12Device_QueryInterface(dev, &IID_ID3D12InfoQueue, (void **)&iq);
    t_note("QI ID3D12InfoQueue on a device created without the debug layer: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && iq) {
        UINT64 n = ID3D12InfoQueue_GetNumStoredMessages(iq);
        t_note("stored messages: %llu", (unsigned long long)n);
        ID3D12InfoQueue_Release(iq);
    }
    ID3D12DebugDevice *dd = NULL;
    hr = ID3D12Device_QueryInterface(dev, &IID_ID3D12DebugDevice, (void **)&dd);
    t_note("QI ID3D12DebugDevice: %s", d3dh_hr_name(hr));
    if (dd) ID3D12DebugDevice_Release(dd);
}
