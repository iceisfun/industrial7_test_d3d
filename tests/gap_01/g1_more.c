/* Additional depth tests: D3DDisassembleRegion, D3DGetTraceInstructionOffsets, D3D10PreprocessShader, D3DCreateBlob / D3DCompile2 edge cases. */
#include "gap1.h"

UDECL(HRESULT, D3DDisassembleRegion, (const void *, SIZE_T, UINT, const char *, SIZE_T, SIZE_T, SIZE_T *, ID3DBlob **));
UDECL(HRESULT, D3DGetTraceInstructionOffsets, (const void *, SIZE_T, UINT, SIZE_T, SIZE_T, SIZE_T *, SIZE_T *));
UDECL(HRESULT, D3D10PreprocessShader, (const char *, SIZE_T, const char *, const void *, void *, ID3DBlob **, ID3DBlob **));
UDECL(HRESULT, D3DCreateBlob, (SIZE_T, ID3DBlob **));
UDECL(HRESULT, D3DCompile2, (const void *, SIZE_T, const char *, const D3D_SHADER_MACRO *, void *, const char *, const char *, UINT, UINT, UINT, const void *, SIZE_T, ID3DBlob **, ID3DBlob **));
#define BREL(b) do { if (b) { ID3D10Blob_Release(b); (b) = NULL; } } while (0)
#define BP(b)  ((const unsigned char *)ID3D10Blob_GetBufferPointer(b))
#define BS(b)  ((SIZE_T)ID3D10Blob_GetBufferSize(b))

static ID3DBlob *flat_ps(void) { return g1_compile(SH_FLAT_PS, "ps_5_0", 0); }

