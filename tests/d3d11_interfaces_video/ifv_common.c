#include "ifv.h"
#include "ifv_qi_pins.h"

static int g_brd;   /* last device created by ifv_dev/ifv_dev_at runs on the Basic Render Driver */

const GUID ifv_IID_VideoDevice2  = {0x59c0cb01, 0x35f0, 0x4a70, {0x8f, 0x67, 0x87, 0x90, 0x5c, 0x90, 0x6a, 0x53}};
const GUID ifv_IID_VideoContext3 = {0xa9e2faa0, 0xcb39, 0x418f, {0xa0, 0xb7, 0xd8, 0xaa, 0xd4, 0xde, 0x67, 0x2e}};
const GUID ifv_IID_Random = {0x6f1d2c8a, 0x3b47, 0x4d95, {0xa0, 0x12, 0x9e, 0xc4, 0x5b, 0x7a, 0x18, 0xe3}};

int ifv_dev(ID3D11Device **d, ID3D11DeviceContext **c, D3D_FEATURE_LEVEL *fl, UINT flags) {
    D3D_FEATURE_LEVEL f = 0;
    HRESULT hr = d3dh_make_device11(d, c, &f, NULL, flags);
    if (FAILED(hr) || !*d || !*c) { t_skip("cannot create a D3D11 device (%s)", d3dh_hr_name(hr)); return 0; }
    g_brd = ifv_is_brd(*d);
    if (fl) *fl = f;
    return 1;
}

int ifv_dev_at(ID3D11Device **d, ID3D11DeviceContext **c, D3D_FEATURE_LEVEL want, UINT flags) {
    D3D_FEATURE_LEVEL got = 0;
    HRESULT hr = d3dh_make_device11_fl(d, c, &want, 1, &got, NULL, flags);
    if (FAILED(hr) || !*d || !*c) return 0;
    if (got != want) { SAFE_RELEASE(*c); SAFE_RELEASE(*d); return 0; }
    g_brd = ifv_is_brd(*d);
    return 1;
}

void *ifv_identity(void *unk) {
    IUnknown *u = NULL, *o = (IUnknown *)unk;
    if (!o) return NULL;
    if (FAILED(o->lpVtbl->QueryInterface(o, &IID_IUnknown, (void **)&u)) || !u) return NULL;
    u->lpVtbl->Release(u);
    return u;
}

HRESULT ifv_qi(void *unk, const GUID *iid, void **ident_out) {
    IUnknown *o = (IUnknown *)unk, *r = (IUnknown *)(ULONG_PTR)0x1;
    HRESULT hr = o->lpVtbl->QueryInterface(o, iid, (void **)&r);
    if (ident_out) *ident_out = NULL;
    if (FAILED(hr)) {
        CHECKF(r == NULL, "QI failure left *ppv = %p for %s", (void *)r, d3dh_guid_str(iid));
        return hr;
    }
    if (ident_out) *ident_out = ifv_identity(r);
    r->lpVtbl->Release(r);
    return hr;
}

void ifv_check_qi_table(void *obj, const struct ifv_qi_ent *tab, unsigned n, const char *what) {
    for (unsigned i = 0; i < n; i++) {
        void *ident = NULL;
        HRESULT hr = ifv_qi(obj, tab[i].iid, &ident);
        if (tab[i].want == 1) {
            t_check(hr == S_OK, __FILE__, __LINE__, "QI succeeds", "%s %s: got %s", what, tab[i].name, d3dh_hr_name(hr));
            if (hr == S_OK) t_check(ident == ifv_identity(obj), __FILE__, __LINE__, "QI identity", "%s %s: different IUnknown identity", what, tab[i].name);
        } else if (tab[i].want == 0) {
            t_check(hr == E_NOINTERFACE, __FILE__, __LINE__, "QI E_NOINTERFACE", "%s %s: got %s", what, tab[i].name, d3dh_hr_name(hr));
        } else {
            unsigned k, pinned = 0;
            t_note("%s QI %s -> %s", what, tab[i].name, d3dh_hr_name(hr));
            if (g_brd) for (k = 0; k < sizeof(k_qi_pins) / sizeof(k_qi_pins[0]); k++)
                if (!strcmp(k_qi_pins[k].what, what) && !strcmp(k_qi_pins[k].name, tab[i].name)) {
                    pinned = 1;
                    t_check(hr == k_qi_pins[k].hr, __FILE__, __LINE__, "QI pinned", "%s QI %s: got %s want %s", what, tab[i].name, d3dh_hr_name(hr), d3dh_hr_name(k_qi_pins[k].hr));
                }
            (void)pinned;
        }
    }
}

