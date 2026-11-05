#include "dxm.h"
/* Output Protection Manager entry points. Argument validation is hard-checked; anything that needs a protected
 * output on a real adapter is reported with t_note (virtual display adapters have none). */

UDECL(HRESULT, OPMGetVideoOutputForTarget, (const LUID *, ULONG, OPM_VIDEO_OUTPUT_SEMANTICS, IOPMVideoOutput **));

#define E_INVALIDARG_ ((HRESULT)0x80070057)
#define HR_INVALID_MONITOR ((HRESULT)0x800705B5)   /* HRESULT_FROM_WIN32(ERROR_INVALID_MONITOR_HANDLE) */

TEST(dxva2_monitor_video_opm_target_invalid_semantics) {
    OPMGetVideoOutputForTarget_pfn f = UN(OPMGetVideoOutputForTarget); REQUIRE(f);
    LUID luid = { 0, 0 };
    static const int bad[] = { 3, 4, -1, 100, 0x7FFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        IOPMVideoOutput *out = (IOPMVideoOutput *)(ULONG_PTR)0x1;
        HRESULT hr = f(&luid, 0, (OPM_VIDEO_OUTPUT_SEMANTICS)bad[i], &out);
        EXPECT_HR(hr, E_INVALIDARG_);          /* semantics are converted before any adapter is looked at */
        EXPECT_EQ(out, 1);                      /* output pointer untouched */
    }
}
TEST(dxva2_monitor_video_opm_target_unknown_adapter) {
    OPMGetVideoOutputForTarget_pfn f = UN(OPMGetVideoOutputForTarget); REQUIRE(f);
    LUID luid = { 0x7FFFFFF1, 0x1234 };   /* no such adapter */
    for (int sem = 0; sem <= 2; sem++) {
        IOPMVideoOutput *out = NULL;
        HRESULT hr = f(&luid, 0, (OPM_VIDEO_OUTPUT_SEMANTICS)sem, &out);
        CHECKF(FAILED(hr), "semantics %d -> %s", sem, d3dh_hr_name(hr));
        CHECK(out == NULL);
        t_note("unknown LUID, semantics %d -> %s", sem, d3dh_hr_name(hr));
    }
}
TEST(dxva2_monitor_video_opm_target_zero_luid_each_semantics) {
    OPMGetVideoOutputForTarget_pfn f = UN(OPMGetVideoOutputForTarget); REQUIRE(f);
    LUID luid = { 0, 0 };
    for (int sem = 0; sem <= 2; sem++) {
        IOPMVideoOutput *out = NULL;
        HRESULT hr = f(&luid, 0xFFFFFFFF, (OPM_VIDEO_OUTPUT_SEMANTICS)sem, &out);
        CHECK(FAILED(hr));
        CHECK(out == NULL);
    }
}
TEST(dxva2_monitor_video_opm_hmonitor_null) {
    HRESULT (WINAPI *f)(HMONITOR, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG *, IOPMVideoOutput ***) = U(OPMGetVideoOutputsFromHMONITOR); REQUIRE(f);
    for (int sem = 0; sem <= 2; sem++) {
        ULONG n = 0x55; IOPMVideoOutput **arr = (IOPMVideoOutput **)(ULONG_PTR)0x1;
        HRESULT hr = f(NULL, (OPM_VIDEO_OUTPUT_SEMANTICS)sem, &n, &arr);
        EXPECT_HR(hr, HR_INVALID_MONITOR);     /* the HMONITOR is resolved first */
        EXPECT_EQ(n, 0x55);
        EXPECT_EQ(arr, 1);
    }
}
TEST(dxva2_monitor_video_opm_hmonitor_null_invalid_semantics) {
    HRESULT (WINAPI *f)(HMONITOR, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG *, IOPMVideoOutput ***) = U(OPMGetVideoOutputsFromHMONITOR); REQUIRE(f);
    ULONG n = 0; IOPMVideoOutput **arr = NULL;
    HRESULT hr = f(NULL, (OPM_VIDEO_OUTPUT_SEMANTICS)7, &n, &arr);
    EXPECT_HR(hr, HR_INVALID_MONITOR);          /* monitor lookup precedes semantics validation */
}
TEST(dxva2_monitor_video_opm_hmonitor_bogus) {
    HRESULT (WINAPI *f)(HMONITOR, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG *, IOPMVideoOutput ***) = U(OPMGetVideoOutputsFromHMONITOR); REQUIRE(f);
    ULONG n = 0; IOPMVideoOutput **arr = NULL;
    EXPECT_HR(f((HMONITOR)(ULONG_PTR)0x5, OPM_VOS_OPM_SEMANTICS, &n, &arr), HR_INVALID_MONITOR);
}
TEST(dxva2_monitor_video_opm_hmonitor_primary) {
    HRESULT (WINAPI *f)(HMONITOR, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG *, IOPMVideoOutput ***) = U(OPMGetVideoOutputsFromHMONITOR); REQUIRE(f);
    HMONITOR hm = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (!hm) { t_skip("no primary monitor"); return; }
    for (int sem = 0; sem <= 1; sem++) {
        ULONG n = 0; IOPMVideoOutput **arr = NULL;
        HRESULT hr = f(hm, (OPM_VIDEO_OUTPUT_SEMANTICS)sem, &n, &arr);
        if (SUCCEEDED(hr)) {
            CHECK(n == 0 || arr != NULL);
            t_note("semantics %d: %lu output(s)", sem, n);
            for (ULONG i = 0; i < n; i++) {
                if (arr[i]) {
                    CHECK(d3dh_refcount(arr[i]) >= 1);
                    IOPMVideoOutput_Release(arr[i]);
                }
            }
            CoTaskMemFree(arr);
        } else t_note("semantics %d -> %s (no protected output)", sem, d3dh_hr_name(hr));
    }
}
TEST(dxva2_monitor_video_opm_hmonitor_primary_invalid_semantics) {
    HRESULT (WINAPI *f)(HMONITOR, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG *, IOPMVideoOutput ***) = U(OPMGetVideoOutputsFromHMONITOR); REQUIRE(f);
    HMONITOR hm = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (!hm) { t_skip("no primary monitor"); return; }
    ULONG n = 0; IOPMVideoOutput **arr = NULL;
    HRESULT hr = f(hm, (OPM_VIDEO_OUTPUT_SEMANTICS)3, &n, &arr);
    CHECK(FAILED(hr));
    if (hr != E_INVALIDARG_) t_note("invalid semantics on primary -> %s (earlier stage failed first)", d3dh_hr_name(hr));
    else EXPECT_HR(hr, E_INVALIDARG_);
}
TEST(dxva2_monitor_video_opm_d3d9_object) {
    HRESULT (WINAPI *f)(IDirect3DDevice9 *, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG *, IOPMVideoOutput ***) = U(OPMGetVideoOutputsFromIDirect3DDevice9Object); REQUIRE(f);
    DXM_D9(d);
    ULONG n = 0; IOPMVideoOutput **arr = NULL;
    HRESULT hr = f(d.dev, OPM_VOS_COPP_SEMANTICS, &n, &arr);
    if (SUCCEEDED(hr)) {
        t_note("%lu OPM output(s) from D3D9 device", n);
        for (ULONG i = 0; i < n; i++) if (arr && arr[i]) IOPMVideoOutput_Release(arr[i]);
        if (arr) CoTaskMemFree(arr);
    } else t_note("OPMGetVideoOutputsFromIDirect3DDevice9Object -> %s", d3dh_hr_name(hr));
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_opm_d3d9_object_invalid_semantics) {
    HRESULT (WINAPI *f)(IDirect3DDevice9 *, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG *, IOPMVideoOutput ***) = U(OPMGetVideoOutputsFromIDirect3DDevice9Object); REQUIRE(f);
    DXM_D9(d);
    ULONG n = 0; IOPMVideoOutput **arr = NULL;
    HRESULT hr = f(d.dev, (OPM_VIDEO_OUTPUT_SEMANTICS)9, &n, &arr);
    CHECK(FAILED(hr));
    t_note("invalid semantics on D3D9 device -> %s", d3dh_hr_name(hr));
    DXM_D9_END(d);
}
TEST(dxva2_monitor_video_opm_d3d9_refcount_balanced) {
    HRESULT (WINAPI *f)(IDirect3DDevice9 *, OPM_VIDEO_OUTPUT_SEMANTICS, ULONG *, IOPMVideoOutput ***) = U(OPMGetVideoOutputsFromIDirect3DDevice9Object); REQUIRE(f);
    DXM_D9(d);
    ULONG before = d3dh_refcount(d.dev);
    ULONG n = 0; IOPMVideoOutput **arr = NULL;
    HRESULT hr = f(d.dev, OPM_VOS_OPM_SEMANTICS, &n, &arr);
    if (SUCCEEDED(hr)) { for (ULONG i = 0; i < n; i++) if (arr && arr[i]) IOPMVideoOutput_Release(arr[i]); if (arr) CoTaskMemFree(arr); }
    EXPECT_EQ(d3dh_refcount(d.dev), before);     /* the call never keeps a reference to the device */
    DXM_D9_END(d);
}
