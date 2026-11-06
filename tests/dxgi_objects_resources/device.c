/* IDXGIDevice / 1 / 2 / 3 / 4 and the GetParent / GetDevice chain. */
#include "objs.h"

/* ---- interface family ---------------------------------------------------------------- */
TEST(objs_dev_qi_dxgidevice)
{
    NEED_DEV(c);
    IDXGIDevice *d = NULL; ID3D11Device *back = NULL;
    EXPECT_HR_OK(ID3D11Device_QueryInterface(c.dev, &IID_IDXGIDevice, (void **)&d));
    CHECK(d != NULL);
    EXPECT_HR_OK(IDXGIDevice_QueryInterface(d, &IID_ID3D11Device, (void **)&back));
    EXPECT_EQ(back, c.dev);
    ID3D11Device_Release(back);
    IDXGIDevice_Release(d);
    objs_close(&c);
}
TEST(objs_dev_qi_dxgidevice1)
{
    NEED_DEV(c);
    IDXGIDevice1 *d1 = NULL; IDXGIDevice *d0 = NULL;
    EXPECT_HR_OK(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice1, (void **)&d1));
    if (d1) {
        EXPECT_HR_OK(IDXGIDevice1_QueryInterface(d1, &IID_IDXGIDevice, (void **)&d0));
        EXPECT_EQ(d0, c.dx);
        SAFE_RELEASE(d0);
        IDXGIDevice1_Release(d1);
    }
    objs_close(&c);
}
TEST(objs_dev_qi_dxgidevice2)
{
    NEED_DEV(c);
    IDXGIDevice2 *d2 = NULL;
    HRESULT hr = IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice2, (void **)&d2);
    EXPECT_HR_OK(hr);
    SAFE_RELEASE(d2);
    objs_close(&c);
}
TEST(objs_dev_qi_dxgidevice3)
{
    NEED_DEV(c);
    IDXGIDevice3 *d3 = NULL;
    HRESULT hr = IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice3, (void **)&d3);
    EXPECT_HR_OK(hr);
    SAFE_RELEASE(d3);
    objs_close(&c);
}
TEST(objs_dev_qi_dxgidevice4)
{
    NEED_DEV(c);
    IDXGIDevice4 *d4 = NULL;
    HRESULT hr = IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice4, (void **)&d4);
    t_note("IDXGIDevice4 via QI: %s (Windows 10 1607+ expected S_OK)", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    SAFE_RELEASE(d4);
    objs_close(&c);
}
TEST(objs_dev_qi_family_same_object)
{
    NEED_DEV(c);
    IDXGIDevice1 *d1 = NULL; IDXGIDevice2 *d2 = NULL; IDXGIDevice3 *d3 = NULL;
    IUnknown *u0 = NULL, *u1 = NULL, *u2 = NULL, *u3 = NULL, *ud = NULL;
    EXPECT_HR_OK(IDXGIDevice_QueryInterface(c.dx, &IID_IUnknown, (void **)&u0));
    if (SUCCEEDED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice1, (void **)&d1))) {
        EXPECT_HR_OK(IDXGIDevice1_QueryInterface(d1, &IID_IUnknown, (void **)&u1));
        EXPECT_EQ(u1, u0);
    }
    if (SUCCEEDED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice2, (void **)&d2))) {
        EXPECT_HR_OK(IDXGIDevice2_QueryInterface(d2, &IID_IUnknown, (void **)&u2));
        EXPECT_EQ(u2, u0);
    }
    if (SUCCEEDED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice3, (void **)&d3))) {
        EXPECT_HR_OK(IDXGIDevice3_QueryInterface(d3, &IID_IUnknown, (void **)&u3));
        EXPECT_EQ(u3, u0);
    }
    if (SUCCEEDED(ID3D11Device_QueryInterface(c.dev, &IID_IUnknown, (void **)&ud)))
        t_note("IUnknown identity of ID3D11Device vs IDXGIDevice: %s", ud == u0 ? "same" : "different");
    SAFE_RELEASE(u0); SAFE_RELEASE(u1); SAFE_RELEASE(u2); SAFE_RELEASE(u3); SAFE_RELEASE(ud);
    SAFE_RELEASE(d1); SAFE_RELEASE(d2); SAFE_RELEASE(d3);
    objs_close(&c);
}
TEST(objs_dev_qi_unsupported)
{
    NEED_DEV(c);
    IUnknown *p = (IUnknown *)(ULONG_PTR)0x1;
    HRESULT hr = IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIFactory, (void **)&p);
    EXPECT_HR(hr, E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    p = (IUnknown *)(ULONG_PTR)0x1;
    hr = IDXGIDevice_QueryInterface(c.dx, &IID_IDXGISwapChain, (void **)&p);
    EXPECT_HR(hr, E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    objs_close(&c);
}
TEST(objs_dev_refcount_balanced)
{
    NEED_DEV(c);
    ULONG before, after; IDXGIDevice1 *d1 = NULL; IDXGIAdapter *a = NULL;
    IDXGIDevice_AddRef(c.dx); before = IDXGIDevice_Release(c.dx);
    if (SUCCEEDED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice1, (void **)&d1))) {
        IDXGIDevice1_Release(d1);
    }
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    IDXGIAdapter_Release(a);
    IDXGIDevice_AddRef(c.dx); after = IDXGIDevice_Release(c.dx);
    EXPECT_EQ(after, before);
    objs_close(&c);
}

