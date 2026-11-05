/* D3D9 transforms, viewport, scissor rect, clip planes, material, lights. */
#include "d3dh.h"
#include "d9sd.h"

static const D3DMATRIX g_ident = {{{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }}};
static D3DMATRIX mk_mat(float seed)
{
    D3DMATRIX m;
    float *f = &m._11;
    for (int i = 0; i < 16; i++) f[i] = seed + (float)i * 0.5f - 3.0f;
    return m;
}
static int mat_eq(const D3DMATRIX *a, const D3DMATRIX *b) { return !memcmp(a, b, sizeof *a); }

static const D3DTRANSFORMSTATETYPE g_xf_types[] = {
    D3DTS_VIEW, D3DTS_PROJECTION, D3DTS_TEXTURE0, D3DTS_TEXTURE1, D3DTS_TEXTURE2, D3DTS_TEXTURE3, D3DTS_TEXTURE4,
    D3DTS_TEXTURE5, D3DTS_TEXTURE6, D3DTS_TEXTURE7, (D3DTRANSFORMSTATETYPE)D3DTS_WORLDMATRIX(0), (D3DTRANSFORMSTATETYPE)D3DTS_WORLDMATRIX(1),
    (D3DTRANSFORMSTATETYPE)D3DTS_WORLDMATRIX(2), (D3DTRANSFORMSTATETYPE)D3DTS_WORLDMATRIX(100), (D3DTRANSFORMSTATETYPE)D3DTS_WORLDMATRIX(255),
};
#define NXF ((int)(sizeof g_xf_types / sizeof g_xf_types[0]))

TEST(d3d9_state_draw_xf_defaults_identity)
{
    SD_OPEN(d);
    for (int i = 0; i < NXF; i++) {
        D3DMATRIX m = mk_mat(7);
        HRESULT hr = IDirect3DDevice9_GetTransform(d.dev, g_xf_types[i], &m);
        t_check(hr == D3D_OK && mat_eq(&m, &g_ident), __FILE__, __LINE__, "default transform is identity", "type %d hr %s", (int)g_xf_types[i], d3dh_hr_name(hr));
    }
    SD_END(d);
}

