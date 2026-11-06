/* ID3D11DeviceContext1: constant buffer offsets, ClearView, Discard*, *Subresource1, context state objects. */
#include "csh.h"

#define OPEN1() csf f; ID3D11DeviceContext1 *c1 = NULL; \
    if (!csf_open(&f)) return; \
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11DeviceContext1, (void **)&c1)) || !c1) { t_skip("no ID3D11DeviceContext1"); CLOSE(); return; }
#define CLOSE1() do { ID3D11DeviceContext_ClearState(f.ctx); ID3D11DeviceContext1_Release(c1); CLOSE(); } while (0)

static const struct { UINT first, num; } CB1[] = { { 0, 16 }, { 16, 16 }, { 32, 64 }, { 240, 16 }, { 0, 256 }, { 128, 128 } };

/* Win10: GetConstantBuffers1 with a NULL ppConstantBuffers is not validated and faults (access violation). */
typedef struct { ID3D11DeviceContext1 *c1; int stage; } getnull_ctx;
static void getcb1_null(void *p)
{
    getnull_ctx *g = p; UINT first = 0, num = 0;
    switch (g->stage) {
    case 0: ID3D11DeviceContext1_VSGetConstantBuffers1(g->c1, 1, 1, NULL, &first, &num); break;
    case 1: ID3D11DeviceContext1_HSGetConstantBuffers1(g->c1, 1, 1, NULL, &first, &num); break;
    case 2: ID3D11DeviceContext1_DSGetConstantBuffers1(g->c1, 1, 1, NULL, &first, &num); break;
    case 3: ID3D11DeviceContext1_GSGetConstantBuffers1(g->c1, 1, 1, NULL, &first, &num); break;
    case 4: ID3D11DeviceContext1_PSGetConstantBuffers1(g->c1, 1, 1, NULL, &first, &num); break;
    case 5: ID3D11DeviceContext1_CSGetConstantBuffers1(g->c1, 1, 1, NULL, &first, &num); break;
    }
}
#define STAGE_INDEX_VS 0
#define STAGE_INDEX_HS 1
#define STAGE_INDEX_DS 2
#define STAGE_INDEX_GS 3
#define STAGE_INDEX_PS 4
#define STAGE_INDEX_CS 5
#define CB1_TESTS(XS) \
TEST(d3d11_context_state_##XS##_cb1_offsets) { \
    OPEN1(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 4096); \
    if (!b) { t_skip("buf"); CLOSE1(); return; } \
    for (unsigned i = 0; i < sizeof CB1 / sizeof CB1[0]; i++) { \
        UINT first = CB1[i].first, num = CB1[i].num, gf = 0xff, gn = 0xff; ID3D11Buffer *g = NULL; \
        ID3D11DeviceContext1_##XS##SetConstantBuffers1(c1, 3, 1, &b, &first, &num); \
        ID3D11DeviceContext1_##XS##GetConstantBuffers1(c1, 3, 1, &g, &gf, &gn); \
        CHECKF(g == b, "case %u buffer", i); CHECKF(gf == CB1[i].first, "case %u first %u", i, gf); CHECKF(gn == CB1[i].num, "case %u num %u", i, gn); \
        SAFE_RELEASE(g); \
        /* the plain getter sees the same buffer */ \
        ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 3, 1, &g); \
        CHECKF(g == b, "case %u plain getter", i); SAFE_RELEASE(g); \
    } \
    SAFE_RELEASE(b); \
    CLOSE1(); \
} \
TEST(d3d11_context_state_##XS##_cb1_plain_set_defaults) { \
    OPEN1(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 1024); \
    if (!b) { t_skip("buf"); CLOSE1(); return; } \
    UINT first = 0x55, num = 0x55; ID3D11Buffer *g = NULL; \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 1, 1, &b); \
    ID3D11DeviceContext1_##XS##GetConstantBuffers1(c1, 1, 1, &g, &first, &num); \
    EXPECT_EQ(g, b); EXPECT_EQ(first, 0); \
    /* Win10: a plain bind reports NumConstants = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT (4096), not the buffer size */ \
    EXPECT_EQ(num, 4096); \
    SAFE_RELEASE(g); \
    /* offsets are optional on Get */ \
    ID3D11DeviceContext1_##XS##GetConstantBuffers1(c1, 1, 1, &g, NULL, NULL); EXPECT_EQ(g, b); SAFE_RELEASE(g); \
    { getnull_ctx gc = { c1, STAGE_INDEX_##XS }; EXPECT_EQ(t_try(getcb1_null, &gc), 0xc0000005); } \
    SAFE_RELEASE(b); \
    CLOSE1(); \
} \
TEST(d3d11_context_state_##XS##_cb1_null_ranges) { \
    OPEN1(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 1024); \
    if (!b) { t_skip("buf"); CLOSE1(); return; } \
    UINT first = 0x55, num = 0x55; ID3D11Buffer *g = NULL; \
    ID3D11DeviceContext1_##XS##SetConstantBuffers1(c1, 0, 1, &b, NULL, NULL); \
    ID3D11DeviceContext1_##XS##GetConstantBuffers1(c1, 0, 1, &g, &first, &num); \
    EXPECT_EQ(g, b); EXPECT_EQ(first, 0); \
    EXPECT_EQ(num, 4096); /* NULL ranges behave like a plain bind */ \
    SAFE_RELEASE(g); \
    SAFE_RELEASE(b); \
    CLOSE1(); \
} \
TEST(d3d11_context_state_##XS##_cb1_misaligned) { \
    OPEN1(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 4096); \
    if (!b) { t_skip("buf"); CLOSE1(); return; } \
    UINT first = 1, num = 16, gf = 0x55, gn = 0x55; ID3D11Buffer *g = NULL; \
    ID3D11DeviceContext1_##XS##SetConstantBuffers1(c1, 2, 1, &b, &first, &num); \
    ID3D11DeviceContext1_##XS##GetConstantBuffers1(c1, 2, 1, &g, &gf, &gn); \
    /* Win10: a FirstConstant that is not a multiple of 16 is rejected; the slot stays empty (defaults reported) */ \
    CHECKF(g == NULL, #XS " misaligned FirstConstant=1 was bound"); EXPECT_EQ(gf, 0); EXPECT_EQ(gn, 4096); \
    SAFE_RELEASE(g); \
    first = 240; num = 32; /* past the end of the buffer */ \
    ID3D11DeviceContext1_##XS##SetConstantBuffers1(c1, 4, 1, &b, &first, &num); \
    g = NULL; ID3D11DeviceContext1_##XS##GetConstantBuffers1(c1, 4, 1, &g, &gf, &gn); \
    /* Win10 (no debug layer): a range running past the end of the buffer is stored as given */ \
    EXPECT_EQ(g, b); EXPECT_EQ(gf, 240); EXPECT_EQ(gn, 32); \
    SAFE_RELEASE(g); \
    SAFE_RELEASE(b); \
    CLOSE1(); \
}
CB1_TESTS(VS)
CB1_TESTS(HS)
CB1_TESTS(DS)
CB1_TESTS(GS)
CB1_TESTS(PS)
CB1_TESTS(CS)

TEST(d3d11_context_state_cb1_multiple_slots_distinct_ranges) {
    OPEN1();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 4096);
    if (!b) { t_skip("buf"); CLOSE1(); return; }
    ID3D11Buffer *set[3] = { b, b, b }, *g[3] = { 0 };
    UINT first[3] = { 0, 16, 32 }, num[3] = { 16, 32, 64 }, gf[3] = { 0 }, gn[3] = { 0 };
    ID3D11DeviceContext1_PSSetConstantBuffers1(c1, 0, 3, set, first, num);
    ID3D11DeviceContext1_PSGetConstantBuffers1(c1, 0, 3, g, gf, gn);
    for (int i = 0; i < 3; i++) { CHECKF(g[i] == b, "slot %d", i); CHECKF(gf[i] == first[i] && gn[i] == num[i], "slot %d range %u/%u", i, gf[i], gn[i]); SAFE_RELEASE(g[i]); }
    SAFE_RELEASE(b);
    CLOSE1();
}
TEST(d3d11_context_state_cb1_rebind_resets_range) {
    OPEN1();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 4096);
    if (!b) { t_skip("buf"); CLOSE1(); return; }
    UINT first = 16, num = 16, gf = 0, gn = 0; ID3D11Buffer *g = NULL;
    ID3D11DeviceContext1_VSSetConstantBuffers1(c1, 0, 1, &b, &first, &num);
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 0, 1, &b);
    ID3D11DeviceContext1_VSGetConstantBuffers1(c1, 0, 1, &g, &gf, &gn);
    EXPECT_EQ(g, b); EXPECT_EQ(gf, 0);
    EXPECT_EQ(gn, 4096); /* a plain SetConstantBuffers drops the earlier sub-range */
    SAFE_RELEASE(g); SAFE_RELEASE(b);
    CLOSE1();
}
TEST(d3d11_context_state_cb1_clearstate_resets_range) {
    OPEN1();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 4096);
    if (!b) { t_skip("buf"); CLOSE1(); return; }
    UINT first = 16, num = 16, gf = 0x55, gn = 0x55; ID3D11Buffer *g = (void *)1;
    ID3D11DeviceContext1_VSSetConstantBuffers1(c1, 0, 1, &b, &first, &num);
    ID3D11DeviceContext_ClearState(f.ctx);
    ID3D11DeviceContext1_VSGetConstantBuffers1(c1, 0, 1, &g, &gf, &gn);
    CHECK(g == NULL);
    EXPECT_EQ(gf, 0); EXPECT_EQ(gn, 4096); /* empty slot reports the defaults */
    SAFE_RELEASE(b);
    CLOSE1();
}
TEST(d3d11_context_state_cb1_deferred_roundtrip) {
    OPEN1();
    ID3D11DeviceContext *d = NULL; ID3D11DeviceContext1 *d1 = NULL;
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 4096);
    if (!b || FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d)) || FAILED(ID3D11DeviceContext_QueryInterface(d, &IID_ID3D11DeviceContext1, (void **)&d1))) { t_skip("setup"); goto out; }
    UINT first = 32, num = 32, gf = 0, gn = 0; ID3D11Buffer *g = NULL;
    ID3D11DeviceContext1_PSSetConstantBuffers1(d1, 5, 1, &b, &first, &num);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, TRUE, &cl), S_OK);
    ID3D11DeviceContext1_PSGetConstantBuffers1(d1, 5, 1, &g, &gf, &gn);
    EXPECT_EQ(g, b); EXPECT_EQ(gf, 32); EXPECT_EQ(gn, 32);
    SAFE_RELEASE(g); SAFE_RELEASE(cl);
