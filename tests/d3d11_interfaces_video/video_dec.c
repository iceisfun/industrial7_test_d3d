/* ID3D11VideoDevice / ID3D11VideoContext: QI ladder, decoder profile enumeration, decoder creation and buffer
 * APIs without a real decode, content protection and crypto sessions. */
#include "ifv.h"

#define QE(iid, w) { &iid, #iid, w }

static const GUID k_h264_vld = {0x1b81be68, 0xa0c7, 0x11d3, {0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5}};

/* Fetches the profile list of the device (up to 256 entries). */
static UINT ifv_profiles(ID3D11VideoDevice *vd, GUID *out, UINT cap) {
    UINT n = ID3D11VideoDevice_GetVideoDecoderProfileCount(vd);
    if (n > cap) n = cap;
    for (UINT i = 0; i < n; i++) if (FAILED(ID3D11VideoDevice_GetVideoDecoderProfile(vd, i, &out[i]))) return i;
    return n;
}

T(video_qi_device_ladder) {
    IFV_DEV(d, c);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_ID3D11VideoDevice, -1), QE(IID_ID3D11VideoDevice1, -1), QE(ifv_IID_VideoDevice2, -1),
        QE(IID_ID3D11VideoContext, 0), QE(IID_ID3D11VideoProcessorEnumerator, 0), QE(IID_ID3D11VideoDecoder, 0),
        QE(IID_ID3D11CryptoSession, 0),
    };
    ifv_check_qi_table(d, tab, sizeof(tab) / sizeof(tab[0]), "device");
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(video_qi_identity_and_refcount) {
    IFV_VIDEO(v);
    CHECK(ifv_identity(v.vd) == ifv_identity(v.dev));
    CHECK(ifv_identity(v.vc) == ifv_identity(v.ctx));
    ULONG before = d3dh_refcount(v.dev);
    ID3D11VideoDevice1 *vd1 = NULL;
    HRESULT hr = ID3D11VideoDevice_QueryInterface(v.vd, &IID_ID3D11VideoDevice1, (void **)&vd1);
    t_note("ID3D11VideoDevice1 -> %s", d3dh_hr_name(hr));
    if (vd1) {
        CHECK(ifv_identity(vd1) == ifv_identity(v.dev));
        EXPECT_EQ(d3dh_refcount(v.dev), before + 1);
        SAFE_RELEASE(vd1);
    }
    EXPECT_EQ(d3dh_refcount(v.dev), before);
    /* The video interfaces route back to the same device / context. */
    ID3D11Device *back = NULL;
    EXPECT_HR(ID3D11VideoDevice_QueryInterface(v.vd, &IID_ID3D11Device, (void **)&back), S_OK);
    CHECK(back == v.dev);
    SAFE_RELEASE(back);
    ID3D11DeviceContext *cb = NULL;
    EXPECT_HR(ID3D11VideoContext_QueryInterface(v.vc, &IID_ID3D11DeviceContext, (void **)&cb), S_OK);
    CHECK(cb == v.ctx);
    SAFE_RELEASE(cb);
    ID3D11Device *g = NULL;
    ID3D11VideoContext_GetDevice(v.vc, &g);
    CHECK(g == v.dev);
    SAFE_RELEASE(g);
    ifv_video_close(&v);
}

T(video_qi_context_ladder) {
    IFV_VIDEO(v);
    static const struct ifv_qi_ent tab[] = {
        QE(IID_ID3D11VideoContext1, -1), QE(IID_ID3D11VideoContext2, -1), QE(ifv_IID_VideoContext3, -1),
        QE(IID_ID3D11DeviceContext, 1), QE(IID_ID3D11DeviceContext1, 1), QE(IID_ID3D11VideoDevice, 0),
        QE(IID_ID3D11Device, 0),
    };
    ifv_check_qi_table(v.vc, tab, sizeof(tab) / sizeof(tab[0]), "video context");
    ifv_video_close(&v);
}

