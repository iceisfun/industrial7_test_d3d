#include "ep.h"

static void rel(IUnknown *u) { if (u) u->lpVtbl->Release(u); }
/* Serialized root signatures are DXBC containers holding one RTS0 part. raw*() are the container,
 * bptr/bsize/dw address the RTS0 payload (the actual root signature blob). */
static const BYTE *rawptr(ID3DBlob *b) { return (const BYTE *)ID3D10Blob_GetBufferPointer(b); }
static SIZE_T rawsize(ID3DBlob *b) { return ID3D10Blob_GetBufferSize(b); }
static DWORD rd32(const BYTE *p) { DWORD v; memcpy(&v, p, 4); return v; }
static int g_parse_ok;
static const BYTE *bptr(ID3DBlob *b) {
    const BYTE *r = rawptr(b); SIZE_T n = rawsize(b);
    g_parse_ok = 0;
    if (n < 44 || memcmp(r, "DXBC", 4) || rd32(r + 28) < 1) return r;
    DWORD off = rd32(r + 32);
    if (off + 8 > n || memcmp(r + off, "RTS0", 4)) return r;
    if (off + 8 + rd32(r + off + 4) > n) return r;
    g_parse_ok = 1;
    return r + off + 8;
}
static SIZE_T bsize(ID3DBlob *b) {
    const BYTE *p = bptr(b);
    return g_parse_ok ? rd32(p - 4) : rawsize(b);
}
static DWORD dw(ID3DBlob *b, unsigned i) { const BYTE *p = bptr(b); if (!g_parse_ok || (i + 1) * 4 > bsize(b)) return 0xBAD0BAD0; return rd32(p + i * 4); }

