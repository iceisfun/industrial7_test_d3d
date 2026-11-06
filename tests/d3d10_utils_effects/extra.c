/* D3D10CompileShader argument rules and concurrency, measured on Win10 22H2. */
#include "ue.h"

typedef HRESULT (WINAPI *xcs_t)(const char *, SIZE_T, const char *, const D3D10_SHADER_MACRO *, ID3D10Include *, const char *, const char *, UINT, ID3D10Blob **, ID3D10Blob **);
#define XCS() ((xcs_t)t_proc_in("d3d10.dll", "D3D10CompileShader"))

typedef struct { const char *src; SIZE_T n; const char *name, *entry, *prof; HRESULT hr; ID3D10Blob *b, *e; } xc;
static void xc_call(void *p) { xc *a = p; a->hr = XCS()(a->src, a->n, a->name, NULL, NULL, a->entry, a->prof, 0, &a->b, &a->e); }
static xc xc_run(const char *src, SIZE_T n, const char *name, const char *entry, const char *prof, DWORD *code)
{
    xc a = { src, n, name, entry, prof, (HRESULT)0x12345678, NULL, NULL };
    *code = t_try(xc_call, &a);
    return a;
}
static int xc_err_has(xc *a, const char *s) { return a->e && strstr((const char *)ID3D10Blob_GetBufferPointer(a->e), s) != NULL; }

TEST(d3d10_utils_effects_xcompile_source_length_rules)
{
    REQUIRE(XCS()); DWORD c; xc a;
    SIZE_T len = strlen(UE_VS_SRC);
    a = xc_run(UE_VS_SRC, len, "x.hlsl", "main", "vs_4_0", &c);          EXPECT_EQ(c, 0); EXPECT_HR(a.hr, S_OK); CHECK(a.b != NULL); UE_REL(a.b); UE_REL(a.e);
    a = xc_run(UE_VS_SRC, len + 1, "x.hlsl", "main", "vs_4_0", &c);      EXPECT_HR(a.hr, S_OK); UE_REL(a.b); UE_REL(a.e);          /* NUL included */
    a = xc_run(UE_VS_SRC, (SIZE_T)-1, "x.hlsl", "main", "vs_4_0", &c);   EXPECT_EQ(c, 0); EXPECT_HR(a.hr, S_OK); UE_REL(a.b); UE_REL(a.e);   /* -1: NUL terminated string */
    a = xc_run(UE_VS_SRC, 0, "x.hlsl", "main", "vs_4_0", &c);            EXPECT_HR(a.hr, E_FAIL); CHECK(a.b == NULL); CHECK(xc_err_has(&a, "error X3501: 'main': entrypoint not found")); UE_REL(a.e);   /* 0 = empty source */
    a = xc_run("", 0, "x.hlsl", "main", "vs_4_0", &c);                   EXPECT_HR(a.hr, E_FAIL); CHECK(xc_err_has(&a, "error X3501: 'main': entrypoint not found")); UE_REL(a.e);
    a = xc_run(NULL, 0, "x.hlsl", "main", "vs_4_0", &c);                 EXPECT_EQ(c, 0); EXPECT_HR(a.hr, E_FAIL); CHECK(xc_err_has(&a, "entrypoint not found")); UE_REL(a.e);   /* NULL with length 0 is an empty source */
    a = xc_run(NULL, 10, "x.hlsl", "main", "vs_4_0", &c);                EXPECT_EQ(c, 0); EXPECT_HR(a.hr, E_D3DERR_INVALIDCALL); CHECK(a.b == NULL && a.e == NULL);
    a = xc_run(UE_VS_SRC, 40, "x.hlsl", "main", "vs_4_0", &c);           EXPECT_HR(a.hr, E_FAIL); CHECK(xc_err_has(&a, "x.hlsl(1): error X3000: syntax error: unexpected end of file")); UE_REL(a.e);   /* truncated mid token */
}

TEST(d3d10_utils_effects_xcompile_name_entry_profile_rules)
{
    REQUIRE(XCS()); DWORD c; xc a; SIZE_T len = strlen(UE_VS_SRC);
    a = xc_run(UE_VS_SRC, len, NULL, "main", "vs_4_0", &c);              EXPECT_EQ(c, 0); EXPECT_HR(a.hr, S_OK); UE_REL(a.b); UE_REL(a.e);          /* a NULL source name is allowed */
    a = xc_run(UE_VS_SRC, len, "x.hlsl", "", "vs_4_0", &c);              EXPECT_HR(a.hr, E_FAIL); CHECK(xc_err_has(&a, "error X3501: '': entrypoint not found")); UE_REL(a.e);
    a = xc_run(UE_VS_SRC, len, "x.hlsl", "MAIN", "vs_4_0", &c);          EXPECT_HR(a.hr, E_FAIL); CHECK(xc_err_has(&a, "error X3501: 'MAIN': entrypoint not found")); UE_REL(a.e);   /* case sensitive */
    static const char *const bad[] = { "", "VS_4_0", "vs_4_0 ", "vs4_0", "vs_4_0_level_9_1" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        char want[96]; sprintf(want, "error X3506: unrecognized compiler target '%s'", bad[i]);
        a = xc_run(UE_VS_SRC, len, "x.hlsl", "main", bad[i], &c);
        CHECKF(c == 0 && a.hr == E_D3DERR_INVALIDCALL && a.b == NULL, "profile '%s': %s", bad[i], d3dh_hr_name(a.hr));
        CHECKF(xc_err_has(&a, want), "profile '%s' error text", bad[i]);
        UE_REL(a.e);
    }
    a = xc_run(UE_VS_SRC, len, "x.hlsl", "main", NULL, &c);              EXPECT_HR(a.hr, E_D3DERR_INVALIDCALL); CHECK(xc_err_has(&a, "error X3506: unrecognized compiler target '(null)'")); UE_REL(a.e);
}

