/* D3D9 render target / depth surface binding rules, scenes, Clear validation, state blocks, queries. */
#include "d3dh.h"
#include "d9sd.h"

/* ---- scene -------------------------------------------------------------------------- */
TEST(d3d9_state_draw_scene_end_without_begin)
{
    SD_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3DERR_INVALIDCALL);
    SD_END(d);
}

TEST(d3d9_state_draw_scene_begin_end_pair)
{
    SD_OPEN(d);
    for (int i = 0; i < 3; i++) {
        EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);
    }
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3DERR_INVALIDCALL);
    SD_END(d);
}

TEST(d3d9_state_draw_scene_nested_begin)
{
    SD_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);        /* the failed nested call did not add a level */
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3DERR_INVALIDCALL);
    SD_END(d);
}

TEST(d3d9_state_draw_scene_reset_clears_scene)
{
    SD_OPEN(d);
    D3DPRESENT_PARAMETERS pp;
    memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = SD_W; pp.BackBufferHeight = SD_H; pp.hDeviceWindow = d.hwnd;
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
    HRESULT hr = IDirect3DDevice9_Reset(d.dev, &pp);
    SD_NEED(d, SUCCEEDED(hr), "Reset failed");
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);
    SD_END(d);
}

TEST(d3d9_state_draw_scene_not_affected_by_set_render_target)
{
    SD_OPEN(d);
    IDirect3DSurface9 *rt = NULL, *orig = NULL;
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(d.dev, 16, 16, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create render target");
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &orig);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, rt), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, orig), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);
    SAFE_RELEASE(orig);
    SAFE_RELEASE(rt);
    SD_END(d);
}

TEST(d3d9_state_draw_scene_while_recording_stateblock)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    SAFE_RELEASE(sb);
    /* EndScene is refused while recording even when a scene is open */
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_scene_clear_allowed_outside_and_inside)
{
    SD_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff102030, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff102030, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);
    SD_END(d);
}

/* ---- Clear validation ------------------------------------------------------------------ */
TEST(d3d9_state_draw_clear_flags_validation)
{
    SD_OPEN_DS(d);
    static const DWORD bad[] = { 0, 8, 0x10, 0x20, 0x80, 0x100, 0x80000000u, 0xfffffff8u };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDirect3DDevice9_Clear(d.dev, 0, NULL, bad[i], 0, 1.0f, 0);
        t_check(hr == D3DERR_INVALIDCALL, __FILE__, __LINE__, "Clear(flags without TARGET/ZBUFFER/STENCIL)", "flags 0x%lx got %s", (unsigned long)bad[i], d3dh_hr_name(hr));
    }
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0), D3D_OK);
    if (d.dsfmt == D3DFMT_D24S8) {
        EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0), D3D_OK);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_clear_zbuffer_without_depth_surface)
{
    SD_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    SD_END(d);
}

TEST(d3d9_state_draw_clear_after_unbinding_depth_surface)
{
    SD_OPEN_DS(d);
    IDirect3DSurface9 *ds = NULL;
    EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &ds), D3D_OK);
    CHECK(ds != NULL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, NULL), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    /* unbinding left ZENABLE alone */
    DWORD z = 0;
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ZENABLE, &z), D3D_OK);
    EXPECT_EQ(z, D3DZB_TRUE);
    IDirect3DSurface9 *g = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &g), D3DERR_NOTFOUND);
    CHECK(g == NULL);
    EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, ds), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0), D3D_OK);
    SAFE_RELEASE(ds);
    SD_END(d);
}

TEST(d3d9_state_draw_clear_stencil_needs_stencil_format)
{
    SD_OPEN(d);
    IDirect3DSurface9 *ds = NULL;
    HRESULT hr = IDirect3DDevice9_CreateDepthStencilSurface(d.dev, SD_W, SD_H, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, FALSE, &ds, NULL);
    SD_NEED(d, SUCCEEDED(hr), "no D16 depth surface");
    EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, ds), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0), D3D_OK);
    IDirect3DDevice9_SetDepthStencilSurface(d.dev, NULL);
    SAFE_RELEASE(ds);
    SD_END(d);
}

TEST(d3d9_state_draw_clear_rect_count_limit)
{
    SD_OPEN(d);
    D3DRECT *r = calloc(5000, sizeof *r);
    SD_NEED(d, r != NULL, "out of memory");
    for (int i = 0; i < 5000; i++) { r[i].x1 = 0; r[i].y1 = 0; r[i].x2 = 1; r[i].y2 = 1; }
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 4097, r, D3DCLEAR_TARGET, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 5000, r, D3DCLEAR_TARGET, 0, 1.0f, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 4096, r, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 1, r, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    free(r);
    SD_END(d);
}

