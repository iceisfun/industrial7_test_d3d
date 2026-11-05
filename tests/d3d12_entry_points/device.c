#include "ep.h"

#define CFS(d, f, p, n) ID3D12Device_CheckFeatureSupport((d), (f), (p), (n))

/* ---------- base device behaviour ---------- */
TEST(d3d12_entry_points_device_nodecount) {
    EP_NEED_DEVICE(d);
    UINT n = ID3D12Device_GetNodeCount(d);
    CHECK(n >= 1 && n <= 32);
    CHECK(n == ID3D12Device_GetNodeCount(d));
    t_note("node count %u", n);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_removed_reason_ok) {
    EP_NEED_DEVICE(d);
    for (int i = 0; i < 3; i++) EXPECT_HR(ID3D12Device_GetDeviceRemovedReason(d), S_OK);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_adapter_luid_stable) {
    EP_NEED_DEVICE(d);
    LUID a = ep_luid(d), b = ep_luid(d);
    CHECK(a.LowPart == b.LowPart && a.HighPart == b.HighPart);
    CHECK(a.LowPart != 0 || a.HighPart != 0);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_adapter_luid_matches_dxgi) {
    EP_NEED_WARP_USABLE(ad);
    ID3D12Device *d = NULL;
    EXPECT_HR(d3dh_D3D12CreateDevice()((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d), S_OK);
    DXGI_ADAPTER_DESC desc; EXPECT_HR(IDXGIAdapter_GetDesc(ad, &desc), S_OK);
    if (d) {
        LUID l = ep_luid(d);
        EP_EQ(l.LowPart, desc.AdapterLuid.LowPart); EP_EQ(l.HighPart, desc.AdapterLuid.HighPart);
        ID3D12Device_Release(d);
    }
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_device_stable_power_state_off) {
    EP_NEED_DEVICE(d);
    HRESULT hr = ID3D12Device_SetStablePowerState(d, FALSE);
    t_note("SetStablePowerState(FALSE) -> %s", d3dh_hr_name(hr));
    EXPECT_HR_IN(hr, S_OK, HR_UNSUPPORTED, E_NOTIMPL, DXGI_ERROR_NOT_CURRENTLY_AVAILABLE);
    ID3D12Device_Release(d);
}

/* ---------- IUnknown / QI ladder ---------- */
#define LADDER_TEST(i) \
TEST(d3d12_entry_points_device_qi_device##i) { \
    EP_NEED_DEVICE(d); \
    void *p = (void *)(ULONG_PTR)1; \
    HRESULT hr = ID3D12Device_QueryInterface(d, EP_DEVICE_IIDS[i], &p); \
    if (SUCCEEDED(hr)) { \
        CHECK(p != NULL && p != (void *)1); \
        IUnknown *u = (IUnknown *)p, *a = NULL, *b = NULL; \
        EXPECT_HR(u->lpVtbl->QueryInterface(u, &IID_IUnknown, (void **)&a), S_OK); \
        EXPECT_HR(ID3D12Device_QueryInterface(d, &IID_IUnknown, (void **)&b), S_OK); \
        CHECK(a == b); \
        if (a) a->lpVtbl->Release(a); if (b) b->lpVtbl->Release(b); \
        ID3D12Device *back = NULL; \
        EXPECT_HR(u->lpVtbl->QueryInterface(u, &IID_ID3D12Device, (void **)&back), S_OK); \
        CHECK((void *)back == (void *)d); if (back) ID3D12Device_Release(back); \
        u->lpVtbl->Release(u); \
    } else { \
        EXPECT_HR(hr, E_NOINTERFACE); CHECK(p == NULL); \
        t_note("ID3D12Device%d not supported", i); \
    } \
    if (i == 0) CHECK(SUCCEEDED(hr)); \
    ID3D12Device_Release(d); \
}
LADDER_TEST(0) LADDER_TEST(1) LADDER_TEST(2) LADDER_TEST(3) LADDER_TEST(4) LADDER_TEST(5)
LADDER_TEST(6) LADDER_TEST(7) LADDER_TEST(8) LADDER_TEST(9) LADDER_TEST(10)

TEST(d3d12_entry_points_device_qi_ladder_monotonic) {
    EP_NEED_DEVICE(d);
    int ok[11]; int highest = -1;
    for (int i = 0; i < 11; i++) {
        void *p = NULL; ok[i] = SUCCEEDED(ID3D12Device_QueryInterface(d, EP_DEVICE_IIDS[i], &p));
        if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
        if (ok[i]) highest = i;
    }
    for (int i = 0; i <= highest; i++) CHECKF(ok[i], "Device%d missing while Device%d present", i, highest);
    t_note("highest supported ID3D12Device interface: ID3D12Device%d", highest);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_qi_refcount) {
    EP_NEED_DEVICE(d);
    ULONG base = d3dh_refcount(d);
    IUnknown *u = NULL;
    EXPECT_HR(ID3D12Device_QueryInterface(d, &IID_IUnknown, (void **)&u), S_OK);
    EP_EQ(d3dh_refcount(d), base + 1);
    u->lpVtbl->Release(u);
    EP_EQ(d3dh_refcount(d), base);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_qi_object) {
    EP_NEED_DEVICE(d);
    ID3D12Object *o = NULL;
    EXPECT_HR(ID3D12Device_QueryInterface(d, &IID_ID3D12Object, (void **)&o), S_OK);
    if (o) { CHECK((void *)o == (void *)d); ID3D12Object_Release(o); }
    ID3D12Device_Release(d);
}

static const struct { const GUID *iid; const char *n; } k_not_on_device[] = {
    {&IID_ID3D12DeviceChild, "ID3D12DeviceChild"}, {&IID_ID3D12Pageable, "ID3D12Pageable"},
    {&IID_ID3D12Resource, "ID3D12Resource"}, {&IID_ID3D12CommandQueue, "ID3D12CommandQueue"},
    {&IID_ID3D12Fence, "ID3D12Fence"}, {&IID_ID3D12GraphicsCommandList, "ID3D12GraphicsCommandList"},
    {&IID_IDXGIObject, "IDXGIObject"}, {&IID_IDXGIDevice, "IDXGIDevice"}, {&IID_IDXGIAdapter, "IDXGIAdapter"},
    {&IID_IDXGIFactory, "IDXGIFactory"}, {&IID_ID3D12Debug, "ID3D12Debug"}, {&IID_ID3D12RootSignature, "ID3D12RootSignature"},
    {&EP_RANDOM_GUID, "random"}, {&GUID_NULL, "GUID_NULL"},
};
TEST(d3d12_entry_points_device_qi_unsupported_interfaces) {
    EP_NEED_DEVICE(d);
    for (unsigned i = 0; i < sizeof k_not_on_device / sizeof k_not_on_device[0]; i++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D12Device_QueryInterface(d, k_not_on_device[i].iid, &p);
        CHECKF(hr == E_NOINTERFACE, "%s -> %s", k_not_on_device[i].n, d3dh_hr_name(hr));
        CHECKF(p == NULL, "%s: *ppv not cleared", k_not_on_device[i].n);
        if (SUCCEEDED(hr) && p && p != (void *)1) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_qi_debug_device_observed) {
    EP_NEED_DEVICE(d);
    void *p = NULL;
    HRESULT hr = ID3D12Device_QueryInterface(d, &IID_ID3D12DebugDevice, &p);
    t_note("ID3D12DebugDevice on a non-debug device -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    p = NULL;
    hr = ID3D12Device_QueryInterface(d, &IID_ID3D12InfoQueue, &p);
    t_note("ID3D12InfoQueue on a non-debug device -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_addref_release_symmetry) {
    EP_NEED_DEVICE(d);
    ULONG base = d3dh_refcount(d);
    ULONG v[8];
    for (int i = 0; i < 8; i++) v[i] = ID3D12Device_AddRef(d);
    for (int i = 0; i < 8; i++) EP_EQ(v[i], base + 1 + i);
    for (int i = 7; i >= 0; i--) EP_EQ(ID3D12Device_Release(d), base + i);
    ID3D12Device_Release(d);
}

/* ---------- private data ---------- */
static const GUID k_pd1 = {0x11111111, 0x2222, 0x3333, {1, 2, 3, 4, 5, 6, 7, 8}};
static const GUID k_pd2 = {0x44444444, 0x5555, 0x6666, {8, 7, 6, 5, 4, 3, 2, 1}};

TEST(d3d12_entry_points_device_private_data_roundtrip) {
    EP_NEED_DEVICE(d);
    static const BYTE data[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    EXPECT_HR(ID3D12Device_SetPrivateData(d, &k_pd1, sizeof data, data), S_OK);
    BYTE out[16]; memset(out, 0xAA, sizeof out);
    UINT sz = sizeof out;
    EXPECT_HR(ID3D12Device_GetPrivateData(d, &k_pd1, &sz, out), S_OK);
    EP_EQ(sz, sizeof data);
    CHECK(!memcmp(out, data, sizeof data));
    CHECK(out[12] == 0xAA);
    EXPECT_HR(ID3D12Device_SetPrivateData(d, &k_pd1, 0, NULL), S_OK); /* delete */
    sz = sizeof out;
    EXPECT_HR(ID3D12Device_GetPrivateData(d, &k_pd1, &sz, out), DXGI_ERROR_NOT_FOUND);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_private_data_size_protocol) {
    EP_NEED_DEVICE(d);
    static const BYTE data[8] = {9, 8, 7, 6, 5, 4, 3, 2};
    EXPECT_HR(ID3D12Device_SetPrivateData(d, &k_pd2, sizeof data, data), S_OK);
    UINT sz = 0;
    HRESULT hr = ID3D12Device_GetPrivateData(d, &k_pd2, &sz, NULL);   /* size query */
    EXPECT_HR(hr, S_OK); EP_EQ(sz, 8);
    BYTE out[8]; memset(out, 0, sizeof out);
    sz = 4;
    hr = ID3D12Device_GetPrivateData(d, &k_pd2, &sz, out);
    EXPECT_HR(hr, DXGI_ERROR_MORE_DATA); EP_EQ(sz, 8);
    sz = 8; hr = ID3D12Device_GetPrivateData(d, &k_pd2, &sz, out);
    EXPECT_HR(hr, S_OK); CHECK(!memcmp(out, data, 8));
    sz = 100; hr = ID3D12Device_GetPrivateData(d, &k_pd2, &sz, out);
    EXPECT_HR(hr, S_OK); EP_EQ(sz, 8);
    ID3D12Device_SetPrivateData(d, &k_pd2, 0, NULL);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_private_data_not_found) {
    EP_NEED_DEVICE(d);
    UINT sz = 4; BYTE out[4];
    EXPECT_HR(ID3D12Device_GetPrivateData(d, &EP_RANDOM_GUID, &sz, out), DXGI_ERROR_NOT_FOUND);
    sz = 0;
    EXPECT_HR(ID3D12Device_GetPrivateData(d, &EP_RANDOM_GUID, &sz, NULL), DXGI_ERROR_NOT_FOUND);
    EP_EQ(sz, 0);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_private_data_replace) {
    EP_NEED_DEVICE(d);
    static const BYTE a[4] = {1, 1, 1, 1}; static const BYTE b[6] = {2, 2, 2, 2, 2, 2};
    EXPECT_HR(ID3D12Device_SetPrivateData(d, &k_pd1, sizeof a, a), S_OK);
    EXPECT_HR(ID3D12Device_SetPrivateData(d, &k_pd1, sizeof b, b), S_OK);
    UINT sz = 0; BYTE out[8] = {0};
    EXPECT_HR(ID3D12Device_GetPrivateData(d, &k_pd1, &sz, NULL), S_OK); EP_EQ(sz, 6);
    sz = 8; EXPECT_HR(ID3D12Device_GetPrivateData(d, &k_pd1, &sz, out), S_OK);
    CHECK(sz == 6 && !memcmp(out, b, 6));
    ID3D12Device_SetPrivateData(d, &k_pd1, 0, NULL);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_private_data_two_keys_independent) {
    EP_NEED_DEVICE(d);
    DWORD a = 0x11, b = 0x22, ra = 0, rb = 0; UINT sz = 4;
    EXPECT_HR(ID3D12Device_SetPrivateData(d, &k_pd1, 4, &a), S_OK);
    EXPECT_HR(ID3D12Device_SetPrivateData(d, &k_pd2, 4, &b), S_OK);
    EXPECT_HR(ID3D12Device_GetPrivateData(d, &k_pd1, &sz, &ra), S_OK);
    sz = 4; EXPECT_HR(ID3D12Device_GetPrivateData(d, &k_pd2, &sz, &rb), S_OK);
    EP_EQ(ra, 0x11); EP_EQ(rb, 0x22);
    ID3D12Device_SetPrivateData(d, &k_pd1, 0, NULL);
    sz = 4; EXPECT_HR(ID3D12Device_GetPrivateData(d, &k_pd2, &sz, &rb), S_OK);
    ID3D12Device_SetPrivateData(d, &k_pd2, 0, NULL);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_private_data_interface_refcount) {
    EP_NEED_DEVICE(d);
    /* use another COM object (the device's own IUnknown) as the stored interface */
    IUnknown *obj = NULL;
    EXPECT_HR(ID3D12Device_QueryInterface(d, &IID_IUnknown, (void **)&obj), S_OK);
    if (!obj) { ID3D12Device_Release(d); return; }
    ULONG before = d3dh_refcount(d);
    EXPECT_HR(ID3D12Device_SetPrivateDataInterface(d, &k_pd1, obj), S_OK);
    EP_EQ(d3dh_refcount(d), before + 1);
    IUnknown *got = NULL; UINT sz = sizeof got;
    EXPECT_HR(ID3D12Device_GetPrivateData(d, &k_pd1, &sz, &got), S_OK);
    EP_EQ(sz, sizeof(IUnknown *));
    CHECK(got == obj);
    if (got) EP_EQ(d3dh_refcount(d), before + 2); /* GetPrivateData AddRef'd the interface */
    if (got) got->lpVtbl->Release(got);
    EXPECT_HR(ID3D12Device_SetPrivateData(d, &k_pd1, 0, NULL), S_OK);
    EP_EQ(d3dh_refcount(d), before);
    obj->lpVtbl->Release(obj);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_device_set_name) {
    EP_NEED_DEVICE(d);
    EXPECT_HR(ID3D12Device_SetName(d, L"d3dt-entry-device"), S_OK);
    UINT sz = 0;
    HRESULT hr = ID3D12Device_GetPrivateData(d, &EP_WKPDID_NameW, &sz, NULL);
    t_note("name private data query -> %s size %u", d3dh_hr_name(hr), sz);
    if (SUCCEEDED(hr)) {
        WCHAR buf[64] = {0}; UINT s2 = sizeof buf;
        EXPECT_HR(ID3D12Device_GetPrivateData(d, &EP_WKPDID_NameW, &s2, buf), S_OK);
        CHECK(!lstrcmpW(buf, L"d3dt-entry-device"));
    }
    ID3D12Device_Release(d);
}

/* ---------- CheckFeatureSupport ---------- */
typedef struct { BYTE b[128]; } fbuf;
typedef void (*finit)(void *);
static void init_fmt(void *p) { D3D12_FEATURE_DATA_FORMAT_SUPPORT *s = p; s->Format = DXGI_FORMAT_R8G8B8A8_UNORM; }
static void init_ms(void *p) { D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS *s = p; s->Format = DXGI_FORMAT_R8G8B8A8_UNORM; s->SampleCount = 1; }
static void init_fi(void *p) { D3D12_FEATURE_DATA_FORMAT_INFO *s = p; s->Format = DXGI_FORMAT_R8G8B8A8_UNORM; }
static void init_sm(void *p) { D3D12_FEATURE_DATA_SHADER_MODEL *s = p; s->HighestShaderModel = D3D_SHADER_MODEL_5_1; }
static void init_rs(void *p) { D3D12_FEATURE_DATA_ROOT_SIGNATURE *s = p; s->HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1; }
static void init_pri(void *p) { D3D12_FEATURE_DATA_COMMAND_QUEUE_PRIORITY *s = p; s->CommandListType = D3D12_COMMAND_LIST_TYPE_DIRECT; s->Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; }

static const struct {
    int feature; UINT size; const char *name; int core; /* core: must succeed on any Windows 10 19041 device */ finit init;
} k_feat[] = {
#define F(e, st, core, init) { D3D12_FEATURE_##e, sizeof(D3D12_FEATURE_DATA_##st), #e, core, init }
    F(D3D12_OPTIONS, D3D12_OPTIONS, 1, NULL),
    F(ARCHITECTURE, ARCHITECTURE, 1, NULL),
    F(FORMAT_SUPPORT, FORMAT_SUPPORT, 1, init_fmt),
    F(MULTISAMPLE_QUALITY_LEVELS, MULTISAMPLE_QUALITY_LEVELS, 1, init_ms),
    F(FORMAT_INFO, FORMAT_INFO, 1, init_fi),
    F(GPU_VIRTUAL_ADDRESS_SUPPORT, GPU_VIRTUAL_ADDRESS_SUPPORT, 1, NULL),
    F(SHADER_MODEL, SHADER_MODEL, 1, init_sm),
    F(D3D12_OPTIONS1, D3D12_OPTIONS1, 1, NULL),
    F(PROTECTED_RESOURCE_SESSION_SUPPORT, PROTECTED_RESOURCE_SESSION_SUPPORT, 0, NULL),
    F(ROOT_SIGNATURE, ROOT_SIGNATURE, 1, init_rs),
    F(ARCHITECTURE1, ARCHITECTURE1, 1, NULL),
    F(D3D12_OPTIONS2, D3D12_OPTIONS2, 1, NULL),
    F(SHADER_CACHE, SHADER_CACHE, 1, NULL),
    F(COMMAND_QUEUE_PRIORITY, COMMAND_QUEUE_PRIORITY, 1, init_pri),
    F(D3D12_OPTIONS3, D3D12_OPTIONS3, 1, NULL),
    F(EXISTING_HEAPS, EXISTING_HEAPS, 1, NULL),
    F(D3D12_OPTIONS4, D3D12_OPTIONS4, 1, NULL),
    F(SERIALIZATION, SERIALIZATION, 1, NULL),
    F(CROSS_NODE, CROSS_NODE, 1, NULL),
    F(D3D12_OPTIONS5, D3D12_OPTIONS5, 0, NULL),
    F(DISPLAYABLE, DISPLAYABLE, 0, NULL),
    F(D3D12_OPTIONS6, D3D12_OPTIONS6, 0, NULL),
    F(D3D12_OPTIONS7, D3D12_OPTIONS7, 0, NULL),
    F(PROTECTED_RESOURCE_SESSION_TYPE_COUNT, PROTECTED_RESOURCE_SESSION_TYPE_COUNT, 0, NULL),
    F(D3D12_OPTIONS8, D3D12_OPTIONS8, 0, NULL),
    F(D3D12_OPTIONS9, D3D12_OPTIONS9, 0, NULL),
    F(D3D12_OPTIONS10, D3D12_OPTIONS10, 0, NULL),
    F(D3D12_OPTIONS11, D3D12_OPTIONS11, 0, NULL),
    F(D3D12_OPTIONS12, D3D12_OPTIONS12, 0, NULL),
    F(D3D12_OPTIONS13, D3D12_OPTIONS13, 0, NULL),
    F(D3D12_OPTIONS14, D3D12_OPTIONS14, 0, NULL),
    F(D3D12_OPTIONS15, D3D12_OPTIONS15, 0, NULL),
    F(D3D12_OPTIONS16, D3D12_OPTIONS16, 0, NULL),
    F(D3D12_OPTIONS17, D3D12_OPTIONS17, 0, NULL),
    F(D3D12_OPTIONS18, D3D12_OPTIONS18, 0, NULL),
#undef F
};
#define NFEAT (sizeof k_feat / sizeof k_feat[0])

static HRESULT feat_call(ID3D12Device *d, unsigned i, void *buf, UINT size) {
    memset(buf, 0, sizeof(fbuf));
    if (k_feat[i].init) k_feat[i].init(buf);
    return CFS(d, k_feat[i].feature, buf, size);
}

TEST(d3d12_entry_points_checkfeature_matrix_correct_size) {
    EP_NEED_DEVICE(d);
    for (unsigned i = 0; i < NFEAT; i++) {
        fbuf b; HRESULT hr = feat_call(d, i, &b, k_feat[i].size);
        if (k_feat[i].core) CHECKF(hr == S_OK, "%s -> %s", k_feat[i].name, d3dh_hr_name(hr));
        else { CHECKF(hr == S_OK || hr == E_INVALIDARG || hr == HR_UNSUPPORTED, "%s -> %s", k_feat[i].name, d3dh_hr_name(hr)); }
        t_note("feature %s (size %u) -> %s", k_feat[i].name, k_feat[i].size, d3dh_hr_name(hr));
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_matrix_size_minus_one) {
    EP_NEED_DEVICE(d);
    for (unsigned i = 0; i < NFEAT; i++) {
        fbuf b; HRESULT ok = feat_call(d, i, &b, k_feat[i].size);
        if (FAILED(ok)) continue;
        HRESULT hr = feat_call(d, i, &b, k_feat[i].size - 1);
        CHECKF(hr == E_INVALIDARG, "%s size-1 -> %s", k_feat[i].name, d3dh_hr_name(hr));
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_matrix_size_plus_four) {
    EP_NEED_DEVICE(d);
    for (unsigned i = 0; i < NFEAT; i++) {
        fbuf b; HRESULT ok = feat_call(d, i, &b, k_feat[i].size);
        if (FAILED(ok)) continue;
        HRESULT hr = feat_call(d, i, &b, k_feat[i].size + 4);
        CHECKF(hr == E_INVALIDARG, "%s size+4 -> %s", k_feat[i].name, d3dh_hr_name(hr));
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_matrix_size_zero) {
    EP_NEED_DEVICE(d);
    for (unsigned i = 0; i < NFEAT; i++) {
        fbuf b; HRESULT ok = feat_call(d, i, &b, k_feat[i].size);
        if (FAILED(ok)) continue;
        HRESULT hr = feat_call(d, i, &b, 0);
        CHECKF(hr == E_INVALIDARG, "%s size 0 -> %s", k_feat[i].name, d3dh_hr_name(hr));
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_matrix_null_data) {
    EP_NEED_DEVICE(d);
    for (unsigned i = 0; i < NFEAT; i++) {
        fbuf b; HRESULT ok = feat_call(d, i, &b, k_feat[i].size);
        if (FAILED(ok)) continue;
        HRESULT hr = CFS(d, k_feat[i].feature, NULL, k_feat[i].size);
        CHECKF(FAILED(hr), "%s NULL data -> %s", k_feat[i].name, d3dh_hr_name(hr));
        if (hr != E_INVALIDARG) t_note("%s NULL data -> %s", k_feat[i].name, d3dh_hr_name(hr));
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_matrix_repeatable) {
    EP_NEED_DEVICE(d);
    for (unsigned i = 0; i < NFEAT; i++) {
        fbuf a, b; HRESULT h1 = feat_call(d, i, &a, k_feat[i].size), h2 = feat_call(d, i, &b, k_feat[i].size);
        CHECKF(h1 == h2, "%s", k_feat[i].name);
        if (SUCCEEDED(h1)) CHECKF(!memcmp(&a, &b, k_feat[i].size), "%s data differs between calls", k_feat[i].name);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_unknown_values) {
    EP_NEED_DEVICE(d);
    static const int unk[] = { 9, 11, 13, 14, 15, 17, 1000, 0x7fff, 0x7fffffff, -1 };
    for (unsigned i = 0; i < sizeof unk / sizeof unk[0]; i++) {
        fbuf b; memset(&b, 0, sizeof b);
        HRESULT hr = CFS(d, (D3D12_FEATURE)unk[i], &b, 4);
        CHECKF(hr == E_INVALIDARG, "feature %d -> %s", unk[i], d3dh_hr_name(hr));
        hr = CFS(d, (D3D12_FEATURE)unk[i], &b, sizeof b);
        CHECKF(hr == E_INVALIDARG, "feature %d size %u -> %s", unk[i], (unsigned)sizeof b, d3dh_hr_name(hr));
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_options_ranges) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_D3D12_OPTIONS o; memset(&o, 0, sizeof o);
    EXPECT_HR(CFS(d, D3D12_FEATURE_D3D12_OPTIONS, &o, sizeof o), S_OK);
    CHECK(o.DoublePrecisionFloatShaderOps == TRUE || o.DoublePrecisionFloatShaderOps == FALSE);
    CHECK(o.OutputMergerLogicOp == TRUE || o.OutputMergerLogicOp == FALSE);
    CHECK(o.TypedUAVLoadAdditionalFormats == TRUE || o.TypedUAVLoadAdditionalFormats == FALSE);
    CHECK(o.ROVsSupported == TRUE || o.ROVsSupported == FALSE);
    CHECK(o.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation == TRUE || o.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation == FALSE);
    CHECK((int)o.TiledResourcesTier >= 0 && (int)o.TiledResourcesTier <= 4);
    CHECK((int)o.ResourceBindingTier >= 1 && (int)o.ResourceBindingTier <= 3);
    CHECK((int)o.ConservativeRasterizationTier >= 0 && (int)o.ConservativeRasterizationTier <= 3);
    CHECK((int)o.CrossNodeSharingTier >= 0 && (int)o.CrossNodeSharingTier <= 4);
    CHECK((int)o.ResourceHeapTier == 1 || (int)o.ResourceHeapTier == 2);
    CHECK(((int)o.MinPrecisionSupport & ~3) == 0);
    t_note("OPTIONS: tiled=%d binding=%d conservative=%d heapTier=%d rov=%d", o.TiledResourcesTier, o.ResourceBindingTier, o.ConservativeRasterizationTier, o.ResourceHeapTier, o.ROVsSupported);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_options_input_ignored) {
    /* OPTIONS is output only: prefilled garbage must be overwritten with the same answer */
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_D3D12_OPTIONS a, b; memset(&a, 0, sizeof a); memset(&b, 0xFF, sizeof b);
    EXPECT_HR(CFS(d, D3D12_FEATURE_D3D12_OPTIONS, &a, sizeof a), S_OK);
    EXPECT_HR(CFS(d, D3D12_FEATURE_D3D12_OPTIONS, &b, sizeof b), S_OK);
    CHECK(!memcmp(&a, &b, sizeof a));
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_options1_wave) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 o; memset(&o, 0, sizeof o);
    EXPECT_HR(CFS(d, D3D12_FEATURE_D3D12_OPTIONS1, &o, sizeof o), S_OK);
    if (o.WaveOps) {
        CHECK(o.WaveLaneCountMin >= 1 && o.WaveLaneCountMin <= o.WaveLaneCountMax);
        CHECK(o.TotalLaneCount >= o.WaveLaneCountMax);
    }
    t_note("OPTIONS1: waveops=%d lanes=%u..%u total=%u int64=%d", o.WaveOps, o.WaveLaneCountMin, o.WaveLaneCountMax, o.TotalLaneCount, o.Int64ShaderOps);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_architecture_node0) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_ARCHITECTURE a; memset(&a, 0, sizeof a); a.NodeIndex = 0;
    EXPECT_HR(CFS(d, D3D12_FEATURE_ARCHITECTURE, &a, sizeof a), S_OK);
    EP_EQ(a.NodeIndex, 0);
    CHECK(a.TileBasedRenderer == TRUE || a.TileBasedRenderer == FALSE);
    CHECK(a.UMA == TRUE || a.UMA == FALSE);
    if (a.CacheCoherentUMA) CHECK(a.UMA);
    t_note("ARCHITECTURE: tbr=%d uma=%d cc-uma=%d", a.TileBasedRenderer, a.UMA, a.CacheCoherentUMA);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_architecture_node_out_of_range) {
    EP_NEED_DEVICE(d);
    UINT nodes = ID3D12Device_GetNodeCount(d);
    D3D12_FEATURE_DATA_ARCHITECTURE a; memset(&a, 0, sizeof a); a.NodeIndex = nodes;
    EXPECT_HR(CFS(d, D3D12_FEATURE_ARCHITECTURE, &a, sizeof a), E_INVALIDARG);
    a.NodeIndex = 0xFFFFFFFF;
    EXPECT_HR(CFS(d, D3D12_FEATURE_ARCHITECTURE, &a, sizeof a), E_INVALIDARG);
    D3D12_FEATURE_DATA_ARCHITECTURE1 a1; memset(&a1, 0, sizeof a1); a1.NodeIndex = nodes;
    EXPECT_HR(CFS(d, D3D12_FEATURE_ARCHITECTURE1, &a1, sizeof a1), E_INVALIDARG);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_architecture1_consistent) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_ARCHITECTURE a; D3D12_FEATURE_DATA_ARCHITECTURE1 a1;
    memset(&a, 0, sizeof a); memset(&a1, 0, sizeof a1);
    EXPECT_HR(CFS(d, D3D12_FEATURE_ARCHITECTURE, &a, sizeof a), S_OK);
    EXPECT_HR(CFS(d, D3D12_FEATURE_ARCHITECTURE1, &a1, sizeof a1), S_OK);
    EP_EQ(a1.TileBasedRenderer, a.TileBasedRenderer); EP_EQ(a1.UMA, a.UMA); EP_EQ(a1.CacheCoherentUMA, a.CacheCoherentUMA);
    CHECK(a1.IsolatedMMU == TRUE || a1.IsolatedMMU == FALSE);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_feature_levels_basic) {
    EP_NEED_DEVICE(d);
    D3D_FEATURE_LEVEL req[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D12_FEATURE_DATA_FEATURE_LEVELS f; memset(&f, 0, sizeof f);
    f.NumFeatureLevels = 1; f.pFeatureLevelsRequested = req;
    EXPECT_HR(CFS(d, D3D12_FEATURE_FEATURE_LEVELS, &f, sizeof f), S_OK);
    EP_EQ(f.MaxSupportedFeatureLevel, D3D_FEATURE_LEVEL_11_0);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_feature_levels_list_order) {
    EP_NEED_DEVICE(d);
    D3D_FEATURE_LEVEL asc[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_12_1 };
    D3D_FEATURE_LEVEL desc[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D12_FEATURE_DATA_FEATURE_LEVELS f1, f2; memset(&f1, 0, sizeof f1); memset(&f2, 0, sizeof f2);
    f1.NumFeatureLevels = 4; f1.pFeatureLevelsRequested = asc;
    f2.NumFeatureLevels = 4; f2.pFeatureLevelsRequested = desc;
    EXPECT_HR(CFS(d, D3D12_FEATURE_FEATURE_LEVELS, &f1, sizeof f1), S_OK);
    EXPECT_HR(CFS(d, D3D12_FEATURE_FEATURE_LEVELS, &f2, sizeof f2), S_OK);
    EP_EQ(f1.MaxSupportedFeatureLevel, f2.MaxSupportedFeatureLevel);
    CHECK(f1.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0);
    CHECK(f1.MaxSupportedFeatureLevel == D3D_FEATURE_LEVEL_11_0 || f1.MaxSupportedFeatureLevel == D3D_FEATURE_LEVEL_11_1 ||
          f1.MaxSupportedFeatureLevel == D3D_FEATURE_LEVEL_12_0 || f1.MaxSupportedFeatureLevel == D3D_FEATURE_LEVEL_12_1);
    t_note("max supported feature level 0x%x", f1.MaxSupportedFeatureLevel);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_feature_levels_ceiling) {
    /* the answer never exceeds the highest level in the request */
    EP_NEED_DEVICE(d);
    D3D_FEATURE_LEVEL up_to[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_12_1 };
    for (unsigned n = 1; n <= 4; n++) {
        D3D12_FEATURE_DATA_FEATURE_LEVELS f; memset(&f, 0, sizeof f);
        f.NumFeatureLevels = n; f.pFeatureLevelsRequested = up_to;
        EXPECT_HR(CFS(d, D3D12_FEATURE_FEATURE_LEVELS, &f, sizeof f), S_OK);
        CHECKF(f.MaxSupportedFeatureLevel <= up_to[n - 1], "n=%u got 0x%x", n, f.MaxSupportedFeatureLevel);
        CHECKF(f.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0, "n=%u", n);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_feature_levels_unsupported_only) {
    EP_NEED_DEVICE(d);
    D3D_FEATURE_LEVEL req[] = { D3D_FEATURE_LEVEL_9_1 };
    D3D12_FEATURE_DATA_FEATURE_LEVELS f; memset(&f, 0, sizeof f);
    f.NumFeatureLevels = 1; f.pFeatureLevelsRequested = req;
    HRESULT hr = CFS(d, D3D12_FEATURE_FEATURE_LEVELS, &f, sizeof f);
    t_note("request {9_1} -> %s max=0x%x", d3dh_hr_name(hr), f.MaxSupportedFeatureLevel);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_feature_levels_empty_request) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_FEATURE_LEVELS f; memset(&f, 0, sizeof f);
    HRESULT hr = CFS(d, D3D12_FEATURE_FEATURE_LEVELS, &f, sizeof f);
    t_note("empty feature level request -> %s", d3dh_hr_name(hr));
    CHECK(hr == E_INVALIDARG || SUCCEEDED(hr));
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_format_support_rgba8) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT s; memset(&s, 0, sizeof s); s.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    EXPECT_HR(CFS(d, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s), S_OK);
    EP_EQ(s.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    D3D12_FORMAT_SUPPORT1 need = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_BLENDABLE | D3D12_FORMAT_SUPPORT1_MIP;
    CHECKF((s.Support1 & need) == need, "Support1=0x%x", s.Support1);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_format_support_buffer_formats) {
    EP_NEED_DEVICE(d);
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32B32_FLOAT };
    for (unsigned i = 0; i < sizeof fm / sizeof fm[0]; i++) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT s; memset(&s, 0, sizeof s); s.Format = fm[i];
        EXPECT_HR(CFS(d, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s), S_OK);
        CHECKF(s.Support1 & D3D12_FORMAT_SUPPORT1_BUFFER, "format %u Support1=0x%x", fm[i], s.Support1);
        CHECKF(s.Support1 & D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER, "format %u vb Support1=0x%x", fm[i], s.Support1);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_format_support_depth) {
    EP_NEED_DEVICE(d);
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT };
    for (unsigned i = 0; i < 3; i++) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT s; memset(&s, 0, sizeof s); s.Format = fm[i];
        EXPECT_HR(CFS(d, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s), S_OK);
        CHECKF(s.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL, "format %u Support1=0x%x", fm[i], s.Support1);
        CHECKF(s.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D, "format %u t2d Support1=0x%x", fm[i], s.Support1);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_format_support_compressed) {
    EP_NEED_DEVICE(d);
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC4_UNORM, DXGI_FORMAT_BC5_UNORM, DXGI_FORMAT_BC7_UNORM };
    for (unsigned i = 0; i < 6; i++) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT s; memset(&s, 0, sizeof s); s.Format = fm[i];
        EXPECT_HR(CFS(d, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s), S_OK);
        CHECKF(s.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D, "format %u Support1=0x%x", fm[i], s.Support1);
        CHECKF(!(s.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET), "BC format %u renderable", fm[i]);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_format_support_unknown_and_invalid) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT s; memset(&s, 0, sizeof s); s.Format = DXGI_FORMAT_UNKNOWN;
    HRESULT hr = CFS(d, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s);
    t_note("DXGI_FORMAT_UNKNOWN -> %s support1=0x%x", d3dh_hr_name(hr), s.Support1);
    memset(&s, 0, sizeof s); s.Format = (DXGI_FORMAT)0x7777;
    hr = CFS(d, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s);
    t_note("format 0x7777 -> %s support1=0x%x", d3dh_hr_name(hr), s.Support1);
    if (SUCCEEDED(hr)) CHECK(s.Support1 == 0);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_format_support_idempotent_inputs) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT s; memset(&s, 0xFF, sizeof s); s.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    EXPECT_HR(CFS(d, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s), S_OK);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT t; memset(&t, 0, sizeof t); t.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    EXPECT_HR(CFS(d, D3D12_FEATURE_FORMAT_SUPPORT, &t, sizeof t), S_OK);
    EP_EQ(s.Support1, t.Support1); EP_EQ(s.Support2, t.Support2);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_multisample_levels) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS m;
    memset(&m, 0, sizeof m); m.Format = DXGI_FORMAT_R8G8B8A8_UNORM; m.SampleCount = 1;
    EXPECT_HR(CFS(d, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &m, sizeof m), S_OK);
    CHECK(m.NumQualityLevels >= 1);
    memset(&m, 0, sizeof m); m.Format = DXGI_FORMAT_R8G8B8A8_UNORM; m.SampleCount = 4;
    EXPECT_HR(CFS(d, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &m, sizeof m), S_OK);
    CHECK(m.NumQualityLevels >= 1);
    for (UINT sc = 2; sc <= 16; sc *= 2) {
        memset(&m, 0, sizeof m); m.Format = DXGI_FORMAT_R8G8B8A8_UNORM; m.SampleCount = sc;
        HRESULT hr = CFS(d, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &m, sizeof m);
        if (sc == 2) EXPECT_HR(hr, S_OK);
        t_note("RGBA8 x%u -> %u quality levels", sc, m.NumQualityLevels);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_multisample_one_sample_counts_as_levels) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS m;
    memset(&m, 0xCC, sizeof m); m.Format = DXGI_FORMAT_R8G8B8A8_UNORM; m.SampleCount = 1; m.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    EXPECT_HR(CFS(d, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &m, sizeof m), S_OK);
    EP_EQ(m.SampleCount, 1); EP_EQ(m.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_multisample_invalid_counts) {
    EP_NEED_DEVICE(d);
    static const UINT sc[] = { 0, 3, 5, 6, 7, 17, 32, 33 };
    for (unsigned i = 0; i < sizeof sc / sizeof sc[0]; i++) {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS m; memset(&m, 0, sizeof m);
        m.Format = DXGI_FORMAT_R8G8B8A8_UNORM; m.SampleCount = sc[i];
        HRESULT hr = CFS(d, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &m, sizeof m);
        t_note("sample count %u -> %s levels=%u", sc[i], d3dh_hr_name(hr), m.NumQualityLevels);
        if (SUCCEEDED(hr)) CHECKF(m.NumQualityLevels == 0, "count %u reports %u levels", sc[i], m.NumQualityLevels);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_format_info_planes) {
    EP_NEED_DEVICE(d);
    static const struct { DXGI_FORMAT f; UINT planes; } t[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM, 1 }, { DXGI_FORMAT_R32_FLOAT, 1 }, { DXGI_FORMAT_BC1_UNORM, 1 },
        { DXGI_FORMAT_D32_FLOAT, 1 }, { DXGI_FORMAT_D24_UNORM_S8_UINT, 2 }, { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 2 },
        { DXGI_FORMAT_NV12, 2 }, { DXGI_FORMAT_P010, 2 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        D3D12_FEATURE_DATA_FORMAT_INFO fi; memset(&fi, 0, sizeof fi); fi.Format = t[i].f;
        HRESULT hr = CFS(d, D3D12_FEATURE_FORMAT_INFO, &fi, sizeof fi);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) { CHECKF(fi.PlaneCount == t[i].planes, "format %u planes %u want %u", t[i].f, fi.PlaneCount, t[i].planes); EP_EQ(fi.Format, t[i].f); }
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_gpu_virtual_address) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT g; memset(&g, 0, sizeof g);
    EXPECT_HR(CFS(d, D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &g, sizeof g), S_OK);
    CHECK(g.MaxGPUVirtualAddressBitsPerResource > 0 && g.MaxGPUVirtualAddressBitsPerResource <= 64);
    CHECK(g.MaxGPUVirtualAddressBitsPerProcess >= g.MaxGPUVirtualAddressBitsPerResource && g.MaxGPUVirtualAddressBitsPerProcess <= 64);
    t_note("GPU VA bits: resource=%u process=%u", g.MaxGPUVirtualAddressBitsPerResource, g.MaxGPUVirtualAddressBitsPerProcess);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_shader_model_floor) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_SHADER_MODEL sm; memset(&sm, 0, sizeof sm); sm.HighestShaderModel = D3D_SHADER_MODEL_5_1;
    EXPECT_HR(CFS(d, D3D12_FEATURE_SHADER_MODEL, &sm, sizeof sm), S_OK);
    EP_EQ(sm.HighestShaderModel, D3D_SHADER_MODEL_5_1);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_shader_model_capped_by_request) {
    EP_NEED_DEVICE(d);
    static const int req[] = { 0x51, 0x60, 0x61, 0x62 };
    int prev = 0;
    for (unsigned i = 0; i < 4; i++) {
        D3D12_FEATURE_DATA_SHADER_MODEL sm; memset(&sm, 0, sizeof sm); sm.HighestShaderModel = (D3D_SHADER_MODEL)req[i];
        HRESULT hr = CFS(d, D3D12_FEATURE_SHADER_MODEL, &sm, sizeof sm);
        if (hr == E_INVALIDARG) { t_note("shader model 0x%x unknown to this runtime", req[i]); continue; }
        EXPECT_HR(hr, S_OK);
        CHECKF((int)sm.HighestShaderModel <= req[i] && (int)sm.HighestShaderModel >= 0x51, "req 0x%x -> 0x%x", req[i], sm.HighestShaderModel);
        CHECKF((int)sm.HighestShaderModel >= prev, "answer shrank when request grew (0x%x < 0x%x)", sm.HighestShaderModel, prev);
        prev = sm.HighestShaderModel;
        t_note("shader model request 0x%x -> 0x%x", req[i], sm.HighestShaderModel);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_shader_model_bogus_request) {
    EP_NEED_DEVICE(d);
    static const int req[] = { 0x00, 0x40, 0x50, 0x7F, 0xFF, 0x1000 };
    for (unsigned i = 0; i < sizeof req / sizeof req[0]; i++) {
        D3D12_FEATURE_DATA_SHADER_MODEL sm; memset(&sm, 0, sizeof sm); sm.HighestShaderModel = (D3D_SHADER_MODEL)req[i];
        HRESULT hr = CFS(d, D3D12_FEATURE_SHADER_MODEL, &sm, sizeof sm);
        t_note("shader model request 0x%x -> %s (out 0x%x)", req[i], d3dh_hr_name(hr), sm.HighestShaderModel);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_root_signature_versions) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_ROOT_SIGNATURE rs; memset(&rs, 0, sizeof rs); rs.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    EXPECT_HR(CFS(d, D3D12_FEATURE_ROOT_SIGNATURE, &rs, sizeof rs), S_OK);
    CHECK(rs.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_0 || rs.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_1);
    t_note("highest root signature version: %d", rs.HighestVersion);
    memset(&rs, 0, sizeof rs); rs.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    EXPECT_HR(CFS(d, D3D12_FEATURE_ROOT_SIGNATURE, &rs, sizeof rs), S_OK);
    EP_EQ(rs.HighestVersion, D3D_ROOT_SIGNATURE_VERSION_1_0);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_root_signature_invalid_request) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_ROOT_SIGNATURE rs; memset(&rs, 0, sizeof rs);
    rs.HighestVersion = (D3D_ROOT_SIGNATURE_VERSION)0;
    HRESULT hr = CFS(d, D3D12_FEATURE_ROOT_SIGNATURE, &rs, sizeof rs);
    t_note("root signature request 0 -> %s", d3dh_hr_name(hr));
    rs.HighestVersion = (D3D_ROOT_SIGNATURE_VERSION)99;
    hr = CFS(d, D3D12_FEATURE_ROOT_SIGNATURE, &rs, sizeof rs);
    t_note("root signature request 99 -> %s out=%d", d3dh_hr_name(hr), rs.HighestVersion);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_queue_priority) {
    EP_NEED_DEVICE(d);
    static const D3D12_COMMAND_LIST_TYPE types[] = { D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY };
    for (unsigned i = 0; i < 3; i++) {
        D3D12_FEATURE_DATA_COMMAND_QUEUE_PRIORITY p; memset(&p, 0, sizeof p);
        p.CommandListType = types[i]; p.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        EXPECT_HR(CFS(d, D3D12_FEATURE_COMMAND_QUEUE_PRIORITY, &p, sizeof p), S_OK);
        CHECKF(p.PriorityForTypeIsSupported, "normal priority unsupported for type %d", types[i]);
        p.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
        EXPECT_HR(CFS(d, D3D12_FEATURE_COMMAND_QUEUE_PRIORITY, &p, sizeof p), S_OK);
        t_note("type %d HIGH priority supported=%d", types[i], p.PriorityForTypeIsSupported);
        p.Priority = 12345;
        HRESULT hr = CFS(d, D3D12_FEATURE_COMMAND_QUEUE_PRIORITY, &p, sizeof p);
        t_note("type %d priority 12345 -> %s supported=%d", types[i], d3dh_hr_name(hr), p.PriorityForTypeIsSupported);
        if (SUCCEEDED(hr)) CHECK(!p.PriorityForTypeIsSupported);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_queue_priority_invalid_type) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_COMMAND_QUEUE_PRIORITY p; memset(&p, 0, sizeof p);
    p.CommandListType = (D3D12_COMMAND_LIST_TYPE)99; p.Priority = 0;
    HRESULT hr = CFS(d, D3D12_FEATURE_COMMAND_QUEUE_PRIORITY, &p, sizeof p);
    t_note("invalid command list type -> %s", d3dh_hr_name(hr));
    CHECK(FAILED(hr) || !p.PriorityForTypeIsSupported);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_options_tiers_consistent) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 o3; memset(&o3, 0, sizeof o3);
    EXPECT_HR(CFS(d, D3D12_FEATURE_D3D12_OPTIONS3, &o3, sizeof o3), S_OK);
    CHECK((int)o3.CopyQueueTimestampQueriesSupported == 0 || (int)o3.CopyQueueTimestampQueriesSupported == 1);
    CHECK(((int)o3.ViewInstancingTier >= 0 && (int)o3.ViewInstancingTier <= 3));
    CHECK(((int)o3.WriteBufferImmediateSupportFlags & ~0xF) == 0);
    D3D12_FEATURE_DATA_D3D12_OPTIONS2 o2; memset(&o2, 0, sizeof o2);
    EXPECT_HR(CFS(d, D3D12_FEATURE_D3D12_OPTIONS2, &o2, sizeof o2), S_OK);
    CHECK((int)o2.ProgrammableSamplePositionsTier >= 0 && (int)o2.ProgrammableSamplePositionsTier <= 2);
    D3D12_FEATURE_DATA_D3D12_OPTIONS4 o4; memset(&o4, 0, sizeof o4);
    EXPECT_HR(CFS(d, D3D12_FEATURE_D3D12_OPTIONS4, &o4, sizeof o4), S_OK);
    CHECK((int)o4.SharedResourceCompatibilityTier >= 0 && (int)o4.SharedResourceCompatibilityTier <= 4);
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5; memset(&o5, 0, sizeof o5);
    HRESULT hr = CFS(d, D3D12_FEATURE_D3D12_OPTIONS5, &o5, sizeof o5);
    if (SUCCEEDED(hr)) {
        CHECK((int)o5.RenderPassesTier >= 0 && (int)o5.RenderPassesTier <= 2);
        CHECK((int)o5.RaytracingTier == 0 || (int)o5.RaytracingTier == 10 || (int)o5.RaytracingTier == 11);
        t_note("OPTIONS5: renderpasses=%d raytracing=%d", o5.RenderPassesTier, o5.RaytracingTier);
    }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_options6_7_ranges) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_D3D12_OPTIONS6 o6; memset(&o6, 0, sizeof o6);
    HRESULT hr = CFS(d, D3D12_FEATURE_D3D12_OPTIONS6, &o6, sizeof o6);
    if (SUCCEEDED(hr)) { CHECK((int)o6.VariableShadingRateTier >= 0 && (int)o6.VariableShadingRateTier <= 2); t_note("OPTIONS6: vrs tier=%d", o6.VariableShadingRateTier); }
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 o7; memset(&o7, 0, sizeof o7);
    hr = CFS(d, D3D12_FEATURE_D3D12_OPTIONS7, &o7, sizeof o7);
    if (SUCCEEDED(hr)) { CHECK((int)o7.MeshShaderTier == 0 || (int)o7.MeshShaderTier == 10); CHECK((int)o7.SamplerFeedbackTier == 0 || (int)o7.SamplerFeedbackTier == 90 || (int)o7.SamplerFeedbackTier == 100); }
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_existing_heaps_serialization) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_EXISTING_HEAPS e; memset(&e, 0, sizeof e);
    EXPECT_HR(CFS(d, D3D12_FEATURE_EXISTING_HEAPS, &e, sizeof e), S_OK);
    CHECK(e.Supported == TRUE || e.Supported == FALSE);
    D3D12_FEATURE_DATA_SERIALIZATION s; memset(&s, 0, sizeof s); s.NodeIndex = 0;
    EXPECT_HR(CFS(d, D3D12_FEATURE_SERIALIZATION, &s, sizeof s), S_OK);
    CHECK((int)s.HeapSerializationTier == 0 || (int)s.HeapSerializationTier == 10);
    s.NodeIndex = 7;
    HRESULT hr = CFS(d, D3D12_FEATURE_SERIALIZATION, &s, sizeof s);
    t_note("serialization node 7 -> %s", d3dh_hr_name(hr));
    D3D12_FEATURE_DATA_CROSS_NODE c; memset(&c, 0, sizeof c);
    EXPECT_HR(CFS(d, D3D12_FEATURE_CROSS_NODE, &c, sizeof c), S_OK);
    CHECK((int)c.SharingTier >= 0 && (int)c.SharingTier <= 4);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_shader_cache) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_SHADER_CACHE sc; memset(&sc, 0, sizeof sc);
    EXPECT_HR(CFS(d, D3D12_FEATURE_SHADER_CACHE, &sc, sizeof sc), S_OK);
    CHECK(((int)sc.SupportFlags & ~0x3F) == 0);
    t_note("shader cache support flags 0x%x", sc.SupportFlags);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_protected_session_node) {
    EP_NEED_DEVICE(d);
    D3D12_FEATURE_DATA_PROTECTED_RESOURCE_SESSION_SUPPORT p; memset(&p, 0, sizeof p); p.NodeIndex = 0;
    HRESULT hr = CFS(d, D3D12_FEATURE_PROTECTED_RESOURCE_SESSION_SUPPORT, &p, sizeof p);
    t_note("protected resource session support -> %s support=%d", d3dh_hr_name(hr), p.Support);
    if (SUCCEEDED(hr)) CHECK((int)p.Support == 0 || (int)p.Support == 1);
    ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_via_other_interface) {
    /* same answers through any device interface of the same object */
    EP_NEED_DEVICE(d);
    ID3D12Device1 *d1 = NULL;
    if (FAILED(ID3D12Device_QueryInterface(d, &IID_ID3D12Device1, (void **)&d1)) || !d1) { ID3D12Device_Release(d); t_skip("no ID3D12Device1"); return; }
    D3D12_FEATURE_DATA_D3D12_OPTIONS a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EXPECT_HR(ID3D12Device_CheckFeatureSupport(d, D3D12_FEATURE_D3D12_OPTIONS, &a, sizeof a), S_OK);
    EXPECT_HR(ID3D12Device1_CheckFeatureSupport(d1, D3D12_FEATURE_D3D12_OPTIONS, &b, sizeof b), S_OK);
    CHECK(!memcmp(&a, &b, sizeof a));
    EP_EQ(ID3D12Device1_GetNodeCount(d1), ID3D12Device_GetNodeCount(d));
    ID3D12Device1_Release(d1); ID3D12Device_Release(d);
}

