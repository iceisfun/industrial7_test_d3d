/* IDirectDrawClipper and IDirectDrawPalette behaviour. */
#include "dl.h"

static IDirectDraw7 *setup(HWND *w) {
    IDirectDraw7 *dd = dl_create7();
    if (!dd) return NULL;
    *w = dl_window();
    HRESULT hr = IDirectDraw7_SetCooperativeLevel(dd, *w, DDSCL_NORMAL);
    if (FAILED(hr)) { t_skip("SetCooperativeLevel(NORMAL) -> %s", d3dh_hr_name(hr)); IDirectDraw7_Release(dd); dl_window_free(*w); return NULL; }
    return dd;
}
static void teardown(IDirectDraw7 *dd, HWND w) { IDirectDraw7_Release(dd); dl_window_free(w); }

static LPDIRECTDRAWCLIPPER mkclip(void) {
    dl_createclipper_t f = DL_PROC(dl_createclipper_t, "DirectDrawCreateClipper");
    LPDIRECTDRAWCLIPPER c = NULL;
    if (!f) { t_skip("DirectDrawCreateClipper missing"); return NULL; }
    if (FAILED(f(0, &c, NULL)) || !c) { t_skip("cannot create clipper"); return NULL; }
    return c;
}

/* A clip list with n rectangles: RGNDATAHEADER (32 bytes) followed by the RECTs. */
#define RGN_BYTES(n) (sizeof(RGNDATAHEADER) + (n) * sizeof(RECT))
static RGNDATA *mkrgn(int n, const RECT *rects, BYTE *storage) {
    RGNDATA *rd = (RGNDATA *)storage;
    memset(rd, 0, RGN_BYTES(n));
    rd->rdh.dwSize = sizeof(RGNDATAHEADER);
    rd->rdh.iType = RDH_RECTANGLES;
    rd->rdh.nCount = n;
    rd->rdh.nRgnSize = n * sizeof(RECT);
    if (n) { rd->rdh.rcBound = rects[0]; memcpy(rd->Buffer, rects, n * sizeof(RECT)); }
    for (int i = 1; i < n; i++) UnionRect(&rd->rdh.rcBound, &rd->rdh.rcBound, &rects[i]);
    return rd;
}

/* ---------------- clipper: HWND ---------------- */
TEST(ddraw_legacy_clipper_gethwnd_default_null) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    HWND h = (HWND)(ULONG_PTR)0x1234;
    EXPECT_HR(IDirectDrawClipper_GetHWnd(c, &h), DD_OK);
    EXPECT_EQ(h, 0);
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_sethwnd_roundtrip) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    HWND w = dl_window(), h = NULL;
    EXPECT_HR(IDirectDrawClipper_SetHWnd(c, 0, w), DD_OK);
    EXPECT_HR(IDirectDrawClipper_GetHWnd(c, &h), DD_OK);
    EXPECT_EQ(h, w);
    EXPECT_HR(IDirectDrawClipper_SetHWnd(c, 0, NULL), DD_OK);       /* NULL detaches */
    EXPECT_HR(IDirectDrawClipper_GetHWnd(c, &h), DD_OK);
    EXPECT_EQ(h, 0);
    dl_window_free(w);
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_sethwnd_bad_args) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    HWND w = dl_window();
    DWORD bad[] = { 1, 2, 0x10, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < 5; i++) {
        HRESULT hr = IDirectDrawClipper_SetHWnd(c, bad[i], w);
        CHECKF(hr == DDERR_INVALIDPARAMS, "flags 0x%lx -> %s", bad[i], d3dh_hr_name(hr));
    }
    HWND dead = dl_window();
    DestroyWindow(dead);
    EXPECT_HR(IDirectDrawClipper_SetHWnd(c, 0, dead), DDERR_INVALIDPARAMS);   /* must be a live window */
    EXPECT_HR(IDirectDrawClipper_SetHWnd(c, 0, (HWND)(ULONG_PTR)0x3), DDERR_INVALIDPARAMS);
    HWND h = (HWND)(ULONG_PTR)1;
    EXPECT_HR(IDirectDrawClipper_GetHWnd(c, &h), DD_OK);
    EXPECT_EQ(h, 0);                                                          /* failed calls change nothing */
    dl_window_free(w);
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_sethwnd_does_not_ref_window) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    HWND w = dl_window();
    EXPECT_HR(IDirectDrawClipper_SetHWnd(c, 0, w), DD_OK);
    dl_window_free(w);                                                        /* window destroyed under the clipper */
    HWND h = NULL;
    EXPECT_HR(IDirectDrawClipper_GetHWnd(c, &h), DD_OK);
    EXPECT_EQ(h, w);                                                          /* the stale handle is returned as stored */
    EXPECT_RET(IDirectDrawClipper_Release(c), 0);
}

