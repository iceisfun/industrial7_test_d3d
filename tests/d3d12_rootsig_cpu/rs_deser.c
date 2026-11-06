/* Root signature deserializers: creation, riid handling, corrupt input, version conversion. */
#include "rs.h"
#define ALL D3D12_SHADER_VISIBILITY_ALL
#define VS  D3D12_SHADER_VISIBILITY_VERTEX
#define PS  D3D12_SHADER_VISIBILITY_PIXEL
#define SRV D3D12_DESCRIPTOR_RANGE_TYPE_SRV
#define UAV D3D12_DESCRIPTOR_RANGE_TYPE_UAV
#define CBV D3D12_DESCRIPTOR_RANGE_TYPE_CBV
#define SMP D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
#define RCBV D3D12_ROOT_PARAMETER_TYPE_CBV
#define RSRV D3D12_ROOT_PARAMETER_TYPE_SRV
#define RUAV D3D12_ROOT_PARAMETER_TYPE_UAV
#define APPEND D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
#define E_INVALID ((HRESULT)0x80070057)
#define E_NOIFACE ((HRESULT)0x80004002)
#define E_FAILX   ((HRESULT)0x80004005)

/* container buffers carry slack behind them: the creation functions read the size from the container header */
typedef struct { uint8_t d[16384]; size_t n; } CBUF;

static void mk_blob(CBUF *b, const void *rts, size_t n, int fix_hash) {
    memset(b->d, 0, sizeof b->d); b->n = rs_dxbc_wrap(rts, n, b->d, fix_hash);
}
/* container with several parts; offsets contiguous unless gap > 0 is requested before the last part */
static void mk_multi(CBUF *b, const char *const *tags, const void *const *data, const size_t *sizes, unsigned cnt, size_t gap) {
    uint8_t *p = b->d; uint32_t *h = (uint32_t *)p; unsigned i; size_t pos = 32 + 4 * (size_t)cnt;
    memset(b->d, 0, sizeof b->d);
    memcpy(p, "DXBC", 4); h[5] = 1; h[7] = cnt;
    for (i = 0; i < cnt; i++) {
        if (i == cnt - 1) pos += gap;
        h[8 + i] = (uint32_t)pos; memcpy(p + pos, tags[i], 4); *(uint32_t *)(p + pos + 4) = (uint32_t)sizes[i];
        memcpy(p + pos + 8, data[i], sizes[i]); pos += 8 + sizes[i];
    }
    h[6] = (uint32_t)pos; b->n = pos; rs_dxbc_hash(p, pos, p + 4);
}
/* the output pointer starts as a sentinel: a failing creation must have cleared it */
static HRESULT cr_chk(HRESULT hr, void **out) {
    if (FAILED(hr) && *out == (void *)(ULONG_PTR)1) { CHECKF(0, "output pointer not cleared on failure (hr=%08lx)", (unsigned long)hr); *out = NULL; }
    return hr;
}
static HRESULT cr0(const CBUF *b, size_t size, const IID *iid, void **out) { *out = (void *)(ULONG_PTR)1; return cr_chk(CRDES()(b->d, size, iid, out), out); }
static HRESULT crv(const CBUF *b, size_t size, const IID *iid, void **out) { *out = (void *)(ULONG_PTR)1; return cr_chk(CRVDES()(b->d, size, iid, out), out); }

