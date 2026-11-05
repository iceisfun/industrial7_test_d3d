/* Shared helpers for the d3dcompiler_compile area. Everything is resolved from d3dcompiler_47.dll
 * by GetProcAddress; the compiler DLL is never linked. */
#ifndef CC_H
#define CC_H
#include "d3dh.h"
#include <d3dcompiler.h>
#include <stdint.h>

#define CC_DLL "d3dcompiler_47.dll"
#define CC_INVALIDCALL  ((HRESULT)0x8876086C)   /* D3DERR_INVALIDCALL, returned for bad flags / bad entry */
#define CC_COMPILEFAIL  ((HRESULT)0x88760B59)   /* returned when the compile produced errors after parsing */

UDECL(HRESULT, D3DAssemble, (LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, UINT, ID3DBlob **, ID3DBlob **));
UDECL(HRESULT, D3DDisassembleRegion, (LPCVOID, SIZE_T, UINT, LPCSTR, SIZE_T, SIZE_T, SIZE_T *, ID3DBlob **));
UDECL(HRESULT, D3DGetTraceInstructionOffsets, (LPCVOID, SIZE_T, UINT, SIZE_T, SIZE_T, SIZE_T *, SIZE_T *));
UDECL(HRESULT, D3DDisassemble10Effect, (void *, UINT, ID3DBlob **));

#define CCF(name)  UIN(CC_DLL, name)
#define CCN(name)  ((name##_pfn)t_proc_in(CC_DLL, #name))

/* ---- canned sources ---------------------------------------------------------- */
#define CC_PS_SRC   "float4 main(float4 p : SV_Position) : SV_Target { return p * 0.5f; }"
#define CC_VS_SRC   "float4 main(float4 p : POSITION) : SV_Position { return p; }"
#define CC_PS_LEGACY "float4 main(float4 c : COLOR0) : COLOR0 { return c * 0.5f; }"
#define CC_VS_LEGACY "float4 main(float4 p : POSITION) : POSITION { return p; }"
#define CC_PS_CONST "float4 main() : SV_Target { return float4(1,0,0,1); }"

/* ---- compile result -------------------------------------------------------- */
typedef struct {
    HRESULT hr;
    ID3DBlob *code;
    ID3DBlob *errb;
    char err[8192];
} cc_r;

static inline const unsigned char *cc_ptr(ID3DBlob *b) { return b ? (const unsigned char *)ID3D10Blob_GetBufferPointer(b) : NULL; }
static inline SIZE_T cc_size(ID3DBlob *b) { return b ? ID3D10Blob_GetBufferSize(b) : 0; }

static inline void cc_blobstr(ID3DBlob *b, char *out, size_t cap)
{
    size_t n = cc_size(b);
    out[0] = 0;
    if (!b) return;
    if (n >= cap) n = cap - 1;
    memcpy(out, cc_ptr(b), n);
    out[n] = 0;
}

static inline void cc_free(cc_r *r)
{
    SAFE_RELEASE(r->code);
    SAFE_RELEASE(r->errb);
    r->err[0] = 0;
}

/* Full-control compile through D3DCompile. Returns r->hr. */
static inline HRESULT cc_compile_full(cc_r *r, const char *src, SIZE_T len, const char *name, const D3D_SHADER_MACRO *defs,
                                      ID3DInclude *inc, const char *entry, const char *target, UINT f1, UINT f2)
{
    __typeof__(&D3DCompile) fn = CCF(D3DCompile);
    memset(r, 0, sizeof *r);
    r->hr = fn(src, len, name, defs, inc, entry, target, f1, f2, &r->code, &r->errb);
    cc_blobstr(r->errb, r->err, sizeof r->err);
    return r->hr;
}

static inline HRESULT cc_run(cc_r *r, const char *src, const char *entry, const char *target, UINT f1)
{
    return cc_compile_full(r, src, strlen(src), "t.hlsl", NULL, NULL, entry, target, f1, 0);
}

/* ---- DXBC container helpers -------------------------------------------------- */
#define CC_FOURCC(a, b, c, d) ((DWORD)(a) | ((DWORD)(b) << 8) | ((DWORD)(c) << 16) | ((DWORD)(d) << 24))

static inline DWORD cc_rd32(const unsigned char *p) { DWORD v; memcpy(&v, p, 4); return v; }

