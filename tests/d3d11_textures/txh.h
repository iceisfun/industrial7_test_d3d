/* Local helpers for the d3d11_textures area: device wrapper, format geometry, deterministic
 * subresource patterns, and a staging-based fetch that returns tightly packed rows. */
#ifndef TXH_H
#define TXH_H
#include "d3dh.h"
#include <d3d11_3.h>
#include <dxgi1_2.h>

typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE drv; } tx_ctx;

static int tx_open_flags(tx_ctx *c, UINT flags) {
    HRESULT hr;
    memset(c, 0, sizeof *c);
    hr = d3dh_make_device11(&c->dev, &c->ctx, &c->fl, &c->drv, flags | D3D11_CREATE_DEVICE_BGRA_SUPPORT);
    if (FAILED(hr)) { t_skip("no D3D11 device (%s)", d3dh_hr_name(hr)); return 0; }
    return 1;
}
static int  tx_open(tx_ctx *c) { return tx_open_flags(c, 0); }
static void tx_close(tx_ctx *c) { SAFE_RELEASE(c->ctx); SAFE_RELEASE(c->dev); }

/* Test with a device; the body receives `c`. */
#define DEVTEST(n) \
    static void body_##n(tx_ctx *c); \
    TEST(n) { tx_ctx tc_; if (!tx_open(&tc_)) return; body_##n(&tc_); tx_close(&tc_); } \
    static void body_##n(tx_ctx *c)

/* Hard check when the device's feature level is at least minfl, otherwise only a note. */
#define TX_EXPECT_HR(c, hr, want, minfl) do { HRESULT h_ = (HRESULT)(hr), w_ = (HRESULT)(want); \
    if ((c)->fl >= (D3D_FEATURE_LEVEL)(minfl)) t_check(h_ == w_, __FILE__, __LINE__, #hr " == " #want, "got %s want %s", d3dh_hr_name(h_), d3dh_hr_name(w_)); \
    else t_note("%s:%d feature level %#x < %#x: got %s want %s", __FILE__, __LINE__, (c)->fl, (UINT)(minfl), d3dh_hr_name(h_), d3dh_hr_name(w_)); } while (0)

/* ---- geometry ----------------------------------------------------------------- */
static UINT tx_dim(UINT v, UINT mip) { v >>= mip; return v ? v : 1; }
static UINT tx_full_mips(UINT maxdim) { UINT n = 1; while (maxdim > 1) { maxdim >>= 1; n++; } return n; }

typedef struct { UINT bw, bh, bpb; } tx_finfo_t;   /* block size in texels and bytes per block */
static int tx_finfo(DXGI_FORMAT f, tx_finfo_t *o) {
    switch (f) {
    case DXGI_FORMAT_BC1_TYPELESS: case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS: case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC4_SNORM:
        o->bw = o->bh = 4; o->bpb = 8; return 1;
    case DXGI_FORMAT_BC2_TYPELESS: case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS: case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS: case DXGI_FORMAT_BC5_UNORM: case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS: case DXGI_FORMAT_BC6H_UF16: case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS: case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
        o->bw = o->bh = 4; o->bpb = 16; return 1;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT: case DXGI_FORMAT_R32G32B32A32_SINT:
        o->bw = o->bh = 1; o->bpb = 16; return 1;
    case DXGI_FORMAT_R32G32B32_TYPELESS: case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT: case DXGI_FORMAT_R32G32B32_SINT:
        o->bw = o->bh = 1; o->bpb = 12; return 1;
    case DXGI_FORMAT_R32G8X24_TYPELESS: case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        o->bw = o->bh = 1; o->bpb = 8; return 1;
    default: break;
    }
    UINT b = d3dh_format_bytes(f);
    if (!b || f == DXGI_FORMAT_R8G8_B8G8_UNORM || f == DXGI_FORMAT_G8R8_G8B8_UNORM) return 0;
    o->bw = o->bh = 1; o->bpb = b; return 1;
}
static UINT tx_row_bytes(DXGI_FORMAT f, UINT w) { tx_finfo_t i; if (!tx_finfo(f, &i)) return 0; return (w + i.bw - 1) / i.bw * i.bpb; }
static UINT tx_rows(DXGI_FORMAT f, UINT h) { tx_finfo_t i; if (!tx_finfo(f, &i)) return 0; return (h + i.bh - 1) / i.bh; }
static int  tx_is_bc(DXGI_FORMAT f) { tx_finfo_t i; return tx_finfo(f, &i) && i.bw == 4; }

/* ---- deterministic pattern ------------------------------------------------------ */
static unsigned char tx_pat(UINT seed, size_t i) { return (unsigned char)(seed * 0x9D + i * 7 + (i >> 5) * 3 + 0x31); }

/* ---- initial data builder -------------------------------------------------------- */
typedef struct { D3D11_SUBRESOURCE_DATA *sd; unsigned char **blocks; UINT n; } tx_init;
static void tx_init_free(tx_init *o) {
    if (o->blocks) { for (UINT i = 0; i < o->n; i++) free(o->blocks[i]); free(o->blocks); }
    free(o->sd); memset(o, 0, sizeof *o);
}
/* Builds mips*arr subresources of pattern data (seed0 + subresource index). rowpad/slicepad add padding bytes
 * to SysMemPitch / SysMemSlicePitch. mips == 0 means the full chain. Padding bytes are 0xEE. */
static int tx_init_make(tx_init *o, UINT w, UINT h, UINT d, UINT mips, UINT arr, DXGI_FORMAT f, UINT rowpad, UINT slicepad, UINT seed0) {
    UINT m, a, i;
    memset(o, 0, sizeof *o);
    if (!mips) mips = tx_full_mips(w > h ? (w > d ? w : d) : (h > d ? h : d));
    o->n = mips * arr;
    o->sd = calloc(o->n, sizeof *o->sd);
    o->blocks = calloc(o->n, sizeof *o->blocks);
    if (!o->sd || !o->blocks) return 0;
    for (a = 0; a < arr; a++) for (m = 0; m < mips; m++) {
        UINT sub = a * mips + m, mw = tx_dim(w, m), mh = tx_dim(h, m), md = tx_dim(d, m);
        UINT rb = tx_row_bytes(f, mw), rows = tx_rows(f, mh), pitch = rb + rowpad, slice = pitch * rows + slicepad;
        size_t sz = (size_t)slice * md, idx = 0;
        unsigned char *p = malloc(sz ? sz : 1);
        UINT z, y, x;
        if (!p) return 0;
        memset(p, 0xEE, sz);
        for (z = 0; z < md; z++) for (y = 0; y < rows; y++) for (x = 0; x < rb; x++, idx++)
            p[(size_t)z * slice + (size_t)y * pitch + x] = tx_pat(seed0 + sub, idx);
        o->blocks[sub] = p;
        o->sd[sub].pSysMem = p; o->sd[sub].SysMemPitch = pitch; o->sd[sub].SysMemSlicePitch = md > 1 ? slice : 0;
    }
    (void)i;
    return 1;
}

/* ---- creation shortcuts -------------------------------------------------------------- */
static HRESULT tx_mk1d(tx_ctx *c, UINT w, UINT mips, UINT arr, DXGI_FORMAT f, D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, const D3D11_SUBRESOURCE_DATA *sd, ID3D11Texture1D **out) {
    D3D11_TEXTURE1D_DESC d; memset(&d, 0, sizeof d);
    d.Width = w; d.MipLevels = mips; d.ArraySize = arr; d.Format = f; d.Usage = u; d.BindFlags = bind; d.CPUAccessFlags = cpu; d.MiscFlags = misc;
    return ID3D11Device_CreateTexture1D(c->dev, &d, sd, out);
}
static HRESULT tx_mk2d(tx_ctx *c, UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT f, D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, const D3D11_SUBRESOURCE_DATA *sd, ID3D11Texture2D **out) {
    D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = arr; d.Format = f; d.SampleDesc.Count = 1;
    d.Usage = u; d.BindFlags = bind; d.CPUAccessFlags = cpu; d.MiscFlags = misc;
    return ID3D11Device_CreateTexture2D(c->dev, &d, sd, out);
}
static HRESULT tx_mk3d(tx_ctx *c, UINT w, UINT h, UINT dp, UINT mips, DXGI_FORMAT f, D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, const D3D11_SUBRESOURCE_DATA *sd, ID3D11Texture3D **out) {
    D3D11_TEXTURE3D_DESC d; memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.Depth = dp; d.MipLevels = mips; d.Format = f; d.Usage = u; d.BindFlags = bind; d.CPUAccessFlags = cpu; d.MiscFlags = misc;
    return ID3D11Device_CreateTexture3D(c->dev, &d, sd, out);
}

/* ---- fetch one subresource into tightly packed memory --------------------------------------- */
typedef struct { UINT w, h, d, rowbytes, rows, rowpitch, depthpitch; unsigned char *data; } tx_img;
static void tx_img_free(tx_img *im) { free(im->data); memset(im, 0, sizeof *im); }

static HRESULT tx_fetch(tx_ctx *c, ID3D11Resource *res, UINT sub, tx_img *o) {
    D3D11_RESOURCE_DIMENSION dim; ID3D11Resource *st = NULL; D3D11_MAPPED_SUBRESOURCE m;
    UINT w = 0, h = 1, dp = 1, mips = 1; DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN; HRESULT hr;
    memset(o, 0, sizeof *o);
    ID3D11Resource_GetType(res, &dim);
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
        D3D11_TEXTURE1D_DESC d; ID3D11Texture1D *t = NULL;
        ID3D11Texture1D_GetDesc((ID3D11Texture1D *)res, &d);
        w = d.Width; mips = d.MipLevels; f = d.Format;
        d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
        hr = ID3D11Device_CreateTexture1D(c->dev, &d, NULL, &t); st = (ID3D11Resource *)t;
    } else if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *t = NULL;
        ID3D11Texture2D_GetDesc((ID3D11Texture2D *)res, &d);
        w = d.Width; h = d.Height; mips = d.MipLevels; f = d.Format;
        d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
        hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &t); st = (ID3D11Resource *)t;
    } else if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
        D3D11_TEXTURE3D_DESC d; ID3D11Texture3D *t = NULL;
        ID3D11Texture3D_GetDesc((ID3D11Texture3D *)res, &d);
        w = d.Width; h = d.Height; dp = d.Depth; mips = d.MipLevels; f = d.Format;
        d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
        hr = ID3D11Device_CreateTexture3D(c->dev, &d, NULL, &t); st = (ID3D11Resource *)t;
    } else return E_INVALIDARG;
    if (FAILED(hr)) return hr;
    ID3D11DeviceContext_CopyResource(c->ctx, st, res);
    hr = ID3D11DeviceContext_Map(c->ctx, st, sub, D3D11_MAP_READ, 0, &m);
    if (SUCCEEDED(hr)) {
        UINT mip = sub % mips, z, y;
        o->w = tx_dim(w, mip); o->h = tx_dim(h, mip); o->d = dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D ? tx_dim(dp, mip) : 1;
        o->rowbytes = tx_row_bytes(f, o->w); o->rows = tx_rows(f, o->h);
        o->rowpitch = m.RowPitch; o->depthpitch = m.DepthPitch;
        o->data = malloc((size_t)o->rowbytes * o->rows * o->d + 1);
        if (!o->data) hr = E_OUTOFMEMORY;
        else for (z = 0; z < o->d; z++) for (y = 0; y < o->rows; y++)
            memcpy(o->data + ((size_t)z * o->rows + y) * o->rowbytes,
                   (const char *)m.pData + (size_t)z * m.DepthPitch + (size_t)y * m.RowPitch, o->rowbytes);
        ID3D11DeviceContext_Unmap(c->ctx, st, sub);
    }
    ID3D11Resource_Release(st);
    return hr;
}
/* 1 when the whole image equals tx_pat(seed, index) in tight order. *bad receives the first mismatching offset. */
static int tx_img_is_pat(const tx_img *im, UINT seed, size_t *bad) {
    size_t n = (size_t)im->rowbytes * im->rows * im->d, i;
    for (i = 0; i < n; i++) if (im->data[i] != tx_pat(seed, i)) { if (bad) *bad = i; return 0; }
    return 1;
}
static int tx_img_is_const(const tx_img *im, unsigned char v, size_t *bad) {
    size_t n = (size_t)im->rowbytes * im->rows * im->d, i;
    for (i = 0; i < n; i++) if (im->data[i] != v) { if (bad) *bad = i; return 0; }
    return 1;
}
/* Read-back as 32-bit pixel at (x,y,z) of a fetched 4-byte-per-texel image. */
static UINT tx_px32(const tx_img *im, UINT x, UINT y, UINT z) {
    UINT v; memcpy(&v, im->data + (((size_t)z * im->rows + y) * im->rowbytes) + (size_t)x * 4, 4); return v;
}

