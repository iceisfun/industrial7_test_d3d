/* Local helpers for the d3d12_objects area (everything static: safe to include from several .c files). */
#ifndef D12O_H
#define D12O_H
#include "d3dh.h"
#include "shaders.h"

#define KB 1024ull
#define MB (1024ull * 1024ull)
#define D12_E_ADAPTER_NOT_FOUND   ((HRESULT)0x887E0001)
#define D12_E_DRIVER_VERSION_MISM ((HRESULT)0x887E0002)

/* ---- struct-returning vtable methods (x64 ABI: hidden result pointer after 'this') ---- */
static D3D12_COMMAND_QUEUE_DESC d12_queue_desc(ID3D12CommandQueue *q) {
    D3D12_COMMAND_QUEUE_DESC d; memset(&d, 0xCD, sizeof d);
    ((void *(WINAPI *)(ID3D12CommandQueue *, D3D12_COMMAND_QUEUE_DESC *))q->lpVtbl->GetDesc)(q, &d);
    return d;
}
static D3D12_HEAP_DESC d12_heap_desc(ID3D12Heap *h) {
    D3D12_HEAP_DESC d; memset(&d, 0xCD, sizeof d);
    ((void *(WINAPI *)(ID3D12Heap *, D3D12_HEAP_DESC *))h->lpVtbl->GetDesc)(h, &d);
    return d;
}
static D3D12_DESCRIPTOR_HEAP_DESC d12_dh_desc(ID3D12DescriptorHeap *h) {
    D3D12_DESCRIPTOR_HEAP_DESC d; memset(&d, 0xCD, sizeof d);
    ((void *(WINAPI *)(ID3D12DescriptorHeap *, D3D12_DESCRIPTOR_HEAP_DESC *))h->lpVtbl->GetDesc)(h, &d);
    return d;
}
static D3D12_RESOURCE_DESC d12_res_desc(ID3D12Resource *r) { return d3dh_d12_resource_desc(r); }
static D3D12_RESOURCE_ALLOCATION_INFO d12_alloc_info(ID3D12Device *dev, const D3D12_RESOURCE_DESC *d, UINT n) {
    D3D12_RESOURCE_ALLOCATION_INFO i; memset(&i, 0xCD, sizeof i);
    ((void *(WINAPI *)(ID3D12Device *, D3D12_RESOURCE_ALLOCATION_INFO *, UINT, UINT, const D3D12_RESOURCE_DESC *))dev->lpVtbl->GetResourceAllocationInfo)(dev, &i, 0, n, d);
    return i;
}
static D3D12_HEAP_PROPERTIES d12_custom_heap_props(ID3D12Device *dev, D3D12_HEAP_TYPE t) {
    D3D12_HEAP_PROPERTIES p; memset(&p, 0xCD, sizeof p);
    ((void *(WINAPI *)(ID3D12Device *, D3D12_HEAP_PROPERTIES *, UINT, D3D12_HEAP_TYPE))dev->lpVtbl->GetCustomHeapProperties)(dev, &p, 0, t);
    return p;
}

/* ---- shared device (created once, released at exit) ---------------------------------- */
static ID3D12Device *g_d12dev;
static int g_d12tried, g_d12warp;
static void d12_atexit(void) { if (g_d12dev) { ID3D12Device_Release(g_d12dev); g_d12dev = NULL; } }
/* Returns a shared WARP-preferred device or NULL (caller t_skip()s). Do not Release. */
static ID3D12Device *d12_dev(void) {
    if (!g_d12tried) {
        g_d12tried = 1;
        if (!d3dh_D3D12CreateDevice()) return NULL;
        if (SUCCEEDED(d3dh_make_device12(&g_d12dev, D3D_FEATURE_LEVEL_11_0, &g_d12warp)) && g_d12dev) atexit(d12_atexit);
        else g_d12dev = NULL;
    }
    return g_d12dev;
}
#define NEED_DEV() ID3D12Device *dev = d12_dev(); if (!dev) { t_skip("no D3D12 device"); return; }

/* ---- command context: queue + allocator + list + fence ---------------------------------- */
typedef struct {
    ID3D12Device *dev; ID3D12CommandQueue *q; ID3D12CommandAllocator *al; ID3D12GraphicsCommandList *cl;
    ID3D12Fence *fence; HANDLE ev; UINT64 val; D3D12_COMMAND_LIST_TYPE type;
} d12ctx;

