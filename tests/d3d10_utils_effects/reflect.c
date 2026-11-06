/* D3D10ReflectShader and the ID3D10ShaderReflection object tree. */
#include "ue.h"
#include <d3d11shader.h>

#define E_INVALIDARG_ ((HRESULT)0x80070057u)
#define E_FAIL_       ((HRESULT)0x80004005u)

static const char RF_SRC[] =
    "cbuffer cbA : register(b1) { float4 a; float3 b; float c; float4x4 m; float arr[3]; float unused_in_a; };\n"
    "cbuffer cbB { int2 iv; uint u; bool flag; };\n"
    "Texture2D t0 : register(t0);\n"
    "SamplerState s0 : register(s0);\n"
    "float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target\n"
    "{\n"
    "    float4 r = t0.Sample(s0, uv) * a + float4(b, c) + mul(p, m) + arr[1] + arr[2];\n"
    "    if (flag) r += float4(iv.x, iv.y, u, 0);\n"
    "    return r;\n"
    "}\n";

typedef HRESULT (WINAPI *refl_fn)(const void *, SIZE_T, ID3D10ShaderReflection **);
#define REFL(dll) ((refl_fn)t_proc_in(dll, "D3D10ReflectShader"))
static const char *const k_who[2] = { "d3d10", "d3dcompiler_47" };

static HRESULT make_rf(int who, const char *src, const char *profile, ID3D10Blob **out)
{
    if (who == 0) return ue_compile_vs(src, "main", profile, out);
    return ue_d3dc(src, "main", profile, 0, out);
}

typedef struct { ID3D10Blob *sh; ID3D10ShaderReflection *r; } rf_t;
static int rf_open(rf_t *x, int who, const char *src, const char *profile)
{
    memset(x, 0, sizeof *x);
    if (!REFL("d3d10.dll")) { t_skip("D3D10ReflectShader missing"); return 0; }
    HRESULT hr = make_rf(who, src, profile, &x->sh);
    if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "shader compile", "%s %s: %s", k_who[who], profile, d3dh_hr_name(hr)); return 0; }
    hr = REFL("d3d10.dll")(ue_bp(x->sh), ue_bs(x->sh), &x->r);
    if (FAILED(hr) || !x->r) { t_check(0, __FILE__, __LINE__, "reflect", "%s %s: %s", k_who[who], profile, d3dh_hr_name(hr)); UE_REL(x->sh); return 0; }
    return 1;
}
static void rf_close(rf_t *x) { UE_REL(x->r); UE_REL(x->sh); }

static int find_binding(ID3D10ShaderReflection *r, UINT n, const char *name, D3D10_SHADER_INPUT_BIND_DESC *out)
{
    for (UINT i = 0; i < n; i++) {
        D3D10_SHADER_INPUT_BIND_DESC d;
        if (SUCCEEDED(ID3D10ShaderReflection_GetResourceBindingDesc(r, i, &d)) && d.Name && !strcmp(d.Name, name)) { *out = d; return 1; }
    }
    return 0;
}
static int find_cb(ID3D10ShaderReflection *r, UINT n, const char *name, ID3D10ShaderReflectionConstantBuffer **out)
{
    for (UINT i = 0; i < n; i++) {
        ID3D10ShaderReflectionConstantBuffer *cb = ID3D10ShaderReflection_GetConstantBufferByIndex(r, i);
        D3D10_SHADER_BUFFER_DESC d;
        if (cb && SUCCEEDED(ID3D10ShaderReflectionConstantBuffer_GetDesc(cb, &d)) && !strcmp(d.Name, name)) { *out = cb; return 1; }
    }
    return 0;
}

#define FOR_BOTH(x, src, prof) for (int who = 0; who < 2; who++) for (int once = 1; once; once = 0) \
    for (rf_t x; once && rf_open(&x, who, src, prof); rf_close(&x), once = 0)

