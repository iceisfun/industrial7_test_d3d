/* D3D9 drawing: primitive types, culling, fill modes, shading, blending, tests; golden pixels via GetRenderTargetData. */
#include "d3dh.h"
#include "drh.h"

TEST(d3d9_state_draw_dr_flat_quad)
{
    DR_OPEN(r);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_rect(&r, 8, 8, 56, 56, 0.5f, 0xff3366ccu), D3D_OK);
    DWORD px[DW * DH];
    if (dr_read(&r, px)) {
        PXEQ(px, 32, 32, 0xff3366ccu); PXEQ(px, 10, 10, 0xff3366ccu); PXEQ(px, 53, 53, 0xff3366ccu);
        PXEQ(px, 10, 53, 0xff3366ccu); PXEQ(px, 53, 10, 0xff3366ccu);
        PXEQ(px, 4, 32, BLACK); PXEQ(px, 60, 32, BLACK); PXEQ(px, 32, 4, BLACK); PXEQ(px, 32, 60, BLACK);
        int n = sd_count_is(px, DW, 0, 0, DW, DH, 0xff3366ccu);
        CHECKF(n >= 48 * 48 - 2 * 48 && n <= 48 * 48 + 2 * 48, "covered pixel count %d (expected about 2304)", n);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_full_target_covered)
{
    DR_OPEN(r);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, 0xff00ff80u), D3D_OK);
    DWORD px[DW * DH];
    if (dr_read(&r, px)) CHECKF(sd_count_is(px, DW, 0, 0, DW, DH, 0xff00ff80u) == DW * DH, "not every pixel was covered");
    DR_END(r);
}

