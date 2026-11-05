/* IDXGIOutput: FindClosestMatchingMode, gamma, surface data, vblank, frame statistics, IDXGIOutput2..5. */
#include "ao.h"

/* ---- FindClosestMatchingMode --------------------------------------------------------- */
AO_TEST_OUTPUT(dxgi_adapter_output_closest_null_args) {
    DXGI_MODE_DESC want, got;
    memset(&want, 0, sizeof want);
    want.Format = DXGI_FORMAT_B8G8R8A8_UNORM; want.Width = 800; want.Height = 600;
    EXPECT_HR(IDXGIOutput_FindClosestMatchingMode(o, NULL, &got, NULL), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIOutput_FindClosestMatchingMode(o, &want, NULL, NULL), DXGI_ERROR_INVALID_CALL);
}
AO_TEST_OUTPUT(dxgi_adapter_output_closest_unknown_format_needs_device) {
    DXGI_MODE_DESC want, got;
    memset(&want, 0, sizeof want); memset(&got, 0xCC, sizeof got);
    want.Format = DXGI_FORMAT_UNKNOWN; want.Width = 800; want.Height = 600;
    EXPECT_HR(IDXGIOutput_FindClosestMatchingMode(o, &want, &got, NULL), DXGI_ERROR_INVALID_CALL);
}
AO_TEST_OUTPUT(dxgi_adapter_output_closest_exact_listed_mode) {
    UINT n = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) != S_OK || n == 0 || n > 5000) { t_skip("no mode list"); return; }
    DXGI_MODE_DESC *m = calloc(n, sizeof *m);
    UINT got_n = n;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &got_n, m) == S_OK && got_n) {
        DXGI_MODE_DESC res; memset(&res, 0xCC, sizeof res);
        HRESULT hr = IDXGIOutput_FindClosestMatchingMode(o, &m[0], &res, NULL);
        EXPECT_HR(hr, S_OK);
        if (hr == S_OK) {
            EXPECT_EQ(res.Width, m[0].Width);
            EXPECT_EQ(res.Height, m[0].Height);
            EXPECT_EQ(res.Format, m[0].Format);
            EXPECT_EQ(res.RefreshRate.Numerator * (UINT64)m[0].RefreshRate.Denominator, m[0].RefreshRate.Numerator * (UINT64)res.RefreshRate.Denominator);
        }
    }
    free(m);
}
AO_TEST_OUTPUT(dxgi_adapter_output_closest_result_is_listed) {
    UINT n = 0;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_SCALING, &n, NULL) != S_OK || n == 0 || n > 5000) { t_skip("no mode list"); return; }
    DXGI_MODE_DESC *m = calloc(n, sizeof *m);
    UINT got_n = n;
    if (IDXGIOutput_GetDisplayModeList(o, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_SCALING, &got_n, m) == S_OK) {
        DXGI_MODE_DESC want, res; memset(&want, 0, sizeof want);
        want.Format = DXGI_FORMAT_B8G8R8A8_UNORM; want.Width = 1000; want.Height = 700;
        HRESULT hr = IDXGIOutput_FindClosestMatchingMode(o, &want, &res, NULL);
        EXPECT_HR(hr, S_OK);
        if (hr == S_OK) {
            int found = 0;
            for (UINT i = 0; i < got_n; i++) if (m[i].Width == res.Width && m[i].Height == res.Height) found = 1;
            CHECK(found);
            EXPECT_EQ(res.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
        }
    }
    free(m);
}
AO_TEST_OUTPUT(dxgi_adapter_output_closest_width_height_zero_pair) {
    DXGI_MODE_DESC want, res;
    memset(&want, 0, sizeof want); memset(&res, 0, sizeof res);
    want.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    HRESULT hr = IDXGIOutput_FindClosestMatchingMode(o, &want, &res, NULL);
    t_note("FindClosestMatchingMode(0x0) -> %s result %ux%u", d3dh_hr_name(hr), res.Width, res.Height);
    want.Width = 800; want.Height = 0;
    hr = IDXGIOutput_FindClosestMatchingMode(o, &want, &res, NULL);
    t_note("FindClosestMatchingMode(800x0) -> %s", d3dh_hr_name(hr));
    want.Width = 0; want.Height = 600;
    hr = IDXGIOutput_FindClosestMatchingMode(o, &want, &res, NULL);
    t_note("FindClosestMatchingMode(0x600) -> %s", d3dh_hr_name(hr));
}
AO_TEST_OUTPUT(dxgi_adapter_output_closest1_null_args) {
    IDXGIOutput1 *o1 = NULL; DXGI_MODE_DESC1 want, got;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput1, (void **)&o1))) { t_skip("IDXGIOutput1 unavailable"); return; }
    memset(&want, 0, sizeof want);
    want.Format = DXGI_FORMAT_B8G8R8A8_UNORM; want.Width = 800; want.Height = 600;
    EXPECT_HR(IDXGIOutput1_FindClosestMatchingMode1(o1, NULL, &got, NULL), DXGI_ERROR_INVALID_CALL);
    EXPECT_HR(IDXGIOutput1_FindClosestMatchingMode1(o1, &want, NULL, NULL), DXGI_ERROR_INVALID_CALL);
    want.Format = DXGI_FORMAT_UNKNOWN;
    EXPECT_HR(IDXGIOutput1_FindClosestMatchingMode1(o1, &want, &got, NULL), DXGI_ERROR_INVALID_CALL);
    IDXGIOutput1_Release(o1);
}
AO_TEST_OUTPUT(dxgi_adapter_output_closest1_exact_listed_mode) {
    IDXGIOutput1 *o1 = NULL; UINT n = 0;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput1, (void **)&o1))) { t_skip("IDXGIOutput1 unavailable"); return; }
    if (IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &n, NULL) == S_OK && n && n < 5000) {
        DXGI_MODE_DESC1 *m = calloc(n, sizeof *m), res;
        UINT g = n;
        if (IDXGIOutput1_GetDisplayModeList1(o1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, &g, m) == S_OK && g) {
            EXPECT_HR(IDXGIOutput1_FindClosestMatchingMode1(o1, &m[0], &res, NULL), S_OK);
            EXPECT_EQ(res.Width, m[0].Width); EXPECT_EQ(res.Height, m[0].Height); CHECK(res.Stereo == FALSE);
        }
        free(m);
    } else t_skip("no mode list");
    IDXGIOutput1_Release(o1);
}