static HRESULT d12_ctx_init(d12ctx *c, ID3D12Device *dev, D3D12_COMMAND_LIST_TYPE type) {
    HRESULT hr;
    D3D12_COMMAND_QUEUE_DESC qd; memset(&qd, 0, sizeof qd); qd.Type = type;
    memset(c, 0, sizeof *c); c->dev = dev; c->type = type;
    hr = ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void **)&c->q);
    if (FAILED(hr)) return hr;
    hr = ID3D12Device_CreateCommandAllocator(dev, type, &IID_ID3D12CommandAllocator, (void **)&c->al);
    if (FAILED(hr)) return hr;
    hr = ID3D12Device_CreateCommandList(dev, 0, type, c->al, NULL, &IID_ID3D12GraphicsCommandList, (void **)&c->cl);
    if (FAILED(hr)) return hr;
    hr = ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&c->fence);
    if (FAILED(hr)) return hr;
    c->ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    return c->ev ? S_OK : E_FAIL;
}
static void d12_ctx_free(d12ctx *c) {
    if (c->ev) CloseHandle(c->ev);
    if (c->fence) ID3D12Fence_Release(c->fence);
    if (c->cl) ID3D12GraphicsCommandList_Release(c->cl);
    if (c->al) ID3D12CommandAllocator_Release(c->al);
    if (c->q) ID3D12CommandQueue_Release(c->q);
    memset(c, 0, sizeof *c);
}
/* Wait until the context's fence reaches v (bounded 15 s). */
static HRESULT d12_wait(d12ctx *c, UINT64 v) {
    if (ID3D12Fence_GetCompletedValue(c->fence) >= v) return S_OK;
    HRESULT hr = ID3D12Fence_SetEventOnCompletion(c->fence, v, c->ev);
    if (FAILED(hr)) return hr;
    return WaitForSingleObject(c->ev, 15000) == WAIT_OBJECT_0 ? S_OK : HRESULT_FROM_WIN32(WAIT_TIMEOUT);
}
/* Close the list, execute it on the queue and wait for the GPU. */
static HRESULT d12_run(d12ctx *c) {
    HRESULT hr = ID3D12GraphicsCommandList_Close(c->cl);
    if (FAILED(hr)) return hr;
    ID3D12CommandList *l = (ID3D12CommandList *)c->cl;
    ID3D12CommandQueue_ExecuteCommandLists(c->q, 1, &l);
    hr = ID3D12CommandQueue_Signal(c->q, c->fence, ++c->val);
    if (FAILED(hr)) return hr;
    return d12_wait(c, c->val);
}
/* Reset allocator and list so recording can restart (call after d12_run). */
static HRESULT d12_rerecord(d12ctx *c) {
    HRESULT hr = ID3D12CommandAllocator_Reset(c->al);
    if (FAILED(hr)) return hr;
    return ID3D12GraphicsCommandList_Reset(c->cl, c->al, NULL);
}

