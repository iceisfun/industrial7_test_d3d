/* d3d10.dll / d3d10_1.dll: shader profiles, state-block mask algebra, DisassembleShader, ReflectShader, CompileEffectFromMemory. */
#include "gap1.h"
#include <d3d10shader.h>
#include <d3d10effect.h>
#include <d3d11shader.h>

#define BREL(b) do { if (b) { ID3D10Blob_Release(b); (b) = NULL; } } while (0)
#define BP(b)  ((const unsigned char *)ID3D10Blob_GetBufferPointer(b))
#define BS(b)  ((SIZE_T)ID3D10Blob_GetBufferSize(b))

/* ---------------- shader profile strings ---------------- */
TEST(gap1_profile_pixel_d3d10) {
    REQUIRE(UIN("d3d10.dll", D3D10GetPixelShaderProfile));
    const char *s = UIN("d3d10.dll", D3D10GetPixelShaderProfile)(NULL);
    CHECK(s != NULL);
    if (s) EXPECT_STR(s, "ps_4_0");
}
TEST(gap1_profile_vertex_d3d10) {
    REQUIRE(UIN("d3d10.dll", D3D10GetVertexShaderProfile));
    const char *s = UIN("d3d10.dll", D3D10GetVertexShaderProfile)(NULL);
    CHECK(s != NULL);
    if (s) EXPECT_STR(s, "vs_4_0");
}
TEST(gap1_profile_geometry_d3d10) {
    REQUIRE(UIN("d3d10.dll", D3D10GetGeometryShaderProfile));
    const char *s = UIN("d3d10.dll", D3D10GetGeometryShaderProfile)(NULL);
    CHECK(s != NULL);
    if (s) EXPECT_STR(s, "gs_4_0");
}
TEST(gap1_profile_pointers_are_stable_and_static) {
    REQUIRE(UIN("d3d10.dll", D3D10GetPixelShaderProfile)); REQUIRE(UIN("d3d10.dll", D3D10GetVertexShaderProfile)); REQUIRE(UIN("d3d10.dll", D3D10GetGeometryShaderProfile));
    const char *a = UIN("d3d10.dll", D3D10GetPixelShaderProfile)(NULL), *b = UIN("d3d10.dll", D3D10GetPixelShaderProfile)(NULL);
    CHECK(a == b);                                           /* a constant string, no per-call allocation */
    const char *v = UIN("d3d10.dll", D3D10GetVertexShaderProfile)(NULL), *g = UIN("d3d10.dll", D3D10GetGeometryShaderProfile)(NULL);
    CHECK(a != v && v != g && a != g);
    if (a && v && g) { CHECK(a[0] == 'p' && v[0] == 'v' && g[0] == 'g'); CHECK(strlen(a) == 6 && strlen(v) == 6 && strlen(g) == 6); }
}
TEST(gap1_profile_d3d10_ignores_device_argument) {
    REQUIRE(UIN("d3d10.dll", D3D10GetPixelShaderProfile));
    ID3D10Device *dev = NULL; D3D10_DRIVER_TYPE dt;
    if (FAILED(d3dh_make_device10(&dev, &dt, 0)) || !dev) { t_skip("no D3D10 device"); return; }
    EXPECT_STR(UIN("d3d10.dll", D3D10GetPixelShaderProfile)(dev), "ps_4_0");
    EXPECT_STR(UIN("d3d10.dll", D3D10GetVertexShaderProfile)(dev), "vs_4_0");
    EXPECT_STR(UIN("d3d10.dll", D3D10GetGeometryShaderProfile)(dev), "gs_4_0");
    SAFE_RELEASE(dev);
}
TEST(gap1_profile_d3d10_1_dll_matches_family) {
    REQUIRE(UIN("d3d10_1.dll", D3D10GetPixelShaderProfile)); REQUIRE(UIN("d3d10_1.dll", D3D10GetVertexShaderProfile)); REQUIRE(UIN("d3d10_1.dll", D3D10GetGeometryShaderProfile));
    const char *p = UIN("d3d10_1.dll", D3D10GetPixelShaderProfile)(NULL), *v = UIN("d3d10_1.dll", D3D10GetVertexShaderProfile)(NULL), *g = UIN("d3d10_1.dll", D3D10GetGeometryShaderProfile)(NULL);
    CHECK(p != NULL && v != NULL && g != NULL);
    if (p && v && g) {
        CHECK(!strncmp(p, "ps_4_", 5) && !strncmp(v, "vs_4_", 5) && !strncmp(g, "gs_4_", 5));
        EXPECT_STR(p, "ps_4_0"); EXPECT_STR(v, "vs_4_0"); EXPECT_STR(g, "gs_4_0");   /* NULL device: the baseline 4.0 profiles */
    }
}

