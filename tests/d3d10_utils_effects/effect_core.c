/* D3D10CreateEffectFromMemory / CompileEffectFromMemory / ID3D10Effect lookups and invalid objects. */
#include "ue.h"

#define E_INVALIDARG_ ((HRESULT)0x80070057u)
#define E_FAIL_       ((HRESULT)0x80004005u)

typedef HRESULT (WINAPI *create_fx_t)(void *, SIZE_T, UINT, ID3D10Device *, ID3D10EffectPool *, ID3D10Effect **);
typedef HRESULT (WINAPI *create_pool_t)(void *, SIZE_T, UINT, ID3D10Device *, ID3D10EffectPool **);
typedef HRESULT (WINAPI *disasm_fx_t)(ID3D10Effect *, BOOL, ID3D10Blob **);
#define CREATE_FX(dll) ((create_fx_t)t_proc_in(dll, "D3D10CreateEffectFromMemory"))
#define CREATE_POOL(dll) ((create_pool_t)t_proc_in(dll, "D3D10CreateEffectPoolFromMemory"))

static const char *vname(ID3D10EffectVariable *v)
{
    D3D10_EFFECT_VARIABLE_DESC d;
    if (!v || FAILED(ID3D10EffectVariable_GetDesc(v, &d))) return NULL;
    return d.Name;
}

/* ---- compile ---- */
TEST(d3d10_utils_effects_fx_compile_basic)
{
    ID3D10Blob *b = NULL;
    HRESULT hr = ue_compile_fx(UE_FX_BASIC, 0, 0, &b);
    if (hr == D3DH_E_NOCOMPILER) { t_skip("missing export"); return; }
    EXPECT_HR(hr, S_OK);
    CHECK(b != NULL && ue_bs(b) > 32);
    if (b) {
        const unsigned char *p = ue_bp(b);
        if (memcmp(p, "DXBC", 4)) t_note("compiled effect does not start with DXBC: %02x%02x%02x%02x", p[0], p[1], p[2], p[3]);
        else {
            CHECK(ue_dxbc_chunk(p, ue_bs(b), FOURCC('F','X','1','0'), NULL) != NULL);
            EXPECT_EQ(*(const DWORD *)(p + 24), ue_bs(b));
        }
    }
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_compile_syntax_error)
{
    ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1, *e = NULL;
    typedef HRESULT (WINAPI *cfx_t)(void *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, UINT, UINT, ID3D10Blob **, ID3D10Blob **);
    cfx_t f = (cfx_t)t_proc_in("d3d10.dll", "D3D10CompileEffectFromMemory");
    REQUIRE(f);
    const char *src = "float4 x\ntechnique10 T { pass P { } }\n";
    HRESULT hr = f((void *)src, strlen(src), "bad.fx", NULL, NULL, 0, 0, &b, &e);
    CHECK(FAILED(hr));
    EXPECT_EQ(b == (ID3D10Blob *)(ULONG_PTR)1, 0);
    CHECK(e != NULL);
    if (e) {
        char t[600];
        size_t n = ID3D10Blob_GetBufferSize(e); if (n > 599) n = 599;
        memcpy(t, ID3D10Blob_GetBufferPointer(e), n); t[n] = 0;
        CHECK(strstr(t, "bad.fx") != NULL);
        t_note("effect syntax error: %s : %.120s", d3dh_hr_name(hr), t);
    }
    if (b != (ID3D10Blob *)(ULONG_PTR)1) UE_REL(b);
    UE_REL(e);
}
TEST(d3d10_utils_effects_fx_compile_empty_source)
{
    ID3D10Blob *b = NULL;
    HRESULT hr = ue_compile_fx("", 0, 0, &b);
    if (hr == D3DH_E_NOCOMPILER) { t_skip("missing export"); return; }
    t_note("compiling an empty effect: %s", d3dh_hr_name(hr));
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_compile_no_technique)
{
    ID3D10Blob *b = NULL;
    HRESULT hr = ue_compile_fx("float4 x;\n", 0, 0, &b);
    if (hr == D3DH_E_NOCOMPILER) { t_skip("missing export"); return; }
    EXPECT_HR(hr, S_OK);
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_compile_with_macros)
{
    typedef HRESULT (WINAPI *cfx_t)(void *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, UINT, UINT, ID3D10Blob **, ID3D10Blob **);
    cfx_t f = (cfx_t)t_proc_in("d3d10.dll", "D3D10CompileEffectFromMemory");
    REQUIRE(f);
    const char *src = "#ifndef NEED\n#error NEED missing\n#endif\nfloat4 x;\ntechnique10 T { pass P { } }\n";
    D3D10_SHADER_MACRO defs[] = { { "NEED", "1" }, { NULL, NULL } };
    ID3D10Blob *b = NULL, *e = NULL;
    EXPECT_HR(f((void *)src, strlen(src), "m.fx", defs, NULL, 0, 0, &b, &e), S_OK);
    UE_REL(b); UE_REL(e);
    HRESULT hr = f((void *)src, strlen(src), "m.fx", NULL, NULL, 0, 0, &b, &e);
    CHECK(FAILED(hr));
    UE_REL(b); UE_REL(e);
}
TEST(d3d10_utils_effects_fx_compile_deterministic)
{
    ID3D10Blob *a = NULL, *b = NULL;
    HRESULT h1 = ue_compile_fx(UE_FX_BASIC, 0, 0, &a);
    if (h1 == D3DH_E_NOCOMPILER) { t_skip("missing export"); return; }
    HRESULT h2 = ue_compile_fx(UE_FX_BASIC, 0, 0, &b);
    EXPECT_HR(h1, S_OK); EXPECT_HR(h2, S_OK);
    if (a && b) CHECK(ue_bs(a) == ue_bs(b) && !memcmp(ue_bp(a), ue_bp(b), ue_bs(a)));
    UE_REL(a); UE_REL(b);
}
TEST(d3d10_utils_effects_fx_compile_via_d3d10_1)
{
    typedef HRESULT (WINAPI *cfx_t)(void *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, UINT, UINT, ID3D10Blob **, ID3D10Blob **);
    cfx_t f = (cfx_t)t_proc_in("d3d10_1.dll", "D3D10CompileEffectFromMemory");
    REQUIRE(f);
    ID3D10Blob *b = NULL, *e = NULL;
    EXPECT_HR(f((void *)UE_FX_BASIC, strlen(UE_FX_BASIC), "b.fx", NULL, NULL, 0, 0, &b, &e), S_OK);
    CHECK(b != NULL);
    UE_REL(b); UE_REL(e);
}
static int g_fx_opens, g_fx_closes;
static const char g_fx_inc_content[] = "float4 from_include;\n";
static HRESULT WINAPI fx_inc_open(ID3DInclude *t, D3D_INCLUDE_TYPE ty, LPCSTR n, LPCVOID p, LPCVOID *d, UINT *b) { g_fx_opens++; *d = g_fx_inc_content; *b = sizeof g_fx_inc_content - 1; return S_OK; }
static HRESULT WINAPI fx_inc_close(ID3DInclude *t, LPCVOID d) { g_fx_closes++; return S_OK; }
static const ID3DIncludeVtbl g_fx_inc_vt = { fx_inc_open, fx_inc_close };
TEST(d3d10_utils_effects_fx_compile_include_handler)
{
    typedef HRESULT (WINAPI *cfx_t)(void *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, UINT, UINT, ID3D10Blob **, ID3D10Blob **);
    cfx_t f = (cfx_t)t_proc_in("d3d10.dll", "D3D10CompileEffectFromMemory");
    REQUIRE(f);
    const ID3DIncludeVtbl *iobj = &g_fx_inc_vt;
    const char *src = "#include \"inc.fxh\"\ntechnique10 T { pass P { } }\n";
    ID3D10Blob *b = NULL, *e = NULL;
    g_fx_opens = g_fx_closes = 0;
    HRESULT hr = f((void *)src, strlen(src), "i.fx", NULL, (ID3D10Include *)&iobj, 0, 0, &b, &e);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(g_fx_opens, 1);
    EXPECT_EQ(g_fx_closes, 1);
    UE_REL(b); UE_REL(e);
}

