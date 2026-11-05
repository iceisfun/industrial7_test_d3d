/* GetDesc normalisation, full mip chain length, resource type, QueryInterface matrix, DXGI sub-object
 * behaviour, private data protocol, eviction priority, multisample/format-support queries. */
#include "txh.h"

/* ---- full-chain mip count: floor(log2(max(w,h,d))) + 1 ----------------------------------------------- */
#define MIP2D(nm, W, H, EXP) DEVTEST(d3d11_textures_mipcount_2d_##nm) { \
    ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC d; \
    EXPECT_HR(tx_mk2d(c, W, H, 0, 1, F_R8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return; \
    ID3D11Texture2D_GetDesc(t, &d); EXPECT_EQ(d.MipLevels, EXP); EXPECT_EQ(d.Width, W); EXPECT_EQ(d.Height, H); ID3D11Texture2D_Release(t); }
MIP2D(1x1, 1, 1, 1)
MIP2D(2x1, 2, 1, 2)
MIP2D(1x2, 1, 2, 2)
MIP2D(2x2, 2, 2, 2)
MIP2D(3x3, 3, 3, 2)
MIP2D(4x4, 4, 4, 3)
MIP2D(5x5, 5, 5, 3)
MIP2D(7x7, 7, 7, 3)
MIP2D(8x8, 8, 8, 4)
MIP2D(9x9, 9, 9, 4)
MIP2D(15x15, 15, 15, 4)
MIP2D(16x16, 16, 16, 5)
MIP2D(17x3, 17, 3, 5)
MIP2D(3x17, 3, 17, 5)
MIP2D(31x31, 31, 31, 5)
MIP2D(32x32, 32, 32, 6)
MIP2D(100x100, 100, 100, 7)
MIP2D(255x255, 255, 255, 8)
MIP2D(256x256, 256, 256, 9)
MIP2D(257x257, 257, 257, 9)
MIP2D(511x1, 511, 1, 9)
MIP2D(512x512, 512, 512, 10)
MIP2D(1000x600, 1000, 600, 10)
MIP2D(1023x1, 1023, 1, 10)
MIP2D(1024x1, 1024, 1, 11)
MIP2D(1x1025, 1, 1025, 11)
MIP2D(2048x2048, 2048, 2048, 12)
MIP2D(4096x1, 4096, 1, 13)
MIP2D(8192x1, 8192, 1, 14)

#define MIP1D(nm, W, EXP) DEVTEST(d3d11_textures_mipcount_1d_##nm) { \
    ID3D11Texture1D *t = NULL; D3D11_TEXTURE1D_DESC d; \
    EXPECT_HR(tx_mk1d(c, W, 0, 1, F_R8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return; \
    ID3D11Texture1D_GetDesc(t, &d); EXPECT_EQ(d.MipLevels, EXP); ID3D11Texture1D_Release(t); }
MIP1D(1, 1, 1)
MIP1D(2, 2, 2)
MIP1D(3, 3, 2)
MIP1D(4, 4, 3)
MIP1D(100, 100, 7)
MIP1D(127, 127, 7)
MIP1D(128, 128, 8)
MIP1D(129, 129, 8)
MIP1D(512, 512, 10)
MIP1D(4096, 4096, 13)
MIP1D(8192, 8192, 14)

#define MIP3D(nm, W, H, DP, EXP) DEVTEST(d3d11_textures_mipcount_3d_##nm) { \
    ID3D11Texture3D *t = NULL; D3D11_TEXTURE3D_DESC d; \
    EXPECT_HR(tx_mk3d(c, W, H, DP, 0, F_R8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return; \
    ID3D11Texture3D_GetDesc(t, &d); EXPECT_EQ(d.MipLevels, EXP); ID3D11Texture3D_Release(t); }
MIP3D(1x1x1, 1, 1, 1, 1)
MIP3D(2x2x2, 2, 2, 2, 2)
MIP3D(4x4x4, 4, 4, 4, 3)
MIP3D(8x2x2, 8, 2, 2, 4)
MIP3D(2x2x16, 2, 2, 16, 5)
MIP3D(3x5x7, 3, 5, 7, 3)
MIP3D(64x64x64, 64, 64, 64, 7)
MIP3D(128x1x1, 128, 1, 1, 8)
MIP3D(1x128x1, 1, 128, 1, 8)
MIP3D(1x1x128, 1, 1, 128, 8)
MIP3D(32x8x2, 32, 8, 2, 6)
MIP3D(5x5x300, 5, 5, 300, 9)
MIP3D(100x10x1, 100, 10, 1, 7)
MIP3D(2048x1x1, 2048, 1, 1, 12)

/* ---- GetDesc / GetType ------------------------------------------------------------------------------------- */
DEVTEST(d3d11_textures_getdesc_overwrites_struct_2d) {
    ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC d;
    EXPECT_HR(tx_mk2d(c, 8, 4, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    memset(&d, 0xCC, sizeof d);
    ID3D11Texture2D_GetDesc(t, &d);
    EXPECT_EQ(d.Width, 8); EXPECT_EQ(d.Height, 4); EXPECT_EQ(d.MipLevels, 1); EXPECT_EQ(d.ArraySize, 1);
    EXPECT_EQ(d.Format, F_RGBA8); EXPECT_EQ(d.SampleDesc.Count, 1); EXPECT_EQ(d.SampleDesc.Quality, 0);
    EXPECT_EQ(d.Usage, U_DEF); EXPECT_EQ(d.BindFlags, B_SRV); EXPECT_EQ(d.CPUAccessFlags, 0); EXPECT_EQ(d.MiscFlags, 0);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_getdesc_overwrites_struct_1d) {
    ID3D11Texture1D *t = NULL; D3D11_TEXTURE1D_DESC d;
    EXPECT_HR(tx_mk1d(c, 8, 1, 2, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    memset(&d, 0xCC, sizeof d);
    ID3D11Texture1D_GetDesc(t, &d);
    EXPECT_EQ(d.Width, 8); EXPECT_EQ(d.MipLevels, 1); EXPECT_EQ(d.ArraySize, 2); EXPECT_EQ(d.Format, F_RGBA8);
    EXPECT_EQ(d.Usage, U_DEF); EXPECT_EQ(d.BindFlags, B_RT); EXPECT_EQ(d.CPUAccessFlags, 0); EXPECT_EQ(d.MiscFlags, 0);
    ID3D11Texture1D_Release(t);
}
DEVTEST(d3d11_textures_getdesc_overwrites_struct_3d) {
    ID3D11Texture3D *t = NULL; D3D11_TEXTURE3D_DESC d;
    EXPECT_HR(tx_mk3d(c, 4, 2, 8, 1, F_RGBA8, U_STG, 0, CPU_R, 0, NULL, &t), S_OK); if (!t) return;
    memset(&d, 0xCC, sizeof d);
    ID3D11Texture3D_GetDesc(t, &d);
    EXPECT_EQ(d.Width, 4); EXPECT_EQ(d.Height, 2); EXPECT_EQ(d.Depth, 8); EXPECT_EQ(d.MipLevels, 1);
    EXPECT_EQ(d.Usage, U_STG); EXPECT_EQ(d.BindFlags, 0); EXPECT_EQ(d.CPUAccessFlags, CPU_R); EXPECT_EQ(d.MiscFlags, 0);
    ID3D11Texture3D_Release(t);
}
DEVTEST(d3d11_textures_getdesc_typeless_format_kept) {
    ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC d;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, U_DEF, B_SRV | B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    ID3D11Texture2D_GetDesc(t, &d);
    EXPECT_EQ(d.Format, DXGI_FORMAT_R8G8B8A8_TYPELESS);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_getdesc_msaa_roundtrip) {
    D3D11_TEXTURE2D_DESC d, g; ID3D11Texture2D *t = NULL; UINT q = 0;
    if (FAILED(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 4, &q)) || !q) { t_skip("4x MSAA unsupported"); return; }
    memset(&d, 0, sizeof d);
    d.Width = 32; d.Height = 16; d.MipLevels = 1; d.ArraySize = 2; d.Format = F_RGBA8; d.SampleDesc.Count = 4; d.Usage = U_DEF; d.BindFlags = B_RT | B_SRV;
    EXPECT_HR(ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &t), S_OK); if (!t) return;
    ID3D11Texture2D_GetDesc(t, &g);
    EXPECT_EQ(g.SampleDesc.Count, 4); EXPECT_EQ(g.SampleDesc.Quality, 0); EXPECT_EQ(g.ArraySize, 2); EXPECT_EQ(g.MipLevels, 1);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_getdesc_mip_request_explicit_kept) {
    ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC d;
    EXPECT_HR(tx_mk2d(c, 64, 64, 3, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    ID3D11Texture2D_GetDesc(t, &d); EXPECT_EQ(d.MipLevels, 3);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_gettype_1d) {
    ID3D11Texture1D *t = NULL; D3D11_RESOURCE_DIMENSION dim = (D3D11_RESOURCE_DIMENSION)99;
    EXPECT_HR(tx_mk1d(c, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    ID3D11Texture1D_GetType(t, &dim); EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_TEXTURE1D);
    ID3D11Texture1D_Release(t);
}
DEVTEST(d3d11_textures_gettype_2d) {
    ID3D11Texture2D *t = NULL; D3D11_RESOURCE_DIMENSION dim = (D3D11_RESOURCE_DIMENSION)99;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    ID3D11Texture2D_GetType(t, &dim); EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_TEXTURE2D);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_gettype_3d) {
    ID3D11Texture3D *t = NULL; D3D11_RESOURCE_DIMENSION dim = (D3D11_RESOURCE_DIMENSION)99;
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    ID3D11Texture3D_GetType(t, &dim); EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_TEXTURE3D);
    ID3D11Texture3D_Release(t);
}
DEVTEST(d3d11_textures_gettype_cube_is_2d) {
    ID3D11Texture2D *t = NULL; D3D11_RESOURCE_DIMENSION dim = (D3D11_RESOURCE_DIMENSION)99;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 6, F_RGBA8, U_DEF, B_SRV, 0, M_CUBE, NULL, &t), S_OK); if (!t) return;
    ID3D11Texture2D_GetType(t, &dim); EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_TEXTURE2D);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_getdevice_identity_and_refcount) {
    ID3D11Texture2D *t = NULL; ID3D11Device *dev = NULL; ULONG before, after;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    before = d3dh_refcount(c->dev);
    ID3D11Texture2D_GetDevice(t, &dev);
    EXPECT_EQ(dev, c->dev);
    after = d3dh_refcount(c->dev);
    EXPECT_EQ(after, before + 1);
    SAFE_RELEASE(dev);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_getdevice_1d_3d) {
    ID3D11Texture1D *t1 = NULL; ID3D11Texture3D *t3 = NULL; ID3D11Device *dev = NULL;
    EXPECT_HR(tx_mk1d(c, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t1), S_OK);
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t3), S_OK);
    if (t1) { ID3D11Texture1D_GetDevice(t1, &dev); EXPECT_EQ(dev, c->dev); SAFE_RELEASE(dev); ID3D11Texture1D_Release(t1); }
    if (t3) { ID3D11Texture3D_GetDevice(t3, &dev); EXPECT_EQ(dev, c->dev); SAFE_RELEASE(dev); ID3D11Texture3D_Release(t3); }
}

/* ---- QueryInterface matrix -------------------------------------------------------------------------------- */
static void tx_qi_expect(IUnknown *u, REFIID iid, HRESULT want, const char *file, int line) {
    void *p = (void *)(ULONG_PTR)0xDEADBEEF; HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    t_check(hr == want, file, line, "QueryInterface", "%s: got %s want %s", d3dh_guid_str(iid), d3dh_hr_name(hr), d3dh_hr_name(want));
    if (SUCCEEDED(hr)) IUnknown_Release((IUnknown *)p);
    else t_check(p == NULL, file, line, "QueryInterface failure clears output", "out=%p", p);
}
#define QI2D(nm, IID, WANT) DEVTEST(d3d11_textures_qi_2d_##nm) { ID3D11Texture2D *t = NULL; \
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_SRV | B_RT, 0, 0, NULL, &t), S_OK); if (!t) return; \
    tx_qi_expect((IUnknown *)t, &IID, WANT, __FILE__, __LINE__); ID3D11Texture2D_Release(t); }
