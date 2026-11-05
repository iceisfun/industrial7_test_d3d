/* d3dcompiler_47 D3DReflect: constant buffers, variables, types, default values, out-of-range handling. */
#include "rb.h"

static ID3D11ShaderReflectionConstantBuffer *rb_cb(ID3D11ShaderReflection *r, UINT i) { return r->lpVtbl->GetConstantBufferByIndex(r, i); }
static int rb_cb_desc(ID3D11ShaderReflectionConstantBuffer *c, D3D11_SHADER_BUFFER_DESC *d)
{
    HRESULT hr; memset(d, 0, sizeof *d); hr = c->lpVtbl->GetDesc(c, d);
    t_check(SUCCEEDED(hr), __FILE__, __LINE__, "cbuffer desc", "%s", d3dh_hr_name(hr));
    return SUCCEEDED(hr);
}
static int rb_var_in_cb(ID3D11ShaderReflectionConstantBuffer *c, const char *name, D3D11_SHADER_VARIABLE_DESC *d)
{
    ID3D11ShaderReflectionVariable *v = c->lpVtbl->GetVariableByName(c, name); HRESULT hr; memset(d, 0, sizeof *d);
    hr = v->lpVtbl->GetDesc(v, d);
    t_check(SUCCEEDED(hr), __FILE__, __LINE__, "variable desc in cbuffer", "%s: %s", name, d3dh_hr_name(hr));
    return SUCCEEDED(hr);
}

#define PACK_SRC \
    "cbuffer CB : register(b0) { float4 a; float3 b; float c; float2 d; float4x4 m; int4 i; };\n" \
    "float4 main() : SV_Target { return a + float4(b, c) + float4(d, 0, 0) + mul(m, a) + i; }\n"

TEST(rb_cb_desc_counts_and_name)
{
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    D3D11_SHADER_DESC d; D3D11_SHADER_BUFFER_DESC bd; memset(&d, 0, sizeof d);
    EXPECT_HR(r->lpVtbl->GetDesc(r, &d), S_OK);
    EXPECT_EQ(d.ConstantBuffers, 1);
    if (rb_cb_desc(rb_cb(r, 0), &bd)) {
        EXPECT_STR(bd.Name, "CB"); EXPECT_EQ(bd.Type, D3D_CT_CBUFFER); EXPECT_EQ(bd.Variables, 6); EXPECT_EQ(bd.Size, 128); EXPECT_EQ(bd.uFlags, 0);
    }
    RB_DONE(r);
}
TEST(rb_cb_packing_offsets_and_sizes)
{
    static const struct { const char *n; UINT off, size; } want[] = { { "a", 0, 16 }, { "b", 16, 12 }, { "c", 28, 4 }, { "d", 32, 8 }, { "m", 48, 64 }, { "i", 112, 16 } };
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    ID3D11ShaderReflectionConstantBuffer *c = rb_cb(r, 0);
    for (size_t k = 0; k < sizeof want / sizeof *want; k++) {
        D3D11_SHADER_VARIABLE_DESC v;
        if (!rb_var_in_cb(c, want[k].n, &v)) continue;
        t_check(!strcmp(v.Name, want[k].n), __FILE__, __LINE__, "name", "%s", v.Name);
        t_check(v.StartOffset == want[k].off, __FILE__, __LINE__, "offset", "%s got %u want %u", want[k].n, v.StartOffset, want[k].off);
        t_check(v.Size == want[k].size, __FILE__, __LINE__, "size", "%s got %u want %u", want[k].n, v.Size, want[k].size);
        CHECK(v.uFlags & D3D_SVF_USED);
        CHECK(v.DefaultValue == NULL);
        EXPECT_EQ(v.StartTexture, 0xffffffffu); EXPECT_EQ(v.TextureSize, 0); EXPECT_EQ(v.StartSampler, 0xffffffffu); EXPECT_EQ(v.SamplerSize, 0);
    }
    RB_DONE(r);
}
TEST(rb_cb_variable_by_index_order_is_declaration_order)
{
    static const char *const names[] = { "a", "b", "c", "d", "m", "i" };
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    ID3D11ShaderReflectionConstantBuffer *c = rb_cb(r, 0);
    for (UINT k = 0; k < 6; k++) {
        D3D11_SHADER_VARIABLE_DESC v; ID3D11ShaderReflectionVariable *var = c->lpVtbl->GetVariableByIndex(c, k); memset(&v, 0, sizeof v);
        EXPECT_HR(var->lpVtbl->GetDesc(var, &v), S_OK);
        if (v.Name) t_check(!strcmp(v.Name, names[k]), __FILE__, __LINE__, "variable order", "index %u got %s want %s", k, v.Name, names[k]);
    }
    RB_DONE(r);
}
TEST(rb_cb_scalar_vector_matrix_type_descs)
{
    static const struct { const char *n; D3D_SHADER_VARIABLE_CLASS cls; D3D_SHADER_VARIABLE_TYPE ty; UINT rows, cols; const char *tn; } want[] = {
        { "a", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 1, 4, "float4" }, { "b", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 1, 3, "float3" }, { "c", D3D_SVC_SCALAR, D3D_SVT_FLOAT, 1, 1, "float" },
        { "d", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 1, 2, "float2" }, { "m", D3D_SVC_MATRIX_COLUMNS, D3D_SVT_FLOAT, 4, 4, "float4x4" }, { "i", D3D_SVC_VECTOR, D3D_SVT_INT, 1, 4, "int4" },
    };
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    for (size_t k = 0; k < sizeof want / sizeof *want; k++) {
        D3D11_SHADER_TYPE_DESC t;
        if (!rb_type_desc(r, want[k].n, &t)) continue;
        t_check(t.Class == want[k].cls, __FILE__, __LINE__, "class", "%s got %d want %d", want[k].n, t.Class, want[k].cls);
        t_check(t.Type == want[k].ty, __FILE__, __LINE__, "type", "%s got %d want %d", want[k].n, t.Type, want[k].ty);
        t_check(t.Rows == want[k].rows && t.Columns == want[k].cols, __FILE__, __LINE__, "dims", "%s got %ux%u", want[k].n, t.Rows, t.Columns);
        t_check(t.Elements == 0 && t.Members == 0 && t.Offset == 0, __FILE__, __LINE__, "elements/members/offset", "%s: %u %u %u", want[k].n, t.Elements, t.Members, t.Offset);
        t_check(t.Name && !strcmp(t.Name, want[k].tn), __FILE__, __LINE__, "type name", "%s got %s want %s", want[k].n, t.Name ? t.Name : "(null)", want[k].tn);
    }
    RB_DONE(r);
}

