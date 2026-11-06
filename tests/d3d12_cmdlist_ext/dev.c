/* ID3D12Device1..10 methods not covered elsewhere. */
#include "cle.h"
#include <d3d12video.h>
#define CLE_STREAM_ALIGN __attribute__((aligned(8)))

TEST(cle_device_residency_and_multi_fence) {
    NEED_DEV();
    ID3D12Device1 *d1 = cle_qi(dev, &IID_ID3D12Device1); if (!d1) { t_skip("no Device1"); return; }
    ID3D12Resource *b = NULL; EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 65536, D3D12_RESOURCE_FLAG_NONE, &b), S_OK);
    if (b) {
        ID3D12Pageable *pg[1] = { (ID3D12Pageable *)b }; D3D12_RESIDENCY_PRIORITY pr = D3D12_RESIDENCY_PRIORITY_HIGH;
        HRESULT hr = ID3D12Device1_SetResidencyPriority(d1, 1, pg, &pr);
        WARP_HR(hr, S_OK);
    }
    ID3D12Fence *f1 = NULL, *f2 = NULL;
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f1), S_OK);
    EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f2), S_OK);
    if (f1 && f2) {
        ID3D12Fence *fs[2] = { f1, f2 }; UINT64 vals[2] = { 5, 7 };
        HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
        HRESULT hr = ID3D12Device1_SetEventOnMultipleFenceCompletion(d1, fs, vals, 2, D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL, ev);
        WARP_HR(hr, S_OK);
        EXPECT_EQ(WaitForSingleObject(ev, 50), WAIT_TIMEOUT);
        ID3D12Fence_Signal(f1, 5); EXPECT_EQ(WaitForSingleObject(ev, 50), WAIT_TIMEOUT);
        ID3D12Fence_Signal(f2, 7); EXPECT_EQ(WaitForSingleObject(ev, 2000), WAIT_OBJECT_0);
        UINT64 vals2[2] = { 9, 11 }; hr = ID3D12Device1_SetEventOnMultipleFenceCompletion(d1, fs, vals2, 2, D3D12_MULTIPLE_FENCE_WAIT_FLAG_ANY, ev);
        WARP_HR(hr, S_OK);
        EXPECT_EQ(WaitForSingleObject(ev, 50), WAIT_TIMEOUT);
        ID3D12Fence_Signal(f2, 11); EXPECT_EQ(WaitForSingleObject(ev, 2000), WAIT_OBJECT_0);   /* ANY: one fence at its value is enough */
        CloseHandle(ev);
    }
    SAFE_RELEASE(f1); SAFE_RELEASE(f2); SAFE_RELEASE(b); ID3D12Device1_Release(d1);
}

TEST(cle_device3_enqueue_make_resident) {
    NEED_DEV();
    ID3D12Device3 *d3 = cle_qi(dev, &IID_ID3D12Device3); if (!d3) { t_skip("no Device3"); return; }
    ID3D12Resource *b = NULL; EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 65536, D3D12_RESOURCE_FLAG_NONE, &b), S_OK);
    ID3D12Fence *f = NULL; EXPECT_HR(ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&f), S_OK);
    if (b && f) {
        ID3D12Pageable *pg[1] = { (ID3D12Pageable *)b };
        HRESULT hr = ID3D12Device_Evict(dev, 1, pg); WARP_HR(hr, S_OK);
        hr = ID3D12Device3_EnqueueMakeResident(d3, D3D12_RESIDENCY_FLAG_NONE, 1, pg, f, 1);
        WARP_HR(hr, S_OK);
        HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL); ID3D12Fence_SetEventOnCompletion(f, 1, ev);
        EXPECT_EQ(WaitForSingleObject(ev, 5000), WAIT_OBJECT_0); CloseHandle(ev);
        hr = ID3D12Device3_EnqueueMakeResident(d3, D3D12_RESIDENCY_FLAG_DENY_OVERBUDGET, 1, pg, f, 2); WARP_HR(hr, S_OK);
        hr = ID3D12Device3_EnqueueMakeResident(d3, D3D12_RESIDENCY_FLAG_NONE, 0, pg, f, 3); WARP_HR(hr, S_OK);
        hr = ID3D12Device3_EnqueueMakeResident(d3, D3D12_RESIDENCY_FLAG_NONE, 1, pg, NULL, 4); WARP_HR(hr, S_OK);   /* a NULL fence is not validated */
    }
    SAFE_RELEASE(f); SAFE_RELEASE(b); ID3D12Device3_Release(d3);
}

