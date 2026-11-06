/* Contexts (immediate/deferred), threads, lifetimes, and WARP-deterministic execution of created shaders. */
#include "sh_common.h"

static const char *const stage_bind_vs[] = { "vs_pass", "ps_const", "gs_tri", "hs_tri_int", "ds_tri", "cs_basic" };

TEST(d3d11_shaders_deferred_context_type_and_device)
{
    SH_ENV(e, 0); ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK); if (!dc) return;
    EXPECT_EQ(ID3D11DeviceContext_GetType(dc), D3D11_DEVICE_CONTEXT_DEFERRED);
    EXPECT_EQ(ID3D11DeviceContext_GetType(e.ctx), D3D11_DEVICE_CONTEXT_IMMEDIATE);
    ID3D11Device *d = NULL; ID3D11DeviceContext_GetDevice(dc, &d); CHECK(d == e.dev); SH_REL(d);
    SH_REL(dc); sh_env_free(&e);
}
TEST(d3d11_shaders_deferred_bind_all_stages)
{
    SH_ENV(e, 0xb000); ID3D11DeviceContext *dc = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK); if (!dc) return;
    for (int st = 0; st < ST_COUNT; st++) {
        ID3DBlob *b = sh_blob(stage_bind_vs[st]); if (!b) continue;
        void *sh = NULL, *got = NULL;
        EXPECT_HR(sh_create(e.dev, st, SH_BP(b), SH_BN(b), NULL, &sh), S_OK); if (!sh) continue;
        sh_set(dc, st, sh, NULL, 0);
        sh_get(dc, st, &got, NULL, NULL);
        t_check(got == sh, __FILE__, __LINE__, "deferred context read back", "stage %s", sh_stage_name[st]); SH_REL(got);
        sh_get(e.ctx, st, &got, NULL, NULL);
        t_check(got == NULL, __FILE__, __LINE__, "immediate context unaffected by deferred bind", "stage %s", sh_stage_name[st]); SH_REL(got);
        sh_set(dc, st, NULL, NULL, 0);
        SH_REL(sh);
    }
    SH_REL(dc); sh_env_free(&e);
}
TEST(d3d11_shaders_deferred_finish_resets_state)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); ID3D11DeviceContext *dc = NULL; ID3D11VertexShader *vs = NULL, *got = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK); if (!dc) return;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(b), SH_BN(b), NULL, &vs), S_OK);
    ID3D11DeviceContext_VSSetShader(dc, vs, NULL, 0);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
    ID3D11DeviceContext_VSGetShader(dc, &got, NULL, NULL); CHECK(got == NULL); SH_REL(got);
    SH_REL(cl); SH_REL(vs); SH_REL(dc); sh_env_free(&e);
}
TEST(d3d11_shaders_deferred_finish_restore_keeps_state)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); ID3D11DeviceContext *dc = NULL; ID3D11VertexShader *vs = NULL, *got = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK); if (!dc) return;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(b), SH_BN(b), NULL, &vs), S_OK);
    ID3D11DeviceContext_VSSetShader(dc, vs, NULL, 0);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, TRUE, &cl), S_OK);
    ID3D11DeviceContext_VSGetShader(dc, &got, NULL, NULL); CHECK(got == vs); SH_REL(got);
    SH_REL(cl); ID3D11DeviceContext_VSSetShader(dc, NULL, NULL, 0); SH_REL(vs); SH_REL(dc); sh_env_free(&e);
}
TEST(d3d11_shaders_execute_command_list_restore_flag)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); ID3D11DeviceContext *dc = NULL; ID3D11VertexShader *vs = NULL, *got = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK); if (!dc) return;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(b), SH_BN(b), NULL, &vs), S_OK);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK); if (!cl) return;
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0);
    ID3D11DeviceContext_ExecuteCommandList(e.ctx, cl, TRUE);
    ID3D11DeviceContext_VSGetShader(e.ctx, &got, NULL, NULL); CHECK(got == vs); SH_REL(got);
    ID3D11DeviceContext_ExecuteCommandList(e.ctx, cl, FALSE);
    ID3D11DeviceContext_VSGetShader(e.ctx, &got, NULL, NULL); CHECK(got == NULL); SH_REL(got);
    SH_REL(cl); SH_REL(vs); SH_REL(dc); sh_env_free(&e);
}
TEST(d3d11_shaders_command_list_records_shader_binding)
{
    SH_ENV(e, 0); SH_BLOB(b, "ps_const"); ID3D11DeviceContext *dc = NULL; ID3D11PixelShader *ps = NULL, *got = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK); if (!dc) return;
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(b), SH_BN(b), NULL, &ps), S_OK);
    ID3D11DeviceContext_PSSetShader(dc, ps, NULL, 0);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
    ID3D11DeviceContext_ExecuteCommandList(e.ctx, cl, FALSE);
    ID3D11DeviceContext_PSGetShader(e.ctx, &got, NULL, NULL);
    CHECK(got == NULL);   /* Win10 22H2: ExecuteCommandList(FALSE) leaves the immediate context's PS binding reset */
    SH_REL(got); ID3D11DeviceContext_ClearState(e.ctx);
    SH_REL(cl); SH_REL(ps); SH_REL(dc); sh_env_free(&e);
}
TEST(d3d11_shaders_release_shader_after_command_list_recorded)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); ID3D11DeviceContext *dc = NULL; ID3D11VertexShader *vs = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK); if (!dc) return;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(b), SH_BN(b), NULL, &vs), S_OK);
    ID3D11DeviceContext_VSSetShader(dc, vs, NULL, 0);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
    EXPECT_EQ(d3dh_refcount(vs), 1);   /* the command list holds an internal reference only */
    SH_REL(vs); /* command list keeps it alive internally */
    ID3D11DeviceContext_ExecuteCommandList(e.ctx, cl, FALSE); /* must not crash */
    SH_REL(cl); SH_REL(dc); sh_env_free(&e);
}
TEST(d3d11_shaders_reuse_across_immediate_and_deferred)
{
    SH_ENV(e, 0xb000);
    ID3DBlob *b = sh_blob("cs_basic"); if (!b) return;
    ID3D11DeviceContext *dc = NULL; ID3D11ComputeShader *cs = NULL, *g1 = NULL, *g2 = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK); if (!dc) return;
    EXPECT_HR(ID3D11Device_CreateComputeShader(e.dev, SH_BP(b), SH_BN(b), NULL, &cs), S_OK);
    ID3D11DeviceContext_CSSetShader(e.ctx, cs, NULL, 0); ID3D11DeviceContext_CSSetShader(dc, cs, NULL, 0);
    ID3D11DeviceContext_CSGetShader(e.ctx, &g1, NULL, NULL); ID3D11DeviceContext_CSGetShader(dc, &g2, NULL, NULL);
    CHECK(g1 == cs && g2 == cs); SH_REL(g1); SH_REL(g2);
    ID3D11DeviceContext_CSSetShader(e.ctx, NULL, NULL, 0); ID3D11DeviceContext_CSSetShader(dc, NULL, NULL, 0);
    SH_REL(cs); SH_REL(dc); sh_env_free(&e);
}
TEST(d3d11_shaders_reuse_across_two_deferred_contexts)
{
    SH_ENV(e, 0); SH_BLOB(b, "ps_const"); ID3D11DeviceContext *a = NULL, *c = NULL; ID3D11PixelShader *ps = NULL, *g = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &a), S_OK); EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &c), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(b), SH_BN(b), NULL, &ps), S_OK);
    if (a && c && ps) {
        ID3D11DeviceContext_PSSetShader(a, ps, NULL, 0); ID3D11DeviceContext_PSSetShader(c, ps, NULL, 0);
        ID3D11DeviceContext_PSGetShader(a, &g, NULL, NULL); CHECK(g == ps); SH_REL(g);
        ID3D11DeviceContext_PSGetShader(c, &g, NULL, NULL); CHECK(g == ps); SH_REL(g);
    }
    SH_REL(ps); SH_REL(a); SH_REL(c); sh_env_free(&e);
}
TEST(d3d11_shaders_context_release_before_shader)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); ID3D11VertexShader *vs = NULL;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(b), SH_BN(b), NULL, &vs), S_OK);
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0);
    ID3D11DeviceContext_ClearState(e.ctx);
    SH_REL(e.ctx);
    EXPECT_EQ(d3dh_refcount(vs), 1);
    SH_REL(vs); SH_REL(e.dev);
}
TEST(d3d11_shaders_shader_outlives_app_device_reference)
{
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl;
    HRESULT hr = d3dh_make_device11(&dev, &ctx, &fl, NULL, 0);
    if (FAILED(hr)) { t_skip("no device"); return; }
    SH_BLOB(b, "vs_pass");
    ID3D11VertexShader *vs = NULL;
    EXPECT_HR(ID3D11Device_CreateVertexShader(dev, SH_BP(b), SH_BN(b), NULL, &vs), S_OK);
    ID3D11DeviceContext_ClearState(ctx); SH_REL(ctx); SH_REL(dev);
    ID3D11Device *d2 = NULL; ID3D11VertexShader_GetDevice(vs, &d2);
    CHECK(d2 != NULL);
    if (d2) { ID3D11VertexShader *vs2 = NULL; EXPECT_HR(ID3D11Device_CreateVertexShader(d2, SH_BP(b), SH_BN(b), NULL, &vs2), S_OK); SH_REL(vs2); SH_REL(d2); }
    SH_REL(vs);
}
TEST(d3d11_shaders_device_refcount_after_shader_creation_observed)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); ID3D11VertexShader *vs = NULL;
    ULONG before = d3dh_refcount(e.dev);
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(b), SH_BN(b), NULL, &vs), S_OK);
    ULONG after = d3dh_refcount(e.dev);
    EXPECT_EQ(after, before + 1);   /* a device child holds one public reference on its device */
    SH_REL(vs);
    sh_env_free(&e);
}

