/* d3d8.dll (shipped with Windows 10 but legacy). This file deliberately includes only the D3D8 headers
   (they clash with the D3D9 types used elsewhere in this area). */
#define COBJMACROS
#define CINTERFACE
#include "t.h"
#include <objbase.h>
#include <d3d8.h>

typedef IDirect3D8 *(WINAPI *PFN_Create8)(UINT);
static PFN_Create8 create8(void) { return (PFN_Create8)t_proc_in("d3d8.dll", "Direct3DCreate8"); }
#define NEED8() do { if (!LoadLibraryA("d3d8.dll")) { t_skip("d3d8.dll absent"); return; } } while (0)
#define D3D8_SDK 220

TEST(d3dx_redist_optional_d3d8_presence_and_exports) {
    HMODULE m = LoadLibraryA("d3d8.dll"); t_note("d3d8.dll %s", m ? "present" : "absent");
    if (!m) { t_skip("d3d8.dll absent"); return; }
    static const char *n[] = { "Direct3DCreate8", "ValidatePixelShader", "ValidateVertexShader", "DebugSetMute", "Direct3D8EnableMaximizedWindowedModeShim" };
    for (unsigned i = 0; i < 5; i++) t_note("%s %s", n[i], GetProcAddress(m, n[i]) ? "exported" : "missing");
    CHECK(GetProcAddress(m, "Direct3DCreate8") != NULL);
}
TEST(d3dx_redist_optional_d3d8_create_and_refcount) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d && t_is_wine()) { t_skip("no IDirect3D8 in this emulator headless"); return; } CHECK(d != NULL); if (!d) return;
    ULONG r = IDirect3D8_AddRef(d); EXPECT_EQ(r, 2);
    r = IDirect3D8_Release(d); EXPECT_EQ(r, 1);
    r = IDirect3D8_Release(d); EXPECT_EQ(r, 0);
}
TEST(d3dx_redist_optional_d3d8_two_instances_are_distinct) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *a = f(D3D8_SDK), *b = f(D3D8_SDK);
    if (!a && t_is_wine()) { t_skip("no IDirect3D8 in this emulator headless"); return; }
    CHECK(a != NULL && b != NULL); if (a && b) CHECK(a != b);
    if (a) IDirect3D8_Release(a); if (b) IDirect3D8_Release(b);
}
TEST(d3dx_redist_optional_d3d8_wrong_sdk_version) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    static const UINT v[] = { 0, 1, 119, 219, 221, 32, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) { IDirect3D8 *d = f(v[i]); t_note("Direct3DCreate8(%u) -> %s", v[i], d ? "object" : "NULL"); if (d) IDirect3D8_Release(d); }
}
TEST(d3dx_redist_optional_d3d8_query_interface) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    IUnknown *u = NULL; HRESULT hr = IDirect3D8_QueryInterface(d, &IID_IUnknown, (void **)&u); CHECK(hr == S_OK); CHECK(u == (IUnknown *)d); if (u) IUnknown_Release(u);
    IDirect3D8 *same = NULL; hr = IDirect3D8_QueryInterface(d, &IID_IDirect3D8, (void **)&same); CHECK(hr == S_OK); if (same) IDirect3D8_Release(same);
    void *bad = (void *)1; hr = IDirect3D8_QueryInterface(d, &IID_IDirect3DDevice8, &bad); CHECK(hr == E_NOINTERFACE); CHECK(bad == NULL);
    IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_adapter_count_and_identifier) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    UINT n = IDirect3D8_GetAdapterCount(d); t_note("adapter count %u", n);
    if (n == 0) { IDirect3D8_Release(d); t_skip("no adapters"); return; }
    D3DADAPTER_IDENTIFIER8 id; memset(&id, 0, sizeof id);
    HRESULT hr = IDirect3D8_GetAdapterIdentifier(d, 0, 0, &id); CHECK(hr == D3D_OK);
    if (SUCCEEDED(hr)) { CHECK(id.Driver[0] != 0); CHECK(id.Description[0] != 0); t_note("adapter 0: %s / %s vendor 0x%lx device 0x%lx", id.Driver, id.Description, id.VendorId, id.DeviceId); }
    hr = IDirect3D8_GetAdapterIdentifier(d, n, 0, &id); CHECK(hr == D3DERR_INVALIDCALL);
    hr = IDirect3D8_GetAdapterIdentifier(d, 0xFFFF, 0, &id); CHECK(hr == D3DERR_INVALIDCALL);
    IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_adapter_display_mode) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    if (IDirect3D8_GetAdapterCount(d) == 0) { IDirect3D8_Release(d); t_skip("no adapters"); return; }
    D3DDISPLAYMODE m; memset(&m, 0, sizeof m); HRESULT hr = IDirect3D8_GetAdapterDisplayMode(d, 0, &m);
    CHECK(hr == D3D_OK); if (SUCCEEDED(hr)) { CHECK(m.Width > 0 && m.Height > 0); CHECK(m.Format != D3DFMT_UNKNOWN); t_note("desktop %ux%u @%u fmt %d", m.Width, m.Height, m.RefreshRate, m.Format); }
    hr = IDirect3D8_GetAdapterDisplayMode(d, 99, &m); CHECK(hr == D3DERR_INVALIDCALL);
    IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_adapter_mode_count) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    UINT n = IDirect3D8_GetAdapterModeCount(d, 0); t_note("mode count %u", n);
    CHECK(IDirect3D8_GetAdapterModeCount(d, 99) == 0);
    for (UINT i = 0; i < n && i < 3; i++) { D3DDISPLAYMODE m; CHECK(IDirect3D8_EnumAdapterModes(d, 0, i, &m) == D3D_OK); CHECK(m.Width > 0); }
    D3DDISPLAYMODE m; HRESULT hr = IDirect3D8_EnumAdapterModes(d, 0, 0xFFFFFF, &m); CHECK(hr == D3DERR_INVALIDCALL);
    IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_device_caps_hal) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    if (IDirect3D8_GetAdapterCount(d) == 0) { IDirect3D8_Release(d); t_skip("no adapters"); return; }
    D3DCAPS8 c; memset(&c, 0, sizeof c); HRESULT hr = IDirect3D8_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c);
    t_note("GetDeviceCaps(HAL) -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) { CHECK(c.DeviceType == D3DDEVTYPE_HAL); CHECK(c.AdapterOrdinal == 0); CHECK(c.MaxTextureWidth >= 256); CHECK(c.MaxTextureHeight >= 256); t_note("D3D8 caps: MaxTexture %lux%lu, PS 0x%lx VS 0x%lx", c.MaxTextureWidth, c.MaxTextureHeight, c.PixelShaderVersion, c.VertexShaderVersion); }
    hr = IDirect3D8_GetDeviceCaps(d, 99, D3DDEVTYPE_HAL, &c); CHECK(hr == D3DERR_INVALIDCALL);
    hr = IDirect3D8_GetDeviceCaps(d, 0, (D3DDEVTYPE)99, &c); CHECK(hr == D3DERR_INVALIDCALL);
    IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_check_device_type_and_format) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    if (IDirect3D8_GetAdapterCount(d) == 0) { IDirect3D8_Release(d); t_skip("no adapters"); return; }
    HRESULT hr = IDirect3D8_CheckDeviceType(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE); t_note("CheckDeviceType(HAL, X8R8G8B8 windowed) -> 0x%08lx", (unsigned long)hr);
    hr = IDirect3D8_CheckDeviceFormat(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8); t_note("CheckDeviceFormat(texture A8R8G8B8) -> 0x%08lx", (unsigned long)hr);
    hr = IDirect3D8_CheckDeviceFormat(d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_UNKNOWN); CHECK(FAILED(hr));
    hr = IDirect3D8_CheckDeviceFormat(d, 99, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8); CHECK(hr == D3DERR_INVALIDCALL);
    hr = IDirect3D8_CheckDeviceType(d, 99, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE); CHECK(hr == D3DERR_INVALIDCALL);
    IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_monitor_from_adapter) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    if (IDirect3D8_GetAdapterCount(d) == 0) { IDirect3D8_Release(d); t_skip("no adapters"); return; }
    HMONITOR m0 = IDirect3D8_GetAdapterMonitor(d, 0); CHECK(m0 != NULL);
    HMONITOR bad = IDirect3D8_GetAdapterMonitor(d, 99); CHECK(bad == NULL);
    IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_register_software_device_null) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    HRESULT hr = IDirect3D8_RegisterSoftwareDevice(d, NULL); t_note("RegisterSoftwareDevice(NULL) -> 0x%08lx", (unsigned long)hr); CHECK(FAILED(hr));
    IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_create_device_hal_windowed) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    if (IDirect3D8_GetAdapterCount(d) == 0) { IDirect3D8_Release(d); t_skip("no adapters"); return; }
    HWND w = CreateWindowExA(0, "STATIC", "d3dx_redist_d3d8", WS_POPUP, 0, 0, 64, 64, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!w) { IDirect3D8_Release(d); t_skip("no window"); return; }
    D3DPRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.hDeviceWindow = w;
    IDirect3DDevice8 *dev = NULL;
    HRESULT hr = IDirect3D8_CreateDevice(d, 0, D3DDEVTYPE_HAL, w, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
    t_note("CreateDevice(HAL, windowed, software VP) -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && dev) {
        D3DDEVICE_CREATION_PARAMETERS cp; memset(&cp, 0, sizeof cp);
        CHECK(IDirect3DDevice8_GetCreationParameters(dev, &cp) == D3D_OK);
        CHECK(cp.AdapterOrdinal == 0); CHECK(cp.DeviceType == D3DDEVTYPE_HAL); CHECK(cp.hFocusWindow == w); CHECK((cp.BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0);
        IDirect3D8 *back = NULL; CHECK(IDirect3DDevice8_GetDirect3D(dev, &back) == D3D_OK); CHECK(back == d); if (back) IDirect3D8_Release(back);
        IDirect3DTexture8 *t = NULL; hr = IDirect3DDevice8_CreateTexture(dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t); CHECK(hr == D3D_OK);
        if (t) { D3DSURFACE_DESC sd; CHECK(IDirect3DTexture8_GetLevelDesc(t, 0, &sd) == D3D_OK); CHECK(sd.Width == 4 && sd.Height == 4 && sd.Format == D3DFMT_A8R8G8B8 && sd.Pool == D3DPOOL_MANAGED); IDirect3DTexture8_Release(t); }
        IDirect3DTexture8 *bad = (IDirect3DTexture8 *)1;
        hr = IDirect3DDevice8_CreateTexture(dev, 0, 0, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &bad); t_note("CreateTexture(0x0) -> 0x%08lx", (unsigned long)hr); if (SUCCEEDED(hr) && bad != (IDirect3DTexture8 *)1) IDirect3DTexture8_Release(bad);
        ULONG rc = IDirect3DDevice8_Release(dev); CHECK(rc == 0);
    }
    DestroyWindow(w); IDirect3D8_Release(d);
}
TEST(d3dx_redist_optional_d3d8_create_device_invalid) {
    NEED8(); PFN_Create8 f = create8(); REQUIRE(f);
    IDirect3D8 *d = f(D3D8_SDK); if (!d) { t_skip("no IDirect3D8"); return; }
    if (IDirect3D8_GetAdapterCount(d) == 0) { IDirect3D8_Release(d); t_skip("no adapters"); return; }
    IDirect3DDevice8 *dev = NULL; D3DPRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
    HRESULT hr = IDirect3D8_CreateDevice(d, 99, D3DDEVTYPE_HAL, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev); CHECK(hr == D3DERR_INVALIDCALL); CHECK(dev == NULL);
    hr = IDirect3D8_CreateDevice(d, 0, (D3DDEVTYPE)77, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev); CHECK(hr == D3DERR_INVALIDCALL);
    hr = IDirect3D8_CreateDevice(d, 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, NULL, &dev); CHECK(hr == D3DERR_INVALIDCALL);
    IDirect3D8_Release(d);
}
