/* Private data on contexts, resource min-LOD, tiled-resource entry points, context type/flags corners. */
#include "csh.h"

static const GUID PD1 = { 0x6a1e5c10, 0x11, 0x22, { 1, 2, 3, 4, 5, 6, 7, 8 } };
static const GUID PD2 = { 0x6a1e5c11, 0x11, 0x22, { 1, 2, 3, 4, 5, 6, 7, 8 } };

/* ---- private data on device contexts (immediate and deferred) -------------------------------- */
typedef struct { ID3D11DeviceContext *c; int which; HRESULT hr; } pdctx;
static void pd_risky(void *p)
{
    pdctx *x = p; static BYTE big[70000]; char buf[16]; x->hr = 0x7fffffff;
    switch (x->which) {
    case 0: x->hr = ID3D11DeviceContext_SetPrivateData(x->c, &PD2, 65536, big); break;
    case 1: x->hr = ID3D11DeviceContext_SetPrivateData(x->c, &PD2, 0xffffffffu, big); break;
    case 2: x->hr = ID3D11DeviceContext_GetPrivateData(x->c, &PD2, NULL, buf); break;
    }
}
static void pd_matrix(ID3D11DeviceContext *c)
{
    char buf[64]; UINT sz; HRESULT hr;
    BYTE data[16]; for (int i = 0; i < 16; i++) data[i] = (BYTE)(0xA0 + i);
    sz = 0x1234;
    EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD1, &sz, NULL), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(sz, 0); /* size is zeroed when the entry is missing */
    sz = 16; memset(buf, 0x55, sizeof buf);
    EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD1, &sz, buf), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(sz, 0); EXPECT_EQ((BYTE)buf[0], 0x55);
    EXPECT_HR(ID3D11DeviceContext_SetPrivateData(c, &PD1, 16, data), S_OK);
    sz = 0; EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD1, &sz, NULL), S_OK); EXPECT_EQ(sz, 16);
    sz = 4; memset(buf, 0x55, sizeof buf);
    EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD1, &sz, buf), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 16); EXPECT_EQ((BYTE)buf[0], 0x55); /* nothing copied */
    sz = 64; memset(buf, 0, sizeof buf);
    EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD1, &sz, buf), S_OK);
    EXPECT_EQ(sz, 16); CHECK(!memcmp(buf, data, 16));
    EXPECT_HR(ID3D11DeviceContext_SetPrivateData(c, &PD1, 4, data), S_OK); /* replaces */
    sz = 64; EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD1, &sz, buf), S_OK); EXPECT_EQ(sz, 4);
    EXPECT_HR(ID3D11DeviceContext_SetPrivateData(c, &PD1, 0, NULL), S_OK); /* deletes */
    sz = 64; EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD1, &sz, buf), DXGI_ERROR_NOT_FOUND); EXPECT_EQ(sz, 0);
    /* zero size with a pointer, and non-zero size with a NULL pointer, are both accepted (the latter stores nothing readable) */
    EXPECT_HR(ID3D11DeviceContext_SetPrivateData(c, &PD2, 0, data), S_OK);
    EXPECT_HR(ID3D11DeviceContext_SetPrivateData(c, &PD2, 4, NULL), S_OK);
    /* large data is accepted; the size is trusted (0xffffffff faults); a NULL size pointer is rejected */
    pdctx x = { c, 0, 0 };
    EXPECT_EQ(t_try(pd_risky, &x), 0); EXPECT_HR(x.hr, S_OK);
    x.which = 1; EXPECT_EQ(t_try(pd_risky, &x), 0xc0000005);
    x.which = 2; EXPECT_EQ(t_try(pd_risky, &x), 0); EXPECT_HR(x.hr, E_INVALIDARG);
    ID3D11DeviceContext_SetPrivateData(c, &PD2, 0, NULL);
    /* debug object name */
    EXPECT_HR(ID3D11DeviceContext_SetPrivateData(c, &WKPDID_D3DDebugObjectName, 5, "abcd"), S_OK);
    sz = 64; EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &WKPDID_D3DDebugObjectName, &sz, buf), S_OK); EXPECT_EQ(sz, 5);
    EXPECT_STR(buf, "abcd");
    ID3D11DeviceContext_SetPrivateData(c, &WKPDID_D3DDebugObjectName, 0, NULL);
    /* interface entries hold a reference; replacing or deleting releases it */
    IUnknown *u = (IUnknown *)c, *g = NULL;
    ULONG b = d3dh_refcount(c);
    EXPECT_HR(ID3D11DeviceContext_SetPrivateDataInterface(c, &PD2, u), S_OK);
    EXPECT_EQ(d3dh_refcount(c), b + 1);
    sz = 0; EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD2, &sz, NULL), S_OK); EXPECT_EQ(sz, sizeof(void *));
    sz = sizeof g; hr = ID3D11DeviceContext_GetPrivateData(c, &PD2, &sz, &g);
    EXPECT_HR(hr, S_OK); EXPECT_EQ(g, u); EXPECT_EQ(d3dh_refcount(c), b + 2); /* Get adds a reference */
    if (g) g->lpVtbl->Release(g);
    EXPECT_HR(ID3D11DeviceContext_SetPrivateData(c, &PD2, 4, data), S_OK);
    EXPECT_EQ(d3dh_refcount(c), b); /* replaced by plain data: reference released */
    /* a NULL interface is stored as a NULL pointer entry (8 bytes), it does not delete */
    EXPECT_HR(ID3D11DeviceContext_SetPrivateDataInterface(c, &PD2, NULL), S_OK);
    sz = 64; g = (IUnknown *)1; EXPECT_HR(ID3D11DeviceContext_GetPrivateData(c, &PD2, &sz, &g), S_OK);
    EXPECT_EQ(sz, sizeof(void *)); CHECK(g == NULL);
    ID3D11DeviceContext_SetPrivateData(c, &PD2, 0, NULL);
}
TEST(d3d11_context_state_private_data_immediate) {
    OPEN();
    pd_matrix(f.ctx);
    CLOSE();
}
TEST(d3d11_context_state_private_data_deferred) {
    OPEN();
    ID3D11DeviceContext *d = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d))) { t_skip("deferred"); CLOSE(); return; }
    pd_matrix(d);
    /* private data is per object: the immediate context does not see the deferred context's entries */
    BYTE x = 7; char buf[4]; UINT sz = 4;
    ID3D11DeviceContext_SetPrivateData(d, &PD1, 1, &x);
    EXPECT_HR(ID3D11DeviceContext_GetPrivateData(f.ctx, &PD1, &sz, buf), DXGI_ERROR_NOT_FOUND);
    ID3D11DeviceContext_Release(d);
    CLOSE();
}
TEST(d3d11_context_state_private_data_interface_released_with_context) {
    OPEN();
    ID3D11DeviceContext *d = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d))) { t_skip("deferred"); CLOSE(); return; }
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!b) { t_skip("buf"); ID3D11DeviceContext_Release(d); CLOSE(); return; }
    ULONG base = RC(b);
    EXPECT_HR(ID3D11DeviceContext_SetPrivateDataInterface(d, &PD1, (IUnknown *)b), S_OK);
    EXPECT_EQ(RC(b), base + 1);
    EXPECT_EQ(ID3D11DeviceContext_Release(d), 0);
    EXPECT_EQ(RC(b), base); /* destroying the context drops the entry */
    SAFE_RELEASE(b);
    CLOSE();
}

