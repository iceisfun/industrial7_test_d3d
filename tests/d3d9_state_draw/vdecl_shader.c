/* D3D9 FVF, vertex declarations, shader creation and constants, stream sources, index buffers. */
#include "d3dh.h"
#include "d9sd.h"

#define DECL_END_ELEM D3DDECL_END()
static int decl_equal(IDirect3DVertexDeclaration9 *decl, const D3DVERTEXELEMENT9 *want)
{
    D3DVERTEXELEMENT9 got[MAXD3DDECLLENGTH + 1];
    UINT n = 0, nw = 0;
    if (!decl) return 0;
    if (FAILED(IDirect3DVertexDeclaration9_GetDeclaration(decl, got, &n))) return 0;
    while (want[nw].Stream != 0xff) nw++;
    nw++;
    return n == nw && !memcmp(got, want, nw * sizeof *want);
}

/* ---- FVF -------------------------------------------------------------------- */
TEST(d3d9_state_draw_fvf_default_zero)
{
    SD_OPEN(d);
    DWORD fvf = 0xdeadc0de;
    EXPECT_HR(IDirect3DDevice9_GetFVF(d.dev, &fvf), D3D_OK);
    EXPECT_EQ(fvf, 0);
    SD_END(d);
}

TEST(d3d9_state_draw_fvf_roundtrip)
{
    SD_OPEN(d);
    static const DWORD t[] = {
        D3DFVF_XYZ, D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1,
        D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2, D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0),
        D3DFVF_XYZB3 | D3DFVF_DIFFUSE, D3DFVF_XYZB1 | D3DFVF_LASTBETA_UBYTE4, D3DFVF_XYZW | D3DFVF_TEX8, D3DFVF_XYZ | D3DFVF_PSIZE | D3DFVF_DIFFUSE,
        D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE4(0), D3DFVF_NORMAL, D3DFVF_DIFFUSE,
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        DWORD g = 0;
        EXPECT_HR(IDirect3DDevice9_SetFVF(d.dev, t[i]), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetFVF(d.dev, &g), D3D_OK);
        CHECKF(g == t[i], "FVF 0x%lx read back as 0x%lx", (unsigned long)t[i], (unsigned long)g);
    }
    SD_END(d);
}

typedef struct { DWORD fvf; D3DVERTEXELEMENT9 el[8]; } FVFDECL;
#define E(off, ty, us, ix) { 0, off, ty, 0, us, ix }
static const FVFDECL g_fvfdecl[] = {
    { D3DFVF_XYZ, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), DECL_END_ELEM } },
    { D3DFVF_XYZW, { E(0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION, 0), DECL_END_ELEM } },
    { D3DFVF_XYZRHW, { E(0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITIONT, 0), DECL_END_ELEM } },
    { D3DFVF_XYZB1, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_BLENDWEIGHT, 0), DECL_END_ELEM } },
    { D3DFVF_XYZB2, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_BLENDWEIGHT, 0), DECL_END_ELEM } },
    { D3DFVF_XYZB3, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_BLENDWEIGHT, 0), DECL_END_ELEM } },
    { D3DFVF_XYZB4, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_BLENDWEIGHT, 0), DECL_END_ELEM } },
    { D3DFVF_XYZB1 | D3DFVF_LASTBETA_UBYTE4, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_UBYTE4, D3DDECLUSAGE_BLENDINDICES, 0), DECL_END_ELEM } },
    { D3DFVF_XYZB1 | D3DFVF_LASTBETA_D3DCOLOR, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_BLENDINDICES, 0), DECL_END_ELEM } },
    { D3DFVF_XYZB2 | D3DFVF_LASTBETA_UBYTE4, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_BLENDWEIGHT, 0), E(16, D3DDECLTYPE_UBYTE4, D3DDECLUSAGE_BLENDINDICES, 0), DECL_END_ELEM } },
    { D3DFVF_XYZB5, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_BLENDWEIGHT, 0), E(28, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_BLENDINDICES, 0), DECL_END_ELEM } },
    { D3DFVF_NORMAL, { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0), DECL_END_ELEM } },
    { D3DFVF_PSIZE, { E(0, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_PSIZE, 0), DECL_END_ELEM } },
    { D3DFVF_DIFFUSE, { E(0, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0), DECL_END_ELEM } },
    { D3DFVF_SPECULAR, { E(0, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 1), DECL_END_ELEM } },
    { D3DFVF_TEX1, { E(0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0), DECL_END_ELEM } },
    { D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE1(0), { E(0, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_TEXCOORD, 0), DECL_END_ELEM } },
    { D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0), { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TEXCOORD, 0), DECL_END_ELEM } },
    { D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE4(0), { E(0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 0), DECL_END_ELEM } },
    { D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2,
      { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0), E(24, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0),
        E(28, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0), E(36, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1), DECL_END_ELEM } },
    { D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
      { E(0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITIONT, 0), E(16, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0), E(20, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 1),
        E(24, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0), DECL_END_ELEM } },
};

TEST(d3d9_state_draw_fvf_implicit_declaration)
{
    SD_OPEN(d);
    for (unsigned i = 0; i < sizeof g_fvfdecl / sizeof g_fvfdecl[0]; i++) {
        IDirect3DVertexDeclaration9 *decl = NULL;
        EXPECT_HR(IDirect3DDevice9_SetFVF(d.dev, g_fvfdecl[i].fvf), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetVertexDeclaration(d.dev, &decl), D3D_OK);
        t_check(decl && decl_equal(decl, g_fvfdecl[i].el), __FILE__, __LINE__, "SetFVF implicit declaration", "FVF 0x%lx", (unsigned long)g_fvfdecl[i].fvf);
        SAFE_RELEASE(decl);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_fvf_same_fvf_same_declaration_object)
{
    SD_OPEN(d);
    IDirect3DVertexDeclaration9 *a = NULL, *b = NULL, *c = NULL;
    IDirect3DDevice9_SetFVF(d.dev, D3DFVF_XYZRHW);
    IDirect3DDevice9_GetVertexDeclaration(d.dev, &a);
    IDirect3DDevice9_GetVertexDeclaration(d.dev, &b);
    CHECK(a != NULL && a == b);
    IDirect3DDevice9_SetFVF(d.dev, D3DFVF_NORMAL);
    IDirect3DDevice9_GetVertexDeclaration(d.dev, &c);
    CHECK(c != a);
    IDirect3DDevice9_SetFVF(d.dev, D3DFVF_XYZRHW);
    SAFE_RELEASE(c);
    IDirect3DDevice9_GetVertexDeclaration(d.dev, &c);
    CHECK(c == a);
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c);
    SD_END(d);
}

