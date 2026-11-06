/* Rendering on a rasterising device (WARP preferred): clears, draws, state effects, queries, copies, resolve, mips, Map. */
#include "dd.h"

#define T(n) TEST(d3d10_device_render_##n)
#define RED   0xff0000ffu
#define GREEN 0xff00ff00u
#define BLUE  0xffff0000u
#define BLACK 0xff000000u
#define CLEARED 0u

typedef struct { float x, y, z, w; } v4;
typedef struct {
    ID3D10Device *dev; UINT w, h;
    ID3D10Texture2D *rt; ID3D10RenderTargetView *rtv; ID3D10RasterizerState *rs;
} fx;

static void fx_clear(fx *f, float r, float g, float b, float a)
{
    float c[4] = { r, g, b, a };
    ID3D10Device_ClearRenderTargetView(f->dev, f->rtv, c);
}
static int fx_init(fx *f, UINT w, UINT h)
{
    memset(f, 0, sizeof *f);
    if (!DD_C10()) { t_skip("no D3D10CreateDevice"); return 0; }
    HRESULT hr = dd_make(0, &f->dev);
    if (FAILED(hr) || !f->dev) { t_skip("no device: %s", d3dh_hr_name(hr)); return 0; }
    if (!dd_can_render()) { ID3D10Device_Release(f->dev); f->dev = NULL; t_skip("no rendering driver"); return 0; }
    f->w = w; f->h = h;
    D3D10_TEXTURE2D_DESC td = { w, h, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE, 0, 0 };
    if (FAILED(ID3D10Device_CreateTexture2D(f->dev, &td, NULL, &f->rt)) ||
        FAILED(ID3D10Device_CreateRenderTargetView(f->dev, (ID3D10Resource *)f->rt, NULL, &f->rtv))) {
        t_skip("cannot create render target"); return 0;
    }
    D3D10_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd);
    rd.FillMode = D3D10_FILL_SOLID; rd.CullMode = D3D10_CULL_NONE; rd.DepthClipEnable = TRUE;
    ID3D10Device_CreateRasterizerState(f->dev, &rd, &f->rs);
    D3D10_VIEWPORT vp = { 0, 0, w, h, 0.0f, 1.0f };
    ID3D10Device_OMSetRenderTargets(f->dev, 1, &f->rtv, NULL);
    ID3D10Device_RSSetViewports(f->dev, 1, &vp);
    ID3D10Device_RSSetState(f->dev, f->rs);
    fx_clear(f, 0, 0, 0, 0);
    return 1;
}
static void fx_done(fx *f)
{
    if (!f->dev) return;
    ID3D10Device_ClearState(f->dev);
    SAFE_RELEASE(f->rs); SAFE_RELEASE(f->rtv); SAFE_RELEASE(f->rt);
    ULONG rc = ID3D10Device_Release(f->dev);
    t_check(rc == 0, __FILE__, __LINE__, "device refcount after fixture", "got %lu", rc);
}
#define FX(f, w, h) fx f; if (!fx_init(&f, w, h)) return;

static int fx_read(fx *f, d3dh_image *im)
{
    memset(im, 0, sizeof *im);
    HRESULT hr = d3dh_readback_texture2d10(f->dev, f->rt, im);
    t_check(SUCCEEDED(hr) && im->data, __FILE__, __LINE__, "readback", "hr %s", d3dh_hr_name(hr));
    return SUCCEEDED(hr) && im->data;
}
static UINT32 px(const d3dh_image *im, UINT x, UINT y) { return d3dh_image_pixel32(im, x, y); }
static void expect_px(const d3dh_image *im, UINT x, UINT y, UINT32 want, int line)
{
    UINT32 got = px(im, x, y);
    t_check(got == want, __FILE__, line, "pixel", "(%u,%u) got 0x%08x want 0x%08x", x, y, got, want);
}
#define PX(im, x, y, want) expect_px(&(im), x, y, want, __LINE__)
static void expect_all(const d3dh_image *im, UINT32 want, int line)
{
    UINT bad = 0, first = 0;
    for (UINT y = 0; y < im->height; y++) for (UINT x = 0; x < im->width; x++) if (px(im, x, y) != want) { if (!bad) first = y * im->width + x; bad++; }
    t_check(!bad, __FILE__, line, "all pixels", "%u of %u differ from 0x%08x (first at %u,%u = 0x%08x)", bad, im->width * im->height, want, first % im->width, first / im->width, px(im, first % im->width, first / im->width));
}
#define ALL(im, want) expect_all(&(im), want, __LINE__)
static int near8(UINT32 a, UINT32 b, int tol)
{
    for (int s = 0; s < 32; s += 8) { int d = (int)((a >> s) & 0xff) - (int)((b >> s) & 0xff); if (d < -tol || d > tol) return 0; }
    return 1;
}

static ID3D10VertexShader *mkvs(ID3D10Device *dev, const char *src, ID3DBlob **keep)
{
    ID3DBlob *b = dd_blob(src, "vs_4_0"); ID3D10VertexShader *vs = NULL;
    if (!b) return NULL;
    if (FAILED(ID3D10Device_CreateVertexShader(dev, d3dh_blob_ptr(b), d3dh_blob_size(b), &vs))) vs = NULL;
    if (keep) *keep = b; else ID3D10Blob_Release(b);
    return vs;
}
static ID3D10PixelShader *mkps(ID3D10Device *dev, const char *src)
{
    ID3DBlob *b = dd_blob(src, "ps_4_0"); ID3D10PixelShader *ps = NULL;
    if (!b) return NULL;
    if (FAILED(ID3D10Device_CreatePixelShader(dev, d3dh_blob_ptr(b), d3dh_blob_size(b), &ps))) ps = NULL;
    ID3D10Blob_Release(b);
    return ps;
}
static ID3D10Buffer *mkbuf(ID3D10Device *dev, UINT size, UINT bind, D3D10_USAGE usage, UINT cpu, const void *data)
{
    D3D10_BUFFER_DESC d = { size, usage, bind, cpu, 0 }; D3D10_SUBRESOURCE_DATA sd = { data, 0, 0 }; ID3D10Buffer *b = NULL;
    HRESULT hr = ID3D10Device_CreateBuffer(dev, &d, data ? &sd : NULL, &b);
    if (FAILED(hr)) { t_note("buffer creation failed: %s", d3dh_hr_name(hr)); return NULL; }
    return b;
}
#define FSZ(z) { { -1, 1, z, 1 }, { 3, 1, z, 1 }, { -1, -3, z, 1 } }

/* draw a fullscreen triangle with the given pixel shader source, VS from SV_VertexID */
static int draw_fs(fx *f, const char *ps_src)
{
    ID3D10VertexShader *vs = mkvs(f->dev, DD_VS_ID, NULL); ID3D10PixelShader *ps = mkps(f->dev, ps_src);
    if (!vs || !ps) { SAFE_RELEASE(vs); SAFE_RELEASE(ps); t_skip("shader compile unavailable"); return 0; }
    ID3D10Device_IASetPrimitiveTopology(f->dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D10Device_VSSetShader(f->dev, vs); ID3D10Device_PSSetShader(f->dev, ps);
    ID3D10Device_Draw(f->dev, 3, 0);
    ID3D10Device_VSSetShader(f->dev, NULL); ID3D10Device_PSSetShader(f->dev, NULL);
    ID3D10VertexShader_Release(vs); ID3D10PixelShader_Release(ps);
    return 1;
}

/* ---- clears ---- */
static const struct { float c[4]; UINT32 want; } g_clear[] = {
    { { 0, 0, 0, 0 }, 0x00000000u }, { { 1, 1, 1, 1 }, 0xffffffffu }, { { 1, 0, 0, 1 }, RED }, { { 0, 1, 0, 1 }, GREEN }, { { 0, 0, 1, 1 }, BLUE },
    { { 0, 0, 0, 1 }, BLACK }, { { 0, 0, 0, 1 }, BLACK }, { { 2, 3, 4, 5 }, 0xffffffffu }, { { -1, -2, -3, -4 }, 0x00000000u },
    { { 1, 0, 1, 0 }, 0x00ff00ffu }, { { 0, 1, 1, 0 }, 0x00ffff00u }, { { 1, 1, 0, 1 }, 0xff00ffffu },
};
T(clear_rtv_table)
{
    FX(f, 16, 16);
    for (unsigned i = 0; i < sizeof g_clear / sizeof *g_clear; i++) {
        d3dh_image im;
        fx_clear(&f, g_clear[i].c[0], g_clear[i].c[1], g_clear[i].c[2], g_clear[i].c[3]);
        if (fx_read(&f, &im)) { ALL(im, g_clear[i].want); d3dh_image_free(&im); }
    }
    fx_done(&f);
}
T(clear_rtv_half_values)
{
    FX(f, 8, 8);
    d3dh_image im;
    fx_clear(&f, 0.5f, 0.25f, 0.75f, 1.0f);
    if (fx_read(&f, &im)) {
        UINT32 p = px(&im, 3, 3);
        CHECK(near8(p, 0xffbf4080u, 1));
        ALL(im, p);
        d3dh_image_free(&im);
    }
    fx_done(&f);
}
T(clear_rtv_unorm_rounding_endpoints)
{
    FX(f, 4, 4);
    d3dh_image im;
    fx_clear(&f, 1.0f / 255.0f, 254.0f / 255.0f, 128.0f / 255.0f, 0.0f);
    if (fx_read(&f, &im)) { ALL(im, 0x0080fe01u); d3dh_image_free(&im); }
    fx_done(&f);
}
T(clear_rtv_overwrites_previous_draw)
{
    FX(f, 8, 8);
    d3dh_image im;
    if (draw_fs(&f, DD_PS_RED)) {
        fx_clear(&f, 0, 1, 0, 1);
        if (fx_read(&f, &im)) { ALL(im, GREEN); d3dh_image_free(&im); }
    }
    fx_done(&f);
}
T(clear_rtv_ignores_scissor_and_viewport)
{
    FX(f, 16, 16);
    d3dh_image im;
    D3D10_VIEWPORT vp = { 4, 4, 4, 4, 0, 1 }; D3D10_RECT r = { 6, 6, 8, 8 };
    ID3D10Device_RSSetViewports(f.dev, 1, &vp); ID3D10Device_RSSetScissorRects(f.dev, 1, &r);
    fx_clear(&f, 1, 0, 0, 1);
    if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
    fx_done(&f);
}

/* ---- draws ---- */
T(draw_fullscreen_triangle)
{
    FX(f, 16, 16);
    d3dh_image im;
    if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
    fx_done(&f);
}
T(draw_nothing_with_zero_count)
{
    FX(f, 8, 8);
    d3dh_image im;
    fx_clear(&f, 0, 0, 1, 1);
    ID3D10VertexShader *vs = mkvs(f.dev, DD_VS_ID, NULL); ID3D10PixelShader *ps = mkps(f.dev, DD_PS_RED);
    if (vs && ps) {
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_VSSetShader(f.dev, vs); ID3D10Device_PSSetShader(f.dev, ps);
        ID3D10Device_Draw(f.dev, 0, 0);
        ID3D10Device_Draw(f.dev, 2, 0);   /* less than one triangle */
        if (fx_read(&f, &im)) { ALL(im, BLUE); d3dh_image_free(&im); }
    } else t_skip("no shaders");
    SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    fx_done(&f);
}
T(draw_without_pixel_shader_writes_nothing)
{
    FX(f, 8, 8);
    d3dh_image im;
    fx_clear(&f, 0, 1, 0, 1);
    ID3D10VertexShader *vs = mkvs(f.dev, DD_VS_ID, NULL);
    if (vs) {
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_VSSetShader(f.dev, vs);
        ID3D10Device_Draw(f.dev, 3, 0);
        if (fx_read(&f, &im)) { ALL(im, GREEN); d3dh_image_free(&im); }
    } else t_skip("no shader");
    SAFE_RELEASE(vs);
    fx_done(&f);
}
T(draw_vertex_buffer_half_screen)
{
    FX(f, 16, 16);
    static const v4 tri[6] = { { -1, 1, 0, 1 }, { 0, 1, 0, 1 }, { -1, -1, 0, 1 }, { 0, 1, 0, 1 }, { 0, -1, 0, 1 }, { -1, -1, 0, 1 } };
    ID3D10VertexShader *vs; ID3DBlob *blob = NULL; ID3D10PixelShader *ps = mkps(f.dev, DD_PS_RED);
    vs = mkvs(f.dev, DD_VS_POS, &blob);
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof tri, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, tri);
    D3D10_INPUT_ELEMENT_DESC e = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 };
    ID3D10InputLayout *il = NULL;
    if (vs && ps && vb && blob && SUCCEEDED(ID3D10Device_CreateInputLayout(f.dev, &e, 1, d3dh_blob_ptr(blob), d3dh_blob_size(blob), &il))) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_IASetInputLayout(f.dev, il); ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_VSSetShader(f.dev, vs); ID3D10Device_PSSetShader(f.dev, ps);
        ID3D10Device_Draw(f.dev, 6, 0);
        if (fx_read(&f, &im)) {
            for (UINT y = 0; y < 16; y++) for (UINT x = 0; x < 16; x++) if (px(&im, x, y) != (x < 8 ? RED : CLEARED)) { t_check(0, __FILE__, __LINE__, "half screen", "(%u,%u)=0x%08x", x, y, px(&im, x, y)); y = 16; break; }
            d3dh_image_free(&im);
        }
    } else t_skip("setup failed");
    SAFE_RELEASE(il); SAFE_RELEASE(vb); SAFE_RELEASE(vs); SAFE_RELEASE(ps); if (blob) ID3D10Blob_Release(blob);
    fx_done(&f);
}

