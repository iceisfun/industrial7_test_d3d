/* Internal exports with process-global side effects (TLS blocks, ETW events, journal sentinel, shim flag).
 * Nothing touches the registry, the display, or the GPU. */
#include "t.h"

/* ---- d3d12 PIX plumbing ---------------------------------------------------- */
typedef void *(WINAPI *pix_threadinfo_t)(void);
typedef void  (WINAPI *pix_counter_t)(const WCHAR *name, float value);
typedef void  (WINAPI *pix_wake_t)(HANDLE ev, ULONG_PTR a);

TEST(dxgap3_pix_get_thread_info_non_null) {
    pix_threadinfo_t f = (pix_threadinfo_t)t_proc_in("d3d12.dll", "D3D12PIXGetThreadInfo"); REQUIRE(f);
    CHECK(f() != NULL);
}
TEST(dxgap3_pix_get_thread_info_stable_in_thread) {
    pix_threadinfo_t f = (pix_threadinfo_t)t_proc_in("d3d12.dll", "D3D12PIXGetThreadInfo"); REQUIRE(f);
    void *a = f(), *b = f(), *c = f();
    CHECK(a != NULL); CHECK(a == b); CHECK(b == c);
}
static pix_threadinfo_t g_ti; static void *g_other;
static DWORD WINAPI ti_thread(void *p) { g_other = g_ti(); return 0; }
TEST(dxgap3_pix_get_thread_info_other_thread) {
    g_ti = (pix_threadinfo_t)t_proc_in("d3d12.dll", "D3D12PIXGetThreadInfo"); REQUIRE(g_ti);
    void *mine = g_ti();
    HANDLE h = CreateThread(NULL, 0, ti_thread, NULL, 0, NULL);
    REQUIRE(h);
    EXPECT_EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(g_other != NULL);
    /* Win10 retail d3d12.dll: PIX markers are disabled, every thread gets the same static block */
    CHECK(g_other == mine);
}
TEST(dxgap3_pix_report_counter_returns) {
    pix_counter_t f = (pix_counter_t)t_proc_in("d3d12.dll", "D3D12PIXReportCounter"); REQUIRE(f);
    f(L"gap3 counter", 1.5f);
    f(L"gap3 counter", 0.0f);
    f(L"", -1.0f);
    EXPECT_EQ(GetLastError(), 0); /* no error set (SetLastError(0) at test start) */
}
TEST(dxgap3_pix_notify_wake_returns) {
    pix_wake_t f = (pix_wake_t)t_proc_in("d3d12.dll", "D3D12PIXNotifyWakeFromFenceSignal"); REQUIRE(f);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL); REQUIRE(ev);
    f(ev, 0);
    f(ev, 1);
    CHECK(WaitForSingleObject(ev, 0) == WAIT_TIMEOUT);   /* the notification never signals the event */
    CloseHandle(ev);
}
TEST(dxgap3_pix_calls_keep_last_error) {
    pix_threadinfo_t f = (pix_threadinfo_t)t_proc_in("d3d12.dll", "D3D12PIXGetThreadInfo"); REQUIRE(f);
    pix_counter_t c = (pix_counter_t)t_proc_in("d3d12.dll", "D3D12PIXReportCounter"); REQUIRE(c);
    SetLastError(0x4242); f(); c(L"x", 1.0f);
    EXPECT_EQ(GetLastError(), 0x4242);
}

/* ---- d3d10level9 journal --------------------------------------------------- */
typedef void (__cdecl *journal_cb_t)(const char *);
typedef void (__cdecl *dump_t)(journal_cb_t);
static int g_cb_n, g_cb_bad, g_nested_n; static dump_t g_dump;
static void __cdecl cb(const char *s) {
    g_cb_n++;
    size_t n = s ? strlen(s) : 0;
    if (!s || strncmp(s, "D3D10Level9 Error ", 18) || n < 20 || strcmp(s + n - 2, "\r\n")) g_cb_bad++;
    int before = g_cb_n; g_dump(cb);   /* re-entrant dump is ignored while one is in progress */
    g_nested_n += g_cb_n - before;
}
TEST(dxgap3_level9_dump_journal_callback_format) {
    g_dump = (dump_t)t_proc_in("d3d10level9.dll", "D3D10Level9DumpJournal"); REQUIRE(g_dump);
    g_cb_n = g_cb_bad = g_nested_n = 0;
    g_dump(cb);
    CHECK(g_cb_bad == 0);
    CHECK(g_nested_n == 0);
    EXPECT_EQ(g_cb_n, 0); /* a fresh process has an empty level 9 journal */
}
static int g_plain;
static void __cdecl cb_plain(const char *s) { g_plain++; }
TEST(dxgap3_level9_dump_journal_repeatable) {
    dump_t f = (dump_t)t_proc_in("d3d10level9.dll", "D3D10Level9DumpJournal"); REQUIRE(f);
    g_plain = 0; f(cb_plain); int first = g_plain;
    g_plain = 0; f(cb_plain);
    EXPECT_EQ(g_plain, first);   /* dumping does not consume the journal */
}

/* ---- d3d10level9 filtered open-adapter hook ------------------------------- */
typedef void *(__cdecl *retrieve_t)(void);
TEST(dxgap3_retrieve_filtered_open_adapter_null) {
    retrieve_t f = (retrieve_t)t_proc_in("d3d10level9.dll", "RetrieveFilteredOpenAdapter"); REQUIRE(f);
    CHECK(f() == NULL);
    for (int i = 0; i < 300; i++) if (f() != NULL) { CHECKF(0, "non-NULL on call %d", i); break; }
}

