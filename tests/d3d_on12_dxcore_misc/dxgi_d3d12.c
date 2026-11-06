/* DXGI swap chains driven by a D3D12 direct command queue. */
#include "odm.h"

typedef struct { ID3D12Device *dev; ID3D12CommandQueue *q; IDXGIFactory2 *fac; HWND hwnd; } sc12;

static int sc12_init(sc12 *s, D3D12_COMMAND_LIST_TYPE type) {
    memset(s, 0, sizeof *s);
    if (t_is_wine()) return 0;
    s->dev = odm_dev12();
    if (!s->dev) return 0;
    s->q = odm_queue(s->dev, type, 0, 0);
    if (!s->q) return 0;
    if (FAILED(d3dh_make_factory2(&s->fac)) || !s->fac) return 0;
    s->hwnd = d3dh_create_hidden_window();
    return s->hwnd != NULL;
}
static void sc12_free(sc12 *s) {
    SAFE_RELEASE(s->fac); SAFE_RELEASE(s->q); SAFE_RELEASE(s->dev);
    if (s->hwnd) d3dh_destroy_window(s->hwnd);
}
#define NEED_SC12(type) sc12 s; if (!sc12_init(&s, type)) { sc12_free(&s); t_skip("no D3D12 / DXGI 1.2 / window"); return; }

static DXGI_SWAP_CHAIN_DESC1 desc1(UINT bufs, DXGI_SWAP_EFFECT eff) {
    DXGI_SWAP_CHAIN_DESC1 d; memset(&d, 0, sizeof d);
    d.Width = 64; d.Height = 64; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = bufs; d.SwapEffect = eff; d.Scaling = DXGI_SCALING_STRETCH; d.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    return d;
}
#define CREATE(s, dev_, hw_, pd_, out_) IDXGIFactory2_CreateSwapChainForHwnd((s)->fac, (IUnknown *)(dev_), hw_, pd_, NULL, NULL, out_)