/* ---------------- state block mask: Intersect / Difference ---------------- */
#define MASK_BYTES 76
static void fill_mask(D3D10_STATE_BLOCK_MASK *m, d3dh_rng *r) { for (int i = 0; i < MASK_BYTES; i++) ((unsigned char *)m)[i] = (unsigned char)d3dh_rng_u32(r); }
typedef HRESULT (WINAPI *mask_fn)(D3D10_STATE_BLOCK_MASK *, D3D10_STATE_BLOCK_MASK *, D3D10_STATE_BLOCK_MASK *);

static mask_fn mf_intersect(const char *dll) { return (mask_fn)t_proc_in(dll, "D3D10StateBlockMaskIntersect"); }
static mask_fn mf_difference(const char *dll) { return (mask_fn)t_proc_in(dll, "D3D10StateBlockMaskDifference"); }

static void mask_fuzz(mask_fn f, int is_and, const char *what) {
    d3dh_rng r; d3dh_rng_seed(&r, 0x6a09e667f3bcc908ull);
    int bad = 0;
    for (int it = 0; it < 300; it++) {
        D3D10_STATE_BLOCK_MASK a, b, out;
        fill_mask(&a, &r); fill_mask(&b, &r);
        memset(&out, 0xcc, sizeof out);
        HRESULT hr = f(&a, &b, &out);
        if (hr != S_OK) { bad++; continue; }
        for (int i = 0; i < MASK_BYTES; i++) {
            unsigned char x = ((unsigned char *)&a)[i], y = ((unsigned char *)&b)[i], e = is_and ? (x & y) : (x ^ y);
            if (((unsigned char *)&out)[i] != e) { bad++; break; }
        }
    }
    CHECKF(bad == 0, "%s: %d mismatching iterations", what, bad);
}
TEST(gap1_mask_intersect_random_vectors_d3d10) { mask_fn f = mf_intersect("d3d10.dll"); REQUIRE(f); mask_fuzz(f, 1, "intersect"); }
TEST(gap1_mask_difference_random_vectors_d3d10) { mask_fn f = mf_difference("d3d10.dll"); REQUIRE(f); mask_fuzz(f, 0, "difference"); }
TEST(gap1_mask_intersect_random_vectors_d3d10_1) { mask_fn f = mf_intersect("d3d10_1.dll"); REQUIRE(f); mask_fuzz(f, 1, "intersect(10_1)"); }
TEST(gap1_mask_difference_random_vectors_d3d10_1) { mask_fn f = mf_difference("d3d10_1.dll"); REQUIRE(f); mask_fuzz(f, 0, "difference(10_1)"); }