/* ---------------- clipper: clip list ---------------- */
TEST(ddraw_legacy_clipper_getcliplist_empty) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    DWORD sz = 0;
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, NULL, &sz), DDERR_NOCLIPLIST);
    BYTE buf[256]; sz = sizeof buf;
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, (RGNDATA *)buf, &sz), DDERR_NOCLIPLIST);
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_setcliplist_roundtrip) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    RECT r[3] = { { 0, 0, 10, 10 }, { 20, 5, 30, 15 }, { 4, 40, 8, 44 } };
    BYTE store[512], out[512];
    RGNDATA *rd = mkrgn(3, r, store);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, rd, 0), DD_OK);
    memset(out, 0xCD, sizeof out);
    DWORD sz = (DWORD)RGN_BYTES(3);
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, (RGNDATA *)out, &sz), DD_OK);
    EXPECT_EQ(sz, RGN_BYTES(3));
    CHECK(!memcmp(out, store, RGN_BYTES(3)));
    CHECK(out[RGN_BYTES(3)] == 0xCD);                                         /* nothing written past the list */
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_getcliplist_size_protocol) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    RECT r[2] = { { 1, 1, 5, 5 }, { 7, 7, 9, 9 } };
    BYTE store[256], out[256];
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, mkrgn(2, r, store), 0), DD_OK);
    const DWORD need = (DWORD)RGN_BYTES(2);
    DWORD sz = 0;
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, NULL, &sz), DD_OK);     /* size query */
    EXPECT_EQ(sz, need);
    DWORD small[] = { 0, 1, 31, need - 1 };
    for (unsigned i = 0; i < 4; i++) {
        memset(out, 0xCD, sizeof out);
        sz = small[i];
        HRESULT hr = IDirectDrawClipper_GetClipList(c, NULL, (RGNDATA *)out, &sz);
        CHECKF(hr == DDERR_REGIONTOOSMALL, "buffer %lu -> %s", small[i], d3dh_hr_name(hr));
        EXPECT_EQ(sz, need);                                                  /* required size reported */
        CHECK(out[0] == 0xCD && out[need - 1] == 0xCD);                       /* buffer untouched */
    }
    sz = need + 100;
    memset(out, 0xCD, sizeof out);
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, (RGNDATA *)out, &sz), DD_OK);
    CHECK(!memcmp(out, store, need));
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_setcliplist_validation) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    RECT r = { 0, 0, 4, 4 };
    BYTE store[256];
    RGNDATA *rd = mkrgn(1, &r, store);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, rd, 1), DDERR_INVALIDPARAMS);   /* flags must be 0 */
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, rd, 0x80000000), DDERR_INVALIDPARAMS);
    rd->rdh.nCount = 0;
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, rd, 0), DDERR_INVALIDCLIPLIST); /* empty list */
    rd->rdh.nCount = 1;
    rd->rdh.dwSize = 16;
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, rd, 0), DDERR_INVALIDCLIPLIST); /* header too small */
    rd->rdh.dwSize = sizeof(RGNDATAHEADER);
    rd->rdh.iType = 2;
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, rd, 0), DDERR_INVALIDCLIPLIST); /* only RDH_RECTANGLES */
    rd->rdh.iType = 0;
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, rd, 0), DDERR_INVALIDCLIPLIST);
    DWORD sz = 0;
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, NULL, &sz), DDERR_NOCLIPLIST);   /* nothing was stored */
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_setcliplist_null_clears) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    RECT r = { 0, 0, 4, 4 };
    BYTE store[256];
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, mkrgn(1, &r, store), 0), DD_OK);
    DWORD sz = 0;
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, NULL, &sz), DD_OK);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, NULL, 0), DD_OK);
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, NULL, &sz), DDERR_NOCLIPLIST);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, NULL, 0), DD_OK);               /* clearing twice is fine */
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_setcliplist_replace) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    RECT a[1] = { { 0, 0, 4, 4 } }, b[3] = { { 0, 0, 1, 1 }, { 2, 2, 3, 3 }, { 5, 5, 6, 6 } };
    BYTE sa[256], sb[256], out[256];
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, mkrgn(1, a, sa), 0), DD_OK);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, mkrgn(3, b, sb), 0), DD_OK);
    DWORD sz = 0;
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, NULL, &sz), DD_OK);
    EXPECT_EQ(sz, RGN_BYTES(3));
    sz = (DWORD)RGN_BYTES(3);
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, (RGNDATA *)out, &sz), DD_OK);
    CHECK(!memcmp(out, sb, RGN_BYTES(3)));
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_setcliplist_copies_data) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    RECT r[1] = { { 3, 3, 9, 9 } };
    BYTE store[256], out[256];
    RGNDATA *rd = mkrgn(1, r, store);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, rd, 0), DD_OK);
    memset(store, 0, sizeof store);                                            /* caller's buffer no longer matters */
    DWORD sz = (DWORD)RGN_BYTES(1);
    EXPECT_HR(IDirectDrawClipper_GetClipList(c, NULL, (RGNDATA *)out, &sz), DD_OK);
    CHECK(((RGNDATA *)out)->rdh.nCount == 1);
    CHECK(((RECT *)((RGNDATA *)out)->Buffer)->left == 3);
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_hwnd_excludes_cliplist) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    HWND w = dl_window();
    RECT r = { 0, 0, 4, 4 };
    BYTE store[256];
    EXPECT_HR(IDirectDrawClipper_SetHWnd(c, 0, w), DD_OK);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, mkrgn(1, &r, store), 0), DDERR_CLIPPERISUSINGHWND);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, NULL, 0), DDERR_CLIPPERISUSINGHWND);
    EXPECT_HR(IDirectDrawClipper_SetHWnd(c, 0, NULL), DD_OK);
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, mkrgn(1, &r, store), 0), DD_OK);
    dl_window_free(w);
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_hwnd_cliplist_query) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    HWND w = dl_window();
    EXPECT_HR(IDirectDrawClipper_SetHWnd(c, 0, w), DD_OK);
    DWORD sz = 0;
    HRESULT hr = IDirectDrawClipper_GetClipList(c, NULL, NULL, &sz);
    t_note("GetClipList(size query) for a hidden window -> %s, %lu bytes", d3dh_hr_name(hr), sz);
    if (SUCCEEDED(hr) && sz >= sizeof(RGNDATAHEADER)) {
        BYTE *buf = calloc(1, sz);
        DWORD s2 = sz;
        HRESULT h2 = IDirectDrawClipper_GetClipList(c, NULL, (RGNDATA *)buf, &s2);
        t_note("GetClipList(data) -> %s", d3dh_hr_name(h2));
        if (SUCCEEDED(h2)) {
            CHECK(((RGNDATA *)buf)->rdh.iType == RDH_RECTANGLES);
            CHECK(((RGNDATA *)buf)->rdh.dwSize == sizeof(RGNDATAHEADER));
        }
        free(buf);
    }
    dl_window_free(w);
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_iscliplistchanged) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    BOOL ch = 0x77;
    EXPECT_HR(IDirectDrawClipper_IsClipListChanged(c, &ch), DD_OK);
    EXPECT_EQ(ch, 1);                                                          /* always reported as changed */
    ch = 0x77;
    EXPECT_HR(IDirectDrawClipper_IsClipListChanged(c, &ch), DD_OK);
    EXPECT_EQ(ch, 1);
    IDirectDrawClipper_Release(c);
}
TEST(ddraw_legacy_clipper_qi) {
    LPDIRECTDRAWCLIPPER c = mkclip(); if (!c) return;
    IUnknown *u = NULL; void *p = (void *)0x42;
    EXPECT_HR(IDirectDrawClipper_QueryInterface(c, &IID_IUnknown, (void **)&u), DD_OK);
    if (u) { CHECK(u == (IUnknown *)c); EXPECT_RET(IUnknown_Release(u), 1); }
    EXPECT_HR(IDirectDrawClipper_QueryInterface(c, &IID_IDirectDrawClipper, (void **)&u), DD_OK);
    if (u) { CHECK(u == (IUnknown *)c); IUnknown_Release(u); }
    EXPECT_HR(IDirectDrawClipper_QueryInterface(c, &IID_IDirectDraw, &p), E_NOINTERFACE);
    EXPECT_HR(IDirectDrawClipper_QueryInterface(c, &IID_IDirectDrawSurface7, &p), E_NOINTERFACE);
    EXPECT_HR(IDirectDrawClipper_QueryInterface(c, &IID_IDirectDrawPalette, &p), E_NOINTERFACE);
    EXPECT_RET(IDirectDrawClipper_Release(c), 0);
}
TEST(ddraw_legacy_clipper_independent_state) {
    LPDIRECTDRAWCLIPPER a = mkclip(), b = mkclip();
    if (a && b) {
        RECT r = { 0, 0, 4, 4 }; BYTE store[256];
        EXPECT_HR(IDirectDrawClipper_SetClipList(a, mkrgn(1, &r, store), 0), DD_OK);
        DWORD sz = 0;
        EXPECT_HR(IDirectDrawClipper_GetClipList(b, NULL, NULL, &sz), DDERR_NOCLIPLIST);
    }
    if (a) IDirectDrawClipper_Release(a);
    if (b) IDirectDrawClipper_Release(b);
}