TEST(d3d9_state_draw_clear_rects_clipped_and_empty)
{
    SD_OPEN(d);
    D3DRECT inside = { 4, 4, 20, 20 };
    D3DRECT outside = { -10, -10, 200, 200 };
    D3DRECT empty = { 10, 10, 10, 10 };
    D3DRECT inverted = { 30, 30, 20, 20 };
    D3DRECT offscreen = { 100, 100, 120, 120 };
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 1, &inside, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
    HRESULT hr = IDirect3DDevice9_Clear(d.dev, 1, &outside, D3DCLEAR_TARGET, 0, 1.0f, 0);
    t_note("Clear with a rectangle larger than the target -> %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9_Clear(d.dev, 1, &empty, D3DCLEAR_TARGET, 0, 1.0f, 0);
    t_note("Clear with an empty rectangle -> %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9_Clear(d.dev, 1, &inverted, D3DCLEAR_TARGET, 0, 1.0f, 0);
    t_note("Clear with an inverted rectangle -> %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9_Clear(d.dev, 1, &offscreen, D3DCLEAR_TARGET, 0, 1.0f, 0);
    t_note("Clear with an off-target rectangle -> %s", d3dh_hr_name(hr));
    SD_END(d);
}

TEST(d3d9_state_draw_clear_count_rect_pointer_pairing)
{
    SD_OPEN(d);
    D3DRECT r = { 0, 0, 8, 8 };
    HRESULT h1 = IDirect3DDevice9_Clear(d.dev, 0, &r, D3DCLEAR_TARGET, 0, 1.0f, 0);
    HRESULT h2 = IDirect3DDevice9_Clear(d.dev, 1, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
    t_note("Clear(Count=0, pRects!=NULL) -> %s; Clear(Count=1, pRects=NULL) -> %s", d3dh_hr_name(h1), d3dh_hr_name(h2));
    SD_END(d);
}

TEST(d3d9_state_draw_clear_color_exact)
{
    SD_OPEN_R(d);
    SDRT rt;
    if (!sd_rt_make(&d, &rt, 32, 32, D3DFMT_A8R8G8B8, 1)) { SD_END(d); return; }
    static const DWORD cols[] = { 0xff336699u, 0x80ff0000u, 0x00000000u, 0xffffffffu, 0x12345678u };
    DWORD px[32 * 32];
    for (unsigned c = 0; c < sizeof cols / sizeof cols[0]; c++) {
        EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, cols[c], 1.0f, 0), D3D_OK);
        if (!sd_rt_read(&d, &rt, px)) break;
        int bad = 0;
        for (int i = 0; i < 32 * 32; i++) if (px[i] != cols[c]) bad++;
        CHECKF(bad == 0, "clear colour 0x%08lx: %d pixels differ (first 0x%08lx)", (unsigned long)cols[c], bad, (unsigned long)px[0]);
    }
    sd_rt_free(&d, &rt);
    SD_END(d);
}

TEST(d3d9_state_draw_clear_rects_partial)
{
    SD_OPEN_R(d);
    SDRT rt;
    if (!sd_rt_make(&d, &rt, 32, 32, D3DFMT_A8R8G8B8, 1)) { SD_END(d); return; }
    DWORD px[32 * 32];
    IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
    D3DRECT r[2] = { { 2, 3, 10, 12 }, { 20, 20, 30, 28 } };
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 2, r, D3DCLEAR_TARGET, 0xffff8000u, 1.0f, 0), D3D_OK);
    if (sd_rt_read(&d, &rt, px)) {
        int in0 = 0, in1 = 0, out = 0;
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++) {
                DWORD c = SD_PX(px, 32, x, y);
                int i0 = x >= 2 && x < 10 && y >= 3 && y < 12, i1 = x >= 20 && x < 30 && y >= 20 && y < 28;
                if (i0) in0 += c == 0xffff8000u;
                else if (i1) in1 += c == 0xffff8000u;
                else out += c != 0xff000000u;
            }
        CHECKF(in0 == 8 * 9, "first rectangle: %d of %d pixels cleared", in0, 72);
        CHECKF(in1 == 10 * 8, "second rectangle: %d of %d pixels cleared", in1, 80);
        CHECKF(out == 0, "%d pixels outside the rectangles were modified", out);
    }
    sd_rt_free(&d, &rt);
    SD_END(d);
}

TEST(d3d9_state_draw_clear_respects_viewport_not_scissor)
{
    SD_OPEN_R(d);
    SDRT rt;
    if (!sd_rt_make(&d, &rt, 32, 32, D3DFMT_A8R8G8B8, 1)) { SD_END(d); return; }
    DWORD px[32 * 32];
    IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
    D3DVIEWPORT9 vp = { 8, 8, 16, 16, 0.0f, 1.0f };
    IDirect3DDevice9_SetViewport(d.dev, &vp);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff00ff00u, 1.0f, 0), D3D_OK);
    if (sd_rt_read(&d, &rt, px)) {
        CHECK(SD_PX(px, 32, 16, 16) == 0xff00ff00u);
        CHECK(SD_PX(px, 32, 2, 2) == 0xff000000u);
        CHECK(SD_PX(px, 32, 30, 30) == 0xff000000u);
        CHECK(SD_PX(px, 32, 8, 8) == 0xff00ff00u);
        CHECK(SD_PX(px, 32, 23, 23) == 0xff00ff00u);
        CHECK(SD_PX(px, 32, 24, 24) == 0xff000000u);
        CHECK(SD_PX(px, 32, 7, 16) == 0xff000000u);
    }
    /* the scissor rectangle only matters when D3DRS_SCISSORTESTENABLE is on */
    D3DVIEWPORT9 full = { 0, 0, 32, 32, 0.0f, 1.0f };
    IDirect3DDevice9_SetViewport(d.dev, &full);
    IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
    RECT sc = { 4, 4, 12, 12 };
    IDirect3DDevice9_SetScissorRect(d.dev, &sc);
    IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff0000ffu, 1.0f, 0);
    if (sd_rt_read(&d, &rt, px)) CHECK(SD_PX(px, 32, 20, 20) == 0xff0000ffu);
    if (d.caps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST) {
        IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
        IDirect3DDevice9_SetRenderState(d.dev, D3DRS_SCISSORTESTENABLE, TRUE);
        IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff0000ffu, 1.0f, 0);
        IDirect3DDevice9_SetRenderState(d.dev, D3DRS_SCISSORTESTENABLE, FALSE);
        if (sd_rt_read(&d, &rt, px)) {
            CHECK(SD_PX(px, 32, 8, 8) == 0xff0000ffu);
            CHECK(SD_PX(px, 32, 20, 20) == 0xff000000u);
        }
    }
    sd_rt_free(&d, &rt);
    SD_END(d);
}

/* ---- render targets and depth surfaces ------------------------------------------------------ */
TEST(d3d9_state_draw_rt_get_default)
{
    SD_OPEN(d);
    IDirect3DSurface9 *rt = NULL;
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 0, &rt), D3D_OK);
    CHECK(rt != NULL);
    if (rt) {
        D3DSURFACE_DESC desc;
        EXPECT_HR(IDirect3DSurface9_GetDesc(rt, &desc), D3D_OK);
        EXPECT_EQ(desc.Width, SD_W); EXPECT_EQ(desc.Height, SD_H);
        EXPECT_EQ(desc.Type, D3DRTYPE_SURFACE);
        CHECK(desc.Usage & D3DUSAGE_RENDERTARGET);
        EXPECT_EQ(desc.Pool, D3DPOOL_DEFAULT);
        IDirect3DSurface9 *bb = NULL;
        EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3D_OK);
        CHECK(bb == rt);
        SAFE_RELEASE(bb);
    }
    SAFE_RELEASE(rt);
    SD_END(d);
}