/* ---- resource min LOD ----------------------------------------------------------------------- */
TEST(d3d11_context_state_resource_min_lod_getter_reads_zero) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 16, 16, 5, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    if (!t) { t_skip("texture"); CLOSE(); return; }
    EXPECT_EQ(ID3D11DeviceContext_GetResourceMinLOD(f.ctx, (ID3D11Resource *)t) == 0.0f, 1);
    /* Win10: SetResourceMinLOD is accepted for any value but GetResourceMinLOD always reads 0 */
    static const float lods[] = { 0.0f, 2.5f, 4.0f, 100.0f, -3.0f };
    for (unsigned i = 0; i < sizeof lods / sizeof lods[0]; i++) {
        ID3D11DeviceContext_SetResourceMinLOD(f.ctx, (ID3D11Resource *)t, lods[i]);
        FLOAT g = ID3D11DeviceContext_GetResourceMinLOD(f.ctx, (ID3D11Resource *)t);
        CHECKF(g == 0.0f, "min LOD after Set(%g) reads %g", lods[i], g);
    }
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_SHADER_RESOURCE, 64);
    if (b) { ID3D11DeviceContext_SetResourceMinLOD(f.ctx, (ID3D11Resource *)b, 1.0f); CHECK(ID3D11DeviceContext_GetResourceMinLOD(f.ctx, (ID3D11Resource *)b) == 0.0f); }
    SAFE_RELEASE(b); SAFE_RELEASE(t);
    CLOSE();
}

