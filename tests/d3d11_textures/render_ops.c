/* ClearRenderTargetView + readback, ResolveSubresource, GenerateMips, UAV clears on textures. */
#include "txh.h"
#include <math.h>

static HRESULT tx_rtv(tx_ctx *c, ID3D11Resource *r, DXGI_FORMAT f, D3D11_RTV_DIMENSION dim, UINT mip, UINT first, UINT n, ID3D11RenderTargetView **out) {
    D3D11_RENDER_TARGET_VIEW_DESC d; memset(&d, 0, sizeof d); d.Format = f; d.ViewDimension = dim;
    switch (dim) {
    case D3D11_RTV_DIMENSION_TEXTURE1D: d.Texture1D.MipSlice = mip; break;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: d.Texture1DArray.MipSlice = mip; d.Texture1DArray.FirstArraySlice = first; d.Texture1DArray.ArraySize = n; break;
    case D3D11_RTV_DIMENSION_TEXTURE2D: d.Texture2D.MipSlice = mip; break;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: d.Texture2DArray.MipSlice = mip; d.Texture2DArray.FirstArraySlice = first; d.Texture2DArray.ArraySize = n; break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: d.Texture2DMSArray.FirstArraySlice = first; d.Texture2DMSArray.ArraySize = n; break;
    case D3D11_RTV_DIMENSION_TEXTURE3D: d.Texture3D.MipSlice = mip; d.Texture3D.FirstWSlice = first; d.Texture3D.WSize = n; break;
    default: break;
    }
    *out = NULL;
    return ID3D11Device_CreateRenderTargetView(c->dev, r, &d, out);
}
static int tx_clear_sub2d(tx_ctx *c, ID3D11Texture2D *t, DXGI_FORMAT vf, UINT mip, UINT slice, const float col[4]) {
    ID3D11RenderTargetView *v = NULL; D3D11_TEXTURE2D_DESC td; HRESULT hr;
    ID3D11Texture2D_GetDesc(t, &td);
    hr = td.ArraySize == 1 ? tx_rtv(c, (ID3D11Resource *)t, vf, D3D11_RTV_DIMENSION_TEXTURE2D, mip, 0, 0, &v)
                           : tx_rtv(c, (ID3D11Resource *)t, vf, D3D11_RTV_DIMENSION_TEXTURE2DARRAY, mip, slice, 1, &v);
    EXPECT_HR(hr, S_OK);
    if (FAILED(hr)) return 0;
    ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, col);
    ID3D11RenderTargetView_Release(v);
    return 1;
}
/* every byte within tol of expected[0..n) in every texel; texel size == n or n < texel size (only leading bytes compared) */
static void tx_check_const(const tx_img *im, UINT texel, const unsigned char *exp, UINT n, int tol, const char *what, const char *file, int line) {
    UINT total = im->w * im->rows * im->d, i, k;
    for (i = 0; i < total; i++) for (k = 0; k < n; k++) {
        int got = im->data[(size_t)i * texel + k], d = got - exp[k];
        if (d < -tol || d > tol) { t_check(0, file, line, what, "texel %u byte %u: got %02x want %02x (tol %d)", i, k, got, exp[k], tol); return; }
    }
    t_check(1, file, line, what, NULL);
}

/* ---- ClearRenderTargetView + readback ------------------------------------------------------------------------------------ */
typedef struct { DXGI_FORMAT fmt; float col[4]; UINT texel; UINT n; unsigned char exp[16]; int tol; int hard; } tx_clr;
static void tx_clear_run(tx_ctx *c, const tx_clr *p, const char *file, int line) {
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im; HRESULT hr; UINT total, i, k; int ok = 1;
    hr = tx_mk2d(c, 5, 3, 1, 1, p->fmt, U_DEF, B_RT, 0, 0, NULL, &t);
    if (FAILED(hr)) { t_note("%s:%d RT format %u not creatable: %s", file, line, p->fmt, d3dh_hr_name(hr)); return; }
    hr = tx_rtv(c, (ID3D11Resource *)t, p->fmt, D3D11_RTV_DIMENSION_TEXTURE2D, 0, 0, 0, &v);
    t_check(hr == S_OK, file, line, "CreateRenderTargetView", "%s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, p->col);
        hr = tx_fetch(c, (ID3D11Resource *)t, 0, &im);
        t_check(hr == S_OK, file, line, "readback", "%s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            total = im.w * im.rows;
            for (i = 0; i < total && ok; i++) for (k = 0; k < p->n; k++) {
                int got = im.data[(size_t)i * p->texel + k], d = got - p->exp[k];
                if (d < -p->tol || d > p->tol) {
                    ok = 0;
                    if (p->hard) t_check(0, file, line, "cleared texel", "texel %u byte %u: got %02x want %02x (tol %d)", i, k, got, p->exp[k], p->tol);
                    else t_note("%s:%d texel %u byte %u: got %02x want %02x", file, line, i, k, got, p->exp[k]);
                    break;
                }
            }
            if (ok) t_check(1, file, line, "cleared texel", NULL);
            tx_img_free(&im);
        }
        ID3D11RenderTargetView_Release(v);
    }
    ID3D11Texture2D_Release(t);
}
#define CLR(nm, F, TEXEL, N, TOL, HARD, R, G, B, A, ...) DEVTEST(d3d11_textures_clear_##nm) { \
    tx_clr p = { F, { R, G, B, A }, TEXEL, N, { __VA_ARGS__ }, TOL, HARD }; tx_clear_run(c, &p, __FILE__, __LINE__); }
