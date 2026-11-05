/* IDirectDrawSurface7 (and older) creation, locking, blitting, colour keys, DC access. */
#include "dl.h"

/* A DirectDraw7 object with a normal-level cooperative window; NULL (after t_skip) when unavailable. */
static IDirectDraw7 *setup(HWND *w) {
    IDirectDraw7 *dd = dl_create7();
    if (!dd) return NULL;
    *w = dl_window();
    HRESULT hr = IDirectDraw7_SetCooperativeLevel(dd, *w, DDSCL_NORMAL);
    if (FAILED(hr)) { t_skip("SetCooperativeLevel(NORMAL) -> %s", d3dh_hr_name(hr)); IDirectDraw7_Release(dd); dl_window_free(*w); return NULL; }
    return dd;
}
static void teardown(IDirectDraw7 *dd, HWND w) { IDirectDraw7_Release(dd); dl_window_free(w); }

static IDirectDrawSurface7 *mk(IDirectDraw7 *dd, DWORD w, DWORD h, const DDPIXELFORMAT *pf, DWORD caps, HRESULT *hr_out) {
    DDSURFACEDESC2 d; IDirectDrawSurface7 *s = NULL;
    memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    d.dwWidth = w; d.dwHeight = h; d.ddsCaps.dwCaps = caps;
    if (pf) { d.dwFlags |= DDSD_PIXELFORMAT; d.ddpfPixelFormat = *pf; }
    HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &s, NULL);
    if (hr_out) *hr_out = hr;
    return SUCCEEDED(hr) ? s : NULL;
}
#define SYSPLAIN (DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY)

static HRESULT lock_all(IDirectDrawSurface7 *s, DDSURFACEDESC2 *d, DWORD fl) {
    memset(d, 0, sizeof *d); d->dwSize = sizeof *d;
    return IDirectDrawSurface7_Lock(s, NULL, d, fl | DDLOCK_WAIT, NULL);
}
static DWORD px32(IDirectDrawSurface7 *s, int x, int y) {
    DDSURFACEDESC2 d; DWORD v = 0xdeadbeef;
    if (SUCCEEDED(lock_all(s, &d, DDLOCK_READONLY))) { v = *(DWORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 4); IDirectDrawSurface7_Unlock(s, NULL); }
    return v;
}
static void fill32(IDirectDrawSurface7 *s, DWORD v) {
    DDSURFACEDESC2 d;
    if (FAILED(lock_all(s, &d, 0))) return;
    for (DWORD y = 0; y < d.dwHeight; y++) for (DWORD x = 0; x < d.dwWidth; x++) *(DWORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 4) = v;
    IDirectDrawSurface7_Unlock(s, NULL);
}
static DWORD cfill(IDirectDrawSurface7 *s, RECT *r, DWORD color, HRESULT *hr) {
    DDBLTFX fx; memset(&fx, 0, sizeof fx); fx.dwSize = sizeof fx; fx.dwFillColor = color;
    HRESULT h = IDirectDrawSurface7_Blt(s, r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    if (hr) *hr = h;
    return h;
}

/* ---------------- CreateSurface argument validation ---------------- */
TEST(ddraw_legacy_surf_create_requires_cooperative_level) {
    DL_SKIP_IF_NO_DD7(dd);
    HRESULT hr; IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, &hr);
    EXPECT_HR(hr, DDERR_NOCOOPERATIVELEVELSET);
    if (s) IDirectDrawSurface7_Release(s);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_surf_create_aggregation) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.dwWidth = d.dwHeight = 8; d.ddsCaps.dwCaps = SYSPLAIN;
    IDirectDrawSurface7 *s = NULL;
    EXPECT_HR(IDirectDraw7_CreateSurface(dd, &d, &s, (IUnknown *)(ULONG_PTR)0x10), CLASS_E_NOAGGREGATION);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_create_bad_desc_size_dd7) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DWORD bad[] = { 0, 8, 108, 120, 124, 132, 140 };
    for (unsigned i = 0; i < 7; i++) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = bad[i];
        d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.dwWidth = d.dwHeight = 8; d.ddsCaps.dwCaps = SYSPLAIN;
        IDirectDrawSurface7 *s = (IDirectDrawSurface7 *)(ULONG_PTR)1;
        HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &s, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_create_bad_desc_size_dd4) {
    HWND w; IDirectDraw7 *dd7 = setup(&w); if (!dd7) return;
    IDirectDraw4 *dd = NULL;
    if (FAILED(IDirectDraw7_QueryInterface(dd7, &IID_IDirectDraw4, (void **)&dd)) || !dd) { teardown(dd7, w); return; }
    DWORD bad[] = { 0, 108, 120, 124 };
    for (unsigned i = 0; i < 4; i++) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = bad[i];
        d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.dwWidth = d.dwHeight = 8; d.ddsCaps.dwCaps = SYSPLAIN;
        IDirectDrawSurface4 *s = NULL;
        HRESULT hr = IDirectDraw4_CreateSurface(dd, &d, &s, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
        if (s) IDirectDrawSurface4_Release(s);
    }
    IDirectDraw4_Release(dd);
    teardown(dd7, w);
}
TEST(ddraw_legacy_surf_create_invalid_caps) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    struct { DWORD caps; const char *n; } c[] = {
        { DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY | DDSCAPS_SYSTEMMEMORY, "VIDEOMEMORY|SYSTEMMEMORY" },
        { DDSCAPS_OFFSCREENPLAIN | DDSCAPS_LOCALVIDMEM, "LOCALVIDMEM without VIDEOMEMORY" },
        { DDSCAPS_OFFSCREENPLAIN | DDSCAPS_NONLOCALVIDMEM, "NONLOCALVIDMEM without VIDEOMEMORY" },
        { DDSCAPS_OFFSCREENPLAIN | DDSCAPS_LOCALVIDMEM | DDSCAPS_NONLOCALVIDMEM, "LOCAL|NONLOCAL" },
    };
    for (unsigned i = 0; i < 4; i++) {
        HRESULT hr; IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, c[i].caps, &hr);
        CHECKF(hr == DDERR_INVALIDCAPS, "%s -> %s", c[i].n, d3dh_hr_name(hr));
        if (s) IDirectDrawSurface7_Release(s);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_create_owndc_needs_sysmem) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    HRESULT hr; IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, DDSCAPS_OFFSCREENPLAIN | DDSCAPS_OWNDC, &hr);
    EXPECT_HR(hr, DDERR_UNSUPPORTED);
    if (s) IDirectDrawSurface7_Release(s);
    s = mk(dd, 8, 8, NULL, SYSPLAIN | DDSCAPS_OWNDC, &hr);
    t_note("system-memory OWNDC surface -> %s", d3dh_hr_name(hr));
    if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_create_zero_size) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    HRESULT hr; IDirectDrawSurface7 *s;
    s = mk(dd, 0, 8, NULL, SYSPLAIN, &hr);
    CHECKF(FAILED(hr), "width 0 -> %s", d3dh_hr_name(hr)); if (s) IDirectDrawSurface7_Release(s);
    s = mk(dd, 8, 0, NULL, SYSPLAIN, &hr);
    CHECKF(FAILED(hr), "height 0 -> %s", d3dh_hr_name(hr)); if (s) IDirectDrawSurface7_Release(s);
    s = mk(dd, 0, 0, NULL, SYSPLAIN, &hr);
    CHECKF(FAILED(hr), "0x0 -> %s", d3dh_hr_name(hr)); if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}

