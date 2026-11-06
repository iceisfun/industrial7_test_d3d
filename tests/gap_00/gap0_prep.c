/* D3D10GetShaderDebugInfo, D3D10PreprocessShader, D3DDisassemble11Trace (argument handling). */
#include "gap0.h"

typedef HRESULT (WINAPI *dbginfo_t)(const void *, SIZE_T, ID3DBlob **);
typedef HRESULT (WINAPI *prep_t)(const char *, SIZE_T, const char *, const D3D_SHADER_MACRO *, ID3DInclude *, ID3DBlob **, ID3DBlob **);
typedef HRESULT (WINAPI *trace_t)(const void *, SIZE_T, void *, UINT, UINT, UINT, ID3DBlob **);

static const char *const dbg_mods[] = { "d3d10.dll", "d3d10_1.dll" };
#define SENTINEL ((ID3DBlob *)(ULONG_PTR)0x10)

/* ---- D3D10GetShaderDebugInfo ---- */

TEST(gap0_dbginfo_null_data)
{
    size_t m;
    for (m = 0; m < 2; m++) {
        dbginfo_t fn = (dbginfo_t)t_proc_in(dbg_mods[m], "D3D10GetShaderDebugInfo");
        ID3DBlob *out = SENTINEL;
        if (!fn) { t_skip("missing D3D10GetShaderDebugInfo in %s", dbg_mods[m]); return; }
        EXPECT_HR(fn(NULL, 0, &out), INVALIDCALL_HR);
        CHECK(out == NULL);                         /* output is cleared before validation */
        out = SENTINEL;
        EXPECT_HR(fn(NULL, 100, &out), INVALIDCALL_HR);
        CHECK(out == NULL);
        EXPECT_HR(fn(NULL, 0, NULL), INVALIDCALL_HR);
    }
}

TEST(gap0_dbginfo_bad_magic)
{
    size_t m, i;
    unsigned char buf[96];
    for (m = 0; m < 2; m++) {
        dbginfo_t fn = (dbginfo_t)t_proc_in(dbg_mods[m], "D3D10GetShaderDebugInfo");
        if (!fn) { t_skip("missing export"); return; }
        for (i = 0; i < 4; i++) {
            ID3DBlob *out = SENTINEL;
            memset(buf, 0, sizeof buf);
            memcpy(buf, "DXBC", 4);
            buf[i] ^= 0x20;                         /* one wrong magic byte */
            EXPECT_HR(fn(buf, sizeof buf, &out), E_FAIL_HR);
            CHECK(out == NULL);
        }
        {   ID3DBlob *out = SENTINEL;
            memset(buf, 0xcd, sizeof buf);
            EXPECT_HR(fn(buf, sizeof buf, &out), E_FAIL_HR);
            CHECK(out == NULL);
            out = SENTINEL;
            /* bad magic is rejected before the size is examined */
            EXPECT_HR(fn(buf, 0, &out), E_FAIL_HR);
            CHECK(out == NULL);
        }
    }
}

TEST(gap0_dbginfo_null_out_valid_data)
{
    ID3DBlob *sh = NULL;
    size_t m;
    if (FAILED(d3dh_compile(GAP0_PS_SRC, "main", "ps_4_0", D3DCOMPILE_DEBUG, &sh))) { t_skip("no compiler"); return; }
    for (m = 0; m < 2; m++) {
        dbginfo_t fn = (dbginfo_t)t_proc_in(dbg_mods[m], "D3D10GetShaderDebugInfo");
        if (!fn) continue;
        EXPECT_HR(fn(d3dh_blob_ptr(sh), d3dh_blob_size(sh), NULL), INVALIDCALL_HR);
    }
    SAFE_RELEASE(sh);
}

TEST(gap0_dbginfo_zero_filled_container)
{
    size_t m;
    unsigned char buf[96];
    for (m = 0; m < 2; m++) {
        dbginfo_t fn = (dbginfo_t)t_proc_in(dbg_mods[m], "D3D10GetShaderDebugInfo");
        ID3DBlob *out = SENTINEL;
        if (!fn) { t_skip("missing export"); return; }
        memset(buf, 0, sizeof buf); memcpy(buf, "DXBC", 4);
        EXPECT_HR_FAILED(fn(buf, sizeof buf, &out));   /* hash / container invalid */
        CHECK(out == NULL);
    }
}

