#include "dx.h"
/* d3dx9_43.dll: functionality that needs no device (version, image info, stacks, buffers, FVF, geometry, shaders). */

#define D3DXERR_INVALIDDATA_HR ((HRESULT)0x88760B59)
#define INVALIDCALL ((HRESULT)0x8876086C)

TEST(d3dx_redist_optional_x9_dll_presence) {
    HMODULE m = LoadLibraryA(X9);
    t_note("%s %s", X9, m ? "present" : "absent");
    static const char *others[] = { "d3dx9_42.dll","d3dx9_41.dll","d3dx9_40.dll","d3dx9_36.dll","d3dx9_30.dll","d3dx9_24.dll","d3dx10_43.dll","d3dx11_43.dll","d3dcompiler_43.dll","d3dcompiler_42.dll","d3d8.dll","opengl32.dll","d3dx10.dll","d3dx9d.dll" };
    for (unsigned i = 0; i < sizeof others / sizeof *others; i++) {
        HMODULE h = GetModuleHandleA(others[i]); BOOL was = h != NULL;
        h = LoadLibraryA(others[i]);
        t_note("%-20s %s", others[i], h ? "loadable" : "absent");
        if (h && !was) FreeLibrary(h);
    }
}
TEST(d3dx_redist_optional_x9_check_version) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCheckVersion));
    CHECK(F9(D3DXCheckVersion)(D3D_SDK_VERSION, D3DX_SDK_VERSION) == TRUE);
    CHECK(F9(D3DXCheckVersion)(D3D_SDK_VERSION + 1, D3DX_SDK_VERSION) == FALSE);
    CHECK(F9(D3DXCheckVersion)(D3D_SDK_VERSION, D3DX_SDK_VERSION - 1) == FALSE);
    CHECK(F9(D3DXCheckVersion)(0, 0) == FALSE);
    CHECK(D3DX_SDK_VERSION == 43 && D3D_SDK_VERSION == 32);
}

/* ---- image info ---------------------------------------------------------- */
static HRESULT info_of(const void *p, UINT n, D3DXIMAGE_INFO *i) {
    memset(i, 0xCD, sizeof *i);
    return F9(D3DXGetImageInfoFromFileInMemory)(p, n, i);
}
TEST(d3dx_redist_optional_x9_imageinfo_bmp) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    D3DXIMAGE_INFO i; EXPECT_HR(info_of(dxa_bmp24_2x2, sizeof dxa_bmp24_2x2, &i), S_OK);
    EXPECT_EQ(i.Width, 2); EXPECT_EQ(i.Height, 2); EXPECT_EQ(i.Depth, 1); EXPECT_EQ(i.MipLevels, 1);
    EXPECT_EQ(i.ResourceType, D3DRTYPE_TEXTURE); EXPECT_EQ(i.ImageFileFormat, D3DXIFF_BMP);
    t_note("24bpp BMP reports D3DFORMAT %d (R8G8B8=20, X8R8G8B8=22)", i.Format);
}
TEST(d3dx_redist_optional_x9_imageinfo_tga) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    D3DXIMAGE_INFO i; EXPECT_HR(info_of(dxa_tga32_2x2, sizeof dxa_tga32_2x2, &i), S_OK);
    EXPECT_EQ(i.Width, 2); EXPECT_EQ(i.Height, 2); EXPECT_EQ(i.Depth, 1); EXPECT_EQ(i.MipLevels, 1);
    EXPECT_EQ(i.ResourceType, D3DRTYPE_TEXTURE); EXPECT_EQ(i.ImageFileFormat, D3DXIFF_TGA);
    t_note("32bpp TGA reports D3DFORMAT %d (A8R8G8B8=21)", i.Format);
}
TEST(d3dx_redist_optional_x9_imageinfo_png) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    D3DXIMAGE_INFO i; EXPECT_HR(info_of(dxa_png_2x2, sizeof dxa_png_2x2, &i), S_OK);
    EXPECT_EQ(i.Width, 2); EXPECT_EQ(i.Height, 2); EXPECT_EQ(i.Depth, 1); EXPECT_EQ(i.MipLevels, 1);
    EXPECT_EQ(i.ResourceType, D3DRTYPE_TEXTURE); EXPECT_EQ(i.ImageFileFormat, D3DXIFF_PNG);
    t_note("RGBA8 PNG reports D3DFORMAT %d (A8R8G8B8=21)", i.Format);
}
TEST(d3dx_redist_optional_x9_imageinfo_dds_dxt1) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    D3DXIMAGE_INFO i; EXPECT_HR(info_of(dxa_dds_dxt1_4x4, sizeof dxa_dds_dxt1_4x4, &i), S_OK);
    EXPECT_EQ(i.Width, 4); EXPECT_EQ(i.Height, 4); EXPECT_EQ(i.Depth, 1); EXPECT_EQ(i.MipLevels, 1);
    EXPECT_EQ(i.Format, D3DFMT_DXT1); EXPECT_EQ(i.ResourceType, D3DRTYPE_TEXTURE); EXPECT_EQ(i.ImageFileFormat, D3DXIFF_DDS);
}
TEST(d3dx_redist_optional_x9_imageinfo_dds_mips) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    D3DXIMAGE_INFO i; EXPECT_HR(info_of(dxa_dds_dxt1_8x8_mips, sizeof dxa_dds_dxt1_8x8_mips, &i), S_OK);
    EXPECT_EQ(i.Width, 8); EXPECT_EQ(i.Height, 8); EXPECT_EQ(i.MipLevels, 4); EXPECT_EQ(i.Format, D3DFMT_DXT1);
    EXPECT_EQ(i.ImageFileFormat, D3DXIFF_DDS);
}
TEST(d3dx_redist_optional_x9_imageinfo_dds_argb) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    D3DXIMAGE_INFO i; EXPECT_HR(info_of(dxa_dds_argb_4x4, sizeof dxa_dds_argb_4x4, &i), S_OK);
    EXPECT_EQ(i.Width, 4); EXPECT_EQ(i.Height, 4); EXPECT_EQ(i.Format, D3DFMT_A8R8G8B8); EXPECT_EQ(i.MipLevels, 1);
}
TEST(d3dx_redist_optional_x9_imageinfo_null_and_empty) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    D3DXIMAGE_INFO i;
    EXPECT_HR(F9(D3DXGetImageInfoFromFileInMemory)(NULL, 0, &i), INVALIDCALL);
    EXPECT_HR(F9(D3DXGetImageInfoFromFileInMemory)(NULL, 16, &i), INVALIDCALL);
    EXPECT_HR(F9(D3DXGetImageInfoFromFileInMemory)(dxa_bmp24_2x2, 0, &i), INVALIDCALL);
}
TEST(d3dx_redist_optional_x9_imageinfo_garbage) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    static const unsigned char junk[64] = { 1,2,3,4,5,6,7,8,9,10 };
    D3DXIMAGE_INFO i;
    EXPECT_HR(info_of(junk, sizeof junk, &i), D3DXERR_INVALIDDATA_HR);
    unsigned char one = 'B';
    EXPECT_HR(info_of(&one, 1, &i), D3DXERR_INVALIDDATA_HR);
}
TEST(d3dx_redist_optional_x9_imageinfo_null_info_is_ok) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    HRESULT hr = F9(D3DXGetImageInfoFromFileInMemory)(dxa_png_2x2, sizeof dxa_png_2x2, NULL);
    t_note("NULL D3DXIMAGE_INFO with valid data -> %s", d3dh_hr_name(hr));
}
TEST(d3dx_redist_optional_x9_imageinfo_truncated) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    D3DXIMAGE_INFO i; HRESULT hr;
    hr = info_of(dxa_dds_dxt1_4x4, 20, &i); CHECK(FAILED(hr)); t_note("truncated DDS(20 bytes) -> %s", d3dh_hr_name(hr));
    hr = info_of(dxa_bmp24_2x2, 20, &i);    t_note("truncated BMP(20 bytes) -> %s", d3dh_hr_name(hr));
    hr = info_of(dxa_png_2x2, 16, &i);      t_note("truncated PNG(16 bytes) -> %s", d3dh_hr_name(hr));
}
TEST(d3dx_redist_optional_x9_imageinfo_signature_detection) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetImageInfoFromFileInMemory));
    /* the container is chosen from content, not from any name; a DDS with a corrupted magic is not a DDS */
    unsigned char copy[sizeof dxa_dds_dxt1_4x4]; memcpy(copy, dxa_dds_dxt1_4x4, sizeof copy);
    copy[0] = 'X';
    D3DXIMAGE_INFO i; HRESULT hr = info_of(copy, sizeof copy, &i);
    CHECK(FAILED(hr) || i.ImageFileFormat != D3DXIFF_DDS);
}