TEST(d3d9_state_draw_fvf_zero_keeps_declaration)
{
    SD_OPEN(d);
    static const D3DVERTEXELEMENT9 el[] = { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 3), DECL_END_ELEM };
    IDirect3DVertexDeclaration9 *decl = NULL, *got = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    SD_NEED(d, SUCCEEDED(hr), "CreateVertexDeclaration failed");
    EXPECT_HR(IDirect3DDevice9_SetVertexDeclaration(d.dev, decl), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetFVF(d.dev, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetVertexDeclaration(d.dev, &got), D3D_OK);
    CHECK(got == decl);
    SAFE_RELEASE(got);
    SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_fvf_declaration_overrides_fvf)
{
    SD_OPEN(d);
    static const D3DVERTEXELEMENT9 el[] = { E(0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0), DECL_END_ELEM };
    IDirect3DVertexDeclaration9 *decl = NULL;
    DWORD fvf = 0xdead;
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    SD_NEED(d, SUCCEEDED(hr), "CreateVertexDeclaration failed");
    IDirect3DDevice9_SetFVF(d.dev, D3DFVF_XYZ | D3DFVF_DIFFUSE);
    EXPECT_HR(IDirect3DDevice9_SetVertexDeclaration(d.dev, decl), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetFVF(d.dev, &fvf), D3D_OK);
    EXPECT_EQ(fvf, 0);
    SAFE_RELEASE(decl);
    IDirect3DDevice9_SetVertexDeclaration(d.dev, NULL);
    SD_END(d);
}

/* declarations that happen to be an FVF report that FVF; others report 0 */
TEST(d3d9_state_draw_fvf_from_declaration)
{
    SD_OPEN(d);
    struct { D3DVERTEXELEMENT9 el[3]; DWORD fvf; const char *what; } t[] = {
        { { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), DECL_END_ELEM }, D3DFVF_XYZ, "FLOAT3 POSITION" },
        { { E(0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITIONT, 0), DECL_END_ELEM }, D3DFVF_XYZRHW, "FLOAT4 POSITIONT" },
        { { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0), DECL_END_ELEM }, 0, "NORMAL only" },
        { { E(0, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0), DECL_END_ELEM }, 0, "COLOR0 only" },
        { { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 1), DECL_END_ELEM }, 0, "POSITION1" },
        { { DECL_END_ELEM }, 0, "empty" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDirect3DVertexDeclaration9 *decl = NULL;
        DWORD fvf = 0xdead;
        IDirect3DDevice9_SetFVF(d.dev, D3DFVF_DIFFUSE | D3DFVF_SPECULAR);
        HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, t[i].el, &decl);
        t_check(SUCCEEDED(hr), __FILE__, __LINE__, "CreateVertexDeclaration", "%s: %s", t[i].what, d3dh_hr_name(hr));
        if (FAILED(hr)) continue;
        IDirect3DDevice9_SetVertexDeclaration(d.dev, decl);
        EXPECT_HR(IDirect3DDevice9_GetFVF(d.dev, &fvf), D3D_OK);
        t_check(fvf == t[i].fvf, __FILE__, __LINE__, "GetFVF after SetVertexDeclaration", "%s: got 0x%lx want 0x%lx", t[i].what, (unsigned long)fvf, (unsigned long)t[i].fvf);
        IDirect3DDevice9_SetVertexDeclaration(d.dev, NULL);
        SAFE_RELEASE(decl);
    }
    SD_END(d);
}

