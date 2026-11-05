#include "dxm.h"
/* DXVA2CreateVideoService and the decoder / processor / acceleration service interfaces. */

#define E_INVALIDARG_ ((HRESULT)0x80070057)
#define E_NOINTERFACE_ ((HRESULT)0x80004002)
#define E_FAIL_ ((HRESULT)0x80004005)

typedef HRESULT (WINAPI *create_svc_fn)(IDirect3DDevice9 *, REFIID, void **);
static create_svc_fn svc_create(void) { return (create_svc_fn)t_proc_in("dxva2.dll", "DXVA2CreateVideoService"); }

#define UNK(p) ((IUnknown *)(p))
#define REL(p) do { if (p) UNK(p)->lpVtbl->Release(UNK(p)); } while (0)

/* Fixture: device + one service interface of the requested kind. */
typedef struct { DXMD9 d; IDirectXVideoDecoderService *dec; IDirectXVideoProcessorService *proc; } SVC;
static int svc_open(SVC *s, int want_dec, int want_proc)
{
    memset(s, 0, sizeof *s);
    create_svc_fn c = svc_create();
    if (!c) { t_skip("DXVA2CreateVideoService missing"); return 0; }
    if (!dxm_d9_open(&s->d)) return 0;
    if (want_dec && (FAILED(c(s->d.dev, &DXM_IID_DecSvc, (void **)&s->dec)) || !s->dec)) {
        t_skip("decoder service unavailable on this device"); dxm_d9_close(&s->d); return 0; }
    if (want_proc && (FAILED(c(s->d.dev, &DXM_IID_ProcSvc, (void **)&s->proc)) || !s->proc)) {
        t_skip("processor service unavailable on this device"); REL(s->dec); dxm_d9_close(&s->d); return 0; }
    return 1;
}
static void svc_close(SVC *s) { REL(s->dec); REL(s->proc); dxm_d9_close(&s->d); }
#define DEC_OPEN(s) SVC s; if (!svc_open(&s, 1, 0)) return
#define PROC_OPEN(s) SVC s; if (!svc_open(&s, 0, 1)) return

/* ---------------- creation and QueryInterface ---------------- */
TEST(dxva2_monitor_video_svc_create_each_interface) {
    create_svc_fn c = svc_create(); REQUIRE(c);
    DXM_D9(d);
    const GUID *iids[] = { &DXM_IID_Accel, &DXM_IID_DecSvc, &DXM_IID_ProcSvc };
    for (int i = 0; i < 3; i++) {
        void *p = NULL;
        HRESULT hr = c(d.dev, iids[i], &p);
        if (FAILED(hr)) { t_note("iid %d: create -> %s", i, d3dh_hr_name(hr)); continue; }
        CHECK(p != NULL);
        EXPECT_EQ(UNK(p)->lpVtbl->Release(UNK(p)), 0);
    }
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_svc_create_unknown_iid) {
    create_svc_fn c = svc_create(); REQUIRE(c);
    DXM_D9(d);
    ULONG rc = d3dh_refcount(d.dev);
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = c(d.dev, &DXM_IID_Bogus, &p);
    CHECK(FAILED(hr));
    if (hr != E_NOINTERFACE_) t_note("bogus IID -> %s (device may not support DXVA2 at all)", d3dh_hr_name(hr));
    else CHECK(p == NULL);
    EXPECT_EQ(d3dh_refcount(d.dev), rc);   /* the temporary service is destroyed again */
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_svc_create_wrong_service_iid) {
    create_svc_fn c = svc_create(); REQUIRE(c);
    DXM_D9(d);
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = c(d.dev, &DXM_IID_DevMgr9, &p);   /* the manager interface is not a service interface */
    CHECK(FAILED(hr));
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_svc_device_refcount_balanced) {
    create_svc_fn c = svc_create(); REQUIRE(c);
    DXM_D9(d);
    ULONG rc = d3dh_refcount(d.dev);
    void *p = NULL;
    if (FAILED(c(d.dev, &DXM_IID_DecSvc, &p))) { t_skip("service unavailable"); DXM_D9_END(d); return; }
    CHECK(d3dh_refcount(d.dev) > rc);       /* service pins the device */
    REL(p);
    EXPECT_EQ(d3dh_refcount(d.dev), rc);
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_svc_shared_refcount_across_interfaces) {
    DEC_OPEN(s);
    EXPECT_EQ(UNK(s.dec)->lpVtbl->AddRef(UNK(s.dec)), 2);
    IDirectXVideoProcessorService *ps = NULL;
    EXPECT_HR(IDirectXVideoDecoderService_QueryInterface(s.dec, &DXM_IID_ProcSvc, (void **)&ps), S_OK);
    CHECK(ps != NULL);
    if (ps) {
        EXPECT_EQ(UNK(ps)->lpVtbl->AddRef(UNK(ps)), 4);     /* one count for the whole object, whichever interface is used */
        EXPECT_EQ(UNK(ps)->lpVtbl->Release(UNK(ps)), 3);
        EXPECT_EQ(UNK(ps)->lpVtbl->Release(UNK(ps)), 2);
    }
    EXPECT_EQ(UNK(s.dec)->lpVtbl->Release(UNK(s.dec)), 1);
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_qi_between_interfaces) {
    DEC_OPEN(s);
    IUnknown *u = NULL;
    IDirectXVideoAccelerationService *acc = NULL;
    IDirectXVideoProcessorService *ps = NULL;
    EXPECT_HR(IDirectXVideoDecoderService_QueryInterface(s.dec, &DXM_IID_Accel, (void **)&acc), S_OK);
    EXPECT_HR(IDirectXVideoDecoderService_QueryInterface(s.dec, &DXM_IID_ProcSvc, (void **)&ps), S_OK);
    EXPECT_HR(IDirectXVideoDecoderService_QueryInterface(s.dec, &DXM_IID_Unk, (void **)&u), S_OK);
    /* QI(IID_IUnknown) is the object identity: identical whichever interface is asked */
    IUnknown *u2 = NULL;
    if (ps) EXPECT_HR(UNK(ps)->lpVtbl->QueryInterface(UNK(ps), &DXM_IID_Unk, (void **)&u2), S_OK);
    CHECK(u != NULL && u == u2);
    if (acc != (IDirectXVideoAccelerationService *)s.dec) t_note("acceleration-service interface pointer differs from the decoder-service pointer");
    REL(acc); REL(ps); REL(u); REL(u2);
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_qi_unsupported) {
    DEC_OPEN(s);
    const GUID *bad[] = { &DXM_IID_Bogus, &DXM_IID_DevMgr9, &DXM_IID_Proc, &GUID_NULL };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        void *p = (void *)(ULONG_PTR)1;
        EXPECT_HR(IDirectXVideoDecoderService_QueryInterface(s.dec, bad[i], &p), E_NOINTERFACE_);
        CHECK(p == NULL);
    }
    svc_close(&s);
}

