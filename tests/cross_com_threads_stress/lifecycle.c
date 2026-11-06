/* Object and device lifetimes: many devices, creation order versus release order, repeated creation without
 * growth, adapter enumeration protocol, device-removed bookkeeping and ID3D12Device5::RemoveDevice. */
#include "cts.h"

/* ---- DXGI/D3D error constants (the values implementors must return) ------------------------------ */
T(lifecycle_device_removed_error_values)
{
    EXPECT_EQ((UINT)DXGI_ERROR_DEVICE_HUNG, 0x887A0006u);
    EXPECT_EQ((UINT)DXGI_ERROR_DEVICE_REMOVED, 0x887A0005u);
    EXPECT_EQ((UINT)DXGI_ERROR_DEVICE_RESET, 0x887A0007u);
    EXPECT_EQ((UINT)DXGI_ERROR_DRIVER_INTERNAL_ERROR, 0x887A0020u);
    EXPECT_EQ((UINT)DXGI_ERROR_INVALID_CALL, 0x887A0001u);
    EXPECT_EQ((UINT)DXGI_ERROR_NOT_FOUND, 0x887A0002u);
    EXPECT_EQ((UINT)DXGI_ERROR_UNSUPPORTED, 0x887A0004u);
    EXPECT_EQ((UINT)DXGI_ERROR_SDK_COMPONENT_MISSING, 0x887A002Du);
    EXPECT_EQ((UINT)DXGI_ERROR_ACCESS_LOST, 0x887A0026u);
    EXPECT_EQ((UINT)DXGI_ERROR_WAS_STILL_DRAWING, 0x887A000Au);
    EXPECT_EQ((UINT)D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS, 0x887C0001u);
    EXPECT_EQ((UINT)D3D11_ERROR_FILE_NOT_FOUND, 0x887C0002u);
    EXPECT_EQ((UINT)D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS, 0x887C0003u);
    EXPECT_EQ((UINT)D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD, 0x887C0004u);
}

/* ---- healthy devices report S_OK, every API level --------------------------------------------------- */
T(lifecycle_healthy_d3d11_device_not_removed)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    for (int i = 0; i < 5; i++) EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(lifecycle_healthy_d3d10_device_not_removed)
{
    ID3D10Device *d = NULL;
    REQUIRE(d3dh_D3D10CreateDevice());
    if (FAILED(d3dh_make_device10(&d, NULL, 0))) { t_skip("no 10 device"); return; }
    EXPECT_HR(ID3D10Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(d);
}
T(lifecycle_healthy_d3d12_device_not_removed)
{
    ID3D12Device *d = NULL; int w;
    REQUIRE(d3dh_D3D12CreateDevice());
    if (FAILED(d3dh_make_device12(&d, D3D_FEATURE_LEVEL_11_0, &w))) { t_skip("no 12 device"); return; }
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(d), S_OK);
    EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(d), S_OK);
    SAFE_RELEASE(d);
}

/* ---- exception mode (documented on ID3D11Device): round trip only, never triggers anything ---------- */
T(lifecycle_d3d11_exception_mode_roundtrip)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), 0);
    EXPECT_HR_OK(ID3D11Device_SetExceptionMode(d, D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR));
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR);
    EXPECT_HR_OK(ID3D11Device_SetExceptionMode(d, 0));
    EXPECT_EQ(ID3D11Device_GetExceptionMode(d), 0);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- many devices ---------------------------------------------------------------------------------------- */
