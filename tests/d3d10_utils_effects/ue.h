/* Shared helpers for the d3d10_utils_effects area. */
#ifndef UE_H
#define UE_H
#include "d3dh.h"
#include <d3d10effect.h>
#include <d3d10shader.h>
#include <d3d10misc.h>

#define E_D3DERR_INVALIDCALL ((HRESULT)0x8876086Cu)
#define E_COMPILE_FAILED     ((HRESULT)0x88760B59u)

typedef HRESULT (WINAPI *ue_CompileEffect_t)(void *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, UINT, UINT, ID3D10Blob **, ID3D10Blob **);
typedef HRESULT (WINAPI *ue_CompileShader_t)(const char *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, const char *, const char *, UINT, ID3D10Blob **, ID3D10Blob **);

/* Compile effect source with D3D10CompileEffectFromMemory (d3d10.dll). Returns the HRESULT; *out is the compiled effect blob. */
HRESULT ue_compile_fx(const char *src, UINT hlsl_flags, UINT fx_flags, ID3D10Blob **out);
/* Compile a shader with d3d10 (D3D10CompileShader) */
HRESULT ue_compile_vs(const char *src, const char *entry, const char *profile, ID3D10Blob **out);
/* Create an effect from source on a device. Returns S_OK or a failure; sets *skip when no compile path. */
HRESULT ue_make_effect(const char *src, ID3D10Device *dev, ID3D10Effect **fx);
const void *ue_bp(ID3D10Blob *b);
SIZE_T ue_bs(ID3D10Blob *b);
/* DXBC helpers */
const unsigned char *ue_dxbc_chunk(const void *dxbc, SIZE_T size, DWORD fourcc, DWORD *chunk_size);
DWORD ue_dxbc_nchunks(const void *dxbc);
/* Fetch the d3dcompiler-47 compile of an HLSL shader (reference). */
HRESULT ue_d3dc(const char *src, const char *entry, const char *target, UINT flags, ID3D10Blob **out);
#define FOURCC(a,b,c,d) ((DWORD)(a) | ((DWORD)(b) << 8) | ((DWORD)(c) << 16) | ((DWORD)(d) << 24))
#define UE_REL(p) SAFE_RELEASE(p)
/* EXPECT_EQ evaluates its arguments twice: use EQV for expressions with side effects (Release, AddRef, calls). */
#define EQV(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); EXPECT_EQ(v_, (want)); } while (0)
#define HRV(expr, want) do { HRESULT hv_ = (expr); EXPECT_HR(hv_, (want)); } while (0)

#define NOWINE(why) do { if (t_is_wine()) { t_skip("not run under an emulator: %s", why); return; } } while (0)

extern const char UE_VS_SRC[];
extern const char UE_PS_SRC[];
extern const char UE_GS_SRC[];
#endif

/* Shared WARP/ref D3D10 device for the effect tests (created once, released at process exit). NULL when unavailable. */
ID3D10Device *ue_shared_device(void);
#define NEED_DEVICE(dev) ID3D10Device *dev = ue_shared_device(); if (!dev) { t_skip("no D3D10 device"); return; }
/* compile + create; skips the test (returns 0) if the effect cannot be compiled/created at all */
int ue_fx_open(ID3D10Device *dev, const char *src, ID3D10Effect **fx);
extern const char UE_FX_BASIC[];
extern const char UE_FX_OBJECTS[];
extern const char UE_FX_SHADERS[];
extern const char UE_FX_STRING[];
extern const char UE_FX_POOL[];
extern const char UE_FX_CHILD[];
/* true when the shared device is only the emulator stand-in */
int ue_device_is_fake(void);
#define FX_OPEN(fx, src) ID3D10Effect *fx = NULL; if (!ue_fx_open(dev, src, &fx)) return