/* ---- descriptors of resources ---------------------------------------------------------- */
static D3D12_RESOURCE_DESC d12_buf_desc(UINT64 size, D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC d; memset(&d, 0, sizeof d);
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; d.Width = size; d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1;
    d.Format = DXGI_FORMAT_UNKNOWN; d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; d.Flags = flags;
    return d;
}
static D3D12_RESOURCE_DESC d12_tex2d_desc(UINT w, UINT h, DXGI_FORMAT fmt, UINT mips, UINT arr, D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC d; memset(&d, 0, sizeof d);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; d.Width = w; d.Height = h; d.DepthOrArraySize = (UINT16)arr; d.MipLevels = (UINT16)mips;
    d.Format = fmt; d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; d.Flags = flags;
    return d;
}
static D3D12_HEAP_PROPERTIES d12_heap_props(D3D12_HEAP_TYPE t) {
    D3D12_HEAP_PROPERTIES p; memset(&p, 0, sizeof p); p.Type = t; p.CreationNodeMask = 1; p.VisibleNodeMask = 1;
    return p;
}
static HRESULT d12_create_committed(ID3D12Device *dev, D3D12_HEAP_TYPE ht, const D3D12_RESOURCE_DESC *d, D3D12_RESOURCE_STATES st,
                                    const D3D12_CLEAR_VALUE *cv, ID3D12Resource **out) {
    D3D12_HEAP_PROPERTIES hp = d12_heap_props(ht);
    return ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, d, st, cv, &IID_ID3D12Resource, (void **)out);
}
/* Buffer in the given heap type with the natural initial state for that heap. */
static HRESULT d12_make_buffer(ID3D12Device *dev, D3D12_HEAP_TYPE ht, UINT64 size, D3D12_RESOURCE_FLAGS flags, ID3D12Resource **out) {
    D3D12_RESOURCE_DESC d = d12_buf_desc(size, flags);
    D3D12_RESOURCE_STATES st = ht == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ :
                               ht == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COMMON;
    return d12_create_committed(dev, ht, &d, st, NULL, out);
}
static HRESULT d12_make_tex2d(ID3D12Device *dev, UINT w, UINT h, DXGI_FORMAT fmt, UINT mips, UINT arr, D3D12_RESOURCE_FLAGS flags,
                              D3D12_RESOURCE_STATES st, const D3D12_CLEAR_VALUE *cv, ID3D12Resource **out) {
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(w, h, fmt, mips, arr, flags);
    return d12_create_committed(dev, D3D12_HEAP_TYPE_DEFAULT, &d, st, cv, out);
}
/* Upload buffer filled with data (size bytes; rest zero when total > size). */
static HRESULT d12_make_upload(ID3D12Device *dev, const void *data, UINT64 size, UINT64 total, ID3D12Resource **out) {
    HRESULT hr = d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, total, D3D12_RESOURCE_FLAG_NONE, out);
    if (FAILED(hr)) return hr;
    void *p = NULL; D3D12_RANGE rr = { 0, 0 };
    hr = ID3D12Resource_Map(*out, 0, &rr, &p);
    if (FAILED(hr) || !p) { return FAILED(hr) ? hr : E_FAIL; }
    memset(p, 0, (size_t)total);
    if (data && size) memcpy(p, data, (size_t)size);
    D3D12_RANGE wr = { 0, (SIZE_T)total };
    ID3D12Resource_Unmap(*out, 0, &wr);
    return S_OK;
}

/* ---- barriers -------------------------------------------------------------------------- */
static void d12_transition(ID3D12GraphicsCommandList *cl, ID3D12Resource *r, UINT sub, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
    D3D12_RESOURCE_BARRIER br; memset(&br, 0, sizeof br);
    br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br.Transition.pResource = r; br.Transition.Subresource = sub;
    br.Transition.StateBefore = a; br.Transition.StateAfter = b;
    ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &br);
}
static void d12_uav_barrier(ID3D12GraphicsCommandList *cl, ID3D12Resource *r) {
    D3D12_RESOURCE_BARRIER br; memset(&br, 0, sizeof br);
    br.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; br.UAV.pResource = r;
    ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &br);
}

/* ---- footprints & raw readback ------------------------------------------------------------ */
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) / (a) * (a))