TEST(gap1_mask_difference_is_symmetric_xor_not_set_subtraction) {
    mask_fn f = mf_difference("d3d10.dll"); REQUIRE(f);
    D3D10_STATE_BLOCK_MASK a, b, ab, ba;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    ((unsigned char *)&a)[3] = 0x0f; ((unsigned char *)&b)[3] = 0xf0;     /* disjoint bits: A-B would keep 0x0f, xor gives 0xff */
    ((unsigned char *)&a)[10] = 0xff; ((unsigned char *)&b)[10] = 0x0f;
    EXPECT_HR(f(&a, &b, &ab), S_OK);
    EXPECT_HR(f(&b, &a, &ba), S_OK);
    EXPECT_EQ(((unsigned char *)&ab)[3], 0xff);
    EXPECT_EQ(((unsigned char *)&ab)[10], 0xf0);
    CHECK(!memcmp(&ab, &ba, sizeof ab));
}
TEST(gap1_mask_intersect_with_complement_is_empty) {
    mask_fn f = mf_intersect("d3d10.dll"); REQUIRE(f);
    d3dh_rng r; d3dh_rng_seed(&r, 99);
    D3D10_STATE_BLOCK_MASK a, na, out;
    fill_mask(&a, &r);
    for (int i = 0; i < MASK_BYTES; i++) ((unsigned char *)&na)[i] = (unsigned char)~((unsigned char *)&a)[i];
    memset(&out, 0xcc, sizeof out);
    EXPECT_HR(f(&a, &na, &out), S_OK);
    for (int i = 0; i < MASK_BYTES; i++) if (((unsigned char *)&out)[i]) { CHECKF(0, "byte %d = %02x", i, ((unsigned char *)&out)[i]); break; }
}
static void mask_null_table(mask_fn f) {
    D3D10_STATE_BLOCK_MASK a, b, out;
    memset(&a, 0x11, sizeof a); memset(&b, 0x22, sizeof b); memset(&out, 0x5a, sizeof out);
    EXPECT_HR(f(NULL, &b, &out), E_INVALIDARG);
    EXPECT_HR(f(&a, NULL, &out), E_INVALIDARG);
    EXPECT_HR(f(&a, &b, NULL), E_INVALIDARG);
    EXPECT_HR(f(NULL, NULL, NULL), E_INVALIDARG);
    for (int i = 0; i < MASK_BYTES; i++) if (((unsigned char *)&out)[i] != 0x5a) { CHECKF(0, "result touched on failure at byte %d", i); break; }
}
TEST(gap1_mask_intersect_null_leaves_result_untouched) { mask_fn f = mf_intersect("d3d10.dll"); REQUIRE(f); mask_null_table(f); }
TEST(gap1_mask_difference_null_leaves_result_untouched) { mask_fn f = mf_difference("d3d10.dll"); REQUIRE(f); mask_null_table(f); }
TEST(gap1_mask_ops_write_exactly_76_bytes) {
    mask_fn fs[2] = { mf_intersect("d3d10.dll"), mf_difference("d3d10.dll") };
    REQUIRE(fs[0]); REQUIRE(fs[1]);
    for (int k = 0; k < 2; k++) {
        unsigned char a[MASK_BYTES + 8], b[MASK_BYTES + 8], out[MASK_BYTES + 8];
        memset(a, 0xff, sizeof a); memset(b, 0xff, sizeof b); memset(out, 0xab, sizeof out);
        EXPECT_HR(fs[k]((D3D10_STATE_BLOCK_MASK *)a, (D3D10_STATE_BLOCK_MASK *)b, (D3D10_STATE_BLOCK_MASK *)out), S_OK);
        for (int i = MASK_BYTES; i < MASK_BYTES + 8; i++) CHECKF(out[i] == 0xab, "op %d overran result at %d", k, i);
        for (int i = 0; i < MASK_BYTES; i++) CHECKF(out[i] == (k == 0 ? 0xff : 0x00), "op %d byte %d", k, i);
    }
}
TEST(gap1_mask_ops_ignore_trailing_input_bytes) {
    mask_fn f = mf_intersect("d3d10.dll"); REQUIRE(f);
    unsigned char a[MASK_BYTES + 8], b[MASK_BYTES + 8], out[MASK_BYTES], out2[MASK_BYTES];
    memset(a, 0xff, sizeof a); memset(b, 0xff, sizeof b);
    EXPECT_HR(f((D3D10_STATE_BLOCK_MASK *)a, (D3D10_STATE_BLOCK_MASK *)b, (D3D10_STATE_BLOCK_MASK *)out), S_OK);
    memset(a + MASK_BYTES, 0, 8);
    EXPECT_HR(f((D3D10_STATE_BLOCK_MASK *)a, (D3D10_STATE_BLOCK_MASK *)b, (D3D10_STATE_BLOCK_MASK *)out2), S_OK);
    CHECK(!memcmp(out, out2, MASK_BYTES));
}
TEST(gap1_mask_result_may_alias_either_input) {
    for (int k = 0; k < 2; k++) {
        mask_fn f = k ? mf_difference("d3d10.dll") : mf_intersect("d3d10.dll"); REQUIRE(f);
        d3dh_rng r; d3dh_rng_seed(&r, 7 + k);
        D3D10_STATE_BLOCK_MASK a, b, want, t1, t2;
        fill_mask(&a, &r); fill_mask(&b, &r);
        EXPECT_HR(f(&a, &b, &want), S_OK);
        t1 = a;
        EXPECT_HR(f(&t1, &b, &t1), S_OK);        /* result == first input */
        CHECK(!memcmp(&t1, &want, MASK_BYTES));
        t2 = b;
        EXPECT_HR(f(&a, &t2, &t2), S_OK);        /* result == second input */
        CHECK(!memcmp(&t2, &want, MASK_BYTES));
        t1 = a;
        EXPECT_HR(f(&t1, &t1, &t1), S_OK);       /* all three the same */
        for (int i = 0; i < MASK_BYTES; i++) if (((unsigned char *)&t1)[i] != (k ? 0 : ((unsigned char *)&a)[i])) { CHECKF(0, "k=%d self-op byte %d", k, i); break; }
    }
}
TEST(gap1_mask_struct_is_76_bytes) {
    CHECK(sizeof(D3D10_STATE_BLOCK_MASK) == MASK_BYTES);
    CHECK(sizeof(((D3D10_STATE_BLOCK_MASK *)0)->VSSamplers) == 2 && sizeof(((D3D10_STATE_BLOCK_MASK *)0)->VSShaderResources) == 16);
}