QI2D(unknown, IID_IUnknown, S_OK)
QI2D(devicechild, IID_ID3D11DeviceChild, S_OK)
QI2D(resource, IID_ID3D11Resource, S_OK)
QI2D(self, IID_ID3D11Texture2D, S_OK)
QI2D(dxgiobject, IID_IDXGIObject, S_OK)
QI2D(dxgisubobject, IID_IDXGIDeviceSubObject, S_OK)
QI2D(dxgiresource, IID_IDXGIResource, S_OK)
QI2D(dxgiresource1, IID_IDXGIResource1, S_OK)
QI2D(dxgisurface, IID_IDXGISurface, S_OK)
QI2D(dxgisurface1, IID_IDXGISurface1, S_OK)
QI2D(tex1d, IID_ID3D11Texture1D, E_NOINTERFACE)
QI2D(tex3d, IID_ID3D11Texture3D, E_NOINTERFACE)
QI2D(buffer, IID_ID3D11Buffer, E_NOINTERFACE)
QI2D(srv, IID_ID3D11ShaderResourceView, E_NOINTERFACE)
QI2D(ctx, IID_ID3D11DeviceContext, E_NOINTERFACE)
QI2D(device, IID_ID3D11Device, E_NOINTERFACE)
QI2D(keyedmutex, IID_IDXGIKeyedMutex, E_NOINTERFACE)
QI2D(factory, IID_IDXGIFactory, E_NOINTERFACE)