/* ---- gamma ------------------------------------------------------------------------------ */
AO_TEST_OUTPUT(dxgi_adapter_output_gamma_caps) {
    DXGI_GAMMA_CONTROL_CAPABILITIES c;
    memset(&c, 0xCC, sizeof c);
    HRESULT hr = IDXGIOutput_GetGammaControlCapabilities(o, &c);
    t_note("GetGammaControlCapabilities -> %s", d3dh_hr_name(hr));
    if (hr != S_OK) return; /* drivers without gamma support (basic display) may refuse */
    CHECKF(c.NumGammaControlPoints <= 1025, "NumGammaControlPoints %u", c.NumGammaControlPoints);
    CHECK(c.NumGammaControlPoints > 0);
    CHECK(c.MaxConvertedValue >= c.MinConvertedValue);
    t_note("gamma caps: scale %d offset %d points %u range %.3f..%.3f", c.ScaleAndOffsetSupported, 0, c.NumGammaControlPoints,
           c.MinConvertedValue, c.MaxConvertedValue);
    for (UINT i = 1; i < c.NumGammaControlPoints; i++)
        CHECKF(c.ControlPointPositions[i] >= c.ControlPointPositions[i - 1], "control point %u not monotonic", i);
}
AO_TEST_OUTPUT(dxgi_adapter_output_gamma_caps_null) {
    EXPECT_HR(IDXGIOutput_GetGammaControlCapabilities(o, NULL), DXGI_ERROR_INVALID_CALL);
}
AO_TEST_OUTPUT(dxgi_adapter_output_gamma_get_null) {
    EXPECT_HR(IDXGIOutput_GetGammaControl(o, NULL), DXGI_ERROR_INVALID_CALL);
}
AO_TEST_OUTPUT(dxgi_adapter_output_gamma_get_current) {
    static DXGI_GAMMA_CONTROL g;
    memset(&g, 0, sizeof g);
    HRESULT hr = IDXGIOutput_GetGammaControl(o, &g);
    t_note("GetGammaControl -> %s scale %.3f,%.3f,%.3f offset %.3f,%.3f,%.3f", d3dh_hr_name(hr), g.Scale.Red, g.Scale.Green, g.Scale.Blue,
           g.Offset.Red, g.Offset.Green, g.Offset.Blue);
}
AO_TEST_OUTPUT(dxgi_adapter_output_gamma_set_null) {
    /* Only the argument validation path: no gamma ramp is ever applied. */
    HRESULT hr = IDXGIOutput_SetGammaControl(o, NULL);
    EXPECT_HR_FAILED(hr);
    t_note("SetGammaControl(NULL) -> %s", d3dh_hr_name(hr));
}