/* ---------------- client-memory surfaces (DDSD_LPSURFACE) ---------------- */
static HRESULT mk_user(IDirectDraw7 *dd, DWORD flags_extra, DWORD flags_drop, void *mem, LONG pitch, DWORD caps, IDirectDrawSurface7 **out) {
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = (DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_LPSURFACE | flags_extra) & ~flags_drop;
    d.dwWidth = 8; d.dwHeight = 4; d.lPitch = pitch; d.lpSurface = mem; d.ddsCaps.dwCaps = caps;
    dl_pf_xrgb32(&d.ddpfPixelFormat);
    *out = NULL;
    return IDirectDraw7_CreateSurface(dd, &d, out, NULL);
}
TEST(ddraw_legacy_surf_usermem_validation) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    static DWORD mem[8 * 4 + 8];
    IDirectDrawSurface7 *s;
    EXPECT_HR(mk_user(dd, 0, DDSD_PITCH, mem, 32, SYSPLAIN, &s), DDERR_INVALIDPARAMS);            if (s) IDirectDrawSurface7_Release(s);
    EXPECT_HR(mk_user(dd, 0, 0, NULL, 32, SYSPLAIN, &s), DDERR_INVALIDPARAMS);                     if (s) IDirectDrawSurface7_Release(s);
    EXPECT_HR(mk_user(dd, 0, DDSD_WIDTH, mem, 32, SYSPLAIN, &s), DDERR_INVALIDPARAMS);             if (s) IDirectDrawSurface7_Release(s);
    EXPECT_HR(mk_user(dd, 0, DDSD_HEIGHT, mem, 32, SYSPLAIN, &s), DDERR_INVALIDPARAMS);            if (s) IDirectDrawSurface7_Release(s);
    EXPECT_HR(mk_user(dd, 0, 0, mem, 28, SYSPLAIN, &s), DDERR_INVALIDPARAMS);                      if (s) IDirectDrawSurface7_Release(s);   /* pitch < width*4 */
    EXPECT_HR(mk_user(dd, 0, 0, mem, 34, SYSPLAIN, &s), DDERR_INVALIDPARAMS);                      if (s) IDirectDrawSurface7_Release(s);   /* pitch not DWORD aligned */
    EXPECT_HR(mk_user(dd, 0, 0, mem, 32, DDSCAPS_OFFSCREENPLAIN, &s), DDERR_INVALIDCAPS);          if (s) IDirectDrawSurface7_Release(s);   /* needs SYSTEMMEMORY */
    EXPECT_HR(mk_user(dd, 0, 0, mem, 32, DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY, &s), DDERR_INVALIDCAPS); if (s) IDirectDrawSurface7_Release(s);
    EXPECT_HR(mk_user(dd, 0, 0, mem, 32, SYSPLAIN | DDSCAPS_PRIMARYSURFACE, &s), DDERR_INVALIDCAPS);          if (s) IDirectDrawSurface7_Release(s);
    EXPECT_HR(mk_user(dd, 0, 0, mem, 32, SYSPLAIN | DDSCAPS_OVERLAY, &s), DDERR_INVALIDCAPS);                  if (s) IDirectDrawSurface7_Release(s);
    EXPECT_HR(mk_user(dd, 0, 0, mem, 32, SYSPLAIN | DDSCAPS_COMPLEX, &s), DDERR_INVALIDCAPS);                  if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_usermem_aliasing) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DWORD mem[8 * 4 + 8];
    for (int i = 0; i < 8 * 4 + 8; i++) mem[i] = 0x11111111;
    IDirectDrawSurface7 *s;
    HRESULT hr = mk_user(dd, 0, 0, mem, 32, SYSPLAIN, &s);
    if (FAILED(hr)) { t_skip("client memory surface -> %s", d3dh_hr_name(hr)); teardown(dd, w); return; }
    DDSURFACEDESC2 d;
    EXPECT_HR(lock_all(s, &d, 0), DD_OK);
    CHECK(d.lpSurface == (void *)mem);                 /* the surface is backed by the caller's memory */
    EXPECT_EQ(d.lPitch, 32);
    IDirectDrawSurface7_Unlock(s, NULL);
    EXPECT_HR(cfill(s, NULL, 0x00abcdef, NULL), DD_OK);
    for (int i = 0; i < 8 * 4; i++) { if (mem[i] != 0x00abcdef) { CHECKF(0, "mem[%d]=%08lx", i, (unsigned long)mem[i]); break; } }
    CHECK(mem[8 * 4] == 0x11111111);                    /* nothing written beyond the last row */
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}

/* ---------------- default offscreen surface ---------------- */
TEST(ddraw_legacy_surf_offscreen_desc) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    HRESULT hr; IDirectDrawSurface7 *s = mk(dd, 64, 32, NULL, SYSPLAIN, &hr);
    EXPECT_HR(hr, DD_OK);
    if (s) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
        EXPECT_HR(IDirectDrawSurface7_GetSurfaceDesc(s, &d), DD_OK);
        EXPECT_EQ(d.dwSize, sizeof d);
        CHECK((d.dwFlags & (DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)) == (DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT));
        EXPECT_EQ(d.dwWidth, 64);
        EXPECT_EQ(d.dwHeight, 32);
        CHECK((d.ddsCaps.dwCaps & SYSPLAIN) == SYSPLAIN);
        EXPECT_EQ(d.ddpfPixelFormat.dwSize, sizeof(DDPIXELFORMAT));
        DDSURFACEDESC2 cur; memset(&cur, 0, sizeof cur); cur.dwSize = sizeof cur;
        if (SUCCEEDED(IDirectDraw7_GetDisplayMode(dd, &cur)))       /* default format is the display format */
            EXPECT_EQ(d.ddpfPixelFormat.dwRGBBitCount, cur.ddpfPixelFormat.dwRGBBitCount);
        EXPECT_RET(IDirectDrawSurface7_Release(s), 0);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_getsurfacedesc_bad_size) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 16, 16, NULL, SYSPLAIN, NULL);
    if (s) {
        DDSURFACEDESC2 d;
        DWORD bad[] = { 0, 108, 120, 124, 140 };
        for (unsigned i = 0; i < 5; i++) {
            memset(&d, 0, sizeof d); d.dwSize = bad[i];
            HRESULT hr = IDirectDrawSurface7_GetSurfaceDesc(s, &d);
            CHECKF(hr == DDERR_INVALIDPARAMS, "dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
        }
        IDirectDrawSurface7_Release(s);
    } else t_skip("no surface");
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_getsurfacedesc_older_interfaces) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s7 = mk(dd, 16, 8, NULL, SYSPLAIN, NULL);
    if (s7) {
        IDirectDrawSurface *s1 = NULL; IDirectDrawSurface4 *s4 = NULL;
        EXPECT_HR(IDirectDrawSurface7_QueryInterface(s7, &IID_IDirectDrawSurface, (void **)&s1), DD_OK);
        EXPECT_HR(IDirectDrawSurface7_QueryInterface(s7, &IID_IDirectDrawSurface4, (void **)&s4), DD_OK);
        if (s1) {
            DDSURFACEDESC d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
            EXPECT_HR(IDirectDrawSurface_GetSurfaceDesc(s1, &d), DD_OK);
            EXPECT_EQ(d.dwWidth, 16); EXPECT_EQ(d.dwHeight, 8);
            d.dwSize = 108; EXPECT_HR(IDirectDrawSurface_GetSurfaceDesc(s1, &d), DDERR_INVALIDPARAMS);
            IDirectDrawSurface_Release(s1);
        }
        if (s4) {
            DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
            EXPECT_HR(IDirectDrawSurface4_GetSurfaceDesc(s4, &d), DD_OK);
            EXPECT_EQ(d.dwWidth, 16); EXPECT_EQ(d.dwHeight, 8);
            IDirectDrawSurface4_Release(s4);
        }
        IDirectDrawSurface7_Release(s7);
    } else t_skip("no surface");
    teardown(dd, w);
}

