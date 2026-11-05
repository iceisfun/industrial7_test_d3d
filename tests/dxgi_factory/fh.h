/* dxgi_factory shared helpers (static, per translation unit). */
#ifndef FH_H
#define FH_H
#include "d3dh.h"
#include <dxgi1_6.h>
#include <dxgidebug.h>

#define DXGI_E_INVALID_CALL ((HRESULT)0x887A0001)
#define DXGI_E_NOT_FOUND    ((HRESULT)0x887A0002)
#define DXGI_E_MORE_DATA    ((HRESULT)0x887A0003)
#define DXGI_E_UNSUPPORTED  ((HRESULT)0x887A0004)
#define DXGI_E_NOT_CURRENTLY_AVAILABLE ((HRESULT)0x887A0022)
#define POISON ((void *)(ULONG_PTR)0xdeadbeefdeadbeefULL)

typedef HRESULT (WINAPI *pfn_cf)(REFIID, void **);
typedef HRESULT (WINAPI *pfn_cf2)(UINT, REFIID, void **);

static pfn_cf  fh_cf0(void) { return (pfn_cf)d3dh_getproc("dxgi.dll", "CreateDXGIFactory"); }
static pfn_cf  fh_cf1(void) { return (pfn_cf)d3dh_getproc("dxgi.dll", "CreateDXGIFactory1"); }
static pfn_cf2 fh_cf2(void) { return (pfn_cf2)d3dh_getproc("dxgi.dll", "CreateDXGIFactory2"); }

/* highest-level factory through CreateDXGIFactory2(0,...) or CreateDXGIFactory1 */
static HRESULT fh_make(REFIID riid, void **out) {
    pfn_cf2 c2 = fh_cf2();
    if (c2) return c2(0, riid, out);
    pfn_cf c1 = fh_cf1();
    return c1 ? c1(riid, out) : D3DH_E_NOCOMPILER;
}
static IDXGIFactory1 *fh_f1(void) {
    IDXGIFactory1 *f = NULL;
    if (FAILED(fh_make(&IID_IDXGIFactory1, (void **)&f))) return NULL;
    return f;
}
/* QI helper: returns NULL if unsupported */
static void *fh_qi(IUnknown *u, REFIID iid) {
    void *p = NULL;
    if (FAILED(u->lpVtbl->QueryInterface(u, iid, &p))) return NULL;
    return p;
}
static ULONG fh_rc(IUnknown *u) { u->lpVtbl->AddRef(u); return u->lpVtbl->Release(u); }
static IUnknown *fh_ident(IUnknown *u) { return (IUnknown *)fh_qi(u, &IID_IUnknown); }
static int fh_luid_eq(LUID a, LUID b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; }
static int fh_inv_ok(HRESULT hr) { return hr == DXGI_E_INVALID_CALL || hr == E_INVALIDARG || hr == E_POINTER; }

#define WINE_NOFAC() do { if (t_is_wine()) { void *p_ = NULL; if (FAILED(fh_make(&IID_IUnknown, &p_))) { t_skip("wine: no DXGI factory in this environment"); return; } if (p_) ((IUnknown *)p_)->lpVtbl->Release((IUnknown *)p_); } } while (0)
#define WINE_SKIP() do { if (t_is_wine()) { t_skip("export set/behaviour differs under wine"); return; } } while (0)
#define NEED_F1(var) IDXGIFactory1 *var = fh_f1(); if (!var) { t_skip("no IDXGIFactory1"); return; }
#define NEED_FAC(var, T, iid) T *var = NULL; if (FAILED(fh_make(&iid, (void **)&var)) || !var) { t_skip("no " #T); return; }
#endif