typedef struct { ID3D11Device *dev; const void *p; SIZE_T n; int ok, made; } thr_arg;
static DWORD WINAPI create_thread(LPVOID a)
{
    thr_arg *t = a;
    for (int i = 0; i < 40; i++) {
        ID3D11VertexShader *vs = NULL;
        if (SUCCEEDED(ID3D11Device_CreateVertexShader(t->dev, t->p, t->n, NULL, &vs)) && vs) { t->made++; IUnknown_Release((IUnknown *)vs); }
    }
    t->ok = 1; return 0;
}
TEST(d3d11_shaders_concurrent_creation_from_threads)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass");
    thr_arg a[4]; HANDLE h[4];
    for (int i = 0; i < 4; i++) { a[i].dev = e.dev; a[i].p = SH_BP(b); a[i].n = SH_BN(b); a[i].ok = a[i].made = 0; h[i] = CreateThread(NULL, 0, create_thread, &a[i], 0, NULL); }
    DWORD w = WaitForMultipleObjects(4, h, TRUE, 20000);
    CHECK(w == WAIT_OBJECT_0);
    if (w != WAIT_OBJECT_0) { t_note("threads did not finish"); return; }
    for (int i = 0; i < 4; i++) { EXPECT_EQ(a[i].made, 40); CloseHandle(h[i]); }
    sh_env_free(&e);
}

