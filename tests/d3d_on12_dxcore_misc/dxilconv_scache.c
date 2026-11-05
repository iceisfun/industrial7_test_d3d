/* dxilconv.dll (DXBC->DXIL converter factory) and d3dscache.dll (shader cache). */
#include "odm.h"

/* ---------------- dxilconv ---------------- */
static const GUID odm_CLSID_DxbcConverter = {0x4900391e, 0xb752, 0x4edd, {0xa8, 0x85, 0x6f, 0xb7, 0x6e, 0x25, 0xad, 0x6d}};
typedef HRESULT (WINAPI *dxc_create_t)(const GUID *, const GUID *, void **);
typedef HRESULT (WINAPI *dxc_create2_t)(IMalloc *, const GUID *, const GUID *, void **);

static HMODULE h_dxilconv(void) { return t_is_wine() ? NULL : LoadLibraryA("dxilconv.dll"); }

typedef struct { IMalloc iface; LONG ref, allocs, frees, reallocs; } odm_malloc;
static HRESULT WINAPI om_qi(IMalloc *s, REFIID r, void **o) { if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IMalloc)) { *o = s; s->lpVtbl->AddRef(s); return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI om_addref(IMalloc *s) { return InterlockedIncrement(&((odm_malloc *)s)->ref); }
static ULONG WINAPI om_release(IMalloc *s) { return InterlockedDecrement(&((odm_malloc *)s)->ref); }
static void *WINAPI om_alloc(IMalloc *s, SIZE_T n) { InterlockedIncrement(&((odm_malloc *)s)->allocs); return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }
static void *WINAPI om_realloc(IMalloc *s, void *p, SIZE_T n) { InterlockedIncrement(&((odm_malloc *)s)->reallocs); if (!p) return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); if (!n) { HeapFree(GetProcessHeap(), 0, p); return NULL; } return HeapReAlloc(GetProcessHeap(), 0, p, n); }
static void WINAPI om_free(IMalloc *s, void *p) { if (p) { InterlockedIncrement(&((odm_malloc *)s)->frees); HeapFree(GetProcessHeap(), 0, p); } }
static SIZE_T WINAPI om_getsize(IMalloc *s, void *p) { return p ? HeapSize(GetProcessHeap(), 0, p) : (SIZE_T)-1; }
static int WINAPI om_didalloc(IMalloc *s, void *p) { return -1; }
static void WINAPI om_heapmin(IMalloc *s) {}
static IMallocVtbl om_vtbl = {om_qi, om_addref, om_release, om_alloc, om_realloc, om_free, om_getsize, om_didalloc, om_heapmin};
static void om_init(odm_malloc *m) { memset(m, 0, sizeof *m); m->iface.lpVtbl = &om_vtbl; m->ref = 1; }