/* ---- buffer ---------------------------------------------------------------- */
TEST(d3dx_redist_optional_x9_create_buffer) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateBuffer));
    ID3DXBuffer *b = NULL;
    EXPECT_HR(F9(D3DXCreateBuffer)(100, &b), S_OK);
    if (!b) return;
    EXPECT_EQ(ID3DXBuffer_GetBufferSize(b), 100);
    void *p = ID3DXBuffer_GetBufferPointer(b); CHECK(p != NULL);
    if (p) { memset(p, 0xAB, 100); CHECK(((unsigned char *)p)[99] == 0xAB); }
    IUnknown *u = NULL;
    EXPECT_HR(IUnknown_QueryInterface((IUnknown *)b, &IID_IUnknown, (void **)&u), S_OK);
    if (u) IUnknown_Release(u);
    CHECK((ULONG)ID3DXBuffer_Release(b) == 0);
}
TEST(d3dx_redist_optional_x9_create_buffer_zero_and_null) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateBuffer));
    ID3DXBuffer *b = NULL;
    HRESULT hr = F9(D3DXCreateBuffer)(0, &b);
    t_note("D3DXCreateBuffer(0) -> %s", d3dh_hr_name(hr));
    if (b) ID3DXBuffer_Release(b);
    HRESULT hr2 = F9(D3DXCreateBuffer)(16, NULL);
    t_note("D3DXCreateBuffer(16, NULL) -> %s", d3dh_hr_name(hr2));
}
TEST(d3dx_redist_optional_x9_create_buffer_sizes) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateBuffer));
    static const DWORD sizes[] = { 1, 3, 4, 7, 4096, 65537, 1 << 20 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        ID3DXBuffer *b = NULL; EXPECT_HR(F9(D3DXCreateBuffer)(sizes[i], &b), S_OK);
        if (b) { EXPECT_EQ(ID3DXBuffer_GetBufferSize(b), sizes[i]); ID3DXBuffer_Release(b); }
    }
}