TEST(cle_device4_resources_and_alloc_info1) {
    NEED_DEV();
    ID3D12Device4 *d4 = cle_qi(dev, &IID_ID3D12Device4); if (!d4) { t_skip("no Device4"); return; }
    D3D12_RESOURCE_DESC bd = d12_buf_desc(65536, D3D12_RESOURCE_FLAG_NONE);
    D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    ID3D12Resource *r = NULL;
    HRESULT hr = ID3D12Device4_CreateCommittedResource1(d4, &hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COMMON, NULL, NULL, &IID_ID3D12Resource, (void **)&r);
    WARP_HR(hr, S_OK); SAFE_RELEASE(r);
    D3D12_RESOURCE_ALLOCATION_INFO1 ai[2]; memset(ai, 0xCD, sizeof ai);
    D3D12_RESOURCE_DESC ds[2] = { bd, d12_tex2d_desc(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0) };
    D3D12_RESOURCE_ALLOCATION_INFO inf; memset(&inf, 0xCD, sizeof inf);
    ((void *(WINAPI *)(ID3D12Device4 *, D3D12_RESOURCE_ALLOCATION_INFO *, UINT, UINT, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_ALLOCATION_INFO1 *))d4->lpVtbl->GetResourceAllocationInfo1)(d4, &inf, 0, 2, ds, ai);
    WARP_EQ(inf.SizeInBytes, 131072); WARP_EQ(inf.Alignment, 65536);
    WARP_EQ(ai[0].Offset, 0); WARP_EQ(ai[0].Alignment, 65536); WARP_EQ(ai[0].SizeInBytes, 65536);
    WARP_EQ(ai[1].Offset, 65536); WARP_EQ(ai[1].Alignment, 65536); WARP_EQ(ai[1].SizeInBytes, 65536);
    D3D12_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.SizeInBytes = 65536; hd.Properties = hp;
    ID3D12Heap *h = NULL; hr = ID3D12Device4_CreateHeap1(d4, &hd, NULL, &IID_ID3D12Heap, (void **)&h); WARP_HR(hr, S_OK); SAFE_RELEASE(h);
    hr = ID3D12Device4_CreateReservedResource1(d4, &bd, D3D12_RESOURCE_STATE_COMMON, NULL, NULL, &IID_ID3D12Resource, (void **)&r); WARP_HR(hr, S_OK); SAFE_RELEASE(r);
    ID3D12ProtectedResourceSession *ps = NULL; D3D12_PROTECTED_RESOURCE_SESSION_DESC pd; memset(&pd, 0, sizeof pd);
    hr = ID3D12Device4_CreateProtectedResourceSession(d4, &pd, &IID_ID3D12ProtectedResourceSession, (void **)&ps); WARP_HR(hr, E_INVALIDARG); EXPECT_EQ(ps == NULL, 1);   /* no protected sessions on WARP */ SAFE_RELEASE(ps);
    ID3D12Device4_Release(d4);
}

