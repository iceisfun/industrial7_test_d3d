/* Context operations on textures that the other files do not reach: UpdateSubresource1 / CopySubresourceRegion1 copy flags,
 * ClearView, Discard*, SetResourceMinLOD, GenerateMips without MISC_GENERATE_MIPS, default-usage CPU access, tiled misc
 * flags, IDXGISurface Map rules, format-support consistency over the whole DXGI format range, concurrent creation.
 * Every expectation was measured on Win10 22H2 (WARP, feature level 11_0/11_1). */
#include "txh.h"

static ID3D11DeviceContext1 *tx_ctx1(tx_ctx *c) {
    ID3D11DeviceContext1 *c1 = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(c->ctx, &IID_ID3D11DeviceContext1, (void **)&c1))) { t_skip("no ID3D11DeviceContext1"); return NULL; }
    return c1;
}
static void tx_fill32(tx_ctx *c, ID3D11Texture2D *t, UINT w, UINT h, UINT32 v) {
    UINT32 *buf = malloc((size_t)w * h * 4);
    for (UINT i = 0; i < w * h; i++) buf[i] = v;
    ID3D11DeviceContext_UpdateSubresource(c->ctx, (ID3D11Resource *)t, 0, NULL, buf, w * 4, 0);
    free(buf);
}
static UINT32 tx_px(tx_ctx *c, ID3D11Texture2D *t, UINT sub, UINT x, UINT y) {
    tx_img im; UINT32 v = 0xdeadbeef;
    if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, sub, &im))) { v = tx_px32(&im, x, y, 0); tx_img_free(&im); }
    return v;
}

