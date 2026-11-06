/* Additional rendering contracts probed on the Windows 10 Basic Render Driver (D3D9 HAL): depth bias, multiple render targets,
 * sRGB write, separate alpha blend, mip selection, point sprites, texkill, lockable depth, NULLREF behaviour. */
#include "drh.h"

static IDirect3DPixelShader9 *xps(SD *d, const char *src, const char *target)
{
    ID3DBlob *b = NULL; IDirect3DPixelShader9 *ps = NULL;
    if (FAILED(d3dh_compile(src, "main", target, 0, &b))) return NULL;
    if (FAILED(IDirect3DDevice9_CreatePixelShader(d->dev, d3dh_blob_ptr(b), &ps))) ps = NULL;
    SAFE_RELEASE(b);
    return ps;
}
static IDirect3DTexture9 *xtex(SD *d, UINT w, UINT h, UINT levels, const DWORD *level_colors)
{
    IDirect3DTexture9 *t = NULL; UINT l;
    if (FAILED(IDirect3DDevice9_CreateTexture(d->dev, w, h, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) return NULL;
    for (l = 0; l < IDirect3DTexture9_GetLevelCount(t); l++) {
        D3DLOCKED_RECT lr; UINT lw = w >> l, lh = h >> l, x, y; if (!lw) lw = 1; if (!lh) lh = 1;
        if (SUCCEEDED(IDirect3DTexture9_LockRect(t, l, &lr, NULL, 0))) {
            for (y = 0; y < lh; y++) for (x = 0; x < lw; x++) ((DWORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x] = level_colors[l];
            IDirect3DTexture9_UnlockRect(t, l);
        }
    }
    return t;
}
typedef struct { float x, y, z, rhw, u, v; } TV;
static void quad_tv(TV v[6], float x0, float y0, float x1, float y1, float z)
{
    static const int ix[6] = { 0, 1, 0, 1, 1, 0 }, iy[6] = { 0, 0, 1, 0, 1, 1 };
    for (int i = 0; i < 6; i++) { v[i].x = ix[i] ? x1 : x0; v[i].y = iy[i] ? y1 : y0; v[i].z = z; v[i].rhw = 1; v[i].u = (float)ix[i]; v[i].v = (float)iy[i]; }
}

/* D3DRS_DEPTHBIAS moves a coplanar fragment in front of (negative) or behind (positive) the stored depth. */
TEST(d3d9_state_draw_dr_depth_bias)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZFUNC, D3DCMP_LESS);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, RED), D3D_OK);
    EXPECT_HR(dr_full(&r, 0.5f, GREEN), D3D_OK);                     /* equal depth fails ZFUNC_LESS */
    if (dr_read(&r, px)) PXEQ(px, 32, 32, RED);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DEPTHBIAS, f2d(-0.01f));
    EXPECT_HR(dr_full(&r, 0.5f, GREEN), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, GREEN);                   /* biased towards the viewer: passes */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DEPTHBIAS, f2d(0.01f));
    EXPECT_HR(dr_full(&r, 0.5f, BLUE), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, GREEN);                   /* biased away: fails */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DEPTHBIAS, 0);
    DR_END(r);
}

