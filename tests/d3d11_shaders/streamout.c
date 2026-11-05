/* CreateGeometryShaderWithStreamOutput: declarations, strides, rasterized stream, source bytecode kinds. */
#include "sh_common.h"

typedef D3D11_SO_DECLARATION_ENTRY SOE;
enum { M_OK = 1, M_FAIL = 0, M_NOTE = 2 };

static HRESULT so_run(const char *shname, const char *target, const SOE *d, UINT n, const UINT *st, UINT ns, UINT rast, int mode, UINT minfl, const char *what)
{
    SHENV e; if (!sh_env(&e, minfl)) return E_FAIL;
    const sh_ent *t = sh_find(shname);
    ID3DBlob *b = sh_blob_target(shname, target ? target : t->target);
    if (!b) { sh_env_free(&e); return E_FAIL; }
    ID3D11GeometryShader *gs = (ID3D11GeometryShader *)(ULONG_PTR)0;
    HRESULT hr = ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), d, n, st, ns, rast, NULL, &gs);
    if (mode == M_OK) { t_check(hr == S_OK && gs, __FILE__, __LINE__, "SO shader created", "%s: %s", what, d3dh_hr_name(hr)); }
    else if (mode == M_FAIL) { t_check(FAILED(hr) && !gs, __FILE__, __LINE__, "SO shader rejected", "%s: %s", what, d3dh_hr_name(hr)); t_note("%s -> %s", what, d3dh_hr_name(hr)); }
    else { t_note("%s -> %s", what, d3dh_hr_name(hr)); }
    /* NULL ppGeometryShader must agree with the real path */
    HRESULT hv = ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), d, n, st, ns, rast, NULL, NULL);
    if (mode == M_OK) t_check(hv == S_FALSE, __FILE__, __LINE__, "NULL out validates", "%s: %s", what, d3dh_hr_name(hv));
    else if (mode == M_FAIL) t_check(FAILED(hv), __FILE__, __LINE__, "NULL out validation fails", "%s: %s", what, d3dh_hr_name(hv));
    SH_REL(gs); sh_env_free(&e);
    return hr;
}