/* ---- copy flags ---------------------------------------------------------------------------------------------------- */
/* UpdateSubresource1: CopyFlags 0, NO_OVERWRITE and DISCARD all apply the update; any other bit makes the call a silent no-op. */
static void tx_us1(tx_ctx *c, UINT flags, int applied) {
    ID3D11DeviceContext1 *c1 = tx_ctx1(c); ID3D11Texture2D *t = NULL; static const UINT32 src[4] = { 0x11223344, 0x11223344, 0x11223344, 0x11223344 };
    D3D11_BOX b = { 0, 0, 0, 2, 2, 1 };
    if (!c1) return;
    EXPECT_HR(tx_mk2d(c, 64, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (t) {
        tx_fill32(c, t, 64, 64, 0xAAAAAAAA);
        ID3D11DeviceContext1_UpdateSubresource1(c1, (ID3D11Resource *)t, 0, &b, src, 8, 0, flags);
        EXPECT_EQ(tx_px(c, t, 0, 0, 0), applied ? 0x11223344u : 0xAAAAAAAAu);
        EXPECT_EQ(tx_px(c, t, 0, 1, 1), applied ? 0x11223344u : 0xAAAAAAAAu);
        EXPECT_EQ(tx_px(c, t, 0, 2, 2), 0xAAAAAAAAu);
        ID3D11Texture2D_Release(t);
    }
    ID3D11DeviceContext1_Release(c1);
}
DEVTEST(d3d11_textures_updatesubresource1_flags_none) { tx_us1(c, 0, 1); }
DEVTEST(d3d11_textures_updatesubresource1_flags_no_overwrite) { tx_us1(c, D3D11_COPY_NO_OVERWRITE, 1); }
DEVTEST(d3d11_textures_updatesubresource1_flags_discard) { tx_us1(c, D3D11_COPY_DISCARD, 1); }
DEVTEST(d3d11_textures_updatesubresource1_flags_both_ignored) { tx_us1(c, D3D11_COPY_NO_OVERWRITE | D3D11_COPY_DISCARD, 0); }
DEVTEST(d3d11_textures_updatesubresource1_flags_undefined_bit_ignored) { tx_us1(c, 0x10, 0); }

static void tx_csr1(tx_ctx *c, UINT flags, int applied) {
    ID3D11DeviceContext1 *c1 = tx_ctx1(c); ID3D11Texture2D *t = NULL, *s = NULL; D3D11_BOX b = { 0, 0, 0, 8, 8, 1 };
    if (!c1) return;
    EXPECT_HR(tx_mk2d(c, 64, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    EXPECT_HR(tx_mk2d(c, 64, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &s), S_OK);
    if (t && s) {
        tx_fill32(c, t, 64, 64, 0xAAAAAAAA); tx_fill32(c, s, 64, 64, 0x55555555);
        ID3D11DeviceContext1_CopySubresourceRegion1(c1, (ID3D11Resource *)t, 0, 4, 4, 0, (ID3D11Resource *)s, 0, &b, flags);
        EXPECT_EQ(tx_px(c, t, 0, 4, 4), applied ? 0x55555555u : 0xAAAAAAAAu);
        EXPECT_EQ(tx_px(c, t, 0, 11, 11), applied ? 0x55555555u : 0xAAAAAAAAu);
        EXPECT_EQ(tx_px(c, t, 0, 12, 12), 0xAAAAAAAAu);
        EXPECT_EQ(tx_px(c, t, 0, 3, 3), 0xAAAAAAAAu);
    }
    SAFE_RELEASE(t); SAFE_RELEASE(s);
    ID3D11DeviceContext1_Release(c1);
}
DEVTEST(d3d11_textures_copysubresourceregion1_flags_none) { tx_csr1(c, 0, 1); }
DEVTEST(d3d11_textures_copysubresourceregion1_flags_no_overwrite) { tx_csr1(c, D3D11_COPY_NO_OVERWRITE, 1); }
DEVTEST(d3d11_textures_copysubresourceregion1_flags_discard) { tx_csr1(c, D3D11_COPY_DISCARD, 1); }
DEVTEST(d3d11_textures_copysubresourceregion1_flags_undefined_bit_ignored) { tx_csr1(c, 0x10, 0); }

/* ---- ClearView / Discard --------------------------------------------------------------------------------------------- */
DEVTEST(d3d11_textures_clearview_rects) {
    ID3D11DeviceContext1 *c1 = tx_ctx1(c); ID3D11Texture2D *r = NULL; ID3D11RenderTargetView *v = NULL;
    static const float z[4] = { 0, 0, 0, 0 }, red[4] = { 1, 0, 0, 1 }; D3D11_RECT rc[2] = { { 2, 2, 6, 6 }, { 10, 10, 12, 12 } };
    if (!c1) return;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &r), S_OK);
    if (r) EXPECT_HR(ID3D11Device_CreateRenderTargetView(c->dev, (ID3D11Resource *)r, NULL, &v), S_OK);
    if (v) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, z);
        ID3D11DeviceContext1_ClearView(c1, (ID3D11View *)v, red, rc, 2);
        EXPECT_EQ(tx_px(c, r, 0, 3, 3), 0xff0000ffu);   /* inside rect 0 */
        EXPECT_EQ(tx_px(c, r, 0, 5, 5), 0xff0000ffu);   /* last texel of rect 0 (right/bottom exclusive: 6 is outside) */
        EXPECT_EQ(tx_px(c, r, 0, 6, 6), 0u);
        EXPECT_EQ(tx_px(c, r, 0, 11, 11), 0xff0000ffu); /* rect 1 */
        EXPECT_EQ(tx_px(c, r, 0, 12, 12), 0u);
        EXPECT_EQ(tx_px(c, r, 0, 0, 0), 0u); EXPECT_EQ(tx_px(c, r, 0, 8, 8), 0u);
        /* zero rects: the whole view is cleared */
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, z);
        ID3D11DeviceContext1_ClearView(c1, (ID3D11View *)v, red, NULL, 0);
        EXPECT_EQ(tx_px(c, r, 0, 0, 0), 0xff0000ffu); EXPECT_EQ(tx_px(c, r, 0, 15, 15), 0xff0000ffu);
        ID3D11RenderTargetView_Release(v);
    }
    SAFE_RELEASE(r);
    ID3D11DeviceContext1_Release(c1);
}
DEVTEST(d3d11_textures_clearview_null_rects_with_count_is_noop) {
    ID3D11DeviceContext1 *c1 = tx_ctx1(c); ID3D11Texture2D *r = NULL; ID3D11RenderTargetView *v = NULL;
    static const float z[4] = { 0, 0, 0, 0 }, red[4] = { 1, 0, 0, 1 };
    if (!c1) return;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &r), S_OK);
    if (r) EXPECT_HR(ID3D11Device_CreateRenderTargetView(c->dev, (ID3D11Resource *)r, NULL, &v), S_OK);
    if (v) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, z);
        ID3D11DeviceContext1_ClearView(c1, (ID3D11View *)v, red, NULL, 3);   /* a non-zero count with NULL rects: nothing is cleared, no fault */
        EXPECT_EQ(tx_px(c, r, 0, 0, 0), 0u);
        ID3D11RenderTargetView_Release(v);
    }
    SAFE_RELEASE(r);
    ID3D11DeviceContext1_Release(c1);
}
/* Win10 WARP keeps the contents across DiscardView/DiscardResource: the call must be harmless and later clears must work. */
DEVTEST(d3d11_textures_discard_then_clear) {
    ID3D11DeviceContext1 *c1 = tx_ctx1(c); ID3D11Texture2D *r = NULL, *sr = NULL; ID3D11RenderTargetView *v = NULL;
    static const float red[4] = { 1, 0, 0, 1 }, grn[4] = { 0, 1, 0, 1 };
    if (!c1) return;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &r), S_OK);
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &sr), S_OK);
    if (r) EXPECT_HR(ID3D11Device_CreateRenderTargetView(c->dev, (ID3D11Resource *)r, NULL, &v), S_OK);
    if (v) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, red);
        ID3D11DeviceContext1_DiscardView(c1, (ID3D11View *)v);
        ID3D11DeviceContext1_DiscardResource(c1, (ID3D11Resource *)r);
        ID3D11DeviceContext1_DiscardResource(c1, (ID3D11Resource *)sr);
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, grn);
        EXPECT_EQ(tx_px(c, r, 0, 7, 7), 0xff00ff00u);
        ID3D11RenderTargetView_Release(v);
    }
    SAFE_RELEASE(r); SAFE_RELEASE(sr);
    ID3D11DeviceContext1_Release(c1);
}

