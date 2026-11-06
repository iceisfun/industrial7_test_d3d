/* Root-argument setters that no other area calls: root CBV / SRV / UAV and single 32-bit constants, compute and graphics. */
#include "cle.h"

static const char *cs_src =
    "cbuffer c0 : register(b0) { uint4 k0; };\n"
    "cbuffer c1 : register(b1) { uint4 k1; };\n"
    "ByteAddressBuffer s : register(t0);\n"
    "RWByteAddressBuffer u : register(u0);\n"
    "[numthreads(1,1,1)] void main() { uint4 v = s.Load4(0); u.Store4(0, k0 + k1 + v); u.Store4(16, uint4(k0.x, k1.y, v.z, 0xA5A5A5A5)); }\n";

static HRESULT make_rs_cs(ID3D12Device *dev, ID3D12RootSignature **out) {
    D3D12_ROOT_PARAMETER p[4]; memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p[0].Constants.ShaderRegister = 0; p[0].Constants.Num32BitValues = 4;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; p[1].Descriptor.ShaderRegister = 1;
    p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[2].Descriptor.ShaderRegister = 0;
    p[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[3].Descriptor.ShaderRegister = 0;
    for (int i = 0; i < 4; i++) p[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    return d12_make_rootsig(dev, 4, p, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE, out);
}

TEST(cle_compute_root_cbv_srv_uav_and_single_constants) {
    NEED_DEV();
    ID3DBlob *cs = d12_compile_or_null(cs_src, "cs_5_0"); if (!cs) { t_skip("no compiler"); return; }
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    EXPECT_HR(make_rs_cs(dev, &rs), S_OK);
    EXPECT_HR(d12_make_cs_pso(dev, rs, cs, &pso), S_OK);
    ID3D12Resource *cbv = NULL, *srv = NULL, *uav = NULL;
    UINT cbd[4] = { 100, 200, 300, 400 }, srd[4] = { 1000, 2000, 3000, 4000 };
    EXPECT_HR(d12_make_upload(dev, cbd, sizeof cbd, 256, &cbv), S_OK);
    EXPECT_HR(d12_make_upload(dev, srd, sizeof srd, 256, &srv), S_OK);
    EXPECT_HR(cle_uav_buf(dev, 256, &uav), S_OK);
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    if (pso && cbv && srv && uav && c.cl) {
        ID3D12GraphicsCommandList *cl = c.cl;
        ID3D12GraphicsCommandList_SetComputeRootSignature(cl, rs);
        ID3D12GraphicsCommandList_SetPipelineState(cl, pso);
        /* four single-constant writes covering all slots, set out of order */
        ID3D12GraphicsCommandList_SetComputeRoot32BitConstant(cl, 0, 4, 3);
        ID3D12GraphicsCommandList_SetComputeRoot32BitConstant(cl, 0, 1, 0);
        ID3D12GraphicsCommandList_SetComputeRoot32BitConstant(cl, 0, 2, 2);
        ID3D12GraphicsCommandList_SetComputeRoot32BitConstant(cl, 0, 3, 1);
        ID3D12GraphicsCommandList_SetComputeRootConstantBufferView(cl, 1, ID3D12Resource_GetGPUVirtualAddress(cbv));
        ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(cl, 2, ID3D12Resource_GetGPUVirtualAddress(srv));
        ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(cl, 3, ID3D12Resource_GetGPUVirtualAddress(uav));
        ID3D12GraphicsCommandList_Dispatch(cl, 1, 1, 1);
        EXPECT_HR(d12_run(&c), S_OK);
        size_t n = 0; UINT *r = cle_read(dev, uav, D3D12_RESOURCE_STATE_COMMON, &n);
        if (r && n >= 32) {
            /* k0 = {1,3,2,4} */
            EXPECT_EQ(r[0], 1 + 100 + 1000); EXPECT_EQ(r[1], 3 + 200 + 2000); EXPECT_EQ(r[2], 2 + 300 + 3000); EXPECT_EQ(r[3], 4 + 400 + 4000);
            EXPECT_EQ(r[4], 1); EXPECT_EQ(r[5], 200); EXPECT_EQ(r[6], 3000); EXPECT_EQ(r[7], 0xA5A5A5A5u);
        } else CHECK(0);
        free(r);
    }
    d12_ctx_free(&c); SAFE_RELEASE(cbv); SAFE_RELEASE(srv); SAFE_RELEASE(uav); SAFE_RELEASE(pso); SAFE_RELEASE(rs); ID3D10Blob_Release(cs);
}

/* graphics: PS output from root constants (single + array), a root CBV and a root SRV; a pixel-shader UAV write goes to a root UAV */
static const char *ps_src =
    "cbuffer c0 : register(b0) { uint2 k; };\n"
    "cbuffer c1 : register(b1) { uint4 kk; };\n"
    "ByteAddressBuffer s : register(t0);\n"
    "RWByteAddressBuffer u : register(u1);\n"
    "float4 main(float4 p : SV_Position) : SV_Target { uint v = s.Load(0); if (uint(p.x) == 0 && uint(p.y) == 0) u.Store(0, k.x * 1000 + k.y + kk.w);"
    " return float4(k.x / 255.0, kk.x / 255.0, v / 255.0, 1); }\n";

TEST(cle_graphics_root_cbv_srv_uav_and_constants) {
    NEED_DEV();
    ID3DBlob *vs = d12_compile_or_null(d12_vs_src, "vs_5_0"), *ps = d12_compile_or_null(ps_src, "ps_5_0");
    if (!vs || !ps) { t_skip("no compiler"); return; }
    D3D12_ROOT_PARAMETER p[4]; memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p[0].Constants.ShaderRegister = 0; p[0].Constants.Num32BitValues = 2;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; p[1].Descriptor.ShaderRegister = 1;
    p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[2].Descriptor.ShaderRegister = 0;
    p[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[3].Descriptor.ShaderRegister = 1;
    for (int i = 0; i < 4; i++) p[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    ID3D12RootSignature *rs = NULL; ID3D12PipelineState *pso = NULL;
    EXPECT_HR(d12_make_rootsig(dev, 4, p, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE, &rs), S_OK);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gd; if (rs) { d12_fill_gfx_desc(&gd, rs, vs, ps, DXGI_FORMAT_R8G8B8A8_UNORM); EXPECT_HR(ID3D12Device_CreateGraphicsPipelineState(dev, &gd, &IID_ID3D12PipelineState, (void **)&pso), S_OK); }
    ID3D12Resource *rt = NULL, *cbv = NULL, *srv = NULL, *uav = NULL; ID3D12DescriptorHeap *rtvh = NULL;
    EXPECT_HR(d12_make_tex2d(dev, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &rt), S_OK);
    EXPECT_HR(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, 0, &rtvh), S_OK);
    UINT cbd[4] = { 77, 0, 0, 5 }, srd[1] = { 33 };
    EXPECT_HR(d12_make_upload(dev, cbd, sizeof cbd, 256, &cbv), S_OK);
    EXPECT_HR(d12_make_upload(dev, srd, sizeof srd, 256, &srv), S_OK);
    EXPECT_HR(cle_uav_buf(dev, 256, &uav), S_OK);
    d12ctx c; EXPECT_HR(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT), S_OK);
    if (pso && rt && rtvh && cbv && srv && uav && c.cl) {
        ID3D12GraphicsCommandList *cl = c.cl;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3dh_d12_heap_cpu_start(rtvh);
        ID3D12Device_CreateRenderTargetView(dev, rt, NULL, rtv);
        D3D12_VIEWPORT vp = { 0, 0, 16, 16, 0, 1 }; D3D12_RECT sc = { 0, 0, 16, 16 };
        ID3D12GraphicsCommandList_SetGraphicsRootSignature(cl, rs);
        ID3D12GraphicsCommandList_SetPipelineState(cl, pso);
        ID3D12GraphicsCommandList_OMSetRenderTargets(cl, 1, &rtv, FALSE, NULL);
        ID3D12GraphicsCommandList_RSSetViewports(cl, 1, &vp); ID3D12GraphicsCommandList_RSSetScissorRects(cl, 1, &sc);
        ID3D12GraphicsCommandList_IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstant(cl, 0, 9, 1);        /* k.y */
        ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(cl, 0, 1, &(UINT){ 200 }, 0);  /* k.x */
        ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(cl, 1, ID3D12Resource_GetGPUVirtualAddress(cbv));
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(cl, 2, ID3D12Resource_GetGPUVirtualAddress(srv));
        ID3D12GraphicsCommandList_SetGraphicsRootUnorderedAccessView(cl, 3, ID3D12Resource_GetGPUVirtualAddress(uav));
        ID3D12GraphicsCommandList_DrawInstanced(cl, 3, 1, 0, 0);
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s; memset(&s, 0, sizeof s);
        if (SUCCEEDED(d12_readback_sub(dev, rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) {
            EXPECT_EQ(d12_sub_px32(&s, 8, 8, 0), 0xFF000000u | (33u << 16) | (77u << 8) | 200u);
            EXPECT_EQ(d12_sub_count32(&s, 0, 0, 16, 16, 0xFF000000u | (33u << 16) | (77u << 8) | 200u), 256);
            d12_sub_free(&s);
        } else CHECK(0);
        size_t n = 0; UINT *r = cle_read(dev, uav, D3D12_RESOURCE_STATE_COMMON, &n);
        if (r && n >= 4) EXPECT_EQ(r[0], 200u * 1000u + 9u + 5u); else CHECK(0);
        free(r);
    }
    d12_ctx_free(&c); SAFE_RELEASE(rt); SAFE_RELEASE(rtvh); SAFE_RELEASE(cbv); SAFE_RELEASE(srv); SAFE_RELEASE(uav); SAFE_RELEASE(pso); SAFE_RELEASE(rs);
    if (vs) ID3D10Blob_Release(vs); if (ps) ID3D10Blob_Release(ps);
}