/* Two simultaneous render targets written by a ps_2_0 / ps_3_0 (COLOR0 and COLOR1). */
TEST(d3d9_state_draw_dr_multiple_render_targets)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.NumSimultaneousRTs >= 2 && r.d.caps.PixelShaderVersion >= D3DPS_VERSION(2, 0), "no MRT");
    IDirect3DPixelShader9 *ps = xps(&r.d, "struct O { float4 a : COLOR0; float4 b : COLOR1; };\nO main() { O o; o.a = float4(1,0,0,1); o.b = float4(0,1,0,1); return o; }\n", "ps_2_0");
    DR_NEED(r, ps != NULL, "cannot build ps_2_0");
    IDirect3DSurface9 *rt1 = NULL, *sys = NULL;
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(DEV(r), DW, DH, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt1, NULL);
    EXPECT_HR(hr, D3D_OK);
    if (FAILED(hr)) { SAFE_RELEASE(ps); DR_NEED(r, 0, "no second render target"); }
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(DEV(r), 1, rt1), D3D_OK);
    IDirect3DDevice9_SetPixelShader(DEV(r), ps);
    dr_clear(&r, BLACK, 1.0f, 0);
    IDirect3DDevice9_Clear(DEV(r), 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, WHITE), D3D_OK);
    DWORD px[DW * DH];
    if (dr_read(&r, px)) PXEQ(px, 32, 32, RED);
    if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(DEV(r), DW, DH, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, NULL))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(DEV(r), rt1, sys)) && SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
            DWORD g = ((DWORD *)((BYTE *)lr.pBits + 32 * lr.Pitch))[32];
            t_check(g == GREEN, __FILE__, __LINE__, "second render target pixel", "got 0x%08lx want 0x%08lx", (unsigned long)g, (unsigned long)GREEN);
            IDirect3DSurface9_UnlockRect(sys);
        } else CHECK(0);
    }
    /* per-target write masks */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_COLORWRITEENABLE1, 0);
    IDirect3DDevice9_Clear(DEV(r), 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
    IDirect3DDevice9_SetRenderTarget(DEV(r), 1, NULL);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_COLORWRITEENABLE1, 0xf);
    IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    SAFE_RELEASE(sys); SAFE_RELEASE(rt1); SAFE_RELEASE(ps);
    DR_END(r);
}

/* SRGBWRITEENABLE converts linear shader output to sRGB when writing: 0.5 -> 0xbc (737/1000). */
TEST(d3d9_state_draw_dr_srgb_write)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRGBWRITEENABLE, TRUE);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, 0xff808080u), D3D_OK);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xffbcbcbcu, 3);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRGBWRITEENABLE, FALSE);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, 0xff808080u), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, 0xff808080u);
    DR_END(r);
}

/* SEPARATEALPHABLENDENABLE gives alpha its own factors and operation. */
TEST(d3d9_state_draw_dr_separate_alpha_blend)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRCBLEND, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DESTBLEND, D3DBLEND_ZERO);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_BLENDOP, D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
    dr_clear(&r, 0x30102030u, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, 0x40aabbccu), D3D_OK);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0x70aabbccu, 1);         /* colour replaced, alpha 0x30 + 0x40 */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_BLENDOPALPHA, D3DBLENDOP_MAX);
    dr_clear(&r, 0x30102030u, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, 0x40aabbccu), D3D_OK);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0x40aabbccu, 1);         /* alpha max(0x40, 0x30) */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_BLENDOPALPHA, D3DBLENDOP_REVSUBTRACT);
    dr_clear(&r, 0x30102030u, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, 0x40aabbccu), D3D_OK);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0x00aabbccu, 1);         /* dest - src = 0x30 - 0x40 clamps to 0 */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    DR_END(r);
}

/* Mip level selection: base level at 1:1, the next level when minified 2:1 (point mip filter), MAXMIPLEVEL clamps. */
TEST(d3d9_state_draw_dr_mip_selection)
{
    DR_OPEN(r);
    static const DWORD lv[4] = { RED, GREEN, BLUE, WHITE };
    IDirect3DTexture9 *tex = xtex(&r.d, 8, 8, 4, lv);
    DR_NEED(r, tex != NULL, "cannot create 4-level texture");
    DWORD px[DW * DH]; TV v[6];
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW | D3DFVF_TEX1);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)tex);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
    quad_tv(v, 0, 0, 8, 8, 0.5f);                                    /* 8 texels on 8 pixels: level 0 */
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TV)), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 4, 4, RED);
    quad_tv(v, 0, 0, 4, 4, 0.5f);                                    /* 8 texels on 4 pixels: level 1 */
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TV)), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 2, 2, GREEN);
    quad_tv(v, 0, 0, 2, 2, 0.5f);                                    /* 8 on 2: level 2 */
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TV)), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 1, 1, BLUE);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MAXMIPLEVEL, 1);   /* level 1 is the most detailed allowed */
    quad_tv(v, 0, 0, 8, 8, 0.5f);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TV)), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 4, 4, GREEN);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MAXMIPLEVEL, 0);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);   /* no mip filter: only level 0 */
    quad_tv(v, 0, 0, 2, 2, 0.5f);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TV)), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 1, 1, RED);
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(tex);
    DR_END(r);
}

