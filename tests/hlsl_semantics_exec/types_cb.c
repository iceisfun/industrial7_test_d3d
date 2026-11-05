/* constant buffer packing (reflection + execution), matrix orientation, double precision and minimum precision types. */
#include "hse.h"

/* ---- reflection helpers ----------------------------------------------------------------------------------- */
typedef struct { const char *name; int off; const char *use; } lv;   /* use: float/uint expression reading the variable's first scalar */

static ID3D11ShaderReflection *reflect_blob(ID3DBlob *bl) {
    typeof(&D3DReflect) f = d3dh_D3DReflect();
    ID3D11ShaderReflection *r = NULL;
    if (!f) return NULL;
    if (FAILED(f(d3dh_blob_ptr(bl), d3dh_blob_size(bl), &IID_ID3D11ShaderReflection, (void **)&r))) return NULL;
    return r;
}
static void build_cb_shader(char *src, size_t cap, const char *decls, const lv *v, int n, const char *extra_pre) {
    int p = snprintf(src, cap, "%s\ncbuffer cb : register(b0) { %s };\nRWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() {\n", extra_pre ? extra_pre : "", decls);
    for (int i = 0; i < n; i++) if (v[i].use) p += snprintf(src + p, cap - p, "  O.Store(%d, asuint(%s));\n", i * 4, v[i].use);
    snprintf(src + p, cap - p, "}\n");
}
/* Compile a cbuffer, reflect it; hard-check offsets/size when `hard`, otherwise record them. Execution cross-check when a device exists. */
static void check_layout_at(const char *file, int line, const char *decls, const char *pre, const lv *v, int n, int size, int hard, UINT cflags) {
    char src[4096];
    build_cb_shader(src, sizeof src, decls, v, n, pre);
    ID3DBlob *bl = hse_compile_at(file, line, src, "cs_5_0", cflags);
    if (!bl) return;
    ID3D11ShaderReflection *r = reflect_blob(bl);
    if (!r) { t_check(0, file, line, "D3DReflect", NULL); SAFE_RELEASE(bl); return; }
    ID3D11ShaderReflectionConstantBuffer *cbr = r->lpVtbl->GetConstantBufferByName(r, "cb");
    D3D11_SHADER_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
    HRESULT hr = cbr ? cbr->lpVtbl->GetDesc(cbr, &bd) : E_FAIL;
    t_check(SUCCEEDED(hr), file, line, "constant buffer cb reflected", "hr=%s", d3dh_hr_name(hr));
    int offs[32];
    if (SUCCEEDED(hr)) {
        t_check(bd.Type == D3D_CT_CBUFFER, file, line, "buffer type is cbuffer", "type %d", bd.Type);
        if (hard) t_check((int)bd.Size == size, file, line, "buffer size", "got %u want %d", bd.Size, size);
        else if ((int)bd.Size != size) t_note("layout '%.60s': buffer size %u (expected %d)", decls, bd.Size, size);
        for (int i = 0; i < n; i++) {
            ID3D11ShaderReflectionVariable *var = cbr->lpVtbl->GetVariableByName(cbr, v[i].name);
            D3D11_SHADER_VARIABLE_DESC vd; memset(&vd, 0, sizeof vd);
            HRESULT h2 = var ? var->lpVtbl->GetDesc(var, &vd) : E_FAIL;
            offs[i] = -1;
            if (FAILED(h2)) { t_check(0, file, line, "variable reflected", "%s hr=%s", v[i].name, d3dh_hr_name(h2)); continue; }
            offs[i] = (int)vd.StartOffset;
            if (hard) t_check((int)vd.StartOffset == v[i].off, file, line, "variable offset", "%s: got %u want %d", v[i].name, vd.StartOffset, v[i].off);
            else if ((int)vd.StartOffset != v[i].off) t_note("layout '%.60s': %s at offset %u (expected %d)", decls, v[i].name, vd.StartOffset, v[i].off);
            if (v[i].use) t_check(vd.uFlags & D3D_SVF_USED, file, line, "variable flagged used", "%s flags %x", v[i].name, vd.uFlags);
        }
    }
    /* execution: word k of the constant buffer holds 1000+k; each variable's first scalar must come from its reflected offset */
    if (hse.dev && SUCCEEDED(hr)) {
        uint32_t cbd[64], out[32] = { 0 };
        for (int k = 0; k < 64; k++) cbd[k] = 1000 + k;
        hse_cs c; memset(&c, 0, sizeof c);
        c.src = src; c.cflags = cflags; c.cb = cbd; c.cbbytes = bd.Size < 16 ? 16 : (bd.Size + 15) & ~15u; c.outbytes = 128; c.gx = 1; c.out = out;
        if (hse_dispatch_at(file, line, &c))
            for (int i = 0; i < n; i++) if (v[i].use && offs[i] >= 0)
                t_check(out[i] == 1000u + (uint32_t)offs[i] / 4, file, line, "executed read matches reflected offset", "%s off=%d got %u want %u", v[i].name, offs[i], out[i], 1000u + offs[i] / 4);
    } else if (!hse.dev) { /* compile-only emulation run */ }
    SAFE_RELEASE(r); SAFE_RELEASE(bl);
}
#define CHECK_LAYOUT(decls, v, size) do { HSE_NEED(); check_layout_at(__FILE__, __LINE__, decls, NULL, v, (int)(sizeof v / sizeof v[0]), size, 1, 0); } while (0)
#define NOTE_LAYOUT(decls, v, size) do { HSE_NEED(); check_layout_at(__FILE__, __LINE__, decls, NULL, v, (int)(sizeof v / sizeof v[0]), size, 0, 0); } while (0)

