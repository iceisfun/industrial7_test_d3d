/* Additional Win10-probed contracts: unsupported formats cannot create resources, argument handling of the
 * format / multisample / counter queries, and the feature ids past the last one Windows 10 knows. */
#include "cf.h"

/* Every format whose support word is empty (R1_UNORM, the planar video ids, undefined ids) is rejected by texture creation for any bind flag. */
TEST(d3d11_caps_formats_unsupported_formats_cannot_create)
{
    ID3D11Device *d = cf_shared(NULL); UINT f, n = 0;
    static const UINT binds[] = { D3D11_BIND_SHADER_RESOURCE, D3D11_BIND_RENDER_TARGET, D3D11_BIND_DEPTH_STENCIL, D3D11_BIND_UNORDERED_ACCESS, 0 };
    if (!d) { t_skip("no D3D11 device"); return; }
    for (f = 1; f < 135; f++) {
        UINT s = cf_sup(d, f); size_t i;
        if (s) continue;
        for (i = 0; i < sizeof binds / sizeof binds[0]; i++) {
            D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t = (ID3D11Texture2D *)(ULONG_PTR)1; HRESULT hr;
            memset(&td, 0, sizeof td); td.Width = 16; td.Height = 16; td.MipLevels = 1; td.ArraySize = 1; td.Format = (DXGI_FORMAT)f;
            td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = binds[i];
            hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &t);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "unsupported format texture", "format %u (%s) bind %#x got %s", f, cf_fmt_name(f), binds[i], d3dh_hr_name(hr));
            if (SUCCEEDED(hr)) SAFE_RELEASE(t);
        }
        n++;
    }
    CHECK(n > 0);
    t_note("%u formats with an empty support word", n);
}

/* Typeless formats create with SRV, and the other bind flags follow the typed family (probed on the Basic Render Driver). */
TEST(d3d11_caps_formats_typeless_bind_rules)
{
    ID3D11Device *d = cf_shared(NULL);
    static const struct { UINT f; HRESULT srv, rt, ds, uav; } t[] = {
        { DXGI_FORMAT_R32G32B32A32_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK }, { DXGI_FORMAT_R32G32B32_TYPELESS, S_OK, S_OK, E_INVALIDARG, E_INVALIDARG },
        { DXGI_FORMAT_R16G16B16A16_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK }, { DXGI_FORMAT_R32G32_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK },
        { DXGI_FORMAT_R32G8X24_TYPELESS, S_OK, E_INVALIDARG, S_OK, E_INVALIDARG }, { DXGI_FORMAT_R10G10B10A2_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK },
        { DXGI_FORMAT_R8G8B8A8_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK }, { DXGI_FORMAT_R16G16_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK },
        { DXGI_FORMAT_R32_TYPELESS, S_OK, S_OK, S_OK, S_OK }, { DXGI_FORMAT_R24G8_TYPELESS, S_OK, E_INVALIDARG, S_OK, E_INVALIDARG },
        { DXGI_FORMAT_R8G8_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK }, { DXGI_FORMAT_R16_TYPELESS, S_OK, S_OK, S_OK, S_OK },
        { DXGI_FORMAT_R8_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK }, { DXGI_FORMAT_BC1_TYPELESS, S_OK, E_INVALIDARG, E_INVALIDARG, E_INVALIDARG },
        { DXGI_FORMAT_BC7_TYPELESS, S_OK, E_INVALIDARG, E_INVALIDARG, E_INVALIDARG }, { DXGI_FORMAT_B8G8R8A8_TYPELESS, S_OK, S_OK, E_INVALIDARG, S_OK },
    };
    size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        static const UINT binds[4] = { D3D11_BIND_SHADER_RESOURCE, D3D11_BIND_RENDER_TARGET, D3D11_BIND_DEPTH_STENCIL, D3D11_BIND_UNORDERED_ACCESS };
        const HRESULT want[4] = { t[i].srv, t[i].rt, t[i].ds, t[i].uav };
        int k;
        for (k = 0; k < 4; k++) {
            D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *tex = NULL; HRESULT hr;
            memset(&td, 0, sizeof td); td.Width = 16; td.Height = 16; td.MipLevels = 1; td.ArraySize = 1; td.Format = (DXGI_FORMAT)t[i].f;
            td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = binds[k];
            hr = ID3D11Device_CreateTexture2D(d, &td, NULL, &tex);
            t_check(hr == want[k], __FILE__, __LINE__, "typeless bind", "%s bind %#x got %s want %s", cf_fmt_name(t[i].f), binds[k], d3dh_hr_name(hr), d3dh_hr_name(want[k]));
            SAFE_RELEASE(tex);
        }
    }
}

