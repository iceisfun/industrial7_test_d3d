/* Pinned per-feature-level tables of the Windows 10 22H2 (build 19045) Microsoft Basic Render Driver / WARP: the full
 * CheckFormatSupport + FORMAT_SUPPORT2 word of every DXGI format id 0..132 and the multisample quality levels for
 * 1,2,4,8,16,32 samples.  Other drivers (vendor != 0x1414) legitimately differ and are only reported. */
#include "cf.h"
#include "cf_tables.h"

static DWORD cf_os_build(void)
{
    typedef LONG (WINAPI *rtl_t)(OSVERSIONINFOW *);
    rtl_t f = (rtl_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    OSVERSIONINFOW vi; memset(&vi, 0, sizeof vi); vi.dwOSVersionInfoSize = sizeof vi;
    return (f && f(&vi) == 0) ? vi.dwBuildNumber : 0;
}
static UINT cf_adapter_vendor(ID3D11Device *d)
{
    IDXGIDevice *dd = NULL; IDXGIAdapter *a = NULL; DXGI_ADAPTER_DESC ad; UINT v = 0;
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd))) {
        if (SUCCEEDED(IDXGIDevice_GetAdapter(dd, &a)) && SUCCEEDED(IDXGIAdapter_GetDesc(a, &ad))) v = ad.VendorId;
        SAFE_RELEASE(a); SAFE_RELEASE(dd);
    }
    return v;
}
/* 0 = pin, 1 = report only; -1 = skip */
static int cf_pin_mode(ID3D11Device *d, DWORD *build)
{
    UINT v = cf_adapter_vendor(d);
    *build = cf_os_build();
    if (v != 0x1414) { t_skip("adapter vendor %#x is not the Microsoft Basic Render Driver (0x1414): tables are driver specific", v); return -1; }
    return *build == 19045 ? 0 : 1;
}
static void table_fl(D3D_FEATURE_LEVEL fl, const unsigned (*tbl)[2], const char *const *msq)
{
    ID3D11Device *d = cf_make(fl); DWORD build; int mode, bad = 0; UINT f, c;
    if (!d) { t_skip("feature level %s unavailable", cf_fl_name(fl)); return; }
    mode = cf_pin_mode(d, &build);
    if (mode < 0) { SAFE_RELEASE(d); return; }
    for (f = 0; f < 133; f++) {
        UINT s = 0xdeadbeef; HRESULT hr = ID3D11Device_CheckFormatSupport(d, (DXGI_FORMAT)f, &s);
        D3D11_FEATURE_DATA_FORMAT_SUPPORT2 f2; HRESULT h2;
        f2.InFormat = (DXGI_FORMAT)f; f2.OutFormatSupport2 = 0xdeadbeef;
        h2 = ID3D11Device_CheckFeatureSupport(d, D3D11_FEATURE_FORMAT_SUPPORT2, &f2, sizeof f2);
        {
            int ok1 = tbl[f][0] == 0xffffffffu ? (hr == E_FAIL && s == 0) : (hr == S_OK && s == tbl[f][0]);
            int ok2 = tbl[f][1] == 0xffffffffu ? (h2 == E_FAIL && f2.OutFormatSupport2 == 0) : (h2 == S_OK && f2.OutFormatSupport2 == tbl[f][1]);
            if (!ok1 || !ok2) {
                bad++;
                if (mode) t_note("%s %s: support %s %#x / support2 %s %#x, table %#x / %#x (0xffffffff = E_FAIL)", cf_fl_name(fl), cf_fmt_name(f), d3dh_hr_name(hr), s, d3dh_hr_name(h2), f2.OutFormatSupport2, tbl[f][0], tbl[f][1]);
                else t_check(0, __FILE__, __LINE__, "format support words", "%s %s: got %s %#x / %s %#x, want %#x / %#x (0xffffffff = E_FAIL)", cf_fl_name(fl), cf_fmt_name(f), d3dh_hr_name(hr), s, d3dh_hr_name(h2), f2.OutFormatSupport2, tbl[f][0], tbl[f][1]);
            }
        }
        if (f == 0) continue;
        for (c = 0; c < 6; c++) {
            UINT q = 0xdeadbeef, n = 1u << c; char w = msq[f][c], got;
            hr = ID3D11Device_CheckMultisampleQualityLevels(d, (DXGI_FORMAT)f, n, &q);
            got = hr == S_OK ? (char)('0' + q) : (hr == E_INVALIDARG ? 'I' : '?');
            if (got != w) { bad++; if (mode) t_note("%s %s x%u quality levels %c want %c", cf_fl_name(fl), cf_fmt_name(f), n, got, w);
                            else t_check(0, __FILE__, __LINE__, "multisample quality levels", "%s %s x%u: got %c (%s) want %c", cf_fl_name(fl), cf_fmt_name(f), n, got, d3dh_hr_name(hr), w); }
        }
    }
    if (mode && bad) t_note("%s: %d table differences on OS build %lu (tables are for 19045)", cf_fl_name(fl), bad, (unsigned long)build);
    SAFE_RELEASE(d);
}
#define TBL(n, l) TEST(d3d11_caps_formats_warp_table_fl##n) { table_fl(l, cf_tbl_##n, cf_msq_##n); }
TBL(9_1, D3D_FEATURE_LEVEL_9_1)
TBL(9_3, D3D_FEATURE_LEVEL_9_3)
TBL(10_0, D3D_FEATURE_LEVEL_10_0)
TBL(10_1, D3D_FEATURE_LEVEL_10_1)
TBL(11_0, D3D_FEATURE_LEVEL_11_0)
TBL(11_1, D3D_FEATURE_LEVEL_11_1)

