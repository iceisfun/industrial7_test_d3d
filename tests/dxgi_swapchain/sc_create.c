/* Legacy IDXGIFactory::CreateSwapChain and the IDXGISwapChain base interface (windowed only, hidden window). */
#include "sc.h"

#define OPEN(s) SC s; if (!sc_open(&s, 0)) return

TEST(dxgi_swapchain_create_null_args)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc = (IDXGISwapChain *)(ULONG_PTR)0x1;
    EXPECT_HR(IDXGIFactory_CreateSwapChain(s.fac, NULL, &d, &sc), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIFactory_CreateSwapChain(s.fac, s.unk, NULL, &sc), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIFactory_CreateSwapChain(s.fac, s.unk, &d, NULL), DXGI_ERROR_INVALID_CALL);
    d.OutputWindow = NULL;
    sc = NULL;
    EXPECT_HR(IDXGIFactory_CreateSwapChain(s.fac, s.unk, &d, &sc), DXGI_ERROR_INVALID_CALL);
    CHECK(sc == NULL);
    sc_close(&s);
}

TEST(dxgi_swapchain_create_not_a_device)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc = NULL;
    /* The factory is not a device: creation must fail. */
    HRESULT hr = IDXGIFactory_CreateSwapChain(s.fac, (IUnknown *)s.fac, &d, &sc);
    PIN_HR("CreateSwapChain(factory as device)", hr, DXGI_ERROR_INVALID_CALL);
    if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_create_invalid_window)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0((HWND)(ULONG_PTR)0x7ffe0, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc = NULL;
    /* DXGI does not validate the window at creation: a stale handle, a destroyed window and the desktop window all create a chain */
    HRESULT hr = IDXGIFactory_CreateSwapChain(s.fac, s.unk, &d, &sc);
    PIN_HR("CreateSwapChain(stale HWND)", hr, S_OK);
    if (SUCCEEDED(hr)) {
        DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
        EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
        CHECK(g.OutputWindow == (HWND)(ULONG_PTR)0x7ffe0);
        EXPECT_HR(IDXGISwapChain_Present(sc, 0, 0), S_OK);
        SC_REL(sc);
    }
    HWND w = sc_window(32, 32);
    DestroyWindow(w);
    d.OutputWindow = w;
    hr = IDXGIFactory_CreateSwapChain(s.fac, s.unk, &d, &sc);
    PIN_HR("CreateSwapChain(destroyed HWND)", hr, S_OK);
    if (SUCCEEDED(hr)) SC_REL(sc);
    d.OutputWindow = GetDesktopWindow();
    hr = IDXGIFactory_CreateSwapChain(s.fac, s.unk, &d, &sc);
    PIN_HR("CreateSwapChain(desktop window)", hr, S_OK);
    if (SUCCEEDED(hr)) SC_REL(sc);
    sc_close(&s);
}

/* Buffer count / swap effect matrix for windowed chains. highest = highest GetBuffer index that must work. */
static const struct { UINT count; int effect; HRESULT hr; UINT highest; } k_matrix[] = {
    { 1, DXGI_SWAP_EFFECT_DISCARD,         S_OK, 0 },
    { 2, DXGI_SWAP_EFFECT_DISCARD,         S_OK, 0 },
    { 3, DXGI_SWAP_EFFECT_DISCARD,         S_OK, 0 },
    { 16, DXGI_SWAP_EFFECT_DISCARD,        S_OK, 0 },
    { 1, DXGI_SWAP_EFFECT_SEQUENTIAL,      S_OK, 0 },
    { 2, DXGI_SWAP_EFFECT_SEQUENTIAL,      S_OK, 1 },
    { 3, DXGI_SWAP_EFFECT_SEQUENTIAL,      S_OK, 2 },
    { 16, DXGI_SWAP_EFFECT_SEQUENTIAL,     S_OK, 15 },
    { 0, 2,                                DXGI_ERROR_INVALID_CALL, 0 },
    { 1, 2,                                DXGI_ERROR_INVALID_CALL, 0 },
    { 2, 2,                                DXGI_ERROR_INVALID_CALL, 0 },
    { 0, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_ERROR_INVALID_CALL, 0 },
    { 1, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_ERROR_INVALID_CALL, 0 },
    { 2, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, S_OK, 1 },
    { 3, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, S_OK, 2 },
    { 16, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, S_OK, 15 },
    { 17, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_ERROR_INVALID_CALL, 0 },
    { 0, DXGI_SWAP_EFFECT_FLIP_DISCARD,    DXGI_ERROR_INVALID_CALL, 0 },
    { 1, DXGI_SWAP_EFFECT_FLIP_DISCARD,    DXGI_ERROR_INVALID_CALL, 0 },
    { 2, DXGI_SWAP_EFFECT_FLIP_DISCARD,    S_OK, 0 },
    { 3, DXGI_SWAP_EFFECT_FLIP_DISCARD,    S_OK, 0 },
    { 16, DXGI_SWAP_EFFECT_FLIP_DISCARD,   S_OK, 0 },
    { 17, DXGI_SWAP_EFFECT_FLIP_DISCARD,   DXGI_ERROR_INVALID_CALL, 0 },
    { 0, 5,                                DXGI_ERROR_INVALID_CALL, 0 },
    { 1, 5,                                DXGI_ERROR_INVALID_CALL, 0 },
    { 2, 5,                                DXGI_ERROR_INVALID_CALL, 0 },
};

