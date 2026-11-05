/* Context identity, defaults, ClearState and fixed-function (IA/OM/RS/SO) state round-trips. */
#include "csh.h"

#define P "d3d11_context_state_"

TEST(d3d11_context_state_type_immediate) {
    OPEN();
    EXPECT_EQ(ID3D11DeviceContext_GetType(f.ctx), D3D11_DEVICE_CONTEXT_IMMEDIATE);
    EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(f.ctx), 0);
    CLOSE();
}
TEST(d3d11_context_state_immediate_identity) {
    OPEN();
    ID3D11DeviceContext *a = NULL, *b = NULL;
    ULONG rc0 = RC(f.ctx);
    ID3D11Device_GetImmediateContext(f.dev, &a);
    EXPECT_EQ(a, f.ctx);
    EXPECT_EQ(RC(f.ctx), rc0 + 1);
    ID3D11Device_GetImmediateContext(f.dev, &b);
    EXPECT_EQ(b, f.ctx);
    EXPECT_EQ(RC(f.ctx), rc0 + 2);
    ID3D11DeviceContext_Release(a);
    ID3D11DeviceContext_Release(b);
    EXPECT_EQ(RC(f.ctx), rc0);
    CLOSE();
}
TEST(d3d11_context_state_getdevice) {
    OPEN();
    ID3D11Device *d = NULL;
    ULONG rc0 = RC(f.dev);
    ID3D11DeviceContext_GetDevice(f.ctx, &d);
    EXPECT_EQ(d, f.dev);
    EXPECT_EQ(RC(f.dev), rc0 + 1);
    ID3D11Device_Release(d);
    CLOSE();
}
TEST(d3d11_context_state_qi_matrix) {
    OPEN();
    void *p;
    const struct { const IID *iid; const char *name; int hard; } t[] = {
        { &IID_IUnknown, "IUnknown", 1 },
        { &IID_ID3D11DeviceChild, "ID3D11DeviceChild", 1 },
        { &IID_ID3D11DeviceContext, "ID3D11DeviceContext", 1 },
        { &IID_ID3D11DeviceContext1, "ID3D11DeviceContext1", 1 },
        { &IID_ID3D11DeviceContext2, "ID3D11DeviceContext2", 1 },
        { &IID_ID3D11DeviceContext3, "ID3D11DeviceContext3", 0 },
        { &IID_ID3D11DeviceContext4, "ID3D11DeviceContext4", 0 },
        { &IID_ID3D11Multithread, "ID3D11Multithread", 1 },
        { &IID_ID3DUserDefinedAnnotation, "ID3DUserDefinedAnnotation", 1 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        p = (void *)1;
        HRESULT hr = ID3D11DeviceContext_QueryInterface(f.ctx, t[i].iid, &p);
        if (t[i].hard) CHECKF(hr == S_OK, "%s: %s", t[i].name, d3dh_hr_name(hr));
        else t_note("QI %s -> %s", t[i].name, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
        else CHECKF(p == NULL, "%s: out pointer not cleared", t[i].name);
    }
    /* interfaces a context must not expose */
    p = (void *)1;
    EXPECT_HR(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Device, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)1;
    EXPECT_HR(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11Texture2D, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    CLOSE();
}
TEST(d3d11_context_state_qi_same_identity) {
    OPEN();
    ID3D11DeviceContext1 *c1 = NULL;
    IUnknown *u1 = NULL, *u2 = NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(f.ctx, &IID_ID3D11DeviceContext1, (void **)&c1))) { t_skip("no ctx1"); CLOSE(); return; }
    EXPECT_EQ(c1, (ID3D11DeviceContext1 *)f.ctx);
    ID3D11DeviceContext_QueryInterface(f.ctx, &IID_IUnknown, (void **)&u1);
    ID3D11DeviceContext1_QueryInterface(c1, &IID_IUnknown, (void **)&u2);
    EXPECT_EQ(u1, u2);
    u1->lpVtbl->Release(u1); u2->lpVtbl->Release(u2);
    ID3D11DeviceContext1_Release(c1);
    CLOSE();
}
TEST(d3d11_context_state_deferred_type) {
    OPEN();
    ID3D11DeviceContext *d = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(f.dev, 0, &d), S_OK);
    if (d) {
        EXPECT_EQ(ID3D11DeviceContext_GetType(d), D3D11_DEVICE_CONTEXT_DEFERRED);
        EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(d), 0);
        EXPECT_NE(d, f.ctx);
        ID3D11Device *dv = NULL;
        ID3D11DeviceContext_GetDevice(d, &dv);
        EXPECT_EQ(dv, f.dev);
        if (dv) ID3D11Device_Release(dv);
        EXPECT_EQ(ID3D11DeviceContext_Release(d), 0);
    }
    CLOSE();
}
TEST(d3d11_context_state_deferred_flags_nonzero) {
    OPEN();
    ID3D11DeviceContext *d = (void *)1;
    HRESULT hr = ID3D11Device_CreateDeferredContext(f.dev, 0x1, &d);
    t_note("CreateDeferredContext(flags=1) -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { t_note("GetContextFlags=%u", ID3D11DeviceContext_GetContextFlags(d)); ID3D11DeviceContext_Release(d); }
    CLOSE();
}
TEST(d3d11_context_state_deferred_unique) {
    OPEN();
    ID3D11DeviceContext *a = NULL, *b = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(f.dev, 0, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateDeferredContext(f.dev, 0, &b), S_OK);
    if (a && b) EXPECT_NE(a, b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_flush_repeat) {
    OPEN();
    for (int i = 0; i < 5; i++) ID3D11DeviceContext_Flush(f.ctx);
    ID3D11DeviceContext *d = NULL;
    if (SUCCEEDED(ID3D11Device_CreateDeferredContext(f.dev, 0, &d))) {
        ID3D11DeviceContext_Flush(d);
        ID3D11CommandList *cl = NULL;
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(d, FALSE, &cl), S_OK);
        SAFE_RELEASE(cl);
        ID3D11DeviceContext_Release(d);
    }
    CLOSE();
}

/* ---- defaults -------------------------------------------------------------- */
static void check_fixed_defaults(csf *f)
{
    ID3D11DeviceContext *c = f->ctx;
    /* IA */
    ID3D11Buffer *vb[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]; UINT st[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT], of[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    memset(vb, 0x55, sizeof vb); memset(st, 0x55, sizeof st); memset(of, 0x55, sizeof of);
    ID3D11DeviceContext_IAGetVertexBuffers(c, 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, vb, st, of);
    for (int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
        CHECKF(vb[i] == NULL, "vb[%d]=%p", i, vb[i]); CHECKF(st[i] == 0, "stride[%d]=%u", i, st[i]); CHECKF(of[i] == 0, "offset[%d]=%u", i, of[i]);
    }
    ID3D11Buffer *ib = (void *)1; DXGI_FORMAT ff = 0x55; UINT io = 0x55;
    ID3D11DeviceContext_IAGetIndexBuffer(c, &ib, &ff, &io);
    CHECK(ib == NULL); EXPECT_EQ(ff, DXGI_FORMAT_UNKNOWN); EXPECT_EQ(io, 0);
    ID3D11InputLayout *il = (void *)1;
    ID3D11DeviceContext_IAGetInputLayout(c, &il); CHECK(il == NULL);
    D3D11_PRIMITIVE_TOPOLOGY top = 0x55;
    ID3D11DeviceContext_IAGetPrimitiveTopology(c, &top); EXPECT_EQ(top, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    /* OM */
    ID3D11RenderTargetView *rt[8]; ID3D11DepthStencilView *dsv = (void *)1;
    memset(rt, 0x55, sizeof rt);
    ID3D11DeviceContext_OMGetRenderTargets(c, 8, rt, &dsv);
    for (int i = 0; i < 8; i++) CHECKF(rt[i] == NULL, "rtv[%d]=%p", i, rt[i]);
    CHECK(dsv == NULL);
    ID3D11BlendState *bs = (void *)1; FLOAT bf[4] = { 9, 9, 9, 9 }; UINT mask = 0;
    ID3D11DeviceContext_OMGetBlendState(c, &bs, bf, &mask);
    CHECK(bs == NULL);
    for (int i = 0; i < 4; i++) CHECKF(bf[i] == 1.0f, "blend factor[%d]=%g", i, bf[i]);
    EXPECT_EQ(mask, 0xffffffffu);
    ID3D11DepthStencilState *ds = (void *)1; UINT ref = 0x55;
    ID3D11DeviceContext_OMGetDepthStencilState(c, &ds, &ref);
    CHECK(ds == NULL); EXPECT_EQ(ref, 0);
    /* RS */
    ID3D11RasterizerState *rs = (void *)1;
    ID3D11DeviceContext_RSGetState(c, &rs); CHECK(rs == NULL);
    UINT nv = 99, ns = 99;
    ID3D11DeviceContext_RSGetViewports(c, &nv, NULL); EXPECT_EQ(nv, 0);
    ID3D11DeviceContext_RSGetScissorRects(c, &ns, NULL); EXPECT_EQ(ns, 0);
    /* SO */
    ID3D11Buffer *so[4]; memset(so, 0x55, sizeof so);
    ID3D11DeviceContext_SOGetTargets(c, 4, so);
    for (int i = 0; i < 4; i++) CHECKF(so[i] == NULL, "so[%d]=%p", i, so[i]);
    /* predication */
    ID3D11Predicate *pr = (void *)1; BOOL pv = 0x55;
    ID3D11DeviceContext_GetPredication(c, &pr, &pv);
    CHECK(pr == NULL); EXPECT_EQ(pv, FALSE);
}
TEST(d3d11_context_state_defaults_fresh) {
    OPEN();
    check_fixed_defaults(&f);
    CLOSE();
}
TEST(d3d11_context_state_clearstate_idempotent) {
    OPEN();
    ID3D11DeviceContext_ClearState(f.ctx);
    ID3D11DeviceContext_ClearState(f.ctx);
    check_fixed_defaults(&f);
    CLOSE();
}
TEST(d3d11_context_state_clearstate_fixed_function) {
    OPEN();
    ID3D11Buffer *vb = cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 256), *ib = cs_buf(&f, D3D11_BIND_INDEX_BUFFER, 256);
    ID3D11Buffer *so = cs_buf(&f, D3D11_BIND_STREAM_OUTPUT, 256);
    ID3D11Texture2D *tx = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *dt = cs_tex(&f, 8, 8, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL);
    ID3D11RenderTargetView *rtv = tx ? cs_rtv(&f, (ID3D11Resource *)tx, NULL) : NULL;
    ID3D11DepthStencilView *dsv = NULL;
    if (dt) ID3D11Device_CreateDepthStencilView(f.dev, (ID3D11Resource *)dt, NULL, &dsv);
    ID3D11InputLayout *il = cs_layout(&f);
    ID3D11BlendState *bs = cs_bs(&f, TRUE);
    ID3D11DepthStencilState *dss = cs_dss(&f, TRUE);
    ID3D11RasterizerState *rs = cs_rs(&f, D3D11_CULL_NONE, TRUE);
    if (!vb || !ib || !so || !rtv || !dsv || !il || !bs || !dss || !rs) { t_skip("resource creation failed"); goto out; }
    UINT stride = 16, off = 4, sooff = 0;
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 2, 1, &vb, &stride, &off);
    ID3D11DeviceContext_IASetIndexBuffer(f.ctx, ib, DXGI_FORMAT_R16_UINT, 2);
    ID3D11DeviceContext_IASetInputLayout(f.ctx, il);
    ID3D11DeviceContext_IASetPrimitiveTopology(f.ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, dsv);
    FLOAT bf[4] = { .1f, .2f, .3f, .4f };
    ID3D11DeviceContext_OMSetBlendState(f.ctx, bs, bf, 0x0f0f0f0f);
    ID3D11DeviceContext_OMSetDepthStencilState(f.ctx, dss, 7);
    ID3D11DeviceContext_RSSetState(f.ctx, rs);
    D3D11_VIEWPORT vp = { 1, 2, 3, 4, 0.25f, 0.75f }; D3D11_RECT sr = { 1, 2, 3, 4 };
    ID3D11DeviceContext_RSSetViewports(f.ctx, 1, &vp);
    ID3D11DeviceContext_RSSetScissorRects(f.ctx, 1, &sr);
    ID3D11DeviceContext_SOSetTargets(f.ctx, 1, &so, &sooff);
    ID3D11DeviceContext_ClearState(f.ctx);
    check_fixed_defaults(&f);
out:
    SAFE_RELEASE(vb); SAFE_RELEASE(ib); SAFE_RELEASE(so); SAFE_RELEASE(rtv); SAFE_RELEASE(dsv); SAFE_RELEASE(tx); SAFE_RELEASE(dt);
    SAFE_RELEASE(il); SAFE_RELEASE(bs); SAFE_RELEASE(dss); SAFE_RELEASE(rs);
    CLOSE();
}
TEST(d3d11_context_state_clearstate_releases_refs) {
    OPEN();
    ID3D11Buffer *vb = cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 64);
    if (!vb) { t_skip("buf"); CLOSE(); return; }
    ULONG base = RC(vb);
    UINT s = 16, o = 0;
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 1, &vb, &s, &o);
    CHECK(RC(vb) > base);
    ID3D11DeviceContext_ClearState(f.ctx);
    EXPECT_EQ(RC(vb), base);
    SAFE_RELEASE(vb);
    CLOSE();
}
TEST(d3d11_context_state_set_holds_ref_get_adds_ref) {
    OPEN();
    ID3D11Buffer *vb = cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 64);
    if (!vb) { t_skip("buf"); CLOSE(); return; }
    UINT s = 16, o = 0;
    ULONG base = RC(vb);
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 1, &vb, &s, &o);
    ULONG bound = RC(vb);
    CHECKF(bound > base, "binding must hold a reference (%lu -> %lu)", base, bound);
    ID3D11Buffer *g = NULL;
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 0, 1, &g, NULL, NULL);
    EXPECT_EQ(g, vb);
    EXPECT_EQ(RC(vb), bound + 1);
    ID3D11Buffer_Release(g);
    EXPECT_EQ(RC(vb), bound);
    /* destroying the app reference must keep the object alive while bound */
    ID3D11Buffer_Release(vb);
    g = NULL;
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 0, 1, &g, NULL, NULL);
    CHECK(g != NULL);
    if (g) ID3D11Buffer_Release(g);
    ID3D11DeviceContext_ClearState(f.ctx);
    CLOSE();
}