/* ---- create ---- */
TEST(d3d10_utils_effects_fx_create_ok)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    CHECK(ID3D10Effect_IsValid(fx) != 0);
    EXPECT_EQ(ID3D10Effect_IsPool(fx), 0);
    EQV(ID3D10Effect_Release(fx), 0);
}
TEST(d3d10_utils_effects_fx_create_flag_rules)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10.dll") && CREATE_POOL("d3d10.dll"));
    ID3D10Blob *b = NULL;
    if (FAILED(ue_compile_fx(UE_FX_BASIC, 0, 0, &b))) { CHECK(0); return; }
    ID3D10Effect *fx = (ID3D10Effect *)(ULONG_PTR)1;
    /* a child effect (bit 0) needs a pool ... */
    EXPECT_HR(CREATE_FX("d3d10.dll")((void *)ue_bp(b), ue_bs(b), D3D10_EFFECT_COMPILE_CHILD_EFFECT, dev, NULL, &fx), E_INVALIDARG_);
    EXPECT_HR(CREATE_FX("d3d10.dll")((void *)ue_bp(b), ue_bs(b), 3, dev, NULL, &fx), E_INVALIDARG_);
    /* ... and a pool needs bit 0 */
    ID3D10Blob *pb = NULL;
    ID3D10EffectPool *pool = NULL;
    if (SUCCEEDED(ue_compile_fx(UE_FX_POOL, 0, 0, &pb)) && SUCCEEDED(CREATE_POOL("d3d10.dll")((void *)ue_bp(pb), ue_bs(pb), 0, dev, &pool)) && pool) {
        EXPECT_HR(CREATE_FX("d3d10.dll")((void *)ue_bp(b), ue_bs(b), 0, dev, pool, &fx), E_INVALIDARG_);
        EXPECT_HR(CREATE_FX("d3d10.dll")((void *)ue_bp(b), ue_bs(b), D3D10_EFFECT_SINGLE_THREADED, dev, pool, &fx), E_INVALIDARG_);
        ID3D10EffectPool_Release(pool);
    }
    UE_REL(pb);
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_create_flag_rules_precede_data_validation)
{
    /* the flag/pool consistency is checked before the data is looked at */
    REQUIRE(CREATE_FX("d3d10.dll"));
    unsigned char junk[16] = {0};
    ID3D10Effect *fx = NULL;
    EXPECT_HR(CREATE_FX("d3d10.dll")(junk, sizeof junk, 1, NULL, NULL, &fx), E_INVALIDARG_);
    EXPECT_HR(CREATE_FX("d3d10.dll")(NULL, 0, 1, NULL, NULL, &fx), E_INVALIDARG_);
    EXPECT_HR(CREATE_FX("d3d10_1.dll")(junk, sizeof junk, 1, NULL, NULL, &fx), E_INVALIDARG_);
}
TEST(d3d10_utils_effects_fx_create_garbage_blob)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10.dll"));
    unsigned char junk[256];
    for (int i = 0; i < 256; i++) junk[i] = (unsigned char)(i * 5 + 9);
    ID3D10Effect *fx = (ID3D10Effect *)(ULONG_PTR)1;
    HRESULT hr = CREATE_FX("d3d10.dll")(junk, sizeof junk, 0, dev, NULL, &fx);
    CHECK(FAILED(hr));
    EXPECT_EQ(fx, NULL);
    memset(junk, 0, sizeof junk);
    fx = (ID3D10Effect *)(ULONG_PTR)1;
    hr = CREATE_FX("d3d10.dll")(junk, sizeof junk, 0, dev, NULL, &fx);
    CHECK(FAILED(hr));
    EXPECT_EQ(fx, NULL);
}
TEST(d3d10_utils_effects_fx_create_zero_size)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10.dll"));
    ID3D10Blob *b = NULL;
    if (FAILED(ue_compile_fx(UE_FX_BASIC, 0, 0, &b))) { CHECK(0); return; }
    ID3D10Effect *fx = (ID3D10Effect *)(ULONG_PTR)1;
    HRESULT hr = CREATE_FX("d3d10.dll")((void *)ue_bp(b), 0, 0, dev, NULL, &fx);
    CHECK(FAILED(hr));
    EXPECT_EQ(fx, NULL);
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_create_truncated_blob)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10.dll"));
    ID3D10Blob *b = NULL;
    if (FAILED(ue_compile_fx(UE_FX_BASIC, 0, 0, &b))) { CHECK(0); return; }
    SIZE_T n = ue_bs(b);
    SIZE_T cuts[] = { 4, 8, 32, 36, 64, n / 2, n - 1 };
    unsigned char *copy = malloc(n);
    memcpy(copy, ue_bp(b), n);
    for (unsigned i = 0; i < sizeof cuts / sizeof *cuts; i++) {
        ID3D10Effect *fx = (ID3D10Effect *)(ULONG_PTR)1;
        HRESULT hr = CREATE_FX("d3d10.dll")(copy, cuts[i], 0, dev, NULL, &fx);
        t_check(FAILED(hr) && !fx, __FILE__, __LINE__, "truncated effect rejected", "size %u of %u: %s", (unsigned)cuts[i], (unsigned)n, d3dh_hr_name(hr));
        if (fx && fx != (ID3D10Effect *)(ULONG_PTR)1) ID3D10Effect_Release(fx);
    }
    free(copy);
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_create_corrupt_header)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10.dll"));
    ID3D10Blob *b = NULL;
    if (FAILED(ue_compile_fx(UE_FX_BASIC, 0, 0, &b))) { CHECK(0); return; }
    SIZE_T n = ue_bs(b);
    unsigned char *copy = malloc(n);
    memcpy(copy, ue_bp(b), n);
    copy[0] ^= 0xff;
    ID3D10Effect *fx = (ID3D10Effect *)(ULONG_PTR)1;
    HRESULT hr = CREATE_FX("d3d10.dll")(copy, n, 0, dev, NULL, &fx);
    CHECK(FAILED(hr));
    EXPECT_EQ(fx, NULL);
    free(copy);
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_create_via_d3d10_1)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10_1.dll"));
    ID3D10Blob *b = NULL;
    if (FAILED(ue_compile_fx(UE_FX_BASIC, 0, 0, &b))) { CHECK(0); return; }
    ID3D10Effect *fx = NULL;
    EXPECT_HR(CREATE_FX("d3d10_1.dll")((void *)ue_bp(b), ue_bs(b), 0, dev, NULL, &fx), S_OK);
    CHECK(fx && ID3D10Effect_IsValid(fx));
    SAFE_RELEASE(fx);
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_create_null_device)
{
    REQUIRE(CREATE_FX("d3d10.dll"));
    NOWINE("emulator dereferences the device");
    ID3D10Blob *b = NULL;
    if (FAILED(ue_compile_fx(UE_FX_BASIC, 0, 0, &b))) { CHECK(0); return; }
    ID3D10Effect *fx = (ID3D10Effect *)(ULONG_PTR)1;
    HRESULT hr = CREATE_FX("d3d10.dll")((void *)ue_bp(b), ue_bs(b), 0, NULL, NULL, &fx);
    t_note("create effect with NULL device: %s (effect %p)", d3dh_hr_name(hr), (void *)fx);
    if (SUCCEEDED(hr) && fx && fx != (ID3D10Effect *)(ULONG_PTR)1) ID3D10Effect_Release(fx);
    UE_REL(b);
}
TEST(d3d10_utils_effects_fx_create_from_d3dcompiler_fx_4_0)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10.dll") && d3dh_D3DCompile());
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile(UE_FX_BASIC, NULL, "fx_4_0", 0, &b);
    t_note("D3DCompile fx_4_0: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        ID3D10Effect *fx = NULL;
        hr = CREATE_FX("d3d10.dll")((void *)ue_bp((ID3D10Blob *)b), ue_bs((ID3D10Blob *)b), 0, dev, NULL, &fx);
        t_note("create from fx_4_0 blob: %s", d3dh_hr_name(hr));
        EXPECT_HR(hr, S_OK);
        SAFE_RELEASE(fx);
    }
    SAFE_RELEASE(b);
}
TEST(d3d10_utils_effects_fx_create_from_d3dcompiler_fx_5_0)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10.dll") && d3dh_D3DCompile());
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile(UE_FX_BASIC, NULL, "fx_5_0", 0, &b);
    t_note("D3DCompile fx_5_0: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        ID3D10Effect *fx = (ID3D10Effect *)(ULONG_PTR)1;
        hr = CREATE_FX("d3d10.dll")((void *)ue_bp((ID3D10Blob *)b), ue_bs((ID3D10Blob *)b), 0, dev, NULL, &fx);
        t_note("create from fx_5_0 blob with D3D10CreateEffectFromMemory: %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && fx) ID3D10Effect_Release(fx);
    }
    SAFE_RELEASE(b);
}
TEST(d3d10_utils_effects_fx_create_from_d3dcompiler_fx_4_0_has_same_shape)
{
    NEED_DEVICE(dev);
    REQUIRE(CREATE_FX("d3d10.dll") && d3dh_D3DCompile());
    ID3DBlob *b = NULL;
    ID3D10Effect *fx = NULL;
    if (FAILED(d3dh_compile(UE_FX_BASIC, NULL, "fx_4_0", 0, &b))) { t_skip("fx_4_0 target unavailable"); return; }
    if (FAILED(CREATE_FX("d3d10.dll")((void *)ue_bp((ID3D10Blob *)b), ue_bs((ID3D10Blob *)b), 0, dev, NULL, &fx)) || !fx) { t_skip("fx_4_0 blob not loadable"); SAFE_RELEASE(b); return; }
    D3D10_EFFECT_DESC d;
    EXPECT_HR(ID3D10Effect_GetDesc(fx, &d), S_OK);
    EXPECT_EQ(d.Techniques, 2);
    EXPECT_EQ(d.GlobalVariables, 9);
    CHECK(ID3D10Effect_GetVariableByName(fx, "fA") && ID3D10EffectVariable_IsValid(ID3D10Effect_GetVariableByName(fx, "fA")));
    ID3D10Effect_Release(fx);
    SAFE_RELEASE(b);
}

/* ---- IUnknown / device / desc ---- */
TEST(d3d10_utils_effects_fx_refcount)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    EQV(ID3D10Effect_AddRef(fx), 2);
    EQV(ID3D10Effect_AddRef(fx), 3);
    EQV(ID3D10Effect_Release(fx), 2);
    EQV(ID3D10Effect_Release(fx), 1);
    EQV(ID3D10Effect_Release(fx), 0);
}
TEST(d3d10_utils_effects_fx_holds_device_reference)
{
    NEED_DEVICE(dev);
    if (ue_device_is_fake()) { t_skip("needs a real device"); return; }
    ID3D10Device_AddRef(dev);
    ULONG before = ID3D10Device_Release(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10Device_AddRef(dev);
    ULONG during = ID3D10Device_Release(dev);
    CHECK(during > before);
    ID3D10Effect_Release(fx);
    ID3D10Device_AddRef(dev);
    ULONG after = ID3D10Device_Release(dev);
    EXPECT_EQ(after, before);
}
TEST(d3d10_utils_effects_fx_get_device)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10Device *d2 = NULL;
    EXPECT_HR(ID3D10Effect_GetDevice(fx, &d2), S_OK);
    EXPECT_EQ(d2, dev);
    if (d2 && !ue_device_is_fake()) {
        ID3D10Device_AddRef(dev);
        ULONG r1 = ID3D10Device_Release(dev);
        ID3D10Device_Release(d2);
        ID3D10Device_AddRef(dev);
        ULONG r2 = ID3D10Device_Release(dev);
        EXPECT_EQ(r2 + 1, r1);
    } else if (d2) ID3D10Device_Release(d2);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_get_device_null)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    EXPECT_HR(ID3D10Effect_GetDevice(fx, NULL), E_INVALIDARG_);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_get_desc)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    D3D10_EFFECT_DESC d;
    memset(&d, 0xcd, sizeof d);
    EXPECT_HR(ID3D10Effect_GetDesc(fx, &d), S_OK);
    EXPECT_EQ(d.IsChildEffect, 0);
    EXPECT_EQ(d.SharedConstantBuffers, 0);
    EXPECT_EQ(d.SharedGlobalVariables, 0);
    EXPECT_EQ(d.GlobalVariables, 9);
    EXPECT_EQ(d.Techniques, 2);
    CHECK(d.ConstantBuffers >= 1);
    t_note("effect desc: constant buffers %u", d.ConstantBuffers);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_get_desc_null)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    EXPECT_HR(ID3D10Effect_GetDesc(fx, NULL), E_INVALIDARG_);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_get_desc_objects_only)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    D3D10_EFFECT_DESC d;
    EXPECT_HR(ID3D10Effect_GetDesc(fx, &d), S_OK);
    EXPECT_EQ(d.Techniques, 1);
    EXPECT_EQ(d.GlobalVariables, 11);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_qi_null_out)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    EXPECT_HR(ID3D10Effect_QueryInterface(fx, &IID_IUnknown, NULL), E_INVALIDARG_);
    EXPECT_HR(ID3D10Effect_QueryInterface(fx, &IID_ID3D10Effect, NULL), E_INVALIDARG_);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_qi_edge_cases)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    void *o = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Effect_QueryInterface(fx, &IID_IDXGIObject, &o), E_NOINTERFACE);
    EXPECT_EQ(o, NULL);
    o = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Effect_QueryInterface(fx, &IID_ID3D10Device, &o), E_NOINTERFACE);
    EXPECT_EQ(o, NULL);
    o = NULL;
    HRESULT hr = ID3D10Effect_QueryInterface(fx, &IID_ID3D10Effect, &o);
    t_note("QI(IID_ID3D10Effect) on an effect: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && o) ID3D10Effect_Release((ID3D10Effect *)o);
    o = NULL;
    hr = ID3D10Effect_QueryInterface(fx, &IID_IUnknown, &o);
    t_note("QI(IID_IUnknown) on an effect: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && o) ID3D10Effect_Release((ID3D10Effect *)o);
    ID3D10Effect_Release(fx);
}

/* ---- lookups ---- */
static const char *const k_basic_vars[9] = { "vecA", "fA", "iA", "bA", "mA", "arrA", "v3", "iv2", "vArr" };
TEST(d3d10_utils_effects_fx_variable_by_name)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    for (int i = 0; i < 9; i++) {
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableByName(fx, k_basic_vars[i]);
        t_check(v && ID3D10EffectVariable_IsValid(v), __FILE__, __LINE__, "valid", "%s", k_basic_vars[i]);
        const char *n = vname(v);
        t_check(n && !strcmp(n, k_basic_vars[i]), __FILE__, __LINE__, "desc name", "%s", k_basic_vars[i]);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_by_name_is_stable)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *a = ID3D10Effect_GetVariableByName(fx, "fA"), *b = ID3D10Effect_GetVariableByName(fx, "fA");
    EXPECT_EQ(a, b);
    ID3D10EffectVariable *c = ID3D10Effect_GetVariableByIndex(fx, 1);
    EXPECT_EQ(a, c);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_by_name_case_sensitive)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const char *const bad[] = { "FA", "fa", "VECA", "Veca", "arra", "ARRA" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableByName(fx, bad[i]);
        t_check(v && !ID3D10EffectVariable_IsValid(v), __FILE__, __LINE__, "case mismatch is invalid", "%s", bad[i]);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_by_name_misses)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const char *const bad[] = { "", " ", "fA ", " fA", "f", "fAA", "cb", "T0", "nosuch", "MYSEM" };
    ID3D10EffectVariable *first = NULL;
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableByName(fx, bad[i]);
        t_check(v && !ID3D10EffectVariable_IsValid(v), __FILE__, __LINE__, "miss is invalid", "'%s'", bad[i]);
        if (!first) first = v;
        else EXPECT_EQ(v, first);
    }
    ID3D10EffectVariable *n = ID3D10Effect_GetVariableByName(fx, NULL);
    CHECK(n && !ID3D10EffectVariable_IsValid(n));
    EXPECT_EQ(n, first);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_by_index)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    for (UINT i = 0; i < 9; i++) {
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableByIndex(fx, i);
        const char *n = vname(v);
        t_check(v && ID3D10EffectVariable_IsValid(v) && n && !strcmp(n, k_basic_vars[i]), __FILE__, __LINE__, "declaration order", "index %u: %s", i, n ? n : "(null)");
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_by_index_out_of_range)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const UINT idx[] = { 9, 10, 100, 0x7fffffff, 0xffffffffu };
    ID3D10EffectVariable *first = NULL;
    for (unsigned i = 0; i < sizeof idx / sizeof *idx; i++) {
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableByIndex(fx, idx[i]);
        t_check(v && !ID3D10EffectVariable_IsValid(v), __FILE__, __LINE__, "invalid", "index %u", idx[i]);
        if (!first) first = v; else EXPECT_EQ(v, first);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_by_semantic)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *v = ID3D10Effect_GetVariableBySemantic(fx, "MYSEM");
    CHECK(v && ID3D10EffectVariable_IsValid(v));
    const char *n = vname(v);
    CHECK(n && !strcmp(n, "vecA"));
    D3D10_EFFECT_VARIABLE_DESC d;
    if (v && SUCCEEDED(ID3D10EffectVariable_GetDesc(v, &d))) { CHECK(d.Semantic && !strcmp(d.Semantic, "MYSEM")); }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_by_semantic_case_insensitive)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const char *const names[] = { "mysem", "MySem", "MYSEM" };
    for (unsigned i = 0; i < 3; i++) {
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableBySemantic(fx, names[i]);
        const char *n = vname(v);
        t_check(v && ID3D10EffectVariable_IsValid(v) && n && !strcmp(n, "vecA"), __FILE__, __LINE__, "semantic match ignores case", "%s", names[i]);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_by_semantic_misses)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const char *const bad[] = { "", "MYSE", "MYSEMX", "vecA", "POSITION" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableBySemantic(fx, bad[i]);
        t_check(v && !ID3D10EffectVariable_IsValid(v), __FILE__, __LINE__, "invalid", "'%s'", bad[i]);
    }
    ID3D10EffectVariable *n = ID3D10Effect_GetVariableBySemantic(fx, NULL);
    CHECK(n && !ID3D10EffectVariable_IsValid(n));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_desc_fields)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const UINT offs[9] = { 0, 16, 20, 24, 32, 96, 132, 144, 160 };
    for (UINT i = 0; i < 9; i++) {
        D3D10_EFFECT_VARIABLE_DESC d;
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableByIndex(fx, i);
        if (FAILED(ID3D10EffectVariable_GetDesc(v, &d))) { CHECK(0); continue; }
        t_check(d.BufferOffset == offs[i], __FILE__, __LINE__, "BufferOffset", "%s: %u want %u", d.Name, d.BufferOffset, offs[i]);
        EXPECT_EQ(d.Annotations, 0);
        EXPECT_EQ(d.Flags & D3D10_EFFECT_VARIABLE_ANNOTATION, 0);
        EXPECT_EQ(d.Flags & D3D10_EFFECT_VARIABLE_POOLED, 0);
        EXPECT_EQ(d.Flags & D3D10_EFFECT_VARIABLE_EXPLICIT_BIND_POINT, 0);
        if (i == 0) { CHECK(d.Semantic && !strcmp(d.Semantic, "MYSEM")); } else CHECK(d.Semantic == NULL);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_desc_null)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *v = ID3D10Effect_GetVariableByName(fx, "fA");
    HRESULT hr = ID3D10EffectVariable_GetDesc(v, NULL);
    CHECK(FAILED(hr));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_constant_buffer_lookup)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectConstantBuffer *cb = ID3D10Effect_GetConstantBufferByName(fx, "cb");
    CHECK(cb && ID3D10EffectConstantBuffer_IsValid(cb));
    D3D10_EFFECT_VARIABLE_DESC d;
    if (cb && SUCCEEDED(ID3D10EffectConstantBuffer_GetDesc(cb, &d))) { EXPECT_STR(d.Name, "cb"); EXPECT_EQ(d.Annotations, 0); }
    int found = 0;
    D3D10_EFFECT_DESC ed;
    ID3D10Effect_GetDesc(fx, &ed);
    for (UINT i = 0; i < ed.ConstantBuffers; i++) if (ID3D10Effect_GetConstantBufferByIndex(fx, i) == cb) found = 1;
    CHECK(found);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_constant_buffer_misses)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    D3D10_EFFECT_DESC ed;
    ID3D10Effect_GetDesc(fx, &ed);
    ID3D10EffectConstantBuffer *c1 = ID3D10Effect_GetConstantBufferByName(fx, "CB");
    ID3D10EffectConstantBuffer *c2 = ID3D10Effect_GetConstantBufferByName(fx, "nosuch");
    ID3D10EffectConstantBuffer *c3 = ID3D10Effect_GetConstantBufferByName(fx, NULL);
    ID3D10EffectConstantBuffer *c4 = ID3D10Effect_GetConstantBufferByIndex(fx, ed.ConstantBuffers);
    ID3D10EffectConstantBuffer *c5 = ID3D10Effect_GetConstantBufferByIndex(fx, 0xffffffffu);
    t_check(c1 && c2 && c3 && c4 && c5, __FILE__, __LINE__, "never NULL", "");
    if (c1) CHECK(!ID3D10EffectConstantBuffer_IsValid(c1));
    if (c2) CHECK(!ID3D10EffectConstantBuffer_IsValid(c2));
    if (c3) CHECK(!ID3D10EffectConstantBuffer_IsValid(c3));
    if (c4) CHECK(!ID3D10EffectConstantBuffer_IsValid(c4));
    EXPECT_EQ(c1, c2);
    EXPECT_EQ(c2, c4);
    EXPECT_EQ(c4, c5);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_variable_parent_constant_buffer)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectConstantBuffer *cb = ID3D10Effect_GetConstantBufferByName(fx, "cb");
    for (UINT i = 0; i < 9; i++) {
        ID3D10EffectVariable *v = ID3D10Effect_GetVariableByIndex(fx, i);
        ID3D10EffectConstantBuffer *p = ID3D10EffectVariable_GetParentConstantBuffer(v);
        t_check(p == cb, __FILE__, __LINE__, "parent cbuffer", "variable %u", i);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_cbuffer_variable_lookup_is_not_a_variable)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *v = ID3D10Effect_GetVariableByName(fx, "cb");
    CHECK(v && !ID3D10EffectVariable_IsValid(v));
    ID3D10Effect_Release(fx);
}

/* ---- techniques ---- */
TEST(d3d10_utils_effects_fx_technique_by_index_and_name)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    static const char *const names[2] = { "T0", "T1" };
    static const UINT passes[2] = { 2, 1 };
    for (UINT i = 0; i < 2; i++) {
        ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByIndex(fx, i);
        ID3D10EffectTechnique *n = ID3D10Effect_GetTechniqueByName(fx, names[i]);
        CHECK(t && ID3D10EffectTechnique_IsValid(t));
        EXPECT_EQ(t, n);
        D3D10_TECHNIQUE_DESC d;
        memset(&d, 0xcd, sizeof d);
        EXPECT_HR(ID3D10EffectTechnique_GetDesc(t, &d), S_OK);
        t_check(d.Name && !strcmp(d.Name, names[i]), __FILE__, __LINE__, "name", "%u", i);
        EXPECT_EQ(d.Passes, passes[i]);
        EXPECT_EQ(d.Annotations, 0);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_technique_misses)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectTechnique *a = ID3D10Effect_GetTechniqueByIndex(fx, 2);
    ID3D10EffectTechnique *b = ID3D10Effect_GetTechniqueByIndex(fx, 0xffffffffu);
    ID3D10EffectTechnique *c = ID3D10Effect_GetTechniqueByName(fx, "t0");
    ID3D10EffectTechnique *d = ID3D10Effect_GetTechniqueByName(fx, "nosuch");
    ID3D10EffectTechnique *e = ID3D10Effect_GetTechniqueByName(fx, NULL);
    ID3D10EffectTechnique *f = ID3D10Effect_GetTechniqueByName(fx, "");
    t_check(a && b && c && d && e && f, __FILE__, __LINE__, "never NULL", "");
    ID3D10EffectTechnique *all[] = { a, b, c, d, e, f };
    for (int i = 0; i < 6; i++) { if (all[i]) CHECK(!ID3D10EffectTechnique_IsValid(all[i])); EXPECT_EQ(all[i], a); }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_pass_lookup)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByName(fx, "T0");
    static const char *const names[2] = { "P0", "P1" };
    for (UINT i = 0; i < 2; i++) {
        ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(t, i);
        ID3D10EffectPass *q = ID3D10EffectTechnique_GetPassByName(t, names[i]);
        CHECK(p && ID3D10EffectPass_IsValid(p));
        EXPECT_EQ(p, q);
        D3D10_PASS_DESC d;
        memset(&d, 0xcd, sizeof d);
        EXPECT_HR(ID3D10EffectPass_GetDesc(p, &d), S_OK);
        t_check(d.Name && !strcmp(d.Name, names[i]), __FILE__, __LINE__, "pass name", "%u", i);
        EXPECT_EQ(d.Annotations, 0);
    }
    ID3D10EffectPass *bad1 = ID3D10EffectTechnique_GetPassByIndex(t, 2);
    ID3D10EffectPass *bad2 = ID3D10EffectTechnique_GetPassByName(t, "p0");
    ID3D10EffectPass *bad3 = ID3D10EffectTechnique_GetPassByName(t, NULL);
    t_check(bad1 && bad2 && bad3, __FILE__, __LINE__, "never NULL", "");
    if (bad1) CHECK(!ID3D10EffectPass_IsValid(bad1));
    if (bad2) CHECK(!ID3D10EffectPass_IsValid(bad2));
    if (bad3) CHECK(!ID3D10EffectPass_IsValid(bad3));
    EXPECT_EQ(bad1, bad2);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_passes_of_different_techniques_differ)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectPass *a = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByIndex(fx, 0), 0);
    ID3D10EffectPass *b = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByIndex(fx, 1), 0);
    CHECK(a != b);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_technique_annotations_none)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByIndex(fx, 0);
    ID3D10EffectVariable *a = ID3D10EffectTechnique_GetAnnotationByIndex(t, 0);
    ID3D10EffectVariable *b = ID3D10EffectTechnique_GetAnnotationByName(t, "x");
    CHECK(a && !ID3D10EffectVariable_IsValid(a));
    CHECK(b && !ID3D10EffectVariable_IsValid(b));
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(t, 0);
    a = ID3D10EffectPass_GetAnnotationByIndex(p, 0);
    CHECK(a && !ID3D10EffectVariable_IsValid(a));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_annotations_present)
{
    NEED_DEVICE(dev);
    const char *src =
        "float4 tinted <string UIName = \"Tint\"; float UIMin = 0.5; int Steps = 4;> = float4(1,1,1,1);\n"
        "technique10 T <string Group = \"g\";> { pass P <float Strength = 2.5;> { } }\n";
    FX_OPEN(fx, src);
    ID3D10EffectVariable *v = ID3D10Effect_GetVariableByName(fx, "tinted");
    D3D10_EFFECT_VARIABLE_DESC d;
    EXPECT_HR(ID3D10EffectVariable_GetDesc(v, &d), S_OK);
    EXPECT_EQ(d.Annotations, 3);
    ID3D10EffectVariable *a0 = ID3D10EffectVariable_GetAnnotationByIndex(v, 0);
    ID3D10EffectVariable *a1 = ID3D10EffectVariable_GetAnnotationByName(v, "UIMin");
    ID3D10EffectVariable *a2 = ID3D10EffectVariable_GetAnnotationByName(v, "Steps");
    CHECK(a0 && ID3D10EffectVariable_IsValid(a0));
    D3D10_EFFECT_VARIABLE_DESC ad;
    if (a0 && SUCCEEDED(ID3D10EffectVariable_GetDesc(a0, &ad))) { EXPECT_STR(ad.Name, "UIName"); CHECK(ad.Flags & D3D10_EFFECT_VARIABLE_ANNOTATION); }
    float f = 0;
    if (a1 && ID3D10EffectVariable_AsScalar(a1) && SUCCEEDED(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(a1), &f))) EXPECT_EQ(f == 0.5f, 1);
    else CHECK(0);
    int i = 0;
    if (a2 && SUCCEEDED(ID3D10EffectScalarVariable_GetInt(ID3D10EffectVariable_AsScalar(a2), &i))) EXPECT_EQ(i, 4);
    else CHECK(0);
    LPCSTR s = NULL;
    ID3D10EffectStringVariable *sv = ID3D10EffectVariable_AsString(a0);
    if (sv && SUCCEEDED(ID3D10EffectStringVariable_GetString(sv, &s)) && s) EXPECT_STR(s, "Tint");
    else if (!t_is_wine()) CHECK(0);
    /* annotations are read-only */
    if (a1) {
        HRESULT hr = ID3D10EffectScalarVariable_SetFloat(ID3D10EffectVariable_AsScalar(a1), 9.0f);
        EXPECT_HR(hr, E_D3DERR_INVALIDCALL);
    }
    ID3D10EffectVariable *miss = ID3D10EffectVariable_GetAnnotationByIndex(v, 3);
    CHECK(miss && !ID3D10EffectVariable_IsValid(miss));
    miss = ID3D10EffectVariable_GetAnnotationByName(v, "uimin");
    CHECK(miss && !ID3D10EffectVariable_IsValid(miss));
    ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByName(fx, "T");
    D3D10_TECHNIQUE_DESC td;
    ID3D10EffectTechnique_GetDesc(t, &td);
    EXPECT_EQ(td.Annotations, 1);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(t, 0);
    D3D10_PASS_DESC pd;
    ID3D10EffectPass_GetDesc(p, &pd);
    EXPECT_EQ(pd.Annotations, 1);
    ID3D10Effect_Release(fx);
}

/* ---- invalid objects ---- */
TEST(d3d10_utils_effects_fx_invalid_variable_methods_fail)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *inv = ID3D10Effect_GetVariableByName(fx, "nosuch");
    CHECK(!ID3D10EffectVariable_IsValid(inv));
    D3D10_EFFECT_VARIABLE_DESC d;
    EXPECT_HR(ID3D10EffectVariable_GetDesc(inv, &d), E_FAIL_);
    float f[4] = {1,2,3,4};
    int bufi[4] = {0};
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(inv);
    CHECK(s != NULL && !ID3D10EffectScalarVariable_IsValid(s));
    if (s) {
        EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 1.0f), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, f), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_SetInt(s, 1), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_GetInt(s, bufi), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_SetBool(s, TRUE), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_GetBool(s, bufi), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_SetFloatArray(s, f, 0, 4), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloatArray(s, f, 0, 4), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_SetIntArray(s, bufi, 0, 4), E_FAIL_);
        EXPECT_HR(ID3D10EffectScalarVariable_GetIntArray(s, bufi, 0, 4), E_FAIL_);
    }
    ID3D10EffectVectorVariable *vv = ID3D10EffectVariable_AsVector(inv);
    CHECK(vv != NULL && !ID3D10EffectVectorVariable_IsValid(vv));
    if (vv) {
        EXPECT_HR(ID3D10EffectVectorVariable_SetFloatVector(vv, f), E_FAIL_);
        EXPECT_HR(ID3D10EffectVectorVariable_GetFloatVector(vv, f), E_FAIL_);
        EXPECT_HR(ID3D10EffectVectorVariable_SetIntVector(vv, bufi), E_FAIL_);
        EXPECT_HR(ID3D10EffectVectorVariable_GetIntVector(vv, bufi), E_FAIL_);
    }
    ID3D10EffectMatrixVariable *mv = ID3D10EffectVariable_AsMatrix(inv);
    CHECK(mv != NULL && !ID3D10EffectMatrixVariable_IsValid(mv));
    if (mv) {
        float m[16] = {0};
        EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrix(mv, m), E_FAIL_);
        EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrix(mv, m), E_FAIL_);
        EXPECT_HR(ID3D10EffectMatrixVariable_SetMatrixTranspose(mv, m), E_FAIL_);
        EXPECT_HR(ID3D10EffectMatrixVariable_GetMatrixTranspose(mv, m), E_FAIL_);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_invalid_variable_casts_stay_invalid)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *inv = ID3D10Effect_GetVariableByName(fx, "nosuch");
    CHECK(!ID3D10EffectScalarVariable_IsValid(ID3D10EffectVariable_AsScalar(inv)));
    CHECK(!ID3D10EffectVectorVariable_IsValid(ID3D10EffectVariable_AsVector(inv)));
    CHECK(!ID3D10EffectMatrixVariable_IsValid(ID3D10EffectVariable_AsMatrix(inv)));
    CHECK(!ID3D10EffectStringVariable_IsValid(ID3D10EffectVariable_AsString(inv)));
    CHECK(!ID3D10EffectShaderResourceVariable_IsValid(ID3D10EffectVariable_AsShaderResource(inv)));
    CHECK(!ID3D10EffectRenderTargetViewVariable_IsValid(ID3D10EffectVariable_AsRenderTargetView(inv)));
    CHECK(!ID3D10EffectDepthStencilViewVariable_IsValid(ID3D10EffectVariable_AsDepthStencilView(inv)));
    CHECK(!ID3D10EffectConstantBuffer_IsValid(ID3D10EffectVariable_AsConstantBuffer(inv)));
    CHECK(!ID3D10EffectShaderVariable_IsValid(ID3D10EffectVariable_AsShader(inv)));
    CHECK(!ID3D10EffectBlendVariable_IsValid(ID3D10EffectVariable_AsBlend(inv)));
    CHECK(!ID3D10EffectDepthStencilVariable_IsValid(ID3D10EffectVariable_AsDepthStencil(inv)));
    CHECK(!ID3D10EffectRasterizerVariable_IsValid(ID3D10EffectVariable_AsRasterizer(inv)));
    CHECK(!ID3D10EffectSamplerVariable_IsValid(ID3D10EffectVariable_AsSampler(inv)));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_invalid_variable_navigation)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *inv = ID3D10Effect_GetVariableByName(fx, "nosuch");
    ID3D10EffectType *t = ID3D10EffectVariable_GetType(inv);
    CHECK(t && !ID3D10EffectType_IsValid(t));
    D3D10_EFFECT_TYPE_DESC td;
    EXPECT_HR(ID3D10EffectType_GetDesc(t, &td), E_FAIL_);
    ID3D10EffectVariable *e = ID3D10EffectVariable_GetElement(inv, 0);
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    e = ID3D10EffectVariable_GetMemberByIndex(inv, 0);
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    e = ID3D10EffectVariable_GetMemberByName(inv, "x");
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    e = ID3D10EffectVariable_GetMemberBySemantic(inv, "x");
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    e = ID3D10EffectVariable_GetAnnotationByIndex(inv, 0);
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    e = ID3D10EffectVariable_GetAnnotationByName(inv, "x");
    CHECK(e && !ID3D10EffectVariable_IsValid(e));
    ID3D10EffectConstantBuffer *p = ID3D10EffectVariable_GetParentConstantBuffer(inv);
    CHECK(p != NULL);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_invalid_type_navigation)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectType *t = ID3D10EffectVariable_GetType(ID3D10Effect_GetVariableByName(fx, "nosuch"));
    CHECK(!ID3D10EffectType_IsValid(t));
    ID3D10EffectType *m = ID3D10EffectType_GetMemberTypeByIndex(t, 0);
    CHECK(m && !ID3D10EffectType_IsValid(m));
    m = ID3D10EffectType_GetMemberTypeByName(t, "x");
    CHECK(m && !ID3D10EffectType_IsValid(m));
    m = ID3D10EffectType_GetMemberTypeBySemantic(t, "x");
    CHECK(m && !ID3D10EffectType_IsValid(m));
    EXPECT_EQ(ID3D10EffectType_GetMemberName(t, 0), NULL);
    EXPECT_EQ(ID3D10EffectType_GetMemberSemantic(t, 0), NULL);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_invalid_technique_and_pass_methods)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByName(fx, "nosuch");
    D3D10_TECHNIQUE_DESC td;
    D3D10_STATE_BLOCK_MASK mask;
    EXPECT_HR(ID3D10EffectTechnique_GetDesc(t, &td), E_FAIL_);
    EXPECT_HR(ID3D10EffectTechnique_ComputeStateBlockMask(t, &mask), E_FAIL_);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(t, 0);
    CHECK(p && !ID3D10EffectPass_IsValid(p));
    D3D10_PASS_DESC pd;
    D3D10_PASS_SHADER_DESC sd;
    EXPECT_HR(ID3D10EffectPass_GetDesc(p, &pd), E_FAIL_);
    EXPECT_HR(ID3D10EffectPass_Apply(p, 0), E_FAIL_);
    EXPECT_HR(ID3D10EffectPass_ComputeStateBlockMask(p, &mask), E_FAIL_);
    EXPECT_HR(ID3D10EffectPass_GetVertexShaderDesc(p, &sd), E_FAIL_);
    EXPECT_HR(ID3D10EffectPass_GetGeometryShaderDesc(p, &sd), E_FAIL_);
    EXPECT_HR(ID3D10EffectPass_GetPixelShaderDesc(p, &sd), E_FAIL_);
    ID3D10EffectVariable *a = ID3D10EffectPass_GetAnnotationByIndex(p, 0);
    CHECK(a && !ID3D10EffectVariable_IsValid(a));
    a = ID3D10EffectPass_GetAnnotationByName(p, "x");
    CHECK(a && !ID3D10EffectVariable_IsValid(a));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_invalid_constant_buffer_methods)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectConstantBuffer *c = ID3D10Effect_GetConstantBufferByName(fx, "nosuch");
    CHECK(!ID3D10EffectConstantBuffer_IsValid(c));
    ID3D10Buffer *b = (ID3D10Buffer *)(ULONG_PTR)1;
    ID3D10ShaderResourceView *srv = (ID3D10ShaderResourceView *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10EffectConstantBuffer_SetConstantBuffer(c, NULL), E_FAIL_);
    EXPECT_HR(ID3D10EffectConstantBuffer_GetConstantBuffer(c, &b), E_FAIL_);
    EXPECT_HR(ID3D10EffectConstantBuffer_SetTextureBuffer(c, NULL), E_FAIL_);
    EXPECT_HR(ID3D10EffectConstantBuffer_GetTextureBuffer(c, &srv), E_FAIL_);
    D3D10_EFFECT_VARIABLE_DESC d;
    EXPECT_HR(ID3D10EffectConstantBuffer_GetDesc(c, &d), E_FAIL_);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_invalid_objects_are_shared_per_kind)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectVariable *a = ID3D10Effect_GetVariableByName(fx, "n1"), *b = ID3D10Effect_GetVariableByIndex(fx, 99), *c = ID3D10Effect_GetVariableBySemantic(fx, "n2");
    EXPECT_EQ(a, b);
    EXPECT_EQ(b, c);
    ID3D10EffectVariable *d = ID3D10EffectVariable_GetElement(ID3D10Effect_GetVariableByName(fx, "fA"), 0);
    EXPECT_EQ(d, a);
    ID3D10Effect_Release(fx);
}

/* ---- Optimize ---- */
TEST(d3d10_utils_effects_fx_optimize_flags)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    EXPECT_EQ(ID3D10Effect_IsOptimized(fx), 0);
    EXPECT_HR(ID3D10Effect_Optimize(fx), S_OK);
    CHECK(ID3D10Effect_IsOptimized(fx) != 0);
    EXPECT_HR(ID3D10Effect_Optimize(fx), S_OK);
    CHECK(ID3D10Effect_IsOptimized(fx) != 0);
    CHECK(ID3D10Effect_IsValid(fx));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_optimize_removes_names)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    EXPECT_HR(ID3D10Effect_Optimize(fx), S_OK);
    ID3D10EffectVariable *v = ID3D10Effect_GetVariableByName(fx, "fA");
    CHECK(v && !ID3D10EffectVariable_IsValid(v));
    v = ID3D10Effect_GetVariableBySemantic(fx, "MYSEM");
    CHECK(v && !ID3D10EffectVariable_IsValid(v));
    ID3D10EffectConstantBuffer *c = ID3D10Effect_GetConstantBufferByName(fx, "cb");
    CHECK(c && !ID3D10EffectConstantBuffer_IsValid(c));
    ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByName(fx, "T0");
    CHECK(t && !ID3D10EffectTechnique_IsValid(t));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_optimize_keeps_indexing_and_values)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    EXPECT_HR(ID3D10Effect_Optimize(fx), S_OK);
    ID3D10EffectVariable *v = ID3D10Effect_GetVariableByIndex(fx, 1);
    CHECK(v && ID3D10EffectVariable_IsValid(v));
    ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(v);
    float f = 0;
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK);
    EXPECT_EQ(f == 1.5f, 1);
    EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(s, 8.25f), S_OK);
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(s, &f), S_OK);
    EXPECT_EQ(f == 8.25f, 1);
    ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByIndex(fx, 0);
    CHECK(t && ID3D10EffectTechnique_IsValid(t));
    D3D10_TECHNIQUE_DESC td;
    EXPECT_HR(ID3D10EffectTechnique_GetDesc(t, &td), S_OK);
    EXPECT_EQ(td.Passes, 2);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_optimize_drops_desc_names)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    EXPECT_HR(ID3D10Effect_Optimize(fx), S_OK);
    D3D10_EFFECT_VARIABLE_DESC d;
    EXPECT_HR(ID3D10EffectVariable_GetDesc(ID3D10Effect_GetVariableByIndex(fx, 0), &d), S_OK);
    t_note("variable desc after Optimize: name %s semantic %s", d.Name ? d.Name : "(null)", d.Semantic ? d.Semantic : "(null)");
    D3D10_TECHNIQUE_DESC td;
    EXPECT_HR(ID3D10EffectTechnique_GetDesc(ID3D10Effect_GetTechniqueByIndex(fx, 0), &td), S_OK);
    t_note("technique desc after Optimize: name %s", td.Name ? td.Name : "(null)");
    ID3D10Effect_Release(fx);
}

/* ---- DisassembleEffect ---- */
TEST(d3d10_utils_effects_fx_disassemble_args)
{
    NOWINE("D3D10DisassembleEffect is a stub");
    disasm_fx_t f = (disasm_fx_t)t_proc_in("d3d10.dll", "D3D10DisassembleEffect");
    REQUIRE(f);
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1;
    EXPECT_HR(f(NULL, FALSE, &b), E_D3DERR_INVALIDCALL);
    EXPECT_HR(f(fx, FALSE, NULL), E_D3DERR_INVALIDCALL);
    EXPECT_HR(f(NULL, FALSE, NULL), E_D3DERR_INVALIDCALL);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_disassemble_text)
{
    NOWINE("D3D10DisassembleEffect is a stub");
    disasm_fx_t f = (disasm_fx_t)t_proc_in("d3d10.dll", "D3D10DisassembleEffect");
    REQUIRE(f);
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10Blob *b = NULL;
    HRESULT hr = f(fx, FALSE, &b);
    EXPECT_HR(hr, S_OK);
    if (b) {
        size_t n = ID3D10Blob_GetBufferSize(b);
        char *t = malloc(n + 1);
        memcpy(t, ID3D10Blob_GetBufferPointer(b), n); t[n] = 0;
        CHECK(strstr(t, "technique10 T0") != NULL);
        CHECK(strstr(t, "technique10 T1") != NULL);
        CHECK(strstr(t, "pass P0") != NULL);
        CHECK(strstr(t, "pass P1") != NULL);
        CHECK(strstr(t, "<pre>") == NULL);
        free(t);
    }
    UE_REL(b);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fx_disassemble_color_code)
{
    NOWINE("D3D10DisassembleEffect is a stub");
    disasm_fx_t f = (disasm_fx_t)t_proc_in("d3d10.dll", "D3D10DisassembleEffect");
    REQUIRE(f);
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10Blob *b = NULL;
    EXPECT_HR(f(fx, TRUE, &b), S_OK);
    if (b) {
        const char *p = ID3D10Blob_GetBufferPointer(b);
        CHECK(ID3D10Blob_GetBufferSize(b) > 5 && !strncmp(p, "<pre>", 5));
    }
    UE_REL(b);
    ID3D10Effect_Release(fx);
}