T(video_deferred_context_has_video_context) {
    IFV_DEV(d, c);
    ID3D11DeviceContext *dc = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(d, 0, &dc))) { t_skip("no deferred"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    HRESULT hr = ifv_qi(dc, &IID_ID3D11VideoContext, NULL);
    t_note("deferred context QI ID3D11VideoContext -> %s", d3dh_hr_name(hr));
    SAFE_RELEASE(dc); SAFE_RELEASE(c); SAFE_RELEASE(d);
}

T(video_support_flag_driver_matrix) {
    __typeof__(&D3D11CreateDevice) create = d3dh_D3D11CreateDevice();
    REQUIRE(create);
    ID3D11Device *d = (void *)(ULONG_PTR)1;
    /* a NULL adapter with an UNKNOWN or SOFTWARE type (and no software module) is invalid whatever the flags */
    HRESULT hr = create(NULL, D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &d, NULL, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    d = (void *)(ULONG_PTR)1;
    hr = create(NULL, D3D_DRIVER_TYPE_SOFTWARE, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &d, NULL, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    static const D3D_DRIVER_TYPE types[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_NULL };
    static const char *names[] = { "HARDWARE", "WARP", "REFERENCE", "NULL" };
    for (unsigned i = 0; i < 4; i++) {
        d = NULL;
        hr = create(NULL, types[i], NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0, D3D11_SDK_VERSION, &d, NULL, NULL);
        t_note("VIDEO_SUPPORT with %s -> %s", names[i], d3dh_hr_name(hr));
        if (types[i] != D3D_DRIVER_TYPE_HARDWARE) EXPECT_HR(hr, DXGI_ERROR_UNSUPPORTED);   /* only HARDWARE can carry video */
        else EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr) && d) {
            ID3D11VideoDevice *vd = NULL;
            /* HARDWARE accepts VIDEO_SUPPORT, yet the Basic Render Driver exposes no ID3D11VideoDevice (QI -> E_NOINTERFACE) while
             * ID3D11VideoContext still answers on the context; a real GPU exposes both. */
            EXPECT_HR(ID3D11Device_QueryInterface(d, &IID_ID3D11VideoDevice, (void **)&vd), ifv_is_brd(d) ? E_NOINTERFACE : S_OK);
            if (ifv_is_brd(d)) CHECK(vd == NULL);
            if (vd) { t_note("  decoder profile count %u", ID3D11VideoDevice_GetVideoDecoderProfileCount(vd)); SAFE_RELEASE(vd); }
            CHECK((ID3D11Device_GetCreationFlags(d) & D3D11_CREATE_DEVICE_VIDEO_SUPPORT) != 0);
            SAFE_RELEASE(d);
        } else CHECK(d == NULL);
    }
}

T(video_dec_profile_count_stable) {
    IFV_VIDEO(v);
    UINT n = ID3D11VideoDevice_GetVideoDecoderProfileCount(v.vd);
    t_note("decoder profile count %u (feature level 0x%x)", n, v.fl);
    for (int i = 0; i < 5; i++) EXPECT_EQ(ID3D11VideoDevice_GetVideoDecoderProfileCount(v.vd), n);
    ifv_video_close(&v);
}

T(video_dec_profile_listing) {
    IFV_VIDEO(v);
    GUID g[256];
    UINT n = ifv_profiles(v.vd, g, 256);
    UINT cnt = ID3D11VideoDevice_GetVideoDecoderProfileCount(v.vd);
    EXPECT_EQ(n, cnt > 256 ? 256 : cnt);
    for (UINT i = 0; i < n; i++) {
        if (!memcmp(&g[i], &GUID_NULL, sizeof(GUID))) t_note("profile %u is GUID_NULL", i);
        for (UINT j = i + 1; j < n; j++) if (!memcmp(&g[i], &g[j], sizeof(GUID))) t_note("profiles %u and %u are identical", i, j);
        t_note("profile[%u] = %s", i, d3dh_guid_str(&g[i]));
    }
    /* listing twice is stable */
    GUID g2[256];
    UINT n2 = ifv_profiles(v.vd, g2, 256);
    EXPECT_EQ(n2, n);
    CHECK(!memcmp(g, g2, n * sizeof(GUID)));
    ifv_video_close(&v);
}

T(video_dec_profile_invalid_arguments) {
    IFV_VIDEO(v);
    UINT n = ID3D11VideoDevice_GetVideoDecoderProfileCount(v.vd);
    GUID g = ifv_IID_Random;
    /* a NULL output is rejected before the index is looked at */
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderProfile(v.vd, 0, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderProfile(v.vd, n, NULL), E_INVALIDARG);
    static const UINT64 off[] = { 1, 2, 100, 0x7fffffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof(off) / sizeof(off[0]); i++) {
        UINT idx = (UINT)(n + off[i]);
        if (idx <= n) continue;      /* wrapped around */
        g = ifv_IID_Random;
        HRESULT hr = ID3D11VideoDevice_GetVideoDecoderProfile(v.vd, idx, &g);
        CHECKF(hr == E_INVALIDARG, "profile index %u of %u -> %s", idx, n, d3dh_hr_name(hr));
        CHECK(!memcmp(&g, &ifv_IID_Random, sizeof(GUID)));    /* output untouched on failure */
    }
    ifv_video_close(&v);
}

T(video_dec_check_format_null_arguments) {
    IFV_VIDEO(v);
    BOOL sup = 0x5a5a;
    EXPECT_HR(ID3D11VideoDevice_CheckVideoDecoderFormat(v.vd, &k_h264_vld, DXGI_FORMAT_NV12, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CheckVideoDecoderFormat(v.vd, NULL, DXGI_FORMAT_NV12, &sup), E_INVALIDARG);
    EXPECT_EQ(sup, 0x5a5a);
    ifv_video_close(&v);
}

T(video_dec_check_format_unknown_profile) {
    IFV_VIDEO(v);
    BOOL sup = 0x5a5a;
    /* profiles the device does not list are rejected, not reported as "unsupported" */
    EXPECT_HR(ID3D11VideoDevice_CheckVideoDecoderFormat(v.vd, &ifv_IID_Random, DXGI_FORMAT_NV12, &sup), E_INVALIDARG);
    EXPECT_EQ(sup, 0x5a5a);
    EXPECT_HR(ID3D11VideoDevice_CheckVideoDecoderFormat(v.vd, &GUID_NULL, DXGI_FORMAT_NV12, &sup), E_INVALIDARG);
    EXPECT_EQ(sup, 0x5a5a);
    ifv_video_close(&v);
}

T(video_dec_check_format_matrix) {
    IFV_VIDEO(v);
    GUID g[256];
    UINT n = ifv_profiles(v.vd, g, 256);
    if (!n) { t_note("no decoder profiles on this device"); ifv_video_close(&v); t_skip("no decoder profiles"); return; }
    static const DXGI_FORMAT fmts[] = { DXGI_FORMAT_NV12, DXGI_FORMAT_P010, DXGI_FORMAT_YUY2, DXGI_FORMAT_AYUV,
        DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32B32A32_FLOAT };
    for (UINT i = 0; i < n; i++) for (unsigned f = 0; f < sizeof(fmts) / sizeof(fmts[0]); f++) {
        BOOL sup = 0x5a5a;
        HRESULT hr = ID3D11VideoDevice_CheckVideoDecoderFormat(v.vd, &g[i], fmts[f], &sup);
        CHECKF(hr == S_OK, "profile %u format %d -> %s", i, fmts[f], d3dh_hr_name(hr));
        CHECKF(sup == 0 || sup == 1, "profile %u format %d: supported = %d", i, fmts[f], sup);
        if (sup) t_note("profile %s supports format %d", d3dh_guid_str(&g[i]), fmts[f]);
    }
    ifv_video_close(&v);
}

T(video_dec_config_count_unknown_profile) {
    IFV_VIDEO(v);
    D3D11_VIDEO_DECODER_DESC desc = { ifv_IID_Random, 64, 64, DXGI_FORMAT_NV12 };
    UINT cnt = 0x5a5a;
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfigCount(v.vd, &desc, &cnt), E_INVALIDARG);
    EXPECT_EQ(cnt, 0);                      /* the count is cleared before the description is validated */
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfigCount(v.vd, NULL, &cnt), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfigCount(v.vd, &desc, NULL), E_INVALIDARG);
    ifv_video_close(&v);
}

