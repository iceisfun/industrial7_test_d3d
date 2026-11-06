/* D3D9 drawing from buffers, DrawPrimitive* protocol rules, shaders, instancing, ProcessVertices, patches. */
#include "d3dh.h"
#include "drh.h"
#include <math.h>

static IDirect3DVertexBuffer9 *make_vb(SD *d, const void *data, UINT size, DWORD fvf)
{
    IDirect3DVertexBuffer9 *vb = NULL;
    void *p;
    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(d->dev, size, 0, fvf, D3DPOOL_DEFAULT, &vb, NULL))) return NULL;
    if (FAILED(IDirect3DVertexBuffer9_Lock(vb, 0, 0, &p, 0))) { IDirect3DVertexBuffer9_Release(vb); return NULL; }
    memcpy(p, data, size);
    IDirect3DVertexBuffer9_Unlock(vb);
    return vb;
}
static IDirect3DIndexBuffer9 *make_ib(SD *d, const void *data, UINT size, D3DFORMAT fmt)
{
    IDirect3DIndexBuffer9 *ib = NULL;
    void *p;
    if (FAILED(IDirect3DDevice9_CreateIndexBuffer(d->dev, size, 0, fmt, D3DPOOL_DEFAULT, &ib, NULL))) return NULL;
    if (FAILED(IDirect3DIndexBuffer9_Lock(ib, 0, 0, &p, 0))) { IDirect3DIndexBuffer9_Release(ib); return NULL; }
    memcpy(p, data, size);
    IDirect3DIndexBuffer9_Unlock(ib);
    return ib;
}
static HRESULT scene_draw(SD *d, HRESULT (*fn)(IDirect3DDevice9 *, void *), void *arg)
{
    HRESULT hr = IDirect3DDevice9_BeginScene(d->dev), h2;
    if (FAILED(hr)) return hr;
    hr = fn(d->dev, arg);
    h2 = IDirect3DDevice9_EndScene(d->dev);
    return FAILED(hr) ? hr : h2;
}

typedef struct { D3DPRIMITIVETYPE t; UINT a, b, c, d, e; } DA;
static HRESULT do_dp(IDirect3DDevice9 *dev, void *p) { DA *a = p; return IDirect3DDevice9_DrawPrimitive(dev, a->t, a->a, a->b); }
static HRESULT do_dip(IDirect3DDevice9 *dev, void *p) { DA *a = p; return IDirect3DDevice9_DrawIndexedPrimitive(dev, a->t, (INT)a->a, a->b, a->c, a->d, a->e); }

TEST(d3d9_state_draw_dr_vertex_buffer_start_vertex)
{
    DR_OPEN(r);
    TLV v[6];
    tlv_set(&v[0], 4, 4, 0.5f, RED);   tlv_set(&v[1], 28, 4, 0.5f, RED);   tlv_set(&v[2], 4, 28, 0.5f, RED);
    tlv_set(&v[3], 36, 36, 0.5f, GREEN); tlv_set(&v[4], 60, 36, 0.5f, GREEN); tlv_set(&v[5], 36, 60, 0.5f, GREEN);
    IDirect3DVertexBuffer9 *vb = make_vb(&r.d, v, sizeof v, TLV_FVF);
    DR_NEED(r, vb != NULL, "cannot create vertex buffer");
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(DEV(r), 0, vb, 0, sizeof(TLV)), D3D_OK);
    DWORD px[DW * DH];
    DA a = { D3DPT_TRIANGLELIST, 0, 2, 0, 0, 0 };
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dp, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, RED); PXEQ(px, 40, 40, GREEN); }
    a.a = 3; a.b = 1;                                 /* start at vertex 3: only the green triangle */
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dp, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, BLACK); PXEQ(px, 40, 40, GREEN); }
    a.a = 0; a.b = 1;
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dp, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, RED); PXEQ(px, 40, 40, BLACK); }
    /* a stream offset behaves like a start vertex */
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(DEV(r), 0, vb, 3 * sizeof(TLV), sizeof(TLV)), D3D_OK);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dp, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, BLACK); PXEQ(px, 40, 40, GREEN); }
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, NULL, 0, 0);
    SAFE_RELEASE(vb);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_indexed_base_vertex_and_start_index)
{
    DR_OPEN(r);
    TLV v[8];
    tlv_rect(v, 8, 8, 56, 56, 0.5f, WHITE);           /* writes 6 vertices, only the 4 corners are used below */
    TLV q[8];
    tlv_set(&q[0], 8, 8, 0.5f, WHITE);  tlv_set(&q[1], 56, 8, 0.5f, WHITE);  tlv_set(&q[2], 8, 56, 0.5f, WHITE);  tlv_set(&q[3], 56, 56, 0.5f, WHITE);
    tlv_set(&q[4], 8, 8, 0.5f, GREEN);  tlv_set(&q[5], 56, 8, 0.5f, GREEN);  tlv_set(&q[6], 8, 56, 0.5f, GREEN);  tlv_set(&q[7], 56, 56, 0.5f, GREEN);
    /* first 6 indices: whole quad; next 3: a single triangle */
    static const WORD idx[9] = { 0, 1, 2, 2, 1, 3, 0, 1, 2 };
    IDirect3DVertexBuffer9 *vb = make_vb(&r.d, q, sizeof q, TLV_FVF);
    IDirect3DIndexBuffer9 *ib = make_ib(&r.d, idx, sizeof idx, D3DFMT_INDEX16);
    if (!vb || !ib) { SAFE_RELEASE(vb); SAFE_RELEASE(ib); DR_NEED(r, 0, "cannot create buffers"); }
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, vb, 0, sizeof(TLV));
    EXPECT_HR(IDirect3DDevice9_SetIndices(DEV(r), ib), D3D_OK);
    DWORD px[DW * DH];
    DA a = { D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2 };      /* Base, MinIndex, NumVertices, StartIndex, PrimitiveCount */
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dip, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, WHITE); PXEQ(px, 48, 48, WHITE); PXEQ(px, 16, 48, WHITE); }
    a.a = 4;                                           /* BaseVertexIndex 4 selects the green copy */
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dip, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, GREEN); PXEQ(px, 48, 48, GREEN); PXEQ(px, 16, 48, GREEN); }
    a.a = 0; a.d = 6; a.e = 1;                         /* StartIndex 6: the single triangle 0,1,2 */
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dip, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, WHITE); PXEQ(px, 16, 40, WHITE); PXEQ(px, 48, 48, BLACK); }
    a.a = 4;
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dip, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, GREEN); PXEQ(px, 48, 48, BLACK); PXEQ(px, 16, 40, GREEN); }
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, NULL, 0, 0);
    IDirect3DDevice9_SetIndices(DEV(r), NULL);
    SAFE_RELEASE(vb); SAFE_RELEASE(ib);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_indexed_32bit)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.MaxVertexIndex > 0xffff, "no 32 bit indices");
    TLV q[4];
    tlv_set(&q[0], 8, 8, 0.5f, WHITE);  tlv_set(&q[1], 56, 8, 0.5f, WHITE);  tlv_set(&q[2], 8, 56, 0.5f, WHITE);  tlv_set(&q[3], 56, 56, 0.5f, WHITE);
    static const DWORD idx[6] = { 0, 1, 2, 2, 1, 3 };
    IDirect3DVertexBuffer9 *vb = make_vb(&r.d, q, sizeof q, TLV_FVF);
    IDirect3DIndexBuffer9 *ib = make_ib(&r.d, idx, sizeof idx, D3DFMT_INDEX32);
    if (!vb || !ib) { SAFE_RELEASE(vb); SAFE_RELEASE(ib); DR_NEED(r, 0, "cannot create buffers"); }
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, vb, 0, sizeof(TLV));
    IDirect3DDevice9_SetIndices(DEV(r), ib);
    DA a = { D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2 };
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dip, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, WHITE); PXEQ(px, 48, 48, WHITE); }
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, NULL, 0, 0);
    IDirect3DDevice9_SetIndices(DEV(r), NULL);
    SAFE_RELEASE(vb); SAFE_RELEASE(ib);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_indexed_up_16_and_32)
{
    DR_OPEN(r);
    TLV q[4];
    tlv_set(&q[0], 8, 8, 0.5f, WHITE);  tlv_set(&q[1], 56, 8, 0.5f, WHITE);  tlv_set(&q[2], 8, 56, 0.5f, WHITE);  tlv_set(&q[3], 56, 56, 0.5f, WHITE);
    static const WORD i16[6] = { 0, 1, 2, 2, 1, 3 };
    static const DWORD i32[6] = { 0, 1, 2, 2, 1, 3 };
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    IDirect3DDevice9_BeginScene(DEV(r));
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitiveUP(DEV(r), D3DPT_TRIANGLELIST, 0, 4, 2, i16, D3DFMT_INDEX16, q, sizeof(TLV)), D3D_OK);
    IDirect3DDevice9_EndScene(DEV(r));
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, WHITE); PXEQ(px, 48, 48, WHITE); PXEQ(px, 4, 4, BLACK); }
    if (r.d.caps.MaxVertexIndex > 0xffff) {
        dr_clear(&r, BLACK, 1.0f, 0);
        IDirect3DDevice9_BeginScene(DEV(r));
        EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitiveUP(DEV(r), D3DPT_TRIANGLELIST, 0, 4, 2, i32, D3DFMT_INDEX32, q, sizeof(TLV)), D3D_OK);
        IDirect3DDevice9_EndScene(DEV(r));
        if (dr_read(&r, px)) { PXEQ(px, 16, 16, WHITE); PXEQ(px, 48, 48, WHITE); }
    }
    /* MinVertexIndex / NumVertices restrict nothing the indices do not reference */
    dr_clear(&r, BLACK, 1.0f, 0);
    IDirect3DDevice9_BeginScene(DEV(r));
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitiveUP(DEV(r), D3DPT_TRIANGLELIST, 0, 4, 1, i16, D3DFMT_INDEX16, q, sizeof(TLV)), D3D_OK);
    IDirect3DDevice9_EndScene(DEV(r));
    if (dr_read(&r, px)) { PXEQ(px, 16, 16, WHITE); PXEQ(px, 48, 48, BLACK); }
    DR_END(r);
}