out:
    if (d1) ID3D11DeviceContext1_Release(d1);
    if (d) { ID3D11DeviceContext_ClearState(d); ID3D11DeviceContext_Release(d); }
    SAFE_RELEASE(b);
    CLOSE1();
}

/* ---- ClearView ----------------------------------------------------------------- */
static ID3D11Texture2D *rt8(csf *f, ID3D11RenderTargetView **v)
{
    ID3D11Texture2D *t = cs_tex(f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    *v = t ? cs_rtv(f, (ID3D11Resource *)t, NULL) : NULL;
    return t;
}
TEST(d3d11_context_state_clearview_whole) {
    OPEN1();
    ID3D11RenderTargetView *rtv; ID3D11Texture2D *t = rt8(&f, &rtv);
    if (!rtv) { t_skip("rtv"); SAFE_RELEASE(t); CLOSE1(); return; }
    FLOAT green[4] = { 0, 1, 0, 1 }, red[4] = { 1, 0, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, green);
    ID3D11DeviceContext1_ClearView(c1, (ID3D11View *)rtv, red, NULL, 0);
    uint32_t px;
    if (cs_read_px(&f, t, 0, 0, &px)) EXPECT_EQ(px, 0xff0000ffu);
    if (cs_read_px(&f, t, 7, 7, &px)) EXPECT_EQ(px, 0xff0000ffu);
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE1();
}
TEST(d3d11_context_state_clearview_rects) {
    OPEN1();
    ID3D11RenderTargetView *rtv; ID3D11Texture2D *t = rt8(&f, &rtv);
    if (!rtv) { t_skip("rtv"); SAFE_RELEASE(t); CLOSE1(); return; }
    FLOAT green[4] = { 0, 1, 0, 1 }, red[4] = { 1, 0, 0, 1 };
    D3D11_RECT r[2] = { { 2, 2, 6, 6 }, { 0, 7, 8, 8 } };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, green);
    ID3D11DeviceContext1_ClearView(c1, (ID3D11View *)rtv, red, r, 2);
    static const struct { UINT x, y; uint32_t want; } px[] = {
        { 2, 2, 0xff0000ffu }, { 5, 5, 0xff0000ffu }, { 6, 6, 0xff00ff00u }, { 1, 1, 0xff00ff00u }, { 2, 6, 0xff00ff00u },
        { 0, 7, 0xff0000ffu }, { 7, 7, 0xff0000ffu }, { 3, 6, 0xff00ff00u }, { 0, 0, 0xff00ff00u }, { 7, 0, 0xff00ff00u } };
    d3dh_image im;
    if (SUCCEEDED(d3dh_readback_texture2d11(f.dev, f.ctx, t, 0, &im))) {
        for (unsigned i = 0; i < sizeof px / sizeof px[0]; i++) {
            uint32_t v = d3dh_image_pixel32(&im, px[i].x, px[i].y);
            CHECKF(v == px[i].want, "pixel (%u,%u) = 0x%08x want 0x%08x", px[i].x, px[i].y, v, px[i].want);
        }
        d3dh_image_free(&im);
    }
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE1();
}
TEST(d3d11_context_state_clearview_deferred) {
    OPEN1();
    ID3D11RenderTargetView *rtv; ID3D11Texture2D *t = rt8(&f, &rtv);
    ID3D11DeviceContext *d = NULL; ID3D11DeviceContext1 *d1 = NULL;
    if (!rtv || FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d)) || FAILED(ID3D11DeviceContext_QueryInterface(d, &IID_ID3D11DeviceContext1, (void **)&d1))) { t_skip("setup"); goto out; }
    FLOAT green[4] = { 0, 1, 0, 1 }, red[4] = { 1, 0, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, green);
    ID3D11DeviceContext1_ClearView(d1, (ID3D11View *)rtv, red, NULL, 0);
    uint32_t px;
    if (cs_read_px(&f, t, 3, 3, &px)) EXPECT_EQ(px, 0xff00ff00u);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    if (cs_read_px(&f, t, 3, 3, &px)) EXPECT_EQ(px, 0xff0000ffu);
    SAFE_RELEASE(cl);