/* ---- IA --------------------------------------------------------------------- */
TEST(d3d11_context_state_ia_topology_roundtrip) {
    OPEN();
    static const D3D11_PRIMITIVE_TOPOLOGY t[] = {
        D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, D3D11_PRIMITIVE_TOPOLOGY_LINELIST, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        D3D11_PRIMITIVE_TOPOLOGY g = 0x55;
        ID3D11DeviceContext_IASetPrimitiveTopology(f.ctx, t[i]);
        ID3D11DeviceContext_IAGetPrimitiveTopology(f.ctx, &g);
        CHECKF(g == t[i], "topology %d -> %d", t[i], g);
    }
    CLOSE();
}
TEST(d3d11_context_state_ia_topology_patchlists) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    for (int n = 1; n <= 32; n++) {
        D3D11_PRIMITIVE_TOPOLOGY want = (D3D11_PRIMITIVE_TOPOLOGY)(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + n - 1), g = 0x55;
        ID3D11DeviceContext_IASetPrimitiveTopology(f.ctx, want);
        ID3D11DeviceContext_IAGetPrimitiveTopology(f.ctx, &g);
        CHECKF(g == want, "patch %d: %d -> %d", n, want, g);
    }
    CLOSE();
}
TEST(d3d11_context_state_ia_topology_undefined_set) {
    OPEN();
    D3D11_PRIMITIVE_TOPOLOGY g = 0x55;
    ID3D11DeviceContext_IASetPrimitiveTopology(f.ctx, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D11DeviceContext_IASetPrimitiveTopology(f.ctx, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    ID3D11DeviceContext_IAGetPrimitiveTopology(f.ctx, &g);
    EXPECT_EQ(g, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
    CLOSE();
}
TEST(d3d11_context_state_ia_vb_each_slot) {
    OPEN();
    ID3D11Buffer *vb = cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 1024);
    if (!vb) { t_skip("buf"); CLOSE(); return; }
    for (UINT slot = 0; slot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; slot++) {
        UINT s = 4 + slot, o = 8 * slot, gs = 0, go = 0;
        ID3D11Buffer *g = NULL;
        ID3D11DeviceContext_IASetVertexBuffers(f.ctx, slot, 1, &vb, &s, &o);
        ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, slot, 1, &g, &gs, &go);
        CHECKF(g == vb, "slot %u buffer", slot);
        CHECKF(gs == s, "slot %u stride %u", slot, gs);
        CHECKF(go == o, "slot %u offset %u", slot, go);
        if (g) ID3D11Buffer_Release(g);
        ID3D11DeviceContext_IASetVertexBuffers(f.ctx, slot, 1, (ID3D11Buffer *[]){ NULL }, &s, &o);
    }
    SAFE_RELEASE(vb);
    CLOSE();
}
TEST(d3d11_context_state_ia_vb_range) {
    OPEN();
    ID3D11Buffer *b[3] = { cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 256), cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 256), cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 256) };
    if (!b[0] || !b[1] || !b[2]) { t_skip("buf"); goto out; }
    UINT s[3] = { 12, 16, 20 }, o[3] = { 0, 4, 8 };
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 3, 3, b, s, o);
    ID3D11Buffer *g[8]; UINT gs[8], go[8];
    memset(g, 0x55, sizeof g);
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 0, 8, g, gs, go);
    for (int i = 0; i < 8; i++) {
        ID3D11Buffer *want = (i >= 3 && i < 6) ? b[i - 3] : NULL;
        CHECKF(g[i] == want, "slot %d: %p want %p", i, g[i], want);
        if (want) { CHECKF(gs[i] == s[i - 3], "stride[%d]=%u", i, gs[i]); CHECKF(go[i] == o[i - 3], "offset[%d]=%u", i, go[i]); }
        else { CHECKF(gs[i] == 0, "stride[%d]=%u", i, gs[i]); CHECKF(go[i] == 0, "offset[%d]=%u", i, go[i]); }
        if (g[i]) ID3D11Buffer_Release(g[i]);
    }
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    for (int i = 0; i < 3; i++) SAFE_RELEASE(b[i]);
    CLOSE();
}
TEST(d3d11_context_state_ia_vb_null_outputs) {
    OPEN();
    ID3D11Buffer *vb = cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 256);
    if (!vb) { t_skip("buf"); CLOSE(); return; }
    UINT s = 24, o = 12, g1 = 0, g2 = 0;
    ID3D11Buffer *g = NULL;
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 1, 1, &vb, &s, &o);
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 1, 1, &g, NULL, NULL); EXPECT_EQ(g, vb); SAFE_RELEASE(g);
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 1, 1, NULL, &g1, NULL); EXPECT_EQ(g1, 24);
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 1, 1, NULL, NULL, &g2); EXPECT_EQ(g2, 12);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(vb);
    CLOSE();
}
TEST(d3d11_context_state_ia_vb_zero_count_noop) {
    OPEN();
    ID3D11Buffer *vb = cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 256);
    if (!vb) { t_skip("buf"); CLOSE(); return; }
    UINT s = 24, o = 12;
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 1, &vb, &s, &o);
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 0, NULL, NULL, NULL);
    ID3D11Buffer *g = NULL;
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 0, 1, &g, NULL, NULL);
    EXPECT_EQ(g, vb); SAFE_RELEASE(g);
    /* a zero-count Get must not write anything */
    ID3D11Buffer *sentinel = (ID3D11Buffer *)(ULONG_PTR)0x1234; UINT sst = 0x77;
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 0, 0, &sentinel, &sst, NULL);
    EXPECT_EQ((ULONG_PTR)sentinel, 0x1234); EXPECT_EQ(sst, 0x77);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(vb);
    CLOSE();
}
TEST(d3d11_context_state_ia_vb_null_unbinds_one) {
    OPEN();
    ID3D11Buffer *b[2] = { cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 64), cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 64) };
    if (!b[0] || !b[1]) { t_skip("buf"); goto out; }
    UINT s[2] = { 16, 16 }, o[2] = { 0, 0 };
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 2, b, s, o);
    ID3D11Buffer *n[1] = { NULL };
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 1, n, s, o);
    ID3D11Buffer *g[2] = { 0 };
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 0, 2, g, NULL, NULL);
    CHECK(g[0] == NULL); EXPECT_EQ(g[1], b[1]);
    SAFE_RELEASE(g[1]);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(b[0]); SAFE_RELEASE(b[1]);
    CLOSE();
}
TEST(d3d11_context_state_ia_index_buffer) {
    OPEN();
    ID3D11Buffer *ib = cs_buf(&f, D3D11_BIND_INDEX_BUFFER, 256);
    if (!ib) { t_skip("buf"); CLOSE(); return; }
    static const struct { DXGI_FORMAT fmt; UINT off; } t[] = {
        { DXGI_FORMAT_R16_UINT, 0 }, { DXGI_FORMAT_R16_UINT, 2 }, { DXGI_FORMAT_R32_UINT, 0 }, { DXGI_FORMAT_R32_UINT, 64 } };
    for (unsigned i = 0; i < 4; i++) {
        ID3D11Buffer *g = NULL; DXGI_FORMAT gf = 0; UINT go = 0xff;
        ID3D11DeviceContext_IASetIndexBuffer(f.ctx, ib, t[i].fmt, t[i].off);
        ID3D11DeviceContext_IAGetIndexBuffer(f.ctx, &g, &gf, &go);
        CHECKF(g == ib, "case %u buffer", i); CHECKF(gf == t[i].fmt, "case %u fmt %d", i, gf); CHECKF(go == t[i].off, "case %u off %u", i, go);
        SAFE_RELEASE(g);
    }
    /* Get with each output optional */
    ID3D11Buffer *g = NULL; DXGI_FORMAT gf = 0; UINT go = 0xff;
    ID3D11DeviceContext_IAGetIndexBuffer(f.ctx, &g, NULL, NULL); EXPECT_EQ(g, ib); SAFE_RELEASE(g);
    ID3D11DeviceContext_IAGetIndexBuffer(f.ctx, NULL, &gf, NULL); EXPECT_EQ(gf, DXGI_FORMAT_R32_UINT);
    ID3D11DeviceContext_IAGetIndexBuffer(f.ctx, NULL, NULL, &go); EXPECT_EQ(go, 64);
    /* NULL unbinds */
    ID3D11DeviceContext_IASetIndexBuffer(f.ctx, NULL, DXGI_FORMAT_R16_UINT, 0);
    g = (void *)1;
    ID3D11DeviceContext_IAGetIndexBuffer(f.ctx, &g, &gf, &go);
    CHECK(g == NULL);
    t_note("after IASetIndexBuffer(NULL, R16_UINT, 0): format=%d offset=%u", gf, go);
    SAFE_RELEASE(ib);
    CLOSE();
}
TEST(d3d11_context_state_ia_input_layout) {
    OPEN();
    ID3D11InputLayout *il = cs_layout(&f);
    if (!il) { t_skip("layout"); CLOSE(); return; }
    ULONG base = RC(il);
    ID3D11DeviceContext_IASetInputLayout(f.ctx, il);
    ID3D11InputLayout *g = NULL;
    ID3D11DeviceContext_IAGetInputLayout(f.ctx, &g);
    EXPECT_EQ(g, il);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_IASetInputLayout(f.ctx, NULL);
    g = (void *)1;
    ID3D11DeviceContext_IAGetInputLayout(f.ctx, &g);
    CHECK(g == NULL);
    EXPECT_EQ(RC(il), base);
    SAFE_RELEASE(il);
    CLOSE();
}
TEST(d3d11_context_state_ia_vb_refcount_symmetry) {
    OPEN();
    ID3D11Buffer *vb = cs_buf(&f, D3D11_BIND_VERTEX_BUFFER, 64);
    if (!vb) { t_skip("buf"); CLOSE(); return; }
    ULONG base = RC(vb);
    UINT s = 8, o = 0;
    for (int i = 0; i < 5; i++) ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 1, &vb, &s, &o);
    ULONG once = RC(vb);
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 1, &vb, &s, &o);
    EXPECT_EQ(RC(vb), once);
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 5, 1, &vb, &s, &o);
    EXPECT_EQ(RC(vb), once + 1);
    ID3D11DeviceContext_ClearState(f.ctx);
    EXPECT_EQ(RC(vb), base);
    SAFE_RELEASE(vb);
    CLOSE();
}

