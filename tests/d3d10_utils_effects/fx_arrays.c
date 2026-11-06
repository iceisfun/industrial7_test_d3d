/* Array accessors of effect variables that effect_vars.c does not reach: matrix, bool, bool-vector, int-vector arrays.
 * Values measured on Windows 10 22H2 (d3d10 effects runtime). */
#include "ue.h"

static const char *fx_arr_src =
    "float4x4 mats[3];\n"
    "bool bools[4];\n"
    "bool4 bvecs[2];\n"
    "int4 ivecs[2];\n"
    "technique10 T { pass P { } }\n";
#define BVN(fx, name) ID3D10Effect_GetVariableByName(fx, name)

TEST(d3d10_utils_effects_fxa_matrix_array_and_transpose) {
    NEED_DEVICE(dev);
    FX_OPEN(fx, fx_arr_src);
    ID3D10EffectMatrixVariable *mv = ID3D10EffectVariable_AsMatrix(BVN(fx, "mats"));
    float in[48], out[48], tr[48];
    for (int i = 0; i < 48; i++) in[i] = (float)(i + 1);
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrixArray(mv, in, 0, 3), S_OK);
    memset(out, 0, sizeof out); EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrixArray(mv, out, 0, 3), S_OK);
    EXPECT_EQ(memcmp(in, out, sizeof in), 0);
    memset(tr, 0, sizeof tr); EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrixTransposeArray(mv, tr, 0, 3), S_OK);
    int bad = 0; for (int m = 0; m < 3; m++) for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (tr[m * 16 + r * 4 + c] != in[m * 16 + c * 4 + r]) bad++;
    EXPECT_EQ(bad, 0);
    /* a transposed store of one matrix at element offset 1 leaves its neighbours alone */
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrixTransposeArray(mv, in, 1, 1), S_OK);
    memset(out, 0, sizeof out); EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrixArray(mv, out, 0, 3), S_OK);
    bad = 0; for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (out[16 + r * 4 + c] != in[c * 4 + r]) bad++;
    EXPECT_EQ(bad, 0);                                              /* element 1 = transpose of in[0] */
    EXPECT_EQ(memcmp(out, in, 16 * sizeof(float)), 0);              /* element 0 untouched */
    EXPECT_EQ(memcmp(out + 32, in + 32, 16 * sizeof(float)), 0);    /* element 2 untouched */
    /* the runtime does not range-check offset/count against the array length, nor reject a zero count */
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrixArray(mv, in, 2, 2), S_OK);
    EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrixArray(mv, out, 3, 1), S_OK);
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrixArray(mv, in, 0, 0), S_OK);
    ID3D10Effect_Release(fx);
}

TEST(d3d10_utils_effects_fxa_bool_arrays_store_raw_values_and_ignore_offset) {
    NEED_DEVICE(dev);
    FX_OPEN(fx, fx_arr_src);
    ID3D10EffectScalarVariable *bv = ID3D10EffectVariable_AsScalar(BVN(fx, "bools"));
    BOOL b[4] = { TRUE, FALSE, 5, -1 }, bo[4] = { 9, 9, 9, 9 };
    EXPECT_HR(ID3D10EffectScalarVariable_SetBoolArray(bv, b, 0, 4), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBoolArray(bv, bo, 0, 4), S_OK);
    EXPECT_EQ(memcmp(b, bo, sizeof b), 0);                          /* nonzero values are not normalised to TRUE */
    /* Offset is ignored by both accessors: element 0.. are read and written whatever Offset says */
    BOOL s2[4] = { 10, 11, 12, 13 }, g2[4] = { 9, 9, 9, 9 };
    EXPECT_HR(ID3D10EffectScalarVariable_SetBoolArray(bv, s2, 0, 4), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBoolArray(bv, g2, 2, 2), S_OK);
    EXPECT_EQ(g2[0], 10); EXPECT_EQ(g2[1], 11); EXPECT_EQ(g2[2], 9); EXPECT_EQ(g2[3], 9);
    BOOL s3[2] = { 20, 21 };
    EXPECT_HR(ID3D10EffectScalarVariable_SetBoolArray(bv, s3, 1, 2), S_OK);
    memset(g2, 0, sizeof g2); EXPECT_HR(ID3D10EffectScalarVariable_GetBoolArray(bv, g2, 0, 4), S_OK);
    EXPECT_EQ(g2[0], 20); EXPECT_EQ(g2[1], 21); EXPECT_EQ(g2[2], 12); EXPECT_EQ(g2[3], 13);
    EXPECT_HR(ID3D10EffectScalarVariable_SetBoolArray(bv, b, 3, 4), S_OK);   /* count past the end of the array is accepted */
    ID3D10Effect_Release(fx);
}

TEST(d3d10_utils_effects_fxa_bool_and_int_vector_arrays) {
    NEED_DEVICE(dev);
    FX_OPEN(fx, fx_arr_src);
    ID3D10EffectVectorVariable *vv = ID3D10EffectVariable_AsVector(BVN(fx, "bvecs"));
    BOOL bvin[8] = { 1, 0, 7, 0, 0, 1, 0, -3 }, bvout[8]; memset(bvout, 0x55, sizeof bvout);
    EXPECT_HR(ID3D10EffectVectorVariable_SetBoolVectorArray(vv, bvin, 0, 2), S_OK);
    EXPECT_HR(ID3D10EffectVectorVariable_GetBoolVectorArray(vv, bvout, 0, 2), S_OK);
    EXPECT_EQ(memcmp(bvin, bvout, sizeof bvin), 0);                 /* raw values again */
    ID3D10EffectVectorVariable *iv = ID3D10EffectVariable_AsVector(BVN(fx, "ivecs"));
    int ivin[8] = { 1, -2, 3, -4, 5, -6, 7, -8 }, ivout[8]; memset(ivout, 0, sizeof ivout);
    EXPECT_HR(ID3D10EffectVectorVariable_SetIntVectorArray(iv, ivin, 0, 2), S_OK);
    EXPECT_HR(ID3D10EffectVectorVariable_GetIntVectorArray(iv, ivout, 0, 2), S_OK);
    EXPECT_EQ(memcmp(ivin, ivout, sizeof ivin), 0);
    float fo[8] = { 0 }; EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVectorArray(iv, fo, 0, 2), S_OK);
    EXPECT_EQ(fo[0] == 1.0f && fo[1] == -2.0f && fo[2] == 3.0f && fo[3] == -4.0f, 1);   /* int4[] converts to float on read */
    EXPECT_HR(ID3D10EffectVectorVariable_SetBoolVectorArray(vv, bvin, 1, 2), S_OK);      /* past the end: accepted */
    ID3D10Effect_Release(fx);
}