TEST(d3d9_state_draw_rt_index_rules)
{
    SD_OPEN(d);
    DWORD n = d.caps.NumSimultaneousRTs;
    t_note("NumSimultaneousRTs=%lu", (unsigned long)n);
    CHECK(n >= 1);
    IDirect3DSurface9 *rt = NULL, *cur = NULL;
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(d.dev, SD_W, SD_H, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create render target");
    /* slot 0 may never be empty */
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, NULL), D3DERR_INVALIDCALL);
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &cur);
    CHECK(cur != NULL);
    SAFE_RELEASE(cur);
    /* slots beyond the cap are invalid */
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, n, rt), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 100, rt), D3DERR_INVALIDCALL);
    for (DWORD i = 1; i < n; i++) {
        cur = rt;
        EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, i, &cur), D3DERR_NOTFOUND);
        CHECKF(cur == NULL, "unbound slot %lu returned a surface", (unsigned long)i);
        EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, i, NULL), D3D_OK);
    }
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, n, &cur), D3DERR_INVALIDCALL);
    SAFE_RELEASE(rt);
    SD_END(d);
}

TEST(d3d9_state_draw_rt_multiple_targets)
{
    SD_OPEN_R(d);
    SD_NEED(d, d.caps.NumSimultaneousRTs >= 2, "no multiple render targets");
    IDirect3DSurface9 *a = NULL, *b = NULL, *orig = NULL, *g = NULL;
    HRESULT h1 = IDirect3DDevice9_CreateRenderTarget(d.dev, SD_W, SD_H, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &a, NULL);
    HRESULT h2 = IDirect3DDevice9_CreateRenderTarget(d.dev, SD_W, SD_H, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &b, NULL);
    if (FAILED(h1) || FAILED(h2)) { SAFE_RELEASE(a); SAFE_RELEASE(b); SD_NEED(d, 0, "cannot create render targets"); }
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &orig);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, a), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 1, b), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 1, &g), D3D_OK);
    CHECK(g == b);
    SAFE_RELEASE(g);
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 0, &g), D3D_OK);
    CHECK(g == a);
    SAFE_RELEASE(g);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 1, NULL), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 1, &g), D3DERR_NOTFOUND);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, orig), D3D_OK);
    SAFE_RELEASE(orig);
    SAFE_RELEASE(a);
    SAFE_RELEASE(b);
    SD_END(d);
}

TEST(d3d9_state_draw_rt_requires_render_target_usage)
{
    SD_OPEN(d);
    IDirect3DSurface9 *plain = NULL;
    HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(d.dev, SD_W, SD_H, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &plain, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create offscreen plain surface");
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, plain), D3DERR_INVALIDCALL);
    SAFE_RELEASE(plain);
    IDirect3DTexture9 *tex = NULL;
    hr = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL);
    if (SUCCEEDED(hr)) {
        IDirect3DSurface9 *lvl = NULL;
        IDirect3DTexture9_GetSurfaceLevel(tex, 0, &lvl);
        EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, lvl), D3DERR_INVALIDCALL);
        SAFE_RELEASE(lvl);
        SAFE_RELEASE(tex);
    }
    IDirect3DSurface9 *ds = NULL;
    hr = IDirect3DDevice9_CreateDepthStencilSurface(d.dev, SD_W, SD_H, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, FALSE, &ds, NULL);
    if (SUCCEEDED(hr)) {
        EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, ds), D3DERR_INVALIDCALL);
        SAFE_RELEASE(ds);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_rt_render_target_texture)
{
    SD_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 16, 8, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create render target texture");
    IDirect3DSurface9 *lvl = NULL, *orig = NULL, *g = NULL;
    IDirect3DTexture9_GetSurfaceLevel(tex, 0, &lvl);
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &orig);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, lvl), D3D_OK);
    D3DVIEWPORT9 vp;
    IDirect3DDevice9_GetViewport(d.dev, &vp);
    EXPECT_EQ(vp.Width, 16); EXPECT_EQ(vp.Height, 8);
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &g);
    CHECK(g == lvl);
    SAFE_RELEASE(g);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, orig), D3D_OK);
    SAFE_RELEASE(orig);
    SAFE_RELEASE(lvl);
    SAFE_RELEASE(tex);
    SD_END(d);
}

TEST(d3d9_state_draw_rt_set_during_stateblock_recording_is_immediate)
{
    SD_OPEN(d);
    IDirect3DSurface9 *rt = NULL, *orig = NULL, *g = NULL;
    IDirect3DStateBlock9 *sb = NULL;
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(d.dev, 32, 32, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create render target");
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &orig);
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, rt), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &g);
    CHECK(g == rt);
    SAFE_RELEASE(g);
    D3DVIEWPORT9 vp;
    IDirect3DDevice9_GetViewport(d.dev, &vp);
    EXPECT_EQ(vp.Width, 32);
    IDirect3DDevice9_SetRenderTarget(d.dev, 0, orig);
    SAFE_RELEASE(sb); SAFE_RELEASE(orig); SAFE_RELEASE(rt);
    SD_END(d);
}

TEST(d3d9_state_draw_ds_get_set)
{
    SD_OPEN_DS(d);
    IDirect3DSurface9 *auto_ds = NULL, *mine = NULL, *g = NULL;
    EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &auto_ds), D3D_OK);
    CHECK(auto_ds != NULL);
    if (auto_ds) {
        D3DSURFACE_DESC desc;
        IDirect3DSurface9_GetDesc(auto_ds, &desc);
        EXPECT_EQ(desc.Width, SD_W); EXPECT_EQ(desc.Height, SD_H);
        EXPECT_EQ(desc.Format, d.dsfmt);
        CHECK(desc.Usage & D3DUSAGE_DEPTHSTENCIL);
    }
    HRESULT hr = IDirect3DDevice9_CreateDepthStencilSurface(d.dev, SD_W, SD_H, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, FALSE, &mine, NULL);
    if (SUCCEEDED(hr)) {
        EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, mine), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &g), D3D_OK);
        CHECK(g == mine);
        SAFE_RELEASE(g);
        EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, auto_ds), D3D_OK);
    }
    SAFE_RELEASE(mine);
    SAFE_RELEASE(auto_ds);
    SD_END(d);
}