#define QI1D(nm, IID, WANT) DEVTEST(d3d11_textures_qi_1d_##nm) { ID3D11Texture1D *t = NULL; \
    EXPECT_HR(tx_mk1d(c, 16, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return; \
    tx_qi_expect((IUnknown *)t, &IID, WANT, __FILE__, __LINE__); ID3D11Texture1D_Release(t); }
QI1D(unknown, IID_IUnknown, S_OK)
QI1D(resource, IID_ID3D11Resource, S_OK)
QI1D(self, IID_ID3D11Texture1D, S_OK)
QI1D(dxgiresource, IID_IDXGIResource, S_OK)
QI1D(dxgisurface, IID_IDXGISurface, S_OK)
QI1D(tex2d, IID_ID3D11Texture2D, E_NOINTERFACE)
QI1D(tex3d, IID_ID3D11Texture3D, E_NOINTERFACE)

#define QI3D(nm, IID, WANT) DEVTEST(d3d11_textures_qi_3d_##nm) { ID3D11Texture3D *t = NULL; \
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return; \
    tx_qi_expect((IUnknown *)t, &IID, WANT, __FILE__, __LINE__); ID3D11Texture3D_Release(t); }
QI3D(unknown, IID_IUnknown, S_OK)
QI3D(resource, IID_ID3D11Resource, S_OK)
QI3D(self, IID_ID3D11Texture3D, S_OK)
QI3D(dxgiresource, IID_IDXGIResource, S_OK)
QI3D(dxgisurface, IID_IDXGISurface, E_NOINTERFACE)
QI3D(tex2d, IID_ID3D11Texture2D, E_NOINTERFACE)
QI3D(tex1d, IID_ID3D11Texture1D, E_NOINTERFACE)