/* A representative description: root CBV, root constants, table with two ranges, one static sampler. */
static D3D12_DESCRIPTOR_RANGE g_ranges[2];
static D3D12_ROOT_PARAMETER g_params[3];
static D3D12_STATIC_SAMPLER_DESC g_sampler;
static D3D12_ROOT_SIGNATURE_DESC g_desc;
static void build_desc(D3D12_ROOT_SIGNATURE_FLAGS flags) {
    memset(g_params, 0, sizeof g_params); memset(g_ranges, 0, sizeof g_ranges); memset(&g_sampler, 0, sizeof g_sampler);
    g_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    g_params[0].Descriptor.ShaderRegister = 0; g_params[0].Descriptor.RegisterSpace = 1;
    g_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    g_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    g_params[1].Constants.ShaderRegister = 1; g_params[1].Constants.RegisterSpace = 0; g_params[1].Constants.Num32BitValues = 4;
    g_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    g_ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; g_ranges[0].NumDescriptors = 4; g_ranges[0].BaseShaderRegister = 0;
    g_ranges[0].RegisterSpace = 0; g_ranges[0].OffsetInDescriptorsFromTableStart = 0;
    g_ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; g_ranges[1].NumDescriptors = (UINT)-1; g_ranges[1].BaseShaderRegister = 2;
    g_ranges[1].RegisterSpace = 3; g_ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    g_params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    g_params[2].DescriptorTable.NumDescriptorRanges = 2; g_params[2].DescriptorTable.pDescriptorRanges = g_ranges;
    g_params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    g_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; g_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    g_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; g_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    g_sampler.MipLODBias = 0.5f; g_sampler.MaxAnisotropy = 8; g_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    g_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; g_sampler.MinLOD = 0.0f; g_sampler.MaxLOD = 12.0f;
    g_sampler.ShaderRegister = 5; g_sampler.RegisterSpace = 2; g_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    memset(&g_desc, 0, sizeof g_desc);
    g_desc.NumParameters = 3; g_desc.pParameters = g_params; g_desc.NumStaticSamplers = 1; g_desc.pStaticSamplers = &g_sampler; g_desc.Flags = flags;
}
static void check_desc_equal(const D3D12_ROOT_SIGNATURE_DESC *d) {
    EXPECT_EQ(d->NumParameters, 3); EXPECT_EQ(d->NumStaticSamplers, 1); EXPECT_EQ(d->Flags, g_desc.Flags);
    if (d->NumParameters != 3 || d->NumStaticSamplers != 1) return;
    const D3D12_ROOT_PARAMETER *p = d->pParameters;
    EXPECT_EQ(p[0].ParameterType, D3D12_ROOT_PARAMETER_TYPE_CBV);
    EXPECT_EQ(p[0].Descriptor.ShaderRegister, 0); EXPECT_EQ(p[0].Descriptor.RegisterSpace, 1);
    EXPECT_EQ(p[0].ShaderVisibility, D3D12_SHADER_VISIBILITY_PIXEL);
    EXPECT_EQ(p[1].ParameterType, D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS);
    EXPECT_EQ(p[1].Constants.ShaderRegister, 1); EXPECT_EQ(p[1].Constants.RegisterSpace, 0); EXPECT_EQ(p[1].Constants.Num32BitValues, 4);
    EXPECT_EQ(p[1].ShaderVisibility, D3D12_SHADER_VISIBILITY_ALL);
    EXPECT_EQ(p[2].ParameterType, D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
    EXPECT_EQ(p[2].ShaderVisibility, D3D12_SHADER_VISIBILITY_VERTEX);
    EXPECT_EQ(p[2].DescriptorTable.NumDescriptorRanges, 2);
    if (p[2].DescriptorTable.NumDescriptorRanges == 2) {
        const D3D12_DESCRIPTOR_RANGE *r = p[2].DescriptorTable.pDescriptorRanges;
        EXPECT_EQ(r[0].RangeType, D3D12_DESCRIPTOR_RANGE_TYPE_SRV); EXPECT_EQ(r[0].NumDescriptors, 4);
        EXPECT_EQ(r[0].BaseShaderRegister, 0); EXPECT_EQ(r[0].RegisterSpace, 0); EXPECT_EQ(r[0].OffsetInDescriptorsFromTableStart, 0);
        EXPECT_EQ(r[1].RangeType, D3D12_DESCRIPTOR_RANGE_TYPE_UAV); EXPECT_EQ(r[1].NumDescriptors, (UINT)-1);
        EXPECT_EQ(r[1].BaseShaderRegister, 2); EXPECT_EQ(r[1].RegisterSpace, 3);
        EXPECT_EQ(r[1].OffsetInDescriptorsFromTableStart, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    }
    const D3D12_STATIC_SAMPLER_DESC *s = d->pStaticSamplers;
    EXPECT_EQ(s->Filter, D3D12_FILTER_MIN_MAG_MIP_LINEAR); EXPECT_EQ(s->AddressU, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    EXPECT_EQ(s->AddressV, D3D12_TEXTURE_ADDRESS_MODE_CLAMP); EXPECT_EQ(s->AddressW, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
    CHECK(s->MipLODBias == 0.5f); EXPECT_EQ(s->MaxAnisotropy, 8); EXPECT_EQ(s->ComparisonFunc, D3D12_COMPARISON_FUNC_ALWAYS);
    EXPECT_EQ(s->BorderColor, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);
    CHECK(s->MinLOD == 0.0f); CHECK(s->MaxLOD == 12.0f);
    EXPECT_EQ(s->ShaderRegister, 5); EXPECT_EQ(s->RegisterSpace, 2); EXPECT_EQ(s->ShaderVisibility, D3D12_SHADER_VISIBILITY_PIXEL);
}

/* ---------- serialization ---------- */
TEST(d3d12_entry_points_rootsig_serialize_empty_v10) {
    REQUIRE(U(D3D12SerializeRootSignature));
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, &e);
    EXPECT_HR(hr, S_OK); CHECK(b != NULL);
    if (!b) return;
    EP_EQ(bsize(b), 24);
    if (bsize(b) >= 24) {
        EXPECT_EQ(dw(b, 0), 1); EXPECT_EQ(dw(b, 1), 0); EXPECT_EQ(dw(b, 2), 24); EXPECT_EQ(dw(b, 3), 0); EXPECT_EQ(dw(b, 4), 24); EXPECT_EQ(dw(b, 5), 0);
    }
    rel((IUnknown *)b); rel((IUnknown *)e);
}

TEST(d3d12_entry_points_rootsig_serialize_container_layout) {
    REQUIRE(U(D3D12SerializeRootSignature));
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    const BYTE *r = rawptr(b);
    EP_EQ(rawsize(b), 68);
    if (rawsize(b) >= 44) {
        CHECK(!memcmp(r, "DXBC", 4));
        EP_EQ(rd32(r + 20), 1);              /* container version */
        EP_EQ(rd32(r + 24), rawsize(b));     /* total size */
        EP_EQ(rd32(r + 28), 1);              /* part count */
        EP_EQ(rd32(r + 32), 36);             /* offset of the only part */
        CHECK(!memcmp(r + 36, "RTS0", 4));
        EP_EQ(rd32(r + 40), 24);             /* part size */
    }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_serialize_empty_v11) {
    REQUIRE(U(D3D12SerializeRootSignature));
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_1, &b, &e);
    /* a 1.0 desc can be serialized as 1.1 (version argument selects the output format) */
    t_note("v1.0 desc serialized as 1_1 -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && b && bsize(b) >= 4) EXPECT_EQ(dw(b, 0), 2);
    rel((IUnknown *)b); rel((IUnknown *)e);
}

#define FLAGS_TEST(label, fl) \
TEST(d3d12_entry_points_rootsig_serialize_flags_##label) { \
    REQUIRE(U(D3D12SerializeRootSignature)); \
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d); d.Flags = (fl); \
    ID3DBlob *b = NULL; \
    EXPECT_HR(U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK); \
    if (b) { CHECK(bsize(b) == 24); if (bsize(b) >= 24) EXPECT_EQ(dw(b, 5), (fl)); rel((IUnknown *)b); } \
}
FLAGS_TEST(ia, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
FLAGS_TEST(deny_vs, D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS)
FLAGS_TEST(deny_ps, D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS)
FLAGS_TEST(stream_output, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT)
FLAGS_TEST(local, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE)
FLAGS_TEST(combo, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS)

TEST(d3d12_entry_points_rootsig_serialize_one_constants_param) {
    REQUIRE(U(D3D12SerializeRootSignature));
    D3D12_ROOT_PARAMETER p; memset(&p, 0, sizeof p);
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p.Constants.ShaderRegister = 3; p.Constants.RegisterSpace = 2; p.Constants.Num32BitValues = 7;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d); d.NumParameters = 1; d.pParameters = &p;
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    EP_EQ(bsize(b), 48);
    if (bsize(b) >= 48) {
        EXPECT_EQ(dw(b, 0), 1); EXPECT_EQ(dw(b, 1), 1); EXPECT_EQ(dw(b, 2), 24);
        EXPECT_EQ(dw(b, 6), D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS); EXPECT_EQ(dw(b, 7), D3D12_SHADER_VISIBILITY_GEOMETRY);
        DWORD off = dw(b, 8); EXPECT_EQ(off, 36);
        if (off + 12 <= bsize(b)) { EXPECT_EQ(dw(b, off / 4), 3); EXPECT_EQ(dw(b, off / 4 + 1), 2); EXPECT_EQ(dw(b, off / 4 + 2), 7); }
    }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_serialize_full_v10) {
    REQUIRE(U(D3D12SerializeRootSignature));
    build_desc(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ID3DBlob *b = NULL, *e = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&g_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, &e), S_OK);
    CHECK(b != NULL);
    if (b) { CHECK(bsize(b) > 24); EXPECT_EQ(dw(b, 0), 1); EXPECT_EQ(dw(b, 1), 3); EXPECT_EQ(dw(b, 3), 1); }
    rel((IUnknown *)b); rel((IUnknown *)e);
}

TEST(d3d12_entry_points_rootsig_serialize_deterministic) {
    REQUIRE(U(D3D12SerializeRootSignature));
    build_desc(0);
    ID3DBlob *a = NULL, *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&g_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &a, NULL), S_OK);
    EXPECT_HR(U(D3D12SerializeRootSignature)(&g_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (a && b) { EP_EQ(bsize(a), bsize(b)); CHECK(bsize(a) == bsize(b) && !memcmp(bptr(a), bptr(b), bsize(a))); }
    rel((IUnknown *)a); rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_serialize_blob_is_refcounted) {
    REQUIRE(U(D3D12SerializeRootSignature));
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    EP_EQ(d3dh_refcount(b), 1);
    EP_EQ(ID3D10Blob_Release(b), 0);
}

TEST(d3d12_entry_points_rootsig_serialize_invalid_version) {
    REQUIRE(U(D3D12SerializeRootSignature));
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    static const int vers[] = { 0, 3, 100, -1 };
    for (unsigned i = 0; i < 4; i++) {
        ID3DBlob *b = NULL, *e = NULL;
        HRESULT hr = U(D3D12SerializeRootSignature)(&d, (D3D_ROOT_SIGNATURE_VERSION)vers[i], &b, &e);
        CHECKF(FAILED(hr), "version %d -> %s", vers[i], d3dh_hr_name(hr));
        if (hr != E_INVALIDARG) t_note("version %d -> %s", vers[i], d3dh_hr_name(hr));
        CHECK(b == NULL);
        rel((IUnknown *)b); rel((IUnknown *)e);
    }
}

TEST(d3d12_entry_points_rootsig_serialize_null_blob_out) {
    REQUIRE(U(D3D12SerializeRootSignature));
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    HRESULT hr = U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, NULL, NULL);
    t_note("NULL ppBlob -> %s", d3dh_hr_name(hr));
    CHECK(hr != S_OK || 1);
}

