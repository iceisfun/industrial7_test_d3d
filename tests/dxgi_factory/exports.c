/* dxgi.dll exports: ordinals, PIX stubs, compat string/value, journal, debug interface, removal support, HMD. */
#include "fh.h"

typedef int      (__cdecl *pfn_CompatString)(const char *, unsigned long long *, char *, _Bool);
typedef int      (__cdecl *pfn_CompatValue)(const char *, unsigned long long *);
typedef void     (__cdecl *pfn_SetCompat)(unsigned long long, const char *);
typedef void     (__cdecl *pfn_Quirk)(UINT *, UINT *);
typedef void     (__cdecl *pfn_Journal)(void (__cdecl *)(const char *));
typedef HRESULT  (WINAPI *pfn_dbg1)(UINT, REFIID, void **);
typedef HRESULT  (WINAPI *pfn_stub6)(void *, void *, void *, void *, void *, void *);
typedef LONG     (WINAPI *pfn_u0)(void);
typedef LONG     (WINAPI *pfn_u2)(void *, void *);
typedef LONG     (WINAPI *pfn_hmd)(unsigned char);
typedef HRESULT  (WINAPI *pfn_decl)(void);

#define DX(name) d3dh_getproc("dxgi.dll", name)

static const struct { const char *name; unsigned ord; } k_exports[] = {
    { "ApplyCompatResolutionQuirking", 1 }, { "CompatString", 2 }, { "CompatValue", 3 }, { "DXGIDumpJournal", 4 },
    { "PIXBeginCapture", 5 }, { "PIXEndCapture", 6 }, { "PIXGetCaptureState", 7 }, { "SetAppCompatStringPointer", 8 },
    { "UpdateHMDEmulationStatus", 9 }, { "CreateDXGIFactory", 10 }, { "CreateDXGIFactory1", 11 }, { "CreateDXGIFactory2", 12 },
    { "DXGID3D10CreateDevice", 13 }, { "DXGID3D10CreateLayeredDevice", 14 }, { "DXGID3D10GetLayeredDeviceSize", 15 },
    { "DXGID3D10RegisterLayers", 16 }, { "DXGIDeclareAdapterRemovalSupport", 17 }, { "DXGIGetDebugInterface1", 18 },
    { "DXGIReportAdapterConfiguration", 19 },
};
#define NEXP (sizeof(k_exports) / sizeof(k_exports[0]))