/* ---- GetAdapter ---------------------------------------------------------------------------- */
TEST(objs_dev_getadapter_basic)
{
    NEED_DEV(c);
    IDXGIAdapter *a = (IDXGIAdapter *)(ULONG_PTR)1; DXGI_ADAPTER_DESC d;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    CHECK(a != NULL && a != (IDXGIAdapter *)(ULONG_PTR)1);
    if (a) {
        memset(&d, 0xcc, sizeof(d));
        EXPECT_HR_OK(IDXGIAdapter_GetDesc(a, &d));
        CHECK(d.Description[0] != 0xcccc);
        IDXGIAdapter_Release(a);
    }
    objs_close(&c);
}
TEST(objs_dev_getadapter_same_pointer)
{
    NEED_DEV(c);
    IDXGIAdapter *a1 = NULL, *a2 = NULL;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a1));
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a2));
    EXPECT_EQ(a1, a2);
    SAFE_RELEASE(a1); SAFE_RELEASE(a2);
    objs_close(&c);
}
TEST(objs_dev_getadapter_adds_ref)
{
    NEED_DEV(c);
    IDXGIAdapter *a1 = NULL, *a2 = NULL; ULONG r1, r2;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a1));
    IDXGIAdapter_AddRef(a1); r1 = IDXGIAdapter_Release(a1);
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a2));
    IDXGIAdapter_AddRef(a1); r2 = IDXGIAdapter_Release(a1);
    EXPECT_EQ(r2, r1 + 1);
    IDXGIAdapter_Release(a2); IDXGIAdapter_Release(a1);
    objs_close(&c);
}
TEST(objs_dev_getadapter_null)
{
    NEED_DEV(c);
    HRESULT hr = IDXGIDevice_GetAdapter(c.dx, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    t_note("GetAdapter(NULL) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
    objs_close(&c);
}
TEST(objs_dev_getadapter_via_device1)
{
    NEED_DEV(c);
    IDXGIDevice1 *d1 = NULL; IDXGIAdapter *a = NULL, *b = NULL;
    if (FAILED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice1, (void **)&d1))) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIDevice1_GetAdapter(d1, &a));
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &b));
    EXPECT_EQ(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b); IDXGIDevice1_Release(d1);
    objs_close(&c);
}
TEST(objs_dev_adapter_is_warp_identity)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; DXGI_ADAPTER_DESC d;
    if (c.drv != D3D_DRIVER_TYPE_WARP) { t_skip("device is not WARP"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    EXPECT_HR_OK(IDXGIAdapter_GetDesc(a, &d));
    EXPECT_EQ(d.VendorId, 0x1414);
    EXPECT_EQ(d.DeviceId, 0x8c);
    EXPECT_WSTR(d.Description, L"Microsoft Basic Render Driver");
    EXPECT_EQ(d.DedicatedVideoMemory, 0);
    IDXGIAdapter_Release(a);
    objs_close(&c);
}

/* ---- GetParent chain -------------------------------------------------------------------------- */
TEST(objs_parent_device_to_adapter)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IUnknown *p = NULL;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    EXPECT_HR_OK(IDXGIDevice_GetParent(c.dx, &IID_IDXGIAdapter, (void **)&p));
    EXPECT_EQ(p, (IUnknown *)a);
    SAFE_RELEASE(p); SAFE_RELEASE(a);
    objs_close(&c);
}
TEST(objs_parent_device_to_adapter1)
{
    NEED_DEV(c);
    IUnknown *p = NULL; HRESULT hr = IDXGIDevice_GetParent(c.dx, &IID_IDXGIAdapter1, (void **)&p);
    t_note("device->GetParent(IDXGIAdapter1) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) { CHECK(p != NULL); IUnknown_Release(p); }
    else CHECK(p == NULL);
    objs_close(&c);
}
TEST(objs_parent_device_wrong_iid)
{
    NEED_DEV(c);
    IUnknown *p = (IUnknown *)(ULONG_PTR)1;
    HRESULT hr = IDXGIDevice_GetParent(c.dx, &IID_IDXGISwapChain, (void **)&p);
    EXPECT_HR(hr, E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    objs_close(&c);
}
TEST(objs_parent_device_iid_factory)
{
    NEED_DEV(c);
    IUnknown *p = (IUnknown *)(ULONG_PTR)1;
    HRESULT hr = IDXGIDevice_GetParent(c.dx, &IID_IDXGIFactory, (void **)&p);
    t_note("device->GetParent(IDXGIFactory) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_NOINTERFACE);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) IUnknown_Release(p);
    objs_close(&c);
}
TEST(objs_parent_device_same_each_time)
{
    NEED_DEV(c);
    IUnknown *p1 = NULL, *p2 = NULL;
    EXPECT_HR_OK(IDXGIDevice_GetParent(c.dx, &IID_IDXGIAdapter, (void **)&p1));
    EXPECT_HR_OK(IDXGIDevice_GetParent(c.dx, &IID_IDXGIAdapter, (void **)&p2));
    EXPECT_EQ(p1, p2);
    SAFE_RELEASE(p1); SAFE_RELEASE(p2);
    objs_close(&c);
}
TEST(objs_parent_adapter_to_factory)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IDXGIFactory *f = NULL, *f2 = NULL;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    EXPECT_HR_OK(IDXGIAdapter_GetParent(a, &IID_IDXGIFactory, (void **)&f));
    CHECK(f != NULL);
    EXPECT_HR_OK(IDXGIAdapter_GetParent(a, &IID_IDXGIFactory, (void **)&f2));
    EXPECT_EQ(f, f2);
    SAFE_RELEASE(f); SAFE_RELEASE(f2); SAFE_RELEASE(a);
    objs_close(&c);
}
TEST(objs_parent_adapter_to_factory1)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IDXGIFactory1 *f = NULL;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    HRESULT hr = IDXGIAdapter_GetParent(a, &IID_IDXGIFactory1, (void **)&f);
    EXPECT_HR_OK(hr);
    SAFE_RELEASE(f); SAFE_RELEASE(a);
    objs_close(&c);
}
TEST(objs_parent_adapter_wrong_iid)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IUnknown *p = (IUnknown *)(ULONG_PTR)1;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    HRESULT hr = IDXGIAdapter_GetParent(a, &IID_IDXGIDevice, (void **)&p);
    EXPECT_HR(hr, E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    SAFE_RELEASE(a);
    objs_close(&c);
}
TEST(objs_parent_factory_has_no_parent)
{
    IDXGIFactory1 *f = NULL; IUnknown *p = (IUnknown *)(ULONG_PTR)1;
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (FAILED(d3dh_make_factory1(&f))) { t_skip("no factory"); return; }
    HRESULT hr = IDXGIFactory1_GetParent(f, &IID_IUnknown, (void **)&p);
    EXPECT_HR(hr, E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    p = (IUnknown *)(ULONG_PTR)1;
    hr = IDXGIFactory1_GetParent(f, &IID_IDXGIObject, (void **)&p);
    EXPECT_HR(hr, E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    IDXGIFactory1_Release(f);
}
TEST(objs_parent_factory_chain_ends)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IDXGIFactory *f = NULL; IUnknown *p = (IUnknown *)(ULONG_PTR)1;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    EXPECT_HR_OK(IDXGIAdapter_GetParent(a, &IID_IDXGIFactory, (void **)&f));
    EXPECT_HR(IDXGIFactory_GetParent(f, &IID_IDXGIAdapter, (void **)&p), E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    SAFE_RELEASE(f); SAFE_RELEASE(a);
    objs_close(&c);
}
TEST(objs_parent_output_to_adapter)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IDXGIOutput *o = NULL; IUnknown *p = NULL;
    {   /* the device's own adapter (software, factory adapter 1 on the reference VM) has no outputs: DXGI_ERROR_NOT_FOUND */
        IDXGIOutput *none = (IDXGIOutput *)(ULONG_PTR)1;
        EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
        EXPECT_HR(IDXGIAdapter_EnumOutputs(a, 0, &none), DXGI_ERROR_NOT_FOUND);
        EXPECT_EQ(none, NULL);
        IDXGIAdapter_Release(a); a = NULL;
    }
    if (!objs_first_output(&a, &o)) { t_skip("no adapter with an output"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIOutput_GetParent(o, &IID_IDXGIAdapter, (void **)&p));
    EXPECT_EQ(p, (IUnknown *)a);
    SAFE_RELEASE(p);
    p = (IUnknown *)(ULONG_PTR)1;
    EXPECT_HR(IDXGIOutput_GetParent(o, &IID_IDXGIDevice, (void **)&p), E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    IDXGIOutput_Release(o); IDXGIAdapter_Release(a);
    objs_close(&c);
}
TEST(objs_parent_full_chain_device_to_factory)
{
    NEED_DEV(c);
    IDXGIObject *o = NULL; IDXGIObject *next = NULL; int depth = 0;
    EXPECT_HR_OK(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIObject, (void **)&o));
    while (o && depth < 5) {
        HRESULT hr = IDXGIObject_GetParent(o, &IID_IDXGIObject, (void **)&next);
        t_note("chain level %d GetParent(IDXGIObject) -> %s", depth, d3dh_hr_name(hr));
        IDXGIObject_Release(o); o = SUCCEEDED(hr) ? next : NULL; next = NULL; depth++;
    }
    CHECK(depth >= 1 && depth <= 4);
    SAFE_RELEASE(o);
    objs_close(&c);
}
TEST(objs_parent_surface_to_device)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL; IUnknown *p = NULL;
    memset(&d, 0, sizeof(d)); d.Width = 64; d.Height = 64; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
    EXPECT_HR_OK(IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s));
    if (s) {
        EXPECT_HR_OK(IDXGISurface_GetParent(s, &IID_IDXGIDevice, (void **)&p));
        EXPECT_EQ(p, (IUnknown *)c.dx);
        SAFE_RELEASE(p);
        p = (IUnknown *)(ULONG_PTR)1;
        EXPECT_HR(IDXGISurface_GetParent(s, &IID_IDXGIFactory, (void **)&p), E_NOINTERFACE);
        EXPECT_EQ(p, NULL);
        IDXGISurface_Release(s);
    }
    objs_close(&c);
}
TEST(objs_parent_texture_to_device)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIObject *o = NULL; IUnknown *p = NULL;
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIObject, (void **)&o));
    EXPECT_HR_OK(IDXGIObject_GetParent(o, &IID_IDXGIDevice, (void **)&p));
    EXPECT_EQ(p, (IUnknown *)c.dx);
    SAFE_RELEASE(p);
    HRESULT hr = IDXGIObject_GetParent(o, &IID_IDXGIAdapter, (void **)&p);
    t_note("texture->GetParent(IDXGIAdapter) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_NOINTERFACE);   /* Win10 22H2 */
    SAFE_RELEASE(p);
    IDXGIObject_Release(o); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_parent_buffer_to_device)
{
    NEED_DEV(c);
    ID3D11Buffer *b = NULL; IDXGIObject *o = NULL; IUnknown *p = NULL;
    if (FAILED(objs_buf(c.dev, 64, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, &b))) { t_skip("no buffer"); objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Buffer_QueryInterface(b, &IID_IDXGIObject, (void **)&o));
    EXPECT_HR_OK(IDXGIObject_GetParent(o, &IID_IDXGIDevice, (void **)&p));
    EXPECT_EQ(p, (IUnknown *)c.dx);
    SAFE_RELEASE(p);
    IDXGIObject_Release(o); ID3D11Buffer_Release(b);
    objs_close(&c);
}
TEST(objs_parent_resource_vs_surface)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; IDXGISurface *s = NULL; IUnknown *p = (IUnknown *)(ULONG_PTR)1;
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    EXPECT_HR_OK(IDXGIResource_QueryInterface(r, &IID_IDXGISurface, (void **)&s));
    /* a resource's parent is its device, never the surface/resource view of itself */
    EXPECT_HR(IDXGIResource_GetParent(r, &IID_IDXGISurface, (void **)&p), E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    p = (IUnknown *)(ULONG_PTR)1;
    EXPECT_HR(IDXGISurface_GetParent(s, &IID_IDXGIResource, (void **)&p), E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    SAFE_RELEASE(s); SAFE_RELEASE(r); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_parent_keeps_device_alive)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IDXGIFactory *f = NULL; DXGI_ADAPTER_DESC d;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    EXPECT_HR_OK(IDXGIAdapter_GetParent(a, &IID_IDXGIFactory, (void **)&f));
    objs_close(&c);
    /* adapter and factory remain valid after the device is gone */
    EXPECT_HR_OK(IDXGIAdapter_GetDesc(a, &d));
    {
        IDXGIAdapter *e = NULL;
        EXPECT_HR_OK(IDXGIFactory_EnumAdapters(f, 0, &e));
        SAFE_RELEASE(e);
    }
    IDXGIFactory_Release(f); IDXGIAdapter_Release(a);
}

