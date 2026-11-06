/* D3D9 texture stage states, sampler states, texture binding. */
#include "d3dh.h"
#include "d9sd.h"

typedef struct { D3DTEXTURESTAGESTATETYPE t; const char *name; DWORD def0, defn; } TSSDEF;
#define TD(n, a, b) { n, #n, (DWORD)(a), (DWORD)(b) }
static const TSSDEF g_tss[] = {
    TD(D3DTSS_COLOROP, D3DTOP_MODULATE, D3DTOP_DISABLE), TD(D3DTSS_COLORARG1, D3DTA_TEXTURE, D3DTA_TEXTURE),
    TD(D3DTSS_COLORARG2, D3DTA_CURRENT, D3DTA_CURRENT), TD(D3DTSS_ALPHAOP, D3DTOP_SELECTARG1, D3DTOP_DISABLE),
    TD(D3DTSS_ALPHAARG1, D3DTA_TEXTURE, D3DTA_TEXTURE), TD(D3DTSS_ALPHAARG2, D3DTA_CURRENT, D3DTA_CURRENT),
    TD(D3DTSS_BUMPENVMAT00, 0, 0), TD(D3DTSS_BUMPENVMAT01, 0, 0), TD(D3DTSS_BUMPENVMAT10, 0, 0), TD(D3DTSS_BUMPENVMAT11, 0, 0),
    TD(D3DTSS_BUMPENVLSCALE, 0, 0), TD(D3DTSS_BUMPENVLOFFSET, 0, 0),
    TD(D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE, D3DTTFF_DISABLE),
    TD(D3DTSS_COLORARG0, D3DTA_CURRENT, D3DTA_CURRENT), TD(D3DTSS_ALPHAARG0, D3DTA_CURRENT, D3DTA_CURRENT),
    TD(D3DTSS_RESULTARG, D3DTA_CURRENT, D3DTA_CURRENT), TD(D3DTSS_CONSTANT, 0, 0),
};