/* ---------------- CreateSurface ---------------- */
static const struct { D3DFORMAT f; const char *n; } kFormats[] = {
    { DXM_NV12, "NV12" }, { DXM_YUY2, "YUY2" }, { (D3DFORMAT)MAKEFOURCC('U','Y','V','Y'), "UYVY" },
    { (D3DFORMAT)MAKEFOURCC('Y','V','1','2'), "YV12" }, { (D3DFORMAT)MAKEFOURCC('A','Y','U','V'), "AYUV" },
    { (D3DFORMAT)MAKEFOURCC('I','M','C','3'), "IMC3" }, { (D3DFORMAT)MAKEFOURCC('P','0','1','0'), "P010" },
    { D3DFMT_X8R8G8B8, "X8R8G8B8" }, { D3DFMT_A8R8G8B8, "A8R8G8B8" }, { D3DFMT_R5G6B5, "R5G6B5" },
};
TEST(dxva2_monitor_video_svc_createsurface_invalid_type) {
    DEC_OPEN(s);
    static const DWORD bad[] = { 3, 4, 0x10, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        IDirect3DSurface9 *surf = (IDirect3DSurface9 *)(ULONG_PTR)1;
        HRESULT hr = IDirectXVideoDecoderService_CreateSurface(s.dec, 64, 64, 0, DXM_NV12, D3DPOOL_DEFAULT, 0, bad[i], &surf, NULL);
        EXPECT_HR(hr, E_INVALIDARG_);
        EXPECT_EQ(surf, 1);
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_createsurface_invalid_type_via_processor_iface) {
    PROC_OPEN(s);
    IDirect3DSurface9 *surf = (IDirect3DSurface9 *)(ULONG_PTR)1;
    EXPECT_HR(IDirectXVideoProcessorService_CreateSurface(s.proc, 64, 64, 0, DXM_NV12, D3DPOOL_DEFAULT, 0, 3, &surf, NULL), E_INVALIDARG_);
    EXPECT_EQ(surf, 1);
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_createsurface_formats_software_target) {
    DEC_OPEN(s);
    for (size_t i = 0; i < sizeof kFormats / sizeof *kFormats; i++) {
        IDirect3DSurface9 *surf = NULL;
        HRESULT hr = IDirectXVideoDecoderService_CreateSurface(s.dec, 64, 48, 0, kFormats[i].f, D3DPOOL_DEFAULT, 0, DXVA2_VideoSoftwareRenderTarget, &surf, NULL);
        if (SUCCEEDED(hr)) {
            D3DSURFACE_DESC desc; memset(&desc, 0, sizeof desc);
            CHECK(surf != NULL);
            if (surf && SUCCEEDED(IDirect3DSurface9_GetDesc(surf, &desc))) {
                EXPECT_EQ(desc.Width, 64); EXPECT_EQ(desc.Height, 48);
                EXPECT_EQ(desc.Format, kFormats[i].f);
                EXPECT_EQ(desc.Pool, D3DPOOL_DEFAULT);
                EXPECT_EQ(desc.Type, D3DRTYPE_SURFACE);
            }
            REL(surf);
        } else t_note("software-target surface %s -> %s", kFormats[i].n, d3dh_hr_name(hr));
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_createsurface_processor_target) {
    PROC_OPEN(s);
    for (size_t i = 0; i < sizeof kFormats / sizeof *kFormats; i++) {
        IDirect3DSurface9 *surf = NULL;
        HRESULT hr = IDirectXVideoProcessorService_CreateSurface(s.proc, 128, 96, 0, kFormats[i].f, D3DPOOL_DEFAULT, 0, DXVA2_VideoProcessorRenderTarget, &surf, NULL);
        if (SUCCEEDED(hr)) {
            D3DSURFACE_DESC desc; memset(&desc, 0, sizeof desc);
            if (surf && SUCCEEDED(IDirect3DSurface9_GetDesc(surf, &desc))) {
                EXPECT_EQ(desc.Width, 128); EXPECT_EQ(desc.Height, 96); EXPECT_EQ(desc.Format, kFormats[i].f);
                t_note("processor-target %s: usage 0x%lx", kFormats[i].n, (unsigned long)desc.Usage);
            }
            REL(surf);
        } else t_note("processor-target surface %s -> %s", kFormats[i].n, d3dh_hr_name(hr));
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_createsurface_backbuffers_array) {
    DEC_OPEN(s);
    IDirect3DSurface9 *surf[4] = { 0 };
    HRESULT hr = IDirectXVideoDecoderService_CreateSurface(s.dec, 64, 64, 3, DXM_NV12, D3DPOOL_DEFAULT, 0, DXVA2_VideoSoftwareRenderTarget, surf, NULL);
    if (SUCCEEDED(hr)) {
        for (int i = 0; i < 4; i++) { CHECK(surf[i] != NULL); }
        CHECK(surf[0] != surf[1] && surf[1] != surf[2] && surf[2] != surf[3]);   /* BackBuffers + 1 distinct surfaces */
        for (int i = 0; i < 4; i++) REL(surf[i]);
    } else t_note("4 NV12 surfaces -> %s", d3dh_hr_name(hr));
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_createsurface_zero_size) {
    DEC_OPEN(s);
    IDirect3DSurface9 *surf = NULL;
    HRESULT hr = IDirectXVideoDecoderService_CreateSurface(s.dec, 0, 0, 0, DXM_NV12, D3DPOOL_DEFAULT, 0, DXVA2_VideoSoftwareRenderTarget, &surf, NULL);
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr)) REL(surf); else t_note("0x0 surface -> %s", d3dh_hr_name(hr));
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_createsurface_unknown_format) {
    DEC_OPEN(s);
    IDirect3DSurface9 *surf = NULL;
    HRESULT hr = IDirectXVideoDecoderService_CreateSurface(s.dec, 64, 64, 0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, 0, DXVA2_VideoSoftwareRenderTarget, &surf, NULL);
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr)) REL(surf); else t_note("D3DFMT_UNKNOWN surface -> %s", d3dh_hr_name(hr));
    svc_close(&s);
}
TEST(dxva2_monitor_video_svc_createsurface_releases_device_refs) {
    DEC_OPEN(s);
    IDirect3DSurface9 *surf = NULL;
    ULONG rc = d3dh_refcount(s.d.dev);
    HRESULT hr = IDirectXVideoDecoderService_CreateSurface(s.dec, 64, 64, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, 0, DXVA2_VideoSoftwareRenderTarget, &surf, NULL);
    if (SUCCEEDED(hr)) { REL(surf); EXPECT_EQ(d3dh_refcount(s.d.dev), rc); }
    else t_note("-> %s", d3dh_hr_name(hr));
    svc_close(&s);
}