/* IDXGISurface is offered only for single-subresource 2D textures. */
DEVTEST(d3d11_textures_qi_surface_mip_chain_refused) {
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(tx_mk2d(c, 16, 16, 0, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    tx_qi_expect((IUnknown *)t, &IID_IDXGISurface, E_NOINTERFACE, __FILE__, __LINE__);
    tx_qi_expect((IUnknown *)t, &IID_IDXGIResource, S_OK, __FILE__, __LINE__);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_qi_surface_array_refused) {
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 2, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    tx_qi_expect((IUnknown *)t, &IID_IDXGISurface, E_NOINTERFACE, __FILE__, __LINE__);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_qi_surface_two_mips_refused) {
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(tx_mk2d(c, 16, 16, 2, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    tx_qi_expect((IUnknown *)t, &IID_IDXGISurface, E_NOINTERFACE, __FILE__, __LINE__);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_qi_surface_single_mip_with_chain_request_one) {
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(tx_mk2d(c, 1, 1, 0, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    tx_qi_expect((IUnknown *)t, &IID_IDXGIResource, S_OK, __FILE__, __LINE__);
    t_note("1x1 with MipLevels=0 (one level) IDXGISurface -> %s", d3dh_hr_name(({ void *p = NULL; HRESULT h = IUnknown_QueryInterface((IUnknown *)t, &IID_IDXGISurface, &p); if (p) IUnknown_Release((IUnknown *)p); h; })));
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_qi_d3d10_texture_note) {
    ID3D11Texture2D *t = NULL; void *p = NULL; HRESULT hr;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    hr = IUnknown_QueryInterface((IUnknown *)t, &IID_ID3D10Texture2D, &p);
    t_note("ID3D11Texture2D -> ID3D10Texture2D: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        D3D10_TEXTURE2D_DESC d10;
        ID3D10Texture2D_GetDesc((ID3D10Texture2D *)p, &d10);
        EXPECT_EQ(d10.Width, 16); EXPECT_EQ(d10.Height, 16); EXPECT_EQ(d10.Format, F_RGBA8); EXPECT_EQ(d10.BindFlags, D3D10_BIND_RENDER_TARGET);
        IUnknown_Release((IUnknown *)p);
    }
    ID3D11Texture2D_Release(t);
}

/* ---- DXGI view of the same object ---------------------------------------------------------------------------- */
DEVTEST(d3d11_textures_dxgi_identity) {
    ID3D11Texture2D *t = NULL; IDXGIResource *res = NULL; IDXGISurface *sur = NULL; IDXGIObject *obj = NULL; IDXGIDeviceSubObject *sub = NULL; IDXGIResource *res2 = NULL;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&res), S_OK);
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&sur), S_OK);
    if (res && sur) {
        EXPECT_HR(IDXGISurface_QueryInterface(sur, &IID_IDXGIResource, (void **)&res2), S_OK);
        EXPECT_EQ(res2, res); SAFE_RELEASE(res2);
        EXPECT_HR(IDXGISurface_QueryInterface(sur, &IID_IDXGIObject, (void **)&obj), S_OK);
        EXPECT_EQ(obj, (IDXGIObject *)res); SAFE_RELEASE(obj);
        EXPECT_HR(IDXGISurface_QueryInterface(sur, &IID_IDXGIDeviceSubObject, (void **)&sub), S_OK);
        EXPECT_EQ(sub, (IDXGIDeviceSubObject *)res); SAFE_RELEASE(sub);
        EXPECT_EQ(sur, (IDXGISurface *)res);
    }
    SAFE_RELEASE(res); SAFE_RELEASE(sur);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_dxgi_getparent_refuses_siblings) {
    ID3D11Texture2D *t = NULL; IDXGIResource *res = NULL; void *p = NULL;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&res), S_OK);
    if (res) {
        EXPECT_HR(IDXGIResource_GetParent(res, &IID_IDXGISurface, &p), E_NOINTERFACE);
        EXPECT_HR(IDXGIResource_GetParent(res, &IID_IDXGIResource, &p), E_NOINTERFACE);
    }
    SAFE_RELEASE(res);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_dxgi_surface_getdesc) {
    ID3D11Texture2D *t = NULL; IDXGISurface *sur = NULL; DXGI_SURFACE_DESC d;
    EXPECT_HR(tx_mk2d(c, 24, 12, 1, 1, F_BGRA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&sur), S_OK);
    if (sur) {
        memset(&d, 0xCC, sizeof d);
        EXPECT_HR(IDXGISurface_GetDesc(sur, &d), S_OK);
        EXPECT_EQ(d.Width, 24); EXPECT_EQ(d.Height, 12); EXPECT_EQ(d.Format, F_BGRA8);
        EXPECT_EQ(d.SampleDesc.Count, 1); EXPECT_EQ(d.SampleDesc.Quality, 0);
        IDXGISurface_Release(sur);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_dxgi_surface_getdesc_typeless) {
    ID3D11Texture2D *t = NULL; IDXGISurface *sur = NULL; DXGI_SURFACE_DESC d;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, DXGI_FORMAT_R32_TYPELESS, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&sur), S_OK);
    if (sur) { EXPECT_HR(IDXGISurface_GetDesc(sur, &d), S_OK); EXPECT_EQ(d.Format, DXGI_FORMAT_R32_TYPELESS); IDXGISurface_Release(sur); }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_dxgi_subobject_getdevice) {
    ID3D11Texture2D *t = NULL; IDXGIDeviceSubObject *sub = NULL; void *p = NULL; ULONG before, after;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIDeviceSubObject, (void **)&sub), S_OK);
    if (sub) {
        before = d3dh_refcount(c->dev);
        EXPECT_HR(IDXGIDeviceSubObject_GetDevice(sub, &IID_ID3D11Device, &p), S_OK);
        EXPECT_EQ(p, c->dev);
        after = d3dh_refcount(c->dev);
        EXPECT_EQ(after, before + 1);
        if (p) IUnknown_Release((IUnknown *)p);
        p = NULL;
        EXPECT_HR(IDXGIDeviceSubObject_GetDevice(sub, &IID_IDXGIDevice, &p), S_OK);
        if (p) IUnknown_Release((IUnknown *)p);
        IDXGIDeviceSubObject_Release(sub);
    }
    ID3D11Texture2D_Release(t);
}
#define USAGE_CASE(nm, FMT, BIND, EXPECT_USAGE) DEVTEST(d3d11_textures_dxgi_usage_##nm) { \
    ID3D11Texture2D *t = NULL; IDXGIResource *res = NULL; DXGI_USAGE u = 0xCCCCCCCC; \
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, FMT, U_DEF, BIND, 0, 0, NULL, &t), S_OK); if (!t) return; \
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&res), S_OK); \
    if (res) { EXPECT_HR(IDXGIResource_GetUsage(res, &u), S_OK); EXPECT_EQ(u, EXPECT_USAGE); IDXGIResource_Release(res); } \
    ID3D11Texture2D_Release(t); }
