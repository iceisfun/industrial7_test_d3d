/* D3D9 drawing: stencil, textures, fog, clip planes, transforms, lighting, buffers, shaders, ProcessVertices. */
#include "d3dh.h"
#include "drh.h"

/* ---- stencil ------------------------------------------------------------------------------ */
static int has_stencil(DR *r) { return r->d.dsfmt == D3DFMT_D24S8 && (r->d.caps.StencilCaps & D3DSTENCILCAPS_KEEP); }
static void clear_stencil(DR *r, DWORD v) { IDirect3DDevice9_Clear(r->d.dev, 0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, v); }
/* run an operation pass that only touches the stencil buffer */
static void stencil_pass(DR *r, D3DCMPFUNC func, DWORD ref, DWORD mask, DWORD wmask, DWORD fail, DWORD zfail, DWORD pass, float z)
{
    IDirect3DDevice9 *dev = r->d.dev;
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFUNC, func);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILREF, ref);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILMASK, mask);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILWRITEMASK, wmask);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFAIL, fail);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILZFAIL, zfail);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILPASS, pass);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0);
    dr_full(r, z, WHITE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xf);
}
/* does the stencil buffer (full target) currently equal value, using an EQUAL test draw? */
static int stencil_equals(DR *r, DWORD value)
{
    IDirect3DDevice9 *dev = r->d.dev;
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILREF, value);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILMASK, 0xff);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILWRITEMASK, 0xff);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
    dr_full(r, 0.5f, WHITE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    if (!dr_read(r, px)) return -1;
    return SD_PX(px, DW, 32, 32) == WHITE;
}

