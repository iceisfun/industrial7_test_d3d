/* shared fixture for the d3d9_device_resources area */
#ifndef D9R_H
#define D9R_H
#include "d3dh.h"

typedef struct {
    IDirect3D9 *d3d;
    IDirect3DDevice9 *dev;
    HWND hwnd;
    D3DDEVTYPE type;
} D9;

/* Device type order for the fixture. A NULLREF device has no swap chain, back buffer or render target (Windows reports
 * D3DERR_NOTFOUND / D3DERR_INVALIDCALL for them), so resource behaviour is only meaningful on a HAL device: HAL first,
 * then NULLREF, then REF. I7_D9_NULLREF=1 in the environment puts NULLREF first (the old order) for comparison runs. */
static inline void d9_type_order(D3DDEVTYPE *o)
{
    char e[8]; int nullfirst = GetEnvironmentVariableA("I7_D9_NULLREF", e, sizeof e) > 0 && e[0] == '1';
    o[0] = nullfirst ? D3DDEVTYPE_NULLREF : D3DDEVTYPE_HAL; o[1] = nullfirst ? D3DDEVTYPE_HAL : D3DDEVTYPE_NULLREF; o[2] = D3DDEVTYPE_REF;
}
static inline void d9_fill_pp(D3DPRESENT_PARAMETERS *pp, HWND hw)
{
    memset(pp, 0, sizeof *pp);
    pp->Windowed = TRUE; pp->SwapEffect = D3DSWAPEFFECT_DISCARD; pp->BackBufferFormat = D3DFMT_UNKNOWN;
    pp->BackBufferWidth = 64; pp->BackBufferHeight = 64; pp->hDeviceWindow = hw;
}
static inline HRESULT d9_make(IDirect3D9 **d3d, IDirect3DDevice9 **dev, HWND *hw, D3DDEVTYPE *type)
{
    D3DDEVTYPE ty[3]; HRESULT hr = E_FAIL;
    __typeof__(&Direct3DCreate9) c = d3dh_Direct3DCreate9();
    *d3d = NULL; *dev = NULL; d9_type_order(ty);
    *hw = d3dh_create_hidden_window();
    *d3d = c(D3D_SDK_VERSION);
    if (!*d3d) return D3DERR_NOTAVAILABLE;
    for (int i = 0; i < 3; i++) {
        D3DPRESENT_PARAMETERS pp; d9_fill_pp(&pp, *hw);
        hr = IDirect3D9_CreateDevice(*d3d, D3DADAPTER_DEFAULT, ty[i], *hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, dev);
        if (SUCCEEDED(hr)) { *type = ty[i]; return hr; }
    }
    return hr;
}
static inline HRESULT d9x_make(IDirect3D9Ex **d3d, IDirect3DDevice9Ex **dev, HWND *hw, D3DDEVTYPE *type)
{
    D3DDEVTYPE ty[3]; HRESULT hr;
    *d3d = NULL; *dev = NULL; d9_type_order(ty);
    *hw = d3dh_create_hidden_window();
    hr = d3dh_Direct3DCreate9Ex()(D3D_SDK_VERSION, d3d);
    if (FAILED(hr)) return hr;
    for (int i = 0; i < 3; i++) {
        D3DPRESENT_PARAMETERS pp; d9_fill_pp(&pp, *hw);
        hr = IDirect3D9Ex_CreateDeviceEx(*d3d, D3DADAPTER_DEFAULT, ty[i], *hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, NULL, dev);
        if (SUCCEEDED(hr)) { *type = ty[i]; return hr; }
    }
    return hr;
}

/* ---- access-violation probe -------------------------------------------------------------------------------------
 * The d3d9 runtime does not validate every NULL out-pointer: some calls fault inside d3d9.dll instead of returning
 * D3DERR_INVALIDCALL. D9_AV(stmt) runs stmt and evaluates to 1 if it raised an access violation (and 0 otherwise); the
 * statement's own result must be stored by the caller into a volatile variable. */
#include <setjmp.h>
static jmp_buf g_d9_jb; static volatile int g_d9_guard, g_d9_veh;
static LONG CALLBACK d9_veh(PEXCEPTION_POINTERS ep)
{
    if (g_d9_guard && ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) { g_d9_guard = 0; longjmp(g_d9_jb, 1); }
    return EXCEPTION_CONTINUE_SEARCH;
}
#define D9_AV(stmt) ({ volatile int av_ = 0; if (!g_d9_veh) { AddVectoredExceptionHandler(1, d9_veh); g_d9_veh = 1; } \
    if (!setjmp(g_d9_jb)) { g_d9_guard = 1; stmt; g_d9_guard = 0; } else { av_ = 1; } av_; })

#define D9_AVHR ((HRESULT)0x7ff9a001)   /* sentinel result: the call raised an access violation */
#define D9_CALL(call) ({ volatile HRESULT hr_ = E_FAIL; int avc_ = D9_AV(hr_ = (HRESULT)(call)); avc_ ? D9_AVHR : hr_; })
#define EXPECT_HRN(call, want) EXPECT_HR(D9_CALL(call), want)

