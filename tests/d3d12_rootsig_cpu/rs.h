/* Shared helpers for the d3d12_rootsig_cpu area.
 * A small, independent model of the serialized root signature container (DXBC wrapper + RTS0 part)
 * is used to build the expected bytes for a given description; the real functions are only reached
 * through GetProcAddress (U()). */
#ifndef RS_H
#define RS_H
#include "d3dh.h"
#include <stdint.h>

#define SER10()   U(D3D12SerializeRootSignature)
#define SERV()    U(D3D12SerializeVersionedRootSignature)
#define CRDES()   U(D3D12CreateRootSignatureDeserializer)
#define CRVDES()  U(D3D12CreateVersionedRootSignatureDeserializer)

/* ---- description builders ------------------------------------------------ */
#define RS_MAXP 72
#define RS_MAXR 160
#define RS_MAXS 24

typedef struct {
    D3D12_ROOT_PARAMETER p[RS_MAXP];
    D3D12_DESCRIPTOR_RANGE r[RS_MAXR];
    D3D12_STATIC_SAMPLER_DESC s[RS_MAXS];
    UINT np, nr, ns;
    UINT first[RS_MAXP];
    D3D12_ROOT_SIGNATURE_DESC d;
} RS0;

typedef struct {
    D3D12_ROOT_PARAMETER1 p[RS_MAXP];
    D3D12_DESCRIPTOR_RANGE1 r[RS_MAXR];
    D3D12_STATIC_SAMPLER_DESC s[RS_MAXS];
    UINT np, nr, ns;
    UINT first[RS_MAXP];
    D3D12_ROOT_SIGNATURE_DESC1 d;
} RS1;

static inline void rs0_init(RS0 *b) { memset(b, 0, sizeof *b); }
static inline void rs1_init(RS1 *b) { memset(b, 0, sizeof *b); }