/* Windows 10: a declaration that puts POSITIONT in stream 0 and anything in another stream is rejected (E_FAIL). */
TEST(d3d9_state_draw_decl_positiont_multi_stream_rejected)
{
    SD_OPEN(d);
    IDirect3DVertexDeclaration9 *decl = (void *)(ULONG_PTR)0x1;
    D3DVERTEXELEMENT9 el4[3] = { { 0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITIONT, 0 }, { 1, 0, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0 }, D3DDECL_END() };
    D3DVERTEXELEMENT9 el3[3] = { { 0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITIONT, 0 }, { 1, 0, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0 }, D3DDECL_END() };
    D3DVERTEXELEMENT9 ok[3] = { { 0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0 }, { 1, 0, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0 }, D3DDECL_END() };
    EXPECT_HR(IDirect3DDevice9_CreateVertexDeclaration(d.dev, el4, &decl), E_FAIL);
    EXPECT_HR(IDirect3DDevice9_CreateVertexDeclaration(d.dev, el3, &decl), E_FAIL);
    decl = NULL;
    EXPECT_HR(IDirect3DDevice9_CreateVertexDeclaration(d.dev, ok, &decl), D3D_OK);
    SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_dr_multiple_streams)
{
    DR_OPEN(r);
    /* stream 0: clip-space positions (identity transforms), stream 1: diffuse colours */
    typedef struct { float x, y, z; } P3;
    P3 pos[6];
    DWORD col[6] = { RED, RED, RED, GREEN, GREEN, GREEN };
    static const float xs[6] = { 4, 28, 4, 36, 60, 36 }, ys[6] = { 4, 4, 28, 36, 36, 60 };
    for (int i = 0; i < 6; i++) { pos[i].x = xs[i] / 32.0f - 1.0f; pos[i].y = 1.0f - ys[i] / 32.0f; pos[i].z = 0.5f; }
    D3DVERTEXELEMENT9 el[3] = { { 0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 }, { 1, 0, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0 }, D3DDECL_END() };
    IDirect3DVertexDeclaration9 *decl = NULL;
    IDirect3DVertexBuffer9 *vb0 = make_vb(&r.d, pos, sizeof pos, 0), *vb1 = make_vb(&r.d, col, sizeof col, 0);
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(DEV(r), el, &decl);
    EXPECT_HR(hr, D3D_OK);
    if (!vb0 || !vb1 || FAILED(hr)) { SAFE_RELEASE(vb0); SAFE_RELEASE(vb1); SAFE_RELEASE(decl); DR_NEED(r, 0, "cannot create buffers/declaration"); }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetVertexDeclaration(DEV(r), decl);
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, vb0, 0, sizeof(P3));
    IDirect3DDevice9_SetStreamSource(DEV(r), 1, vb1, 0, sizeof(DWORD));
    DA a = { D3DPT_TRIANGLELIST, 0, 2, 0, 0, 0 };
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(scene_draw(&r.d, do_dp, &a), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, RED); PXEQ(px, 40, 40, GREEN); PXEQ(px, 8, 40, BLACK); PXEQ(px, 40, 8, BLACK); }
    /* a stride of 0 repeats the first colour for every vertex */
    IDirect3DDevice9_SetStreamSource(DEV(r), 1, vb1, 0, 0);
    dr_clear(&r, BLACK, 1.0f, 0);
    hr = scene_draw(&r.d, do_dp, &a);
    t_note("DrawPrimitive with a zero stride stream -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && dr_read(&r, px)) { PXEQ(px, 8, 8, RED); PXEQ(px, 40, 40, RED); }
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, NULL, 0, 0);
    IDirect3DDevice9_SetStreamSource(DEV(r), 1, NULL, 0, 0);
    IDirect3DDevice9_SetVertexDeclaration(DEV(r), NULL);
    SAFE_RELEASE(vb0); SAFE_RELEASE(vb1); SAFE_RELEASE(decl);
    DR_END(r);
}

