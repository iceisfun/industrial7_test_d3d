/* ID3D11Device5 fences, tiled resource capability reporting, and the Device1/2/3 additions. */
#include "ifv.h"

#define QE(iid, w) { &iid, #iid, w }

struct fx { ID3D11Device *d; ID3D11DeviceContext *c; D3D_FEATURE_LEVEL fl; ID3D11Device5 *d5; ID3D11DeviceContext4 *c4; };

static int fx_open(struct fx *f) {
    memset(f, 0, sizeof(*f));
    if (!ifv_dev(&f->d, &f->c, &f->fl, 0)) return 0;
    if (FAILED(ID3D11Device_QueryInterface(f->d, &IID_ID3D11Device5, (void **)&f->d5)) ||
        FAILED(ID3D11DeviceContext_QueryInterface(f->c, &IID_ID3D11DeviceContext4, (void **)&f->c4))) {
        t_skip("ID3D11Device5/ID3D11DeviceContext4 unavailable");
        SAFE_RELEASE(f->c4); SAFE_RELEASE(f->d5); SAFE_RELEASE(f->c); SAFE_RELEASE(f->d);
        return 0;
    }
    return 1;
}
static void fx_close(struct fx *f) { SAFE_RELEASE(f->c4); SAFE_RELEASE(f->d5); SAFE_RELEASE(f->c); SAFE_RELEASE(f->d); }
/* Creates a fence; skips the caller when fences are unsupported on this device. */
static ID3D11Fence *fx_fence(struct fx *f, UINT64 init, D3D11_FENCE_FLAG flags) {
    ID3D11Fence *fe = NULL;
    HRESULT hr = ID3D11Device5_CreateFence(f->d5, init, flags, &IID_ID3D11Fence, (void **)&fe);
    if (FAILED(hr) || !fe) { t_skip("CreateFence(flags 0x%x) unsupported: %s", flags, d3dh_hr_name(hr)); return NULL; }
    return fe;
}
#define FX(f) struct fx f; if (!fx_open(&f)) return

T(fence_create_invalid_flags) {
    FX(f);
    static const UINT bad[] = { 0x10, 0x20, 0x40, 0x100, 0x80000000u, 0xfffffff0u, 0xffffffffu, 4 /* cross-adapter needs SHARED */ };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        ID3D11Fence *fe = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D11Device5_CreateFence(f.d5, 0, (D3D11_FENCE_FLAG)bad[i], &IID_ID3D11Fence, (void **)&fe);
        CHECKF(hr == E_INVALIDARG, "CreateFence(flags 0x%x) = %s", bad[i], d3dh_hr_name(hr));
        CHECKF(fe == NULL, "out pointer not cleared for flags 0x%x", bad[i]);
        if (SUCCEEDED(hr)) SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_create_null_out) {
    FX(f);
    ID3D11Fence *probe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (!probe) { fx_close(&f); return; }
    SAFE_RELEASE(probe);
    /* valid arguments but no output pointer: validation only */
    EXPECT_HR(ID3D11Device5_CreateFence(f.d5, 0, D3D11_FENCE_FLAG_NONE, &IID_ID3D11Fence, NULL), S_FALSE);
    fx_close(&f);
}

T(fence_initial_value_roundtrip) {
    FX(f);
    static const UINT64 vals[] = { 0, 1, 7, 0xffffffffull, 0x100000000ull, 0x123456789abcdef0ull };
    for (unsigned i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        ID3D11Fence *fe = fx_fence(&f, vals[i], D3D11_FENCE_FLAG_NONE);
        if (!fe) break;
        UINT64 got = ID3D11Fence_GetCompletedValue(fe);
        CHECKF(got == vals[i], "initial 0x%llx -> completed 0x%llx", (unsigned long long)vals[i], (unsigned long long)got);
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_interfaces) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        static const struct ifv_qi_ent tab[] = {
            QE(IID_IUnknown, 1), QE(IID_ID3D11Fence, 1), QE(IID_ID3D11DeviceChild, 1),
            QE(IID_ID3D11Device, 0), QE(IID_ID3D11Resource, 0), QE(IID_ID3D11DeviceContext, 0), QE(IID_ID3D11Asynchronous, 0),
        };
        ifv_check_qi_table(fe, tab, sizeof(tab) / sizeof(tab[0]), "fence");
        EXPECT_EQ(d3dh_refcount(fe), 1);
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_create_with_other_iids) {
    FX(f);
    ID3D11Fence *probe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (probe) {
        SAFE_RELEASE(probe);
        void *p = NULL;
        EXPECT_HR(ID3D11Device5_CreateFence(f.d5, 0, 0, &IID_IUnknown, &p), S_OK);
        if (p) { IUnknown_Release((IUnknown *)p); p = NULL; }
        EXPECT_HR(ID3D11Device5_CreateFence(f.d5, 0, 0, &IID_ID3D11DeviceChild, &p), S_OK);
        if (p) { IUnknown_Release((IUnknown *)p); p = NULL; }
        HRESULT hr = ID3D11Device5_CreateFence(f.d5, 0, 0, &ifv_IID_Random, &p);
        CHECKF(FAILED(hr), "CreateFence(random IID) = %s", d3dh_hr_name(hr));
        CHECK(p == NULL);
        if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    }
    fx_close(&f);
}