/* ---------------- clippers created from a DirectDraw object ---------------- */
TEST(ddraw_legacy_clipper_from_dd_basics) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    LPDIRECTDRAWCLIPPER c = NULL;
    EXPECT_HR(IDirectDraw7_CreateClipper(dd, 0, &c, NULL), DD_OK);
    if (c) {
        EXPECT_HR(IDirectDrawClipper_Initialize(c, NULL, 0), DDERR_ALREADYINITIALIZED);
        IDirectDrawClipper_Release(c);
    }
    c = (LPDIRECTDRAWCLIPPER)(ULONG_PTR)0x55;
    EXPECT_HR(IDirectDraw7_CreateClipper(dd, 1, &c, NULL), DDERR_INVALIDPARAMS);
    EXPECT_HR(IDirectDraw7_CreateClipper(dd, 0, &c, (IUnknown *)(ULONG_PTR)0x10), CLASS_E_NOAGGREGATION);
    teardown(dd, w);
}
TEST(ddraw_legacy_clipper_from_dd7_holds_dd_reference) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    EXPECT_EQ(d3dh_refcount(dd), 1);
    LPDIRECTDRAWCLIPPER c = NULL;
    if (SUCCEEDED(IDirectDraw7_CreateClipper(dd, 0, &c, NULL)) && c) {
        EXPECT_EQ(d3dh_refcount(dd), 2);                    /* clippers of DD4/DD7 objects pin their creator */
        IDirectDrawClipper_Release(c);
        EXPECT_EQ(d3dh_refcount(dd), 1);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_clipper_from_dd1_no_dd_reference) {
    HWND w; IDirectDraw7 *dd7 = setup(&w); if (!dd7) return;
    IDirectDraw *dd = NULL;
    if (FAILED(IDirectDraw7_QueryInterface(dd7, &IID_IDirectDraw, (void **)&dd)) || !dd) { teardown(dd7, w); return; }
    ULONG before = d3dh_refcount(dd);
    LPDIRECTDRAWCLIPPER c = NULL;
    if (SUCCEEDED(IDirectDraw_CreateClipper(dd, 0, &c, NULL)) && c) {
        EXPECT_EQ(d3dh_refcount(dd), before);
        IDirectDrawClipper_Release(c);
    }
    IDirectDraw_Release(dd);
    teardown(dd7, w);
}
TEST(ddraw_legacy_clipper_attach_to_surface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = dl_offscreen7(dd, 16, 16, DDSCAPS_SYSTEMMEMORY, NULL);
    LPDIRECTDRAWCLIPPER c = NULL, got = NULL;
    if (s && SUCCEEDED(IDirectDraw7_CreateClipper(dd, 0, &c, NULL)) && c) {
        EXPECT_HR(IDirectDrawSurface7_GetClipper(s, &got), DDERR_NOCLIPPERATTACHED);
        ULONG base = d3dh_refcount(c);
        EXPECT_HR(IDirectDrawSurface7_SetClipper(s, c), DD_OK);
        EXPECT_EQ(d3dh_refcount(c), base + 1);                   /* the surface references the clipper */
        EXPECT_HR(IDirectDrawSurface7_GetClipper(s, &got), DD_OK);
        CHECK(got == c);
        if (got) IDirectDrawClipper_Release(got);
        EXPECT_HR(IDirectDrawSurface7_SetClipper(s, NULL), DD_OK);
        EXPECT_EQ(d3dh_refcount(c), base);
        EXPECT_HR(IDirectDrawSurface7_GetClipper(s, &got), DDERR_NOCLIPPERATTACHED);
    } else t_skip("no surface/clipper");
    if (c) IDirectDrawClipper_Release(c);
    if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_clipper_surface_release_detaches) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = dl_offscreen7(dd, 16, 16, DDSCAPS_SYSTEMMEMORY, NULL);
    LPDIRECTDRAWCLIPPER c = NULL;
    if (s && SUCCEEDED(IDirectDraw7_CreateClipper(dd, 0, &c, NULL)) && c) {
        ULONG base = d3dh_refcount(c);
        IDirectDrawSurface7_SetClipper(s, c);
        IDirectDrawSurface7_Release(s); s = NULL;
        EXPECT_EQ(d3dh_refcount(c), base);                       /* destroying the surface drops its clipper reference */
    } else t_skip("no surface/clipper");
    if (c) IDirectDrawClipper_Release(c);
    if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_clipper_replace_on_surface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    IDirectDrawSurface7 *s = dl_offscreen7(dd, 16, 16, DDSCAPS_SYSTEMMEMORY, NULL);
    LPDIRECTDRAWCLIPPER a = NULL, b = NULL;
    if (s && SUCCEEDED(IDirectDraw7_CreateClipper(dd, 0, &a, NULL)) && SUCCEEDED(IDirectDraw7_CreateClipper(dd, 0, &b, NULL)) && a && b) {
        ULONG ra = d3dh_refcount(a), rb = d3dh_refcount(b);
        EXPECT_HR(IDirectDrawSurface7_SetClipper(s, a), DD_OK);
        EXPECT_HR(IDirectDrawSurface7_SetClipper(s, b), DD_OK);
        EXPECT_EQ(d3dh_refcount(a), ra);                        /* old clipper released */
        EXPECT_EQ(d3dh_refcount(b), rb + 1);
        IDirectDrawSurface7_SetClipper(s, NULL);
    } else t_skip("no surface/clippers");
    if (a) IDirectDrawClipper_Release(a);
    if (b) IDirectDrawClipper_Release(b);
    if (s) IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_clipper_blt_with_cliplist) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT; d.dwWidth = d.dwHeight = 8;
    d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY; d.ddpfPixelFormat = pf;
    IDirectDrawSurface7 *s = NULL;
    LPDIRECTDRAWCLIPPER c = NULL;
    if (FAILED(IDirectDraw7_CreateSurface(dd, &d, &s, NULL)) || !s || FAILED(IDirectDraw7_CreateClipper(dd, 0, &c, NULL)) || !c) {
        t_skip("no surface/clipper"); if (s) IDirectDrawSurface7_Release(s); if (c) IDirectDrawClipper_Release(c); teardown(dd, w); return;
    }
    DDBLTFX fx; memset(&fx, 0, sizeof fx); fx.dwSize = sizeof fx;
    fx.dwFillColor = 0;
    IDirectDrawSurface7_Blt(s, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    RECT r = { 2, 2, 5, 5 }; BYTE store[256];
    EXPECT_HR(IDirectDrawClipper_SetClipList(c, mkrgn(1, &r, store), 0), DD_OK);
    EXPECT_HR(IDirectDrawSurface7_SetClipper(s, c), DD_OK);
    fx.dwFillColor = 0x00ffffff;
    HRESULT hr = IDirectDrawSurface7_Blt(s, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
    t_note("colour fill through a clip list -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        DDSURFACEDESC2 l; memset(&l, 0, sizeof l); l.dwSize = sizeof l;
        if (SUCCEEDED(IDirectDrawSurface7_Lock(s, NULL, &l, DDLOCK_WAIT | DDLOCK_READONLY, NULL))) {
            int bad = 0;
            for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
                DWORD v = *(DWORD *)((BYTE *)l.lpSurface + y * l.lPitch + x * 4) & 0xffffff;
                int inside = x >= 2 && x < 5 && y >= 2 && y < 5;
                if (v != (inside ? 0xffffffu : 0u)) bad++;
            }
            IDirectDrawSurface7_Unlock(s, NULL);
            EXPECT_EQ(bad, 0);                                  /* only the clip rectangle was painted */
        }
    }
    IDirectDrawSurface7_SetClipper(s, NULL);
    IDirectDrawClipper_Release(c);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}

