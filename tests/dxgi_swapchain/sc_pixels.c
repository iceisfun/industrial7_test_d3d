/* Back buffer contents: clears, copies, updates, MSAA resolve and views, read back through a staging copy. */
#include "sc.h"

#define OPEN2(s) SC s; if (!sc_open2(&s, 0)) return

static const int k_effs[] = { DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD };

static uint64_t px64(const d3dh_image *im, UINT x, UINT y)
{
    UINT bpp = d3dh_format_bytes(im->format);
    uint64_t v = 0;
    memcpy(&v, im->data + (size_t)y * im->pitch + (size_t)x * bpp, bpp > 8 ? 8 : bpp);
    return v;
}

static int near_u8(unsigned a, unsigned b) { return a + 1 >= b && a <= b + 1; }

static const struct { DXGI_FORMAT f; float c[4]; uint64_t want; const char *n; } k_clear[] = {
    { DXGI_FORMAT_B8G8R8A8_UNORM,     { 1, 0, 0, 1 },        0xFFFF0000ull, "B8G8R8A8 red" },
    { DXGI_FORMAT_B8G8R8A8_UNORM,     { 0, 1, 0, 1 },        0xFF00FF00ull, "B8G8R8A8 green" },
    { DXGI_FORMAT_B8G8R8A8_UNORM,     { 0, 0, 1, 1 },        0xFF0000FFull, "B8G8R8A8 blue" },
    { DXGI_FORMAT_B8G8R8A8_UNORM,     { 0, 0, 0, 0 },        0x00000000ull, "B8G8R8A8 transparent black" },
    { DXGI_FORMAT_B8G8R8A8_UNORM,     { 0.5f, 0.25f, 0.75f, 1 }, 0xFF8040BFull, "B8G8R8A8 mixed" },
    { DXGI_FORMAT_R8G8B8A8_UNORM,     { 1, 0, 0, 1 },        0xFF0000FFull, "R8G8B8A8 red" },
    { DXGI_FORMAT_R8G8B8A8_UNORM,     { 0, 1, 0, 1 },        0xFF00FF00ull, "R8G8B8A8 green" },
    { DXGI_FORMAT_R8G8B8A8_UNORM,     { 0, 0, 1, 1 },        0xFFFF0000ull, "R8G8B8A8 blue" },
    { DXGI_FORMAT_R8G8B8A8_UNORM,     { 0.5f, 0.25f, 0.75f, 1 }, 0xFFBF4080ull, "R8G8B8A8 mixed" },
    { DXGI_FORMAT_R10G10B10A2_UNORM,  { 1, 0, 0, 1 },        0xC00003FFull, "R10G10B10A2 red" },
    { DXGI_FORMAT_R10G10B10A2_UNORM,  { 0, 1, 0, 1 },        0xC00FFC00ull, "R10G10B10A2 green" },
    { DXGI_FORMAT_R10G10B10A2_UNORM,  { 0, 0, 1, 1 },        0xFFF00000ull, "R10G10B10A2 blue" },
    { DXGI_FORMAT_R10G10B10A2_UNORM,  { 0, 0, 0, 0 },        0x00000000ull, "R10G10B10A2 clear" },
    { DXGI_FORMAT_R16G16B16A16_FLOAT, { 1, 0.5f, 0.25f, 1 }, 0x3C00340038003C00ull, "R16G16B16A16F mixed" },
    { DXGI_FORMAT_R16G16B16A16_FLOAT, { 0, 0, 0, 0 },        0x0000000000000000ull, "R16G16B16A16F zero" },
    { DXGI_FORMAT_R16G16B16A16_FLOAT, { 2, -1, 0, 1 },       0x3C000000BC004000ull, "R16G16B16A16F out of [0,1]" },
};