/* 1 when the blob is a structurally valid DXBC container whose size field matches n. */
static inline int cc_dxbc_ok(const void *data, SIZE_T n)
{
    const unsigned char *p = (const unsigned char *)data;
    DWORD i, cnt;
    if (!p || n < 32) return 0;
    if (cc_rd32(p) != CC_FOURCC('D', 'X', 'B', 'C')) return 0;
    if (cc_rd32(p + 20) != 1) return 0;
    if (cc_rd32(p + 24) != n) return 0;
    cnt = cc_rd32(p + 28);
    if (32 + (SIZE_T)cnt * 4 > n) return 0;
    for (i = 0; i < cnt; i++) {
        DWORD off = cc_rd32(p + 32 + i * 4), sz;
        if ((SIZE_T)off + 8 > n) return 0;
        sz = cc_rd32(p + off + 4);
        if ((SIZE_T)off + 8 + sz > n) return 0;
    }
    return 1;
}

static inline DWORD cc_nparts(const void *data) { return cc_rd32((const unsigned char *)data + 28); }

/* Returns pointer to the part payload (after the 8 byte part header) or NULL. */
static inline const unsigned char *cc_part(const void *data, SIZE_T n, DWORD fourcc, DWORD *size)
{
    const unsigned char *p = (const unsigned char *)data;
    DWORD i, cnt;
    if (!cc_dxbc_ok(data, n)) return NULL;
    cnt = cc_rd32(p + 28);
    for (i = 0; i < cnt; i++) {
        DWORD off = cc_rd32(p + 32 + i * 4);
        if (cc_rd32(p + off) == fourcc) {
            if (size) *size = cc_rd32(p + off + 4);
            return p + off + 8;
        }
    }
    return NULL;
}

static inline int cc_has_part(ID3DBlob *b, const char *cc4)
{
    return cc_part(cc_ptr(b), cc_size(b), CC_FOURCC(cc4[0], cc4[1], cc4[2], cc4[3]), NULL) != NULL;
}

/* "RDEF ISGN OSGN SHEX STAT" */
static inline void cc_part_list(ID3DBlob *b, char *out, size_t cap)
{
    const unsigned char *p = cc_ptr(b);
    DWORD i, cnt;
    size_t o = 0;
    out[0] = 0;
    if (!cc_dxbc_ok(p, cc_size(b))) return;
    cnt = cc_rd32(p + 28);
    for (i = 0; i < cnt && o + 6 < cap; i++) {
        DWORD off = cc_rd32(p + 32 + i * 4);
        if (i) out[o++] = ' ';
        memcpy(out + o, p + off, 4);
        o += 4;
    }
    out[o] = 0;
}

/* ---- DXBC checksum (MD5 variant used by the D3D runtime/compiler) -------------- */
static inline uint32_t cc_rol(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }
static inline void cc_md5_block(uint32_t st[4], const uint32_t m[16])
{
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
    static const int S[64] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                               5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                               4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                               6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
    uint32_t a = st[0], b = st[1], c = st[2], d = st[3];
    int i;
    for (i = 0; i < 64; i++) {
        uint32_t f, tmp;
        int g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | ~d); g = (7 * i) & 15; }
        tmp = d; d = c; c = b;
        b = b + cc_rol(a + f + K[i] + m[g], S[i]);
        a = tmp;
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
}