/* ---- vertex declarations ---------------------------------------------------------- */
TEST(d3d9_state_draw_vdecl_create_get)
{
    SD_OPEN(d);
    static const D3DVERTEXELEMENT9 el[] = {
        E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0),
        E(16, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0), { 1, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 1 }, DECL_END_ELEM };
    IDirect3DVertexDeclaration9 *decl = NULL;
    EXPECT_HR(IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl), D3D_OK);
    SD_NEED(d, decl != NULL, "no declaration");
    UINT n = 0x1337c0de;
    EXPECT_HR(IDirect3DVertexDeclaration9_GetDeclaration(decl, NULL, &n), D3D_OK);
    EXPECT_EQ(n, 5);
    D3DVERTEXELEMENT9 got[8];
    memset(got, 0xcc, sizeof got);
    n = 0;
    EXPECT_HR(IDirect3DVertexDeclaration9_GetDeclaration(decl, got, &n), D3D_OK);
    EXPECT_EQ(n, 5);
    CHECK(!memcmp(got, el, sizeof el));
    IDirect3DDevice9 *dev = NULL;
    EXPECT_HR(IDirect3DVertexDeclaration9_GetDevice(decl, &dev), D3D_OK);
    CHECK(dev == d.dev);
    SAFE_RELEASE(dev);
    SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_set_get_refcounts)
{
    SD_OPEN(d);
    static const D3DVERTEXELEMENT9 el[] = { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), DECL_END_ELEM };
    IDirect3DVertexDeclaration9 *decl = NULL, *tmp = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    SD_NEED(d, SUCCEEDED(hr), "CreateVertexDeclaration failed");
    ULONG before = d3dh_refcount(decl);
    EXPECT_HR(IDirect3DDevice9_SetVertexDeclaration(d.dev, decl), D3D_OK);
    EXPECT_EQ(d3dh_refcount(decl), before);          /* the device does not add a public reference */
    EXPECT_HR(IDirect3DDevice9_GetVertexDeclaration(d.dev, &tmp), D3D_OK);
    CHECK(tmp == decl);
    EXPECT_EQ(d3dh_refcount(decl), before + 1);      /* Get adds one */
    SAFE_RELEASE(tmp);
    SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_set_null)
{
    SD_OPEN(d);
    static const D3DVERTEXELEMENT9 el[] = { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), DECL_END_ELEM };
    IDirect3DVertexDeclaration9 *decl = NULL, *got = (void *)(ULONG_PTR)1;
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    SD_NEED(d, SUCCEEDED(hr), "CreateVertexDeclaration failed");
    IDirect3DDevice9_SetVertexDeclaration(d.dev, decl);
    EXPECT_HR(IDirect3DDevice9_SetVertexDeclaration(d.dev, NULL), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetVertexDeclaration(d.dev, &got), D3D_OK);
    CHECK(got == NULL);
    SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_default_is_none)
{
    SD_OPEN(d);
    IDirect3DVertexDeclaration9 *got = (void *)(ULONG_PTR)1;
    HRESULT hr = IDirect3DDevice9_GetVertexDeclaration(d.dev, &got);
    EXPECT_HR(hr, D3D_OK);
    CHECK(got == NULL);
    if (SUCCEEDED(hr) && got && got != (void *)(ULONG_PTR)1) IDirect3DVertexDeclaration9_Release(got);
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_empty_is_valid)
{
    SD_OPEN(d);
    static const D3DVERTEXELEMENT9 el[] = { DECL_END_ELEM };
    IDirect3DVertexDeclaration9 *decl = NULL;
    EXPECT_HR(IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl), D3D_OK);
    if (decl) {
        UINT n = 0;
        EXPECT_HR(IDirect3DVertexDeclaration9_GetDeclaration(decl, NULL, &n), D3D_OK);
        EXPECT_EQ(n, 1);
    }
    SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_color_offset_alignment)
{
    SD_OPEN(d);
    for (WORD off = 12; off <= 24; off++) {
        D3DVERTEXELEMENT9 el[3] = { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(off, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0), DECL_END_ELEM };
        IDirect3DVertexDeclaration9 *decl = NULL;
        HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
        HRESULT want = (off % 4 == 0) ? D3D_OK : E_FAIL;
        t_check(hr == want, __FILE__, __LINE__, "declaration element alignment", "COLOR at offset %u: got %s want %s", off, d3dh_hr_name(hr), d3dh_hr_name(want));
        SAFE_RELEASE(decl);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_unused_type_rejected)
{
    SD_OPEN(d);
    static const struct { BYTE stream; BYTE usage; BYTE index; } t[] = {
        { 0, D3DDECLUSAGE_COLOR, 0 }, { 0, D3DDECLUSAGE_TEXCOORD, 0 }, { 0, D3DDECLUSAGE_TEXCOORD, 1 }, { 0, D3DDECLUSAGE_TEXCOORD, 12 },
        { 1, D3DDECLUSAGE_TEXCOORD, 12 }, { 0, D3DDECLUSAGE_NORMAL, 0 }, { 1, D3DDECLUSAGE_NORMAL, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        D3DVERTEXELEMENT9 el[3] = { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), { t[i].stream, 16, D3DDECLTYPE_UNUSED, 0, t[i].usage, t[i].index }, DECL_END_ELEM };
        IDirect3DVertexDeclaration9 *decl = NULL;
        HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
        t_check(hr == E_FAIL, __FILE__, __LINE__, "D3DDECLTYPE_UNUSED element", "case %u got %s", i, d3dh_hr_name(hr));
        SAFE_RELEASE(decl);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_all_basic_types)
{
    SD_OPEN(d);
    static const struct { D3DDECLTYPE ty; WORD size; } t[] = {
        { D3DDECLTYPE_FLOAT1, 4 }, { D3DDECLTYPE_FLOAT2, 8 }, { D3DDECLTYPE_FLOAT3, 12 }, { D3DDECLTYPE_FLOAT4, 16 }, { D3DDECLTYPE_D3DCOLOR, 4 },
        { D3DDECLTYPE_UBYTE4, 4 }, { D3DDECLTYPE_SHORT2, 4 }, { D3DDECLTYPE_SHORT4, 8 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        D3DVERTEXELEMENT9 el[3] = { E(0, t[i].ty, D3DDECLUSAGE_TEXCOORD, 0), E(t[i].size, t[i].ty, D3DDECLUSAGE_TEXCOORD, 1), DECL_END_ELEM };
        IDirect3DVertexDeclaration9 *decl = NULL;
        HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
        t_check(hr == D3D_OK, __FILE__, __LINE__, "CreateVertexDeclaration(type)", "type %d got %s", (int)t[i].ty, d3dh_hr_name(hr));   /* UBYTE4, SHORT2, SHORT4 are accepted too */
        SAFE_RELEASE(decl);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_multi_stream)
{
    SD_OPEN(d);
    D3DVERTEXELEMENT9 el[4] = {
        { 0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 }, { 1, 0, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0 },
        { 15, 0, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 }, DECL_END_ELEM };
    IDirect3DVertexDeclaration9 *decl = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    EXPECT_HR(hr, D3D_OK);
    if (decl) CHECK(decl_equal(decl, el));
    SAFE_RELEASE(decl);
    SD_END(d);
}

TEST(d3d9_state_draw_vdecl_recorded_in_stateblock)
{
    SD_OPEN(d);
    static const D3DVERTEXELEMENT9 el[] = { E(0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0), E(12, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0), DECL_END_ELEM };
    IDirect3DVertexDeclaration9 *decl = NULL, *got = NULL;
    IDirect3DStateBlock9 *sb = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexDeclaration(d.dev, el, &decl);
    SD_NEED(d, SUCCEEDED(hr), "CreateVertexDeclaration failed");
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetVertexDeclaration(d.dev, decl), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    IDirect3DDevice9_GetVertexDeclaration(d.dev, &got);
    CHECK(got != decl);
    SAFE_RELEASE(got);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetVertexDeclaration(d.dev, &got);
    CHECK(got == decl);
    SAFE_RELEASE(got);
    IDirect3DDevice9_SetVertexDeclaration(d.dev, NULL);
    SAFE_RELEASE(sb);
    SAFE_RELEASE(decl);
    SD_END(d);
}

/* ---- shader creation -------------------------------------------------------------- */
static const DWORD g_vs11[] = { 0xfffe0101, 0x02000001, 0xc00f0000, 0x90e40000, 0x0000ffff };   /* vs_1_1: mov oPos, v0 */
static const DWORD g_ps11[] = { 0xffff0101, 0x02000001, 0x800f0000, 0x90e40000, 0x0000ffff };   /* ps_1_1: mov r0, v0 */

static const DWORD g_vs20[] = { 0xfffe0200, 0x0200001f, 0x80000000, 0x900f0000, 0x02000001, 0xc00f0000, 0x90e40000, 0x0000ffff };   /* vs_2_0: dcl_position v0; mov oPos, v0 */
static const DWORD g_vs30[] = { 0xfffe0300, 0x0200001f, 0x80000000, 0x900f0000, 0x0200001f, 0x80000000, 0xe00f0000, 0x02000001, 0xe00f0000, 0x90e40000, 0x0000ffff };   /* vs_3_0 */
static const DWORD g_ps20[] = { 0xffff0200, 0x02000001, 0x800f0800, 0xa0e40000, 0x0000ffff };   /* ps_2_0: mov oC0, c0 */
static const DWORD g_ps30[] = { 0xffff0300, 0x02000001, 0x800f0800, 0xa0e40000, 0x0000ffff };   /* ps_3_0 */
static const DWORD g_ps14[] = { 0xffff0104, 0x02000001, 0x800f0000, 0xa0e40000, 0x0000ffff };   /* ps_1_4: mov r0, c0 */
/* Windows 10 (HAL on the Basic Render Driver): caps report vs_3_0/ps_3_0 but shader model 1.x bytecode is refused at creation. */
TEST(d3d9_state_draw_shader_create_sm1_rejected)
{
    SD_OPEN(d);
    IDirect3DVertexShader9 *vs = NULL;
    IDirect3DPixelShader9 *ps = NULL;
    EXPECT_HR(IDirect3DDevice9_CreateVertexShader(d.dev, g_vs11, &vs), D3DERR_INVALIDCALL);
    CHECK(vs == NULL);
    EXPECT_HR(IDirect3DDevice9_CreatePixelShader(d.dev, g_ps11, &ps), D3DERR_INVALIDCALL);
    CHECK(ps == NULL);
    EXPECT_HR(IDirect3DDevice9_CreatePixelShader(d.dev, g_ps14, &ps), D3DERR_INVALIDCALL);
    CHECK(ps == NULL);
    SD_END(d);
}

TEST(d3d9_state_draw_shader_create_vs20)
{
    SD_OPEN(d);
    SD_NEED(d, d.caps.VertexShaderVersion >= D3DVS_VERSION(2, 0), "no vs20");
    IDirect3DVertexShader9 *vs = NULL;
    EXPECT_HR(IDirect3DDevice9_CreateVertexShader(d.dev, g_vs20, &vs), D3D_OK);
    if (vs) {
        UINT sz = 0;
        EXPECT_HR(IDirect3DVertexShader9_GetFunction(vs, NULL, &sz), D3D_OK);
        EXPECT_EQ(sz, sizeof g_vs20);
        DWORD buf[16];
        memset(buf, 0xcc, sizeof buf);
        sz = sizeof buf;
        EXPECT_HR(IDirect3DVertexShader9_GetFunction(vs, buf, &sz), D3D_OK);
        EXPECT_EQ(sz, sizeof g_vs20);
        CHECK(!memcmp(buf, g_vs20, sizeof g_vs20));
        IDirect3DDevice9 *dev = NULL;
        EXPECT_HR(IDirect3DVertexShader9_GetDevice(vs, &dev), D3D_OK);
        CHECK(dev == d.dev);
        SAFE_RELEASE(dev);
    }
    SAFE_RELEASE(vs);
    SD_END(d);
}

TEST(d3d9_state_draw_shader_create_vs30)
{
    SD_OPEN(d);
    SD_NEED(d, d.caps.VertexShaderVersion >= D3DVS_VERSION(3, 0), "no vs30");
    IDirect3DVertexShader9 *vs = NULL;
    EXPECT_HR(IDirect3DDevice9_CreateVertexShader(d.dev, g_vs30, &vs), D3D_OK);
    if (vs) {
        UINT sz = 0;
        EXPECT_HR(IDirect3DVertexShader9_GetFunction(vs, NULL, &sz), D3D_OK);
        EXPECT_EQ(sz, sizeof g_vs30);
        DWORD buf[16];
        memset(buf, 0xcc, sizeof buf);
        sz = sizeof buf;
        EXPECT_HR(IDirect3DVertexShader9_GetFunction(vs, buf, &sz), D3D_OK);
        EXPECT_EQ(sz, sizeof g_vs30);
        CHECK(!memcmp(buf, g_vs30, sizeof g_vs30));
        IDirect3DDevice9 *dev = NULL;
        EXPECT_HR(IDirect3DVertexShader9_GetDevice(vs, &dev), D3D_OK);
        CHECK(dev == d.dev);
        SAFE_RELEASE(dev);
    }
    SAFE_RELEASE(vs);
    SD_END(d);
}

TEST(d3d9_state_draw_shader_create_ps20)
{
    SD_OPEN(d);
    SD_NEED(d, d.caps.PixelShaderVersion >= D3DPS_VERSION(2, 0), "no ps20");
    IDirect3DPixelShader9 *ps = NULL;
    EXPECT_HR(IDirect3DDevice9_CreatePixelShader(d.dev, g_ps20, &ps), D3D_OK);
    if (ps) {
        UINT sz = 0;
        EXPECT_HR(IDirect3DPixelShader9_GetFunction(ps, NULL, &sz), D3D_OK);
        EXPECT_EQ(sz, sizeof g_ps20);
        DWORD buf[16];
        sz = sizeof buf;
        EXPECT_HR(IDirect3DPixelShader9_GetFunction(ps, buf, &sz), D3D_OK);
        CHECK(!memcmp(buf, g_ps20, sizeof g_ps20));
    }
    SAFE_RELEASE(ps);
    SD_END(d);
}

TEST(d3d9_state_draw_shader_create_ps30)
{
    SD_OPEN(d);
    SD_NEED(d, d.caps.PixelShaderVersion >= D3DPS_VERSION(3, 0), "no ps30");
    IDirect3DPixelShader9 *ps = NULL;
    EXPECT_HR(IDirect3DDevice9_CreatePixelShader(d.dev, g_ps30, &ps), D3D_OK);
    if (ps) {
        UINT sz = 0;
        EXPECT_HR(IDirect3DPixelShader9_GetFunction(ps, NULL, &sz), D3D_OK);
        EXPECT_EQ(sz, sizeof g_ps30);
        DWORD buf[16];
        sz = sizeof buf;
        EXPECT_HR(IDirect3DPixelShader9_GetFunction(ps, buf, &sz), D3D_OK);
        CHECK(!memcmp(buf, g_ps30, sizeof g_ps30));
    }
    SAFE_RELEASE(ps);
    SD_END(d);
}

TEST(d3d9_state_draw_shader_version_tokens_validated)
{
    SD_OPEN(d);
    /* a pixel shader token stream is not a vertex shader and vice versa */
    DWORD vs_as_ps[8], ps_as_vs[5];
    memcpy(vs_as_ps, g_vs20, sizeof vs_as_ps);
    memcpy(ps_as_vs, g_ps20, sizeof ps_as_vs);
    IDirect3DVertexShader9 *vs = NULL;
    IDirect3DPixelShader9 *ps = NULL;
    EXPECT_HR(IDirect3DDevice9_CreateVertexShader(d.dev, ps_as_vs, &vs), D3DERR_INVALIDCALL);
    CHECK(vs == NULL);
    EXPECT_HR(IDirect3DDevice9_CreatePixelShader(d.dev, vs_as_ps, &ps), D3DERR_INVALIDCALL);
    CHECK(ps == NULL);
    /* versions that do not exist for D3D9, and tokens that are not version tokens at all */
    static const DWORD bad_vs[] = { 0xfffe0400, 0xfffe0500, 0xfffe0600, 0x12345678, 0x00000000, 0xffffffff, 0xffff0200, 0xffff0300, 0xffff0400 };
    static const DWORD bad_ps[] = { 0xffff0400, 0xffff0500, 0xffff0600, 0x12345678, 0x00000000, 0xffffffff, 0xfffe0200, 0xfffe0300, 0xfffe0400 };
    for (unsigned i = 0; i < sizeof bad_vs / sizeof bad_vs[0]; i++) {
        DWORD code[8];
        memcpy(code, g_vs20, sizeof code);
        code[0] = bad_vs[i];
        vs = NULL;
        HRESULT hr = IDirect3DDevice9_CreateVertexShader(d.dev, code, &vs);
        t_check(hr == D3DERR_INVALIDCALL && !vs, __FILE__, __LINE__, "vertex shader bad version token", "0x%08lx got %s", (unsigned long)bad_vs[i], d3dh_hr_name(hr));
        SAFE_RELEASE(vs);
    }
    for (unsigned i = 0; i < sizeof bad_ps / sizeof bad_ps[0]; i++) {
        DWORD code[5];
        memcpy(code, g_ps20, sizeof code);
        code[0] = bad_ps[i];
        ps = NULL;
        HRESULT hr = IDirect3DDevice9_CreatePixelShader(d.dev, code, &ps);
        t_check(hr == D3DERR_INVALIDCALL && !ps, __FILE__, __LINE__, "pixel shader bad version token", "0x%08lx got %s", (unsigned long)bad_ps[i], d3dh_hr_name(hr));
        SAFE_RELEASE(ps);
    }
    SD_END(d);
}

static const char g_hlsl_vs[] = "float4 main(float4 p : POSITION) : POSITION { return p; }\n";
static const char g_hlsl_vs3[] = "void main(float4 p : POSITION, out float4 o : POSITION, out float4 c : COLOR0) { o = p; c = float4(1, 0, 0, 1); }\n";
static const char g_hlsl_ps[] = "float4 main() : COLOR { return float4(0.0, 1.0, 0.0, 1.0); }\n";
static const char g_hlsl_ps3[] = "float4 main(float4 c : COLOR0) : COLOR0 { return c; }\n";

static void shader_compiled_roundtrip(const char *src, const char *target, int is_vs, DWORD min_caps_ver, DWORD caps_ver)
{
    SD d;
    ID3DBlob *blob = NULL;
    if (!sd_open(&d, SD_STATE)) return;
    (void)caps_ver;
    if (is_vs ? d.caps.VertexShaderVersion < min_caps_ver : d.caps.PixelShaderVersion < min_caps_ver) { t_skip("device lacks %s", target); sd_close(&d); return; }
    HRESULT hr = d3dh_compile(src, "main", target, 0, &blob);
    if (FAILED(hr)) { t_skip("cannot compile %s: %s", target, d3dh_hr_name(hr)); sd_close(&d); return; }
    const DWORD *code = d3dh_blob_ptr(blob);
    SIZE_T size = d3dh_blob_size(blob);
    CHECK(size >= 8 && (size % 4) == 0);
    if (is_vs) {
        IDirect3DVertexShader9 *s = NULL;
        EXPECT_HR(IDirect3DDevice9_CreateVertexShader(d.dev, code, &s), D3D_OK);
        if (s) {
            UINT sz = 0;
            EXPECT_HR(IDirect3DVertexShader9_GetFunction(s, NULL, &sz), D3D_OK);
            EXPECT_EQ(sz, size);
            BYTE *buf = calloc(1, size + 16);
            sz = (UINT)(size + 16);
            EXPECT_HR(IDirect3DVertexShader9_GetFunction(s, buf, &sz), D3D_OK);
            EXPECT_EQ(sz, size);
            CHECK(!memcmp(buf, code, size));
            free(buf);
            EXPECT_HR(IDirect3DDevice9_SetVertexShader(d.dev, s), D3D_OK);
            IDirect3DVertexShader9 *g = NULL;
            EXPECT_HR(IDirect3DDevice9_GetVertexShader(d.dev, &g), D3D_OK);
            CHECK(g == s);
            SAFE_RELEASE(g);
            IDirect3DDevice9_SetVertexShader(d.dev, NULL);
            IDirect3DVertexShader9_Release(s);
        }
    } else {
        IDirect3DPixelShader9 *s = NULL;
        EXPECT_HR(IDirect3DDevice9_CreatePixelShader(d.dev, code, &s), D3D_OK);
        if (s) {
            UINT sz = 0;
            EXPECT_HR(IDirect3DPixelShader9_GetFunction(s, NULL, &sz), D3D_OK);
            EXPECT_EQ(sz, size);
            BYTE *buf = calloc(1, size + 16);
            sz = (UINT)(size + 16);
            EXPECT_HR(IDirect3DPixelShader9_GetFunction(s, buf, &sz), D3D_OK);
            CHECK(!memcmp(buf, code, size));
            free(buf);
            EXPECT_HR(IDirect3DDevice9_SetPixelShader(d.dev, s), D3D_OK);
            IDirect3DPixelShader9 *g = NULL;
            EXPECT_HR(IDirect3DDevice9_GetPixelShader(d.dev, &g), D3D_OK);
            CHECK(g == s);
            SAFE_RELEASE(g);
            IDirect3DDevice9_SetPixelShader(d.dev, NULL);
            IDirect3DPixelShader9_Release(s);
        }
    }
    SAFE_RELEASE(blob);
    sd_close(&d);
}

TEST(d3d9_state_draw_shader_compiled_vs_2_0) { shader_compiled_roundtrip(g_hlsl_vs, "vs_2_0", 1, D3DVS_VERSION(2, 0), 0); }
TEST(d3d9_state_draw_shader_compiled_vs_3_0) { shader_compiled_roundtrip(g_hlsl_vs3, "vs_3_0", 1, D3DVS_VERSION(3, 0), 0); }
TEST(d3d9_state_draw_shader_compiled_ps_2_0) { shader_compiled_roundtrip(g_hlsl_ps, "ps_2_0", 0, D3DPS_VERSION(2, 0), 0); }
TEST(d3d9_state_draw_shader_compiled_ps_3_0) { shader_compiled_roundtrip(g_hlsl_ps3, "ps_3_0", 0, D3DPS_VERSION(3, 0), 0); }

TEST(d3d9_state_draw_shader_version_token_matches_target)
{
    ID3DBlob *b = NULL;
    REQUIRE(d3dh_D3DCompile());
    static const struct { const char *src, *target; DWORD tok; } t[] = {
        { g_hlsl_vs, "vs_2_0", 0xfffe0200 }, { g_hlsl_vs3, "vs_3_0", 0xfffe0300 }, { g_hlsl_ps, "ps_2_0", 0xffff0200 }, { g_hlsl_ps3, "ps_3_0", 0xffff0300 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        HRESULT hr = d3dh_compile(t[i].src, "main", t[i].target, 0, &b);
        if (FAILED(hr)) { t_note("compile %s failed %s", t[i].target, d3dh_hr_name(hr)); continue; }
        const DWORD *c = d3dh_blob_ptr(b);
        t_check(c[0] == t[i].tok, __FILE__, __LINE__, "bytecode version token", "%s first token 0x%08lx want 0x%08lx", t[i].target, (unsigned long)c[0], (unsigned long)t[i].tok);
        t_check(c[d3dh_blob_size(b) / 4 - 1] == 0x0000ffff, __FILE__, __LINE__, "bytecode END token", "%s", t[i].target);
        SAFE_RELEASE(b);
    }
}

TEST(d3d9_state_draw_shader_default_none_and_replace)
{
    SD_OPEN(d);
    IDirect3DVertexShader9 *vs = (void *)(ULONG_PTR)1;
    IDirect3DPixelShader9 *ps = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDevice9_GetVertexShader(d.dev, &vs), D3D_OK);
    CHECK(vs == NULL);
    EXPECT_HR(IDirect3DDevice9_GetPixelShader(d.dev, &ps), D3D_OK);
    CHECK(ps == NULL);
    EXPECT_HR(IDirect3DDevice9_SetVertexShader(d.dev, NULL), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetPixelShader(d.dev, NULL), D3D_OK);
    SD_END(d);
}

TEST(d3d9_state_draw_shader_set_get_refcounts)
{
    SD_OPEN(d);
    SD_NEED(d, d.caps.VertexShaderVersion >= D3DVS_VERSION(2, 0), "no vs_2_0");
    IDirect3DVertexShader9 *vs = NULL, *g = NULL;
    EXPECT_HR(IDirect3DDevice9_CreateVertexShader(d.dev, g_vs20, &vs), D3D_OK);
    SD_NEED(d, vs != NULL, "CreateVertexShader failed");
    ULONG before = d3dh_refcount(vs);
    IDirect3DDevice9_SetVertexShader(d.dev, vs);
    ULONG after_set = d3dh_refcount(vs);
    IDirect3DDevice9_GetVertexShader(d.dev, &g);
    ULONG after_get = d3dh_refcount(vs);
    t_note("vertex shader refcounts: create %lu set %lu get %lu", (unsigned long)before, (unsigned long)after_set, (unsigned long)after_get);
    EXPECT_EQ(after_get, after_set + 1);
    SAFE_RELEASE(g);
    IDirect3DDevice9_SetVertexShader(d.dev, NULL);
    SAFE_RELEASE(vs);
    SD_END(d);
}

/* ---- shader constants ------------------------------------------------------------- */
TEST(d3d9_state_draw_const_ps_float_roundtrip)
{
    SD_OPEN(d);
    float in[4 * 8], out[4 * 8];
    for (int i = 0; i < 32; i++) in[i] = (float)i * 0.5f - 3.0f;
    memset(out, 0xcc, sizeof out);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 5, in, 8), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantF(d.dev, 5, out, 8), D3D_OK);
    CHECK(!memcmp(in, out, sizeof in));
    /* sub range */
    memset(out, 0xcc, sizeof out);
    EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantF(d.dev, 7, out, 2), D3D_OK);
    CHECK(!memcmp(in + 8, out, 8 * sizeof(float)));
    SD_END(d);
}

TEST(d3d9_state_draw_const_vs_float_roundtrip)
{
    SD_OPEN(d);
    float in[4 * 16], out[4 * 16];
    for (int i = 0; i < 64; i++) in[i] = 100.0f + (float)i;
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 200, in, 16), D3D_OK);
    memset(out, 0, sizeof out);
    EXPECT_HR(IDirect3DDevice9_GetVertexShaderConstantF(d.dev, 200, out, 16), D3D_OK);
    CHECK(!memcmp(in, out, sizeof in));
    SD_END(d);
}