#define MANY 16
T(lifecycle_sixteen_d3d11_devices)
{
    ID3D11Device *d[MANY]; ID3D11DeviceContext *c[MANY]; ID3D11Buffer *b[MANY]; int n = 0;
    REQUIRE(d3dh_D3D11CreateDevice());
    memset(d, 0, sizeof d); memset(c, 0, sizeof c); memset(b, 0, sizeof b);
    for (int i = 0; i < MANY; i++) {
        HRESULT h = cts_dev11(0, &d[i], &c[i]);
        if (FAILED(h)) { if (i == 0) { t_skip("no 11 device"); return; } EXPECT_HR_OK(h); break; }
        n++;
        EXPECT_HR_OK(cts_make_buffer(d[i], 128, D3D11_BIND_VERTEX_BUFFER, &b[i]));
    }
    EXPECT_EQ(n, MANY);
    for (int i = 0; i < n; i++) for (int k = i + 1; k < n; k++) { CHECK(d[i] != d[k]); CHECK(c[i] != c[k]); }
    for (int i = 0; i < n; i++) {
        EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d[i]), S_OK);
        if (b[i]) { ID3D11Device *own = NULL; ID3D11Buffer_GetDevice(b[i], &own); CHECK(own == d[i]); SAFE_RELEASE(own); }
    }
    /* release in an interleaved order: odd indexes first (descending), then even (ascending) */
    for (int i = MANY - 1; i >= 0; i--) if (i & 1) { SAFE_RELEASE(b[i]); SAFE_RELEASE(c[i]); SAFE_RELEASE(d[i]); }
    for (int i = 0; i < MANY; i++) if (!(i & 1)) { SAFE_RELEASE(b[i]); SAFE_RELEASE(c[i]); SAFE_RELEASE(d[i]); }
}
T(lifecycle_eight_d3d12_devices)
{
    ID3D12Device *d[8]; int n = 0, w;
    REQUIRE(d3dh_D3D12CreateDevice());
    memset(d, 0, sizeof d);
    for (int i = 0; i < 8; i++) {
        HRESULT h = d3dh_make_device12(&d[i], D3D_FEATURE_LEVEL_11_0, &w);
        if (FAILED(h)) { if (i == 0) { t_skip("no 12 device"); return; } EXPECT_HR_OK(h); break; }
        n++;
    }
    EXPECT_EQ(n, 8);
    for (int i = 0; i < n; i++) {
        ID3D12CommandAllocator *ca = NULL;
        EXPECT_HR_OK(ID3D12Device_CreateCommandAllocator(d[i], D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&ca));
        SAFE_RELEASE(ca);
    }
    for (int i = n - 1; i >= 0; i--) SAFE_RELEASE(d[i]);
}
T(lifecycle_mixed_api_devices_coexist)
{
    ID3D11Device *d11 = NULL; ID3D11DeviceContext *c11 = NULL; ID3D10Device *d10 = NULL; ID3D12Device *d12 = NULL; int w, got = 0;
    if (d3dh_D3D11CreateDevice() && SUCCEEDED(cts_dev11(0, &d11, &c11))) got++;
    if (d3dh_D3D10CreateDevice() && SUCCEEDED(d3dh_make_device10(&d10, NULL, 0))) got++;
    if (d3dh_D3D12CreateDevice() && SUCCEEDED(d3dh_make_device12(&d12, D3D_FEATURE_LEVEL_11_0, &w))) got++;
    if (!got) { t_skip("no devices"); return; }
    if (d11) EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(d11), S_OK);
    if (d10) EXPECT_HR(ID3D10Device_GetDeviceRemovedReason(d10), S_OK);
    if (d12) EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(d12), S_OK);
    SAFE_RELEASE(d12); SAFE_RELEASE(d10); SAFE_RELEASE(c11); SAFE_RELEASE(d11);
}

/* ---- release order ---------------------------------------------------------------------------------------- */
T(lifecycle_child_keeps_device_alive)
{
    ID3D11Device *d = NULL, *own = NULL; ID3D11DeviceContext *c = NULL; ID3D11Buffer *b = NULL, *b2 = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    EXPECT_HR_OK(cts_make_buffer(d, 256, D3D11_BIND_VERTEX_BUFFER, &b));
    SAFE_RELEASE(c);
    SAFE_RELEASE(d);                     /* the application dropped both its device references */
    if (b) {
        ID3D11Buffer_GetDevice(b, &own);
        CHECK(own != NULL);
        if (own) {
            EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(own), S_OK);
            EXPECT_HR_OK(cts_make_buffer(own, 64, D3D11_BIND_INDEX_BUFFER, &b2));
        }
    }
    SAFE_RELEASE(b2); SAFE_RELEASE(own); SAFE_RELEASE(b);
}
T(lifecycle_get_immediate_context_addrefs)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL, *c2 = NULL, *c3 = NULL; ULONG before, after;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    before = d3dh_refcount(c);
    ID3D11Device_GetImmediateContext(d, &c2);
    ID3D11Device_GetImmediateContext(d, &c3);
    CHECK(c2 == c); CHECK(c3 == c);
    after = d3dh_refcount(c);
    CHECK(after >= before + 1);
    if (after != before + 2) t_note("immediate context refcount delta %d (expected 2)", (int)(after - before));
    SAFE_RELEASE(c3); SAFE_RELEASE(c2);
    CHECK(d3dh_refcount(c) <= after);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