/* ---- IDXGIDeviceSubObject::GetDevice ------------------------------------------------------------------ */
TEST(objs_subobject_getdevice_surface)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL; IDXGIDeviceSubObject *so = NULL; IUnknown *p = NULL;
    memset(&d, 0, sizeof(d)); d.Width = 16; d.Height = 16; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
    EXPECT_HR_OK(IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s));
    if (!s) { objs_close(&c); return; }
    EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGIDeviceSubObject, (void **)&so));
    if (so) {
        EXPECT_HR_OK(IDXGIDeviceSubObject_GetDevice(so, &IID_IDXGIDevice, (void **)&p));
        EXPECT_EQ(p, (IUnknown *)c.dx);
        SAFE_RELEASE(p);
        EXPECT_HR_OK(IDXGIDeviceSubObject_GetDevice(so, &IID_ID3D11Device, (void **)&p));
        EXPECT_EQ(p, (IUnknown *)c.dev);
        SAFE_RELEASE(p);
        p = (IUnknown *)(ULONG_PTR)1;
        HRESULT hr = IDXGIDeviceSubObject_GetDevice(so, &IID_IDXGIFactory, (void **)&p);
        EXPECT_HR_FAILED(hr);
        t_note("GetDevice(IDXGIFactory) -> %s", d3dh_hr_name(hr));
        EXPECT_HR(hr, E_NOINTERFACE);   /* Win10 22H2 */
        CHECK(p == NULL);
        IDXGIDeviceSubObject_Release(so);
    }
    IDXGISurface_Release(s);
    objs_close(&c);
}
TEST(objs_subobject_getdevice_texture)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIDeviceSubObject *so = NULL; IUnknown *p = NULL;
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIDeviceSubObject, (void **)&so));
    if (so) {
        EXPECT_HR_OK(IDXGIDeviceSubObject_GetDevice(so, &IID_IDXGIDevice, (void **)&p));
        EXPECT_EQ(p, (IUnknown *)c.dx);
        SAFE_RELEASE(p);
        EXPECT_HR_OK(IDXGIDeviceSubObject_GetDevice(so, &IID_ID3D11Device, (void **)&p));
        EXPECT_EQ(p, (IUnknown *)c.dev);
        SAFE_RELEASE(p);
        IDXGIDeviceSubObject_Release(so);
    }
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_subobject_buffer_has_no_subobject)
{
    NEED_DEV(c);
    ID3D11Buffer *b = NULL; IUnknown *p = (IUnknown *)(ULONG_PTR)1; HRESULT hr;
    if (FAILED(objs_buf(c.dev, 64, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, &b))) { t_skip("no buffer"); objs_close(&c); return; }
    /* Win10: a buffer answers QI for IDXGISurface (see objs_res_buffer_as_surface for what it then does) */
    hr = ID3D11Buffer_QueryInterface(b, &IID_IDXGISurface, (void **)&p);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(p);
    p = (IUnknown *)(ULONG_PTR)1;
    hr = ID3D11Buffer_QueryInterface(b, &IID_IDXGIResource, (void **)&p);
    EXPECT_HR_OK(hr);
    SAFE_RELEASE(p);
    ID3D11Buffer_Release(b);
    objs_close(&c);
}