TEST(d3d_on12_dxcore_misc_dxilconv_exports) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    CHECK(GetProcAddress(m, "DxcCreateInstance") != NULL);
    CHECK(GetProcAddress(m, "DxcCreateInstance2") != NULL);
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(2)) == GetProcAddress(m, "DxcCreateInstance"));
    CHECK(GetProcAddress(m, MAKEINTRESOURCEA(1)) == GetProcAddress(m, "DxcCreateInstance2"));
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_only_converter_exposed) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    static const char *names[] = {"DxcCreateInstance3", "DxilConvertDxbc", "DxcCreateCompiler", "ConvertDxbcToDxil"};
    for (unsigned i = 0; i < 4; i++) { SET_ERR(0); CHECKF(GetProcAddress(m, names[i]) == NULL, "%s", names[i]); }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_create_converter) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create_t f = (dxc_create_t)GetProcAddress(m, "DxcCreateInstance"); REQUIRE(f);
    void *o = (void *)1;
    HRESULT hr = f(&odm_CLSID_DxbcConverter, &IID_IUnknown, &o);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) { CHECK(o != NULL && o != (void *)1); EXPECT_EQ(odm_release(o), 0); }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_unknown_clsid) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create_t f = (dxc_create_t)GetProcAddress(m, "DxcCreateInstance"); REQUIRE(f);
    static const GUID bogus = {0x12345678, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}};
    void *o = (void *)1;
    EXPECT_HR(f(&bogus, &IID_IUnknown, &o), (HRESULT)0x80040154);
    CHECK(o == NULL);
    o = (void *)1;
    EXPECT_HR(f(&IID_IUnknown, &IID_IUnknown, &o), (HRESULT)0x80040154);
    CHECK(o == NULL);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_other_dxc_clsids_not_available) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create_t f = (dxc_create_t)GetProcAddress(m, "DxcCreateInstance"); REQUIRE(f);
    /* CLSID_DxcCompiler, CLSID_DxcLibrary, CLSID_DxcValidator are served by dxcompiler.dll, not by the converter */
    static const GUID c[3] = {
        {0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}},
        {0x6245d6af, 0x66e0, 0x48fd, {0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c}},
        {0x8ca3e215, 0xf728, 0x4cf3, {0x8c, 0xdd, 0x88, 0xaf, 0x91, 0x75, 0x87, 0xa1}},
    };
    for (int i = 0; i < 3; i++) { void *o = (void *)1; EXPECT_HR(f(&c[i], &IID_IUnknown, &o), (HRESULT)0x80040154); CHECK(o == NULL); }
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_unknown_iid) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create_t f = (dxc_create_t)GetProcAddress(m, "DxcCreateInstance"); REQUIRE(f);
    void *o = (void *)1;
    EXPECT_HR(f(&odm_CLSID_DxbcConverter, &DXC_ATTR_BOGUS, &o), E_NOIFACE);
    CHECK(o == NULL);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_refcount_and_qi) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create_t f = (dxc_create_t)GetProcAddress(m, "DxcCreateInstance"); REQUIRE(f);
    void *o = NULL;
    if (FAILED(f(&odm_CLSID_DxbcConverter, &IID_IUnknown, &o)) || !o) { FreeLibrary(m); t_skip("create failed"); return; }
    ULONG a = odm_addref(o), b = odm_addref(o);
    CHECK(b == a + 1);
    odm_release(o); odm_release(o);
    void *p = NULL;
    EXPECT_HR(odm_qi(o, &IID_IUnknown, &p), S_OK);
    CHECK(p == o);
    ODM_REL(p);
    p = (void *)1;
    EXPECT_HR(odm_qi(o, &DXC_ATTR_BOGUS, &p), E_NOIFACE);
    CHECK(p == NULL);
    odm_release(o);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_instance2_null_out) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create2_t f = (dxc_create2_t)GetProcAddress(m, "DxcCreateInstance2"); REQUIRE(f);
    EXPECT_HR(f(NULL, &odm_CLSID_DxbcConverter, &IID_IUnknown, NULL), E_PTR);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_instance2_unknown_clsid) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create2_t f = (dxc_create2_t)GetProcAddress(m, "DxcCreateInstance2"); REQUIRE(f);
    void *o = (void *)1;
    EXPECT_HR(f(NULL, &IID_IUnknown, &IID_IUnknown, &o), (HRESULT)0x80040154);
    CHECK(o == NULL);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_instance2_null_malloc) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create2_t f = (dxc_create2_t)GetProcAddress(m, "DxcCreateInstance2"); REQUIRE(f);
    void *o = NULL;
    HRESULT hr = f(NULL, &odm_CLSID_DxbcConverter, &IID_IUnknown, &o);
    EXPECT_HR(hr, S_OK);
    ODM_REL(o);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_instance2_custom_malloc_used) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create2_t f = (dxc_create2_t)GetProcAddress(m, "DxcCreateInstance2"); REQUIRE(f);
    odm_malloc mal; om_init(&mal);
    void *o = NULL;
    HRESULT hr = f(&mal.iface, &odm_CLSID_DxbcConverter, &IID_IUnknown, &o);
    EXPECT_HR(hr, S_OK);
    CHECK(mal.allocs > 0);
    ODM_REL(o);
    t_note("custom IMalloc: allocs=%ld frees=%ld refs=%ld", mal.allocs, mal.frees, mal.ref);
    CHECK(mal.ref >= 1);
    FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_instance2_failure_leaves_malloc_balanced) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    dxc_create2_t f = (dxc_create2_t)GetProcAddress(m, "DxcCreateInstance2"); REQUIRE(f);
    odm_malloc mal; om_init(&mal);
    void *o = (void *)1;
    EXPECT_HR(f(&mal.iface, &IID_IUnknown, &IID_IUnknown, &o), (HRESULT)0x80040154);
    EXPECT_EQ(mal.ref, 1);
    FreeLibrary(m);
}