TEST(cle_device5_raytracing_and_meta_commands) {
    NEED_DEV();
    ID3D12Device5 *d5 = cle_qi(dev, &IID_ID3D12Device5); if (!d5) { t_skip("no Device5"); return; }
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5; memset(&o5, 0, sizeof o5); EXPECT_HR(CFS(dev, D3D12_FEATURE_D3D12_OPTIONS5, o5), S_OK);
    if (cle_warp()) EXPECT_EQ(o5.RaytracingTier, 0);
    UINT n = 0xCDCDCDCD; HRESULT hr = ID3D12Device5_EnumerateMetaCommands(d5, &n, NULL); WARP_HR(hr, S_OK); WARP_EQ(n, 1);
    if (n == 1) { D3D12_META_COMMAND_DESC md[1]; memset(md, 0, sizeof md); UINT n2 = 1; hr = ID3D12Device5_EnumerateMetaCommands(d5, &n2, md); t_note("meta command[0]: hr=%s name=%ls id=%s", d3dh_hr_name(hr), md[0].Name ? md[0].Name : L"(null)", d3dh_guid_str(&md[0].Id)); }
    UINT np = 0xCDCDCDCD; hr = ID3D12Device5_EnumerateMetaCommandParameters(d5, &IID_ID3D12Device, D3D12_META_COMMAND_PARAMETER_STAGE_CREATION, NULL, &np, NULL); WARP_HR(hr, DXGI_ERROR_UNSUPPORTED);   /* unknown command id */
    ID3D12MetaCommand *mc = NULL; hr = ID3D12Device5_CreateMetaCommand(d5, &IID_ID3D12Device, 0, NULL, 0, &IID_ID3D12MetaCommand, (void **)&mc); WARP_HR(hr, DXGI_ERROR_UNSUPPORTED); EXPECT_EQ(mc == NULL, 1); SAFE_RELEASE(mc);
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO pi; memset(&pi, 0xCD, sizeof pi);
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS in; memset(&in, 0, sizeof in); in.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ID3D12Device5_GetRaytracingAccelerationStructurePrebuildInfo(d5, &in, &pi);
    WARP_EQ(pi.ResultDataMaxSizeInBytes, 65536); WARP_EQ(pi.ScratchDataSizeInBytes, 65536); WARP_EQ(pi.UpdateScratchDataSizeInBytes, 65536);   /* filled in although RaytracingTier is NOT_SUPPORTED */
    D3D12_STATE_OBJECT_DESC so; memset(&so, 0, sizeof so); so.Type = D3D12_STATE_OBJECT_TYPE_COLLECTION;
    ID3D12StateObject *s = NULL; hr = ID3D12Device5_CreateStateObject(d5, &so, &IID_ID3D12StateObject, (void **)&s); WARP_HR(hr, E_INVALIDARG); SAFE_RELEASE(s);
    hr = ID3D12Device5_CheckDriverMatchingIdentifier(d5, D3D12_SERIALIZED_DATA_RAYTRACING_ACCELERATION_STRUCTURE, &(D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER){ {0} }); (void)hr;
    D3D12_DRIVER_MATCHING_IDENTIFIER_STATUS st = ID3D12Device5_CheckDriverMatchingIdentifier(d5, D3D12_SERIALIZED_DATA_RAYTRACING_ACCELERATION_STRUCTURE, &(D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER){ {0} });
    WARP_EQ(st, D3D12_DRIVER_MATCHING_IDENTIFIER_UNSUPPORTED_TYPE);
    ID3D12LifetimeTracker *lt = NULL; ID3D12Device5_Release(d5); (void)lt;
}