TEST(d3d9_state_draw_xf_roundtrip)
{
    SD_OPEN(d);
    for (int i = 0; i < NXF; i++) {
        D3DMATRIX in = mk_mat((float)i), out;
        memset(&out, 0xcc, sizeof out);
        EXPECT_HR(IDirect3DDevice9_SetTransform(d.dev, g_xf_types[i], &in), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetTransform(d.dev, g_xf_types[i], &out), D3D_OK);
        CHECKF(mat_eq(&in, &out), "transform type %d did not round trip", (int)g_xf_types[i]);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_xf_types_are_independent)
{
    SD_OPEN(d);
    for (int i = 0; i < NXF; i++) { D3DMATRIX in = mk_mat((float)(i * 10)); IDirect3DDevice9_SetTransform(d.dev, g_xf_types[i], &in); }
    for (int i = 0; i < NXF; i++) {
        D3DMATRIX in = mk_mat((float)(i * 10)), out;
        IDirect3DDevice9_GetTransform(d.dev, g_xf_types[i], &out);
        CHECKF(mat_eq(&in, &out), "transform type %d was overwritten", (int)g_xf_types[i]);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_xf_world_matrix_macro_values)
{
    CHECK(D3DTS_VIEW == 2);
    CHECK(D3DTS_PROJECTION == 3);
    CHECK(D3DTS_TEXTURE0 == 16);
    CHECK(D3DTS_TEXTURE7 == 23);
    CHECK(D3DTS_WORLD == 256);
    CHECK(D3DTS_WORLDMATRIX(0) == 256);
    CHECK(D3DTS_WORLDMATRIX(255) == 511);
}

TEST(d3d9_state_draw_xf_world1_is_world_matrix_1)
{
    SD_OPEN(d);
    D3DMATRIX in = mk_mat(3), out;
    EXPECT_HR(IDirect3DDevice9_SetTransform(d.dev, D3DTS_WORLD, &in), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetTransform(d.dev, (D3DTRANSFORMSTATETYPE)D3DTS_WORLDMATRIX(0), &out), D3D_OK);
    CHECK(mat_eq(&in, &out));
    in = mk_mat(4);
    EXPECT_HR(IDirect3DDevice9_SetTransform(d.dev, D3DTS_WORLD1, &in), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetTransform(d.dev, (D3DTRANSFORMSTATETYPE)D3DTS_WORLDMATRIX(1), &out), D3D_OK);
    CHECK(mat_eq(&in, &out));
    SD_END(d);
}

/* GetTransform accepts VIEW, PROJECTION, TEXTUREn and the world matrix range only. */
TEST(d3d9_state_draw_xf_get_invalid_types)
{
    SD_OPEN(d);
    static const int bad[] = { 0, 1, 4, 5, 8, 15, 24, 25, 100, 255, 512, 513, 1000, 0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        D3DMATRIX m = mk_mat(1);
        HRESULT hr = IDirect3DDevice9_GetTransform(d.dev, (D3DTRANSFORMSTATETYPE)bad[i], &m);
        t_check(hr == D3DERR_INVALIDCALL, __FILE__, __LINE__, "GetTransform(invalid type)", "type %d got %s", bad[i], d3dh_hr_name(hr));
    }
    SD_END(d);
}

TEST(d3d9_state_draw_xf_multiply_order)
{
    SD_OPEN(d);
    /* documented order: result = argument * current */
    D3DMATRIX a = mk_mat(1), b = mk_mat(5), r, got;
    float *fa = &a._11, *fb = &b._11, *fr = &r._11;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += fb[i * 4 + k] * fa[k * 4 + j];   /* b * a */
            fr[i * 4 + j] = s;
        }
    EXPECT_HR(IDirect3DDevice9_SetTransform(d.dev, D3DTS_VIEW, &a), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_MultiplyTransform(d.dev, D3DTS_VIEW, &b), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetTransform(d.dev, D3DTS_VIEW, &got), D3D_OK);
    int ok = 1;
    for (int i = 0; i < 16; i++) {
        float x = (&got._11)[i], y = fr[i], ay = y < 0 ? -y : y, df = x > y ? x - y : y - x;
        if (df > 0.001f * (1.0f + ay)) ok = 0;
    }
    CHECKF(ok, "MultiplyTransform did not compute argument * current");
    SD_END(d);
}

TEST(d3d9_state_draw_xf_multiply_all_types_from_identity)
{
    SD_OPEN(d);
    D3DMATRIX two = {{{ 2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,2 }}}, got;
    for (int i = 0; i < NXF; i++) {
        EXPECT_HR(IDirect3DDevice9_MultiplyTransform(d.dev, g_xf_types[i], &two), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetTransform(d.dev, g_xf_types[i], &got), D3D_OK);
        CHECKF(mat_eq(&got, &two), "type %d: identity * 2 != 2", (int)g_xf_types[i]);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_xf_multiply_not_recorded_in_stateblock)
{
    SD_OPEN(d);
    D3DMATRIX two = {{{ 2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,2 }}}, got;
    IDirect3DStateBlock9 *sb = NULL;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_MultiplyTransform(d.dev, D3DTS_VIEW, &two), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    /* the multiplication took effect immediately instead of being recorded */
    EXPECT_HR(IDirect3DDevice9_GetTransform(d.dev, D3DTS_VIEW, &got), D3D_OK);
    CHECK(mat_eq(&got, &two));
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_xf_set_in_stateblock_is_deferred)
{
    SD_OPEN(d);
    D3DMATRIX m = mk_mat(9), got;
    IDirect3DStateBlock9 *sb = NULL;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetTransform(d.dev, D3DTS_PROJECTION, &m), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetTransform(d.dev, D3DTS_PROJECTION, &got), D3D_OK);
    CHECK(mat_eq(&got, &g_ident));
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetTransform(d.dev, D3DTS_PROJECTION, &got), D3D_OK);
    CHECK(mat_eq(&got, &m));
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_xf_set_out_of_range_type)
{
    SD_OPEN(d);
    D3DMATRIX m = mk_mat(2);
    HRESULT hr = IDirect3DDevice9_SetTransform(d.dev, (D3DTRANSFORMSTATETYPE)1000, &m);
    t_note("SetTransform(1000) returned %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9_SetTransform(d.dev, (D3DTRANSFORMSTATETYPE)0, &m);
    t_note("SetTransform(0) returned %s", d3dh_hr_name(hr));
    SD_END(d);
}

/* ---- viewport ---------------------------------------------------------------- */
TEST(d3d9_state_draw_vp_default)
{
    SD_OPEN(d);
    D3DVIEWPORT9 vp;
    memset(&vp, 0xcc, sizeof vp);
    EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &vp), D3D_OK);
    EXPECT_EQ(vp.X, 0); EXPECT_EQ(vp.Y, 0); EXPECT_EQ(vp.Width, SD_W); EXPECT_EQ(vp.Height, SD_H);
    CHECK(vp.MinZ == 0.0f); CHECK(vp.MaxZ == 1.0f);
    SD_END(d);
}

