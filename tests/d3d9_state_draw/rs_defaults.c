/* D3D9 render states: documented defaults, set/get round trips, reset behaviour, invalid indices. */
#include "d3dh.h"
#include "d9sd.h"

typedef struct { D3DRENDERSTATETYPE rs; const char *name; DWORD def; } RSDEF;
#define RD(n, v) { n, #n, (DWORD)(v) }

/* Defaults from the D3DRENDERSTATETYPE documentation (device without automatic depth buffer). */
static const RSDEF g_def_core[] = {
    RD(D3DRS_FILLMODE, D3DFILL_SOLID), RD(D3DRS_SHADEMODE, D3DSHADE_GOURAUD), RD(D3DRS_ZWRITEENABLE, TRUE),
    RD(D3DRS_ALPHATESTENABLE, FALSE), RD(D3DRS_LASTPIXEL, TRUE), RD(D3DRS_SRCBLEND, D3DBLEND_ONE),
    RD(D3DRS_DESTBLEND, D3DBLEND_ZERO), RD(D3DRS_CULLMODE, D3DCULL_CCW), RD(D3DRS_ZFUNC, D3DCMP_LESSEQUAL),
    RD(D3DRS_ALPHAREF, 0), RD(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS), RD(D3DRS_DITHERENABLE, FALSE),
    RD(D3DRS_ALPHABLENDENABLE, FALSE), RD(D3DRS_SPECULARENABLE, FALSE),
    RD(D3DRS_STENCILENABLE, FALSE), RD(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP), RD(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP),
    RD(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP), RD(D3DRS_STENCILFUNC, D3DCMP_ALWAYS), RD(D3DRS_STENCILREF, 0),
    RD(D3DRS_STENCILMASK, 0xffffffffu), RD(D3DRS_STENCILWRITEMASK, 0xffffffffu),
    RD(D3DRS_TEXTUREFACTOR, 0xffffffffu), RD(D3DRS_CLIPPING, TRUE),
    RD(D3DRS_CLIPPLANEENABLE, 0), RD(D3DRS_MULTISAMPLEANTIALIAS, TRUE), RD(D3DRS_MULTISAMPLEMASK, 0xffffffffu),
    RD(D3DRS_COLORWRITEENABLE, 0xf), RD(D3DRS_BLENDOP, D3DBLENDOP_ADD), RD(D3DRS_SCISSORTESTENABLE, FALSE),
    RD(D3DRS_SLOPESCALEDEPTHBIAS, 0), RD(D3DRS_DEPTHBIAS, 0),
    RD(D3DRS_TWOSIDEDSTENCILMODE, FALSE), RD(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP),
    RD(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP), RD(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP),
    RD(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS), RD(D3DRS_COLORWRITEENABLE1, 0xf), RD(D3DRS_COLORWRITEENABLE2, 0xf),
    RD(D3DRS_COLORWRITEENABLE3, 0xf), RD(D3DRS_BLENDFACTOR, 0xffffffffu), RD(D3DRS_SRGBWRITEENABLE, 0),
    RD(D3DRS_SEPARATEALPHABLENDENABLE, FALSE), RD(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE),
    RD(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO), RD(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD),
};
static const RSDEF g_def_fog[] = {
    RD(D3DRS_FOGENABLE, FALSE), RD(D3DRS_FOGCOLOR, 0), RD(D3DRS_FOGTABLEMODE, D3DFOG_NONE),
    RD(D3DRS_FOGSTART, 0x00000000), RD(D3DRS_FOGEND, 0x3f800000), RD(D3DRS_FOGDENSITY, 0x3f800000),
    RD(D3DRS_RANGEFOGENABLE, FALSE), RD(D3DRS_FOGVERTEXMODE, D3DFOG_NONE),
};
static const RSDEF g_def_light[] = {
    RD(D3DRS_LIGHTING, TRUE), RD(D3DRS_AMBIENT, 0), RD(D3DRS_COLORVERTEX, TRUE), RD(D3DRS_LOCALVIEWER, TRUE),
    RD(D3DRS_NORMALIZENORMALS, FALSE), RD(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1),
    RD(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2), RD(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL),
    RD(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL), RD(D3DRS_VERTEXBLEND, D3DVBF_DISABLE),
    RD(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE), RD(D3DRS_TWEENFACTOR, 0x00000000),
};
static const RSDEF g_def_point[] = {
    RD(D3DRS_POINTSIZE, 0x3f800000), RD(D3DRS_POINTSPRITEENABLE, FALSE), RD(D3DRS_POINTSCALEENABLE, FALSE),
    RD(D3DRS_POINTSCALE_A, 0x3f800000), RD(D3DRS_POINTSCALE_B, 0), RD(D3DRS_POINTSCALE_C, 0),
};
static const RSDEF g_def_patch[] = {
    RD(D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_DISCRETE), RD(D3DRS_POSITIONDEGREE, D3DDEGREE_CUBIC),
    RD(D3DRS_NORMALDEGREE, D3DDEGREE_LINEAR), RD(D3DRS_MINTESSELLATIONLEVEL, 0x3f800000),
    RD(D3DRS_MAXTESSELLATIONLEVEL, 0x3f800000), RD(D3DRS_ENABLEADAPTIVETESSELLATION, FALSE),
    RD(D3DRS_ANTIALIASEDLINEENABLE, FALSE),
};
static const D3DRENDERSTATETYPE g_wraps[] = {
    D3DRS_WRAP0, D3DRS_WRAP1, D3DRS_WRAP2, D3DRS_WRAP3, D3DRS_WRAP4, D3DRS_WRAP5, D3DRS_WRAP6, D3DRS_WRAP7,
    D3DRS_WRAP8, D3DRS_WRAP9, D3DRS_WRAP10, D3DRS_WRAP11, D3DRS_WRAP12, D3DRS_WRAP13, D3DRS_WRAP14, D3DRS_WRAP15,
};

