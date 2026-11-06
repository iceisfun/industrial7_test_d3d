/* Deferred contexts, command lists, ExecuteCommandList restore semantics, deferred Map rules. */
#include "csh.h"

static int mk_deferred(csf *f, ID3D11DeviceContext **d)
{
    HRESULT hr = ID3D11Device_CreateDeferredContext(f->dev, 0, d);
    if (FAILED(hr)) { t_skip("CreateDeferredContext: %s", d3dh_hr_name(hr)); *d = NULL; return 0; }
    return 1;
}
#define OPEN2() csf f; ID3D11DeviceContext *d = NULL; if (!csf_open(&f)) return; if (!mk_deferred(&f, &d)) { CLOSE(); return; }
#define CLOSE2() do { if (d) { ID3D11DeviceContext_ClearState(d); ID3D11DeviceContext_Release(d); } CLOSE(); } while (0)

static void draw_tri(ID3D11DeviceContext *c, ID3D11RenderTargetView *rtv, ID3D11VertexShader *vs, ID3D11PixelShader *ps, UINT size)
{
    D3D11_VIEWPORT vp = { 0, 0, (FLOAT)size, (FLOAT)size, 0, 1 };
    ID3D11DeviceContext_OMSetRenderTargets(c, 1, &rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(c, 1, &vp);
    ID3D11DeviceContext_IASetPrimitiveTopology(c, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(c, vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(c, ps, NULL, 0);
    ID3D11DeviceContext_Draw(c, 3, 0);
}

TEST(d3d11_context_state_deferred_default_state) {
    OPEN2();
    ID3D11Buffer *g[4]; memset(g, 0x55, sizeof g);
    ID3D11DeviceContext_VSGetConstantBuffers(d, 0, 4, g);
    for (int i = 0; i < 4; i++) CHECK(g[i] == NULL);
    FLOAT bf[4] = { 0 }; UINT m = 0; ID3D11BlendState *bs = (void *)1;
    ID3D11DeviceContext_OMGetBlendState(d, &bs, bf, &m);
    CHECK(bs == NULL); EXPECT_EQ(m, 0xffffffffu);
    for (int i = 0; i < 4; i++) CHECK(bf[i] == 1.0f);
    D3D11_PRIMITIVE_TOPOLOGY t = 0x55; ID3D11DeviceContext_IAGetPrimitiveTopology(d, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    UINT nv = 9; ID3D11DeviceContext_RSGetViewports(d, &nv, NULL); EXPECT_EQ(nv, 0);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_state_isolated_from_immediate) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 0, 1, &b);
    ID3D11Buffer *g = (void *)1;
    ID3D11DeviceContext_VSGetConstantBuffers(d, 0, 1, &g);
    CHECK(g == NULL);
    ID3D11DeviceContext_PSSetConstantBuffers(d, 1, 1, &b);
    g = (void *)1;
    ID3D11DeviceContext_PSGetConstantBuffers(f.ctx, 1, 1, &g);
    CHECK(g == NULL);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_setget_roundtrip) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    ID3D11BlendState *bs = cs_bs(&f, TRUE);
    ID3D11DepthStencilState *dss = cs_dss(&f, TRUE);
    ID3D11RasterizerState *rs = cs_rs(&f, D3D11_CULL_NONE, TRUE);
    if (!b || !bs || !dss || !rs) { t_skip("objects"); goto out; }
    ID3D11DeviceContext_GSSetConstantBuffers(d, 3, 1, &b);
    FLOAT fac[4] = { .1f, .2f, .3f, .4f };
    ID3D11DeviceContext_OMSetBlendState(d, bs, fac, 0xabcd);
    ID3D11DeviceContext_OMSetDepthStencilState(d, dss, 9);
    ID3D11DeviceContext_RSSetState(d, rs);
    D3D11_VIEWPORT vp = { 1, 2, 3, 4, 0, 1 }; ID3D11DeviceContext_RSSetViewports(d, 1, &vp);
    ID3D11Buffer *gb = NULL; ID3D11BlendState *gbs = NULL; ID3D11DepthStencilState *gd = NULL; ID3D11RasterizerState *gr = NULL;
    FLOAT gf[4]; UINT gm = 0, gref = 0, nv = 0;
    ID3D11DeviceContext_GSGetConstantBuffers(d, 3, 1, &gb); EXPECT_EQ(gb, b);
    ID3D11DeviceContext_OMGetBlendState(d, &gbs, gf, &gm); EXPECT_EQ(gbs, bs); EXPECT_EQ(gm, 0xabcd); CHECK(gf[2] == fac[2]);
    ID3D11DeviceContext_OMGetDepthStencilState(d, &gd, &gref); EXPECT_EQ(gd, dss); EXPECT_EQ(gref, 9);
    ID3D11DeviceContext_RSGetState(d, &gr); EXPECT_EQ(gr, rs);
    ID3D11DeviceContext_RSGetViewports(d, &nv, NULL); EXPECT_EQ(nv, 1);
    SAFE_RELEASE(gb); SAFE_RELEASE(gbs); SAFE_RELEASE(gd); SAFE_RELEASE(gr);
out:
    SAFE_RELEASE(b); SAFE_RELEASE(bs); SAFE_RELEASE(dss); SAFE_RELEASE(rs);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_clearstate) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    ID3D11DeviceContext_VSSetConstantBuffers(d, 0, 1, &b);
    ID3D11DeviceContext_IASetPrimitiveTopology(d, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D11DeviceContext_ClearState(d);
    ID3D11Buffer *g = (void *)1; D3D11_PRIMITIVE_TOPOLOGY t = 0x55;
    ID3D11DeviceContext_VSGetConstantBuffers(d, 0, 1, &g); CHECK(g == NULL);
    ID3D11DeviceContext_IAGetPrimitiveTopology(d, &t); EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_finish_restore_true_keeps_state) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    ID3D11DeviceContext_VSSetConstantBuffers(d, 2, 1, &b);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, TRUE, &cl), S_OK);
    ID3D11Buffer *g = NULL;
    ID3D11DeviceContext_VSGetConstantBuffers(d, 2, 1, &g);
    EXPECT_EQ(g, b); SAFE_RELEASE(g);
    SAFE_RELEASE(cl);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_finish_restore_false_clears_state) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    ID3D11BlendState *bs = cs_bs(&f, TRUE);
    if (!b || !bs) { t_skip("objs"); goto out; }
    FLOAT fac[4] = { 0, 0, 0, 0 };
    ID3D11DeviceContext_VSSetConstantBuffers(d, 2, 1, &b);
    ID3D11DeviceContext_OMSetBlendState(d, bs, fac, 5);
    ID3D11DeviceContext_IASetPrimitiveTopology(d, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11Buffer *g = (void *)1; ID3D11BlendState *gbs = (void *)1; FLOAT gf[4] = { 0 }; UINT gm = 0; D3D11_PRIMITIVE_TOPOLOGY t = 0x55;
    ID3D11DeviceContext_VSGetConstantBuffers(d, 2, 1, &g); CHECK(g == NULL);
    ID3D11DeviceContext_OMGetBlendState(d, &gbs, gf, &gm); CHECK(gbs == NULL); EXPECT_EQ(gm, 0xffffffffu); CHECK(gf[0] == 1.0f && gf[3] == 1.0f);
    ID3D11DeviceContext_IAGetPrimitiveTopology(d, &t); EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    SAFE_RELEASE(cl);
out:
    SAFE_RELEASE(b); SAFE_RELEASE(bs);
    CLOSE2();
}
TEST(d3d11_context_state_finish_command_list_object) {
    OPEN2();
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    if (!cl) { CLOSE2(); return; }
    EXPECT_EQ(ID3D11CommandList_GetContextFlags(cl), 0);
    ID3D11Device *dv = NULL; ID3D11CommandList_GetDevice(cl, &dv);
    EXPECT_EQ(dv, f.dev); SAFE_RELEASE(dv);
    ID3D11DeviceChild *ch = NULL;
    EXPECT_HR(ID3D11CommandList_QueryInterface(cl, &IID_ID3D11DeviceChild, (void **)&ch), S_OK);
    SAFE_RELEASE(ch);
    void *p = (void *)1;
    EXPECT_HR(ID3D11CommandList_QueryInterface(cl, &IID_ID3D11DeviceContext, &p), E_NOINTERFACE);
    EXPECT_EQ(ID3D11CommandList_Release(cl), 0);
    CLOSE2();
}
TEST(d3d11_context_state_finish_empty_twice) {
    OPEN2();
    ID3D11CommandList *a = NULL, *b = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &a), S_OK);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &b), S_OK);
    if (a && b) EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_finish_on_immediate_fails) {
    OPEN();
    ID3D11CommandList *cl = (void *)1;
    HRESULT hr = ID3D11DeviceContext_FinishCommandList(f.ctx, FALSE, &cl);
    EXPECT_HR(hr, DXGI_ERROR_INVALID_CALL);
    CHECKF(cl == NULL || cl == (void *)1, "no command list may be returned (%p)", cl);
    CLOSE();
}
TEST(d3d11_context_state_execute_restore_false_clears_immediate) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    ID3D11CommandList *cl = NULL;
    if (!b) { t_skip("buf"); goto out; }
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 1, 1, &b);
    ID3D11DeviceContext_IASetPrimitiveTopology(f.ctx, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    ID3D11Buffer *g = (void *)1; D3D11_PRIMITIVE_TOPOLOGY t = 0x55;
    ID3D11DeviceContext_VSGetConstantBuffers(f.ctx, 1, 1, &g);
    CHECKF(g == NULL, "RestoreContextState=FALSE must leave the immediate context in default state");
    ID3D11DeviceContext_IAGetPrimitiveTopology(f.ctx, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
out:
    SAFE_RELEASE(cl); SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_execute_restore_true_keeps_immediate) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    ID3D11CommandList *cl = NULL;
    if (!b) { t_skip("buf"); goto out; }
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 1, 1, &b);
    ID3D11DeviceContext_IASetPrimitiveTopology(f.ctx, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, TRUE);
    ID3D11Buffer *g = NULL; D3D11_PRIMITIVE_TOPOLOGY t = 0x55;
    ID3D11DeviceContext_VSGetConstantBuffers(f.ctx, 1, 1, &g);
    EXPECT_EQ(g, b); SAFE_RELEASE(g);
    ID3D11DeviceContext_IAGetPrimitiveTopology(f.ctx, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(cl); SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_execute_list_state_not_leaked_restore_true) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    ID3D11CommandList *cl = NULL;
    if (!b) { t_skip("buf"); goto out; }
    ID3D11DeviceContext_PSSetConstantBuffers(d, 0, 1, &b);
    ID3D11DeviceContext_IASetPrimitiveTopology(d, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, TRUE);
    ID3D11Buffer *g = (void *)1; D3D11_PRIMITIVE_TOPOLOGY t = 0x55;
    ID3D11DeviceContext_PSGetConstantBuffers(f.ctx, 0, 1, &g);
    CHECKF(g == NULL, "state recorded by a deferred context leaked into the immediate context");
    ID3D11DeviceContext_IAGetPrimitiveTopology(f.ctx, &t);
    EXPECT_EQ(t, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
out:
    SAFE_RELEASE(cl); SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_command_list_holds_resources) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    ULONG base = RC(b);
    ID3D11DeviceContext_VSSetConstantBuffers(d, 0, 1, &b);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_ClearState(d);
    /* a recorded list keeps the resources it references alive (at least until released) */
    ULONG with_list = RC(b);
    EXPECT_EQ(with_list, base); /* Win10: the list holds the resource through internal references only */
    SAFE_RELEASE(cl);
    ID3D11DeviceContext_Flush(f.ctx);
    EXPECT_EQ(RC(b), base);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_command_list_reuse_copy) {
    OPEN2();
    UINT src_data[16], zero[16] = { 0 };
    for (int i = 0; i < 16; i++) src_data[i] = 0x1000 + i;
    ID3D11Buffer *src = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 64, src_data);
    ID3D11Buffer *dst = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 64, zero);
    ID3D11CommandList *cl = NULL;
    if (!src || !dst) { t_skip("bufs"); goto out; }
    ID3D11DeviceContext_CopyResource(d, (ID3D11Resource *)dst, (ID3D11Resource *)src);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    for (int round = 0; round < 3; round++) {
        ID3D11DeviceContext_UpdateSubresource(f.ctx, (ID3D11Resource *)dst, 0, NULL, zero, 0, 0);
        void *p = NULL; size_t n = 0;
        EXPECT_HR_OK(d3dh_readback_buffer11(f.dev, f.ctx, dst, &p, &n));
        if (p) { CHECKF(((UINT *)p)[0] == 0, "round %d: pre-state", round); free(p); }
        ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
        p = NULL;
        EXPECT_HR_OK(d3dh_readback_buffer11(f.dev, f.ctx, dst, &p, &n));
        if (p) { CHECKF(n == 64 && !memcmp(p, src_data, 64), "round %d: list did not run", round); free(p); }
    }