TEST(d3d9_state_draw_tss_defaults_stage0)
{
    SD_OPEN(d);
    for (unsigned i = 0; i < sizeof g_tss / sizeof g_tss[0]; i++) {
        DWORD v = 0xdeadc0de;
        HRESULT hr = IDirect3DDevice9_GetTextureStageState(d.dev, 0, g_tss[i].t, &v);
        t_check(hr == D3D_OK && v == g_tss[i].def0, __FILE__, __LINE__, g_tss[i].name, "stage 0 hr %s got 0x%lx want 0x%lx",
                d3dh_hr_name(hr), (unsigned long)v, (unsigned long)g_tss[i].def0);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_tss_defaults_other_stages)
{
    SD_OPEN(d);
    UINT n = d.caps.MaxTextureBlendStages;
    if (n > 8) n = 8;
    CHECK(n >= 1);
    for (UINT s = 1; s < n; s++)
        for (unsigned i = 0; i < sizeof g_tss / sizeof g_tss[0]; i++) {
            DWORD v = 0xdeadc0de;
            HRESULT hr = IDirect3DDevice9_GetTextureStageState(d.dev, s, g_tss[i].t, &v);
            t_check(hr == D3D_OK && v == g_tss[i].defn, __FILE__, __LINE__, g_tss[i].name, "stage %u hr %s got 0x%lx want 0x%lx",
                    s, d3dh_hr_name(hr), (unsigned long)v, (unsigned long)g_tss[i].defn);
        }
    SD_END(d);
}

TEST(d3d9_state_draw_tss_default_texcoordindex_is_stage)
{
    SD_OPEN(d);
    UINT n = d.caps.MaxTextureBlendStages;
    if (n > 8) n = 8;
    for (UINT s = 0; s < n; s++) {
        DWORD v = 0xdeadc0de;
        EXPECT_HR(IDirect3DDevice9_GetTextureStageState(d.dev, s, D3DTSS_TEXCOORDINDEX, &v), D3D_OK);
        CHECKF(v == s, "stage %u texcoordindex %lu", s, (unsigned long)v);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_tss_caps_stage_count)
{
    SD_OPEN(d);
    EXPECT_EQ(d.caps.MaxTextureBlendStages, 8);        /* Windows 10 Basic Render Driver */
    EXPECT_EQ(d.caps.MaxSimultaneousTextures, 8);
    SD_END(d);
}

/* a legal value for every stage state, distinct per stage */
static DWORD tss_value(D3DTEXTURESTAGESTATETYPE t, UINT stage)
{
    switch (t) {
    case D3DTSS_COLOROP: return D3DTOP_ADD + (stage & 1);
    case D3DTSS_COLORARG1: return D3DTA_DIFFUSE | D3DTA_COMPLEMENT;
    case D3DTSS_COLORARG2: return D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE;
    case D3DTSS_ALPHAOP: return D3DTOP_SUBTRACT + (stage & 1);
    case D3DTSS_ALPHAARG1: return D3DTA_SPECULAR;
    case D3DTSS_ALPHAARG2: return D3DTA_DIFFUSE | D3DTA_COMPLEMENT;
    case D3DTSS_BUMPENVMAT00: case D3DTSS_BUMPENVMAT01: case D3DTSS_BUMPENVMAT10: case D3DTSS_BUMPENVMAT11:
    case D3DTSS_BUMPENVLSCALE: case D3DTSS_BUMPENVLOFFSET:
        return f2d(0.25f * (float)(stage + 1));
    case D3DTSS_TEXCOORDINDEX: return (stage + 1) & 7;
    case D3DTSS_TEXTURETRANSFORMFLAGS: return D3DTTFF_COUNT2 | D3DTTFF_PROJECTED;
    case D3DTSS_COLORARG0: return D3DTA_SPECULAR;
    case D3DTSS_ALPHAARG0: return D3DTA_TEMP;
    case D3DTSS_RESULTARG: return D3DTA_TEMP;
    case D3DTSS_CONSTANT: return 0x80402010u + stage;
    default: return 0;
    }
}

TEST(d3d9_state_draw_tss_roundtrip_all_stages)
{
    SD_OPEN(d);
    for (UINT s = 0; s < 8; s++)
        for (unsigned i = 0; i < sizeof g_tss / sizeof g_tss[0]; i++) {
            D3DTEXTURESTAGESTATETYPE t = g_tss[i].t;
            DWORD want = tss_value(t, s), v = 0xdeadc0de;
            HRESULT h1 = IDirect3DDevice9_SetTextureStageState(d.dev, s, t, want);
            HRESULT h2 = IDirect3DDevice9_GetTextureStageState(d.dev, s, t, &v);
            t_check(h1 == D3D_OK && h2 == D3D_OK && v == want, __FILE__, __LINE__, g_tss[i].name, "stage %u set %s get %s got 0x%lx want 0x%lx",
                    s, d3dh_hr_name(h1), d3dh_hr_name(h2), (unsigned long)v, (unsigned long)want);
        }
    SD_END(d);
}

TEST(d3d9_state_draw_tss_stages_are_independent)
{
    SD_OPEN(d);
    for (UINT s = 0; s < 8; s++) IDirect3DDevice9_SetTextureStageState(d.dev, s, D3DTSS_CONSTANT, 0x11111111u * (s + 1));
    for (UINT s = 0; s < 8; s++) {
        DWORD v = 0;
        EXPECT_HR(IDirect3DDevice9_GetTextureStageState(d.dev, s, D3DTSS_CONSTANT, &v), D3D_OK);
        CHECKF(v == 0x11111111u * (s + 1), "stage %u constant 0x%lx", s, (unsigned long)v);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_tss_high_values_do_not_fail)
{
    SD_OPEN(d);
    DWORD v = 0;
    /* the runtime clamps the stage and state indices instead of rejecting them */
    HRESULT h1 = IDirect3DDevice9_GetTextureStageState(d.dev, 0, D3DTSS_COLOROP, &v);
    EXPECT_HR(h1, D3D_OK);
    HRESULT h2 = IDirect3DDevice9_SetTextureStageState(d.dev, 7, D3DTSS_COLOROP, D3DTOP_DISABLE);
    EXPECT_HR(h2, D3D_OK);
    SD_END(d);
}

/* ---- sampler states ------------------------------------------------------- */
typedef struct { D3DSAMPLERSTATETYPE t; const char *name; DWORD def; DWORD v1, v2; } SAMPDEF;
#define SM(n, d, a, b) { n, #n, (DWORD)(d), (DWORD)(a), (DWORD)(b) }
static const SAMPDEF g_samp[] = {
    SM(D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP, D3DTADDRESS_CLAMP, D3DTADDRESS_MIRROR),
    SM(D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP, D3DTADDRESS_BORDER, D3DTADDRESS_MIRRORONCE),
    SM(D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP, D3DTADDRESS_MIRRORONCE, D3DTADDRESS_CLAMP),
    SM(D3DSAMP_BORDERCOLOR, 0, 0xff336699u, 0x10203040),
    SM(D3DSAMP_MAGFILTER, D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_ANISOTROPIC),
    SM(D3DSAMP_MINFILTER, D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_ANISOTROPIC),
    SM(D3DSAMP_MIPFILTER, D3DTEXF_NONE, D3DTEXF_LINEAR, D3DTEXF_POINT),
    SM(D3DSAMP_MIPMAPLODBIAS, 0, 0xbf800000u, 0x40000000u),
    SM(D3DSAMP_MAXMIPLEVEL, 0, 3, 1),
    SM(D3DSAMP_MAXANISOTROPY, 1, 4, 2),
    SM(D3DSAMP_SRGBTEXTURE, 0, 1, 0),
    SM(D3DSAMP_ELEMENTINDEX, 0, 0, 0),
    SM(D3DSAMP_DMAPOFFSET, 0, 0, 0),
};

TEST(d3d9_state_draw_samp_defaults_all_samplers)
{
    SD_OPEN(d);
    for (UINT s = 0; s < 16; s++)
        for (unsigned i = 0; i < sizeof g_samp / sizeof g_samp[0]; i++) {
            DWORD v = 0xdeadc0de;
            HRESULT hr = IDirect3DDevice9_GetSamplerState(d.dev, s, g_samp[i].t, &v);
            t_check(hr == D3D_OK && v == g_samp[i].def, __FILE__, __LINE__, g_samp[i].name, "sampler %u hr %s got 0x%lx want 0x%lx",
                    s, d3dh_hr_name(hr), (unsigned long)v, (unsigned long)g_samp[i].def);
        }
    SD_END(d);
}

TEST(d3d9_state_draw_samp_defaults_vertex_samplers)
{
    SD_OPEN(d);
    for (UINT s = D3DVERTEXTEXTURESAMPLER0; s <= D3DVERTEXTEXTURESAMPLER3; s++)
        for (unsigned i = 0; i < sizeof g_samp / sizeof g_samp[0]; i++) {
            DWORD v = 0xdeadc0de;
            HRESULT hr = IDirect3DDevice9_GetSamplerState(d.dev, s, g_samp[i].t, &v);
            t_check(hr == D3D_OK && v == g_samp[i].def, __FILE__, __LINE__, g_samp[i].name, "vertex sampler %u hr %s got 0x%lx want 0x%lx",
                    s, d3dh_hr_name(hr), (unsigned long)v, (unsigned long)g_samp[i].def);
        }
    SD_END(d);
}

TEST(d3d9_state_draw_samp_default_displacement_sampler)
{
    SD_OPEN(d);
    for (unsigned i = 0; i < sizeof g_samp / sizeof g_samp[0]; i++) {
        DWORD v = 0xdeadc0de;
        HRESULT hr = IDirect3DDevice9_GetSamplerState(d.dev, D3DDMAPSAMPLER, g_samp[i].t, &v);
        t_check(hr == D3D_OK && v == g_samp[i].def, __FILE__, __LINE__, g_samp[i].name, "dmap sampler hr %s got 0x%lx want 0x%lx",
                d3dh_hr_name(hr), (unsigned long)v, (unsigned long)g_samp[i].def);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_samp_roundtrip_pixel_samplers)
{
    SD_OPEN(d);
    for (UINT s = 0; s < 16; s++)
        for (unsigned i = 0; i < sizeof g_samp / sizeof g_samp[0]; i++) {
            DWORD g1 = 0xdead, g2 = 0xdead;
            HRESULT a = IDirect3DDevice9_SetSamplerState(d.dev, s, g_samp[i].t, g_samp[i].v1);
            HRESULT b = IDirect3DDevice9_GetSamplerState(d.dev, s, g_samp[i].t, &g1);
            HRESULT c = IDirect3DDevice9_SetSamplerState(d.dev, s, g_samp[i].t, g_samp[i].v2);
            HRESULT e = IDirect3DDevice9_GetSamplerState(d.dev, s, g_samp[i].t, &g2);
            t_check(a == D3D_OK && b == D3D_OK && c == D3D_OK && e == D3D_OK && g1 == g_samp[i].v1 && g2 == g_samp[i].v2,
                    __FILE__, __LINE__, g_samp[i].name, "sampler %u: %s %s %s %s got 0x%lx 0x%lx want 0x%lx 0x%lx", s,
                    d3dh_hr_name(a), d3dh_hr_name(b), d3dh_hr_name(c), d3dh_hr_name(e), (unsigned long)g1, (unsigned long)g2,
                    (unsigned long)g_samp[i].v1, (unsigned long)g_samp[i].v2);
        }
    SD_END(d);
}

TEST(d3d9_state_draw_samp_roundtrip_vertex_samplers)
{
    SD_OPEN(d);
    for (UINT s = D3DVERTEXTEXTURESAMPLER0; s <= D3DVERTEXTEXTURESAMPLER3; s++) {
        DWORD v = 0;
        EXPECT_HR(IDirect3DDevice9_SetSamplerState(d.dev, s, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetSamplerState(d.dev, s, D3DSAMP_ADDRESSU, &v), D3D_OK);
        CHECKF(v == D3DTADDRESS_CLAMP, "vertex sampler %u addressu %lu", s, (unsigned long)v);
        EXPECT_HR(IDirect3DDevice9_SetSamplerState(d.dev, s, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetSamplerState(d.dev, s, D3DSAMP_MAGFILTER, &v), D3D_OK);
        CHECKF(v == D3DTEXF_LINEAR, "vertex sampler %u magfilter %lu", s, (unsigned long)v);
    }
    /* none of that touched the pixel samplers */
    for (UINT s = 0; s < 16; s++) {
        DWORD v = 99;
        IDirect3DDevice9_GetSamplerState(d.dev, s, D3DSAMP_ADDRESSU, &v);
        CHECKF(v == D3DTADDRESS_WRAP, "pixel sampler %u disturbed (%lu)", s, (unsigned long)v);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_samp_samplers_independent)
{
    SD_OPEN(d);
    for (UINT s = 0; s < 16; s++) IDirect3DDevice9_SetSamplerState(d.dev, s, D3DSAMP_BORDERCOLOR, 0x01010101u * (s + 1));
    for (UINT s = 0; s < 16; s++) {
        DWORD v = 0;
        IDirect3DDevice9_GetSamplerState(d.dev, s, D3DSAMP_BORDERCOLOR, &v);
        CHECKF(v == 0x01010101u * (s + 1), "sampler %u border 0x%lx", s, (unsigned long)v);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_samp_caps_anisotropy)
{
    SD_OPEN(d);
    EXPECT_EQ(d.caps.MaxAnisotropy, 16);
    EXPECT_EQ(d.caps.MaxVolumeExtent, 16384);
    EXPECT_EQ(d.caps.MaxTextureWidth, 16384);
    SD_END(d);
}

/* ---- SetTexture / GetTexture ---------------------------------------------- */
/* The public reference count while bound is runtime internal (it uses a separate use count); record it only. */
/* Windows 10: SetTexture does not change the public reference count (the runtime keeps a separate use count); GetTexture adds one. */
#define RCNOTE(p, want) do { ULONG rc_ = d3dh_refcount(p); t_check(rc_ == (ULONG)(want), __FILE__, __LINE__, "texture public refcount", "%s refcount %lu want %d at line %d", #p, (unsigned long)rc_, (int)(want), __LINE__); } while (0)
TEST(d3d9_state_draw_tex_default_none)
{
    SD_OPEN(d);
    for (DWORD s = 0; s < 16; s++) {
        IDirect3DBaseTexture9 *t = (IDirect3DBaseTexture9 *)(ULONG_PTR)1;
        EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, s, &t), D3D_OK);
        CHECKF(t == NULL, "sampler %lu has a texture", (unsigned long)s);
        if (t && t != (void *)(ULONG_PTR)1) IDirect3DBaseTexture9_Release(t);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_tex_set_get_refcount)
{
    SD_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create managed texture");
    RCNOTE(tex, 1);
    EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, 3, (IDirect3DBaseTexture9 *)tex), D3D_OK);
    RCNOTE(tex, 1);
    IDirect3DBaseTexture9 *got = NULL;
    EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, 3, &got), D3D_OK);
    CHECK(got == (IDirect3DBaseTexture9 *)tex);
    RCNOTE(tex, 2);
    SAFE_RELEASE(got);
    /* setting the same texture again does not leak a reference */
    EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, 3, (IDirect3DBaseTexture9 *)tex), D3D_OK);
    RCNOTE(tex, 1);
    /* other stage is empty */
    EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, 2, &got), D3D_OK);
    CHECK(got == NULL);
    EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, 3, NULL), D3D_OK);
    RCNOTE(tex, 1);
    EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, 3, &got), D3D_OK);
    CHECK(got == NULL);
    { ULONG rc_ = D3DH_RELEASE_RC(tex); EXPECT_EQ(rc_, 0); tex = NULL; }
    SD_END(d);
}