CLR(rgba8, F_RGBA8, 4, 4, 1, 1, 0.2f, 0.4f, 0.6f, 0.8f, 51, 102, 153, 204)
CLR(rgba8_zero, F_RGBA8, 4, 4, 0, 1, 0.f, 0.f, 0.f, 0.f, 0, 0, 0, 0)
CLR(rgba8_one, F_RGBA8, 4, 4, 0, 1, 1.f, 1.f, 1.f, 1.f, 255, 255, 255, 255)
CLR(rgba8_clamped, F_RGBA8, 4, 4, 0, 1, -1.f, 2.f, 0.f, 1.f, 0, 255, 0, 255)
CLR(rgba8_clamped_big, F_RGBA8, 4, 4, 0, 1, 100.f, -100.f, 1.5f, -0.5f, 255, 0, 255, 0)
CLR(rgba8_channels_distinct, F_RGBA8, 4, 4, 1, 1, 1.f, 0.f, 0.f, 0.f, 255, 0, 0, 0)
CLR(rgba8_green_only, F_RGBA8, 4, 4, 1, 1, 0.f, 1.f, 0.f, 0.f, 0, 255, 0, 0)
CLR(rgba8_blue_only, F_RGBA8, 4, 4, 1, 1, 0.f, 0.f, 1.f, 0.f, 0, 0, 255, 0)
CLR(rgba8_alpha_only, F_RGBA8, 4, 4, 1, 1, 0.f, 0.f, 0.f, 1.f, 0, 0, 0, 255)
CLR(bgra8, F_BGRA8, 4, 4, 1, 1, 0.2f, 0.4f, 0.6f, 0.8f, 153, 102, 51, 204)
CLR(bgra8_red_is_third_byte, F_BGRA8, 4, 4, 1, 1, 1.f, 0.f, 0.f, 0.f, 0, 0, 255, 0)
CLR(bgrx8, DXGI_FORMAT_B8G8R8X8_UNORM, 4, 3, 1, 1, 0.2f, 0.4f, 0.6f, 0.8f, 153, 102, 51)
CLR(rgba8_srgb, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, 4, 1, 1, 0.2f, 0.4f, 0.6f, 0.8f, 124, 170, 203, 204)
CLR(rgba8_srgb_extremes, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, 4, 0, 1, 0.f, 1.f, 0.f, 1.f, 0, 255, 0, 255)
CLR(bgra8_srgb, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 4, 4, 1, 1, 0.2f, 0.4f, 0.6f, 0.8f, 203, 170, 124, 204)
CLR(r8, F_R8, 1, 1, 1, 1, 0.4f, 0.f, 0.f, 0.f, 102)
CLR(r8_clamped, F_R8, 1, 1, 0, 1, 7.f, 0.f, 0.f, 0.f, 255)
CLR(r8g8, DXGI_FORMAT_R8G8_UNORM, 2, 2, 1, 1, 0.2f, 0.4f, 0.f, 0.f, 51, 102)
CLR(a8, DXGI_FORMAT_A8_UNORM, 1, 1, 1, 1, 0.f, 0.f, 0.f, 0.8f, 204)
CLR(r16, DXGI_FORMAT_R16_UNORM, 2, 2, 1, 1, 0.4f, 0.f, 0.f, 0.f, 0x66, 0x66)
CLR(r16g16b16a16_unorm, DXGI_FORMAT_R16G16B16A16_UNORM, 8, 8, 1, 1, 0.2f, 0.4f, 0.6f, 0.8f, 0x33, 0x33, 0x66, 0x66, 0x99, 0x99, 0xCC, 0xCC)
CLR(r16g16b16a16_float, DXGI_FORMAT_R16G16B16A16_FLOAT, 8, 8, 0, 1, 0.5f, 0.25f, 2.f, 1.f, 0x00, 0x38, 0x00, 0x34, 0x00, 0x40, 0x00, 0x3C)
CLR(r16g16b16a16_float_negative, DXGI_FORMAT_R16G16B16A16_FLOAT, 8, 8, 0, 1, -0.5f, 0.f, 1.f, -2.f, 0x00, 0xB8, 0x00, 0x00, 0x00, 0x3C, 0x00, 0xC0)
CLR(r16g16b16a16_float_unclamped, DXGI_FORMAT_R16G16B16A16_FLOAT, 8, 8, 0, 1, 4.f, 8.f, 16.f, 0.5f, 0x00, 0x44, 0x00, 0x48, 0x00, 0x4C, 0x00, 0x38)
CLR(r16g16_float, DXGI_FORMAT_R16G16_FLOAT, 4, 4, 0, 1, 1.f, -1.f, 0.f, 0.f, 0x00, 0x3C, 0x00, 0xBC)
CLR(r16_float, DXGI_FORMAT_R16_FLOAT, 2, 2, 0, 1, 0.5f, 0.f, 0.f, 0.f, 0x00, 0x38)
CLR(r32_float, DXGI_FORMAT_R32_FLOAT, 4, 4, 0, 1, 0.375f, 0.f, 0.f, 0.f, 0x00, 0x00, 0xC0, 0x3E)
CLR(r32_float_big, DXGI_FORMAT_R32_FLOAT, 4, 4, 0, 1, 100.f, 0.f, 0.f, 0.f, 0x00, 0x00, 0xC8, 0x42)
CLR(r32g32_float, DXGI_FORMAT_R32G32_FLOAT, 8, 8, 0, 1, 0.25f, 8.f, 0.f, 0.f, 0x00, 0x00, 0x80, 0x3E, 0x00, 0x00, 0x00, 0x41)
CLR(r32g32b32a32_float, DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 16, 0, 1, 1.5f, -2.f, 0.f, 1.f, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F)
CLR(r32g32b32a32_float_big_values, DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 16, 0, 1, 65504.f, 0.0625f, 0.f, 3.f, 0x00, 0xE0, 0x7F, 0x47, 0x00, 0x00, 0x80, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40)
CLR(rgba8_snorm, DXGI_FORMAT_R8G8B8A8_SNORM, 4, 4, 1, 1, -0.5f, 0.5f, 1.f, -1.f, 0xC0, 0x40, 0x7F, 0x81)
CLR(rgba8_snorm_clamped, DXGI_FORMAT_R8G8B8A8_SNORM, 4, 4, 1, 1, -3.f, 3.f, 0.f, 0.f, 0x81, 0x7F, 0x00, 0x00)
CLR(rgba8_uint_soft, DXGI_FORMAT_R8G8B8A8_UINT, 4, 4, 0, 0, 1.f, 2.f, 3.f, 4.f, 1, 2, 3, 4)
CLR(r32_uint_soft, DXGI_FORMAT_R32_UINT, 4, 4, 0, 0, 7.f, 0.f, 0.f, 0.f, 7, 0, 0, 0)
CLR(r32_sint_soft, DXGI_FORMAT_R32_SINT, 4, 4, 0, 0, -7.f, 0.f, 0.f, 0.f, 0xF9, 0xFF, 0xFF, 0xFF)
CLR(r11g11b10_float_soft, DXGI_FORMAT_R11G11B10_FLOAT, 4, 4, 0, 0, 1.f, 1.f, 1.f, 0.f, 0xC0, 0x03, 0x1E, 0x78)

