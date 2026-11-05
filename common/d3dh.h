/* d3dh: shared helpers for C (COM via lpVtbl) Direct3D tests.
 * Nothing here links a D3D/DXGI/compiler DLL: every entry point is resolved with LoadLibrary/GetProcAddress
 * (d3dh_getproc, cached). Only IID/CLSID data comes from -ldxguid/-luuid.
 * Policy: prefer WARP, then REFERENCE, then NULL, then HARDWARE; if no device can be made the caller
 * does t_skip(). Never show a window. */
#ifndef D3DH_H
#define D3DH_H
#define COBJMACROS
#define CINTERFACE
#include "t.h"
#include <objbase.h>
#include <d3dcommon.h>
#include <dxgi1_4.h>
#include <d3d9.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <ddraw.h>
#include <stdint.h>
#include <stdlib.h>

/* ---- dynamic loading ----------------------------------------------------- */
FARPROC d3dh_getproc(const char *dll, const char *name);   /* LoadLibrary + GetProcAddress, result cached; NULL if absent */
int     d3dh_have_dll(const char *dll);                     /* 1 if the DLL loads */
#define D3DH_FN(dll, name) ((__typeof__(&name))d3dh_getproc(dll, #name))
/* Typed loaders (NULL when the DLL or export is missing). Use: REQUIRE(d3dh_D3D11CreateDevice()); */
#define d3dh_D3D11CreateDevice()               D3DH_FN("d3d11.dll", D3D11CreateDevice)
#define d3dh_D3D11CreateDeviceAndSwapChain()   D3DH_FN("d3d11.dll", D3D11CreateDeviceAndSwapChain)
#define d3dh_CreateDXGIFactory()               D3DH_FN("dxgi.dll", CreateDXGIFactory)
#define d3dh_CreateDXGIFactory1()              D3DH_FN("dxgi.dll", CreateDXGIFactory1)
#define d3dh_CreateDXGIFactory2()              D3DH_FN("dxgi.dll", CreateDXGIFactory2)
#define d3dh_DXGIGetDebugInterface1()          D3DH_FN("dxgi.dll", DXGIGetDebugInterface1)
#define d3dh_D3D12CreateDevice()               D3DH_FN("d3d12.dll", D3D12CreateDevice)
#define d3dh_D3D12GetDebugInterface()          D3DH_FN("d3d12.dll", D3D12GetDebugInterface)
#define d3dh_D3D12SerializeRootSignature()     D3DH_FN("d3d12.dll", D3D12SerializeRootSignature)
#define d3dh_D3D12SerializeVersionedRootSignature() D3DH_FN("d3d12.dll", D3D12SerializeVersionedRootSignature)
#define d3dh_D3D12CreateRootSignatureDeserializer() D3DH_FN("d3d12.dll", D3D12CreateRootSignatureDeserializer)
#define d3dh_Direct3DCreate9()                 D3DH_FN("d3d9.dll", Direct3DCreate9)
#define d3dh_Direct3DCreate9Ex()               D3DH_FN("d3d9.dll", Direct3DCreate9Ex)
#define d3dh_D3D10CreateDevice()               D3DH_FN("d3d10.dll", D3D10CreateDevice)
#define d3dh_D3D10CreateDevice1()              D3DH_FN("d3d10_1.dll", D3D10CreateDevice1)
#define d3dh_D3D10CreateDeviceAndSwapChain()   D3DH_FN("d3d10.dll", D3D10CreateDeviceAndSwapChain)
#define d3dh_D3DCompile()                      D3DH_FN("d3dcompiler_47.dll", D3DCompile)
#define d3dh_D3DCompile2()                     D3DH_FN("d3dcompiler_47.dll", D3DCompile2)
#define d3dh_D3DReflect()                      D3DH_FN("d3dcompiler_47.dll", D3DReflect)
#define d3dh_D3DDisassemble()                  D3DH_FN("d3dcompiler_47.dll", D3DDisassemble)
#define d3dh_D3DCreateBlob()                   D3DH_FN("d3dcompiler_47.dll", D3DCreateBlob)
#define d3dh_D3DStripShader()                  D3DH_FN("d3dcompiler_47.dll", D3DStripShader)
#define d3dh_D3DGetBlobPart()                  D3DH_FN("d3dcompiler_47.dll", D3DGetBlobPart)
#define d3dh_D3DPreprocess()                   D3DH_FN("d3dcompiler_47.dll", D3DPreprocess)
#define d3dh_DirectDrawCreate()                D3DH_FN("ddraw.dll", DirectDrawCreate)
#define d3dh_DirectDrawCreateEx()              D3DH_FN("ddraw.dll", DirectDrawCreateEx)