static void check_table(SD *d, const RSDEF *t, int n)
{
    for (int i = 0; i < n; i++) {
        DWORD v = 0xdeadc0de;
        HRESULT hr = IDirect3DDevice9_GetRenderState(d->dev, t[i].rs, &v);
        t_check(hr == D3D_OK && v == t[i].def, __FILE__, __LINE__, t[i].name, "hr %s got 0x%08lx want 0x%08lx",
                d3dh_hr_name(hr), (unsigned long)v, (unsigned long)t[i].def);
    }
}
#define CHECK_TABLE(d, t) check_table(&(d), t, (int)(sizeof(t) / sizeof((t)[0])))

TEST(d3d9_state_draw_rs_default_core)  { SD_OPEN(d);  CHECK_TABLE(d, g_def_core); SD_END(d); }
TEST(d3d9_state_draw_rs_default_fog)   { SD_OPEN(d);  CHECK_TABLE(d, g_def_fog); SD_END(d); }
TEST(d3d9_state_draw_rs_default_light) { SD_OPEN(d);  CHECK_TABLE(d, g_def_light); SD_END(d); }
TEST(d3d9_state_draw_rs_default_point) { SD_OPEN(d);  CHECK_TABLE(d, g_def_point); SD_END(d); }
TEST(d3d9_state_draw_rs_default_patch) { SD_OPEN(d);  CHECK_TABLE(d, g_def_patch); SD_END(d); }

TEST(d3d9_state_draw_rs_default_wrap)
{
    SD_OPEN(d);
    for (int i = 0; i < 16; i++) {
        DWORD v = 1;
        EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, g_wraps[i], &v), D3D_OK);
        CHECKF(v == 0, "WRAP%d default 0x%lx", i, (unsigned long)v);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_rs_default_adaptive_tess)
{
    SD_OPEN(d);
    static const D3DRENDERSTATETYPE a[] = { D3DRS_ADAPTIVETESS_X, D3DRS_ADAPTIVETESS_Y, D3DRS_ADAPTIVETESS_Z, D3DRS_ADAPTIVETESS_W };
    for (int i = 0; i < 4; i++) {
        DWORD v = 1;
        EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, a[i], &v), D3D_OK);
        t_note("ADAPTIVETESS_%c default bits 0x%08lx (%g)", "XYZW"[i], (unsigned long)v, d2f(v));
    }
    SD_END(d);
}