/* ---------------- palettes ---------------- */
static BYTE g_pal_entries[256 * 4];
static void pal_fill(void) { for (int i = 0; i < 256; i++) { g_pal_entries[i * 4] = i; g_pal_entries[i * 4 + 1] = 255 - i; g_pal_entries[i * 4 + 2] = i ^ 0x55; g_pal_entries[i * 4 + 3] = 0; } }

TEST(ddraw_legacy_palette_create_requires_cooperative_level) {
    DL_SKIP_IF_NO_DD7(dd);
    pal_fill();
    LPDIRECTDRAWPALETTE p = (LPDIRECTDRAWPALETTE)(ULONG_PTR)0x77;
    EXPECT_HR(IDirectDraw7_CreatePalette(dd, DDPCAPS_8BIT, (LPPALETTEENTRY)g_pal_entries, &p, NULL), DDERR_NOCOOPERATIVELEVELSET);
    EXPECT_EQ(p, 0);
    IDirectDraw7_Release(dd);
}
TEST(ddraw_legacy_palette_create_flag_matrix_invalid) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    struct { DWORD fl; const char *n; } bad[] = {
        { 0, "no depth" },
        { DDPCAPS_8BIT | DDPCAPS_4BIT, "8BIT|4BIT" },
        { DDPCAPS_8BIT | DDPCAPS_1BIT, "8BIT|1BIT" },
        { DDPCAPS_2BIT | DDPCAPS_1BIT, "2BIT|1BIT" },
        { DDPCAPS_8BIT | DDPCAPS_8BITENTRIES, "8BIT|8BITENTRIES" },
        { DDPCAPS_4BIT | DDPCAPS_ALLOW256, "4BIT|ALLOW256" },
        { DDPCAPS_ALLOW256, "ALLOW256 alone" },
        { DDPCAPS_8BIT | DDPCAPS_PRIMARYSURFACE, "8BIT|PRIMARYSURFACE" },
        { DDPCAPS_8BIT | DDPCAPS_PRIMARYSURFACELEFT, "8BIT|PRIMARYSURFACELEFT" },
        { DDPCAPS_8BIT | DDPCAPS_VSYNC, "8BIT|VSYNC" },
        { DDPCAPS_8BIT | 0x800, "8BIT|bit11" },
        { DDPCAPS_8BIT | 0x80000000, "8BIT|bit31" },
        { DDPCAPS_1BIT | DDPCAPS_ALPHA | DDPCAPS_8BITENTRIES, "ALPHA|8BITENTRIES" },
    };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        LPDIRECTDRAWPALETTE p = (LPDIRECTDRAWPALETTE)(ULONG_PTR)0x77;
        HRESULT hr = IDirectDraw7_CreatePalette(dd, bad[i].fl, (LPPALETTEENTRY)g_pal_entries, &p, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "%s (0x%lx) -> %s", bad[i].n, bad[i].fl, d3dh_hr_name(hr));
        EXPECT_EQ(p, 0);
        if (SUCCEEDED(hr) && p) IDirectDrawPalette_Release(p);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_create_valid_depths) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    struct { DWORD fl; DWORD n; const char *name; } ok[] = {
        { DDPCAPS_8BIT, 256, "8BIT" }, { DDPCAPS_8BIT | DDPCAPS_ALLOW256, 256, "8BIT|ALLOW256" },
        { DDPCAPS_4BIT, 16, "4BIT" }, { DDPCAPS_2BIT, 4, "2BIT" }, { DDPCAPS_1BIT, 2, "1BIT" },
        { DDPCAPS_4BIT | DDPCAPS_8BITENTRIES, 16, "4BIT|8BITENTRIES" },
    };
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        LPDIRECTDRAWPALETTE p = NULL;
        HRESULT hr = IDirectDraw7_CreatePalette(dd, ok[i].fl, (LPPALETTEENTRY)g_pal_entries, &p, NULL);
        CHECKF(hr == DD_OK && p, "%s -> %s", ok[i].name, d3dh_hr_name(hr));
        if (!p) continue;
        DWORD caps = 0;
        EXPECT_HR(IDirectDrawPalette_GetCaps(p, &caps), DD_OK);
        DWORD depth = ok[i].fl & (DDPCAPS_8BIT | DDPCAPS_4BIT | DDPCAPS_2BIT | DDPCAPS_1BIT);
        CHECKF((caps & depth) == depth, "%s caps 0x%lx", ok[i].name, caps);
        CHECKF((caps & DDPCAPS_8BITENTRIES) == (ok[i].fl & DDPCAPS_8BITENTRIES), "%s 8BITENTRIES in caps 0x%lx", ok[i].name, caps);
        EXPECT_RET(IDirectDrawPalette_Release(p), 0);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_aggregation) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    LPDIRECTDRAWPALETTE p = (LPDIRECTDRAWPALETTE)(ULONG_PTR)0x77;
    HRESULT hr = IDirectDraw7_CreatePalette(dd, DDPCAPS_8BIT, (LPPALETTEENTRY)g_pal_entries, &p, (IUnknown *)(ULONG_PTR)0x10);
    CHECKF(FAILED(hr), "outer unknown -> %s", d3dh_hr_name(hr));
    t_note("CreatePalette with pUnkOuter -> %s", d3dh_hr_name(hr));
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_entries_roundtrip) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    LPDIRECTDRAWPALETTE p = NULL;
    if (FAILED(IDirectDraw7_CreatePalette(dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, (LPPALETTEENTRY)g_pal_entries, &p, NULL)) || !p) { t_skip("no palette"); teardown(dd, w); return; }
    BYTE out[256 * 4]; memset(out, 0xCD, sizeof out);
    EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 0, 256, (LPPALETTEENTRY)out), DD_OK);
    CHECK(!memcmp(out, g_pal_entries, sizeof out));
    memset(out, 0xCD, sizeof out);
    EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 10, 4, (LPPALETTEENTRY)out), DD_OK);
    CHECK(!memcmp(out, g_pal_entries + 40, 16));
    CHECK(out[16] == 0xCD);
    BYTE patch[8 * 4]; for (int i = 0; i < 8 * 4; i++) patch[i] = 0xA0 + i;
    EXPECT_HR(IDirectDrawPalette_SetEntries(p, 0, 100, 8, (LPPALETTEENTRY)patch), DD_OK);
    EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 99, 10, (LPPALETTEENTRY)out), DD_OK);
    CHECK(!memcmp(out, g_pal_entries + 99 * 4, 4));
    CHECK(!memcmp(out + 4, patch, 32));
    CHECK(!memcmp(out + 36, g_pal_entries + 108 * 4, 4));
    EXPECT_HR(IDirectDrawPalette_SetEntries(p, 0, 255, 1, (LPPALETTEENTRY)patch), DD_OK);   /* last entry */
    EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 255, 1, (LPPALETTEENTRY)out), DD_OK);
    CHECK(!memcmp(out, patch, 4));
    IDirectDrawPalette_Release(p);
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_entries_bounds) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    LPDIRECTDRAWPALETTE p = NULL;
    if (FAILED(IDirectDraw7_CreatePalette(dd, DDPCAPS_8BIT, (LPPALETTEENTRY)g_pal_entries, &p, NULL)) || !p) { t_skip("no palette"); teardown(dd, w); return; }
    BYTE buf[300 * 4];
    struct { DWORD fl, start, n; } bad[] = {
        { 0, 0, 0 }, { 0, 0, 257 }, { 0, 256, 1 }, { 0, 255, 2 }, { 0, 250, 10 }, { 0, 1000, 1 }, { 0, 1, 256 }, { 1, 0, 1 }, { 0x80000000, 0, 1 },
    };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        memset(buf, 0xCD, sizeof buf);
        HRESULT hr = IDirectDrawPalette_GetEntries(p, bad[i].fl, bad[i].start, bad[i].n, (LPPALETTEENTRY)buf);
        CHECKF(hr == DDERR_INVALIDPARAMS, "GetEntries(fl=%lx,start=%lu,n=%lu) -> %s", bad[i].fl, bad[i].start, bad[i].n, d3dh_hr_name(hr));
        CHECK(buf[0] == 0xCD);
        hr = IDirectDrawPalette_SetEntries(p, bad[i].fl, bad[i].start, bad[i].n, (LPPALETTEENTRY)buf);
        CHECKF(hr == DDERR_INVALIDPARAMS, "SetEntries(fl=%lx,start=%lu,n=%lu) -> %s", bad[i].fl, bad[i].start, bad[i].n, d3dh_hr_name(hr));
    }
    BYTE out[256 * 4];
    EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 0, 256, (LPPALETTEENTRY)out), DD_OK);
    CHECK(!memcmp(out, g_pal_entries, sizeof out));                              /* rejected SetEntries changed nothing */
    IDirectDrawPalette_Release(p);
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_small_depth_bounds) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    struct { DWORD fl, n; } c[] = { { DDPCAPS_1BIT, 2 }, { DDPCAPS_2BIT, 4 }, { DDPCAPS_4BIT, 16 } };
    for (unsigned i = 0; i < 3; i++) {
        LPDIRECTDRAWPALETTE p = NULL;
        if (FAILED(IDirectDraw7_CreatePalette(dd, c[i].fl, (LPPALETTEENTRY)g_pal_entries, &p, NULL)) || !p) continue;
        BYTE buf[64 * 4];
        EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 0, c[i].n, (LPPALETTEENTRY)buf), DD_OK);
        EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 0, c[i].n + 1, (LPPALETTEENTRY)buf), DDERR_INVALIDPARAMS);
        EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, c[i].n, 1, (LPPALETTEENTRY)buf), DDERR_INVALIDPARAMS);
        EXPECT_HR(IDirectDrawPalette_SetEntries(p, 0, c[i].n - 1, 1, (LPPALETTEENTRY)buf), DD_OK);
        EXPECT_HR(IDirectDrawPalette_SetEntries(p, 0, c[i].n - 1, 2, (LPPALETTEENTRY)buf), DDERR_INVALIDPARAMS);
        IDirectDrawPalette_Release(p);
    }
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_8bitentries_format) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    BYTE idx[16]; for (int i = 0; i < 16; i++) idx[i] = i * 3;
    LPDIRECTDRAWPALETTE p = NULL;
    if (FAILED(IDirectDraw7_CreatePalette(dd, DDPCAPS_4BIT | DDPCAPS_8BITENTRIES, (LPPALETTEENTRY)idx, &p, NULL)) || !p) { t_skip("no palette"); teardown(dd, w); return; }
    BYTE out[16]; memset(out, 0xCD, sizeof out);
    EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 0, 16, (LPPALETTEENTRY)out), DD_OK);
    CHECK(!memcmp(out, idx, 16));                                                /* one byte per entry */
    IDirectDrawPalette_Release(p);
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_initialize_and_qi) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    LPDIRECTDRAWPALETTE p = NULL;
    if (FAILED(IDirectDraw7_CreatePalette(dd, DDPCAPS_8BIT, (LPPALETTEENTRY)g_pal_entries, &p, NULL)) || !p) { t_skip("no palette"); teardown(dd, w); return; }
    EXPECT_HR(IDirectDrawPalette_Initialize(p, dd ? (LPDIRECTDRAW)NULL : NULL, 0, (LPPALETTEENTRY)g_pal_entries), DDERR_ALREADYINITIALIZED);
    IUnknown *u = NULL; void *x = (void *)0x42;
    EXPECT_HR(IDirectDrawPalette_QueryInterface(p, &IID_IUnknown, (void **)&u), DD_OK);
    if (u) { CHECK(u == (IUnknown *)p); EXPECT_RET(IUnknown_Release(u), 1); }
    EXPECT_HR(IDirectDrawPalette_QueryInterface(p, &IID_IDirectDrawPalette, (void **)&u), DD_OK);
    if (u) IUnknown_Release(u);
    EXPECT_HR(IDirectDrawPalette_QueryInterface(p, &IID_IDirectDrawClipper, &x), E_NOINTERFACE);
    EXPECT_HR(IDirectDrawPalette_QueryInterface(p, &IID_IDirectDraw7, &x), E_NOINTERFACE);
    IDirectDrawPalette_Release(p);
    teardown(dd, w);
}

