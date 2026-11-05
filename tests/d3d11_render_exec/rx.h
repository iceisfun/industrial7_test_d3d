/* Local helpers for the d3d11_render_exec area: device + render-target scaffolding, tiny pixel/buffer readers. */
#ifndef RX_H
#define RX_H
#include "d3dh.h"
#include <d3d11_1.h>
#include <math.h>

/* Stringify HLSL written as ordinary tokens (no // comments, no # directives inside). */
#define HL(...) #__VA_ARGS__

#define RGBA8(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

typedef struct rx {
    ID3D11Device *dev;
    ID3D11DeviceContext *ctx;
    D3D_FEATURE_LEVEL fl;
    D3D_DRIVER_TYPE drv;
    ID3D11VertexShader *vsq;     /* rectangle from SV_VertexID + cb (triangle strip, 4 vertices) */
    ID3D11PixelShader *psc;      /* constant colour from cb b0 */
    ID3D11Buffer *cbv, *cbp;
    ID3D11Texture2D *rt;
    ID3D11RenderTargetView *rtv;
    UINT w, h;
    ID3D11RasterizerState *rs;
    ID3D11BlendState *bs;
    ID3D11DepthStencilState *dss;
    ID3D11Texture2D *ds;
    ID3D11DepthStencilView *dsv;
} rx;

static const char RX_VSQ[] = HL(
    cbuffer VC : register(b0) { float4 rect; float4 zw; };
    float4 main(uint id : SV_VertexID) : SV_Position {
        float x = (id & 1) ? rect.z : rect.x;
        float y = (id & 2) ? rect.w : rect.y;
        return float4(x, y, zw.x, 1);
    });
static const char RX_PSC[] = HL(
    cbuffer PC : register(b0) { float4 c; };
    float4 main(float4 p : SV_Position) : SV_Target { return c; });

static ID3DBlob *rx_blob_ex(const D3D_SHADER_MACRO *defs, const char *src, const char *target, UINT flags)
{
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile_ex(src, "rx_src", defs, "main", target, flags, 0, &b);
    if (FAILED(hr)) {
        if (t_is_wine()) t_skip("wine compiler cannot build %s shader: %s", target, d3dh_hr_name(hr));
        else CHECKF(0, "compile %s failed %s: %s", target, d3dh_hr_name(hr), d3dh_compile_errors());
        return NULL;
    }
    return b;
}
static ID3DBlob *rx_blob(const char *src, const char *target) { return rx_blob_ex(NULL, src, target, 0); }

#define RX_MKSHADER(NAME, IFACE, CREATE, TARGET) \
static IFACE *NAME(rx *r, const char *src) { \
    ID3DBlob *b = rx_blob(src, TARGET); IFACE *s = NULL; \
    if (!b) return NULL; \
    HRESULT hr = CREATE(r->dev, d3dh_blob_ptr(b), d3dh_blob_size(b), NULL, &s); \
    EXPECT_HR_OK(hr); SAFE_RELEASE(b); return s; }
RX_MKSHADER(rx_vs, ID3D11VertexShader, ID3D11Device_CreateVertexShader, "vs_5_0")
RX_MKSHADER(rx_ps, ID3D11PixelShader, ID3D11Device_CreatePixelShader, "ps_5_0")
RX_MKSHADER(rx_cs, ID3D11ComputeShader, ID3D11Device_CreateComputeShader, "cs_5_0")
RX_MKSHADER(rx_gs, ID3D11GeometryShader, ID3D11Device_CreateGeometryShader, "gs_5_0")
RX_MKSHADER(rx_hs, ID3D11HullShader, ID3D11Device_CreateHullShader, "hs_5_0")
RX_MKSHADER(rx_ds, ID3D11DomainShader, ID3D11Device_CreateDomainShader, "ds_5_0")

static void rx_close(rx *r)
{
    if (r->ctx) { ID3D11DeviceContext_ClearState(r->ctx); ID3D11DeviceContext_Flush(r->ctx); }
    SAFE_RELEASE(r->rs); SAFE_RELEASE(r->bs); SAFE_RELEASE(r->dss);
    SAFE_RELEASE(r->dsv); SAFE_RELEASE(r->ds);
    SAFE_RELEASE(r->rtv); SAFE_RELEASE(r->rt);
    SAFE_RELEASE(r->cbv); SAFE_RELEASE(r->cbp); SAFE_RELEASE(r->vsq); SAFE_RELEASE(r->psc);
    SAFE_RELEASE(r->ctx); SAFE_RELEASE(r->dev);
}