TEST(d3d9_state_draw_dr_triangle_coverage)
{
    DR_OPEN(r);
    dr_clear(&r, BLACK, 1.0f, 0);
    TLV v[3];
    tlv_set(&v[0], 4, 4, 0.5f, WHITE); tlv_set(&v[1], 60, 4, 0.5f, WHITE); tlv_set(&v[2], 4, 60, 0.5f, WHITE);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, v, sizeof(TLV)), D3D_OK);
    DWORD px[DW * DH];
    if (dr_read(&r, px)) {
        PXEQ(px, 10, 10, WHITE); PXEQ(px, 20, 20, WHITE); PXEQ(px, 8, 50, WHITE); PXEQ(px, 50, 8, WHITE);
        PXEQ(px, 50, 50, BLACK); PXEQ(px, 40, 40, BLACK); PXEQ(px, 2, 2, BLACK);
        int n = sd_count_is(px, DW, 0, 0, DW, DH, WHITE);
        CHECKF(n > 1450 && n < 1750, "triangle area %d (expected about 1568)", n);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_trianglelist_counts)
{
    DR_OPEN(r);
    /* two disjoint triangles, drawn with PrimitiveCount 1 and then 2 */
    TLV v[6];
    tlv_set(&v[0], 4, 4, 0.5f, RED);   tlv_set(&v[1], 28, 4, 0.5f, RED);   tlv_set(&v[2], 4, 28, 0.5f, RED);
    tlv_set(&v[3], 36, 36, 0.5f, GREEN); tlv_set(&v[4], 60, 36, 0.5f, GREEN); tlv_set(&v[5], 36, 60, 0.5f, GREEN);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, RED); PXEQ(px, 40, 40, BLACK); }
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 8, 8, RED); PXEQ(px, 40, 40, GREEN); PXEQ(px, 50, 10, BLACK); }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_trianglestrip_counts)
{
    DR_OPEN(r);
    /* strip over a quad A B C D: count 1 draws ABC, count 2 adds BCD */
    TLV v[4];
    tlv_set(&v[0], 4, 4, 0.5f, WHITE); tlv_set(&v[1], 60, 4, 0.5f, WHITE);
    tlv_set(&v[2], 4, 60, 0.5f, WHITE); tlv_set(&v[3], 60, 60, 0.5f, WHITE);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLESTRIP, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 12, 12, WHITE); PXEQ(px, 52, 52, BLACK); }
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLESTRIP, 2, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        PXEQ(px, 12, 12, WHITE); PXEQ(px, 52, 52, WHITE); PXEQ(px, 12, 52, WHITE); PXEQ(px, 52, 12, WHITE); PXEQ(px, 2, 32, BLACK);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_trianglefan_counts)
{
    DR_OPEN(r);
    /* fan around the first vertex: A B D C -> ABD, ADC */
    TLV v[4];
    tlv_set(&v[0], 4, 4, 0.5f, WHITE); tlv_set(&v[1], 60, 4, 0.5f, WHITE);
    tlv_set(&v[2], 60, 60, 0.5f, WHITE); tlv_set(&v[3], 4, 60, 0.5f, WHITE);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLEFAN, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 52, 12, WHITE); PXEQ(px, 12, 52, BLACK); }
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLEFAN, 2, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 52, 12, WHITE); PXEQ(px, 12, 52, WHITE); PXEQ(px, 32, 32, WHITE); PXEQ(px, 2, 2, BLACK); }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_line_list)
{
    DR_OPEN(r);
    TLV v[4];
    tlv_set(&v[0], 8, 20, 0.5f, WHITE); tlv_set(&v[1], 56, 20, 0.5f, WHITE);
    tlv_set(&v[2], 8, 44, 0.5f, GREEN); tlv_set(&v[3], 56, 44, 0.5f, GREEN);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_LINELIST, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        int rows = 0;
        for (int y = 18; y <= 22; y++) if (SD_PX(px, DW, 32, y) == WHITE) rows++;
        CHECKF(rows >= 1 && rows <= 2, "line is %d pixels thick at x=32", rows);
        int other = 0;
        for (int y = 40; y <= 48; y++) other += SD_PX(px, DW, 32, y) != BLACK;
        CHECK(other == 0);
        int total = sd_count_not(px, DW, 0, 0, DW, DH, BLACK);
        CHECKF(total >= 40 && total <= 110, "line pixel count %d", total);
    }
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_LINELIST, 2, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        int a = 0, b = 0;
        for (int y = 38; y <= 50; y++) if (SD_PX(px, DW, 32, y) == GREEN) b++;
        for (int y = 14; y <= 26; y++) if (SD_PX(px, DW, 32, y) == WHITE) a++;
        CHECK(a >= 1); CHECK(b >= 1);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_line_strip)
{
    DR_OPEN(r);
    TLV v[3];
    tlv_set(&v[0], 8, 8, 0.5f, WHITE); tlv_set(&v[1], 56, 8, 0.5f, WHITE); tlv_set(&v[2], 56, 56, 0.5f, WHITE);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_LINESTRIP, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        int vert = 0;
        for (int y = 30; y <= 34; y++) for (int x = 54; x <= 58; x++) vert += SD_PX(px, DW, x, y) != BLACK;
        CHECKF(vert == 0, "second segment drawn for count 1");
    }
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_LINESTRIP, 2, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        int horiz = 0, vert = 0;
        for (int y = 6; y <= 10; y++) horiz += SD_PX(px, DW, 30, y) == WHITE;
        for (int x = 54; x <= 58; x++) vert += SD_PX(px, DW, x, 32) == WHITE;
        CHECK(horiz >= 1); CHECK(vert >= 1);
        PXEQ(px, 20, 40, BLACK);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_point_list)
{
    DR_OPEN(r);
    TLV v[3];
    tlv_set(&v[0], 10, 10, 0.5f, WHITE); tlv_set(&v[1], 30, 40, 0.5f, WHITE); tlv_set(&v[2], 50, 20, 0.5f, WHITE);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_POINTLIST, 3, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        for (int i = 0; i < 3; i++) {
            int n = 0;
            for (int y = (int)v[i].y - 2; y <= (int)v[i].y + 2; y++) for (int x = (int)v[i].x - 2; x <= (int)v[i].x + 2; x++) n += SD_PX(px, DW, x, y) == WHITE;
            CHECKF(n >= 1 && n <= 4, "point %d covers %d pixels", i, n);
        }
        int total = sd_count_not(px, DW, 0, 0, DW, DH, BLACK);
        CHECKF(total >= 3 && total <= 12, "total point pixels %d", total);
    }
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_POINTLIST, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) { int total = sd_count_not(px, DW, 0, 0, DW, DH, BLACK); CHECKF(total >= 1 && total <= 4, "one point -> %d pixels", total); }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_point_size)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.MaxPointSize >= 8.0f, "MaxPointSize below 8");
    TLV v[1];
    tlv_set(&v[0], 32, 32, 0.5f, WHITE);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_POINTSIZE, f2d(8.0f));
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_POINTSIZE_MIN, f2d(1.0f));
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_POINTSIZE_MAX, f2d(r.d.caps.MaxPointSize < 64.0f ? r.d.caps.MaxPointSize : 64.0f));
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_POINTLIST, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        int n = sd_count_not(px, DW, 0, 0, DW, DH, BLACK);
        CHECKF(n >= 40 && n <= 100, "8 pixel point covers %d pixels", n);
        PXEQ(px, 32, 32, WHITE);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_cull_modes)
{
    DR_OPEN(r);
    /* on screen (y down) A->B->C below is clockwise */
    TLV cw[3], ccw[3];
    tlv_set(&cw[0], 8, 8, 0.5f, WHITE); tlv_set(&cw[1], 56, 8, 0.5f, WHITE); tlv_set(&cw[2], 8, 56, 0.5f, WHITE);
    tlv_set(&ccw[0], 8, 8, 0.5f, WHITE); tlv_set(&ccw[1], 8, 56, 0.5f, WHITE); tlv_set(&ccw[2], 56, 8, 0.5f, WHITE);
    static const struct { DWORD mode; int cw_drawn, ccw_drawn; } t[] = {
        { D3DCULL_NONE, 1, 1 }, { D3DCULL_CCW, 1, 0 }, { D3DCULL_CW, 0, 1 },
    };
    DWORD px[DW * DH];
    for (unsigned i = 0; i < 3; i++) {
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_CULLMODE, t[i].mode);
        dr_clear(&r, BLACK, 1.0f, 0);
        EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, cw, sizeof(TLV)), D3D_OK);
        if (dr_read(&r, px)) t_check((SD_PX(px, DW, 16, 16) == WHITE) == t[i].cw_drawn, __FILE__, __LINE__, "clockwise triangle", "cull mode %lu", (unsigned long)t[i].mode);
        dr_clear(&r, BLACK, 1.0f, 0);
        EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, ccw, sizeof(TLV)), D3D_OK);
        if (dr_read(&r, px)) t_check((SD_PX(px, DW, 16, 16) == WHITE) == t[i].ccw_drawn, __FILE__, __LINE__, "counter-clockwise triangle", "cull mode %lu", (unsigned long)t[i].mode);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_fillmode)
{
    DR_OPEN(r);
    TLV v[3];
    tlv_set(&v[0], 8, 8, 0.5f, WHITE); tlv_set(&v[1], 56, 8, 0.5f, WHITE); tlv_set(&v[2], 8, 56, 0.5f, WHITE);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        PXEQ(px, 20, 20, BLACK);
        int n = sd_count_not(px, DW, 0, 0, DW, DH, BLACK);
        CHECKF(n >= 100 && n <= 400, "wireframe outline has %d pixels", n);
        int edge = 0;
        for (int x = 20; x <= 44; x++) for (int y = 6; y <= 10; y++) edge += SD_PX(px, DW, x, y) == WHITE;
        CHECK(edge >= 10);
    }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FILLMODE, D3DFILL_POINT);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        int n = sd_count_not(px, DW, 0, 0, DW, DH, BLACK);
        PXEQ(px, 20, 20, BLACK);
        CHECKF(n >= 3 && n <= 30, "point fill mode has %d pixels", n);
    }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_FILLMODE, D3DFILL_SOLID);
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) PXEQ(px, 20, 20, WHITE);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_gouraud_gradient)
{
    DR_OPEN(r);
    TLV v[6];
    tlv_set(&v[0], 0, 0, 0.5f, RED);  tlv_set(&v[1], 64, 0, 0.5f, BLUE); tlv_set(&v[2], 0, 64, 0.5f, RED);
    tlv_set(&v[3], 64, 0, 0.5f, BLUE); tlv_set(&v[4], 64, 64, 0.5f, BLUE); tlv_set(&v[5], 0, 64, 0.5f, RED);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 2, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) {
        int prev_r = 256, prev_b = -1, ok = 1;
        for (int x = 4; x < 60; x += 8) {
            DWORD c = SD_PX(px, DW, x, 32);
            int rr = (c >> 16) & 0xff, gg = (c >> 8) & 0xff, bb = c & 0xff;
            if (rr > prev_r || bb < prev_b || gg != 0) ok = 0;
            if (rr + bb < 248 || rr + bb > 262) ok = 0;
            prev_r = rr; prev_b = bb;
        }
        CHECKF(ok, "horizontal red to blue gradient is not monotonic / not a linear blend");
        DWORD mid = SD_PX(px, DW, 32, 32);
        int mr = (mid >> 16) & 0xff, mb = mid & 0xff;
        CHECKF(mr > 110 && mr < 145 && mb > 110 && mb < 145, "midpoint colour 0x%08lx", (unsigned long)mid);
        /* the colour does not vary along the other axis */
        CHECK(sd_rgb_near(SD_PX(px, DW, 32, 8), SD_PX(px, DW, 32, 56), 3));
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_flat_shading_uses_first_vertex)
{
    DR_OPEN(r);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SHADEMODE, D3DSHADE_FLAT);
    TLV v[3];
    tlv_set(&v[0], 4, 4, 0.5f, RED); tlv_set(&v[1], 60, 4, 0.5f, GREEN); tlv_set(&v[2], 4, 60, 0.5f, BLUE);
    DWORD px[DW * DH];
    dr_clear(&r, BLACK, 1.0f, 0);
    EXPECT_HR(dr_draw_up(&r, D3DPT_TRIANGLELIST, 1, v, sizeof(TLV)), D3D_OK);
    if (dr_read(&r, px)) { PXEQ(px, 10, 10, RED); PXEQ(px, 40, 10, RED); PXEQ(px, 10, 40, RED); PXEQ(px, 20, 30, RED); }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_colorwrite_mask)
{
    DR_OPEN(r);
    static const struct { DWORD mask; DWORD want; } t[] = {
        { 0xf, 0xff102030u }, { D3DCOLORWRITEENABLE_RED, 0x00100000u }, { D3DCOLORWRITEENABLE_GREEN, 0x00002000u },
        { D3DCOLORWRITEENABLE_BLUE, 0x00000030u }, { D3DCOLORWRITEENABLE_ALPHA, 0xff000000u }, { 0, 0 },
        { D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_BLUE, 0x00100030u },
    };
    DWORD px[DW * DH];
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_COLORWRITEENABLE, 0xf);
        dr_clear(&r, 0, 1.0f, 0);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_COLORWRITEENABLE, t[i].mask);
        EXPECT_HR(dr_full(&r, 0.5f, 0xff102030u), D3D_OK);
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_COLORWRITEENABLE, 0xf);
        if (dr_read(&r, px)) t_check(SD_PX(px, DW, 32, 32) == t[i].want, __FILE__, __LINE__, "colour write mask", "mask 0x%lx got 0x%08lx want 0x%08lx", (unsigned long)t[i].mask,
                                    (unsigned long)SD_PX(px, DW, 32, 32), (unsigned long)t[i].want);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_alpha_blend_srcalpha)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dr_clear(&r, 0xff0000ffu, 1.0f, 0);
    EXPECT_HR(dr_full(&r, 0.5f, 0x80ff0000u), D3D_OK);
    if (dr_read(&r, px)) {
        DWORD c = SD_PX(px, DW, 32, 32);
        int rr = (c >> 16) & 0xff, bb = c & 0xff;
        CHECKF(rr >= 126 && rr <= 130 && bb >= 125 && bb <= 129 && ((c >> 8) & 0xff) == 0, "50%% blend gave 0x%08lx", (unsigned long)c);
    }
    /* alpha 0 leaves the destination, alpha 255 replaces it */
    dr_clear(&r, 0xff0000ffu, 1.0f, 0);
    dr_full(&r, 0.5f, 0x00ff0000u);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff0000ffu, 0);
    dr_clear(&r, 0xff0000ffu, 1.0f, 0);
    dr_full(&r, 0.5f, 0xffff0000u);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xffff0000u, 0);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_blend_factors_and_ops)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHABLENDENABLE, TRUE);
    /* ONE + ONE */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRCBLEND, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DESTBLEND, D3DBLEND_ONE);
    dr_clear(&r, 0xff0a141eu, 1.0f, 0);
    dr_full(&r, 0.5f, 0xff646464u);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff6e7882u, 1);   /* 0x0a+0x64, 0x14+0x64, 0x1e+0x64 */
    /* saturation */
    dr_clear(&r, 0xffc8c8c8u, 1.0f, 0);
    dr_full(&r, 0.5f, 0xff646464u);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xffffffffu, 0);
    /* ZERO + ZERO */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRCBLEND, D3DBLEND_ZERO);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DESTBLEND, D3DBLEND_ZERO);
    dr_clear(&r, 0xff808080u, 1.0f, 0);
    dr_full(&r, 0.5f, 0xffffffffu);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff000000u, 0);
    /* DESTCOLOR * SRC + 0 */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
    dr_clear(&r, 0xff808080u, 1.0f, 0);
    dr_full(&r, 0.5f, 0xff804020u);
    if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff402010u, 2);
    /* BLENDFACTOR */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRCBLEND, D3DBLEND_BLENDFACTOR);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DESTBLEND, D3DBLEND_INVBLENDFACTOR);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_BLENDFACTOR, 0xff404040u);
    dr_clear(&r, 0xff000000u, 1.0f, 0);
    dr_full(&r, 0.5f, 0xffffffffu);
    if ((r.d.caps.SrcBlendCaps & D3DPBLENDCAPS_BLENDFACTOR) && (r.d.caps.DestBlendCaps & D3DPBLENDCAPS_BLENDFACTOR)) {
        if (dr_read(&r, px)) PXNEAR(px, 32, 32, 0xff404040u, 2);
    } else t_note("blend factor not supported (caps)");
    DR_END(r);
}

