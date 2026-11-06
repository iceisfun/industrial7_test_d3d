/* Effect variable casts, descriptors and value roundtrips (CPU-side shadow memory of the effect). */
#include "ue.h"
#include <float.h>
#include <limits.h>

#define E_INVALIDARG_ ((HRESULT)0x80070057u)
#define E_FAIL_       ((HRESULT)0x80004005u)

#define BV(fx, name) ID3D10Effect_GetVariableByName(fx, name)

/* ---- casts ---- */
typedef struct { const char *name; unsigned mask; } castrow;
enum { C_SCALAR = 1, C_VECTOR = 2, C_MATRIX = 4, C_STRING = 8, C_SRV = 16, C_RTV = 32, C_DSV = 64, C_CB = 128, C_SHADER = 256, C_BLEND = 512, C_DSS = 1024, C_RAST = 2048, C_SAMP = 4096 };

static unsigned cast_mask(ID3D10EffectVariable *v)
{
    unsigned m = 0;
    if (ID3D10EffectScalarVariable_IsValid(ID3D10EffectVariable_AsScalar(v))) m |= C_SCALAR;
    if (ID3D10EffectVectorVariable_IsValid(ID3D10EffectVariable_AsVector(v))) m |= C_VECTOR;
    if (ID3D10EffectMatrixVariable_IsValid(ID3D10EffectVariable_AsMatrix(v))) m |= C_MATRIX;
    if (ID3D10EffectStringVariable_IsValid(ID3D10EffectVariable_AsString(v))) m |= C_STRING;
    if (ID3D10EffectShaderResourceVariable_IsValid(ID3D10EffectVariable_AsShaderResource(v))) m |= C_SRV;
    if (ID3D10EffectRenderTargetViewVariable_IsValid(ID3D10EffectVariable_AsRenderTargetView(v))) m |= C_RTV;
    if (ID3D10EffectDepthStencilViewVariable_IsValid(ID3D10EffectVariable_AsDepthStencilView(v))) m |= C_DSV;
    if (ID3D10EffectConstantBuffer_IsValid(ID3D10EffectVariable_AsConstantBuffer(v))) m |= C_CB;
    if (ID3D10EffectShaderVariable_IsValid(ID3D10EffectVariable_AsShader(v))) m |= C_SHADER;
    if (ID3D10EffectBlendVariable_IsValid(ID3D10EffectVariable_AsBlend(v))) m |= C_BLEND;
    if (ID3D10EffectDepthStencilVariable_IsValid(ID3D10EffectVariable_AsDepthStencil(v))) m |= C_DSS;
    if (ID3D10EffectRasterizerVariable_IsValid(ID3D10EffectVariable_AsRasterizer(v))) m |= C_RAST;
    if (ID3D10EffectSamplerVariable_IsValid(ID3D10EffectVariable_AsSampler(v))) m |= C_SAMP;
    return m;
}
static void check_casts(ID3D10Effect *fx, const castrow *rows, int n)
{
    for (int i = 0; i < n; i++) {
        ID3D10EffectVariable *v = BV(fx, rows[i].name);
        if (!v || !ID3D10EffectVariable_IsValid(v)) { t_check(0, __FILE__, __LINE__, "variable exists", "%s", rows[i].name); continue; }
        unsigned m = cast_mask(v);
        t_check(m == rows[i].mask, __FILE__, __LINE__, "cast validity mask", "%s: got 0x%x want 0x%x", rows[i].name, m, rows[i].mask);
    }
}
TEST(d3d10_utils_effects_fxv_casts_numeric)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const castrow rows[] = {
        { "vecA", C_VECTOR }, { "fA", C_SCALAR }, { "iA", C_SCALAR }, { "bA", C_SCALAR }, { "mA", C_MATRIX },
        { "arrA", C_SCALAR }, { "v3", C_VECTOR }, { "iv2", C_VECTOR }, { "vArr", C_VECTOR },
    };
    check_casts(fx, rows, sizeof rows / sizeof *rows);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_casts_objects)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    static const castrow rows[] = {
        { "tex", C_SRV }, { "texarr", C_SRV }, { "tex3", C_SRV }, { "smp", C_SAMP }, { "bs", C_BLEND }, { "rs", C_RAST },
        { "dss", C_DSS }, { "rtv", C_RTV }, { "dsv", C_DSV }, { "gcol", C_VECTOR }, { "gscalar", C_SCALAR },
    };
    check_casts(fx, rows, sizeof rows / sizeof *rows);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_casts_string)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_STRING);
    static const castrow rows[] = { { "title", C_STRING }, { "names", C_STRING } };
    check_casts(fx, rows, 2);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_cast_returns_same_object)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *v = BV(fx, "fA");
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(v);
    EXPECT_EQ(ID3D10EffectVariable_AsScalar(v), s);
    EXPECT_EQ((void *)s, (void *)v);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_cast_invalid_results_shared)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *f = BV(fx, "fA"), *m = BV(fx, "mA");
    ID3D10EffectVectorVariable *a = ID3D10EffectVariable_AsVector(f), *b = ID3D10EffectVariable_AsVector(m);
    EXPECT_EQ(a, b);
    ID3D10EffectScalarVariable *c = ID3D10EffectVariable_AsScalar(m), *d = ID3D10EffectVariable_AsScalar(BV(fx, "vecA"));
    EXPECT_EQ(c, d);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_cast_mismatch_methods_fail)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "vecA"));
    CHECK(s && !ID3D10EffectScalarVariable_IsValid(s));
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 1.0f), E_FAIL_);
    ID3D10EffectVectorVariable *v = ID3D10EffectVariable_AsVector(BV(fx, "fA"));
    float f4[4] = {1, 2, 3, 4};
    EXPECT_HR(ID3D10EffectVectorVariable_SetFloatVector(v, f4), E_FAIL_);
    ID3D10EffectMatrixVariable *m = ID3D10EffectVariable_AsMatrix(BV(fx, "vecA"));
    float m16[16] = {0};
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrix(m, m16), E_FAIL_);
    float f = 0;
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(BV(fx, "fA")), &f), S_OK);
    EXPECT_EQ(f == 1.5f, 1);
    ID3D10Effect_Release(fx);
}