TEST(d3d9_state_draw_ds_rejects_non_depth_surface)
{
    SD_OPEN_DS(d);
    IDirect3DSurface9 *rt = NULL;
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(d.dev, SD_W, SD_H, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create render target");
    EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, rt), D3DERR_INVALIDCALL);
    IDirect3DSurface9 *g = NULL;
    EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &g), D3D_OK);
    CHECK(g != NULL);
    SAFE_RELEASE(g);
    SAFE_RELEASE(rt);
    SD_END(d);
}

TEST(d3d9_state_draw_ds_none_by_default_without_auto)
{
    SD_OPEN(d);
    IDirect3DSurface9 *g = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &g), D3DERR_NOTFOUND);
    CHECK(g == NULL);
    EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(d.dev, NULL), D3D_OK);
    SD_END(d);
}

/* ---- state blocks ------------------------------------------------------------------------------ */
TEST(d3d9_state_draw_sb_create_types)
{
    SD_OPEN(d);
    static const D3DSTATEBLOCKTYPE ok_t[] = { D3DSBT_ALL, D3DSBT_PIXELSTATE, D3DSBT_VERTEXSTATE };
    for (int i = 0; i < 3; i++) {
        IDirect3DStateBlock9 *sb = NULL;
        EXPECT_HR(IDirect3DDevice9_CreateStateBlock(d.dev, ok_t[i], &sb), D3D_OK);
        CHECK(sb != NULL);
        if (sb) {
            IDirect3DDevice9 *dev = NULL;
            EXPECT_HR(IDirect3DStateBlock9_GetDevice(sb, &dev), D3D_OK);
            CHECK(dev == d.dev);
            SAFE_RELEASE(dev);
        }
        SAFE_RELEASE(sb);
    }
    static const int bad[] = { 0, 4, 5, 100, 0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDirect3DStateBlock9 *sb = (void *)(ULONG_PTR)0xdeadbeef;
        HRESULT hr = IDirect3DDevice9_CreateStateBlock(d.dev, (D3DSTATEBLOCKTYPE)bad[i], &sb);
        t_check(hr == D3DERR_INVALIDCALL, __FILE__, __LINE__, "CreateStateBlock(invalid type)", "type %d got %s", bad[i], d3dh_hr_name(hr));
    }
    SD_END(d);
}

static const D3DMATRIX g_id = {{{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }}};