TEST(d3d9_state_draw_dr_blend_ops)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.PrimitiveMiscCaps & D3DPMISCCAPS_BLENDOP, "no D3DPMISCCAPS_BLENDOP");
    DWORD px[DW * DH];
    static const struct { D3DBLENDOP op; DWORD dst, src, want; } t[] = {
        { D3DBLENDOP_ADD, 0xff203040u, 0xff101010u, 0xff304050u }, { D3DBLENDOP_SUBTRACT, 0xff203040u, 0xff305070u, 0xff102030u },
        { D3DBLENDOP_REVSUBTRACT, 0xff305070u, 0xff203040u, 0xff102030u }, { D3DBLENDOP_MIN, 0xff203040u, 0xff305020u, 0xff203020u },
        { D3DBLENDOP_MAX, 0xff203040u, 0xff305020u, 0xff305040u },
    };
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SRCBLEND, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_DESTBLEND, D3DBLEND_ONE);
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_BLENDOP, t[i].op);
        dr_clear(&r, t[i].dst, 1.0f, 0);
        dr_full(&r, 0.5f, t[i].src);
        if (dr_read(&r, px)) t_check(sd_rgb_near(SD_PX(px, DW, 32, 32), t[i].want, 1), __FILE__, __LINE__, "blend op", "op %d got 0x%08lx want 0x%08lx", (int)t[i].op, (unsigned long)SD_PX(px, DW, 32, 32), (unsigned long)t[i].want);
    }
    DR_END(r);
}

