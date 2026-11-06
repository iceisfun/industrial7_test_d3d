/* More rendering contracts (Windows 10 Basic Render Driver): vs_3_0 loops, render-target textures, texture-coordinate transforms,
 * cube and volume sampling. */
#include "drh.h"

static IDirect3DPixelShader9 *x2ps(SD *d, const char *src, const char *target)
{
    ID3DBlob *b = NULL; IDirect3DPixelShader9 *ps = NULL;
    if (FAILED(d3dh_compile(src, "main", target, 0, &b))) return NULL;
    if (FAILED(IDirect3DDevice9_CreatePixelShader(d->dev, d3dh_blob_ptr(b), &ps))) ps = NULL;
    SAFE_RELEASE(b);
    return ps;
}
static IDirect3DVertexShader9 *x2vs(SD *d, const char *src, const char *target)
{
    ID3DBlob *b = NULL; IDirect3DVertexShader9 *vs = NULL;
    if (FAILED(d3dh_compile(src, "main", target, 0, &b))) return NULL;
    if (FAILED(IDirect3DDevice9_CreateVertexShader(d->dev, d3dh_blob_ptr(b), &vs))) vs = NULL;
    SAFE_RELEASE(b);
    return vs;
}
static const int g_ix[6] = { 0, 1, 0, 1, 1, 0 }, g_iy[6] = { 0, 0, 1, 0, 1, 1 };

/* a vs_3_0 loop over four constant registers: the colour is the sum of c0..c3 (0.25 per channel pair = 0.5 grey) */
TEST(d3d9_state_draw_dr_vs30_loop_sums_constants)
{
    DR_OPEN(r);
    IDirect3DVertexShader9 *vs = x2vs(&r.d, "float4 c[4] : register(c0);\nvoid main(float4 p : POSITION, out float4 o : POSITION, out float4 col : COLOR0) { float4 acc = 0; for (int i = 0; i < 4; i++) acc += c[i]; o = float4(p.xyz, 1); col = acc; }\n", "vs_3_0");
    IDirect3DPixelShader9 *ps = x2ps(&r.d, "float4 main(float4 c : COLOR0) : COLOR0 { return c; }\n", "ps_3_0");
    DR_NEED(r, vs && ps, "cannot build sm3 shaders");
    float cs[16] = { 0.25f,0,0,0.25f, 0,0.25f,0,0.25f, 0,0,0.25f,0.25f, 0.25f,0.25f,0.25f,0.25f };
    struct { float x, y, z; } q[6] = { { -1, -1, 0.5f }, { 1, -1, 0.5f }, { -1, 1, 0.5f }, { 1, -1, 0.5f }, { 1, 1, 0.5f }, { -1, 1, 0.5f } };
    DWORD px[DW * DH];
    IDirect3DDevice9_SetVertexShaderConstantF(DEV(r), 0, cs, 4);
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZ);
    IDirect3DDevice9_SetVertexShader(DEV(r), vs); IDirect3DDevice9_SetPixelShader(DEV(r), ps);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 12), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, 0xff808080u);
    IDirect3DDevice9_SetVertexShader(DEV(r), NULL); IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    DR_END(r);
}

/* A D3DUSAGE_RENDERTARGET texture: cleared through its surface, sampled afterwards, and not lockable (default pool). */
TEST(d3d9_state_draw_dr_render_to_texture_then_sample)
{
    DR_OPEN(r);
    IDirect3DTexture9 *rtex = NULL; IDirect3DSurface9 *rs = NULL, *old = NULL; D3DLOCKED_RECT lr;
    HRESULT hr = IDirect3DDevice9_CreateTexture(DEV(r), 16, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &rtex, NULL);
    EXPECT_HR(hr, D3D_OK);
    DR_NEED(r, SUCCEEDED(hr), "no render target texture");
    typedef struct { float x, y, z, rhw, u, v; } TV; TV v[6]; DWORD px[DW * DH];
    IDirect3DTexture9_GetSurfaceLevel(rtex, 0, &rs);
    IDirect3DDevice9_GetRenderTarget(DEV(r), 0, &old);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(DEV(r), 0, rs), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_Clear(DEV(r), 0, NULL, D3DCLEAR_TARGET, 0xff336699, 1.0f, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetRenderTarget(DEV(r), 0, old), D3D_OK);
    for (int i = 0; i < 6; i++) { v[i].x = g_ix[i] ? 64.f : 0.f; v[i].y = g_iy[i] ? 64.f : 0.f; v[i].z = 0.5f; v[i].rhw = 1; v[i].u = (float)g_ix[i]; v[i].v = (float)g_iy[i]; }
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW | D3DFVF_TEX1);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)rtex);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TV)), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, 0xff336699u);
    EXPECT_HR(IDirect3DTexture9_LockRect(rtex, 0, &lr, NULL, D3DLOCK_READONLY), D3DERR_INVALIDCALL);
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(rs); SAFE_RELEASE(old); SAFE_RELEASE(rtex);
    DR_END(r);
}

