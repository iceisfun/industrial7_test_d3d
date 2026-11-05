#include "rc.h"

const GUID RC_GUID_A = {0x6a1c0001, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x01}};
const GUID RC_GUID_B = {0x6a1c0002, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x02}};
const GUID RC_GUID_C = {0x6a1c0003, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x03}};

/* ---- counting object ------------------------------------------------------ */
static HRESULT WINAPI o_qi(IUnknown *This, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown)) { InterlockedIncrement(&((rc_obj *)This)->refs); *ppv = This; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI o_addref(IUnknown *This) { return InterlockedIncrement(&((rc_obj *)This)->refs); }
static ULONG WINAPI o_release(IUnknown *This)
{
    rc_obj *o = (rc_obj *)This;
    LONG r = InterlockedDecrement(&o->refs);
    if (r == 0) InterlockedExchange(&o->destroyed, 1);   /* memory is kept so the test can inspect it */
    return r;
}
static IUnknownVtbl o_vtbl = { o_qi, o_addref, o_release };
rc_obj *rc_obj_new(void)
{
    rc_obj *o = calloc(1, sizeof(*o));
    o->iface.lpVtbl = &o_vtbl; o->refs = 1;
    return o;
}
LONG rc_obj_refs(rc_obj *o) { return o->refs; }
void rc_obj_delete(rc_obj *o) { free(o); }

/* ---- kinds ------------------------------------------------------------------ */
int rc_kind_is_state(int k) { return k == K_BLEND || k == K_RAST || k == K_DSS || k == K_SAMP; }
int rc_kind_is_resource(int k) { return k == K_BUF || k == K_T1D || k == K_T2D || k == K_T3D; }

ID3DBlob *rc_shader_blob(const char *which)
{
    static ID3DBlob *vs, *ps, *cs;
    ID3DBlob **slot = which[0] == 'v' ? &vs : which[0] == 'p' ? &ps : &cs;
    if (!*slot) {
        const char *src, *tgt;
        if (which[0] == 'v') { src = "float4 main(float4 p : POSITION) : SV_Position { return p; }"; tgt = "vs_4_0"; }
        else if (which[0] == 'p') { src = "float4 main(float4 p : SV_Position) : SV_Target { return float4(1,0,0,1); }"; tgt = "ps_4_0"; }
        else { src = "RWByteAddressBuffer b : register(u0); [numthreads(1,1,1)] void main() { b.Store(0, 1); }"; tgt = "cs_5_0"; }
        if (FAILED(d3dh_compile(src, "main", tgt, 0, slot))) *slot = NULL;
    }
    return *slot;
}

static HRESULT make_tex2d(ID3D11Device *dev, UINT bind, DXGI_FORMAT fmt, ID3D11Texture2D **t)
{
    D3D11_TEXTURE2D_DESC d = {16, 16, 1, 1, fmt, {1, 0}, D3D11_USAGE_DEFAULT, bind, 0, 0};
    return ID3D11Device_CreateTexture2D(dev, &d, NULL, t);
}

HRESULT rc_make(int kind, ID3D11Device *dev, ID3D11DeviceChild **out)
{
    HRESULT hr = E_FAIL;
    void *obj = NULL;
    *out = NULL;
    switch (kind) {
    case K_BUF: {
        D3D11_BUFFER_DESC d = {256, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0};
        hr = ID3D11Device_CreateBuffer(dev, &d, NULL, (ID3D11Buffer **)&obj); break; }
    case K_T1D: {
        D3D11_TEXTURE1D_DESC d = {16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0};
        hr = ID3D11Device_CreateTexture1D(dev, &d, NULL, (ID3D11Texture1D **)&obj); break; }
    case K_T2D: hr = make_tex2d(dev, D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM, (ID3D11Texture2D **)&obj); break;
    case K_T3D: {
        D3D11_TEXTURE3D_DESC d = {4, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0};
        hr = ID3D11Device_CreateTexture3D(dev, &d, NULL, (ID3D11Texture3D **)&obj); break; }
    case K_SRV: case K_RTV: {
        ID3D11Texture2D *t;
        hr = make_tex2d(dev, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, DXGI_FORMAT_R8G8B8A8_UNORM, &t);
        if (FAILED(hr)) break;
        if (kind == K_SRV) hr = ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)t, NULL, (ID3D11ShaderResourceView **)&obj);
        else hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)t, NULL, (ID3D11RenderTargetView **)&obj);
        ID3D11Texture2D_Release(t);   /* the view keeps the texture alive */
        break; }
    case K_DSV: {
        ID3D11Texture2D *t;
        hr = make_tex2d(dev, D3D11_BIND_DEPTH_STENCIL, DXGI_FORMAT_D24_UNORM_S8_UINT, &t);
        if (FAILED(hr)) break;
        hr = ID3D11Device_CreateDepthStencilView(dev, (ID3D11Resource *)t, NULL, (ID3D11DepthStencilView **)&obj);
        ID3D11Texture2D_Release(t);
        break; }
    case K_UAV: {
        ID3D11Texture2D *t;
        hr = make_tex2d(dev, D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, &t);
        if (FAILED(hr)) break;
        hr = ID3D11Device_CreateUnorderedAccessView(dev, (ID3D11Resource *)t, NULL, (ID3D11UnorderedAccessView **)&obj);
        ID3D11Texture2D_Release(t);
        break; }
    case K_BLEND: {
        D3D11_BLEND_DESC d; memset(&d, 0, sizeof(d));
        d.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; d.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
        d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        d.RenderTarget[0].RenderTargetWriteMask = 0xf;
        hr = ID3D11Device_CreateBlendState(dev, &d, (ID3D11BlendState **)&obj); break; }
    case K_RAST: {
        D3D11_RASTERIZER_DESC d; memset(&d, 0, sizeof(d));
        d.FillMode = D3D11_FILL_SOLID; d.CullMode = D3D11_CULL_BACK; d.DepthClipEnable = TRUE;
        hr = ID3D11Device_CreateRasterizerState(dev, &d, (ID3D11RasterizerState **)&obj); break; }
    case K_DSS: {
        D3D11_DEPTH_STENCIL_DESC d; memset(&d, 0, sizeof(d));
        d.DepthEnable = TRUE; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D11_COMPARISON_LESS;
        d.StencilReadMask = 0xff; d.StencilWriteMask = 0xff;
        d.FrontFace.StencilFunc = d.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        d.BackFace.StencilFailOp = d.BackFace.StencilDepthFailOp = d.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        hr = ID3D11Device_CreateDepthStencilState(dev, &d, (ID3D11DepthStencilState **)&obj); break; }
    case K_SAMP: {
        D3D11_SAMPLER_DESC d; memset(&d, 0, sizeof(d));
        d.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        d.MaxAnisotropy = 1; d.ComparisonFunc = D3D11_COMPARISON_NEVER; d.MaxLOD = D3D11_FLOAT32_MAX;
        hr = ID3D11Device_CreateSamplerState(dev, &d, (ID3D11SamplerState **)&obj); break; }
    case K_VS: case K_IL: {
        ID3DBlob *b = rc_shader_blob("vs");
        if (!b) { hr = E_FAIL; break; }
        if (kind == K_VS) hr = ID3D11Device_CreateVertexShader(dev, d3dh_blob_ptr(b), d3dh_blob_size(b), NULL, (ID3D11VertexShader **)&obj);
        else {
            D3D11_INPUT_ELEMENT_DESC e = {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
            hr = ID3D11Device_CreateInputLayout(dev, &e, 1, d3dh_blob_ptr(b), d3dh_blob_size(b), (ID3D11InputLayout **)&obj);
        }
        break; }
    case K_PS: {
        ID3DBlob *b = rc_shader_blob("ps");
        if (!b) break;
        hr = ID3D11Device_CreatePixelShader(dev, d3dh_blob_ptr(b), d3dh_blob_size(b), NULL, (ID3D11PixelShader **)&obj); break; }
    case K_CS: {
        ID3DBlob *b = rc_shader_blob("cs");
        if (!b) break;
        hr = ID3D11Device_CreateComputeShader(dev, d3dh_blob_ptr(b), d3dh_blob_size(b), NULL, (ID3D11ComputeShader **)&obj); break; }
    case K_QUERY: case K_PRED: {
        D3D11_QUERY_DESC d = { kind == K_QUERY ? D3D11_QUERY_EVENT : D3D11_QUERY_OCCLUSION_PREDICATE, 0 };
        if (kind == K_QUERY) hr = ID3D11Device_CreateQuery(dev, &d, (ID3D11Query **)&obj);
        else hr = ID3D11Device_CreatePredicate(dev, &d, (ID3D11Predicate **)&obj);
        break; }
    case K_CLINK:
        hr = ID3D11Device_CreateClassLinkage(dev, (ID3D11ClassLinkage **)&obj); break;
    case K_DEFCTX: case K_CMDLIST: {
        ID3D11DeviceContext *dc = NULL;
        hr = ID3D11Device_CreateDeferredContext(dev, 0, &dc);
        if (FAILED(hr)) break;
        if (kind == K_DEFCTX) obj = dc;
        else { hr = ID3D11DeviceContext_FinishCommandList(dc, FALSE, (ID3D11CommandList **)&obj); ID3D11DeviceContext_Release(dc); }
        break; }
    }
    if (FAILED(hr)) return hr;
    if (!obj) return E_FAIL;
    *out = (ID3D11DeviceChild *)obj;
    return S_OK;
}
