/* d3dcompiler_47: DXBC container access: D3DGetBlobPart, signature blobs, D3DGetDebugInfo, D3DSetBlobPart, D3DStripShader. */
#include "rb.h"

#define VS_SRC "struct VI { float4 p : POSITION; float2 uv : TEXCOORD0; float3 n : NORMAL; };\n" \
               "struct VO { float4 p : SV_Position; float2 uv : TEXCOORD0; float3 n : TEXCOORD1; };\n" \
               "cbuffer CB : register(b0) { float4x4 wvp; float4 tint; };\n" \
               "VO main(VI i) { VO o; o.p = mul(i.p, wvp); o.uv = i.uv; o.n = i.n * tint.xyz; return o; }\n"

typedef HRESULT (WINAPI *GetPart_t)(const void *, SIZE_T, D3D_BLOB_PART, UINT, ID3DBlob **);
typedef HRESULT (WINAPI *SetPart_t)(const void *, SIZE_T, D3D_BLOB_PART, UINT, const void *, SIZE_T, ID3DBlob **);
typedef HRESULT (WINAPI *Strip_t)(const void *, SIZE_T, UINT, ID3DBlob **);
typedef HRESULT (WINAPI *Sig_t)(const void *, SIZE_T, ID3DBlob **);
#define GETPART ((GetPart_t)t_proc_in("d3dcompiler_47.dll", "D3DGetBlobPart"))
#define SETPART ((SetPart_t)t_proc_in("d3dcompiler_47.dll", "D3DSetBlobPart"))
#define STRIP ((Strip_t)t_proc_in("d3dcompiler_47.dll", "D3DStripShader"))
#define GETIN ((Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputSignatureBlob"))
#define GETOUT ((Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetOutputSignatureBlob"))
#define GETIO ((Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetInputAndOutputSignatureBlob"))
#define GETDBG ((Sig_t)t_proc_in("d3dcompiler_47.dll", "D3DGetDebugInfo"))

static BYTE *rb_dup(const void *p, size_t n) { BYTE *b = malloc(n); memcpy(b, p, n); return b; }

/* ---- D3DGetBlobPart: argument validation ------------------------------------------------------------ */
TEST(rb_container_getblobpart_flags_must_be_zero)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETPART);
    ID3DBlob *o = NULL; UINT flags[] = { 1, 2, 0x10, 0x80000000u, 0xffffffffu };
    for (int i = 0; i < 5; i++) { o = NULL; HRESULT hr = GETPART(RB_BP(b), RB_BS(b), D3D_BLOB_INPUT_SIGNATURE_BLOB, flags[i], &o); t_check(hr == RB_INVALIDCALL && !o, __FILE__, __LINE__, "flags rejected", "flags 0x%x got %s", flags[i], d3dh_hr_name(hr)); SAFE_RELEASE(o); }
    ID3D10Blob_Release(b);
}
TEST(rb_container_getblobpart_flags_checked_before_data)
{
    ID3DBlob *o = NULL; REQUIRE(GETPART);
    EXPECT_HR(GETPART(NULL, 0, D3D_BLOB_INPUT_SIGNATURE_BLOB, 1, &o), RB_INVALIDCALL);
    EXPECT_HR(GETPART(NULL, 0, D3D_BLOB_INPUT_SIGNATURE_BLOB, 0, &o), RB_INVALIDCALL);
}
TEST(rb_container_getblobpart_null_out)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETPART);
    for (int part = 0; part <= 12; part++) {
        HRESULT hr = GETPART(RB_BP(b), RB_BS(b), (D3D_BLOB_PART)part, 0, NULL);
        t_check(hr == RB_INVALIDCALL, __FILE__, __LINE__, "NULL out", "part %d got %s", part, d3dh_hr_name(hr));
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_getblobpart_undefined_part_values)
{
    static const int parts[] = { 13, 14, 15, 100, 0x7fff, 0x8004, 0x8005, 0x10000, 0x7fffffff, -1, -100 };
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETPART);
    for (size_t i = 0; i < sizeof parts / sizeof *parts; i++) {
        ID3DBlob *o = NULL; HRESULT hr = GETPART(RB_BP(b), RB_BS(b), (D3D_BLOB_PART)parts[i], 0, &o);
        t_check(hr == RB_INVALIDCALL && !o, __FILE__, __LINE__, "undefined part", "part %d got %s", parts[i], d3dh_hr_name(hr));
        SAFE_RELEASE(o);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_getblobpart_container_checked_before_part)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETPART);
    BYTE *c = rb_dup(RB_BP(b), RB_BS(b)); ID3DBlob *o = NULL;
    c[4] ^= 0x55; /* checksum no longer matches */
    EXPECT_HR(GETPART(c, RB_BS(b), (D3D_BLOB_PART)999, 0, &o), RB_E_FAIL);
    c[0] = 'X';   /* structurally broken */
    EXPECT_HR(GETPART(c, RB_BS(b), (D3D_BLOB_PART)999, 0, &o), RB_INVALIDCALL);
    free(c); ID3D10Blob_Release(b);
}

