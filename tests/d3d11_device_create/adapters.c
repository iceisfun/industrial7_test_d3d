/* Device creation on explicit adapters, concurrent device creation, device identity, and CheckFormatSupport cross-checks
 * (Win10 22H2 oracle: adapter 0 and adapter 1 are both "Microsoft Basic Render Driver", the second one without outputs). */
#include "dc.h"

#define T(n) TEST(d3d11_device_create_##n)

static int adapters_enum(IDXGIFactory1 **f, IDXGIAdapter1 **a, int max)
{
    int n = 0;
    *f = NULL;
    if (FAILED(d3dh_make_factory1(f)) || !*f) return 0;
    while (n < max && IDXGIFactory1_EnumAdapters1(*f, n, &a[n]) == S_OK) n++;
    return n;
}

T(adapter_enumeration_matches_oracle)
{
    IDXGIFactory1 *f; IDXGIAdapter1 *a[4] = { 0 }; int n = adapters_enum(&f, a, 4);
    if (!n) { t_skip("no DXGI adapters"); SAFE_RELEASE(f); return; }
    for (int i = 0; i < n; i++) {
        DXGI_ADAPTER_DESC1 d; IDXGIOutput *o = NULL; memset(&d, 0, sizeof d);
        EXPECT_HR(IDXGIAdapter1_GetDesc1(a[i], &d), S_OK);
        EXPECT_EQ(d.VendorId, 0x1414); EXPECT_EQ(d.DeviceId, 0x8c);
        /* adapter 0: the display adapter (has the desktop output); adapter 1: the same driver as the SOFTWARE-flagged WARP adapter without outputs */
        EXPECT_EQ(d.Flags, i == 0 ? 0 : DXGI_ADAPTER_FLAG_SOFTWARE);
        EXPECT_HR(IDXGIAdapter1_EnumOutputs(a[i], 0, &o), i == 0 ? S_OK : DXGI_ERROR_NOT_FOUND);
        SAFE_RELEASE(o);
    }
    EXPECT_EQ(n, 2);
    if (n == 2) { DXGI_ADAPTER_DESC1 d0, d1; IDXGIAdapter1_GetDesc1(a[0], &d0); IDXGIAdapter1_GetDesc1(a[1], &d1);
        CHECK(d0.AdapterLuid.LowPart != d1.AdapterLuid.LowPart || d0.AdapterLuid.HighPart != d1.AdapterLuid.HighPart); }
    for (int i = 0; i < n; i++) SAFE_RELEASE(a[i]);
    SAFE_RELEASE(f);
}

/* UNKNOWN driver type + explicit adapter creates a device on each enumerated adapter. */
T(unknown_driver_on_each_adapter)
{
    IDXGIFactory1 *f; IDXGIAdapter1 *a[4] = { 0 }; int n;
    REQUIRE(DC_FN());
    n = adapters_enum(&f, a, 4);
    if (!n) { t_skip("no DXGI adapters"); SAFE_RELEASE(f); return; }
    for (int i = 0; i < n; i++) {
        ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = DC_FL_SENTINEL;
        HRESULT hr = DC_FN()((IDXGIAdapter *)a[i], D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, &c);
        EXPECT_HR(hr, S_OK); EXPECT_EQ(fl, D3D_FEATURE_LEVEL_11_0);
        if (d) { IDXGIDevice *dd = NULL; IDXGIAdapter *ga = NULL; DXGI_ADAPTER_DESC want, got;
            memset(&want, 0, sizeof want); memset(&got, 0, sizeof got);
            IDXGIAdapter1_GetDesc((IDXGIAdapter1 *)a[i], &want);
            if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd))) {
                if (SUCCEEDED(IDXGIDevice_GetAdapter(dd, &ga))) { IDXGIAdapter_GetDesc(ga, &got);
                    CHECKF(got.AdapterLuid.LowPart == want.AdapterLuid.LowPart && got.AdapterLuid.HighPart == want.AdapterLuid.HighPart, "device %d runs on a different adapter", i); IDXGIAdapter_Release(ga); }
                IDXGIDevice_Release(dd); } }
        DC_REL(d, c);
    }
    for (int i = 0; i < n; i++) SAFE_RELEASE(a[i]);
    SAFE_RELEASE(f);
}
/* An explicit adapter needs driver type UNKNOWN; any other type with an adapter is E_INVALIDARG. */
T(adapter_with_other_driver_types_rejected)
{
    IDXGIFactory1 *f; IDXGIAdapter1 *a[1] = { 0 }; int n;
    static const D3D_DRIVER_TYPE dts[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_NULL, D3D_DRIVER_TYPE_WARP };
    REQUIRE(DC_FN());
    n = adapters_enum(&f, a, 1);
    if (!n) { t_skip("no DXGI adapters"); SAFE_RELEASE(f); return; }
    for (unsigned i = 0; i < sizeof dts / sizeof *dts; i++)
        dc_expect_fail_cleared(E_INVALIDARG, (IDXGIAdapter *)a[0], dts[i], NULL, 0, NULL, 0, D3D11_SDK_VERSION, "adapter + non-UNKNOWN driver type");
    SAFE_RELEASE(a[0]); SAFE_RELEASE(f);
}