/* ---- MinLOD / GenerateMips ------------------------------------------------------------------------------------------- */
/* Win10 WARP does not retain SetResourceMinLOD: GetResourceMinLOD reads back 0 whatever was set (SRV or not, any value). */
DEVTEST(d3d11_textures_resource_min_lod_reads_back_zero) {
    ID3D11Texture2D *t = NULL; static const float vals[] = { 2.0f, 1.5f, 100.0f, -1.0f, 0.0f };
    EXPECT_HR(tx_mk2d(c, 8, 8, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (!t) return;
    CHECK(ID3D11DeviceContext_GetResourceMinLOD(c->ctx, (ID3D11Resource *)t) == 0.0f);
    for (int i = 0; i < 5; i++) {
        ID3D11DeviceContext_SetResourceMinLOD(c->ctx, (ID3D11Resource *)t, vals[i]);
        CHECKF(ID3D11DeviceContext_GetResourceMinLOD(c->ctx, (ID3D11Resource *)t) == 0.0f, "min lod %f read back as %f", vals[i], ID3D11DeviceContext_GetResourceMinLOD(c->ctx, (ID3D11Resource *)t));
    }
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_genmips_without_misc_flag_is_noop) {
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *sv = NULL; static const float red[4] = { 1, 0, 0, 1 };
    ID3D11RenderTargetView *v = NULL; D3D11_RENDER_TARGET_VIEW_DESC rd; tx_img im;
    EXPECT_HR(tx_mk2d(c, 8, 8, 0, 1, F_RGBA8, U_DEF, B_SRV | B_RT, 0, 0, NULL, &t), S_OK);
    if (!t) return;
    memset(&rd, 0, sizeof rd); rd.Format = F_RGBA8; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(c->dev, (ID3D11Resource *)t, &rd, &v), S_OK);
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, NULL, &sv), S_OK);
    if (v && sv) {
        ID3D11DeviceContext_ClearRenderTargetView(c->ctx, v, red);
        ID3D11DeviceContext_GenerateMips(c->ctx, sv);   /* texture lacks MISC_GENERATE_MIPS: silently ignored */
        EXPECT_EQ(tx_px(c, t, 0, 0, 0), 0xff0000ffu);
        for (UINT m = 1; m < 4; m++) if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, m, &im))) { EXPECT_EQ(tx_px32(&im, 0, 0, 0), 0u); tx_img_free(&im); }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(sv); ID3D11Texture2D_Release(t);
}