/* ---- creation ---- */
TEST(d3d10_utils_effects_reflect_null_args)
{
    REQUIRE(REFL("d3d10.dll"));
    NOWINE("reflection is a stub");
    ID3D10Blob *sh = NULL;
    if (FAILED(make_rf(0, RF_SRC, "ps_4_0", &sh))) { CHECK(0); return; }
    ID3D10ShaderReflection *r = (ID3D10ShaderReflection *)(ULONG_PTR)1;
    EXPECT_HR(REFL("d3d10.dll")(NULL, 100, &r), E_INVALIDARG_);
    EXPECT_HR(REFL("d3d10.dll")(ue_bp(sh), ue_bs(sh), NULL), E_INVALIDARG_);
    EXPECT_HR(REFL("d3d10.dll")(NULL, 0, NULL), E_INVALIDARG_);
    UE_REL(sh);
}
TEST(d3d10_utils_effects_reflect_garbage_data)
{
    REQUIRE(REFL("d3d10.dll"));
    unsigned char junk[256];
    for (int i = 0; i < 256; i++) junk[i] = (unsigned char)(i * 11 + 5);
    ID3D10ShaderReflection *r = (ID3D10ShaderReflection *)(ULONG_PTR)1;
    HRESULT hr = REFL("d3d10.dll")(junk, sizeof junk, &r);
    EXPECT_HR(hr, E_INVALIDARG_);
    EXPECT_EQ(r, NULL);
    memset(junk, 0, sizeof junk);
    r = (ID3D10ShaderReflection *)(ULONG_PTR)1;
    EXPECT_HR(REFL("d3d10.dll")(junk, sizeof junk, &r), E_INVALIDARG_);
    EXPECT_EQ(r, NULL);
}
TEST(d3d10_utils_effects_reflect_truncated_container)
{
    REQUIRE(REFL("d3d10.dll"));
    ID3D10Blob *sh = NULL;
    if (FAILED(make_rf(0, RF_SRC, "ps_4_0", &sh))) { CHECK(0); return; }
    SIZE_T n = ue_bs(sh);
    static const SIZE_T cuts[] = { 0, 4, 31, 32, 36, 64 };
    for (unsigned i = 0; i < sizeof cuts / sizeof *cuts; i++) {
        ID3D10ShaderReflection *r = NULL;
        HRESULT hr = REFL("d3d10.dll")(ue_bp(sh), cuts[i], &r);
        t_check(hr == E_INVALIDARG_ && !r, __FILE__, __LINE__, "truncated", "size %u: %s", (unsigned)cuts[i], d3dh_hr_name(hr));
        UE_REL(r);
    }
    ID3D10ShaderReflection *r = NULL;
    HRESULT hr = REFL("d3d10.dll")(ue_bp(sh), n - 1, &r);
    CHECK(FAILED(hr) && !r);
    UE_REL(r); UE_REL(sh);
}
TEST(d3d10_utils_effects_reflect_via_d3d10_1)
{
    REQUIRE(REFL("d3d10_1.dll"));
    NOWINE("reflection is a stub");
    ID3D10Blob *sh = NULL;
    if (FAILED(make_rf(0, RF_SRC, "ps_4_0", &sh))) { CHECK(0); return; }
    ID3D10ShaderReflection *r = NULL;
    EXPECT_HR(REFL("d3d10_1.dll")(ue_bp(sh), ue_bs(sh), &r), S_OK);
    CHECK(r != NULL);
    UE_REL(r);
    r = (ID3D10ShaderReflection *)(ULONG_PTR)1;
    EXPECT_HR(REFL("d3d10_1.dll")(NULL, 0, &r), E_INVALIDARG_);
    UE_REL(sh);
}

/* ---- IUnknown ---- */
TEST(d3d10_utils_effects_reflect_refcount)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        EQV(ID3D10ShaderReflection_AddRef(x.r), 2);
        EQV(ID3D10ShaderReflection_AddRef(x.r), 3);
        EQV(ID3D10ShaderReflection_Release(x.r), 2);
        EQV(ID3D10ShaderReflection_Release(x.r), 1);
    }
}
TEST(d3d10_utils_effects_reflect_qi)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        void *o = NULL;
        EXPECT_HR(ID3D10ShaderReflection_QueryInterface(x.r, &IID_ID3D10ShaderReflection, &o), S_OK);
        EXPECT_EQ(o, x.r);
        EQV(ID3D10ShaderReflection_Release(x.r), 1);
        o = NULL;
        EXPECT_HR(ID3D10ShaderReflection_QueryInterface(x.r, &IID_IUnknown, &o), S_OK);
        EXPECT_EQ(o, x.r);
        EQV(ID3D10ShaderReflection_Release(x.r), 1);
        o = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D10ShaderReflection_QueryInterface(x.r, &IID_ID3D10Device, &o), E_NOINTERFACE);
        EXPECT_EQ(o, NULL);
        o = (void *)(ULONG_PTR)1;
        EXPECT_HR(ID3D10ShaderReflection_QueryInterface(x.r, &IID_ID3D10Blob, &o), E_NOINTERFACE);
        EXPECT_EQ(o, NULL);
    }
}
TEST(d3d10_utils_effects_reflect_qi_null_out)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        if (t_is_wine()) { t_skip("emulator"); break; }
        EXPECT_HR(ID3D10ShaderReflection_QueryInterface(x.r, &IID_ID3D10ShaderReflection, NULL), E_FAIL_);
    }
}

