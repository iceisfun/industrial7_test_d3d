/* Argument validation: NULL / invalid pointers passed to IDirectDraw7 and IDirectDrawSurface7 methods. Each case runs in its own process
 * (ddraw raises access violations for some NULL pointers, which would leave its global lock held); the child's exit code is the HRESULT
 * (0xC0000005 = the call faulted). */
#include "dl.h"

#define DDOVERZ_SENDTOFRONT_ 0
struct na_ctx { IDirectDraw7 *dd; IDirectDrawSurface7 *s, *s2; HWND w; };
typedef HRESULT (*na_fn)(struct na_ctx *);
static HRESULT WINAPI na_cb(LPDDSURFACEDESC2 d, LPVOID c) { return DDENUMRET_OK; }
static HRESULT WINAPI na_scb(LPDIRECTDRAWSURFACE7 s, LPDDSURFACEDESC2 d, LPVOID c) { IDirectDrawSurface7_Release(s); return DDENUMRET_OK; }
#define NA(name, expr) static HRESULT na_##name(struct na_ctx *c) { return (expr); }
/* IDirectDraw7 */
NA(dd_createclipper_null, IDirectDraw7_CreateClipper(c->dd, 0, NULL, NULL))
NA(dd_createpalette_null, IDirectDraw7_CreatePalette(c->dd, DDPCAPS_8BIT, NULL, NULL, NULL))
NA(dd_createsurface_null_desc, IDirectDraw7_CreateSurface(c->dd, NULL, &c->s2, NULL))
NA(dd_createsurface_null_out, ({ DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.dwWidth = d.dwHeight = 4; d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY; IDirectDraw7_CreateSurface(c->dd, &d, NULL, NULL); }))
NA(dd_duplicate_null_src, IDirectDraw7_DuplicateSurface(c->dd, NULL, &c->s2))
NA(dd_duplicate_null_out, IDirectDraw7_DuplicateSurface(c->dd, c->s, NULL))
NA(dd_enummodes_null_cb, IDirectDraw7_EnumDisplayModes(c->dd, 0, NULL, NULL, NULL))
NA(dd_enumsurfaces_null_cb, IDirectDraw7_EnumSurfaces(c->dd, DDENUMSURFACES_ALL | DDENUMSURFACES_DOESEXIST, NULL, NULL, NULL))
NA(dd_enumsurfaces_bad_flags, IDirectDraw7_EnumSurfaces(c->dd, 0, NULL, NULL, na_scb))
NA(dd_getcaps_both_null, IDirectDraw7_GetCaps(c->dd, NULL, NULL))
NA(dd_getfourcc_null_count, IDirectDraw7_GetFourCCCodes(c->dd, NULL, NULL))
NA(dd_getgdi_null, IDirectDraw7_GetGDISurface(c->dd, NULL))
NA(dd_getmonfreq_null, IDirectDraw7_GetMonitorFrequency(c->dd, NULL))
NA(dd_getscanline_null, IDirectDraw7_GetScanLine(c->dd, NULL))
NA(dd_getvblank_null, IDirectDraw7_GetVerticalBlankStatus(c->dd, NULL))
NA(dd_getdevid_null, IDirectDraw7_GetDeviceIdentifier(c->dd, NULL, 0))
NA(dd_getdevid_bad_flags, ({ DDDEVICEIDENTIFIER2 id; IDirectDraw7_GetDeviceIdentifier(c->dd, &id, 0x10); }))
NA(dd_getavailvidmem_null_caps, ({ DWORD t, f; IDirectDraw7_GetAvailableVidMem(c->dd, NULL, &t, &f); }))
NA(dd_getsurfacefromdc_null, ({ LPDIRECTDRAWSURFACE7 s = NULL; IDirectDraw7_GetSurfaceFromDC(c->dd, NULL, &s); }))
NA(dd_setcooplevel_null_hwnd_normal, IDirectDraw7_SetCooperativeLevel(c->dd, NULL, DDSCL_NORMAL))
NA(dd_setcooplevel_null_hwnd_exclusive, IDirectDraw7_SetCooperativeLevel(c->dd, NULL, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN))
NA(dd_setcooplevel_bad_flags, IDirectDraw7_SetCooperativeLevel(c->dd, c->w, 0xffff0000))
NA(dd_setdisplaymode_zero, IDirectDraw7_SetDisplayMode(c->dd, 0, 0, 0, 0, 0))
NA(dd_setdisplaymode_without_exclusive, IDirectDraw7_SetDisplayMode(c->dd, 640, 480, 32, 0, 0))
NA(dd_waitvblank_bad_flags, IDirectDraw7_WaitForVerticalBlank(c->dd, 0, NULL))
NA(dd_waitvblank_flags_ffff, IDirectDraw7_WaitForVerticalBlank(c->dd, 0xffff, NULL))
NA(dd_startmodetest_null, IDirectDraw7_StartModeTest(c->dd, NULL, 0, 0))
NA(dd_evaluatemode_without_test, IDirectDraw7_EvaluateMode(c->dd, 0, NULL))
NA(dd_initialize_already, IDirectDraw7_Initialize(c->dd, NULL))
/* IDirectDrawSurface7 */
NA(s_blt_null_all, IDirectDrawSurface7_Blt(c->s, NULL, NULL, NULL, 0, NULL))
NA(s_blt_colorfill_null_fx, IDirectDrawSurface7_Blt(c->s, NULL, NULL, NULL, DDBLT_COLORFILL, NULL))
NA(s_blt_rop_null_fx, IDirectDrawSurface7_Blt(c->s, NULL, NULL, NULL, DDBLT_ROP, NULL))
NA(s_blt_src_self_no_flags, IDirectDrawSurface7_Blt(c->s, NULL, c->s2, NULL, 0, NULL))
NA(s_bltfast_null_src, IDirectDrawSurface7_BltFast(c->s, 0, 0, NULL, NULL, 0))
NA(s_bltbatch_null, IDirectDrawSurface7_BltBatch(c->s, NULL, 0, 0))
NA(s_addattached_null, IDirectDrawSurface7_AddAttachedSurface(c->s, NULL))
NA(s_deleteattached_null, IDirectDrawSurface7_DeleteAttachedSurface(c->s, 0, NULL))
NA(s_enumattached_null_cb, IDirectDrawSurface7_EnumAttachedSurfaces(c->s, NULL, NULL))
NA(s_enumoverlayz_null_cb, IDirectDrawSurface7_EnumOverlayZOrders(c->s, 0, NULL, NULL))
NA(s_flip_plain, IDirectDrawSurface7_Flip(c->s, NULL, 0))
NA(s_getattached_null_caps, ({ LPDIRECTDRAWSURFACE7 a = NULL; IDirectDrawSurface7_GetAttachedSurface(c->s, NULL, &a); }))
NA(s_getattached_null_out, ({ DDSCAPS2 cp; memset(&cp, 0, sizeof cp); IDirectDrawSurface7_GetAttachedSurface(c->s, &cp, NULL); }))
NA(s_getbltstatus_flags0, IDirectDrawSurface7_GetBltStatus(c->s, 0))
NA(s_getcaps_null, IDirectDrawSurface7_GetCaps(c->s, NULL))
NA(s_getclipper_null, IDirectDrawSurface7_GetClipper(c->s, NULL))
NA(s_getclipper_none, ({ LPDIRECTDRAWCLIPPER k = NULL; IDirectDrawSurface7_GetClipper(c->s, &k); }))
NA(s_getcolorkey_null, IDirectDrawSurface7_GetColorKey(c->s, DDCKEY_SRCBLT, NULL))
NA(s_getcolorkey_unset, ({ DDCOLORKEY k; IDirectDrawSurface7_GetColorKey(c->s, DDCKEY_SRCBLT, &k); }))
NA(s_getdc_null, IDirectDrawSurface7_GetDC(c->s, NULL))
NA(s_getflipstatus_flags0, IDirectDrawSurface7_GetFlipStatus(c->s, 0))
NA(s_getoverlaypos_null, IDirectDrawSurface7_GetOverlayPosition(c->s, NULL, NULL))
NA(s_getpalette_null, IDirectDrawSurface7_GetPalette(c->s, NULL))
NA(s_getpalette_none, ({ LPDIRECTDRAWPALETTE p = NULL; IDirectDrawSurface7_GetPalette(c->s, &p); }))
NA(s_getpixelformat_null, IDirectDrawSurface7_GetPixelFormat(c->s, NULL))
NA(s_getsurfacedesc_null, IDirectDrawSurface7_GetSurfaceDesc(c->s, NULL))
NA(s_initialize_null, IDirectDrawSurface7_Initialize(c->s, NULL, NULL))
NA(s_islost, IDirectDrawSurface7_IsLost(c->s))
NA(s_lock_null_desc, IDirectDrawSurface7_Lock(c->s, NULL, NULL, DDLOCK_WAIT, NULL))
NA(s_lock_bad_rect, ({ DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; RECT r = { 0, 0, 100, 100 }; IDirectDrawSurface7_Lock(c->s, &r, &d, DDLOCK_WAIT, NULL); }))
NA(s_lock_inverted_rect, ({ DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; RECT r = { 3, 3, 1, 1 }; IDirectDrawSurface7_Lock(c->s, &r, &d, DDLOCK_WAIT, NULL); }))
NA(s_releasedc_null, IDirectDrawSurface7_ReleaseDC(c->s, NULL))
NA(s_setclipper_null, IDirectDrawSurface7_SetClipper(c->s, NULL))
NA(s_setcolorkey_null, IDirectDrawSurface7_SetColorKey(c->s, DDCKEY_SRCBLT, NULL))
NA(s_setcolorkey_bad_flags, ({ DDCOLORKEY k = { 1, 1 }; IDirectDrawSurface7_SetColorKey(c->s, 0, &k); }))
NA(s_setoverlaypos_plain, IDirectDrawSurface7_SetOverlayPosition(c->s, 0, 0))
NA(s_setpalette_null, IDirectDrawSurface7_SetPalette(c->s, NULL))
NA(s_unlock_not_locked, IDirectDrawSurface7_Unlock(c->s, NULL))
NA(s_updateoverlay_plain, IDirectDrawSurface7_UpdateOverlay(c->s, NULL, c->s2, NULL, DDOVER_SHOW, NULL))
NA(s_updateoverlaydisplay, IDirectDrawSurface7_UpdateOverlayDisplay(c->s, 0))
NA(s_updateoverlayzorder_plain, IDirectDrawSurface7_UpdateOverlayZOrder(c->s, DDOVERZ_SENDTOFRONT_, NULL))
NA(s_pagelock_unlock, IDirectDrawSurface7_PageLock(c->s, 0))
NA(s_pageunlock_not_locked, IDirectDrawSurface7_PageUnlock(c->s, 0))
NA(s_setprivatedata_null, IDirectDrawSurface7_SetPrivateData(c->s, &IID_IUnknown, NULL, 0, 0))
NA(s_getprivatedata_none, ({ DWORD sz = 0; IDirectDrawSurface7_GetPrivateData(c->s, &IID_IUnknown, NULL, &sz); }))
NA(s_freeprivatedata_none, IDirectDrawSurface7_FreePrivateData(c->s, &IID_IUnknown))
NA(s_getuniquenessvalue_null, IDirectDrawSurface7_GetUniquenessValue(c->s, NULL))
NA(s_changeuniqueness, IDirectDrawSurface7_ChangeUniquenessValue(c->s))

