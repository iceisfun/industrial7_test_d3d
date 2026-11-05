/* Local helpers for the d3d_on12_dxcore_misc area. Everything static: safe to include from several .c files. */
#ifndef ODM_H
#define ODM_H
#include "d3dh.h"
#include <d3d11on12.h>

#define AREA_NTSTATUS(x) ((LONG)(x))
#define ST_SUCCESS            ((LONG)0x00000000)
#define ST_UNSUCCESSFUL       ((LONG)0xC0000001)
#define ST_INVALID_PARAMETER  ((LONG)0xC000000D)
#define ST_INVALID_PARAM_1    ((LONG)0xC00000EF)
#define ST_INVALID_INFO_CLASS ((LONG)0xC0000003)
#define ST_NOT_IMPLEMENTED    ((LONG)0xC0000002)
#define ST_INVALID_HANDLE     ((LONG)0xC0000008)

#define E_PTR       ((HRESULT)0x80004003)
#define E_NOIFACE   ((HRESULT)0x80004002)
#define E_NOTIMPL_H ((HRESULT)0x80004001)
#define E_INVARG    ((HRESULT)0x80070057)
#define DXGI_INVALID_CALL ((HRESULT)0x887A0001)
#define DXGI_UNSUPPORTED  ((HRESULT)0x887A0004)
#define DXGI_REMOVED      ((HRESULT)0x887A0005)

/* ---- raw COM calling (no headers exist for dxcore in the toolchain) ---- */
#define VTBL(o)        (*(void ***)(o))
#define VCALL(o, i, T) ((T)VTBL(o)[i])
typedef ULONG   (WINAPI *odm_rel_t)(void *);
typedef ULONG   (WINAPI *odm_addref_t)(void *);
typedef HRESULT (WINAPI *odm_qi_t)(void *, REFIID, void **);
static ULONG  odm_release(void *o) { return o ? VCALL(o, 2, odm_rel_t)(o) : 0; }
static ULONG  odm_addref(void *o)  { return VCALL(o, 1, odm_addref_t)(o); }
static HRESULT odm_qi(void *o, REFIID r, void **out) { return VCALL(o, 0, odm_qi_t)(o, r, out); }
static ULONG  odm_rc(void *o) { if (!o) return 0; odm_addref(o); return odm_release(o); }
#define ODM_REL(p) do { if (p) { odm_release(p); (p) = NULL; } } while (0)

/* ---- dxcore ---- */
static const GUID IID_IDXCoreAdapterFactory_ = {0x78ee5945, 0xc36e, 0x4b13, {0xa6, 0x69, 0x00, 0x5d, 0xd1, 0x1c, 0x0f, 0x06}};
static const GUID IID_IDXCoreAdapterList_    = {0x526c7776, 0x40e9, 0x459b, {0xb7, 0x11, 0xf3, 0x2a, 0xd7, 0x6d, 0xfc, 0x28}};
static const GUID IID_IDXCoreAdapter_        = {0xf0db4c7f, 0xfe5a, 0x42a2, {0xbd, 0x62, 0xf2, 0xa6, 0xcf, 0x6f, 0xc8, 0x3e}};
static const GUID DXC_ATTR_D3D11_GRAPHICS    = {0x8c47866b, 0x7583, 0x450d, {0xf0, 0xf0, 0x6b, 0xad, 0xa8, 0x95, 0xaf, 0x4b}};
static const GUID DXC_ATTR_D3D12_GRAPHICS    = {0x0c9ece4d, 0x2f6e, 0x4f01, {0x8c, 0x96, 0xe8, 0x9e, 0x33, 0x1b, 0x47, 0xb1}};
static const GUID DXC_ATTR_D3D12_CORE_COMPUTE= {0x248e2800, 0xa793, 0x4724, {0xab, 0xaa, 0x23, 0xa6, 0xde, 0x1b, 0xe0, 0x90}};
static const GUID DXC_ATTR_BOGUS             = {0x12345678, 0x1234, 0x1234, {1, 2, 3, 4, 5, 6, 7, 8}};

enum { DXP_InstanceLuid = 0, DXP_DriverVersion, DXP_DriverDescription, DXP_HardwareID, DXP_KmdModelVersion,
       DXP_ComputePreemption, DXP_GraphicsPreemption, DXP_DedicatedAdapterMemory, DXP_DedicatedSystemMemory,
       DXP_SharedSystemMemory, DXP_AcgCompatible, DXP_IsHardware, DXP_IsIntegrated, DXP_IsDetachable };
enum { DXPREF_Hardware = 0, DXPREF_MinimumPower, DXPREF_HighPerformance };

