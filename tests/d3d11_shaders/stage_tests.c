/* Per-stage creation contract, generated for VS/PS/GS/HS/DS/CS from one macro. */
#include "sh_common.h"

static const char *const other_names[ST_COUNT] = { "vs_pass", "ps_const", "gs_tri", "hs_tri_int", "ds_tri", "cs_basic" };
static const UINT stage_minfl[ST_COUNT] = { 0xa000, 0xa000, 0xa000, 0xb000, 0xb000, 0xa000 };
#define CS_OK_LEVEL 0xb000

#define STAGE_TESTS(tag, ST, SHNAME, MINFL) \
TEST(d3d11_shaders_##tag##_create_ok) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); \
    void *sh = (void *)(ULONG_PTR)1; \
    HRESULT hr = sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh); \
    EXPECT_HR(hr, S_OK); CHECK(sh != NULL && sh != (void *)(ULONG_PTR)1); \
    if (sh && sh != (void *)(ULONG_PTR)1) { EXPECT_EQ(d3dh_refcount(sh), 1); SH_REL(sh); } \
    sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_create_null_out_validates) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); \
    EXPECT_HR(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, NULL), S_FALSE); \
    sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_null_out_bad_bytecode) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); \
    unsigned char *p = sh_dup(b, 0); memcpy(p, "XXXX", 4); \
    EXPECT_HR(sh_create(e.dev, ST, p, SH_BN(b), NULL, NULL), E_INVALIDARG); \
    free(p); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_null_bytecode) { \
    SH_ENV(e, MINFL); void *sh = NULL; \
    EXPECT_HR(sh_create(e.dev, ST, NULL, 0, NULL, &sh), E_INVALIDARG); CHECK(sh == NULL); \
    EXPECT_HR(sh_create(e.dev, ST, NULL, 64, NULL, &sh), E_INVALIDARG); CHECK(sh == NULL); \
    EXPECT_HR(sh_create(e.dev, ST, NULL, 0, NULL, NULL), E_INVALIDARG); \
    sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_zero_length) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL; \
    EXPECT_HR(sh_create(e.dev, ST, SH_BP(b), 0, NULL, &sh), E_INVALIDARG); CHECK(sh == NULL); \
    EXPECT_HR(sh_create(e.dev, ST, SH_BP(b), 0, NULL, NULL), E_INVALIDARG); \
    sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_truncated_lengths) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); \
    SIZE_T n = SH_BN(b); SIZE_T lens[] = { 1, 3, 4, 8, 16, 31, n / 2, n - 1 }; \
    for (int i = 0; i < 8; i++) { void *sh = NULL; HRESULT hr = sh_create(e.dev, ST, SH_BP(b), lens[i], NULL, &sh); \
        t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "truncated create fails", "len %llu hr %s", (unsigned long long)lens[i], d3dh_hr_name(hr)); SH_REL(sh); } \
    sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_bad_magic) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); unsigned char *p = sh_dup(b, 0); \
    static const char *const bad[] = { "DXBD", "dxbc", "\0\0\0\0", "CBXD", "DXIL" }; \
    for (int i = 0; i < 5; i++) { void *sh = NULL; memcpy(p, bad[i], 4); \
        t_check(sh_create(e.dev, ST, p, SH_BN(b), NULL, &sh) == E_INVALIDARG && !sh, __FILE__, __LINE__, "bad magic rejected", "variant %d", i); SH_REL(sh); } \
    free(p); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_bad_total_size_field) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); unsigned char *p = sh_dup(b, 0); void *sh = NULL; \
    DWORD orig = *(DWORD *)(p + 24); \
    *(DWORD *)(p + 24) = orig + 4; HRESULT hr = sh_create(e.dev, ST, p, SH_BN(b), NULL, &sh); \
    t_note(#tag ": container size field larger than buffer -> %s", d3dh_hr_name(hr)); EXPECT_HR(hr, E_INVALIDARG); SH_REL(sh); \
    *(DWORD *)(p + 24) = orig - 4; sh = NULL; hr = sh_create(e.dev, ST, p, SH_BN(b), NULL, &sh); \
    t_note(#tag ": container size field smaller than buffer -> %s", d3dh_hr_name(hr)); SH_REL(sh); \
    free(p); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_bad_chunk_offset) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); unsigned char *p = sh_dup(b, 0); void *sh = NULL; \
    DWORD cnt = *(DWORD *)(p + 28); CHECK(cnt >= 1); \
    *(DWORD *)(p + 32) = (DWORD)SH_BN(b) + 16; /* first chunk offset beyond the container */ \
    HRESULT hr = sh_create(e.dev, ST, p, SH_BN(b), NULL, &sh); EXPECT_HR(hr, E_INVALIDARG); CHECK(sh == NULL); SH_REL(sh); \
    free(p); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_checksum_corruption_observed) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); unsigned char *p = sh_dup(b, 0); void *sh = NULL; \
    for (int i = 4; i < 20; i++) p[i] ^= 0xA5; \
    HRESULT hr = sh_create(e.dev, ST, p, SH_BN(b), NULL, &sh); \
    t_note(#tag ": corrupted container checksum -> %s", d3dh_hr_name(hr)); \
    CHECK(hr == S_OK || hr == E_INVALIDARG); SH_REL(sh); free(p); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_empty_container) { \
    SH_ENV(e, MINFL); unsigned char buf[64]; size_t n = sh_empty_dxbc(buf); void *sh = NULL; \
    EXPECT_HR(sh_create(e.dev, ST, buf, n, NULL, &sh), E_INVALIDARG); CHECK(sh == NULL); \
    sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_wrong_stage_bytecode) { \
    SH_ENV(e, 0xb000); \
    for (int o = 0; o < ST_COUNT; o++) { if (o == ST) continue; \
        ID3DBlob *b = sh_blob(other_names[o]); if (!b) return; void *sh = NULL; \
        HRESULT hr = sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh); \
        t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "wrong stage bytecode rejected", "%s bytecode in %s creator: %s", sh_stage_name[o], sh_stage_name[ST], d3dh_hr_name(hr)); SH_REL(sh); \
        hr = sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, NULL); \
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "wrong stage rejected on validation path", "%s in %s: %s", sh_stage_name[o], sh_stage_name[ST], d3dh_hr_name(hr)); } \
    sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_queryinterface) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    IUnknown *u = (IUnknown *)sh; void *q = NULL; \
    EXPECT_HR(IUnknown_QueryInterface(u, sh_iid(ST), &q), S_OK); EXPECT_EQ(q, sh); SH_REL(q); \
    EXPECT_HR(IUnknown_QueryInterface(u, &IID_ID3D11DeviceChild, &q), S_OK); EXPECT_EQ(q, sh); SH_REL(q); \
    EXPECT_HR(IUnknown_QueryInterface(u, &IID_IUnknown, &q), S_OK); EXPECT_EQ(q, sh); SH_REL(q); \
    for (int o = 0; o < ST_COUNT; o++) { if (o == ST) continue; q = (void *)(ULONG_PTR)1; \
        HRESULT hr = IUnknown_QueryInterface(u, sh_iid(o), &q); \
        t_check(hr == E_NOINTERFACE && q == NULL, __FILE__, __LINE__, "QI to other stage interface", "%s -> %s", sh_stage_name[o], d3dh_hr_name(hr)); SH_REL(q); } \
    EXPECT_HR(IUnknown_QueryInterface(u, &IID_ID3D11Buffer, &q), E_NOINTERFACE); \
    EXPECT_HR(IUnknown_QueryInterface(u, &IID_ID3D11ClassLinkage, &q), E_NOINTERFACE); \
    SH_REL(sh); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_refcount_protocol) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    IUnknown *u = (IUnknown *)sh; \
    EXPECT_EQ(IUnknown_AddRef(u), 2); EXPECT_EQ(IUnknown_AddRef(u), 3); \
    EXPECT_EQ(IUnknown_Release(u), 2); EXPECT_EQ(IUnknown_Release(u), 1); \
    ID3D11Device *d = NULL; ID3D11DeviceChild_GetDevice((ID3D11DeviceChild *)sh, &d); \
    CHECK(d == e.dev); SH_REL(d); \
    EXPECT_EQ(IUnknown_Release(u), 0); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_getdevice_addrefs_device) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    ULONG before = d3dh_refcount(e.dev); ID3D11Device *d = NULL; \
    ID3D11DeviceChild_GetDevice((ID3D11DeviceChild *)sh, &d); CHECK(d == e.dev); \
    EXPECT_EQ(d3dh_refcount(e.dev), before + 1); SH_REL(d); EXPECT_EQ(d3dh_refcount(e.dev), before); \
    SH_REL(sh); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_private_data) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    ID3D11DeviceChild *dc = (ID3D11DeviceChild *)sh; \
    static const GUID g = { 0x1234abcd, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } }; \
    UINT sz = 99; char buf[16] = ""; \
    EXPECT_HR(ID3D11DeviceChild_GetPrivateData(dc, &g, &sz, buf), DXGI_ERROR_NOT_FOUND); EXPECT_EQ(sz, 0); \
    EXPECT_HR(ID3D11DeviceChild_SetPrivateData(dc, &g, 6, "hello"), S_OK); \
    sz = 0; { HRESULT hq = ID3D11DeviceChild_GetPrivateData(dc, &g, &sz, NULL); t_note("size query with NULL buffer -> %s size %u", d3dh_hr_name(hq), sz); CHECK(hq == S_OK || hq == DXGI_ERROR_MORE_DATA); EXPECT_EQ(sz, 6); } \
    sz = 3; EXPECT_HR(ID3D11DeviceChild_GetPrivateData(dc, &g, &sz, buf), DXGI_ERROR_MORE_DATA); EXPECT_EQ(sz, 6); \
    sz = 16; EXPECT_HR(ID3D11DeviceChild_GetPrivateData(dc, &g, &sz, buf), S_OK); EXPECT_EQ(sz, 6); EXPECT_STR(buf, "hello"); \
    EXPECT_HR(ID3D11DeviceChild_SetPrivateData(dc, &g, 3, "ab"), S_OK); /* replace */ \
    sz = 16; EXPECT_HR(ID3D11DeviceChild_GetPrivateData(dc, &g, &sz, buf), S_OK); EXPECT_EQ(sz, 3); \
    EXPECT_HR(ID3D11DeviceChild_SetPrivateData(dc, &g, 0, NULL), S_OK); /* delete */ \
    sz = 16; EXPECT_HR(ID3D11DeviceChild_GetPrivateData(dc, &g, &sz, buf), DXGI_ERROR_NOT_FOUND); \
    SH_REL(sh); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_private_data_interface) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    ID3D11DeviceChild *dc = (ID3D11DeviceChild *)sh; \
    static const GUID g = { 0x1234abce, 0x1111, 0x2222, { 1, 2, 3, 4, 5, 6, 7, 8 } }; \
    ID3D11Query *q = NULL; D3D11_QUERY_DESC qd = { D3D11_QUERY_EVENT, 0 }; \
    if (FAILED(ID3D11Device_CreateQuery(e.dev, &qd, &q))) { t_skip("no query"); return; } \
    ULONG rc = d3dh_refcount(q); \
    EXPECT_HR(ID3D11DeviceChild_SetPrivateDataInterface(dc, &g, (IUnknown *)q), S_OK); \
    EXPECT_EQ(d3dh_refcount(q), rc + 1); \
    UINT sz = sizeof(void *); void *got = NULL; \
    EXPECT_HR(ID3D11DeviceChild_GetPrivateData(dc, &g, &sz, &got), S_OK); CHECK(got == q); EXPECT_EQ(sz, sizeof(void *)); \
    { ULONG rc2 = d3dh_refcount(q); t_note("GetPrivateData of an interface entry changes refcount by %ld", (long)rc2 - (long)(rc + 1)); if (rc2 > rc + 1) { IUnknown_Release((IUnknown *)q); } } \
    SH_REL(sh); EXPECT_EQ(d3dh_refcount(q), rc); SH_REL(q); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_debug_object_name) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    ID3D11DeviceChild *dc = (ID3D11DeviceChild *)sh; char nm[32] = ""; UINT sz = sizeof nm; \
    EXPECT_HR(ID3D11DeviceChild_SetPrivateData(dc, &WKPDID_D3DDebugObjectName, 7, "MyName"), S_OK); \
    EXPECT_HR(ID3D11DeviceChild_GetPrivateData(dc, &WKPDID_D3DDebugObjectName, &sz, nm), S_OK); EXPECT_EQ(sz, 7); EXPECT_STR(nm, "MyName"); \
    SH_REL(sh); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_two_objects_from_one_bytecode) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *a = NULL, *c = NULL; \
    unsigned char *p = sh_dup(b, 0); \
    EXPECT_HR(sh_create(e.dev, ST, p, SH_BN(b), NULL, &a), S_OK); \
    memset(p, 0, SH_BN(b)); /* creation must not retain the caller's buffer */ \
    EXPECT_HR(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &c), S_OK); \
    CHECK(a != NULL && c != NULL && a != c); \
    free(p); SH_REL(a); SH_REL(c); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_bind_roundtrip) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL, *got = NULL; UINT n = 0; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    sh_set(e.ctx, ST, sh, NULL, 0); \
    sh_get(e.ctx, ST, &got, NULL, &n); CHECK(got == sh); EXPECT_EQ(n, 0); SH_REL(got); \
    sh_set(e.ctx, ST, NULL, NULL, 0); got = (void *)(ULONG_PTR)1; \
    sh_get(e.ctx, ST, &got, NULL, NULL); CHECK(got == NULL); \
    SH_REL(sh); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_bound_object_outlives_release) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL, *got = NULL; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    sh_set(e.ctx, ST, sh, NULL, 0); SH_REL(sh); \
    sh_get(e.ctx, ST, &got, NULL, NULL); CHECK(got != NULL); \
    sh_set(e.ctx, ST, NULL, NULL, 0); SH_REL(got); \
    sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_clearstate_unbinds) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *sh = NULL, *got = (void *)(ULONG_PTR)1; \
    if (FAILED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); return; } \
    sh_set(e.ctx, ST, sh, NULL, 0); ID3D11DeviceContext_ClearState(e.ctx); \
    sh_get(e.ctx, ST, &got, NULL, NULL); CHECK(got == NULL); \
    SH_REL(sh); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_default_binding_null) { \
    SH_ENV(e, MINFL); void *got = (void *)(ULONG_PTR)1; UINT n = 77; \
    sh_get(e.ctx, ST, &got, NULL, &n); CHECK(got == NULL); EXPECT_EQ(n, 0); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_create_with_empty_linkage) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); ID3D11ClassLinkage *cl = NULL; void *sh = NULL; \
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK); if (!cl) return; \
    EXPECT_HR(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), cl, &sh), S_OK); CHECK(sh != NULL); \
    EXPECT_HR(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), cl, NULL), S_FALSE); \
    SH_REL(sh); SH_REL(cl); sh_env_free(&e); } \