out:
    if (d1) ID3D11DeviceContext1_Release(d1);
    SAFE_RELEASE(d); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE1();
}
TEST(d3d11_context_state_clearview_does_not_use_bound_scissor) {
    OPEN1();
    ID3D11RenderTargetView *rtv; ID3D11Texture2D *t = rt8(&f, &rtv);
    ID3D11RasterizerState *rs = cs_rs(&f, D3D11_CULL_NONE, TRUE);
    if (!rtv || !rs) { t_skip("setup"); goto out; }
    D3D11_RECT sc = { 0, 0, 1, 1 };
    FLOAT green[4] = { 0, 1, 0, 1 }, red[4] = { 1, 0, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, green);
    ID3D11DeviceContext_RSSetState(f.ctx, rs);
    ID3D11DeviceContext_RSSetScissorRects(f.ctx, 1, &sc);
    ID3D11DeviceContext1_ClearView(c1, (ID3D11View *)rtv, red, NULL, 0);
    uint32_t px;
    if (cs_read_px(&f, t, 5, 5, &px)) EXPECT_EQ(px, 0xff0000ffu);
out:
    SAFE_RELEASE(rs); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE1();
}

/* ---- Discard -------------------------------------------------------------------- */
TEST(d3d11_context_state_discard_view_then_clear) {
    OPEN1();
    ID3D11RenderTargetView *rtv; ID3D11Texture2D *t = rt8(&f, &rtv);
    if (!rtv) { t_skip("rtv"); SAFE_RELEASE(t); CLOSE1(); return; }
    FLOAT blue[4] = { 0, 0, 1, 1 };
    ID3D11DeviceContext1_DiscardView(c1, (ID3D11View *)rtv);
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, blue);
    uint32_t px;
    if (cs_read_px(&f, t, 2, 2, &px)) EXPECT_EQ(px, 0xffff0000u);
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE1();
}
TEST(d3d11_context_state_discard_resource_then_clear) {
    OPEN1();
    ID3D11RenderTargetView *rtv; ID3D11Texture2D *t = rt8(&f, &rtv);
    if (!rtv) { t_skip("rtv"); SAFE_RELEASE(t); CLOSE1(); return; }
    FLOAT blue[4] = { 0, 0, 1, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, blue);
    ID3D11DeviceContext1_DiscardResource(c1, (ID3D11Resource *)t);
    FLOAT red[4] = { 1, 0, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, red);
    uint32_t px;
    if (cs_read_px(&f, t, 2, 2, &px)) EXPECT_EQ(px, 0xff0000ffu);
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE1();
}
TEST(d3d11_context_state_discard_view1_rects) {
    OPEN1();
    ID3D11RenderTargetView *rtv; ID3D11Texture2D *t = rt8(&f, &rtv);
    if (!rtv) { t_skip("rtv"); SAFE_RELEASE(t); CLOSE1(); return; }
    D3D11_RECT r = { 0, 0, 4, 4 };
    ID3D11DeviceContext1_DiscardView1(c1, (ID3D11View *)rtv, &r, 1);
    ID3D11DeviceContext1_DiscardView1(c1, (ID3D11View *)rtv, NULL, 0);
    FLOAT red[4] = { 1, 0, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, red);
    uint32_t px;
    if (cs_read_px(&f, t, 6, 6, &px)) EXPECT_EQ(px, 0xff0000ffu);
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE1();
}
TEST(d3d11_context_state_discard_deferred_recorded) {
    OPEN1();
    ID3D11RenderTargetView *rtv; ID3D11Texture2D *t = rt8(&f, &rtv);
    ID3D11DeviceContext *d = NULL; ID3D11DeviceContext1 *d1 = NULL;
    if (!rtv || FAILED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d)) || FAILED(ID3D11DeviceContext_QueryInterface(d, &IID_ID3D11DeviceContext1, (void **)&d1))) { t_skip("setup"); goto out; }
    ID3D11DeviceContext1_DiscardView(d1, (ID3D11View *)rtv);
    ID3D11DeviceContext1_DiscardResource(d1, (ID3D11Resource *)t);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    SAFE_RELEASE(cl);