TEST(d3d9_state_draw_vp_roundtrip_inside)
{
    SD_OPEN(d);
    static const D3DVIEWPORT9 t[] = {
        { 0, 0, 64, 64, 0.0f, 1.0f }, { 8, 4, 32, 40, 0.25f, 0.75f }, { 63, 63, 1, 1, 0.0f, 1.0f }, { 0, 0, 1, 1, 0.5f, 0.5f },
        { 10, 20, 54, 44, 0.0f, 0.5f }, { 32, 0, 32, 64, 1.0f, 1.0f },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        D3DVIEWPORT9 g;
        memset(&g, 0, sizeof g);
        EXPECT_HR(IDirect3DDevice9_SetViewport(d.dev, &t[i]), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &g), D3D_OK);
        CHECKF(g.X == t[i].X && g.Y == t[i].Y && g.Width == t[i].Width && g.Height == t[i].Height && g.MinZ == t[i].MinZ && g.MaxZ == t[i].MaxZ,
               "viewport %u: got %lu,%lu %lux%lu z %g..%g", i, (unsigned long)g.X, (unsigned long)g.Y, (unsigned long)g.Width, (unsigned long)g.Height, g.MinZ, g.MaxZ);
    }
    SD_END(d);
}

/* SetViewport itself does not validate against the render target; the failure is reported at Clear / draw time. */
TEST(d3d9_state_draw_vp_set_outside_target_succeeds)
{
    SD_OPEN(d);
    D3DVIEWPORT9 big = { 0, 0, 128, 128, 0.0f, 1.0f }, g;
    EXPECT_HR(IDirect3DDevice9_SetViewport(d.dev, &big), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &g), D3D_OK);
    EXPECT_EQ(g.Width, 128); EXPECT_EQ(g.Height, 128);
    D3DVIEWPORT9 off = { 60, 60, 16, 16, 0.0f, 1.0f };
    EXPECT_HR(IDirect3DDevice9_SetViewport(d.dev, &off), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &g), D3D_OK);
    EXPECT_EQ(g.X, 60); EXPECT_EQ(g.Width, 16);
    SD_END(d);
}

TEST(d3d9_state_draw_vp_clear_outside_target_fails)
{
    SD_OPEN(d);
    D3DVIEWPORT9 big = { 0, 0, 128, 128, 0.0f, 1.0f };
    EXPECT_HR(IDirect3DDevice9_SetViewport(d.dev, &big), D3D_OK);
    HRESULT hr = IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
    if (d.type == D3DDEVTYPE_NULLREF) t_note("NULLREF: Clear with oversized viewport -> %s", d3dh_hr_name(hr));
    else EXPECT_HR(hr, D3DERR_INVALIDCALL);
    D3DVIEWPORT9 ok = { 0, 0, 64, 64, 0.0f, 1.0f };
    EXPECT_HR(IDirect3DDevice9_SetViewport(d.dev, &ok), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0), D3D_OK);
    SD_END(d);
}

