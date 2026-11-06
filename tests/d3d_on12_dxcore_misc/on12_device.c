/* D3D11On12CreateDevice parameter matrix and ID3D11On12Device basics. */
#include "odm.h"

#define NEED_ON12() PFN_D3D11ON12_CREATE_DEVICE create = odm_on12_create(); REQUIRE(create); \
    ID3D12Device *d12 = odm_dev12(); if (!d12) { t_skip("no D3D12 device"); return; }
#define END_ON12() do { SAFE_RELEASE(d12); } while (0)

static const D3D_FEATURE_LEVEL g_fl_all[] = {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

static int fl_known(D3D_FEATURE_LEVEL f) {
    switch (f) { case D3D_FEATURE_LEVEL_12_1: case D3D_FEATURE_LEVEL_12_0: case D3D_FEATURE_LEVEL_11_1: case D3D_FEATURE_LEVEL_11_0:
    case D3D_FEATURE_LEVEL_10_1: case D3D_FEATURE_LEVEL_10_0: return 1; default: return 0; }
}

TEST(d3d_on12_dxcore_misc_on12_export_present) {
    HMODULE m = t_module("d3d11.dll");
    if (!m || t_is_wine()) { t_skip("d3d11.dll missing or emulated"); return; }
    CHECK(GetProcAddress(m, "D3D11On12CreateDevice") != NULL);
    CHECK(GetProcAddress(m, "D3D11CreateDeviceForD3D12") != NULL);
}
TEST(d3d_on12_dxcore_misc_on12_export_ordinals) {
    HMODULE m = t_module("d3d11.dll");
    if (!m || t_is_wine()) { t_skip("d3d11.dll missing or emulated"); return; }
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(24)) == GetProcAddress(m, "D3D11On12CreateDevice"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(1)) == GetProcAddress(m, "D3D11CreateDeviceForD3D12"));
}
TEST(d3d_on12_dxcore_misc_on12_d3d11on12_dll_export) {
    HMODULE m = t_is_wine() ? NULL : LoadLibraryA("d3d11on12.dll");
    if (!m) { t_skip("d3d11on12.dll missing"); return; }
    CHECK(GetProcAddress(m, "OpenAdapter_D3D11On12") != NULL);
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(1)) == GetProcAddress(m, "OpenAdapter_D3D11On12"));
    SET_ERR(0);
    CHECK(GetProcAddress(m, "D3D11On12CreateDevice") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_on12_enable_feature_level_upgrade) {
    typedef int (WINAPI *efl_t)(void);
    efl_t f = (efl_t)d3dh_getproc("d3d11.dll", "EnableFeatureLevelUpgrade");
    REQUIRE(f);
    EXPECT_EQ(f(), 1);
    EXPECT_EQ(f(), 1);
}

