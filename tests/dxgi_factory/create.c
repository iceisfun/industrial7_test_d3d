/* CreateDXGIFactory / 1 / 2: argument handling, riid matrix, QueryInterface chain, identity, refcounts. */
#include "fh.h"

typedef struct { const char *name; const IID *iid; int level; /* 0 base, N = IDXGIFactoryN; -1 other */ int hard; } iid_ent;
static const iid_ent k_good[] = {
    { "IUnknown",      &IID_IUnknown,      -1, 1 },
    { "IDXGIObject",   &IID_IDXGIObject,   -1, 1 },
    { "IDXGIFactory",  &IID_IDXGIFactory,   0, 1 },
    { "IDXGIFactory1", &IID_IDXGIFactory1,  1, 1 },
    { "IDXGIFactory2", &IID_IDXGIFactory2,  2, 1 },
    { "IDXGIFactory3", &IID_IDXGIFactory3,  3, 1 },
    { "IDXGIFactory4", &IID_IDXGIFactory4,  4, 1 },
    { "IDXGIFactory5", &IID_IDXGIFactory5,  5, 0 },
    { "IDXGIFactory6", &IID_IDXGIFactory6,  6, 0 },
    { "IDXGIFactory7", &IID_IDXGIFactory7,  7, 0 },
};
#define NGOOD (sizeof(k_good) / sizeof(k_good[0]))

static const iid_ent k_bad[] = {
    { "IDXGIDevice",     &IID_IDXGIDevice, -1, 1 },
    { "IDXGIAdapter",    &IID_IDXGIAdapter, -1, 1 },
    { "IDXGIAdapter1",   &IID_IDXGIAdapter1, -1, 1 },
    { "IDXGISwapChain",  &IID_IDXGISwapChain, -1, 1 },
    { "IDXGIOutput",     &IID_IDXGIOutput, -1, 1 },
    { "IDXGIDebug",      &IID_IDXGIDebug, -1, 1 },
    { "ID3D11Device",    &IID_ID3D11Device, -1, 1 },
    { "ID3D11DeviceContext", &IID_ID3D11DeviceContext, -1, 1 },
    { "IDXGIDeviceSubObject", &IID_IDXGIDeviceSubObject, -1, 1 },
    { "IDXGISurface",    &IID_IDXGISurface, -1, 1 },
    { "IDXGIResource",   &IID_IDXGIResource, -1, 1 },
    { "IDXGIKeyedMutex", &IID_IDXGIKeyedMutex, -1, 1 },
    { "IID_NULL",        &GUID_NULL, -1, 1 },
    { "IMalloc",         &IID_IMalloc, -1, 1 },
    { "IClassFactory",   &IID_IClassFactory, -1, 1 },
};
#define NBAD (sizeof(k_bad) / sizeof(k_bad[0]))