/* ---- maximum frame latency -------------------------------------------------------------------------------- */
static IDXGIDevice1 *get_dev1(objs_ctx *c)
{
    IDXGIDevice1 *d1 = NULL;
    if (FAILED(IDXGIDevice_QueryInterface(c->dx, &IID_IDXGIDevice1, (void **)&d1))) return NULL;
    return d1;
}
TEST(objs_dev_latency_default)
{
    NEED_DEV(c);
    IDXGIDevice1 *d = get_dev1(&c); UINT l = 0xdead;
    if (!d) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(d, &l));
    EXPECT_EQ(l, 3);
    IDXGIDevice1_Release(d); objs_close(&c);
}
TEST(objs_dev_latency_null_out)
{
    NEED_DEV(c);
    IDXGIDevice1 *d = get_dev1(&c);
    if (!d) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    EXPECT_HR(IDXGIDevice1_GetMaximumFrameLatency(d, NULL), DXGI_ERROR_INVALID_CALL);
    IDXGIDevice1_Release(d); objs_close(&c);
}
TEST(objs_dev_latency_range_roundtrip)
{
    NEED_DEV(c);
    IDXGIDevice1 *d = get_dev1(&c); UINT v;
    if (!d) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    for (v = 1; v <= 16; v++) {
        UINT l = 0xdead; HRESULT hr = IDXGIDevice1_SetMaximumFrameLatency(d, v);
        CHECKF(hr == S_OK, "set %u: %s", v, d3dh_hr_name(hr));
        hr = IDXGIDevice1_GetMaximumFrameLatency(d, &l);
        CHECKF(hr == S_OK && l == v, "get after set %u: %s %u", v, d3dh_hr_name(hr), l);
    }
    IDXGIDevice1_Release(d); objs_close(&c);
}
TEST(objs_dev_latency_too_large)
{
    NEED_DEV(c);
    IDXGIDevice1 *d = get_dev1(&c); static const UINT bad[] = { 17, 18, 100, 0x10000, 0xffffffffu }; int i;
    if (!d) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIDevice1_SetMaximumFrameLatency(d, 5));
    for (i = 0; i < OBJS_ARRAY(bad); i++) {
        UINT l = 0; HRESULT hr = IDXGIDevice1_SetMaximumFrameLatency(d, bad[i]);
        CHECKF(hr == DXGI_ERROR_INVALID_CALL, "set %u: %s", bad[i], d3dh_hr_name(hr));
        IDXGIDevice1_GetMaximumFrameLatency(d, &l);
        CHECKF(l == 5, "value changed after rejected set %u: %u", bad[i], l);
    }
    IDXGIDevice1_Release(d); objs_close(&c);
}
TEST(objs_dev_latency_zero)
{
    NEED_DEV(c);
    IDXGIDevice1 *d = get_dev1(&c); UINT l = 0xdead;
    if (!d) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIDevice1_SetMaximumFrameLatency(d, 0));
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(d, &l));
    EXPECT_EQ(l, 0);   /* stored verbatim (not reset to the default 3) */
    IDXGIDevice1_Release(d); objs_close(&c);
}
TEST(objs_dev_latency_per_device)
{
    NEED_DEV(c);
    objs_ctx c2; IDXGIDevice1 *d = get_dev1(&c), *e = NULL; UINT l = 0;
    if (!d) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    if (!objs_open(&c2)) { t_skip("second device"); IDXGIDevice1_Release(d); objs_close(&c); return; }
    e = get_dev1(&c2);
    EXPECT_HR_OK(IDXGIDevice1_SetMaximumFrameLatency(d, 9));
    EXPECT_HR_OK(IDXGIDevice1_GetMaximumFrameLatency(e, &l));
    EXPECT_EQ(l, 3);
    IDXGIDevice1_Release(e); objs_close(&c2);
    IDXGIDevice1_Release(d); objs_close(&c);
}