TEST(d3d9_state_draw_rs_default_pointsize_extremes)
{
    SD_OPEN(d);
    DWORD mn = 0, mx = 0;
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_POINTSIZE_MIN, &mn), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_POINTSIZE_MAX, &mx), D3D_OK);
    t_note("POINTSIZE_MIN=%g POINTSIZE_MAX=%g caps.MaxPointSize=%g", d2f(mn), d2f(mx), d.caps.MaxPointSize);
    CHECK(d2f(mn) >= 0.0f);
    SD_END(d);
}

TEST(d3d9_state_draw_rs_default_debugmonitor)
{
    SD_OPEN(d);
    DWORD v = 0xdeadc0de;
    HRESULT hr = IDirect3DDevice9_GetRenderState(d.dev, D3DRS_DEBUGMONITORTOKEN, &v);
    t_note("D3DRS_DEBUGMONITORTOKEN: %s value %lu", d3dh_hr_name(hr), (unsigned long)v);
    SD_END(d);
}

/* ZENABLE default depends on the swap chain's automatic depth buffer */
TEST(d3d9_state_draw_rs_default_zenable_without_ds)
{
    SD_OPEN(d);
    DWORD v = 99;
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ZENABLE, &v), D3D_OK);
    EXPECT_EQ(v, D3DZB_FALSE);
    SD_END(d);
}
TEST(d3d9_state_draw_rs_default_zenable_with_ds)
{
    SD_OPEN_DS(d);
    DWORD v = 99;
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ZENABLE, &v), D3D_OK);
    EXPECT_EQ(v, D3DZB_TRUE);
    SD_END(d);
}