T(lifecycle_factory_and_device_release_orders)
{
    IDXGIFactory1 *f = NULL; ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; IDXGIDevice *dd = NULL; IDXGIAdapter *a = NULL; IDXGIFactory1 *f2 = NULL;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    if (FAILED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd))) { t_skip("no IDXGIDevice"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(dd, &a));
    if (a) EXPECT_HR_OK(IDXGIAdapter_GetParent(a, &IID_IDXGIFactory1, (void **)&f2));
    SAFE_RELEASE(dd); SAFE_RELEASE(c); SAFE_RELEASE(d);
    if (a) { DXGI_ADAPTER_DESC ad; EXPECT_HR_OK(IDXGIAdapter_GetDesc(a, &ad)); }   /* adapter outlives the device */
    if (f2) { IDXGIAdapter *a0 = NULL; EXPECT_HR_OK(IDXGIFactory1_EnumAdapters(f2, 0, &a0)); SAFE_RELEASE(a0); }
    SAFE_RELEASE(f2); SAFE_RELEASE(a);
    (void)f;
}

/* ---- resource growth over repeated creation ------------------------------------------------------------------ */
T(lifecycle_device_create_destroy_loop_no_growth)
{
    SIZE_T base = 0, now; int ok = 0;
    REQUIRE(d3dh_D3D11CreateDevice());
    for (int i = 0; i < 40; i++) {
        ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; ID3D11Buffer *b = NULL;
        if (FAILED(cts_dev11(0, &d, &c))) { if (i == 0) { t_skip("no 11 device"); return; } break; }
        cts_make_buffer(d, 4096, D3D11_BIND_VERTEX_BUFFER, &b);
        SAFE_RELEASE(b); SAFE_RELEASE(c); SAFE_RELEASE(d);
        ok++;
        if (i == 9) base = cts_private_bytes();
    }
    EXPECT_EQ(ok, 40);
    now = cts_private_bytes();
    if (!now) { t_skip("no process memory counters"); return; }
    t_note("private bytes after warm-up %llu, after 40 cycles %llu", (unsigned long long)base, (unsigned long long)now);
    CHECKF(now < base + CTS_HEADROOM_MB(96), "private bytes grew by %lld KiB over 30 device cycles", (long long)(now - base) / 1024);
}
T(lifecycle_resource_create_destroy_loop_no_growth)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; SIZE_T base = 0, now;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(cts_dev11(0, &d, &c))) { t_skip("no 11 device"); return; }
    for (int i = 0; i < 4000; i++) {
        ID3D11Buffer *b = NULL; ID3D11Texture2D *t = NULL;
        cts_make_buffer(d, 65536, D3D11_BIND_VERTEX_BUFFER, &b);
        cts_make_tex(d, 64, 64, &t);
        SAFE_RELEASE(t); SAFE_RELEASE(b);
        /* Windows 10: the runtime defers the destruction of released resources until the immediate context is flushed,
           so an unflushed loop grows by ~80 KiB per pair (289 MB over 3500 pairs measured); Flush every 64 iterations. */
        if ((i & 63) == 63) ID3D11DeviceContext_Flush(c);
        if (i == 499) base = cts_private_bytes();
    }
    ID3D11DeviceContext_Flush(c);
    now = cts_private_bytes();
    if (!now) { t_skip("no process memory counters"); }
    else CHECKF(now < base + CTS_HEADROOM_MB(128), "private bytes grew by %lld KiB over 3500 create/destroy pairs", (long long)(now - base) / 1024);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