/* ---------------- execution on WARP ---------------- */
typedef struct { ID3D11Texture2D *tex; ID3D11RenderTargetView *rtv; ID3D11RasterizerState *rs; } rtctx;
static int rt_setup(SHENV *e, rtctx *r)
{
    D3D11_TEXTURE2D_DESC td = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
    D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd); rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    memset(r, 0, sizeof *r);
    if (FAILED(ID3D11Device_CreateTexture2D(e->dev, &td, NULL, &r->tex))) return 0;
    if (FAILED(ID3D11Device_CreateRenderTargetView(e->dev, (ID3D11Resource *)r->tex, NULL, &r->rtv))) return 0;
    if (FAILED(ID3D11Device_CreateRasterizerState(e->dev, &rd, &r->rs))) return 0;
    D3D11_VIEWPORT vp = { 0, 0, 4, 4, 0, 1 };
    static const float clear[4] = { 0, 0, 0, 0 };
    ID3D11DeviceContext_ClearRenderTargetView(e->ctx, r->rtv, clear);
    ID3D11DeviceContext_OMSetRenderTargets(e->ctx, 1, &r->rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(e->ctx, 1, &vp); ID3D11DeviceContext_RSSetState(e->ctx, r->rs);
    ID3D11DeviceContext_IASetPrimitiveTopology(e->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    return 1;
}
static void rt_free(rtctx *r) { SH_REL(r->rs); SH_REL(r->rtv); SH_REL(r->tex); }

TEST(d3d11_shaders_exec_const_pixel_shader_after_buffer_trashed)
{
    SH_ENV(e, 0); SH_BLOB(vb, "vs_noinput"); SH_BLOB(pb, "ps_const");
    unsigned char *copy = sh_dup(pb, 0);
    ID3D11VertexShader *vs = NULL; ID3D11PixelShader *ps = NULL; rtctx r;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &vs), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, copy, SH_BN(pb), NULL, &ps), S_OK);
    memset(copy, 0xee, SH_BN(pb)); free(copy);
    if (!vs || !ps || !rt_setup(&e, &r)) { CHECK(0); return; }
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0); ID3D11DeviceContext_PSSetShader(e.ctx, ps, NULL, 0);
    ID3D11DeviceContext_Draw(e.ctx, 3, 0);
    d3dh_image im;
    if (SUCCEEDED(d3dh_readback_texture2d11(e.dev, e.ctx, r.tex, 0, &im))) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) {
            UINT32 p = d3dh_image_pixel32(&im, x, y);
            t_check((p & 0xff) == 0x40 && ((p >> 8) & 0xff) >= 0x7f && ((p >> 8) & 0xff) <= 0x80 && ((p >> 16) & 0xff) == 0xbf && (p >> 24) == 0xff,
                    __FILE__, __LINE__, "constant colour (0.25,0.5,0.75,1)", "pixel(%u,%u)=0x%08x", x, y, p);
        }
        d3dh_image_free(&im);
    } else CHECK(0);
    rt_free(&r); SH_REL(vs); SH_REL(ps); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_pixel_shader_replaced_between_draws)
{
    SH_ENV(e, 0); SH_BLOB(vb, "vs_noinput"); SH_BLOB(pb, "ps_const"); SH_BLOB(p2, "ps_noinput");
    ID3D11VertexShader *vs = NULL; ID3D11PixelShader *ps = NULL, *ps2 = NULL; rtctx r;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &vs), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(pb), SH_BN(pb), NULL, &ps), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(p2), SH_BN(p2), NULL, &ps2), S_OK);
    if (!vs || !ps || !ps2 || !rt_setup(&e, &r)) { CHECK(0); return; }
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(e.ctx, ps, NULL, 0); ID3D11DeviceContext_Draw(e.ctx, 3, 0);
    ID3D11DeviceContext_PSSetShader(e.ctx, ps2, NULL, 0); ID3D11DeviceContext_Draw(e.ctx, 3, 0);
    d3dh_image im;
    if (SUCCEEDED(d3dh_readback_texture2d11(e.dev, e.ctx, r.tex, 0, &im))) {
        t_check(d3dh_image_pixel32(&im, 1, 1) == 0xff0000ffu, __FILE__, __LINE__, "second shader wins", "0x%08x", d3dh_image_pixel32(&im, 1, 1));
        d3dh_image_free(&im);
    } else CHECK(0);
    rt_free(&r); SH_REL(vs); SH_REL(ps); SH_REL(ps2); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_no_pixel_shader_leaves_target_untouched)
{
    SH_ENV(e, 0); SH_BLOB(vb, "vs_noinput"); ID3D11VertexShader *vs = NULL; rtctx r;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &vs), S_OK);
    if (!vs || !rt_setup(&e, &r)) { CHECK(0); return; }
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0); ID3D11DeviceContext_PSSetShader(e.ctx, NULL, NULL, 0);
    ID3D11DeviceContext_Draw(e.ctx, 3, 0);
    d3dh_image im;
    if (SUCCEEDED(d3dh_readback_texture2d11(e.dev, e.ctx, r.tex, 0, &im))) {
        t_check(d3dh_image_pixel32(&im, 2, 2) == 0, __FILE__, __LINE__, "no PS: no colour written", "0x%08x", d3dh_image_pixel32(&im, 2, 2));
        d3dh_image_free(&im);
    } else CHECK(0);
    rt_free(&r); SH_REL(vs); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_vertex_id_pixel_discard)
{
    SH_ENV(e, 0); SH_BLOB(vb, "vs_noinput"); SH_BLOB(pb, "ps_discard");
    ID3D11VertexShader *vs = NULL; ID3D11PixelShader *ps = NULL; rtctx r;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &vs), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(pb), SH_BN(pb), NULL, &ps), S_OK);
    if (!vs || !ps || !rt_setup(&e, &r)) { CHECK(0); return; }
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0); ID3D11DeviceContext_PSSetShader(e.ctx, ps, NULL, 0);
    ID3D11DeviceContext_Draw(e.ctx, 3, 0);
    d3dh_image im;
    if (SUCCEEDED(d3dh_readback_texture2d11(e.dev, e.ctx, r.tex, 0, &im))) {
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) {
            UINT32 want = x < 2 ? 0 : 0xffffffffu; /* SV_Position.x = x + 0.5 */
            t_check(d3dh_image_pixel32(&im, x, y) == want, __FILE__, __LINE__, "discard for x < 2", "pixel(%u,%u)=0x%08x", x, y, d3dh_image_pixel32(&im, x, y));
        }
        d3dh_image_free(&im);
    } else CHECK(0);
    rt_free(&r); SH_REL(vs); SH_REL(ps); sh_env_free(&e);
}

