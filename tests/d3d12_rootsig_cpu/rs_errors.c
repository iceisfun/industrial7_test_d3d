/* Invalid root signature descriptions: exact HRESULT, exact error blob text, no output blob. */
#include "rs.h"
#include <math.h>
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
#define UNB 0xffffffffu
#define E_INVALID ((HRESULT)0x80070057)

/* the error text (NUL terminated, size == strlen + 1) or NULL when there is no error blob */
static void check_err(ID3DBlob *e, const char *want, const char *tag) {
    if (!want) { CHECKF(e == NULL, "%s: unexpected error blob", tag); return; }
    if (!e) { CHECKF(0, "%s: missing error blob, want \"%s\"", tag, want); return; }
    {
        const char *s = rs_errstr(e); SIZE_T n = ID3D10Blob_GetBufferSize(e), w = strlen(want);
        CHECKF(n == w + 1, "%s: error blob size %u want %u", tag, (unsigned)n, (unsigned)(w + 1));
        CHECKF(n >= 1 && s[n - 1] == 0, "%s: error text not NUL terminated", tag);
        CHECKF(n >= w && !memcmp(s, want, w), "%s: error text \"%.*s\" want \"%s\"", tag, (int)(n > 300 ? 300 : n), s, want);
    }
}
/* prefix variant, for texts carrying a %f we do not pin down */
static void check_err_prefix(ID3DBlob *e, const char *prefix, const char *tag) {
    if (!e) { CHECKF(0, "%s: missing error blob, want prefix \"%s\"", tag, prefix); return; }
    {
        const char *s = rs_errstr(e); SIZE_T n = ID3D10Blob_GetBufferSize(e), p = strlen(prefix);
        CHECKF(n >= p && !memcmp(s, prefix, p), "%s: error text \"%.*s\" lacks prefix \"%s\"", tag, (int)(n > 300 ? 300 : n), s, prefix);
        CHECKF(n >= 1 && s[n - 1] == 0 && strlen(s) == n - 1, "%s: error blob is not a NUL terminated string of size strlen+1", tag);
    }
}
/* v1.0 description through D3D12SerializeRootSignature and the versioned entry point */
static void fail0(const D3D12_ROOT_SIGNATURE_DESC *d, const char *want, const char *prefix, const char *tag) {
    ID3DBlob *b = (ID3DBlob *)1, *e = NULL; HRESULT hr;
    hr = rs_ser0(d, &b, &e); SAFE_RELEASE(b);
    EXPECT_HR(hr, E_INVALID);
    if (prefix) check_err_prefix(e, prefix, tag); else check_err(e, want, tag);
    SAFE_RELEASE(e);
    b = (ID3DBlob *)(ULONG_PTR)0x1234;
    {
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
        v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0; v.Desc_1_0 = *d;
        hr = SERV()(&v, &b, &e);
    }
    EXPECT_HR(hr, E_INVALID);
    CHECKF(b == NULL, "%s: versioned serializer must NULL *ppBlob on failure", tag);
    if (prefix) check_err_prefix(e, prefix, tag); else check_err(e, want, tag);
    if (b == (ID3DBlob *)(ULONG_PTR)0x1234) b = NULL; /* an implementation that leaves the sentinel behind must not crash us */
    SAFE_RELEASE(e); SAFE_RELEASE(b);
}
static void fail1(const D3D12_ROOT_SIGNATURE_DESC1 *d, const char *want, const char *prefix, const char *tag) {
    ID3DBlob *b = (ID3DBlob *)(ULONG_PTR)0x1234, *e = NULL; HRESULT hr;
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
    v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1 = *d;
    hr = SERV()(&v, &b, &e);
    EXPECT_HR(hr, E_INVALID);
    CHECKF(b == NULL, "%s: *ppBlob must be NULL on failure", tag);
    if (prefix) check_err_prefix(e, prefix, tag); else check_err(e, want, tag);
    if (b == (ID3DBlob *)(ULONG_PTR)0x1234) b = NULL;
    SAFE_RELEASE(e); SAFE_RELEASE(b);
    /* a failing call without an error blob pointer must fail the same way */
    b = (ID3DBlob *)(ULONG_PTR)0x1234;
    hr = SERV()(&v, &b, NULL);
    EXPECT_HR(hr, E_INVALID);
    CHECKF(b == NULL, "%s: *ppBlob must be NULL on failure (no error blob requested)", tag);
    if (b == (ID3DBlob *)(ULONG_PTR)0x1234) b = NULL;
    SAFE_RELEASE(b);
}
#define F0(name, want, flags, ...) TEST(d3d12_rootsig_cpu_err10_##name) { RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; \
    rs0_init(b); __VA_ARGS__; fail0(rs0_done(b, flags), want, NULL, #name); free(b); }
#define F0P(name, prefix, flags, ...) TEST(d3d12_rootsig_cpu_err10_##name) { RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; \
    rs0_init(b); __VA_ARGS__; fail0(rs0_done(b, flags), NULL, prefix, #name); free(b); }
#define F1(name, want, flags, ...) TEST(d3d12_rootsig_cpu_err11_##name) { RS_REQUIRE_ALL(); RS1 *b = calloc(1, sizeof *b); if (!b) return; \
    rs1_init(b); __VA_ARGS__; fail1(rs1_done(b, flags), want, NULL, #name); free(b); }
#define F1P(name, prefix, flags, ...) TEST(d3d12_rootsig_cpu_err11_##name) { RS_REQUIRE_ALL(); RS1 *b = calloc(1, sizeof *b); if (!b) return; \
    rs1_init(b); __VA_ARGS__; fail1(rs1_done(b, flags), NULL, prefix, #name); free(b); }

/* ---------------- argument validation ---------------- */
TEST(d3d12_rootsig_cpu_err_ser10_null_desc) {
    RS_REQUIRE_ALL(); if (t_is_wine()) { t_skip("emulator dereferences a NULL description"); return; } ID3DBlob *b = NULL, *e = NULL;
    EXPECT_HR(SER10()(NULL, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, &e), E_INVALID);
    CHECK(b == NULL); CHECK(e == NULL);
}
TEST(d3d12_rootsig_cpu_err_ser10_null_blob_ptr) {
    RS_REQUIRE_ALL(); D3D12_ROOT_SIGNATURE_DESC d; ID3DBlob *e = NULL; memset(&d, 0, sizeof d);
    EXPECT_HR(SER10()(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, NULL, &e), E_INVALID);
    CHECK(e == NULL);
}
TEST(d3d12_rootsig_cpu_err_ser10_both_null) {
    RS_REQUIRE_ALL(); if (t_is_wine()) { t_skip("emulator dereferences a NULL description"); return; }
    EXPECT_HR(SER10()(NULL, D3D_ROOT_SIGNATURE_VERSION_1_0, NULL, NULL), E_INVALID);
}
TEST(d3d12_rootsig_cpu_err_ser10_error_ptr_null_success) {
    RS_REQUIRE_ALL(); D3D12_ROOT_SIGNATURE_DESC d; ID3DBlob *b = NULL; memset(&d, 0, sizeof d);
    EXPECT_HR(SER10()(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    CHECK(b != NULL); SAFE_RELEASE(b);
}
TEST(d3d12_rootsig_cpu_err_ser10_version_values) {
    RS_REQUIRE_ALL(); static const UINT vers[] = { 0, 2, 3, 4, 0x100, 0xffffffffu }; size_t i;
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    for (i = 0; i < sizeof vers / sizeof *vers; i++) {
        ID3DBlob *b = NULL, *e = NULL;
        HRESULT hr = SER10()(&d, (D3D_ROOT_SIGNATURE_VERSION)vers[i], &b, &e);
        CHECKF(hr == E_INVALID, "version %u: hr=%08lx", vers[i], (unsigned long)hr);
        CHECK(b == NULL); CHECK(e == NULL);
        SAFE_RELEASE(b); SAFE_RELEASE(e);
    }
}
TEST(d3d12_rootsig_cpu_err_serv_null_desc) {
    RS_REQUIRE_ALL(); if (t_is_wine()) { t_skip("emulator dereferences a NULL description"); return; } ID3DBlob *b = (ID3DBlob *)(ULONG_PTR)1, *e = NULL;
    EXPECT_HR(SERV()(NULL, &b, &e), E_INVALID);
    /* with a NULL description the output pointer is not touched */
}
TEST(d3d12_rootsig_cpu_err_serv_null_blob_ptr) {
    RS_REQUIRE_ALL(); D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; ID3DBlob *e = NULL; memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
    EXPECT_HR(SERV()(&v, NULL, &e), E_INVALID);
}
TEST(d3d12_rootsig_cpu_err_serv_unknown_versions) {
    RS_REQUIRE_ALL(); static const UINT vers[] = { 0, 4, 5, 0x100, 0xffffffffu }; size_t i;
    for (i = 0; i < sizeof vers / sizeof *vers; i++) {
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; ID3DBlob *b = (ID3DBlob *)(ULONG_PTR)0x1234, *e = NULL;
        memset(&v, 0, sizeof v); v.Version = (D3D_ROOT_SIGNATURE_VERSION)vers[i];
        HRESULT hr = SERV()(&v, &b, &e);
        CHECKF(hr == E_INVALID, "version %u: hr=%08lx", vers[i], (unsigned long)hr);
        CHECKF(b == NULL, "version %u: *ppBlob not cleared", vers[i]);
        CHECKF(e == NULL, "version %u: unexpected error blob", vers[i]);
        if (b == (ID3DBlob *)(ULONG_PTR)0x1234) b = NULL;
        SAFE_RELEASE(b); SAFE_RELEASE(e);
    }
}
TEST(d3d12_rootsig_cpu_err_serv_version_1_2) {
    RS_REQUIRE_ALL(); RS_VERSIONED_DESC2 v; ID3DBlob *b = NULL, *e = NULL; HRESULT hr;
    memset(&v, 0, sizeof v); v.Version = 3;
    hr = SERV()((const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *)&v, &b, &e);
    if (FAILED(hr)) {
        /* Windows 10 inbox d3d12core: only versions 1 and 2 are known and no error text is produced */
        EXPECT_HR(hr, E_INVALID); CHECK(b == NULL);
    } else {
        const uint8_t *p = b ? ID3D10Blob_GetBufferPointer(b) : NULL; size_t rn = 0;
        const uint8_t *r = p ? rs_find_rts0(p, ID3D10Blob_GetBufferSize(b), &rn) : NULL;
        CHECK(r != NULL); if (r) { uint32_t ver; memcpy(&ver, r, 4); EXPECT_EQ(ver, 3); }
        CHECKF(0, "Win10 22H2 d3d12 rejects version 3 (E_INVALIDARG) but it was accepted");
    }
    SAFE_RELEASE(b); SAFE_RELEASE(e);
}
TEST(d3d12_rootsig_cpu_err_serv_clears_blob_on_success_input) {
    RS_REQUIRE_ALL(); D3D12_ROOT_SIGNATURE_DESC d; ID3DBlob *b = NULL, *e = (ID3DBlob *)(ULONG_PTR)0x1234; memset(&d, 0, sizeof d);
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v); v.Version = 1; v.Desc_1_0 = d;
    EXPECT_HR(SERV()(&v, &b, &e), S_OK);
    CHECKF(e == NULL, "the error blob pointer is cleared on entry");
    SAFE_RELEASE(b);
}
TEST(d3d12_rootsig_cpu_err_error_blob_refcount) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e = NULL; if (!b) return; rs0_init(b);
    rs0_sampler(b, 0, 0, ALL)->AddressU = (D3D12_TEXTURE_ADDRESS_MODE)0;
    EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), E_INVALID);
    if (e) {
        EQV(ID3D10Blob_AddRef(e), 2); EQV(ID3D10Blob_Release(e), 1); EQV(ID3D10Blob_Release(e), 0);
    } else CHECK(e != NULL);
    free(b);
}
TEST(d3d12_rootsig_cpu_err_success_blob_refcount) {
    RS_REQUIRE_ALL(); D3D12_ROOT_SIGNATURE_DESC d; ID3DBlob *bl = NULL, *e = NULL; memset(&d, 0, sizeof d);
    EXPECT_HR(rs_ser0(&d, &bl, &e), S_OK);
    if (bl) { EQV(ID3D10Blob_AddRef(bl), 2); EQV(ID3D10Blob_Release(bl), 1); EQV(ID3D10Blob_Release(bl), 0); }
}
TEST(d3d12_rootsig_cpu_err_two_blobs_distinct) {
    RS_REQUIRE_ALL(); D3D12_ROOT_SIGNATURE_DESC d; ID3DBlob *a = NULL, *b = NULL, *e = NULL; memset(&d, 0, sizeof d);
    EXPECT_HR(rs_ser0(&d, &a, &e), S_OK); EXPECT_HR(rs_ser0(&d, &b, &e), S_OK);
    CHECK(a && b && a != b);
    if (a && b) CHECK(ID3D10Blob_GetBufferPointer(a) != ID3D10Blob_GetBufferPointer(b));
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

/* ---------------- root signature flags ---------------- */
F0(flags_local_with_ia, "D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE combined with other flag(s) that don't make sense (root signature flags 81).\n", 0x81, (void)0)
F0(flags_local_with_deny_vs, "D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE combined with other flag(s) that don't make sense (root signature flags 82).\n", 0x82, (void)0)
F0(flags_local_with_so, "D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE combined with other flag(s) that don't make sense (root signature flags c0).\n", 0xc0, (void)0)
F0(flags_local_with_deny_ms, "D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE combined with other flag(s) that don't make sense (root signature flags 280).\n", 0x280, (void)0)
F0(flags_local_param_not_all_vis, "Local root signatures only support shader visibility flag D3D12_SHADER_VISIBILITY_ALL.  ShaderVisibility value: 1 (root parameter [0]).\n",
   0x80, rs0_root(b, RCBV, 0, 0, VS))
F0(flags_local_param_second_not_all_vis, "Local root signatures only support shader visibility flag D3D12_SHADER_VISIBILITY_ALL.  ShaderVisibility value: 5 (root parameter [1]).\n",
   0x80, rs0_root(b, RCBV, 0, 0, ALL); rs0_consts(b, 1, 0, 1, PS))
F0(flags_local_sampler_not_all_vis, "Local root signatures only support shader visibility flag D3D12_SHADER_VISIBILITY_ALL.  ShaderVisibility value: 5 (static sampler [0]).\n",
   0x80, rs0_sampler(b, 0, 0, PS))
F1(flags_local_with_ia_v11, "D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE combined with other flag(s) that don't make sense (root signature flags 81).\n", 0x81, (void)0)

TEST(d3d12_rootsig_cpu_err_flags_local_alone_ok) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e; if (!b) return; rs0_init(b);
    rs0_root(b, RCBV, 0, 0, ALL); rs0_consts(b, 1, 0, 2, ALL); rs0_sampler(b, 0, 0, ALL);
    EXPECT_HR(rs_ser0(rs0_done(b, 0x80), &bl, &e), S_OK);
    { RSBUF m; rs_model0(&b->d, &m); rs_blob_matches(bl, &m, "local"); }
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
}
TEST(d3d12_rootsig_cpu_err_flags_unknown_high_bits) {
    RS_REQUIRE_ALL(); static const UINT fl[] = { 0x1000, 0x2000, 0x10000, 0x1000000, 0x40000000 }; size_t i;
    for (i = 0; i < sizeof fl / sizeof *fl; i++) {
        D3D12_ROOT_SIGNATURE_DESC d; ID3DBlob *b = NULL, *e = NULL; char want[128];
        memset(&d, 0, sizeof d); d.Flags = fl[i];
        HRESULT hr = rs_ser0(&d, &b, &e);
        if (SUCCEEDED(hr)) { CHECKF(0, "flags %x accepted (Win10 22H2 rejects undefined bits)", fl[i]); SAFE_RELEASE(b); SAFE_RELEASE(e); continue; }
        EXPECT_HR(hr, E_INVALID);
        sprintf(want, "Unsupported bit-flag set (root signature flags %x).\n", fl[i]);
        check_err(e, want, "high flag"); SAFE_RELEASE(e);
    }
}
TEST(d3d12_rootsig_cpu_err_flags_directly_indexed_heaps) {
    RS_REQUIRE_ALL(); static const UINT fl[] = { 0x400, 0x800, 0xc00 }; size_t i;
    for (i = 0; i < sizeof fl / sizeof *fl; i++) {
        D3D12_ROOT_SIGNATURE_DESC d; ID3DBlob *b = NULL, *e = NULL; char want[128];
        memset(&d, 0, sizeof d); d.Flags = fl[i];
        HRESULT hr = rs_ser0(&d, &b, &e);
        if (SUCCEEDED(hr)) { CHECKF(0, "flags %x accepted (Win10 22H2 d3d12 predates directly indexed heaps)", fl[i]); SAFE_RELEASE(b); SAFE_RELEASE(e); continue; }
        EXPECT_HR(hr, E_INVALID);
        sprintf(want, "Unsupported bit-flag set (root signature flags %x).\n", fl[i]);
        check_err(e, want, "dir idx flag"); SAFE_RELEASE(e);
    }
}

/* ---------------- shader visibility ---------------- */
F0(vis_param_8, "Unsupported ShaderVisibility value 8 (root parameter [0]).\n", 0, rs0_root(b, RCBV, 0, 0, (D3D12_SHADER_VISIBILITY)8))
F0(vis_param_100, "Unsupported ShaderVisibility value 100 (root parameter [2]).\n", 0,
   rs0_root(b, RCBV, 0, 0, ALL); rs0_root(b, RCBV, 1, 0, ALL); rs0_consts(b, 2, 0, 1, (D3D12_SHADER_VISIBILITY)100))
F0(vis_param_negative, "Unsupported ShaderVisibility value 4294967295 (root parameter [0]).\n", 0, rs0_root(b, RSRV, 0, 0, (D3D12_SHADER_VISIBILITY)-1))
F0(vis_table_bad, "Unsupported ShaderVisibility value 9 (root parameter [1]).\n", 0,
   rs0_root(b, RCBV, 0, 0, ALL); rs0_table(b, (D3D12_SHADER_VISIBILITY)9); rs0_range(b, SRV, 1, 0, 0, APPEND))
F0(vis_sampler_bad, "Unsupported ShaderVisibility value 8 (static sampler [0]).\n", 0, rs0_sampler(b, 0, 0, (D3D12_SHADER_VISIBILITY)8))
F0(vis_sampler_bad_second, "Unsupported ShaderVisibility value 12 (static sampler [1]).\n", 0,
   rs0_sampler(b, 0, 0, ALL); rs0_sampler(b, 1, 0, (D3D12_SHADER_VISIBILITY)12))
F1(vis_param_8_v11, "Unsupported ShaderVisibility value 8 (root parameter [0]).\n", 0, rs1_root(b, RCBV, 0, 0, 0, (D3D12_SHADER_VISIBILITY)8))
F1(vis_sampler_bad_v11, "Unsupported ShaderVisibility value 8 (static sampler [0]).\n", 0, rs1_sampler(b, 0, 0, (D3D12_SHADER_VISIBILITY)8))
F0(vis_first_error_wins, "Unsupported ShaderVisibility value 8 (root parameter [0]).\n", 0,
   rs0_root(b, RCBV, 0, 0, (D3D12_SHADER_VISIBILITY)8); rs0_root(b, RCBV, 0, 0, (D3D12_SHADER_VISIBILITY)9))

/* ---------------- parameter type ---------------- */
TEST(d3d12_rootsig_cpu_err_param_type_v10_no_text) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; rs0_init(b);
    rs0_root(b, RCBV, 0, 0, ALL); b->p[0].ParameterType = (D3D12_ROOT_PARAMETER_TYPE)5;
    /* the v1.0 -> v1.1 upgrade done before verification fails without producing text */
    fail0(rs0_done(b, 0), NULL, NULL, "param_type_5"); free(b);
}
F1(param_type_5_v11, "Unsupported ParameterType value 5 (root parameter 0)\n", 0, rs1_root(b, RCBV, 0, 0, 0, ALL); b->p[0].ParameterType = (D3D12_ROOT_PARAMETER_TYPE)5)
F1(param_type_77_second, "Unsupported ParameterType value 77 (root parameter 1)\n", 0,
   rs1_consts(b, 0, 0, 1, ALL); rs1_consts(b, 1, 0, 1, ALL); b->p[1].ParameterType = (D3D12_ROOT_PARAMETER_TYPE)77)

/* ---------------- descriptor tables ---------------- */
F0(table_range_type_4, "Unsupported RangeType value 4 (descriptor table slot [0], root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, (D3D12_DESCRIPTOR_RANGE_TYPE)4, 1, 0, 0, APPEND))
F0(table_range_type_second_slot, "Unsupported RangeType value 9 (descriptor table slot [1], root parameter [1]).\n", 0,
   rs0_root(b, RCBV, 0, 0, ALL); rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, APPEND); rs0_range(b, (D3D12_DESCRIPTOR_RANGE_TYPE)9, 1, 5, 0, APPEND))
F1(table_range_type_4_v11, "Unsupported RangeType value 4 (descriptor table slot [0], root parameter [0]).\n", 0,
   rs1_table(b, ALL); rs1_range(b, (D3D12_DESCRIPTOR_RANGE_TYPE)4, 1, 0, 0, 0, APPEND))
F0(table_sampler_then_srv, "Samplers cannot be mixed with other resource types in a descriptor table (root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, SMP, 1, 0, 0, APPEND); rs0_range(b, SRV, 1, 0, 0, APPEND))
F0(table_srv_then_sampler, "Samplers cannot be mixed with other resource types in a descriptor table (root parameter [1]).\n", 0,
   rs0_root(b, RCBV, 0, 0, ALL); rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, APPEND); rs0_range(b, SMP, 1, 0, 0, APPEND))
F0(table_uav_then_sampler, "Samplers cannot be mixed with other resource types in a descriptor table (root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, UAV, 1, 0, 0, APPEND); rs0_range(b, SMP, 1, 0, 0, APPEND))
F0(table_cbv_sampler_cbv, "Samplers cannot be mixed with other resource types in a descriptor table (root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, CBV, 1, 0, 0, APPEND); rs0_range(b, SMP, 1, 0, 0, APPEND); rs0_range(b, CBV, 1, 1, 0, APPEND))
F1(table_sampler_srv_v11, "Samplers cannot be mixed with other resource types in a descriptor table (root parameter [0]).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SMP, 1, 0, 0, 0, APPEND); rs1_range(b, SRV, 1, 0, 0, 0, APPEND))
F0(table_zero_descriptors, "NumDescriptors cannot be 0 (descriptor table slot [0], root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 0, 0, 0, APPEND))
F0(table_zero_descriptors_slot1, "NumDescriptors cannot be 0 (descriptor table slot [1], root parameter [2]).\n", 0,
   rs0_root(b, RCBV, 0, 0, ALL); rs0_root(b, RCBV, 1, 0, ALL); rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, APPEND); rs0_range(b, UAV, 0, 0, 0, APPEND))
F1(table_zero_descriptors_v11, "NumDescriptors cannot be 0 (descriptor table slot [0], root parameter [0]).\n", 0,
   rs1_table(b, ALL); rs1_range(b, CBV, 0, 0, 0, 0, APPEND))
F0(table_append_after_unbounded, "Cannot append range with implicit lower bound after an unbounded range (descriptor table slot [1], root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, SRV, UNB, 0, 0, APPEND); rs0_range(b, UAV, 1, 0, 0, APPEND))
F0(table_append_after_unbounded_slot2, "Cannot append range with implicit lower bound after an unbounded range (descriptor table slot [2], root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 2, 0, 0, APPEND); rs0_range(b, SRV, UNB, 2, 0, APPEND); rs0_range(b, CBV, 1, 0, 0, APPEND))
F0(table_append_after_unbounded_explicit_first, "Cannot append range with implicit lower bound after an unbounded range (descriptor table slot [1], root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, UAV, UNB, 0, 0, 0); rs0_range(b, SRV, 1, 0, 0, APPEND))
F1(table_append_after_unbounded_v11, "Cannot append range with implicit lower bound after an unbounded range (descriptor table slot [1], root parameter [0]).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SRV, UNB, 0, 0, 0, APPEND); rs1_range(b, SRV, 1, 1, 0, 0, APPEND))
F0(table_register_overflow, "Overflow for shader register range: BaseShaderRegister=4294967280, NumDescriptor=32; (descriptor table slot [0], root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 32, 0xfffffff0u, 0, APPEND))
F0(table_register_overflow_by_one, "Overflow for shader register range: BaseShaderRegister=4294967295, NumDescriptor=2; (descriptor table slot [0], root parameter [0]).\n", 0,
   rs0_table(b, ALL); rs0_range(b, CBV, 2, 0xffffffffu, 0, APPEND))
F0(table_descriptor_overflow, "Overflow for descriptor range (descriptor table slot [0], root parameter [0])\n", 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 32, 0, 0, 0xfffffff0u))
F0(table_descriptor_overflow_by_append, "Overflow for descriptor range (descriptor table slot [1], root parameter [0])\n", 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 0xfffffff0u, 0, 0, APPEND); rs0_range(b, UAV, 0x20, 0, 0, APPEND))

TEST(d3d12_rootsig_cpu_err_table_boundary_ok) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e; if (!b) return; rs0_init(b);
    rs0_table(b, ALL); rs0_range(b, SRV, 1, 0xffffffffu, 0, APPEND); rs0_range(b, UAV, 0xfffffff0u, 0xf, 0, APPEND);
    EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), S_OK);
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
}
TEST(d3d12_rootsig_cpu_err_table_range_overlap_in_descriptor_space_accepted) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e; if (!b) return; rs0_init(b);
    /* explicit offsets that overlap in the descriptor heap range are not rejected by the serializer */
    rs0_table(b, ALL); rs0_range(b, SRV, 4, 0, 0, 0); rs0_range(b, UAV, 4, 0, 0, 2);
    HRESULT hr = rs_ser0(rs0_done(b, 0), &bl, &e);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) { RSBUF m; rs_model0(&b->d, &m); rs_blob_matches(bl, &m, "overlap accepted"); }
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
}
TEST(d3d12_rootsig_cpu_err_table_empty_ok) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e; if (!b) return; rs0_init(b);
    rs0_table(b, ALL);
    EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), S_OK);
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
}