out:
    if (d1) ID3D11DeviceContext1_Release(d1);
    SAFE_RELEASE(d); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE1();
}

/* ---- *Subresource1 ---------------------------------------------------------------- */
TEST(d3d11_context_state_copy_subresource_region1_buffer) {
    OPEN1();
    UINT src[64], dst0[64];
    for (int i = 0; i < 64; i++) { src[i] = 0x100 + i; dst0[i] = 0xdd000000u + i; }
    ID3D11Buffer *s = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 256, src), *d = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 256, dst0);
    if (!s || !d) { t_skip("bufs"); goto out; }
    D3D11_BOX box = { 32, 0, 0, 96, 1, 1 }; /* bytes 32..96 = elements 8..23 */
    ID3D11DeviceContext1_CopySubresourceRegion1(c1, (ID3D11Resource *)d, 0, 128, 0, 0, (ID3D11Resource *)s, 0, &box, 0);
    void *p = NULL; size_t n = 0;
    EXPECT_HR_OK(d3dh_readback_buffer11(f.dev, f.ctx, d, &p, &n));
    if (p) {
        const UINT *u = p;
        for (int i = 0; i < 64; i++) {
            UINT want = (i >= 32 && i < 48) ? src[8 + (i - 32)] : dst0[i];
            CHECKF(u[i] == want, "element %d = 0x%x want 0x%x", i, u[i], want);
        }
        free(p);
    }