/* ---------------- palettes and surfaces ---------------- */
TEST(ddraw_legacy_palette_surface_attach) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    DDPIXELFORMAT pf; dl_pf_pal8(&pf);
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT; d.dwWidth = d.dwHeight = 8;
    d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY; d.ddpfPixelFormat = pf;
    IDirectDrawSurface7 *s = NULL; LPDIRECTDRAWPALETTE p = NULL, got = NULL;
    if (FAILED(IDirectDraw7_CreateSurface(dd, &d, &s, NULL)) || !s || FAILED(IDirectDraw7_CreatePalette(dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, (LPPALETTEENTRY)g_pal_entries, &p, NULL)) || !p) {
        t_skip("no P8 surface/palette"); if (s) IDirectDrawSurface7_Release(s); if (p) IDirectDrawPalette_Release(p); teardown(dd, w); return;
    }
    EXPECT_HR(IDirectDrawSurface7_GetPalette(s, &got), DDERR_NOPALETTEATTACHED);
    EXPECT_HR(IDirectDrawSurface7_SetPalette(s, NULL), DDERR_NOPALETTEATTACHED);      /* nothing to detach */
    ULONG base = d3dh_refcount(p);
    EXPECT_HR(IDirectDrawSurface7_SetPalette(s, p), DD_OK);
    EXPECT_EQ(d3dh_refcount(p), base + 1);
    EXPECT_HR(IDirectDrawSurface7_GetPalette(s, &got), DD_OK);
    CHECK(got == p);
    if (got) IDirectDrawPalette_Release(got);
    EXPECT_HR(IDirectDrawSurface7_SetPalette(s, NULL), DD_OK);
    EXPECT_EQ(d3dh_refcount(p), base);
    EXPECT_HR(IDirectDrawSurface7_GetPalette(s, &got), DDERR_NOPALETTEATTACHED);
    IDirectDrawPalette_Release(p);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_surface_wrong_format) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    DDPIXELFORMAT pf; dl_pf_xrgb32(&pf);
    IDirectDrawSurface7 *s = NULL; LPDIRECTDRAWPALETTE p = NULL;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT; d.dwWidth = d.dwHeight = 8;
    d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY; d.ddpfPixelFormat = pf;
    if (FAILED(IDirectDraw7_CreateSurface(dd, &d, &s, NULL)) || !s || FAILED(IDirectDraw7_CreatePalette(dd, DDPCAPS_8BIT, (LPPALETTEENTRY)g_pal_entries, &p, NULL)) || !p) {
        t_skip("no surface/palette"); if (s) IDirectDrawSurface7_Release(s); if (p) IDirectDrawPalette_Release(p); teardown(dd, w); return;
    }
    EXPECT_HR(IDirectDrawSurface7_SetPalette(s, p), DDERR_INVALIDPIXELFORMAT);       /* 8-bit palette on a 32-bit surface */
    LPDIRECTDRAWPALETTE got = NULL;
    EXPECT_HR(IDirectDrawSurface7_GetPalette(s, &got), DDERR_NOPALETTEATTACHED);
    IDirectDrawPalette_Release(p);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_mismatched_depth_surface) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    DDPIXELFORMAT pf; dl_pf_pal8(&pf);
    IDirectDrawSurface7 *s = NULL; LPDIRECTDRAWPALETTE p = NULL;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT; d.dwWidth = d.dwHeight = 8;
    d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY; d.ddpfPixelFormat = pf;
    if (FAILED(IDirectDraw7_CreateSurface(dd, &d, &s, NULL)) || !s || FAILED(IDirectDraw7_CreatePalette(dd, DDPCAPS_4BIT, (LPPALETTEENTRY)g_pal_entries, &p, NULL)) || !p) {
        t_skip("no surface/palette"); if (s) IDirectDrawSurface7_Release(s); if (p) IDirectDrawPalette_Release(p); teardown(dd, w); return;
    }
    HRESULT hr = IDirectDrawSurface7_SetPalette(s, p);
    EXPECT_HR(hr, DDERR_INVALIDPIXELFORMAT);                                         /* 4-bit palette on an 8-bit surface */
    if (SUCCEEDED(hr)) IDirectDrawSurface7_SetPalette(s, NULL);
    IDirectDrawPalette_Release(p);
    IDirectDrawSurface7_Release(s);
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_share_between_surfaces) {
    HWND w; IDirectDraw7 *dd = setup(&w); if (!dd) return;
    pal_fill();
    DDPIXELFORMAT pf; dl_pf_pal8(&pf);
    IDirectDrawSurface7 *a = NULL, *b = NULL; LPDIRECTDRAWPALETTE p = NULL;
    DDSURFACEDESC2 d; memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT; d.dwWidth = d.dwHeight = 8;
    d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY; d.ddpfPixelFormat = pf;
    if (FAILED(IDirectDraw7_CreateSurface(dd, &d, &a, NULL)) || FAILED(IDirectDraw7_CreateSurface(dd, &d, &b, NULL)) || !a || !b ||
        FAILED(IDirectDraw7_CreatePalette(dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, (LPPALETTEENTRY)g_pal_entries, &p, NULL)) || !p) {
        t_skip("no surfaces/palette");
    } else {
        ULONG base = d3dh_refcount(p);
        EXPECT_HR(IDirectDrawSurface7_SetPalette(a, p), DD_OK);
        EXPECT_HR(IDirectDrawSurface7_SetPalette(b, p), DD_OK);
        EXPECT_EQ(d3dh_refcount(p), base + 2);
        IDirectDrawSurface7_Release(a); a = NULL;
        EXPECT_EQ(d3dh_refcount(p), base + 1);                                       /* surface destruction drops its palette ref */
        IDirectDrawSurface7_Release(b); b = NULL;
        EXPECT_EQ(d3dh_refcount(p), base);
    }
    if (a) IDirectDrawSurface7_Release(a);
    if (b) IDirectDrawSurface7_Release(b);
    if (p) IDirectDrawPalette_Release(p);
    teardown(dd, w);
}
TEST(ddraw_legacy_palette_dd1_interface) {
    HWND w; IDirectDraw7 *dd7 = setup(&w); if (!dd7) return;
    IDirectDraw *dd = NULL;
    if (FAILED(IDirectDraw7_QueryInterface(dd7, &IID_IDirectDraw, (void **)&dd)) || !dd) { teardown(dd7, w); return; }
    pal_fill();
    LPDIRECTDRAWPALETTE p = NULL;
    EXPECT_HR(IDirectDraw_CreatePalette(dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, (LPPALETTEENTRY)g_pal_entries, &p, NULL), DD_OK);
    if (p) {
        BYTE out[1024];
        EXPECT_HR(IDirectDrawPalette_GetEntries(p, 0, 0, 256, (LPPALETTEENTRY)out), DD_OK);
        CHECK(!memcmp(out, g_pal_entries, sizeof out));
        IDirectDrawPalette_Release(p);
    }
    IDirectDraw_Release(dd);
    teardown(dd7, w);
}