TEST(d3d12_entry_points_rootsig_serialize_null_desc) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL"); return; }
    REQUIRE(U(D3D12SerializeRootSignature));
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = U(D3D12SerializeRootSignature)(NULL, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, &e);
    CHECK(FAILED(hr)); CHECK(b == NULL);
    t_note("NULL desc -> %s", d3dh_hr_name(hr));
    rel((IUnknown *)b); rel((IUnknown *)e);
}

TEST(d3d12_entry_points_rootsig_serialize_constants_too_big) {
    /* a root signature may use at most 64 DWORDs; 65 root constants must be rejected */
    REQUIRE(U(D3D12SerializeRootSignature));
    D3D12_ROOT_PARAMETER p; memset(&p, 0, sizeof p);
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p.Constants.Num32BitValues = 65;
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d); d.NumParameters = 1; d.pParameters = &p;
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, &e);
    t_note("65 root constants -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) t_note("serializer does not enforce the 64 DWORD limit; creation does");
    rel((IUnknown *)b); rel((IUnknown *)e);
}

TEST(d3d12_entry_points_rootsig_serialize_via_ordinal) {
    HMODULE m = t_module("d3d12.dll"); if (!m) { t_skip("no d3d12.dll"); return; }
    typeof(&D3D12SerializeRootSignature) f = (typeof(&D3D12SerializeRootSignature))GetProcAddress(m, (LPCSTR)116);
    if (!f) { t_skip("ordinal 116 absent"); return; }
    CHECK(f == U(D3D12SerializeRootSignature) || t_proc("D3D12SerializeRootSignature") != NULL);
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d);
    ID3DBlob *b = NULL;
    EXPECT_HR(f(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    rel((IUnknown *)b);
}

/* ---------- v1.0 deserializer ---------- */
TEST(d3d12_entry_points_rootsig_roundtrip_v10) {
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12CreateRootSignatureDeserializer));
    build_desc(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&g_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    ID3D12RootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12RootSignatureDeserializer, (void **)&ds), S_OK);
    if (ds) {
        EP_EQ(d3dh_refcount(ds), 1);
        const D3D12_ROOT_SIGNATURE_DESC *d = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(ds);
        CHECK(d != NULL);
        if (d) check_desc_equal(d);
        CHECK(ID3D12RootSignatureDeserializer_GetRootSignatureDesc(ds) == d);
        EP_EQ(ID3D12RootSignatureDeserializer_Release(ds), 0);
    }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_roundtrip_v10_buffer_copy) {
    /* the deserialized desc must not depend on the lifetime of the input buffer */
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12CreateRootSignatureDeserializer));
    build_desc(0);
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&g_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    BYTE *copy = (BYTE *)malloc(rawsize(b)); memcpy(copy, rawptr(b), rawsize(b));
    ID3D12RootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateRootSignatureDeserializer)(copy, rawsize(b), &IID_ID3D12RootSignatureDeserializer, (void **)&ds), S_OK);
    memset(copy, 0xCD, rawsize(b)); free(copy);
    if (ds) { const D3D12_ROOT_SIGNATURE_DESC *d = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(ds); if (d) check_desc_equal(d); ID3D12RootSignatureDeserializer_Release(ds); }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_deserialize_v11_blob_as_v10) {
    REQUIRE(U(D3D12SerializeVersionedRootSignature)); REQUIRE(U(D3D12CreateRootSignatureDeserializer));
    build_desc(0);
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
    D3D12_DESCRIPTOR_RANGE1 r1[1]; memset(r1, 0, sizeof r1);
    r1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; r1[0].NumDescriptors = 2; r1[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER1 p[1]; memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; p[0].DescriptorTable.NumDescriptorRanges = 1; p[0].DescriptorTable.pDescriptorRanges = r1;
    v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1.NumParameters = 1; v.Desc_1_1.pParameters = p;
    ID3DBlob *b = NULL, *e = NULL;
    EXPECT_HR(U(D3D12SerializeVersionedRootSignature)(&v, &b, &e), S_OK);
    if (!b) return;
    EXPECT_EQ(dw(b, 0), 2);
    ID3D12RootSignatureDeserializer *ds = NULL;
    HRESULT hr = U(D3D12CreateRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12RootSignatureDeserializer, (void **)&ds);
    t_note("1.0 deserializer on a 1.1 blob -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && ds) {
        const D3D12_ROOT_SIGNATURE_DESC *d = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(ds);
        CHECK(d != NULL);
        if (d) { EXPECT_EQ(d->NumParameters, 1); if (d->NumParameters == 1) { EXPECT_EQ(d->pParameters[0].ParameterType, D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE); } }
        ID3D12RootSignatureDeserializer_Release(ds);
    }
    rel((IUnknown *)b); rel((IUnknown *)e);
}

TEST(d3d12_entry_points_rootsig_deserialize_empty) {
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12CreateRootSignatureDeserializer));
    D3D12_ROOT_SIGNATURE_DESC d0; memset(&d0, 0, sizeof d0); d0.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&d0, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    ID3D12RootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12RootSignatureDeserializer, (void **)&ds), S_OK);
    if (ds) {
        const D3D12_ROOT_SIGNATURE_DESC *d = ID3D12RootSignatureDeserializer_GetRootSignatureDesc(ds);
        if (d) { EXPECT_EQ(d->NumParameters, 0); EXPECT_EQ(d->NumStaticSamplers, 0); EXPECT_EQ(d->Flags, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT); }
        ID3D12RootSignatureDeserializer_Release(ds);
    }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_deserialize_truncated) {
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12CreateRootSignatureDeserializer));
    build_desc(0);
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&g_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    SIZE_T full = rawsize(b);
    BYTE *copy = (BYTE *)malloc(full); memcpy(copy, rawptr(b), full);
    static const SIZE_T cuts[] = { 0, 4, 12, 23, 35, 43 };
    for (unsigned i = 0; i < 6; i++) {
        ID3D12RootSignatureDeserializer *ds = NULL;
        HRESULT hr = U(D3D12CreateRootSignatureDeserializer)(copy, cuts[i], &IID_ID3D12RootSignatureDeserializer, (void **)&ds);
        CHECKF(FAILED(hr), "size %u -> %s", (unsigned)cuts[i], d3dh_hr_name(hr));
        CHECK(ds == NULL); if (ds) ID3D12RootSignatureDeserializer_Release(ds);
    }
    ID3D12RootSignatureDeserializer *ds = NULL;
    HRESULT hr = U(D3D12CreateRootSignatureDeserializer)(copy, full - 1, &IID_ID3D12RootSignatureDeserializer, (void **)&ds);
    t_note("blob minus one byte -> %s", d3dh_hr_name(hr));
    if (ds) ID3D12RootSignatureDeserializer_Release(ds);
    free(copy); rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_deserialize_bad_version) {
    REQUIRE(U(D3D12CreateRootSignatureDeserializer));
    DWORD blob[6] = { 0, 0, 24, 0, 24, 0 };
    static const DWORD vers[] = { 0, 3, 0xFFFFFFFF };
    for (unsigned i = 0; i < 3; i++) {
        blob[0] = vers[i];
        ID3D12RootSignatureDeserializer *ds = NULL;
        HRESULT hr = U(D3D12CreateRootSignatureDeserializer)(blob, sizeof blob, &IID_ID3D12RootSignatureDeserializer, (void **)&ds);
        CHECKF(FAILED(hr), "version %lu -> %s", (unsigned long)vers[i], d3dh_hr_name(hr));
        if (ds) ID3D12RootSignatureDeserializer_Release(ds);
    }
}

