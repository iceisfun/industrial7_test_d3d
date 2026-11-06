/* IDXGISwapChain1..4 extras: frame latency, waitable object, rotation, source size, background colour,
 * matrix transform, colour space, HDR metadata, temporary mono. */
#include "sc.h"

#define OPEN2(s) SC s; if (!sc_open2(&s, 0)) return

static IDXGISwapChain2 *mk2(SC *s, DXGI_SWAP_EFFECT eff, UINT flags, UINT count)
{
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, count, eff);
    d.Flags = flags;
    IDXGISwapChain1 *sc = NULL; IDXGISwapChain2 *s2 = NULL;
    HRESULT hr = sc_create1(s, s->hwnd, &d, &sc);
    if (!sc_okish(hr)) { CHECKF(0, "create: %s", d3dh_hr_name(hr)); return NULL; }
    hr = IDXGISwapChain1_QueryInterface(sc, &IID_IDXGISwapChain2, (void **)&s2);
    IDXGISwapChain1_Release(sc);
    if (FAILED(hr)) { t_skip("IDXGISwapChain2 unavailable"); return NULL; }
    return s2;
}
static IDXGISwapChain3 *mk3(SC *s, DXGI_SWAP_EFFECT eff, DXGI_FORMAT f, UINT count)
{
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, f, count, eff);
    IDXGISwapChain1 *sc = NULL; IDXGISwapChain3 *s3 = NULL;
    HRESULT hr = sc_create1(s, s->hwnd, &d, &sc);
    if (!sc_okish(hr)) { CHECKF(0, "create: %s", d3dh_hr_name(hr)); return NULL; }
    hr = IDXGISwapChain1_QueryInterface(sc, &IID_IDXGISwapChain3, (void **)&s3);
    IDXGISwapChain1_Release(sc);
    if (FAILED(hr)) { t_skip("IDXGISwapChain3 unavailable"); return NULL; }
    return s3;
}