T(video_dec_config_count_zero_size) {
    IFV_VIDEO(v);
    GUID g[256];
    UINT n = ifv_profiles(v.vd, g, 256);
    UINT cnt = 0x5a5a;
    D3D11_VIDEO_DECODER_DESC desc = { n ? g[0] : k_h264_vld, 0, 64, DXGI_FORMAT_NV12 };
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfigCount(v.vd, &desc, &cnt), E_INVALIDARG);
    desc.SampleWidth = 64; desc.SampleHeight = 0; cnt = 0x5a5a;
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfigCount(v.vd, &desc, &cnt), E_INVALIDARG);
    ifv_video_close(&v);
}

T(video_dec_config_enumeration) {
    IFV_VIDEO(v);
    GUID g[256];
    UINT n = ifv_profiles(v.vd, g, 256), tested = 0;
    for (UINT i = 0; i < n; i++) {
        BOOL sup = 0;
        if (FAILED(ID3D11VideoDevice_CheckVideoDecoderFormat(v.vd, &g[i], DXGI_FORMAT_NV12, &sup)) || !sup) continue;
        D3D11_VIDEO_DECODER_DESC desc = { g[i], 1920, 1088, DXGI_FORMAT_NV12 };
        UINT cnt = 0x5a5a;
        HRESULT hr = ID3D11VideoDevice_GetVideoDecoderConfigCount(v.vd, &desc, &cnt);
        t_note("profile %s: config count %u (%s)", d3dh_guid_str(&g[i]), cnt, d3dh_hr_name(hr));
        EXPECT_HR(hr, S_OK);
        for (UINT k = 0; k < cnt && k < 8; k++) {
            D3D11_VIDEO_DECODER_CONFIG cfg; memset(&cfg, 0xcc, sizeof(cfg));
            EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfig(v.vd, &desc, k, &cfg), S_OK);
        }
        D3D11_VIDEO_DECODER_CONFIG cfg; memset(&cfg, 0xcc, sizeof(cfg));
        EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfig(v.vd, &desc, cnt, &cfg), E_INVALIDARG);
        static const unsigned char zero[sizeof(D3D11_VIDEO_DECODER_CONFIG)] = {0};
        CHECK(!memcmp(&cfg, zero, sizeof(cfg)));     /* the output is cleared before the index check */
        tested++;
    }
    if (!tested) { ifv_video_close(&v); t_skip("no NV12 capable decoder profile"); return; }
    ifv_video_close(&v);
}