/* ---- type descriptors ---- */
typedef struct { const char *var, *type; D3D_SHADER_VARIABLE_CLASS cls; D3D_SHADER_VARIABLE_TYPE ty; UINT elems, rows, cols, packed, unpacked; } typerow;
TEST(d3d10_utils_effects_fxv_type_descs)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const typerow rows[] = {
        { "vecA", "float4", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 0, 1, 4, 16, 16 },
        { "fA", "float", D3D_SVC_SCALAR, D3D_SVT_FLOAT, 0, 1, 1, 4, 4 },
        { "iA", "int", D3D_SVC_SCALAR, D3D_SVT_INT, 0, 1, 1, 4, 4 },
        { "bA", "bool", D3D_SVC_SCALAR, D3D_SVT_BOOL, 0, 1, 1, 4, 4 },
        { "mA", "float4x4", D3D_SVC_MATRIX_ROWS, D3D_SVT_FLOAT, 0, 4, 4, 64, 64 },
        { "arrA", "float", D3D_SVC_SCALAR, D3D_SVT_FLOAT, 3, 1, 1, 12, 36 },
        { "v3", "float3", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 0, 1, 3, 12, 12 },
        { "iv2", "int2", D3D_SVC_VECTOR, D3D_SVT_INT, 0, 1, 2, 8, 8 },
        { "vArr", "float4", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 2, 1, 4, 32, 32 },
    };
    for (unsigned i = 0; i < sizeof rows / sizeof *rows; i++) {
        D3D10_EFFECT_TYPE_DESC d;
        ID3D10EffectType *t = ID3D10EffectVariable_GetType(BV(fx, rows[i].var));
        if (!t || !ID3D10EffectType_IsValid(t) || FAILED(ID3D10EffectType_GetDesc(t, &d))) { t_check(0, __FILE__, __LINE__, "type desc", "%s", rows[i].var); continue; }
        t_check(d.TypeName && !strcmp(d.TypeName, rows[i].type), __FILE__, __LINE__, "TypeName", "%s: %s", rows[i].var, d.TypeName ? d.TypeName : "(null)");
        t_check(d.Class == rows[i].cls, __FILE__, __LINE__, "Class", "%s: %d", rows[i].var, d.Class);
        t_check(d.Type == rows[i].ty, __FILE__, __LINE__, "Type", "%s: %d", rows[i].var, d.Type);
        t_check(d.Elements == rows[i].elems, __FILE__, __LINE__, "Elements", "%s: %u", rows[i].var, d.Elements);
        t_check(d.Members == 0, __FILE__, __LINE__, "Members", "%s: %u", rows[i].var, d.Members);
        t_check(d.Rows == rows[i].rows && d.Columns == rows[i].cols, __FILE__, __LINE__, "Rows/Columns", "%s: %ux%u", rows[i].var, d.Rows, d.Columns);
        t_check(d.PackedSize == rows[i].packed, __FILE__, __LINE__, "PackedSize", "%s: %u want %u", rows[i].var, d.PackedSize, rows[i].packed);
        t_check(d.UnpackedSize == rows[i].unpacked, __FILE__, __LINE__, "UnpackedSize", "%s: %u want %u", rows[i].var, d.UnpackedSize, rows[i].unpacked);
        t_check(d.Stride == (strcmp(rows[i].var, "mA") ? 16u : 64u), __FILE__, __LINE__, "Stride", "%s: %u", rows[i].var, d.Stride);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_type_desc_null)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectType *t = ID3D10EffectVariable_GetType(BV(fx, "fA"));
    HRESULT hr = ID3D10EffectType_GetDesc(t, NULL);
    CHECK(FAILED(hr));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_type_pointers_shared_for_same_type)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectType *a = ID3D10EffectVariable_GetType(BV(fx, "fA"));
    ID3D10EffectType *b = ID3D10EffectVariable_GetType(BV(fx, "fA"));
    EXPECT_EQ(a, b);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_type_members_of_non_struct)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectType *t = ID3D10EffectVariable_GetType(BV(fx, "vecA"));
    ID3D10EffectType *m = ID3D10EffectType_GetMemberTypeByIndex(t, 0);
    CHECK(m && !ID3D10EffectType_IsValid(m));
    m = ID3D10EffectType_GetMemberTypeByName(t, "x");
    CHECK(m && !ID3D10EffectType_IsValid(m));
    EXPECT_EQ(ID3D10EffectType_GetMemberName(t, 0), NULL);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_struct_members)
{
    NEED_DEVICE(dev);
    const char *src =
        "struct Light { float4 pos; float3 dir; float range; int kind; };\n"
        "cbuffer cbL { Light lights[2]; Light single : LIGHTSEM; float tail; };\n"
        "technique10 T { pass P { } }\n";
    FX_OPEN(fx, src);
    ID3D10EffectVariable *s = BV(fx, "single");
    CHECK(s && ID3D10EffectVariable_IsValid(s));
    D3D10_EFFECT_TYPE_DESC td;
    ID3D10EffectType *t = ID3D10EffectVariable_GetType(s);
    EXPECT_HR(ID3D10EffectType_GetDesc(t, &td), S_OK);
    EXPECT_EQ(td.Class, D3D_SVC_STRUCT);
    EXPECT_EQ(td.Members, 4);
    t_check(td.TypeName && !strcmp(td.TypeName, "Light"), __FILE__, __LINE__, "struct type name", "%s", td.TypeName ? td.TypeName : "(null)");
    static const char *const names[4] = { "pos", "dir", "range", "kind" };
    for (UINT i = 0; i < 4; i++) {
        LPCSTR mn = ID3D10EffectType_GetMemberName(t, i);
        t_check(mn && !strcmp(mn, names[i]), __FILE__, __LINE__, "member name", "%u", i);
        ID3D10EffectVariable *m = ID3D10EffectVariable_GetMemberByIndex(s, i);
        ID3D10EffectVariable *n = ID3D10EffectVariable_GetMemberByName(s, names[i]);
        CHECK(m && ID3D10EffectVariable_IsValid(m));
        EXPECT_EQ(m, n);
        D3D10_EFFECT_VARIABLE_DESC vd;
        if (m && SUCCEEDED(ID3D10EffectVariable_GetDesc(m, &vd))) t_check(vd.Name && !strcmp(vd.Name, names[i]), __FILE__, __LINE__, "member var name", "%u", i);
    }
    ID3D10EffectVariable *bad = ID3D10EffectVariable_GetMemberByIndex(s, 4);
    CHECK(bad && !ID3D10EffectVariable_IsValid(bad));
    bad = ID3D10EffectVariable_GetMemberByName(s, "POS");
    CHECK(bad && !ID3D10EffectVariable_IsValid(bad));
    bad = ID3D10EffectVariable_GetMemberByName(s, NULL);
    CHECK(bad && !ID3D10EffectVariable_IsValid(bad));
    LPCSTR mn = ID3D10EffectType_GetMemberName(t, 4);
    EXPECT_EQ(mn, NULL);
    /* member values are writable through the member interface */
    ID3D10EffectScalarVariable *r = ID3D10EffectVariable_AsScalar(ID3D10EffectVariable_GetMemberByName(s, "range"));
    float f = 0;
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(r, 12.5f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(r, &f), S_OK);
    EXPECT_EQ(f == 12.5f, 1);
    ID3D10EffectScalarVariable *k = ID3D10EffectVariable_AsScalar(ID3D10EffectVariable_GetMemberByName(s, "kind"));
    int ki = 0;
    EXPECT_HR(ID3D10EffectScalarVariable_SetInt(k, 77), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(k, &ki), S_OK);
    EXPECT_EQ(ki, 77);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(r, &f), S_OK);
    EXPECT_EQ(f == 12.5f, 1);
    /* arrays of structs */
    ID3D10EffectVariable *la = BV(fx, "lights");
    ID3D10EffectVariable *l1 = ID3D10EffectVariable_GetElement(la, 1);
    CHECK(l1 && ID3D10EffectVariable_IsValid(l1));
    ID3D10EffectVariable *l2 = ID3D10EffectVariable_GetElement(la, 2);
    CHECK(l2 && !ID3D10EffectVariable_IsValid(l2));
    ID3D10EffectVariable *m = ID3D10EffectVariable_GetMemberByName(l1, "dir");
    CHECK(m && ID3D10EffectVariable_IsValid(m));
    ID3D10Effect_Release(fx);
}