/* ---- protocol rules (device type independent) ------------------------------------------------------ */
TEST(d3d9_state_draw_draw_without_declaration_fails)
{
    SD_OPEN_DS(d);
    TLV q[4];
    tlv_set(&q[0], 8, 8, 0.5f, WHITE);  tlv_set(&q[1], 56, 8, 0.5f, WHITE);  tlv_set(&q[2], 8, 56, 0.5f, WHITE);  tlv_set(&q[3], 56, 56, 0.5f, WHITE);
    static const WORD idx[6] = { 0, 1, 2, 2, 1, 3 };
    IDirect3DVertexBuffer9 *vb = make_vb(&d, q, sizeof q, TLV_FVF);
    IDirect3DIndexBuffer9 *ib = make_ib(&d, idx, sizeof idx, D3DFMT_INDEX16);
    if (!vb || !ib) { SAFE_RELEASE(vb); SAFE_RELEASE(ib); SD_NEED(d, 0, "cannot create buffers"); }
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, sizeof(TLV));
    IDirect3DDevice9_SetVertexDeclaration(d.dev, NULL);
    IDirect3DDevice9_BeginScene(d.dev);
    EXPECT_HR(IDirect3DDevice9_DrawPrimitive(d.dev, D3DPT_TRIANGLELIST, 0, 2), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_DrawPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 2, q, sizeof(TLV)), D3DERR_INVALIDCALL);
    IDirect3DDevice9_SetIndices(d.dev, NULL);
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitive(d.dev, D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2), D3DERR_INVALIDCALL);
    IDirect3DDevice9_SetIndices(d.dev, ib);
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitive(d.dev, D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 0, 4, 2, idx, D3DFMT_INDEX16, q, sizeof(TLV)), D3DERR_INVALIDCALL);
    IDirect3DDevice9_EndScene(d.dev);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0);
    IDirect3DDevice9_SetIndices(d.dev, NULL);
    SAFE_RELEASE(vb); SAFE_RELEASE(ib);
    SD_END(d);
}

TEST(d3d9_state_draw_draw_up_zero_stride_fails)
{
    SD_OPEN_DS(d);
    TLV q[4];
    tlv_set(&q[0], 8, 8, 0.5f, WHITE);  tlv_set(&q[1], 56, 8, 0.5f, WHITE);  tlv_set(&q[2], 8, 56, 0.5f, WHITE);  tlv_set(&q[3], 56, 56, 0.5f, WHITE);
    static const WORD idx[6] = { 0, 1, 2, 2, 1, 3 };
    IDirect3DDevice9_SetFVF(d.dev, TLV_FVF);
    IDirect3DDevice9_BeginScene(d.dev);
    EXPECT_HR(IDirect3DDevice9_DrawPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 2, q, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 0, 4, 2, idx, D3DFMT_INDEX16, q, 0), D3DERR_INVALIDCALL);
    IDirect3DDevice9_EndScene(d.dev);
    SD_END(d);
}

