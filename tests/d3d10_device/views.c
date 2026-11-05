/* Views: RTV / DSV / SRV default descriptors, validation, resource back-pointers, D3D10.1 SRV1. */
#include "dd.h"

#define T(n) TEST(d3d10_device_view_##n)
#define SR D3D10_BIND_SHADER_RESOURCE
#define RT D3D10_BIND_RENDER_TARGET
#define DS D3D10_BIND_DEPTH_STENCIL
#define DEF D3D10_USAGE_DEFAULT
#define RGBA DXGI_FORMAT_R8G8B8A8_UNORM

static ID3D10Texture2D *mk2d(ID3D10Device *dev, UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT f, UINT ms, UINT bind, UINT misc)
{
    D3D10_TEXTURE2D_DESC d = { w, h, mips, arr, f, { ms, 0 }, DEF, bind, 0, misc };
    ID3D10Texture2D *t = NULL;
    HRESULT hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &t);
    if (FAILED(hr)) { t_note("setup texture failed: %s", d3dh_hr_name(hr)); return NULL; }
    return t;
}
#define R(x) ((ID3D10Resource *)(x))

/* ---- RTV ---- */
T(rtv_default_desc_2d)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 16, 16, 5, 1, RGBA, 1, RT | SR, 0);
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v));
        if (v) {
            memset(&d, 0xcc, sizeof d);
            ID3D10RenderTargetView_GetDesc(v, &d);
            EXPECT_EQ(d.Format, RGBA); EXPECT_EQ(d.ViewDimension, D3D10_RTV_DIMENSION_TEXTURE2D); EXPECT_EQ(d.Texture2D.MipSlice, 0);
            ID3D10RenderTargetView_Release(v);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_default_desc_2d_array)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 4, RGBA, 1, RT, 0);
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v));
        if (v) {
            ID3D10RenderTargetView_GetDesc(v, &d);
            EXPECT_EQ(d.ViewDimension, D3D10_RTV_DIMENSION_TEXTURE2DARRAY);
            EXPECT_EQ(d.Texture2DArray.MipSlice, 0); EXPECT_EQ(d.Texture2DArray.FirstArraySlice, 0); EXPECT_EQ(d.Texture2DArray.ArraySize, 4);
            ID3D10RenderTargetView_Release(v);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_default_desc_ms)
{
    DD_DEV_R(dev);
    UINT q = 0; ID3D10Device_CheckMultisampleQualityLevels(dev, RGBA, 4, &q);
    if (!q) { DD_END(dev); t_skip("no 4x MSAA"); return; }
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 4, RT, 0);
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v));
        if (v) { ID3D10RenderTargetView_GetDesc(v, &d); EXPECT_EQ(d.ViewDimension, D3D10_RTV_DIMENSION_TEXTURE2DMS); ID3D10RenderTargetView_Release(v); }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_default_desc_1d)
{
    DD_DEV(dev);
    D3D10_TEXTURE1D_DESC td = { 16, 1, 1, RGBA, DEF, RT, 0, 0 }; ID3D10Texture1D *t = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture1D(dev, &td, NULL, &t));
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v));
        if (v) { ID3D10RenderTargetView_GetDesc(v, &d); EXPECT_EQ(d.ViewDimension, D3D10_RTV_DIMENSION_TEXTURE1D); EXPECT_EQ(d.Format, RGBA); EXPECT_EQ(d.Texture1D.MipSlice, 0); ID3D10RenderTargetView_Release(v); }
        ID3D10Texture1D_Release(t);
    }
    DD_END(dev);
}
T(rtv_default_desc_1d_array)
{
    DD_DEV(dev);
    D3D10_TEXTURE1D_DESC td = { 16, 1, 3, RGBA, DEF, RT, 0, 0 }; ID3D10Texture1D *t = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture1D(dev, &td, NULL, &t));
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v));
        if (v) { ID3D10RenderTargetView_GetDesc(v, &d); EXPECT_EQ(d.ViewDimension, D3D10_RTV_DIMENSION_TEXTURE1DARRAY); EXPECT_EQ(d.Texture1DArray.ArraySize, 3); EXPECT_EQ(d.Texture1DArray.FirstArraySlice, 0); ID3D10RenderTargetView_Release(v); }
        ID3D10Texture1D_Release(t);
    }
    DD_END(dev);
}
T(rtv_default_desc_3d)
{
    DD_DEV(dev);
    D3D10_TEXTURE3D_DESC td = { 8, 8, 6, 1, RGBA, DEF, RT, 0, 0 }; ID3D10Texture3D *t = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture3D(dev, &td, NULL, &t));
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v));
        if (v) {
            ID3D10RenderTargetView_GetDesc(v, &d);
            EXPECT_EQ(d.ViewDimension, D3D10_RTV_DIMENSION_TEXTURE3D);
            EXPECT_EQ(d.Texture3D.MipSlice, 0); EXPECT_EQ(d.Texture3D.FirstWSlice, 0); EXPECT_EQ(d.Texture3D.WSize, 6);
            ID3D10RenderTargetView_Release(v);
        }
        ID3D10Texture3D_Release(t);
    }
    DD_END(dev);
}
T(rtv_buffer_requires_desc)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC bd = { 256, DEF, RT | SR, 0, 0 }; ID3D10Buffer *b = NULL; ID3D10RenderTargetView *v = (void *)1;
    if (SUCCEEDED(ID3D10Device_CreateBuffer(dev, &bd, NULL, &b))) {
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(b), NULL, &v), E_INVALIDARG);
        D3D10_RENDER_TARGET_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_FLOAT; d.ViewDimension = D3D10_RTV_DIMENSION_BUFFER; d.Buffer.ElementOffset = 0; d.Buffer.ElementWidth = 16;
        v = NULL;
        HRESULT hr = ID3D10Device_CreateRenderTargetView(dev, R(b), &d, &v);
        t_note("buffer RTV with explicit desc: %s", d3dh_hr_name(hr));
        SAFE_RELEASE(v);
        ID3D10Buffer_Release(b);
    } else t_skip("buffer with RT bind not supported");
    DD_END(dev);
}
T(rtv_explicit_roundtrip_mip_slice)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 16, 16, 5, 1, RGBA, 1, RT | SR, 0);
    if (t) {
        for (UINT m = 0; m < 5; m++) {
            D3D10_RENDER_TARGET_VIEW_DESC d, g; ID3D10RenderTargetView *v = NULL;
            memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D; d.Texture2D.MipSlice = m;
            EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v));
            if (v) { memset(&g, 0, sizeof g); ID3D10RenderTargetView_GetDesc(v, &g); EXPECT_EQ(g.Texture2D.MipSlice, m); EXPECT_EQ(g.Format, RGBA); ID3D10RenderTargetView_Release(v); }
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_mip_slice_out_of_range)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 16, 16, 5, 1, RGBA, 1, RT | SR, 0);
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = (void *)1;
        memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D; d.Texture2D.MipSlice = 5;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Texture2D.MipSlice = 0xffffffffu;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_array_slice_validation)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 4, RGBA, 1, RT, 0);
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = NULL;
        memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
        d.Texture2DArray.FirstArraySlice = 1; d.Texture2DArray.ArraySize = 3;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v));
        if (v) { D3D10_RENDER_TARGET_VIEW_DESC g; ID3D10RenderTargetView_GetDesc(v, &g); EXPECT_EQ(g.Texture2DArray.FirstArraySlice, 1); EXPECT_EQ(g.Texture2DArray.ArraySize, 3); ID3D10RenderTargetView_Release(v); }
        d.Texture2DArray.FirstArraySlice = 2; d.Texture2DArray.ArraySize = 3;
        v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Texture2DArray.FirstArraySlice = 4; d.Texture2DArray.ArraySize = 1;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Texture2DArray.FirstArraySlice = 0; d.Texture2DArray.ArraySize = 0;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Texture2DArray.FirstArraySlice = 0; d.Texture2DArray.ArraySize = 0xffffffffu;
        t_note("ArraySize -1 (all remaining): %s", d3dh_hr_name(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, NULL)));
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_dimension_mismatch)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, RT, 0);
    if (t) {
        static const UINT dims[] = { D3D10_RTV_DIMENSION_TEXTURE1D, D3D10_RTV_DIMENSION_TEXTURE1DARRAY, D3D10_RTV_DIMENSION_TEXTURE3D, D3D10_RTV_DIMENSION_BUFFER, D3D10_RTV_DIMENSION_TEXTURE2DMS };
        for (unsigned i = 0; i < 5; i++) {
            D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = (void *)1;
            memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = (D3D10_RTV_DIMENSION)dims[i];
            if (dims[i] == D3D10_RTV_DIMENSION_TEXTURE1DARRAY || dims[i] == D3D10_RTV_DIMENSION_TEXTURE3D) { d.Texture3D.WSize = 1; }
            HRESULT hr = ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "dim mismatch", "dimension %u: %s", dims[i], d3dh_hr_name(hr));
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_invalid_dimension_values)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, RT, 0);
    if (t) {
        static const UINT dims[] = { 0, 9, 100, 0xffffffffu };
        for (unsigned i = 0; i < 4; i++) {
            D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = (void *)1;
            memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = (D3D10_RTV_DIMENSION)dims[i];
            HRESULT hr = ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "bad dim", "dimension %u: %s", dims[i], d3dh_hr_name(hr));
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_format_not_castable)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, RT, 0);
    if (t) {
        static const DXGI_FORMAT f[] = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_D32_FLOAT };
        for (unsigned i = 0; i < 5; i++) {
            D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = (void *)1;
            memset(&d, 0, sizeof d); d.Format = f[i]; d.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
            HRESULT hr = ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "rtv format", "format %u: %s", f[i], d3dh_hr_name(hr));
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_typeless_needs_desc)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, 1, RT | SR, 0);
    if (t) {
        ID3D10RenderTargetView *v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v), E_INVALIDARG);
        D3D10_RENDER_TARGET_VIEW_DESC d; memset(&d, 0, sizeof d); d.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
        static const DXGI_FORMAT ok[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_SINT };
        for (unsigned i = 0; i < 5; i++) {
            d.Format = ok[i]; v = NULL;
            HRESULT hr = ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v);
            t_check(hr == S_OK, __FILE__, __LINE__, "typeless view", "format %u: %s", ok[i], d3dh_hr_name(hr));
            if (v) { D3D10_RENDER_TARGET_VIEW_DESC g; ID3D10RenderTargetView_GetDesc(v, &g); EXPECT_EQ(g.Format, ok[i]); ID3D10RenderTargetView_Release(v); }
        }
        d.Format = DXGI_FORMAT_R32_FLOAT; v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_requires_bind_flag)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, SR, 0);
    if (t) { ID3D10RenderTargetView *v = (void *)1; EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v), E_INVALIDARG); ID3D10Texture2D_Release(t); }
    DD_END(dev);
}
T(rtv_null_resource)
{
    DD_DEV(dev);
    ID3D10RenderTargetView *v = (void *)1;
    EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, NULL, NULL, &v), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, NULL, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(rtv_validate_only_s_false)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, RT, 0);
    if (t) { EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, NULL), S_FALSE); ID3D10Texture2D_Release(t); }
    DD_END(dev);
}
T(rtv_resource_backpointer)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, RT, 0);
    if (t) {
        ID3D10RenderTargetView *v = NULL; ID3D10Resource *r = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v));
        if (v) {
            ULONG rc = d3dh_refcount(t);
            ID3D10RenderTargetView_GetResource(v, &r);
            CHECK(r == R(t));
            EXPECT_EQ(d3dh_refcount(t), rc + 1);
            SAFE_RELEASE(r);
            /* the view keeps the texture alive */
            ID3D10Texture2D_Release(t); t = NULL;
            ID3D10RenderTargetView_GetResource(v, &r);
            CHECK(r != NULL);
            if (r) { D3D10_RESOURCE_DIMENSION dim = 0; ID3D10Resource_GetType(r, &dim); EXPECT_EQ(dim, D3D10_RESOURCE_DIMENSION_TEXTURE2D); }
            SAFE_RELEASE(r);
            ID3D10Device *got = NULL; ID3D10RenderTargetView_GetDevice(v, &got); CHECK(got == dev); SAFE_RELEASE(got);
            EXPECT_EQ(ID3D10RenderTargetView_Release(v), 0);
        }
        SAFE_RELEASE(t);
    }
    DD_END(dev);
}
T(rtv_qi_matrix)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, RT, 0);
    if (t) {
        ID3D10RenderTargetView *v = NULL; void *p;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &v));
        if (v) {
            static const IID *yes[] = { &IID_IUnknown, &IID_ID3D10RenderTargetView, &IID_ID3D10View, &IID_ID3D10DeviceChild };
            for (unsigned i = 0; i < 4; i++) { p = NULL; HRESULT hr = ID3D10RenderTargetView_QueryInterface(v, yes[i], &p); t_check(hr == S_OK, __FILE__, __LINE__, "rtv QI", "%s: %s", d3dh_guid_str(yes[i]), d3dh_hr_name(hr)); if (p) IUnknown_Release((IUnknown *)p); }
            static const IID *no[] = { &IID_ID3D10ShaderResourceView, &IID_ID3D10DepthStencilView, &IID_ID3D10Resource };
            for (unsigned i = 0; i < 3; i++) { p = (void *)1; HRESULT hr = ID3D10RenderTargetView_QueryInterface(v, no[i], &p); t_check(hr == E_NOINTERFACE && !p, __FILE__, __LINE__, "rtv QI neg", "%s: %s", d3dh_guid_str(no[i]), d3dh_hr_name(hr)); if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p); }
            ID3D10RenderTargetView_Release(v);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(rtv_3d_wslice_validation)
{
    DD_DEV(dev);
    D3D10_TEXTURE3D_DESC td = { 8, 8, 8, 4, RGBA, DEF, RT, 0, 0 }; ID3D10Texture3D *t = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture3D(dev, &td, NULL, &t));
    if (t) {
        D3D10_RENDER_TARGET_VIEW_DESC d; ID3D10RenderTargetView *v = NULL;
        memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D; d.Texture3D.MipSlice = 1; d.Texture3D.FirstWSlice = 1; d.Texture3D.WSize = 3;
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v));
        SAFE_RELEASE(v);
        d.Texture3D.FirstWSlice = 2; d.Texture3D.WSize = 3; /* mip 1 has depth 4: slices 2..4 out of range */
        v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Texture3D.MipSlice = 4;
        EXPECT_HR(ID3D10Device_CreateRenderTargetView(dev, R(t), &d, &v), E_INVALIDARG);
        ID3D10Texture3D_Release(t);
    }
    DD_END(dev);
}