TEST(d3d9_state_draw_vp_draw_outside_target_fails)
{
    SD_OPEN_R(d);
    TLV v[3];
    tlv_set(&v[0], 0, 0, 0.5f, 0xffffffff); tlv_set(&v[1], 10, 0, 0.5f, 0xffffffff); tlv_set(&v[2], 0, 10, 0.5f, 0xffffffff);
    D3DVIEWPORT9 bad = { 32, 32, 64, 64, 0.0f, 1.0f };
    IDirect3DDevice9_SetViewport(d.dev, &bad);
    IDirect3DDevice9_SetFVF(d.dev, TLV_FVF);
    IDirect3DDevice9_BeginScene(d.dev);
    HRESULT hr = IDirect3DDevice9_DrawPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 1, v, sizeof(TLV));
    IDirect3DDevice9_EndScene(d.dev);
    if (hr != D3DERR_INVALIDCALL) t_note("draw with a viewport outside the target returned %s", d3dh_hr_name(hr));
    SD_END(d);
}

TEST(d3d9_state_draw_vp_reset_by_set_render_target)
{
    SD_OPEN(d);
    IDirect3DSurface9 *rt = NULL, *orig = NULL;
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(d.dev, 32, 16, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create render target");
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &orig);
    D3DVIEWPORT9 vp = { 4, 4, 8, 8, 0.1f, 0.9f }, g;
    IDirect3DDevice9_SetViewport(d.dev, &vp);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, rt), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &g), D3D_OK);
    EXPECT_EQ(g.X, 0); EXPECT_EQ(g.Y, 0); EXPECT_EQ(g.Width, 32); EXPECT_EQ(g.Height, 16);
    CHECK(g.MinZ == 0.0f); CHECK(g.MaxZ == 1.0f);
    RECT sc;
    EXPECT_HR(IDirect3DDevice9_GetScissorRect(d.dev, &sc), D3D_OK);
    EXPECT_EQ(sc.left, 0); EXPECT_EQ(sc.top, 0); EXPECT_EQ(sc.right, 32); EXPECT_EQ(sc.bottom, 16);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, orig), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &g), D3D_OK);
    EXPECT_EQ(g.Width, SD_W); EXPECT_EQ(g.Height, SD_H);
    SAFE_RELEASE(orig);
    SAFE_RELEASE(rt);
    SD_END(d);
}

TEST(d3d9_state_draw_vp_same_target_resets_viewport)
{
    SD_OPEN(d);
    IDirect3DSurface9 *orig = NULL;
    IDirect3DDevice9_GetRenderTarget(d.dev, 0, &orig);
    D3DVIEWPORT9 vp = { 4, 4, 8, 8, 0.1f, 0.9f }, g;
    IDirect3DDevice9_SetViewport(d.dev, &vp);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, orig), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &g), D3D_OK);
    /* setting a render target resets viewport and scissor even when it is the surface already bound */
    EXPECT_EQ(g.X, 0); EXPECT_EQ(g.Y, 0); EXPECT_EQ(g.Width, SD_W); EXPECT_EQ(g.Height, SD_H);
    CHECK(g.MinZ == 0.0f && g.MaxZ == 1.0f);
    RECT sc = { 50, 50, 60, 60 };
    IDirect3DDevice9_SetScissorRect(d.dev, &sc);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(d.dev, 0, orig), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetScissorRect(d.dev, &sc), D3D_OK);
    EXPECT_EQ(sc.left, 0); EXPECT_EQ(sc.top, 0); EXPECT_EQ(sc.right, SD_W); EXPECT_EQ(sc.bottom, SD_H);
    SAFE_RELEASE(orig);
    SD_END(d);
}

TEST(d3d9_state_draw_vp_stateblock_records_viewport)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    D3DVIEWPORT9 vp = { 2, 3, 20, 30, 0.2f, 0.8f }, g;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetViewport(d.dev, &vp), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    IDirect3DDevice9_GetViewport(d.dev, &g);
    EXPECT_EQ(g.Width, SD_W);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetViewport(d.dev, &g);
    EXPECT_EQ(g.X, 2); EXPECT_EQ(g.Y, 3); EXPECT_EQ(g.Width, 20); EXPECT_EQ(g.Height, 30);
    SAFE_RELEASE(sb);
    SD_END(d);
}