TEST(d3d9_state_draw_const_float_bit_patterns_preserved)
{
    SD_OPEN(d);
    DWORD in[4] = { 0x7fc00000u, 0xff800000u, 0x80000000u, 0x00000001u }, out[4] = { 0 };
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 0, (float *)in, 1), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantF(d.dev, 0, (float *)out, 1), D3D_OK);
    CHECKF(!memcmp(in, out, sizeof in), "float constant bit patterns changed: %08lx %08lx %08lx %08lx", (unsigned long)out[0], (unsigned long)out[1], (unsigned long)out[2], (unsigned long)out[3]);
    SD_END(d);
}

TEST(d3d9_state_draw_const_int_roundtrip)
{
    SD_OPEN(d);
    int in[4 * 4] = { 1,2,3,4, -5,-6,-7,-8, 100,200,300,400, 0,1,0,1 }, out[16];
    memset(out, 0xcc, sizeof out);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantI(d.dev, 2, in, 4), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantI(d.dev, 2, out, 4), D3D_OK);
    CHECK(!memcmp(in, out, sizeof in));
    memset(out, 0xcc, sizeof out);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantI(d.dev, 1, in, 4), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetVertexShaderConstantI(d.dev, 1, out, 4), D3D_OK);
    CHECK(!memcmp(in, out, sizeof in));
    SD_END(d);
}

