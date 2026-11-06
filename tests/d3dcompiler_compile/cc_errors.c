/* Golden diagnostics: "name(line,col): error|warning Xnnnn: text" for representative snippets.
 * every row is checked (code, severity, line, format); the tables were regenerated from a Windows 10 run (CC_DUMP=1). */
#include "cc.h"

#define NEED() do { REQUIRE(CCF(D3DCompile)); } while (0)

typedef struct {
    int hard;            /* 1: CHECK the code, 0: note only */
    char kind;           /* 'e' error expected (compile fails), 'w' warning expected (compile succeeds), 'n' compiles with no diagnostics */
    int code;
    int ln;              /* expected line of the first diagnostic with this code, 0 = unchecked */
    const char *target;
    UINT flags;
    const char *src;
} cc_row;

#define FR(body) "float4 main(float4 p : SV_Position) : SV_Target {\n" body "\n    return 0;\n}\n"
#define FRV(body) "float4 main(float4 p : SV_Position) : SV_Target {\n" body "\n}\n"

/* CC_DUMP=1 in the environment prints every row's actual diagnostics ("DUMP tag idx hr diags") so the tables can be
 * regenerated from a Windows run instead of guessed. */
static void cc_dump_row(const char *tag, unsigned i, const cc_r *r)
{
    const char *s = r->err;
    char buf[512]; size_t o = 0;
    buf[0] = 0;
    while (*s && o + 40 < sizeof buf) {
        const char *e = strchr(s, '\n');
        size_t l = e ? (size_t)(e - s) : strlen(s);
        char ln[1024];
        const char *k;
        int line = 0, code = 0;
        if (l >= sizeof ln) l = sizeof ln - 1;
        memcpy(ln, s, l); ln[l] = 0;
        if ((k = strstr(ln, "): error X")) || (k = strstr(ln, "): warning X"))) {
            const char *p = k; int kind = strstr(k, "warning") == k + 3 ? 'w' : 'e';
            while (p > ln && *p != '(') p--;
            if (*p == '(') sscanf(p + 1, "%d", &line);
            sscanf(strstr(k, " X") + 2, "%d", &code);
            o += snprintf(buf + o, sizeof buf - o, "%c%d@%d,", kind, code, line);
        } else if ((k = strstr(ln, "error X")) == ln || (k = strstr(ln, "warning X")) == ln) {
            sscanf(strstr(ln, " X") + 2, "%d", &code);
            o += snprintf(buf + o, sizeof buf - o, "%c%d@0,", ln[0], code);
        }
        if (!e) break;
        s = e + 1;
    }
    printf("  DUMP %s %u hr=%08lx code=%d diags=%s\n", tag, i, (unsigned long)r->hr, r->code != NULL, buf);
}

static void cc_rows_run(const cc_row *rows, unsigned n, const char *tag)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        const cc_row *w = &rows[i];
        cc_r r;
        char pat[40], pat2[MAX_PATH + 24], full[MAX_PATH];
        const char *at;
        const char *s;
        int bad_lines = 0, nlines = 0;
        cc_compile_full(&r, w->src, strlen(w->src), "t.hlsl", NULL, NULL, "main", w->target, w->flags, 0);
        if (getenv("CC_DUMP")) cc_dump_row(tag, i, &r);
        if (w->code == 0) snprintf(pat, sizeof pat, "error:"); /* #error: no X code */
        else snprintf(pat, sizeof pat, "%s X%d", w->kind == 'e' ? "error" : "warning", w->code);
        if (w->kind == 'n') {
            /* Win10 accepts the source silently: success, code produced, no diagnostics */
            CHECKF(SUCCEEDED(r.hr) && r.code != NULL, "%s[%u] must compile (hr=0x%08lx): %.160s", tag, i, (unsigned long)r.hr, r.err);
            CHECKF(!strstr(r.err, " X"), "%s[%u] unexpected diagnostics: %.160s", tag, i, r.err);
            cc_free(&r);
            continue;
        }
        at = strstr(r.err, pat);
        cc_fullname("t.hlsl", full, sizeof full);
        snprintf(pat2, sizeof pat2, "%s(%d,", full, w->ln);
        if (w->hard) {
            CHECKF(at != NULL, "%s[%u] expected '%s' in: %.220s", tag, i, pat, r.err);
            if (w->kind == 'e') {
                CHECKF((LONG)r.hr < 0 && r.code == NULL, "%s[%u] must fail (hr=0x%08lx)", tag, i, (unsigned long)r.hr);
                CHECKF(r.hr == E_FAIL || r.hr == CC_COMPILEFAIL, "%s[%u] hr=0x%08lx", tag, i, (unsigned long)r.hr);
            } else {
                CHECKF(SUCCEEDED(r.hr) && r.code != NULL, "%s[%u] must succeed with warning (hr=0x%08lx)", tag, i, (unsigned long)r.hr);
            }
            if (at && w->ln) {
                /* the line number of the diagnostic carrying this code */
                const char *ls = at;
                while (ls > r.err && ls[-1] != '\n') ls--;
                CHECKF(!strncmp(ls, pat2, strlen(pat2)), "%s[%u] expected line %d, got '%.60s'", tag, i, w->ln, ls);
            }
        } else if (!at) {
            t_note("%s[%u] expected %s but got hr=0x%08lx: %.160s", tag, i, pat, (unsigned long)r.hr, r.err);
        }
        /* every diagnostic line begins "<full path of t.hlsl>(" and must be well formed */
        s = r.err;
        while (*s) {
            const char *e = strchr(s, '\n');
            size_t l = e ? (size_t)(e - s) : strlen(s);
            if (l > strlen(full) && !strncmp(s, full, strlen(full)) && s[strlen(full)] == '(') {
                nlines++;
                if (!cc_line_format_ok(s, l, "t.hlsl")) bad_lines++;
            }
            if (!e) break;
            s = e + 1;
        }
        if (w->hard) CHECKF(bad_lines == 0, "%s[%u] malformed diagnostic line in: %.220s", tag, i, r.err);
        cc_free(&r);
    }
}