/* ---- signature blobs ---------------------------------------------------------------------------------- */
TEST(rb_container_input_signature_blob_is_isgn_container)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETIN);
    rb_chunk c[RB_MAXCHUNKS], r[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS), rn; ID3DBlob *o = NULL;
    const rb_chunk *isgn = rb_find(c, n, FCC_ISGN);
    CHECK(isgn != NULL);
    EXPECT_HR(GETIN(RB_BP(b), RB_BS(b), &o), S_OK);
    if (o && isgn) {
        rn = rb_dxbc_parse(RB_BP(o), RB_BS(o), r, RB_MAXCHUNKS);
        EXPECT_EQ(rn, 1);
        if (rn == 1) { EXPECT_EQ(r[0].fourcc, FCC_ISGN); CHECK(r[0].size == isgn->size && !memcmp(r[0].data, isgn->data, isgn->size)); }
        /* the produced container carries a valid checksum of its own */
        BYTE *cp = rb_dup(RB_BP(o), RB_BS(o)); rb_dxbc_rehash(cp, RB_BS(o)); CHECK(!memcmp(cp + 4, RB_BP(o) + 4, 16)); free(cp);
    }
    SAFE_RELEASE(o); ID3D10Blob_Release(b);
}
TEST(rb_container_output_signature_blob_is_osgn_container)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETOUT);
    rb_chunk c[RB_MAXCHUNKS], r[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS); ID3DBlob *o = NULL;
    const rb_chunk *osgn = rb_find(c, n, FCC_OSGN);
    EXPECT_HR(GETOUT(RB_BP(b), RB_BS(b), &o), S_OK);
    if (o && osgn) {
        int rn = rb_dxbc_parse(RB_BP(o), RB_BS(o), r, RB_MAXCHUNKS);
        EXPECT_EQ(rn, 1);
        if (rn == 1) { EXPECT_EQ(r[0].fourcc, FCC_OSGN); CHECK(r[0].size == osgn->size && !memcmp(r[0].data, osgn->data, osgn->size)); }
    }
    SAFE_RELEASE(o); ID3D10Blob_Release(b);
}
TEST(rb_container_input_and_output_signature_blob)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETIO);
    rb_chunk c[RB_MAXCHUNKS], r[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS); ID3DBlob *o = NULL;
    EXPECT_HR(GETIO(RB_BP(b), RB_BS(b), &o), S_OK);
    if (o) {
        int rn = rb_dxbc_parse(RB_BP(o), RB_BS(o), r, RB_MAXCHUNKS);
        EXPECT_EQ(rn, 2);
        if (rn == 2) {
            EXPECT_EQ(r[0].fourcc, FCC_ISGN); EXPECT_EQ(r[1].fourcc, FCC_OSGN);
            CHECK(r[0].size == rb_find(c, n, FCC_ISGN)->size && !memcmp(r[0].data, rb_find(c, n, FCC_ISGN)->data, r[0].size));
            CHECK(r[1].size == rb_find(c, n, FCC_OSGN)->size && !memcmp(r[1].data, rb_find(c, n, FCC_OSGN)->data, r[1].size));
        }
    }
    SAFE_RELEASE(o); ID3D10Blob_Release(b);
}
TEST(rb_container_signature_helpers_match_getblobpart)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETIN); REQUIRE(GETOUT); REQUIRE(GETIO); REQUIRE(GETPART);
    Sig_t fns[3] = { GETIN, GETOUT, GETIO }; D3D_BLOB_PART parts[3] = { D3D_BLOB_INPUT_SIGNATURE_BLOB, D3D_BLOB_OUTPUT_SIGNATURE_BLOB, D3D_BLOB_INPUT_AND_OUTPUT_SIGNATURE_BLOB };
    for (int i = 0; i < 3; i++) {
        ID3DBlob *a = NULL, *p = NULL;
        EXPECT_HR(fns[i](RB_BP(b), RB_BS(b), &a), S_OK); EXPECT_HR(GETPART(RB_BP(b), RB_BS(b), parts[i], 0, &p), S_OK);
        if (a && p) CHECK(RB_BS(a) == RB_BS(p) && !memcmp(RB_BP(a), RB_BP(p), RB_BS(p)));
        SAFE_RELEASE(a); SAFE_RELEASE(p);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_signature_helpers_null_out_and_data)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETIN); REQUIRE(GETOUT); REQUIRE(GETIO);
    Sig_t fns[3] = { GETIN, GETOUT, GETIO }; ID3DBlob *o = NULL;
    for (int i = 0; i < 3; i++) {
        EXPECT_HR(fns[i](RB_BP(b), RB_BS(b), NULL), RB_INVALIDCALL);
        EXPECT_HR(fns[i](NULL, RB_BS(b), &o), RB_INVALIDCALL);
        EXPECT_HR(fns[i](RB_BP(b), 0, &o), RB_INVALIDCALL);
        EXPECT_HR(fns[i](RB_BP(b), 16, &o), RB_INVALIDCALL);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_signature_of_pixel_shader)
{
    RB_COMPILE(b, "float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { return float4(uv, p.z, 1); }", "ps_5_0"); REQUIRE(GETIO);
    ID3DBlob *o = NULL; rb_chunk r[RB_MAXCHUNKS];
    EXPECT_HR(GETIO(RB_BP(b), RB_BS(b), &o), S_OK);
    if (o) { EXPECT_EQ(rb_dxbc_parse(RB_BP(o), RB_BS(o), r, RB_MAXCHUNKS), 2); }
    SAFE_RELEASE(o); ID3D10Blob_Release(b);
}
TEST(rb_container_patch_constant_part_missing_for_vertex_shader)
{
    ID3DBlob *o = NULL;
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETPART);
    EXPECT_HR(GETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PATCH_CONSTANT_SIGNATURE_BLOB, 0, &o), RB_E_FAIL); CHECK(o == NULL);
    ID3D10Blob_Release(b);
}
TEST(rb_container_all_signature_blob_needs_patch_constants)
{
    ID3DBlob *o = NULL; HRESULT hr;
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETPART);
    hr = GETPART(RB_BP(b), RB_BS(b), D3D_BLOB_ALL_SIGNATURE_BLOB, 0, &o);
    t_note("ALL_SIGNATURE_BLOB on a vertex shader: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(o); ID3D10Blob_Release(b);
}
TEST(rb_container_all_signature_blob_for_hull_shader)
{
    static const char *hs =
        "struct VO { float4 p : POSITION; };\nstruct HC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };\n"
        "HC pcf(InputPatch<VO, 3> ip) { HC o; o.e[0] = o.e[1] = o.e[2] = 1; o.i = 1; return o; }\n"
        "[domain(\"tri\")][partitioning(\"integer\")][outputtopology(\"triangle_cw\")][outputcontrolpoints(3)][patchconstantfunc(\"pcf\")]\n"
        "VO main(InputPatch<VO, 3> ip, uint id : SV_OutputControlPointID) { return ip[id]; }\n";
    ID3DBlob *b = NULL, *o = NULL; rb_chunk r[RB_MAXCHUNKS];
    REQUIRE(GETPART);
    if (!rb_compile_t(hs, "hs_5_0", 0, &b)) return;
    EXPECT_HR(GETPART(RB_BP(b), RB_BS(b), D3D_BLOB_ALL_SIGNATURE_BLOB, 0, &o), S_OK);
    if (o) {
        int rn = rb_dxbc_parse(RB_BP(o), RB_BS(o), r, RB_MAXCHUNKS);
        EXPECT_EQ(rn, 3);
        if (rn == 3) { EXPECT_EQ(r[0].fourcc, FCC_ISGN); EXPECT_EQ(r[1].fourcc, FCC_OSGN); EXPECT_EQ(r[2].fourcc, FCC_PCSG); }
        SAFE_RELEASE(o);
    }
    EXPECT_HR(GETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PATCH_CONSTANT_SIGNATURE_BLOB, 0, &o), S_OK);
    if (o) { EXPECT_EQ(rb_dxbc_parse(RB_BP(o), RB_BS(o), r, RB_MAXCHUNKS), 1); EXPECT_EQ(r[0].fourcc, FCC_PCSG); SAFE_RELEASE(o); }
    ID3D10Blob_Release(b);
}
TEST(rb_container_signature_prefers_extended_chunk_when_present)
{
    /* a container that carries an ISG1 chunk is served that chunk for the input signature part */
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETIN); REQUIRE(GETPART);
    rb_chunk c[RB_MAXCHUNKS], r[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS); size_t sz;
    const rb_chunk *isgn = rb_find(c, n, FCC_ISGN);
    BYTE pat[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    BYTE *x = rb_dxbc_edit(RB_BP(b), RB_BS(b), 0, RB_FOURCC('I','S','G','1'), pat, sizeof pat, &sz); ID3DBlob *o = NULL;
    CHECK(isgn && x);
    if (x) {
        EXPECT_HR(GETIN(x, sz, &o), S_OK);
        if (o) {
            int rn = rb_dxbc_parse(RB_BP(o), RB_BS(o), r, RB_MAXCHUNKS);
            EXPECT_EQ(rn, 1);
            if (rn == 1) { EXPECT_EQ(r[0].fourcc, RB_FOURCC('I','S','G','1')); CHECK(r[0].size == 16 && !memcmp(r[0].data, pat, 16)); }
            SAFE_RELEASE(o);
        }
        free(x);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_signature_from_stripped_shader)
{
    /* signatures survive reflection stripping */
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(STRIP); REQUIRE(GETIO);
    ID3DBlob *s = NULL, *o = NULL;
    EXPECT_HR(STRIP(RB_BP(b), RB_BS(b), D3DCOMPILER_STRIP_REFLECTION_DATA, &s), S_OK);
    if (s) { EXPECT_HR(GETIO(RB_BP(s), RB_BS(s), &o), S_OK); SAFE_RELEASE(o); SAFE_RELEASE(s); }
    ID3D10Blob_Release(b);
}

/* ---- parts that are returned as the raw chunk payload ------------------------------------------------- */
static const struct { D3D_BLOB_PART part; DWORD fcc; const char *name; } rb_raw_parts[] = {
    { D3D_BLOB_DEBUG_INFO, FCC_SDBG, "SDBG" }, { D3D_BLOB_LEGACY_SHADER, RB_FOURCC('A','o','n','9'), "Aon9" },
    { D3D_BLOB_XNA_PREPASS_SHADER, RB_FOURCC('X','N','A','P'), "XNAP" }, { D3D_BLOB_XNA_SHADER, RB_FOURCC('X','N','A','S'), "XNAS" },
    { D3D_BLOB_PDB, FCC_SPDB, "SPDB" }, { D3D_BLOB_PRIVATE_DATA, FCC_PRIV, "PRIV" }, { D3D_BLOB_DEBUG_NAME, FCC_ILDN, "ILDN" },
    { D3D_BLOB_TEST_ALTERNATE_SHADER, RB_FOURCC('S','M','I','D'), "SMID" }, { D3D_BLOB_TEST_COMPILE_DETAILS, RB_FOURCC('S','D','T','L'), "SDTL" },
    { D3D_BLOB_TEST_COMPILE_PERF, RB_FOURCC('P','E','R','F'), "PERF" }, { D3D_BLOB_TEST_COMPILE_REPORT, RB_FOURCC('C','R','P','T'), "CRPT" },
};
TEST(rb_container_raw_parts_absent_is_efail)
{
    RB_COMPILE(b, "float4 main() : SV_Target { return 0; }", "ps_4_0"); REQUIRE(GETPART);
    for (size_t i = 0; i < sizeof rb_raw_parts / sizeof *rb_raw_parts; i++) {
        ID3DBlob *o = (ID3DBlob *)0x1; HRESULT hr;
        if (rb_raw_parts[i].part == D3D_BLOB_DEBUG_INFO) continue; /* plain compile might legitimately emit it */
        hr = GETPART(RB_BP(b), RB_BS(b), rb_raw_parts[i].part, 0, &o);
        t_check(hr == RB_E_FAIL, __FILE__, __LINE__, "absent part", "%s got %s", rb_raw_parts[i].name, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(o);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_raw_parts_returned_verbatim)
{
    RB_COMPILE(b, "float4 main() : SV_Target { return 0; }", "ps_4_0"); REQUIRE(GETPART);
    for (size_t i = 0; i < sizeof rb_raw_parts / sizeof *rb_raw_parts; i++) {
        BYTE pat[12]; size_t sz; BYTE *x; ID3DBlob *o = NULL; HRESULT hr;
        for (int k = 0; k < 12; k++) pat[k] = (BYTE)(0x40 + i * 3 + k);
        x = rb_dxbc_edit(RB_BP(b), RB_BS(b), rb_raw_parts[i].fcc, rb_raw_parts[i].fcc, pat, sizeof pat, &sz);
        if (!x) { CHECK(0); continue; }
        hr = GETPART(x, sz, rb_raw_parts[i].part, 0, &o);
        t_check(hr == S_OK && o, __FILE__, __LINE__, "raw part", "%s got %s", rb_raw_parts[i].name, d3dh_hr_name(hr));
        if (o) { t_check(RB_BS(o) == 12 && !memcmp(RB_BP(o), pat, 12), __FILE__, __LINE__, "payload", "%s size %zu", rb_raw_parts[i].name, (size_t)RB_BS(o)); ID3D10Blob_Release(o); }
        free(x);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_debug_info_without_debug_chunk)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETDBG);
    rb_chunk c[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS); ID3DBlob *o = NULL;
    if (rb_find(c, n, FCC_SDBG)) { t_note("release compile emitted SDBG"); ID3D10Blob_Release(b); return; }
    EXPECT_HR(GETDBG(RB_BP(b), RB_BS(b), &o), RB_E_FAIL); CHECK(o == NULL);
    ID3D10Blob_Release(b);
}
TEST(rb_container_debug_info_matches_sdbg_chunk)
{
    RB_COMPILE_F(b, VS_SRC, "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION); REQUIRE(GETDBG);
    rb_chunk c[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS); ID3DBlob *o = NULL;
    const rb_chunk *sd = rb_find(c, n, FCC_SDBG);
    HRESULT hr = GETDBG(RB_BP(b), RB_BS(b), &o);
    if (!sd) { t_note("D3DCOMPILE_DEBUG produced no SDBG chunk; chunks=%d, GetDebugInfo=%s", n, d3dh_hr_name(hr)); SAFE_RELEASE(o); ID3D10Blob_Release(b); return; }
    EXPECT_HR(hr, S_OK);
    if (o) { CHECK(RB_BS(o) == sd->size && !memcmp(RB_BP(o), sd->data, sd->size)); ID3D10Blob_Release(o); }
    ID3D10Blob_Release(b);
}
TEST(rb_container_debug_info_null_out)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(GETDBG);
    EXPECT_HR(GETDBG(RB_BP(b), RB_BS(b), NULL), RB_INVALIDCALL);
    ID3D10Blob_Release(b);
}
TEST(rb_container_debug_info_compiled_without_debug_flag_vs_with)
{
    RB_COMPILE(a, VS_SRC, "vs_5_0"); RB_COMPILE_F(d, VS_SRC, "vs_5_0", D3DCOMPILE_DEBUG);
    t_note("release container %zu bytes, debug container %zu bytes", (size_t)RB_BS(a), (size_t)RB_BS(d));
    CHECK(RB_BS(d) >= RB_BS(a));
    ID3D10Blob_Release(a); ID3D10Blob_Release(d);
}
TEST(rb_container_root_signature_part_is_container)
{
    /* the root signature part is returned wrapped in a container, like the signature parts */
    RB_COMPILE(b, "float4 main() : SV_Target { return 0; }", "ps_5_0"); REQUIRE(GETPART);
    DWORD rs[6] = { 1, 0, 24, 0, 24, 0 }; size_t sz; BYTE *x = rb_dxbc_edit(RB_BP(b), RB_BS(b), 0, FCC_RTS0, rs, sizeof rs, &sz); ID3DBlob *o = NULL; rb_chunk r[RB_MAXCHUNKS];
    CHECK(x != NULL);
    if (x) {
        EXPECT_HR(GETPART(x, sz, D3D_BLOB_ROOT_SIGNATURE, 0, &o), S_OK);
        if (o) {
            int rn = rb_dxbc_parse(RB_BP(o), RB_BS(o), r, RB_MAXCHUNKS);
            EXPECT_EQ(rn, 1);
            if (rn == 1) { EXPECT_EQ(r[0].fourcc, FCC_RTS0); CHECK(r[0].size == sizeof rs && !memcmp(r[0].data, rs, sizeof rs)); }
            SAFE_RELEASE(o);
        }
        free(x);
    }
    EXPECT_HR(GETPART(RB_BP(b), RB_BS(b), D3D_BLOB_ROOT_SIGNATURE, 0, &o), RB_E_FAIL);
    ID3D10Blob_Release(b);
}

/* ---- D3DSetBlobPart ------------------------------------------------------------------------------------ */
TEST(rb_container_setblobpart_private_data_roundtrip)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart"); REQUIRE(GETPART);
    static const BYTE priv[20] = "private-data-0123456";
    ID3DBlob *o = NULL, *back = NULL; HRESULT hr;
    hr = SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, priv, 20, &o);
    EXPECT_HR(hr, S_OK);
    if (o) {
        rb_chunk c[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(o), RB_BS(o), c, RB_MAXCHUNKS);
        CHECK(n > 0 && rb_find(c, n, FCC_PRIV) != NULL);
        CHECK(RB_BS(o) > RB_BS(b));
        EXPECT_EQ(*(const DWORD *)(RB_BP(o) + 0x18), RB_BS(o));
        { BYTE *cp = rb_dup(RB_BP(o), RB_BS(o)); rb_dxbc_rehash(cp, RB_BS(o)); CHECK(!memcmp(cp + 4, RB_BP(o) + 4, 16)); free(cp); }
        EXPECT_HR(GETPART(RB_BP(o), RB_BS(o), D3D_BLOB_PRIVATE_DATA, 0, &back), S_OK);
        if (back) { CHECK(RB_BS(back) >= 20 && !memcmp(RB_BP(back), priv, 20)); t_note("private data read back as %zu bytes", (size_t)RB_BS(back)); SAFE_RELEASE(back); }
        SAFE_RELEASE(o);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_setblobpart_replaces_existing_part)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart"); REQUIRE(GETPART);
    ID3DBlob *o1 = NULL, *o2 = NULL, *back = NULL;
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, "AAAAAAAA", 8, &o1), S_OK);
    if (o1) {
        EXPECT_HR(SETPART(RB_BP(o1), RB_BS(o1), D3D_BLOB_PRIVATE_DATA, 0, "BBBBBBBBBBBB", 12, &o2), S_OK);
        if (o2) {
            rb_chunk c[RB_MAXCHUNKS], c1[RB_MAXCHUNKS]; int n = rb_dxbc_parse(RB_BP(o2), RB_BS(o2), c, RB_MAXCHUNKS), n1 = rb_dxbc_parse(RB_BP(o1), RB_BS(o1), c1, RB_MAXCHUNKS), cnt = 0;
            for (int i = 0; i < n; i++) cnt += c[i].fourcc == FCC_PRIV;
            EXPECT_EQ(cnt, 1); EXPECT_EQ(n, n1);
            EXPECT_HR(GETPART(RB_BP(o2), RB_BS(o2), D3D_BLOB_PRIVATE_DATA, 0, &back), S_OK);
            if (back) { CHECK(RB_BS(back) >= 12 && !memcmp(RB_BP(back), "BBBBBBBBBBBB", 12)); SAFE_RELEASE(back); }
            SAFE_RELEASE(o2);
        }
        SAFE_RELEASE(o1);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_setblobpart_other_chunks_preserved)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart");
    ID3DBlob *o = NULL; rb_chunk c0[RB_MAXCHUNKS], c1[RB_MAXCHUNKS];
    int n0 = rb_dxbc_parse(RB_BP(b), RB_BS(b), c0, RB_MAXCHUNKS);
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, "xyz1", 4, &o), S_OK);
    if (o) {
        int n1 = rb_dxbc_parse(RB_BP(o), RB_BS(o), c1, RB_MAXCHUNKS);
        EXPECT_EQ(n1, n0 + 1);
        for (int i = 0; i < n0; i++) {
            const rb_chunk *m = rb_find(c1, n1, c0[i].fourcc);
            t_check(m && m->size == c0[i].size && !memcmp(m->data, c0[i].data, c0[i].size), __FILE__, __LINE__, "chunk preserved", "fourcc %08x", c0[i].fourcc);
        }
        SAFE_RELEASE(o);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_setblobpart_invalid_arguments)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart");
    ID3DBlob *o = (ID3DBlob *)0x1; const BYTE d[4] = { 1, 2, 3, 4 };
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 1, d, 4, &o), RB_INVALIDCALL);       /* flags */
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, NULL, 4, &o), RB_INVALIDCALL);     /* NULL part data */
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, d, 0, &o), RB_INVALIDCALL);        /* zero part size */
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, d, 0x100000000ull, &o), RB_INVALIDCALL); /* part size over 32 bits */
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, d, 4, NULL), RB_INVALIDCALL);      /* NULL out */
    EXPECT_HR(SETPART(NULL, RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, d, 4, &o), RB_INVALIDCALL);
    EXPECT_HR(SETPART(RB_BP(b), 0x10, D3D_BLOB_PRIVATE_DATA, 0, d, 4, &o), RB_INVALIDCALL);
    ID3D10Blob_Release(b);
}
TEST(rb_container_setblobpart_only_settable_parts)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart");
    static const int bad[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13, 14, 0x8000, 0x8001, 0x8003, 0x7fffffff, -1 };
    const BYTE d[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        ID3DBlob *o = NULL; HRESULT hr = SETPART(RB_BP(b), RB_BS(b), (D3D_BLOB_PART)bad[i], 0, d, 8, &o);
        t_check(hr == RB_INVALIDCALL && !o, __FILE__, __LINE__, "part cannot be set", "part %d got %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(o);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_setblobpart_debug_name_part)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart"); REQUIRE(GETPART);
    /* ILDN layout: u16 flags, u16 name length, name, NUL, padded to 4 */
    BYTE ildn[16] = { 0, 0, 7, 0, 'a', 'b', 'c', '.', 'p', 'd', 'b', 0, 0, 0, 0, 0 };
    ID3DBlob *o = NULL, *back = NULL; HRESULT hr = SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_DEBUG_NAME, 0, ildn, 16, &o);
    t_note("SetBlobPart(DEBUG_NAME) = %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && o) {
        EXPECT_HR(GETPART(RB_BP(o), RB_BS(o), D3D_BLOB_DEBUG_NAME, 0, &back), S_OK);
        if (back) { CHECK(RB_BS(back) >= 16 && !memcmp(RB_BP(back), ildn, 16)); SAFE_RELEASE(back); }
    }
    SAFE_RELEASE(o); ID3D10Blob_Release(b);
}
TEST(rb_container_setblobpart_root_signature_requires_container)
{
    RB_COMPILE(b, "float4 main() : SV_Target { return 0; }", "ps_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart"); REQUIRE(GETPART);
    DWORD rs[6] = { 1, 0, 24, 0, 24, 0 }; size_t sz; BYTE *rsc; ID3DBlob *o = NULL, *back = NULL; rb_chunk one;
    one.fourcc = FCC_RTS0; one.data = rs; one.size = sizeof rs;
    rsc = rb_dxbc_build(&one, 1, &sz);
    /* part data is itself a container holding RTS0 */
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_ROOT_SIGNATURE, 0, rsc, sz, &o), S_OK);
    if (o) {
        EXPECT_HR(GETPART(RB_BP(o), RB_BS(o), D3D_BLOB_ROOT_SIGNATURE, 0, &back), S_OK);
        if (back) { rb_chunk r[4]; EXPECT_EQ(rb_dxbc_parse(RB_BP(back), RB_BS(back), r, 4), 1); SAFE_RELEASE(back); }
        SAFE_RELEASE(o);
    }
    /* raw bytes that are not a container are rejected */
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_ROOT_SIGNATURE, 0, rs, sizeof rs, &o), RB_INVALIDCALL); CHECK(o == NULL);
    /* a container without RTS0 */
    { rb_chunk other; DWORD w = 7; size_t s2; BYTE *oc; other.fourcc = FCC_PRIV; other.data = &w; other.size = 4; oc = rb_dxbc_build(&other, 1, &s2);
      EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_ROOT_SIGNATURE, 0, oc, s2, &o), RB_E_FAIL); CHECK(o == NULL); free(oc); }
    free(rsc); ID3D10Blob_Release(b);
}
TEST(rb_container_setblobpart_result_reflects)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart");
    ID3DBlob *o = NULL; ID3D11ShaderReflection *r = NULL;
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, "data", 4, &o), S_OK);
    if (o) {
        EXPECT_HR(d3dh_D3DReflect()(RB_BP(o), RB_BS(o), &IID_ID3D11ShaderReflection, (void **)&r), S_OK);
        if (r) {
            D3D11_SHADER_DESC d; memset(&d, 0, sizeof d); EXPECT_HR(r->lpVtbl->GetDesc(r, &d), S_OK);
            EXPECT_EQ(d.ConstantBuffers, 1); r->lpVtbl->Release(r);
        }
        SAFE_RELEASE(o);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_setblobpart_input_not_modified)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(SETPART); RB_NOWINE("D3DSetBlobPart");
    BYTE *orig = rb_dup(RB_BP(b), RB_BS(b)); ID3DBlob *o = NULL;
    EXPECT_HR(SETPART(RB_BP(b), RB_BS(b), D3D_BLOB_PRIVATE_DATA, 0, "data", 4, &o), S_OK);
    CHECK(!memcmp(orig, RB_BP(b), RB_BS(b))); SAFE_RELEASE(o); free(orig); ID3D10Blob_Release(b);
}