TEST(d3d9_state_draw_draw_up_unbinds_stream_zero_and_indices)
{
    SD_OPEN_DS(d);
    static const D3DVERTEXELEMENT9 el[] = { { 0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 }, { 0, 12, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0 }, D3DDECL_END() };
    struct { float x, y, z; DWORD c; } q[4] = { { -1, -1, 0.5f, WHITE }, { 1, -1, 0.5f, WHITE }, { -1, 1, 0.5f, WHITE }, { 1, 1, 0.5f, WHITE } };
    static const WORD idx[6] = { 0, 1, 2, 2, 1, 3 };
    IDirect3DVertexDeclaration9 *decl = NULL;
    IDirect3DVertexBuffer9 *vb = make_vb(&d, q, sizeof q, 0), *got = NULL;
    IDirect3DIndexBuffer9 *ib = make_ib(&d, idx, sizeof idx, D3DFMT_INDEX16), *gib = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    if (!vb || !ib || FAILED(hr)) { SAFE_RELEASE(vb); SAFE_RELEASE(ib); SAFE_RELEASE(decl); SD_NEED(d, 0, "cannot create resources"); }
    UINT off, stride;
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetVertexDeclaration(d.dev, decl);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, 16);
    IDirect3DDevice9_BeginScene(d.dev);
    EXPECT_HR(IDirect3DDevice9_DrawPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 2, q, 16), D3D_OK);
    IDirect3DDevice9_GetStreamSource(d.dev, 0, &got, &off, &stride);
    CHECK(got == NULL); EXPECT_EQ(off, 0); EXPECT_EQ(stride, 0);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, 16);
    IDirect3DDevice9_SetIndices(d.dev, ib);
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 0, 4, 2, idx, D3DFMT_INDEX16, q, 16), D3D_OK);
    IDirect3DDevice9_GetStreamSource(d.dev, 0, &got, &off, &stride);
    CHECK(got == NULL); EXPECT_EQ(off, 0); EXPECT_EQ(stride, 0);
    IDirect3DDevice9_GetIndices(d.dev, &gib);
    CHECK(gib == NULL);
    IDirect3DDevice9_EndScene(d.dev);
    SAFE_RELEASE(got); SAFE_RELEASE(gib);
    SAFE_RELEASE(vb); SAFE_RELEASE(ib); SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_draw_up_reset_not_recorded_in_stateblock)
{
    SD_OPEN_DS(d);
    static const D3DVERTEXELEMENT9 el[] = { { 0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 }, { 0, 12, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0 }, D3DDECL_END() };
    struct { float x, y, z; DWORD c; } q[4] = { { -1, -1, 0.5f, WHITE }, { 1, -1, 0.5f, WHITE }, { -1, 1, 0.5f, WHITE }, { 1, 1, 0.5f, WHITE } };
    static const WORD idx[6] = { 0, 1, 2, 2, 1, 3 };
    IDirect3DVertexDeclaration9 *decl = NULL;
    IDirect3DVertexBuffer9 *vb = make_vb(&d, q, sizeof q, 0), *got = NULL;
    IDirect3DIndexBuffer9 *ib = make_ib(&d, idx, sizeof idx, D3DFMT_INDEX16), *gib = NULL;
    IDirect3DStateBlock9 *sb = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    if (!vb || !ib || FAILED(hr)) { SAFE_RELEASE(vb); SAFE_RELEASE(ib); SAFE_RELEASE(decl); SD_NEED(d, 0, "cannot create resources"); }
    UINT off, stride;
    IDirect3DDevice9_SetVertexDeclaration(d.dev, decl);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, 16);
    IDirect3DDevice9_SetIndices(d.dev, ib);
    IDirect3DDevice9_BeginScene(d.dev);
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 0, 4, 2, idx, D3DFMT_INDEX16, q, 16), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    IDirect3DDevice9_EndScene(d.dev);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, 16);
    IDirect3DDevice9_SetIndices(d.dev, ib);
    if (sb) {
        EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
        IDirect3DDevice9_GetStreamSource(d.dev, 0, &got, &off, &stride);
        CHECK(got == vb);
        SAFE_RELEASE(got);
        IDirect3DDevice9_GetIndices(d.dev, &gib);
        CHECK(gib == ib);
        SAFE_RELEASE(gib);
    }
    IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0);
    IDirect3DDevice9_SetIndices(d.dev, NULL);
    SAFE_RELEASE(sb); SAFE_RELEASE(vb); SAFE_RELEASE(ib); SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_draw_null_stream_with_declaration)
{
    SD_OPEN_DS(d);
    static const D3DVERTEXELEMENT9 el[] = { { 0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 }, { 0, 12, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0 }, D3DDECL_END() };
    static const WORD idx[6] = { 0, 1, 2, 2, 1, 3 };
    IDirect3DVertexDeclaration9 *decl = NULL;
    IDirect3DIndexBuffer9 *ib = make_ib(&d, idx, sizeof idx, D3DFMT_INDEX16);
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    if (!ib || FAILED(hr)) { SAFE_RELEASE(ib); SAFE_RELEASE(decl); SD_NEED(d, 0, "cannot create resources"); }
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetVertexDeclaration(d.dev, decl);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0);
    IDirect3DDevice9_BeginScene(d.dev);
    IDirect3DDevice9_SetIndices(d.dev, ib);
    /* Windows 10 (HAL/WARP): drawing with a declaration, NULL stream 0 AND an index buffer dereferences NULL (0xc0000005), so only the
       no-index-buffer case is run; it is rejected before anything is read. */
    IDirect3DDevice9_SetIndices(d.dev, NULL);
    EXPECT_HR(IDirect3DDevice9_DrawIndexedPrimitive(d.dev, D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2), D3DERR_INVALIDCALL);
    IDirect3DDevice9_EndScene(d.dev);
    IDirect3DDevice9_SetIndices(d.dev, NULL);
    SAFE_RELEASE(ib); SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_draw_outside_scene)
{
    SD_OPEN_DS(d);
    struct { float x, y, z, w; DWORD c; } q[3] = { { 8, 8, 0.5f, 1, WHITE }, { 56, 8, 0.5f, 1, WHITE }, { 8, 56, 0.5f, 1, WHITE } };
    IDirect3DDevice9_SetFVF(d.dev, TLV_FVF);
    HRESULT hr = IDirect3DDevice9_DrawPrimitiveUP(d.dev, D3DPT_TRIANGLELIST, 1, q, 20);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);  /* Windows 10: drawing outside BeginScene/EndScene is rejected */
    SD_END(d);
}

TEST(d3d9_state_draw_draw_primitive_count_limits_noted)
{
    SD_OPEN_DS(d);
    EXPECT_EQ(d.caps.MaxPrimitiveCount, 0x7fffff);       /* Windows 10 Basic Render Driver */
    EXPECT_EQ(d.caps.MaxVertexIndex, 0xffffff);
    EXPECT_EQ(d.caps.MaxStreams, 16);
    EXPECT_EQ(d.caps.MaxStreamStride, 255);
    SD_END(d);
}

/* ---- shaders ------------------------------------------------------------------------------------------ */
static IDirect3DPixelShader9 *compile_ps(SD *d, const char *src, const char *target)
{
    ID3DBlob *b = NULL;
    IDirect3DPixelShader9 *ps = NULL;
    if (FAILED(d3dh_compile(src, "main", target, 0, &b))) return NULL;
    if (FAILED(IDirect3DDevice9_CreatePixelShader(d->dev, d3dh_blob_ptr(b), &ps))) ps = NULL;
    SAFE_RELEASE(b);
    return ps;
}
static IDirect3DVertexShader9 *compile_vs(SD *d, const char *src, const char *target)
{
    ID3DBlob *b = NULL;
    IDirect3DVertexShader9 *vs = NULL;
    if (FAILED(d3dh_compile(src, "main", target, 0, &b))) return NULL;
    if (FAILED(IDirect3DDevice9_CreateVertexShader(d->dev, d3dh_blob_ptr(b), &vs))) vs = NULL;
    SAFE_RELEASE(b);
    return vs;
}