typedef struct { unsigned char *data; UINT pitch, rows, rowsize, depth; UINT64 total; } d12_sub;
/* Copy one subresource of a texture (currently in 'before') to a readback buffer and return its bytes (malloc'd, pitch-strided). */
static HRESULT d12_readback_sub(ID3D12Device *dev, ID3D12Resource *tex, UINT sub, D3D12_RESOURCE_STATES before, d12_sub *out) {
    D3D12_RESOURCE_DESC d = d12_res_desc(tex);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT rows = 0; UINT64 rowsize = 0, total = 0;
    ID3D12Device_GetCopyableFootprints(dev, &d, sub, 1, 0, &fp, &rows, &rowsize, &total);
    memset(out, 0, sizeof *out);
    ID3D12Resource *rb = NULL; d12ctx c;
    HRESULT hr = d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, total, D3D12_RESOURCE_FLAG_NONE, &rb);
    if (FAILED(hr)) return hr;
    hr = d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (SUCCEEDED(hr)) {
        D3D12_TEXTURE_COPY_LOCATION dl, sl; memset(&dl, 0, sizeof dl); memset(&sl, 0, sizeof sl);
        dl.pResource = rb; dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dl.PlacedFootprint = fp;
        sl.pResource = tex; sl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; sl.SubresourceIndex = sub;
        if (before != D3D12_RESOURCE_STATE_COPY_SOURCE) d12_transition(c.cl, tex, sub, before, D3D12_RESOURCE_STATE_COPY_SOURCE);
        ID3D12GraphicsCommandList_CopyTextureRegion(c.cl, &dl, 0, 0, 0, &sl, NULL);
        if (before != D3D12_RESOURCE_STATE_COPY_SOURCE) d12_transition(c.cl, tex, sub, D3D12_RESOURCE_STATE_COPY_SOURCE, before);
        hr = d12_run(&c);
    }
    d12_ctx_free(&c);
    if (SUCCEEDED(hr)) {
        void *p = NULL; D3D12_RANGE rr = { 0, (SIZE_T)total };
        hr = ID3D12Resource_Map(rb, 0, &rr, &p);
        if (SUCCEEDED(hr)) {
            out->data = malloc((size_t)total + 1);
            memcpy(out->data, p, (size_t)total);
            out->pitch = fp.Footprint.RowPitch; out->rows = rows; out->rowsize = (UINT)rowsize; out->depth = fp.Footprint.Depth; out->total = total;
            D3D12_RANGE wr = { 0, 0 };
            ID3D12Resource_Unmap(rb, 0, &wr);
        }
    }
    ID3D12Resource_Release(rb);
    return hr;
}
static void d12_sub_free(d12_sub *s) { free(s->data); s->data = NULL; }
static UINT32 d12_sub_px32(const d12_sub *s, UINT x, UINT y, UINT z) {
    UINT32 v; memcpy(&v, s->data + (size_t)z * s->rows * s->pitch + (size_t)y * s->pitch + (size_t)x * 4, 4); return v;
}
/* Count 32-bit pixels in [x0,x1)x[y0,y1) (slice 0) equal to want. */
static UINT d12_sub_count32(const d12_sub *s, UINT x0, UINT y0, UINT x1, UINT y1, UINT32 want) {
    UINT n = 0;
    for (UINT y = y0; y < y1; y++) for (UINT x = x0; x < x1; x++) if (d12_sub_px32(s, x, y, 0) == want) n++;
    return n;
}

/* ---- shaders ------------------------------------------------------------------------------------ */
static const char *d12_vs_src =
    "static const float2 P[6] = { float2(-1,-1), float2(-1,3), float2(3,-1), float2(5,5), float2(5,6), float2(6,5) };\n"
    "float4 main(uint vid : SV_VertexID, uint iid : SV_InstanceID) : SV_Position {\n"
    "  if (iid != 0) return float4(5,5,0,1);\n"
    "  return float4(P[vid % 6], 0, 1); }\n";
static const char *d12_ps_red_src = "float4 main() : SV_Target { return float4(1,0,0,1); }\n";
static const char *d12_cs_iota_src =
    "RWByteAddressBuffer u : register(u0);\n"
    "cbuffer c : register(b0) { uint4 k; };\n"
    "[numthreads(8,1,1)] void main(uint3 id : SV_DispatchThreadID) { u.Store(id.x * 4, id.x * k.x + k.y); }\n";

