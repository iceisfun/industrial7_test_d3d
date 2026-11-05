/* Shared helpers for the d3d11_device_create area. */
#ifndef DC_H
#define DC_H
#include "d3dh.h"
#include <d3d11_4.h>
#include <d3d11on12.h>
#include <dxgi1_6.h>

#define DC_ERRMASK_SDK_COMPONENT_MISSING ((HRESULT)0x887A002D)

/* Create a WARP device through the exported entry point. Any out pointer may be NULL. */
static inline HRESULT dc_warp(UINT flags, const D3D_FEATURE_LEVEL *fls, UINT n,
                              ID3D11Device **dev, ID3D11DeviceContext **ctx, D3D_FEATURE_LEVEL *fl)
{
    HRESULT (WINAPI *f)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
                        ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**) =
        (void *)t_proc_in("d3d11.dll", "D3D11CreateDevice");
    if (!f) return E_NOTIMPL;
    return f(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, fls, n, D3D11_SDK_VERSION, dev, fl, ctx);
}

/* Create a default WARP device; skips the calling test (and returns 0 from the macro user) on failure. */
#define DC_NEED_WARP(dev, ctx, fl) do { \
    HRESULT hr_ = dc_warp(0, NULL, 0, &(dev), &(ctx), &(fl)); \
    if (FAILED(hr_)) { t_skip("no WARP device: %s", d3dh_hr_name(hr_)); return; } } while (0)

#define DC_REL(dev, ctx) do { SAFE_RELEASE(ctx); SAFE_RELEASE(dev); } while (0)

static inline int dc_fl_known(D3D_FEATURE_LEVEL f)
{
    switch ((int)f) {
    case 0x9100: case 0x9200: case 0x9300: case 0xa000: case 0xa100:
    case 0xb000: case 0xb100: case 0xc000: case 0xc100: return 1;
    }
    return 0;
}
static inline const char *dc_fl_name(D3D_FEATURE_LEVEL f)
{
    switch ((int)f) {
    case 0x9100: return "9_1"; case 0x9200: return "9_2"; case 0x9300: return "9_3";
    case 0xa000: return "10_0"; case 0xa100: return "10_1"; case 0xb000: return "11_0";
    case 0xb100: return "11_1"; case 0xc000: return "12_0"; case 0xc100: return "12_1";
    case 0xc200: return "12_2"; case 0x1000: return "1_0_core"; case 0: return "0";
    }
    return "?";
}

/* Adapter helpers (via dxgi.dll only). Both return 0 on failure. */
static inline int dc_get_adapter0(IDXGIAdapter **out)
{
    IDXGIFactory1 *f = NULL;
    *out = NULL;
    if (FAILED(d3dh_make_factory1(&f)) || !f) return 0;
    if (FAILED(IDXGIFactory1_EnumAdapters(f, 0, out))) *out = NULL;
    IDXGIFactory1_Release(f);
    return *out != NULL;
}
static inline int dc_get_warp_adapter(IDXGIAdapter **out)
{
    IDXGIFactory1 *f = NULL;
    IDXGIFactory4 *f4 = NULL;
    *out = NULL;
    if (FAILED(d3dh_make_factory1(&f)) || !f) return 0;
    if (SUCCEEDED(IDXGIFactory1_QueryInterface(f, &IID_IDXGIFactory4, (void **)&f4))) {
        if (FAILED(IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter, (void **)out))) *out = NULL;
        IDXGIFactory4_Release(f4);
    }
    IDXGIFactory1_Release(f);
    return *out != NULL;
}

typedef HRESULT (WINAPI *dc_create_fn)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
                                       ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
typedef HRESULT (WINAPI *dc_create_sc_fn)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
                                          const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*,
                                          ID3D11DeviceContext**);
#define DC_FN()    ((dc_create_fn)t_proc_in("d3d11.dll", "D3D11CreateDevice"))
#define DC_SC_FN() ((dc_create_sc_fn)t_proc_in("d3d11.dll", "D3D11CreateDeviceAndSwapChain"))
#define DC_FL_SENTINEL ((D3D_FEATURE_LEVEL)0x12345678)

/* Call D3D11CreateDevice expecting an argument error: result, and every output cleared. */
static inline void dc_expect_fail_cleared(HRESULT want, IDXGIAdapter *ad, D3D_DRIVER_TYPE dt, HMODULE sw, UINT flags,
                                          const D3D_FEATURE_LEVEL *fls, UINT n, UINT sdk, const char *what)
{
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
    HRESULT hr = DC_FN()(ad, dt, sw, flags, fls, n, sdk, &dev, &fl, &ctx);
    if (want) t_check(hr == want, __FILE__, __LINE__, what, "got %s want %s", d3dh_hr_name(hr), d3dh_hr_name(want));
    else t_check(FAILED(hr), __FILE__, __LINE__, what, "got %s want failure", d3dh_hr_name(hr));
    t_check(dev == NULL, __FILE__, __LINE__, what, "device output not cleared");
    t_check(ctx == NULL, __FILE__, __LINE__, what, "context output not cleared");
    t_check(fl == 0, __FILE__, __LINE__, what, "feature level output = 0x%x, want 0", (unsigned)fl);
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
}
#endif