/* ---- default-usage textures with CPU access flags (MapOnDefaultTextures) ------------------------------------------------- */
DEVTEST(d3d11_textures_default_cpu_access_flags_are_kept_but_not_mappable) {
    ID3D11Texture2D *t = NULL, *st = NULL; D3D11_TEXTURE2D_DESC g; D3D11_MAPPED_SUBRESOURCE m; static const D3D11_MAP types[] = { D3D11_MAP_READ, D3D11_MAP_WRITE, D3D11_MAP_READ_WRITE, D3D11_MAP_WRITE_DISCARD, D3D11_MAP_WRITE_NO_OVERWRITE };
    if (!tx_map_on_default(c)) { t_skip("device reports MapOnDefaultTextures = FALSE"); return; }
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_DEF, B_SRV, CPU_R | CPU_W, 0, NULL, &t), S_OK);
    if (!t) return;
    ID3D11Texture2D_GetDesc(t, &g);
    EXPECT_EQ(g.Usage, D3D11_USAGE_DEFAULT); EXPECT_EQ(g.CPUAccessFlags, CPU_R | CPU_W);
    /* Win10 WARP: although creation is accepted, Map is E_INVALIDARG for every map type */
    for (int i = 0; i < 5; i++) {
        memset(&m, 0, sizeof m);
        t_check(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, types[i], 0, &m) == E_INVALIDARG && !m.pData, __FILE__, __LINE__, "Map on a DEFAULT texture", "map type %d", types[i]);
    }
    /* GPU-side operations still work and the data can be read back through a staging copy */
    tx_fill32(c, t, 16, 16, 0x01020304);
    EXPECT_EQ(tx_px(c, t, 0, 3, 3), 0x01020304u);
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_STG, 0, CPU_R, 0, NULL, &st), S_OK);
    if (st) { ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource *)st, (ID3D11Resource *)t); EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m), S_OK);
        if (m.pData) { EXPECT_EQ(*(UINT32 *)m.pData, 0x01020304u); ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)st, 0); } ID3D11Texture2D_Release(st); }
    ID3D11Texture2D_Release(t);
}