typedef struct { D3DCMPFUNC f; DWORD cap; } CMPCAP;
static const CMPCAP g_cmp[] = {
    { D3DCMP_NEVER, D3DPCMPCAPS_NEVER }, { D3DCMP_LESS, D3DPCMPCAPS_LESS }, { D3DCMP_EQUAL, D3DPCMPCAPS_EQUAL },
    { D3DCMP_LESSEQUAL, D3DPCMPCAPS_LESSEQUAL }, { D3DCMP_GREATER, D3DPCMPCAPS_GREATER }, { D3DCMP_NOTEQUAL, D3DPCMPCAPS_NOTEQUAL },
    { D3DCMP_GREATEREQUAL, D3DPCMPCAPS_GREATEREQUAL }, { D3DCMP_ALWAYS, D3DPCMPCAPS_ALWAYS },
};
static int cmp_pass(D3DCMPFUNC f, int a, int b)   /* a = incoming, b = stored */
{
    switch (f) {
    case D3DCMP_NEVER: return 0; case D3DCMP_LESS: return a < b; case D3DCMP_EQUAL: return a == b; case D3DCMP_LESSEQUAL: return a <= b;
    case D3DCMP_GREATER: return a > b; case D3DCMP_NOTEQUAL: return a != b; case D3DCMP_GREATEREQUAL: return a >= b; default: return 1;
    }
}

