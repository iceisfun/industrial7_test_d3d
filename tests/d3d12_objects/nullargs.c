/* d3d12_objects: NULL / malformed pointer arguments of the device factory methods.
 * Measured on Win10 22H2: the runtime validates NULL ppv inconsistently (S_FALSE "validate only" for some creators,
 * E_UNEXPECTED for others) and does not validate several input pointers at all (access violation). */
#include "d12o.h"

enum { N_HEAP_PPV, N_HEAP_DESC, N_COMMITTED_PPV, N_COMMITTED_HP, N_COMMITTED_DESC, N_QUEUE_DESC, N_QUEUE_PPV, N_ALLOC_PPV, N_FENCE_PPV,
       N_DHEAP_DESC, N_DHEAP_PPV, N_QHEAP_DESC, N_QHEAP_PPV, N_QHEAP_TYPE99, N_QHEAP_COUNT0, N_CMDSIG_DESC, N_CMDSIG_PPV, N_CMDSIG_ZERO_ARGS,
       N_CMDSIG_NULL_ARGS, N_LIST_NULL_ALLOC, N_GFX_PSO_DESC, N_COMP_PSO_DESC, N_ROOTSIG_NULL_BLOB, N_SHARED_HANDLE_NULL, N_PLACED_PPV,
       N_PLACED_DESC, N_PLACED_HEAP, N_RESERVED_PPV, N_RESERVED_DESC, N_COUNT };