/* ---- matrix stack ------------------------------------------------------------ */
static ID3DXMatrixStack *mk_stack(void) {
    ID3DXMatrixStack *s = NULL; HRESULT hr = F9(D3DXCreateMatrixStack)(0, &s); EXPECT_HR(hr, S_OK); return s;
}
TEST(d3dx_redist_optional_x9_matrixstack_initial_identity) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    D3DXMATRIX *top = ID3DXMatrixStack_GetTop(s); CHECK(top != NULL);
    if (top) CHECK_MAT(*top, 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
    CHECK((ULONG)ID3DXMatrixStack_Release(s) == 0);
}
TEST(d3dx_redist_optional_x9_matrixstack_translate_scale_order) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    ID3DXMatrixStack_Translate(s, 1, 2, 3);
    ID3DXMatrixStack_Scale(s, 2, 2, 2);        /* top = top * S, i.e. T * S : the translation row is scaled */
    D3DXMATRIX *t = ID3DXMatrixStack_GetTop(s);
    CHECK_MAT(*t, 2,0,0,0, 0,2,0,0, 0,0,2,0, 2,4,6,1);
    ID3DXMatrixStack_LoadIdentity(s);
    ID3DXMatrixStack_Translate(s, 1, 2, 3);
    ID3DXMatrixStack_ScaleLocal(s, 2, 2, 2);   /* top = S * top : translation untouched */
    t = ID3DXMatrixStack_GetTop(s);
    CHECK_MAT(*t, 2,0,0,0, 0,2,0,0, 0,0,2,0, 1,2,3,1);
    ID3DXMatrixStack_Release(s);
}
TEST(d3dx_redist_optional_x9_matrixstack_push_pop) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    ID3DXMatrixStack_Translate(s, 1, 0, 0);
    EXPECT_HR(ID3DXMatrixStack_Push(s), S_OK);
    ID3DXMatrixStack_Translate(s, 0, 5, 0);
    D3DXMATRIX *t = ID3DXMatrixStack_GetTop(s); CHECKN(t->_42, 5.f);
    EXPECT_HR(ID3DXMatrixStack_Pop(s), S_OK);
    t = ID3DXMatrixStack_GetTop(s); CHECKN(t->_41, 1.f); CHECKN(t->_42, 0.f);
    ID3DXMatrixStack_Release(s);
}
TEST(d3dx_redist_optional_x9_matrixstack_push_copies_top) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    ID3DXMatrixStack_Translate(s, 3, 4, 5);
    ID3DXMatrixStack_Push(s);
    D3DXMATRIX *t = ID3DXMatrixStack_GetTop(s); CHECKN(t->_41, 3.f); CHECKN(t->_43, 5.f);
    ID3DXMatrixStack_Pop(s);
    ID3DXMatrixStack_Release(s);
}
TEST(d3dx_redist_optional_x9_matrixstack_load_and_mult) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack)); REQUIRE(F9(D3DXMatrixScaling)); REQUIRE(F9(D3DXMatrixTranslation));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    D3DXMATRIX sc, tr; F9(D3DXMatrixScaling)(&sc, 2, 3, 4); F9(D3DXMatrixTranslation)(&tr, 1, 1, 1);
    ID3DXMatrixStack_LoadMatrix(s, &sc);
    D3DXMATRIX *t = ID3DXMatrixStack_GetTop(s); CHECK_MAT(*t, 2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,1);
    ID3DXMatrixStack_MultMatrix(s, &tr);          /* sc * tr */
    t = ID3DXMatrixStack_GetTop(s); CHECK_MAT(*t, 2,0,0,0, 0,3,0,0, 0,0,4,0, 1,1,1,1);
    ID3DXMatrixStack_LoadMatrix(s, &sc);
    ID3DXMatrixStack_MultMatrixLocal(s, &tr);     /* tr * sc */
    t = ID3DXMatrixStack_GetTop(s); CHECK_MAT(*t, 2,0,0,0, 0,3,0,0, 0,0,4,0, 2,3,4,1);
    ID3DXMatrixStack_Release(s);
}
TEST(d3dx_redist_optional_x9_matrixstack_rotate_axis) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    D3DXVECTOR3 z = {0,0,1};
    EXPECT_HR(ID3DXMatrixStack_RotateAxis(s, &z, DX_PI / 2), S_OK);
    D3DXMATRIX *t = ID3DXMatrixStack_GetTop(s); CHECK_MAT(*t, 0,1,0,0, -1,0,0,0, 0,0,1,0, 0,0,0,1);
    ID3DXMatrixStack_Release(s);
}
TEST(d3dx_redist_optional_x9_matrixstack_pop_underflow) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    HRESULT hr = ID3DXMatrixStack_Pop(s);
    t_note("Pop on a fresh stack -> %s", d3dh_hr_name(hr));
    D3DXMATRIX *t = ID3DXMatrixStack_GetTop(s); t_note("GetTop after underflow pop: %p", (void *)t);
    ID3DXMatrixStack_Release(s);
}
TEST(d3dx_redist_optional_x9_matrixstack_deep_push) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    for (int i = 0; i < 200; i++) { EXPECT_HR(ID3DXMatrixStack_Push(s), S_OK); ID3DXMatrixStack_Translate(s, 1, 0, 0); }
    CHECKN(ID3DXMatrixStack_GetTop(s)->_41, 200.f);
    for (int i = 0; i < 200; i++) EXPECT_HR(ID3DXMatrixStack_Pop(s), S_OK);
    CHECKN(ID3DXMatrixStack_GetTop(s)->_41, 0.f);
    ID3DXMatrixStack_Release(s);
}
TEST(d3dx_redist_optional_x9_matrixstack_unknown_interface) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCreateMatrixStack));
    ID3DXMatrixStack *s = mk_stack(); if (!s) return;
    IUnknown *u = NULL;
    EXPECT_HR(IUnknown_QueryInterface((IUnknown *)s, &IID_IUnknown, (void **)&u), S_OK);
    if (u) IUnknown_Release(u);
    void *bad = (void *)1;
    EXPECT_HR(IUnknown_QueryInterface((IUnknown *)s, &IID_IDirect3D9, &bad), E_NOINTERFACE);
    CHECK(bad == (void *)1);   /* the stack leaves *ppv untouched on failure (probed), unlike the COM convention of NULL */
    ID3DXMatrixStack_Release(s);
}