/* Common setup: VS_POS + float4 position VB bound; returns 0 on skip. */
typedef struct { ID3D10VertexShader *vs; ID3D10PixelShader *ps; ID3D10InputLayout *il; ID3DBlob *blob; } pipe;
static int pipe_init(fx *f, pipe *p, const char *vs_src, const char *ps_src, const D3D10_INPUT_ELEMENT_DESC *e, UINT ne)
{
    memset(p, 0, sizeof *p);
    p->vs = mkvs(f->dev, vs_src, &p->blob); p->ps = mkps(f->dev, ps_src);
    if (!p->vs || !p->ps || FAILED(ID3D10Device_CreateInputLayout(f->dev, e, ne, d3dh_blob_ptr(p->blob), d3dh_blob_size(p->blob), &p->il))) {
        SAFE_RELEASE(p->vs); SAFE_RELEASE(p->ps); if (p->blob) ID3D10Blob_Release(p->blob);
        t_skip("shader/layout setup unavailable"); return 0;
    }
    ID3D10Device_IASetInputLayout(f->dev, p->il);
    ID3D10Device_VSSetShader(f->dev, p->vs); ID3D10Device_PSSetShader(f->dev, p->ps);
    return 1;
}
static void pipe_done(pipe *p) { SAFE_RELEASE(p->il); SAFE_RELEASE(p->vs); SAFE_RELEASE(p->ps); if (p->blob) ID3D10Blob_Release(p->blob); }
static const D3D10_INPUT_ELEMENT_DESC E_POS = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 };

T(draw_indexed_start_and_base)
{
    FX(f, 16, 16);
    pipe p;
    /* vertices 0..2 are off-screen junk, 3..5 cover the screen; indices {0,0,0,0,1,2} */
    v4 vtx[6] = { { 5, 5, 0, 1 }, { 6, 5, 0, 1 }, { 5, 6, 0, 1 }, { -1, 1, 0, 1 }, { 3, 1, 0, 1 }, { -1, -3, 0, 1 } };
    unsigned short idx[6] = { 0, 0, 0, 0, 1, 2 };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    ID3D10Buffer *ib = mkbuf(f.dev, sizeof idx, D3D10_BIND_INDEX_BUFFER, D3D10_USAGE_DEFAULT, 0, idx);
    if (vb && ib && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetIndexBuffer(f.dev, ib, DXGI_FORMAT_R16_UINT, 0);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_DrawIndexed(f.dev, 3, 3, 3);
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        fx_clear(&f, 0, 0, 0, 0);
        /* IA index buffer byte offset: skip the three junk indices */
        ID3D10Device_IASetIndexBuffer(f.dev, ib, DXGI_FORMAT_R16_UINT, 6);
        ID3D10Device_DrawIndexed(f.dev, 3, 0, 3);
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        fx_clear(&f, 0, 0, 0, 0);
        /* the junk-only triangle writes nothing */
        ID3D10Device_IASetIndexBuffer(f.dev, ib, DXGI_FORMAT_R16_UINT, 0);
        ID3D10Device_DrawIndexed(f.dev, 3, 0, 0);
        if (fx_read(&f, &im)) { ALL(im, CLEARED); d3dh_image_free(&im); }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb); SAFE_RELEASE(ib);
    fx_done(&f);
}
T(draw_indexed_32bit_indices)
{
    FX(f, 8, 8);
    pipe p;
    v4 vtx[3] = FSZ(0);
    UINT idx[3] = { 2, 1, 0 };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    ID3D10Buffer *ib = mkbuf(f.dev, sizeof idx, D3D10_BIND_INDEX_BUFFER, D3D10_USAGE_DEFAULT, 0, idx);
    if (vb && ib && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetIndexBuffer(f.dev, ib, DXGI_FORMAT_R32_UINT, 0);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_DrawIndexed(f.dev, 3, 0, 0);
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb); SAFE_RELEASE(ib);
    fx_done(&f);
}
T(draw_vertex_buffer_offset_and_stride)
{
    FX(f, 8, 8);
    pipe p;
    struct { float pad[3]; v4 v[3]; } data = { { 0, 0, 0 }, FSZ(0) };
    /* interleave padding: stride 16 + 8 bytes */
    BYTE raw[3 * 24 + 8]; memset(raw, 0, sizeof raw);
    for (int i = 0; i < 3; i++) memcpy(raw + 8 + i * 24, &data.v[i], 16);
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof raw, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, raw);
    if (vb && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = 24, off = 8; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_Draw(f.dev, 3, 0);
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb);
    fx_done(&f);
}
T(draw_start_vertex_location)
{
    FX(f, 8, 8);
    pipe p;
    v4 vtx[5] = { { 4, 4, 0, 1 }, { 5, 4, 0, 1 }, { -1, 1, 0, 1 }, { 3, 1, 0, 1 }, { -1, -3, 0, 1 } };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    if (vb && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_Draw(f.dev, 3, 2);
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb);
    fx_done(&f);
}
T(draw_triangle_strip_quad)
{
    FX(f, 8, 8);
    pipe p;
    v4 vtx[4] = { { -1, 1, 0, 1 }, { 1, 1, 0, 1 }, { -1, -1, 0, 1 }, { 1, -1, 0, 1 } };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    if (vb && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ID3D10Device_Draw(f.dev, 4, 0);
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb);
    fx_done(&f);
}
T(draw_point_list_single_pixel)
{
    FX(f, 16, 16);
    pipe p;
    v4 vtx[2] = { { (5.5f / 16.0f) * 2 - 1, 1 - (3.5f / 16.0f) * 2, 0, 1 }, { (10.5f / 16.0f) * 2 - 1, 1 - (12.5f / 16.0f) * 2, 0, 1 } };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    if (vb && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im; UINT n = 0;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D10Device_Draw(f.dev, 2, 0);
        if (fx_read(&f, &im)) {
            PX(im, 5, 3, RED); PX(im, 10, 12, RED);
            for (UINT y = 0; y < 16; y++) for (UINT x = 0; x < 16; x++) if (px(&im, x, y) == RED) n++;
            EXPECT_EQ(n, 2);
            d3dh_image_free(&im);
        }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb);
    fx_done(&f);
}
T(draw_line_list_horizontal)
{
    FX(f, 16, 16);
    pipe p;
    float y = 1 - (8.5f / 16.0f) * 2;
    v4 vtx[2] = { { -1, y, 0, 1 }, { 1, y, 0, 1 } };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    if (vb && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
        ID3D10Device_Draw(f.dev, 2, 0);
        if (fx_read(&f, &im)) {
            for (UINT x = 1; x < 15; x++) { PX(im, x, 8, RED); PX(im, x, 7, CLEARED); PX(im, x, 9, CLEARED); }
            d3dh_image_free(&im);
        }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb);
    fx_done(&f);
}
static const char VS_INST[] =
    "float4 main(float4 p : POSITION, uint iid : SV_InstanceID) : SV_Position { p.x += iid * 1.0; return p; }\n";
T(draw_instanced_instance_id)
{
    FX(f, 16, 16);
    pipe p;
    v4 vtx[6] = { { -1, 1, 0, 1 }, { -0.5f, 1, 0, 1 }, { -1, -1, 0, 1 }, { -0.5f, 1, 0, 1 }, { -0.5f, -1, 0, 1 }, { -1, -1, 0, 1 } };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    if (vb && pipe_init(&f, &p, VS_INST, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_DrawInstanced(f.dev, 6, 2, 0, 0);
        if (fx_read(&f, &im)) {
            for (UINT y = 0; y < 16; y += 5) for (UINT x = 0; x < 16; x++) {
                UINT32 want = (x < 4 || (x >= 8 && x < 12)) ? RED : CLEARED;
                if (px(&im, x, y) != want) t_check(0, __FILE__, __LINE__, "instances", "(%u,%u)=0x%08x", x, y, px(&im, x, y));
            }
            d3dh_image_free(&im);
        }
        fx_clear(&f, 0, 0, 0, 0);
        ID3D10Device_DrawInstanced(f.dev, 6, 1, 0, 0);
        if (fx_read(&f, &im)) { PX(im, 1, 8, RED); PX(im, 9, 8, CLEARED); d3dh_image_free(&im); }
        fx_clear(&f, 0, 0, 0, 0);
        ID3D10Device_DrawInstanced(f.dev, 6, 0, 0, 0);
        if (fx_read(&f, &im)) { ALL(im, CLEARED); d3dh_image_free(&im); }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb);
    fx_done(&f);
}
T(draw_instanced_per_instance_data_and_start_location)
{
    FX(f, 16, 16);
    pipe p;
    static const char vs[] = "float4 main(float4 p : POSITION, float dx : XOFF) : SV_Position { p.x += dx; return p; }\n";
    D3D10_INPUT_ELEMENT_DESC e[2] = { E_POS, { "XOFF", 0, DXGI_FORMAT_R32_FLOAT, 1, 0, D3D10_INPUT_PER_INSTANCE_DATA, 1 } };
    v4 vtx[6] = { { -1, 1, 0, 1 }, { -0.5f, 1, 0, 1 }, { -1, -1, 0, 1 }, { -0.5f, 1, 0, 1 }, { -0.5f, -1, 0, 1 }, { -1, -1, 0, 1 } };
    float inst[4] = { 0.0f, 0.5f, 1.0f, 1.5f };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    ID3D10Buffer *ib = mkbuf(f.dev, sizeof inst, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, inst);
    if (vb && ib && pipe_init(&f, &p, vs, DD_PS_RED, e, 2)) {
        ID3D10Buffer *bufs[2] = { vb, ib }; UINT st[2] = { sizeof(v4), 4 }, of[2] = { 0, 0 }; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 2, bufs, st, of);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        /* StartInstanceLocation 2: reads instance data [2] = 1.0 -> pixels 8..11 */
        ID3D10Device_DrawInstanced(f.dev, 6, 1, 0, 2);
        if (fx_read(&f, &im)) {
            for (UINT x = 0; x < 16; x++) { UINT32 want = (x >= 8 && x < 12) ? RED : CLEARED; if (px(&im, x, 8) != want) t_check(0, __FILE__, __LINE__, "start instance", "x %u = 0x%08x", x, px(&im, x, 8)); }
            d3dh_image_free(&im);
        }
        fx_clear(&f, 0, 0, 0, 0);
        ID3D10Device_DrawInstanced(f.dev, 6, 2, 0, 1);  /* data[1], data[2] -> 4..7 and 8..11 */
        if (fx_read(&f, &im)) {
            for (UINT x = 0; x < 16; x++) { UINT32 want = (x >= 4 && x < 12) ? RED : CLEARED; if (px(&im, x, 8) != want) t_check(0, __FILE__, __LINE__, "two instances", "x %u = 0x%08x", x, px(&im, x, 8)); }
            d3dh_image_free(&im);
        }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb); SAFE_RELEASE(ib);
    fx_done(&f);
}
T(draw_indexed_instanced)
{
    FX(f, 16, 16);
    pipe p;
    v4 vtx[4] = { { -1, 1, 0, 1 }, { -0.5f, 1, 0, 1 }, { -1, -1, 0, 1 }, { -0.5f, -1, 0, 1 } };
    unsigned short idx[6] = { 0, 1, 2, 1, 3, 2 };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    ID3D10Buffer *ib = mkbuf(f.dev, sizeof idx, D3D10_BIND_INDEX_BUFFER, D3D10_USAGE_DEFAULT, 0, idx);
    if (vb && ib && pipe_init(&f, &p, VS_INST, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetIndexBuffer(f.dev, ib, DXGI_FORMAT_R16_UINT, 0);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_DrawIndexedInstanced(f.dev, 6, 3, 0, 0, 0);
        if (fx_read(&f, &im)) {
            for (UINT x = 0; x < 16; x++) { UINT32 want = (x < 4 || (x >= 8 && x < 12)) ? RED : CLEARED; if (px(&im, x, 8) != want) t_check(0, __FILE__, __LINE__, "indexed instances", "x %u = 0x%08x", x, px(&im, x, 8)); }
            d3dh_image_free(&im);
        }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb); SAFE_RELEASE(ib);
    fx_done(&f);
}
T(draw_per_vertex_colour_interpolant_flat)
{
    FX(f, 8, 8);
    pipe p;
    D3D10_INPUT_ELEMENT_DESC e[2] = { E_POS, { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0 } };
    struct { v4 p, c; } vtx[3] = { { { -1, 1, 0, 1 }, { 0, 1, 0, 1 } }, { { 3, 1, 0, 1 }, { 0, 1, 0, 1 } }, { { -1, -3, 0, 1 }, { 0, 1, 0, 1 } } };
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof vtx, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, vtx);
    if (vb && pipe_init(&f, &p, DD_VS_POS_COL, DD_PS_COL, e, 2)) {
        UINT stride = sizeof vtx[0], off = 0; d3dh_image im;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_Draw(f.dev, 3, 0);
        if (fx_read(&f, &im)) { ALL(im, GREEN); d3dh_image_free(&im); }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb);
    fx_done(&f);
}