#define NRS D3D11_SO_NO_RASTERIZED_STREAM
/* SO_CASE(name, mode, shader, rasterized, nstrides, s0, s1, s2, s3, entries...) */
#define SO_CASE(nm, mode, sh, rast, ns, s0, s1, s2, s3, ...) \
TEST(d3d11_shaders_so_##nm) { \
    static const SOE d[] = { __VA_ARGS__ }; const UINT st[4] = { s0, s1, s2, s3 }; \
    so_run(sh, NULL, d, sizeof d / sizeof *d, (ns) ? st : NULL, ns, rast, mode, 0xa000, #nm); }

SO_CASE(position_only, M_OK, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(position_rasterized_stream0, M_OK, "gs_so_src", 0, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(all_semantics_one_slot, M_OK, "gs_so_src", NRS, 1, 52, 0, 0, 0,
        { 0, "SV_Position", 0, 0, 4, 0 }, { 0, "NORMAL", 0, 0, 3, 0 }, { 0, "TEXCOORD", 0, 0, 2, 0 }, { 0, "COLOR", 0, 0, 4, 0 })
SO_CASE(all_semantics_four_slots, M_OK, "gs_so_src", NRS, 4, 16, 12, 8, 16,
        { 0, "SV_Position", 0, 0, 4, 0 }, { 0, "NORMAL", 0, 0, 3, 1 }, { 0, "TEXCOORD", 0, 0, 2, 2 }, { 0, "COLOR", 0, 0, 4, 3 })
SO_CASE(partial_components, M_OK, "gs_so_src", NRS, 1, 8, 0, 0, 0, { 0, "NORMAL", 0, 1, 2, 0 })
SO_CASE(single_component, M_OK, "gs_so_src", NRS, 1, 4, 0, 0, 0, { 0, "COLOR", 0, 3, 1, 0 })
SO_CASE(upper_components, M_OK, "gs_so_src", NRS, 1, 8, 0, 0, 0, { 0, "SV_Position", 0, 2, 2, 0 })
SO_CASE(split_one_semantic_across_slots, M_OK, "gs_so_src", NRS, 2, 8, 8, 0, 0, { 0, "SV_Position", 0, 0, 2, 0 }, { 0, "SV_Position", 0, 2, 2, 1 })
SO_CASE(gap_entry, M_OK, "gs_so_src", NRS, 1, 28, 0, 0, 0, { 0, NULL, 0, 0, 3, 0 }, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(gap_entry_trailing, M_OK, "gs_so_src", NRS, 1, 32, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 }, { 0, NULL, 0, 0, 4, 0 })
SO_CASE(gap_entry_over_four_components, M_OK, "gs_so_src", NRS, 1, 48, 0, 0, 0, { 0, NULL, 0, 0, 8, 0 }, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stride_with_padding, M_OK, "gs_so_src", NRS, 1, 64, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stride_max_2048, M_OK, "gs_so_src", NRS, 1, 2048, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stride_over_max, M_FAIL, "gs_so_src", NRS, 1, 2052, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stride_too_small, M_FAIL, "gs_so_src", NRS, 1, 8, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stride_zero_for_used_slot, M_FAIL, "gs_so_src", NRS, 1, 0, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stride_one_short_of_data, M_FAIL, "gs_so_src", NRS, 1, 12, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stride_not_multiple_of_four, M_NOTE, "gs_so_src", NRS, 1, 18, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(too_many_strides, M_FAIL, "gs_so_src", NRS, 5, 16, 16, 16, 16, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(strides_without_entries_for_slot, M_NOTE, "gs_so_src", NRS, 2, 16, 16, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(slot_without_stride, M_NOTE, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 }, { 0, "COLOR", 0, 0, 4, 1 })
SO_CASE(output_slot_4, M_FAIL, "gs_so_src", NRS, 4, 16, 16, 16, 16, { 0, "SV_Position", 0, 0, 4, 4 })
SO_CASE(output_slot_large, M_FAIL, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0xffffffff })
SO_CASE(component_count_zero, M_FAIL, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 0, 0 })
SO_CASE(component_count_five, M_FAIL, "gs_so_src", NRS, 1, 20, 0, 0, 0, { 0, "SV_Position", 0, 0, 5, 0 })
SO_CASE(start_plus_count_overflow, M_FAIL, "gs_so_src", NRS, 1, 8, 0, 0, 0, { 0, "SV_Position", 0, 3, 2, 0 })
SO_CASE(start_component_4, M_FAIL, "gs_so_src", NRS, 1, 4, 0, 0, 0, { 0, "SV_Position", 0, 4, 1, 0 })
SO_CASE(unknown_semantic, M_FAIL, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 0, "NOSUCHSEMANTIC", 0, 0, 4, 0 })
SO_CASE(wrong_semantic_index, M_FAIL, "gs_so_src", NRS, 1, 8, 0, 0, 0, { 0, "TEXCOORD", 1, 0, 2, 0 })
SO_CASE(empty_semantic_name, M_FAIL, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 0, "", 0, 0, 4, 0 })
SO_CASE(lowercase_semantic_name, M_NOTE, "gs_so_src", NRS, 1, 8, 0, 0, 0, { 0, "texcoord", 0, 0, 2, 0 })
SO_CASE(semantic_with_digit_suffix, M_NOTE, "gs_so_src", NRS, 1, 8, 0, 0, 0, { 0, "TEXCOORD0", 0, 0, 2, 0 })
SO_CASE(components_beyond_signature_mask, M_NOTE, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 0, "NORMAL", 0, 0, 4, 0 })
SO_CASE(duplicate_entry, M_NOTE, "gs_so_src", NRS, 1, 32, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 }, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stream_1_in_gs_4_0, M_FAIL, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 1, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stream_4, M_FAIL, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 4, "SV_Position", 0, 0, 4, 0 })
SO_CASE(stream_huge, M_FAIL, "gs_so_src", NRS, 1, 16, 0, 0, 0, { 0xffffffff, "SV_Position", 0, 0, 4, 0 })
SO_CASE(rasterized_stream_4, M_FAIL, "gs_so_src", 4, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(rasterized_stream_0xfffffffe, M_FAIL, "gs_so_src", 0xfffffffe, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(rasterized_stream_1_in_gs_4_0, M_NOTE, "gs_so_src", 1, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(from_vertex_shader_bytecode, M_OK, "vs_so_src", NRS, 1, 52, 0, 0, 0,
        { 0, "SV_Position", 0, 0, 4, 0 }, { 0, "NORMAL", 0, 0, 3, 0 }, { 0, "TEXCOORD", 0, 0, 2, 0 }, { 0, "COLOR", 0, 0, 4, 0 })
SO_CASE(from_vertex_shader_rasterized_0, M_OK, "vs_so_src", 0, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(from_geometry_shader_5_0, M_OK, "gs_tri_50", NRS, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(from_domain_shader_bytecode, M_OK, "ds_tri", NRS, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(from_pixel_shader_bytecode, M_FAIL, "ps_const", NRS, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(from_compute_shader_bytecode, M_FAIL, "cs_basic", NRS, 1, 16, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 })
SO_CASE(from_hull_shader_bytecode, M_NOTE, "hs_tri_int", NRS, 1, 12, 0, 0, 0, { 0, "POSITION", 0, 0, 3, 0 })
SO_CASE(multi_stream_two_streams, M_OK, "gs_stream", 0, 2, 16, 16, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 }, { 1, "COLOR", 0, 0, 4, 1 })
SO_CASE(multi_stream_no_raster, M_OK, "gs_stream", NRS, 2, 16, 16, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 }, { 1, "COLOR", 0, 0, 4, 1 })
SO_CASE(multi_stream_raster_stream_1, M_NOTE, "gs_stream", 1, 2, 16, 16, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 }, { 1, "COLOR", 0, 0, 4, 1 })
SO_CASE(multi_stream_same_slot, M_NOTE, "gs_stream", NRS, 1, 32, 0, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 }, { 1, "COLOR", 0, 0, 4, 0 })
SO_CASE(multi_stream_semantic_in_wrong_stream, M_FAIL, "gs_stream", NRS, 2, 16, 16, 0, 0, { 1, "SV_Position", 0, 0, 4, 0 }, { 0, "COLOR", 0, 0, 4, 1 })
SO_CASE(multi_stream_stream_2_missing, M_FAIL, "gs_stream", NRS, 2, 16, 16, 0, 0, { 0, "SV_Position", 0, 0, 4, 0 }, { 2, "COLOR", 0, 0, 4, 1 })

/* Zero entries: pure geometry shader with no stream output. */
TEST(d3d11_shaders_so_zero_entries_null_everything)
{
    so_run("gs_so_src", NULL, NULL, 0, NULL, 0, NRS, M_OK, 0xa000, "0 entries, NULL decl, NULL strides");
}
TEST(d3d11_shaders_so_zero_entries_rasterized_0)
{
    so_run("gs_so_src", NULL, NULL, 0, NULL, 0, 0, M_OK, 0xa000, "0 entries rasterized stream 0");
}
TEST(d3d11_shaders_so_zero_entries_with_strides)
{
    UINT s[1] = { 16 };
    so_run("gs_so_src", NULL, NULL, 0, s, 1, NRS, M_NOTE, 0xa000, "0 entries with 1 stride");
}
TEST(d3d11_shaders_so_null_decl_with_entries)
{
    UINT s[1] = { 16 };
    so_run("gs_so_src", NULL, NULL, 1, s, 1, NRS, M_FAIL, 0xa000, "NULL decl with 1 entry");
}
TEST(d3d11_shaders_so_entry_count_over_limit)
{
    enum { N = D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT + 1 };
    SOE *d = calloc(N, sizeof *d);
    for (int i = 0; i < N; i++) { d[i].SemanticName = "SV_Position"; d[i].ComponentCount = 4; }
    UINT s[1] = { 16 };
    so_run("gs_so_src", NULL, d, N, s, 1, NRS, M_FAIL, 0xa000, "513 entries");
    free(d);
}
TEST(d3d11_shaders_so_many_gap_entries)
{
    SOE d[64]; UINT s[1] = { 64 * 4 };
    for (int i = 0; i < 64; i++) { d[i].Stream = 0; d[i].SemanticName = NULL; d[i].SemanticIndex = 0; d[i].StartComponent = 0; d[i].ComponentCount = 1; d[i].OutputSlot = 0; }
    so_run("gs_so_src", NULL, d, 64, s, 1, NRS, M_NOTE, 0xa000, "64 one-component gap entries");
}
TEST(d3d11_shaders_so_null_and_empty_bytecode)
{
    SH_ENV(e, 0);
    SOE d = { 0, "SV_Position", 0, 0, 4, 0 }; UINT s = 16; ID3D11GeometryShader *gs = NULL;
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, NULL, 0, &d, 1, &s, 1, NRS, NULL, &gs), E_INVALIDARG);
    CHECK(gs == NULL);
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, NULL, 100, &d, 1, &s, 1, NRS, NULL, &gs), E_INVALIDARG);
    SH_BLOB(b, "gs_so_src");
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), 0, &d, 1, &s, 1, NRS, NULL, &gs), E_INVALIDARG);
    CHECK(gs == NULL);
    sh_env_free(&e);
}
TEST(d3d11_shaders_so_truncated_bytecode)
{
    SH_ENV(e, 0); SH_BLOB(b, "gs_so_src");
    SOE d = { 0, "SV_Position", 0, 0, 4, 0 }; UINT s = 16;
    SIZE_T n = SH_BN(b), lens[] = { 4, 31, n / 2, n - 1 };
    for (int i = 0; i < 4; i++) {
        ID3D11GeometryShader *gs = NULL;
        HRESULT hr = ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), lens[i], &d, 1, &s, 1, NRS, NULL, &gs);
        t_check(hr == E_INVALIDARG && !gs, __FILE__, __LINE__, "truncated SO bytecode", "len %llu: %s", (unsigned long long)lens[i], d3dh_hr_name(hr)); SH_REL(gs);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_so_bad_magic)
{
    SH_ENV(e, 0); SH_BLOB(b, "gs_so_src");
    SOE d = { 0, "SV_Position", 0, 0, 4, 0 }; UINT s = 16; ID3D11GeometryShader *gs = NULL;
    unsigned char *p = sh_dup(b, 0); memcpy(p, "DXBD", 4);
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, p, SH_BN(b), &d, 1, &s, 1, NRS, NULL, &gs), E_INVALIDARG);
    free(p); sh_env_free(&e);
}
TEST(d3d11_shaders_so_decl_not_modified_and_copied)
{
    SH_ENV(e, 0); SH_BLOB(b, "gs_so_src");
    SOE d[2] = { { 0, "SV_Position", 0, 0, 4, 0 }, { 0, "COLOR", 0, 0, 4, 0 } }; UINT s = 32; ID3D11GeometryShader *g1 = NULL, *g2 = NULL;
    char *name = _strdup("SV_Position"); d[0].SemanticName = name;
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), d, 2, &s, 1, NRS, NULL, &g1), S_OK);
    memset(name, 'x', strlen(name)); /* declaration strings are not retained */
    memset(d, 0, sizeof d);
    EXPECT_EQ(d3dh_refcount(g1), 1);
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), d, 0, NULL, 0, NRS, NULL, &g2), S_OK);
    free(name); SH_REL(g1); SH_REL(g2); sh_env_free(&e);
}
TEST(d3d11_shaders_so_object_is_geometry_shader)
{
    SH_ENV(e, 0); SH_BLOB(b, "gs_so_src");
    SOE d = { 0, "SV_Position", 0, 0, 4, 0 }; UINT s = 16; ID3D11GeometryShader *gs = NULL; void *q = NULL;
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), &d, 1, &s, 1, NRS, NULL, &gs), S_OK);
    if (gs) {
        EXPECT_HR(IUnknown_QueryInterface((IUnknown *)gs, &IID_ID3D11GeometryShader, &q), S_OK); SH_REL(q);
        EXPECT_HR(IUnknown_QueryInterface((IUnknown *)gs, &IID_ID3D11VertexShader, &q), E_NOINTERFACE);
        ID3D11DeviceContext_GSSetShader(e.ctx, gs, NULL, 0);
        ID3D11GeometryShader *g = NULL; ID3D11DeviceContext_GSGetShader(e.ctx, &g, NULL, NULL); CHECK(g == gs); SH_REL(g);
        ID3D11DeviceContext_GSSetShader(e.ctx, NULL, NULL, 0);
    }
    SH_REL(gs); sh_env_free(&e);
}
TEST(d3d11_shaders_so_with_class_linkage_param)
{
    SH_ENV(e, 0); SH_BLOB(b, "gs_so_src");
    SOE d = { 0, "SV_Position", 0, 0, 4, 0 }; UINT s = 16; ID3D11GeometryShader *gs = NULL; ID3D11ClassLinkage *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), &d, 1, &s, 1, NRS, cl, &gs), S_OK);
    SH_REL(gs); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_so_blob_from_getblobpart_output_signature)
{
    REQUIRE(d3dh_D3DGetBlobPart());
    SH_ENV(e, 0); SH_BLOB(b, "vs_so_src");
    ID3DBlob *sig = NULL;
    HRESULT hr = d3dh_D3DGetBlobPart()(SH_BP(b), SH_BN(b), D3D_BLOB_OUTPUT_SIGNATURE_BLOB, 0, &sig);
    if (FAILED(hr)) { t_skip("D3DGetBlobPart: %s", d3dh_hr_name(hr)); return; }
    SOE d = { 0, "SV_Position", 0, 0, 4, 0 }; UINT s = 16; ID3D11GeometryShader *gs = NULL;
    hr = ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(sig), SH_BN(sig), &d, 1, &s, 1, NRS, NULL, &gs);
    t_note("signature-only blob (D3D_BLOB_OUTPUT_SIGNATURE_BLOB part) accepted as SO source: %s", d3dh_hr_name(hr));
    SH_REL(gs); ID3D10Blob_Release(sig); sh_env_free(&e);
}