/* Round trip of legal values for each state; the runtime stores what it is given. */
typedef struct { D3DRENDERSTATETYPE rs; const char *name; DWORD v1, v2; } RSRT;
#define RR(n, a, b) { n, #n, (DWORD)(a), (DWORD)(b) }
static const RSRT g_rt[] = {
    RR(D3DRS_ZENABLE, D3DZB_TRUE, D3DZB_FALSE), RR(D3DRS_FILLMODE, D3DFILL_WIREFRAME, D3DFILL_POINT),
    RR(D3DRS_SHADEMODE, D3DSHADE_FLAT, D3DSHADE_GOURAUD), RR(D3DRS_ZWRITEENABLE, FALSE, TRUE),
    RR(D3DRS_ALPHATESTENABLE, TRUE, FALSE), RR(D3DRS_LASTPIXEL, FALSE, TRUE),
    RR(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA, D3DBLEND_INVDESTCOLOR), RR(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA, D3DBLEND_ONE),
    RR(D3DRS_CULLMODE, D3DCULL_NONE, D3DCULL_CW), RR(D3DRS_ZFUNC, D3DCMP_NEVER, D3DCMP_GREATER),
    RR(D3DRS_ALPHAREF, 0x80, 0xff), RR(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL, D3DCMP_NOTEQUAL),
    RR(D3DRS_DITHERENABLE, TRUE, FALSE), RR(D3DRS_ALPHABLENDENABLE, TRUE, FALSE),
    RR(D3DRS_FOGENABLE, TRUE, FALSE), RR(D3DRS_SPECULARENABLE, TRUE, FALSE), RR(D3DRS_FOGCOLOR, 0x00336699, 0xff102030),
    RR(D3DRS_FOGTABLEMODE, D3DFOG_EXP, D3DFOG_EXP2), RR(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR, D3DFOG_EXP),
    RR(D3DRS_RANGEFOGENABLE, TRUE, FALSE), RR(D3DRS_STENCILENABLE, TRUE, FALSE),
    RR(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO, D3DSTENCILOP_REPLACE), RR(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCRSAT, D3DSTENCILOP_DECRSAT),
    RR(D3DRS_STENCILPASS, D3DSTENCILOP_INVERT, D3DSTENCILOP_INCR), RR(D3DRS_STENCILFUNC, D3DCMP_EQUAL, D3DCMP_LESS),
    RR(D3DRS_STENCILREF, 0x55, 0xa5), RR(D3DRS_STENCILMASK, 0x0f, 0xf0), RR(D3DRS_STENCILWRITEMASK, 0x3c, 0xc3),
    RR(D3DRS_TEXTUREFACTOR, 0x80402010, 0x01020304), RR(D3DRS_WRAP0, D3DWRAPCOORD_0, D3DWRAP_U | D3DWRAP_V),
    RR(D3DRS_WRAP7, D3DWRAP_W, D3DWRAPCOORD_1 | D3DWRAPCOORD_3), RR(D3DRS_CLIPPING, FALSE, TRUE),
    RR(D3DRS_LIGHTING, FALSE, TRUE), RR(D3DRS_AMBIENT, 0xff123456, 0x00000001), RR(D3DRS_COLORVERTEX, FALSE, TRUE),
    RR(D3DRS_LOCALVIEWER, FALSE, TRUE), RR(D3DRS_NORMALIZENORMALS, TRUE, FALSE),
    RR(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL, D3DMCS_COLOR2), RR(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR1, D3DMCS_MATERIAL),
    RR(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1, D3DMCS_COLOR2), RR(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR2, D3DMCS_COLOR1),
    RR(D3DRS_CLIPPLANEENABLE, 0x3f & ((1u << 6) - 1), 0x01), RR(D3DRS_POINTSPRITEENABLE, TRUE, FALSE),
    RR(D3DRS_POINTSCALEENABLE, TRUE, FALSE), RR(D3DRS_MULTISAMPLEANTIALIAS, FALSE, TRUE), RR(D3DRS_MULTISAMPLEMASK, 0x0000ffff, 0xffffffffu),
    RR(D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_CONTINUOUS, D3DPATCHEDGE_DISCRETE), RR(D3DRS_COLORWRITEENABLE, 0x5, 0xf),
    RR(D3DRS_BLENDOP, D3DBLENDOP_SUBTRACT, D3DBLENDOP_MAX), RR(D3DRS_SCISSORTESTENABLE, TRUE, FALSE),
    RR(D3DRS_ANTIALIASEDLINEENABLE, TRUE, FALSE), RR(D3DRS_TWOSIDEDSTENCILMODE, TRUE, FALSE),
    RR(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_ZERO, D3DSTENCILOP_KEEP), RR(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_INVERT, D3DSTENCILOP_KEEP),
    RR(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP), RR(D3DRS_CCW_STENCILFUNC, D3DCMP_NEVER, D3DCMP_ALWAYS),
    RR(D3DRS_COLORWRITEENABLE1, 0x1, 0xf), RR(D3DRS_COLORWRITEENABLE2, 0x2, 0xf), RR(D3DRS_COLORWRITEENABLE3, 0x4, 0xf),
    RR(D3DRS_BLENDFACTOR, 0x80ff4020, 0xffffffffu), RR(D3DRS_SRGBWRITEENABLE, TRUE, FALSE),
    RR(D3DRS_SEPARATEALPHABLENDENABLE, TRUE, FALSE), RR(D3DRS_SRCBLENDALPHA, D3DBLEND_DESTALPHA, D3DBLEND_ONE),
    RR(D3DRS_DESTBLENDALPHA, D3DBLEND_SRCALPHA, D3DBLEND_ZERO), RR(D3DRS_BLENDOPALPHA, D3DBLENDOP_REVSUBTRACT, D3DBLENDOP_ADD),
    RR(D3DRS_WRAP8, 0x7, 0), RR(D3DRS_WRAP15, 0xf, 0),
    RR(D3DRS_VERTEXBLEND, D3DVBF_1WEIGHTS, D3DVBF_DISABLE), RR(D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE, FALSE),
    RR(D3DRS_ENABLEADAPTIVETESSELLATION, TRUE, FALSE), RR(D3DRS_POSITIONDEGREE, D3DDEGREE_LINEAR, D3DDEGREE_CUBIC),
    RR(D3DRS_NORMALDEGREE, D3DDEGREE_QUADRATIC, D3DDEGREE_LINEAR),
};
/* float valued states, stored as raw bit patterns */
static const RSRT g_rt_f[] = {
    RR(D3DRS_FOGSTART, 0x3e800000, 0x40a00000), RR(D3DRS_FOGEND, 0x40000000, 0x3f000000),
    RR(D3DRS_FOGDENSITY, 0x3f000000, 0x40400000), RR(D3DRS_POINTSIZE, 0x40000000, 0x3f800000),
    RR(D3DRS_POINTSCALE_A, 0x40000000, 0x3f800000), RR(D3DRS_POINTSCALE_B, 0x3f000000, 0),
    RR(D3DRS_POINTSCALE_C, 0x3e800000, 0), RR(D3DRS_TWEENFACTOR, 0x3f000000, 0),
    RR(D3DRS_SLOPESCALEDEPTHBIAS, 0, 0), RR(D3DRS_DEPTHBIAS, 0, 0),
    RR(D3DRS_ADAPTIVETESS_X, 0x3f800000, 0), RR(D3DRS_ADAPTIVETESS_Y, 0x40000000, 0),
    RR(D3DRS_ADAPTIVETESS_Z, 0x40400000, 0), RR(D3DRS_ADAPTIVETESS_W, 0x40800000, 0),
};