/* 10:10:10:2 needs field-wise comparison */
DEVTEST(d3d11_textures_clear_r10g10b10a2) {
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im; static const float col[4] = {0.2f, 0.4f, 0.6f, 0.8f}; UINT px, r, g, b, a;
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, DXGI_FORMAT_R10G10B10A2_UNORM, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, DXGI_FORMAT_R10G10B10A2_UNORM, D3D11_RTV_DIMENSION_TEXTURE2D, 0, 0, 0, &v), S_OK);
    if (v) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, col);
        EXPECT_HR(tx_fetch(c, (ID3D11Resource *)t, 0, &im), S_OK);
        if (im.data) {
            px = tx_px32(&im, 3, 2, 0); r = px & 1023; g = (px >> 10) & 1023; b = (px >> 20) & 1023; a = px >> 30;
            CHECKF(abs((int)r - 205) <= 1 && abs((int)g - 409) <= 1 && abs((int)b - 614) <= 1, "rgb %u %u %u", r, g, b);
            CHECKF(a == 2, "alpha %u", a);
            tx_img_free(&im);
        }
        ID3D11RenderTargetView_Release(v);
    }
    ID3D11Texture2D_Release(t);
}
/* Viewing a typeless / UNORM texture through the sRGB twin reinterprets the clear colour. */
static void tx_clear_view_fmt(tx_ctx *c, DXGI_FORMAT texf, DXGI_FORMAT viewf, const unsigned char *exp, const char *file, int line) {
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im; static const float col[4] = {0.2f, 0.4f, 0.6f, 0.8f};
    HRESULT hr = tx_mk2d(c, 4, 4, 1, 1, texf, U_DEF, B_RT, 0, 0, NULL, &t);
    t_check(hr == S_OK, file, line, "create", "%s", d3dh_hr_name(hr)); if (FAILED(hr)) return;
    hr = tx_rtv(c, (ID3D11Resource *)t, viewf, D3D11_RTV_DIMENSION_TEXTURE2D, 0, 0, 0, &v);
    t_check(hr == S_OK, file, line, "CreateRenderTargetView(view format)", "%s", d3dh_hr_name(hr));
    if (v) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, col);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { tx_check_const(&im, 4, exp, 4, 1, "reinterpreted clear", file, line); tx_img_free(&im); }
        ID3D11RenderTargetView_Release(v);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_clear_typeless_as_srgb) { static const unsigned char e[4] = {124, 170, 203, 204}; tx_clear_view_fmt(c, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, e, __FILE__, __LINE__); }
DEVTEST(d3d11_textures_clear_typeless_as_unorm) { static const unsigned char e[4] = {51, 102, 153, 204}; tx_clear_view_fmt(c, DXGI_FORMAT_R8G8B8A8_TYPELESS, F_RGBA8, e, __FILE__, __LINE__); }
DEVTEST(d3d11_textures_clear_unorm_as_srgb) { static const unsigned char e[4] = {124, 170, 203, 204}; tx_clear_view_fmt(c, F_RGBA8, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, e, __FILE__, __LINE__); }
DEVTEST(d3d11_textures_clear_srgb_as_unorm) { static const unsigned char e[4] = {51, 102, 153, 204}; tx_clear_view_fmt(c, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, F_RGBA8, e, __FILE__, __LINE__); }
DEVTEST(d3d11_textures_clear_bgra_typeless_as_unorm) { static const unsigned char e[4] = {153, 102, 51, 204}; tx_clear_view_fmt(c, DXGI_FORMAT_B8G8R8A8_TYPELESS, F_BGRA8, e, __FILE__, __LINE__); }
DEVTEST(d3d11_textures_clear_rtv_incompatible_format) {
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *v = NULL;
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, DXGI_FORMAT_R32_FLOAT, D3D11_RTV_DIMENSION_TEXTURE2D, 0, 0, 0, &v), E_INVALIDARG);
    SAFE_RELEASE(v);
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, DXGI_FORMAT_R16G16_FLOAT, D3D11_RTV_DIMENSION_TEXTURE2D, 0, 0, 0, &v), E_INVALIDARG);
    SAFE_RELEASE(v);
    ID3D11Texture2D_Release(t);
}