/* ---- scalar ---- */
TEST(d3d10_utils_effects_fxv_scalar_defaults)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    float f = -1; int i = -1; BOOL b = 0;
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(BV(fx, "fA")), &f), S_OK);
    EXPECT_EQ(f == 1.5f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(ID3D10EffectVariable_AsScalar(BV(fx, "iA")), &i), S_OK);
    EXPECT_EQ(i, 3);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(ID3D10EffectVariable_AsScalar(BV(fx, "bA")), &b), S_OK);
    CHECK(b != 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_float_roundtrip)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "fA"));
    static const float vals[] = { 0.0f, 1.0f, -1.0f, 0.1f, 123456.789f, -3.5e20f, 1e-30f, FLT_MAX, FLT_MIN, -FLT_MAX, 16777217.0f };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        float g = 12345.0f;
        EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, vals[i]), S_OK);
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &g), S_OK);
        t_check(memcmp(&g, &vals[i], 4) == 0, __FILE__, __LINE__, "float roundtrip", "%g -> %g", vals[i], g);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_float_negative_zero)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "fA"));
    float nz = -0.0f, g = 1;
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, nz), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &g), S_OK);
    EXPECT_EQ(memcmp(&g, &nz, 4), 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_float_int_conversions)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "fA"));
    float f; int i;
    EXPECT_HR(ID3D10EffectScalarVariable_SetInt(s, 5), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK); EXPECT_EQ(f == 5.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetInt(s, -300), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK); EXPECT_EQ(f == -300.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 2.7f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, 2);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, -2.7f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, -2);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 0.99f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_float_bool_conversions)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "fA"));
    float f; BOOL b;
    EXPECT_HR(ID3D10EffectScalarVariable_SetBool(s, TRUE), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK); EXPECT_EQ(f == -1.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetBool(s, 5), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK); EXPECT_EQ(f == -1.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetBool(s, FALSE), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK); EXPECT_EQ(f == 0.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 0.0f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(s, &b), S_OK); EXPECT_EQ(b, 0);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 0.001f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(s, &b), S_OK);
    CHECK(b != 0);
    EXPECT_EQ(b, -1);                                 /* Win10: a non-zero float reads back as -1 (all bits set), like the other numeric-to-bool conversions */
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, -7.0f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(s, &b), S_OK);
    CHECK(b != 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_int_roundtrip)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "iA"));
    static const int vals[] = { 0, 1, -1, 7, 255, 65536, INT_MAX, INT_MIN, 0x12345678 };
    for (unsigned k = 0; k < sizeof vals / sizeof *vals; k++) {
        int g = 99;
        EXPECT_HR(ID3D10EffectScalarVariable_SetInt(s, vals[k]), S_OK);
        EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &g), S_OK);
        t_check(g == vals[k], __FILE__, __LINE__, "int roundtrip", "%d -> %d", vals[k], g);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_int_conversions)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "iA"));
    int i; float f;
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 3.9f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, 3);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, -3.9f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, -3);
    EXPECT_HR(ID3D10EffectScalarVariable_SetInt(s, 7), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK); EXPECT_EQ(f == 7.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetBool(s, 7), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, -1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetBool(s, 0), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_bool)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "bA"));
    BOOL b; int i; float f;
    EXPECT_HR(ID3D10EffectScalarVariable_SetBool(s, FALSE), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(s, &b), S_OK); EXPECT_EQ(b, 0);
    EXPECT_HR(ID3D10EffectScalarVariable_SetBool(s, TRUE), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(s, &b), S_OK); CHECK(b != 0);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 0.5f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(s, &b), S_OK); CHECK(b != 0);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK); EXPECT_EQ(f == -1.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, -1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 0.0f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(s, &b), S_OK); EXPECT_EQ(b, 0);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK); EXPECT_EQ(f == 0.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, &i), S_OK); EXPECT_EQ(i, 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_independent_variables)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *a = ID3D10EffectVariable_AsScalar(BV(fx, "fA")), *b = ID3D10EffectVariable_AsScalar(BV(fx, "iA")), *c = ID3D10EffectVariable_AsScalar(BV(fx, "bA"));
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(a, 9.0f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_SetInt(b, 11), S_OK);
    float f; int i; BOOL bb;
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(a, &f), S_OK); EXPECT_EQ(f == 9.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(b, &i), S_OK); EXPECT_EQ(i, 11);
    EXPECT_HR(ID3D10EffectScalarVariable_GetBool(c, &bb), S_OK); CHECK(bb != 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_two_effects_do_not_share_values)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10Effect *fy = NULL;
    if (!ue_fx_open(dev, UE_FX_BASIC, &fy)) { ID3D10Effect_Release(fx); return; }
    ID3D10EffectScalarVariable *a = ID3D10EffectVariable_AsScalar(BV(fx, "fA")), *b = ID3D10EffectVariable_AsScalar(BV(fy, "fA"));
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(a, 42.0f), S_OK);
    float f;
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(b, &f), S_OK);
    EXPECT_EQ(f == 1.5f, 1);
    ID3D10Effect_Release(fx);
    ID3D10Effect_Release(fy);
}
TEST(d3d10_utils_effects_fxv_scalar_array)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "arrA"));
    float in[3] = { 1.5f, -2.5f, 3.25f }, out[3] = { 0 };
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloatArray(s, in, 0, 3), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloatArray(s, out, 0, 3), S_OK);
    EXPECT_EQ(memcmp(in, out, sizeof in), 0);
    for (UINT i = 0; i < 3; i++) {
        float e = 0;
        ID3D10EffectScalarVariable *es = ID3D10EffectVariable_AsScalar(ID3D10EffectVariable_GetElement(BV(fx, "arrA"), i));
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(es, &e), S_OK);
        t_check(e == in[i], __FILE__, __LINE__, "element value", "%u: %g", i, e);
    }
    int ii[3] = { 0 };
    EXPECT_HR(ID3D10EffectScalarVariable_GetIntArray(s, ii, 0, 3), S_OK);
    EXPECT_EQ(ii[0], 1); EXPECT_EQ(ii[1], -2); EXPECT_EQ(ii[2], 3);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_array_partial_count)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "arrA"));
    float zero[3] = { 0, 0, 0 }, two[2] = { 7, 8 }, out[3];
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloatArray(s, zero, 0, 3), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloatArray(s, two, 0, 2), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloatArray(s, out, 0, 3), S_OK);
    EXPECT_EQ(out[0] == 7.0f && out[1] == 8.0f && out[2] == 0.0f, 1);
    float sentinel[3] = { 5, 5, 5 };
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloatArray(s, sentinel, 0, 2), S_OK);
    EXPECT_EQ(sentinel[0] == 7.0f && sentinel[1] == 8.0f && sentinel[2] == 5.0f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloatArray(s, two, 0, 0), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloatArray(s, out, 0, 0), S_OK);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_array_offset_argument_is_ignored)
{
    /* observed in the native implementation: the offset argument of the scalar array accessors is not applied */
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(BV(fx, "arrA"));
    float zero[3] = { 0, 0, 0 }, two[2] = { 7, 8 }, out[3];
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloatArray(s, zero, 0, 3), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloatArray(s, two, 1, 2), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloatArray(s, out, 0, 3), S_OK);
    if (out[0] == 7.0f && out[1] == 8.0f && out[2] == 0.0f) t_note("offset ignored (elements 0..1 written)");
    else if (out[0] == 0.0f && out[1] == 7.0f && out[2] == 8.0f) t_note("offset honoured (elements 1..2 written)");
    CHECK(out[0] == 7.0f && out[1] == 8.0f && out[2] == 0.0f);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_scalar_array_element_set)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *arr = BV(fx, "arrA");
    float zero[3] = { 0 }, out[3];
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(arr);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloatArray(s, zero, 0, 3), S_OK);
    ID3D10EffectScalarVariable *e1 = ID3D10EffectVariable_AsScalar(ID3D10EffectVariable_GetElement(arr, 1));
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(e1, 4.5f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloatArray(s, out, 0, 3), S_OK);
    EXPECT_EQ(out[0] == 0.0f && out[1] == 4.5f && out[2] == 0.0f, 1);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_get_element_bounds)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *arr = BV(fx, "arrA");
    for (UINT i = 0; i < 3; i++) { ID3D10EffectVariable *e = ID3D10EffectVariable_GetElement(arr, i); CHECK(e && ID3D10EffectVariable_IsValid(e)); }
    ID3D10EffectVariable *e = ID3D10EffectVariable_GetElement(arr, 3);
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    e = ID3D10EffectVariable_GetElement(arr, 0xffffffffu);
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    /* non-array variables have no elements */
    e = ID3D10EffectVariable_GetElement(BV(fx, "fA"), 0);
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    e = ID3D10EffectVariable_GetElement(BV(fx, "vecA"), 0);
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    ID3D10EffectVariable *ve = ID3D10EffectVariable_GetElement(BV(fx, "vArr"), 1);
    CHECK(ve && ID3D10EffectVariable_IsValid(ve));
    CHECK(ID3D10EffectVectorVariable_IsValid(ID3D10EffectVariable_AsVector(ve)));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_element_type_is_element_type)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *e = ID3D10EffectVariable_GetElement(BV(fx, "arrA"), 2);
    D3D10_EFFECT_TYPE_DESC d;
    EXPECT_HR(ID3D10EffectType_GetDesc(ID3D10EffectVariable_GetType(e), &d), S_OK);
    EXPECT_EQ(d.Elements, 0);
    EXPECT_EQ(d.Class, D3D_SVC_SCALAR);
    EXPECT_EQ(d.PackedSize, 4);
    D3D10_EFFECT_VARIABLE_DESC vd;
    EXPECT_HR(ID3D10EffectVariable_GetDesc(e, &vd), S_OK);
    EXPECT_EQ(vd.BufferOffset, 96 + 2 * 16);
    ID3D10Effect_Release(fx);
}

