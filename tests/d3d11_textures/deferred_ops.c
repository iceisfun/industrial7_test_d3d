/* Texture operations recorded on deferred contexts and replayed on the immediate context (Win10 22H2 WARP). */
#include "txh.h"

static UINT32 df_px(tx_ctx *c, ID3D11Texture2D *t, UINT x, UINT y) {
    tx_img im; UINT32 v = 0xdeadbeef;
    if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, 0, &im))) { v = tx_px32(&im, x, y, 0); tx_img_free(&im); }
    return v;
}
static void df_fill(tx_ctx *c, ID3D11DeviceContext *ctx, ID3D11Texture2D *t, UINT w, UINT h, UINT32 v) {
    UINT32 *b = malloc((size_t)w * h * 4); UINT i;
    for (i = 0; i < w * h; i++) b[i] = v;
    ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource *)t, 0, NULL, b, w * 4, 0);
    free(b); (void)c;
}

DEVTEST(d3d11_textures_deferred_update_visible_after_execute) {
    ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL; ID3D11Texture2D *t = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(c->dev, 0, &dc), S_OK);
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (dc && t) {
        df_fill(c, c->ctx, t, 8, 8, 0x11111111);
        df_fill(c, dc, t, 8, 8, 0x22222222);
        EXPECT_EQ(df_px(c, t, 0, 0), 0x11111111u);                       /* nothing happens until the list runs */
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
        EXPECT_EQ(df_px(c, t, 0, 0), 0x11111111u);
        if (cl) {
            ID3D11DeviceContext_ExecuteCommandList(c->ctx, cl, FALSE);
            EXPECT_EQ(df_px(c, t, 7, 7), 0x22222222u);
            /* a command list can be executed more than once */
            df_fill(c, c->ctx, t, 8, 8, 0x33333333);
            ID3D11DeviceContext_ExecuteCommandList(c->ctx, cl, FALSE);
            EXPECT_EQ(df_px(c, t, 3, 3), 0x22222222u);
            ID3D11CommandList_Release(cl);
        }
    }
    SAFE_RELEASE(t); SAFE_RELEASE(dc);
}
DEVTEST(d3d11_textures_deferred_map_discard_on_dynamic) {
    ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL; ID3D11Texture2D *dyn = NULL, *st = NULL; D3D11_MAPPED_SUBRESOURCE m; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(c->dev, 0, &dc), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, NULL, &dyn), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_STG, 0, CPU_R, 0, NULL, &st), S_OK);
    if (dc && dyn && st) {
        memset(&m, 0, sizeof m);
        hr = ID3D11DeviceContext_Map(dc, (ID3D11Resource *)dyn, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
        EXPECT_HR(hr, S_OK);
        /* a deferred Map cannot know the final layout: RowPitch and DepthPitch are 0 and the memory is tightly packed rows */
        EXPECT_EQ(m.RowPitch, 0); EXPECT_EQ(m.DepthPitch, 0); CHECK(m.pData != NULL);
        if (SUCCEEDED(hr) && m.pData) {
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) ((UINT32 *)m.pData)[y * 4 + x] = 0xA0000000u + y * 16 + x;
            ID3D11DeviceContext_Unmap(dc, (ID3D11Resource *)dyn, 0);
        }
        ID3D11DeviceContext_CopyResource(dc, (ID3D11Resource *)st, (ID3D11Resource *)dyn);
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
        if (cl) {
            ID3D11DeviceContext_ExecuteCommandList(c->ctx, cl, FALSE);
            EXPECT_EQ(df_px(c, st, 0, 0), 0xA0000000u); EXPECT_EQ(df_px(c, st, 3, 3), 0xA0000000u + 3 * 16 + 3);
            ID3D11CommandList_Release(cl);
        }
    }
    SAFE_RELEASE(dyn); SAFE_RELEASE(st); SAFE_RELEASE(dc);
}
DEVTEST(d3d11_textures_deferred_map_rules) {
    ID3D11DeviceContext *dc = NULL; ID3D11Texture2D *st = NULL, *dyn = NULL; D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(c->dev, 0, &dc), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &st), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, NULL, &dyn), S_OK);
    if (dc && st && dyn) {
        /* a deferred context can only record discard-style maps of dynamic resources */
        EXPECT_HR(ID3D11DeviceContext_Map(dc, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m), E_INVALIDARG);
        EXPECT_HR(ID3D11DeviceContext_Map(dc, (ID3D11Resource *)st, 0, D3D11_MAP_WRITE, 0, &m), E_INVALIDARG);
        EXPECT_HR(ID3D11DeviceContext_Map(dc, (ID3D11Resource *)st, 0, D3D11_MAP_READ_WRITE, 0, &m), E_INVALIDARG);
        EXPECT_HR(ID3D11DeviceContext_Map(dc, (ID3D11Resource *)dyn, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &m), D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD);   /* needs an earlier DISCARD in the list */
        EXPECT_HR(ID3D11DeviceContext_Map(dc, (ID3D11Resource *)dyn, 0, D3D11_MAP_READ, 0, &m), E_INVALIDARG);
        EXPECT_HR(ID3D11DeviceContext_Map(dc, (ID3D11Resource *)dyn, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), S_OK);
        ID3D11DeviceContext_Unmap(dc, (ID3D11Resource *)dyn, 0);
    }
    SAFE_RELEASE(st); SAFE_RELEASE(dyn); SAFE_RELEASE(dc);
}
DEVTEST(d3d11_textures_deferred_clear_and_copy_region) {
    ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL; ID3D11Texture2D *rt = NULL, *dst = NULL; ID3D11RenderTargetView *v = NULL; static const float red[4] = { 1, 0, 0, 1 }; D3D11_BOX b = { 0, 0, 0, 2, 2, 1 };
    EXPECT_HR(ID3D11Device_CreateDeferredContext(c->dev, 0, &dc), S_OK);
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, NULL, &rt), S_OK);
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &dst), S_OK);
    if (rt) EXPECT_HR(ID3D11Device_CreateRenderTargetView(c->dev, (ID3D11Resource *)rt, NULL, &v), S_OK);
    if (dc && rt && dst && v) {
        df_fill(c, c->ctx, dst, 8, 8, 0x01010101);
        ID3D11DeviceContext_ClearRenderTargetView(dc, v, red);
        ID3D11DeviceContext_CopySubresourceRegion(dc, (ID3D11Resource *)dst, 0, 4, 4, 0, (ID3D11Resource *)rt, 0, &b);
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
        if (cl) {
            ID3D11DeviceContext_ExecuteCommandList(c->ctx, cl, FALSE);
            EXPECT_EQ(df_px(c, rt, 7, 7), 0xff0000ffu);
            EXPECT_EQ(df_px(c, dst, 4, 4), 0xff0000ffu); EXPECT_EQ(df_px(c, dst, 5, 5), 0xff0000ffu);
            EXPECT_EQ(df_px(c, dst, 6, 6), 0x01010101u); EXPECT_EQ(df_px(c, dst, 3, 3), 0x01010101u);
            ID3D11CommandList_Release(cl);
        }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(rt); SAFE_RELEASE(dst); SAFE_RELEASE(dc);
}
/* GenerateMips recorded in a deferred context. */
DEVTEST(d3d11_textures_deferred_generate_mips) {
    ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL; ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *v = NULL; D3D11_RENDER_TARGET_VIEW_DESC rd; static const float grn[4] = { 0, 1, 0, 1 }; tx_img im;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(c->dev, 0, &dc), S_OK);
    EXPECT_HR(tx_mk2d(c, 16, 16, 0, 1, F_RGBA8, U_DEF, B_RT | B_SRV, 0, M_GEN, NULL, &t), S_OK);
    if (t) {
        memset(&rd, 0, sizeof rd); rd.Format = F_RGBA8; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        EXPECT_HR(ID3D11Device_CreateRenderTargetView(c->dev, (ID3D11Resource *)t, &rd, &v), S_OK);
        EXPECT_HR(ID3D11Device_CreateShaderResourceView(c->dev, (ID3D11Resource *)t, NULL, &srv), S_OK);
    }
    if (dc && v && srv) {
        ID3D11DeviceContext_ClearRenderTargetView(dc, v, grn);
        ID3D11DeviceContext_GenerateMips(dc, srv);
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
        if (cl) {
            ID3D11DeviceContext_ExecuteCommandList(c->ctx, cl, FALSE);
            for (UINT m = 0; m < 5; m++) if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t, m, &im))) { EXPECT_EQ(tx_px32(&im, 0, 0, 0), 0xff00ff00u); tx_img_free(&im); }
            ID3D11CommandList_Release(cl);
        }
    }
    SAFE_RELEASE(srv); SAFE_RELEASE(v); SAFE_RELEASE(t); SAFE_RELEASE(dc);
}