/* Subresource selection through the view. */
DEVTEST(d3d11_textures_clear_single_mip_and_slice) {
    static const float A[4] = {0.2f, 0.4f, 0.6f, 0.8f}, B[4] = {1.f, 0.f, 0.f, 1.f};
    static const unsigned char eA[4] = {51, 102, 153, 204}, eB[4] = {255, 0, 0, 255};
    ID3D11Texture2D *t = NULL; UINT s, m, a;
    EXPECT_HR(tx_mk2d(c, 16, 16, 4, 2, F_RGBA8, U_DEF, B_RT | B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    for (a = 0; a < 2; a++) for (m = 0; m < 4; m++) tx_clear_sub2d(c, t, F_RGBA8, m, a, A);
    tx_clear_sub2d(c, t, F_RGBA8, 2, 1, B);
    for (s = 0; s < 8; s++) {
        tx_img im;
        if (FAILED(tx_fetch(c, (ID3D11Resource *)t, s, &im))) { CHECK(0); continue; }
        tx_check_const(&im, 4, s == 1 * 4 + 2 ? eB : eA, 4, 1, s == 6 ? "cleared subresource" : "untouched subresource", __FILE__, __LINE__);
        tx_img_free(&im);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_clear_array_range) {
    static const float A[4] = {0.2f, 0.4f, 0.6f, 0.8f}, B[4] = {1.f, 1.f, 0.f, 1.f};
    static const unsigned char eA[4] = {51, 102, 153, 204}, eB[4] = {255, 255, 0, 255};
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *v = NULL; UINT s;
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 4, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    for (s = 0; s < 4; s++) tx_clear_sub2d(c, t, F_RGBA8, 0, s, A);
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE2DARRAY, 0, 1, 2, &v), S_OK);
    if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, B);
    SAFE_RELEASE(v);
    for (s = 0; s < 4; s++) {
        tx_img im;
        if (FAILED(tx_fetch(c, (ID3D11Resource *)t, s, &im))) { CHECK(0); continue; }
        tx_check_const(&im, 4, (s == 1 || s == 2) ? eB : eA, 4, 1, "array slice", __FILE__, __LINE__);
        tx_img_free(&im);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_clear_array_view_on_non_array_texture) {
    static const float B[4] = {0.f, 1.f, 0.f, 1.f}; static const unsigned char eB[4] = {0, 255, 0, 255};
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im;
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    { HRESULT h_ = tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE2DARRAY, 0, 0, 1, &v); t_note("array-dimension RTV on a non-array texture -> %s", d3dh_hr_name(h_)); }
    if (v) { ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, B); if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { tx_check_const(&im, 4, eB, 4, 0, "clear", __FILE__, __LINE__); tx_img_free(&im); } }
    SAFE_RELEASE(v);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_clear_3d_slices) {
    static const float A[4] = {0.2f, 0.4f, 0.6f, 0.8f}, B[4] = {0.f, 0.f, 1.f, 1.f};
    static const unsigned char eA[4] = {51, 102, 153, 204}, eB[4] = {0, 0, 255, 255};
    ID3D11Texture3D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im; UINT z;
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE3D, 0, 0, 4, &v), S_OK);
    if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, A);
    SAFE_RELEASE(v);
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE3D, 0, 1, 2, &v), S_OK);
    if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, B);
    SAFE_RELEASE(v);
    if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) {
        for (z = 0; z < 4; z++) {
            const unsigned char *e = (z == 1 || z == 2) ? eB : eA; UINT i; int bad = 0;
            for (i = 0; i < 16 * 4; i++) if (im.data[(size_t)z * 64 + i] != e[i & 3]) bad = 1;
            CHECKF(!bad, "3D slice %u has wrong colour", z);
        }
        tx_img_free(&im);
    }
    ID3D11Texture3D_Release(t);
}
DEVTEST(d3d11_textures_clear_3d_mip1) {
    static const float A[4] = {0.2f, 0.4f, 0.6f, 0.8f}, B[4] = {1.f, 0.f, 1.f, 1.f};
    static const unsigned char eA[4] = {51, 102, 153, 204}, eB[4] = {255, 0, 255, 255};
    ID3D11Texture3D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im;
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 3, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    for (UINT m = 0; m < 3; m++) {
        EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE3D, m, 0, tx_dim(4, m), &v), S_OK);
        if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, A);
        SAFE_RELEASE(v);
    }
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE3D, 1, 0, 2, &v), S_OK);
    if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, B);
    SAFE_RELEASE(v);
    for (UINT s = 0; s < 3; s++) if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, s, &im))) { tx_check_const(&im, 4, s == 1 ? eB : eA, 4, 1, "3D mip", __FILE__, __LINE__); tx_img_free(&im); }
    ID3D11Texture3D_Release(t);
}
DEVTEST(d3d11_textures_clear_1d) {
    static const float A[4] = {0.2f, 0.4f, 0.6f, 0.8f}; static const unsigned char eA[4] = {51, 102, 153, 204};
    ID3D11Texture1D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im;
    EXPECT_HR(tx_mk1d(c, 13, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE1D, 0, 0, 0, &v), S_OK);
    if (v) { ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, A); if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { tx_check_const(&im, 4, eA, 4, 1, "1D clear", __FILE__, __LINE__); tx_img_free(&im); } }
    SAFE_RELEASE(v);
    ID3D11Texture1D_Release(t);
}
DEVTEST(d3d11_textures_clear_1d_array_mip) {
    static const float A[4] = {0.2f, 0.4f, 0.6f, 0.8f}, B[4] = {1.f, 0.f, 0.f, 1.f};
    static const unsigned char eA[4] = {51, 102, 153, 204}, eB[4] = {255, 0, 0, 255};
    ID3D11Texture1D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im; UINT m, a, s;
    EXPECT_HR(tx_mk1d(c, 8, 3, 2, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    for (a = 0; a < 2; a++) for (m = 0; m < 3; m++) {
        EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE1DARRAY, m, a, 1, &v), S_OK);
        if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, (a == 1 && m == 1) ? B : A);
        SAFE_RELEASE(v);
    }
    for (s = 0; s < 6; s++) if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, s, &im))) { tx_check_const(&im, 4, s == 4 ? eB : eA, 4, 1, "1D array subresource", __FILE__, __LINE__); tx_img_free(&im); }
    ID3D11Texture1D_Release(t);
}
DEVTEST(d3d11_textures_clear_overwrites_previous_clear) {
    static const float A[4] = {1.f, 0.f, 0.f, 1.f}, B[4] = {0.f, 1.f, 0.f, 1.f}; static const unsigned char eB[4] = {0, 255, 0, 255};
    ID3D11Texture2D *t = NULL; ID3D11RenderTargetView *v = NULL; tx_img im;
    EXPECT_HR(tx_mk2d(c, 33, 7, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE2D, 0, 0, 0, &v), S_OK);
    if (v) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, A);
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, B);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { tx_check_const(&im, 4, eB, 4, 0, "second clear wins", __FILE__, __LINE__); tx_img_free(&im); }
    }
    SAFE_RELEASE(v);
    ID3D11Texture2D_Release(t);
}