static ID3DBlob *d12_compile_or_null(const char *src, const char *target) {
    ID3DBlob *b = NULL;
    if (FAILED(d3dh_compile(src, "main", target, 0, &b))) return NULL;
    return b;
}
/* Serialized root signature -> ID3D12RootSignature. params may be NULL. */
static HRESULT d12_make_rootsig(ID3D12Device *dev, UINT nparams, const D3D12_ROOT_PARAMETER *params, UINT nstatic,
                                const D3D12_STATIC_SAMPLER_DESC *st, D3D12_ROOT_SIGNATURE_FLAGS flags, ID3D12RootSignature **out) {
    __typeof__(&D3D12SerializeRootSignature) ser = d3dh_D3D12SerializeRootSignature();
    if (!ser) return D3DH_E_NOCOMPILER;
    D3D12_ROOT_SIGNATURE_DESC rd; memset(&rd, 0, sizeof rd);
    rd.NumParameters = nparams; rd.pParameters = params; rd.NumStaticSamplers = nstatic; rd.pStaticSamplers = st; rd.Flags = flags;
    ID3DBlob *blob = NULL, *err = NULL;
    HRESULT hr = ser(&rd, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err);
    if (err) ID3D10Blob_Release(err);
    if (FAILED(hr)) return hr;
    hr = ID3D12Device_CreateRootSignature(dev, 0, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), &IID_ID3D12RootSignature, (void **)out);
    ID3D10Blob_Release(blob);
    return hr;
}
/* Root signature: [0]=4 root 32-bit constants b0, [1]=root UAV u0. */
static HRESULT d12_make_cs_rootsig(ID3D12Device *dev, ID3D12RootSignature **out) {
    D3D12_ROOT_PARAMETER p[2]; memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p[0].Constants.ShaderRegister = 0; p[0].Constants.Num32BitValues = 4;
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[1].Descriptor.ShaderRegister = 0; p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    return d12_make_rootsig(dev, 2, p, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE, out);
}
static HRESULT d12_make_cs_pso(ID3D12Device *dev, ID3D12RootSignature *rs, ID3DBlob *cs, ID3D12PipelineState **out) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC d; memset(&d, 0, sizeof d);
    d.pRootSignature = rs; d.CS.pShaderBytecode = ID3D10Blob_GetBufferPointer(cs); d.CS.BytecodeLength = ID3D10Blob_GetBufferSize(cs);
    return ID3D12Device_CreateComputePipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)out);
}
static void d12_fill_gfx_desc(D3D12_GRAPHICS_PIPELINE_STATE_DESC *d, ID3D12RootSignature *rs, ID3DBlob *vs, ID3DBlob *ps, DXGI_FORMAT rtv) {
    memset(d, 0, sizeof *d);
    d->pRootSignature = rs;
    d->VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs); d->VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
    d->PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps); d->PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);
    for (int i = 0; i < 8; i++) { d->BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        d->BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE; d->BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        d->BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE; d->BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        d->BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD; d->BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        d->BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP; }
    d->SampleMask = 0xFFFFFFFFu;
    d->RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; d->RasterizerState.CullMode = D3D12_CULL_MODE_NONE; d->RasterizerState.DepthClipEnable = TRUE;
    d->DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d->DepthStencilState.FrontFace.StencilFailOp = d->DepthStencilState.FrontFace.StencilDepthFailOp = d->DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    d->DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d->DepthStencilState.BackFace = d->DepthStencilState.FrontFace;
    d->PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d->NumRenderTargets = rtv == DXGI_FORMAT_UNKNOWN ? 0 : 1; d->RTVFormats[0] = rtv;
    d->SampleDesc.Count = 1;
}