/* ---- scissor rect ------------------------------------------------------------- */
TEST(d3d9_state_draw_scissor_default_is_target)
{
    SD_OPEN(d);
    RECT r = { -1, -1, -1, -1 };
    EXPECT_HR(IDirect3DDevice9_GetScissorRect(d.dev, &r), D3D_OK);
    EXPECT_EQ(r.left, 0); EXPECT_EQ(r.top, 0); EXPECT_EQ(r.right, SD_W); EXPECT_EQ(r.bottom, SD_H);
    SD_END(d);
}

TEST(d3d9_state_draw_scissor_roundtrip)
{
    SD_OPEN(d);
    static const RECT t[] = { { 0, 0, 64, 64 }, { 8, 8, 24, 40 }, { 63, 63, 64, 64 }, { 0, 0, 1, 1 }, { 10, 5, 11, 60 } };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RECT g = { 0 };
        EXPECT_HR(IDirect3DDevice9_SetScissorRect(d.dev, &t[i]), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetScissorRect(d.dev, &g), D3D_OK);
        CHECKF(g.left == t[i].left && g.top == t[i].top && g.right == t[i].right && g.bottom == t[i].bottom, "scissor %u mismatch", i);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_scissor_independent_of_enable_state)
{
    SD_OPEN(d);
    RECT r = { 1, 2, 30, 31 }, g;
    IDirect3DDevice9_SetScissorRect(d.dev, &r);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_SCISSORTESTENABLE, TRUE);
    IDirect3DDevice9_GetScissorRect(d.dev, &g);
    EXPECT_EQ(g.right, 30);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_GetScissorRect(d.dev, &g);
    EXPECT_EQ(g.right, 30);
    SD_END(d);
}

TEST(d3d9_state_draw_scissor_caps_bit)
{
    SD_OPEN(d);
    t_note("D3DPRASTERCAPS_SCISSORTEST: %s", (d.caps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST) ? "yes" : "no");
    SD_END(d);
}