static const char *odm_ps_src = "float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { return float4(uv, p.z, 1); }";
typedef HRESULT (WINAPI *conv_convert_t)(void *, const void *, UINT, const WCHAR *, void **, UINT *, WCHAR **, UINT *);

static void *odm_make_converter(HMODULE m) {
    dxc_create_t f = (dxc_create_t)GetProcAddress(m, "DxcCreateInstance"); void *o = NULL;
    if (!f || FAILED(f(&odm_CLSID_DxbcConverter, &IID_IUnknown, &o))) return NULL;
    return o;
}
static const unsigned char *odm_find_part(const unsigned char *c, UINT size, const char *fourcc, UINT *psize) {
    if (size < 32 || memcmp(c, "DXBC", 4)) return NULL;
    UINT n = *(const UINT *)(c + 28);
    for (UINT i = 0; i < n && 32 + i * 4 + 4 <= size; i++) {
        UINT off = *(const UINT *)(c + 32 + i * 4);
        if (off + 8 > size) return NULL;
        if (!memcmp(c + off, fourcc, 4)) { if (psize) *psize = *(const UINT *)(c + off + 4); return c + off + 8; }
    }
    return NULL;
}
TEST(d3d_on12_dxcore_misc_dxilconv_convert_ps_to_dxil) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    void *o = odm_make_converter(m);
    if (!o) { FreeLibrary(m); t_skip("no converter"); return; }
    ID3DBlob *blob = NULL;
    if (FAILED(d3dh_compile(odm_ps_src, "main", "ps_5_0", 0, &blob)) || !blob) { odm_release(o); FreeLibrary(m); t_skip("no compiler"); return; }
    void *dxil = NULL; UINT dsize = 0; WCHAR *diag = NULL; UINT dgsize = 0;
    conv_convert_t conv = VCALL(o, 3, conv_convert_t);
    HRESULT hr = conv(o, d3dh_blob_ptr(blob), (UINT)d3dh_blob_size(blob), NULL, &dxil, &dsize, &diag, &dgsize);
    if (FAILED(hr)) { t_note("Convert failed: %s diag=%ls", d3dh_hr_name(hr), diag ? diag : L"(none)"); }
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && dxil) {
        CHECK(dsize > 32);
        CHECK(!memcmp(dxil, "DXBC", 4));
        if (dsize >= 28) EXPECT_EQ(*(UINT *)((unsigned char *)dxil + 24), dsize);
        UINT psz = 0;
        const unsigned char *part = odm_find_part(dxil, dsize, "DXIL", &psz);
        CHECK(part != NULL);
        if (part) { CHECK(psz > 24); CHECK(psz % 4 == 0); }
        t_note("output parts: ISG1=%d OSG1=%d PSV0=%d SHEX=%d", odm_find_part(dxil, dsize, "ISG1", NULL) != NULL, odm_find_part(dxil, dsize, "OSG1", NULL) != NULL, odm_find_part(dxil, dsize, "PSV0", NULL) != NULL, odm_find_part(dxil, dsize, "SHEX", NULL) != NULL);
    }
    if (dxil) CoTaskMemFree(dxil);
    if (diag) CoTaskMemFree(diag);
    SAFE_RELEASE(blob); odm_release(o); FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_convert_vs_to_dxil) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    void *o = odm_make_converter(m);
    if (!o) { FreeLibrary(m); t_skip("no converter"); return; }
    ID3DBlob *blob = NULL;
    if (FAILED(d3dh_compile("float4 main(float4 p : POSITION) : SV_Position { return p * 2; }", "main", "vs_5_0", 0, &blob)) || !blob) { odm_release(o); FreeLibrary(m); t_skip("no compiler"); return; }
    void *dxil = NULL; UINT dsize = 0; WCHAR *diag = NULL; UINT dgsize = 0;
    HRESULT hr = VCALL(o, 3, conv_convert_t)(o, d3dh_blob_ptr(blob), (UINT)d3dh_blob_size(blob), NULL, &dxil, &dsize, &diag, &dgsize);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && dxil) { CHECK(odm_find_part(dxil, dsize, "DXIL", NULL) != NULL); CHECK(!memcmp(dxil, "DXBC", 4)); }
    if (dxil) CoTaskMemFree(dxil);
    if (diag) CoTaskMemFree(diag);
    SAFE_RELEASE(blob); odm_release(o); FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_convert_garbage_fails) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    void *o = odm_make_converter(m);
    if (!o) { FreeLibrary(m); t_skip("no converter"); return; }
    unsigned char zeros[64]; memset(zeros, 0, sizeof zeros);
    void *dxil = NULL; UINT dsize = 0; WCHAR *diag = NULL; UINT dgsize = 0;
    HRESULT hr = VCALL(o, 3, conv_convert_t)(o, zeros, sizeof zeros, NULL, &dxil, &dsize, &diag, &dgsize);
    EXPECT_HR_FAILED(hr);
    if (dxil) CoTaskMemFree(dxil);
    if (diag) CoTaskMemFree(diag);
    odm_release(o); FreeLibrary(m);
}
TEST(d3d_on12_dxcore_misc_dxilconv_convert_truncated_container_fails) {
    HMODULE m = h_dxilconv(); if (!m) { t_skip("dxilconv.dll unavailable"); return; }
    void *o = odm_make_converter(m);
    if (!o) { FreeLibrary(m); t_skip("no converter"); return; }
    ID3DBlob *blob = NULL;
    if (FAILED(d3dh_compile(odm_ps_src, "main", "ps_5_0", 0, &blob)) || !blob) { odm_release(o); FreeLibrary(m); t_skip("no compiler"); return; }
    void *dxil = NULL; UINT dsize = 0; WCHAR *diag = NULL; UINT dgsize = 0;
    HRESULT hr = VCALL(o, 3, conv_convert_t)(o, d3dh_blob_ptr(blob), 20, NULL, &dxil, &dsize, &diag, &dgsize);
    EXPECT_HR_FAILED(hr);
    if (dxil) CoTaskMemFree(dxil);
    if (diag) CoTaskMemFree(diag);
    SAFE_RELEASE(blob); odm_release(o); FreeLibrary(m);
}