/* D3DTSS_TEXTURETRANSFORMFLAGS = COUNT2 applies the stage's texture matrix to untransformed vertices: 2D translation lives in row 3 (_31/_32),
 * row 4 is ignored for COUNT2, and pretransformed (XYZRHW) vertices bypass the transform. */
TEST(d3d9_state_draw_dr_texture_transform_count2)
{
    DR_OPEN(r);
    IDirect3DTexture9 *tex = NULL; D3DLOCKED_RECT lr;
    HRESULT hr = IDirect3DDevice9_CreateTexture(DEV(r), 2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    DR_NEED(r, SUCCEEDED(hr), "cannot create texture");
    IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
    ((DWORD *)lr.pBits)[0] = RED; ((DWORD *)lr.pBits)[1] = GREEN;
    ((DWORD *)((BYTE *)lr.pBits + lr.Pitch))[0] = BLUE; ((DWORD *)((BYTE *)lr.pBits + lr.Pitch))[1] = WHITE;
    IDirect3DTexture9_UnlockRect(tex, 0);
    struct { float x, y, z, u, v; } q[6];
    struct { float x, y, z, rhw, u, v; } t[6];
    for (int i = 0; i < 6; i++) {
        q[i].x = g_ix[i] ? 1.f : -1.f; q[i].y = g_iy[i] ? -1.f : 1.f; q[i].z = 0.5f; q[i].u = (float)g_ix[i]; q[i].v = (float)g_iy[i];
        t[i].x = g_ix[i] ? 64.f : 0.f; t[i].y = g_iy[i] ? 64.f : 0.f; t[i].z = 0.5f; t[i].rhw = 1; t[i].u = (float)g_ix[i]; t[i].v = (float)g_iy[i];
    }
    DWORD px[DW * DH]; D3DMATRIX m;
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)tex);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZ | D3DFVF_TEX1);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 20), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, RED); PXEQ(px, 48, 16, GREEN); PXEQ(px, 16, 48, BLUE); PXEQ(px, 48, 48, WHITE); }
    memset(&m, 0, sizeof m); m._11 = m._22 = m._33 = m._44 = 1.0f; m._31 = 0.5f;               /* u += 0.5 */
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_TEXTURE0, &m);
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 20), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, GREEN); PXEQ(px, 48, 16, RED); PXEQ(px, 16, 48, WHITE); PXEQ(px, 48, 48, BLUE); }
    m._31 = 0.0f; m._41 = 0.5f;                                                                  /* row 4 is not used by COUNT2 */
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_TEXTURE0, &m);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 20), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, RED); PXEQ(px, 48, 16, GREEN); PXEQ(px, 16, 48, BLUE); PXEQ(px, 48, 48, WHITE); }
    m._41 = 0.0f; m._11 = 0.5f;                                                                  /* u *= 0.5: only the left texel column */
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_TEXTURE0, &m);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 20), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, RED); PXEQ(px, 48, 16, RED); PXEQ(px, 16, 48, BLUE); PXEQ(px, 48, 48, BLUE); }
    memset(&m, 0, sizeof m); m._12 = 1.0f; m._21 = 1.0f; m._33 = 1.0f; m._44 = 1.0f;            /* swap u and v */
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_TEXTURE0, &m);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 20), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, RED); PXEQ(px, 48, 16, BLUE); PXEQ(px, 16, 48, GREEN); PXEQ(px, 48, 48, WHITE); }
    /* pretransformed vertices ignore the texture matrix */
    memset(&m, 0, sizeof m); m._11 = m._22 = m._33 = m._44 = 1.0f; m._31 = 0.5f;
    IDirect3DDevice9_SetTransform(DEV(r), D3DTS_TEXTURE0, &m);
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW | D3DFVF_TEX1);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, t, 24), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, RED); PXEQ(px, 48, 16, GREEN); PXEQ(px, 16, 48, BLUE); PXEQ(px, 48, 48, WHITE); }
    IDirect3DDevice9_SetTextureStageState(DEV(r), 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(tex);
    DR_END(r);
}

