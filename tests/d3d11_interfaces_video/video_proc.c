/* ID3D11VideoProcessorEnumerator / ID3D11VideoProcessor: creation validation, capability reporting, per-stream
 * and output state accessors, view validation and a tiny NV12 -> BGRA blit. */
#include "ifv.h"

#define QE(iid, w) { &iid, #iid, w }
#define VC(v) ((v).vc)
#define NZ_COLOR_SPACE_BITS(cs) (*(UINT *)&(cs))

static D3D11_VIDEO_PROCESSOR_CAPS ifv_caps(ID3D11VideoProcessorEnumerator *e) {
    D3D11_VIDEO_PROCESSOR_CAPS c; memset(&c, 0, sizeof(c));
    ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(e, &c);
    return c;
}

/* ---------------------------------------------------------------- enumerator creation */

T(proc_enum_null_arguments) {
    IFV_VIDEO(v);
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC d;
    ifv_content_desc(&d, 64, 64, 64, 64);
    ID3D11VideoProcessorEnumerator *e = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, NULL, &e), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, &d, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, NULL, NULL), E_INVALIDARG);
    ifv_video_close(&v);
}

T(proc_enum_invalid_frame_format_and_usage) {
    IFV_VIDEO(v);
    static const int bad[] = { 3, 4, 100, -1, 0x7fffffff };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC d;
        ID3D11VideoProcessorEnumerator *e = (void *)(ULONG_PTR)1;
        ifv_content_desc(&d, 64, 64, 64, 64);
        d.InputFrameFormat = (D3D11_VIDEO_FRAME_FORMAT)bad[i];
        HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, &d, &e);
        CHECKF(hr == E_INVALIDARG, "InputFrameFormat %d -> %s", bad[i], d3dh_hr_name(hr));
        CHECKF(e == NULL, "InputFrameFormat %d: out pointer not cleared", bad[i]);
        ifv_content_desc(&d, 64, 64, 64, 64);
        d.Usage = (D3D11_VIDEO_USAGE)bad[i];
        e = (void *)(ULONG_PTR)1;
        hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, &d, &e);
        CHECKF(hr == E_INVALIDARG, "Usage %d -> %s", bad[i], d3dh_hr_name(hr));
        CHECKF(e == NULL, "Usage %d: out pointer not cleared", bad[i]);
    }
    ifv_video_close(&v);
}

T(proc_enum_invalid_frame_rates) {
    IFV_VIDEO(v);
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC d;
    ID3D11VideoProcessorEnumerator *e;
    /* a non-zero numerator with a zero denominator is invalid for either rate */
    static const UINT nums[] = { 1, 24, 30000, 0xffffffffu };
    for (unsigned i = 0; i < sizeof(nums) / sizeof(nums[0]); i++) {
        ifv_content_desc(&d, 64, 64, 64, 64);
        d.InputFrameRate.Numerator = nums[i]; d.InputFrameRate.Denominator = 0;
        e = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, &d, &e);
        CHECKF(hr == E_INVALIDARG, "input rate %u/0 -> %s", nums[i], d3dh_hr_name(hr));
        CHECK(e == NULL);
        ifv_content_desc(&d, 64, 64, 64, 64);
        d.OutputFrameRate.Numerator = nums[i]; d.OutputFrameRate.Denominator = 0;
        e = (void *)(ULONG_PTR)1;
        hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, &d, &e);
        CHECKF(hr == E_INVALIDARG, "output rate %u/0 -> %s", nums[i], d3dh_hr_name(hr));
        CHECK(e == NULL);
    }
    /* 0/0 ("unspecified") passes the argument validation */
    ifv_content_desc(&d, 64, 64, 64, 64);
    d.InputFrameRate.Numerator = 0; d.InputFrameRate.Denominator = 0;
    d.OutputFrameRate.Numerator = 0; d.OutputFrameRate.Denominator = 0;
    e = NULL;
    HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, &d, &e);
    t_note("unspecified frame rates (0/0) -> %s", d3dh_hr_name(hr));
    CHECKF(hr != E_INVALIDARG || e == NULL, "inconsistent result");
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_enum_zero_dimensions) {
    IFV_VIDEO(v);
    for (int which = 0; which < 4; which++) {
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC d;
        ifv_content_desc(&d, 64, 64, 64, 64);
        switch (which) { case 0: d.InputWidth = 0; break; case 1: d.InputHeight = 0; break; case 2: d.OutputWidth = 0; break; default: d.OutputHeight = 0; }
        ID3D11VideoProcessorEnumerator *e = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, &d, &e);
        CHECKF(hr == E_INVALIDARG, "zero dimension #%d -> %s", which, d3dh_hr_name(hr));
        CHECKF(e == NULL, "zero dimension #%d: out pointer not cleared", which);
    }
    ifv_video_close(&v);
}

T(proc_enum_valid_matrix) {
    IFV_VIDEO(v);
    static const struct { UINT iw, ih, ow, oh; int fmt, usage; UINT fn, fd; } t[] = {
        { 16, 16, 16, 16, 0, 0, 30, 1 }, { 64, 48, 128, 96, 0, 0, 30000, 1001 }, { 320, 240, 160, 120, 1, 1, 25, 1 },
        { 640, 480, 640, 480, 2, 2, 60, 1 }, { 1920, 1080, 1280, 720, 0, 0, 24000, 1001 }, { 3840, 2160, 3840, 2160, 0, 1, 30, 1 },
        { 2, 2, 2, 2, 0, 0, 30, 1 }, { 1, 1, 1, 1, 0, 2, 1, 1 },
    };
    int made = 0;
    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC d;
        ifv_content_desc(&d, t[i].iw, t[i].ih, t[i].ow, t[i].oh);
        d.InputFrameFormat = (D3D11_VIDEO_FRAME_FORMAT)t[i].fmt; d.Usage = (D3D11_VIDEO_USAGE)t[i].usage;
        d.InputFrameRate.Numerator = d.OutputFrameRate.Numerator = t[i].fn;
        d.InputFrameRate.Denominator = d.OutputFrameRate.Denominator = t[i].fd;
        ID3D11VideoProcessorEnumerator *e = NULL;
        HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(v.vd, &d, &e);
        t_note("enumerator %ux%u -> %ux%u fmt %d usage %d: %s", t[i].iw, t[i].ih, t[i].ow, t[i].oh, t[i].fmt, t[i].usage, d3dh_hr_name(hr));
        if (FAILED(hr)) { CHECK(e == NULL); continue; }
        made++;
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC g; memset(&g, 0xcc, sizeof(g));
        EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorContentDesc(e, &g), S_OK);
        CHECK(!memcmp(&g, &d, sizeof(d)));
        SAFE_RELEASE(e);
    }
    if (!made) { ifv_video_close(&v); t_skip("no enumerator could be created"); return; }
    ifv_video_close(&v);
}

T(proc_enum_content_desc_null) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 32, 32, &e))) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorContentDesc(e, NULL), E_INVALIDARG);
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_enum_interfaces_and_lifetime) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    ULONG rc = d3dh_refcount(v.dev);
    if (FAILED(ifv_make_enum(&v, 32, 32, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    EXPECT_EQ(d3dh_refcount(v.dev), rc + 1);
    EXPECT_EQ(d3dh_refcount(e), 1);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_IUnknown, 1), QE(IID_ID3D11DeviceChild, 1), QE(IID_ID3D11VideoProcessorEnumerator, 1),
        QE(IID_ID3D11VideoProcessorEnumerator1, -1), QE(IID_ID3D11Device, 0), QE(IID_ID3D11VideoDevice, 0),
        QE(IID_ID3D11VideoProcessor, 0), QE(IID_ID3D11Resource, 0),
    };
    ifv_check_qi_table(e, tab, sizeof(tab) / sizeof(tab[0]), "video processor enumerator");
    ID3D11Device *g = NULL;
    ID3D11VideoProcessorEnumerator_GetDevice(e, &g);
    CHECK(g == v.dev);
    SAFE_RELEASE(g);
    EXPECT_EQ(D3DH_RELEASE_RC(e), 0);
    EXPECT_EQ(d3dh_refcount(v.dev), rc);
    ifv_video_close(&v);
}