/* ---------------- d3dscache ---------------- */
typedef struct { const void *data; UINT size; UINT pad; } odm_keypart;
typedef struct { void *data; UINT size; UINT pad; } odm_sc_value;
typedef HRESULT (WINAPI *sc_create_t)(const void *, void **);
typedef HRESULT (WINAPI *sc_add_t)(void *, const odm_keypart *, UINT, const void *, const void *, UINT);
typedef HRESULT (WINAPI *sc_find_t)(void *, const odm_keypart *, UINT, const void *, odm_sc_value **);
typedef HRESULT (WINAPI *sc_destroy_t)(void *);
typedef HRESULT (WINAPI *sc_flush_t)(void *);
typedef HRESULT (WINAPI *sc_free_t)(odm_sc_value *);
typedef void    (WINAPI *sc_getdesc_t)(void *, void *);

typedef struct { sc_create_t create; sc_add_t add; sc_find_t find; sc_destroy_t destroy; sc_flush_t flush; sc_free_t freev; sc_getdesc_t getdesc; HMODULE mod; } odm_sc;
static int sc_load(odm_sc *s) {
    memset(s, 0, sizeof *s);
    if (t_is_wine()) return 0;
    s->mod = LoadLibraryA("d3dscache.dll");
    if (!s->mod) return 0;
    s->create = (sc_create_t)GetProcAddress(s->mod, "ShaderCache_Create");
    s->add = (sc_add_t)GetProcAddress(s->mod, "ShaderCache_AddValue");
    s->find = (sc_find_t)GetProcAddress(s->mod, "ShaderCache_FindValue");
    s->destroy = (sc_destroy_t)GetProcAddress(s->mod, "ShaderCache_Destroy");
    s->flush = (sc_flush_t)GetProcAddress(s->mod, "ShaderCache_Flush");
    s->freev = (sc_free_t)GetProcAddress(s->mod, "ShaderCache_FreeValue");
    s->getdesc = (sc_getdesc_t)GetProcAddress(s->mod, "ShaderCache_GetDesc");
    return s->create && s->add && s->find && s->destroy && s->flush && s->freev && s->getdesc;
}
#define NEED_SC() odm_sc sc; if (!sc_load(&sc)) { if (sc.mod) FreeLibrary(sc.mod); t_skip("d3dscache.dll unavailable"); return; }
#define END_SC() FreeLibrary(sc.mod)