TEST(d3d12_entry_points_rootsig_deserialize_wrong_iid) {
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12CreateRootSignatureDeserializer));
    D3D12_ROOT_SIGNATURE_DESC d0; memset(&d0, 0, sizeof d0);
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&d0, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    void *p = NULL;
    HRESULT hr = U(D3D12CreateRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12Device, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    t_note("wrong iid -> %s", d3dh_hr_name(hr));
    if (p) rel((IUnknown *)p);
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_deserializer_qi) {
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12CreateRootSignatureDeserializer));
    D3D12_ROOT_SIGNATURE_DESC d0; memset(&d0, 0, sizeof d0);
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&d0, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    ID3D12RootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12RootSignatureDeserializer, (void **)&ds), S_OK);
    if (ds) {
        IUnknown *u = NULL;
        HRESULT hq = ID3D12RootSignatureDeserializer_QueryInterface(ds, &IID_IUnknown, (void **)&u);
        if (t_is_wine() && hq == E_NOINTERFACE) t_note("emulator: IUnknown QI on deserializer -> E_NOINTERFACE");
        else { EXPECT_HR(hq, S_OK); CHECK((void *)u == (void *)ds); EP_EQ(d3dh_refcount(ds), 2); }
        rel(u);
        void *x = (void *)1;
        EXPECT_HR(ID3D12RootSignatureDeserializer_QueryInterface(ds, &IID_ID3D12Device, &x), E_NOINTERFACE);
        CHECK(x == NULL);
        ID3D12VersionedRootSignatureDeserializer *vd = NULL;
        HRESULT hr = ID3D12RootSignatureDeserializer_QueryInterface(ds, &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&vd);
        t_note("1.0 deserializer QI for versioned -> %s", d3dh_hr_name(hr)); rel((IUnknown *)vd);
        ID3D12RootSignatureDeserializer_Release(ds);
    }
    rel((IUnknown *)b);
}