static inline void rs0_consts(RS0 *b, UINT reg, UINT space, UINT n, D3D12_SHADER_VISIBILITY vis) {
    D3D12_ROOT_PARAMETER *p = &b->p[b->np++];
    p->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p->ShaderVisibility = vis;
    p->Constants.ShaderRegister = reg; p->Constants.RegisterSpace = space; p->Constants.Num32BitValues = n;
}
static inline void rs0_root(RS0 *b, D3D12_ROOT_PARAMETER_TYPE t, UINT reg, UINT space, D3D12_SHADER_VISIBILITY vis) {
    D3D12_ROOT_PARAMETER *p = &b->p[b->np++];
    p->ParameterType = t; p->ShaderVisibility = vis;
    p->Descriptor.ShaderRegister = reg; p->Descriptor.RegisterSpace = space;
}
static inline void rs0_table(RS0 *b, D3D12_SHADER_VISIBILITY vis) {
    D3D12_ROOT_PARAMETER *p = &b->p[b->np]; b->first[b->np] = b->nr; b->np++;
    p->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; p->ShaderVisibility = vis;
    p->DescriptorTable.NumDescriptorRanges = 0;
}
static inline void rs0_range(RS0 *b, D3D12_DESCRIPTOR_RANGE_TYPE t, UINT n, UINT reg, UINT space, UINT off) {
    D3D12_DESCRIPTOR_RANGE *r = &b->r[b->nr++];
    r->RangeType = t; r->NumDescriptors = n; r->BaseShaderRegister = reg; r->RegisterSpace = space;
    r->OffsetInDescriptorsFromTableStart = off;
    b->p[b->np - 1].DescriptorTable.NumDescriptorRanges++;
}
static inline D3D12_STATIC_SAMPLER_DESC *rs0_sampler(RS0 *b, UINT reg, UINT space, D3D12_SHADER_VISIBILITY vis) {
    D3D12_STATIC_SAMPLER_DESC *s = &b->s[b->ns++];
    memset(s, 0, sizeof *s);
    s->Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; s->AddressU = s->AddressV = s->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; s->BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    s->MinLOD = 0.0f; s->MaxLOD = 3.402823466e+38f;
    s->ShaderRegister = reg; s->RegisterSpace = space; s->ShaderVisibility = vis;
    return s;
}
static inline const D3D12_ROOT_SIGNATURE_DESC *rs0_done(RS0 *b, UINT flags) {
    UINT i;
    for (i = 0; i < b->np; i++)
        if (b->p[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && b->p[i].DescriptorTable.NumDescriptorRanges)
            b->p[i].DescriptorTable.pDescriptorRanges = &b->r[b->first[i]];
    b->d.NumParameters = b->np; b->d.pParameters = b->np ? b->p : NULL;
    b->d.NumStaticSamplers = b->ns; b->d.pStaticSamplers = b->ns ? b->s : NULL;
    b->d.Flags = flags;
    return &b->d;
}

static inline void rs1_consts(RS1 *b, UINT reg, UINT space, UINT n, D3D12_SHADER_VISIBILITY vis) {
    D3D12_ROOT_PARAMETER1 *p = &b->p[b->np++];
    p->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p->ShaderVisibility = vis;
    p->Constants.ShaderRegister = reg; p->Constants.RegisterSpace = space; p->Constants.Num32BitValues = n;
}
static inline void rs1_root(RS1 *b, D3D12_ROOT_PARAMETER_TYPE t, UINT reg, UINT space, UINT flags, D3D12_SHADER_VISIBILITY vis) {
    D3D12_ROOT_PARAMETER1 *p = &b->p[b->np++];
    p->ParameterType = t; p->ShaderVisibility = vis;
    p->Descriptor.ShaderRegister = reg; p->Descriptor.RegisterSpace = space; p->Descriptor.Flags = flags;
}
static inline void rs1_table(RS1 *b, D3D12_SHADER_VISIBILITY vis) {
    D3D12_ROOT_PARAMETER1 *p = &b->p[b->np]; b->first[b->np] = b->nr; b->np++;
    p->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; p->ShaderVisibility = vis;
    p->DescriptorTable.NumDescriptorRanges = 0;
}
static inline void rs1_range(RS1 *b, D3D12_DESCRIPTOR_RANGE_TYPE t, UINT n, UINT reg, UINT space, UINT flags, UINT off) {
    D3D12_DESCRIPTOR_RANGE1 *r = &b->r[b->nr++];
    r->RangeType = t; r->NumDescriptors = n; r->BaseShaderRegister = reg; r->RegisterSpace = space;
    r->Flags = flags; r->OffsetInDescriptorsFromTableStart = off;
    b->p[b->np - 1].DescriptorTable.NumDescriptorRanges++;
}
static inline D3D12_STATIC_SAMPLER_DESC *rs1_sampler(RS1 *b, UINT reg, UINT space, D3D12_SHADER_VISIBILITY vis) {
    D3D12_STATIC_SAMPLER_DESC *s = &b->s[b->ns++];
    memset(s, 0, sizeof *s);
    s->Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; s->AddressU = s->AddressV = s->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; s->BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    s->MinLOD = 0.0f; s->MaxLOD = 3.402823466e+38f;
    s->ShaderRegister = reg; s->RegisterSpace = space; s->ShaderVisibility = vis;
    return s;
}
static inline const D3D12_ROOT_SIGNATURE_DESC1 *rs1_done(RS1 *b, UINT flags) {
    UINT i;
    for (i = 0; i < b->np; i++)
        if (b->p[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && b->p[i].DescriptorTable.NumDescriptorRanges)
            b->p[i].DescriptorTable.pDescriptorRanges = &b->r[b->first[i]];
    b->d.NumParameters = b->np; b->d.pParameters = b->np ? b->p : NULL;
    b->d.NumStaticSamplers = b->ns; b->d.pStaticSamplers = b->ns ? b->s : NULL;
    b->d.Flags = flags;
    return &b->d;
}

/* ---- v1.2 (Agility SDK) structures, declared locally: the SDK headers of the toolchain stop at 1.1 ---- */
typedef struct {
    UINT Filter, AddressU, AddressV, AddressW; float MipLODBias; UINT MaxAnisotropy; UINT ComparisonFunc;
    UINT BorderColor; float MinLOD, MaxLOD; UINT ShaderRegister, RegisterSpace, ShaderVisibility; UINT Flags;
} RS_STATIC_SAMPLER_DESC1;
typedef struct {
    UINT NumParameters; const D3D12_ROOT_PARAMETER1 *pParameters;
    UINT NumStaticSamplers; const RS_STATIC_SAMPLER_DESC1 *pStaticSamplers; UINT Flags;
} RS_ROOT_SIGNATURE_DESC2;
typedef struct { UINT Version; RS_ROOT_SIGNATURE_DESC2 Desc_1_2; } RS_VERSIONED_DESC2;

/* ---- independent model of the serialized form ---------------------------- */
typedef struct { uint32_t w[8192]; size_t n; /* dwords */ } RSBUF;

static inline size_t rs_model0(const D3D12_ROOT_SIGNATURE_DESC *d, RSBUF *o) {
    uint32_t *w = o->w; UINT i, k; size_t pos = 6;
    w[0] = 1; w[1] = d->NumParameters; w[2] = 24; w[3] = d->NumStaticSamplers; w[5] = d->Flags;
    pos += 3 * d->NumParameters;
    for (i = 0; i < d->NumParameters; i++) {
        const D3D12_ROOT_PARAMETER *p = &d->pParameters[i];
        w[6 + 3 * i] = p->ParameterType; w[7 + 3 * i] = p->ShaderVisibility; w[8 + 3 * i] = (uint32_t)(pos * 4);
        switch (p->ParameterType) {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            w[pos++] = p->DescriptorTable.NumDescriptorRanges; w[pos] = (uint32_t)((pos + 1) * 4); pos++;
            for (k = 0; k < p->DescriptorTable.NumDescriptorRanges; k++) {
                const D3D12_DESCRIPTOR_RANGE *r = &p->DescriptorTable.pDescriptorRanges[k];
                w[pos++] = r->RangeType; w[pos++] = r->NumDescriptors; w[pos++] = r->BaseShaderRegister;
                w[pos++] = r->RegisterSpace; w[pos++] = r->OffsetInDescriptorsFromTableStart;
            }
            break;
        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            w[pos++] = p->Constants.ShaderRegister; w[pos++] = p->Constants.RegisterSpace; w[pos++] = p->Constants.Num32BitValues;
            break;
        default:
            w[pos++] = p->Descriptor.ShaderRegister; w[pos++] = p->Descriptor.RegisterSpace;
            break;
        }
    }
    w[4] = (uint32_t)(pos * 4);
    if (d->NumStaticSamplers) { memcpy(&w[pos], d->pStaticSamplers, 52u * d->NumStaticSamplers); pos += 13u * d->NumStaticSamplers; }
    o->n = pos;
    return pos * 4;
}

static inline size_t rs_model1(const D3D12_ROOT_SIGNATURE_DESC1 *d, RSBUF *o) {
    uint32_t *w = o->w; UINT i, k; size_t pos = 6;
    w[0] = 2; w[1] = d->NumParameters; w[2] = 24; w[3] = d->NumStaticSamplers; w[5] = d->Flags;
    pos += 3 * d->NumParameters;
    for (i = 0; i < d->NumParameters; i++) {
        const D3D12_ROOT_PARAMETER1 *p = &d->pParameters[i];
        w[6 + 3 * i] = p->ParameterType; w[7 + 3 * i] = p->ShaderVisibility; w[8 + 3 * i] = (uint32_t)(pos * 4);
        switch (p->ParameterType) {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            w[pos++] = p->DescriptorTable.NumDescriptorRanges; w[pos] = (uint32_t)((pos + 1) * 4); pos++;
            for (k = 0; k < p->DescriptorTable.NumDescriptorRanges; k++) {
                const D3D12_DESCRIPTOR_RANGE1 *r = &p->DescriptorTable.pDescriptorRanges[k];
                w[pos++] = r->RangeType; w[pos++] = r->NumDescriptors; w[pos++] = r->BaseShaderRegister;
                w[pos++] = r->RegisterSpace; w[pos++] = r->Flags; w[pos++] = r->OffsetInDescriptorsFromTableStart;
            }
            break;
        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            w[pos++] = p->Constants.ShaderRegister; w[pos++] = p->Constants.RegisterSpace; w[pos++] = p->Constants.Num32BitValues;
            break;
        default:
            w[pos++] = p->Descriptor.ShaderRegister; w[pos++] = p->Descriptor.RegisterSpace; w[pos++] = p->Descriptor.Flags;
            break;
        }
    }
    w[4] = (uint32_t)(pos * 4);
    if (d->NumStaticSamplers) { memcpy(&w[pos], d->pStaticSamplers, 52u * d->NumStaticSamplers); pos += 13u * d->NumStaticSamplers; }
    o->n = pos;
    return pos * 4;
}

/* ---- DXBC container: checksum, wrap, parse ---------------------------------- */
static inline uint32_t rs_rol(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }
static inline void rs_md5_block(uint32_t st[4], const uint32_t m[16]) {
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 };
    static const int S[64] = { 7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22, 5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                               4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23, 6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21 };
    uint32_t a = st[0], b = st[1], c = st[2], d = st[3]; int i;
    for (i = 0; i < 64; i++) {
        uint32_t f; int g;
        if (i < 16)      { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d;          g = (3 * i + 5) & 15; }
        else             { f = c ^ (b | ~d);       g = (7 * i) & 15; }
        f = f + a + K[i] + m[g]; a = d; d = c; c = b; b = b + rs_rol(f, S[i]);
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
}
/* DXBC container hash: MD5 variant over everything after the 20 byte header prefix */
static inline void rs_dxbc_hash(const uint8_t *dxbc, size_t size, uint8_t out[16]) {
    uint32_t st[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    const uint8_t *p = dxbc + 20; size_t n = size - 20, full = n & ~(size_t)63, left = n - full, i;
    uint32_t m[16]; uint8_t tmp[64];
    for (i = 0; i < full; i += 64) { memcpy(m, p + i, 64); rs_md5_block(st, m); }
    memset(tmp, 0, 64);
    if (left >= 56) {
        memcpy(tmp, p + full, left); tmp[left] = 0x80; memcpy(m, tmp, 64); rs_md5_block(st, m);
        memset(m, 0, sizeof m); m[0] = (uint32_t)(n * 8); m[15] = (uint32_t)(n * 2) | 1; rs_md5_block(st, m);
    } else {
        memset(m, 0, sizeof m); m[0] = (uint32_t)(n * 8);
        memcpy(tmp, p + full, left); tmp[left] = 0x80; memcpy(&m[1], tmp, 56); m[15] = (uint32_t)(n * 2) | 1;
        rs_md5_block(st, m);
    }
    memcpy(out, st, 16);
}
/* wrap an RTS0 payload in a one part DXBC container (with a correct hash when fix_hash) */
static inline size_t rs_dxbc_wrap(const void *rts, size_t n, uint8_t *out, int fix_hash) {
    uint32_t *h = (uint32_t *)out; size_t total = 32 + 4 + 8 + n;
    memset(out, 0, 32);
    memcpy(out, "DXBC", 4); h[5] = 1; h[6] = (uint32_t)total; h[7] = 1; h[8] = 36;
    memcpy(out + 36, "RTS0", 4); h[10] = (uint32_t)n; memcpy(out + 44, rts, n);
    if (fix_hash) rs_dxbc_hash(out, total, out + 4);
    return total;
}
/* find the RTS0 payload of a container; returns NULL when the container is malformed */
static inline const uint8_t *rs_find_rts0(const uint8_t *b, size_t size, size_t *rts_size) {
    uint32_t cnt, i;
    if (size < 36 || memcmp(b, "DXBC", 4)) return NULL;
    memcpy(&cnt, b + 28, 4);
    if (cnt > 16 || size < 32 + 4 * (size_t)cnt) return NULL;
    for (i = 0; i < cnt; i++) {
        uint32_t off, sz;
        memcpy(&off, b + 32 + 4 * i, 4);
        if (off + 8 > size) return NULL;
        memcpy(&sz, b + off + 4, 4);
        if (!memcmp(b + off, "RTS0", 4)) { if (off + 8 + (size_t)sz > size) return NULL; *rts_size = sz; return b + off + 8; }
    }
    return NULL;
}

/* Compare a blob returned by the DLL with the model bytes; every difference is a failure. */
static inline int rs_blob_matches(ID3DBlob *blob, const RSBUF *m, const char *tag) {
    const uint8_t *p; size_t n, rn, mb = m->n * 4; const uint8_t *r; uint8_t hash[16]; int ok = 1;
    if (!blob) { CHECKF(0, "%s: no blob", tag); return 0; }
    p = ID3D10Blob_GetBufferPointer(blob); n = ID3D10Blob_GetBufferSize(blob);
    CHECKF(n == 32 + 4 + 8 + mb, "%s: blob size %u, want %u", tag, (unsigned)n, (unsigned)(44 + mb));
    if (n < 44) return 0;
    r = rs_find_rts0(p, n, &rn);
    CHECKF(r != NULL, "%s: no RTS0 part", tag);
    if (!r) return 0;
    CHECKF(rn == mb, "%s: RTS0 size %u, want %u", tag, (unsigned)rn, (unsigned)mb);
    if (rn == mb && memcmp(r, m->w, mb)) {
        size_t i; for (i = 0; i < mb; i++) if (r[i] != ((const uint8_t *)m->w)[i]) break;
        CHECKF(0, "%s: RTS0 differs at byte %u (got %02x want %02x)", tag, (unsigned)i, r[i], ((const uint8_t *)m->w)[i]);
        ok = 0;
    }
    if (n >= 36 + 8) {
        uint32_t t, c, off;
        memcpy(&t, p + 24, 4); memcpy(&c, p + 28, 4); memcpy(&off, p + 32, 4);
        CHECKF(t == n, "%s: container size field %u vs %u", tag, t, (unsigned)n);
        CHECKF(c == 1, "%s: part count %u", tag, c);
        CHECKF(off == 36, "%s: part offset %u", tag, off);
        CHECKF(*(const uint32_t *)(p + 20) == 1, "%s: container version dword", tag);
        rs_dxbc_hash(p, n, hash);
        CHECKF(!memcmp(hash, p + 4, 16), "%s: container digest mismatch", tag);
    }
    return ok;
}

static inline const char *rs_errstr(ID3DBlob *e) { return e ? (const char *)ID3D10Blob_GetBufferPointer(e) : NULL; }

/* serialize helper variants: return HRESULT, release nothing (caller owns *b and *e) */
static inline HRESULT rs_ser0(const D3D12_ROOT_SIGNATURE_DESC *d, ID3DBlob **b, ID3DBlob **e) {
    *b = NULL; if (e) *e = NULL;
    return SER10()(d, D3D_ROOT_SIGNATURE_VERSION_1_0, b, e);
}
static inline HRESULT rs_serv0(const D3D12_ROOT_SIGNATURE_DESC *d, ID3DBlob **b, ID3DBlob **e) {
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
    v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0; v.Desc_1_0 = *d; *b = NULL; if (e) *e = NULL;
    return SERV()(&v, b, e);
}
static inline HRESULT rs_serv1(const D3D12_ROOT_SIGNATURE_DESC1 *d, ID3DBlob **b, ID3DBlob **e) {
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
    v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1 = *d; *b = NULL; if (e) *e = NULL;
    return SERV()(&v, b, e);
}

/* need all four entry points; skip otherwise */
/* compare a value that comes from a call with side effects (AddRef/Release): evaluated exactly once */
#define EQV(call, want) do { ULONG_PTR g_ = (ULONG_PTR)(call), w_ = (ULONG_PTR)(want); \
    t_check(g_ == w_, __FILE__, __LINE__, #call " == " #want, "got %llu want %llu", (unsigned long long)g_, (unsigned long long)w_); } while (0)

#define RS_REQUIRE_ALL() do { REQUIRE(SER10()); REQUIRE(SERV()); REQUIRE(CRDES()); REQUIRE(CRVDES()); } while (0)


/* ---- description comparison (field by field, pointers followed) ----------- */
static inline int rs_samplers_eq(const D3D12_STATIC_SAMPLER_DESC *a, const D3D12_STATIC_SAMPLER_DESC *b, UINT n) {
    return n == 0 || (a && b && !memcmp(a, b, sizeof(*a) * n));
}
static inline int rs_desc_eq0(const D3D12_ROOT_SIGNATURE_DESC *a, const D3D12_ROOT_SIGNATURE_DESC *b) {
    UINT i, k;
    if (a->NumParameters != b->NumParameters || a->NumStaticSamplers != b->NumStaticSamplers || a->Flags != b->Flags) return 0;
    for (i = 0; i < a->NumParameters; i++) {
        const D3D12_ROOT_PARAMETER *p = &a->pParameters[i], *q = &b->pParameters[i];
        if (p->ParameterType != q->ParameterType || p->ShaderVisibility != q->ShaderVisibility) return 0;
        if (p->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            if (p->DescriptorTable.NumDescriptorRanges != q->DescriptorTable.NumDescriptorRanges) return 0;
            for (k = 0; k < p->DescriptorTable.NumDescriptorRanges; k++)
                if (memcmp(&p->DescriptorTable.pDescriptorRanges[k], &q->DescriptorTable.pDescriptorRanges[k], sizeof(D3D12_DESCRIPTOR_RANGE))) return 0;
        } else if (p->ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
            if (memcmp(&p->Constants, &q->Constants, sizeof p->Constants)) return 0;
        } else if (memcmp(&p->Descriptor, &q->Descriptor, sizeof p->Descriptor)) return 0;
    }
    return rs_samplers_eq(a->pStaticSamplers, b->pStaticSamplers, a->NumStaticSamplers);
}
static inline int rs_desc_eq1(const D3D12_ROOT_SIGNATURE_DESC1 *a, const D3D12_ROOT_SIGNATURE_DESC1 *b) {
    UINT i, k;
    if (a->NumParameters != b->NumParameters || a->NumStaticSamplers != b->NumStaticSamplers || a->Flags != b->Flags) return 0;
    for (i = 0; i < a->NumParameters; i++) {
        const D3D12_ROOT_PARAMETER1 *p = &a->pParameters[i], *q = &b->pParameters[i];
        if (p->ParameterType != q->ParameterType || p->ShaderVisibility != q->ShaderVisibility) return 0;
        if (p->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            if (p->DescriptorTable.NumDescriptorRanges != q->DescriptorTable.NumDescriptorRanges) return 0;
            for (k = 0; k < p->DescriptorTable.NumDescriptorRanges; k++)
                if (memcmp(&p->DescriptorTable.pDescriptorRanges[k], &q->DescriptorTable.pDescriptorRanges[k], sizeof(D3D12_DESCRIPTOR_RANGE1))) return 0;
        } else if (p->ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
            if (memcmp(&p->Constants, &q->Constants, sizeof p->Constants)) return 0;
        } else if (memcmp(&p->Descriptor, &q->Descriptor, sizeof p->Descriptor)) return 0;
    }
    return rs_samplers_eq(a->pStaticSamplers, b->pStaticSamplers, a->NumStaticSamplers);
}

/* Win10 22H2: the serializer accepts a descriptor table with zero ranges, but both deserializers then answer E_INVALIDARG and corrupt the
   heap (0xc0000374 / 0xc0000005 in later allocations, 3 of 4 runs). Round trips are therefore skipped for such descriptions. */
static inline int rs_has_empty_table0(const D3D12_ROOT_SIGNATURE_DESC *d) {
    UINT i; for (i = 0; i < d->NumParameters; i++) if (d->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && !d->pParameters[i].DescriptorTable.NumDescriptorRanges) return 1;
    return 0;
}
static inline int rs_has_empty_table1(const D3D12_ROOT_SIGNATURE_DESC1 *d) {
    UINT i; for (i = 0; i < d->NumParameters; i++) if (d->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && !d->pParameters[i].DescriptorTable.NumDescriptorRanges) return 1;
    return 0;
}
/* serialize and fully check one v1.0 description: bytes, determinism, both entry points, round trip */
static inline void rs_case0(const D3D12_ROOT_SIGNATURE_DESC *d, const char *tag) {
    RSBUF m; ID3DBlob *b = NULL, *e = NULL, *b2 = NULL, *e2 = NULL; HRESULT hr;
    ID3D12RootSignatureDeserializer *des = NULL;
    rs_model0(d, &m);
    hr = rs_ser0(d, &b, &e); EXPECT_HR(hr, S_OK); CHECKF(e == NULL, "%s: error blob on success", tag);
    if (SUCCEEDED(hr) && b) {
        rs_blob_matches(b, &m, tag);
        hr = rs_serv0(d, &b2, &e2); EXPECT_HR(hr, S_OK);
        if (b2) {
            CHECKF(ID3D10Blob_GetBufferSize(b) == ID3D10Blob_GetBufferSize(b2) &&
                   !memcmp(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferPointer(b2), ID3D10Blob_GetBufferSize(b)),
                   "%s: versioned and plain v1.0 serialization differ", tag);
        }
        if (rs_has_empty_table0(d)) goto done0;
        hr = CRDES()(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &IID_ID3D12RootSignatureDeserializer, (void **)&des);
        EXPECT_HR(hr, S_OK);
        if (des) {
            const D3D12_ROOT_SIGNATURE_DESC *g = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(des);
            CHECKF(g && rs_desc_eq0(d, g), "%s: deserialized description differs", tag);
            ID3D12RootSignatureDeserializer_Release(des);
        }
    }
done0:
    SAFE_RELEASE(b); SAFE_RELEASE(b2); SAFE_RELEASE(e); SAFE_RELEASE(e2);
}
/* same for v1.1 descriptions (only the versioned entry point exists) */
static inline void rs_case1(const D3D12_ROOT_SIGNATURE_DESC1 *d, const char *tag) {
    RSBUF m; ID3DBlob *b = NULL, *e = NULL, *b2 = NULL; HRESULT hr;
    ID3D12VersionedRootSignatureDeserializer *des = NULL;
    rs_model1(d, &m);
    hr = rs_serv1(d, &b, &e); EXPECT_HR(hr, S_OK); CHECKF(e == NULL, "%s: error blob on success", tag);
    if (SUCCEEDED(hr) && b) {
        rs_blob_matches(b, &m, tag);
        hr = rs_serv1(d, &b2, NULL); EXPECT_HR(hr, S_OK);
        if (b2) {
            CHECKF(ID3D10Blob_GetBufferSize(b) == ID3D10Blob_GetBufferSize(b2) &&
                   !memcmp(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferPointer(b2), ID3D10Blob_GetBufferSize(b)),
                   "%s: two serializations differ", tag);
        }
        if (rs_has_empty_table1(d)) goto done1;
        hr = CRVDES()(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&des);
        EXPECT_HR(hr, S_OK);
        if (des) {
            const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(des);
            CHECKF(g && g->Version == D3D_ROOT_SIGNATURE_VERSION_1_1 && rs_desc_eq1(d, &g->Desc_1_1), "%s: deserialized description differs", tag);
            ID3D12VersionedRootSignatureDeserializer_Release(des);
        }
    }
done1:
    SAFE_RELEASE(b); SAFE_RELEASE(b2); SAFE_RELEASE(e);
}

#endif