/* ---- shader desc ---- */
TEST(d3d10_utils_effects_reflect_desc_ps_counts)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_DESC d;
        memset(&d, 0xcd, sizeof d);
        EXPECT_HR(ID3D10ShaderReflection_GetDesc(x.r, &d), S_OK);
        t_check(d.ConstantBuffers == 2, __FILE__, __LINE__, "ConstantBuffers", "%s: %u", k_who[who], d.ConstantBuffers);
        t_check(d.BoundResources == 4, __FILE__, __LINE__, "BoundResources", "%s: %u", k_who[who], d.BoundResources);
        t_check(d.InputParameters == 2, __FILE__, __LINE__, "InputParameters", "%s: %u", k_who[who], d.InputParameters);
        t_check(d.OutputParameters == 1, __FILE__, __LINE__, "OutputParameters", "%s: %u", k_who[who], d.OutputParameters);
        CHECK(d.InstructionCount > 4);
        CHECK(d.Creator != NULL);
        EXPECT_EQ(d.TextureNormalInstructions, 1);
        EXPECT_EQ(d.GSMaxOutputVertexCount, 0);
    }
}
TEST(d3d10_utils_effects_reflect_desc_version_matches_token)
{
    static const char *const prof[3] = { "vs_4_0", "ps_4_0", "gs_4_0" };
    if (t_is_wine()) { t_skip("emulator GS"); }
    for (int st = 0; st < 2; st++) {
        FOR_BOTH(x, st ? RF_SRC : UE_VS_SRC, prof[st]) {
            D3D10_SHADER_DESC d;
            EXPECT_HR(ID3D10ShaderReflection_GetDesc(x.r, &d), S_OK);
            DWORD csz;
            const unsigned char *c = ue_dxbc_chunk(ue_bp(x.sh), ue_bs(x.sh), FOURCC('S','H','D','R'), &csz);
            if (c) t_check(d.Version == *(const DWORD *)c, __FILE__, __LINE__, "Version == SHDR token", "%s %s: %08x vs %08x", k_who[who], prof[st], d.Version, *(const DWORD *)c);
        }
    }
}
TEST(d3d10_utils_effects_reflect_desc_vs_counts)
{
    FOR_BOTH(x, UE_VS_SRC, "vs_4_0") {
        D3D10_SHADER_DESC d;
        EXPECT_HR(ID3D10ShaderReflection_GetDesc(x.r, &d), S_OK);
        t_check(d.ConstantBuffers == 1 && d.BoundResources == 1 && d.InputParameters == 3 && d.OutputParameters == 3, __FILE__, __LINE__, "VS counts", "%s: cb %u res %u in %u out %u", k_who[who], d.ConstantBuffers, d.BoundResources, d.InputParameters, d.OutputParameters);
    }
}
TEST(d3d10_utils_effects_reflect_desc_gs)
{
    if (t_is_wine()) { t_skip("emulator HLSL front end lacks geometry shaders"); return; }
    FOR_BOTH(x, UE_GS_SRC, "gs_4_0") {
        D3D10_SHADER_DESC d;
        EXPECT_HR(ID3D10ShaderReflection_GetDesc(x.r, &d), S_OK);
        EXPECT_EQ(d.GSMaxOutputVertexCount, 3);
        EXPECT_EQ(d.GSOutputTopology, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        CHECK(d.EmitInstructionCount >= 1);
        EXPECT_EQ(d.InputParameters, 1);
        EXPECT_EQ(d.OutputParameters, 1);
    }
}
TEST(d3d10_utils_effects_reflect_desc_no_resources)
{
    const char *src = "float4 main(float4 p : SV_Position) : SV_Target { return p * 2; }\n";
    FOR_BOTH(x, src, "ps_4_0") {
        D3D10_SHADER_DESC d;
        EXPECT_HR(ID3D10ShaderReflection_GetDesc(x.r, &d), S_OK);
        EXPECT_EQ(d.ConstantBuffers, 0);
        EXPECT_EQ(d.BoundResources, 0);
        EXPECT_EQ(d.InputParameters, 1);
        EXPECT_EQ(d.OutputParameters, 1);
    }
}
TEST(d3d10_utils_effects_reflect_unused_resources_dropped)
{
    const char *src = "cbuffer unused_cb { float4 zz; };\nTexture2D unused_tex;\nSamplerState unused_smp;\nfloat4 main() : SV_Target { return float4(1,2,3,4); }\n";
    FOR_BOTH(x, src, "ps_4_0") {
        D3D10_SHADER_DESC d;
        EXPECT_HR(ID3D10ShaderReflection_GetDesc(x.r, &d), S_OK);
        EXPECT_EQ(d.ConstantBuffers, 0);
        EXPECT_EQ(d.BoundResources, 0);
    }
}
TEST(d3d10_utils_effects_reflect_desc_same_creator_pointer_stable)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_DESC d1, d2;
        EXPECT_HR(ID3D10ShaderReflection_GetDesc(x.r, &d1), S_OK);
        EXPECT_HR(ID3D10ShaderReflection_GetDesc(x.r, &d2), S_OK);
        EXPECT_EQ(d1.Version, d2.Version);
        EXPECT_EQ(d1.Creator, d2.Creator);
        EXPECT_EQ(d1.ConstantBuffers, d2.ConstantBuffers);
        EXPECT_EQ(d1.InstructionCount, d2.InstructionCount);
        EXPECT_EQ(d1.TempRegisterCount, d2.TempRegisterCount);
    }
}