/* ---- E_INVALIDARG matrix ---- */
TEST(d3d_on12_dxcore_misc_on12_null_device) {
    PFN_D3D11ON12_CREATE_DEVICE create = odm_on12_create(); REQUIRE(create);
    ID3D11Device *dev = (ID3D11Device *)1; ID3D11DeviceContext *ctx = (ID3D11DeviceContext *)1; D3D_FEATURE_LEVEL fl = (D3D_FEATURE_LEVEL)0x1234;
    HRESULT hr = create(NULL, 0, NULL, 0, NULL, 0, 0, &dev, &ctx, &fl);
    EXPECT_HR(hr, E_INVARG);
    CHECK(dev == NULL); CHECK(ctx == NULL);
    CHECK(fl == (D3D_FEATURE_LEVEL)0x1234);
}
TEST(d3d_on12_dxcore_misc_on12_null_device_null_outputs) {
    PFN_D3D11ON12_CREATE_DEVICE create = odm_on12_create(); REQUIRE(create);
    EXPECT_HR(create(NULL, 0, NULL, 0, NULL, 0, 0, NULL, NULL, NULL), E_INVARG);
}
TEST(d3d_on12_dxcore_misc_on12_device_not_d3d12_dxgi_factory) {
    PFN_D3D11ON12_CREATE_DEVICE create = odm_on12_create(); REQUIRE(create);
    IDXGIFactory1 *f = NULL;
    if (FAILED(d3dh_make_factory1(&f)) || !f) { t_skip("no dxgi"); return; }
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)f, 0, NULL, 0, NULL, 0, 0, &dev, NULL, NULL), E_INVARG);
    CHECK(dev == NULL);
    SAFE_RELEASE(f);
}
TEST(d3d_on12_dxcore_misc_on12_device_is_d3d11_device) {
    PFN_D3D11ON12_CREATE_DEVICE create = odm_on12_create(); REQUIRE(create);
    ID3D11Device *d11 = NULL; ID3D11DeviceContext *c11 = NULL;
    if (FAILED(d3dh_make_device11(&d11, &c11, NULL, NULL, 0))) { t_skip("no D3D11 device"); return; }
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)d11, 0, NULL, 0, NULL, 0, 0, &dev, NULL, NULL), E_INVARG);
    CHECK(dev == NULL);
    SAFE_RELEASE(c11); SAFE_RELEASE(d11);
}
TEST(d3d_on12_dxcore_misc_on12_device_is_command_queue) {
    NEED_ON12();
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0);
    if (!q) { END_ON12(); t_skip("no queue"); return; }
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)q, 0, NULL, 0, NULL, 0, 0, &dev, NULL, NULL), E_INVARG);
    CHECK(dev == NULL);
    SAFE_RELEASE(q); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_nodemask_multiple_bits) {
    NEED_ON12();
    static const UINT masks[] = {3, 5, 6, 7, 0xFF, 0x80000001, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof masks / sizeof masks[0]; i++) {
        ID3D11Device *dev = (ID3D11Device *)1; ID3D11DeviceContext *ctx = (ID3D11DeviceContext *)1; D3D_FEATURE_LEVEL fl = (D3D_FEATURE_LEVEL)0x55;
        HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, masks[i], &dev, &ctx, &fl);
        CHECKF(hr == E_INVARG, "mask 0x%x hr=%s", masks[i], d3dh_hr_name(hr));
        CHECK(dev == NULL); CHECK(ctx == NULL);
        CHECK(fl == 0);
    }
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_nodemask_zero_means_one) {
    NEED_ON12();
    ID3D11Device *dev = NULL;
    HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 0, &dev, NULL, NULL);
    if (FAILED(hr)) { t_note("create with default node mask: %s", d3dh_hr_name(hr)); END_ON12(); t_skip("on12 device not creatable"); return; }
    SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_nodemask_one_equals_zero) {
    NEED_ON12();
    ID3D11Device *a = NULL, *b = NULL;
    HRESULT h0 = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 0, &a, NULL, NULL);
    HRESULT h1 = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 1, &b, NULL, NULL);
    CHECK((h0 == S_OK) == (h1 == S_OK));
    SAFE_RELEASE(a); SAFE_RELEASE(b); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_nodemask_unavailable_node) {
    NEED_ON12();
    ID3D11Device *dev = (ID3D11Device *)1;
    UINT nodes = ID3D12Device_GetNodeCount(d12);
    if (nodes >= 2) { SAFE_RELEASE(dev); END_ON12(); t_skip("multi-node adapter"); return; }
    HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 2, &dev, NULL, NULL);
    EXPECT_HR_FAILED(hr);
    CHECK(dev == NULL);
    EXPECT_HR(hr, E_INVALIDARG); /* Win10: probed */
    t_note("node mask 2 on single-node device: %s", d3dh_hr_name(hr));
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_queue_count_without_array) {
    NEED_ON12();
    ID3D11Device *dev = (ID3D11Device *)1;
    HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, NULL, 1, 0, &dev, NULL, NULL);
    EXPECT_HR(hr, E_INVARG); CHECK(dev == NULL);
    dev = (ID3D11Device *)1;
    hr = create((IUnknown *)d12, 0, NULL, 0, NULL, 7, 0, &dev, NULL, NULL);
    EXPECT_HR(hr, E_INVARG); CHECK(dev == NULL);
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_queue_array_without_count) {
    NEED_ON12();
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0);
    if (!q) { END_ON12(); t_skip("no queue"); return; }
    IUnknown *arr[1] = {(IUnknown *)q};
    ID3D11Device *dev = (ID3D11Device *)1;
    HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, arr, 0, 0, &dev, NULL, NULL);
    EXPECT_HR(hr, E_INVARG); CHECK(dev == NULL);
    SAFE_RELEASE(q); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_queue_element_not_a_queue) {
    NEED_ON12();
    IUnknown *arr[1] = {(IUnknown *)d12};
    ID3D11Device *dev = (ID3D11Device *)1;
    HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, arr, 1, 0, &dev, NULL, NULL);
    EXPECT_HR(hr, E_INVARG); CHECK(dev == NULL);
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_queue_type_copy_rejected) {
    NEED_ON12();
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_COPY, 0, 0);
    if (!q) { END_ON12(); t_skip("no copy queue"); return; }
    IUnknown *arr[1] = {(IUnknown *)q};
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)d12, 0, NULL, 0, arr, 1, 0, &dev, NULL, NULL), E_INVARG);
    CHECK(dev == NULL);
    SAFE_RELEASE(q); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_queue_type_compute_rejected) {
    NEED_ON12();
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, 0);
    if (!q) { END_ON12(); t_skip("no compute queue"); return; }
    IUnknown *arr[1] = {(IUnknown *)q};
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)d12, 0, NULL, 0, arr, 1, 0, &dev, NULL, NULL), E_INVARG);
    CHECK(dev == NULL);
    SAFE_RELEASE(q); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_two_direct_queues_rejected) {
    NEED_ON12();
    ID3D12CommandQueue *q1 = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0), *q2 = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0);
    if (!q1 || !q2) { SAFE_RELEASE(q1); SAFE_RELEASE(q2); END_ON12(); t_skip("no queue"); return; }
    IUnknown *arr[2] = {(IUnknown *)q1, (IUnknown *)q2};
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)d12, 0, NULL, 0, arr, 2, 0, &dev, NULL, NULL), E_INVARG);
    CHECK(dev == NULL);
    SAFE_RELEASE(q1); SAFE_RELEASE(q2); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_same_queue_twice_rejected) {
    NEED_ON12();
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0);
    if (!q) { END_ON12(); t_skip("no queue"); return; }
    IUnknown *arr[2] = {(IUnknown *)q, (IUnknown *)q};
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)d12, 0, NULL, 0, arr, 2, 0, &dev, NULL, NULL), E_INVARG);
    CHECK(dev == NULL);
    SAFE_RELEASE(q); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_disable_gpu_timeout_needs_flagged_queue) {
    NEED_ON12();
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_FLAG_NONE, 0);
    if (!q) { END_ON12(); t_skip("no queue"); return; }
    IUnknown *arr[1] = {(IUnknown *)q};
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)d12, D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT, NULL, 0, arr, 1, 0, &dev, NULL, NULL), E_INVARG);
    CHECK(dev == NULL);
    SAFE_RELEASE(q); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_disable_gpu_timeout_with_flagged_queue) {
    NEED_ON12();
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT, 0);
    if (!q) { END_ON12(); t_skip("cannot create DISABLE_GPU_TIMEOUT queue"); return; }
    IUnknown *arr[1] = {(IUnknown *)q};
    ID3D11Device *dev = NULL;
    HRESULT hr = create((IUnknown *)d12, D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT, NULL, 0, arr, 1, 0, &dev, NULL, NULL);
    EXPECT_HR(hr, S_OK); /* Win10: probed */
    t_note("DISABLE_GPU_TIMEOUT + flagged queue: %s", d3dh_hr_name(hr));
    CHECK(hr != E_INVARG || dev == NULL);
    SAFE_RELEASE(dev); SAFE_RELEASE(q); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_chosen_level_overlaps_input_array) {
    NEED_ON12();
    D3D_FEATURE_LEVEL arr[4]; memcpy(arr, g_fl_all, sizeof arr);
    for (int i = 0; i < 4; i++) {
        ID3D11Device *dev = (ID3D11Device *)1;
        HRESULT hr = create((IUnknown *)d12, 0, arr, 4, NULL, 0, 0, &dev, NULL, &arr[i]);
        CHECKF(hr == E_INVARG, "overlap at %d: %s", i, d3dh_hr_name(hr));
        CHECK(dev == NULL);
    }
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_chosen_level_just_outside_array_not_overlap) {
    NEED_ON12();
    D3D_FEATURE_LEVEL buf[5]; memcpy(buf, g_fl_all, sizeof g_fl_all); buf[4] = (D3D_FEATURE_LEVEL)0;
    ID3D11Device *dev = NULL;
    HRESULT hr = create((IUnknown *)d12, 0, buf, 4, NULL, 0, 0, &dev, NULL, &buf[4]);
    EXPECT_HR(hr, S_OK); /* Win10: probed */
    t_note("chosen level adjacent to input array: %s", d3dh_hr_name(hr));
    CHECK(hr != E_INVARG);
    SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_validation_failures_do_not_leak_device_ref) {
    NEED_ON12();
    ULONG before = d3dh_refcount(d12);
    ID3D11Device *dev;
    dev = (ID3D11Device *)1; create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 3, &dev, NULL, NULL);
    dev = (ID3D11Device *)1; create((IUnknown *)d12, 0, NULL, 0, NULL, 2, 0, &dev, NULL, NULL);
    IUnknown *arr[1] = {(IUnknown *)d12};
    dev = (ID3D11Device *)1; create((IUnknown *)d12, 0, NULL, 0, arr, 1, 0, &dev, NULL, NULL);
    EXPECT_EQ(d3dh_refcount(d12), before);
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_failure_zeroes_context_output) {
    NEED_ON12();
    ID3D11Device *dev = (ID3D11Device *)1; ID3D11DeviceContext *ctx = (ID3D11DeviceContext *)1;
    EXPECT_HR(create((IUnknown *)d12, 0, NULL, 0, NULL, 1, 0, &dev, &ctx, NULL), E_INVARG);
    CHECK(dev == NULL); CHECK(ctx == NULL);
    END_ON12();
}