TEST(d3d9_state_draw_dr_pixel_shader_constant)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.PixelShaderVersion >= D3DPS_VERSION(2, 0), "no ps_2_0");
    IDirect3DPixelShader9 *ps = compile_ps(&r.d, "float4 c : register(c0);\nfloat4 main() : COLOR { return c; }\n", "ps_2_0");
    DR_NEED(r, ps != NULL, "cannot build ps_2_0");
    DWORD px[DW * DH];
    float c1[4] = { 0.25f, 0.5f, 0.75f, 1.0f }, c2[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    EXPECT_HR(IDirect3DDevice9_SetPixelShader(DEV(r), ps), D3D_OK);
    IDirect3DDevice9_SetPixelShaderConstantF(DEV(r), 0, c1, 1);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, WHITE), D3D_OK);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff4080bfu, 1);
    IDirect3DDevice9_SetPixelShaderConstantF(DEV(r), 0, c2, 1);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_full(&r, 0.5f, WHITE);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, RED, 0);
    /* unbinding returns to the fixed function pipeline (vertex colour) */
    IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_full(&r, 0.5f, 0xff102030u);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, 0xff102030u);
    SAFE_RELEASE(ps);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_vertex_shader_constant)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.PixelShaderVersion >= D3DPS_VERSION(2, 0) && r.d.caps.VertexShaderVersion >= D3DVS_VERSION(2, 0), "no sm2 shaders");
    IDirect3DVertexShader9 *vs = compile_vs(&r.d, "float4 s : register(c0);\nfloat4 main(float4 p : POSITION) : POSITION { return float4(p.xy * s.xy, p.z, 1.0); }\n", "vs_2_0");
    IDirect3DPixelShader9 *ps = compile_ps(&r.d, "float4 main() : COLOR { return float4(0.0, 1.0, 0.0, 1.0); }\n", "ps_2_0");
    if (!vs || !ps) { SAFE_RELEASE(vs); SAFE_RELEASE(ps); DR_NEED(r, 0, "cannot build shaders"); }
    struct { float x, y, z; } q[6] = { { -1, -1, 0.5f }, { 1, -1, 0.5f }, { -1, 1, 0.5f }, { 1, -1, 0.5f }, { 1, 1, 0.5f }, { -1, 1, 0.5f } };
    float s1[4] = { 1, 1, 1, 1 }, s2[4] = { 0.5f, 0.5f, 1, 1 };
    DWORD px[DW * DH];
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZ);
    EXPECT_HR(IDirect3DDevice9_SetVertexShader(DEV(r), vs), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetPixelShader(DEV(r), ps), D3D_OK);
    IDirect3DDevice9_SetVertexShaderConstantF(DEV(r), 0, s1, 1);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 12), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 32, 32, GREEN); PXEQ(px, 2, 2, GREEN); PXEQ(px, 61, 61, GREEN); PXEQ(px, 2, 61, GREEN); PXEQ(px, 61, 2, GREEN); }
    IDirect3DDevice9_SetVertexShaderConstantF(DEV(r), 0, s2, 1);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 12);
    if (dr_read(&r, px)) {
        PXEQ(px, 32, 32, GREEN); PXEQ(px, 20, 20, GREEN); PXEQ(px, 44, 44, GREEN); PXEQ(px, 8, 32, BLACK); PXEQ(px, 56, 32, BLACK);
        int n = sd_count_is(px, DW, 0, 0, DW, DH, GREEN);
        CHECKF(n >= 32 * 32 - 70 && n <= 32 * 32 + 70, "scaled quad area %d", n);
    }
    IDirect3DDevice9_SetVertexShader(DEV(r), NULL);
    IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_shader_model3_pair)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.PixelShaderVersion >= D3DPS_VERSION(3, 0) && r.d.caps.VertexShaderVersion >= D3DVS_VERSION(3, 0), "no sm3 shaders");
    IDirect3DVertexShader9 *vs = compile_vs(&r.d, "void main(float4 p : POSITION, out float4 o : POSITION, out float4 c : COLOR0) { o = float4(p.xyz, 1.0); c = float4(1.0, 0.0, 0.0, 1.0); }\n", "vs_3_0");
    IDirect3DPixelShader9 *ps = compile_ps(&r.d, "float4 main(float4 c : COLOR0) : COLOR0 { return c; }\n", "ps_3_0");
    if (!vs || !ps) { SAFE_RELEASE(vs); SAFE_RELEASE(ps); DR_NEED(r, 0, "cannot build sm3 shaders"); }
    struct { float x, y, z; } q[6] = { { -1, -1, 0.5f }, { 1, -1, 0.5f }, { -1, 1, 0.5f }, { 1, -1, 0.5f }, { 1, 1, 0.5f }, { -1, 1, 0.5f } };
    DWORD px[DW * DH];
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZ);
    IDirect3DDevice9_SetVertexShader(DEV(r), vs);
    IDirect3DDevice9_SetPixelShader(DEV(r), ps);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, q, 12), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 32, 32, RED); PXEQ(px, 3, 3, RED); PXEQ(px, 60, 60, RED); }
    IDirect3DDevice9_SetVertexShader(DEV(r), NULL);
    IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_pixel_shader_texture_sample)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.PixelShaderVersion >= D3DPS_VERSION(2, 0), "no ps_2_0");
    IDirect3DPixelShader9 *ps = compile_ps(&r.d, "sampler2D s : register(s0);\nfloat4 main(float2 uv : TEXCOORD0) : COLOR { return tex2D(s, uv); }\n", "ps_2_0");
    DR_NEED(r, ps != NULL, "cannot build ps_2_0");
    typedef struct { float x, y, z, rhw, u, v; } TT;
    TT v[6];
    static const int ix[6] = { 0, 1, 0, 1, 1, 0 }, iy[6] = { 0, 0, 1, 0, 1, 1 };
    for (int i = 0; i < 6; i++) { v[i].x = ix[i] ? 64.f : 0.f; v[i].y = iy[i] ? 64.f : 0.f; v[i].z = 0.5f; v[i].rhw = 1; v[i].u = (float)ix[i]; v[i].v = (float)iy[i]; }
    IDirect3DTexture9 *tex = NULL;
    D3DLOCKED_RECT lr;
    HRESULT hr = IDirect3DDevice9_CreateTexture(DEV(r), 2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    if (FAILED(hr)) { SAFE_RELEASE(ps); DR_NEED(r, 0, "cannot create texture"); }
    IDirect3DTexture9_LockRect(tex, 0, &lr, NULL, 0);
    ((DWORD *)lr.pBits)[0] = RED; ((DWORD *)lr.pBits)[1] = GREEN;
    ((DWORD *)((BYTE *)lr.pBits + lr.Pitch))[0] = BLUE; ((DWORD *)((BYTE *)lr.pBits + lr.Pitch))[1] = WHITE;
    IDirect3DTexture9_UnlockRect(tex, 0);
    IDirect3DDevice9_SetFVF(DEV(r), D3DFVF_XYZRHW | D3DFVF_TEX1);
    IDirect3DDevice9_SetPixelShader(DEV(r), ps);
    IDirect3DDevice9_SetTexture(DEV(r), 0, (IDirect3DBaseTexture9 *)tex);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(DEV(r), 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TT)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, RED); PXEQ(px, 56, 8, GREEN); PXEQ(px, 8, 56, BLUE); PXEQ(px, 56, 56, WHITE); }
    IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    IDirect3DDevice9_SetTexture(DEV(r), 0, NULL);
    SAFE_RELEASE(tex); SAFE_RELEASE(ps);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_instancing)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.VertexShaderVersion >= D3DVS_VERSION(3, 0) && r.d.caps.PixelShaderVersion >= D3DPS_VERSION(3, 0), "no sm3 shaders");
    IDirect3DVertexShader9 *vs = compile_vs(&r.d, "void main(float3 p : POSITION, float2 off : TEXCOORD0, out float4 o : POSITION, out float4 c : COLOR0) { o = float4(p.xy + off, p.z, 1.0); c = float4(1.0, 1.0, 1.0, 1.0); }\n", "vs_3_0");
    IDirect3DPixelShader9 *ps = compile_ps(&r.d, "float4 main(float4 c : COLOR0) : COLOR0 { return c; }\n", "ps_3_0");
    if (!vs || !ps) { SAFE_RELEASE(vs); SAFE_RELEASE(ps); DR_NEED(r, 0, "cannot build shaders"); }
    /* a small quad as geometry (stream 0), two instance offsets (stream 1) */
    float geo[4][3] = { { -0.25f, -0.25f, 0.5f }, { 0.25f, -0.25f, 0.5f }, { -0.25f, 0.25f, 0.5f }, { 0.25f, 0.25f, 0.5f } };
    float inst[2][2] = { { -0.5f, 0.0f }, { 0.5f, 0.0f } };
    static const WORD idx[6] = { 0, 1, 2, 2, 1, 3 };
    D3DVERTEXELEMENT9 el[3] = { { 0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 }, { 1, 0, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 }, D3DDECL_END() };
    IDirect3DVertexDeclaration9 *decl = NULL;
    IDirect3DVertexBuffer9 *vb0 = make_vb(&r.d, geo, sizeof geo, 0), *vb1 = make_vb(&r.d, inst, sizeof inst, 0);
    IDirect3DIndexBuffer9 *ib = make_ib(&r.d, idx, sizeof idx, D3DFMT_INDEX16);
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(DEV(r), el, &decl);
    if (!vb0 || !vb1 || !ib || FAILED(hr)) { SAFE_RELEASE(vb0); SAFE_RELEASE(vb1); SAFE_RELEASE(ib); SAFE_RELEASE(decl); SAFE_RELEASE(vs); SAFE_RELEASE(ps); DR_NEED(r, 0, "cannot create resources"); }
    IDirect3DDevice9_SetVertexDeclaration(DEV(r), decl);
    IDirect3DDevice9_SetVertexShader(DEV(r), vs);
    IDirect3DDevice9_SetPixelShader(DEV(r), ps);
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, vb0, 0, 12);
    IDirect3DDevice9_SetStreamSource(DEV(r), 1, vb1, 0, 8);
    IDirect3DDevice9_SetIndices(DEV(r), ib);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(DEV(r), 0, D3DSTREAMSOURCE_INDEXEDDATA | 2), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(DEV(r), 1, D3DSTREAMSOURCE_INSTANCEDATA | 1), D3D_OK);
    DA a = { D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2 };
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    hr = scene_draw(&r.d, do_dip, &a);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr) && dr_read(&r, px)) {
        PXEQ(px, 16, 32, WHITE);      /* left instance: centre (-0.5, 0) */
        PXEQ(px, 48, 32, WHITE);      /* right instance */
        PXEQ(px, 32, 32, BLACK);
        PXEQ(px, 16, 8, BLACK);
    }
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(DEV(r), 0, 1), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(DEV(r), 1, 1), D3D_OK);
    IDirect3DDevice9_SetVertexShader(DEV(r), NULL);
    IDirect3DDevice9_SetPixelShader(DEV(r), NULL);
    IDirect3DDevice9_SetVertexDeclaration(DEV(r), NULL);
    IDirect3DDevice9_SetStreamSource(DEV(r), 0, NULL, 0, 0);
    IDirect3DDevice9_SetStreamSource(DEV(r), 1, NULL, 0, 0);
    IDirect3DDevice9_SetIndices(DEV(r), NULL);
    SAFE_RELEASE(vb0); SAFE_RELEASE(vb1); SAFE_RELEASE(ib); SAFE_RELEASE(decl); SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    DR_END(r);
}