T(proc_enum_private_data) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 32, 32, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    static const GUID g = {0x7c1a9d22, 0x44fb, 0x4a62, {0xb1, 0x90, 0x02, 0x6e, 0x5d, 0x83, 0x4a, 0x7f}};
    UINT data[4] = { 1, 2, 3, 4 }, got[4] = {0}, sz = sizeof(got);
    EXPECT_HR(ID3D11VideoProcessorEnumerator_SetPrivateData(e, &g, sizeof(data), data), S_OK);
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetPrivateData(e, &g, &sz, got), S_OK);
    EXPECT_EQ(sz, sizeof(data));
    CHECK(!memcmp(data, got, sizeof(data)));
    sz = 1;
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetPrivateData(e, &g, &sz, got), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, sizeof(data));
    sz = 16;
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetPrivateData(e, &ifv_IID_Random, &sz, got), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(sz, 0);
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

/* ---------------------------------------------------------------- capability reporting */

T(proc_caps_null_output) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 32, 32, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(e, NULL), E_INVALIDARG);
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_caps_stable_and_sane) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 64, 64, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    D3D11_VIDEO_PROCESSOR_CAPS a, b;
    memset(&a, 0xcc, sizeof(a)); memset(&b, 0x33, sizeof(b));
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(e, &a), S_OK);
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(e, &b), S_OK);
    CHECK(!memcmp(&a, &b, sizeof(a)));
    EXPECT_EQ(sizeof(D3D11_VIDEO_PROCESSOR_CAPS), 36);
    t_note("caps: device 0x%x feature 0x%x filter 0x%x inputfmt 0x%x autostream 0x%x stereo 0x%x rateconv %u maxin %u maxstates %u",
           a.DeviceCaps, a.FeatureCaps, a.FilterCaps, a.InputFormatCaps, a.AutoStreamCaps, a.StereoCaps,
           a.RateConversionCapsCount, a.MaxInputStreams, a.MaxStreamStates);
    CHECK((a.FilterCaps & ~0xffu) == 0);                 /* only the 8 defined filters */
    /* a second enumerator of the same content reports identical capabilities */
    ID3D11VideoProcessorEnumerator *e2 = NULL;
    if (SUCCEEDED(ifv_make_enum(&v, 64, 64, &e2)) && e2) {
        D3D11_VIDEO_PROCESSOR_CAPS c; memset(&c, 0, sizeof(c));
        ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(e2, &c);
        CHECK(!memcmp(&a, &c, sizeof(a)));
        SAFE_RELEASE(e2);
    }
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_check_format_flags) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 64, 64, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    EXPECT_HR(ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(e, DXGI_FORMAT_NV12, NULL), E_INVALIDARG);
    static const DXGI_FORMAT fmts[] = { DXGI_FORMAT_NV12, DXGI_FORMAT_YUY2, DXGI_FORMAT_P010, DXGI_FORMAT_AYUV, DXGI_FORMAT_Y410,
        DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R32G32B32A32_FLOAT };
    for (unsigned i = 0; i < sizeof(fmts) / sizeof(fmts[0]); i++) {
        UINT fl = 0xdeadbeef;
        HRESULT hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(e, fmts[i], &fl);
        CHECKF(hr == S_OK, "format %d -> %s", fmts[i], d3dh_hr_name(hr));
        CHECKF((fl & ~3u) == 0, "format %d: flags 0x%x contain undefined bits", fmts[i], fl);
        t_note("format %d: input=%d output=%d", fmts[i], !!(fl & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT), !!(fl & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT));
    }
    UINT fl = 0xdeadbeef;
    HRESULT hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(e, DXGI_FORMAT_UNKNOWN, &fl);
    t_note("format UNKNOWN -> %s flags 0x%x", d3dh_hr_name(hr), fl);
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_filter_range_table) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 64, 64, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    D3D11_VIDEO_PROCESSOR_FILTER_RANGE r;
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(e, D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS, NULL), E_INVALIDARG);
    for (int f = 0; f < 8; f++) {
        memset(&r, 0xcc, sizeof(r));
        HRESULT hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(e, (D3D11_VIDEO_PROCESSOR_FILTER)f, &r);
        if (caps.FilterCaps & (1u << f)) {
            CHECKF(hr == S_OK, "supported filter %d -> %s", f, d3dh_hr_name(hr));
            CHECKF(r.Minimum <= r.Default && r.Default <= r.Maximum, "filter %d range %d..%d default %d", f, r.Minimum, r.Maximum, r.Default);
            t_note("filter %d: min %d max %d default %d mult %g", f, r.Minimum, r.Maximum, r.Default, r.Multiplier);
        } else {
            CHECKF(hr == E_INVALIDARG, "unsupported filter %d -> %s", f, d3dh_hr_name(hr));
            CHECKF(!r.Minimum && !r.Maximum && !r.Default && r.Multiplier == 0.0f, "filter %d: range not cleared", f);
        }
    }
    static const int bad[] = { 8, 9, 31, 100, -1, 0x7fffffff };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        memset(&r, 0xcc, sizeof(r));
        HRESULT hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(e, (D3D11_VIDEO_PROCESSOR_FILTER)bad[i], &r);
        CHECKF(hr == E_INVALIDARG, "filter %d -> %s", bad[i], d3dh_hr_name(hr));
    }
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_rate_conversion_caps_indices) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 64, 64, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rc;
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(e, 0, NULL), E_INVALIDARG);
    for (UINT i = 0; i < caps.RateConversionCapsCount && i < 16; i++) {
        memset(&rc, 0xcc, sizeof(rc));
        EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(e, i, &rc), S_OK);
        t_note("rate conv %u: past %u future %u proc 0x%x itelecine 0x%x custom rates %u", i, rc.PastFrames, rc.FutureFrames,
               rc.ProcessorCaps, rc.ITelecineCaps, rc.CustomRateCount);
    }
    static const UINT64 off[] = { 0, 1, 2, 1000 };
    for (unsigned i = 0; i < sizeof(off) / sizeof(off[0]); i++) {
        UINT idx = caps.RateConversionCapsCount + (UINT)off[i];
        memset(&rc, 0xcc, sizeof(rc));
        HRESULT hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(e, idx, &rc);
        CHECKF(hr == E_INVALIDARG, "rate conversion index %u of %u -> %s", idx, caps.RateConversionCapsCount, d3dh_hr_name(hr));
    }
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(e, 0xffffffffu, &rc), E_INVALIDARG);
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_custom_rate_indices) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 64, 64, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    D3D11_VIDEO_PROCESSOR_CUSTOM_RATE cr;
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCustomRate(e, 0, 0, NULL), E_INVALIDARG);
    memset(&cr, 0xcc, sizeof(cr));
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCustomRate(e, caps.RateConversionCapsCount, 0, &cr), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCustomRate(e, 0xffffffffu, 0, &cr), E_INVALIDARG);
    for (UINT t = 0; t < caps.RateConversionCapsCount && t < 4; t++) {
        D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rc;
        if (FAILED(ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(e, t, &rc))) continue;
        for (UINT k = 0; k < rc.CustomRateCount && k < 8; k++) {
            memset(&cr, 0xcc, sizeof(cr));
            EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCustomRate(e, t, k, &cr), S_OK);
            t_note("custom rate %u/%u: %u/%u out frames %u interlaced %d in %u", t, k, cr.CustomRate.Numerator, cr.CustomRate.Denominator,
                   cr.OutputFrames, cr.InputInterlaced, cr.InputFramesOrFields);
        }
        EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCustomRate(e, t, rc.CustomRateCount, &cr), E_INVALIDARG);
        EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorCustomRate(e, t, 0xffffffffu, &cr), E_INVALIDARG);
    }
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_enumerator1_format_conversion) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 64, 64, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    ID3D11VideoProcessorEnumerator1 *e1 = NULL;
    if (FAILED(ID3D11VideoProcessorEnumerator_QueryInterface(e, &IID_ID3D11VideoProcessorEnumerator1, (void **)&e1))) {
        SAFE_RELEASE(e); ifv_video_close(&v); t_skip("no ID3D11VideoProcessorEnumerator1"); return;
    }
    BOOL ok = 0x5a5a;
    EXPECT_HR(ID3D11VideoProcessorEnumerator1_CheckVideoProcessorFormatConversion(e1, DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
              DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoProcessorEnumerator1_CheckVideoProcessorFormatConversion(e1, DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
              DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &ok), S_OK);
    CHECK(ok == 0 || ok == 1);
    t_note("NV12/709 -> BGRA/RGB full conversion supported: %d", ok);
    ok = 0x5a5a;
    HRESULT hr = ID3D11VideoProcessorEnumerator1_CheckVideoProcessorFormatConversion(e1, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
              DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &ok);
    EXPECT_HR(hr, S_OK);
    t_note("BGRA -> BGRA conversion supported: %d", ok);
    ID3D11VideoProcessorEnumerator1_Release(e1);
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

/* ---------------------------------------------------------------- processor creation */

T(proc_create_null_arguments) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 64, 64, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    ID3D11VideoProcessor *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessor(v.vd, NULL, 0, &p), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessor(v.vd, e, 0, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessor(v.vd, NULL, 0, NULL), E_INVALIDARG);
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_create_rate_conversion_index_range) {
    IFV_VIDEO(v);
    ID3D11VideoProcessorEnumerator *e = NULL;
    if (FAILED(ifv_make_enum(&v, 64, 64, &e)) || !e) { ifv_video_close(&v); t_skip("no enumerator"); return; }
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    static const UINT64 off[] = { 0, 1, 7, 0x1000 };
    for (unsigned i = 0; i < sizeof(off) / sizeof(off[0]); i++) {
        UINT idx = caps.RateConversionCapsCount + (UINT)off[i];
        ID3D11VideoProcessor *p = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D11VideoDevice_CreateVideoProcessor(v.vd, e, idx, &p);
        CHECKF(hr == E_INVALIDARG, "rate conversion index %u of %u -> %s", idx, caps.RateConversionCapsCount, d3dh_hr_name(hr));
        CHECKF(p == NULL, "index %u: out pointer not cleared", idx);
        if (SUCCEEDED(hr)) SAFE_RELEASE(p);
    }
    SAFE_RELEASE(e);
    ifv_video_close(&v);
}