TEST(d3d9_state_draw_dr_stencil_operations)
{
    DR_OPEN(r);
    DR_NEED(r, has_stencil(&r), "no D24S8 stencil buffer");
    static const struct { DWORD init, op, ref, want; const char *name; } t[] = {
        { 0x10, D3DSTENCILOP_KEEP, 0x55, 0x10, "KEEP" }, { 0x10, D3DSTENCILOP_ZERO, 0x55, 0x00, "ZERO" },
        { 0x10, D3DSTENCILOP_REPLACE, 0x55, 0x55, "REPLACE" }, { 0x10, D3DSTENCILOP_INCR, 0x55, 0x11, "INCR" },
        { 0x10, D3DSTENCILOP_DECR, 0x55, 0x0f, "DECR" }, { 0x10, D3DSTENCILOP_INVERT, 0x55, 0xef, "INVERT" },
        { 0x10, D3DSTENCILOP_INCRSAT, 0x55, 0x11, "INCRSAT" }, { 0x10, D3DSTENCILOP_DECRSAT, 0x55, 0x0f, "DECRSAT" },
        { 0xff, D3DSTENCILOP_INCR, 0x55, 0x00, "INCR wraps" }, { 0xff, D3DSTENCILOP_INCRSAT, 0x55, 0xff, "INCRSAT saturates" },
        { 0x00, D3DSTENCILOP_DECR, 0x55, 0xff, "DECR wraps" }, { 0x00, D3DSTENCILOP_DECRSAT, 0x55, 0x00, "DECRSAT saturates" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        dr_clear(&r, BLACK, 1.0f, t[i].init);
        stencil_pass(&r, D3DCMP_ALWAYS, t[i].ref, 0xff, 0xff, D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, t[i].op, 0.5f);
        int yes = stencil_equals(&r, t[i].want);
        int no = stencil_equals(&r, (t[i].want + 1) & 0xff);
        t_check(yes == 1, __FILE__, __LINE__, "stencil op result", "%s: stencil != 0x%02lx", t[i].name, (unsigned long)t[i].want);
        t_check(no == 0, __FILE__, __LINE__, "stencil op result (negative)", "%s: stencil also matched 0x%02lx", t[i].name, (unsigned long)((t[i].want + 1) & 0xff));
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_stencil_functions)
{
    DR_OPEN(r);
    DR_NEED(r, has_stencil(&r), "no D24S8 stencil buffer");
    DWORD px[DW * DH];
    /* stored value 0x50, reference 0x40: the test is  ref FUNC stored */
    for (unsigned i = 0; i < 8; i++) {
        if (!(r.d.caps.StencilCaps & D3DSTENCILCAPS_KEEP)) break;
        dr_clear(&r, BLACK, 1.0f, 0x50);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILENABLE, TRUE);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILFUNC, g_cmp_fn[i]);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILREF, 0x40);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILMASK, 0xff);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
        dr_full(&r, 0.5f, WHITE);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILENABLE, FALSE);
        if (!dr_read(&r, px)) continue;
        int drawn = SD_PX(px, DW, 32, 32) == WHITE;
        t_check(drawn == cmp_pass2(g_cmp_fn[i], 0x40, 0x50), __FILE__, __LINE__, "stencil function", "func %d: ref 0x40 stored 0x50 drawn %d", (int)g_cmp_fn[i], drawn);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_stencil_read_mask)
{
    DR_OPEN(r);
    DR_NEED(r, has_stencil(&r), "no D24S8 stencil buffer");
    DWORD px[DW * DH];
    static const struct { DWORD mask; int drawn; } t[] = { { 0xff, 0 }, { 0x0f, 1 }, { 0xf0, 0 }, { 0x00, 1 } };
    for (unsigned i = 0; i < 4; i++) {
        dr_clear(&r, BLACK, 1.0f, 0x50);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILENABLE, TRUE);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILFUNC, D3DCMP_EQUAL);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILREF, 0x40);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILMASK, t[i].mask);
        dr_full(&r, 0.5f, WHITE);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_STENCILENABLE, FALSE);
        if (dr_read(&r, px)) t_check((SD_PX(px, DW, 32, 32) == WHITE) == t[i].drawn, __FILE__, __LINE__, "stencil read mask", "mask 0x%02lx (0x40 vs 0x50)", (unsigned long)t[i].mask);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_stencil_write_mask)
{
    DR_OPEN(r);
    DR_NEED(r, has_stencil(&r), "no D24S8 stencil buffer");
    dr_clear(&r, BLACK, 1.0f, 0xa0);
    stencil_pass(&r, D3DCMP_ALWAYS, 0xff, 0xff, 0x0f, D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, 0.5f);
    CHECK(stencil_equals(&r, 0xaf) == 1);
    dr_clear(&r, BLACK, 1.0f, 0xa0);
    stencil_pass(&r, D3DCMP_ALWAYS, 0x00, 0xff, 0xf0, D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, 0.5f);
    CHECK(stencil_equals(&r, 0x00) == 1);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_stencil_fail_and_zfail)
{
    DR_OPEN(r);
    DR_NEED(r, has_stencil(&r), "no D24S8 stencil buffer");
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZFUNC, D3DCMP_LESS);
    /* depth fails (0.75 is not less than the stored 0.25): STENCILZFAIL applies */
    dr_clear(&r, BLACK, 0.25f, 0x00);
    stencil_pass(&r, D3DCMP_ALWAYS, 0x33, 0xff, 0xff, D3DSTENCILOP_ZERO, D3DSTENCILOP_REPLACE, D3DSTENCILOP_INCR, 0.75f);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_FALSE);
    CHECK(stencil_equals(&r, 0x33) == 1);
    /* depth passes: STENCILPASS applies */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    dr_clear(&r, BLACK, 0.75f, 0x00);
    stencil_pass(&r, D3DCMP_ALWAYS, 0x33, 0xff, 0xff, D3DSTENCILOP_ZERO, D3DSTENCILOP_REPLACE, D3DSTENCILOP_INCR, 0.25f);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_FALSE);
    CHECK(stencil_equals(&r, 0x01) == 1);
    /* stencil test fails: STENCILFAIL applies and the depth test is not reached */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    dr_clear(&r, BLACK, 0.75f, 0x07);
    stencil_pass(&r, D3DCMP_NEVER, 0x33, 0xff, 0xff, D3DSTENCILOP_INCR, D3DSTENCILOP_ZERO, D3DSTENCILOP_ZERO, 0.25f);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_FALSE);
    CHECK(stencil_equals(&r, 0x08) == 1);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_stencil_two_sided)
{
    DR_OPEN(r);
    DR_NEED(r, has_stencil(&r) && (r.d.caps.StencilCaps & D3DSTENCILCAPS_TWOSIDED), "no two sided stencil");
    IDirect3DDevice9 *dev = DEV(r);
    TLV cw[3], ccw[3];
    tlv_set(&cw[0], -4, -4, 0.5f, WHITE); tlv_set(&cw[1], 200, -4, 0.5f, WHITE); tlv_set(&cw[2], -4, 200, 0.5f, WHITE);
    tlv_set(&ccw[0], -4, -4, 0.5f, WHITE); tlv_set(&ccw[1], -4, 200, 0.5f, WHITE); tlv_set(&ccw[2], 200, -4, 0.5f, WHITE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_TWOSIDEDSTENCILMODE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILPASS, D3DSTENCILOP_INCR);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CCW_STENCILPASS, D3DSTENCILOP_DECR);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0);
    dr_clear(&r, BLACK, 1.0f, 0x10);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, cw, sizeof(TLV)), D3D_OK);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xf);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_TWOSIDEDSTENCILMODE, FALSE);
    CHECK(stencil_equals(&r, 0x11) == 1);          /* clockwise face: front operations */
    IDirect3DDevice9_SetRenderState(dev, D3DRS_TWOSIDEDSTENCILMODE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0);
    dr_clear(&r, BLACK, 1.0f, 0x10);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, ccw, sizeof(TLV)), D3D_OK);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xf);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_TWOSIDEDSTENCILMODE, FALSE);
    CHECK(stencil_equals(&r, 0x0f) == 1);          /* counter-clockwise face: CCW_ operations */
    DR_END(r);
}