out:
    SAFE_RELEASE(cl); SAFE_RELEASE(src); SAFE_RELEASE(dst);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_update_copies_data_at_record) {
    OPEN2();
    UINT zero[16] = { 0 }, data[16];
    for (int i = 0; i < 16; i++) data[i] = 0xA0 + i;
    ID3D11Buffer *dst = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 64, zero);
    ID3D11CommandList *cl = NULL;
    if (!dst) { t_skip("buf"); goto out; }
    UINT expect[16]; memcpy(expect, data, 64);
    ID3D11DeviceContext_UpdateSubresource(d, (ID3D11Resource *)dst, 0, NULL, data, 0, 0);
    memset(data, 0xEE, sizeof data); /* the source memory may change after the call returns */
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    void *p = NULL; size_t n = 0;
    EXPECT_HR_OK(d3dh_readback_buffer11(f.dev, f.ctx, dst, &p, &n));
    if (p) { CHECK(n == 64 && !memcmp(p, expect, 64)); free(p); }
out:
    SAFE_RELEASE(cl); SAFE_RELEASE(dst);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_not_executed_has_no_effect) {
    OPEN2();
    UINT zero[16] = { 0 }, data[16];
    for (int i = 0; i < 16; i++) data[i] = 7;
    ID3D11Buffer *dst = cs_buf_data(&f, D3D11_BIND_SHADER_RESOURCE, 64, zero);
    if (!dst) { t_skip("buf"); goto out; }
    ID3D11DeviceContext_UpdateSubresource(d, (ID3D11Resource *)dst, 0, NULL, data, 0, 0);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_Flush(f.ctx);
    void *p = NULL; size_t n = 0;
    EXPECT_HR_OK(d3dh_readback_buffer11(f.dev, f.ctx, dst, &p, &n));
    if (p) { CHECK(!memcmp(p, zero, 64)); free(p); }
    SAFE_RELEASE(cl);
out:
    SAFE_RELEASE(dst);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_execute_order) {
    OPEN();
    ID3D11DeviceContext *d1 = NULL, *d2 = NULL;
    ID3D11Texture2D *t = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    ID3D11CommandList *c1 = NULL, *c2 = NULL;
    if (!rtv || !mk_deferred(&f, &d1) || !mk_deferred(&f, &d2)) { t_skip("setup"); goto out; }
    FLOAT red[4] = { 1, 0, 0, 1 }, green[4] = { 0, 1, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(d1, rtv, red);
    ID3D11DeviceContext_ClearRenderTargetView(d2, rtv, green);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d1, FALSE, &c1), S_OK);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d2, FALSE, &c2), S_OK);
    uint32_t px = 0;
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, c2, FALSE);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, c1, FALSE);
    if (cs_read_px(&f, t, 3, 3, &px)) EXPECT_EQ(px, 0xff0000ffu); /* red last */
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, c1, FALSE);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, c2, FALSE);
    if (cs_read_px(&f, t, 3, 3, &px)) EXPECT_EQ(px, 0xff00ff00u); /* green last */