/* data/n is the whole container; the hash covers everything after the 20 byte header prefix. */
static inline void cc_dxbc_checksum(const void *data, SIZE_T n, uint32_t out[4])
{
    const unsigned char *p = (const unsigned char *)data + 20;
    uint32_t size = (uint32_t)(n - 20), left = size & 63, i;
    uint32_t st[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    uint32_t blk[16];
    unsigned char *bb = (unsigned char *)blk;
    for (i = 0; i < size - left; i += 64) {
        memcpy(blk, p + i, 64);
        cc_md5_block(st, blk);
    }
    p += size - left;
    if (left >= 56) {
        memset(blk, 0, 64);
        memcpy(bb, p, left);
        bb[left] = 0x80;
        cc_md5_block(st, blk);
        memset(blk, 0, 64);
        blk[0] = size * 8;
        blk[15] = (size * 2) | 1;
        cc_md5_block(st, blk);
    } else {
        memset(blk, 0, 64);
        blk[0] = size * 8;
        memcpy(bb + 4, p, left);
        bb[4 + left] = 0x80;
        blk[15] = (size * 2) | 1;
        cc_md5_block(st, blk);
    }
    memcpy(out, st, 16);
}

/* ---- message parsing ------------------------------------------------------ */
/* Finds "error X####" / "warning X####" in text. Returns 1 when present. */
static inline int cc_has_msg(const char *text, const char *kind, int code)
{
    char pat[48];
    snprintf(pat, sizeof pat, "%s X%d", kind, code);
    return strstr(text, pat) != NULL;
}
static inline int cc_has_error(const char *t, int code) { return cc_has_msg(t, "error", code); }
static inline int cc_has_warning(const char *t, int code) { return cc_has_msg(t, "warning", code); }

/* Validates one diagnostic line "name(line,col[-col]): (error|warning) Xnnnn: text". Returns 1 if conformant. */
static inline int cc_line_format_ok(const char *ln, size_t len, const char *name)
{
    char buf[1024];
    size_t nl = strlen(name);
    const char *p;
    int line = 0, col = 0, n = 0;
    if (len >= sizeof buf) len = sizeof buf - 1;
    memcpy(buf, ln, len); buf[len] = 0;
    if (strncmp(buf, name, nl) || buf[nl] != '(') return 0;
    p = buf + nl + 1;
    if (sscanf(p, "%d,%d%n", &line, &col, &n) < 2) return 0;
    p += n;
    if (*p == '-') { int c2 = 0, m = 0; if (sscanf(p + 1, "%d%n", &c2, &m) < 1) return 0; p += 1 + m; }
    if (strncmp(p, "): ", 3)) return 0;
    p += 3;
    if (!strncmp(p, "error X", 7)) p += 7;
    else if (!strncmp(p, "warning X", 9)) p += 9;
    else return 0;
    {
        int d = 0;
        while (*p >= '0' && *p <= '9') { p++; d++; }
        if (d < 4) return 0;
    }
    return !strncmp(p, ": ", 2);
}

/* Counts lines in text (newline separated, ignoring empty). */
static inline int cc_count_lines(const char *t)
{
    int n = 0;
    const char *s = t;
    while (*s) {
        const char *e = strchr(s, '\n');
        size_t l = e ? (size_t)(e - s) : strlen(s);
        if (l) n++;
        if (!e) break;
        s = e + 1;
    }
    return n;
}

/* ---- ID3DInclude test implementation ------------------------------------------ */
#define CC_MAX_INC 64
typedef struct cc_inc {
    const struct ID3DIncludeVtbl *lpVtbl;
    int n_open, n_close;
    D3D_INCLUDE_TYPE types[CC_MAX_INC];
    char names[CC_MAX_INC][64];
    const void *parents[CC_MAX_INC];
    const void *datas[CC_MAX_INC];
    const void *closed[CC_MAX_INC];
    int order[CC_MAX_INC * 2];    /* +i+1 open of i, -(i+1) close (index into opens) */
    int n_order;
    /* name -> content table */
    const char *file_name[8];
    const char *file_text[8];
    int n_files;
    HRESULT fail_hr;              /* returned when the name is unknown */
    int null_data_ok;
} cc_inc;

static HRESULT STDMETHODCALLTYPE cc_inc_open(ID3DInclude *This, D3D_INCLUDE_TYPE type, const char *name, const void *parent, const void **data, UINT *bytes)
{
    cc_inc *s = (cc_inc *)This;
    int i, idx = s->n_open;
    if (idx < CC_MAX_INC) {
        s->types[idx] = type;
        snprintf(s->names[idx], sizeof s->names[idx], "%s", name ? name : "(null)");
        s->parents[idx] = parent;
        s->order[s->n_order++] = idx + 1;
    }
    s->n_open++;
    for (i = 0; i < s->n_files; i++) {
        if (!strcmp(s->file_name[i], name)) {
            *data = s->file_text[i];
            *bytes = (UINT)strlen(s->file_text[i]);
            if (idx < CC_MAX_INC) s->datas[idx] = *data;
            return S_OK;
        }
    }
    *data = NULL;
    *bytes = 0;
    return s->fail_hr ? s->fail_hr : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE cc_inc_close(ID3DInclude *This, const void *data)
{
    cc_inc *s = (cc_inc *)This;
    int idx = s->n_close++;
    if (idx < CC_MAX_INC) {
        int i;
        s->closed[idx] = data;
        for (i = 0; i < s->n_open && i < CC_MAX_INC; i++)
            if (s->datas[i] == data) { s->order[s->n_order++] = -(i + 1); break; }
    }
    return S_OK;
}

static const struct ID3DIncludeVtbl cc_inc_vtbl = { cc_inc_open, cc_inc_close };

static inline void cc_inc_init(cc_inc *s)
{
    memset(s, 0, sizeof *s);
    s->lpVtbl = &cc_inc_vtbl;
}
static inline void cc_inc_add(cc_inc *s, const char *name, const char *text)
{
    s->file_name[s->n_files] = name;
    s->file_text[s->n_files] = text;
    s->n_files++;
}
static inline int cc_inc_balanced(const cc_inc *s)
{
    int i;
    if (s->n_open != s->n_close) return 0;
    for (i = 0; i < s->n_open && i < CC_MAX_INC; i++)
        if (s->closed[i] == NULL && s->datas[i] != NULL) return 0;
    return 1;
}

#endif