/* ---- hidden window --------------------------------------------------------
 * WS_POPUP window of the given size, class registered once, never shown (falls back to a message-only window when no desktop exists). */
HWND d3dh_create_hidden_window(void);                       /* 64x64 */
HWND d3dh_create_hidden_window_sized(int w, int h);
void d3dh_destroy_window(HWND h);                           /* DestroyWindow (NULL-safe) */

/* ---- COM release helpers -------------------------------------------------- */
#define SAFE_RELEASE(p)       do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)
ULONG d3dh_refcount(void *unk);                             /* current refcount (AddRef then Release; 0 for NULL) */
#define D3DH_RELEASE_RC(p)    ((p) ? (p)->lpVtbl->Release(p) : 0)  /* Release and yield the count after it */
#define D3DH_QI(p, iid, out)  ((p)->lpVtbl->QueryInterface((p), &(iid), (void **)(out)))

/* ---- HRESULT names / EXPECT_HR -------------------------------------------- */
const char *d3dh_hr_name(HRESULT hr);                       /* "DXGI_ERROR_DEVICE_REMOVED" or "0x887a0005" (4 rotating buffers) */
#define EXPECT_HR(hr, want) do { HRESULT h_ = (HRESULT)(hr), w_ = (HRESULT)(want); \
    t_check(h_ == w_, __FILE__, __LINE__, #hr " == " #want, "got %s want %s", d3dh_hr_name(h_), d3dh_hr_name(w_)); } while (0)
#define EXPECT_HR_OK(hr)    do { HRESULT h_ = (HRESULT)(hr); \
    t_check(SUCCEEDED(h_), __FILE__, __LINE__, "SUCCEEDED(" #hr ")", "got %s", d3dh_hr_name(h_)); } while (0)
#define EXPECT_HR_FAILED(hr) do { HRESULT h_ = (HRESULT)(hr); \
    t_check(FAILED(h_), __FILE__, __LINE__, "FAILED(" #hr ")", "got %s", d3dh_hr_name(h_)); } while (0)

/* ---- GUID printing --------------------------------------------------------- */
const char *d3dh_guid_str(const GUID *g);                   /* "{xxxxxxxx-...}" (4 rotating buffers); known D3D IIDs are suffixed with a name */

/* ---- deterministic PRNG (splitmix64) and hashes ---------------------------- */
typedef struct { uint64_t s; } d3dh_rng;
void     d3dh_rng_seed(d3dh_rng *r, uint64_t seed);
uint32_t d3dh_rng_u32(d3dh_rng *r);
float    d3dh_rng_float(d3dh_rng *r);                       /* [0,1) */
uint32_t d3dh_fnv1a(const void *data, size_t len);          /* FNV-1a 32 */
uint32_t d3dh_crc32(const void *data, size_t len);          /* IEEE CRC32 */

/* ---- formats --------------------------------------------------------------- */
UINT d3dh_format_bytes(DXGI_FORMAT f);                      /* bytes per pixel for common uncompressed formats, 0 if unknown/compressed */

/* ---- images read back from the GPU ------------------------------------------- */
typedef struct { UINT width, height, pitch; DXGI_FORMAT format; unsigned char *data; size_t size; } d3dh_image;
void     d3dh_image_free(d3dh_image *im);
uint32_t d3dh_image_hash(const d3dh_image *im);             /* FNV-1a over tightly packed rows (pitch padding excluded) */
uint32_t d3dh_image_pixel32(const d3dh_image *im, UINT x, UINT y); /* 4-byte pixel as DWORD */

/* ---- device creation (each returns HRESULT; caller t_skip()s on failure) ----- */
/* 11: tries WARP, REFERENCE, NULL, HARDWARE. *fl_out and *driver_out may be NULL. flags = D3D11_CREATE_DEVICE_*. */
HRESULT d3dh_make_device11(ID3D11Device **dev, ID3D11DeviceContext **ctx, D3D_FEATURE_LEVEL *fl_out, D3D_DRIVER_TYPE *driver_out, UINT flags);
/* 11 with an explicit minimum feature-level list. */
HRESULT d3dh_make_device11_fl(ID3D11Device **dev, ID3D11DeviceContext **ctx, const D3D_FEATURE_LEVEL *fls, UINT nfl, D3D_FEATURE_LEVEL *fl_out, D3D_DRIVER_TYPE *driver_out, UINT flags);
/* 10.0: WARP, REFERENCE, NULL, HARDWARE (D3D10_DRIVER_TYPE_*). */
HRESULT d3dh_make_device10(ID3D10Device **dev, D3D10_DRIVER_TYPE *driver_out, UINT flags);
/* 10.1: same order, tries 10_1 then 10_0 hardware level (D3D10_FEATURE_LEVEL1). */
HRESULT d3dh_make_device10_1(ID3D10Device1 **dev, D3D10_FEATURE_LEVEL1 *fl_out, D3D10_DRIVER_TYPE *driver_out, UINT flags);
/* 12: WARP adapter via IDXGIFactory4::EnumWarpAdapter, then default adapter. min_fl e.g. D3D_FEATURE_LEVEL_11_0. */
HRESULT d3dh_make_device12(ID3D12Device **dev, D3D_FEATURE_LEVEL min_fl, int *is_warp_out);
/* 9: D3DDEVTYPE_NULLREF then HAL then REF, windowed on hwnd (NULL -> a hidden window is created and returned via *hwnd_out). */
HRESULT d3dh_make_device9(IDirect3D9 **d3d, IDirect3DDevice9 **dev, HWND hwnd, HWND *hwnd_out, D3DDEVTYPE *type_out);
/* 9Ex: same with Direct3DCreate9Ex / CreateDeviceEx. */
HRESULT d3dh_make_device9ex(IDirect3D9Ex **d3d, IDirect3DDevice9Ex **dev, HWND hwnd, HWND *hwnd_out, D3DDEVTYPE *type_out);
/* DXGI factory conveniences (riid-typed): */
HRESULT d3dh_make_factory1(IDXGIFactory1 **f);              /* CreateDXGIFactory1 */
HRESULT d3dh_make_factory2(IDXGIFactory2 **f);              /* CreateDXGIFactory2(0) falling back to CreateDXGIFactory1+QI */

/* ---- shader compile ---------------------------------------------------------- */
/* Compile via d3dcompiler_47. Returns HRESULT (E_NOTIMPL-like D3DH_E_NOCOMPILER if DLL missing).
 * Error/warning text of the last compile is in d3dh_compile_errors(). target e.g. "vs_5_0". */
#define D3DH_E_NOCOMPILER ((HRESULT)0x8007007Eu)            /* HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND) */
HRESULT d3dh_compile(const char *src, const char *entry, const char *target, UINT flags, ID3DBlob **out);
HRESULT d3dh_compile_ex(const char *src, const char *srcname, const D3D_SHADER_MACRO *defs, const char *entry, const char *target, UINT flags1, UINT flags2, ID3DBlob **out);
const char *d3dh_compile_errors(void);                      /* text of last d3dh_compile* ("" if none) */
const void *d3dh_blob_ptr(ID3DBlob *b);
SIZE_T      d3dh_blob_size(ID3DBlob *b);

/* ---- readback (staging copy + Map); caller frees with d3dh_image_free / free() ---- */
HRESULT d3dh_readback_texture2d11(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *src, UINT subresource, d3dh_image *out);
HRESULT d3dh_readback_buffer11(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Buffer *src, void **data, size_t *size);
HRESULT d3dh_readback_texture2d10(ID3D10Device *dev, ID3D10Texture2D *src, d3dh_image *out);
HRESULT d3dh_readback_buffer10(ID3D10Device *dev, ID3D10Buffer *src, void **data, size_t *size);
/* 12: builds a private queue/allocator/list, transitions from state_before to COPY_SOURCE, copies to a readback heap, waits. */
HRESULT d3dh_readback_texture2d12(ID3D12Device *dev, ID3D12Resource *src, D3D12_RESOURCE_STATES state_before, d3dh_image *out);
HRESULT d3dh_readback_buffer12(ID3D12Device *dev, ID3D12Resource *src, D3D12_RESOURCE_STATES state_before, void **data, size_t *size);
/* 12 helper: wait for all work on a queue (creates a fence). */
HRESULT d3dh_d12_flush(ID3D12Device *dev, ID3D12CommandQueue *q);
/* 12 C-ABI wrappers for methods returning structs by value: */
D3D12_RESOURCE_DESC d3dh_d12_resource_desc(ID3D12Resource *r);
D3D12_CPU_DESCRIPTOR_HANDLE d3dh_d12_heap_cpu_start(ID3D12DescriptorHeap *h);
D3D12_GPU_DESCRIPTOR_HANDLE d3dh_d12_heap_gpu_start(ID3D12DescriptorHeap *h);
#endif