/* ---- scalar type table: one cbuffer per base type ------------------------------------------------ */
#define TYPE_CASE(nm, hl, cls_, ty_, rows_, cols_, tn_, size_) \
TEST(rb_cb_type_##nm) { \
    RB_REFLECT(r, "cbuffer CB { float4 pad; " hl " v; };\nfloat4 main() : SV_Target { return pad + (float4)v.xxxx; }\n", "ps_5_0"); \
    (void)0; RB_DONE(r); }
/* The generic form above cannot swizzle every type; use per-type expressions instead. */
#undef TYPE_CASE
#define TYPE_CASE(nm, decl, use, cls_, ty_, rows_, cols_, tn_, size_) \
TEST(rb_cb_type_##nm) { \
    RB_REFLECT(r, "cbuffer CB { " decl " };\nfloat4 main() : SV_Target { return " use "; }\n", "ps_5_0"); \
    D3D11_SHADER_TYPE_DESC t; D3D11_SHADER_VARIABLE_DESC v; \
    if (rb_type_desc(r, "v", &t)) { \
        EXPECT_EQ(t.Class, cls_); EXPECT_EQ(t.Type, ty_); EXPECT_EQ(t.Rows, rows_); EXPECT_EQ(t.Columns, cols_); EXPECT_STR(t.Name ? t.Name : "", tn_); \
    } \
    if (rb_var_desc(r, "v", &v)) { EXPECT_EQ(v.Size, size_); EXPECT_EQ(v.StartOffset, 0); } \
    RB_DONE(r); }
TYPE_CASE(float1, "float v;", "v", D3D_SVC_SCALAR, D3D_SVT_FLOAT, 1, 1, "float", 4)
TYPE_CASE(float2, "float2 v;", "float4(v, 0, 0)", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 1, 2, "float2", 8)
TYPE_CASE(float3, "float3 v;", "float4(v, 0)", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 1, 3, "float3", 12)
TYPE_CASE(float4, "float4 v;", "v", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 1, 4, "float4", 16)
TYPE_CASE(int1, "int v;", "(float)v", D3D_SVC_SCALAR, D3D_SVT_INT, 1, 1, "int", 4)
TYPE_CASE(int3, "int3 v;", "float4(v, 0)", D3D_SVC_VECTOR, D3D_SVT_INT, 1, 3, "int3", 12)
TYPE_CASE(uint1, "uint v;", "(float)v", D3D_SVC_SCALAR, D3D_SVT_UINT, 1, 1, "uint", 4)
TYPE_CASE(uint2, "uint2 v;", "float4(v, 0, 0)", D3D_SVC_VECTOR, D3D_SVT_UINT, 1, 2, "uint2", 8)
TYPE_CASE(uint4, "uint4 v;", "v", D3D_SVC_VECTOR, D3D_SVT_UINT, 1, 4, "uint4", 16)
TYPE_CASE(bool1, "bool v;", "v ? 1.0 : 0.0", D3D_SVC_SCALAR, D3D_SVT_BOOL, 1, 1, "bool", 4)
TYPE_CASE(bool2, "bool2 v;", "float4(v, 0, 0)", D3D_SVC_VECTOR, D3D_SVT_BOOL, 1, 2, "bool2", 8)
TYPE_CASE(double1, "double v;", "(float)v", D3D_SVC_SCALAR, D3D_SVT_DOUBLE, 1, 1, "double", 8)
TYPE_CASE(double2, "double2 v;", "float4((float2)v, 0, 0)", D3D_SVC_VECTOR, D3D_SVT_DOUBLE, 1, 2, "double2", 16)
TYPE_CASE(float2x2, "float2x2 v;", "float4(v[0], v[1])", D3D_SVC_MATRIX_COLUMNS, D3D_SVT_FLOAT, 2, 2, "float2x2", 24)
TYPE_CASE(float3x3, "float3x3 v;", "float4(v[0] + v[1] + v[2], 0)", D3D_SVC_MATRIX_COLUMNS, D3D_SVT_FLOAT, 3, 3, "float3x3", 44)
TYPE_CASE(float4x4, "float4x4 v;", "v[0] + v[1] + v[2] + v[3]", D3D_SVC_MATRIX_COLUMNS, D3D_SVT_FLOAT, 4, 4, "float4x4", 64)
TYPE_CASE(row_major_float3x3, "row_major float3x3 v;", "float4(v[0] + v[1] + v[2], 0)", D3D_SVC_MATRIX_ROWS, D3D_SVT_FLOAT, 3, 3, "float3x3", 44)
TYPE_CASE(row_major_float4x4, "row_major float4x4 v;", "v[0] + v[1] + v[2] + v[3]", D3D_SVC_MATRIX_ROWS, D3D_SVT_FLOAT, 4, 4, "float4x4", 64)
TYPE_CASE(row_major_float3x4, "row_major float3x4 v;", "v[0] + v[1] + v[2]", D3D_SVC_MATRIX_ROWS, D3D_SVT_FLOAT, 3, 4, "float3x4", 48)
TYPE_CASE(column_major_float3x4, "column_major float3x4 v;", "float4(v[0] + v[1] + v[2], 0)", D3D_SVC_MATRIX_COLUMNS, D3D_SVT_FLOAT, 3, 4, "float3x4", 60)
TYPE_CASE(row_major_float4x3, "row_major float4x3 v;", "float4(v[0] + v[1] + v[2] + v[3], 0)", D3D_SVC_MATRIX_ROWS, D3D_SVT_FLOAT, 4, 3, "float4x3", 60)
TYPE_CASE(int2x2, "int2x2 v;", "float4(v[0], v[1])", D3D_SVC_MATRIX_COLUMNS, D3D_SVT_INT, 2, 2, "int2x2", 24)
TYPE_CASE(uint3x2, "uint3x2 v;", "float4(v[0] + v[1] + v[2], 0, 0)", D3D_SVC_MATRIX_COLUMNS, D3D_SVT_UINT, 3, 2, "uint3x2", 28)

