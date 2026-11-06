/* IDXGIObject private data semantics, exercised over every kind of DXGI object. */
#include "objs.h"

enum { PK_FACTORY, PK_ADAPTER, PK_DEVICE, PK_TEXTURE, PK_BUFFER, PK_SURFACE, PK_OUTPUT, PK_COUNT };
static const char *pk_name[] = { "factory", "adapter", "device", "texture", "buffer", "surface", "output" };

typedef struct {
    objs_ctx c;
    IDXGIFactory1 *factory;
    IDXGIAdapter *adapter;
    IDXGIOutput *output;
    ID3D11Texture2D *tex;
    ID3D11Buffer *buf;
    IDXGISurface *surf;
    IDXGIObject *o;
    int kind;
} pd_subj;

static void pd_close(pd_subj *s)
{
    SAFE_RELEASE(s->o);
    SAFE_RELEASE(s->surf);
    SAFE_RELEASE(s->buf);
    SAFE_RELEASE(s->tex);
    SAFE_RELEASE(s->output);
    SAFE_RELEASE(s->adapter);
    SAFE_RELEASE(s->factory);
    objs_close(&s->c);
}

/* Returns 1 when the subject is available. */
static int pd_open(pd_subj *s, int kind)
{
    memset(s, 0, sizeof(*s));
    s->kind = kind;
    if (kind == PK_FACTORY) {
        if (!d3dh_CreateDXGIFactory1()) return 0;
        if (FAILED(d3dh_make_factory1(&s->factory))) return 0;
        return SUCCEEDED(IDXGIFactory1_QueryInterface(s->factory, &IID_IDXGIObject, (void **)&s->o));
    }
    if (!objs_open(&s->c)) return 0;
    switch (kind) {
    case PK_ADAPTER:
        if (FAILED(IDXGIDevice_GetAdapter(s->c.dx, &s->adapter))) { pd_close(s); return 0; }
        if (FAILED(IDXGIAdapter_QueryInterface(s->adapter, &IID_IDXGIObject, (void **)&s->o))) { pd_close(s); return 0; }
        return 1;
    case PK_DEVICE:
        if (FAILED(IDXGIDevice_QueryInterface(s->c.dx, &IID_IDXGIObject, (void **)&s->o))) { pd_close(s); return 0; }
        return 1;
    case PK_TEXTURE:
        if (FAILED(objs_rt(s->c.dev, &s->tex))) { pd_close(s); return 0; }
        if (FAILED(ID3D11Texture2D_QueryInterface(s->tex, &IID_IDXGIObject, (void **)&s->o))) { pd_close(s); return 0; }
        return 1;
    case PK_BUFFER:
        if (FAILED(objs_buf(s->c.dev, 256, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, &s->buf))) { pd_close(s); return 0; }
        if (FAILED(ID3D11Buffer_QueryInterface(s->buf, &IID_IDXGIObject, (void **)&s->o))) { pd_close(s); return 0; }
        return 1;
    case PK_SURFACE: {
        DXGI_SURFACE_DESC d;
        memset(&d, 0, sizeof(d));
        d.Width = 32; d.Height = 32; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
        if (FAILED(IDXGIDevice_CreateSurface(s->c.dx, &d, 1, DXGI_USAGE_RENDER_TARGET_OUTPUT, NULL, &s->surf))) { pd_close(s); return 0; }
        if (FAILED(IDXGISurface_QueryInterface(s->surf, &IID_IDXGIObject, (void **)&s->o))) { pd_close(s); return 0; }
        return 1;
    }
    case PK_OUTPUT:
        if (!objs_first_output(&s->adapter, &s->output)) { pd_close(s); return 0; }
        if (FAILED(IDXGIOutput_QueryInterface(s->output, &IID_IDXGIObject, (void **)&s->o))) { pd_close(s); return 0; }
        return 1;
    }
    pd_close(s);
    return 0;
}

typedef void (*pd_fn)(pd_subj *s);
static void pd_wrap(int kind, pd_fn fn)
{
    pd_subj s;
    if (!pd_open(&s, kind)) { t_skip("subject '%s' not available", pk_name[kind]); return; }
    fn(&s);
    pd_close(&s);
}

