/* Root signatures written in HLSL: compiled by d3dcompiler_47 and compared with the serializer's view. */
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
#define UNB 0xffffffffu
#define FLT_MAXV 3.402823466e+38f

UDECL(HRESULT, D3DSetBlobPart_t, (const void *data, SIZE_T size, UINT part, UINT flags, const void *p, SIZE_T psize, ID3DBlob **out));

/* the emulator's compiler lacks rootsig targets and attributes: such tests are skipped there */
static int rs_unsup(HRESULT hr) {
    if (hr == (HRESULT)0x80004001 && t_is_wine()) { t_skip("emulator d3dcompiler has no root signature support"); return 1; }
    return 0;
}
/* compile a root signature string with the rootsig_1_x target; returns the container blob */
static HRESULT compile_rs(const char *text, const char *target, ID3DBlob **out) {
    char src[2048];
    snprintf(src, sizeof src, "#define RS \"%s\"\n", text);
    HRESULT hr = d3dh_compile_ex(src, "rs.hlsl", NULL, "RS", target, 0, 0, out);
    return hr;
}
/* RTS0 payload of a container (or of a raw RTS0 part blob) */
static const uint8_t *rts_of(ID3DBlob *b, size_t *n) {
    const uint8_t *p = ID3D10Blob_GetBufferPointer(b); SIZE_T sz = ID3D10Blob_GetBufferSize(b);
    if (sz >= 4 && !memcmp(p, "DXBC", 4)) return rs_find_rts0(p, sz, n);
    *n = sz; return p;
}
static void check_rts_equal(ID3DBlob *b, const RSBUF *m, const char *tag) {
    size_t n = 0; const uint8_t *r = rts_of(b, &n);
    CHECKF(r != NULL, "%s: no RTS0", tag);
    if (!r) return;
    CHECKF(n == m->n * 4, "%s: RTS0 size %u want %u", tag, (unsigned)n, (unsigned)(m->n * 4));
    if (n == m->n * 4) {
        size_t i; for (i = 0; i < n; i++) if (r[i] != ((const uint8_t *)m->w)[i]) break;
        CHECKF(i == n, "%s: RTS0 differs from the serializer model at byte %u (got %02x want %02x)", tag, (unsigned)i, r[i], ((const uint8_t *)m->w)[i]);
    }
}

#define H0(name, text, flags, ...) TEST(d3d12_rootsig_cpu_hlsl_v10_##name) { \
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL; RS0 *b = calloc(1, sizeof *b); RSBUF m; HRESULT hr; if (!b) return; \
    rs0_init(b); __VA_ARGS__; rs0_done(b, flags); rs_model0(&b->d, &m); \
    hr = compile_rs(text, "rootsig_1_0", &code); \
    if (rs_unsup(hr)) { free(b); return; } \
    if (FAILED(hr)) { CHECKF(0, "compile failed hr=%08lx: %s", (unsigned long)hr, d3dh_compile_errors()); } \
    else { check_rts_equal(code, &m, #name); \
        { ID3DBlob *sb = NULL; if (SUCCEEDED(rs_ser0(&b->d, &sb, NULL))) { /* same bytes as the d3d12 serializer when both emit a container */ \
            size_t n1 = 0, n2 = 0; const uint8_t *r1 = rts_of(code, &n1), *r2 = rts_of(sb, &n2); \
            CHECK(r1 && r2 && n1 == n2 && !memcmp(r1, r2, n1)); SAFE_RELEASE(sb); } } } \
    SAFE_RELEASE(code); free(b); }

