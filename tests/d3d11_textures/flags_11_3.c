/* SHARED / KEYEDMUTEX / NTHANDLE / GDI_COMPATIBLE resources, ID3D11Texture2D1 / Texture3D1 (GetDesc1, texture layout), video formats. */
#include "txh.h"

/* ---- sharing flags ---------------------------------------------------------------------------------------------------- */
static void tx_shared_case(tx_ctx *c, UINT misc, const char *file, int line) {
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *tex = NULL, *tex2 = NULL; IDXGIResource *res = NULL; IDXGIResource1 *res1 = NULL; IDXGIKeyedMutex *km = NULL;
    ID3D11Device1 *dev1 = NULL; HANDLE h, handle = NULL; HRESULT hr; int nth = !!(misc & D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
    UINT stype = misc & (M_SHR | M_KEYED);
    int valid = !misc || stype == M_SHR || stype == M_KEYED;
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 256; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 1; d.Usage = U_DEF; d.BindFlags = B_SRV; d.MiscFlags = misc;
    hr = ID3D11Device_QueryInterface(c->dev, &IID_ID3D11Device1, (void **)&dev1);
    if (FAILED(hr)) { t_skip("ID3D11Device1 not available"); return; }
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &tex);
    if (!valid) {
        t_check(hr == E_INVALIDARG, file, line, "invalid sharing flag combination", "misc %#x: %s", misc, d3dh_hr_name(hr));
        SAFE_RELEASE(tex); ID3D11Device1_Release(dev1); return;
    }
    if (FAILED(hr)) { t_note("%s:%d misc %#x not creatable: %s", file, line, misc, d3dh_hr_name(hr)); ID3D11Device1_Release(dev1); return; }
    hr = ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km);
    t_check(hr == ((misc & M_KEYED) ? S_OK : E_NOINTERFACE), file, line, "QI IDXGIKeyedMutex", "misc %#x: %s", misc, d3dh_hr_name(hr));
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res), S_OK);
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource1, (void **)&res1), S_OK);
    if (res) {
        h = (HANDLE)(ULONG_PTR)0xDEADBEEF;
        hr = IDXGIResource_GetSharedHandle(res, &h);
        if (nth) {
            t_check(hr == E_INVALIDARG, file, line, "GetSharedHandle on NT-handle resource", "%s", d3dh_hr_name(hr));
            t_check(h == (HANDLE)(ULONG_PTR)0xDEADBEEF, file, line, "handle untouched", "%p", h);
        } else if (misc) {
            t_check(hr == S_OK, file, line, "GetSharedHandle", "%s", d3dh_hr_name(hr));
            t_check(h != NULL && h != (HANDLE)(ULONG_PTR)0xDEADBEEF, file, line, "legacy shared handle", "%p", h);
            handle = h;
        } else {
            t_check(hr == S_OK, file, line, "GetSharedHandle on unshared resource", "%s", d3dh_hr_name(hr));
            t_check(h == NULL, file, line, "NULL handle for unshared resource", "%p", h);
        }
    }
    if (res1) {
        h = (HANDLE)(ULONG_PTR)0xDEADBEEF;
        hr = IDXGIResource1_CreateSharedHandle(res1, NULL, GENERIC_ALL | DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &h);
        if (nth) { t_check(hr == S_OK, file, line, "CreateSharedHandle", "%s", d3dh_hr_name(hr)); handle = h; }
        else { t_check(hr == E_INVALIDARG, file, line, "CreateSharedHandle on non-NT resource", "%s", d3dh_hr_name(hr)); t_check(h == NULL, file, line, "handle cleared", "%p", h); }
    }
    if (misc && handle) {
        hr = ID3D11Device_OpenSharedResource(c->dev, handle, &IID_ID3D11Texture2D, (void **)&tex2);
        t_check(hr == (nth ? E_INVALIDARG : S_OK), file, line, "OpenSharedResource", "%s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            D3D11_TEXTURE2D_DESC g; ID3D11Texture2D_GetDesc(tex2, &g);
            EXPECT_EQ(g.Width, 256); EXPECT_EQ(g.Height, 256); EXPECT_EQ(g.Format, F_RGBA8);
            t_note("opened shared texture MiscFlags %#x (created with %#x)", g.MiscFlags, misc);
            ID3D11Texture2D_Release(tex2); tex2 = NULL;
        }
        hr = ID3D11Device1_OpenSharedResource1(dev1, handle, &IID_ID3D11Texture2D, (void **)&tex2);
        t_check(hr == (nth ? S_OK : E_INVALIDARG), file, line, "OpenSharedResource1", "%s", d3dh_hr_name(hr));
        SAFE_RELEASE(tex2);
        if (nth) CloseHandle(handle);
    }
    SAFE_RELEASE(km); SAFE_RELEASE(res); SAFE_RELEASE(res1); SAFE_RELEASE(tex);
    ID3D11Device1_Release(dev1);
}
#define SHARED(nm, MISC) DEVTEST(d3d11_textures_shared_##nm) { tx_shared_case(c, MISC, __FILE__, __LINE__); }
#define M_NT D3D11_RESOURCE_MISC_SHARED_NTHANDLE
SHARED(none, 0)
SHARED(shared, M_SHR)
SHARED(nthandle_alone, M_NT)
SHARED(shared_nthandle, M_SHR | M_NT)
SHARED(keyed, M_KEYED)
SHARED(keyed_nthandle, M_KEYED | M_NT)
SHARED(shared_and_keyed, M_SHR | M_KEYED)
SHARED(shared_keyed_nthandle, M_SHR | M_KEYED | M_NT)

