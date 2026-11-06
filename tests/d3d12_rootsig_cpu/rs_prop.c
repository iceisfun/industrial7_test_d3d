/* Property tests: pseudo-random, valid root signature descriptions must serialize to exactly the modelled bytes, deserialize back
 * to an equal description, and behave identically from several threads. Fixed seeds: the tests are fully deterministic. */
#include "rs.h"

typedef struct { uint32_t s; } prng;
static uint32_t rnd(prng *r) { uint32_t x = r->s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return r->s = x; }
static uint32_t rn(prng *r, uint32_t n) { return rnd(r) % n; }

static const D3D12_SHADER_VISIBILITY k_vis[] = { D3D12_SHADER_VISIBILITY_ALL, D3D12_SHADER_VISIBILITY_VERTEX, D3D12_SHADER_VISIBILITY_HULL,
    D3D12_SHADER_VISIBILITY_DOMAIN, D3D12_SHADER_VISIBILITY_GEOMETRY, D3D12_SHADER_VISIBILITY_PIXEL };
static const UINT k_rootflags[] = { 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x100, 0x200 };   /* every legal bit except LOCAL (needs ALL visibility) */
static const UINT k_rangeflags[] = { 0, 1, 2, 3, 4, 8, 0x10000 };
static const UINT k_rootdescflags[] = { 0, 2, 4, 8 };

/* every shader register is used once, so no overlap rule can fire */
typedef struct { UINT reg[4][4]; int closed[3]; } regs;   /* closed: the unbounded SRV/UAV/CBV range of space 3 was already placed */      /* [type: 0 SRV 1 UAV 2 CBV 3 SAMPLER][space] */
static UINT next_reg(regs *g, int type, UINT space, UINT n) { UINT r = g->reg[type][space]; g->reg[type][space] += n; return r; }

static void fill_sampler(prng *r, D3D12_STATIC_SAMPLER_DESC *s, regs *g)
{
    static const D3D12_FILTER f[] = { D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_FILTER_ANISOTROPIC,
        D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT, D3D12_FILTER_MAXIMUM_ANISOTROPIC };
    UINT space = rn(r, 3);
    memset(s, 0, sizeof *s);
    s->Filter = f[rn(r, 6)];
    s->AddressU = (D3D12_TEXTURE_ADDRESS_MODE)(1 + rn(r, 5)); s->AddressV = (D3D12_TEXTURE_ADDRESS_MODE)(1 + rn(r, 5)); s->AddressW = (D3D12_TEXTURE_ADDRESS_MODE)(1 + rn(r, 5));
    s->MipLODBias = (float)((int)rn(r, 33) - 16) * 0.5f; s->MaxAnisotropy = rn(r, 17);
    s->ComparisonFunc = (D3D12_COMPARISON_FUNC)(1 + rn(r, 8)); s->BorderColor = (D3D12_STATIC_BORDER_COLOR)rn(r, 3);
    s->MinLOD = (float)rn(r, 4); s->MaxLOD = s->MinLOD + (float)rn(r, 8);
    s->ShaderRegister = next_reg(g, 3, space, 1); s->RegisterSpace = space; s->ShaderVisibility = k_vis[rn(r, 6)];
}