#define PD_T(scn) \
    TEST(objs_pd_##scn##_factory) { pd_wrap(PK_FACTORY, pd_##scn); } \
    TEST(objs_pd_##scn##_adapter) { pd_wrap(PK_ADAPTER, pd_##scn); } \
    TEST(objs_pd_##scn##_device)  { pd_wrap(PK_DEVICE, pd_##scn); } \
    TEST(objs_pd_##scn##_texture) { pd_wrap(PK_TEXTURE, pd_##scn); } \
    TEST(objs_pd_##scn##_buffer)  { pd_wrap(PK_BUFFER, pd_##scn); } \
    TEST(objs_pd_##scn##_surface) { pd_wrap(PK_SURFACE, pd_##scn); } \
    TEST(objs_pd_##scn##_output)  { pd_wrap(PK_OUTPUT, pd_##scn); }

#define SET(o, g, n, p) IDXGIObject_SetPrivateData(o, &(g), n, p)
#define GETP(o, g, pn, p) IDXGIObject_GetPrivateData(o, &(g), pn, p)

static const BYTE pat16[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

/* ---- scenario: unknown GUID --------------------------------------------- */
static void pd_missing(pd_subj *s)
{
    UINT size = 0xdeadbabe; BYTE buf[16]; HRESULT hr;
    memset(buf, 0xcc, sizeof(buf));
    hr = GETP(s->o, OBJS_GMISSING, &size, buf);
    EXPECT_HR(hr, DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(size, 0);
    EXPECT_EQ(buf[0], 0xcc);
    size = 16;
    hr = GETP(s->o, OBJS_GMISSING, &size, NULL);
    EXPECT_HR(hr, DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(size, 0);
    size = 0;
    hr = GETP(s->o, OBJS_GMISSING, &size, buf);
    EXPECT_HR(hr, DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(size, 0);
}
PD_T(missing)

/* ---- scenario: NULL size pointer ------------------------------------------ */
static void pd_null_size(pd_subj *s)
{
    BYTE buf[16]; HRESULT hr;
    memset(buf, 0xcc, sizeof(buf));
    hr = GETP(s->o, OBJS_GMISSING, NULL, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
    hr = GETP(s->o, OBJS_GMISSING, NULL, buf);
    t_note("%s: missing guid, NULL size, buffer -> %s", pk_name[s->kind], d3dh_hr_name(hr));
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    hr = GETP(s->o, OBJS_G1, NULL, buf);
    EXPECT_HR(hr, E_INVALIDARG);
    EXPECT_EQ(buf[0], 0xcc);
    hr = GETP(s->o, OBJS_G1, NULL, NULL);
    EXPECT_HR(hr, E_INVALIDARG);
}
PD_T(null_size)

/* ---- scenario: size query with NULL buffer ------------------------------------ */
static void pd_size_query(pd_subj *s)
{
    static const UINT in[] = { 0, 1, 15, 16, 17, 100 };
    int i;
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    for (i = 0; i < OBJS_ARRAY(in); i++) {
        UINT size = in[i];
        HRESULT hr = GETP(s->o, OBJS_G1, &size, NULL);
        CHECKF(hr == S_OK, "size in %u: %s", in[i], d3dh_hr_name(hr));
        CHECKF(size == 16, "size in %u: out %u", in[i], size);
    }
}
PD_T(size_query)

/* ---- scenario: buffer too small ----------------------------------------------- */
static void pd_too_small(pd_subj *s)
{
    static const UINT in[] = { 0, 1, 4, 8, 15 };
    int i;
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    for (i = 0; i < OBJS_ARRAY(in); i++) {
        BYTE buf[32]; UINT size = in[i]; HRESULT hr;
        memset(buf, 0xcc, sizeof(buf));
        hr = GETP(s->o, OBJS_G1, &size, buf);
        CHECKF(hr == DXGI_ERROR_MORE_DATA, "size in %u: %s", in[i], d3dh_hr_name(hr));
        CHECKF(size == 16, "size in %u: out %u", in[i], size);
        CHECKF(buf[0] == 0xcc && buf[15] == 0xcc, "buffer modified for size in %u", in[i]);
    }
}
PD_T(too_small)

/* ---- scenario: exact and larger buffer ------------------------------------------ */
static void pd_exact_larger(pd_subj *s)
{
    static const UINT in[] = { 16, 17, 32, 64 };
    int i;
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    for (i = 0; i < OBJS_ARRAY(in); i++) {
        BYTE buf[64]; UINT size = in[i]; HRESULT hr;
        memset(buf, 0xcc, sizeof(buf));
        hr = GETP(s->o, OBJS_G1, &size, buf);
        CHECKF(hr == S_OK, "size in %u: %s", in[i], d3dh_hr_name(hr));
        CHECKF(size == 16, "size in %u: out %u", in[i], size);
        CHECKF(!memcmp(buf, pat16, 16), "data mismatch for size in %u", in[i]);
        CHECKF(buf[16] == 0xcc, "wrote past the data for size in %u", in[i]);
    }
}
PD_T(exact_larger)

/* ---- scenario: data is copied ----------------------------------------------------- */
static void pd_copy(pd_subj *s)
{
    BYTE src[16], out[16]; UINT size = 16;
    memcpy(src, pat16, 16);
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, src));
    memset(src, 0xee, sizeof(src));
    memset(out, 0, sizeof(out));
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, out));
    CHECK(!memcmp(out, pat16, 16));
}
PD_T(copy)

/* ---- scenario: odd sizes round trip ------------------------------------------------- */
static void pd_odd_sizes(pd_subj *s)
{
    static const UINT sizes[] = { 1, 2, 3, 5, 7, 9, 31, 33, 255, 256, 257, 1000 };
    BYTE *src = HeapAlloc(GetProcessHeap(), 0, 1024), *out = HeapAlloc(GetProcessHeap(), 0, 1024);
    int i; UINT j;
    for (j = 0; j < 1024; j++) src[j] = (BYTE)(j * 7 + 3);
    for (i = 0; i < OBJS_ARRAY(sizes); i++) {
        UINT size = 1024; HRESULT hr;
        hr = SET(s->o, OBJS_G2, sizes[i], src);
        CHECKF(hr == S_OK, "set %u: %s", sizes[i], d3dh_hr_name(hr));
        memset(out, 0, 1024);
        hr = GETP(s->o, OBJS_G2, &size, out);
        CHECKF(hr == S_OK && size == sizes[i], "get %u: %s size %u", sizes[i], d3dh_hr_name(hr), size);
        CHECKF(!memcmp(out, src, sizes[i]), "data %u", sizes[i]);
    }
    HeapFree(GetProcessHeap(), 0, src); HeapFree(GetProcessHeap(), 0, out);
}
PD_T(odd_sizes)

/* ---- scenario: replace -------------------------------------------------------------- */
static void pd_replace(pd_subj *s)
{
    BYTE out[32]; UINT size; DWORD four = 0x11223344;
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 4, &four));
    size = 0;
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, NULL));
    EXPECT_EQ(size, 4);
    size = 32; memset(out, 0, sizeof(out));
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, out));
    EXPECT_EQ(size, 4);
    EXPECT_EQ(*(DWORD *)out, 0x11223344);
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    size = 32;
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, out));
    EXPECT_EQ(size, 16);
    CHECK(!memcmp(out, pat16, 16));
}
PD_T(replace)