H0(flags_ia, "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)", 1, (void)0)
H0(flags_zero, "RootFlags(0)", 0, (void)0)
H0(flags_deny_vs_ps, "RootFlags(DENY_VERTEX_SHADER_ROOT_ACCESS | DENY_PIXEL_SHADER_ROOT_ACCESS)", 0x22, (void)0)
H0(flags_deny_hs_ds_gs, "RootFlags(DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS)", 0x1c, (void)0)
H0(flags_stream_output, "RootFlags(ALLOW_STREAM_OUTPUT)", 0x40, (void)0)
H0(flags_ia_so, "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | ALLOW_STREAM_OUTPUT)", 0x41, (void)0)
H0(root_constants, "RootConstants(num32BitConstants=4, b0)", 0, rs0_consts(b, 0, 0, 4, ALL))
H0(root_constants_space_vis, "RootConstants(num32BitConstants=2, b3, space=1, visibility=SHADER_VISIBILITY_PIXEL)", 0, rs0_consts(b, 3, 1, 2, PS))
H0(root_cbv, "CBV(b0)", 0, rs0_root(b, RCBV, 0, 0, ALL))
H0(root_srv_space, "SRV(t2, space=1)", 0, rs0_root(b, RSRV, 2, 1, ALL))
H0(root_uav_vis, "UAV(u5, visibility=SHADER_VISIBILITY_VERTEX)", 0, rs0_root(b, RUAV, 5, 0, VS))
H0(table_cbv_srv_uav, "DescriptorTable(CBV(b0), SRV(t0, numDescriptors=4), UAV(u0))", 0,
   rs0_table(b, ALL); rs0_range(b, CBV, 1, 0, 0, APPEND); rs0_range(b, SRV, 4, 0, 0, APPEND); rs0_range(b, UAV, 1, 0, 0, APPEND))
H0(table_sampler, "DescriptorTable(Sampler(s0, numDescriptors=2))", 0, rs0_table(b, ALL); rs0_range(b, SMP, 2, 0, 0, APPEND))
H0(table_unbounded, "DescriptorTable(SRV(t0, numDescriptors=unbounded))", 0, rs0_table(b, ALL); rs0_range(b, SRV, UNB, 0, 0, APPEND))
H0(table_offsets, "DescriptorTable(SRV(t0, offset=10), UAV(u0, offset=DESCRIPTOR_RANGE_OFFSET_APPEND))", 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, 10); rs0_range(b, UAV, 1, 0, 0, APPEND))
H0(table_space_and_vis, "DescriptorTable(SRV(t4, space=2, numDescriptors=3), visibility=SHADER_VISIBILITY_PIXEL)", 0,
   rs0_table(b, PS); rs0_range(b, SRV, 3, 4, 2, APPEND))
H0(two_tables, "DescriptorTable(SRV(t0)), DescriptorTable(UAV(u0, numDescriptors=2))", 0,
   rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, APPEND); rs0_table(b, ALL); rs0_range(b, UAV, 2, 0, 0, APPEND))
H0(static_sampler_defaults, "StaticSampler(s0)", 0, { D3D12_STATIC_SAMPLER_DESC *s = rs0_sampler(b, 0, 0, ALL);
   s->Filter = D3D12_FILTER_ANISOTROPIC; s->AddressU = s->AddressV = s->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; s->MipLODBias = 0; s->MaxAnisotropy = 16;
   s->ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; s->BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; s->MinLOD = 0; s->MaxLOD = FLT_MAXV; })
H0(static_sampler_all_fields,
   "StaticSampler(s1, filter=FILTER_MIN_MAG_MIP_LINEAR, addressU=TEXTURE_ADDRESS_CLAMP, addressV=TEXTURE_ADDRESS_MIRROR, addressW=TEXTURE_ADDRESS_BORDER, mipLODBias=0.5, maxAnisotropy=8, comparisonFunc=COMPARISON_NEVER, borderColor=STATIC_BORDER_COLOR_TRANSPARENT_BLACK, minLOD=1.0, maxLOD=4.0, space=2, visibility=SHADER_VISIBILITY_PIXEL)", 0,
   { D3D12_STATIC_SAMPLER_DESC *s = rs0_sampler(b, 1, 2, PS);
   s->Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; s->AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; s->AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
   s->AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER; s->MipLODBias = 0.5f; s->MaxAnisotropy = 8; s->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
   s->BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; s->MinLOD = 1.0f; s->MaxLOD = 4.0f; })
H0(static_sampler_comparison, "StaticSampler(s0, filter=FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, comparisonFunc=COMPARISON_GREATER)", 0,
   { D3D12_STATIC_SAMPLER_DESC *s = rs0_sampler(b, 0, 0, ALL); s->Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
   s->MaxAnisotropy = 16; s->AddressU = s->AddressV = s->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
   s->ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER; s->BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; s->MaxLOD = FLT_MAXV; })