/* ---- documented packing examples ------------------------------------------------------------------------------ */
TEST(hlsl_semantics_exec_cb_float_float3_float_float2) {
    static const lv v[] = { { "a", 0, "a" }, { "b", 4, "b.x" }, { "c", 16, "c" }, { "d", 20, "d.x" } };
    CHECK_LAYOUT("float a; float3 b; float c; float2 d;", v, 32);
}
TEST(hlsl_semantics_exec_cb_float4_float2_float2_new_vector) {
    static const lv v[] = { { "Val1", 0, "Val1.x" }, { "Val2", 16, "Val2.x" }, { "Val3", 24, "Val3.x" } };
    CHECK_LAYOUT("float4 Val1; float2 Val2; float2 Val3;", v, 32);
}
TEST(hlsl_semantics_exec_cb_float2_float4_float2_three_vectors) {
    static const lv v[] = { { "Val1", 0, "Val1.x" }, { "Val2", 16, "Val2.x" }, { "Val3", 32, "Val3.x" } };
    CHECK_LAYOUT("float2 Val1; float4 Val2; float2 Val3;", v, 48);
}
TEST(hlsl_semantics_exec_cb_float_float_float2_one_vector) {
    static const lv v[] = { { "Val1", 0, "Val1" }, { "Val2", 4, "Val2" }, { "Val3", 8, "Val3.x" } };
    CHECK_LAYOUT("float Val1; float Val2; float2 Val3;", v, 16);
}
TEST(hlsl_semantics_exec_cb_float_float2_float_one_vector) {
    static const lv v[] = { { "Val1", 0, "Val1" }, { "Val2", 4, "Val2.x" }, { "Val3", 12, "Val3" } };
    CHECK_LAYOUT("float Val1; float2 Val2; float Val3;", v, 16);
}
TEST(hlsl_semantics_exec_cb_three_floats_then_float2_straddles) {
    static const lv v[] = { { "A", 0, "A" }, { "B", 4, "B" }, { "C", 8, "C" }, { "D", 16, "D.x" } };
    CHECK_LAYOUT("float A; float B; float C; float2 D;", v, 32);
}
TEST(hlsl_semantics_exec_cb_float3_float_one_vector) {
    static const lv v[] = { { "Val1", 0, "Val1.x" }, { "Val2", 12, "Val2" } };
    CHECK_LAYOUT("float3 Val1; float Val2;", v, 16);
}
TEST(hlsl_semantics_exec_cb_float_float3_one_vector) {
    static const lv v[] = { { "Val1", 0, "Val1" }, { "Val2", 4, "Val2.x" } };
    CHECK_LAYOUT("float Val1; float3 Val2;", v, 16);
}
TEST(hlsl_semantics_exec_cb_float_float_float3_new_vector) {
    static const lv v[] = { { "A", 0, "A" }, { "B", 4, "B" }, { "C", 16, "C.x" } };
    CHECK_LAYOUT("float A; float B; float3 C;", v, 32);
}
TEST(hlsl_semantics_exec_cb_float2_straddle_moves_to_next_vector) {
    static const lv v[] = { { "a", 0, "a" }, { "b", 4, "b.x" }, { "c", 16, "c.x" } };
    CHECK_LAYOUT("float a; float2 b; float2 c;", v, 32);
}
TEST(hlsl_semantics_exec_cb_float4_after_scalar_new_vector) {
    static const lv v[] = { { "a", 0, "a" }, { "b", 16, "b.x" }, { "c", 32, "c" } };
    CHECK_LAYOUT("float a; float4 b; float c;", v, 48);
}
TEST(hlsl_semantics_exec_cb_struct_forces_new_vector_after_scalar) {
    static const lv v[] = { { "Val1", 0, "Val1" }, { "Val2", 16, "Val2.SVal1.x" } };
    char dummy;(void)dummy;
    HSE_NEED();
    check_layout_at(__FILE__, __LINE__, "float Val1; SS Val2;", "struct SS { float4 SVal1; float SVal2; };", v, 2, 48, 1, 0);
}
TEST(hlsl_semantics_exec_cb_struct_small_members_new_vector) {
    static const lv v[] = { { "Val1", 0, "Val1" }, { "Val2", 16, "Val2.SVal1" } };
    HSE_NEED();
    check_layout_at(__FILE__, __LINE__, "float Val1; SS Val2;", "struct SS { float SVal1; float4 SVal2; };", v, 2, 48, 1, 0);
}
TEST(hlsl_semantics_exec_cb_scalar_after_struct_new_vector) {
    static const lv v[] = { { "Val1", 0, "Val1.SVal1.x" }, { "Val2", 32, "Val2" } };
    HSE_NEED();
    check_layout_at(__FILE__, __LINE__, "SS Val1; float Val2;", "struct SS { float4 SVal1; float SVal2; };", v, 2, 48, 1, 0);
}
TEST(hlsl_semantics_exec_cb_struct_size_rounded_to_vector) {
    static const lv v[] = { { "a", 0, "a.x.x" }, { "b", 16, "b.x.x" }, { "c", 32, "c" } };
    HSE_NEED();
    /* each 12-byte struct occupies a full vector, the following variable starts on the next one */
    check_layout_at(__FILE__, __LINE__, "SS a; SS b; float c;", "struct SS { float x; float2 y; };", v, 3, 48, 1, 0);
}
TEST(hlsl_semantics_exec_cb_ints_bool_are_four_bytes) {
    static const lv v[] = { { "a", 0, "a" }, { "b", 4, "b" }, { "c", 8, NULL }, { "d", 12, "d" } };
    CHECK_LAYOUT("int a; uint b; bool c; float d;", v, 16);
}
TEST(hlsl_semantics_exec_cb_array_element_per_vector) {
    static const lv v[] = { { "a", 0, "a" }, { "e", 16, "e[1]" }, { "g", 0, NULL } };
    HSE_NEED();
    /* arrays start on a new vector and every element has its own vector: e[1] is read from byte 32 */
    char src[2048];
    snprintf(src, sizeof src, "cbuffer cb { float a; float e[3]; float4 f; }; RWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { O.Store4(0, asuint(float4(a, e[0], e[1], e[2]))); O.Store(16, asuint(f.x)); }");
    ID3DBlob *bl = HSE_COMPILE(src, "cs_5_0", 0);
    if (!bl) return;
    ID3D11ShaderReflection *r = reflect_blob(bl);
    if (r) {
        ID3D11ShaderReflectionConstantBuffer *cbr = r->lpVtbl->GetConstantBufferByIndex(r, 0);
        D3D11_SHADER_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
        if (cbr && SUCCEEDED(cbr->lpVtbl->GetDesc(cbr, &bd))) {
            EXPECT_EQ(bd.Variables, 3);
            ID3D11ShaderReflectionVariable *e = cbr->lpVtbl->GetVariableByName(cbr, "e"), *f = cbr->lpVtbl->GetVariableByName(cbr, "f");
            D3D11_SHADER_VARIABLE_DESC ed, fd; memset(&ed, 0, sizeof ed); memset(&fd, 0, sizeof fd);
            if (e && f && SUCCEEDED(e->lpVtbl->GetDesc(e, &ed)) && SUCCEEDED(f->lpVtbl->GetDesc(f, &fd))) {
                EXPECT_EQ(ed.StartOffset, 16);
                CHECKF(ed.Size == 36 || ed.Size == 48, "array size %u", ed.Size);
                t_note("float e[3]: size %u (unpadded last element = 36), next float4 at %u", ed.Size, fd.StartOffset);
                EXPECT_EQ(fd.StartOffset, 64);
            }
            EXPECT_EQ(bd.Size, 80);
        }
        r->lpVtbl->Release(r);
    }
    SAFE_RELEASE(bl);
    if (hse.dev) {
        uint32_t cbd[32], out[8] = { 0 };
        for (int k = 0; k < 32; k++) cbd[k] = 1000 + k;
        hse_cs c; memset(&c, 0, sizeof c);
        c.src = src; c.cb = cbd; c.cbbytes = 80; c.outbytes = 32; c.gx = 1; c.out = out;
        if (HSE_DISPATCH(&c)) { EXPECT_EQ(out[0], 1000); EXPECT_EQ(out[1], 1004); EXPECT_EQ(out[2], 1008); EXPECT_EQ(out[3], 1012); EXPECT_EQ(out[4], 1016); }
    }
}
TEST(hlsl_semantics_exec_cb_array_scalar_after_array_observed) {
    static const lv v[] = { { "e", 0, "e[1]" }, { "g", 20, "g" } };
    NOTE_LAYOUT("float e[2]; float g;", v, 32);
}
TEST(hlsl_semantics_exec_cb_float2_array_then_float2_observed) {
    static const lv v[] = { { "e", 0, "e[1].x" }, { "g", 24, "g.x" } };
    NOTE_LAYOUT("float2 e[2]; float2 g;", v, 32);
}
TEST(hlsl_semantics_exec_cb_packoffset_places_variables) {
    static const lv v[] = { { "a", 16, "a.x" }, { "b", 8, "b" }, { "c", 0, "c" }, { "d", 44, "d" } };
    CHECK_LAYOUT("float4 a : packoffset(c1); float b : packoffset(c0.z); float c : packoffset(c0.x); float d : packoffset(c2.w);", v, 48);
}
TEST(hlsl_semantics_exec_cb_packoffset_c_register_units) {
    static const lv v[] = { { "x", 48, "x" }, { "y", 4, "y" } };
    CHECK_LAYOUT("float x : packoffset(c3); float y : packoffset(c0.y);", v, 64);
}
TEST(hlsl_semantics_exec_cb_double_packing_observed) {
    static const lv v[] = { { "a", 0, "asuint(a)" }, { "b", 16, "asuint(b.x)" }, { "c", 32, "c" } };
    HSE_NEED();
    D3D11_FEATURE_DATA_DOUBLES dd = { 0 };
    if (hse.dev) { ID3D11Device_CheckFeatureSupport(hse.dev, D3D11_FEATURE_DOUBLES, &dd, sizeof dd); if (!dd.DoublePrecisionFloatShaderOps) { t_skip("doubles unsupported"); return; } }
    char src[1024];
    snprintf(src, sizeof src, "cbuffer cb { double a; double2 b; float c; }; RWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { uint l, h; asuint(a, l, h); uint l2, h2; asuint(b.x, l2, h2); O.Store4(0, uint4(l, h, l2, h2)); O.Store(16, asuint(c)); }");
    ID3DBlob *bl = HSE_COMPILE(src, "cs_5_0", 0);
    if (!bl) return;
    ID3D11ShaderReflection *r = reflect_blob(bl);
    if (r) {
        ID3D11ShaderReflectionConstantBuffer *cbr = r->lpVtbl->GetConstantBufferByIndex(r, 0);
        D3D11_SHADER_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
        if (cbr && SUCCEEDED(cbr->lpVtbl->GetDesc(cbr, &bd))) {
            for (int i = 0; i < 3; i++) {
                ID3D11ShaderReflectionVariable *var = cbr->lpVtbl->GetVariableByName(cbr, v[i].name);
                D3D11_SHADER_VARIABLE_DESC vd; memset(&vd, 0, sizeof vd);
                if (var && SUCCEEDED(var->lpVtbl->GetDesc(var, &vd))) t_note("double layout: %s offset %u size %u", v[i].name, vd.StartOffset, vd.Size);
            }
            t_note("double layout: buffer size %u", bd.Size);
            EXPECT_EQ(bd.Size % 16, 0);
        }
        r->lpVtbl->Release(r);
    }
    SAFE_RELEASE(bl);
}
TEST(hlsl_semantics_exec_cb_buffer_desc_fields) {
    HSE_NEED();
    char src[1024];
    snprintf(src, sizeof src, "cbuffer MyBuf : register(b2) { float4 p; float q; }; cbuffer Other { float z; }; RWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { O.Store4(0, asuint(p)); O.Store2(16, asuint(float2(q, z))); }");
    ID3DBlob *bl = HSE_COMPILE(src, "cs_5_0", 0);
    if (!bl) return;
    ID3D11ShaderReflection *r = reflect_blob(bl);
    if (!r) { t_check(0, __FILE__, __LINE__, "D3DReflect", NULL); SAFE_RELEASE(bl); return; }
    D3D11_SHADER_DESC sd; memset(&sd, 0, sizeof sd);
    EXPECT_HR_OK(r->lpVtbl->GetDesc(r, &sd));
    EXPECT_EQ(sd.ConstantBuffers, 2);
    ID3D11ShaderReflectionConstantBuffer *cb = r->lpVtbl->GetConstantBufferByName(r, "MyBuf");
    D3D11_SHADER_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
    if (cb && SUCCEEDED(cb->lpVtbl->GetDesc(cb, &bd))) {
        EXPECT_STR(bd.Name, "MyBuf"); EXPECT_EQ(bd.Type, D3D_CT_CBUFFER); EXPECT_EQ(bd.Variables, 2); EXPECT_EQ(bd.Size, 32);
    } else t_check(0, __FILE__, __LINE__, "MyBuf reflected", NULL);
    D3D11_SHADER_INPUT_BIND_DESC ib; memset(&ib, 0, sizeof ib);
    if (SUCCEEDED(r->lpVtbl->GetResourceBindingDescByName(r, "MyBuf", &ib))) { EXPECT_EQ(ib.Type, D3D_SIT_CBUFFER); EXPECT_EQ(ib.BindPoint, 2); EXPECT_EQ(ib.BindCount, 1); }
    else t_check(0, __FILE__, __LINE__, "MyBuf binding", NULL);
    r->lpVtbl->Release(r); SAFE_RELEASE(bl);
}
TEST(hlsl_semantics_exec_cb_unused_variable_not_flagged_used) {
    HSE_NEED();
    char src[1024];
    snprintf(src, sizeof src, "cbuffer cb { float a; float b; float c; }; RWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { O.Store2(0, asuint(float2(a, c))); }");
    ID3DBlob *bl = HSE_COMPILE(src, "cs_5_0", 0);
    if (!bl) return;
    ID3D11ShaderReflection *r = reflect_blob(bl);
    if (r) {
        ID3D11ShaderReflectionConstantBuffer *cb = r->lpVtbl->GetConstantBufferByName(r, "cb");
        static const char *names[3] = { "a", "b", "c" }; static const int used[3] = { 1, 0, 1 };
        for (int i = 0; cb && i < 3; i++) {
            ID3D11ShaderReflectionVariable *v = cb->lpVtbl->GetVariableByName(cb, names[i]);
            D3D11_SHADER_VARIABLE_DESC vd; memset(&vd, 0, sizeof vd);
            if (v && SUCCEEDED(v->lpVtbl->GetDesc(v, &vd))) { CHECKF(!!(vd.uFlags & D3D_SVF_USED) == used[i], "%s flags %x", names[i], vd.uFlags); EXPECT_EQ(vd.StartOffset, i * 4); }
            else t_check(0, __FILE__, __LINE__, "variable present", names[i]);
        }
        r->lpVtbl->Release(r);
    }
    SAFE_RELEASE(bl);
}

