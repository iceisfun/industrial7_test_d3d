#include "dx.h"
/* d3dcompiler_43.dll (June 2010 redistributable compiler). Behaviour is compared with the in-box d3dcompiler_47.dll
   where both exist; only invariants both must satisfy are hard-checked. */

#define C43 "d3dcompiler_43.dll"
#define C47 "d3dcompiler_47.dll"
#define F43(name) UIN(C43, name)
#define F47(name) UIN(C47, name)

static const char SRC_VS[] = "cbuffer cb : register(b0) { float4 c; float4x4 m; };\n"
    "struct VSIn { float4 p : POSITION; float2 uv : TEXCOORD0; };\n"
    "struct VSOut { float4 p : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut main(VSIn i) { VSOut o; o.p = mul(i.p, m) + c; o.uv = i.uv; return o; }\n";
static const char SRC_PS[] = "Texture2D t : register(t0); SamplerState s : register(s0);\n"
    "float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { return t.Sample(s, uv); }\n";

static void squeeze(char *s) { char *d = s; for (; *s; s++) if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') *d++ = *s; *d = 0; }
static HRESULT comp(HRESULT (WINAPI *f)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **),
                    const char *src, const char *entry, const char *target, UINT f1, ID3DBlob **out, ID3DBlob **err) {
    *out = NULL; if (err) *err = NULL;
    ID3DBlob *e = NULL; HRESULT hr = f(src, strlen(src), "test.hlsl", NULL, NULL, entry, target, f1, 0, out, err ? err : &e);
    if (!err && e) ID3D10Blob_Release(e);
    return hr;
}
#define REL(b) do { if (b) { ID3D10Blob_Release(b); (b) = NULL; } } while (0)

TEST(d3dx_redist_optional_c43_presence_and_exports) {
    HMODULE m = LoadLibraryA(C43); t_note("%s %s", C43, m ? "present" : "absent");
    if (!m) { t_skip("d3dcompiler_43.dll absent"); return; }
    static const char *n[] = { "D3DCompile","D3DReflect","D3DDisassemble","D3DCreateBlob","D3DGetBlobPart","D3DStripShader","D3DPreprocess","D3DGetInputSignatureBlob",
        "D3DGetOutputSignatureBlob","D3DGetInputAndOutputSignatureBlob","D3DCompress","D3DCompressShaders","D3DDecompressShaders","D3DDisassemble10Effect","D3DGetDebugInfo","D3DCompile2","D3DReadFileToBlob","D3DSetBlobPart" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) t_note("%s %s", n[i], GetProcAddress(m, n[i]) ? "exported" : "missing");
    CHECK(GetProcAddress(m, "D3DCompile") != NULL); CHECK(GetProcAddress(m, "D3DReflect") != NULL); CHECK(GetProcAddress(m, "D3DDisassemble") != NULL); CHECK(GetProcAddress(m, "D3DCreateBlob") != NULL);
}
TEST(d3dx_redist_optional_c43_create_blob) {
    NEED_DLL(C43); REQUIRE(F43(D3DCreateBlob));
    static const SIZE_T sz[] = { 1, 100, 4096, 1 << 20 };
    for (unsigned i = 0; i < 4; i++) {
        ID3DBlob *b = NULL; EXPECT_HR(F43(D3DCreateBlob)(sz[i], &b), S_OK);
        if (b) {
            EXPECT_EQ(ID3D10Blob_GetBufferSize(b), sz[i]);
            BYTE *p = ID3D10Blob_GetBufferPointer(b); CHECK(p != NULL);
            if (p) { memset(p, 0x5A, sz[i]); CHECK(p[sz[i] - 1] == 0x5A); }
            IUnknown *u = NULL; EXPECT_HR(IUnknown_QueryInterface((IUnknown *)b, &IID_IUnknown, (void **)&u), S_OK); if (u) IUnknown_Release(u);
            CHECK((ULONG)ID3D10Blob_Release(b) == 0);
        }
    }
}
TEST(d3dx_redist_optional_c43_create_blob_null_out) {
    NEED_DLL(C43); REQUIRE(F43(D3DCreateBlob));
    HRESULT hr = F43(D3DCreateBlob)(16, NULL);
    CHECK(FAILED(hr)); t_note("D3DCreateBlob(16, NULL) -> %s (in-box 47: 0x8876086c)", d3dh_hr_name(hr));
}
TEST(d3dx_redist_optional_c43_create_blob_zero_size) {
    NEED_DLL(C43); REQUIRE(F43(D3DCreateBlob));
    ID3DBlob *b = NULL; HRESULT hr = F43(D3DCreateBlob)(0, &b); t_note("D3DCreateBlob(0) -> %s size=%lu", d3dh_hr_name(hr), b ? (unsigned long)ID3D10Blob_GetBufferSize(b) : 0UL);
    REL(b);
}
TEST(d3dx_redist_optional_c43_blob_refcount) {
    NEED_DLL(C43); REQUIRE(F43(D3DCreateBlob));
    ID3DBlob *b = NULL; if (FAILED(F43(D3DCreateBlob)(8, &b)) || !b) { t_skip("no blob"); return; }
    ULONG r;
    r = ID3D10Blob_AddRef(b); EXPECT_EQ(r, 2); r = ID3D10Blob_AddRef(b); EXPECT_EQ(r, 3);
    r = ID3D10Blob_Release(b); EXPECT_EQ(r, 2); r = ID3D10Blob_Release(b); EXPECT_EQ(r, 1); r = ID3D10Blob_Release(b); EXPECT_EQ(r, 0);
}