/* ---- GPU thread priority --------------------------------------------------------------------------------------- */
TEST(objs_dev_gpu_priority_default)
{
    NEED_DEV(c);
    INT p = 0x7777; HRESULT hr = IDXGIDevice_GetGPUThreadPriority(c.dx, &p);
    EXPECT_HR_OK(hr);
    EXPECT_EQ(p, 0);
    objs_close(&c);
}
TEST(objs_dev_gpu_priority_range)
{
    NEED_DEV(c);
    INT v;
    for (v = -7; v <= 7; v++) {
        INT p = 0x7777; HRESULT hr = IDXGIDevice_SetGPUThreadPriority(c.dx, v);
        if (FAILED(hr)) { t_note("set %d on non-WARP -> %s", v, d3dh_hr_name(hr)); continue; }
        hr = IDXGIDevice_GetGPUThreadPriority(c.dx, &p);
        CHECKF(hr == S_OK, "get after set %d: %s", v, d3dh_hr_name(hr));
        if (p != v) t_note("set %d read back %d", v, p);
    }
    IDXGIDevice_SetGPUThreadPriority(c.dx, 0);
    objs_close(&c);
}
TEST(objs_dev_gpu_priority_out_of_range)
{
    NEED_DEV(c);
    static const INT bad[] = { -8, 8, -100, 100, INT_MIN, INT_MAX }; int i;
    for (i = 0; i < OBJS_ARRAY(bad); i++) {
        HRESULT hr = IDXGIDevice_SetGPUThreadPriority(c.dx, bad[i]);
        INT p = 0x7777;
        t_note("SetGPUThreadPriority(%d) -> %s", bad[i], d3dh_hr_name(hr));
        EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
        if (SUCCEEDED(hr)) { IDXGIDevice_GetGPUThreadPriority(c.dx, &p); t_note("  readback %d", p); }
    }
    IDXGIDevice_SetGPUThreadPriority(c.dx, 0);
    objs_close(&c);
}
TEST(objs_dev_gpu_priority_per_device)
{
    NEED_DEV(c);
    objs_ctx c2; INT p = 0x7777;
    if (c.drv != D3D_DRIVER_TYPE_WARP) { t_skip("WARP only"); objs_close(&c); return; }
    if (!objs_open(&c2)) { t_skip("second device"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIDevice_SetGPUThreadPriority(c.dx, 3));
    EXPECT_HR_OK(IDXGIDevice_GetGPUThreadPriority(c2.dx, &p));
    EXPECT_EQ(p, 0);   /* the GPU thread priority is per device, not per adapter */
    IDXGIDevice_SetGPUThreadPriority(c.dx, 0);
    objs_close(&c2); objs_close(&c);
}

/* ---- device status ------------------------------------------------------------------------------------------------ */
TEST(objs_dev_removed_reason_ok)
{
    NEED_DEV(c);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(c.dev), S_OK);
    objs_close(&c);
}