TEST(dxgi_swapchain_latency_without_flag)
{
    OPEN2(s);
    static const int effs[] = { DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD };
    for (unsigned i = 0; i < 4; i++) {
        IDXGISwapChain2 *s2 = mk2(&s, effs[i], 0, 2);
        if (!s2) continue;
        UINT lat = 0xDEADBEEF;
        EXPECT_HR(IDXGISwapChain2_GetMaximumFrameLatency(s2, &lat), DXGI_ERROR_INVALID_CALL);
        EXPECT_EQ(lat, 0xDEADBEEF);
        EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, 1), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, 0), DXGI_ERROR_INVALID_CALL);
        CHECK(IDXGISwapChain2_GetFrameLatencyWaitableObject(s2) == NULL);
        SC_REL(s2);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_latency_default_is_one)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, 2);
    if (!s2) { sc_close(&s); return; }
    UINT lat = 0;
    EXPECT_HR(IDXGISwapChain2_GetMaximumFrameLatency(s2, &lat), S_OK);
    EXPECT_EQ(lat, 1);
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_latency_set_get_roundtrip)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, 3);
    if (!s2) { sc_close(&s); return; }
    EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, 0), DXGI_ERROR_INVALID_CALL);
    UINT lat = 0;
    /* Win10 22H2: 1..40 are accepted (the documented DXGI maximum of 16 is not enforced), 41 and up are rejected. */
    static const UINT okv[] = { 1, 2, 3, 4, 8, 15, 16, 17, 20, 39, 40 }, badv[] = { 41, 42, 64, 100, 0x7fffffff, 0xFFFFFFFF };
    for (unsigned i = 0; i < ARRAY_SIZE_(okv); i++) {
        EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, okv[i]), S_OK);
        EXPECT_HR(IDXGISwapChain2_GetMaximumFrameLatency(s2, &lat), S_OK);
        EXPECT_EQ(lat, okv[i]);
    }
    for (unsigned i = 0; i < ARRAY_SIZE_(badv); i++) {
        EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, badv[i]), DXGI_ERROR_INVALID_CALL);
        IDXGISwapChain2_GetMaximumFrameLatency(s2, &lat);
        EXPECT_EQ(lat, 40);               /* a rejected value does not change the stored one */
    }
    EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, 0), DXGI_ERROR_INVALID_CALL);
    PIN_HR("GetMaximumFrameLatency(NULL)", IDXGISwapChain2_GetMaximumFrameLatency(s2, NULL), DXGI_ERROR_INVALID_CALL);
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_latency_waitable_handles)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, 2);
    if (!s2) { sc_close(&s); return; }
    HANDLE a = IDXGISwapChain2_GetFrameLatencyWaitableObject(s2);
    HANDLE b = IDXGISwapChain2_GetFrameLatencyWaitableObject(s2);
    CHECK(a != NULL && b != NULL);
    CHECK(a != b);   /* each call returns a new duplicated handle */
    DWORD flags = 0;
    CHECK(a && GetHandleInformation(a, &flags));
    if (a) CHECK(CloseHandle(a));
    if (b) CHECK(CloseHandle(b));
    /* a handle may be fetched again after both were closed */
    a = IDXGISwapChain2_GetFrameLatencyWaitableObject(s2);
    CHECK(a != NULL);
    if (a) {
        /* initially signalled once for the default latency of 1 */
        EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_TIMEOUT);
        CloseHandle(a);
    }
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_latency_raise_releases_waiters)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, 3);
    if (!s2) { sc_close(&s); return; }
    HANDLE h = IDXGISwapChain2_GetFrameLatencyWaitableObject(s2);
    if (!h) { CHECK(0); SC_REL(s2); sc_close(&s); return; }
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, 3), S_OK);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 50), WAIT_TIMEOUT);
    CloseHandle(h);
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_latency_present_signals)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, 2);
    if (!s2) { sc_close(&s); return; }
    HANDLE h = IDXGISwapChain2_GetFrameLatencyWaitableObject(s2);
    if (!h) { CHECK(0); SC_REL(s2); sc_close(&s); return; }
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) {
        EXPECT_HR(IDXGISwapChain2_Present(s2, 0, 0), S_OK);
        /* each frame that completes releases one count; allow the presentation thread some time */
        DWORD r = WaitForSingleObject(h, 2000);
        CHECKF(r == WAIT_OBJECT_0, "frame %d: wait result %lu", i, r);
    }
    CloseHandle(h);
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_latency_device_level)
{
    OPEN2(s);
    IDXGIDevice1 *d1 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(s.dev, &IID_IDXGIDevice1, (void **)&d1))) { t_skip("IDXGIDevice1 unavailable"); sc_close(&s); return; }
    UINT lat = 0;
    EXPECT_HR(IDXGIDevice1_GetMaximumFrameLatency(d1, NULL), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIDevice1_GetMaximumFrameLatency(d1, &lat), S_OK);
    EXPECT_EQ(lat, 3);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, 16), S_OK);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, 17), DXGI_ERROR_INVALID_CALL);
    IDXGIDevice1_GetMaximumFrameLatency(d1, &lat);
    EXPECT_EQ(lat, 16);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, 1), S_OK);
    IDXGIDevice1_GetMaximumFrameLatency(d1, &lat);
    EXPECT_EQ(lat, 1);
    EXPECT_HR(IDXGIDevice1_SetMaximumFrameLatency(d1, 3), S_OK);
    SAFE_RELEASE(d1);
    sc_close(&s);
}