/* Everything needed to draw the full-screen triangle into an 8x8 RGBA8 target. */
typedef struct {
    ID3D12RootSignature *rs; ID3D12PipelineState *pso; ID3D12Resource *rt; ID3D12DescriptorHeap *rtvh; D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    ID3DBlob *vs, *ps;
} d12draw;
#define D12_RT_W 8
#define D12_RT_H 8
static D3D12_CPU_DESCRIPTOR_HANDLE d12_dh_cpu(ID3D12DescriptorHeap *h, UINT idx, UINT inc) {
    D3D12_CPU_DESCRIPTOR_HANDLE c = d3dh_d12_heap_cpu_start(h); c.ptr += (SIZE_T)idx * inc; return c;
}
static HRESULT d12_draw_init(ID3D12Device *dev, d12draw *dr) {
    memset(dr, 0, sizeof *dr);
    dr->vs = d12_compile_or_null(d12_vs_src, "vs_5_0"); dr->ps = d12_compile_or_null(d12_ps_red_src, "ps_5_0");
    if (!dr->vs || !dr->ps) return D3DH_E_NOCOMPILER;
    HRESULT hr = d12_make_rootsig(dev, 0, NULL, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE, &dr->rs);
    if (FAILED(hr)) return hr;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gd; d12_fill_gfx_desc(&gd, dr->rs, dr->vs, dr->ps, DXGI_FORMAT_R8G8B8A8_UNORM);
    hr = ID3D12Device_CreateGraphicsPipelineState(dev, &gd, &IID_ID3D12PipelineState, (void **)&dr->pso);
    if (FAILED(hr)) return hr;
    D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    hr = d12_make_tex2d(dev, D12_RT_W, D12_RT_H, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, &dr->rt);
    if (FAILED(hr)) return hr;
    D3D12_DESCRIPTOR_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
    hr = ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&dr->rtvh);
    if (FAILED(hr)) return hr;
    dr->rtv = d3dh_d12_heap_cpu_start(dr->rtvh);
    ID3D12Device_CreateRenderTargetView(dev, dr->rt, NULL, dr->rtv);
    return S_OK;
}
static void d12_draw_free(d12draw *dr) {
    SAFE_RELEASE(dr->rtvh); SAFE_RELEASE(dr->rt); SAFE_RELEASE(dr->pso); SAFE_RELEASE(dr->rs);
    if (dr->vs) ID3D10Blob_Release(dr->vs);
    if (dr->ps) ID3D10Blob_Release(dr->ps);
    memset(dr, 0, sizeof *dr);
}
/* Record the state needed for draws into dr->rt (RT must be in RENDER_TARGET state). */
static void d12_draw_setup(ID3D12GraphicsCommandList *cl, d12draw *dr) {
    D3D12_VIEWPORT vp = { 0, 0, D12_RT_W, D12_RT_H, 0, 1 }; D3D12_RECT sc = { 0, 0, D12_RT_W, D12_RT_H };
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(cl, dr->rs);
    ID3D12GraphicsCommandList_SetPipelineState(cl, dr->pso);
    ID3D12GraphicsCommandList_RSSetViewports(cl, 1, &vp);
    ID3D12GraphicsCommandList_RSSetScissorRects(cl, 1, &sc);
    ID3D12GraphicsCommandList_OMSetRenderTargets(cl, 1, &dr->rtv, FALSE, NULL);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
static void d12_clear_rt(ID3D12GraphicsCommandList *cl, d12draw *dr, float r, float g, float b, float a) {
    float c[4] = { r, g, b, a };
    ID3D12GraphicsCommandList_ClearRenderTargetView(cl, dr->rtv, c, 0, NULL);
}
#define PX_BLACK 0xFF000000u
#define PX_RED   0xFF0000FFu
#define PX_GREEN 0xFF00FF00u
#define PX_BLUE  0xFFFF0000u
#define PX_ZERO  0x00000000u

/* Fill a shader-visible-or-not descriptor heap. */
static HRESULT d12_make_dheap(ID3D12Device *dev, D3D12_DESCRIPTOR_HEAP_TYPE t, UINT n, int visible, ID3D12DescriptorHeap **out) {
    D3D12_DESCRIPTOR_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.Type = t; hd.NumDescriptors = n;
    hd.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    return ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)out);
}
/* Upload tightly-packed rows (src_rowbytes each, `rows` rows, `depth` slices) into one subresource of a texture that is in COPY_DEST. */
static HRESULT tex_upload(ID3D12Device *dev, ID3D12Resource *tex, UINT sub, const void *data, UINT src_rowbytes, UINT rows, UINT depth) {
    D3D12_RESOURCE_DESC d = d12_res_desc(tex);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT nrows; UINT64 rowsize, total;
    ID3D12Device_GetCopyableFootprints(dev, &d, sub, 1, 0, &fp, &nrows, &rowsize, &total);
    ID3D12Resource *up = NULL; d12ctx c = {0};
    HRESULT hr = d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, total, 0, &up);
    if (FAILED(hr)) return hr;
    unsigned char *p = NULL; D3D12_RANGE none = { 0, 0 };
    hr = ID3D12Resource_Map(up, 0, &none, (void **)&p);
    if (FAILED(hr) || !p) { ID3D12Resource_Release(up); return FAILED(hr) ? hr : E_FAIL; }
    memset(p, 0, (size_t)total);
    for (UINT z = 0; z < depth; z++) for (UINT y = 0; y < rows; y++)
        memcpy(p + (size_t)z * nrows * fp.Footprint.RowPitch + (size_t)y * fp.Footprint.RowPitch, (const unsigned char *)data + ((size_t)z * rows + y) * src_rowbytes, src_rowbytes);
    ID3D12Resource_Unmap(up, 0, NULL);
    hr = d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (SUCCEEDED(hr)) {
        D3D12_TEXTURE_COPY_LOCATION dl, sl; memset(&dl, 0, sizeof dl); memset(&sl, 0, sizeof sl);
        dl.pResource = tex; dl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dl.SubresourceIndex = sub;
        sl.pResource = up; sl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; sl.PlacedFootprint = fp;
        ID3D12GraphicsCommandList_CopyTextureRegion(c.cl, &dl, 0, 0, 0, &sl, NULL);
        hr = d12_run(&c);
    }
    d12_ctx_free(&c);
    ID3D12Resource_Release(up);
    return hr;
}
#endif