/* ---- DSV ---- */
T(dsv_default_desc)
{
    DD_DEV(dev);
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT };
    for (unsigned i = 0; i < 4; i++) {
        ID3D10Texture2D *t = mk2d(dev, 16, 16, 1, 1, f[i], 1, DS, 0);
        if (!t) continue;
        D3D10_DEPTH_STENCIL_VIEW_DESC d; ID3D10DepthStencilView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateDepthStencilView(dev, R(t), NULL, &v));
        if (v) { memset(&d, 0xcc, sizeof d); ID3D10DepthStencilView_GetDesc(v, &d); EXPECT_EQ(d.Format, f[i]); EXPECT_EQ(d.ViewDimension, D3D10_DSV_DIMENSION_TEXTURE2D); EXPECT_EQ(d.Texture2D.MipSlice, 0); ID3D10DepthStencilView_Release(v); }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(dsv_default_desc_array)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 3, DXGI_FORMAT_D32_FLOAT, 1, DS, 0);
    if (t) {
        D3D10_DEPTH_STENCIL_VIEW_DESC d; ID3D10DepthStencilView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateDepthStencilView(dev, R(t), NULL, &v));
        if (v) { ID3D10DepthStencilView_GetDesc(v, &d); EXPECT_EQ(d.ViewDimension, D3D10_DSV_DIMENSION_TEXTURE2DARRAY); EXPECT_EQ(d.Texture2DArray.FirstArraySlice, 0); EXPECT_EQ(d.Texture2DArray.ArraySize, 3); ID3D10DepthStencilView_Release(v); }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(dsv_typeless_needs_desc)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R24G8_TYPELESS, 1, DS | SR, 0);
    if (t) {
        ID3D10DepthStencilView *v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), NULL, &v), E_INVALIDARG);
        D3D10_DEPTH_STENCIL_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; d.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D; v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v));
        SAFE_RELEASE(v);
        d.Format = DXGI_FORMAT_R24G8_TYPELESS; v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Format = DXGI_FORMAT_D32_FLOAT;
        EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v), E_INVALIDARG);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(dsv_requires_bind_flag)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R24G8_TYPELESS, 1, SR, 0);
    if (t) {
        D3D10_DEPTH_STENCIL_VIEW_DESC d; memset(&d, 0, sizeof d); d.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; d.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
        ID3D10DepthStencilView *v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v), E_INVALIDARG);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(dsv_null_resource_and_out)
{
    DD_DEV(dev);
    ID3D10DepthStencilView *v = (void *)1;
    EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, NULL, NULL, &v), E_INVALIDARG);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_D32_FLOAT, 1, DS, 0);
    if (t) { EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), NULL, NULL), S_FALSE); ID3D10Texture2D_Release(t); }
    DD_END(dev);
}
T(dsv_mip_and_array_validation)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 16, 16, 3, 2, DXGI_FORMAT_D32_FLOAT, 1, DS, 0);
    if (t) {
        D3D10_DEPTH_STENCIL_VIEW_DESC d; ID3D10DepthStencilView *v = NULL;
        memset(&d, 0, sizeof d); d.Format = DXGI_FORMAT_D32_FLOAT; d.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
        d.Texture2DArray.MipSlice = 2; d.Texture2DArray.FirstArraySlice = 1; d.Texture2DArray.ArraySize = 1;
        EXPECT_HR_OK(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v));
        if (v) { D3D10_DEPTH_STENCIL_VIEW_DESC g; ID3D10DepthStencilView_GetDesc(v, &g); EXPECT_EQ(g.Texture2DArray.MipSlice, 2); EXPECT_EQ(g.Texture2DArray.FirstArraySlice, 1); ID3D10DepthStencilView_Release(v); }
        v = (void *)1;
        d.Texture2DArray.MipSlice = 3;
        EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v), E_INVALIDARG);
        d.Texture2DArray.MipSlice = 0; d.Texture2DArray.FirstArraySlice = 2;
        EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v), E_INVALIDARG);
        d.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE1D;
        EXPECT_HR(ID3D10Device_CreateDepthStencilView(dev, R(t), &d, &v), E_INVALIDARG);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(dsv_resource_backpointer)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_D32_FLOAT, 1, DS, 0);
    if (t) {
        ID3D10DepthStencilView *v = NULL; ID3D10Resource *r = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateDepthStencilView(dev, R(t), NULL, &v));
        if (v) { ID3D10DepthStencilView_GetResource(v, &r); CHECK(r == R(t)); SAFE_RELEASE(r); ID3D10DepthStencilView_Release(v); }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}