/* ---- ProcessVertices / patches ------------------------------------------------------------------------------- */
TEST(d3d9_state_draw_process_vertices)
{
    SD_OPEN_DS(d);
    struct { float x, y, z; } src[2] = { { 0.0f, 0.0f, 0.5f }, { -1.0f, 1.0f, 0.25f } };
    struct { float x, y, z, rhw; } dst[2];
    D3DMATRIX m;
    IDirect3DVertexBuffer9 *svb = make_vb(&d, src, sizeof src, D3DFVF_XYZ), *dvb = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexBuffer(d.dev, sizeof dst, D3DUSAGE_SOFTWAREPROCESSING, D3DFVF_XYZRHW, D3DPOOL_SYSTEMMEM, &dvb, NULL);
    if (!svb || FAILED(hr)) { SAFE_RELEASE(svb); SAFE_RELEASE(dvb); SD_NEED(d, 0, "cannot create buffers"); }
    memset(&m, 0, sizeof m); m._11 = m._22 = m._33 = m._44 = 1.0f;
    IDirect3DDevice9_SetTransform(d.dev, D3DTS_WORLD, &m);
    IDirect3DDevice9_SetTransform(d.dev, D3DTS_VIEW, &m);
    IDirect3DDevice9_SetTransform(d.dev, D3DTS_PROJECTION, &m);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetFVF(d.dev, D3DFVF_XYZ);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, svb, 0, 12);
    hr = IDirect3DDevice9_ProcessVertices(d.dev, 0, 0, 2, dvb, NULL, 0);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) {
        void *p = NULL;
        if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(dvb, 0, 0, &p, D3DLOCK_READONLY))) {
            memcpy(dst, p, sizeof dst);
            IDirect3DVertexBuffer9_Unlock(dvb);
            /* viewport transform of the default 64x64 viewport */
            t_check(fabsf(dst[0].x - 32.0f) < 0.01f && fabsf(dst[0].y - 32.0f) < 0.01f && fabsf(dst[0].z - 0.5f) < 0.001f && fabsf(dst[0].rhw - 1.0f) < 0.001f,
                    __FILE__, __LINE__, "ProcessVertices vertex 0", "got %g %g %g %g", dst[0].x, dst[0].y, dst[0].z, dst[0].rhw);
            t_check(fabsf(dst[1].x - 0.0f) < 0.01f && fabsf(dst[1].y - 0.0f) < 0.01f && fabsf(dst[1].z - 0.25f) < 0.001f,
                    __FILE__, __LINE__, "ProcessVertices vertex 1", "got %g %g %g %g", dst[1].x, dst[1].y, dst[1].z, dst[1].rhw);
        }
    }
    IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0);
    SAFE_RELEASE(svb); SAFE_RELEASE(dvb);
    SD_END(d);
}