/* ---- bound resources ---- */
TEST(d3d10_utils_effects_reflect_resource_texture)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_DESC sd; D3D10_SHADER_INPUT_BIND_DESC b;
        ID3D10ShaderReflection_GetDesc(x.r, &sd);
        if (!find_binding(x.r, sd.BoundResources, "t0", &b)) { t_check(0, __FILE__, __LINE__, "t0 found", "%s", k_who[who]); continue; }
        EXPECT_EQ(b.Type, D3D_SIT_TEXTURE);
        EXPECT_EQ(b.BindPoint, 0);
        EXPECT_EQ(b.BindCount, 1);
        EXPECT_EQ(b.ReturnType, 5);       /* float */
        EXPECT_EQ(b.Dimension, 4);        /* TEXTURE2D */
        EXPECT_EQ(b.NumSamples, who == 0 ? 0u : 0xffffffffu);   /* D3D10CompileShader: 0; d3dcompiler_47: -1 */
    }
}
TEST(d3d10_utils_effects_reflect_resource_sampler)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_DESC sd; D3D10_SHADER_INPUT_BIND_DESC b;
        ID3D10ShaderReflection_GetDesc(x.r, &sd);
        if (!find_binding(x.r, sd.BoundResources, "s0", &b)) { t_check(0, __FILE__, __LINE__, "s0 found", "%s", k_who[who]); continue; }
        EXPECT_EQ(b.Type, D3D_SIT_SAMPLER);
        EXPECT_EQ(b.BindPoint, 0);
        EXPECT_EQ(b.BindCount, 1);
        EXPECT_EQ(b.uFlags & D3D_SIF_COMPARISON_SAMPLER, 0);
    }
}
TEST(d3d10_utils_effects_reflect_resource_cbuffers)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_DESC sd; D3D10_SHADER_INPUT_BIND_DESC b;
        ID3D10ShaderReflection_GetDesc(x.r, &sd);
        if (find_binding(x.r, sd.BoundResources, "cbA", &b)) { EXPECT_EQ(b.Type, D3D_SIT_CBUFFER); EXPECT_EQ(b.BindPoint, 1); EXPECT_EQ(b.BindCount, 1); }
        else t_check(0, __FILE__, __LINE__, "cbA found", "%s", k_who[who]);
        if (find_binding(x.r, sd.BoundResources, "cbB", &b)) { EXPECT_EQ(b.Type, D3D_SIT_CBUFFER); EXPECT_EQ(b.BindPoint, 0); }
        else t_check(0, __FILE__, __LINE__, "cbB found", "%s", k_who[who]);
    }
}
TEST(d3d10_utils_effects_reflect_resource_out_of_range)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_INPUT_BIND_DESC b;
        HRESULT hr = ID3D10ShaderReflection_GetResourceBindingDesc(x.r, 4, &b);
        t_check(FAILED(hr), __FILE__, __LINE__, "index == count", "%s: %s", k_who[who], d3dh_hr_name(hr));
        hr = ID3D10ShaderReflection_GetResourceBindingDesc(x.r, 1000, &b);
        CHECK(FAILED(hr));
        hr = ID3D10ShaderReflection_GetResourceBindingDesc(x.r, 0xffffffffu, &b);
        CHECK(FAILED(hr));
    }
}
TEST(d3d10_utils_effects_reflect_resource_names_unique_and_complete)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_DESC sd;
        ID3D10ShaderReflection_GetDesc(x.r, &sd);
        int got[4] = {0};
        static const char *const names[4] = { "t0", "s0", "cbA", "cbB" };
        for (UINT i = 0; i < sd.BoundResources; i++) {
            D3D10_SHADER_INPUT_BIND_DESC b;
            if (FAILED(ID3D10ShaderReflection_GetResourceBindingDesc(x.r, i, &b))) { CHECK(0); continue; }
            for (int k = 0; k < 4; k++) if (!strcmp(b.Name, names[k])) got[k]++;
        }
        for (int k = 0; k < 4; k++) t_check(got[k] == 1, __FILE__, __LINE__, "resource listed once", "%s: %s x%d", k_who[who], names[k], got[k]);
    }
}

/* ---- constant buffers ---- */
TEST(d3d10_utils_effects_reflect_cb_desc_cbA)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *cb = NULL;
        D3D10_SHADER_DESC sd; D3D10_SHADER_BUFFER_DESC d;
        ID3D10ShaderReflection_GetDesc(x.r, &sd);
        if (!find_cb(x.r, sd.ConstantBuffers, "cbA", &cb)) { t_check(0, __FILE__, __LINE__, "cbA", "%s", k_who[who]); continue; }
        EXPECT_HR(ID3D10ShaderReflectionConstantBuffer_GetDesc(cb, &d), S_OK);
        EXPECT_EQ(d.Type, D3D_CT_CBUFFER);
        EXPECT_EQ(d.Variables, 6);
        t_check(d.Size == 144, __FILE__, __LINE__, "size rounded to 16", "%s: %u", k_who[who], d.Size);
    }
}
TEST(d3d10_utils_effects_reflect_cb_desc_cbB)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *cb = NULL;
        D3D10_SHADER_DESC sd; D3D10_SHADER_BUFFER_DESC d;
        ID3D10ShaderReflection_GetDesc(x.r, &sd);
        if (!find_cb(x.r, sd.ConstantBuffers, "cbB", &cb)) { t_check(0, __FILE__, __LINE__, "cbB", "%s", k_who[who]); continue; }
        EXPECT_HR(ID3D10ShaderReflectionConstantBuffer_GetDesc(cb, &d), S_OK);
        EXPECT_EQ(d.Variables, 3);
        EXPECT_EQ(d.Size, 16);
    }
}
TEST(d3d10_utils_effects_reflect_cb_by_name)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *a = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "cbA");
        D3D10_SHADER_BUFFER_DESC d;
        CHECK(a != NULL);
        if (a) { EXPECT_HR(ID3D10ShaderReflectionConstantBuffer_GetDesc(a, &d), S_OK); EXPECT_STR(d.Name, "cbA"); }
        ID3D10ShaderReflectionConstantBuffer *again = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "cbA");
        EXPECT_EQ(again, a);
    }
}
TEST(d3d10_utils_effects_reflect_cb_name_case_sensitive)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_BUFFER_DESC d;
        ID3D10ShaderReflectionConstantBuffer *c = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "CBA");
        CHECK(c != NULL);
        if (c) CHECK(FAILED(ID3D10ShaderReflectionConstantBuffer_GetDesc(c, &d)));
    }
}
TEST(d3d10_utils_effects_reflect_cb_invalid_lookups)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SHADER_BUFFER_DESC d;
        ID3D10ShaderReflectionConstantBuffer *c1 = ID3D10ShaderReflection_GetConstantBufferByIndex(x.r, 2);
        ID3D10ShaderReflectionConstantBuffer *c2 = ID3D10ShaderReflection_GetConstantBufferByIndex(x.r, 0xffffffffu);
        ID3D10ShaderReflectionConstantBuffer *c3 = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "nosuch");
        ID3D10ShaderReflectionConstantBuffer *c4 = ID3D10ShaderReflection_GetConstantBufferByName(x.r, NULL);
        t_check(c1 && c2 && c3, __FILE__, __LINE__, "never NULL", "%s", k_who[who]);
        if (c1) CHECK(FAILED(ID3D10ShaderReflectionConstantBuffer_GetDesc(c1, &d)));
        if (c2) CHECK(FAILED(ID3D10ShaderReflectionConstantBuffer_GetDesc(c2, &d)));
        if (c3) CHECK(FAILED(ID3D10ShaderReflectionConstantBuffer_GetDesc(c3, &d)));
        if (c4) CHECK(FAILED(ID3D10ShaderReflectionConstantBuffer_GetDesc(c4, &d)));
        /* all invalid results are the same shared object */
        if (c1 && c3) EXPECT_EQ(c1, c3);
    }
}