/* ---- GetDisplaySurfaceData ---------------------------------------------------------------- */
AO_TEST_OUTPUT(dxgi_adapter_output_surface_data_null) {
    EXPECT_HR(IDXGIOutput_GetDisplaySurfaceData(o, NULL), DXGI_ERROR_INVALID_CALL);
}
AO_TEST_OUTPUT(dxgi_adapter_output_surface_data1_null) {
    IDXGIOutput1 *o1 = NULL;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput1, (void **)&o1))) { t_skip("IDXGIOutput1 unavailable"); return; }
    EXPECT_HR(IDXGIOutput1_GetDisplaySurfaceData1(o1, NULL), DXGI_ERROR_INVALID_CALL);
    IDXGIOutput1_Release(o1);
}

/* ---- WaitForVBlank / GetFrameStatistics -------------------------------------------------------- */
typedef struct { IDXGIOutput *o; HRESULT hr; } ao_vb;
static DWORD WINAPI ao_vblank_thread(void *p) { ao_vb *v = p; v->hr = IDXGIOutput_WaitForVBlank(v->o); return 0; }
AO_TEST_OUTPUT(dxgi_adapter_output_wait_for_vblank_bounded) {
    ao_vb v = { o, E_PENDING };
    IDXGIOutput_AddRef(o);
    HANDLE th = CreateThread(NULL, 0, ao_vblank_thread, &v, 0, NULL);
    if (!th) { IDXGIOutput_Release(o); t_skip("CreateThread failed"); return; }
    if (WaitForSingleObject(th, 3000) == WAIT_OBJECT_0) {
        t_note("WaitForVBlank -> %s", d3dh_hr_name(v.hr));
        CHECK(v.hr == S_OK || FAILED(v.hr));
        CloseHandle(th); IDXGIOutput_Release(o);
    } else {
        t_note("WaitForVBlank did not return within 3s (no vblank source); thread left running");
        CloseHandle(th); /* output reference intentionally leaked: the thread still uses it */
    }
}
AO_TEST_OUTPUT(dxgi_adapter_output_frame_stats_null) {
    EXPECT_HR(IDXGIOutput_GetFrameStatistics(o, NULL), DXGI_ERROR_INVALID_CALL);
}
AO_TEST_OUTPUT(dxgi_adapter_output_frame_stats_no_fullscreen_owner) {
    DXGI_FRAME_STATISTICS s; memset(&s, 0, sizeof s);
    HRESULT hr = IDXGIOutput_GetFrameStatistics(o, &s);
    /* Statistics are only available while a swap chain owns the output in fullscreen. */
    t_note("GetFrameStatistics -> %s", d3dh_hr_name(hr));
    CHECK(hr == S_OK || FAILED(hr));
}

