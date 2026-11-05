/* Shared fixtures for the d3d11_context_state area. */
#ifndef CSH_H
#define CSH_H
#include "d3dh.h"
#include <d3d11_4.h>

#define CS_ERR_DEFERRED_MAP_NO_DISCARD ((HRESULT)0x887C0004u)

typedef struct {
    ID3D11Device *dev;
    ID3D11DeviceContext *ctx;
    D3D_FEATURE_LEVEL fl;
    D3D_DRIVER_TYPE dt;
} csf;

static int csf_open(csf *f)
{
    memset(f, 0, sizeof *f);
    HRESULT hr = d3dh_make_device11(&f->dev, &f->ctx, &f->fl, &f->dt, 0);
    if (FAILED(hr) || !f->dev || !f->ctx) { t_skip("no D3D11 device: %s", d3dh_hr_name(hr)); return 0; }
    return 1;
}
static void csf_close(csf *f)
{
    if (f->ctx) ID3D11DeviceContext_ClearState(f->ctx);
    SAFE_RELEASE(f->ctx);
    SAFE_RELEASE(f->dev);
}
/* open a fixture or skip the test */
#define OPEN() csf f; if (!csf_open(&f)) return
#define CLOSE() csf_close(&f)
/* skip when the feature level is below the given one */
#define NEED_FL(l) do { if (f.fl < (l)) { t_skip("feature level 0x%x < 0x%x", f.fl, (l)); csf_close(&f); return; } } while (0)