/* ---- OM ---------------------------------------------------------------------- */
TEST(d3d11_context_state_om_rtv_counts) {
    OPEN();
    ID3D11Texture2D *t[8] = { 0 }; ID3D11RenderTargetView *v[8] = { 0 };
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        t[i] = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
        v[i] = t[i] ? cs_rtv(&f, (ID3D11Resource *)t[i], NULL) : NULL;
        if (!v[i]) ok = 0;
    }
    if (!ok) { t_skip("rtv creation"); goto out; }
    for (UINT n = 1; n <= 8; n++) {
        ID3D11RenderTargetView *g[8]; memset(g, 0x55, sizeof g);
        ID3D11DeviceContext_OMSetRenderTargets(f.ctx, n, v, NULL);
        ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 8, g, NULL);
        for (UINT i = 0; i < 8; i++) {
            ID3D11RenderTargetView *want = i < n ? v[i] : NULL;
            CHECKF(g[i] == want, "n=%u slot %u: %p want %p", n, i, g[i], want);
            if (g[i]) ID3D11RenderTargetView_Release(g[i]);
        }
    }
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    for (int i = 0; i < 8; i++) { SAFE_RELEASE(v[i]); SAFE_RELEASE(t[i]); }
    CLOSE();
}
TEST(d3d11_context_state_om_zero_unbinds_all) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *dt = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL);
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    ID3D11DepthStencilView *dsv = NULL;
    if (dt) ID3D11Device_CreateDepthStencilView(f.dev, (ID3D11Resource *)dt, NULL, &dsv);
    if (!rtv || !dsv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, dsv);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 0, NULL, NULL);
    ID3D11RenderTargetView *g = (void *)1; ID3D11DepthStencilView *gd = (void *)1;
    ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 1, &g, &gd);
    CHECK(g == NULL); CHECK(gd == NULL);