TEST(d3d11_shaders_##tag##_create_many) { \
    SH_ENV(e, MINFL); SH_BLOB(b, SHNAME); void *s[200]; int ok = 0, distinct = 1; \
    for (int i = 0; i < 200; i++) { s[i] = NULL; if (SUCCEEDED(sh_create(e.dev, ST, SH_BP(b), SH_BN(b), NULL, &s[i]))) ok++; } \
    for (int i = 1; i < 200; i++) if (s[i] == s[i - 1]) distinct = 0; \
    EXPECT_EQ(ok, 200); CHECK(distinct); for (int i = 0; i < 200; i++) SH_REL(s[i]); sh_env_free(&e); }

STAGE_TESTS(vs, ST_VS, "vs_pass", 0xa000)
STAGE_TESTS(ps, ST_PS, "ps_const", 0xa000)
STAGE_TESTS(gs, ST_GS, "gs_tri", 0xa000)
STAGE_TESTS(hs, ST_HS, "hs_tri_int", 0xb000)
STAGE_TESTS(ds, ST_DS, "ds_tri", 0xb000)
STAGE_TESTS(cs, ST_CS, "cs_basic", 0xb000)

/* No API exposes the retained bytecode: shader objects only implement the device-child surface. */
TEST(d3d11_shaders_objects_expose_no_bytecode_interfaces)
{
    SH_ENV(e, 0xb000);
    static const char *const names[ST_COUNT] = { "vs_pass", "ps_const", "gs_tri", "hs_tri_int", "ds_tri", "cs_basic" };
    const IID *foreign[] = { &IID_ID3D10Blob, &IID_ID3D11Buffer, &IID_ID3D11Resource, &IID_ID3D11View, &IID_ID3D11InputLayout, &IID_ID3D11Device,
                             &IID_ID3D11DeviceContext, &IID_IDXGIObject, &IID_IDXGIDeviceSubObject, &IID_ID3D11Asynchronous };
    for (int st = 0; st < ST_COUNT; st++) {
        ID3DBlob *b = sh_blob(names[st]); if (!b) continue;
        void *sh = NULL; if (FAILED(sh_create(e.dev, st, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECK(0); continue; }
        for (size_t i = 0; i < sizeof foreign / sizeof *foreign; i++) {
            void *q = (void *)(ULONG_PTR)1; HRESULT hr = IUnknown_QueryInterface((IUnknown *)sh, foreign[i], &q);
            t_check(hr == E_NOINTERFACE && q == NULL, __FILE__, __LINE__, "foreign interface refused", "%s iid#%d: %s", sh_stage_name[st], (int)i, d3dh_hr_name(hr)); SH_REL(q);
        }
        SH_REL(sh);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_objects_distinct_identity_per_creation)
{
    SH_ENV(e, 0);
    SH_BLOB(vb, "vs_pass"); SH_BLOB(pb, "ps_const");
    ID3D11VertexShader *v1 = NULL, *v2 = NULL; ID3D11PixelShader *p1 = NULL;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &v1), S_OK);
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &v2), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(pb), SH_BN(pb), NULL, &p1), S_OK);
    CHECK(v1 && v2 && p1 && (void *)v1 != (void *)v2 && (void *)v1 != (void *)p1);
    SH_REL(v1); SH_REL(v2); SH_REL(p1); sh_env_free(&e);
}
TEST(d3d11_shaders_vs_and_gs_bindings_independent)
{
    /* Binding a shader returned by one creator through another stage's setter is a caller bug; the typed Get must still return
     * the object that was bound last through the matching setter. */
    SH_ENV(e, 0); SH_BLOB(vb, "vs_pass"); SH_BLOB(gb, "gs_tri");
    ID3D11VertexShader *vs = NULL, *g = NULL; ID3D11GeometryShader *gs = NULL, *gg = NULL;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &vs), S_OK);
    EXPECT_HR(ID3D11Device_CreateGeometryShader(e.dev, SH_BP(gb), SH_BN(gb), NULL, &gs), S_OK);
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0); ID3D11DeviceContext_GSSetShader(e.ctx, gs, NULL, 0);
    ID3D11DeviceContext_VSGetShader(e.ctx, &g, NULL, NULL); ID3D11DeviceContext_GSGetShader(e.ctx, &gg, NULL, NULL);
    CHECK(g == vs && gg == gs); SH_REL(g); SH_REL(gg);
    ID3D11DeviceContext_ClearState(e.ctx);
    SH_REL(vs); SH_REL(gs); sh_env_free(&e);
}