TEST(gap0_dbginfo_compiled_shader)
{
    /* Shader compiled with and without debug info. */
    int dbg;
    for (dbg = 0; dbg < 2; dbg++) {
        ID3DBlob *sh = NULL, *out = NULL;
        dbginfo_t fn = (dbginfo_t)t_proc_in("d3d10.dll", "D3D10GetShaderDebugInfo");
        HRESULT hr;
        REQUIRE(fn);
        if (FAILED(d3dh_compile(GAP0_PS_SRC, "main", "ps_4_0", dbg ? D3DCOMPILE_DEBUG : 0, &sh))) { t_skip("no compiler"); return; }
        hr = fn(d3dh_blob_ptr(sh), d3dh_blob_size(sh), &out);
        t_note("D3D10GetShaderDebugInfo(%s) = %s, blob %p size %u", dbg ? "debug build" : "plain build", d3dh_hr_name(hr), (void *)out, out ? (unsigned)d3dh_blob_size(out) : 0);
        if (dbg) { EXPECT_HR(hr, INVALIDCALL_HR); CHECK(out == NULL); }   /* a debug-flag build has no SDBG chunk d3d10 understands */
        else { EXPECT_HR(hr, S_OK); }
        SAFE_RELEASE(out); SAFE_RELEASE(sh);
    }
}

/* ---- D3D10PreprocessShader ---- */

/* the emulated d3d10 only has an aborting stub for this export */
static prep_t get_prep(const char *mod) { return t_is_wine() ? NULL : (prep_t)t_proc_in(mod, "D3D10PreprocessShader"); }

static char *blob_str(ID3DBlob *b)
{
    SIZE_T n = d3dh_blob_size(b);
    char *s = malloc(n + 1);
    memcpy(s, d3dh_blob_ptr(b), n); s[n] = 0;
    return s;
}

static void dump_blob(const char *tag, ID3DBlob *b)
{
    char *s = blob_str(b), *q; char esc[600]; size_t n = 0;
    for (q = s; *q && n < sizeof esc - 5; q++) {
        if (*q == '\n') { esc[n++] = '\\'; esc[n++] = 'n'; } else if (*q == '\r') { esc[n++] = '\\'; esc[n++] = 'r'; } else esc[n++] = *q;
    }
    esc[n] = 0; t_note("PROBE %s len=%u: %s", tag, (unsigned)d3dh_blob_size(b), esc); free(s);
}

/* Pin the exact preprocessed text: Windows emits one token stream, tokens separated by single spaces, one source line per output line. */
static void expect_text(const char *tag, ID3DBlob *b, const char *want)
{
    char *s = blob_str(b);
    SIZE_T n = d3dh_blob_size(b);
    /* the blob carries the terminating NUL */
    CHECKF(n == strlen(want) + 1 && !memcmp(s, want, n), "%s: preprocessed text mismatch (len %u want %u)", tag, (unsigned)n, (unsigned)strlen(want) + 1);
    if (strcmp(s, want)) dump_blob(tag, b);
    free(s);
}

TEST(gap0_prep_invalid_args)
{
    size_t m;
    static const char src[] = "float4 a;\n";
    for (m = 0; m < 2; m++) {
        prep_t fn = get_prep(dbg_mods[m]);
        ID3DBlob *out = NULL, *err = NULL;
        if (!fn) { t_skip("missing D3D10PreprocessShader"); return; }
        EXPECT_HR(fn(NULL, 10, "x.hlsl", NULL, NULL, &out, &err), E_INVALIDARG_HR);
        EXPECT_HR(fn(NULL, 0, NULL, NULL, NULL, &out, NULL), E_INVALIDARG_HR);
        EXPECT_HR(fn(src, sizeof src - 1, "x.hlsl", NULL, NULL, NULL, &err), E_INVALIDARG_HR);
        EXPECT_HR(fn(src, sizeof src - 1, NULL, NULL, NULL, NULL, NULL), E_INVALIDARG_HR);
        CHECK(out == NULL); CHECK(err == NULL);
    }
}

TEST(gap0_prep_plain_text)
{
    size_t m;
    static const char src[] = "float4 a;\nfloat4 b;\n";
    for (m = 0; m < 2; m++) {
        prep_t fn = get_prep(dbg_mods[m]);
        ID3DBlob *out = NULL, *err = NULL;
        HRESULT hr;
        char *s;
        if (!fn) { t_skip("missing D3D10PreprocessShader"); return; }
        hr = fn(src, sizeof src - 1, "plain.hlsl", NULL, NULL, &out, &err);
        EXPECT_HR(hr, S_OK);
        if (out) {
            expect_text("plain", out, "float4 a ;\nfloat4 b ;\n");
        }
        SAFE_RELEASE(out); SAFE_RELEASE(err);
        /* the error blob is optional */
        hr = fn(src, sizeof src - 1, "plain.hlsl", NULL, NULL, &out, NULL);
        EXPECT_HR(hr, S_OK);
        SAFE_RELEASE(out);
        /* NULL file name is accepted */
        hr = fn(src, sizeof src - 1, NULL, NULL, NULL, &out, NULL);
        EXPECT_HR(hr, S_OK);
        SAFE_RELEASE(out);
    }
}