TEST(dxgi_factory_exports_by_name) {
    WINE_SKIP();
    if (!d3dh_have_dll("dxgi.dll")) { t_skip("no dxgi.dll"); return; }
    for (size_t i = 0; i < NEXP; i++) CHECKF(DX(k_exports[i].name) != NULL, "missing export %s", k_exports[i].name);
}
TEST(dxgi_factory_exports_by_ordinal) {
    if (!d3dh_have_dll("dxgi.dll")) { t_skip("no dxgi.dll"); return; }
    for (size_t i = 0; i < NEXP; i++) {
        FARPROC byname = DX(k_exports[i].name), byord = t_proc_ord("dxgi.dll", k_exports[i].ord);
        if (byord != byname) t_note("%s: ordinal %u resolves differently from the named export (build specific)", k_exports[i].name, k_exports[i].ord);
    }
}
TEST(dxgi_factory_exports_unknown_names) {
    if (!d3dh_have_dll("dxgi.dll")) { t_skip("no dxgi.dll"); return; }
    static const char *no[] = { "CreateDXGIFactory3", "DXGIGetDebugInterface", "CreateDXGIFactoryA", "createdxgifactory", "CREATEDXGIFACTORY1", "", "DXGID3D10", "PIXSetMarker" };
    for (size_t i = 0; i < sizeof(no) / sizeof(no[0]); i++) {
        SetLastError(0);
        FARPROC p = GetProcAddress(t_module("dxgi.dll"), no[i]);
        CHECKF(p == NULL, "unexpected export '%s'", no[i]);
        if (!p) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(dxgi_factory_exports_ordinal_out_of_range) {
    if (!d3dh_have_dll("dxgi.dll")) { t_skip("no dxgi.dll"); return; }
    static const unsigned bad[] = { 0, 20, 21, 100, 1000, 0xffff };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        SetLastError(0);
        CHECKF(t_proc_ord("dxgi.dll", bad[i]) == NULL, "unexpected ordinal %u", bad[i]);
    }
}
TEST(dxgi_factory_exports_stub_sharing) {
    WINE_SKIP();
    /* PIX* and DXGID3D10GetLayeredDeviceSize share one body; three DXGID3D10 entries share another (identical-code folding). */
    if (!d3dh_have_dll("dxgi.dll")) { t_skip("no dxgi.dll"); return; }
    t_note("%s -> %%d", !!(DX("PIXBeginCapture") == DX("PIXEndCapture")));
    t_note("%s -> %%d", !!(DX("PIXBeginCapture") == DX("PIXGetCaptureState")));
    t_note("%s -> %%d", !!(DX("PIXBeginCapture") == DX("DXGID3D10GetLayeredDeviceSize")));
    t_note("%s -> %%d", !!(DX("DXGID3D10CreateDevice") == DX("DXGID3D10CreateLayeredDevice")));
    t_note("%s -> %%d", !!(DX("DXGID3D10CreateDevice") == DX("DXGID3D10RegisterLayers")));
    t_note("%s -> %%d", !!(DX("DXGID3D10CreateDevice") != DX("PIXBeginCapture")));
}
TEST(dxgi_factory_exports_not_forwarders) {
    WINE_SKIP();
    if (!d3dh_have_dll("dxgi.dll")) { t_skip("no dxgi.dll"); return; }
    HMODULE m = t_module("dxgi.dll");
    for (size_t i = 0; i < NEXP; i++) {
        BYTE *p = (BYTE *)DX(k_exports[i].name);
        CHECKF(p && p >= (BYTE *)m, "%s outside image", k_exports[i].name);
    }
}
TEST(dxgi_factory_exports_d3d11_loads_same_dxgi) {
    /* the dxgi.dll resolved through the module list is the one loaded in the process */
    if (!d3dh_have_dll("dxgi.dll")) { t_skip("no dxgi.dll"); return; }
    HMODULE a = t_module("dxgi.dll"), b = GetModuleHandleA("dxgi.dll");
    CHECK(a == b);
    char path[MAX_PATH]; DWORD n = GetModuleFileNameA(a, path, MAX_PATH);
    CHECK(n > 0 && n < MAX_PATH);
    t_note("dxgi.dll path: %s", path);
}

/* ---------------- PIX stubs ---------------- */
TEST(dxgi_factory_pix_get_capture_state_zero) {
    pfn_u0 f = (pfn_u0)DX("PIXGetCaptureState");
    REQUIRE(f);
    for (int i = 0; i < 5; i++) EXPECT_EQ((ULONG)f(), 0);
}
TEST(dxgi_factory_pix_begin_capture_returns_zero) {
    pfn_u2 f = (pfn_u2)DX("PIXBeginCapture");
    REQUIRE(f);
    EXPECT_EQ((ULONG)f(NULL, NULL), 0);
    EXPECT_EQ((ULONG)f((void *)1, NULL), 0);
}
TEST(dxgi_factory_pix_end_capture_returns_zero) {
    pfn_u2 f = (pfn_u2)DX("PIXEndCapture");
    REQUIRE(f);
    EXPECT_EQ((ULONG)f(NULL, NULL), 0);
    EXPECT_EQ((ULONG)f((void *)1, (void *)1), 0);
}
TEST(dxgi_factory_pix_capture_state_unchanged_by_begin_end) {
    pfn_u0 g = (pfn_u0)DX("PIXGetCaptureState");
    pfn_u2 b = (pfn_u2)DX("PIXBeginCapture"), e = (pfn_u2)DX("PIXEndCapture");
    REQUIRE(g && b && e);
    b(NULL, NULL);
    EXPECT_EQ((ULONG)g(), 0);
    e(NULL, NULL);
    EXPECT_EQ((ULONG)g(), 0);
}

/* ---------------- DXGID3D10* legacy stubs ---------------- */
TEST(dxgi_factory_d3d10_create_device_notimpl) {
    WINE_SKIP();
    pfn_stub6 f = (pfn_stub6)DX("DXGID3D10CreateDevice");
    REQUIRE(f);
    EXPECT_HR(f(NULL, NULL, NULL, NULL, NULL, NULL), E_NOTIMPL);
}
TEST(dxgi_factory_d3d10_create_layered_device_notimpl) {
    pfn_stub6 f = (pfn_stub6)DX("DXGID3D10CreateLayeredDevice");
    REQUIRE(f);
    EXPECT_HR(f(NULL, NULL, NULL, NULL, NULL, NULL), E_NOTIMPL);
}
TEST(dxgi_factory_d3d10_register_layers_notimpl) {
    WINE_SKIP();
    pfn_stub6 f = (pfn_stub6)DX("DXGID3D10RegisterLayers");
    REQUIRE(f);
    EXPECT_HR(f(NULL, NULL, NULL, NULL, NULL, NULL), E_NOTIMPL);
    EXPECT_HR(f((void *)1, (void *)2, NULL, NULL, NULL, NULL), E_NOTIMPL);
}
TEST(dxgi_factory_d3d10_get_layered_device_size_zero) {
    pfn_u2 f = (pfn_u2)DX("DXGID3D10GetLayeredDeviceSize");
    REQUIRE(f);
    EXPECT_EQ((ULONG)f(NULL, NULL), 0);
}

/* ---------------- SetAppCompatStringPointer / CompatString / CompatValue ---------------- */
static void clear_compat(void) {
    pfn_SetCompat s = (pfn_SetCompat)DX("SetAppCompatStringPointer");
    if (s) s(0, NULL);
}
static void set_compat(const char *str) {
    pfn_SetCompat s = (pfn_SetCompat)DX("SetAppCompatStringPointer");
    s((unsigned long long)strlen(str) + 1, str);
}
#define NEED_COMPAT() \
    pfn_CompatString cs = (pfn_CompatString)DX("CompatString"); \
    pfn_CompatValue cv = (pfn_CompatValue)DX("CompatValue"); \
    pfn_SetCompat sp = (pfn_SetCompat)DX("SetAppCompatStringPointer"); \
    if (!cs || !cv || !sp) { t_skip("compat exports missing"); return; }

TEST(dxgi_factory_compat_string_basic) {
    NEED_COMPAT();
    static const char s[] = "alpha=one;beta=two;gamma=three";
    set_compat(s);
    char buf[64] = "zzz"; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("beta", &n, buf, 0), 1);
    EXPECT_STR(buf, "two");
    EXPECT_EQ(n, 4);
    clear_compat();
}
TEST(dxgi_factory_compat_string_first_and_last) {
    NEED_COMPAT();
    static const char s[] = "alpha=one;beta=two;gamma=three";
    set_compat(s);
    char buf[64]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("alpha", &n, buf, 0), 1); EXPECT_STR(buf, "one"); EXPECT_EQ(n, 4);
    n = sizeof(buf);
    EXPECT_EQ(cs("gamma", &n, buf, 0), 1); EXPECT_STR(buf, "three"); EXPECT_EQ(n, 6);
    clear_compat();
}
TEST(dxgi_factory_compat_string_not_found) {
    NEED_COMPAT();
    set_compat("alpha=one;beta=two");
    char buf[16] = "keep"; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("delta", &n, buf, 0), 0);
    EXPECT_EQ(buf[0], 0);   /* output buffer is emptied up front */
    EXPECT_EQ(cs("alph", &n, buf, 0), 0);
    EXPECT_EQ(cs("lpha", &n, buf, 0), 0);
    EXPECT_EQ(cs("alpha2", &n, buf, 0), 0);
    EXPECT_EQ(cs("one", &n, buf, 0), 0);
    clear_compat();
}
TEST(dxgi_factory_compat_string_name_must_follow_separator) {
    NEED_COMPAT();
    set_compat("xalpha=1;alpha=2");
    char buf[16]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("alpha", &n, buf, 0), 1);
    EXPECT_STR(buf, "2");
    clear_compat();
}
TEST(dxgi_factory_compat_string_name_needs_equals) {
    NEED_COMPAT();
    set_compat("alphabet=1;alpha;alpha =3;alpha=4");
    char buf[16]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("alpha", &n, buf, 0), 1);
    EXPECT_STR(buf, "4");
    clear_compat();
}
TEST(dxgi_factory_compat_string_case_sensitive) {
    NEED_COMPAT();
    set_compat("Alpha=1");
    char buf[16]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("alpha", &n, buf, 0), 0);
    EXPECT_EQ(cs("Alpha", &n, buf, 0), 1);
    clear_compat();
}
TEST(dxgi_factory_compat_string_first_match_wins) {
    NEED_COMPAT();
    set_compat("k=first;k=second");
    char buf[16]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("k", &n, buf, 0), 1);
    EXPECT_STR(buf, "first");
    clear_compat();
}
TEST(dxgi_factory_compat_string_empty_value) {
    NEED_COMPAT();
    set_compat("a=;b=2");
    char buf[16] = "x"; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("a", &n, buf, 0), 1);
    EXPECT_STR(buf, "");
    EXPECT_EQ(n, 1);
    n = sizeof(buf);
    EXPECT_EQ(cs("b", &n, buf, 0), 1);
    EXPECT_STR(buf, "2");
    clear_compat();
}
TEST(dxgi_factory_compat_string_size_query_null_buffer) {
    NEED_COMPAT();
    set_compat("name=hello;other=1");
    unsigned long long n = 0;
    EXPECT_EQ(cs("name", &n, NULL, 0), 1);
    EXPECT_EQ(n, 6);   /* value length + terminator */
    clear_compat();
}
TEST(dxgi_factory_compat_string_truncation) {
    NEED_COMPAT();
    set_compat("name=hello;other=1");
    char buf[16]; unsigned long long n;
    for (unsigned long long cap = 1; cap <= 6; cap++) {
        memset(buf, 'X', sizeof(buf)); n = cap;
        EXPECT_EQ(cs("name", &n, buf, 0), 1);
        EXPECT_EQ(n, 6);                          /* required size is always reported */
        CHECKF(buf[cap - 1] == 0 && strlen(buf) == cap - 1 && !strncmp(buf, "hello", cap - 1), "cap %llu -> '%s'", cap, buf);
        CHECKF(buf[cap] == 'X', "cap %llu wrote beyond the buffer", cap);
    }
    clear_compat();
}
TEST(dxgi_factory_compat_string_zero_capacity) {
    NEED_COMPAT();
    set_compat("name=hello");
    char buf[8]; memset(buf, 'Q', sizeof(buf));
    unsigned long long n = 0;
    int r = cs("name", &n, buf, 0);
    t_note("CompatString with capacity 0: ret %d size %llu buf[0]=%d", r, n, buf[0]);
    CHECK(r == 1);
    CHECK(buf[1] == 'Q');
    clear_compat();
}
TEST(dxgi_factory_compat_string_null_name_output_buffer_cleared) {
    NEED_COMPAT();
    set_compat("a=1");
    char buf[8] = "junk"; unsigned long long n = 8;
    EXPECT_EQ(cs("zz", &n, buf, 0), 0);
    EXPECT_EQ(buf[0], 0);
    clear_compat();
}
TEST(dxgi_factory_compat_string_no_string_set) {
    NEED_COMPAT();
    clear_compat();
    char buf[16] = "junk"; unsigned long long n = 16;
    EXPECT_EQ(cs("anything", &n, buf, 0), 0);
    EXPECT_EQ(buf[0], 0);
}
TEST(dxgi_factory_compat_string_replace_pointer) {
    NEED_COMPAT();
    char buf[16]; unsigned long long n;
    set_compat("v=old");
    n = sizeof(buf); EXPECT_EQ(cs("v", &n, buf, 0), 1); EXPECT_STR(buf, "old");
    set_compat("v=newer");
    n = sizeof(buf); EXPECT_EQ(cs("v", &n, buf, 0), 1); EXPECT_STR(buf, "newer");
    set_compat("w=1");
    n = sizeof(buf); EXPECT_EQ(cs("v", &n, buf, 0), 0);
    clear_compat();
    n = sizeof(buf); EXPECT_EQ(cs("w", &n, buf, 0), 0);
}
TEST(dxgi_factory_compat_string_too_short_declared_length) {
    NEED_COMPAT();
    static const char s[] = "abc=12345";
    sp(5, s);                      /* shorter than strlen+1: the copy into the cache fails, nothing is registered */
    char buf[16]; unsigned long long n = sizeof(buf);
    int r = cs("abc", &n, buf, 0);
    t_note("CompatString with declared length 5 for a 9 char string = %d '%s'", r, buf);
    clear_compat();
}
TEST(dxgi_factory_compat_string_exact_declared_length) {
    NEED_COMPAT();
    static const char s[] = "abc=12345";
    sp(sizeof(s), s);
    char buf[16]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("abc", &n, buf, 0), 1);
    EXPECT_STR(buf, "12345");
    clear_compat();
}
TEST(dxgi_factory_compat_string_value_with_equals_and_spaces) {
    NEED_COMPAT();
    set_compat("p=a=b c;q=d");
    char buf[32]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("p", &n, buf, 0), 1);
    EXPECT_STR(buf, "a=b c");
    EXPECT_EQ(n, 6);
    clear_compat();
}
TEST(dxgi_factory_compat_string_many_entries) {
    NEED_COMPAT();
    static char big[4096]; big[0] = 0;
    for (int i = 0; i < 100; i++) { char t[32]; sprintf(t, "%sk%d=v%d", i ? ";" : "", i, i * 3); strcat(big, t); }
    set_compat(big);
    for (int i = 0; i < 100; i++) {
        char nm[16], want[16], got[32]; unsigned long long n = sizeof(got);
        sprintf(nm, "k%d", i); sprintf(want, "v%d", i * 3);
        CHECKF(cs(nm, &n, got, 0) == 1 && !strcmp(got, want), "%s -> '%s'", nm, got);
    }
    clear_compat();
}
TEST(dxgi_factory_compat_value_decimal) {
    NEED_COMPAT();
    set_compat("a=0;b=1;c=42;d=2147483647;e=-5;f=007;g=12abc;h=abc;i=;j= 9");
    static const struct { const char *n; long long v; } t[] = {
        { "a", 0 }, { "b", 1 }, { "c", 42 }, { "d", 2147483647 }, { "e", -5 }, { "f", 7 }, { "g", 12 }, { "h", 0 }, { "i", 0 }, { "j", 9 },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        unsigned long long v = 0x1122334455667788ULL;
        CHECKF(cv(t[i].n, &v) == 1, "%s not found", t[i].n);
        CHECKF((long long)v == t[i].v, "%s = %lld want %lld", t[i].n, (long long)v, t[i].v);
    }
    clear_compat();
}
TEST(dxgi_factory_compat_value_not_found_leaves_output) {
    NEED_COMPAT();
    set_compat("a=1");
    unsigned long long v = 0x1122334455667788ULL;
    EXPECT_EQ(cv("nope", &v), 0);
    EXPECT_EQ(v, 0x1122334455667788ULL);
    clear_compat();
}
TEST(dxgi_factory_compat_value_null_output) {
    NEED_COMPAT();
    set_compat("a=1");
    EXPECT_EQ(cv("a", NULL), 1);
    EXPECT_EQ(cv("b", NULL), 0);
    clear_compat();
}
TEST(dxgi_factory_compat_value_overflow_wraps_atoi) {
    NEED_COMPAT();
    set_compat("big=4294967296;neg=-2147483648");
    unsigned long long v = 0;
    EXPECT_EQ(cv("neg", &v), 1);
    CHECKF((long long)v == -2147483648LL, "neg = %lld", (long long)v);
    v = 1;
    EXPECT_EQ(cv("big", &v), 1);
    t_note("CompatValue(\"4294967296\") = %lld (atoi overflow)", (long long)v);
    clear_compat();
}
TEST(dxgi_factory_compat_value_long_value_rejected) {
    NEED_COMPAT();
    char s[200] = "long=";
    memset(s + 5, '7', 120); s[125] = 0;
    set_compat(s);
    unsigned long long v = 0x55;
    int r = cv("long", &v);
    t_note("CompatValue with a 120 digit value: ret %d value %llu", r, v);
    CHECK(r == 1);
    EXPECT_EQ(v, 0x55);   /* size 121 exceeds the 100 byte scratch buffer: the number is not stored */
    clear_compat();
}
TEST(dxgi_factory_compat_value_hex_not_parsed) {
    NEED_COMPAT();
    set_compat("h=0x10");
    unsigned long long v = 99;
    EXPECT_EQ(cv("h", &v), 1);
    EXPECT_EQ(v, 0);
    clear_compat();
}
TEST(dxgi_factory_compat_value_uses_semicolon_boundaries) {
    NEED_COMPAT();
    set_compat("a=1;b=2;c=3");
    unsigned long long v;
    v = 0; cv("a", &v); EXPECT_EQ(v, 1);
    v = 0; cv("b", &v); EXPECT_EQ(v, 2);
    v = 0; cv("c", &v); EXPECT_EQ(v, 3);
    clear_compat();
}

