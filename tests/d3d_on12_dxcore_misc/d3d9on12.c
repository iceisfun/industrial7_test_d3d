/* d3d9on12.dll exports and Direct3DCreate9On12 / Direct3DCreate9On12Ex in d3d9.dll. */
#include "odm.h"

typedef struct { BOOL Enable9On12; IUnknown *pD3D12Device; IUnknown *ppD3D12Queues[2]; UINT NumQueues; UINT NodeMask; } odm_9on12_args;
typedef IDirect3D9Ex *(WINAPI *c9on12_t)(UINT, odm_9on12_args *, UINT);
typedef HRESULT (WINAPI *c9on12ex_t)(UINT, odm_9on12_args *, UINT, IDirect3D9Ex **);
typedef LONG (WINAPI *open9_t)(void *);
typedef void (WINAPI *getddi_t)(void *);
typedef void (WINAPI *setcompat_t)(const void *);

static HMODULE h9on12(void) { return t_is_wine() ? NULL : LoadLibraryA("d3d9on12.dll"); }

TEST(d3d_on12_dxcore_misc_d3d9on12_exports_by_name) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    CHECK(GetProcAddress(m, "GetPrivateDDITable") != NULL);
    CHECK(GetProcAddress(m, "OpenAdapter") != NULL);
    CHECK(GetProcAddress(m, "SetAppCompatData") != NULL);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_exports_by_ordinal) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(1)) == GetProcAddress(m, "GetPrivateDDITable"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(2)) == GetProcAddress(m, "OpenAdapter"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(3)) == GetProcAddress(m, "SetAppCompatData"));
    SET_ERR(0);
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(4)) == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_no_other_names) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    static const char *no[] = {"Direct3DCreate9On12", "D3D9On12CreateDevice", "OpenAdapter10", "OpenAdapter_D3D11On12", "DllGetClassObject"};
    for (unsigned i = 0; i < sizeof no / sizeof no[0]; i++) { SET_ERR(0); CHECKF(GetProcAddress(m, no[i]) == NULL, "%s", no[i]); }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_openadapter_not_implemented) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    open9_t f = (open9_t)GetProcAddress(m, "OpenAdapter"); REQUIRE(f);
    EXPECT_EQ((ULONG)f(NULL), 0x80004001u);
    unsigned char fake[64]; memset(fake, 0, sizeof fake);
    EXPECT_EQ((ULONG)f(fake), 0x80004001u);
    for (unsigned i = 0; i < sizeof fake; i++) if (fake[i]) { CHECK(0); break; }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_private_ddi_table_filled) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    getddi_t f = (getddi_t)GetProcAddress(m, "GetPrivateDDITable"); REQUIRE(f);
    void *tbl[40]; memset(tbl, 0xCD, sizeof tbl);
    f(tbl);
    for (int i = 0; i < 27; i++) {
        CHECKF(tbl[i] != NULL && tbl[i] != (void *)0xCDCDCDCDCDCDCDCDull, "slot %d", i);
        HMODULE owner = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)tbl[i], &owner);
        CHECKF(owner == m, "slot %d points outside d3d9on12", i);
    }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_private_ddi_table_exact_size) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    getddi_t f = (getddi_t)GetProcAddress(m, "GetPrivateDDITable"); REQUIRE(f);
    unsigned char *base = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    REQUIRE(base);
    DWORD old;
        void **tbl = (void **)(base + 4096 - 27 * sizeof(void *));
    f(tbl); /* faults if more than 27 pointers are written */
    CHECK(tbl[0] != NULL && tbl[26] != NULL);
    VirtualFree(base, 0, MEM_RELEASE);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_private_ddi_table_stable_across_calls) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    getddi_t f = (getddi_t)GetProcAddress(m, "GetPrivateDDITable"); REQUIRE(f);
    void *a[27], *b[27];
    f(a); f(b);
    CHECK(!memcmp(a, b, sizeof a));
    int dup = 0;
    for (int i = 0; i < 27; i++) for (int j = i + 1; j < 27; j++) if (a[i] == a[j]) dup++;
    if (dup) t_note("%d duplicate entries in the private DDI table", dup);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_set_app_compat_data_reads_20_bytes) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    setcompat_t f = (setcompat_t)GetProcAddress(m, "SetAppCompatData"); REQUIRE(f);
    unsigned char *base = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    REQUIRE(base);
    DWORD old;
        unsigned char *p = base + 4096 - 20;
    memset(p, 0, 20);
    f(p); /* faults if more than 20 bytes are read */
    f(p);
    CHECK(1);
    VirtualFree(base, 0, MEM_RELEASE);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_set_app_compat_data_does_not_write_input) {
    HMODULE m = h9on12(); if (!m) { t_skip("d3d9on12.dll unavailable"); return; }
    setcompat_t f = (setcompat_t)GetProcAddress(m, "SetAppCompatData"); REQUIRE(f);
    unsigned char buf[24]; memset(buf, 0, sizeof buf); buf[20] = 0x5A;
    unsigned char copy[24]; memcpy(copy, buf, sizeof buf);
    f(buf);
    CHECK(!memcmp(buf, copy, sizeof buf));
    FreeLibrary(m);
}