/* The device created on the WARP adapter (EnumWarpAdapter) reports feature level 11_1 for a list with 11_1 first. */
T(warp_adapter_via_enumwarpadapter)
{
    IDXGIAdapter *w = NULL; ID3D11Device *d = NULL; D3D_FEATURE_LEVEL fl = 0; static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    DXGI_ADAPTER_DESC ad;
    REQUIRE(DC_FN());
    if (!dc_get_warp_adapter(&w)) { t_skip("no IDXGIFactory4::EnumWarpAdapter"); return; }
    memset(&ad, 0, sizeof ad); IDXGIAdapter_GetDesc(w, &ad);
    EXPECT_EQ(ad.VendorId, 0x1414);
    EXPECT_HR(DC_FN()(w, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, l, 2, D3D11_SDK_VERSION, &d, &fl, NULL), S_OK);
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_11_1);
    SAFE_RELEASE(d); SAFE_RELEASE(w);
}

/* ---- concurrency and lifetime ---- */
typedef struct { int n; volatile LONG bad; } dc_thr;
static DWORD WINAPI dc_thr_main(LPVOID p)
{
    dc_thr *a = p;
    for (int i = 0; i < a->n; i++) {
        ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL, *c2 = NULL; D3D_FEATURE_LEVEL fl = 0;
        if (DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d, &fl, &c) != S_OK || !d || !c) { InterlockedIncrement(&a->bad); continue; }
        ID3D11Device_GetImmediateContext(d, &c2);
        if (c2 != c) InterlockedIncrement(&a->bad);
        if (c2) ID3D11DeviceContext_Release(c2);
        if (ID3D11Device_GetFeatureLevel(d) != fl || fl != D3D_FEATURE_LEVEL_11_0) InterlockedIncrement(&a->bad);
        ID3D11DeviceContext_Release(c);
        if (ID3D11Device_Release(d) != 0) InterlockedIncrement(&a->bad);
    }
    return 0;
}
T(concurrent_device_creation)
{
    enum { N = 6 }; HANDLE h[N]; dc_thr a[N]; LONG bad = 0;
    REQUIRE(DC_FN());
    for (int i = 0; i < N; i++) { a[i].n = 12; a[i].bad = 0; h[i] = CreateThread(NULL, 0, dc_thr_main, &a[i], 0, NULL); }
    for (int i = 0; i < N; i++) { if (h[i]) { WaitForSingleObject(h[i], 240000); CloseHandle(h[i]); } else bad++; }
    for (int i = 0; i < N; i++) bad += a[i].bad;
    EXPECT_EQ(bad, 0);
}
T(many_devices_alive_together)
{
    enum { N = 24 }; ID3D11Device *d[N] = { 0 }; ID3D11DeviceContext *c[N] = { 0 }; int ok = 0;
    REQUIRE(DC_FN());
    for (int i = 0; i < N; i++) if (DC_FN()(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d[i], NULL, &c[i]) == S_OK) ok++;
    EXPECT_EQ(ok, N);
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (d[i] && d[j]) CHECK(d[i] != d[j] && c[i] != c[j]);
    /* an object created on one device is independent of the others: release in an interleaved order */
    for (int i = 0; i < N; i += 2) DC_REL(d[i], c[i]);
    for (int i = 1; i < N; i += 2) if (d[i]) { D3D11_TEXTURE2D_DESC td = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        EXPECT_HR(ID3D11Device_CreateTexture2D(d[i], &td, NULL, NULL), S_FALSE); }
    for (int i = 1; i < N; i += 2) DC_REL(d[i], c[i]);
}
/* A context obtained from GetImmediateContext is the same object each time and keeps the device alive. */
T(immediate_context_identity_and_lifetime)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL, *c2 = NULL, *c3 = NULL; ID3D11Device *back = NULL;
    REQUIRE(DC_FN());
    if (FAILED(dc_warp(0, NULL, 0, &d, &c, NULL))) { t_skip("no WARP"); return; }
    ID3D11Device_GetImmediateContext(d, &c2); ID3D11Device_GetImmediateContext(d, &c3);
    CHECK(c == c2 && c2 == c3);
    ID3D11DeviceContext_GetDevice(c, &back); CHECK(back == d);
    SAFE_RELEASE(back);
    EXPECT_EQ(ID3D11DeviceContext_GetType(c), D3D11_DEVICE_CONTEXT_IMMEDIATE);
    EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(c), 0);
    /* drop the application's device reference: the context still holds the device */
    SAFE_RELEASE(d);
    ID3D11DeviceContext_GetDevice(c, &back);
    CHECK(back != NULL);
    if (back) { EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(back), S_OK); SAFE_RELEASE(back); }
    SAFE_RELEASE(c3); SAFE_RELEASE(c2); SAFE_RELEASE(c);
}