/* CheckMultisampleQualityLevels argument handling (Windows 10 d3d11): count 0 and counts above 32 are E_FAIL; other non-power-of-two counts
 * succeed with 0 levels; formats outside 0..132 are E_INVALIDARG and leave *out untouched; DXGI_FORMAT_UNKNOWN answers only for count 1. */
TEST(d3d11_caps_formats_msaa_quality_argument_rules)
{
    ID3D11Device *d = cf_shared(NULL); UINT q, c;
    if (!d) { t_skip("no D3D11 device"); return; }
    q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &q), E_FAIL); EXPECT_EQ(q, 0);
    for (c = 33; c <= 64; c += 31) { q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, c, &q), E_FAIL); EXPECT_EQ(q, 0); }
    q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, 0x80000000u, &q), E_FAIL); EXPECT_EQ(q, 0);
    q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, 0xffffffffu, &q), E_FAIL); EXPECT_EQ(q, 0);
    for (c = 3; c <= 31; c++) {
        if (!(c & (c - 1))) continue;                      /* skip powers of two */
        q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, c, &q), S_OK); EXPECT_EQ(q, 0);
    }
    q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_R8G8B8A8_UNORM, 32, &q), S_OK); EXPECT_EQ(q, 0);
    q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_UNKNOWN, 1, &q), S_OK); EXPECT_EQ(q, 1);
    q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_UNKNOWN, 2, &q), S_OK); EXPECT_EQ(q, 0);
    q = 0xdeadbeef; EXPECT_HR(ID3D11Device_CheckMultisampleQualityLevels(d, DXGI_FORMAT_UNKNOWN, 0, &q), E_FAIL); EXPECT_EQ(q, 0);
    {
        static const UINT bad[] = { 133, 200, 0x7fffffffu, 0xffffffffu };
        size_t i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++)
            for (c = 0; c <= 33; c += 11) {
                q = 0xdeadbeef;
                t_check(ID3D11Device_CheckMultisampleQualityLevels(d, (DXGI_FORMAT)bad[i], c, &q) == E_INVALIDARG && q == 0xdeadbeef, __FILE__, __LINE__, "msaa invalid format", "format %u count %u q=%#x", bad[i], c, q);
            }
    }
}

/* CheckFormatSupport / FORMAT_SUPPORT / FORMAT_SUPPORT2 with an id outside the format enumeration: E_FAIL with zeroed output (Windows 10). */
TEST(d3d11_caps_formats_support_invalid_ids_efail)
{
    ID3D11Device *d = cf_shared(NULL);
    static const UINT ids[] = { 133, 134, 135, 140, 200, 1000, 0x7fffffffu, 0xffffffffu };
    size_t i;
    if (!d) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        UINT s = 0xdeadbeef; HRESULT hr = ID3D11Device_CheckFormatSupport(d, (DXGI_FORMAT)ids[i], &s);
        D3D11_FEATURE_DATA_FORMAT_SUPPORT f1; D3D11_FEATURE_DATA_FORMAT_SUPPORT2 f2; HRESULT h1, h2;
        f1.InFormat = (DXGI_FORMAT)ids[i]; f1.OutFormatSupport = 0xdeadbeef; f2.InFormat = (DXGI_FORMAT)ids[i]; f2.OutFormatSupport2 = 0xdeadbeef;
        h1 = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_FORMAT_SUPPORT, &f1, sizeof f1);
        h2 = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_FORMAT_SUPPORT2, &f2, sizeof f2);
        t_check(hr == E_FAIL && s == 0 && h1 == E_FAIL && f1.OutFormatSupport == 0 && h2 == E_FAIL && f2.OutFormatSupport2 == 0, __FILE__, __LINE__, "invalid format id",
                "id %#x: %s s=%#x / %s %#x / %s %#x", ids[i], d3dh_hr_name(hr), s, d3dh_hr_name(h1), f1.OutFormatSupport, d3dh_hr_name(h2), f2.OutFormatSupport2);
    }
}