/* ---- QueryResourceResidency ------------------------------------------------------------------------------------------------ */
TEST(objs_dev_residency_valid)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; ID3D11Buffer *b = NULL; IUnknown *res[2]; DXGI_RESIDENCY st[2] = { 0, 0 };
    if (FAILED(objs_rt(c.dev, &t)) || FAILED(objs_buf(c.dev, 256, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, &b))) {
        t_skip("no resources"); SAFE_RELEASE(t); objs_close(&c); return;
    }
    res[0] = (IUnknown *)t; res[1] = (IUnknown *)b;
    HRESULT hr = IDXGIDevice_QueryResourceResidency(c.dx, res, st, 2);
    EXPECT_HR_OK(hr); EXPECT_EQ(st[0], DXGI_RESIDENCY_RESIDENT_IN_SHARED_MEMORY); EXPECT_EQ(st[1], DXGI_RESIDENCY_RESIDENT_IN_SHARED_MEMORY);
    if (SUCCEEDED(hr)) {
        CHECKF(st[0] >= DXGI_RESIDENCY_FULLY_RESIDENT && st[0] <= DXGI_RESIDENCY_EVICTED_TO_DISK, "status0 %d", st[0]);
        CHECKF(st[1] >= DXGI_RESIDENCY_FULLY_RESIDENT && st[1] <= DXGI_RESIDENCY_EVICTED_TO_DISK, "status1 %d", st[1]);
    }
    ID3D11Buffer_Release(b); ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_dev_residency_single)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IUnknown *res[1]; DXGI_RESIDENCY st[1] = { 0 };
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); objs_close(&c); return; }
    res[0] = (IUnknown *)t;
    HRESULT hr = IDXGIDevice_QueryResourceResidency(c.dx, res, st, 1);
    if (SUCCEEDED(hr)) CHECKF(st[0] >= 1 && st[0] <= 3, "status %d", st[0]);
    else t_note("QueryResourceResidency(1) -> %s", d3dh_hr_name(hr));
    ID3D11Texture2D_Release(t);
    objs_close(&c);
}
TEST(objs_dev_residency_other_device_resource)
{
    NEED_DEV(c);
    objs_ctx c2; ID3D11Texture2D *t = NULL; IUnknown *res[1]; DXGI_RESIDENCY st[1] = { 0 };
    if (!objs_open(&c2)) { t_skip("second device"); objs_close(&c); return; }
    if (FAILED(objs_rt(c2.dev, &t))) { t_skip("no texture"); objs_close(&c2); objs_close(&c); return; }
    res[0] = (IUnknown *)t;
    HRESULT hr = IDXGIDevice_QueryResourceResidency(c.dx, res, st, 1);
    t_note("QueryResourceResidency(resource of another device) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    ID3D11Texture2D_Release(t);
    objs_close(&c2); objs_close(&c);
}
TEST(objs_dev_residency_zero_count)
{
    NEED_DEV(c);
    DXGI_RESIDENCY st[1] = { 0 }; IUnknown *res[1] = { NULL };
    HRESULT hr = IDXGIDevice_QueryResourceResidency(c.dx, res, st, 0);
    t_note("QueryResourceResidency(0 resources) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    objs_close(&c);
}

/* ---- OfferResources / ReclaimResources ------------------------------------------------------------------------------------------------- */
static int get_dev2(objs_ctx *c, IDXGIDevice2 **d2)
{
    *d2 = NULL;
    return SUCCEEDED(IDXGIDevice_QueryInterface(c->dx, &IID_IDXGIDevice2, (void **)d2));
}
TEST(objs_dev_offer_reclaim_roundtrip)
{
    NEED_DEV(c);
    IDXGIDevice2 *d2; ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; BOOL disc = 0x55;
    if (!get_dev2(&c, &d2)) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); IDXGIDevice2_Release(d2); objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r));
    HRESULT hr = IDXGIDevice2_OfferResources(d2, 1, &r, DXGI_OFFER_RESOURCE_PRIORITY_NORMAL);
    t_note("OfferResources(NORMAL) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) {
        hr = IDXGIDevice2_ReclaimResources(d2, 1, &r, &disc);
        EXPECT_HR_OK(hr);
        CHECKF(disc == 0 || disc == 1, "discarded flag %d", disc);
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t); IDXGIDevice2_Release(d2);
    objs_close(&c);
}
TEST(objs_dev_offer_priorities)
{
    NEED_DEV(c);
    IDXGIDevice2 *d2; ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; BOOL disc;
    static const DXGI_OFFER_RESOURCE_PRIORITY pr[] = { DXGI_OFFER_RESOURCE_PRIORITY_LOW, DXGI_OFFER_RESOURCE_PRIORITY_NORMAL, DXGI_OFFER_RESOURCE_PRIORITY_HIGH };
    int i;
    if (!get_dev2(&c, &d2)) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); IDXGIDevice2_Release(d2); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    for (i = 0; i < 3; i++) {
        HRESULT hr = IDXGIDevice2_OfferResources(d2, 1, &r, pr[i]);
        t_note("OfferResources(priority %d) -> %s", pr[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            disc = 0x55;
            EXPECT_HR_OK(IDXGIDevice2_ReclaimResources(d2, 1, &r, &disc));
        }
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t); IDXGIDevice2_Release(d2);
    objs_close(&c);
}
TEST(objs_dev_offer_bad_priority)
{
    NEED_DEV(c);
    IDXGIDevice2 *d2; ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; BOOL disc;
    if (!get_dev2(&c, &d2)) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); IDXGIDevice2_Release(d2); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    HRESULT hr = IDXGIDevice2_OfferResources(d2, 1, &r, (DXGI_OFFER_RESOURCE_PRIORITY)0);
    t_note("OfferResources(priority 0) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) IDXGIDevice2_ReclaimResources(d2, 1, &r, &disc);
    hr = IDXGIDevice2_OfferResources(d2, 1, &r, (DXGI_OFFER_RESOURCE_PRIORITY)4);
    t_note("OfferResources(priority 4) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) IDXGIDevice2_ReclaimResources(d2, 1, &r, &disc);
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t); IDXGIDevice2_Release(d2);
    objs_close(&c);
}
TEST(objs_dev_reclaim_without_offer)
{
    NEED_DEV(c);
    IDXGIDevice2 *d2; ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; BOOL disc = 0x55;
    if (!get_dev2(&c, &d2)) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); IDXGIDevice2_Release(d2); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    HRESULT hr = IDXGIDevice2_ReclaimResources(d2, 1, &r, &disc);
    t_note("ReclaimResources on a resource that was not offered -> %s (discarded %d)", d3dh_hr_name(hr), disc);
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t); IDXGIDevice2_Release(d2);
    objs_close(&c);
}
TEST(objs_dev_offer_twice)
{
    NEED_DEV(c);
    IDXGIDevice2 *d2; ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; BOOL disc;
    if (!get_dev2(&c, &d2)) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); IDXGIDevice2_Release(d2); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    HRESULT h1 = IDXGIDevice2_OfferResources(d2, 1, &r, DXGI_OFFER_RESOURCE_PRIORITY_LOW);
    if (SUCCEEDED(h1)) {
        HRESULT h2 = IDXGIDevice2_OfferResources(d2, 1, &r, DXGI_OFFER_RESOURCE_PRIORITY_LOW);
        t_note("second OfferResources of an offered resource -> %s", d3dh_hr_name(h2));
        EXPECT_HR(h2, E_INVALIDARG);   /* Win10 22H2 */
        IDXGIDevice2_ReclaimResources(d2, 1, &r, &disc);
    } else t_note("OfferResources -> %s", d3dh_hr_name(h1));
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t); IDXGIDevice2_Release(d2);
    objs_close(&c);
}
TEST(objs_dev_offer_multiple)
{
    NEED_DEV(c);
    IDXGIDevice2 *d2; ID3D11Texture2D *t[3] = { 0 }; IDXGIResource *r[3] = { 0 }; BOOL disc[3] = { 5, 5, 5 }; int i, ok = 1;
    if (!get_dev2(&c, &d2)) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    for (i = 0; i < 3; i++) {
        if (FAILED(objs_rt(c.dev, &t[i])) || FAILED(ID3D11Texture2D_QueryInterface(t[i], &IID_IDXGIResource, (void **)&r[i]))) ok = 0;
    }
    if (ok) {
        HRESULT hr = IDXGIDevice2_OfferResources(d2, 3, r, DXGI_OFFER_RESOURCE_PRIORITY_NORMAL);
        t_note("OfferResources(3) -> %s", d3dh_hr_name(hr));
        EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
        if (SUCCEEDED(hr)) {
            hr = IDXGIDevice2_ReclaimResources(d2, 3, r, disc);
            EXPECT_HR_OK(hr);
            for (i = 0; i < 3; i++) CHECKF(disc[i] == 0 || disc[i] == 1, "discarded[%d]=%d", i, disc[i]);
        }
    } else t_skip("no textures");
    for (i = 0; i < 3; i++) { SAFE_RELEASE(r[i]); SAFE_RELEASE(t[i]); }
    IDXGIDevice2_Release(d2);
    objs_close(&c);
}
TEST(objs_dev_offer_reclaim_null_discarded)
{
    NEED_DEV(c);
    IDXGIDevice2 *d2; ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL;
    if (!get_dev2(&c, &d2)) { t_skip("no IDXGIDevice2"); objs_close(&c); return; }
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); IDXGIDevice2_Release(d2); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    if (SUCCEEDED(IDXGIDevice2_OfferResources(d2, 1, &r, DXGI_OFFER_RESOURCE_PRIORITY_NORMAL))) {
        HRESULT hr = IDXGIDevice2_ReclaimResources(d2, 1, &r, NULL);
        t_note("ReclaimResources with NULL pDiscarded -> %s (NULL is documented as allowed)", d3dh_hr_name(hr));
        EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t); IDXGIDevice2_Release(d2);
    objs_close(&c);
}