/* ---- textures ------------------------------------------------------------------------------- */
typedef struct { float x, y, z, rhw; DWORD c; float u, v; } TLT;
#define TLT_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
static void tlt_quad(TLT v[6], float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, DWORD c)
{
    static const int ix[6] = { 0, 1, 0, 1, 1, 0 }, iy[6] = { 0, 0, 1, 0, 1, 1 };
    for (int i = 0; i < 6; i++) {
        v[i].x = ix[i] ? x1 : x0; v[i].y = iy[i] ? y1 : y0; v[i].z = 0.5f; v[i].rhw = 1.0f; v[i].c = c;
        v[i].u = ix[i] ? u1 : u0; v[i].v = iy[i] ? v1 : v0;
    }
}
static IDirect3DTexture9 *make_tex2x2(SD *d, DWORD tl, DWORD tr, DWORD bl, DWORD br)
{
    IDirect3DTexture9 *t = NULL;
    D3DLOCKED_RECT lr;
    if (FAILED(IDirect3DDevice9_CreateTexture(d->dev, 2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) return NULL;
    if (FAILED(IDirect3DTexture9_LockRect(t, 0, &lr, NULL, 0))) { IDirect3DTexture9_Release(t); return NULL; }
    DWORD *r0 = lr.pBits, *r1 = (DWORD *)((BYTE *)lr.pBits + lr.Pitch);
    r0[0] = tl; r0[1] = tr; r1[0] = bl; r1[1] = br;
    IDirect3DTexture9_UnlockRect(t, 0);
    return t;
}
static void tex_stage_texture(DR *r)
{
    IDirect3DDevice9 *dev = DEV(*r);
    IDirect3DDevice9_SetFVF(dev, TLT_FVF);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

TEST(d3d9_state_draw_dr_texture_point_sampling)
{
    DR_OPEN(r);
    IDirect3DTexture9 *tex = make_tex2x2(&r.d, RED, GREEN, BLUE, WHITE);
    DR_NEED(r, tex != NULL, "cannot create texture");
    tex_stage_texture(&r);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)tex);
    TLT v[6];
    DWORD px[DW * DH];
    tlt_quad(v, 0, 0, 64, 64, 0, 0, 1, 1, WHITE);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT)), D3D_OK);
    if (dr_read(&r, px)) {
        static const int xs[] = { 8, 24, 40, 56 };
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
            DWORD want = j < 2 ? (i < 2 ? RED : GREEN) : (i < 2 ? BLUE : WHITE);
            PXEQ(px, xs[i], xs[j], want);
        }
    }
    /* flipped coordinates swap the texels */
    tlt_quad(v, 0, 0, 64, 64, 1, 1, 0, 0, WHITE);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT));
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, WHITE); PXEQ(px, 56, 8, BLUE); PXEQ(px, 8, 56, GREEN); PXEQ(px, 56, 56, RED); }
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(tex);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_texture_bilinear_centre)
{
    DR_OPEN(r);
    IDirect3DTexture9 *tex = make_tex2x2(&r.d, RED, GREEN, BLUE, WHITE);
    DR_NEED(r, tex != NULL, "cannot create texture");
    DR_NEED(r, (r.d.caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR), "no linear magnification");
    tex_stage_texture(&r);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)tex);
    TLT v[6];
    DWORD px[DW * DH];
    tlt_quad(v, 0, 0, 64, 64, 0, 0, 1, 1, WHITE);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT)), D3D_OK);
    if (dr_read(&r, px)) {
        PXNEAR(px, 32, 32, 0xff7f7f7fu, 16);
        PXNEAR(px, 31, 31, 0xff7f7f7fu, 16);
        /* texel centres still reproduce the texel colours */
        PXNEAR(px, 16, 16, RED, 4); PXNEAR(px, 48, 16, GREEN, 4); PXNEAR(px, 16, 48, BLUE, 4); PXNEAR(px, 48, 48, WHITE, 4);
        /* halfway between two texels the colour is the mean */
        PXNEAR(px, 32, 16, 0xff7f7f00u, 16);
    }
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(tex);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_texture_addressing)
{
    DR_OPEN(r);
    IDirect3DTexture9 *tex = make_tex2x2(&r.d, RED, GREEN, BLUE, WHITE);
    DR_NEED(r, tex != NULL, "cannot create texture");
    tex_stage_texture(&r);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)tex);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_BORDERCOLOR, 0xff808080u);
    TLT v[6];
    DWORD px[DW * DH];
    tlt_quad(v, 0, 0, 64, 64, 0, 0, 2, 2, WHITE);       /* uv covers [0,2]x[0,2]; row 8 is v=0.25 (top texel row) */
    static const struct { D3DTEXTUREADDRESS mode; DWORD cap; DWORD at40, at56; } t[] = {
        { D3DTADDRESS_WRAP, D3DPTADDRESSCAPS_WRAP, RED, GREEN },
        { D3DTADDRESS_CLAMP, D3DPTADDRESSCAPS_CLAMP, GREEN, GREEN },
        { D3DTADDRESS_MIRROR, D3DPTADDRESSCAPS_MIRROR, GREEN, RED },
        { D3DTADDRESS_BORDER, D3DPTADDRESSCAPS_BORDER, 0xff808080u, 0xff808080u },
    };
    for (unsigned i = 0; i < 4; i++) {
        if (!(r.d.caps.TextureAddressCaps & t[i].cap)) { CHECKF(0, "address mode %d not reported in TextureAddressCaps (Win10 reports all)", (int)t[i].mode); continue; }
        IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_ADDRESSU, t[i].mode);
        IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dr_clear(&r, BLACK, 1.0f, 0);
        EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT)), D3D_OK);
        if (!dr_read(&r, px)) continue;
        PXEQ(px, 8, 8, RED);
        t_check(SD_PX(px, DW, 40, 8) == t[i].at40, __FILE__, __LINE__, "address mode u=1.25", "mode %d got 0x%08lx want 0x%08lx", (int)t[i].mode, (unsigned long)SD_PX(px, DW, 40, 8), (unsigned long)t[i].at40);
        t_check(SD_PX(px, DW, 56, 8) == t[i].at56, __FILE__, __LINE__, "address mode u=1.75", "mode %d got 0x%08lx want 0x%08lx", (int)t[i].mode, (unsigned long)SD_PX(px, DW, 56, 8), (unsigned long)t[i].at56);
    }
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(tex);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_texture_stage_arithmetic)
{
    DR_OPEN(r);
    IDirect3DTexture9 *tex = make_tex2x2(&r.d, WHITE, WHITE, WHITE, WHITE);
    DR_NEED(r, tex != NULL, "cannot create texture");
    tex_stage_texture(&r);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)tex);
    TLT v[6];
    DWORD px[DW * DH];
    tlt_quad(v, 0, 0, 64, 64, 0, 0, 1, 1, 0xff808080u);
    /* texture * texture factor */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_TEXTUREFACTOR, 0xff804020u);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT)), D3D_OK);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff804020u, 1);
    /* diffuse * texture factor */
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff402010u, 2);
    /* ADD */
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLOROP, D3DTOP_ADD);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xffffc0a0u, 2);
    /* SELECTARG2 / DISABLE of the second stage */
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff804020u, 1);
    /* alpha operation: texture alpha * diffuse alpha written to the target */
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_TEXTUREFACTOR, 0x44804020u);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT));
    if (dr_read(&r, px)) t_check((SD_PX(px, DW, 32, 32) >> 24) == 0x44, __FILE__, __LINE__, "alpha from texture factor", "alpha 0x%02lx", (unsigned long)(SD_PX(px, DW, 32, 32) >> 24));
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(tex);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_texture_stage_two_blend)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.MaxTextureBlendStages >= 2 && r.d.caps.MaxSimultaneousTextures >= 2, "fewer than two texture stages");
    IDirect3DTexture9 *t0 = make_tex2x2(&r.d, RED, RED, RED, RED), *t1 = make_tex2x2(&r.d, 0xff80ff80u, 0xff80ff80u, 0xff80ff80u, 0xff80ff80u);
    if (!t0 || !t1) { SAFE_RELEASE(t0); SAFE_RELEASE(t1); DR_NEED(r, 0, "cannot create textures"); }
    tex_stage_texture(&r);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)t0);
    IDirect3DDevice9_SetTexture(DEV(r), 1, (IDirect3DBaseTexture9 *)t1);
    IDirect3DDevice9_SetSamplerState(DEV(r), 1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(DEV(r), 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 1, D3DTSS_COLOROP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 1, D3DTSS_COLORARG2, D3DTA_CURRENT);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 1, D3DTSS_TEXCOORDINDEX, 0);
    TLT v[6];
    DWORD px[DW * DH];
    tlt_quad(v, 0, 0, 64, 64, 0, 0, 1, 1, WHITE);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLT)), D3D_OK);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff800000u, 2);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    IDirect3DDevice9_SetTexture(DEV(r), 1, NULL);
    SAFE_RELEASE(t0);
    SAFE_RELEASE(t1);
    DR_END(r);
}