/* clip() / texkill discards pixels with a negative argument. */
TEST(d3d9_state_draw_dr_ps_clip_kills_pixels)
{
    DR_OPEN(r);
    IDirect3DPixelShader9 *ps = xps(&r.d, "float4 main(float2 uv : TEXCOORD0) : COLOR { clip(uv.x - 0.5); return float4(1,1,1,1); }\n", "ps_2_0");
    DR_NEED(r, ps != NULL, "cannot build ps_2_0");
    DWORD px[DW * DH]; TV v[6];
    quad_tv(v, 0, 0, 64, 64, 0.5f);
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW | D3DFVF_TEX1);
    IDirect3DDevice9_SetPixelShader(DEV(r), ps);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TV)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 32, BLACK); PXEQ(px, 24, 32, BLACK); PXEQ(px, 40, 32, WHITE); PXEQ(px, 56, 32, WHITE); }
    IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    SAFE_RELEASE(ps);
    DR_END(r);
}

/* Point sprites: a POINTSPRITEENABLE point covers a POINTSIZE square and generates texture coordinates 0..1 across it. */
TEST(d3d9_state_draw_dr_point_sprite)
{
    DR_OPEN(r);
    static const DWORD lv[1] = { WHITE };
    (void)lv;
    IDirect3DTexture9 *tex = NULL; D3DLOCKED_RECT lr;
    HRESULT hr = IDirect3DDevice9_CreateTexture(DEV(r), 2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    DR_NEED(r, SUCCEEDED(hr), "cannot create texture");
    IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
    ((DWORD *)lr.pBits)[0] = RED; ((DWORD *)lr.pBits)[1] = GREEN;
    ((DWORD *)((BYTE *)lr.pBits + lr.Pitch))[0] = BLUE; ((DWORD *)((BYTE *)lr.pBits + lr.Pitch))[1] = WHITE;
    IDirect3DTexture9_UnlockRect(tex, 0);
    struct { float x, y, z, rhw; } p = { 32, 32, 0.5f, 1 };
    DWORD px[DW * DH];
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)tex);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_POINTSPRITEENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_POINTSIZE, f2d(16.0f));
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_POINTLIST, 1, &p, sizeof p), D3D_OK);
    if (dr_read(&r, px)) {
        PXEQ(px, 28, 28, RED); PXEQ(px, 36, 28, GREEN); PXEQ(px, 28, 36, BLUE); PXEQ(px, 36, 36, WHITE);
        PXEQ(px, 20, 20, BLACK); PXEQ(px, 44, 44, BLACK);
    }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_POINTSPRITEENABLE, FALSE);
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(tex);
    DR_END(r);
}

/* Lockable depth surfaces expose the stored depth: Clear(z) -> D16_LOCKABLE reads back z * 65535 (rounded). */
TEST(d3d9_state_draw_dr_lockable_depth_value)
{
    DR_OPEN(r);
    IDirect3DSurface9 *ds = NULL, *old = NULL; D3DLOCKED_RECT lr;
    HRESULT hr = IDirect3DDevice9_CreateDepthStencilSurface(DEV(r), DW, DH, D3DFMT_D16_LOCKABLE, D3DMULTISAMPLE_NONE, 0, FALSE, &ds, NULL);
    EXPECT_HR(hr, D3D_OK);
    DR_NEED(r, SUCCEEDED(hr), "no lockable depth format");
    IDirect3DDevice9_GetDepthStencilSurface(DEV(r), &old);
    EXPECT_HR(IDirect3DDevice9_SetDepthStencilSurface(DEV(r), ds), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(DEV(r), 0, NULL, D3DCLEAR_ZBUFFER, 0, 0.25f, 0), D3D_OK);
    EXPECT_HR(IDirect3DSurface9_LockRect(ds, &lr, NULL, D3DLOCK_READONLY), D3D_OK);
    if (lr.pBits) {
        unsigned short z = ((unsigned short *)lr.pBits)[0];
        t_check(z >= 16382 && z <= 16385, __FILE__, __LINE__, "D16 depth after Clear(0.25)", "got %u want ~16384", z);
        IDirect3DSurface9_UnlockRect(ds);
    }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    dr_full(&r, 0.75f, WHITE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_FALSE);
    if (SUCCEEDED(IDirect3DSurface9_LockRect(ds, &lr, NULL, D3DLOCK_READONLY))) {
        unsigned short z = ((unsigned short *)lr.pBits)[32 * (lr.Pitch / 2) + 32];
        t_check(z == 16384, __FILE__, __LINE__, "D16 depth unchanged by a failing draw", "got %u", z);     /* z=0.75 fails LESSEQUAL against 0.25 */
        IDirect3DSurface9_UnlockRect(ds);
    }
    IDirect3DDevice9_SetDepthStencilSurface(DEV(r), old);
    SAFE_RELEASE(old); SAFE_RELEASE(ds);
    DR_END(r);
}