static const struct na_case { const char *name; na_fn fn; HRESULT want; } k_na[] = {   /* 0xC0000005 = access violation inside ddraw; measured on Win10 22H2 x64 */
    { "dd_createclipper_null", na_dd_createclipper_null, (HRESULT)0x80070057 },
    { "dd_createpalette_null", na_dd_createpalette_null, (HRESULT)0x80070057 },
    { "dd_createsurface_null_desc", na_dd_createsurface_null_desc, (HRESULT)0x80070057 },
    { "dd_createsurface_null_out", na_dd_createsurface_null_out, (HRESULT)0x80070057 },
    { "dd_duplicate_null_src", na_dd_duplicate_null_src, (HRESULT)0x80070057 },
    { "dd_duplicate_null_out", na_dd_duplicate_null_out, (HRESULT)0x80070057 },
    { "dd_enummodes_null_cb", na_dd_enummodes_null_cb, (HRESULT)0x80070057 },
    { "dd_enumsurfaces_null_cb", na_dd_enumsurfaces_null_cb, (HRESULT)0x80070057 },
    { "dd_enumsurfaces_bad_flags", na_dd_enumsurfaces_bad_flags, (HRESULT)0x80070057 },
    { "dd_getcaps_both_null", na_dd_getcaps_both_null, (HRESULT)0x80070057 },
    { "dd_getfourcc_null_count", na_dd_getfourcc_null_count, (HRESULT)0x80070057 },
    { "dd_getgdi_null", na_dd_getgdi_null, (HRESULT)0x80070057 },
    { "dd_getmonfreq_null", na_dd_getmonfreq_null, (HRESULT)0x80070057 },
    { "dd_getscanline_null", na_dd_getscanline_null, (HRESULT)0xc0000005 },
    { "dd_getvblank_null", na_dd_getvblank_null, (HRESULT)0xc0000005 },
    { "dd_getdevid_null", na_dd_getdevid_null, (HRESULT)0x80070057 },
    { "dd_getdevid_bad_flags", na_dd_getdevid_bad_flags, (HRESULT)0x80070057 },
    { "dd_getavailvidmem_null_caps", na_dd_getavailvidmem_null_caps, (HRESULT)0x80070057 },
    { "dd_getsurfacefromdc_null", na_dd_getsurfacefromdc_null, (HRESULT)0x887600ff },
    { "dd_setcooplevel_null_hwnd_normal", na_dd_setcooplevel_null_hwnd_normal, (HRESULT)0x00000000 },
    { "dd_setcooplevel_null_hwnd_exclusive", na_dd_setcooplevel_null_hwnd_exclusive, (HRESULT)0x80070057 },
    { "dd_setcooplevel_bad_flags", na_dd_setcooplevel_bad_flags, (HRESULT)0x80070057 },
    { "dd_setdisplaymode_zero", na_dd_setdisplaymode_zero, (HRESULT)0x80004001 },
    { "dd_setdisplaymode_without_exclusive", na_dd_setdisplaymode_without_exclusive, (HRESULT)0x00000000 },
    { "dd_waitvblank_bad_flags", na_dd_waitvblank_bad_flags, (HRESULT)0x80070057 },
    { "dd_waitvblank_flags_ffff", na_dd_waitvblank_flags_ffff, (HRESULT)0x80004001 },
    { "dd_startmodetest_null", na_dd_startmodetest_null, (HRESULT)0x887602b4 },
    { "dd_evaluatemode_without_test", na_dd_evaluatemode_without_test, (HRESULT)0x80070057 },
    { "dd_initialize_already", na_dd_initialize_already, (HRESULT)0x88760005 },
    { "s_blt_null_all", na_s_blt_null_all, (HRESULT)0x80070057 },
    { "s_blt_colorfill_null_fx", na_s_blt_colorfill_null_fx, (HRESULT)0x80070057 },
    { "s_blt_rop_null_fx", na_s_blt_rop_null_fx, (HRESULT)0x80070057 },
    { "s_blt_src_self_no_flags", na_s_blt_src_self_no_flags, (HRESULT)0x00000000 },
    { "s_bltfast_null_src", na_s_bltfast_null_src, (HRESULT)0xc0000005 },
    { "s_bltbatch_null", na_s_bltbatch_null, (HRESULT)0x80004001 },
    { "s_addattached_null", na_s_addattached_null, (HRESULT)0x80070057 },
    { "s_deleteattached_null", na_s_deleteattached_null, (HRESULT)0x00000000 },
    { "s_enumattached_null_cb", na_s_enumattached_null_cb, (HRESULT)0x80070057 },
    { "s_enumoverlayz_null_cb", na_s_enumoverlayz_null_cb, (HRESULT)0x80070057 },
    { "s_flip_plain", na_s_flip_plain, (HRESULT)0x88760246 },
    { "s_getattached_null_caps", na_s_getattached_null_caps, (HRESULT)0x80070057 },
    { "s_getattached_null_out", na_s_getattached_null_out, (HRESULT)0x80070057 },
    { "s_getbltstatus_flags0", na_s_getbltstatus_flags0, (HRESULT)0x80070057 },
    { "s_getcaps_null", na_s_getcaps_null, (HRESULT)0x80070057 },
    { "s_getclipper_null", na_s_getclipper_null, (HRESULT)0x80070057 },
    { "s_getclipper_none", na_s_getclipper_none, (HRESULT)0x88760238 },
    { "s_getcolorkey_null", na_s_getcolorkey_null, (HRESULT)0x887600d7 },
    { "s_getcolorkey_unset", na_s_getcolorkey_unset, (HRESULT)0x887600d7 },
    { "s_getdc_null", na_s_getdc_null, (HRESULT)0x80070057 },
    { "s_getflipstatus_flags0", na_s_getflipstatus_flags0, (HRESULT)0x80070057 },
    { "s_getoverlaypos_null", na_s_getoverlaypos_null, (HRESULT)0x80070057 },
    { "s_getpalette_null", na_s_getpalette_null, (HRESULT)0x80070057 },
    { "s_getpalette_none", na_s_getpalette_none, (HRESULT)0x8876023c },
    { "s_getpixelformat_null", na_s_getpixelformat_null, (HRESULT)0x80070057 },
    { "s_getsurfacedesc_null", na_s_getsurfacedesc_null, (HRESULT)0x80070057 },
    { "s_initialize_null", na_s_initialize_null, (HRESULT)0x88760005 },
    { "s_islost", na_s_islost, (HRESULT)0x00000000 },
    { "s_lock_null_desc", na_s_lock_null_desc, (HRESULT)0x80070057 },
    { "s_lock_bad_rect", na_s_lock_bad_rect, (HRESULT)0x80070057 },
    { "s_lock_inverted_rect", na_s_lock_inverted_rect, (HRESULT)0x80070057 },
    { "s_releasedc_null", na_s_releasedc_null, (HRESULT)0x8876024a },
    { "s_setclipper_null", na_s_setclipper_null, (HRESULT)0x88760238 },
    { "s_setcolorkey_null", na_s_setcolorkey_null, (HRESULT)0x00000000 },
    { "s_setcolorkey_bad_flags", na_s_setcolorkey_bad_flags, (HRESULT)0x80070057 },
    { "s_setoverlaypos_plain", na_s_setoverlaypos_plain, (HRESULT)0x88760244 },
    { "s_setpalette_null", na_s_setpalette_null, (HRESULT)0x8876023c },
    { "s_unlock_not_locked", na_s_unlock_not_locked, (HRESULT)0x88760248 },
    { "s_updateoverlay_plain", na_s_updateoverlay_plain, (HRESULT)0x88760244 },
    { "s_updateoverlaydisplay", na_s_updateoverlaydisplay, (HRESULT)0x80004001 },
    { "s_updateoverlayzorder_plain", na_s_updateoverlayzorder_plain, (HRESULT)0x88760244 },
    { "s_pagelock_unlock", na_s_pagelock_unlock, (HRESULT)0x00000000 },
    { "s_pageunlock_not_locked", na_s_pageunlock_not_locked, (HRESULT)0x00000000 },
    { "s_setprivatedata_null", na_s_setprivatedata_null, (HRESULT)0x80070057 },
    { "s_getprivatedata_none", na_s_getprivatedata_none, (HRESULT)0x887600ff },
    { "s_freeprivatedata_none", na_s_freeprivatedata_none, (HRESULT)0x887600ff },
    { "s_getuniquenessvalue_null", na_s_getuniquenessvalue_null, (HRESULT)0x80070057 },
    { "s_changeuniqueness", na_s_changeuniqueness, (HRESULT)0x00000000 },
};
#define NNA ((int)(sizeof k_na / sizeof k_na[0]))
CHILD(ddl_nullarg) {
    int idx = argc > 1 ? atoi(argv[1]) : -1;
    if (idx < 0 || idx >= NNA) return 0xC0DE0002;
    struct na_ctx c; memset(&c, 0, sizeof c);
    c.dd = dl_create7(); if (!c.dd) return 0xC0DE0003;
    c.w = dl_window();
    if (IDirectDraw7_SetCooperativeLevel(c.dd, c.w, DDSCL_NORMAL) != DD_OK) return 0xC0DE0004;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.dwWidth = d.dwHeight = 4; d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    if (IDirectDraw7_CreateSurface(c.dd, &d, &c.s, NULL) != DD_OK) return 0xC0DE0005;
    if (IDirectDraw7_CreateSurface(c.dd, &d, &c.s2, NULL) != DD_OK) return 0xC0DE0006;
    return (int)k_na[idx].fn(&c);
}
TEST(ddraw_legacy_nullargs_matrix) {
    DL_SKIP_IF_NO_DD7(dd0); IDirectDraw7_Release(dd0);
    for (int i = 0; i < NNA; i++) {
        char a[16]; snprintf(a, sizeof a, "%d", i);
        DWORD rc = t_run_child("ddl_nullarg", a, 60000);
        CHECKF(rc == (DWORD)k_na[i].want, "%s -> %08lx want %08lx", k_na[i].name, (unsigned long)rc, (unsigned long)k_na[i].want);
    }
}