/* ---------------- D3D10DisassembleShader ---------------- */
TEST(gap1_d3d10disasm_null_arguments) {
    SKIP_ON_WINE("wine d3d10 differs");
    REQUIRE(U(D3D10DisassembleShader));
    ID3DBlob *vs = g1_compile(SH_FULLSCREEN_VS, "vs_4_0", 0); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *out = (ID3DBlob *)(ULONG_PTR)0x1000;
    EXPECT_HR(U(D3D10DisassembleShader)(NULL, 0, FALSE, NULL, &out), G1_D3DERR_INVALIDCALL);
    EXPECT_HR(U(D3D10DisassembleShader)(NULL, BS(vs), FALSE, NULL, &out), G1_D3DERR_INVALIDCALL);
    EXPECT_HR(U(D3D10DisassembleShader)(BP(vs), BS(vs), FALSE, NULL, NULL), G1_D3DERR_INVALIDCALL);
    CHECK(out == (ID3DBlob *)(ULONG_PTR)0x1000);
    BREL(vs);
}
TEST(gap1_d3d10disasm_text_for_vs40) {
    REQUIRE(U(D3D10DisassembleShader));
    ID3DBlob *vs = g1_compile(SH_FULLSCREEN_VS, "vs_4_0", 0); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *out = NULL;
    EXPECT_HR(U(D3D10DisassembleShader)(BP(vs), BS(vs), FALSE, NULL, &out), S_OK);
    if (out) {
        const char *t = (const char *)BP(out);
        CHECK(BS(out) > 0 && strstr(t, "vs_4_0") != NULL);
        CHECK(strstr(t, "dcl_input_sgv") != NULL || strstr(t, "dcl_input") != NULL);
        CHECK(strstr(t, "ret") != NULL);
        if (!memchr(t, 0, BS(out))) t_note("disassembly blob is not NUL terminated");
    }
    BREL(out); BREL(vs);
}
TEST(gap1_d3d10disasm_comment_header) {
    SKIP_ON_WINE("wine disassembler differs");
    REQUIRE(U(D3D10DisassembleShader));
    ID3DBlob *ps = g1_compile(SH_FLAT_PS, "ps_4_0", 0); if (!ps) { t_skip("no compiler"); return; }
    ID3DBlob *out = NULL;
    EXPECT_HR(U(D3D10DisassembleShader)(BP(ps), BS(ps), FALSE, "gap1_marker_comment", &out), S_OK);
    if (out) {
        const char *t = (const char *)BP(out);
        const char *m = strstr(t, "gap1_marker_comment");
        CHECK(m != NULL);
        const char *ps4 = strstr(t, "ps_4_0");
        CHECK(ps4 != NULL);
        if (m && ps4) CHECK(m < ps4);                           /* comments precede the listing */
    }
    BREL(out); BREL(ps);
}
TEST(gap1_d3d10disasm_matches_d3ddisassemble_instructions) {
    REQUIRE(U(D3D10DisassembleShader)); REQUIRE(U(D3DDisassemble));
    ID3DBlob *ps = g1_compile(SH_FLAT_PS, "ps_4_0", 0); if (!ps) { t_skip("no compiler"); return; }
    ID3DBlob *a = NULL, *b = NULL;
    EXPECT_HR(U(D3D10DisassembleShader)(BP(ps), BS(ps), FALSE, NULL, &a), S_OK);
    EXPECT_HR(U(D3DDisassemble)(BP(ps), BS(ps), 0, NULL, &b), S_OK);
    if (a && b) {
        const char *ta = (const char *)BP(a), *tb = (const char *)BP(b);
        const char *ma = strstr(ta, "ps_4_0"), *mb = strstr(tb, "ps_4_0");
        CHECK(ma && mb);
        if (ma && mb) {
            CHECK(strcmp(ma, mb) != 0);   /* Win10: the d3d10 listing is formatted differently from D3DDisassemble's, even after the profile line */
            CHECK(strstr(ma, "mov o0.xyzw") != NULL && strstr(mb, "mov o0.xyzw") != NULL);
        }
    }
    BREL(a); BREL(b); BREL(ps);
}
TEST(gap1_d3d10disasm_color_code_changes_output) {
    REQUIRE(U(D3D10DisassembleShader));
    ID3DBlob *ps = g1_compile(SH_FLAT_PS, "ps_4_0", 0); if (!ps) { t_skip("no compiler"); return; }
    ID3DBlob *plain = NULL, *col = NULL;
    EXPECT_HR(U(D3D10DisassembleShader)(BP(ps), BS(ps), FALSE, NULL, &plain), S_OK);
    HRESULT hr = U(D3D10DisassembleShader)(BP(ps), BS(ps), TRUE, NULL, &col);
    EXPECT_HR(hr, S_OK);
    if (plain && col) {
        CHECK(BS(col) > BS(plain));   /* colour coding adds HTML markup */
        CHECK(strstr((const char *)BP(col), "<font") != NULL || strstr((const char *)BP(col), "<span") != NULL || strstr((const char *)BP(col), "<") != NULL);
    }
    BREL(plain); BREL(col); BREL(ps);
}
TEST(gap1_d3d10disasm_truncated_bytecode_fails) {
    REQUIRE(U(D3D10DisassembleShader));
    ID3DBlob *ps = g1_compile(SH_FLAT_PS, "ps_4_0", 0); if (!ps) { t_skip("no compiler"); return; }
    ID3DBlob *out = NULL;
    HRESULT hr = U(D3D10DisassembleShader)(BP(ps), 12, FALSE, NULL, &out);
    EXPECT_HR_FAILED(hr);
    EXPECT_HR(hr, G1_D3DERR_INVALIDCALL);   /* D3D10DisassembleShader(12 bytes): measured on Win10 */
    BREL(out); BREL(ps);
}

