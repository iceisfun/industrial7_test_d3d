/* d3dcompiler_47: D3DSetBlobPart / D3DGetOutputSignatureBlob / D3DGetDebugInfo (plus the D3DGetBlobPart behaviour they share). */
#include "gap1.h"

UDECL(HRESULT, D3DSetBlobPart, (const void *, SIZE_T, int, UINT, const void *, SIZE_T, ID3DBlob **));

#define BREL(b) do { if (b) { ID3D10Blob_Release(b); (b) = NULL; } } while (0)
#define BP(b)  ((const unsigned char *)ID3D10Blob_GetBufferPointer(b))
#define BS(b)  ((SIZE_T)ID3D10Blob_GetBufferSize(b))

#define PART_OUTPUT_SIG   1
#define PART_DEBUG_INFO   5
#define PART_PRIVATE_DATA 10
#define PART_ROOT_SIG     11

#define F_PRIV G1_FOURCC('P', 'R', 'I', 'V')
#define F_RTS0 G1_FOURCC('R', 'T', 'S', '0')
#define F_OSGN G1_FOURCC('O', 'S', 'G', 'N')
#define F_OSG1 G1_FOURCC('O', 'S', 'G', '1')
#define F_OSG5 G1_FOURCC('O', 'S', 'G', '5')
#define F_ISGN G1_FOURCC('I', 'S', 'G', 'N')
#define F_SDBG G1_FOURCC('S', 'D', 'B', 'G')
#define F_SHEX G1_FOURCC('S', 'H', 'E', 'X')

static ID3DBlob *fx_vs(UINT flags) { return g1_compile(SH_VS_TRANSFORM, "vs_5_0", flags); }

static const unsigned char *find_osg(const void *p, SIZE_T n, DWORD *fourcc, DWORD *sz) {
    static const DWORD cand[] = { F_OSG5, F_OSG1, F_OSGN };
    for (int i = 0; i < 3; i++) { const unsigned char *d = g1_find_chunk(p, n, cand[i], sz); if (d) { *fourcc = cand[i]; return d; } }
    return NULL;
}

