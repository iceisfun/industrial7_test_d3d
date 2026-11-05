/* gap_01: shared helpers (DXBC container walking, fixtures). */
#ifndef GAP1_H
#define GAP1_H
#include "d3dh.h"
#include "shaders.h"

#define G1_FOURCC(a, b, c, d) ((DWORD)(unsigned char)(a) | ((DWORD)(unsigned char)(b) << 8) | ((DWORD)(unsigned char)(c) << 16) | ((DWORD)(unsigned char)(d) << 24))
#define G1_DXBC G1_FOURCC('D', 'X', 'B', 'C')

#define G1_E_FAIL            ((HRESULT)0x80004005)
#define G1_E_INVALIDARG      ((HRESULT)0x80070057)
#define G1_E_NOINTERFACE     ((HRESULT)0x80004002)
#define G1_E_POINTER         ((HRESULT)0x80004003)
#define G1_E_NOTIMPL         ((HRESULT)0x80004001)
#define G1_E_HANDLE          ((HRESULT)0x80070006)
#define G1_D3DERR_INVALIDCALL ((HRESULT)0x8876086C)
#define G1_DXGI_ERROR_INVALID_CALL ((HRESULT)0x887A0001)
#define G1_DXGI_ERROR_NOT_FOUND ((HRESULT)0x887A0002)
#define G1_DXGI_ERROR_MORE_DATA ((HRESULT)0x887A0003)
#define G1_DXGI_ERROR_SDK_COMPONENT_MISSING ((HRESULT)0x887A002D)
#define G1_CLASS_E_NOAGGREGATION ((HRESULT)0x80040110)
#define G1_CLASS_E_CLASSNOTAVAILABLE ((HRESULT)0x80040111)

/* ---- DXBC container -------------------------------------------------------
 * header: 'DXBC', 16 byte hash, u16 major(1) u16 minor(0), u32 total size, u32 chunk count, u32 offsets[count];
 * chunk: fourcc, u32 size, payload. */
static inline DWORD g1_rd32(const void *p) { DWORD v; memcpy(&v, p, 4); return v; }
static inline int g1_is_dxbc(const void *p, SIZE_T n) {
    return n >= 0x20 && g1_rd32(p) == G1_DXBC && g1_rd32((const char *)p + 0x18) <= n;
}
static inline DWORD g1_chunk_count(const void *p) { return g1_rd32((const char *)p + 0x1c); }
static inline DWORD g1_chunk_fourcc(const void *p, DWORD i) { return g1_rd32((const char *)p + g1_rd32((const char *)p + 0x20 + 4 * i)); }
static inline DWORD g1_chunk_size_at(const void *p, DWORD i) { return g1_rd32((const char *)p + g1_rd32((const char *)p + 0x20 + 4 * i) + 4); }
static inline const unsigned char *g1_chunk_data_at(const void *p, DWORD i) { return (const unsigned char *)p + g1_rd32((const char *)p + 0x20 + 4 * i) + 8; }
/* First chunk with the given fourcc; returns payload pointer (NULL if none) and its size. */
static inline const unsigned char *g1_find_chunk(const void *p, SIZE_T n, DWORD fourcc, DWORD *size) {
    if (!g1_is_dxbc(p, n)) return NULL;
    DWORD c = g1_chunk_count(p);
    for (DWORD i = 0; i < c; i++) {
        if (g1_chunk_fourcc(p, i) == fourcc) { if (size) *size = g1_chunk_size_at(p, i); return g1_chunk_data_at(p, i); }
    }
    return NULL;
}
static inline int g1_has_chunk(const void *p, SIZE_T n, DWORD fourcc) { return g1_find_chunk(p, n, fourcc, NULL) != NULL; }

/* ---- stub IUnknown used as a pUnkOuter that must never be called ---------- */
typedef struct { const IUnknownVtbl *lpVtbl; LONG calls; } g1_unk;
static inline HRESULT WINAPI g1_unk_qi(IUnknown *s, REFIID r, void **o) { ((g1_unk *)s)->calls++; *o = NULL; return E_NOINTERFACE; }
static inline ULONG WINAPI g1_unk_addref(IUnknown *s) { ((g1_unk *)s)->calls++; return 2; }
static inline ULONG WINAPI g1_unk_release(IUnknown *s) { ((g1_unk *)s)->calls++; return 1; }
static const IUnknownVtbl g1_unk_vtbl = { g1_unk_qi, g1_unk_addref, g1_unk_release };

/* expression evaluated exactly once (EXPECT_EQ evaluates its arguments twice) */
#define G1_EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)

#define SKIP_ON_WINE(why) do { if (t_is_wine()) { t_skip("emulated: " why); return; } } while (0)

/* Fixture bytecode compiled with the system compiler: returns NULL (and notes) when it cannot be built. */
static inline ID3DBlob *g1_compile(const char *src, const char *target, UINT flags) {
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile(src, "main", target, flags, &b);
    if (FAILED(hr) || !b) { t_note("fixture compile (%s) failed: %s", target, d3dh_hr_name(hr)); if (b) { ID3D10Blob_Release(b); } return NULL; }
    return b;
}
#endif
