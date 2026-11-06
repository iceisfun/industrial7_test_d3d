/* IDXGIOutput: enumeration, descriptors, display mode lists and the count protocol. Skips when no output exists (headless). */
#include "ao.h"

static const DXGI_FORMAT ao_mode_formats[] = {
    DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_B8G8R8X8_UNORM,
};
#define AO_NFMT (sizeof ao_mode_formats / sizeof *ao_mode_formats)

/* ---- EnumOutputs ---------------------------------------------------------- */
AO_TEST_FACTORY(dxgi_adapter_output_enumoutputs_end_condition) {
    UINT na = ao_count_adapters(f);
    if (!na) { t_skip("no adapters"); return; }
    for (UINT i = 0; i < na; i++) {
        IDXGIAdapter1 *a = NULL; UINT n = 0;
        if (IDXGIFactory1_EnumAdapters1(f, i, &a) != S_OK) { CHECK(0); continue; }
        for (;; n++) {
            IDXGIOutput *o = (IDXGIOutput *)(UINT_PTR)0x1234;
            HRESULT hr = IDXGIAdapter1_EnumOutputs(a, n, &o);
            if (hr != S_OK) { EXPECT_HR(hr, DXGI_ERROR_NOT_FOUND); CHECK(o == NULL); break; }
            CHECK(o != NULL);
            IDXGIOutput_Release(o);
            if (n > 32) { CHECK(0); break; }
        }
        IDXGIOutput *o = (IDXGIOutput *)(UINT_PTR)0x1234;
        EXPECT_HR(IDXGIAdapter1_EnumOutputs(a, n + 5, &o), DXGI_ERROR_NOT_FOUND);
        CHECK(o == NULL);
        o = (IDXGIOutput *)(UINT_PTR)0x1234;
        EXPECT_HR(IDXGIAdapter1_EnumOutputs(a, 0xFFFFFFFFu, &o), DXGI_ERROR_NOT_FOUND);
        CHECK(o == NULL);
        t_note("adapter %u has %u outputs", i, n);
        IDXGIAdapter1_Release(a);
    }
}
AO_TEST_ADAPTER(dxgi_adapter_output_enumoutputs_null_out) {
    HRESULT hr = IDXGIAdapter1_EnumOutputs(a, 0, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("EnumOutputs(0, NULL) -> %s", d3dh_hr_name(hr));
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_getparent_adapter) {
    IDXGIAdapter *p = NULL;
    EXPECT_HR(IDXGIOutput_GetParent(o, &IID_IDXGIAdapter, (void **)&p), S_OK);
    if (p) { t_note("output parent is the enumerating adapter object: %d", ao_same_object((IUnknown *)p, (IUnknown *)a)); IDXGIAdapter_Release(p); }
    IDXGIAdapter1 *p1 = NULL;
    EXPECT_HR(IDXGIOutput_GetParent(o, &IID_IDXGIAdapter1, (void **)&p1), S_OK);
    if (p1) IDXGIAdapter1_Release(p1);
    void *bad = (void *)(UINT_PTR)1;
    EXPECT_HR_FAILED(IDXGIOutput_GetParent(o, &IID_IDXGIFactory, &bad));
    CHECK(bad == NULL);
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_qi_interfaces) {
    static const struct { const IID *iid; const char *n; int req; } t[] = {
        { &IID_IUnknown, "IUnknown", 1 }, { &IID_IDXGIObject, "IDXGIObject", 1 }, { &IID_IDXGIOutput, "IDXGIOutput", 1 },
        { &IID_IDXGIOutput1, "IDXGIOutput1", 1 }, { &IID_IDXGIOutput2, "IDXGIOutput2", 1 }, { &IID_IDXGIOutput3, "IDXGIOutput3", 1 },
        { &IID_IDXGIOutput4, "IDXGIOutput4", 0 }, { &IID_IDXGIOutput5, "IDXGIOutput5", 0 }, { &IID_IDXGIOutput6, "IDXGIOutput6", 0 },
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        void *p = (void *)(UINT_PTR)1;
        HRESULT hr = IDXGIOutput_QueryInterface(o, t[i].iid, &p);
        if (t[i].req) CHECKF(hr == S_OK, "QI %s -> %s", t[i].n, d3dh_hr_name(hr)); else if (hr != S_OK) t_note("QI %s -> %s", t[i].n, d3dh_hr_name(hr));
        if (hr == S_OK) { CHECK(ao_same_object((IUnknown *)o, (IUnknown *)p)); IUnknown_Release((IUnknown *)p); } else CHECK(p == NULL);
    }
    void *p = (void *)(UINT_PTR)1;
    EXPECT_HR(IDXGIOutput_QueryInterface(o, &IID_IDXGIAdapter, &p), E_NOINTERFACE);
    CHECK(p == NULL);
}

/* ---- GetDesc ---------------------------------------------------------------- */
AO_TEST_OUTPUT(dxgi_adapter_output_out_getdesc) {
    DXGI_OUTPUT_DESC d;
    memset(&d, 0xCC, sizeof d);
    EXPECT_HR(IDXGIOutput_GetDesc(o, &d), S_OK);
    CHECK(ao_wide_terminated(d.DeviceName, 32));
    CHECK(d.DeviceName[0] == L'\\' && d.DeviceName[1] == L'\\' && d.DeviceName[2] == L'.' && d.DeviceName[3] == L'\\');
    CHECK(d.DesktopCoordinates.right >= d.DesktopCoordinates.left);
    CHECK(d.DesktopCoordinates.bottom >= d.DesktopCoordinates.top);
    CHECK(d.Rotation >= DXGI_MODE_ROTATION_UNSPECIFIED && d.Rotation <= DXGI_MODE_ROTATION_ROTATE270);
    CHECK(d.AttachedToDesktop == TRUE || d.AttachedToDesktop == FALSE);
    if (d.AttachedToDesktop) CHECK(d.Monitor != NULL);
    t_note("output %ls attached %d rect %ld,%ld,%ld,%ld rot %d", d.DeviceName, d.AttachedToDesktop, d.DesktopCoordinates.left,
           d.DesktopCoordinates.top, d.DesktopCoordinates.right, d.DesktopCoordinates.bottom, d.Rotation);
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_getdesc_monitor_valid) {
    DXGI_OUTPUT_DESC d; MONITORINFOEXW mi;
    EXPECT_HR_OK(IDXGIOutput_GetDesc(o, &d));
    memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    if (!d.Monitor || !GetMonitorInfoW(d.Monitor, (MONITORINFO *)&mi)) { t_note("GetMonitorInfo failed %lu", GetLastError()); return; }
    CHECK(!lstrcmpW(mi.szDevice, d.DeviceName));
    CHECK(mi.rcMonitor.left == d.DesktopCoordinates.left && mi.rcMonitor.top == d.DesktopCoordinates.top);
    CHECK(mi.rcMonitor.right == d.DesktopCoordinates.right && mi.rcMonitor.bottom == d.DesktopCoordinates.bottom);
}
static void out_getdesc_null_call(void *p) { IDXGIOutput_GetDesc(((struct { IDXGIOutput *o; } *)p)->o, NULL); }
AO_TEST_OUTPUT(dxgi_adapter_output_out_getdesc_null) {
    /* Win10 (probed): the output descriptor is written unconditionally, NULL raises an access violation */
    struct { IDXGIOutput *o; } c = { o };
    EXPECT_EQ(t_try(out_getdesc_null_call, &c), 0xc0000005u);
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_names_unique) {
    DXGI_OUTPUT_DESC d[16]; UINT n = 0;
    for (UINT i = 0; i < 16; i++) {
        IDXGIOutput *x = NULL;
        if (IDXGIAdapter1_EnumOutputs(a, i, &x) != S_OK) break;
        EXPECT_HR_OK(IDXGIOutput_GetDesc(x, &d[n++]));
        IDXGIOutput_Release(x);
    }
    for (UINT i = 0; i < n; i++) for (UINT j = i + 1; j < n; j++) CHECK(lstrcmpW(d[i].DeviceName, d[j].DeviceName) != 0);
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_desc_idempotent) {
    DXGI_OUTPUT_DESC d1, d2;
    EXPECT_HR_OK(IDXGIOutput_GetDesc(o, &d1));
    EXPECT_HR_OK(IDXGIOutput_GetDesc(o, &d2));
    CHECK(!lstrcmpW(d1.DeviceName, d2.DeviceName));
    EXPECT_EQ(d1.Monitor, d2.Monitor);
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_desc1) {
    IDXGIOutput6 *o6 = NULL; DXGI_OUTPUT_DESC1 d1; DXGI_OUTPUT_DESC d;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput6, (void **)&o6))) { t_skip("IDXGIOutput6 unavailable"); return; }
    memset(&d1, 0xCC, sizeof d1);
    EXPECT_HR(IDXGIOutput6_GetDesc1(o6, &d1), S_OK);
    EXPECT_HR_OK(IDXGIOutput_GetDesc(o, &d));
    CHECK(!lstrcmpW(d.DeviceName, d1.DeviceName));
    EXPECT_EQ(d.Monitor, d1.Monitor);
    CHECK(d.DesktopCoordinates.left == d1.DesktopCoordinates.left && d.DesktopCoordinates.right == d1.DesktopCoordinates.right);
    EXPECT_EQ(d.Rotation, d1.Rotation);
    CHECK(d1.MaxLuminance >= d1.MinLuminance);
    CHECKF(d1.BitsPerColor <= 16, "BitsPerColor %u", d1.BitsPerColor);
    CHECKF((int)d1.ColorSpace >= 0 && (int)d1.ColorSpace <= (int)DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020 + 1, "ColorSpace %d", d1.ColorSpace);
    /* An SDR desktop reports sRGB full-range G22 (0). */
    t_note("desc1 colorspace %d bpc %u lum %.2f..%.2f full %.2f", d1.ColorSpace, d1.BitsPerColor, d1.MinLuminance, d1.MaxLuminance, d1.MaxFullFrameLuminance);
    for (int i = 0; i < 2; i++) {
        CHECK(d1.RedPrimary[i] >= 0.0f && d1.RedPrimary[i] <= 1.0f);
        CHECK(d1.GreenPrimary[i] >= 0.0f && d1.GreenPrimary[i] <= 1.0f);
        CHECK(d1.BluePrimary[i] >= 0.0f && d1.BluePrimary[i] <= 1.0f);
        CHECK(d1.WhitePoint[i] >= 0.0f && d1.WhitePoint[i] <= 1.0f);
    }
    IDXGIOutput6_Release(o6);
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_desc1_null) {
    IDXGIOutput6 *o6 = NULL;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput6, (void **)&o6))) { t_skip("IDXGIOutput6 unavailable"); return; }
    HRESULT hr = IDXGIOutput6_GetDesc1(o6, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("GetDesc1(NULL) -> %s", d3dh_hr_name(hr));
    IDXGIOutput6_Release(o6);
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_hdr_colorspace_flags) {
    IDXGIOutput6 *o6 = NULL; UINT fl = 0xFFFFFFFFu;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput6, (void **)&o6))) { t_skip("IDXGIOutput6 unavailable"); return; }
    EXPECT_HR(IDXGIOutput6_CheckHardwareCompositionSupport(o6, &fl), S_OK);
    CHECK((fl & ~(UINT)(DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN | DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_WINDOWED |
                        DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_CURSOR_STRETCHED)) == 0);
    t_note("hardware composition flags 0x%x", fl);
    IDXGIOutput6_Release(o6);
}