/* ShaderCacheDesc is 0x40 bytes: memory-only caches use mode 0 at +0x24 and an empty path at +0x18 */
static void sc_desc(unsigned char d[0x40]) { memset(d, 0, 0x40); *(const WCHAR **)(d + 0x18) = L""; }
static void *sc_new(odm_sc *sc) {
    unsigned char d[0x40]; void *c = NULL; sc_desc(d);
    if (FAILED(sc->create(d, &c))) return NULL;
    return c;
}

TEST(d3d_on12_dxcore_misc_scache_exports_ordinals) {
    NEED_SC();
    static const struct { const char *n; unsigned o; } t[] = {
        {"DllCanUnloadNow", 1}, {"DllGetClassObject", 2}, {"ShaderCache_AddValue", 3}, {"ShaderCache_Create", 4},
        {"ShaderCache_Destroy", 5}, {"ShaderCache_FindValue", 6}, {"ShaderCache_Flush", 7}, {"ShaderCache_FreeValue", 8}, {"ShaderCache_GetDesc", 9},
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) CHECKF(GetProcAddress(sc.mod, MAKEINTRESOURCEA(t[i].o)) == GetProcAddress(sc.mod, t[i].n), "%s", t[i].n);
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_create_null_args) {
    NEED_SC();
    unsigned char d[0x40]; sc_desc(d); void *c = (void *)1;
    EXPECT_HR(sc.create(NULL, &c), E_INVARG);
    EXPECT_HR(sc.create(d, NULL), E_INVARG);
    EXPECT_HR(sc.create(NULL, NULL), E_INVARG);
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_create_memory_cache) {
    NEED_SC();
    unsigned char d[0x40]; sc_desc(d); void *c = NULL;
    EXPECT_HR(sc.create(d, &c), S_OK);
    CHECK(c != NULL);
    if (c) EXPECT_HR(sc.destroy(c), S_OK);
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_create_mode2_not_implemented) {
    NEED_SC();
    unsigned char d[0x40]; sc_desc(d); void *c = (void *)1;
    *(UINT *)(d + 0x24) = 2;
    EXPECT_HR(sc.create(d, &c), E_NOTIMPL_H);
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_create_twice_distinct) {
    NEED_SC();
    void *a = sc_new(&sc), *b = sc_new(&sc);
    CHECK(a && b && a != b);
    if (a) sc.destroy(a);
    if (b) sc.destroy(b);
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_destroy_null) {
    NEED_SC();
    EXPECT_HR(sc.destroy(NULL), E_INVARG);
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_flush_null_and_memory) {
    NEED_SC();
    EXPECT_HR(sc.flush(NULL), E_INVARG);
    void *c = sc_new(&sc);
    if (c) { EXPECT_HR(sc.flush(c), S_OK); sc.destroy(c); }
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_freevalue_null) {
    NEED_SC();
    EXPECT_HR(sc.freev(NULL), E_INVARG);
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_getdesc_null_args_ignored) {
    NEED_SC();
    unsigned char out[0x40]; memset(out, 0x77, sizeof out);
    sc.getdesc(NULL, out);
    CHECK(out[0] == 0x77 && out[0x3f] == 0x77);
    void *c = sc_new(&sc);
    if (c) { sc.getdesc(c, NULL); sc.destroy(c); }
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_getdesc_defaults_filled) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    unsigned char out[0x40]; memset(out, 0x77, sizeof out);
    sc.getdesc(c, out);
    EXPECT_EQ(*(UINT *)(out + 0x24), 0);
    EXPECT_EQ(*(UINT *)(out + 0x28), 0x100000);
    EXPECT_EQ(*(UINT *)(out + 0x2c), 0x80);
    EXPECT_EQ(*(UINT *)(out + 0x30), 0x10000);
    EXPECT_EQ(*(UINT *)(out + 0x34), 0x10);
    EXPECT_EQ(*(UINT *)(out + 0x38), 0x8000000);
    const WCHAR *path = *(const WCHAR **)(out + 0x18);
    CHECK(path != NULL && path[0] == 0);
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_getdesc_explicit_limits_roundtrip) {
    NEED_SC();
    unsigned char d[0x40]; sc_desc(d);
    *(UINT *)(d + 0x28) = 0x40000; *(UINT *)(d + 0x2c) = 0x20; *(UINT *)(d + 0x30) = 0x4000; *(UINT *)(d + 0x34) = 4; *(UINT *)(d + 0x38) = 0x1000000;
    void *c = NULL;
    EXPECT_HR(sc.create(d, &c), S_OK);
    if (c) {
        unsigned char out[0x40]; memset(out, 0, sizeof out);
        sc.getdesc(c, out);
        EXPECT_EQ(*(UINT *)(out + 0x28), 0x40000);
        EXPECT_EQ(*(UINT *)(out + 0x2c), 0x20);
        EXPECT_EQ(*(UINT *)(out + 0x30), 0x4000);
        EXPECT_EQ(*(UINT *)(out + 0x34), 4);
        EXPECT_EQ(*(UINT *)(out + 0x38), 0x1000000);
        sc.destroy(c);
    }
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_add_invalid_args) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    odm_keypart k = {"key", 3, 0}; const char v[4] = {1, 2, 3, 4};
    EXPECT_HR(sc.add(NULL, &k, 1, NULL, v, 4), E_INVARG);
    EXPECT_HR(sc.add(c, NULL, 1, NULL, v, 4), E_INVARG);
    EXPECT_HR(sc.add(c, &k, 0, NULL, v, 4), E_INVARG);
    EXPECT_HR(sc.add(c, &k, 1, NULL, NULL, 4), E_INVARG);
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_find_invalid_args) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    odm_keypart k = {"key", 3, 0}; odm_sc_value *v = NULL;
    EXPECT_HR(sc.find(NULL, &k, 1, NULL, &v), E_INVARG);
    EXPECT_HR(sc.find(c, NULL, 1, NULL, &v), E_INVARG);
    EXPECT_HR(sc.find(c, &k, 0, NULL, &v), E_INVARG);
    EXPECT_HR(sc.find(c, &k, 1, NULL, NULL), E_INVARG);
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_find_null_keypart_data) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    odm_keypart k = {NULL, 3, 0}; odm_sc_value *v = NULL;
    EXPECT_HR(sc.find(c, &k, 1, NULL, &v), E_INVARG);
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_find_miss) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    odm_keypart k = {"absent", 6, 0}; odm_sc_value *v = (odm_sc_value *)1;
    HRESULT hr = sc.find(c, &k, 1, NULL, &v);
    EXPECT_HR_FAILED(hr);
    t_note("ShaderCache_FindValue miss: %s out=%p", d3dh_hr_name(hr), v);
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_add_find_roundtrip) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    odm_keypart k = {"shader-key-1", 12, 0};
    unsigned char val[100]; for (int i = 0; i < 100; i++) val[i] = (unsigned char)(i * 7 + 3);
    EXPECT_HR(sc.add(c, &k, 1, NULL, val, sizeof val), S_OK);
    odm_sc_value *v = NULL;
    HRESULT hr = sc.find(c, &k, 1, NULL, &v);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && v) {
        EXPECT_EQ(v->size, 100);
        CHECK(v->data != NULL && v->size == 100 && !memcmp(v->data, val, 100));
        EXPECT_HR(sc.freev(v), S_OK);
    }
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_value_is_a_copy) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    odm_keypart k = {"copy-key", 8, 0};
    unsigned char val[16]; memset(val, 0x11, sizeof val);
    EXPECT_HR(sc.add(c, &k, 1, NULL, val, sizeof val), S_OK);
    memset(val, 0x22, sizeof val);
    odm_sc_value *v = NULL;
    if (SUCCEEDED(sc.find(c, &k, 1, NULL, &v)) && v) {
        CHECK(v->size == 16 && ((unsigned char *)v->data)[0] == 0x11 && ((unsigned char *)v->data)[15] == 0x11);
        sc.freev(v);
    }
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_multi_part_key) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    odm_keypart k2[2] = {{"part-a", 6, 0}, {"part-b", 6, 0}};
    unsigned char val[8] = {9, 8, 7, 6, 5, 4, 3, 2};
    EXPECT_HR(sc.add(c, k2, 2, NULL, val, sizeof val), S_OK);
    odm_sc_value *v = NULL;
    HRESULT hr = sc.find(c, k2, 2, NULL, &v);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && v) { CHECK(v->size == 8 && !memcmp(v->data, val, 8)); sc.freev(v); }
    odm_keypart one = {"part-a", 6, 0}; odm_sc_value *w = NULL;
    HRESULT h2 = sc.find(c, &one, 1, NULL, &w);
    CHECK(FAILED(h2));
    if (SUCCEEDED(h2) && w) sc.freev(w);
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_different_keys_different_values) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    for (int i = 0; i < 16; i++) {
        char key[16]; sprintf(key, "key-%02d", i);
        odm_keypart k = {key, (UINT)strlen(key), 0}; unsigned char val[4] = {(unsigned char)i, (unsigned char)(i + 1), 0, 0};
        EXPECT_HR(sc.add(c, &k, 1, NULL, val, 4), S_OK);
    }
    for (int i = 0; i < 16; i++) {
        char key[16]; sprintf(key, "key-%02d", i);
        odm_keypart k = {key, (UINT)strlen(key), 0}; odm_sc_value *v = NULL;
        HRESULT hr = sc.find(c, &k, 1, NULL, &v);
        CHECKF(hr == S_OK, "key %d: %s", i, d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && v) { CHECK(v->size == 4 && ((unsigned char *)v->data)[0] == i); sc.freev(v); }
    }
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_local_hash_key) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    unsigned long long hash[2] = {0x1122334455667788ull, 0x99aabbccddeeff00ull};
    odm_keypart k = {"hashed", 6, 0}; unsigned char val[3] = {1, 2, 3};
    EXPECT_HR(sc.add(c, &k, 1, hash, val, 3), S_OK);
    odm_sc_value *v = NULL;
    HRESULT hr = sc.find(c, &k, 1, hash, &v);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && v) { CHECK(v->size == 3); sc.freev(v); }
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_flush_after_add) {
    NEED_SC();
    void *c = sc_new(&sc); if (!c) { END_SC(); t_skip("no cache"); return; }
    odm_keypart k = {"flushed", 7, 0}; unsigned char val[3] = {1, 2, 3};
    sc.add(c, &k, 1, NULL, val, 3);
    EXPECT_HR(sc.flush(c), S_OK);
    odm_sc_value *v = NULL;
    if (SUCCEEDED(sc.find(c, &k, 1, NULL, &v)) && v) { CHECK(v->size == 3); sc.freev(v); }
    sc.destroy(c); END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_dllgetclassobject_args) {
    NEED_SC();
    typedef HRESULT (WINAPI *gco_t)(const GUID *, const GUID *, void **);
    gco_t g = (gco_t)GetProcAddress(sc.mod, "DllGetClassObject"); 
    if (!g) { END_SC(); t_skip("no DllGetClassObject"); return; }
    EXPECT_HR(g(&DXC_ATTR_BOGUS, &IID_IClassFactory, NULL), E_PTR);
    void *p = (void *)1;
    EXPECT_HR(g(&DXC_ATTR_BOGUS, &IID_IClassFactory, &p), (HRESULT)0x80040111);
    CHECK(p == NULL);
    END_SC();
}
TEST(d3d_on12_dxcore_misc_scache_dllcanunloadnow) {
    NEED_SC();
    typedef HRESULT (WINAPI *cun_t)(void);
    cun_t f = (cun_t)GetProcAddress(sc.mod, "DllCanUnloadNow");
    if (!f) { END_SC(); t_skip("no DllCanUnloadNow"); return; }
    HRESULT hr = f();
    CHECK(hr == S_OK || hr == S_FALSE);
    END_SC();
}