/* Short aliases used by the case tables. */
#define U_DEF D3D11_USAGE_DEFAULT
#define U_IMM D3D11_USAGE_IMMUTABLE
#define U_DYN D3D11_USAGE_DYNAMIC
#define U_STG D3D11_USAGE_STAGING
#define B_SRV D3D11_BIND_SHADER_RESOURCE
#define B_RT  D3D11_BIND_RENDER_TARGET
#define B_DS  D3D11_BIND_DEPTH_STENCIL
#define B_UAV D3D11_BIND_UNORDERED_ACCESS
#define B_VB  D3D11_BIND_VERTEX_BUFFER
#define B_IB  D3D11_BIND_INDEX_BUFFER
#define B_CB  D3D11_BIND_CONSTANT_BUFFER
#define B_SO  D3D11_BIND_STREAM_OUTPUT
#define CPU_R D3D11_CPU_ACCESS_READ
#define CPU_W D3D11_CPU_ACCESS_WRITE
#define M_GEN D3D11_RESOURCE_MISC_GENERATE_MIPS
#define M_SHR D3D11_RESOURCE_MISC_SHARED
#define M_CUBE D3D11_RESOURCE_MISC_TEXTURECUBE
#define M_KEYED D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
#define M_GDI D3D11_RESOURCE_MISC_GDI_COMPATIBLE
#define F_RGBA8 DXGI_FORMAT_R8G8B8A8_UNORM
#define F_BGRA8 DXGI_FORMAT_B8G8R8A8_UNORM
#define F_R8 DXGI_FORMAT_R8_UNORM
#define FL_10_0 D3D_FEATURE_LEVEL_10_0
#define FL_10_1 D3D_FEATURE_LEVEL_10_1
#define FL_11_0 D3D_FEATURE_LEVEL_11_0
#define FL_11_1 D3D_FEATURE_LEVEL_11_1
#endif