/* ---------------- D3D10ReflectShader ---------------- */
TEST(gap1_d3d10reflect_null_arguments) {
    SKIP_ON_WINE("wine d3d10 differs");
    REQUIRE(U(D3D10ReflectShader));
    ID3DBlob *vs = g1_compile(SH_VS_TRANSFORM, "vs_4_0", 0); if (!vs) { t_skip("no compiler"); return; }
    ID3D10ShaderReflection *r = (ID3D10ShaderReflection *)(ULONG_PTR)0x1000;
    EXPECT_HR(U(D3D10ReflectShader)(NULL, 100, &r), G1_E_INVALIDARG);
    EXPECT_HR(U(D3D10ReflectShader)(BP(vs), BS(vs), NULL), G1_E_INVALIDARG);
    CHECK(r == (ID3D10ShaderReflection *)(ULONG_PTR)0x1000);        /* untouched when an argument is NULL */
    BREL(vs);
}
TEST(gap1_d3d10reflect_garbage_is_invalidarg_and_clears_output) {
    SKIP_ON_WINE("wine d3d10 differs");
    REQUIRE(U(D3D10ReflectShader));
    unsigned char junk[64]; memset(junk, 0x33, sizeof junk);
    ID3D10ShaderReflection *r = (ID3D10ShaderReflection *)(ULONG_PTR)0x1000;
    EXPECT_HR(U(D3D10ReflectShader)(junk, sizeof junk, &r), G1_E_INVALIDARG);
    CHECK(r == NULL);
}
TEST(gap1_d3d10reflect_truncated_container_is_invalidarg) {
    SKIP_ON_WINE("wine d3d10 differs");
    REQUIRE(U(D3D10ReflectShader));
    ID3DBlob *vs = g1_compile(SH_VS_TRANSFORM, "vs_4_0", 0); if (!vs) { t_skip("no compiler"); return; }
    ID3D10ShaderReflection *r = NULL;
    EXPECT_HR(U(D3D10ReflectShader)(BP(vs), 16, &r), G1_E_INVALIDARG);
    CHECK(r == NULL);
    BREL(vs);
}
TEST(gap1_d3d10reflect_desc_for_vs40) {
    SKIP_ON_WINE("wine d3d10 reflection differs");
    REQUIRE(U(D3D10ReflectShader));
    ID3DBlob *vs = g1_compile(SH_VS_TRANSFORM, "vs_4_0", 0); if (!vs) { t_skip("no compiler"); return; }
    ID3D10ShaderReflection *r = NULL;
    EXPECT_HR(U(D3D10ReflectShader)(BP(vs), BS(vs), &r), S_OK);
    if (r) {
        D3D10_SHADER_DESC d; memset(&d, 0, sizeof d);
        EXPECT_HR(ID3D10ShaderReflection_GetDesc(r, &d), S_OK);
        EXPECT_EQ((d.Version >> 16) & 0xffff, 1);            /* vertex shader */
        EXPECT_EQ((d.Version >> 4) & 0xf, 4);
        EXPECT_EQ(d.Version & 0xf, 0);
        CHECK(d.Creator != NULL && d.Creator[0] != 0);
        EXPECT_EQ(d.ConstantBuffers, 1);
        EXPECT_EQ(d.BoundResources, 1);
        EXPECT_EQ(d.InputParameters, 3);
        EXPECT_EQ(d.OutputParameters, 3);
        EXPECT_EQ(d.TextureNormalInstructions, 0);
        CHECK(d.InstructionCount > 0);
        G1_EQ(IUnknown_Release((IUnknown *)r), 0);
    }
    BREL(vs);
}
TEST(gap1_d3d10reflect_signature_elements) {
    SKIP_ON_WINE("wine d3d10 reflection differs");
    REQUIRE(U(D3D10ReflectShader));
    ID3DBlob *vs = g1_compile(SH_VS_TRANSFORM, "vs_4_0", 0); if (!vs) { t_skip("no compiler"); return; }
    ID3D10ShaderReflection *r = NULL;
    EXPECT_HR(U(D3D10ReflectShader)(BP(vs), BS(vs), &r), S_OK);
    if (r) {
        static const char *in_names[3] = { "POSITION", "NORMAL", "TEXCOORD" };
        static const UINT in_mask[3] = { 7, 7, 3 };
        for (UINT i = 0; i < 3; i++) {
            D3D10_SIGNATURE_PARAMETER_DESC sp; memset(&sp, 0, sizeof sp);
            EXPECT_HR(ID3D10ShaderReflection_GetInputParameterDesc(r, i, &sp), S_OK);
            CHECKF(sp.SemanticName && !strcmp(sp.SemanticName, in_names[i]), "input %u name %s", i, sp.SemanticName ? sp.SemanticName : "(null)");
            EXPECT_EQ(sp.SemanticIndex, 0);
            EXPECT_EQ(sp.Register, i);
            EXPECT_EQ(sp.Mask, in_mask[i]);
            EXPECT_EQ(sp.SystemValueType, D3D10_NAME_UNDEFINED);
            EXPECT_EQ(sp.ComponentType, D3D10_REGISTER_COMPONENT_FLOAT32);
        }
        D3D10_SIGNATURE_PARAMETER_DESC o; memset(&o, 0, sizeof o);
        EXPECT_HR(ID3D10ShaderReflection_GetOutputParameterDesc(r, 0, &o), S_OK);
        CHECK(o.SemanticName && !strcmp(o.SemanticName, "SV_Position"));
        EXPECT_EQ(o.SystemValueType, D3D10_NAME_POSITION);
        EXPECT_EQ(o.Mask, 0xf);
        /* out of range index */
        EXPECT_HR_FAILED(ID3D10ShaderReflection_GetInputParameterDesc(r, 3, &o));
        EXPECT_HR_FAILED(ID3D10ShaderReflection_GetOutputParameterDesc(r, 3, &o));
        IUnknown_Release((IUnknown *)r);
    }
    BREL(vs);
}
TEST(gap1_d3d10reflect_constant_buffer_and_binding) {
    SKIP_ON_WINE("wine d3d10 reflection differs");
    REQUIRE(U(D3D10ReflectShader));
    ID3DBlob *vs = g1_compile(SH_VS_TRANSFORM, "vs_4_0", 0); if (!vs) { t_skip("no compiler"); return; }
    ID3D10ShaderReflection *r = NULL;
    EXPECT_HR(U(D3D10ReflectShader)(BP(vs), BS(vs), &r), S_OK);
    if (r) {
        ID3D10ShaderReflectionConstantBuffer *cb = ID3D10ShaderReflection_GetConstantBufferByIndex(r, 0);
        CHECK(cb != NULL);
        if (cb) {
            D3D10_SHADER_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
            EXPECT_HR(ID3D10ShaderReflectionConstantBuffer_GetDesc(cb, &bd), S_OK);
            CHECK(bd.Name && !strcmp(bd.Name, "Transform"));
            EXPECT_EQ(bd.Type, D3D10_CT_CBUFFER);
            EXPECT_EQ(bd.Variables, 1);
            EXPECT_EQ(bd.Size, 64);
            ID3D10ShaderReflectionVariable *v = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, 0);
            D3D10_SHADER_VARIABLE_DESC vd; memset(&vd, 0, sizeof vd);
            if (v) {
                EXPECT_HR(ID3D10ShaderReflectionVariable_GetDesc(v, &vd), S_OK);
                CHECK(vd.Name && !strcmp(vd.Name, "wvp"));
                EXPECT_EQ(vd.StartOffset, 0);
                EXPECT_EQ(vd.Size, 64);
            }
        }
        D3D10_SHADER_INPUT_BIND_DESC ib; memset(&ib, 0, sizeof ib);
        EXPECT_HR(ID3D10ShaderReflection_GetResourceBindingDesc(r, 0, &ib), S_OK);
        CHECK(ib.Name && !strcmp(ib.Name, "Transform"));
        EXPECT_EQ(ib.Type, D3D10_SIT_CBUFFER);
        EXPECT_EQ(ib.BindPoint, 0);
        EXPECT_EQ(ib.BindCount, 1);
        EXPECT_HR_FAILED(ID3D10ShaderReflection_GetResourceBindingDesc(r, 1, &ib));
        IUnknown_Release((IUnknown *)r);
    }
    BREL(vs);
}
TEST(gap1_d3d10reflect_agrees_with_d3dreflect_counts) {
    SKIP_ON_WINE("wine d3d10 reflection differs");
    REQUIRE(U(D3D10ReflectShader)); REQUIRE(U(D3DReflect));
    ID3DBlob *ps = g1_compile(SH_TEXTURED_PS, "ps_4_0", 0); if (!ps) { t_skip("no compiler"); return; }
    ID3D10ShaderReflection *r10 = NULL; ID3D11ShaderReflection *r11 = NULL;
    EXPECT_HR(U(D3D10ReflectShader)(BP(ps), BS(ps), &r10), S_OK);
    EXPECT_HR(U(D3DReflect)(BP(ps), BS(ps), &IID_ID3D11ShaderReflection, (void **)&r11), S_OK);
    if (r10 && r11) {
        D3D10_SHADER_DESC a; D3D11_SHADER_DESC b;
        ID3D10ShaderReflection_GetDesc(r10, &a); r11->lpVtbl->GetDesc(r11, &b);
        EXPECT_EQ(a.BoundResources, b.BoundResources);
        EXPECT_EQ(a.BoundResources, 2);                         /* sampler + texture */
        EXPECT_EQ(a.InputParameters, b.InputParameters);
        EXPECT_EQ(a.OutputParameters, b.OutputParameters);
        EXPECT_EQ(a.InstructionCount, b.InstructionCount);
        EXPECT_EQ(a.TextureNormalInstructions, b.TextureNormalInstructions);
        EXPECT_EQ(a.TextureNormalInstructions, 1);
    }
    if (r10) IUnknown_Release((IUnknown *)r10);
    if (r11) IUnknown_Release((IUnknown *)r11);
    BREL(ps);
}
TEST(gap1_d3d10reflect_sm5_bytecode_observed) {
    SKIP_ON_WINE("wine d3d10 reflection differs");
    REQUIRE(U(D3D10ReflectShader));
    ID3DBlob *ps = g1_compile(SH_FLAT_PS, "ps_5_0", 0); if (!ps) { t_skip("no compiler"); return; }
    ID3D10ShaderReflection *r = NULL;
    HRESULT hr = U(D3D10ReflectShader)(BP(ps), BS(ps), &r);
    EXPECT_HR(hr, G1_E_INVALIDARG);   /* D3D10ReflectShader(ps_5_0): measured on Win10 */
    if (r) IUnknown_Release((IUnknown *)r);
    BREL(ps);
}