/* ---- CheckFormatSupport vs CheckFeatureSupport(FORMAT_SUPPORT / FORMAT_SUPPORT2) ---- */
T(format_support_query_paths_agree)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0; UINT f, nok = 0;
    REQUIRE(DC_FN()); DC_NEED_WARP(d, c, fl);
    for (f = 0; f <= 131; f++) {
        UINT s = 0xcccccccc; HRESULT h1 = ID3D11Device_CheckFormatSupport(d, (DXGI_FORMAT)f, &s);
        D3D11_FEATURE_DATA_FORMAT_SUPPORT fs = { (DXGI_FORMAT)f, 0xcccccccc }; HRESULT h2 = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_FORMAT_SUPPORT, &fs, sizeof fs);
        D3D11_FEATURE_DATA_FORMAT_SUPPORT2 f2 = { (DXGI_FORMAT)f, 0xcccccccc }; HRESULT h3 = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_FORMAT_SUPPORT2, &f2, sizeof f2);
        if (SUCCEEDED(h1)) nok++;
        CHECKF(h1 == h2, "format %u: CheckFormatSupport %s vs FORMAT_SUPPORT %s", f, d3dh_hr_name(h1), d3dh_hr_name(h2));
        if (SUCCEEDED(h1)) CHECKF(fs.OutFormatSupport == s, "format %u: %#x vs %#x", f, s, fs.OutFormatSupport);
        /* FORMAT_SUPPORT2 answers E_FAIL for sixteen formats that CheckFormatSupport answers (UNKNOWN, some typeless and
         * 32-bit-per-component 3-channel formats, B5G6R5-family and the 4:2:0 video formats): the exact set is a WARP fact. */
        { static const UINT f2fail[] = { 0, 5, 6, 8, 19, 20, 21, 22, 44, 45, 46, 47, 68, 69, 113, 114 }; int exp = 0;
          for (unsigned k = 0; k < sizeof f2fail / sizeof *f2fail; k++) if (f2fail[k] == f) exp = 1;
          if (SUCCEEDED(h1)) CHECKF((FAILED(h3) ? 1 : 0) == exp, "format %u: FORMAT_SUPPORT2 %s (expected %s)", f, d3dh_hr_name(h3), exp ? "E_FAIL" : "success");
          else CHECKF(FAILED(h3), "format %u: unsupported yet FORMAT_SUPPORT2 %s", f, d3dh_hr_name(h3)); }
    }
    EXPECT_EQ(nok, 106);
    DC_REL(d, c);
}
/* The unsupported-format answer: out value is zero and the failure code is E_FAIL (not E_INVALIDARG). */
T(format_support_unsupported_format_result)
{
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl = 0; UINT s = 0xcccccccc;
    REQUIRE(DC_FN()); DC_NEED_WARP(d, c, fl);
    EXPECT_HR(ID3D11Device_CheckFormatSupport(d, (DXGI_FORMAT)191, &s), E_FAIL); EXPECT_EQ(s, 0);
    s = 0xcccccccc; EXPECT_HR(ID3D11Device_CheckFormatSupport(d, (DXGI_FORMAT)66, &s), E_FAIL); EXPECT_EQ(s, 0);
    s = 0xcccccccc; EXPECT_HR(ID3D11Device_CheckFormatSupport(d, (DXGI_FORMAT)0xffff, &s), E_FAIL); EXPECT_EQ(s, 0);
    DC_REL(d, c);
}