/* ---------- versioned serializer / deserializer ---------- */
TEST(d3d12_entry_points_rootsig_versioned_serialize_v10_matches_plain) {
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12SerializeVersionedRootSignature));
    build_desc(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
    v.Version = D3D_ROOT_SIGNATURE_VERSION_1_0; v.Desc_1_0 = g_desc;
    ID3DBlob *a = NULL, *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&g_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &a, NULL), S_OK);
    EXPECT_HR(U(D3D12SerializeVersionedRootSignature)(&v, &b, NULL), S_OK);
    if (a && b) CHECK(bsize(a) == bsize(b) && !memcmp(bptr(a), bptr(b), bsize(a)));
    rel((IUnknown *)a); rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_versioned_serialize_empty_v11) {
    REQUIRE(U(D3D12SerializeVersionedRootSignature));
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v); v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeVersionedRootSignature)(&v, &b, NULL), S_OK);
    if (!b) return;
    EP_EQ(bsize(b), 24); if (bsize(b) >= 24) { EXPECT_EQ(dw(b, 0), 2); EXPECT_EQ(dw(b, 1), 0); EXPECT_EQ(dw(b, 3), 0); }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_versioned_serialize_invalid_version) {
    REQUIRE(U(D3D12SerializeVersionedRootSignature));
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
    static const int vers[] = { 0, 3, 7, -1 };
    for (unsigned i = 0; i < 4; i++) {
        v.Version = (D3D_ROOT_SIGNATURE_VERSION)vers[i];
        ID3DBlob *b = NULL, *e = NULL;
        HRESULT hr = U(D3D12SerializeVersionedRootSignature)(&v, &b, &e);
        CHECKF(FAILED(hr), "version %d -> %s", vers[i], d3dh_hr_name(hr));
        CHECK(b == NULL); rel((IUnknown *)b); rel((IUnknown *)e);
    }
}