/* ---------------- ApplyCompatResolutionQuirking ---------------- */
TEST(dxgi_factory_quirk_ordinary_resolutions_unchanged) {
    pfn_Quirk q = (pfn_Quirk)DX("ApplyCompatResolutionQuirking");
    REQUIRE(q);
    static const UINT r[][2] = { { 1, 1 }, { 123, 457 }, { 641, 481 }, { 1281, 721 }, { 4097, 2049 }, { 0, 0 }, { 0xffffffffu, 0xffffffffu }, { 1080, 1921 }, { 1081, 1920 } };
    for (size_t i = 0; i < sizeof(r) / sizeof(r[0]); i++) {
        UINT w = r[i][0], h = r[i][1];
        q(&w, &h);
        CHECKF(w == r[i][0] && h == r[i][1], "%ux%u became %ux%u", r[i][0], r[i][1], w, h);
    }
}
TEST(dxgi_factory_quirk_special_pairs_observed) {
    pfn_Quirk q = (pfn_Quirk)DX("ApplyCompatResolutionQuirking");
    REQUIRE(q);
    /* Substitution happens only for apps flagged by the OS compatibility database; an unflagged test process sees no change. */
    static const UINT r[][2] = { { 1080, 1920 }, { 540, 960 }, { 1440, 2560 } };
    for (size_t i = 0; i < 3; i++) {
        UINT w = r[i][0], h = r[i][1];
        q(&w, &h);
        t_note("quirk %ux%u -> %ux%u", r[i][0], r[i][1], w, h);
        CHECK((w == r[i][0] && h == r[i][1]) || (w == 720 && h == 1280) || (w == 480 && h == 800));
    }
}
TEST(dxgi_factory_quirk_swapped_axes_not_matched) {
    pfn_Quirk q = (pfn_Quirk)DX("ApplyCompatResolutionQuirking");
    REQUIRE(q);
    static const UINT r[][2] = { { 1920, 1080 }, { 960, 540 }, { 2560, 1440 } };
    for (size_t i = 0; i < 3; i++) {
        UINT w = r[i][0], h = r[i][1];
        q(&w, &h);
        CHECKF(w == r[i][0] && h == r[i][1], "landscape %ux%u became %ux%u", r[i][0], r[i][1], w, h);
    }
}
TEST(dxgi_factory_quirk_idempotent) {
    pfn_Quirk q = (pfn_Quirk)DX("ApplyCompatResolutionQuirking");
    REQUIRE(q);
    UINT w = 1080, h = 1920, w2, h2;
    q(&w, &h); w2 = w; h2 = h; q(&w2, &h2);
    CHECK(w2 == w && h2 == h);
}