/* ---- ID3D11DeviceContext2: markers and tiled resources --------------------------------------- */
#define OPEN2X() csf f; ID3D11Device2 *d2 = NULL; ID3D11DeviceContext2 *c2 = NULL; \
    if (!csf_open(&f)) return; \
    ID3D11Device_QueryInterface(f.dev, &IID_ID3D11Device2, (void **)&d2); ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11DeviceContext2, (void **)&c2); \
    if (!d2 || !c2) { t_skip("no ID3D11Device2/ID3D11DeviceContext2"); if (d2) ID3D11Device2_Release(d2); if (c2) ID3D11DeviceContext2_Release(c2); CLOSE(); return; }
#define CLOSE2X() do { ID3D11DeviceContext_ClearState(f.ctx); ID3D11Device2_Release(d2); ID3D11DeviceContext2_Release(c2); CLOSE(); } while (0)

TEST(d3d11_context_state_marker_int_entry_points) {
    OPEN2X();
    EXPECT_EQ(ID3D11DeviceContext2_IsAnnotationEnabled(c2), FALSE); /* no capture tool attached */
    ID3D11DeviceContext2_SetMarkerInt(c2, L"marker", 5);
    ID3D11DeviceContext2_BeginEventInt(c2, L"event", 7);
    ID3D11DeviceContext2_EndEvent(c2);
    ID3D11DeviceContext2_EndEvent(c2); /* unbalanced: harmless */
    ID3D11DeviceContext2_SetMarkerInt(c2, NULL, 0);
    CLOSE2X();
}
static void tile_risky(void *p);
typedef struct { ID3D11DeviceContext2 *c2; ID3D11Texture2D *t; ID3D11Buffer *b; D3D11_TILED_RESOURCE_COORDINATE *co; D3D11_TILE_REGION_SIZE *rs; int which; HRESULT hr; } tilectx;
static void tile_risky(void *p)
{
    tilectx *x = p; x->hr = 0x7fffffff;
    switch (x->which) {
    case 0: x->hr = ID3D11DeviceContext2_CopyTileMappings(x->c2, (ID3D11Resource *)x->t, x->co, (ID3D11Resource *)x->t, x->co, x->rs, 0); break;
    case 1: x->hr = ID3D11DeviceContext2_ResizeTilePool(x->c2, x->b, 65536); break;
    case 2: ID3D11DeviceContext2_TiledResourceBarrier(x->c2, NULL, NULL); x->hr = S_OK; break;
    }
}
TEST(d3d11_context_state_tiled_options_warp) {
    OPEN2X();
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o1; memset(&o1, 0xcc, sizeof o1);
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(f.dev, D3D11_FEATURE_D3D11_OPTIONS1, &o1, sizeof o1), S_OK);
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2; memset(&o2, 0xcc, sizeof o2);
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(f.dev, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof o2), S_OK);
    EXPECT_EQ(o1.TiledResourcesTier, o2.TiledResourcesTier);
    if (f.dt == D3D_DRIVER_TYPE_WARP) {
        /* Win10 WARP: tiled resources tier 1, min/max filtering, no ROVs */
        EXPECT_EQ(o1.TiledResourcesTier, D3D11_TILED_RESOURCES_TIER_1);
        EXPECT_EQ(o1.MinMaxFiltering, TRUE);
        EXPECT_EQ(o2.ROVsSupported, FALSE);
        EXPECT_EQ(o2.ConservativeRasterizationTier, D3D11_CONSERVATIVE_RASTERIZATION_NOT_SUPPORTED);
    }
    CLOSE2X();
}
TEST(d3d11_context_state_tiled_ops_on_ordinary_resources) {
    OPEN2X();
    ID3D11Texture2D *t = cs_tex(&f, 16, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_SHADER_RESOURCE, 64);
    if (!t || !b) { t_skip("resources"); goto out; }
    D3D11_TILED_RESOURCE_COORDINATE co; memset(&co, 0, sizeof co);
    D3D11_TILE_REGION_SIZE rs; memset(&rs, 0, sizeof rs); rs.NumTiles = 1;
    UINT rf = 0, rc = 1;
    /* Win10: an ordinary (non-tiled) resource is rejected by UpdateTileMappings / ResizeTilePool; CopyTileMappings faults */
    EXPECT_HR(ID3D11DeviceContext2_UpdateTileMappings(c2, (ID3D11Resource *)t, 1, &co, &rs, NULL, 1, &rf, NULL, &rc, 0), E_INVALIDARG);
    tilectx tc = { c2, t, b, &co, &rs, 0, 0 };
    EXPECT_EQ(t_try(tile_risky, &tc), 0xc0000005);
    tc.which = 1; EXPECT_EQ(t_try(tile_risky, &tc), 0); EXPECT_HR(tc.hr, E_INVALIDARG);
    tc.which = 2; EXPECT_EQ(t_try(tile_risky, &tc), 0); /* barrier with no resources is a no-op */
out:
    SAFE_RELEASE(t); SAFE_RELEASE(b);
    CLOSE2X();
}
TEST(d3d11_context_state_tiled_resource_create_tiling_and_mapping) {
    OPEN2X();
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o1; memset(&o1, 0, sizeof o1);
    ID3D11Device_CheckFeatureSupport(f.dev, D3D11_FEATURE_D3D11_OPTIONS1, &o1, sizeof o1);
    if (o1.TiledResourcesTier < D3D11_TILED_RESOURCES_TIER_1) { t_skip("no tiled resources (tier %d)", o1.TiledResourcesTier); CLOSE2X(); return; }
    D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd); bd.ByteWidth = 65536 * 4; bd.Usage = D3D11_USAGE_DEFAULT; bd.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;
    ID3D11Buffer *pool = NULL; ID3D11Texture2D *t = NULL;
    EXPECT_HR(ID3D11Device_CreateBuffer(f.dev, &bd, NULL, &pool), S_OK);
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td); td.Width = 256; td.Height = 256; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.MiscFlags = D3D11_RESOURCE_MISC_TILED;
    EXPECT_HR(ID3D11Device_CreateTexture2D(f.dev, &td, NULL, &t), S_OK);
    if (!pool || !t) { t_skip("tiled create failed"); goto out; }
    D3D11_TEXTURE2D_DESC g; ID3D11Texture2D_GetDesc(t, &g);
    EXPECT_EQ(g.MiscFlags, D3D11_RESOURCE_MISC_TILED);
    UINT ntiles = 0, nsub = 1; D3D11_PACKED_MIP_DESC pm; D3D11_TILE_SHAPE ts; D3D11_SUBRESOURCE_TILING st; memset(&st, 0, sizeof st);
    ID3D11Device2_GetResourceTiling(d2, (ID3D11Resource *)t, &ntiles, &pm, &ts, &nsub, 0, &st);
    EXPECT_EQ(ntiles, 4);                        /* 256x256 RGBA8 = four 64 KB tiles */
    EXPECT_EQ(ts.WidthInTexels, 128); EXPECT_EQ(ts.HeightInTexels, 128); EXPECT_EQ(ts.DepthInTexels, 1);
    EXPECT_EQ(st.WidthInTiles, 2); EXPECT_EQ(st.HeightInTiles, 2); EXPECT_EQ(st.DepthInTiles, 1); EXPECT_EQ(st.StartTileIndexInOverallResource, 0);
    EXPECT_EQ(pm.NumStandardMips, 1); EXPECT_EQ(pm.NumPackedMips, 0);
    D3D11_TILED_RESOURCE_COORDINATE co; memset(&co, 0, sizeof co); D3D11_TILE_REGION_SIZE rs; memset(&rs, 0, sizeof rs); rs.NumTiles = 4;
    UINT rf = 0, ro = 0, rc = 4;
    EXPECT_HR(ID3D11DeviceContext2_UpdateTileMappings(c2, (ID3D11Resource *)t, 1, &co, &rs, pool, 1, &rf, &ro, &rc, 0), S_OK);
    /* pool resize: grow/shrink in whole 64 KB tiles, unaligned sizes rejected */
    EXPECT_HR(ID3D11DeviceContext2_ResizeTilePool(c2, pool, 65536 * 8), S_OK);
    EXPECT_HR(ID3D11DeviceContext2_ResizeTilePool(c2, pool, 65536 * 4), S_OK);
    EXPECT_HR(ID3D11DeviceContext2_ResizeTilePool(c2, pool, 65536 * 4 + 1), E_INVALIDARG);
    /* tiled resources cannot be mapped; SRV creation works */
    D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(ID3D11DeviceContext_Map(f.ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &m), E_INVALIDARG);
    ID3D11ShaderResourceView *v = NULL;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(f.dev, (ID3D11Resource *)t, NULL, &v), S_OK);
    SAFE_RELEASE(v);
    ID3D11DeviceContext2_TiledResourceBarrier(c2, (ID3D11DeviceChild *)t, (ID3D11DeviceChild *)t);
out:
    SAFE_RELEASE(pool); SAFE_RELEASE(t);
    CLOSE2X();
}