TEST(d3d9_state_draw_const_bool_roundtrip)
{
    SD_OPEN(d);
    BOOL in[8] = { 1, 0, 1, 1, 0, 0, 1, 0 }, out[8];
    memset(out, 0xcc, sizeof out);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantB(d.dev, 3, in, 8), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantB(d.dev, 3, out, 8), D3D_OK);
    CHECK(!memcmp(in, out, sizeof in));
    memset(out, 0xcc, sizeof out);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantB(d.dev, 0, in, 8), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetVertexShaderConstantB(d.dev, 0, out, 8), D3D_OK);
    CHECK(!memcmp(in, out, sizeof in));
    BOOL big[2] = { 5, -1 }, got[2] = { 0, 0 };
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantB(d.dev, 0, big, 2), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantB(d.dev, 0, got, 2), D3D_OK);
    EXPECT_EQ(got[0], 5);               /* Windows 10 stores BOOL constants unmodified (no normalisation to 1) */
    EXPECT_EQ(got[1], -1);
    SD_END(d);
}

TEST(d3d9_state_draw_const_ps_float_limit)
{
    SD_OPEN(d);
    float buf[4 * 256];
    memset(buf, 0, sizeof buf);
    UINT limit = (d.caps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) ? 224 : 32;
    EXPECT_EQ(d.caps.PixelShaderVersion, D3DPS_VERSION(3, 0));
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 0, buf, 32), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 31, buf, 1), D3D_OK);
    if (d.caps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) {
        EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 0, buf, 224), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 223, buf, 1), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 224, buf, 1), D3DERR_INVALIDCALL);
        EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 0, buf, 225), D3DERR_INVALIDCALL);
        EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantF(d.dev, 224, buf, 1), D3DERR_INVALIDCALL);
    } else {
        EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 32, buf, 1), D3DERR_INVALIDCALL);
    }
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 0, buf, 256), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 0xffff, buf, 1), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 0xffffffffu, buf, 1), D3DERR_INVALIDCALL);
    SD_END(d);
}