static void roundtrip(SD *d, const RSRT *t, int n)
{
    for (int i = 0; i < n; i++) {
        DWORD g1 = 0xdeadc0de, g2 = 0xdeadc0de;
        HRESULT h1 = IDirect3DDevice9_SetRenderState(d->dev, t[i].rs, t[i].v1);
        HRESULT g1h = IDirect3DDevice9_GetRenderState(d->dev, t[i].rs, &g1);
        HRESULT h2 = IDirect3DDevice9_SetRenderState(d->dev, t[i].rs, t[i].v2);
        HRESULT g2h = IDirect3DDevice9_GetRenderState(d->dev, t[i].rs, &g2);
        t_check(h1 == D3D_OK && h2 == D3D_OK && g1h == D3D_OK && g2h == D3D_OK, __FILE__, __LINE__, t[i].name,
                "hr set1 %s get1 %s set2 %s get2 %s", d3dh_hr_name(h1), d3dh_hr_name(g1h), d3dh_hr_name(h2), d3dh_hr_name(g2h));
        t_check(g1 == t[i].v1 && g2 == t[i].v2, __FILE__, __LINE__, t[i].name, "round trip got 0x%08lx/0x%08lx want 0x%08lx/0x%08lx",
                (unsigned long)g1, (unsigned long)g2, (unsigned long)t[i].v1, (unsigned long)t[i].v2);
    }
}

TEST(d3d9_state_draw_rs_roundtrip_ints)   { SD_OPEN(d); roundtrip(&d, g_rt, (int)(sizeof g_rt / sizeof g_rt[0])); SD_END(d); }
TEST(d3d9_state_draw_rs_roundtrip_floats) { SD_OPEN(d); roundtrip(&d, g_rt_f, (int)(sizeof g_rt_f / sizeof g_rt_f[0])); SD_END(d); }

TEST(d3d9_state_draw_rs_roundtrip_ds_device)
{
    SD_OPEN_DS(d);
    roundtrip(&d, g_rt, (int)(sizeof g_rt / sizeof g_rt[0]));
    SD_END(d);
}

TEST(d3d9_state_draw_rs_pointsize_minmax_roundtrip)
{
    SD_OPEN(d);
    DWORD v = 0;
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_POINTSIZE_MIN, f2d(0.5f)), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_POINTSIZE_MIN, &v), D3D_OK);
    EXPECT_EQ(v, f2d(0.5f));
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_POINTSIZE_MAX, f2d(2.0f)), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_POINTSIZE_MAX, &v), D3D_OK);
    EXPECT_EQ(v, f2d(2.0f));
    SD_END(d);
}

/* Two writes: the last one wins; writing the same value twice is harmless. */
TEST(d3d9_state_draw_rs_last_write_wins)
{
    SD_OPEN(d);
    DWORD v = 0;
    for (DWORD i = 0; i < 20; i++) EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_TEXTUREFACTOR, 0x01010101u * i), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_TEXTUREFACTOR, &v), D3D_OK);
    EXPECT_EQ(v, 0x01010101u * 19);
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_TEXTUREFACTOR, v), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_TEXTUREFACTOR, &v), D3D_OK);
    EXPECT_EQ(v, 0x01010101u * 19);
    SD_END(d);
}