static void gen0(RS0 *b, uint32_t seed)
{
    prng r = { seed * 2654435761u + 12345u }; regs g; UINT np = rn(&r, 7), ns = rn(&r, 4), i, k;
    memset(&g, 0, sizeof g); rs0_init(b);
    for (i = 0; i < np; i++) {
        D3D12_SHADER_VISIBILITY vis = k_vis[rn(&r, 6)]; UINT space = rn(&r, 3);
        switch (rn(&r, 4)) {
        case 0: rs0_consts(b, next_reg(&g, 2, space, 1), space, 1 + rn(&r, 64), vis); break;
        case 1: { int t = rn(&r, 3); rs0_root(b, t == 0 ? D3D12_ROOT_PARAMETER_TYPE_SRV : t == 1 ? D3D12_ROOT_PARAMETER_TYPE_UAV : D3D12_ROOT_PARAMETER_TYPE_CBV, next_reg(&g, t == 0 ? 0 : t == 1 ? 1 : 2, space, 1), space, vis); break; }
        case 2: {
            UINT nr = 1 + rn(&r, 4); rs0_table(b, vis);
            for (k = 0; k < nr; k++) {
                int t = rn(&r, 3); UINT n = 1 + rn(&r, 8); int last = (k == nr - 1); UINT rspace = space;
                if (last && rn(&r, 6) == 0 && !g.closed[t]) { n = 0xffffffffu; rspace = 3; g.closed[t] = 1; }   /* unbounded: own space, nothing else of that type there */
                rs0_range(b, t == 0 ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : t == 1 ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV : D3D12_DESCRIPTOR_RANGE_TYPE_CBV, n,
                          next_reg(&g, t, rspace, n == 0xffffffffu ? 1 : n), rspace, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
            }
            break; }
        default: {
            UINT nr = 1 + rn(&r, 3); rs0_table(b, vis);
            for (k = 0; k < nr; k++) { UINT n = 1 + rn(&r, 4); rs0_range(b, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, n, next_reg(&g, 3, space, n), space, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); }
            break; }
        }
    }
    for (i = 0; i < ns; i++) fill_sampler(&r, &b->s[b->ns++], &g);
    rs0_done(b, rn(&r, 2) ? k_rootflags[rn(&r, 9)] | (rn(&r, 2) ? k_rootflags[rn(&r, 9)] : 0) : 0);
}

static void gen1(RS1 *b, uint32_t seed)
{
    prng r = { seed * 2246822519u + 777u }; regs g; UINT np = rn(&r, 7), ns = rn(&r, 4), i, k;
    memset(&g, 0, sizeof g); rs1_init(b);
    for (i = 0; i < np; i++) {
        D3D12_SHADER_VISIBILITY vis = k_vis[rn(&r, 6)]; UINT space = rn(&r, 3);
        switch (rn(&r, 4)) {
        case 0: rs1_consts(b, next_reg(&g, 2, space, 1), space, 1 + rn(&r, 64), vis); break;
        case 1: { int t = rn(&r, 3); rs1_root(b, t == 0 ? D3D12_ROOT_PARAMETER_TYPE_SRV : t == 1 ? D3D12_ROOT_PARAMETER_TYPE_UAV : D3D12_ROOT_PARAMETER_TYPE_CBV, next_reg(&g, t, space, 1), space, k_rootdescflags[rn(&r, 4)], vis); break; }
        case 2: {
            UINT nr = 1 + rn(&r, 4); rs1_table(b, vis);
            for (k = 0; k < nr; k++) {
                int t = rn(&r, 3); UINT n = 1 + rn(&r, 8); int last = (k == nr - 1); UINT rspace = space;
                if (last && rn(&r, 6) == 0 && !g.closed[t]) { n = 0xffffffffu; rspace = 3; g.closed[t] = 1; }   /* unbounded: own space, nothing else of that type there */
                rs1_range(b, t == 0 ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : t == 1 ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV : D3D12_DESCRIPTOR_RANGE_TYPE_CBV, n,
                          next_reg(&g, t, rspace, n == 0xffffffffu ? 1 : n), rspace, k_rangeflags[rn(&r, 7)], D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
            }
            break; }
        default: {
            UINT nr = 1 + rn(&r, 3); rs1_table(b, vis);
            for (k = 0; k < nr; k++) { UINT n = 1 + rn(&r, 4); rs1_range(b, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, n, next_reg(&g, 3, space, n), space, rn(&r, 2), D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); }
            break; }
        }
    }
    for (i = 0; i < ns; i++) fill_sampler(&r, &b->s[b->ns++], &g);
    rs1_done(b, rn(&r, 2) ? k_rootflags[rn(&r, 9)] : 0);
}

/* one seed, returns the number of problems found (each is also reported with CHECKF when report is set) */
static int check0(uint32_t seed, int report)
{
    RS0 *b = calloc(1, sizeof *b); RSBUF *m = calloc(1, sizeof *m); ID3DBlob *bl = NULL, *e = NULL; int bad = 0;
    gen0(b, seed); rs_model0(&b->d, m);
    HRESULT hr = rs_ser0(&b->d, &bl, &e);
    if (hr != S_OK || !bl) { bad++; if (report) CHECKF(0, "v1.0 seed %u: serialize %s: %s", seed, d3dh_hr_name(hr), e ? rs_errstr(e) : ""); }
    else {
        const uint8_t *p = ID3D10Blob_GetBufferPointer(bl); size_t n = ID3D10Blob_GetBufferSize(bl), rn_ = 0; const uint8_t *rt = rs_find_rts0(p, n, &rn_);
        if (!rt || rn_ != m->n * 4 || memcmp(rt, m->w, rn_)) { bad++; if (report) CHECKF(0, "v1.0 seed %u: bytes differ from the model", seed); }
        if (!rs_has_empty_table0(&b->d)) {
            ID3D12RootSignatureDeserializer *des = NULL;
            hr = CRDES()(p, n, &IID_ID3D12RootSignatureDeserializer, (void **)&des);
            if (hr != S_OK || !des) { bad++; if (report) CHECKF(0, "v1.0 seed %u: deserialize %s", seed, d3dh_hr_name(hr)); }
            else {
                const D3D12_ROOT_SIGNATURE_DESC *g = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(des);
                if (!g || !rs_desc_eq0(&b->d, g)) { bad++; if (report) CHECKF(0, "v1.0 seed %u: round trip differs", seed); }
                ID3D12RootSignatureDeserializer_Release(des);
            }
        }
    }
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b); free(m);
    return bad;
}
static int check1(uint32_t seed, int report)
{
    RS1 *b = calloc(1, sizeof *b); RSBUF *m = calloc(1, sizeof *m); ID3DBlob *bl = NULL, *e = NULL; int bad = 0;
    gen1(b, seed); rs_model1(&b->d, m);
    HRESULT hr = rs_serv1(&b->d, &bl, &e);
    if (hr != S_OK || !bl) { bad++; if (report) CHECKF(0, "v1.1 seed %u: serialize %s: %s", seed, d3dh_hr_name(hr), e ? rs_errstr(e) : ""); }
    else {
        const uint8_t *p = ID3D10Blob_GetBufferPointer(bl); size_t n = ID3D10Blob_GetBufferSize(bl), rn_ = 0; const uint8_t *rt = rs_find_rts0(p, n, &rn_);
        if (!rt || rn_ != m->n * 4 || memcmp(rt, m->w, rn_)) { bad++; if (report) CHECKF(0, "v1.1 seed %u: bytes differ from the model", seed); }
        if (!rs_has_empty_table1(&b->d)) {
            ID3D12VersionedRootSignatureDeserializer *des = NULL;
            hr = CRVDES()(p, n, &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&des);
            if (hr != S_OK || !des) { bad++; if (report) CHECKF(0, "v1.1 seed %u: deserialize %s", seed, d3dh_hr_name(hr)); }
            else {
                const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(des);
                if (!g || g->Version != D3D_ROOT_SIGNATURE_VERSION_1_1 || !rs_desc_eq1(&b->d, &g->Desc_1_1)) { bad++; if (report) CHECKF(0, "v1.1 seed %u: round trip differs", seed); }
                ID3D12VersionedRootSignatureDeserializer_Release(des);
            }
        }
    }
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b); free(m);
    return bad;
}