TEST(d3d9_state_draw_dr_alpha_test_functions)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    static const int alphas[] = { 0x7f, 0x80, 0x81 };         /* incoming alpha against reference 0x80 */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHATESTENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHAREF, 0x80);
    for (unsigned i = 0; i < 8; i++) {
        if (!(r.d.caps.AlphaCmpCaps & g_cmp[i].cap)) { CHECKF(0, "alpha compare function %d not reported in AlphaCmpCaps (Win10 reports all)", (int)g_cmp[i].f); continue; }
        IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHAFUNC, g_cmp[i].f);
        for (int a = 0; a < 3; a++) {
            dr_clear(&r, 0xff000000u, 1.0f, 0);
            dr_full(&r, 0.5f, ((DWORD)alphas[a] << 24) | 0x00ff00u);
            if (!dr_read(&r, px)) continue;
            int drawn = (SD_PX(px, DW, 32, 32) & 0x00ffffff) == 0x00ff00u;
            t_check(drawn == cmp_pass(g_cmp[i].f, alphas[a], 0x80), __FILE__, __LINE__, "alpha test", "func %d alpha 0x%x ref 0x80 drawn %d", (int)g_cmp[i].f, alphas[a], drawn);
        }
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_alpha_test_disabled_ignores_func)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHAFUNC, D3DCMP_NEVER);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ALPHAREF, 0xff);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_full(&r, 0.5f, 0x0000ff00u);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, 0x0000ff00u);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_depth_functions)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    static const float zs[] = { 0.25f, 0.5f, 0.75f };         /* incoming depth against stored 0.5 */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    for (unsigned i = 0; i < 8; i++) {
        if (!(r.d.caps.ZCmpCaps & g_cmp[i].cap)) { CHECKF(0, "depth compare function %d not reported in ZCmpCaps (Win10 reports all)", (int)g_cmp[i].f); continue; }
        for (int k = 0; k < 3; k++) {
            IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZFUNC, D3DCMP_ALWAYS);
            dr_clear(&r, BLACK, 0.5f, 0);
            IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZFUNC, g_cmp[i].f);
            dr_full(&r, zs[k], WHITE);
            if (!dr_read(&r, px)) continue;
            int drawn = SD_PX(px, DW, 32, 32) == WHITE;
            int a = k - 1;            /* -1, 0, +1: incoming relative to stored */
            t_check(drawn == cmp_pass(g_cmp[i].f, a, 0), __FILE__, __LINE__, "depth test", "func %d z %g stored 0.5 drawn %d", (int)g_cmp[i].f, zs[k], drawn);
        }
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_depth_write_toggle)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZFUNC, D3DCMP_LESS);
    /* near quad with depth writes: the far quad drawn afterwards is rejected */
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_full(&r, 0.25f, RED);
    dr_full(&r, 0.75f, GREEN);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, RED);
    /* near quad without depth writes: the far quad is accepted */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZWRITEENABLE, FALSE);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_full(&r, 0.25f, RED);
    dr_full(&r, 0.75f, GREEN);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, GREEN);
    /* depth test disabled: always drawn, and (with writes on) not stored */
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZWRITEENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_FALSE);
    dr_clear(&r, BLACK, 0.0f, 0);
    dr_full(&r, 0.9f, BLUE);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, BLUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    dr_full(&r, 0.5f, GREEN);   /* buffer still holds 0.0 -> LESS fails */
    if (dr_read(&r, px)) PXEQ(px, 32, 32, BLUE);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_depth_clear_value)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZENABLE, D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_ZFUNC, D3DCMP_LESS);
    dr_clear(&r, BLACK, 0.5f, 0);
    dr_full(&r, 0.25f, RED);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, RED);
    dr_clear(&r, BLACK, 0.5f, 0);
    dr_full(&r, 0.75f, RED);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, BLACK);
    dr_clear(&r, BLACK, 0.0f, 0);
    dr_full(&r, 0.0f, RED);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, BLACK);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_full(&r, 0.999f, RED);
    if (dr_read(&r, px)) PXEQ(px, 32, 32, RED);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_viewport_limits_drawing)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    D3DVIEWPORT9 vp = { 16, 8, 32, 40, 0.0f, 1.0f };
    dr_clear(&r, BLACK, 1.0f, 0);          /* Clear is itself limited to the viewport: clear the whole target first */
    IDirect3DDevice9_SetViewport(DEV(r), &vp);
    /* pretransformed coordinates are viewport relative only for the clip; the quad is clipped to the viewport rectangle */
    EXPECT_HR(dr_full(&r, 0.5f, WHITE), D3D_OK);
    if (dr_read(&r, px)) {
        int n = sd_count_is(px, DW, 0, 0, DW, DH, WHITE);
        PXEQ(px, 32, 28, WHITE); PXEQ(px, 8, 28, BLACK); PXEQ(px, 56, 28, BLACK); PXEQ(px, 32, 4, BLACK); PXEQ(px, 32, 52, BLACK);
        CHECKF(n >= 32 * 40 - 80 && n <= 32 * 40 + 80, "clipped area %d (expected about 1280)", n);
    }
    DR_END(r);
}