TEST(gap0_prep_macros)
{
    static const char src[] = "float x = FOO_VALUE;\nfloat y = BAR_VALUE;\n#ifdef FLAG_ON\nfloat z_on;\n#else\nfloat z_off;\n#endif\n";
    D3D_SHADER_MACRO defs[] = { { "FOO_VALUE", "42" }, { "BAR_VALUE", "(1+2)" }, { "FLAG_ON", "1" }, { NULL, NULL } };
    D3D_SHADER_MACRO defs2[] = { { "FOO_VALUE", "7" }, { NULL, NULL } };
    size_t m;
    for (m = 0; m < 2; m++) {
        prep_t fn = get_prep(dbg_mods[m]);
        ID3DBlob *out = NULL;
        char *s;
        if (!fn) { t_skip("missing export"); return; }
        EXPECT_HR(fn(src, sizeof src - 1, "m.hlsl", defs, NULL, &out, NULL), S_OK);
        if (out) {
            expect_text("macros defs", out, "float x = 42 ;\nfloat y = ( 1 + 2 ) ;\nfloat z_on ;\n");
        }
        SAFE_RELEASE(out);
        EXPECT_HR(fn(src, sizeof src - 1, "m.hlsl", defs2, NULL, &out, NULL), S_OK);
        if (out) {
            expect_text("macros defs2", out, "float x = 7 ;\nfloat y = BAR_VALUE ;\nfloat z_off ;\n");
        }
        SAFE_RELEASE(out);
    }
}

TEST(gap0_prep_inline_define_and_comments)
{
    static const char src[] = "#define TWICE(a) ((a)*2)\n// comment line gone\nfloat v = TWICE(3); /* block */ float w;\n";
    prep_t fn = get_prep("d3d10.dll");
    ID3DBlob *out = NULL;
    char *s;
    REQUIRE(fn);
    EXPECT_HR(fn(src, sizeof src - 1, "d.hlsl", NULL, NULL, &out, NULL), S_OK);
    if (out) {
        expect_text("inline define", out, "float v = ( ( 3 ) * 2 ) ;\nfloat w ;\n");
    }
    SAFE_RELEASE(out);
}

TEST(gap0_prep_size_limits_input)
{
    static const char src[] = "AAAA_TOKEN\nBBBB_TOKEN\n";
    prep_t fn = get_prep("d3d10.dll");
    ID3DBlob *out = NULL;
    char *s;
    REQUIRE(fn);
    EXPECT_HR(fn(src, 11, "s.hlsl", NULL, NULL, &out, NULL), S_OK);
    if (out) {
        s = blob_str(out);
        CHECK(strstr(s, "AAAA_TOKEN") != NULL);
        if (strstr(s, "BBBB_TOKEN")) t_note("source size did not truncate the preprocessed text");
        free(s);
    }
    SAFE_RELEASE(out);
}

TEST(gap0_prep_error_directive)
{
    static const char src[] = "float a;\n#error stop_here_marker\nfloat b;\n";
    size_t m;
    for (m = 0; m < 2; m++) {
        prep_t fn = get_prep(dbg_mods[m]);
        ID3DBlob *out = NULL, *err = NULL;
        HRESULT hr;
        if (!fn) { t_skip("missing export"); return; }
        hr = fn(src, sizeof src - 1, "e.hlsl", NULL, NULL, &out, &err);
        EXPECT_HR(hr, E_FAIL_HR);
        CHECK(out == NULL);
        CHECK(err != NULL);
        if (err) {
            char *s = blob_str(err);
            CHECKF(!strcmp(s, "e.hlsl(2): error: stop_here_marker\n"), "error text: %s", s);
            CHECKF(strstr(s, "stop_here_marker") != NULL, "error text lacks the #error message: %s", s);
            free(s);
        }
        SAFE_RELEASE(out); SAFE_RELEASE(err);
    }
}