out:
    SAFE_RELEASE(c1); SAFE_RELEASE(c2); SAFE_RELEASE(d1); SAFE_RELEASE(d2); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_deferred_draw_executes) {
    OPEN2();
    ID3D11VertexShader *vs = mk_VSID(&f); ID3D11PixelShader *ps = mk_PS(&f);
    ID3D11Texture2D *t = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    ID3D11CommandList *cl = NULL;
    if (!vs || !ps || !rtv) { t_skip("setup"); goto out; }
    FLOAT blue[4] = { 0, 0, 1, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, blue);
    draw_tri(d, rtv, vs, ps, 8);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    uint32_t px = 0;
    if (cs_read_px(&f, t, 4, 4, &px)) EXPECT_EQ(px, 0xffff0000u); /* nothing drawn before execute */
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    if (cs_read_px(&f, t, 4, 4, &px)) EXPECT_EQ(px, 0xffffffffu);
    if (cs_read_px(&f, t, 0, 0, &px)) EXPECT_EQ(px, 0xffffffffu);
    if (cs_read_px(&f, t, 7, 7, &px)) EXPECT_EQ(px, 0xffffffffu);
out:
    SAFE_RELEASE(cl); SAFE_RELEASE(vs); SAFE_RELEASE(ps); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_restore_true_continues_state_across_lists) {
    OPEN2();
    ID3D11VertexShader *vs = mk_VSID(&f); ID3D11PixelShader *ps = mk_PS(&f);
    ID3D11Texture2D *t = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    ID3D11CommandList *c1 = NULL, *c2 = NULL;
    if (!vs || !ps || !rtv) { t_skip("setup"); goto out; }
    FLOAT blue[4] = { 0, 0, 1, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(f.ctx, rtv, blue);
    draw_tri(d, rtv, vs, ps, 8);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, TRUE, &c1), S_OK);
    /* with restored state a second Draw works without re-binding anything */
    ID3D11DeviceContext_Draw(d, 3, 0);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, TRUE, &c2), S_OK);
    uint32_t px = 0;
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, c2, FALSE);
    if (cs_read_px(&f, t, 4, 4, &px)) EXPECT_EQ(px, 0xffffffffu);
