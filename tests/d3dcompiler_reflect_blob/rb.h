/* Shared fixtures for the d3dcompiler_reflect_blob area: compile helpers, DXBC container editing, reflection access. */
#ifndef RB_H
#define RB_H
#include "d3dh.h"
#include <d3d11shader.h>
#include <math.h>

#define RB_E_FAIL        ((HRESULT)0x80004005L)
#define RB_E_INVALIDARG  ((HRESULT)0x80070057L)
#define RB_E_NOINTERFACE ((HRESULT)0x80004002L)
#define RB_INVALIDCALL   ((HRESULT)0x8876086CL)  /* D3DERR_INVALIDCALL */

#define RB_FOURCC(a, b, c, d) ((DWORD)(BYTE)(a) | ((DWORD)(BYTE)(b) << 8) | ((DWORD)(BYTE)(c) << 16) | ((DWORD)(BYTE)(d) << 24))
#define FCC_RDEF RB_FOURCC('R','D','E','F')
#define FCC_ISGN RB_FOURCC('I','S','G','N')
#define FCC_OSGN RB_FOURCC('O','S','G','N')
#define FCC_PCSG RB_FOURCC('P','C','S','G')
#define FCC_SHDR RB_FOURCC('S','H','D','R')
#define FCC_SHEX RB_FOURCC('S','H','E','X')
#define FCC_STAT RB_FOURCC('S','T','A','T')
#define FCC_SFI0 RB_FOURCC('S','F','I','0')
#define FCC_SPDB RB_FOURCC('S','P','D','B')
#define FCC_SDBG RB_FOURCC('S','D','B','G')
#define FCC_PRIV RB_FOURCC('P','R','I','V')
#define FCC_RTS0 RB_FOURCC('R','T','S','0')
#define FCC_ILDN RB_FOURCC('I','L','D','N')
#define FCC_DXBC RB_FOURCC('D','X','B','C')

/* ---- libraries / library reflection interfaces missing from the headers ----------------------- */
typedef struct { LPCSTR Creator; UINT Flags; UINT FunctionCount; } RB_LIBRARY_DESC;
typedef struct {
    UINT Version; LPCSTR Creator; UINT Flags; UINT ConstantBuffers; UINT BoundResources; UINT InstructionCount;
    UINT TempRegisterCount; UINT TempArrayCount; UINT DefCount; UINT DclCount; UINT TextureNormalInstructions;
    UINT TextureLoadInstructions; UINT TextureCompInstructions; UINT TextureBiasInstructions; UINT TextureGradientInstructions;
    UINT FloatInstructionCount; UINT IntInstructionCount; UINT UintInstructionCount; UINT StaticFlowControlCount;
    UINT DynamicFlowControlCount; UINT MacroInstructionCount; UINT ArrayInstructionCount; UINT MovInstructionCount;
    UINT MovcInstructionCount; UINT ConversionInstructionCount; UINT BitwiseInstructionCount;
    D3D_FEATURE_LEVEL MinFeatureLevel; UINT64 RequiredFeatureFlags; LPCSTR Name; INT FunctionParameterCount;
    BOOL HasReturn; BOOL Has10Level9VertexShader; BOOL Has10Level9PixelShader;
} RB_FUNCTION_DESC;

typedef struct RB_FnParam RB_FnParam;
typedef struct RB_FnParamVtbl { HRESULT (WINAPI *GetDesc)(RB_FnParam *, D3D11_PARAMETER_DESC *); } RB_FnParamVtbl;
struct RB_FnParam { const RB_FnParamVtbl *lpVtbl; };

typedef struct RB_Fn RB_Fn;
typedef struct RB_FnVtbl {
    HRESULT (WINAPI *GetDesc)(RB_Fn *, RB_FUNCTION_DESC *);
    ID3D11ShaderReflectionConstantBuffer *(WINAPI *GetConstantBufferByIndex)(RB_Fn *, UINT);
    ID3D11ShaderReflectionConstantBuffer *(WINAPI *GetConstantBufferByName)(RB_Fn *, LPCSTR);
    HRESULT (WINAPI *GetResourceBindingDesc)(RB_Fn *, UINT, D3D11_SHADER_INPUT_BIND_DESC *);
    ID3D11ShaderReflectionVariable *(WINAPI *GetVariableByName)(RB_Fn *, LPCSTR);
    HRESULT (WINAPI *GetResourceBindingDescByName)(RB_Fn *, LPCSTR, D3D11_SHADER_INPUT_BIND_DESC *);
    RB_FnParam *(WINAPI *GetFunctionParameter)(RB_Fn *, INT);
} RB_FnVtbl;
struct RB_Fn { const RB_FnVtbl *lpVtbl; };