/* ---------------- CreateDXGIFactory ---------------- */
TEST(dxgi_factory_cf0_exports_present) {
    REQUIRE(fh_cf0());
    CHECK(fh_cf1() != NULL);
    CHECK(fh_cf2() != NULL);
}
TEST(dxgi_factory_cf0_unknown) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    IUnknown *u = POISON;
    EXPECT_HR(fh_cf0()(&IID_IUnknown, (void **)&u), S_OK);
    CHECK(u != NULL && u != POISON);
    if (u && u != POISON) EXPECT_EQ(u->lpVtbl->Release(u), 0);
}
TEST(dxgi_factory_cf0_object) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    IDXGIObject *o = POISON;
    EXPECT_HR(fh_cf0()(&IID_IDXGIObject, (void **)&o), S_OK);
    if (o && o != POISON) EXPECT_EQ(IDXGIObject_Release(o), 0);
}
TEST(dxgi_factory_cf0_factory) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    IDXGIFactory *f = POISON;
    EXPECT_HR(fh_cf0()(&IID_IDXGIFactory, (void **)&f), S_OK);
    CHECK(f != NULL && f != POISON);
    if (f && f != POISON) EXPECT_EQ(IDXGIFactory_Release(f), 0);
}
TEST(dxgi_factory_cf0_factory1_observed) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    /* The legacy entry point hands out a DXGI 1.0 factory; whether IDXGIFactory1 is exposed is observed only. */
    IUnknown *u = POISON;
    HRESULT hr = fh_cf0()(&IID_IDXGIFactory1, (void **)&u);
    t_note("CreateDXGIFactory(IID_IDXGIFactory1) = %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) u->lpVtbl->Release(u); else CHECK(u == NULL);
}
TEST(dxgi_factory_cf0_wrong_iid_nulls_out) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    for (size_t i = 0; i < NBAD; i++) {
        void *p = POISON;
        HRESULT hr = fh_cf0()(k_bad[i].iid, &p);
        CHECKF(hr == E_NOINTERFACE, "%s: got %s", k_bad[i].name, d3dh_hr_name(hr));
        CHECKF(p == NULL, "%s: out not cleared", k_bad[i].name);
    }
}
TEST(dxgi_factory_cf0_null_out) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    HRESULT hr = fh_cf0()(&IID_IDXGIFactory, NULL);
    CHECKF(fh_inv_ok(hr), "got %s", d3dh_hr_name(hr));
    t_note("CreateDXGIFactory(IDXGIFactory, NULL) = %s", d3dh_hr_name(hr));
}
TEST(dxgi_factory_cf0_null_out_wrong_iid) {
    REQUIRE(fh_cf0());
    HRESULT hr = fh_cf0()(&IID_IDXGIDevice, NULL);
    CHECK(FAILED(hr));
    t_note("CreateDXGIFactory(IDXGIDevice, NULL) = %s", d3dh_hr_name(hr));
}
TEST(dxgi_factory_cf0_distinct_objects_each_call) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    IDXGIFactory *a = NULL, *b = NULL;
    EXPECT_HR(fh_cf0()(&IID_IDXGIFactory, (void **)&a), S_OK);
    EXPECT_HR(fh_cf0()(&IID_IDXGIFactory, (void **)&b), S_OK);
    if (a && b) t_note("two CreateDXGIFactory calls %s", a == b ? "return the same object" : "return distinct objects");
    if (b && b != a) IDXGIFactory_Release(b);
    if (a) IDXGIFactory_Release(a);
}
TEST(dxgi_factory_cf0_refcount_roundtrip) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    IDXGIFactory *f = NULL;
    EXPECT_HR(fh_cf0()(&IID_IDXGIFactory, (void **)&f), S_OK);
    if (!f) return;
    EXPECT_EQ(IDXGIFactory_AddRef(f), 2);
    EXPECT_EQ(IDXGIFactory_AddRef(f), 3);
    EXPECT_EQ(IDXGIFactory_Release(f), 2);
    EXPECT_EQ(IDXGIFactory_Release(f), 1);
    EXPECT_EQ(IDXGIFactory_Release(f), 0);
}
TEST(dxgi_factory_cf0_qi_up_levels_observed) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    IDXGIFactory *f = NULL;
    EXPECT_HR(fh_cf0()(&IID_IDXGIFactory, (void **)&f), S_OK);
    if (!f) return;
    for (size_t i = 0; i < NGOOD; i++) {
        void *p = POISON;
        HRESULT hr = IDXGIFactory_QueryInterface(f, k_good[i].iid, &p);
        t_note("cf0 factory QI %s = %s", k_good[i].name, d3dh_hr_name(hr));
        if (FAILED(hr)) CHECKF(p == NULL, "%s: out not cleared on failure", k_good[i].name);
        else ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
    IDXGIFactory_Release(f);
}

