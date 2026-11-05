/* D3D10CreateBlob and the ID3D10Blob object. */
#include "ue.h"

static const GUID k_iid_blob = {0x8BA5FB08, 0x5195, 0x40e2, {0xAC, 0x58, 0x0D, 0x98, 0x9C, 0x3A, 0x01, 0x02}};

TEST(d3d10_utils_effects_blob_null_out)
{
    REQUIRE(U(D3D10CreateBlob));
    EXPECT_HR(U(D3D10CreateBlob)(16, NULL), E_D3DERR_INVALIDCALL);
    EXPECT_HR(U(D3D10CreateBlob)(0, NULL), E_D3DERR_INVALIDCALL);
}
TEST(d3d10_utils_effects_blob_null_out_d3d10_1)
{
    REQUIRE(UIN("d3d10_1.dll", D3D10CreateBlob));
    EXPECT_HR(UIN("d3d10_1.dll", D3D10CreateBlob)(16, NULL), E_D3DERR_INVALIDCALL);
}
TEST(d3d10_utils_effects_blob_size_roundtrip)
{
    static const SIZE_T sizes[] = { 1, 2, 3, 4, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 100, 255, 256, 257, 1000, 4096, 4097, 65536, 1u << 20 };
    REQUIRE(U(D3D10CreateBlob));
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        ID3D10Blob *b = (ID3D10Blob *)(ULONG_PTR)1;
        HRESULT hr = U(D3D10CreateBlob)(sizes[i], &b);
        EXPECT_HR(hr, S_OK);
        if (FAILED(hr)) continue;
        EXPECT_EQ(ID3D10Blob_GetBufferSize(b), sizes[i]);
        CHECK(ID3D10Blob_GetBufferPointer(b) != NULL);
        EQV(D3DH_RELEASE_RC(b), 0);
    }
}
TEST(d3d10_utils_effects_blob_zero_size)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *b = NULL;
    HRESULT hr = U(D3D10CreateBlob)(0, &b);
    EXPECT_HR(hr, S_OK);
    if (b) {
        EXPECT_EQ(ID3D10Blob_GetBufferSize(b), 0);
        if (!ID3D10Blob_GetBufferPointer(b)) t_note("zero-size blob has a NULL buffer pointer");
        EQV(D3DH_RELEASE_RC(b), 0);
    }
}
TEST(d3d10_utils_effects_blob_memory_writable)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *b = NULL;
    EXPECT_HR(U(D3D10CreateBlob)(4096, &b), S_OK);
    if (!b) return;
    unsigned char *p = ID3D10Blob_GetBufferPointer(b);
    for (int i = 0; i < 4096; i++) p[i] = (unsigned char)(i * 7 + 3);
    p = ID3D10Blob_GetBufferPointer(b);
    int ok = 1;
    for (int i = 0; i < 4096; i++) if (p[i] != (unsigned char)(i * 7 + 3)) ok = 0;
    CHECK(ok);
    ID3D10Blob_Release(b);
}
TEST(d3d10_utils_effects_blob_pointer_stable)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *b = NULL;
    EXPECT_HR(U(D3D10CreateBlob)(64, &b), S_OK);
    if (!b) return;
    void *p1 = ID3D10Blob_GetBufferPointer(b), *p2 = ID3D10Blob_GetBufferPointer(b);
    EXPECT_EQ(p1, p2);
    EXPECT_EQ(ID3D10Blob_GetBufferSize(b), ID3D10Blob_GetBufferSize(b));
    ID3D10Blob_Release(b);
}
TEST(d3d10_utils_effects_blob_distinct_buffers)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *a = NULL, *b = NULL;
    EXPECT_HR(U(D3D10CreateBlob)(32, &a), S_OK);
    EXPECT_HR(U(D3D10CreateBlob)(32, &b), S_OK);
    if (a && b) {
        CHECK(a != b);
        CHECK(ID3D10Blob_GetBufferPointer(a) != ID3D10Blob_GetBufferPointer(b));
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
TEST(d3d10_utils_effects_blob_refcount)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *b = NULL;
    EXPECT_HR(U(D3D10CreateBlob)(8, &b), S_OK);
    if (!b) return;
    EQV(ID3D10Blob_AddRef(b), 2);
    EQV(ID3D10Blob_AddRef(b), 3);
    EQV(ID3D10Blob_Release(b), 2);
    EQV(ID3D10Blob_Release(b), 1);
    EQV(ID3D10Blob_Release(b), 0);
}
TEST(d3d10_utils_effects_blob_qi_iunknown)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *b = NULL;
    EXPECT_HR(U(D3D10CreateBlob)(8, &b), S_OK);
    if (!b) return;
    void *o = NULL;
    EXPECT_HR(ID3D10Blob_QueryInterface(b, &IID_IUnknown, &o), S_OK);
    EXPECT_EQ(o, b);
    EQV(ID3D10Blob_Release(b), 1);
    EQV(ID3D10Blob_Release(b), 0);
}
TEST(d3d10_utils_effects_blob_qi_blob_iid)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *b = NULL;
    EXPECT_HR(U(D3D10CreateBlob)(8, &b), S_OK);
    if (!b) return;
    void *o = NULL;
    EXPECT_HR(ID3D10Blob_QueryInterface(b, &k_iid_blob, &o), S_OK);
    EXPECT_EQ(o, b);
    EQV(ID3D10Blob_Release(b), 1);
    EQV(ID3D10Blob_Release(b), 0);
}
TEST(d3d10_utils_effects_blob_qi_unsupported)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *b = NULL;
    EXPECT_HR(U(D3D10CreateBlob)(8, &b), S_OK);
    if (!b) return;
    static const GUID *iids[] = { &IID_ID3D10Device, &IID_ID3D10Buffer, &IID_IDXGIObject, &IID_ID3D10ShaderReflection, &IID_ID3D10Effect, &IID_IClassFactory };
    for (unsigned i = 0; i < sizeof iids / sizeof *iids; i++) {
        void *o = (void *)(ULONG_PTR)0x1234;
        HRESULT hr = ID3D10Blob_QueryInterface(b, iids[i], &o);
        EXPECT_HR(hr, E_NOINTERFACE);
        EXPECT_EQ(o, NULL);
    }
    EQV(ID3D10Blob_Release(b), 0);
}
TEST(d3d10_utils_effects_blob_cross_dll_same_behaviour)
{
    REQUIRE(UIN("d3d10.dll", D3D10CreateBlob)); REQUIRE(UIN("d3d10_1.dll", D3D10CreateBlob));
    ID3D10Blob *a = NULL, *b = NULL;
    EXPECT_HR(UIN("d3d10.dll", D3D10CreateBlob)(123, &a), S_OK);
    EXPECT_HR(UIN("d3d10_1.dll", D3D10CreateBlob)(123, &b), S_OK);
    if (a && b) EXPECT_EQ(ID3D10Blob_GetBufferSize(a), ID3D10Blob_GetBufferSize(b));
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}
TEST(d3d10_utils_effects_blob_many_alloc_free)
{
    REQUIRE(U(D3D10CreateBlob));
    ID3D10Blob *bl[64] = {0};
    for (int i = 0; i < 64; i++) {
        EXPECT_HR(U(D3D10CreateBlob)(100 + i * 13, &bl[i]), S_OK);
        if (bl[i]) memset(ID3D10Blob_GetBufferPointer(bl[i]), i, 100 + i * 13);
    }
    for (int i = 0; i < 64; i++) {
        if (!bl[i]) continue;
        const unsigned char *p = ID3D10Blob_GetBufferPointer(bl[i]);
        CHECK(p[0] == i && p[99 + i * 13] == i);
        EQV(ID3D10Blob_Release(bl[i]), 0);
    }
}
TEST(d3d10_utils_effects_blob_matches_d3dcompiler_blob_iid)
{
    /* The blob type is shared with d3dcompiler: a blob made by D3DCreateBlob answers the same IID. */
    REQUIRE(d3dh_D3DCreateBlob()); REQUIRE(U(D3D10CreateBlob));
    ID3DBlob *c = NULL;
    EXPECT_HR(d3dh_D3DCreateBlob()(16, &c), S_OK);
    if (!c) return;
    void *o = NULL;
    EXPECT_HR(ID3D10Blob_QueryInterface((ID3D10Blob *)c, &k_iid_blob, &o), S_OK);
    if (o) ID3D10Blob_Release((ID3D10Blob *)o);
    EXPECT_EQ(ID3D10Blob_GetBufferSize((ID3D10Blob *)c), 16);
    ID3D10Blob_Release((ID3D10Blob *)c);
}