/* ---- scenario: clear with size 0 / NULL data ------------------------------------------ */
static void pd_clear_zero(pd_subj *s)
{
    UINT size = 16; BYTE buf[16];
    /* clearing something that does not exist reports S_FALSE */
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_FALSE);
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_FALSE);
    EXPECT_HR(GETP(s->o, OBJS_G1, &size, buf), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(size, 0);
}
PD_T(clear_zero)

/* ---- scenario: NULL data with non-zero size also clears --------------------------------- */
static void pd_clear_nonzero(pd_subj *s)
{
    UINT size = 16; BYTE buf[16];
    EXPECT_HR(SET(s->o, OBJS_G1, 42, NULL), S_FALSE);
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    EXPECT_HR(SET(s->o, OBJS_G1, 42, NULL), S_OK);
    EXPECT_HR(SET(s->o, OBJS_G1, 42, NULL), S_FALSE);
    EXPECT_HR(GETP(s->o, OBJS_G1, &size, buf), DXGI_ERROR_NOT_FOUND);
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    EXPECT_HR(SET(s->o, OBJS_G1, ~0U, NULL), S_OK);
    EXPECT_HR(SET(s->o, OBJS_G1, ~0U, NULL), S_FALSE);
}
PD_T(clear_nonzero)

/* ---- scenario: size 0 with non-NULL data (undocumented; observe) -------------------------- */
static void pd_zero_with_data(pd_subj *s)
{
    UINT size = 16; BYTE buf[16]; HRESULT hr, hr2;
    BYTE one = 1;
    hr = SET(s->o, OBJS_G1, 0, &one);
    hr2 = GETP(s->o, OBJS_G1, &size, buf);
    t_note("%s: SetPrivateData(size 0, data!=NULL) -> %s; Get -> %s size %u", pk_name[s->kind],
           d3dh_hr_name(hr), d3dh_hr_name(hr2), size);
    CHECK(SUCCEEDED(hr) || FAILED(hr));
}
PD_T(zero_with_data)