out:
    SAFE_RELEASE(c1); SAFE_RELEASE(c2); SAFE_RELEASE(vs); SAFE_RELEASE(ps); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_restore_false_loses_bindings_for_next_list) {
    OPEN2();
    ID3D11VertexShader *vs = mk_VSID(&f); ID3D11PixelShader *ps = mk_PS(&f);
    ID3D11Texture2D *t = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    ID3D11CommandList *c1 = NULL;
    if (!vs || !ps || !rtv) { t_skip("setup"); goto out; }
    draw_tri(d, rtv, vs, ps, 8);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &c1), S_OK);
    ID3D11RenderTargetView *g = (void *)1; ID3D11VertexShader *gv = (void *)1;
    ID3D11DeviceContext_OMGetRenderTargets(d, 1, &g, NULL); CHECK(g == NULL);
    ID3D11DeviceContext_VSGetShader(d, &gv, NULL, NULL); CHECK(gv == NULL);
out:
    SAFE_RELEASE(c1); SAFE_RELEASE(vs); SAFE_RELEASE(ps); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE2();
}

/* ---- deferred Map ------------------------------------------------------------ */
static ID3D11Buffer *dyn_buf(csf *f, UINT bytes, UINT bind)
{
    D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
    bd.ByteWidth = bytes; bd.Usage = D3D11_USAGE_DYNAMIC; bd.BindFlags = bind; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer *b = NULL;
    HRESULT hr = ID3D11Device_CreateBuffer(f->dev, &bd, NULL, &b);
    if (FAILED(hr)) { t_note("dynamic buffer: %s", d3dh_hr_name(hr)); return NULL; }
    return b;
}
TEST(d3d11_context_state_deferred_map_discard_ok) {
    OPEN2();
    ID3D11Buffer *b = dyn_buf(&f, 256, D3D11_BIND_CONSTANT_BUFFER);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    D3D11_MAPPED_SUBRESOURCE m; memset(&m, 0, sizeof m);
    EXPECT_HR(ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), S_OK);
    CHECK(m.pData != NULL);
    if (m.pData) memset(m.pData, 0x5a, 256);
    ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    SAFE_RELEASE(cl); SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_map_read_write_invalid) {
    OPEN2();
    ID3D11Buffer *b = dyn_buf(&f, 256, D3D11_BIND_CONSTANT_BUFFER);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    static const D3D11_MAP bad[] = { D3D11_MAP_READ, D3D11_MAP_READ_WRITE, D3D11_MAP_WRITE };
    D3D11_MAPPED_SUBRESOURCE m;
    for (unsigned i = 0; i < 3; i++) {
        memset(&m, 0x55, sizeof m);
        HRESULT hr = ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, bad[i], 0, &m);
        CHECKF(hr == E_INVALIDARG, "map type %d: %s", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    }
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_map_no_overwrite_without_discard) {
    OPEN2();
    ID3D11Buffer *b = dyn_buf(&f, 256, D3D11_BIND_VERTEX_BUFFER);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr = ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &m);
    CHECKF(hr == CS_ERR_DEFERRED_MAP_NO_DISCARD, "got %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    /* after a discard in the same context, no-overwrite is accepted */
    EXPECT_HR(ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), S_OK);
    ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    EXPECT_HR(ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &m), S_OK);
    ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_map_no_overwrite_after_immediate_discard) {
    OPEN2();
    ID3D11Buffer *b = dyn_buf(&f, 256, D3D11_BIND_VERTEX_BUFFER);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(ID3D11DeviceContext_Map(f.ctx, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), S_OK);
    ID3D11DeviceContext_Unmap(f.ctx, (ID3D11Resource *)b, 0);
    HRESULT hr = ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &m);
    EXPECT_HR(hr, CS_ERR_DEFERRED_MAP_NO_DISCARD);
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_map_default_usage_invalid) {
    OPEN2();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr = ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    EXPECT_HR(hr, E_INVALIDARG);
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_map_applies_on_execute) {
    OPEN2();
    ID3D11Buffer *b = dyn_buf(&f, 64, D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_SHADER_RESOURCE * 0);
    ID3D11CommandList *cl = NULL;
    if (!b) { t_skip("buf"); goto out; }
    UINT pat[16]; for (int i = 0; i < 16; i++) pat[i] = 0xC0DE0000u + i;
    D3D11_MAPPED_SUBRESOURCE m; memset(&m, 0, sizeof m);
    EXPECT_HR(ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), S_OK);
    if (!m.pData) goto out;
    memcpy(m.pData, pat, 64);
    ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, cl, FALSE);
    void *p = NULL; size_t n = 0;
    EXPECT_HR_OK(d3dh_readback_buffer11(f.dev, f.ctx, b, &p, &n));
    if (p) { CHECK(n == 64 && !memcmp(p, pat, 64)); free(p); }