out:
    SAFE_RELEASE(rtv); SAFE_RELEASE(dsv); SAFE_RELEASE(t); SAFE_RELEASE(dt);
    CLOSE();
}
TEST(d3d11_context_state_om_dsv_only) {
    OPEN();
    ID3D11Texture2D *dt = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_D32_FLOAT, D3D11_BIND_DEPTH_STENCIL);
    ID3D11DepthStencilView *dsv = NULL, *g = NULL;
    if (dt) ID3D11Device_CreateDepthStencilView(f.dev, (ID3D11Resource *)dt, NULL, &dsv);
    if (!dsv) { t_skip("dsv"); goto out; }
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 0, NULL, dsv);
    ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 0, NULL, &g);
    EXPECT_EQ(g, dsv);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 0, NULL, NULL);
    ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 0, NULL, &g);
    CHECK(g == NULL);
out:
    SAFE_RELEASE(dsv); SAFE_RELEASE(dt);
    CLOSE();
}
TEST(d3d11_context_state_om_rtv_gap) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    if (!rtv) { t_skip("rtv"); goto out; }
    ID3D11RenderTargetView *set[3] = { NULL, rtv, NULL }, *g[3] = { 0 };
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 3, set, NULL);
    ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 3, g, NULL);
    CHECK(g[0] == NULL); EXPECT_EQ(g[1], rtv); CHECK(g[2] == NULL);
    SAFE_RELEASE(g[1]);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_om_rtv_replaced) {
    OPEN();
    ID3D11Texture2D *t[2] = { cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET), cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET) };
    ID3D11RenderTargetView *v[2] = { t[0] ? cs_rtv(&f, (ID3D11Resource *)t[0], NULL) : NULL, t[1] ? cs_rtv(&f, (ID3D11Resource *)t[1], NULL) : NULL };
    if (!v[0] || !v[1]) { t_skip("rtv"); goto out; }
    ULONG b0 = RC(v[0]);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &v[0], NULL);
    CHECK(RC(v[0]) > b0);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &v[1], NULL);
    EXPECT_EQ(RC(v[0]), b0);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    for (int i = 0; i < 2; i++) { SAFE_RELEASE(v[i]); SAFE_RELEASE(t[i]); }
    CLOSE();
}
TEST(d3d11_context_state_om_blend_roundtrip) {
    OPEN();
    ID3D11BlendState *bs = cs_bs(&f, TRUE);
    if (!bs) { t_skip("bs"); CLOSE(); return; }
    static const struct { FLOAT f[4]; UINT mask; } t[] = {
        { { 0, 0, 0, 0 }, 0 }, { { .25f, .5f, .75f, 1.f }, 0x1 }, { { 1, 1, 1, 1 }, 0xffffffffu }, { { -1.f, 2.f, 0.f, 8.f }, 0x80000000u } };
    for (unsigned i = 0; i < 4; i++) {
        ID3D11BlendState *g = NULL; FLOAT gf[4] = { 7, 7, 7, 7 }; UINT gm = 0;
        ID3D11DeviceContext_OMSetBlendState(f.ctx, bs, t[i].f, t[i].mask);
        ID3D11DeviceContext_OMGetBlendState(f.ctx, &g, gf, &gm);
        CHECKF(g == bs, "case %u state", i);
        for (int k = 0; k < 4; k++) CHECKF(gf[k] == t[i].f[k], "case %u factor[%d]=%g", i, k, gf[k]);
        CHECKF(gm == t[i].mask, "case %u mask 0x%x", i, gm);
        SAFE_RELEASE(g);
    }
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(bs);
    CLOSE();
}
TEST(d3d11_context_state_om_blend_null_factor) {
    OPEN();
    ID3D11BlendState *bs = cs_bs(&f, FALSE);
    if (!bs) { t_skip("bs"); CLOSE(); return; }
    FLOAT nz[4] = { .1f, .2f, .3f, .4f }, gf[4] = { 5, 5, 5, 5 };
    ID3D11DeviceContext_OMSetBlendState(f.ctx, bs, nz, 0xff);
    ID3D11DeviceContext_OMSetBlendState(f.ctx, bs, NULL, 0xff);
    ID3D11DeviceContext_OMGetBlendState(f.ctx, NULL, gf, NULL);
    for (int k = 0; k < 4; k++) CHECKF(gf[k] == 1.0f, "factor[%d]=%g after NULL factor", k, gf[k]);
    ID3D11DeviceContext_OMSetBlendState(f.ctx, NULL, NULL, 0xffffffffu);
    ID3D11BlendState *g = (void *)1; UINT gm = 0;
    ID3D11DeviceContext_OMGetBlendState(f.ctx, &g, NULL, &gm);
    CHECK(g == NULL); EXPECT_EQ(gm, 0xffffffffu);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(bs);
    CLOSE();
}
TEST(d3d11_context_state_om_blend_get_optional) {
    OPEN();
    ID3D11BlendState *bs = cs_bs(&f, TRUE);
    if (!bs) { t_skip("bs"); CLOSE(); return; }
    FLOAT fac[4] = { 1, 2, 3, 4 };
    ID3D11DeviceContext_OMSetBlendState(f.ctx, bs, fac, 0x1234);
    UINT m = 0; ID3D11BlendState *g = NULL;
    ID3D11DeviceContext_OMGetBlendState(f.ctx, NULL, NULL, &m); EXPECT_EQ(m, 0x1234);
    ID3D11DeviceContext_OMGetBlendState(f.ctx, &g, NULL, NULL); EXPECT_EQ(g, bs); SAFE_RELEASE(g);
    ID3D11DeviceContext_OMGetBlendState(f.ctx, NULL, NULL, NULL);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(bs);
    CLOSE();
}
TEST(d3d11_context_state_om_blend_state_refcount) {
    OPEN();
    ID3D11BlendState *bs = cs_bs(&f, TRUE);
    if (!bs) { t_skip("bs"); CLOSE(); return; }
    ULONG base = RC(bs);
    ID3D11DeviceContext_OMSetBlendState(f.ctx, bs, NULL, ~0u);
    CHECK(RC(bs) > base);
    ID3D11DeviceContext_OMSetBlendState(f.ctx, NULL, NULL, ~0u);
    EXPECT_EQ(RC(bs), base);
    SAFE_RELEASE(bs);
    CLOSE();
}
TEST(d3d11_context_state_om_dss_roundtrip) {
    OPEN();
    ID3D11DepthStencilState *ds = cs_dss(&f, TRUE);
    if (!ds) { t_skip("dss"); CLOSE(); return; }
    static const UINT refs[] = { 0, 1, 0x7f, 0xff, 0x100, 0xffffffffu };
    for (unsigned i = 0; i < sizeof refs / sizeof refs[0]; i++) {
        ID3D11DepthStencilState *g = NULL; UINT gr = 0x55;
        ID3D11DeviceContext_OMSetDepthStencilState(f.ctx, ds, refs[i]);
        ID3D11DeviceContext_OMGetDepthStencilState(f.ctx, &g, &gr);
        CHECKF(g == ds, "ref %u state", refs[i]);
        CHECKF(gr == refs[i], "ref 0x%x -> 0x%x", refs[i], gr);
        SAFE_RELEASE(g);
    }
    ID3D11DeviceContext_OMSetDepthStencilState(f.ctx, NULL, 3);
    ID3D11DepthStencilState *g = (void *)1; UINT gr = 0;
    ID3D11DeviceContext_OMGetDepthStencilState(f.ctx, &g, &gr);
    CHECK(g == NULL); EXPECT_EQ(gr, 3);
    ID3D11DeviceContext_OMGetDepthStencilState(f.ctx, NULL, &gr);
    ID3D11DeviceContext_OMGetDepthStencilState(f.ctx, &g, NULL);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(ds);
    CLOSE();
}
TEST(d3d11_context_state_om_keep_rtv_dsv) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET);
    ID3D11Texture2D *dt = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL);
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    ID3D11DepthStencilView *dsv = NULL;
    if (dt) ID3D11Device_CreateDepthStencilView(f.dev, (ID3D11Resource *)dt, NULL, &dsv);
    ID3D11Buffer *ub = cs_buf(&f, D3D11_BIND_UNORDERED_ACCESS, 64);
    ID3D11UnorderedAccessView *uav = NULL;
    if (ub) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
        ud.Format = DXGI_FORMAT_R32_UINT; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 16;
        ID3D11Device_CreateUnorderedAccessView(f.dev, (ID3D11Resource *)ub, &ud, &uav);
    }
    if (!rtv || !dsv || !uav) { t_skip("views"); goto out; }
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, dsv);
    /* keep RTV/DSV, bind a UAV at slot 1 */
    UINT init = 0xffffffffu;
    ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(f.ctx, D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, NULL, NULL, 1, 1, &uav, &init);
    ID3D11RenderTargetView *grt = NULL; ID3D11DepthStencilView *gds = NULL; ID3D11UnorderedAccessView *gu = NULL;
    ID3D11DeviceContext_OMGetRenderTargetsAndUnorderedAccessViews(f.ctx, 1, &grt, &gds, 1, 1, &gu);
    EXPECT_EQ(grt, rtv); EXPECT_EQ(gds, dsv); EXPECT_EQ(gu, uav);
    SAFE_RELEASE(grt); SAFE_RELEASE(gds); SAFE_RELEASE(gu);
    /* keep UAVs, change RTV to none */
    ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(f.ctx, 1, &rtv, dsv, 1, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, NULL, NULL);
    ID3D11DeviceContext_OMGetRenderTargetsAndUnorderedAccessViews(f.ctx, 1, &grt, &gds, 1, 1, &gu);
    EXPECT_EQ(grt, rtv); EXPECT_EQ(gds, dsv); EXPECT_EQ(gu, uav);
    SAFE_RELEASE(grt); SAFE_RELEASE(gds); SAFE_RELEASE(gu);
    /* plain OMSetRenderTargets does not drop UAVs placed after the RTVs?  observation only */
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, dsv);
    ID3D11DeviceContext_OMGetRenderTargetsAndUnorderedAccessViews(f.ctx, 0, NULL, NULL, 1, 1, &gu);
    t_note("UAV slot 1 after plain OMSetRenderTargets: %p (bound earlier: %p)", gu, uav);
    SAFE_RELEASE(gu);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(uav); SAFE_RELEASE(ub); SAFE_RELEASE(rtv); SAFE_RELEASE(dsv); SAFE_RELEASE(t); SAFE_RELEASE(dt);
    CLOSE();
}
TEST(d3d11_context_state_om_uav_roundtrip_slots) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Buffer *ub = cs_buf(&f, D3D11_BIND_UNORDERED_ACCESS, 64);
    ID3D11UnorderedAccessView *uav = NULL;
    if (ub) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
        ud.Format = DXGI_FORMAT_R32_UINT; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 16;
        ID3D11Device_CreateUnorderedAccessView(f.dev, (ID3D11Resource *)ub, &ud, &uav);
    }
    if (!uav) { t_skip("uav"); goto out; }
    for (UINT slot = 1; slot < D3D11_PS_CS_UAV_REGISTER_COUNT; slot++) {
        ID3D11UnorderedAccessView *g = NULL; UINT init = 0xffffffffu;
        ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(f.ctx, 0, NULL, NULL, slot, 1, &uav, &init);
        ID3D11DeviceContext_OMGetRenderTargetsAndUnorderedAccessViews(f.ctx, 0, NULL, NULL, slot, 1, &g);
        CHECKF(g == uav, "UAV slot %u", slot);
        SAFE_RELEASE(g);
    }
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(uav); SAFE_RELEASE(ub);
    CLOSE();
}