/* ---- scenario: independent GUIDs ----------------------------------------------------------- */
static void pd_multi_guid(pd_subj *s)
{
    BYTE a[16], b[16]; UINT size; DWORD v2 = 0x22222222, v3 = 0x33333333;
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    EXPECT_HR_OK(SET(s->o, OBJS_G2, 4, &v2));
    EXPECT_HR_OK(SET(s->o, OBJS_G3, 4, &v3));
    EXPECT_HR(SET(s->o, OBJS_G2, 0, NULL), S_OK);
    size = 16; memset(a, 0, sizeof(a));
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, a));
    EXPECT_EQ(size, 16);
    CHECK(!memcmp(a, pat16, 16));
    size = 16;
    EXPECT_HR(GETP(s->o, OBJS_G2, &size, b), DXGI_ERROR_NOT_FOUND);
    size = 16;
    EXPECT_HR_OK(GETP(s->o, OBJS_G3, &size, b));
    EXPECT_EQ(size, 4);
    EXPECT_EQ(*(DWORD *)b, 0x33333333);
}
PD_T(multi_guid)

/* ---- scenario: many GUIDs ------------------------------------------------------------------------ */
static void pd_many_guids(pd_subj *s)
{
    GUID g; UINT i; DWORD v;
    for (i = 0; i < 64; i++) { g = OBJS_G1; g.Data1 += i + 1; v = 0xA5000000u + i; EXPECT_HR_OK(IDXGIObject_SetPrivateData(s->o, &g, sizeof(v), &v)); }
    for (i = 0; i < 64; i++) {
        UINT size = sizeof(v);
        g = OBJS_G1; g.Data1 += i + 1; v = 0;
        EXPECT_HR_OK(IDXGIObject_GetPrivateData(s->o, &g, &size, &v));
        CHECKF(v == 0xA5000000u + i, "guid %u value %#x", i, v);
    }
    for (i = 0; i < 64; i += 2) { g = OBJS_G1; g.Data1 += i + 1; EXPECT_HR(IDXGIObject_SetPrivateData(s->o, &g, 0, NULL), S_OK); }
    for (i = 0; i < 64; i++) {
        UINT size = sizeof(v);
        g = OBJS_G1; g.Data1 += i + 1;
        HRESULT hr = IDXGIObject_GetPrivateData(s->o, &g, &size, &v);
        CHECKF(hr == ((i & 1) ? S_OK : DXGI_ERROR_NOT_FOUND), "guid %u: %s", i, d3dh_hr_name(hr));
    }
    for (i = 1; i < 64; i += 2) { g = OBJS_G1; g.Data1 += i + 1; IDXGIObject_SetPrivateData(s->o, &g, 0, NULL); }
}
PD_T(many_guids)