/* ---- fog ---------------------------------------------------------------------------------------- */
TEST(d3d9_state_draw_dr_table_fog)
{
    DR_OPEN(r);
    DR_NEED(r, (r.d.caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE) && (r.d.caps.RasterCaps & D3DPRASTERCAPS_ZFOG), "no z based table fog");
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGCOLOR, 0x0000ff00u);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGSTART, f2d(0.25f));
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGEND, f2d(0.75f));
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGDENSITY, f2d(1.0f));
    static const struct { D3DFOGMODE mode; float z, f; } t[] = {
        { D3DFOG_LINEAR, 0.5f, 0.5f }, { D3DFOG_LINEAR, 0.25f, 1.0f }, { D3DFOG_LINEAR, 0.75f, 0.0f }, { D3DFOG_LINEAR, 0.1f, 1.0f }, { D3DFOG_LINEAR, 0.9f, 0.0f },
        { D3DFOG_LINEAR, 0.375f, 0.75f }, { D3DFOG_EXP, 0.5f, 0.60653f }, { D3DFOG_EXP2, 0.5f, 0.77880f },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGTABLEMODE, t[i].mode);
        dr_clear(&r, BLACK, 1.0f, 0);
        EXPECT_HR(dr_full(&r, t[i].z, WHITE), D3D_OK);
        if (!dr_read(&r, px)) continue;
        DWORD c = SD_PX(px, DW, 32, 32);
        int er = (int)(t[i].f * 255.0f + 0.5f), eg = 255;
        int rr = (c >> 16) & 0xff, gg = (c >> 8) & 0xff, bb = c & 0xff;
        t_check(abs(rr - er) <= 6 && abs(bb - er) <= 6 && abs(gg - eg) <= 2, __FILE__, __LINE__, "table fog", "mode %d z %g: got 0x%08lx want r/b %d g %d", (int)t[i].mode, t[i].z, (unsigned long)c, er, eg);
    }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGENABLE, FALSE);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_full(&r, 0.5f, WHITE);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, WHITE);
    DR_END(r);
}

