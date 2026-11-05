/* Shared helpers for the d3d9_core area. */
#ifndef D9C_H
#define D9C_H
#include "d3dh.h"

#define D9_INVALIDCALL  ((HRESULT)0x8876086C)
#define D9_NOTAVAILABLE ((HRESULT)0x8876086A)
#define D9_NOAUTOGEN    ((HRESULT)0x0876086F)
#define D9_E_NOINTERFACE ((HRESULT)0x80004002)

#define D9_CREATE9()   UIN("d3d9.dll", Direct3DCreate9)
#define D9_CREATE9EX() UIN("d3d9.dll", Direct3DCreate9Ex)

/* Obtain an IDirect3D9 or skip the calling test. */
#define D9_GET(d3d) \
    IDirect3D9 *d3d = NULL; \
    do { __typeof__(&Direct3DCreate9) c_ = D9_CREATE9(); \
         if (!c_) { t_skip("missing export: Direct3DCreate9"); return; } \
         d3d = c_(D3D_SDK_VERSION); \
         if (!d3d) { t_skip("Direct3DCreate9 returned NULL"); return; } } while (0)

#define D9_GET_EX(d3d) \
    IDirect3D9Ex *d3d = NULL; \
    do { __typeof__(&Direct3DCreate9Ex) c_ = D9_CREATE9EX(); \
         if (!c_) { t_skip("missing export: Direct3DCreate9Ex"); return; } \
         HRESULT h_ = c_(D3D_SDK_VERSION, &d3d); \
         if (FAILED(h_) || !d3d) { t_skip("Direct3DCreate9Ex failed %s", d3dh_hr_name(h_)); return; } } while (0)

/* True when the adapter 0 can describe the (X8R8G8B8 windowed) HAL format table; used to guard device-type dependent checks. */
static inline int d9_have_devtype(IDirect3D9 *d3d, D3DDEVTYPE t) {
    return SUCCEEDED(IDirect3D9_CheckDeviceFormat(d3d, 0, t, D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE, D3DFMT_X8R8G8B8));
}
#endif
