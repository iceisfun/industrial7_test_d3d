/* Shared scaffolding for the d3d11_state_objects area. */
#ifndef SO_COMMON_H
#define SO_COMMON_H
#include "d3dh.h"
#include <d3d11_1.h>
#include <d3d11_3.h>
#include <float.h>
#include <limits.h>
#include <math.h>

extern ID3D11Device *so_dev;
extern ID3D11DeviceContext *so_ctx;
extern D3D_FEATURE_LEVEL so_fl;
extern D3D_DRIVER_TYPE so_drv;

int so_init(void);                       /* 1 when a device exists (shared, created lazily) */
ID3D11Device1 *so_dev1(void);            /* NULL when ID3D11Device1 is unavailable */
ID3D11Device3 *so_dev3(void);            /* NULL when ID3D11Device3 is unavailable */
ID3DBlob *so_compile_vs(const char *src);/* cached vs_5_0 compile (entry main), NULL if the compiler is missing/failed */
int so_logic_ops(void);                  /* OutputMergerLogicOp support */

#define SO_NEED() do { if (!so_init()) { t_skip("no D3D11 device"); return; } } while (0)
#define SO_NEED1() do { SO_NEED(); if (!so_dev1()) { t_skip("no ID3D11Device1"); return; } } while (0)
#define SO_NEED3() do { SO_NEED(); if (!so_dev3()) { t_skip("no ID3D11Device3"); return; } } while (0)

#define SO_XTEST(n) TEST(n)
/* ST(foo) declares a test named d3d11_state_objects_foo (the paste happens before TEST stringifies). */
#define ST(n) SO_XTEST(d3d11_state_objects_##n)

#define SO_SENTINEL(T) ((T *)(ULONG_PTR)0x10)

/* Release helper that returns the count after the release. */
static inline ULONG so_rel(void *p) { return p ? ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p) : 0; }
static inline ULONG so_addref_count(void *p) { return d3dh_refcount(p); }

/* default descriptors (the documented CD3D11_* defaults) */
static inline D3D11_BLEND_DESC so_blend_default(void)
{
    D3D11_BLEND_DESC d; int i;
    memset(&d, 0, sizeof d);
    for (i = 0; i < 8; i++) {
        d.RenderTarget[i].BlendEnable = FALSE;
        d.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
        d.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
        d.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        d.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        d.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
        d.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        d.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    return d;
}
static inline D3D11_BLEND_DESC1 so_blend1_default(void)
{
    D3D11_BLEND_DESC1 d; int i;
    memset(&d, 0, sizeof d);
    for (i = 0; i < 8; i++) {
        d.RenderTarget[i].BlendEnable = FALSE;
        d.RenderTarget[i].LogicOpEnable = FALSE;
        d.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
        d.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
        d.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        d.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        d.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
        d.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        d.RenderTarget[i].LogicOp = D3D11_LOGIC_OP_NOOP;
        d.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    return d;
}
static inline D3D11_DEPTH_STENCIL_DESC so_ds_default(void)
{
    D3D11_DEPTH_STENCIL_DESC d;
    memset(&d, 0, sizeof d);
    d.DepthEnable = TRUE;
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    d.DepthFunc = D3D11_COMPARISON_LESS;
    d.StencilEnable = FALSE;
    d.StencilReadMask = 0xFF;
    d.StencilWriteMask = 0xFF;
    d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    d.BackFace = d.FrontFace;
    return d;
}
static inline D3D11_RASTERIZER_DESC so_rast_default(void)
{
    D3D11_RASTERIZER_DESC d;
    memset(&d, 0, sizeof d);
    d.FillMode = D3D11_FILL_SOLID;
    d.CullMode = D3D11_CULL_BACK;
    d.DepthClipEnable = TRUE;
    return d;
}
static inline D3D11_SAMPLER_DESC so_samp_default(void)
{
    D3D11_SAMPLER_DESC d;
    memset(&d, 0, sizeof d);
    d.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    d.MaxAnisotropy = 1;
    d.ComparisonFunc = D3D11_COMPARISON_NEVER;
    d.MinLOD = -D3D11_FLOAT32_MAX;
    d.MaxLOD = D3D11_FLOAT32_MAX;
    return d;
}

/* Thin call wrappers. */
#define SO_CBLEND(d, o)  ID3D11Device_CreateBlendState(so_dev, (d), (o))
#define SO_CDS(d, o)     ID3D11Device_CreateDepthStencilState(so_dev, (d), (o))
#define SO_CRAST(d, o)   ID3D11Device_CreateRasterizerState(so_dev, (d), (o))
#define SO_CSAMP(d, o)   ID3D11Device_CreateSamplerState(so_dev, (d), (o))

/* Expect that a desc is accepted: validate-only call gives S_FALSE and a real create gives S_OK. Releases the object. */
#define SO_ACCEPT_BLEND(d)  do { D3D11_BLEND_DESC *d_ = (d); ID3D11BlendState *s_ = NULL; \
    EXPECT_HR(SO_CBLEND(d_, NULL), S_FALSE); EXPECT_HR(SO_CBLEND(d_, &s_), S_OK); SAFE_RELEASE(s_); } while (0)
#define SO_REJECT_BLEND(d)  do { D3D11_BLEND_DESC *d_ = (d); ID3D11BlendState *s_ = SO_SENTINEL(ID3D11BlendState); \
    EXPECT_HR(SO_CBLEND(d_, NULL), E_INVALIDARG); EXPECT_HR(SO_CBLEND(d_, &s_), E_INVALIDARG); CHECK(s_ == NULL); } while (0)