DEVTEST(d3d11_textures_shared_open_same_texture_desc) {
    D3D11_TEXTURE2D_DESC d, g; ID3D11Texture2D *tex = NULL, *t2 = NULL; IDXGIResource *res = NULL; HANDLE h = NULL; HRESULT hr;
    memset(&d, 0, sizeof d);
    d.Width = 64; d.Height = 32; d.MipLevels = d.ArraySize = 1; d.Format = F_BGRA8; d.SampleDesc.Count = 1; d.Usage = U_DEF; d.BindFlags = B_SRV | B_RT; d.MiscFlags = M_SHR;
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &tex);
    if (FAILED(hr)) { t_note("shared RT|SRV texture not creatable: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res), S_OK);
    if (res) {
        EXPECT_HR(IDXGIResource_GetSharedHandle(res, &h), S_OK);
        hr = ID3D11Device_OpenSharedResource(c->dev, h, &IID_ID3D11Texture2D, (void **)&t2);
        EXPECT_HR(hr, S_OK);
        if (t2) {
            ID3D11Texture2D_GetDesc(t2, &g);
            EXPECT_EQ(g.Width, 64); EXPECT_EQ(g.Height, 32); EXPECT_EQ(g.Format, F_BGRA8); EXPECT_EQ(g.MipLevels, 1); EXPECT_EQ(g.ArraySize, 1);
            EXPECT_EQ(g.BindFlags & (B_SRV | B_RT), B_SRV | B_RT);
            ID3D11Texture2D_Release(t2);
        }
        IDXGIResource_Release(res);
    }
    ID3D11Texture2D_Release(tex);
}
DEVTEST(d3d11_textures_shared_open_shares_content) {
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *tex = NULL, *t2 = NULL; IDXGIResource *res = NULL; HANDLE h = NULL; HRESULT hr; tx_ctx c2; tx_img im;
    static const float col[4] = {0.2f, 0.4f, 0.6f, 0.8f}; ID3D11RenderTargetView *v = NULL; D3D11_RENDER_TARGET_VIEW_DESC rd;
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 8; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 1; d.Usage = U_DEF; d.BindFlags = B_SRV | B_RT; d.MiscFlags = M_SHR;
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &tex);
    if (FAILED(hr)) { t_note("shared texture not creatable: %s", d3dh_hr_name(hr)); return; }
    memset(&rd, 0, sizeof rd); rd.Format = F_RGBA8; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(c->dev, (ID3D11Resource *)tex, &rd, &v), S_OK);
    if (v) { ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, col); ID3D11DeviceContext_Flush(c->ctx); ID3D11RenderTargetView_Release(v); }
    if (!tx_open(&c2)) { ID3D11Texture2D_Release(tex); return; }
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res), S_OK);
    if (res) {
        EXPECT_HR(IDXGIResource_GetSharedHandle(res, &h), S_OK);
        hr = ID3D11Device_OpenSharedResource(c2.dev, h, &IID_ID3D11Texture2D, (void **)&t2);
        EXPECT_HR(hr, S_OK);
        if (t2) {
            if (SUCCEEDED(tx_fetch(&c2, (ID3D11Resource *)t2, 0, &im))) {
                EXPECT_EQ(tx_px32(&im, 0, 0, 0), 0xcc996633u);   /* bytes 51 102 153 204 */
                tx_img_free(&im);
            }
            ID3D11Texture2D_Release(t2);
        }
        IDXGIResource_Release(res);
    }
    tx_close(&c2);
    ID3D11Texture2D_Release(tex);
}
DEVTEST(d3d11_textures_shared_open_wrong_iid) {
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *tex = NULL; IDXGIResource *res = NULL; HANDLE h = NULL; HRESULT hr; void *p = NULL;
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 8; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 1; d.Usage = U_DEF; d.BindFlags = B_SRV; d.MiscFlags = M_SHR;
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &tex);
    if (FAILED(hr)) { t_note("shared texture not creatable: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res), S_OK);
    if (res) {
        EXPECT_HR(IDXGIResource_GetSharedHandle(res, &h), S_OK);
        hr = ID3D11Device_OpenSharedResource(c->dev, h, &IID_ID3D11Texture2D, &p);
        EXPECT_HR(hr, S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL;
        hr = ID3D11Device_OpenSharedResource(c->dev, h, &IID_ID3D11Resource, &p);
        EXPECT_HR(hr, S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL;
        hr = ID3D11Device_OpenSharedResource(c->dev, h, &IID_ID3D11Buffer, &p);
        EXPECT_HR(hr, E_NOINTERFACE); CHECK(p == NULL); if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
        IDXGIResource_Release(res);
    }
    ID3D11Texture2D_Release(tex);
}
DEVTEST(d3d11_textures_shared_open_null_handle) {
    void *p = (void *)(ULONG_PTR)0xDEADBEEF; HRESULT hr = ID3D11Device_OpenSharedResource(c->dev, NULL, &IID_ID3D11Texture2D, &p);
    EXPECT_HR(hr, E_INVALIDARG);
}

/* ---- IDXGIKeyedMutex semantics ------------------------------------------------------------------------------------------- */
#define DXGI_INVALID_CALL DXGI_ERROR_INVALID_CALL
DEVTEST(d3d11_textures_keyedmutex_single_device_sequence) {
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *tex = NULL; IDXGIKeyedMutex *km = NULL; HRESULT hr;
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 256; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 1; d.Usage = U_DEF; d.BindFlags = B_SRV; d.MiscFlags = M_KEYED;
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &tex);
    if (FAILED(hr)) { t_note("keyed mutex texture not creatable: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km), S_OK);
    if (km) {
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), DXGI_INVALID_CALL);                 /* not acquired */
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 1), DXGI_INVALID_CALL);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 1, 0), WAIT_TIMEOUT);                    /* initial key is 0 */
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), DXGI_INVALID_CALL);              /* already acquired on this device */
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 1, 0), DXGI_INVALID_CALL);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 1), S_OK);                               /* may release to any key */
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), WAIT_TIMEOUT);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 2, 0), WAIT_TIMEOUT);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 1, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
        IDXGIKeyedMutex_Release(km);
    }
    ID3D11Texture2D_Release(tex);
}
DEVTEST(d3d11_textures_keyedmutex_large_keys) {
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *tex = NULL; IDXGIKeyedMutex *km = NULL; HRESULT hr;
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 16; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 1; d.Usage = U_DEF; d.BindFlags = B_SRV; d.MiscFlags = M_KEYED;
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &tex);
    if (FAILED(hr)) { t_note("keyed mutex texture not creatable: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km), S_OK);
    if (km) {
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0xFFFFFFFFFFFFFFFFull), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0xFFFFFFFFFFFFFFFFull, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0x123456789ull), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0x123456789ull, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
        IDXGIKeyedMutex_Release(km);
    }
    ID3D11Texture2D_Release(tex);
}
DEVTEST(d3d11_textures_keyedmutex_identity_with_resource) {
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *tex = NULL; IDXGIKeyedMutex *km = NULL; IDXGIObject *o1 = NULL, *o2 = NULL; IDXGIResource *res = NULL; HRESULT hr;
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 16; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 1; d.Usage = U_DEF; d.BindFlags = B_SRV; d.MiscFlags = M_KEYED;
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &tex);
    if (FAILED(hr)) { t_note("keyed mutex texture not creatable: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km), S_OK);
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res), S_OK);
    if (km && res) {
        EXPECT_HR(IDXGIKeyedMutex_QueryInterface(km, &IID_IDXGIObject, (void **)&o1), S_OK);
        EXPECT_HR(IDXGIResource_QueryInterface(res, &IID_IDXGIObject, (void **)&o2), S_OK);
        EXPECT_EQ(o1, o2);
    }
    SAFE_RELEASE(o1); SAFE_RELEASE(o2); SAFE_RELEASE(km); SAFE_RELEASE(res);
    ID3D11Texture2D_Release(tex);
}
DEVTEST(d3d11_textures_keyedmutex_two_devices) {
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *tex = NULL, *tex2 = NULL; IDXGIKeyedMutex *km = NULL, *km2 = NULL; IDXGIResource *res = NULL; HANDLE h = NULL; HRESULT hr; tx_ctx c2;
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 256; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 1; d.Usage = U_DEF; d.BindFlags = B_SRV; d.MiscFlags = M_KEYED;
    hr = ID3D11Device_CreateTexture2D(c->dev, &d, NULL, &tex);
    if (FAILED(hr)) { t_note("keyed mutex texture not creatable: %s", d3dh_hr_name(hr)); return; }
    if (!tx_open(&c2)) { ID3D11Texture2D_Release(tex); return; }
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIKeyedMutex, (void **)&km), S_OK);
    EXPECT_HR(ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res), S_OK);
    if (res) { EXPECT_HR(IDXGIResource_GetSharedHandle(res, &h), S_OK); IDXGIResource_Release(res); }
    hr = ID3D11Device_OpenSharedResource(c2.dev, h, &IID_ID3D11Texture2D, (void **)&tex2);
    EXPECT_HR(hr, S_OK);
    if (tex2) EXPECT_HR(ID3D11Texture2D_QueryInterface(tex2, &IID_IDXGIKeyedMutex, (void **)&km2), S_OK);
    if (km && km2) {
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), DXGI_INVALID_CALL);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 0, 0), WAIT_TIMEOUT);       /* locked by the other device */
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 1, 0), WAIT_TIMEOUT);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km2, 0), DXGI_INVALID_CALL);     /* not held by this device */
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km2, 1), DXGI_INVALID_CALL);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km2, 0, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km2, 7), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 0, 0), WAIT_TIMEOUT);
        EXPECT_HR(IDXGIKeyedMutex_AcquireSync(km, 7, 0), S_OK);
        EXPECT_HR(IDXGIKeyedMutex_ReleaseSync(km, 0), S_OK);
    }
    SAFE_RELEASE(km); SAFE_RELEASE(km2); SAFE_RELEASE(tex); SAFE_RELEASE(tex2);
    tx_close(&c2);
}