TEST(d3d10_utils_effects_xcompile_large_source_and_integer_divide_error)
{
    REQUIRE(XCS()); DWORD c; xc a; SIZE_T len = strlen(UE_VS_SRC), total = (SIZE_T)2 << 20;
    char *big = malloc(total + 1);
    memset(big, ' ', total); memcpy(big + total - len, UE_VS_SRC, len); big[total] = 0;
    a = xc_run(big, total, "big.hlsl", "main", "vs_4_0", &c);            EXPECT_EQ(c, 0); EXPECT_HR(a.hr, S_OK); CHECK(a.b != NULL); UE_REL(a.b); UE_REL(a.e);   /* 2 MiB of leading blanks */
    free(big);
    static const char div0[] = "float4 main() : SV_Position { return 1/0; }\n";
    a = xc_run(div0, sizeof div0 - 1, "x.hlsl", "main", "vs_4_0", &c);   EXPECT_HR(a.hr, E_FAIL); CHECK(xc_err_has(&a, "x.hlsl(1): error X4010: Unsigned integer divide by zero")); UE_REL(a.e);
}

/* four threads compile the same shaders concurrently: every compile succeeds and the code chunk is identical everywhere */
typedef struct { int idx; volatile LONG failed; unsigned char *shdr[2]; DWORD shdr_n[2]; } cthr;
static DWORD WINAPI cthr_main(LPVOID p)
{
    cthr *t = p; int it, k;
    for (it = 0; it < 12; it++) for (k = 0; k < 2; k++) {
        ID3D10Blob *b = NULL, *e = NULL;
        HRESULT hr = XCS()(k ? UE_PS_SRC : UE_VS_SRC, strlen(k ? UE_PS_SRC : UE_VS_SRC), "ue.hlsl", NULL, NULL, "main", k ? "ps_4_0" : "vs_4_0", 0, &b, &e);
        if (hr != S_OK || !b) { InterlockedIncrement(&t->failed); UE_REL(e); continue; }
        DWORD n = 0; const unsigned char *c = ue_dxbc_chunk(ue_bp(b), ue_bs(b), FOURCC('S','H','D','R'), &n);
        if (!c) InterlockedIncrement(&t->failed);
        else if (!t->shdr[k]) { t->shdr[k] = malloc(n); memcpy(t->shdr[k], c, n); t->shdr_n[k] = n; }
        else if (t->shdr_n[k] != n || memcmp(t->shdr[k], c, n)) InterlockedIncrement(&t->failed);
        UE_REL(b); UE_REL(e);
    }
    return 0;
}
TEST(d3d10_utils_effects_xcompile_concurrent_threads_agree)
{
    REQUIRE(XCS()); cthr t[4]; HANDLE h[4]; int i, k;
    memset(t, 0, sizeof t);
    for (i = 0; i < 4; i++) { t[i].idx = i; h[i] = CreateThread(NULL, 0, cthr_main, &t[i], 0, NULL); }
    WaitForMultipleObjects(4, h, TRUE, 120000);
    for (i = 0; i < 4; i++) { if (h[i]) CloseHandle(h[i]); EXPECT_EQ(t[i].failed, 0); }
    for (k = 0; k < 2; k++) for (i = 1; i < 4; i++) CHECKF(t[i].shdr[k] && t[0].shdr[k] && t[i].shdr_n[k] == t[0].shdr_n[k] && !memcmp(t[i].shdr[k], t[0].shdr[k], t[0].shdr_n[k]), "thread %d vs 0, shader %d", i, k);
    for (i = 0; i < 4; i++) for (k = 0; k < 2; k++) free(t[i].shdr[k]);
}

/* effects: independent effect objects used from several threads on the shared device */
typedef struct { ID3D10Device *dev; volatile LONG failed; } ethr;
static DWORD WINAPI ethr_main(LPVOID p)
{
    ethr *t = p; int it;
    for (it = 0; it < 6; it++) {
        ID3D10Effect *fx = NULL;
        if (FAILED(ue_make_effect(UE_FX_BASIC, t->dev, &fx)) || !fx) { InterlockedIncrement(&t->failed); continue; }
        ID3D10EffectScalarVariable *s = ID3D10EffectVariable_AsScalar(ID3D10Effect_GetVariableByName(fx, "fA"));
        float f = 0;
        if (FAILED(ID3D10EffectScalarVariable_SetFloat(s, 3.25f + it)) || FAILED(ID3D10EffectScalarVariable_GetFloat(s, &f)) || f != 3.25f + it) InterlockedIncrement(&t->failed);
        ID3D10Effect_Release(fx);
    }
    return 0;
}
TEST(d3d10_utils_effects_fx_concurrent_effect_create_and_set)
{
    NEED_DEVICE(dev);
    ethr t[3]; HANDLE h[3]; int i;
    for (i = 0; i < 3; i++) { t[i].dev = dev; t[i].failed = 0; h[i] = CreateThread(NULL, 0, ethr_main, &t[i], 0, NULL); }
    WaitForMultipleObjects(3, h, TRUE, 180000);
    for (i = 0; i < 3; i++) { if (h[i]) CloseHandle(h[i]); EXPECT_EQ(t[i].failed, 0); }
}