TEST(rb_cb_pack_matrix_compile_flag_changes_class)
{
    static const char *src = "cbuffer CB { float4x4 v; };\nfloat4 main() : SV_Target { return v[0] + v[3]; }\n";
    D3D11_SHADER_TYPE_DESC t;
    { RB_COMPILE_F(b, src, "ps_5_0", D3DCOMPILE_PACK_MATRIX_ROW_MAJOR); ID3D11ShaderReflection *r = rb_reflect(b);
      if (r) { if (rb_type_desc(r, "v", &t)) EXPECT_EQ(t.Class, D3D_SVC_MATRIX_ROWS); r->lpVtbl->Release(r); } ID3D10Blob_Release(b); }
    { RB_COMPILE_F(b, src, "ps_5_0", D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR); ID3D11ShaderReflection *r = rb_reflect(b);
      if (r) { if (rb_type_desc(r, "v", &t)) EXPECT_EQ(t.Class, D3D_SVC_MATRIX_COLUMNS); r->lpVtbl->Release(r); } ID3D10Blob_Release(b); }
}

/* ---- arrays --------------------------------------------------------------------------------------- */
#define ARR_SRC "cbuffer CB { float4 arr[3]; float sc[3]; float3 v3[2]; float4x4 ma[2]; float tail; };\n" \
    "float4 main(uint i : TEXCOORD0) : SV_Target { return arr[i] + sc[i] + float4(v3[i], 0) + ma[i][0] + tail; }\n"
TEST(rb_cb_array_layout)
{
    static const struct { const char *n; UINT off, size, elems; } want[] = { { "arr", 0, 48, 3 }, { "sc", 48, 36, 3 }, { "v3", 96, 28, 2 }, { "ma", 128, 128, 2 }, { "tail", 256, 4, 0 } };
    RB_REFLECT(r, ARR_SRC, "ps_5_0");
    for (size_t k = 0; k < sizeof want / sizeof *want; k++) {
        D3D11_SHADER_VARIABLE_DESC v; D3D11_SHADER_TYPE_DESC t;
        if (rb_var_desc(r, want[k].n, &v)) {
            t_check(v.StartOffset == want[k].off, __FILE__, __LINE__, "array offset", "%s got %u want %u", want[k].n, v.StartOffset, want[k].off);
            t_check(v.Size == want[k].size, __FILE__, __LINE__, "array size", "%s got %u want %u", want[k].n, v.Size, want[k].size);
        }
        if (rb_type_desc(r, want[k].n, &t)) t_check(t.Elements == want[k].elems, __FILE__, __LINE__, "elements", "%s got %u want %u", want[k].n, t.Elements, want[k].elems);
    }
    { D3D11_SHADER_BUFFER_DESC bd; if (rb_cb_desc(rb_cb(r, 0), &bd)) { EXPECT_EQ(bd.Variables, 5); EXPECT_EQ(bd.Size, 272); } }
    RB_DONE(r);
}
TEST(rb_cb_array_element_type_description)
{
    RB_REFLECT(r, ARR_SRC, "ps_5_0");
    D3D11_SHADER_TYPE_DESC t;
    if (rb_type_desc(r, "arr", &t)) { EXPECT_EQ(t.Class, D3D_SVC_VECTOR); EXPECT_EQ(t.Type, D3D_SVT_FLOAT); EXPECT_EQ(t.Columns, 4); EXPECT_EQ(t.Elements, 3); }
    if (rb_type_desc(r, "sc", &t)) { EXPECT_EQ(t.Class, D3D_SVC_SCALAR); EXPECT_EQ(t.Elements, 3); }
    if (rb_type_desc(r, "ma", &t)) { EXPECT_EQ(t.Class, D3D_SVC_MATRIX_COLUMNS); EXPECT_EQ(t.Rows, 4); EXPECT_EQ(t.Columns, 4); EXPECT_EQ(t.Elements, 2); }
    RB_DONE(r);
}

/* ---- structs ---------------------------------------------------------------------------------------- */
#define STRUCT_SRC "struct Inner { float2 p; float q; };\nstruct S { float3 a; float b; int2 c; float4 d; Inner in1; };\n" \
    "cbuffer CB { S s; float after; S arr[2]; };\n" \
    "float4 main() : SV_Target { return float4(s.a, s.b) + float4(s.c, 0, 0) + s.d + float4(s.in1.p, s.in1.q, after) + arr[1].d + arr[0].d; }\n"