struct fmt_case { const char *name; DDPIXELFORMAT pf; };
static int fmt_core(const char *n) { return !strcmp(n, "X8R8G8B8") || !strcmp(n, "R5G6B5") || !strcmp(n, "P8"); }
static void fmt_table(struct fmt_case *c, int *n) {
    int i = 0;
    memset(c, 0, 16 * sizeof *c);
    c[i].name = "X8R8G8B8"; dl_pf_xrgb32(&c[i].pf); i++;
    c[i].name = "A8R8G8B8"; dl_pf_xrgb32(&c[i].pf); c[i].pf.dwFlags |= DDPF_ALPHAPIXELS; c[i].pf.dwRGBAlphaBitMask = 0xff000000; i++;
    c[i].name = "R8G8B8"; dl_pf_xrgb32(&c[i].pf); c[i].pf.dwRGBBitCount = 24; i++;
    c[i].name = "R5G6B5"; dl_pf_rgb565(&c[i].pf); i++;
    c[i].name = "X1R5G5B5"; dl_pf_rgb565(&c[i].pf); c[i].pf.dwRBitMask = 0x7c00; c[i].pf.dwGBitMask = 0x03e0; c[i].pf.dwBBitMask = 0x001f; i++;
    c[i].name = "A1R5G5B5"; dl_pf_rgb565(&c[i].pf); c[i].pf.dwFlags |= DDPF_ALPHAPIXELS; c[i].pf.dwRBitMask = 0x7c00; c[i].pf.dwGBitMask = 0x03e0; c[i].pf.dwBBitMask = 0x001f; c[i].pf.dwRGBAlphaBitMask = 0x8000; i++;
    c[i].name = "A4R4G4B4"; dl_pf_rgb565(&c[i].pf); c[i].pf.dwFlags |= DDPF_ALPHAPIXELS; c[i].pf.dwRBitMask = 0x0f00; c[i].pf.dwGBitMask = 0x00f0; c[i].pf.dwBBitMask = 0x000f; c[i].pf.dwRGBAlphaBitMask = 0xf000; i++;
    c[i].name = "P8"; dl_pf_pal8(&c[i].pf); i++;
    *n = i;
}
TEST(ddraw_legacy_surf_sysmem_formats_roundtrip) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    struct fmt_case c[16]; int n; fmt_table(c, &n);
    for (int i = 0; i < n; i++) {
        HRESULT hr; IDirectDrawSurface7 *s = mk(dd, 13, 5, &c[i].pf, SYSPLAIN, &hr);
        if (fmt_core(c[i].name)) CHECKF(hr == DD_OK, "%s -> %s", c[i].name, d3dh_hr_name(hr));
        else t_note("%s system-memory surface -> %s", c[i].name, d3dh_hr_name(hr));
        if (!s) continue;
        DDPIXELFORMAT pf; memset(&pf, 0, sizeof pf); pf.dwSize = sizeof pf;
        EXPECT_HR(IDirectDrawSurface7_GetPixelFormat(s, &pf), DD_OK);
        CHECKF(pf.dwRGBBitCount == c[i].pf.dwRGBBitCount, "%s bitcount %lu", c[i].name, pf.dwRGBBitCount);
        CHECKF(pf.dwRBitMask == c[i].pf.dwRBitMask && pf.dwGBitMask == c[i].pf.dwGBitMask && pf.dwBBitMask == c[i].pf.dwBBitMask, "%s masks", c[i].name);
        CHECKF((pf.dwFlags & c[i].pf.dwFlags) == c[i].pf.dwFlags, "%s flags 0x%lx vs 0x%lx", c[i].name, pf.dwFlags, c[i].pf.dwFlags);
        DDSURFACEDESC2 d;
        EXPECT_HR(lock_all(s, &d, 0), DD_OK);
        DWORD bpp = c[i].pf.dwRGBBitCount;
        CHECKF(d.lPitch >= (LONG)(13 * bpp / 8), "%s pitch %ld", c[i].name, (long)d.lPitch);
        CHECKF((d.lPitch & 3) == 0, "%s pitch %ld not DWORD aligned", c[i].name, (long)d.lPitch);
        CHECK(d.lpSurface != NULL);
        CHECK(d.dwFlags & DDSD_LPSURFACE);
        IDirectDrawSurface7_Unlock(s, NULL);
        IDirectDrawSurface7_Release(s);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_pixelformat_bad_dwsize) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    DWORD bad[] = { 0, 24, 28, 36 };
    for (unsigned i = 0; i < 4; i++) {
        pf.dwSize = bad[i];
        HRESULT hr; IDirectDrawSurface7 *s = mk(dd, 8, 8, &pf, SYSPLAIN, &hr);
        CHECKF(FAILED(hr), "pf.dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
        if (hr != DDERR_INVALIDPARAMS && hr != DDERR_INVALIDPIXELFORMAT) t_note("pf.dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
        if (s) IDirectDrawSurface7_Release(s);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_pixelformat_bad_masks) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    pf.dwRBitMask = pf.dwGBitMask = pf.dwBBitMask = 0;
    HRESULT hr; IDirectDrawSurface7 *s = mk(dd, 8, 8, &pf, SYSPLAIN, &hr);
    CHECKF(FAILED(hr), "all-zero masks -> %s", d3dh_hr_name(hr));
    if (s) IDirectDrawSurface7_Release(s);
    dl_pf_xrgb32(&pf); pf.dwRGBBitCount = 7;
    s = mk(dd, 8, 8, &pf, SYSPLAIN, &hr);
    CHECKF(FAILED(hr), "7 bpp RGB -> %s", d3dh_hr_name(hr));
    if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}

/* ---------------- Lock / Unlock ---------------- */
TEST(ddraw_legacy_surf_lock_unlock_roundtrip) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = mk(dd, 20, 10, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDSURFACEDESC2 d;
    EXPECT_HR(lock_all(s, &d, 0), DD_OK);
    EXPECT_EQ(d.dwSize, sizeof d);
    CHECK(d.lpSurface != NULL);
    CHECK(d.lPitch >= 80);
    for (int y = 0; y < 10; y++) for (int x = 0; x < 20; x++) *(DWORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 4) = (y << 8) | x | 0xff000000;
    EXPECT_HR(IDirectDrawSurface7_Unlock(s, NULL), DD_OK);
    EXPECT_EQ(px32(s, 0, 0), 0xff000000);
    EXPECT_EQ(px32(s, 19, 9), 0xff000913);
    EXPECT_EQ(px32(s, 7, 3), 0xff000307);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_unlock_without_lock) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (s) {
        EXPECT_HR(IDirectDrawSurface7_Unlock(s, NULL), DDERR_NOTLOCKED);
        DDSURFACEDESC2 d;
        EXPECT_HR(lock_all(s, &d, 0), DD_OK);
        EXPECT_HR(IDirectDrawSurface7_Unlock(s, NULL), DD_OK);
        EXPECT_HR(IDirectDrawSurface7_Unlock(s, NULL), DDERR_NOTLOCKED);
        IDirectDrawSurface7_Release(s);
    } else t_skip("no surface");
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_lock_rect_pointer) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = mk(dd, 32, 16, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDSURFACEDESC2 whole;
    lock_all(s, &whole, 0);
    BYTE *base = whole.lpSurface; LONG pitch = whole.lPitch;
    IDirectDrawSurface7_Unlock(s, NULL);
    RECT r = { 5, 3, 17, 9 };
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    EXPECT_HR(IDirectDrawSurface7_Lock(s, &r, &d, DDLOCK_WAIT, NULL), DD_OK);
    CHECK(d.lpSurface == base + 3 * pitch + 5 * 4);       /* pointer is the top-left of the requested rectangle */
    EXPECT_EQ(d.lPitch, pitch);
    EXPECT_HR(IDirectDrawSurface7_Unlock(s, &r), DD_OK);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_lock_bad_rect) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 16, 16, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    /* Lock rejects out-of-range and inverted rectangles with DDERR_INVALIDPARAMS (not DDERR_INVALIDRECT) */
    RECT bad[] = { { 0, 0, 17, 16 }, { 0, 0, 16, 17 }, { -1, 0, 8, 8 }, { 0, -1, 8, 8 }, { 8, 8, 4, 12 }, { 8, 8, 12, 4 }, { 20, 20, 30, 30 } };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
        HRESULT hr = IDirectDrawSurface7_Lock(s, &bad[i], &d, DDLOCK_WAIT, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "rect %u: (%ld,%ld,%ld,%ld) -> %s", i, bad[i].left, bad[i].top, bad[i].right, bad[i].bottom, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirectDrawSurface7_Unlock(s, &bad[i]);
    }
    RECT empty = { 8, 0, 8, 8 };                       /* right == left is not rejected */
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    HRESULT hr = IDirectDrawSurface7_Lock(s, &empty, &d, DDLOCK_WAIT, NULL);
    t_note("Lock of an empty rectangle -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) IDirectDrawSurface7_Unlock(s, &empty);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_lock_bad_desc_size) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 16, 16, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DWORD bad[] = { 0, 108, 124, 132, 140 };          /* Lock accepts exactly 120 or 136 on every surface interface */
    for (unsigned i = 0; i < 5; i++) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = bad[i];
        HRESULT hr = IDirectDrawSurface7_Lock(s, NULL, &d, DDLOCK_WAIT, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "dwSize %lu -> %s", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirectDrawSurface7_Unlock(s, NULL);
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_lock_desc_fields) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_rgb565(&pf);
    IDirectDrawSurface7 *s = mk(dd, 30, 7, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDSURFACEDESC2 d;
    EXPECT_HR(lock_all(s, &d, 0), DD_OK);
    CHECK((d.dwFlags & (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_LPSURFACE)) == (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_LPSURFACE));
    EXPECT_EQ(d.dwWidth, 30);
    EXPECT_EQ(d.dwHeight, 7);
    CHECK(d.lPitch >= 60);
    EXPECT_EQ(d.ddpfPixelFormat.dwRGBBitCount, 16);
    IDirectDrawSurface7_Unlock(s, NULL);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_lock_flags_validation) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DWORD bad[] = { 0x80000000, 0x100, 0x4, 0x8, 0x40, 0x80, 0x200, 0x400, 0x40000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
        HRESULT hr = IDirectDrawSurface7_Lock(s, NULL, &d, bad[i], NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "flags 0x%lx -> %s", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirectDrawSurface7_Unlock(s, NULL);
    }
    DWORD ok[] = { 0, DDLOCK_WAIT, DDLOCK_READONLY, DDLOCK_WRITEONLY, DDLOCK_NOSYSLOCK, DDLOCK_WAIT | DDLOCK_READONLY };
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
        HRESULT hr = IDirectDrawSurface7_Lock(s, NULL, &d, ok[i], NULL);
        CHECKF(hr == DD_OK, "flags 0x%lx -> %s", ok[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirectDrawSurface7_Unlock(s, NULL);
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_lock_conflicts) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 16, 16, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    RECT a = { 0, 0, 8, 8 }, b = { 8, 8, 16, 16 }, c = { 4, 4, 12, 12 };
    DDSURFACEDESC2 da, db, dc, dw;
    memset(&da, 0, sizeof da); da.dwSize = sizeof da; db = dc = dw = da;
    EXPECT_HR(IDirectDrawSurface7_Lock(s, &a, &da, 0, NULL), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_Lock(s, &b, &db, 0, NULL), DD_OK);          /* disjoint rectangles may be locked together */
    EXPECT_HR(IDirectDrawSurface7_Lock(s, &c, &dc, 0, NULL), DDERR_SURFACEBUSY);   /* overlapping one may not */
    EXPECT_HR(IDirectDrawSurface7_Lock(s, NULL, &dw, 0, NULL), DDERR_SURFACEBUSY); /* nor the whole surface */
    EXPECT_HR(IDirectDrawSurface7_Unlock(s, &a), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_Lock(s, NULL, &dw, 0, NULL), DDERR_SURFACEBUSY); /* b is still locked */
    EXPECT_HR(IDirectDrawSurface7_Unlock(s, &b), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_Lock(s, NULL, &dw, 0, NULL), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_Lock(s, NULL, &da, 0, NULL), DDERR_SURFACEBUSY);   /* whole surface locked twice */
    EXPECT_HR(IDirectDrawSurface7_Lock(s, &a, &da, 0, NULL), DDERR_SURFACEBUSY);      /* rect while the whole is locked */
    EXPECT_HR(IDirectDrawSurface7_Unlock(s, NULL), DD_OK);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_isLost_and_restore) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (s) {
        EXPECT_HR(IDirectDrawSurface7_IsLost(s), DD_OK);
        EXPECT_HR(IDirectDrawSurface7_Restore(s), DD_OK);
        IDirectDrawSurface7_Release(s);
    } else t_skip("no surface");
    teardown(dd, w);
}

/* ---------------- GetDC / ReleaseDC ---------------- */
TEST(ddraw_legacy_surf_getdc_gdi_draw) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = mk(dd, 16, 16, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    fill32(s, 0);
    HDC dc = NULL;
    HRESULT hr = IDirectDrawSurface7_GetDC(s, &dc);
    EXPECT_HR(hr, DD_OK);
    if (SUCCEEDED(hr) && dc) {
        EXPECT_EQ(GetObjectType(dc), OBJ_MEMDC);
        SetPixel(dc, 3, 2, RGB(255, 0, 0));
        SetPixel(dc, 4, 2, RGB(0, 255, 0));
        SetPixel(dc, 5, 2, RGB(0, 0, 255));
        GdiFlush();
        EXPECT_HR(IDirectDrawSurface7_ReleaseDC(s, dc), DD_OK);
        EXPECT_EQ(px32(s, 3, 2) & 0xffffff, 0xff0000);
        EXPECT_EQ(px32(s, 4, 2) & 0xffffff, 0x00ff00);
        EXPECT_EQ(px32(s, 5, 2) & 0xffffff, 0x0000ff);
        EXPECT_EQ(px32(s, 0, 0) & 0xffffff, 0);
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_getdc_twice_and_releasedc_twice) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    HDC dc = NULL, dc2 = NULL;
    if (SUCCEEDED(IDirectDrawSurface7_GetDC(s, &dc))) {
        EXPECT_HR(IDirectDrawSurface7_GetDC(s, &dc2), DDERR_DCALREADYCREATED);
        EXPECT_HR(IDirectDrawSurface7_ReleaseDC(s, dc), DD_OK);
        EXPECT_HR(IDirectDrawSurface7_ReleaseDC(s, dc), DDERR_NODC);
    } else t_skip("GetDC unavailable");
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_releasedc_wrong_dc) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    HDC dc = NULL;
    if (SUCCEEDED(IDirectDrawSurface7_GetDC(s, &dc))) {
        HDC other = CreateCompatibleDC(NULL);
        HRESULT hr = IDirectDrawSurface7_ReleaseDC(s, other);
        CHECKF(FAILED(hr), "ReleaseDC with a foreign DC -> %s", d3dh_hr_name(hr));
        DeleteDC(other);
        EXPECT_HR(IDirectDrawSurface7_ReleaseDC(s, dc), DD_OK);
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_getdc_palettized_surface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_pal8(&pf);
    IDirectDrawSurface7 *s = mk(dd, 8, 8, &pf, SYSPLAIN, NULL);
    if (s) {
        HDC dc = NULL;
        HRESULT hr = IDirectDrawSurface7_GetDC(s, &dc);
        t_note("GetDC on a P8 surface without palette -> %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirectDrawSurface7_ReleaseDC(s, dc);
        IDirectDrawSurface7_Release(s);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_getdc_16bit_surface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_rgb565(&pf);
    IDirectDrawSurface7 *s = mk(dd, 8, 8, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    HDC dc = NULL;
    HRESULT hr = IDirectDrawSurface7_GetDC(s, &dc);
    EXPECT_HR(hr, DD_OK);
    if (SUCCEEDED(hr)) {
        SetPixel(dc, 1, 1, RGB(255, 255, 255));
        GdiFlush();
        IDirectDrawSurface7_ReleaseDC(s, dc);
        DDSURFACEDESC2 d; lock_all(s, &d, DDLOCK_READONLY);
        WORD v = *(WORD *)((BYTE *)d.lpSurface + 1 * d.lPitch + 2);
        IDirectDrawSurface7_Unlock(s, NULL);
        EXPECT_EQ(v, 0xffff);
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}

/* ---------------- Blt ---------------- */
TEST(ddraw_legacy_blt_colorfill_whole) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = mk(dd, 12, 9, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    HRESULT hr;
    cfill(s, NULL, 0x00336699, &hr);
    EXPECT_HR(hr, DD_OK);
    for (int y = 0; y < 9; y++) for (int x = 0; x < 12; x++) if ((px32(s, x, y) & 0xffffff) != 0x336699) { CHECKF(0, "pixel (%d,%d) = %08lx", x, y, (unsigned long)px32(s, x, y)); y = 9; break; }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_colorfill_subrect) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = mk(dd, 16, 16, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    fill32(s, 0);
    RECT r = { 4, 5, 10, 9 };
    HRESULT hr; cfill(s, &r, 0x00ffffff, &hr);
    EXPECT_HR(hr, DD_OK);
    int bad = 0;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        int inside = x >= 4 && x < 10 && y >= 5 && y < 9;
        DWORD v = px32(s, x, y) & 0xffffff;
        if (v != (inside ? 0xffffffu : 0u)) bad++;
    }
    EXPECT_EQ(bad, 0);                                    /* right/bottom edge exclusive */
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_colorfill_16bit_masks) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_rgb565(&pf);
    IDirectDrawSurface7 *s = mk(dd, 8, 4, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    HRESULT hr; cfill(s, NULL, 0xf81f, &hr);
    EXPECT_HR(hr, DD_OK);
    DDSURFACEDESC2 d; lock_all(s, &d, DDLOCK_READONLY);
    int bad = 0;
    for (int y = 0; y < 4; y++) for (int x = 0; x < 8; x++) if (*(WORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 2) != 0xf81f) bad++;
    IDirectDrawSurface7_Unlock(s, NULL);
    EXPECT_EQ(bad, 0);                                    /* the fill colour is the raw pixel value */
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_colorfill_8bit_index) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_pal8(&pf);
    IDirectDrawSurface7 *s = mk(dd, 8, 4, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    HRESULT hr; cfill(s, NULL, 0x5a, &hr);
    EXPECT_HR(hr, DD_OK);
    DDSURFACEDESC2 d; lock_all(s, &d, DDLOCK_READONLY);
    int bad = 0;
    for (int y = 0; y < 4; y++) for (int x = 0; x < 8; x++) if (((BYTE *)d.lpSurface)[y * d.lPitch + x] != 0x5a) bad++;
    IDirectDrawSurface7_Unlock(s, NULL);
    EXPECT_EQ(bad, 0);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_colorfill_needs_fx) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    HRESULT hr = IDirectDrawSurface7_Blt(s, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, NULL);
    CHECKF(FAILED(hr), "COLORFILL without DDBLTFX -> %s", d3dh_hr_name(hr));
    DDBLTFX fx; memset(&fx, 0, sizeof fx); fx.dwSize = sizeof fx - 4;
    hr = IDirectDrawSurface7_Blt(s, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    CHECKF(FAILED(hr), "COLORFILL with wrong DDBLTFX size -> %s", d3dh_hr_name(hr));
    CHECK(sizeof(DDBLTFX) > sizeof(DWORD));
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_bad_dest_rect) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 16, 16, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    RECT bad[] = { { 0, 0, 17, 16 }, { 0, 0, 16, 17 }, { -1, 0, 8, 8 }, { 0, -1, 8, 8 }, { 8, 8, 8, 12 }, { 8, 8, 12, 8 }, { 12, 12, 4, 4 } };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr; cfill(s, &bad[i], 0xffffff, &hr);
        CHECKF(hr == DDERR_INVALIDRECT, "dest rect %u -> %s", i, d3dh_hr_name(hr));
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_copy_same_format) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *src = mk(dd, 8, 8, &pf, SYSPLAIN, NULL), *dst = mk(dd, 16, 16, &pf, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    {
        DDSURFACEDESC2 d; lock_all(src, &d, 0);
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) *(DWORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 4) = (y << 4) | x;
        IDirectDrawSurface7_Unlock(src, NULL);
        fill32(dst, 0xffffffff);
        RECT dr = { 4, 6, 12, 14 };
        EXPECT_HR(IDirectDrawSurface7_Blt(dst, &dr, src, NULL, DDBLT_WAIT, NULL), DD_OK);
        int bad = 0;
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            int in = x >= 4 && x < 12 && y >= 6 && y < 14;
            DWORD want = in ? (DWORD)(((y - 6) << 4) | (x - 4)) : 0xffffffffu;
            if (px32(dst, x, y) != want) bad++;
        }
        EXPECT_EQ(bad, 0);
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_source_subrect) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *src = mk(dd, 8, 8, &pf, SYSPLAIN, NULL), *dst = mk(dd, 8, 8, &pf, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    {
        DDSURFACEDESC2 d; lock_all(src, &d, 0);
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) *(DWORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 4) = (y << 4) | x;
        IDirectDrawSurface7_Unlock(src, NULL);
        fill32(dst, 0);
        RECT sr = { 2, 3, 6, 7 }, dr = { 0, 0, 4, 4 };
        EXPECT_HR(IDirectDrawSurface7_Blt(dst, &dr, src, &sr, DDBLT_WAIT, NULL), DD_OK);
        EXPECT_EQ(px32(dst, 0, 0), 0x32);
        EXPECT_EQ(px32(dst, 3, 3), 0x65);
        EXPECT_EQ(px32(dst, 4, 4), 0);
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_bad_source_rect) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *src = mk(dd, 8, 8, NULL, SYSPLAIN, NULL), *dst = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    {
        RECT bad[] = { { 0, 0, 9, 8 }, { -1, 0, 4, 4 }, { 4, 4, 4, 8 }, { 6, 6, 2, 2 } };
        for (unsigned i = 0; i < 4; i++) {
            HRESULT hr = IDirectDrawSurface7_Blt(dst, NULL, src, &bad[i], DDBLT_WAIT, NULL);
            CHECKF(hr == DDERR_INVALIDRECT, "src rect %u -> %s", i, d3dh_hr_name(hr));
        }
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_stretch_integer_upscale) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *src = mk(dd, 2, 2, &pf, SYSPLAIN, NULL), *dst = mk(dd, 4, 4, &pf, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    {
        DDSURFACEDESC2 d; lock_all(src, &d, 0);
        DWORD c[4] = { 0x10, 0x20, 0x30, 0x40 };
        for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) *(DWORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 4) = c[y * 2 + x];
        IDirectDrawSurface7_Unlock(src, NULL);
        HRESULT hr = IDirectDrawSurface7_Blt(dst, NULL, src, NULL, DDBLT_WAIT, NULL);
        if (FAILED(hr)) { t_skip("stretch blt -> %s", d3dh_hr_name(hr)); goto out; }
        int bad = 0;
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (px32(dst, x, y) != c[(y / 2) * 2 + (x / 2)]) bad++;
        EXPECT_EQ(bad, 0);                                /* 2x nearest-neighbour replication */
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_format_conversion_32_to_16) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT p32, p16; dl_pf_xrgb32(&p32); dl_pf_rgb565(&p16);
    IDirectDrawSurface7 *src = mk(dd, 4, 4, &p32, SYSPLAIN, NULL), *dst = mk(dd, 4, 4, &p16, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    fill32(src, 0x00ff00ff);
    {
        HRESULT hr = IDirectDrawSurface7_Blt(dst, NULL, src, NULL, DDBLT_WAIT, NULL);
        t_note("32->16 bpp blt -> %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) {
            DDSURFACEDESC2 d; lock_all(dst, &d, DDLOCK_READONLY);
            WORD v = *(WORD *)d.lpSurface;
            IDirectDrawSurface7_Unlock(dst, NULL);
            t_note("converted pixel 0x%04x", v);
        }
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_src_colorkey) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *src = mk(dd, 4, 4, &pf, SYSPLAIN, NULL), *dst = mk(dd, 4, 4, &pf, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    {
        DDSURFACEDESC2 d; lock_all(src, &d, 0);
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) *(DWORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 4) = ((x + y) & 1) ? 0x00ff00ff : 0x00112233;
        IDirectDrawSurface7_Unlock(src, NULL);
        DDCOLORKEY ck = { 0x00ff00ff, 0x00ff00ff };
        EXPECT_HR(IDirectDrawSurface7_SetColorKey(src, DDCKEY_SRCBLT, &ck), DD_OK);
        fill32(dst, 0x00abcdef);
        EXPECT_HR(IDirectDrawSurface7_Blt(dst, NULL, src, NULL, DDBLT_KEYSRC | DDBLT_WAIT, NULL), DD_OK);
        int bad = 0;
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
            DWORD want = ((x + y) & 1) ? 0x00abcdefu : 0x00112233u;
            if ((px32(dst, x, y) & 0xffffff) != want) bad++;
        }
        EXPECT_EQ(bad, 0);                                /* key-coloured source pixels are skipped */
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_src_colorkey_override) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *src = mk(dd, 4, 4, &pf, SYSPLAIN, NULL), *dst = mk(dd, 4, 4, &pf, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    fill32(src, 0x00123456);
    fill32(dst, 0x00abcdef);
    {
        DDBLTFX fx; memset(&fx, 0, sizeof fx); fx.dwSize = sizeof fx;
        fx.ddckSrcColorkey.dwColorSpaceLowValue = fx.ddckSrcColorkey.dwColorSpaceHighValue = 0x00123456;
        HRESULT hr = IDirectDrawSurface7_Blt(dst, NULL, src, NULL, DDBLT_KEYSRCOVERRIDE | DDBLT_WAIT, &fx);
        EXPECT_HR(hr, DD_OK);
        EXPECT_EQ(px32(dst, 1, 1) & 0xffffff, 0xabcdef);  /* every pixel matched the override key */
        fx.ddckSrcColorkey.dwColorSpaceLowValue = fx.ddckSrcColorkey.dwColorSpaceHighValue = 0x00000001;
        hr = IDirectDrawSurface7_Blt(dst, NULL, src, NULL, DDBLT_KEYSRCOVERRIDE | DDBLT_WAIT, &fx);
        EXPECT_HR(hr, DD_OK);
        EXPECT_EQ(px32(dst, 1, 1) & 0xffffff, 0x123456);
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_keysrc_without_colorkey) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *src = mk(dd, 4, 4, NULL, SYSPLAIN, NULL), *dst = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    if (src && dst) {
        HRESULT hr = IDirectDrawSurface7_Blt(dst, NULL, src, NULL, DDBLT_KEYSRC | DDBLT_WAIT, NULL);
        t_note("DDBLT_KEYSRC with no colour key on the source -> %s", d3dh_hr_name(hr));
    } else t_skip("no surfaces");
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_overlapping_self_copy) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = mk(dd, 8, 8, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDSURFACEDESC2 d; lock_all(s, &d, 0);
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) *(DWORD *)((BYTE *)d.lpSurface + y * d.lPitch + x * 4) = y * 8 + x;
    IDirectDrawSurface7_Unlock(s, NULL);
    RECT dr = { 0, 1, 8, 8 }, sr = { 0, 0, 8, 7 };
    HRESULT hr = IDirectDrawSurface7_Blt(s, &dr, s, &sr, DDBLT_WAIT, NULL);
    t_note("self blt shifted by one row -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        int bad = 0;
        for (int y = 1; y < 8; y++) for (int x = 0; x < 8; x++) if (px32(s, x, y) != (DWORD)((y - 1) * 8 + x)) bad++;
        if (bad) t_note("overlapping self-blt produced %d unexpected pixels", bad);
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_lost_state_not_set) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    if (s) {
        EXPECT_HR(IDirectDrawSurface7_GetBltStatus(s, DDGBS_CANBLT), DD_OK);
        EXPECT_HR(IDirectDrawSurface7_GetBltStatus(s, DDGBS_ISBLTDONE), DD_OK);
        HRESULT hr = IDirectDrawSurface7_GetBltStatus(s, 0);
        CHECKF(hr == DDERR_INVALIDPARAMS, "flags 0 -> %s", d3dh_hr_name(hr));
        IDirectDrawSurface7_Release(s);
    } else t_skip("no surface");
    teardown(dd, w);
}
TEST(ddraw_legacy_blt_batch_unsupported) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface *s1 = NULL;
    IDirectDrawSurface7 *s = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    if (s && SUCCEEDED(IDirectDrawSurface7_QueryInterface(s, &IID_IDirectDrawSurface, (void **)&s1)) && s1) {
        DDBLTBATCH b; memset(&b, 0, sizeof b);
        EXPECT_HR(IDirectDrawSurface_BltBatch(s1, &b, 1, 0), DDERR_UNSUPPORTED);
        IDirectDrawSurface_Release(s1);
    } else t_skip("no surface");
    if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_bltfast_copy) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *src = mk(dd, 4, 4, &pf, SYSPLAIN, NULL), *dst = mk(dd, 8, 8, &pf, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    fill32(src, 0x00777777); fill32(dst, 0);
    {
        HRESULT hr = IDirectDrawSurface7_BltFast(dst, 2, 3, src, NULL, DDBLTFAST_WAIT);
        EXPECT_HR(hr, DD_OK);
        EXPECT_EQ(px32(dst, 2, 3) & 0xffffff, 0x777777);
        EXPECT_EQ(px32(dst, 5, 6) & 0xffffff, 0x777777);
        EXPECT_EQ(px32(dst, 6, 3) & 0xffffff, 0);
        EXPECT_EQ(px32(dst, 1, 3) & 0xffffff, 0);
        EXPECT_EQ(px32(dst, 2, 7) & 0xffffff, 0);
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_bltfast_out_of_bounds) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *src = mk(dd, 4, 4, NULL, SYSPLAIN, NULL), *dst = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    {
        struct { int x, y; } p[] = { { 5, 0 }, { 0, 5 }, { -1, 0 }, { 0, -1 }, { 8, 8 }, { 100, 100 } };
        for (unsigned i = 0; i < 6; i++) {
            HRESULT hr = IDirectDrawSurface7_BltFast(dst, p[i].x, p[i].y, src, NULL, DDBLTFAST_WAIT);
            CHECKF(hr == DDERR_INVALIDRECT, "BltFast at (%d,%d) -> %s", p[i].x, p[i].y, d3dh_hr_name(hr));
        }
        RECT bad = { 0, 0, 5, 4 };
        HRESULT hr = IDirectDrawSurface7_BltFast(dst, 0, 0, src, &bad, DDBLTFAST_WAIT);
        CHECKF(hr == DDERR_INVALIDRECT, "BltFast with oversized source rect -> %s", d3dh_hr_name(hr));
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}
TEST(ddraw_legacy_bltfast_srccolorkey) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *src = mk(dd, 2, 1, &pf, SYSPLAIN, NULL), *dst = mk(dd, 2, 1, &pf, SYSPLAIN, NULL);
    if (!src || !dst) { t_skip("no surfaces"); goto out; }
    {
        DDSURFACEDESC2 d; lock_all(src, &d, 0);
        ((DWORD *)d.lpSurface)[0] = 0x00ff00ff; ((DWORD *)d.lpSurface)[1] = 0x00010203;
        IDirectDrawSurface7_Unlock(src, NULL);
        DDCOLORKEY ck = { 0x00ff00ff, 0x00ff00ff };
        IDirectDrawSurface7_SetColorKey(src, DDCKEY_SRCBLT, &ck);
        fill32(dst, 0x00404040);
        EXPECT_HR(IDirectDrawSurface7_BltFast(dst, 0, 0, src, NULL, DDBLTFAST_SRCCOLORKEY | DDBLTFAST_WAIT), DD_OK);
        EXPECT_EQ(px32(dst, 0, 0) & 0xffffff, 0x404040);
        EXPECT_EQ(px32(dst, 1, 0) & 0xffffff, 0x010203);
    }
out:
    if (src) IDirectDrawSurface7_Release(src);
    if (dst) IDirectDrawSurface7_Release(dst);
    teardown(dd, w);
}