H0(combined, "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), RootConstants(num32BitConstants=4, b0), CBV(b1), DescriptorTable(SRV(t0, numDescriptors=2), UAV(u0)), SRV(t5), StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_POINT)", 1,
   rs0_consts(b, 0, 0, 4, ALL); rs0_root(b, RCBV, 1, 0, ALL); rs0_table(b, ALL); rs0_range(b, SRV, 2, 0, 0, APPEND); rs0_range(b, UAV, 1, 0, 0, APPEND);
   rs0_root(b, RSRV, 5, 0, ALL); { D3D12_STATIC_SAMPLER_DESC *s = rs0_sampler(b, 0, 0, ALL); s->Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; s->MaxAnisotropy = 16;
   s->ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; s->BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; s->MaxLOD = FLT_MAXV; })
H0(order_preserved, "UAV(u0), SRV(t0), CBV(b0), RootConstants(num32BitConstants=1, b1)", 0,
   rs0_root(b, RUAV, 0, 0, ALL); rs0_root(b, RSRV, 0, 0, ALL); rs0_root(b, RCBV, 0, 0, ALL); rs0_consts(b, 1, 0, 1, ALL))

/* ---------------- version 1.1 syntax ---------------- */
static const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *deser11(ID3DBlob *code, ID3D12VersionedRootSignatureDeserializer **out) {
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u = NULL; *out = NULL;
    if (FAILED(CRVDES()(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), &IID_ID3D12VersionedRootSignatureDeserializer, (void **)out))) return NULL;
    u = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(*out);
    return u;
}
TEST(d3d12_rootsig_cpu_hlsl_v11_version_dword) {
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL; size_t n = 0; const uint8_t *r; uint32_t v;
    HRESULT hr = compile_rs("CBV(b0)", "rootsig_1_1", &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { CHECKF(0, "compile failed hr=%08lx: %s", (unsigned long)hr, d3dh_compile_errors()); return; }
    r = rts_of(code, &n); CHECK(r && n >= 24);
    if (r && n >= 24) { memcpy(&v, r, 4); EXPECT_EQ(v, 2); }
    SAFE_RELEASE(code);
}
TEST(d3d12_rootsig_cpu_hlsl_v10_version_dword) {
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL; size_t n = 0; const uint8_t *r; uint32_t v;
    HRESULT hr = compile_rs("CBV(b0)", "rootsig_1_0", &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { CHECKF(0, "compile failed hr=%08lx: %s", (unsigned long)hr, d3dh_compile_errors()); return; }
    r = rts_of(code, &n); CHECK(r && n >= 24);
    if (r && n >= 24) { memcpy(&v, r, 4); EXPECT_EQ(v, 1); }
    SAFE_RELEASE(code);
}
static void check_v11_root_flags(const char *text, UINT want, const char *tag) {
    ID3DBlob *code = NULL; ID3D12VersionedRootSignatureDeserializer *d; const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u;
    HRESULT hr = compile_rs(text, "rootsig_1_1", &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { CHECKF(0, "%s: compile failed hr=%08lx: %s", tag, (unsigned long)hr, d3dh_compile_errors()); return; }
    u = deser11(code, &d);
    CHECKF(u && u->Version == D3D_ROOT_SIGNATURE_VERSION_1_1 && u->Desc_1_1.NumParameters == 1, "%s: unexpected description", tag);
    if (u && u->Desc_1_1.NumParameters == 1) {
        const D3D12_ROOT_PARAMETER1 *p = &u->Desc_1_1.pParameters[0];
        if (p->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            CHECKF(p->DescriptorTable.pDescriptorRanges[0].Flags == want, "%s: range flags %x want %x", tag, p->DescriptorTable.pDescriptorRanges[0].Flags, want);
        else CHECKF(p->Descriptor.Flags == want, "%s: root descriptor flags %x want %x", tag, p->Descriptor.Flags, want);
    }
    if (d) ID3D12VersionedRootSignatureDeserializer_Release(d);
    SAFE_RELEASE(code);
}
#define HF(name, text, want) TEST(d3d12_rootsig_cpu_hlsl_v11_flags_##name) { REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); check_v11_root_flags(text, want, #name); }
HF(root_cbv_data_volatile, "CBV(b0, flags=DATA_VOLATILE)", 2)
HF(root_cbv_data_static, "CBV(b0, flags=DATA_STATIC)", 8)
HF(root_srv_data_static_while_set, "SRV(t0, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE)", 4)
HF(root_uav_data_volatile, "UAV(u0, flags=DATA_VOLATILE)", 2)
HF(range_srv_volatile_both, "DescriptorTable(SRV(t0, flags=DESCRIPTORS_VOLATILE | DATA_VOLATILE))", 3)
HF(range_srv_descriptors_volatile, "DescriptorTable(SRV(t0, flags=DESCRIPTORS_VOLATILE))", 1)
HF(range_srv_data_static_while_set, "DescriptorTable(SRV(t0, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE))", 4)
HF(range_uav_data_static, "DescriptorTable(UAV(u0, flags=DATA_STATIC))", 8)
HF(range_cbv_volatile_static_while_set, "DescriptorTable(CBV(b0, flags=DESCRIPTORS_VOLATILE | DATA_STATIC_WHILE_SET_AT_EXECUTE))", 5)
HF(range_srv_bounds_checks, "DescriptorTable(SRV(t0, flags=DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS))", 0x10000)
TEST(d3d12_rootsig_cpu_hlsl_v11_default_flags_observed) {
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL; ID3D12VersionedRootSignatureDeserializer *d; const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u;
    HRESULT hr = compile_rs("CBV(b0), SRV(t0), UAV(u0), DescriptorTable(CBV(b1), SRV(t1), UAV(u1)), DescriptorTable(Sampler(s0))", "rootsig_1_1", &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { CHECKF(0, "compile failed hr=%08lx: %s", (unsigned long)hr, d3dh_compile_errors()); return; }
    u = deser11(code, &d);
    CHECK(u != NULL);
    if (u && u->Desc_1_1.NumParameters == 5) {
        const D3D12_ROOT_PARAMETER1 *p = u->Desc_1_1.pParameters;
        /* Win10 22H2 d3dcompiler_47: unflagged v1.1 root descriptors and ranges are emitted with flags 0 (NONE), not the documented
           DATA_STATIC_WHILE_SET_AT_EXECUTE / DESCRIPTORS_VOLATILE defaults */
        EXPECT_EQ(p[0].Descriptor.Flags, 0); EXPECT_EQ(p[1].Descriptor.Flags, 0); EXPECT_EQ(p[2].Descriptor.Flags, 0);
        EXPECT_EQ(p[3].DescriptorTable.pDescriptorRanges[0].Flags, 0); EXPECT_EQ(p[3].DescriptorTable.pDescriptorRanges[1].Flags, 0);
        EXPECT_EQ(p[3].DescriptorTable.pDescriptorRanges[2].Flags, 0); EXPECT_EQ(p[4].DescriptorTable.pDescriptorRanges[0].Flags, 0);
    } else CHECK(0);
    if (d) ID3D12VersionedRootSignatureDeserializer_Release(d);
    SAFE_RELEASE(code);
}
TEST(d3d12_rootsig_cpu_hlsl_v11_matches_serializer_bytes) {
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL, *ser = NULL; ID3D12VersionedRootSignatureDeserializer *d; const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u;
    HRESULT hr = compile_rs("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), RootConstants(num32BitConstants=2, b0), CBV(b1, flags=DATA_STATIC), DescriptorTable(SRV(t0, numDescriptors=3, flags=DATA_STATIC_WHILE_SET_AT_EXECUTE)), DescriptorTable(Sampler(s4)), StaticSampler(s2)", "rootsig_1_1", &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { CHECKF(0, "compile failed hr=%08lx: %s", (unsigned long)hr, d3dh_compile_errors()); return; }
    u = deser11(code, &d);
    if (u) {
        /* the compiler's output re-serialized by d3d12 gives the same RTS0 bytes */
        hr = SERV()(u, &ser, NULL);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) {
            size_t n1 = 0, n2 = 0; const uint8_t *r1 = rts_of(code, &n1), *r2 = rts_of(ser, &n2);
            CHECK(r1 && r2 && n1 == n2 && !memcmp(r1, r2, n1));
            SAFE_RELEASE(ser);
        }
    } else CHECK(0);
    if (d) ID3D12VersionedRootSignatureDeserializer_Release(d);
    SAFE_RELEASE(code);
}

/* ---------------- attribute on a shader entry point ---------------- */
static HRESULT compile_attr(const char *src, ID3DBlob **code) {
    HRESULT hr = d3dh_compile(src, "main", "vs_5_0", 0, code);
    if (t_is_wine() && (FAILED(hr) || (src[0] == '#' && !rs_find_rts0(ID3D10Blob_GetBufferPointer(*code), ID3D10Blob_GetBufferSize(*code), &(size_t){0}))))
        { if (*code) { ID3D10Blob_Release(*code); *code = NULL; } return (HRESULT)0x80004001; }
    return hr;
}
static const char *k_vs_attr = "#define RS \"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), CBV(b0), DescriptorTable(SRV(t0, numDescriptors=2))\"\n"
    "[RootSignature(RS)] float4 main(float4 p : POSITION) : SV_Position { return p; }\n";
static const char *k_vs_plain = "float4 main(float4 p : POSITION) : SV_Position { return p; }\n";

TEST(d3d12_rootsig_cpu_hlsl_attribute_embeds_rts0) {
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL, *rsc = NULL; HRESULT hr;
    hr = compile_attr(k_vs_attr, &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { CHECKF(0, "compile failed hr=%08lx: %s", (unsigned long)hr, d3dh_compile_errors()); return; }
    {
        size_t n1 = 0, n2 = 0; const uint8_t *r1 = rs_find_rts0(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), &n1), *r2;
        CHECK(r1 != NULL);
        /* Win10 22H2: the [RootSignature] attribute embeds a version 1.1 (RTS0 version dword 2) blob, equal to a rootsig_1_1 compile */
        if (r1 && n1 >= 4) { uint32_t ver; memcpy(&ver, r1, 4); EXPECT_EQ(ver, 2); }
        hr = compile_rs("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), CBV(b0), DescriptorTable(SRV(t0, numDescriptors=2))", "rootsig_1_1", &rsc);
        if (SUCCEEDED(hr)) { r2 = rts_of(rsc, &n2); CHECK(r1 && r2 && n1 == n2 && !memcmp(r1, r2, n1)); }
        else CHECKF(0, "rootsig compile failed: %s", d3dh_compile_errors());
    }
    SAFE_RELEASE(code); SAFE_RELEASE(rsc);
}
TEST(d3d12_rootsig_cpu_hlsl_attribute_shader_accepted_by_deserializer) {
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL; void *p = NULL; HRESULT hr;
    hr = compile_attr(k_vs_attr, &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { t_skip("attribute compile unavailable (hr=%08lx)", (unsigned long)hr); return; }
    hr = CRDES()(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), &IID_ID3D12RootSignatureDeserializer, &p);
    EXPECT_HR(hr, S_OK);
    if (p) {
        const D3D12_ROOT_SIGNATURE_DESC *d = ID3D12RootSignatureDeserializer_GetRootSignatureDesc((ID3D12RootSignatureDeserializer *)p);
        EXPECT_EQ(d->NumParameters, 2); EXPECT_EQ(d->Flags, 1); EXPECT_EQ(d->NumStaticSamplers, 0);
        if (d->NumParameters == 2) { EXPECT_EQ(d->pParameters[0].ParameterType, RCBV); EXPECT_EQ(d->pParameters[1].ParameterType, D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
            EXPECT_EQ(d->pParameters[1].DescriptorTable.pDescriptorRanges[0].NumDescriptors, 2); }
        ID3D12RootSignatureDeserializer_Release((ID3D12RootSignatureDeserializer *)p);
    }
    SAFE_RELEASE(code);
}
TEST(d3d12_rootsig_cpu_hlsl_shader_without_rootsig_rejected_by_deserializer) {
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL; void *p = (void *)(ULONG_PTR)1; HRESULT hr;
    hr = d3dh_compile(k_vs_plain, "main", "vs_5_0", 0, &code);
    if (FAILED(hr)) { t_skip("compile unavailable (hr=%08lx)", (unsigned long)hr); return; }
    hr = CRDES()(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), &IID_ID3D12RootSignatureDeserializer, &p);
    EXPECT_HR(hr, (HRESULT)0x80070057); if (!t_is_wine()) CHECK(p == NULL);
    SAFE_RELEASE(code);
}
TEST(d3d12_rootsig_cpu_hlsl_get_blob_part_root_signature) {
    REQUIRE(d3dh_D3DCompile()); REQUIRE(d3dh_D3DGetBlobPart()); RS_REQUIRE_ALL(); ID3DBlob *code = NULL, *part = NULL; HRESULT hr;
    hr = compile_attr(k_vs_attr, &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { t_skip("attribute compile unavailable (hr=%08lx)", (unsigned long)hr); return; }
    hr = d3dh_D3DGetBlobPart()(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), D3D_BLOB_ROOT_SIGNATURE, 0, &part);
    EXPECT_HR(hr, S_OK);
    if (part) {
        size_t n = 0; const uint8_t *r = rs_find_rts0(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), &n);
        const uint8_t *g = ID3D10Blob_GetBufferPointer(part); SIZE_T gn = ID3D10Blob_GetBufferSize(part);
        CHECK(r != NULL);
        /* Win10: the part comes back wrapped in a DXBC container */
        CHECK(gn >= 4 && !memcmp(g, "DXBC", 4));
        { size_t n2 = 0; const uint8_t *r2 = rs_find_rts0(g, gn, &n2); CHECK(r && r2 && n == n2 && !memcmp(r, r2, n)); }
    }
    SAFE_RELEASE(part); SAFE_RELEASE(code);
}
TEST(d3d12_rootsig_cpu_hlsl_get_blob_part_missing_root_signature) {
    REQUIRE(d3dh_D3DCompile()); REQUIRE(d3dh_D3DGetBlobPart()); ID3DBlob *code = NULL, *part = (ID3DBlob *)(ULONG_PTR)1; HRESULT hr;
    hr = d3dh_compile(k_vs_plain, "main", "vs_5_0", 0, &code);
    if (FAILED(hr)) { t_skip("compile unavailable (hr=%08lx)", (unsigned long)hr); return; }
    hr = d3dh_D3DGetBlobPart()(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), D3D_BLOB_ROOT_SIGNATURE, 0, &part);
    EXPECT_HR(hr, E_FAIL);
    if (FAILED(hr) && part == (ID3DBlob *)(ULONG_PTR)1) part = NULL;
    SAFE_RELEASE(part); SAFE_RELEASE(code);
}
TEST(d3d12_rootsig_cpu_hlsl_strip_root_signature) {
    REQUIRE(d3dh_D3DCompile()); REQUIRE(d3dh_D3DStripShader()); ID3DBlob *code = NULL, *s1 = NULL, *s2 = NULL; HRESULT hr; size_t n;
    hr = compile_attr(k_vs_attr, &code);
    if (rs_unsup(hr)) return; if (FAILED(hr)) { t_skip("attribute compile unavailable (hr=%08lx)", (unsigned long)hr); return; }
    hr = d3dh_D3DStripShader()(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), D3DCOMPILER_STRIP_ROOT_SIGNATURE, &s1);
    EXPECT_HR(hr, S_OK);
    if (s1) CHECK(rs_find_rts0(ID3D10Blob_GetBufferPointer(s1), ID3D10Blob_GetBufferSize(s1), &n) == NULL);
    hr = d3dh_D3DStripShader()(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), D3DCOMPILER_STRIP_REFLECTION_DATA, &s2);
    EXPECT_HR(hr, S_OK);
    if (s2) CHECK(rs_find_rts0(ID3D10Blob_GetBufferPointer(s2), ID3D10Blob_GetBufferSize(s2), &n) != NULL);
    SAFE_RELEASE(s1); SAFE_RELEASE(s2); SAFE_RELEASE(code);
}
TEST(d3d12_rootsig_cpu_hlsl_set_blob_part_root_signature) {
    REQUIRE(d3dh_D3DCompile()); RS_REQUIRE_ALL();
    if (t_is_wine()) { t_skip("emulator d3dcompiler lacks D3DSetBlobPart"); return; }
    D3DSetBlobPart_t_pfn setp = (D3DSetBlobPart_t_pfn)t_proc("D3DSetBlobPart"); REQUIRE(setp);
    ID3DBlob *code = NULL, *out = NULL; RS0 b; HRESULT hr; ID3DBlob *ser = NULL;
    hr = d3dh_compile(k_vs_plain, "main", "vs_5_0", 0, &code);
    if (FAILED(hr)) { t_skip("compile unavailable (hr=%08lx)", (unsigned long)hr); return; }
    rs0_init(&b); rs0_root(&b, RCBV, 0, 0, ALL); rs0_done(&b, 0);
    hr = rs_ser0(&b.d, &ser, NULL); EXPECT_HR(hr, S_OK);
    if (ser) {
        size_t sn = 0; const uint8_t *sr = rs_find_rts0(ID3D10Blob_GetBufferPointer(ser), ID3D10Blob_GetBufferSize(ser), &sn);
        hr = setp(ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), D3D_BLOB_ROOT_SIGNATURE, 0, sr, sn, &out);
        /* Win10: D3DSetBlobPart wants the raw RTS0 payload wrapped as a container; the bare part bytes are D3DERR_INVALIDCALL */
        EXPECT_HR(hr, (HRESULT)0x8876086C);
        CHECK(out == NULL);
    }
    SAFE_RELEASE(out); SAFE_RELEASE(ser); SAFE_RELEASE(code);
}