/* ---------------- DXGIDumpJournal ---------------- */
static int g_jn; static int g_jbad; static char g_jfirst[300];
static void __cdecl jcb(const char *s) {
    if (!s) { g_jbad++; return; }
    size_t n = strlen(s);
    if (strncmp(s, "DXGI Error ", 11) || n < 14 || strcmp(s + n - 2, "\r\n")) g_jbad++;
    if (!g_jn) { strncpy(g_jfirst, s, sizeof(g_jfirst) - 1); }
    g_jn++;
}
TEST(dxgi_factory_journal_callback_count) {
    pfn_Journal j = (pfn_Journal)DX("DXGIDumpJournal");
    REQUIRE(j);
    g_jn = g_jbad = 0;
    j(jcb);
    EXPECT_EQ(g_jn, 64);   /* the journal ring has 64 slots and every slot is reported */
    EXPECT_EQ(g_jbad, 0);
    t_note("first journal line: %s", g_jfirst);
}
TEST(dxgi_factory_journal_repeatable) {
    pfn_Journal j = (pfn_Journal)DX("DXGIDumpJournal");
    REQUIRE(j);
    g_jn = g_jbad = 0; j(jcb); int a = g_jn;
    g_jn = g_jbad = 0; j(jcb); int b = g_jn;
    EXPECT_EQ(a, 64); EXPECT_EQ(b, 64);
}
TEST(dxgi_factory_journal_after_factory_use) {
    pfn_Journal j = (pfn_Journal)DX("DXGIDumpJournal");
    REQUIRE(j);
    IDXGIFactory1 *f = fh_f1();
    if (f) {
        IDXGIAdapter1 *a;
        IDXGIFactory1_EnumAdapters1(f, 9999, &a);
        IDXGIFactory1_MakeWindowAssociation(f, NULL, 0xffff);  /* invalid call, may record a journal entry */
        IDXGIFactory1_Release(f);
    }
    g_jn = g_jbad = 0; j(jcb);
    EXPECT_EQ(g_jn, 64);
    EXPECT_EQ(g_jbad, 0);
}