/* ---------------- D3DGetOutputSignatureBlob ---------------- */
TEST(gap1_outsig_null_out) {
    REQUIRE(U(D3DGetOutputSignatureBlob));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    EXPECT_HR(U(D3DGetOutputSignatureBlob)(BP(vs), BS(vs), NULL), G1_D3DERR_INVALIDCALL);
    BREL(vs);
}
TEST(gap1_outsig_vertex_shader_container) {
    REQUIRE(U(D3DGetOutputSignatureBlob));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    DWORD fc = 0, osz = 0;
    const unsigned char *osg = find_osg(BP(vs), BS(vs), &fc, &osz);
    CHECK(osg != NULL);
    ID3DBlob *sig = NULL;
    HRESULT hr = U(D3DGetOutputSignatureBlob)(BP(vs), BS(vs), &sig);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && sig && osg) {
        const unsigned char *p = BP(sig);
        CHECK(g1_is_dxbc(p, BS(sig)));
        EXPECT_EQ(g1_rd32(p + 0x18), BS(sig));
        EXPECT_EQ(*(unsigned short *)(p + 0x14), 1);
        EXPECT_EQ(*(unsigned short *)(p + 0x16), 0);
        EXPECT_EQ(g1_chunk_count(p), 1);                     /* just the signature, nothing else */
        EXPECT_EQ(g1_chunk_fourcc(p, 0), fc);
        EXPECT_EQ(g1_chunk_size_at(p, 0), osz);
        CHECK(!g1_has_chunk(p, BS(sig), F_ISGN));
        CHECK(!g1_has_chunk(p, BS(sig), F_SHEX));
        CHECK(BS(sig) >= 0x20 + 4 + 8 + osz);
        if (g1_chunk_size_at(p, 0) == osz) CHECK(!memcmp(g1_chunk_data_at(p, 0), osg, osz));
    }
    BREL(sig); BREL(vs);
}
TEST(gap1_outsig_equals_getblobpart_output_signature) {
    REQUIRE(U(D3DGetOutputSignatureBlob)); REQUIRE(U(D3DGetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *a = NULL, *b = NULL;
    EXPECT_HR(U(D3DGetOutputSignatureBlob)(BP(vs), BS(vs), &a), S_OK);
    EXPECT_HR(U(D3DGetBlobPart)(BP(vs), BS(vs), D3D_BLOB_OUTPUT_SIGNATURE_BLOB, 0, &b), S_OK);
    if (a && b) { EXPECT_EQ(BS(a), BS(b)); CHECK(BS(a) == BS(b) && !memcmp(BP(a), BP(b), BS(a))); }
    BREL(a); BREL(b); BREL(vs);
}
TEST(gap1_outsig_pixel_shader) {
    REQUIRE(U(D3DGetOutputSignatureBlob));
    ID3DBlob *ps = g1_compile(SH_FLAT_PS, "ps_5_0", 0); if (!ps) { t_skip("no compiler"); return; }
    DWORD fc = 0, osz = 0;
    const unsigned char *osg = find_osg(BP(ps), BS(ps), &fc, &osz);
    ID3DBlob *sig = NULL;
    HRESULT hr = U(D3DGetOutputSignatureBlob)(BP(ps), BS(ps), &sig);
    CHECK((hr == S_OK) == (osg != NULL));                    /* success exactly when the shader carries an output signature */
    if (SUCCEEDED(hr) && sig && osg) {
        EXPECT_EQ(g1_chunk_count(BP(sig)), 1);
        CHECK(!memcmp(g1_chunk_data_at(BP(sig), 0), osg, osz));
    }
    BREL(sig); BREL(ps);
}
TEST(gap1_outsig_shader_without_outputs_matches_chunk_presence) {
    REQUIRE(U(D3DGetOutputSignatureBlob));
    ID3DBlob *ps = g1_compile("void main(float4 p : SV_Position) { }", "ps_5_0", 0); if (!ps) { t_skip("no compiler"); return; }
    DWORD fc = 0, osz = 0;
    const unsigned char *osg = find_osg(BP(ps), BS(ps), &fc, &osz);
    ID3DBlob *sig = NULL;
    HRESULT hr = U(D3DGetOutputSignatureBlob)(BP(ps), BS(ps), &sig);
    if (osg) EXPECT_HR(hr, S_OK); else EXPECT_HR(hr, G1_E_FAIL);
    BREL(sig); BREL(ps);
}
TEST(gap1_outsig_survives_strip_of_debug_parts) {
    REQUIRE(U(D3DGetOutputSignatureBlob)); REQUIRE(U(D3DStripShader));
    ID3DBlob *vs = fx_vs(D3DCOMPILE_DEBUG); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *stripped = NULL;
    HRESULT hr = U(D3DStripShader)(BP(vs), BS(vs), D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_REFLECTION_DATA, &stripped);
    if (SUCCEEDED(hr) && stripped) {
        ID3DBlob *sig = NULL;
        EXPECT_HR(U(D3DGetOutputSignatureBlob)(BP(stripped), BS(stripped), &sig), S_OK);
        BREL(sig);
    } else t_note("D3DStripShader -> %s", d3dh_hr_name(hr));
    BREL(stripped); BREL(vs);
}
TEST(gap1_outsig_rejects_truncated_container) {
    REQUIRE(U(D3DGetOutputSignatureBlob));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *sig = NULL;
    EXPECT_HR_FAILED(U(D3DGetOutputSignatureBlob)(BP(vs), 8, &sig));
    sig = NULL;
    EXPECT_HR_FAILED(U(D3DGetOutputSignatureBlob)(BP(vs), 0x20, &sig));
    BREL(sig); BREL(vs);
}
TEST(gap1_outsig_rejects_corrupt_magic) {
    REQUIRE(U(D3DGetOutputSignatureBlob));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    unsigned char *copy = malloc(BS(vs)); memcpy(copy, BP(vs), BS(vs));
    copy[0] = 'Y';
    ID3DBlob *sig = NULL;
    HRESULT hr = U(D3DGetOutputSignatureBlob)(copy, BS(vs), &sig);
    EXPECT_HR_FAILED(hr);
    EXPECT_HR(hr, G1_D3DERR_INVALIDCALL);   /* corrupt magic: measured on Win10 */
    BREL(sig); free(copy); BREL(vs);
}

/* ---------------- D3DGetDebugInfo ---------------- */
TEST(gap1_debuginfo_null_out) {
    REQUIRE(U(D3DGetDebugInfo));
    ID3DBlob *vs = fx_vs(D3DCOMPILE_DEBUG); if (!vs) { t_skip("no compiler"); return; }
    EXPECT_HR(U(D3DGetDebugInfo)(BP(vs), BS(vs), NULL), G1_D3DERR_INVALIDCALL);
    BREL(vs);
}
TEST(gap1_debuginfo_absent_without_debug_flag) {
    REQUIRE(U(D3DGetDebugInfo));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    CHECK(!g1_has_chunk(BP(vs), BS(vs), F_SDBG));
    ID3DBlob *dbg = NULL;
    EXPECT_HR(U(D3DGetDebugInfo)(BP(vs), BS(vs), &dbg), G1_E_FAIL);
    BREL(dbg); BREL(vs);
}
TEST(gap1_debuginfo_returns_raw_chunk_payload) {
    REQUIRE(U(D3DGetDebugInfo));
    ID3DBlob *vs = fx_vs(D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION); if (!vs) { t_skip("no compiler"); return; }
    DWORD sz = 0;
    const unsigned char *sdbg = g1_find_chunk(BP(vs), BS(vs), F_SDBG, &sz);
    ID3DBlob *dbg = NULL;
    HRESULT hr = U(D3DGetDebugInfo)(BP(vs), BS(vs), &dbg);
    if (!sdbg) { t_note("compiler embedded no SDBG chunk for the debug flag (D3DGetDebugInfo -> %s)", d3dh_hr_name(hr)); EXPECT_HR(hr, G1_E_FAIL); }
    else {
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr) && dbg) {
            /* debug info is returned as the bare chunk payload, not wrapped in a container */
            EXPECT_EQ(BS(dbg), sz);
            CHECK(BS(dbg) == sz && !memcmp(BP(dbg), sdbg, sz));
            CHECK(!g1_is_dxbc(BP(dbg), BS(dbg)));
        }
    }
    BREL(dbg); BREL(vs);
}
TEST(gap1_debuginfo_equals_getblobpart_debug_info) {
    REQUIRE(U(D3DGetDebugInfo)); REQUIRE(U(D3DGetBlobPart));
    ID3DBlob *vs = fx_vs(D3DCOMPILE_DEBUG); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *a = NULL, *b = NULL;
    HRESULT h1 = U(D3DGetDebugInfo)(BP(vs), BS(vs), &a);
    HRESULT h2 = U(D3DGetBlobPart)(BP(vs), BS(vs), D3D_BLOB_DEBUG_INFO, 0, &b);
    EXPECT_HR(h1, h2);
    if (a && b) { EXPECT_EQ(BS(a), BS(b)); CHECK(BS(a) == BS(b) && !memcmp(BP(a), BP(b), BS(a))); }
    BREL(a); BREL(b); BREL(vs);
}
TEST(gap1_debuginfo_rejects_truncated) {
    REQUIRE(U(D3DGetDebugInfo));
    ID3DBlob *vs = fx_vs(D3DCOMPILE_DEBUG); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *dbg = NULL;
    EXPECT_HR_FAILED(U(D3DGetDebugInfo)(BP(vs), 8, &dbg));
    BREL(dbg); BREL(vs);
}

