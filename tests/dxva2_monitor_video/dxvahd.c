#include "dxm.h"
/* DXVAHD_CreateDevice and IDXVAHD_Device. A DXVA-HD capable driver is rarely present on a virtual machine, so
 * everything that needs a live device is conditional; the description validation is hard-checked. */

#define E_INVALIDARG_ ((HRESULT)0x80070057)

typedef HRESULT (WINAPI *create_hd_fn)(IDirect3DDevice9Ex *, const DXVAHD_CONTENT_DESC *, DXVAHD_DEVICE_USAGE, PDXVAHDSW_Plugin, IDXVAHD_Device **);
static create_hd_fn hd_create(void) { return (create_hd_fn)t_proc_in("dxva2.dll", "DXVAHD_CreateDevice"); }

static DXVAHD_CONTENT_DESC good_desc(void)
{
    DXVAHD_CONTENT_DESC d; memset(&d, 0, sizeof d);
    d.InputFrameFormat = DXVAHD_FRAME_FORMAT_PROGRESSIVE;
    d.InputFrameRate.Numerator = 30; d.InputFrameRate.Denominator = 1;
    d.InputWidth = 640; d.InputHeight = 480;
    d.OutputFrameRate.Numerator = 30; d.OutputFrameRate.Denominator = 1;
    d.OutputWidth = 640; d.OutputHeight = 480;
    return d;
}