typedef struct { float x, y, z, rhw; DWORD c, s; } TLVS;
TEST(d3d9_state_draw_dr_vertex_fog_specular_alpha)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.RasterCaps & D3DPRASTERCAPS_FOGVERTEX, "no vertex fog");
    TLVS v[6];
    static const float px_[6] = { 0, 1, 0, 1, 1, 0 }, py_[6] = { 0, 0, 1, 0, 1, 1 };
    for (int i = 0; i < 6; i++) { v[i].x = px_[i] ? 68 : -4; v[i].y = py_[i] ? 68 : -4; v[i].z = 0.5f; v[i].rhw = 1; v[i].c = WHITE; v[i].s = 0x80000000u; }
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGCOLOR, 0x0000ff00u);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLVS)), D3D_OK);
    DWORD px[DW * DH];
    if (dr_read(&r, px)) {
        DWORD c = SD_PX(px, DW, 32, 32);
        int rr = (c >> 16) & 0xff, gg = (c >> 8) & 0xff;
        CHECKF(c == 0xff80ff80u || (gg == 255 && rr > 120 && rr < 136),   /* Windows 10 Basic Render Driver: exactly 0xff80ff80 */ "fog factor from specular alpha not applied (0x%08lx)", (unsigned long)c);
    }
    v[0].s = v[1].s = v[2].s = v[3].s = v[4].s = v[5].s = 0xff000000u;
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLVS));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, WHITE, 2);
    v[0].s = v[1].s = v[2].s = v[3].s = v[4].s = v[5].s = 0x00000000u;
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLVS));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff00ff00u, 2);
    DR_END(r);
}