TEST(d3d_on12_dxcore_misc_sc12_null_device) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    EXPECT_HR(CREATE(&s, NULL, s.hwnd, &d, &sw), DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_device_instead_of_queue) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    EXPECT_HR(CREATE(&s, s.dev, s.hwnd, &d, &sw), DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_factory_as_device) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    EXPECT_HR(CREATE(&s, s.fac, s.hwnd, &d, &sw), DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_copy_queue_rejected) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_COPY);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    EXPECT_HR(CREATE(&s, s.q, s.hwnd, &d, &sw), DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_compute_queue_rejected) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_COMPUTE);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    EXPECT_HR(CREATE(&s, s.q, s.hwnd, &d, &sw), DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_null_desc) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    EXPECT_HR(CREATE(&s, s.q, s.hwnd, NULL, &sw), DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_null_hwnd) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    EXPECT_HR(CREATE(&s, s.q, NULL, &d, &sw), DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_one_buffer_flip_rejected) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_EFFECT eff[2] = {DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL};
    for (int i = 0; i < 2; i++) {
        DXGI_SWAP_CHAIN_DESC1 d = desc1(1, eff[i]);
        IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
        HRESULT hr = CREATE(&s, s.q, s.hwnd, &d, &sw);
        CHECKF(hr == DXGI_INVALID_CALL, "effect %d: %s", i, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(sw);
    }
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_zero_buffers_rejected) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(0, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    HRESULT hr = CREATE(&s, s.q, s.hwnd, &d, &sw);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL); /* Win10: probed */
    t_note("BufferCount 0 with flip model: %s", d3dh_hr_name(hr));
    CHECK(hr == DXGI_INVALID_CALL || SUCCEEDED(hr));
    if (SUCCEEDED(hr)) { DXGI_SWAP_CHAIN_DESC1 got; IDXGISwapChain1_GetDesc1(sw, &got); CHECK(got.BufferCount >= 2); SAFE_RELEASE(sw); }
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_too_many_buffers_rejected) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(17, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    EXPECT_HR(CREATE(&s, s.q, s.hwnd, &d, &sw), DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_discard_effect_rejected) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_EFFECT eff[2] = {DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_SEQUENTIAL};
    for (int i = 0; i < 2; i++) {
        DXGI_SWAP_CHAIN_DESC1 d = desc1(2, eff[i]);
        IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
        HRESULT hr = CREATE(&s, s.q, s.hwnd, &d, &sw);
        CHECKF(hr == DXGI_INVALID_CALL, "effect %d: %s", eff[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(sw);
    }
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_multisampled_flip_rejected) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    d.SampleDesc.Count = 4;
    IDXGISwapChain1 *sw = (IDXGISwapChain1 *)1;
    HRESULT hr = CREATE(&s, s.q, s.hwnd, &d, &sw);
    CHECKF(hr == DXGI_INVALID_CALL, "%s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(sw);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_create_flip_discard_and_inspect) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = NULL;
    HRESULT hr = CREATE(&s, s.q, s.hwnd, &d, &sw);
    if (FAILED(hr)) { t_note("swap chain on this host: %s", d3dh_hr_name(hr)); sc12_free(&s); t_skip("cannot create swap chain here"); return; }
    DXGI_SWAP_CHAIN_DESC1 got; memset(&got, 0xCD, sizeof got);
    EXPECT_HR(IDXGISwapChain1_GetDesc1(sw, &got), S_OK);
    CHECK(got.Width == 64 && got.Height == 64); CHECK(got.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
    CHECK(got.BufferCount == 2); CHECK(got.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD); CHECK(got.SampleDesc.Count == 1);
    SAFE_RELEASE(sw);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_buffers_are_d3d12_resources) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(3, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = NULL;
    if (FAILED(CREATE(&s, s.q, s.hwnd, &d, &sw)) || !sw) { sc12_free(&s); t_skip("cannot create swap chain here"); return; }
    for (UINT i = 0; i < 3; i++) {
        ID3D12Resource *r = NULL;
        HRESULT hr = IDXGISwapChain1_GetBuffer(sw, i, &IID_ID3D12Resource, (void **)&r);
        CHECKF(hr == S_OK, "buffer %u: %s", i, d3dh_hr_name(hr));
        if (r) {
            D3D12_RESOURCE_DESC rd = d3dh_d12_resource_desc(r);
            CHECK(rd.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D); CHECK(rd.Width == 64 && rd.Height == 64); CHECK(rd.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
            CHECK((rd.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0);
            SAFE_RELEASE(r);
        }
    }
    ID3D12Resource *r = (ID3D12Resource *)1;
    EXPECT_HR(IDXGISwapChain1_GetBuffer(sw, 3, &IID_ID3D12Resource, (void **)&r), DXGI_INVALID_CALL);
    SAFE_RELEASE(sw);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_buffer_not_d3d11_texture) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = NULL;
    if (FAILED(CREATE(&s, s.q, s.hwnd, &d, &sw)) || !sw) { sc12_free(&s); t_skip("cannot create swap chain here"); return; }
    void *t = (void *)1;
    HRESULT hr = IDXGISwapChain1_GetBuffer(sw, 0, &IID_ID3D11Texture2D, &t);
    EXPECT_HR(hr, E_NOIFACE);
    CHECK(t == NULL);
    SAFE_RELEASE(sw);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_get_device_returns_queue) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = NULL;
    if (FAILED(CREATE(&s, s.q, s.hwnd, &d, &sw)) || !sw) { sc12_free(&s); t_skip("cannot create swap chain here"); return; }
    ID3D12CommandQueue *q = NULL; ID3D12Device *dv = NULL;
    HRESULT hq = IDXGISwapChain1_GetDevice(sw, &IID_ID3D12CommandQueue, (void **)&q);
    HRESULT hd = IDXGISwapChain1_GetDevice(sw, &IID_ID3D12Device, (void **)&dv);
    t_note("GetDevice(ID3D12CommandQueue)=%s GetDevice(ID3D12Device)=%s", d3dh_hr_name(hq), d3dh_hr_name(hd));
    /* Win10 (probed): GetDevice answers ID3D12Device but not ID3D12CommandQueue */
    EXPECT_HR(hq, E_NOINTERFACE); CHECK(q == NULL);
    EXPECT_HR(hd, S_OK); if (dv) CHECK(dv == s.dev);
    SAFE_RELEASE(q); SAFE_RELEASE(dv);
    SAFE_RELEASE(sw);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_swapchain3_current_buffer_index) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = NULL;
    if (FAILED(CREATE(&s, s.q, s.hwnd, &d, &sw)) || !sw) { sc12_free(&s); t_skip("cannot create swap chain here"); return; }
    IDXGISwapChain3 *sw3 = NULL;
    EXPECT_HR(IDXGISwapChain1_QueryInterface(sw, &IID_IDXGISwapChain3, (void **)&sw3), S_OK);
    if (sw3) {
        UINT idx = IDXGISwapChain3_GetCurrentBackBufferIndex(sw3);
        CHECK(idx < 2);
        UINT support = 0;
        HRESULT hr = IDXGISwapChain3_CheckColorSpaceSupport(sw3, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &support);
        EXPECT_HR(hr, S_OK);
        CHECK((support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) != 0);
        SAFE_RELEASE(sw3);
    }
    SAFE_RELEASE(sw);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_resize_with_outstanding_buffer_fails) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = NULL;
    if (FAILED(CREATE(&s, s.q, s.hwnd, &d, &sw)) || !sw) { sc12_free(&s); t_skip("cannot create swap chain here"); return; }
    ID3D12Resource *r = NULL;
    IDXGISwapChain1_GetBuffer(sw, 0, &IID_ID3D12Resource, (void **)&r);
    HRESULT hr = IDXGISwapChain1_ResizeBuffers(sw, 0, 32, 32, DXGI_FORMAT_UNKNOWN, 0);
    CHECK(hr == DXGI_INVALID_CALL || hr == DXGI_ERROR_INVALID_CALL);
    SAFE_RELEASE(r);
    hr = IDXGISwapChain1_ResizeBuffers(sw, 0, 32, 32, DXGI_FORMAT_UNKNOWN, 0);
    EXPECT_HR(hr, S_OK);
    DXGI_SWAP_CHAIN_DESC1 got; IDXGISwapChain1_GetDesc1(sw, &got);
    CHECK(got.Width == 32 && got.Height == 32);
    SAFE_RELEASE(sw);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_two_swapchains_one_hwnd) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *a = NULL, *b = (IDXGISwapChain1 *)1;
    if (FAILED(CREATE(&s, s.q, s.hwnd, &d, &a)) || !a) { sc12_free(&s); t_skip("cannot create swap chain here"); return; }
    HRESULT hr = CREATE(&s, s.q, s.hwnd, &d, &b);
    EXPECT_HR(hr, E_ACCESSDENIED); /* Win10 (probed): the window already owns a swap chain */
    if (SUCCEEDED(hr)) SAFE_RELEASE(b);
    SAFE_RELEASE(a);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_recreate_after_release) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *a = NULL;
    if (FAILED(CREATE(&s, s.q, s.hwnd, &d, &a)) || !a) { sc12_free(&s); t_skip("cannot create swap chain here"); return; }
    EXPECT_EQ(IDXGISwapChain1_Release(a), 0);
    a = NULL;
    HRESULT hr = CREATE(&s, s.q, s.hwnd, &d, &a);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(a);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_legacy_createswapchain_with_queue) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC dd; memset(&dd, 0, sizeof dd);
    dd.BufferDesc.Width = 64; dd.BufferDesc.Height = 64; dd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; dd.SampleDesc.Count = 1;
    dd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; dd.BufferCount = 2; dd.OutputWindow = s.hwnd; dd.Windowed = TRUE; dd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain *sw = NULL;
    HRESULT hr = IDXGIFactory2_CreateSwapChain(s.fac, (IUnknown *)s.q, &dd, &sw);
    EXPECT_HR(hr, S_OK); /* Win10: probed */
    t_note("IDXGIFactory::CreateSwapChain with a command queue: %s", d3dh_hr_name(hr));
    CHECK(SUCCEEDED(hr) || hr == DXGI_INVALID_CALL || hr == (HRESULT)0x887A0022);
    SAFE_RELEASE(sw);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_legacy_createswapchain_with_d3d12_device_rejected) {
    NEED_SC12(D3D12_COMMAND_LIST_TYPE_DIRECT);
    DXGI_SWAP_CHAIN_DESC dd; memset(&dd, 0, sizeof dd);
    dd.BufferDesc.Width = 64; dd.BufferDesc.Height = 64; dd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; dd.SampleDesc.Count = 1;
    dd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; dd.BufferCount = 2; dd.OutputWindow = s.hwnd; dd.Windowed = TRUE; dd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain *sw = (IDXGISwapChain *)1;
    HRESULT hr = IDXGIFactory2_CreateSwapChain(s.fac, (IUnknown *)s.dev, &dd, &sw);
    EXPECT_HR(hr, DXGI_INVALID_CALL);
    sc12_free(&s);
}
TEST(d3d_on12_dxcore_misc_sc12_d3d11on12_device_not_valid_swapchain_source) {
    sc12 s;
    if (!sc12_init(&s, D3D12_COMMAND_LIST_TYPE_DIRECT)) { sc12_free(&s); t_skip("no D3D12"); return; }
    PFN_D3D11ON12_CREATE_DEVICE create = odm_on12_create();
    ID3D11Device *dev = NULL;
    IUnknown *arr[1] = {(IUnknown *)s.q};
    if (!create || FAILED(create((IUnknown *)s.dev, 0, NULL, 0, arr, 1, 0, &dev, NULL, NULL)) || !dev) { sc12_free(&s); t_skip("no on12 device"); return; }
    DXGI_SWAP_CHAIN_DESC1 d = desc1(2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sw = NULL;
    HRESULT hr = CREATE(&s, dev, s.hwnd, &d, &sw);
    EXPECT_HR(hr, S_OK); /* Win10: probed */
    t_note("D3D11On12 device as swap chain device: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(sw); SAFE_RELEASE(dev);
    sc12_free(&s);
}