/* sample v1.0 description: 3 root parameters (constants, root CBV, table with SRV + UAV ranges) and one sampler */
static const D3D12_ROOT_SIGNATURE_DESC *sample0(RS0 *b) {
    rs0_init(b);
    rs0_consts(b, 0, 0, 4, ALL); rs0_root(b, RCBV, 1, 0, VS);
    rs0_table(b, PS); rs0_range(b, SRV, 2, 0, 0, APPEND); rs0_range(b, UAV, 4, 0, 0, APPEND);
    rs0_sampler(b, 0, 0, PS)->Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    return rs0_done(b, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
}
/* sample v1.1 description exercising every flag field */
static const D3D12_ROOT_SIGNATURE_DESC1 *sample1(RS1 *b) {
    rs1_init(b);
    rs1_consts(b, 0, 0, 4, ALL); rs1_root(b, RCBV, 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, VS);
    rs1_root(b, RSRV, 2, 1, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, ALL);
    rs1_root(b, RUAV, 3, 2, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, PS);
    rs1_table(b, PS);
    rs1_range(b, SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, APPEND);
    rs1_range(b, UAV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE, APPEND);
    rs1_range(b, CBV, 1, 5, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS, 100);   /* b5: b0 is taken by the root constants and Win10 rejects overlapping register ranges */
    rs1_table(b, ALL); rs1_range(b, SMP, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, APPEND);
    rs1_sampler(b, 7, 3, PS)->Filter = D3D12_FILTER_ANISOTROPIC;
    return rs1_done(b, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT);
}

/* ---------------- creation arguments ---------------- */
TEST(d3d12_rootsig_cpu_des_null_output) {
    RS_REQUIRE_ALL(); if (t_is_wine()) { t_skip("emulator dereferences NULL"); return; } RS0 b; RSBUF m; CBUF bl; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(CRDES()(bl.d, bl.n, &IID_ID3D12RootSignatureDeserializer, NULL), E_INVALID);
    EXPECT_HR(CRVDES()(bl.d, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, NULL), E_INVALID);
}
TEST(d3d12_rootsig_cpu_des_null_data) {
    RS_REQUIRE_ALL(); if (t_is_wine()) { t_skip("emulator dereferences NULL"); return; } void *p = (void *)(ULONG_PTR)1;
    EXPECT_HR(CRDES()(NULL, 100, &IID_ID3D12RootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
    p = (void *)(ULONG_PTR)1;
    EXPECT_HR(CRVDES()(NULL, 100, &IID_ID3D12VersionedRootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_size_zero) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, 0, &IID_ID3D12RootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
    EXPECT_HR(crv(&bl, 0, &IID_ID3D12VersionedRootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_size_too_small_by_one) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n - 1, &IID_ID3D12RootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
    EXPECT_HR(crv(&bl, bl.n - 1, &IID_ID3D12VersionedRootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_size_too_large_by_four) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n + 4, &IID_ID3D12RootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
    EXPECT_HR(crv(&bl, bl.n + 4, &IID_ID3D12VersionedRootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_size_below_header) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; static const size_t sz[] = { 1, 4, 31, 32, 35, 36, 43 }; size_t i;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        HRESULT hr = cr0(&bl, sz[i], &IID_ID3D12RootSignatureDeserializer, &p);
        CHECKF(hr == E_INVALID && p == NULL, "size %u: hr=%08lx", (unsigned)sz[i], (unsigned long)hr);
    }
}
TEST(d3d12_rootsig_cpu_des_baseline_ok) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; const D3D12_ROOT_SIGNATURE_DESC *d = sample0(&b);
    n = rs_model0(d, &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12RootSignatureDeserializer *ds = p;
        CHECK(rs_desc_eq0(d, ID3D12RootSignatureDeserializer_GetRootSignatureDesc(ds)));
        ID3D12RootSignatureDeserializer_Release(ds);
    }
}
TEST(d3d12_rootsig_cpu_des_digest_not_verified) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; const D3D12_ROOT_SIGNATURE_DESC *d = sample0(&b);
    n = rs_model0(d, &m);
    mk_blob(&bl, m.w, n, 0); /* all-zero digest */
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p);
    mk_blob(&bl, m.w, n, 1); bl.d[5] ^= 0xff; /* wrong digest */
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p);
    EXPECT_HR(crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) ID3D12VersionedRootSignatureDeserializer_Release((ID3D12VersionedRootSignatureDeserializer *)p);
}
TEST(d3d12_rootsig_cpu_des_source_buffer_not_retained) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; const D3D12_ROOT_SIGNATURE_DESC *d = sample0(&b);
    n = rs_model0(d, &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    memset(bl.d, 0xcc, sizeof bl.d);
    if (p) {
        ID3D12RootSignatureDeserializer *ds = p;
        CHECK(rs_desc_eq0(d, ID3D12RootSignatureDeserializer_GetRootSignatureDesc(ds)));
        ID3D12RootSignatureDeserializer_Release(ds);
    }
}
TEST(d3d12_rootsig_cpu_des_raw_rts0_without_container_rejected) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); memset(bl.d, 0, sizeof bl.d); memcpy(bl.d, m.w, n);
    EXPECT_HR(cr0(&bl, n, &IID_ID3D12RootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
    EXPECT_HR(crv(&bl, n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}

/* ---------------- container damage ---------------- */
#define CONT(name, code) TEST(d3d12_rootsig_cpu_des_container_##name) { RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; \
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1); { code; } \
    HRESULT hr = cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p); EXPECT_HR(hr, E_INVALID); CHECK(p == NULL); \
    hr = crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p); EXPECT_HR(hr, E_INVALID); CHECK(p == NULL); }
CONT(bad_magic, bl.d[0] = 'X')
CONT(bad_magic_last_byte, bl.d[3] = 'd')
CONT(major_version_2, bl.d[20] = 2)
CONT(major_version_0, bl.d[20] = 0)
CONT(minor_version_1, bl.d[22] = 1)
CONT(size_field_smaller, *(uint32_t *)(bl.d + 24) -= 4)
CONT(size_field_larger, *(uint32_t *)(bl.d + 24) += 4)
CONT(part_count_zero, *(uint32_t *)(bl.d + 28) = 0)
CONT(part_count_two, *(uint32_t *)(bl.d + 28) = 2)
CONT(part_count_huge, *(uint32_t *)(bl.d + 28) = 0x40000000)
CONT(part_offset_beyond_end, *(uint32_t *)(bl.d + 32) = (uint32_t)bl.n)
CONT(part_offset_before_table, *(uint32_t *)(bl.d + 32) = 32)
CONT(part_offset_not_contiguous, *(uint32_t *)(bl.d + 32) = 40)
CONT(part_size_beyond_end, *(uint32_t *)(bl.d + 40) += 4)
/* Win10 22H2: the part size is not checked against the container; a part claiming 0xfffffff0 bytes is still accepted by both deserializers */
TEST(d3d12_rootsig_cpu_des_container_part_size_huge_accepted) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1); *(uint32_t *)(bl.d + 40) = 0xfffffff0u;
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p);
    EXPECT_HR(crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) ID3D12VersionedRootSignatureDeserializer_Release((ID3D12VersionedRootSignatureDeserializer *)p);
}
CONT(part_tag_renamed, bl.d[36] = 'X')
TEST(d3d12_rootsig_cpu_des_container_no_rts0_part) {
    RS_REQUIRE_ALL(); CBUF bl; void *p; const char *tags[1] = { "ABCD" }; uint32_t data[4] = { 1, 2, 3, 4 }; const void *dp[1] = { data }; size_t sz[1] = { 16 };
    mk_multi(&bl, tags, dp, sz, 1, 0);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
    EXPECT_HR(crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_container_empty_no_parts) {
    RS_REQUIRE_ALL(); CBUF bl; void *p;
    mk_multi(&bl, NULL, NULL, NULL, 0, 0);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_container_rts0_after_other_part) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; const D3D12_ROOT_SIGNATURE_DESC *d = sample0(&b);
    const char *tags[2] = { "ISGN", "RTS0" }; uint32_t junk[2] = { 0, 8 }; const void *dp[2]; size_t sz[2];
    n = rs_model0(d, &m); dp[0] = junk; sz[0] = 8; dp[1] = m.w; sz[1] = n;
    mk_multi(&bl, tags, dp, sz, 2, 0);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) {
        CHECK(rs_desc_eq0(d, ID3D12RootSignatureDeserializer_GetRootSignatureDesc((ID3D12RootSignatureDeserializer *)p)));
        ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p);
    }
}
TEST(d3d12_rootsig_cpu_des_container_rts0_before_other_part) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; const D3D12_ROOT_SIGNATURE_DESC *d = sample0(&b);
    const char *tags[2] = { "RTS0", "XXXX" }; uint32_t junk[2] = { 0, 8 }; const void *dp[2]; size_t sz[2];
    n = rs_model0(d, &m); dp[0] = m.w; sz[0] = n; dp[1] = junk; sz[1] = 8;
    mk_multi(&bl, tags, dp, sz, 2, 0);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p);
}
TEST(d3d12_rootsig_cpu_des_container_gap_between_parts_rejected) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    const char *tags[2] = { "ISGN", "RTS0" }; uint32_t junk[2] = { 0, 8 }; const void *dp[2]; size_t sz[2];
    n = rs_model0(sample0(&b), &m); dp[0] = junk; sz[0] = 8; dp[1] = m.w; sz[1] = n;
    mk_multi(&bl, tags, dp, sz, 2, 4);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}