/* ---- GDI compatible surfaces ---------------------------------------------------------------------------------------------- */
DEVTEST(d3d11_textures_gdi_getdc_without_flag_fails) {
    ID3D11Texture2D *t = NULL; IDXGISurface1 *s = NULL; HDC dc = NULL;
    EXPECT_HR(tx_mk2d(c, 64, 64, 1, 1, F_BGRA8, U_DEF, B_RT, 0, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface1, (void **)&s), S_OK);
    if (s) { EXPECT_HR(IDXGISurface1_GetDC(s, FALSE, &dc), DXGI_ERROR_INVALID_CALL); IDXGISurface1_Release(s); }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_gdi_release_without_get_fails) {
    ID3D11Texture2D *t = NULL; IDXGISurface1 *s = NULL;
    EXPECT_HR(tx_mk2d(c, 64, 64, 1, 1, F_BGRA8, U_DEF, B_RT, 0, M_GDI, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface1, (void **)&s), S_OK);
    if (s) { EXPECT_HR(IDXGISurface1_ReleaseDC(s, NULL), DXGI_ERROR_INVALID_CALL); IDXGISurface1_Release(s); }
    ID3D11Texture2D_Release(t);
}
static void tx_gdi_fmt(tx_ctx *c, DXGI_FORMAT f, int supported, const char *file, int line) {
    ID3D11Texture2D *t = NULL; IDXGISurface1 *s = NULL; HDC dc = (HDC)(ULONG_PTR)0x1234; HRESULT hr;
    hr = tx_mk2d(c, 64, 64, 1, 1, f, U_STG, 0, CPU_R, 0, NULL, &t);
    t_check(hr == S_OK, file, line, "staging without GDI flag", "%s", d3dh_hr_name(hr)); SAFE_RELEASE(t);
    hr = tx_mk2d(c, 64, 64, 1, 1, f, U_STG, 0, CPU_R, M_GDI, NULL, &t);
    t_check(FAILED(hr), file, line, "STAGING with GDI_COMPATIBLE", "%s", d3dh_hr_name(hr)); SAFE_RELEASE(t);
    hr = tx_mk2d(c, 64, 64, 1, 1, f, U_DEF, B_RT, 0, M_GDI, NULL, &t);
    if (supported) t_check(hr == S_OK, file, line, "GDI_COMPATIBLE render target", "%s", d3dh_hr_name(hr));
    else { t_check(FAILED(hr), file, line, "GDI_COMPATIBLE unsupported format", "%s", d3dh_hr_name(hr)); SAFE_RELEASE(t); return; }
    if (FAILED(hr)) return;
    hr = ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface1, (void **)&s);
    t_check(hr == S_OK, file, line, "QI IDXGISurface1", "%s", d3dh_hr_name(hr));
    if (s) {
        hr = IDXGISurface1_GetDC(s, FALSE, &dc);
        t_check(hr == S_OK, file, line, "GetDC", "%s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            DIBSECTION dib; HGDIOBJ bmp; int size;
            t_check(GetObjectType(dc) == OBJ_MEMDC, file, line, "DC is a memory DC", "type %lu", (unsigned long)GetObjectType(dc));
            bmp = GetCurrentObject(dc, OBJ_BITMAP);
            t_check(GetObjectType(bmp) == OBJ_BITMAP, file, line, "selected object is a bitmap", "type %lu", (unsigned long)GetObjectType(bmp));
            size = GetObjectA(bmp, sizeof dib, &dib);
            t_check(size == (int)sizeof dib || size == (int)sizeof dib.dsBm, file, line, "GetObject size", "%d", size);
            t_check(dib.dsBm.bmWidth == 64 && dib.dsBm.bmHeight == 64 && dib.dsBm.bmBitsPixel == 32 && dib.dsBm.bmPlanes == 1, file, line, "bitmap geometry", "%dx%d bpp %u planes %u", dib.dsBm.bmWidth, dib.dsBm.bmHeight, dib.dsBm.bmBitsPixel, dib.dsBm.bmPlanes);
            t_check(dib.dsBm.bmWidthBytes == 64 * 4, file, line, "bmWidthBytes", "%d", dib.dsBm.bmWidthBytes);
            hr = IDXGISurface1_ReleaseDC(s, NULL);
            t_check(hr == S_OK, file, line, "ReleaseDC", "%s", d3dh_hr_name(hr));
        }
        IDXGISurface1_Release(s);
    }
    ID3D11Texture2D_Release(t);
}
#define GDIFMT(nm, F, SUP) DEVTEST(d3d11_textures_gdi_format_##nm) { tx_gdi_fmt(c, F, SUP, __FILE__, __LINE__); }
GDIFMT(b8g8r8a8_unorm, DXGI_FORMAT_B8G8R8A8_UNORM, 1)
GDIFMT(b8g8r8a8_typeless, DXGI_FORMAT_B8G8R8A8_TYPELESS, 1)
GDIFMT(b8g8r8a8_srgb, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 1)
GDIFMT(b8g8r8x8_unorm, DXGI_FORMAT_B8G8R8X8_UNORM, 0)
GDIFMT(b8g8r8x8_typeless, DXGI_FORMAT_B8G8R8X8_TYPELESS, 0)
GDIFMT(b8g8r8x8_srgb, DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, 0)