TEST(d3d9_state_draw_sb_all_capture_and_apply)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    D3DMATRIX m = {{{ 2,0,0,0, 0,3,0,0, 0,0,4,0, 5,6,7,1 }}}, g;
    D3DVIEWPORT9 vp = { 1, 2, 30, 40, 0.1f, 0.9f }, gv;
    DWORD v;
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_TEXTUREFACTOR, 0x11223344);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetTransform(d.dev, D3DTS_VIEW, &m);
    IDirect3DDevice9_SetViewport(d.dev, &vp);
    IDirect3DDevice9_SetTextureStageState(d.dev, 2, D3DTSS_CONSTANT, 0xabcdef01);
    IDirect3DDevice9_SetSamplerState(d.dev, 3, D3DSAMP_BORDERCOLOR, 0x01020304);
    EXPECT_HR(IDirect3DDevice9_CreateStateBlock(d.dev, D3DSBT_ALL, &sb), D3D_OK);
    SD_NEED(d, sb != NULL, "no state block");
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_CW);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_TEXTUREFACTOR, 0);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetTransform(d.dev, D3DTS_VIEW, &g_id);
    D3DVIEWPORT9 vp2 = { 0, 0, 10, 10, 0.0f, 1.0f };
    IDirect3DDevice9_SetViewport(d.dev, &vp2);
    IDirect3DDevice9_SetTextureStageState(d.dev, 2, D3DTSS_CONSTANT, 0);
    IDirect3DDevice9_SetSamplerState(d.dev, 3, D3DSAMP_BORDERCOLOR, 0);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v);      EXPECT_EQ(v, D3DCULL_NONE);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_TEXTUREFACTOR, &v); EXPECT_EQ(v, 0x11223344);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_LIGHTING, &v);      EXPECT_EQ(v, FALSE);
    IDirect3DDevice9_GetTransform(d.dev, D3DTS_VIEW, &g);            CHECK(!memcmp(&g, &m, sizeof g));
    IDirect3DDevice9_GetViewport(d.dev, &gv);                        EXPECT_EQ(gv.X, 1); EXPECT_EQ(gv.Height, 40);
    IDirect3DDevice9_GetTextureStageState(d.dev, 2, D3DTSS_CONSTANT, &v); EXPECT_EQ(v, 0xabcdef01);
    IDirect3DDevice9_GetSamplerState(d.dev, 3, D3DSAMP_BORDERCOLOR, &v);  EXPECT_EQ(v, 0x01020304);
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_pixel_state_scope)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    DWORD v;
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ALPHAREF, 0x40);            /* pixel state */
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_TEXTUREFACTOR, 0x55555555); /* pixel state */
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_NONE);    /* vertex state */
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_AMBIENT, 0x00ff00ff);       /* vertex state */
    EXPECT_HR(IDirect3DDevice9_CreateStateBlock(d.dev, D3DSBT_PIXELSTATE, &sb), D3D_OK);
    SD_NEED(d, sb != NULL, "no state block");
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ALPHAREF, 0x99);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_TEXTUREFACTOR, 0x01010101);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_CW);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_AMBIENT, 0x12345678);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ALPHAREF, &v);       EXPECT_EQ(v, 0x40);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_TEXTUREFACTOR, &v);  EXPECT_EQ(v, 0x55555555);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v);       t_note("pixel block: CULLMODE after apply=%lu (docs: vertex state)", (unsigned long)v);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_AMBIENT, &v);        EXPECT_EQ(v, 0x12345678);
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_vertex_state_scope)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    DWORD v;
    float c1[4] = { 1, 2, 3, 4 }, c2[4] = { 9, 9, 9, 9 }, g[4];
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_CW);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ALPHAREF, 0x33);
    IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 20, c1, 1);
    IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 20, c1, 1);
    IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, 4);
    EXPECT_HR(IDirect3DDevice9_CreateStateBlock(d.dev, D3DSBT_VERTEXSTATE, &sb), D3D_OK);
    SD_NEED(d, sb != NULL, "no state block");
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_CCW);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ALPHAREF, 0x77);
    IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 20, c2, 1);
    IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 20, c2, 1);
    IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, 8);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_LIGHTING, &v);  EXPECT_EQ(v, FALSE);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v);  t_note("vertex block: CULLMODE after apply=%lu (docs: vertex state)", (unsigned long)v);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ALPHAREF, &v);  EXPECT_EQ(v, 0x77);
    IDirect3DDevice9_GetVertexShaderConstantF(d.dev, 20, g, 1);  CHECK(!memcmp(g, c1, sizeof g));
    IDirect3DDevice9_GetPixelShaderConstantF(d.dev, 20, g, 1);   CHECK(!memcmp(g, c2, sizeof g));
    UINT f = 0;
    IDirect3DDevice9_GetStreamSourceFreq(d.dev, 1, &f);          EXPECT_EQ(f, 4);
    IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, 1);
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_capture_updates_block)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    DWORD v;
    EXPECT_HR(IDirect3DDevice9_CreateStateBlock(d.dev, D3DSBT_ALL, &sb), D3D_OK);
    SD_NEED(d, sb != NULL, "no state block");
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_FOGCOLOR, 0x00aabbcc);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_STENCILREF, 7);
    EXPECT_HR(IDirect3DStateBlock9_Capture(sb), D3D_OK);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_FOGCOLOR, 0);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_STENCILREF, 1);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_FOGCOLOR, &v);   EXPECT_EQ(v, 0x00aabbcc);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_STENCILREF, &v); EXPECT_EQ(v, 7);
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_recorded_applies_only_recorded)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    DWORD v;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ALPHAREF, 77), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetTextureStageState(d.dev, 1, D3DTSS_COLOROP, D3DTOP_ADD), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetSamplerState(d.dev, 2, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    SD_NEED(d, sb != NULL, "no state block");
    /* recording does not change the live state */
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ALPHAREF, &v);              EXPECT_EQ(v, 0);
    IDirect3DDevice9_GetTextureStageState(d.dev, 1, D3DTSS_COLOROP, &v);     EXPECT_EQ(v, D3DTOP_DISABLE);
    IDirect3DDevice9_GetSamplerState(d.dev, 2, D3DSAMP_ADDRESSU, &v);        EXPECT_EQ(v, D3DTADDRESS_WRAP);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_NONE);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ALPHAREF, &v);              EXPECT_EQ(v, 77);
    IDirect3DDevice9_GetTextureStageState(d.dev, 1, D3DTSS_COLOROP, &v);     EXPECT_EQ(v, D3DTOP_ADD);
    IDirect3DDevice9_GetSamplerState(d.dev, 2, D3DSAMP_ADDRESSU, &v);        EXPECT_EQ(v, D3DTADDRESS_MIRROR);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v);              EXPECT_EQ(v, D3DCULL_NONE);    /* not recorded: untouched */
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_begin_end_protocol)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = (void *)(ULONG_PTR)0xdeadbeef, *sb2 = (void *)(ULONG_PTR)0xdeadbeef, *sb3 = NULL;
    DWORD v;
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3DERR_INVALIDCALL);       /* no Begin */
    CHECK(sb == (void *)(ULONG_PTR)0xdeadbeef);
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    sb = (void *)(ULONG_PTR)0xdeadbeef;
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    CHECK(sb != NULL && sb != (void *)(ULONG_PTR)0xdeadbeef);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb2), D3DERR_INVALIDCALL);      /* already ended */
    CHECK(sb2 == (void *)(ULONG_PTR)0xdeadbeef);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_LIGHTING, &v);                      /* recording did not change it */
    EXPECT_EQ(v, TRUE);
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3DERR_INVALIDCALL);          /* nested */
    if (sb) {
        EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3DERR_INVALIDCALL);               /* no Apply/Capture while recording */
        EXPECT_HR(IDirect3DStateBlock9_Capture(sb), D3DERR_INVALIDCALL);
    }
    EXPECT_HR(IDirect3DDevice9_CreateStateBlock(d.dev, D3DSBT_ALL, &sb3), D3DERR_INVALIDCALL);
    CHECK(sb3 == NULL);
    IDirect3DDevice9_GetRenderState(d.dev, D3DRS_LIGHTING, &v);
    EXPECT_EQ(v, TRUE);
    sb2 = NULL;
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb2), D3D_OK);
    if (sb2) {
        EXPECT_HR(IDirect3DStateBlock9_Apply(sb2), D3D_OK);                          /* an empty block changes nothing */
        IDirect3DDevice9_GetRenderState(d.dev, D3DRS_LIGHTING, &v);
        EXPECT_EQ(v, TRUE);
    }
    if (sb) {
        EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
        IDirect3DDevice9_GetRenderState(d.dev, D3DRS_LIGHTING, &v);
        EXPECT_EQ(v, FALSE);
    }
    SAFE_RELEASE(sb);
    SAFE_RELEASE(sb2);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_apply_is_repeatable)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    DWORD v;
    IDirect3DDevice9_BeginStateBlock(d.dev);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ZFUNC, D3DCMP_NOTEQUAL);
    IDirect3DDevice9_EndStateBlock(d.dev, &sb);
    SD_NEED(d, sb != NULL, "no state block");
    for (int i = 0; i < 3; i++) {
        IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ZFUNC, D3DCMP_LESS);
        EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
        IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ZFUNC, &v);
        EXPECT_EQ(v, D3DCMP_NOTEQUAL);
    }
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_texture_recorded)
{
    SD_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create texture");
    IDirect3DStateBlock9 *sb = NULL;
    IDirect3DBaseTexture9 *g = NULL;
    IDirect3DDevice9_BeginStateBlock(d.dev);
    EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, 4, (IDirect3DBaseTexture9 *)tex), D3D_OK);
    IDirect3DDevice9_EndStateBlock(d.dev, &sb);
    IDirect3DDevice9_GetTexture(d.dev, 4, &g);
    CHECK(g == NULL);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetTexture(d.dev, 4, &g);
    CHECK(g == (IDirect3DBaseTexture9 *)tex);
    SAFE_RELEASE(g);
    IDirect3DDevice9_SetTexture(d.dev, 4, NULL);
    SAFE_RELEASE(sb);
    SAFE_RELEASE(tex);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_stream_source_recorded)
{
    SD_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL, *g = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexBuffer(d.dev, 256, 0, 0, D3DPOOL_DEFAULT, &vb, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create vertex buffer");
    IDirect3DStateBlock9 *sb = NULL;
    UINT off, stride;
    IDirect3DDevice9_BeginStateBlock(d.dev);
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 2, vb, 8, 24), D3D_OK);
    IDirect3DDevice9_EndStateBlock(d.dev, &sb);
    IDirect3DDevice9_GetStreamSource(d.dev, 2, &g, &off, &stride);
    CHECK(g == NULL);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetStreamSource(d.dev, 2, &g, &off, &stride);
    CHECK(g == vb); EXPECT_EQ(off, 8); EXPECT_EQ(stride, 24);
    SAFE_RELEASE(g);
    IDirect3DDevice9_SetStreamSource(d.dev, 2, NULL, 0, 0);
    SAFE_RELEASE(sb);
    SAFE_RELEASE(vb);
    SD_END(d);
}