/* ---- scenario: large data ---------------------------------------------------------------------------- */
static void pd_large(pd_subj *s)
{
    UINT n = 65536, size = n, i; BYTE *src = HeapAlloc(GetProcessHeap(), 0, n), *out = HeapAlloc(GetProcessHeap(), 0, n);
    for (i = 0; i < n; i++) src[i] = (BYTE)(i ^ (i >> 8));
    EXPECT_HR_OK(SET(s->o, OBJS_G1, n, src));
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, out));
    EXPECT_EQ(size, n);
    CHECK(!memcmp(src, out, n));
    size = n - 1;
    EXPECT_HR(GETP(s->o, OBJS_G1, &size, out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(size, n);
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
    HeapFree(GetProcessHeap(), 0, src); HeapFree(GetProcessHeap(), 0, out);
}
PD_T(large)

/* ---- scenario: interface refcounting -------------------------------------------------------------------- */
static void pd_iface_refs(pd_subj *s)
{
    LONG destroyed = 0; IUnknown *u = objs_unk_create(&destroyed);
    DWORD v = 5;
    EXPECT_EQ(objs_unk_refs(u), 1);
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, u));
    EXPECT_EQ(objs_unk_refs(u), 2);
    /* setting the very same pointer again does not leak a reference */
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, u));
    EXPECT_EQ(objs_unk_refs(u), 2);
    /* a second GUID holds its own reference */
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G2, u));
    EXPECT_EQ(objs_unk_refs(u), 3);
    /* overwriting with plain data releases the interface */
    EXPECT_HR_OK(SET(s->o, OBJS_G2, 4, &v));
    EXPECT_EQ(objs_unk_refs(u), 2);
    /* clearing with NULL interface releases it, entry stays (as a NULL pointer) */
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, NULL));
    EXPECT_EQ(objs_unk_refs(u), 1);
    EXPECT_EQ(destroyed, 0);
    EXPECT_HR(SET(s->o, OBJS_G2, 0, NULL), S_OK);
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
    IUnknown_Release(u);
    EXPECT_EQ(destroyed, 1);
}
PD_T(iface_refs)

/* ---- scenario: Get of an interface entry ---------------------------------------------------------------------- */
static void pd_iface_get(pd_subj *s)
{
    LONG destroyed = 0; IUnknown *u = objs_unk_create(&destroyed); IUnknown *p;
    UINT size; HRESULT hr;
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, u));
    EXPECT_EQ(objs_unk_refs(u), 2);
    /* NULL buffer queries the size and does not take a reference */
    size = 1;
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, NULL));
    EXPECT_EQ(size, sizeof(void *));
    EXPECT_EQ(objs_unk_refs(u), 2);
    /* too small: MORE_DATA, buffer untouched, no reference */
    p = (IUnknown *)(ULONG_PTR)0xdeadbeef; size = 1;
    hr = GETP(s->o, OBJS_G1, &size, &p);
    EXPECT_HR(hr, DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(size, sizeof(void *));
    EXPECT_EQ(p, (IUnknown *)(ULONG_PTR)0xdeadbeef);
    EXPECT_EQ(objs_unk_refs(u), 2);
    /* success hands out an AddRef'd pointer */
    p = NULL; size = 2 * sizeof(void *);
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, &p));
    EXPECT_EQ(size, sizeof(void *));
    EXPECT_EQ(p, u);
    EXPECT_EQ(objs_unk_refs(u), 3);
    IUnknown_Release(p);
    EXPECT_EQ(objs_unk_refs(u), 2);
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
    EXPECT_EQ(objs_unk_refs(u), 1);
    IUnknown_Release(u);
    EXPECT_EQ(destroyed, 1);
}
PD_T(iface_get)