TEST(d3d9_state_draw_const_vs_float_limit)
{
    SD_OPEN(d);
    static float buf[4 * 512];
    memset(buf, 0, sizeof buf);
    EXPECT_EQ(d.caps.MaxVertexShaderConst, 8192);
    UINT lim = d.caps.MaxVertexShaderConst ? d.caps.MaxVertexShaderConst : 256;
    if (lim > 512) lim = 512;
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 0, buf, lim), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantF(d.dev, lim - 1, buf, 1), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 0x10000, buf, 1), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 0, buf, 0x10000), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 0xffffffffu, buf, 2), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetVertexShaderConstantF(d.dev, 0x10000, buf, 1), D3DERR_INVALIDCALL);
    /* with software vertex processing the register file is larger than the reported caps */
    HRESULT hr = IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 256, buf, 1);
    EXPECT_HR(hr, D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 8191, buf, 1), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantF(d.dev, 8192, buf, 1), D3DERR_INVALIDCALL);
    SD_END(d);
}

TEST(d3d9_state_draw_const_int_bool_limits)
{
    SD_OPEN(d);
    int ib[4 * 16] = { 0 };
    BOOL bb[16] = { 0 };
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantI(d.dev, 0, ib, 16), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantI(d.dev, 15, ib, 1), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantI(d.dev, 16, ib, 1), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantI(d.dev, 0, ib, 17), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantB(d.dev, 0, bb, 16), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantB(d.dev, 16, bb, 1), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantB(d.dev, 0, bb, 17), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantI(d.dev, 0, ib, 16), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantB(d.dev, 0, bb, 16), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantI(d.dev, 0x10000, ib, 1), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetVertexShaderConstantB(d.dev, 0x10000, bb, 1), D3DERR_INVALIDCALL);
    SD_END(d);
}