TEST(gap1_disasm_region_whole_shader) {
    REQUIRE(UN(D3DDisassembleRegion));
    ID3DBlob *ps = flat_ps(), *o = NULL; if (!ps) { t_skip("no compiler"); return; }
    SIZE_T fin = 0xdead;
    EXPECT_HR(UN(D3DDisassembleRegion)(BP(ps), BS(ps), 0, NULL, 0, 100, &fin, &o), S_OK);
    EXPECT_EQ(fin, 60);   /* Win10: byte offset behind the last instruction, relative to the SHEX chunk payload (the whole 60 byte chunk) */
    CHECK(o && strstr((const char *)BP(o), "mov o0.xyzw") && strstr((const char *)BP(o), "ret"));
    BREL(o);
    /* the finish offset is optional */
    EXPECT_HR(UN(D3DDisassembleRegion)(BP(ps), BS(ps), 0, NULL, 0, 2, NULL, &o), S_OK);
    CHECK(o != NULL); BREL(o);
    BREL(ps);
}
TEST(gap1_disasm_region_comment_and_zero_count) {
    REQUIRE(UN(D3DDisassembleRegion));
    ID3DBlob *ps = flat_ps(), *o = NULL; if (!ps) { t_skip("no compiler"); return; }
    SIZE_T fin = 0xdead;
    EXPECT_HR(UN(D3DDisassembleRegion)(BP(ps), BS(ps), 0, "hello-region", 0, 2, &fin, &o), S_OK);
    CHECK(o && strstr((const char *)BP(o), "hello-region"));   /* szComments is emitted in the listing */
    BREL(o);
    /* NumInsts 0 and 1 both list only the header (no instruction text), finish offset 0 */
    EXPECT_HR(UN(D3DDisassembleRegion)(BP(ps), BS(ps), 0, NULL, 0, 0, &fin, &o), S_OK);
    EXPECT_EQ(fin, 0); CHECK(o && !strstr((const char *)BP(o), "mov o0.xyzw")); BREL(o);
    fin = 0xdead;
    EXPECT_HR(UN(D3DDisassembleRegion)(BP(ps), BS(ps), 0, NULL, 0, 1, &fin, &o), S_OK);
    EXPECT_EQ(fin, 0); BREL(o);
    BREL(ps);
}
TEST(gap1_trace_instruction_offsets) {
    REQUIRE(UN(D3DGetTraceInstructionOffsets));
    ID3DBlob *ps = flat_ps(); if (!ps) { t_skip("no compiler"); return; }
    SIZE_T offs[16], tot = 0xdead;
    EXPECT_HR(UN(D3DGetTraceInstructionOffsets)(BP(ps), BS(ps), 0, 0, 16, offs, &tot), S_OK);
    EXPECT_EQ(tot, 2);                       /* mov + ret */
    EXPECT_EQ(offs[0], 24); EXPECT_EQ(offs[1], 56);   /* Win10: byte offsets inside the SHEX payload */
    tot = 0xdead;
    EXPECT_HR(UN(D3DGetTraceInstructionOffsets)(BP(ps), BS(ps), 0, 0, 0, NULL, &tot), S_OK);   /* count only */
    EXPECT_EQ(tot, 2);
    tot = 0xdead;
    EXPECT_HR(UN(D3DGetTraceInstructionOffsets)(BP(ps), BS(ps), 1, 0, 0, NULL, &tot), S_OK);   /* flags 1 (D3D_GET_INST_OFFSETS_INCLUDE_NON_EXECUTABLE) counts declarations too */
    EXPECT_EQ(tot, 4);
    EXPECT_HR(UN(D3DGetTraceInstructionOffsets)(BP(ps), BS(ps), 0, 1000, 4, offs, &tot), S_OK);   /* a start index behind the end is not an error */
    EXPECT_HR(UN(D3DGetTraceInstructionOffsets)(BP(ps), BS(ps), 0, 0, 4, NULL, &tot), S_OK);       /* NULL offsets with a non-zero count is accepted */
    BREL(ps);
}
TEST(gap1_d3d10_preprocess_expands_macros) {
    REQUIRE(UN(D3D10PreprocessShader));
    const char *src = "#define A 5\nfloat4 main() : SV_Target { return float4(A, B, __LINE__, 0); }\n";
    D3D_SHADER_MACRO m[] = { { "B", "7" }, { NULL, NULL } };
    ID3DBlob *t = NULL, *e = NULL;
    EXPECT_HR(UN(D3D10PreprocessShader)(src, strlen(src), "x.hlsl", m, NULL, &t, &e), S_OK);
    /* Win10: tokens are re-emitted separated by blanks, user and #define macros are expanded, __LINE__ is left alone */
    CHECK(t && !strcmp((const char *)BP(t), "float4 main ( ) : SV_Target \n{\n    return float4 ( 5 , 7 , __LINE__ , 0 ) ;\n}\n"));
    EXPECT_EQ(t ? BS(t) : 0, 79);
    BREL(t); BREL(e);
    EXPECT_HR(UN(D3D10PreprocessShader)(src, strlen(src), NULL, NULL, NULL, NULL, &e), G1_E_INVALIDARG);   /* ppShaderText is required */
    BREL(e);
    EXPECT_HR(UN(D3D10PreprocessShader)(src, 0, "x.hlsl", NULL, NULL, &t, &e), S_OK);   /* empty source */
    BREL(t); BREL(e);
}
TEST(gap1_d3d10_preprocess_include_without_handler) {
    REQUIRE(UN(D3D10PreprocessShader));
    ID3DBlob *t = NULL, *e = NULL;
    EXPECT_HR(UN(D3D10PreprocessShader)("#include \"nofile.h\"\n", 20, "x.hlsl", NULL, NULL, &t, &e), G1_E_FAIL);
    CHECK(e && strstr((const char *)BP(e), "No include handler specified"));
    BREL(t); BREL(e);
}
TEST(gap1_d3dcreateblob_sizes_and_arguments) {
    REQUIRE(UN(D3DCreateBlob));
    static const SIZE_T sizes[] = { 0, 1, 7, 4096 };
    for (int i = 0; i < 4; i++) {
        ID3DBlob *b = NULL;
        EXPECT_HR(UN(D3DCreateBlob)(sizes[i], &b), S_OK);
        CHECK(b != NULL);
        if (!b) continue;
        EXPECT_EQ(BS(b), sizes[i]);
        CHECK(BP(b) != NULL);   /* even a zero-sized blob has a buffer pointer */
        if (sizes[i]) memset((void *)BP(b), 0xAB, sizes[i]);
        G1_EQ(ID3D10Blob_AddRef(b), 2); G1_EQ(ID3D10Blob_Release(b), 1);
        G1_EQ(ID3D10Blob_Release(b), 0);
    }
    EXPECT_HR(UN(D3DCreateBlob)(1, NULL), G1_D3DERR_INVALIDCALL);
    ID3DBlob *h = (ID3DBlob *)(ULONG_PTR)0x5ab0c1d2;
    EXPECT_HR(UN(D3DCreateBlob)((SIZE_T)-1 / 2, &h), (HRESULT)0x8007000e);   /* E_OUTOFMEMORY */
    CHECK(h == (ID3DBlob *)(ULONG_PTR)0x5ab0c1d2);   /* untouched on failure */
}
TEST(gap1_d3dcompile2_secondary_data_arguments) {
    REQUIRE(UN(D3DCompile2));
    ID3DBlob *o = NULL, *e = NULL;
    EXPECT_HR(UN(D3DCompile2)(SH_FLAT_PS, sizeof SH_FLAT_PS - 1, "a.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, 0, NULL, 0, &o, &e), S_OK);
    EXPECT_EQ(o ? BS(o) : 0, 488);   /* identical to D3DCompile's output for the same source (deterministic) */
    BREL(o); BREL(e);
    /* secondary flags without data are accepted, secondary data without flags is rejected */
    EXPECT_HR(UN(D3DCompile2)(SH_FLAT_PS, sizeof SH_FLAT_PS - 1, "a.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, 1, NULL, 0, &o, &e), S_OK);
    BREL(o); BREL(e);
    EXPECT_HR(UN(D3DCompile2)(SH_FLAT_PS, sizeof SH_FLAT_PS - 1, "a.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, 0, "xx", 2, &o, &e), G1_E_INVALIDARG);
    BREL(o); BREL(e);
    /* ppCode may be NULL: the compile is performed and only the success code returned */
    EXPECT_HR(UN(D3DCompile2)(SH_FLAT_PS, sizeof SH_FLAT_PS - 1, "a.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0, 0, NULL, 0, NULL, &e), S_OK);
    BREL(e);
}
