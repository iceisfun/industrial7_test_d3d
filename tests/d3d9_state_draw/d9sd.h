/* shared fixture for the d3d9_state_draw area */
#ifndef D9SD_H
#define D9SD_H
#include "d3dh.h"

typedef struct {
    IDirect3D9 *d3d;
    IDirect3DDevice9 *dev;
    HWND hwnd;
    D3DDEVTYPE type;
    D3DCAPS9 caps;
    int has_ds;
    D3DFORMAT dsfmt;
} SD;

enum { SD_STATE = 0, SD_STATE_DS = 1, SD_RENDER = 2 };
#define SD_W 64
#define SD_H 64

static inline void sd_close(SD *d)
{
    if (d->dev) IDirect3DDevice9_Release(d->dev);
    if (d->d3d) IDirect3D9_Release(d->d3d);
    d3dh_destroy_window(d->hwnd);
    memset(d, 0, sizeof *d);
}

/* Windowed 64x64 hidden device, software vertex processing.
 *  SD_STATE:    NULLREF, HAL, REF; no automatic depth buffer.
 *  SD_STATE_DS: same types, automatic depth/stencil (D24S8, then D16).
 *  SD_RENDER:   HAL, REF only (NULLREF cannot render); automatic depth/stencil. */
static inline int sd_open(SD *d, int mode)
{
    static const D3DDEVTYPE all[3] = { D3DDEVTYPE_NULLREF, D3DDEVTYPE_HAL, D3DDEVTYPE_REF };
    static const D3DFORMAT dsf[2] = { D3DFMT_D24S8, D3DFMT_D16 };
    int first = (mode == SD_RENDER) ? 1 : 0;
    HRESULT hr = D3DERR_NOTAVAILABLE;
    memset(d, 0, sizeof *d);
    if (!d3dh_Direct3DCreate9()) { t_skip("no Direct3DCreate9"); return 0; }
    d->d3d = d3dh_Direct3DCreate9()(D3D_SDK_VERSION);
    if (!d->d3d) { t_skip("Direct3DCreate9 failed"); return 0; }
    d->hwnd = d3dh_create_hidden_window();
    for (int i = first; i < 3 && !d->dev; i++) {
        int nds = (mode == SD_STATE) ? 1 : 2;
        for (int k = 0; k < nds && !d->dev; k++) {
            D3DPRESENT_PARAMETERS pp;
            memset(&pp, 0, sizeof pp);
            pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
            pp.BackBufferWidth = SD_W; pp.BackBufferHeight = SD_H; pp.hDeviceWindow = d->hwnd;
            if (mode != SD_STATE) { pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = dsf[k]; }
            hr = IDirect3D9_CreateDevice(d->d3d, D3DADAPTER_DEFAULT, all[i], d->hwnd,
                                         D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &d->dev);
            if (SUCCEEDED(hr) && d->dev) {
                d->type = all[i];
                d->has_ds = (mode != SD_STATE);
                d->dsfmt = d->has_ds ? dsf[k] : D3DFMT_UNKNOWN;
            } else d->dev = NULL;
        }
    }
    if (!d->dev) {
        t_skip("no D3D9 %s device (%s)", mode == SD_RENDER ? "rendering" : "state", d3dh_hr_name(hr));
        sd_close(d);
        return 0;
    }
    memset(&d->caps, 0, sizeof d->caps);
    IDirect3DDevice9_GetDeviceCaps(d->dev, &d->caps);
    return 1;
}
#define SD_OPEN(d)    SD d; if (!sd_open(&d, SD_STATE)) return
#define SD_OPEN_DS(d) SD d; if (!sd_open(&d, SD_STATE_DS)) return
#define SD_OPEN_R(d)  SD d; if (!sd_open(&d, SD_RENDER)) return
#define SD_END(d)     sd_close(&d)
/* skip the rest of a test and release the fixture */
#define SD_NEED(d, cond, why) do { if (!(cond)) { t_skip("%s", why); sd_close(&d); return; } } while (0)

static inline DWORD f2d(float f) { DWORD v; memcpy(&v, &f, 4); return v; }
static inline float d2f(DWORD v) { float f; memcpy(&f, &v, 4); return f; }

/* ---- pretransformed triangle helpers ------------------------------------- */
typedef struct { float x, y, z, rhw; DWORD c; } TLV;
#define TLV_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
static inline void tlv_set(TLV *v, float x, float y, float z, DWORD c) { v->x = x; v->y = y; v->z = z; v->rhw = 1.0f; v->c = c; }
/* two triangles (6 vertices) covering the rectangle [x0,x1]x[y0,y1] at depth z */
static inline void tlv_rect(TLV v[6], float x0, float y0, float x1, float y1, float z, DWORD c)
{
    tlv_set(&v[0], x0, y0, z, c); tlv_set(&v[1], x1, y0, z, c); tlv_set(&v[2], x0, y1, z, c);
    tlv_set(&v[3], x1, y0, z, c); tlv_set(&v[4], x1, y1, z, c); tlv_set(&v[5], x0, y1, z, c);
}
/* whole render target with a safe margin */
static inline void tlv_full(TLV v[6], float z, DWORD c) { tlv_rect(v, -4.0f, -4.0f, SD_W + 4.0f, SD_H + 4.0f, z, c); }