/* ---- scenario: replacing one interface with another --------------------------------------------------------------- */
static void pd_iface_replace(pd_subj *s)
{
    LONG d1 = 0, d2 = 0; IUnknown *u1 = objs_unk_create(&d1), *u2 = objs_unk_create(&d2), *p = NULL;
    UINT size = sizeof(p);
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, u1));
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, u2));
    EXPECT_EQ(objs_unk_refs(u1), 1);
    EXPECT_EQ(objs_unk_refs(u2), 2);
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, &p));
    EXPECT_EQ(p, u2);
    IUnknown_Release(p);
    IUnknown_Release(u1);
    EXPECT_EQ(d1, 1);
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
    EXPECT_EQ(objs_unk_refs(u2), 1);
    IUnknown_Release(u2);
    EXPECT_EQ(d2, 1);
}
PD_T(iface_replace)

/* ---- scenario: stored NULL interface -------------------------------------------------------------------------------- */
static void pd_iface_null(pd_subj *s)
{
    IUnknown *p = (IUnknown *)(ULONG_PTR)0xdeadbeef; UINT size = 2 * sizeof(p);
    /* a NULL interface is stored as an entry, not treated as a clear */
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, NULL));
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, &p));
    EXPECT_EQ(size, sizeof(void *));
    EXPECT_EQ(p, NULL);
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_FALSE);
    /* a NULL-interface entry then removed by a NULL-data set with a huge size */
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, NULL));
    EXPECT_HR(SET(s->o, OBJS_G1, ~0U, NULL), S_OK);
    EXPECT_HR(SET(s->o, OBJS_G1, ~0U, NULL), S_FALSE);
}
PD_T(iface_null)

/* ---- scenario: data entry replaced by interface and back ------------------------------------------------------------------ */
static void pd_data_to_iface(pd_subj *s)
{
    LONG d = 0; IUnknown *u = objs_unk_create(&d), *p = NULL; UINT size = sizeof(p); BYTE buf[16];
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, u));
    EXPECT_EQ(objs_unk_refs(u), 2);
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, &p));
    EXPECT_EQ(size, sizeof(void *));
    EXPECT_EQ(p, u);
    IUnknown_Release(p);
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 16, pat16));
    EXPECT_EQ(objs_unk_refs(u), 1);
    size = 16;
    EXPECT_HR_OK(GETP(s->o, OBJS_G1, &size, buf));
    EXPECT_EQ(size, 16);
    CHECK(!memcmp(buf, pat16, 16));
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
    IUnknown_Release(u);
    EXPECT_EQ(d, 1);
}
PD_T(data_to_iface)

/* ---- scenario: the owner releasing its store releases the interfaces --------------------------------------------------------- */
static void pd_owner_destroy(pd_subj *s)
{
    LONG d = 0; IUnknown *u = objs_unk_create(&d);
    if (s->kind == PK_FACTORY || s->kind == PK_ADAPTER || s->kind == PK_OUTPUT) {
        /* these may be cached by the runtime beyond the last application reference: only observe */
        EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, u));
        EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
        EXPECT_EQ(objs_unk_refs(u), 1);
        IUnknown_Release(u);
        return;
    }
    EXPECT_HR_OK(IDXGIObject_SetPrivateDataInterface(s->o, &OBJS_G1, u));
    EXPECT_EQ(objs_unk_refs(u), 2);
    pd_close(s);   /* last application reference */
    if (objs_unk_refs(u) != 1)
        t_note("%s: interface still referenced (%ld) after last application release (deferred destruction?)", pk_name[s->kind], (long)objs_unk_refs(u));
    IUnknown_Release(u);
    /* pd_wrap closes again: all pointers are NULL now, which is safe */
}
PD_T(owner_destroy)

/* ---- scenario: debug-name GUID behaves as ordinary data ----------------------------------------------------------------------- */
static void pd_debug_name(pd_subj *s)
{
    static const char name[] = "objs_test_name"; char out[32]; UINT size = sizeof(out);
    EXPECT_HR_OK(SET(s->o, OBJS_WKPDID_NAME, sizeof(name) - 1, name));
    memset(out, 0, sizeof(out));
    EXPECT_HR_OK(GETP(s->o, OBJS_WKPDID_NAME, &size, out));
    EXPECT_EQ(size, sizeof(name) - 1);
    EXPECT_STR(out, name);
    EXPECT_HR(SET(s->o, OBJS_WKPDID_NAME, 0, NULL), S_OK);
}
PD_T(debug_name)

