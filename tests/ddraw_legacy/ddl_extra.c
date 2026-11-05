/* Additional argument-protocol tests for IDirectDraw objects. */
#include "dl.h"

TEST(ddraw_legacy_extra_wait_vblank_event_flag_unsupported) {
    DL_SKIP_IF_NO_DD7(dd);
    /* bit 1 (the event variants) is rejected before the flag value is inspected */
    EXPECT_HR(IDirectDraw7_WaitForVerticalBlank(dd, DDWAITVB_BLOCKBEGINEVENT, NULL), DDERR_UNSUPPORTED);
    EXPECT_HR(IDirectDraw7_WaitForVerticalBlank(dd, 3, NULL), DDERR_UNSUPPORTED);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_extra_wait_vblank_all_versions) {
    DL_SKIP_IF_NO_DD1(dd1);
    EXPECT_HR(IDirectDraw_WaitForVerticalBlank(dd1, 8, NULL), DDERR_INVALIDPARAMS);
    EXPECT_HR(IDirectDraw_WaitForVerticalBlank(dd1, 0, NULL), DDERR_INVALIDPARAMS);
    IDirectDraw_Release(dd1);
}
TEST(ddraw_legacy_extra_fourcc_count_matches_buffered_copy) {
    DL_SKIP_IF_NO_DD7(dd);
    DWORD n = 0;
    EXPECT_HR(IDirectDraw7_GetFourCCCodes(dd, &n, NULL), DD_OK);
    if (n > 0 && n < 256) {
        DWORD codes[256], m = n, k = 0;
        memset(codes, 0, sizeof codes);
        EXPECT_HR(IDirectDraw7_GetFourCCCodes(dd, &m, codes), DD_OK);
        EXPECT_EQ(m, n);
        k = n - 1;                       /* truncation: count is clamped to the buffer size */
        EXPECT_HR(IDirectDraw7_GetFourCCCodes(dd, &k, codes), DD_OK);
        EXPECT_EQ(k, n - 1);
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_extra_getcaps_size_zero_and_odd) {
    DL_SKIP_IF_NO_DD7(dd);
    DDCAPS c; memset(&c, 0, sizeof c);
    DWORD bad[] = { 0, 1, 171, 173, 315, 317, 379, 381, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        c.dwSize = bad[i];
        HRESULT hr = IDirectDraw7_GetCaps(dd, &c, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
    }
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_extra_release_after_qi_chain_balanced) {
    DL_SKIP_IF_NO_DD1(dd1);
    IDirectDraw7 *a = NULL, *b = NULL;
    EXPECT_HR(IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw7, (void **)&a), S_OK);
    if (a) EXPECT_HR(IDirectDraw7_QueryInterface(a, &IID_IDirectDraw7, (void **)&b), S_OK);
    if (b) { EXPECT_RET(IDirectDraw7_Release(b), 1); }
    if (a) { EXPECT_RET(IDirectDraw7_Release(a), 0); }
    EXPECT_RET(IDirectDraw_Release(dd1), 0);
}