/* ---- RS ------------------------------------------------------------------------ */
TEST(d3d11_context_state_rs_state_roundtrip) {
    OPEN();
    ID3D11RasterizerState *a = cs_rs(&f, D3D11_CULL_NONE, FALSE), *b = cs_rs(&f, D3D11_CULL_BACK, TRUE);
    if (!a || !b) { t_skip("rs"); goto out; }
    ID3D11RasterizerState *g = NULL;
    ID3D11DeviceContext_RSSetState(f.ctx, a); ID3D11DeviceContext_RSGetState(f.ctx, &g); EXPECT_EQ(g, a); SAFE_RELEASE(g);
    ID3D11DeviceContext_RSSetState(f.ctx, b); ID3D11DeviceContext_RSGetState(f.ctx, &g); EXPECT_EQ(g, b); SAFE_RELEASE(g);
    ID3D11DeviceContext_RSSetState(f.ctx, NULL); g = (void *)1; ID3D11DeviceContext_RSGetState(f.ctx, &g); CHECK(g == NULL);
out:
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_rs_viewports_roundtrip) {
    OPEN();
    for (UINT n = 1; n <= D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; n++) {
        D3D11_VIEWPORT v[16], g[16]; UINT cnt = 16;
        for (UINT i = 0; i < n; i++) { v[i].TopLeftX = (FLOAT)i; v[i].TopLeftY = (FLOAT)(2 * i); v[i].Width = 3.f + i; v[i].Height = 4.f + i; v[i].MinDepth = i * 0.01f; v[i].MaxDepth = (i + 1) * 0.01f; }
        memset(g, 0x55, sizeof g);
        ID3D11DeviceContext_RSSetViewports(f.ctx, n, v);
        UINT q = 0; ID3D11DeviceContext_RSGetViewports(f.ctx, &q, NULL);
        CHECKF(q == n, "count query after setting %u: %u", n, q);
        ID3D11DeviceContext_RSGetViewports(f.ctx, &cnt, g);
        CHECKF(cnt == n, "n=%u cnt=%u", n, cnt);
        for (UINT i = 0; i < n && i < cnt; i++)
            CHECKF(!memcmp(&v[i], &g[i], sizeof v[i]), "n=%u viewport %u mismatch", n, i);
    }
    CLOSE();
}
TEST(d3d11_context_state_rs_viewports_zero_clears) {
    OPEN();
    D3D11_VIEWPORT v = { 0, 0, 8, 8, 0, 1 };
    ID3D11DeviceContext_RSSetViewports(f.ctx, 1, &v);
    ID3D11DeviceContext_RSSetViewports(f.ctx, 0, NULL);
    UINT q = 99; ID3D11DeviceContext_RSGetViewports(f.ctx, &q, NULL);
    EXPECT_EQ(q, 0);
    CLOSE();
}
TEST(d3d11_context_state_rs_viewports_replace_fewer) {
    OPEN();
    D3D11_VIEWPORT v[3] = { { 0, 0, 1, 1, 0, 1 }, { 1, 1, 2, 2, 0, 1 }, { 2, 2, 3, 3, 0, 1 } };
    ID3D11DeviceContext_RSSetViewports(f.ctx, 3, v);
    ID3D11DeviceContext_RSSetViewports(f.ctx, 1, &v[2]);
    UINT q = 0; ID3D11DeviceContext_RSGetViewports(f.ctx, &q, NULL);
    EXPECT_EQ(q, 1);
    D3D11_VIEWPORT g[3]; UINT c = 3; memset(g, 0x55, sizeof g);
    ID3D11DeviceContext_RSGetViewports(f.ctx, &c, g);
    EXPECT_EQ(c, 1);
    CHECK(!memcmp(&g[0], &v[2], sizeof g[0]));
    CLOSE();
}
TEST(d3d11_context_state_rs_viewports_get_smaller_buffer) {
    OPEN();
    D3D11_VIEWPORT v[4] = { { 0, 0, 1, 1, 0, 1 }, { 1, 1, 2, 2, 0, 1 }, { 2, 2, 3, 3, 0, 1 }, { 3, 3, 4, 4, 0, 1 } };
    ID3D11DeviceContext_RSSetViewports(f.ctx, 4, v);
    D3D11_VIEWPORT g[4]; UINT c = 2; memset(g, 0x55, sizeof g);
    ID3D11DeviceContext_RSGetViewports(f.ctx, &c, g);
    CHECK(!memcmp(&g[0], &v[0], sizeof g[0])); CHECK(!memcmp(&g[1], &v[1], sizeof g[1]));
    t_note("RSGetViewports(cap 2 of 4): *num=%u, slot2 untouched=%d", c, ((unsigned char *)&g[2])[0] == 0x55);
    CLOSE();
}
TEST(d3d11_context_state_rs_viewports_get_larger_buffer) {
    OPEN();
    D3D11_VIEWPORT v[2] = { { 5, 6, 7, 8, 0.1f, 0.9f }, { 1, 2, 3, 4, 0, 1 } };
    ID3D11DeviceContext_RSSetViewports(f.ctx, 2, v);
    D3D11_VIEWPORT g[4]; UINT c = 4; memset(g, 0x55, sizeof g);
    ID3D11DeviceContext_RSGetViewports(f.ctx, &c, g);
    EXPECT_EQ(c, 2);
    CHECK(!memcmp(&g[0], &v[0], sizeof g[0])); CHECK(!memcmp(&g[1], &v[1], sizeof g[1]));
    D3D11_VIEWPORT z; memset(&z, 0, sizeof z);
    t_note("RSGetViewports tail zeroed: %d", !memcmp(&g[2], &z, sizeof z));
    CLOSE();
}
TEST(d3d11_context_state_rs_scissors_roundtrip) {
    OPEN();
    for (UINT n = 1; n <= D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; n++) {
        D3D11_RECT r[16], g[16]; UINT cnt = 16;
        for (UINT i = 0; i < n; i++) { r[i].left = i; r[i].top = 2 * i; r[i].right = i + 1; r[i].bottom = 2 * (i + 1); }
        memset(g, 0x55, sizeof g);
        ID3D11DeviceContext_RSSetScissorRects(f.ctx, n, r);
        UINT q = 0; ID3D11DeviceContext_RSGetScissorRects(f.ctx, &q, NULL);
        CHECKF(q == n, "count query n=%u: %u", n, q);
        ID3D11DeviceContext_RSGetScissorRects(f.ctx, &cnt, g);
        CHECKF(cnt == n, "n=%u cnt=%u", n, cnt);
        for (UINT i = 0; i < n && i < cnt; i++) CHECKF(!memcmp(&r[i], &g[i], sizeof r[i]), "n=%u rect %u mismatch", n, i);
    }
    CLOSE();
}
TEST(d3d11_context_state_rs_scissors_zero_clears) {
    OPEN();
    D3D11_RECT r = { 0, 0, 8, 8 };
    ID3D11DeviceContext_RSSetScissorRects(f.ctx, 1, &r);
    ID3D11DeviceContext_RSSetScissorRects(f.ctx, 0, NULL);
    UINT q = 99; ID3D11DeviceContext_RSGetScissorRects(f.ctx, &q, NULL);
    EXPECT_EQ(q, 0);
    CLOSE();
}
TEST(d3d11_context_state_rs_viewport_float_values_preserved) {
    OPEN();
    D3D11_VIEWPORT v = { -10.5f, 3.25f, 0.5f, 1e6f, -1.0f, 2.0f }, g; UINT c = 1;
    ID3D11DeviceContext_RSSetViewports(f.ctx, 1, &v);
    ID3D11DeviceContext_RSGetViewports(f.ctx, &c, &g);
    CHECK(g.TopLeftX == v.TopLeftX); CHECK(g.TopLeftY == v.TopLeftY); CHECK(g.Width == v.Width);
    CHECK(g.Height == v.Height); CHECK(g.MinDepth == v.MinDepth); CHECK(g.MaxDepth == v.MaxDepth);
    CLOSE();
}