TEST(gap0_prep_unbalanced_if)
{
    static const char src[] = "#if 1\nfloat a;\n";
    prep_t fn = get_prep("d3d10.dll");
    ID3DBlob *out = NULL, *err = NULL;
    HRESULT hr;
    REQUIRE(fn);
    hr = fn(src, sizeof src - 1, "u.hlsl", NULL, NULL, &out, &err);
    /* An unterminated #if is only diagnosed: the call succeeds, returns the text, and reports the error in the message blob. */
    EXPECT_HR(hr, S_OK);
    CHECK(out != NULL && err != NULL);
    if (out) expect_text("unbalanced out", out, "float a ;\n");
    if (err) {
        char *m = blob_str(err);
        CHECKF(strstr(m, "u.hlsl(3): error X1502: unexpected end of file") != NULL, "error text: %s", m);
        free(m);
    }
    SAFE_RELEASE(out); SAFE_RELEASE(err);
}

/* ---- include handler ---- */
typedef struct {
    const ID3DIncludeVtbl *lpVtbl;
    int opens, closes;
    int types[8];
    char names[8][32];
    const void *parents[8];
    const void *returned[8];
    const void *closed[8];
    int fail_open;
} inc_t;
static const char inc_a[] = "float from_a;\n#include \"b.h\"\n";
static const char inc_b[] = "float from_b;\n";
static const char inc_s[] = "float from_sys;\n";

static HRESULT WINAPI inc_open(ID3DInclude *This, D3D_INCLUDE_TYPE t, LPCSTR name, LPCVOID parent, LPCVOID *data, UINT *bytes)
{
    inc_t *s = (inc_t *)This;
    const char *src;
    int i = s->opens++;
    if (i < 8) { s->types[i] = t; strncpy(s->names[i], name, 31); s->parents[i] = parent; }
    if (s->fail_open) return E_FAIL;
    if (!strcmp(name, "a.h")) src = inc_a;
    else if (!strcmp(name, "b.h")) src = inc_b;
    else if (!strcmp(name, "sys.h")) src = inc_s;
    else return E_FAIL;
    *data = src; *bytes = (UINT)strlen(src);
    if (i < 8) s->returned[i] = src;
    return S_OK;
}
static HRESULT WINAPI inc_close(ID3DInclude *This, LPCVOID data)
{
    inc_t *s = (inc_t *)This;
    if (s->closes < 8) s->closed[s->closes] = data;
    s->closes++;
    return S_OK;
}
static const ID3DIncludeVtbl inc_vtbl = { inc_open, inc_close };

TEST(gap0_prep_include_callbacks)
{
    static const char src[] = "#include \"a.h\"\n#include <sys.h>\nfloat from_main;\n";
    size_t m;
    for (m = 0; m < 2; m++) {
        prep_t fn = get_prep(dbg_mods[m]);
        inc_t inc = { &inc_vtbl };
        ID3DBlob *out = NULL, *err = NULL;
        HRESULT hr;
        int i, j;
        if (!fn) { t_skip("missing export"); return; }
        hr = fn(src, sizeof src - 1, "main.hlsl", NULL, (ID3DInclude *)&inc, &out, &err);
        EXPECT_HR(hr, S_OK);
        EXPECT_EQ(inc.opens, 3);
        EXPECT_EQ(inc.closes, 3);
        if (inc.opens >= 3) {
            EXPECT_STR(inc.names[0], "a.h");
            EXPECT_EQ(inc.types[0], D3D_INCLUDE_LOCAL);
            EXPECT_STR(inc.names[1], "b.h");
            EXPECT_EQ(inc.types[1], D3D_INCLUDE_LOCAL);
            EXPECT_STR(inc.names[2], "sys.h");
            EXPECT_EQ(inc.types[2], D3D_INCLUDE_SYSTEM);
            if (inc.parents[0] != NULL) t_note("top-level include pParentData = %p (expected NULL)", inc.parents[0]);
            if (inc.parents[1] != inc.returned[0]) t_note("nested include pParentData %p != parent's data %p", inc.parents[1], inc.returned[0]);
        }
        /* every pointer returned by Open is handed back to Close exactly once */
        for (i = 0; i < 3 && i < inc.opens; i++) {
            int found = 0;
            for (j = 0; j < 3 && j < inc.closes; j++) if (inc.closed[j] == inc.returned[i]) found++;
            EXPECT_EQ(found, 1);
        }
        if (out) {
            char *s = blob_str(out);
            CHECK(strstr(s, "from_a") && strstr(s, "from_b") && strstr(s, "from_sys") && strstr(s, "from_main"));
            if (strstr(s, "from_a") && strstr(s, "from_b") && strstr(s, "from_sys") && strstr(s, "from_main")) {
                CHECK(strstr(s, "from_a") < strstr(s, "from_b"));
                CHECK(strstr(s, "from_b") < strstr(s, "from_sys"));
                CHECK(strstr(s, "from_sys") < strstr(s, "from_main"));
            }
            CHECK(strstr(s, "#include") == NULL);
            free(s);
        }
        SAFE_RELEASE(out); SAFE_RELEASE(err);
    }
}