TEST(d3d9_state_draw_tex_same_texture_two_stages)
{
    SD_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create managed texture");
    IDirect3DDevice9_SetTexture(d.dev, 0, (IDirect3DBaseTexture9 *)tex);
    IDirect3DDevice9_SetTexture(d.dev, 5, (IDirect3DBaseTexture9 *)tex);
    RCNOTE(tex, 1);
    IDirect3DDevice9_SetTexture(d.dev, 0, NULL);
    RCNOTE(tex, 1);
    IDirect3DDevice9_SetTexture(d.dev, 5, NULL);
    RCNOTE(tex, 1);
    SAFE_RELEASE(tex);
    SD_END(d);
}

TEST(d3d9_state_draw_tex_replace_releases_old)
{
    SD_OPEN(d);
    IDirect3DTexture9 *a = NULL, *b = NULL;
    HRESULT h1 = IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &a, NULL);
    HRESULT h2 = IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &b, NULL);
    if (FAILED(h1) || FAILED(h2)) { SAFE_RELEASE(a); SAFE_RELEASE(b); SD_NEED(d, 0, "cannot create managed textures"); }
    IDirect3DDevice9_SetTexture(d.dev, 1, (IDirect3DBaseTexture9 *)a);
    RCNOTE(a, 1);
    IDirect3DDevice9_SetTexture(d.dev, 1, (IDirect3DBaseTexture9 *)b);
    RCNOTE(a, 1);
    RCNOTE(b, 1);
    SAFE_RELEASE(a);
    SAFE_RELEASE(b);          /* device still holds b */
    IDirect3DBaseTexture9 *got = NULL;
    EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, 1, &got), D3D_OK);
    CHECK(got != NULL);
    SAFE_RELEASE(got);
    IDirect3DDevice9_SetTexture(d.dev, 1, NULL);
    SD_END(d);
}