TEST(d3d9_state_draw_sb_survives_after_many_applies_and_release)
{
    SD_OPEN(d);
    for (int i = 0; i < 50; i++) {
        IDirect3DStateBlock9 *sb = NULL;
        EXPECT_HR(IDirect3DDevice9_CreateStateBlock(d.dev, (D3DSTATEBLOCKTYPE)(1 + i % 3), &sb), D3D_OK);
        if (sb) { IDirect3DStateBlock9_Apply(sb); IDirect3DStateBlock9_Release(sb); }
    }
    DWORD v;
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v), D3D_OK);
    EXPECT_EQ(v, D3DCULL_CCW);
    SD_END(d);
}

/* ---- queries ---------------------------------------------------------------------------------- */
static const D3DQUERYTYPE g_qtypes[] = {
    D3DQUERYTYPE_VCACHE, D3DQUERYTYPE_RESOURCEMANAGER, D3DQUERYTYPE_VERTEXSTATS, D3DQUERYTYPE_EVENT, D3DQUERYTYPE_OCCLUSION,
    D3DQUERYTYPE_TIMESTAMP, D3DQUERYTYPE_TIMESTAMPDISJOINT, D3DQUERYTYPE_TIMESTAMPFREQ, D3DQUERYTYPE_PIPELINETIMINGS,
    D3DQUERYTYPE_INTERFACETIMINGS, D3DQUERYTYPE_VERTEXTIMINGS, D3DQUERYTYPE_PIXELTIMINGS, D3DQUERYTYPE_BANDWIDTHTIMINGS,
    D3DQUERYTYPE_CACHEUTILIZATION,
};