T(proc_create_basic_properties) {
    IFV_VP(v, e, p, 48, 32);
    EXPECT_EQ(d3dh_refcount(p), 1);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_IUnknown, 1), QE(IID_ID3D11DeviceChild, 1), QE(IID_ID3D11VideoProcessor, 1),
        QE(IID_ID3D11VideoProcessorEnumerator, 0), QE(IID_ID3D11Device, 0), QE(IID_ID3D11Resource, 0),
    };
    ifv_check_qi_table(p, tab, sizeof(tab) / sizeof(tab[0]), "video processor");
    ID3D11Device *g = NULL;
    ID3D11VideoProcessor_GetDevice(p, &g);
    CHECK(g == v.dev);
    SAFE_RELEASE(g);
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC want, got;
    ID3D11VideoProcessorEnumerator_GetVideoProcessorContentDesc(e, &want);
    memset(&got, 0xcc, sizeof(got));
    ID3D11VideoProcessor_GetContentDesc(p, &got);
    CHECK(!memcmp(&want, &got, sizeof(want)));
    EXPECT_EQ(got.InputWidth, 48); EXPECT_EQ(got.InputHeight, 32);
    D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS a, b;
    memset(&a, 0xcc, sizeof(a)); memset(&b, 0x33, sizeof(b));
    EXPECT_HR(ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(e, 0, &a), S_OK);
    ID3D11VideoProcessor_GetRateConversionCaps(p, &b);
    CHECK(!memcmp(&a, &b, sizeof(a)));
    /* the processor outlives its enumerator */
    SAFE_RELEASE(e);
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC again; memset(&again, 0xcc, sizeof(again));
    ID3D11VideoProcessor_GetContentDesc(p, &again);
    CHECK(!memcmp(&again, &want, sizeof(want)));
    SAFE_RELEASE(p);
    ifv_video_close(&v);
}

T(proc_create_multiple_distinct) {
    IFV_VP(v, e, p, 32, 32);
    ID3D11VideoProcessor *p2 = NULL, *p3 = NULL;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessor(v.vd, e, 0, &p2), S_OK);
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessor(v.vd, e, 0, &p3), S_OK);
    CHECK(p2 && p3 && p != p2 && p2 != p3 && p != p3);
    /* independent state */
    RECT r1 = { 1, 2, 11, 12 }, r2 = { 3, 4, 13, 14 }, g = {0};
    BOOL en = 0;
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p, TRUE, &r1);
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p2, TRUE, &r2);
    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(v.vc, p, &en, &g);
    CHECK(en && !memcmp(&g, &r1, sizeof(g)));
    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(v.vc, p2, &en, &g);
    CHECK(en && !memcmp(&g, &r2, sizeof(g)));
    en = 1;
    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(v.vc, p3, &en, &g);
    EXPECT_EQ(en, FALSE);
    SAFE_RELEASE(p3); SAFE_RELEASE(p2);
    IFV_VP_CLOSE(v, e, p);
}

/* ---------------------------------------------------------------- default state */

T(proc_state_defaults_output) {
    IFV_VP(v, e, p, 32, 32);
    BOOL en = 0x5a5a;
    RECT r; memset(&r, 0xcc, sizeof(r));
    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(v.vc, p, &en, &r);
    EXPECT_EQ(en, FALSE);
    CHECK(r.left == 0 && r.top == 0 && r.right == 0 && r.bottom == 0);
    D3D11_VIDEO_COLOR col; memset(&col, 0xcc, sizeof(col));
    en = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetOutputBackgroundColor(v.vc, p, &en, &col);
    EXPECT_EQ(en, FALSE);                                     /* the flag is "YCbCr", not "enable" */
    t_note("default background colour %g %g %g %g", col.RGBA.R, col.RGBA.G, col.RGBA.B, col.RGBA.A);
    D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE mode = 0x5a5a; UINT si = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetOutputAlphaFillMode(v.vc, p, &mode, &si);
    EXPECT_EQ(mode, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_OPAQUE);
    EXPECT_EQ(si, 0);
    SIZE sz = { 0x5a5a, 0x5a5a };
    en = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetOutputConstriction(v.vc, p, &en, &sz);
    EXPECT_EQ(en, FALSE);
    CHECK(sz.cx == 0 && sz.cy == 0);
    en = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetOutputStereoMode(v.vc, p, &en);
    EXPECT_EQ(en, FALSE);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_state_defaults_stream0) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    if (caps.MaxStreamStates == 0) { t_note("MaxStreamStates is 0"); IFV_VP_CLOSE(v, e, p); t_skip("no stream states"); return; }
    D3D11_VIDEO_FRAME_FORMAT ff = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(v.vc, p, 0, &ff);
    EXPECT_EQ(ff, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    D3D11_VIDEO_PROCESSOR_OUTPUT_RATE rate = 0x5a5a; BOOL rep = 0x5a5a; DXGI_RATIONAL cr = { 0x5a5a, 0x5a5a };
    ID3D11VideoContext_VideoProcessorGetStreamOutputRate(v.vc, p, 0, &rate, &rep, &cr);
    EXPECT_EQ(rate, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL);
    EXPECT_EQ(rep, FALSE);
    EXPECT_EQ(cr.Numerator, 1); EXPECT_EQ(cr.Denominator, 1);
    BOOL en = 0x5a5a; RECT r; memset(&r, 0xcc, sizeof(r));
    ID3D11VideoContext_VideoProcessorGetStreamSourceRect(v.vc, p, 0, &en, &r);
    EXPECT_EQ(en, FALSE);
    CHECK(!r.left && !r.top && !r.right && !r.bottom);
    en = 0x5a5a; memset(&r, 0xcc, sizeof(r));
    ID3D11VideoContext_VideoProcessorGetStreamDestRect(v.vc, p, 0, &en, &r);
    EXPECT_EQ(en, FALSE);
    CHECK(!r.left && !r.top && !r.right && !r.bottom);
    float alpha = -1.0f; en = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetStreamAlpha(v.vc, p, 0, &en, &alpha);
    EXPECT_EQ(en, FALSE);
    CHECK(alpha == 1.0f);
    float lo = -1, hi = -1; en = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetStreamLumaKey(v.vc, p, 0, &en, &lo, &hi);
    EXPECT_EQ(en, FALSE);
    CHECK(lo == 0.0f && hi == 0.0f);
    en = 0;
    ID3D11VideoContext_VideoProcessorGetStreamAutoProcessingMode(v.vc, p, 0, &en);
    EXPECT_EQ(en, TRUE);
    BOOL rot_en = 0x5a5a; D3D11_VIDEO_PROCESSOR_ROTATION rot = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetStreamRotation(v.vc, p, 0, &rot_en, &rot);
    EXPECT_EQ(rot_en, FALSE);
    EXPECT_EQ(rot, D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY);
    for (int f = 0; f < 8; f++) {
        BOOL fe = 0x5a5a; int lvl = 0x5a5a;
        ID3D11VideoContext_VideoProcessorGetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, &fe, &lvl);
        CHECKF(fe == FALSE && lvl == 0, "filter %d default: enabled %d level %d", f, fe, lvl);
    }
    IFV_VP_CLOSE(v, e, p);
}