/* ---------------- v1.1 range / root descriptor flags ---------------- */
#define F_DV  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
#define F_DDV D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE
#define F_DSE D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
#define F_DS  D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC
#define F_BC  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS
F1(range_flags_unsupported_bit_0x20, "Unsupported bit-flag set (descriptor range flags 20).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, 0x20, APPEND))
F1(range_flags_unsupported_bit_0x20000, "Unsupported bit-flag set (descriptor range flags 20000).\n", 0,
   rs1_table(b, ALL); rs1_range(b, UAV, 1, 0, 0, 0x20000, APPEND))
F1(range_flags_unsupported_bit_0x10, "Unsupported bit-flag set (descriptor range flags 10).\n", 0,
   rs1_table(b, ALL); rs1_range(b, CBV, 1, 0, 0, 0x10, APPEND))
F1(range_flags_unsupported_bit_high, "Unsupported bit-flag set (descriptor range flags 80000000).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, 0x80000000u, APPEND))
F1(range_flags_unsupported_bit_second_range, "Unsupported bit-flag set (descriptor range flags 100).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, 0, APPEND); rs1_range(b, SRV, 1, 1, 0, 0x100, APPEND))
F1(range_flags_two_descriptor_flags, "Descriptor range flags cannot specify more than one DESCRIPTOR_* flag at a time (descriptor range flags 10001).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DV | F_BC, APPEND))
F1(range_flags_two_data_flags_dv_ds, "Descriptor range flags cannot specify more than one DATA_* flag at a time (descriptor range flags a).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DDV | F_DS, APPEND))
F1(range_flags_two_data_flags_dv_dse, "Descriptor range flags cannot specify more than one DATA_* flag at a time (descriptor range flags 6).\n", 0,
   rs1_table(b, ALL); rs1_range(b, UAV, 1, 0, 0, F_DDV | F_DSE, APPEND))
F1(range_flags_two_data_flags_dse_ds, "Descriptor range flags cannot specify more than one DATA_* flag at a time (descriptor range flags c).\n", 0,
   rs1_table(b, ALL); rs1_range(b, CBV, 1, 0, 0, F_DSE | F_DS, APPEND))
F1(range_flags_three_data_flags, "Descriptor range flags cannot specify more than one DATA_* flag at a time (descriptor range flags e).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DDV | F_DSE | F_DS, APPEND))
F1(range_flags_volatile_with_data_static,
   "Descriptor range flags cannot specify DESCRIPTORS_VOLATILE with the DATA_STATIC flag at the same time (descriptor range flags 9). DATA_STATIC_WHILE_SET_AT_EXECUTE is fine to combine with DESCRIPTORS_VOLATILE, since DESCRIPTORS_VOLATILE still requires descriptors don't change during execution. \n", 0,
   rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DV | F_DS, APPEND))
F1(range_flags_sampler_data_volatile, "Sampler descriptor ranges can't specify DATA_* flags since there is no data pointed to by samplers (descriptor range flags 2).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SMP, 1, 0, 0, F_DDV, APPEND))
F1(range_flags_sampler_data_static, "Sampler descriptor ranges can't specify DATA_* flags since there is no data pointed to by samplers (descriptor range flags 8).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SMP, 1, 0, 0, F_DS, APPEND))
F1(range_flags_sampler_data_static_dv, "Sampler descriptor ranges can't specify DATA_* flags since there is no data pointed to by samplers (descriptor range flags 9).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SMP, 1, 0, 0, F_DV | F_DS, APPEND))
F1(range_flags_sampler_bounds_checks, "Sampler descriptor ranges can't specify DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS, since there are no bounds to check (descriptor range flags 10000).\n", 0,
   rs1_table(b, ALL); rs1_range(b, SMP, 1, 0, 0, F_BC, APPEND))

TEST(d3d12_rootsig_cpu_err_range_flags_valid_combinations) {
    RS_REQUIRE_ALL();
    static const UINT ok[] = { 0, F_DV, F_DDV, F_DSE, F_DS, F_BC, F_DV | F_DDV, F_DV | F_DSE, F_BC | F_DDV, F_BC | F_DSE, F_BC | F_DS };
    size_t i; int t;
    for (t = 0; t < 3; t++) for (i = 0; i < sizeof ok / sizeof *ok; i++) {
        RS1 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL, *e = NULL; if (!b) return; rs1_init(b);
        rs1_table(b, ALL); rs1_range(b, (D3D12_DESCRIPTOR_RANGE_TYPE)t, 1, 0, 0, ok[i], APPEND);
        HRESULT hr = rs_serv1(rs1_done(b, 0), &bl, &e);
        CHECKF(hr == S_OK, "type %d flags %x: hr=%08lx %s", t, ok[i], (unsigned long)hr, e ? rs_errstr(e) : "");
        SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
    }
}
TEST(d3d12_rootsig_cpu_err_range_flags_sampler_valid_combinations) {
    RS_REQUIRE_ALL(); static const UINT ok[] = { 0, F_DV }; size_t i;
    for (i = 0; i < 2; i++) {
        RS1 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL, *e = NULL; if (!b) return; rs1_init(b);
        rs1_table(b, ALL); rs1_range(b, SMP, 1, 0, 0, ok[i], APPEND);
        EXPECT_HR(rs_serv1(rs1_done(b, 0), &bl, &e), S_OK);
        SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
    }
}
F1(root_desc_flags_bit0, "Unsupported bit-flag set (root descriptor flags 1).\n", 0, rs1_root(b, RCBV, 0, 0, 1, ALL))
F1(root_desc_flags_bit4, "Unsupported bit-flag set (root descriptor flags 10).\n", 0, rs1_root(b, RSRV, 0, 0, 0x10, ALL))
F1(root_desc_flags_bit31, "Unsupported bit-flag set (root descriptor flags 80000000).\n", 0, rs1_root(b, RUAV, 0, 0, 0x80000000u, ALL))
F1(root_desc_flags_dv_ds, "Root descriptor flags cannot specify more than one DATA_* flag at a time (root descriptor flags a).\n", 0,
   rs1_root(b, RCBV, 0, 0, 2 | 8, ALL))
F1(root_desc_flags_dv_dse, "Root descriptor flags cannot specify more than one DATA_* flag at a time (root descriptor flags 6).\n", 0,
   rs1_root(b, RUAV, 0, 0, 2 | 4, ALL))
F1(root_desc_flags_dse_ds, "Root descriptor flags cannot specify more than one DATA_* flag at a time (root descriptor flags c).\n", 0,
   rs1_root(b, RSRV, 0, 0, 4 | 8, ALL))
F1(root_desc_flags_all_three, "Root descriptor flags cannot specify more than one DATA_* flag at a time (root descriptor flags e).\n", 0,
   rs1_root(b, RSRV, 0, 0, 14, ALL))
TEST(d3d12_rootsig_cpu_err_root_desc_flags_valid) {
    RS_REQUIRE_ALL(); static const UINT ok[] = { 0, 2, 4, 8 }; size_t i; int t;
    for (t = 0; t < 3; t++) for (i = 0; i < 4; i++) {
        RS1 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL, *e = NULL; if (!b) return; rs1_init(b);
        rs1_root(b, (D3D12_ROOT_PARAMETER_TYPE)(RCBV + t), 0, 0, ok[i], ALL);
        HRESULT hr = rs_serv1(rs1_done(b, 0), &bl, &e);
        CHECKF(hr == S_OK, "type %d flags %x: hr=%08lx", RCBV + t, ok[i], (unsigned long)hr);
        SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
    }
}

/* ---------------- register range overlaps ---------------- */
#define OV(type, newp, oldp) "Shader register range of type " type " " newp " overlaps with another shader register range " oldp ".\n"
F0(overlap_cbv_cbv, OV("CBV", "(root parameter [1], visibility ALL)", "(root parameter [0], visibility ALL)"), 0,
   rs0_root(b, RCBV, 0, 0, ALL); rs0_root(b, RCBV, 0, 0, ALL))
F0(overlap_srv_srv, OV("SRV", "(root parameter [1], visibility ALL)", "(root parameter [0], visibility ALL)"), 0,
   rs0_root(b, RSRV, 3, 0, ALL); rs0_root(b, RSRV, 3, 0, ALL))
F0(overlap_consts_cbv, OV("CBV", "(root parameter [1], visibility ALL)", "(root parameter [0], visibility ALL)"), 0,
   rs0_consts(b, 2, 0, 4, ALL); rs0_root(b, RCBV, 2, 0, ALL))
F0(overlap_cbv_consts, OV("CBV", "(root parameter [1], visibility ALL)", "(root parameter [0], visibility ALL)"), 0,
   rs0_root(b, RCBV, 7, 0, ALL); rs0_consts(b, 7, 0, 1, ALL))
F0(overlap_consts_consts, OV("CBV", "(root parameter [2], visibility ALL)", "(root parameter [1], visibility ALL)"), 0,
   rs0_consts(b, 0, 0, 1, ALL); rs0_consts(b, 1, 0, 1, ALL); rs0_consts(b, 1, 0, 3, ALL))
F0(overlap_table_then_root_cbv,
   OV("CBV", "(root parameter [1], visibility ALL)", "(root parameter[0], visibility ALL, descriptor table slot [0])"), 0,
   rs0_table(b, ALL); rs0_range(b, CBV, 4, 0, 0, APPEND); rs0_root(b, RCBV, 3, 0, ALL))
F0(overlap_root_cbv_then_table,
   OV("CBV", "(root parameter [1], visibility ALL, descriptor table slot [0])", "(root parameter [0], visibility ALL)"), 0,
   rs0_root(b, RCBV, 0, 0, ALL); rs0_table(b, ALL); rs0_range(b, CBV, 1, 0, 0, APPEND))
F0(overlap_table_slot1,
   OV("SRV", "(root parameter [1], visibility ALL, descriptor table slot [1])", "(root parameter[0], visibility ALL, descriptor table slot [0])"), 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 4, 0, 0, APPEND); rs0_table(b, ALL); rs0_range(b, UAV, 1, 0, 0, APPEND); rs0_range(b, SRV, 2, 3, 0, APPEND))
F0(overlap_partial_range_end, OV("SRV", "(root parameter [1], visibility ALL)", "(root parameter[0], visibility ALL, descriptor table slot [0])"), 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 4, 0, 0, APPEND); rs0_root(b, RSRV, 3, 0, ALL))
F0(overlap_unbounded_vs_high_register, OV("SRV", "(root parameter [1], visibility ALL)", "(root parameter[0], visibility ALL, descriptor table slot [0])"), 0,
   rs0_table(b, ALL); rs0_range(b, SRV, UNB, 4, 0, APPEND); rs0_root(b, RSRV, 1000, 0, ALL))
F0(overlap_unbounded_vs_unbounded, OV("UAV", "(root parameter [1], visibility ALL, descriptor table slot [0])", "(root parameter[0], visibility ALL, descriptor table slot [0])"), 0,
   rs0_table(b, ALL); rs0_range(b, UAV, UNB, 0, 0, APPEND); rs0_table(b, ALL); rs0_range(b, UAV, UNB, 5, 0, APPEND))
F0P(overlap_all_then_vertex, "Shader register range of type CBV (root parameter [1], visibility ", 0,
   rs0_root(b, RCBV, 0, 0, ALL); rs0_root(b, RCBV, 0, 0, VS))
F0P(overlap_vertex_then_all, "Shader register range of type CBV (root parameter [1], visibility ALL) overlaps with another shader register range (root parameter [0], visibility ", 0,
   rs0_root(b, RCBV, 0, 0, VS); rs0_root(b, RCBV, 0, 0, ALL))
F0P(overlap_vertex_then_vertex, "Shader register range of type UAV (root parameter [1], visibility ", 0,
   rs0_root(b, RUAV, 0, 0, VS); rs0_root(b, RUAV, 0, 0, VS))
F0P(overlap_pixel_pixel, "Shader register range of type SRV (root parameter [1], visibility ", 0,
   rs0_root(b, RSRV, 0, 0, PS); rs0_root(b, RSRV, 0, 0, PS))
F0P(overlap_later_visibility_all_vs_pixel, "Shader register range of type SRV (root parameter [2], visibility ALL) overlaps with another shader register range (root parameter [1], visibility ", 0,
   rs0_root(b, RSRV, 5, 0, VS); rs0_root(b, RSRV, 0, 0, PS); rs0_root(b, RSRV, 0, 0, ALL))
TEST(d3d12_rootsig_cpu_err_overlap_visibility_names) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e; if (!b) return; rs0_init(b);
    rs0_root(b, RCBV, 0, 0, VS); rs0_root(b, RCBV, 0, 0, PS);
    EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), S_OK);   /* different stages never overlap */
    SAFE_RELEASE(bl); SAFE_RELEASE(e); rs0_init(b);
    rs0_root(b, RCBV, 0, 0, VS); rs0_root(b, RCBV, 0, 0, VS);
    EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), E_INVALID);
    CHECK(e != NULL);
    if (e) CHECKF(strstr(rs_errstr(e), "(root parameter [1], visibility VERTEX) overlaps with another shader register range (root parameter [0], visibility VERTEX).") != NULL, "text: %s", rs_errstr(e));
    SAFE_RELEASE(e); free(b);
}
F0(overlap_in_nonzero_space, OV("CBV", "(root parameter [1], visibility ALL)", "(root parameter [0], visibility ALL)"), 0,
   rs0_root(b, RCBV, 4, 9, ALL); rs0_root(b, RCBV, 4, 9, ALL))
F1(overlap_cbv_cbv_v11, OV("CBV", "(root parameter [1], visibility ALL)", "(root parameter [0], visibility ALL)"), 0,
   rs1_root(b, RCBV, 0, 0, 0, ALL); rs1_root(b, RCBV, 0, 0, 0, ALL))
F1(overlap_table_table_v11,
   OV("SRV", "(root parameter [1], visibility ALL, descriptor table slot [0])", "(root parameter[0], visibility ALL, descriptor table slot [0])"), 0,
   rs1_table(b, ALL); rs1_range(b, SRV, 2, 0, 0, 0, APPEND); rs1_table(b, ALL); rs1_range(b, SRV, 2, 1, 0, 0, APPEND))
F0P(overlap_static_sampler_pair, "Shader register range of type ", 0, rs0_sampler(b, 0, 0, ALL); rs0_sampler(b, 0, 0, ALL))
F0P(overlap_static_sampler_vs_all, "Shader register range of type ", 0, rs0_sampler(b, 2, 0, VS); rs0_sampler(b, 2, 0, ALL))
F0P(overlap_static_sampler_vs_table_sampler, "Shader register range of type ", 0,
   rs0_sampler(b, 2, 0, ALL); rs0_table(b, ALL); rs0_range(b, SMP, 4, 0, 0, APPEND))
TEST(d3d12_rootsig_cpu_err_overlap_static_sampler_text) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e; if (!b) return; rs0_init(b);
    rs0_sampler(b, 5, 0, ALL); rs0_sampler(b, 5, 0, ALL);
    EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), E_INVALID);
    if (e) {
        const char *s = rs_errstr(e);
        CHECKF(strstr(s, " (static sampler [1], visibility ALL) overlaps with another shader register range (static sampler [0], visibility ALL).\n") != NULL, "text: %s", s);
        CHECKF(!strncmp(s, "Shader register range of type ", 30), "text: %s", s);
    } else CHECK(e != NULL);
    SAFE_RELEASE(e); free(b);
}
TEST(d3d12_rootsig_cpu_err_overlap_type_names) {
    RS_REQUIRE_ALL(); static const char *const names[] = { "SRV", "UAV", "CBV" }; int t;
    for (t = 0; t < 3; t++) {
        RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e; char want[256]; if (!b) return; rs0_init(b);
        rs0_root(b, (D3D12_ROOT_PARAMETER_TYPE)(RCBV + (t == 0 ? 1 : t == 1 ? 2 : 0)), 0, 0, ALL);
        rs0_root(b, (D3D12_ROOT_PARAMETER_TYPE)(RCBV + (t == 0 ? 1 : t == 1 ? 2 : 0)), 0, 0, ALL);
        EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), E_INVALID);
        sprintf(want, OV("%s", "(root parameter [1], visibility ALL)", "(root parameter [0], visibility ALL)"), names[t]);
        check_err(e, want, "type name"); SAFE_RELEASE(e); free(b);
    }
}
TEST(d3d12_rootsig_cpu_err_no_overlap_cases_succeed) {
    RS_REQUIRE_ALL(); int c;
    for (c = 0; c < 9; c++) {
        RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL, *e = NULL; if (!b) return; rs0_init(b);
        switch (c) {
        case 0: rs0_root(b, RCBV, 0, 0, ALL); rs0_root(b, RSRV, 0, 0, ALL); rs0_root(b, RUAV, 0, 0, ALL); break; /* different types */
        case 1: rs0_root(b, RCBV, 0, 0, VS); rs0_root(b, RCBV, 0, 0, PS); break; /* different stages */
        case 2: rs0_root(b, RCBV, 0, 0, ALL); rs0_root(b, RCBV, 0, 1, ALL); break; /* different spaces */
        case 3: rs0_root(b, RCBV, 0, 0, ALL); rs0_root(b, RCBV, 1, 0, ALL); break; /* adjacent registers */
        case 4: rs0_table(b, ALL); rs0_range(b, SRV, 4, 0, 0, APPEND); rs0_root(b, RSRV, 4, 0, ALL); break;
        case 5: rs0_table(b, ALL); rs0_range(b, SRV, UNB, 4, 0, APPEND); rs0_root(b, RSRV, 3, 0, ALL); break;
        case 6: rs0_sampler(b, 0, 0, ALL); rs0_sampler(b, 0, 1, ALL); break;
        case 7: rs0_sampler(b, 0, 0, VS); rs0_sampler(b, 0, 0, PS); break;
        default: rs0_sampler(b, 0, 0, ALL); rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, APPEND); break; /* sampler vs srv */
        }
        HRESULT hr = rs_ser0(rs0_done(b, 0), &bl, &e);
        CHECKF(hr == S_OK, "case %d: hr=%08lx %s", c, (unsigned long)hr, e ? rs_errstr(e) : "");
        SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
    }
}