TEST(dxgi_swapchain_rotation_bitblt_invalid)
{
    OPEN2(s);
    for (int eff = 0; eff <= 1; eff++) {
        IDXGISwapChain2 *s2 = mk2(&s, eff, 0, 2);
        if (!s2) continue;
        EXPECT_HR(IDXGISwapChain2_SetRotation(s2, DXGI_MODE_ROTATION_IDENTITY), DXGI_ERROR_INVALID_CALL);
        EXPECT_HR(IDXGISwapChain2_SetRotation(s2, DXGI_MODE_ROTATION_ROTATE90), DXGI_ERROR_INVALID_CALL);
        DXGI_MODE_ROTATION r = 0xFF;
        HRESULT hr = IDXGISwapChain2_GetRotation(s2, &r);
        EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL); EXPECT_EQ(r, 0xFF);   /* the out value is not written */
        SC_REL(s2);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_rotation_flip)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0, 2);
    if (!s2) { sc_close(&s); return; }
    DXGI_MODE_ROTATION r = DXGI_MODE_ROTATION_UNSPECIFIED;
    HRESULT hr = IDXGISwapChain2_GetRotation(s2, &r);
    EXPECT_HR(hr, S_OK); EXPECT_EQ(r, DXGI_MODE_ROTATION_IDENTITY);
    hr = IDXGISwapChain2_SetRotation(s2, DXGI_MODE_ROTATION_IDENTITY);
    PIN_HR("SetRotation(IDENTITY) on a windowed flip chain", hr, S_OK);
    if (hr == S_OK) {
        r = 0xFF;
        EXPECT_HR(IDXGISwapChain2_GetRotation(s2, &r), S_OK);
        EXPECT_EQ(r, DXGI_MODE_ROTATION_IDENTITY);
        static const DXGI_MODE_ROTATION rots[] = { DXGI_MODE_ROTATION_ROTATE90, DXGI_MODE_ROTATION_ROTATE180, DXGI_MODE_ROTATION_ROTATE270 };
        for (unsigned i = 0; i < 3; i++) {
            hr = IDXGISwapChain2_SetRotation(s2, rots[i]);
            EXPECT_HR(hr, S_OK);
            r = 0xFF; IDXGISwapChain2_GetRotation(s2, &r); EXPECT_EQ(r, rots[i]);
        }
        IDXGISwapChain2_SetRotation(s2, DXGI_MODE_ROTATION_IDENTITY);
        hr = IDXGISwapChain2_SetRotation(s2, (DXGI_MODE_ROTATION)5);
        PIN_HR("SetRotation(5)", hr, DXGI_ERROR_INVALID_CALL);
    }
    EXPECT_HR(IDXGISwapChain2_GetRotation(s2, NULL), DXGI_ERROR_INVALID_CALL);
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_source_size_invalid)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0, 2);
    if (!s2) { sc_close(&s); return; }
    UINT w = 0, h = 0;
    /* larger than the buffer is rejected and leaves the stored size alone */
    EXPECT_HR(IDXGISwapChain2_SetSourceSize(s2, 65, 48), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_SetSourceSize(s2, 64, 49), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_SetSourceSize(s2, 0xFFFFFFFF, 1), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_SetSourceSize(s2, 1, 0xFFFFFFFF), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_GetSourceSize(s2, &w, &h), S_OK);
    EXPECT_EQ(w, 64); EXPECT_EQ(h, 48);
    /* Win10 stores zero-sized source rectangles without complaint (it is not documented to be an error either) */
    EXPECT_HR(IDXGISwapChain2_SetSourceSize(s2, 0, 0), S_OK);
    EXPECT_HR(IDXGISwapChain2_GetSourceSize(s2, &w, &h), S_OK); EXPECT_EQ(w, 0); EXPECT_EQ(h, 0);
    EXPECT_HR(IDXGISwapChain2_SetSourceSize(s2, 0, 10), S_OK);
    EXPECT_HR(IDXGISwapChain2_GetSourceSize(s2, &w, &h), S_OK); EXPECT_EQ(w, 0); EXPECT_EQ(h, 10);
    EXPECT_HR(IDXGISwapChain2_SetSourceSize(s2, 10, 0), S_OK);
    EXPECT_HR(IDXGISwapChain2_GetSourceSize(s2, &w, &h), S_OK); EXPECT_EQ(w, 10); EXPECT_EQ(h, 0);
    /* a NULL out-pointer anywhere is an error */
    EXPECT_HR(IDXGISwapChain2_GetSourceSize(s2, NULL, NULL), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_GetSourceSize(s2, &w, NULL), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_GetSourceSize(s2, NULL, &h), DXGI_ERROR_INVALID_CALL);
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_source_size_roundtrip)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0, 2);
    if (!s2) { sc_close(&s); return; }
    UINT w = 0, h = 0;
    HRESULT hr = IDXGISwapChain2_GetSourceSize(s2, &w, &h);
    EXPECT_HR(hr, S_OK); EXPECT_EQ(w, 64); EXPECT_EQ(h, 48);
    hr = IDXGISwapChain2_SetSourceSize(s2, 32, 24);
    PIN_HR("SetSourceSize(32,24)", hr, S_OK);
    {
        EXPECT_HR(IDXGISwapChain2_GetSourceSize(s2, &w, &h), S_OK);
        EXPECT_EQ(w, 32); EXPECT_EQ(h, 24);
        EXPECT_HR(IDXGISwapChain2_Present(s2, 0, 0), S_OK);
        EXPECT_HR(IDXGISwapChain2_SetSourceSize(s2, 64, 48), S_OK);
        IDXGISwapChain2_GetSourceSize(s2, &w, &h);
        EXPECT_EQ(w, 64); EXPECT_EQ(h, 48);
    }
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_background_color)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc;
    if (!sc_okish(sc_create1(&s, s.hwnd, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    EXPECT_HR(IDXGISwapChain1_SetBackgroundColor(sc, NULL), DXGI_ERROR_INVALID_CALL);
    DXGI_RGBA bad[] = { { -0.01f, 0, 0, 1 }, { 0, 1.01f, 0, 1 }, { 0, 0, 2.0f, 1 }, { 0, 0, 0, -1.0f }, { 0, 0, 0, 1.5f } };
    for (unsigned i = 0; i < ARRAY_SIZE_(bad); i++)
        EXPECT_HR(IDXGISwapChain1_SetBackgroundColor(sc, &bad[i]), DXGI_ERROR_INVALID_CALL);
    DXGI_RGBA c = { 0.25f, 0.5f, 0.75f, 1.0f }, g = { 9, 9, 9, 9 };
    EXPECT_HR(IDXGISwapChain1_SetBackgroundColor(sc, &c), S_OK);
    EXPECT_HR(IDXGISwapChain1_GetBackgroundColor(sc, &g), S_OK);
    CHECK(g.r == c.r && g.g == c.g && g.b == c.b && g.a == c.a);
    DXGI_RGBA lim0 = { 0, 0, 0, 0 }, lim1 = { 1, 1, 1, 1 };
    EXPECT_HR(IDXGISwapChain1_SetBackgroundColor(sc, &lim0), S_OK);
    EXPECT_HR(IDXGISwapChain1_SetBackgroundColor(sc, &lim1), S_OK);
    IDXGISwapChain1_GetBackgroundColor(sc, &g);
    CHECK(g.r == 1 && g.a == 1);
    /* a rejected value must not clobber the stored one */
    IDXGISwapChain1_SetBackgroundColor(sc, &bad[0]);
    IDXGISwapChain1_GetBackgroundColor(sc, &g);
    CHECK(g.r == 1 && g.g == 1 && g.b == 1 && g.a == 1);
    HRESULT hr = IDXGISwapChain1_GetBackgroundColor(sc, NULL);
    PIN_HR("GetBackgroundColor(NULL)", hr, DXGI_ERROR_INVALID_CALL);
    /* The first Present of a flip chain rejects DO_NOT_SEQUENCE; after a plain Present it is accepted. */
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_DO_NOT_SEQUENCE), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, DXGI_PRESENT_DO_NOT_SEQUENCE), S_OK);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_background_color_default)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    DXGI_RGBA g = { 9, 9, 9, 9 };
    HRESULT hr = IDXGISwapChain1_GetBackgroundColor(sc, &g);
    EXPECT_HR(hr, S_OK);
    CHECKF(g.r == 0 && g.g == 0 && g.b == 0 && g.a == 1, "default background colour (%g %g %g %g)", g.r, g.g, g.b, g.a);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_matrix_transform_hwnd_chain)
{
    OPEN2(s);
    IDXGISwapChain2 *s2 = mk2(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0, 2);
    if (!s2) { sc_close(&s); return; }
    DXGI_MATRIX_3X2_F m = { 2, 0, 0, 2, 0, 0 }, g;
    EXPECT_HR(IDXGISwapChain2_SetMatrixTransform(s2, &m), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_GetMatrixTransform(s2, &g), DXGI_ERROR_INVALID_CALL);
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_matrix_transform_composition)
{
    OPEN2(s);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    d.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    IDXGISwapChain1 *sc = NULL; IDXGISwapChain2 *s2 = NULL;
    HRESULT hr = IDXGIFactory2_CreateSwapChainForComposition(s.fac2, s.unk, &d, NULL, &sc);
    if (FAILED(hr)) { t_skip("composition swap chain unavailable (%s)", d3dh_hr_name(hr)); sc_close(&s); return; }
    if (FAILED(IDXGISwapChain1_QueryInterface(sc, &IID_IDXGISwapChain2, (void **)&s2))) { t_skip("no SwapChain2"); SAFE_RELEASE(sc); sc_close(&s); return; }
    IDXGISwapChain1_Release(sc);
    DXGI_MATRIX_3X2_F m = { 2, 0, 0, 3, 10, 20 }, g = { 0 };
    EXPECT_HR(IDXGISwapChain2_SetMatrixTransform(s2, &m), S_OK);
    EXPECT_HR(IDXGISwapChain2_GetMatrixTransform(s2, &g), S_OK);
    CHECK(g._11 == 2 && g._22 == 3 && g._31 == 10 && g._32 == 20 && g._12 == 0 && g._21 == 0);
    DXGI_MATRIX_3X2_F skew = { 1, 0.5f, 0, 1, 0, 0 };
    EXPECT_HR(IDXGISwapChain2_SetMatrixTransform(s2, &skew), DXGI_ERROR_INVALID_CALL);
    DXGI_MATRIX_3X2_F rot = { 1, 0, 0.5f, 1, 0, 0 };
    EXPECT_HR(IDXGISwapChain2_SetMatrixTransform(s2, &rot), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_SetMatrixTransform(s2, NULL), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGISwapChain2_GetMatrixTransform(s2, NULL), DXGI_ERROR_INVALID_CALL);
    /* only the off-diagonal terms are validated: zero and negative scales are stored */
    DXGI_MATRIX_3X2_F odd[] = { { 0, 0, 0, 0, 0, 0 }, { -1, 0, 0, 1, 0, 0 }, { 1, 0, 0, 1, 0, 0 } };
    for (unsigned i = 0; i < 3; i++) {
        DXGI_MATRIX_3X2_F gg = { 9, 9, 9, 9, 9, 9 };
        EXPECT_HR(IDXGISwapChain2_SetMatrixTransform(s2, &odd[i]), S_OK);
        EXPECT_HR(IDXGISwapChain2_GetMatrixTransform(s2, &gg), S_OK);
        CHECK(!memcmp(&gg, &odd[i], sizeof gg));
    }
    SC_REL(s2);
    sc_close(&s);
}