typedef HRESULT (WINAPI *CompileFn)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);
static void check_dxbc(ID3DBlob *b, const char *what) {
    const DWORD *p = ID3D10Blob_GetBufferPointer(b); SIZE_T n = ID3D10Blob_GetBufferSize(b);
    CHECKF(n >= 32, "%s too small: %lu", what, (unsigned long)n);
    if (n >= 32) { CHECKF(p[0] == MAKEFOURCC('D','X','B','C'), "%s magic 0x%08lx", what, p[0]); CHECKF(p[6] == n, "%s total size field %lu vs %lu", what, p[6], (unsigned long)n); CHECKF(p[7] >= 1, "%s chunk count", what); }
}
TEST(d3dx_redist_optional_c43_compile_vs40) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    ID3DBlob *b, *e; HRESULT hr = comp(f, SRC_VS, "main", "vs_4_0", 0, &b, &e);
    EXPECT_HR(hr, S_OK); if (b) { check_dxbc(b, "vs_4_0"); REL(b); } REL(e);
}
TEST(d3dx_redist_optional_c43_compile_ps40) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    ID3DBlob *b, *e; HRESULT hr = comp(f, SRC_PS, "main", "ps_4_0", 0, &b, &e);
    EXPECT_HR(hr, S_OK); if (b) { check_dxbc(b, "ps_4_0"); REL(b); } REL(e);
}
TEST(d3dx_redist_optional_c43_compile_profiles) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const struct { const char *src, *target; int hard; } t[] = {
        { SRC_VS, "vs_4_0", 1 }, { SRC_VS, "vs_4_1", 1 }, { SRC_PS, "ps_4_0", 1 }, { SRC_PS, "ps_4_1", 1 }, { SRC_VS, "vs_4_0_level_9_1", 1 }, { SRC_PS, "ps_4_0_level_9_3", 1 },
        { "float4 main(float4 p : POSITION) : POSITION { return p; }", "vs_2_0", 1 }, { "float4 main(float4 p : POSITION) : POSITION { return p; }", "vs_3_0", 0 },
        { "float4 main() : COLOR { return 1; }", "ps_2_0", 1 }, { "float4 main() : COLOR { return 1; }", "ps_3_0", 1 }, { SRC_VS, "vs_5_0", 0 }, { SRC_PS, "ps_5_0", 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ID3DBlob *b, *e; HRESULT hr = comp(f, t[i].src, "main", t[i].target, 0, &b, &e);
        if (t[i].hard) CHECKF(SUCCEEDED(hr), "%s -> %s", t[i].target, d3dh_hr_name(hr)); else t_note("%s -> %s", t[i].target, d3dh_hr_name(hr));
        REL(b); REL(e);
    }
}
TEST(d3dx_redist_optional_c43_compile_matches_47_success) {
    NEED_DLL(C43); NEED_DLL(C47); CompileFn a = (CompileFn)F43(D3DCompile), b47 = (CompileFn)F47(D3DCompile); REQUIRE(a); REQUIRE(b47);
    static const char *targets[] = { "vs_4_0", "vs_4_1" };
    for (int i = 0; i < 2; i++) {
        ID3DBlob *x, *y, *e1, *e2; HRESULT h1 = comp(a, SRC_VS, "main", targets[i], 0, &x, &e1), h2 = comp(b47, SRC_VS, "main", targets[i], 0, &y, &e2);
        CHECKF(SUCCEEDED(h1) == SUCCEEDED(h2), "%s: 43 %s 47 %s", targets[i], d3dh_hr_name(h1), d3dh_hr_name(h2));
        if (x && y) t_note("%s blob sizes 43=%lu 47=%lu", targets[i], (unsigned long)ID3D10Blob_GetBufferSize(x), (unsigned long)ID3D10Blob_GetBufferSize(y));
        REL(x); REL(y); REL(e1); REL(e2);
    }
}
TEST(d3dx_redist_optional_c43_compile_matches_47_failure) {
    NEED_DLL(C43); NEED_DLL(C47); CompileFn a = (CompileFn)F43(D3DCompile), b47 = (CompileFn)F47(D3DCompile); REQUIRE(a); REQUIRE(b47);
    static const char *bad[] = { "float4 main( : SV_Position { return 1 }", "float4 main() : SV_Target { return undefined_symbol; }", "float4 main() : SV_Target { return float3(1,2,3); }", "void main() { int x = 1/0; }" };
    for (unsigned i = 0; i < 4; i++) {
        ID3DBlob *x, *y, *e1, *e2; HRESULT h1 = comp(a, bad[i], "main", "ps_4_0", 0, &x, &e1), h2 = comp(b47, bad[i], "main", "ps_4_0", 0, &y, &e2);
        CHECKF(SUCCEEDED(h1) == SUCCEEDED(h2), "case %u: 43 %s 47 %s", i, d3dh_hr_name(h1), d3dh_hr_name(h2));
        if (FAILED(h1)) { CHECKF(x == NULL, "case %u produced code", i); CHECKF(e1 != NULL, "case %u has no error blob", i); }
        if (FAILED(h1) && FAILED(h2)) t_note("case %u hr 43=%s 47=%s", i, d3dh_hr_name(h1), d3dh_hr_name(h2));
        REL(x); REL(y); REL(e1); REL(e2);
    }
}
TEST(d3dx_redist_optional_c43_compile_error_message) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const char bad[] = "float4 main() : SV_Target\n{\n  return nosuchvar;\n}\n";
    ID3DBlob *b, *e; HRESULT hr = comp(f, bad, "main", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr)); CHECK(b == NULL); CHECK(e != NULL);
    if (e) { const char *m = ID3D10Blob_GetBufferPointer(e); CHECK(strstr(m, "test.hlsl") != NULL); CHECK(strstr(m, "nosuchvar") != NULL); if (!t_is_wine()) CHECK(strstr(m, "(3,") != NULL); t_note("message: %.120s", m); }
    REL(b); REL(e);
}
TEST(d3dx_redist_optional_c43_compile_missing_entry) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    ID3DBlob *b, *e; HRESULT hr = comp(f, SRC_PS, "no_such_entry", "ps_4_0", 0, &b, &e);
    CHECK(FAILED(hr)); CHECK(b == NULL); REL(b); REL(e);
}
TEST(d3dx_redist_optional_c43_compile_bad_target) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const char *t[] = { "xs_4_0", "vs_9_9", "vs", "", "ps_4", "ps_4_0_x" };
    for (unsigned i = 0; i < 6; i++) { ID3DBlob *b, *e; HRESULT hr = comp(f, SRC_PS, "main", t[i], 0, &b, &e); CHECKF(FAILED(hr), "target \"%s\" -> %s", t[i], d3dh_hr_name(hr)); REL(b); REL(e); }
}
TEST(d3dx_redist_optional_c43_compile_null_source) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = f(NULL, 0, NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &b, &e);
    CHECK(FAILED(hr)); t_note("D3DCompile(NULL, 0) -> %s", d3dh_hr_name(hr)); REL(b); REL(e);
}
TEST(d3dx_redist_optional_c43_compile_debug_adds_chunk) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    ID3DBlob *a, *b, *e; comp(f, SRC_VS, "main", "vs_4_0", 0, &a, &e); REL(e); comp(f, SRC_VS, "main", "vs_4_0", D3DCOMPILE_DEBUG, &b, &e); REL(e);
    if (a && b) {
        if (t_is_wine()) t_note("debug blob %lu vs plain %lu", (unsigned long)ID3D10Blob_GetBufferSize(b), (unsigned long)ID3D10Blob_GetBufferSize(a));
        else CHECKF(ID3D10Blob_GetBufferSize(b) > ID3D10Blob_GetBufferSize(a), "debug blob %lu not larger than %lu", (unsigned long)ID3D10Blob_GetBufferSize(b), (unsigned long)ID3D10Blob_GetBufferSize(a));
    } else CHECK(0);
    REL(a); REL(b);
}
TEST(d3dx_redist_optional_c43_strip_shader) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f); REQUIRE(F43(D3DStripShader));
    ID3DBlob *dbg, *e, *st = NULL; comp(f, SRC_VS, "main", "vs_4_0", D3DCOMPILE_DEBUG, &dbg, &e); REL(e);
    if (!dbg) { t_skip("compile failed"); return; }
    HRESULT hr = F43(D3DStripShader)(ID3D10Blob_GetBufferPointer(dbg), ID3D10Blob_GetBufferSize(dbg), D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_REFLECTION_DATA, &st);
    EXPECT_HR(hr, S_OK);
    if (st) { check_dxbc(st, "stripped"); CHECK(ID3D10Blob_GetBufferSize(st) < ID3D10Blob_GetBufferSize(dbg)); REL(st); }
    REL(dbg);
}
TEST(d3dx_redist_optional_c43_strip_shader_garbage) {
    NEED_DLL(C43); REQUIRE(F43(D3DStripShader));
    BYTE junk[64] = { 1, 2, 3 }; ID3DBlob *o = NULL;
    HRESULT hr = F43(D3DStripShader)(junk, sizeof junk, D3DCOMPILER_STRIP_DEBUG_INFO, &o);
    CHECK(FAILED(hr)); CHECK(o == NULL); REL(o);
}
TEST(d3dx_redist_optional_c43_get_blob_part_signatures) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f); REQUIRE(F43(D3DGetBlobPart));
    ID3DBlob *b, *e; comp(f, SRC_VS, "main", "vs_4_0", 0, &b, &e); REL(e);
    if (!b) { t_skip("compile failed"); return; }
    static const D3D_BLOB_PART parts[] = { D3D_BLOB_INPUT_SIGNATURE_BLOB, D3D_BLOB_OUTPUT_SIGNATURE_BLOB, D3D_BLOB_INPUT_AND_OUTPUT_SIGNATURE_BLOB };
    for (int i = 0; i < 3; i++) {
        ID3DBlob *part = NULL; HRESULT hr = F43(D3DGetBlobPart)(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), parts[i], 0, &part);
        CHECKF(SUCCEEDED(hr) && part, "part %d hr %s", parts[i], d3dh_hr_name(hr));
        if (part) { CHECK(ID3D10Blob_GetBufferSize(part) > 0); REL(part); }
    }
    ID3DBlob *part = NULL;
    HRESULT hr = F43(D3DGetBlobPart)(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), D3D_BLOB_INPUT_SIGNATURE_BLOB, 1, &part);
    CHECK(FAILED(hr)); t_note("nonzero flags -> %s (47: 0x8876086c)", d3dh_hr_name(hr)); REL(part);
    REL(b);
}
TEST(d3dx_redist_optional_c43_input_output_signature_blobs) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f); REQUIRE(F43(D3DGetInputSignatureBlob)); REQUIRE(F43(D3DGetOutputSignatureBlob));
    ID3DBlob *b, *e; comp(f, SRC_VS, "main", "vs_4_0", 0, &b, &e); REL(e);
    if (!b) { t_skip("compile failed"); return; }
    ID3DBlob *in = NULL, *out = NULL;
    EXPECT_HR(F43(D3DGetInputSignatureBlob)(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &in), S_OK);
    EXPECT_HR(F43(D3DGetOutputSignatureBlob)(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &out), S_OK);
    if (in) { CHECK(ID3D10Blob_GetBufferSize(in) >= 8); t_note("input signature blob: %lu bytes, first dword 0x%08lx", (unsigned long)ID3D10Blob_GetBufferSize(in), *(const DWORD *)ID3D10Blob_GetBufferPointer(in)); REL(in); }
    if (out) { CHECK(ID3D10Blob_GetBufferSize(out) >= 8); REL(out); }
    REL(b);
}
TEST(d3dx_redist_optional_c43_signature_blob_matches_47) {
    NEED_DLL(C43); NEED_DLL(C47); CompileFn a = (CompileFn)F43(D3DCompile), b47 = (CompileFn)F47(D3DCompile); REQUIRE(a); REQUIRE(b47); REQUIRE(F43(D3DGetInputSignatureBlob)); REQUIRE(F47(D3DGetInputSignatureBlob));
    ID3DBlob *x, *y, *e; comp(a, SRC_VS, "main", "vs_4_0", 0, &x, &e); REL(e); comp(b47, SRC_VS, "main", "vs_4_0", 0, &y, &e); REL(e);
    if (x && y) {
        ID3DBlob *sx = NULL, *sy = NULL;
        F43(D3DGetInputSignatureBlob)(ID3D10Blob_GetBufferPointer(x), ID3D10Blob_GetBufferSize(x), &sx);
        F47(D3DGetInputSignatureBlob)(ID3D10Blob_GetBufferPointer(y), ID3D10Blob_GetBufferSize(y), &sy);
        if (sx && sy) {
            const DWORD *px = ID3D10Blob_GetBufferPointer(sx), *py = ID3D10Blob_GetBufferPointer(sy);
            EXPECT_EQ(px[0], py[0]);   /* same element count */
            t_note("input signature size 43=%lu 47=%lu", (unsigned long)ID3D10Blob_GetBufferSize(sx), (unsigned long)ID3D10Blob_GetBufferSize(sy));
        }
        REL(sx); REL(sy);
    }
    REL(x); REL(y);
}
TEST(d3dx_redist_optional_c43_disassemble) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f); REQUIRE(F43(D3DDisassemble));
    ID3DBlob *b, *e; comp(f, SRC_VS, "main", "vs_4_0", 0, &b, &e); REL(e);
    if (!b) { t_skip("compile failed"); return; }
    ID3DBlob *d = NULL; EXPECT_HR(F43(D3DDisassemble)(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), 0, NULL, &d), S_OK);
    if (d) {
        const char *t = ID3D10Blob_GetBufferPointer(d); CHECK(strstr(t, "vs_4_0") != NULL); if (!t_is_wine()) { CHECK(strstr(t, "dcl_input") != NULL); CHECK(strstr(t, "dcl_output_siv") != NULL); CHECK(strstr(t, "dp4") != NULL); CHECK(strstr(t, "ret") != NULL); }
        REL(d);
    }
    EXPECT_HR(F43(D3DDisassemble)(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), D3D_DISASM_ENABLE_COLOR_CODE, "; hello", &d), S_OK);
    if (d) { if (!t_is_wine()) CHECK(strstr((const char *)ID3D10Blob_GetBufferPointer(d), "; hello") != NULL); REL(d); }
    REL(b);
}
TEST(d3dx_redist_optional_c43_disassemble_garbage) {
    NEED_DLL(C43); REQUIRE(F43(D3DDisassemble));
    BYTE junk[64] = { 9, 9, 9 }; ID3DBlob *d = NULL;
    HRESULT hr = F43(D3DDisassemble)(junk, sizeof junk, 0, NULL, &d);
    EXPECT_HR(hr, E_FAIL);
    CHECK(d != NULL);   /* an empty 1-byte text blob is still returned alongside the failure (probed) */
    if (d) { EXPECT_EQ(ID3D10Blob_GetBufferSize(d), 1); CHECK(((char *)ID3D10Blob_GetBufferPointer(d))[0] == 0); }
    REL(d);
}
TEST(d3dx_redist_optional_c43_reflect) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f); REQUIRE(F43(D3DReflect));
    ID3DBlob *b, *e; comp(f, SRC_VS, "main", "vs_4_0", 0, &b, &e); REL(e);
    if (!b) { t_skip("compile failed"); return; }
    ID3D11ShaderReflection *rf = NULL;
    HRESULT hr = F43(D3DReflect)(ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(b), &IID_ID3D11ShaderReflection, (void **)&rf);
    t_note("D3DReflect(IID_ID3D11ShaderReflection) on a 43 blob: %s", d3dh_hr_name(hr));
    if (rf) {
        D3D11_SHADER_DESC sd; EXPECT_HR(rf->lpVtbl->GetDesc(rf, &sd), S_OK);
        EXPECT_EQ(sd.ConstantBuffers, 1); EXPECT_EQ(sd.InputParameters, 2); EXPECT_EQ(sd.OutputParameters, 2); EXPECT_EQ(sd.BoundResources, 1);
        ID3D11ShaderReflectionConstantBuffer *cb = rf->lpVtbl->GetConstantBufferByIndex(rf, 0);
        D3D11_SHADER_BUFFER_DESC bd; if (cb && SUCCEEDED(cb->lpVtbl->GetDesc(cb, &bd))) { EXPECT_STR(bd.Name, "cb"); EXPECT_EQ(bd.Variables, 2); EXPECT_EQ(bd.Size, 80); }
        D3D11_SIGNATURE_PARAMETER_DESC pd;
        if (SUCCEEDED(rf->lpVtbl->GetInputParameterDesc(rf, 0, &pd))) { EXPECT_STR(pd.SemanticName, "POSITION"); EXPECT_EQ(pd.Register, 0); EXPECT_EQ(pd.Mask, 15); }
        if (SUCCEEDED(rf->lpVtbl->GetOutputParameterDesc(rf, 0, &pd))) { EXPECT_STR(pd.SemanticName, "SV_Position"); EXPECT_EQ(pd.SystemValueType, D3D_NAME_POSITION); }
        rf->lpVtbl->Release(rf);
    }
    REL(b);
}
TEST(d3dx_redist_optional_c43_reflect_garbage) {
    NEED_DLL(C43); REQUIRE(F43(D3DReflect));
    BYTE junk[64] = { 4, 3, 2 }; void *o = NULL;
    HRESULT hr = F43(D3DReflect)(junk, sizeof junk, &IID_ID3D11ShaderReflection, &o); CHECK(FAILED(hr)); CHECK(o == NULL);
}
TEST(d3dx_redist_optional_c43_preprocess) {
    NEED_DLL(C43); REQUIRE(F43(D3DPreprocess));
    static const char src[] = "#define A 3\n#define B(x) ((x)*2)\nfloat x = A; float y = B(5);\n#ifdef Q\nint q;\n#endif\n";
    ID3DBlob *o = NULL, *e = NULL;
    HRESULT hr = F43(D3DPreprocess)(src, sizeof src - 1, "pp.hlsl", NULL, NULL, &o, &e);
    EXPECT_HR(hr, S_OK);
    if (o) {
        char *t = ID3D10Blob_GetBufferPointer(o); SIZE_T n = ID3D10Blob_GetBufferSize(o);
        char *cp = malloc(n + 1); memcpy(cp, t, n); cp[n] = 0;
        t_note("preprocessed: %.120s", cp);
        squeeze(cp);
        if (!t_is_wine()) { CHECK(strstr(cp, "floatx=3;") != NULL); CHECK(strstr(cp, "((5)*2)") != NULL); }
        CHECK(strstr(cp, "intq") == NULL);
        free(cp);
    }
    REL(o); REL(e);
}
TEST(d3dx_redist_optional_c43_preprocess_macros) {
    NEED_DLL(C43); REQUIRE(F43(D3DPreprocess));
    static const char src[] = "float v = VALUE;\n#ifdef Q\nint q;\n#endif\n";
    D3D_SHADER_MACRO m[] = { { "VALUE", "42" }, { "Q", "1" }, { NULL, NULL } };
    ID3DBlob *o = NULL, *e = NULL;
    EXPECT_HR(F43(D3DPreprocess)(src, sizeof src - 1, NULL, m, NULL, &o, &e), S_OK);
    if (o) { char *cp = calloc(ID3D10Blob_GetBufferSize(o) + 1, 1); memcpy(cp, ID3D10Blob_GetBufferPointer(o), ID3D10Blob_GetBufferSize(o)); t_note("preprocessed: %.80s", cp); squeeze(cp); if (!t_is_wine()) { CHECK(strstr(cp, "floatv=42;") != NULL); CHECK(strstr(cp, "intq;") != NULL); } free(cp); }
    REL(o); REL(e);
}
TEST(d3dx_redist_optional_c43_compile_with_macros) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const char src[] = "float4 main() : SV_Target { return float4(K, K, K, 1); }";
    D3D_SHADER_MACRO m[] = { { "K", "0.5" }, { NULL, NULL } };
    ID3DBlob *b = NULL, *e = NULL;
    EXPECT_HR(f(src, sizeof src - 1, "m.hlsl", m, NULL, "main", "ps_4_0", 0, 0, &b, &e), S_OK); REL(b); REL(e);
    HRESULT hr = f(src, sizeof src - 1, "m.hlsl", NULL, NULL, "main", "ps_4_0", 0, 0, &b, &e); CHECK(FAILED(hr)); REL(b); REL(e);
}