int ifv_video_open(struct ifv_video *v) {
    memset(v, 0, sizeof(*v));
    if (!ifv_dev(&v->dev, &v->ctx, &v->fl, 0)) return 0;
    HRESULT h1 = ID3D11Device_QueryInterface(v->dev, &IID_ID3D11VideoDevice, (void **)&v->vd);
    HRESULT h2 = ID3D11DeviceContext_QueryInterface(v->ctx, &IID_ID3D11VideoContext, (void **)&v->vc);
    if (FAILED(h1) || FAILED(h2)) {
        t_skip("no video interfaces on this device: QI ID3D11VideoDevice %s, ID3D11VideoContext %s, Basic Render Driver %d",
               d3dh_hr_name(h1), d3dh_hr_name(h2), ifv_is_brd(v->dev));
        ifv_video_close(v);
        return 0;
    }
    return 1;
}

void ifv_video_close(struct ifv_video *v) {
    SAFE_RELEASE(v->vc); SAFE_RELEASE(v->vd); SAFE_RELEASE(v->ctx); SAFE_RELEASE(v->dev);
}

void ifv_content_desc(D3D11_VIDEO_PROCESSOR_CONTENT_DESC *d, UINT iw, UINT ih, UINT ow, UINT oh) {
    memset(d, 0, sizeof(*d));
    d->InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    d->InputFrameRate.Numerator = 30; d->InputFrameRate.Denominator = 1;
    d->InputWidth = iw; d->InputHeight = ih;
    d->OutputFrameRate.Numerator = 30; d->OutputFrameRate.Denominator = 1;
    d->OutputWidth = ow; d->OutputHeight = oh;
    d->Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
}

HRESULT ifv_make_enum(struct ifv_video *v, UINT w, UINT h, ID3D11VideoProcessorEnumerator **e) {
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC d;
    ifv_content_desc(&d, w, h, w, h);
    *e = NULL;
    return ID3D11VideoDevice_CreateVideoProcessorEnumerator(v->vd, &d, e);
}

int ifv_make_vp(struct ifv_video *v, UINT w, UINT h, ID3D11VideoProcessorEnumerator **e, ID3D11VideoProcessor **p) {
    *p = NULL;
    HRESULT hr = ifv_make_enum(v, w, h, e);
    if (FAILED(hr) || !*e) { t_skip("CreateVideoProcessorEnumerator unsupported (%s)", d3dh_hr_name(hr)); return 0; }
    hr = ID3D11VideoDevice_CreateVideoProcessor(v->vd, *e, 0, p);
    if (FAILED(hr) || !*p) { t_skip("CreateVideoProcessor unsupported (%s)", d3dh_hr_name(hr)); SAFE_RELEASE(*e); return 0; }
    return 1;
}

int ifv_brd(void) { return g_brd; }

int ifv_is_brd(ID3D11Device *dev)
{
    IDXGIDevice *dd = NULL;
    IDXGIAdapter *ad = NULL;
    DXGI_ADAPTER_DESC desc;
    int r = 0;
    if (FAILED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd))) return 0;
    if (SUCCEEDED(IDXGIDevice_GetAdapter(dd, &ad)) && SUCCEEDED(IDXGIAdapter_GetDesc(ad, &desc))) r = desc.VendorId == 0x1414;
    if (ad) IDXGIAdapter_Release(ad);
    IDXGIDevice_Release(dd);
    return r;
}