/* compute helpers */
typedef enum { UB_RAW, UB_STRUCT, UB_TYPED_UINT, UB_APPEND } ubkind;
static ID3D11Buffer *mk_ubuf(SHENV *e, UINT nuints, ubkind k, ID3D11UnorderedAccessView **uav)
{
    D3D11_BUFFER_DESC bd = { nuints * 4, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS, 0, 0, 4 };
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
    ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = nuints;
    if (k == UB_RAW) { bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS; bd.StructureByteStride = 0; ud.Format = DXGI_FORMAT_R32_TYPELESS; ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW; }
    else if (k == UB_STRUCT) { bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; ud.Format = DXGI_FORMAT_UNKNOWN; }
    else if (k == UB_APPEND) { bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; ud.Format = DXGI_FORMAT_UNKNOWN; ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND; }
    else { bd.StructureByteStride = 0; ud.Format = DXGI_FORMAT_R32_UINT; }
    ID3D11Buffer *b = NULL; *uav = NULL;
    if (FAILED(ID3D11Device_CreateBuffer(e->dev, &bd, NULL, &b))) return NULL;
    if (FAILED(ID3D11Device_CreateUnorderedAccessView(e->dev, (ID3D11Resource *)b, &ud, uav))) { SH_REL(b); return NULL; }
    return b;
}
static UINT *cs_exec(SHENV *e, const char *name, UINT nuints, ubkind k, UINT gx, UINT gy, UINT gz, size_t *outsz)
{
    ID3DBlob *cb = sh_blob(name); if (!cb) return NULL;
    ID3D11ComputeShader *cs = NULL; ID3D11UnorderedAccessView *uav = NULL; UINT *res = NULL;
    if (FAILED(ID3D11Device_CreateComputeShader(e->dev, SH_BP(cb), SH_BN(cb), NULL, &cs))) { CHECKF(0, "create %s", name); return NULL; }
    ID3D11Buffer *buf = mk_ubuf(e, nuints, k, &uav);
    if (buf) {
        UINT init = 0;
        ID3D11DeviceContext_CSSetShader(e->ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetUnorderedAccessViews(e->ctx, 0, 1, &uav, &init);
        ID3D11DeviceContext_Dispatch(e->ctx, gx, gy, gz);
        ID3D11UnorderedAccessView *nul = NULL; ID3D11DeviceContext_CSSetUnorderedAccessViews(e->ctx, 0, 1, &nul, &init);
        ID3D11DeviceContext_CSSetShader(e->ctx, NULL, NULL, 0);
        void *data = NULL; size_t sz = 0;
        if (SUCCEEDED(d3dh_readback_buffer11(e->dev, e->ctx, buf, &data, &sz))) { res = data; if (outsz) *outsz = sz; }
    }
    SH_REL(uav); SH_REL(buf); SH_REL(cs);
    return res;
}
TEST(d3d11_shaders_exec_cs_raw_store_thread_ids)
{
    SH_ENV(e, 0xb000); size_t sz; UINT *r = cs_exec(&e, "cs_basic", 16, UB_RAW, 2, 1, 1, &sz);
    if (!r) { CHECK(0); return; }
    for (UINT i = 0; i < 16; i++) t_check(r[i] == i, __FILE__, __LINE__, "raw UAV thread id", "[%u]=%u", i, r[i]);
    free(r); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_4_1_structured_store)
{
    SH_ENV(e, 0xa100); size_t sz; UINT *r = cs_exec(&e, "cs_41", 8, UB_STRUCT, 1, 1, 1, &sz);
    if (!r) { CHECK(0); return; }
    for (UINT i = 0; i < 8; i++) t_check(r[i] == i * 3, __FILE__, __LINE__, "structured UAV", "[%u]=%u", i, r[i]);
    free(r); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_4_0_raw_store_on_fl11)
{
    SH_ENV(e, 0xa000); size_t sz; UINT *r = cs_exec(&e, "cs_40", 8, UB_RAW, 1, 1, 1, &sz);
    if (!r) { CHECK(0); return; }
    for (UINT i = 0; i < 8; i++) t_check(r[i] == i, __FILE__, __LINE__, "cs_4_0 raw UAV", "[%u]=%u", i, r[i]);
    free(r); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_groupshared_reverse)
{
    SH_ENV(e, 0xb000); size_t sz; UINT *r = cs_exec(&e, "cs_shared", 128, UB_STRUCT, 2, 1, 1, &sz);
    if (!r) { CHECK(0); return; }
    for (UINT g = 0; g < 2; g++) for (UINT t = 0; t < 64; t++)
        t_check(r[g * 64 + t] == g * 64 + 63 - t, __FILE__, __LINE__, "groupshared reversal", "[%u]=%u", g * 64 + t, r[g * 64 + t]);
    free(r); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_atomics)
{
    SH_ENV(e, 0xb000); size_t sz; UINT *r = cs_exec(&e, "cs_atomic", 4, UB_STRUCT, 2, 1, 1, &sz);
    if (!r) { CHECK(0); return; }
    EXPECT_EQ(r[0], 64); EXPECT_EQ(r[1], 63);
    free(r); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_1024_threads)
{
    SH_ENV(e, 0xb000); size_t sz; UINT *r = cs_exec(&e, "cs_1024", 1024, UB_STRUCT, 1, 1, 1, &sz);
    if (!r) { CHECK(0); return; }
    int bad = 0; for (UINT i = 0; i < 1024; i++) if (r[i] != i) bad++;
    EXPECT_EQ(bad, 0); free(r); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_3d_group_index)
{
    SH_ENV(e, 0xb000); size_t sz; UINT *r = cs_exec(&e, "cs_3d", 64, UB_STRUCT, 1, 1, 1, &sz);
    if (!r) { CHECK(0); return; }
    int bad = 0; for (UINT i = 0; i < 64; i++) if (r[i] != i) bad++;
    EXPECT_EQ(bad, 0); free(r); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_dispatch_zero_groups_writes_nothing)
{
    SH_ENV(e, 0xb000); size_t sz; UINT *r = cs_exec(&e, "cs_basic", 8, UB_RAW, 0, 1, 1, &sz);
    if (!r) { CHECK(0); return; }
    for (UINT i = 0; i < 8; i++) EXPECT_EQ(r[i], 0);
    free(r); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_append_counter)
{
    SH_ENV(e, 0xb000); SH_BLOB(cb, "cs_append");
    ID3D11ComputeShader *cs = NULL; ID3D11UnorderedAccessView *uav = NULL; ID3D11Buffer *cnt = NULL;
    EXPECT_HR(ID3D11Device_CreateComputeShader(e.dev, SH_BP(cb), SH_BN(cb), NULL, &cs), S_OK);
    ID3D11Buffer *buf = mk_ubuf(&e, 16, UB_APPEND, &uav);
    D3D11_BUFFER_DESC cd = { 16, D3D11_USAGE_DEFAULT, 0, 0, 0, 0 };
    EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &cd, NULL, &cnt), S_OK);
    if (!cs || !buf || !cnt) { CHECK(0); return; }
    UINT zero = 0;
    ID3D11DeviceContext_CSSetShader(e.ctx, cs, NULL, 0);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(e.ctx, 0, 1, &uav, &zero);
    ID3D11DeviceContext_Dispatch(e.ctx, 1, 1, 1);
    ID3D11DeviceContext_CopyStructureCount(e.ctx, cnt, 0, uav);
    ID3D11UnorderedAccessView *nul = NULL; ID3D11DeviceContext_CSSetUnorderedAccessViews(e.ctx, 0, 1, &nul, &zero);
    void *d = NULL; size_t sz = 0;
    if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, cnt, &d, &sz))) { EXPECT_EQ(((UINT *)d)[0], 16); free(d); } else CHECK(0);
    if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, buf, &d, &sz))) {
        UINT seen = 0; for (UINT i = 0; i < 16; i++) { UINT v = ((UINT *)d)[i]; if (v < 16) seen |= 1u << v; }
        EXPECT_EQ(seen, 0xffff); free(d);
    } else CHECK(0);
    ID3D11DeviceContext_CSSetShader(e.ctx, NULL, NULL, 0);
    SH_REL(cnt); SH_REL(uav); SH_REL(buf); SH_REL(cs); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_constant_buffer_param)
{
    SH_ENV(e, 0xb000); SH_BLOB(cb, "cs_cb");
    ID3D11ComputeShader *cs = NULL; ID3D11UnorderedAccessView *uav = NULL; ID3D11Buffer *cbuf = NULL;
    EXPECT_HR(ID3D11Device_CreateComputeShader(e.dev, SH_BP(cb), SH_BN(cb), NULL, &cs), S_OK);
    ID3D11Buffer *buf = mk_ubuf(&e, 4, UB_STRUCT, &uav);
    UINT params[4] = { 40, 2, 0, 0 };
    D3D11_BUFFER_DESC cd = { 16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 }; D3D11_SUBRESOURCE_DATA sd = { params, 0, 0 };
    EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &cd, &sd, &cbuf), S_OK);
    if (!cs || !buf || !cbuf) { CHECK(0); return; }
    UINT init = 0;
    ID3D11DeviceContext_CSSetShader(e.ctx, cs, NULL, 0); ID3D11DeviceContext_CSSetConstantBuffers(e.ctx, 0, 1, &cbuf);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(e.ctx, 0, 1, &uav, &init);
    ID3D11DeviceContext_Dispatch(e.ctx, 1, 1, 1);
    void *d = NULL; size_t sz = 0;
    if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, buf, &d, &sz))) { EXPECT_EQ(((UINT *)d)[0], 42); free(d); } else CHECK(0);
    ID3D11DeviceContext_ClearState(e.ctx);
    SH_REL(cbuf); SH_REL(uav); SH_REL(buf); SH_REL(cs); sh_env_free(&e);
}
TEST(d3d11_shaders_exec_cs_dispatch_on_deferred_context)
{
    SH_ENV(e, 0xb000); SH_BLOB(cb, "cs_basic");
    ID3D11DeviceContext *dc = NULL; ID3D11ComputeShader *cs = NULL; ID3D11UnorderedAccessView *uav = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateDeferredContext(e.dev, 0, &dc), S_OK);
    EXPECT_HR(ID3D11Device_CreateComputeShader(e.dev, SH_BP(cb), SH_BN(cb), NULL, &cs), S_OK);
    ID3D11Buffer *buf = mk_ubuf(&e, 8, UB_RAW, &uav);
    if (!dc || !cs || !buf) { CHECK(0); return; }
    UINT init = 0;
    ID3D11DeviceContext_CSSetShader(dc, cs, NULL, 0); ID3D11DeviceContext_CSSetUnorderedAccessViews(dc, 0, 1, &uav, &init);
    ID3D11DeviceContext_Dispatch(dc, 1, 1, 1);
    EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
    void *d = NULL; size_t sz = 0;
    if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, buf, &d, &sz))) { EXPECT_EQ(((UINT *)d)[7], 0); free(d); }
    ID3D11DeviceContext_ExecuteCommandList(e.ctx, cl, FALSE);
    if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, buf, &d, &sz))) { for (UINT i = 0; i < 8; i++) EXPECT_EQ(((UINT *)d)[i], i); free(d); } else CHECK(0);
    SH_REL(cl); SH_REL(uav); SH_REL(buf); SH_REL(cs); SH_REL(dc); sh_env_free(&e);
}