typedef struct RB_LibRefl RB_LibRefl;
typedef struct RB_LibReflVtbl {
    HRESULT (WINAPI *QueryInterface)(RB_LibRefl *, REFIID, void **);
    ULONG (WINAPI *AddRef)(RB_LibRefl *);
    ULONG (WINAPI *Release)(RB_LibRefl *);
    HRESULT (WINAPI *GetDesc)(RB_LibRefl *, RB_LIBRARY_DESC *);
    RB_Fn *(WINAPI *GetFunctionByIndex)(RB_LibRefl *, INT);
} RB_LibReflVtbl;
struct RB_LibRefl { const RB_LibReflVtbl *lpVtbl; };
static const GUID RB_IID_LibraryReflection = { 0x54384f1b, 0x5b3e, 0x4bb7, { 0xae, 0x01, 0x60, 0xba, 0x30, 0x97, 0xcb, 0xb6 } };
static const GUID RB_IID_ShaderReflection12 = { 0x5a58797d, 0xa72c, 0x478d, { 0x8b, 0xa2, 0xef, 0xc6, 0xb0, 0xef, 0xe8, 0x8e } };

typedef HRESULT (WINAPI *RB_ReflectLibrary_t)(const void *, SIZE_T, REFIID, void **);
typedef HRESULT (WINAPI *RB_Compress_t)(UINT, const void *, UINT, ID3DBlob **);
typedef HRESULT (WINAPI *RB_Decompress_t)(const void *, SIZE_T, UINT, UINT, UINT *, UINT, ID3DBlob **, UINT *);
typedef HRESULT (WINAPI *RB_CreateFLG_t)(UINT, ID3D11FunctionLinkingGraph **);
typedef struct { const void *pBytecode; SIZE_T BytecodeLength; } RB_SHADER_DATA;
typedef void (WINAPI *RB_DebugSetMute_t)(BOOL);
#define RB_D3DReflectLibrary() ((RB_ReflectLibrary_t)t_proc_in("d3dcompiler_47.dll", "D3DReflectLibrary"))
#define RB_D3DCompressShaders() ((RB_Compress_t)t_proc_in("d3dcompiler_47.dll", "D3DCompressShaders"))
#define RB_D3DDecompressShaders() ((RB_Decompress_t)t_proc_in("d3dcompiler_47.dll", "D3DDecompressShaders"))
#define RB_D3DCreateFunctionLinkingGraph() ((RB_CreateFLG_t)t_proc_in("d3dcompiler_47.dll", "D3DCreateFunctionLinkingGraph"))
#define RB_DebugSetMute() ((RB_DebugSetMute_t)t_proc_in("d3dcompiler_47.dll", "DebugSetMute"))

/* EXPECT_EQ evaluates its arguments twice: capture calls with side effects first. */
#define RB_REFS(op, b, n) do { ULONG r_ = ID3D10Blob_##op(b); EXPECT_EQ(r_, n); } while (0)

/* Entry points that are unimplemented stubs under the emulator abort the process: skip those tests there. */
#define RB_NOWINE(what) do { if (t_is_wine()) { t_skip("%s is not implemented by the emulator", what); return; } } while (0)

/* ---- DXBC container editing ---------------------------------------------------------------------- */
typedef struct { DWORD fourcc; const void *data; DWORD size; } rb_chunk;
#define RB_MAXCHUNKS 32

static int rb_dxbc_parse(const void *p, size_t n, rb_chunk *out, int max)
{
    const BYTE *b = p; DWORD cnt, i;
    if (n < 0x20 || memcmp(b, "DXBC", 4)) return -1;
    cnt = *(const DWORD *)(b + 0x1c);
    if (cnt > (DWORD)max || 0x20 + cnt * 4 > n) return -1;
    for (i = 0; i < cnt; i++) {
        DWORD off = *(const DWORD *)(b + 0x20 + i * 4);
        if (off + 8 > n) return -1;
        out[i].fourcc = *(const DWORD *)(b + off); out[i].size = *(const DWORD *)(b + off + 4); out[i].data = b + off + 8;
        if (off + 8 + out[i].size > n) return -1;
    }
    return (int)cnt;
}
static const rb_chunk *rb_find(const rb_chunk *c, int n, DWORD fcc) { for (int i = 0; i < n; i++) if (c[i].fourcc == fcc) return &c[i]; return NULL; }