T(proc_state_defaults_apply_to_all_streams) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    UINT n = caps.MaxStreamStates < 8 ? caps.MaxStreamStates : 8;
    if (!n) { IFV_VP_CLOSE(v, e, p); t_skip("no stream states"); return; }
    for (UINT s = 0; s < n; s++) {
        D3D11_VIDEO_FRAME_FORMAT ff = 0x5a5a; BOOL en = 0x5a5a; RECT r;
        ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(v.vc, p, s, &ff);
        CHECKF(ff == D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE, "stream %u frame format %d", s, ff);
        ID3D11VideoContext_VideoProcessorGetStreamSourceRect(v.vc, p, s, &en, &r);
        CHECKF(en == FALSE, "stream %u source rect enabled", s);
        en = 0;
        ID3D11VideoContext_VideoProcessorGetStreamAutoProcessingMode(v.vc, p, s, &en);
        CHECKF(en == TRUE, "stream %u auto processing %d", s, en);
    }
    IFV_VP_CLOSE(v, e, p);
}

/* ---------------------------------------------------------------- output state */

T(proc_output_target_rect_roundtrip) {
    IFV_VP(v, e, p, 64, 64);
    static const RECT rs[] = { {0, 0, 1, 1}, {1, 2, 30, 40}, {0, 0, 64, 64}, {-5, -7, 100, 200}, {10, 10, 10, 10} };
    for (unsigned i = 0; i < sizeof(rs) / sizeof(rs[0]); i++) {
        BOOL en = 0; RECT g; memset(&g, 0xcc, sizeof(g));
        ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p, TRUE, &rs[i]);
        ID3D11VideoContext_VideoProcessorGetOutputTargetRect(v.vc, p, &en, &g);
        CHECKF(en == TRUE, "rect #%u not enabled", i);
        CHECKF(!memcmp(&g, &rs[i], sizeof(g)), "rect #%u mismatch (%ld,%ld,%ld,%ld)", i, g.left, g.top, g.right, g.bottom);
    }
    /* disabling clears the stored rectangle, whatever was passed in */
    RECT junk = { 5, 5, 9, 9 }, g; BOOL en = 1;
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p, FALSE, &junk);
    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(v.vc, p, &en, &g);
    EXPECT_EQ(en, FALSE);
    CHECK(!g.left && !g.top && !g.right && !g.bottom);
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p, FALSE, NULL);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_output_target_rect_invalid_ignored) {
    IFV_VP(v, e, p, 64, 64);
    RECT ok = { 2, 3, 20, 30 }, bad_h = { 10, 0, 5, 10 }, bad_v = { 0, 10, 10, 5 }, g;
    BOOL en = 0;
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p, TRUE, &ok);
    /* rejected calls (inverted rectangle, missing rectangle) leave the previous state in place */
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p, TRUE, &bad_h);
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p, TRUE, &bad_v);
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, p, TRUE, NULL);
    memset(&g, 0xcc, sizeof(g));
    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(v.vc, p, &en, &g);
    EXPECT_EQ(en, TRUE);
    CHECK(!memcmp(&g, &ok, sizeof(g)));
    /* a NULL processor is reported and ignored, not dereferenced */
    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(v.vc, NULL, TRUE, &ok);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_output_background_color_roundtrip) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_COLOR c, g;
    memset(&c, 0, sizeof(c));
    c.RGBA.R = 0.25f; c.RGBA.G = 0.5f; c.RGBA.B = 0.75f; c.RGBA.A = 1.0f;
    BOOL ycbcr = 0x5a5a;
    ID3D11VideoContext_VideoProcessorSetOutputBackgroundColor(v.vc, p, FALSE, &c);
    memset(&g, 0xcc, sizeof(g));
    ID3D11VideoContext_VideoProcessorGetOutputBackgroundColor(v.vc, p, &ycbcr, &g);
    EXPECT_EQ(ycbcr, FALSE);
    CHECK(g.RGBA.R == 0.25f && g.RGBA.G == 0.5f && g.RGBA.B == 0.75f && g.RGBA.A == 1.0f);
    memset(&c, 0, sizeof(c));
    c.YCbCr.Y = 0.5f; c.YCbCr.Cb = 0.25f; c.YCbCr.Cr = 0.75f; c.YCbCr.A = 0.5f;
    ID3D11VideoContext_VideoProcessorSetOutputBackgroundColor(v.vc, p, TRUE, &c);
    memset(&g, 0xcc, sizeof(g));
    ID3D11VideoContext_VideoProcessorGetOutputBackgroundColor(v.vc, p, &ycbcr, &g);
    EXPECT_EQ(ycbcr, TRUE);
    CHECK(g.YCbCr.Y == 0.5f && g.YCbCr.Cb == 0.25f && g.YCbCr.Cr == 0.75f && g.YCbCr.A == 0.5f);
    /* alpha outside [0,1] or a NULL colour is rejected and keeps the previous value */
    D3D11_VIDEO_COLOR bad; memset(&bad, 0, sizeof(bad)); bad.RGBA.R = 1.0f; bad.RGBA.A = 2.0f;
    ID3D11VideoContext_VideoProcessorSetOutputBackgroundColor(v.vc, p, FALSE, &bad);
    bad.RGBA.A = -0.5f;
    ID3D11VideoContext_VideoProcessorSetOutputBackgroundColor(v.vc, p, FALSE, &bad);
    ID3D11VideoContext_VideoProcessorSetOutputBackgroundColor(v.vc, p, FALSE, NULL);
    memset(&g, 0xcc, sizeof(g));
    ID3D11VideoContext_VideoProcessorGetOutputBackgroundColor(v.vc, p, &ycbcr, &g);
    EXPECT_EQ(ycbcr, TRUE);
    CHECK(g.YCbCr.Y == 0.5f && g.YCbCr.A == 0.5f);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_output_alpha_fill_modes) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE m = 0x5a5a; UINT si = 0x5a5a;
    if (!(caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_ALPHA_FILL)) {
        t_note("ALPHA_FILL feature not reported (feature caps 0x%x)", caps.FeatureCaps);
        ID3D11VideoContext_VideoProcessorSetOutputAlphaFillMode(v.vc, p, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_OPAQUE, 0);
        ID3D11VideoContext_VideoProcessorGetOutputAlphaFillMode(v.vc, p, &m, &si);
        EXPECT_EQ(m, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_OPAQUE);
        IFV_VP_CLOSE(v, e, p);
        return;
    }
    static const int modes[] = { 0, 1, 2 };
    for (unsigned i = 0; i < 3; i++) {
        ID3D11VideoContext_VideoProcessorSetOutputAlphaFillMode(v.vc, p, (D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE)modes[i], 0);
        m = 0x5a5a; si = 0x5a5a;
        ID3D11VideoContext_VideoProcessorGetOutputAlphaFillMode(v.vc, p, &m, &si);
        CHECKF(m == modes[i], "mode %d read back as %d", modes[i], m);
    }
    ID3D11VideoContext_VideoProcessorSetOutputAlphaFillMode(v.vc, p, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_SOURCE_STREAM, 0);
    ID3D11VideoContext_VideoProcessorGetOutputAlphaFillMode(v.vc, p, &m, &si);
    EXPECT_EQ(m, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_SOURCE_STREAM);
    EXPECT_EQ(si, 0);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_output_constriction_and_stereo_gated) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    BOOL en = 0x5a5a; SIZE sz = {0};
    if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_CONSTRICTION) {
        SIZE want = { 16, 8 };
        ID3D11VideoContext_VideoProcessorSetOutputConstriction(v.vc, p, TRUE, want);
        ID3D11VideoContext_VideoProcessorGetOutputConstriction(v.vc, p, &en, &sz);
        EXPECT_EQ(en, TRUE); EXPECT_EQ(sz.cx, 16); EXPECT_EQ(sz.cy, 8);
    } else t_note("CONSTRICTION not reported");
    BOOL st = 0x5a5a;
    if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_STEREO) {
        ID3D11VideoContext_VideoProcessorSetOutputStereoMode(v.vc, p, TRUE);
        ID3D11VideoContext_VideoProcessorGetOutputStereoMode(v.vc, p, &st);
        EXPECT_EQ(st, TRUE);
        ID3D11VideoContext_VideoProcessorSetOutputStereoMode(v.vc, p, FALSE);
        ID3D11VideoContext_VideoProcessorGetOutputStereoMode(v.vc, p, &st);
        EXPECT_EQ(st, FALSE);
    } else t_note("STEREO not reported");
    IFV_VP_CLOSE(v, e, p);
}