T(video_dec_config_invalid_arguments) {
    IFV_VIDEO(v);
    D3D11_VIDEO_DECODER_DESC desc = { ifv_IID_Random, 64, 64, DXGI_FORMAT_NV12 };
    D3D11_VIDEO_DECODER_CONFIG cfg;
    memset(&cfg, 0xcc, sizeof(cfg));
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfig(v.vd, NULL, 0, &cfg), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfig(v.vd, &desc, 0, NULL), E_INVALIDARG);
    memset(&cfg, 0xcc, sizeof(cfg));
    EXPECT_HR(ID3D11VideoDevice_GetVideoDecoderConfig(v.vd, &desc, 0, &cfg), E_INVALIDARG);
    static const unsigned char zero[sizeof(D3D11_VIDEO_DECODER_CONFIG)] = {0};
    CHECK(!memcmp(&cfg, zero, sizeof(cfg)));
    EXPECT_EQ(sizeof(D3D11_VIDEO_DECODER_CONFIG), 100);
    ifv_video_close(&v);
}

T(video_dec_create_decoder_null_arguments) {
    IFV_VIDEO(v);
    D3D11_VIDEO_DECODER_DESC desc = { k_h264_vld, 64, 64, DXGI_FORMAT_NV12 };
    D3D11_VIDEO_DECODER_CONFIG cfg; memset(&cfg, 0, sizeof(cfg));
    ID3D11VideoDecoder *dec = (void *)(ULONG_PTR)0x1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoder(v.vd, NULL, &cfg, &dec), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoder(v.vd, &desc, NULL, &dec), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoder(v.vd, &desc, &cfg, NULL), E_INVALIDARG);
    dec = (void *)(ULONG_PTR)0x1;
    /* all pointers valid: the profile / size validation happens first and clears the output */
    HRESULT hr = ID3D11VideoDevice_CreateVideoDecoder(v.vd, &desc, &cfg, &dec);
    t_note("CreateVideoDecoder(H264 VLD 64x64 NV12, zero config) -> %s", d3dh_hr_name(hr));
    if (FAILED(hr)) CHECK(dec == NULL); else SAFE_RELEASE(dec);
    ifv_video_close(&v);
}

T(video_dec_create_decoder_rejects_bad_description) {
    IFV_VIDEO(v);
    D3D11_VIDEO_DECODER_CONFIG cfg; memset(&cfg, 0, sizeof(cfg));
    ID3D11VideoDecoder *dec = (void *)(ULONG_PTR)0x1;
    D3D11_VIDEO_DECODER_DESC unknown = { ifv_IID_Random, 64, 64, DXGI_FORMAT_NV12 };
    EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoder(v.vd, &unknown, &cfg, &dec), E_INVALIDARG);
    CHECK(dec == NULL);
    D3D11_VIDEO_DECODER_DESC zw = { k_h264_vld, 0, 64, DXGI_FORMAT_NV12 };
    dec = (void *)(ULONG_PTR)0x1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoder(v.vd, &zw, &cfg, &dec), E_INVALIDARG);
    CHECK(dec == NULL);
    D3D11_VIDEO_DECODER_DESC zh = { k_h264_vld, 64, 0, DXGI_FORMAT_NV12 };
    dec = (void *)(ULONG_PTR)0x1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoder(v.vd, &zh, &cfg, &dec), E_INVALIDARG);
    CHECK(dec == NULL);
    D3D11_VIDEO_DECODER_DESC badfmt = { k_h264_vld, 64, 64, DXGI_FORMAT_R32G32B32A32_FLOAT };
    dec = (void *)(ULONG_PTR)0x1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoder(v.vd, &badfmt, &cfg, &dec), E_INVALIDARG);
    CHECK(dec == NULL);
    ifv_video_close(&v);
}