TEST(rb_cb_struct_type_members)
{
    RB_REFLECT(r, STRUCT_SRC, "ps_5_0");
    D3D11_SHADER_TYPE_DESC t, m; ID3D11ShaderReflectionVariable *v = rb_var(r, "s"); ID3D11ShaderReflectionType *ty = v->lpVtbl->GetType(v);
    memset(&t, 0, sizeof t);
    EXPECT_HR(ty->lpVtbl->GetDesc(ty, &t), S_OK);
    EXPECT_EQ(t.Class, D3D_SVC_STRUCT); EXPECT_EQ(t.Type, D3D_SVT_VOID); EXPECT_EQ(t.Members, 5); EXPECT_EQ(t.Elements, 0);
    EXPECT_STR(t.Name ? t.Name : "", "S");
    {
        static const struct { const char *n; D3D_SHADER_VARIABLE_CLASS cls; D3D_SHADER_VARIABLE_TYPE ty; UINT cols, off; } mem[] = {
            { "a", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 3, 0 }, { "b", D3D_SVC_SCALAR, D3D_SVT_FLOAT, 1, 12 }, { "c", D3D_SVC_VECTOR, D3D_SVT_INT, 2, 16 },
            { "d", D3D_SVC_VECTOR, D3D_SVT_FLOAT, 4, 32 }, { "in1", D3D_SVC_STRUCT, D3D_SVT_VOID, 0, 48 } };
        for (UINT k = 0; k < 5; k++) {
            ID3D11ShaderReflectionType *mt = ty->lpVtbl->GetMemberTypeByIndex(ty, k), *mn = ty->lpVtbl->GetMemberTypeByName(ty, mem[k].n);
            const char *nm = ty->lpVtbl->GetMemberTypeName(ty, k);
            memset(&m, 0, sizeof m);
            EXPECT_HR(mt->lpVtbl->GetDesc(mt, &m), S_OK);
            t_check(nm && !strcmp(nm, mem[k].n), __FILE__, __LINE__, "member name", "index %u got %s want %s", k, nm ? nm : "(null)", mem[k].n);
            t_check(mt == mn, __FILE__, __LINE__, "by index == by name", "%s", mem[k].n);
            t_check(m.Class == mem[k].cls && m.Type == mem[k].ty, __FILE__, __LINE__, "member class/type", "%s: %d %d", mem[k].n, m.Class, m.Type);
            if (mem[k].cols) EXPECT_EQ(m.Columns, mem[k].cols);
            t_check(m.Offset == mem[k].off, __FILE__, __LINE__, "member offset", "%s got %u want %u", mem[k].n, m.Offset, mem[k].off);
        }
    }
    RB_DONE(r);
}
TEST(rb_cb_struct_nested_members)
{
    RB_REFLECT(r, STRUCT_SRC, "ps_5_0");
    ID3D11ShaderReflectionVariable *v = rb_var(r, "s"); ID3D11ShaderReflectionType *ty = v->lpVtbl->GetType(v), *in1 = ty->lpVtbl->GetMemberTypeByName(ty, "in1");
    D3D11_SHADER_TYPE_DESC t; memset(&t, 0, sizeof t);
    EXPECT_HR(in1->lpVtbl->GetDesc(in1, &t), S_OK);
    EXPECT_EQ(t.Members, 2); EXPECT_STR(t.Name ? t.Name : "", "Inner");
    { const char *n0 = in1->lpVtbl->GetMemberTypeName(in1, 0), *n1 = in1->lpVtbl->GetMemberTypeName(in1, 1); CHECK(n0 && !strcmp(n0, "p")); CHECK(n1 && !strcmp(n1, "q")); }
    RB_DONE(r);
}
TEST(rb_cb_struct_variable_layout)
{
    RB_REFLECT(r, STRUCT_SRC, "ps_5_0");
    D3D11_SHADER_VARIABLE_DESC v;
    /* a struct starts on a register boundary, its size ends with its last member, and whatever follows starts a new register;
     * array elements of struct type are register aligned except for the size of the final element */
    if (rb_var_desc(r, "s", &v)) { EXPECT_EQ(v.StartOffset, 0); EXPECT_EQ(v.Size, 60); }
    if (rb_var_desc(r, "after", &v)) EXPECT_EQ(v.StartOffset, 64);
    if (rb_var_desc(r, "arr", &v)) { EXPECT_EQ(v.StartOffset, 80); EXPECT_EQ(v.Size, 124); }
    { D3D11_SHADER_TYPE_DESC t; if (rb_type_desc(r, "arr", &t)) { EXPECT_EQ(t.Elements, 2); EXPECT_EQ(t.Class, D3D_SVC_STRUCT); EXPECT_EQ(t.Members, 5); } }
    RB_DONE(r);
}
TEST(rb_cb_member_type_lookups_out_of_range)
{
    RB_REFLECT(r, STRUCT_SRC, "ps_5_0");
    ID3D11ShaderReflectionVariable *v = rb_var(r, "s"); ID3D11ShaderReflectionType *ty = v->lpVtbl->GetType(v), *bad1, *bad2, *bad3; D3D11_SHADER_TYPE_DESC t;
    bad1 = ty->lpVtbl->GetMemberTypeByIndex(ty, 5); bad2 = ty->lpVtbl->GetMemberTypeByIndex(ty, 1000); bad3 = ty->lpVtbl->GetMemberTypeByName(ty, "nope");
    CHECK(bad1 != NULL && bad1 == bad2 && bad2 == bad3);
    CHECK(bad1 != ty->lpVtbl->GetMemberTypeByIndex(ty, 0));
    EXPECT_HR(bad1->lpVtbl->GetDesc(bad1, &t), RB_E_FAIL);
    CHECK(ty->lpVtbl->GetMemberTypeByName(ty, NULL) == bad1);
    CHECK(ty->lpVtbl->GetMemberTypeName(ty, 5) == NULL); CHECK(ty->lpVtbl->GetMemberTypeName(ty, 0xffffffffu) == NULL);
    EXPECT_HR(ty->lpVtbl->GetDesc(ty, NULL), RB_E_FAIL);
    /* name lookup is case sensitive */
    CHECK(ty->lpVtbl->GetMemberTypeByName(ty, "A") == bad1);
    RB_DONE(r);
}
TEST(rb_cb_scalar_type_has_no_members)
{
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    ID3D11ShaderReflectionVariable *v = rb_var(r, "a"); ID3D11ShaderReflectionType *ty = v->lpVtbl->GetType(v), *inv;
    CHECK(ty->lpVtbl->GetMemberTypeName(ty, 0) == NULL);
    inv = ty->lpVtbl->GetMemberTypeByIndex(ty, 0);
    { D3D11_SHADER_TYPE_DESC t; EXPECT_HR(inv->lpVtbl->GetDesc(inv, &t), RB_E_FAIL); }
    EXPECT_EQ(ty->lpVtbl->GetNumInterfaces(ty), 0);
    CHECK(ty->lpVtbl->GetSubType(ty) == NULL); CHECK(ty->lpVtbl->GetBaseClass(ty) == NULL);
    RB_DONE(r);
}
TEST(rb_cb_type_is_equal)
{
    RB_REFLECT(r, "cbuffer CB { float4 x; float4 y; float3 z; int4 w; float4 q[2]; };\nfloat4 main(uint i : TEXCOORD0) : SV_Target { return x + y + float4(z, 0) + w + q[i]; }\n", "ps_5_0");
    ID3D11ShaderReflectionType *tx = rb_var(r, "x")->lpVtbl->GetType(rb_var(r, "x")), *ty = rb_var(r, "y")->lpVtbl->GetType(rb_var(r, "y")), *tz = rb_var(r, "z")->lpVtbl->GetType(rb_var(r, "z"));
    ID3D11ShaderReflectionType *tw = rb_var(r, "w")->lpVtbl->GetType(rb_var(r, "w")), *tq = rb_var(r, "q")->lpVtbl->GetType(rb_var(r, "q"));
    EXPECT_HR(tx->lpVtbl->IsEqual(tx, tx), S_OK);
    EXPECT_HR(tx->lpVtbl->IsEqual(tx, ty), S_OK);
    EXPECT_HR(tx->lpVtbl->IsEqual(tx, tz), S_FALSE);
    EXPECT_HR(tx->lpVtbl->IsEqual(tx, tw), S_FALSE);
    EXPECT_HR(tx->lpVtbl->IsEqual(tx, tq), S_FALSE);
    EXPECT_HR(tz->lpVtbl->IsEqual(tz, tx), S_FALSE);
    RB_DONE(r);
}