TEST(d3d9_state_draw_dr_scissor_limits_drawing)
{
    DR_OPEN(r);
    DR_NEED(r, r.d.caps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST, "no scissor test");
    DWORD px[DW * DH];
    RECT sc = { 10, 12, 30, 40 };
    dr_clear(&r, BLACK, 1.0f, 0);          /* Clear honours the scissor test: clear before enabling it */
    IDirect3DDevice9_SetScissorRect(DEV(r), &sc);
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SCISSORTESTENABLE, TRUE);
    dr_full(&r, 0.5f, WHITE);
    if (dr_read(&r, px)) {
        PXEQ(px, 20, 25, WHITE); PXEQ(px, 5, 25, BLACK); PXEQ(px, 35, 25, BLACK); PXEQ(px, 20, 5, BLACK); PXEQ(px, 20, 45, BLACK);
        PXEQ(px, 10, 12, WHITE); PXEQ(px, 29, 39, WHITE); PXEQ(px, 30, 39, BLACK); PXEQ(px, 29, 40, BLACK); PXEQ(px, 9, 12, BLACK);
        CHECK(sd_count_is(px, DW, 0, 0, DW, DH, WHITE) == 20 * 28);
    }
    IDirect3DDevice9_SetRenderState(DEV(r), D3DRS_SCISSORTESTENABLE, FALSE);
    dr_clear(&r, BLACK, 1.0f, 0);
    dr_full(&r, 0.5f, WHITE);
    if (dr_read(&r, px)) PXEQ(px, 5, 5, WHITE);
    DR_END(r);
}

TEST(d3d9_state_draw_dr_clear_then_draw_order)
{
    DR_OPEN(r);
    DWORD px[DW * DH];
    dr_clear(&r, RED, 1.0f, 0);
    dr_rect(&r, 16, 16, 48, 48, 0.5f, GREEN);
    dr_clear(&r, BLUE, 1.0f, 0);
    if (dr_read(&r, px)) { PXEQ(px, 32, 32, BLUE); PXEQ(px, 2, 2, BLUE); }
    DR_END(r);
}