TEST(dxgi_swapchain_temporary_mono)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    EXPECT_EQ(IDXGISwapChain1_IsTemporaryMonoSupported(sc), FALSE);   /* only meaningful for stereo chains */
    SC_REL(sc);
    sc_close(&s);
}

/* ---- colour space ---------------------------------------------------------- */
TEST(dxgi_swapchain_colorspace_support_table)
{
    OPEN2(s);
    IDXGISwapChain3 *s3 = mk3(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_FORMAT_R8G8B8A8_UNORM, 2);
    if (!s3) { sc_close(&s); return; }
    for (int cs = 0; cs <= 20; cs++) {
        UINT sup = 0xDEADBEEF;
        HRESULT hr = IDXGISwapChain3_CheckColorSpaceSupport(s3, (DXGI_COLOR_SPACE_TYPE)cs, &sup);
        CHECKF(hr == S_OK, "CheckColorSpaceSupport(%d) -> %s", cs, d3dh_hr_name(hr));
        CHECKF((sup & ~DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == 0, "colour space %d: flags 0x%x", cs, sup);
        if (cs == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709) CHECKF(sup & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT, "sRGB colour space unsupported");
        if (cs == DXGI_COLOR_SPACE_RESERVED) CHECKF(sup == 0, "RESERVED colour space supported");
        hr = IDXGISwapChain3_SetColorSpace1(s3, (DXGI_COLOR_SPACE_TYPE)cs);
        HRESULT want = (sup & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) ? S_OK : E_INVALIDARG;
        CHECKF(hr == want, "SetColorSpace1(%d) -> %s want %s (support 0x%x)", cs, d3dh_hr_name(hr), d3dh_hr_name(want), sup);
    }
    SC_REL(s3);
    sc_close(&s);
}

TEST(dxgi_swapchain_colorspace_newer_enums)
{
    OPEN2(s);
    IDXGISwapChain3 *s3 = mk3(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_FORMAT_R8G8B8A8_UNORM, 2);
    if (!s3) { sc_close(&s); return; }
    for (int cs = 21; cs <= 26; cs++) {
        UINT sup = 0xDEADBEEF;
        HRESULT hr = IDXGISwapChain3_CheckColorSpaceSupport(s3, (DXGI_COLOR_SPACE_TYPE)cs, &sup);
        EXPECT_HR(hr, S_OK);   /* 21 is presentable, 22..26 are accepted enum values without support */
        CHECKF(sup == (cs == 21 ? 1u : 0u), "colour space %d support 0x%x", cs, sup);
    }
    SC_REL(s3);
    sc_close(&s);
}

TEST(dxgi_swapchain_colorspace_out_of_range)
{
    OPEN2(s);
    IDXGISwapChain3 *s3 = mk3(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_FORMAT_R8G8B8A8_UNORM, 2);
    if (!s3) { sc_close(&s); return; }
    UINT sup = 0xDEADBEEF;
    HRESULT hr = IDXGISwapChain3_CheckColorSpaceSupport(s3, (DXGI_COLOR_SPACE_TYPE)0xFFFF, &sup);
    EXPECT_HR(hr, S_OK); EXPECT_EQ(sup, 0);   /* an out-of-range value is "supported by nothing", not an error */
    hr = IDXGISwapChain3_SetColorSpace1(s3, (DXGI_COLOR_SPACE_TYPE)0xFFFF);
    PIN_HR("SetColorSpace1(0xFFFF)", hr, E_INVALIDARG);
    hr = IDXGISwapChain3_SetColorSpace1(s3, DXGI_COLOR_SPACE_CUSTOM);
    PIN_HR("SetColorSpace1(CUSTOM)", hr, E_INVALIDARG);
    hr = IDXGISwapChain3_CheckColorSpaceSupport(s3, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, NULL);
    PIN_HR("CheckColorSpaceSupport(NULL out)", hr, E_INVALIDARG);
    /* sRGB full range must always be settable */
    EXPECT_HR(IDXGISwapChain3_SetColorSpace1(s3, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709), S_OK);
    SC_REL(s3);
    sc_close(&s);
}

TEST(dxgi_swapchain_colorspace_by_format)
{
    OPEN2(s);
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT };
    static const DXGI_COLOR_SPACE_TYPE cs[] = { DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,
        DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709 };
    for (unsigned i = 0; i < 4; i++) {
        IDXGISwapChain3 *s3 = mk3(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, fm[i], 2);
        if (!s3) continue;
        for (unsigned j = 0; j < 4; j++) {
            UINT sup = 0;
            HRESULT hr = IDXGISwapChain3_CheckColorSpaceSupport(s3, cs[j], &sup);
            EXPECT_HR(hr, S_OK);
            /* Measured on Win10 22H2 (WARP, flip chain): 8-bit and 10-bit formats present sRGB, studio sRGB and HDR10; scRGB (linear) only on FP16. */
            UINT want = (fm[i] == DXGI_FORMAT_R16G16B16A16_FLOAT) ? (cs[j] == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709) : (cs[j] != DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
            CHECKF(sup == want, "format %d colour space %d: support 0x%x want 0x%x", fm[i], cs[j], sup, want);
        }
        SC_REL(s3);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_colorspace_bitblt)
{
    OPEN2(s);
    IDXGISwapChain3 *s3 = mk3(&s, DXGI_SWAP_EFFECT_DISCARD, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    if (!s3) { sc_close(&s); return; }
    UINT sup = 0xDEADBEEF;
    HRESULT hr = IDXGISwapChain3_CheckColorSpaceSupport(s3, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &sup);
    EXPECT_HR(hr, S_OK); EXPECT_EQ(sup, DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
    hr = IDXGISwapChain3_SetColorSpace1(s3, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    PIN_HR("bitblt SetColorSpace1(sRGB)", hr, S_OK);
    SC_REL(s3);
    sc_close(&s);
}

TEST(dxgi_swapchain_hdr_metadata)
{
    OPEN2(s);
    IDXGISwapChain3 *s3 = mk3(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_FORMAT_R10G10B10A2_UNORM, 2);
    if (!s3) { sc_close(&s); return; }
    IDXGISwapChain4 *s4 = NULL;
    if (FAILED(IDXGISwapChain3_QueryInterface(s3, &IID_IDXGISwapChain4, (void **)&s4))) { t_skip("IDXGISwapChain4 unavailable"); SC_REL(s3); sc_close(&s); return; }
    IDXGISwapChain3_Release(s3);
    HRESULT hr = IDXGISwapChain4_SetHDRMetaData(s4, DXGI_HDR_METADATA_TYPE_NONE, 0, NULL);
    EXPECT_HR(hr, S_OK);
    hr = IDXGISwapChain4_SetHDRMetaData(s4, DXGI_HDR_METADATA_TYPE_NONE, 4, "abcd");
    PIN_HR("SetHDRMetaData(NONE, size 4)", hr, E_INVALIDARG);
    hr = IDXGISwapChain4_SetHDRMetaData(s4, DXGI_HDR_METADATA_TYPE_HDR10, 0, NULL);
    PIN_HR("SetHDRMetaData(HDR10, size 0)", hr, E_INVALIDARG);
    DXGI_HDR_METADATA_HDR10 md; memset(&md, 0, sizeof md);
    md.MaxMasteringLuminance = 1000; md.MinMasteringLuminance = 1; md.MaxContentLightLevel = 1000; md.MaxFrameAverageLightLevel = 400;
    hr = IDXGISwapChain4_SetHDRMetaData(s4, DXGI_HDR_METADATA_TYPE_HDR10, sizeof md, &md);
    PIN_HR("SetHDRMetaData(HDR10) without an HDR colour space", hr, S_OK);
    hr = IDXGISwapChain4_SetHDRMetaData(s4, DXGI_HDR_METADATA_TYPE_HDR10, sizeof md - 1, &md);
    CHECK(FAILED(hr));
    hr = IDXGISwapChain4_SetHDRMetaData(s4, (DXGI_HDR_METADATA_TYPE)9, sizeof md, &md);
    PIN_HR("SetHDRMetaData(type 9)", hr, E_INVALIDARG);
    SC_REL(s4);
    sc_close(&s);
}

/* ---- D3D12 flavours ---------------------------------------------------------- */
static IDXGISwapChain3 *mk12(SC12 *s, UINT count, UINT flags, DXGI_SWAP_EFFECT eff)
{
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(64, 48, DXGI_FORMAT_R8G8B8A8_UNORM, count, eff);
    d.Flags = flags;
    IDXGISwapChain1 *sc = NULL; IDXGISwapChain3 *s3 = NULL;
    HRESULT hr = sc12_create(s, s->hwnd, &d, &sc);
    if (!sc_okish(hr)) { CHECKF(0, "create: %s", d3dh_hr_name(hr)); return NULL; }
    hr = IDXGISwapChain1_QueryInterface(sc, &IID_IDXGISwapChain3, (void **)&s3);
    IDXGISwapChain1_Release(sc);
    if (FAILED(hr)) { t_skip("no IDXGISwapChain3"); return NULL; }
    return s3;
}

TEST(dxgi_swapchain_d12_backbuffer_index_cycles)
{
    SC12 s; if (!sc12_open(&s)) return;
    static const DXGI_SWAP_EFFECT effs[] = { DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD };
    for (unsigned e = 0; e < 2; e++) for (UINT count = 2; count <= 4; count++) {
        IDXGISwapChain3 *s3 = mk12(&s, count, 0, effs[e]);
        if (!s3) continue;
        for (UINT j = 0; j < 2 * count + 2; j++) {
            UINT idx = IDXGISwapChain3_GetCurrentBackBufferIndex(s3);
            CHECKF(idx == j % count, "effect %d count %u present %u: index %u", effs[e], count, j, idx);
            EXPECT_HR(IDXGISwapChain3_Present(s3, 0, 0), S_OK);
        }
        /* the index restarts at zero after ResizeBuffers */
        EXPECT_HR(IDXGISwapChain3_ResizeBuffers(s3, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0), S_OK);
        CHECK(IDXGISwapChain3_GetCurrentBackBufferIndex(s3) == 0);
        d3dh_d12_flush(s.dev, s.q);
        SC_REL(s3);
    }
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_latency_object)
{
    SC12 s; if (!sc12_open(&s)) return;
    IDXGISwapChain3 *s3 = mk12(&s, 2, 0, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    if (!s3) { sc12_close(&s); return; }
    IDXGISwapChain2 *s2 = NULL;
    IDXGISwapChain3_QueryInterface(s3, &IID_IDXGISwapChain2, (void **)&s2);
    if (s2) {
        UINT lat = 0xDEADBEEF;
        EXPECT_HR(IDXGISwapChain2_GetMaximumFrameLatency(s2, &lat), DXGI_ERROR_INVALID_CALL);
        EXPECT_EQ(lat, 0xDEADBEEF);
        EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, 1), DXGI_ERROR_INVALID_CALL);
        CHECK(IDXGISwapChain2_GetFrameLatencyWaitableObject(s2) == NULL);
        SAFE_RELEASE(s2);
    }
    SC_REL(s3);
    s3 = mk12(&s, 2, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    if (s3 && SUCCEEDED(IDXGISwapChain3_QueryInterface(s3, &IID_IDXGISwapChain2, (void **)&s2))) {
        UINT lat = 0;
        EXPECT_HR(IDXGISwapChain2_GetMaximumFrameLatency(s2, &lat), S_OK);
        EXPECT_EQ(lat, 1);
        EXPECT_HR(IDXGISwapChain2_SetMaximumFrameLatency(s2, 0), DXGI_ERROR_INVALID_CALL);
        HANDLE h = IDXGISwapChain2_GetFrameLatencyWaitableObject(s2);
        CHECK(h != NULL);
        if (h) { EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0); CloseHandle(h); }
        SAFE_RELEASE(s2);
    }
    if (s3) { d3dh_d12_flush(s.dev, s.q); SC_REL(s3); }
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_present_count)
{
    SC12 s; if (!sc12_open(&s)) return;
    IDXGISwapChain3 *s3 = mk12(&s, 3, 0, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    if (!s3) { sc12_close(&s); return; }
    UINT c = ~0u;
    EXPECT_HR(IDXGISwapChain3_GetLastPresentCount(s3, &c), S_OK);
    EXPECT_EQ(c, 0);
    EXPECT_HR(IDXGISwapChain3_Present(s3, 0, 0), S_OK);
    IDXGISwapChain3_GetLastPresentCount(s3, &c);
    EXPECT_EQ(c, 1);
    EXPECT_HR(IDXGISwapChain3_Present(s3, 10, 0), DXGI_ERROR_INVALID_CALL);
    IDXGISwapChain3_GetLastPresentCount(s3, &c);
    EXPECT_EQ(c, 1);
    EXPECT_HR(IDXGISwapChain3_Present(s3, 0, DXGI_PRESENT_TEST), S_OK);
    IDXGISwapChain3_GetLastPresentCount(s3, &c);
    EXPECT_EQ(c, 1);
    d3dh_d12_flush(s.dev, s.q);
    SC_REL(s3);
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_colorspace)
{
    SC12 s; if (!sc12_open(&s)) return;
    IDXGISwapChain3 *s3 = mk12(&s, 2, 0, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    if (!s3) { sc12_close(&s); return; }
    UINT sup = 0;
    EXPECT_HR(IDXGISwapChain3_CheckColorSpaceSupport(s3, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &sup), S_OK);
    CHECK(sup & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
    EXPECT_HR(IDXGISwapChain3_SetColorSpace1(s3, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709), S_OK);
    EXPECT_HR(IDXGISwapChain3_CheckColorSpaceSupport(s3, DXGI_COLOR_SPACE_RESERVED, &sup), S_OK);
    EXPECT_EQ(sup, 0);
    EXPECT_HR(IDXGISwapChain3_SetColorSpace1(s3, DXGI_COLOR_SPACE_RESERVED), E_INVALIDARG);
    d3dh_d12_flush(s.dev, s.q);
    SC_REL(s3);
    sc12_close(&s);
}