T(proc_output_color_space_roundtrip) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE cs, g;
    memset(&cs, 0, sizeof(cs));
    cs.Usage = 1; cs.RGB_Range = 1; cs.YCbCr_Matrix = 1; cs.YCbCr_xvYCC = 0; cs.Nominal_Range = 2;
    ID3D11VideoContext_VideoProcessorSetOutputColorSpace(v.vc, p, &cs);
    memset(&g, 0xcc, sizeof(g));
    ID3D11VideoContext_VideoProcessorGetOutputColorSpace(v.vc, p, &g);
    t_note("output colour space set 0x%x read back 0x%x", NZ_COLOR_SPACE_BITS(cs), NZ_COLOR_SPACE_BITS(g));
    CHECK(g.Reserved == 0 || g.Reserved == 0x3ffffff);
    ID3D11VideoContext_VideoProcessorSetOutputColorSpace(v.vc, p, NULL);   /* rejected, no crash */
    IFV_VP_CLOSE(v, e, p);
}

/* ---------------------------------------------------------------- stream state */

T(proc_stream_frame_format_roundtrip) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    if (!caps.MaxStreamStates) { IFV_VP_CLOSE(v, e, p); t_skip("no stream states"); return; }
    for (int f = 0; f < 3; f++) {
        D3D11_VIDEO_FRAME_FORMAT g = 0x5a5a;
        ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(v.vc, p, 0, (D3D11_VIDEO_FRAME_FORMAT)f);
        ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(v.vc, p, 0, &g);
        CHECKF(g == f, "frame format %d read back as %d", f, g);
    }
    /* out-of-range formats are rejected without touching the stored value */
    static const int bad[] = { 3, 4, -1, 0x7fffffff };
    ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(v.vc, p, 0, D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST);
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        D3D11_VIDEO_FRAME_FORMAT g = 0x5a5a;
        ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(v.vc, p, 0, (D3D11_VIDEO_FRAME_FORMAT)bad[i]);
        ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(v.vc, p, 0, &g);
        CHECKF(g == D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST, "bad format %d changed the state to %d", bad[i], g);
    }
    IFV_VP_CLOSE(v, e, p);
}

T(proc_stream_output_rate_roundtrip) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    if (!caps.MaxStreamStates) { IFV_VP_CLOSE(v, e, p); t_skip("no stream states"); return; }
    D3D11_VIDEO_PROCESSOR_OUTPUT_RATE r = 0x5a5a; BOOL rep = 0x5a5a; DXGI_RATIONAL cr = { 0x5a5a, 0x5a5a };
    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(v.vc, p, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_HALF, TRUE, NULL);
    ID3D11VideoContext_VideoProcessorGetStreamOutputRate(v.vc, p, 0, &r, &rep, &cr);
    EXPECT_EQ(r, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_HALF);
    EXPECT_EQ(rep, FALSE);                                /* RepeatFrame only applies to NORMAL/HALF in this runtime: stored as 0 */
    EXPECT_EQ(cr.Numerator, 1); EXPECT_EQ(cr.Denominator, 1);
    DXGI_RATIONAL want = { 3, 2 };
    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(v.vc, p, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM, TRUE, &want);
    r = 0x5a5a; rep = 0x5a5a; memset(&cr, 0, sizeof(cr));
    ID3D11VideoContext_VideoProcessorGetStreamOutputRate(v.vc, p, 0, &r, &rep, &cr);
    EXPECT_EQ(r, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM);
    EXPECT_EQ(rep, TRUE);
    EXPECT_EQ(cr.Numerator, 3); EXPECT_EQ(cr.Denominator, 2);
    /* a custom rate needs a non-zero rational */
    DXGI_RATIONAL zero_num = { 0, 5 }, zero_den = { 5, 0 };
    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(v.vc, p, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM, FALSE, &zero_num);
    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(v.vc, p, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM, FALSE, &zero_den);
    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(v.vc, p, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM, FALSE, NULL);
    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_OUTPUT_RATE)3, FALSE, &want);
    r = 0x5a5a; rep = 0x5a5a;
    ID3D11VideoContext_VideoProcessorGetStreamOutputRate(v.vc, p, 0, &r, &rep, &cr);
    EXPECT_EQ(r, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM);
    EXPECT_EQ(rep, TRUE);
    EXPECT_EQ(cr.Numerator, 3); EXPECT_EQ(cr.Denominator, 2);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_stream_rects_roundtrip) {
    IFV_VP(v, e, p, 64, 64);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    if (!caps.MaxStreamStates) { IFV_VP_CLOSE(v, e, p); t_skip("no stream states"); return; }
    RECT src = { 4, 6, 40, 50 }, dst = { 0, 0, 32, 32 }, g;
    BOOL en = 0;
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(v.vc, p, 0, TRUE, &src);
    ID3D11VideoContext_VideoProcessorSetStreamDestRect(v.vc, p, 0, TRUE, &dst);
    memset(&g, 0xcc, sizeof(g));
    ID3D11VideoContext_VideoProcessorGetStreamSourceRect(v.vc, p, 0, &en, &g);
    EXPECT_EQ(en, TRUE); CHECK(!memcmp(&g, &src, sizeof(g)));
    memset(&g, 0xcc, sizeof(g)); en = 0;
    ID3D11VideoContext_VideoProcessorGetStreamDestRect(v.vc, p, 0, &en, &g);
    EXPECT_EQ(en, TRUE); CHECK(!memcmp(&g, &dst, sizeof(g)));
    /* the two rectangles are independent */
    RECT src2 = { 1, 1, 2, 2 };
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(v.vc, p, 0, TRUE, &src2);
    ID3D11VideoContext_VideoProcessorGetStreamDestRect(v.vc, p, 0, &en, &g);
    CHECK(!memcmp(&g, &dst, sizeof(g)));
    /* inverted / missing rectangles are rejected */
    RECT inv = { 9, 9, 3, 3 };
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(v.vc, p, 0, TRUE, &inv);
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(v.vc, p, 0, TRUE, NULL);
    ID3D11VideoContext_VideoProcessorGetStreamSourceRect(v.vc, p, 0, &en, &g);
    EXPECT_EQ(en, TRUE); CHECK(!memcmp(&g, &src2, sizeof(g)));
    /* disabling zeroes the stored rectangle */
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(v.vc, p, 0, FALSE, &src);
    memset(&g, 0xcc, sizeof(g));
    ID3D11VideoContext_VideoProcessorGetStreamSourceRect(v.vc, p, 0, &en, &g);
    EXPECT_EQ(en, FALSE);
    CHECK(!g.left && !g.top && !g.right && !g.bottom);
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(v.vc, NULL, 0, TRUE, &src);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_stream_alpha_validation) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    if (!caps.MaxStreamStates) { IFV_VP_CLOSE(v, e, p); t_skip("no stream states"); return; }
    float a = -1.0f; BOOL en = 0x5a5a;
    if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_ALPHA_STREAM) {
        ID3D11VideoContext_VideoProcessorSetStreamAlpha(v.vc, p, 0, TRUE, 0.25f);
        ID3D11VideoContext_VideoProcessorGetStreamAlpha(v.vc, p, 0, &en, &a);
        EXPECT_EQ(en, TRUE); CHECK(a == 0.25f);
        /* out-of-range alpha is ignored */
        ID3D11VideoContext_VideoProcessorSetStreamAlpha(v.vc, p, 0, TRUE, 1.5f);
        ID3D11VideoContext_VideoProcessorSetStreamAlpha(v.vc, p, 0, TRUE, -0.1f);
        ID3D11VideoContext_VideoProcessorGetStreamAlpha(v.vc, p, 0, &en, &a);
        CHECK(a == 0.25f);
        /* disabling resets the stored alpha to 1.0 */
        ID3D11VideoContext_VideoProcessorSetStreamAlpha(v.vc, p, 0, FALSE, 0.5f);
        ID3D11VideoContext_VideoProcessorGetStreamAlpha(v.vc, p, 0, &en, &a);
        EXPECT_EQ(en, FALSE); CHECK(a == 1.0f);
    } else {
        t_note("ALPHA_STREAM not reported: enabling stream alpha must be refused");
        ID3D11VideoContext_VideoProcessorSetStreamAlpha(v.vc, p, 0, TRUE, 0.25f);
        ID3D11VideoContext_VideoProcessorGetStreamAlpha(v.vc, p, 0, &en, &a);
        EXPECT_EQ(en, FALSE); CHECK(a == 1.0f);
    }
    IFV_VP_CLOSE(v, e, p);
}