/* NULLREF device (Windows 10): creation succeeds, but there is no real back buffer or viewport state. */
TEST(d3d9_state_draw_nullref_device_behaviour)
{
    SD d; HRESULT hr; D3DPRESENT_PARAMETERS pp;
    memset(&d, 0, sizeof d);
    if (!d3dh_Direct3DCreate9()) { t_skip("no Direct3DCreate9"); return; }
    d.d3d = d3dh_Direct3DCreate9()(D3D_SDK_VERSION);
    d.hwnd = d3dh_create_hidden_window();
    memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.BackBufferWidth = SD_W; pp.BackBufferHeight = SD_H; pp.hDeviceWindow = d.hwnd;
    hr = IDirect3D9_CreateDevice(d.d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, d.hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &d.dev);
    EXPECT_HR(hr, D3D_OK);
    if (FAILED(hr) || !d.dev) { sd_close(&d); return; }
    {
        D3DVIEWPORT9 vp; IDirect3DSurface9 *s = (void *)(ULONG_PTR)1; RECT sc; D3DDEVICE_CREATION_PARAMETERS cp;
        memset(&vp, 0xcc, sizeof vp);
        EXPECT_HR(IDirect3DDevice9_GetViewport(d.dev, &vp), D3D_OK);
        EXPECT_EQ(vp.X, 0); EXPECT_EQ(vp.Y, 0); EXPECT_EQ(vp.Width, 0); EXPECT_EQ(vp.Height, 0);
        CHECK(vp.MinZ == 0.0f && vp.MaxZ == 0.001f);       /* the empty viewport is {0,0,0,0, 0, 0.001} (minimum depth gap applied) */
        EXPECT_HR(IDirect3DDevice9_GetRenderTarget(d.dev, 0, &s), D3DERR_NOTFOUND);
        CHECK(s == NULL);
        s = (void *)(ULONG_PTR)1;
        EXPECT_HR(IDirect3DDevice9_GetDepthStencilSurface(d.dev, &s), D3DERR_NOTFOUND);
        memset(&sc, 0xcc, sizeof sc);
        EXPECT_HR(IDirect3DDevice9_GetScissorRect(d.dev, &sc), D3D_OK);
        EXPECT_EQ(sc.right, 0); EXPECT_EQ(sc.bottom, 0);
        EXPECT_HR(IDirect3DDevice9_GetCreationParameters(d.dev, &cp), D3D_OK);
        EXPECT_EQ(cp.DeviceType, D3DDEVTYPE_NULLREF);
        /* it still tracks state and accepts scenes */
        EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_CW), D3D_OK);
        { DWORD v = 0; IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v); EXPECT_EQ(v, D3DCULL_CW); }
        EXPECT_HR(IDirect3DDevice9_BeginScene(d.dev), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_EndScene(d.dev), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_Clear(d.dev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_Present(d.dev, NULL, NULL, NULL, NULL), D3D_OK);
        s = (void *)(ULONG_PTR)1;
        EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &s), D3DERR_INVALIDCALL);
        {
            IDirect3DTexture9 *t = NULL; IDirect3DSurface9 *rt = NULL; D3DCAPS9 c; D3DDISPLAYMODE dm;
            EXPECT_HR(IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL), D3D_OK);
            EXPECT_HR(IDirect3DDevice9_CreateRenderTarget(d.dev, 16, 16, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL), D3D_OK);
            SAFE_RELEASE(t); SAFE_RELEASE(rt);
            memset(&c, 0, sizeof c);
            EXPECT_HR(IDirect3DDevice9_GetDeviceCaps(d.dev, &c), D3D_OK);
            EXPECT_EQ(c.DeviceType, D3DDEVTYPE_NULLREF);
            EXPECT_EQ(c.VertexShaderVersion, 0xfffe0300u);
            EXPECT_EQ(c.PixelShaderVersion, 0xffff0300u);
            EXPECT_EQ(c.NumSimultaneousRTs, 4);
            memset(&dm, 0, sizeof dm);
            EXPECT_HR(IDirect3DDevice9_GetDisplayMode(d.dev, 0, &dm), D3D_OK);
            EXPECT_EQ(dm.Format, D3DFMT_X8R8G8B8);
        }
    }
    sd_close(&d);
}