TEST(d3d9_state_draw_const_zero_count)
{
    SD_OPEN(d);
    float f[4] = { 0 };
    HRESULT hr = IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 0, f, 0);
    EXPECT_HR(hr, D3D_OK);              /* count 0 is a no-op, even with the start register one past the end */
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 224, f, 0), D3D_OK);
    SD_END(d);
}

TEST(d3d9_state_draw_const_stateblock_records_constants)
{
    SD_OPEN(d);
    float a[4] = { 1, 2, 3, 4 }, z[4] = { 0 }, g[4];
    IDirect3DStateBlock9 *sb = NULL;
    IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 10, z, 1);
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetPixelShaderConstantF(d.dev, 10, a, 1), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantF(d.dev, 10, g, 1), D3D_OK);
    CHECK(!memcmp(g, z, sizeof g));
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetPixelShaderConstantF(d.dev, 10, g, 1), D3D_OK);
    CHECK(!memcmp(g, a, sizeof g));
    SAFE_RELEASE(sb);
    SD_END(d);
}

TEST(d3d9_state_draw_const_caps)
{
    SD_OPEN(d);
    EXPECT_EQ(d.caps.VertexShaderVersion, 0xfffe0300u);          /* Windows 10 Basic Render Driver: vs_3_0 / ps_3_0 */
    EXPECT_EQ(d.caps.PixelShaderVersion, 0xffff0300u);
    EXPECT_EQ(d.caps.MaxVShaderInstructionsExecuted, 0xffffffffu);
    EXPECT_EQ(d.caps.MaxPShaderInstructionsExecuted, 0xffffffffu);
    EXPECT_EQ(d.caps.NumSimultaneousRTs, 4);
    SD_END(d);
}

/* ---- stream sources -------------------------------------------------------------- */
TEST(d3d9_state_draw_stream_default_empty)
{
    SD_OPEN(d);
    for (UINT s = 0; s < 16; s++) {
        IDirect3DVertexBuffer9 *vb = (void *)(ULONG_PTR)1;
        UINT off = 0x1234, stride = 0x1234;
        EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, s, &vb, &off, &stride), D3D_OK);
        CHECKF(vb == NULL && off == 0 && stride == 0, "stream %u: vb %p offset %u stride %u", s, (void *)vb, off, stride);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_stream_default_frequency_one)
{
    SD_OPEN(d);
    for (UINT s = 0; s < 16; s++) {
        UINT f = 0;
        EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, s, &f), D3D_OK);
        CHECKF(f == 1, "stream %u default frequency %u", s, f);
    }
    SD_END(d);
}

TEST(d3d9_state_draw_stream_set_get)
{
    SD_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL, *got = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexBuffer(d.dev, 512, 0, 0, D3DPOOL_DEFAULT, &vb, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create vertex buffer");
    UINT off = 0, stride = 0;
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, 32), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, 0, &got, &off, &stride), D3D_OK);
    CHECK(got == vb); EXPECT_EQ(off, 0); EXPECT_EQ(stride, 32);
    SAFE_RELEASE(got);
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 3, vb, 64, 16), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, 3, &got, &off, &stride), D3D_OK);
    CHECK(got == vb); EXPECT_EQ(off, 64); EXPECT_EQ(stride, 16);
    SAFE_RELEASE(got);
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 15, vb, 4, 12), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, 15, &got, &off, &stride), D3D_OK);
    CHECK(got == vb); EXPECT_EQ(off, 4); EXPECT_EQ(stride, 12);
    SAFE_RELEASE(got);
    EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, 1, &got, &off, &stride), D3D_OK);
    CHECK(got == NULL);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0);
    IDirect3DDevice9_SetStreamSource(d.dev, 3, NULL, 0, 0);
    IDirect3DDevice9_SetStreamSource(d.dev, 15, NULL, 0, 0);
    SAFE_RELEASE(vb);
    SD_END(d);
}

TEST(d3d9_state_draw_stream_null_buffer_keeps_offset_stride)
{
    SD_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL, *got = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexBuffer(d.dev, 512, 0, 0, D3DPOOL_DEFAULT, &vb, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create vertex buffer");
    UINT off = 0, stride = 0;
    IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 4, 32);
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, 0, &got, &off, &stride), D3D_OK);
    CHECK(got == NULL);
    EXPECT_EQ(off, 4);                  /* Windows 10: SetStreamSource(NULL, 0, 0) keeps the previous offset and stride */
    EXPECT_EQ(stride, 32);
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, 0), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, 0, &got, &off, &stride), D3D_OK);
    CHECK(got == vb); EXPECT_EQ(off, 0); EXPECT_EQ(stride, 0);
    SAFE_RELEASE(got);
    IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0);
    SAFE_RELEASE(vb);
    SD_END(d);
}

TEST(d3d9_state_draw_stream_offset_alignment)
{
    SD_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexBuffer(d.dev, 512, 0, 0, D3DPOOL_DEFAULT, &vb, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create vertex buffer");
    for (UINT off = 0; off < 8; off++) {
        hr = IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, off, 32);
        EXPECT_HR(hr, D3D_OK);          /* Windows 10 does not require 4-byte aligned stream offsets */
    }
    IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0);
    SAFE_RELEASE(vb);
    SD_END(d);
}

TEST(d3d9_state_draw_stream_out_of_range_index)
{
    SD_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    HRESULT hr = IDirect3DDevice9_CreateVertexBuffer(d.dev, 128, 0, 0, D3DPOOL_DEFAULT, &vb, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create vertex buffer");
    t_note("MaxStreams=%lu MaxStreamStride=%lu", (unsigned long)d.caps.MaxStreams, (unsigned long)d.caps.MaxStreamStride);
    /* Windows 10: SetStreamSource(stream == MaxStreams) is not validated and corrupts the heap (0xc0000374), so only the last valid stream is exercised. */
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, d.caps.MaxStreams - 1, vb, 0, 16), D3D_OK);
    IDirect3DDevice9_SetStreamSource(d.dev, d.caps.MaxStreams - 1, NULL, 0, 0);
    SAFE_RELEASE(vb);
    SD_END(d);
}