/* ---- FVF / declarator ----------------------------------------------------------- */
TEST(d3dx_redist_optional_x9_fvf_vertex_size_table) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetFVFVertexSize));
    static const struct { DWORD fvf; UINT size; } t[] = {
        { D3DFVF_XYZ, 12 }, { D3DFVF_XYZ | D3DFVF_NORMAL, 24 }, { D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1, 32 },
        { D3DFVF_XYZ | D3DFVF_DIFFUSE, 16 }, { D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX2, 40 },
        { D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0), 24 }, { D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE1(0), 16 },
        { D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE4(0), 28 }, { D3DFVF_XYZB3, 24 }, { D3DFVF_XYZB1 | D3DFVF_LASTBETA_UBYTE4, 16 },
        { D3DFVF_XYZ | D3DFVF_PSIZE, 16 }, { D3DFVF_XYZ | D3DFVF_TEX8, 12 + 8 * 8 },
        { D3DFVF_XYZ | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE3(1), 12 + 8 + 12 },
        { D3DFVF_XYZB5, 12 + 20 }, { D3DFVF_XYZW, 0 } /* XYZW (0x4002) is not understood by D3DX 9.43: size 0, probed */, { D3DFVF_XYZ | D3DFVF_XYZRHW, 16 } /* == XYZB1 */, { 0, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        UINT got = F9(D3DXGetFVFVertexSize)(t[i].fvf);
        CHECKF(got == t[i].size, "fvf 0x%lx size got %u want %u", t[i].fvf, got, t[i].size);
    }
}
TEST(d3dx_redist_optional_x9_declarator_from_fvf) {
    NEED_DLL(X9); REQUIRE(F9(D3DXDeclaratorFromFVF));
    D3DVERTEXELEMENT9 d[MAX_FVF_DECL_SIZE]; memset(d, 0xEE, sizeof d);
    EXPECT_HR(F9(D3DXDeclaratorFromFVF)(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1, d), S_OK);
    EXPECT_EQ(d[0].Stream, 0); EXPECT_EQ(d[0].Offset, 0); EXPECT_EQ(d[0].Type, D3DDECLTYPE_FLOAT3); EXPECT_EQ(d[0].Usage, D3DDECLUSAGE_POSITION); EXPECT_EQ(d[0].UsageIndex, 0);
    EXPECT_EQ(d[1].Offset, 12); EXPECT_EQ(d[1].Type, D3DDECLTYPE_FLOAT3); EXPECT_EQ(d[1].Usage, D3DDECLUSAGE_NORMAL);
    EXPECT_EQ(d[2].Offset, 24); EXPECT_EQ(d[2].Type, D3DDECLTYPE_FLOAT2); EXPECT_EQ(d[2].Usage, D3DDECLUSAGE_TEXCOORD); EXPECT_EQ(d[2].UsageIndex, 0);
    EXPECT_EQ(d[3].Stream, 0xFF); EXPECT_EQ(d[3].Type, D3DDECLTYPE_UNUSED);  /* D3DDECL_END */
}
TEST(d3dx_redist_optional_x9_declarator_from_fvf_color) {
    NEED_DLL(X9); REQUIRE(F9(D3DXDeclaratorFromFVF));
    D3DVERTEXELEMENT9 d[MAX_FVF_DECL_SIZE];
    EXPECT_HR(F9(D3DXDeclaratorFromFVF)(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR, d), S_OK);
    EXPECT_EQ(d[0].Type, D3DDECLTYPE_FLOAT4); EXPECT_EQ(d[0].Usage, D3DDECLUSAGE_POSITIONT);
    EXPECT_EQ(d[1].Type, D3DDECLTYPE_D3DCOLOR); EXPECT_EQ(d[1].Usage, D3DDECLUSAGE_COLOR); EXPECT_EQ(d[1].UsageIndex, 0); EXPECT_EQ(d[1].Offset, 16);
    EXPECT_EQ(d[2].Type, D3DDECLTYPE_D3DCOLOR); EXPECT_EQ(d[2].UsageIndex, 1); EXPECT_EQ(d[2].Offset, 20);
    EXPECT_EQ(d[3].Stream, 0xFF);
}
TEST(d3dx_redist_optional_x9_fvf_from_declarator_roundtrip) {
    NEED_DLL(X9); REQUIRE(F9(D3DXDeclaratorFromFVF)); REQUIRE(F9(D3DXFVFFromDeclarator));
    static const DWORD fvfs[] = { D3DFVF_XYZ, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0), D3DFVF_XYZ | D3DFVF_DIFFUSE };
    for (unsigned i = 0; i < sizeof fvfs / sizeof *fvfs; i++) {
        D3DVERTEXELEMENT9 d[MAX_FVF_DECL_SIZE]; DWORD back = 0xDEAD;
        EXPECT_HR(F9(D3DXDeclaratorFromFVF)(fvfs[i], d), S_OK);
        EXPECT_HR(F9(D3DXFVFFromDeclarator)(d, &back), S_OK);
        CHECKF(back == fvfs[i], "fvf 0x%lx -> 0x%lx", fvfs[i], back);
        if (F9(D3DXGetDeclVertexSize)) CHECKF(F9(D3DXGetDeclVertexSize)(d, 0) == F9(D3DXGetFVFVertexSize)(fvfs[i]), "decl size vs fvf size");
    }
}

