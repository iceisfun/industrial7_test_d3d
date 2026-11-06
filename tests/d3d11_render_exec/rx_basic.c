/* Clear and basic draw paths on a WARP device with exact readback. */
#include "rx.h"

#define A(a) RX_N(a)

/* ------------------------------------------------------------ clears ---- */
TEST(d3d11_render_exec_clear_unorm_table)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { float c[4]; uint32_t want; } tab[] = {
        { { 0, 0, 0, 0 }, 0 }, { { 1, 1, 1, 1 }, 0xFFFFFFFFu },
        { { 1, 0, 0, 0 }, 0x000000FFu }, { { 0, 1, 0, 0 }, 0x0000FF00u },
        { { 0, 0, 1, 0 }, 0x00FF0000u }, { { 0, 0, 0, 1 }, 0xFF000000u },
        { { 64 / 255.f, 128 / 255.f, 192 / 255.f, 32 / 255.f }, RGBA8(64, 128, 192, 32) },
        { { 51 / 255.f, 102 / 255.f, 153 / 255.f, 204 / 255.f }, RGBA8(51, 102, 153, 204) },
        { { 1 / 255.f, 254 / 255.f, 127 / 255.f, 129 / 255.f }, RGBA8(1, 254, 127, 129) },
        { { 2, 2, 2, 2 }, 0xFFFFFFFFu }, { { -1, -1, -1, -1 }, 0 }, { { 100.f, -100.f, 1.0f, 0.0f }, RGBA8(255, 0, 255, 0) },
    };
    if (rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im;
            ID3D11DeviceContext_ClearRenderTargetView(r.ctx, r.rtv, tab[i].c);
            if (!rx_read_rt(&r, &im)) break;
            uint32_t got = d3dh_image_pixel32(&im, 0, 0);
            CHECKF(got == tab[i].want, "case %u got %08x want %08x", i, got, tab[i].want);
            rx_expect_all32(&im, tab[i].want);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_bgra_channel_order)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { float c[4]; uint32_t want; } tab[] = {
        { { 1, 0, 0, 0 }, 0x00FF0000u }, { { 0, 1, 0, 0 }, 0x0000FF00u }, { { 0, 0, 1, 0 }, 0x000000FFu },
        { { 0, 0, 0, 1 }, 0xFF000000u }, { { 16 / 255.f, 32 / 255.f, 48 / 255.f, 64 / 255.f }, (64u << 24) | (16u << 16) | (32u << 8) | 48u },
    };
    if (rx_target(&r, 3, 2, DXGI_FORMAT_B8G8R8A8_UNORM)) {
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im;
            ID3D11DeviceContext_ClearRenderTargetView(r.ctx, r.rtv, tab[i].c);
            if (!rx_read_rt(&r, &im)) break;
            rx_expect_all32(&im, tab[i].want);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_float32_exact)
{
    rx r; if (!rx_open(&r)) return;
    static const float c[4] = { 0.25f, -1.5f, 1.0e10f, 0.125f };
    if (rx_target(&r, 5, 3, DXGI_FORMAT_R32G32B32A32_FLOAT)) {
        d3dh_image im;
        ID3D11DeviceContext_ClearRenderTargetView(r.ctx, r.rtv, c);
        if (rx_read_rt(&r, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < im.height; y++) for (UINT x = 0; x < im.width; x++)
                for (UINT k = 0; k < 4; k++) if (rx_f32(&im, x, y, k) != c[k]) bad++;
            CHECKF(bad == 0, "%u mismatching components", bad);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_half_float)
{
    rx r; if (!rx_open(&r)) return;
    static const float c[4] = { 1.0f, 0.5f, 2.0f, -1.0f };
    static const uint16_t want[4] = { 0x3C00, 0x3800, 0x4000, 0xBC00 };
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        d3dh_image im;
        ID3D11DeviceContext_ClearRenderTargetView(r.ctx, r.rtv, c);
        if (rx_read_rt(&r, &im)) {
            const uint16_t *p = (const uint16_t *)im.data;
            for (int k = 0; k < 4; k++) CHECKF(p[k] == want[k], "comp %d got %04x want %04x", k, p[k], want[k]);
            CHECK(!memcmp(im.data, im.data + 8, 8));
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_r10g10b10a2)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { float c[4]; uint32_t want; } tab[] = {
        { { 1, 0, 0, 0 }, 0x000003FFu }, { { 0, 1, 0, 0 }, 0x000FFC00u }, { { 0, 0, 1, 0 }, 0x3FF00000u },
        { { 0, 0, 0, 1 }, 0xC0000000u }, { { 1, 1, 1, 1 }, 0xFFFFFFFFu }, { { 0, 0, 0, 1.0f / 3 }, 0x40000000u },
        { { 0, 0, 0, 2.0f / 3 }, 0x80000000u },
    };
    if (rx_target(&r, 2, 2, DXGI_FORMAT_R10G10B10A2_UNORM)) {
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im;
            ID3D11DeviceContext_ClearRenderTargetView(r.ctx, r.rtv, tab[i].c);
            if (!rx_read_rt(&r, &im)) break;
            rx_expect_all32(&im, tab[i].want);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_narrow_unorm_formats)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { DXGI_FORMAT f; float c[4]; unsigned bpp; uint64_t want; } tab[] = {
        { DXGI_FORMAT_R8_UNORM, { 1, 0, 0, 0 }, 1, 0xFF },
        { DXGI_FORMAT_R8_UNORM, { 0, 1, 1, 1 }, 1, 0x00 },
        { DXGI_FORMAT_R8G8_UNORM, { 1, 0, 1, 1 }, 2, 0x00FF },
        { DXGI_FORMAT_R8G8_UNORM, { 0, 1, 1, 1 }, 2, 0xFF00 },
        { DXGI_FORMAT_R16_UNORM, { 1, 0, 0, 0 }, 2, 0xFFFF },
        { DXGI_FORMAT_R16_UNORM, { 0.25f, 0, 0, 0 }, 2, 0x4000 },
        { DXGI_FORMAT_R16G16_UNORM, { 1, 0, 0, 0 }, 4, 0x0000FFFFu },
        { DXGI_FORMAT_R16G16_UNORM, { 0, 1, 0, 0 }, 4, 0xFFFF0000u },
        { DXGI_FORMAT_R16G16B16A16_UNORM, { 1, 0, 0, 1 }, 8, 0xFFFF00000000FFFFull },
    };
    for (unsigned i = 0; i < A(tab); i++) {
        if (!rx_target(&r, 3, 3, tab[i].f)) break;
        d3dh_image im;
        ID3D11DeviceContext_ClearRenderTargetView(r.ctx, r.rtv, tab[i].c);
        if (!rx_read_rt(&r, &im)) break;
        unsigned bad = 0;
        for (UINT p = 0; p < 9; p++) {
            uint64_t v = 0; memcpy(&v, im.data + (size_t)(p / 3) * im.pitch + (size_t)(p % 3) * tab[i].bpp, tab[i].bpp);
            if (v != tab[i].want) bad++;
        }
        CHECKF(bad == 0, "format %d case %u: %u pixels differ from %llx", (int)tab[i].f, i, bad, (unsigned long long)tab[i].want);
        d3dh_image_free(&im);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_view_rects)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11DeviceContext1 *c1 = NULL;
    if (FAILED(D3DH_QI(r.ctx, IID_ID3D11DeviceContext1, &c1)) || !c1) { t_skip("ID3D11DeviceContext1 not available"); rx_close(&r); return; }
    if (rx_target(&r, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        static const float blue[4] = { 0, 0, 1, 1 };
        D3D11_RECT rc[2] = { { 1, 1, 3, 2 }, { 5, 4, 8, 8 } };
        ID3D11DeviceContext1_ClearView(c1, (ID3D11View *)r.rtv, blue, rc, 2);
        if (rx_read_rt(&r, &im)) {
            unsigned lit = 0, bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
                int in = 0;
                for (int k = 0; k < 2; k++) if ((LONG)x >= rc[k].left && (LONG)x < rc[k].right && (LONG)y >= rc[k].top && (LONG)y < rc[k].bottom) in = 1;
                uint32_t p = d3dh_image_pixel32(&im, x, y);
                if (p != (in ? RGBA8(0, 0, 255, 255) : 0u)) bad++;
                lit += in;
            }
            CHECKF(bad == 0, "%u pixels wrong (%u expected lit)", bad, lit);
            d3dh_image_free(&im);
        }
        /* NULL rect list clears the whole view */
        ID3D11DeviceContext1_ClearView(c1, (ID3D11View *)r.rtv, RX_GREEN, NULL, 0);
        if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, RGBA8(0, 255, 0, 255)); d3dh_image_free(&im); }
    }
    ID3D11DeviceContext1_Release(c1);
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_depth_value_clamp)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { float in; uint32_t bits; } tab[] = {
        { 0.0f, 0x00000000u }, { 1.0f, 0x3F800000u }, { 0.25f, 0x3E800000u }, { 0.5f, 0x3F000000u },
        { 2.0f, 0x3F000000u }, { -1.0f, 0x3F000000u }, { 0.75f, 0x3F400000u },
    };
    /* Measured on Win10 22H2 (WARP): a ClearDepthStencilView with a depth outside [0,1] is DROPPED, not clamped; the target keeps the previous
     * clear value. In this table the entries 2.0 and -1.0 therefore expect the preceding value 0.5 (0x3F000000). */
    if (rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im; uint32_t got;
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, tab[i].in, 0);
            if (!rx_read(&r, r.ds, 0, &im)) break;
            memcpy(&got, im.data, 4);
            CHECKF(got == tab[i].bits, "clear depth %g: got %08x want %08x (out-of-range clears are ignored)", tab[i].in, got, tab[i].bits);
            rx_expect_all32(&im, tab[i].bits);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_depth_d16_values)
{
    rx r; if (!rx_open(&r)) return;
    /* out-of-range depth clears are ignored (see clear_depth_value_clamp): 2.0 keeps 1.0 (0xFFFF), -3.0 keeps it too */
    static const struct { float in; uint16_t want; } tab[] = { { 0.0f, 0 }, { 1.0f, 0xFFFF }, { 2.0f, 0xFFFF }, { -3.0f, 0xFFFF }, { 0.0f, 0 }, { 1.5f, 0 } };
    if (rx_target(&r, 4, 2, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D16_UNORM, 0)) {
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im;
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, tab[i].in, 0);
            if (!rx_read(&r, r.ds, 0, &im)) break;
            const uint16_t *p = (const uint16_t *)im.data; unsigned bad = 0;
            for (UINT y = 0; y < im.height; y++) for (UINT x = 0; x < im.width; x++) if (((const uint16_t *)(im.data + y * im.pitch))[x] != tab[i].want) bad++;
            CHECKF(bad == 0, "D16 clear %g: %u texels differ from %04x (first %04x)", tab[i].in, bad, tab[i].want, p[0]);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

/* ---------------------------------------------------- flat triangles ---- */
static const char VS_FS_TRI[] = HL(
    float4 main(uint id : SV_VertexID) : SV_Position {
        float2 uv = float2((id << 1) & 2, id & 2);
        return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    });

TEST(d3d11_render_exec_fullscreen_triangle_sizes)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs = rx_vs(&r, VS_FS_TRI);
    static const UINT sz[][2] = { { 1, 1 }, { 1, 7 }, { 3, 5 }, { 16, 16 }, { 33, 17 }, { 64, 1 }, { 1, 64 }, { 127, 63 }, { 256, 256 } };
    if (vs) {
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        for (unsigned i = 0; i < A(sz); i++) {
            d3dh_image im;
            if (!rx_target(&r, sz[i][0], sz[i][1], DXGI_FORMAT_R8G8B8A8_UNORM)) break;
            ID3D11DeviceContext_Draw(r.ctx, 3, 0);
            if (!rx_read_rt(&r, &im)) break;
            unsigned n = rx_count32(&im, 0xFFFFFFFFu);
            CHECKF(n == sz[i][0] * sz[i][1], "%ux%u: %u pixels covered", sz[i][0], sz[i][1], n);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_flat_colour_psc)
{
    rx r; if (!rx_open(&r)) return;
    static const float cols[][4] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 0, 0, 0, 0 }, { 64 / 255.f, 0, 0, 1 }, { 1, 1, 1, 0 } };
    static const uint32_t want[] = { RGBA8(255, 0, 0, 255), RGBA8(0, 255, 0, 255), RGBA8(0, 0, 255, 255), 0, RGBA8(64, 0, 0, 255), RGBA8(255, 255, 255, 0) };
    if (rx_target(&r, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        for (unsigned i = 0; i < A(cols); i++) {
            d3dh_image im;
            rx_clear(&r, 0, 0, 0, 0);
            rx_full(&r, 0, cols[i]);
            if (!rx_read_rt(&r, &im)) break;
            rx_expect_all32(&im, want[i]);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_shared_edge_covered_once)
{
    /* additive blend of 1/255: every pixel of the two-triangle rectangle (diagonal included) is shaded exactly once */
    rx r; if (!rx_open(&r)) return;
    D3D11_BLEND_DESC bd; rx_blend_default(&bd);
    bd.RenderTarget[0].BlendEnable = TRUE; bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    static const UINT sz[][2] = { { 16, 16 }, { 17, 13 }, { 31, 5 }, { 64, 64 } };
    static const float one[4] = { 1 / 255.f, 1 / 255.f, 1 / 255.f, 1 / 255.f };
    for (unsigned i = 0; i < A(sz); i++) {
        d3dh_image im;
        if (!rx_target(&r, sz[i][0], sz[i][1], DXGI_FORMAT_R8G8B8A8_UNORM)) break;
        rx_set_bs(&r, &bd, RX_ZERO, 0xFFFFFFFFu);
        rx_full(&r, 0, one);
        if (!rx_read_rt(&r, &im)) break;
        rx_expect_all32(&im, 0x01010101u);
        d3dh_image_free(&im);
        rx_full(&r, 0, one);
        if (!rx_read_rt(&r, &im)) break;
        rx_expect_all32(&im, 0x02020202u);
        d3dh_image_free(&im);
    }
    rx_close(&r);
}

/* ------------------------------------------------- vertex attributes ---- */
static const char VS_GRAD[] = HL(
    struct O { float4 p : SV_Position; float4 c : COLOR; };
    O main(uint id : SV_VertexID) {
        O o;
        o.p = float4((id & 1) ? 1 : -1, (id & 2) ? -1 : 1, 0, 1);
        o.c = float4((id & 1) ? 1 : 0, 0, (id & 2) ? 1 : 0, 1);
        return o;
    });
static const char PS_COL[] = HL(
    float4 main(float4 p : SV_Position, float4 c : COLOR) : SV_Target { return c; });

TEST(d3d11_render_exec_vertex_colour_gradient)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs = rx_vs(&r, VS_GRAD); ID3D11PixelShader *ps = rx_ps(&r, PS_COL);
    if (vs && ps && rx_target(&r, 64, 32, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);
        if (rx_read_rt(&r, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 32; y++) for (UINT x = 0; x < 64; x++) {
                uint32_t p = d3dh_image_pixel32(&im, x, y);
                int er = (int)((x + 0.5f) / 64 * 255 + 0.5f), eb = (int)((y + 0.5f) / 32 * 255 + 0.5f);
                int gr = p & 255, gb = (p >> 16) & 255;
                if (abs(gr - er) > 2 || abs(gb - eb) > 2 || ((p >> 8) & 255) != 0 || (p >> 24) != 255) bad++;
            }
            CHECKF(bad == 0, "%u pixels outside +-2 of the linear gradient", bad);
            /* monotonic along x and y */
            unsigned nonmono = 0;
            for (UINT x = 1; x < 64; x++) if ((d3dh_image_pixel32(&im, x, 5) & 255) < (d3dh_image_pixel32(&im, x - 1, 5) & 255)) nonmono++;
            CHECK(nonmono == 0);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_nointerpolation_provoking_vertex)
{
    rx r; if (!rx_open(&r)) return;
    static const char vs_src[] = HL(
        struct O { float4 p : SV_Position; nointerpolation float4 c : COLOR; };
        O main(uint id : SV_VertexID) {
            O o; float2 uv = float2((id << 1) & 2, id & 2);
            o.p = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
            o.c = float4(id == 0, id == 1, id == 2, 1);
            return o;
        });
    static const char ps_src[] = HL(float4 main(float4 p : SV_Position, nointerpolation float4 c : COLOR) : SV_Target { return c; });
    ID3D11VertexShader *vs = rx_vs(&r, vs_src); ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (vs && ps && rx_target(&r, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 3, 0);
        if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, RGBA8(255, 0, 0, 255)); d3dh_image_free(&im); }
    }
    SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    rx_close(&r);
}

/* ---------------------------------------- point-list fetch patterns ---- */
static const char VS_PT[] = HL(float4 main(float2 p : POSITION) : SV_Position { return float4(p, 0, 1); });
static const char VS_PT_INST[] = HL(
    float4 main(float2 p : POSITION, float2 o : OFFSET) : SV_Position { return float4(p + o * float2(0.25, -0.5), 0, 1); });

static unsigned rx_rowmask(rx *r)
{
    d3dh_image im; unsigned m = 0;
    if (!rx_read_rt(r, &im)) return 0xFFFFFFFFu;
    for (UINT x = 0; x < im.width && x < 32; x++) if (d3dh_image_pixel32(&im, x, 0)) m |= 1u << x;
    d3dh_image_free(&im);
    return m;
}
static void fill_points16(float *v, unsigned stride_floats, unsigned pos_off)
{
    for (unsigned k = 0; k < 16; k++) {
        for (unsigned j = 0; j < stride_floats; j++) v[k * stride_floats + j] = -777.0f;
        v[k * stride_floats + pos_off] = (k + 0.5f) / 16 * 2 - 1;
        v[k * stride_floats + pos_off + 1] = 0.0f;
    }
}

TEST(d3d11_render_exec_vb_offset_stride_start)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el0[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    static const D3D11_INPUT_ELEMENT_DESC el8[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *il0 = NULL, *il8 = NULL, *dummy = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, VS_PT, el0, 1, &il0);
    float tight[32], wide[64];
    fill_points16(tight, 2, 0); fill_points16(wide, 4, 2);
    ID3D11Buffer *vb0 = rx_buffer(&r, sizeof tight, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, tight);
    ID3D11Buffer *vb1 = rx_buffer(&r, sizeof wide, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, wide);
    if (vs) { ID3DBlob *b = rx_blob(VS_PT, "vs_5_0"); if (b) { EXPECT_HR_OK(ID3D11Device_CreateInputLayout(r.dev, el8, 1, d3dh_blob_ptr(b), d3dh_blob_size(b), &il8)); SAFE_RELEASE(b); } }
    (void)dummy;
    if (vs && il0 && il8 && vb0 && vb1 && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        static const struct { int wide; UINT off; UINT n, start; unsigned want; } tab[] = {
            { 0, 0, 4, 0, 0x000Fu }, { 0, 0, 4, 5, 0x01E0u }, { 0, 24, 4, 0, 0x0078u }, { 0, 8, 2, 4, 0x0060u },
            { 1, 0, 3, 2, 0x001Cu }, { 1, 16, 2, 0, 0x0006u }, { 1, 32, 2, 3, 0x0060u }, { 0, 0, 16, 0, 0xFFFFu }, { 0, 0, 0, 0, 0u },
            { 0, 0, 1, 15, 0x8000u },
        };
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        for (unsigned i = 0; i < A(tab); i++) {
            UINT stride = tab[i].wide ? 16 : 8, off = tab[i].off;
            ID3D11Buffer *vb = tab[i].wide ? vb1 : vb0;
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_IASetInputLayout(r.ctx, tab[i].wide ? il8 : il0);
            ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &vb, &stride, &off);
            ID3D11DeviceContext_Draw(r.ctx, tab[i].n, tab[i].start);
            unsigned m = rx_rowmask(&r);
            CHECKF(m == tab[i].want, "case %u: mask %04x want %04x", i, m, tab[i].want);
        }
    }
    SAFE_RELEASE(vb0); SAFE_RELEASE(vb1); SAFE_RELEASE(il0); SAFE_RELEASE(il8); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_drawindexed_start_base_offset)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el0[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, VS_PT, el0, 1, &il);
    float v[32]; fill_points16(v, 2, 0);
    static const uint16_t i16[6] = { 2, 4, 6, 8, 10, 12 };
    static const uint32_t i32[6] = { 2, 4, 6, 8, 10, 12 };
    static const uint16_t dup[3] = { 5, 5, 5 };
    ID3D11Buffer *vb = rx_buffer(&r, sizeof v, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, v);
    ID3D11Buffer *ib16 = rx_buffer(&r, sizeof i16, D3D11_BIND_INDEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, i16);
    ID3D11Buffer *ib32 = rx_buffer(&r, sizeof i32, D3D11_BIND_INDEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, i32);
    ID3D11Buffer *ibd = rx_buffer(&r, sizeof dup, D3D11_BIND_INDEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, dup);
    if (vs && il && vb && ib16 && ib32 && ibd && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        static const struct { int which; UINT ibofs; UINT n, start; INT base; unsigned want; } tab[] = {
            { 0, 0, 3, 0, 0, 0x0054u },        /* 2,4,6 */
            { 0, 0, 3, 2, 0, 0x0540u },        /* 6,8,10 */
            { 0, 0, 3, 0, 1, 0x00A8u },        /* 3,5,7 */
            { 0, 0, 2, 1, -2, 0x0014u },       /* 2,4 */
            { 0, 4, 2, 0, 0, 0x0140u },        /* ib offset 2 indices: 6,8 */
            { 1, 0, 3, 0, 0, 0x0054u },        /* 32-bit */
            { 1, 8, 2, 1, 1, 0x0A00u },        /* ib offset 2 indices + start 1: indices 8,10, +1 = 9,11 */
            { 2, 0, 3, 0, 0, 0x0020u },        /* duplicate index */
            { 0, 0, 6, 0, 3, 0xAAA0u },        /* 5,7,9,11,13,15 */
            { 0, 0, 0, 0, 0, 0u },
        };
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        UINT stride = 8, off = 0;
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &vb, &stride, &off);
        for (unsigned i = 0; i < A(tab); i++) {
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_IASetIndexBuffer(r.ctx, tab[i].which == 0 ? ib16 : tab[i].which == 1 ? ib32 : ibd,
                tab[i].which == 1 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT, tab[i].ibofs);
            ID3D11DeviceContext_DrawIndexed(r.ctx, tab[i].n, tab[i].start, tab[i].base);
            unsigned m = rx_rowmask(&r);
            unsigned want = tab[i].want;
            CHECKF(m == want, "case %u: mask %04x want %04x", i, m, want);
        }
    }
    SAFE_RELEASE(vb); SAFE_RELEASE(ib16); SAFE_RELEASE(ib32); SAFE_RELEASE(ibd); SAFE_RELEASE(il); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_index_cut_restarts_line_strip)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el0[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, VS_PT, el0, 1, &il);
    float v[32]; fill_points16(v, 2, 0);
    static const uint16_t a16[5] = { 2, 6, 0xFFFF, 9, 13 };
    static const uint32_t a32[5] = { 2, 6, 0xFFFFFFFFu, 9, 13 };
    ID3D11Buffer *vb = rx_buffer(&r, sizeof v, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, v);
    ID3D11Buffer *ib16 = rx_buffer(&r, sizeof a16, D3D11_BIND_INDEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, a16);
    ID3D11Buffer *ib32 = rx_buffer(&r, sizeof a32, D3D11_BIND_INDEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, a32);
    if (vs && il && vb && ib16 && ib32 && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        UINT stride = 8, off = 0;
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &vb, &stride, &off);
        rx_set_color(&r, RX_WHITE);
        for (int w = 0; w < 2; w++) {
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_IASetIndexBuffer(r.ctx, w ? ib32 : ib16, w ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT, 0);
            ID3D11DeviceContext_DrawIndexed(r.ctx, 5, 0, 0);
            unsigned m = rx_rowmask(&r);
            CHECKF((m & 0x01C0u) == 0, "%d-bit: pixels 6..8 lit, so the cut index did not restart the strip (mask %04x)", w ? 32 : 16, m);
            CHECKF((m & 0x0038u) == 0x0038u, "%d-bit: first segment interior missing (mask %04x)", w ? 32 : 16, m);
            CHECKF((m & 0x1C00u) == 0x1C00u, "%d-bit: second segment interior missing (mask %04x)", w ? 32 : 16, m);
            CHECKF((m & ~0x3FFFu) == 0, "mask %04x has pixels outside the segments", m);
            t_note("line strip with cut (%d-bit) lit mask %04x", w ? 32 : 16, m);
        }
    }
    SAFE_RELEASE(vb); SAFE_RELEASE(ib16); SAFE_RELEASE(ib32); SAFE_RELEASE(il); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_line_list_horizontal_segments)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el0[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, VS_PT, el0, 1, &il);
    float v[32]; fill_points16(v, 2, 0);
    ID3D11Buffer *vb = rx_buffer(&r, sizeof v, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, v);
    if (vs && il && vb && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        UINT stride = 8, off = 0;
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &vb, &stride, &off);
        rx_set_color(&r, RX_WHITE);
        /* list: (1,5) (8,12); a dangling 5th vertex draws nothing */
        ID3D11DeviceContext_Draw(r.ctx, 4, 1);  /* vertices 1,2,3,4 -> lines 1-2 and 3-4 */
        unsigned m = rx_rowmask(&r);
        CHECKF((m & ~0x001Eu) == 0, "pixels outside the two segments: %04x", m);
        CHECKF((m & 0x0002u) != 0 || (m & 0x0004u) != 0, "line 1-2 not drawn: %04x", m);
        CHECKF((m & 0x0008u) != 0 || (m & 0x0010u) != 0 || (m & 0x0004u) != 0, "line 3-4 not drawn: %04x", m);
        t_note("line list mask %04x", m);
        rx_clear(&r, 0, 0, 0, 0);
        ID3D11DeviceContext_Draw(r.ctx, 3, 0);  /* odd count: one complete line (0-1), dangling vertex ignored */
        m = rx_rowmask(&r);
        CHECKF((m & ~0x0003u) == 0, "dangling vertex produced pixels: %04x", m);
        CHECKF(m != 0, "complete line 0-1 not drawn");
        rx_clear(&r, 0, 0, 0, 0);
        ID3D11DeviceContext_Draw(r.ctx, 1, 0);  /* fewer vertices than a primitive: nothing */
        CHECK(rx_rowmask(&r) == 0);
    }
    SAFE_RELEASE(vb); SAFE_RELEASE(il); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_instance_data_step_rates)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "OFFSET", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 } };
    static const D3D11_INPUT_ELEMENT_DESC el2[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "OFFSET", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 2 } };
    ID3D11InputLayout *il = NULL, *il2 = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, VS_PT_INST, el, 2, &il);
    if (vs) { ID3DBlob *b = rx_blob(VS_PT_INST, "vs_5_0"); if (b) { EXPECT_HR_OK(ID3D11Device_CreateInputLayout(r.dev, el2, 2, d3dh_blob_ptr(b), d3dh_blob_size(b), &il2)); SAFE_RELEASE(b); } }
    /* base vertex = centre of pixel (0,0) of an 8x4 target. offset unit = 1 pixel: x 2/8 = 0.25, y 2/4 = 0.5 (flipped) */
    static const float base[2] = { -0.875f, 0.75f };
    static const float inst[8] = { 1, 0, 3, 1, 5, 2, 7, 3 };
    ID3D11Buffer *vb0 = rx_buffer(&r, sizeof base, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, base);
    ID3D11Buffer *vb1 = rx_buffer(&r, sizeof inst, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, inst);
    if (vs && il && il2 && vb0 && vb1 && rx_target(&r, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        ID3D11Buffer *vbs[2] = { vb0, vb1 }; UINT strides[2] = { 8, 8 }, offs[2] = { 0, 0 };
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 2, vbs, strides, offs);
        rx_set_color(&r, RX_WHITE);
        static const struct { int stepping2; UINT ninst, startinst; unsigned n; int px[4][2]; } tab[] = {
            { 0, 4, 0, 4, { { 1, 0 }, { 3, 1 }, { 5, 2 }, { 7, 3 } } },
            { 0, 3, 1, 3, { { 3, 1 }, { 5, 2 }, { 7, 3 } } },
            { 0, 1, 2, 1, { { 5, 2 } } },
            { 1, 4, 0, 2, { { 1, 0 }, { 3, 1 } } },
            { 1, 2, 0, 1, { { 1, 0 } } },
        };
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im;
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_IASetInputLayout(r.ctx, tab[i].stepping2 ? il2 : il);
            ID3D11DeviceContext_DrawInstanced(r.ctx, 1, tab[i].ninst, 0, tab[i].startinst);
            if (!rx_read_rt(&r, &im)) break;
            unsigned lit = 0, ok = 0;
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 8; x++) if (d3dh_image_pixel32(&im, x, y)) lit++;
            for (unsigned k = 0; k < tab[i].n; k++) if (d3dh_image_pixel32(&im, tab[i].px[k][0], tab[i].px[k][1])) ok++;
            CHECKF(lit == tab[i].n && ok == tab[i].n, "case %u: %u lit, %u of %u expected pixels", i, lit, ok, tab[i].n);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(vb0); SAFE_RELEASE(vb1); SAFE_RELEASE(il); SAFE_RELEASE(il2); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_vertex_and_instance_id_observed)
{
    /* SV_VertexID / SV_InstanceID base conventions differ between hardware and documentation; record them */
    rx r; if (!rx_open(&r)) return;
    static const char vs_src[] = HL(
        float4 main(uint v : SV_VertexID, uint i : SV_InstanceID) : SV_Position {
            return float4((v + 0.5) / 16 * 2 - 1, 0.75 - 0.5 * i, 0, 1); });
    ID3D11VertexShader *vs = rx_vs(&r, vs_src);
    if (vs && rx_target(&r, 16, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        ID3D11DeviceContext_DrawInstanced(r.ctx, 4, 2, 3, 1);
        if (rx_read_rt(&r, &im)) {
            unsigned rows[4] = { 0, 0, 0, 0 };
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 16; x++) if (d3dh_image_pixel32(&im, x, y)) rows[y] |= 1u << x;
            t_note("DrawInstanced(4,2,start 3,startinst 1): row masks %04x %04x %04x %04x", rows[0], rows[1], rows[2], rows[3]);
            unsigned lit = 0; for (int k = 0; k < 4; k++) for (int b = 0; b < 16; b++) lit += (rows[k] >> b) & 1;
            CHECKF(lit == 8, "8 points expected, %u pixels lit", lit);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(vs);
    rx_close(&r);
}

/* ------------------------------------------------ rasterizer state ---- */
TEST(d3d11_render_exec_cull_mode_winding_table)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        for (int cull = D3D11_CULL_NONE; cull <= D3D11_CULL_BACK; cull++)
            for (int ccw = 0; ccw < 2; ccw++)
                for (int rev = 0; rev < 2; rev++) {
                    d3dh_image im;
                    /* screen-clockwise unless reversed */
                    int is_front = rev ? ccw : !ccw;
                    int want = (cull == D3D11_CULL_NONE) || (is_front ? cull == D3D11_CULL_BACK : cull == D3D11_CULL_FRONT);
                    rx_clear(&r, 0, 0, 0, 0);
                    rx_set_rs(&r, D3D11_FILL_SOLID, (D3D11_CULL_MODE)cull, ccw, TRUE, FALSE);
                    if (rev) rx_rect(&r, 14, 2, 2, 14, 0, RX_WHITE); else rx_rect(&r, 2, 2, 14, 14, 0, RX_WHITE);
                    if (!rx_read_rt(&r, &im)) continue;
                    int a = d3dh_image_pixel32(&im, 8, 4) != 0, b = d3dh_image_pixel32(&im, 8, 12) != 0;
                    CHECKF(a == want && b == want, "cull %d ccw %d reversed %d: tri0 %d tri1 %d want %d", cull, ccw, rev, a, b, want);
                    d3dh_image_free(&im);
                }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_sv_isfrontface)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        float4 main(float4 p : SV_Position, bool f : SV_IsFrontFace) : SV_Target { return f ? float4(1, 0, 0, 1) : float4(0, 1, 0, 1); });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (ps && rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        for (int ccw = 0; ccw < 2; ccw++) for (int rev = 0; rev < 2; rev++) {
            d3dh_image im;
            int is_front = rev ? ccw : !ccw;
            rx_clear(&r, 0, 0, 0, 0);
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, ccw, TRUE, FALSE);
            if (rev) rx_rect(&r, 14, 2, 2, 14, 0, RX_WHITE); else rx_rect(&r, 2, 2, 14, 14, 0, RX_WHITE);
            ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
            ID3D11DeviceContext_Draw(r.ctx, 4, 0);
            if (!rx_read_rt(&r, &im)) continue;
            uint32_t want = is_front ? RGBA8(255, 0, 0, 255) : RGBA8(0, 255, 0, 255);
            CHECKF(d3dh_image_pixel32(&im, 8, 4) == want && d3dh_image_pixel32(&im, 8, 12) == want,
                   "ccw %d rev %d: got %08x/%08x want %08x", ccw, rev, d3dh_image_pixel32(&im, 8, 4), d3dh_image_pixel32(&im, 8, 12), want);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_wireframe_leaves_interior)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image solid, wire;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_rect(&r, 1.5f, 1.5f, 14.5f, 14.5f, 0, RX_WHITE);
        if (rx_read_rt(&r, &solid)) {
            rx_clear(&r, 0, 0, 0, 0);
            rx_set_rs(&r, D3D11_FILL_WIREFRAME, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
            rx_rect(&r, 1.5f, 1.5f, 14.5f, 14.5f, 0, RX_WHITE);
            if (rx_read_rt(&r, &wire)) {
                unsigned ns = rx_count32(&solid, 0xFFFFFFFFu), nw = rx_count32(&wire, 0xFFFFFFFFu);
                CHECKF(ns >= 12 * 12, "solid count %u", ns);
                CHECKF(nw > 0 && nw < ns / 2, "wireframe count %u vs solid %u", nw, ns);
                CHECK(d3dh_image_pixel32(&wire, 8, 1) != 0);       /* top edge */
                CHECK(d3dh_image_pixel32(&wire, 1, 8) != 0);       /* left edge */
                CHECK(d3dh_image_pixel32(&wire, 4, 8) == 0);       /* interior off the diagonal */
                CHECK(d3dh_image_pixel32(&wire, 12, 5) == 0);
                CHECK(d3dh_image_pixel32(&solid, 4, 8) != 0);
                d3dh_image_free(&wire);
            }
            d3dh_image_free(&solid);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_scissor_rect_cases)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        static const struct { int enable; D3D11_RECT rc; unsigned want; } tab[] = {
            { 1, { 3, 2, 11, 9 }, 8 * 7 }, { 1, { 0, 0, 16, 16 }, 256 }, { 1, { 5, 5, 6, 6 }, 1 },
            { 1, { 4, 4, 4, 9 }, 0 }, { 1, { 0, 0, 100, 100 }, 256 }, { 0, { 3, 2, 11, 9 }, 256 },
            { 1, { 15, 0, 16, 16 }, 16 }, { 1, { 0, 15, 16, 16 }, 16 },
        };
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im; D3D11_RECT rc = tab[i].rc;
            rx_clear(&r, 0, 0, 0, 0);
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, tab[i].enable);
            ID3D11DeviceContext_RSSetScissorRects(r.ctx, 1, &rc);
            rx_full(&r, 0, RX_WHITE);
            if (!rx_read_rt(&r, &im)) break;
            unsigned n = rx_count32(&im, 0xFFFFFFFFu), inside = 0;
            if (tab[i].enable) for (UINT y = 0; y < 16; y++) for (UINT x = 0; x < 16; x++)
                if ((LONG)x >= rc.left && (LONG)x < rc.right && (LONG)y >= rc.top && (LONG)y < rc.bottom && d3dh_image_pixel32(&im, x, y)) inside++;
            CHECKF(n == tab[i].want, "case %u: %u pixels, want %u", i, n, tab[i].want);
            if (tab[i].enable) CHECKF(inside == n, "case %u: %u pixels lit outside the scissor", i, n - inside);
            d3dh_image_free(&im);
        }
        /* RSGetScissorRects roundtrip */
        D3D11_RECT in = { 1, 2, 3, 4 }, out[2]; UINT n = 1;
        ID3D11DeviceContext_RSSetScissorRects(r.ctx, 1, &in);
        memset(out, 0xCC, sizeof out);
        ID3D11DeviceContext_RSGetScissorRects(r.ctx, &n, out);
        CHECK(n == 1 || n == 16 || n == 0);
        CHECK(out[0].left == 1 && out[0].top == 2 && out[0].right == 3 && out[0].bottom == 4);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_viewport_subrect_clips)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        static const float vp[][4] = { { 4, 4, 8, 8 }, { 0, 0, 16, 16 }, { 8, 0, 8, 16 }, { 0, 8, 16, 8 }, { 3, 5, 1, 1 }, { 2, 2, 12, 3 } };
        for (unsigned i = 0; i < A(vp); i++) {
            d3dh_image im; unsigned bad = 0, lit = 0;
            rx_clear(&r, 0, 0, 0, 0);
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
            rx_viewport(&r, vp[i][0], vp[i][1], vp[i][2], vp[i][3], 0, 1);
            rx_rect(&r, 0, 0, 16, 16, 0, RX_WHITE);  /* NDC [-1,1]^2: exactly the viewport */
            rx_viewport(&r, 0, 0, 16, 16, 0, 1);
            if (!rx_read_rt(&r, &im)) break;
            for (UINT y = 0; y < 16; y++) for (UINT x = 0; x < 16; x++) {
                int in = x >= vp[i][0] && x < vp[i][0] + vp[i][2] && y >= vp[i][1] && y < vp[i][1] + vp[i][3];
                int p = d3dh_image_pixel32(&im, x, y) != 0;
                lit += p; if (p != in) bad++;
            }
            CHECKF(bad == 0, "viewport %g,%g %gx%g: %u wrong pixels (%u lit)", vp[i][0], vp[i][1], vp[i][2], vp[i][3], bad, lit);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_viewport_depth_range_maps_z)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        static const struct { float zmin, zmax, z, want; } tab[] = {
            { 0, 1, 0.5f, 0.5f }, { 0.25f, 0.75f, 1.0f, 0.75f }, { 0.25f, 0.75f, 0.0f, 0.25f }, { 0.25f, 0.75f, 0.5f, 0.5f },
            { 0.5f, 0.5f, 0.75f, 0.5f }, { 1.0f, 0.0f, 0.25f, 0.5f }, { 0.0f, 0.5f, 1.0f, 0.5f },
        };
        /* case 5 (MinDepth 1.0 > MaxDepth 0.0) is rejected by the runtime: the previous viewport (0.5..0.5) stays bound, so the depth is 0.5, not the mirrored 0.75 */
        D3D11_DEPTH_STENCIL_DESC dd; rx_dss_default(&dd); dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        rx_set_dss(&r, &dd, 0);
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im;
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
            rx_viewport(&r, 0, 0, 4, 4, tab[i].zmin, tab[i].zmax);
            rx_full(&r, tab[i].z, RX_WHITE);
            if (!rx_read(&r, r.ds, 0, &im)) break;
            float d = rx_depth_at(&im, 1, 1);
            CHECKF(rx_near(d, tab[i].want, 1e-6f) && rx_near(rx_depth_at(&im, 3, 2), tab[i].want, 1e-6f),
                   "case %u: depth %.9g want %.9g", i, d, tab[i].want);
            d3dh_image_free(&im);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_depth_clip_enable)
{
    rx r; if (!rx_open(&r)) return;
    if (rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) && rx_depth(&r, DXGI_FORMAT_D32_FLOAT, 0)) {
        D3D11_DEPTH_STENCIL_DESC dd; rx_dss_default(&dd); dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        rx_set_dss(&r, &dd, 0);
        static const float zs[] = { -0.5f, 1.5f, 0.5f };
        for (int clip = 0; clip < 2; clip++) for (unsigned i = 0; i < A(zs); i++) {
            d3dh_image im, dm;
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 0.5f, 0);
            rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, clip, FALSE);
            rx_full(&r, zs[i], RX_WHITE);
            if (!rx_read_rt(&r, &im)) break;
            if (!rx_read(&r, r.ds, 0, &dm)) { d3dh_image_free(&im); break; }
            int outside = zs[i] < 0 || zs[i] > 1;
            unsigned n = rx_count32(&im, 0xFFFFFFFFu);
            unsigned want_n = (outside && clip) ? 0 : 16;
            float wd = (outside && clip) ? 0.5f : (zs[i] < 0 ? 0.0f : zs[i] > 1 ? 1.0f : zs[i]);
            CHECKF(n == want_n, "clip %d z %g: %u pixels want %u", clip, zs[i], n, want_n);
            CHECKF(rx_near(rx_depth_at(&dm, 2, 2), wd, 1e-6f), "clip %d z %g: depth %g want %g", clip, zs[i], rx_depth_at(&dm, 2, 2), wd);
            d3dh_image_free(&im); d3dh_image_free(&dm);
        }
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_depth_bias_direction)
{
    rx r; if (!rx_open(&r)) return;
    static const DXGI_FORMAT fmts[] = { DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D16_UNORM };
    for (unsigned f = 0; f < 2; f++) {
        if (!rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM) || !rx_depth(&r, fmts[f], 0)) break;
        D3D11_DEPTH_STENCIL_DESC dd; rx_dss_default(&dd); dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        rx_set_dss(&r, &dd, 0);
        float depth[3]; static const INT bias[3] = { 0, 100, -100 };
        for (int i = 0; i < 3; i++) {
            D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd);
            rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE; rd.DepthBias = bias[i];
            rx_set_rs_desc(&r, &rd);
            d3dh_image dm;
            ID3D11DeviceContext_ClearDepthStencilView(r.ctx, r.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
            rx_full(&r, 0.5f, RX_WHITE);
            if (!rx_read(&r, r.ds, 0, &dm)) { depth[i] = -1; continue; }
            if (fmts[f] == DXGI_FORMAT_D32_FLOAT) depth[i] = rx_depth_at(&dm, 1, 1);
            else depth[i] = ((const uint16_t *)dm.data)[1 * (dm.pitch / 2) + 1] / 65535.0f;
            d3dh_image_free(&dm);
        }
        CHECKF(depth[1] > depth[0], "fmt %d: positive bias did not increase depth (%g -> %g)", (int)fmts[f], depth[0], depth[1]);
        CHECKF(depth[2] < depth[0], "fmt %d: negative bias did not decrease depth (%g -> %g)", (int)fmts[f], depth[0], depth[2]);
        CHECKF(rx_near(depth[0], 0.5f, 1.0f / 65535), "unbiased depth %g", depth[0]);
        t_note("depth bias 100 on fmt %d moved depth by %g", (int)fmts[f], depth[1] - depth[0]);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_sv_primitiveid_in_ps)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        float4 main(float4 p : SV_Position, uint id : SV_PrimitiveID) : SV_Target { return float4(id == 0, id == 1, id >= 2, 1); });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (ps && rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        rx_full(&r, 0, RX_WHITE);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);
        if (rx_read_rt(&r, &im)) {
            CHECK(d3dh_image_pixel32(&im, 3, 3) == RGBA8(255, 0, 0, 255));
            CHECK(d3dh_image_pixel32(&im, 12, 12) == RGBA8(0, 255, 0, 255));
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_triangle_list_two_triangles_disjoint)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el0[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, VS_PT, el0, 1, &il);
    /* two pixel-aligned right triangles on an 8x8 target, each clockwise on screen */
    static const float px[6][2] = { { 0, 0 }, { 4.5f, 0 }, { 0, 4.5f }, { 4, 4 }, { 8.5f, 4 }, { 4, 8.5f } };
    float v[12];
    for (int i = 0; i < 6; i++) { v[i * 2] = px[i][0] / 8 * 2 - 1; v[i * 2 + 1] = 1 - px[i][1] / 8 * 2; }
    ID3D11Buffer *vb = rx_buffer(&r, sizeof v, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, v);
    if (vs && il && vb && rx_target(&r, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im; UINT stride = 8, off = 0;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, TRUE, FALSE);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &vb, &stride, &off);
        rx_set_color(&r, RX_WHITE);
        ID3D11DeviceContext_Draw(r.ctx, 6, 0);
        if (rx_read_rt(&r, &im)) {
            /* hypotenuses at x+y=4.5 avoid centre ties: 4+3+2+1 = 10 pixels per triangle */
            unsigned n = rx_count32(&im, 0xFFFFFFFFu);
            unsigned a = 0, b = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) if (d3dh_image_pixel32(&im, x, y)) { if (y < 4 && x < 4) a++; else b++; }
            CHECKF(n == 20, "total %u want 20", n);
            CHECKF(a == 10, "triangle 0 pixels %u want 10", a);
            CHECKF(b == 10, "triangle 1 pixels %u want 10", b);
            CHECK(d3dh_image_pixel32(&im, 0, 0) != 0);
            CHECK(d3dh_image_pixel32(&im, 7, 0) == 0);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(vb); SAFE_RELEASE(il); SAFE_RELEASE(vs);
    rx_close(&r);
}