DEVTEST(d3d11_textures_gdi_draw_pixel_visible_in_texture) {
    ID3D11Texture2D *t = NULL; IDXGISurface1 *s = NULL; HDC dc = NULL; ID3D11RenderTargetView *v = NULL; tx_img im; static const float zero[4] = {0, 0, 0, 0};
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_BGRA8, U_DEF, B_RT, 0, M_GDI, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(c->dev, (ID3D11Resource *)t, NULL, &v), S_OK);
    if (v) { ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, zero); ID3D11RenderTargetView_Release(v); }
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface1, (void **)&s), S_OK);
    if (s && SUCCEEDED(IDXGISurface1_GetDC(s, FALSE, &dc))) {
        SetPixel(dc, 3, 5, RGB(255, 128, 0));
        EXPECT_HR(IDXGISurface1_ReleaseDC(s, NULL), S_OK);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) {
            const unsigned char *p = im.data + (5 * im.rowbytes) + 3 * 4;
            CHECKF(p[0] == 0 && p[1] == 128 && p[2] == 255, "pixel bytes B,G,R = %u,%u,%u", p[0], p[1], p[2]);
            CHECKF(im.data[0] == 0 && im.data[1] == 0 && im.data[2] == 0, "untouched pixel changed: %u %u %u", im.data[0], im.data[1], im.data[2]);
            tx_img_free(&im);
        }
    }
    SAFE_RELEASE(s);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_gdi_double_getdc_note) {
    ID3D11Texture2D *t = NULL; IDXGISurface1 *s = NULL; HDC dc = NULL, dc2 = NULL; HRESULT hr;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_BGRA8, U_DEF, B_RT, 0, M_GDI, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface1, (void **)&s), S_OK);
    if (s && SUCCEEDED(IDXGISurface1_GetDC(s, FALSE, &dc))) {
        hr = IDXGISurface1_GetDC(s, FALSE, &dc2);
        EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
        if (SUCCEEDED(hr)) IDXGISurface1_ReleaseDC(s, NULL);
        IDXGISurface1_ReleaseDC(s, NULL);
    }
    SAFE_RELEASE(s);
    ID3D11Texture2D_Release(t);
}