/* UAV clears on texture UAVs */
DEVTEST(d3d11_textures_uav_clear_uint) {
    ID3D11Texture2D *t = NULL; ID3D11UnorderedAccessView *u = NULL; D3D11_UNORDERED_ACCESS_VIEW_DESC d; tx_img im; static const UINT v[4] = {0x01020304, 0, 0, 0};
    if (c->fl < FL_11_0) { t_skip("needs feature level 11_0"); return; }
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, DXGI_FORMAT_R32_UINT, U_DEF, B_UAV, 0, 0, NULL, &t), S_OK); if (!t) return;
    memset(&d, 0, sizeof d); d.Format = DXGI_FORMAT_R32_UINT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(c->dev, (ID3D11Resource *)t, &d, &u), S_OK);
    if (u) {
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(c->ctx, u, v);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { for (UINT i = 0; i < 16; i++) EXPECT_EQ(tx_px32(&im, i % 4, i / 4, 0), 0x01020304); tx_img_free(&im); }
        ID3D11UnorderedAccessView_Release(u);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_uav_clear_float) {
    ID3D11Texture2D *t = NULL; ID3D11UnorderedAccessView *u = NULL; D3D11_UNORDERED_ACCESS_VIEW_DESC d; tx_img im; static const float v[4] = {0.375f, 0, 0, 0};
    if (c->fl < FL_11_0) { t_skip("needs feature level 11_0"); return; }
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, DXGI_FORMAT_R32_FLOAT, U_DEF, B_UAV, 0, 0, NULL, &t), S_OK); if (!t) return;
    memset(&d, 0, sizeof d); d.Format = DXGI_FORMAT_R32_FLOAT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(c->dev, (ID3D11Resource *)t, &d, &u), S_OK);
    if (u) {
        ID3D11DeviceContext_ClearUnorderedAccessViewFloat(c->ctx, u, v);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { EXPECT_EQ(tx_px32(&im, 2, 2, 0), 0x3EC00000); tx_img_free(&im); }
        ID3D11UnorderedAccessView_Release(u);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_uav_clear_3d_mip) {
    ID3D11Texture3D *t = NULL; ID3D11UnorderedAccessView *u = NULL; D3D11_UNORDERED_ACCESS_VIEW_DESC d; tx_img im; static const UINT v[4] = {77, 0, 0, 0}, z[4] = {0, 0, 0, 0};
    if (c->fl < FL_11_0) { t_skip("needs feature level 11_0"); return; }
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 2, DXGI_FORMAT_R32_UINT, U_DEF, B_UAV, 0, 0, NULL, &t), S_OK); if (!t) return;
    for (UINT m = 0; m < 2; m++) {
        memset(&d, 0, sizeof d); d.Format = DXGI_FORMAT_R32_UINT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D; d.Texture3D.MipSlice = m; d.Texture3D.FirstWSlice = 0; d.Texture3D.WSize = tx_dim(4, m);
        u = NULL; EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(c->dev, (ID3D11Resource *)t, &d, &u), S_OK);
        if (u) { ID3D11DeviceContext_ClearUnorderedAccessViewUint(c->ctx, u, m == 1 ? v : z); ID3D11UnorderedAccessView_Release(u); }
    }
    if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 1, &im))) { for (UINT i = 0; i < 8; i++) EXPECT_EQ(tx_px32(&im, i % 2, (i / 2) % 2, i / 4), 77); tx_img_free(&im); }
    if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { EXPECT_EQ(tx_px32(&im, 3, 3, 3), 0); tx_img_free(&im); }
    ID3D11Texture3D_Release(t);
}

/* ---- ResolveSubresource ---------------------------------------------------------------------------------------------------- */
static UINT tx_msaa_levels(tx_ctx *c, DXGI_FORMAT f, UINT n) { UINT q = 0; if (FAILED(ID3D11Device_CheckMultisampleQualityLevels(c->dev, f, n, &q))) return 0; return q; }
typedef struct { DXGI_FORMAT texf, viewf, resolvef; UINT count; float col[4]; UINT texel; UINT n; unsigned char exp[16]; int tol; } tx_res;
static void tx_resolve_run(tx_ctx *c, const tx_res *p, const char *file, int line) {
    ID3D11Texture2D *ms = NULL, *dst = NULL; ID3D11RenderTargetView *v = NULL; tx_img im; HRESULT hr; D3D11_TEXTURE2D_DESC d;
    if (!tx_msaa_levels(c, p->viewf, p->count)) { t_skip("%ux MSAA not supported for format %u", p->count, p->viewf); return; }
    memset(&d, 0, sizeof d);
    d.Width = 8; d.Height = 6; d.MipLevels = 1; d.ArraySize = 1; d.Format = p->texf; d.SampleDesc.Count = p->count; d.Usage = U_DEF; d.BindFlags = B_RT;
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &ms);
    t_check(hr == S_OK, file, line, "create MSAA", "%s", d3dh_hr_name(hr)); if (FAILED(hr)) return;
    hr = tx_mk2d(c, 8, 6, 1, 1, p->texf, U_DEF, 0, 0, 0, NULL, &dst);
    t_check(hr == S_OK, file, line, "create resolve target", "%s", d3dh_hr_name(hr));
    hr = tx_rtv(c, (ID3D11Resource *)ms, p->viewf, D3D11_RTV_DIMENSION_TEXTURE2DMS, 0, 0, 0, &v);
    t_check(hr == S_OK, file, line, "MSAA RTV", "%s", d3dh_hr_name(hr));
    if (v && dst) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, p->col);
        ID3D11DeviceContext_ResolveSubresource(c->ctx, (ID3D11Resource *)dst, 0, (ID3D11Resource *)ms, 0, p->resolvef);
        hr = tx_fetch(c, (ID3D11Resource *)dst, 0, &im);
        t_check(hr == S_OK, file, line, "readback", "%s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) { tx_check_const(&im, p->texel, p->exp, p->n, p->tol, "resolved texel", file, line); tx_img_free(&im); }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(dst); SAFE_RELEASE(ms);
}
#define RES(nm, TF, VF, RF, CNT, TEXEL, N, TOL, R, G, B, A, ...) DEVTEST(d3d11_textures_resolve_##nm) { \
    tx_res p = { TF, VF, RF, CNT, { R, G, B, A }, TEXEL, N, { __VA_ARGS__ }, TOL }; tx_resolve_run(c, &p, __FILE__, __LINE__); }