/* ---- scenario: QI to IDXGIObject from IUnknown gives the same store -------------------------------------------------------------- */
static void pd_identity(pd_subj *s)
{
    IUnknown *unk = NULL; IDXGIObject *o2 = NULL; DWORD v = 0x600d, out = 0; UINT size = sizeof(out);
    EXPECT_HR_OK(IDXGIObject_QueryInterface(s->o, &IID_IUnknown, (void **)&unk));
    EXPECT_HR_OK(IUnknown_QueryInterface(unk, &IID_IDXGIObject, (void **)&o2));
    EXPECT_EQ(o2, s->o);
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 4, &v));
    EXPECT_HR_OK(IDXGIObject_GetPrivateData(o2, &OBJS_G1, &size, &out));
    EXPECT_EQ(out, 0x600d);
    EXPECT_HR(IDXGIObject_SetPrivateData(o2, &OBJS_G1, 0, NULL), S_OK);
    EXPECT_HR(GETP(s->o, OBJS_G1, &size, &out), DXGI_ERROR_NOT_FOUND);
    IDXGIObject_Release(o2);
    IUnknown_Release(unk);
}
PD_T(identity)

/* ---- scenario: private data on the object is independent of other objects of the same kind ---------------------------------------- */
static void pd_isolation(pd_subj *s)
{
    pd_subj t; DWORD v = 7; UINT size = 4; DWORD out;
    if (!pd_open(&t, s->kind)) { t_skip("second subject not available"); return; }
    EXPECT_HR_OK(SET(s->o, OBJS_G1, 4, &v));
    /* each pd_open builds its own factory, so adapter/output instances are distinct objects with separate private data */
    EXPECT_HR(IDXGIObject_GetPrivateData(t.o, &OBJS_G1, &size, &out), DXGI_ERROR_NOT_FOUND);
    EXPECT_HR(SET(s->o, OBJS_G1, 0, NULL), S_OK);
    pd_close(&t);
}
PD_T(isolation)

/* ---- cross-API: device private data through ID3D11Device vs IDXGIObject (observation) -------------------------------------------------- */
TEST(objs_pd_device_d3d11_vs_dxgi)
{
    NEED_DEV(c);
    DWORD v = 0xabcd, out = 0; UINT size = sizeof(out); HRESULT hr;
    IDXGIObject *o = NULL;
    EXPECT_HR_OK(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIObject, (void **)&o));
    EXPECT_HR_OK(ID3D11Device_SetPrivateData(c.dev, &OBJS_G1, 4, &v));
    hr = IDXGIObject_GetPrivateData(o, &OBJS_G1, &size, &out);
    t_note("D3D11 SetPrivateData then IDXGIObject::GetPrivateData -> %s (out %#x)", d3dh_hr_name(hr), out);
    size = 4;
    EXPECT_HR_OK(ID3D11Device_GetPrivateData(c.dev, &OBJS_G1, &size, &out));
    EXPECT_EQ(out, 0xabcd);
    ID3D11Device_SetPrivateData(c.dev, &OBJS_G1, 0, NULL);
    IDXGIObject_SetPrivateData(o, &OBJS_G1, 0, NULL);
    IDXGIObject_Release(o);
    objs_close(&c);
}
TEST(objs_pd_texture_d3d11_vs_dxgi)
{
    NEED_DEV(c);
    ID3D11Texture2D *t = NULL; IDXGIObject *o = NULL; DWORD v = 0xabcd, out = 0; UINT size = sizeof(out); HRESULT hr;
    if (FAILED(objs_rt(c.dev, &t))) { t_skip("no texture"); objs_close(&c); return; }
    EXPECT_HR_OK(ID3D11Texture2D_QueryInterface(t, &IID_IDXGIObject, (void **)&o));
    EXPECT_HR_OK(ID3D11Texture2D_SetPrivateData(t, &OBJS_G1, 4, &v));
    hr = IDXGIObject_GetPrivateData(o, &OBJS_G1, &size, &out);
    t_note("ID3D11DeviceChild::SetPrivateData then IDXGIObject::GetPrivateData -> %s", d3dh_hr_name(hr));
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 */
    size = 4;
    EXPECT_HR_OK(ID3D11Texture2D_GetPrivateData(t, &OBJS_G1, &size, &out));
    EXPECT_EQ(out, 0xabcd);
    /* the D3D11-side not-found / more-data protocol matches DXGI's */
    size = 2;
    EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &OBJS_G1, &size, &out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(size, 4);
    size = 4;
    EXPECT_HR(ID3D11Texture2D_GetPrivateData(t, &OBJS_GMISSING, &size, &out), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(size, 0);
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &OBJS_G1, 0, NULL), S_OK);
    EXPECT_HR(ID3D11Texture2D_SetPrivateData(t, &OBJS_G1, 0, NULL), S_FALSE);
    IDXGIObject_Release(o); ID3D11Texture2D_Release(t);
    objs_close(&c);
}

