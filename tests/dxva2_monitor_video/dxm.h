/* shared fixtures for the dxva2_monitor_video area */
#ifndef DXM_H
#define DXM_H
#include "d3dh.h"
#include <dxva2api.h>
#include <dxvahd.h>
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <opmapi.h>

#define DXM_GUID(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
    static const GUID name = { a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }
DXM_GUID(DXM_IID_Accel,   0xfc51a550, 0xd5e7, 0x11d9, 0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);
DXM_GUID(DXM_IID_DecSvc,  0xfc51a551, 0xd5e7, 0x11d9, 0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);
DXM_GUID(DXM_IID_ProcSvc, 0xfc51a552, 0xd5e7, 0x11d9, 0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);
DXM_GUID(DXM_IID_DevMgr9, 0xa0cade0f, 0x06d5, 0x4cf4, 0xa1,0xc7,0xf3,0xcd,0xd7,0x25,0xaa,0x75);
DXM_GUID(DXM_IID_Proc,    0x8c3a39f0, 0x916e, 0x4690, 0x80,0x4f,0x4c,0x80,0x01,0x35,0x5d,0x25);
DXM_GUID(DXM_SwProc,      0x4553d47f, 0xee7e, 0x4e3f, 0x94,0x75,0xdb,0xf1,0x37,0x6c,0x48,0x10);
DXM_GUID(DXM_IID_Unk,     0x00000000, 0x0000, 0x0000, 0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
/* a GUID that no component implements */
DXM_GUID(DXM_IID_Bogus,   0x7a1d0e55, 0x1234, 0x4abc, 0x9d,0x00,0x11,0x22,0x33,0x44,0x55,0x66);

/* EXPECT_EQ evaluates its first argument twice; use this for expressions with side effects */
#define DXM_EQ(expr, want) do { ULONG_PTR dxm_v_ = (ULONG_PTR)(expr); EXPECT_EQ(dxm_v_, want); } while (0)

#define DXM_E_NOT_INITIALIZED 0x80041000
#define DXM_E_NEW_VIDEO_DEVICE 0x80041001
#define DXM_E_VIDEO_DEVICE_LOCKED 0x80041002

/* ---- D3D9 device fixture ---- */
/* The shared d3dh helpers try NULLREF first; hal.c needs a device of an explicit class. */
static inline void dxm_pp(D3DPRESENT_PARAMETERS *pp, HWND hw)
{
    memset(pp, 0, sizeof *pp);
    pp->Windowed = TRUE; pp->SwapEffect = D3DSWAPEFFECT_DISCARD; pp->BackBufferFormat = D3DFMT_UNKNOWN;
    pp->BackBufferWidth = 64; pp->BackBufferHeight = 64; pp->hDeviceWindow = hw;
}
static inline HRESULT dxm_make9(IDirect3D9 **d3d, IDirect3DDevice9 **dev, HWND *hw_out, D3DDEVTYPE type)
{
    __typeof__(&Direct3DCreate9) c = d3dh_Direct3DCreate9();
    HWND hw = d3dh_create_hidden_window();
    HRESULT hr;
    *d3d = NULL; *dev = NULL; *hw_out = hw;
    if (!c) return E_FAIL;
    *d3d = c(D3D_SDK_VERSION);
    if (!*d3d) return D3DERR_NOTAVAILABLE;
    { D3DPRESENT_PARAMETERS pp; dxm_pp(&pp, hw);
      hr = IDirect3D9_CreateDevice(*d3d, D3DADAPTER_DEFAULT, type, hw, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, dev); }
    return hr;
}
typedef struct { IDirect3D9 *d3d; IDirect3DDevice9 *dev; HWND hwnd; D3DDEVTYPE type; } DXMD9;
static inline int dxm_d9_open(DXMD9 *d)
{
    memset(d, 0, sizeof *d);
    if (!d3dh_Direct3DCreate9()) { t_skip("no Direct3DCreate9"); return 0; }
    HRESULT hr = d3dh_make_device9(&d->d3d, &d->dev, NULL, &d->hwnd, &d->type);
    if (FAILED(hr) || !d->dev) {
        t_skip("no D3D9 device (%s)", d3dh_hr_name(hr));
        if (d->d3d) IDirect3D9_Release(d->d3d);
        d3dh_destroy_window(d->hwnd);
        memset(d, 0, sizeof *d);
        return 0;
    }
    return 1;
}
static inline void dxm_d9_close(DXMD9 *d)
{
    if (d->dev) IDirect3DDevice9_Release(d->dev);
    if (d->d3d) IDirect3D9_Release(d->d3d);
    d3dh_destroy_window(d->hwnd);
    memset(d, 0, sizeof *d);
}
#define DXM_D9(d) DXMD9 d; if (!dxm_d9_open(&d)) return
#define DXM_D9_END(d) dxm_d9_close(&d)

typedef struct { IDirect3D9Ex *d3d; IDirect3DDevice9Ex *dev; HWND hwnd; D3DDEVTYPE type; } DXMD9X;
static inline int dxm_d9x_open(DXMD9X *d)
{
    memset(d, 0, sizeof *d);
    if (!d3dh_Direct3DCreate9Ex()) { t_skip("no Direct3DCreate9Ex"); return 0; }
    HRESULT hr = d3dh_make_device9ex(&d->d3d, &d->dev, NULL, &d->hwnd, &d->type);
    if (FAILED(hr) || !d->dev) {
        t_skip("no D3D9Ex device (%s)", d3dh_hr_name(hr));
        if (d->d3d) IDirect3D9Ex_Release(d->d3d);
        d3dh_destroy_window(d->hwnd);
        memset(d, 0, sizeof *d);
        return 0;
    }
    return 1;
}
static inline void dxm_d9x_close(DXMD9X *d)
{
    if (d->dev) IDirect3DDevice9Ex_Release(d->dev);
    if (d->d3d) IDirect3D9Ex_Release(d->d3d);
    d3dh_destroy_window(d->hwnd);
    memset(d, 0, sizeof *d);
}
#define DXM_D9X(d) DXMD9X d; if (!dxm_d9x_open(&d)) return
#define DXM_D9X_END(d) dxm_d9x_close(&d)

/* A valid, progressive video description. */
static inline DXVA2_VideoDesc dxm_desc(UINT w, UINT h, D3DFORMAT fmt)
{
    DXVA2_VideoDesc d; memset(&d, 0, sizeof d);
    d.SampleWidth = w; d.SampleHeight = h; d.Format = fmt;
    d.InputSampleFreq.Numerator = 30; d.InputSampleFreq.Denominator = 1;
    d.OutputFrameFreq.Numerator = 30; d.OutputFrameFreq.Denominator = 1;
    return d;
}
/* Descriptions that the service rejects with E_INVALIDARG: a non-zero numerator over a zero denominator. */
static inline DXVA2_VideoDesc dxm_desc_badin(void) { DXVA2_VideoDesc d = dxm_desc(64, 64, (D3DFORMAT)MAKEFOURCC('N','V','1','2')); d.InputSampleFreq.Denominator = 0; return d; }
static inline DXVA2_VideoDesc dxm_desc_badout(void) { DXVA2_VideoDesc d = dxm_desc(64, 64, (D3DFORMAT)MAKEFOURCC('N','V','1','2')); d.OutputFrameFreq.Denominator = 0; return d; }
#define DXM_NV12 ((D3DFORMAT)MAKEFOURCC('N','V','1','2'))
#define DXM_YUY2 ((D3DFORMAT)MAKEFOURCC('Y','U','Y','2'))

/* Create the video service on a D3D9 device; skips when the runtime refuses (no usable device class). */
static inline IUnknown *dxm_service(IDirect3DDevice9 *dev, REFIID riid)
{
    HRESULT (WINAPI *create)(IDirect3DDevice9 *, REFIID, void **) = (void *)t_proc_in("dxva2.dll", "DXVA2CreateVideoService");
    void *p = NULL;
    if (!create) { t_skip("DXVA2CreateVideoService missing"); return NULL; }
    HRESULT hr = create(dev, riid, &p);
    if (FAILED(hr) || !p) { t_skip("DXVA2CreateVideoService failed on this device (%s)", d3dh_hr_name(hr)); return NULL; }
    return (IUnknown *)p;
}
#endif