TEST(d3d12_entry_points_checkfeature_cross_device_same_adapter) {
    EP_NEED_WARP_USABLE(ad);
    ID3D12Device *a = NULL, *b = NULL;
    EXPECT_HR(d3dh_D3D12CreateDevice()((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&a), S_OK);
    EXPECT_HR(d3dh_D3D12CreateDevice()((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&b), S_OK);
    if (a && b) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS x, y; memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
        EXPECT_HR(CFS(a, D3D12_FEATURE_D3D12_OPTIONS, &x, sizeof x), S_OK);
        EXPECT_HR(CFS(b, D3D12_FEATURE_D3D12_OPTIONS, &y, sizeof y), S_OK);
        CHECK(!memcmp(&x, &y, sizeof x));
    }
    if (a) ID3D12Device_Release(a);
    if (b) ID3D12Device_Release(b);
    SAFE_RELEASE(ad);
}

TEST(d3d12_entry_points_checkfeature_min_fl_does_not_change_caps) {
    EP_NEED_WARP_USABLE(ad);
    ID3D12Device *a = NULL; ID3D12Device *b = NULL;
    EXPECT_HR(d3dh_D3D12CreateDevice()((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&a), S_OK);
    HRESULT hr = d3dh_D3D12CreateDevice()((IUnknown *)ad, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device, (void **)&b);
    if (SUCCEEDED(hr) && a && b) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS x, y; memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
        CFS(a, D3D12_FEATURE_D3D12_OPTIONS, &x, sizeof x); CFS(b, D3D12_FEATURE_D3D12_OPTIONS, &y, sizeof y);
        CHECK(!memcmp(&x, &y, sizeof x));
    }
    if (a) ID3D12Device_Release(a);
    if (b) ID3D12Device_Release(b);
    SAFE_RELEASE(ad);
}

/* ---------- ID3D12DeviceFactory (Agility SDK only) ---------- */
typedef struct EpFactoryVtbl {
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *InitializeFromGlobalState)(void *);
    HRESULT (WINAPI *ApplyToGlobalState)(void *);
    HRESULT (WINAPI *SetFlags)(void *, UINT);
    UINT (WINAPI *GetFlags)(void *);
    HRESULT (WINAPI *GetConfigurationInterface)(void *, REFCLSID, REFIID, void **);
    HRESULT (WINAPI *EnableExperimentalFeatures)(void *, UINT, const IID *, void *, UINT *);
    HRESULT (WINAPI *CreateDevice)(void *, IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
} EpFactoryVtbl;
typedef struct { const EpFactoryVtbl *lpVtbl; } EpFactory;

TEST(d3d12_entry_points_device_factory_create_device) {
    REQUIRE(U(D3D12GetInterface));
    if (t_is_wine()) { t_skip("emulator stubs D3D12GetInterface"); return; }
    EP_NEED_WARP_USABLE(ad);
    EpFactory *f = NULL;
    HRESULT hr = U(D3D12GetInterface)(&EP_CLSID_DeviceFactory, &EP_IID_DeviceFactory, (void **)&f);
    if (FAILED(hr) || !f) { SAFE_RELEASE(ad); t_skip("ID3D12DeviceFactory unavailable (%s)", d3dh_hr_name(hr)); return; }
    ID3D12Device *d = NULL;
    hr = f->lpVtbl->CreateDevice(f, (IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&d);
    EXPECT_HR(hr, S_OK);
    if (d) { EP_EQ(ID3D12Device_Release(d), 0); }
    hr = f->lpVtbl->CreateDevice(f, (IUnknown *)ad, (D3D_FEATURE_LEVEL)0, &IID_ID3D12Device, (void **)&d);
    EXPECT_HR(hr, E_INVALIDARG);
    f->lpVtbl->Release(f);
    SAFE_RELEASE(ad);
}