/* Every invalid description is E_INVALIDARG, the device refcount is unchanged and the output pointer untouched. */
static void expect_invalid(DXVAHD_CONTENT_DESC *desc, DXVAHD_DEVICE_USAGE usage, const char *what)
{
    create_hd_fn c = hd_create();
    DXM_D9X(d);
    ULONG rc = d3dh_refcount(d.dev);
    IDXVAHD_Device *dev = (IDXVAHD_Device *)(ULONG_PTR)1;
    HRESULT hr = c(d.dev, desc, usage, NULL, &dev);
    CHECKF(hr == E_INVALIDARG_, "%s: got %s", what, d3dh_hr_name(hr));
    CHECKF(dev == (IDXVAHD_Device *)(ULONG_PTR)1, "%s: output pointer written", what);
    CHECKF(d3dh_refcount(d.dev) == rc, "%s: device refcount leaked", what);
    DXM_D9X_END(d);
}
#define HD_INVALID_TEST(id, mutate, usage) \
TEST(dxva2_monitor_video_hd_invalid_##id) { \
    REQUIRE(hd_create()); \
    DXVAHD_CONTENT_DESC desc = good_desc(); mutate; \
    expect_invalid(&desc, usage, #id); \
}
HD_INVALID_TEST(frame_format_3, desc.InputFrameFormat = (DXVAHD_FRAME_FORMAT)3, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)
HD_INVALID_TEST(frame_format_big, desc.InputFrameFormat = (DXVAHD_FRAME_FORMAT)0xFFFFFFFF, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)
HD_INVALID_TEST(input_rate_zero_denominator, desc.InputFrameRate.Denominator = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)
HD_INVALID_TEST(output_rate_zero_denominator, desc.OutputFrameRate.Denominator = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)
HD_INVALID_TEST(input_width_zero, desc.InputWidth = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)
HD_INVALID_TEST(input_height_zero, desc.InputHeight = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)
HD_INVALID_TEST(output_width_zero, desc.OutputWidth = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)
HD_INVALID_TEST(output_height_zero, desc.OutputHeight = 0, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)
HD_INVALID_TEST(usage_3, (void)0, (DXVAHD_DEVICE_USAGE)3)
HD_INVALID_TEST(usage_big, (void)0, (DXVAHD_DEVICE_USAGE)0x7FFFFFFF)
HD_INVALID_TEST(all_zero, memset(&desc, 0, sizeof desc), DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL)

TEST(dxva2_monitor_video_hd_zero_rates_not_rejected_as_rate) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    DXVAHD_CONTENT_DESC desc = good_desc();
    desc.InputFrameRate.Numerator = 0; desc.InputFrameRate.Denominator = 0;   /* 0/0 means "unspecified" and is valid */
    IDXVAHD_Device *dev = NULL;
    HRESULT hr = c(d.dev, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &dev);
    if (hr == E_INVALIDARG_) t_note("0/0 input frame rate rejected");
    if (SUCCEEDED(hr) && dev) IDXVAHD_Device_Release(dev);
    DXM_D9X_END(d);
}
TEST(dxva2_monitor_video_hd_valid_usages) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    for (int u = 0; u < 3; u++) {
        DXVAHD_CONTENT_DESC desc = good_desc();
        IDXVAHD_Device *dev = NULL;
        ULONG rc = d3dh_refcount(d.dev);
        HRESULT hr = c(d.dev, &desc, (DXVAHD_DEVICE_USAGE)u, NULL, &dev);
        if (SUCCEEDED(hr)) { CHECK(dev != NULL); IDXVAHD_Device_Release(dev); }
        else { CHECK(dev == NULL); t_note("usage %d -> %s (no DXVA-HD capable driver)", u, d3dh_hr_name(hr)); }
        EXPECT_EQ(d3dh_refcount(d.dev), rc);
    }
    DXM_D9X_END(d);
}
TEST(dxva2_monitor_video_hd_device_refcount_and_qi) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    DXVAHD_CONTENT_DESC desc = good_desc();
    IDXVAHD_Device *dev = NULL;
    if (FAILED(c(d.dev, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &dev)) || !dev) { t_skip("no DXVA-HD device on this machine"); DXM_D9X_END(d); return; }
    EXPECT_EQ(IDXVAHD_Device_AddRef(dev), 2);
    EXPECT_EQ(IDXVAHD_Device_Release(dev), 1);
    void *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDXVAHD_Device_QueryInterface(dev, &DXM_IID_Bogus, &p), (HRESULT)0x80004002);
    CHECK(p == NULL);
    IDXVAHD_Device_Release(dev);
    DXM_D9X_END(d);
}
TEST(dxva2_monitor_video_hd_device_caps_consistency) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    DXVAHD_CONTENT_DESC desc = good_desc();
    IDXVAHD_Device *dev = NULL;
    if (FAILED(c(d.dev, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &dev)) || !dev) { t_skip("no DXVA-HD device on this machine"); DXM_D9X_END(d); return; }
    DXVAHD_VPDEVCAPS caps; memset(&caps, 0, sizeof caps);
    EXPECT_HR(IDXVAHD_Device_GetVideoProcessorDeviceCaps(dev, &caps), S_OK);
    /* the creation code rejects a device with no processors, output formats or input formats */
    CHECK(caps.VideoProcessorCount > 0);
    CHECK(caps.OutputFormatCount > 0);
    CHECK(caps.InputFormatCount > 0);
    t_note("DXVA-HD device type %d, %u processors, %u in / %u out formats, max streams %u, max input %u",
           caps.DeviceType, caps.VideoProcessorCount, caps.InputFormatCount, caps.OutputFormatCount, caps.MaxStreamStates, caps.MaxInputStreams);
    IDXVAHD_Device_Release(dev);
    DXM_D9X_END(d);
}
TEST(dxva2_monitor_video_hd_formats_count_protocol) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    DXVAHD_CONTENT_DESC desc = good_desc();
    IDXVAHD_Device *dev = NULL;
    if (FAILED(c(d.dev, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &dev)) || !dev) { t_skip("no DXVA-HD device on this machine"); DXM_D9X_END(d); return; }
    DXVAHD_VPDEVCAPS caps; IDXVAHD_Device_GetVideoProcessorDeviceCaps(dev, &caps);
    D3DFORMAT *f = (D3DFORMAT *)calloc(caps.OutputFormatCount + caps.InputFormatCount + 2, sizeof *f);
    if (f) {
        /* the count passed in must equal the device's count exactly */
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorOutputFormats(dev, caps.OutputFormatCount, f), S_OK);
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorOutputFormats(dev, caps.OutputFormatCount + 1, f), E_INVALIDARG_);
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorOutputFormats(dev, caps.OutputFormatCount - 1, f), E_INVALIDARG_);
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorInputFormats(dev, caps.InputFormatCount, f), S_OK);
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorInputFormats(dev, caps.InputFormatCount + 1, f), E_INVALIDARG_);
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorInputFormats(dev, 0, f), caps.InputFormatCount == 0 ? S_OK : E_INVALIDARG_);
        free(f);
    }
    IDXVAHD_Device_Release(dev);
    DXM_D9X_END(d);
}
TEST(dxva2_monitor_video_hd_vpcaps_count_protocol) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    DXVAHD_CONTENT_DESC desc = good_desc();
    IDXVAHD_Device *dev = NULL;
    if (FAILED(c(d.dev, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &dev)) || !dev) { t_skip("no DXVA-HD device on this machine"); DXM_D9X_END(d); return; }
    DXVAHD_VPDEVCAPS caps; IDXVAHD_Device_GetVideoProcessorDeviceCaps(dev, &caps);
    DXVAHD_VPCAPS *vp = (DXVAHD_VPCAPS *)calloc(caps.VideoProcessorCount + 2, sizeof *vp);
    if (vp) {
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorCaps(dev, caps.VideoProcessorCount, vp), S_OK);
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorCaps(dev, caps.VideoProcessorCount + 1, vp), E_INVALIDARG_);
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorCaps(dev, 0, vp), caps.VideoProcessorCount == 0 ? S_OK : E_INVALIDARG_);
        for (UINT i = 0; i < caps.VideoProcessorCount; i++) {
            for (UINT j = i + 1; j < caps.VideoProcessorCount; j++) CHECK(!IsEqualGUID(&vp[i].VPGuid, &vp[j].VPGuid));
            /* each processor can be instantiated */
            IDXVAHD_VideoProcessor *p = NULL;
            HRESULT hr = IDXVAHD_Device_CreateVideoProcessor(dev, &vp[i].VPGuid, &p);
            if (SUCCEEDED(hr)) { CHECK(p != NULL); if (p) IDXVAHD_VideoProcessor_Release(p); }
            else t_note("CreateVideoProcessor(%s) -> %s", d3dh_guid_str(&vp[i].VPGuid), d3dh_hr_name(hr));
        }
        free(vp);
    }
    IDXVAHD_Device_Release(dev);
    DXM_D9X_END(d);
}
TEST(dxva2_monitor_video_hd_filter_range_selectors) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    DXVAHD_CONTENT_DESC desc = good_desc();
    IDXVAHD_Device *dev = NULL;
    if (FAILED(c(d.dev, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &dev)) || !dev) { t_skip("no DXVA-HD device on this machine"); DXM_D9X_END(d); return; }
    DXVAHD_VPDEVCAPS caps; IDXVAHD_Device_GetVideoProcessorDeviceCaps(dev, &caps);
    static const UINT invalid[] = { 7, 8, 100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; i++) {
        DXVAHD_FILTER_RANGE_DATA r;
        EXPECT_HR(IDXVAHD_Device_GetVideoProcessorFilterRange(dev, (DXVAHD_FILTER)invalid[i], &r), E_INVALIDARG_);
    }
    for (UINT f = 0; f < 7; f++) {
        DXVAHD_FILTER_RANGE_DATA r; memset(&r, 0, sizeof r);
        HRESULT hr = IDXVAHD_Device_GetVideoProcessorFilterRange(dev, (DXVAHD_FILTER)f, &r);
        if (caps.FilterCaps & (1u << f)) {
            EXPECT_HR(hr, S_OK);
            CHECK(r.Maximum >= r.Minimum);
            CHECK(r.Default >= r.Minimum && r.Default <= r.Maximum);
            CHECK(r.Multiplier > 0.0f);
        } else EXPECT_HR(hr, E_INVALIDARG_);    /* an unsupported filter is rejected */
    }
    IDXVAHD_Device_Release(dev);
    DXM_D9X_END(d);
}
TEST(dxva2_monitor_video_hd_custom_rates_zero_count) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    DXVAHD_CONTENT_DESC desc = good_desc();
    IDXVAHD_Device *dev = NULL;
    if (FAILED(c(d.dev, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &dev)) || !dev) { t_skip("no DXVA-HD device on this machine"); DXM_D9X_END(d); return; }
    DXVAHD_VPDEVCAPS caps; IDXVAHD_Device_GetVideoProcessorDeviceCaps(dev, &caps);
    DXVAHD_VPCAPS *vp = (DXVAHD_VPCAPS *)calloc(caps.VideoProcessorCount + 1, sizeof *vp);
    if (vp && SUCCEEDED(IDXVAHD_Device_GetVideoProcessorCaps(dev, caps.VideoProcessorCount, vp)) && caps.VideoProcessorCount) {
        DXVAHD_CUSTOM_RATE_DATA *rates = (DXVAHD_CUSTOM_RATE_DATA *)calloc(vp[0].CustomRateCount + 1, sizeof *rates);
        if (rates) {
            HRESULT hr = IDXVAHD_Device_GetVideoProcessorCustomRates(dev, &vp[0].VPGuid, vp[0].CustomRateCount, rates);
            if (vp[0].CustomRateCount == 0) t_note("processor 0 has no custom rates -> %s", d3dh_hr_name(hr));
            else for (UINT i = 0; SUCCEEDED(hr) && i < vp[0].CustomRateCount; i++) { CHECK(rates[i].CustomRate.Denominator != 0); CHECK(rates[i].OutputFrames != 0); }
            free(rates);
        }
    }
    free(vp);
    IDXVAHD_Device_Release(dev);
    DXM_D9X_END(d);
}
TEST(dxva2_monitor_video_hd_create_unknown_processor) {
    create_hd_fn c = hd_create(); REQUIRE(c);
    DXM_D9X(d);
    DXVAHD_CONTENT_DESC desc = good_desc();
    IDXVAHD_Device *dev = NULL;
    if (FAILED(c(d.dev, &desc, DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL, NULL, &dev)) || !dev) { t_skip("no DXVA-HD device on this machine"); DXM_D9X_END(d); return; }
    IDXVAHD_VideoProcessor *p = (IDXVAHD_VideoProcessor *)(ULONG_PTR)1;
    HRESULT hr = IDXVAHD_Device_CreateVideoProcessor(dev, &DXM_IID_Bogus, &p);
    CHECK(FAILED(hr));
    if (FAILED(hr)) CHECK(p == (IDXVAHD_VideoProcessor *)(ULONG_PTR)1);
    IDXVAHD_Device_Release(dev);
    DXM_D9X_END(d);
}