typedef struct { const char *name; UINT off, size; UINT cls, type, rows, cols, elems; } varexp;
static void check_vars(ID3D10ShaderReflection *r, const char *cbname, const varexp *v, int n, int who)
{
    D3D10_SHADER_DESC sd;
    ID3D10ShaderReflectionConstantBuffer *cb = NULL;
    ID3D10ShaderReflection_GetDesc(r, &sd);
    if (!find_cb(r, sd.ConstantBuffers, cbname, &cb)) { t_check(0, __FILE__, __LINE__, "cb found", "%s %s", k_who[who], cbname); return; }
    for (int i = 0; i < n; i++) {
        ID3D10ShaderReflectionVariable *var = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, i);
        D3D10_SHADER_VARIABLE_DESC d;
        if (!var || FAILED(ID3D10ShaderReflectionVariable_GetDesc(var, &d))) { t_check(0, __FILE__, __LINE__, "variable desc", "%s %s[%d]", k_who[who], cbname, i); continue; }
        t_check(!strcmp(d.Name, v[i].name), __FILE__, __LINE__, "variable order/name", "%s %s[%d]: %s want %s", k_who[who], cbname, i, d.Name, v[i].name);
        t_check(d.StartOffset == v[i].off, __FILE__, __LINE__, "StartOffset", "%s %s: %u want %u", k_who[who], v[i].name, d.StartOffset, v[i].off);
        t_check(d.Size == v[i].size, __FILE__, __LINE__, "Size", "%s %s: %u want %u", k_who[who], v[i].name, d.Size, v[i].size);
        ID3D10ShaderReflectionType *t = ID3D10ShaderReflectionVariable_GetType(var);
        D3D10_SHADER_TYPE_DESC td;
        if (!t || FAILED(ID3D10ShaderReflectionType_GetDesc(t, &td))) { t_check(0, __FILE__, __LINE__, "type desc", "%s %s", k_who[who], v[i].name); continue; }
        t_check(td.Class == v[i].cls, __FILE__, __LINE__, "Class", "%s %s: %d want %u", k_who[who], v[i].name, td.Class, v[i].cls);
        t_check(td.Type == v[i].type, __FILE__, __LINE__, "Type", "%s %s: %d want %u", k_who[who], v[i].name, td.Type, v[i].type);
        t_check(td.Rows == v[i].rows && td.Columns == v[i].cols, __FILE__, __LINE__, "Rows/Columns", "%s %s: %ux%u", k_who[who], v[i].name, td.Rows, td.Columns);
        /* shaders from D3D10CompileShader report Elements == 1 for non-arrays; d3dcompiler_47 shaders report 0 */
        UINT want_el = (who == 0 && v[i].elems == 0) ? 1 : v[i].elems;
        t_check(td.Elements == want_el, __FILE__, __LINE__, "Elements", "%s %s: %u want %u", k_who[who], v[i].name, td.Elements, want_el);
        EXPECT_EQ(td.Members, 0);
    }
}
TEST(d3d10_utils_effects_reflect_cbA_variables)
{
    static const varexp v[] = {
        { "a", 0, 16, D3D_SVC_VECTOR, D3D_SVT_FLOAT, 1, 4, 0 },
        { "b", 16, 12, D3D_SVC_VECTOR, D3D_SVT_FLOAT, 1, 3, 0 },
        { "c", 28, 4, D3D_SVC_SCALAR, D3D_SVT_FLOAT, 1, 1, 0 },
        { "m", 32, 64, D3D_SVC_MATRIX_COLUMNS, D3D_SVT_FLOAT, 4, 4, 0 },
        { "arr", 96, 36, D3D_SVC_SCALAR, D3D_SVT_FLOAT, 1, 1, 3 },
        { "unused_in_a", 132, 4, D3D_SVC_SCALAR, D3D_SVT_FLOAT, 1, 1, 0 },
    };
    FOR_BOTH(x, RF_SRC, "ps_4_0") check_vars(x.r, "cbA", v, 6, who);
}
TEST(d3d10_utils_effects_reflect_cbB_variables)
{
    static const varexp v[] = {
        { "iv", 0, 8, D3D_SVC_VECTOR, D3D_SVT_INT, 1, 2, 0 },
        { "u", 8, 4, D3D_SVC_SCALAR, D3D_SVT_UINT, 1, 1, 0 },
        { "flag", 12, 4, D3D_SVC_SCALAR, D3D_SVT_BOOL, 1, 1, 0 },
    };
    FOR_BOTH(x, RF_SRC, "ps_4_0") check_vars(x.r, "cbB", v, 3, who);
}
TEST(d3d10_utils_effects_reflect_variable_used_flags)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *cb = NULL;
        D3D10_SHADER_DESC sd;
        ID3D10ShaderReflection_GetDesc(x.r, &sd);
        if (!find_cb(x.r, sd.ConstantBuffers, "cbA", &cb)) { CHECK(0); continue; }
        static const UINT used[6] = { D3D_SVF_USED, D3D_SVF_USED, D3D_SVF_USED, D3D_SVF_USED, D3D_SVF_USED, 0 };
        for (int i = 0; i < 6; i++) {
            D3D10_SHADER_VARIABLE_DESC d;
            ID3D10ShaderReflectionVariable *var = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, i);
            if (var && SUCCEEDED(ID3D10ShaderReflectionVariable_GetDesc(var, &d)))
                t_check(d.uFlags == used[i], __FILE__, __LINE__, "uFlags", "%s %s: %u want %u", k_who[who], d.Name, d.uFlags, used[i]);
            else CHECK(0);
        }
    }
}
TEST(d3d10_utils_effects_reflect_variable_by_name)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *cb = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "cbA");
        D3D10_SHADER_VARIABLE_DESC d;
        ID3D10ShaderReflectionVariable *v = ID3D10ShaderReflectionConstantBuffer_GetVariableByName(cb, "m");
        if (v && SUCCEEDED(ID3D10ShaderReflectionVariable_GetDesc(v, &d))) { EXPECT_STR(d.Name, "m"); EXPECT_EQ(d.StartOffset, 32); }
        else CHECK(0);
        v = ID3D10ShaderReflectionConstantBuffer_GetVariableByName(cb, "nosuch");
        CHECK(v != NULL);
        if (v) CHECK(FAILED(ID3D10ShaderReflectionVariable_GetDesc(v, &d)));
        v = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, 6);
        CHECK(v != NULL);
        if (v) CHECK(FAILED(ID3D10ShaderReflectionVariable_GetDesc(v, &d)));
        v = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, 0xffffffffu);
        CHECK(v != NULL);
    }
}
TEST(d3d10_utils_effects_reflect_variable_default_value_null)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *cb = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "cbA");
        D3D10_SHADER_VARIABLE_DESC d;
        ID3D10ShaderReflectionVariable *v = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, 0);
        if (v && SUCCEEDED(ID3D10ShaderReflectionVariable_GetDesc(v, &d))) EXPECT_EQ(d.DefaultValue, NULL);
    }
}
TEST(d3d10_utils_effects_reflect_variable_type_pointer_stable)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *cb = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "cbA");
        ID3D10ShaderReflectionVariable *v = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, 3);
        if (!v) { CHECK(0); continue; }
        ID3D10ShaderReflectionType *t1 = ID3D10ShaderReflectionVariable_GetType(v), *t2 = ID3D10ShaderReflectionVariable_GetType(v);
        EXPECT_EQ(t1, t2);
    }
}
TEST(d3d10_utils_effects_reflect_row_major_matrix_class)
{
    const char *src = "cbuffer c { row_major float4x4 rm; column_major float4x4 cm; float3x2 d; };\nfloat4 main(float4 p : SV_Position) : SV_Target { return mul(p, rm) + mul(p, cm) + float4(d[0], d[1].x, 0); }\n";
    FOR_BOTH(x, src, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *cb = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "c");
        static const struct { UINT cls, rows, cols; } want[3] = { { D3D_SVC_MATRIX_ROWS, 4, 4 }, { D3D_SVC_MATRIX_COLUMNS, 4, 4 }, { D3D_SVC_MATRIX_COLUMNS, 3, 2 } };
        for (int i = 0; i < 3; i++) {
            ID3D10ShaderReflectionVariable *v = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, i);
            D3D10_SHADER_TYPE_DESC td;
            if (v && SUCCEEDED(ID3D10ShaderReflectionType_GetDesc(ID3D10ShaderReflectionVariable_GetType(v), &td)))
                t_check(td.Class == want[i].cls && td.Rows == want[i].rows && td.Columns == want[i].cols, __FILE__, __LINE__, "matrix class", "%s var %d: class %d %ux%u", k_who[who], i, td.Class, td.Rows, td.Columns);
            else CHECK(0);
        }
    }
}
TEST(d3d10_utils_effects_reflect_struct_member_types)
{
    const char *src = "struct S { float4 p; int2 q; float r; };\ncbuffer c { S s; float tail; };\nfloat4 main() : SV_Target { return s.p + s.q.x + s.r + tail; }\n";
    FOR_BOTH(x, src, "ps_4_0") {
        ID3D10ShaderReflectionConstantBuffer *cb = ID3D10ShaderReflection_GetConstantBufferByName(x.r, "c");
        ID3D10ShaderReflectionVariable *v = ID3D10ShaderReflectionConstantBuffer_GetVariableByIndex(cb, 0);
        D3D10_SHADER_TYPE_DESC td;
        if (!v) { CHECK(0); continue; }
        ID3D10ShaderReflectionType *t = ID3D10ShaderReflectionVariable_GetType(v);
        if (FAILED(ID3D10ShaderReflectionType_GetDesc(t, &td))) { CHECK(0); continue; }
        EXPECT_EQ(td.Class, D3D_SVC_STRUCT);
        EXPECT_EQ(td.Members, 3);
        static const char *const names[3] = { "p", "q", "r" };
        for (UINT i = 0; i < td.Members; i++) {
            const char *nm = ID3D10ShaderReflectionType_GetMemberTypeName(t, i);
            t_check(nm && !strcmp(nm, names[i]), __FILE__, __LINE__, "member name", "%s %u: %s", k_who[who], i, nm ? nm : "(null)");
            ID3D10ShaderReflectionType *mt = ID3D10ShaderReflectionType_GetMemberTypeByIndex(t, i);
            D3D10_SHADER_TYPE_DESC md;
            if (mt && SUCCEEDED(ID3D10ShaderReflectionType_GetDesc(mt, &md))) {
                if (i == 0) { EXPECT_EQ(md.Class, D3D_SVC_VECTOR); EXPECT_EQ(md.Columns, 4); EXPECT_EQ(md.Offset, 0); }
                if (i == 1) { EXPECT_EQ(md.Type, D3D_SVT_INT); EXPECT_EQ(md.Columns, 2); EXPECT_EQ(md.Offset, 16); }
                if (i == 2) { EXPECT_EQ(md.Class, D3D_SVC_SCALAR); EXPECT_EQ(md.Offset, 24); }
            } else CHECK(0);
        }
    }
}

