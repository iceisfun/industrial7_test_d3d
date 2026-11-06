/* d3d9.dll creation entry points: Direct3DCreate9 / Ex / On12, QueryInterface ladder, refcounts. */
#include "d9c.h"

/* Skip when the runtime cannot create any enumeration object at all (e.g. an emulator without a display driver). */
static int can_create(void) {
    __typeof__(&Direct3DCreate9) c = D9_CREATE9();
    if (!c) { t_skip("missing export: Direct3DCreate9"); return 0; }
    IDirect3D9 *d = c(D3D_SDK_VERSION);
    if (!d) { t_skip("Direct3DCreate9 returned NULL for the current SDK version"); return 0; }
    IDirect3D9_Release(d);
    return 1;
}
#define NEED_CREATE() do { if (!can_create()) return; } while (0)

/* ---- Direct3DCreate9: SDK version handling (accepted: 31, 32, >= 132 after masking bit 31) ---- */
TEST(d3d9_core_create9_current_version) {
    __typeof__(&Direct3DCreate9) c = D9_CREATE9(); REQUIRE(c); NEED_CREATE();
    IDirect3D9 *d = c(D3D_SDK_VERSION);
    CHECK(d != NULL);
    if (d) EXPECT_EQ(IDirect3D9_Release(d), 0);
}
TEST(d3d9_core_create9_sdk_version_constant) { EXPECT_EQ(D3D_SDK_VERSION, 32); }
TEST(d3d9_core_create9_valid_versions) {
    __typeof__(&Direct3DCreate9) c = D9_CREATE9(); REQUIRE(c); NEED_CREATE();
    static const UINT v[] = { 31, 32, 0x80000020u, 0x8000001Fu, 132, 200, 1000 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        IDirect3D9 *d = c(v[i]);
        CHECKF(d != NULL, "version 0x%x", v[i]);
        if (d) IDirect3D9_Release(d);
    }
}
TEST(d3d9_core_create9_invalid_versions) {
    __typeof__(&Direct3DCreate9) c = D9_CREATE9(); REQUIRE(c); NEED_CREATE();
    static const UINT v[] = { 0, 1, 2, 7, 15, 29, 30, 33, 34, 40, 64, 100, 131, 0x80000000u, 0x80000001u, 0x8000001Eu, 0x80000021u, 0x80000083u };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        IDirect3D9 *d = c(v[i]);
        CHECKF(d == NULL, "version 0x%x gave %p", v[i], (void *)d);
        if (d) IDirect3D9_Release(d);
    }
}
TEST(d3d9_core_create9_boundaries_exact) {
    /* 30 rejected, 31 accepted, 32 accepted, 33 rejected, 131 rejected, 132 accepted. */
    __typeof__(&Direct3DCreate9) c = D9_CREATE9(); REQUIRE(c); NEED_CREATE();
    static const struct { UINT v; int ok; } t[] = { {30,0},{31,1},{32,1},{33,0},{131,0},{132,1} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3D9 *d = c(t[i].v);
        CHECKF((d != NULL) == t[i].ok, "version %u", t[i].v);
        if (d) IDirect3D9_Release(d);
    }
}
TEST(d3d9_core_create9_distinct_objects) {
    __typeof__(&Direct3DCreate9) c = D9_CREATE9(); REQUIRE(c); NEED_CREATE();
    IDirect3D9 *a = c(D3D_SDK_VERSION), *b = c(D3D_SDK_VERSION);
    CHECK(a && b);
    if (a && b) { CHECK(a != b); EXPECT_EQ(IDirect3D9_Release(a), 0); EXPECT_EQ(IDirect3D9_Release(b), 0); }
    else { SAFE_RELEASE(a); SAFE_RELEASE(b); }
}
TEST(d3d9_core_create9_many_create_release) {
    __typeof__(&Direct3DCreate9) c = D9_CREATE9(); REQUIRE(c); NEED_CREATE();
    for (int i = 0; i < 50; i++) {
        IDirect3D9 *d = c(D3D_SDK_VERSION);
        if (!d) { CHECKF(0, "iteration %d returned NULL", i); break; }
        IDirect3D9_Release(d);
    }
}
TEST(d3d9_core_create9_debug_flag_bit31) {
    /* Bit 31 of the SDK version is masked before validation. */
    __typeof__(&Direct3DCreate9) c = D9_CREATE9(); REQUIRE(c); NEED_CREATE();
    IDirect3D9 *d = c(D3D_SDK_VERSION | 0x80000000u);
    CHECK(d != NULL);
    if (d) IDirect3D9_Release(d);
}

