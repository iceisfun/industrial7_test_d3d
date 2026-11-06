/* Shared helpers for the dxgi_objects_resources area. */
#ifndef OBJS_H
#define OBJS_H
#include "d3dh.h"
#include <d3d11_1.h>
#include <dxgi1_5.h>

/* Open D3D11 device (WARP preferred) with its IDXGIDevice. */
typedef struct {
    ID3D11Device *dev;
    ID3D11DeviceContext *ctx;
    IDXGIDevice *dx;
    D3D_DRIVER_TYPE drv;
    D3D_FEATURE_LEVEL fl;
} objs_ctx;

static int objs_open(objs_ctx *c)
{
    memset(c, 0, sizeof(*c));
    if (!d3dh_D3D11CreateDevice()) return 0;
    if (FAILED(d3dh_make_device11(&c->dev, &c->ctx, &c->fl, &c->drv, D3D11_CREATE_DEVICE_BGRA_SUPPORT))) return 0;
    if (FAILED(ID3D11Device_QueryInterface(c->dev, &IID_IDXGIDevice, (void **)&c->dx))) {
        SAFE_RELEASE(c->ctx); SAFE_RELEASE(c->dev); return 0;
    }
    return 1;
}
static void objs_close(objs_ctx *c)
{
    SAFE_RELEASE(c->dx); SAFE_RELEASE(c->ctx); SAFE_RELEASE(c->dev);
}
#define NEED_DEV(c) objs_ctx c; \
    do { if (!objs_open(&c)) { t_skip("no D3D11 device"); return; } } while (0)

/* The display adapter that owns the output. On the oracle this is factory adapter 0 (Microsoft Basic Render Driver, flags 0, one output on
 * the desktop), which is NOT the adapter D3D11CreateDevice(NULL, HARDWARE/WARP) ends up on (factory adapter 1, software, no outputs). */
static int objs_first_output(IDXGIAdapter **adapter, IDXGIOutput **output)
{
    IDXGIFactory1 *f = NULL;
    UINT i;
    *adapter = NULL; *output = NULL;
    if (FAILED(d3dh_make_factory1(&f))) return 0;
    for (i = 0; ; i++) {
        IDXGIAdapter1 *a1 = NULL;
        IDXGIOutput *o = NULL;
        if (IDXGIFactory1_EnumAdapters1(f, i, &a1) != S_OK) break;
        if (IDXGIAdapter1_EnumOutputs(a1, 0, &o) == S_OK) {
            *adapter = (IDXGIAdapter *)a1; *output = o;
            IDXGIFactory1_Release(f);
            return 1;
        }
        IDXGIAdapter1_Release(a1);
    }
    IDXGIFactory1_Release(f);
    return 0;
}

static HRESULT objs_tex2d(ID3D11Device *d, UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT f,
                          D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, ID3D11Texture2D **out)
{
    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = w; desc.Height = h; desc.MipLevels = mips; desc.ArraySize = arr; desc.Format = f;
    desc.SampleDesc.Count = 1; desc.Usage = u; desc.BindFlags = bind; desc.CPUAccessFlags = cpu; desc.MiscFlags = misc;
    return ID3D11Device_CreateTexture2D(d, &desc, NULL, out);
}
static HRESULT objs_buf(ID3D11Device *d, UINT bytes, D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, ID3D11Buffer **out)
{
    D3D11_BUFFER_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.ByteWidth = bytes; desc.Usage = u; desc.BindFlags = bind; desc.CPUAccessFlags = cpu; desc.MiscFlags = misc;
    return ID3D11Device_CreateBuffer(d, &desc, NULL, out);
}
/* Default 64x64 BGRA render-target texture. */
static HRESULT objs_rt(ID3D11Device *d, ID3D11Texture2D **out)
{
    return objs_tex2d(d, 64, 64, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_USAGE_DEFAULT,
                      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 0, out);
}
/* Staging texture with CPU read+write. */
static HRESULT objs_staging(ID3D11Device *d, UINT w, UINT h, DXGI_FORMAT f, ID3D11Texture2D **out)
{
    return objs_tex2d(d, w, h, 1, 1, f, D3D11_USAGE_STAGING, 0,
                      D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, out);
}

static const GUID OBJS_G1 = {0x7a1b0c01, 0x1111, 0x4a01, {0x80, 0x01, 0x10, 0x20, 0x30, 0x40, 0x50, 0x61}};
static const GUID OBJS_G2 = {0x7a1b0c02, 0x2222, 0x4a02, {0x80, 0x02, 0x10, 0x20, 0x30, 0x40, 0x50, 0x62}};
static const GUID OBJS_G3 = {0x7a1b0c03, 0x3333, 0x4a03, {0x80, 0x03, 0x10, 0x20, 0x30, 0x40, 0x50, 0x63}};
static const GUID OBJS_GMISSING = {0x7a1b0cff, 0xffff, 0x4aff, {0x80, 0xff, 0x10, 0x20, 0x30, 0x40, 0x50, 0xff}};
/* WKPDID_D3DDebugObjectName */
static const GUID OBJS_WKPDID_NAME = {0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};

/* A small refcounted IUnknown used as private-data interface payload. */
typedef struct { IUnknown iface; LONG ref; LONG *destroyed; } objs_unk;
static HRESULT WINAPI objs_unk_qi(IUnknown *i, REFIID riid, void **o)
{
    if (IsEqualGUID(riid, &IID_IUnknown)) { *o = i; IUnknown_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI objs_unk_addref(IUnknown *i) { return InterlockedIncrement(&((objs_unk *)i)->ref); }
static ULONG WINAPI objs_unk_release(IUnknown *i)
{
    objs_unk *u = (objs_unk *)i;
    LONG r = InterlockedDecrement(&u->ref);
    if (!r) { if (u->destroyed) InterlockedIncrement(u->destroyed); HeapFree(GetProcessHeap(), 0, u); }
    return r;
}
static IUnknownVtbl objs_unk_vtbl = { objs_unk_qi, objs_unk_addref, objs_unk_release };
static IUnknown *objs_unk_create(LONG *destroyed)
{
    objs_unk *u = HeapAlloc(GetProcessHeap(), 0, sizeof(*u));
    u->iface.lpVtbl = &objs_unk_vtbl; u->ref = 1; u->destroyed = destroyed;
    return &u->iface;
}
static LONG objs_unk_refs(IUnknown *i) { return ((objs_unk *)i)->ref; }

#define OBJS_ARRAY(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif
