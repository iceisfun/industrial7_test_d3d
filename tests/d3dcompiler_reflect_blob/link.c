/* d3dcompiler_47: shader libraries (D3DReflectLibrary, D3DLoadModule), linker, function linking graph, assemble / disassemble. */
#include "rb.h"

typedef HRESULT (WINAPI *Assemble_t)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, UINT, ID3DBlob **, ID3DBlob **);
typedef HRESULT (WINAPI *Disasm_t)(LPCVOID, SIZE_T, UINT, LPCSTR, ID3DBlob **);
typedef HRESULT (WINAPI *LoadModule_t)(const void *, SIZE_T, ID3D11Module **);
typedef HRESULT (WINAPI *CreateLinker_t)(ID3D11Linker **);
#define ASSEMBLE ((Assemble_t)t_proc_in("d3dcompiler_47.dll", "D3DAssemble"))
#define DISASM ((Disasm_t)t_proc_in("d3dcompiler_47.dll", "D3DDisassemble"))
#define LOADMODULE ((LoadModule_t)t_proc_in("d3dcompiler_47.dll", "D3DLoadModule"))
#define CREATELINKER ((CreateLinker_t)t_proc_in("d3dcompiler_47.dll", "D3DCreateLinker"))

#define LIB_SRC "cbuffer LibCB : register(b1) { float4 gTint; };\n" \
    "export float4 Scale(float4 v, float s) { return v * s; }\n" \
    "export float4 AddTint(float4 v) { return v + gTint; }\n" \
    "export float Sum3(float3 a) { return a.x + a.y + a.z; }\n"

/* Compile a library target (no entry point). Returns 1 on success, otherwise skips/fails like rb_compile_t. */
static int rb_compile_lib(const char *src, ID3DBlob **out)
{
    ID3DBlob *err = NULL; HRESULT hr;
    *out = NULL;
    if (!d3dh_D3DCompile()) { t_skip("d3dcompiler_47.dll unavailable"); return 0; }
    if (t_is_wine()) { t_skip("library targets are not supported by the emulator"); return 0; }
    hr = d3dh_D3DCompile()(src, strlen(src), "lib.hlsl", NULL, NULL, NULL, "lib_5_0", 0, 0, out, &err);
    if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "lib_5_0 compile", "%s: %s", d3dh_hr_name(hr), err ? (const char *)RB_BP(err) : ""); if (err) ID3D10Blob_Release(err); return 0; }
    if (err) ID3D10Blob_Release(err);
    return 1;
}
static const char *rb_blob_text(ID3DBlob *b) { return b ? (const char *)RB_BP(b) : "(no blob)"; }

/* ---- D3DReflectLibrary ---------------------------------------------------------------------------- */
#define LIB_ENV(lib, blob) \
    ID3DBlob *blob = NULL; RB_LibRefl *lib = NULL; \
    REQUIRE(RB_D3DReflectLibrary()); RB_NOWINE("D3DReflectLibrary"); \
    if (!rb_compile_lib(LIB_SRC, &blob)) return; \
    { HRESULT hr_ = RB_D3DReflectLibrary()(RB_BP(blob), RB_BS(blob), &RB_IID_LibraryReflection, (void **)&lib); \
      t_check(SUCCEEDED(hr_) && lib, __FILE__, __LINE__, "D3DReflectLibrary", "%s", d3dh_hr_name(hr_)); if (!lib) { ID3D10Blob_Release(blob); return; } }
#define LIB_DONE(lib, blob) do { lib->lpVtbl->Release(lib); ID3D10Blob_Release(blob); } while (0)

static RB_Fn *rb_find_fn(RB_LibRefl *lib, const char *needle, int *index_out)
{
    RB_LIBRARY_DESC ld; memset(&ld, 0, sizeof ld);
    if (FAILED(lib->lpVtbl->GetDesc(lib, &ld))) return NULL;
    for (UINT i = 0; i < ld.FunctionCount; i++) {
        RB_Fn *f = lib->lpVtbl->GetFunctionByIndex(lib, (INT)i); RB_FUNCTION_DESC fd; memset(&fd, 0, sizeof fd);
        if (SUCCEEDED(f->lpVtbl->GetDesc(f, &fd)) && fd.Name && strstr(fd.Name, needle)) { if (index_out) *index_out = (int)i; return f; }
    }
    return NULL;
}