/* ---- vector ---- */
TEST(d3d10_utils_effects_fxv_vector_defaults_zero)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    float v[4] = { 9, 9, 9, 9 };
    EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVector(ID3D10EffectVariable_AsVector(BV(fx, "vecA")), v), S_OK);
    EXPECT_EQ(v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0, 1);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_vector_float4_roundtrip)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVectorVariable *vv = ID3D10EffectVariable_AsVector(BV(fx, "vecA"));
    float in[4] = { 1.25f, -2.5f, 1e10f, 0.0f }, out[4] = { 0 };
    EXPECT_HR(ID3D10EffectVectorVariable_SetFloatVector(vv, in), S_OK);
    EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVector(vv, out), S_OK);
    EXPECT_EQ(memcmp(in, out, 16), 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_vector_float3_touches_three_components)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVectorVariable *vv = ID3D10EffectVariable_AsVector(BV(fx, "v3"));
    float in[4] = { 1, 2, 3, 4 }, out[4] = { -1, -1, -1, -1 };
    EXPECT_HR(ID3D10EffectVectorVariable_SetFloatVector(vv, in), S_OK);
    EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVector(vv, out), S_OK);
    EXPECT_EQ(out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == -1, 1);
    /* the neighbouring variable in the same register (arrA tail occupies 128..131) is not clobbered */
    float tail = -9;
    ID3D10EffectScalarVariable *e2 = ID3D10EffectVariable_AsScalar(ID3D10EffectVariable_GetElement(BV(fx, "arrA"), 2));
    ID3D10EffectScalarVariable_SetFloat(e2, 55.0f);
    EXPECT_HR(ID3D10EffectVectorVariable_SetFloatVector(vv, in), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(e2, &tail), S_OK);
    EXPECT_EQ(tail == 55.0f, 1);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_vector_int2_conversions)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVectorVariable *vv = ID3D10EffectVariable_AsVector(BV(fx, "iv2"));
    int in[4] = { 7, -8, 9, 10 }, out[4] = { 5, 5, 5, 5 };
    EXPECT_HR(ID3D10EffectVectorVariable_SetIntVector(vv, in), S_OK);
    EXPECT_HR(ID3D10EffectVectorVariable_GetIntVector(vv, out), S_OK);
    EXPECT_EQ(out[0] == 7 && out[1] == -8 && out[2] == 5 && out[3] == 5, 1);
    float fin[4] = { 1.9f, -2.9f, 0, 0 }, fout[4] = { -1, -1, -1, -1 };
    EXPECT_HR(ID3D10EffectVectorVariable_SetFloatVector(vv, fin), S_OK);
    EXPECT_HR(ID3D10EffectVectorVariable_GetIntVector(vv, out), S_OK);
    EXPECT_EQ(out[0], 1); EXPECT_EQ(out[1], -2);
    EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVector(vv, fout), S_OK);
    EXPECT_EQ(fout[0] == 1.0f && fout[1] == -2.0f && fout[2] == -1.0f, 1);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_vector_bool_vector)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVectorVariable *vv = ID3D10EffectVariable_AsVector(BV(fx, "vecA"));
    BOOL in[4] = { TRUE, FALSE, 3, 0 }, out[4] = { 5, 5, 5, 5 };
    HRESULT hr = ID3D10EffectVectorVariable_SetBoolVector(vv, in);
    EXPECT_HR(hr, S_OK);
    hr = ID3D10EffectVectorVariable_GetBoolVector(vv, out);
    EXPECT_HR(hr, S_OK);
    CHECK(out[0] != 0 && out[1] == 0 && out[2] != 0 && out[3] == 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_vector_array)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVectorVariable *vv = ID3D10EffectVariable_AsVector(BV(fx, "vArr"));
    float in[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, out[8] = { 0 };
    EXPECT_HR(ID3D10EffectVectorVariable_SetFloatVectorArray(vv, in, 0, 2), S_OK);
    EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVectorArray(vv, out, 0, 2), S_OK);
    EXPECT_EQ(memcmp(in, out, sizeof in), 0);
    float e1[4] = { 0 };
    ID3D10EffectVectorVariable *ev = ID3D10EffectVariable_AsVector(ID3D10EffectVariable_GetElement(BV(fx, "vArr"), 1));
    EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVector(ev, e1), S_OK);
    EXPECT_EQ(e1[0] == 5 && e1[1] == 6 && e1[2] == 7 && e1[3] == 8, 1);
    int iv[8] = { 0 };
    EXPECT_HR(ID3D10EffectVectorVariable_GetIntVectorArray(vv, iv, 0, 2), S_OK);
    EXPECT_EQ(iv[0], 1); EXPECT_EQ(iv[7], 8);
    ID3D10Effect_Release(fx);
}