out:
    SAFE_RELEASE(s); SAFE_RELEASE(d);
    CLOSE1();
}
TEST(d3d11_context_state_copy_subresource_region1_full_no_box) {
    OPEN1();
    UINT src[16], z[16] = { 0 };
    for (int i = 0; i < 16; i++) src[i] = 0x77 + i;
    ID3D11Buffer *s = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 64, src), *d = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 64, z);
    if (!s || !d) { t_skip("bufs"); goto out; }
    ID3D11DeviceContext1_CopySubresourceRegion1(c1, (ID3D11Resource *)d, 0, 0, 0, 0, (ID3D11Resource *)s, 0, NULL, 0);
    void *p = NULL; size_t n = 0;
    EXPECT_HR_OK(d3dh_readback_buffer11(f.dev, f.ctx, d, &p, &n));
    if (p) { CHECK(n == 64 && !memcmp(p, src, 64)); free(p); }
out:
    SAFE_RELEASE(s); SAFE_RELEASE(d);
    CLOSE1();
}
TEST(d3d11_context_state_copy_subresource_region1_flags) {
    OPEN1();
    UINT src[16], z[16] = { 0 };
    for (int i = 0; i < 16; i++) src[i] = 0x55 + i;
    ID3D11Buffer *s = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 64, src), *d = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 64, z);
    if (!s || !d) { t_skip("bufs"); goto out; }
    static const UINT fl[] = { D3D11_COPY_DISCARD, D3D11_COPY_NO_OVERWRITE };
    for (int k = 0; k < 2; k++) {
        ID3D11DeviceContext_UpdateSubresource(f.ctx, (ID3D11Resource *)d, 0, NULL, z, 0, 0);
        ID3D11DeviceContext1_CopySubresourceRegion1(c1, (ID3D11Resource *)d, 0, 0, 0, 0, (ID3D11Resource *)s, 0, NULL, fl[k]);
        void *p = NULL; size_t n = 0;
        if (SUCCEEDED(d3dh_readback_buffer11(f.dev, f.ctx, d, &p, &n))) {
            CHECKF(!memcmp(p, src, 64), "CopySubresourceRegion1 flags=%u on a DEFAULT buffer must copy", fl[k]); /* Win10: copied for both */
            free(p);
        }
    }
out:
    SAFE_RELEASE(s); SAFE_RELEASE(d);
    CLOSE1();
}
TEST(d3d11_context_state_copy_subresource_region1_texture) {
    OPEN1();
    ID3D11Texture2D *s = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *d = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11RenderTargetView *sv = s ? cs_rtv(&f, (ID3D11Resource *)s, NULL) : NULL, *dv = d ? cs_rtv(&f, (ID3D11Resource *)d, NULL) : NULL;
    if (!sv || !dv) { t_skip("setup"); goto out; }
    FLOAT red[4] = { 1, 0, 0, 1 }, blue[4] = { 0, 0, 1, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, sv, red);
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, dv, blue);
    D3D11_BOX box = { 0, 0, 0, 4, 4, 1 };
    ID3D11DeviceContext1_CopySubresourceRegion1(c1, (ID3D11Resource *)d, 0, 4, 4, 0, (ID3D11Resource *)s, 0, &box, 0);
    d3dh_image im;
    if (SUCCEEDED(d3dh_readback_texture2d11(f.dev, f.ctx, d, 0, &im))) {
        EXPECT_EQ(d3dh_image_pixel32(&im, 4, 4), 0xff0000ffu);
        EXPECT_EQ(d3dh_image_pixel32(&im, 7, 7), 0xff0000ffu);
        EXPECT_EQ(d3dh_image_pixel32(&im, 3, 3), 0xffff0000u);
        EXPECT_EQ(d3dh_image_pixel32(&im, 0, 7), 0xffff0000u);
        d3dh_image_free(&im);
    }