/* ---- clip planes -------------------------------------------------------------- */
TEST(d3d9_state_draw_clip_caps_and_default)
{
    SD_OPEN(d);
    t_note("MaxUserClipPlanes=%lu", (unsigned long)d.caps.MaxUserClipPlanes);
    for (DWORD i = 0; i < d.caps.MaxUserClipPlanes && i < 6; i++) {
        float p[4] = { 9, 9, 9, 9 };
        EXPECT_HR(IDirect3DDevice9_GetClipPlane(d.dev, i, p), D3D_OK);
        if (p[0] != 0 || p[1] != 0 || p[2] != 0 || p[3] != 0) t_note("clip plane %lu default (%g %g %g %g)", (unsigned long)i, p[0], p[1], p[2], p[3]);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_clip_roundtrip)
{
    SD_OPEN(d);
    SD_NEED(d, d.caps.MaxUserClipPlanes >= 1, "no user clip planes");
    for (DWORD i = 0; i < d.caps.MaxUserClipPlanes && i < 6; i++) {
        float in[4] = { 1.0f * (i + 1), -2.5f, 0.125f * i, 4.0f }, out[4] = { 0 };
        EXPECT_HR(IDirect3DDevice9_SetClipPlane(d.dev, i, in), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetClipPlane(d.dev, i, out), D3D_OK);
        CHECKF(!memcmp(in, out, sizeof in), "clip plane %lu did not round trip", (unsigned long)i);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_clip_planes_independent)
{
    SD_OPEN(d);
    SD_NEED(d, d.caps.MaxUserClipPlanes >= 2, "need two clip planes");
    float a[4] = { 1, 0, 0, -1 }, b[4] = { 0, 1, 0, -2 }, g[4];
    IDirect3DDevice9_SetClipPlane(d.dev, 0, a);
    IDirect3DDevice9_SetClipPlane(d.dev, 1, b);
    IDirect3DDevice9_GetClipPlane(d.dev, 0, g);
    CHECK(!memcmp(a, g, sizeof a));
    IDirect3DDevice9_GetClipPlane(d.dev, 1, g);
    CHECK(!memcmp(b, g, sizeof b));
    SD_END(d);
}

TEST(d3d9_state_draw_clip_stateblock_records_plane)
{
    SD_OPEN(d);
    SD_NEED(d, d.caps.MaxUserClipPlanes >= 1, "no user clip planes");
    float in[4] = { 0.5f, 0.25f, 0.125f, 1.0f }, g[4];
    IDirect3DStateBlock9 *sb = NULL;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetClipPlane(d.dev, 0, in), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    IDirect3DDevice9_GetClipPlane(d.dev, 0, g);
    CHECK(g[0] == 0 && g[3] == 0);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetClipPlane(d.dev, 0, g);
    CHECK(!memcmp(in, g, sizeof in));
    SAFE_RELEASE(sb);
    SD_END(d);
}

/* ---- material ------------------------------------------------------------------- */
TEST(d3d9_state_draw_material_roundtrip)
{
    SD_OPEN(d);
    D3DMATERIAL9 in, out;
    memset(&in, 0, sizeof in);
    in.Diffuse.r = 0.1f; in.Diffuse.g = 0.2f; in.Diffuse.b = 0.3f; in.Diffuse.a = 0.4f;
    in.Ambient.r = 0.5f; in.Ambient.g = 0.6f; in.Ambient.b = 0.7f; in.Ambient.a = 0.8f;
    in.Specular.r = 0.9f; in.Specular.g = 1.0f; in.Specular.b = 1.5f; in.Specular.a = -0.5f;
    in.Emissive.r = 2.0f; in.Emissive.g = 3.0f; in.Emissive.b = 4.0f; in.Emissive.a = 5.0f;
    in.Power = 16.0f;
    EXPECT_HR(IDirect3DDevice9_SetMaterial(d.dev, &in), D3D_OK);
    memset(&out, 0xcc, sizeof out);
    EXPECT_HR(IDirect3DDevice9_GetMaterial(d.dev, &out), D3D_OK);
    CHECK(!memcmp(&in, &out, sizeof in));
    SD_END(d);
}

TEST(d3d9_state_draw_material_default_recorded)
{
    SD_OPEN(d);
    D3DMATERIAL9 m;
    memset(&m, 0xcc, sizeof m);
    EXPECT_HR(IDirect3DDevice9_GetMaterial(d.dev, &m), D3D_OK);
    t_note("default material: diffuse %g %g %g %g ambient %g %g %g %g specular %g %g %g %g emissive %g %g %g %g power %g",
           m.Diffuse.r, m.Diffuse.g, m.Diffuse.b, m.Diffuse.a, m.Ambient.r, m.Ambient.g, m.Ambient.b, m.Ambient.a,
           m.Specular.r, m.Specular.g, m.Specular.b, m.Specular.a, m.Emissive.r, m.Emissive.g, m.Emissive.b, m.Emissive.a, m.Power);
    SD_END(d);
}

TEST(d3d9_state_draw_material_stateblock)
{
    SD_OPEN(d);
    D3DMATERIAL9 in, g;
    memset(&in, 0, sizeof in);
    in.Diffuse.r = 0.25f; in.Power = 4.0f;
    IDirect3DStateBlock9 *sb = NULL;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetMaterial(d.dev, &in), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    IDirect3DDevice9_GetMaterial(d.dev, &g);
    CHECK(g.Power != 4.0f);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetMaterial(d.dev, &g);
    CHECK(g.Power == 4.0f && g.Diffuse.r == 0.25f);
    SAFE_RELEASE(sb);
    SD_END(d);
}

/* ---- lights ----------------------------------------------------------------------- */
static D3DLIGHT9 mk_light(D3DLIGHTTYPE type, float s)
{
    D3DLIGHT9 l;
    memset(&l, 0, sizeof l);
    l.Type = type;
    l.Diffuse.r = 0.5f * s; l.Diffuse.g = 0.25f * s; l.Diffuse.b = 0.125f * s; l.Diffuse.a = 1.0f;
    l.Specular.r = 0.75f; l.Specular.g = 0.5f; l.Specular.b = 0.25f; l.Specular.a = 1.0f;
    l.Ambient.r = 0.1f; l.Ambient.g = 0.2f; l.Ambient.b = 0.3f; l.Ambient.a = 0.4f;
    l.Position.x = 1.0f * s; l.Position.y = 2.0f; l.Position.z = 3.0f;
    l.Direction.x = 0.0f; l.Direction.y = -1.0f; l.Direction.z = 0.0f;
    l.Range = 100.0f * s; l.Falloff = 1.5f; l.Attenuation0 = 1.0f; l.Attenuation1 = 0.25f; l.Attenuation2 = 0.0625f;
    l.Theta = 0.5f; l.Phi = 1.0f;
    return l;
}

TEST(d3d9_state_draw_light_get_unset_fails)
{
    SD_OPEN(d);
    D3DLIGHT9 l;
    BOOL en = 77;
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 0, &l), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 7, &l), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 0, &en), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 1000, &en), D3DERR_INVALIDCALL);
    SD_END(d);
}