/* ---------------- RTS0 payload damage (container is well formed) ---------------- */
/* word layout of sample0: 0..5 header, 6..14 params, 15..17 constants, 18..19 root CBV, 20..21 table header,
   22..31 two ranges, 32..44 sampler; 45 dwords = 180 bytes */
#define PAY(name, okexpect, code, checkcode) TEST(d3d12_rootsig_cpu_des_payload_##name) { RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; uint32_t w[64]; \
    HRESULT hr; n = rs_model0(sample0(&b), &m); memcpy(w, m.w, n); { code; } mk_blob(&bl, w, n, 1); \
    hr = cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p); \
    if (okexpect) { EXPECT_HR(hr, S_OK); if (p) { const D3D12_ROOT_SIGNATURE_DESC *g = ID3D12RootSignatureDeserializer_GetRootSignatureDesc((ID3D12RootSignatureDeserializer *)p); \
        (void)g; checkcode; ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p); } } \
    else { EXPECT_HR(hr, E_INVALID); CHECK(p == NULL); } \
    hr = crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p); \
    if (okexpect) { EXPECT_HR(hr, S_OK); if (p) ID3D12VersionedRootSignatureDeserializer_Release((ID3D12VersionedRootSignatureDeserializer *)p); } \
    else { EXPECT_HR(hr, E_INVALID); CHECK(p == NULL); } }
PAY(version_0, 0, w[0] = 0, (void)0)
PAY(version_4, 0, w[0] = 4, (void)0)
PAY(version_99, 0, w[0] = 99, (void)0)
PAY(version_ffffffff, 0, w[0] = 0xffffffffu, (void)0)
PAY(shorter_than_header, 0, n = 23, (void)0)
PAY(only_version, 0, n = 4, (void)0)
PAY(header_only_empty_ok, 1, w[1] = 0; w[3] = 0; w[4] = 24; n = 24, CHECK(g->NumParameters == 0 && g->NumStaticSamplers == 0))
/* Win10 22H2: the parameter count and offset are not range checked against the payload. Counts/offsets that run off the buffer fault inside the
   deserializer (access violation, 3/3 fresh VMs); a count one past the real table just reads the next bytes as a parameter and succeeds. */
typedef struct { const CBUF *bl; HRESULT hr; void *p; int versioned; } payf_args;
static void payf_call(void *x) {
    payf_args *a = x; a->p = (void *)(ULONG_PTR)1;
    a->hr = a->versioned ? CRVDES()(a->bl->d, a->bl->n, &IID_ID3D12VersionedRootSignatureDeserializer, &a->p) : CRDES()(a->bl->d, a->bl->n, &IID_ID3D12RootSignatureDeserializer, &a->p);
}
#define PAYF(name, code) TEST(d3d12_rootsig_cpu_des_payload_##name##_faults) { RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; size_t n; uint32_t w[64]; \
    n = rs_model0(sample0(&b), &m); memcpy(w, m.w, n); { code; } mk_blob(&bl, w, n, 1); \
    payf_args a = { &bl, S_OK, NULL, 0 }; \
    EXPECT_EQ(t_try(payf_call, &a), 0xC0000005u); }