TEST(dxgi_swapchain_create_matrix_count_effect)
{
    OPEN(s);
    for (unsigned i = 0; i < ARRAY_SIZE_(k_matrix); i++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(w, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, k_matrix[i].count, k_matrix[i].effect);
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        if (k_matrix[i].hr == S_OK) CHECKF(sc_okish(hr), "row %u (count %u effect %d): got %s", i, k_matrix[i].count, k_matrix[i].effect, d3dh_hr_name(hr));
        else CHECKF(hr == k_matrix[i].hr, "row %u (count %u effect %d): got %s", i, k_matrix[i].count, k_matrix[i].effect, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            for (UINT j = 0; j <= k_matrix[i].highest; j++) {
                IDXGIResource *r = NULL;
                HRESULT h2 = IDXGISwapChain_GetBuffer(sc, j, &IID_IDXGIResource, (void **)&r);
                CHECKF(h2 == S_OK, "row %u GetBuffer(%u) = %s", i, j, d3dh_hr_name(h2));
                SAFE_RELEASE(r);
            }
            IDXGIResource *r = (IDXGIResource *)(ULONG_PTR)1;
            HRESULT h3 = IDXGISwapChain_GetBuffer(sc, k_matrix[i].highest + 1, &IID_IDXGIResource, (void **)&r);
            CHECKF(h3 == DXGI_ERROR_INVALID_CALL, "row %u GetBuffer(%u) = %s", i, k_matrix[i].highest + 1, d3dh_hr_name(h3));
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_getbuffer_index_far)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_SEQUENTIAL);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    void *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 2, &IID_IDXGISurface, &p), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0xFFFFFFFF, &IID_IDXGISurface, &p), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0x80000000, &IID_IDXGISurface, &p), DXGI_ERROR_INVALID_CALL);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_getbuffer_iids)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 32, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    static const struct { const IID *iid; const char *n; } ok_iids[] = {
        { &IID_ID3D11Texture2D, "ID3D11Texture2D" }, { &IID_ID3D11Resource, "ID3D11Resource" },
        { &IID_IDXGISurface, "IDXGISurface" }, { &IID_IDXGISurface1, "IDXGISurface1" },
        { &IID_IDXGIResource, "IDXGIResource" }, { &IID_IDXGIObject, "IDXGIObject" },
        { &IID_IDXGIDeviceSubObject, "IDXGIDeviceSubObject" }, { &IID_IUnknown, "IUnknown" },
        { &IID_ID3D11DeviceChild, "ID3D11DeviceChild" },
    };
    for (unsigned i = 0; i < ARRAY_SIZE_(ok_iids); i++) {
        void *p = NULL;
        HRESULT hr = IDXGISwapChain_GetBuffer(sc, 0, ok_iids[i].iid, &p);
        CHECKF(hr == S_OK && p, "GetBuffer(%s) = %s", ok_iids[i].n, d3dh_hr_name(hr));
        if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
    static const struct { const IID *iid; const char *n; } bad_iids[] = {
        { &IID_ID3D11Buffer, "ID3D11Buffer" }, { &IID_ID3D11Device, "ID3D11Device" },
        { &IID_IDXGIFactory, "IDXGIFactory" }, { &IID_IDXGISwapChain, "IDXGISwapChain" },
        { &IID_ID3D11Texture3D, "ID3D11Texture3D" },
    };
    for (unsigned i = 0; i < ARRAY_SIZE_(bad_iids); i++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = IDXGISwapChain_GetBuffer(sc, 0, bad_iids[i].iid, &p);
        CHECKF(hr == E_NOINTERFACE, "GetBuffer(%s) = %s", bad_iids[i].n, d3dh_hr_name(hr));
        CHECKF(p == NULL, "out pointer not cleared for %s", bad_iids[i].n);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_getbuffer_same_object)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 32, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_SEQUENTIAL);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    ID3D11Texture2D *a = NULL, *b = NULL, *c = NULL;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&a), S_OK);
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&b), S_OK);
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 1, &IID_ID3D11Texture2D, (void **)&c), S_OK);
    CHECK(a && a == b);
    CHECK(a != c);
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_buffer_texture_desc)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 100, 50, DXGI_FORMAT_B8G8R8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t), S_OK);
    if (t) {
        D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
        EXPECT_EQ(td.Width, 100); EXPECT_EQ(td.Height, 50);
        EXPECT_EQ(td.MipLevels, 1); EXPECT_EQ(td.ArraySize, 1);
        EXPECT_EQ(td.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
        EXPECT_EQ(td.SampleDesc.Count, 1); EXPECT_EQ(td.SampleDesc.Quality, 0);
        EXPECT_EQ(td.Usage, D3D11_USAGE_DEFAULT);
        EXPECT_EQ(td.BindFlags, D3D11_BIND_RENDER_TARGET);
        EXPECT_EQ(td.CPUAccessFlags, 0);
        EXPECT_EQ(td.MiscFlags, 0);
        SAFE_RELEASE(t);
    }
    IDXGISurface *su = NULL;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_IDXGISurface, (void **)&su), S_OK);
    if (su) {
        DXGI_SURFACE_DESC sd; memset(&sd, 0, sizeof sd);
        EXPECT_HR(IDXGISurface_GetDesc(su, &sd), S_OK);
        EXPECT_EQ(sd.Width, 100); EXPECT_EQ(sd.Height, 50);
        EXPECT_EQ(sd.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
        EXPECT_EQ(sd.SampleDesc.Count, 1);
        SAFE_RELEASE(su);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_buffer_usage_bits)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 3, DXGI_SWAP_EFFECT_SEQUENTIAL);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    for (UINT j = 0; j < 3; j++) {
        IDXGIResource *r = NULL; DXGI_USAGE u = 0;
        EXPECT_HR(IDXGISwapChain_GetBuffer(sc, j, &IID_IDXGIResource, (void **)&r), S_OK);
        if (!r) continue;
        EXPECT_HR(IDXGIResource_GetUsage(r, &u), S_OK);
        const DXGI_USAGE need = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
        CHECKF((u & need) == need, "buffer %u usage 0x%x", j, u);
        if (j > 0) CHECKF(u & DXGI_USAGE_READ_ONLY, "buffer %u should be READ_ONLY on Win10 (usage 0x%x)", j, u);
        else CHECKF(!(u & DXGI_USAGE_READ_ONLY), "buffer 0 must be writable (usage 0x%x)", u);
        SAFE_RELEASE(r);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_usage_roundtrip)
{
    OPEN(s);
    static const DXGI_USAGE tests[] = {
        0, DXGI_USAGE_BACK_BUFFER, DXGI_USAGE_SHADER_INPUT, DXGI_USAGE_RENDER_TARGET_OUTPUT, DXGI_USAGE_DISCARD_ON_PRESENT,
        DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER,
        DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_DISCARD_ON_PRESENT,
        DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_DISCARD_ON_PRESENT,
        DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT,
        DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_DISCARD_ON_PRESENT,
    };
    for (unsigned i = 0; i < ARRAY_SIZE_(tests); i++) {
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
        d.BufferUsage = tests[i];
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        CHECKF(sc_okish(hr), "usage 0x%x: %s", tests[i], d3dh_hr_name(hr));
        if (!sc) continue;
        DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
        EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
        CHECKF(g.BufferUsage == tests[i], "usage 0x%x came back as 0x%x", tests[i], g.BufferUsage);
        UINT want = 0;
        if (tests[i] & DXGI_USAGE_RENDER_TARGET_OUTPUT) want |= D3D11_BIND_RENDER_TARGET;
        if (tests[i] & DXGI_USAGE_SHADER_INPUT) want |= D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D *t = NULL;
        EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t), S_OK);
        if (t) {
            D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
            CHECKF(td.BindFlags == want, "usage 0x%x: bind flags 0x%x want 0x%x", tests[i], td.BindFlags, want);
            SAFE_RELEASE(t);
        }
        IDXGIResource *r = NULL; DXGI_USAGE u = 0;
        EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_IDXGIResource, (void **)&r), S_OK);
        if (r) {
            EXPECT_HR(IDXGIResource_GetUsage(r, &u), S_OK);
            DXGI_USAGE w = tests[i] | DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_DISCARD_ON_PRESENT;
            CHECKF(u == w, "usage 0x%x: resource usage 0x%x want 0x%x", tests[i], u, w);
            SAFE_RELEASE(r);
        }
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_ms_flip_rejected)
{
    OPEN(s);
    for (int eff = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; eff <= DXGI_SWAP_EFFECT_FLIP_DISCARD; eff++) {
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 4, eff);
        d.SampleDesc.Count = 4;
        IDXGISwapChain *sc = NULL;
        EXPECT_HR(sc_create0(&s, &d, &sc), DXGI_ERROR_INVALID_CALL);
        CHECK(sc == NULL);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_ms_bitblt)
{
    OPEN(s);
    UINT q = 0;
    ID3D11Device_CheckMultisampleQualityLevels(s.dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4, &q);
    if (!q) { t_skip("4x MSAA unsupported for R8G8B8A8_UNORM"); sc_close(&s); return; }
    for (int eff = DXGI_SWAP_EFFECT_DISCARD; eff <= DXGI_SWAP_EFFECT_SEQUENTIAL; eff++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(w, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, eff);
        d.SampleDesc.Count = 4;
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        CHECKF(sc_okish(hr), "effect %d: %s", eff, d3dh_hr_name(hr));
        if (sc) {
            DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
            EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
            EXPECT_EQ(g.SampleDesc.Count, 4);
            ID3D11Texture2D *t = NULL;
            if (SUCCEEDED(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t))) {
                D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(t, &td);
                EXPECT_EQ(td.SampleDesc.Count, 4);
                SAFE_RELEASE(t);
            }
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_ms_bad_quality)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    d.SampleDesc.Count = 1; d.SampleDesc.Quality = 1; /* quality 1 does not exist for 1 sample */
    IDXGISwapChain *sc = NULL;
    HRESULT hr = sc_create0(&s, &d, &sc);
    PIN_HR("SampleDesc{1,1}", hr, E_INVALIDARG);
    if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    d.SampleDesc.Count = 3; d.SampleDesc.Quality = 0; /* 3x never exists */
    hr = sc_create0(&s, &d, &sc);
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    sc_close(&s);
}