/* ---- d3d9 maximized-windowed-mode shim (flag 0: no registry access) --------- */
typedef void (WINAPI *shim_t)(int addKey);
TEST(dxgap3_d3d9_maximized_windowed_shim_no_registry_flag) {
    shim_t f = (shim_t)t_proc_in("d3d9.dll", "Direct3D9EnableMaximizedWindowedModeShim"); REQUIRE(f);
    SET_ERR(0x4343);
    f(0);
    f(0);   /* idempotent */
    EXPECT_EQ(GetLastError(), 0x4343);
}

/* ---- layered device registration ------------------------------------------- */
typedef int (WINAPI *reg_layers_t)(const void *layers, ULONG_PTR count, BOOL lock);
TEST(dxgap3_d3d11_register_layers_empty_many_times) {
    reg_layers_t f = (reg_layers_t)t_proc_in("d3d11.dll", "D3D11CoreRegisterLayers"); REQUIRE(f);
    for (int i = 0; i < 1000; i++) if (f(NULL, 0, i & 1) != 0) { CHECKF(0, "iteration %d", i); break; }
}

/* ---- the PIX / journal stubs perform no validation ------------------------------------------ */
typedef struct { int which; } stubctx;
static void stub_call(void *p)
{
    stubctx *c = p;
    static WCHAR big[5000];
    static int init;
    if (!init) { for (int i = 0; i < 4999; i++) big[i] = L'a'; init = 1; }
    switch (c->which) {
    case 0: ((pix_counter_t)t_proc_in("d3d12.dll", "D3D12PIXReportCounter"))(NULL, 1.0f); break;
    case 1: ((pix_counter_t)t_proc_in("d3d12.dll", "D3D12PIXReportCounter"))(big, 1.0f); break;
    case 2: ((pix_counter_t)t_proc_in("d3d12.dll", "D3D12PIXReportCounter"))(L"nan", 0.0f / 0.0f); break;
    case 3: ((pix_wake_t)t_proc_in("d3d12.dll", "D3D12PIXNotifyWakeFromFenceSignal"))(NULL, 0); break;
    case 4: ((pix_wake_t)t_proc_in("d3d12.dll", "D3D12PIXNotifyWakeFromFenceSignal"))((HANDLE)(ULONG_PTR)0x1234, 5); break;
    case 5: ((dump_t)t_proc_in("d3d10level9.dll", "D3D10Level9DumpJournal"))(NULL); break;
    }
}
TEST(dxgap3_stubs_accept_garbage_arguments) {
    REQUIRE(t_proc_in("d3d12.dll", "D3D12PIXReportCounter")); REQUIRE(t_proc_in("d3d12.dll", "D3D12PIXNotifyWakeFromFenceSignal"));
    REQUIRE(t_proc_in("d3d10level9.dll", "D3D10Level9DumpJournal"));
    for (int i = 0; i <= 5; i++) { stubctx c = { i }; DWORD code = t_try(stub_call, &c); CHECKF(code == 0, "stub case %d raised 0x%lx", i, code); }
}

/* ---- PIX thread info is one process-wide block, whichever thread asks ------------------------- */
#define NT 8
static void *g_blk[NT]; static pix_threadinfo_t g_tif;
static DWORD WINAPI blk_thread(void *p) { int i = (int)(ULONG_PTR)p; for (int k = 0; k < 100; k++) { void *b = g_tif(); if (k == 0) g_blk[i] = b; else if (b != g_blk[i]) g_blk[i] = NULL; } return 0; }
TEST(dxgap3_pix_get_thread_info_same_block_on_many_threads) {
    g_tif = (pix_threadinfo_t)t_proc_in("d3d12.dll", "D3D12PIXGetThreadInfo"); REQUIRE(g_tif);
    HANDLE h[NT]; void *mine = g_tif();
    for (int i = 0; i < NT; i++) { g_blk[i] = (void *)1; h[i] = CreateThread(NULL, 0, blk_thread, (void *)(ULONG_PTR)i, 0, NULL); REQUIRE(h[i]); }
    EXPECT_EQ(WaitForMultipleObjects(NT, h, TRUE, 20000), WAIT_OBJECT_0);
    for (int i = 0; i < NT; i++) { CloseHandle(h[i]); CHECKF(g_blk[i] == mine, "thread %d saw %p, main thread %p", i, g_blk[i], mine); }
}

/* ---- RetrieveFilteredOpenAdapter from several threads ----------------------------------------- */
static retrieve_t g_ret; static volatile LONG g_ret_bad;
static DWORD WINAPI ret_thread(void *p) { for (int k = 0; k < 500; k++) if (g_ret() != NULL) InterlockedIncrement(&g_ret_bad); return 0; }
TEST(dxgap3_retrieve_filtered_open_adapter_null_on_threads) {
    g_ret = (retrieve_t)t_proc_in("d3d10level9.dll", "RetrieveFilteredOpenAdapter"); REQUIRE(g_ret);
    HANDLE h[4];
    for (int i = 0; i < 4; i++) { h[i] = CreateThread(NULL, 0, ret_thread, NULL, 0, NULL); REQUIRE(h[i]); }
    EXPECT_EQ(WaitForMultipleObjects(4, h, TRUE, 20000), WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) CloseHandle(h[i]);
    EXPECT_EQ(g_ret_bad, 0);
}