/* Depth/stencil surface formats of the Basic Render Driver and the CheckDeviceFormat answers that go with them.  Lockable formats cannot be
 * created with Discard = TRUE. */
TEST(d3d9_state_draw_depth_format_matrix)
{
    SD_OPEN_R(d);
    static const struct { D3DFORMAT f; const char *n; int ok; int lockable; } t[] = {
        { D3DFMT_D16_LOCKABLE, "D16_LOCKABLE", 1, 1 }, { D3DFMT_D32, "D32", 0, 0 }, { D3DFMT_D15S1, "D15S1", 0, 0 }, { D3DFMT_D24S8, "D24S8", 1, 0 },
        { D3DFMT_D24X8, "D24X8", 1, 0 }, { D3DFMT_D24X4S4, "D24X4S4", 0, 0 }, { D3DFMT_D16, "D16", 1, 0 }, { D3DFMT_D32F_LOCKABLE, "D32F_LOCKABLE", 1, 1 },
        { D3DFMT_D24FS8, "D24FS8", 0, 0 }, { D3DFMT_D32_LOCKABLE, "D32_LOCKABLE", 0, 0 }, { D3DFMT_S8_LOCKABLE, "S8_LOCKABLE", 0, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDirect3DSurface9 *s = NULL; D3DLOCKED_RECT lr;
        HRESULT h1 = IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 64, 64, t[i].f, D3DMULTISAMPLE_NONE, 0, FALSE, &s, NULL);
        HRESULT hc = IDirect3D9_CheckDeviceFormat(d.d3d, 0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, t[i].f);
        t_check(h1 == (t[i].ok ? D3D_OK : D3DERR_INVALIDCALL), __FILE__, __LINE__, "CreateDepthStencilSurface", "%s got %s", t[i].n, d3dh_hr_name(h1));
        t_check(hc == (t[i].ok ? D3D_OK : D3DERR_NOTAVAILABLE), __FILE__, __LINE__, "CheckDeviceFormat(DEPTHSTENCIL)", "%s got %s", t[i].n, d3dh_hr_name(hc));
        if (s) {
            HRESULT hl = IDirect3DSurface9_LockRect(s, &lr, NULL, D3DLOCK_READONLY);
            t_check(SUCCEEDED(hl) == t[i].lockable, __FILE__, __LINE__, "depth surface lockability", "%s lock %s", t[i].n, d3dh_hr_name(hl));
            if (SUCCEEDED(hl)) IDirect3DSurface9_UnlockRect(s);
            IDirect3DSurface9_Release(s);
        }
        if (t[i].lockable) {
            s = NULL;
            t_check(IDirect3DDevice9_CreateDepthStencilSurface(d.dev, 64, 64, t[i].f, D3DMULTISAMPLE_NONE, 0, TRUE, &s, NULL) == D3DERR_INVALIDCALL, __FILE__, __LINE__, "lockable depth with Discard", "%s", t[i].n);
            SAFE_RELEASE(s);
        }
    }
    SD_END(d);
}
