/* ID3D11FunctionLinkingGraph: value passing, swizzles, error reporting and NULL-argument faults (d3dcompiler_47). */
#include "gap0.h"
#include <d3d11shader.h>

typedef HRESULT (WINAPI *flg_t)(UINT, ID3D11FunctionLinkingGraph **);
static ID3D11FunctionLinkingGraph *G;
static ID3D11LinkingNode *NIN, *NOUT, *NCALL;
static HRESULT g_hr;

static const char HLSL_EMPTY[] = "\nvoid main(in float4 inputPos : POSITION, out float4 outputPos : SV_POSITION)\n{\n}\n";
#define HLSL_BODY(stmt) "\nvoid main(in float4 inputPos : POSITION, out float4 outputPos : SV_POSITION)\n{\n    " stmt "\n}\n"
#define ERRP(code, msg) "error " code ": ID3D11FunctionLinkingGraph::" msg "\n"

static void do_op(void *c)
{
    switch ((int)(ULONG_PTR)c) {
    case 0:  g_hr = G->lpVtbl->PassValue(G, NIN, 0, NOUT, 0); break;
    case 1:  g_hr = G->lpVtbl->PassValue(G, NIN, 5, NOUT, 0); break;
    case 2:  g_hr = G->lpVtbl->PassValue(G, NIN, -1, NOUT, 0); break;
    case 3:  g_hr = G->lpVtbl->PassValue(G, NULL, 0, NOUT, 0); break;
    case 4:  g_hr = G->lpVtbl->PassValue(G, NIN, 0, NULL, 0); break;
    case 5:  g_hr = G->lpVtbl->PassValue(G, NOUT, 0, NIN, 0); break;
    case 6:  g_hr = G->lpVtbl->PassValue(G, NIN, 0, NIN, 0); break;
    case 7:  g_hr = G->lpVtbl->PassValueWithSwizzle(G, NIN, 0, "xy", NOUT, 0, "zw"); break;
    case 8:  g_hr = G->lpVtbl->PassValueWithSwizzle(G, NIN, 0, "q", NOUT, 0, "x"); break;
    case 9:  g_hr = G->lpVtbl->PassValueWithSwizzle(G, NIN, 0, NULL, NOUT, 0, NULL); break;
    case 10: g_hr = G->lpVtbl->PassValueWithSwizzle(G, NIN, 0, "xyzw", NOUT, 0, "wzyx"); break;
    case 11: g_hr = G->lpVtbl->PassValueWithSwizzle(G, NIN, 0, "xyzw", NOUT, 0, ""); break;
    case 12: g_hr = G->lpVtbl->CallFunction(G, "", NULL, "f", &NCALL); break;
    case 13: g_hr = G->lpVtbl->CallFunction(G, "", NULL, NULL, &NCALL); break;
    case 14: { ID3DBlob *e = NULL; g_hr = G->lpVtbl->CreateModuleInstance(G, NULL, &e); if (e) e->lpVtbl->Release(e); break; }
    }
}

typedef struct { int op, in_ty; HRESULT hr; DWORD exc; const char *err; const char *hlsl; } flg_case;
static const flg_case k_cases[] = {
    {  0, 0, S_OK,           0, NULL, HLSL_BODY("outputPos = inputPos;") },
    {  1, 0, E_FAIL_HR,      0, ERRP("X9008", "PassValueWithSwizzle: illegal source parameter index"), HLSL_EMPTY },
    {  2, 0, E_FAIL_HR,      0, ERRP("X9008", "PassValueWithSwizzle: illegal source parameter index"), HLSL_EMPTY },
    {  3, 0, E_INVALIDARG_HR, 0, ERRP("X9000", "PassValueWithSwizzle: NULL parameter"), HLSL_EMPTY },
    {  4, 0, E_INVALIDARG_HR, 0, ERRP("X9000", "PassValueWithSwizzle: NULL parameter"), HLSL_EMPTY },
    {  5, 0, E_FAIL_HR,      0, ERRP("X9010", "PassValueWithSwizzle: source node must preceed destination node in FLG"), HLSL_EMPTY },
    {  6, 0, E_FAIL_HR,      0, ERRP("X9010", "PassValueWithSwizzle: source node must preceed destination node in FLG"), HLSL_EMPTY },
    {  7, 0, S_OK,           0, NULL, HLSL_BODY("outputPos.zw = inputPos.xy;") },
    {  8, 0, E_FAIL_HR,      0, ERRP("X9028", "PassValueWithSwizzle: bad source swizzle 'q'"), HLSL_EMPTY },
    {  9, 0, S_OK,           0, NULL, HLSL_BODY("outputPos = inputPos;") },
    { 10, 0, S_OK,           0, NULL, HLSL_BODY("outputPos = inputPos.wzyx;") },
    { 11, 0, E_FAIL_HR,      0, ERRP("X9000", "PassValueWithSwizzle: swizzle string cannot be empty"), HLSL_EMPTY },
    { 12, 0, 0, 0xC0000005u, NULL, NULL },       /* CallFunction does not validate the module pointer */
    { 13, 0, 0, 0xC0000005u, NULL, NULL },
    { 14, 0, E_INVALIDARG_HR, 0, ERRP("X9000", "CreateModuleInstance: NULL parameter"), HLSL_EMPTY },
    {  0, 1, E_FAIL_HR,      0, ERRP("X9012", "PassValueWithSwizzle: incompatible element types"), "\\vint" },
    {  0, 2, E_FAIL_HR,      0, ERRP("X9012", "PassValueWithSwizzle: source value has too few components"), "\\vfloat3" },
};

