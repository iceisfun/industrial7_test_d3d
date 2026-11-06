/* Binding / lifetime semantics of state objects on immediate and deferred contexts: reference counts, ClearState, per-stage sampler slots.
 * Values measured on Win10 22H2 (WARP, feature level 11_0). */
#include "so_common.h"

ST(life_bound_state_holds_no_public_reference) {
    ID3D11BlendState *b = NULL, *g = NULL; D3D11_BLEND_DESC bd = so_blend_default(); FLOAT f[4] = { .1f, .2f, .3f, .4f }, gf[4] = { 9, 9, 9, 9 }; UINT gm = 0;
    SO_NEED();
    ID3D11DeviceContext_ClearState(so_ctx);
    bd.AlphaToCoverageEnable = TRUE;
    EXPECT_HR(SO_CBLEND(&bd, &b), S_OK);
    if (!b) return;
    EXPECT_EQ(so_addref_count(b), 1);
    ID3D11DeviceContext_OMSetBlendState(so_ctx, b, f, 0x1234);
    EXPECT_EQ(so_addref_count(b), 1);                     /* binding is an internal reference: the COM count does not change */
    ID3D11DeviceContext_OMGetBlendState(so_ctx, &g, gf, &gm);
    CHECK(g == b);
    EXPECT_EQ(so_addref_count(b), 2);                     /* ... but Get hands out a counted reference */
    CHECK(gf[0] == .1f && gf[1] == .2f && gf[2] == .3f && gf[3] == .4f);
    EXPECT_EQ(gm, 0x1234);
    SAFE_RELEASE(g);
    EXPECT_EQ(so_addref_count(b), 1);
    ID3D11DeviceContext_ClearState(so_ctx);
    EXPECT_EQ(so_addref_count(b), 1);
    gm = 5; g = SO_SENTINEL(ID3D11BlendState);
    ID3D11DeviceContext_OMGetBlendState(so_ctx, &g, gf, &gm);
    CHECK(g == NULL);                                      /* ClearState: default state reported as NULL */
    CHECK(gf[0] == 1.f && gf[1] == 1.f && gf[2] == 1.f && gf[3] == 1.f);
    EXPECT_EQ(gm, 0xffffffff);
    SAFE_RELEASE(b);
}
ST(life_bound_state_survives_last_app_release) {
    ID3D11BlendState *b = NULL, *g = NULL; D3D11_BLEND_DESC bd = so_blend_default();
    SO_NEED();
    ID3D11DeviceContext_ClearState(so_ctx);
    bd.AlphaToCoverageEnable = TRUE;
    EXPECT_HR(SO_CBLEND(&bd, &b), S_OK);
    if (!b) return;
    ID3D11DeviceContext_OMSetBlendState(so_ctx, b, NULL, 0xffffffff);
    EXPECT_EQ(ID3D11BlendState_Release(b), 0);             /* the app's last reference goes away while the context still has it bound ... */
    ID3D11DeviceContext_OMGetBlendState(so_ctx, &g, NULL, NULL);
    CHECK(g == b);                                         /* ... the same object is still there, its count starts again at 1 */
    if (g) { EXPECT_EQ(so_addref_count(g), 1); ID3D11BlendState_Release(g); }
    ID3D11DeviceContext_ClearState(so_ctx);
}
ST(life_depth_stencil_ref_roundtrip_and_clear) {
    ID3D11DepthStencilState *ds = NULL, *g = NULL; D3D11_DEPTH_STENCIL_DESC dd = so_ds_default(); UINT sr = 0;
    SO_NEED();
    ID3D11DeviceContext_ClearState(so_ctx);
    dd.DepthFunc = D3D11_COMPARISON_GREATER;
    EXPECT_HR(SO_CDS(&dd, &ds), S_OK);
    if (!ds) return;
    ID3D11DeviceContext_OMSetDepthStencilState(so_ctx, ds, 77);
    ID3D11DeviceContext_OMGetDepthStencilState(so_ctx, &g, &sr);
    CHECK(g == ds); EXPECT_EQ(sr, 77); EXPECT_EQ(so_addref_count(ds), 2);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(so_ctx);
    sr = 5; g = SO_SENTINEL(ID3D11DepthStencilState);
    ID3D11DeviceContext_OMGetDepthStencilState(so_ctx, &g, &sr);
    CHECK(g == NULL); EXPECT_EQ(sr, 0);                     /* ClearState resets the stencil reference to 0 */
    ID3D11DeviceContext_OMSetDepthStencilState(so_ctx, NULL, 9);
    ID3D11DeviceContext_OMGetDepthStencilState(so_ctx, &g, &sr);
    CHECK(g == NULL); EXPECT_EQ(sr, 9);                     /* a NULL state still stores the reference */
    ID3D11DeviceContext_ClearState(so_ctx);
    SAFE_RELEASE(ds);
}
ST(life_blend_factor_defaults_when_null) {
    FLOAT f[4] = { 7, 7, 7, 7 }, gf[4] = { 0 }; UINT m = 0;
    ID3D11BlendState *g = NULL;
    SO_NEED();
    ID3D11DeviceContext_ClearState(so_ctx);
    ID3D11DeviceContext_OMSetBlendState(so_ctx, NULL, f, 0x55);
    ID3D11DeviceContext_OMGetBlendState(so_ctx, &g, gf, &m);
    CHECK(gf[0] == 7.f && gf[1] == 7.f && gf[2] == 7.f && gf[3] == 7.f); EXPECT_EQ(m, 0x55);
    ID3D11DeviceContext_OMSetBlendState(so_ctx, NULL, NULL, 0x55);       /* NULL factor: reset to 1,1,1,1 (not "keep the old value") */
    ID3D11DeviceContext_OMGetBlendState(so_ctx, &g, gf, &m);
    CHECK(gf[0] == 1.f && gf[1] == 1.f && gf[2] == 1.f && gf[3] == 1.f); EXPECT_EQ(m, 0x55);
    ID3D11DeviceContext_ClearState(so_ctx);
}
static void set_samplers(int st, UINT start, UINT n, ID3D11SamplerState *const *p) {
    switch (st) {
    case 0: ID3D11DeviceContext_VSSetSamplers(so_ctx, start, n, p); break; case 1: ID3D11DeviceContext_PSSetSamplers(so_ctx, start, n, p); break;
    case 2: ID3D11DeviceContext_GSSetSamplers(so_ctx, start, n, p); break; case 3: ID3D11DeviceContext_HSSetSamplers(so_ctx, start, n, p); break;
    case 4: ID3D11DeviceContext_DSSetSamplers(so_ctx, start, n, p); break; default: ID3D11DeviceContext_CSSetSamplers(so_ctx, start, n, p); break; }
}
static void get_samplers(int st, UINT start, UINT n, ID3D11SamplerState **p) {
    switch (st) {
    case 0: ID3D11DeviceContext_VSGetSamplers(so_ctx, start, n, p); break; case 1: ID3D11DeviceContext_PSGetSamplers(so_ctx, start, n, p); break;
    case 2: ID3D11DeviceContext_GSGetSamplers(so_ctx, start, n, p); break; case 3: ID3D11DeviceContext_HSGetSamplers(so_ctx, start, n, p); break;
    case 4: ID3D11DeviceContext_DSGetSamplers(so_ctx, start, n, p); break; default: ID3D11DeviceContext_CSGetSamplers(so_ctx, start, n, p); break; }
}
ST(life_sampler_slots_on_every_stage) {
    ID3D11SamplerState *s[2] = { NULL, NULL }, *got[16]; D3D11_SAMPLER_DESC sd = so_samp_default();
    static const char *nm[] = { "VS", "PS", "GS", "HS", "DS", "CS" };
    int st, i;
    SO_NEED();
    ID3D11DeviceContext_ClearState(so_ctx);
    SO_CSAMP(&sd, &s[0]); sd.MaxLOD = 5; SO_CSAMP(&sd, &s[1]);
    if (!s[0] || !s[1]) { SAFE_RELEASE(s[0]); SAFE_RELEASE(s[1]); return; }
    for (st = 0; st < 6; st++) {
        set_samplers(st, 3, 2, s);
        memset(got, 0xAA, sizeof got);
        get_samplers(st, 0, 8, got);
        CHECKF(got[0] == NULL && got[1] == NULL && got[2] == NULL, "%s: untouched slots 0..2 must read back NULL", nm[st]);
        CHECKF(got[3] == s[0] && got[4] == s[1], "%s: slots 3 and 4 read back what was set", nm[st]);
        CHECKF(got[5] == NULL && got[7] == NULL, "%s: slots after the range stay NULL", nm[st]);
        EXPECT_EQ(so_addref_count(s[0]), 2);                  /* the Get reference only */
        for (i = 0; i < 8; i++) SAFE_RELEASE(got[i]);
        ID3D11DeviceContext_ClearState(so_ctx);
        EXPECT_EQ(so_addref_count(s[0]), 1);
    }
    /* the last two slots (14, 15) and clearing a range with NULLs */
    set_samplers(1, 15, 1, s);
    memset(got, 0, sizeof got); get_samplers(1, 15, 1, got); CHECK(got[0] == s[0]); SAFE_RELEASE(got[0]);
    set_samplers(1, 14, 2, s);
    memset(got, 0, sizeof got); get_samplers(1, 14, 2, got); CHECK(got[0] == s[0] && got[1] == s[1]); SAFE_RELEASE(got[0]); SAFE_RELEASE(got[1]);
    { ID3D11SamplerState *nn[2] = { NULL, NULL }; set_samplers(1, 14, 2, nn); }
    memset(got, 0xAA, sizeof got); get_samplers(1, 14, 2, got); CHECK(got[0] == NULL && got[1] == NULL);
    memset(got, 0xAA, sizeof got); get_samplers(1, 0, 16, got);
    for (i = 0; i < 16; i++) CHECK(got[i] == NULL);
    ID3D11DeviceContext_ClearState(so_ctx);
    SAFE_RELEASE(s[0]); SAFE_RELEASE(s[1]);
}
ST(life_deferred_context_state_is_separate) {
    ID3D11DeviceContext *dc = NULL, *bad = SO_SENTINEL(ID3D11DeviceContext);
    ID3D11BlendState *db = NULL, *x = NULL; D3D11_BLEND_DESC bd = so_blend_default(); FLOAT f[4] = { .1f, .2f, .3f, .4f }, xf[4]; UINT xm;
    ID3D11CommandList *cl = NULL;
    SO_NEED();
    ID3D11DeviceContext_ClearState(so_ctx);
    EXPECT_HR(ID3D11Device_CreateDeferredContext(so_dev, 0, NULL), S_FALSE);          /* validation only */
    EXPECT_HR(ID3D11Device_CreateDeferredContext(so_dev, 1, &bad), E_INVALIDARG);     /* ContextFlags must be 0 */
    EXPECT_HR(ID3D11Device_CreateDeferredContext(so_dev, 0, &dc), S_OK);
    if (!dc) return;
    EXPECT_EQ((int)ID3D11DeviceContext_GetType(dc), (int)D3D11_DEVICE_CONTEXT_DEFERRED);
    EXPECT_EQ(ID3D11DeviceContext_GetContextFlags(dc), 0);
    bd.AlphaToCoverageEnable = TRUE;
    EXPECT_HR(SO_CBLEND(&bd, &db), S_OK);
    if (!db) { SAFE_RELEASE(dc); return; }
    ID3D11DeviceContext_OMSetBlendState(dc, db, f, 0xabcd);
    ID3D11DeviceContext_OMGetBlendState(so_ctx, &x, xf, &xm);
    CHECK(x == NULL);                                                                  /* the immediate context is unaffected */
    ID3D11DeviceContext_OMGetBlendState(dc, &x, xf, &xm); CHECK(x == db); EXPECT_EQ(xm, 0xabcd); SAFE_RELEASE(x);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
    ID3D11DeviceContext_OMGetBlendState(dc, &x, xf, &xm);
    CHECK(x == NULL && xf[0] == 1.f && xm == 0xffffffff);                              /* FinishCommandList(FALSE) resets the deferred context */
    SAFE_RELEASE(x);
    if (cl) {
        ID3D11DeviceContext_OMSetBlendState(so_ctx, db, f, 0x77);
        ID3D11DeviceContext_ExecuteCommandList(so_ctx, cl, FALSE);
        ID3D11DeviceContext_OMGetBlendState(so_ctx, &x, xf, &xm);
        CHECK(x == NULL); EXPECT_EQ(xm, 0xffffffff);                                   /* Execute(RestoreContextState = FALSE) leaves the immediate context cleared */
        SAFE_RELEASE(x);
        ID3D11DeviceContext_OMSetBlendState(so_ctx, db, f, 0x77);
        ID3D11DeviceContext_ExecuteCommandList(so_ctx, cl, TRUE);
        ID3D11DeviceContext_OMGetBlendState(so_ctx, &x, xf, &xm);
        CHECK(x == db); EXPECT_EQ(xm, 0x77);                                           /* ... and TRUE restores what was bound */
        SAFE_RELEASE(x);
        EXPECT_EQ(so_addref_count(cl), 1);                                             /* executing does not consume the list */
        SAFE_RELEASE(cl);
    }
    ID3D11DeviceContext_ClearState(so_ctx);
    SAFE_RELEASE(db); SAFE_RELEASE(dc);
}