RES(rgba8_4x, F_RGBA8, F_RGBA8, F_RGBA8, 4, 4, 4, 1, 0.2f, 0.4f, 0.6f, 0.8f, 51, 102, 153, 204)
RES(rgba8_2x, F_RGBA8, F_RGBA8, F_RGBA8, 2, 4, 4, 1, 0.2f, 0.4f, 0.6f, 0.8f, 51, 102, 153, 204)
RES(rgba8_8x, F_RGBA8, F_RGBA8, F_RGBA8, 8, 4, 4, 1, 0.2f, 0.4f, 0.6f, 0.8f, 51, 102, 153, 204)
RES(rgba8_4x_extremes, F_RGBA8, F_RGBA8, F_RGBA8, 4, 4, 4, 0, 1.f, 0.f, 1.f, 0.f, 255, 0, 255, 0)
RES(bgra8_4x, F_BGRA8, F_BGRA8, F_BGRA8, 4, 4, 4, 1, 0.2f, 0.4f, 0.6f, 0.8f, 153, 102, 51, 204)
RES(rgba8_srgb_4x, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, 4, 4, 1, 0.2f, 0.4f, 0.6f, 0.8f, 124, 170, 203, 204)
RES(typeless_unorm_4x, DXGI_FORMAT_R8G8B8A8_TYPELESS, F_RGBA8, F_RGBA8, 4, 4, 4, 1, 0.2f, 0.4f, 0.6f, 0.8f, 51, 102, 153, 204)
RES(typeless_srgb_4x, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, 4, 4, 1, 0.2f, 0.4f, 0.6f, 0.8f, 124, 170, 203, 204)
RES(r8_4x, F_R8, F_R8, F_R8, 4, 1, 1, 1, 0.4f, 0.f, 0.f, 0.f, 102)
RES(r8g8_4x, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM, 4, 2, 2, 1, 0.2f, 0.4f, 0.f, 0.f, 51, 102)
RES(r16g16b16a16_float_4x, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, 4, 8, 8, 0, 0.5f, 0.25f, 2.f, 1.f, 0x00, 0x38, 0x00, 0x34, 0x00, 0x40, 0x00, 0x3C)
RES(r16g16b16a16_unorm_4x, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM, 4, 8, 8, 1, 0.2f, 0.4f, 0.6f, 0.8f, 0x33, 0x33, 0x66, 0x66, 0x99, 0x99, 0xCC, 0xCC)
RES(r32_float_4x, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, 4, 4, 4, 0, 0.375f, 0.f, 0.f, 0.f, 0x00, 0x00, 0xC0, 0x3E)
RES(r32g32b32a32_float_4x, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 16, 16, 0, 1.5f, -2.f, 0.f, 1.f, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F)
RES(r10g10b10a2_4x_zero, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, 4, 4, 4, 0, 0.f, 0.f, 0.f, 0.f, 0, 0, 0, 0)

DEVTEST(d3d11_textures_resolve_into_mip_of_larger_texture) {
    static const float col[4] = {0.2f, 0.4f, 0.6f, 0.8f}; static const unsigned char e[4] = {51, 102, 153, 204};
    ID3D11Texture2D *ms = NULL, *dst = NULL; ID3D11RenderTargetView *v = NULL; D3D11_TEXTURE2D_DESC d; tx_init in; tx_img im; size_t bad;
    if (!tx_msaa_levels(c, F_RGBA8, 4)) { t_skip("4x MSAA unsupported"); return; }
    memset(&d, 0, sizeof d); d.Width = 8; d.Height = 8; d.MipLevels = 1; d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 4; d.Usage = U_DEF; d.BindFlags = B_RT;
    EXPECT_HR(ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &ms), S_OK);
    CHECK(tx_init_make(&in, 16, 16, 1, 2, 1, F_RGBA8, 0, 0, 800));
    EXPECT_HR(tx_mk2d(c, 16, 16, 2, 1, F_RGBA8, U_DEF, 0, 0, 0, in.sd, &dst), S_OK);
    if (ms && dst) {
        EXPECT_HR(tx_rtv(c, (ID3D11Resource *)ms, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE2DMS, 0, 0, 0, &v), S_OK);
        if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, col);
        ID3D11DeviceContext_ResolveSubresource(c->ctx, (ID3D11Resource *)dst, 1, (ID3D11Resource *)ms, 0, F_RGBA8);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)dst, 1, &im))) { tx_check_const(&im, 4, e, 4, 1, "resolved mip", __FILE__, __LINE__); tx_img_free(&im); }
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)dst, 0, &im))) { CHECK(tx_img_is_pat(&im, 800, &bad)); tx_img_free(&im); }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(dst); SAFE_RELEASE(ms); tx_init_free(&in);
}
DEVTEST(d3d11_textures_resolve_array_slices) {
    static const float c0[4] = {1.f, 0.f, 0.f, 1.f}, c1[4] = {0.f, 0.f, 1.f, 1.f};
    static const unsigned char e0[4] = {255, 0, 0, 255}, e1[4] = {0, 0, 255, 255};
    ID3D11Texture2D *ms = NULL, *dst = NULL; ID3D11RenderTargetView *v = NULL; D3D11_TEXTURE2D_DESC d; tx_img im;
    if (!tx_msaa_levels(c, F_RGBA8, 4)) { t_skip("4x MSAA unsupported"); return; }
    memset(&d, 0, sizeof d); d.Width = 4; d.Height = 4; d.MipLevels = 1; d.ArraySize = 2; d.Format = F_RGBA8; d.SampleDesc.Count = 4; d.Usage = U_DEF; d.BindFlags = B_RT;
    EXPECT_HR(ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &ms), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 2, F_RGBA8, U_DEF, 0, 0, 0, NULL, &dst), S_OK);
    if (ms && dst) {
        for (UINT s = 0; s < 2; s++) {
            EXPECT_HR(tx_rtv(c, (ID3D11Resource *)ms, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY, 0, s, 1, &v), S_OK);
            if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, s ? c1 : c0);
            SAFE_RELEASE(v);
        }
        ID3D11DeviceContext_ResolveSubresource(c->ctx, (ID3D11Resource *)dst, 1, (ID3D11Resource *)ms, 1, F_RGBA8);
        ID3D11DeviceContext_ResolveSubresource(c->ctx, (ID3D11Resource *)dst, 0, (ID3D11Resource *)ms, 0, F_RGBA8);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)dst, 0, &im))) { tx_check_const(&im, 4, e0, 4, 0, "slice 0", __FILE__, __LINE__); tx_img_free(&im); }
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)dst, 1, &im))) { tx_check_const(&im, 4, e1, 4, 0, "slice 1", __FILE__, __LINE__); tx_img_free(&im); }
    }
    SAFE_RELEASE(dst); SAFE_RELEASE(ms);
}
DEVTEST(d3d11_textures_resolve_overwrites_destination) {
    static const float col[4] = {0.f, 1.f, 0.f, 1.f}; static const unsigned char e[4] = {0, 255, 0, 255};
    ID3D11Texture2D *ms = NULL, *dst = NULL; ID3D11RenderTargetView *v = NULL, *dv = NULL; D3D11_TEXTURE2D_DESC d; tx_img im; static const float red[4] = {1.f, 0.f, 0.f, 1.f};
    if (!tx_msaa_levels(c, F_RGBA8, 4)) { t_skip("4x MSAA unsupported"); return; }
    memset(&d, 0, sizeof d); d.Width = 4; d.Height = 4; d.MipLevels = 1; d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 4; d.Usage = U_DEF; d.BindFlags = B_RT;
    EXPECT_HR(ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &ms), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &dst), S_OK);
    if (ms && dst) {
        EXPECT_HR(tx_rtv(c, (ID3D11Resource *)dst, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE2D, 0, 0, 0, &dv), S_OK);
        if (dv) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, dv, red);
        EXPECT_HR(tx_rtv(c, (ID3D11Resource *)ms, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE2DMS, 0, 0, 0, &v), S_OK);
        if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, col);
        ID3D11DeviceContext_ResolveSubresource(c->ctx, (ID3D11Resource *)dst, 0, (ID3D11Resource *)ms, 0, F_RGBA8);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)dst, 0, &im))) { tx_check_const(&im, 4, e, 4, 0, "resolve replaces content", __FILE__, __LINE__); tx_img_free(&im); }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(dv); SAFE_RELEASE(dst); SAFE_RELEASE(ms);
}