TEST(d3d12_entry_points_rootsig_versioned_serialize_null_desc) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL"); return; }
    REQUIRE(U(D3D12SerializeVersionedRootSignature));
    ID3DBlob *b = NULL;
    HRESULT hr = U(D3D12SerializeVersionedRootSignature)(NULL, &b, NULL);
    CHECK(FAILED(hr)); CHECK(b == NULL); rel((IUnknown *)b);
}

static HRESULT make_v11_blob(ID3DBlob **b, D3D12_DESCRIPTOR_RANGE_FLAGS rf, D3D12_ROOT_DESCRIPTOR_FLAGS df) {
    D3D12_DESCRIPTOR_RANGE1 r[1]; memset(r, 0, sizeof r);
    r[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; r[0].NumDescriptors = 3; r[0].BaseShaderRegister = 1; r[0].RegisterSpace = 4;
    r[0].Flags = rf; r[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER1 p[2]; memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; p[0].DescriptorTable.NumDescriptorRanges = 1;
    p[0].DescriptorTable.pDescriptorRanges = r; p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[1].Descriptor.ShaderRegister = 9; p[1].Descriptor.RegisterSpace = 2; p[1].Descriptor.Flags = df;
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC v; memset(&v, 0, sizeof v);
    v.Version = D3D_ROOT_SIGNATURE_VERSION_1_1; v.Desc_1_1.NumParameters = 2; v.Desc_1_1.pParameters = p;
    v.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    return U(D3D12SerializeVersionedRootSignature)(&v, b, NULL);
}

TEST(d3d12_entry_points_rootsig_versioned_roundtrip_v11) {
    REQUIRE(U(D3D12SerializeVersionedRootSignature)); REQUIRE(U(D3D12CreateVersionedRootSignatureDeserializer));
    ID3DBlob *b = NULL;
    EXPECT_HR(make_v11_blob(&b, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC), S_OK);
    if (!b) return;
    ID3D12VersionedRootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateVersionedRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&ds), S_OK);
    if (ds) {
        EP_EQ(d3dh_refcount(ds), 1);
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(ds);
        CHECK(u != NULL);
        if (u) {
            EXPECT_EQ(u->Version, D3D_ROOT_SIGNATURE_VERSION_1_1);
            EXPECT_EQ(u->Desc_1_1.NumParameters, 2); EXPECT_EQ(u->Desc_1_1.Flags, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
            if (u->Desc_1_1.NumParameters == 2) {
                const D3D12_ROOT_PARAMETER1 *p = u->Desc_1_1.pParameters;
                EXPECT_EQ(p[0].ParameterType, D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE); EXPECT_EQ(p[0].ShaderVisibility, D3D12_SHADER_VISIBILITY_PIXEL);
                EXPECT_EQ(p[0].DescriptorTable.NumDescriptorRanges, 1);
                if (p[0].DescriptorTable.NumDescriptorRanges == 1) {
                    const D3D12_DESCRIPTOR_RANGE1 *r = p[0].DescriptorTable.pDescriptorRanges;
                    EXPECT_EQ(r->RangeType, D3D12_DESCRIPTOR_RANGE_TYPE_CBV); EXPECT_EQ(r->NumDescriptors, 3); EXPECT_EQ(r->BaseShaderRegister, 1);
                    EXPECT_EQ(r->RegisterSpace, 4); EXPECT_EQ(r->Flags, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS);
                    EXPECT_EQ(r->OffsetInDescriptorsFromTableStart, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
                }
                EXPECT_EQ(p[1].ParameterType, D3D12_ROOT_PARAMETER_TYPE_SRV); EXPECT_EQ(p[1].Descriptor.ShaderRegister, 9);
                EXPECT_EQ(p[1].Descriptor.RegisterSpace, 2); EXPECT_EQ(p[1].Descriptor.Flags, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
            }
        }
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *same = NULL;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_1, &same), S_OK);
        CHECK(same != NULL); if (same) EXPECT_EQ(same->Version, D3D_ROOT_SIGNATURE_VERSION_1_1);
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *down = NULL;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_0, &down), S_OK);
        if (down) { EXPECT_EQ(down->Version, D3D_ROOT_SIGNATURE_VERSION_1_0); EXPECT_EQ(down->Desc_1_0.NumParameters, 2); EXPECT_EQ(down->Desc_1_0.Flags, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT); }
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_versioned_upconvert_v10) {
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12CreateVersionedRootSignatureDeserializer));
    build_desc(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&g_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    ID3D12VersionedRootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateVersionedRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&ds), S_OK);
    if (ds) {
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(ds);
        if (u) { EXPECT_EQ(u->Version, D3D_ROOT_SIGNATURE_VERSION_1_0); check_desc_equal(&u->Desc_1_0); }
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *up = NULL;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, D3D_ROOT_SIGNATURE_VERSION_1_1, &up), S_OK);
        if (up) {
            EXPECT_EQ(up->Version, D3D_ROOT_SIGNATURE_VERSION_1_1);
            EXPECT_EQ(up->Desc_1_1.NumParameters, 3); EXPECT_EQ(up->Desc_1_1.NumStaticSamplers, 1);
            EXPECT_EQ(up->Desc_1_1.Flags, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
            if (up->Desc_1_1.NumParameters == 3) {
                const D3D12_ROOT_PARAMETER1 *p = up->Desc_1_1.pParameters;
                EXPECT_EQ(p[1].Constants.Num32BitValues, 4);
                EXPECT_EQ(p[2].DescriptorTable.NumDescriptorRanges, 2);
                if (p[2].DescriptorTable.NumDescriptorRanges == 2) {
                    t_note("upconverted range flags: srv=0x%x uav=0x%x", p[2].DescriptorTable.pDescriptorRanges[0].Flags, p[2].DescriptorTable.pDescriptorRanges[1].Flags);
                    EXPECT_EQ(p[2].DescriptorTable.pDescriptorRanges[1].NumDescriptors, (UINT)-1);
                }
                t_note("upconverted root CBV flags=0x%x", p[0].Descriptor.Flags);
            }
        }
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_versioned_invalid_convert_version) {
    REQUIRE(U(D3D12SerializeVersionedRootSignature)); REQUIRE(U(D3D12CreateVersionedRootSignatureDeserializer));
    ID3DBlob *b = NULL;
    EXPECT_HR(make_v11_blob(&b, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, D3D12_ROOT_DESCRIPTOR_FLAG_NONE), S_OK);
    if (!b) return;
    ID3D12VersionedRootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateVersionedRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&ds), S_OK);
    if (ds) {
        static const int vers[] = { 0, 3, 99 };
        for (unsigned i = 0; i < 3; i++) {
            const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *d = NULL;
            HRESULT hr = ID3D12VersionedRootSignatureDeserializer_GetRootSignatureDescAtVersion(ds, (D3D_ROOT_SIGNATURE_VERSION)vers[i], &d);
            CHECKF(FAILED(hr), "convert to %d -> %s", vers[i], d3dh_hr_name(hr));
        }
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_versioned_deserialize_bad_data) {
    REQUIRE(U(D3D12CreateVersionedRootSignatureDeserializer));
    DWORD blob[6] = { 0, 0, 24, 0, 24, 0 };
    ID3D12VersionedRootSignatureDeserializer *ds = NULL;
    HRESULT hr = U(D3D12CreateVersionedRootSignatureDeserializer)(blob, sizeof blob, &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&ds);
    CHECK(FAILED(hr)); CHECK(ds == NULL);
    hr = U(D3D12CreateVersionedRootSignatureDeserializer)(blob, 0, &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&ds);
    CHECK(FAILED(hr)); CHECK(ds == NULL);
    blob[0] = 9;
    hr = U(D3D12CreateVersionedRootSignatureDeserializer)(blob, sizeof blob, &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&ds);
    CHECK(FAILED(hr)); CHECK(ds == NULL);
    if (ds) ID3D12VersionedRootSignatureDeserializer_Release(ds);
}