/* ---------------- static samplers ---------------- */
static D3D12_STATIC_SAMPLER_DESC *one_sampler(RS0 *b) { rs0_init(b); return rs0_sampler(b, 0, 0, ALL); }
#define SS(name, want, ...) TEST(d3d12_rootsig_cpu_err_sampler_##name) { RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; \
    D3D12_STATIC_SAMPLER_DESC *s = one_sampler(b); __VA_ARGS__; fail0(rs0_done(b, 0), want, NULL, #name); free(b); }
#define SSP(name, prefix, ...) TEST(d3d12_rootsig_cpu_err_sampler_##name) { RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; \
    D3D12_STATIC_SAMPLER_DESC *s = one_sampler(b); __VA_ARGS__; fail0(rs0_done(b, 0), NULL, prefix, #name); free(b); }
static float f_from_bits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

SS(filter_2, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)2)
SS(filter_3, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)3)
SS(filter_0x56, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)0x56)
SS(filter_0x82, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)0x82)
SS(filter_0x83, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)0x83)
SS(filter_0xd4, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)0xd4)
SS(filter_0x102, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)0x102)
SS(filter_0x156, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)0x156)
SS(filter_0x200, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)0x200)
SS(filter_all_ones, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)0xffffffffu)
SS(filter_wins_over_other_errors, "Static sampler: Filter unrecognized.", s->Filter = (D3D12_FILTER)2; s->AddressU = (D3D12_TEXTURE_ADDRESS_MODE)0; s->MaxAnisotropy = 99)
SS(address_u_0, "Static sampler: AddressU unrecognized.", s->AddressU = (D3D12_TEXTURE_ADDRESS_MODE)0)
SS(address_u_6, "Static sampler: AddressU unrecognized.", s->AddressU = (D3D12_TEXTURE_ADDRESS_MODE)6)
SS(address_v_0, "Static sampler: AddressV unrecognized.", s->AddressV = (D3D12_TEXTURE_ADDRESS_MODE)0)
SS(address_v_7, "Static sampler: AddressV unrecognized.", s->AddressV = (D3D12_TEXTURE_ADDRESS_MODE)7)
SS(address_w_0, "Static sampler: AddressW unrecognized.", s->AddressW = (D3D12_TEXTURE_ADDRESS_MODE)0)
SS(address_w_big, "Static sampler: AddressW unrecognized.", s->AddressW = (D3D12_TEXTURE_ADDRESS_MODE)0x10000)
SS(address_u_wins_over_v, "Static sampler: AddressU unrecognized.", s->AddressU = (D3D12_TEXTURE_ADDRESS_MODE)0; s->AddressV = (D3D12_TEXTURE_ADDRESS_MODE)0)
SS(address_v_wins_over_w, "Static sampler: AddressV unrecognized.", s->AddressV = (D3D12_TEXTURE_ADDRESS_MODE)0; s->AddressW = (D3D12_TEXTURE_ADDRESS_MODE)0)
SSP(lod_bias_too_big, "Static sampler: MipLODBias must be in the range [", s->MipLODBias = 16.0f)
SSP(lod_bias_too_small, "Static sampler: MipLODBias must be in the range [", s->MipLODBias = -16.5f)
SSP(lod_bias_just_over, "Static sampler: MipLODBias must be in the range [", s->MipLODBias = 15.9901f)
SSP(lod_bias_nan, "Static sampler: MipLODBias must be in the range [", s->MipLODBias = f_from_bits(0x7fc00000u))
SSP(lod_bias_inf, "Static sampler: MipLODBias must be in the range [", s->MipLODBias = f_from_bits(0x7f800000u))
SSP(lod_bias_neg_inf, "Static sampler: MipLODBias must be in the range [", s->MipLODBias = f_from_bits(0xff800000u))
SSP(max_anisotropy_17, "Static sampler: MaxAnisotropy must be in the range [0 to 16].", s->MaxAnisotropy = 17)
SSP(max_anisotropy_huge, "Static sampler: MaxAnisotropy must be in the range [0 to 16].", s->MaxAnisotropy = 0x80000000u)
SSP(max_anisotropy_17_point_filter, "Static sampler: MaxAnisotropy must be in the range [0 to 16].", s->Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; s->MaxAnisotropy = 100)
SS(comparison_func_0, "Static sampler: ComparisonFunc unrecognized.", s->Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT; s->ComparisonFunc = (D3D12_COMPARISON_FUNC)0)
SS(comparison_func_9, "Static sampler: ComparisonFunc unrecognized.", s->Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; s->ComparisonFunc = (D3D12_COMPARISON_FUNC)9)
SS(comparison_func_aniso, "Static sampler: ComparisonFunc unrecognized.", s->Filter = D3D12_FILTER_COMPARISON_ANISOTROPIC; s->MaxAnisotropy = 4; s->ComparisonFunc = (D3D12_COMPARISON_FUNC)0)
SSP(min_lod_nan, "Static sampler: MinLOD be in the range [-INF to +INF].  ", s->MinLOD = f_from_bits(0x7fc00000u))
SSP(max_lod_nan, "MaxLOD be in the range [-INF to +INF].  ", s->MaxLOD = f_from_bits(0x7fc00001u))