static ID3D11Buffer *cs_buf(csf *f, UINT bind, UINT bytes)
{
    D3D11_BUFFER_DESC d; memset(&d, 0, sizeof d);
    d.ByteWidth = bytes; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    ID3D11Buffer *b = NULL;
    HRESULT hr = ID3D11Device_CreateBuffer(f->dev, &d, NULL, &b);
    if (FAILED(hr)) { t_note("CreateBuffer(bind=0x%x) failed: %s", bind, d3dh_hr_name(hr)); return NULL; }
    return b;
}
static ID3D11Buffer *cs_buf_data(csf *f, UINT bind, UINT bytes, const void *data)
{
    D3D11_BUFFER_DESC d; memset(&d, 0, sizeof d);
    d.ByteWidth = bytes; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    D3D11_SUBRESOURCE_DATA sd; memset(&sd, 0, sizeof sd); sd.pSysMem = data;
    ID3D11Buffer *b = NULL;
    HRESULT hr = ID3D11Device_CreateBuffer(f->dev, &d, data ? &sd : NULL, &b);
    if (FAILED(hr)) { t_note("CreateBuffer failed: %s", d3dh_hr_name(hr)); return NULL; }
    return b;
}
static ID3D11Texture2D *cs_tex(csf *f, UINT w, UINT h, UINT mips, DXGI_FORMAT fmt, UINT bind)
{
    D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = 1; d.Format = fmt;
    d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(f->dev, &d, NULL, &t);
    if (FAILED(hr)) { t_note("CreateTexture2D failed: %s", d3dh_hr_name(hr)); return NULL; }
    return t;
}
static ID3D11ShaderResourceView *cs_srv(csf *f, ID3D11Resource *r)
{
    ID3D11ShaderResourceView *v = NULL;
    HRESULT hr = ID3D11Device_CreateShaderResourceView(f->dev, r, NULL, &v);
    if (FAILED(hr)) { t_note("CreateSRV failed: %s", d3dh_hr_name(hr)); return NULL; }
    return v;
}
static ID3D11RenderTargetView *cs_rtv(csf *f, ID3D11Resource *r, const D3D11_RENDER_TARGET_VIEW_DESC *d)
{
    ID3D11RenderTargetView *v = NULL;
    HRESULT hr = ID3D11Device_CreateRenderTargetView(f->dev, r, d, &v);
    if (FAILED(hr)) { t_note("CreateRTV failed: %s", d3dh_hr_name(hr)); return NULL; }
    return v;
}
static ID3D11SamplerState *cs_sampler(csf *f, D3D11_FILTER filt)
{
    D3D11_SAMPLER_DESC d; memset(&d, 0, sizeof d);
    d.Filter = filt; d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    d.MaxLOD = D3D11_FLOAT32_MAX; d.ComparisonFunc = D3D11_COMPARISON_NEVER; d.MaxAnisotropy = 1;
    ID3D11SamplerState *s = NULL;
    HRESULT hr = ID3D11Device_CreateSamplerState(f->dev, &d, &s);
    if (FAILED(hr)) { t_note("CreateSamplerState failed: %s", d3dh_hr_name(hr)); return NULL; }
    return s;
}
static ID3D11RasterizerState *cs_rs(csf *f, D3D11_CULL_MODE cull, BOOL scissor)
{
    D3D11_RASTERIZER_DESC d; memset(&d, 0, sizeof d);
    d.FillMode = D3D11_FILL_SOLID; d.CullMode = cull; d.DepthClipEnable = TRUE; d.ScissorEnable = scissor;
    ID3D11RasterizerState *s = NULL;
    HRESULT hr = ID3D11Device_CreateRasterizerState(f->dev, &d, &s);
    if (FAILED(hr)) { t_note("CreateRasterizerState failed: %s", d3dh_hr_name(hr)); return NULL; }
    return s;
}
static ID3D11BlendState *cs_bs(csf *f, BOOL blend)
{
    D3D11_BLEND_DESC d; memset(&d, 0, sizeof d);
    d.RenderTarget[0].BlendEnable = blend;
    d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ID3D11BlendState *s = NULL;
    HRESULT hr = ID3D11Device_CreateBlendState(f->dev, &d, &s);
    if (FAILED(hr)) { t_note("CreateBlendState failed: %s", d3dh_hr_name(hr)); return NULL; }
    return s;
}
static ID3D11DepthStencilState *cs_dss(csf *f, BOOL depth)
{
    D3D11_DEPTH_STENCIL_DESC d; memset(&d, 0, sizeof d);
    d.DepthEnable = depth; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D11_COMPARISON_LESS;
    d.StencilEnable = TRUE; d.StencilReadMask = 0xff; d.StencilWriteMask = 0xff;
    d.FrontFace.StencilFunc = d.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    d.FrontFace.StencilPassOp = d.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilFailOp = d.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilDepthFailOp = d.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    ID3D11DepthStencilState *s = NULL;
    HRESULT hr = ID3D11Device_CreateDepthStencilState(f->dev, &d, &s);
    if (FAILED(hr)) { t_note("CreateDepthStencilState failed: %s", d3dh_hr_name(hr)); return NULL; }
    return s;
}

/* ---- shaders ------------------------------------------------------------- */
static const char CS_SRC_VS[] = "float4 main(float4 p : POSITION) : SV_Position { return p; }";
static const char CS_SRC_VSID[] =
    "float4 main(uint vid : SV_VertexID) : SV_Position {"
    " uint id = (vid == 1) ? 2 : ((vid == 2) ? 1 : 0);" /* clockwise winding: survives default back-face culling */
    " float2 p = float2((id << 1) & 2, id & 2); return float4(p * 2.0 - 1.0, 0, 1); }";
static const char CS_SRC_PS[] = "float4 main() : SV_Target { return float4(1,1,1,1); }";
static const char CS_SRC_CS[] = "[numthreads(1,1,1)] void main() { }";
static const char CS_SRC_GS[] =
    "struct V { float4 p : SV_Position; };\n"
    "[maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> s) {"
    " for (int k = 0; k < 3; k++) s.Append(i[k]); }";