/* ---------------- colour keys ---------------- */
/* GetColorKey reports DDERR_NOCOLORKEYHW first when neither driver layer supports colour keys at all */
static int ck_is(HRESULT hr, HRESULT want) { return hr == want || hr == DDERR_NOCOLORKEYHW; }
TEST(ddraw_legacy_ckey_none_set) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    if (s) {
        DDCOLORKEY ck = { 1, 2 };
        HRESULT hr;
        hr = IDirectDrawSurface7_GetColorKey(s, DDCKEY_SRCBLT, &ck);      CHECKF(ck_is(hr, DDERR_NOCOLORKEY), "SRCBLT -> %s", d3dh_hr_name(hr));
        hr = IDirectDrawSurface7_GetColorKey(s, DDCKEY_DESTBLT, &ck);     CHECKF(ck_is(hr, DDERR_NOCOLORKEY), "DESTBLT -> %s", d3dh_hr_name(hr));
        hr = IDirectDrawSurface7_GetColorKey(s, DDCKEY_DESTOVERLAY, &ck); CHECKF(ck_is(hr, DDERR_NOCOLORKEY), "DESTOVERLAY -> %s", d3dh_hr_name(hr));
        /* a source overlay key only exists on overlay surfaces */
        hr = IDirectDrawSurface7_GetColorKey(s, DDCKEY_SRCOVERLAY, &ck);  CHECKF(ck_is(hr, DDERR_INVALIDPARAMS), "SRCOVERLAY on a non-overlay -> %s", d3dh_hr_name(hr));
        EXPECT_EQ(ck.dwColorSpaceLowValue, 1);                            /* output untouched on failure */
        IDirectDrawSurface7_Release(s);
    } else t_skip("no surface");
    teardown(dd, w);
}
TEST(ddraw_legacy_ckey_srcblt_roundtrip) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = mk(dd, 4, 4, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDCOLORKEY ck = { 0x00112233, 0x00112233 }, out = { 0, 0 };
    EXPECT_HR(IDirectDrawSurface7_SetColorKey(s, DDCKEY_SRCBLT, &ck), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_GetColorKey(s, DDCKEY_SRCBLT, &out), DD_OK);
    EXPECT_EQ(out.dwColorSpaceLowValue, 0x00112233);
    EXPECT_EQ(out.dwColorSpaceHighValue, 0x00112233);
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    IDirectDrawSurface7_GetSurfaceDesc(s, &d);
    CHECK(d.dwFlags & DDSD_CKSRCBLT);
    EXPECT_EQ(d.ddckCKSrcBlt.dwColorSpaceLowValue, 0x00112233);
    EXPECT_HR(IDirectDrawSurface7_GetColorKey(s, DDCKEY_DESTBLT, &out), DDERR_NOCOLORKEY);   /* other key kinds unaffected */
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_ckey_single_value_sets_high_equal_low) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDCOLORKEY ck = { 0x00abcdef, 0 }, out = { 0, 0 };
    EXPECT_HR(IDirectDrawSurface7_SetColorKey(s, DDCKEY_SRCBLT, &ck), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_GetColorKey(s, DDCKEY_SRCBLT, &out), DD_OK);
    EXPECT_EQ(out.dwColorSpaceLowValue, 0x00abcdef);
    t_note("high value after a single-value key: 0x%lx", out.dwColorSpaceHighValue);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_ckey_clear_with_null) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDCOLORKEY ck = { 5, 5 }, out;
    EXPECT_HR(IDirectDrawSurface7_SetColorKey(s, DDCKEY_SRCBLT, &ck), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_SetColorKey(s, DDCKEY_SRCBLT, NULL), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_GetColorKey(s, DDCKEY_SRCBLT, &out), DDERR_NOCOLORKEY);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_ckey_bad_flags) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDCOLORKEY ck = { 1, 1 };
    HRESULT hr;
    DWORD hi[] = { 0x20, 0x40, 0x100, 0x80000000, 0xffffffe0 };
    for (unsigned i = 0; i < 5; i++) {
        hr = IDirectDrawSurface7_SetColorKey(s, hi[i], &ck);
        CHECKF(hr == DDERR_INVALIDPARAMS, "SetColorKey(0x%lx) -> %s", hi[i], d3dh_hr_name(hr));
        hr = IDirectDrawSurface7_GetColorKey(s, hi[i], &ck);
        CHECKF(hr == DDERR_INVALIDPARAMS, "GetColorKey(0x%lx) -> %s", hi[i], d3dh_hr_name(hr));
    }
    hr = IDirectDrawSurface7_SetColorKey(s, 0, &ck);
    CHECKF(FAILED(hr), "SetColorKey(0) -> %s", d3dh_hr_name(hr));
    hr = IDirectDrawSurface7_GetColorKey(s, 0, &ck);
    CHECKF(ck_is(hr, DDERR_INVALIDPARAMS), "GetColorKey(0) -> %s", d3dh_hr_name(hr));
    hr = IDirectDrawSurface7_GetColorKey(s, DDCKEY_SRCBLT | DDCKEY_DESTBLT, &ck);          /* kinds cannot be combined */
    CHECKF(ck_is(hr, DDERR_INVALIDPARAMS), "GetColorKey(SRCBLT|DESTBLT) -> %s", d3dh_hr_name(hr));
    hr = IDirectDrawSurface7_GetColorKey(s, DDCKEY_COLORSPACE, &ck);                       /* a kind is required */
    CHECKF(ck_is(hr, DDERR_INVALIDPARAMS), "GetColorKey(COLORSPACE only) -> %s", d3dh_hr_name(hr));
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_ckey_dest_and_overlay) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDCOLORKEY ck = { 9, 9 };
    HRESULT h1 = IDirectDrawSurface7_SetColorKey(s, DDCKEY_DESTBLT, &ck);
    HRESULT h2 = IDirectDrawSurface7_SetColorKey(s, DDCKEY_SRCOVERLAY, &ck);
    HRESULT h3 = IDirectDrawSurface7_SetColorKey(s, DDCKEY_DESTOVERLAY, &ck);
    t_note("DESTBLT -> %s, SRCOVERLAY -> %s, DESTOVERLAY -> %s", d3dh_hr_name(h1), d3dh_hr_name(h2), d3dh_hr_name(h3));
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}