out:
    SAFE_RELEASE(sv); SAFE_RELEASE(dv); SAFE_RELEASE(s); SAFE_RELEASE(d);
    CLOSE1();
}
TEST(d3d11_context_state_update_subresource1_box) {
    OPEN1();
    UINT z[64] = { 0 }, data[16];
    for (int i = 0; i < 16; i++) data[i] = 0xBEE0 + i;
    ID3D11Buffer *b = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 256, z);
    if (!b) { t_skip("buf"); CLOSE1(); return; }
    D3D11_BOX box = { 64, 0, 0, 128, 1, 1 };
    ID3D11DeviceContext1_UpdateSubresource1(c1, (ID3D11Resource *)b, 0, &box, data, 0, 0, 0);
    void *p = NULL; size_t n = 0;
    EXPECT_HR_OK(d3dh_readback_buffer11(f.dev, f.ctx, b, &p, &n));
    if (p) {
        const UINT *u = p;
        for (int i = 0; i < 64; i++) { UINT want = (i >= 16 && i < 32) ? data[i - 16] : 0; CHECKF(u[i] == want, "element %d = 0x%x want 0x%x", i, u[i], want); }
        free(p);
    }
    SAFE_RELEASE(b);
    CLOSE1();
}
TEST(d3d11_context_state_update_subresource1_texture_pitch) {
    OPEN1();
    ID3D11Texture2D *t = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    if (!t) { t_skip("tex"); CLOSE1(); return; }
    uint32_t px[16 * 4]; /* 4 rows, source pitch 64 bytes (wider than the 2-pixel box) */
    for (int y = 0; y < 4; y++) for (int x = 0; x < 16; x++) px[y * 16 + x] = 0xff000000u | (y << 8) | x;
    D3D11_BOX box = { 2, 3, 0, 4, 7, 1 }; /* 2 wide, 4 tall */
    ID3D11DeviceContext1_UpdateSubresource1(c1, (ID3D11Resource *)t, 0, &box, px, 64, 0, 0);
    d3dh_image im;
    if (SUCCEEDED(d3dh_readback_texture2d11(f.dev, f.ctx, t, 0, &im))) {
        for (int y = 0; y < 4; y++) for (int x = 0; x < 2; x++)
            CHECKF(d3dh_image_pixel32(&im, 2 + x, 3 + y) == px[y * 16 + x], "pixel (%d,%d) = 0x%08x", 2 + x, 3 + y, d3dh_image_pixel32(&im, 2 + x, 3 + y));
        d3dh_image_free(&im);
    }
    SAFE_RELEASE(t);
    CLOSE1();
}
TEST(d3d11_context_state_update_subresource1_discard) {
    OPEN1();
    UINT data[16];
    for (int i = 0; i < 16; i++) data[i] = i + 1;
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_SHADER_RESOURCE, 64);
    if (!b) { t_skip("buf"); CLOSE1(); return; }
    ID3D11DeviceContext1_UpdateSubresource1(c1, (ID3D11Resource *)b, 0, NULL, data, 0, 0, D3D11_COPY_DISCARD);
    void *p = NULL; size_t n = 0;
    if (SUCCEEDED(d3dh_readback_buffer11(f.dev, f.ctx, b, &p, &n))) { CHECK(!memcmp(p, data, 64)); /* Win10: stored */ free(p); }
    SAFE_RELEASE(b);
    CLOSE1();
}

/* ---- device context state objects ---------------------------------------------------- */
#define OPEN_STATE() csf f; ID3D11DeviceContext1 *c1 = NULL; ID3D11Device1 *d1 = NULL; \
    if (!csf_open(&f)) return; \
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11DeviceContext1, (void **)&c1)) || \
        FAILED(ID3D11Device_QueryInterface(f.dev, &IID_ID3D11Device1, (void **)&d1))) { \
        t_skip("no D3D11.1 interfaces"); if (c1) ID3D11DeviceContext1_Release(c1); CLOSE(); return; }
#define CLOSE_STATE() do { ID3D11DeviceContext_ClearState(f.ctx); ID3D11DeviceContext1_Release(c1); ID3D11Device1_Release(d1); CLOSE(); } while (0)