/* ---- transforms, clip planes, viewport mapping ---------------------------------------------------------- */
typedef struct { float x, y, z; DWORD c; } V3C;
#define V3C_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)
static void v3c_rect(V3C v[6], float x0, float y0, float x1, float y1, float z, DWORD c)
{
    static const int ix[6] = { 0, 1, 0, 1, 1, 0 }, iy[6] = { 0, 0, 1, 0, 1, 1 };
    for (int i = 0; i < 6; i++) { v[i].x = ix[i] ? x1 : x0; v[i].y = iy[i] ? y1 : y0; v[i].z = z; v[i].c = c; }
}
static void ident(D3DMATRIX *m) { memset(m, 0, sizeof *m); m->_11 = m->_22 = m->_33 = m->_44 = 1.0f; }
static void set_ident_transforms(DR *r)
{
    D3DMATRIX m;
    ident(&m);
    IDirect3DDevice9_SetTransform(DEV(*r), D3DTS_WORLD, &m);
    IDirect3DDevice9_SetTransform(DEV(*r), D3DTS_VIEW, &m);
    IDirect3DDevice9_SetTransform(DEV(*r), D3DTS_PROJECTION, &m);
}
static void ff_state(DR *r) { IDirect3DDevice9_SetFVF(DEV(*r), V3C_FVF); set_ident_transforms(r); }

