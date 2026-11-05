#include "dl.h"

IDirectDraw *dl_create1(void) {
    dl_create_t f = DL_PROC(dl_create_t, "DirectDrawCreate");
    IDirectDraw *dd = NULL;
    if (!f) { t_skip("DirectDrawCreate not exported"); return NULL; }
    HRESULT hr = f(NULL, &dd, NULL);
    if (FAILED(hr) || !dd) { t_skip("DirectDrawCreate -> %s (no DirectDraw device)", d3dh_hr_name(hr)); return NULL; }
    return dd;
}
IDirectDraw7 *dl_create7(void) {
    dl_createex_t f = DL_PROC(dl_createex_t, "DirectDrawCreateEx");
    IDirectDraw7 *dd = NULL;
    if (!f) { t_skip("DirectDrawCreateEx not exported"); return NULL; }
    HRESULT hr = f(NULL, (LPVOID *)&dd, &IID_IDirectDraw7, NULL);
    if (FAILED(hr) || !dd) { t_skip("DirectDrawCreateEx -> %s (no DirectDraw device)", d3dh_hr_name(hr)); return NULL; }
    return dd;
}
HWND dl_window(void) { return d3dh_create_hidden_window(); }
void dl_window_free(HWND h) { d3dh_destroy_window(h); }
HRESULT dl_set_normal(IDirectDraw7 *dd, HWND w) { return IDirectDraw7_SetCooperativeLevel(dd, w, DDSCL_NORMAL); }

void dl_pf_xrgb32(DDPIXELFORMAT *pf) {
    memset(pf, 0, sizeof *pf); pf->dwSize = sizeof *pf; pf->dwFlags = DDPF_RGB; pf->dwRGBBitCount = 32;
    pf->dwRBitMask = 0xff0000; pf->dwGBitMask = 0xff00; pf->dwBBitMask = 0xff;
}
void dl_pf_rgb565(DDPIXELFORMAT *pf) {
    memset(pf, 0, sizeof *pf); pf->dwSize = sizeof *pf; pf->dwFlags = DDPF_RGB; pf->dwRGBBitCount = 16;
    pf->dwRBitMask = 0xf800; pf->dwGBitMask = 0x07e0; pf->dwBBitMask = 0x001f;
}
void dl_pf_pal8(DDPIXELFORMAT *pf) {
    memset(pf, 0, sizeof *pf); pf->dwSize = sizeof *pf; pf->dwFlags = DDPF_PALETTEINDEXED8 | DDPF_RGB; pf->dwRGBBitCount = 8;
}
IDirectDrawSurface7 *dl_offscreen7(IDirectDraw7 *dd, DWORD w, DWORD h, DWORD caps_extra, HRESULT *hr_out) {
    DDSURFACEDESC2 d; IDirectDrawSurface7 *s = NULL;
    memset(&d, 0, sizeof d); d.dwSize = sizeof d;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    d.dwWidth = w; d.dwHeight = h; d.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | caps_extra;
    HRESULT hr = IDirectDraw7_CreateSurface(dd, &d, &s, NULL);
    if (hr_out) *hr_out = hr;
    return SUCCEEDED(hr) ? s : NULL;
}