/* ---- adapter enumeration repeated ---------------------------------------------------------------------------------- */
T(lifecycle_adapter_enumeration_protocol)
{
    IDXGIFactory1 *f = NULL; IDXGIAdapter1 *a = (IDXGIAdapter1 *)(ULONG_PTR)0x1; UINT n = 0;
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no factory"); return; }
    for (;; n++) {
        a = (IDXGIAdapter1 *)(ULONG_PTR)0x1;
        HRESULT h = IDXGIFactory1_EnumAdapters1(f, n, &a);
        if (h == DXGI_ERROR_NOT_FOUND) { CHECK(a == NULL); break; }
        EXPECT_HR_OK(h);
        if (FAILED(h) || n > 64) break;
        SAFE_RELEASE(a);
    }
    t_note("adapter count %u", n);
    a = (IDXGIAdapter1 *)(ULONG_PTR)0x1;
    EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, n + 5, &a), DXGI_ERROR_NOT_FOUND);
    CHECK(a == NULL);
    EXPECT_HR(IDXGIFactory1_EnumAdapters1(f, 0xFFFFFFFFu, &a), DXGI_ERROR_NOT_FOUND);
    SAFE_RELEASE(f);
}
T(lifecycle_adapter_enumeration_1000x_new_factory)
{
    SIZE_T base = 0, now; UINT first = 0; int bad = 0;
    REQUIRE(d3dh_CreateDXGIFactory1());
    for (int i = 0; i < 1000; i++) {
        IDXGIFactory1 *f = NULL; IDXGIAdapter1 *a = NULL; UINT n = 0;
        if (FAILED(d3dh_make_factory1(&f))) { if (i == 0) { t_skip("no factory"); return; } bad++; break; }
        while (IDXGIFactory1_EnumAdapters1(f, n, &a) == S_OK) {
            DXGI_ADAPTER_DESC1 ad; if (FAILED(IDXGIAdapter1_GetDesc1(a, &ad))) bad++;
            SAFE_RELEASE(a); n++;
        }
        if (i == 0) first = n; else if (n != first) bad++;
        SAFE_RELEASE(f);
        if (i == 99) base = cts_private_bytes();
    }
    EXPECT_EQ(bad, 0);
    now = cts_private_bytes();
    if (!now) { t_skip("no process memory counters"); return; }
    CHECKF(now < base + CTS_HEADROOM_MB(48), "private bytes grew by %lld KiB over 900 factory cycles", (long long)(now - base) / 1024);
}
T(lifecycle_adapter_enumeration_1000x_same_factory)
{
    IDXGIFactory1 *f = NULL; SIZE_T base = 0, now; int bad = 0; UINT first = 0;
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no factory"); return; }
    for (int i = 0; i < 1000; i++) {
        IDXGIAdapter1 *a = NULL; UINT n = 0;
        while (IDXGIFactory1_EnumAdapters1(f, n, &a) == S_OK) {
            DXGI_ADAPTER_DESC1 ad; if (FAILED(IDXGIAdapter1_GetDesc1(a, &ad))) bad++;
            SAFE_RELEASE(a); n++;
        }
        if (i == 0) first = n; else if (n != first) bad++;
        if (i == 99) base = cts_private_bytes();
    }
    EXPECT_EQ(bad, 0);
    EXPECT_EQ(d3dh_refcount(f), 1);
    now = cts_private_bytes();
    if (now) CHECKF(now < base + CTS_HEADROOM_MB(48), "private bytes grew by %lld KiB over 900 enumerations", (long long)(now - base) / 1024);
    SAFE_RELEASE(f);
}
T(lifecycle_adapter_and_output_enumeration_1000x)
{
    IDXGIFactory1 *f = NULL; int bad = 0, outs = 0; SIZE_T base = 0, now;
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no factory"); return; }
    for (int i = 0; i < 1000; i++) {
        IDXGIAdapter1 *a = NULL;
        if (IDXGIFactory1_EnumAdapters1(f, 0, &a) != S_OK) { if (i == 0) { SAFE_RELEASE(f); t_skip("no adapter"); return; } bad++; break; }
        for (UINT o = 0;; o++) {
            IDXGIOutput *op = NULL; HRESULT h = IDXGIAdapter1_EnumOutputs(a, o, &op);
            if (h == DXGI_ERROR_NOT_FOUND) { if (op) bad++; break; }
            if (FAILED(h)) { bad++; break; }
            if (i == 0) outs++;
            SAFE_RELEASE(op);
        }
        SAFE_RELEASE(a);
        if (i == 99) base = cts_private_bytes();
    }
    EXPECT_EQ(bad, 0);
    t_note("outputs on adapter 0: %d", outs);
    now = cts_private_bytes();
    if (now) CHECKF(now < base + CTS_HEADROOM_MB(48), "private bytes grew by %lld KiB", (long long)(now - base) / 1024);
    SAFE_RELEASE(f);
}