/* ---------------- CreateDXGIFactory1 / 2 : riid matrix ---------------- */
static void riid_matrix(const char *api, int use2, UINT flags) {
    for (size_t i = 0; i < NGOOD; i++) {
        void *p = POISON;
        HRESULT hr = use2 ? fh_cf2()(flags, k_good[i].iid, &p) : fh_cf1()(k_good[i].iid, &p);
        if (k_good[i].hard) {
            CHECKF(hr == S_OK, "%s(%s) = %s", api, k_good[i].name, d3dh_hr_name(hr));
            CHECKF(SUCCEEDED(hr) ? (p != NULL && p != POISON) : p == NULL, "%s(%s) out pointer", api, k_good[i].name);
        } else if (FAILED(hr)) {
            t_note("%s(%s) = %s (not available on this build)", api, k_good[i].name, d3dh_hr_name(hr));
            CHECKF(hr == E_NOINTERFACE, "%s(%s) = %s", api, k_good[i].name, d3dh_hr_name(hr));
            CHECK(p == NULL);
        }
        if (SUCCEEDED(hr) && p && p != POISON) EXPECT_EQ(((IUnknown *)p)->lpVtbl->Release((IUnknown *)p), 0);
    }
}
static void bad_matrix(const char *api, int use2, UINT flags) {
    for (size_t i = 0; i < NBAD; i++) {
        void *p = POISON;
        HRESULT hr = use2 ? fh_cf2()(flags, k_bad[i].iid, &p) : fh_cf1()(k_bad[i].iid, &p);
        CHECKF(hr == E_NOINTERFACE, "%s(%s) = %s", api, k_bad[i].name, d3dh_hr_name(hr));
        CHECKF(p == NULL, "%s(%s): out not cleared", api, k_bad[i].name);
    }
}
TEST(dxgi_factory_cf1_riid_matrix) { WINE_NOFAC(); REQUIRE(fh_cf1()); riid_matrix("CreateDXGIFactory1", 0, 0); }
TEST(dxgi_factory_cf1_bad_riid_matrix) { WINE_NOFAC(); REQUIRE(fh_cf1()); bad_matrix("CreateDXGIFactory1", 0, 0); }
TEST(dxgi_factory_cf2_riid_matrix) { WINE_NOFAC(); REQUIRE(fh_cf2()); riid_matrix("CreateDXGIFactory2", 1, 0); }
TEST(dxgi_factory_cf2_bad_riid_matrix) { WINE_NOFAC(); REQUIRE(fh_cf2()); bad_matrix("CreateDXGIFactory2", 1, 0); }
TEST(dxgi_factory_cf1_null_out) {
    WINE_NOFAC();
    REQUIRE(fh_cf1());
    HRESULT hr = fh_cf1()(&IID_IDXGIFactory1, NULL);
    CHECKF(fh_inv_ok(hr), "got %s", d3dh_hr_name(hr));
    t_note("CreateDXGIFactory1(IDXGIFactory1, NULL) = %s", d3dh_hr_name(hr));
}
TEST(dxgi_factory_cf2_null_out) {
    WINE_NOFAC();
    REQUIRE(fh_cf2());
    HRESULT hr = fh_cf2()(0, &IID_IDXGIFactory2, NULL);
    CHECKF(fh_inv_ok(hr), "got %s", d3dh_hr_name(hr));
    t_note("CreateDXGIFactory2(0, IDXGIFactory2, NULL) = %s", d3dh_hr_name(hr));
}
TEST(dxgi_factory_cf2_null_out_all_iids) {
    REQUIRE(fh_cf2());
    for (size_t i = 0; i < NGOOD; i++) {
        HRESULT hr = fh_cf2()(0, k_good[i].iid, NULL);
        CHECKF(FAILED(hr), "%s with NULL out returned %s", k_good[i].name, d3dh_hr_name(hr));
    }
}
TEST(dxgi_factory_cf1_null_out_all_iids) {
    REQUIRE(fh_cf1());
    for (size_t i = 0; i < NGOOD; i++) {
        HRESULT hr = fh_cf1()(k_good[i].iid, NULL);
        CHECKF(FAILED(hr), "%s with NULL out returned %s", k_good[i].name, d3dh_hr_name(hr));
    }
}
TEST(dxgi_factory_cf1_refcount_roundtrip) {
    NEED_F1(f);
    EXPECT_EQ(IDXGIFactory1_AddRef(f), 2);
    EXPECT_EQ(IDXGIFactory1_Release(f), 1);
    EXPECT_EQ(IDXGIFactory1_Release(f), 0);
}
TEST(dxgi_factory_cf2_flag0_ok) {
    WINE_NOFAC();
    REQUIRE(fh_cf2());
    IDXGIFactory2 *f = NULL;
    EXPECT_HR(fh_cf2()(0, &IID_IDXGIFactory2, (void **)&f), S_OK);
    if (f) EXPECT_EQ(IDXGIFactory2_Release(f), 0);
}
TEST(dxgi_factory_cf2_debug_flag_observed) {
    WINE_NOFAC();
    REQUIRE(fh_cf2());
    void *p = POISON;
    HRESULT hr = fh_cf2()(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory3, &p);
    /* Without the Graphics Tools feature the debug DLL is absent; the call must still either succeed or fail cleanly. */
    t_note("CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG) = %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        CHECK(p != NULL && p != POISON);
        IDXGIFactory3 *f3 = p;
        if (p && p != POISON) {
            UINT fl = IDXGIFactory3_GetCreationFlags(f3);
            t_note("debug factory creation flags %#x", fl);
            if (!(fl & DXGI_CREATE_FACTORY_DEBUG)) t_note("debug flag not reflected in GetCreationFlags (debug layer absent?)");
            IDXGIFactory3_Release(f3);
        }
    } else {
        CHECK(p == NULL);
    }
}
TEST(dxgi_factory_cf2_debug_flag_riid_matrix) {
    REQUIRE(fh_cf2());
    /* If the debug factory is creatable the riid matrix must be unchanged. */
    void *p = NULL;
    if (FAILED(fh_cf2()(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory2, &p))) { t_skip("debug factory unavailable"); return; }
    ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    riid_matrix("CreateDXGIFactory2(DEBUG)", 1, DXGI_CREATE_FACTORY_DEBUG);
    bad_matrix("CreateDXGIFactory2(DEBUG)", 1, DXGI_CREATE_FACTORY_DEBUG);
}
TEST(dxgi_factory_cf2_unknown_flag_bits_observed) {
    WINE_NOFAC();
    REQUIRE(fh_cf2());
    static const UINT fl[] = { 0x2, 0x4, 0x80, 0x10000, 0x80000000u, 0xFFFFFFFEu };
    for (size_t i = 0; i < sizeof(fl) / sizeof(fl[0]); i++) {
        void *p = POISON;
        HRESULT hr = fh_cf2()(fl[i], &IID_IDXGIFactory2, &p);
        t_note("CreateDXGIFactory2(flags=%#x) = %s", fl[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p); else CHECK(p == NULL);
    }
}
TEST(dxgi_factory_cf2_wrong_iid_with_debug_flag) {
    WINE_NOFAC();
    REQUIRE(fh_cf2());
    void *p = POISON;
    HRESULT hr = fh_cf2()(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIDevice, &p);
    CHECKF(FAILED(hr), "got %s", d3dh_hr_name(hr));
    CHECK(p == NULL);
}

/* ---------------- GetCreationFlags ---------------- */
TEST(dxgi_factory_creation_flags_cf1_zero) {
    NEED_F1(f);
    IDXGIFactory3 *f3 = fh_qi((IUnknown *)f, &IID_IDXGIFactory3);
    if (!f3) { t_skip("no IDXGIFactory3"); IDXGIFactory1_Release(f); return; }
    EXPECT_EQ(IDXGIFactory3_GetCreationFlags(f3), 0);
    IDXGIFactory3_Release(f3);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_creation_flags_cf2_zero) {
    WINE_NOFAC();
    REQUIRE(fh_cf2());
    IDXGIFactory3 *f3 = NULL;
    EXPECT_HR(fh_cf2()(0, &IID_IDXGIFactory3, (void **)&f3), S_OK);
    if (!f3) return;
    EXPECT_EQ(IDXGIFactory3_GetCreationFlags(f3), 0);
    IDXGIFactory3_Release(f3);
}
TEST(dxgi_factory_creation_flags_cf0_zero) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    IDXGIFactory *f = NULL;
    EXPECT_HR(fh_cf0()(&IID_IDXGIFactory, (void **)&f), S_OK);
    if (!f) return;
    IDXGIFactory3 *f3 = fh_qi((IUnknown *)f, &IID_IDXGIFactory3);
    if (f3) { EXPECT_EQ(IDXGIFactory3_GetCreationFlags(f3), 0); IDXGIFactory3_Release(f3); }
    else t_note("cf0 factory does not expose IDXGIFactory3");
    IDXGIFactory_Release(f);
}