static const struct { DXGI_FORMAT f; int eff; int ok; const char *n; } k_fmt[] = {
    { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_EFFECT_DISCARD, 1, "R8G8B8A8" },
    { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_EFFECT_SEQUENTIAL, 1, "R8G8B8A8" },
    { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 1, "R8G8B8A8" },
    { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1, "R8G8B8A8" },
    { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_DISCARD, 1, "R8G8B8A8_SRGB" },
    { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_SEQUENTIAL, 1, "R8G8B8A8_SRGB" },
    { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0, "R8G8B8A8_SRGB" },
    { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0, "R8G8B8A8_SRGB" },
    { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_EFFECT_DISCARD, 1, "B8G8R8A8" },
    { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_EFFECT_SEQUENTIAL, 1, "B8G8R8A8" },
    { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 1, "B8G8R8A8" },
    { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1, "B8G8R8A8" },
    { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_DISCARD, 1, "B8G8R8A8_SRGB" },
    { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_SEQUENTIAL, 1, "B8G8R8A8_SRGB" },
    { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0, "B8G8R8A8_SRGB" },
    { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0, "B8G8R8A8_SRGB" },
    { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_SWAP_EFFECT_DISCARD, 1, "R10G10B10A2" },
    { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_SWAP_EFFECT_SEQUENTIAL, 1, "R10G10B10A2" },
    { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 1, "R10G10B10A2" },
    { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1, "R10G10B10A2" },
    { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_SWAP_EFFECT_DISCARD, 1, "R16G16B16A16_FLOAT" },
    { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_SWAP_EFFECT_SEQUENTIAL, 1, "R16G16B16A16_FLOAT" },
    { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 1, "R16G16B16A16_FLOAT" },
    { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_SWAP_EFFECT_FLIP_DISCARD, 1, "R16G16B16A16_FLOAT" },
    { DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0, "XR_BIAS" },
    { DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 0, "XR_BIAS" },
};