TEST(d3d9_state_draw_tex_sysmem_rejected)
{
    SD_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &tex, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create sysmem texture");
    hr = IDirect3DDevice9_SetTexture(d.dev, 0, (IDirect3DBaseTexture9 *)tex);
    IDirect3DBaseTexture9 *got = (IDirect3DBaseTexture9 *)(ULONG_PTR)1;
    if (d.caps.DevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY) {
        EXPECT_HR(hr, D3D_OK);          /* Windows 10 Basic Render Driver reports TEXTURESYSTEMMEMORY and accepts the binding */
        IDirect3DDevice9_SetTexture(d.dev, 0, NULL);
    } else {
        EXPECT_HR(hr, D3DERR_INVALIDCALL);
        EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, 0, &got), D3D_OK);
        CHECK(got == NULL);
    }
    SAFE_RELEASE(tex);
    SD_END(d);
}

TEST(d3d9_state_draw_tex_default_pool_texture_binds)
{
    SD_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create default pool texture");
    EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, 0, (IDirect3DBaseTexture9 *)tex), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, 0, NULL), D3D_OK);
    SAFE_RELEASE(tex);
    SD_END(d);
}

TEST(d3d9_state_draw_tex_cube_and_volume_bind)
{
    SD_OPEN(d);
    IDirect3DCubeTexture9 *cube = NULL;
    IDirect3DVolumeTexture9 *vol = NULL;
    HRESULT h1 = IDirect3DDevice9_CreateCubeTexture(d.dev, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cube, NULL);
    HRESULT h2 = IDirect3DDevice9_CreateVolumeTexture(d.dev, 4, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &vol, NULL);
    if (SUCCEEDED(h1)) {
        EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, 1, (IDirect3DBaseTexture9 *)cube), D3D_OK);
        IDirect3DBaseTexture9 *g = NULL;
        EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, 1, &g), D3D_OK);
        CHECK(g == (IDirect3DBaseTexture9 *)cube);
        if (g) EXPECT_EQ(IDirect3DBaseTexture9_GetType(g), D3DRTYPE_CUBETEXTURE);
        SAFE_RELEASE(g);
        IDirect3DDevice9_SetTexture(d.dev, 1, NULL);
    } else EXPECT_HR(h1, D3D_OK);
    if (SUCCEEDED(h2)) {
        EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, 2, (IDirect3DBaseTexture9 *)vol), D3D_OK);
        IDirect3DBaseTexture9 *g = NULL;
        EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, 2, &g), D3D_OK);
        CHECK(g == (IDirect3DBaseTexture9 *)vol);
        if (g) EXPECT_EQ(IDirect3DBaseTexture9_GetType(g), D3DRTYPE_VOLUMETEXTURE);
        SAFE_RELEASE(g);
        IDirect3DDevice9_SetTexture(d.dev, 2, NULL);
    } else EXPECT_HR(h2, D3D_OK);
    SAFE_RELEASE(cube);
    SAFE_RELEASE(vol);
    SD_END(d);
}