/* include handler */
typedef struct { ID3DInclude base; int opens, closes; D3D_INCLUDE_TYPE last_type; char last_name[64]; } INC;
static HRESULT WINAPI inc_open(ID3DInclude *This, D3D_INCLUDE_TYPE ty, LPCSTR name, LPCVOID parent, LPCVOID *data, UINT *bytes) {
    INC *i = (INC *)This; static const char body[] = "#define FROM_INC 7\n";
    i->opens++; i->last_type = ty; lstrcpynA(i->last_name, name, sizeof i->last_name);
    *data = body; *bytes = sizeof body - 1; return S_OK;
}
static HRESULT WINAPI inc_close(ID3DInclude *This, LPCVOID data) { ((INC *)This)->closes++; return S_OK; }
static const ID3DIncludeVtbl inc_vtbl = { inc_open, inc_close };
TEST(d3dx_redist_optional_c43_include_handler) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const char src[] = "#include \"my.h\"\nfloat4 main() : SV_Target { return FROM_INC; }\n";
    INC inc; memset(&inc, 0, sizeof inc); inc.base.lpVtbl = (ID3DIncludeVtbl *)&inc_vtbl;
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = f(src, sizeof src - 1, "i.hlsl", NULL, &inc.base, "main", "ps_4_0", 0, 0, &b, &e);
    EXPECT_HR(hr, S_OK); EXPECT_EQ(inc.opens, 1); EXPECT_EQ(inc.closes, 1); EXPECT_EQ(inc.last_type, D3D_INCLUDE_LOCAL); EXPECT_STR(inc.last_name, "my.h");
    REL(b); REL(e);
    memset(&inc, 0, sizeof inc); inc.base.lpVtbl = (ID3DIncludeVtbl *)&inc_vtbl;
    static const char src2[] = "#include <sys.h>\nfloat4 main() : SV_Target { return FROM_INC; }\n";
    hr = f(src2, sizeof src2 - 1, "i.hlsl", NULL, &inc.base, "main", "ps_4_0", 0, 0, &b, &e);
    EXPECT_HR(hr, S_OK); EXPECT_EQ(inc.last_type, D3D_INCLUDE_SYSTEM); EXPECT_STR(inc.last_name, "sys.h");
    REL(b); REL(e);
}
TEST(d3dx_redist_optional_c43_include_without_handler_fails) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const char src[] = "#include \"nonexistent_file_xyz.h\"\nfloat4 main() : SV_Target { return 0; }\n";
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = f(src, sizeof src - 1, "i.hlsl", NULL, NULL, "main", "ps_4_0", 0, 0, &b, &e);
    CHECK(FAILED(hr)); t_note("include with no handler -> %s", d3dh_hr_name(hr)); REL(b); REL(e);
}
TEST(d3dx_redist_optional_c43_compile_effect_target) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const char fx[] = "technique10 T { pass P { } }";
    ID3DBlob *b = NULL, *e = NULL;
    HRESULT hr = f(fx, sizeof fx - 1, "e.fx", NULL, NULL, NULL, "fx_4_0", 0, 0, &b, &e);
    t_note("fx_4_0 compile -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && b) { CHECK(ID3D10Blob_GetBufferSize(b) > 16); t_note("effect blob magic 0x%08lx", *(const DWORD *)ID3D10Blob_GetBufferPointer(b)); }
    REL(b); REL(e);
}
TEST(d3dx_redist_optional_c43_compile_deterministic) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    ID3DBlob *a, *b, *e; comp(f, SRC_VS, "main", "vs_4_0", 0, &a, &e); REL(e); comp(f, SRC_VS, "main", "vs_4_0", 0, &b, &e); REL(e);
    if (a && b) { CHECK(ID3D10Blob_GetBufferSize(a) == ID3D10Blob_GetBufferSize(b)); CHECK(!memcmp(ID3D10Blob_GetBufferPointer(a), ID3D10Blob_GetBufferPointer(b), ID3D10Blob_GetBufferSize(a))); }
    REL(a); REL(b);
}
TEST(d3dx_redist_optional_c43_optimization_flags) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const char src[] = "float4 c; float4 main() : SV_Target { float4 r = 0; for (int i = 0; i < 4; i++) r += c * i; return r; }";
    static const UINT fl[] = { D3DCOMPILE_OPTIMIZATION_LEVEL0, D3DCOMPILE_OPTIMIZATION_LEVEL1, D3DCOMPILE_OPTIMIZATION_LEVEL2, D3DCOMPILE_OPTIMIZATION_LEVEL3, D3DCOMPILE_SKIP_OPTIMIZATION, D3DCOMPILE_SKIP_VALIDATION, D3DCOMPILE_ENABLE_STRICTNESS, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, D3DCOMPILE_AVOID_FLOW_CONTROL };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) { ID3DBlob *b = NULL, *e = NULL; HRESULT hr = f(src, sizeof src - 1, "o.hlsl", NULL, NULL, "main", "ps_4_0", fl[i], 0, &b, &e); CHECKF(SUCCEEDED(hr), "flags 0x%x -> %s", fl[i], d3dh_hr_name(hr)); REL(b); REL(e); }
}
TEST(d3dx_redist_optional_c43_unoptimized_not_smaller) {
    NEED_DLL(C43); CompileFn f = (CompileFn)F43(D3DCompile); REQUIRE(f);
    static const char src[] = "float4 c; float4 main() : SV_Target { float4 r = 0; for (int i = 0; i < 4; i++) r += c * i; return r; }";
    ID3DBlob *o0 = NULL, *o3 = NULL, *e = NULL;
    f(src, sizeof src - 1, "o.hlsl", NULL, NULL, "main", "ps_4_0", D3DCOMPILE_SKIP_OPTIMIZATION, 0, &o0, &e); REL(e);
    f(src, sizeof src - 1, "o.hlsl", NULL, NULL, "main", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &o3, &e); REL(e);
    if (o0 && o3) CHECKF(ID3D10Blob_GetBufferSize(o0) >= ID3D10Blob_GetBufferSize(o3), "unoptimised %lu smaller than optimised %lu", (unsigned long)ID3D10Blob_GetBufferSize(o0), (unsigned long)ID3D10Blob_GetBufferSize(o3));
    REL(o0); REL(o3);
}