TEST(dxgi_swapchain_create_format_matrix)
{
    OPEN(s);
    for (unsigned i = 0; i < ARRAY_SIZE_(k_fmt); i++) {
        HWND w = sc_window(64, 64);
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(w, 64, 64, k_fmt[i].f, 4, k_fmt[i].eff);
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        if (k_fmt[i].ok) CHECKF(sc_okish(hr), "%s effect %d: %s", k_fmt[i].n, k_fmt[i].eff, d3dh_hr_name(hr));
        else CHECKF(hr == DXGI_ERROR_INVALID_CALL, "%s effect %d: %s", k_fmt[i].n, k_fmt[i].eff, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
            EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
            CHECKF(g.BufferDesc.Format == k_fmt[i].f, "%s: format came back %d", k_fmt[i].n, g.BufferDesc.Format);
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_create_format_unknown)
{
    OPEN(s);
    for (int eff = 0; eff <= DXGI_SWAP_EFFECT_FLIP_DISCARD; eff++) {
        if (eff == 2) continue;
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_UNKNOWN, 2, eff);
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        /* bit-blt models: E_INVALIDARG; flip models: DXGI_ERROR_INVALID_CALL */
        EXPECT_HR(hr, sc_is_flip((DXGI_SWAP_EFFECT)eff) ? DXGI_ERROR_INVALID_CALL : E_INVALIDARG);
        if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_create_format_rejected_misc)
{
    OPEN(s);
    static const DXGI_FORMAT bad[] = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_UINT,
        DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_NV12, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_B8G8R8X8_UNORM };
    for (unsigned i = 0; i < ARRAY_SIZE_(bad); i++) {
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, bad[i], 1, DXGI_SWAP_EFFECT_DISCARD);
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        if (SUCCEEDED(hr)) { t_note("format %d accepted by bitblt CreateSwapChain", bad[i]); SAFE_RELEASE(sc); }
        else PINF(hr, "format %d", bad[i]);
    }
    /* Depth/compressed/video formats cannot be a back buffer. */
    static const DXGI_FORMAT hard[] = { DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_NV12 };
    for (unsigned i = 0; i < ARRAY_SIZE_(hard); i++) {
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, hard[i], 1, DXGI_SWAP_EFFECT_DISCARD);
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        CHECKF(FAILED(hr), "format %d accepted", hard[i]);
        if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_create_zero_size_uses_client)
{
    OPEN(s);
    static const struct { int w, h; } t[] = { { 0, 0 }, { 200, 0 }, { 0, 200 }, { 200, 200 }, { 100, 60 } };
    for (unsigned i = 0; i < ARRAY_SIZE_(t); i++) {
        HWND w = sc_window(t[i].w, t[i].h);
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(w, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        CHECKF(sc_okish(hr), "window %dx%d: %s", t[i].w, t[i].h, d3dh_hr_name(hr));
        if (sc) {
            RECT r = { 0 }; GetClientRect(w, &r);
            DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
            EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
            UINT ew = r.right > 0 ? r.right : 8, eh = r.bottom > 0 ? r.bottom : 8;
            CHECKF(g.BufferDesc.Width == ew && g.BufferDesc.Height == eh, "window %dx%d client %ldx%ld: buffer %ux%u want %ux%u",
                   t[i].w, t[i].h, r.right, r.bottom, g.BufferDesc.Width, g.BufferDesc.Height, ew, eh);
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_create_input_desc_unmodified)
{
    OPEN(s);
    HWND w = sc_window(90, 70);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(w, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD), keep = d;
    IDXGISwapChain *sc = NULL;
    HRESULT hr = IDXGIFactory_CreateSwapChain(s.fac, s.unk, &d, &sc);
    CHECK(sc_okish(hr));
    CHECK(memcmp(&d, &keep, sizeof d) == 0);
    if (sc) SC_REL(sc);
    DestroyWindow(w);
    sc_close(&s);
}

TEST(dxgi_swapchain_refresh_rate_roundtrip)
{
    OPEN(s);
    static const struct { UINT n, d; } rr[] = { { 60, 60 }, { 60, 0 }, { 60, 1 }, { 0, 60 }, { 0, 0 }, { 59940, 1000 } };
    for (unsigned i = 0; i < ARRAY_SIZE_(rr); i++) {
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
        d.BufferDesc.RefreshRate.Numerator = rr[i].n; d.BufferDesc.RefreshRate.Denominator = rr[i].d;
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        CHECKF(sc_okish(hr), "rate %u/%u: %s", rr[i].n, rr[i].d, d3dh_hr_name(hr));
        if (!sc) continue;
        DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
        EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
        CHECKF(g.BufferDesc.RefreshRate.Numerator == rr[i].n && g.BufferDesc.RefreshRate.Denominator == rr[i].d,
               "rate %u/%u came back %u/%u", rr[i].n, rr[i].d, g.BufferDesc.RefreshRate.Numerator, g.BufferDesc.RefreshRate.Denominator);
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_scanline_scaling_roundtrip)
{
    OPEN(s);
    for (int so = 0; so <= 3; so++) for (int sc_ = 0; sc_ <= 2; sc_++) {
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
        d.BufferDesc.ScanlineOrdering = so; d.BufferDesc.Scaling = sc_;
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        if (!SUCCEEDED(hr)) { t_note("scanline %d scaling %d -> %s", so, sc_, d3dh_hr_name(hr)); continue; }
        DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
        IDXGISwapChain_GetDesc(sc, &g);
        CHECKF(g.BufferDesc.ScanlineOrdering == (DXGI_MODE_SCANLINE_ORDER)so, "scanline %d -> %d", so, g.BufferDesc.ScanlineOrdering);
        CHECKF(g.BufferDesc.Scaling == (DXGI_MODE_SCALING)sc_, "scaling %d -> %d", sc_, g.BufferDesc.Scaling);
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_getdesc_null)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    EXPECT_HR(IDXGISwapChain_GetDesc(sc, NULL), E_INVALIDARG);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_getdesc_roundtrip)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 77, 33, DXGI_FORMAT_B8G8R8A8_UNORM, 3, DXGI_SWAP_EFFECT_SEQUENTIAL);
    d.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    DXGI_SWAP_CHAIN_DESC g; memset(&g, 0xCC, sizeof g);
    EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
    EXPECT_EQ(g.BufferDesc.Width, 77); EXPECT_EQ(g.BufferDesc.Height, 33);
    EXPECT_EQ(g.BufferDesc.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(g.SampleDesc.Count, 1); EXPECT_EQ(g.SampleDesc.Quality, 0);
    EXPECT_EQ(g.BufferUsage, DXGI_USAGE_RENDER_TARGET_OUTPUT);
    EXPECT_EQ(g.BufferCount, 3);
    EXPECT_EQ(g.OutputWindow, s.hwnd);
    EXPECT_EQ(g.Windowed, TRUE);
    EXPECT_EQ(g.SwapEffect, DXGI_SWAP_EFFECT_SEQUENTIAL);
    EXPECT_EQ(g.Flags, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_getparent_factory)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    IUnknown *p = NULL;
    EXPECT_HR(IDXGISwapChain_GetParent(sc, &IID_IUnknown, (void **)&p), S_OK);
    CHECK(p == (IUnknown *)s.fac);
    SAFE_RELEASE(p);
    EXPECT_HR(IDXGISwapChain_GetParent(sc, &IID_IDXGIFactory, (void **)&p), S_OK);
    CHECK(p == (IUnknown *)s.fac);
    SAFE_RELEASE(p);
    EXPECT_HR(IDXGISwapChain_GetParent(sc, &IID_IDXGIFactory1, (void **)&p), S_OK);
    SAFE_RELEASE(p);
    p = (IUnknown *)(ULONG_PTR)1;
    HRESULT hr = IDXGISwapChain_GetParent(sc, &IID_ID3D11Device, (void **)&p);
    CHECK(FAILED(hr));
    CHECK(p == NULL);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_getdevice)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    ID3D11Device *dv = NULL;
    EXPECT_HR(IDXGISwapChain_GetDevice(sc, &IID_ID3D11Device, (void **)&dv), S_OK);
    CHECK(dv == s.dev);
    SAFE_RELEASE(dv);
    IDXGIDevice *dd = NULL;
    EXPECT_HR(IDXGISwapChain_GetDevice(sc, &IID_IDXGIDevice, (void **)&dd), S_OK);
    SAFE_RELEASE(dd);
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = IDXGISwapChain_GetDevice(sc, &IID_IDXGIFactory, &p);
    CHECK(FAILED(hr));
    CHECK(p == NULL);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_refcounts_restored)
{
    OPEN(s);
    ULONG fac0 = d3dh_refcount(s.fac), dev0 = d3dh_refcount(s.dev);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_SEQUENTIAL);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    CHECK(d3dh_refcount(s.dev) > dev0);   /* the chain keeps its device alive */
    ID3D11Texture2D *t = NULL;
    IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t);
    SAFE_RELEASE(t);
    SC_REL(sc);
    EXPECT_EQ(d3dh_refcount(s.fac), fac0);
    EXPECT_EQ(d3dh_refcount(s.dev), dev0);
    sc_close(&s);
}