/* ---- SO ------------------------------------------------------------------------- */
TEST(d3d11_context_state_so_targets_roundtrip) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_10_0);
    ID3D11Buffer *b[4] = { 0 };
    for (int i = 0; i < 4; i++) { b[i] = cs_buf(&f, D3D11_BIND_STREAM_OUTPUT, 256); if (!b[i]) { t_skip("so buf"); goto out; } }
    UINT off[4] = { 0, 0, 0, 0 };
    for (UINT n = 1; n <= 4; n++) {
        ID3D11Buffer *g[4]; memset(g, 0x55, sizeof g);
        ID3D11DeviceContext_SOSetTargets(f.ctx, n, b, off);
        ID3D11DeviceContext_SOGetTargets(f.ctx, 4, g);
        for (UINT i = 0; i < 4; i++) {
            ID3D11Buffer *want = i < n ? b[i] : NULL;
            CHECKF(g[i] == want, "n=%u slot %u", n, i);
            SAFE_RELEASE(g[i]);
        }
    }
    ID3D11DeviceContext_SOSetTargets(f.ctx, 0, NULL, NULL);
    ID3D11Buffer *g[4]; memset(g, 0x55, sizeof g);
    ID3D11DeviceContext_SOGetTargets(f.ctx, 4, g);
    for (int i = 0; i < 4; i++) CHECKF(g[i] == NULL, "slot %d not unbound by SOSetTargets(0)", i);