PAYF(params_count_overruns, w[1] = 100)
PAYF(params_count_huge, w[1] = 0xffffffffu)
PAYF(params_offset_beyond, w[2] = 4000)
PAY(params_count_one_too_many, 1, w[1] = 4, (void)0)
PAY(sampler_count_overruns, 0, w[3] = 2, (void)0)
PAY(sampler_count_huge, 0, w[3] = 0x7fffffffu, (void)0)
PAY(sampler_offset_beyond, 0, w[4] = 1000, (void)0)
PAY(sampler_offset_end_minus_1, 0, w[4] = 180 - 51, (void)0)
PAY(sampler_count_zero_offset_at_end, 1, w[3] = 0; w[4] = 180, CHECK(g->NumStaticSamplers == 0))
PAY(sampler_count_zero_offset_far_ok, 0, w[3] = 0; w[4] = 400, (void)0)
PAY(param_type_5, 0, w[6] = 5, (void)0)
PAY(param_type_ffffffff, 0, w[9] = 0xffffffffu, (void)0)
PAY(constants_payload_beyond, 0, w[8] = 4000, (void)0)
PAY(constants_payload_crosses_end, 0, w[8] = 180 - 8, (void)0)
PAY(cbv_payload_crosses_end, 0, w[11] = 180 - 4, (void)0)
PAY(table_header_crosses_end, 0, w[14] = 180 - 4, (void)0)
/* table_header_beyond (table header offset 100000 bytes past a 16 KiB buffer) is deliberately not run: Win10 22H2 reads and writes through the
   unchecked offset and dies with heap corruption (0xc0000374) at an unpredictable later point, so no stable outcome can be pinned in-process. */
PAY(flags_top_bit_dropped, 1, w[5] = 0x80000001u, EXPECT_EQ(g->Flags, 1))
PAY(flags_all_low_bits_kept, 1, w[5] = 0x7fffffffu, EXPECT_EQ(g->Flags, 0x7fffffffu))
PAY(visibility_not_validated, 1, w[7] = 77, EXPECT_EQ(g->pParameters[0].ShaderVisibility, 77))
PAY(sampler_fields_not_validated, 1, w[33] = 999; w[34] = 1234, EXPECT_EQ(g->pStaticSamplers[0].AddressU, 999))
PAY(trailing_bytes_ignored, 1, n = 180 + 8, (void)0)
/* Win10 22H2: descriptor tables with zero ranges. The serializer accepts them; the deserializers either answer E_INVALIDARG (blob written by
   the serializer) or S_OK (hand-made payload), and in both cases damage the heap: the process later dies with 0xc0000374 or 0xc0000005 (observed in
   3 of 4 in-process attempts, 100% within 40 iterations). The cases therefore run in a child process and only the crash is asserted. */
CHILD(rs_zero_range_table) {
    int mode = argc > 0 ? atoi(argv[0]) : 0, it, k; ID3DBlob *bl = NULL;
    if (!SER10() || !CRDES() || !CRVDES()) return 77;
    RS0 *b = calloc(1, sizeof *b); RSBUF m; CBUF cb; size_t n;
    if (mode == 0) {
        rs0_init(b); rs0_table(b, ALL);
        if (FAILED(SER10()(rs0_done(b, 0), D3D_ROOT_SIGNATURE_VERSION_1_0, &bl, NULL))) return 78;
        memcpy(cb.d, ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl)); cb.n = ID3D10Blob_GetBufferSize(bl);
    } else {
        uint32_t w[64]; n = rs_model0(sample0(b), &m); memcpy(w, m.w, n); w[20] = 0; w[21] = 88; mk_blob(&cb, w, n, 1);
    }
    for (it = 0; it < 40; it++) {
        void *p = NULL;
        HRESULT hr = (it & 1) ? CRVDES()(cb.d, cb.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p) : CRDES()(cb.d, cb.n, &IID_ID3D12RootSignatureDeserializer, &p);
        if (SUCCEEDED(hr) && p) { if (it & 1) ID3D12VersionedRootSignatureDeserializer_Release((ID3D12VersionedRootSignatureDeserializer *)p); else ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p); }
        for (k = 0; k < 400; k++) { void *q = malloc(16 + (k % 97)); memset(q, 1, 16); free(q); }
    }
    return 0;
}
TEST(d3d12_rootsig_cpu_des_zero_range_table_serialized_blob_damages_heap) {
    RS_REQUIRE_ALL(); DWORD rc = t_run_child("rs_zero_range_table", "0", 120000);
    CHECKF(rc == 0xC0000374u || rc == 0xC0000005u, "child exit 0x%08lx (expected a heap-corruption/access-violation crash)", (unsigned long)rc);
}
TEST(d3d12_rootsig_cpu_des_zero_range_table_handmade_payload_damages_heap) {
    RS_REQUIRE_ALL(); DWORD rc = t_run_child("rs_zero_range_table", "1", 120000);
    CHECKF(rc == 0xC0000374u || rc == 0xC0000005u, "child exit 0x%08lx (expected a heap-corruption/access-violation crash)", (unsigned long)rc);
}