/* ---- matrix ---- */
static void mat_fill(float *m) { for (int i = 0; i < 16; i++) m[i] = (float)(i + 1); }
TEST(d3d10_utils_effects_fxv_matrix_roundtrip)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectMatrixVariable *mv = ID3D10EffectVariable_AsMatrix(BV(fx, "mA"));
    float in[16], out[16] = { 0 };
    mat_fill(in);
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrix(mv, in), S_OK);
    EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrix(mv, out), S_OK);
    EXPECT_EQ(memcmp(in, out, sizeof in), 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_matrix_transpose)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectMatrixVariable *mv = ID3D10EffectVariable_AsMatrix(BV(fx, "mA"));
    float in[16], out[16] = { 0 }, tr[16];
    mat_fill(in);
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) tr[c * 4 + r] = in[r * 4 + c];
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrix(mv, in), S_OK);
    EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrixTranspose(mv, out), S_OK);
    EXPECT_EQ(memcmp(tr, out, sizeof tr), 0);
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrixTranspose(mv, in), S_OK);
    EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrix(mv, out), S_OK);
    EXPECT_EQ(memcmp(tr, out, sizeof tr), 0);
    EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrixTranspose(mv, out), S_OK);
    EXPECT_EQ(memcmp(in, out, sizeof in), 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_matrix_default_zero)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    float out[16];
    memset(out, 0x7f, sizeof out);
    EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrix(ID3D10EffectVariable_AsMatrix(BV(fx, "mA")), out), S_OK);
    int z = 1;
    for (int i = 0; i < 16; i++) if (out[i] != 0.0f) z = 0;
    CHECK(z);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_matrix_does_not_touch_neighbours)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectScalarVariable *c = ID3D10EffectVariable_AsScalar(BV(fx, "iA"));
    EXPECT_HR(ID3D10EffectScalarVariable_SetInt(c, 1234), S_OK);
    float m[16];
    mat_fill(m);
    EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrix(ID3D10EffectVariable_AsMatrix(BV(fx, "mA")), m), S_OK);
    int i = 0;
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(c, &i), S_OK);
    EXPECT_EQ(i, 1234);
    ID3D10Effect_Release(fx);
}