/* ---------------- surface interfaces ---------------- */
TEST(ddraw_legacy_surf_qi_ladder) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    const IID *iids[] = { &IID_IUnknown, &IID_IDirectDrawSurface, &IID_IDirectDrawSurface2, &IID_IDirectDrawSurface3, &IID_IDirectDrawSurface4, &IID_IDirectDrawSurface7 };
    for (unsigned i = 0; i < 6; i++) {
        IUnknown *q = NULL;
        HRESULT hr = IDirectDrawSurface7_QueryInterface(s, iids[i], (void **)&q);
        CHECKF(hr == S_OK && q, "QI %s -> %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        if (q) IUnknown_Release(q);
    }
    EXPECT_RET(IDirectDrawSurface7_Release(s), 0);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_qi_errors) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    void *p = (void *)0x42;
    if (!t_is_wine()) { EXPECT_HR(s->lpVtbl->QueryInterface(s, NULL, &p), E_INVALIDARG); EXPECT_EQ(p, 0); }
    p = (void *)0x42;
    HRESULT hr = IDirectDrawSurface7_QueryInterface(s, &IID_IDirectDraw7, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    EXPECT_EQ(p, 0);
    hr = IDirectDrawSurface7_QueryInterface(s, &IID_IDirectDrawClipper, &p);
    EXPECT_HR(hr, E_NOINTERFACE);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_qi_gamma_control_not_on_offscreen) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    void *p = NULL;
    HRESULT hr = IDirectDrawSurface7_QueryInterface(s, &IID_IDirectDrawGammaControl, &p);
    t_note("QI(IDirectDrawGammaControl) on an offscreen surface -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_refcount_independent_interfaces) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    IDirectDrawSurface4 *s4 = NULL;
    EXPECT_HR(IDirectDrawSurface7_QueryInterface(s, &IID_IDirectDrawSurface4, (void **)&s4), DD_OK);
    if (s4) {
        EXPECT_RET(IDirectDrawSurface4_AddRef(s4), 2);
        EXPECT_RET(IDirectDrawSurface4_Release(s4), 1);
        EXPECT_RET(IDirectDrawSurface7_Release(s), 0);        /* surface7's own count */
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
        EXPECT_HR(IDirectDrawSurface4_GetSurfaceDesc(s4, &d), DD_OK);   /* object still alive through s4 */
        EXPECT_RET(IDirectDrawSurface4_Release(s4), 0);
    } else IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_dd7_surface_holds_dd_reference) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    EXPECT_EQ(d3dh_refcount(dd), 1);
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    EXPECT_EQ(d3dh_refcount(dd), 2);                           /* a DD4/DD7 surface AddRefs its creator */
    EXPECT_RET(IDirectDrawSurface7_Release(s), 0);
    EXPECT_EQ(d3dh_refcount(dd), 1);
    s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    EXPECT_RET(IDirectDraw7_Release(dd), 1);                   /* the surface keeps the object usable */
    DDSURFACEDESC2 d; lock_all(s, &d, 0);
    CHECK(d.lpSurface != NULL);
    IDirectDrawSurface7_Unlock(s, NULL);
    IDirectDrawSurface7_Release(s);
    dl_window_free(w);
}
TEST(ddraw_legacy_surf_dd1_surface_does_not_hold_dd_reference) {
    HWND w; IDirectDraw7 *dd7 = setup(&w); if (!dd7) return;
    IDirectDraw *dd = NULL;
    if (FAILED(IDirectDraw7_QueryInterface(dd7, &IID_IDirectDraw, (void **)&dd)) || !dd) { teardown(dd7, w); return; }
    ULONG before = d3dh_refcount(dd);
    DDSURFACEDESC d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.dwWidth = d.dwHeight = 8; d.ddsCaps.dwCaps = SYSPLAIN;
    IDirectDrawSurface *s = NULL;
    if (SUCCEEDED(IDirectDraw_CreateSurface(dd, &d, &s, NULL)) && s) {
        EXPECT_EQ(d3dh_refcount(dd), before);                  /* unlike DD4/DD7, DD1/DD2 surfaces take no interface reference */
        IDirectDrawSurface_Release(s);
    }
    IDirectDraw_Release(dd);
    teardown(dd7, w);
}
TEST(ddraw_legacy_surf_get_dd_interface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    IDirectDraw7 *g = NULL;
    HRESULT hr = IDirectDrawSurface7_GetDDInterface(s, (LPVOID *)&g);
    EXPECT_HR(hr, DD_OK);
    if (g) {
        t_note("GetDDInterface returned %s object", g == dd ? "the same" : "a different");
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
        EXPECT_HR(IDirectDraw7_GetDisplayMode(g, &d), DD_OK);
        IDirectDraw7_Release(g);
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_getcaps) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDSCAPS2 c; memset(&c, 0xCD, sizeof c);
    EXPECT_HR(IDirectDrawSurface7_GetCaps(s, &c), DD_OK);
    CHECK((c.dwCaps & SYSPLAIN) == SYSPLAIN);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_private_data) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    static const GUID tag = { 0x1234abcd, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    char buf[32] = "hello", out[32] = { 0 }; DWORD sz = sizeof out;
    EXPECT_HR(IDirectDrawSurface7_GetPrivateData(s, &tag, out, &sz), DDERR_NOTFOUND);
    EXPECT_HR(IDirectDrawSurface7_SetPrivateData(s, &tag, buf, 6, 0), DD_OK);
    sz = sizeof out;
    EXPECT_HR(IDirectDrawSurface7_GetPrivateData(s, &tag, out, &sz), DD_OK);
    EXPECT_EQ(sz, 6);
    EXPECT_STR(out, "hello");
    sz = 2;                                                    /* buffer-size protocol */
    HRESULT hr = IDirectDrawSurface7_GetPrivateData(s, &tag, out, &sz);
    EXPECT_HR(hr, DDERR_MOREDATA);
    EXPECT_EQ(sz, 6);
    EXPECT_HR(IDirectDrawSurface7_FreePrivateData(s, &tag), DD_OK);
    sz = sizeof out;
    EXPECT_HR(IDirectDrawSurface7_GetPrivateData(s, &tag, out, &sz), DDERR_NOTFOUND);
    EXPECT_HR(IDirectDrawSurface7_FreePrivateData(s, &tag), DDERR_NOTFOUND);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_private_data_iunknown) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    static const GUID tag = { 0x1234abce, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    IDirectDraw7_AddRef(dd);                                   /* use the dd interface as an arbitrary IUnknown */
    ULONG before = d3dh_refcount(dd);
    EXPECT_HR(IDirectDrawSurface7_SetPrivateData(s, &tag, dd, sizeof(void *), DDSPD_IUNKNOWNPOINTER), DD_OK);
    ULONG held = d3dh_refcount(dd);
    CHECK(held == before + 1);                                 /* the surface AddRef'd it */
    EXPECT_HR(IDirectDrawSurface7_FreePrivateData(s, &tag), DD_OK);
    EXPECT_EQ(d3dh_refcount(dd), before);
    IDirectDraw7_Release(dd);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_uniqueness_value) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DWORD a = 0, b = 0, c = 0;
    EXPECT_HR(IDirectDrawSurface7_GetUniquenessValue(s, &a), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_ChangeUniquenessValue(s), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_GetUniquenessValue(s, &b), DD_OK);
    CHECK(a != b);
    EXPECT_HR(IDirectDrawSurface7_GetUniquenessValue(s, &c), DD_OK);
    EXPECT_EQ(b, c);                                           /* stable until something changes */
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_priority_and_lod_need_managed) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DWORD v = 0;
    HRESULT h1 = IDirectDrawSurface7_SetPriority(s, 1);
    HRESULT h2 = IDirectDrawSurface7_GetPriority(s, &v);
    HRESULT h3 = IDirectDrawSurface7_SetLOD(s, 1);
    HRESULT h4 = IDirectDrawSurface7_GetLOD(s, &v);
    t_note("SetPriority=%s GetPriority=%s SetLOD=%s GetLOD=%s on a plain surface", d3dh_hr_name(h1), d3dh_hr_name(h2), d3dh_hr_name(h3), d3dh_hr_name(h4));
    CHECK(FAILED(h4));                                         /* LOD is a managed-texture-only operation */
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}