#define SO_ACCEPT_DS(d)  do { D3D11_DEPTH_STENCIL_DESC *d_ = (d); ID3D11DepthStencilState *s_ = NULL; \
    EXPECT_HR(SO_CDS(d_, NULL), S_FALSE); EXPECT_HR(SO_CDS(d_, &s_), S_OK); SAFE_RELEASE(s_); } while (0)
#define SO_REJECT_DS(d)  do { D3D11_DEPTH_STENCIL_DESC *d_ = (d); ID3D11DepthStencilState *s_ = SO_SENTINEL(ID3D11DepthStencilState); \
    EXPECT_HR(SO_CDS(d_, NULL), E_INVALIDARG); EXPECT_HR(SO_CDS(d_, &s_), E_INVALIDARG); CHECK(s_ == NULL); } while (0)
#define SO_ACCEPT_RAST(d)  do { D3D11_RASTERIZER_DESC *d_ = (d); ID3D11RasterizerState *s_ = NULL; \
    EXPECT_HR(SO_CRAST(d_, NULL), S_FALSE); EXPECT_HR(SO_CRAST(d_, &s_), S_OK); SAFE_RELEASE(s_); } while (0)
#define SO_REJECT_RAST(d)  do { D3D11_RASTERIZER_DESC *d_ = (d); ID3D11RasterizerState *s_ = NULL; \
    EXPECT_HR(SO_CRAST(d_, NULL), E_INVALIDARG); EXPECT_HR(SO_CRAST(d_, &s_), E_INVALIDARG); CHECK(s_ == NULL); } while (0)
#define SO_ACCEPT_SAMP(d)  do { D3D11_SAMPLER_DESC *d_ = (d); ID3D11SamplerState *s_ = NULL; \
    EXPECT_HR(SO_CSAMP(d_, NULL), S_FALSE); EXPECT_HR(SO_CSAMP(d_, &s_), S_OK); SAFE_RELEASE(s_); } while (0)
#define SO_REJECT_SAMP(d)  do { D3D11_SAMPLER_DESC *d_ = (d); ID3D11SamplerState *s_ = NULL; \
    EXPECT_HR(SO_CSAMP(d_, NULL), E_INVALIDARG); EXPECT_HR(SO_CSAMP(d_, &s_), E_INVALIDARG); CHECK(s_ == NULL); } while (0)

#endif