TEST(d3d9_state_draw_query_support_probe_consistent)
{
    SD_OPEN(d);
    for (unsigned i = 0; i < sizeof g_qtypes / sizeof g_qtypes[0]; i++) {
        IDirect3DQuery9 *q = NULL;
        HRESULT probe = IDirect3DDevice9_CreateQuery(d.dev, g_qtypes[i], NULL);
        HRESULT real = IDirect3DDevice9_CreateQuery(d.dev, g_qtypes[i], &q);
        t_check(probe == D3D_OK || probe == D3DERR_NOTAVAILABLE, __FILE__, __LINE__, "CreateQuery(NULL) probe", "type %d got %s", (int)g_qtypes[i], d3dh_hr_name(probe));
        t_check(real == D3D_OK || real == D3DERR_NOTAVAILABLE, __FILE__, __LINE__, "CreateQuery", "type %d got %s", (int)g_qtypes[i], d3dh_hr_name(real));
        t_check((probe == D3D_OK) == (q != NULL), __FILE__, __LINE__, "probe agrees with creation", "type %d probe %s query %p", (int)g_qtypes[i], d3dh_hr_name(probe), (void *)q);
        t_note("query type %d: %s", (int)g_qtypes[i], probe == D3D_OK ? "supported" : "not available");
        SAFE_RELEASE(q);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_query_invalid_types)
{
    SD_OPEN(d);
    for (int t = 0; t < 40; t++) {
        if ((t >= D3DQUERYTYPE_VCACHE && t <= D3DQUERYTYPE_MEMORYPRESSURE) || t == 0x16) continue;   /* 0x16 exists on Windows 10 */
        IDirect3DQuery9 *q = (void *)(ULONG_PTR)0xdeadbeef;
        HRESULT h1 = IDirect3DDevice9_CreateQuery(d.dev, (D3DQUERYTYPE)t, NULL);
        HRESULT h2 = IDirect3DDevice9_CreateQuery(d.dev, (D3DQUERYTYPE)t, &q);
        t_check(h1 == D3DERR_NOTAVAILABLE && h2 == D3DERR_NOTAVAILABLE && q == (void *)(ULONG_PTR)0xdeadbeef, __FILE__, __LINE__, "CreateQuery(invalid type)",
                "type %d: probe %s create %s query %p", t, d3dh_hr_name(h1), d3dh_hr_name(h2), (void *)q);
    }
    IDirect3DQuery9 *q = NULL;
    HRESULT hr = IDirect3DDevice9_CreateQuery(d.dev, (D3DQUERYTYPE)0x16, NULL);
    t_note("undocumented query type 0x16 probe: %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9_CreateQuery(d.dev, (D3DQUERYTYPE)0x16, &q);
    t_note("undocumented query type 0x16 create: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(q);
    SD_END(d);
}

static IDirect3DQuery9 *mkq(SD *d, D3DQUERYTYPE t)
{
    IDirect3DQuery9 *q = NULL;
    HRESULT hr = IDirect3DDevice9_CreateQuery(d->dev, t, &q);
    if (FAILED(hr)) return NULL;
    return q;
}

TEST(d3d9_state_draw_query_type_and_size)
{
    SD_OPEN(d);
    static const struct { D3DQUERYTYPE t; DWORD size; } t[] = {
        { D3DQUERYTYPE_EVENT, sizeof(BOOL) }, { D3DQUERYTYPE_OCCLUSION, sizeof(DWORD) }, { D3DQUERYTYPE_TIMESTAMP, sizeof(UINT64) },
        { D3DQUERYTYPE_TIMESTAMPDISJOINT, sizeof(BOOL) }, { D3DQUERYTYPE_TIMESTAMPFREQ, sizeof(UINT64) },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDirect3DQuery9 *q = mkq(&d, t[i].t);
        if (!q) { t_note("query type %d unavailable", (int)t[i].t); continue; }
        EXPECT_EQ(IDirect3DQuery9_GetType(q), t[i].t);
        DWORD sz = IDirect3DQuery9_GetDataSize(q);
        t_check(sz == t[i].size, __FILE__, __LINE__, "GetDataSize", "type %d got %lu want %lu", (int)t[i].t, (unsigned long)sz, (unsigned long)t[i].size);
        IDirect3DDevice9 *dev = NULL;
        EXPECT_HR(IDirect3DQuery9_GetDevice(q, &dev), D3D_OK);
        CHECK(dev == d.dev);
        SAFE_RELEASE(dev);
        IDirect3DQuery9_Release(q);
    }
    for (unsigned i = 0; i < sizeof g_qtypes / sizeof g_qtypes[0]; i++) {
        IDirect3DQuery9 *q = mkq(&d, g_qtypes[i]);
        if (!q) continue;
        t_note("query type %d: GetType %d GetDataSize %lu", (int)g_qtypes[i], (int)IDirect3DQuery9_GetType(q), (unsigned long)IDirect3DQuery9_GetDataSize(q));
        EXPECT_EQ(IDirect3DQuery9_GetType(q), g_qtypes[i]);
        IDirect3DQuery9_Release(q);
    }
    SD_END(d);
}

static HRESULT wait_query(IDirect3DQuery9 *q, void *data, DWORD size)
{
    DWORD start = GetTickCount();
    HRESULT hr;
    while ((hr = IDirect3DQuery9_GetData(q, data, size, D3DGETDATA_FLUSH)) == S_FALSE) {
        if (GetTickCount() - start > 5000) break;
        Sleep(1);
    }
    return hr;
}

TEST(d3d9_state_draw_query_event_flow)
{
    SD_OPEN(d);
    IDirect3DQuery9 *q = mkq(&d, D3DQUERYTYPE_EVENT);
    SD_NEED(d, q != NULL, "no event query");
    BOOL b = 77;
    EXPECT_HR(IDirect3DQuery9_Issue(q, D3DISSUE_END), D3D_OK);
    HRESULT hr = wait_query(q, &b, sizeof b);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(b, TRUE);
    /* it can be reused */
    b = 77;
    EXPECT_HR(IDirect3DQuery9_Issue(q, D3DISSUE_END), D3D_OK);
    EXPECT_HR(wait_query(q, &b, sizeof b), S_OK);
    EXPECT_EQ(b, TRUE);
    /* a zero sized request only reports status */
    EXPECT_HR(IDirect3DQuery9_GetData(q, NULL, 0, D3DGETDATA_FLUSH), S_OK);
    IDirect3DQuery9_Release(q);
    SD_END(d);
}

TEST(d3d9_state_draw_query_event_never_issued)
{
    SD_OPEN(d);
    IDirect3DQuery9 *q = mkq(&d, D3DQUERYTYPE_EVENT);
    SD_NEED(d, q != NULL, "no event query");
    BOOL b = 77;
    HRESULT hr = IDirect3DQuery9_GetData(q, &b, sizeof b, D3DGETDATA_FLUSH);
    t_note("GetData on never issued event query -> %s value %ld", d3dh_hr_name(hr), (long)b);
    IDirect3DQuery9_Release(q);
    SD_END(d);
}

TEST(d3d9_state_draw_query_issue_flags)
{
    SD_OPEN(d);
    IDirect3DQuery9 *q = mkq(&d, D3DQUERYTYPE_EVENT);
    SD_NEED(d, q != NULL, "no event query");
    HRESULT hr = IDirect3DQuery9_Issue(q, D3DISSUE_BEGIN);
    t_note("Issue(BEGIN) on an event query -> %s", d3dh_hr_name(hr));
    hr = IDirect3DQuery9_Issue(q, 0);
    t_note("Issue(0) on an event query -> %s", d3dh_hr_name(hr));
    hr = IDirect3DQuery9_Issue(q, 4);
    t_note("Issue(4) on an event query -> %s", d3dh_hr_name(hr));
    hr = IDirect3DQuery9_Issue(q, D3DISSUE_BEGIN | D3DISSUE_END);
    t_note("Issue(BEGIN|END) on an event query -> %s", d3dh_hr_name(hr));
    IDirect3DQuery9_Release(q);
    SD_END(d);
}

TEST(d3d9_state_draw_query_occlusion_issue_order_free)
{
    SD_OPEN(d);
    IDirect3DQuery9 *q = mkq(&d, D3DQUERYTYPE_OCCLUSION);
    SD_NEED(d, q != NULL, "no occlusion query");
    EXPECT_EQ(IDirect3DQuery9_GetDataSize(q), 4);
    EXPECT_HR(IDirect3DQuery9_Issue(q, D3DISSUE_END), D3D_OK);
    EXPECT_HR(IDirect3DQuery9_Issue(q, D3DISSUE_BEGIN), D3D_OK);
    EXPECT_HR(IDirect3DQuery9_Issue(q, D3DISSUE_BEGIN), D3D_OK);
    EXPECT_HR(IDirect3DQuery9_Issue(q, D3DISSUE_END), D3D_OK);
    EXPECT_HR(IDirect3DQuery9_Issue(q, D3DISSUE_END), D3D_OK);
    DWORD v = 0xffffffff;
    EXPECT_HR(wait_query(q, &v, sizeof v), S_OK);
    EXPECT_EQ(v, 0);                                 /* nothing was drawn */
    IDirect3DQuery9_Release(q);
    SD_END(d);
}

TEST(d3d9_state_draw_query_getdata_size_semantics)
{
    SD_OPEN(d);
    IDirect3DQuery9 *q = mkq(&d, D3DQUERYTYPE_OCCLUSION);
    SD_NEED(d, q != NULL, "no occlusion query");
    IDirect3DQuery9_Issue(q, D3DISSUE_BEGIN);
    IDirect3DQuery9_Issue(q, D3DISSUE_END);
    DWORD v[2];
    v[0] = v[1] = 0xf0f0f0f0;
    EXPECT_HR(wait_query(q, v, 0), S_OK);            /* size 0: status only, nothing is written */
    EXPECT_EQ(v[0], 0xf0f0f0f0);
    v[0] = v[1] = 0xf0f0f0f0;
    EXPECT_HR(IDirect3DQuery9_GetData(q, v, 2, D3DGETDATA_FLUSH), S_OK);   /* a short size copies only that many bytes */
    CHECK((v[0] & 0xffff0000u) == 0xf0f00000u);
    EXPECT_EQ(v[0] & 0xffff, 0);
    v[0] = v[1] = 0xf0f0f0f0;
    EXPECT_HR(IDirect3DQuery9_GetData(q, v, 4, D3DGETDATA_FLUSH), S_OK);
    EXPECT_EQ(v[0], 0);
    EXPECT_EQ(v[1], 0xf0f0f0f0);
    IDirect3DQuery9_Release(q);
    SD_END(d);
}

TEST(d3d9_state_draw_query_occlusion_pixel_counts)
{
    SD_OPEN_R(d);
    IDirect3DQuery9 *q = mkq(&d, D3DQUERYTYPE_OCCLUSION);
    SD_NEED(d, q != NULL, "no occlusion query");
    SDRT rt;
    if (!sd_rt_make(&d, &rt, 32, 32, D3DFMT_A8R8G8B8, 1)) { IDirect3DQuery9_Release(q); SD_END(d); return; }
    TLV v[6];
    DWORD cnt = 0xffffffff;
    IDirect3DDevice9_SetFVF(d.dev, TLV_FVF);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0);
    /* whole target */
    tlv_full(v, 0.5f, 0xffffffff);
    IDirect3DDevice9_BeginScene(d.dev);
    IDirect3DQuery9_Issue(q, D3DISSUE_BEGIN);
    EXPECT_HR(IDirect3DDevice9_DrawPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 2, v, sizeof(TLV)), D3D_OK);
    IDirect3DQuery9_Issue(q, D3DISSUE_END);
    IDirect3DDevice9_EndScene(d.dev);
    EXPECT_HR(wait_query(q, &cnt, sizeof cnt), S_OK);
    EXPECT_EQ(cnt, 32 * 32);
    /* the same quad again, now hidden behind the first one: rejected by the depth test */
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_ZFUNC, D3DCMP_LESS);
    cnt = 0xffffffff;
    IDirect3DDevice9_BeginScene(d.dev);
    IDirect3DQuery9_Issue(q, D3DISSUE_BEGIN);
    IDirect3DDevice9_DrawPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 2, v, sizeof(TLV));
    IDirect3DQuery9_Issue(q, D3DISSUE_END);
    IDirect3DDevice9_EndScene(d.dev);
    EXPECT_HR(wait_query(q, &cnt, sizeof cnt), S_OK);
    EXPECT_EQ(cnt, 0);
    /* a nearer rectangle of 8x8 */
    tlv_rect(v, 4.0f, 4.0f, 12.0f, 12.0f, 0.25f, 0xff00ff00u);
    cnt = 0xffffffff;
    IDirect3DDevice9_BeginScene(d.dev);
    IDirect3DQuery9_Issue(q, D3DISSUE_BEGIN);
    IDirect3DDevice9_DrawPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 2, v, sizeof(TLV));
    IDirect3DQuery9_Issue(q, D3DISSUE_END);
    IDirect3DDevice9_EndScene(d.dev);
    EXPECT_HR(wait_query(q, &cnt, sizeof cnt), S_OK);
    EXPECT_EQ(cnt, 64);
    sd_rt_free(&d, &rt);
    IDirect3DQuery9_Release(q);
    SD_END(d);
}