/* ---------------- attached surfaces, flipping ---------------- */
TEST(ddraw_legacy_surf_no_attached_surfaces) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDSCAPS2 caps; memset(&caps, 0, sizeof caps); caps.dwCaps = DDSCAPS_ZBUFFER;
    IDirectDrawSurface7 *a = (IDirectDrawSurface7 *)(ULONG_PTR)1;
    HRESULT hr = IDirectDrawSurface7_GetAttachedSurface(s, &caps, &a);
    EXPECT_HR(hr, DDERR_NOTFOUND);
    if (SUCCEEDED(hr) && a) IDirectDrawSurface7_Release(a);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
static HRESULT WINAPI count_att(LPDIRECTDRAWSURFACE7 s, LPDDSURFACEDESC2 d, LPVOID ctx) { (*(int *)ctx)++; IDirectDrawSurface7_Release(s); return DDENUMRET_OK; }
TEST(ddraw_legacy_surf_enum_attached_none) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    int n = 0;
    EXPECT_HR(IDirectDrawSurface7_EnumAttachedSurfaces(s, &n, count_att), DD_OK);
    EXPECT_EQ(n, 0);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_attach_plain_surfaces_refused) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *a = mk(dd, 8, 8, NULL, SYSPLAIN, NULL), *b = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (a && b) {
        HRESULT hr = IDirectDrawSurface7_AddAttachedSurface(a, b);
        EXPECT_HR(hr, DDERR_CANNOTATTACHSURFACE);
        if (SUCCEEDED(hr)) IDirectDrawSurface7_DeleteAttachedSurface(a, 0, b);
        hr = IDirectDrawSurface7_AddAttachedSurface(a, a);
        CHECKF(FAILED(hr), "attach to itself -> %s", d3dh_hr_name(hr));
    } else t_skip("no surfaces");
    if (a) IDirectDrawSurface7_Release(a);
    if (b) IDirectDrawSurface7_Release(b);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_delete_attached_not_found) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *a = mk(dd, 8, 8, NULL, SYSPLAIN, NULL), *b = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (a && b) {
        HRESULT hr = IDirectDrawSurface7_DeleteAttachedSurface(a, 0, b);
        CHECKF(FAILED(hr), "DeleteAttachedSurface of a non-attached surface -> %s", d3dh_hr_name(hr));
        hr = IDirectDrawSurface7_DeleteAttachedSurface(a, 1, NULL);
        CHECKF(FAILED(hr), "nonzero flags -> %s", d3dh_hr_name(hr));
    } else t_skip("no surfaces");
    if (a) IDirectDrawSurface7_Release(a);
    if (b) IDirectDrawSurface7_Release(b);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_flip_not_flippable) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    EXPECT_HR(IDirectDrawSurface7_Flip(s, NULL, DDFLIP_WAIT), DDERR_NOTFLIPPABLE);
    HRESULT hr = IDirectDrawSurface7_GetFlipStatus(s, DDGFS_CANFLIP);
    t_note("GetFlipStatus on a plain surface -> %s", d3dh_hr_name(hr));
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_mipmap_chain) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_MIPMAPCOUNT | DDSD_PIXELFORMAT;
    d.dwWidth = d.dwHeight = 32; d.dwMipMapCount = 4;
    d.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX | DDSCAPS_SYSTEMMEMORY;
    dl_pf_xrgb32(&d.ddpfPixelFormat); d.ddpfPixelFormat.dwFlags |= DDPF_ALPHAPIXELS; d.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
    IDirectDrawSurface7 *top = NULL;
    HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &top, NULL);
    if (FAILED(hr)) { t_skip("system-memory mip chain -> %s", d3dh_hr_name(hr)); teardown(dd, w); return; }
    DWORD expect = 32; IDirectDrawSurface7 *cur = top; IDirectDrawSurface7_AddRef(cur);
    for (int level = 0; level < 4; level++) {
        DDSURFACEDESC2 g; memset(&g, 0, sizeof g); g.dwSize = sizeof g;
        EXPECT_HR(IDirectDrawSurface7_GetSurfaceDesc(cur, &g), DD_OK);
        CHECKF(g.dwWidth == expect && g.dwHeight == expect, "level %d is %lux%lu, want %lu", level, g.dwWidth, g.dwHeight, expect);
        if (level == 0) EXPECT_EQ(g.dwMipMapCount, 4);
        DDSCAPS2 caps; memset(&caps, 0, sizeof caps); caps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
        IDirectDrawSurface7 *next = NULL;
        hr = IDirectDrawSurface7_GetAttachedSurface(cur, &caps, &next);
        IDirectDrawSurface7_Release(cur);
        if (level == 3) { EXPECT_HR(hr, DDERR_NOTFOUND); cur = NULL; break; }
        EXPECT_HR(hr, DD_OK);
        if (FAILED(hr)) { cur = NULL; break; }
        cur = next; expect >>= 1;
    }
    if (cur) IDirectDrawSurface7_Release(cur);
    IDirectDrawSurface7_Release(top);
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_zbuffer_create) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    d.dwWidth = d.dwHeight = 16; d.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_SYSTEMMEMORY;
    d.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT); d.ddpfPixelFormat.dwFlags = DDPF_ZBUFFER;
    d.ddpfPixelFormat.dwZBufferBitDepth = 16; d.ddpfPixelFormat.dwZBitMask = 0xffff;
    IDirectDrawSurface7 *z = NULL;
    HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &z, NULL);
    t_note("16-bit system-memory z-buffer -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && z) {
        DDPIXELFORMAT pf; memset(&pf, 0, sizeof pf); pf.dwSize = sizeof pf;
        EXPECT_HR(IDirectDrawSurface7_GetPixelFormat(z, &pf), DD_OK);
        CHECK(pf.dwFlags & DDPF_ZBUFFER);
        EXPECT_EQ(pf.dwZBufferBitDepth, 16);
        IDirectDrawSurface7_Release(z);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_surf_getpixelformat_bad_size) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 8, 8, NULL, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    DDPIXELFORMAT pf; memset(&pf, 0, sizeof pf); pf.dwSize = sizeof pf;
    EXPECT_HR(IDirectDrawSurface7_GetPixelFormat(s, &pf), DD_OK);
    EXPECT_EQ(pf.dwSize, sizeof pf);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}