/* States are independent: setting one never disturbs another. */
TEST(d3d9_state_draw_rs_independence)
{
    SD_OPEN(d);
    DWORD before[64], after[64];
    static const D3DRENDERSTATETYPE watch[] = { D3DRS_ZWRITEENABLE, D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_CULLMODE, D3DRS_ALPHAREF,
        D3DRS_STENCILREF, D3DRS_TEXTUREFACTOR, D3DRS_FOGCOLOR, D3DRS_AMBIENT, D3DRS_COLORWRITEENABLE, D3DRS_BLENDFACTOR, D3DRS_CLIPPLANEENABLE };
    int n = (int)(sizeof watch / sizeof watch[0]);
    for (int i = 0; i < n; i++) IDirect3DDevice9_GetRenderState(d.dev, watch[i], &before[i]);
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_FILLMODE, D3DFILL_WIREFRAME), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetRenderState(d.dev, D3DRS_SHADEMODE, D3DSHADE_FLAT), D3D_OK);
    for (int i = 0; i < n; i++) IDirect3DDevice9_GetRenderState(d.dev, watch[i], &after[i]);
    for (int i = 0; i < n; i++) CHECKF(before[i] == after[i], "state %d changed 0x%lx -> 0x%lx", (int)watch[i], (unsigned long)before[i], (unsigned long)after[i]);
    SD_END(d);
}

/* Indices beyond the render state table are rejected by Get. */
TEST(d3d9_state_draw_rs_get_out_of_range)
{
    SD_OPEN(d);
    static const DWORD bad[] = { 256, 257, 300, 1000, 0xffff, 0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWORD v = 0x1234;
        HRESULT hr = IDirect3DDevice9_GetRenderState(d.dev, (D3DRENDERSTATETYPE)bad[i], &v);
        t_check(hr == D3DERR_INVALIDCALL, __FILE__, __LINE__, "GetRenderState(out of range)", "state %lu got %s", (unsigned long)bad[i], d3dh_hr_name(hr));
    }
    SD_END(d);
}