typedef HRESULT (WINAPI *DXCoreCreateAdapterFactory_t)(REFIID, void **);
typedef HRESULT (WINAPI *fac_createlist_t)(void *, UINT, const GUID *, REFIID, void **);
typedef HRESULT (WINAPI *fac_byluid_t)(void *, const LUID *, REFIID, void **);
typedef unsigned char (WINAPI *fac_isnotif_t)(void *, UINT);
typedef HRESULT (WINAPI *fac_reg_t)(void *, void *, UINT, void *, void *, UINT *);
typedef HRESULT (WINAPI *fac_unreg_t)(void *, UINT);
typedef HRESULT (WINAPI *list_getadapter_t)(void *, UINT, REFIID, void **);
typedef UINT    (WINAPI *list_count_t)(void *);
typedef unsigned char (WINAPI *list_stale_t)(void *);
typedef HRESULT (WINAPI *list_getfactory_t)(void *, REFIID, void **);
typedef HRESULT (WINAPI *list_sort_t)(void *, UINT, const UINT *);
typedef unsigned char (WINAPI *list_prefsupp_t)(void *, UINT);
typedef unsigned char (WINAPI *ad_isvalid_t)(void *);
typedef unsigned char (WINAPI *ad_isattr_t)(void *, const GUID *);
typedef unsigned char (WINAPI *ad_isprop_t)(void *, UINT);
typedef HRESULT (WINAPI *ad_getprop_t)(void *, UINT, SIZE_T, void *);
typedef HRESULT (WINAPI *ad_getpropsize_t)(void *, UINT, SIZE_T *);
typedef unsigned char (WINAPI *ad_isstate_t)(void *, UINT);
typedef HRESULT (WINAPI *ad_querystate_t)(void *, UINT, SIZE_T, const void *, SIZE_T, void *);
typedef HRESULT (WINAPI *ad_setstate_t)(void *, UINT, SIZE_T, const void *, SIZE_T, const void *);
typedef HRESULT (WINAPI *ad_getfactory_t)(void *, REFIID, void **);

#define FAC_CreateList(f, n, g, r, o)   VCALL(f, 3, fac_createlist_t)(f, n, g, r, o)
#define FAC_ByLuid(f, l, r, o)          VCALL(f, 4, fac_byluid_t)(f, l, r, o)
#define FAC_IsNotif(f, t)               VCALL(f, 5, fac_isnotif_t)(f, t)
#define FAC_Register(f, obj, t, cb, ctx, ck) VCALL(f, 6, fac_reg_t)(f, obj, t, cb, ctx, ck)
#define FAC_Unregister(f, ck)           VCALL(f, 7, fac_unreg_t)(f, ck)
#define LST_GetAdapter(l, i, r, o)      VCALL(l, 3, list_getadapter_t)(l, i, r, o)
#define LST_Count(l)                    VCALL(l, 4, list_count_t)(l)
#define LST_IsStale(l)                  VCALL(l, 5, list_stale_t)(l)
#define LST_GetFactory(l, r, o)         VCALL(l, 6, list_getfactory_t)(l, r, o)
#define LST_Sort(l, n, p)               VCALL(l, 7, list_sort_t)(l, n, p)
#define LST_PrefSupp(l, p)              VCALL(l, 8, list_prefsupp_t)(l, p)
#define AD_IsValid(a)                   VCALL(a, 3, ad_isvalid_t)(a)
#define AD_IsAttr(a, g)                 VCALL(a, 4, ad_isattr_t)(a, g)
#define AD_IsProp(a, p)                 VCALL(a, 5, ad_isprop_t)(a, p)
#define AD_GetProp(a, p, n, b)          VCALL(a, 6, ad_getprop_t)(a, p, n, b)
#define AD_GetPropSize(a, p, s)         VCALL(a, 7, ad_getpropsize_t)(a, p, s)
#define AD_IsQState(a, s)               VCALL(a, 8, ad_isstate_t)(a, s)
#define AD_QueryState(a, s, n, i, m, o) VCALL(a, 9, ad_querystate_t)(a, s, n, i, m, o)
#define AD_IsSState(a, s)               VCALL(a, 10, ad_isstate_t)(a, s)
#define AD_SetState(a, s, n, i, m, o)   VCALL(a, 11, ad_setstate_t)(a, s, n, i, m, o)
#define AD_GetFactory(a, r, o)          VCALL(a, 12, ad_getfactory_t)(a, r, o)

static DXCoreCreateAdapterFactory_t odm_dxc_create(void) {
    return (DXCoreCreateAdapterFactory_t)d3dh_getproc("dxcore.dll", "DXCoreCreateAdapterFactory");
}
/* Returns a factory (caller releases) or NULL after t_skip-worthy failure. */
static void *odm_factory(void) {
    DXCoreCreateAdapterFactory_t c = odm_dxc_create(); void *f = NULL;
    if (!c) return NULL;
    if (FAILED(c(&IID_IDXCoreAdapterFactory_, &f))) return NULL;
    return f;
}
#define NEED_FAC() void *fac = odm_factory(); if (!fac) { t_skip("dxcore factory unavailable"); return; }

/* adapter list for a single attribute GUID; NULL when it cannot be made */
static void *odm_list(void *fac, const GUID *attr) {
    void *l = NULL;
    if (FAILED(FAC_CreateList(fac, 1, attr, &IID_IDXCoreAdapterList_, &l))) return NULL;
    return l;
}

/* ---- D3D12 / D3D11On12 helpers ---- */
static ID3D12Device *odm_dev12(void) {
    ID3D12Device *d = NULL;
    if (FAILED(d3dh_make_device12(&d, D3D_FEATURE_LEVEL_11_0, NULL))) return NULL;
    return d;
}
static ID3D12CommandQueue *odm_queue(ID3D12Device *dev, D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_QUEUE_FLAGS flags, UINT node) {
    D3D12_COMMAND_QUEUE_DESC qd; ID3D12CommandQueue *q = NULL;
    memset(&qd, 0, sizeof qd); qd.Type = type; qd.Flags = flags; qd.NodeMask = node;
    if (FAILED(ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void **)&q))) return NULL;
    return q;
}
static PFN_D3D11ON12_CREATE_DEVICE odm_on12_create(void) {
    return (PFN_D3D11ON12_CREATE_DEVICE)d3dh_getproc("d3d11.dll", "D3D11On12CreateDevice");
}
#endif