TEST(d3d9_state_draw_query_timestamp_frequency)
{
    SD_OPEN(d);
    IDirect3DQuery9 *q = mkq(&d, D3DQUERYTYPE_TIMESTAMPFREQ);
    SD_NEED(d, q != NULL, "no timestamp frequency query");
    UINT64 freq = 0;
    IDirect3DQuery9_Issue(q, D3DISSUE_END);
    EXPECT_HR(wait_query(q, &freq, sizeof freq), S_OK);
    t_note("D3DQUERYTYPE_TIMESTAMPFREQ = %llu", (unsigned long long)freq);
    CHECK(freq != 0);
    IDirect3DQuery9_Release(q);
    SD_END(d);
}

TEST(d3d9_state_draw_query_timestamp_disjoint_flow)
{
    SD_OPEN(d);
    IDirect3DQuery9 *dq = mkq(&d, D3DQUERYTYPE_TIMESTAMPDISJOINT);
    IDirect3DQuery9 *tq = mkq(&d, D3DQUERYTYPE_TIMESTAMP);
    if (!dq || !tq) { SAFE_RELEASE(dq); SAFE_RELEASE(tq); SD_NEED(d, 0, "timestamp queries unavailable"); }
    BOOL disjoint = 77;
    UINT64 ts = 0;
    EXPECT_HR(IDirect3DQuery9_Issue(dq, D3DISSUE_BEGIN), D3D_OK);
    EXPECT_HR(IDirect3DQuery9_Issue(tq, D3DISSUE_END), D3D_OK);
    EXPECT_HR(IDirect3DQuery9_Issue(dq, D3DISSUE_END), D3D_OK);
    EXPECT_HR(wait_query(dq, &disjoint, sizeof disjoint), S_OK);
    t_note("timestamp disjoint = %ld", (long)disjoint);
    CHECK(disjoint == TRUE || disjoint == FALSE);
    EXPECT_HR(wait_query(tq, &ts, sizeof ts), S_OK);
    IDirect3DQuery9_Release(dq);
    IDirect3DQuery9_Release(tq);
    SD_END(d);
}

TEST(d3d9_state_draw_query_many_create_release)
{
    SD_OPEN(d);
    for (int i = 0; i < 200; i++) {
        IDirect3DQuery9 *q = mkq(&d, (i & 1) ? D3DQUERYTYPE_OCCLUSION : D3DQUERYTYPE_EVENT);
        if (!q) break;
        IDirect3DQuery9_Issue(q, D3DISSUE_END);
        IDirect3DQuery9_Release(q);
    }
    SD_END(d);
}