/* ---------------- D3D10CompileEffectFromMemory ---------------- */
static const char FX_SRC[] =
    "float4 g_color : COLOR;\n"
    "float4 VS(float4 p : POSITION) : SV_Position { return p; }\n"
    "float4 PS() : SV_Target { return g_color; }\n"
    "technique10 T0 { pass P0 {\n"
    "  SetVertexShader(CompileShader(vs_4_0, VS()));\n"
    "  SetGeometryShader(NULL);\n"
    "  SetPixelShader(CompileShader(ps_4_0, PS()));\n"
    "} }\n";

TEST(gap1_fxcompile_success_returns_blob_and_no_errors) {
    SKIP_ON_WINE("wine d3d10 differs");
    REQUIRE(U(D3D10CompileEffectFromMemory));
    ID3DBlob *fx = NULL, *err = NULL;
    HRESULT hr = U(D3D10CompileEffectFromMemory)((void *)FX_SRC, sizeof FX_SRC - 1, "gap1.fx", NULL, NULL, 0, 0, &fx, &err);
    EXPECT_HR(hr, S_OK);
    CHECK(fx != NULL && BS(fx) > 0);
    if (err) t_note("compile produced a message blob: %.*s", (int)BS(err), (const char *)BP(err));
    BREL(fx); BREL(err);
}
TEST(gap1_fxcompile_syntax_error_reports_text) {
    SKIP_ON_WINE("wine d3d10 differs");
    REQUIRE(U(D3D10CompileEffectFromMemory));
    static const char bad[] = "technique10 T0 { pass P0 { SetVertexShader(CompileShader(vs_4_0, NoSuchFunction())); } }\n";
    ID3DBlob *fx = (ID3DBlob *)(ULONG_PTR)0x1000, *err = NULL;
    HRESULT hr = U(D3D10CompileEffectFromMemory)((void *)bad, sizeof bad - 1, "bad.fx", NULL, NULL, 0, 0, &fx, &err);
    EXPECT_HR_FAILED(hr);
    CHECK(fx == (ID3DBlob *)(ULONG_PTR)0x1000);                      /* effect output only written on success */
    CHECK(err != NULL && BS(err) > 0);
    if (err) {
        CHECK(strstr((const char *)BP(err), "NoSuchFunction") != NULL);
        CHECK(strstr((const char *)BP(err), "bad.fx") != NULL);       /* diagnostics are prefixed with the supplied file name */
    }
    BREL(err);
}
TEST(gap1_fxcompile_macro_definitions_are_applied) {
    REQUIRE(U(D3D10CompileEffectFromMemory));
    static const char src[] =
        "#ifndef GAP1_NEEDED\n#error GAP1_NEEDED is not defined\n#endif\n"
        "technique10 T0 { pass P0 { } }\n";
    D3D10_SHADER_MACRO defs[] = { { "GAP1_NEEDED", "1" }, { NULL, NULL } };
    ID3DBlob *fx = NULL, *err = NULL;
    EXPECT_HR_FAILED(U(D3D10CompileEffectFromMemory)((void *)src, sizeof src - 1, "m.fx", NULL, NULL, 0, 0, &fx, &err));
    BREL(fx); BREL(err);
    EXPECT_HR(U(D3D10CompileEffectFromMemory)((void *)src, sizeof src - 1, "m.fx", defs, NULL, 0, 0, &fx, &err), S_OK);
    BREL(fx); BREL(err);
}