/* ---------------- interface identity ---------------- */
TEST(d3d12_rootsig_cpu_des_riid_plain) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; HRESULT hr;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    hr = cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p); EXPECT_HR(hr, S_OK);
    if (p) ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p);
}
TEST(d3d12_rootsig_cpu_des_riid_versioned_on_plain_creator) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), E_NOIFACE); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_riid_plain_on_versioned_creator) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(crv(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), E_NOIFACE); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_riid_iunknown_rejected) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    /* the deserializer objects implement exactly one interface id and do not answer IUnknown */
    EXPECT_HR(cr0(&bl, bl.n, &IID_IUnknown, &p), E_NOIFACE); CHECK(p == NULL);
    EXPECT_HR(crv(&bl, bl.n, &IID_IUnknown, &p), E_NOIFACE); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_riid_unrelated) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; const IID *ids[] = { &GUID_NULL, &IID_ID3D12Object, &IID_ID3D12RootSignature, &IID_ID3D12Device }; size_t i;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    for (i = 0; i < 4; i++) {
        HRESULT hr = cr0(&bl, bl.n, ids[i], &p); CHECKF(hr == E_NOIFACE && p == NULL, "plain riid %u: %08lx", (unsigned)i, (unsigned long)hr);
        hr = crv(&bl, bl.n, ids[i], &p); CHECKF(hr == E_NOIFACE && p == NULL, "versioned riid %u: %08lx", (unsigned)i, (unsigned long)hr);
    }
}
TEST(d3d12_rootsig_cpu_des_riid_checked_after_blob) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1); bl.d[0] = 'Z';
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), E_INVALID); CHECK(p == NULL);
}
TEST(d3d12_rootsig_cpu_des_iid_values) {
    static const GUID g1 = { 0x34ab647b, 0x3cc8, 0x46ac, { 0x84, 0x1b, 0xc0, 0x96, 0x56, 0x45, 0xc0, 0x46 } };
    static const GUID g2 = { 0x7f91ce67, 0x090c, 0x4bb7, { 0xb7, 0x8e, 0xed, 0x8f, 0xf2, 0xe3, 0x1d, 0xa0 } };
    CHECK(!memcmp(&IID_ID3D12RootSignatureDeserializer, &g1, sizeof g1));
    CHECK(!memcmp(&IID_ID3D12VersionedRootSignatureDeserializer, &g2, sizeof g2));
}

/* ---------------- reference counting and QueryInterface ---------------- */
TEST(d3d12_rootsig_cpu_des_refcount_plain) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12RootSignatureDeserializer *d = p;
        EQV(ID3D12RootSignatureDeserializer_AddRef(d), 2);
        EQV(ID3D12RootSignatureDeserializer_AddRef(d), 3);
        EQV(ID3D12RootSignatureDeserializer_Release(d), 2);
        EQV(ID3D12RootSignatureDeserializer_Release(d), 1);
        EQV(ID3D12RootSignatureDeserializer_Release(d), 0);
    }
}
TEST(d3d12_rootsig_cpu_des_refcount_versioned) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *d = p;
        EQV(ID3D12VersionedRootSignatureDeserializer_AddRef(d), 2);
        EQV(ID3D12VersionedRootSignatureDeserializer_Release(d), 1);
        EQV(ID3D12VersionedRootSignatureDeserializer_Release(d), 0);
    }
}
TEST(d3d12_rootsig_cpu_des_qi_plain) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p, *q = NULL; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12RootSignatureDeserializer *d = p;
        EXPECT_HR(ID3D12RootSignatureDeserializer_QueryInterface(d, &IID_ID3D12RootSignatureDeserializer, &q), S_OK);
        CHECK(q == p);
        EQV(ID3D12RootSignatureDeserializer_Release(d), 1);   /* the QI added one */
        q = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D12RootSignatureDeserializer_QueryInterface(d, &IID_IUnknown, &q), E_NOIFACE); CHECK(q == NULL);
        q = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D12RootSignatureDeserializer_QueryInterface(d, &IID_ID3D12VersionedRootSignatureDeserializer, &q), E_NOIFACE); CHECK(q == NULL);
        if (!t_is_wine()) EXPECT_HR(ID3D12RootSignatureDeserializer_QueryInterface(d, &IID_ID3D12RootSignatureDeserializer, NULL), E_FAILX);
        EQV(ID3D12RootSignatureDeserializer_Release(d), 0);
    }
}
TEST(d3d12_rootsig_cpu_des_qi_versioned) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p, *q = NULL; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *d = p;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_QueryInterface(d, &IID_ID3D12VersionedRootSignatureDeserializer, &q), S_OK);
        CHECK(q == p);
        EQV(ID3D12VersionedRootSignatureDeserializer_Release(d), 1);
        q = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_QueryInterface(d, &IID_IUnknown, &q), E_NOIFACE); CHECK(q == NULL);
        q = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_QueryInterface(d, &IID_ID3D12RootSignatureDeserializer, &q), E_NOIFACE); CHECK(q == NULL);
        if (!t_is_wine()) EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_QueryInterface(d, &IID_ID3D12VersionedRootSignatureDeserializer, NULL), E_FAILX);
        EQV(ID3D12VersionedRootSignatureDeserializer_Release(d), 0);
    }
}
TEST(d3d12_rootsig_cpu_des_two_objects_independent) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p1, *p2; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p1), S_OK);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p2), S_OK);
    CHECK(p1 && p2 && p1 != p2);
    if (p1 && p2) {
        const D3D12_ROOT_SIGNATURE_DESC *a = ID3D12RootSignatureDeserializer_GetRootSignatureDesc((ID3D12RootSignatureDeserializer *)p1);
        const D3D12_ROOT_SIGNATURE_DESC *c = ID3D12RootSignatureDeserializer_GetRootSignatureDesc((ID3D12RootSignatureDeserializer *)p2);
        CHECK(a != c); CHECK(a->pParameters != c->pParameters);
        ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p1);
        CHECK(rs_desc_eq0(c, &b.d));
        ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p2);
    }
}
TEST(d3d12_rootsig_cpu_des_get_desc_stable_pointer) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(cr0(&bl, bl.n, &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12RootSignatureDeserializer *d = p;
        const D3D12_ROOT_SIGNATURE_DESC *a = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(d);
        const D3D12_ROOT_SIGNATURE_DESC *c = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(d);
        CHECK(a == c); ID3D12RootSignatureDeserializer_Release(d);
    }
}