static ID3DDeviceContextState *mk_state(ID3D11Device1 *d1, csf *f, const IID *emu, D3D_FEATURE_LEVEL *chosen)
{
    static const D3D_FEATURE_LEVEL all[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    ID3DDeviceContextState *s = NULL;
    D3D_FEATURE_LEVEL ch = 0;
    UINT skip = 0;
    while (skip < 4 && all[skip] > f->fl) skip++;
    HRESULT hr = ID3D11Device1_CreateDeviceContextState(d1, 0, all + skip, 4 - skip, D3D11_SDK_VERSION, emu, &ch, &s);
    if (FAILED(hr)) { t_note("CreateDeviceContextState: %s", d3dh_hr_name(hr)); return NULL; }
    if (chosen) *chosen = ch;
    return s;
}
TEST(d3d11_context_state_state_object_create) {
    OPEN_STATE();
    D3D_FEATURE_LEVEL ch = 0;
    ID3DDeviceContextState *s = mk_state(d1, &f, &IID_ID3D11Device1, &ch);
    if (!s) { t_skip("no state"); CLOSE_STATE(); return; }
    EXPECT_EQ(ch, f.fl);
    ID3D11Device *dv = NULL;
    ID3DDeviceContextState_GetDevice(s, &dv);
    EXPECT_EQ(dv, f.dev); SAFE_RELEASE(dv);
    ID3D11DeviceChild *c = NULL;
    EXPECT_HR(ID3DDeviceContextState_QueryInterface(s, &IID_ID3D11DeviceChild, (void **)&c), S_OK);
    SAFE_RELEASE(c);
    EXPECT_EQ(ID3DDeviceContextState_Release(s), 0);
    CLOSE_STATE();
}
TEST(d3d11_context_state_state_object_chosen_level_lowest) {
    OPEN_STATE();
    static const D3D_FEATURE_LEVEL l10[] = { D3D_FEATURE_LEVEL_10_0 };
    ID3DDeviceContextState *s = NULL;
    D3D_FEATURE_LEVEL ch = 0;
    HRESULT hr = ID3D11Device1_CreateDeviceContextState(d1, 0, l10, 1, D3D11_SDK_VERSION, &IID_ID3D11Device, &ch, &s);
    if (FAILED(hr)) { t_note("CreateDeviceContextState(10_0): %s", d3dh_hr_name(hr)); t_skip("not supported"); CLOSE_STATE(); return; }
    EXPECT_EQ(ch, D3D_FEATURE_LEVEL_10_0);
    SAFE_RELEASE(s);
    CLOSE_STATE();
}
TEST(d3d11_context_state_state_object_create_args) {
    OPEN_STATE();
    static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_11_0 };
    ID3DDeviceContextState *s = (void *)1; D3D_FEATURE_LEVEL ch = 0;
    HRESULT hr = ID3D11Device1_CreateDeviceContextState(d1, 0, l, 1, D3D11_SDK_VERSION, &IID_IUnknown, &ch, &s);
    EXPECT_HR(hr, E_INVALIDARG);
    if (SUCCEEDED(hr)) SAFE_RELEASE(s);
    s = (void *)1;
    hr = ID3D11Device1_CreateDeviceContextState(d1, 0x80, l, 1, D3D11_SDK_VERSION, &IID_ID3D11Device1, &ch, &s);
    EXPECT_HR(hr, E_INVALIDARG);
    if (SUCCEEDED(hr)) SAFE_RELEASE(s);
    s = (void *)1;
    hr = ID3D11Device1_CreateDeviceContextState(d1, 0, l, 0, D3D11_SDK_VERSION, &IID_ID3D11Device1, &ch, &s);
    EXPECT_HR(hr, E_INVALIDARG);
    if (SUCCEEDED(hr)) SAFE_RELEASE(s);
    hr = ID3D11Device1_CreateDeviceContextState(d1, 0, l, 1, D3D11_SDK_VERSION, &IID_ID3D11Device1, &ch, NULL);
    EXPECT_HR(hr, S_FALSE); /* validate-only: success without creating an object */
    CLOSE_STATE();
}
TEST(d3d11_context_state_state_swap_isolation) {
    OPEN_STATE();
    ID3DDeviceContextState *s = mk_state(d1, &f, &IID_ID3D11Device1, NULL), *prev = NULL, *prev2 = NULL;
    ID3D11Buffer *b1 = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64), *b2 = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!s || !b1 || !b2) { t_skip("setup"); goto out; }
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 0, 1, &b1);
    ID3D11DeviceContext_IASetPrimitiveTopology(f.ctx, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, s, &prev);
    CHECK(prev != NULL);
    ID3D11Buffer *g = (void *)1; D3D11_PRIMITIVE_TOPOLOGY t = 0x55;
    ID3D11DeviceContext_VSGetConstantBuffers(f.ctx, 0, 1, &g);
    CHECKF(g == NULL, "a freshly created context state must be empty");
    SAFE_RELEASE(g);
    ID3D11DeviceContext_IAGetPrimitiveTopology(f.ctx, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 0, 1, &b2);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, prev, &prev2);
    EXPECT_EQ(prev2, s);
    ID3D11DeviceContext_VSGetConstantBuffers(f.ctx, 0, 1, &g);
    EXPECT_EQ(g, b1); SAFE_RELEASE(g);
    ID3D11DeviceContext_IAGetPrimitiveTopology(f.ctx, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    /* swap back to the other state without asking for the previous one */
    ID3D11DeviceContext1_SwapDeviceContextState(c1, s, NULL);
    ID3D11DeviceContext_VSGetConstantBuffers(f.ctx, 0, 1, &g);
    EXPECT_EQ(g, b2); SAFE_RELEASE(g);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, prev, NULL);