/* ---------------- D3DGetBlobPart selection rules shared by both ---------------- */
TEST(gap1_getblobpart_invalid_part_values) {
    REQUIRE(U(D3DGetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    static const int bad[] = { 13, 14, 100, 0x7fff, 0x8004, -1 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ID3DBlob *o = NULL;
        HRESULT hr = U(D3DGetBlobPart)(BP(vs), BS(vs), (D3D_BLOB_PART)bad[i], 0, &o);
        CHECKF(hr == G1_D3DERR_INVALIDCALL, "part %d -> %s", bad[i], d3dh_hr_name(hr));
        BREL(o);
    }
    BREL(vs);
}
TEST(gap1_getblobpart_missing_chunk_is_efail) {
    SKIP_ON_WINE("private data / root signature parts unsupported");
    REQUIRE(U(D3DGetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *o = NULL;
    /* a plain vertex shader has no private data, root signature, or patch constant signature */
    CHECK(!g1_has_chunk(BP(vs), BS(vs), F_PRIV));
    EXPECT_HR(U(D3DGetBlobPart)(BP(vs), BS(vs), (D3D_BLOB_PART)PART_PRIVATE_DATA, 0, &o), G1_E_FAIL);
    EXPECT_HR(U(D3DGetBlobPart)(BP(vs), BS(vs), (D3D_BLOB_PART)PART_ROOT_SIG, 0, &o), G1_E_FAIL);
    EXPECT_HR(U(D3DGetBlobPart)(BP(vs), BS(vs), D3D_BLOB_PATCH_CONSTANT_SIGNATURE_BLOB, 0, &o), G1_E_FAIL);
    BREL(o); BREL(vs);
}

/* ---------------- D3DSetBlobPart ---------------- */
TEST(gap1_setblobpart_argument_table) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    static const unsigned char pay[16] = "0123456789abcde";
    ID3DBlob *o = NULL;
    EXPECT_HR(UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 1, pay, sizeof pay, &o), G1_D3DERR_INVALIDCALL);       /* flags must be 0 */
    EXPECT_HR(UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 0x80000000u, pay, sizeof pay, &o), G1_D3DERR_INVALIDCALL);
    EXPECT_HR(UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 0, NULL, sizeof pay, &o), G1_D3DERR_INVALIDCALL);       /* part data */
    EXPECT_HR(UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 0, pay, 0, &o), G1_D3DERR_INVALIDCALL);                 /* part size */
    EXPECT_HR(UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 0, pay, (SIZE_T)1 << 32, &o), G1_D3DERR_INVALIDCALL);
    EXPECT_HR(UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 0, pay, sizeof pay, NULL), G1_D3DERR_INVALIDCALL);       /* output */
    CHECK(o == NULL);
    BREL(o); BREL(vs);
}
TEST(gap1_setblobpart_unsupported_part_values) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    static const unsigned char pay[16] = "0123456789abcde";
    /* only PRIVATE_DATA (10), ROOT_SIGNATURE (11) and DEBUG_NAME (12) can be written */
    static const int bad[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13, 14, 255, -1 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ID3DBlob *o = NULL;
        HRESULT hr = UN(D3DSetBlobPart)(BP(vs), BS(vs), bad[i], 0, pay, sizeof pay, &o);
        CHECKF(hr == G1_D3DERR_INVALIDCALL, "part %d -> %s", bad[i], d3dh_hr_name(hr));
        CHECKF(o == NULL, "part %d produced output", bad[i]);
        BREL(o);
    }
    BREL(vs);
}
TEST(gap1_setblobpart_rejects_truncated_source) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart));
    static const unsigned char pay[16] = "0123456789abcde";
    unsigned char src[8] = { 'D', 'X', 'B', 'C', 0, 0, 0, 0 };
    ID3DBlob *o = NULL;
    HRESULT hr = UN(D3DSetBlobPart)(src, sizeof src, PART_PRIVATE_DATA, 0, pay, sizeof pay, &o);
    EXPECT_HR_FAILED(hr);
    CHECK(o == NULL);
    BREL(o);
}
TEST(gap1_setblobpart_private_data_appended) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart)); REQUIRE(U(D3DGetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    static const unsigned char pay[24] = "gap1-private-data-0123";
    DWORD before = g1_chunk_count(BP(vs));
    ID3DBlob *o = NULL;
    HRESULT hr = UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 0, pay, sizeof pay, &o);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && o) {
        const unsigned char *p = BP(o);
        CHECK(g1_is_dxbc(p, BS(o)));
        EXPECT_EQ(g1_rd32(p + 0x18), BS(o));
        EXPECT_EQ(g1_chunk_count(p), before + 1);
        if (g1_chunk_count(p) == before + 1) {
            for (DWORD i = 0; i < before; i++) {                    /* existing chunks keep order and content */
                CHECKF(g1_chunk_fourcc(p, i) == g1_chunk_fourcc(BP(vs), i), "chunk %u fourcc", i);
                CHECKF(g1_chunk_size_at(p, i) == g1_chunk_size_at(BP(vs), i), "chunk %u size", i);
            }
            EXPECT_EQ(g1_chunk_fourcc(p, before), F_PRIV);          /* the new part goes last */
            EXPECT_EQ(g1_chunk_size_at(p, before), sizeof pay);
            CHECK(!memcmp(g1_chunk_data_at(p, before), pay, sizeof pay));
        }
        ID3DBlob *back = NULL;
        EXPECT_HR(U(D3DGetBlobPart)(p, BS(o), (D3D_BLOB_PART)PART_PRIVATE_DATA, 0, &back), S_OK);
        if (back) { EXPECT_EQ(BS(back), sizeof pay); CHECK(BS(back) == sizeof pay && !memcmp(BP(back), pay, sizeof pay)); }
        BREL(back);
        DWORD s1 = 0, s2 = 0;
        const unsigned char *a = g1_find_chunk(BP(vs), BS(vs), F_SHEX, &s1), *b = g1_find_chunk(p, BS(o), F_SHEX, &s2);
        if (a && b) CHECK(s1 == s2 && !memcmp(a, b, s1));
    }
    BREL(o); BREL(vs);
}
TEST(gap1_setblobpart_private_data_replaces_previous) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart)); REQUIRE(U(D3DGetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    static const unsigned char p1[8] = "first!!", p2[32] = "second payload that is longer.";
    ID3DBlob *a = NULL, *b = NULL;
    EXPECT_HR(UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 0, p1, sizeof p1, &a), S_OK);
    if (a) {
        EXPECT_HR(UN(D3DSetBlobPart)(BP(a), BS(a), PART_PRIVATE_DATA, 0, p2, sizeof p2, &b), S_OK);
        if (b) {
            EXPECT_EQ(g1_chunk_count(BP(b)), g1_chunk_count(BP(a)));       /* replaced, not duplicated */
            DWORD n = 0; int priv = 0;
            for (DWORD i = 0; i < g1_chunk_count(BP(b)); i++) if (g1_chunk_fourcc(BP(b), i) == F_PRIV) { priv++; n = g1_chunk_size_at(BP(b), i); }
            EXPECT_EQ(priv, 1);
            EXPECT_EQ(n, sizeof p2);
            ID3DBlob *back = NULL;
            EXPECT_HR(U(D3DGetBlobPart)(BP(b), BS(b), (D3D_BLOB_PART)PART_PRIVATE_DATA, 0, &back), S_OK);
            if (back) CHECK(BS(back) == sizeof p2 && !memcmp(BP(back), p2, sizeof p2));
            BREL(back);
        }
    }
    BREL(a); BREL(b); BREL(vs);
}
TEST(gap1_setblobpart_does_not_modify_source) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    unsigned char *copy = malloc(BS(vs)); memcpy(copy, BP(vs), BS(vs));
    static const unsigned char pay[8] = "payload";
    ID3DBlob *o = NULL;
    EXPECT_HR(UN(D3DSetBlobPart)(copy, BS(vs), PART_PRIVATE_DATA, 0, pay, sizeof pay, &o), S_OK);
    CHECK(!memcmp(copy, BP(vs), BS(vs)));
    CHECK(o == NULL || (const void *)BP(o) != (const void *)copy);
    BREL(o); free(copy); BREL(vs);
}
TEST(gap1_setblobpart_odd_sized_private_data) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart)); REQUIRE(U(D3DGetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    static const unsigned char pay[5] = { 1, 2, 3, 4, 5 };
    ID3DBlob *o = NULL, *back = NULL;
    HRESULT hr = UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_PRIVATE_DATA, 0, pay, sizeof pay, &o);
    EXPECT_HR(hr, S_OK);
    if (o) {
        EXPECT_EQ(g1_rd32(BP(o) + 0x18) % 4, 1);   /* Win10: the 5 byte payload is stored unpadded (offset 4 + header 8 + 5) */
        EXPECT_EQ(g1_rd32(BP(o) + 0x18), BS(o));
        hr = U(D3DGetBlobPart)(BP(o), BS(o), (D3D_BLOB_PART)PART_PRIVATE_DATA, 0, &back);
        EXPECT_HR(hr, S_OK);
        if (back) {
            EXPECT_EQ(BS(back), 5);
            CHECK(BS(back) >= sizeof pay && !memcmp(BP(back), pay, sizeof pay));
        }
    }
    BREL(back); BREL(o); BREL(vs);
}
TEST(gap1_setblobpart_root_signature_requires_container_with_rts0) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart));
    ID3DBlob *vs = fx_vs(0); if (!vs) { t_skip("no compiler"); return; }
    ID3DBlob *o = NULL;
    /* a container that has no RTS0 chunk cannot supply a root signature */
    CHECK(!g1_has_chunk(BP(vs), BS(vs), F_RTS0));
    HRESULT hr = UN(D3DSetBlobPart)(BP(vs), BS(vs), PART_ROOT_SIG, 0, BP(vs), BS(vs), &o);
    EXPECT_HR(hr, G1_E_FAIL);
    CHECK(o == NULL);
    BREL(o); BREL(vs);
}
TEST(gap1_setblobpart_root_signature_roundtrip) {
    SKIP_ON_WINE("D3DSetBlobPart not implemented");
    REQUIRE(UN(D3DSetBlobPart)); REQUIRE(U(D3DGetBlobPart));
    ID3DBlob *rsvs = g1_compile("[RootSignature(\"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)\")] float4 main(float4 p : POSITION) : SV_Position { return p; }", "vs_5_0", 0);
    ID3DBlob *plain = g1_compile(SH_FULLSCREEN_VS, "vs_5_0", 0);
    if (!rsvs || !plain) { BREL(rsvs); BREL(plain); t_skip("compiler without root signature support"); return; }
    DWORD rsz = 0;
    const unsigned char *rts = g1_find_chunk(BP(rsvs), BS(rsvs), F_RTS0, &rsz);
    if (!rts) { t_note("compiler did not embed RTS0"); BREL(rsvs); BREL(plain); t_skip("no RTS0"); return; }
    ID3DBlob *rsblob = NULL;
    EXPECT_HR(U(D3DGetBlobPart)(BP(rsvs), BS(rsvs), (D3D_BLOB_PART)PART_ROOT_SIG, 0, &rsblob), S_OK);
    if (rsblob) {
        CHECK(g1_is_dxbc(BP(rsblob), BS(rsblob)));                       /* signature parts are wrapped in a container */
        EXPECT_EQ(g1_chunk_count(BP(rsblob)), 1);
        EXPECT_EQ(g1_chunk_fourcc(BP(rsblob), 0), F_RTS0);
        ID3DBlob *merged = NULL;
        HRESULT hr = UN(D3DSetBlobPart)(BP(plain), BS(plain), PART_ROOT_SIG, 0, BP(rsblob), BS(rsblob), &merged);
        EXPECT_HR(hr, S_OK);
        if (merged) {
            DWORD msz = 0;
            const unsigned char *m = g1_find_chunk(BP(merged), BS(merged), F_RTS0, &msz);
            CHECK(m != NULL);
            if (m) CHECK(msz == rsz && !memcmp(m, rts, rsz));
            CHECK(g1_chunk_count(BP(merged)) == g1_chunk_count(BP(plain)) + 1);
        }
        BREL(merged);
    }
    BREL(rsblob); BREL(rsvs); BREL(plain);
}