/* ---- used / unused / default values ---------------------------------------------------------------- */
TEST(rb_cb_used_and_unused_variable_flags)
{
    RB_REFLECT(r, "cbuffer CB { float4 used1; float4 unused1; float4 used2; float4 unused2; };\nfloat4 main() : SV_Target { return used1 * used2; }\n", "ps_5_0");
    D3D11_SHADER_VARIABLE_DESC v; D3D11_SHADER_BUFFER_DESC bd;
    if (rb_cb_desc(rb_cb(r, 0), &bd)) EXPECT_EQ(bd.Variables, 4);
    if (rb_var_desc(r, "used1", &v)) CHECK(v.uFlags & D3D_SVF_USED);
    if (rb_var_desc(r, "used2", &v)) CHECK(v.uFlags & D3D_SVF_USED);
    if (rb_var_desc(r, "unused1", &v)) { CHECK(!(v.uFlags & D3D_SVF_USED)); EXPECT_EQ(v.StartOffset, 16); EXPECT_EQ(v.Size, 16); }
    if (rb_var_desc(r, "unused2", &v)) { CHECK(!(v.uFlags & D3D_SVF_USED)); EXPECT_EQ(v.StartOffset, 48); }
    RB_DONE(r);
}
TEST(rb_cb_partial_use_marks_variable_used)
{
    RB_REFLECT(r, "cbuffer CB { float4 v; float4 w; };\nfloat4 main() : SV_Target { return v.x; }\n", "ps_5_0");
    D3D11_SHADER_VARIABLE_DESC d;
    if (rb_var_desc(r, "v", &d)) CHECK(d.uFlags & D3D_SVF_USED);
    if (rb_var_desc(r, "w", &d)) CHECK(!(d.uFlags & D3D_SVF_USED));
    RB_DONE(r);
}
TEST(rb_cb_struct_member_use_marks_struct_used)
{
    RB_REFLECT(r, "struct S { float4 a; float4 b; };\ncbuffer CB { S used; S unused; };\nfloat4 main() : SV_Target { return used.b; }\n", "ps_5_0");
    D3D11_SHADER_VARIABLE_DESC d;
    if (rb_var_desc(r, "used", &d)) CHECK(d.uFlags & D3D_SVF_USED);
    if (rb_var_desc(r, "unused", &d)) CHECK(!(d.uFlags & D3D_SVF_USED));
    RB_DONE(r);
}
TEST(rb_cb_default_values_of_initialized_variables)
{
    RB_REFLECT(r, "cbuffer CB { float4 withdef = float4(1, 2, 3, 4); float4 nodef; int2 idef = int2(7, -9); };\nfloat4 main() : SV_Target { return withdef + nodef + float4(idef, 0, 0); }\n", "ps_5_0");
    D3D11_SHADER_VARIABLE_DESC d;
    if (rb_var_desc(r, "nodef", &d)) CHECK(d.DefaultValue == NULL);
    if (rb_var_desc(r, "withdef", &d)) {
        if (d.DefaultValue) { const float *f = d.DefaultValue; CHECK(f[0] == 1.0f && f[1] == 2.0f && f[2] == 3.0f && f[3] == 4.0f); }
        else t_note("initializer of withdef produced no default value");
    }
    if (rb_var_desc(r, "idef", &d)) {
        if (d.DefaultValue) { const int *iv = d.DefaultValue; CHECK(iv[0] == 7 && iv[1] == -9); }
        else t_note("initializer of idef produced no default value");
    }
    RB_DONE(r);
}
TEST(rb_cb_default_value_survives_release_of_nothing)
{
    /* default value memory belongs to the reflector and stays valid while it is alive */
    RB_REFLECT(r, "cbuffer CB { float4 w = float4(5, 6, 7, 8); };\nfloat4 main() : SV_Target { return w; }\n", "ps_5_0");
    D3D11_SHADER_VARIABLE_DESC d1, d2;
    if (rb_var_desc(r, "w", &d1) && rb_var_desc(r, "w", &d2)) { CHECK(d1.DefaultValue == d2.DefaultValue); CHECK(d1.Name == d2.Name); }
    RB_DONE(r);
}