/* ---------------- primary surface in normal mode ---------------- */
TEST(ddraw_legacy_primary_create_normal_mode) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    HRESULT hr; IDirectDrawSurface7 *p = mk(dd, 0, 0, NULL, DDSCAPS_PRIMARYSURFACE, &hr);
    if (!p) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.dwFlags = DDSD_CAPS; d.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
        hr = IDirectDraw7_CreateSurface(dd, &d, &p, NULL);
    }
    if (FAILED(hr) || !p) { t_skip("primary surface unavailable here: %s", d3dh_hr_name(hr)); teardown(dd, w); return; }
    DDSURFACEDESC2 g; memset(&g, 0, sizeof g); g.dwSize = sizeof g;
    EXPECT_HR(IDirectDrawSurface7_GetSurfaceDesc(p, &g), DD_OK);
    CHECK(g.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE);
    DDSURFACEDESC2 m; memset(&m, 0, sizeof m); m.dwSize = sizeof m;
    EXPECT_HR(IDirectDraw7_GetDisplayMode(dd, &m), DD_OK);
    EXPECT_EQ(g.dwWidth, m.dwWidth);
    EXPECT_EQ(g.dwHeight, m.dwHeight);
    EXPECT_EQ(g.ddpfPixelFormat.dwRGBBitCount, m.ddpfPixelFormat.dwRGBBitCount);
    EXPECT_RET(IDirectDrawSurface7_Release(p), 0);
    teardown(dd, w);
}
TEST(ddraw_legacy_primary_second_primary_refused) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    HRESULT hr; IDirectDrawSurface7 *p = mk(dd, 0, 0, NULL, DDSCAPS_PRIMARYSURFACE, &hr);
    if (!p) {
        DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.dwFlags = DDSD_CAPS; d.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
        hr = IDirectDraw7_CreateSurface(dd, &d, &p, NULL);
    }
    if (FAILED(hr) || !p) { t_skip("primary surface unavailable: %s", d3dh_hr_name(hr)); teardown(dd, w); return; }
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.dwFlags = DDSD_CAPS; d.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    IDirectDrawSurface7 *p2 = NULL;
    hr = IDirectDraw7_CreateSurface(dd, &d, &p2, NULL);
    EXPECT_HR(hr, DDERR_PRIMARYSURFACEALREADYEXISTS);
    if (p2) IDirectDrawSurface7_Release(p2);
    IDirectDrawSurface7_Release(p);
    teardown(dd, w);
}
TEST(ddraw_legacy_primary_flip_chain_needs_exclusive) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT; d.dwBackBufferCount = 1;
    d.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    IDirectDrawSurface7 *p = NULL;
    HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &p, NULL);
    CHECKF(FAILED(hr), "flip chain in NORMAL cooperative mode -> %s", d3dh_hr_name(hr));
    t_note("primary flip chain without exclusive mode -> %s", d3dh_hr_name(hr));
    if (p) IDirectDrawSurface7_Release(p);
    teardown(dd, w);
}
TEST(ddraw_legacy_primary_gdi_surface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.dwFlags = DDSD_CAPS; d.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    IDirectDrawSurface7 *p = NULL;
    HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &p, NULL);
    if (FAILED(hr) || !p) { t_skip("primary surface unavailable: %s", d3dh_hr_name(hr)); teardown(dd, w); return; }
    IDirectDrawSurface7 *g = NULL;
    hr = IDirectDraw7_GetGDISurface(dd, &g);
    EXPECT_HR(hr, DD_OK);
    if (g) { t_note("GDI surface is %s the primary", g == p ? "the same as" : "different from"); IDirectDrawSurface7_Release(g); }
    hr = IDirectDraw7_FlipToGDISurface(dd);
    t_note("FlipToGDISurface with a lone primary -> %s", d3dh_hr_name(hr));
    IDirectDrawSurface7_Release(p);
    teardown(dd, w);
}
TEST(ddraw_legacy_primary_gamma_control_query_only) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.dwFlags = DDSD_CAPS; d.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    IDirectDrawSurface7 *p = NULL;
    HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &p, NULL);
    if (FAILED(hr) || !p) { t_skip("primary surface unavailable: %s", d3dh_hr_name(hr)); teardown(dd, w); return; }
    IDirectDrawGammaControl *gc = NULL;
    hr = IDirectDrawSurface7_QueryInterface(p, &IID_IDirectDrawGammaControl, (void **)&gc);
    t_note("QI(IDirectDrawGammaControl) on primary -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && gc) {
        DDGAMMARAMP ramp; memset(&ramp, 0, sizeof ramp);
        HRESULT g = IDirectDrawGammaControl_GetGammaRamp(gc, 0, &ramp);   /* read only: never SetGammaRamp */
        t_note("GetGammaRamp -> %s", d3dh_hr_name(g));
        if (SUCCEEDED(g)) {
            int mono = 1;
            for (int i = 1; i < 256; i++) if (ramp.red[i] < ramp.red[i - 1]) mono = 0;
            CHECK(mono);
        }
        IDirectDrawGammaControl_Release(gc);
    }
    void *cc = NULL;
    hr = IDirectDrawSurface7_QueryInterface(p, &IID_IDirectDrawColorControl, &cc);
    t_note("QI(IDirectDrawColorControl) on primary -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && cc) IUnknown_Release((IUnknown *)cc);
    IDirectDrawSurface7_Release(p);
    teardown(dd, w);
}
TEST(ddraw_legacy_dd1_surface_create_and_lock) {
    HWND w; IDirectDraw7 *dd7 = setup(&w); if (!dd7) return;
    IDirectDraw *dd = NULL;
    if (FAILED(IDirectDraw7_QueryInterface(dd7, &IID_IDirectDraw, (void **)&dd)) || !dd) { teardown(dd7, w); return; }
    DDSURFACEDESC d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.dwWidth = 12; d.dwHeight = 6; d.ddsCaps.dwCaps = SYSPLAIN;
    IDirectDrawSurface *s = NULL;
    HRESULT hr = IDirectDraw_CreateSurface(dd, &d, &s, NULL);
    EXPECT_HR(hr, DD_OK);
    if (s) {
        DDSURFACEDESC l; memset(&l, 0, sizeof l); l.dwSize = sizeof l;
        EXPECT_HR(IDirectDrawSurface_Lock(s, NULL, &l, DDLOCK_WAIT, NULL), DD_OK);
        EXPECT_EQ(l.dwWidth, 12);
        CHECK(l.lpSurface != NULL);
        EXPECT_HR(IDirectDrawSurface_Unlock(s, NULL), DD_OK);
        IDirectDrawSurface_Release(s);
    }
    d.dwSize = 108;
    hr = IDirectDraw_CreateSurface(dd, &d, &s, NULL);
    CHECKF(FAILED(hr), "legacy 108-byte DDSURFACEDESC on x64 -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && s) IDirectDrawSurface_Release(s);
    IDirectDraw_Release(dd);
    teardown(dd7, w);
}
TEST(ddraw_legacy_dd7_duplicate_surface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = mk(dd, 4, 4, &pf, SYSPLAIN, NULL);
    if (!s) { t_skip("no surface"); teardown(dd, w); return; }
    fill32(s, 0x00445566);
    IDirectDrawSurface7 *dup = NULL;
    HRESULT hr = IDirectDraw7_DuplicateSurface(dd, s, &dup);
    t_note("DuplicateSurface -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && dup) {
        CHECK(dup != s);
        EXPECT_EQ(px32(dup, 1, 1) & 0xffffff, 0x445566);   /* duplicates share the pixel memory */
        fill32(s, 0x00112233);
        EXPECT_EQ(px32(dup, 1, 1) & 0xffffff, 0x112233);
        IDirectDrawSurface7_Release(dup);
    }
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_dd7_restore_all_surfaces) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = mk(dd, 4, 4, NULL, SYSPLAIN, NULL);
    EXPECT_HR(IDirectDraw7_RestoreAllSurfaces(dd), DD_OK);
    if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_dd7_evaluate_mode_flags) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DWORD timeout = 0;
    HRESULT hr = IDirectDraw7_EvaluateMode(dd, 0x80, &timeout);
    t_note("EvaluateMode(0x80) -> %s", d3dh_hr_name(hr));
    teardown(dd, w);
}