T(video_dec_create_decoder_for_listed_profile) {
    IFV_VIDEO(v);
    GUID g[256];
    UINT n = ifv_profiles(v.vd, g, 256), made = 0;
    for (UINT i = 0; i < n && !made; i++) {
        BOOL sup = 0;
        if (FAILED(ID3D11VideoDevice_CheckVideoDecoderFormat(v.vd, &g[i], DXGI_FORMAT_NV12, &sup)) || !sup) continue;
        D3D11_VIDEO_DECODER_DESC desc = { g[i], 128, 128, DXGI_FORMAT_NV12 };
        UINT cnt = 0;
        if (FAILED(ID3D11VideoDevice_GetVideoDecoderConfigCount(v.vd, &desc, &cnt)) || !cnt) continue;
        D3D11_VIDEO_DECODER_CONFIG cfg;
        if (FAILED(ID3D11VideoDevice_GetVideoDecoderConfig(v.vd, &desc, 0, &cfg))) continue;
        ID3D11VideoDecoder *dec = NULL;
        HRESULT hr = ID3D11VideoDevice_CreateVideoDecoder(v.vd, &desc, &cfg, &dec);
        t_note("CreateVideoDecoder(%s) -> %s", d3dh_guid_str(&g[i]), d3dh_hr_name(hr));
        if (dec) {
            made++;
            D3D11_VIDEO_DECODER_DESC gd; D3D11_VIDEO_DECODER_CONFIG gc;
            memset(&gd, 0xcc, sizeof(gd)); memset(&gc, 0xcc, sizeof(gc));
            EXPECT_HR(ID3D11VideoDecoder_GetCreationParameters(dec, &gd, &gc), S_OK);
            CHECK(!memcmp(&gd.Guid, &desc.Guid, sizeof(GUID)));
            EXPECT_EQ(gd.SampleWidth, 128); EXPECT_EQ(gd.SampleHeight, 128); EXPECT_EQ(gd.OutputFormat, DXGI_FORMAT_NV12);
            CHECK(!memcmp(&gc, &cfg, sizeof(cfg)));
            void *drv = (void *)(ULONG_PTR)1;
            HRESULT h2 = ID3D11VideoDecoder_GetDriverHandle(dec, &drv);
            t_note("GetDriverHandle -> %s", d3dh_hr_name(h2));
            ID3D11Device *g2 = NULL;
            ID3D11VideoDecoder_GetDevice(dec, &g2);
            CHECK(g2 == v.dev);
            SAFE_RELEASE(g2);
            EXPECT_EQ(D3DH_RELEASE_RC(dec), 0);
        }
    }
    if (!made) { ifv_video_close(&v); t_skip("no decoder could be created on this device"); return; }
    ifv_video_close(&v);
}

T(video_dec_output_view_validation) {
    IFV_VIDEO(v);
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC vd; memset(&vd, 0, sizeof(vd));
    vd.DecodeProfile = k_h264_vld; vd.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
    ID3D11VideoDecoderOutputView *ov = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoderOutputView(v.vd, NULL, &vd, &ov), E_INVALIDARG);
    CHECK(ov == NULL);
    D3D11_TEXTURE2D_DESC td = { 64, 64, 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(v.dev, &td, NULL, &t))) {
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        ID3D11Device_CreateTexture2D(v.dev, &td, NULL, &t);
    }
    if (t) {
        ov = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoderOutputView(v.vd, (ID3D11Resource *)t, NULL, &ov), E_INVALIDARG);
        CHECK(ov == NULL);
        /* the texture has no BIND_DECODER */
        ov = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoderOutputView(v.vd, (ID3D11Resource *)t, &vd, &ov), E_INVALIDARG);
        CHECK(ov == NULL);
        SAFE_RELEASE(t);
    }
    D3D11_BUFFER_DESC bd = { 256, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, 0 };
    ID3D11Buffer *b = NULL;
    if (SUCCEEDED(ID3D11Device_CreateBuffer(v.dev, &bd, NULL, &b))) {
        ov = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11VideoDevice_CreateVideoDecoderOutputView(v.vd, (ID3D11Resource *)b, &vd, &ov), E_INVALIDARG);
        CHECK(ov == NULL);
        SAFE_RELEASE(b);
    }
    ifv_video_close(&v);
}