/* Cube map sampling by direction: +x -x +y -y +z -z select their faces. */
TEST(d3d9_state_draw_dr_cube_texture_faces)
{
    DR_OPEN(r);
    IDirect3DPixelShader9 *ps = x2ps(&r.d, "samplerCUBE s : register(s0);\nfloat4 main(float3 d : TEXCOORD0) : COLOR { return texCUBE(s, d); }\n", "ps_2_0");
    IDirect3DCubeTexture9 *cube = NULL;
    HRESULT hr = IDirect3DDevice9_CreateCubeTexture(DEV(r), 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cube, NULL);
    EXPECT_HR(hr, D3D_OK);
    if (FAILED(hr) || !ps) { SAFE_RELEASE(cube); SAFE_RELEASE(ps); DR_NEED(r, 0, "cannot build cube sampling"); }
    static const DWORD face[6] = { 0xffff0000, 0xff00ffff, 0xff00ff00, 0xffff00ff, 0xff0000ff, 0xffffff00 };
    static const float dirs[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    struct { float x, y, z, rhw, u, v, w; } v[6]; DWORD px[DW * DH];
    for (int f = 0; f < 6; f++) {
        D3DLOCKED_RECT lr;
        IDirect3DCubeTexture9_LockRect(cube, (D3DCUBEMAP_FACES)f, 0, &lr, NULL, 0);
        for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) ((DWORD *)((BYTE *)lr.pBits + y * lr.Pitch))[x] = face[f];
        IDirect3DCubeTexture9_UnlockRect(cube, (D3DCUBEMAP_FACES)f, 0);
    }
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0));
    IDirect3DDevice9_SetPixelShader(DEV(r), ps);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)cube);
    for (int f = 0; f < 6; f++) {
        for (int i = 0; i < 6; i++) { v[i].x = g_ix[i] ? 64.f : 0.f; v[i].y = g_iy[i] ? 64.f : 0.f; v[i].z = 0.5f; v[i].rhw = 1; v[i].u = dirs[f][0]; v[i].v = dirs[f][1]; v[i].w = dirs[f][2]; }
        dr_clear(&r, BLACK, 1.0f, 0);
        EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof v[0]), D3D_OK);
        if (dr_read(&r, px)) t_check(SD_PX(px, DW, 32, 32) == face[f], __FILE__, __LINE__, "cube face", "direction %d got 0x%08lx want 0x%08lx", f, (unsigned long)SD_PX(px, DW, 32, 32), (unsigned long)face[f]);
    }
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL); IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    SAFE_RELEASE(cube); SAFE_RELEASE(ps);
    DR_END(r);
}

/* Volume texture sampling: w selects the depth slice (point filter): 4 slices, centres at 1/8, 3/8, 5/8, 7/8; 0.5 lies between slices 1 and 2 and picks slice 2. */
TEST(d3d9_state_draw_dr_volume_texture_slices)
{
    DR_OPEN(r);
    IDirect3DPixelShader9 *ps = x2ps(&r.d, "sampler3D s : register(s0);\nfloat4 main(float3 d : TEXCOORD0) : COLOR { return tex3D(s, d); }\n", "ps_2_0");
    IDirect3DVolumeTexture9 *vol = NULL; D3DLOCKED_BOX lb;
    HRESULT hr = IDirect3DDevice9_CreateVolumeTexture(DEV(r), 2, 2, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &vol, NULL);
    EXPECT_HR(hr, D3D_OK);
    if (FAILED(hr) || !ps) { SAFE_RELEASE(vol); SAFE_RELEASE(ps); DR_NEED(r, 0, "cannot build volume sampling"); }
    static const DWORD slice[4] = { RED, GREEN, BLUE, WHITE };
    static const float ws[5] = { 0.125f, 0.375f, 0.625f, 0.875f, 0.5f };
    static const DWORD want[5] = { RED, GREEN, BLUE, WHITE, BLUE };
    struct { float x, y, z, rhw, u, v, w; } v[6]; DWORD px[DW * DH];
    IDirect3DVolumeTexture9_LockBox(vol, 0, &lb, NULL, 0);
    for (int z = 0; z < 4; z++) for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) ((DWORD *)((BYTE *)lb.pBits + z * lb.SlicePitch + y * lb.RowPitch))[x] = slice[z];
    IDirect3DVolumeTexture9_UnlockBox(vol, 0);
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0));
    IDirect3DDevice9_SetPixelShader(DEV(r), ps);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)vol);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    for (int s = 0; s < 5; s++) {
        for (int i = 0; i < 6; i++) { v[i].x = g_ix[i] ? 64.f : 0.f; v[i].y = g_iy[i] ? 64.f : 0.f; v[i].z = 0.5f; v[i].rhw = 1; v[i].u = 0.25f; v[i].v = 0.25f; v[i].w = ws[s]; }
        dr_clear(&r, BLACK, 1.0f, 0);
        EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof v[0]), D3D_OK);
        if (dr_read(&r, px)) t_check(SD_PX(px, DW, 32, 32) == want[s], __FILE__, __LINE__, "volume slice", "w=%g got 0x%08lx want 0x%08lx", ws[s], (unsigned long)SD_PX(px, DW, 32, 32), (unsigned long)want[s]);
    }
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL); IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    SAFE_RELEASE(vol); SAFE_RELEASE(ps);
    DR_END(r);
}
