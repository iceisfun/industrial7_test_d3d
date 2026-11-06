/* CreateDirect3D11DeviceFromDXGIDevice / CreateDirect3D11SurfaceFromDXGISurface (WinRT interop in d3d11.dll). */
#include "dxgap0.h"

/* IInspectable, IDXGIInterfaceAccess */
static const GUID IID_IInspectable_ = { 0xAF86E2E0, 0xB12D, 0x4c6a, { 0x9C, 0x5A, 0xD7, 0xAA, 0x65, 0x10, 0x1E, 0x90 } };
static const GUID IID_IDXGIInterfaceAccess_ = { 0xA9B3D012, 0x3DF2, 0x4EE3, { 0xB8, 0xD1, 0x86, 0x95, 0xF4, 0x57, 0xD3, 0xC1 } };
typedef struct IDXGIInterfaceAccess IDXGIInterfaceAccess;
typedef struct { HRESULT (WINAPI *QueryInterface)(IDXGIInterfaceAccess *, REFIID, void **);
                 ULONG (WINAPI *AddRef)(IDXGIInterfaceAccess *); ULONG (WINAPI *Release)(IDXGIInterfaceAccess *);
                 HRESULT (WINAPI *GetInterface)(IDXGIInterfaceAccess *, REFIID, void **); } IDXGIInterfaceAccessVtbl;
struct IDXGIInterfaceAccess { const IDXGIInterfaceAccessVtbl *lpVtbl; };

typedef HRESULT (WINAPI *mkdev_t)(IDXGIDevice *, IUnknown **);
typedef HRESULT (WINAPI *mksurf_t)(IDXGISurface *, IUnknown **);

static int dev_and_dxgi(ID3D11Device **dev, IDXGIDevice **dx)
{
    HRESULT hr = d3dh_make_device11(dev, NULL, NULL, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT);
    if (FAILED(hr)) { t_skip("no D3D11 device: %s", d3dh_hr_name(hr)); return 0; }
    if (FAILED(ID3D11Device_QueryInterface(*dev, &IID_IDXGIDevice, (void **)dx))) { SAFE_RELEASE(*dev); t_skip("no IDXGIDevice"); return 0; }
    return 1;
}

TEST(dxgap0_interop_device_roundtrip)
{
    mkdev_t fn = (mkdev_t)t_proc_in("d3d11.dll", "CreateDirect3D11DeviceFromDXGIDevice");
    ID3D11Device *dev = NULL;
    IDXGIDevice *dx = NULL;
    IUnknown *winrt = NULL, *insp = NULL;
    IDXGIInterfaceAccess *acc = NULL;
    HRESULT hr;
    REQUIRE(fn);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!dev_and_dxgi(&dev, &dx)) return;
    hr = fn(dx, &winrt);
    EXPECT_HR(hr, S_OK);
    if (winrt) {
        EXPECT_HR(IUnknown_QueryInterface(winrt, &IID_IInspectable_, (void **)&insp), S_OK);
        SAFE_RELEASE(insp);
        hr = IUnknown_QueryInterface(winrt, &IID_IDXGIInterfaceAccess_, (void **)&acc);
        EXPECT_HR(hr, S_OK);
        if (acc) {
            IDXGIDevice *back = NULL;
            hr = acc->lpVtbl->GetInterface(acc, &IID_IDXGIDevice, (void **)&back);
            EXPECT_HR(hr, S_OK);
            CHECK(back == dx);                       /* the wrapper exposes the original DXGI device */
            SAFE_RELEASE(back);
            acc->lpVtbl->Release(acc);
        }
        { ULONG rc = IUnknown_Release(winrt); t_note("WinRT device wrapper refcount after release: %lu", rc); }
    }
    SAFE_RELEASE(dx); SAFE_RELEASE(dev);
}

TEST(dxgap0_interop_device_distinct_wrappers)
{
    mkdev_t fn = (mkdev_t)t_proc_in("d3d11.dll", "CreateDirect3D11DeviceFromDXGIDevice");
    ID3D11Device *dev = NULL;
    IDXGIDevice *dx = NULL;
    IUnknown *a = NULL, *b = NULL;
    REQUIRE(fn);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!dev_and_dxgi(&dev, &dx)) return;
    EXPECT_HR(fn(dx, &a), S_OK);
    EXPECT_HR(fn(dx, &b), S_OK);
    CHECK(a && b);
    t_note("two wrappers of one device: %p %p", (void *)a, (void *)b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    SAFE_RELEASE(dx); SAFE_RELEASE(dev);
}

TEST(dxgap0_interop_device_null_args)
{
    mkdev_t fn = (mkdev_t)t_proc_in("d3d11.dll", "CreateDirect3D11DeviceFromDXGIDevice");
    ID3D11Device *dev = NULL;
    IDXGIDevice *dx = NULL;
    IUnknown *out = NULL;
    HRESULT hr;
    REQUIRE(fn);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = GAP0_CALL(fn(NULL, &out));
    t_note("CreateDirect3D11DeviceFromDXGIDevice(NULL) = %s", d3dh_hr_name(hr));
    CHECK(out == NULL);
    if (!dev_and_dxgi(&dev, &dx)) return;
    hr = GAP0_CALL(fn(dx, NULL));
    t_note("CreateDirect3D11DeviceFromDXGIDevice(dev, NULL out) = %s", d3dh_hr_name(hr));
    SAFE_RELEASE(dx); SAFE_RELEASE(dev);
}

TEST(dxgap0_interop_surface_roundtrip)
{
    mksurf_t fn = (mksurf_t)t_proc_in("d3d11.dll", "CreateDirect3D11SurfaceFromDXGISurface");
    ID3D11Device *dev = NULL;
    IDXGIDevice *dx = NULL;
    ID3D11Texture2D *tex = NULL;
    IDXGISurface *surf = NULL;
    IUnknown *winrt = NULL, *insp = NULL;
    D3D11_TEXTURE2D_DESC td;
    HRESULT hr;
    REQUIRE(fn);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!dev_and_dxgi(&dev, &dx)) return;
    memset(&td, 0, sizeof td);
    td.Width = 16; td.Height = 16; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    EXPECT_HR(ID3D11Device_CreateTexture2D(dev, &td, NULL, &tex), S_OK);
    if (tex && SUCCEEDED(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGISurface, (void **)&surf))) {
        IDXGIInterfaceAccess *acc = NULL;
        hr = fn(surf, &winrt);
        EXPECT_HR(hr, S_OK);
        if (winrt) {
            EXPECT_HR(IUnknown_QueryInterface(winrt, &IID_IInspectable_, (void **)&insp), S_OK);
            SAFE_RELEASE(insp);
            if (SUCCEEDED(IUnknown_QueryInterface(winrt, &IID_IDXGIInterfaceAccess_, (void **)&acc))) {
                IDXGISurface *back = NULL;
                EXPECT_HR(acc->lpVtbl->GetInterface(acc, &IID_IDXGISurface, (void **)&back), S_OK);
                CHECK(back == surf);
                SAFE_RELEASE(back);
                acc->lpVtbl->Release(acc);
            } else t_note("wrapper lacks IDXGIInterfaceAccess");
            IUnknown_Release(winrt);
        }
    }
    {   IUnknown *out = NULL;
        hr = fn(NULL, &out);
        EXPECT_HR_FAILED(hr);
        CHECK(out == NULL);
    }
    SAFE_RELEASE(surf); SAFE_RELEASE(tex); SAFE_RELEASE(dx); SAFE_RELEASE(dev);
}