/* ---- Direct3DCreate9Ex ---- */
TEST(d3d9_core_create9ex_current_version) {
    __typeof__(&Direct3DCreate9Ex) c = D9_CREATE9EX(); REQUIRE(c); NEED_CREATE();
    IDirect3D9Ex *d = NULL;
    HRESULT hr = c(D3D_SDK_VERSION, &d);
    EXPECT_HR(hr, S_OK);
    CHECK(d != NULL);
    if (d) EXPECT_EQ(IDirect3D9Ex_Release(d), 0);
}
TEST(d3d9_core_create9ex_invalid_versions) {
    __typeof__(&Direct3DCreate9Ex) c = D9_CREATE9EX(); REQUIRE(c); NEED_CREATE();
    static const UINT v[] = { 0, 1, 30, 33, 100, 131, 0x80000000u, 0x80000021u };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        IDirect3D9Ex *d = NULL;
        HRESULT hr = c(v[i], &d);
        CHECKF(hr == D9_NOTAVAILABLE, "version 0x%x hr %s", v[i], d3dh_hr_name(hr));
        CHECKF(d == NULL, "version 0x%x out %p", v[i], (void *)d);
        if (d) IDirect3D9Ex_Release(d);
    }
}
TEST(d3d9_core_create9ex_out_cleared_on_failure) {
    /* The out slot is written NULL before anything else happens, even when the call fails. */
    __typeof__(&Direct3DCreate9Ex) c = D9_CREATE9EX(); REQUIRE(c); NEED_CREATE();
    IDirect3D9Ex *d = (IDirect3D9Ex *)(ULONG_PTR)0x10;
    HRESULT hr = c(1, &d);
    EXPECT_HR(hr, D9_NOTAVAILABLE);
    CHECK(d == NULL);
}
TEST(d3d9_core_create9ex_out_overwritten_on_success) {
    __typeof__(&Direct3DCreate9Ex) c = D9_CREATE9EX(); REQUIRE(c); NEED_CREATE();
    IDirect3D9Ex *d = (IDirect3D9Ex *)(ULONG_PTR)0x10;
    HRESULT hr = c(D3D_SDK_VERSION, &d);
    EXPECT_HR(hr, S_OK);
    CHECK(d != NULL && d != (IDirect3D9Ex *)(ULONG_PTR)0x10);
    if (d && d != (IDirect3D9Ex *)(ULONG_PTR)0x10) IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_create9ex_boundaries) {
    __typeof__(&Direct3DCreate9Ex) c = D9_CREATE9EX(); REQUIRE(c); NEED_CREATE();
    static const struct { UINT v; int ok; } t[] = { {30,0},{31,1},{32,1},{33,0},{131,0},{132,1},{0x80000020u,1} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3D9Ex *d = NULL;
        HRESULT hr = c(t[i].v, &d);
        if (t[i].ok) { CHECKF(SUCCEEDED(hr) && d, "version 0x%x hr %s", t[i].v, d3dh_hr_name(hr)); }
        else { CHECKF(hr == D9_NOTAVAILABLE && !d, "version 0x%x hr %s", t[i].v, d3dh_hr_name(hr)); }
        if (d) IDirect3D9Ex_Release(d);
    }
}
TEST(d3d9_core_create9ex_null_out_note) {
    /* Behaviour with a NULL out pointer is not documented as an INVALIDCALL path on Windows 10; deliberately not called. */
    __typeof__(&Direct3DCreate9Ex) c = D9_CREATE9EX(); REQUIRE(c); NEED_CREATE();
    t_note("Direct3DCreate9Ex(ver, NULL) is not exercised: d3d9.dll writes *ppD3D9Ex first (no INVALIDCALL validation)");
}