/* ---------------- QueryInterface chain / identity ---------------- */
TEST(dxgi_factory_qi_identity_all_levels) {
    NEED_F1(f);
    IUnknown *base = fh_ident((IUnknown *)f);
    CHECK(base != NULL);
    for (size_t i = 0; i < NGOOD; i++) {
        void *p = fh_qi((IUnknown *)f, k_good[i].iid);
        if (!p) { CHECKF(!k_good[i].hard, "%s unavailable", k_good[i].name); continue; }
        IUnknown *id = fh_ident((IUnknown *)p);
        CHECKF(id == base, "%s: IUnknown identity differs", k_good[i].name);
        if (id) id->lpVtbl->Release(id);
        ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
    if (base) base->lpVtbl->Release(base);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_qi_same_pointer_per_iid) {
    NEED_F1(f);
    for (size_t i = 0; i < NGOOD; i++) {
        void *a = fh_qi((IUnknown *)f, k_good[i].iid), *b = fh_qi((IUnknown *)f, k_good[i].iid);
        CHECKF(a == b, "%s: repeated QI returned different pointers", k_good[i].name);
        if (a) ((IUnknown *)a)->lpVtbl->Release((IUnknown *)a);
        if (b) ((IUnknown *)b)->lpVtbl->Release((IUnknown *)b);
    }
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_qi_refcount_shared_across_interfaces) {
    NEED_F1(f);
    IDXGIFactory2 *f2 = fh_qi((IUnknown *)f, &IID_IDXGIFactory2);
    if (!f2) { t_skip("no IDXGIFactory2"); IDXGIFactory1_Release(f); return; }
    EXPECT_EQ(fh_rc((IUnknown *)f), 2);
    EXPECT_EQ(IDXGIFactory2_Release(f2), 1);
    EXPECT_EQ(fh_rc((IUnknown *)f), 1);
    EXPECT_EQ(IDXGIFactory1_Release(f), 0);
}
TEST(dxgi_factory_qi_every_level_from_every_level) {
    for (size_t src = 2; src < NGOOD; src++) {
        void *s = NULL;
        if (FAILED(fh_make(k_good[src].iid, &s))) continue;
        for (size_t dst = 0; dst < NGOOD; dst++) {
            void *d = POISON;
            HRESULT hr = ((IUnknown *)s)->lpVtbl->QueryInterface((IUnknown *)s, k_good[dst].iid, &d);
            if (k_good[dst].level <= k_good[src].level || k_good[dst].level < 0)
                CHECKF(hr == S_OK, "QI %s -> %s = %s", k_good[src].name, k_good[dst].name, d3dh_hr_name(hr));
            if (SUCCEEDED(hr)) ((IUnknown *)d)->lpVtbl->Release((IUnknown *)d);
            else CHECK(d == NULL);
        }
        EXPECT_EQ(((IUnknown *)s)->lpVtbl->Release((IUnknown *)s), 0);
    }
}
TEST(dxgi_factory_qi_downward_consistency) {
    /* If a higher factory interface exists, all lower ones must too. */
    NEED_F1(f);
    int highest = 1;
    for (size_t i = 0; i < NGOOD; i++) {
        void *p = fh_qi((IUnknown *)f, k_good[i].iid);
        if (p) { if (k_good[i].level > highest) highest = k_good[i].level; ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p); }
    }
    t_note("highest IDXGIFactoryN exposed: %d", highest);
    CHECK(highest >= 4);
    for (size_t i = 0; i < NGOOD; i++) {
        if (k_good[i].level < 0 || k_good[i].level > highest) continue;
        void *p = fh_qi((IUnknown *)f, k_good[i].iid);
        CHECKF(p != NULL, "%s missing though level %d exposed", k_good[i].name, highest);
        if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    }
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_qi_bad_iids_clear_out) {
    NEED_F1(f);
    for (size_t i = 0; i < NBAD; i++) {
        void *p = POISON;
        HRESULT hr = IDXGIFactory1_QueryInterface(f, k_bad[i].iid, &p);
        CHECKF(hr == E_NOINTERFACE, "%s = %s", k_bad[i].name, d3dh_hr_name(hr));
        CHECKF(p == NULL, "%s: out not cleared", k_bad[i].name);
    }
    EXPECT_EQ(IDXGIFactory1_Release(f), 0);
}
TEST(dxgi_factory_qi_failure_keeps_refcount) {
    NEED_F1(f);
    void *p = NULL;
    IDXGIFactory1_QueryInterface(f, &IID_IDXGIDevice, &p);
    EXPECT_EQ(fh_rc((IUnknown *)f), 1);
    EXPECT_EQ(IDXGIFactory1_Release(f), 0);
}
TEST(dxgi_factory_qi_media_factory_observed) {
    NEED_F1(f);
    void *p = POISON;
    HRESULT hr = IDXGIFactory1_QueryInterface(f, &IID_IDXGIFactoryMedia, &p);
    t_note("QI IDXGIFactoryMedia = %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p); else CHECK(p == NULL);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_qi_interleaved_releases) {
    NEED_F1(f);
    IDXGIFactory2 *a = fh_qi((IUnknown *)f, &IID_IDXGIFactory2);
    IDXGIFactory3 *b = fh_qi((IUnknown *)f, &IID_IDXGIFactory3);
    IDXGIFactory4 *c = fh_qi((IUnknown *)f, &IID_IDXGIFactory4);
    ULONG held = 1 + (a != 0) + (b != 0) + (c != 0);
    EXPECT_EQ(fh_rc((IUnknown *)f), held);
    if (b) { IDXGIFactory3_Release(b); held--; }
    IDXGIFactory1_Release(f); held--;
    if (a) { EXPECT_EQ(fh_rc((IUnknown *)a), held); }
    if (c) { IDXGIFactory4_Release(c); held--; }
    if (a) EXPECT_EQ(IDXGIFactory2_Release(a), 0);
}

/* ---------------- IDXGIObject on the factory ---------------- */
static const GUID k_pd = { 0x1d6a2c10, 0x7e4f, 0x4c1e, { 0x9a, 0x21, 0x5f, 0x7d, 0x30, 0xee, 0x11, 0x01 } };
static const GUID k_pd2 = { 0x1d6a2c10, 0x7e4f, 0x4c1e, { 0x9a, 0x21, 0x5f, 0x7d, 0x30, 0xee, 0x11, 0x02 } };

TEST(dxgi_factory_private_data_roundtrip) {
    NEED_F1(f);
    static const BYTE src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    EXPECT_HR(IDXGIFactory1_SetPrivateData(f, &k_pd, sizeof(src), src), S_OK);
    BYTE dst[8] = { 0 }; UINT sz = sizeof(dst);
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, dst), S_OK);
    EXPECT_EQ(sz, 8);
    CHECK(!memcmp(src, dst, 8));
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_unknown_guid) {
    NEED_F1(f);
    BYTE dst[8]; UINT sz = sizeof(dst);
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd2, &sz, dst), DXGI_E_NOT_FOUND);
    EXPECT_EQ(sz, 0);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_buffer_protocol) {
    NEED_F1(f);
    static const BYTE src[8] = { 9, 8, 7, 6, 5, 4, 3, 2 };
    IDXGIFactory1_SetPrivateData(f, &k_pd, sizeof(src), src);
    BYTE dst[16]; UINT sz;
    sz = 0; EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, NULL), S_OK); EXPECT_EQ(sz, 8);
    sz = 4; memset(dst, 0xAA, sizeof(dst));
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, dst), DXGI_E_MORE_DATA); EXPECT_EQ(sz, 8);
    sz = 7; EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, dst), DXGI_E_MORE_DATA); EXPECT_EQ(sz, 8);
    sz = 8; EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, dst), S_OK); EXPECT_EQ(sz, 8);
    sz = 16; memset(dst, 0xAA, sizeof(dst));
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, dst), S_OK); EXPECT_EQ(sz, 8);
    CHECK(!memcmp(dst, src, 8) && dst[8] == 0xAA);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_overwrite_and_remove) {
    NEED_F1(f);
    static const BYTE a[4] = { 1, 1, 1, 1 }, b[2] = { 2, 2 };
    IDXGIFactory1_SetPrivateData(f, &k_pd, 4, a);
    EXPECT_HR(IDXGIFactory1_SetPrivateData(f, &k_pd, 2, b), S_OK);
    BYTE d[8]; UINT sz = 8;
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, d), S_OK); EXPECT_EQ(sz, 2);
    EXPECT_HR(IDXGIFactory1_SetPrivateData(f, &k_pd, 0, NULL), S_OK);
    sz = 8; EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, d), DXGI_E_NOT_FOUND);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_two_guids_independent) {
    NEED_F1(f);
    BYTE a = 0x11, b = 0x22, d = 0; UINT sz = 1;
    IDXGIFactory1_SetPrivateData(f, &k_pd, 1, &a);
    IDXGIFactory1_SetPrivateData(f, &k_pd2, 1, &b);
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, &d), S_OK); EXPECT_EQ(d, 0x11);
    sz = 1; EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd2, &sz, &d), S_OK); EXPECT_EQ(d, 0x22);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_null_data_nonzero_size) {
    NEED_F1(f);
    HRESULT hr = IDXGIFactory1_SetPrivateData(f, &k_pd, 4, NULL);
    CHECKF(hr == E_INVALIDARG || hr == DXGI_E_INVALID_CALL || hr == E_POINTER, "got %s", d3dh_hr_name(hr));
    t_note("SetPrivateData(size 4, NULL) = %s", d3dh_hr_name(hr));
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_interface_roundtrip) {
    NEED_F1(f);
    IDXGIFactory1 *held = NULL;
    EXPECT_HR(fh_make(&IID_IDXGIFactory1, (void **)&held), S_OK);
    if (!held) { IDXGIFactory1_Release(f); return; }
    EXPECT_HR(IDXGIFactory1_SetPrivateDataInterface(f, &k_pd, (IUnknown *)held), S_OK);
    EXPECT_EQ(fh_rc((IUnknown *)held), 2);
    IUnknown *got = NULL; UINT sz = sizeof(got);
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, &got), S_OK);
    EXPECT_EQ(sz, sizeof(IUnknown *));
    CHECK(got == (IUnknown *)held);
    EXPECT_EQ(fh_rc((IUnknown *)held), 3);  /* GetPrivateData AddRefs the stored interface */
    if (got) got->lpVtbl->Release(got);
    IDXGIFactory1_SetPrivateData(f, &k_pd, 0, NULL);
    EXPECT_EQ(fh_rc((IUnknown *)held), 1);
    IDXGIFactory1_Release(held);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_interface_released_with_factory) {
    NEED_F1(f);
    IDXGIFactory1 *held = fh_f1();
    if (!held) { IDXGIFactory1_Release(f); t_skip("second factory"); return; }
    IDXGIFactory1_SetPrivateDataInterface(f, &k_pd, (IUnknown *)held);
    EXPECT_EQ(fh_rc((IUnknown *)held), 2);
    EXPECT_EQ(IDXGIFactory1_Release(f), 0);
    EXPECT_EQ(fh_rc((IUnknown *)held), 1);
    IDXGIFactory1_Release(held);
}
TEST(dxgi_factory_private_data_interface_replaced) {
    NEED_F1(f);
    IDXGIFactory1 *held = fh_f1();
    if (!held) { IDXGIFactory1_Release(f); t_skip("second factory"); return; }
    IDXGIFactory1_SetPrivateDataInterface(f, &k_pd, (IUnknown *)held);
    BYTE b = 3;
    IDXGIFactory1_SetPrivateData(f, &k_pd, 1, &b);
    EXPECT_EQ(fh_rc((IUnknown *)held), 1);
    IDXGIFactory1_Release(held);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_interface_null_removes) {
    NEED_F1(f);
    BYTE b = 3; UINT sz = 1;
    IDXGIFactory1_SetPrivateData(f, &k_pd, 1, &b);
    HRESULT hr = IDXGIFactory1_SetPrivateDataInterface(f, &k_pd, NULL);
    t_note("SetPrivateDataInterface(NULL) = %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        hr = IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, &b);
        t_note("GetPrivateData after NULL interface = %s size %u", d3dh_hr_name(hr), sz);
    }
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_large_blob) {
    NEED_F1(f);
    BYTE *big = malloc(65536), *back = malloc(65536);
    for (int i = 0; i < 65536; i++) big[i] = (BYTE)(i * 7 + 1);
    EXPECT_HR(IDXGIFactory1_SetPrivateData(f, &k_pd, 65536, big), S_OK);
    UINT sz = 65536;
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, back), S_OK);
    EXPECT_EQ(sz, 65536);
    CHECK(!memcmp(big, back, 65536));
    free(big); free(back);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_per_object) {
    NEED_F1(f);
    IDXGIFactory1 *g = fh_f1();
    BYTE b = 5, d; UINT sz = 1;
    IDXGIFactory1_SetPrivateData(f, &k_pd, 1, &b);
    if (g) {
        HRESULT hr = IDXGIFactory1_GetPrivateData(g, &k_pd, &sz, &d);
        t_note("private data visible from a second factory object: %s", d3dh_hr_name(hr));
        IDXGIFactory1_Release(g);
    }
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_private_data_shared_across_iface_views) {
    NEED_F1(f);
    IDXGIFactory2 *f2 = fh_qi((IUnknown *)f, &IID_IDXGIFactory2);
    if (!f2) { t_skip("no IDXGIFactory2"); IDXGIFactory1_Release(f); return; }
    BYTE b = 0x42, d = 0; UINT sz = 1;
    EXPECT_HR(IDXGIFactory2_SetPrivateData(f2, &k_pd, 1, &b), S_OK);
    EXPECT_HR(IDXGIFactory1_GetPrivateData(f, &k_pd, &sz, &d), S_OK);
    EXPECT_EQ(d, 0x42);
    IDXGIFactory2_Release(f2);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_get_parent_observed) {
    NEED_F1(f);
    void *p = POISON;
    HRESULT hr = IDXGIFactory1_GetParent(f, &IID_IUnknown, &p);
    t_note("Factory::GetParent(IUnknown) = %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(p == NULL);
    IDXGIFactory1_Release(f);
}
TEST(dxgi_factory_get_parent_null_out) {
    NEED_F1(f);
    HRESULT hr = IDXGIFactory1_GetParent(f, &IID_IUnknown, NULL);
    CHECK(FAILED(hr));
    t_note("Factory::GetParent(NULL out) = %s", d3dh_hr_name(hr));
    IDXGIFactory1_Release(f);
}

/* ---------------- many creations ---------------- */
TEST(dxgi_factory_create_release_loop) {
    WINE_NOFAC();
    REQUIRE(fh_cf1());
    for (int i = 0; i < 200; i++) {
        IDXGIFactory1 *f = NULL;
        HRESULT hr = fh_cf1()(&IID_IDXGIFactory1, (void **)&f);
        if (hr != S_OK) { CHECKF(0, "iteration %d: %s", i, d3dh_hr_name(hr)); break; }
        if (IDXGIFactory1_Release(f) != 0) { CHECKF(0, "iteration %d leaked", i); break; }
    }
}
TEST(dxgi_factory_many_alive_at_once) {
    WINE_NOFAC();
    REQUIRE(fh_cf2());
    IDXGIFactory2 *f[32] = { 0 };
    int n = 0;
    for (; n < 32; n++) if (FAILED(fh_cf2()(0, &IID_IDXGIFactory2, (void **)&f[n]))) break;
    EXPECT_EQ(n, 32);
    for (int i = 0; i < n; i++) EXPECT_EQ(IDXGIFactory2_Release(f[i]), 0);
}
TEST(dxgi_factory_mixed_entry_points_coexist) {
    WINE_NOFAC();
    REQUIRE(fh_cf0());
    IDXGIFactory *a = NULL; IDXGIFactory1 *b = NULL; IDXGIFactory2 *c = NULL;
    HRESULT h0 = fh_cf0()(&IID_IDXGIFactory, (void **)&a);
    HRESULT h1 = fh_cf1() ? fh_cf1()(&IID_IDXGIFactory1, (void **)&b) : E_FAIL;
    HRESULT h2 = fh_cf2() ? fh_cf2()(0, &IID_IDXGIFactory2, (void **)&c) : E_FAIL;
    EXPECT_HR(h0, S_OK); EXPECT_HR(h1, S_OK); EXPECT_HR(h2, S_OK);
    if (a) IDXGIFactory_Release(a);
    if (b) IDXGIFactory1_Release(b);
    if (c) IDXGIFactory2_Release(c);
}
TEST(dxgi_factory_cf2_vs_cf1_same_adapter_set) {
    IDXGIFactory1 *a = fh_f1();
    IDXGIFactory2 *b = NULL;
    if (!a || FAILED(fh_make(&IID_IDXGIFactory2, (void **)&b)) || !b) { t_skip("factories"); if (a) IDXGIFactory1_Release(a); return; }
    UINT na = 0, nb = 0;
    IDXGIAdapter1 *ad;
    while (SUCCEEDED(IDXGIFactory1_EnumAdapters1(a, na, &ad)) && na < 64) { IDXGIAdapter1_Release(ad); na++; }
    IDXGIFactory1 *b1 = fh_qi((IUnknown *)b, &IID_IDXGIFactory1);
    while (b1 && SUCCEEDED(IDXGIFactory1_EnumAdapters1(b1, nb, &ad)) && nb < 64) { IDXGIAdapter1_Release(ad); nb++; }
    EXPECT_EQ(na, nb);
    if (b1) IDXGIFactory1_Release(b1);
    IDXGIFactory2_Release(b);
    IDXGIFactory1_Release(a);
}