T(video_ctx_decoder_calls_reject_null_decoder) {
    IFV_VIDEO(v);
    void *buf = (void *)(ULONG_PTR)1; UINT sz = 7;
    /* No decoder object: every decoder entry point reports E_INVALIDARG and writes nothing. */
    EXPECT_HR(ID3D11VideoContext_GetDecoderBuffer(v.vc, NULL, D3D11_VIDEO_DECODER_BUFFER_BITSTREAM, &sz, &buf), E_INVALIDARG);
    EXPECT_EQ(sz, 7);
    EXPECT_EQ(buf, 1);
    EXPECT_HR(ID3D11VideoContext_ReleaseDecoderBuffer(v.vc, NULL, D3D11_VIDEO_DECODER_BUFFER_BITSTREAM), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_DecoderBeginFrame(v.vc, NULL, NULL, 0, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_DecoderEndFrame(v.vc, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_SubmitDecoderBuffers(v.vc, NULL, 0, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoContext_DecoderExtension(v.vc, NULL, NULL), E_INVALIDARG);
    ifv_video_close(&v);
}

T(video_ctx_decoder_buffer_types_table) {
    IFV_VIDEO(v);
    static const int types[] = { D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS, D3D11_VIDEO_DECODER_BUFFER_MACROBLOCK_CONTROL,
        D3D11_VIDEO_DECODER_BUFFER_RESIDUAL_DIFFERENCE, D3D11_VIDEO_DECODER_BUFFER_DEBLOCKING_CONTROL,
        D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX, D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL,
        D3D11_VIDEO_DECODER_BUFFER_BITSTREAM, D3D11_VIDEO_DECODER_BUFFER_MOTION_VECTOR, D3D11_VIDEO_DECODER_BUFFER_FILM_GRAIN };
    for (unsigned i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        void *buf = NULL; UINT sz = 0;
        EXPECT_HR(ID3D11VideoContext_GetDecoderBuffer(v.vc, NULL, (D3D11_VIDEO_DECODER_BUFFER_TYPE)types[i], &sz, &buf), E_INVALIDARG);
        EXPECT_HR(ID3D11VideoContext_ReleaseDecoderBuffer(v.vc, NULL, (D3D11_VIDEO_DECODER_BUFFER_TYPE)types[i]), E_INVALIDARG);
    }
    EXPECT_EQ(D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS, 0);
    EXPECT_EQ(D3D11_VIDEO_DECODER_BUFFER_BITSTREAM, 6);
    EXPECT_EQ(D3D11_VIDEO_DECODER_BUFFER_FILM_GRAIN, 8);
    ifv_video_close(&v);
}

T(video_ctx_video_context1_decoder_extras_null) {
    IFV_VIDEO(v);
    ID3D11VideoContext1 *c1 = NULL;
    if (FAILED(ID3D11VideoContext_QueryInterface(v.vc, &IID_ID3D11VideoContext1, (void **)&c1))) { ifv_video_close(&v); t_skip("no ID3D11VideoContext1"); return; }
    /* VideoContext1 adds decoder helpers; with no decoder / session they must fail cleanly rather than crash. */
    HRESULT hr = ID3D11VideoContext1_SubmitDecoderBuffers1(c1, NULL, 0, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(c1);
    ifv_video_close(&v);
}

T(video_protection_caps_null_output) {
    IFV_VIDEO(v);
    EXPECT_HR(ID3D11VideoDevice_GetContentProtectionCaps(v.vd, NULL, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_GetContentProtectionCaps(v.vd, &k_h264_vld, &k_h264_vld, NULL), E_INVALIDARG);
    ifv_video_close(&v);
}

T(video_protection_caps_values) {
    IFV_VIDEO(v);
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS caps; memset(&caps, 0xcc, sizeof(caps));
    HRESULT hr = ID3D11VideoDevice_GetContentProtectionCaps(v.vd, NULL, NULL, &caps);
    t_note("GetContentProtectionCaps(NULL,NULL) -> %s caps 0x%x keyx %u align %u mem %llu", d3dh_hr_name(hr), caps.Caps,
           caps.KeyExchangeTypeCount, caps.BlockAlignmentSize, (unsigned long long)caps.ProtectedMemorySize);
    EXPECT_HR(hr, S_OK);
    /* the output structure is always fully written (zeroed when the driver reports nothing) */
    CHECK(caps.Caps != 0xcccccccc || caps.KeyExchangeTypeCount != 0xcccccccc);
    if (caps.Caps == 0) {
        EXPECT_EQ(caps.KeyExchangeTypeCount, 0);
        EXPECT_EQ(caps.BlockAlignmentSize, 0);
        EXPECT_EQ(caps.ProtectedMemorySize, 0);
    }
    ifv_video_close(&v);
}

T(video_protection_check_key_exchange) {
    IFV_VIDEO(v);
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS caps; memset(&caps, 0, sizeof(caps));
    ID3D11VideoDevice_GetContentProtectionCaps(v.vd, NULL, NULL, &caps);
    GUID out = ifv_IID_Random;
    EXPECT_HR(ID3D11VideoDevice_CheckCryptoKeyExchange(v.vd, NULL, NULL, 0, &out), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CheckCryptoKeyExchange(v.vd, &ifv_IID_Random, NULL, 0, NULL), E_INVALIDARG);
    out = ifv_IID_Random;
    HRESULT hr = ID3D11VideoDevice_CheckCryptoKeyExchange(v.vd, &ifv_IID_Random, NULL, caps.KeyExchangeTypeCount, &out);
    CHECKF(hr == E_INVALIDARG, "index == KeyExchangeTypeCount (%u) -> %s", caps.KeyExchangeTypeCount, d3dh_hr_name(hr));
    hr = ID3D11VideoDevice_CheckCryptoKeyExchange(v.vd, &ifv_IID_Random, NULL, 0xffffffff, &out);
    EXPECT_HR(hr, E_INVALIDARG);
    if (caps.KeyExchangeTypeCount == 0) {
        EXPECT_HR(ID3D11VideoDevice_CheckCryptoKeyExchange(v.vd, &ifv_IID_Random, NULL, 0, &out), E_INVALIDARG);
    } else {
        for (UINT i = 0; i < caps.KeyExchangeTypeCount && i < 8; i++) {
            out = ifv_IID_Random;
            hr = ID3D11VideoDevice_CheckCryptoKeyExchange(v.vd, &ifv_IID_Random, NULL, i, &out);
            t_note("key exchange %u -> %s %s", i, d3dh_hr_name(hr), d3dh_guid_str(&out));
        }
    }
    ifv_video_close(&v);
}

T(video_crypto_session_null_arguments) {
    IFV_VIDEO(v);
    ID3D11CryptoSession *cs = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11VideoDevice_CreateCryptoSession(v.vd, NULL, NULL, &ifv_IID_Random, &cs), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CreateCryptoSession(v.vd, &ifv_IID_Random, NULL, NULL, &cs), E_INVALIDARG);
    EXPECT_HR(ID3D11VideoDevice_CreateCryptoSession(v.vd, &ifv_IID_Random, NULL, &ifv_IID_Random, NULL), E_INVALIDARG);
    ifv_video_close(&v);
}

T(video_crypto_session_unsupported_type) {
    IFV_VIDEO(v);
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS caps; memset(&caps, 0, sizeof(caps));
    ID3D11VideoDevice_GetContentProtectionCaps(v.vd, &ifv_IID_Random, NULL, &caps);
    ID3D11CryptoSession *cs = (void *)(ULONG_PTR)1;
    HRESULT hr = ID3D11VideoDevice_CreateCryptoSession(v.vd, &ifv_IID_Random, NULL, &ifv_IID_Random, &cs);
    t_note("CreateCryptoSession(random type): caps 0x%x -> %s", caps.Caps, d3dh_hr_name(hr));
    CHECK(cs == NULL || SUCCEEDED(hr));
    if (caps.Caps == 0) EXPECT_HR(hr, E_INVALIDARG);    /* "no protection support" is reported as invalid arguments */
    if (SUCCEEDED(hr)) SAFE_RELEASE(cs);
    ifv_video_close(&v);
}

T(video_authenticated_channel_arguments) {
    IFV_VIDEO(v);
    ID3D11AuthenticatedChannel *ac = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11VideoDevice_CreateAuthenticatedChannel(v.vd, D3D11_AUTHENTICATED_CHANNEL_D3D11, NULL), E_INVALIDARG);
    static const int bad[] = { 0, 4, 5, 100, -1 };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        ac = (void *)(ULONG_PTR)1;
        HRESULT hr = ID3D11VideoDevice_CreateAuthenticatedChannel(v.vd, (D3D11_AUTHENTICATED_CHANNEL_TYPE)bad[i], &ac);
        CHECKF(hr == E_INVALIDARG, "channel type %d -> %s", bad[i], d3dh_hr_name(hr));
        CHECKF(ac == NULL, "channel type %d: out pointer not cleared", bad[i]);
    }
    ifv_video_close(&v);
}

T(video_authenticated_channel_driver_types_need_caps) {
    IFV_VIDEO(v);
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS caps; memset(&caps, 0, sizeof(caps));
    ID3D11VideoDevice_GetContentProtectionCaps(v.vd, NULL, NULL, &caps);
    ID3D11AuthenticatedChannel *ac = (void *)(ULONG_PTR)1;
    HRESULT hr = ID3D11VideoDevice_CreateAuthenticatedChannel(v.vd, D3D11_AUTHENTICATED_CHANNEL_DRIVER_SOFTWARE, &ac);
    t_note("software channel (caps 0x%x) -> %s", caps.Caps, d3dh_hr_name(hr));
    if (!(caps.Caps & D3D11_CONTENT_PROTECTION_CAPS_SOFTWARE)) { EXPECT_HR(hr, E_INVALIDARG); CHECK(ac == NULL); }
    if (SUCCEEDED(hr)) SAFE_RELEASE(ac);
    ac = (void *)(ULONG_PTR)1;
    hr = ID3D11VideoDevice_CreateAuthenticatedChannel(v.vd, D3D11_AUTHENTICATED_CHANNEL_DRIVER_HARDWARE, &ac);
    t_note("hardware channel (caps 0x%x) -> %s", caps.Caps, d3dh_hr_name(hr));
    if (!(caps.Caps & D3D11_CONTENT_PROTECTION_CAPS_HARDWARE)) { EXPECT_HR(hr, E_INVALIDARG); CHECK(ac == NULL); }
    if (SUCCEEDED(hr)) SAFE_RELEASE(ac);
    ac = (void *)(ULONG_PTR)1;
    hr = ID3D11VideoDevice_CreateAuthenticatedChannel(v.vd, D3D11_AUTHENTICATED_CHANNEL_D3D11, &ac);
    t_note("D3D11 channel -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        UINT sz = 0;
        ID3D11AuthenticatedChannel_GetCertificateSize(ac, &sz);
        t_note("  certificate size %u", sz);
        SAFE_RELEASE(ac);
    } else CHECK(ac == NULL);
    ifv_video_close(&v);
}

T(video_extension_device_absent) {
    IFV_VIDEO(v);
    /* The driver extension entry points must not be reachable through any documented QI path. */
    D3D11_VIDEO_DECODER_EXTENSION ext; memset(&ext, 0, sizeof(ext));
    EXPECT_HR(ID3D11VideoContext_DecoderExtension(v.vc, NULL, &ext), E_INVALIDARG);
    ifv_video_close(&v);
}

T(video_device_private_data) {
    IFV_VIDEO(v);
    static const GUID g = {0x31ab5b06, 0x7d2c, 0x4eb4, {0x8e, 0x6b, 0x13, 0xb0, 0x58, 0xdc, 0x41, 0x11}};
    UINT data[2] = { 0xdeadbeef, 0x12345678 }, got[2] = {0}, sz = sizeof(got);
    EXPECT_HR(ID3D11VideoDevice_SetPrivateData(v.vd, &g, sizeof(data), data), S_OK);
    /* private data is stored on the shared device object */
    EXPECT_HR(ID3D11Device_GetPrivateData(v.dev, &g, &sz, got), S_OK);
    EXPECT_EQ(sz, sizeof(data));
    CHECK(!memcmp(data, got, sizeof(data)));
    EXPECT_HR(ID3D11Device_SetPrivateData(v.dev, &g, 0, NULL), S_OK);
    sz = sizeof(got);
    HRESULT hr = ID3D11Device_GetPrivateData(v.dev, &g, &sz, got);
    CHECKF(FAILED(hr), "private data still present after removal: %s", d3dh_hr_name(hr));
    ifv_video_close(&v);
}

/* The reference VM's only adapter (Basic Render Driver) has no video processor/decoder: pin exactly how that shows up so the 70+ skipped video
 * tests are explained by a measured fact rather than a guess. */
T(video_unavailable_on_basic_render_driver) {
    IFV_DEV(d, c);
    if (!ifv_is_brd(d)) { t_skip("not the Basic Render Driver: video interfaces are expected"); SAFE_RELEASE(c); SAFE_RELEASE(d); return; }
    void *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11Device_QueryInterface(d, &IID_ID3D11VideoDevice, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = NULL;
    EXPECT_HR(ID3D11DeviceContext_QueryInterface(c, &IID_ID3D11VideoContext, &p), S_OK);   /* the context interface is exposed without a device */
    if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    SAFE_RELEASE(c); SAFE_RELEASE(d);
}