/* ---- QueryInterface ladder ---- */
TEST(d3d9_core_qi_plain_iunknown_and_d3d9) {
    D9_GET(d);
    void *p = NULL;
    EXPECT_HR(IDirect3D9_QueryInterface(d, &IID_IUnknown, &p), S_OK);
    CHECK(p == (void *)d);
    if (p) EXPECT_EQ(IUnknown_Release((IUnknown *)p), 1);
    p = NULL;
    EXPECT_HR(IDirect3D9_QueryInterface(d, &IID_IDirect3D9, &p), S_OK);
    CHECK(p == (void *)d);
    if (p) EXPECT_EQ(IUnknown_Release((IUnknown *)p), 1);
    EXPECT_EQ(IDirect3D9_Release(d), 0);
}
TEST(d3d9_core_qi_plain_rejects_ex) {
    D9_GET(d);
    void *p = (void *)(ULONG_PTR)0x20;
    HRESULT hr = IDirect3D9_QueryInterface(d, &IID_IDirect3D9Ex, &p);
    EXPECT_HR(hr, D9_E_NOINTERFACE);
    CHECK(p == NULL);
    if (p) IUnknown_Release((IUnknown *)p);
    EXPECT_EQ(IDirect3D9_Release(d), 0);
}
TEST(d3d9_core_qi_plain_unknown_iids) {
    D9_GET(d);
    const GUID *iids[] = { &IID_IDirect3DDevice9, &IID_IDirect3DDevice9Ex, &IID_IDirect3DTexture9, &IID_IClassFactory, &IID_IDirect3DSwapChain9, &GUID_NULL };
    for (unsigned i = 0; i < sizeof iids / sizeof *iids; i++) {
        void *p = (void *)(ULONG_PTR)0x20;
        HRESULT hr = IDirect3D9_QueryInterface(d, iids[i], &p);
        CHECKF(hr == D9_E_NOINTERFACE, "iid %s hr %s", d3dh_guid_str(iids[i]), d3dh_hr_name(hr));
        CHECKF(p == NULL, "iid %s out %p", d3dh_guid_str(iids[i]), p);
        if (p) IUnknown_Release((IUnknown *)p);
    }
    EXPECT_EQ(IDirect3D9_Release(d), 0);
}
TEST(d3d9_core_qi_ex_ladder) {
    D9_GET_EX(d);
    void *p = NULL;
    const GUID *good[] = { &IID_IUnknown, &IID_IDirect3D9, &IID_IDirect3D9Ex };
    for (unsigned i = 0; i < 3; i++) {
        p = NULL;
        HRESULT hr = IDirect3D9Ex_QueryInterface(d, good[i], &p);
        CHECKF(hr == S_OK, "iid %s hr %s", d3dh_guid_str(good[i]), d3dh_hr_name(hr));
        CHECKF(p == (void *)d, "iid %s ptr %p vs %p", d3dh_guid_str(good[i]), p, (void *)d);
        if (p) IUnknown_Release((IUnknown *)p);
    }
    p = (void *)(ULONG_PTR)0x20;
    EXPECT_HR(IDirect3D9Ex_QueryInterface(d, &IID_IDirect3DDevice9, &p), D9_E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_EQ(IDirect3D9Ex_Release(d), 0);
}
TEST(d3d9_core_qi_ex_down_and_up_same_object) {
    D9_GET_EX(d);
    IDirect3D9 *d9 = NULL; IDirect3D9Ex *d9e = NULL;
    EXPECT_HR(IDirect3D9Ex_QueryInterface(d, &IID_IDirect3D9, (void **)&d9), S_OK);
    CHECK((void *)d9 == (void *)d);
    if (d9) {
        EXPECT_HR(IDirect3D9_QueryInterface(d9, &IID_IDirect3D9Ex, (void **)&d9e), S_OK);
        CHECK((void *)d9e == (void *)d);
        SAFE_RELEASE(d9e);
        IDirect3D9_Release(d9);
    }
    EXPECT_EQ(IDirect3D9Ex_Release(d), 0);
}
TEST(d3d9_core_qi_result_matches_create_flavour) {
    /* Create9 object is not an Ex object, Create9Ex object always is; the bit is fixed at creation. */
    D9_GET(a);
    D9_GET_EX(b);
    void *p = NULL;
    CHECK(FAILED(IDirect3D9_QueryInterface(a, &IID_IDirect3D9Ex, &p)));
    CHECK(SUCCEEDED(IDirect3D9Ex_QueryInterface(b, &IID_IDirect3D9Ex, &p)));
    if (p) IUnknown_Release((IUnknown *)p);
    IDirect3D9_Release(a);
    IDirect3D9Ex_Release(b);
}

/* ---- Refcounts ---- */
TEST(d3d9_core_refcount_fresh_object_is_one) {
    D9_GET(d);
    EXPECT_EQ(IDirect3D9_AddRef(d), 2);
    EXPECT_EQ(IDirect3D9_Release(d), 1);
    EXPECT_EQ(IDirect3D9_Release(d), 0);
}
TEST(d3d9_core_refcount_addref_sequence) {
    D9_GET(d);
    for (ULONG i = 2; i < 12; i++) EXPECT_EQ(IDirect3D9_AddRef(d), i);
    for (ULONG i = 10; i >= 1; i--) EXPECT_EQ(IDirect3D9_Release(d), i);
    EXPECT_EQ(IDirect3D9_Release(d), 0);
}
TEST(d3d9_core_refcount_qi_adds_ref) {
    D9_GET_EX(d);
    IUnknown *u = NULL;
    EXPECT_HR(IDirect3D9Ex_QueryInterface(d, &IID_IUnknown, (void **)&u), S_OK);
    EXPECT_EQ(IDirect3D9Ex_AddRef(d), 3);
    EXPECT_EQ(IDirect3D9Ex_Release(d), 2);
    EXPECT_EQ(IUnknown_Release(u), 1);
    EXPECT_EQ(IDirect3D9Ex_Release(d), 0);
}
TEST(d3d9_core_refcount_failed_qi_no_ref) {
    D9_GET(d);
    void *p = NULL;
    IDirect3D9_QueryInterface(d, &IID_IDirect3D9Ex, &p);
    EXPECT_EQ(IDirect3D9_AddRef(d), 2);
    EXPECT_EQ(IDirect3D9_Release(d), 1);
    EXPECT_EQ(IDirect3D9_Release(d), 0);
}
TEST(d3d9_core_refcount_independent_objects) {
    D9_GET(a); D9_GET(b);
    EXPECT_EQ(IDirect3D9_AddRef(a), 2);
    EXPECT_EQ(IDirect3D9_AddRef(b), 2);
    EXPECT_EQ(IDirect3D9_Release(a), 1);
    EXPECT_EQ(IDirect3D9_Release(a), 0);
    EXPECT_EQ(IDirect3D9_Release(b), 1);
    EXPECT_EQ(IDirect3D9_Release(b), 0);
}
TEST(d3d9_core_object_usable_after_other_released) {
    D9_GET(a); D9_GET(b);
    UINT want = IDirect3D9_GetAdapterCount(b);
    IDirect3D9_Release(a);
    EXPECT_EQ(IDirect3D9_GetAdapterCount(b), want);   /* the surviving object is independent of its sibling */
    EXPECT_EQ(IDirect3D9_Release(b), 0);
}
TEST(d3d9_core_object_cross_thread_refcount) {
    D9_GET(d);
    struct ctx { IDirect3D9 *d; ULONG a; } c = { d, 0 };
    DWORD WINAPI th(LPVOID p) { struct ctx *x = p; x->a = IDirect3D9_AddRef(x->d); IDirect3D9_Release(x->d); return 0; }
    HANDLE h = CreateThread(NULL, 0, th, &c, 0, NULL);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0); CloseHandle(h); }
    EXPECT_EQ(c.a, 2);
    EXPECT_EQ(IDirect3D9_Release(d), 0);
}
TEST(d3d9_core_object_methods_from_other_thread) {
    D9_GET(d);
    struct ctx { IDirect3D9 *d; UINT n, m; } c = { d, 0, 0 };
    DWORD WINAPI th(LPVOID p) { struct ctx *x = p; x->n = IDirect3D9_GetAdapterCount(x->d); return 0; }
    c.m = IDirect3D9_GetAdapterCount(d);
    HANDLE h = CreateThread(NULL, 0, th, &c, 0, NULL);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0); CloseHandle(h); }
    EXPECT_EQ(c.n, c.m);
    IDirect3D9_Release(d);
}