out:
    ID3D11DeviceContext_ClearState(f.ctx);
    for (int i = 0; i < 4; i++) SAFE_RELEASE(b[i]);
    CLOSE();
}
TEST(d3d11_context_state_so_append_offset) {
    OPEN();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_STREAM_OUTPUT, 256);
    if (!b) { t_skip("so buf"); CLOSE(); return; }
    UINT off = (UINT)-1;
    ID3D11DeviceContext_SOSetTargets(f.ctx, 1, &b, &off);
    ID3D11Buffer *g = NULL;
    ID3D11DeviceContext_SOGetTargets(f.ctx, 1, &g);
    EXPECT_EQ(g, b);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_so_targets_unbind_refcount) {
    OPEN();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_STREAM_OUTPUT, 256);
    if (!b) { t_skip("so buf"); CLOSE(); return; }
    ULONG base = RC(b); UINT off = 0;
    ID3D11DeviceContext_SOSetTargets(f.ctx, 1, &b, &off);
    CHECK(RC(b) > base);
    ID3D11DeviceContext_SOSetTargets(f.ctx, 0, NULL, NULL);
    EXPECT_EQ(RC(b), base);
    SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_so_vs_ia_hazard) {
    OPEN();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_VERTEX_BUFFER, 256);
    if (!b) { t_skip("buf"); CLOSE(); return; }
    UINT s = 16, o = 0;
    ID3D11DeviceContext_IASetVertexBuffers(f.ctx, 0, 1, &b, &s, &o);
    ID3D11DeviceContext_SOSetTargets(f.ctx, 1, &b, &o);
    ID3D11Buffer *gv = NULL, *gs = NULL;
    ID3D11DeviceContext_IAGetVertexBuffers(f.ctx, 0, 1, &gv, NULL, NULL);
    ID3D11DeviceContext_SOGetTargets(f.ctx, 1, &gs);
    t_note("buffer bound as IA vertex buffer then SO target: IA slot=%p SO slot=%p (buffer %p)", gv, gs, b);
    CHECKF(!(gv == b && gs == b), "a buffer must not stay bound as both vertex buffer and SO target");
    SAFE_RELEASE(gv); SAFE_RELEASE(gs);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(b);
    CLOSE();
}

