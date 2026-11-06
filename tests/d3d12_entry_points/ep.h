/* Shared helpers for the d3d12_entry_points area. */
#ifndef EP_H
#define EP_H
#include "d3dh.h"

#define EP_PFX_UNUSED
#define HR_UNSUPPORTED   ((HRESULT)0x887A0004)
#define HR_MOREDATA      ((HRESULT)0x887A0003)
#define HR_NOTFOUND      ((HRESULT)0x887A0002)
#define HR_SDKMISSING    ((HRESULT)0x887A002D)
#define HR_NOINTERFACE   ((HRESULT)0x80004002)
#define HR_INVALIDARG    ((HRESULT)0x80070057)

#define EP_FL_12_2 ((D3D_FEATURE_LEVEL)0xc200)

/* GUIDs not present in the mingw headers */
static const GUID EP_IID_Debug6 = {0x82a816d6, 0x5d01, 0x4157, {0x97, 0xd0, 0x49, 0x75, 0x46, 0x3f, 0xd1, 0xed}};
static const GUID EP_IID_Device11 = {0x5405c344, 0xd457, 0x444e, {0xb4, 0xdd, 0x23, 0x66, 0xe4, 0x5a, 0xee, 0x70}};
static const GUID EP_IID_DeviceFactory = {0x61f307d3, 0xd34e, 0x4e7c, {0x83, 0x74, 0x3b, 0xa4, 0xde, 0x23, 0xcc, 0xcb}};
static const GUID EP_CLSID_DeviceFactory = {0x114863bf, 0xc386, 0x4aee, {0xb3, 0x9d, 0x8f, 0x0b, 0xbb, 0x06, 0x29, 0x55}};
static const GUID EP_EXP_ShaderModels = {0x76f5573e, 0xf13a, 0x40f5, {0xb2, 0x97, 0x81, 0xce, 0x9e, 0x18, 0x93, 0x3f}};
static const GUID EP_EXP_TiledTier4 = {0xc9c4725f, 0xa81a, 0x4f56, {0x8c, 0x5b, 0xc5, 0x10, 0x39, 0xd6, 0x94, 0xfb}};
static const GUID EP_EXP_MetaCommand = {0xc734c97e, 0x8077, 0x48c8, {0x9f, 0xdc, 0xd9, 0xd1, 0xdd, 0x31, 0xdd, 0x77}};
static const GUID EP_WKPDID_NameW = {0x4cca5fd8, 0x921f, 0x42c8, {0x85, 0x66, 0x70, 0xca, 0xc7, 0xc1, 0xf0, 0xe5}};
static const GUID EP_RANDOM_GUID = {0x0badf00d, 0x1234, 0x4abc, {0x9d, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab}};

static const GUID *const EP_DEVICE_IIDS[11] = {
    &IID_ID3D12Device, &IID_ID3D12Device1, &IID_ID3D12Device2, &IID_ID3D12Device3, &IID_ID3D12Device4,
    &IID_ID3D12Device5, &IID_ID3D12Device6, &IID_ID3D12Device7, &IID_ID3D12Device8, &IID_ID3D12Device9,
    &IID_ID3D12Device10 };

static LUID ep_luid(ID3D12Device *d) { LUID l; memset(&l, 0, sizeof l); d->lpVtbl->GetAdapterLuid(d, &l); return l; }

/* Create a WARP-preferred device or skip the calling test. */
#define EP_NEED_DEVICE(dev) \
    ID3D12Device *dev = NULL; \
    do { REQUIRE(d3dh_D3D12CreateDevice()); \
         HRESULT hr_dev_ = d3dh_make_device12(&dev, D3D_FEATURE_LEVEL_11_0, NULL); \
         if (FAILED(hr_dev_) || !dev) { t_skip("no D3D12 device (%s)", d3dh_hr_name(hr_dev_)); return; } } while (0)

/* Get the WARP adapter (IDXGIAdapter) or NULL. */
static IDXGIAdapter *ep_warp_adapter(void) {
    __typeof__(&CreateDXGIFactory1) cf = d3dh_CreateDXGIFactory1();
    IDXGIFactory4 *f = NULL; IDXGIAdapter *a = NULL;
    if (!cf || FAILED(cf(&IID_IDXGIFactory4, (void **)&f)) || !f) return NULL;
    if (FAILED(IDXGIFactory4_EnumWarpAdapter(f, &IID_IDXGIAdapter, (void **)&a))) a = NULL;
    IDXGIFactory4_Release(f);
    return a;
}
#define EP_NEED_WARP(ad) \
    IDXGIAdapter *ad = NULL; \
    do { REQUIRE(d3dh_D3D12CreateDevice()); ad = ep_warp_adapter(); \
         if (!ad) { t_skip("no WARP adapter"); return; } } while (0)

/* Can a D3D12 device be created on the WARP adapter at 11_0? (validation only) */
static int ep_warp_usable(IDXGIAdapter *ad) {
    __typeof__(&D3D12CreateDevice) cd = d3dh_D3D12CreateDevice();
    return cd && SUCCEEDED(cd((IUnknown *)ad, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL));
}
#define EP_NEED_WARP_USABLE(ad) \
    EP_NEED_WARP(ad); \
    if (!ep_warp_usable(ad)) { SAFE_RELEASE(ad); t_skip("WARP cannot create a D3D12 device"); return; }

/* Is this device on the WARP adapter? Numeric capability values below were measured on WARP (Win10 22H2); on other adapters they are only logged. */
static int ep_on_warp(ID3D12Device *d) {
    IDXGIAdapter *w = ep_warp_adapter(); int r = 0;
    if (w) { DXGI_ADAPTER_DESC ad; LUID l = ep_luid(d); if (SUCCEEDED(IDXGIAdapter_GetDesc(w, &ad))) r = ad.AdapterLuid.LowPart == l.LowPart && ad.AdapterLuid.HighPart == l.HighPart; IDXGIAdapter_Release(w); }
    return r;
}
static int ep_hr_in(HRESULT hr, const HRESULT *set, int n) { for (int i = 0; i < n; i++) if (set[i] == hr) return 1; return 0; }
#define EP_HR_IN(hr, ...) ({ static const HRESULT s_[] = { __VA_ARGS__ }; ep_hr_in((hr), s_, (int)(sizeof s_ / sizeof s_[0])); })
#define EXPECT_HR_IN(hr, ...) do { HRESULT h_ = (HRESULT)(hr); \
    t_check(EP_HR_IN(h_, __VA_ARGS__), __FILE__, __LINE__, #hr " in {" #__VA_ARGS__ "}", "got %s", d3dh_hr_name(h_)); } while (0)
#define EP_EQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#endif
