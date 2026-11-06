/* Shared helpers for the d3d11_interfaces_video area. */
#ifndef IFV_H
#define IFV_H
#include "d3dh.h"
#include <d3d11_4.h>
#include <d3d11on12.h>
#include <dxgi1_5.h>

#define T(n) TEST(d3d11_interfaces_video_##n)

/* IIDs missing from the mingw headers (used with raw QueryInterface only). */
extern const GUID ifv_IID_VideoDevice2;
extern const GUID ifv_IID_VideoContext3;
extern const GUID ifv_IID_Random;       /* a GUID no D3D object implements */

/* Create a device or skip the calling test (callers must `return` after a 0). */
int  ifv_dev(ID3D11Device **d, ID3D11DeviceContext **c, D3D_FEATURE_LEVEL *fl, UINT flags);
/* Device pinned to one feature level (WARP preferred); 0 when that level is unavailable (no skip is issued). */
int  ifv_dev_at(ID3D11Device **d, ID3D11DeviceContext **c, D3D_FEATURE_LEVEL want, UINT flags);

#define IFV_DEV(d, c) \
    ID3D11Device *d = NULL; ID3D11DeviceContext *c = NULL; D3D_FEATURE_LEVEL fl_ = 0; \
    if (!ifv_dev(&d, &c, &fl_, 0)) return

/* QueryInterface helper: returns the HRESULT, checks that *out is NULL on failure, releases the result.
 * If ident_out is non-NULL it receives the IUnknown identity pointer of the result (no reference kept). */
HRESULT ifv_qi(void *unk, const GUID *iid, void **ident_out);
/* IUnknown identity of an object (no reference kept). */
void *ifv_identity(void *unk);

/* Table-driven QI check. want: 1 = must succeed, 0 = must be E_NOINTERFACE, -1 = record with t_note. */
struct ifv_qi_ent { const GUID *iid; const char *name; int want; };
void ifv_check_qi_table(void *obj, const struct ifv_qi_ent *tab, unsigned n, const char *what);

/* Video bundle */
struct ifv_video {
    ID3D11Device *dev; ID3D11DeviceContext *ctx; ID3D11VideoDevice *vd; ID3D11VideoContext *vc;
    D3D_FEATURE_LEVEL fl;
};
int  ifv_video_open(struct ifv_video *v);          /* 0 (after t_skip) when no device or no video interfaces */
void ifv_video_close(struct ifv_video *v);
#define IFV_VIDEO(v) struct ifv_video v; if (!ifv_video_open(&v)) return

/* Fills a content desc (progressive 30/1 -> 30/1, usage PLAYBACK_NORMAL). */
void ifv_content_desc(D3D11_VIDEO_PROCESSOR_CONTENT_DESC *d, UINT iw, UINT ih, UINT ow, UINT oh);
/* Enumerator for a w x h progressive stream; returns HRESULT. */
HRESULT ifv_make_enum(struct ifv_video *v, UINT w, UINT h, ID3D11VideoProcessorEnumerator **e);
/* Enumerator + processor (rate conversion index 0); returns 0 and t_skips when unsupported. */
int  ifv_make_vp(struct ifv_video *v, UINT w, UINT h, ID3D11VideoProcessorEnumerator **e, ID3D11VideoProcessor **p);
#define IFV_VP(v, e, p, w, h) \
    IFV_VIDEO(v); ID3D11VideoProcessorEnumerator *e = NULL; ID3D11VideoProcessor *p = NULL; \
    if (!ifv_make_vp(&v, w, h, &e, &p)) { ifv_video_close(&v); return; }
#define IFV_VP_CLOSE(v, e, p) do { SAFE_RELEASE(p); SAFE_RELEASE(e); ifv_video_close(&v); } while (0)
/* 1 when dev runs on the Microsoft Basic Render Driver (vendor 0x1414): the reference VM's only adapter, WARP-backed, no video decoder/processor. */
int  ifv_is_brd(ID3D11Device *dev);
/* Pins of values the runtime leaves to the adapter apply to the reference adapter only (the last device made by ifv_dev/ifv_dev_at). */
int  ifv_brd(void);
#define BRD_EQ(a, b) do { if (ifv_brd()) EXPECT_EQ(a, b); } while (0)
#define BRD_HR(a, b) do { if (ifv_brd()) EXPECT_HR(a, b); } while (0)
#endif