typedef struct { int k; ID3D12Device *dev; HRESULT hr; } nc;
static void do_call(void *c) {
    nc *p = c; ID3D12Device *dev = p->dev;
    D3D12_RESOURCE_DESC d = d12_buf_desc(256, 0); D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.SizeInBytes = 64 * KB; hd.Properties = hp; hd.Alignment = 64 * KB; hd.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    D3D12_COMMAND_QUEUE_DESC qd; D3D12_DESCRIPTOR_HEAP_DESC dh; D3D12_QUERY_HEAP_DESC qh; D3D12_COMMAND_SIGNATURE_DESC cs; D3D12_INDIRECT_ARGUMENT_DESC ia;
    memset(&qd, 0, sizeof qd); memset(&dh, 0, sizeof dh); dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; dh.NumDescriptors = 4;
    memset(&qh, 0, sizeof qh); qh.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION; qh.Count = 2;
    memset(&ia, 0, sizeof ia); ia.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW; memset(&cs, 0, sizeof cs); cs.ByteStride = 16; cs.NumArgumentDescs = 1; cs.pArgumentDescs = &ia;
    ID3D12Heap *h = NULL; void *o = NULL;
    switch (p->k) {
    case N_HEAP_PPV: p->hr = ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, NULL); break;
    case N_HEAP_DESC: p->hr = ID3D12Device_CreateHeap(dev, NULL, &IID_ID3D12Heap, &o); break;
    case N_COMMITTED_PPV: p->hr = ID3D12Device_CreateCommittedResource(dev, &hp, 0, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, NULL); break;
    case N_COMMITTED_HP: p->hr = ID3D12Device_CreateCommittedResource(dev, NULL, 0, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, &o); break;
    case N_COMMITTED_DESC: p->hr = ID3D12Device_CreateCommittedResource(dev, &hp, 0, NULL, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, &o); break;
    case N_QUEUE_DESC: p->hr = ID3D12Device_CreateCommandQueue(dev, NULL, &IID_ID3D12CommandQueue, &o); break;
    case N_QUEUE_PPV: p->hr = ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, NULL); break;
    case N_ALLOC_PPV: p->hr = ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, NULL); break;
    case N_FENCE_PPV: p->hr = ID3D12Device_CreateFence(dev, 0, 0, &IID_ID3D12Fence, NULL); break;
    case N_DHEAP_DESC: p->hr = ID3D12Device_CreateDescriptorHeap(dev, NULL, &IID_ID3D12DescriptorHeap, &o); break;
    case N_DHEAP_PPV: p->hr = ID3D12Device_CreateDescriptorHeap(dev, &dh, &IID_ID3D12DescriptorHeap, NULL); break;
    case N_QHEAP_DESC: p->hr = ID3D12Device_CreateQueryHeap(dev, NULL, &IID_ID3D12QueryHeap, &o); break;
    case N_QHEAP_PPV: p->hr = ID3D12Device_CreateQueryHeap(dev, &qh, &IID_ID3D12QueryHeap, NULL); break;
    case N_QHEAP_TYPE99: qh.Type = (D3D12_QUERY_HEAP_TYPE)99; p->hr = ID3D12Device_CreateQueryHeap(dev, &qh, &IID_ID3D12QueryHeap, &o); break;
    case N_QHEAP_COUNT0: qh.Count = 0; p->hr = ID3D12Device_CreateQueryHeap(dev, &qh, &IID_ID3D12QueryHeap, &o); break;
    case N_CMDSIG_DESC: p->hr = ID3D12Device_CreateCommandSignature(dev, NULL, NULL, &IID_ID3D12CommandSignature, &o); break;
    case N_CMDSIG_PPV: p->hr = ID3D12Device_CreateCommandSignature(dev, &cs, NULL, &IID_ID3D12CommandSignature, NULL); break;
    case N_CMDSIG_ZERO_ARGS: cs.NumArgumentDescs = 0; p->hr = ID3D12Device_CreateCommandSignature(dev, &cs, NULL, &IID_ID3D12CommandSignature, &o); break;
    case N_CMDSIG_NULL_ARGS: cs.pArgumentDescs = NULL; p->hr = ID3D12Device_CreateCommandSignature(dev, &cs, NULL, &IID_ID3D12CommandSignature, &o); break;
    case N_LIST_NULL_ALLOC: p->hr = ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, NULL, NULL, &IID_ID3D12GraphicsCommandList, &o); break;
    case N_GFX_PSO_DESC: p->hr = ID3D12Device_CreateGraphicsPipelineState(dev, NULL, &IID_ID3D12PipelineState, &o); break;
    case N_COMP_PSO_DESC: p->hr = ID3D12Device_CreateComputePipelineState(dev, NULL, &IID_ID3D12PipelineState, &o); break;
    case N_ROOTSIG_NULL_BLOB: p->hr = ID3D12Device_CreateRootSignature(dev, 0, NULL, 0, &IID_ID3D12RootSignature, &o); break;
    case N_SHARED_HANDLE_NULL: p->hr = ID3D12Device_CreateSharedHandle(dev, NULL, NULL, 0, NULL, (HANDLE *)&o); break;
    case N_PLACED_PPV: case N_PLACED_DESC:
        p->hr = ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h);
        if (h) {
            p->hr = ID3D12Device_CreatePlacedResource(dev, h, 0, p->k == N_PLACED_DESC ? NULL : &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, p->k == N_PLACED_PPV ? NULL : &o);
            ID3D12Heap_Release(h);
        }
        break;
    case N_PLACED_HEAP: p->hr = ID3D12Device_CreatePlacedResource(dev, NULL, 0, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, &o); break;
    case N_RESERVED_PPV: p->hr = ID3D12Device_CreateReservedResource(dev, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, NULL); break;
    case N_RESERVED_DESC: p->hr = ID3D12Device_CreateReservedResource(dev, NULL, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, &o); break;
    }
    if (o) ((IUnknown *)o)->lpVtbl->Release((IUnknown *)o);
}
#define AV ((HRESULT)0x7AA0C5)   /* marker: expect an access violation */
static const struct { int k; HRESULT want; const char *n; } g_nc[] = {
    { N_HEAP_PPV, S_FALSE, "CreateHeap ppv NULL" }, { N_HEAP_DESC, AV, "CreateHeap desc NULL" },
    { N_COMMITTED_PPV, S_FALSE, "CreateCommittedResource ppv NULL" }, { N_COMMITTED_HP, AV, "CreateCommittedResource heap props NULL" },
    { N_COMMITTED_DESC, E_INVALIDARG, "CreateCommittedResource desc NULL" },
    { N_QUEUE_DESC, E_INVALIDARG, "CreateCommandQueue desc NULL" }, { N_QUEUE_PPV, E_UNEXPECTED, "CreateCommandQueue ppv NULL" },
    { N_ALLOC_PPV, E_UNEXPECTED, "CreateCommandAllocator ppv NULL" }, { N_FENCE_PPV, S_FALSE, "CreateFence ppv NULL" },
    { N_DHEAP_DESC, AV, "CreateDescriptorHeap desc NULL" }, { N_DHEAP_PPV, S_FALSE, "CreateDescriptorHeap ppv NULL" },
    { N_QHEAP_DESC, AV, "CreateQueryHeap desc NULL" }, { N_QHEAP_PPV, E_UNEXPECTED, "CreateQueryHeap ppv NULL" },
    { N_QHEAP_TYPE99, E_INVALIDARG, "CreateQueryHeap type 99" }, { N_QHEAP_COUNT0, E_INVALIDARG, "CreateQueryHeap count 0" },
    { N_CMDSIG_DESC, AV, "CreateCommandSignature desc NULL" }, { N_CMDSIG_PPV, E_UNEXPECTED, "CreateCommandSignature ppv NULL" },
    { N_CMDSIG_ZERO_ARGS, E_INVALIDARG, "CreateCommandSignature 0 arguments" }, { N_CMDSIG_NULL_ARGS, AV, "CreateCommandSignature pArgumentDescs NULL" },
    { N_LIST_NULL_ALLOC, E_INVALIDARG, "CreateCommandList allocator NULL" },
    { N_GFX_PSO_DESC, AV, "CreateGraphicsPipelineState desc NULL" }, { N_COMP_PSO_DESC, AV, "CreateComputePipelineState desc NULL" },
    { N_ROOTSIG_NULL_BLOB, E_FAIL, "CreateRootSignature blob NULL/0" }, { N_SHARED_HANDLE_NULL, E_INVALIDARG, "CreateSharedHandle all NULL" },
    { N_PLACED_PPV, E_UNEXPECTED, "CreatePlacedResource ppv NULL" }, { N_PLACED_DESC, E_INVALIDARG, "CreatePlacedResource desc NULL" },
    { N_PLACED_HEAP, E_INVALIDARG, "CreatePlacedResource heap NULL" },
    { N_RESERVED_PPV, E_UNEXPECTED, "CreateReservedResource ppv NULL" }, { N_RESERVED_DESC, E_INVALIDARG, "CreateReservedResource desc NULL" },
};
TEST(d3d12_objects_null_args_matrix) {
    NEED_DEV();
    for (unsigned i = 0; i < sizeof g_nc / sizeof *g_nc; i++) {
        nc c = { g_nc[i].k, dev, 0x11111111 };
        DWORD ex = t_try(do_call, &c);
        if (g_nc[i].want == AV) CHECKF(ex == 0xC0000005, "%s: exception 0x%08lx hr %s (want access violation)", g_nc[i].n, ex, d3dh_hr_name(c.hr));
        else CHECKF(ex == 0 && c.hr == g_nc[i].want, "%s: exception 0x%08lx hr %s (want %s)", g_nc[i].n, ex, d3dh_hr_name(c.hr), d3dh_hr_name(g_nc[i].want));
    }
    /* the device is still healthy after all those failures */
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(dev), S_OK);
}