#define N(a) (sizeof(a) / sizeof((a)[0]))

/* ---------------- X3000 syntax ---------------- */
static const cc_row cc_syntax[] = {
    { 1, 'e', 3000, 3, "ps_5_0", 0, FR("    float4 a = 1") },
    { 1, 'e', 3000, 2, "ps_5_0", 0, FR("    float4 a = (1;") },
    { 1, 'e', 3000, 2, "ps_5_0", 0, FR("    float4 a = ;") },
    { 1, 'e', 3000, 2, "ps_5_0", 0, FR("    float4 = 1;") },
    { 1, 'e', 3000, 2, "ps_5_0", 0, FR("    if (1 { }") },
    { 1, 'e', 3000, 2, "ps_5_0", 0, FR("    for (int i = 0; i < 4 i++) { }") },
    { 1, 'e', 3000, 2, "ps_5_0", 0, FR("    float4 a = float4(1,2,3,4;") },
    { 1, 'e', 3000, 2, "ps_5_0", 0, FR("    int 1x = 0;") },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main( : SV_Target { return 1; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() SV_Target { return 1; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { return 1; " },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { return 1; } }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "struct S { float a } ; float4 main() : SV_Target { return 1; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "Foo main() : SV_Target { return 1; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { return 1 +; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { return * 1; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { while { } return 1; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { else { } return 1; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { return 1; } @" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "cbuffer cb { float4 a; float4 main() : SV_Target { return a; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main(float4 a,) : SV_Target { return a; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { float a[; return 1; }" },
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { switch (1) { default } return 1; }" },
    { 1, 'e', 3000, 1, "vs_5_0", 0, "float4 main() : SV_Position { return float4(0,0,0,1) }" },
};

TEST(d3dcompiler_compile_errors_x3000_syntax)
{
    WIN_ONLY();
    NEED();
    cc_rows_run(cc_syntax, N(cc_syntax), "syntax");
}

/* ---------------- identifiers, scope, redefinition ---------------- */
static const cc_row cc_names[] = {
    { 1, 'e', 3004, 2, "ps_5_0", 0, FR("    float4 a = missing;") },
    { 1, 'e', 3004, 2, "ps_5_0", 0, FR("    float4 a = foo(1);") },
    { 1, 'e', 3004, 2, "ps_5_0", 0, FR("    { float a = 1; } a = 2;") },
    { 1, 'e', 3004, 2, "ps_5_0", 0, FR("    a = 1; float a;") },
    { 1, 'e', 3004, 2, "ps_5_0", 0, FR("    float4 a = undefined_tex.Sample(s, p.xy);") },
    { 1, 'e', 3004, 2, "ps_5_0", 0, FR("    float a = p.x + (q * 2);") },
    { 1, 'e', 3004, 1, "ps_5_0", 0, "float4 main() : SV_Target { return k; }" },
    { 1, 'e', 3004, 1, "ps_5_0", 0, "float4 helper(); float4 main() : SV_Target { return nothing(); }" },
    { 1, 'e', 3004, 2, "ps_5_0", 0, "struct S { float a; };\nfloat4 main() : SV_Target { return T; }" },
    { 1, 'n', 0, 0, "ps_5_0", 0, "float4 main() : SV_Target { for (int i = 0; i < 4; i++) { } return i; }" }, /* Win10: was e 3004 */
    { 1, 'e', 3003, 2, "ps_5_0", 0, FR("    float a; float a;") },
    { 1, 'e', 3003, 2, "ps_5_0", 0, FR("    float a = 1; int a = 2;") },
    { 1, 'e', 3003, 2, "ps_5_0", 0, "float f() { return 1; }\nfloat f() { return 2; }\nfloat4 main() : SV_Target { return f(); }" },
    { 1, 'e', 3003, 2, "ps_5_0", 0, "float g;\nfloat g;\nfloat4 main() : SV_Target { return g; }" },
    { 1, 'e', 3003, 2, "ps_5_0", 0, "struct S { float a; };\nstruct S { float b; };\nfloat4 main() : SV_Target { return 1; }" },
    { 1, 'e', 3003, 1, "ps_5_0", 0, "float4 main(float a, float a) : SV_Target { return a; }" },
    { 1, 'e', 3003, 2, "ps_5_0", 0, "cbuffer c1 { float v; };\ncbuffer c2 { float v; };\nfloat4 main() : SV_Target { return v; }" },
    { 1, 'e', 3003, 1, "ps_5_0", 0, "struct S { float a; float a; };\nfloat4 main() : SV_Target { return 1; }" },
    { 1, 'w', 3078, 2, "ps_5_0", 0, FR("    int i = 0; for (int i = 0; i < 2; i++) { }") },
    { 1, 'n', 0, 0, "ps_5_0", 0, "float4 main() : SV_Target { float4 float4; return 1; }" }, /* Win10: was e 3005 */
};

TEST(d3dcompiler_compile_errors_names_and_redefinition)
{
    WIN_ONLY();
    NEED();
    cc_rows_run(cc_names, N(cc_names), "names");
}

/* ---------------- types and conversions ---------------- */
static const cc_row cc_types[] = {
    { 1, 'e', 3014, 2, "ps_5_0", 0, FR("    float4 a = float4(1, 2, 3);") },
    { 1, 'e', 3014, 2, "ps_5_0", 0, FR("    float2 a = float2(1, 2, 3);") },
    { 1, 'e', 3014, 2, "ps_5_0", 0, FR("    float3x3 m = float3x3(1, 2, 3, 4);") },
    { 1, 'e', 3014, 2, "ps_5_0", 0, FR("    float4 a = float4(1, 2, 3, 4, 5);") },
    { 1, 'e', 3017, 2, "ps_5_0", 0, "struct S { float a; };\n" "float4 main() : SV_Target { S s; float f = s; return f; }" },
    { 1, 'e', 3017, 2, "ps_5_0", 0, "Texture2D t;\nfloat4 main() : SV_Target { return t; }" },
    { 1, 'e', 3017, 2, "ps_5_0", 0, "SamplerState s;\nfloat4 main() : SV_Target { float f = s; return f; }" },
    { 1, 'e', 3017, 2, "ps_5_0", 0, "struct S { float a; };\nfloat4 main() : SV_Target { S s; return s; }" },
    { 1, 'n', 0, 0, "ps_5_0", 0, "struct S { float a; }; struct T { float a; };\nfloat4 main() : SV_Target { S s; T t = s; return 1; }" }, /* Win10: was e 3017 */
    { 1, 'e', 3018, 2, "ps_5_0", 0, FR("    float2 v = p.xy; float f = v.z;") },
    { 1, 'e', 3018, 2, "ps_5_0", 0, FR("    float f = p.q;") },
    { 1, 'e', 3018, 2, "ps_5_0", 0, FR("    float3 v = p.xyz; float f = v.w;") },
    { 1, 'e', 3018, 2, "ps_5_0", 0, "struct S { float a; };\nfloat4 main() : SV_Target { S s; s.nothere = 1; return 1; }" },
    { 1, 'e', 3018, 2, "ps_5_0", 0, FR("    float f = p.xyzwx;") },
    { 1, 'e', 3018, 2, "ps_5_0", 0, FR("    float f = p.xg;") },
    { 1, 'e', 3018, 2, "ps_5_0", 0, FR("    float3x3 m = 0; float f = m._44;") },
    { 1, 'e', 3121, 2, "ps_5_0", 0, FR("    float f = 1; float g = f[0];") },
    { 1, 'e', 3121, 2, "ps_5_0", 0, "struct S { float a; };\nfloat4 main() : SV_Target { S s; return s[0]; }" },
    { 1, 'w', 3206, 2, "ps_5_0", 0, FR("    float3 a = p;") },
    { 1, 'w', 3206, 2, "ps_5_0", 0, FR("    float f = p;") },
    { 1, 'w', 3206, 2, "ps_5_0", 0, FR("    float2 a = float3(1,2,3);") },
    { 1, 'w', 3206, 2, "ps_5_0", 0, FR("    float3 a; a = p.xyzw;") },
    { 1, 'e', 3000, 2, "ps_5_0", 0, FR("    float4 a = p.;") },
    { 1, 'w', 3206, 1, "ps_5_0", 0, "float4 main() : SV_Target { float2 a = 1; float3 b = 1; return float4(a + b, 0, 1); }" }, /* Win10: was e 3020 */
    { 1, 'n', 0, 0, "ps_5_0", 0, FR("    float f = 1.0; half h = f; int i = f;") }, /* Win10: was w 3205 */
    { 1, 'e', 3020, 3, "ps_5_0", 0, "struct S { float a; };\nfloat4 main() : SV_Target {\n S s; if (s) { } return 1; }" }, /* Win10: was e 3022 */
    { 1, 'e', 3012, 1, "ps_5_0", 0, "float4 main() : SV_Target { const float c; return c; }" },
    { 1, 'e', 3025, 2, "ps_5_0", 0, "static const float c = 1;\nfloat4 main() : SV_Target { c = 2; return c; }" },
    { 1, 'e', 3018, 1, "ps_5_0", 0, "float4 main() : SV_Target { float4 a; a.x.y = 1; return a; }" }, /* Win10: was e 3033 */
    { 1, 'e', 3059, 1, "ps_5_0", 0, "float4 main() : SV_Target { float a[0]; return 1; }" },
    { 1, 'e', 3059, 1, "ps_5_0", 0, "float4 main() : SV_Target { float a[-1]; return 1; }" },
    { 1, 'e', 3058, 2, "ps_5_0", 0, "cbuffer c { int n; };\nfloat4 main() : SV_Target { float a[n]; return 1; }" },
};

TEST(d3dcompiler_compile_errors_types_and_conversions)
{
    WIN_ONLY();
    NEED();
    cc_rows_run(cc_types, N(cc_types), "types");
}

/* ---------------- functions and intrinsics ---------------- */
static const cc_row cc_funcs[] = {
    { 1, 'e', 3013, 2, "ps_5_0", 0, FR("    float f = sin();") },
    { 1, 'e', 3013, 2, "ps_5_0", 0, FR("    float f = dot(1);") },
    { 1, 'e', 3013, 2, "ps_5_0", 0, FR("    float4 f = lerp(p, p);") },
    { 1, 'e', 3013, 2, "ps_5_0", 0, FR("    float4 f = saturate(p, p);") },
    { 1, 'e', 3013, 2, "ps_5_0", 0, FR("    float f = pow(p.x);") },
    { 1, 'e', 3013, 2, "ps_5_0", 0, FR("    float4 f = mul(p);") },
    { 1, 'e', 3013, 2, "ps_5_0", 0, FR("    float f = length();") },
    { 1, 'e', 3013, 2, "ps_5_0", 0, FR("    float f = clamp(1, 2);") },
    { 1, 'e', 3013, 2, "ps_5_0", 0, "float f(float a) { return a; }\nfloat4 main() : SV_Target { return f(1, 2); }" },
    { 1, 'e', 3013, 2, "ps_5_0", 0, "float f(float a, float b) { return a; }\nfloat4 main() : SV_Target { return f(1); }" },
    { 1, 'e', 3013, 2, "ps_5_0", 0, "struct S { float a; };\nfloat4 main() : SV_Target { S s; return sin(s); }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return normalize(); }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return cross(float2(1,2), float2(3,4)).xyxy; }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return float4(reflect(float3(1,0,0)), 1); }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return step(1); }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return float4(smoothstep(0, 1), 0, 0, 1); }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return float4(distance(1), 0, 0, 1); }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return float4(frac(), 0, 0, 1); }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return float4(min(1), 0, 0, 1); }" },
    { 1, 'e', 3013, 1, "ps_5_0", 0, "float4 main() : SV_Target { return float4(abs(1, 2), 0, 0, 1); }" },
    { 1, 'e', 3003, 2, "ps_5_0", 0, "float f(float a) { return a; }\nfloat f(float a) { return a; }\nfloat4 main() : SV_Target { return f(1); }" }, /* Win10: was e 3067 */
    { 1, 'e', 3510, 2, "ps_5_0", 0, "float f(float a);\nfloat4 main() : SV_Target { return f(1); }" }, /* Win10: was e 3074 */
    { 1, 'e', 3000, 2, "ps_5_0", 0, "float f(float a) { return a; }\nfloat4 main() : SV_Target { return f(1, ); }" }, /* Win10: was e 3080 */
    { 1, 'e', 3507, 1, "ps_5_0", 0, "float4 main() : SV_Target { }" }, /* Win10: was e 3075 */
    { 1, 'e', 3025, 2, "ps_5_0", 0, "float f(inout float a) { a = 1; return a; }\nfloat4 main() : SV_Target { return f(2); }" }, /* Win10: was e 3079 */
    { 1, 'e', 3017, 2, "ps_5_0", 0, "void f() { }\nfloat4 main() : SV_Target { return f(); }" }, /* Win10: was e 3084 */
    { 1, 'e', 3079, 1, "ps_5_0", 0, "void f() { return 1; }\nfloat4 main() : SV_Target { f(); return 1; }" }, /* Win10: was e 3086 */
    { 1, 'e', 3017, 1, "ps_5_0", 0, "float4 main() : SV_Target { return float4(1,2,3,4) * float3(1,2,3); }" }, /* Win10: was e 3116 */
    { 1, 'e', 3017, 1, "ps_5_0", 0, "float4 main() : SV_Target { return mul(float3x3(1,2,3,4,5,6,7,8,9), float4(1,2,3,4)); }" }, /* Win10: was e 3037 */
    { 1, 'e', 3013, 2, "ps_5_0", 0, "Texture2D t; SamplerState s;\nfloat4 main() : SV_Target { return t.Sample(s); }" },
    { 1, 'e', 3013, 2, "ps_5_0", 0, "Texture2D t; SamplerState s;\nfloat4 main() : SV_Target { return t.SampleLevel(s, float2(0,0)); }" },
    { 1, 'e', 3088, 2, "ps_5_0", 0, "Texture2D t; SamplerState s;\nfloat4 main() : SV_Target { return t.NoSuchMethod(s, 0); }" }, /* Win10: was e 3039 */
    { 1, 'e', 3087, 1, "ps_5_0", 0, "float4 main() : SV_Target { float4 a = 1; return a.Sample(a, a); }" }, /* Win10: was e 3040 */
};

TEST(d3dcompiler_compile_errors_functions_and_intrinsics)
{
    WIN_ONLY();
    NEED();
    cc_rows_run(cc_funcs, N(cc_funcs), "funcs");
}

/* ---------------- entry points, semantics and profiles ---------------- */
static const cc_row cc_entry[] = {
    { 1, 'e', 3501, 0, "ps_5_0", 0, "float4 notmain() : SV_Target { return 1; }" },
    { 1, 'e', 3501, 0, "vs_5_0", 0, "float4 other(float4 p : POSITION) : SV_Position { return p; }" },
    { 1, 'e', 3501, 0, "cs_5_0", 0, "[numthreads(1,1,1)] void other() { }" },
    { 1, 'e', 3501, 0, "ps_5_0", 0, "" },
    { 1, 'e', 3501, 0, "ps_4_0", 0, "// only a comment\n" },
    { 1, 'e', 3501, 0, "ps_5_0", 0, "float main_;\nfloat4 Main() : SV_Target { return 1; }" },
    { 1, 'e', 3501, 0, "ps_5_0", 0, "struct main { float a; };\nfloat4 other() : SV_Target { return 1; }" },
    { 1, 'e', 4502, 1, "vs_5_0", 0, "float4 main() : SV_Target { return 1; }" },
    { 1, 'e', 4555, 1, "ps_5_0", 0, "float4 main() : SV_Depth { return 1; }" }, /* Win10: was e 4502 */
    { 1, 'e', 3503, 1, "ps_5_0", 0, "float4 main() { return 1; }" }, /* Win10: was e 3530 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "float4 main(float4 a : SV_Position, float4 b : SV_Position) : SV_Target { return a + b; }" }, /* Win10: was e 3076 */
    { 1, 'e', 4530, 0, "ps_3_0", 0, "void main(out float4 c : COLOR0) { }" }, /* Win10: was e 3502 */
    { 1, 'e', 3017, 2, "ps_3_0", 0, "cbuffer cb { int idx; };\nfloat4 main() : COLOR0 { float4 a[4] = { 1,2,3,4 }; a[idx] = 5; return a[0]; }" }, /* Win10: was e 3550 */
    { 1, 'n', 0, 0, "ps_3_0", 0, "float4 main() : SV_Target { return 1; }" }, /* Win10: was e 4502 */
    { 1, 'e', 1500, 2, "ps_5_0", 0, "float4 main(float4 a : SV_Position) : SV_Target { return a; }\n#if\n#endif" }, /* Win10: was e 3002 */
};

TEST(d3dcompiler_compile_errors_entry_and_semantics)
{
    WIN_ONLY();
    NEED();
    cc_rows_run(cc_entry, N(cc_entry), "entry");
}

/* ---------------- warnings with successful compile ---------------- */
static const cc_row cc_warn[] = {
    { 1, 'w', 3578, 1, "ps_5_0", 0, FRV("    float4 c; return c;") }, /* Win10: was w 4000 */
    { 1, 'e', 4000, 3, "ps_5_0", 0, "float4 main(float4 p : SV_Position) : SV_Target {\n    float4 c;\n    return c + 1;\n}\n" }, /* Win10: was w 4000 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "cbuffer cb { float k; };\nfloat4 main(float4 p : SV_Position) : SV_Target {\n    float4 c; if (k > 1) c = 1;\n    return c;\n}\n" }, /* Win10: was w 4000 */
    { 1, 'w', 3556, 3, "ps_5_0", 0, "cbuffer cb { int x; int y; };\nfloat4 main(float4 p : SV_Position) : SV_Target {\n    int q = x / y;\n    return q;\n}\n" },
    { 1, 'w', 3568, 1, "ps_5_0", 0, "#pragma no_such_pragma\nfloat4 main() : SV_Target { return 1; }" },
    { 1, 'w', 3571, 2, "ps_5_0", 0, "float4 main(float4 p : SV_Position) : SV_Target {\n    return pow(p, p);\n}\n" },
    { 1, 'w', 3206, 1, "ps_5_0", 0, "float4 main(float4 p : SV_Position) : SV_Target { float2 q = p; return float4(q, 0, 1); }" },
    { 1, 'w', 3078, 1, "ps_5_0", 0, "float4 main() : SV_Target { int i = 0; for (int i = 0; i < 2; i++) { } return 1; }" },
    { 1, 'w', 3578, 1, "ps_5_0", 0, "void main(out float4 c : SV_Target) { }" },
    { 1, 'e', 4580, 1, "vs_5_0", 0, "float4 main() : SV_Position { float4 p; return p; }" }, /* Win10: was w 3578 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "float4 main() : SV_Target { float4 a = 0; [unroll] for (int i = 0; i < 0; i++) a += 1; return a; }" }, /* Win10: was w 3557 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "Texture2D t; SamplerState s; cbuffer c { int n; };\nfloat4 main(float2 uv : TEXCOORD0) : SV_Target { float4 a = 0; for (int i = 0; i < n; i++) a += t.Sample(s, uv); return a; }" }, /* Win10: was w 3570 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "float4 main() : SV_Target { return 1; }" }, /* Win10: was w 3550 */
    { 1, 'n', 0, 0, "ps_5_0", D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, "float4 main() : SV_Target { return 1; }" }, /* Win10: was w 4717 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "float4 main(float4 p : SV_Position) : SV_Target { float2x2 m = float2x2(p); return 1; }" }, /* Win10: was w 3207 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "float4 main() : SV_Target { float f = 1; uint u = f; return u; }" }, /* Win10: was w 3203 */
    { 1, 'w', 3081, 1, "ps_5_0", 0, "float4 main() : SV_Target { float4 a = (1, 2, 3, 4); return a; }" },
    { 1, 'n', 0, 0, "ps_5_0", 0, "float4 main() : SV_Target { float a = 1; a; return a; }" }, /* Win10: was w 3084 */
};

TEST(d3dcompiler_compile_errors_warnings_do_not_fail)
{
    WIN_ONLY();
    NEED();
    cc_rows_run(cc_warn, N(cc_warn), "warn");
}

/* ---------------- flow control, unrolling and resources ---------------- */
static const cc_row cc_flow[] = {
    { 1, 'e', 3511, 2, "ps_5_0", 0, "cbuffer cb { int n; };\nfloat4 main() : SV_Target { float4 a = 0; [unroll] for (int i = 0; i < n; i++) a += 1; return a; }" },
    { 1, 'n', 0, 0, "ps_5_0", 0, "cbuffer cb { int n; };\nfloat4 main() : SV_Target { float4 a = 0; [unroll(8)] for (int i = 0; i < n; i++) a += 1; return a; }" }, /* Win10: was e 3511 */
    { 1, 'e', 3511, 1, "ps_5_0", 0, "float4 main() : SV_Target { float4 a = 0; [unroll] while (true) { a += 1; } return a; }" },
    { 1, 'e', 3524, 2, "ps_5_0", 0, "cbuffer cb { int n; };\nfloat4 main() : SV_Target { float4 a = 0; [unroll][loop] for (int i = 0; i < 4; i++) a += 1; return a; }" }, /* Win10: was e 3531 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "Texture2D t; SamplerState s; cbuffer c { float k; };\nfloat4 main(float2 uv : TEXCOORD0) : SV_Target {\n [branch] if (k > 0) return t.Sample(s, uv);\n return 0; }" }, /* Win10: was e 4014 */
    { 1, 'e', 3512, 2, "ps_5_0", 0, "Texture2D t[2]; SamplerState s; cbuffer c { int i; };\nfloat4 main() : SV_Target { return t[i].SampleLevel(s, 0, 0); }" },
    { 1, 'e', 3518, 1, "ps_5_0", 0, "float4 main() : SV_Target { break; return 1; }" }, /* Win10: was e 3079 */
    { 1, 'e', 3519, 1, "ps_5_0", 0, "float4 main() : SV_Target { continue; return 1; }" }, /* Win10: was e 3092 */
    { 1, 'e', 3507, 1, "ps_5_0", 0, "float4 main() : SV_Target { discard; }" }, /* Win10: was e 3095 */
    { 1, 'e', 3000, 1, "ps_5_0", 0, "float4 main() : SV_Target { goto L; return 1; }" },
    { 1, 'e', 4500, 1, "ps_5_0", 0, "Texture2D a : register(t0); Texture2D b : register(t0);\nfloat4 main() : SV_Target { return a.Load(0) + b.Load(0); }" },
    { 1, 'e', 4509, 1, "ps_5_0", 0, "Texture2D a : register(t200);\nfloat4 main() : SV_Target { return a.Load(0); }" }, /* Win10: was e 3057 */
    { 1, 'e', 4509, 1, "ps_5_0", 0, "SamplerState s : register(s20);\nTexture2D t;\nfloat4 main() : SV_Target { return t.Sample(s, 0); }" }, /* Win10: was e 3036 */
    { 1, 'e', 4567, 0, "ps_5_0", 0, "cbuffer c : register(b99) { float4 v; };\nfloat4 main() : SV_Target { return v; }" }, /* Win10: was e 3050 */
    { 1, 'e', 3530, 1, "ps_5_0", 0, "Texture2D t : register(u0);\nfloat4 main() : SV_Target { return t.Load(0); }" }, /* Win10: was e 3050 */
    { 1, 'e', 3004, 1, "ps_5_0", 0, "float4 main() : SV_Target { return float4(1,2,3,4).xyzw + SV_Position; }" },
    { 1, 'e', 4509, 1, "ps_5_0", 0, "RWTexture2D<float4> u : register(u0);\nfloat4 main() : SV_Target { u[uint2(0,0)] = 1; return 1; }" }, /* Win10: was e 3033 */
};

TEST(d3dcompiler_compile_errors_flow_control_and_resources)
{
    WIN_ONLY();
    NEED();
    cc_rows_run(cc_flow, N(cc_flow), "flow");
}

/* ---------------- preprocessor ---------------- */
static const cc_row cc_pp[] = {
    { 1, 'e', 1505, 1, "ps_5_0", 0, "#include \"does_not_exist.hlsli\"\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 1507 */
    { 1, 'e', 1505, 1, "ps_5_0", 0, "#include <does_not_exist.h>\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 1507 */
    { 1, 'e', 1500, 1, "ps_5_0", 0, "#if\n#endif\nfloat4 main() : SV_Target { return 1; }" },
    { 1, 'e', 1509, 1, "ps_5_0", 0, "#else\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 1510 */
    { 1, 'e', 1510, 1, "ps_5_0", 0, "#endif\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 1511 */
    { 1, 'e', 1502, 2, "ps_5_0", 0, "#if 1\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 1500 */
    { 1, 'e', 1500, 1, "ps_5_0", 0, "#define\nfloat4 main() : SV_Target { return 1; }" },
    { 1, 'e', 1500, 1, "ps_5_0", 0, "#undef\nfloat4 main() : SV_Target { return 1; }" },
    { 1, 'e', 1504, 1, "ps_5_0", 0, "#bogus\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 1500 */
    { 1, 'e', 3080, 2, "ps_5_0", 0, "#define F(x) x\nfloat4 main() : SV_Target { return F(; }" }, /* Win10: was e 1500 */
    { 1, 'e', 1516, 2, "ps_5_0", 0, "#define F(x, y) x\nfloat4 main() : SV_Target { return F(1); }" }, /* Win10: was e 1500 */
    { 1, 'e', 0, 0, "ps_5_0", 0, "#error user message\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 1500 */
    { 1, 'e', 1500, 1, "ps_5_0", 0, "#line\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 3000 */
    { 1, 'w', 3568, 1, "ps_5_0", 0, "#pragma once_not_real\nfloat4 main() : SV_Target { return 1; }" },
    { 1, 'w', 1519, 2, "ps_5_0", 0, "#define A A\n#define A B\nfloat4 main() : SV_Target { return 1; }" }, /* Win10: was e 1500 */
    { 1, 'e', 1500, 1, "ps_5_0", 0, "#if 1 +\n#endif\nfloat4 main() : SV_Target { return 1; }" },
};

TEST(d3dcompiler_compile_errors_preprocessor)
{
    WIN_ONLY();
    NEED();
    cc_rows_run(cc_pp, N(cc_pp), "pp");
}

/* ---------------- compute / geometry / tessellation specific ---------------- */
static const cc_row cc_stage[] = {
    { 1, 'e', 3503, 1, "cs_5_0", 0, "[numthreads(1,1,1)] float4 main() { return 1; }" }, /* Win10: was e 3000 */
    { 1, 'e', 3655, 1, "cs_5_0", 0, "[numthreads(0,1,1)] void main() { }" }, /* Win10: was e 3000 */
    { 1, 'e', 3084, 1, "cs_5_0", 0, "[numthreads(1,1)] void main() { }" }, /* Win10: was e 3000 */
    { 1, 'e', 4502, 1, "cs_5_0", 0, "[numthreads(1,1,1)] void main(uint3 i : SV_Position) { }" }, /* Win10: was e 3000 */
    { 1, 'e', 3514, 1, "gs_5_0", 0, "[maxvertexcount(0)] void main(point float4 p[1] : SV_Position, inout PointStream<float4> s) { }" }, /* Win10: was e 3000 */
    { 1, 'e', 8000, 0, "gs_5_0", 0, "[maxvertexcount(2000)] void main(point float4 p[1] : SV_Position, inout PointStream<float4> s) { }" }, /* Win10: was e 3000 */
    { 1, 'w', 3554, 1, "vs_5_0", 0, "[numthreads(1,1,1)] float4 main() : SV_Position { return 1; }" }, /* Win10: was e 3000 */
    { 1, 'n', 0, 0, "ps_5_0", 0, "[earlydepthstencil] float4 main() : SV_Target { return 1; }" }, /* Win10: was e 3000 */
    { 1, 'e', 3013, 1, "ps_5_0", 0, "Texture2D t; float4 main() : SV_Target { return t.Load(int3(0,0,0)) + t.GatherRed(0, 0); }" }, /* Win10: was e 3000 */
    { 1, 'e', 3084, 1, "hs_5_0", 0, "[domain(\"tri\")] void main() { }" }, /* Win10: was e 3000 */
    { 1, 'e', 3084, 0, "ds_5_0", 0, "float4 main() : SV_Position { return 1; }" }, /* Win10: was e 3000 */
    { 1, 'n', 0, 0, "cs_5_0", 0, "groupshared float g[40000];\n[numthreads(1,1,1)] void main() { g[0] = 1; }" }, /* Win10: was e 3000 */
    { 1, 'e', 3681, 2, "cs_5_0", 0, "RWStructuredBuffer<float> b;\n[numthreads(1,1,1)] void main() { InterlockedAdd(b[0], 1); }" }, /* Win10: was e 3000 */
};

TEST(d3dcompiler_compile_errors_stage_specific_notes)
{
    NEED();
    if (t_is_wine()) { t_skip("stage-specific front ends"); return; }
    cc_rows_run(cc_stage, N(cc_stage), "stage");
}

/* ---------------- overall properties ---------------- */
TEST(d3dcompiler_compile_errors_all_confident_rows_total)
{
    NEED();
    t_note("diagnostic rows: syntax %u names %u types %u funcs %u entry %u warn %u flow %u pp %u stage %u",
           (unsigned)N(cc_syntax), (unsigned)N(cc_names), (unsigned)N(cc_types), (unsigned)N(cc_funcs), (unsigned)N(cc_entry),
           (unsigned)N(cc_warn), (unsigned)N(cc_flow), (unsigned)N(cc_pp), (unsigned)N(cc_stage));
}

TEST(d3dcompiler_compile_errors_only_first_error_reported)
{
    WIN_ONLY();
    static const char *src =
        "float4 main() : SV_Target {\n"
        "    float a = u1;\n"
        "    float b = u2;\n"
        "    float c = u3;\n"
        "    return a + b + c;\n"
        "}\n";
    cc_r r;
    int n = 0;
    const char *p;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    for (p = r.err; (p = strstr(p, "error X3004")) != NULL; p++) n++;
    /* Win10 reports only the first of several independent undeclared identifiers (compilation stops at the first semantic error) */
    EXPECT_EQ(n, 1);
    CHECK(strstr(r.err, "t.hlsl(2,") != NULL);
    CHECK(strstr(r.err, "'u1'") != NULL);
    CHECK(strstr(r.err, "t.hlsl(3,") == NULL);
    CHECK(strstr(r.err, "t.hlsl(4,") == NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_first_in_source_order_wins)
{
    static const char *src =
        "float4 main() : SV_Target {\n"
        "    float a = first_missing;\n"
        "    float b = second_missing;\n"
        "    return a + b;\n"
        "}\n";
    cc_r r;
    const char *a, *b;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    a = strstr(r.err, "first_missing");
    b = strstr(r.err, "second_missing");
    CHECK(a != NULL);
    CHECK(b == NULL); /* Win10: the first error aborts the compile */
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_column_points_at_token)
{
    WIN_ONLY();
    static const char *src = "float4 main() : SV_Target {\n    return unknown_ident;\n}\n";
    cc_r r;
    int line = 0, col = 0;
    const char *p;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    p = strstr(r.err, "t.hlsl(");
    CHECK(p != NULL);
    if (p && sscanf(p, "t.hlsl(%d,%d", &line, &col) == 2) {
        EXPECT_EQ(line, 2);
        EXPECT_EQ(col, 12);     /* 1-based column of 'unknown_ident' */
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_tabs_count_as_one_column)
{
    static const char *src = "float4 main() : SV_Target {\n\treturn unknown_ident;\n}\n";
    cc_r r;
    int line = 0, col = 0;
    const char *p;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    p = strstr(r.err, "t.hlsl(");
    if (p && sscanf(p, "t.hlsl(%d,%d", &line, &col) == 2) {
        EXPECT_EQ(line, 2);
        EXPECT_EQ(col, 9);   /* a tab counts as one column: 1 + len("return ") + 1 */
    }
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_line_directive_remaps_location)
{
    WIN_ONLY();
    static const char *src =
        "#line 100 \"remapped.hlsl\"\n"
        "float4 main() : SV_Target { return unknown_ident; }\n";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(strstr(r.err, "remapped.hlsl(100,") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_line_directive_without_file)
{
    WIN_ONLY();
    static const char *src = "#line 40\nfloat4 main() : SV_Target { return unknown_ident; }\n";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(strstr(r.err, "t.hlsl(40,") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_multiline_source_line_numbers)
{
    WIN_ONLY();
    char src[4096];
    size_t n = 0;
    int i;
    cc_r r;
    NEED();
    for (i = 0; i < 50; i++) n += snprintf(src + n, sizeof src - n, "// filler %d\n", i);
    n += snprintf(src + n, sizeof src - n, "float4 main() : SV_Target { return unknown_ident; }\n");
    cc_compile_full(&r, src, n, "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK(strstr(r.err, "t.hlsl(51,") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_block_comment_line_counting)
{
    WIN_ONLY();
    static const char *src = "/* one\n two\n three */\nfloat4 main() : SV_Target { return unknown_ident; }\n";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK(strstr(r.err, "t.hlsl(4,") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_crlf_line_counting)
{
    WIN_ONLY();
    static const char *src = "float4 main() : SV_Target {\r\n    float a = 1;\r\n    return unknown_ident;\r\n}\r\n";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK(strstr(r.err, "t.hlsl(3,") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_macro_expansion_location)
{
    WIN_ONLY();
    static const char *src =
        "#define BAD unknown_ident\n"
        "float4 main() : SV_Target {\n"
        "    return BAD;\n"
        "}\n";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(cc_has_error(r.err, 3004));
    CHECK(strstr(r.err, "t.hlsl(3,") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_warnings_listed_with_errors)
{
    WIN_ONLY();
    static const char *src =
        "float4 main(float4 p : SV_Position) : SV_Target {\n"
        "    float3 t = p;\n"
        "    return unknown_ident;\n"
        "}\n";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(cc_has_error(r.err, 3004));
    CHECK(cc_has_warning(r.err, 3206)); /* the earlier warning is still reported next to the error */
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_no_code_on_error_even_with_warnings_only_flag)
{
    static const char *src = "float4 main() : SV_Target { return unknown_ident; }";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_SKIP_OPTIMIZATION, 0);
    CHECK((LONG)r.hr < 0);
    CHECK(r.code == NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_error_text_stable_across_calls)
{
    static const char *src = "float4 main() : SV_Target { return unknown_ident; }";
    cc_r a, b;
    NEED();
    cc_compile_full(&a, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    cc_compile_full(&b, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK(!strcmp(a.err, b.err));
    cc_free(&a); cc_free(&b);
}

TEST(d3dcompiler_compile_errors_unicode_in_source_comment)
{
    WIN_ONLY();
    static const char *src = "// caf\xC3\xA9 \xE2\x82\xAC\nfloat4 main() : SV_Target { return unknown_ident; }\n";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK(strstr(r.err, "t.hlsl(2,") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_identifier_quoted_in_message)
{
    WIN_ONLY();
    static const char *src = "float4 main() : SV_Target { return some_long_identifier_name; }";
    cc_r r;
    NEED();
    cc_compile_full(&r, src, strlen(src), "t.hlsl", NULL, NULL, "main", "ps_5_0", 0, 0);
    CHECK(strstr(r.err, "'some_long_identifier_name'") != NULL);
    cc_free(&r);
}

TEST(d3dcompiler_compile_errors_x3501_names_entry)
{
    WIN_ONLY();
    static const char *names[] = { "a", "VeryLongEntryPointName_0123456789", "_x", "main2" };
    unsigned i;
    NEED();
    for (i = 0; i < N(names); i++) {
        cc_r r;
        char want[128];
        cc_run(&r, CC_PS_SRC, names[i], "ps_5_0", 0);
        snprintf(want, sizeof want, "'%s': entrypoint not found", names[i]);
        CHECKF(strstr(r.err, want) != NULL, "name '%s' err '%.120s'", names[i], r.err);
        cc_free(&r);
    }
}
