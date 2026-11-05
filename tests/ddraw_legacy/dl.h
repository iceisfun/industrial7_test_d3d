/* ddraw_legacy: shared helpers. All ddraw.dll entry points are reached through GetProcAddress. */
#ifndef DL_H
#define DL_H
#include "d3dh.h"

/* ddraw.dll exports not declared (or not usable via typeof) in the SDK headers */
typedef HRESULT (WINAPI *dl_dllgetclassobject_t)(REFCLSID, REFIID, LPVOID *);
typedef HRESULT (WINAPI *dl_dllcanunloadnow_t)(void);
typedef BOOL (WINAPI *dl_enumcb_a)(GUID *, LPSTR, LPSTR, LPVOID);
typedef BOOL (WINAPI *dl_enumcb_exa)(GUID *, LPSTR, LPSTR, LPVOID, HMONITOR);
typedef BOOL (WINAPI *dl_enumcb_w)(GUID *, LPWSTR, LPWSTR, LPVOID);
typedef HRESULT (WINAPI *dl_enumA_t)(dl_enumcb_a, LPVOID);
typedef HRESULT (WINAPI *dl_enumW_t)(dl_enumcb_w, LPVOID);
typedef HRESULT (WINAPI *dl_enumExA_t)(dl_enumcb_exa, LPVOID, DWORD);
typedef HRESULT (WINAPI *dl_enumExW_t)(LPVOID, LPVOID, DWORD);
typedef HRESULT (WINAPI *dl_create_t)(GUID *, LPDIRECTDRAW *, IUnknown *);
typedef HRESULT (WINAPI *dl_createex_t)(GUID *, LPVOID *, REFIID, IUnknown *);
typedef HRESULT (WINAPI *dl_createclipper_t)(DWORD, LPDIRECTDRAWCLIPPER *, IUnknown *);

#define DL_PROC(type, name) ((type)t_proc_in("ddraw.dll", name))

static inline FARPROC dl_export(const char *n) { return t_proc_in("ddraw.dll", n); }

/* Create objects; on failure t_skip and return NULL. */
IDirectDraw  *dl_create1(void);
IDirectDraw7 *dl_create7(void);
/* Hidden window (WS_POPUP, never shown). */
HWND dl_window(void);
void dl_window_free(HWND h);
/* Convenience: NORMAL cooperative level on a fresh hidden window; returns hr */
HRESULT dl_set_normal(IDirectDraw7 *dd, HWND w);
/* Create a plain offscreen system-memory surface; NULL when unsupported. */
IDirectDrawSurface7 *dl_offscreen7(IDirectDraw7 *dd, DWORD w, DWORD h, DWORD caps_extra, HRESULT *hr_out);
/* 32bpp XRGB pixel format */
void dl_pf_xrgb32(DDPIXELFORMAT *pf);
void dl_pf_rgb565(DDPIXELFORMAT *pf);
void dl_pf_pal8(DDPIXELFORMAT *pf);

#define DL_SKIP_IF_NO_DD7(p) IDirectDraw7 *p = dl_create7(); if (!p) return
#define DL_SKIP_IF_NO_DD1(p) IDirectDraw *p = dl_create1(); if (!p) return

#define EXPECT_RET(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); EXPECT_EQ(v_, (want)); } while (0)
#endif