/* ---- Trim (IDXGIDevice3) / IDXGIDevice4 ------------------------------------------------------------------------------------------------------------ */
TEST(objs_dev_trim_keeps_working)
{
    NEED_DEV(c);
    IDXGIDevice3 *d3 = NULL; ID3D11Texture2D *t = NULL; int i;
    if (FAILED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice3, (void **)&d3))) { t_skip("no IDXGIDevice3"); objs_close(&c); return; }
    for (i = 0; i < 3; i++) IDXGIDevice3_Trim(d3);
    EXPECT_HR_OK(objs_rt(c.dev, &t));
    SAFE_RELEASE(t);
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(c.dev), S_OK);
    IDXGIDevice3_Release(d3);
    objs_close(&c);
}
TEST(objs_dev_trim_with_live_resource)
{
    NEED_DEV(c);
    IDXGIDevice3 *d3 = NULL; ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC d;
    if (FAILED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice3, (void **)&d3))) { t_skip("no IDXGIDevice3"); objs_close(&c); return; }
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); IDXGIDevice3_Release(d3); objs_close(&c); return; }
    IDXGIDevice3_Trim(d3);
    ID3D11Texture2D_GetDesc(t, &d);
    EXPECT_EQ(d.Width, 64);
    ID3D11Texture2D_Release(t);
    IDXGIDevice3_Release(d3);
    objs_close(&c);
}
TEST(objs_dev_offer1_reclaim1)
{
    NEED_DEV(c);
    IDXGIDevice4 *d4 = NULL; ID3D11Texture2D *t = NULL; IDXGIResource *r = NULL; DXGI_RECLAIM_RESOURCE_RESULTS res = 0xff;
    if (FAILED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice4, (void **)&d4))) { t_skip("no IDXGIDevice4"); objs_close(&c); return; }
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); IDXGIDevice4_Release(d4); objs_close(&c); return; }
    ID3D11Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r);
    HRESULT hr = IDXGIDevice4_OfferResources1(d4, 1, &r, DXGI_OFFER_RESOURCE_PRIORITY_LOW, 0);
    t_note("OfferResources1 -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) {
        hr = IDXGIDevice4_ReclaimResources1(d4, 1, &r, &res);
        EXPECT_HR_OK(hr);
        CHECKF(res == DXGI_RECLAIM_RESOURCE_RESULT_OK || res == DXGI_RECLAIM_RESOURCE_RESULT_DISCARDED || res == DXGI_RECLAIM_RESOURCE_RESULT_NOT_COMMITTED, "result %d", res);
    }
    IDXGIResource_Release(r); ID3D11Texture2D_Release(t); IDXGIDevice4_Release(d4);
    objs_close(&c);
}