/* ---- SRV ---- */
T(srv_default_desc_2d_all_mips)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 16, 16, 5, 1, RGBA, 1, SR, 0);
    if (t) {
        D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &v));
        if (v) {
            memset(&d, 0xcc, sizeof d); ID3D10ShaderResourceView_GetDesc(v, &d);
            EXPECT_EQ(d.Format, RGBA); EXPECT_EQ(d.ViewDimension, D3D10_SRV_DIMENSION_TEXTURE2D);
            EXPECT_EQ(d.Texture2D.MostDetailedMip, 0); EXPECT_EQ(d.Texture2D.MipLevels, 5);
            ID3D10ShaderResourceView_Release(v);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_default_desc_2d_array)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 3, 5, RGBA, 1, SR, 0);
    if (t) {
        D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &v));
        if (v) {
            ID3D10ShaderResourceView_GetDesc(v, &d);
            EXPECT_EQ(d.ViewDimension, D3D10_SRV_DIMENSION_TEXTURE2DARRAY);
            EXPECT_EQ(d.Texture2DArray.MostDetailedMip, 0); EXPECT_EQ(d.Texture2DArray.MipLevels, 3);
            EXPECT_EQ(d.Texture2DArray.FirstArraySlice, 0); EXPECT_EQ(d.Texture2DArray.ArraySize, 5);
            ID3D10ShaderResourceView_Release(v);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_default_desc_cube)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 4, 6, RGBA, 1, SR, D3D10_RESOURCE_MISC_TEXTURECUBE);
    if (t) {
        D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &v));
        if (v) {
            ID3D10ShaderResourceView_GetDesc(v, &d);
            EXPECT_EQ(d.ViewDimension, D3D10_SRV_DIMENSION_TEXTURECUBE);
            EXPECT_EQ(d.TextureCube.MostDetailedMip, 0); EXPECT_EQ(d.TextureCube.MipLevels, 4);
            ID3D10ShaderResourceView_Release(v);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_default_desc_ms)
{
    DD_DEV_R(dev);
    UINT q = 0; ID3D10Device_CheckMultisampleQualityLevels(dev, RGBA, 4, &q);
    if (!q) { DD_END(dev); t_skip("no 4x MSAA"); return; }
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 4, RT | SR, 0);
    if (t) {
        D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = NULL;
        HRESULT hr = ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &v);
        if (SUCCEEDED(hr) && v) { ID3D10ShaderResourceView_GetDesc(v, &d); EXPECT_EQ(d.ViewDimension, D3D10_SRV_DIMENSION_TEXTURE2DMS); ID3D10ShaderResourceView_Release(v); }
        else t_note("MS SRV default: %s", d3dh_hr_name(hr));
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_default_desc_1d_and_3d)
{
    DD_DEV(dev);
    D3D10_TEXTURE1D_DESC t1 = { 16, 3, 1, RGBA, DEF, SR, 0, 0 }; ID3D10Texture1D *a = NULL;
    D3D10_TEXTURE3D_DESC t3 = { 8, 8, 8, 4, RGBA, DEF, SR, 0, 0 }; ID3D10Texture3D *b = NULL;
    D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture1D(dev, &t1, NULL, &a));
    if (a && SUCCEEDED(ID3D10Device_CreateShaderResourceView(dev, R(a), NULL, &v))) {
        ID3D10ShaderResourceView_GetDesc(v, &d);
        EXPECT_EQ(d.ViewDimension, D3D10_SRV_DIMENSION_TEXTURE1D); EXPECT_EQ(d.Texture1D.MostDetailedMip, 0); EXPECT_EQ(d.Texture1D.MipLevels, 3);
        ID3D10ShaderResourceView_Release(v); v = NULL;
    }
    EXPECT_HR_OK(ID3D10Device_CreateTexture3D(dev, &t3, NULL, &b));
    if (b && SUCCEEDED(ID3D10Device_CreateShaderResourceView(dev, R(b), NULL, &v))) {
        ID3D10ShaderResourceView_GetDesc(v, &d);
        EXPECT_EQ(d.ViewDimension, D3D10_SRV_DIMENSION_TEXTURE3D); EXPECT_EQ(d.Texture3D.MostDetailedMip, 0); EXPECT_EQ(d.Texture3D.MipLevels, 4);
        ID3D10ShaderResourceView_Release(v);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    DD_END(dev);
}
T(srv_default_desc_1d_array)
{
    DD_DEV(dev);
    D3D10_TEXTURE1D_DESC t1 = { 16, 1, 4, RGBA, DEF, SR, 0, 0 }; ID3D10Texture1D *a = NULL; ID3D10ShaderResourceView *v = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture1D(dev, &t1, NULL, &a));
    if (a && SUCCEEDED(ID3D10Device_CreateShaderResourceView(dev, R(a), NULL, &v))) {
        D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView_GetDesc(v, &d);
        EXPECT_EQ(d.ViewDimension, D3D10_SRV_DIMENSION_TEXTURE1DARRAY); EXPECT_EQ(d.Texture1DArray.ArraySize, 4); EXPECT_EQ(d.Texture1DArray.FirstArraySlice, 0);
        ID3D10ShaderResourceView_Release(v);
    }
    SAFE_RELEASE(a);
    DD_END(dev);
}
T(srv_typeless_needs_desc)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, 1, SR, 0);
    if (t) {
        ID3D10ShaderResourceView *v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &v), E_INVALIDARG);
        D3D10_SHADER_RESOURCE_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; d.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1; v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(t), &d, &v));
        if (v) { D3D10_SHADER_RESOURCE_VIEW_DESC g; ID3D10ShaderResourceView_GetDesc(v, &g); EXPECT_EQ(g.Format, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB); ID3D10ShaderResourceView_Release(v); }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_depth_typeless_formats)
{
    DD_DEV(dev);
    static const struct { DXGI_FORMAT res, view, bad; } c[] = {
        { DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_R32_FLOAT },
        { DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS },
        { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R8_UNORM },
    };
    for (unsigned i = 0; i < 3; i++) {
        ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, c[i].res, 1, DS | SR, 0);
        if (!t) continue;
        D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = NULL;
        memset(&d, 0, sizeof d); d.Format = c[i].view; d.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1;
        HRESULT hr = ID3D10Device_CreateShaderResourceView(dev, R(t), &d, &v);
        t_check(hr == S_OK, __FILE__, __LINE__, "depth srv", "res %u view %u: %s", c[i].res, c[i].view, d3dh_hr_name(hr));
        SAFE_RELEASE(v);
        d.Format = c[i].bad; v = (void *)1;
        hr = ID3D10Device_CreateShaderResourceView(dev, R(t), &d, &v);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "depth srv bad", "res %u view %u: %s", c[i].res, c[i].bad, d3dh_hr_name(hr));
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_requires_bind_flag)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, RT, 0);
    if (t) { ID3D10ShaderResourceView *v = (void *)1; EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &v), E_INVALIDARG); ID3D10Texture2D_Release(t); }
    DD_END(dev);
}
T(srv_null_resource_and_out)
{
    DD_DEV(dev);
    ID3D10ShaderResourceView *v = (void *)1;
    EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, NULL, NULL, &v), E_INVALIDARG);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, SR, 0);
    if (t) { EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, NULL), S_FALSE); ID3D10Texture2D_Release(t); }
    DD_END(dev);
}
T(srv_mip_range_validation)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 16, 16, 5, 1, RGBA, 1, SR, 0);
    if (t) {
        static const struct { UINT most, levels; int ok; } c[] = {
            { 0, 5, 1 }, { 0, 1, 1 }, { 4, 1, 1 }, { 2, 3, 1 }, { 1, 4, 1 },
            { 5, 1, 0 }, { 0, 6, 0 }, { 4, 2, 0 }, { 3, 3, 0 }, { 0xffffffffu, 1, 0 },
        };
        for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
            D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = c[i].ok ? NULL : (void *)1;
            memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MostDetailedMip = c[i].most; d.Texture2D.MipLevels = c[i].levels;
            HRESULT hr = ID3D10Device_CreateShaderResourceView(dev, R(t), &d, &v);
            t_check(hr == (c[i].ok ? S_OK : E_INVALIDARG), __FILE__, __LINE__, "srv mips", "most %u levels %u: %s", c[i].most, c[i].levels, d3dh_hr_name(hr));
            if (SUCCEEDED(hr) && v) {
                D3D10_SHADER_RESOURCE_VIEW_DESC g; ID3D10ShaderResourceView_GetDesc(v, &g);
                EXPECT_EQ(g.Texture2D.MostDetailedMip, c[i].most); EXPECT_EQ(g.Texture2D.MipLevels, c[i].levels);
                ID3D10ShaderResourceView_Release(v);
            }
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_mip_levels_minus_one_observed)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 16, 16, 5, 1, RGBA, 1, SR, 0);
    if (t) {
        D3D10_SHADER_RESOURCE_VIEW_DESC d, g; ID3D10ShaderResourceView *v = NULL;
        memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MostDetailedMip = 1; d.Texture2D.MipLevels = (UINT)-1;
        HRESULT hr = ID3D10Device_CreateShaderResourceView(dev, R(t), &d, &v);
        t_note("MipLevels=-1 from mip 1: %s", d3dh_hr_name(hr));
        if (v) { ID3D10ShaderResourceView_GetDesc(v, &g); t_note("reads back MostDetailedMip %u MipLevels %u", g.Texture2D.MostDetailedMip, g.Texture2D.MipLevels); ID3D10ShaderResourceView_Release(v); }
        d.Texture2D.MipLevels = 0; v = NULL;
        hr = ID3D10Device_CreateShaderResourceView(dev, R(t), &d, &v);
        t_note("MipLevels=0: %s", d3dh_hr_name(hr));
        SAFE_RELEASE(v);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_array_validation)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 2, 4, RGBA, 1, SR, 0);
    if (t) {
        static const struct { UINT first, n; int ok; } c[] = { { 0, 4, 1 }, { 3, 1, 1 }, { 1, 2, 1 }, { 0, 5, 0 }, { 4, 1, 0 }, { 2, 3, 0 }, { 0, 0, 0 } };
        for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
            D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = c[i].ok ? NULL : (void *)1;
            memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
            d.Texture2DArray.MipLevels = 2; d.Texture2DArray.FirstArraySlice = c[i].first; d.Texture2DArray.ArraySize = c[i].n;
            HRESULT hr = ID3D10Device_CreateShaderResourceView(dev, R(t), &d, &v);
            t_check(hr == (c[i].ok ? S_OK : E_INVALIDARG), __FILE__, __LINE__, "srv array", "first %u n %u: %s", c[i].first, c[i].n, d3dh_hr_name(hr));
            SAFE_RELEASE(v);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_cube_requires_cube_resource)
{
    DD_DEV(dev);
    ID3D10Texture2D *plain = mk2d(dev, 8, 8, 1, 6, RGBA, 1, SR, 0), *cube = mk2d(dev, 8, 8, 1, 6, RGBA, 1, SR, D3D10_RESOURCE_MISC_TEXTURECUBE);
    D3D10_SHADER_RESOURCE_VIEW_DESC d; ID3D10ShaderResourceView *v = (void *)1;
    memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE; d.TextureCube.MipLevels = 1;
    if (plain) { EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(plain), &d, &v), E_INVALIDARG); }
    if (cube) { v = NULL; EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(cube), &d, &v)); SAFE_RELEASE(v);
        /* a cube texture can also be viewed as an array of 6 faces */
        d.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY; d.Texture2DArray.MipLevels = 1; d.Texture2DArray.ArraySize = 6;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(cube), &d, &v)); SAFE_RELEASE(v); }
    SAFE_RELEASE(plain); SAFE_RELEASE(cube);
    DD_END(dev);
}
T(srv_buffer_requires_desc)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC bd = { 256, DEF, SR, 0, 0 }; ID3D10Buffer *b = NULL; ID3D10ShaderResourceView *v = (void *)1;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &bd, NULL, &b));
    if (b) {
        EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(b), NULL, &v), E_INVALIDARG);
        D3D10_SHADER_RESOURCE_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_FLOAT; d.ViewDimension = D3D10_SRV_DIMENSION_BUFFER; d.Buffer.ElementOffset = 0; d.Buffer.ElementWidth = 64;
        v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(b), &d, &v));
        if (v) { D3D10_SHADER_RESOURCE_VIEW_DESC g; ID3D10ShaderResourceView_GetDesc(v, &g); EXPECT_EQ(g.ViewDimension, D3D10_SRV_DIMENSION_BUFFER); EXPECT_EQ(g.Format, DXGI_FORMAT_R32_FLOAT); EXPECT_EQ(g.Buffer.ElementOffset, 0); EXPECT_EQ(g.Buffer.ElementWidth, 64); ID3D10ShaderResourceView_Release(v); }
        d.Buffer.ElementWidth = 65; v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(b), &d, &v), E_INVALIDARG);
        d.Buffer.ElementOffset = 1; d.Buffer.ElementWidth = 64;
        EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(b), &d, &v), E_INVALIDARG);
        d.Buffer.ElementOffset = 0; d.Buffer.ElementWidth = 0;
        t_note("zero-width buffer SRV: %s", d3dh_hr_name(ID3D10Device_CreateShaderResourceView(dev, R(b), &d, NULL)));
        d.Format = DXGI_FORMAT_UNKNOWN; d.Buffer.ElementWidth = 16; v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(b), &d, &v), E_INVALIDARG);
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; d.Buffer.ElementWidth = 16;
        v = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(b), &d, &v));
        SAFE_RELEASE(v);
        d.Buffer.ElementWidth = 17; v = (void *)1;
        EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(b), &d, &v), E_INVALIDARG);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(srv_buffer_without_bind_flag)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC bd = { 256, DEF, D3D10_BIND_VERTEX_BUFFER, 0, 0 }; ID3D10Buffer *b = NULL; ID3D10ShaderResourceView *v = (void *)1;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &bd, NULL, &b));
    if (b) {
        D3D10_SHADER_RESOURCE_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_FLOAT; d.ViewDimension = D3D10_SRV_DIMENSION_BUFFER; d.Buffer.ElementWidth = 16;
        EXPECT_HR(ID3D10Device_CreateShaderResourceView(dev, R(b), &d, &v), E_INVALIDARG);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(srv_resource_backpointer)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, SR, 0);
    if (t) {
        ID3D10ShaderResourceView *v = NULL; ID3D10Resource *r = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &v));
        if (v) {
            ID3D10ShaderResourceView_GetResource(v, &r); CHECK(r == R(t)); SAFE_RELEASE(r);
            void *p = NULL; HRESULT hr = ID3D10ShaderResourceView_QueryInterface(v, &IID_ID3D10ShaderResourceView1, &p);
            t_note("SRV QI ID3D10ShaderResourceView1: %s", d3dh_hr_name(hr)); if (p) IUnknown_Release((IUnknown *)p);
            ID3D10ShaderResourceView_Release(v);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(srv_unique_objects_per_create)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, SR | RT, 0);
    if (t) {
        ID3D10ShaderResourceView *a = NULL, *b = NULL; ID3D10RenderTargetView *c = NULL, *d = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &a));
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &b));
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &c));
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &d));
        CHECK(a != b); CHECK(c != d);
        SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c); SAFE_RELEASE(d);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(view_refs_keep_resource)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = mk2d(dev, 8, 8, 1, 1, RGBA, 1, SR | RT, 0);
    if (t) {
        ID3D10ShaderResourceView *a = NULL; ID3D10RenderTargetView *c = NULL;
        ULONG base = d3dh_refcount(t);
        EXPECT_HR_OK(ID3D10Device_CreateShaderResourceView(dev, R(t), NULL, &a));
        EXPECT_HR_OK(ID3D10Device_CreateRenderTargetView(dev, R(t), NULL, &c));
        t_note("texture refcount with two views: %lu (base %lu)", d3dh_refcount(t), base);
        CHECK(d3dh_refcount(t) >= base);
        SAFE_RELEASE(a); SAFE_RELEASE(c);
        EXPECT_EQ(d3dh_refcount(t), base);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}