static void rb_md5_block(DWORD st[4], const BYTE *p)
{
    static DWORD K[64]; static int init;
    static const int S[64] = { 7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22, 5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                               4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23, 6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21 };
    DWORD M[16], a, b, c, d;
    if (!init) { for (int i = 0; i < 64; i++) { double v = sin((double)(i + 1)); if (v < 0) v = -v; K[i] = (DWORD)(4294967296.0 * v); } init = 1; }
    memcpy(M, p, 64); a = st[0]; b = st[1]; c = st[2]; d = st[3];
    for (int i = 0; i < 64; i++) {
        DWORD f, t, x; int g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | ~d); g = (7 * i) & 15; }
        t = d; d = c; c = b; x = a + f + K[i] + M[g];
        b = b + ((x << S[i]) | (x >> (32 - S[i]))); a = t;
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
}
/* Container hash: MD5 over everything after the 20-byte header prefix, with the container's own padding scheme. */
static void rb_dxbc_rehash(BYTE *data, size_t total)
{
    const BYTE *p = data + 20; size_t size = total - 20, full = size / 64, left = size & 63; BYTE blk[64]; DWORD st[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    DWORD bits = (DWORD)size * 8, bits_or = ((DWORD)size * 2) | 1;
    for (size_t i = 0; i < full; i++) rb_md5_block(st, p + i * 64);
    p += full * 64;
    if (left >= 56) {
        memset(blk, 0, 64); memcpy(blk, p, left); blk[left] = 0x80; rb_md5_block(st, blk);
        memset(blk, 0, 64); memcpy(blk, &bits, 4); memcpy(blk + 60, &bits_or, 4); rb_md5_block(st, blk);
    } else {
        memset(blk, 0, 64); memcpy(blk, &bits, 4); memcpy(blk + 4, p, left); blk[4 + left] = 0x80; memcpy(blk + 60, &bits_or, 4); rb_md5_block(st, blk);
    }
    memcpy(data + 4, st, 16);
}
/* Assemble a container from chunks. The container format requires chunks to be packed back to back, so a chunk size
 * that is not a multiple of 4 is padded with zero bytes and the padded size is stored. gap = bytes of slack inserted
 * before chunk number gap_at (gap_at < 0: none); the container reader rejects slack between chunks. */
static BYTE *rb_dxbc_build_gap(const rb_chunk *c, int n, int gap_at, DWORD gap, size_t *out_size)
{
    size_t total = 0x20 + (size_t)n * 4, pos; BYTE *b;
    for (int i = 0; i < n; i++) total += 8 + ((c[i].size + 3) & ~3u) + (i == gap_at ? gap : 0);
    b = calloc(1, total); if (!b) return NULL;
    memcpy(b, "DXBC", 4); *(WORD *)(b + 0x14) = 1; *(DWORD *)(b + 0x18) = (DWORD)total; *(DWORD *)(b + 0x1c) = n;
    pos = 0x20 + (size_t)n * 4;
    for (int i = 0; i < n; i++) {
        DWORD psz = (c[i].size + 3) & ~3u;
        if (i == gap_at) pos += gap;
        *(DWORD *)(b + 0x20 + i * 4) = (DWORD)pos;
        *(DWORD *)(b + pos) = c[i].fourcc; *(DWORD *)(b + pos + 4) = psz;
        if (c[i].size) memcpy(b + pos + 8, c[i].data, c[i].size);
        pos += 8 + psz;
    }
    rb_dxbc_rehash(b, total);
    if (out_size) *out_size = total;
    return b;
}
static BYTE *rb_dxbc_build(const rb_chunk *c, int n, size_t *out_size) { return rb_dxbc_build_gap(c, n, -1, 0, out_size); }
/* Copy of a container with the chunk(s) of one fourcc removed (0 = keep all), optionally replaced by new data. */
static BYTE *rb_dxbc_edit(const void *src, size_t n, DWORD drop, DWORD add_fcc, const void *add, DWORD add_size, size_t *out_size)
{
    rb_chunk c[RB_MAXCHUNKS + 1]; rb_chunk o[RB_MAXCHUNKS + 1]; int k = 0, cnt = rb_dxbc_parse(src, n, c, RB_MAXCHUNKS);
    if (cnt < 0) return NULL;
    for (int i = 0; i < cnt; i++) if (!drop || c[i].fourcc != drop) o[k++] = c[i];
    if (add_fcc) { o[k].fourcc = add_fcc; o[k].data = add; o[k].size = add_size; k++; }
    return rb_dxbc_build(o, k, out_size);
}

/* ---- compile / reflect -------------------------------------------------------------------------- */
/* Compile "main"; on failure: skip under an emulator, record a failed check otherwise. Returns 1 on success. */
static int rb_compile_t(const char *src, const char *target, UINT flags, ID3DBlob **out)
{
    HRESULT hr;
    *out = NULL;
    if (!d3dh_D3DCompile()) { t_skip("d3dcompiler_47.dll unavailable"); return 0; }
    hr = d3dh_compile(src, "main", target, flags, out);
    if (FAILED(hr)) {
        if (t_is_wine()) { t_skip("compiler cannot build this shader under emulation (%s)", d3dh_hr_name(hr)); return 0; }
        t_check(0, __FILE__, __LINE__, "compile succeeds", "%s target %s: %s", d3dh_hr_name(hr), target, d3dh_compile_errors());
        return 0;
    }
    return 1;
}
#define RB_COMPILE(blob, src, target) ID3DBlob *blob = NULL; do { if (!rb_compile_t((src), (target), 0, &blob)) return; } while (0)
#define RB_COMPILE_F(blob, src, target, flags) ID3DBlob *blob = NULL; do { if (!rb_compile_t((src), (target), (flags), &blob)) return; } while (0)
#define RB_BP(b) ((const BYTE *)ID3D10Blob_GetBufferPointer(b))
#define RB_BS(b) ((SIZE_T)ID3D10Blob_GetBufferSize(b))

static ID3D11ShaderReflection *rb_reflect(ID3DBlob *b)
{
    ID3D11ShaderReflection *r = NULL;
    HRESULT hr = d3dh_D3DReflect()(RB_BP(b), RB_BS(b), &IID_ID3D11ShaderReflection, (void **)&r);
    t_check(SUCCEEDED(hr) && r, __FILE__, __LINE__, "D3DReflect ok", "%s", d3dh_hr_name(hr));
    return r;
}
/* Compile + reflect in one go; releases the blob after. Variables: r (reflection), blob. */
#define RB_REFLECT(r, src, target) \
    RB_COMPILE(r##_blob, src, target); \
    ID3D11ShaderReflection *r = rb_reflect(r##_blob); \
    if (!r) { ID3D10Blob_Release(r##_blob); return; }
#define RB_DONE(r) do { r->lpVtbl->Release(r); ID3D10Blob_Release(r##_blob); } while (0)

static ID3D11ShaderReflectionVariable *rb_var(ID3D11ShaderReflection *r, const char *name) { return r->lpVtbl->GetVariableByName(r, name); }
static int rb_var_desc(ID3D11ShaderReflection *r, const char *name, D3D11_SHADER_VARIABLE_DESC *d)
{
    ID3D11ShaderReflectionVariable *v = rb_var(r, name);
    HRESULT hr; memset(d, 0, sizeof *d);
    hr = v->lpVtbl->GetDesc(v, d);
    t_check(SUCCEEDED(hr), __FILE__, __LINE__, "variable desc", "%s: %s", name, d3dh_hr_name(hr));
    return SUCCEEDED(hr);
}
static int rb_type_desc(ID3D11ShaderReflection *r, const char *name, D3D11_SHADER_TYPE_DESC *d)
{
    ID3D11ShaderReflectionVariable *v = rb_var(r, name); ID3D11ShaderReflectionType *t = v->lpVtbl->GetType(v);
    HRESULT hr; memset(d, 0, sizeof *d);
    hr = t->lpVtbl->GetDesc(t, d);
    t_check(SUCCEEDED(hr), __FILE__, __LINE__, "type desc", "%s: %s", name, d3dh_hr_name(hr));
    return SUCCEEDED(hr);
}

/* Common HLSL fragments. */
#define RB_VS_PASS "float4 main(float4 p : POSITION) : SV_Position { return p; }\n"
#define RB_PS_CONST "float4 main() : SV_Target { return float4(1,0,0,1); }\n"
#endif