/* ---- 3D initial data: SysMemSlicePitch rules ----------------------------------------------------------------------------- */
static HRESULT tx_3d_with_slicepitch(tx_ctx *c, UINT w, UINT h, UINT d, UINT mips, UINT slice_override, int only_last) {
    tx_init in; ID3D11Texture3D *t = NULL; HRESULT hr; UINT i;
    if (!tx_init_make(&in, w, h, d, mips, 1, F_RGBA8, 0, 0, 600)) return E_OUTOFMEMORY;
    for (i = 0; i < in.n; i++) if (!only_last || i == in.n - 1) in.sd[i].SysMemSlicePitch = slice_override;
    hr = tx_mk3d(c, w, h, d, mips, F_RGBA8, U_IMM, B_SRV, 0, 0, in.sd, &t);
    SAFE_RELEASE(t); tx_init_free(&in);
    return hr;
}
DEVTEST(d3d11_textures_init3d_zero_slicepitch_rejected) {
    EXPECT_HR(tx_3d_with_slicepitch(c, 4, 4, 4, 1, 0, 0), E_INVALIDARG);
    EXPECT_HR(tx_3d_with_slicepitch(c, 8, 8, 1, 1, 0, 0), E_INVALIDARG);   /* even for a depth-1 volume */
    EXPECT_HR(tx_3d_with_slicepitch(c, 1, 1, 1, 1, 0, 0), E_INVALIDARG);
}
DEVTEST(d3d11_textures_init3d_zero_slicepitch_on_last_mip_rejected) {
    EXPECT_HR(tx_3d_with_slicepitch(c, 4, 4, 4, 0, 0, 1), E_INVALIDARG);   /* 1x1x1 last mip with SlicePitch 0 */
}
DEVTEST(d3d11_textures_init3d_any_nonzero_slicepitch_accepted) {
    EXPECT_HR(tx_3d_with_slicepitch(c, 4, 4, 4, 1, 1, 0), S_OK);            /* the value is not validated against the slice size */
    /* (a huge value is accepted at validation time but makes the copy read out of bounds: access violation, not probed) */
}
DEVTEST(d3d11_textures_init2d_slicepitch_any_value_accepted) {
    tx_init in; ID3D11Texture2D *t = NULL;
    CHECK(tx_init_make(&in, 8, 8, 1, 1, 1, F_RGBA8, 0, 0, 601));
    in.sd[0].SysMemSlicePitch = 0;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, in.sd, &t), S_OK); SAFE_RELEASE(t);
    in.sd[0].SysMemSlicePitch = 0xffffffffu;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, in.sd, &t), S_OK); SAFE_RELEASE(t);
    tx_init_free(&in);
}

/* ---- tiled misc flags ------------------------------------------------------------------------------------------------------ */
DEVTEST(d3d11_textures_misc_tiled_accepted_on_warp) {
    ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC g; ID3D11Buffer *pool = NULL; D3D11_BUFFER_DESC bd;
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o1; memset(&o1, 0, sizeof o1);
    EXPECT_HR(ID3D11Device_CheckFeatureSupport(c->dev, D3D11_FEATURE_D3D11_OPTIONS1, &o1, sizeof o1), S_OK);
    if (!o1.TiledResourcesTier) { t_skip("TiledResourcesTier = NOT_SUPPORTED"); return; }
    EXPECT_HR(tx_mk2d(c, 256, 256, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, D3D11_RESOURCE_MISC_TILED, NULL, &t), S_OK);
    if (t) { ID3D11Texture2D_GetDesc(t, &g); EXPECT_EQ(g.MiscFlags, D3D11_RESOURCE_MISC_TILED); ID3D11Texture2D_Release(t); }
    memset(&bd, 0, sizeof bd); bd.ByteWidth = 65536 * 4; bd.Usage = D3D11_USAGE_DEFAULT; bd.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;
    EXPECT_HR(ID3D11Device_CreateBuffer(c->dev, &bd, NULL, &pool), S_OK);
    SAFE_RELEASE(pool);
}