/* Open a windowed device (HAL, NULLREF, REF in that order). Returns 1 on success; otherwise skips. */
static inline int d9_open(D9 *d)
{
    memset(d, 0, sizeof *d);
    if (!d3dh_Direct3DCreate9()) { t_skip("no Direct3DCreate9"); return 0; }
    HRESULT hr = d9_make(&d->d3d, &d->dev, &d->hwnd, &d->type);
    if (FAILED(hr) || !d->dev) {
        t_skip("no D3D9 device (%s)", d3dh_hr_name(hr));
        if (d->d3d) IDirect3D9_Release(d->d3d);
        d3dh_destroy_window(d->hwnd);
        memset(d, 0, sizeof *d);
        return 0;
    }
    return 1;
}
static inline void d9_close(D9 *d)
{
    if (d->dev) IDirect3DDevice9_Release(d->dev);
    if (d->d3d) IDirect3D9_Release(d->d3d);
    d3dh_destroy_window(d->hwnd);
    memset(d, 0, sizeof *d);
}
#define D9_OPEN(d) D9 d; if (!d9_open(&d)) return
#define D9_END(d) d9_close(&d)

typedef struct {
    IDirect3D9Ex *d3d;
    IDirect3DDevice9Ex *dev;
    HWND hwnd;
    D3DDEVTYPE type;
} D9X;
static inline int d9x_open(D9X *d)
{
    memset(d, 0, sizeof *d);
    if (!d3dh_Direct3DCreate9Ex()) { t_skip("no Direct3DCreate9Ex"); return 0; }
    HRESULT hr = d9x_make(&d->d3d, &d->dev, &d->hwnd, &d->type);
    if (FAILED(hr) || !d->dev) {
        t_skip("no D3D9Ex device (%s)", d3dh_hr_name(hr));
        if (d->d3d) IDirect3D9Ex_Release(d->d3d);
        d3dh_destroy_window(d->hwnd);
        memset(d, 0, sizeof *d);
        return 0;
    }
    return 1;
}
static inline void d9x_close(D9X *d)
{
    if (d->dev) IDirect3DDevice9Ex_Release(d->dev);
    if (d->d3d) IDirect3D9Ex_Release(d->d3d);
    d3dh_destroy_window(d->hwnd);
    memset(d, 0, sizeof *d);
}
#define D9X_OPEN(d) D9X d; if (!d9x_open(&d)) return
#define D9X_END(d) d9x_close(&d)

static inline void d9_pp(D3DPRESENT_PARAMETERS *pp, HWND hw)
{
    memset(pp, 0, sizeof *pp);
    pp->Windowed = TRUE; pp->SwapEffect = D3DSWAPEFFECT_DISCARD; pp->BackBufferFormat = D3DFMT_UNKNOWN;
    pp->BackBufferWidth = 64; pp->BackBufferHeight = 64; pp->hDeviceWindow = hw;
}
/* CreateDevice on the same d3d / device type as the open fixture. */
static inline HRESULT d9_create(D9 *d, const D3DPRESENT_PARAMETERS *pp_in, DWORD flags, HWND focus, IDirect3DDevice9 **out)
{
    D3DPRESENT_PARAMETERS pp = *pp_in;
    *out = NULL;
    return IDirect3D9_CreateDevice(d->d3d, D3DADAPTER_DEFAULT, d->type, focus, flags, &pp, out);
}
#define SWVP D3DCREATE_SOFTWARE_VERTEXPROCESSING

/* Resource creation result classes: a device type/driver may not offer a format. */
static inline int d9_unsupported(HRESULT hr) { return hr == D3DERR_NOTAVAILABLE; }

static inline UINT d9_bpp(D3DFORMAT f)
{
    switch ((int)f) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A2B10G10R10: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
    case D3DFMT_G16R16: case D3DFMT_D24S8: case D3DFMT_D24X8: case D3DFMT_D32: case D3DFMT_R32F: case D3DFMT_A2R10G10B10: return 4;
    case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4:
    case D3DFMT_A8L8: case D3DFMT_V8U8: case D3DFMT_L16: case D3DFMT_D16: case D3DFMT_R16F: case D3DFMT_A8R3G3B2: return 2;
    case D3DFMT_R8G8B8: return 3;
    case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_P8: case D3DFMT_R3G3B2: case D3DFMT_A4L4: return 1;
    case D3DFMT_A16B16G16R16: case D3DFMT_A16B16G16R16F: case D3DFMT_G32R32F: return 8;
    case D3DFMT_A32B32G32R32F: return 16;
    case D3DFMT_G16R16F: return 4;
    }
    return 0;
}
/* block bytes for DXT family, 0 otherwise */
static inline UINT d9_blockbytes(D3DFORMAT f)
{
    switch ((int)f) {
    case D3DFMT_DXT1: return 8;
    case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5: return 16;
    }
    return 0;
}
static inline UINT d9_minpitch(D3DFORMAT f, UINT w)
{
    UINT b = d9_blockbytes(f);
    if (b) { UINT bw = (w + 3) / 4; return (bw ? bw : 1) * b; }
    return w * d9_bpp(f);
}
#endif