TEST(d3d9_state_draw_dr_ff_identity_transform)
{
    DR_OPEN(r);
    ff_state(&r);
    V3C v[6];
    DWORD px[DW * DH];
    v3c_rect(v, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, WHITE);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C)), D3D_OK);
    if (dr_read(&r, px)) {
        PXEQ(px, 32, 32, WHITE); PXEQ(px, 20, 20, WHITE); PXEQ(px, 44, 44, WHITE); PXEQ(px, 12, 32, BLACK); PXEQ(px, 52, 32, BLACK); PXEQ(px, 32, 12, BLACK);
        int n = sd_count_is(px, DW, 0, 0, DW, DH, WHITE);
        CHECKF(n >= 32 * 32 - 70 && n <= 32 * 32 + 70, "quad area %d (expected about 1024)", n);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_ff_world_translation)
{
    DR_OPEN(r);
    ff_state(&r);
    V3C v[6];
    DWORD px[DW * DH];
    D3DMATRIX m;
    v3c_rect(v, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, WHITE);
    ident(&m); m._41 = 0.5f;                                  /* row vectors: translation lives in the fourth row */
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_WORLD, &m);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 48, 32, WHITE); PXEQ(px, 36, 32, WHITE); PXEQ(px, 60, 32, WHITE); PXEQ(px, 24, 32, BLACK); PXEQ(px, 48, 8, BLACK); }
    ident(&m); m._42 = 0.5f;                                  /* +y is up on screen */
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_WORLD, &m);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) { PXEQ(px, 32, 16, WHITE); PXEQ(px, 32, 28, WHITE); PXEQ(px, 32, 44, BLACK); PXEQ(px, 32, 60, BLACK); }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_ff_matrix_order)
{
    DR_OPEN(r);
    ff_state(&r);
    V3C v[6];
    DWORD px[DW * DH];
    D3DMATRIX w, vw, p;
    v3c_rect(v, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, WHITE);
    /* final position = vertex * world * view * projection */
    ident(&w); w._41 = 1.0f;                                  /* x -> [0.5, 1.5] */
    ident(&vw); vw._41 = -0.5f;                               /* x -> [0, 1] */
    ident(&p); p._11 = 0.5f;                                  /* x -> [0, 0.5] */
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_WORLD, &w);
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_VIEW, &vw);
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_PROJECTION, &p);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 40, 32, WHITE); PXEQ(px, 46, 32, WHITE); PXEQ(px, 28, 32, BLACK); PXEQ(px, 56, 32, BLACK); }
    /* projection scaling applied after translation (a different order would give a different result) */
    ident(&w); w._41 = 1.0f; ident(&vw); ident(&p); p._11 = 0.25f;
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_WORLD, &w);
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_VIEW, &vw);
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_PROJECTION, &p);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) { PXEQ(px, 40, 32, WHITE); PXEQ(px, 37, 32, WHITE); PXEQ(px, 33, 32, BLACK); PXEQ(px, 46, 32, BLACK); }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_ff_viewport_mapping)
{
    DR_OPEN(r);
    ff_state(&r);
    V3C v[6];
    DWORD px[DW * DH];
    D3DVIEWPORT9 vp = { 16, 8, 32, 40, 0.0f, 1.0f };
    v3c_rect(v, -1.0f, -1.0f, 1.0f, 1.0f, 0.5f, WHITE);
    dr_clear(&r, BLACK, 1.0f, 0);          /* Clear is limited to the viewport */
    IDirect3DDevice9_SetViewport(DEV(r), &vp);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C)), D3D_OK);
    if (dr_read(&r, px)) {
        int n = sd_count_is(px, DW, 0, 0, DW, DH, WHITE);
        CHECKF(n >= 32 * 40 - 80 && n <= 32 * 40 + 80, "viewport area %d (expected about 1280)", n);
        PXEQ(px, 18, 10, WHITE); PXEQ(px, 45, 45, WHITE); PXEQ(px, 14, 28, BLACK); PXEQ(px, 50, 28, BLACK); PXEQ(px, 32, 6, BLACK); PXEQ(px, 32, 50, BLACK);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_ff_viewport_depth_range)
{
    DR_OPEN(r);
    ff_state(&r);
    V3C v[6];
    DWORD px[DW * DH];
    /* NDC z 0.9 maps to depth 0.45 with the range [0, 0.5], and to 0.9 with the default range */
    D3DVIEWPORT9 half = { 0, 0, 64, 64, 0.0f, 0.5f }, full = { 0, 0, 64, 64, 0.0f, 1.0f };
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZFUNC, D3DCMP_LESS);
    v3c_rect(v, -1.0f, -1.0f, 1.0f, 1.0f, 0.9f, WHITE);
    IDirect3DDevice9_SetViewport(DEV(r), &full);
    dr_clear(&r, BLACK, 0.5f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) PXEQ(px, 32, 32, BLACK);              /* 0.9 is not less than 0.5 */
    IDirect3DDevice9_SetViewport(DEV(r), &half);
    dr_clear(&r, BLACK, 0.5f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) PXEQ(px, 32, 32, WHITE);              /* 0.45 is */
    D3DVIEWPORT9 late = { 0, 0, 64, 64, 0.6f, 1.0f };
    v3c_rect(v, -1.0f, -1.0f, 1.0f, 1.0f, 0.1f, WHITE);        /* default range: 0.1 passes; [0.6,1]: 0.64 fails */
    IDirect3DDevice9_SetViewport(DEV(r), &full);
    dr_clear(&r, BLACK, 0.5f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) PXEQ(px, 32, 32, WHITE);
    IDirect3DDevice9_SetViewport(DEV(r), &late);
    dr_clear(&r, BLACK, 0.5f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) PXEQ(px, 32, 32, BLACK);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_user_clip_planes)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.MaxUserClipPlanes >= 2, "fewer than two user clip planes");
    ff_state(&r);
    V3C v[6];
    DWORD px[DW * DH];
    float left[4] = { -1, 0, 0, 0 }, right[4] = { 1, 0, 0, 0 }, top[4] = { 0, 1, 0, 0 };
    v3c_rect(v, -1.0f, -1.0f, 1.0f, 1.0f, 0.5f, WHITE);
    IDirect3DDevice9_SetClipPlane(DEV(r), 0, left);
    IDirect3DDevice9_SetClipPlane(DEV(r), 1, top);
    /* plane 0 is configured but not enabled: nothing is clipped */
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) { PXEQ(px, 8, 32, WHITE); PXEQ(px, 56, 32, WHITE); }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_CLIPPLANEENABLE, 1);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C)), D3D_OK);
    if (dr_read(&r, px)) {
        PXEQ(px, 8, 32, WHITE); PXEQ(px, 28, 8, WHITE); PXEQ(px, 28, 56, WHITE); PXEQ(px, 36, 32, BLACK); PXEQ(px, 56, 32, BLACK);
        int n = sd_count_is(px, DW, 0, 0, DW, DH, WHITE);
        CHECKF(n >= 32 * 64 - 130 && n <= 32 * 64 + 130, "left half area %d", n);
    }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_CLIPPLANEENABLE, 3);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, WHITE); PXEQ(px, 28, 28, WHITE); PXEQ(px, 8, 56, BLACK); PXEQ(px, 56, 8, BLACK); PXEQ(px, 56, 56, BLACK); }
    /* only plane 0 replaced by the opposite half */
    IDirect3DDevice9_SetClipPlane(DEV(r), 0, right);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_CLIPPLANEENABLE, 1);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3C));
    if (dr_read(&r, px)) { PXEQ(px, 56, 32, WHITE); PXEQ(px, 8, 32, BLACK); }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_CLIPPLANEENABLE, 0);
    DR_END(r);
}