/* Every documented state enum value can be read. */
TEST(d3d9_state_draw_rs_get_all_documented)
{
    SD_OPEN(d);
    static const D3DRENDERSTATETYPE all[] = {
        D3DRS_ZENABLE, D3DRS_FILLMODE, D3DRS_SHADEMODE, D3DRS_ZWRITEENABLE, D3DRS_ALPHATESTENABLE, D3DRS_LASTPIXEL, D3DRS_SRCBLEND,
        D3DRS_DESTBLEND, D3DRS_CULLMODE, D3DRS_ZFUNC, D3DRS_ALPHAREF, D3DRS_ALPHAFUNC, D3DRS_DITHERENABLE, D3DRS_ALPHABLENDENABLE,
        D3DRS_FOGENABLE, D3DRS_SPECULARENABLE, D3DRS_FOGCOLOR, D3DRS_FOGTABLEMODE, D3DRS_FOGSTART, D3DRS_FOGEND, D3DRS_FOGDENSITY,
        D3DRS_RANGEFOGENABLE, D3DRS_STENCILENABLE, D3DRS_STENCILFAIL, D3DRS_STENCILZFAIL, D3DRS_STENCILPASS, D3DRS_STENCILFUNC,
        D3DRS_STENCILREF, D3DRS_STENCILMASK, D3DRS_STENCILWRITEMASK, D3DRS_TEXTUREFACTOR, D3DRS_WRAP0, D3DRS_WRAP1, D3DRS_WRAP2,
        D3DRS_WRAP3, D3DRS_WRAP4, D3DRS_WRAP5, D3DRS_WRAP6, D3DRS_WRAP7, D3DRS_CLIPPING, D3DRS_LIGHTING, D3DRS_AMBIENT,
        D3DRS_FOGVERTEXMODE, D3DRS_COLORVERTEX, D3DRS_LOCALVIEWER, D3DRS_NORMALIZENORMALS, D3DRS_DIFFUSEMATERIALSOURCE,
        D3DRS_SPECULARMATERIALSOURCE, D3DRS_AMBIENTMATERIALSOURCE, D3DRS_EMISSIVEMATERIALSOURCE, D3DRS_VERTEXBLEND,
        D3DRS_CLIPPLANEENABLE, D3DRS_POINTSIZE, D3DRS_POINTSIZE_MIN, D3DRS_POINTSPRITEENABLE, D3DRS_POINTSCALEENABLE,
        D3DRS_POINTSCALE_A, D3DRS_POINTSCALE_B, D3DRS_POINTSCALE_C, D3DRS_MULTISAMPLEANTIALIAS, D3DRS_MULTISAMPLEMASK,
        D3DRS_PATCHEDGESTYLE, D3DRS_DEBUGMONITORTOKEN, D3DRS_POINTSIZE_MAX, D3DRS_INDEXEDVERTEXBLENDENABLE, D3DRS_COLORWRITEENABLE,
        D3DRS_TWEENFACTOR, D3DRS_BLENDOP, D3DRS_POSITIONDEGREE, D3DRS_NORMALDEGREE, D3DRS_SCISSORTESTENABLE,
        D3DRS_SLOPESCALEDEPTHBIAS, D3DRS_ANTIALIASEDLINEENABLE, D3DRS_MINTESSELLATIONLEVEL, D3DRS_MAXTESSELLATIONLEVEL,
        D3DRS_ADAPTIVETESS_X, D3DRS_ADAPTIVETESS_Y, D3DRS_ADAPTIVETESS_Z, D3DRS_ADAPTIVETESS_W, D3DRS_ENABLEADAPTIVETESSELLATION,
        D3DRS_TWOSIDEDSTENCILMODE, D3DRS_CCW_STENCILFAIL, D3DRS_CCW_STENCILZFAIL, D3DRS_CCW_STENCILPASS, D3DRS_CCW_STENCILFUNC,
        D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2, D3DRS_COLORWRITEENABLE3, D3DRS_BLENDFACTOR, D3DRS_SRGBWRITEENABLE,
        D3DRS_DEPTHBIAS, D3DRS_WRAP8, D3DRS_WRAP9, D3DRS_WRAP10, D3DRS_WRAP11, D3DRS_WRAP12, D3DRS_WRAP13, D3DRS_WRAP14, D3DRS_WRAP15,
        D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA, D3DRS_DESTBLENDALPHA, D3DRS_BLENDOPALPHA,
    };
    for (unsigned i = 0; i < sizeof all / sizeof all[0]; i++) {
        DWORD v = 0;
        HRESULT hr = IDirect3DDevice9_GetRenderState(d.dev, all[i], &v);
        t_check(hr == D3D_OK, __FILE__, __LINE__, "GetRenderState(documented)", "state %d got %s", (int)all[i], d3dh_hr_name(hr));
    }
    CHECK(sizeof all / sizeof all[0] == 100);
    SD_END(d);
}

TEST(d3d9_state_draw_rs_get_undocumented_holes)
{
    SD_OPEN(d);
    /* values between the documented ids: the observed result is recorded, not required */
    static const int holes[] = { 0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 17, 18, 21, 30, 31, 32, 33, 39, 43, 44, 45, 46, 47, 49, 50, 51, 61, 100, 127, 138, 200, 255 };
    int rejected = 0;
    for (unsigned i = 0; i < sizeof holes / sizeof holes[0]; i++) {
        DWORD v = 0;
        HRESULT hr = IDirect3DDevice9_GetRenderState(d.dev, (D3DRENDERSTATETYPE)holes[i], &v);
        if (hr == D3DERR_INVALIDCALL) rejected++;
        else if (hr != D3D_OK) t_note("GetRenderState(%d) returned %s", holes[i], d3dh_hr_name(hr));
    }
    t_note("%d of %u undocumented render state ids rejected by GetRenderState", rejected, (unsigned)(sizeof holes / sizeof holes[0]));
    SD_END(d);
}

TEST(d3d9_state_draw_rs_set_out_of_range_not_fatal)
{
    SD_OPEN(d);
    HRESULT hr = IDirect3DDevice9_SetRenderState(d.dev, (D3DRENDERSTATETYPE)300, 5);
    t_note("SetRenderState(300) returned %s", d3dh_hr_name(hr));
    /* the call must not damage the documented states */
    DWORD v = 0;
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v), D3D_OK);
    EXPECT_EQ(v, D3DCULL_CCW);
    SD_END(d);
}

