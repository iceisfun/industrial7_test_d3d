/* d3d9.dll exports: ordinals, D3DPERF_* markers, debug stubs, shader validator. */
#include "d9c.h"

static const struct { const char *name; unsigned ord; } k_exports[] = {
    { "Direct3DCreate9On12", 20 }, { "Direct3DCreate9On12Ex", 21 }, { "Direct3DShaderValidatorCreate9", 24 },
    { "PSGPError", 25 }, { "PSGPSampleTexture", 26 }, { "D3DPERF_BeginEvent", 27 }, { "D3DPERF_EndEvent", 28 },
    { "D3DPERF_GetStatus", 29 }, { "D3DPERF_QueryRepeatFrame", 30 }, { "D3DPERF_SetMarker", 31 }, { "D3DPERF_SetOptions", 32 },
    { "D3DPERF_SetRegion", 33 }, { "DebugSetLevel", 34 }, { "DebugSetMute", 35 },
    { "Direct3D9EnableMaximizedWindowedModeShim", 36 }, { "Direct3DCreate9", 37 }, { "Direct3DCreate9Ex", 38 },
};

TEST(d3d9_core_exports_present) {
    HMODULE m = t_module("d3d9.dll"); REQUIRE(m);
    if (t_is_wine()) { t_skip("emulator export list differs"); return; }
    for (unsigned i = 0; i < sizeof k_exports / sizeof *k_exports; i++)
        CHECKF(GetProcAddress(m, k_exports[i].name) != NULL, "%s", k_exports[i].name);
}
TEST(d3d9_core_exports_ordinals) {
    HMODULE m = t_module("d3d9.dll"); REQUIRE(m);
    if (t_is_wine()) { t_skip("emulator export table differs"); return; }
    for (unsigned i = 0; i < sizeof k_exports / sizeof *k_exports; i++) {
        FARPROC byname = GetProcAddress(m, k_exports[i].name);
        FARPROC byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)k_exports[i].ord);
        CHECKF(byname && byord == byname, "%s ordinal %u: name %p ord %p", k_exports[i].name, k_exports[i].ord, (void *)byname, (void *)byord);
    }
}
TEST(d3d9_core_exports_unnamed_ordinal_17) {
    /* Direct3D9SetMaximizedWindowedModeShim is exported by ordinal only. Not called: it can write app-compat registry state. */
    HMODULE m = t_module("d3d9.dll"); REQUIRE(m);
    if (t_is_wine()) { t_skip("emulator export table differs"); return; }
    FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)17);
    CHECK(p != NULL);
}
TEST(d3d9_core_exports_missing_names) {
    HMODULE m = t_module("d3d9.dll"); REQUIRE(m);
    static const char *const bad[] = { "Direct3DCreate8", "Direct3DCreate9Ex2", "D3DPERF_Nothing", "direct3dcreate9", "Direct3DCreate9@4", "" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        FARPROC p = GetProcAddress(m, bad[i]);
        CHECKF(p == NULL, "name '%s'", bad[i]);
        if (!p) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(d3d9_core_exports_ordinal_out_of_range) {
    HMODULE m = t_module("d3d9.dll"); REQUIRE(m);
    SET_ERR(0);
    FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)0x7000);
    CHECK(p == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(d3d9_core_exports_module_filename) {
    HMODULE m = t_module("d3d9.dll"); REQUIRE(m);
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(m, buf, sizeof buf);
    CHECK(n > 0 && n < sizeof buf);
    size_t l = strlen(buf);
    CHECK(l >= 8 && !lstrcmpiA(buf + l - 8, "d3d9.dll"));
}
TEST(d3d9_core_exports_not_forwarders) {
    /* All named exports resolve inside the d3d9.dll image (no forwarder strings). */
    HMODULE m = t_module("d3d9.dll"); REQUIRE(m);
    if (t_is_wine()) { t_skip("emulator export table differs"); return; }
    BYTE *base = (BYTE *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    DWORD sz = nt->OptionalHeader.SizeOfImage;
    for (unsigned i = 0; i < sizeof k_exports / sizeof *k_exports; i++) {
        BYTE *p = (BYTE *)GetProcAddress(m, k_exports[i].name);
        CHECKF(p && p >= base && p < base + sz, "%s", k_exports[i].name);
    }
}
TEST(d3d9_core_exports_machine_x64) {
    HMODULE m = t_module("d3d9.dll"); REQUIRE(m);
    BYTE *base = (BYTE *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    EXPECT_EQ(nt->FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64);
    EXPECT_EQ(nt->OptionalHeader.Magic, IMAGE_NT_OPTIONAL_HDR64_MAGIC);
}

/* ---- D3DPERF_*: without a capture tool attached these are stubs ---- */
static int capture_loaded(void) { return GetModuleHandleW(L"DXCaptureReplay.dll") != NULL; }
#define PERF_FN(T, n) __typeof__(&n) T = (__typeof__(&n))t_proc_in("d3d9.dll", #n)

TEST(d3d9_core_perf_begin_end_no_tool) {
    PERF_FN(b, D3DPERF_BeginEvent); PERF_FN(e, D3DPERF_EndEvent); REQUIRE(b); REQUIRE(e);
    if (capture_loaded() || t_is_wine()) { t_note("capture tool or emulator present: skipping hard checks"); return; }
    EXPECT_EQ(b(0xFF00FF00, L"event"), -1);
    EXPECT_EQ(e(), -1);
}
TEST(d3d9_core_perf_begin_null_name) {
    PERF_FN(b, D3DPERF_BeginEvent); PERF_FN(e, D3DPERF_EndEvent); REQUIRE(b); REQUIRE(e);
    if (capture_loaded() || t_is_wine()) { t_note("skipped"); return; }
    EXPECT_EQ(b(0, NULL), -1);
    EXPECT_EQ(e(), -1);
}
TEST(d3d9_core_perf_nesting_depth) {
    PERF_FN(b, D3DPERF_BeginEvent); PERF_FN(e, D3DPERF_EndEvent); REQUIRE(b); REQUIRE(e);
    if (capture_loaded() || t_is_wine()) { t_note("skipped"); return; }
    /* Without a tool no depth is tracked: every Begin and every End yields -1, even unbalanced. */
    for (int i = 0; i < 8; i++) EXPECT_EQ(b(0xFFFFFFFF, L"nest"), -1);
    for (int i = 0; i < 12; i++) EXPECT_EQ(e(), -1);
}
TEST(d3d9_core_perf_end_unbalanced) {
    PERF_FN(e, D3DPERF_EndEvent); REQUIRE(e);
    if (capture_loaded() || t_is_wine()) { t_note("skipped"); return; }
    EXPECT_EQ(e(), -1);
    EXPECT_EQ(e(), -1);
}
TEST(d3d9_core_perf_get_status) {
    PERF_FN(g, D3DPERF_GetStatus); REQUIRE(g);
    DWORD r = g();
    if (capture_loaded() || t_is_wine()) { t_note("GetStatus=0x%lx (tool/emulator)", r); return; }
    EXPECT_EQ(r & 0xFF, 0);
}
TEST(d3d9_core_perf_get_status_stable) {
    PERF_FN(g, D3DPERF_GetStatus); REQUIRE(g);
    DWORD a = g() & 0xFF, b = g() & 0xFF;
    EXPECT_EQ(a, b);
}
TEST(d3d9_core_perf_query_repeat_frame) {
    PERF_FN(q, D3DPERF_QueryRepeatFrame); REQUIRE(q);
    if (t_is_wine()) { t_note("emulator"); return; }
    for (int i = 0; i < 4; i++) EXPECT_EQ(q() & 0xFF, 0);
}
TEST(d3d9_core_perf_set_marker_region_void) {
    PERF_FN(m, D3DPERF_SetMarker); PERF_FN(r, D3DPERF_SetRegion); REQUIRE(m); REQUIRE(r);
    m(0xFF123456, L"marker");
    r(0xFF654321, L"region");
    m(0, NULL);
    r(0, NULL);
    CHECK(1);
}
TEST(d3d9_core_perf_set_options) {
    PERF_FN(s, D3DPERF_SetOptions); REQUIRE(s);
    s(0);
    s(1);
    s(0xFFFFFFFF);
    s(0);
    CHECK(1);
}
TEST(d3d9_core_perf_set_options_no_effect_on_status) {
    PERF_FN(s, D3DPERF_SetOptions); PERF_FN(g, D3DPERF_GetStatus); REQUIRE(s); REQUIRE(g);
    DWORD before = g() & 0xFF;
    s(1);
    EXPECT_EQ(g() & 0xFF, before);
    s(0);
}
TEST(d3d9_core_perf_calls_with_live_object) {
    D9_GET(d);
    PERF_FN(b, D3DPERF_BeginEvent); PERF_FN(e, D3DPERF_EndEvent); REQUIRE(b); REQUIRE(e);
    int r1 = b(0xFF000000, L"scope"); int r2 = e();
    t_note("BeginEvent=%d EndEvent=%d with live IDirect3D9", r1, r2);
    EXPECT_EQ(r1, r2);   /* both stubbed alike, or both report depth 0 */
    IDirect3D9_Release(d);
}
TEST(d3d9_core_perf_cross_thread) {
    PERF_FN(b, D3DPERF_BeginEvent); PERF_FN(e, D3DPERF_EndEvent); REQUIRE(b); REQUIRE(e);
    if (capture_loaded() || t_is_wine()) { t_note("skipped"); return; }
    static __typeof__(&D3DPERF_BeginEvent) sb; static __typeof__(&D3DPERF_EndEvent) se; static int r[2];
    sb = b; se = e;
    DWORD WINAPI th(LPVOID p) { r[0] = sb(1, L"t"); r[1] = se(); return 0; }
    HANDLE h = CreateThread(NULL, 0, th, NULL, 0, NULL);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0); CloseHandle(h); }
    EXPECT_EQ(r[0], -1); EXPECT_EQ(r[1], -1);
}

/* ---- Debug stubs ---- */
UDECL(void, DebugSetMute, (DWORD));
UDECL(void, DebugSetLevel, (DWORD));
TEST(d3d9_core_debug_set_mute_stub) {
    DebugSetMute_pfn f = (DebugSetMute_pfn)t_proc_in("d3d9.dll", "DebugSetMute"); REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator export has a different signature"); return; }
    f(0); f(1); f(0);
    CHECK(1);
}
TEST(d3d9_core_debug_set_level_stub) {
    DebugSetLevel_pfn f = (DebugSetLevel_pfn)t_proc_in("d3d9.dll", "DebugSetLevel"); REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator export has a different signature"); return; }
    f(0); f(1); f(0);
    CHECK(1);
}
TEST(d3d9_core_debug_stubs_do_not_break_create) {
    DebugSetMute_pfn f = (DebugSetMute_pfn)t_proc_in("d3d9.dll", "DebugSetMute"); REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator export has a different signature"); return; }
    f(1);
    D9_GET(d);
    IDirect3D9_Release(d);
    f(0);
}

/* ---- Direct3DShaderValidatorCreate9 ---- */
typedef struct v9 v9;
typedef struct v9vt {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(v9 *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(v9 *);
    ULONG (STDMETHODCALLTYPE *Release)(v9 *);
    HRESULT (STDMETHODCALLTYPE *Begin)(v9 *, void *cb, void *ctx, DWORD reserved);
    HRESULT (STDMETHODCALLTYPE *Instruction)(v9 *, const char *comp, UINT line, const DWORD *tok, UINT cdw);
    HRESULT (STDMETHODCALLTYPE *End)(v9 *);
} v9vt;
struct v9 { const v9vt *vt; };
UDECL(v9 *, Direct3DShaderValidatorCreate9, (void));

static HRESULT CALLBACK val_cb(const char *file, UINT line, DWORD flags, UINT id, const char *msg, void *ctx) {
    if (ctx) (*(int *)ctx)++;
    return S_OK;
}
static v9 *mkval(void) {
    Direct3DShaderValidatorCreate9_pfn c = (Direct3DShaderValidatorCreate9_pfn)t_proc_in("d3d9.dll", "Direct3DShaderValidatorCreate9");
    if (!c) { t_skip("missing export: Direct3DShaderValidatorCreate9"); return NULL; }
    if (t_is_wine()) { t_skip("emulator does not implement the shader validator object"); return NULL; }
    v9 *v = c();
    if (!v) t_skip("validator not created");
    return v;
}
TEST(d3d9_core_validator_create_release) {
    v9 *v = mkval(); if (!v) return;
    EXPECT_EQ(v->vt->Release(v), 0);
}
TEST(d3d9_core_validator_refcount) {
    v9 *v = mkval(); if (!v) return;
    EXPECT_EQ(v->vt->AddRef(v), 2);
    EXPECT_EQ(v->vt->AddRef(v), 3);
    EXPECT_EQ(v->vt->Release(v), 2);
    EXPECT_EQ(v->vt->Release(v), 1);
    EXPECT_EQ(v->vt->Release(v), 0);
}
TEST(d3d9_core_validator_distinct) {
    v9 *a = mkval(); if (!a) return;
    v9 *b = mkval();
    CHECK(b != NULL && b != a);
    a->vt->Release(a);
    if (b) b->vt->Release(b);
}
TEST(d3d9_core_validator_qi_iunknown) {
    v9 *v = mkval(); if (!v) return;
    void *p = NULL;
    EXPECT_HR(v->vt->QueryInterface(v, &IID_IUnknown, &p), S_OK);
    CHECK(p == (void *)v);
    if (p) EXPECT_EQ(v->vt->Release(v), 1);
    EXPECT_EQ(v->vt->Release(v), 0);
}
TEST(d3d9_core_validator_qi_unknown_iid) {
    v9 *v = mkval(); if (!v) return;
    void *p = (void *)(ULONG_PTR)0x40;
    EXPECT_HR(v->vt->QueryInterface(v, &IID_IDirect3D9, &p), D9_E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_HR(v->vt->QueryInterface(v, &IID_IDirect3DDevice9, &p), D9_E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_EQ(v->vt->Release(v), 0);
}
TEST(d3d9_core_validator_end_before_begin_fails) {
    v9 *v = mkval(); if (!v) return;
    EXPECT_HR(v->vt->End(v), E_FAIL);
    v->vt->Release(v);
}
TEST(d3d9_core_validator_begin_twice_fails) {
    v9 *v = mkval(); if (!v) return;
    int calls = 0;
    EXPECT_HR(v->vt->Begin(v, (void *)val_cb, &calls, 0), S_OK);
    EXPECT_HR(v->vt->Begin(v, (void *)val_cb, &calls, 0), E_FAIL);
    v->vt->Release(v);
}
TEST(d3d9_core_validator_begin_end_without_tokens) {
    v9 *v = mkval(); if (!v) return;
    int calls = 0;
    EXPECT_HR(v->vt->Begin(v, (void *)val_cb, &calls, 0), S_OK);
    /* No version token was supplied: the validator reports a missing end token. */
    EXPECT_HR(v->vt->End(v), E_FAIL);
    v->vt->Release(v);
}
TEST(d3d9_core_validator_failed_end_poisons_state) {
    v9 *v = mkval(); if (!v) return;
    int calls = 0;
    EXPECT_HR(v->vt->End(v), E_FAIL);
    /* After an out-of-order End the validator stays in its error state. */
    EXPECT_HR(v->vt->Begin(v, (void *)val_cb, &calls, 0), E_FAIL);
    EXPECT_HR(v->vt->End(v), E_FAIL);
    v->vt->Release(v);
}
TEST(d3d9_core_validator_instruction_stream_note) {
    v9 *v = mkval(); if (!v) return;
    int calls = 0;
    static const DWORD ver[] = { 0xFFFE0200 };
    static const DWORD dcl[] = { 0x0200001F, 0x80000000, 0x900F0000 };
    static const DWORD mov[] = { 0x02000001, 0xC00F0000, 0x90E40000 };
    static const DWORD end[] = { 0x0000FFFF };
    HRESULT hb = v->vt->Begin(v, (void *)val_cb, &calls, 0);
    EXPECT_HR(hb, S_OK);
    HRESULT h1 = v->vt->Instruction(v, "t", 1, ver, 1);
    HRESULT h2 = v->vt->Instruction(v, "t", 2, dcl, 3);
    HRESULT h3 = v->vt->Instruction(v, "t", 3, mov, 3);
    HRESULT h4 = v->vt->Instruction(v, "t", 4, end, 1);
    HRESULT he = v->vt->End(v);
    t_note("validator vs_2_0 stream: ver=%s dcl=%s mov=%s end=%s End=%s callbacks=%d", d3dh_hr_name(h1), d3dh_hr_name(h2), d3dh_hr_name(h3), d3dh_hr_name(h4), d3dh_hr_name(he), calls);
    v->vt->Release(v);
}

/* ---- Not exercised on purpose ---- */
TEST(d3d9_core_psgp_exports_documented) {
    /* PSGPError unconditionally throws a C++ exception (0x8876086c) and PSGPSampleTexture dereferences an internal
       vertex-processing object; neither can be driven through a public contract. Presence is checked above. */
    t_note("PSGPError/PSGPSampleTexture are internal software-vertex-processing hooks; not called");
    CHECK(t_proc_in("d3d9.dll", "PSGPError") != NULL || t_is_wine());
}
TEST(d3d9_core_maximized_windowed_shim_export) {
    FARPROC p = t_proc_in("d3d9.dll", "Direct3D9EnableMaximizedWindowedModeShim");
    REQUIRE(p);
    t_note("Direct3D9EnableMaximizedWindowedModeShim(nonzero) writes an app-compat registry key; not called");
}