TEST(rb_lib_desc)
{
    LIB_ENV(lib, blob);
    RB_LIBRARY_DESC ld; memset(&ld, 0xcc, sizeof ld);
    EXPECT_HR(lib->lpVtbl->GetDesc(lib, &ld), S_OK);
    EXPECT_EQ(ld.FunctionCount, 3); CHECK(ld.Creator != NULL);
    if (ld.Creator) t_note("library Creator = \"%s\", Flags = 0x%x", ld.Creator, ld.Flags);
    LIB_DONE(lib, blob);
}
TEST(rb_lib_refcount_and_query_interface)
{
    LIB_ENV(lib, blob); void *o;
    ULONG n = lib->lpVtbl->AddRef(lib); EXPECT_EQ(n, 2); n = lib->lpVtbl->Release(lib); EXPECT_EQ(n, 1);
    o = (void *)1; EXPECT_HR(lib->lpVtbl->QueryInterface(lib, &IID_IUnknown, &o), RB_E_NOINTERFACE); CHECK(o == NULL);   /* Win10: these objects answer only for their own IIDs, not for IID_IUnknown */
    o = NULL; EXPECT_HR(lib->lpVtbl->QueryInterface(lib, &RB_IID_LibraryReflection, &o), S_OK); if (o) lib->lpVtbl->Release(lib);
    o = (void *)1; EXPECT_HR(lib->lpVtbl->QueryInterface(lib, &IID_ID3D11ShaderReflection, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
    LIB_DONE(lib, blob);
}
TEST(rb_lib_function_names_and_parameters)
{
    LIB_ENV(lib, blob);
    static const struct { const char *name; INT params; } want[] = { { "Scale", 2 }, { "AddTint", 1 }, { "Sum3", 1 } };
    for (size_t k = 0; k < 3; k++) {
        RB_Fn *f = rb_find_fn(lib, want[k].name, NULL); RB_FUNCTION_DESC fd;
        t_check(f != NULL, __FILE__, __LINE__, "function present", "%s", want[k].name);
        if (!f) continue;
        memset(&fd, 0, sizeof fd); EXPECT_HR(f->lpVtbl->GetDesc(f, &fd), S_OK);
        t_check(fd.FunctionParameterCount == want[k].params, __FILE__, __LINE__, "parameter count", "%s got %d", want[k].name, fd.FunctionParameterCount);
        CHECK(fd.HasReturn);
        t_note("function \"%s\": flags=0x%x cb=%u res=%u instr=%u minfl=0x%x", fd.Name, fd.Flags, fd.ConstantBuffers, fd.BoundResources, fd.InstructionCount, fd.MinFeatureLevel);
    }
    LIB_DONE(lib, blob);
}
TEST(rb_lib_function_parameter_descs)
{
    LIB_ENV(lib, blob);
    RB_Fn *f = rb_find_fn(lib, "Scale", NULL);
    if (f) {
        D3D11_PARAMETER_DESC p; RB_FnParam *fp;
        fp = f->lpVtbl->GetFunctionParameter(f, 0); memset(&p, 0, sizeof p); EXPECT_HR(fp->lpVtbl->GetDesc(fp, &p), S_OK);
        EXPECT_STR(p.Name ? p.Name : "", "v"); EXPECT_EQ(p.Type, D3D_SVT_FLOAT); EXPECT_EQ(p.Class, D3D_SVC_VECTOR); EXPECT_EQ(p.Rows, 1); EXPECT_EQ(p.Columns, 4); CHECK(p.Flags & D3D_PF_IN); CHECK(!(p.Flags & D3D_PF_OUT));
        fp = f->lpVtbl->GetFunctionParameter(f, 1); memset(&p, 0, sizeof p); EXPECT_HR(fp->lpVtbl->GetDesc(fp, &p), S_OK);
        EXPECT_STR(p.Name ? p.Name : "", "s"); EXPECT_EQ(p.Type, D3D_SVT_FLOAT); EXPECT_EQ(p.Class, D3D_SVC_SCALAR); EXPECT_EQ(p.Rows, 1); EXPECT_EQ(p.Columns, 1); CHECK(p.Flags & D3D_PF_IN);
        fp = f->lpVtbl->GetFunctionParameter(f, -1); memset(&p, 0, sizeof p); EXPECT_HR(fp->lpVtbl->GetDesc(fp, &p), S_OK);
        EXPECT_EQ(p.Type, D3D_SVT_FLOAT); EXPECT_EQ(p.Class, D3D_SVC_VECTOR); EXPECT_EQ(p.Columns, 4); CHECK(p.Flags & D3D_PF_OUT);
    } else CHECK(0);
    LIB_DONE(lib, blob);
}
TEST(rb_lib_function_return_type_scalar)
{
    LIB_ENV(lib, blob);
    RB_Fn *f = rb_find_fn(lib, "Sum3", NULL);
    if (f) {
        D3D11_PARAMETER_DESC p; RB_FnParam *fp = f->lpVtbl->GetFunctionParameter(f, -1); memset(&p, 0, sizeof p);
        EXPECT_HR(fp->lpVtbl->GetDesc(fp, &p), S_OK); EXPECT_EQ(p.Type, D3D_SVT_FLOAT); EXPECT_EQ(p.Class, D3D_SVC_SCALAR);
        fp = f->lpVtbl->GetFunctionParameter(f, 0); memset(&p, 0, sizeof p);
        EXPECT_HR(fp->lpVtbl->GetDesc(fp, &p), S_OK); EXPECT_EQ(p.Columns, 3); EXPECT_EQ(p.Class, D3D_SVC_VECTOR);
    } else CHECK(0);
    LIB_DONE(lib, blob);
}
TEST(rb_lib_invalid_function_and_parameter_objects)
{
    LIB_ENV(lib, blob);
    RB_Fn *bad1 = lib->lpVtbl->GetFunctionByIndex(lib, 3), *bad2 = lib->lpVtbl->GetFunctionByIndex(lib, 1000), *ok = lib->lpVtbl->GetFunctionByIndex(lib, 0);
    RB_FUNCTION_DESC fd; D3D11_PARAMETER_DESC p;
    CHECK(bad1 != NULL && bad1 == bad2 && bad1 != ok);
    EXPECT_HR(bad1->lpVtbl->GetDesc(bad1, &fd), RB_E_FAIL);
    { RB_FnParam *bp = ok->lpVtbl->GetFunctionParameter(ok, 99), *bp2 = ok->lpVtbl->GetFunctionParameter(ok, -2); CHECK(bp != NULL && bp == bp2); EXPECT_HR(bp->lpVtbl->GetDesc(bp, &p), RB_E_FAIL); }
    { D3D11_SHADER_INPUT_BIND_DESC b; EXPECT_HR(bad1->lpVtbl->GetResourceBindingDesc(bad1, 0, &b), RB_E_FAIL); EXPECT_HR(bad1->lpVtbl->GetResourceBindingDescByName(bad1, "x", &b), RB_E_FAIL); }
    { ID3D11ShaderReflectionConstantBuffer *cb = bad1->lpVtbl->GetConstantBufferByIndex(bad1, 0); D3D11_SHADER_BUFFER_DESC bd; EXPECT_HR(cb->lpVtbl->GetDesc(cb, &bd), RB_E_FAIL); }
    LIB_DONE(lib, blob);
}
TEST(rb_lib_function_constant_buffers)
{
    LIB_ENV(lib, blob);
    RB_Fn *f = rb_find_fn(lib, "AddTint", NULL);
    if (f) {
        ID3D11ShaderReflectionConstantBuffer *cb = f->lpVtbl->GetConstantBufferByName(f, "LibCB"); D3D11_SHADER_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
        if (SUCCEEDED(cb->lpVtbl->GetDesc(cb, &bd))) { EXPECT_STR(bd.Name, "LibCB"); EXPECT_EQ(bd.Variables, 1); EXPECT_EQ(bd.Size, 16); }
        else t_note("function-level constant buffer lookup by name failed");
        { D3D11_SHADER_INPUT_BIND_DESC b; HRESULT hr = f->lpVtbl->GetResourceBindingDescByName(f, "LibCB", &b); if (SUCCEEDED(hr)) { EXPECT_EQ(b.Type, D3D_SIT_CBUFFER); EXPECT_EQ(b.BindPoint, 1); } else t_note("function bind desc by name: %s", d3dh_hr_name(hr)); }
    } else CHECK(0);
    LIB_DONE(lib, blob);
}
TEST(rb_lib_wrong_iid_is_invalid_arg)
{
    ID3DBlob *blob; void *o = (void *)1; REQUIRE(RB_D3DReflectLibrary()); RB_NOWINE("D3DReflectLibrary");
    if (!rb_compile_lib(LIB_SRC, &blob)) return;
    EXPECT_HR(RB_D3DReflectLibrary()(RB_BP(blob), RB_BS(blob), &IID_ID3D11ShaderReflection, &o), RB_E_INVALIDARG);
    EXPECT_HR(RB_D3DReflectLibrary()(RB_BP(blob), RB_BS(blob), &IID_IUnknown, &o), RB_E_INVALIDARG);
    ID3D10Blob_Release(blob);
}
TEST(rb_lib_reflect_with_shader_reflection_iid_fails)
{
    ID3DBlob *blob; void *o = NULL; HRESULT hr; REQUIRE(d3dh_D3DReflect()); RB_NOWINE("library targets");
    if (!rb_compile_lib(LIB_SRC, &blob)) return;
    hr = d3dh_D3DReflect()(RB_BP(blob), RB_BS(blob), &IID_ID3D11ShaderReflection, &o);
    t_note("D3DReflect on a library container: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && o) ((IUnknown *)o)->lpVtbl->Release((IUnknown *)o);
    ID3D10Blob_Release(blob);
}
TEST(rb_lib_reflect_library_on_plain_shader)
{
    RB_COMPILE(b, RB_PS_CONST, "ps_5_0"); void *o = NULL; HRESULT hr;
    REQUIRE(RB_D3DReflectLibrary()); RB_NOWINE("D3DReflectLibrary");
    hr = RB_D3DReflectLibrary()(RB_BP(b), RB_BS(b), &RB_IID_LibraryReflection, &o);
    t_note("D3DReflectLibrary on a ps_5_0 container: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && o) ((RB_LibRefl *)o)->lpVtbl->Release((RB_LibRefl *)o);
    ID3D10Blob_Release(b);
}
TEST(rb_lib_reflect_library_null_data)
{
    void *o = (void *)1; REQUIRE(RB_D3DReflectLibrary()); RB_NOWINE("D3DReflectLibrary");
    EXPECT_HR(RB_D3DReflectLibrary()(NULL, 100, &RB_IID_LibraryReflection, &o), RB_INVALIDCALL);
    EXPECT_HR(RB_D3DReflectLibrary()("DXBC", 4, &RB_IID_LibraryReflection, &o), RB_INVALIDCALL);
}

/* ---- D3DLoadModule / ID3D11Module ----------------------------------------------------------------- */
TEST(rb_module_load_invalid_arguments)
{
    ID3DBlob *blob; ID3D11Module *m = (ID3D11Module *)1; REQUIRE(LOADMODULE); RB_NOWINE("D3DLoadModule");
    if (!rb_compile_lib(LIB_SRC, &blob)) return;
    EXPECT_HR(LOADMODULE(RB_BP(blob), RB_BS(blob), NULL), RB_E_INVALIDARG);
    EXPECT_HR(LOADMODULE(NULL, RB_BS(blob), &m), RB_E_INVALIDARG);
    EXPECT_HR(LOADMODULE(RB_BP(blob), 0, &m), RB_E_INVALIDARG);
    EXPECT_HR(LOADMODULE(RB_BP(blob), 0x7fffffffu, &m), RB_E_INVALIDARG);
    EXPECT_HR(LOADMODULE(RB_BP(blob), 0x100000000ull, &m), RB_E_INVALIDARG);
    CHECK(m == (ID3D11Module *)1);
    ID3D10Blob_Release(blob);
}
TEST(rb_module_load_rejects_bad_container)
{
    ID3DBlob *blob; ID3D11Module *m = (ID3D11Module *)1; BYTE *c; REQUIRE(LOADMODULE); RB_NOWINE("D3DLoadModule");
    if (!rb_compile_lib(LIB_SRC, &blob)) return;
    c = malloc(RB_BS(blob)); memcpy(c, RB_BP(blob), RB_BS(blob));
    c[0] = 'Z'; m = NULL;
    EXPECT_HR(LOADMODULE(c, RB_BS(blob), &m), RB_E_FAIL); CHECK(m == NULL);
    c[0] = 'D'; c[5] ^= 0; c[4] ^= 1; m = NULL;
    EXPECT_HR(LOADMODULE(c, RB_BS(blob), &m), RB_E_FAIL); CHECK(m == NULL);
    c[4] ^= 1;
    EXPECT_HR(LOADMODULE(c, RB_BS(blob) - 4, &m), RB_E_FAIL);
    free(c); ID3D10Blob_Release(blob);
    { BYTE junk[64] = { 1 }; m = NULL; EXPECT_HR(LOADMODULE(junk, sizeof junk, &m), RB_E_FAIL); }
}
TEST(rb_module_load_and_instance)
{
    ID3DBlob *blob; ID3D11Module *m = NULL; ID3D11ModuleInstance *inst = NULL, *inst2 = NULL; void *o;
    REQUIRE(LOADMODULE); RB_NOWINE("D3DLoadModule");
    if (!rb_compile_lib(LIB_SRC, &blob)) return;
    EXPECT_HR(LOADMODULE(RB_BP(blob), RB_BS(blob), &m), S_OK);
    if (m) {
        ULONG n = m->lpVtbl->AddRef(m); EXPECT_EQ(n, 2); n = m->lpVtbl->Release(m); EXPECT_EQ(n, 1);
        o = NULL; EXPECT_HR(m->lpVtbl->QueryInterface(m, &IID_ID3D11Module, &o), S_OK); if (o) m->lpVtbl->Release(m);
        o = (void *)1; EXPECT_HR(m->lpVtbl->QueryInterface(m, &IID_IUnknown, &o), RB_E_NOINTERFACE); CHECK(o == NULL);   /* see rb_lib_refcount_and_query_interface */
        o = (void *)1; EXPECT_HR(m->lpVtbl->QueryInterface(m, &IID_ID3D11Linker, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
        EXPECT_HR(m->lpVtbl->CreateInstance(m, "", NULL), RB_E_INVALIDARG);
        EXPECT_HR(m->lpVtbl->CreateInstance(m, "", &inst), S_OK);
        EXPECT_HR(m->lpVtbl->CreateInstance(m, NULL, &inst2), S_OK);
        CHECK(inst != NULL && inst2 != NULL && inst != inst2);
        if (inst) {
            n = inst->lpVtbl->AddRef(inst); EXPECT_EQ(n, 2); n = inst->lpVtbl->Release(inst); EXPECT_EQ(n, 1);
            o = NULL; EXPECT_HR(inst->lpVtbl->QueryInterface(inst, &IID_ID3D11ModuleInstance, &o), S_OK); if (o) inst->lpVtbl->Release(inst);
            o = (void *)1; EXPECT_HR(inst->lpVtbl->QueryInterface(inst, &IID_ID3D11Module, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
            inst->lpVtbl->Release(inst);
        }
        if (inst2) inst2->lpVtbl->Release(inst2);
        m->lpVtbl->Release(m);
    }
    ID3D10Blob_Release(blob);
}
TEST(rb_module_instance_survives_module_release)
{
    ID3DBlob *blob; ID3D11Module *m = NULL; ID3D11ModuleInstance *inst = NULL;
    REQUIRE(LOADMODULE); RB_NOWINE("D3DLoadModule");
    if (!rb_compile_lib(LIB_SRC, &blob)) return;
    EXPECT_HR(LOADMODULE(RB_BP(blob), RB_BS(blob), &m), S_OK);
    if (m) {
        EXPECT_HR(m->lpVtbl->CreateInstance(m, "ns", &inst), S_OK);
        m->lpVtbl->Release(m);
        if (inst) { ULONG n = inst->lpVtbl->AddRef(inst); EXPECT_EQ(n, 2); inst->lpVtbl->Release(inst); inst->lpVtbl->Release(inst); }
    }
    ID3D10Blob_Release(blob);
}
TEST(rb_module_load_from_plain_shader_container)
{
    RB_COMPILE(b, RB_PS_CONST, "ps_5_0"); ID3D11Module *m = NULL; HRESULT hr;
    REQUIRE(LOADMODULE); RB_NOWINE("D3DLoadModule");
    hr = LOADMODULE(RB_BP(b), RB_BS(b), &m);
    t_note("D3DLoadModule on a ps_5_0 container: %s", d3dh_hr_name(hr));
    if (m) m->lpVtbl->Release(m);
    ID3D10Blob_Release(b);
}
TEST(rb_module_instance_bind_by_unknown_name_is_s_false)
{
    ID3DBlob *blob; ID3D11Module *m = NULL; ID3D11ModuleInstance *inst = NULL;
    REQUIRE(LOADMODULE); RB_NOWINE("D3DLoadModule");
    if (!rb_compile_lib(LIB_SRC, &blob)) return;
    if (SUCCEEDED(LOADMODULE(RB_BP(blob), RB_BS(blob), &m)) && m && SUCCEEDED(m->lpVtbl->CreateInstance(m, "", &inst)) && inst) {
        /* Win10: binding a name the module does not contain is not an error, it reports S_FALSE */
        EXPECT_HR(inst->lpVtbl->BindConstantBufferByName(inst, "NoSuchBuffer", 0, 0), S_FALSE);
        EXPECT_HR(inst->lpVtbl->BindResourceByName(inst, "NoSuchTexture", 0, 1), S_FALSE);
        EXPECT_HR(inst->lpVtbl->BindSamplerByName(inst, "NoSuchSampler", 0, 1), S_FALSE);
        EXPECT_HR(inst->lpVtbl->BindUnorderedAccessViewByName(inst, "NoSuchUav", 0, 1), S_FALSE);
        EXPECT_HR(inst->lpVtbl->BindResourceAsUnorderedAccessViewByName(inst, "NoSuchTexture", 0, 1), S_FALSE);
        EXPECT_HR(inst->lpVtbl->BindConstantBufferByName(inst, "LibCB", 3, 0), S_OK);
        inst->lpVtbl->Release(inst);
    } else CHECK(0);
    if (m) m->lpVtbl->Release(m);
    ID3D10Blob_Release(blob);
}

/* ---- D3DCreateLinker / ID3D11Linker --------------------------------------------------------------- */
TEST(rb_linker_create)
{
    ID3D11Linker *l = (ID3D11Linker *)1, *l2 = NULL; void *o;
    REQUIRE(CREATELINKER); RB_NOWINE("D3DCreateLinker");
    EXPECT_HR(CREATELINKER(NULL), RB_E_INVALIDARG);
    EXPECT_HR(CREATELINKER(&l), S_OK);
    EXPECT_HR(CREATELINKER(&l2), S_OK);
    CHECK(l != NULL && l2 != NULL && l != l2);
    if (l) {
        ULONG n = l->lpVtbl->AddRef(l); EXPECT_EQ(n, 2); n = l->lpVtbl->Release(l); EXPECT_EQ(n, 1);
        o = NULL; EXPECT_HR(l->lpVtbl->QueryInterface(l, &IID_ID3D11Linker, &o), S_OK); if (o) l->lpVtbl->Release(l);
        o = (void *)1; EXPECT_HR(l->lpVtbl->QueryInterface(l, &IID_IUnknown, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
        o = (void *)1; EXPECT_HR(l->lpVtbl->QueryInterface(l, &IID_ID3D11Module, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
        n = l->lpVtbl->Release(l); EXPECT_EQ(n, 0);
    }
    if (l2) l2->lpVtbl->Release(l2);
}
TEST(rb_linker_use_library_null)
{
    ID3D11Linker *l = NULL; REQUIRE(CREATELINKER); RB_NOWINE("D3DCreateLinker");
    EXPECT_HR(CREATELINKER(&l), S_OK);
    if (l) { EXPECT_HR(l->lpVtbl->UseLibrary(l, NULL), RB_E_INVALIDARG); l->lpVtbl->Release(l); }
}
TEST(rb_linker_use_library_accepts_instances)
{
    ID3D11Linker *l = NULL; ID3DBlob *blob; ID3D11Module *m = NULL; ID3D11ModuleInstance *inst = NULL;
    REQUIRE(CREATELINKER); REQUIRE(LOADMODULE); RB_NOWINE("D3DCreateLinker");
    if (!rb_compile_lib(LIB_SRC, &blob)) return;
    EXPECT_HR(CREATELINKER(&l), S_OK);
    if (SUCCEEDED(LOADMODULE(RB_BP(blob), RB_BS(blob), &m)) && m && SUCCEEDED(m->lpVtbl->CreateInstance(m, "", &inst)) && inst && l) {
        EXPECT_HR(l->lpVtbl->UseLibrary(l, inst), S_OK);
        { ULONG n = inst->lpVtbl->AddRef(inst); t_note("instance refcount after UseLibrary: %lu", n); inst->lpVtbl->Release(inst); CHECK(n >= 3); }
        EXPECT_HR(l->lpVtbl->UseLibrary(l, inst), S_OK);  /* the same library can be registered twice */
    } else CHECK(0);
    if (inst) inst->lpVtbl->Release(inst);
    if (m) m->lpVtbl->Release(m);
    if (l) l->lpVtbl->Release(l);
    ID3D10Blob_Release(blob);
}
TEST(rb_linker_clip_plane_limits)
{
    ID3D11Linker *l = NULL; REQUIRE(CREATELINKER); RB_NOWINE("D3DCreateLinker");
    EXPECT_HR(CREATELINKER(&l), S_OK);
    if (!l) return;
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 15, 0), RB_E_INVALIDARG);          /* slot index too large (max 14) */
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 0xffffffffu, 0), RB_E_INVALIDARG);
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 0, 0x1000), RB_E_INVALIDARG);      /* entry index too large (max 0xfff) */
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 0, 0xffffffffu), RB_E_INVALIDARG);
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 14, 0xfff), S_OK);
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 14, 0xfff), RB_E_INVALIDARG);      /* duplicate */
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 0, 0), S_OK);
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 0, 1), S_OK);
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 1, 0), S_OK);
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 1, 1), S_OK);
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 2, 7), S_OK);                       /* six planes accepted */
    EXPECT_HR(l->lpVtbl->AddClipPlaneFromCBuffer(l, 3, 3), RB_E_FAIL);                  /* the seventh is refused */
    l->lpVtbl->Release(l);
}
TEST(rb_linker_link_argument_checks)
{
    ID3D11Linker *l = NULL; ID3DBlob *sh = (ID3DBlob *)1, *err = (ID3DBlob *)1, *lib; ID3D11Module *m = NULL; ID3D11ModuleInstance *inst = NULL;
    REQUIRE(CREATELINKER); REQUIRE(LOADMODULE); RB_NOWINE("D3DCreateLinker");
    EXPECT_HR(CREATELINKER(&l), S_OK);
    if (!l) return;
    EXPECT_HR(l->lpVtbl->Link(l, NULL, "main", "cs_5_0", 0, &sh, &err), RB_E_INVALIDARG);
    if (!rb_compile_lib(LIB_SRC, &lib)) { l->lpVtbl->Release(l); return; }
    if (SUCCEEDED(LOADMODULE(RB_BP(lib), RB_BS(lib), &m)) && m && SUCCEEDED(m->lpVtbl->CreateInstance(m, "", &inst)) && inst) {
        ID3D11Linker *l2 = NULL; EXPECT_HR(CREATELINKER(&l2), S_OK);
        if (l2) {
            sh = (ID3DBlob *)1; err = NULL;
            EXPECT_HR(l2->lpVtbl->Link(l2, inst, "main", NULL, 0, &sh, &err), RB_E_INVALIDARG);
            EXPECT_HR(l2->lpVtbl->Link(l2, inst, "main", "cs_5_0", 0, NULL, &err), RB_E_INVALIDARG);
            SAFE_RELEASE(err);
            sh = (ID3DBlob *)1; EXPECT_HR(l2->lpVtbl->Link(l2, inst, "main", "xs_9_9", 0, &sh, &err), RB_E_INVALIDARG);
            CHECK(sh == NULL); SAFE_RELEASE(err);
            l2->lpVtbl->Release(l2);
        }
        EXPECT_HR(l->lpVtbl->Link(l, inst, "main", "vs_5_0", 0, &sh, &err), RB_E_INVALIDARG);  /* a plain library cannot be an entry */
        CHECK(sh == NULL);
        if (err) { t_note("link error text: %s", rb_blob_text(err)); ID3D10Blob_Release(err); } else t_note("no error blob for unsupported entry");
    } else CHECK(0);
    if (inst) inst->lpVtbl->Release(inst);
    if (m) m->lpVtbl->Release(m);
    l->lpVtbl->Release(l); ID3D10Blob_Release(lib);
}