/* ---------------- decoder service ---------------- */
static UINT dec_count(IDirectXVideoDecoderService *d, HRESULT *hr_out)
{
    UINT n = 0xDEAD;
    HRESULT hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(d, &n, NULL);
    if (hr_out) *hr_out = hr;
    return SUCCEEDED(hr) ? n : 0;
}
TEST(dxva2_monitor_video_dec_guids_count_only) {
    DEC_OPEN(s);
    UINT n = 0xDEAD;
    HRESULT hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(s.dec, &n, NULL);
    EXPECT_HR(hr, S_OK);
    CHECK(n < 1024);
    t_note("decoder device GUID count: %u", n);
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_guids_array) {
    DEC_OPEN(s);
    HRESULT hr0; UINT cnt = dec_count(s.dec, &hr0);
    UINT n = 0; GUID *g = (GUID *)(ULONG_PTR)1;
    HRESULT hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(s.dec, &n, &g);
    if (cnt == 0) {
        EXPECT_HR(hr, E_FAIL_);                 /* no decoder at all: the array request fails, it does not return an empty array */
        CHECK(g == NULL);
        EXPECT_EQ(n, 0);
    } else {
        EXPECT_HR(hr, S_OK);
        EXPECT_EQ(n, cnt);
        CHECK(g != NULL);
        if (g) {
            for (UINT i = 0; i < n; i++)
                for (UINT j = i + 1; j < n; j++) CHECK(!IsEqualGUID(&g[i], &g[j]));
            for (UINT i = 0; i < n && i < 32; i++) t_note("decoder %u: %s", i, d3dh_guid_str(&g[i]));
            CoTaskMemFree(g);
        }
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_guids_repeatable) {
    DEC_OPEN(s);
    UINT a = dec_count(s.dec, NULL), b = dec_count(s.dec, NULL);
    EXPECT_EQ(a, b);
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_rendertargets_unknown_guid) {
    DEC_OPEN(s);
    UINT cnt = dec_count(s.dec, NULL);
    UINT n = 0; D3DFORMAT *f = (D3DFORMAT *)(ULONG_PTR)1;
    HRESULT hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(s.dec, &DXM_IID_Bogus, &n, &f);
    CHECK(FAILED(hr));
    if (cnt == 0) EXPECT_HR(hr, E_FAIL_);     /* the decoder GUID list is consulted first, and an empty list is E_FAIL */
    else t_note("unknown decoder GUID -> %s", d3dh_hr_name(hr));
    CHECK(f == NULL);                          /* output array is reset on failure */
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_rendertargets_for_each_guid) {
    DEC_OPEN(s);
    UINT n = 0; GUID *g = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderDeviceGuids(s.dec, &n, &g)) || n == 0) { t_skip("no decoder devices on this machine"); svc_close(&s); return; }
    for (UINT i = 0; i < n && i < 32; i++) {
        UINT cnt = 0; D3DFORMAT *f = NULL;
        HRESULT hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(s.dec, &g[i], &cnt, NULL);
        EXPECT_HR(hr, S_OK);
        if (FAILED(hr)) continue;
        hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(s.dec, &g[i], &cnt, &f);
        if (cnt == 0) { EXPECT_HR(hr, E_FAIL_); CHECK(f == NULL); }
        else { EXPECT_HR(hr, S_OK); CHECK(f != NULL); if (f) { for (UINT k = 0; k < cnt; k++) CHECK(f[k] != D3DFMT_UNKNOWN); CoTaskMemFree(f); } }
    }
    CoTaskMemFree(g);
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_configs_invalid_desc) {
    DEC_OPEN(s);
    UINT cnt = dec_count(s.dec, NULL);
    DXVA2_VideoDesc bad = dxm_desc_badin();
    UINT n = 0x55; DXVA2_ConfigPictureDecode *cfg = (DXVA2_ConfigPictureDecode *)(ULONG_PTR)1;
    HRESULT hr = IDirectXVideoDecoderService_GetDecoderConfigurations(s.dec, &DXM_IID_Bogus, &bad, NULL, &n, &cfg);
    /* with decoders present the unknown GUID passes the list scan and the description check rejects the call; with none, the empty list is E_FAIL */
    EXPECT_HR(hr, cnt == 0 ? E_FAIL_ : E_INVALIDARG_);
    CHECK(cfg == NULL);
    DXVA2_VideoDesc bad2 = dxm_desc_badout();
    cfg = (DXVA2_ConfigPictureDecode *)(ULONG_PTR)1;
    hr = IDirectXVideoDecoderService_GetDecoderConfigurations(s.dec, &DXM_IID_Bogus, &bad2, NULL, &n, &cfg);
    EXPECT_HR(hr, cnt == 0 ? E_FAIL_ : E_INVALIDARG_);
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_configs_for_each_guid) {
    DEC_OPEN(s);
    UINT n = 0; GUID *g = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderDeviceGuids(s.dec, &n, &g)) || n == 0) { t_skip("no decoder devices"); svc_close(&s); return; }
    DXVA2_VideoDesc desc = dxm_desc(1280, 720, DXM_NV12);
    for (UINT i = 0; i < n && i < 16; i++) {
        UINT cnt = 0; DXVA2_ConfigPictureDecode *cfg = NULL;
        HRESULT hr = IDirectXVideoDecoderService_GetDecoderConfigurations(s.dec, &g[i], &desc, NULL, &cnt, &cfg);
        if (SUCCEEDED(hr)) {
            CHECK(cfg != NULL || cnt == 0);
                        t_note("decoder %s: %u configuration(s)", d3dh_guid_str(&g[i]), cnt);
            CoTaskMemFree(cfg);
        } else t_note("decoder %s: configurations -> %s", d3dh_guid_str(&g[i]), d3dh_hr_name(hr));
    }
    CoTaskMemFree(g);
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_create_decoder_zero_surfaces) {
    DEC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(64, 64, DXM_NV12);
    DXVA2_ConfigPictureDecode cfg; memset(&cfg, 0, sizeof cfg);
    IDirect3DSurface9 *surf = NULL;
    IDirectXVideoDecoder *dec = (IDirectXVideoDecoder *)(ULONG_PTR)1;
    EXPECT_HR(IDirectXVideoDecoderService_CreateVideoDecoder(s.dec, &DXM_IID_Bogus, &desc, &cfg, &surf, 0, &dec), E_INVALIDARG_);
    EXPECT_EQ(dec, 1);   /* rejected before the output is touched */
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_create_decoder_invalid_desc) {
    DEC_OPEN(s);
    UINT cnt = dec_count(s.dec, NULL);
    DXVA2_VideoDesc bad = dxm_desc_badin();
    DXVA2_ConfigPictureDecode cfg; memset(&cfg, 0, sizeof cfg);
    IDirect3DSurface9 *surf = NULL;
    IDirectXVideoDecoder *dec = (IDirectXVideoDecoder *)(ULONG_PTR)1;
    HRESULT hr = IDirectXVideoDecoderService_CreateVideoDecoder(s.dec, &DXM_IID_Bogus, &bad, &cfg, &surf, 1, &dec);
    EXPECT_HR(hr, cnt == 0 ? E_FAIL_ : E_INVALIDARG_);
    CHECK(dec == NULL);
    svc_close(&s);
}
TEST(dxva2_monitor_video_dec_create_decoder_full_flow) {
    DEC_OPEN(s);
    UINT n = 0; GUID *g = NULL;
    if (FAILED(IDirectXVideoDecoderService_GetDecoderDeviceGuids(s.dec, &n, &g)) || n == 0) { t_skip("no decoder devices"); svc_close(&s); return; }
    DXVA2_VideoDesc desc = dxm_desc(640, 480, DXM_NV12);
    int done = 0;
    for (UINT i = 0; i < n && !done; i++) {
        UINT fc = 0; D3DFORMAT *fmts = NULL;
        if (FAILED(IDirectXVideoDecoderService_GetDecoderRenderTargets(s.dec, &g[i], &fc, &fmts)) || !fmts) continue;
        int has = 0; for (UINT k = 0; k < fc; k++) if (fmts[k] == DXM_NV12) has = 1;
        CoTaskMemFree(fmts);
        if (!has) continue;
        UINT cc = 0; DXVA2_ConfigPictureDecode *cfg = NULL;
        if (FAILED(IDirectXVideoDecoderService_GetDecoderConfigurations(s.dec, &g[i], &desc, NULL, &cc, &cfg)) || !cfg || !cc) { CoTaskMemFree(cfg); continue; }
        IDirect3DSurface9 *surf[3] = { 0 };
        HRESULT hr = IDirectXVideoDecoderService_CreateSurface(s.dec, 640, 480, 2, DXM_NV12, D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, surf, NULL);
        if (SUCCEEDED(hr)) {
            IDirectXVideoDecoder *dec = NULL;
            hr = IDirectXVideoDecoderService_CreateVideoDecoder(s.dec, &g[i], &desc, &cfg[0], surf, 3, &dec);
            if (SUCCEEDED(hr) && dec) {
                GUID got; DXVA2_VideoDesc gd; DXVA2_ConfigPictureDecode gc; IDirect3DSurface9 **rts = NULL; UINT ns = 0;
                hr = IDirectXVideoDecoder_GetCreationParameters(dec, &got, &gd, &gc, &rts, &ns);
                if (SUCCEEDED(hr)) {
                    CHECK(IsEqualGUID(&got, &g[i]));
                    EXPECT_EQ(gd.SampleWidth, 640); EXPECT_EQ(gd.SampleHeight, 480);
                    EXPECT_EQ(ns, 3);
                    if (rts) { for (UINT k = 0; k < ns; k++) REL(rts[k]); CoTaskMemFree(rts); }
                }
                IDirectXVideoDecoderService *back = NULL;
                if (SUCCEEDED(IDirectXVideoDecoder_GetVideoDecoderService(dec, &back))) { CHECK(back == s.dec); REL(back); }
                REL(dec);
                done = 1;
            } else t_note("CreateVideoDecoder(%s) -> %s", d3dh_guid_str(&g[i]), d3dh_hr_name(hr));
            for (int k = 0; k < 3; k++) REL(surf[k]);
        }
        CoTaskMemFree(cfg);
    }
    if (!done) t_note("no decoder/NV12 combination could be instantiated");
    CoTaskMemFree(g);
    svc_close(&s);
}