out:
    SAFE_RELEASE(prev); SAFE_RELEASE(prev2); SAFE_RELEASE(s); SAFE_RELEASE(b1); SAFE_RELEASE(b2);
    CLOSE_STATE();
}
TEST(d3d11_context_state_state_swap_returns_referenced_previous) {
    OPEN_STATE();
    ID3DDeviceContextState *s = mk_state(d1, &f, &IID_ID3D11Device1, NULL), *prev = NULL;
    if (!s) { t_skip("no state"); CLOSE_STATE(); return; }
    ID3D11DeviceContext1_SwapDeviceContextState(c1, s, &prev);
    if (prev) {
        ULONG rc = RC(prev);
        CHECK(rc >= 1);
        ID3D11DeviceContext1_SwapDeviceContextState(c1, prev, NULL);
        ID3DDeviceContextState_Release(prev);
    }
    SAFE_RELEASE(s);
    CLOSE_STATE();
}
TEST(d3d11_context_state_state_swap_same_state_twice) {
    OPEN_STATE();
    ID3DDeviceContextState *s = mk_state(d1, &f, &IID_ID3D11Device1, NULL), *p1 = NULL, *p2 = NULL, *orig = NULL;
    if (!s) { t_skip("no state"); CLOSE_STATE(); return; }
    ID3D11DeviceContext1_SwapDeviceContextState(c1, s, &orig);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, s, &p1);
    EXPECT_EQ(p1, s);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, orig, &p2);
    EXPECT_EQ(p2, s);
    SAFE_RELEASE(orig); SAFE_RELEASE(p1); SAFE_RELEASE(p2); SAFE_RELEASE(s);
    CLOSE_STATE();
}
TEST(d3d11_context_state_state_swap_emulated_feature_level) {
    OPEN_STATE();
    static const D3D_FEATURE_LEVEL l10[] = { D3D_FEATURE_LEVEL_10_0 };
    ID3DDeviceContextState *s = NULL, *prev = NULL;
    D3D_FEATURE_LEVEL ch = 0;
    if (FAILED(ID3D11Device1_CreateDeviceContextState(d1, 0, l10, 1, D3D11_SDK_VERSION, &IID_ID3D11Device, &ch, &s))) { t_skip("10_0 state"); CLOSE_STATE(); return; }
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(f.dev), f.fl);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, s, &prev);
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(f.dev), D3D_FEATURE_LEVEL_10_0); /* the device reports the emulated level while the state is active */
    ID3D11Device1 *q = NULL;
    HRESULT hr = ID3D11Device_QueryInterface(f.dev, &IID_ID3D11Device1, (void **)&q);
    EXPECT_HR(hr, S_OK);
    if (q) ID3D11Device1_Release(q);
    if (prev) { ID3D11DeviceContext1_SwapDeviceContextState(c1, prev, NULL); ID3DDeviceContextState_Release(prev); }
    EXPECT_EQ(ID3D11Device_GetFeatureLevel(f.dev), f.fl);
    SAFE_RELEASE(s);
    CLOSE_STATE();
}
TEST(d3d11_context_state_state_clearstate_in_swapped_state) {
    OPEN_STATE();
    ID3DDeviceContextState *s = mk_state(d1, &f, &IID_ID3D11Device1, NULL), *prev = NULL;
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!s || !b) { t_skip("setup"); goto out; }
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 0, 1, &b);
    ID3D11DeviceContext1_SwapDeviceContextState(c1, s, &prev);
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 0, 1, &b);
    ID3D11DeviceContext_ClearState(f.ctx); /* clears only the active state */
    ID3D11DeviceContext1_SwapDeviceContextState(c1, prev, NULL);
    ID3D11Buffer *g = NULL;
    ID3D11DeviceContext_VSGetConstantBuffers(f.ctx, 0, 1, &g);
    EXPECT_EQ(g, b); SAFE_RELEASE(g);
out:
    SAFE_RELEASE(prev); SAFE_RELEASE(s); SAFE_RELEASE(b);
    CLOSE_STATE();
}
