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

/* ---- context behaviour per feature level (WARP, feature level requested explicitly) ------------------- */
static void fl_row(D3D_FEATURE_LEVEL want, int so_targets)
{
    csf f; memset(&f, 0, sizeof f);
    D3D_FEATURE_LEVEL got = 0;
    HRESULT hr = d3dh_make_device11_fl(&f.dev, &f.ctx, &want, 1, &got, &f.dt, 0);
    if (FAILED(hr)) { t_skip("feature level 0x%x not available: %s", want, d3dh_hr_name(hr)); return; }
    if (f.dt != D3D_DRIVER_TYPE_WARP) { t_skip("measured for WARP"); csf_close(&f); return; }
    EXPECT_EQ(got, want);
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(f.dev), want);
    /* 16 viewports / scissor rects are accepted at every feature level */
    D3D11_VIEWPORT vp[16]; D3D11_RECT rc[16]; UINT n = 0, m = 0;
    for (int i = 0; i < 16; i++) { vp[i].TopLeftX = (FLOAT)i; vp[i].TopLeftY = (FLOAT)i; vp[i].Width = 10; vp[i].Height = 10; vp[i].MinDepth = 0; vp[i].MaxDepth = 1; rc[i].left = 0; rc[i].top = 0; rc[i].right = 5; rc[i].bottom = 5; }
    ID3D11DeviceContext_RSSetViewports(f.ctx, 16, vp); ID3D11DeviceContext_RSGetViewports(f.ctx, &n, NULL);
    ID3D11DeviceContext_RSSetScissorRects(f.ctx, 16, rc); ID3D11DeviceContext_RSGetScissorRects(f.ctx, &m, NULL);
    EXPECT_EQ(n, 16); EXPECT_EQ(m, 16);
    /* stream-output targets exist from 10_0 up; at 9_x SOSetTargets is silently ignored */
    ID3D11Buffer *b[4]; ID3D11Buffer *g[4] = { 0 }; UINT off[4] = { 0 }; int bound = 0;
    for (int i = 0; i < 4; i++) b[i] = cs_buf(&f, D3D11_BIND_STREAM_OUTPUT, 256);
    if (b[0] && b[1] && b[2] && b[3]) {
        ID3D11DeviceContext_SOSetTargets(f.ctx, 4, b, off);
        ID3D11DeviceContext_SOGetTargets(f.ctx, 4, g);
        for (int i = 0; i < 4; i++) { if (g[i] == b[i]) bound++; SAFE_RELEASE(g[i]); }
        EXPECT_EQ(bound, so_targets);
    } else if (so_targets) t_skip("stream output buffers not creatable at 0x%x", want);
    /* eight render targets, one texture each, are all bound */
    ID3D11Texture2D *t[8] = { 0 }; ID3D11RenderTargetView *rv[8] = { 0 }, *gr[8] = { 0 }; int nr = 0, ok = 1;
    for (int i = 0; i < 8; i++) { t[i] = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET); rv[i] = t[i] ? cs_rtv(&f, (ID3D11Resource *)t[i], NULL) : NULL; if (!rv[i]) ok = 0; }
    if (ok) {
        ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 8, rv, NULL);
        ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 8, gr, NULL);
        for (int i = 0; i < 8; i++) { if (gr[i] == rv[i]) nr++; SAFE_RELEASE(gr[i]); }
        EXPECT_EQ(nr, 8);
    }
    /* a deferred context can be made at every feature level */
    ID3D11DeviceContext *d = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(f.dev, 0, &d), S_OK);
    /* compute shaders: a cs_5_0 blob needs 11_0 or higher */
    ID3D11ComputeShader *cs = mk_CS(&f);
    CHECKF((cs != NULL) == (want >= D3D_FEATURE_LEVEL_11_0), "cs_5_0 shader creation at 0x%x: %s", want, cs ? "created" : "failed");
    D3D11_FEATURE_DATA_THREADING th; memset(&th, 0, sizeof th);
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(f.dev, D3D11_FEATURE_THREADING, &th, sizeof th), S_OK);
    EXPECT_EQ(th.DriverConcurrentCreates, TRUE); EXPECT_EQ(th.DriverCommandLists, FALSE);   /* WARP: concurrent creates yes, driver command lists no */
    ID3D11DeviceContext_ClearState(f.ctx);
    for (int i = 0; i < 4; i++) SAFE_RELEASE(b[i]);
    for (int i = 0; i < 8; i++) { SAFE_RELEASE(rv[i]); SAFE_RELEASE(t[i]); }
    SAFE_RELEASE(d); SAFE_RELEASE(cs);
    csf_close(&f);
}
TEST(d3d11_context_state_fl_matrix_11_1) { fl_row(D3D_FEATURE_LEVEL_11_1, 4); }
TEST(d3d11_context_state_fl_matrix_11_0) { fl_row(D3D_FEATURE_LEVEL_11_0, 4); }
TEST(d3d11_context_state_fl_matrix_10_1) { fl_row(D3D_FEATURE_LEVEL_10_1, 4); }
TEST(d3d11_context_state_fl_matrix_10_0) { fl_row(D3D_FEATURE_LEVEL_10_0, 4); }
TEST(d3d11_context_state_fl_matrix_9_3) { fl_row(D3D_FEATURE_LEVEL_9_3, 0); }
TEST(d3d11_context_state_fl_matrix_9_2) { fl_row(D3D_FEATURE_LEVEL_9_2, 0); }
TEST(d3d11_context_state_fl_matrix_9_1) { fl_row(D3D_FEATURE_LEVEL_9_1, 0); }

/* The same texture bound twice (through one RTV or two) in one OMSetRenderTargets call is a write hazard. */
TEST(d3d11_context_state_om_same_resource_in_two_rtv_slots_binds_nothing) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11RenderTargetView *v[2] = { t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL, t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL }, *g[2] = { 0 };
    if (!v[0] || !v[1]) { t_skip("rtv"); goto out; }
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 2, v, NULL);
    ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 2, g, NULL);
    CHECK(g[0] == NULL && g[1] == NULL);   /* Win10: the hazardous call is dropped as a whole, nothing is bound */
    SAFE_RELEASE(g[0]); SAFE_RELEASE(g[1]);
    ID3D11RenderTargetView *same[2] = { v[0], v[0] };
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 2, same, NULL);
    ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 2, g, NULL);
    CHECK(g[0] == NULL && g[1] == NULL);
    SAFE_RELEASE(g[0]); SAFE_RELEASE(g[1]);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(v[0]); SAFE_RELEASE(v[1]); SAFE_RELEASE(t);
    CLOSE();
}
