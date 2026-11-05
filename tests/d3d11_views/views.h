/* Shared fixtures for the d3d11_views area. Everything resolves through d3dh (LoadLibrary/GetProcAddress). */
#ifndef VIEWS_H
#define VIEWS_H
#include "d3dh.h"
#include <d3d11_3.h>

#define NEED_DEV() \
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0; \
    do { REQUIRE(d3dh_D3D11CreateDevice()); \
         if (FAILED(d3dh_make_device11(&dev, &ctx, &fl, NULL, 0))) { t_skip("no D3D11 device"); return; } } while (0)
#define NEED_FL11() do { if (fl < D3D_FEATURE_LEVEL_11_0) { SAFE_RELEASE(ctx); SAFE_RELEASE(dev); t_skip("needs feature level 11_0"); return; } } while (0)
#define DONE() do { SAFE_RELEASE(ctx); SAFE_RELEASE(dev); } while (0)

#define R(p) ((ID3D11Resource *)(p))
#define MK_SRV(dev, res, desc, pv) ID3D11Device_CreateShaderResourceView(dev, R(res), desc, pv)
#define MK_RTV(dev, res, desc, pv) ID3D11Device_CreateRenderTargetView(dev, R(res), desc, pv)
#define MK_DSV(dev, res, desc, pv) ID3D11Device_CreateDepthStencilView(dev, R(res), desc, pv)
#define MK_UAV(dev, res, desc, pv) ID3D11Device_CreateUnorderedAccessView(dev, R(res), desc, pv)

#define E_INV ((HRESULT)E_INVALIDARG)