USAGE_CASE(none, F_RGBA8, 0, 0)
USAGE_CASE(srv, F_RGBA8, B_SRV, DXGI_USAGE_SHADER_INPUT)
USAGE_CASE(rt, F_RGBA8, B_RT, DXGI_USAGE_RENDER_TARGET_OUTPUT)
USAGE_CASE(srv_rt, F_RGBA8, B_SRV | B_RT, DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT)
USAGE_CASE(ds, DXGI_FORMAT_D24_UNORM_S8_UINT, B_DS, 0)
USAGE_CASE(uav, DXGI_FORMAT_R32_UINT, B_UAV, DXGI_USAGE_UNORDERED_ACCESS)
USAGE_CASE(srv_uav, DXGI_FORMAT_R32_UINT, B_SRV | B_UAV, DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_UNORDERED_ACCESS)

DEVTEST(d3d11_textures_dxgi_eviction_priority_roundtrip) {
    static const UINT prio[] = { DXGI_RESOURCE_PRIORITY_MINIMUM, DXGI_RESOURCE_PRIORITY_LOW, DXGI_RESOURCE_PRIORITY_NORMAL, DXGI_RESOURCE_PRIORITY_HIGH, DXGI_RESOURCE_PRIORITY_MAXIMUM };
    ID3D11Texture2D *t = NULL; IDXGIResource *res = NULL;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&res), S_OK);
    if (res) {
        UINT p = 0; HRESULT hr = IDXGIResource_GetEvictionPriority(res, &p);
        EXPECT_HR(hr, S_OK);
        t_note("default DXGI eviction priority %#x", p);
        for (UINT i = 0; i < 5; i++) {
            EXPECT_HR(IDXGIResource_SetEvictionPriority(res, prio[i]), S_OK);
            p = 0; EXPECT_HR(IDXGIResource_GetEvictionPriority(res, &p), S_OK);
            CHECKF(p == prio[i], "set %#x got %#x", prio[i], p);
        }
        IDXGIResource_Release(res);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_d3d11_eviction_priority) {
    ID3D11Texture2D *t = NULL; UINT p;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    p = ID3D11Texture2D_GetEvictionPriority(t);
    t_note("default D3D11 eviction priority %#x", p);
    ID3D11Texture2D_SetEvictionPriority(t, DXGI_RESOURCE_PRIORITY_HIGH);
    p = ID3D11Texture2D_GetEvictionPriority(t);
    CHECKF(p == DXGI_RESOURCE_PRIORITY_HIGH, "got %#x", p);
    ID3D11Texture2D_SetEvictionPriority(t, DXGI_RESOURCE_PRIORITY_MINIMUM);
    p = ID3D11Texture2D_GetEvictionPriority(t);
    CHECKF(p == DXGI_RESOURCE_PRIORITY_MINIMUM, "got %#x", p);
    ID3D11Texture2D_Release(t);
}