/* ---- ID3D12Device5::RemoveDevice, run in a child so a removed device never affects other tests ------------------- */
static const GUID cts_unknown_feature = { 0x7a3b1c2d, 0x1234, 0x4abc, { 0x9d, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd } };
static int reason_is_documented(HRESULT hr)
{
    return hr == DXGI_ERROR_DEVICE_HUNG || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
           hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR || hr == DXGI_ERROR_INVALID_CALL;
}
CHILD(d3d12_remove)
{
    HMODULE m = LoadLibraryW(L"d3d12.dll");
    HRESULT (WINAPI *cd)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
    ID3D12Device *d = NULL; ID3D12Device5 *d5 = NULL; ID3D12Fence *fe = NULL, *fe2 = NULL; HANDLE ev; int bits = 0;
    HRESULT hr, r1, r2;
    if (!m) return 0x4000;
    cd = (void *)GetProcAddress(m, "D3D12CreateDevice");
    if (!cd) return 0x4000;
    hr = cd(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d);
    if (FAILED(hr) || !d) return 0x4000;
    if (FAILED(ID3D12Device_QueryInterface(d, &IID_ID3D12Device5, (void **)&d5))) { ID3D12Device_Release(d); return 0x8000; }
    if (FAILED(ID3D12Device_CreateFence(d, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fe))) return 0x4000;
    ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (FAILED(ID3D12Fence_SetEventOnCompletion(fe, 10, ev))) bits |= 1;
    if (ID3D12Device_GetDeviceRemovedReason(d) != S_OK) bits |= 2;
    ID3D12Device5_RemoveDevice(d5);
    r1 = ID3D12Device_GetDeviceRemovedReason(d);
    r2 = ID3D12Device_GetDeviceRemovedReason(d);
    printf("  NOTE d3d12 removed reason %s\n", d3dh_hr_name(r1));
    if (FAILED(r1)) {} else bits |= 4;
    if (!reason_is_documented(r1)) printf("  NOTE reason is outside the documented DXGI error set\n");
    if (r1 != r2) bits |= 8;
    if (WaitForSingleObject(ev, 10000) != WAIT_OBJECT_0) bits |= 16;      /* monitored fences are signalled */
    if (ID3D12Fence_GetCompletedValue(fe) != (UINT64)-1) bits |= 32;
    {
        ID3D12CommandAllocator *ca = NULL;
        hr = ID3D12Device_CreateCommandAllocator(d, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&ca);
        printf("  NOTE CreateCommandAllocator after removal: %s\n", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) { bits |= 64; ID3D12CommandAllocator_Release(ca); }
    }
    {
        ID3D12Resource *res = NULL; D3D12_HEAP_PROPERTIES hp; D3D12_RESOURCE_DESC rd;
        memset(&hp, 0, sizeof hp); hp.Type = D3D12_HEAP_TYPE_DEFAULT; hp.CreationNodeMask = hp.VisibleNodeMask = 1;
        memset(&rd, 0, sizeof rd); rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width = 256; rd.Height = 1; rd.DepthOrArraySize = 1;
        rd.MipLevels = 1; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hr = ID3D12Device_CreateCommittedResource(d, &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&res);
        printf("  NOTE CreateCommittedResource after removal: %s\n", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) { bits |= 128; ID3D12Resource_Release(res); }
    }
    if (FAILED(ID3D12Device_CreateFence(d, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fe2))) printf("  NOTE CreateFence after removal fails\n");
    else ID3D12Fence_Release(fe2);
    ID3D12Device5_RemoveDevice(d5);                                   /* a second removal is harmless */
    if (ID3D12Device_GetDeviceRemovedReason(d) != r1) bits |= 256;
    ID3D12Fence_Release(fe); CloseHandle(ev);
    ID3D12Device5_Release(d5); ID3D12Device_Release(d);
    return bits;
}
T(lifecycle_d3d12_remove_device)
{
    DWORD rc;
    REQUIRE(d3dh_D3D12CreateDevice());
    rc = t_run_child("d3d12_remove", "", 90000);
    if (rc == 0x4000) { t_skip("no d3d12 device"); return; }
    if (rc == 0x8000) { t_skip("ID3D12Device5 not available"); return; }
    EXPECT_EQ(rc & 1, 0);      /* SetEventOnCompletion before removal */
    EXPECT_EQ(rc & 2, 0);      /* healthy before removal */
    EXPECT_EQ(rc & 4, 0);      /* reason is a failure code */
    EXPECT_EQ(rc & 8, 0);      /* reason is sticky */
    EXPECT_EQ(rc & 16, 0);     /* pending fence waiters are released */
    EXPECT_EQ(rc & 32, 0);     /* GetCompletedValue == UINT64_MAX on a removed device */
    EXPECT_EQ(rc & 64, 0);     /* object creation fails */
    EXPECT_EQ(rc & 128, 0);
    EXPECT_EQ(rc & 256, 0);
    EXPECT_EQ(rc & ~0x1FFu, 0);
}