/* ---------------- version conversion ---------------- */
static int get_blob(const void *desc_v, ID3DBlob **b) {
    *b = NULL; return SUCCEEDED(SERV()((const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *)desc_v, b, NULL));
}
TEST(d3d12_rootsig_cpu_ver_unconverted_v10) {
    RS_REQUIRE_ALL(); RS0 b; ID3DBlob *bl; void *p; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; const D3D12_ROOT_SIGNATURE_DESC *d = sample0(&b);
    memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0; v.Desc_1_0 = *d;
    if (!get_blob(&v, &bl)) { CHECK(0); return; }
    EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *ds = p;
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(ds);
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = NULL;
        CHECK(u != NULL && u->Version == D3D_ROOT_SIGNATURE_VERSION_1_0);
        if (u) CHECK(rs_desc_eq0(d, &u->Desc_1_0));
        CHECK(u == ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(ds));
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_0, &g), S_OK);
        CHECK(g == u);
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    SAFE_RELEASE(bl);
}
TEST(d3d12_rootsig_cpu_ver_unconverted_v11) {
    RS_REQUIRE_ALL(); RS1 b; ID3DBlob *bl; void *p; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; const D3D12_ROOT_SIGNATURE_DESC1 *d = sample1(&b);
    memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1 = *d;
    if (!get_blob(&v, &bl)) { CHECK(0); return; }
    EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *ds = p;
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(ds), *g = NULL;
        CHECK(u != NULL && u->Version == D3D_ROOT_SIGNATURE_VERSION_1_1);
        if (u) CHECK(rs_desc_eq1(d, &u->Desc_1_1));
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_1, &g), S_OK);
        CHECK(g == u);
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    SAFE_RELEASE(bl);
}
TEST(d3d12_rootsig_cpu_ver_upgrade_10_to_11_default_flags) {
    RS_REQUIRE_ALL(); RS0 b; ID3DBlob *bl; void *p; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; const D3D12_ROOT_SIGNATURE_DESC *d = sample0(&b);
    memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0; v.Desc_1_0 = *d;
    if (!get_blob(&v, &bl)) { CHECK(0); return; }
    EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *ds = p;
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = NULL, *g2 = NULL;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_1, &g), S_OK);
        if (g) {
            const D3D12_ROOT_SIGNATURE_DESC1 *c = &g->Desc_1_1; const D3D12_ROOT_PARAMETER1 *t;
            EXPECT_EQ(g->Version, D3D_ROOT_SIGNATURE_VERSION_1_1);
            EXPECT_EQ(c->NumParameters, 3); EXPECT_EQ(c->NumStaticSamplers, 1);
            EXPECT_EQ(c->Flags, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
            CHECK(c->pParameters != NULL);
            if (c->pParameters) {
                EXPECT_EQ(c->pParameters[0].ParameterType, D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS);
                EXPECT_EQ(c->pParameters[0].Constants.Num32BitValues, 4);
                EXPECT_EQ(c->pParameters[1].ParameterType, RCBV);
                EXPECT_EQ(c->pParameters[1].ShaderVisibility, VS);
                EXPECT_EQ(c->pParameters[1].Descriptor.ShaderRegister, 1);
                /* root descriptors default to DATA_VOLATILE */
                EXPECT_EQ(c->pParameters[1].Descriptor.Flags, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);
                t = &c->pParameters[2];
                EXPECT_EQ(t->DescriptorTable.NumDescriptorRanges, 2);
                if (t->DescriptorTable.pDescriptorRanges) {
                    /* resource ranges default to DESCRIPTORS_VOLATILE | DATA_VOLATILE */
                    EXPECT_EQ(t->DescriptorTable.pDescriptorRanges[0].Flags, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
                    EXPECT_EQ(t->DescriptorTable.pDescriptorRanges[1].Flags, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
                    EXPECT_EQ(t->DescriptorTable.pDescriptorRanges[0].NumDescriptors, 2);
                    EXPECT_EQ(t->DescriptorTable.pDescriptorRanges[1].NumDescriptors, 4);
                    EXPECT_EQ(t->DescriptorTable.pDescriptorRanges[1].RangeType, UAV);
                    EXPECT_EQ(t->DescriptorTable.pDescriptorRanges[1].OffsetInDescriptorsFromTableStart, APPEND);
                }
            }
            CHECK(c->pStaticSamplers && !memcmp(c->pStaticSamplers, d->pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC)));
        }
        /* cached: second request returns the same object */
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_1, &g2), S_OK);
        CHECK(g2 == g);
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    SAFE_RELEASE(bl);
}
TEST(d3d12_rootsig_cpu_ver_upgrade_sampler_range_flags) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl; void *p; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; if (!b) return;
    rs0_init(b); rs0_table(b, ALL); rs0_range(b, SMP, 4, 0, 0, APPEND); rs0_table(b, ALL); rs0_range(b, CBV, 1, 0, 0, APPEND);
    rs0_range(b, SRV, 1, 0, 0, APPEND); rs0_range(b, UAV, 1, 0, 0, APPEND);
    rs0_done(b, 0); memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0; v.Desc_1_0 = b->d;
    if (!get_blob(&v, &bl)) { CHECK(0); free(b); return; }
    EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *ds = p; const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = NULL;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_1, &g), S_OK);
        if (g && g->Desc_1_1.pParameters) {
            const D3D12_ROOT_PARAMETER1 *a = &g->Desc_1_1.pParameters[0], *c = &g->Desc_1_1.pParameters[1];
            /* sampler ranges: DESCRIPTORS_VOLATILE only (samplers have no data flags) */
            EXPECT_EQ(a->DescriptorTable.pDescriptorRanges[0].Flags, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
            EXPECT_EQ(c->DescriptorTable.pDescriptorRanges[0].Flags, 3);
            EXPECT_EQ(c->DescriptorTable.pDescriptorRanges[1].Flags, 3);
            EXPECT_EQ(c->DescriptorTable.pDescriptorRanges[2].Flags, 3);
        }
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    SAFE_RELEASE(bl); free(b);
}
TEST(d3d12_rootsig_cpu_ver_upgraded_desc_reserializes_to_expected_v11_bytes) {
    RS_REQUIRE_ALL(); RS0 b; ID3DBlob *bl, *bl2; void *p; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; const D3D12_ROOT_SIGNATURE_DESC *d = sample0(&b);
    memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0; v.Desc_1_0 = *d;
    if (!get_blob(&v, &bl)) { CHECK(0); return; }
    EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *ds = p; const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = NULL; RSBUF m;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_1, &g), S_OK);
        if (g) {
            HRESULT hr = SERV()(g, &bl2, NULL);
            EXPECT_HR(hr, S_OK);
            if (SUCCEEDED(hr)) { rs_model1(&g->Desc_1_1, &m); rs_blob_matches(bl2, &m, "upgraded"); SAFE_RELEASE(bl2); }
        }
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    SAFE_RELEASE(bl);
}
TEST(d3d12_rootsig_cpu_ver_downgrade_11_to_10_drops_flags) {
    RS_REQUIRE_ALL(); RS1 b; ID3DBlob *bl; void *p; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; const D3D12_ROOT_SIGNATURE_DESC1 *d = sample1(&b);
    memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1 = *d;
    if (!get_blob(&v, &bl)) { CHECK(0); return; }
    EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *ds = p; const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = NULL, *g2 = NULL; UINT i, k;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_0, &g), S_OK);
        if (g) {
            const D3D12_ROOT_SIGNATURE_DESC *c = &g->Desc_1_0;
            EXPECT_EQ(g->Version, D3D_ROOT_SIGNATURE_VERSION_1_0);
            EXPECT_EQ(c->NumParameters, d->NumParameters); EXPECT_EQ(c->NumStaticSamplers, d->NumStaticSamplers); EXPECT_EQ(c->Flags, d->Flags);
            for (i = 0; c->pParameters && i < c->NumParameters; i++) {
                const D3D12_ROOT_PARAMETER *q = &c->pParameters[i]; const D3D12_ROOT_PARAMETER1 *s = &d->pParameters[i];
                EXPECT_EQ(q->ParameterType, s->ParameterType); EXPECT_EQ(q->ShaderVisibility, s->ShaderVisibility);
                if (s->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
                    EXPECT_EQ(q->DescriptorTable.NumDescriptorRanges, s->DescriptorTable.NumDescriptorRanges);
                    for (k = 0; k < s->DescriptorTable.NumDescriptorRanges; k++) {
                        const D3D12_DESCRIPTOR_RANGE *r = &q->DescriptorTable.pDescriptorRanges[k]; const D3D12_DESCRIPTOR_RANGE1 *r1 = &s->DescriptorTable.pDescriptorRanges[k];
                        EXPECT_EQ(r->RangeType, r1->RangeType); EXPECT_EQ(r->NumDescriptors, r1->NumDescriptors);
                        EXPECT_EQ(r->BaseShaderRegister, r1->BaseShaderRegister); EXPECT_EQ(r->RegisterSpace, r1->RegisterSpace);
                        EXPECT_EQ(r->OffsetInDescriptorsFromTableStart, r1->OffsetInDescriptorsFromTableStart);
                    }
                } else if (s->ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
                    CHECK(!memcmp(&q->Constants, &s->Constants, sizeof q->Constants));
                } else {
                    EXPECT_EQ(q->Descriptor.ShaderRegister, s->Descriptor.ShaderRegister); EXPECT_EQ(q->Descriptor.RegisterSpace, s->Descriptor.RegisterSpace);
                }
            }
            CHECK(c->pStaticSamplers && !memcmp(c->pStaticSamplers, d->pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC)));
        }
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_0, &g2), S_OK);
        CHECK(g2 == g);
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    SAFE_RELEASE(bl);
}
TEST(d3d12_rootsig_cpu_ver_plain_deserializer_downgrades_v11_blob) {
    RS_REQUIRE_ALL(); RS1 b; ID3DBlob *bl, *bl2; void *p, *q; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; const D3D12_ROOT_SIGNATURE_DESC1 *d = sample1(&b);
    memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1 = *d;
    if (!get_blob(&v, &bl)) { CHECK(0); return; }
    EXPECT_HR(CRDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12RootSignatureDeserializer, &p), S_OK);
    EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &q), S_OK);
    if (p && q) {
        const D3D12_ROOT_SIGNATURE_DESC *a = ID3D12RootSignatureDeserializer_GetRootSignatureDesc((ID3D12RootSignatureDeserializer *)p);
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = NULL;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion((ID3D12VersionedRootSignatureDeserializer *)q, D3D_ROOT_SIGNATURE_VERSION_1_0, &g), S_OK);
        CHECK(a && g && rs_desc_eq0(a, &g->Desc_1_0));
        /* serializing the downgraded description yields the 1.0 container */
        if (a) { RSBUF m; HRESULT hr = rs_ser0(a, &bl2, NULL); EXPECT_HR(hr, S_OK); if (SUCCEEDED(hr)) { rs_model0(a, &m); rs_blob_matches(bl2, &m, "downgraded"); SAFE_RELEASE(bl2); } }
    }
    if (p) ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p);
    if (q) ID3D12VersionedRootSignatureDeserializer_Release((ID3D12VersionedRootSignatureDeserializer *)q);
    SAFE_RELEASE(bl);
}
TEST(d3d12_rootsig_cpu_ver_at_version_invalid) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n; static const UINT vers[] = { 0, 3, 4, 100, 0xffffffffu }; size_t i;
    n = rs_model0(sample0(&b), &m); mk_blob(&bl, m.w, n, 1);
    EXPECT_HR(crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
    if (p) {
        ID3D12VersionedRootSignatureDeserializer *ds = p;
        for (i = 0; i < sizeof vers / sizeof *vers; i++) {
            const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *g = (const void *)(ULONG_PTR)0x77;
            HRESULT hr = ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, (D3D_ROOT_SIGNATURE_VERSION)vers[i], &g);
            CHECKF(hr == E_FAILX, "version %u: hr=%08lx", vers[i], (unsigned long)hr);
            CHECKF(g == (const void *)(ULONG_PTR)0x77, "version %u: output pointer modified", vers[i]);
        }
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
}
TEST(d3d12_rootsig_cpu_ver_roundtrip_flags_all_range_combinations) {
    RS_REQUIRE_ALL(); static const UINT ok[] = { 0, 1, 2, 4, 8, 0x10000, 3, 5, 0x10002, 0x10004, 0x10008 }; size_t i;
    for (i = 0; i < sizeof ok / sizeof *ok; i++) {
        RS1 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL; void *p; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; if (!b) return;
        rs1_init(b); rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, ok[i], APPEND); rs1_done(b, 0);
        memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1 = b->d;
        if (get_blob(&v, &bl)) {
            EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
            if (p) {
                const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc((ID3D12VersionedRootSignatureDeserializer *)p);
                CHECKF(u && u->Desc_1_1.pParameters[0].DescriptorTable.pDescriptorRanges[0].Flags == ok[i], "flags %x not preserved", ok[i]);
                ID3D12VersionedRootSignatureDeserializer_Release((ID3D12VersionedRootSignatureDeserializer *)p);
            }
        } else CHECKF(0, "serialize flags %x", ok[i]);
        SAFE_RELEASE(bl); free(b);
    }
}
TEST(d3d12_rootsig_cpu_ver_roundtrip_root_descriptor_flags) {
    RS_REQUIRE_ALL(); static const UINT ok[] = { 0, 2, 4, 8 }; size_t i;
    for (i = 0; i < 4; i++) {
        RS1 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL; void *p; D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; if (!b) return;
        rs1_init(b); rs1_root(b, RSRV, 0, 0, ok[i], ALL); rs1_done(b, 0);
        memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1 = b->d;
        if (get_blob(&v, &bl)) {
            EXPECT_HR(CRVDES()(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &IID_ID3D12VersionedRootSignatureDeserializer, &p), S_OK);
            if (p) {
                const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc((ID3D12VersionedRootSignatureDeserializer *)p);
                CHECKF(u && u->Desc_1_1.pParameters[0].Descriptor.Flags == ok[i], "root flags %x not preserved", ok[i]);
                ID3D12VersionedRootSignatureDeserializer_Release((ID3D12VersionedRootSignatureDeserializer *)p);
            }
        } else CHECKF(0, "serialize root flags %x", ok[i]);
        SAFE_RELEASE(bl); free(b);
    }
}
TEST(d3d12_rootsig_cpu_ver_versioned_serializes_both_versions_with_version_dword) {
    RS_REQUIRE_ALL(); RS0 b0; RS1 b1; ID3DBlob *bl; size_t rn; const uint8_t *r; uint32_t ver;
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
    v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0; v.Desc_1_0 = *sample0(&b0);
    CHECK(get_blob(&v, &bl)); r = rs_find_rts0(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &rn);
    CHECK(r != NULL); if (r) { memcpy(&ver, r, 4); EXPECT_EQ(ver, 1); } SAFE_RELEASE(bl);
    memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1 = *sample1(&b1);
    CHECK(get_blob(&v, &bl)); r = rs_find_rts0(ID3D10Blob_GetBufferPointer(bl), ID3D10Blob_GetBufferSize(bl), &rn);
    CHECK(r != NULL); if (r) { memcpy(&ver, r, 4); EXPECT_EQ(ver, 2); } SAFE_RELEASE(bl);
}
TEST(d3d12_rootsig_cpu_ver_v12_blob_version3) {
    RS_REQUIRE_ALL(); RS0 b; RSBUF m; CBUF bl; void *p; size_t n;
    n = rs_model0(sample0(&b), &m); m.w[0] = 3; mk_blob(&bl, m.w, n, 1);
    HRESULT hr = crv(&bl, bl.n, &IID_ID3D12VersionedRootSignatureDeserializer, &p);
    EXPECT_HR(hr, E_INVALID);                          /* Win10 22H2: version 3 (1.2) blobs are rejected */
    CHECK(p == NULL);
    if (SUCCEEDED(hr) && p) ID3D12VersionedRootSignatureDeserializer_Release((ID3D12VersionedRootSignatureDeserializer *)p);
}