TEST(d3d9_state_draw_stream_freq_rules)
{
    SD_OPEN(d);
    UINT f = 0;
    /* stream 0 may not be instance data */
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 0, D3DSTREAMSOURCE_INSTANCEDATA | 1), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 0, D3DSTREAMSOURCE_INSTANCEDATA), D3DERR_INVALIDCALL);
    /* zero divider */
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 0, 0), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 1, &f), D3D_OK);
    EXPECT_EQ(f, 1);
    /* plain divider */
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, 2), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 1, &f), D3D_OK);
    EXPECT_EQ(f, 2);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, 0x3fffffff), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 1, &f), D3D_OK);
    EXPECT_EQ(f, 0x3fffffff);
    /* flags with a zero count are accepted */
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, D3DSTREAMSOURCE_INDEXEDDATA), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 1, &f), D3D_OK);
    EXPECT_EQ(f, D3DSTREAMSOURCE_INDEXEDDATA);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, D3DSTREAMSOURCE_INSTANCEDATA), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 1, &f), D3D_OK);
    EXPECT_EQ(f, D3DSTREAMSOURCE_INSTANCEDATA);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 2, D3DSTREAMSOURCE_INSTANCEDATA | 3), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 2, &f), D3D_OK);
    EXPECT_EQ(f, D3DSTREAMSOURCE_INSTANCEDATA | 3);
    /* both flags together are rejected and the previous value stays */
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, D3DSTREAMSOURCE_INSTANCEDATA | D3DSTREAMSOURCE_INDEXEDDATA), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 1, D3DSTREAMSOURCE_INSTANCEDATA | D3DSTREAMSOURCE_INDEXEDDATA | 5), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 1, &f), D3D_OK);
    EXPECT_EQ(f, D3DSTREAMSOURCE_INSTANCEDATA);
    /* indexed data is fine on stream 0 */
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 0, D3DSTREAMSOURCE_INDEXEDDATA | 4), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 0, &f), D3D_OK);
    EXPECT_EQ(f, D3DSTREAMSOURCE_INDEXEDDATA | 4);
    /* restore */
    for (UINT s = 0; s < 16; s++) IDirect3DDevice9_SetStreamSourceFreq(d.dev, s, 1);
    SD_END(d);
}

TEST(d3d9_state_draw_stream_freq_index_clamped)
{
    SD_OPEN(d);
    UINT f = 0;
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 16, 7), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 15, &f), D3D_OK);
    EXPECT_EQ(f, 7);                    /* Windows 10 clamps the stream index to the last stream */
    EXPECT_HR(IDirect3DDevice9_GetStreamSourceFreq(d.dev, 100, &f), D3D_OK);
    EXPECT_EQ(f, 7);
    IDirect3DDevice9_SetStreamSourceFreq(d.dev, 15, 1);
    SD_END(d);
}

TEST(d3d9_state_draw_stream_freq_stateblock)
{
    SD_OPEN(d);
    IDirect3DStateBlock9 *sb = NULL;
    UINT f = 0;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetStreamSourceFreq(d.dev, 2, D3DSTREAMSOURCE_INSTANCEDATA | 2), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    IDirect3DDevice9_GetStreamSourceFreq(d.dev, 2, &f);
    EXPECT_EQ(f, 1);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetStreamSourceFreq(d.dev, 2, &f);
    EXPECT_EQ(f, D3DSTREAMSOURCE_INSTANCEDATA | 2);
    IDirect3DDevice9_SetStreamSourceFreq(d.dev, 2, 1);
    SAFE_RELEASE(sb);
    SD_END(d);
}

/* ---- index buffers ---------------------------------------------------------------- */
TEST(d3d9_state_draw_indices_default_none)
{
    SD_OPEN(d);
    IDirect3DIndexBuffer9 *ib = (void *)(ULONG_PTR)1;
    EXPECT_HR(IDirect3DDevice9_GetIndices(d.dev, &ib), D3D_OK);
    CHECK(ib == NULL);
    EXPECT_HR(IDirect3DDevice9_SetIndices(d.dev, NULL), D3D_OK);
    SD_END(d);
}

TEST(d3d9_state_draw_indices_set_get)
{
    SD_OPEN(d);
    IDirect3DIndexBuffer9 *ib = NULL, *ib2 = NULL, *got = NULL;
    HRESULT h1 = IDirect3DDevice9_CreateIndexBuffer(d.dev, 64, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, NULL);
    HRESULT h2 = IDirect3DDevice9_CreateIndexBuffer(d.dev, 64, 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ib2, NULL);
    SD_NEED(d, SUCCEEDED(h1), "cannot create index buffer");
    EXPECT_HR(IDirect3DDevice9_SetIndices(d.dev, ib), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetIndices(d.dev, &got), D3D_OK);
    CHECK(got == ib);
    SAFE_RELEASE(got);
    if (SUCCEEDED(h2)) {
        EXPECT_HR(IDirect3DDevice9_SetIndices(d.dev, ib2), D3D_OK);
        EXPECT_HR(IDirect3DDevice9_GetIndices(d.dev, &got), D3D_OK);
        CHECK(got == ib2);
        SAFE_RELEASE(got);
    } else EXPECT_HR(h2, D3D_OK);       /* 32-bit index buffers exist (MaxVertexIndex 0xffffff) */
    EXPECT_HR(IDirect3DDevice9_SetIndices(d.dev, NULL), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_GetIndices(d.dev, &got), D3D_OK);
    CHECK(got == NULL);
    SAFE_RELEASE(ib);
    SAFE_RELEASE(ib2);
    SD_END(d);
}

TEST(d3d9_state_draw_indices_stateblock)
{
    SD_OPEN(d);
    IDirect3DIndexBuffer9 *ib = NULL, *got = NULL;
    HRESULT hr = IDirect3DDevice9_CreateIndexBuffer(d.dev, 64, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, NULL);
    SD_NEED(d, SUCCEEDED(hr), "cannot create index buffer");
    IDirect3DStateBlock9 *sb = NULL;
    EXPECT_HR(IDirect3DDevice9_BeginStateBlock(d.dev), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_SetIndices(d.dev, ib), D3D_OK);
    EXPECT_HR(IDirect3DDevice9_EndStateBlock(d.dev, &sb), D3D_OK);
    IDirect3DDevice9_GetIndices(d.dev, &got);
    CHECK(got == NULL);
    EXPECT_HR(IDirect3DStateBlock9_Apply(sb), D3D_OK);
    IDirect3DDevice9_GetIndices(d.dev, &got);
    CHECK(got == ib);
    SAFE_RELEASE(got);
    IDirect3DDevice9_SetIndices(d.dev, NULL);
    SAFE_RELEASE(sb);
    SAFE_RELEASE(ib);
    SD_END(d);
}