/* ---- GenerateMips ---------------------------------------------------------------------------------------------------------------- */
static void tx_genmips_run(tx_ctx *c, DXGI_FORMAT f, UINT w, UINT h, UINT arr, int cube, UINT texel, int tol, const char *file, int line) {
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *srv = NULL; HRESULT hr; UINT mips = tx_full_mips(w > h ? w : h), s, m, k;
    tx_img base[12]; memset(base, 0, sizeof base);
    hr = tx_mk2d(c, w, h, 0, arr, f, U_DEF, B_RT | B_SRV, 0, M_GEN | (cube ? M_CUBE : 0), NULL, &t);
    if (FAILED(hr)) { t_note("%s:%d create GENERATE_MIPS %ux%u fmt %u: %s", file, line, w, h, f, d3dh_hr_name(hr)); return; }
    for (s = 0; s < arr; s++) {
        float col[4] = { 0.1f + 0.15f * s, 0.9f - 0.1f * s, 0.3f + 0.05f * s, 1.f };
        tx_clear_sub2d(c, t, f, 0, s, col);
    }
    hr = ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, NULL, &srv);
    t_check(hr == S_OK, file, line, "CreateShaderResourceView(NULL desc)", "%s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        ID3D11DeviceContext_GenerateMips(c->ctx, srv);
        for (s = 0; s < arr; s++) {
            hr = tx_fetch(c, (ID3D11Resource *)t, s * mips, &base[s]);
            if (FAILED(hr)) continue;
            for (m = 1; m < mips; m++) {
                tx_img im; char what[64];
                if (FAILED(tx_fetch(c, (ID3D11Resource *)t, s * mips + m, &im))) { t_check(0, file, line, "fetch", "slice %u mip %u", s, m); continue; }
                snprintf(what, sizeof what, "slice %u mip %u equals mip 0", s, m);
                tx_check_const(&im, texel, base[s].data, texel, tol, what, file, line);
                tx_img_free(&im);
            }
        }
        for (s = 1; s < arr && s < 12; s++) if (base[s].data && base[0].data) t_check(memcmp(base[s].data, base[0].data, texel) != 0, file, line, "slices were cleared to different colours", NULL);
        ID3D11ShaderResourceView_Release(srv);
    }
    for (k = 0; k < 12; k++) tx_img_free(&base[k]);
    ID3D11Texture2D_Release(t);
}
#define GEN(nm, F, W, H, A, CUBE, TEXEL, TOL) DEVTEST(d3d11_textures_genmips_##nm) { tx_genmips_run(c, F, W, H, A, CUBE, TEXEL, TOL, __FILE__, __LINE__); }
GEN(rgba8_64, F_RGBA8, 64, 64, 1, 0, 4, 1)
GEN(rgba8_1x1, F_RGBA8, 1, 1, 1, 0, 4, 1)
GEN(rgba8_2x2, F_RGBA8, 2, 2, 1, 0, 4, 1)
GEN(rgba8_wide_64x4, F_RGBA8, 64, 4, 1, 0, 4, 1)
GEN(rgba8_tall_4x64, F_RGBA8, 4, 64, 1, 0, 4, 1)
GEN(rgba8_odd_37x19, F_RGBA8, 37, 19, 1, 0, 4, 1)
GEN(rgba8_odd_255x3, F_RGBA8, 255, 3, 1, 0, 4, 1)
GEN(rgba8_256, F_RGBA8, 256, 256, 1, 0, 4, 1)
GEN(bgra8_32, F_BGRA8, 32, 32, 1, 0, 4, 1)
GEN(rgba8_srgb_32, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 32, 32, 1, 0, 4, 1)
GEN(bgra8_srgb_32, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 32, 32, 1, 0, 4, 1)
GEN(r8_32, F_R8, 32, 32, 1, 0, 1, 1)
GEN(r8g8_32, DXGI_FORMAT_R8G8_UNORM, 32, 32, 1, 0, 2, 1)
GEN(r16g16b16a16_unorm_16, DXGI_FORMAT_R16G16B16A16_UNORM, 16, 16, 1, 0, 8, 2)
GEN(r16g16b16a16_float_16, DXGI_FORMAT_R16G16B16A16_FLOAT, 16, 16, 1, 0, 8, 2)
GEN(r32g32b32a32_float_8, DXGI_FORMAT_R32G32B32A32_FLOAT, 8, 8, 1, 0, 16, 2)
GEN(r32_float_8, DXGI_FORMAT_R32_FLOAT, 8, 8, 1, 0, 4, 2)
GEN(r10g10b10a2_8, DXGI_FORMAT_R10G10B10A2_UNORM, 8, 8, 1, 0, 4, 4)
GEN(rgba8_array2, F_RGBA8, 16, 16, 2, 0, 4, 1)
GEN(rgba8_array4, F_RGBA8, 32, 8, 4, 0, 4, 1)
GEN(rgba8_cube, F_RGBA8, 16, 16, 6, 1, 4, 1)
GEN(rgba8_cube_array, F_RGBA8, 8, 8, 12, 1, 4, 1)