T(fence_getdevice_and_refs) {
    FX(f);
    ULONG rc = d3dh_refcount(f.d);
    ID3D11Fence *fe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        t_note("device refcount %lu -> %lu after CreateFence", rc, d3dh_refcount(f.d));
        ID3D11Device *g = NULL;
        ID3D11Fence_GetDevice(fe, &g);
        CHECK(g == f.d);
        SAFE_RELEASE(g);
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_private_data) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        static const GUID g = {0x4f6a1c10, 0x2d3b, 0x4e57, {0x9a, 0x01, 0x6c, 0x77, 0x18, 0xb2, 0x0e, 0x55}};
        UINT v[3] = { 11, 22, 33 }, got[3] = {0}, sz = sizeof(got);
        EXPECT_HR(ID3D11Fence_SetPrivateData(fe, &g, sizeof(v), v), S_OK);
        EXPECT_HR(ID3D11Fence_GetPrivateData(fe, &g, &sz, got), S_OK);
        EXPECT_EQ(sz, sizeof(v));
        CHECK(!memcmp(v, got, sizeof(v)));
        sz = 2;
        EXPECT_HR(ID3D11Fence_GetPrivateData(fe, &g, &sz, got), DXGI_ERROR_MORE_DATA);
        EXPECT_EQ(sz, sizeof(v));
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_event_for_completed_value) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 5, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
        /* the fence is already at 5: events for 0..5 fire at once */
        for (UINT64 v = 0; v <= 5; v += 5) {
            ResetEvent(ev);
            EXPECT_HR(ID3D11Fence_SetEventOnCompletion(fe, v, ev), S_OK);
            EXPECT_EQ(WaitForSingleObject(ev, 2000), WAIT_OBJECT_0);
        }
        /* NULL event on an already completed value returns without blocking */
        EXPECT_HR(ID3D11Fence_SetEventOnCompletion(fe, 5, NULL), S_OK);
        CloseHandle(ev);
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_signal_completes_value) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
        EXPECT_HR(ID3D11Fence_SetEventOnCompletion(fe, 3, ev), S_OK);
        CHECK(WaitForSingleObject(ev, 0) == WAIT_TIMEOUT);           /* not signalled yet */
        EXPECT_HR(ID3D11DeviceContext4_Signal(f.c4, fe, 3), S_OK);
        ID3D11DeviceContext_Flush(f.c);
        DWORD w = WaitForSingleObject(ev, 5000);
        t_check(w == WAIT_OBJECT_0, __FILE__, __LINE__, "event fires after Signal", "wait=0x%lx", w);
        EXPECT_EQ(ID3D11Fence_GetCompletedValue(fe), 3);
        CloseHandle(ev);
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_signal_monotonic_sequence) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
        for (UINT64 v = 1; v <= 8; v++) {
            EXPECT_HR(ID3D11DeviceContext4_Signal(f.c4, fe, v * 10), S_OK);
            ID3D11DeviceContext_Flush(f.c);
            EXPECT_HR(ID3D11Fence_SetEventOnCompletion(fe, v * 10, ev), S_OK);
            DWORD w = WaitForSingleObject(ev, 5000);
            if (w != WAIT_OBJECT_0) { CHECKF(0, "step %llu event wait=0x%lx", (unsigned long long)v, w); break; }
            UINT64 got = ID3D11Fence_GetCompletedValue(fe);
            CHECKF(got >= v * 10, "completed %llu < %llu", (unsigned long long)got, (unsigned long long)(v * 10));
        }
        CloseHandle(ev);
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_wait_on_completed_value) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 2, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        /* GPU-side wait on a value that is already reached must not stall the queue */
        EXPECT_HR(ID3D11DeviceContext4_Wait(f.c4, fe, 2), S_OK);
        EXPECT_HR(ID3D11DeviceContext4_Wait(f.c4, fe, 1), S_OK);
        ID3D11Fence *fe2 = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
        if (fe2) {
            EXPECT_HR(ID3D11DeviceContext4_Signal(f.c4, fe2, 9), S_OK);
            EXPECT_HR(ID3D11DeviceContext4_Wait(f.c4, fe2, 9), S_OK);     /* waits for our own earlier signal */
            ID3D11DeviceContext_Flush(f.c);
            HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
            EXPECT_HR(ID3D11Fence_SetEventOnCompletion(fe2, 9, ev), S_OK);
            EXPECT_EQ(WaitForSingleObject(ev, 5000), WAIT_OBJECT_0);
            CloseHandle(ev);
            SAFE_RELEASE(fe2);
        }
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_signal_with_pending_work) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        D3D11_BUFFER_DESC bd = { 4096, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
        ID3D11Buffer *a = NULL, *b = NULL;
        ID3D11Device_CreateBuffer(f.d, &bd, NULL, &a);
        ID3D11Device_CreateBuffer(f.d, &bd, NULL, &b);
        for (int i = 0; i < 8 && a && b; i++) ID3D11DeviceContext_CopyResource(f.c, (ID3D11Resource *)b, (ID3D11Resource *)a);
        EXPECT_HR(ID3D11DeviceContext4_Signal(f.c4, fe, 1), S_OK);
        HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
        EXPECT_HR(ID3D11Fence_SetEventOnCompletion(fe, 1, ev), S_OK);
        ID3D11DeviceContext_Flush(f.c);
        EXPECT_EQ(WaitForSingleObject(ev, 5000), WAIT_OBJECT_0);
        CloseHandle(ev);
        SAFE_RELEASE(b); SAFE_RELEASE(a); SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_shared_handle_roundtrip) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 4, D3D11_FENCE_FLAG_SHARED);
    if (fe) {
        HANDLE h = NULL;
        EXPECT_HR(ID3D11Fence_CreateSharedHandle(fe, NULL, GENERIC_ALL, NULL, &h), S_OK);
        CHECK(h != NULL && h != INVALID_HANDLE_VALUE);
        if (h) {
            ID3D11Fence *fe2 = NULL;
            EXPECT_HR(ID3D11Device5_OpenSharedFence(f.d5, h, &IID_ID3D11Fence, (void **)&fe2), S_OK);
            if (fe2) {
                CHECK(fe2 != fe);
                EXPECT_EQ(ID3D11Fence_GetCompletedValue(fe2), 4);
                EXPECT_HR(ID3D11DeviceContext4_Signal(f.c4, fe, 6), S_OK);
                ID3D11DeviceContext_Flush(f.c);
                HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
                EXPECT_HR(ID3D11Fence_SetEventOnCompletion(fe2, 6, ev), S_OK);
                DWORD w = WaitForSingleObject(ev, 5000);
                t_check(w == WAIT_OBJECT_0, __FILE__, __LINE__, "opened fence observes signal", "wait=0x%lx", w);
                EXPECT_EQ(ID3D11Fence_GetCompletedValue(fe2), 6);
                CloseHandle(ev);
                SAFE_RELEASE(fe2);
            }
            CloseHandle(h);
        }
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_shared_handle_independent_lifetime) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 1, D3D11_FENCE_FLAG_SHARED);
    if (fe) {
        HANDLE h = NULL;
        ID3D11Fence_CreateSharedHandle(fe, NULL, GENERIC_ALL, NULL, &h);
        SAFE_RELEASE(fe);                      /* the handle keeps the sync object alive */
        if (h) {
            ID3D11Fence *fe2 = NULL;
            EXPECT_HR(ID3D11Device5_OpenSharedFence(f.d5, h, &IID_ID3D11Fence, (void **)&fe2), S_OK);
            if (fe2) { EXPECT_EQ(ID3D11Fence_GetCompletedValue(fe2), 1); SAFE_RELEASE(fe2); }
            CloseHandle(h);
        }
    }
    fx_close(&f);
}