TEST(d3d9_state_draw_light_roundtrip_types)
{
    SD_OPEN(d);
    static const D3DLIGHTTYPE types[] = { D3DLIGHT_POINT, D3DLIGHT_SPOT, D3DLIGHT_DIRECTIONAL };
    for (int i = 0; i < 3; i++) {
        D3DLIGHT9 in = mk_light(types[i], (float)(i + 1)), out;
        memset(&out, 0xcc, sizeof out);
        EXPECT_HR(IDirect3DDevice9_SetLight(d.dev, i, &in), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, i, &out), D3D_OK);
        CHECKF(!memcmp(&in, &out, sizeof in), "light %d type %d did not round trip", i, (int)types[i]);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_light_direction_not_normalised)
{
    SD_OPEN(d);
    D3DLIGHT9 in = mk_light(D3DLIGHT_DIRECTIONAL, 1.0f), out;
    in.Direction.x = 3.0f; in.Direction.y = 4.0f; in.Direction.z = 0.0f;
    EXPECT_HR(IDirect3DDevice9_SetLight(d.dev, 0, &in), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 0, &out), D3D_OK);
    CHECK(out.Direction.x == 3.0f && out.Direction.y == 4.0f && out.Direction.z == 0.0f);
    SD_END(d);
}

TEST(d3d9_state_draw_light_replace_existing)
{
    SD_OPEN(d);
    D3DLIGHT9 a = mk_light(D3DLIGHT_POINT, 1.0f), b = mk_light(D3DLIGHT_SPOT, 3.0f), g;
    IDirect3DDevice9_SetLight(d.dev, 4, &a);
    EXPECT_HR(IDirect3DDevice9_LightEnable(d.dev, 4, TRUE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetLight(d.dev, 4, &b), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 4, &g), D3D_OK);
    CHECK(!memcmp(&b, &g, sizeof b));
    BOOL en = 0;
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 4, &en), D3D_OK);
    CHECK(en != 0);          /* replacing the light keeps its enable state */
    SD_END(d);
}

TEST(d3d9_state_draw_light_index_growth)
{
    SD_OPEN(d);
    static const DWORD idx[] = { 0, 1, 31, 32, 33, 63, 64, 100, 255, 256, 1000, 4096 };
    for (unsigned i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        D3DLIGHT9 in = mk_light(D3DLIGHT_POINT, (float)(i + 1)), out;
        EXPECT_HR(IDirect3DDevice9_SetLight(d.dev, idx[i], &in), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, idx[i], &out), D3D_OK);
        CHECKF(!memcmp(&in, &out, sizeof in), "light index %lu mismatch", (unsigned long)idx[i]);
    }
    /* indices that were skipped over are still unset */
    D3DLIGHT9 l;
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 2, &l), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 500, &l), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 4097, &l), D3DERR_INVALIDCALL);
    SD_END(d);
}

TEST(d3d9_state_draw_light_set_does_not_enable)
{
    SD_OPEN(d);
    D3DLIGHT9 in = mk_light(D3DLIGHT_POINT, 1.0f);
    BOOL en = 55;
    IDirect3DDevice9_SetLight(d.dev, 2, &in);
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 2, &en), D3D_OK);
    EXPECT_EQ(en, 0);
    EXPECT_HR(IDirect3DDevice9_LightEnable(d.dev, 2, TRUE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 2, &en), D3D_OK);
    CHECK(en != 0);
    t_note("GetLightEnable raw value for an enabled light: 0x%lx", (unsigned long)en);
    EXPECT_HR(IDirect3DDevice9_LightEnable(d.dev, 2, FALSE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 2, &en), D3D_OK);
    EXPECT_EQ(en, 0);
    SD_END(d);
}