/* ---- buffer kinds and enumeration ------------------------------------------------------------------ */
TEST(rb_cb_globals_buffer)
{
    RB_REFLECT(r, "float4 g1; float g2; float4 main() : SV_Target { return g1 * g2; }\n", "ps_5_0");
    D3D11_SHADER_BUFFER_DESC bd; D3D11_SHADER_DESC d; memset(&d, 0, sizeof d);
    EXPECT_HR(r->lpVtbl->GetDesc(r, &d), S_OK); EXPECT_EQ(d.ConstantBuffers, 1);
    if (rb_cb_desc(rb_cb(r, 0), &bd)) { EXPECT_STR(bd.Name, "$Globals"); EXPECT_EQ(bd.Type, D3D_CT_CBUFFER); EXPECT_EQ(bd.Variables, 2); EXPECT_EQ(bd.Size, 32); }
    { D3D11_SHADER_VARIABLE_DESC v; if (rb_var_desc(r, "g1", &v)) EXPECT_EQ(v.StartOffset, 0); if (rb_var_desc(r, "g2", &v)) EXPECT_EQ(v.StartOffset, 16); }
    { D3D11_SHADER_INPUT_BIND_DESC bind; EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "$Globals", &bind), S_OK); EXPECT_EQ(bind.Type, D3D_SIT_CBUFFER); EXPECT_EQ(bind.BindPoint, 0); }
    RB_DONE(r);
}
TEST(rb_cb_tbuffer_kind)
{
    RB_REFLECT(r, "tbuffer TB : register(t3) { float4 tv; float4 tw; };\nfloat4 main() : SV_Target { return tv + tw; }\n", "ps_5_0");
    D3D11_SHADER_BUFFER_DESC bd; D3D11_SHADER_INPUT_BIND_DESC bind;
    if (rb_cb_desc(rb_cb(r, 0), &bd)) { EXPECT_STR(bd.Name, "TB"); EXPECT_EQ(bd.Type, D3D_CT_TBUFFER); EXPECT_EQ(bd.Variables, 2); EXPECT_EQ(bd.Size, 32); }
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "TB", &bind), S_OK);
    EXPECT_EQ(bind.Type, D3D_SIT_TBUFFER); EXPECT_EQ(bind.BindPoint, 3); EXPECT_EQ(bind.BindCount, 1);
    RB_DONE(r);
}
TEST(rb_cb_register_slot_in_bind_desc)
{
    RB_REFLECT(r, "cbuffer A : register(b2) { float4 a; };\ncbuffer B : register(b7) { float4 b; };\nfloat4 main() : SV_Target { return a + b; }\n", "ps_5_0");
    D3D11_SHADER_INPUT_BIND_DESC bind;
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "A", &bind), S_OK); EXPECT_EQ(bind.BindPoint, 2); EXPECT_EQ(bind.Type, D3D_SIT_CBUFFER);
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "B", &bind), S_OK); EXPECT_EQ(bind.BindPoint, 7); EXPECT_EQ(bind.BindCount, 1);
    EXPECT_EQ(bind.Dimension, D3D_SRV_DIMENSION_UNKNOWN); EXPECT_EQ(bind.NumSamples, 0); EXPECT_EQ(bind.ReturnType, 0);
    RB_DONE(r);
}
TEST(rb_cb_multiple_buffers_by_name_and_index)
{
    RB_REFLECT(r, "cbuffer First : register(b0) { float4 f1; };\ncbuffer Second : register(b1) { float4 s1; float4 s2; };\ncbuffer Third : register(b2) { float3 t1; };\n"
              "float4 main() : SV_Target { return f1 + s1 + s2 + float4(t1, 0); }\n", "ps_5_0");
    static const char *const names[] = { "First", "Second", "Third" }; static const UINT nvars[] = { 1, 2, 1 };
    D3D11_SHADER_DESC d; memset(&d, 0, sizeof d);
    EXPECT_HR(r->lpVtbl->GetDesc(r, &d), S_OK); EXPECT_EQ(d.ConstantBuffers, 3);
    for (int k = 0; k < 3; k++) {
        ID3D11ShaderReflectionConstantBuffer *byn = r->lpVtbl->GetConstantBufferByName(r, names[k]); D3D11_SHADER_BUFFER_DESC bd;
        if (rb_cb_desc(byn, &bd)) { EXPECT_STR(bd.Name, names[k]); EXPECT_EQ(bd.Variables, nvars[k]); }
    }
    { int found = 0; for (UINT i = 0; i < 3; i++) { D3D11_SHADER_BUFFER_DESC bd; if (rb_cb_desc(rb_cb(r, i), &bd)) for (int k = 0; k < 3; k++) found += !strcmp(bd.Name, names[k]); } EXPECT_EQ(found, 3); }
    { D3D11_SHADER_BUFFER_DESC bd; if (rb_cb_desc(r->lpVtbl->GetConstantBufferByName(r, "Third"), &bd)) EXPECT_EQ(bd.Size, 16); }
    RB_DONE(r);
}
TEST(rb_cb_get_variable_by_name_searches_all_buffers)
{
    RB_REFLECT(r, "cbuffer P : register(b0) { float4 pv; };\ncbuffer Q : register(b1) { float4 qv; };\nfloat4 main() : SV_Target { return pv + qv; }\n", "ps_5_0");
    D3D11_SHADER_VARIABLE_DESC d;
    if (rb_var_desc(r, "pv", &d)) EXPECT_STR(d.Name, "pv");
    if (rb_var_desc(r, "qv", &d)) EXPECT_STR(d.Name, "qv");
    { ID3D11ShaderReflectionVariable *v = rb_var(r, "qv"); ID3D11ShaderReflectionConstantBuffer *owner = v->lpVtbl->GetBuffer(v); D3D11_SHADER_BUFFER_DESC bd; if (rb_cb_desc(owner, &bd)) EXPECT_STR(bd.Name, "Q"); }
    RB_DONE(r);
}
TEST(rb_cb_invalid_lookups_return_shared_invalid_objects)
{
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    ID3D11ShaderReflectionConstantBuffer *c0 = rb_cb(r, 0), *i1 = rb_cb(r, 1), *i2 = rb_cb(r, 999), *i3 = r->lpVtbl->GetConstantBufferByName(r, "Missing"), *i4 = r->lpVtbl->GetConstantBufferByName(r, NULL), *i5 = r->lpVtbl->GetConstantBufferByName(r, "cb");
    D3D11_SHADER_BUFFER_DESC bd;
    CHECK(c0 && i1 && c0 != i1); CHECK(i1 == i2 && i2 == i3 && i3 == i4 && i4 == i5);
    memset(&bd, 0xcc, sizeof bd); EXPECT_HR(i1->lpVtbl->GetDesc(i1, &bd), RB_E_FAIL);
    EXPECT_HR(c0->lpVtbl->GetDesc(c0, NULL), RB_E_FAIL);
    { ID3D11ShaderReflectionVariable *iv = i1->lpVtbl->GetVariableByIndex(i1, 0), *iv2 = i1->lpVtbl->GetVariableByName(i1, "a"); D3D11_SHADER_VARIABLE_DESC vd; CHECK(iv && iv == iv2); EXPECT_HR(iv->lpVtbl->GetDesc(iv, &vd), RB_E_FAIL); }
    RB_DONE(r);
}
TEST(rb_cb_invalid_variable_objects)
{
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    ID3D11ShaderReflectionConstantBuffer *c0 = rb_cb(r, 0);
    ID3D11ShaderReflectionVariable *ok = c0->lpVtbl->GetVariableByIndex(c0, 0), *b1 = c0->lpVtbl->GetVariableByIndex(c0, 6), *b2 = c0->lpVtbl->GetVariableByIndex(c0, 0xffffffffu), *b3 = c0->lpVtbl->GetVariableByName(c0, "zzz"), *b4 = c0->lpVtbl->GetVariableByName(c0, NULL), *b5 = r->lpVtbl->GetVariableByName(r, "zzz"), *b6 = r->lpVtbl->GetVariableByName(r, NULL), *b7 = c0->lpVtbl->GetVariableByName(c0, "A");
    D3D11_SHADER_VARIABLE_DESC vd; D3D11_SHADER_TYPE_DESC td;
    CHECK(ok != b1); CHECK(b1 == b2 && b2 == b3 && b3 == b4 && b4 == b5 && b5 == b6 && b6 == b7);
    EXPECT_HR(b1->lpVtbl->GetDesc(b1, &vd), RB_E_FAIL);
    EXPECT_HR(ok->lpVtbl->GetDesc(ok, NULL), RB_E_FAIL);
    { ID3D11ShaderReflectionType *it = b1->lpVtbl->GetType(b1); CHECK(it != NULL); EXPECT_HR(it->lpVtbl->GetDesc(it, &td), RB_E_FAIL); }
    { ID3D11ShaderReflectionConstantBuffer *ib = b1->lpVtbl->GetBuffer(b1); D3D11_SHADER_BUFFER_DESC bd; CHECK(ib != NULL); EXPECT_HR(ib->lpVtbl->GetDesc(ib, &bd), RB_E_FAIL); }
    EXPECT_EQ(b1->lpVtbl->GetInterfaceSlot(b1, 0), 0xffffffffu);
    RB_DONE(r);
}
TEST(rb_cb_variable_get_buffer_roundtrip)
{
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    ID3D11ShaderReflectionVariable *v = rb_var(r, "m"); ID3D11ShaderReflectionConstantBuffer *owner = v->lpVtbl->GetBuffer(v); D3D11_SHADER_BUFFER_DESC bd;
    if (rb_cb_desc(owner, &bd)) { EXPECT_STR(bd.Name, "CB"); EXPECT_EQ(bd.Variables, 6); }
    RB_DONE(r);
}
TEST(rb_cb_pointers_stable_while_reflector_alive)
{
    RB_REFLECT(r, PACK_SRC, "ps_5_0");
    ID3D11ShaderReflectionConstantBuffer *a = rb_cb(r, 0), *b = rb_cb(r, 0); ID3D11ShaderReflectionVariable *v1 = rb_var(r, "d"), *v2 = rb_var(r, "d");
    CHECK(a == b); CHECK(v1 == v2);
    { D3D11_SHADER_BUFFER_DESC b1, b2; if (rb_cb_desc(a, &b1) && rb_cb_desc(b, &b2)) CHECK(b1.Name == b2.Name); }
    RB_DONE(r);
}
TEST(rb_cb_unused_cbuffer_is_still_listed_or_dropped)
{
    RB_REFLECT(r, "cbuffer Used : register(b0) { float4 u; };\ncbuffer Unused : register(b1) { float4 x; };\nfloat4 main() : SV_Target { return u; }\n", "ps_5_0");
    D3D11_SHADER_DESC d; memset(&d, 0, sizeof d); EXPECT_HR(r->lpVtbl->GetDesc(r, &d), S_OK);
    CHECK(d.ConstantBuffers >= 1 && d.ConstantBuffers <= 2);
    t_note("unreferenced cbuffer: ConstantBuffers=%u BoundResources=%u", d.ConstantBuffers, d.BoundResources);
    RB_DONE(r);
}
TEST(rb_cb_userpacked_flag_with_packoffset)
{
    RB_REFLECT(r, "cbuffer CB { float4 a : packoffset(c1); float4 b : packoffset(c0); };\nfloat4 main() : SV_Target { return a + b; }\n", "ps_5_0");
    D3D11_SHADER_BUFFER_DESC bd; D3D11_SHADER_VARIABLE_DESC v;
    if (rb_var_desc(r, "a", &v)) EXPECT_EQ(v.StartOffset, 16);
    if (rb_var_desc(r, "b", &v)) EXPECT_EQ(v.StartOffset, 0);
    if (rb_cb_desc(rb_cb(r, 0), &bd)) { if (!(bd.uFlags & D3D_CBF_USERPACKED)) t_note("packoffset did not set D3D_CBF_USERPACKED (flags 0x%x)", bd.uFlags); EXPECT_EQ(bd.Size, 32); }
    RB_DONE(r);
}
TEST(rb_cb_many_variables)
{
    char src[8192]; int n = 0; char *p = src;
    p += sprintf(p, "cbuffer CB {\n"); for (int i = 0; i < 40; i++) p += sprintf(p, "float4 v%d;\n", i); p += sprintf(p, "};\nfloat4 main() : SV_Target { return 0");
    for (int i = 0; i < 40; i++) { p += sprintf(p, " + v%d", i); } p += sprintf(p, "; }\n"); (void)n;
    RB_REFLECT(r, src, "ps_5_0");
    D3D11_SHADER_BUFFER_DESC bd; D3D11_SHADER_VARIABLE_DESC v;
    if (rb_cb_desc(rb_cb(r, 0), &bd)) { EXPECT_EQ(bd.Variables, 40); EXPECT_EQ(bd.Size, 640); }
    if (rb_var_desc(r, "v39", &v)) EXPECT_EQ(v.StartOffset, 39 * 16);
    if (rb_var_desc(r, "v17", &v)) EXPECT_EQ(v.StartOffset, 17 * 16);
    RB_DONE(r);
}
TEST(rb_cb_cbuffer_size_rounding)
{
    RB_REFLECT(r, "cbuffer Small { float s; };\ncbuffer Mid : register(b1) { float3 m3; };\ncbuffer Big : register(b2) { float4 b4; float b1; };\nfloat4 main() : SV_Target { return s + float4(m3, b4.x + b1); }\n", "ps_5_0");
    D3D11_SHADER_BUFFER_DESC bd;
    if (rb_cb_desc(r->lpVtbl->GetConstantBufferByName(r, "Small"), &bd)) EXPECT_EQ(bd.Size, 16);
    if (rb_cb_desc(r->lpVtbl->GetConstantBufferByName(r, "Mid"), &bd)) EXPECT_EQ(bd.Size, 16);
    if (rb_cb_desc(r->lpVtbl->GetConstantBufferByName(r, "Big"), &bd)) EXPECT_EQ(bd.Size, 32);
    RB_DONE(r);
}
TEST(rb_cb_same_layout_across_shader_stages)
{
    static const char *tail[] = { "ps_5_0", "vs_5_0", "gs_5_0" };
    (void)tail;
    ID3DBlob *bp = NULL, *bv = NULL;
    if (!rb_compile_t("cbuffer CB { float4 a; float3 b; float c; };\nfloat4 main() : SV_Target { return a + float4(b, c); }\n", "ps_5_0", 0, &bp)) return;
    if (!rb_compile_t("cbuffer CB { float4 a; float3 b; float c; };\nfloat4 main() : SV_Position { return a + float4(b, c); }\n", "vs_5_0", 0, &bv)) { ID3D10Blob_Release(bp); return; }
    ID3D11ShaderReflection *rp = rb_reflect(bp), *rv = rb_reflect(bv);
    if (rp && rv) {
        D3D11_SHADER_VARIABLE_DESC a, b; const char *n[] = { "a", "b", "c" };
        for (int k = 0; k < 3; k++) { if (rb_var_desc(rp, n[k], &a) && rb_var_desc(rv, n[k], &b)) CHECK(a.StartOffset == b.StartOffset && a.Size == b.Size); }
    }
    if (rp) rp->lpVtbl->Release(rp);
    if (rv) rv->lpVtbl->Release(rv);
    ID3D10Blob_Release(bp); ID3D10Blob_Release(bv);
}
TEST(rb_cb_target_level_changes_nothing_in_layout)
{
    static const char *const targets[] = { "ps_4_0", "ps_4_1", "ps_5_0" };
    for (int t = 0; t < 3; t++) {
        ID3DBlob *b; ID3D11ShaderReflection *r; D3D11_SHADER_VARIABLE_DESC v;
        if (!rb_compile_t("cbuffer CB { float4 a; float3 b; float c; float2 d; };\nfloat4 main() : SV_Target { return a + float4(b, c) + float4(d, 0, 0); }\n", targets[t], 0, &b)) continue;
        r = rb_reflect(b);
        if (r) { if (rb_var_desc(r, "c", &v)) t_check(v.StartOffset == 28, __FILE__, __LINE__, "offset of c", "%s: %u", targets[t], v.StartOffset); if (rb_var_desc(r, "d", &v)) t_check(v.StartOffset == 32, __FILE__, __LINE__, "offset of d", "%s: %u", targets[t], v.StartOffset); r->lpVtbl->Release(r); }
        ID3D10Blob_Release(b);
    }
}