TEST(dxgi_swapchain_qi_interfaces)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    static const struct { const IID *iid; const char *n; } ifs[] = {
        { &IID_IUnknown, "IUnknown" }, { &IID_IDXGIObject, "IDXGIObject" }, { &IID_IDXGIDeviceSubObject, "IDXGIDeviceSubObject" },
        { &IID_IDXGISwapChain, "IDXGISwapChain" }, { &IID_IDXGISwapChain1, "IDXGISwapChain1" }, { &IID_IDXGISwapChain2, "IDXGISwapChain2" },
        { &IID_IDXGISwapChain3, "IDXGISwapChain3" },
    };
    for (unsigned i = 0; i < ARRAY_SIZE_(ifs); i++) {
        IUnknown *u = NULL;
        HRESULT hr = IDXGISwapChain_QueryInterface(sc, ifs[i].iid, (void **)&u);
        CHECKF(hr == S_OK && u, "QI %s = %s", ifs[i].n, d3dh_hr_name(hr));
        if (u) u->lpVtbl->Release(u);
    }
    /* IDXGISwapChain4 appeared in Windows 10 1607 */
    IUnknown *u4 = NULL;
    HRESULT hr = IDXGISwapChain_QueryInterface(sc, &IID_IDXGISwapChain4, (void **)&u4);
    PIN_HR("QI IDXGISwapChain4", hr, S_OK);
    if (u4) u4->lpVtbl->Release(u4);
    /* identity: IUnknown from different interfaces is the same pointer */
    IUnknown *a = NULL, *b = NULL; IDXGISwapChain1 *s1 = NULL;
    IDXGISwapChain_QueryInterface(sc, &IID_IUnknown, (void **)&a);
    if (SUCCEEDED(IDXGISwapChain_QueryInterface(sc, &IID_IDXGISwapChain1, (void **)&s1))) {
        IDXGISwapChain1_QueryInterface(s1, &IID_IUnknown, (void **)&b);
        CHECK(a == b);
        SAFE_RELEASE(b); SAFE_RELEASE(s1);
    }
    SAFE_RELEASE(a);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_qi_unsupported)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    static const IID *bad[] = { &IID_IDXGIFactory, &IID_IDXGIAdapter, &IID_IDXGIDevice, &IID_ID3D11Device, &IID_IDXGISurface };
    for (unsigned i = 0; i < ARRAY_SIZE_(bad); i++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = IDXGISwapChain_QueryInterface(sc, bad[i], &p);
        CHECKF(hr == E_NOINTERFACE, "QI #%u = %s", i, d3dh_hr_name(hr));
        CHECKF(p == NULL, "out not cleared #%u", i);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_fullscreen_state_default)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    BOOL fs = 0xDEAD; IDXGIOutput *out = (IDXGIOutput *)(ULONG_PTR)0xDEAD;
    EXPECT_HR(IDXGISwapChain_GetFullscreenState(sc, &fs, &out), S_OK);
    EXPECT_EQ(fs, 0); CHECK(out == NULL);
    EXPECT_HR(IDXGISwapChain_GetFullscreenState(sc, NULL, NULL), S_OK);
    fs = 0xDEAD;
    EXPECT_HR(IDXGISwapChain_GetFullscreenState(sc, &fs, NULL), S_OK);
    EXPECT_EQ(fs, 0);
    out = (IDXGIOutput *)(ULONG_PTR)0xDEAD;
    EXPECT_HR(IDXGISwapChain_GetFullscreenState(sc, NULL, &out), S_OK);
    CHECK(out == NULL);
    /* leaving fullscreen while windowed is a successful no-op */
    EXPECT_HR(IDXGISwapChain_SetFullscreenState(sc, FALSE, NULL), S_OK);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_private_data)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    static const GUID g1 = { 0x11111111, 0x2222, 0x3333, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    static const GUID g2 = { 0x44444444, 0x5555, 0x6666, { 8, 7, 6, 5, 4, 3, 2, 1 } };
    UINT sz = 4; char buf[16] = { 0 };
    EXPECT_HR(IDXGISwapChain_GetPrivateData(sc, &g1, &sz, buf), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(sz, 0);
    const char data[8] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h' };
    EXPECT_HR(IDXGISwapChain_SetPrivateData(sc, &g1, 8, data), S_OK);
    sz = 8;
    EXPECT_HR(IDXGISwapChain_GetPrivateData(sc, &g1, &sz, buf), S_OK);
    EXPECT_EQ(sz, 8); CHECK(!memcmp(buf, data, 8));
    sz = 4;
    EXPECT_HR(IDXGISwapChain_GetPrivateData(sc, &g1, &sz, buf), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 8);
    sz = 16;
    EXPECT_HR(IDXGISwapChain_GetPrivateData(sc, &g1, &sz, buf), S_OK);
    EXPECT_EQ(sz, 8);
    sz = 8;
    EXPECT_HR(IDXGISwapChain_GetPrivateData(sc, &g2, &sz, buf), DXGI_ERROR_NOT_FOUND);
    /* replace, then zero-size removal */
    EXPECT_HR(IDXGISwapChain_SetPrivateData(sc, &g1, 3, "xyz"), S_OK);
    sz = 16;
    EXPECT_HR(IDXGISwapChain_GetPrivateData(sc, &g1, &sz, buf), S_OK);
    EXPECT_EQ(sz, 3);
    EXPECT_HR(IDXGISwapChain_SetPrivateData(sc, &g1, 0, NULL), S_OK);
    sz = 16;
    EXPECT_HR(IDXGISwapChain_GetPrivateData(sc, &g1, &sz, buf), DXGI_ERROR_NOT_FOUND);
    /* interface payload is AddRef'd and dropped with the chain */
    ULONG before = d3dh_refcount(s.fac);
    EXPECT_HR(IDXGISwapChain_SetPrivateDataInterface(sc, &g2, (IUnknown *)s.fac), S_OK);
    EXPECT_EQ(d3dh_refcount(s.fac), before + 1);
    IUnknown *got = NULL; sz = sizeof got;
    EXPECT_HR(IDXGISwapChain_GetPrivateData(sc, &g2, &sz, &got), S_OK);
    CHECK(got == (IUnknown *)s.fac);
    EXPECT_EQ(sz, sizeof(IUnknown *));
    SAFE_RELEASE(got);
    EXPECT_HR(IDXGISwapChain_SetPrivateDataInterface(sc, &g2, NULL), S_OK);
    EXPECT_EQ(d3dh_refcount(s.fac), before);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_second_chain_same_window)
{
    OPEN(s);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *a, *b = NULL;
    if (!sc_okish(sc_create0(&s, &d, &a))) { CHECK(0); sc_close(&s); return; }
    HRESULT hr = sc_create0(&s, &d, &b);
    PIN_HR("second CreateSwapChain on the same HWND", hr, S_OK);
    SAFE_RELEASE(b);
    SC_REL(a);
    /* after the first is released the window can be reused */
    hr = sc_create0(&s, &d, &b);
    CHECK(sc_okish(hr));
    if (b) SC_REL(b);
    sc_close(&s);
}

TEST(dxgi_swapchain_create_flags_legacy)
{
    OPEN(s);
    static const struct { UINT f; const char *n; } fl[] = {
        { DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH, "ALLOW_MODE_SWITCH" },
        { DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE, "GDI_COMPATIBLE" },
        { DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT, "RESTRICTED_CONTENT" },
        { DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER, "RESTRICT_SHARED_RESOURCE_DRIVER" },
    };
    for (unsigned i = 0; i < ARRAY_SIZE_(fl); i++) {
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
        d.Flags = fl[i].f;
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        if (FAILED(hr)) { t_note("flag %s on a WARP device -> %s", fl[i].n, d3dh_hr_name(hr)); continue; }
        DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
        IDXGISwapChain_GetDesc(sc, &g);
        CHECKF(g.Flags == fl[i].f, "flag %s came back as 0x%x", fl[i].n, g.Flags);
        SC_REL(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_create_flags_unsupported_for_bitblt)
{
    OPEN(s);
    /* Waitable object and tearing are flip-model only. */
    static const UINT fl[] = { DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING };
    for (unsigned i = 0; i < ARRAY_SIZE_(fl); i++) for (int eff = 0; eff <= 1; eff++) {
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 2, eff);
        d.Flags = fl[i];
        IDXGISwapChain *sc = NULL;
        HRESULT hr = sc_create0(&s, &d, &sc);
        CHECKF(FAILED(hr), "flag 0x%x with bitblt effect %d accepted", fl[i], eff);
        PINF(hr, "flag 0x%x bitblt effect %d", fl[i], eff);
        if (SUCCEEDED(hr)) SAFE_RELEASE(sc);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_create_gdi_compatible_surface)
{
    SC s;
    if (!sc_open(&s, D3D11_CREATE_DEVICE_BGRA_SUPPORT)) return;
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 64, 64, DXGI_FORMAT_B8G8R8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    d.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;
    IDXGISwapChain *sc = NULL;
    HRESULT hr = sc_create0(&s, &d, &sc);
    if (FAILED(hr)) { t_note("GDI_COMPATIBLE chain on BGRA device -> %s", d3dh_hr_name(hr)); sc_close(&s); return; }
    IDXGISurface1 *su = NULL;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_IDXGISurface1, (void **)&su), S_OK);
    if (su) {
        HDC dc = NULL;
        hr = IDXGISurface1_GetDC(su, FALSE, &dc);
        PIN_HR("IDXGISurface1::GetDC on GDI-compatible back buffer", hr, S_OK);
        if (SUCCEEDED(hr)) { CHECK(dc != NULL); IDXGISurface1_ReleaseDC(su, NULL); }
        SAFE_RELEASE(su);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_create_many_sequential)
{
    OPEN(s);
    for (int i = 0; i < 20; i++) {
        HWND w = sc_window(32, 32);
        DXGI_SWAP_CHAIN_DESC d = sc_desc0(w, 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_SEQUENTIAL);
        IDXGISwapChain *sc = NULL;
        CHECK(sc_okish(sc_create0(&s, &d, &sc)));
        if (sc) {
            CHECK(sc_okish(IDXGISwapChain_Present(sc, 0, 0)));
            SC_REL(sc);
        }
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_destroy_window_before_release)
{
    OPEN(s);
    HWND w = sc_window(32, 32);
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(w, 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc = NULL;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); DestroyWindow(w); sc_close(&s); return; }
    DestroyWindow(w);
    /* Buffers stay accessible; the chain object is still valid. */
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t), S_OK);
    SAFE_RELEASE(t);
    DXGI_SWAP_CHAIN_DESC g; memset(&g, 0, sizeof g);
    EXPECT_HR(IDXGISwapChain_GetDesc(sc, &g), S_OK);
    HRESULT hr = IDXGISwapChain_Present(sc, 0, 0);
    PIN_HR("Present after DestroyWindow", hr, S_OK);
    SC_REL(sc);
    sc_close(&s);
}