TEST(d3d12_rootsig_cpu_err_sampler_text_is_not_newline_terminated) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl, *e; if (!b) return;
    one_sampler(b)->Filter = (D3D12_FILTER)2;
    EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), E_INVALID);
    if (e) { const char *s = rs_errstr(e); size_t n = ID3D10Blob_GetBufferSize(e); CHECK(n > 1 && s[n - 1] == 0 && s[n - 2] != '\n'); }
    else CHECK(e != NULL);
    SAFE_RELEASE(e); free(b);
}
TEST(d3d12_rootsig_cpu_err_sampler_comparison_func_ignored_for_plain_filters) {
    RS_REQUIRE_ALL(); static const UINT cf[] = { 0, 9, 100 }; size_t i;
    for (i = 0; i < 3; i++) {
        RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL, *e = NULL; if (!b) return;
        one_sampler(b)->ComparisonFunc = cf[i];
        HRESULT hr = rs_ser0(rs0_done(b, 0), &bl, &e);
        CHECKF(hr == S_OK, "ComparisonFunc %u with a non comparison filter: hr=%08lx", cf[i], (unsigned long)hr);
        SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
    }
}
TEST(d3d12_rootsig_cpu_err_sampler_min_greater_than_max_lod_accepted) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL, *e = NULL; if (!b) return;
    D3D12_STATIC_SAMPLER_DESC *s = one_sampler(b); s->MinLOD = 8.0f; s->MaxLOD = 1.0f;
    HRESULT hr = rs_ser0(rs0_done(b, 0), &bl, &e);
    EXPECT_HR(hr, S_OK);
    if (bl) { RSBUF m; rs_model0(&b->d, &m); rs_blob_matches(bl, &m, "min>max lod"); }
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
}
TEST(d3d12_rootsig_cpu_err_sampler_infinite_lods_accepted) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL, *e = NULL; if (!b) return;
    D3D12_STATIC_SAMPLER_DESC *s = one_sampler(b); s->MinLOD = f_from_bits(0xff800000u); s->MaxLOD = f_from_bits(0x7f800000u);
    EXPECT_HR(rs_ser0(rs0_done(b, 0), &bl, &e), S_OK);
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
}
TEST(d3d12_rootsig_cpu_err_sampler_lod_bias_boundaries_accepted) {
    RS_REQUIRE_ALL(); static const float v[] = { -16.0f, 15.99f, 0.0f, -0.0f, 1e-30f }; size_t i;
    for (i = 0; i < sizeof v / sizeof *v; i++) {
        RS0 *b = calloc(1, sizeof *b); ID3DBlob *bl = NULL, *e = NULL; if (!b) return;
        one_sampler(b)->MipLODBias = v[i];
        HRESULT hr = rs_ser0(rs0_done(b, 0), &bl, &e);
        CHECKF(hr == S_OK, "bias %g: hr=%08lx", v[i], (unsigned long)hr);
        SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
    }
}
TEST(d3d12_rootsig_cpu_err_sampler_second_sampler_error) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; rs0_init(b);
    rs0_sampler(b, 0, 0, ALL); rs0_sampler(b, 1, 0, ALL)->AddressW = (D3D12_TEXTURE_ADDRESS_MODE)0;
    fail0(rs0_done(b, 0), "Static sampler: AddressW unrecognized.", NULL, "second sampler");
    free(b);
}
TEST(d3d12_rootsig_cpu_err_sampler_v11_same_checks) {
    RS_REQUIRE_ALL(); RS1 *b = calloc(1, sizeof *b); if (!b) return; rs1_init(b);
    rs1_sampler(b, 0, 0, ALL)->Filter = (D3D12_FILTER)3;
    fail1(rs1_done(b, 0), "Static sampler: Filter unrecognized.", NULL, "v11 sampler");
    free(b);
}
TEST(d3d12_rootsig_cpu_err_params_checked_before_samplers) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; rs0_init(b);
    rs0_root(b, RCBV, 0, 0, (D3D12_SHADER_VISIBILITY)8); rs0_sampler(b, 0, 0, ALL)->Filter = (D3D12_FILTER)3;
    fail0(rs0_done(b, 0), "Unsupported ShaderVisibility value 8 (root parameter [0]).\n", NULL, "order");
    free(b);
}
TEST(d3d12_rootsig_cpu_err_flags_checked_before_params) {
    RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; rs0_init(b);
    rs0_root(b, RCBV, 0, 0, (D3D12_SHADER_VISIBILITY)8);
    fail0(rs0_done(b, 0x81), "D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE combined with other flag(s) that don't make sense (root signature flags 81).\n", NULL, "order flags");
    free(b);
}