static char *blob_text(ID3DBlob *b, SIZE_T *n)
{
    char *s; *n = d3dh_blob_size(b); s = malloc(*n + 1); memcpy(s, d3dh_blob_ptr(b), *n); s[*n] = 0; return s;
}

TEST(gap0_flg_pass_value_matrix)
{
    flg_t fn = t_is_wine() ? NULL : (flg_t)t_proc_in("d3dcompiler_47.dll", "D3DCreateFunctionLinkingGraph");
    D3D11_PARAMETER_DESC p4, p3, pi, po; size_t k;
    REQUIRE(fn);
    memset(&p4, 0, sizeof p4); p4.Name = "inputPos"; p4.SemanticName = "POSITION"; p4.Type = D3D_SVT_FLOAT; p4.Class = D3D_SVC_VECTOR; p4.Rows = 1; p4.Columns = 4;
    p3 = p4; p3.Name = "col"; p3.SemanticName = "COLOR"; p3.Columns = 3;
    pi = p4; pi.Name = "idx"; pi.SemanticName = "TEXCOORD"; pi.Type = D3D_SVT_INT; pi.Columns = 1;
    po = p4; po.Name = "outputPos"; po.SemanticName = "SV_POSITION";
    for (k = 0; k < sizeof k_cases / sizeof *k_cases; k++) {
        const flg_case *c = &k_cases[k]; ID3DBlob *h = NULL, *e = NULL; DWORD x; HRESULT hr; SIZE_T n; char *s;
        G = NULL; NIN = NOUT = NCALL = NULL;
        EXPECT_HR(fn(0, &G), S_OK); if (!G) return;
        EXPECT_HR(G->lpVtbl->SetInputSignature(G, c->in_ty == 1 ? &pi : c->in_ty == 2 ? &p3 : &p4, 1, &NIN), S_OK);
        EXPECT_HR(G->lpVtbl->SetOutputSignature(G, &po, 1, &NOUT), S_OK);
        g_hr = (HRESULT)0x77777777;
        x = t_try(do_op, (void *)(ULONG_PTR)c->op);
        CHECKF(x == c->exc, "case %u (op %d in %d): exception 0x%lx want 0x%lx", (unsigned)k, c->op, c->in_ty, x, (unsigned long)c->exc);
        if (!c->exc) {
            CHECKF(g_hr == c->hr, "case %u (op %d in %d): %s want %s", (unsigned)k, c->op, c->in_ty, d3dh_hr_name(g_hr), d3dh_hr_name(c->hr));
            hr = G->lpVtbl->GetLastError(G, &e);
            EXPECT_HR(hr, S_OK);
            if (c->err) {
                CHECKF(e != NULL, "case %u: no error blob", (unsigned)k);
                if (e) { s = blob_text(e, &n); CHECKF(!strcmp(s, c->err), "case %u error text: %s", (unsigned)k, s); CHECKF(n == strlen(c->err) + 1, "case %u error blob size %u", (unsigned)k, (unsigned)n); free(s); }
            } else CHECKF(e == NULL, "case %u: unexpected error blob", (unsigned)k);
            hr = G->lpVtbl->GenerateHlsl(G, 0, &h);
            EXPECT_HR(hr, S_OK);                  /* a failed pass leaves the graph generating the signature only */
            if (h) {
                s = blob_text(h, &n);
                if (c->hlsl[0] == '\\' && c->hlsl[1] == 'v') {   /* signature varies with the input type */
                    CHECKF(strstr(s, c->hlsl + 2) != NULL && strstr(s, "{\n}\n") != NULL, "case %u hlsl: %s", (unsigned)k, s);
                } else CHECKF(n == strlen(c->hlsl) + 1 && !strcmp(s, c->hlsl), "case %u hlsl (%u bytes): %s", (unsigned)k, (unsigned)n, s);
                free(s);
            }
            SAFE_RELEASE(h); SAFE_RELEASE(e);
        }
        if (NIN) IUnknown_Release((IUnknown *)NIN);
        if (NOUT) IUnknown_Release((IUnknown *)NOUT);
        { ULONG rc = G->lpVtbl->Release(G); EXPECT_EQ(rc, 0); }
    }
}