/* A removed device must not poison the adapter: a new device can be made afterwards. */
CHILD(d3d12_remove_then_new)
{
    HMODULE m = LoadLibraryW(L"d3d12.dll");
    HRESULT (WINAPI *cd)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
    ID3D12Device *d = NULL, *d2 = NULL; ID3D12Device5 *d5 = NULL; HRESULT hr;
    if (!m) return 0x4000;
    cd = (void *)GetProcAddress(m, "D3D12CreateDevice");
    if (!cd || FAILED(cd(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d))) return 0x4000;
    if (FAILED(ID3D12Device_QueryInterface(d, &IID_ID3D12Device5, (void **)&d5))) return 0x8000;
    ID3D12Device5_RemoveDevice(d5);
    ID3D12Device5_Release(d5); ID3D12Device_Release(d);
    hr = cd(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d2);
    printf("  NOTE new d3d12 device after a removal: %s\n", d3dh_hr_name(hr));
    if (FAILED(hr)) return 1;
    hr = ID3D12Device_GetDeviceRemovedReason(d2);
    ID3D12Device_Release(d2);
    return hr == S_OK ? 0 : 2;
}
T(lifecycle_d3d12_new_device_after_removal)
{
    DWORD rc;
    REQUIRE(d3dh_D3D12CreateDevice());
    rc = t_run_child("d3d12_remove_then_new", "", 90000);
    if (rc == 0x4000) { t_skip("no d3d12 device"); return; }
    if (rc == 0x8000) { t_skip("ID3D12Device5 not available"); return; }
    t_note("new device after removal child result %lu (0 = ok)", rc);
    EXPECT_EQ(rc, 0);
}

/* Experimental features: an unknown feature id can never be enabled. Process-wide state, so a child. */
CHILD(d3d12_experimental)
{
    HMODULE m = LoadLibraryW(L"d3d12.dll");
    HRESULT (WINAPI *ex)(UINT, const IID *, void *, UINT *);
    HRESULT h0, h1; UINT sz = 0;
    if (!m) return 0x4000;
    ex = (void *)GetProcAddress(m, "D3D12EnableExperimentalFeatures");
    if (!ex) return 0x4000;
    h0 = ex(0, NULL, NULL, NULL);
    printf("  NOTE EnableExperimentalFeatures(0 features) = %s\n", d3dh_hr_name(h0));
    h1 = ex(1, &cts_unknown_feature, NULL, &sz);
    printf("  NOTE EnableExperimentalFeatures(unknown id) = %s\n", d3dh_hr_name(h1));
    return SUCCEEDED(h1) ? 1 : 0;
}
T(lifecycle_d3d12_experimental_unknown_feature_fails)
{
    DWORD rc;
    REQUIRE(d3dh_getproc("d3d12.dll", "D3D12EnableExperimentalFeatures"));
    rc = t_run_child("d3d12_experimental", "", 60000);
    if (rc == 0x4000) { t_skip("export missing"); return; }
    EXPECT_EQ(rc, 0);
}