/* ---------------- compile failures ---------------- */
#define HE(name, text, target, want) TEST(d3d12_rootsig_cpu_hlsl_error_##name) { REQUIRE(d3dh_D3DCompile()); ID3DBlob *code = NULL; HRESULT hr = compile_rs(text, target, &code); \
    if (rs_unsup(hr)) return; CHECKF(FAILED(hr), "compile of \"%s\" unexpectedly succeeded", text); EXPECT_HR(hr, E_FAIL); \
    CHECKF(strstr(d3dh_compile_errors(), want) != NULL, "%s: error text \"%s\" lacks \"%s\"", #name, d3dh_compile_errors(), want); SAFE_RELEASE(code); }
#define X4612(tok) "error X4612: Unexpected token '" tok "'"
#define X4614 "error X4614: RootSignature creation failed: "
HE(syntax_garbage, "this is not a root signature", "rootsig_1_0", X4612("this") " when parsing root signature")
HE(unknown_token, "NoSuchThing(b0)", "rootsig_1_0", X4612("NoSuchThing") " when parsing root signature")
HE(overlap_cbv, "CBV(b0), CBV(b0)", "rootsig_1_0", X4614 "Shader register range of type CBV (root parameter [1], visibility ALL) overlaps with another shader register range (root parameter [0], visibility ALL)")
HE(overlap_constants_cbv, "RootConstants(num32BitConstants=1, b2), CBV(b2)", "rootsig_1_0", X4614 "Shader register range of type CBV (root parameter [1], visibility ALL) overlaps with another shader register range (root parameter [0], vis")
HE(overlap_table_root, "DescriptorTable(SRV(t0, numDescriptors=4)), SRV(t3)", "rootsig_1_0", X4614 "Shader register range of type SRV (root parameter [1], visibility ALL) overlaps with another shader register range (root parameter[0], visibil")
HE(sampler_mixed_in_table, "DescriptorTable(SRV(t0), Sampler(s0))", "rootsig_1_0", X4614 "Samplers cannot be mixed with other resource types in a descriptor table (root parameter [0]).")
HE(zero_descriptors, "DescriptorTable(SRV(t0, numDescriptors=0))", "rootsig_1_0", X4614 "NumDescriptors cannot be 0 (descriptor table slot [0], root parameter [0]).")
HE(append_after_unbounded, "DescriptorTable(SRV(t0, numDescriptors=unbounded), UAV(u0))", "rootsig_1_0", X4614 "Cannot append range with implicit lower bound after an unbounded range (descriptor table slot [1], root parameter [0]).")
HE(sampler_dup, "StaticSampler(s0), StaticSampler(s0)", "rootsig_1_0", X4614 "Shader register range of type SAMPLER (static sampler [1], visibility ALL) overlaps with another shader register range (static sampler [0], visibility ALL)")
HE(bad_aniso, "StaticSampler(s0, maxAnisotropy=17)", "rootsig_1_0", X4614 "Static sampler: MaxAnisotropy must be in the range [0 to 16].  17 specified.")
HE(missing_register, "CBV()", "rootsig_1_0", X4612(")"))
HE(unterminated, "CBV(b0", "rootsig_1_0", X4612(""))
HE(v10_rejects_flags_keyword, "CBV(b0, flags=DATA_STATIC)", "rootsig_1_0", "error X4619: Root descriptor flags cannot be specified for root_sig_1_0")
HE(v11_double_data_flags, "CBV(b0, flags=DATA_VOLATILE | DATA_STATIC)", "rootsig_1_1", X4614 "Root descriptor flags cannot specify more than one DATA_* flag at a time (root descriptor flags a).")
HE(v11_volatile_with_data_static, "DescriptorTable(SRV(t0, flags=DESCRIPTORS_VOLATILE | DATA_STATIC))", "rootsig_1_1", X4614 "Descriptor range flags cannot specify DESCRIPTORS_VOLATILE with the DATA_STATIC flag at the same time (descriptor range flags 9).")
HE(v11_sampler_data_flag, "DescriptorTable(Sampler(s0, flags=DATA_VOLATILE))", "rootsig_1_1", X4614 "Sampler descriptor ranges can't specify DATA_* flags since there is no data pointed to by samplers (descriptor range flags 2).")