/* ---- D3DStripShader ------------------------------------------------------------------------------------- */
static const struct { DWORD fcc; UINT flag; const char *name; } rb_strippable[] = {
    { FCC_SDBG, D3DCOMPILER_STRIP_DEBUG_INFO, "SDBG" }, { FCC_SPDB, D3DCOMPILER_STRIP_DEBUG_INFO, "SPDB" },
    { RB_FOURCC('R','P','L','Y'), D3DCOMPILER_STRIP_DEBUG_INFO, "RPLY" },
    { RB_FOURCC('S','M','I','D'), D3DCOMPILER_STRIP_TEST_BLOBS, "SMID" }, { RB_FOURCC('S','D','T','L'), D3DCOMPILER_STRIP_TEST_BLOBS, "SDTL" },
    { RB_FOURCC('P','E','R','F'), D3DCOMPILER_STRIP_TEST_BLOBS, "PERF" }, { RB_FOURCC('C','R','P','T'), D3DCOMPILER_STRIP_TEST_BLOBS, "CRPT" },
    { FCC_PRIV, D3DCOMPILER_STRIP_PRIVATE_DATA, "PRIV" }, { FCC_RTS0, D3DCOMPILER_STRIP_ROOT_SIGNATURE, "RTS0" },
    { FCC_RDEF, D3DCOMPILER_STRIP_REFLECTION_DATA, "RDEF" }, { FCC_STAT, D3DCOMPILER_STRIP_REFLECTION_DATA, "STAT" },
};
/* base shader carrying every strippable chunk */
static BYTE *rb_fat_shader(ID3DBlob *b, size_t *sz)
{
    rb_chunk c[RB_MAXCHUNKS + 16]; int n = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS); static const DWORD payload[4] = { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
    for (size_t i = 0; i < sizeof rb_strippable / sizeof *rb_strippable; i++) {
        if (rb_find(c, n, rb_strippable[i].fcc)) continue;
        c[n].fourcc = rb_strippable[i].fcc; c[n].data = payload; c[n].size = sizeof payload; n++;
    }
    return rb_dxbc_build(c, n, sz);
}
static int rb_has(const void *p, size_t sz, DWORD fcc) { rb_chunk c[RB_MAXCHUNKS + 16]; int n = rb_dxbc_parse(p, sz, c, RB_MAXCHUNKS + 16); return n >= 0 && rb_find(c, n, fcc) != NULL; }

