/* Win10 22H2 pins found in the oracle review pass: limits, format table, argument validation, creation flags, info queue defaults.
 * Everything here was measured on the oracle (WARP only); WARP-lenient results are guarded with dd_warp(). */
#include "dd.h"

#define T(n) TEST(d3d10_device_extra_##n)
#define RGBA DXGI_FORMAT_R8G8B8A8_UNORM
#define NEED10() REQUIRE(DD_C10())
#define NEED_DRV() do { if (dd_driver() < 0) { t_skip("no D3D10 device can be created"); return; } } while (0)

static ID3D10Device *x_dev; static HRESULT x_hr;
static void x_cfs_null(void *c) { UINT f = (UINT)(ULONG_PTR)c; x_hr = ID3D10Device_CheckFormatSupport(x_dev, (DXGI_FORMAT)f, NULL); }

T(format_support_range)
{
    DD_DEV(dev);
    for (UINT f = 0; f <= 130; f++) {
        UINT s = 0xdeadbeef; HRESULT hr = ID3D10Device_CheckFormatSupport(dev, (DXGI_FORMAT)f, &s);
        /* D3D10 knows formats 0..65 and 67..84; 66 (R1... gap) and everything from 85 up (B5G6R5, BGRA, 16-bit, video formats) is E_FAIL */
        int known = f <= 65 || (f >= 67 && f <= 84);
        t_check(known ? hr == S_OK : hr == E_FAIL, __FILE__, __LINE__, "format table", "format %u: %s", f, d3dh_hr_name(hr));
        if (!known) CHECKF(s == 0, "format %u support 0x%x", f, s);
    }
    static const UINT far_[] = { 131, 200, 255, 256, 1000, 0xffffffffu };
    for (unsigned i = 0; i < sizeof far_ / sizeof *far_; i++) {
        UINT s = 0xdeadbeef, q = 77;
        EXPECT_HR(ID3D10Device_CheckFormatSupport(dev, (DXGI_FORMAT)far_[i], &s), E_FAIL); EXPECT_EQ(s, 0);
        EXPECT_HR(ID3D10Device_CheckMultisampleQualityLevels(dev, (DXGI_FORMAT)far_[i], 4, &q), E_INVALIDARG); EXPECT_EQ(q, 77);
    }
    DD_END(dev);
}
T(format_support_null_out_faults)
{
    DD_DEV(dev);
    x_dev = dev; x_hr = S_OK;
    EXPECT_EQ(t_try(x_cfs_null, (void *)(ULONG_PTR)RGBA), 0xc0000005u);   /* Win10 writes the support mask unconditionally (probed) */
    DD_END(dev);
}
T(msaa_quality_levels_matrix)
{
    DD_DEV(dev);
    UINT q = 77;
    static const UINT cnt[] = { 1, 2, 4, 8, 16 };   /* each reports exactly one quality level for RGBA8 on WARP, 32 reports none */
    for (unsigned i = 0; i < 5; i++) { q = 77; EXPECT_HR(ID3D10Device_CheckMultisampleQualityLevels(dev, RGBA, cnt[i], &q), S_OK); if (dd_warp()) EXPECT_EQ(q, 1); }
    q = 77; EXPECT_HR(ID3D10Device_CheckMultisampleQualityLevels(dev, RGBA, 32, &q), S_OK); EXPECT_EQ(q, 0);
    q = 77; EXPECT_HR(ID3D10Device_CheckMultisampleQualityLevels(dev, RGBA, 0, &q), E_FAIL); EXPECT_EQ(q, 0);
    EXPECT_HR(ID3D10Device_CheckMultisampleQualityLevels(dev, RGBA, 4, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(tex2d_sample_desc_matrix)
{
    DD_DEV(dev);
    static const UINT counts[] = { 0, 1, 2, 4, 8, 16, 32, 33 };
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++) {
        for (UINT quality = 0; quality < 2; quality++) {
            D3D10_TEXTURE2D_DESC d = { 16, 16, 1, 1, RGBA, { counts[i], quality }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET, 0, 0 };
            HRESULT hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, NULL);
            /* count 1 accepts any quality value, counts 2..16 need quality 0 (< levels reported), 0 / 32 / 33 are rejected */
            HRESULT want = (counts[i] == 1) ? S_FALSE : (counts[i] >= 2 && counts[i] <= 16 && quality == 0) ? S_FALSE : E_INVALIDARG;
            t_check(hr == want, __FILE__, __LINE__, "sample desc", "count %u quality %u: %s", counts[i], quality, d3dh_hr_name(hr));
        }
    }
    DD_END(dev);
}
T(tex_size_limits_validate)
{
    DD_DEV(dev);
    static const UINT ws[] = { 8192, 8193, 16384, 16385, 32768, 65535, 65536, 131072 };
    for (unsigned i = 0; i < sizeof ws / sizeof *ws; i++) {
        D3D10_TEXTURE2D_DESC d = { ws[i], 1, 1, 1, DXGI_FORMAT_R8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 };
        HRESULT w = ID3D10Device_CreateTexture2D(dev, &d, NULL, NULL); d.Width = 1; d.Height = ws[i]; HRESULT h = ID3D10Device_CreateTexture2D(dev, &d, NULL, NULL);
        D3D10_TEXTURE1D_DESC e = { ws[i], 1, 1, DXGI_FORMAT_R8_UNORM, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 }; HRESULT o = ID3D10Device_CreateTexture1D(dev, &e, NULL, NULL);
        /* documented D3D10 limit is 8192 everywhere; Win10 WARP validates 2D up to at least 131072 (probed) but 1D stays at 8192 */
        HRESULT w2 = (ws[i] <= 8192 || dd_warp()) ? S_FALSE : E_INVALIDARG;
        t_check(w == w2 && h == w2, __FILE__, __LINE__, "2D size", "%u: w %s h %s", ws[i], d3dh_hr_name(w), d3dh_hr_name(h));
        t_check(o == (ws[i] <= 8192 ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "1D size", "%u: %s", ws[i], d3dh_hr_name(o));
    }
    D3D10_TEXTURE2D_DESC d = { 0x7fffffff, 1, 1, 1, DXGI_FORMAT_R8_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 };
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &d, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(tex2d_validate_null_args)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 4, 4, 1, 1, RGBA, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 };
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &d, NULL, NULL), S_FALSE);   /* NULL output = validate only */
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, NULL, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, NULL, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(bgra_formats_unsupported_either_way)
{
    /* D3D10 has no BGRA texture support at all, with or without the 0x20 creation flag, and the flag is not echoed by GetCreationFlags (probed) */
    for (int fl = 0; fl < 2; fl++) {
        ID3D10Device *d = NULL; NEED10(); NEED_DRV();
        if (FAILED(dd_make(fl ? D3D10_CREATE_DEVICE_BGRA_SUPPORT : 0, &d)) || !d) { t_skip("device"); return; }
        EXPECT_EQ(ID3D10Device_GetCreationFlags(d), 0);
        static const DXGI_FORMAT fm[] = { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G5R5A1_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB };
        for (unsigned i = 0; i < 5; i++) {
            D3D10_TEXTURE2D_DESC td = { 4, 4, 1, 1, fm[i], { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 };
            ID3D10Texture2D *t = (void *)1; HRESULT hr = ID3D10Device_CreateTexture2D(d, &td, NULL, &t);
            t_check(hr == E_INVALIDARG && t == NULL, __FILE__, __LINE__, "BGRA texture", "flag %d fmt %u: %s", fl, fm[i], d3dh_hr_name(hr));
        }
        ID3D10Device_Release(d);
    }
}
T(creation_flags_echo)
{
    NEED10(); NEED_DRV();
    /* the four documented behaviour flags are echoed; ALLOW_NULL_FROM_MAP, STRICT_VALIDATION, BGRA and undefined bits are dropped (probed) */
    static const struct { UINT in, out; } c[] = {
        { D3D10_CREATE_DEVICE_SINGLETHREADED, 1 }, { D3D10_CREATE_DEVICE_DEBUG, 2 }, { D3D10_CREATE_DEVICE_SWITCH_TO_REF, 4 },
        { D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS, 8 }, { D3D10_CREATE_DEVICE_ALLOW_NULL_FROM_MAP, 0 },
        { 0x100, 0 }, { 0x80000000u, 0 }, { 1 | 8, 9 },
    };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        ID3D10Device *d = NULL; HRESULT hr = dd_make(c[i].in, &d);
        if (hr == (HRESULT)0x887A002D) continue;   /* DEBUG without the Graphics Tools feature */
        t_check(SUCCEEDED(hr) && d, __FILE__, __LINE__, "create", "flags 0x%x: %s", c[i].in, d3dh_hr_name(hr));
        if (!d) continue;
        UINT got = ID3D10Device_GetCreationFlags(d);
        t_check(got == c[i].out, __FILE__, __LINE__, "GetCreationFlags", "in 0x%x got 0x%x want 0x%x", c[i].in, got, c[i].out);
        EXPECT_HR(ID3D10Device_GetDeviceRemovedReason(d), S_OK);
        ID3D10Device_Release(d);
    }
}
T(infoqueue_app_message_severities)
{
    ID3D10Device *dev = NULL; ID3D10InfoQueue *iq = NULL;
    NEED10(); NEED_DRV();
    HRESULT hr = dd_make(D3D10_CREATE_DEVICE_DEBUG, &dev);
    if (FAILED(hr) || !dev) { t_skip("no debug layer: %s", d3dh_hr_name(hr)); return; }
    if (FAILED(ID3D10Device_QueryInterface(dev, &IID_ID3D10InfoQueue, (void **)&iq)) || !iq) { ID3D10Device_Release(dev); t_skip("no InfoQueue"); return; }
    EXPECT_EQ(ID3D10InfoQueue_GetMessageCountLimit(iq), 1024);
    EXPECT_EQ(ID3D10InfoQueue_GetStorageFilterStackSize(iq), 1);     /* the default filter that drops INFO */
    EXPECT_EQ(ID3D10InfoQueue_GetRetrievalFilterStackSize(iq), 0);
    EXPECT_EQ(ID3D10InfoQueue_GetMuteDebugOutput(iq), 0);
    for (int sev = D3D10_MESSAGE_SEVERITY_CORRUPTION; sev <= D3D10_MESSAGE_SEVERITY_INFO; sev++) {
        ID3D10InfoQueue_ClearStoredMessages(iq);
        HRESULT h = ID3D10InfoQueue_AddApplicationMessage(iq, (D3D10_MESSAGE_SEVERITY)sev, "d3d10_device severity probe");
        if (sev == D3D10_MESSAGE_SEVERITY_INFO) { EXPECT_HR(h, S_FALSE); EXPECT_EQ(ID3D10InfoQueue_GetNumStoredMessages(iq), 0); }
        else { EXPECT_HR(h, S_OK); EXPECT_EQ(ID3D10InfoQueue_GetNumStoredMessages(iq), 1); }
    }
    ID3D10InfoQueue_ClearStoredMessages(iq);
    ID3D10InfoQueue_Release(iq); ID3D10Device_Release(dev);
}