/* ---- lighting ---------------------------------------------------------------------------------------------- */
typedef struct { float x, y, z, nx, ny, nz; } V3N;
TEST(d3d9_state_draw_dr_fixed_function_lighting)
{
    DR_OPEN(r);
    set_ident_transforms(&r);
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZ | D3DFVF_NORMAL);
    V3N v[6];
    static const int ix[6] = { 0, 1, 0, 1, 1, 0 }, iy[6] = { 0, 0, 1, 0, 1, 1 };
    for (int i = 0; i < 6; i++) { v[i].x = ix[i] ? 1.0f : -1.0f; v[i].y = iy[i] ? 1.0f : -1.0f; v[i].z = 0.5f; v[i].nx = 0; v[i].ny = 0; v[i].nz = -1.0f; }
    D3DMATERIAL9 mat;
    memset(&mat, 0, sizeof mat);
    mat.Diffuse.r = 0.5f; mat.Diffuse.g = 0.25f; mat.Diffuse.b = 1.0f; mat.Diffuse.a = 1.0f;
    mat.Ambient.r = mat.Ambient.g = mat.Ambient.b = mat.Ambient.a = 1.0f;
    IDirect3DDevice9_SetMaterial(DEV(r), &mat);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_LIGHTING, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_COLORVERTEX, FALSE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SPECULARENABLE, FALSE);
    DWORD px[DW * DH];
    /* ambient only */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_AMBIENT, 0x00402010u);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3N)), D3D_OK);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff402010u, 1);
    /* one directional light facing the surface */
    D3DLIGHT9 l;
    memset(&l, 0, sizeof l);
    l.Type = D3DLIGHT_DIRECTIONAL; l.Diffuse.r = l.Diffuse.g = l.Diffuse.b = 1.0f; l.Diffuse.a = 1.0f; l.Direction.z = 1.0f;
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_AMBIENT, 0);
    IDirect3DDevice9_SetLight(DEV(r), 0, &l);
    IDirect3DDevice9_LightEnable(DEV(r), 0, TRUE);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3N));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff8040ffu, 2);
    /* light shining on the back side contributes nothing */
    l.Direction.z = -1.0f;
    IDirect3DDevice9_SetLight(DEV(r), 0, &l);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3N));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, BLACK, 1);
    /* disabled light, emissive material colour */
    IDirect3DDevice9_LightEnable(DEV(r), 0, FALSE);
    mat.Emissive.r = 0.25f;
    IDirect3DDevice9_SetMaterial(DEV(r), &mat);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3N));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff400000u, 1);
    /* lighting off: the material is ignored and a vertex without colour is white */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_LIGHTING, FALSE);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(V3N));
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, WHITE, 1);
    DR_END(r);
}