/* ---- ownership calls that do not change state ---------------------------------------------------- */
AO_TEST_OUTPUT(dxgi_adapter_output_release_ownership_unowned) {
    /* ReleaseOwnership returns void and is harmless when the output is not owned. */
    IDXGIOutput_ReleaseOwnership(o);
    CHECK(IDXGIOutput_AddRef(o) >= 2);
    IDXGIOutput_Release(o);
}
AO_TEST_OUTPUT(dxgi_adapter_output_take_ownership_null_device) {
    HRESULT hr = IDXGIOutput_TakeOwnership(o, NULL, FALSE);
    EXPECT_HR_FAILED(hr);
    t_note("TakeOwnership(NULL) -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) IDXGIOutput_ReleaseOwnership(o);
}

/* ---- IDXGIOutput2..5 -------------------------------------------------------------------------------- */
AO_TEST_OUTPUT(dxgi_adapter_output_supports_overlays) {
    IDXGIOutput2 *o2 = NULL;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput2, (void **)&o2))) { t_skip("IDXGIOutput2 unavailable"); return; }
    BOOL b = IDXGIOutput2_SupportsOverlays(o2);
    CHECK(b == TRUE || b == FALSE);
    t_note("SupportsOverlays %d", b);
    IDXGIOutput2_Release(o2);
}
AO_TEST_OUTPUT(dxgi_adapter_output_check_overlay_support_null_args) {
    IDXGIOutput3 *o3 = NULL; UINT fl = 0;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput3, (void **)&o3))) { t_skip("IDXGIOutput3 unavailable"); return; }
    HRESULT hr = IDXGIOutput3_CheckOverlaySupport(o3, DXGI_FORMAT_B8G8R8A8_UNORM, NULL, &fl);
    EXPECT_HR_FAILED(hr);
    t_note("CheckOverlaySupport(NULL device) -> %s", d3dh_hr_name(hr));
    IDXGIOutput3_Release(o3);
}
AO_TEST_OUTPUT(dxgi_adapter_output_check_overlay_colorspace_null_args) {
    IDXGIOutput4 *o4 = NULL; UINT fl = 0;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput4, (void **)&o4))) { t_skip("IDXGIOutput4 unavailable"); return; }
    HRESULT hr = IDXGIOutput4_CheckOverlayColorSpaceSupport(o4, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, NULL, &fl);
    EXPECT_HR_FAILED(hr);
    t_note("CheckOverlayColorSpaceSupport(NULL device) -> %s", d3dh_hr_name(hr));
    IDXGIOutput4_Release(o4);
}
AO_TEST_OUTPUT(dxgi_adapter_output_duplicate_output_null_device) {
    IDXGIOutput5 *o5 = NULL; IDXGIOutputDuplication *dup = (IDXGIOutputDuplication *)(UINT_PTR)1;
    if (FAILED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput5, (void **)&o5))) { t_skip("IDXGIOutput5 unavailable"); return; }
    DXGI_FORMAT f1 = DXGI_FORMAT_B8G8R8A8_UNORM;
    HRESULT hr = IDXGIOutput5_DuplicateOutput1(o5, NULL, 0, 1, &f1, &dup);
    EXPECT_HR_FAILED(hr);
    t_note("DuplicateOutput1(NULL device) -> %s", d3dh_hr_name(hr));
    CHECK(dup == NULL || dup == (IDXGIOutputDuplication *)(UINT_PTR)1);
    IDXGIOutput5_Release(o5);
}

/* ---- same object via different enumeration paths ------------------------------------------------------ */
AO_TEST_OUTPUT(dxgi_adapter_output_out_refcount_balance) {
    ULONG base = d3dh_refcount(o);
    IDXGIOutput1 *o1 = NULL;
    if (SUCCEEDED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput1, (void **)&o1))) {
        EXPECT_EQ(d3dh_refcount(o), base + 1);
        IDXGIOutput1_Release(o1);
    }
    EXPECT_EQ(d3dh_refcount(o), base);
}
AO_TEST_OUTPUT(dxgi_adapter_output_out_privdata) {
    static const GUID g = { 0x6a2c8b20, 0x1111, 0x4222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    DWORD in = 0xA5A5A5A5, out = 0; UINT sz = sizeof out;
    EXPECT_HR(IDXGIOutput_SetPrivateData(o, &g, sizeof in, &in), S_OK);
    EXPECT_HR(IDXGIOutput_GetPrivateData(o, &g, &sz, &out), S_OK);
    EXPECT_EQ(out, in);
    sz = 2;
    EXPECT_HR(IDXGIOutput_GetPrivateData(o, &g, &sz, &out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 4);
    IDXGIOutput_SetPrivateData(o, &g, 0, NULL);
}