TEST(dxgi_swapchain_pixels_clear_all_formats_all_effects)
{
    OPEN2(s);
    for (unsigned e = 0; e < 4; e++) for (unsigned i = 0; i < ARRAY_SIZE_(k_clear); i++) {
        HWND w = sc_window(16, 8);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(16, 8, k_clear[i].f, 2, k_effs[e]);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, w, &d, &sc);
        if (!sc_okish(hr)) { CHECKF(0, "%s effect %d create: %s", k_clear[i].n, k_effs[e], d3dh_hr_name(hr)); DestroyWindow(w); continue; }
        hr = sc_clear(&s, (IDXGISwapChain *)sc, 0, k_clear[i].c, DXGI_FORMAT_UNKNOWN);
        CHECKF(hr == S_OK, "%s effect %d clear: %s", k_clear[i].n, k_effs[e], d3dh_hr_name(hr));
        d3dh_image im;
        if (SUCCEEDED(sc_readback(&s, (IDXGISwapChain *)sc, 0, &im))) {
            CHECK(im.width == 16 && im.height == 8);
            for (UINT y = 0; y < 8; y += 7) for (UINT x = 0; x < 16; x += 15) {
                uint64_t v = px64(&im, x, y);
                CHECKF(v == k_clear[i].want, "%s effect %d (%u,%u): got 0x%llx want 0x%llx", k_clear[i].n, k_effs[e], x, y,
                       (unsigned long long)v, (unsigned long long)k_clear[i].want);
            }
            d3dh_image_free(&im);
        } else CHECKF(0, "%s effect %d readback failed", k_clear[i].n, k_effs[e]);
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_pixels_srgb_bitblt_clear)
{
    OPEN2(s);
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB };
    for (unsigned i = 0; i < 2; i++) for (int eff = 0; eff <= 1; eff++) {
        HWND w = sc_window(8, 8);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(8, 8, fm[i], 2, eff);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, w, &d, &sc);
        if (!sc_okish(hr)) { CHECKF(0, "create %s", d3dh_hr_name(hr)); DestroyWindow(w); continue; }
        const float lin[4] = { 0.5f, 0.0f, 1.0f, 1.0f };
        EXPECT_HR(sc_clear(&s, (IDXGISwapChain *)sc, 0, lin, DXGI_FORMAT_UNKNOWN), S_OK);
        d3dh_image im;
        if (SUCCEEDED(sc_readback(&s, (IDXGISwapChain *)sc, 0, &im))) {
            uint32_t v = d3dh_image_pixel32(&im, 3, 3);
            unsigned c0, c1, c2, c3;
            if (fm[i] == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) { c0 = v & 255; c1 = (v >> 8) & 255; c2 = (v >> 16) & 255; }
            else { c2 = v & 255; c1 = (v >> 8) & 255; c0 = (v >> 16) & 255; }
            c3 = v >> 24;
            CHECKF(near_u8(c0, 188), "format %d effect %d: encoded 0.5 -> %u (want 188)", fm[i], eff, c0);
            CHECKF(c1 == 0, "zero channel -> %u", c1);
            CHECKF(c2 == 255, "one channel -> %u", c2);
            CHECKF(c3 == 255, "alpha -> %u", c3);
            d3dh_image_free(&im);
        } else CHECK(0);
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_pixels_flip_srgb_view)
{
    OPEN2(s);
    /* Flip-model chains cannot be created with an _SRGB format, but an _SRGB render target view may be created on them. */
    static const DXGI_FORMAT fm[][2] = { { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
                                         { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB } };
    for (unsigned i = 0; i < 2; i++) for (int eff = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; eff <= DXGI_SWAP_EFFECT_FLIP_DISCARD; eff++) {
        HWND w = sc_window(8, 8);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(8, 8, fm[i][0], 2, eff);
        IDXGISwapChain1 *sc = NULL;
        HRESULT hr = sc_create1(&s, w, &d, &sc);
        if (!sc_okish(hr)) { CHECKF(0, "create %s", d3dh_hr_name(hr)); DestroyWindow(w); continue; }
        const float gray[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
        hr = sc_clear(&s, (IDXGISwapChain *)sc, 0, gray, fm[i][1]);
        CHECKF(hr == S_OK, "SRGB view on a flip chain of format %d: %s", fm[i][0], d3dh_hr_name(hr));
        d3dh_image im;
        if (hr == S_OK && SUCCEEDED(sc_readback(&s, (IDXGISwapChain *)sc, 0, &im))) {
            uint32_t v = d3dh_image_pixel32(&im, 1, 1);
            CHECKF(near_u8(v & 255, 188) && near_u8((v >> 8) & 255, 188) && near_u8((v >> 16) & 255, 188), "gray through sRGB view -> 0x%08x", v);
            d3dh_image_free(&im);
        }
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}

/* What GetBuffer(0) reads after Present, measured on Win10 22H2 (identical for hidden and shown windows):
 * DISCARD keeps the presented pixels for any buffer count. SEQUENTIAL with a single buffer keeps them; with two or more buffers Windows rotates the
 * buffers even in the bit-blt model, so buffer 0 holds an untouched (zero) buffer after the first Present, the presented one again after the
 * second (2 buffers) or only after the third (3 buffers). */
/* Contents of a buffer that was never drawn to are allocator-defined: pinned (zero) on WARP, logged elsewhere. */
#define SC_CONTENT_CHECK(s, got, want, ...) do { if ((want) == 0 && (s)->drv != D3D_DRIVER_TYPE_WARP) t_note("untouched buffer reads 0x%08x", (unsigned)(got)); else CHECKF((got) == (want), __VA_ARGS__); } while (0)
static void seq_content(SC *s, DXGI_SWAP_EFFECT eff, UINT count, uint32_t want1, uint32_t want2)
{
    HWND w = sc_window(8, 8);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, count, eff);
    IDXGISwapChain1 *sc = NULL;
    if (!sc_okish(sc_create1(s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); return; }
    const float c[4] = { 0, 1, 0, 1 };
    sc_clear(s, (IDXGISwapChain *)sc, 0, c, DXGI_FORMAT_UNKNOWN);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    d3dh_image im;
    if (SUCCEEDED(sc_readback(s, (IDXGISwapChain *)sc, 0, &im))) {
        SC_CONTENT_CHECK(s, d3dh_image_pixel32(&im, 4, 4), want1, "eff %d count %u after Present: 0x%08x want 0x%08x", eff, count, d3dh_image_pixel32(&im, 4, 4), want1);
        d3dh_image_free(&im);
    } else CHECK(0);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    if (SUCCEEDED(sc_readback(s, (IDXGISwapChain *)sc, 0, &im))) {
        SC_CONTENT_CHECK(s, d3dh_image_pixel32(&im, 0, 0), want2, "eff %d count %u after second Present: 0x%08x want 0x%08x", eff, count, d3dh_image_pixel32(&im, 0, 0), want2);
        d3dh_image_free(&im);
    }
    SC_REL(sc);
    DestroyWindow(w);
}
TEST(dxgi_swapchain_pixels_sequential_content_by_buffer_count)
{
    OPEN2(s);
    seq_content(&s, DXGI_SWAP_EFFECT_SEQUENTIAL, 1, 0xFF00FF00u, 0xFF00FF00u);
    seq_content(&s, DXGI_SWAP_EFFECT_SEQUENTIAL, 2, 0x00000000u, 0xFF00FF00u);
    seq_content(&s, DXGI_SWAP_EFFECT_SEQUENTIAL, 3, 0x00000000u, 0x00000000u);
    sc_close(&s);
}
TEST(dxgi_swapchain_pixels_discard_content_by_buffer_count)
{
    OPEN2(s);
    for (UINT n = 1; n <= 3; n++) seq_content(&s, DXGI_SWAP_EFFECT_DISCARD, n, 0xFF00FF00u, 0xFF00FF00u);
    sc_close(&s);
}
TEST(dxgi_swapchain_pixels_flip_content_by_buffer_count)
{
    OPEN2(s);
    seq_content(&s, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2, 0x00000000u, 0xFF00FF00u);
    seq_content(&s, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 3, 0x00000000u, 0x00000000u);
    seq_content(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, 0x00000000u, 0xFF00FF00u);
    seq_content(&s, DXGI_SWAP_EFFECT_FLIP_DISCARD, 3, 0x00000000u, 0x00000000u);
    sc_close(&s);
}

TEST(dxgi_swapchain_pixels_flip_buffer_rotation)
{
    OPEN2(s);
    HWND w = sc_window(8, 8);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 3, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
    IDXGISwapChain1 *sc = NULL;
    if (!sc_okish(sc_create1(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); sc_close(&s); return; }
    const float c[4] = { 1, 0, 0, 1 };
    sc_clear(&s, (IDXGISwapChain *)sc, 0, c, DXGI_FORMAT_UNKNOWN);
    IDXGISwapChain1_Present(sc, 0, 0);
    /* D3D11 flip chains present GetBuffer(0) and shift: the presented image is then found in the last buffer */
    static const uint32_t want[3] = { 0x00000000u, 0x00000000u, 0xFF0000FFu };
    for (UINT j = 0; j < 3; j++) {
        d3dh_image im;
        if (SUCCEEDED(sc_readback(&s, (IDXGISwapChain *)sc, j, &im))) {
            SC_CONTENT_CHECK(&s, d3dh_image_pixel32(&im, 0, 0), want[j], "flip sequential, buffer %u after one Present reads 0x%08x want 0x%08x", j, d3dh_image_pixel32(&im, 0, 0), want[j]);
            d3dh_image_free(&im);
        } else CHECK(0);
    }
    SC_REL(sc);
    DestroyWindow(w);
    sc_close(&s);
}

TEST(dxgi_swapchain_pixels_update_subresource_pattern)
{
    OPEN2(s);
    HWND w = sc_window(8, 8);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain1 *sc = NULL;
    if (!sc_okish(sc_create1(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); sc_close(&s); return; }
    const float c[4] = { 0, 0, 0, 1 };
    sc_clear(&s, (IDXGISwapChain *)sc, 0, c, DXGI_FORMAT_UNKNOWN);
    uint32_t patch[4 * 3];
    for (int i = 0; i < 12; i++) patch[i] = 0xFF000000u | (uint32_t)(i * 20) | ((uint32_t)(i * 10) << 8) | ((uint32_t)(255 - i) << 16);
    D3D11_BOX box = { 2, 3, 0, 6, 6, 1 };
    ID3D11Texture2D *t = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&t);
    ID3D11DeviceContext_UpdateSubresource(s.ctx, (ID3D11Resource *)t, 0, &box, patch, 16, 0);
    SAFE_RELEASE(t);
    d3dh_image im;
    if (SUCCEEDED(sc_readback(&s, (IDXGISwapChain *)sc, 0, &im))) {
        for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
            uint32_t want = 0xFF000000u;
            if (x >= 2 && x < 6 && y >= 3 && y < 6) want = patch[(y - 3) * 4 + (x - 2)];
            uint32_t got = d3dh_image_pixel32(&im, x, y);
            CHECKF(got == want, "(%u,%u): 0x%08x want 0x%08x", x, y, got, want);
        }
        d3dh_image_free(&im);
    } else CHECK(0);
    SC_REL(sc);
    DestroyWindow(w);
    sc_close(&s);
}

TEST(dxgi_swapchain_pixels_copy_from_texture)
{
    OPEN2(s);
    HWND w = sc_window(16, 16);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(16, 16, DXGI_FORMAT_B8G8R8A8_UNORM, 2, DXGI_SWAP_EFFECT_SEQUENTIAL);
    IDXGISwapChain1 *sc = NULL;
    if (!sc_okish(sc_create1(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); sc_close(&s); return; }
    uint32_t src[16 * 16];
    for (int i = 0; i < 256; i++) src[i] = 0xFF000000u | (uint32_t)(i * 0x010203u & 0xFFFFFF);
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = 16; td.Height = 16; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA sd = { src, 64, 0 };
    ID3D11Texture2D *tx = NULL, *bb = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(s.dev, &td, &sd, &tx);
    EXPECT_HR(hr, S_OK);
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    if (tx && bb) {
        ID3D11DeviceContext_CopyResource(s.ctx, (ID3D11Resource *)bb, (ID3D11Resource *)tx);
        d3dh_image im;
        if (SUCCEEDED(sc_readback(&s, (IDXGISwapChain *)sc, 0, &im))) {
            int bad = 0;
            for (UINT y = 0; y < 16; y++) for (UINT x = 0; x < 16; x++) if (d3dh_image_pixel32(&im, x, y) != src[y * 16 + x]) bad++;
            CHECKF(bad == 0, "%d pixels differ after CopyResource into the back buffer", bad);
            d3dh_image_free(&im);
        } else CHECK(0);
        /* a 2-buffer SEQUENTIAL chain rotates on Present: buffer 0 is untouched after the first one, and holds the copy again after the second */
        EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
        if (SUCCEEDED(sc_readback(&s, (IDXGISwapChain *)sc, 0, &im))) {
            CHECKF(d3dh_image_pixel32(&im, 5, 7) == 0, "after one Present: 0x%08x", d3dh_image_pixel32(&im, 5, 7));
            d3dh_image_free(&im);
        } else CHECK(0);
        EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
        if (SUCCEEDED(sc_readback(&s, (IDXGISwapChain *)sc, 0, &im))) {
            CHECK(d3dh_image_pixel32(&im, 5, 7) == src[7 * 16 + 5]);
            d3dh_image_free(&im);
        } else CHECK(0);
    }
    SAFE_RELEASE(tx); SAFE_RELEASE(bb);
    SC_REL(sc);
    DestroyWindow(w);
    sc_close(&s);
}

TEST(dxgi_swapchain_pixels_copy_back_buffer_to_texture)
{
    OPEN2(s);
    HWND w = sc_window(12, 6);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(12, 6, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc = NULL;
    if (!sc_okish(sc_create1(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); sc_close(&s); return; }
    const float c[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    sc_clear(&s, (IDXGISwapChain *)sc, 0, c, DXGI_FORMAT_UNKNOWN);
    ID3D11Texture2D *bb = NULL, *cp = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(bb, &td);
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    EXPECT_HR(ID3D11Device_CreateTexture2D(s.dev, &td, NULL, &cp), S_OK);
    if (cp) {
        ID3D11DeviceContext_CopyResource(s.ctx, (ID3D11Resource *)cp, (ID3D11Resource *)bb);
        d3dh_image im;
        if (SUCCEEDED(d3dh_readback_texture2d11(s.dev, s.ctx, cp, 0, &im))) {
            EXPECT_EQ(im.width, 12); EXPECT_EQ(im.height, 6);
            EXPECT_EQ(d3dh_image_pixel32(&im, 11, 5), 0xFFFF0000u);
            d3dh_image_free(&im);
        } else CHECK(0);
    }
    SAFE_RELEASE(cp); SAFE_RELEASE(bb);
    SC_REL(sc);
    DestroyWindow(w);
    sc_close(&s);
}

TEST(dxgi_swapchain_pixels_msaa_resolve)
{
    OPEN2(s);
    UINT q = 0;
    ID3D11Device_CheckMultisampleQualityLevels(s.dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4, &q);
    if (!q) { t_skip("no 4x MSAA"); sc_close(&s); return; }
    HWND w = sc_window(8, 8);
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_DISCARD);
    d.SampleDesc.Count = 4;
    IDXGISwapChain1 *sc = NULL;
    HRESULT hr = sc_create1(&s, w, &d, &sc);
    if (!sc_okish(hr)) { CHECKF(0, "create: %s", d3dh_hr_name(hr)); DestroyWindow(w); sc_close(&s); return; }
    ID3D11Texture2D *bb = NULL, *dst = NULL; ID3D11RenderTargetView *rtv = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    D3D11_TEXTURE2D_DESC td; ID3D11Texture2D_GetDesc(bb, &td);
    EXPECT_EQ(td.SampleDesc.Count, 4);
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(s.dev, (ID3D11Resource *)bb, NULL, &rtv), S_OK);
    if (rtv) {
        D3D11_RENDER_TARGET_VIEW_DESC rd; memset(&rd, 0, sizeof rd);
        ID3D11RenderTargetView_GetDesc(rtv, &rd);
        EXPECT_EQ(rd.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2DMS);
        EXPECT_EQ(rd.Format, DXGI_FORMAT_R8G8B8A8_UNORM);
        const float c[4] = { 1, 0, 0, 1 };
        ID3D11DeviceContext_ClearRenderTargetView(s.ctx, rtv, c);
        td.SampleDesc.Count = 1; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        EXPECT_HR(ID3D11Device_CreateTexture2D(s.dev, &td, NULL, &dst), S_OK);
        if (dst) {
            ID3D11DeviceContext_ResolveSubresource(s.ctx, (ID3D11Resource *)dst, 0, (ID3D11Resource *)bb, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
            d3dh_image im;
            if (SUCCEEDED(d3dh_readback_texture2d11(s.dev, s.ctx, dst, 0, &im))) {
                EXPECT_EQ(d3dh_image_pixel32(&im, 4, 4), 0xFF0000FFu);
                d3dh_image_free(&im);
            } else CHECK(0);
        }
    }
    SAFE_RELEASE(dst); SAFE_RELEASE(rtv); SAFE_RELEASE(bb);
    SC_REL(sc);
    DestroyWindow(w);
    sc_close(&s);
}

TEST(dxgi_swapchain_view_rtv_default_desc)
{
    OPEN2(s);
    static const DXGI_FORMAT fm[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT };
    for (unsigned i = 0; i < 4; i++) {
        IDXGISwapChain1 *sc = NULL;
        HWND w = sc_window(8, 8);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(8, 8, fm[i], 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
        if (!sc_okish(sc_create1(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); continue; }
        ID3D11Texture2D *bb = NULL; ID3D11RenderTargetView *rtv = NULL;
        IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
        EXPECT_HR(ID3D11Device_CreateRenderTargetView(s.dev, (ID3D11Resource *)bb, NULL, &rtv), S_OK);
        if (rtv) {
            D3D11_RENDER_TARGET_VIEW_DESC rd; memset(&rd, 0, sizeof rd);
            ID3D11RenderTargetView_GetDesc(rtv, &rd);
            CHECKF(rd.Format == fm[i], "format %d: default RTV format %d", fm[i], rd.Format);
            EXPECT_EQ(rd.ViewDimension, D3D11_RTV_DIMENSION_TEXTURE2D);
            EXPECT_EQ(rd.Texture2D.MipSlice, 0);
            SAFE_RELEASE(rtv);
        }
        SAFE_RELEASE(bb);
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_view_srv_requires_shader_input)
{
    OPEN2(s);
    for (int withsrv = 0; withsrv <= 1; withsrv++) {
        HWND w = sc_window(8, 8);
        DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_DISCARD);
        if (withsrv) d.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
        IDXGISwapChain1 *sc = NULL;
        if (!sc_okish(sc_create1(&s, w, &d, &sc))) { CHECK(0); DestroyWindow(w); continue; }
        ID3D11Texture2D *bb = NULL; ID3D11ShaderResourceView *srv = NULL;
        IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
        HRESULT hr = ID3D11Device_CreateShaderResourceView(s.dev, (ID3D11Resource *)bb, NULL, &srv);
        if (withsrv) CHECKF(hr == S_OK, "SRV with SHADER_INPUT usage -> %s", d3dh_hr_name(hr));
        else CHECKF(hr == E_INVALIDARG, "SRV without SHADER_INPUT usage -> %s", d3dh_hr_name(hr));
        SAFE_RELEASE(srv); SAFE_RELEASE(bb);
        SC_REL(sc);
        DestroyWindow(w);
    }
    sc_close(&s);
}

TEST(dxgi_swapchain_view_uav_not_allowed_by_default)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    ID3D11Texture2D *bb = NULL; ID3D11UnorderedAccessView *uav = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    HRESULT hr = ID3D11Device_CreateUnorderedAccessView(s.dev, (ID3D11Resource *)bb, NULL, &uav);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(uav); SAFE_RELEASE(bb);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_view_dsv_not_allowed)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    ID3D11Texture2D *bb = NULL; ID3D11DepthStencilView *dsv = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    HRESULT hr = ID3D11Device_CreateDepthStencilView(s.dev, (ID3D11Resource *)bb, NULL, &dsv);
    CHECK(FAILED(hr));
    SAFE_RELEASE(dsv); SAFE_RELEASE(bb);
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_surface_getdc_without_gdi_flag)
{
    SC s;
    if (!sc_open(&s, D3D11_CREATE_DEVICE_BGRA_SUPPORT)) return;
    DXGI_SWAP_CHAIN_DESC d = sc_desc0(s.hwnd, 32, 32, DXGI_FORMAT_B8G8R8A8_UNORM, 1, DXGI_SWAP_EFFECT_DISCARD);
    IDXGISwapChain *sc = NULL;
    if (!sc_okish(sc_create0(&s, &d, &sc))) { CHECK(0); sc_close(&s); return; }
    IDXGISurface1 *su = NULL;
    if (SUCCEEDED(IDXGISwapChain_GetBuffer(sc, 0, &IID_IDXGISurface1, (void **)&su))) {
        HDC dc = NULL;
        HRESULT hr = IDXGISurface1_GetDC(su, FALSE, &dc);
        CHECKF(FAILED(hr), "GetDC on a chain without GDI_COMPATIBLE succeeded");
        PIN_HR("GetDC without GDI_COMPATIBLE", hr, DXGI_ERROR_INVALID_CALL);
        if (SUCCEEDED(hr)) IDXGISurface1_ReleaseDC(su, NULL);
        SAFE_RELEASE(su);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_surface_map_not_cpu_accessible)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    IDXGISurface *su = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_IDXGISurface, (void **)&su);
    if (su) {
        DXGI_MAPPED_RECT mr; memset(&mr, 0, sizeof mr);
        HRESULT hr = IDXGISurface_Map(su, &mr, DXGI_MAP_READ);
        CHECKF(FAILED(hr), "mapping the back buffer succeeded");
        PIN_HR("IDXGISurface::Map(READ) on a back buffer", hr, E_INVALIDARG);
        if (SUCCEEDED(hr)) IDXGISurface_Unmap(su);
        SAFE_RELEASE(su);
    }
    SC_REL(sc);
    sc_close(&s);
}

TEST(dxgi_swapchain_resource_eviction_priority_and_sharing)
{
    OPEN2(s);
    IDXGISwapChain1 *sc = sc_mk(&s, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD, 0);
    if (!sc) { sc_close(&s); return; }
    IDXGIResource *r = NULL;
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_IDXGIResource, (void **)&r);
    if (r) {
        UINT pri = 0;
        HRESULT hr = IDXGIResource_GetEvictionPriority(r, &pri);
        EXPECT_HR(hr, S_OK); EXPECT_EQ(pri, DXGI_RESOURCE_PRIORITY_NORMAL);
        HANDLE h = NULL;
        hr = IDXGIResource_GetSharedHandle(r, &h);
        EXPECT_HR(hr, E_INVALIDARG); CHECK(h == NULL);
        IDXGIDevice *dd = NULL;
        EXPECT_HR(IDXGIResource_GetDevice(r, &IID_IDXGIDevice, (void **)&dd), S_OK);
        SAFE_RELEASE(dd);
        void *par = NULL;
        hr = IDXGIResource_GetParent(r, &IID_IDXGIDevice, &par);
        PIN_HR("IDXGIResource::GetParent(IDXGIDevice)", hr, E_NOINTERFACE);
        if (par) ((IUnknown *)par)->lpVtbl->Release((IUnknown *)par);
        SAFE_RELEASE(r);
    }
    SC_REL(sc);
    sc_close(&s);
}

/* ---- Direct3D 12: clear through a command list, read back ------------------------- */
TEST(dxgi_swapchain_d12_pixels_clear)
{
    SC12 s; if (!sc12_open(&s)) return;
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(16, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 2, DXGI_SWAP_EFFECT_FLIP_DISCARD);
    IDXGISwapChain1 *sc = NULL;
    if (!sc_okish(sc12_create(&s, s.hwnd, &d, &sc))) { CHECK(0); sc12_close(&s); return; }
    ID3D12Resource *bb = NULL;
    EXPECT_HR(IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D12Resource, (void **)&bb), S_OK);
    ID3D12DescriptorHeap *heap = NULL; ID3D12CommandAllocator *ca = NULL; ID3D12GraphicsCommandList *cl = NULL;
    D3D12_DESCRIPTOR_HEAP_DESC hd; memset(&hd, 0, sizeof hd); hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
    HRESULT hr = ID3D12Device_CreateDescriptorHeap(s.dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&heap);
    if (SUCCEEDED(hr)) hr = ID3D12Device_CreateCommandAllocator(s.dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&ca);
    if (SUCCEEDED(hr)) hr = ID3D12Device_CreateCommandList(s.dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, ca, NULL, &IID_ID3D12GraphicsCommandList, (void **)&cl);
    if (FAILED(hr) || !bb) { CHECKF(0, "setup failed %s", d3dh_hr_name(hr)); goto out; }
    D3D12_CPU_DESCRIPTOR_HANDLE h = d3dh_d12_heap_cpu_start(heap);
    ID3D12Device_CreateRenderTargetView(s.dev, bb, NULL, h);
    D3D12_RESOURCE_BARRIER b; memset(&b, 0, sizeof b);
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = bb;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &b);
    const float c[4] = { 1, 0, 0, 1 };
    ID3D12GraphicsCommandList_ClearRenderTargetView(cl, h, c, 0, NULL);
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &b);
    EXPECT_HR(ID3D12GraphicsCommandList_Close(cl), S_OK);
    ID3D12CommandQueue_ExecuteCommandLists(s.q, 1, (ID3D12CommandList **)&cl);
    d3dh_d12_flush(s.dev, s.q);
    d3dh_image im;
    if (SUCCEEDED(d3dh_readback_texture2d12(s.dev, bb, D3D12_RESOURCE_STATE_PRESENT, &im))) {
        EXPECT_EQ(im.width, 16); EXPECT_EQ(im.height, 8);
        EXPECT_EQ(d3dh_image_pixel32(&im, 0, 0), 0xFF0000FFu);
        EXPECT_EQ(d3dh_image_pixel32(&im, 15, 7), 0xFF0000FFu);
        d3dh_image_free(&im);
    } else CHECK(0);
    EXPECT_HR(IDXGISwapChain1_Present(sc, 0, 0), S_OK);
    d3dh_d12_flush(s.dev, s.q);
out:
    SAFE_RELEASE(cl); SAFE_RELEASE(ca); SAFE_RELEASE(heap); SAFE_RELEASE(bb);
    SC_REL(sc);
    sc12_close(&s);
}

TEST(dxgi_swapchain_d12_buffer_resource_desc)
{
    SC12 s; if (!sc12_open(&s)) return;
    DXGI_SWAP_CHAIN_DESC1 d = sc_desc1(40, 20, DXGI_FORMAT_B8G8R8A8_UNORM, 3, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
    IDXGISwapChain1 *sc = NULL;
    if (!sc_okish(sc12_create(&s, s.hwnd, &d, &sc))) { CHECK(0); sc12_close(&s); return; }
    ID3D12Resource *prev = NULL;
    for (UINT j = 0; j < 3; j++) {
        ID3D12Resource *r = NULL;
        EXPECT_HR(IDXGISwapChain1_GetBuffer(sc, j, &IID_ID3D12Resource, (void **)&r), S_OK);
        if (!r) continue;
        D3D12_RESOURCE_DESC rd = d3dh_d12_resource_desc(r);
        EXPECT_EQ(rd.Dimension, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
        EXPECT_EQ(rd.Width, 40); EXPECT_EQ(rd.Height, 20);
        EXPECT_EQ(rd.DepthOrArraySize, 1); EXPECT_EQ(rd.MipLevels, 1);
        EXPECT_EQ(rd.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
        EXPECT_EQ(rd.SampleDesc.Count, 1);
        CHECK(rd.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        CHECK(r != prev);
        SAFE_RELEASE(prev);
        prev = r;
    }
    SAFE_RELEASE(prev);
    d3dh_d12_flush(s.dev, s.q);
    SC_REL(sc);
    sc12_close(&s);
}