/* End to end: stream a vertex shader's outputs into a buffer and read them back (WARP-deterministic). */
TEST(d3d11_shaders_so_functional_vertex_shader_capture)
{
    SH_ENV(e, 0); SH_BLOB(vb, "vs_so_src");
    SOE d[4] = { { 0, "SV_Position", 0, 0, 4, 0 }, { 0, "NORMAL", 0, 0, 3, 0 }, { 0, "TEXCOORD", 0, 0, 2, 0 }, { 0, "COLOR", 0, 0, 4, 0 } };
    UINT stride = 52; ID3D11GeometryShader *so = NULL; ID3D11VertexShader *vs = NULL;
    if (FAILED(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(vb), SH_BN(vb), d, 4, &stride, 1, NRS, NULL, &so))) { CHECK(0); return; }
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &vs), S_OK);
    D3D11_INPUT_ELEMENT_DESC el = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    ID3D11InputLayout *il = NULL;
    EXPECT_HR(ID3D11Device_CreateInputLayout(e.dev, &el, 1, SH_BP(vb), SH_BN(vb), &il), S_OK);
    float verts[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    D3D11_BUFFER_DESC bd = { sizeof verts, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA sd = { verts, 0, 0 };
    ID3D11Buffer *vbuf = NULL, *sob = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &bd, &sd, &vbuf), S_OK);
    D3D11_BUFFER_DESC od = { 3 * 52 + 12, D3D11_USAGE_DEFAULT, D3D11_BIND_STREAM_OUTPUT, 0, 0, 0 };
    EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &od, NULL, &sob), S_OK);
    if (vs && il && vbuf && sob) {
        UINT vstride = 16, voff = 0, soff = 0;
        ID3D11DeviceContext_IASetInputLayout(e.ctx, il);
        ID3D11DeviceContext_IASetVertexBuffers(e.ctx, 0, 1, &vbuf, &vstride, &voff);
        ID3D11DeviceContext_IASetPrimitiveTopology(e.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0);
        ID3D11DeviceContext_GSSetShader(e.ctx, so, NULL, 0);
        ID3D11DeviceContext_SOSetTargets(e.ctx, 1, &sob, &soff);
        ID3D11DeviceContext_Draw(e.ctx, 3, 0);
        ID3D11Buffer *none = NULL; ID3D11DeviceContext_SOSetTargets(e.ctx, 1, &none, &soff);
        void *data = NULL; size_t sz = 0;
        if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, sob, &data, &sz))) {
            const float *f = data;
            for (int v = 0; v < 3; v++) {
                const float *o = f + v * 13, *in = verts + v * 4;
                CHECKF(o[0] == in[0] && o[1] == in[1] && o[2] == in[2] && o[3] == in[3], "vertex %d position", v);
                CHECKF(o[4] == in[0] && o[5] == in[1] && o[6] == in[2], "vertex %d normal", v);
                CHECKF(o[7] == in[0] && o[8] == in[1], "vertex %d uv", v);
                CHECKF(o[9] == in[0] && o[10] == in[1] && o[11] == in[2] && o[12] == in[3], "vertex %d color", v);
            }
            free(data);
        } else CHECK(0);
    }
    ID3D11DeviceContext_GSSetShader(e.ctx, NULL, NULL, 0);
    SH_REL(vbuf); SH_REL(sob); SH_REL(il); SH_REL(vs); SH_REL(so); sh_env_free(&e);
}