TEST(cle_device6_to_8_methods) {
    NEED_DEV();
    ID3D12Device6 *d6 = cle_qi(dev, &IID_ID3D12Device6);
    if (d6) { HRESULT hr = ID3D12Device6_SetBackgroundProcessingMode(d6, D3D12_BACKGROUND_PROCESSING_MODE_ALLOWED, D3D12_MEASUREMENTS_ACTION_KEEP_ALL, NULL, NULL); WARP_HR(hr, S_OK); ID3D12Device6_Release(d6); }
    ID3D12Device8 *d8 = cle_qi(dev, &IID_ID3D12Device8);
    if (d8) {
        D3D12_RESOURCE_DESC1 rd; memset(&rd, 0, sizeof rd); rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width = 65536; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT); ID3D12Resource *r = NULL;
        HRESULT hr = ID3D12Device8_CreateCommittedResource2(d8, &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, NULL, NULL, &IID_ID3D12Resource, (void **)&r);
        WARP_HR(hr, S_OK); SAFE_RELEASE(r);
        D3D12_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.SizeInBytes = 131072; hd.Properties = hp; ID3D12Heap *h = NULL;
        hr = ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h);
        if (h) { hr = ID3D12Device8_CreatePlacedResource1(d8, h, 0, &rd, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r); WARP_HR(hr, S_OK); SAFE_RELEASE(r); SAFE_RELEASE(h); }
        D3D12_RESOURCE_ALLOCATION_INFO1 ai[1]; memset(ai, 0xCD, sizeof ai); D3D12_RESOURCE_ALLOCATION_INFO inf;
        ((void *(WINAPI *)(ID3D12Device8 *, D3D12_RESOURCE_ALLOCATION_INFO *, UINT, UINT, const D3D12_RESOURCE_DESC1 *, D3D12_RESOURCE_ALLOCATION_INFO1 *))d8->lpVtbl->GetResourceAllocationInfo2)(d8, &inf, 0, 1, &rd, ai);
        WARP_EQ(inf.SizeInBytes, 65536); WARP_EQ(inf.Alignment, 65536);
        D3D12_RESOURCE_DESC1 td; memset(&td, 0, sizeof td); td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width = 64; td.Height = 64; td.DepthOrArraySize = 1; td.MipLevels = 3; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp[3]; UINT rows[3]; UINT64 rsz[3], tot = 0;
        ID3D12Device8_GetCopyableFootprints1(d8, &td, 0, 3, 0, fp, rows, rsz, &tot);
        EXPECT_EQ(fp[1].Footprint.RowPitch, 256); EXPECT_EQ(rows[1], 32); EXPECT_EQ(fp[1].Offset, 16384); EXPECT_EQ(tot, 28480);   /* 64x64 RGBA8, 3 mips */
        ID3D12Device8_Release(d8);
    }
    ID3D12Device7 *d7 = cle_qi(dev, &IID_ID3D12Device7);
    if (d7) { D3D12_PROTECTED_RESOURCE_SESSION_DESC1 pd; memset(&pd, 0, sizeof pd); ID3D12ProtectedResourceSession1 *ps = NULL; HRESULT hr = ID3D12Device7_CreateProtectedResourceSession1(d7, &pd, &IID_ID3D12ProtectedResourceSession1, (void **)&ps); WARP_HR(hr, E_INVALIDARG); SAFE_RELEASE(ps); ID3D12Device7_Release(d7); }
}