TEST(d3d9_state_draw_patches)
{
    SD_OPEN_DS(d);
    CHECK(!(d.caps.DevCaps & D3DDEVCAPS_RTPATCHES));          /* Windows 10 Basic Render Driver reports none of the patch caps */
    CHECK(!(d.caps.DevCaps & D3DDEVCAPS_NPATCHES));
    CHECK(!(d.caps.DevCaps & D3DDEVCAPS_QUINTICRTPATCHES));
    float segs[4] = { 1, 1, 1, 1 };
    D3DRECTPATCH_INFO ri = { 0, 0, 4, 4, 4, D3DBASIS_BEZIER, D3DDEGREE_CUBIC };
    D3DTRIPATCH_INFO ti = { 0, 10, D3DBASIS_BEZIER, D3DDEGREE_CUBIC };
    float cp[16][3];
    for (int i = 0; i < 16; i++) { cp[i][0] = (float)(i % 4) / 3.0f * 2 - 1; cp[i][1] = (float)(i / 4) / 3.0f * 2 - 1; cp[i][2] = 0.5f; }
    IDirect3DVertexBuffer9 *vb = make_vb(&d, cp, sizeof cp, D3DFVF_XYZ);
    if (!vb) SD_NEED(d, 0, "cannot create vertex buffer");
    IDirect3DDevice9_SetFVF(d.dev, D3DFVF_XYZ);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, 12);
    IDirect3DDevice9_BeginScene(d.dev);
    HRESULT hr = IDirect3DDevice9_DrawRectPatch(d.dev, 1, segs, &ri);
    EXPECT_HR(hr, E_FAIL);              /* patches are not supported: E_FAIL, not INVALIDCALL */
    HRESULT h2 = IDirect3DDevice9_DrawTriPatch(d.dev, 2, segs, &ti);
    EXPECT_HR(h2, E_FAIL);
    IDirect3DDevice9_EndScene(d.dev);
    HRESULT h3 = IDirect3DDevice9_DeletePatch(d.dev, 1);
    EXPECT_HR(h3, D3DERR_INVALIDCALL);
    h3 = IDirect3DDevice9_DeletePatch(d.dev, 999);
    EXPECT_HR(h3, D3DERR_INVALIDCALL);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0);
    SAFE_RELEASE(vb);
    SD_END(d);
}

TEST(d3d9_state_draw_npatch_mode)
{
    SD_OPEN(d);
    float g = d2f(0xdeadbeefu);
    EXPECT_HR(IDirect3DDevice9_SetNPatchMode(d.dev, 0.0f), D3D_OK);
    g = IDirect3DDevice9_GetNPatchMode(d.dev);
    CHECK(g == 0.0f);
    HRESULT hr = IDirect3DDevice9_SetNPatchMode(d.dev, 3.0f);
    EXPECT_HR(hr, D3D_OK);              /* accepted although NPATCHES is not reported; the value is stored */
    CHECK(IDirect3DDevice9_GetNPatchMode(d.dev) == 3.0f);
    IDirect3DDevice9_SetNPatchMode(d.dev, 0.0f);
    SD_END(d);
}

/* ---- Direct3D 9Ex ------------------------------------------------------------------------------------------------ */
typedef struct { USHORT X, Y, Width, Height; } CRDESC;
typedef struct { USHORT SrcRectIndex, Reserved; SHORT X, Y; } CRDEST;
static int open_ex(IDirect3D9Ex **d3d, IDirect3DDevice9Ex **dev, HWND *hwnd, D3DDEVTYPE *type)
{
    static const D3DDEVTYPE ty[2] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF };
    *d3d = NULL; *dev = NULL;
    if (!d3dh_Direct3DCreate9Ex()) { t_skip("no Direct3DCreate9Ex"); return 0; }
    if (FAILED(d3dh_Direct3DCreate9Ex()(D3D_SDK_VERSION, d3d)) || !*d3d) { t_skip("Direct3DCreate9Ex failed"); return 0; }
    *hwnd = d3dh_create_hidden_window();
    for (int i = 0; i < 2 && !*dev; i++) {
        D3DPRESENT_PARAMETERS pp;
        memset(&pp, 0, sizeof pp);
        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.BackBufferWidth = SD_W; pp.BackBufferHeight = SD_H; pp.hDeviceWindow = *hwnd;
        if (SUCCEEDED(IDirect3D9Ex_CreateDeviceEx(*d3d, D3DADAPTER_DEFAULT, ty[i], *hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, NULL, dev)) && *dev) *type = ty[i];
        else *dev = NULL;
    }
    if (!*dev) { t_skip("no rendering D3D9Ex device"); IDirect3D9Ex_Release(*d3d); *d3d = NULL; d3dh_destroy_window(*hwnd); return 0; }
    return 1;
}
#define EX_OPEN() IDirect3D9Ex *d3d; IDirect3DDevice9Ex *dev; HWND hw; D3DDEVTYPE ty; if (!open_ex(&d3d, &dev, &hw, &ty)) return
#define EX_END() do { IDirect3DDevice9Ex_Release(dev); IDirect3D9Ex_Release(d3d); d3dh_destroy_window(hw); } while (0)

TEST(d3d9_state_draw_ex_present_ex)
{
    EX_OPEN();
    EXPECT_HR(IDirect3DDevice9Ex_Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0xff102030u, 1.0f, 0), D3D_OK);
    HRESULT hr = IDirect3DDevice9Ex_PresentEx(dev, NULL, NULL, NULL, NULL, 0);
    t_check(hr == S_OK || hr == S_PRESENT_OCCLUDED || hr == S_PRESENT_MODE_CHANGED, __FILE__, __LINE__, "PresentEx", "got %s", d3dh_hr_name(hr));
    RECT r = { 0, 0, 32, 32 };
    hr = IDirect3DDevice9Ex_PresentEx(dev, &r, &r, NULL, NULL, 0);
    EXPECT_HR(hr, S_OK);
    hr = IDirect3DDevice9Ex_PresentEx(dev, NULL, NULL, NULL, NULL, D3DPRESENT_DONOTWAIT);
    t_check(hr == S_OK || hr == S_PRESENT_OCCLUDED || hr == D3DERR_WASSTILLDRAWING || hr == S_PRESENT_MODE_CHANGED, __FILE__, __LINE__, "PresentEx(DONOTWAIT)", "got %s", d3dh_hr_name(hr));
    hr = IDirect3DDevice9Ex_CheckDeviceState(dev, hw);
    t_check(hr == S_OK || hr == S_PRESENT_OCCLUDED || hr == S_PRESENT_MODE_CHANGED, __FILE__, __LINE__, "CheckDeviceState", "got %s", d3dh_hr_name(hr));
    EX_END();
}