/* ---- raw value ---- */
TEST(d3d10_utils_effects_fxv_raw_value_roundtrip)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *v = BV(fx, "vecA");
    float in[4] = { 3, 4, 5, 6 }, out[4] = { 0 };
    EXPECT_HR(ID3D10EffectVariable_SetRawValue(v, in, 0, 16), S_OK);
    EXPECT_HR(ID3D10EffectVariable_GetRawValue(v, out, 0, 16), S_OK);
    EXPECT_EQ(memcmp(in, out, 16), 0);
    float g[4] = { 0 };
    EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVector(ID3D10EffectVariable_AsVector(v), g), S_OK);
    EXPECT_EQ(memcmp(in, g, 16), 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_raw_value_partial)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *v = BV(fx, "vecA");
    float base[4] = { 1, 1, 1, 1 }, patch = 9.0f, out[4];
    EXPECT_HR(ID3D10EffectVariable_SetRawValue(v, base, 0, 16), S_OK);
    EXPECT_HR(ID3D10EffectVariable_SetRawValue(v, &patch, 8, 4), S_OK);
    EXPECT_HR(ID3D10EffectVariable_GetRawValue(v, out, 0, 16), S_OK);
    EXPECT_EQ(out[0] == 1 && out[1] == 1 && out[2] == 9 && out[3] == 1, 1);
    float one = 0;
    EXPECT_HR(ID3D10EffectVariable_GetRawValue(v, &one, 8, 4), S_OK);
    EXPECT_EQ(one == 9.0f, 1);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_raw_value_matches_scalar_accessors)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *v = BV(fx, "iA");
    int x = 424242, y = 0;
    EXPECT_HR(ID3D10EffectVariable_SetRawValue(v, &x, 0, 4), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetInt(ID3D10EffectVariable_AsScalar(v), &y), S_OK);
    EXPECT_EQ(y, 424242);
    ID3D10Effect_Release(fx);
}