/* ---- GetDisplayModeList count protocol ------------------------------------------ */
AO_TEST_OUTPUT(dxgi_adapter_output_modes_null_buffer_count) {
    for (size_t i = 0; i < AO_NFMT; i++) {
        UINT n = 0xDEAD;
        HRESULT hr = IDXGIOutput_GetDisplayModeList(o, ao_mode_formats[i], 0, &n, NULL);
        if (SUCCEEDED(hr)) { EXPECT_HR(hr, S_OK); CHECK(n != 0xDEAD); }
        t_note("format %d: hr %s count %u", ao_mode_formats[i], d3dh_hr_name(hr), n);
    }
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_null_count_ptr) {
    HRESULT hr = IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_R8G8B8A8_UNORM, 0, NULL, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("GetDisplayModeList(NULL count) -> %s", d3dh_hr_name(hr));
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_fetch_exact) {
    UINT n = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n == 0) { t_skip("no B8G8R8A8 modes"); return; }
    if (n > 100000) { t_skip("too many modes"); return; }
    DXGI_MODE_DESC *m = calloc(n + 2, sizeof *m);
    UINT got = n;
    EXPECT_HR(IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, m), S_OK);
    EXPECT_EQ(got, n);
    free(m);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_large_buffer_updates_count) {
    UINT n = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n == 0 || n > 5000) { t_skip("no usable mode list"); return; }
    DXGI_MODE_DESC *m = calloc(n + 16, sizeof *m);
    UINT got = n + 10;
    EXPECT_HR(IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, m), S_OK);
    EXPECT_EQ(got, n);
    free(m);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_small_buffer_more_data) {
    UINT n = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n < 2) { t_skip("need >= 2 modes (have %u)", n); return; }
    DXGI_MODE_DESC *m = calloc(n + 1, sizeof *m);
    UINT got = n - 1;
    EXPECT_HR(IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, m), DXGI_ERROR_MORE_DATA);
    t_note("MORE_DATA with buffer %u of %u: count after = %u, first entry %ux%u", n - 1, n, got, m[0].Width, m[0].Height);
    got = 1;
    EXPECT_HR(IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, m), DXGI_ERROR_MORE_DATA);
    t_note("after MORE_DATA first entry %ux%u", m[0].Width, m[0].Height);
    free(m);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_zero_count_with_buffer) {
    UINT n = 0; DXGI_MODE_DESC m;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n == 0) { t_skip("no modes"); return; }
    UINT got = 0;
    HRESULT hr = IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, &m);
    t_note("count 0 with buffer hr %s", d3dh_hr_name(hr));
    t_note("count 0 with buffer: count after %u", got);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_entries_valid) {
    for (size_t k = 0; k < AO_NFMT; k++) {
        UINT n = 0;
        DXGI_FORMAT fmt = ao_mode_formats[k];
        if (IDXGIOutput_GetDisplayModeList(o, fmt, 0, &n, NULL) != S_OK || n == 0 || n > 5000) continue;
        DXGI_MODE_DESC *m = calloc(n, sizeof *m);
        UINT got = n;
        if (IDXGIOutput_GetDisplayModeList(o, fmt, 0, &got, m) == S_OK) {
            for (UINT i = 0; i < got; i++) {
                CHECK(m[i].Width > 0 && m[i].Height > 0);
                EXPECT_EQ(m[i].Format, fmt);
                CHECK(m[i].RefreshRate.Denominator != 0);
                CHECK(m[i].RefreshRate.Numerator != 0);
                CHECK(m[i].ScanlineOrdering >= DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED && m[i].ScanlineOrdering <= DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST);
                CHECK(m[i].Scaling >= DXGI_MODE_SCALING_UNSPECIFIED && m[i].Scaling <= DXGI_MODE_SCALING_STRETCHED);
            }
        }
        free(m);
    }
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_no_duplicates) {
    UINT n = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n == 0 || n > 600) { t_skip("no usable list"); return; }
    DXGI_MODE_DESC *m = calloc(n, sizeof *m);
    UINT got = n;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, m) == S_OK) {
        for (UINT i = 0; i < got; i++) for (UINT j = i + 1; j < got; j++)
            CHECKF(memcmp(&m[i], &m[j], sizeof m[i]) != 0, "duplicate mode %u/%u", i, j);
    }
    free(m);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_scaling_flag_superset) {
    UINT n0 = 0, n1 = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n0, NULL) != S_OK) { t_skip("no list"); return; }
    HRESULT hr = IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_SCALING, &n1, NULL);
    EXPECT_HR(hr, S_OK);
    CHECKF(n1 >= n0, "scaling %u < default %u", n1, n0);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_interlaced_flag_superset) {
    UINT n0 = 0, n1 = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n0, NULL) != S_OK) { t_skip("no list"); return; }
    HRESULT hr = IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &n1, NULL);
    EXPECT_HR(hr, S_OK);
    CHECKF(n1 >= n0, "interlaced %u < default %u", n1, n0);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_default_excludes_interlaced) {
    UINT n = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n == 0 || n > 5000) { t_skip("no list"); return; }
    DXGI_MODE_DESC *m = calloc(n, sizeof *m);
    UINT got = n;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, m) == S_OK)
        for (UINT i = 0; i < got; i++)
            CHECKF(m[i].ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST && m[i].ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST,
                   "interlaced mode %u listed without flag", i);
    free(m);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_unknown_format) {
    UINT n = 0;
    HRESULT hr = IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_UNKNOWN, 0, &n, NULL);
    t_note("GetDisplayModeList(UNKNOWN) -> %s count %u", d3dh_hr_name(hr), n);
    if (SUCCEEDED(hr)) CHECK(n == 0);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_unsupported_format) {
    UINT n = 0xDEAD;
    HRESULT hr = IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_BC1_UNORM, 0, &n, NULL);
    t_note("GetDisplayModeList(BC1) -> %s count %u", d3dh_hr_name(hr), n);
    if (SUCCEEDED(hr)) CHECK(n == 0);
    hr = IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_D32_FLOAT, 0, &n, NULL);
    t_note("GetDisplayModeList(D32) -> %s count %u", d3dh_hr_name(hr), n);
    hr = IDXGIOutput_GetDisplayModeList(o, (DXGI_FORMAT)0x7fff, 0, &n, NULL);
    t_note("GetDisplayModeList(0x7fff) -> %s", d3dh_hr_name(hr));
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_bad_flags) {
    UINT n = 0;
    HRESULT hr = IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0x80000000u, &n, NULL);
    t_note("GetDisplayModeList(undefined flag bit) -> %s", d3dh_hr_name(hr));
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_repeatable) {
    UINT n1 = 0, n2 = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &n1, NULL) != S_OK) { t_skip("no list"); return; }
    EXPECT_HR(IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &n2, NULL), S_OK);
    EXPECT_EQ(n1, n2);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes_current_desktop_mode_listed) {
    DXGI_OUTPUT_DESC d; DEVMODEW dm; UINT n = 0;
    EXPECT_HR_OK(IDXGIOutput_GetDesc(o, &d));
    memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (!EnumDisplaySettingsW(d.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) { t_skip("EnumDisplaySettings failed"); return; }
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_SCALING, &n, NULL) != S_OK || n == 0 || n > 5000) { t_skip("no list"); return; }
    DXGI_MODE_DESC *m = calloc(n, sizeof *m);
    UINT got = n, found = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_SCALING, &got, m) == S_OK)
        for (UINT i = 0; i < got; i++) if (m[i].Width == dm.dmPelsWidth && m[i].Height == dm.dmPelsHeight) found++;
    t_note("current desktop mode %lux%lu appears %u times", dm.dmPelsWidth, dm.dmPelsHeight, found);
    free(m);
}

