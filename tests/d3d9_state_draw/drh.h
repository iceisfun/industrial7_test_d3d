/* draw test helpers: a 64x64 A8R8G8B8 render target with depth/stencil, a known baseline state, and pixel probes */
#ifndef DRH_H
#define DRH_H
#include "d9sd.h"

#define DW 64
#define DH 64
typedef struct { SD d; SDRT rt; } DR;

static inline void dr_base_state(DR *r)
{
    IDirect3DDevice9 *dev = r->d.dev;
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SPECULARENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xf);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FILLMODE, D3DFILL_SOLID);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CLIPPLANEENABLE, 0);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_POINTSIZE, f2d(1.0f));
    IDirect3DDevice9_SetRenderState(dev, D3DRS_POINTSPRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_POINTSCALEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_DITHERENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTexture(dev, 0, NULL);
    IDirect3DDevice9_SetVertexShader(dev, NULL);
    IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetFVF(dev, TLV_FVF);
}
static inline int dr_open(DR *r)
{
    memset(r, 0, sizeof *r);
    if (!sd_open(&r->d, SD_RENDER)) return 0;
    if (!sd_rt_make(&r->d, &r->rt, DW, DH, D3DFMT_A8R8G8B8, 1)) { sd_close(&r->d); return 0; }
    dr_base_state(r);
    return 1;
}
static inline void dr_close(DR *r) { sd_rt_free(&r->d, &r->rt); sd_close(&r->d); }
#define DR_OPEN(r)  DR r; if (!dr_open(&r)) return
#define DR_END(r)   dr_close(&r)
#define DR_NEED(r, cond, why) do { if (!(cond)) { t_skip("%s", why); dr_close(&r); return; } } while (0)
#define DEV(r) ((r).d.dev)

static inline void dr_clear(DR *r, DWORD color, float z, DWORD stencil)
{
    HRESULT hr = IDirect3DDevice9_Clear(r->d.dev, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, color, z, stencil);
    if (FAILED(hr)) hr = IDirect3DDevice9_Clear(r->d.dev, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, color, z, stencil);
    t_check(SUCCEEDED(hr), __FILE__, __LINE__, "dr_clear", "got %s", d3dh_hr_name(hr));
}
static inline HRESULT dr_draw_up(DR *r, D3DPRIMITIVETYPE pt, UINT count, const void *v, UINT stride)
{
    HRESULT hr, h2;
    hr = IDirect3DDevice9_BeginScene(r->d.dev);
    if (FAILED(hr)) return hr;
    hr = IDirect3DDevice9_DrawPrimitiveUP(r->d.dev, pt, count, v, stride);
    h2 = IDirect3DDevice9_EndScene(r->d.dev);
    return FAILED(hr) ? hr : h2;
}
/* draw a rectangle given in pixels as a pretransformed triangle list */
static inline HRESULT dr_rect(DR *r, float x0, float y0, float x1, float y1, float z, DWORD c)
{
    TLV v[6];
    tlv_rect(v, x0, y0, x1, y1, z, c);
    return dr_draw_up(r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLV));
}
static inline HRESULT dr_full(DR *r, float z, DWORD c) { return dr_rect(r, -4.0f, -4.0f, DW + 4.0f, DH + 4.0f, z, c); }
static inline int dr_read(DR *r, DWORD *px) { return sd_rt_read(&r->d, &r->rt, px); }
/* assert (x,y) holds colour c (exact) */
#define PXEQ(px, x, y, c) do { DWORD g_ = SD_PX(px, DW, x, y); t_check(g_ == (DWORD)(c), __FILE__, __LINE__, "pixel", "(%d,%d) got 0x%08lx want 0x%08lx", (int)(x), (int)(y), (unsigned long)g_, (unsigned long)(DWORD)(c)); } while (0)
#define PXNEAR(px, x, y, c, tol) do { DWORD g_ = SD_PX(px, DW, x, y); t_check(sd_rgb_near(g_, (DWORD)(c), tol), __FILE__, __LINE__, "pixel", "(%d,%d) got 0x%08lx want ~0x%08lx", (int)(x), (int)(y), (unsigned long)g_, (unsigned long)(DWORD)(c)); } while (0)
static const D3DCMPFUNC g_cmp_fn[8] = { D3DCMP_NEVER, D3DCMP_LESS, D3DCMP_EQUAL, D3DCMP_LESSEQUAL, D3DCMP_GREATER, D3DCMP_NOTEQUAL, D3DCMP_GREATEREQUAL, D3DCMP_ALWAYS };
/* comparison  "a FUNC b" */
static inline int cmp_pass2(D3DCMPFUNC f, int a, int b)
{
    switch (f) {
    case D3DCMP_NEVER: return 0; case D3DCMP_LESS: return a < b; case D3DCMP_EQUAL: return a == b; case D3DCMP_LESSEQUAL: return a <= b;
    case D3DCMP_GREATER: return a > b; case D3DCMP_NOTEQUAL: return a != b; case D3DCMP_GREATEREQUAL: return a >= b; default: return 1;
    }
}
#define BLACK 0xff000000u
#define WHITE 0xffffffffu
#define RED   0xffff0000u
#define GREEN 0xff00ff00u
#define BLUE  0xff0000ffu
#endif