static ID3D11Texture2D *mk2d(ID3D11Device *dev, UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT fmt, UINT bind, UINT misc, UINT samples)
{
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *t = NULL;
    memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = arr; d.Format = fmt;
    d.SampleDesc.Count = samples ? samples : 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind; d.MiscFlags = misc;
    if (FAILED(ID3D11Device_CreateTexture2D(dev, &d, NULL, &t))) return NULL;
    return t;
}
static ID3D11Texture1D *mk1d(ID3D11Device *dev, UINT w, UINT mips, UINT arr, DXGI_FORMAT fmt, UINT bind)
{
    D3D11_TEXTURE1D_DESC d; ID3D11Texture1D *t = NULL;
    memset(&d, 0, sizeof d);
    d.Width = w; d.MipLevels = mips; d.ArraySize = arr; d.Format = fmt; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    if (FAILED(ID3D11Device_CreateTexture1D(dev, &d, NULL, &t))) return NULL;
    return t;
}
static ID3D11Texture3D *mk3d(ID3D11Device *dev, UINT w, UINT h, UINT depth, UINT mips, DXGI_FORMAT fmt, UINT bind)
{
    D3D11_TEXTURE3D_DESC d; ID3D11Texture3D *t = NULL;
    memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.Depth = depth; d.MipLevels = mips; d.Format = fmt; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    if (FAILED(ID3D11Device_CreateTexture3D(dev, &d, NULL, &t))) return NULL;
    return t;
}
static ID3D11Buffer *mkbuf(ID3D11Device *dev, UINT bytes, UINT bind, UINT misc, UINT stride)
{
    D3D11_BUFFER_DESC d; ID3D11Buffer *b = NULL;
    memset(&d, 0, sizeof d);
    d.ByteWidth = bytes; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind; d.MiscFlags = misc; d.StructureByteStride = stride;
    if (FAILED(ID3D11Device_CreateBuffer(dev, &d, NULL, &b))) return NULL;
    return b;
}
static int ms_ok(ID3D11Device *dev, DXGI_FORMAT f, UINT n)
{
    UINT q = 0;
    if (FAILED(ID3D11Device_CheckMultisampleQualityLevels(dev, f, n, &q))) return 0;
    return q > 0;
}
static D3D11_SHADER_RESOURCE_VIEW_DESC srvd(D3D11_SRV_DIMENSION dim, DXGI_FORMAT f)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC d; memset(&d, 0, sizeof d); d.Format = f; d.ViewDimension = dim; return d;
}
static D3D11_RENDER_TARGET_VIEW_DESC rtvd(D3D11_RTV_DIMENSION dim, DXGI_FORMAT f)
{
    D3D11_RENDER_TARGET_VIEW_DESC d; memset(&d, 0, sizeof d); d.Format = f; d.ViewDimension = dim; return d;
}
static D3D11_DEPTH_STENCIL_VIEW_DESC dsvd(D3D11_DSV_DIMENSION dim, DXGI_FORMAT f, UINT flags)
{
    D3D11_DEPTH_STENCIL_VIEW_DESC d; memset(&d, 0, sizeof d); d.Format = f; d.ViewDimension = dim; d.Flags = flags; return d;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC uavd(D3D11_UAV_DIMENSION dim, DXGI_FORMAT f)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d); d.Format = f; d.ViewDimension = dim; return d;
}
/* Create-validate-release: returns the HRESULT of a real creation (view released on success). */
#define TRY_SRV(dev, res, desc) try_srv(dev, R(res), desc)
static HRESULT try_srv(ID3D11Device *dev, ID3D11Resource *r, const D3D11_SHADER_RESOURCE_VIEW_DESC *d)
{ ID3D11ShaderResourceView *v = NULL; HRESULT hr = ID3D11Device_CreateShaderResourceView(dev, r, d, &v); if (SUCCEEDED(hr)) { CHECK(v != NULL); SAFE_RELEASE(v); } else CHECK(v == NULL); return hr; }
static HRESULT try_rtv(ID3D11Device *dev, ID3D11Resource *r, const D3D11_RENDER_TARGET_VIEW_DESC *d)
{ ID3D11RenderTargetView *v = NULL; HRESULT hr = ID3D11Device_CreateRenderTargetView(dev, r, d, &v); if (SUCCEEDED(hr)) { CHECK(v != NULL); SAFE_RELEASE(v); } else CHECK(v == NULL); return hr; }
static HRESULT try_dsv(ID3D11Device *dev, ID3D11Resource *r, const D3D11_DEPTH_STENCIL_VIEW_DESC *d)
{ ID3D11DepthStencilView *v = NULL; HRESULT hr = ID3D11Device_CreateDepthStencilView(dev, r, d, &v); if (SUCCEEDED(hr)) { CHECK(v != NULL); SAFE_RELEASE(v); } else CHECK(v == NULL); return hr; }
static HRESULT try_uav(ID3D11Device *dev, ID3D11Resource *r, const D3D11_UNORDERED_ACCESS_VIEW_DESC *d)
{ ID3D11UnorderedAccessView *v = NULL; HRESULT hr = ID3D11Device_CreateUnorderedAccessView(dev, r, d, &v); if (SUCCEEDED(hr)) { CHECK(v != NULL); SAFE_RELEASE(v); } else CHECK(v == NULL); return hr; }
/* Same, but with ppView == NULL (validation only). */
static HRESULT val_srv(ID3D11Device *dev, ID3D11Resource *r, const D3D11_SHADER_RESOURCE_VIEW_DESC *d)
{ return ID3D11Device_CreateShaderResourceView(dev, r, d, NULL); }
static HRESULT val_rtv(ID3D11Device *dev, ID3D11Resource *r, const D3D11_RENDER_TARGET_VIEW_DESC *d)
{ return ID3D11Device_CreateRenderTargetView(dev, r, d, NULL); }
static HRESULT val_dsv(ID3D11Device *dev, ID3D11Resource *r, const D3D11_DEPTH_STENCIL_VIEW_DESC *d)
{ return ID3D11Device_CreateDepthStencilView(dev, r, d, NULL); }
static HRESULT val_uav(ID3D11Device *dev, ID3D11Resource *r, const D3D11_UNORDERED_ACCESS_VIEW_DESC *d)
{ return ID3D11Device_CreateUnorderedAccessView(dev, r, d, NULL); }
#endif