/* ---- successful creation ---- */
#define MAKE_ON12(flags_, nq, qarr) \
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0; \
    HRESULT hr_ = create((IUnknown *)d12, flags_, NULL, 0, qarr, nq, 0, &dev, &ctx, &fl); \
    if (FAILED(hr_)) { END_ON12(); t_skip("D3D11On12CreateDevice failed: %s", d3dh_hr_name(hr_)); return; }

TEST(d3d_on12_dxcore_misc_on12_create_default) {
    NEED_ON12();
    MAKE_ON12(0, 0, NULL);
    CHECK(dev != NULL); CHECK(ctx != NULL);
    CHECK(fl_known(fl));
    t_note("on12 feature level 0x%x", fl);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_create_no_context_output) {
    NEED_ON12();
    ID3D11Device *dev = NULL;
    HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 0, &dev, NULL, NULL);
    if (FAILED(hr)) { END_ON12(); t_skip("%s", d3dh_hr_name(hr)); return; }
    CHECK(dev != NULL);
    SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_create_context_only) {
    NEED_ON12();
    ID3D11DeviceContext *ctx = NULL;
    HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 0, NULL, &ctx, NULL);
    if (FAILED(hr)) { END_ON12(); t_skip("%s", d3dh_hr_name(hr)); return; }
    CHECK(ctx != NULL);
    if (ctx) {
        ID3D11Device *d = NULL;
        ID3D11DeviceContext_GetDevice(ctx, &d);
        CHECK(d != NULL);
        SAFE_RELEASE(d);
    }
    SAFE_RELEASE(ctx); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_context_is_immediate) {
    NEED_ON12();
    MAKE_ON12(0, 0, NULL);
    EXPECT_EQ(ID3D11DeviceContext_GetType(ctx), D3D11_DEVICE_CONTEXT_IMMEDIATE);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_feature_level_matches_device) {
    NEED_ON12();
    MAKE_ON12(0, 0, NULL);
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(dev), fl);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_feature_level_list_respected) {
    NEED_ON12();
    D3D_FEATURE_LEVEL one = D3D_FEATURE_LEVEL_11_0, got = 0;
    ID3D11Device *dev = NULL;
    HRESULT hr = create((IUnknown *)d12, 0, &one, 1, NULL, 0, 0, &dev, NULL, &got);
    if (FAILED(hr)) { END_ON12(); t_skip("%s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(got, D3D_FEATURE_LEVEL_11_0);
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(dev), D3D_FEATURE_LEVEL_11_0);
    SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_feature_level_list_count_zero_uses_default) {
    NEED_ON12();
    D3D_FEATURE_LEVEL only = D3D_FEATURE_LEVEL_9_1, got = 0;
    ID3D11Device *dev = NULL;
    HRESULT hr = create((IUnknown *)d12, 0, &only, 0, NULL, 0, 0, &dev, NULL, &got);
    if (FAILED(hr)) { END_ON12(); t_skip("%s", d3dh_hr_name(hr)); return; }
    CHECK(fl_known(got));
    SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_feature_level_unsupported_request) {
    NEED_ON12();
    D3D_FEATURE_LEVEL only = D3D_FEATURE_LEVEL_9_1, got = (D3D_FEATURE_LEVEL)0x77;
    ID3D11Device *dev = (ID3D11Device *)1;
    HRESULT hr = create((IUnknown *)d12, 0, &only, 1, NULL, 0, 0, &dev, NULL, &got);
    t_note("create with only FL 9_1: %s got=0x%x", d3dh_hr_name(hr), got);
    if (FAILED(hr)) { CHECK(dev == NULL); CHECK(got == 0); } else SAFE_RELEASE(dev);
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_with_explicit_direct_queue) {
    NEED_ON12();
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0);
    if (!q) { END_ON12(); t_skip("no queue"); return; }
    IUnknown *arr[1] = {(IUnknown *)q};
    MAKE_ON12(0, 1, arr);
    CHECK(dev != NULL);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev); SAFE_RELEASE(q); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_flags_variants) {
    NEED_ON12();
    static const UINT flags[] = {0, D3D11_CREATE_DEVICE_SINGLETHREADED, D3D11_CREATE_DEVICE_BGRA_SUPPORT, D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS};
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        ID3D11Device *dev = NULL;
        HRESULT hr = create((IUnknown *)d12, flags[i], NULL, 0, NULL, 0, 0, &dev, NULL, NULL);
        CHECKF(SUCCEEDED(hr), "flags 0x%x hr=%s", flags[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            UINT cf = ID3D11Device_GetCreationFlags(dev);
            CHECKF((cf & flags[i]) == flags[i], "creation flags 0x%x vs requested 0x%x", cf, flags[i]);
        }
        SAFE_RELEASE(dev);
    }
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_debug_flag_without_layers) {
    NEED_ON12();
    ID3D11Device *dev = NULL;
    HRESULT hr = create((IUnknown *)d12, D3D11_CREATE_DEVICE_DEBUG, NULL, 0, NULL, 0, 0, &dev, NULL, NULL);
    CHECKF(hr == S_OK || hr == (HRESULT)0x887A002D, "debug flag: %s", d3dh_hr_name(hr)); /* S_OK with the Graphics Tools feature (oracle image), SDK_COMPONENT_MISSING without */
    t_note("create with DEBUG flag: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) CHECK(dev != NULL); else CHECK(dev == NULL);
    SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_device_exposes_on12_interfaces) {
    NEED_ON12();
    MAKE_ON12(0, 0, NULL);
    ID3D11On12Device *o = NULL;
    EXPECT_HR(ID3D11Device_QueryInterface(dev, &IID_ID3D11On12Device, (void **)&o), S_OK);
    CHECK(o != NULL);
    SAFE_RELEASE(o);
    ID3D11On12Device1 *o1 = NULL;
    HRESULT h1 = ID3D11Device_QueryInterface(dev, &IID_ID3D11On12Device1, (void **)&o1);
    EXPECT_HR(h1, S_OK); /* Win10: probed */
    t_note("ID3D11On12Device1: %s", d3dh_hr_name(h1));
    SAFE_RELEASE(o1);
    ID3D11On12Device2 *o2 = NULL;
    HRESULT h2 = ID3D11Device_QueryInterface(dev, &IID_ID3D11On12Device2, (void **)&o2);
    EXPECT_HR(h2, S_OK); /* Win10: probed */
    t_note("ID3D11On12Device2: %s", d3dh_hr_name(h2));
    SAFE_RELEASE(o2);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_plain_d3d11_device_has_no_on12_interface) {
    ID3D11Device *d11 = NULL; ID3D11DeviceContext *c11 = NULL;
    if (FAILED(d3dh_make_device11(&d11, &c11, NULL, NULL, 0))) { t_skip("no D3D11 device"); return; }
    void *o = (void *)1;
    EXPECT_HR(ID3D11Device_QueryInterface(d11, &IID_ID3D11On12Device, &o), E_NOIFACE);
    CHECK(o == NULL);
    SAFE_RELEASE(c11); SAFE_RELEASE(d11);
}
TEST(d3d_on12_dxcore_misc_on12_device_same_adapter_as_d3d12) {
    NEED_ON12();
    MAKE_ON12(0, 0, NULL);
    IDXGIDevice *dd = NULL; IDXGIAdapter *ad = NULL;
    if (SUCCEEDED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd)) && dd) {
        if (SUCCEEDED(IDXGIDevice_GetAdapter(dd, &ad)) && ad) {
            DXGI_ADAPTER_DESC desc; LUID l12; memset(&l12, 0, sizeof l12); ((void *(WINAPI *)(ID3D12Device *, LUID *))d12->lpVtbl->GetAdapterLuid)(d12, &l12);
            if (SUCCEEDED(IDXGIAdapter_GetDesc(ad, &desc))) {
                CHECK(desc.AdapterLuid.LowPart == l12.LowPart && desc.AdapterLuid.HighPart == l12.HighPart);
            }
            SAFE_RELEASE(ad);
        }
        SAFE_RELEASE(dd);
    }
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_device_holds_d3d12_reference) {
    NEED_ON12();
    ULONG before = d3dh_refcount(d12);
    MAKE_ON12(0, 0, NULL);
    CHECK(d3dh_refcount(d12) > before);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    EXPECT_EQ(d3dh_refcount(d12), before);
    END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_two_devices_same_d3d12_device) {
    NEED_ON12();
    ID3D11Device *a = NULL, *b = NULL;
    HRESULT ha = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 0, &a, NULL, NULL);
    HRESULT hb = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 0, &b, NULL, NULL);
    if (FAILED(ha)) { END_ON12(); t_skip("%s", d3dh_hr_name(ha)); return; }
    t_note("second D3D11On12 device on the same D3D12 device: %s", d3dh_hr_name(hb));
    if (SUCCEEDED(hb)) CHECK(a != b);
    SAFE_RELEASE(a); SAFE_RELEASE(b); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_create_after_failed_create_still_works) {
    NEED_ON12();
    ID3D11Device *dev = (ID3D11Device *)1;
    EXPECT_HR(create((IUnknown *)d12, 0, NULL, 0, NULL, 1, 0, &dev, NULL, NULL), E_INVARG);
    dev = NULL;
    HRESULT hr = create((IUnknown *)d12, 0, NULL, 0, NULL, 0, 0, &dev, NULL, NULL);
    CHECK(SUCCEEDED(hr));
    SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_d3d11_device_threading_cap) {
    NEED_ON12();
    MAKE_ON12(0, 0, NULL);
    D3D11_FEATURE_DATA_THREADING th; memset(&th, 0xCD, sizeof th);
    HRESULT hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_THREADING, &th, sizeof th);
    EXPECT_HR(hr, S_OK);
    t_note("threading: concurrent creates=%d command lists=%d", th.DriverConcurrentCreates, th.DriverCommandLists);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev); END_ON12();
}
TEST(d3d_on12_dxcore_misc_on12_create_texture_without_wrapping) {
    NEED_ON12();
    MAKE_ON12(0, 0, NULL);
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = 16; td.Height = 16; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture2D(dev, &td, NULL, &t), S_OK);
    SAFE_RELEASE(t);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev); END_ON12();
}