/* ---- pipeline state effects ---- */
T(state_scissor_rect_exact)
{
    FX(f, 16, 16);
    D3D10_RASTERIZER_DESC rd; ID3D10RasterizerState *rs = NULL;
    memset(&rd, 0, sizeof rd); rd.FillMode = D3D10_FILL_SOLID; rd.CullMode = D3D10_CULL_NONE; rd.DepthClipEnable = TRUE; rd.ScissorEnable = TRUE;
    if (SUCCEEDED(ID3D10Device_CreateRasterizerState(f.dev, &rd, &rs))) {
        D3D10_RECT r = { 4, 3, 12, 9 }; d3dh_image im;
        ID3D10Device_RSSetState(f.dev, rs); ID3D10Device_RSSetScissorRects(f.dev, 1, &r);
        if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) {
            for (UINT y = 0; y < 16; y++) for (UINT x = 0; x < 16; x++) {
                UINT32 want = (x >= 4 && x < 12 && y >= 3 && y < 9) ? RED : CLEARED;
                if (px(&im, x, y) != want) { t_check(0, __FILE__, __LINE__, "scissor", "(%u,%u)=0x%08x", x, y, px(&im, x, y)); y = 16; break; }
            }
            d3dh_image_free(&im);
        }
        ID3D10RasterizerState_Release(rs);
    }
    fx_done(&f);
}
T(state_scissor_disabled_ignores_rect)
{
    FX(f, 8, 8);
    D3D10_RECT r = { 2, 2, 4, 4 }; d3dh_image im;
    ID3D10Device_RSSetScissorRects(f.dev, 1, &r);
    if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
    fx_done(&f);
}
T(state_viewport_subrect)
{
    FX(f, 16, 16);
    D3D10_VIEWPORT vp = { 4, 2, 8, 6, 0, 1 }; d3dh_image im;
    ID3D10Device_RSSetViewports(f.dev, 1, &vp);
    if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) {
        for (UINT y = 0; y < 16; y++) for (UINT x = 0; x < 16; x++) {
            UINT32 want = (x >= 4 && x < 12 && y >= 2 && y < 8) ? RED : CLEARED;
            if (px(&im, x, y) != want) { t_check(0, __FILE__, __LINE__, "viewport", "(%u,%u)=0x%08x", x, y, px(&im, x, y)); y = 16; break; }
        }
        d3dh_image_free(&im);
    }
    fx_done(&f);
}
T(state_no_viewport_draws_nothing_observed)
{
    FX(f, 8, 8);
    d3dh_image im;
    ID3D10Device_RSSetViewports(f.dev, 0, NULL);
    if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) { t_note("pixel with no viewport set: 0x%08x", px(&im, 4, 4)); d3dh_image_free(&im); }
    fx_done(&f);
}
T(state_cull_back_front_ccw)
{
    FX(f, 8, 8);
    pipe p;
    v4 cw[3] = FSZ(0);
    v4 ccw[3] = { { -1, 1, 0, 1 }, { -1, -3, 0, 1 }, { 3, 1, 0, 1 } };
    ID3D10Buffer *vcw = mkbuf(f.dev, sizeof cw, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, cw);
    ID3D10Buffer *vccw = mkbuf(f.dev, sizeof ccw, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, ccw);
    if (vcw && vccw && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        static const struct { UINT cull, front_ccw; int cw_visible, ccw_visible; } c[] = {
            { D3D10_CULL_NONE, 0, 1, 1 }, { D3D10_CULL_BACK, 0, 1, 0 }, { D3D10_CULL_FRONT, 0, 0, 1 },
            { D3D10_CULL_BACK, 1, 0, 1 }, { D3D10_CULL_FRONT, 1, 1, 0 }, { D3D10_CULL_NONE, 1, 1, 1 },
        };
        UINT stride = sizeof(v4), off = 0;
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (unsigned i = 0; i < 6; i++) {
            D3D10_RASTERIZER_DESC rd; ID3D10RasterizerState *rs = NULL; d3dh_image im;
            memset(&rd, 0, sizeof rd); rd.FillMode = D3D10_FILL_SOLID; rd.CullMode = (D3D10_CULL_MODE)c[i].cull; rd.FrontCounterClockwise = c[i].front_ccw; rd.DepthClipEnable = TRUE;
            if (FAILED(ID3D10Device_CreateRasterizerState(f.dev, &rd, &rs))) continue;
            ID3D10Device_RSSetState(f.dev, rs);
            for (int which = 0; which < 2; which++) {
                ID3D10Buffer *b = which ? vccw : vcw;
                fx_clear(&f, 0, 0, 0, 0);
                ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &b, &stride, &off);
                ID3D10Device_Draw(f.dev, 3, 0);
                if (fx_read(&f, &im)) {
                    int vis = which ? c[i].ccw_visible : c[i].cw_visible;
                    t_check(px(&im, 4, 4) == (vis ? RED : CLEARED), __FILE__, __LINE__, "cull", "cull %u frontccw %u %s: pixel 0x%08x", c[i].cull, c[i].front_ccw, which ? "ccw" : "cw", px(&im, 4, 4));
                    d3dh_image_free(&im);
                }
            }
            ID3D10RasterizerState_Release(rs);
        }
        pipe_done(&p);
    }
    SAFE_RELEASE(vcw); SAFE_RELEASE(vccw);
    fx_done(&f);
}
T(state_color_write_mask)
{
    FX(f, 8, 8);
    static const struct { UINT mask; UINT32 want; } c[] = { { 0xf, 0xffffffffu }, { 0, BLACK }, { 1, 0xff0000ffu }, { 2, 0xff00ff00u }, { 4, 0xffff0000u }, { 8, 0xff000000u | 0 }, { 3, 0xff00ffffu } };
    /* clear to (0,0,0,0): alpha channel visible only with mask bit 8 */
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        D3D10_BLEND_DESC bd; ID3D10BlendState *bs = NULL; d3dh_image im; float bf[4] = { 1, 1, 1, 1 };
        memset(&bd, 0, sizeof bd); bd.SrcBlend = D3D10_BLEND_ONE; bd.DestBlend = D3D10_BLEND_ZERO; bd.BlendOp = D3D10_BLEND_OP_ADD;
        bd.SrcBlendAlpha = D3D10_BLEND_ONE; bd.DestBlendAlpha = D3D10_BLEND_ZERO; bd.BlendOpAlpha = D3D10_BLEND_OP_ADD;
        for (int k = 0; k < 8; k++) bd.RenderTargetWriteMask[k] = (UINT8)c[i].mask;
        if (FAILED(ID3D10Device_CreateBlendState(f.dev, &bd, &bs))) continue;
        fx_clear(&f, 0, 0, 0, 0);
        ID3D10Device_OMSetBlendState(f.dev, bs, bf, 0xffffffffu);
        if (!draw_fs(&f, "float4 main() : SV_Target { return float4(1, 1, 1, 1); }\n")) { ID3D10BlendState_Release(bs); break; }
        UINT32 want = c[i].mask == 0xf ? 0xffffffffu : c[i].mask == 0 ? 0u : ((c[i].mask & 1 ? 0xffu : 0) | (c[i].mask & 2 ? 0xff00u : 0) | (c[i].mask & 4 ? 0xff0000u : 0) | (c[i].mask & 8 ? 0xff000000u : 0));
        if (fx_read(&f, &im)) { t_check(px(&im, 3, 3) == want, __FILE__, __LINE__, "write mask", "mask %u: 0x%08x want 0x%08x", c[i].mask, px(&im, 3, 3), want); d3dh_image_free(&im); }
        ID3D10Device_OMSetBlendState(f.dev, NULL, NULL, 0xffffffffu);
        ID3D10BlendState_Release(bs);
    }
    fx_done(&f);
}
T(state_alpha_blend_half)
{
    FX(f, 8, 8);
    D3D10_BLEND_DESC bd; ID3D10BlendState *bs = NULL; float bf[4] = { 1, 1, 1, 1 };
    memset(&bd, 0, sizeof bd);
    bd.BlendEnable[0] = TRUE; bd.SrcBlend = D3D10_BLEND_SRC_ALPHA; bd.DestBlend = D3D10_BLEND_INV_SRC_ALPHA; bd.BlendOp = D3D10_BLEND_OP_ADD;
    bd.SrcBlendAlpha = D3D10_BLEND_ONE; bd.DestBlendAlpha = D3D10_BLEND_ZERO; bd.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    for (int k = 0; k < 8; k++) bd.RenderTargetWriteMask[k] = 0xf;
    if (SUCCEEDED(ID3D10Device_CreateBlendState(f.dev, &bd, &bs))) {
        d3dh_image im;
        fx_clear(&f, 0, 0, 1, 1);
        ID3D10Device_OMSetBlendState(f.dev, bs, bf, 0xffffffffu);
        if (draw_fs(&f, "float4 main() : SV_Target { return float4(1, 0, 0, 0.5); }\n") && fx_read(&f, &im)) {
            UINT32 p = px(&im, 4, 4);
            t_check(near8(p, 0x80800080u, 1), __FILE__, __LINE__, "blend", "got 0x%08x want ~0x80800080 (ABGR; alpha = source alpha)", p);
            ALL(im, p);
            d3dh_image_free(&im);
        }
        ID3D10BlendState_Release(bs);
    }
    fx_done(&f);
}
T(state_blend_ops_table)
{
    FX(f, 4, 4);
    /* dst = (0,0,0.5,1) after clear; src = (0.5,0,0,1); factors ONE/ONE */
    static const struct { UINT op; UINT32 want; } c[] = {
        { D3D10_BLEND_OP_ADD, 0xff800080u }, { D3D10_BLEND_OP_SUBTRACT, 0xff000080u }, { D3D10_BLEND_OP_REV_SUBTRACT, 0xff800000u },
        { D3D10_BLEND_OP_MIN, 0xff000000u }, { D3D10_BLEND_OP_MAX, 0xff800080u },
    };
    for (unsigned i = 0; i < 5; i++) {
        D3D10_BLEND_DESC bd; ID3D10BlendState *bs = NULL; float bf[4] = { 1, 1, 1, 1 }; d3dh_image im;
        memset(&bd, 0, sizeof bd);
        bd.BlendEnable[0] = TRUE; bd.SrcBlend = D3D10_BLEND_ONE; bd.DestBlend = D3D10_BLEND_ONE; bd.BlendOp = (D3D10_BLEND_OP)c[i].op;
        bd.SrcBlendAlpha = D3D10_BLEND_ZERO; bd.DestBlendAlpha = D3D10_BLEND_ONE; bd.BlendOpAlpha = D3D10_BLEND_OP_ADD;
        for (int k = 0; k < 8; k++) bd.RenderTargetWriteMask[k] = 0xf;
        if (FAILED(ID3D10Device_CreateBlendState(f.dev, &bd, &bs))) continue;
        fx_clear(&f, 0, 0, 0.5f, 1);
        ID3D10Device_OMSetBlendState(f.dev, bs, bf, 0xffffffffu);
        if (!draw_fs(&f, "float4 main() : SV_Target { return float4(0.5, 0, 0, 1); }\n")) { ID3D10BlendState_Release(bs); break; }
        UINT32 want = c[i].want;
        if (fx_read(&f, &im)) { t_check(near8(px(&im, 1, 1), want, 1), __FILE__, __LINE__, "blend op", "op %u: got 0x%08x want ~0x%08x", c[i].op, px(&im, 1, 1), want); d3dh_image_free(&im); }
        ID3D10Device_OMSetBlendState(f.dev, NULL, NULL, 0xffffffffu);
        ID3D10BlendState_Release(bs);
    }
    fx_done(&f);
}
T(state_blend_factor_constant)
{
    FX(f, 4, 4);
    D3D10_BLEND_DESC bd; ID3D10BlendState *bs = NULL; float bf[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    memset(&bd, 0, sizeof bd);
    bd.BlendEnable[0] = TRUE; bd.SrcBlend = D3D10_BLEND_BLEND_FACTOR; bd.DestBlend = D3D10_BLEND_ZERO; bd.BlendOp = D3D10_BLEND_OP_ADD;
    bd.SrcBlendAlpha = D3D10_BLEND_ONE; bd.DestBlendAlpha = D3D10_BLEND_ZERO; bd.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    for (int k = 0; k < 8; k++) bd.RenderTargetWriteMask[k] = 0xf;
    HRESULT hr = ID3D10Device_CreateBlendState(f.dev, &bd, &bs);
    if (SUCCEEDED(hr)) {
        d3dh_image im;
        ID3D10Device_OMSetBlendState(f.dev, bs, bf, 0xffffffffu);
        if (draw_fs(&f, "float4 main() : SV_Target { return float4(1, 1, 1, 1); }\n") && fx_read(&f, &im)) { ALL(im, 0xff0000ffu); d3dh_image_free(&im); }
        ID3D10BlendState_Release(bs);
    } else t_note("BLEND_FACTOR on a D3D10 blend state: %s", d3dh_hr_name(hr));
    fx_done(&f);
}
T(state_sample_mask_zero_discards_all)
{
    FX(f, 4, 4);
    D3D10_BLEND_DESC bd; ID3D10BlendState *bs = NULL; float bf[4] = { 1, 1, 1, 1 };
    memset(&bd, 0, sizeof bd); bd.SrcBlend = D3D10_BLEND_ONE; bd.DestBlend = D3D10_BLEND_ZERO; bd.BlendOp = D3D10_BLEND_OP_ADD;
    bd.SrcBlendAlpha = D3D10_BLEND_ONE; bd.DestBlendAlpha = D3D10_BLEND_ZERO; bd.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    for (int k = 0; k < 8; k++) bd.RenderTargetWriteMask[k] = 0xf;
    if (SUCCEEDED(ID3D10Device_CreateBlendState(f.dev, &bd, &bs))) {
        d3dh_image im;
        ID3D10Device_OMSetBlendState(f.dev, bs, bf, 0);
        if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) { ALL(im, CLEARED); d3dh_image_free(&im); }
        ID3D10Device_OMSetBlendState(f.dev, bs, bf, 1);
        if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        ID3D10BlendState_Release(bs);
    }
    fx_done(&f);
}
T(state_depth_test)
{
    FX(f, 8, 8);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_D32_FLOAT, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_DEPTH_STENCIL, 0, 0 };
    ID3D10Texture2D *dt = NULL; ID3D10DepthStencilView *dsv = NULL; ID3D10DepthStencilState *dss = NULL;
    D3D10_DEPTH_STENCIL_DESC dd; memset(&dd, 0, sizeof dd);
    dd.DepthEnable = TRUE; dd.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL; dd.DepthFunc = D3D10_COMPARISON_LESS;
    dd.FrontFace.StencilFunc = dd.BackFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
    dd.FrontFace.StencilFailOp = dd.FrontFace.StencilDepthFailOp = dd.FrontFace.StencilPassOp = D3D10_STENCIL_OP_KEEP; dd.BackFace = dd.FrontFace;
    dd.StencilReadMask = dd.StencilWriteMask = 0xff;
    pipe p;
    v4 far_tri[3] = FSZ(0.75f), near_tri[3] = FSZ(0.25f);
    ID3D10Buffer *vfar = mkbuf(f.dev, sizeof far_tri, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, far_tri);
    ID3D10Buffer *vnear = mkbuf(f.dev, sizeof near_tri, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, near_tri);
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(f.dev, &td, NULL, &dt)) && SUCCEEDED(ID3D10Device_CreateDepthStencilView(f.dev, (ID3D10Resource *)dt, NULL, &dsv)) &&
        SUCCEEDED(ID3D10Device_CreateDepthStencilState(f.dev, &dd, &dss)) && vfar && vnear && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0; d3dh_image im;
        ID3D10Device_OMSetRenderTargets(f.dev, 1, &f.rtv, dsv);
        ID3D10Device_OMSetDepthStencilState(f.dev, dss, 0);
        ID3D10Device_ClearDepthStencilView(f.dev, dsv, D3D10_CLEAR_DEPTH, 0.5f, 0);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vfar, &stride, &off);
        ID3D10Device_Draw(f.dev, 3, 0);                      /* z 0.75 > 0.5: fails LESS */
        if (fx_read(&f, &im)) { ALL(im, CLEARED); d3dh_image_free(&im); }
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vnear, &stride, &off);
        ID3D10Device_Draw(f.dev, 3, 0);                      /* z 0.25 < 0.5: passes and writes depth */
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        fx_clear(&f, 0, 0, 0, 0);
        ID3D10Device_Draw(f.dev, 3, 0);                      /* 0.25 again: not LESS than the 0.25 written */
        if (fx_read(&f, &im)) { ALL(im, CLEARED); d3dh_image_free(&im); }
        ID3D10Device_ClearDepthStencilView(f.dev, dsv, D3D10_CLEAR_DEPTH, 1.0f, 0);
        ID3D10Device_Draw(f.dev, 3, 0);
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        pipe_done(&p);
    } else t_skip("depth setup unavailable");
    SAFE_RELEASE(vfar); SAFE_RELEASE(vnear); SAFE_RELEASE(dss); SAFE_RELEASE(dsv); SAFE_RELEASE(dt);
    fx_done(&f);
}
T(state_depth_clip)
{
    FX(f, 8, 8);
    pipe p;
    v4 tri[3] = FSZ(1.5f); /* beyond the far plane */
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof tri, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, tri);
    if (vb && pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        static const BOOL clip[2] = { TRUE, FALSE };
        UINT stride = sizeof(v4), off = 0;
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (int i = 0; i < 2; i++) {
            D3D10_RASTERIZER_DESC rd; ID3D10RasterizerState *rs = NULL; d3dh_image im;
            memset(&rd, 0, sizeof rd); rd.FillMode = D3D10_FILL_SOLID; rd.CullMode = D3D10_CULL_NONE; rd.DepthClipEnable = clip[i];
            if (FAILED(ID3D10Device_CreateRasterizerState(f.dev, &rd, &rs))) continue;
            ID3D10Device_RSSetState(f.dev, rs);
            fx_clear(&f, 0, 0, 0, 0);
            ID3D10Device_Draw(f.dev, 3, 0);
            if (fx_read(&f, &im)) { t_check(px(&im, 4, 4) == (clip[i] ? CLEARED : RED), __FILE__, __LINE__, "depth clip", "DepthClipEnable %d pixel 0x%08x", clip[i], px(&im, 4, 4)); d3dh_image_free(&im); }
            ID3D10RasterizerState_Release(rs);
        }
        pipe_done(&p);
    }
    SAFE_RELEASE(vb);
    fx_done(&f);
}
T(state_stencil_ref)
{
    FX(f, 8, 8);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_DEPTH_STENCIL, 0, 0 };
    ID3D10Texture2D *dt = NULL; ID3D10DepthStencilView *dsv = NULL; ID3D10DepthStencilState *dss = NULL;
    D3D10_DEPTH_STENCIL_DESC dd; memset(&dd, 0, sizeof dd);
    dd.DepthEnable = FALSE; dd.StencilEnable = TRUE; dd.StencilReadMask = 0xff; dd.StencilWriteMask = 0xff;
    dd.FrontFace.StencilFunc = dd.BackFace.StencilFunc = D3D10_COMPARISON_EQUAL;
    dd.FrontFace.StencilFailOp = dd.FrontFace.StencilDepthFailOp = dd.FrontFace.StencilPassOp = D3D10_STENCIL_OP_KEEP; dd.BackFace = dd.FrontFace;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(f.dev, &td, NULL, &dt)) && SUCCEEDED(ID3D10Device_CreateDepthStencilView(f.dev, (ID3D10Resource *)dt, NULL, &dsv)) &&
        SUCCEEDED(ID3D10Device_CreateDepthStencilState(f.dev, &dd, &dss))) {
        d3dh_image im;
        ID3D10Device_OMSetRenderTargets(f.dev, 1, &f.rtv, dsv);
        ID3D10Device_ClearDepthStencilView(f.dev, dsv, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0f, 3);
        ID3D10Device_OMSetDepthStencilState(f.dev, dss, 4);
        if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) { ALL(im, CLEARED); d3dh_image_free(&im); }
        ID3D10Device_OMSetDepthStencilState(f.dev, dss, 3);
        if (draw_fs(&f, DD_PS_RED) && fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
    } else t_skip("stencil setup unavailable");
    SAFE_RELEASE(dss); SAFE_RELEASE(dsv); SAFE_RELEASE(dt);
    fx_done(&f);
}
T(state_multiple_render_targets)
{
    FX(f, 8, 8);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET, 0, 0 };
    ID3D10Texture2D *t2 = NULL; ID3D10RenderTargetView *r2 = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(f.dev, &td, NULL, &t2)) && SUCCEEDED(ID3D10Device_CreateRenderTargetView(f.dev, (ID3D10Resource *)t2, NULL, &r2))) {
        ID3D10RenderTargetView *rt[2] = { f.rtv, r2 }; d3dh_image im; d3dh_image im2;
        ID3D10Device_OMSetRenderTargets(f.dev, 2, rt, NULL);
        if (draw_fs(&f, "struct O { float4 a : SV_Target0; float4 b : SV_Target1; };\nO main() { O o; o.a = float4(1, 0, 0, 1); o.b = float4(0, 0, 1, 1); return o; }\n")) {
            if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
            if (SUCCEEDED(d3dh_readback_texture2d10(f.dev, t2, &im2))) { ALL(im2, BLUE); d3dh_image_free(&im2); }
        }
    }
    SAFE_RELEASE(r2); SAFE_RELEASE(t2);
    fx_done(&f);
}
T(state_om_get_render_targets_after_set)
{
    FX(f, 8, 8);
    ID3D10RenderTargetView *g[8] = { 0 }; ID3D10DepthStencilView *gd = NULL;
    ID3D10Device_OMGetRenderTargets(f.dev, 8, g, &gd);
    CHECK(g[0] == f.rtv); for (int i = 1; i < 8; i++) CHECK(g[i] == NULL); CHECK(gd == NULL);
    SAFE_RELEASE(g[0]);
    ID3D10Device_OMSetRenderTargets(f.dev, 0, NULL, NULL);
    ID3D10Device_OMGetRenderTargets(f.dev, 8, g, &gd);
    CHECK(g[0] == NULL);
    fx_done(&f);
}
T(state_constant_buffer_update)
{
    FX(f, 8, 8);
    float c0[4] = { 0, 1, 0, 1 }, c1[4] = { 0, 0, 1, 1 };
    ID3D10Buffer *cb = mkbuf(f.dev, 16, D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DEFAULT, 0, c0);
    if (cb) {
        d3dh_image im;
        ID3D10Device_PSSetConstantBuffers(f.dev, 0, 1, &cb);
        if (draw_fs(&f, DD_PS_CB) && fx_read(&f, &im)) {
            ALL(im, GREEN); d3dh_image_free(&im);
            ID3D10Device_UpdateSubresource(f.dev, (ID3D10Resource *)cb, 0, NULL, c1, 0, 0);
            draw_fs(&f, DD_PS_CB);
            if (fx_read(&f, &im)) { ALL(im, BLUE); d3dh_image_free(&im); }
        }
        ID3D10Buffer_Release(cb);
    }
    fx_done(&f);
}
T(state_constant_buffer_dynamic_map)
{
    FX(f, 8, 8);
    ID3D10Buffer *cb = mkbuf(f.dev, 16, D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE, NULL);
    if (cb) {
        void *m = NULL;
        HRESULT hr = ID3D10Buffer_Map(cb, D3D10_MAP_WRITE_DISCARD, 0, &m);
        EXPECT_HR_OK(hr);
        if (SUCCEEDED(hr) && m) {
            float c[4] = { 1, 1, 0, 1 }; d3dh_image im;
            memcpy(m, c, 16); ID3D10Buffer_Unmap(cb);
            ID3D10Device_PSSetConstantBuffers(f.dev, 0, 1, &cb);
            if (draw_fs(&f, DD_PS_CB) && fx_read(&f, &im)) { ALL(im, 0xff00ffffu); d3dh_image_free(&im); }
        }
        ID3D10Buffer_Release(cb);
    }
    fx_done(&f);
}
T(state_texture_load_copy)
{
    FX(f, 8, 8);
    UINT32 src[64]; for (int i = 0; i < 64; i++) src[i] = 0xff000000u | ((UINT)i * 4) | ((UINT)(63 - i) << 8) | ((UINT)((i * 37) & 0xff) << 16);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd = { src, 32, 0 };
    ID3D10Texture2D *t = NULL; ID3D10ShaderResourceView *v = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(f.dev, &td, &sd, &t)) && SUCCEEDED(ID3D10Device_CreateShaderResourceView(f.dev, (ID3D10Resource *)t, NULL, &v))) {
        d3dh_image im;
        ID3D10Device_PSSetShaderResources(f.dev, 0, 1, &v);
        if (draw_fs(&f, "Texture2D t : register(t0);\nfloat4 main(float4 p : SV_Position) : SV_Target { return t.Load(int3(p.xy, 0)); }\n") && fx_read(&f, &im)) {
            UINT bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) if (px(&im, x, y) != src[y * 8 + x]) bad++;
            t_check(!bad, __FILE__, __LINE__, "texture load", "%u texels differ", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(t);
    fx_done(&f);
}
T(state_texture_point_sample)
{
    FX(f, 8, 8);
    UINT32 src[16]; for (int i = 0; i < 16; i++) src[i] = 0xff000000u | (UINT)(i * 16) | ((UINT)(255 - i * 16) << 8);
    D3D10_TEXTURE2D_DESC td = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd = { src, 16, 0 };
    D3D10_SAMPLER_DESC smd; memset(&smd, 0, sizeof smd);
    smd.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT; smd.AddressU = smd.AddressV = smd.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP; smd.MaxAnisotropy = 1; smd.ComparisonFunc = D3D10_COMPARISON_NEVER; smd.MaxLOD = D3D10_FLOAT32_MAX;
    ID3D10Texture2D *t = NULL; ID3D10ShaderResourceView *v = NULL; ID3D10SamplerState *s = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(f.dev, &td, &sd, &t)) && SUCCEEDED(ID3D10Device_CreateShaderResourceView(f.dev, (ID3D10Resource *)t, NULL, &v)) &&
        SUCCEEDED(ID3D10Device_CreateSamplerState(f.dev, &smd, &s))) {
        d3dh_image im;
        ID3D10Device_PSSetShaderResources(f.dev, 0, 1, &v); ID3D10Device_PSSetSamplers(f.dev, 0, 1, &s);
        if (draw_fs(&f, "Texture2D t : register(t0);\nSamplerState s : register(s0);\nfloat4 main(float4 p : SV_Position) : SV_Target { return t.Sample(s, p.xy / 8.0); }\n") && fx_read(&f, &im)) {
            UINT bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) if (px(&im, x, y) != src[(y / 2) * 4 + x / 2]) bad++;
            t_check(!bad, __FILE__, __LINE__, "point sample", "%u pixels differ", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(s); SAFE_RELEASE(v); SAFE_RELEASE(t);
    fx_done(&f);
}

/* ---- stream output + DrawAuto ---- */
T(so_stream_and_draw_auto)
{
    FX(f, 8, 8);
    ID3DBlob *gb = dd_blob(DD_GS_PASS, "gs_4_0");
    pipe p;
    v4 tri[3] = FSZ(0);
    ID3D10Buffer *vb = mkbuf(f.dev, sizeof tri, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, tri);
    ID3D10Buffer *so = mkbuf(f.dev, 256, D3D10_BIND_STREAM_OUTPUT | D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, NULL);
    ID3D10GeometryShader *gs = NULL;
    D3D10_SO_DECLARATION_ENTRY decl[] = { { "SV_Position", 0, 0, 4, 0 } };
    if (gb && vb && so && SUCCEEDED(ID3D10Device_CreateGeometryShaderWithStreamOutput(f.dev, d3dh_blob_ptr(gb), d3dh_blob_size(gb), decl, 1, 16, &gs)) &&
        pipe_init(&f, &p, DD_VS_POS, DD_PS_RED, &E_POS, 1)) {
        UINT stride = sizeof(v4), off = 0, so_off = 0; d3dh_image im; void *data = NULL; size_t sz = 0;
        D3D10_QUERY_DESC qd = { D3D10_QUERY_SO_STATISTICS, 0 }; ID3D10Query *q = NULL;
        ID3D10Device_CreateQuery(f.dev, &qd, &q);
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, &vb, &stride, &off);
        ID3D10Device_IASetPrimitiveTopology(f.dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D10Device_GSSetShader(f.dev, gs);
        ID3D10Device_SOSetTargets(f.dev, 1, &so, &so_off);
        if (q) ID3D10Query_Begin(q);
        ID3D10Device_Draw(f.dev, 3, 0);
        if (q) ID3D10Query_End(q);
        ID3D10Device_SOSetTargets(f.dev, 0, NULL, NULL);
        ID3D10Device_GSSetShader(f.dev, NULL);
        if (SUCCEEDED(d3dh_readback_buffer10(f.dev, so, &data, &sz)) && data) {
            CHECK(sz >= 48); CHECK(!memcmp(data, tri, sizeof tri)); free(data);
        }
        if (q) {
            D3D10_QUERY_DATA_SO_STATISTICS st; HRESULT hr = S_FALSE;
            for (int i = 0; i < 2000 && hr == S_FALSE; i++) { hr = ID3D10Query_GetData(q, &st, sizeof st, 0); if (hr == S_FALSE) Sleep(1); }
            if (hr == S_OK) { EXPECT_EQ(st.NumPrimitivesWritten, 1); EXPECT_EQ(st.PrimitivesStorageNeeded, 1); } else CHECKF(0, "SO statistics query hr %s", d3dh_hr_name(hr));
            ID3D10Query_Release(q);
        }
        ID3D10Buffer *bufs[1] = { so };
        fx_clear(&f, 0, 0, 0, 0);
        ID3D10Device_IASetVertexBuffers(f.dev, 0, 1, bufs, &stride, &off);
        ID3D10Device_DrawAuto(f.dev);
        if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        pipe_done(&p);
    } else t_skip("stream output setup unavailable");
    SAFE_RELEASE(gs); SAFE_RELEASE(vb); SAFE_RELEASE(so); if (gb) ID3D10Blob_Release(gb);
    fx_done(&f);
}

/* ---- queries ---- */
static HRESULT poll(ID3D10Asynchronous *q, void *data, UINT size)
{
    HRESULT hr = S_FALSE;
    for (int i = 0; i < 3000 && hr == S_FALSE; i++) { hr = ID3D10Asynchronous_GetData(q, data, size, 0); if (hr == S_FALSE) Sleep(1); }
    return hr;
}
T(query_event)
{
    FX(f, 4, 4);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_EVENT, 0 }; ID3D10Query *q = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &qd, &q))) {
        BOOL v = FALSE;
        ID3D10Query_End(q);
        HRESULT hr = poll((ID3D10Asynchronous *)q, &v, sizeof v);
        EXPECT_HR(hr, S_OK); EXPECT_EQ(v, TRUE);
        /* a second GetData on a signalled event is still S_OK */
        v = FALSE; EXPECT_HR(ID3D10Query_GetData(q, &v, sizeof v, 0), S_OK); EXPECT_EQ(v, TRUE);
        ID3D10Query_Release(q);
    }
    fx_done(&f);
}
T(query_getdata_without_buffer_probes)
{
    FX(f, 4, 4);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_EVENT, 0 }; ID3D10Query *q = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &qd, &q))) {
        ID3D10Query_End(q);
        HRESULT hr = poll((ID3D10Asynchronous *)q, NULL, 0);
        EXPECT_HR(hr, S_OK);
        ID3D10Query_Release(q);
    }
    fx_done(&f);
}
T(query_occlusion_counts_samples)
{
    FX(f, 16, 16);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_OCCLUSION, 0 }; ID3D10Query *q = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &qd, &q))) {
        UINT64 v = 0xdead;
        ID3D10Query_Begin(q);
        if (draw_fs(&f, DD_PS_RED)) {
            ID3D10Query_End(q);
            HRESULT hr = poll((ID3D10Asynchronous *)q, &v, sizeof v);
            EXPECT_HR(hr, S_OK); EXPECT_EQ(v, 256);
        }
        ID3D10Query_Release(q);
    }
    fx_done(&f);
}
T(query_occlusion_zero_without_draw)
{
    FX(f, 8, 8);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_OCCLUSION, 0 }; ID3D10Query *q = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &qd, &q))) {
        UINT64 v = 0xdead;
        ID3D10Query_Begin(q); ID3D10Query_End(q);
        EXPECT_HR(poll((ID3D10Asynchronous *)q, &v, sizeof v), S_OK);
        EXPECT_EQ(v, 0);
        ID3D10Query_Release(q);
    }
    fx_done(&f);
}
T(query_occlusion_restart)
{
    FX(f, 8, 8);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_OCCLUSION, 0 }; ID3D10Query *q = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &qd, &q))) {
        UINT64 v = 0xdead;
        ID3D10Query_Begin(q);
        draw_fs(&f, DD_PS_RED);
        ID3D10Query_End(q);
        EXPECT_HR(poll((ID3D10Asynchronous *)q, &v, sizeof v), S_OK); EXPECT_EQ(v, 64);
        ID3D10Query_Begin(q); ID3D10Query_End(q);
        EXPECT_HR(poll((ID3D10Asynchronous *)q, &v, sizeof v), S_OK); EXPECT_EQ(v, 0);
        ID3D10Query_Release(q);
    }
    fx_done(&f);
}
T(query_occlusion_predicate)
{
    FX(f, 8, 8);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_OCCLUSION_PREDICATE, 0 }; ID3D10Predicate *p = NULL;
    if (SUCCEEDED(ID3D10Device_CreatePredicate(f.dev, &qd, &p))) {
        BOOL v = 0xcc;
        ID3D10Predicate_Begin(p); ID3D10Predicate_End(p);
        EXPECT_HR(poll((ID3D10Asynchronous *)p, &v, sizeof v), S_OK); EXPECT_EQ(v, FALSE);
        ID3D10Predicate_Begin(p); draw_fs(&f, DD_PS_RED); ID3D10Predicate_End(p);
        EXPECT_HR(poll((ID3D10Asynchronous *)p, &v, sizeof v), S_OK); EXPECT_EQ(v, TRUE);
        ID3D10Predicate_Release(p);
    }
    fx_done(&f);
}
T(query_timestamp_disjoint)
{
    FX(f, 4, 4);
    D3D10_QUERY_DESC dq = { D3D10_QUERY_TIMESTAMP_DISJOINT, 0 }, tq = { D3D10_QUERY_TIMESTAMP, 0 };
    ID3D10Query *d = NULL, *t0 = NULL, *t1 = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &dq, &d)) && SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &tq, &t0)) && SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &tq, &t1))) {
        D3D10_QUERY_DATA_TIMESTAMP_DISJOINT dj; UINT64 a = 0, b = 0;
        ID3D10Query_Begin(d);
        ID3D10Query_End(t0);
        draw_fs(&f, DD_PS_RED);
        ID3D10Query_End(t1);
        ID3D10Query_End(d);
        EXPECT_HR(poll((ID3D10Asynchronous *)d, &dj, sizeof dj), S_OK);
        EXPECT_HR(poll((ID3D10Asynchronous *)t0, &a, sizeof a), S_OK);
        EXPECT_HR(poll((ID3D10Asynchronous *)t1, &b, sizeof b), S_OK);
        t_note("timestamp frequency %llu disjoint %d ts %llu %llu", (unsigned long long)dj.Frequency, dj.Disjoint, (unsigned long long)a, (unsigned long long)b);
        if (!dj.Disjoint) { CHECK(dj.Frequency != 0); CHECK(b >= a); }
    }
    SAFE_RELEASE(d); SAFE_RELEASE(t0); SAFE_RELEASE(t1);
    fx_done(&f);
}
T(query_pipeline_statistics)
{
    FX(f, 16, 16);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_PIPELINE_STATISTICS, 0 }; ID3D10Query *q = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &qd, &q))) {
        D3D10_QUERY_DATA_PIPELINE_STATISTICS s; memset(&s, 0xcc, sizeof s);
        ID3D10Query_Begin(q);
        draw_fs(&f, DD_PS_RED);
        ID3D10Query_End(q);
        EXPECT_HR(poll((ID3D10Asynchronous *)q, &s, sizeof s), S_OK);
        EXPECT_EQ(s.IAVertices, 3); EXPECT_EQ(s.IAPrimitives, 1);
        EXPECT_EQ(s.CInvocations, 1); t_note("CPrimitives %llu", (unsigned long long)s.CPrimitives); CHECK(s.CPrimitives >= 1);
        EXPECT_EQ(s.GSInvocations, 0); EXPECT_EQ(s.GSPrimitives, 0);
        t_note("VSInvocations %llu PSInvocations %llu", (unsigned long long)s.VSInvocations, (unsigned long long)s.PSInvocations);
        CHECK(s.VSInvocations >= 1);
        CHECK(s.PSInvocations >= 256);
        ID3D10Query_Release(q);
    }
    fx_done(&f);
}
T(query_pipeline_statistics_idle)
{
    FX(f, 8, 8);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_PIPELINE_STATISTICS, 0 }; ID3D10Query *q = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &qd, &q))) {
        D3D10_QUERY_DATA_PIPELINE_STATISTICS s; memset(&s, 0xcc, sizeof s);
        ID3D10Query_Begin(q); ID3D10Query_End(q);
        EXPECT_HR(poll((ID3D10Asynchronous *)q, &s, sizeof s), S_OK);
        EXPECT_EQ(s.IAVertices, 0); EXPECT_EQ(s.IAPrimitives, 0); EXPECT_EQ(s.VSInvocations, 0); EXPECT_EQ(s.PSInvocations, 0);
        EXPECT_EQ(s.CInvocations, 0); EXPECT_EQ(s.CPrimitives, 0);
        ID3D10Query_Release(q);
    }
    fx_done(&f);
}
T(query_getdata_flags_do_not_flush)
{
    FX(f, 4, 4);
    D3D10_QUERY_DESC qd = { D3D10_QUERY_EVENT, 0 }; ID3D10Query *q = NULL;
    if (SUCCEEDED(ID3D10Device_CreateQuery(f.dev, &qd, &q))) {
        BOOL v = 0; HRESULT hr;
        ID3D10Query_End(q);
        hr = ID3D10Query_GetData(q, &v, sizeof v, D3D10_ASYNC_GETDATA_DONOTFLUSH);
        CHECK(hr == S_OK || hr == S_FALSE);
        EXPECT_HR(poll((ID3D10Asynchronous *)q, &v, sizeof v), S_OK);
        ID3D10Query_Release(q);
    }
    fx_done(&f);
}