/* ---- private data ---------------------------------------------------------------------------------------------- */
static const GUID tx_guid_a = {0x6f1c1a50, 0x11a2, 0x4d3f, {0x9a, 0x01, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}};
static const GUID tx_guid_b = {0x6f1c1a51, 0x11a2, 0x4d3f, {0x9a, 0x01, 0x22, 0x33, 0x44, 0x55, 0x66, 0x78}};
DEVTEST(d3d11_textures_privdata_roundtrip) {
    ID3D11Texture2D *t = NULL; static const DWORD in[4] = {1, 2, 3, 4}; DWORD out[4] = {0}; UINT sz = sizeof out;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, sizeof in, in), S_OK);
    EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_a, &sz, out), S_OK);
    EXPECT_EQ(sz, sizeof in);
    CHECK(!memcmp(in, out, sizeof in));
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_privdata_size_query) {
    ID3D11Texture2D *t = NULL; static const DWORD in[4] = {1, 2, 3, 4}; UINT sz = 0;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, sizeof in, in), S_OK);
    EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_a, &sz, NULL), S_OK);
    EXPECT_EQ(sz, sizeof in);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_privdata_buffer_too_small) {
    ID3D11Texture2D *t = NULL; static const DWORD in[4] = {1, 2, 3, 4}; DWORD out[4]; UINT sz = 8;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    memset(out, 0xCC, sizeof out);
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, sizeof in, in), S_OK);
    EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_a, &sz, out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, sizeof in);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_privdata_not_found) {
    ID3D11Texture2D *t = NULL; DWORD out = 0xCCCCCCCC; UINT sz = sizeof out;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_b, &sz, &out), DXGI_ERROR_NOT_FOUND);
    t_note("size after NOT_FOUND: %u", sz);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_privdata_replace_and_two_keys) {
    ID3D11Texture2D *t = NULL; DWORD a1 = 11, a2 = 22, b = 33, out = 0; UINT sz;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, 4, &a1), S_OK);
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_b, 4, &b), S_OK);
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, 4, &a2), S_OK);
    sz = 4; EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_a, &sz, &out), S_OK); EXPECT_EQ(out, 22);
    sz = 4; EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_b, &sz, &out), S_OK); EXPECT_EQ(out, 33);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_privdata_replace_changes_size) {
    ID3D11Texture2D *t = NULL; char big[40]; DWORD small = 7; UINT sz;
    memset(big, 0x5A, sizeof big);
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, sizeof big, big), S_OK);
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, sizeof small, &small), S_OK);
    sz = 0; EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_a, &sz, NULL), S_OK); EXPECT_EQ(sz, sizeof small);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_privdata_clear_with_null) {
    ID3D11Texture2D *t = NULL; DWORD v = 5, out = 0; UINT sz = 4;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, 4, &v), S_OK);
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, 0, NULL), S_OK);
    EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_a, &sz, &out), DXGI_ERROR_NOT_FOUND);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_privdata_interface_refcount) {
    ID3D11Texture2D *t = NULL, *holder = NULL; ULONG base, r; IUnknown *got = NULL; UINT sz = sizeof got;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &holder), S_OK);
    if (!t || !holder) { SAFE_RELEASE(t); SAFE_RELEASE(holder); return; }
    base = d3dh_refcount(holder);
    EXPECT_HR(ID3D11Texture2D_SetPrivateDataInterface(t, &tx_guid_a, (IUnknown *)holder), S_OK);
    r = d3dh_refcount(holder); EXPECT_EQ(r, base + 1);
    EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_a, &sz, &got), S_OK);
    EXPECT_EQ(sz, sizeof(IUnknown *)); EXPECT_EQ(got, (IUnknown *)holder);
    r = d3dh_refcount(holder); EXPECT_EQ(r, base + 2);
    if (got) IUnknown_Release(got);
    /* replacing with plain data drops the reference */
    { DWORD v = 1; EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &tx_guid_a, 4, &v), S_OK); }
    r = d3dh_refcount(holder); EXPECT_EQ(r, base);
    /* destroying the texture drops the reference of an interface stored on it */
    EXPECT_HR(ID3D11Texture2D_SetPrivateDataInterface(t, &tx_guid_b, (IUnknown *)holder), S_OK);
    r = d3dh_refcount(holder); EXPECT_EQ(r, base + 1);
    ID3D11Texture2D_Release(t);
    r = d3dh_refcount(holder); EXPECT_EQ(r, base);
    ID3D11Texture2D_Release(holder);
}
DEVTEST(d3d11_textures_privdata_shared_with_dxgi_view) {
    ID3D11Texture2D *t = NULL; IDXGIResource *res = NULL; DWORD v = 123, out = 0; UINT sz = 4;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&res), S_OK);
    if (res) {
        EXPECT_HR(IDXGIResource_SetPrivateData(res, &tx_guid_a, 4, &v), S_OK);
        EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &tx_guid_a, &sz, &out), S_OK);
        EXPECT_EQ(out, 123);
        IDXGIResource_Release(res);
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_privdata_1d_3d) {
    ID3D11Texture1D *t1 = NULL; ID3D11Texture3D *t3 = NULL; DWORD v = 9, out = 0; UINT sz = 4;
    EXPECT_HR(tx_mk1d(c, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t1), S_OK);
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t3), S_OK);
    if (t1) { EXPECT_HR(ID3D11Texture1D_SetPrivateData(t1, &tx_guid_a, 4, &v), S_OK); out = 0; sz = 4; EXPECT_HR(ID3D11Texture1D_GetPrivateData(t1, &tx_guid_a, &sz, &out), S_OK); EXPECT_EQ(out, 9); ID3D11Texture1D_Release(t1); }
    if (t3) { EXPECT_HR(ID3D11Texture3D_SetPrivateData(t3, &tx_guid_a, 4, &v), S_OK); out = 0; sz = 4; EXPECT_HR(ID3D11Texture3D_GetPrivateData(t3, &tx_guid_a, &sz, &out), S_OK); EXPECT_EQ(out, 9); ID3D11Texture3D_Release(t3); }
}