/* Reset restores every default and un-records the previous values. */
TEST(d3d9_state_draw_rs_reset_restores_defaults)
{
    SD_OPEN(d);
    D3DPRESENT_PARAMETERS pp;
    memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = SD_W; pp.BackBufferHeight = SD_H; pp.hDeviceWindow = d.hwnd;
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_TEXTUREFACTOR, 0x12345678);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(d.dev, D3DRS_FOGSTART, f2d(5.0f));
    HRESULT hr = IDirect3DDevice9_Reset(d.dev, &pp);
    if (FAILED(hr)) { t_skip("Reset failed %s", d3dh_hr_name(hr)); SD_END(d); return; }
    DWORD v;
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_CULLMODE, &v), D3D_OK);   EXPECT_EQ(v, D3DCULL_CCW);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_TEXTUREFACTOR, &v), D3D_OK); EXPECT_EQ(v, 0xffffffffu);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_LIGHTING, &v), D3D_OK);   EXPECT_EQ(v, TRUE);
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_FOGSTART, &v), D3D_OK);   EXPECT_EQ(v, 0);
    CHECK_TABLE(d, g_def_core);
    SD_END(d);
}

TEST(d3d9_state_draw_rs_reset_with_depth_changes_zenable)
{
    SD_OPEN(d);
    D3DPRESENT_PARAMETERS pp;
    memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = SD_W; pp.BackBufferHeight = SD_H; pp.hDeviceWindow = d.hwnd;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D16;
    HRESULT hr = IDirect3DDevice9_Reset(d.dev, &pp);
    if (FAILED(hr)) { t_skip("Reset with depth failed %s", d3dh_hr_name(hr)); SD_END(d); return; }
    DWORD v = 0;
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ZENABLE, &v), D3D_OK);
    EXPECT_EQ(v, D3DZB_TRUE);
    pp.EnableAutoDepthStencil = FALSE; pp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    hr = IDirect3DDevice9_Reset(d.dev, &pp);
    if (FAILED(hr)) { t_skip("second Reset failed %s", d3dh_hr_name(hr)); SD_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_GetRenderState(d.dev, D3DRS_ZENABLE, &v), D3D_OK);
    EXPECT_EQ(v, D3DZB_FALSE);
    SD_END(d);
}

/* Bounded fuzz-free table: every blend factor / op value accepted by the runtime reads back. */
TEST(d3d9_state_draw_rs_enum_values_roundtrip)
{
    SD_OPEN(d);
    struct { D3DRENDERSTATETYPE rs; DWORD lo, hi; } t[] = {
        { D3DRS_SRCBLEND, D3DBLEND_ZERO, D3DBLEND_SRCALPHASAT }, { D3DRS_DESTBLEND, D3DBLEND_ZERO, D3DBLEND_SRCALPHASAT },
        { D3DRS_ZFUNC, D3DCMP_NEVER, D3DCMP_ALWAYS }, { D3DRS_ALPHAFUNC, D3DCMP_NEVER, D3DCMP_ALWAYS },
        { D3DRS_STENCILFUNC, D3DCMP_NEVER, D3DCMP_ALWAYS }, { D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP, D3DSTENCILOP_DECR },
        { D3DRS_BLENDOP, D3DBLENDOP_ADD, D3DBLENDOP_MAX }, { D3DRS_CULLMODE, D3DCULL_NONE, D3DCULL_CCW },
        { D3DRS_FILLMODE, D3DFILL_POINT, D3DFILL_SOLID },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
        for (DWORD x = t[i].lo; x <= t[i].hi; x++) {
            DWORD v = 0xdead;
            HRESULT h1 = IDirect3DDevice9_SetRenderState(d.dev, t[i].rs, x);
            HRESULT h2 = IDirect3DDevice9_GetRenderState(d.dev, t[i].rs, &v);
            t_check(h1 == D3D_OK && h2 == D3D_OK && v == x, __FILE__, __LINE__, "enum round trip", "state %d value %lu: %s %s got %lu",
                    (int)t[i].rs, (unsigned long)x, d3dh_hr_name(h1), d3dh_hr_name(h2), (unsigned long)v);
        }
    SD_END(d);
}
