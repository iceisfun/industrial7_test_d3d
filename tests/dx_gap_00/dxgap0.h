#ifndef DXGAP0_H
#define DXGAP0_H
#define SENT_BLOB ((ID3DBlob *)(ULONG_PTR)0x40)
#define SENT_SC  ((IDXGISwapChain *)(ULONG_PTR)0x10)
#define SENT_DEV ((ID3D11Device *)(ULONG_PTR)0x20)
#define SENT_CTX ((ID3D11DeviceContext *)(ULONG_PTR)0x30)
#include "d3dh.h"

#define E_FAIL_HR          ((HRESULT)0x80004005)
#define E_INVALIDARG_HR    ((HRESULT)0x80070057)
#define E_NOTIMPL_HR       ((HRESULT)0x80004001)
#define E_POINTER_HR       ((HRESULT)0x80004003)
#define E_NOINTERFACE_HR   ((HRESULT)0x80004002)
#define INVALIDCALL_HR     ((HRESULT)0x8876086c)
#define CLASSNOTREG_HR     ((HRESULT)0x80040154)

#define FOURCC(a,b,c,d) ((DWORD)(a) | ((DWORD)(b) << 8) | ((DWORD)(c) << 16) | ((DWORD)(d) << 24))

/* Minimal DXBC container reader. */
static inline const unsigned char *gap0_dxbc_part(const void *data, size_t size, DWORD fourcc, DWORD *psize)
{
    const unsigned char *p = (const unsigned char *)data;
    DWORD i, count;
    if (size < 32 || memcmp(p, "DXBC", 4)) return NULL;
    count = *(const DWORD *)(p + 28);
    for (i = 0; i < count; i++) {
        DWORD off = *(const DWORD *)(p + 32 + 4 * i);
        if (off + 8 > size) return NULL;
        if (*(const DWORD *)(p + off) == fourcc) {
            if (psize) *psize = *(const DWORD *)(p + off + 4);
            return p + off + 8;
        }
    }
    return NULL;
}
static inline DWORD gap0_dxbc_nparts(const void *data, size_t size)
{
    return size >= 32 ? *(const DWORD *)((const unsigned char *)data + 28) : 0;
}
static inline DWORD gap0_dxbc_part_fourcc(const void *data, DWORD idx)
{
    const unsigned char *p = (const unsigned char *)data;
    return *(const DWORD *)(p + *(const DWORD *)(p + 32 + 4 * idx));
}

/* Compile a simple shader; sources are plain HLSL used only to obtain valid containers. */
static const char GAP0_VS_SRC[] =
    "struct VSIn { float4 p : POSITION; float2 uv : TEXCOORD0; };\n"
    "struct VSOut { float4 p : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut main(VSIn i) { VSOut o; o.p = i.p; o.uv = i.uv; return o; }\n";
static const char GAP0_PS_SRC[] =
    "struct PSIn { float4 p : SV_Position; float2 uv : TEXCOORD0; };\n"
    "float4 main(PSIn i) : SV_Target { return float4(i.uv, 0, 1); }\n";

typedef struct { const void *p; SIZE_T n; } gap0_buf;

/* Call a typed signature entry point and return its result; sizes via blob. */
#define GAP0_MODS_D3D10 "d3d10.dll", "d3d10_1.dll"

static inline int gap0_is_all_zero(const unsigned char *p, size_t n) { size_t i; for (i = 0; i < n; i++) if (p[i]) return 0; return 1; }

/* Access-violation probe: GAP0_CALL(call) yields the call's HRESULT, or GAP0_AVHR if it faulted. */
#include <setjmp.h>
#define GAP0_AVHR ((HRESULT)0x7ff9a001)
static jmp_buf g_gap0_jb; static volatile int g_gap0_guard, g_gap0_veh;
static LONG CALLBACK gap0_veh(PEXCEPTION_POINTERS ep)
{
    if (g_gap0_guard && ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) { g_gap0_guard = 0; longjmp(g_gap0_jb, 1); }
    return EXCEPTION_CONTINUE_SEARCH;
}
#define GAP0_CALL(call) ({ volatile HRESULT hr_ = E_FAIL; volatile int av_ = 0; if (!g_gap0_veh) { AddVectoredExceptionHandler(1, gap0_veh); g_gap0_veh = 1; } \
    if (!setjmp(g_gap0_jb)) { g_gap0_guard = 1; hr_ = (HRESULT)(call); g_gap0_guard = 0; } else av_ = 1; av_ ? GAP0_AVHR : hr_; })
#endif