T(proc_stream_filters_gated_by_caps) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    if (!caps.MaxStreamStates) { IFV_VP_CLOSE(v, e, p); t_skip("no stream states"); return; }
    int exercised = 0;
    for (int f = 0; f < 8; f++) {
        BOOL en = 0x5a5a; int lvl = 0x5a5a;
        if (caps.FilterCaps & (1u << f)) {
            D3D11_VIDEO_PROCESSOR_FILTER_RANGE r;
            if (FAILED(ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(e, (D3D11_VIDEO_PROCESSOR_FILTER)f, &r))) continue;
            exercised++;
            ID3D11VideoContext_VideoProcessorSetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, TRUE, r.Maximum);
            ID3D11VideoContext_VideoProcessorGetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, &en, &lvl);
            CHECKF(en == TRUE && lvl == r.Maximum, "filter %d: enabled %d level %d (max %d)", f, en, lvl, r.Maximum);
            ID3D11VideoContext_VideoProcessorSetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, TRUE, r.Maximum + 1);   /* above range: refused */
            ID3D11VideoContext_VideoProcessorGetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, &en, &lvl);
            CHECKF(lvl == r.Maximum, "filter %d: out-of-range level accepted (%d)", f, lvl);
            ID3D11VideoContext_VideoProcessorSetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, FALSE, 0);
            ID3D11VideoContext_VideoProcessorGetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, &en, &lvl);
            CHECKF(en == FALSE, "filter %d still enabled", f);
        } else {
            /* enabling a filter the processor does not report is refused */
            ID3D11VideoContext_VideoProcessorSetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, TRUE, 0);
            ID3D11VideoContext_VideoProcessorGetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)f, &en, &lvl);
            CHECKF(en == FALSE, "unsupported filter %d was enabled", f);
        }
    }
    t_note("%d filters exercised (caps 0x%x)", exercised, caps.FilterCaps);
    BOOL en = 0x5a5a; int lvl = 0x5a5a;
    ID3D11VideoContext_VideoProcessorSetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)8, TRUE, 0);   /* invalid enum: refused */
    ID3D11VideoContext_VideoProcessorGetStreamFilter(v.vc, p, 0, (D3D11_VIDEO_PROCESSOR_FILTER)8, &en, &lvl);
    CHECK(en == 0x5a5a && lvl == 0x5a5a);        /* the getter also refuses and writes nothing */
    IFV_VP_CLOSE(v, e, p);
}

T(proc_stream_misc_state_roundtrip) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    if (!caps.MaxStreamStates) { IFV_VP_CLOSE(v, e, p); t_skip("no stream states"); return; }
    BOOL en = 0x5a5a;
    ID3D11VideoContext_VideoProcessorSetStreamAutoProcessingMode(v.vc, p, 0, FALSE);
    ID3D11VideoContext_VideoProcessorGetStreamAutoProcessingMode(v.vc, p, 0, &en);
    EXPECT_EQ(en, FALSE);
    ID3D11VideoContext_VideoProcessorSetStreamAutoProcessingMode(v.vc, p, 0, TRUE);
    ID3D11VideoContext_VideoProcessorGetStreamAutoProcessingMode(v.vc, p, 0, &en);
    EXPECT_EQ(en, TRUE);
    float lo = -1, hi = -1;
    if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_LUMA_KEY) {
        ID3D11VideoContext_VideoProcessorSetStreamLumaKey(v.vc, p, 0, TRUE, 0.125f, 0.875f);
        ID3D11VideoContext_VideoProcessorGetStreamLumaKey(v.vc, p, 0, &en, &lo, &hi);
        EXPECT_EQ(en, TRUE); CHECK(lo == 0.125f && hi == 0.875f);
    } else t_note("LUMA_KEY not reported");
    if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_PIXEL_ASPECT_RATIO) {
        DXGI_RATIONAL s = { 4, 3 }, d = { 16, 9 }, gs = {0}, gd = {0};
        ID3D11VideoContext_VideoProcessorSetStreamPixelAspectRatio(v.vc, p, 0, TRUE, &s, &d);
        ID3D11VideoContext_VideoProcessorGetStreamPixelAspectRatio(v.vc, p, 0, &en, &gs, &gd);
        EXPECT_EQ(en, TRUE); EXPECT_EQ(gs.Numerator, 4); EXPECT_EQ(gs.Denominator, 3); EXPECT_EQ(gd.Numerator, 16); EXPECT_EQ(gd.Denominator, 9);
    } else t_note("PIXEL_ASPECT_RATIO not reported");
    if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_ROTATION) {
        D3D11_VIDEO_PROCESSOR_ROTATION rot = 0x5a5a;
        ID3D11VideoContext_VideoProcessorSetStreamRotation(v.vc, p, 0, TRUE, D3D11_VIDEO_PROCESSOR_ROTATION_90);
        ID3D11VideoContext_VideoProcessorGetStreamRotation(v.vc, p, 0, &en, &rot);
        EXPECT_EQ(en, TRUE); EXPECT_EQ(rot, D3D11_VIDEO_PROCESSOR_ROTATION_90);
    } else t_note("ROTATION not reported");
    if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_STEREO) {
        D3D11_VIDEO_PROCESSOR_STEREO_FORMAT fmt = 0x5a5a; D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE flip = 0x5a5a; BOOL l = 0x5a5a, b = 0x5a5a; int off = 0x5a5a;
        ID3D11VideoContext_VideoProcessorSetStreamStereoFormat(v.vc, p, 0, TRUE, D3D11_VIDEO_PROCESSOR_STEREO_FORMAT_HORIZONTAL, TRUE, FALSE, D3D11_VIDEO_PROCESSOR_STEREO_FLIP_NONE, 0);
        ID3D11VideoContext_VideoProcessorGetStreamStereoFormat(v.vc, p, 0, &en, &fmt, &l, &b, &flip, &off);
        t_note("stereo format roundtrip: en %d fmt %d left0 %d base0 %d flip %d off %d", en, fmt, l, b, flip, off);
    } else t_note("STEREO not reported");
    IFV_VP_CLOSE(v, e, p);
}