TEST(d3d9_state_draw_ex_present_ex_in_scene)
{
    EX_OPEN();
    EXPECT_HR(IDirect3DDevice9Ex_BeginScene(dev), D3D_OK);
    HRESULT hr = IDirect3DDevice9Ex_PresentEx(dev, NULL, NULL, NULL, NULL, 0);
    EXPECT_HR(hr, D3DERR_INVALIDCALL);
    IDirect3DDevice9Ex_EndScene(dev);
    EX_END();
}

TEST(d3d9_state_draw_ex_compose_rects_constraints)
{
    EX_OPEN();
    IDirect3DSurface9 *a = NULL, *b = NULL, *argb = NULL;
    IDirect3DVertexBuffer9 *src = NULL, *dst = NULL;
    CRDESC rects[2] = { { 0, 0, 8, 8 }, { 8, 0, 8, 8 } };
    CRDEST drects[2] = { { 0, 0, 0, 0 }, { 1, 0, 8, 0 } };
    void *p;
    HRESULT h1 = IDirect3DDevice9Ex_CreateRenderTarget(dev, 32, 32, D3DFMT_A1, D3DMULTISAMPLE_NONE, 0, FALSE, &a, NULL);
    HRESULT h2 = IDirect3DDevice9Ex_CreateOffscreenPlainSurface(dev, 32, 32, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &argb, NULL);
    EXPECT_HR(h1, D3DERR_INVALIDCALL);   /* D3DFMT_A1 render targets cannot be created on the Basic Render Driver, so ComposeRects' A1 path is unreachable */
    HRESULT hv1 = IDirect3DDevice9Ex_CreateVertexBuffer(dev, sizeof rects, 0, 0, D3DPOOL_DEFAULT, &src, NULL);
    HRESULT hv2 = IDirect3DDevice9Ex_CreateVertexBuffer(dev, sizeof drects, 0, 0, D3DPOOL_DEFAULT, &dst, NULL);
    if (SUCCEEDED(hv1) && SUCCEEDED(IDirect3DVertexBuffer9_Lock(src, 0, 0, &p, 0))) { memcpy(p, rects, sizeof rects); IDirect3DVertexBuffer9_Unlock(src); }
    if (SUCCEEDED(hv2) && SUCCEEDED(IDirect3DVertexBuffer9_Lock(dst, 0, 0, &p, 0))) { memcpy(p, drects, sizeof drects); IDirect3DVertexBuffer9_Unlock(dst); }
    if (SUCCEEDED(h2) && SUCCEEDED(hv1) && SUCCEEDED(hv2)) {
        HRESULT hr = IDirect3DDevice9Ex_ComposeRects(dev, argb, argb, src, 2, dst, D3DCOMPOSERECTS_COPY, 0, 0);
        CHECKF(FAILED(hr), "ComposeRects with the same surface as source and destination succeeded (%s)", d3dh_hr_name(hr));
        EXPECT_HR(hr, D3DERR_INVALIDCALL);
        IDirect3DSurface9 *argb2 = NULL;
        if (SUCCEEDED(IDirect3DDevice9Ex_CreateOffscreenPlainSurface(dev, 32, 32, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &argb2, NULL))) {
            hr = IDirect3DDevice9Ex_ComposeRects(dev, argb, argb2, src, 2, dst, D3DCOMPOSERECTS_COPY, 0, 0);
            CHECKF(FAILED(hr), "ComposeRects with D3DFMT_A8R8G8B8 surfaces succeeded (%s)", d3dh_hr_name(hr));
            EXPECT_HR(hr, D3DERR_INVALIDCALL);
            SAFE_RELEASE(argb2);
        }
        if (SUCCEEDED(h1) && a) {
            IDirect3DSurface9 *a2 = NULL;
            if (SUCCEEDED(IDirect3DDevice9Ex_CreateRenderTarget(dev, 32, 32, D3DFMT_A1, D3DMULTISAMPLE_NONE, 0, FALSE, &a2, NULL))) {
                hr = IDirect3DDevice9Ex_ComposeRects(dev, a, a2, src, 2, dst, D3DCOMPOSERECTS_COPY, 0, 0);
                t_note("ComposeRects(A1 surfaces, COPY) -> %s", d3dh_hr_name(hr));
                hr = IDirect3DDevice9Ex_ComposeRects(dev, a, a2, src, 2, dst, (D3DCOMPOSERECTSOP)0, 0, 0);
                t_note("ComposeRects(A1 surfaces, invalid operation) -> %s", d3dh_hr_name(hr));
                SAFE_RELEASE(a2);
            }
        }
    }
    SAFE_RELEASE(a); SAFE_RELEASE(argb); SAFE_RELEASE(src); SAFE_RELEASE(dst);
    EX_END();
}

TEST(d3d9_state_draw_ex_state_matches_d3d9_defaults)
{
    EX_OPEN();
    DWORD v;
    EXPECT_HR(IDirect3DDevice9Ex_GetRenderState(dev, D3DRS_CULLMODE, &v), D3D_OK);   EXPECT_EQ(v, D3DCULL_CCW);
    EXPECT_HR(IDirect3DDevice9Ex_GetRenderState(dev, D3DRS_ZFUNC, &v), D3D_OK);      EXPECT_EQ(v, D3DCMP_LESSEQUAL);
    EXPECT_HR(IDirect3DDevice9Ex_GetRenderState(dev, D3DRS_LIGHTING, &v), D3D_OK);   EXPECT_EQ(v, TRUE);
    EXPECT_HR(IDirect3DDevice9Ex_GetRenderState(dev, D3DRS_TEXTUREFACTOR, &v), D3D_OK); EXPECT_EQ(v, 0xffffffffu);
    EXPECT_HR(IDirect3DDevice9Ex_EndScene(dev), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9Ex_BeginScene(dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9Ex_BeginScene(dev), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9Ex_EndScene(dev), D3D_OK);
    EX_END();
}