/* ---- matrices ---------------------------------------------------------------------------------------------------------- */
static void mat_exec(const char *decl, const char *pre_pragma, UINT cflags, int rows, int cols, int row_major, int base_word, const char *mname, int cbbytes) {
    char src[4096]; int p = 0;
    p += snprintf(src + p, sizeof src - p, "%s\ncbuffer cb { %s };\nRWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() {\n", pre_pragma ? pre_pragma : "", decl);
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        p += snprintf(src + p, sizeof src - p, "  O.Store(%d, asuint(%s[%d][%d]));\n", (r * 4 + c) * 4, mname, r, c);
    snprintf(src + p, sizeof src - p, "}\n");
    uint32_t cbd[64], out[16] = { 0 };
    for (int k = 0; k < 64; k++) cbd[k] = 1000 + k;
    hse_cs cs; memset(&cs, 0, sizeof cs);
    cs.src = src; cs.cflags = cflags; cs.cb = cbd; cs.cbbytes = cbbytes; cs.outbytes = 64; cs.gx = 1; cs.out = out;
    if (!HSE_DISPATCH(&cs)) return;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        uint32_t want = 1000 + base_word + (row_major ? r * 4 + c : c * 4 + r);
        CHECKF(out[r * 4 + c] == want, "%s[%d][%d] got %u want %u", mname, r, c, out[r * 4 + c], want);
    }
}
TEST(hlsl_semantics_exec_mat_float4x4_default_column_major) { HSE_NEED(); mat_exec("float4x4 m;", NULL, 0, 4, 4, 0, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_float4x4_explicit_row_major) { HSE_NEED(); mat_exec("row_major float4x4 m;", NULL, 0, 4, 4, 1, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_float4x4_explicit_column_major) { HSE_NEED(); mat_exec("column_major float4x4 m;", NULL, 0, 4, 4, 0, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_pragma_pack_row_major) { HSE_NEED(); mat_exec("float4x4 m;", "#pragma pack_matrix(row_major)", 0, 4, 4, 1, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_pragma_pack_column_major) { HSE_NEED(); mat_exec("float4x4 m;", "#pragma pack_matrix(column_major)", 0, 4, 4, 0, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_compile_flag_row_major_default) { HSE_NEED(); mat_exec("float4x4 m;", NULL, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, 4, 4, 1, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_compile_flag_column_major_default) { HSE_NEED(); mat_exec("float4x4 m;", NULL, D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, 4, 4, 0, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_explicit_keyword_overrides_compile_flag) {
    HSE_NEED();
    mat_exec("column_major float4x4 m;", NULL, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, 4, 4, 0, 0, "m", 64);
    mat_exec("row_major float4x4 m;", NULL, D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, 4, 4, 1, 0, "m", 64);
}
TEST(hlsl_semantics_exec_mat_float3x3_column_major) { HSE_NEED(); mat_exec("column_major float3x3 m;", NULL, 0, 3, 3, 0, 0, "m", 48); }
TEST(hlsl_semantics_exec_mat_float3x3_row_major) { HSE_NEED(); mat_exec("row_major float3x3 m;", NULL, 0, 3, 3, 1, 0, "m", 48); }
TEST(hlsl_semantics_exec_mat_float2x2_column_major) { HSE_NEED(); mat_exec("column_major float2x2 m;", NULL, 0, 2, 2, 0, 0, "m", 32); }
TEST(hlsl_semantics_exec_mat_float2x2_row_major) { HSE_NEED(); mat_exec("row_major float2x2 m;", NULL, 0, 2, 2, 1, 0, "m", 32); }
TEST(hlsl_semantics_exec_mat_float4x3_column_major) { HSE_NEED(); mat_exec("column_major float4x3 m;", NULL, 0, 4, 3, 0, 0, "m", 48); }
TEST(hlsl_semantics_exec_mat_float4x3_row_major) { HSE_NEED(); mat_exec("row_major float4x3 m;", NULL, 0, 4, 3, 1, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_float3x4_column_major) { HSE_NEED(); mat_exec("column_major float3x4 m;", NULL, 0, 3, 4, 0, 0, "m", 64); }
TEST(hlsl_semantics_exec_mat_float3x4_row_major) { HSE_NEED(); mat_exec("row_major float3x4 m;", NULL, 0, 3, 4, 1, 0, "m", 48); }
TEST(hlsl_semantics_exec_mat_second_matrix_starts_new_vector) {
    HSE_NEED();
    mat_exec("float a; column_major float2x2 m;", NULL, 0, 2, 2, 0, 4, "m", 48);
    mat_exec("float a; row_major float2x2 m;", NULL, 0, 2, 2, 1, 4, "m", 48);
}
static void mul_exec(const char *decl, UINT cflags, int row_major, int use_left_vector) {
    char src[2048];
    snprintf(src, sizeof src, "cbuffer cb { %s };\nRWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { float4 v = float4(1, 2, 3, 4); float4 r = %s; O.Store4(0, asuint(r)); }",
             decl, use_left_vector ? "mul(v, m)" : "mul(m, v)");
    float cbd[16], out[4] = { 0 };
    for (int k = 0; k < 16; k++) cbd[k] = (float)k;
    hse_cs cs; memset(&cs, 0, sizeof cs);
    cs.src = src; cs.cflags = cflags; cs.cb = cbd; cs.cbbytes = 64; cs.outbytes = 16; cs.gx = 1; cs.out = out;
    if (!HSE_DISPATCH(&cs)) return;
    float v[4] = { 1, 2, 3, 4 }, M[4][4], want[4];
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) M[r][c] = cbd[row_major ? r * 4 + c : c * 4 + r];
    for (int i = 0; i < 4; i++) {
        want[i] = 0;
        for (int j = 0; j < 4; j++) want[i] += use_left_vector ? v[j] * M[j][i] : M[i][j] * v[j];
        CHECKF(out[i] == want[i], "component %d got %g want %g", i, out[i], want[i]);
    }
}
TEST(hlsl_semantics_exec_mat_mul_matrix_vector_column_major) { HSE_NEED(); mul_exec("float4x4 m;", 0, 0, 0); }
TEST(hlsl_semantics_exec_mat_mul_vector_matrix_column_major) { HSE_NEED(); mul_exec("float4x4 m;", 0, 0, 1); }
TEST(hlsl_semantics_exec_mat_mul_matrix_vector_row_major) { HSE_NEED(); mul_exec("row_major float4x4 m;", 0, 1, 0); }
TEST(hlsl_semantics_exec_mat_mul_vector_matrix_row_major) { HSE_NEED(); mul_exec("row_major float4x4 m;", 0, 1, 1); }
TEST(hlsl_semantics_exec_mat_array_of_float4x4_stride_64) {
    HSE_NEED();
    char src[1024];
    snprintf(src, sizeof src, "cbuffer cb { column_major float4x4 m[2]; }; RWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { O.Store4(0, asuint(float4(m[0][0][0], m[0][3][3], m[1][0][0], m[1][1][2]))); }");
    uint32_t cbd[32], out[4] = { 0 };
    for (int k = 0; k < 32; k++) cbd[k] = 1000 + k;
    hse_cs cs; memset(&cs, 0, sizeof cs);
    cs.src = src; cs.cb = cbd; cs.cbbytes = 128; cs.outbytes = 16; cs.gx = 1; cs.out = out;
    if (!HSE_DISPATCH(&cs)) return;
    EXPECT_EQ(out[0], 1000); EXPECT_EQ(out[1], 1015); EXPECT_EQ(out[2], 1016); EXPECT_EQ(out[3], 1016 + 2 * 4 + 1);
}
TEST(hlsl_semantics_exec_mat_matrix_reflection_sizes_observed) {
    HSE_NEED();
    static const struct { const char *decl; const char *use; int rows, cols; } t[] = {
        { "float4x4 m;", "m[0][0]", 4, 4 }, { "column_major float3x3 m;", "m[0][0]", 3, 3 }, { "row_major float3x3 m;", "m[0][0]", 3, 3 }, { "column_major float2x2 m;", "m[0][0]", 2, 2 },
        { "row_major float2x2 m;", "m[0][0]", 2, 2 }, { "column_major float4x3 m;", "m[0][0]", 4, 3 }, { "row_major float4x3 m;", "m[0][0]", 4, 3 },
        { "column_major float3x4 m;", "m[0][0]", 3, 4 }, { "row_major float3x4 m;", "m[0][0]", 3, 4 } };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        char src[512];
        snprintf(src, sizeof src, "cbuffer cb { %s float tail; }; RWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { O.Store2(0, asuint(float2(%s, tail))); }", t[i].decl, t[i].use);
        ID3DBlob *bl = HSE_COMPILE(src, "cs_5_0", 0);
        if (!bl) return;
        ID3D11ShaderReflection *r = reflect_blob(bl);
        if (r) {
            ID3D11ShaderReflectionConstantBuffer *cb = r->lpVtbl->GetConstantBufferByName(r, "cb");
            ID3D11ShaderReflectionVariable *m = cb ? cb->lpVtbl->GetVariableByName(cb, "m") : NULL, *tl = cb ? cb->lpVtbl->GetVariableByName(cb, "tail") : NULL;
            D3D11_SHADER_VARIABLE_DESC md, td; memset(&md, 0, sizeof md); memset(&td, 0, sizeof td);
            if (m && tl && SUCCEEDED(m->lpVtbl->GetDesc(m, &md)) && SUCCEEDED(tl->lpVtbl->GetDesc(tl, &td))) {
                EXPECT_EQ(md.StartOffset, 0);
                t_note("%-28s size %u, following float at %u", t[i].decl, md.Size, td.StartOffset);
                CHECKF(td.StartOffset >= md.Size && td.StartOffset % 4 == 0, "tail %u inside matrix of size %u", td.StartOffset, md.Size);
                ID3D11ShaderReflectionType *ty = m->lpVtbl->GetType(m);
                D3D11_SHADER_TYPE_DESC tdsc; memset(&tdsc, 0, sizeof tdsc);
                if (ty && SUCCEEDED(ty->lpVtbl->GetDesc(ty, &tdsc))) {
                    EXPECT_EQ(tdsc.Rows, t[i].rows); EXPECT_EQ(tdsc.Columns, t[i].cols); EXPECT_EQ(tdsc.Type, D3D_SVT_FLOAT);
                    CHECKF(tdsc.Class == (strstr(t[i].decl, "row_major") ? D3D_SVC_MATRIX_ROWS : D3D_SVC_MATRIX_COLUMNS), "class %d for %s", tdsc.Class, t[i].decl);
                }
            }
            r->lpVtbl->Release(r);
        }
        SAFE_RELEASE(bl);
    }
}

/* ---- double precision -------------------------------------------------------------------------------------------------------- */
typedef struct { uint64_t x, y, want; } drow;
static int doubles_ok(int *ext) {
    D3D11_FEATURE_DATA_DOUBLES dd = { 0 };
    D3D11_FEATURE_DATA_D3D11_OPTIONS opt; memset(&opt, 0, sizeof opt);
    if (FAILED(ID3D11Device_CheckFeatureSupport(hse.dev, D3D11_FEATURE_DOUBLES, &dd, sizeof dd))) return 0;
    if (ext) { *ext = SUCCEEDED(ID3D11Device_CheckFeatureSupport(hse.dev, D3D11_FEATURE_D3D11_OPTIONS, &opt, sizeof opt)) ? opt.ExtendedDoublesShaderInstructions : 0; }
    return dd.DoublePrecisionFloatShaderOps;
}
/* result_is_uint: expr yields uint stored in the low word; mode 0 exact bits, 1 any-NaN-equal */
static void dbl_run_at(const char *file, int line, const char *pre, const char *expr, int result_is_uint, const drow *rows, int n, int nan_equal) {
    char src[2048];
    if (result_is_uint)
        snprintf(src, sizeof src, "ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) {\n"
                 "  uint4 w = I.Load4(id.x * 16); double x = asdouble(w.x, w.y), y = asdouble(w.z, w.w); %s\n  O.Store2(id.x * 8, uint2(%s, 0)); }", pre, expr);
    else
        snprintf(src, sizeof src, "ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) {\n"
                 "  uint4 w = I.Load4(id.x * 16); double x = asdouble(w.x, w.y), y = asdouble(w.z, w.w); %s\n  double r = (%s); uint lo, hi; asuint(r, lo, hi); O.Store2(id.x * 8, uint2(lo, hi)); }", pre, expr);
    int groups = (n + 63) / 64;
    drow *in = calloc(groups * 64, sizeof *in); uint64_t *out = calloc(groups * 64, 8);
    memcpy(in, rows, n * sizeof *rows);
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = src; c.in = in; c.inbytes = groups * 64 * sizeof *in; c.outbytes = groups * 64 * 8; c.gx = groups; c.out = out;
    if (hse_dispatch_at(file, line, &c))
        for (int i = 0; i < n; i++) {
            uint64_t g = out[i], w = rows[i].want;
            int gn = ((g >> 52) & 0x7ff) == 0x7ff && (g & 0xfffffffffffffull), wn = ((w >> 52) & 0x7ff) == 0x7ff && (w & 0xfffffffffffffull);
            int ok = (nan_equal && (gn || wn)) ? (gn && wn) : g == w;
            t_check(ok, file, line, expr, "row %d x=%016llx y=%016llx got=%016llx want=%016llx", i, (unsigned long long)rows[i].x, (unsigned long long)rows[i].y, (unsigned long long)g, (unsigned long long)w);
        }
    free(in); free(out);
}
#define DBL_RUN(pre, expr, isu, rows, nan_eq) dbl_run_at(__FILE__, __LINE__, pre, expr, isu, rows, (int)(sizeof rows / sizeof rows[0]), nan_eq)
#define D(v) hse_db(v)
#define DNAN 0x7ff8000000000000ull
#define DINF 0x7ff0000000000000ull
#define DNINF 0xfff0000000000000ull
#define DNEED() do { HSE_NEED(); if (hse.dev && !doubles_ok(NULL)) { t_skip("D3D11_FEATURE_DOUBLES not supported"); return; } } while (0)

TEST(hlsl_semantics_exec_dbl_feature_query_observed) {
    HSE_NEED(); HSE_RUN();
    int ext = 0, ok = doubles_ok(&ext);
    t_note("DoublePrecisionFloatShaderOps=%d ExtendedDoublesShaderInstructions=%d", ok, ext);
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT mp; memset(&mp, 0, sizeof mp);
    if (SUCCEEDED(ID3D11Device_CheckFeatureSupport(hse.dev, D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &mp, sizeof mp)))
        t_note("min precision: pixel=%x other stages=%x", mp.PixelShaderMinPrecision, mp.AllOtherShaderStagesMinPrecision);
    if (hse.fl >= D3D_FEATURE_LEVEL_11_0) CHECK(ok);   /* mandatory for feature level 11_0 ... recorded as failure only when absent on an 11_x device */
}
TEST(hlsl_semantics_exec_dbl_asdouble_asuint_roundtrip) {
    DNEED();
    drow r[] = { { D(1.5), D(0), D(1.5) }, { D(-2.25), 0, D(-2.25) }, { 0x0000000100000002ull, 0, 0x0000000100000002ull }, { D(0), 0, D(0) }, { 0x8000000000000000ull, 0, 0x8000000000000000ull },
                 { DINF, 0, DINF }, { 0x0000000000000001ull, 0, 0x0000000000000001ull }, { 0x7ff0000000000001ull, 0, 0x7ff0000000000001ull } };
    /* signalling NaN payloads may be quieted: compare NaNs loosely */
    DBL_RUN("", "x", 0, r, 1);
}
TEST(hlsl_semantics_exec_dbl_add) {
    DNEED();
    drow r[] = { { D(1.5), D(2.25), D(3.75) }, { D(1e300), D(1e300), D(2e300) }, { D(0.1), D(0.2), D(0.1 + 0.2) }, { D(1.0), D(1.1102230246251565e-16), D(1.0) },
                 { D(1.0), D(2.220446049250313e-16), D(1.0000000000000002) }, { D(9007199254740992.0), D(1.0), D(9007199254740992.0) }, { D(9007199254740992.0), D(3.0), D(9007199254740996.0) },
                 { DINF, D(1.0), DINF }, { DINF, DNINF, DNAN }, { DNAN, D(1.0), DNAN }, { D(1.7976931348623157e308), D(1.7976931348623157e308), DINF } };
    DBL_RUN("", "x + y", 0, r, 1);
}
TEST(hlsl_semantics_exec_dbl_add_denormals_preserved) {
    DNEED();
    drow r[] = { { 1, 1, 2 }, { 0x000fffffffffffffull, 1, 0x0010000000000000ull }, { 0x0010000000000000ull, 0x8000000000000001ull, 0x000fffffffffffffull }, { 1, 0x8000000000000001ull, 0 } };
    DBL_RUN("", "x + y", 0, r, 0);
}
TEST(hlsl_semantics_exec_dbl_mul) {
    DNEED();
    drow r[] = { { D(3.0), D(4.0), D(12.0) }, { D(-3.0), D(4.0), D(-12.0) }, { D(1e200), D(1e200), DINF }, { D(0.5), D(0.5), D(0.25) }, { D(0.1), D(3.0), D(0.1 * 3.0) },
                 { DINF, D(0.0), DNAN }, { D(0.0), D(-5.0), 0x8000000000000000ull }, { D(1e-200), D(1e-200), D(0.0) }, { D(1e-160), D(1e-160), D(1e-320) } };
    DBL_RUN("", "x * y", 0, r, 1);
}
TEST(hlsl_semantics_exec_dbl_sub_neg) {
    DNEED();
    drow r[] = { { D(5.0), D(3.0), D(2.0) }, { D(3.0), D(5.0), D(-2.0) }, { D(1.0), D(1.0), D(0.0) }, { DINF, DINF, DNAN } };
    DBL_RUN("", "x - y", 0, r, 1);
    drow n[] = { { D(2.0), 0, D(-2.0) }, { D(-0.0), 0, D(0.0) }, { D(0.0), 0, 0x8000000000000000ull }, { DINF, 0, DNINF } };
    DBL_RUN("", "-x", 0, n, 0);
}
TEST(hlsl_semantics_exec_dbl_min_max) {
    DNEED();
    drow mn[] = { { D(1.0), D(2.0), D(1.0) }, { D(2.0), D(1.0), D(1.0) }, { D(-1.0), D(1.0), D(-1.0) }, { DNAN, D(1.0), D(1.0) }, { D(1.0), DNAN, D(1.0) }, { DNINF, DINF, DNINF } };
    DBL_RUN("", "min(x, y)", 0, mn, 1);
    drow mx[] = { { D(1.0), D(2.0), D(2.0) }, { D(2.0), D(1.0), D(2.0) }, { D(-1.0), D(1.0), D(1.0) }, { DNAN, D(1.0), D(1.0) }, { D(1.0), DNAN, D(1.0) }, { DNINF, DINF, DINF } };
    DBL_RUN("", "max(x, y)", 0, mx, 1);
}
TEST(hlsl_semantics_exec_dbl_comparisons) {
    DNEED();
    static const double V[] = { 0.0, -0.0, 1.0, -1.0, 1.5, 1e300, -1e300 };
    drow lt[49], le[49], eq[49], ne[49], ge[49]; int n = 0;
    for (int i = 0; i < 7; i++) for (int j = 0; j < 7; j++, n++) {
        double a = V[i], b = V[j];
        lt[n].x = le[n].x = eq[n].x = ne[n].x = ge[n].x = D(a); lt[n].y = le[n].y = eq[n].y = ne[n].y = ge[n].y = D(b);
        lt[n].want = a < b; le[n].want = a <= b; eq[n].want = a == b; ne[n].want = a != b; ge[n].want = a >= b;
    }
    dbl_run_at(__FILE__, __LINE__, "", "x < y ? 1u : 0u", 1, lt, n, 0);
    dbl_run_at(__FILE__, __LINE__, "", "x <= y ? 1u : 0u", 1, le, n, 0);
    dbl_run_at(__FILE__, __LINE__, "", "x == y ? 1u : 0u", 1, eq, n, 0);
    dbl_run_at(__FILE__, __LINE__, "", "x != y ? 1u : 0u", 1, ne, n, 0);
    dbl_run_at(__FILE__, __LINE__, "", "x >= y ? 1u : 0u", 1, ge, n, 0);
}
TEST(hlsl_semantics_exec_dbl_comparisons_nan) {
    DNEED();
    drow r[] = { { DNAN, D(1.0), 0 }, { D(1.0), DNAN, 0 }, { DNAN, DNAN, 0 } };
    dbl_run_at(__FILE__, __LINE__, "", "((x < y) ? 1u : 0u) | ((x <= y) ? 2u : 0u) | ((x == y) ? 4u : 0u) | ((x >= y) ? 8u : 0u) | ((x > y) ? 16u : 0u)", 1, r, 3, 0);
    drow ne[] = { { DNAN, D(1.0), 1 }, { DNAN, DNAN, 1 } };
    dbl_run_at(__FILE__, __LINE__, "", "x != y ? 1u : 0u", 1, ne, 2, 0);
}
TEST(hlsl_semantics_exec_dbl_float_conversions) {
    DNEED();
    static const float fv[] = { 1.5f, -2.0f, 0.1f, 3.4028235e38f };
    drow r[4];
    for (int i = 0; i < 4; i++) { r[i].x = FB(fv[i]); r[i].y = 0; r[i].want = D((double)fv[i]); }
    dbl_run_at(__FILE__, __LINE__, "float f = asfloat(w.x);", "(double)f", 0, r, 4, 0);   /* float -> double is exact */
    drow d2f[] = { { D(1.5), 0, FB(1.5f) }, { D(0.1), 0, FB(0.1f) }, { D(1e300), 0, F_INF }, { D(-1e300), 0, F_NINF }, { D(16777217.0), 0, FB(16777216.0f) }, { D(16777219.0), 0, FB(16777220.0f) },
                   { DINF, 0, F_INF }, { DNINF, 0, F_NINF }, { D(3.0000001192092896), 0, FB(3.0000002f) } };
    dbl_run_at(__FILE__, __LINE__, "", "asuint((float)x)", 1, d2f, 9, 0);   /* double -> float rounds to nearest even */
}
TEST(hlsl_semantics_exec_dbl_float_to_double_nan_inf) {
    DNEED();
    drow r[] = { { F_INF, 0, DINF }, { F_NINF, 0, DNINF }, { F_NAN, 0, DNAN } };
    dbl_run_at(__FILE__, __LINE__, "float f = asfloat(w.x);", "(double)f", 0, r, 3, 1);
}
TEST(hlsl_semantics_exec_dbl_extended_division_rcp) {
    HSE_NEED();
    int ext = 0;
    if (hse.dev) { if (!doubles_ok(&ext)) { t_skip("doubles unsupported"); return; } if (!ext) { t_skip("extended double instructions (ddiv/drcp/dfma) unsupported"); return; } }
    drow dv[] = { { D(6.0), D(3.0), D(2.0) }, { D(1.0), D(4.0), D(0.25) }, { D(1.0), D(0.0), DINF }, { D(-1.0), D(0.0), DNINF }, { D(0.0), D(0.0), DNAN }, { DINF, DINF, DNAN }, { D(1.0), DINF, D(0.0) },
                  { D(10.0), D(4.0), D(2.5) } };
    DBL_RUN("", "x / y", 0, dv, 1);
    drow rc[] = { { D(2.0), 0, D(0.5) }, { D(-8.0), 0, D(-0.125) }, { D(0.0), 0, DINF }, { DINF, 0, D(0.0) }, { DNAN, 0, DNAN } };
    DBL_RUN("", "rcp(x)", 0, rc, 1);
}
TEST(hlsl_semantics_exec_dbl_extended_fma_is_fused) {
    HSE_NEED();
    int ext = 0;
    if (hse.dev) { if (!doubles_ok(&ext)) { t_skip("doubles unsupported"); return; } if (!ext) { t_skip("extended double instructions unsupported"); return; } }
    /* x*x = 1 + 2^-26 + 2^-54 exactly; y = -(1 + 2^-26): the fused result keeps 2^-54, an unfused one gives 0 */
    double x = 1.0 + 7.450580596923828e-09;   /* 1 + 2^-27 */
    drow r[] = { { D(x), D(-(1.0 + 1.4901161193847656e-08)), D(5.551115123125783e-17) }, { D(2.0), D(3.0), D(7.0) } };
    char src[1024];
    snprintf(src, sizeof src, "ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n[numthreads(2,1,1)] void main(uint3 id : SV_DispatchThreadID) {\n"
             "  uint4 w = I.Load4(id.x * 16); double x = asdouble(w.x, w.y), y = asdouble(w.z, w.w); double r = fma(x, id.x == 0 ? x : 2.0, id.x == 0 ? y : 1.0); uint lo, hi; asuint(r, lo, hi); O.Store2(id.x * 8, uint2(lo, hi)); }");
    uint64_t out[2] = { 0 };
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = src; c.in = r; c.inbytes = sizeof r; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    EXPECT_EQ(out[0], D(5.551115123125783e-17));   /* 2^-54 */
    EXPECT_EQ(out[1], D(5.0));
}

/* ---- minimum precision types --------------------------------------------------------------------------------------------------- */
HSE_T(minprec_exact_float_arith_same_either_way, "min16float x = (min16float)fa, y = (min16float)fb; min16float z = x * y + (min16float)fc;", "(float)z", HSE_EXACT, 0,
      { FB(1.5f), FB(2.5f), FB(0.25f), FB(4.0f) }, { FB(-2.0f), FB(8.0f), FB(1.0f), FB(-15.0f) }, { FB(0.5f), FB(0.5f), FB(0.0f), FB(0.25f) })
HSE_T(minprec_float_precision_16_or_32, "min16float x = (min16float)fa;", "(float)x", HSE_EXACT | HSE_SOFT, 0,
      { 0x3f800800u, 0, 0, FB(1.0f) }, { FB(0.1f), 0, 0, 0x3dcccccdu })
HSE_T(minprec_float_value_is_one_of_two_precisions, "min16float x = (min16float)fa; uint r = asuint((float)x);", "(r == asuint(fa) || r == 0x3f800000u) ? 1u : 0u", HSE_EXACT, 0,
      { 0x3f800800u, 0, 0, 1 }, { 0x3f800000u, 0, 0, 1 })
HSE_T(minprec_float_overflow_observed, "min16float x = (min16float)fa;", "(float)x", HSE_EXACT | HSE_SOFT, 0, { FB(70000.0f), 0, 0, F_INF })
HSE_T(minprec_float_overflow_is_inf_or_value, "min16float x = (min16float)fa; uint r = asuint((float)x);", "(r == 0x7f800000u || r == asuint(fa)) ? 1u : 0u", HSE_EXACT, 0, { FB(70000.0f), 0, 0, 1 })
HSE_T(minprec_int_wrap_observed, "min16int x = (min16int)ia;", "(int)x", HSE_EXACT | HSE_SOFT, 0, { 40000, 0, 0, (uint32_t)-25536 }, { 70000, 0, 0, 4464 })
HSE_T(minprec_int_value_is_one_of_two_precisions, "min16int x = (min16int)ia; int r = (int)x;", "(r == ia || r == ((ia << 16) >> 16)) ? 1u : 0u", HSE_EXACT, 0, { 40000, 0, 0, 1 }, { 5, 0, 0, 1 }, { (uint32_t)-7, 0, 0, 1 })
HSE_T(minprec_uint_value_is_one_of_two_precisions, "min16uint x = (min16uint)ua; uint r = (uint)x;", "(r == ua || r == (ua & 0xffff)) ? 1u : 0u", HSE_EXACT, 0, { 70000, 0, 0, 1 }, { 5, 0, 0, 1 }, { 65535, 0, 0, 1 })
HSE_T(minprec_small_ints_exact, "min16int x = (min16int)ia, y = (min16int)ib;", "(int)(x + y * 2)", HSE_EXACT, 0, { 1, 2, 0, 5 }, { (uint32_t)-3, 4, 0, 5 }, { 100, (uint32_t)-50, 0, 0 })
HSE_T(minprec_min16uint_small_exact, "min16uint x = (min16uint)ua, y = (min16uint)ub;", "(uint)(x * y)", HSE_EXACT, 0, { 7, 8, 0, 56 }, { 255, 255, 0, 65025 }, { 0, 5, 0, 0 })
HSE_T(minprec_half_maps_to_float_observed, "half h = fa;", "(float)h", HSE_EXACT | HSE_SOFT, 0, { 0x3f800800u, 0, 0, 0x3f800800u })
HSE_T(minprec_min10float_value_observed, "min10float h = (min10float)fa;", "(float)h", HSE_EXACT | HSE_SOFT, 0, { FB(1.5f), 0, 0, FB(1.5f) }, { FB(0.1f), 0, 0, FB(0.1f) })
TEST(hlsl_semantics_exec_minprec_cbuffer_layout_observed) {
    static const lv v[] = { { "a", 0, "(float)a" }, { "b", 4, "(float)b" }, { "c", 8, "(float)c" } };
    HSE_NEED();
    UINT f = 0;
    (void)f;
    NOTE_LAYOUT("min16float a; min16float b; float c;", v, 16);
}