/* ---- D3D10.1: CreateShaderResourceView1 ---- */
T(srv1_cube_array_on_10_1)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_1);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 1, 12, RGBA, { 1, 0 }, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE }; ID3D10Texture2D *t = NULL;
    EXPECT_HR_OK(ID3D10Device1_CreateTexture2D(dev, &td, NULL, &t));
    if (t) {
        D3D10_SHADER_RESOURCE_VIEW_DESC1 d; ID3D10ShaderResourceView1 *v = NULL;
        memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY;
        d.TextureCubeArray.MostDetailedMip = 0; d.TextureCubeArray.MipLevels = 1; d.TextureCubeArray.First2DArrayFace = 0; d.TextureCubeArray.NumCubes = 2;
        EXPECT_HR_OK(ID3D10Device1_CreateShaderResourceView1(dev, (ID3D10Resource *)t, &d, &v));
        if (v) {
            D3D10_SHADER_RESOURCE_VIEW_DESC1 g; memset(&g, 0, sizeof g); ID3D10ShaderResourceView1_GetDesc1(v, &g);
            EXPECT_EQ(g.ViewDimension, D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY); EXPECT_EQ(g.TextureCubeArray.NumCubes, 2); EXPECT_EQ(g.TextureCubeArray.First2DArrayFace, 0);
            D3D10_SHADER_RESOURCE_VIEW_DESC g0; ID3D10ShaderResourceView1_GetDesc(v, &g0);
            t_note("GetDesc on a cube-array view reports dimension %u", g0.ViewDimension);
            ID3D10ShaderResourceView1_Release(v);
        }
        d.TextureCubeArray.NumCubes = 3; d.TextureCubeArray.First2DArrayFace = 0; v = (void *)1;
        EXPECT_HR(ID3D10Device1_CreateShaderResourceView1(dev, (ID3D10Resource *)t, &d, &v), E_INVALIDARG);
        ID3D10Texture2D_Release(t);
    }
    ID3D10Device1_Release(dev);
}
T(srv1_default_desc_matches_srv)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_0);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 3, 1, RGBA, { 1, 0 }, DEF, SR, 0, 0 }; ID3D10Texture2D *t = NULL; ID3D10ShaderResourceView1 *v = NULL;
    EXPECT_HR_OK(ID3D10Device1_CreateTexture2D(dev, &td, NULL, &t));
    if (t) {
        EXPECT_HR_OK(ID3D10Device1_CreateShaderResourceView1(dev, (ID3D10Resource *)t, NULL, &v));
        if (v) {
            D3D10_SHADER_RESOURCE_VIEW_DESC1 g; memset(&g, 0, sizeof g); ID3D10ShaderResourceView1_GetDesc1(v, &g);
            EXPECT_EQ(g.ViewDimension, D3D10_1_SRV_DIMENSION_TEXTURE2D); EXPECT_EQ(g.Texture2D.MipLevels, 3);
            ID3D10ShaderResourceView1_Release(v);
        }
        ID3D10Texture2D_Release(t);
    }
    ID3D10Device1_Release(dev);
}
T(srv1_cube_array_on_10_0_observed)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_0);
    D3D10_TEXTURE2D_DESC td = { 8, 8, 1, 12, RGBA, { 1, 0 }, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE }; ID3D10Texture2D *t = NULL;
    HRESULT hr = ID3D10Device1_CreateTexture2D(dev, &td, NULL, &t);
    t_note("cube array texture on 10_0: %s", d3dh_hr_name(hr));
    if (t) {
        D3D10_SHADER_RESOURCE_VIEW_DESC1 d; ID3D10ShaderResourceView1 *v = NULL;
        memset(&d, 0, sizeof d); d.Format = RGBA; d.ViewDimension = D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY; d.TextureCubeArray.MipLevels = 1; d.TextureCubeArray.NumCubes = 2;
        hr = ID3D10Device1_CreateShaderResourceView1(dev, (ID3D10Resource *)t, &d, &v);
        t_note("cube array SRV1 on 10_0: %s", d3dh_hr_name(hr));
        SAFE_RELEASE(v);
        ID3D10Texture2D_Release(t);
    }
    ID3D10Device1_Release(dev);
}