TEST(gap0_prep_include_open_fails)
{
    static const char src[] = "#include \"a.h\"\nfloat after;\n";
    prep_t fn = get_prep("d3d10.dll");
    inc_t inc = { &inc_vtbl };
    ID3DBlob *out = NULL, *err = NULL;
    HRESULT hr;
    REQUIRE(fn);
    inc.fail_open = 1;
    hr = fn(src, sizeof src - 1, "main.hlsl", NULL, (ID3DInclude *)&inc, &out, &err);
    EXPECT_HR_FAILED(hr);
    EXPECT_EQ(inc.opens, 1);
    EXPECT_EQ(inc.closes, 0);                       /* nothing was opened successfully, nothing to close */
    SAFE_RELEASE(out); SAFE_RELEASE(err);
}

TEST(gap0_prep_include_without_handler)
{
    static const char src[] = "#include \"nothing.h\"\nfloat after;\n";
    prep_t fn = get_prep("d3d10.dll");
    ID3DBlob *out = NULL, *err = NULL;
    HRESULT hr;
    REQUIRE(fn);
    hr = fn(src, sizeof src - 1, "main.hlsl", NULL, NULL, &out, &err);
    EXPECT_HR_FAILED(hr);
    SAFE_RELEASE(out); SAFE_RELEASE(err);
}

TEST(gap0_prep_matches_d3dpreprocess)
{
    static const char src[] = "#define K 5\nfloat a = K;\n";
    prep_t fn = get_prep("d3d10.dll");
    typedef HRESULT (WINAPI *dp_t)(const void *, SIZE_T, const char *, const D3D_SHADER_MACRO *, ID3DInclude *, ID3DBlob **, ID3DBlob **);
    dp_t dp = (dp_t)t_proc_in("d3dcompiler_47.dll", "D3DPreprocess");
    ID3DBlob *a = NULL, *b = NULL, *e1 = NULL, *e2 = NULL;
    REQUIRE(fn); REQUIRE(dp);
    if (t_is_wine()) { t_skip("emulated D3DPreprocess faults on this input"); return; }
    if (SUCCEEDED(fn(src, sizeof src - 1, "k.hlsl", NULL, NULL, &a, &e1)) && SUCCEEDED(dp(src, sizeof src - 1, "k.hlsl", NULL, NULL, &b, &e2))) {
        char *x = blob_str(a), *y = blob_str(b);
        /* d3d10 emits bare token text; D3DPreprocess prefixes a #line with the (absolute) file name and keeps a trailing blank. */
        expect_text("D3D10PreprocessShader", a, "float a = 5 ;\n");
        CHECKF(!strncmp(y, "#line 1 \"", 9), "D3DPreprocess does not start with a #line directive: %.40s", y);
        { size_t ny = strlen(y), nt = strlen("k.hlsl\"\n\nfloat a = 5 ; \n");
          CHECKF(ny > nt && !strcmp(y + ny - nt, "k.hlsl\"\n\nfloat a = 5 ; \n"), "D3DPreprocess tail mismatch"); }
        CHECK(strcmp(x, y) != 0);
        free(x); free(y);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(e1); SAFE_RELEASE(e2);
}

/* ---- D3DDisassemble11Trace: argument validation that fails before the trace object is used ---- */

TEST(gap0_dis11trace_invalid_container)
{
    trace_t fn = (trace_t)t_proc_in("d3dcompiler_47.dll", "D3DDisassemble11Trace");
    unsigned char zeros[128] = { 0 };
    ID3DBlob *out = NULL;
    HRESULT hr;
    REQUIRE(fn);
    if (t_is_wine()) { t_skip("emulated D3DDisassemble11Trace is a stub that faults"); return; }
    hr = fn(zeros, sizeof zeros, NULL, 0, 0, 0, &out);
    EXPECT_HR_FAILED(hr); CHECK(out == NULL);
    hr = fn(NULL, 0, NULL, 0, 0, 0, &out);
    EXPECT_HR_FAILED(hr); CHECK(out == NULL);
    hr = fn(zeros, 0, NULL, 0, 0, 0, &out);
    EXPECT_HR_FAILED(hr); CHECK(out == NULL);
    memcpy(zeros, "DXBC", 4);
    hr = fn(zeros, sizeof zeros, NULL, 0, 0, 0, &out);
    EXPECT_HR_FAILED(hr); CHECK(out == NULL);
}