/* ---- string ---- */
TEST(d3d10_utils_effects_fxv_string_values)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_STRING);
    ID3D10EffectStringVariable *sv = ID3D10EffectVariable_AsString(BV(fx, "title"));
    LPCSTR s = NULL;
    EXPECT_HR(ID3D10EffectStringVariable_GetString(sv, &s), S_OK);
    CHECK(s != NULL);
    if (s) EXPECT_STR(s, "hello");
    ID3D10EffectStringVariable *na = ID3D10EffectVariable_AsString(BV(fx, "names"));
    LPCSTR arr[2] = { NULL, NULL };
    EXPECT_HR(ID3D10EffectStringVariable_GetStringArray(na, arr, 0, 2), S_OK);
    if (arr[0] && arr[1]) { EXPECT_STR(arr[0], "one"); EXPECT_STR(arr[1], "two"); } else CHECK(0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_string_element)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_STRING);
    ID3D10EffectVariable *e = ID3D10EffectVariable_GetElement(BV(fx, "names"), 1);
    LPCSTR s = NULL;
    CHECK(e && ID3D10EffectVariable_IsValid(e));
    EXPECT_HR(ID3D10EffectStringVariable_GetString(ID3D10EffectVariable_AsString(e), &s), S_OK);
    if (s) EXPECT_STR(s, "two"); else CHECK(0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_string_type_desc)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_STRING);
    D3D10_EFFECT_TYPE_DESC d;
    EXPECT_HR(ID3D10EffectType_GetDesc(ID3D10EffectVariable_GetType(BV(fx, "title")), &d), S_OK);
    EXPECT_EQ(d.Class, D3D_SVC_OBJECT);
    EXPECT_EQ(d.Type, D3D_SVT_STRING);
    t_check(d.TypeName && !strcmp(d.TypeName, "String"), __FILE__, __LINE__, "TypeName (Win10: capital S)", "%s", d.TypeName ? d.TypeName : "(null)");
    ID3D10Effect_Release(fx);
}