static const char CS_SRC_HS[] =
    "struct HC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };\n"
    "struct CP { float4 p : POSITION; };\n"
    "HC pc(InputPatch<CP,3> p) { HC h; h.e[0] = h.e[1] = h.e[2] = 1; h.i = 1; return h; }\n"
    "[domain(\"tri\")][partitioning(\"integer\")][outputtopology(\"triangle_cw\")][outputcontrolpoints(3)][patchconstantfunc(\"pc\")]\n"
    "CP main(InputPatch<CP,3> p, uint i : SV_OutputControlPointID) { return p[i]; }";
static const char CS_SRC_DS[] =
    "struct HC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };\n"
    "struct CP { float4 p : POSITION; };\n"
    "[domain(\"tri\")] float4 main(HC h, float3 l : SV_DomainLocation, const OutputPatch<CP,3> p) : SV_Position {"
    " return p[0].p * l.x + p[1].p * l.y + p[2].p * l.z; }";

static ID3DBlob *cs_blob(const char *src, const char *target)
{
    ID3DBlob *b = NULL;
    if (!d3dh_D3DCompile()) return NULL;
    HRESULT hr = d3dh_compile(src, "main", target, 0, &b);
    if (FAILED(hr)) { t_note("compile %s failed: %s", target, d3dh_hr_name(hr)); return NULL; }
    return b;
}
#define CS_MK(NAME, TYPE, SRC, TARGET, CREATE) \
static TYPE *NAME(csf *f) { \
    TYPE *s = NULL; ID3DBlob *b = cs_blob(SRC, TARGET); if (!b) return NULL; \
    HRESULT hr = CREATE(f->dev, d3dh_blob_ptr(b), d3dh_blob_size(b), NULL, &s); \
    if (FAILED(hr)) { t_note(#NAME " create failed: %s", d3dh_hr_name(hr)); s = NULL; } \
    SAFE_RELEASE(b); return s; }
CS_MK(mk_VS, ID3D11VertexShader,   CS_SRC_VS,   "vs_5_0", ID3D11Device_CreateVertexShader)
CS_MK(mk_VSID, ID3D11VertexShader, CS_SRC_VSID, "vs_5_0", ID3D11Device_CreateVertexShader)
CS_MK(mk_PS, ID3D11PixelShader,    CS_SRC_PS,   "ps_5_0", ID3D11Device_CreatePixelShader)
CS_MK(mk_CS, ID3D11ComputeShader,  CS_SRC_CS,   "cs_5_0", ID3D11Device_CreateComputeShader)
CS_MK(mk_GS, ID3D11GeometryShader, CS_SRC_GS,   "gs_5_0", ID3D11Device_CreateGeometryShader)
CS_MK(mk_HS, ID3D11HullShader,     CS_SRC_HS,   "hs_5_0", ID3D11Device_CreateHullShader)
CS_MK(mk_DS, ID3D11DomainShader,   CS_SRC_DS,   "ds_5_0", ID3D11Device_CreateDomainShader)

static ID3D11InputLayout *cs_layout(csf *f)
{
    ID3DBlob *b = cs_blob(CS_SRC_VS, "vs_5_0"); if (!b) return NULL;
    D3D11_INPUT_ELEMENT_DESC e = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    ID3D11InputLayout *l = NULL;
    HRESULT hr = ID3D11Device_CreateInputLayout(f->dev, &e, 1, d3dh_blob_ptr(b), d3dh_blob_size(b), &l);
    if (FAILED(hr)) { t_note("CreateInputLayout failed: %s", d3dh_hr_name(hr)); l = NULL; }
    SAFE_RELEASE(b);
    return l;
}

/* A refcount delta helper: returns the count of an object without changing it. */
#define RC(p) d3dh_refcount(p)

/* Readback a tightly packed 8x8 RGBA8 pixel; returns 0 on failure. */
static int cs_read_px(csf *f, ID3D11Texture2D *t, UINT x, UINT y, uint32_t *px)
{
    d3dh_image im;
    if (FAILED(d3dh_readback_texture2d11(f->dev, f->ctx, t, 0, &im))) return 0;
    *px = d3dh_image_pixel32(&im, x, y);
    d3dh_image_free(&im);
    return 1;
}
#endif