TEST(gap0_flg_signature_rules)
{
    flg_t fn = t_is_wine() ? NULL : (flg_t)t_proc_in("d3dcompiler_47.dll", "D3DCreateFunctionLinkingGraph");
    D3D11_PARAMETER_DESC p; ID3D11FunctionLinkingGraph *g; ID3D11LinkingNode *n = NULL; ID3DBlob *e = NULL, *h = NULL; char *s; SIZE_T sz;
    REQUIRE(fn);
    memset(&p, 0, sizeof p); p.Name = "inputPos"; p.SemanticName = "POSITION"; p.Type = D3D_SVT_FLOAT; p.Class = D3D_SVC_VECTOR; p.Rows = 1; p.Columns = 4;
    /* an empty graph generates an empty entry point */
    EXPECT_HR(fn(0, &g), S_OK);
    EXPECT_HR(g->lpVtbl->GenerateHlsl(g, 0, &h), S_OK);
    if (h) { s = blob_text(h, &sz); CHECKF(sz == sizeof "void main()\n{\n}\n" && !strcmp(s, "void main()\n{\n}\n"), "empty graph hlsl: %s", s); free(s); SAFE_RELEASE(h); }
    EXPECT_HR(g->lpVtbl->GenerateHlsl(g, 1, &h), S_OK);    /* the flags are ignored */
    SAFE_RELEASE(h);
    EXPECT_HR(g->lpVtbl->GenerateHlsl(g, 0, NULL), E_INVALIDARG_HR);
    EXPECT_HR(g->lpVtbl->GetLastError(g, NULL), S_FALSE);   /* nothing recorded yet, NULL is harmless */
    /* a zero-parameter input signature is legal and may be given only once */
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, NULL, 0, &n), S_OK);
    SAFE_RELEASE(n);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, NULL, 0, &n), E_FAIL_HR);
    EXPECT_HR(g->lpVtbl->GetLastError(g, &e), S_OK);
    if (e) { s = blob_text(e, &sz); CHECKF(!strcmp(s, ERRP("X9001", "SetInputSignature: input signature has already been set")), "text: %s", s); free(s); SAFE_RELEASE(e); }
    g->lpVtbl->Release(g);
    EXPECT_HR(fn(0, &g), S_OK);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, &p, 0, &n), S_OK);
    SAFE_RELEASE(n);
    g->lpVtbl->Release(g);
    EXPECT_HR(fn(0, &g), S_OK);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, &p, 1, NULL), E_FAIL_HR);
    EXPECT_HR(g->lpVtbl->GetLastError(g, &e), S_OK);
    if (e) { s = blob_text(e, &sz); CHECKF(!strcmp(s, ERRP("X9000", "SetInputSignature: NULL parameter")), "text: %s", s); free(s); SAFE_RELEASE(e); }
    g->lpVtbl->Release(g);
    EXPECT_HR(fn(0, &g), S_OK);
    EXPECT_HR(g->lpVtbl->SetInputSignature(g, NULL, 1, &n), E_FAIL_HR);
    EXPECT_HR(g->lpVtbl->GetLastError(g, &e), S_OK);
    if (e) { s = blob_text(e, &sz); CHECKF(!strcmp(s, ERRP("X9000", "SetInputSignature: NULL parameter")), "text: %s", s); free(s); SAFE_RELEASE(e); }
    g->lpVtbl->Release(g);
    EXPECT_HR(fn(0, &g), S_OK);
    EXPECT_HR(g->lpVtbl->SetOutputSignature(g, &p, 1, &n), E_FAIL_HR);
    EXPECT_HR(g->lpVtbl->GetLastError(g, &e), S_OK);
    if (e) { s = blob_text(e, &sz); CHECKF(!strcmp(s, ERRP("X9004", "SetOutputSignature: output signature must be the last FLG node")), "text: %s", s); free(s); SAFE_RELEASE(e); }
    g->lpVtbl->Release(g);
}