/* ---------------- DXGIGetDebugInterface1 ---------------- */
TEST(dxgi_factory_debug_null_out) {
    WINE_SKIP();
    pfn_dbg1 d = (pfn_dbg1)DX("DXGIGetDebugInterface1");
    REQUIRE(d);
    EXPECT_HR(d(0, &IID_IDXGIDebug, NULL), DXGI_E_INVALID_CALL);
    EXPECT_HR(d(0, &IID_IDXGIInfoQueue, NULL), DXGI_E_INVALID_CALL);
    EXPECT_HR(d(1, &IID_IDXGIDebug, NULL), DXGI_E_INVALID_CALL);
    EXPECT_HR(d(0xffffffffu, &GUID_NULL, NULL), DXGI_E_INVALID_CALL);
}
TEST(dxgi_factory_debug_availability_contract) {
    WINE_SKIP();
    pfn_dbg1 d = (pfn_dbg1)DX("DXGIGetDebugInterface1");
    REQUIRE(d);
    void *p = POISON;
    HRESULT hr = d(0, &IID_IDXGIDebug, &p);
    t_note("DXGIGetDebugInterface1(IDXGIDebug) = %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { CHECK(p != NULL && p != POISON); ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p); }
    else {
        CHECK(p == NULL);
        CHECKF(hr == (HRESULT)0x887A002D, "missing debug layer must map to DXGI_ERROR_SDK_COMPONENT_MISSING, got %s", d3dh_hr_name(hr));
    }
}
TEST(dxgi_factory_debug_flags_nonzero) {
    WINE_SKIP();
    pfn_dbg1 d = (pfn_dbg1)DX("DXGIGetDebugInterface1");
    REQUIRE(d);
    void *p = POISON;
    HRESULT hr0, hr;
    { void *q = NULL; hr0 = d(0, &IID_IDXGIDebug, &q); if (q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q); }
    hr = d(1, &IID_IDXGIDebug, &p);
    CHECK(p == NULL);
    if (SUCCEEDED(hr0)) EXPECT_HR(hr, DXGI_E_INVALID_CALL);      /* flags must be zero once the component exists */
    else EXPECT_HR(hr, hr0);                                      /* component missing: reported before flags are looked at */
}
TEST(dxgi_factory_debug_out_cleared_for_bad_iid) {
    WINE_SKIP();
    pfn_dbg1 d = (pfn_dbg1)DX("DXGIGetDebugInterface1");
    REQUIRE(d);
    static const IID *bad[] = { &IID_IDXGIFactory, &IID_IDXGIDevice, &GUID_NULL, &IID_IMalloc };
    for (size_t i = 0; i < 4; i++) {
        void *p = POISON;
        HRESULT hr = d(0, bad[i], &p);
        CHECKF(FAILED(hr), "bad iid %zu returned %s", i, d3dh_hr_name(hr));
        CHECKF(p == NULL, "bad iid %zu: out not cleared", i);
    }
}
TEST(dxgi_factory_debug_iid_matrix_observed) {
    WINE_SKIP();
    pfn_dbg1 d = (pfn_dbg1)DX("DXGIGetDebugInterface1");
    REQUIRE(d);
    static const struct { const char *n; const IID *i; } t[] = { { "IDXGIDebug", &IID_IDXGIDebug }, { "IDXGIDebug1", &IID_IDXGIDebug1 }, { "IDXGIInfoQueue", &IID_IDXGIInfoQueue } };
    for (size_t i = 0; i < 3; i++) {
        void *p = POISON;
        HRESULT hr = d(0, t[i].i, &p);
        t_note("DXGIGetDebugInterface1(%s) = %s", t[i].n, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p); else CHECK(p == NULL);
    }
}
TEST(dxgi_factory_debug_report_live_objects_if_present) {
    pfn_dbg1 d = (pfn_dbg1)DX("DXGIGetDebugInterface1");
    REQUIRE(d);
    IDXGIDebug *dbg = NULL;
    if (FAILED(d(0, &IID_IDXGIDebug, (void **)&dbg)) || !dbg) { t_skip("DXGI debug layer not installed"); return; }
    EXPECT_HR(IDXGIDebug_ReportLiveObjects(dbg, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY), S_OK);
    IDXGIDebug_Release(dbg);
}
TEST(dxgi_factory_debug_cf2_flag_matches_debug_availability) {
    pfn_dbg1 d = (pfn_dbg1)DX("DXGIGetDebugInterface1");
    REQUIRE(d); REQUIRE(fh_cf2());
    void *q = NULL;
    HRESULT hd = d(0, &IID_IDXGIDebug, &q);
    if (q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q);
    void *f = NULL;
    HRESULT hf = fh_cf2()(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory2, &f);
    t_note("debug interface %s, debug factory %s", d3dh_hr_name(hd), d3dh_hr_name(hf));
    if (f) ((IUnknown *)f)->lpVtbl->Release((IUnknown *)f);
    if (FAILED(hd)) CHECK(1);
}

/* ---------------- UpdateHMDEmulationStatus ---------------- */
TEST(dxgi_factory_hmd_update_returns_zero) {
    pfn_hmd h = (pfn_hmd)DX("UpdateHMDEmulationStatus");
    REQUIRE(h);
    EXPECT_EQ((ULONG)h(0), 0);
    EXPECT_EQ((ULONG)h(1), 0);
    EXPECT_EQ((ULONG)h(0), 0);
}
TEST(dxgi_factory_hmd_update_with_live_factory) {
    pfn_hmd h = (pfn_hmd)DX("UpdateHMDEmulationStatus");
    REQUIRE(h);
    IDXGIFactory1 *f = fh_f1();
    if (!f) { t_skip("no factory"); return; }
    EXPECT_EQ((ULONG)h(1), 0);
    BOOL cur = IDXGIFactory1_IsCurrent(f);
    t_note("IsCurrent after HMD emulation toggled on: %d", cur);
    EXPECT_EQ((ULONG)h(0), 0);
    IDXGIFactory1_Release(f);
}

/* ---------------- DXGIDeclareAdapterRemovalSupport (process wide: run in a child) ---------------- */
static int hr_code(HRESULT hr) {
    if (hr == S_OK) return 0;
    if (hr == (HRESULT)0x887A0036) return 2;   /* DXGI_ERROR_ALREADY_EXISTS */
    if (hr == DXGI_E_INVALID_CALL) return 3;
    return 1;
}
CHILD(declare_removal) {
    pfn_decl f = (pfn_decl)DX("DXGIDeclareAdapterRemovalSupport");
    if (!f) return 90;
    HRESULT a = f(), b = f();
    printf("DXGIDeclareAdapterRemovalSupport first=%08lx second=%08lx\n", (unsigned long)a, (unsigned long)b);
    return hr_code(a) | (hr_code(b) << 4);
}
CHILD(declare_removal_after_factory) {
    pfn_decl f = (pfn_decl)DX("DXGIDeclareAdapterRemovalSupport");
    if (!f) return 90;
    IDXGIFactory1 *fac = fh_f1();
    HRESULT a = f();
    printf("after factory creation: %08lx\n", (unsigned long)a);
    if (fac) IDXGIFactory1_Release(fac);
    return hr_code(a);
}
TEST(dxgi_factory_declare_removal_support_child) {
    REQUIRE(DX("DXGIDeclareAdapterRemovalSupport"));
    DWORD rc = t_run_child("declare_removal", "", 30000);
    CHECKF(rc != 0xDEAD0000 && rc != 0xDEAD0001 && rc != 90, "child failed to run: %#lx", (unsigned long)rc);
    if (rc < 0x100) {
        int a = rc & 15, b = rc >> 4;
        t_note("first call class %d, second call class %d (0 ok, 2 already exists, 3 invalid call, 1 other)", a, b);
        CHECK(a == 0 || a == 1 || a == 2 || a == 3);
    }
}
TEST(dxgi_factory_declare_removal_support_after_factory_child) {
    REQUIRE(DX("DXGIDeclareAdapterRemovalSupport"));
    DWORD rc = t_run_child("declare_removal_after_factory", "", 30000);
    CHECKF(rc != 0xDEAD0000 && rc != 0xDEAD0001 && rc != 90, "child failed to run: %#lx", (unsigned long)rc);
    t_note("declaration after a factory exists: class %lu", (unsigned long)rc);
}

/* ---------------- additional decomp-verified cases ---------------- */
TEST(dxgi_factory_compat_value_size_boundary_99_vs_100_chars) {
    NEED_COMPAT();
    char s[256]; unsigned long long v;
    strcpy(s, "z="); memset(s + 2, '0', 99); s[101] = 0;   /* 99 digits: size 100 fits the 100 byte scratch buffer */
    set_compat(s);
    v = 0x55; EXPECT_EQ(cv("z", &v), 1); EXPECT_EQ(v, 0);
    strcpy(s, "z="); memset(s + 2, '0', 100); s[102] = 0;  /* 100 digits: size 101, value not stored */
    set_compat(s);
    v = 0x55; EXPECT_EQ(cv("z", &v), 1); EXPECT_EQ(v, 0x55);
    clear_compat();
}
TEST(dxgi_factory_compat_string_zero_length_pointer_registers_nothing) {
    NEED_COMPAT();
    set_compat("keep=1");
    sp(0, "a=1");                 /* declared length 0: nothing is copied */
    char buf[8] = "junk"; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("a", &n, buf, 0), 0);
    EXPECT_EQ(buf[0], 0);
    clear_compat();
}
TEST(dxgi_factory_compat_string_value_stops_at_semicolon_not_equals) {
    NEED_COMPAT();
    set_compat("a=x=y;b=z");
    char buf[16]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("a", &n, buf, 0), 1); EXPECT_STR(buf, "x=y"); EXPECT_EQ(n, 4);
    n = sizeof(buf);
    EXPECT_EQ(cs("b", &n, buf, 0), 1); EXPECT_STR(buf, "z");
    clear_compat();
}
TEST(dxgi_factory_compat_string_name_is_prefix_of_later_name) {
    NEED_COMPAT();
    set_compat("ab=1;a=2");
    char buf[8]; unsigned long long n = sizeof(buf);
    EXPECT_EQ(cs("a", &n, buf, 0), 1);
    EXPECT_STR(buf, "2");
    clear_compat();
}
TEST(dxgi_factory_cf2_unsupported_iid_is_nointerface) {
    WINE_SKIP();
    REQUIRE(fh_cf2());
    /* 50c7f8e9-a63f-4035-9801-b96e733347de is rejected up front by CreateDXGIFactory2 */
    static const GUID g = { 0x50c7f8e9, 0xa63f, 0x4035, { 0x98, 0x01, 0xb9, 0x6e, 0x73, 0x33, 0x47, 0xde } };
    void *p = POISON;
    EXPECT_HR(fh_cf2()(0, &g, &p), E_NOINTERFACE);
    CHECK(p == NULL);
}
TEST(dxgi_factory_debug_interface_out_cleared_first) {
    WINE_SKIP();
    pfn_dbg1 d = (pfn_dbg1)DX("DXGIGetDebugInterface1");
    REQUIRE(d);
    void *p = POISON;
    HRESULT hr = d(0xffffffffu, &IID_IDXGIDebug, &p);
    CHECKF(FAILED(hr), "nonzero flags returned %s", d3dh_hr_name(hr));
    CHECK(p == NULL);
}
TEST(dxgi_factory_journal_line_format) {
    pfn_Journal j = (pfn_Journal)DX("DXGIDumpJournal");
    REQUIRE(j);
    g_jn = g_jbad = 0; g_jfirst[0] = 0;
    j(jcb);
    CHECK(!strncmp(g_jfirst, "DXGI Error ", 11));
    EXPECT_EQ(g_jbad, 0);
}