/* ---- ID3D11Texture2D1 / ID3D11Texture3D1 --------------------------------------------------------------------------------------- */
static ID3D11Device3 *tx_dev3(tx_ctx *c) {
    ID3D11Device3 *d3 = NULL;
    if (FAILED(ID3D11Device_QueryInterface(c->dev, &IID_ID3D11Device3, (void **)&d3))) { t_skip("ID3D11Device3 not available"); return NULL; }
    return d3;
}
DEVTEST(d3d11_textures_t2d1_qi_and_desc1) {
    ID3D11Device3 *d3 = tx_dev3(c); ID3D11Texture2D *t = NULL; ID3D11Texture2D1 *t1 = NULL; D3D11_TEXTURE2D_DESC1 d1; D3D11_TEXTURE2D_DESC d;
    if (!d3) return;
    EXPECT_HR(tx_mk2d(c, 12, 9, 0, 2, F_RGBA8, U_DEF, B_SRV | B_RT, 0, 0, NULL, &t), S_OK);
    if (t) {
        EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_ID3D11Texture2D1, (void **)&t1), S_OK);
        if (t1) {
            memset(&d1, 0xCC, sizeof d1);
            ID3D11Texture2D1_GetDesc1(t1, &d1);
            ID3D11Texture2D_GetDesc(t, &d);
            EXPECT_EQ(d1.Width, d.Width); EXPECT_EQ(d1.Height, d.Height); EXPECT_EQ(d1.MipLevels, d.MipLevels); EXPECT_EQ(d1.ArraySize, d.ArraySize);
            EXPECT_EQ(d1.Format, d.Format); EXPECT_EQ(d1.SampleDesc.Count, d.SampleDesc.Count); EXPECT_EQ(d1.SampleDesc.Quality, d.SampleDesc.Quality);
            EXPECT_EQ(d1.Usage, d.Usage); EXPECT_EQ(d1.BindFlags, d.BindFlags); EXPECT_EQ(d1.CPUAccessFlags, d.CPUAccessFlags); EXPECT_EQ(d1.MiscFlags, d.MiscFlags);
            EXPECT_EQ(d1.TextureLayout, D3D11_TEXTURE_LAYOUT_UNDEFINED);
            EXPECT_EQ(d1.MipLevels, 4);
            ID3D11Texture2D1_Release(t1);
        }
        ID3D11Texture2D_Release(t);
    }
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t2d1_base_getdesc_works) {
    ID3D11Device3 *d3 = tx_dev3(c); ID3D11Texture2D1 *t1 = NULL; D3D11_TEXTURE2D_DESC1 d1; D3D11_TEXTURE2D_DESC d; HRESULT hr;
    if (!d3) return;
    memset(&d1, 0, sizeof d1);
    d1.Width = 20; d1.Height = 10; d1.MipLevels = 3; d1.ArraySize = 2; d1.Format = F_BGRA8; d1.SampleDesc.Count = 1; d1.Usage = U_DEF; d1.BindFlags = B_SRV; d1.TextureLayout = D3D11_TEXTURE_LAYOUT_UNDEFINED;
    hr = ID3D11Device3_CreateTexture2D1(d3, &d1, NULL, &t1);
    EXPECT_HR(hr, S_OK);
    if (t1) {
        ID3D11Texture2D1_GetDesc(t1, &d);
        EXPECT_EQ(d.Width, 20); EXPECT_EQ(d.Height, 10); EXPECT_EQ(d.MipLevels, 3); EXPECT_EQ(d.ArraySize, 2); EXPECT_EQ(d.Format, F_BGRA8); EXPECT_EQ(d.BindFlags, B_SRV);
        ID3D11Texture2D1_GetDesc1(t1, &d1);
        EXPECT_EQ(d1.TextureLayout, D3D11_TEXTURE_LAYOUT_UNDEFINED);
        ID3D11Texture2D1_Release(t1);
    }
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t2d1_create_validate_only) {
    ID3D11Device3 *d3 = tx_dev3(c); D3D11_TEXTURE2D_DESC1 d1;
    if (!d3) return;
    memset(&d1, 0, sizeof d1);
    d1.Width = d1.Height = 16; d1.MipLevels = d1.ArraySize = 1; d1.Format = F_RGBA8; d1.SampleDesc.Count = 1; d1.Usage = U_DEF; d1.BindFlags = B_SRV;
    EXPECT_HR(ID3D11Device3_CreateTexture2D1(d3, &d1, NULL, NULL), S_FALSE);
    d1.Width = 0;
    EXPECT_HR(ID3D11Device3_CreateTexture2D1(d3, &d1, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device3_CreateTexture2D1(d3, NULL, NULL, NULL), E_INVALIDARG);
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t2d1_same_validity_as_t2d) {
    ID3D11Device3 *d3 = tx_dev3(c); D3D11_TEXTURE2D_DESC1 d1; ID3D11Texture2D1 *t = NULL;
    if (!d3) return;
    memset(&d1, 0, sizeof d1);
    d1.Width = d1.Height = 16; d1.MipLevels = 9; d1.ArraySize = 1; d1.Format = F_RGBA8; d1.SampleDesc.Count = 1; d1.Usage = U_DEF; d1.BindFlags = B_SRV;
    EXPECT_HR(ID3D11Device3_CreateTexture2D1(d3, &d1, NULL, &t), E_INVALIDARG);    /* too many mips */
    SAFE_RELEASE(t);
    d1.MipLevels = 1; d1.Usage = U_IMM;
    EXPECT_HR(ID3D11Device3_CreateTexture2D1(d3, &d1, NULL, &t), E_INVALIDARG);    /* immutable without data */
    SAFE_RELEASE(t);
    d1.Usage = U_DEF; d1.BindFlags = B_DS;
    EXPECT_HR(ID3D11Device3_CreateTexture2D1(d3, &d1, NULL, &t), E_INVALIDARG);    /* RGBA8 cannot be a depth buffer */
    SAFE_RELEASE(t);
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t2d1_initial_data) {
    ID3D11Device3 *d3 = tx_dev3(c); D3D11_TEXTURE2D_DESC1 d1; ID3D11Texture2D1 *t = NULL; tx_init in; tx_img im; size_t bad;
    if (!d3) return;
    CHECK(tx_init_make(&in, 6, 5, 1, 1, 1, F_RGBA8, 4, 0, 33));
    memset(&d1, 0, sizeof d1);
    d1.Width = 6; d1.Height = 5; d1.MipLevels = d1.ArraySize = 1; d1.Format = F_RGBA8; d1.SampleDesc.Count = 1; d1.Usage = U_IMM; d1.BindFlags = B_SRV;
    EXPECT_HR(ID3D11Device3_CreateTexture2D1(d3, &d1, in.sd, &t), S_OK);
    if (t) {
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { CHECK(tx_img_is_pat(&im, 33, &bad)); tx_img_free(&im); }
        ID3D11Texture2D1_Release(t);
    }
    tx_init_free(&in);
    ID3D11Device3_Release(d3);
}
static void tx_layout_note(tx_ctx *c, D3D11_TEXTURE_LAYOUT layout, D3D11_USAGE u, UINT bind, UINT cpu, HRESULT want_validate, HRESULT want_create) {
    ID3D11Device3 *d3 = tx_dev3(c); D3D11_TEXTURE2D_DESC1 d1; ID3D11Texture2D1 *t = NULL; HRESULT hr, hv;
    if (!d3) return;
    memset(&d1, 0, sizeof d1);
    d1.Width = d1.Height = 64; d1.MipLevels = d1.ArraySize = 1; d1.Format = F_RGBA8; d1.SampleDesc.Count = 1; d1.Usage = u; d1.BindFlags = bind; d1.CPUAccessFlags = cpu; d1.TextureLayout = layout;
    hv = ID3D11Device3_CreateTexture2D1(d3, &d1, NULL, NULL);
    hr = ID3D11Device3_CreateTexture2D1(d3, &d1, NULL, &t);
    t_check(hv == want_validate && hr == want_create, __FILE__, __LINE__, "texture layout", "layout %d usage %d bind %#x cpu %#x: validate %s create %s (want %s / %s)", layout, u, bind, cpu, d3dh_hr_name(hv), d3dh_hr_name(hr), d3dh_hr_name(want_validate), d3dh_hr_name(want_create));
    if (SUCCEEDED(hr) && t) {
        D3D11_TEXTURE2D_DESC1 g; ID3D11Texture2D1_GetDesc1(t, &g);
        CHECKF(g.TextureLayout == layout, "created layout %d reported as %d", layout, g.TextureLayout);
        ID3D11Texture2D1_Release(t);
    }
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t2d1_layout_row_major_note) { tx_layout_note(c, D3D11_TEXTURE_LAYOUT_ROW_MAJOR, U_DEF, B_SRV, 0, E_INVALIDARG, E_INVALIDARG); }
DEVTEST(d3d11_textures_t2d1_layout_row_major_staging_note) { tx_layout_note(c, D3D11_TEXTURE_LAYOUT_ROW_MAJOR, U_STG, 0, CPU_R | CPU_W, S_FALSE, S_OK); }
DEVTEST(d3d11_textures_t2d1_layout_undefined_swizzle_note) { tx_layout_note(c, D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE, U_DEF, B_SRV, 0, E_INVALIDARG, E_INVALIDARG); }
DEVTEST(d3d11_textures_t2d1_layout_standard_swizzle_note) { tx_layout_note(c, D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE, U_DEF, B_SRV | B_RT, 0, E_INVALIDARG, E_INVALIDARG); }
DEVTEST(d3d11_textures_t2d1_layout_out_of_range_note) { tx_layout_note(c, (D3D11_TEXTURE_LAYOUT)9, U_DEF, B_SRV, 0, E_INVALIDARG, E_INVALIDARG); }
DEVTEST(d3d11_textures_t2d1_layout_undefined_ok) { tx_layout_note(c, D3D11_TEXTURE_LAYOUT_UNDEFINED, U_DEF, B_SRV, 0, S_FALSE, S_OK); }

DEVTEST(d3d11_textures_t3d1_qi_and_desc1) {
    ID3D11Device3 *d3 = tx_dev3(c); ID3D11Texture3D *t = NULL; ID3D11Texture3D1 *t1 = NULL; D3D11_TEXTURE3D_DESC1 d1; D3D11_TEXTURE3D_DESC d;
    if (!d3) return;
    EXPECT_HR(tx_mk3d(c, 8, 4, 2, 0, F_RGBA8, U_DEF, B_SRV | B_RT, 0, 0, NULL, &t), S_OK);
    if (t) {
        EXPECT_HR(ID3D11Texture3D_QueryInterface(t, &IID_ID3D11Texture3D1, (void **)&t1), S_OK);
        if (t1) {
            memset(&d1, 0xCC, sizeof d1);
            ID3D11Texture3D1_GetDesc1(t1, &d1);
            ID3D11Texture3D_GetDesc(t, &d);
            EXPECT_EQ(d1.Width, d.Width); EXPECT_EQ(d1.Height, d.Height); EXPECT_EQ(d1.Depth, d.Depth); EXPECT_EQ(d1.MipLevels, d.MipLevels);
            EXPECT_EQ(d1.Format, d.Format); EXPECT_EQ(d1.Usage, d.Usage); EXPECT_EQ(d1.BindFlags, d.BindFlags); EXPECT_EQ(d1.CPUAccessFlags, d.CPUAccessFlags); EXPECT_EQ(d1.MiscFlags, d.MiscFlags);
            EXPECT_EQ(d1.TextureLayout, D3D11_TEXTURE_LAYOUT_UNDEFINED);
            EXPECT_EQ(d1.MipLevels, 4);
            ID3D11Texture3D1_Release(t1);
        }
        ID3D11Texture3D_Release(t);
    }
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t3d1_create_and_validate) {
    ID3D11Device3 *d3 = tx_dev3(c); D3D11_TEXTURE3D_DESC1 d1; ID3D11Texture3D1 *t = NULL; D3D11_TEXTURE3D_DESC d;
    if (!d3) return;
    memset(&d1, 0, sizeof d1);
    d1.Width = 8; d1.Height = 8; d1.Depth = 8; d1.MipLevels = 0; d1.Format = F_RGBA8; d1.Usage = U_DEF; d1.BindFlags = B_SRV;
    EXPECT_HR(ID3D11Device3_CreateTexture3D1(d3, &d1, NULL, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device3_CreateTexture3D1(d3, &d1, NULL, &t), S_OK);
    if (t) { ID3D11Texture3D1_GetDesc(t, &d); EXPECT_EQ(d.MipLevels, 4); ID3D11Texture3D1_Release(t); t = NULL; }
    d1.Depth = 0;
    EXPECT_HR(ID3D11Device3_CreateTexture3D1(d3, &d1, NULL, &t), E_INVALIDARG);
    SAFE_RELEASE(t);
    EXPECT_HR(ID3D11Device3_CreateTexture3D1(d3, NULL, NULL, NULL), E_INVALIDARG);
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t3d1_layout_row_major_note) {
    ID3D11Device3 *d3 = tx_dev3(c); D3D11_TEXTURE3D_DESC1 d1; HRESULT hv;
    if (!d3) return;
    memset(&d1, 0, sizeof d1);
    d1.Width = d1.Height = d1.Depth = 8; d1.MipLevels = 1; d1.Format = F_RGBA8; d1.Usage = U_DEF; d1.BindFlags = B_SRV; d1.TextureLayout = D3D11_TEXTURE_LAYOUT_ROW_MAJOR;
    hv = ID3D11Device3_CreateTexture3D1(d3, &d1, NULL, NULL);
    EXPECT_HR(hv, E_INVALIDARG);
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t1d_has_no_desc1_interface) {
    ID3D11Texture1D *t = NULL; void *p = NULL; ID3D11Device3 *d3 = tx_dev3(c);
    if (!d3) return;
    EXPECT_HR(tx_mk1d(c, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (t) { EXPECT_HR(ID3D11Texture1D_QueryInterface(t, &IID_ID3D11Texture2D1, &p), E_NOINTERFACE); EXPECT_HR(ID3D11Texture1D_QueryInterface(t, &IID_ID3D11Texture3D1, &p), E_NOINTERFACE); ID3D11Texture1D_Release(t); }
    ID3D11Device3_Release(d3);
}
DEVTEST(d3d11_textures_t2d1_not_available_for_3d) {
    ID3D11Texture3D *t = NULL; void *p = NULL; ID3D11Device3 *d3 = tx_dev3(c);
    if (!d3) return;
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (t) { EXPECT_HR(ID3D11Texture3D_QueryInterface(t, &IID_ID3D11Texture2D1, &p), E_NOINTERFACE); ID3D11Texture3D_Release(t); }
    ID3D11Device3_Release(d3);
}

/* ---- video / planar formats ------------------------------------------------------------------------------------------------------- */
static int tx_video_supported(tx_ctx *c, DXGI_FORMAT f) {
    UINT s = 0;
    if (FAILED(ID3D11Device_CheckFormatSupport(c->dev, f, &s))) return 0;
    return !!(s & D3D11_FORMAT_SUPPORT_TEXTURE2D);
}
static void tx_video_dims(tx_ctx *c, DXGI_FORMAT f, UINT w, UINT h, int want_ok, const char *file, int line) {
    ID3D11Texture2D *t = NULL; HRESULT hr;
    if (!tx_video_supported(c, f)) {   /* WARP: P010/P016/Y210... have no CheckFormatSupport answer: every size is E_INVALIDARG */
        hr = tx_mk2d(c, w, h, 1, 1, f, U_DEF, B_SRV, 0, 0, NULL, &t);
        t_check(hr == E_INVALIDARG && !t, file, line, "unsupported video format", "format %u %ux%u -> %s", f, w, h, d3dh_hr_name(hr));
        return;
    }
    hr = tx_mk2d(c, 16, 16, 1, 1, f, U_DEF, B_SRV, 0, 0, NULL, &t);
    if (FAILED(hr)) { t_note("%s:%d baseline 16x16 SRV texture of format %u not creatable: %s", file, line, f, d3dh_hr_name(hr)); return; }
    ID3D11Texture2D_Release(t); t = NULL;
    hr = tx_mk2d(c, w, h, 1, 1, f, U_DEF, B_SRV, 0, 0, NULL, &t);
    t_check((hr == S_OK) == !!want_ok, file, line, "planar/subsampled format dimensions", "format %u %ux%u -> %s", f, w, h, d3dh_hr_name(hr));
    SAFE_RELEASE(t);
}
#define VDIM(nm, F, W, H, OK) DEVTEST(d3d11_textures_video_dims_##nm) { tx_video_dims(c, F, W, H, OK, __FILE__, __LINE__); }
VDIM(nv12_16x16, DXGI_FORMAT_NV12, 16, 16, 1)
VDIM(nv12_2x2, DXGI_FORMAT_NV12, 2, 2, 1)
VDIM(nv12_18x10, DXGI_FORMAT_NV12, 18, 10, 1)
VDIM(nv12_odd_width, DXGI_FORMAT_NV12, 15, 16, 0)
VDIM(nv12_odd_height, DXGI_FORMAT_NV12, 16, 15, 0)
VDIM(nv12_odd_both, DXGI_FORMAT_NV12, 3, 3, 0)
VDIM(nv12_1x1, DXGI_FORMAT_NV12, 1, 1, 0)
VDIM(p010_16x16, DXGI_FORMAT_P010, 16, 16, 1)
VDIM(p010_odd_width, DXGI_FORMAT_P010, 15, 16, 0)
VDIM(p010_odd_height, DXGI_FORMAT_P010, 16, 15, 0)
VDIM(p016_16x16, DXGI_FORMAT_P016, 16, 16, 1)
VDIM(p016_odd, DXGI_FORMAT_P016, 17, 17, 0)
VDIM(yuy2_16x16, DXGI_FORMAT_YUY2, 16, 16, 1)
VDIM(yuy2_odd_width, DXGI_FORMAT_YUY2, 15, 16, 0)
VDIM(y210_16x16, DXGI_FORMAT_Y210, 16, 16, 1)
VDIM(y210_odd_width, DXGI_FORMAT_Y210, 15, 16, 0)

DEVTEST(d3d11_textures_video_nv12_staging_pitches) {
    ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE m; HRESULT hr;
    if (!tx_video_supported(c, DXGI_FORMAT_NV12)) { t_skip("NV12 textures not supported"); return; }
    hr = tx_mk2d(c, 18, 10, 1, 1, DXGI_FORMAT_NV12, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t);
    if (FAILED(hr)) { t_note("NV12 staging not creatable: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ_WRITE, 0, &m), S_OK);
    CHECK(m.pData != NULL);
    CHECKF(m.RowPitch >= 18, "RowPitch %u", m.RowPitch);
    CHECKF(m.DepthPitch >= (UINT64)m.RowPitch * 10, "DepthPitch %u RowPitch %u", m.DepthPitch, m.RowPitch);
    EXPECT_EQ(m.RowPitch, 20); EXPECT_EQ(m.DepthPitch, 212);   /* WARP: 4-byte aligned rows; luma + chroma plane layout */
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_video_nv12_staging_roundtrip) {
    ID3D11Texture2D *t = NULL, *t2 = NULL; D3D11_MAPPED_SUBRESOURCE m; unsigned char *p; UINT x, y, rp = 0;
    if (!tx_video_supported(c, DXGI_FORMAT_NV12)) { t_skip("NV12 textures not supported"); return; }
    if (FAILED(tx_mk2d(c, 16, 8, 1, 1, DXGI_FORMAT_NV12, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t))) { t_note("NV12 staging not creatable"); return; }
    if (FAILED(tx_mk2d(c, 16, 8, 1, 1, DXGI_FORMAT_NV12, U_DEF, B_SRV, 0, 0, NULL, &t2))) { t_note("NV12 default not creatable"); ID3D11Texture2D_Release(t); return; }
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_WRITE, 0, &m), S_OK);
    p = m.pData; rp = m.RowPitch;
    for (y = 0; y < 8; y++) for (x = 0; x < 16; x++) p[y * rp + x] = (unsigned char)(x * 9 + y * 5 + 1);
    for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) p[(8 + y) * rp + x] = (unsigned char)(x * 3 + y * 17 + 100);
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource *)t2, (ID3D11Resource *)t);
    ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource *)t, (ID3D11Resource *)t2);
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &m), S_OK);
    p = m.pData;
    { int bad = 0; for (y = 0; y < 8; y++) for (x = 0; x < 16; x++) if (p[y * m.RowPitch + x] != (unsigned char)(x * 9 + y * 5 + 1)) bad++;
      for (y = 0; y < 4; y++) for (x = 0; x < 16; x++) if (p[(8 + y) * m.RowPitch + x] != (unsigned char)(x * 3 + y * 17 + 100)) bad++;
      CHECKF(!bad, "%d NV12 bytes differ after the copy round trip", bad); }
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11Texture2D_Release(t2); ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_video_nv12_view_formats) {
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *v = NULL; D3D11_SHADER_RESOURCE_VIEW_DESC sd; HRESULT hr;
    if (!tx_video_supported(c, DXGI_FORMAT_NV12)) { t_skip("NV12 textures not supported"); return; }
    hr = tx_mk2d(c, 16, 16, 1, 1, DXGI_FORMAT_NV12, U_DEF, B_SRV, 0, 0, NULL, &t);
    if (FAILED(hr)) { t_note("NV12 SRV texture not creatable: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, NULL, &v), E_INVALIDARG);
    SAFE_RELEASE(v);
    memset(&sd, 0, sizeof sd); sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
    sd.Format = DXGI_FORMAT_NV12;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, &sd, &v), E_INVALIDARG);
    SAFE_RELEASE(v);
    sd.Format = DXGI_FORMAT_R8_UNORM;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, &sd, &v), S_OK);
    SAFE_RELEASE(v);
    sd.Format = DXGI_FORMAT_R8G8_UNORM;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, &sd, &v), S_OK);
    SAFE_RELEASE(v);
    sd.Format = F_RGBA8;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, &sd, &v), E_INVALIDARG);
    SAFE_RELEASE(v);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_video_nv12_copy_to_r8_plane_note) {
    ID3D11Texture2D *t = NULL, *r8 = NULL;
    if (!tx_video_supported(c, DXGI_FORMAT_NV12)) { t_skip("NV12 textures not supported"); return; }
    if (FAILED(tx_mk2d(c, 16, 16, 1, 1, DXGI_FORMAT_NV12, U_DEF, B_SRV, 0, 0, NULL, &t))) { t_note("NV12 not creatable"); return; }
    t_note("R8 24-row texture (luma + chroma rows) creation: %s", d3dh_hr_name(tx_mk2d(c, 16, 24, 1, 1, F_R8, U_DEF, B_UAV, 0, 0, NULL, &r8)));
    SAFE_RELEASE(r8);
    ID3D11Texture2D_Release(t);
}
#define VNOTE(nm, F) DEVTEST(d3d11_textures_video_note_##nm) { \
    UINT s = 0; HRESULT hr = ID3D11Device_CheckFormatSupport(c->dev, F, &s); ID3D11Texture2D *t = NULL; \
    t_note("format %u CheckFormatSupport %s support %#x", F, d3dh_hr_name(hr), s); \
    t_note("format %u 16x16 DEFAULT SRV: %s", F, d3dh_hr_name(tx_mk2d(c, 16, 16, 1, 1, F, U_DEF, B_SRV, 0, 0, NULL, &t))); SAFE_RELEASE(t); \
    t_note("format %u 16x16 DEFAULT SRV|RT: %s", F, d3dh_hr_name(tx_mk2d(c, 16, 16, 1, 1, F, U_DEF, B_SRV | B_RT, 0, 0, NULL, &t))); SAFE_RELEASE(t); \
    t_note("format %u 16x16 x2 array: %s", F, d3dh_hr_name(tx_mk2d(c, 16, 16, 1, 2, F, U_DEF, B_SRV, 0, 0, NULL, &t))); SAFE_RELEASE(t); \
    t_note("format %u 16x16 3 mips: %s", F, d3dh_hr_name(tx_mk2d(c, 16, 16, 3, 1, F, U_DEF, B_SRV, 0, 0, NULL, &t))); SAFE_RELEASE(t); \
    t_note("format %u 16x16 STAGING CPU_R: %s", F, d3dh_hr_name(tx_mk2d(c, 16, 16, 1, 1, F, U_STG, 0, CPU_R, 0, NULL, &t))); SAFE_RELEASE(t); \
    t_note("format %u 16x16 DECODER bind: %s", F, d3dh_hr_name(tx_mk2d(c, 16, 16, 1, 1, F, U_DEF, D3D11_BIND_DECODER, 0, 0, NULL, &t))); SAFE_RELEASE(t); }