TEST(d3d9_state_draw_light_enable_creates_default_light)
{
    SD_OPEN(d);
    D3DLIGHT9 l;
    memset(&l, 0xcc, sizeof l);
    EXPECT_HR(IDirect3DDevice9_LightEnable(d.dev, 3, TRUE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 3, &l), D3D_OK);
    EXPECT_EQ(l.Type, D3DLIGHT_DIRECTIONAL);
    CHECK(l.Diffuse.r == 1.0f && l.Diffuse.g == 1.0f && l.Diffuse.b == 1.0f);
    CHECK(l.Direction.x == 0.0f && l.Direction.y == 0.0f && l.Direction.z == 1.0f);
    CHECK(l.Position.x == 0.0f && l.Position.y == 0.0f && l.Position.z == 0.0f);
    CHECK(l.Range == 0.0f && l.Falloff == 0.0f);
    CHECK(l.Specular.r == 0.0f && l.Ambient.r == 0.0f);
    BOOL en = 0;
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 3, &en), D3D_OK);
    CHECK(en != 0);
    SD_END(d);
}

TEST(d3d9_state_draw_light_enable_disable_unset_creates_light)
{
    SD_OPEN(d);
    BOOL en = 99;
    D3DLIGHT9 l;
    EXPECT_HR(IDirect3DDevice9_LightEnable(d.dev, 9, FALSE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 9, &en), D3D_OK);
    EXPECT_EQ(en, 0);
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 9, &l), D3D_OK);
    EXPECT_EQ(l.Type, D3DLIGHT_DIRECTIONAL);
    SD_END(d);
}

TEST(d3d9_state_draw_light_enable_beyond_max_active)
{
    SD_OPEN(d);
    DWORD maxa = d.caps.MaxActiveLights;
    t_note("MaxActiveLights=%lu", (unsigned long)maxa);
    if (maxa == 0 || maxa > 64) maxa = 8;
    for (DWORD i = 0; i < maxa + 3; i++) {
        BOOL en = 0;
        EXPECT_HR(IDirect3DDevice9_LightEnable(d.dev, i, TRUE), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, i, &en), D3D_OK);
        CHECKF(en != 0, "light %lu not reported enabled", (unsigned long)i);
    }
    for (DWORD i = 0; i < maxa + 3; i++) EXPECT_HR(IDirect3DDevice9_LightEnable(d.dev, i, FALSE), D3D_OK);
    SD_END(d);
}

TEST(d3d9_state_draw_light_invalid_type_recorded)
{
    SD_OPEN(d);
    D3DLIGHT9 l = mk_light(D3DLIGHT_POINT, 1.0f);
    static const int types[] = { 0, 4, 5, 100 };
    for (unsigned i = 0; i < sizeof types / sizeof types[0]; i++) {
        l.Type = (D3DLIGHTTYPE)types[i];
        HRESULT hr = IDirect3DDevice9_SetLight(d.dev, 20 + i, &l);
        t_note("SetLight with Type=%d returned %s", types[i], d3dh_hr_name(hr));
    }
    SD_END(d);
}

TEST(d3d9_state_draw_light_stateblock_records_light)
{
    SD_OPEN(d);
    D3DLIGHT9 in = mk_light(D3DLIGHT_SPOT, 2.0f), g;
    IDirect3DStateBlock9 *sb = NULL;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetLight(d.dev, 1, &in), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_LightEnable(d.dev, 1, TRUE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetLight(d.dev, 1, &g), D3D_OK);
    CHECK(!memcmp(&in, &g, sizeof in));
    BOOL en = 0;
    EXPECT_HR(IDirect3DDevice9_GetLightEnable(d.dev, 1, &en), D3D_OK);
    CHECK(en != 0);
    SAFE_RELEASE(sb);
    SD_END(d);
}