/* ---- read/write hazards -------------------------------------------------------- */
TEST(d3d11_context_state_hazard_rtv_set_after_srv) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11ShaderResourceView *srv = t ? cs_srv(&f, (ID3D11Resource *)t) : NULL;
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    if (!srv || !rtv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_PSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11ShaderResourceView *g = NULL;
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 0, 1, &g);
    EXPECT_EQ(g, srv); SAFE_RELEASE(g);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, NULL);
    g = (void *)1;
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 0, 1, &g);
    CHECKF(g == NULL, "SRV must be unbound when its resource becomes a render target (got %p)", g);
    SAFE_RELEASE(g);
    ID3D11RenderTargetView *gr = NULL;
    ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 1, &gr, NULL);
    EXPECT_EQ(gr, rtv); SAFE_RELEASE(gr);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_hazard_srv_set_after_rtv) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11ShaderResourceView *srv = t ? cs_srv(&f, (ID3D11Resource *)t) : NULL;
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    if (!srv || !rtv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, NULL);
    ID3D11DeviceContext_PSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11ShaderResourceView *g = (void *)1;
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 0, 1, &g);
    CHECKF(g == NULL, "binding an SRV of a bound render target must leave the slot NULL (got %p)", g);
    SAFE_RELEASE(g);
    ID3D11RenderTargetView *gr = NULL;
    ID3D11DeviceContext_OMGetRenderTargets(f.ctx, 1, &gr, NULL);
    EXPECT_EQ(gr, rtv); SAFE_RELEASE(gr);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_hazard_other_slots_untouched) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11Texture2D *u = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    ID3D11ShaderResourceView *sv = t ? cs_srv(&f, (ID3D11Resource *)t) : NULL, *su = u ? cs_srv(&f, (ID3D11Resource *)u) : NULL;
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    if (!sv || !su || !rtv) { t_skip("views"); goto out; }
    ID3D11ShaderResourceView *set[3] = { su, sv, su }, *g[3] = { 0 };
    ID3D11DeviceContext_PSSetShaderResources(f.ctx, 0, 3, set);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, NULL);
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 0, 3, g);
    EXPECT_EQ(g[0], su); CHECK(g[1] == NULL); EXPECT_EQ(g[2], su);
    SAFE_RELEASE(g[0]); SAFE_RELEASE(g[1]); SAFE_RELEASE(g[2]);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(sv); SAFE_RELEASE(su); SAFE_RELEASE(rtv); SAFE_RELEASE(t); SAFE_RELEASE(u);
    CLOSE();
}
TEST(d3d11_context_state_hazard_disjoint_mips_coexist) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 8, 8, 2, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *rtv = NULL;
    if (t) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd; memset(&sd, 0, sizeof sd);
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MostDetailedMip = 1; sd.Texture2D.MipLevels = 1;
        ID3D11Device_CreateShaderResourceView(f.dev, (ID3D11Resource *)t, &sd, &srv);
        D3D11_RENDER_TARGET_VIEW_DESC rd; memset(&rd, 0, sizeof rd);
        rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; rd.Texture2D.MipSlice = 0;
        rtv = cs_rtv(&f, (ID3D11Resource *)t, &rd);
    }
    if (!srv || !rtv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, NULL);
    ID3D11DeviceContext_PSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11ShaderResourceView *g = NULL;
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 0, 1, &g);
    CHECKF(g == srv, "SRV of mip 1 must stay bound next to an RTV of mip 0 (got %p)", g);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_hazard_overlapping_mip_range) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 8, 8, 2, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11ShaderResourceView *srv = t ? cs_srv(&f, (ID3D11Resource *)t) : NULL; /* all mips */
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL; /* mip 0 */
    if (!srv || !rtv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_PSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, NULL);
    ID3D11ShaderResourceView *g = (void *)1;
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 0, 1, &g);
    CHECKF(g == NULL, "an SRV covering the RTV's mip must be unbound (got %p)", g);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_hazard_depth_srv_vs_dsv) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_10_0);
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
    ID3D11ShaderResourceView *srv = NULL; ID3D11DepthStencilView *dsv = NULL;
    if (t) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd; memset(&sd, 0, sizeof sd);
        sd.Format = DXGI_FORMAT_R32_FLOAT; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
        ID3D11Device_CreateShaderResourceView(f.dev, (ID3D11Resource *)t, &sd, &srv);
        D3D11_DEPTH_STENCIL_VIEW_DESC dd; memset(&dd, 0, sizeof dd);
        dd.Format = DXGI_FORMAT_D32_FLOAT; dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        ID3D11Device_CreateDepthStencilView(f.dev, (ID3D11Resource *)t, &dd, &dsv);
    }
    if (!srv || !dsv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_PSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 0, NULL, dsv);
    ID3D11ShaderResourceView *g = (void *)1;
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 0, 1, &g);
    CHECKF(g == NULL, "SRV must be unbound when its resource becomes the writable depth buffer (got %p)", g);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(dsv); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_hazard_readonly_dsv_keeps_srv) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R32_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
    ID3D11ShaderResourceView *srv = NULL; ID3D11DepthStencilView *dsv = NULL;
    if (t) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd; memset(&sd, 0, sizeof sd);
        sd.Format = DXGI_FORMAT_R32_FLOAT; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
        ID3D11Device_CreateShaderResourceView(f.dev, (ID3D11Resource *)t, &sd, &srv);
        D3D11_DEPTH_STENCIL_VIEW_DESC dd; memset(&dd, 0, sizeof dd);
        dd.Format = DXGI_FORMAT_D32_FLOAT; dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D; dd.Flags = D3D11_DSV_READ_ONLY_DEPTH;
        ID3D11Device_CreateDepthStencilView(f.dev, (ID3D11Resource *)t, &dd, &dsv);
    }
    if (!srv || !dsv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 0, NULL, dsv);
    ID3D11DeviceContext_PSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11ShaderResourceView *g = NULL;
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 0, 1, &g);
    CHECKF(g == srv, "SRV must remain bound next to a read-only depth view (got %p)", g);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(dsv); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_hazard_vs_srv_observed) {
    OPEN();
    ID3D11Texture2D *t = cs_tex(&f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    ID3D11ShaderResourceView *srv = t ? cs_srv(&f, (ID3D11Resource *)t) : NULL;
    ID3D11RenderTargetView *rtv = t ? cs_rtv(&f, (ID3D11Resource *)t, NULL) : NULL;
    if (!srv || !rtv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_VSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11DeviceContext_GSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11DeviceContext_OMSetRenderTargets(f.ctx, 1, &rtv, NULL);
    ID3D11ShaderResourceView *gv = NULL, *gg = NULL;
    ID3D11DeviceContext_VSGetShaderResources(f.ctx, 0, 1, &gv);
    ID3D11DeviceContext_GSGetShaderResources(f.ctx, 0, 1, &gg);
    t_note("hazard in non-PS stages: VS slot=%p GS slot=%p (srv %p)", gv, gg, srv);
    SAFE_RELEASE(gv); SAFE_RELEASE(gg);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(rtv); SAFE_RELEASE(t);
    CLOSE();
}