T(proc_stream_mirror_and_colorspace1) {
    IFV_VP(v, e, p, 32, 32);
    ID3D11VideoContext1 *vc1 = NULL;
    if (FAILED(ID3D11VideoContext_QueryInterface(v.vc, &IID_ID3D11VideoContext1, (void **)&vc1))) { IFV_VP_CLOSE(v, e, p); t_skip("no ID3D11VideoContext1"); return; }
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    DXGI_COLOR_SPACE_TYPE cs = 0x5a5a;
    ID3D11VideoContext1_VideoProcessorSetOutputColorSpace1(vc1, p, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    ID3D11VideoContext1_VideoProcessorGetOutputColorSpace1(vc1, p, &cs);
    EXPECT_EQ(cs, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    ID3D11VideoContext1_VideoProcessorSetOutputColorSpace1(vc1, p, DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709);
    ID3D11VideoContext1_VideoProcessorGetOutputColorSpace1(vc1, p, &cs);
    EXPECT_EQ(cs, DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709);
    if (caps.MaxStreamStates) {
        cs = 0x5a5a;
        ID3D11VideoContext1_VideoProcessorSetStreamColorSpace1(vc1, p, 0, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709);
        ID3D11VideoContext1_VideoProcessorGetStreamColorSpace1(vc1, p, 0, &cs);
        EXPECT_EQ(cs, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709);
        if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_MIRROR) {
            BOOL en = 0x5a5a, h = 0x5a5a, vv = 0x5a5a;
            ID3D11VideoContext1_VideoProcessorSetStreamMirror(vc1, p, 0, TRUE, TRUE, FALSE);
            ID3D11VideoContext1_VideoProcessorGetStreamMirror(vc1, p, 0, &en, &h, &vv);
            EXPECT_EQ(en, TRUE); EXPECT_EQ(h, TRUE); EXPECT_EQ(vv, FALSE);
        } else t_note("MIRROR not reported");
    }
    if (caps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_SHADER_USAGE) {
        BOOL su = 0x5a5a;
        ID3D11VideoContext1_VideoProcessorSetOutputShaderUsage(vc1, p, TRUE);
        ID3D11VideoContext1_VideoProcessorGetOutputShaderUsage(vc1, p, &su);
        EXPECT_EQ(su, TRUE);
    } else t_note("SHADER_USAGE not reported");
    ID3D11VideoContext1_Release(vc1);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_stream_index_out_of_range_getters_untouched) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    UINT bad = caps.MaxStreamStates + 100;
    D3D11_VIDEO_FRAME_FORMAT ff = 0x5a5a; BOOL en = 0x5a5a; RECT r; memset(&r, 0xcc, sizeof(r));
    ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(v.vc, p, bad, &ff);
    EXPECT_EQ(ff, 0x5a5a);
    ID3D11VideoContext_VideoProcessorGetStreamSourceRect(v.vc, p, bad, &en, &r);
    EXPECT_EQ(en, 0x5a5a);
    ID3D11VideoContext_VideoProcessorGetStreamAutoProcessingMode(v.vc, p, 0xffffffffu, &en);
    EXPECT_EQ(en, 0x5a5a);
    /* setters with a bad stream index are ignored */
    ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(v.vc, p, bad, D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST);
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(v.vc, p, bad, TRUE, &r);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_extension_argument_validation) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    unsigned char buf[16] = {0};
    EXPECT_HR(ID3D11VideoContext_VideoProcessorSetOutputExtension(v.vc, p, NULL, sizeof(buf), buf), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorSetOutputExtension(v.vc, p, &ifv_IID_Random, sizeof(buf), NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorSetOutputExtension(v.vc, p, &ifv_IID_Random, 0, buf), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorGetOutputExtension(v.vc, p, NULL, sizeof(buf), buf), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorGetOutputExtension(v.vc, p, &ifv_IID_Random, sizeof(buf), NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorGetOutputExtension(v.vc, p, &ifv_IID_Random, 0, buf), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorSetStreamExtension(v.vc, p, 0, NULL, sizeof(buf), buf), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorSetStreamExtension(v.vc, p, 0, &ifv_IID_Random, sizeof(buf), NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorSetStreamExtension(v.vc, p, 0, &ifv_IID_Random, 0, buf), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_VideoProcessorSetStreamExtension(v.vc, p, caps.MaxStreamStates + 50, &ifv_IID_Random, sizeof(buf), buf), E_INVALIDARG);
    /* with valid arguments the request reaches the driver, which does not know a random extension GUID */
    HRESULT hr = ID3D11VideoContext_VideoProcessorSetOutputExtension(v.vc, p, &ifv_IID_Random, sizeof(buf), buf);
    t_note("SetOutputExtension(random GUID) -> %s", d3dh_hr_name(hr));
    hr = ID3D11VideoContext_VideoProcessorGetOutputExtension(v.vc, p, &ifv_IID_Random, sizeof(buf), buf);
    t_note("GetOutputExtension(random GUID) -> %s", d3dh_hr_name(hr));
    IFV_VP_CLOSE(v, e, p);
}

/* ---------------------------------------------------------------- views and blt */

T(proc_input_view_argument_validation) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_TEXTURE2D_DESC td = { 32, 32, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0, 0 };
    ID3D11Texture2D *t = NULL;
    ID3D11Device_CreateTexture2D(v.dev, &td, NULL, &t);
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC vd; memset(&vd, 0, sizeof(vd));
    vd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    ID3D11VideoProcessorInputView *iv = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorInputView(v.vd, NULL, e, &vd, &iv), E_INVALIDARG);
    CHECK(iv == NULL);
    if (t) {
        iv = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorInputView(v.vd, (ID3D11Resource *)t, e, NULL, &iv), E_INVALIDARG);
        CHECK(iv == NULL);
        iv = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorInputView(v.vd, (ID3D11Resource *)t, NULL, &vd, &iv), E_INVALIDARG);
        CHECK(iv == NULL);
        static const int bad[] = { 0, 2, 3, 100 };
        for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
            D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC b = vd;
            b.ViewDimension = (D3D11_VPIV_DIMENSION)bad[i];
            iv = (void *)(ULONG_PTR)1;
            HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorInputView(v.vd, (ID3D11Resource *)t, e, &b, &iv);
            CHECKF(hr == E_INVALIDARG, "input view dimension %d -> %s", bad[i], d3dh_hr_name(hr));
            CHECKF(iv == NULL, "dimension %d: out pointer not cleared", bad[i]);
        }
        SAFE_RELEASE(t);
    }
    D3D11_BUFFER_DESC bd = { 256, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, 0 };
    ID3D11Buffer *b = NULL;
    if (SUCCEEDED(ID3D11Device_CreateBuffer(v.dev, &bd, NULL, &b))) {
        iv = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorInputView(v.vd, (ID3D11Resource *)b, e, &vd, &iv), E_INVALIDARG);
        CHECK(iv == NULL);
        SAFE_RELEASE(b);
    }
    IFV_VP_CLOSE(v, e, p);
}

T(proc_output_view_argument_validation) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_TEXTURE2D_DESC td = { 32, 32, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
    ID3D11Texture2D *rt = NULL, *nort = NULL;
    ID3D11Device_CreateTexture2D(v.dev, &td, NULL, &rt);
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Device_CreateTexture2D(v.dev, &td, NULL, &nort);
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC vd; memset(&vd, 0, sizeof(vd));
    vd.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ID3D11VideoProcessorOutputView *ov = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorOutputView(v.vd, NULL, e, &vd, &ov), E_INVALIDARG);
    CHECK(ov == NULL);
    if (rt) {
        ov = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorOutputView(v.vd, (ID3D11Resource *)rt, e, NULL, &ov), E_INVALIDARG);
        CHECK(ov == NULL);
        ov = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorOutputView(v.vd, (ID3D11Resource *)rt, NULL, &vd, &ov), E_INVALIDARG);
        CHECK(ov == NULL);
        static const int bad[] = { 0, 3, 100, -1 };
        for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
            D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC b = vd;
            b.ViewDimension = (D3D11_VPOV_DIMENSION)bad[i];
            ov = (void *)(ULONG_PTR)1;
            HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(v.vd, (ID3D11Resource *)rt, e, &b, &ov);
            CHECKF(hr == E_INVALIDARG, "output view dimension %d -> %s", bad[i], d3dh_hr_name(hr));
            CHECKF(ov == NULL, "dimension %d: out pointer not cleared", bad[i]);
        }
    }
    if (nort) {
        ov = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoProcessorOutputView(v.vd, (ID3D11Resource *)nort, e, &vd, &ov), E_INVALIDARG);
        CHECK(ov == NULL);
    }
    SAFE_RELEASE(nort); SAFE_RELEASE(rt);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_output_view_created_properties) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_TEXTURE2D_DESC td = { 32, 32, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
    ID3D11Texture2D *rt = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(v.dev, &td, NULL, &rt))) { IFV_VP_CLOSE(v, e, p); t_skip("no render target"); return; }
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC vd; memset(&vd, 0, sizeof(vd));
    vd.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ID3D11VideoProcessorOutputView *ov = NULL;
    HRESULT hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(v.vd, (ID3D11Resource *)rt, e, &vd, &ov);
    t_note("CreateVideoProcessorOutputView(BGRA RT) -> %s", d3dh_hr_name(hr));
    if (ov) {
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC g; memset(&g, 0xcc, sizeof(g));
        ID3D11VideoProcessorOutputView_GetDesc(ov, &g);
        EXPECT_EQ(g.ViewDimension, D3D11_VPOV_DIMENSION_TEXTURE2D);
        EXPECT_EQ(g.Texture2D.MipSlice, 0);
        ID3D11Resource *res = NULL;
        ID3D11VideoProcessorOutputView_GetResource(ov, &res);
        CHECK(res == (ID3D11Resource *)rt);
        SAFE_RELEASE(res);
        EXPECT_HR(ifv_qi(ov, &IID_ID3D11View, NULL), S_OK);
        EXPECT_HR(ifv_qi(ov, &IID_ID3D11RenderTargetView, NULL), E_NOINTERFACE);
        SAFE_RELEASE(ov);
    }
    SAFE_RELEASE(rt);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_blt_argument_validation) {
    IFV_VP(v, e, p, 32, 32);
    D3D11_VIDEO_PROCESSOR_CAPS caps = ifv_caps(e);
    D3D11_VIDEO_PROCESSOR_STREAM st; memset(&st, 0, sizeof(st));
    /* no output view */
    EXPECT_HR(ID3D11VideoContext_VideoProcessorBlt(v.vc, p, NULL, 0, 1, &st), E_INVALIDARG);
    /* more streams than the processor accepts: checked before anything is read from the stream array */
    EXPECT_HR(ID3D11VideoContext_VideoProcessorBlt(v.vc, p, NULL, 0, caps.MaxInputStreams + 1, NULL), E_INVALIDARG);
    ifv_video_close(&v);
    SAFE_RELEASE(p); SAFE_RELEASE(e);
    (void)v;
}