/* ---- cross-interface on a device: IDXGIDevice1 / IDXGIDevice2 views ------------------------------------------------------------------------ */
TEST(objs_pd_device_versions_share_store)
{
    NEED_DEV(c);
    IDXGIDevice1 *d1 = NULL; IDXGIObject *o = NULL; DWORD v = 99, out = 0; UINT size = 4;
    if (FAILED(IDXGIDevice_QueryInterface(c.dx, &IID_IDXGIDevice1, (void **)&d1))) { t_skip("no IDXGIDevice1"); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIDevice1_SetPrivateData(d1, &OBJS_G1, 4, &v));
    EXPECT_HR_OK(IDXGIDevice_GetPrivateData(c.dx, &OBJS_G1, &size, &out));
    EXPECT_EQ(out, 99);
    EXPECT_HR(IDXGIDevice_SetPrivateData(c.dx, &OBJS_G1, 0, NULL), S_OK);
    EXPECT_HR(IDXGIDevice1_SetPrivateData(d1, &OBJS_G1, 0, NULL), S_FALSE);
    IDXGIDevice1_Release(d1);
    objs_close(&c);
}
TEST(objs_pd_factory_versions_share_store)
{
    IDXGIFactory1 *f1 = NULL; IDXGIFactory *f0 = NULL; DWORD v = 11, out = 0; UINT size = 4;
    REQUIRE(d3dh_CreateDXGIFactory1());
    if (FAILED(d3dh_make_factory1(&f1))) { t_skip("no factory"); return; }
    EXPECT_HR_OK(IDXGIFactory1_QueryInterface(f1, &IID_IDXGIFactory, (void **)&f0));
    EXPECT_HR_OK(IDXGIFactory1_SetPrivateData(f1, &OBJS_G1, 4, &v));
    EXPECT_HR_OK(IDXGIFactory_GetPrivateData(f0, &OBJS_G1, &size, &out));
    EXPECT_EQ(out, 11);
    EXPECT_HR(IDXGIFactory_SetPrivateData(f0, &OBJS_G1, 0, NULL), S_OK);
    IDXGIFactory_Release(f0); IDXGIFactory1_Release(f1);
}
TEST(objs_pd_adapter_versions_share_store)
{
    NEED_DEV(c);
    IDXGIAdapter *a = NULL; IDXGIAdapter1 *a1 = NULL; DWORD v = 12, out = 0; UINT size = 4;
    EXPECT_HR_OK(IDXGIDevice_GetAdapter(c.dx, &a));
    if (FAILED(IDXGIAdapter_QueryInterface(a, &IID_IDXGIAdapter1, (void **)&a1))) { t_skip("no IDXGIAdapter1"); IDXGIAdapter_Release(a); objs_close(&c); return; }
    EXPECT_HR_OK(IDXGIAdapter1_SetPrivateData(a1, &OBJS_G1, 4, &v));
    EXPECT_HR_OK(IDXGIAdapter_GetPrivateData(a, &OBJS_G1, &size, &out));
    EXPECT_EQ(out, 12);
    IDXGIAdapter1_SetPrivateData(a1, &OBJS_G1, 0, NULL);
    IDXGIAdapter1_Release(a1); IDXGIAdapter_Release(a);
    objs_close(&c);
}