TEST(d3d12_entry_points_rootsig_versioned_qi) {
    REQUIRE(U(D3D12SerializeVersionedRootSignature)); REQUIRE(U(D3D12CreateVersionedRootSignatureDeserializer));
    ID3DBlob *b = NULL;
    EXPECT_HR(make_v11_blob(&b, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, D3D12_ROOT_DESCRIPTOR_FLAG_NONE), S_OK);
    if (!b) return;
    ID3D12VersionedRootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateVersionedRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&ds), S_OK);
    if (ds) {
        IUnknown *u = NULL;
        HRESULT hq = ID3D12VersionedRootSignatureDeserializer_QueryInterface(ds, &IID_IUnknown, (void **)&u);
        if (t_is_wine() && hq == E_NOINTERFACE) t_note("emulator: IUnknown QI on deserializer -> E_NOINTERFACE");
        else { EXPECT_HR(hq, S_OK); CHECK((void *)u == (void *)ds); }
        rel(u);
        void *x = (void *)1;
        EXPECT_HR(ID3D12VersionedRootSignatureDeserializer_QueryInterface(ds, &IID_ID3D12Device, &x), E_NOINTERFACE);
        CHECK(x == NULL);
        EP_EQ(d3dh_refcount(ds), 1);
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    rel((IUnknown *)b);
}