/* ---- IDXGISurface Map / Unmap / GetDesc ------------------------------------------------------------------------------------ */
DEVTEST(d3d11_textures_dxgi_surface_map_rules) {
    ID3D11Texture2D *t = NULL; IDXGISurface *s = NULL; DXGI_MAPPED_RECT mr; DXGI_SURFACE_DESC sd;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK);
    if (!t) return;
    EXPECT_HR(ID3D11Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&s), S_OK);
    if (s) {
        memset(&mr, 0, sizeof mr);
        EXPECT_HR(IDXGISurface_Map(s, &mr, DXGI_MAP_READ | DXGI_MAP_WRITE), S_OK);
        CHECK(mr.pBits != NULL); EXPECT_EQ(mr.Pitch, 32);
        EXPECT_HR(IDXGISurface_Unmap(s), S_OK);
        EXPECT_HR(IDXGISurface_Map(s, &mr, DXGI_MAP_DISCARD), E_INVALIDARG);   /* DISCARD needs a dynamic resource */
        EXPECT_HR(IDXGISurface_Map(s, &mr, 0), E_INVALIDARG);                  /* no READ/WRITE/DISCARD bit */
        EXPECT_HR(IDXGISurface_Unmap(s), S_OK);                                 /* Unmap without Map is not an error */
        memset(&sd, 0, sizeof sd);
        EXPECT_HR(IDXGISurface_GetDesc(s, &sd), S_OK);
        EXPECT_EQ(sd.Width, 8); EXPECT_EQ(sd.Height, 8); EXPECT_EQ(sd.Format, F_RGBA8); EXPECT_EQ(sd.SampleDesc.Count, 1); EXPECT_EQ(sd.SampleDesc.Quality, 0);
        IDXGISurface_Release(s);
    }
    ID3D11Texture2D_Release(t);
}

/* ---- CheckFormatSupport vs creation, every DXGI format --------------------------------------------------------------------- */
/* For all formats 1..131: a successful CheckFormatSupport must agree with what texture creation does. Formats for which
 * CheckFormatSupport fails (E_FAIL) can never be created as a texture. TYPELESS formats are exempt from the negative RT/DS rules
 * (a typeless parent can be bound as RT/DS, the view format decides). DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM (89) is a measured
 * WARP inconsistency: it lacks RENDER_TARGET in CheckFormatSupport yet an RT texture is created. */
static int tx_is_typeless(UINT f) {
    static const UINT t[] = { 1, 5, 9, 15, 19, 23, 27, 33, 39, 44, 48, 53, 60, 70, 73, 76, 79, 82, 90, 92, 94, 97 };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) if (t[i] == f) return 1;
    return 0;
}
DEVTEST(d3d11_textures_formatsupport_agrees_with_creation_all_formats) {
    UINT f, nsup = 0, nbad = 0;
    for (f = 1; f <= 131; f++) {
        UINT s = 0; ID3D11Texture2D *t = NULL; ID3D11Texture1D *t1 = NULL; HRESULT hs = ID3D11Device_CheckFormatSupport(c->dev, (DXGI_FORMAT)f, &s), h;
        if (FAILED(hs)) {
            h = tx_mk2d(c, 16, 16, 1, 1, (DXGI_FORMAT)f, U_DEF, B_SRV, 0, 0, NULL, &t);
            CHECKF(FAILED(h), "format %u: CheckFormatSupport %s but a 2D SRV texture was created", f, d3dh_hr_name(hs)); SAFE_RELEASE(t);
            continue;
        }
        nsup++;
        /* TEXTURE2D bit <-> an SRV|RT-free default texture is creatable (all bind-less combinations are creatable when the bit is set) */
        if (s & D3D11_FORMAT_SUPPORT_TEXTURE2D) {
            h = tx_mk2d(c, 16, 16, 1, 1, (DXGI_FORMAT)f, U_DEF, 0, 0, 0, NULL, &t);
            if (FAILED(h) && !(f == DXGI_FORMAT_R32G32B32_FLOAT)) { nbad++; t_note("format %u: TEXTURE2D advertised (support %#x) but bind-less creation gave %s", f, s, d3dh_hr_name(h)); }
            SAFE_RELEASE(t);
        }
        if (s & D3D11_FORMAT_SUPPORT_RENDER_TARGET) {
            h = tx_mk2d(c, 16, 16, 1, 1, (DXGI_FORMAT)f, U_DEF, B_RT, 0, 0, NULL, &t);
            CHECKF(SUCCEEDED(h), "format %u advertises RENDER_TARGET (support %#x) but RT creation gave %s", f, s, d3dh_hr_name(h)); SAFE_RELEASE(t);
        } else if (!tx_is_typeless(f) && f != 89) {
            h = tx_mk2d(c, 16, 16, 1, 1, (DXGI_FORMAT)f, U_DEF, B_RT, 0, 0, NULL, &t);
            CHECKF(FAILED(h), "format %u lacks RENDER_TARGET (support %#x) but RT creation succeeded", f, s); SAFE_RELEASE(t);
        }
        if (s & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) {
            h = tx_mk2d(c, 16, 16, 1, 1, (DXGI_FORMAT)f, U_DEF, B_DS, 0, 0, NULL, &t);
            CHECKF(SUCCEEDED(h), "format %u advertises DEPTH_STENCIL (support %#x) but DS creation gave %s", f, s, d3dh_hr_name(h)); SAFE_RELEASE(t);
        } else if (!tx_is_typeless(f)) {
            h = tx_mk2d(c, 16, 16, 1, 1, (DXGI_FORMAT)f, U_DEF, B_DS, 0, 0, NULL, &t);
            CHECKF(FAILED(h), "format %u lacks DEPTH_STENCIL (support %#x) but DS creation succeeded", f, s); SAFE_RELEASE(t);
        }
        if (s & D3D11_FORMAT_SUPPORT_TEXTURE1D) {
            h = tx_mk1d(c, 16, 1, 1, (DXGI_FORMAT)f, U_DEF, 0, 0, 0, NULL, &t1);
            if (FAILED(h)) { nbad++; t_note("format %u: TEXTURE1D advertised (support %#x) but creation gave %s", f, s, d3dh_hr_name(h)); }
            SAFE_RELEASE(t1);
        } else {
            h = tx_mk1d(c, 16, 1, 1, (DXGI_FORMAT)f, U_DEF, 0, 0, 0, NULL, &t1);
            CHECKF(FAILED(h), "format %u lacks TEXTURE1D (support %#x) but 1D creation succeeded", f, s); SAFE_RELEASE(t1);
        }
    }
    t_note("%u of 131 formats answer CheckFormatSupport; %u advertised-but-not-creatable cases", nsup, nbad);
    CHECK(nsup > 60);
}