/* ---- object type descriptors ---- */
TEST(d3d10_utils_effects_fxv_object_type_descs)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    static const struct { const char *var, *type; D3D_SHADER_VARIABLE_TYPE ty; } rows[] = {
        { "tex", "Texture2D", D3D_SVT_TEXTURE2D }, { "texarr", "Texture2DArray", D3D_SVT_TEXTURE2DARRAY }, { "tex3", "Texture3D", D3D_SVT_TEXTURE3D },
        { "smp", "SamplerState", D3D_SVT_SAMPLER },
    };
    for (unsigned i = 0; i < sizeof rows / sizeof *rows; i++) {
        D3D10_EFFECT_TYPE_DESC d;
        if (FAILED(ID3D10EffectType_GetDesc(ID3D10EffectVariable_GetType(BV(fx, rows[i].var)), &d))) { t_check(0, __FILE__, __LINE__, "type desc", "%s", rows[i].var); continue; }
        t_check(d.Class == D3D_SVC_OBJECT, __FILE__, __LINE__, "Class OBJECT", "%s: %d", rows[i].var, d.Class);
        t_check(d.Type == rows[i].ty, __FILE__, __LINE__, "Type", "%s: %d want %d", rows[i].var, d.Type, rows[i].ty);
        t_check(d.TypeName && !strcmp(d.TypeName, rows[i].type), __FILE__, __LINE__, "TypeName", "%s: %s", rows[i].var, d.TypeName ? d.TypeName : "(null)");
        EXPECT_EQ(d.Elements, 0);
        EXPECT_EQ(d.Members, 0);
    }
    static const char *const tn[] = { "bs", "rs", "dss", "rtv", "dsv" };
    static const char *const tt[] = { "BlendState", "RasterizerState", "DepthStencilState", "RenderTargetView", "DepthStencilView" };
    for (int i = 0; i < 5; i++) {
        D3D10_EFFECT_TYPE_DESC d;
        if (FAILED(ID3D10EffectType_GetDesc(ID3D10EffectVariable_GetType(BV(fx, tn[i])), &d))) { CHECK(0); continue; }
        t_check(d.TypeName && !strcmp(d.TypeName, tt[i]), __FILE__, __LINE__, "TypeName", "%s: %s", tn[i], d.TypeName ? d.TypeName : "(null)");
        EXPECT_EQ(d.Class, D3D_SVC_OBJECT);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_global_initializers)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    float c[4] = { 0 }, s = 0;
    EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVector(ID3D10EffectVariable_AsVector(BV(fx, "gcol")), c), S_OK);
    EXPECT_EQ(c[0] == 1 && c[1] == 2 && c[2] == 3 && c[3] == 4, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(BV(fx, "gscalar")), &s), S_OK);
    EXPECT_EQ(s == 2.5f, 1);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxv_globals_outside_cbuffer_have_parent_cbuffer)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10EffectConstantBuffer *p = ID3D10EffectVariable_GetParentConstantBuffer(BV(fx, "gcol"));
    CHECK(p && ID3D10EffectConstantBuffer_IsValid(p));
    D3D10_EFFECT_VARIABLE_DESC d;
    if (p && SUCCEEDED(ID3D10EffectConstantBuffer_GetDesc(p, &d))) t_check(d.Name && !strcmp(d.Name, "$Globals"), __FILE__, __LINE__, "implicit cbuffer name", "%s", d.Name ? d.Name : "(null)");
    if (p && SUCCEEDED(ID3D10EffectConstantBuffer_GetDesc(p, &d)) && d.Name) EXPECT_STR(d.Name, "$Globals");
    ID3D10Effect_Release(fx);
}