/* ---- geometry helpers ------------------------------------------------------------ */
TEST(d3dx_redist_optional_x9_compute_bounding_box) {
    NEED_DLL(X9); REQUIRE(F9(D3DXComputeBoundingBox));
    D3DXVECTOR3 v[6] = { {1,0,0}, {-1,0,0}, {0,2,0}, {0,-3,0}, {0,0,4}, {0,0,-5} }, mn, mx;
    EXPECT_HR(F9(D3DXComputeBoundingBox)(v, 6, sizeof v[0], &mn, &mx), S_OK);
    CHECKN(mn.x, -1.f); CHECKN(mn.y, -3.f); CHECKN(mn.z, -5.f); CHECKN(mx.x, 1.f); CHECKN(mx.y, 2.f); CHECKN(mx.z, 4.f);
}
TEST(d3dx_redist_optional_x9_compute_bounding_box_stride) {
    NEED_DLL(X9); REQUIRE(F9(D3DXComputeBoundingBox));
    struct { D3DXVECTOR3 p; float junk[3]; } v[3] = { {{0,0,0},{99,99,99}}, {{2,3,4},{-99,-99,-99}}, {{-1,5,1},{7,7,7}} };
    D3DXVECTOR3 mn, mx;
    EXPECT_HR(F9(D3DXComputeBoundingBox)(&v[0].p, 3, sizeof v[0], &mn, &mx), S_OK);
    CHECKN(mn.x, -1.f); CHECKN(mx.x, 2.f); CHECKN(mx.y, 5.f); CHECKN(mx.z, 4.f); CHECKN(mn.z, 0.f);
}
TEST(d3dx_redist_optional_x9_compute_bounding_box_invalid) {
    NEED_DLL(X9); REQUIRE(F9(D3DXComputeBoundingBox));
    D3DXVECTOR3 v = {0,0,0}, mn, mx;
    EXPECT_HR(F9(D3DXComputeBoundingBox)(NULL, 1, 12, &mn, &mx), INVALIDCALL);
    EXPECT_HR(F9(D3DXComputeBoundingBox)(&v, 1, 12, NULL, &mx), INVALIDCALL);
    EXPECT_HR(F9(D3DXComputeBoundingBox)(&v, 1, 12, &mn, NULL), INVALIDCALL);
}
TEST(d3dx_redist_optional_x9_compute_bounding_sphere) {
    NEED_DLL(X9); REQUIRE(F9(D3DXComputeBoundingSphere));
    D3DXVECTOR3 v[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} }, c; float r = -1;
    EXPECT_HR(F9(D3DXComputeBoundingSphere)(v, 6, sizeof v[0], &c, &r), S_OK);
    CHECKN(c.x, 0.f); CHECKN(c.y, 0.f); CHECKN(c.z, 0.f); CHECKN(r, 1.f);
    D3DXVECTOR3 s[2] = { {2,2,2}, {4,2,2} };
    EXPECT_HR(F9(D3DXComputeBoundingSphere)(s, 2, sizeof s[0], &c, &r), S_OK);
    CHECKN(c.x, 3.f); CHECKN(c.y, 2.f); CHECKN(r, 1.f);
}
TEST(d3dx_redist_optional_x9_compute_bounding_sphere_invalid) {
    NEED_DLL(X9); REQUIRE(F9(D3DXComputeBoundingSphere));
    D3DXVECTOR3 v = {0,0,0}, c; float r;
    EXPECT_HR(F9(D3DXComputeBoundingSphere)(NULL, 1, 12, &c, &r), INVALIDCALL);
    EXPECT_HR(F9(D3DXComputeBoundingSphere)(&v, 1, 12, NULL, &r), INVALIDCALL);
    EXPECT_HR(F9(D3DXComputeBoundingSphere)(&v, 1, 12, &c, NULL), INVALIDCALL);
}
TEST(d3dx_redist_optional_x9_intersect_tri) {
    NEED_DLL(X9); REQUIRE(F9(D3DXIntersectTri));
    D3DXVECTOR3 a = {0,0,0}, b = {1,0,0}, c = {0,1,0}, pos = {0.25f,0.25f,-1}, dir = {0,0,1}; float u = -1, v = -1, d = -1;
    CHECK(F9(D3DXIntersectTri)(&a, &b, &c, &pos, &dir, &u, &v, &d) == TRUE);
    CHECKN(u, 0.25f); CHECKN(v, 0.25f); CHECKN(d, 1.f);
    D3DXVECTOR3 dir2 = {0,0,3}; d = -1;
    F9(D3DXIntersectTri)(&a, &b, &c, &pos, &dir2, &u, &v, &d);   /* distance is in units of the direction length */
    CHECKNE(d, 1.f / 3.f, 1e-5f);
}
TEST(d3dx_redist_optional_x9_intersect_tri_barycentric_corners) {
    NEED_DLL(X9); REQUIRE(F9(D3DXIntersectTri));
    D3DXVECTOR3 a = {0,0,0}, b = {4,0,0}, c = {0,4,0}, dir = {0,0,1}; float u, v, d;
    D3DXVECTOR3 p1 = {2,1,-2};
    CHECK(F9(D3DXIntersectTri)(&a, &b, &c, &p1, &dir, &u, &v, &d) == TRUE);
    CHECKN(u, 0.5f); CHECKN(v, 0.25f); CHECKN(d, 2.f);
}
TEST(d3dx_redist_optional_x9_intersect_tri_miss) {
    NEED_DLL(X9); REQUIRE(F9(D3DXIntersectTri));
    D3DXVECTOR3 a = {0,0,0}, b = {1,0,0}, c = {0,1,0}, dir = {0,0,1}; float u, v, d;
    D3DXVECTOR3 outside = {2,2,-1}, behind = {0.25f,0.25f,1}, nearx = {0.75f,0.75f,-1};
    CHECK(F9(D3DXIntersectTri)(&a, &b, &c, &outside, &dir, &u, &v, &d) == FALSE);
    CHECK(F9(D3DXIntersectTri)(&a, &b, &c, &behind, &dir, &u, &v, &d) == FALSE);
    CHECK(F9(D3DXIntersectTri)(&a, &b, &c, &nearx, &dir, &u, &v, &d) == FALSE);   /* u+v > 1 */
    D3DXVECTOR3 par = {0,0,1}, pdir = {1,0,0};
    CHECK(F9(D3DXIntersectTri)(&a, &b, &c, &par, &pdir, &u, &v, &d) == FALSE);     /* parallel */
}
TEST(d3dx_redist_optional_x9_intersect_tri_backface_hit) {
    NEED_DLL(X9); REQUIRE(F9(D3DXIntersectTri));
    /* D3DX does not cull: hitting the back side still intersects */
    D3DXVECTOR3 a = {0,0,0}, b = {1,0,0}, c = {0,1,0}, pos = {0.25f,0.25f,1}, dir = {0,0,-1}; float u, v, d;
    CHECK(F9(D3DXIntersectTri)(&a, &b, &c, &pos, &dir, &u, &v, &d) == TRUE);
    CHECKN(d, 1.f);
}
TEST(d3dx_redist_optional_x9_box_bound_probe) {
    NEED_DLL(X9); REQUIRE(F9(D3DXBoxBoundProbe));
    D3DXVECTOR3 mn = {-1,-1,-1}, mx = {1,1,1}, dir = {0,0,1};
    D3DXVECTOR3 hit = {0,0,-5}, miss = {5,5,-5}, edge = {0.99f,0.99f,-5}, out = {1.5f,0,-5};
    CHECK(F9(D3DXBoxBoundProbe)(&mn, &mx, &hit, &dir) == TRUE);
    CHECK(F9(D3DXBoxBoundProbe)(&mn, &mx, &miss, &dir) == FALSE);
    CHECK(F9(D3DXBoxBoundProbe)(&mn, &mx, &edge, &dir) == TRUE);
    CHECK(F9(D3DXBoxBoundProbe)(&mn, &mx, &out, &dir) == FALSE);
    D3DXVECTOR3 inside = {0,0,0}; CHECK(F9(D3DXBoxBoundProbe)(&mn, &mx, &inside, &dir) == TRUE);
}
TEST(d3dx_redist_optional_x9_sphere_bound_probe) {
    NEED_DLL(X9); REQUIRE(F9(D3DXSphereBoundProbe));
    D3DXVECTOR3 c = {0,0,0}, dir = {0,0,1};
    D3DXVECTOR3 hit = {0.5f,0,-5}, miss = {1.5f,0,-5}, tang_in = {0.99f,0,-5};
    CHECK(F9(D3DXSphereBoundProbe)(&c, 1.f, &hit, &dir) == TRUE);
    CHECK(F9(D3DXSphereBoundProbe)(&c, 1.f, &miss, &dir) == FALSE);
    CHECK(F9(D3DXSphereBoundProbe)(&c, 1.f, &tang_in, &dir) == TRUE);
    CHECK(F9(D3DXSphereBoundProbe)(&c, 2.f, &miss, &dir) == TRUE);
}

/* ---- shader assemble / compile -------------------------------------------------------- */
static const char VS11[] = "vs_1_1\ndcl_position v0\nmov oPos, v0\n";
static const char PS20[] = "ps_2_0\nmov oC0, c0\n";
static const char HLSL_VS[] = "float4x4 wvp; float4 col;\nfloat4 main(float4 p : POSITION) : POSITION { return mul(p, wvp) + col; }\n";