/* ---- IDXGIOutput1::GetDisplayModeList1 ---------------------------------------------- */
static IDXGIOutput1 *ao_o1(IDXGIOutput *o) {
    IDXGIOutput1 *o1 = NULL;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput1, (void **)&o1))) { t_skip("IDXGIOutput1 unavailable"); return NULL; }
    return o1;
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes1_count_matches_legacy) {
    IDXGIOutput1 *o1 = ao_o1(o); UINT n0 = 0, n1 = 0;
    if (!o1) return;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n0, NULL) != S_OK) { t_skip("no list"); IDXGIOutput1_Release(o1); return; }
    EXPECT_HR(IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n1, NULL), S_OK);
    EXPECT_EQ(n0, n1);
    IDXGIOutput1_Release(o1);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes1_entries_not_stereo) {
    IDXGIOutput1 *o1 = ao_o1(o); UINT n = 0;
    if (!o1) return;
    if (IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n == 0 || n > 5000) { t_skip("no list"); IDXGIOutput1_Release(o1); return; }
    DXGI_MODE_DESC1 *m = calloc(n, sizeof *m);
    UINT got = n;
    EXPECT_HR(IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, m), S_OK);
    for (UINT i = 0; i < got; i++) { CHECK(m[i].Stereo == FALSE); CHECK(m[i].Width > 0 && m[i].Height > 0); EXPECT_EQ(m[i].Format, DXGI_FORMAT_B8G8R8A8_UNORM); }
    free(m);
    IDXGIOutput1_Release(o1);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes1_stereo_flag) {
    IDXGIOutput1 *o1 = ao_o1(o); UINT n = 0;
    if (!o1) return;
    HRESULT hr = IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_STEREO, &n, NULL);
    t_note("GetDisplayModeList1(STEREO) -> %s count %u", d3dh_hr_name(hr), n);
    IDXGIOutput1_Release(o1);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes1_small_buffer) {
    IDXGIOutput1 *o1 = ao_o1(o); UINT n = 0;
    if (!o1) return;
    if (IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n < 2 || n > 5000) { t_skip("need >= 2 modes"); IDXGIOutput1_Release(o1); return; }
    DXGI_MODE_DESC1 *m = calloc(n, sizeof *m);
    UINT got = 1;
    EXPECT_HR(IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got, m), DXGI_ERROR_MORE_DATA);
    free(m);
    IDXGIOutput1_Release(o1);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes1_null_count) {
    IDXGIOutput1 *o1 = ao_o1(o);
    if (!o1) return;
    EXPECT_HR_FAILED(IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, NULL, NULL));
    IDXGIOutput1_Release(o1);
}
AO_TEST_OUTPUT(dxgi_adapter_output_modes1_matches_legacy_entries) {
    IDXGIOutput1 *o1 = ao_o1(o); UINT n = 0;
    if (!o1) return;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n == 0 || n > 2000) { t_skip("no list"); IDXGIOutput1_Release(o1); return; }
    DXGI_MODE_DESC *a0 = calloc(n, sizeof *a0); DXGI_MODE_DESC1 *a1 = calloc(n, sizeof *a1);
    UINT g0 = n, g1 = n;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &g0, a0) == S_OK &&
        IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &g1, a1) == S_OK && g0 == g1) {
        for (UINT i = 0; i < g0; i++) {
            EXPECT_EQ(a0[i].Width, a1[i].Width); EXPECT_EQ(a0[i].Height, a1[i].Height);
            EXPECT_EQ(a0[i].RefreshRate.Numerator, a1[i].RefreshRate.Numerator);
            EXPECT_EQ(a0[i].RefreshRate.Denominator, a1[i].RefreshRate.Denominator);
            EXPECT_EQ(a0[i].Scaling, a1[i].Scaling); EXPECT_EQ(a0[i].ScanlineOrdering, a1[i].ScanlineOrdering);
        }
    }
    free(a0); free(a1);
    IDXGIOutput1_Release(o1);
}
