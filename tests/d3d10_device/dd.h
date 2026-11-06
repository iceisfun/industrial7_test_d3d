/* Shared helpers for the d3d10_device area. */
#ifndef DD_H
#define DD_H
#include "d3dh.h"
#include <d3d10sdklayers.h>

typedef HRESULT (WINAPI *dd_c10_fn)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, ID3D10Device **);
typedef HRESULT (WINAPI *dd_c10_1_fn)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT, ID3D10Device1 **);
typedef HRESULT (WINAPI *dd_c10sc_fn)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D10Device **);
typedef HRESULT (WINAPI *dd_c10sc1_fn)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D10Device1 **);
#define DD_C10()    ((dd_c10_fn)t_proc_in("d3d10.dll", "D3D10CreateDevice"))
#define DD_C10_1()  ((dd_c10_1_fn)t_proc_in("d3d10_1.dll", "D3D10CreateDevice1"))
#define DD_C10SC()  ((dd_c10sc_fn)t_proc_in("d3d10.dll", "D3D10CreateDeviceAndSwapChain"))
#define DD_C10SC1() ((dd_c10sc1_fn)t_proc_in("d3d10_1.dll", "D3D10CreateDeviceAndSwapChain1"))

#define DD_SENT_DEV ((ID3D10Device *)(ULONG_PTR)0xdeadbeef)

/* Driver type that can create a device here (WARP, REFERENCE, NULL, HARDWARE order); -1 if none. Cached per test exe. */
static inline int dd_driver(void)
{
    static int cached = -2;
    static const D3D10_DRIVER_TYPE order[] = { D3D10_DRIVER_TYPE_WARP, D3D10_DRIVER_TYPE_REFERENCE, D3D10_DRIVER_TYPE_NULL, D3D10_DRIVER_TYPE_HARDWARE };
    if (cached != -2) return cached;
    cached = -1;
    dd_c10_fn f = DD_C10();
    if (!f) return cached;
    for (int i = 0; i < 4; i++) {
        ID3D10Device *d = NULL;
        if (SUCCEEDED(f(NULL, order[i], NULL, 0, D3D10_SDK_VERSION, &d)) && d) {
            ID3D10Device_Release(d);
            cached = (int)order[i];
            break;
        }
    }
    return cached;
}
/* 1 when the device under test is the WARP software rasteriser (the only adapter on the Win10 reference VM) */
static inline int dd_warp(void) { return dd_driver() == D3D10_DRIVER_TYPE_WARP; }
static inline int dd_can_render(void)
{
    int d = dd_driver();
    return d == D3D10_DRIVER_TYPE_WARP || d == D3D10_DRIVER_TYPE_REFERENCE || d == D3D10_DRIVER_TYPE_HARDWARE;
}
static inline HRESULT dd_make(UINT flags, ID3D10Device **dev)
{
    int d = dd_driver();
    *dev = NULL;
    if (d < 0) return E_FAIL;
    return DD_C10()(NULL, (D3D10_DRIVER_TYPE)d, NULL, flags, D3D10_SDK_VERSION, dev);
}
static inline HRESULT dd_make1(UINT flags, D3D10_FEATURE_LEVEL1 fl, ID3D10Device1 **dev)
{
    int d = dd_driver();
    *dev = NULL;
    if (d < 0 || !DD_C10_1()) return E_FAIL;
    return DD_C10_1()(NULL, (D3D10_DRIVER_TYPE)d, NULL, flags, fl, D3D10_1_SDK_VERSION, dev);
}
/* declare + create a default device or skip the test */
#define DD_DEV(dev) ID3D10Device *dev = NULL; \
    do { if (!DD_C10()) { t_skip("no D3D10CreateDevice"); return; } \
         HRESULT hr_ = dd_make(0, &dev); if (FAILED(hr_) || !dev) { t_skip("no D3D10 device: %s", d3dh_hr_name(hr_)); return; } } while (0)
/* same, but needs a rasterising driver (not the NULL device) */
#define DD_DEV_R(dev) DD_DEV(dev); \
    do { if (!dd_can_render()) { ID3D10Device_Release(dev); t_skip("no rendering driver"); return; } } while (0)
#define DD_DEV1(dev, fl) ID3D10Device1 *dev = NULL; \
    do { if (!DD_C10_1()) { t_skip("no D3D10CreateDevice1"); return; } \
         HRESULT hr_ = dd_make1(0, fl, &dev); if (FAILED(hr_) || !dev) { t_skip("no D3D10.1 device: %s", d3dh_hr_name(hr_)); return; } } while (0)
#define DD_END(dev) do { ULONG rc_ = ID3D10Device_Release(dev); t_check(rc_ == 0, __FILE__, __LINE__, "device refcount after release", "got %lu", rc_); } while (0)

static inline const char *dd_fl_name(int f)
{
    switch (f) {
    case 0x9100: return "9_1"; case 0x9200: return "9_2"; case 0x9300: return "9_3";
    case 0xa000: return "10_0"; case 0xa100: return "10_1"; }
    return "?";
}

/* compile helper: returns 0 and skip-worthy note if the compiler failed */
static inline ID3DBlob *dd_blob(const char *src, const char *target)
{
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile(src, "main", target, 0, &b);
    if (FAILED(hr)) { t_note("compile %s failed %s: %s", target, d3dh_hr_name(hr), d3dh_compile_errors()); return NULL; }
    return b;
}

/* Basic shader sources (sm4). */
static const char DD_VS_POS[] =
    "float4 main(float4 p : POSITION) : SV_Position { return p; }\n";
static const char DD_VS_POS_COL[] =
    "struct O { float4 p : SV_Position; float4 c : COLOR; };\n"
    "O main(float4 p : POSITION, float4 c : COLOR) { O o; o.p = p; o.c = c; return o; }\n";
static const char DD_VS_ID[] =
    "float4 main(uint id : SV_VertexID) : SV_Position {\n"
    "  float2 uv = float2((id << 1) & 2, id & 2);\n"
    "  return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); }\n";
static const char DD_PS_RED[] = "float4 main() : SV_Target { return float4(1, 0, 0, 1); }\n";
static const char DD_PS_COL[] = "float4 main(float4 p : SV_Position, float4 c : COLOR) : SV_Target { return c; }\n";
static const char DD_PS_CB[] = "cbuffer C : register(b0) { float4 col; };\nfloat4 main() : SV_Target { return col; }\n";
static const char DD_GS_PASS[] =
    "struct V { float4 p : SV_Position; };\n"
    "[maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> s) { for (int k = 0; k < 3; k++) s.Append(i[k]); }\n";

static inline int dd_rgba_eq(UINT32 got, UINT32 want) { return got == want; }
#endif