/* ---- multisample quality / format support queries ------------------------------------------------------------ */
DEVTEST(d3d11_textures_msq_count1_has_one_level) {
    UINT q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 1, &q), S_OK);
    EXPECT_EQ(q, 1);
}
DEVTEST(d3d11_textures_msq_count0_fails) {
    UINT q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 0, &q), E_FAIL);
    EXPECT_EQ(q, 0);
}
DEVTEST(d3d11_textures_msq_count33_64_fail) {
    UINT q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 33, &q), E_FAIL); EXPECT_EQ(q, 0);
    q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 64, &q), E_FAIL); EXPECT_EQ(q, 0);
}
DEVTEST(d3d11_textures_msq_count32_ok_zero) {
    UINT q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 32, &q), S_OK);
    t_note("RGBA8 32x quality levels: %u", q);
}
DEVTEST(d3d11_textures_msq_unsupported_counts_report_zero) {
    static const UINT counts[] = {3, 5, 6, 7, 9, 15};
    for (UINT i = 0; i < sizeof counts / sizeof *counts; i++) {
        UINT q = 0xDEADBEEF;
        EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, counts[i], &q), S_OK);
        CHECKF(q == 0, "count %u reports %u levels", counts[i], q);
    }
}
DEVTEST(d3d11_textures_msq_compressed_zero) {
    UINT q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, DXGI_FORMAT_BC3_UNORM, 2, &q), S_OK);
    EXPECT_EQ(q, 0);
    q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, DXGI_FORMAT_BC1_UNORM, 4, &q), S_OK);
    EXPECT_EQ(q, 0);
}
DEVTEST(d3d11_textures_msq_unknown_format_zero) {
    UINT q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, DXGI_FORMAT_UNKNOWN, 2, &q), S_OK);
    EXPECT_EQ(q, 0);
}
DEVTEST(d3d11_textures_msq_invalid_format) {
    UINT q = 0xDEADBEEF;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, (DXGI_FORMAT)65536, 2, &q), E_INVALIDARG);
}
DEVTEST(d3d11_textures_msq_null_out) {
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 4, NULL), E_INVALIDARG);
}
DEVTEST(d3d11_textures_msq_4x_rgba8_supported_at_fl10_1) {
    UINT q = 0;
    EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 4, &q), S_OK);
    if (c->fl >= FL_10_1) CHECKF(q >= 1, "4x RGBA8 reports %u levels", q);
    else t_note("4x RGBA8 levels %u at fl %#x", q, c->fl);
}

/* Creating a multisampled texture succeeds exactly for the (count, quality) pairs advertised by the query. */
static void tx_msq_consistency(tx_ctx *c, DXGI_FORMAT f, UINT bind, const char *file, int line) {
    for (UINT n = 1; n <= 32; n++) {
        UINT q = 0xDEADBEEF; HRESULT hr = ID3D11Device_CheckMultisampleQualityLevels(c->dev, f, n, &q);
        D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *t = NULL; HRESULT h2; UINT qq;
        if (FAILED(hr)) { t_check(0, file, line, "CheckMultisampleQualityLevels", "format %u count %u: %s", f, n, d3dh_hr_name(hr)); continue; }
        memset(&d, 0, sizeof d);
        d.Width = d.Height = 16; d.MipLevels = d.ArraySize = 1; d.Format = f; d.SampleDesc.Count = n; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
        for (qq = 0; qq < (q < 3 ? q : 3); qq++) {
            d.SampleDesc.Quality = qq; t = NULL;
            h2 = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &t);
            t_check(h2 == S_OK, file, line, "create at advertised quality", "format %u count %u quality %u/%u: %s", f, n, qq, q, d3dh_hr_name(h2));
            SAFE_RELEASE(t);
        }
        d.SampleDesc.Quality = q; t = NULL;
        h2 = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &t);
        t_check(h2 == E_INVALIDARG, file, line, "create at quality == levels", "format %u count %u levels %u: %s", f, n, q, d3dh_hr_name(h2));
        SAFE_RELEASE(t);
    }
}
#define MSQ_CONS(nm, FMT, BIND) DEVTEST(d3d11_textures_msq_consistent_##nm) { tx_msq_consistency(c, FMT, BIND, __FILE__, __LINE__); }
MSQ_CONS(rgba8, F_RGBA8, B_RT)
MSQ_CONS(bgra8, F_BGRA8, B_RT)
MSQ_CONS(rgba8_srgb, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, B_RT)
MSQ_CONS(r16g16b16a16f, DXGI_FORMAT_R16G16B16A16_FLOAT, B_RT)
MSQ_CONS(r32f, DXGI_FORMAT_R32_FLOAT, B_RT)
MSQ_CONS(r32g32b32a32f, DXGI_FORMAT_R32G32B32A32_FLOAT, B_RT)
MSQ_CONS(r10g10b10a2, DXGI_FORMAT_R10G10B10A2_UNORM, B_RT)
MSQ_CONS(r8, F_R8, B_RT)
MSQ_CONS(d24s8, DXGI_FORMAT_D24_UNORM_S8_UINT, B_DS)
MSQ_CONS(d32f, DXGI_FORMAT_D32_FLOAT, B_DS)
MSQ_CONS(d16, DXGI_FORMAT_D16_UNORM, B_DS)
MSQ_CONS(d32fs8, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, B_DS)