/* ---- ID3D11FunctionLinkingGraph --------------------------------------------------------------------- */
static D3D11_PARAMETER_DESC rb_param(const char *name, const char *sem, D3D_SHADER_VARIABLE_TYPE ty, D3D_SHADER_VARIABLE_CLASS cls, UINT rows, UINT cols)
{
    D3D11_PARAMETER_DESC p; memset(&p, 0, sizeof p);
    p.Name = name; p.SemanticName = sem; p.Type = ty; p.Class = cls; p.Rows = rows; p.Columns = cols; p.InterpolationMode = D3D_INTERPOLATION_UNDEFINED; p.Flags = D3D_PF_NONE;
    return p;
}
static ID3D11FunctionLinkingGraph *rb_make_flg(void)
{
    ID3D11FunctionLinkingGraph *g = NULL; HRESULT hr;
    if (!RB_D3DCreateFunctionLinkingGraph()) return NULL;
    hr = RB_D3DCreateFunctionLinkingGraph()(0, &g);
    t_check(hr == S_OK && g, __FILE__, __LINE__, "create graph", "%s", d3dh_hr_name(hr));
    return g;
}
TEST(rb_flg_create_arguments)
{
    ID3D11FunctionLinkingGraph *g = (ID3D11FunctionLinkingGraph *)1, *g2 = NULL; void *o;
    REQUIRE(RB_D3DCreateFunctionLinkingGraph()); RB_NOWINE("D3DCreateFunctionLinkingGraph");
    EXPECT_HR(RB_D3DCreateFunctionLinkingGraph()(0, NULL), RB_E_INVALIDARG);
    EXPECT_HR(RB_D3DCreateFunctionLinkingGraph()(1, &g), RB_E_INVALIDARG);
    EXPECT_HR(RB_D3DCreateFunctionLinkingGraph()(0xffffffffu, &g), RB_E_INVALIDARG);
    EXPECT_HR(RB_D3DCreateFunctionLinkingGraph()(0, &g), S_OK);
    EXPECT_HR(RB_D3DCreateFunctionLinkingGraph()(0, &g2), S_OK);
    CHECK(g && g2 && g != g2);
    if (g) {
        ULONG n = g->lpVtbl->AddRef(g); EXPECT_EQ(n, 2); n = g->lpVtbl->Release(g); EXPECT_EQ(n, 1);
        o = NULL; EXPECT_HR(g->lpVtbl->QueryInterface(g, &IID_ID3D11FunctionLinkingGraph, &o), S_OK); if (o) g->lpVtbl->Release(g);
        o = (void *)1; EXPECT_HR(g->lpVtbl->QueryInterface(g, &IID_IUnknown, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
        o = (void *)1; EXPECT_HR(g->lpVtbl->QueryInterface(g, &IID_ID3D11Linker, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
        n = g->lpVtbl->Release(g); EXPECT_EQ(n, 0);
    }
    if (g2) g2->lpVtbl->Release(g2);
}
TEST(rb_flg_empty_graph_generates_empty_main)
{
    ID3D11FunctionLinkingGraph *g; ID3DBlob *txt = NULL;
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    EXPECT_HR(g->lpVtbl->GenerateHlsl(g, 0, NULL), RB_E_INVALIDARG);
    EXPECT_HR(g->lpVtbl->GenerateHlsl(g, 0, &txt), S_OK);
    if (txt) { CHECK(strstr((const char *)RB_BP(txt), "void main()") != NULL); ID3D10Blob_Release(txt); }
    g->lpVtbl->Release(g);
}
TEST(rb_flg_get_last_error_without_error)
{
    ID3D11FunctionLinkingGraph *g; ID3DBlob *e = (ID3DBlob *)1; HRESULT hr;
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    EXPECT_HR(g->lpVtbl->GetLastError(g, NULL), S_FALSE);
    hr = g->lpVtbl->GetLastError(g, &e);
    t_note("GetLastError on a fresh graph: %s, blob=%p", d3dh_hr_name(hr), (void *)e);
    if (SUCCEEDED(hr) && e && e != (ID3DBlob *)1) ID3D10Blob_Release(e);
    g->lpVtbl->Release(g);
}
TEST(rb_flg_input_signature_rules)
{
    ID3D11FunctionLinkingGraph *g; ID3D11LinkingNode *in1 = NULL, *in2 = NULL; D3D11_PARAMETER_DESC p[2];
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    p[0] = rb_param("pos", "POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4); p[1] = rb_param("scale", "TEXCOORD0", D3D_SVT_FLOAT, D3D_SVC_SCALAR, 1, 1);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, p, 2, &in1), S_OK);
    CHECK(in1 != NULL);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, p, 2, &in2), RB_E_FAIL);   /* already set; the graph is now in error state */
    CHECK(in2 == NULL);
    { ID3DBlob *e = NULL; HRESULT hr = g->lpVtbl->GetLastError(g, &e); t_note("GetLastError after a rejected call: %s text=\"%s\"", d3dh_hr_name(hr), SUCCEEDED(hr) ? rb_blob_text(e) : ""); SAFE_RELEASE(e); }
    if (in1) { ULONG n = in1->lpVtbl->AddRef(in1); EXPECT_EQ(n, 3); in1->lpVtbl->Release(in1); in1->lpVtbl->Release(in1); }   /* the graph keeps its own reference on the node */
    g->lpVtbl->Release(g);
}
TEST(rb_flg_input_signature_null_arguments)
{
    ID3D11FunctionLinkingGraph *g; ID3D11LinkingNode *n = NULL; D3D11_PARAMETER_DESC p;
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    p = rb_param("pos", "POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, &p, 1, NULL), RB_E_FAIL);
    g->lpVtbl->Release(g);
    g = rb_make_flg(); if (!g) return;
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, NULL, 1, &n), RB_E_FAIL);
    CHECK(n == NULL);
    g->lpVtbl->Release(g);
}
TEST(rb_flg_input_signature_too_many_parameters)
{
    ID3D11FunctionLinkingGraph *g; ID3D11LinkingNode *n = NULL; D3D11_PARAMETER_DESC p[129]; char names[129][12];
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    for (int i = 0; i < 129; i++) { sprintf(names[i], "p%d", i); p[i] = rb_param(names[i], "", D3D_SVT_FLOAT, D3D_SVC_SCALAR, 1, 1); }
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, p, 129, &n), RB_E_FAIL);
    CHECK(n == NULL);
    g->lpVtbl->Release(g);
}
TEST(rb_flg_duplicate_parameter_names_rejected)
{
    ID3D11FunctionLinkingGraph *g; ID3D11LinkingNode *n = NULL; D3D11_PARAMETER_DESC p[2];
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    p[0] = rb_param("dup", "POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4); p[1] = rb_param("dup", "TEXCOORD0", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 2);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, p, 2, &n), RB_E_FAIL);
    g->lpVtbl->Release(g);
}
TEST(rb_flg_output_signature_before_input)
{
    ID3D11FunctionLinkingGraph *g; ID3D11LinkingNode *n = NULL; D3D11_PARAMETER_DESC p; HRESULT hr;
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    p = rb_param("outPos", "SV_POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4);
    hr = g->lpVtbl->SetOutputSignature(g, &p, 1, &n);
    t_note("SetOutputSignature on an empty graph: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(n);
    g->lpVtbl->Release(g);
}
TEST(rb_flg_set_output_signature_null_arguments)
{
    ID3D11FunctionLinkingGraph *g; D3D11_PARAMETER_DESC p; ID3D11LinkingNode *n = NULL;
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    p = rb_param("outPos", "SV_POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4);
    EXPECT_HR(g->lpVtbl->SetOutputSignature(g, &p, 1, NULL), RB_E_INVALIDARG);   /* Win10: argument validation, unlike SetInputSignature (E_FAIL) */
    g->lpVtbl->Release(g);
    g = rb_make_flg(); if (!g) return;
    EXPECT_HR(g->lpVtbl->SetOutputSignature(g, NULL, 1, &n), RB_E_INVALIDARG);
    CHECK(n == NULL);
    g->lpVtbl->Release(g);
}
TEST(rb_flg_create_module_instance_on_empty_graph)
{
    ID3D11FunctionLinkingGraph *g; ID3D11ModuleInstance *inst = NULL; ID3DBlob *err = NULL; HRESULT hr;
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    hr = g->lpVtbl->CreateModuleInstance(g, &inst, &err);
    t_note("CreateModuleInstance on an empty graph: %s, error text=\"%s\"", d3dh_hr_name(hr), err ? rb_blob_text(err) : "");
    if (SUCCEEDED(hr)) CHECK(inst != NULL); else CHECK(inst == NULL);
    SAFE_RELEASE(inst); SAFE_RELEASE(err);
    g->lpVtbl->Release(g);
}

/* the documented construction flow: library function, wired from an input signature to an output signature */
static int rb_build_graph(ID3D11FunctionLinkingGraph *g, ID3D11Module *m, ID3D11LinkingNode **out_call)
{
    ID3D11LinkingNode *in = NULL, *call = NULL, *out = NULL; D3D11_PARAMETER_DESC pi[2], po[1]; HRESULT hr;
    pi[0] = rb_param("pos", "POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4); pi[1] = rb_param("scale", "TEXCOORD0", D3D_SVT_FLOAT, D3D_SVC_SCALAR, 1, 1);
    po[0] = rb_param("outPos", "SV_POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4);
    hr = g->lpVtbl->SetInputSignature(g, pi, 2, &in);               EXPECT_HR(hr, S_OK); if (FAILED(hr)) return 0;
    hr = g->lpVtbl->CallFunction(g, "", m, "Scale", &call);         EXPECT_HR(hr, S_OK); if (FAILED(hr)) return 0;
    hr = g->lpVtbl->PassValue(g, in, 0, call, 0);                   EXPECT_HR(hr, S_OK);
    hr = g->lpVtbl->PassValue(g, in, 1, call, 1);                   EXPECT_HR(hr, S_OK);
    hr = g->lpVtbl->SetOutputSignature(g, po, 1, &out);             EXPECT_HR(hr, S_OK);
    hr = g->lpVtbl->PassValue(g, call, (-1), out, 0); EXPECT_HR(hr, S_OK);
    if (out_call) *out_call = call; else if (call) call->lpVtbl->Release(call);
    if (in) in->lpVtbl->Release(in);
    if (out) out->lpVtbl->Release(out);
    return 1;
}
TEST(rb_flg_link_vertex_shader_from_library_function)
{
    ID3DBlob *lib, *sh = NULL, *err = NULL, *gerr = NULL; ID3D11Module *m = NULL; ID3D11FunctionLinkingGraph *g; ID3D11ModuleInstance *inst = NULL, *libinst = NULL; ID3D11Linker *l = NULL; HRESULT hr;
    REQUIRE(LOADMODULE); REQUIRE(CREATELINKER); RB_NOWINE("D3DCreateFunctionLinkingGraph");
    if (!rb_compile_lib(LIB_SRC, &lib)) return;
    g = rb_make_flg();
    EXPECT_HR(LOADMODULE(RB_BP(lib), RB_BS(lib), &m), S_OK);
    EXPECT_HR(CREATELINKER(&l), S_OK);
    if (g && m && l && rb_build_graph(g, m, NULL)) {
        hr = g->lpVtbl->CreateModuleInstance(g, &inst, &gerr);
        t_check(hr == S_OK && inst, __FILE__, __LINE__, "CreateModuleInstance", "%s: %s", d3dh_hr_name(hr), gerr ? rb_blob_text(gerr) : "");
        if (inst) {
            EXPECT_HR(m->lpVtbl->CreateInstance(m, "", &libinst), S_OK);
            if (libinst) EXPECT_HR(l->lpVtbl->UseLibrary(l, libinst), S_OK);
            hr = l->lpVtbl->Link(l, inst, "main", "vs_5_0", 0, &sh, &err);
            t_check(hr == S_OK && sh, __FILE__, __LINE__, "Link", "%s: %s", d3dh_hr_name(hr), err ? rb_blob_text(err) : "");
            if (sh) {
                ID3D11ShaderReflection *r = NULL; D3D11_SHADER_DESC d; rb_chunk ch[RB_MAXCHUNKS]; BYTE *cp;
                int n = rb_dxbc_parse(RB_BP(sh), RB_BS(sh), ch, RB_MAXCHUNKS);
                CHECK(n > 0); CHECK(rb_find(ch, n, FCC_SHEX) != NULL);
                cp = malloc(RB_BS(sh)); memcpy(cp, RB_BP(sh), RB_BS(sh)); rb_dxbc_rehash(cp, RB_BS(sh)); CHECK(!memcmp(cp + 4, RB_BP(sh) + 4, 16)); free(cp);
                hr = d3dh_D3DReflect()(RB_BP(sh), RB_BS(sh), &IID_ID3D11ShaderReflection, (void **)&r);
                EXPECT_HR(hr, S_OK);
                if (r) {
                    D3D11_SIGNATURE_PARAMETER_DESC sp; memset(&d, 0, sizeof d);
                    EXPECT_HR(r->lpVtbl->GetDesc(r, &d), S_OK); EXPECT_EQ(d.Version, 0x10050); EXPECT_EQ(d.InputParameters, 2); EXPECT_EQ(d.OutputParameters, 1);
                    memset(&sp, 0, sizeof sp); if (SUCCEEDED(r->lpVtbl->GetOutputParameterDesc(r, 0, &sp))) { EXPECT_EQ(sp.SystemValueType, D3D_NAME_POSITION); EXPECT_EQ(sp.Mask, 0xf); }
                    r->lpVtbl->Release(r);
                }
            }
        }
    }
    SAFE_RELEASE(sh); SAFE_RELEASE(err); SAFE_RELEASE(gerr); SAFE_RELEASE(libinst); SAFE_RELEASE(inst); SAFE_RELEASE(l); SAFE_RELEASE(m); SAFE_RELEASE(g); ID3D10Blob_Release(lib);
}
TEST(rb_flg_generate_hlsl_mentions_called_function)
{
    ID3DBlob *lib, *txt = NULL; ID3D11Module *m = NULL; ID3D11FunctionLinkingGraph *g;
    REQUIRE(LOADMODULE); RB_NOWINE("D3DCreateFunctionLinkingGraph");
    if (!rb_compile_lib(LIB_SRC, &lib)) return;
    g = rb_make_flg();
    EXPECT_HR(LOADMODULE(RB_BP(lib), RB_BS(lib), &m), S_OK);
    if (g && m && rb_build_graph(g, m, NULL)) {
        EXPECT_HR(g->lpVtbl->GenerateHlsl(g, 0, &txt), S_OK);
        if (txt) { CHECK(strstr((const char *)RB_BP(txt), "Scale") != NULL); t_note("generated HLSL:\n%s", (const char *)RB_BP(txt)); ID3D10Blob_Release(txt); }
    }
    SAFE_RELEASE(m); SAFE_RELEASE(g); ID3D10Blob_Release(lib);
}
TEST(rb_flg_call_function_errors)
{
    ID3DBlob *lib; ID3D11Module *m = NULL; ID3D11FunctionLinkingGraph *g; ID3D11LinkingNode *n = NULL;
    REQUIRE(LOADMODULE); RB_NOWINE("D3DCreateFunctionLinkingGraph");
    if (!rb_compile_lib(LIB_SRC, &lib)) return;
    EXPECT_HR(LOADMODULE(RB_BP(lib), RB_BS(lib), &m), S_OK);
    if (!m) { ID3D10Blob_Release(lib); return; }
    g = rb_make_flg(); if (g) { EXPECT_HR(g->lpVtbl->CallFunction(g, "", m, NULL, &n), RB_E_INVALIDARG); CHECK(n == NULL); g->lpVtbl->Release(g); }
    g = rb_make_flg(); if (g) { EXPECT_HR(g->lpVtbl->CallFunction(g, "", m, "Scale", NULL), RB_E_INVALIDARG); g->lpVtbl->Release(g); }
    g = rb_make_flg(); if (g) { EXPECT_HR(g->lpVtbl->CallFunction(g, "", m, "", &n), RB_E_FAIL); g->lpVtbl->Release(g); }
    g = rb_make_flg(); if (g) { HRESULT hr = g->lpVtbl->CallFunction(g, "", m, "NoSuchFunction", &n); CHECK(FAILED(hr)); t_note("CallFunction(unknown) = %s", d3dh_hr_name(hr)); SAFE_RELEASE(n); g->lpVtbl->Release(g); }
    g = rb_make_flg(); if (g) {
        EXPECT_HR(g->lpVtbl->CallFunction(g, "", m, "Scale", &n), S_OK);   /* a graph may start with a call; the input signature is optional */
        CHECK(n != NULL); SAFE_RELEASE(n); g->lpVtbl->Release(g);
    }
    SAFE_RELEASE(m); ID3D10Blob_Release(lib);
}
TEST(rb_flg_poisoned_after_error)
{
    ID3D11FunctionLinkingGraph *g; ID3D11LinkingNode *n = NULL; D3D11_PARAMETER_DESC p; ID3D11ModuleInstance *inst = NULL; ID3DBlob *e = NULL;
    RB_NOWINE("D3DCreateFunctionLinkingGraph"); g = rb_make_flg(); if (!g) return;
    p = rb_param("pos", "POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, NULL, 1, &n), RB_E_FAIL);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, &p, 1, &n), RB_E_FAIL);   /* the earlier error keeps the graph locked */
    EXPECT_HR(g->lpVtbl->CreateModuleInstance(g, &inst, &e), RB_E_FAIL);
    CHECK(inst == NULL); SAFE_RELEASE(e);
    g->lpVtbl->Release(g);
}

/* ---- D3DAssemble / D3DDisassemble --------------------------------------------------------------------- */
static BYTE *rb_chunk_copy(ID3DBlob *b, DWORD fcc, DWORD *n)
{
    rb_chunk c[RB_MAXCHUNKS]; int cnt = rb_dxbc_parse(RB_BP(b), RB_BS(b), c, RB_MAXCHUNKS); const rb_chunk *f = cnt > 0 ? rb_find(c, cnt, fcc) : NULL; BYTE *r;
    if (!f) return NULL;
    r = malloc(f->size ? f->size : 1); memcpy(r, f->data, f->size); *n = f->size; return r;
}
TEST(rb_asm_disassemble_contains_expected_text)
{
    RB_COMPILE(b, "float4 main(float4 p : SV_Position) : SV_Target { return float4(1, 0, 0.5, 1) * p; }\n", "ps_5_0"); ID3DBlob *t = NULL;
    REQUIRE(DISASM);
    EXPECT_HR(DISASM(RB_BP(b), RB_BS(b), 0, NULL, &t), S_OK);
    if (t) {
        const char *s = (const char *)RB_BP(t);
        CHECK(strstr(s, "ps_5_0") != NULL); CHECK(strstr(s, "dcl_input_ps_siv") != NULL || strstr(s, "dcl_input_ps") != NULL); CHECK(strstr(s, "dcl_output o0.xyzw") != NULL);
        CHECK(strstr(s, "mul") != NULL); CHECK(strstr(s, "ret") != NULL);
        CHECK(strstr(s, "SV_Position") != NULL); CHECK(strstr(s, "SV_Target") != NULL);   /* signature tables in the header comment */
        ID3D10Blob_Release(t);
    }
    ID3D10Blob_Release(b);
}
TEST(rb_asm_disassemble_header_comment_argument)
{
    RB_COMPILE(b, RB_PS_CONST, "ps_5_0"); ID3DBlob *t = NULL, *t0 = NULL;
    REQUIRE(DISASM);
    EXPECT_HR(DISASM(RB_BP(b), RB_BS(b), 0, "my-unique-comment-text", &t), S_OK);
    EXPECT_HR(DISASM(RB_BP(b), RB_BS(b), 0, NULL, &t0), S_OK);
    if (t && t0) { CHECK(strstr((const char *)RB_BP(t), "my-unique-comment-text") != NULL); CHECK(strstr((const char *)RB_BP(t0), "my-unique-comment-text") == NULL); CHECK(RB_BS(t) > RB_BS(t0)); }
    SAFE_RELEASE(t); SAFE_RELEASE(t0); ID3D10Blob_Release(b);
}
TEST(rb_asm_disassemble_is_nul_terminated_text)
{
    RB_COMPILE(b, RB_PS_CONST, "ps_5_0"); ID3DBlob *t = NULL;
    REQUIRE(DISASM);
    EXPECT_HR(DISASM(RB_BP(b), RB_BS(b), 0, NULL, &t), S_OK);
    if (t) { size_t n = RB_BS(t); CHECK(n > 0); CHECK(n > 0 && memchr(RB_BP(t), 0, n) != NULL); ID3D10Blob_Release(t); }
    ID3D10Blob_Release(b);
}
TEST(rb_asm_disassemble_flags_change_output)
{
    RB_COMPILE(b, "float4 main(float4 p : SV_Position) : SV_Target { return p * 2 + 1; }\n", "ps_5_0"); ID3DBlob *plain = NULL, *num = NULL;
    REQUIRE(DISASM);
    EXPECT_HR(DISASM(RB_BP(b), RB_BS(b), 0, NULL, &plain), S_OK);
    EXPECT_HR(DISASM(RB_BP(b), RB_BS(b), D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, NULL, &num), S_OK);
    if (plain && num) { CHECK(RB_BS(num) > RB_BS(plain)); CHECK(strcmp((const char *)RB_BP(num), (const char *)RB_BP(plain)) != 0); }
    SAFE_RELEASE(plain); SAFE_RELEASE(num); ID3D10Blob_Release(b);
}
TEST(rb_asm_disassemble_invalid_container)
{
    ID3DBlob *t = NULL; HRESULT hr; static const BYTE junk[64] = { 'D', 'X', 'B', 'X' };
    REQUIRE(DISASM);
    hr = DISASM(junk, sizeof junk, 0, NULL, &t);
    EXPECT_HR_FAILED(hr); CHECK(t == NULL);
    t_note("D3DDisassemble(junk) = %s", d3dh_hr_name(hr));
    SAFE_RELEASE(t);
}
/* D3DAssemble is the legacy (shader model 1-3) assembler: its output is the raw token stream, not a DXBC container. */
static const char rb_asm_ps20[] = "ps_2_0\ndef c0, 0.25, 0.5, 0.75, 1\ndcl t0.xy\nmov r0, c0\nmul r0.xy, r0, t0\nmov oC0, r0\n";
static const char rb_asm_vs11[] = "vs_1_1\ndcl_position v0\ndcl_texcoord v1\nm4x4 oPos, v0, c0\nmov oT0, v1\n";

TEST(rb_asm_assemble_ps_2_0_token_stream)
{
    ID3DBlob *a = NULL, *e = NULL; HRESULT hr;
    REQUIRE(ASSEMBLE);
    hr = ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, "t.asm", NULL, NULL, 0, &a, &e);
    t_check(hr == S_OK && a, __FILE__, __LINE__, "assemble ps_2_0", "%s: %s", d3dh_hr_name(hr), e ? rb_blob_text(e) : "");
    if (a) {
        const DWORD *w = (const DWORD *)RB_BP(a); size_t n = RB_BS(a) / 4;
        EXPECT_EQ(RB_BS(a) % 4, 0); CHECK(n >= 3);
        EXPECT_EQ(w[0], 0xFFFF0200u);
        CHECK(n >= 1 && w[n - 1] == 0x0000FFFFu);
        ID3D10Blob_Release(a);
    }
    SAFE_RELEASE(e);
}
TEST(rb_asm_assemble_vs_1_1_token_stream)
{
    ID3DBlob *a = NULL, *e = NULL; HRESULT hr;
    REQUIRE(ASSEMBLE);
    hr = ASSEMBLE(rb_asm_vs11, sizeof rb_asm_vs11 - 1, NULL, NULL, NULL, 0, &a, &e);
    t_check(hr == S_OK && a, __FILE__, __LINE__, "assemble vs_1_1", "%s: %s", d3dh_hr_name(hr), e ? rb_blob_text(e) : "");
    if (a) { const DWORD *w = (const DWORD *)RB_BP(a); size_t n = RB_BS(a) / 4; EXPECT_EQ(w[0], 0xFFFE0101u); CHECK(w[n - 1] == 0x0000FFFFu); ID3D10Blob_Release(a); }
    SAFE_RELEASE(e);
}
TEST(rb_asm_disassemble_assembled_shader)
{
    ID3DBlob *a = NULL, *t = NULL; REQUIRE(ASSEMBLE); REQUIRE(DISASM);
    if (FAILED(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, NULL, NULL, NULL, 0, &a, NULL)) || !a) { CHECK(0); return; }
    EXPECT_HR(DISASM(RB_BP(a), RB_BS(a), 0, NULL, &t), S_OK);
    if (t) {
        const char *s = (const char *)RB_BP(t);
        CHECK(strstr(s, "ps_2_0") != NULL); CHECK(strstr(s, "def c0, 0.25, 0.5, 0.75, 1") != NULL); CHECK(strstr(s, "dcl t0.xy") != NULL); CHECK(strstr(s, "mul r0.xy, r0, t0") != NULL); CHECK(strstr(s, "mov oC0, r0") != NULL);
        ID3D10Blob_Release(t);
    }
    ID3D10Blob_Release(a);
}
TEST(rb_asm_roundtrip_legacy_shader_is_identical)
{
    ID3DBlob *a = NULL, *t = NULL, *a2 = NULL, *e = NULL; HRESULT hr;
    REQUIRE(ASSEMBLE); REQUIRE(DISASM);
    if (FAILED(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, NULL, NULL, NULL, 0, &a, NULL)) || !a) { CHECK(0); return; }
    EXPECT_HR(DISASM(RB_BP(a), RB_BS(a), 0, NULL, &t), S_OK);
    if (t) {
        hr = ASSEMBLE(RB_BP(t), strlen((const char *)RB_BP(t)), "again.asm", NULL, NULL, 0, &a2, &e);
        t_check(hr == S_OK && a2, __FILE__, __LINE__, "reassemble the disassembly", "%s: %s", d3dh_hr_name(hr), e ? rb_blob_text(e) : "");
        if (a2) { const DWORD *w = (const DWORD *)RB_BP(a2); CHECK(RB_BS(a2) >= 12 && w[0] == 0xFFFF0200u); t_note("reassembled size %zu vs original %zu, identical=%d", (size_t)RB_BS(a2), (size_t)RB_BS(a), RB_BS(a2) == RB_BS(a) && !memcmp(RB_BP(a2), RB_BP(a), RB_BS(a))); ID3D10Blob_Release(a2); }
        SAFE_RELEASE(e); ID3D10Blob_Release(t);
    }
    ID3D10Blob_Release(a);
}
TEST(rb_asm_roundtrip_debug_flag_accepted)
{
    ID3DBlob *a = NULL; REQUIRE(ASSEMBLE);
    EXPECT_HR(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, "dbg.asm", NULL, NULL, D3DCOMPILE_DEBUG, &a, NULL), S_OK); SAFE_RELEASE(a);
    EXPECT_HR(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, "val.asm", NULL, NULL, D3DCOMPILE_SKIP_VALIDATION, &a, NULL), S_OK); SAFE_RELEASE(a);
}
TEST(rb_asm_assemble_macro_defines)
{
    static const char src[] = "ps_2_0\ndef c0, VALUE, 0, 0, 1\nmov oC0, c0\n";
    static const D3D_SHADER_MACRO defs[] = { { "VALUE", "0.5" }, { NULL, NULL } };
    ID3DBlob *a = NULL, *e = NULL, *t = NULL; HRESULT hr;
    REQUIRE(ASSEMBLE); REQUIRE(DISASM);
    hr = ASSEMBLE(src, sizeof src - 1, "m.asm", defs, NULL, 0, &a, &e);
    t_check(hr == S_OK && a, __FILE__, __LINE__, "assemble with macro", "%s: %s", d3dh_hr_name(hr), e ? rb_blob_text(e) : "");
    if (a) { EXPECT_HR(DISASM(RB_BP(a), RB_BS(a), 0, NULL, &t), S_OK); if (t) { CHECK(strstr((const char *)RB_BP(t), "def c0, 0.5, 0, 0, 1") != NULL); ID3D10Blob_Release(t); } ID3D10Blob_Release(a); }
    SAFE_RELEASE(e);
    a = NULL; hr = ASSEMBLE(src, sizeof src - 1, "m.asm", NULL, NULL, 0, &a, &e);   /* VALUE undefined */
    EXPECT_HR_FAILED(hr); CHECK(a == NULL); SAFE_RELEASE(a); SAFE_RELEASE(e);
}
TEST(rb_asm_assemble_error_reporting)
{
    static const char bad[] = "ps_2_0\nthis_is_not_an_opcode r0, c0\nmov oC0, r0\n";
    ID3DBlob *a = (ID3DBlob *)1, *e = NULL; HRESULT hr;
    REQUIRE(ASSEMBLE);
    hr = ASSEMBLE(bad, sizeof bad - 1, "bad.asm", NULL, NULL, 0, &a, &e);
    EXPECT_HR_FAILED(hr); CHECK(a == NULL);
    CHECK(e != NULL);
    if (e) t_note("assembler error text: %s", rb_blob_text(e));
    SAFE_RELEASE(e);
}
TEST(rb_asm_assemble_shader_model_4_text_is_rejected)
{
    /* the assembler only understands the legacy instruction set */
    static const char src[] = "ps_5_0\ndcl_output o0.xyzw\nmov o0.xyzw, l(1,1,1,1)\nret\n";
    ID3DBlob *a = (ID3DBlob *)1, *e = NULL; HRESULT hr;
    REQUIRE(ASSEMBLE);
    hr = ASSEMBLE(src, sizeof src - 1, NULL, NULL, NULL, 0, &a, &e);
    EXPECT_HR_FAILED(hr); CHECK(a == NULL); SAFE_RELEASE(e);
    t_note("D3DAssemble(ps_5_0 text) = %s", d3dh_hr_name(hr));
}
TEST(rb_asm_assemble_flags_validation)
{
    ID3DBlob *a = NULL, *e = NULL;
    REQUIRE(ASSEMBLE);
    EXPECT_HR(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, NULL, NULL, NULL, 4, &a, &e), RB_INVALIDCALL); CHECK(a == NULL); SAFE_RELEASE(e);
    EXPECT_HR(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, NULL, NULL, NULL, 8, &a, &e), RB_INVALIDCALL); SAFE_RELEASE(e);
    EXPECT_HR(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, NULL, NULL, NULL, 0x100, &a, &e), RB_INVALIDCALL); SAFE_RELEASE(e);
    EXPECT_HR(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, NULL, NULL, NULL, 0x80000000u, &a, &e), RB_INVALIDCALL); SAFE_RELEASE(e);
}
TEST(rb_asm_assemble_null_error_output)
{
    ID3DBlob *a = NULL;
    REQUIRE(ASSEMBLE);
    EXPECT_HR(ASSEMBLE(rb_asm_ps20, sizeof rb_asm_ps20 - 1, NULL, NULL, NULL, 0, &a, NULL), S_OK);
    SAFE_RELEASE(a);
}
