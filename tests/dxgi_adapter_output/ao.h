/* Shared helpers for the dxgi_adapter_output area. All DXGI entry points go through d3dh (LoadLibrary/GetProcAddress). */
#ifndef AO_H
#define AO_H
#include "d3dh.h"
#include <dxgi1_6.h>

#define AO_WARP_VENDOR 0x1414u
#define AO_WARP_DEVICE 0x8cu

/* Create a DXGI 1.1 factory or skip the test (returns NULL after t_skip). */
static IDXGIFactory1 *ao_factory(void) {
    IDXGIFactory1 *f = NULL;
    if (!d3dh_CreateDXGIFactory1()) { t_skip("dxgi.dll!CreateDXGIFactory1 missing"); return NULL; }
    HRESULT hr = d3dh_make_factory1(&f);
    if (FAILED(hr) || !f) { t_skip("CreateDXGIFactory1 failed: %s", d3dh_hr_name(hr)); return NULL; }
    return f;
}
static UINT ao_count_adapters(IDXGIFactory1 *f) {
    UINT n = 0;
    for (;; n++) {
        IDXGIAdapter1 *a = NULL;
        if (IDXGIFactory1_EnumAdapters1(f, n, &a) != S_OK) break;
        IDXGIAdapter1_Release(a);
        if (n > 64) break;
    }
    return n;
}
/* The WARP adapter: IDXGIFactory4::EnumWarpAdapter, else a scan for the Basic Render Driver ids. NULL if none. */
static IDXGIAdapter1 *ao_warp(IDXGIFactory1 *f) {
    IDXGIFactory4 *f4 = NULL; IDXGIAdapter1 *a = NULL;
    if (SUCCEEDED(IDXGIFactory1_QueryInterface(f, &IID_IDXGIFactory4, (void **)&f4)) && f4) {
        HRESULT hr = IDXGIFactory4_EnumWarpAdapter(f4, &IID_IDXGIAdapter1, (void **)&a);
        IDXGIFactory4_Release(f4);
        if (SUCCEEDED(hr) && a) return a;
        a = NULL;
    }
    for (UINT i = 0; i < 64; i++) {
        IDXGIAdapter1 *c = NULL; DXGI_ADAPTER_DESC1 d;
        if (IDXGIFactory1_EnumAdapters1(f, i, &c) != S_OK) break;
        if (SUCCEEDED(IDXGIAdapter1_GetDesc1(c, &d)) && d.VendorId == AO_WARP_VENDOR && d.DeviceId == AO_WARP_DEVICE) return c;
        IDXGIAdapter1_Release(c);
    }
    return NULL;
}
/* First output of any adapter. Returns the output (caller releases) and optionally its adapter. */
static IDXGIOutput *ao_any_output(IDXGIFactory1 *f, IDXGIAdapter1 **adapter_out) {
    for (UINT i = 0; i < 16; i++) {
        IDXGIAdapter1 *a = NULL; IDXGIOutput *o = NULL;
        if (IDXGIFactory1_EnumAdapters1(f, i, &a) != S_OK) break;
        if (IDXGIAdapter1_EnumOutputs(a, 0, &o) == S_OK && o) {
            if (adapter_out) *adapter_out = a; else IDXGIAdapter1_Release(a);
            return o;
        }
        IDXGIAdapter1_Release(a);
    }
    return NULL;
}
static int ao_same_object(IUnknown *a, IUnknown *b) {
    IUnknown *ua = NULL, *ub = NULL; int r;
    IUnknown_QueryInterface(a, &IID_IUnknown, (void **)&ua);
    IUnknown_QueryInterface(b, &IID_IUnknown, (void **)&ub);
    r = ua && ua == ub;
    if (ua) IUnknown_Release(ua);
    if (ub) IUnknown_Release(ub);
    return r;
}
/* SetVideoMemoryReservation answers E_INVALIDARG until a D3D device exists in the process (probed on Win10); keep one for the exe lifetime. */
static int ao_keep_device(void) {
    static ID3D11Device *dev; static ID3D11DeviceContext *ctx; static int tried;
    if (!tried) { tried = 1; D3D_FEATURE_LEVEL fl; D3D_DRIVER_TYPE dt; if (FAILED(d3dh_make_device11(&dev, &ctx, &fl, &dt, 0))) dev = NULL; }
    return dev != NULL;
}
static int ao_luid_eq(LUID a, LUID b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; }
static int ao_wide_terminated(const WCHAR *s, int cap) { for (int i = 0; i < cap; i++) if (!s[i]) return 1; return 0; }

/* Test wrappers: skip when nothing suitable exists, release everything afterwards.  Bodies may `return` early. */
#define AO_TEST_FACTORY(name) \
    static void name##_b(IDXGIFactory1 *f); \
    TEST(name) { IDXGIFactory1 *f = ao_factory(); if (!f) return; name##_b(f); IDXGIFactory1_Release(f); } \
    static void name##_b(IDXGIFactory1 *f)
#define AO_TEST_ADAPTER(name) \
    static void name##_b(IDXGIFactory1 *f, IDXGIAdapter1 *a); \
    TEST(name) { IDXGIFactory1 *f = ao_factory(); if (!f) return; IDXGIAdapter1 *a = NULL; \
        if (IDXGIFactory1_EnumAdapters1(f, 0, &a) != S_OK || !a) { t_skip("no adapter"); IDXGIFactory1_Release(f); return; } \
        name##_b(f, a); IDXGIAdapter1_Release(a); IDXGIFactory1_Release(f); } \
    static void name##_b(IDXGIFactory1 *f, IDXGIAdapter1 *a)
#define AO_TEST_WARP(name) \
    static void name##_b(IDXGIFactory1 *f, IDXGIAdapter1 *a); \
    TEST(name) { IDXGIFactory1 *f = ao_factory(); if (!f) return; IDXGIAdapter1 *a = ao_warp(f); \
        if (!a) { t_skip("no WARP adapter"); IDXGIFactory1_Release(f); return; } \
        name##_b(f, a); IDXGIAdapter1_Release(a); IDXGIFactory1_Release(f); } \
    static void name##_b(IDXGIFactory1 *f, IDXGIAdapter1 *a)
#define AO_TEST_OUTPUT(name) \
    static void name##_b(IDXGIFactory1 *f, IDXGIAdapter1 *a, IDXGIOutput *o); \
    TEST(name) { IDXGIFactory1 *f = ao_factory(); if (!f) return; IDXGIAdapter1 *a = NULL; IDXGIOutput *o = ao_any_output(f, &a); \
        if (!o) { t_skip("no output on any adapter (headless)"); IDXGIFactory1_Release(f); return; } \
        name##_b(f, a, o); IDXGIOutput_Release(o); IDXGIAdapter1_Release(a); IDXGIFactory1_Release(f); } \
    static void name##_b(IDXGIFactory1 *f, IDXGIAdapter1 *a, IDXGIOutput *o)
#endif