/* ---- render target + depth surface that is bound to the device ------------ */
typedef struct {
    IDirect3DSurface9 *rt, *ds, *old_rt, *old_ds;
    UINT w, h;
} SDRT;

static inline int sd_rt_make(SD *d, SDRT *r, UINT w, UINT h, D3DFORMAT fmt, int with_ds)
{
    HRESULT hr;
    memset(r, 0, sizeof *r);
    r->w = w; r->h = h;
    hr = IDirect3DDevice9_CreateRenderTarget(d->dev, w, h, fmt, D3DMULTISAMPLE_NONE, 0, FALSE, &r->rt, NULL);
    if (FAILED(hr)) { t_skip("CreateRenderTarget failed %s", d3dh_hr_name(hr)); return 0; }
    if (with_ds) {
        hr = IDirect3DDevice9_CreateDepthStencilSurface(d->dev, w, h, d->has_ds ? d->dsfmt : D3DFMT_D24S8,
                                                        D3DMULTISAMPLE_NONE, 0, FALSE, &r->ds, NULL);
        if (FAILED(hr)) { IDirect3DSurface9_Release(r->rt); r->rt = NULL; t_skip("CreateDepthStencilSurface failed %s", d3dh_hr_name(hr)); return 0; }
    }
    IDirect3DDevice9_GetRenderTarget(d->dev, 0, &r->old_rt);
    IDirect3DDevice9_GetDepthStencilSurface(d->dev, &r->old_ds);
    hr = IDirect3DDevice9_SetRenderTarget(d->dev, 0, r->rt);
    if (SUCCEEDED(hr)) hr = IDirect3DDevice9_SetDepthStencilSurface(d->dev, r->ds);
    if (FAILED(hr)) {
        t_skip("cannot bind render target %s", d3dh_hr_name(hr));
        if (r->old_rt) IDirect3DDevice9_SetRenderTarget(d->dev, 0, r->old_rt);
        if (r->old_ds) IDirect3DDevice9_SetDepthStencilSurface(d->dev, r->old_ds);
        SAFE_RELEASE(r->rt); SAFE_RELEASE(r->ds); SAFE_RELEASE(r->old_rt); SAFE_RELEASE(r->old_ds);
        return 0;
    }
    return 1;
}
static inline void sd_rt_free(SD *d, SDRT *r)
{
    if (r->old_rt) IDirect3DDevice9_SetRenderTarget(d->dev, 0, r->old_rt);
    IDirect3DDevice9_SetDepthStencilSurface(d->dev, r->old_ds);
    SAFE_RELEASE(r->rt); SAFE_RELEASE(r->ds); SAFE_RELEASE(r->old_rt); SAFE_RELEASE(r->old_ds);
}
/* Read the render target back into out[w*h] (A8R8G8B8 / X8R8G8B8 as stored). Returns 1 on success. */
static inline int sd_rt_read(SD *d, SDRT *r, DWORD *out)
{
    IDirect3DSurface9 *sys = NULL;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    HRESULT hr;
    IDirect3DSurface9_GetDesc(r->rt, &desc);
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(d->dev, r->w, r->h, desc.Format, D3DPOOL_SYSTEMMEM, &sys, NULL);
    if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "CreateOffscreenPlainSurface", "got %s", d3dh_hr_name(hr)); return 0; }
    hr = IDirect3DDevice9_GetRenderTargetData(d->dev, r->rt, sys);
    if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "GetRenderTargetData", "got %s", d3dh_hr_name(hr)); IDirect3DSurface9_Release(sys); return 0; }
    hr = IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY);
    if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "LockRect", "got %s", d3dh_hr_name(hr)); IDirect3DSurface9_Release(sys); return 0; }
    for (UINT y = 0; y < r->h; y++) memcpy(out + (size_t)y * r->w, (BYTE *)lr.pBits + (size_t)y * lr.Pitch, r->w * 4);
    IDirect3DSurface9_UnlockRect(sys);
    IDirect3DSurface9_Release(sys);
    return 1;
}
#define SD_PX(buf, w, x, y) ((buf)[(size_t)(y) * (w) + (x)])
static inline int sd_chan_near(DWORD a, DWORD b, int tol)
{
    for (int s = 0; s < 32; s += 8) {
        int da = (int)((a >> s) & 0xff) - (int)((b >> s) & 0xff);
        if (da < -tol || da > tol) return 0;
    }
    return 1;
}
/* compare ignoring the alpha byte */
static inline int sd_rgb_near(DWORD a, DWORD b, int tol) { return sd_chan_near(a & 0x00ffffff, b & 0x00ffffff, tol); }

/* count pixels in a region that differ from a colour (rgb only) */
static inline int sd_count_not(const DWORD *buf, UINT w, int x0, int y0, int x1, int y1, DWORD rgb)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if ((SD_PX(buf, w, x, y) & 0x00ffffff) != (rgb & 0x00ffffff)) n++;
    return n;
}
static inline int sd_count_is(const DWORD *buf, UINT w, int x0, int y0, int x1, int y1, DWORD rgb)
{
    return (x1 - x0) * (y1 - y0) - sd_count_not(buf, w, x0, y0, x1, y1, rgb);
}

/* Reset-style default: bring a few stateful things back so tests are independent. */
#define SD_NOFLAGS 0
#endif