static ID3D11Buffer *rx_buffer(rx *r, UINT bytes, UINT bind, UINT misc, UINT stride, D3D11_USAGE usage, UINT cpu, const void *init)
{
    D3D11_BUFFER_DESC d; D3D11_SUBRESOURCE_DATA sd; ID3D11Buffer *b = NULL;
    memset(&d, 0, sizeof d);
    d.ByteWidth = bytes; d.Usage = usage; d.BindFlags = bind; d.CPUAccessFlags = cpu; d.MiscFlags = misc; d.StructureByteStride = stride;
    sd.pSysMem = init; sd.SysMemPitch = 0; sd.SysMemSlicePitch = 0;
    HRESULT hr = ID3D11Device_CreateBuffer(r->dev, &d, init ? &sd : NULL, &b);
    EXPECT_HR_OK(hr);
    return SUCCEEDED(hr) ? b : NULL;
}
static ID3D11Buffer *rx_cbuf(rx *r, UINT bytes) { return rx_buffer(r, bytes, D3D11_BIND_CONSTANT_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, NULL); }

static ID3D11Texture2D *rx_tex2d(rx *r, UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT fmt, UINT samples, UINT bind, UINT misc, const D3D11_SUBRESOURCE_DATA *init)
{
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *t = NULL;
    memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = arr; d.Format = fmt;
    d.SampleDesc.Count = samples; d.SampleDesc.Quality = 0; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind; d.MiscFlags = misc;
    HRESULT hr = ID3D11Device_CreateTexture2D(r->dev, &d, init, &t);
    EXPECT_HR_OK(hr);
    return SUCCEEDED(hr) ? t : NULL;
}

static int rx_open(rx *r)
{
    memset(r, 0, sizeof *r);
    HRESULT hr = d3dh_make_device11(&r->dev, &r->ctx, &r->fl, &r->drv, 0);
    if (FAILED(hr)) { t_skip("no D3D11 device: %s", d3dh_hr_name(hr)); return 0; }
    if (r->fl < D3D_FEATURE_LEVEL_11_0) { t_skip("device feature level 0x%x below 11_0", (unsigned)r->fl); rx_close(r); return 0; }
    r->vsq = rx_vs(r, RX_VSQ);
    r->psc = rx_ps(r, RX_PSC);
    r->cbv = rx_cbuf(r, 32);
    r->cbp = rx_cbuf(r, 16);
    if (!r->vsq || !r->psc || !r->cbv || !r->cbp) { rx_close(r); return 0; }
    return 1;
}

static void rx_viewport(rx *r, float x, float y, float w, float h, float zmin, float zmax)
{
    D3D11_VIEWPORT vp; vp.TopLeftX = x; vp.TopLeftY = y; vp.Width = w; vp.Height = h; vp.MinDepth = zmin; vp.MaxDepth = zmax;
    ID3D11DeviceContext_RSSetViewports(r->ctx, 1, &vp);
}

/* Create a w x h render target of fmt (single sample), bind it (optionally with dsv), full viewport. */
static int rx_target_ms(rx *r, UINT w, UINT h, DXGI_FORMAT fmt, UINT samples, UINT extra_bind)
{
    SAFE_RELEASE(r->rtv); SAFE_RELEASE(r->rt);
    r->rt = rx_tex2d(r, w, h, 1, 1, fmt, samples, D3D11_BIND_RENDER_TARGET | extra_bind, 0, NULL);
    if (!r->rt) return 0;
    HRESULT hr = ID3D11Device_CreateRenderTargetView(r->dev, (ID3D11Resource *)r->rt, NULL, &r->rtv);
    EXPECT_HR_OK(hr);
    if (FAILED(hr)) return 0;
    r->w = w; r->h = h;
    ID3D11DeviceContext_OMSetRenderTargets(r->ctx, 1, &r->rtv, NULL);
    rx_viewport(r, 0, 0, (float)w, (float)h, 0, 1);
    return 1;
}
static int rx_target(rx *r, UINT w, UINT h, DXGI_FORMAT fmt)
{
    if (!rx_target_ms(r, w, h, fmt, 1, 0)) return 0;
    static const float z[4] = { 0, 0, 0, 0 };
    ID3D11DeviceContext_ClearRenderTargetView(r->ctx, r->rtv, z);
    return 1;
}
static void rx_clear(rx *r, float cr, float cg, float cb, float ca)
{
    float c[4]; c[0] = cr; c[1] = cg; c[2] = cb; c[3] = ca;
    ID3D11DeviceContext_ClearRenderTargetView(r->ctx, r->rtv, c);
}