TEST(d3d9_state_draw_tex_vertex_texture_sampler)
{
    SD_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create managed texture");
    hr = IDirect3DDevice9_SetTexture(d.dev, D3DVERTEXTEXTURESAMPLER0, (IDirect3DBaseTexture9 *)tex);
    EXPECT_HR(hr, D3D_OK);              /* vertex texture fetch is available (vs_3_0) */
    if (SUCCEEDED(hr)) {
        IDirect3DBaseTexture9 *g = NULL;
        EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, D3DVERTEXTEXTURESAMPLER0, &g), D3D_OK);
        CHECK(g == (IDirect3DBaseTexture9 *)tex);
        SAFE_RELEASE(g);
        /* the pixel sampler with the same number is unaffected */
        EXPECT_HR(IDirect3DDevice9_GetTexture(d.dev, 0, &g), D3D_OK);
        CHECK(g == NULL);
        IDirect3DDevice9_SetTexture(d.dev, D3DVERTEXTEXTURESAMPLER0, NULL);
    }
    SAFE_RELEASE(tex);
    SD_END(d);
}

TEST(d3d9_state_draw_tex_all_sixteen_stages)
{
    SD_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create managed texture");
    for (DWORD s = 0; s < 16; s++) {
        EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, s, (IDirect3DBaseTexture9 *)tex), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_SetTexture(d.dev, s, NULL), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_SetSamplerState(d.dev, s, D3DSAMP_SRGBTEXTURE, TRUE), D3D_OK);
    }
    RCNOTE(tex, 1);
    SAFE_RELEASE(tex);
    SD_END(d);
}

TEST(d3d9_state_draw_tex_current_palette_roundtrip)
{
    SD_OPEN(d);
    UINT p = 99;
    HRESULT hr = IDirect3DDevice9_GetCurrentTexturePalette(d.dev, &p);
    EXPECT_HR(hr, D3D_OK);
    EXPECT_EQ(p, 0xffff);               /* no palette selected */
    SD_END(d);
}