VNOTE(nv12, DXGI_FORMAT_NV12)
VNOTE(p010, DXGI_FORMAT_P010)
VNOTE(p016, DXGI_FORMAT_P016)
VNOTE(yuy2, DXGI_FORMAT_YUY2)
VNOTE(y210, DXGI_FORMAT_Y210)
VNOTE(y216, DXGI_FORMAT_Y216)
VNOTE(y410, DXGI_FORMAT_Y410)
VNOTE(y416, DXGI_FORMAT_Y416)
VNOTE(ayuv, DXGI_FORMAT_AYUV)
VNOTE(nv11, DXGI_FORMAT_NV11)
VNOTE(opaque420, DXGI_FORMAT_420_OPAQUE)
VNOTE(ayuv_ia44, DXGI_FORMAT_IA44)
VNOTE(p8, DXGI_FORMAT_P8)
VNOTE(a8p8, DXGI_FORMAT_A8P8)
VNOTE(b4g4r4a4, DXGI_FORMAT_B4G4R4A4_UNORM)
VNOTE(b5g6r5, DXGI_FORMAT_B5G6R5_UNORM)
VNOTE(b5g5r5a1, DXGI_FORMAT_B5G5R5A1_UNORM)
VNOTE(r1_unorm, DXGI_FORMAT_R1_UNORM)
VNOTE(a4b4g4r4, (DXGI_FORMAT)191)