static void rx_set_rs(rx *r, D3D11_FILL_MODE fill, D3D11_CULL_MODE cull, BOOL ccw, BOOL depthclip, BOOL scissor)
{
    D3D11_RASTERIZER_DESC d; memset(&d, 0, sizeof d);
    d.FillMode = fill; d.CullMode = cull; d.FrontCounterClockwise = ccw; d.DepthClipEnable = depthclip; d.ScissorEnable = scissor;
    SAFE_RELEASE(r->rs);
    HRESULT hr = ID3D11Device_CreateRasterizerState(r->dev, &d, &r->rs);
    EXPECT_HR_OK(hr);
    ID3D11DeviceContext_RSSetState(r->ctx, r->rs);
}
static void rx_set_rs_desc(rx *r, const D3D11_RASTERIZER_DESC *d)
{
    SAFE_RELEASE(r->rs);
    HRESULT hr = ID3D11Device_CreateRasterizerState(r->dev, d, &r->rs);
    EXPECT_HR_OK(hr);
    ID3D11DeviceContext_RSSetState(r->ctx, r->rs);
}
static void rx_set_bs(rx *r, const D3D11_BLEND_DESC *d, const float f[4], UINT mask)
{
    SAFE_RELEASE(r->bs);
    HRESULT hr = ID3D11Device_CreateBlendState(r->dev, d, &r->bs);
    EXPECT_HR_OK(hr);
    ID3D11DeviceContext_OMSetBlendState(r->ctx, r->bs, f, mask);
}
static void rx_set_dss(rx *r, const D3D11_DEPTH_STENCIL_DESC *d, UINT ref)
{
    SAFE_RELEASE(r->dss);
    HRESULT hr = ID3D11Device_CreateDepthStencilState(r->dev, d, &r->dss);
    EXPECT_HR_OK(hr);
    ID3D11DeviceContext_OMSetDepthStencilState(r->ctx, r->dss, ref);
}
static void rx_blend_default(D3D11_BLEND_DESC *d)
{
    memset(d, 0, sizeof *d);
    for (int i = 0; i < 8; i++) {
        d->RenderTarget[i].SrcBlend = D3D11_BLEND_ONE; d->RenderTarget[i].DestBlend = D3D11_BLEND_ZERO; d->RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        d->RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE; d->RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO; d->RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        d->RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
}
static void rx_dss_default(D3D11_DEPTH_STENCIL_DESC *d)
{
    memset(d, 0, sizeof *d);
    d->DepthEnable = TRUE; d->DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d->DepthFunc = D3D11_COMPARISON_LESS;
    d->StencilReadMask = 0xFF; d->StencilWriteMask = 0xFF;
    d->FrontFace.StencilFailOp = d->FrontFace.StencilDepthFailOp = d->FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    d->FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS; d->BackFace = d->FrontFace;
}

/* Set up (but do not draw) an axis aligned pixel rectangle [x0,x1) x [y0,y1) of the current target at depth z: VS, cb, topology. */
static void rx_rect_vs(rx *r, float x0, float y0, float x1, float y1, float z)
{
    float vc[8];
    vc[0] = x0 / r->w * 2.0f - 1.0f; vc[1] = 1.0f - y0 / r->h * 2.0f;
    vc[2] = x1 / r->w * 2.0f - 1.0f; vc[3] = 1.0f - y1 / r->h * 2.0f;
    vc[4] = z; vc[5] = vc[6] = vc[7] = 0;
    ID3D11DeviceContext_UpdateSubresource(r->ctx, (ID3D11Resource *)r->cbv, 0, NULL, vc, 0, 0);
    ID3D11DeviceContext_IASetInputLayout(r->ctx, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(r->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_VSSetShader(r->ctx, r->vsq, NULL, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(r->ctx, 0, 1, &r->cbv);
}
static void rx_full_vs(rx *r, float z) { rx_rect_vs(r, 0, 0, (float)r->w, (float)r->h, z); }
/* Draw the rectangle with a flat colour. */
static void rx_rect(rx *r, float x0, float y0, float x1, float y1, float z, const float col[4])
{
    rx_rect_vs(r, x0, y0, x1, y1, z);
    ID3D11DeviceContext_UpdateSubresource(r->ctx, (ID3D11Resource *)r->cbp, 0, NULL, col, 0, 0);
    ID3D11DeviceContext_PSSetShader(r->ctx, r->psc, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(r->ctx, 0, 1, &r->cbp);
    ID3D11DeviceContext_Draw(r->ctx, 4, 0);
}
static void rx_full(rx *r, float z, const float col[4]) { rx_rect(r, 0, 0, (float)r->w, (float)r->h, z, col); }

static int rx_read(rx *r, ID3D11Texture2D *t, UINT sub, d3dh_image *im)
{
    HRESULT hr = d3dh_readback_texture2d11(r->dev, r->ctx, t, sub, im);
    EXPECT_HR_OK(hr);
    return SUCCEEDED(hr);
}
/* Read the render target; on success *im is valid and must be freed. */
static int rx_read_rt(rx *r, d3dh_image *im) { return rx_read(r, r->rt, 0, im); }

/* count pixels equal to v in a 4-byte-per-pixel image */
static unsigned rx_count32(const d3dh_image *im, uint32_t v)
{
    unsigned n = 0;
    for (UINT y = 0; y < im->height; y++) for (UINT x = 0; x < im->width; x++) if (d3dh_image_pixel32(im, x, y) == v) n++;
    return n;
}
/* every pixel must equal v; reports the first difference */
static void rx_expect_all32(const d3dh_image *im, uint32_t v)
{
    unsigned bad = 0, fx = 0, fy = 0; uint32_t got = 0;
    for (UINT y = 0; y < im->height; y++) for (UINT x = 0; x < im->width; x++) {
        uint32_t p = d3dh_image_pixel32(im, x, y);
        if (p != v) { if (!bad) { fx = x; fy = y; got = p; } bad++; }
    }
    CHECKF(bad == 0, "%u of %ux%u pixels differ from 0x%08x; first at (%u,%u) = 0x%08x", bad, im->width, im->height, v, fx, fy, got);
}
/* byte-wise closeness for 4x8-bit pixels */
static int rx_close32(uint32_t a, uint32_t b, int tol)
{
    for (int i = 0; i < 4; i++) { int d = (int)((a >> (8 * i)) & 255) - (int)((b >> (8 * i)) & 255); if (d < 0) d = -d; if (d > tol) return 0; }
    return 1;
}
static float rx_f32(const d3dh_image *im, UINT x, UINT y, UINT comp)
{
    float v; memcpy(&v, im->data + (size_t)y * im->pitch + ((size_t)x * 4 + comp) * 4, 4); return v;
}
static uint32_t rx_u32(const d3dh_image *im, UINT x, UINT y, UINT comp)
{
    uint32_t v; memcpy(&v, im->data + (size_t)y * im->pitch + ((size_t)x * 4 + comp) * 4, 4); return v;
}
static int rx_near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

/* Wait (bounded) for a query result. */
static HRESULT rx_query(rx *r, ID3D11Query *q, void *data, UINT size)
{
    ID3D11DeviceContext_Flush(r->ctx);
    for (int i = 0; i < 5000; i++) {
        HRESULT hr = ID3D11DeviceContext_GetData(r->ctx, (ID3D11Asynchronous *)q, data, size, 0);
        if (hr == S_OK) return hr;
        if (FAILED(hr)) return hr;
        Sleep(1);
    }
    return S_FALSE;
}
static ID3D11Query *rx_make_query(rx *r, D3D11_QUERY type)
{
    D3D11_QUERY_DESC d; ID3D11Query *q = NULL; d.Query = type; d.MiscFlags = 0;
    HRESULT hr = ID3D11Device_CreateQuery(r->dev, &d, &q);
    EXPECT_HR_OK(hr);
    return SUCCEEDED(hr) ? q : NULL;
}
static void rx_wait_idle(rx *r)
{
    ID3D11Query *q = rx_make_query(r, D3D11_QUERY_EVENT); BOOL done = FALSE;
    if (!q) return;
    ID3D11DeviceContext_End(r->ctx, (ID3D11Asynchronous *)q);
    rx_query(r, q, &done, sizeof done);
    SAFE_RELEASE(q);
}
/* Depth(-stencil) buffer matching the current target size; dsv_fmt is the DSV format. Cleared to depth 1 / stencil 0 and bound with the target. */
static DXGI_FORMAT rx_typeless_of(DXGI_FORMAT f)
{
    switch (f) {
    case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_TYPELESS;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;
    case DXGI_FORMAT_D16_UNORM: return DXGI_FORMAT_R16_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;
    default: return f;
    }
}
static int rx_depth(rx *r, DXGI_FORMAT dsv_fmt, UINT extra_bind)
{
    SAFE_RELEASE(r->dsv); SAFE_RELEASE(r->ds);
    r->ds = rx_tex2d(r, r->w, r->h, 1, 1, rx_typeless_of(dsv_fmt), 1, D3D11_BIND_DEPTH_STENCIL | extra_bind, 0, NULL);
    if (!r->ds) return 0;
    D3D11_DEPTH_STENCIL_VIEW_DESC vd; memset(&vd, 0, sizeof vd);
    vd.Format = dsv_fmt; vd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    HRESULT hr = ID3D11Device_CreateDepthStencilView(r->dev, (ID3D11Resource *)r->ds, &vd, &r->dsv);
    EXPECT_HR_OK(hr);
    if (FAILED(hr)) return 0;
    ID3D11DeviceContext_OMSetRenderTargets(r->ctx, 1, &r->rtv, r->dsv);
    ID3D11DeviceContext_ClearDepthStencilView(r->ctx, r->dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    return 1;
}
static float rx_depth_at(const d3dh_image *im, UINT x, UINT y)
{
    float v; memcpy(&v, im->data + (size_t)y * im->pitch + (size_t)x * 4, 4); return v;
}
static void rx_set_color(rx *r, const float col[4])
{
    ID3D11DeviceContext_UpdateSubresource(r->ctx, (ID3D11Resource *)r->cbp, 0, NULL, col, 0, 0);
    ID3D11DeviceContext_PSSetShader(r->ctx, r->psc, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(r->ctx, 0, 1, &r->cbp);
}
/* VS + input layout from one source */
static ID3D11VertexShader *rx_vs_il(rx *r, const char *src, const D3D11_INPUT_ELEMENT_DESC *el, UINT n, ID3D11InputLayout **il)
{
    ID3DBlob *b = rx_blob(src, "vs_5_0"); ID3D11VertexShader *s = NULL;
    *il = NULL;
    if (!b) return NULL;
    HRESULT hr = ID3D11Device_CreateVertexShader(r->dev, d3dh_blob_ptr(b), d3dh_blob_size(b), NULL, &s);
    EXPECT_HR_OK(hr);
    if (el) { hr = ID3D11Device_CreateInputLayout(r->dev, el, n, d3dh_blob_ptr(b), d3dh_blob_size(b), il); EXPECT_HR_OK(hr); }
    SAFE_RELEASE(b);
    return s;
}
static const float RX_WHITE[4] = { 1, 1, 1, 1 };
static const float RX_RED[4] = { 1, 0, 0, 1 };
static const float RX_GREEN[4] = { 0, 1, 0, 1 };
static const float RX_BLUE[4] = { 0, 0, 1, 1 };
static const float RX_ZERO[4] = { 0, 0, 0, 0 };
#define RX_N(a) ((unsigned)(sizeof(a) / sizeof((a)[0])))
#endif