#define STRIP_CASE(nm, flagexpr) \
TEST(rb_container_strip_##nm) \
{ \
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(STRIP); \
    size_t fsz; BYTE *fat = rb_fat_shader(b, &fsz); ID3DBlob *o = NULL; UINT fl = (flagexpr); \
    EXPECT_HR(STRIP(fat, fsz, fl, &o), S_OK); \
    if (o) { \
        for (size_t i = 0; i < sizeof rb_strippable / sizeof *rb_strippable; i++) { \
            int want = !(rb_strippable[i].flag & fl); \
            t_check(rb_has(RB_BP(o), RB_BS(o), rb_strippable[i].fcc) == want, __FILE__, __LINE__, "chunk presence after strip", "flags 0x%x chunk %s want %s", fl, rb_strippable[i].name, want ? "kept" : "removed"); \
        } \
        CHECK(rb_has(RB_BP(o), RB_BS(o), FCC_ISGN)); CHECK(rb_has(RB_BP(o), RB_BS(o), FCC_OSGN)); CHECK(rb_has(RB_BP(o), RB_BS(o), FCC_SHEX) || rb_has(RB_BP(o), RB_BS(o), FCC_SHDR)); \
        EXPECT_EQ(*(const DWORD *)(RB_BP(o) + 0x18), RB_BS(o)); \
        { BYTE *cp = rb_dup(RB_BP(o), RB_BS(o)); rb_dxbc_rehash(cp, RB_BS(o)); CHECK(!memcmp(cp + 4, RB_BP(o) + 4, 16)); free(cp); } \
        ID3D10Blob_Release(o); \
    } \
    free(fat); ID3D10Blob_Release(b); \
}
STRIP_CASE(no_flags_keeps_everything, 0)
STRIP_CASE(reflection_data, D3DCOMPILER_STRIP_REFLECTION_DATA)
STRIP_CASE(debug_info, D3DCOMPILER_STRIP_DEBUG_INFO)
STRIP_CASE(test_blobs, D3DCOMPILER_STRIP_TEST_BLOBS)
STRIP_CASE(private_data, D3DCOMPILER_STRIP_PRIVATE_DATA)
STRIP_CASE(root_signature, D3DCOMPILER_STRIP_ROOT_SIGNATURE)
STRIP_CASE(reflection_and_debug, D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO)
STRIP_CASE(all_flags, 0x1f)
STRIP_CASE(undefined_bits_ignored, 0xffffffe0u)
STRIP_CASE(undefined_bits_with_debug, 0x80000000u | D3DCOMPILER_STRIP_DEBUG_INFO)

TEST(rb_container_strip_null_out)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(STRIP);
    EXPECT_HR(STRIP(RB_BP(b), RB_BS(b), 0, NULL), RB_E_FAIL);
    EXPECT_HR(STRIP(NULL, 0, 0, NULL), RB_E_FAIL); /* the output pointer is checked before the data */
    ID3D10Blob_Release(b);
}
TEST(rb_container_strip_invalid_data)
{
    ID3DBlob *o = NULL; REQUIRE(STRIP);
    EXPECT_HR(STRIP(NULL, 0, 0, &o), RB_INVALIDCALL);
    EXPECT_HR(STRIP(NULL, 100, 0, &o), RB_INVALIDCALL);
    { BYTE junk[64] = { 'D', 'X', 'B', 'X' }; EXPECT_HR(STRIP(junk, sizeof junk, 0, &o), RB_INVALIDCALL); }
    CHECK(o == NULL);
}
TEST(rb_container_strip_reflection_removes_rdef_from_real_shader)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(STRIP);
    ID3DBlob *o = NULL; ID3D11ShaderReflection *r = NULL; D3D11_SHADER_DESC d, d0;
    EXPECT_HR(STRIP(RB_BP(b), RB_BS(b), D3DCOMPILER_STRIP_REFLECTION_DATA, &o), S_OK);
    if (o) {
        CHECK(RB_BS(o) < RB_BS(b)); CHECK(!rb_has(RB_BP(o), RB_BS(o), FCC_RDEF));
        EXPECT_HR(d3dh_D3DReflect()(RB_BP(o), RB_BS(o), &IID_ID3D11ShaderReflection, (void **)&r), S_OK);
        if (r) {
            ID3D11ShaderReflection *r0 = rb_reflect(b);
            memset(&d, 0, sizeof d); memset(&d0, 0, sizeof d0);
            EXPECT_HR(r->lpVtbl->GetDesc(r, &d), S_OK);
            if (r0) {
                EXPECT_HR(r0->lpVtbl->GetDesc(r0, &d0), S_OK);
                EXPECT_EQ(d.InputParameters, d0.InputParameters); EXPECT_EQ(d.OutputParameters, d0.OutputParameters);
                EXPECT_EQ(d.Version, d0.Version);
                CHECK(d0.ConstantBuffers == 1 && d0.BoundResources == 1);
                r0->lpVtbl->Release(r0);
            }
            EXPECT_EQ(d.ConstantBuffers, 0); EXPECT_EQ(d.BoundResources, 0);
            r->lpVtbl->Release(r);
        }
        ID3D10Blob_Release(o);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_strip_is_idempotent)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(STRIP);
    ID3DBlob *o1 = NULL, *o2 = NULL;
    EXPECT_HR(STRIP(RB_BP(b), RB_BS(b), 0x1f, &o1), S_OK);
    if (o1) {
        EXPECT_HR(STRIP(RB_BP(o1), RB_BS(o1), 0x1f, &o2), S_OK);
        if (o2) { CHECK(RB_BS(o1) == RB_BS(o2) && !memcmp(RB_BP(o1), RB_BP(o2), RB_BS(o1))); ID3D10Blob_Release(o2); }
        ID3D10Blob_Release(o1);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_strip_zero_flags_roundtrips_compiled_shader)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(STRIP);
    ID3DBlob *o = NULL;
    EXPECT_HR(STRIP(RB_BP(b), RB_BS(b), 0, &o), S_OK);
    if (o) {
        t_note("strip(0) size %zu vs original %zu", (size_t)RB_BS(o), (size_t)RB_BS(b));
        CHECK(RB_BS(o) <= RB_BS(b));
        ID3D10Blob_Release(o);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_container_strip_input_unmodified_and_output_independent)
{
    RB_COMPILE(b, VS_SRC, "vs_5_0"); REQUIRE(STRIP);
    BYTE *orig = rb_dup(RB_BP(b), RB_BS(b)); ID3DBlob *o = NULL;
    EXPECT_HR(STRIP(RB_BP(b), RB_BS(b), 0x1f, &o), S_OK);
    CHECK(!memcmp(orig, RB_BP(b), RB_BS(b)));
    if (o) CHECK(RB_BP(o) != RB_BP(b));
    SAFE_RELEASE(o); free(orig); ID3D10Blob_Release(b);
}
TEST(rb_container_strip_debug_compiled_shader_shrinks)
{
    RB_COMPILE_F(b, VS_SRC, "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION); REQUIRE(STRIP);
    ID3DBlob *o = NULL;
    EXPECT_HR(STRIP(RB_BP(b), RB_BS(b), D3DCOMPILER_STRIP_DEBUG_INFO, &o), S_OK);
    if (o) { CHECK(!rb_has(RB_BP(o), RB_BS(o), FCC_SDBG)); CHECK(RB_BS(o) <= RB_BS(b)); if (rb_has(RB_BP(b), RB_BS(b), FCC_SDBG)) CHECK(RB_BS(o) < RB_BS(b)); ID3D10Blob_Release(o); }
    ID3D10Blob_Release(b);
}