/* ---- signatures ---- */
typedef struct { const char *sem; UINT idx, reg; int sv; BYTE mask; } sigexp;
static void check_sig(ID3D10ShaderReflection *r, int output, const sigexp *e, int n, int who, const char *what)
{
    for (int i = 0; i < n; i++) {
        D3D10_SIGNATURE_PARAMETER_DESC d;
        HRESULT hr = output ? ID3D10ShaderReflection_GetOutputParameterDesc(r, i, &d) : ID3D10ShaderReflection_GetInputParameterDesc(r, i, &d);
        if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "signature desc", "%s %s[%d]: %s", k_who[who], what, i, d3dh_hr_name(hr)); continue; }
        t_check(!strcmp(d.SemanticName, e[i].sem), __FILE__, __LINE__, "SemanticName", "%s %s[%d]: %s want %s", k_who[who], what, i, d.SemanticName, e[i].sem);
        t_check(d.SemanticIndex == e[i].idx, __FILE__, __LINE__, "SemanticIndex", "%s %s[%d]: %u", k_who[who], what, i, d.SemanticIndex);
        t_check(d.Register == e[i].reg, __FILE__, __LINE__, "Register", "%s %s[%d]: %u", k_who[who], what, i, d.Register);
        /* D3D10ReflectShader reports SV_Target as system value 0 (undefined), not D3D_NAME_TARGET (64), for either compiler */
        int want_sv = !strcmp(e[i].sem, "SV_Target") ? 0 : e[i].sv;
        t_check((int)d.SystemValueType == want_sv, __FILE__, __LINE__, "SystemValueType", "%s %s[%d]: %d want %d", k_who[who], what, i, d.SystemValueType, want_sv);
        t_check(d.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32, __FILE__, __LINE__, "ComponentType", "%s %s[%d]: %d", k_who[who], what, i, d.ComponentType);
        t_check(d.Mask == e[i].mask, __FILE__, __LINE__, "Mask", "%s %s[%d]: %02x want %02x", k_who[who], what, i, d.Mask, e[i].mask);
    }
}
TEST(d3d10_utils_effects_reflect_ps_input_signature)
{
    static const sigexp e[] = { { "SV_Position", 0, 0, 1, 0xf }, { "TEXCOORD", 0, 1, 0, 0x3 } };
    FOR_BOTH(x, RF_SRC, "ps_4_0") check_sig(x.r, 0, e, 2, who, "ps input");
}
TEST(d3d10_utils_effects_reflect_ps_output_signature)
{
    static const sigexp e[] = { { "SV_Target", 0, 0, 64, 0xf } };
    FOR_BOTH(x, RF_SRC, "ps_4_0") check_sig(x.r, 1, e, 1, who, "ps output");
}
TEST(d3d10_utils_effects_reflect_vs_input_signature)
{
    static const sigexp e[] = { { "POSITION", 0, 0, 0, 0xf }, { "TEXCOORD", 0, 1, 0, 0x3 }, { "NORMAL", 0, 2, 0, 0x7 } };
    FOR_BOTH(x, UE_VS_SRC, "vs_4_0") check_sig(x.r, 0, e, 3, who, "vs input");
}
TEST(d3d10_utils_effects_reflect_vs_output_signature)
{
    static const sigexp e[] = { { "SV_Position", 0, 0, 1, 0xf }, { "TEXCOORD", 0, 1, 0, 0x3 }, { "TEXCOORD", 1, 2, 0, 0x7 } };
    FOR_BOTH(x, UE_VS_SRC, "vs_4_0") check_sig(x.r, 1, e, 3, who, "vs output");
}
TEST(d3d10_utils_effects_reflect_signature_out_of_range)
{
    FOR_BOTH(x, RF_SRC, "ps_4_0") {
        D3D10_SIGNATURE_PARAMETER_DESC d;
        t_check(FAILED(ID3D10ShaderReflection_GetInputParameterDesc(x.r, 2, &d)), __FILE__, __LINE__, "input idx == count", "%s", k_who[who]);
        t_check(FAILED(ID3D10ShaderReflection_GetOutputParameterDesc(x.r, 1, &d)), __FILE__, __LINE__, "output idx == count", "%s", k_who[who]);
        CHECK(FAILED(ID3D10ShaderReflection_GetInputParameterDesc(x.r, 0xffffffffu, &d)));
        CHECK(FAILED(ID3D10ShaderReflection_GetOutputParameterDesc(x.r, 0xffffffffu, &d)));
    }
}
TEST(d3d10_utils_effects_reflect_signature_matches_dxbc_chunk)
{
    FOR_BOTH(x, UE_VS_SRC, "vs_4_0") {
        DWORD csz;
        const unsigned char *c = ue_dxbc_chunk(ue_bp(x.sh), ue_bs(x.sh), FOURCC('I','S','G','N'), &csz);
        if (!c) { CHECK(0); continue; }
        DWORD cnt = *(const DWORD *)c;
        for (DWORD i = 0; i < cnt; i++) {
            const DWORD *e = (const DWORD *)(c + 8 + 24 * i);
            D3D10_SIGNATURE_PARAMETER_DESC d;
            if (FAILED(ID3D10ShaderReflection_GetInputParameterDesc(x.r, i, &d))) { CHECK(0); continue; }
            CHECK(!strcmp(d.SemanticName, (const char *)c + e[0]));
            EXPECT_EQ(d.SemanticIndex, e[1]);
            EXPECT_EQ(d.Register, e[4]);
            EXPECT_EQ(d.Mask, e[5] & 0xff);
        }
    }
}
TEST(d3d10_utils_effects_reflect_semantic_name_pointer_stable)
{
    FOR_BOTH(x, UE_VS_SRC, "vs_4_0") {
        D3D10_SIGNATURE_PARAMETER_DESC a, b;
        ID3D10ShaderReflection_GetInputParameterDesc(x.r, 0, &a);
        ID3D10ShaderReflection_GetInputParameterDesc(x.r, 0, &b);
        EXPECT_EQ(a.SemanticName, b.SemanticName);
    }
}
TEST(d3d10_utils_effects_reflect_integer_signature_component_types)
{
    const char *src = "float4 main(uint u : A, int i : B, float f : C) : SV_Target { return u + i + f; }\n";
    FOR_BOTH(x, src, "ps_4_0") {
        D3D10_SIGNATURE_PARAMETER_DESC d;
        /* integer pixel-shader inputs need nointerpolation to compile in some cases: only inspect when it compiled */
        for (int i = 0; i < 3; i++) {
            if (FAILED(ID3D10ShaderReflection_GetInputParameterDesc(x.r, i, &d))) continue;
            static const D3D_REGISTER_COMPONENT_TYPE want[3] = { D3D_REGISTER_COMPONENT_UINT32, D3D_REGISTER_COMPONENT_SINT32, D3D_REGISTER_COMPONENT_FLOAT32 };
            t_check(d.ComponentType == want[i], __FILE__, __LINE__, "component type", "%s %d: %d", k_who[who], i, d.ComponentType);
        }
    }
}
TEST(d3d10_utils_effects_reflect_matches_d3dcompiler_reflection)
{
    /* cross-check against the d3dcompiler_47 D3D11 reflector on the same bytecode */
    ID3D10Blob *sh = NULL;
    REQUIRE(REFL("d3d10.dll") && d3dh_D3DReflect());
    NOWINE("emulated reflection differs");
    if (FAILED(make_rf(1, RF_SRC, "ps_4_0", &sh))) { CHECK(0); return; }
    ID3D10ShaderReflection *r10 = NULL;
    ID3D11ShaderReflection *r11 = NULL;
    EXPECT_HR(REFL("d3d10.dll")(ue_bp(sh), ue_bs(sh), &r10), S_OK);
    EXPECT_HR(d3dh_D3DReflect()(ue_bp(sh), ue_bs(sh), &IID_ID3D11ShaderReflection, (void **)&r11), S_OK);
    if (r10 && r11) {
        D3D10_SHADER_DESC d10; D3D11_SHADER_DESC d11;
        ID3D10ShaderReflection_GetDesc(r10, &d10);
        r11->lpVtbl->GetDesc(r11, &d11);
        EXPECT_EQ(d10.ConstantBuffers, d11.ConstantBuffers);
        EXPECT_EQ(d10.BoundResources, d11.BoundResources);
        EXPECT_EQ(d10.InputParameters, d11.InputParameters);
        EXPECT_EQ(d10.OutputParameters, d11.OutputParameters);
        EXPECT_EQ(d10.Version, d11.Version);
        EXPECT_EQ(d10.InstructionCount, d11.InstructionCount);
        for (UINT i = 0; i < d10.BoundResources; i++) {
            D3D10_SHADER_INPUT_BIND_DESC b10; D3D11_SHADER_INPUT_BIND_DESC b11;
            ID3D10ShaderReflection_GetResourceBindingDesc(r10, i, &b10);
            r11->lpVtbl->GetResourceBindingDesc(r11, i, &b11);
            EXPECT_STR(b10.Name, b11.Name);
            EXPECT_EQ(b10.Type, b11.Type);
            EXPECT_EQ(b10.BindPoint, b11.BindPoint);
        }
    }
    UE_REL(r10); if (r11) r11->lpVtbl->Release(r11);
    UE_REL(sh);
}