/* ---- Map / copy / update ---- */
T(map_staging_buffer_roundtrip)
{
    DD_DEV_R(dev);
    ID3D10Buffer *st = mkbuf(dev, 64, 0, D3D10_USAGE_STAGING, D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE, NULL);
    if (st) {
        void *m = NULL; BYTE in[64]; for (int i = 0; i < 64; i++) in[i] = (BYTE)(i ^ 0x5a);
        EXPECT_HR_OK(ID3D10Buffer_Map(st, D3D10_MAP_WRITE, 0, &m));
        if (m) { memcpy(m, in, 64); ID3D10Buffer_Unmap(st); }
        m = NULL;
        EXPECT_HR_OK(ID3D10Buffer_Map(st, D3D10_MAP_READ, 0, &m));
        if (m) { CHECK(!memcmp(m, in, 64)); ID3D10Buffer_Unmap(st); }
        m = NULL;
        EXPECT_HR_OK(ID3D10Buffer_Map(st, D3D10_MAP_READ_WRITE, 0, &m));
        if (m) { CHECK(!memcmp(m, in, 64)); ((BYTE *)m)[0] ^= 0xff; ID3D10Buffer_Unmap(st); }
        m = NULL;
        EXPECT_HR_OK(ID3D10Buffer_Map(st, D3D10_MAP_READ, 0, &m));
        if (m) { CHECK(((BYTE *)m)[0] == (BYTE)(in[0] ^ 0xff)); ID3D10Buffer_Unmap(st); }
        ID3D10Buffer_Release(st);
    }
    DD_END(dev);
}
T(map_type_cpu_access_mismatch)
{
    DD_DEV_R(dev);
    ID3D10Buffer *rd = mkbuf(dev, 64, 0, D3D10_USAGE_STAGING, D3D10_CPU_ACCESS_READ, NULL);
    ID3D10Buffer *wr = mkbuf(dev, 64, 0, D3D10_USAGE_STAGING, D3D10_CPU_ACCESS_WRITE, NULL);
    ID3D10Buffer *dyn = mkbuf(dev, 64, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE, NULL);
    void *m = (void *)1;
    if (rd) {
        HRESULT hr = ID3D10Buffer_Map(rd, D3D10_MAP_WRITE, 0, &m); CHECK(FAILED(hr)); t_note("WRITE on read-only staging: %s", d3dh_hr_name(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(rd);
        m = (void *)1; hr = ID3D10Buffer_Map(rd, D3D10_MAP_READ_WRITE, 0, &m); CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(rd);
        m = (void *)1; hr = ID3D10Buffer_Map(rd, D3D10_MAP_WRITE_DISCARD, 0, &m); CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(rd);
    }
    if (wr) {
        m = (void *)1; HRESULT hr = ID3D10Buffer_Map(wr, D3D10_MAP_READ, 0, &m); CHECK(FAILED(hr)); t_note("READ on write-only staging: %s", d3dh_hr_name(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(wr);
        m = (void *)1; hr = ID3D10Buffer_Map(wr, D3D10_MAP_READ_WRITE, 0, &m); CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(wr);
        m = (void *)1; hr = ID3D10Buffer_Map(wr, D3D10_MAP_WRITE, 0, &m); EXPECT_HR_OK(hr); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(wr);
    }
    if (dyn) {
        m = (void *)1; HRESULT hr = ID3D10Buffer_Map(dyn, D3D10_MAP_READ, 0, &m); CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(dyn);
        m = (void *)1; hr = ID3D10Buffer_Map(dyn, D3D10_MAP_READ_WRITE, 0, &m); CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(dyn);
        m = NULL; hr = ID3D10Buffer_Map(dyn, D3D10_MAP_WRITE_DISCARD, 0, &m); EXPECT_HR_OK(hr); CHECK(m != NULL); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(dyn);
        m = NULL; hr = ID3D10Buffer_Map(dyn, D3D10_MAP_WRITE_NO_OVERWRITE, 0, &m); EXPECT_HR_OK(hr); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(dyn);
        m = NULL; hr = ID3D10Buffer_Map(dyn, D3D10_MAP_WRITE, 0, &m); t_note("plain WRITE on dynamic buffer: %s", d3dh_hr_name(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(dyn);
    }
    SAFE_RELEASE(rd); SAFE_RELEASE(wr); SAFE_RELEASE(dyn);
    DD_END(dev);
}
T(map_staging_rejects_discard_and_no_overwrite)
{
    DD_DEV_R(dev);
    ID3D10Buffer *wr = mkbuf(dev, 64, 0, D3D10_USAGE_STAGING, D3D10_CPU_ACCESS_WRITE, NULL);
    if (wr) {
        void *m = (void *)1;
        HRESULT hr = ID3D10Buffer_Map(wr, D3D10_MAP_WRITE_DISCARD, 0, &m); t_note("WRITE_DISCARD on staging: %s", d3dh_hr_name(hr)); CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(wr);
        m = (void *)1; hr = ID3D10Buffer_Map(wr, D3D10_MAP_WRITE_NO_OVERWRITE, 0, &m); t_note("WRITE_NO_OVERWRITE on staging: %s", d3dh_hr_name(hr)); CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(wr);
        ID3D10Buffer_Release(wr);
    }
    DD_END(dev);
}
T(map_default_resource_fails)
{
    DD_DEV_R(dev);
    ID3D10Buffer *b = mkbuf(dev, 64, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, NULL);
    if (b) {
        static const D3D10_MAP types[] = { D3D10_MAP_READ, D3D10_MAP_WRITE, D3D10_MAP_READ_WRITE, D3D10_MAP_WRITE_DISCARD, D3D10_MAP_WRITE_NO_OVERWRITE };
        for (unsigned i = 0; i < 5; i++) {
            void *m = (void *)1; HRESULT hr = ID3D10Buffer_Map(b, types[i], 0, &m);
            t_check(FAILED(hr), __FILE__, __LINE__, "map default", "type %u: %s", types[i], d3dh_hr_name(hr));
            if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(b);
        }
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(map_immutable_resource_fails)
{
    DD_DEV_R(dev);
    UINT data[16] = { 0 };
    ID3D10Buffer *b = mkbuf(dev, 64, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_IMMUTABLE, 0, data);
    if (b) {
        void *m = (void *)1; HRESULT hr = ID3D10Buffer_Map(b, D3D10_MAP_READ, 0, &m);
        CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(b);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(map_do_not_wait_flag)
{
    DD_DEV_R(dev);
    ID3D10Buffer *src = mkbuf(dev, 64, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, NULL);
    ID3D10Buffer *st = mkbuf(dev, 64, 0, D3D10_USAGE_STAGING, D3D10_CPU_ACCESS_READ, NULL);
    if (src && st) {
        void *m = NULL;
        ID3D10Device_CopyResource(dev, (ID3D10Resource *)st, (ID3D10Resource *)src);
        HRESULT hr = S_FALSE;
        for (int i = 0; i < 3000; i++) {
            hr = ID3D10Buffer_Map(st, D3D10_MAP_READ, D3D10_MAP_FLAG_DO_NOT_WAIT, &m);
            if (hr != DXGI_ERROR_WAS_STILL_DRAWING) break;
            Sleep(1);
        }
        CHECK(hr == S_OK);
        if (SUCCEEDED(hr)) ID3D10Buffer_Unmap(st);
    }
    SAFE_RELEASE(src); SAFE_RELEASE(st);
    DD_END(dev);
}
T(map_texture_pitches)
{
    DD_DEV_R(dev);
    D3D10_TEXTURE2D_DESC td = { 13, 5, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_STAGING, 0, D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE, 0 };
    ID3D10Texture2D *t = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(dev, &td, NULL, &t))) {
        D3D10_MAPPED_TEXTURE2D mt; memset(&mt, 0, sizeof mt);
        EXPECT_HR_OK(ID3D10Texture2D_Map(t, 0, D3D10_MAP_READ_WRITE, 0, &mt));
        if (mt.pData) {
            CHECK(mt.RowPitch >= 13 * 4);
            for (UINT y = 0; y < 5; y++) memset((BYTE *)mt.pData + y * mt.RowPitch, 0x10 + y, 13 * 4);
            ID3D10Texture2D_Unmap(t, 0);
            EXPECT_HR_OK(ID3D10Texture2D_Map(t, 0, D3D10_MAP_READ, 0, &mt));
            if (mt.pData) for (UINT y = 0; y < 5; y++) CHECK(((BYTE *)mt.pData)[y * mt.RowPitch + 51] == 0x10 + y);
            ID3D10Texture2D_Unmap(t, 0);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(map_texture_subresource_out_of_range)
{
    DD_DEV_R(dev);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_STAGING, 0, D3D10_CPU_ACCESS_READ, 0 };
    ID3D10Texture2D *t = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(dev, &td, NULL, &t))) {
        D3D10_MAPPED_TEXTURE2D mt; memset(&mt, 0xcc, sizeof mt);
        HRESULT hr = ID3D10Texture2D_Map(t, 1, D3D10_MAP_READ, 0, &mt);
        EXPECT_HR(hr, E_INVALIDARG); /* Win10: probed */
        t_note("Map of subresource 1 of a single-subresource texture: %s", d3dh_hr_name(hr));
        CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Texture2D_Unmap(t, 1);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(map_texture3d_pitches)
{
    DD_DEV_R(dev);
    D3D10_TEXTURE3D_DESC td = { 4, 4, 3, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_USAGE_STAGING, 0, D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE, 0 };
    ID3D10Texture3D *t = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture3D(dev, &td, NULL, &t))) {
        D3D10_MAPPED_TEXTURE3D mt; memset(&mt, 0, sizeof mt);
        EXPECT_HR_OK(ID3D10Texture3D_Map(t, 0, D3D10_MAP_READ_WRITE, 0, &mt));
        if (mt.pData) {
            CHECK(mt.RowPitch >= 16); CHECK(mt.DepthPitch >= mt.RowPitch * 4);
            for (UINT z = 0; z < 3; z++) memset((BYTE *)mt.pData + z * mt.DepthPitch, 0x20 + z, 16);
            ID3D10Texture3D_Unmap(t, 0);
            EXPECT_HR_OK(ID3D10Texture3D_Map(t, 0, D3D10_MAP_READ, 0, &mt));
            if (mt.pData) for (UINT z = 0; z < 3; z++) CHECK(((BYTE *)mt.pData)[z * mt.DepthPitch] == 0x20 + z);
            ID3D10Texture3D_Unmap(t, 0);
        }
        ID3D10Texture3D_Release(t);
    }
    DD_END(dev);
}
T(map_texture1d_roundtrip)
{
    DD_DEV_R(dev);
    D3D10_TEXTURE1D_DESC td = { 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_USAGE_STAGING, 0, D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE, 0 };
    ID3D10Texture1D *t = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture1D(dev, &td, NULL, &t))) {
        void *m = NULL;
        EXPECT_HR_OK(ID3D10Texture1D_Map(t, 0, D3D10_MAP_WRITE, 0, &m));
        if (m) { memset(m, 0x77, 64); ID3D10Texture1D_Unmap(t, 0); }
        m = NULL;
        EXPECT_HR_OK(ID3D10Texture1D_Map(t, 0, D3D10_MAP_READ, 0, &m));
        if (m) { CHECK(((BYTE *)m)[63] == 0x77); ID3D10Texture1D_Unmap(t, 0); }
        ID3D10Texture1D_Release(t);
    }
    DD_END(dev);
}
T(map_dynamic_texture_write_discard)
{
    DD_DEV_R(dev);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DYNAMIC, D3D10_BIND_SHADER_RESOURCE, D3D10_CPU_ACCESS_WRITE, 0 };
    ID3D10Texture2D *t = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(dev, &td, NULL, &t))) {
        D3D10_MAPPED_TEXTURE2D mt; memset(&mt, 0, sizeof mt);
        EXPECT_HR_OK(ID3D10Texture2D_Map(t, 0, D3D10_MAP_WRITE_DISCARD, 0, &mt));
        CHECK(mt.pData != NULL); CHECK(mt.RowPitch >= 32);
        if (mt.pData) ID3D10Texture2D_Unmap(t, 0);
        D3D10_MAPPED_TEXTURE2D m2;
        HRESULT hr = ID3D10Texture2D_Map(t, 0, D3D10_MAP_READ, 0, &m2);
        CHECK(FAILED(hr)); if (SUCCEEDED(hr)) ID3D10Texture2D_Unmap(t, 0);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(copy_resource_buffer)
{
    DD_DEV_R(dev);
    BYTE in[128]; for (int i = 0; i < 128; i++) in[i] = (BYTE)(i * 5 + 1);
    ID3D10Buffer *a = mkbuf(dev, 128, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, in), *b = mkbuf(dev, 128, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, NULL);
    if (a && b) {
        void *out = NULL; size_t sz = 0;
        ID3D10Device_CopyResource(dev, (ID3D10Resource *)b, (ID3D10Resource *)a);
        if (SUCCEEDED(d3dh_readback_buffer10(dev, b, &out, &sz))) { CHECK(sz == 128 && !memcmp(out, in, 128)); free(out); }
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DD_END(dev);
}
T(copy_subresource_region_buffer_box)
{
    DD_DEV_R(dev);
    BYTE in[64]; for (int i = 0; i < 64; i++) in[i] = (BYTE)(i + 1);
    BYTE zero[64] = { 0 };
    ID3D10Buffer *a = mkbuf(dev, 64, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, in), *b = mkbuf(dev, 64, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, zero);
    if (a && b) {
        D3D10_BOX box = { 8, 0, 0, 24, 1, 1 }; void *out = NULL; size_t sz = 0;
        ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)b, 0, 32, 0, 0, (ID3D10Resource *)a, 0, &box);
        if (SUCCEEDED(d3dh_readback_buffer10(dev, b, &out, &sz))) {
            BYTE want[64] = { 0 }; memcpy(want + 32, in + 8, 16);
            CHECK(!memcmp(out, want, 64)); free(out);
        }
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DD_END(dev);
}
T(copy_subresource_region_same_buffer_observed)
{
    DD_DEV_R(dev);
    BYTE in[64]; for (int i = 0; i < 64; i++) in[i] = (BYTE)(i + 1);
    ID3D10Buffer *a = mkbuf(dev, 64, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, in);
    if (a) {
        D3D10_BOX box = { 0, 0, 0, 16, 1, 1 }; void *out = NULL; size_t sz = 0;
        ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)a, 0, 32, 0, 0, (ID3D10Resource *)a, 0, &box);
        if (SUCCEEDED(d3dh_readback_buffer10(dev, a, &out, &sz))) {
            BYTE want[64]; memcpy(want, in, 64); memcpy(want + 32, in, 16);
            t_note("same-resource non-overlapping copy %s the expected result", !memcmp(out, want, 64) ? "produced" : "did not produce"); free(out);
        }
        a->lpVtbl->Release(a);
    }
    DD_END(dev);
}
static UINT32 g_z[256];
static ID3D10Texture2D *mkpat(ID3D10Device *dev, UINT w, UINT h, UINT mips, UINT arr, UINT bind, UINT32 *pat)
{
    D3D10_TEXTURE2D_DESC td = { w, h, mips, arr, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, bind, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd[16]; ID3D10Texture2D *t = NULL;
    UINT n = mips * arr; if (n > 16) n = 16;
    for (UINT i = 0; i < n; i++) { UINT m = i % mips; UINT mw = w >> m; if (!mw) mw = 1; sd[i].pSysMem = pat; sd[i].SysMemPitch = mw * 4; sd[i].SysMemSlicePitch = 0; }
    if (FAILED(ID3D10Device_CreateTexture2D(dev, &td, pat ? sd : NULL, &t))) return NULL;
    return t;
}
T(copy_subresource_region_texture_box)
{
    DD_DEV_R(dev);
    UINT32 pat[64]; for (int i = 0; i < 64; i++) pat[i] = 0xff000000u | (UINT)i | ((UINT)(i * 3) << 8);
    ID3D10Texture2D *src = mkpat(dev, 8, 8, 1, 1, D3D10_BIND_SHADER_RESOURCE, pat), *dst = mkpat(dev, 8, 8, 1, 1, D3D10_BIND_SHADER_RESOURCE, g_z);
    if (src && dst) {
        D3D10_BOX box = { 2, 1, 0, 6, 4, 1 }; d3dh_image im;
        ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)dst, 0, 3, 4, 0, (ID3D10Resource *)src, 0, &box);
        if (SUCCEEDED(d3dh_readback_texture2d10(dev, dst, &im))) {
            UINT bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
                UINT32 want = 0;
                if (x >= 3 && x < 7 && y >= 4 && y < 7) want = pat[(y - 4 + 1) * 8 + (x - 3 + 2)];
                if (px(&im, x, y) != want) bad++;
            }
            t_check(!bad, __FILE__, __LINE__, "texture box copy", "%u pixels differ", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(src); SAFE_RELEASE(dst);
    DD_END(dev);
}
T(copy_resource_texture_mips_and_array)
{
    DD_DEV_R(dev);
    UINT32 pat[64]; for (int i = 0; i < 64; i++) pat[i] = 0xff102030u + (UINT)i;
    ID3D10Texture2D *src = mkpat(dev, 8, 8, 4, 2, D3D10_BIND_SHADER_RESOURCE, pat), *dst = mkpat(dev, 8, 8, 4, 2, D3D10_BIND_SHADER_RESOURCE, NULL);
    if (src && dst) {
        ID3D10Device_CopyResource(dev, (ID3D10Resource *)dst, (ID3D10Resource *)src);
        for (UINT sub = 0; sub < 8; sub++) {
            UINT m = sub % 4, w = 8 >> m; D3D10_TEXTURE2D_DESC sd = { w, w, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_STAGING, 0, D3D10_CPU_ACCESS_READ, 0 };
            ID3D10Texture2D *st = NULL;
            if (FAILED(ID3D10Device_CreateTexture2D(dev, &sd, NULL, &st))) continue;
            ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)st, 0, 0, 0, 0, (ID3D10Resource *)dst, sub, NULL);
            D3D10_MAPPED_TEXTURE2D mt;
            if (SUCCEEDED(ID3D10Texture2D_Map(st, 0, D3D10_MAP_READ, 0, &mt))) {
                UINT bad = 0;
                for (UINT y = 0; y < w; y++) for (UINT x = 0; x < w; x++) if (((UINT32 *)((BYTE *)mt.pData + y * mt.RowPitch))[x] != pat[y * w + x]) bad++;
                t_check(!bad, __FILE__, __LINE__, "mip/array copy", "subresource %u: %u texels differ", sub, bad);
                ID3D10Texture2D_Unmap(st, 0);
            }
            ID3D10Texture2D_Release(st);
        }
    }
    SAFE_RELEASE(src); SAFE_RELEASE(dst);
    DD_END(dev);
}
T(copy_subresource_to_single_array_slice)
{
    DD_DEV_R(dev);
    UINT32 pat[16]; for (int i = 0; i < 16; i++) pat[i] = 0xff00ff00u ^ (UINT)i;
    ID3D10Texture2D *src = mkpat(dev, 4, 4, 1, 1, D3D10_BIND_SHADER_RESOURCE, pat), *dst = mkpat(dev, 4, 4, 1, 3, D3D10_BIND_SHADER_RESOURCE, g_z);
    if (src && dst) {
        ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)dst, 2, 0, 0, 0, (ID3D10Resource *)src, 0, NULL);
        D3D10_TEXTURE2D_DESC sd = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_STAGING, 0, D3D10_CPU_ACCESS_READ, 0 };
        ID3D10Texture2D *st = NULL;
        if (SUCCEEDED(ID3D10Device_CreateTexture2D(dev, &sd, NULL, &st))) {
            for (UINT slice = 0; slice < 3; slice++) {
                D3D10_MAPPED_TEXTURE2D mt;
                ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)st, 0, 0, 0, 0, (ID3D10Resource *)dst, slice, NULL);
                if (SUCCEEDED(ID3D10Texture2D_Map(st, 0, D3D10_MAP_READ, 0, &mt))) {
                    UINT32 p00 = *(UINT32 *)mt.pData;
                    t_check(p00 == (slice == 2 ? pat[0] : 0), __FILE__, __LINE__, "slice", "slice %u first texel 0x%08x", slice, p00);
                    ID3D10Texture2D_Unmap(st, 0);
                }
            }
            ID3D10Texture2D_Release(st);
        }
    }
    SAFE_RELEASE(src); SAFE_RELEASE(dst);
    DD_END(dev);
}
T(update_subresource_texture_box_and_pitch)
{
    DD_DEV_R(dev);
    ID3D10Texture2D *t = mkpat(dev, 8, 8, 1, 1, D3D10_BIND_SHADER_RESOURCE, g_z);
    if (t) {
        UINT32 block[3 * 4]; for (int i = 0; i < 12; i++) block[i] = 0xff000000u | (UINT)(i + 1) * 0x010101;
        D3D10_BOX box = { 1, 2, 0, 4, 6, 1 }; d3dh_image im;
        ID3D10Device_UpdateSubresource(dev, (ID3D10Resource *)t, 0, &box, block, 3 * 4, 0);
        if (SUCCEEDED(d3dh_readback_texture2d10(dev, t, &im))) {
            UINT bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
                UINT32 want = 0;
                if (x >= 1 && x < 4 && y >= 2 && y < 6) want = block[(y - 2) * 3 + (x - 1)];
                if (px(&im, x, y) != want) bad++;
            }
            t_check(!bad, __FILE__, __LINE__, "update box", "%u pixels differ", bad);
            d3dh_image_free(&im);
        }
        D3D10_BOX b2 = { 0, 0, 0, 2, 2, 1 }; UINT32 q[4] = { 0xff0000ffu, 0xff00ff00u, 0xffff0000u, 0xffffffffu };
        ID3D10Device_UpdateSubresource(dev, (ID3D10Resource *)t, 0, &b2, q, 8, 0);
        if (SUCCEEDED(d3dh_readback_texture2d10(dev, t, &im))) { PX(im, 0, 0, q[0]); PX(im, 1, 0, q[1]); PX(im, 0, 1, q[2]); PX(im, 1, 1, q[3]); d3dh_image_free(&im); }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(update_subresource_buffer_box)
{
    DD_DEV_R(dev);
    BYTE zero[64] = { 0 };
    ID3D10Buffer *b = mkbuf(dev, 64, D3D10_BIND_VERTEX_BUFFER, D3D10_USAGE_DEFAULT, 0, zero);
    if (b) {
        BYTE patch[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }; D3D10_BOX box = { 20, 0, 0, 28, 1, 1 }; void *out = NULL; size_t sz = 0;
        ID3D10Device_UpdateSubresource(dev, (ID3D10Resource *)b, 0, &box, patch, 0, 0);
        if (SUCCEEDED(d3dh_readback_buffer10(dev, b, &out, &sz))) {
            BYTE want[64] = { 0 }; memcpy(want + 20, patch, 8);
            CHECK(!memcmp(out, want, 64)); free(out);
        }
        ID3D10Device_UpdateSubresource(dev, (ID3D10Resource *)b, 0, NULL, zero, 0, 0);
        if (SUCCEEDED(d3dh_readback_buffer10(dev, b, &out, &sz))) { CHECK(!memcmp(out, zero, 64)); free(out); }
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(update_subresource_constant_buffer_with_box_observed)
{
    DD_DEV_R(dev);
    ID3D10Buffer *b = mkbuf(dev, 64, D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DEFAULT, 0, NULL);
    if (b) {
        BYTE patch[16] = { 0 }; D3D10_BOX box = { 0, 0, 0, 16, 1, 1 };
        ID3D10Device_UpdateSubresource(dev, (ID3D10Resource *)b, 0, &box, patch, 0, 0);
        ID3D10Device_UpdateSubresource(dev, (ID3D10Resource *)b, 0, NULL, patch, 0, 0);
        t_note("constant buffer updated with and without a box");
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}

/* ---- resolve / mips ---- */
T(resolve_uniform_clear)
{
    FX(f, 8, 8);
    UINT q = 0; ID3D10Device_CheckMultisampleQualityLevels(f.dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4, &q);
    if (!q) { fx_done(&f); t_skip("no 4x MSAA"); return; }
    D3D10_TEXTURE2D_DESC ms = { 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 4, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET, 0, 0 };
    ID3D10Texture2D *mst = NULL; ID3D10RenderTargetView *mrt = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(f.dev, &ms, NULL, &mst)) && SUCCEEDED(ID3D10Device_CreateRenderTargetView(f.dev, (ID3D10Resource *)mst, NULL, &mrt))) {
        float c[4] = { 0.0f, 1.0f, 0.0f, 1.0f }; d3dh_image im;
        ID3D10Device_ClearRenderTargetView(f.dev, mrt, c);
        ID3D10Device_ResolveSubresource(f.dev, (ID3D10Resource *)f.rt, 0, (ID3D10Resource *)mst, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        if (fx_read(&f, &im)) { ALL(im, GREEN); d3dh_image_free(&im); }
    }
    SAFE_RELEASE(mrt); SAFE_RELEASE(mst);
    fx_done(&f);
}
T(resolve_msaa_draw_full_coverage)
{
    FX(f, 8, 8);
    UINT q = 0; ID3D10Device_CheckMultisampleQualityLevels(f.dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4, &q);
    if (!q) { fx_done(&f); t_skip("no 4x MSAA"); return; }
    D3D10_TEXTURE2D_DESC ms = { 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 4, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET, 0, 0 };
    ID3D10Texture2D *mst = NULL; ID3D10RenderTargetView *mrt = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(f.dev, &ms, NULL, &mst)) && SUCCEEDED(ID3D10Device_CreateRenderTargetView(f.dev, (ID3D10Resource *)mst, NULL, &mrt))) {
        float c[4] = { 0, 0, 0, 1 }; d3dh_image im;
        ID3D10Device_OMSetRenderTargets(f.dev, 1, &mrt, NULL);
        ID3D10Device_ClearRenderTargetView(f.dev, mrt, c);
        if (draw_fs(&f, DD_PS_RED)) {
            ID3D10Device_ResolveSubresource(f.dev, (ID3D10Resource *)f.rt, 0, (ID3D10Resource *)mst, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
            if (fx_read(&f, &im)) { ALL(im, RED); d3dh_image_free(&im); }
        }
    }
    SAFE_RELEASE(mrt); SAFE_RELEASE(mst);
    fx_done(&f);
}
T(generate_mips_uniform)
{
    DD_DEV_R(dev);
    D3D10_TEXTURE2D_DESC td = { 16, 16, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS };
    ID3D10Texture2D *t = NULL; ID3D10RenderTargetView *rtv = NULL; ID3D10ShaderResourceView *srv = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(dev, &td, NULL, &t)) && SUCCEEDED(ID3D10Device_CreateRenderTargetView(dev, (ID3D10Resource *)t, NULL, &rtv)) &&
        SUCCEEDED(ID3D10Device_CreateShaderResourceView(dev, (ID3D10Resource *)t, NULL, &srv))) {
        float c[4] = { 0.5f, 0.25f, 1.0f, 1.0f }; D3D10_TEXTURE2D_DESC g; ID3D10Texture2D_GetDesc(t, &g);
        EXPECT_EQ(g.MipLevels, 5);
        ID3D10Device_ClearRenderTargetView(dev, rtv, c);
        ID3D10Device_GenerateMips(dev, srv);
        UINT32 base = 0;
        for (UINT m = 0; m < g.MipLevels; m++) {
            UINT w = 16 >> m; D3D10_TEXTURE2D_DESC sd = { w, w, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_STAGING, 0, D3D10_CPU_ACCESS_READ, 0 };
            ID3D10Texture2D *st = NULL; D3D10_MAPPED_TEXTURE2D mt;
            if (FAILED(ID3D10Device_CreateTexture2D(dev, &sd, NULL, &st))) continue;
            ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)st, 0, 0, 0, 0, (ID3D10Resource *)t, m, NULL);
            if (SUCCEEDED(ID3D10Texture2D_Map(st, 0, D3D10_MAP_READ, 0, &mt))) {
                UINT bad = 0; UINT32 first = *(UINT32 *)mt.pData; if (!m) base = first;
                for (UINT y = 0; y < w; y++) for (UINT x = 0; x < w; x++) if (!near8(((UINT32 *)((BYTE *)mt.pData + y * mt.RowPitch))[x], base, 1)) bad++;
                t_check(!bad, __FILE__, __LINE__, "mip contents", "mip %u: %u texels differ from 0x%08x", m, bad, base);
                ID3D10Texture2D_Unmap(st, 0);
            }
            ID3D10Texture2D_Release(st);
        }
    } else t_skip("generate-mips texture not creatable");
    SAFE_RELEASE(srv); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    DD_END(dev);
}
T(generate_mips_two_by_two_average)
{
    DD_DEV_R(dev);
    D3D10_TEXTURE2D_DESC td = { 2, 2, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS };
    ID3D10Texture2D *t = NULL; ID3D10ShaderResourceView *srv = NULL;
    if (SUCCEEDED(ID3D10Device_CreateTexture2D(dev, &td, NULL, &t)) && SUCCEEDED(ID3D10Device_CreateShaderResourceView(dev, (ID3D10Resource *)t, NULL, &srv))) {
        UINT32 px4[4] = { 0xff000000u, 0xffffffffu, 0xffffffffu, 0xff000000u };
        D3D10_TEXTURE2D_DESC sd = { 1, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D10_USAGE_STAGING, 0, D3D10_CPU_ACCESS_READ, 0 };
        ID3D10Texture2D *st = NULL; D3D10_MAPPED_TEXTURE2D mt;
        ID3D10Device_UpdateSubresource(dev, (ID3D10Resource *)t, 0, NULL, px4, 8, 0);
        ID3D10Device_GenerateMips(dev, srv);
        if (SUCCEEDED(ID3D10Device_CreateTexture2D(dev, &sd, NULL, &st))) {
            ID3D10Device_CopySubresourceRegion(dev, (ID3D10Resource *)st, 0, 0, 0, 0, (ID3D10Resource *)t, 1, NULL);
            if (SUCCEEDED(ID3D10Texture2D_Map(st, 0, D3D10_MAP_READ, 0, &mt))) {
                UINT32 p = *(UINT32 *)mt.pData;
                t_check(near8(p, 0xff808080u, 1), __FILE__, __LINE__, "2x2 average", "mip1 = 0x%08x want ~0xff808080", p);
                ID3D10Texture2D_Unmap(st, 0);
            }
            ID3D10Texture2D_Release(st);
        }
    } else t_skip("generate-mips texture not creatable");
    SAFE_RELEASE(srv); SAFE_RELEASE(t);
    DD_END(dev);
}