static int ifv_nv12_ok(ID3D11Device *d) {
    UINT s = 0;
    return SUCCEEDED(ID3D11Device_CheckFormatSupport(d, DXGI_FORMAT_NV12, &s)) && (s & D3D11_FORMAT_SUPPORT_TEXTURE2D);
}

T(proc_blt_nv12_to_bgra) {
    IFV_VP(v, e, p, 16, 16);
    UINT fl = 0;
    ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(e, DXGI_FORMAT_NV12, &fl);
    UINT fo = 0;
    ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(e, DXGI_FORMAT_B8G8R8A8_UNORM, &fo);
    t_note("NV12 input support %d, BGRA output support %d", !!(fl & 1), !!(fo & 2));
    if (!ifv_nv12_ok(v.dev) || !(fl & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) || !(fo & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) {
        IFV_VP_CLOSE(v, e, p); t_skip("NV12 -> BGRA not supported by this device"); return;
    }
    unsigned char y[16 * 16], uv[16 * 8];
    memset(y, 0x80, sizeof(y)); memset(uv, 0x80, sizeof(uv));
    /* one contiguous NV12 surface: 16 rows of Y followed by 8 rows of interleaved UV */
    unsigned char surf[16 * 24];
    memcpy(surf, y, sizeof(y)); memcpy(surf + sizeof(y), uv, sizeof(uv));
    D3D11_SUBRESOURCE_DATA sd = { surf, 16, 0 };
    D3D11_TEXTURE2D_DESC td = { 16, 16, 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    ID3D11Texture2D *in = NULL, *out = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(v.dev, &td, &sd, &in);
    t_note("NV12 texture -> %s", d3dh_hr_name(hr));
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    HRESULT hr2 = ID3D11Device_CreateTexture2D(v.dev, &td, NULL, &out);
    if (!in || !out) { SAFE_RELEASE(in); SAFE_RELEASE(out); IFV_VP_CLOSE(v, e, p); t_skip("cannot create NV12 / BGRA textures (%s / %s)", d3dh_hr_name(hr), d3dh_hr_name(hr2)); return; }
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivd; memset(&ivd, 0, sizeof(ivd)); ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovd; memset(&ovd, 0, sizeof(ovd)); ovd.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ID3D11VideoProcessorInputView *iv = NULL; ID3D11VideoProcessorOutputView *ov = NULL;
    hr = ID3D11VideoDevice_CreateVideoProcessorInputView(v.vd, (ID3D11Resource *)in, e, &ivd, &iv);
    hr2 = ID3D11VideoDevice_CreateVideoProcessorOutputView(v.vd, (ID3D11Resource *)out, e, &ovd, &ov);
    t_note("views: input %s, output %s", d3dh_hr_name(hr), d3dh_hr_name(hr2));
    if (iv && ov) {
        D3D11_VIDEO_PROCESSOR_STREAM st; memset(&st, 0, sizeof(st));
        st.Enable = TRUE; st.pInputSurface = iv;
        hr = ID3D11VideoContext_VideoProcessorBlt(v.vc, p, ov, 0, 1, &st);
        EXPECT_HR(hr, S_OK);
        ID3D11DeviceContext_Flush(v.ctx);
        d3dh_image im;
        if (SUCCEEDED(d3dh_readback_texture2d11(v.dev, v.ctx, out, 0, &im))) {
            unsigned px = d3dh_image_pixel32(&im, 8, 8);
            t_note("mid grey NV12 -> BGRA pixel(8,8) = 0x%08x", px);
            unsigned b = px & 0xff, g = (px >> 8) & 0xff, r = (px >> 16) & 0xff, a = px >> 24;
            t_note("  B=%u G=%u R=%u A=%u", b, g, r, a);
            d3dh_image_free(&im);
        }
        /* a stream with Enable = FALSE draws nothing but is still a valid call */
        st.Enable = FALSE;
        hr = ID3D11VideoContext_VideoProcessorBlt(v.vc, p, ov, 0, 1, &st);
        t_note("Blt with a disabled stream -> %s", d3dh_hr_name(hr));
    } else t_skip("views could not be created");
    SAFE_RELEASE(ov); SAFE_RELEASE(iv); SAFE_RELEASE(out); SAFE_RELEASE(in);
    IFV_VP_CLOSE(v, e, p);
}

T(proc_blt_bgra_identity) {
    IFV_VP(v, e, p, 8, 8);
    UINT fi = 0, fo = 0;
    ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(e, DXGI_FORMAT_B8G8R8A8_UNORM, &fi);
    fo = fi;
    if (!(fi & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) || !(fo & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) {
        IFV_VP_CLOSE(v, e, p); t_skip("BGRA not usable as both input and output"); return;
    }
    unsigned px[64];
    for (int i = 0; i < 64; i++) px[i] = 0xff000000u | (unsigned)(i * 3) << 16 | 0x4000u | 0x20u;
    D3D11_SUBRESOURCE_DATA sd = { px, 32, 0 };
    D3D11_TEXTURE2D_DESC td = { 8, 8, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    ID3D11Texture2D *in = NULL, *out = NULL;
    ID3D11Device_CreateTexture2D(v.dev, &td, &sd, &in);
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Device_CreateTexture2D(v.dev, &td, NULL, &out);
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivd; memset(&ivd, 0, sizeof(ivd)); ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovd; memset(&ovd, 0, sizeof(ovd)); ovd.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ID3D11VideoProcessorInputView *iv = NULL; ID3D11VideoProcessorOutputView *ov = NULL;
    if (in && out) {
        ID3D11VideoDevice_CreateVideoProcessorInputView(v.vd, (ID3D11Resource *)in, e, &ivd, &iv);
        ID3D11VideoDevice_CreateVideoProcessorOutputView(v.vd, (ID3D11Resource *)out, e, &ovd, &ov);
    }
    if (iv && ov) {
        D3D11_VIDEO_PROCESSOR_STREAM st; memset(&st, 0, sizeof(st));
        st.Enable = TRUE; st.pInputSurface = iv;
        HRESULT hr = ID3D11VideoContext_VideoProcessorBlt(v.vc, p, ov, 0, 1, &st);
        EXPECT_HR(hr, S_OK);
        d3dh_image im;
        if (SUCCEEDED(d3dh_readback_texture2d11(v.dev, v.ctx, out, 0, &im))) {
            int same = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) same += d3dh_image_pixel32(&im, x, y) == px[y * 8 + x];
            t_note("same-size BGRA blt: %d/64 pixels identical", same);
            d3dh_image_free(&im);
        }
    } else t_skip("views not available for BGRA -> BGRA");
    SAFE_RELEASE(ov); SAFE_RELEASE(iv); SAFE_RELEASE(out); SAFE_RELEASE(in);
    IFV_VP_CLOSE(v, e, p);
}