/* QueryInterface ladder on the device: which interface versions WARP12 implements on Win10 22H2 (19045) */
TEST(cle_device_interface_ladder_pins) {
    NEED_DEV();
    static const struct { const char *n; const GUID *i; int warp_yes; } v[] = {
        { "Device1", &IID_ID3D12Device1, 1 }, { "Device2", &IID_ID3D12Device2, 1 }, { "Device3", &IID_ID3D12Device3, 1 }, { "Device4", &IID_ID3D12Device4, 1 },
        { "Device5", &IID_ID3D12Device5, 1 }, { "Device6", &IID_ID3D12Device6, 1 }, { "Device7", &IID_ID3D12Device7, 1 }, { "Device8", &IID_ID3D12Device8, 1 },
        { "Device9", &IID_ID3D12Device9, 0 }, { "Device10", &IID_ID3D12Device10, 0 },
        { "DeviceRemovedExtendedData", &IID_ID3D12DeviceRemovedExtendedData, 1 }, { "DeviceRemovedExtendedData1", &IID_ID3D12DeviceRemovedExtendedData1, 1 },
        { "DeviceRemovedExtendedData2", &IID_ID3D12DeviceRemovedExtendedData2, 0 },
        { "DebugDevice (no debug layer)", &IID_ID3D12DebugDevice, 0 }, { "InfoQueue (no debug layer)", &IID_ID3D12InfoQueue, 0 },
        { "Tools", &IID_ID3D12Tools, 0 }, { "SharingContract", &IID_ID3D12SharingContract, 0 },
        { "VideoDevice", &IID_ID3D12VideoDevice, 0 }, { "VideoDevice1", &IID_ID3D12VideoDevice1, 0 }, { "VideoDevice2", &IID_ID3D12VideoDevice2, 0 }, { "VideoDevice3", &IID_ID3D12VideoDevice3, 0 },   /* WARP12 has no video */
    };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        void *p = NULL; HRESULT hr = ID3D12Device_QueryInterface(dev, v[i].i, &p);
        if (v[i].warp_yes < 0) { t_note("QI %s = %s", v[i].n, d3dh_hr_name(hr)); }
        else if (cle_warp()) { CHECKF((v[i].warp_yes ? hr == S_OK : hr == E_NOINTERFACE), "QI %s = %s", v[i].n, d3dh_hr_name(hr)); }
        EXPECT_EQ(p != NULL, SUCCEEDED(hr));
        if (p) ID3D12Device_Release((ID3D12Device *)p);
    }
}

/* ID3D12PipelineLibrary / PipelineLibrary1::LoadPipeline with an empty library */
TEST(cle_pipeline_library1_load_pipeline) {
    NEED_DEV();
    ID3D12Device1 *d1 = cle_qi(dev, &IID_ID3D12Device1); if (!d1) { t_skip("no Device1"); return; }
    ID3D12PipelineLibrary *pl = NULL; HRESULT hr = ID3D12Device1_CreatePipelineLibrary(d1, NULL, 0, &IID_ID3D12PipelineLibrary, (void **)&pl);
    WARP_HR(hr, S_OK);
    if (pl) {
        ID3D12PipelineLibrary1 *p1 = NULL; hr = ID3D12PipelineLibrary_QueryInterface(pl, &IID_ID3D12PipelineLibrary1, (void **)&p1);
        WARP_HR(hr, S_OK);
        ID3DBlob *cs = d12_compile_or_null(d12_cs_iota_src, "cs_5_0"); ID3D12RootSignature *rs = NULL;
        if (p1 && cs && SUCCEEDED(d12_make_cs_rootsig(dev, &rs))) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC cd; memset(&cd, 0, sizeof cd); cd.pRootSignature = rs; cd.CS.pShaderBytecode = ID3D10Blob_GetBufferPointer(cs); cd.CS.BytecodeLength = ID3D10Blob_GetBufferSize(cs);
            D3D12_PIPELINE_STATE_STREAM_DESC sd; struct { D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t0 CLE_STREAM_ALIGN; ID3D12RootSignature *rs; D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t1 CLE_STREAM_ALIGN; D3D12_SHADER_BYTECODE cs; } st;
            memset(&st, 0, sizeof st); st.t0 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE; st.rs = rs; st.t1 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS; st.cs = cd.CS; sd.SizeInBytes = sizeof st; sd.pPipelineStateSubobjectStream = &st;
            ID3D12PipelineState *ps = NULL;
            hr = ID3D12PipelineLibrary1_LoadPipeline(p1, L"nothing", &sd, &IID_ID3D12PipelineState, (void **)&ps);
            t_note("LoadPipeline(unknown name, valid stream) = %s", d3dh_hr_name(hr));
            WARP_HR(hr, E_INVALIDARG); EXPECT_EQ(ps == NULL, 1);
            SAFE_RELEASE(ps);
        }
        SAFE_RELEASE(rs); if (cs) ID3D10Blob_Release(cs);
        if (p1) ID3D12PipelineLibrary1_Release(p1);
        ID3D12PipelineLibrary_Release(pl);
    }
    ID3D12Device1_Release(d1);
}