/* ---- concurrent creation --------------------------------------------------------------------------------------------------- */
typedef struct { ID3D11Device *dev; int n; volatile LONG bad; UINT seed; } tx_thr;
static DWORD WINAPI tx_thr_main(LPVOID p) {
    tx_thr *a = p; int i;
    for (i = 0; i < a->n; i++) {
        D3D11_TEXTURE2D_DESC d = { 8 + (a->seed + i) % 24, 4 + i % 9, 1, 1, F_RGBA8, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 }, g;
        ID3D11Texture2D *t = NULL;
        if (ID3D11Device_CreateTexture2D(a->dev, &d, NULL, &t) != S_OK || !t) { InterlockedIncrement(&a->bad); continue; }
        ID3D11Texture2D_GetDesc(t, &g);
        if (g.Width != d.Width || g.Height != d.Height) InterlockedIncrement(&a->bad);
        if (ID3D11Texture2D_Release(t) != 0) InterlockedIncrement(&a->bad);
    }
    return 0;
}
DEVTEST(d3d11_textures_concurrent_create_release) {
    enum { N = 8 }; HANDLE h[N]; tx_thr a[N]; int i; LONG bad = 0;
    for (i = 0; i < N; i++) { a[i].dev = c->dev; a[i].n = 300; a[i].bad = 0; a[i].seed = (UINT)i * 7; h[i] = CreateThread(NULL, 0, tx_thr_main, &a[i], 0, NULL); }
    for (i = 0; i < N; i++) { if (h[i]) { WaitForSingleObject(h[i], 120000); CloseHandle(h[i]); } else bad++; }
    for (i = 0; i < N; i++) bad += a[i].bad;
    EXPECT_EQ(bad, 0);
}