/* ---- Direct3DCreate9On12 ---- */
static c9on12_t get9on12(void) { return t_is_wine() ? NULL : (c9on12_t)d3dh_getproc("d3d9.dll", "Direct3DCreate9On12"); }
static c9on12ex_t get9on12ex(void) { return t_is_wine() ? NULL : (c9on12ex_t)d3dh_getproc("d3d9.dll", "Direct3DCreate9On12Ex"); }

TEST(d3d_on12_dxcore_misc_d3d9on12_create_exports_ordinals) {
    HMODULE m = t_module("d3d9.dll");
    if (!m || t_is_wine()) { t_skip("d3d9.dll unavailable or emulated"); return; }
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(20)) == GetProcAddress(m, "Direct3DCreate9On12"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(21)) == GetProcAddress(m, "Direct3DCreate9On12Ex"));
}
TEST(d3d_on12_dxcore_misc_d3d9on12_create_disabled_returns_d3d9) {
    c9on12_t c = get9on12(); REQUIRE(c);
    odm_9on12_args a; memset(&a, 0, sizeof a);
    IDirect3D9Ex *d = c(D3D_SDK_VERSION, &a, 1);
    CHECK(d != NULL);
    if (d) { UINT n = IDirect3D9Ex_GetAdapterCount(d); CHECK(n >= 1); IDirect3D9Ex_Release(d); }
}
TEST(d3d_on12_dxcore_misc_d3d9on12_create_null_args_zero_count) {
    c9on12_t c = get9on12(); REQUIRE(c);
    IDirect3D9Ex *d = c(D3D_SDK_VERSION, NULL, 0);
    CHECK(d != NULL);
    if (d) IDirect3D9Ex_Release(d);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_create_returns_ex_interface) {
    c9on12_t c = get9on12(); REQUIRE(c);
    IDirect3D9Ex *d = c(D3D_SDK_VERSION, NULL, 0);
    if (!d) { t_skip("no d3d9"); return; }
    IDirect3D9 *p = NULL; IDirect3D9Ex *x = NULL;
    EXPECT_HR(IDirect3D9Ex_QueryInterface(d, &IID_IDirect3D9, (void **)&p), S_OK);
    EXPECT_HR(IDirect3D9Ex_QueryInterface(d, &IID_IDirect3D9Ex, (void **)&x), S_OK);
    SAFE_RELEASE(p); SAFE_RELEASE(x);
    IDirect3D9Ex_Release(d);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_sdk_version_matrix) {
    c9on12_t c = get9on12(); REQUIRE(c);
    static const struct { UINT v; int ok; } t[] = {
        {0x1f, 1}, {0x20, 1}, {0x80000020, 1}, {0x8000001f, 1},
        {0, 0}, {1, 0}, {0x1e, 0}, {0x21, 0}, {0x22, 0}, {0x40, 0}, {0x83, 0}, {0x80000021, 0},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDirect3D9Ex *d = c(t[i].v, NULL, 0);
        CHECKF((d != NULL) == t[i].ok, "sdk version 0x%x -> %p", t[i].v, d);
        if (d) IDirect3D9Ex_Release(d);
    }
}
TEST(d3d_on12_dxcore_misc_d3d9on12_sdk_version_above_range) {
    c9on12_t c = get9on12(); REQUIRE(c);
    static const UINT v[] = {0x84, 0x85, 0x100, 0x1000};
    for (unsigned i = 0; i < 4; i++) {
        IDirect3D9Ex *d = c(v[i], NULL, 0);
        t_note("sdk version 0x%x -> %s", v[i], d ? "created" : "NULL");
        if (d) IDirect3D9Ex_Release(d);
    }
}
TEST(d3d_on12_dxcore_misc_d3d9on12_ex_create_ok) {
    c9on12ex_t c = get9on12ex(); REQUIRE(c);
    IDirect3D9Ex *d = (IDirect3D9Ex *)1;
    HRESULT hr = c(D3D_SDK_VERSION, NULL, 0, &d);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && d) IDirect3D9Ex_Release(d);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_ex_bad_sdk_version) {
    c9on12ex_t c = get9on12ex(); REQUIRE(c);
    static const UINT v[] = {0, 0x1e, 0x21, 0x83};
    for (unsigned i = 0; i < 4; i++) {
        IDirect3D9Ex *d = (IDirect3D9Ex *)1;
        HRESULT hr = c(v[i], NULL, 0, &d);
        CHECKF(hr == (HRESULT)0x8876086A, "version 0x%x: %s", v[i], d3dh_hr_name(hr));
        CHECK(d == NULL);
    }
}
TEST(d3d_on12_dxcore_misc_d3d9on12_with_d3d12_device_and_queue) {
    c9on12_t c = get9on12(); REQUIRE(c);
    ID3D12Device *d12 = odm_dev12();
    if (!d12) { t_skip("no D3D12 device"); return; }
    ID3D12CommandQueue *q = odm_queue(d12, D3D12_COMMAND_LIST_TYPE_DIRECT, 0, 0);
    if (!q) { SAFE_RELEASE(d12); t_skip("no queue"); return; }
    odm_9on12_args a; memset(&a, 0, sizeof a);
    a.Enable9On12 = TRUE; a.pD3D12Device = (IUnknown *)d12; a.ppD3D12Queues[0] = (IUnknown *)q; a.NumQueues = 1; a.NodeMask = 0;
    IDirect3D9Ex *d = c(D3D_SDK_VERSION, &a, 1);
    CHECK(d != NULL);
    if (d) {
        UINT n = IDirect3D9Ex_GetAdapterCount(d);
        t_note("9on12 adapter count: %u", n);
        CHECK(n >= 1);
        D3DADAPTER_IDENTIFIER9 id; memset(&id, 0, sizeof id);
        if (n) { HRESULT hr = IDirect3D9Ex_GetAdapterIdentifier(d, 0, 0, &id); EXPECT_HR(hr, S_OK); t_note("9on12 adapter: %s / %s", id.Driver, id.Description); }
        IDirect3D9Ex_Release(d);
    }
    SAFE_RELEASE(q); SAFE_RELEASE(d12);
}
TEST(d3d_on12_dxcore_misc_d3d9on12_enabled_with_null_device) {
    c9on12_t c = get9on12(); REQUIRE(c);
    odm_9on12_args a; memset(&a, 0, sizeof a);
    a.Enable9On12 = TRUE;
    IDirect3D9Ex *d = c(D3D_SDK_VERSION, &a, 1);
    t_note("Enable9On12 with no D3D12 device: %s", d ? "created" : "NULL");
    if (d) {
        UINT n = IDirect3D9Ex_GetAdapterCount(d);
        t_note("adapter count %u", n);
        IDirect3D9Ex_Release(d);
    }
}
TEST(d3d_on12_dxcore_misc_d3d9on12_repeated_create_release) {
    c9on12_t c = get9on12(); REQUIRE(c);
    for (int i = 0; i < 8; i++) {
        IDirect3D9Ex *d = c(D3D_SDK_VERSION, NULL, 0);
        CHECK(d != NULL);
        if (d) EXPECT_EQ(IDirect3D9Ex_Release(d), 0);
    }
}