/* ---- IDXGIDevice::CreateSurface ---------------------------------------------------------------------------------------------------------------------------- */
static void surface_desc(DXGI_SURFACE_DESC *d, UINT w, UINT h, DXGI_FORMAT f)
{
    memset(d, 0, sizeof(*d)); d->Width = w; d->Height = h; d->Format = f; d->SampleDesc.Count = 1;
}
TEST(objs_dev_createsurface_desc_roundtrip)
{
    NEED_DEV(c);
    static const struct { UINT w, h; DXGI_FORMAT f; } tab[] = {
        { 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM }, { 64, 32, DXGI_FORMAT_B8G8R8A8_UNORM }, { 512, 512, DXGI_FORMAT_R8G8B8A8_UNORM },
        { 17, 33, DXGI_FORMAT_R16G16B16A16_FLOAT }, { 128, 8, DXGI_FORMAT_R32_FLOAT }, { 100, 100, DXGI_FORMAT_R10G10B10A2_UNORM },
        { 256, 256, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
    };
    int i;
    for (i = 0; i < OBJS_ARRAY(tab); i++) {
        DXGI_SURFACE_DESC d, g; IDXGISurface *s = NULL;
        surface_desc(&d, tab[i].w, tab[i].h, tab[i].f);
        HRESULT hr = IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s);
        CHECKF(hr == S_OK, "case %d: %s", i, d3dh_hr_name(hr));
        if (!s) continue;
        memset(&g, 0xcc, sizeof(g));
        EXPECT_HR_OK(IDXGISurface_GetDesc(s, &g));
        CHECKF(g.Width == d.Width && g.Height == d.Height && g.Format == d.Format, "case %d desc %ux%u fmt %d", i, g.Width, g.Height, g.Format);
        CHECKF(g.SampleDesc.Count == 1 && g.SampleDesc.Quality == 0, "case %d sample %u/%u", i, g.SampleDesc.Count, g.SampleDesc.Quality);
        IDXGISurface_Release(s);
    }
    objs_close(&c);
}
TEST(objs_dev_createsurface_interfaces)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL; IUnknown *p = NULL; HRESULT hr;
    surface_desc(&d, 128, 128, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR_OK(IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s));
    if (!s) { objs_close(&c); return; }
    EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGIResource, (void **)&p)); SAFE_RELEASE(p);
    EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGIDeviceSubObject, (void **)&p)); SAFE_RELEASE(p);
    EXPECT_HR_OK(IDXGISurface_QueryInterface(s, &IID_IDXGIObject, (void **)&p)); SAFE_RELEASE(p);
    hr = IDXGISurface_QueryInterface(s, &IID_ID3D10Texture2D, (void **)&p);
    t_note("surface->ID3D10Texture2D: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    hr = IDXGISurface_QueryInterface(s, &IID_ID3D11Texture2D, (void **)&p);
    t_note("surface->ID3D11Texture2D: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    hr = IDXGISurface_QueryInterface(s, &IID_IDXGISurface1, (void **)&p);
    t_note("surface->IDXGISurface1: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    hr = IDXGISurface_QueryInterface(s, &IID_IDXGISurface2, (void **)&p);
    t_note("surface->IDXGISurface2: %s", d3dh_hr_name(hr)); SAFE_RELEASE(p);
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    hr = IDXGISurface_QueryInterface(s, &IID_IDXGIFactory, (void **)&p);
    EXPECT_HR(hr, E_NOINTERFACE);
    IDXGISurface_Release(s);
    objs_close(&c);
}
TEST(objs_dev_createsurface_uav)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL; ID3D11Texture2D *t = NULL; ID3D11UnorderedAccessView *uav = NULL;
    if (c.fl < D3D_FEATURE_LEVEL_11_0) { t_skip("UAV on textures needs feature level 11_0"); objs_close(&c); return; }
    surface_desc(&d, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR_OK(IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_UNORDERED_ACCESS, NULL, &s));
    if (s && SUCCEEDED(IDXGISurface_QueryInterface(s, &IID_ID3D11Texture2D, (void **)&t))) {
        EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(c.dev, (ID3D11Resource *)t, NULL, &uav));
        SAFE_RELEASE(uav); ID3D11Texture2D_Release(t);
    }
    SAFE_RELEASE(s);
    objs_close(&c);
}
TEST(objs_dev_createsurface_array)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s[3] = { 0 }; int i;
    surface_desc(&d, 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM);
    HRESULT hr = IDXGIDevice_CreateSurface(c.dx, &d, 3, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, s);
    t_note("CreateSurface(NumSurfaces=3) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    if (SUCCEEDED(hr)) {
        for (i = 0; i < 3; i++) CHECKF(s[i] != NULL, "surface %d NULL", i);
        CHECK(s[0] != s[1] && s[1] != s[2] && s[0] != s[2]);
        for (i = 0; i < 3; i++) SAFE_RELEASE(s[i]);
    }
    objs_close(&c);
}
TEST(objs_dev_createsurface_zero_count)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = (IDXGISurface *)(ULONG_PTR)1;
    surface_desc(&d, 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM);
    HRESULT hr = IDXGIDevice_CreateSurface(c.dx, &d, 0, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s);
    t_note("CreateSurface(NumSurfaces=0) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    if (SUCCEEDED(hr) && s != (IDXGISurface *)(ULONG_PTR)1 && s) IDXGISurface_Release(s);
    objs_close(&c);
}
TEST(objs_dev_createsurface_zero_size)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL;
    surface_desc(&d, 0, 32, DXGI_FORMAT_R8G8B8A8_UNORM);
    HRESULT hr = IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s);
    EXPECT_HR_FAILED(hr);
    t_note("CreateSurface(width 0) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
    SAFE_RELEASE(s);
    surface_desc(&d, 32, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    hr = IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s);
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(s);
    objs_close(&c);
}
TEST(objs_dev_createsurface_unknown_format)
{
    NEED_DEV(c);
    DXGI_SURFACE_DESC d; IDXGISurface *s = NULL;
    surface_desc(&d, 32, 32, DXGI_FORMAT_UNKNOWN);
    HRESULT hr = IDXGIDevice_CreateSurface(c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s);
    EXPECT_HR_FAILED(hr);
    t_note("CreateSurface(DXGI_FORMAT_UNKNOWN) -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 22H2 */
    SAFE_RELEASE(s);
    objs_close(&c);
}
TEST(objs_dev_createsurface_usage_matrix)
{
    NEED_DEV(c);
    static const DXGI_USAGE u[] = { DXGI_USAGE_SHADER_INPUT, DXGI_USAGE_RENDER_TARGET_OUTPUT, DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT, DXGI_USAGE_SHARED, 0 };
    int i;
    for (i = 0; i < OBJS_ARRAY(u); i++) {
        DXGI_SURFACE_DESC d; IDXGISurface *s = NULL; IDXGIResource *r = NULL; DXGI_USAGE got = 0xdead;
        surface_desc(&d, 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM);
        HRESULT hr = IDXGIDevice_CreateSurface(c.dx, &d, 1, u[i], NULL, &s);
        t_note("CreateSurface(usage %#x) -> %s", u[i], d3dh_hr_name(hr));
        EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
        if (SUCCEEDED(hr) && s && SUCCEEDED(IDXGISurface_QueryInterface(s, &IID_IDXGIResource, (void **)&r))) {
            EXPECT_HR_OK(IDXGIResource_GetUsage(r, &got));
            t_note("  GetUsage -> %#x", got);
            IDXGIResource_Release(r);
        }
        SAFE_RELEASE(s);
    }
    objs_close(&c);
}
