/* Shared fixtures for the d3d11_debug_refcount area. */
#ifndef RC_H
#define RC_H
#include "d3dh.h"
#include <d3d11_4.h>
#include <d3d11sdklayers.h>

#define HR_SDK_COMPONENT_MISSING ((HRESULT)0x887A002D)
#define HR_MORE_DATA             ((HRESULT)0x887A0003)
#define HR_NOT_FOUND             ((HRESULT)0x887A0002)
#define HR_TOO_MANY_STATES       ((HRESULT)0x887C0001)

/* a device + immediate context for one test; skips the test when WARP/REF/NULL are all unavailable */
#define RC_DEVICE(dev, ctx, flags) \
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; \
    REQUIRE(d3dh_D3D11CreateDevice()); \
    if (FAILED(d3dh_make_device11(&dev, &ctx, NULL, NULL, (flags)))) { t_skip("no D3D11 device"); return; }
#define RC_DEVICE_END(dev, ctx) do { SAFE_RELEASE(ctx); SAFE_RELEASE(dev); } while (0)

#define UNK(p) ((IUnknown *)(p))
#define RC_ADDREF(p)  (UNK(p)->lpVtbl->AddRef(UNK(p)))
#define RC_RELEASE(p) (UNK(p)->lpVtbl->Release(UNK(p)))
#define RC_QI(p, iid, out) (UNK(p)->lpVtbl->QueryInterface(UNK(p), &(iid), (void **)(out)))

/* object kinds exercised table-driven */
#define RC_KINDS(X) \
    X(K_BUF, buffer) X(K_T1D, tex1d) X(K_T2D, tex2d) X(K_T3D, tex3d) \
    X(K_SRV, srv) X(K_RTV, rtv) X(K_DSV, dsv) X(K_UAV, uav) \
    X(K_BLEND, blend) X(K_RAST, rasterizer) X(K_DSS, depthstencil) X(K_SAMP, sampler) \
    X(K_VS, vertexshader) X(K_PS, pixelshader) X(K_CS, computeshader) X(K_IL, inputlayout) \
    X(K_QUERY, query) X(K_PRED, predicate) X(K_CLINK, classlinkage) X(K_DEFCTX, deferredctx) X(K_CMDLIST, commandlist)
enum {
#define X(k, n) k,
    RC_KINDS(X)
#undef X
    K_COUNT
};
int rc_kind_is_state(int kind);   /* blend/rasterizer/depthstencil/sampler: immutable, deduplicated */
int rc_kind_is_resource(int kind);
/* create one object of the kind on dev; returns the HRESULT (S_OK, or a failure meaning "kind unavailable here") */
HRESULT rc_make(int kind, ID3D11Device *dev, ID3D11DeviceChild **out);
/* compile a tiny shader once; blob stays owned by the helper */
ID3DBlob *rc_shader_blob(const char *which);

/* counting IUnknown used with SetPrivateDataInterface */
typedef struct { IUnknown iface; volatile LONG refs; volatile LONG destroyed; } rc_obj;
rc_obj *rc_obj_new(void);       /* refs == 1 */
LONG rc_obj_refs(rc_obj *o);
void rc_obj_delete(rc_obj *o);
extern const GUID RC_GUID_A, RC_GUID_B, RC_GUID_C;
#endif