TEST(d3dx_redist_optional_x9_assemble_vs11) {
    NEED_DLL(X9); REQUIRE(F9(D3DXAssembleShader)); REQUIRE(F9(D3DXGetShaderVersion)); REQUIRE(F9(D3DXGetShaderSize));
    ID3DXBuffer *sh = NULL, *err = NULL;
    HRESULT hr = F9(D3DXAssembleShader)(VS11, sizeof VS11 - 1, NULL, NULL, 0, &sh, &err);
    EXPECT_HR(hr, S_OK);
    if (!sh) { if (err) ID3DXBuffer_Release(err); return; }
    const DWORD *code = ID3DXBuffer_GetBufferPointer(sh); DWORD sz = ID3DXBuffer_GetBufferSize(sh);
    CHECK(sz >= 12 && (sz % 4) == 0);
    EXPECT_EQ(code[0], 0xFFFE0101);
    EXPECT_EQ(F9(D3DXGetShaderVersion)(code), 0xFFFE0101);
    EXPECT_EQ(code[sz / 4 - 1], 0x0000FFFF);   /* END token */
    EXPECT_EQ(F9(D3DXGetShaderSize)(code), sz);
    ID3DXBuffer_Release(sh);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_assemble_ps20) {
    NEED_DLL(X9); REQUIRE(F9(D3DXAssembleShader)); REQUIRE(F9(D3DXGetShaderVersion));
    ID3DXBuffer *sh = NULL, *err = NULL;
    EXPECT_HR(F9(D3DXAssembleShader)(PS20, sizeof PS20 - 1, NULL, NULL, 0, &sh, &err), S_OK);
    if (sh) {
        const DWORD *code = ID3DXBuffer_GetBufferPointer(sh);
        EXPECT_EQ(F9(D3DXGetShaderVersion)(code), 0xFFFF0200);
        ID3DXBuffer_Release(sh);
    }
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_assemble_instruction_tokens) {
    NEED_DLL(X9); REQUIRE(F9(D3DXAssembleShader));
    /* ps_1_1 'mov r0, v0' : version token, MOV opcode with length 2, dst r0 (full mask), src v0, END */
    static const char src[] = "ps_1_1\nmov r0, v0\n";
    ID3DXBuffer *sh = NULL, *err = NULL;
    EXPECT_HR(F9(D3DXAssembleShader)(src, sizeof src - 1, NULL, NULL, 0, &sh, &err), S_OK);
    if (sh) {
        const DWORD *c = ID3DXBuffer_GetBufferPointer(sh);
        EXPECT_EQ(c[0], 0xFFFF0101);
        EXPECT_EQ(c[1], 1);                          /* D3DSIO_MOV, no length field before sm2 */
        EXPECT_EQ(c[2], 0x800F0000);                 /* r0.xyzw */
        EXPECT_EQ(c[3], 0x90E40000);                 /* v0: D3DSPR_INPUT, identity swizzle */
        ID3DXBuffer_Release(sh);
    }
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_assemble_syntax_error) {
    NEED_DLL(X9); REQUIRE(F9(D3DXAssembleShader));
    static const char bad[] = "vs_1_1\nthisisnotanopcode r0, v0\n";
    ID3DXBuffer *sh = (ID3DXBuffer *)1, *err = NULL;
    HRESULT hr = F9(D3DXAssembleShader)(bad, sizeof bad - 1, NULL, NULL, 0, &sh, &err);
    CHECK(FAILED(hr)); t_note("assemble error hr = %s", d3dh_hr_name(hr));
    CHECK(err != NULL);
    if (err) { const char *txt = ID3DXBuffer_GetBufferPointer(err); t_note("assembler message: %.120s", txt ? txt : "(null)"); ID3DXBuffer_Release(err); }
    if (SUCCEEDED(hr) && sh) ID3DXBuffer_Release(sh);
}
TEST(d3dx_redist_optional_x9_assemble_null_source) {
    NEED_DLL(X9); REQUIRE(F9(D3DXAssembleShader));
    ID3DXBuffer *sh = NULL, *err = NULL;
    HRESULT hr = F9(D3DXAssembleShader)(NULL, 0, NULL, NULL, 0, &sh, &err);
    CHECK(FAILED(hr)); t_note("AssembleShader(NULL, 0) -> %s", d3dh_hr_name(hr));
    if (sh) ID3DXBuffer_Release(sh);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_assemble_defines) {
    NEED_DLL(X9); REQUIRE(F9(D3DXAssembleShader));
    static const char src[] = "vs_1_1\ndcl_position v0\n#ifdef USE_MOV\nmov oPos, v0\n#else\nadd oPos, v0, v0\n#endif\n";
    D3DXMACRO defs[] = { { "USE_MOV", "1" }, { NULL, NULL } };
    ID3DXBuffer *a = NULL, *b = NULL, *e = NULL;
    EXPECT_HR(F9(D3DXAssembleShader)(src, sizeof src - 1, defs, NULL, 0, &a, &e), S_OK);
    if (e) { ID3DXBuffer_Release(e); e = NULL; }
    EXPECT_HR(F9(D3DXAssembleShader)(src, sizeof src - 1, NULL, NULL, 0, &b, &e), S_OK);
    if (a && b) {
        const DWORD *ca = ID3DXBuffer_GetBufferPointer(a), *cb = ID3DXBuffer_GetBufferPointer(b);
        CHECK(ca[2] != cb[2] || ID3DXBuffer_GetBufferSize(a) != ID3DXBuffer_GetBufferSize(b));   /* different opcodes: mov vs add */
    }
    if (a) ID3DXBuffer_Release(a); if (b) ID3DXBuffer_Release(b); if (e) ID3DXBuffer_Release(e);
}
TEST(d3dx_redist_optional_x9_get_shader_version_variants) {
    NEED_DLL(X9); REQUIRE(F9(D3DXAssembleShader)); REQUIRE(F9(D3DXGetShaderVersion));
    static const struct { const char *src; DWORD ver; } t[] = {
        { "vs_1_1\ndcl_position v0\nmov oPos, v0\n", 0xFFFE0101 },
        { "vs_2_0\ndcl_position v0\nmov oPos, v0\n", 0xFFFE0200 },
        { "vs_3_0\ndcl_position v0\ndcl_position o0\nmov o0, v0\n", 0xFFFE0300 },
        { "ps_1_4\nmov r0, c0\n", 0xFFFF0104 },
        { "ps_2_0\nmov oC0, c0\n", 0xFFFF0200 },
        { "ps_3_0\nmov oC0, c0\n", 0xFFFF0300 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ID3DXBuffer *sh = NULL, *e = NULL;
        HRESULT hr = F9(D3DXAssembleShader)(t[i].src, (UINT)strlen(t[i].src), NULL, NULL, 0, &sh, &e);
        CHECKF(SUCCEEDED(hr), "case %u hr %s", i, d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && sh) CHECKF(F9(D3DXGetShaderVersion)(ID3DXBuffer_GetBufferPointer(sh)) == t[i].ver, "case %u version", i);
        if (sh) ID3DXBuffer_Release(sh); if (e) ID3DXBuffer_Release(e);
    }
}
TEST(d3dx_redist_optional_x9_shader_version_null_and_bad) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetShaderVersion)); REQUIRE(F9(D3DXGetShaderSize));
    t_note("GetShaderVersion(NULL)=%lu GetShaderSize(NULL)=%u", F9(D3DXGetShaderVersion)(NULL), F9(D3DXGetShaderSize)(NULL));
    DWORD junk[2] = { 0x12345678, 0 };
    t_note("GetShaderVersion(junk token) = 0x%08lx", F9(D3DXGetShaderVersion)(junk));
}
TEST(d3dx_redist_optional_x9_compile_shader_vs20) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader)); REQUIRE(F9(D3DXGetShaderVersion));
    ID3DXBuffer *sh = NULL, *err = NULL; ID3DXConstantTable *ct = NULL;
    HRESULT hr = F9(D3DXCompileShader)(HLSL_VS, sizeof HLSL_VS - 1, NULL, NULL, "main", "vs_2_0", 0, &sh, &err, &ct);
    EXPECT_HR(hr, S_OK);
    if (sh) {
        EXPECT_EQ(F9(D3DXGetShaderVersion)(ID3DXBuffer_GetBufferPointer(sh)), 0xFFFE0200);
        ID3DXBuffer_Release(sh);
    }
    CHECK(ct != NULL);
    if (ct) ID3DXConstantTable_Release(ct);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_compile_shader_constant_table) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader));
    ID3DXBuffer *sh = NULL, *err = NULL; ID3DXConstantTable *ct = NULL;
    if (FAILED(F9(D3DXCompileShader)(HLSL_VS, sizeof HLSL_VS - 1, NULL, NULL, "main", "vs_2_0", 0, &sh, &err, &ct)) || !ct) { t_skip("compile failed"); return; }
    D3DXCONSTANTTABLE_DESC d; memset(&d, 0, sizeof d);
    EXPECT_HR(ID3DXConstantTable_GetDesc(ct, &d), S_OK);
    EXPECT_EQ(d.Constants, 2); EXPECT_EQ(d.Version, 0xFFFE0200); CHECK(d.Creator != NULL);
    if (d.Creator) t_note("constant table creator: %s", d.Creator);
    D3DXHANDLE hw = ID3DXConstantTable_GetConstantByName(ct, NULL, "wvp"), hc = ID3DXConstantTable_GetConstantByName(ct, NULL, "col");
    CHECK(hw != NULL); CHECK(hc != NULL);
    D3DXCONSTANT_DESC cd; UINT n = 1;
    if (hw) {
        memset(&cd, 0, sizeof cd); n = 1;
        EXPECT_HR(ID3DXConstantTable_GetConstantDesc(ct, hw, &cd, &n), S_OK);
        EXPECT_STR(cd.Name, "wvp"); EXPECT_EQ(cd.RegisterSet, D3DXRS_FLOAT4); EXPECT_EQ(cd.RegisterCount, 4);
        EXPECT_EQ(cd.Rows, 4); EXPECT_EQ(cd.Columns, 4); EXPECT_EQ(cd.Elements, 1); EXPECT_EQ(cd.Type, D3DXPT_FLOAT); EXPECT_EQ(cd.Bytes, 64);
        t_note("wvp class %d register %u", cd.Class, cd.RegisterIndex);
    }
    if (hc) {
        memset(&cd, 0, sizeof cd); n = 1;
        EXPECT_HR(ID3DXConstantTable_GetConstantDesc(ct, hc, &cd, &n), S_OK);
        EXPECT_STR(cd.Name, "col"); EXPECT_EQ(cd.RegisterSet, D3DXRS_FLOAT4); EXPECT_EQ(cd.RegisterCount, 1);
        EXPECT_EQ(cd.Class, D3DXPC_VECTOR); EXPECT_EQ(cd.Type, D3DXPT_FLOAT); EXPECT_EQ(cd.Rows, 1); EXPECT_EQ(cd.Columns, 4);
        t_note("col register %u", cd.RegisterIndex);
    }
    CHECK(ID3DXConstantTable_GetConstantByName(ct, NULL, "nosuchconstant") == NULL);
    ID3DXConstantTable_Release(ct); if (sh) ID3DXBuffer_Release(sh);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_get_shader_constant_table_matches_compile) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader)); REQUIRE(F9(D3DXGetShaderConstantTable));
    ID3DXBuffer *sh = NULL, *err = NULL; ID3DXConstantTable *ct = NULL, *ct2 = NULL;
    if (FAILED(F9(D3DXCompileShader)(HLSL_VS, sizeof HLSL_VS - 1, NULL, NULL, "main", "vs_2_0", 0, &sh, &err, &ct)) || !sh) { t_skip("compile failed"); return; }
    EXPECT_HR(F9(D3DXGetShaderConstantTable)(ID3DXBuffer_GetBufferPointer(sh), &ct2), S_OK);
    if (ct2) {
        D3DXCONSTANTTABLE_DESC d; EXPECT_HR(ID3DXConstantTable_GetDesc(ct2, &d), S_OK); EXPECT_EQ(d.Constants, 2);
        ID3DXConstantTable_Release(ct2);
    }
    if (ct) ID3DXConstantTable_Release(ct); ID3DXBuffer_Release(sh);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_get_shader_constant_table_none) {
    NEED_DLL(X9); REQUIRE(F9(D3DXAssembleShader)); REQUIRE(F9(D3DXGetShaderConstantTable));
    ID3DXBuffer *sh = NULL, *e = NULL; ID3DXConstantTable *ct = (ID3DXConstantTable *)1;
    if (FAILED(F9(D3DXAssembleShader)(PS20, sizeof PS20 - 1, NULL, NULL, 0, &sh, &e))) { t_skip("assemble failed"); return; }
    HRESULT hr = F9(D3DXGetShaderConstantTable)(ID3DXBuffer_GetBufferPointer(sh), &ct);
    t_note("GetShaderConstantTable on an assembled shader without CTAB -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && ct) ID3DXConstantTable_Release(ct);
    ID3DXBuffer_Release(sh); if (e) ID3DXBuffer_Release(e);
}
TEST(d3dx_redist_optional_x9_get_shader_constant_table_null) {
    NEED_DLL(X9); REQUIRE(F9(D3DXGetShaderConstantTable));
    ID3DXConstantTable *ct = NULL;
    EXPECT_HR(F9(D3DXGetShaderConstantTable)(NULL, &ct), INVALIDCALL);
}
TEST(d3dx_redist_optional_x9_compile_shader_syntax_error) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader));
    static const char bad[] = "float4 main( : POSITION { return 1 }";
    ID3DXBuffer *sh = NULL, *err = NULL; ID3DXConstantTable *ct = NULL;
    HRESULT hr = F9(D3DXCompileShader)(bad, sizeof bad - 1, NULL, NULL, "main", "vs_2_0", 0, &sh, &err, &ct);
    CHECK(FAILED(hr)); CHECK(sh == NULL); CHECK(err != NULL);
    t_note("syntax error hr %s", d3dh_hr_name(hr));
    if (err) { t_note("compiler message: %.160s", (const char *)ID3DXBuffer_GetBufferPointer(err)); ID3DXBuffer_Release(err); }
}
TEST(d3dx_redist_optional_x9_compile_shader_bad_profile_and_entry) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader));
    ID3DXBuffer *sh = NULL, *err = NULL; ID3DXConstantTable *ct = NULL;
    HRESULT hr = F9(D3DXCompileShader)(HLSL_VS, sizeof HLSL_VS - 1, NULL, NULL, "main", "xs_9_9", 0, &sh, &err, &ct);
    CHECK(FAILED(hr)); CHECK(sh == NULL);
    if (err) { ID3DXBuffer_Release(err); err = NULL; }
    hr = F9(D3DXCompileShader)(HLSL_VS, sizeof HLSL_VS - 1, NULL, NULL, "nosuchentry", "vs_2_0", 0, &sh, &err, &ct);
    CHECK(FAILED(hr)); CHECK(sh == NULL);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_compile_shader_null_args) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader));
    ID3DXBuffer *sh = NULL, *err = NULL;
    HRESULT hr = F9(D3DXCompileShader)(NULL, 0, NULL, NULL, "main", "vs_2_0", 0, &sh, &err, NULL);
    CHECK(FAILED(hr)); t_note("CompileShader(NULL source) -> %s", d3dh_hr_name(hr));
    if (sh) ID3DXBuffer_Release(sh);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_compile_shader_ps_profiles) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader)); REQUIRE(F9(D3DXGetShaderVersion));
    static const char src[] = "float4 c;\nfloat4 main() : COLOR { return c; }\n";
    static const struct { const char *prof; DWORD ver; } t[] = { { "ps_2_0", 0xFFFF0200 }, { "ps_2_a", 0xFFFF0200 }, { "ps_3_0", 0xFFFF0300 } };
    for (unsigned i = 0; i < 3; i++) {
        ID3DXBuffer *sh = NULL, *err = NULL; ID3DXConstantTable *ct = NULL;
        HRESULT hr = F9(D3DXCompileShader)(src, sizeof src - 1, NULL, NULL, "main", t[i].prof, 0, &sh, &err, &ct);
        CHECKF(SUCCEEDED(hr), "%s hr %s", t[i].prof, d3dh_hr_name(hr));
        if (sh) { t_note("%s version token 0x%08lx", t[i].prof, F9(D3DXGetShaderVersion)(ID3DXBuffer_GetBufferPointer(sh))); ID3DXBuffer_Release(sh); }
        if (ct) ID3DXConstantTable_Release(ct);
        if (err) ID3DXBuffer_Release(err);
    }
}
TEST(d3dx_redist_optional_x9_compile_shader_defines_and_flags) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader));
    static const char src[] = "float4 main(float4 p : POSITION) : POSITION { return p * SCALE; }\n";
    D3DXMACRO defs[] = { { "SCALE", "2.0" }, { NULL, NULL } };
    ID3DXBuffer *sh = NULL, *err = NULL;
    EXPECT_HR(F9(D3DXCompileShader)(src, sizeof src - 1, defs, NULL, "main", "vs_2_0", D3DXSHADER_SKIPVALIDATION, &sh, &err, NULL), S_OK);
    if (sh) ID3DXBuffer_Release(sh);
    if (err) { ID3DXBuffer_Release(err); err = NULL; }
    HRESULT hr = F9(D3DXCompileShader)(src, sizeof src - 1, NULL, NULL, "main", "vs_2_0", 0, &sh, &err, NULL);   /* SCALE undefined */
    CHECK(FAILED(hr));
    if (SUCCEEDED(hr) && sh) ID3DXBuffer_Release(sh);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_compile_shader_disassemble) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader)); REQUIRE(F9(D3DXDisassembleShader));
    ID3DXBuffer *sh = NULL, *err = NULL, *dis = NULL;
    if (FAILED(F9(D3DXCompileShader)(HLSL_VS, sizeof HLSL_VS - 1, NULL, NULL, "main", "vs_2_0", 0, &sh, &err, NULL))) { t_skip("compile failed"); return; }
    EXPECT_HR(F9(D3DXDisassembleShader)(ID3DXBuffer_GetBufferPointer(sh), FALSE, NULL, &dis), S_OK);
    if (dis) {
        const char *txt = ID3DXBuffer_GetBufferPointer(dis);
        CHECK(strstr(txt, "vs_2_0") != NULL); CHECK(strstr(txt, "dp4") != NULL || strstr(txt, "mul") != NULL);
        ID3DXBuffer_Release(dis);
    }
    ID3DXBuffer_Release(sh);
    if (err) ID3DXBuffer_Release(err);
}
TEST(d3dx_redist_optional_x9_find_shader_comment_ctab) {
    NEED_DLL(X9); REQUIRE(F9(D3DXCompileShader)); REQUIRE(F9(D3DXFindShaderComment));
    ID3DXBuffer *sh = NULL, *err = NULL;
    if (FAILED(F9(D3DXCompileShader)(HLSL_VS, sizeof HLSL_VS - 1, NULL, NULL, "main", "vs_2_0", 0, &sh, &err, NULL))) { t_skip("compile failed"); return; }
    const void *data = NULL; UINT size = 0;
    EXPECT_HR(F9(D3DXFindShaderComment)(ID3DXBuffer_GetBufferPointer(sh), MAKEFOURCC('C','T','A','B'), &data, &size), S_OK);
    CHECK(data != NULL && size >= 28);
    if (data && size >= 28) EXPECT_EQ(*(const DWORD *)data, 28);   /* CTAB header size */
    HRESULT hr2 = F9(D3DXFindShaderComment)(ID3DXBuffer_GetBufferPointer(sh), MAKEFOURCC('N','O','P','E'), &data, &size);
    t_note("FindShaderComment for an absent fourcc -> %s", d3dh_hr_name(hr2));
    ID3DXBuffer_Release(sh);
    if (err) ID3DXBuffer_Release(err);
}