DEVTEST(d3d11_textures_genmips_checkerboard_average) {
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *srv = NULL; unsigned char px[8 * 8 * 4]; UINT x, y, m; tx_img im;
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) { unsigned char v = ((x ^ y) & 1) ? 255 : 0; unsigned char *p = px + (y * 8 + x) * 4; p[0] = p[1] = p[2] = v; p[3] = 255; }
    EXPECT_HR(tx_mk2d(c, 8, 8, 0, 1, F_RGBA8, U_DEF, B_RT | B_SRV, 0, M_GEN, NULL, &t), S_OK); if (!t) return;
    ID3D11DeviceContext_UpdateSubresource(c->ctx, (ID3D11Resource *)t, 0, NULL, px, 32, 0);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, NULL, &srv), S_OK);
    if (srv) {
        ID3D11DeviceContext_GenerateMips(c->ctx, srv);
        for (m = 1; m < 4; m++) {
            if (FAILED(tx_fetch(c, (ID3D11Resource *)t, m, &im))) { CHECK(0); continue; }
            for (UINT i = 0; i < im.w * im.rows; i++) {
                int r = im.data[i * 4], g = im.data[i * 4 + 1], b = im.data[i * 4 + 2], a = im.data[i * 4 + 3];
                CHECKF(abs(r - 127) <= 3 && abs(g - 127) <= 3 && abs(b - 127) <= 3 && a == 255, "mip %u texel %u = %d %d %d %d", m, i, r, g, b, a);
            }
            tx_img_free(&im);
        }
        ID3D11ShaderResourceView_Release(srv);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_genmips_leaves_top_level_untouched) {
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *srv = NULL; unsigned char px[4 * 4 * 4]; tx_img im;
    for (UINT i = 0; i < sizeof px; i++) px[i] = (unsigned char)(i * 11 + 5);
    EXPECT_HR(tx_mk2d(c, 4, 4, 0, 1, F_RGBA8, U_DEF, B_RT | B_SRV, 0, M_GEN, NULL, &t), S_OK); if (!t) return;
    ID3D11DeviceContext_UpdateSubresource(c->ctx, (ID3D11Resource *)t, 0, NULL, px, 16, 0);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, NULL, &srv), S_OK);
    if (srv) {
        ID3D11DeviceContext_GenerateMips(c->ctx, srv);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { CHECK(!memcmp(im.data, px, sizeof px)); tx_img_free(&im); }
        ID3D11ShaderResourceView_Release(srv);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_genmips_view_starts_at_mip1) {
    static const float A[4] = {1.f, 0.f, 0.f, 1.f}, B[4] = {0.f, 1.f, 0.f, 1.f};
    static const unsigned char eA[4] = {255, 0, 0, 255}, eB[4] = {0, 255, 0, 255};
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *srv = NULL; D3D11_SHADER_RESOURCE_VIEW_DESC d; tx_img im;
    EXPECT_HR(tx_mk2d(c, 16, 16, 0, 1, F_RGBA8, U_DEF, B_RT | B_SRV, 0, M_GEN, NULL, &t), S_OK); if (!t) return;
    tx_clear_sub2d(c, t, F_RGBA8, 0, 0, A);
    tx_clear_sub2d(c, t, F_RGBA8, 1, 0, B);
    memset(&d, 0, sizeof d); d.Format = F_RGBA8; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MostDetailedMip = 1; d.Texture2D.MipLevels = 4;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, &d, &srv), S_OK);
    if (srv) {
        ID3D11DeviceContext_GenerateMips(c->ctx, srv);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { tx_check_const(&im, 4, eA, 4, 0, "mip 0 untouched", __FILE__, __LINE__); tx_img_free(&im); }
        for (UINT m = 1; m < 5; m++) if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, m, &im))) { tx_check_const(&im, 4, eB, 4, 1, "mip derived from mip 1", __FILE__, __LINE__); tx_img_free(&im); }
        ID3D11ShaderResourceView_Release(srv);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_genmips_3d_constant) {
    static const float A[4] = {0.2f, 0.4f, 0.6f, 0.8f}; static const unsigned char eA[4] = {51, 102, 153, 204};
    ID3D11Texture3D *t = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *v = NULL; tx_img im;
    EXPECT_HR(tx_mk3d(c, 8, 8, 8, 0, F_RGBA8, U_DEF, B_RT | B_SRV, 0, M_GEN, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE3D, 0, 0, 8, &v), S_OK);
    if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, A);
    SAFE_RELEASE(v);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, NULL, &srv), S_OK);
    if (srv) {
        ID3D11DeviceContext_GenerateMips(c->ctx, srv);
        for (UINT m = 1; m < 4; m++) if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, m, &im))) { tx_check_const(&im, 4, eA, 4, 1, "3D mip", __FILE__, __LINE__); tx_img_free(&im); }
        ID3D11ShaderResourceView_Release(srv);
    }
    ID3D11Texture3D_Release(t);
}
DEVTEST(d3d11_textures_genmips_1d_constant) {
    static const float A[4] = {0.2f, 0.4f, 0.6f, 0.8f}; static const unsigned char eA[4] = {51, 102, 153, 204};
    ID3D11Texture1D *t = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *v = NULL; tx_img im;
    EXPECT_HR(tx_mk1d(c, 32, 0, 1, F_RGBA8, U_DEF, B_RT | B_SRV, 0, M_GEN, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(tx_rtv(c, (ID3D11Resource *)t, F_RGBA8, D3D11_RTV_DIMENSION_TEXTURE1D, 0, 0, 0, &v), S_OK);
    if (v) ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, A);
    SAFE_RELEASE(v);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, NULL, &srv), S_OK);
    if (srv) {
        ID3D11DeviceContext_GenerateMips(c->ctx, srv);
        for (UINT m = 1; m < 6; m++) if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, m, &im))) { tx_check_const(&im, 4, eA, 4, 1, "1D mip", __FILE__, __LINE__); tx_img_free(&im); }
        ID3D11ShaderResourceView_Release(srv);
    }
    ID3D11Texture1D_Release(t);
}