/* ---- Direct3DCreate9On12 / On12Ex ---- */
typedef struct { BOOL Enable9On12; IUnknown *pD3D12Device; IUnknown *ppD3D12Queues[2]; UINT NumQueues; UINT NodeMask; } d9c_on12_args;
UDECL(IDirect3D9 *, Direct3DCreate9On12, (UINT, d9c_on12_args *, UINT));
UDECL(HRESULT, Direct3DCreate9On12Ex, (UINT, d9c_on12_args *, UINT, IDirect3D9Ex **));

TEST(d3d9_core_on12_args_layout) {
    EXPECT_EQ(sizeof(d9c_on12_args), 40);
    EXPECT_EQ(offsetof(d9c_on12_args, pD3D12Device), 8);
    EXPECT_EQ(offsetof(d9c_on12_args, NumQueues), 32);
}
TEST(d3d9_core_on12_null_args) {
    Direct3DCreate9On12_pfn c = (Direct3DCreate9On12_pfn)t_proc_in("d3d9.dll", "Direct3DCreate9On12"); REQUIRE(c); NEED_CREATE();
    IDirect3D9 *d = c(D3D_SDK_VERSION, NULL, 0);
    CHECK(d != NULL);
    if (d) {
        void *p = NULL;
        CHECK(FAILED(IDirect3D9_QueryInterface(d, &IID_IDirect3D9Ex, &p)));
        EXPECT_EQ(IDirect3D9_Release(d), 0);
    }
}
TEST(d3d9_core_on12_bad_version) {
    Direct3DCreate9On12_pfn c = (Direct3DCreate9On12_pfn)t_proc_in("d3d9.dll", "Direct3DCreate9On12"); REQUIRE(c); NEED_CREATE();
    static const UINT v[] = { 0, 30, 33, 131, 0x80000021u };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        IDirect3D9 *d = c(v[i], NULL, 0);
        CHECKF(d == NULL, "version 0x%x", v[i]);
        if (d) IDirect3D9_Release(d);
    }
}
TEST(d3d9_core_on12_disabled_args) {
    Direct3DCreate9On12_pfn c = (Direct3DCreate9On12_pfn)t_proc_in("d3d9.dll", "Direct3DCreate9On12"); REQUIRE(c); NEED_CREATE();
    d9c_on12_args a; memset(&a, 0, sizeof a);   /* Enable9On12 = FALSE, no D3D12 objects */
    IDirect3D9 *d = c(D3D_SDK_VERSION, &a, 1);
    CHECK(d != NULL);                          /* Win10: Enable9On12=FALSE falls back to the native d3d9 object */
    if (d) {
        CHECK(IDirect3D9_GetAdapterCount(d) >= 1);
        IDirect3D9_Release(d);
    }
}
TEST(d3d9_core_on12ex_null_args) {
    Direct3DCreate9On12Ex_pfn c = (Direct3DCreate9On12Ex_pfn)t_proc_in("d3d9.dll", "Direct3DCreate9On12Ex"); REQUIRE(c); NEED_CREATE();
    IDirect3D9Ex *d = NULL;
    HRESULT hr = c(D3D_SDK_VERSION, NULL, 0, &d);
    EXPECT_HR(hr, S_OK);
    CHECK(d != NULL);
    if (d) {
        void *p = NULL;
        EXPECT_HR(IDirect3D9Ex_QueryInterface(d, &IID_IDirect3D9Ex, &p), S_OK);
        if (p) IUnknown_Release((IUnknown *)p);
        EXPECT_EQ(IDirect3D9Ex_Release(d), 0);
    }
}
TEST(d3d9_core_on12ex_bad_version) {
    Direct3DCreate9On12Ex_pfn c = (Direct3DCreate9On12Ex_pfn)t_proc_in("d3d9.dll", "Direct3DCreate9On12Ex"); REQUIRE(c); NEED_CREATE();
    static const UINT v[] = { 0, 30, 33, 131 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        IDirect3D9Ex *d = (IDirect3D9Ex *)(ULONG_PTR)0x30;
        HRESULT hr = c(v[i], NULL, 0, &d);
        CHECKF(hr == D9_NOTAVAILABLE, "version %u hr %s", v[i], d3dh_hr_name(hr));
        CHECKF(d == NULL, "version %u out %p", v[i], (void *)d);
    }
}
TEST(d3d9_core_on12_same_adapter_count_as_plain) {
    Direct3DCreate9On12_pfn c = (Direct3DCreate9On12_pfn)t_proc_in("d3d9.dll", "Direct3DCreate9On12"); REQUIRE(c); NEED_CREATE();
    D9_GET(a);
    IDirect3D9 *b = c(D3D_SDK_VERSION, NULL, 0);
    CHECK(b != NULL);
    if (b) { EXPECT_EQ(IDirect3D9_GetAdapterCount(b), IDirect3D9_GetAdapterCount(a)); IDirect3D9_Release(b); }
    IDirect3D9_Release(a);
}