/* ---------------- processor service ---------------- */
TEST(dxva2_monitor_video_proc_guids_invalid_desc) {
    PROC_OPEN(s);
    DXVA2_VideoDesc bads[2] = { dxm_desc_badin(), dxm_desc_badout() };
    for (int i = 0; i < 2; i++) {
        UINT n = 0x1234; GUID *g = (GUID *)(ULONG_PTR)1;
        EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids(s.proc, &bads[i], &n, &g), E_INVALIDARG_);
        EXPECT_EQ(n, 0);                       /* count cleared, array nulled */
        CHECK(g == NULL);
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_guids_count_and_array) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(640, 480, DXM_YUY2);
    UINT cnt = 0xDEAD;
    HRESULT hr = IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids(s.proc, &desc, &cnt, NULL);
    EXPECT_HR(hr, S_OK);
    UINT n = 0; GUID *g = (GUID *)(ULONG_PTR)1;
    hr = IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids(s.proc, &desc, &n, &g);
    if (cnt == 0) { EXPECT_HR(hr, E_FAIL_); CHECK(g == NULL); }
    else {
        EXPECT_HR(hr, S_OK);
        EXPECT_EQ(n, cnt);
        if (g) {
            int sw = 0;
            for (UINT i = 0; i < n; i++) {
                if (IsEqualGUID(&g[i], &DXM_SwProc)) sw = 1;
                for (UINT j = i + 1; j < n; j++) CHECK(!IsEqualGUID(&g[i], &g[j]));
                t_note("processor %u: %s", i, d3dh_guid_str(&g[i]));
            }
            t_note("software processor listed: %d", sw);
            CoTaskMemFree(g);
        }
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_guids_zero_rates_valid) {
    PROC_OPEN(s);
    /* a zero numerator with any denominator, and 0/0, are valid frame rates */
    DXVA2_VideoDesc d1 = dxm_desc(64, 64, DXM_NV12);
    d1.InputSampleFreq.Numerator = 0; d1.InputSampleFreq.Denominator = 0;
    d1.OutputFrameFreq.Numerator = 0; d1.OutputFrameFreq.Denominator = 7;
    UINT n = 0;
    HRESULT hr = IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids(s.proc, &d1, &n, NULL);
    CHECK(hr != E_INVALIDARG_);
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_rendertargets_invalid_desc) {
    PROC_OPEN(s);
    DXVA2_VideoDesc bad = dxm_desc_badin();
    UINT n = 7; D3DFORMAT *f = (D3DFORMAT *)(ULONG_PTR)1;
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorRenderTargets(s.proc, &DXM_SwProc, &bad, &n, &f), E_INVALIDARG_);
    CHECK(f == NULL);
    EXPECT_EQ(n, 7);
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_substream_invalid_desc) {
    PROC_OPEN(s);
    DXVA2_VideoDesc bad = dxm_desc_badout();
    UINT n = 7; D3DFORMAT *f = (D3DFORMAT *)(ULONG_PTR)1;
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorSubStreamFormats(s.proc, &DXM_SwProc, &bad, D3DFMT_X8R8G8B8, &n, &f), E_INVALIDARG_);
    CHECK(f == NULL);
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_caps_invalid_desc) {
    PROC_OPEN(s);
    DXVA2_VideoDesc bad = dxm_desc_badin();
    DXVA2_VideoProcessorCaps caps; memset(&caps, 0x5A, sizeof caps);
    EXPECT_HR(IDirectXVideoProcessorService_GetVideoProcessorCaps(s.proc, &DXM_SwProc, &bad, D3DFMT_X8R8G8B8, &caps), E_INVALIDARG_);
    EXPECT_EQ(caps.DeviceCaps, 0x5A5A5A5A);
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_procamp_invalid_cap) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(64, 64, DXM_NV12);
    /* only the single flags 1, 2, 4, 8 are accepted; the check precedes every other validation */
    static const UINT bad[] = { 0, 3, 5, 6, 7, 9, 10, 12, 15, 16, 0x10, 0x80, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        DXVA2_ValueRange r; memset(&r, 0x77, sizeof r);
        EXPECT_HR(IDirectXVideoProcessorService_GetProcAmpRange(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, bad[i], &r), E_INVALIDARG_);
        EXPECT_EQ(r.MinValue.ll, 0x77777777);
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_procamp_valid_cap_invalid_desc) {
    PROC_OPEN(s);
    DXVA2_VideoDesc bad = dxm_desc_badin();
    for (UINT cap = 1; cap <= 8; cap <<= 1) {
        DXVA2_ValueRange r;
        EXPECT_HR(IDirectXVideoProcessorService_GetProcAmpRange(s.proc, &DXM_SwProc, &bad, D3DFMT_X8R8G8B8, cap, &r), E_INVALIDARG_);
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_procamp_software_ranges) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(320, 240, DXM_YUY2);
    static const char *const names[] = { "brightness", "contrast", "hue", "saturation" };
    for (int i = 0; i < 4; i++) {
        DXVA2_ValueRange r; memset(&r, 0, sizeof r);
        HRESULT hr = IDirectXVideoProcessorService_GetProcAmpRange(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, 1u << i, &r);
        if (SUCCEEDED(hr)) {
            CHECK(r.MinValue.ll <= r.DefaultValue.ll);
            CHECK(r.DefaultValue.ll <= r.MaxValue.ll);
            CHECK(r.StepSize.ll > 0);
            t_note("%s: min %d.%05u max %d default %d step raw 0x%lx", names[i], r.MinValue.Value, r.MinValue.Fraction, r.MaxValue.Value, r.DefaultValue.Value, (unsigned long)r.StepSize.ll);
        } else t_note("%s range -> %s", names[i], d3dh_hr_name(hr));
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_filter_invalid_selector) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(64, 64, DXM_NV12);
    /* valid selectors are 1..12 (noise / detail filters); anything else fails before the description or device is examined */
    static const UINT bad[] = { 0, 13, 14, 100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        DXVA2_ValueRange r;
        EXPECT_HR(IDirectXVideoProcessorService_GetFilterPropertyRange(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, bad[i], &r), E_INVALIDARG_);
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_filter_valid_selector_invalid_desc) {
    PROC_OPEN(s);
    DXVA2_VideoDesc bad = dxm_desc_badin();
    for (UINT f = 1; f <= 12; f++) {
        DXVA2_ValueRange r;
        EXPECT_HR(IDirectXVideoProcessorService_GetFilterPropertyRange(s.proc, &DXM_SwProc, &bad, D3DFMT_X8R8G8B8, f, &r), E_INVALIDARG_);
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_filter_software_ranges) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(320, 240, DXM_YUY2);
    for (UINT f = 1; f <= 12; f++) {
        DXVA2_ValueRange r; memset(&r, 0, sizeof r);
        HRESULT hr = IDirectXVideoProcessorService_GetFilterPropertyRange(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, f, &r);
        if (SUCCEEDED(hr)) { CHECK(r.MinValue.ll <= r.MaxValue.ll); CHECK(r.DefaultValue.ll >= r.MinValue.ll && r.DefaultValue.ll <= r.MaxValue.ll); }
        else t_note("filter %u -> %s", f, d3dh_hr_name(hr));
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_caps_software_device) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(320, 240, DXM_YUY2);
    DXVA2_VideoProcessorCaps caps; memset(&caps, 0xCC, sizeof caps);
    HRESULT hr = IDirectXVideoProcessorService_GetVideoProcessorCaps(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, &caps);
    if (SUCCEEDED(hr)) {
        EXPECT_EQ(caps.Reserved, 0);                                      /* always zeroed */
        CHECK(caps.NumForwardRefSamples + caps.NumBackwardRefSamples < 32);
        t_note("software caps: device 0x%x fwd %u back %u deint 0x%x procamp 0x%x ops 0x%x", caps.DeviceCaps,
               caps.NumForwardRefSamples, caps.NumBackwardRefSamples, caps.DeinterlaceTechnology, caps.ProcAmpControlCaps, caps.VideoProcessorOperations);
    } else t_note("software device caps -> %s", d3dh_hr_name(hr));
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_caps_unknown_guid) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(64, 64, DXM_NV12);
    DXVA2_VideoProcessorCaps caps;
    HRESULT hr = IDirectXVideoProcessorService_GetVideoProcessorCaps(s.proc, &DXM_IID_Bogus, &desc, D3DFMT_X8R8G8B8, &caps);
    CHECK(FAILED(hr));
    t_note("unknown processor GUID caps -> %s", d3dh_hr_name(hr));
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_rendertargets_software_device) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(320, 240, DXM_YUY2);
    UINT n = 0; D3DFORMAT *f = NULL;
    HRESULT hr = IDirectXVideoProcessorService_GetVideoProcessorRenderTargets(s.proc, &DXM_SwProc, &desc, &n, NULL);
    if (SUCCEEDED(hr)) {
        hr = IDirectXVideoProcessorService_GetVideoProcessorRenderTargets(s.proc, &DXM_SwProc, &desc, &n, &f);
        if (n == 0) { EXPECT_HR(hr, E_FAIL_); CHECK(f == NULL); }
        else { EXPECT_HR(hr, S_OK); CHECK(f != NULL); CoTaskMemFree(f); }
    } else t_note("software render targets -> %s", d3dh_hr_name(hr));
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_substreams_software_device) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(320, 240, DXM_YUY2);
    UINT n = 0; D3DFORMAT *f = NULL;
    HRESULT hr = IDirectXVideoProcessorService_GetVideoProcessorSubStreamFormats(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, &n, NULL);
    if (SUCCEEDED(hr)) {
        hr = IDirectXVideoProcessorService_GetVideoProcessorSubStreamFormats(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, &n, &f);
        if (n == 0) { EXPECT_HR(hr, E_FAIL_); CHECK(f == NULL); } else { CHECK(f != NULL); CoTaskMemFree(f); }
    } else t_note("sub-stream formats -> %s", d3dh_hr_name(hr));
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_create_invalid_desc) {
    PROC_OPEN(s);
    DXVA2_VideoDesc bad = dxm_desc_badin();
    IDirectXVideoProcessor *p = (IDirectXVideoProcessor *)(ULONG_PTR)1;
    EXPECT_HR(IDirectXVideoProcessorService_CreateVideoProcessor(s.proc, &DXM_SwProc, &bad, D3DFMT_X8R8G8B8, 0, &p), E_INVALIDARG_);
    CHECK(p == NULL);
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_create_too_many_substreams) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(64, 64, DXM_NV12);
    static const UINT bad[] = { 16, 17, 255, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        IDirectXVideoProcessor *p = (IDirectXVideoProcessor *)(ULONG_PTR)1;
        EXPECT_HR(IDirectXVideoProcessorService_CreateVideoProcessor(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, bad[i], &p), E_INVALIDARG_);
        CHECK(p == NULL);
    }
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_create_unknown_guid) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(64, 64, DXM_NV12);
    IDirectXVideoProcessor *p = (IDirectXVideoProcessor *)(ULONG_PTR)1;
    HRESULT hr = IDirectXVideoProcessorService_CreateVideoProcessor(s.proc, &DXM_IID_Bogus, &desc, D3DFMT_X8R8G8B8, 0, &p);
    CHECK(FAILED(hr));
    CHECK(p == NULL);
    t_note("unknown processor GUID create -> %s", d3dh_hr_name(hr));
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_create_software_processor) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(320, 240, DXM_YUY2);
    IDirectXVideoProcessor *p = NULL;
    HRESULT hr = IDirectXVideoProcessorService_CreateVideoProcessor(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, 2, &p);
    if (FAILED(hr)) { t_note("software processor create -> %s", d3dh_hr_name(hr)); CHECK(p == NULL); svc_close(&s); return; }
    CHECK(p != NULL);
    GUID g; DXVA2_VideoDesc gd; D3DFORMAT fmt = 0; UINT subs = 99;
    EXPECT_HR(IDirectXVideoProcessor_GetCreationParameters(p, &g, &gd, &fmt, &subs), S_OK);
    CHECK(IsEqualGUID(&g, &DXM_SwProc));
    EXPECT_EQ(gd.SampleWidth, 320); EXPECT_EQ(gd.SampleHeight, 240); EXPECT_EQ(gd.Format, DXM_YUY2);
    EXPECT_EQ(fmt, D3DFMT_X8R8G8B8);
    EXPECT_EQ(subs, 2);
    IDirectXVideoProcessorService *back = NULL;
    if (SUCCEEDED(IDirectXVideoProcessor_GetVideoProcessorService(p, &back))) { CHECK(back == s.proc); REL(back); }
    DXVA2_VideoProcessorCaps caps;
    if (SUCCEEDED(IDirectXVideoProcessor_GetVideoProcessorCaps(p, &caps))) EXPECT_EQ(caps.Reserved, 0);
    void *q = (void *)(ULONG_PTR)1;
    EXPECT_HR(UNK(p)->lpVtbl->QueryInterface(UNK(p), &DXM_IID_Bogus, &q), E_NOINTERFACE_);
    CHECK(q == NULL);
    EXPECT_EQ(UNK(p)->lpVtbl->Release(UNK(p)), 0);
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_processor_keeps_service_alive) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(64, 64, DXM_YUY2);
    IDirectXVideoProcessor *p = NULL;
    if (FAILED(IDirectXVideoProcessorService_CreateVideoProcessor(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, 0, &p)) || !p) { t_skip("cannot create a software processor"); svc_close(&s); return; }
    ULONG rc = UNK(s.proc)->lpVtbl->AddRef(UNK(s.proc));
    UNK(s.proc)->lpVtbl->Release(UNK(s.proc));
    REL(p);
    ULONG rc2 = UNK(s.proc)->lpVtbl->AddRef(UNK(s.proc));
    UNK(s.proc)->lpVtbl->Release(UNK(s.proc));
    CHECK(rc2 <= rc);
    t_note("service refcount with processor alive %lu, after release %lu", rc, rc2);
    svc_close(&s);
}
TEST(dxva2_monitor_video_proc_procamp_via_processor_object) {
    PROC_OPEN(s);
    DXVA2_VideoDesc desc = dxm_desc(64, 64, DXM_YUY2);
    IDirectXVideoProcessor *p = NULL;
    if (FAILED(IDirectXVideoProcessorService_CreateVideoProcessor(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, 0, &p)) || !p) { t_skip("cannot create a software processor"); svc_close(&s); return; }
    DXVA2_ValueRange r;
    HRESULT hr = IDirectXVideoProcessor_GetProcAmpRange(p, 3, &r);
    t_note("processor object GetProcAmpRange(3) -> %s", d3dh_hr_name(hr));
    for (UINT cap = 1; cap <= 8; cap <<= 1) {
        DXVA2_ValueRange a, b; memset(&a, 0, sizeof a); memset(&b, 1, sizeof b);
        HRESULT h1 = IDirectXVideoProcessor_GetProcAmpRange(p, cap, &a);
        HRESULT h2 = IDirectXVideoProcessorService_GetProcAmpRange(s.proc, &DXM_SwProc, &desc, D3DFMT_X8R8G8B8, cap, &b);
        if (SUCCEEDED(h1) != SUCCEEDED(h2)) t_note("cap %u: object %s vs service %s", cap, d3dh_hr_name(h1), d3dh_hr_name(h2));
        else if (SUCCEEDED(h1) && (a.MinValue.ll != b.MinValue.ll || a.MaxValue.ll != b.MaxValue.ll || a.DefaultValue.ll != b.DefaultValue.ll)) t_note("cap %u: ranges differ", cap);
    }
    REL(p);
    svc_close(&s);
}