#define CFS(nm, FMT, MUST, MUSTNOT) DEVTEST(d3d11_textures_formatsupport_##nm) { UINT s = 0xDEADBEEF; HRESULT hr = ID3D11Device_CheckFormatSupport(c->dev, FMT, &s); \
    EXPECT_HR(hr, S_OK); if (FAILED(hr)) return; \
    CHECKF((s & (MUST)) == (MUST), "support %#x lacks %#x", s, (MUST) & ~s); CHECKF(!(s & (MUSTNOT)), "support %#x has forbidden %#x", s, s & (MUSTNOT)); }
CFS(rgba8, F_RGBA8, D3D11_FORMAT_SUPPORT_TEXTURE1D | D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_TEXTURE3D | D3D11_FORMAT_SUPPORT_TEXTURECUBE | D3D11_FORMAT_SUPPORT_MIP |
    D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_BLENDABLE | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE | D3D11_FORMAT_SUPPORT_MIP_AUTOGEN, D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
CFS(bgra8, F_BGRA8, D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE, D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
CFS(r32f, DXGI_FORMAT_R32_FLOAT, D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_LOAD, D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
CFS(r16g16b16a16f, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE | D3D11_FORMAT_SUPPORT_BLENDABLE, D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
CFS(d32f, DXGI_FORMAT_D32_FLOAT, D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, D3D11_FORMAT_SUPPORT_RENDER_TARGET)
CFS(d24s8, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, D3D11_FORMAT_SUPPORT_RENDER_TARGET)
CFS(d16, DXGI_FORMAT_D16_UNORM, D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, D3D11_FORMAT_SUPPORT_RENDER_TARGET)
CFS(bc1, DXGI_FORMAT_BC1_UNORM, D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE | D3D11_FORMAT_SUPPORT_MIP, D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
CFS(bc3, DXGI_FORMAT_BC3_UNORM, D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE, D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)

/* Render-target / depth-stencil creation agrees with CheckFormatSupport for typed formats. */
DEVTEST(d3d11_textures_formatsupport_matches_create_rt_ds) {
    static const DXGI_FORMAT fmts[] = {
        DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_UNORM,
        DXGI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R16_FLOAT,
        DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_R9G9B9E5_SHAREDEXP, DXGI_FORMAT_BC1_UNORM,
        DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, DXGI_FORMAT_R8G8_B8G8_UNORM };
    for (UINT i = 0; i < sizeof fmts / sizeof *fmts; i++) {
        UINT s = 0; ID3D11Texture2D *t = NULL; HRESULT hr, h2;
        hr = ID3D11Device_CheckFormatSupport(c->dev, fmts[i], &s);
        if (FAILED(hr)) { t_note("CheckFormatSupport(%u) -> %s", fmts[i], d3dh_hr_name(hr)); continue; }
        h2 = tx_mk2d(c, 16, 16, 1, 1, fmts[i], U_DEF, B_RT, 0, 0, NULL, &t);
        CHECKF(FAILED(h2) || (s & D3D11_FORMAT_SUPPORT_RENDER_TARGET), "format %u RT created but support %#x lacks RENDER_TARGET", fmts[i], s);
        if (FAILED(h2) && (s & D3D11_FORMAT_SUPPORT_RENDER_TARGET)) t_note("format %u advertises RENDER_TARGET but creation gave %s", fmts[i], d3dh_hr_name(h2));
        SAFE_RELEASE(t);
        h2 = tx_mk2d(c, 16, 16, 1, 1, fmts[i], U_DEF, B_DS, 0, 0, NULL, &t);
        CHECKF(FAILED(h2) || (s & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL), "format %u DS created but support %#x lacks DEPTH_STENCIL", fmts[i], s);
        if (FAILED(h2) && (s & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)) t_note("format %u advertises DEPTH_STENCIL but creation gave %s", fmts[i], d3dh_hr_name(h2));
        SAFE_RELEASE(t);
    }
}