TEST(d3d12_rootsig_cpu_prop_random_v10_serialize_roundtrip)
{
    RS_REQUIRE_ALL(); uint32_t s;
    for (s = 1; s <= 400; s++) check0(s, 1);
}
TEST(d3d12_rootsig_cpu_prop_random_v11_serialize_roundtrip)
{
    RS_REQUIRE_ALL(); uint32_t s;
    for (s = 1; s <= 400; s++) check1(s, 1);
}

typedef struct { uint32_t first, count; volatile LONG bad; } prop_thr;
static DWORD WINAPI prop_worker(LPVOID p)
{
    prop_thr *t = p; uint32_t s;
    for (s = t->first; s < t->first + t->count; s++) InterlockedAdd(&t->bad, check0(s, 0) + check1(s, 0));
    return 0;
}
TEST(d3d12_rootsig_cpu_prop_concurrent_serialize_deserialize)
{
    RS_REQUIRE_ALL(); prop_thr t[4]; HANDLE h[4]; int i;
    for (i = 0; i < 4; i++) { t[i].first = 1000 + 100 * i; t[i].count = 100; t[i].bad = 0; h[i] = CreateThread(NULL, 0, prop_worker, &t[i], 0, NULL); }
    WaitForMultipleObjects(4, h, TRUE, 120000);
    for (i = 0; i < 4; i++) { if (h[i]) CloseHandle(h[i]); CHECKF(t[i].bad == 0, "thread %d: %ld mismatches", i, (long)t[i].bad); }
}

TEST(d3d12_rootsig_cpu_prop_deserializer_object_semantics)
{
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL; ID3D12RootSignatureDeserializer *d = NULL;
    gen0(b, 7);
    if (!b->d.NumParameters) gen0(b, 8);
    EXPECT_HR(rs_ser0(&b->d, &bl, NULL), S_OK);
    EXPECT_HR(CRDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12RootSignatureDeserializer, (void **)&d), S_OK);
    if (d) {
        const D3D12_ROOT_SIGNATURE_DESC *g1 = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(d);
        EXPECT_EQ(ID3D12RootSignatureDeserializer_AddRef(d), 2);
        const D3D12_ROOT_SIGNATURE_DESC *g2 = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(d);
        CHECK(g1 == g2);                                                 /* the description lives in the object: stable pointer */
        EXPECT_EQ(ID3D12RootSignatureDeserializer_Release(d), 1);
        SAFE_RELEASE(bl);                                                /* the object keeps its own copy of the data */
        memset(b, 0xcd, sizeof *b);
        CHECK(g1->NumParameters <= 7);
        EXPECT_EQ(ID3D12RootSignatureDeserializer_Release(d), 0);
    }
    free(b);
}