TEST(d3d12_entry_points_rootsig_versioned_deserializer_on_v10_roundtrip_samplers) {
    REQUIRE(U(D3D12SerializeRootSignature)); REQUIRE(U(D3D12CreateVersionedRootSignatureDeserializer));
    D3D12_STATIC_SAMPLER_DESC s[2]; memset(s, 0, sizeof s);
    for (int i = 0; i < 2; i++) { s[i].Filter = D3D12_FILTER_ANISOTROPIC; s[i].MaxAnisotropy = 16; s[i].MaxLOD = D3D12_FLOAT32_MAX; s[i].ShaderRegister = i; s[i].RegisterSpace = 7 + i; s[i].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; s[i].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; }
    D3D12_ROOT_SIGNATURE_DESC d; memset(&d, 0, sizeof d); d.NumStaticSamplers = 2; d.pStaticSamplers = s;
    ID3DBlob *b = NULL;
    EXPECT_HR(U(D3D12SerializeRootSignature)(&d, D3D_ROOT_SIGNATURE_VERSION_1_0, &b, NULL), S_OK);
    if (!b) return;
    ID3D12VersionedRootSignatureDeserializer *ds = NULL;
    EXPECT_HR(U(D3D12CreateVersionedRootSignatureDeserializer)(rawptr(b), rawsize(b), &IID_ID3D12VersionedRootSignatureDeserializer, (void **)&ds), S_OK);
    if (ds) {
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *u = ID3D12VersionedRootSignatureDeserializer_GetUnconvertedRootSignatureDesc(ds);
        if (u && u->Desc_1_0.NumStaticSamplers == 2) {
            for (int i = 0; i < 2; i++) {
                EXPECT_EQ(u->Desc_1_0.pStaticSamplers[i].ShaderRegister, i); EXPECT_EQ(u->Desc_1_0.pStaticSamplers[i].RegisterSpace, 7 + i);
                EXPECT_EQ(u->Desc_1_0.pStaticSamplers[i].MaxAnisotropy, 16); CHECK(u->Desc_1_0.pStaticSamplers[i].MaxLOD == D3D12_FLOAT32_MAX);
            }
        } else CHECK(0);
        ID3D12VersionedRootSignatureDeserializer_Release(ds);
    }
    rel((IUnknown *)b);
}