T(fence_shared_handle_requires_shared_flag) {
    FX(f);
    ID3D11Fence *fe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (fe) {
        HANDLE h = NULL;
        HRESULT hr = ID3D11Fence_CreateSharedHandle(fe, NULL, GENERIC_ALL, NULL, &h);
        CHECKF(FAILED(hr), "CreateSharedHandle on a non-shared fence = %s", d3dh_hr_name(hr));
        t_note("CreateSharedHandle(non-shared) -> %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && h) CloseHandle(h);
        SAFE_RELEASE(fe);
    }
    fx_close(&f);
}

T(fence_open_shared_invalid_handles) {
    FX(f);
    ID3D11Fence *probe = fx_fence(&f, 0, D3D11_FENCE_FLAG_NONE);
    if (probe) {
        SAFE_RELEASE(probe);
        HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
        HANDLE hs[] = { NULL, INVALID_HANDLE_VALUE, ev, (HANDLE)(ULONG_PTR)0x7ffc };
        for (unsigned i = 0; i < sizeof(hs) / sizeof(hs[0]); i++) {
            ID3D11Fence *fe = (void *)(ULONG_PTR)1;
            HRESULT hr = ID3D11Device5_OpenSharedFence(f.d5, hs[i], &IID_ID3D11Fence, (void **)&fe);
            t_note("OpenSharedFence(handle #%u) -> %s", i, d3dh_hr_name(hr));
            CHECKF(FAILED(hr), "OpenSharedFence(handle #%u) succeeded", i);
            CHECKF(fe == NULL, "out pointer not cleared for handle #%u", i);
            if (SUCCEEDED(hr)) SAFE_RELEASE(fe);
        }
        CloseHandle(ev);
    }
    fx_close(&f);
}

T(fence_non_monitored) {
    FX(f);
    ID3D11Fence *fe = NULL;
    HRESULT hr = ID3D11Device5_CreateFence(f.d5, 0, D3D11_FENCE_FLAG_NON_MONITORED, &IID_ID3D11Fence, (void **)&fe);
    t_note("CreateFence(NON_MONITORED) -> %s", d3dh_hr_name(hr));
    if (FAILED(hr)) { CHECK(fe == NULL); fx_close(&f); t_skip("non-monitored fences unsupported"); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    hr = ID3D11Fence_SetEventOnCompletion(fe, 1, ev);
    t_note("SetEventOnCompletion on a non-monitored fence -> %s", d3dh_hr_name(hr));
    CloseHandle(ev);
    SAFE_RELEASE(fe);
    fx_close(&f);
}

T(fence_many_create_release) {
    FX(f);
    ULONG rc = d3dh_refcount(f.d);
    ID3D11Fence *fe[32] = {0};
    int n = 0;
    for (int i = 0; i < 32; i++) {
        if (FAILED(ID3D11Device5_CreateFence(f.d5, (UINT64)i, 0, &IID_ID3D11Fence, (void **)&fe[i]))) break;
        n++;
    }
    if (!n) t_skip("fences unsupported");
    for (int i = 0; i < n; i++) EXPECT_EQ(ID3D11Fence_GetCompletedValue(fe[i]), (UINT64)i);
    for (int i = 0; i < n; i++) SAFE_RELEASE(fe[i]);
    EXPECT_EQ(d3dh_refcount(f.d), rc);
    fx_close(&f);
}

T(feature_tiled_resources_tier) {
    IFV_DEV(d, c);
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o1; memset(&o1, 0xff, sizeof(o1));
    HRESULT hr = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS1, &o1, sizeof(o1));
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        CHECKF((int)o1.TiledResourcesTier >= D3D11_TILED_RESOURCES_NOT_SUPPORTED && (int)o1.TiledResourcesTier <= D3D11_TILED_RESOURCES_TIER_3,
               "tier %d out of range", o1.TiledResourcesTier);
        CHECK(o1.MinMaxFiltering == TRUE || o1.MinMaxFiltering == FALSE);
        CHECK(o1.ClearViewAlsoSupportsDepthOnlyFormats == TRUE || o1.ClearViewAlsoSupportsDepthOnlyFormats == FALSE);
        CHECK(o1.MapOnDefaultBuffers == TRUE || o1.MapOnDefaultBuffers == FALSE);
        t_note("FL 0x%x: tiled tier %d, minmax %d, clearview depth %d, map default buffers %d", fl_,
               o1.TiledResourcesTier, o1.MinMaxFiltering, o1.ClearViewAlsoSupportsDepthOnlyFormats, o1.MapOnDefaultBuffers);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(feature_options2_consistent_with_options1) {
    IFV_DEV(d, c);
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o1; D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2;
    memset(&o1, 0xff, sizeof(o1)); memset(&o2, 0xff, sizeof(o2));
    if (FAILED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS1, &o1, sizeof(o1))) ||
        FAILED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2)))) {
        t_skip("OPTIONS1/2 unavailable"); SAFE_RELEASE(c); SAFE_RELEASE(d); return;
    }
    EXPECT_EQ(o2.TiledResourcesTier, o1.TiledResourcesTier);
    CHECK((int)o2.ConservativeRasterizationTier >= 0 && (int)o2.ConservativeRasterizationTier <= D3D11_CONSERVATIVE_RASTERIZATION_TIER_3);
    CHECK(o2.PSSpecifiedStencilRefSupported == TRUE || o2.PSSpecifiedStencilRefSupported == FALSE);
    CHECK(o2.ROVsSupported == TRUE || o2.ROVsSupported == FALSE);
    CHECK(o2.TypedUAVLoadAdditionalFormats == TRUE || o2.TypedUAVLoadAdditionalFormats == FALSE);
    CHECK(o2.MapOnDefaultTextures == TRUE || o2.MapOnDefaultTextures == FALSE);
    CHECK(o2.StandardSwizzle == TRUE || o2.StandardSwizzle == FALSE);
    CHECK(o2.UnifiedMemoryArchitecture == TRUE || o2.UnifiedMemoryArchitecture == FALSE);
    t_note("conservative raster tier %d, ROV %d, typed UAV additional %d, map default tex %d, UMA %d",
           o2.ConservativeRasterizationTier, o2.ROVsSupported, o2.TypedUAVLoadAdditionalFormats, o2.MapOnDefaultTextures,
           o2.UnifiedMemoryArchitecture);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(feature_tiled_tier_zero_rejects_tiled_resources) {
    IFV_DEV(d, c);
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o1;
    if (FAILED(ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS1, &o1, sizeof(o1)))) { t_skip("no OPTIONS1"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_TEXTURE2D_DESC td = { 256, 256, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TILED };
    ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &t);
    t_note("tier %d: CreateTexture2D(MISC_TILED) -> %s", o1.TiledResourcesTier, d3dh_hr_name(hr));
    if (o1.TiledResourcesTier == D3D11_TILED_RESOURCES_NOT_SUPPORTED) {
        CHECKF(FAILED(hr), "tiled texture created although tiled resources are not supported");
        CHECK(t == NULL);
    }
    SAFE_RELEASE(t);
    D3D11_BUFFER_DESC bd = { 65536 * 4, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TILED, 0 };
    ID3D11Buffer *b = NULL;
    hr = ID3D11Device_CreateBuffer(d, &bd, NULL, &b);
    t_note("tier %d: CreateBuffer(MISC_TILED) -> %s", o1.TiledResourcesTier, d3dh_hr_name(hr));
    if (o1.TiledResourcesTier == D3D11_TILED_RESOURCES_NOT_SUPPORTED) CHECKF(FAILED(hr), "tiled buffer created without support");
    SAFE_RELEASE(b);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(feature_gpu_virtual_address_and_cache) {
    IFV_DEV(d, c);
    D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT va; memset(&va, 0, sizeof(va));
    HRESULT hr = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &va, sizeof(va));
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        t_note("VA bits per resource %u, per process %u", va.MaxGPUVirtualAddressBitsPerResource, va.MaxGPUVirtualAddressBitsPerProcess);
        CHECK(va.MaxGPUVirtualAddressBitsPerProcess >= va.MaxGPUVirtualAddressBitsPerResource || va.MaxGPUVirtualAddressBitsPerProcess == 0);
    }
    D3D11_FEATURE_DATA_SHADER_CACHE sc; memset(&sc, 0, sizeof(sc));
    hr = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_SHADER_CACHE, &sc, sizeof(sc));
    t_note("SHADER_CACHE -> %s support flags 0x%x", d3dh_hr_name(hr), sc.SupportFlags);
    D3D11_FEATURE_DATA_MARKER_SUPPORT ms; memset(&ms, 0, sizeof(ms));
    hr = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_MARKER_SUPPORT, &ms, sizeof(ms));
    EXPECT_HR(hr, S_OK);
    t_note("marker support %d", ms.Profile);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device2_check_multisample_quality_levels1) {
    IFV_DEV(d, c);
    ID3D11Device2 *d2 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device2, (void **)&d2))) { t_skip("no Device2"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    UINT n = 0xdead;
    EXPECT_HR(ID3D11Device2_CheckMultisampleQualityLevels1(d2, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, &n), S_OK);
    EXPECT_EQ(n, 1);
    for (UINT s = 2; s <= 8; s *= 2) {
        n = 0xdead;
        HRESULT hr = ID3D11Device2_CheckMultisampleQualityLevels1(d2, DXGI_FORMAT_R8G8B8A8_UNORM, s, 0, &n);
        t_note("%ux MSAA: %s, %u levels", s, d3dh_hr_name(hr), n);
        CHECK(SUCCEEDED(hr));
        if (SUCCEEDED(hr)) CHECK(n != 0xdead);
        n = 0xdead;
        hr = ID3D11Device2_CheckMultisampleQualityLevels1(d2, DXGI_FORMAT_R8G8B8A8_UNORM, s, D3D11_CHECK_MULTISAMPLE_QUALITY_LEVELS_TILED_RESOURCE, &n);
        t_note("%ux MSAA tiled: %s, %u levels", s, d3dh_hr_name(hr), n);
    }
    n = 0xdead;
    HRESULT hr = ID3D11Device2_CheckMultisampleQualityLevels1(d2, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, &n);
    t_note("3x MSAA (invalid count): %s, %u", d3dh_hr_name(hr), n);
    hr = ID3D11Device2_CheckMultisampleQualityLevels1(d2, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0x80, &n);
    t_note("invalid flags: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(d2); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device1_open_shared_resource_failures) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1))) { t_skip("no Device1"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = ID3D11Device1_OpenSharedResource1(d1, NULL, &IID_ID3D11Texture2D, &p);
    CHECKF(FAILED(hr), "OpenSharedResource1(NULL) = %s", d3dh_hr_name(hr));
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    hr = ID3D11Device1_OpenSharedResource1(d1, INVALID_HANDLE_VALUE, &IID_ID3D11Texture2D, &p);
    CHECKF(FAILED(hr), "OpenSharedResource1(INVALID_HANDLE_VALUE) = %s", d3dh_hr_name(hr));
    p = (void *)(ULONG_PTR)1;
    hr = ID3D11Device1_OpenSharedResourceByName(d1, L"ifv_no_such_shared_resource", GENERIC_ALL, &IID_ID3D11Texture2D, &p);
    t_note("OpenSharedResourceByName(missing) -> %s", d3dh_hr_name(hr));
    CHECKF(FAILED(hr), "OpenSharedResourceByName(missing name) succeeded");
    CHECK(p == NULL);
    SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(context1_constant_buffer_offsets) {
    IFV_DEV(d, c);
    ID3D11DeviceContext1 *c1 = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11DeviceContext1, (void **)&c1))) { t_skip("no Context1"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_FEATURE_DATA_D3D11_OPTIONS o; memset(&o, 0, sizeof(o));
    ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof(o));
    D3D11_BUFFER_DESC bd = { 4096, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    ID3D11Buffer *b = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(d, &bd, NULL, &b), S_OK);
    if (b) {
        /* binding through the classic API reports offset 0 and the whole buffer through the new getter */
        ID3D11DeviceContext_VSSetConstantBuffers(c, 3, 1, &b);
        ID3D11Buffer *gb = NULL; UINT first = 0xdead, cnt = 0xdead;
        ID3D11DeviceContext1_VSGetConstantBuffers1(c1, 3, 1, &gb, &first, &cnt);
        CHECK(gb == b);
        EXPECT_EQ(first, 0);
        t_note("whole-buffer binding reports %u constants (buffer holds %u)", cnt, bd.ByteWidth / 16);
        SAFE_RELEASE(gb);
        if (o.ConstantBufferOffsetting) {
            UINT f = 16, n = 32;
            ID3D11DeviceContext1_PSSetConstantBuffers1(c1, 2, 1, &b, &f, &n);
            first = cnt = 0xdead; gb = NULL;
            ID3D11DeviceContext1_PSGetConstantBuffers1(c1, 2, 1, &gb, &first, &cnt);
            CHECK(gb == b);
            EXPECT_EQ(first, 16);
            EXPECT_EQ(cnt, 32);
            SAFE_RELEASE(gb);
            ID3D11DeviceContext1_VSSetConstantBuffers1(c1, 0, 1, &b, &f, &n);
            first = cnt = 0xdead; gb = NULL;
            ID3D11DeviceContext1_VSGetConstantBuffers1(c1, 0, 1, &gb, &first, &cnt);
            EXPECT_EQ(first, 16);
            EXPECT_EQ(cnt, 32);
            SAFE_RELEASE(gb);
            ID3D11DeviceContext1_CSSetConstantBuffers1(c1, 1, 1, &b, &f, &n);
            first = cnt = 0xdead; gb = NULL;
            ID3D11DeviceContext1_CSGetConstantBuffers1(c1, 1, 1, &gb, &first, &cnt);
            EXPECT_EQ(first, 16);
            EXPECT_EQ(cnt, 32);
            SAFE_RELEASE(gb);
        } else t_note("ConstantBufferOffsetting not supported at FL 0x%x", fl_);
        ID3D11DeviceContext_ClearState(c);
        SAFE_RELEASE(b);
    }
    SAFE_RELEASE(c1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_texture2d1_desc_roundtrip) {
    IFV_DEV(d, c);
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) { t_skip("no Device3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_TEXTURE2D_DESC1 td;
    memset(&td, 0, sizeof(td));
    td.Width = 32; td.Height = 16; td.MipLevels = 3; td.ArraySize = 2; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.TextureLayout = D3D11_TEXTURE_LAYOUT_UNDEFINED;
    ID3D11Texture2D1 *t = NULL;
    EXPECT_HR(ID3D11Device3_CreateTexture2D1(d3, &td, NULL, &t), S_OK);
    if (t) {
        D3D11_TEXTURE2D_DESC1 g; memset(&g, 0xcc, sizeof(g));
        ID3D11Texture2D1_GetDesc1(t, &g);
        EXPECT_EQ(g.Width, 32); EXPECT_EQ(g.Height, 16); EXPECT_EQ(g.MipLevels, 3); EXPECT_EQ(g.ArraySize, 2);
        EXPECT_EQ(g.Format, DXGI_FORMAT_R8G8B8A8_UNORM); EXPECT_EQ(g.SampleDesc.Count, 1); EXPECT_EQ(g.SampleDesc.Quality, 0);
        EXPECT_EQ(g.Usage, D3D11_USAGE_DEFAULT); EXPECT_EQ(g.BindFlags, td.BindFlags); EXPECT_EQ(g.CPUAccessFlags, 0);
        EXPECT_EQ(g.MiscFlags, 0); EXPECT_EQ(g.TextureLayout, D3D11_TEXTURE_LAYOUT_UNDEFINED);
        D3D11_TEXTURE2D_DESC old; memset(&old, 0xcc, sizeof(old));
        ID3D11Texture2D1_GetDesc(t, &old);
        EXPECT_EQ(old.Width, 32); EXPECT_EQ(old.MipLevels, 3); EXPECT_EQ(old.ArraySize, 2);
        EXPECT_HR(ifv_qi(t, &IID_ID3D11Texture2D, NULL), S_OK);
        EXPECT_HR(ifv_qi(t, &IID_ID3D11Resource, NULL), S_OK);
        EXPECT_HR(ifv_qi(t, &IID_IDXGIResource, NULL), S_OK);
        EXPECT_HR(ifv_qi(t, &IID_IDXGISurface, NULL), S_OK);
        SAFE_RELEASE(t);
    }
    SAFE_RELEASE(d3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_texture2d_qi_to_texture2d1) {
    IFV_DEV(d, c);
    D3D11_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture2D(d, &td, NULL, &t), S_OK);
    if (t) {
        ID3D11Texture2D1 *t1 = NULL;
        HRESULT hr = ID3D11Texture2D_QueryInterface(t, &IID_ID3D11Texture2D1, (void **)&t1);
        EXPECT_HR(hr, S_OK);
        if (t1) {
            D3D11_TEXTURE2D_DESC1 g; memset(&g, 0xcc, sizeof(g));
            ID3D11Texture2D1_GetDesc1(t1, &g);
            EXPECT_EQ(g.Width, 8); EXPECT_EQ(g.Format, DXGI_FORMAT_B8G8R8A8_UNORM); EXPECT_EQ(g.TextureLayout, D3D11_TEXTURE_LAYOUT_UNDEFINED);
            SAFE_RELEASE(t1);
        }
        SAFE_RELEASE(t);
    }
    D3D11_TEXTURE1D_DESC t1d = { 16, 1, 1, DXGI_FORMAT_R8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    ID3D11Texture1D *a = NULL;
    if (SUCCEEDED(ID3D11Device_CreateTexture1D(d, &t1d, NULL, &a))) {
        EXPECT_HR(ifv_qi(a, &IID_ID3D11Texture2D1, NULL), E_NOINTERFACE);
        EXPECT_HR(ifv_qi(a, &IID_ID3D11Texture2D, NULL), E_NOINTERFACE);
        SAFE_RELEASE(a);
    }
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_texture3d1_desc_roundtrip) {
    IFV_DEV(d, c);
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) { t_skip("no Device3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_TEXTURE3D_DESC1 td;
    memset(&td, 0, sizeof(td));
    td.Width = 16; td.Height = 8; td.Depth = 4; td.MipLevels = 2; td.Format = DXGI_FORMAT_R16_FLOAT;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.TextureLayout = D3D11_TEXTURE_LAYOUT_UNDEFINED;
    ID3D11Texture3D1 *t = NULL;
    EXPECT_HR(ID3D11Device3_CreateTexture3D1(d3, &td, NULL, &t), S_OK);
    if (t) {
        D3D11_TEXTURE3D_DESC1 g; memset(&g, 0xcc, sizeof(g));
        ID3D11Texture3D1_GetDesc1(t, &g);
        EXPECT_EQ(g.Width, 16); EXPECT_EQ(g.Height, 8); EXPECT_EQ(g.Depth, 4); EXPECT_EQ(g.MipLevels, 2);
        EXPECT_EQ(g.Format, DXGI_FORMAT_R16_FLOAT); EXPECT_EQ(g.BindFlags, D3D11_BIND_SHADER_RESOURCE);
        EXPECT_EQ(g.TextureLayout, D3D11_TEXTURE_LAYOUT_UNDEFINED);
        EXPECT_HR(ifv_qi(t, &IID_ID3D11Texture3D, NULL), S_OK);
        SAFE_RELEASE(t);
    }
    SAFE_RELEASE(d3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_texture_layout_validation) {
    IFV_DEV(d, c);
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) { t_skip("no Device3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_TEXTURE2D_DESC1 td;
    memset(&td, 0, sizeof(td));
    td.Width = 8; td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.TextureLayout = (D3D11_TEXTURE_LAYOUT)99;
    ID3D11Texture2D1 *t = (void *)(ULONG_PTR)1;
    HRESULT hr = ID3D11Device3_CreateTexture2D1(d3, &td, NULL, &t);
    t_note("TextureLayout 99 -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(t);
    static const D3D11_TEXTURE_LAYOUT lay[] = { D3D11_TEXTURE_LAYOUT_ROW_MAJOR, D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE };
    for (unsigned i = 0; i < 2; i++) {
        td.TextureLayout = lay[i];
        td.Usage = D3D11_USAGE_DEFAULT; td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        t = NULL;
        hr = ID3D11Device3_CreateTexture2D1(d3, &td, NULL, &t);
        t_note("layout %d with default usage + CPU write -> %s", lay[i], d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
    SAFE_RELEASE(d3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_rasterizer_state2_roundtrip) {
    IFV_DEV(d, c);
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) { t_skip("no Device3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_RASTERIZER_DESC2 rd;
    memset(&rd, 0, sizeof(rd));
    rd.FillMode = D3D11_FILL_WIREFRAME; rd.CullMode = D3D11_CULL_FRONT; rd.FrontCounterClockwise = TRUE;
    rd.DepthBias = 12; rd.DepthBiasClamp = 0.5f; rd.SlopeScaledDepthBias = 2.0f; rd.DepthClipEnable = FALSE;
    rd.ScissorEnable = TRUE; rd.MultisampleEnable = TRUE; rd.AntialiasedLineEnable = TRUE;
    rd.ForcedSampleCount = 0; rd.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    ID3D11RasterizerState2 *rs = NULL, *rs2 = NULL;
    EXPECT_HR(ID3D11Device3_CreateRasterizerState2(d3, &rd, &rs), S_OK);
    if (rs) {
        D3D11_RASTERIZER_DESC2 g; memset(&g, 0xcc, sizeof(g));
        ID3D11RasterizerState2_GetDesc2(rs, &g);
        CHECK(!memcmp(&g, &rd, sizeof(g)));
        D3D11_RASTERIZER_DESC1 g1; memset(&g1, 0xcc, sizeof(g1));
        ID3D11RasterizerState2_GetDesc1(rs, &g1);
        EXPECT_EQ(g1.FillMode, D3D11_FILL_WIREFRAME); EXPECT_EQ(g1.CullMode, D3D11_CULL_FRONT);
        EXPECT_EQ(g1.FrontCounterClockwise, TRUE); EXPECT_EQ(g1.DepthBias, 12); EXPECT_EQ(g1.ForcedSampleCount, 0);
        D3D11_RASTERIZER_DESC g0; memset(&g0, 0xcc, sizeof(g0));
        ID3D11RasterizerState2_GetDesc(rs, &g0);
        EXPECT_EQ(g0.ScissorEnable, TRUE); EXPECT_EQ(g0.DepthClipEnable, FALSE);
        EXPECT_HR(ifv_qi(rs, &IID_ID3D11RasterizerState1, NULL), S_OK);
        EXPECT_HR(ifv_qi(rs, &IID_ID3D11RasterizerState, NULL), S_OK);
        /* identical descriptions yield the very same state object */
        EXPECT_HR(ID3D11Device3_CreateRasterizerState2(d3, &rd, &rs2), S_OK);
        CHECK(rs2 == rs);
        SAFE_RELEASE(rs2);
        SAFE_RELEASE(rs);
    }
    SAFE_RELEASE(d3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_rasterizer_state2_conservative) {
    IFV_DEV(d, c);
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) { t_skip("no Device3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2; memset(&o2, 0, sizeof(o2));
    ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2));
    D3D11_RASTERIZER_DESC2 rd;
    memset(&rd, 0, sizeof(rd));
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_BACK; rd.DepthClipEnable = TRUE;
    rd.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON;
    ID3D11RasterizerState2 *rs = NULL;
    HRESULT hr = ID3D11Device3_CreateRasterizerState2(d3, &rd, &rs);
    t_note("conservative ON with tier %d -> %s", o2.ConservativeRasterizationTier, d3dh_hr_name(hr));
    if (o2.ConservativeRasterizationTier == D3D11_CONSERVATIVE_RASTERIZATION_NOT_SUPPORTED) CHECKF(FAILED(hr), "conservative rasterization accepted without support");
    else EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(rs);
    rd.ConservativeRaster = (D3D11_CONSERVATIVE_RASTERIZATION_MODE)7;
    hr = ID3D11Device3_CreateRasterizerState2(d3, &rd, &rs);
    t_note("invalid ConservativeRaster value -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(rs);
    SAFE_RELEASE(d3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device1_rasterizer_state1_and_blend_state1) {
    IFV_DEV(d, c);
    ID3D11Device1 *d1 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device1, (void **)&d1))) { t_skip("no Device1"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_RASTERIZER_DESC1 rd;
    memset(&rd, 0, sizeof(rd));
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE; rd.ForcedSampleCount = 0;
    ID3D11RasterizerState1 *rs = NULL;
    EXPECT_HR(ID3D11Device1_CreateRasterizerState1(d1, &rd, &rs), S_OK);
    if (rs) {
        D3D11_RASTERIZER_DESC1 g; memset(&g, 0xcc, sizeof(g));
        ID3D11RasterizerState1_GetDesc1(rs, &g);
        CHECK(!memcmp(&g, &rd, sizeof(g)));
        /* a state object created through the old entry point is an ID3D11RasterizerState1 too */
        D3D11_RASTERIZER_DESC r0;
        ID3D11RasterizerState_GetDesc((ID3D11RasterizerState *)rs, &r0);
        EXPECT_EQ(r0.CullMode, D3D11_CULL_NONE);
        SAFE_RELEASE(rs);
    }
    D3D11_RASTERIZER_DESC old = { D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, 0, 0.0f, 0.0f, TRUE, FALSE, FALSE, FALSE };
    ID3D11RasterizerState *ors = NULL;
    EXPECT_HR(ID3D11Device_CreateRasterizerState(d, &old, &ors), S_OK);
    if (ors) {
        EXPECT_HR(ifv_qi(ors, &IID_ID3D11RasterizerState1, NULL), S_OK);
        SAFE_RELEASE(ors);
    }
    D3D11_BLEND_DESC1 bd;
    memset(&bd, 0, sizeof(bd));
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].LogicOp = D3D11_LOGIC_OP_NOOP; bd.RenderTarget[0].RenderTargetWriteMask = 0xf;
    ID3D11BlendState1 *bs = NULL;
    EXPECT_HR(ID3D11Device1_CreateBlendState1(d1, &bd, &bs), S_OK);
    if (bs) {
        D3D11_BLEND_DESC1 g; memset(&g, 0xcc, sizeof(g));
        ID3D11BlendState1_GetDesc1(bs, &g);
        EXPECT_EQ(g.RenderTarget[0].RenderTargetWriteMask, 0xf);
        EXPECT_EQ(g.RenderTarget[0].LogicOpEnable, FALSE);
        EXPECT_EQ(g.RenderTarget[0].LogicOp, D3D11_LOGIC_OP_NOOP);
        SAFE_RELEASE(bs);
    }
    SAFE_RELEASE(d1); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_query1_roundtrip) {
    IFV_DEV(d, c);
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) { t_skip("no Device3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    static const D3D11_QUERY qs[] = { D3D11_QUERY_EVENT, D3D11_QUERY_OCCLUSION, D3D11_QUERY_TIMESTAMP, D3D11_QUERY_TIMESTAMP_DISJOINT, D3D11_QUERY_PIPELINE_STATISTICS };
    for (unsigned i = 0; i < sizeof(qs) / sizeof(qs[0]); i++) {
        D3D11_QUERY_DESC1 qd = { qs[i], 0, D3D11_CONTEXT_TYPE_ALL };
        ID3D11Query1 *q = NULL;
        HRESULT hr = ID3D11Device3_CreateQuery1(d3, &qd, &q);
        EXPECT_HR(hr, S_OK);
        if (q) {
            D3D11_QUERY_DESC1 g; memset(&g, 0xcc, sizeof(g));
            ID3D11Query1_GetDesc1(q, &g);
            EXPECT_EQ(g.Query, qs[i]); EXPECT_EQ(g.MiscFlags, 0); EXPECT_EQ(g.ContextType, D3D11_CONTEXT_TYPE_ALL);
            D3D11_QUERY_DESC g0; memset(&g0, 0xcc, sizeof(g0));
            ID3D11Query1_GetDesc(q, &g0);
            EXPECT_EQ(g0.Query, qs[i]);
            SAFE_RELEASE(q);
        }
    }
    static const D3D11_CONTEXT_TYPE cts[] = { D3D11_CONTEXT_TYPE_3D, D3D11_CONTEXT_TYPE_COMPUTE, D3D11_CONTEXT_TYPE_COPY, D3D11_CONTEXT_TYPE_VIDEO };
    for (unsigned i = 0; i < 4; i++) {
        D3D11_QUERY_DESC1 qd = { D3D11_QUERY_EVENT, 0, cts[i] };
        ID3D11Query1 *q = NULL;
        HRESULT hr = ID3D11Device3_CreateQuery1(d3, &qd, &q);
        t_note("CreateQuery1(EVENT, context type %d) -> %s", cts[i], d3dh_hr_name(hr));
        if (q) { D3D11_QUERY_DESC1 g; ID3D11Query1_GetDesc1(q, &g); EXPECT_EQ(g.ContextType, cts[i]); SAFE_RELEASE(q); }
    }
    SAFE_RELEASE(d3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_views1_roundtrip) {
    IFV_DEV(d, c);
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&d3))) { t_skip("no Device3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_TEXTURE2D_DESC td = { 16, 16, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS, 0, 0 };
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(d, &td, NULL, &t))) {
        td.BindFlags &= ~D3D11_BIND_UNORDERED_ACCESS;
        ID3D11Device_CreateTexture2D(d, &td, NULL, &t);
    }
    if (t) {
        D3D11_SHADER_RESOURCE_VIEW_DESC1 sd; memset(&sd, 0, sizeof(sd));
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 1; sd.Texture2D.MipLevels = 1; sd.Texture2D.PlaneSlice = 0;
        ID3D11ShaderResourceView1 *srv = NULL;
        EXPECT_HR(ID3D11Device3_CreateShaderResourceView1(d3, (ID3D11Resource *)t, &sd, &srv), S_OK);
        if (srv) {
            D3D11_SHADER_RESOURCE_VIEW_DESC1 g; memset(&g, 0xcc, sizeof(g));
            ID3D11ShaderResourceView1_GetDesc1(srv, &g);
            EXPECT_EQ(g.Format, DXGI_FORMAT_R8G8B8A8_UNORM); EXPECT_EQ(g.ViewDimension, D3D11_SRV_DIMENSION_TEXTURE2D);
            EXPECT_EQ(g.Texture2D.MostDetailedMip, 1); EXPECT_EQ(g.Texture2D.MipLevels, 1); EXPECT_EQ(g.Texture2D.PlaneSlice, 0);
            EXPECT_HR(ifv_qi(srv, &IID_ID3D11ShaderResourceView, NULL), S_OK);
            SAFE_RELEASE(srv);
        }
        D3D11_RENDER_TARGET_VIEW_DESC1 rd; memset(&rd, 0, sizeof(rd));
        rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; rd.Texture2D.MipSlice = 1;
        ID3D11RenderTargetView1 *rtv = NULL;
        EXPECT_HR(ID3D11Device3_CreateRenderTargetView1(d3, (ID3D11Resource *)t, &rd, &rtv), S_OK);
        if (rtv) {
            D3D11_RENDER_TARGET_VIEW_DESC1 g; memset(&g, 0xcc, sizeof(g));
            ID3D11RenderTargetView1_GetDesc1(rtv, &g);
            EXPECT_EQ(g.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2D); EXPECT_EQ(g.Texture2D.MipSlice, 1); EXPECT_EQ(g.Texture2D.PlaneSlice, 0);
            EXPECT_HR(ifv_qi(rtv, &IID_ID3D11RenderTargetView, NULL), S_OK);
            SAFE_RELEASE(rtv);
        }
        if (td.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC1 ud; memset(&ud, 0, sizeof(ud));
            ud.Format = DXGI_FORMAT_R8G8B8A8_UNORM; ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D; ud.Texture2D.MipSlice = 0;
            ID3D11UnorderedAccessView1 *uav = NULL;
            HRESULT hr = ID3D11Device3_CreateUnorderedAccessView1(d3, (ID3D11Resource *)t, &ud, &uav);
            if (fl_ >= D3D_FEATURE_LEVEL_11_0) EXPECT_HR(hr, S_OK); else t_note("UAV1 at FL 0x%x -> %s", fl_, d3dh_hr_name(hr));
            if (uav) {
                D3D11_UNORDERED_ACCESS_VIEW_DESC1 g; memset(&g, 0xcc, sizeof(g));
                ID3D11UnorderedAccessView1_GetDesc1(uav, &g);
                EXPECT_EQ(g.ViewDimension, D3D11_UAV_DIMENSION_TEXTURE2D); EXPECT_EQ(g.Texture2D.MipSlice, 0); EXPECT_EQ(g.Texture2D.PlaneSlice, 0);
                SAFE_RELEASE(uav);
            }
        }
        SAFE_RELEASE(t);
    }
    SAFE_RELEASE(d3); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(device3_read_write_subresource) {
    IFV_DEV(d, c);
    ID3D11Device3 *c3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_ID3D11Device3, (void **)&c3))) { t_skip("no Device3"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2; memset(&o2, 0, sizeof(o2));
    ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2));
    if (!o2.MapOnDefaultTextures) {
        t_note("MapOnDefaultTextures not supported: Read/WriteToSubresource are only exercised on supporting drivers");
        ID3D11Device3_Release(c3); SAFE_RELEASE(c); SAFE_RELEASE(d);
        t_skip("no CPU access to default textures");
        return;
    }
    D3D11_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE,
        D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0 };
    ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &t);
    t_note("default texture with CPU access -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && t) {
        unsigned src[64], dst[64];
        for (int i = 0; i < 64; i++) { src[i] = 0x01010101u * (unsigned)(i + 1); dst[i] = 0; }
        ID3D11Device3_WriteToSubresource(c3, (ID3D11Resource *)t, 0, NULL, src, 32, 256);
        ID3D11Device3_ReadFromSubresource(c3, dst, 32, 256, (ID3D11Resource *)t, 0, NULL);
        t_note("write/read roundtrip %s", memcmp(src, dst, sizeof(src)) ? "mismatch" : "matches");
        D3D11_BOX box = { 2, 2, 0, 6, 6, 1 };
        unsigned patch[16];
        for (int i = 0; i < 16; i++) patch[i] = 0xff000000u | (unsigned)i;
        ID3D11Device3_WriteToSubresource(c3, (ID3D11Resource *)t, 0, &box, patch, 16, 64);
        memset(dst, 0, sizeof(dst));
        ID3D11Device3_ReadFromSubresource(c3, dst, 16, 64, (ID3D11Resource *)t, 0, &box);
        t_note("partial roundtrip %s", memcmp(patch, dst, sizeof(patch)) ? "mismatch" : "matches");
        SAFE_RELEASE(t);
    }
    ID3D11Device3_Release(c3);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