/* Counters on the Basic Render Driver: none (LastDeviceDependentCounter 0, 1 detectable parallel unit); every CheckCounter is E_INVALIDARG. */
TEST(d3d11_caps_formats_counters_none_on_basic_render_driver)
{
    ID3D11Device *d = cf_shared(NULL); D3D11_COUNTER_INFO ci; D3D11_COUNTER_DESC cd; D3D11_COUNTER_TYPE t; UINT active, nl = 0, ul = 0, dl = 0, v = 0;
    static const UINT ids[] = { 0, 1, 0x40000000u, 0x40000001u, 0x7fffffffu, 0xffffffffu };
    size_t i; int k;
    IDXGIDevice *dd = NULL; IDXGIAdapter *a = NULL; DXGI_ADAPTER_DESC ad;
    if (!d) { t_skip("no D3D11 device"); return; }
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd))) {
        if (SUCCEEDED(IDXGIDevice_GetAdapter(dd, &a)) && SUCCEEDED(IDXGIAdapter_GetDesc(a, &ad))) v = ad.VendorId;
        SAFE_RELEASE(a); SAFE_RELEASE(dd);
    }
    if (v != 0x1414) { t_skip("adapter vendor %#x is not the Microsoft Basic Render Driver", v); return; }
    memset(&ci, 0xef, sizeof ci);
    ID3D11Device_CheckCounterInfo(d, &ci);
    EXPECT_EQ(ci.LastDeviceDependentCounter, 0);
    EXPECT_EQ(ci.NumSimultaneousCounters, 0);
    EXPECT_EQ(ci.NumDetectableParallelUnits, 1);
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++)
        for (k = 0; k < 2; k++) {
            cd.Counter = (D3D11_COUNTER)ids[i]; cd.MiscFlags = (UINT)k; nl = ul = dl = 0;
            t_check(ID3D11Device_CheckCounter(d, &cd, &t, &active, NULL, &nl, NULL, &ul, NULL, &dl) == E_INVALIDARG, __FILE__, __LINE__, "CheckCounter", "counter %#x misc %d", ids[i], k);
        }
}

/* D3D11_FEATURE ids 20..40 do not exist on Windows 10 (build 19045): E_INVALIDARG at every size and the buffer is untouched. */
TEST(d3d11_caps_formats_feat_ids_past_last_known)
{
    ID3D11Device *d = cf_shared(NULL); int f; UINT sz;
    OSVERSIONINFOW vi; typedef LONG (WINAPI *rtl_t)(OSVERSIONINFOW *);
    rtl_t rv = (rtl_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (!d) { t_skip("no D3D11 device"); return; }
    memset(&vi, 0, sizeof vi); vi.dwOSVersionInfoSize = sizeof vi;
    if (!rv || rv(&vi) != 0 || vi.dwBuildNumber != 19045) { t_skip("newer D3D11 feature ids exist after build 19045 (running %lu)", (unsigned long)vi.dwBuildNumber); return; }
    for (f = 20; f <= 40; f++)
        for (sz = 0; sz <= 64; sz += 4) {
            unsigned char buf[64], ref[64]; HRESULT hr;
            memset(buf, 0xef, sizeof buf); memcpy(ref, buf, sizeof buf);
            hr = ID3D11Device_CheckFeatureSupport(d, (D3D11_FEATURE)f, buf, sz);
            t_check(hr == E_INVALIDARG && !memcmp(buf, ref, sizeof buf), __FILE__, __LINE__, "feature id past the end", "feature %d size %u got %s", f, sz, d3dh_hr_name(hr));
        }
}
