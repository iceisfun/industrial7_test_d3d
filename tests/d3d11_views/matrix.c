/* Format x view-kind support matrix: for every DXGI format (1..132) a 64x64 2D texture is created with each of the
 * SRV / RTV / DSV / UAV bind flags and a default (NULL-desc) view is created from it. Expected masks (bit0 SRV, bit1 RTV,
 * bit2 DSV, bit3 UAV) were measured on Windows 10 22H2 with the Basic Render Driver (WARP, feature level 11_1);
 * formats not listed are not creatable with any of the four bind flags. */
#include "views.h"

static const struct { int fmt; unsigned tex, view; } MATRIX[] = {
    { 1, 0xb, 0x0 },
    { 2, 0xb, 0xb },
    { 3, 0xb, 0xb },
    { 4, 0xb, 0xb },
    { 5, 0x3, 0x0 },
    { 6, 0x3, 0x3 },
    { 7, 0x3, 0x3 },
    { 8, 0x3, 0x3 },
    { 9, 0xb, 0x0 },
    { 10, 0xb, 0xb },
    { 11, 0xb, 0xb },
    { 12, 0xb, 0xb },
    { 13, 0xb, 0xb },
    { 14, 0xb, 0xb },
    { 15, 0xb, 0x0 },
    { 16, 0xb, 0xb },
    { 17, 0xb, 0xb },
    { 18, 0xb, 0xb },
    { 19, 0x5, 0x0 },
    { 20, 0x4, 0x4 },
    { 21, 0x1, 0x1 },
    { 22, 0x1, 0x1 },
    { 23, 0xb, 0x0 },
    { 24, 0xb, 0xb },
    { 25, 0xb, 0xb },
    { 26, 0xb, 0xb },
    { 27, 0xb, 0x0 },
    { 28, 0xb, 0xb },
    { 29, 0xb, 0x3 },
    { 30, 0xb, 0xb },
    { 31, 0xb, 0xb },
    { 32, 0xb, 0xb },
    { 33, 0xb, 0x0 },
    { 34, 0xb, 0xb },
    { 35, 0xb, 0xb },
    { 36, 0xb, 0xb },
    { 37, 0xb, 0xb },
    { 38, 0xb, 0xb },
    { 39, 0xf, 0x0 },
    { 40, 0x4, 0x4 },
    { 41, 0xb, 0xb },
    { 42, 0xb, 0xb },
    { 43, 0xb, 0xb },
    { 44, 0x5, 0x0 },
    { 45, 0x4, 0x4 },
    { 46, 0x1, 0x1 },
    { 47, 0x1, 0x1 },
    { 48, 0xb, 0x0 },
    { 49, 0xb, 0xb },
    { 50, 0xb, 0xb },
    { 51, 0xb, 0xb },
    { 52, 0xb, 0xb },
    { 53, 0xf, 0x0 },
    { 54, 0xb, 0xb },
    { 55, 0x4, 0x4 },
    { 56, 0xb, 0xb },
    { 57, 0xb, 0xb },
    { 58, 0xb, 0xb },
    { 59, 0xb, 0xb },
    { 60, 0xb, 0x0 },
    { 61, 0xb, 0xb },
    { 62, 0xb, 0xb },
    { 63, 0xb, 0xb },
    { 64, 0xb, 0xb },
    { 65, 0xb, 0xb },
    { 67, 0x1, 0x1 },
    { 68, 0x1, 0x1 },
    { 69, 0x1, 0x1 },
    { 70, 0x1, 0x0 },
    { 71, 0x1, 0x1 },
    { 72, 0x1, 0x1 },
    { 73, 0x1, 0x0 },
    { 74, 0x1, 0x1 },
    { 75, 0x1, 0x1 },
    { 76, 0x1, 0x0 },
    { 77, 0x1, 0x1 },
    { 78, 0x1, 0x1 },
    { 79, 0x1, 0x0 },
    { 80, 0x1, 0x1 },
    { 81, 0x1, 0x1 },
    { 82, 0x1, 0x0 },
    { 83, 0x1, 0x1 },
    { 84, 0x1, 0x1 },
    { 85, 0xb, 0xb },
    { 86, 0xb, 0xb },
    { 87, 0xb, 0xb },
    { 88, 0x3, 0x3 },
    { 89, 0xb, 0x0 },
    { 90, 0xb, 0x0 },
    { 91, 0xb, 0x3 },
    { 92, 0xb, 0x0 },
    { 93, 0x3, 0x3 },
    { 94, 0x1, 0x0 },
    { 95, 0x1, 0x1 },
    { 96, 0x1, 0x1 },
    { 97, 0x1, 0x0 },
    { 98, 0x1, 0x1 },
    { 99, 0x1, 0x1 },
    { 100, 0xb, 0x0 },
    { 103, 0xb, 0x0 },
    { 107, 0x9, 0x0 },
    { 115, 0xb, 0xb }
};
static const char *KIND[4] = { "SRV", "RTV", "DSV", "UAV" };

TEST(d3d11_views_format_bind_matrix_default_views)
{
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0; D3D_DRIVER_TYPE dt = 0;
    REQUIRE(d3dh_D3D11CreateDevice());
    if (FAILED(d3dh_make_device11(&dev, &ctx, &fl, &dt, 0))) { t_skip("no D3D11 device"); return; }
    if (dt != D3D_DRIVER_TYPE_WARP || fl < D3D_FEATURE_LEVEL_11_0) { DONE(); t_skip("matrix measured for WARP at 11_x (got driver type %d, fl 0x%x)", dt, fl); return; }
    static const UINT binds[4] = { D3D11_BIND_SHADER_RESOURCE, D3D11_BIND_RENDER_TARGET, D3D11_BIND_DEPTH_STENCIL, D3D11_BIND_UNORDERED_ACCESS };
    for (int f = 1; f <= 132; f++) {
        unsigned wt = 0, wv = 0, tm = 0, vm = 0;
        for (unsigned i = 0; i < sizeof MATRIX / sizeof MATRIX[0]; i++) if (MATRIX[i].fmt == f) { wt = MATRIX[i].tex; wv = MATRIX[i].view; }
        for (int b = 0; b < 4; b++) {
            ID3D11Texture2D *t = mk2d(dev, 64, 64, 1, 1, (DXGI_FORMAT)f, binds[b], 0, 1);
            if (!t) continue;
            tm |= 1u << b;
            HRESULT hr = E_FAIL;
            if (b == 0) hr = TRY_SRV(dev, t, NULL);
            else if (b == 1) { ID3D11RenderTargetView *v = NULL; hr = MK_RTV(dev, t, NULL, &v); SAFE_RELEASE(v); }
            else if (b == 2) { ID3D11DepthStencilView *v = NULL; hr = MK_DSV(dev, t, NULL, &v); SAFE_RELEASE(v); }
            else { ID3D11UnorderedAccessView *v = NULL; hr = MK_UAV(dev, t, NULL, &v); SAFE_RELEASE(v); }
            if (SUCCEEDED(hr)) vm |= 1u << b;
            SAFE_RELEASE(t);
        }
        for (int b = 0; b < 4; b++) {
            CHECKF(((tm >> b) & 1) == ((wt >> b) & 1), "format %d: %s texture creatable=%u want %u", f, KIND[b], (tm >> b) & 1, (wt >> b) & 1);
            CHECKF(((vm >> b) & 1) == ((wv >> b) & 1), "format %d: default %s view creatable=%u want %u", f, KIND[b], (vm >> b) & 1, (wv >> b) & 1);
        }
    }
    DONE();
}