typedef struct { const ID3DIncludeVtbl *lpVtbl; int opens, closes, last_type; char last_name[64]; char data[128]; } g1_inc;
static HRESULT WINAPI inc_open(ID3DInclude *s, D3D_INCLUDE_TYPE t, const char *name, const void *parent, const void **data, UINT *bytes) {
    g1_inc *i = (g1_inc *)s; i->opens++; i->last_type = t; lstrcpynA(i->last_name, name, sizeof i->last_name);
    *data = i->data; *bytes = (UINT)strlen(i->data); return S_OK;
}
static HRESULT WINAPI inc_close(ID3DInclude *s, const void *data) { ((g1_inc *)s)->closes++; return S_OK; }
static const ID3DIncludeVtbl inc_vtbl = { inc_open, inc_close };

TEST(gap1_fxcompile_include_callback_contract) {
    REQUIRE(U(D3D10CompileEffectFromMemory));
    static const char src[] = "#include \"gap1_inc.fxh\"\ntechnique10 T0 { pass P0 { } }\n";
    g1_inc inc = { &inc_vtbl, 0, 0, -1, "", "float4 g_inc_var;\n" };
    ID3DBlob *fx = NULL, *err = NULL;
    HRESULT hr = U(D3D10CompileEffectFromMemory)((void *)src, sizeof src - 1, "inc.fx", NULL, (ID3D10Include *)&inc, 0, 0, &fx, &err);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(inc.opens, 1);
    EXPECT_EQ(inc.closes, 1);                                         /* every Open is paired with a Close */
    EXPECT_STR(inc.last_name, "gap1_inc.fxh");
    EXPECT_EQ(inc.last_type, D3D_INCLUDE_LOCAL);                      /* quoted include */
    BREL(fx); BREL(err);
}
TEST(gap1_fxcompile_debug_flag_needs_sdk_layers) {
    REQUIRE(U(D3D10CompileEffectFromMemory));
    ID3DBlob *fx = NULL, *err = NULL;
    HRESULT hr = U(D3D10CompileEffectFromMemory)((void *)FX_SRC, sizeof FX_SRC - 1, "dbg.fx", NULL, NULL, D3D10_SHADER_DEBUG, 0, &fx, &err);
    /* the D3D10_SHADER_DEBUG path is delegated to the debug-layer implementation; without it the call is E_NOTIMPL */
    CHECK(hr == S_OK || hr == G1_E_NOTIMPL);
    EXPECT_HR(hr, G1_E_NOTIMPL);   /* D3D10_SHADER_DEBUG: measured on Win10 */
    BREL(fx); BREL(err);
}
TEST(gap1_fxcompile_effect_creates_with_device) {
    REQUIRE(U(D3D10CompileEffectFromMemory)); REQUIRE(U(D3D10CreateEffectFromMemory));
    ID3D10Device *dev = NULL; D3D10_DRIVER_TYPE dt;
    if (FAILED(d3dh_make_device10(&dev, &dt, 0)) || !dev) { t_skip("no D3D10 device"); return; }
    ID3DBlob *fx = NULL, *err = NULL;
    EXPECT_HR(U(D3D10CompileEffectFromMemory)((void *)FX_SRC, sizeof FX_SRC - 1, "gap1.fx", NULL, NULL, 0, 0, &fx, &err), S_OK);
    if (fx) {
        ID3D10Effect *e = NULL;
        HRESULT hr = U(D3D10CreateEffectFromMemory)((void *)BP(fx), BS(fx), 0, dev, NULL, &e);
        EXPECT_HR(hr, S_OK);
        if (e) {
            CHECK(ID3D10Effect_IsValid(e));
            D3D10_EFFECT_DESC ed; memset(&ed, 0, sizeof ed);
            EXPECT_HR(ID3D10Effect_GetDesc(e, &ed), S_OK);
            EXPECT_EQ(ed.Techniques, 1);
            EXPECT_EQ(ed.GlobalVariables, 1);
            ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByName(e, "T0");
            CHECK(t && ID3D10EffectTechnique_IsValid(t));
            ID3D10EffectVariable *v = ID3D10Effect_GetVariableByName(e, "g_color");
            CHECK(v && ID3D10EffectVariable_IsValid(v));
            ID3D10EffectVariable *nv = ID3D10Effect_GetVariableByName(e, "no_such_variable");
            CHECK(nv && !ID3D10EffectVariable_IsValid(nv));          /* a null-object, never NULL */
            ID3D10Effect_Release(e);
        }
    }
    BREL(fx); BREL(err); SAFE_RELEASE(dev);
}