/* ---- CheckFeatureSupport data per feature level ---------------------------------------------------------------------------------- */
#include "cf_feat_tables.h"
static void feat_fl(D3D_FEATURE_LEVEL fl, const cf_featrow *rows)
{
    ID3D11Device *d = cf_make(fl); DWORD build; int mode, bad = 0;
    if (!d) { t_skip("feature level %s unavailable", cf_fl_name(fl)); return; }
    mode = cf_pin_mode(d, &build);
    if (mode < 0) { SAFE_RELEASE(d); return; }
    for (; rows->feat >= 0; rows++) {
        unsigned buf[16]; unsigned i; HRESULT hr;
        memset(buf, 0, sizeof buf);
        hr = ID3D11Device_CheckFeatureSupport(d, (D3D11_FEATURE)rows->feat, buf, rows->size);
        if (hr != S_OK) { bad++; t_check(mode != 0, __FILE__, __LINE__, "feature query", "%s feature %d: %s", cf_fl_name(fl), rows->feat, d3dh_hr_name(hr)); continue; }
        for (i = 0; i < rows->size / 4; i++)
            if (buf[i] != rows->v[i]) {
                bad++;
                if (mode) t_note("%s feature %d dword %u = %#x want %#x", cf_fl_name(fl), rows->feat, i, buf[i], rows->v[i]);
                else t_check(0, __FILE__, __LINE__, "feature data", "%s feature %d dword %u: got %#x want %#x", cf_fl_name(fl), rows->feat, i, buf[i], rows->v[i]);
            }
    }
    if (mode && bad) t_note("%s: %d feature-data differences on OS build %lu", cf_fl_name(fl), bad, (unsigned long)build);
    SAFE_RELEASE(d);
}
#define FEATTBL(n, l) TEST(d3d11_caps_formats_warp_features_fl##n) { feat_fl(l, cf_feat_##n); }
FEATTBL(9_1, D3D_FEATURE_LEVEL_9_1)
FEATTBL(9_3, D3D_FEATURE_LEVEL_9_3)
FEATTBL(10_0, D3D_FEATURE_LEVEL_10_0)
FEATTBL(10_1, D3D_FEATURE_LEVEL_10_1)
FEATTBL(11_0, D3D_FEATURE_LEVEL_11_0)
FEATTBL(11_1, D3D_FEATURE_LEVEL_11_1)