out:
    SAFE_RELEASE(cl); SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_map_dynamic_texture) {
    OPEN2();
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof td);
    td.Width = 8; td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DYNAMIC; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Texture2D *t = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(f.dev, &td, NULL, &t))) { t_skip("dynamic texture"); CLOSE2(); return; }
    D3D11_MAPPED_SUBRESOURCE m; memset(&m, 0, sizeof m);
    EXPECT_HR(ID3D11DeviceContext_Map(d, (ID3D11Resource *)t, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), S_OK);
    CHECK(m.pData != NULL);
    /* Win10: deferred Map of a dynamic texture reports no pitches (the runtime records the data) */
    EXPECT_EQ(m.RowPitch, 0); EXPECT_EQ(m.DepthPitch, 0);
    ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)t, 0);
    D3D11_MAPPED_SUBRESOURCE m2;
    HRESULT hr = ID3D11DeviceContext_Map(d, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &m2);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(t);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_map_staging_read) {
    OPEN2();
    D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
    bd.ByteWidth = 64; bd.Usage = D3D11_USAGE_STAGING; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Buffer *b = NULL;
    if (FAILED(ID3D11Device_CreateBuffer(f.dev, &bd, NULL, &b))) { t_skip("staging buffer"); CLOSE2(); return; }
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr = ID3D11DeviceContext_Map(d, (ID3D11Resource *)b, 0, D3D11_MAP_READ, 0, &m);
    EXPECT_HR(hr, E_INVALIDARG); /* a deferred context cannot read back resource contents */
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(d, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_map_immediate_dynamic_still_ok) {
    OPEN2();
    ID3D11Buffer *b = dyn_buf(&f, 64, D3D11_BIND_CONSTANT_BUFFER);
    if (!b) { t_skip("buf"); CLOSE2(); return; }
    D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(ID3D11DeviceContext_Map(f.ctx, (ID3D11Resource *)b, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), S_OK);
    ID3D11DeviceContext_Unmap(f.ctx, (ID3D11Resource *)b, 0);
    HRESULT hr = ID3D11DeviceContext_Map(f.ctx, (ID3D11Resource *)b, 0, D3D11_MAP_READ, 0, &m);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(b);
    CLOSE2();
}
TEST(d3d11_context_state_deferred_release_with_pending_commands) {
    OPEN();
    ID3D11DeviceContext *d = NULL;
    if (!mk_deferred(&f, &d)) { CLOSE(); return; }
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (b) {
        ULONG base = RC(b);
        ID3D11DeviceContext_VSSetConstantBuffers(d, 0, 1, &b);
        UINT zero[16] = { 0 };
        ID3D11DeviceContext_UpdateSubresource(d, (ID3D11Resource *)b, 0, NULL, zero, 0, 0);
        EXPECT_EQ(ID3D11DeviceContext_Release(d), 0);
        d = NULL;
        EXPECT_EQ(RC(b), base);
        SAFE_RELEASE(b);
    } else ID3D11DeviceContext_Release(d);
    CLOSE();
}
TEST(d3d11_context_state_deferred_many_lists_one_context) {
    OPEN2();
    ID3D11CommandList *l[8] = { 0 };
    for (int i = 0; i < 8; i++) {
        ID3D11DeviceContext_IASetPrimitiveTopology(d, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &l[i]), S_OK);
    }
    for (int i = 0; i < 8; i++) for (int k = i + 1; k < 8; k++) if (l[i] && l[k]) CHECK(l[i] != l[k]);
    for (int i = 0; i < 8; i++) { if (l[i]) ID3D11DeviceContext_ExecuteCommandList(f.ctx, l[i], FALSE); SAFE_RELEASE(l[i]); }
    CLOSE2();
}
TEST(d3d11_context_state_deferred_execute_nested) {
    OPEN();
    ID3D11DeviceContext *outer = NULL, *inner = NULL;
    ID3D11CommandList *il = NULL, *ol = NULL;
    ID3D11Texture2D *t = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    if (!rtv || !mk_deferred(&f, &outer) || !mk_deferred(&f, &inner)) { t_skip("setup"); goto out; }
    FLOAT c[4] = { 1, 1, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(inner, rtv, c);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(inner, FALSE, &il), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(outer, il, FALSE);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(outer, FALSE, &ol), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(f.ctx, ol, FALSE);
    uint32_t px = 0;
    if (cs_read_px(&f, t, 1, 1, &px)) EXPECT_EQ(px, 0xff00ffffu); /* Win10: nested lists execute */
out:
    SAFE_RELEASE(il); SAFE_RELEASE(ol); SAFE_RELEASE(inner); SAFE_RELEASE(outer); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE();
}
