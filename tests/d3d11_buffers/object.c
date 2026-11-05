/* Buffer object identity, private data, eviction priority and pipeline binding roundtrips. */
#include "bufh.h"

#define VB D3D11_BIND_VERTEX_BUFFER
#define IB D3D11_BIND_INDEX_BUFFER
#define CB D3D11_BIND_CONSTANT_BUFFER
#define SR D3D11_BIND_SHADER_RESOURCE
#define SO D3D11_BIND_STREAM_OUTPUT
#define DEF D3D11_USAGE_DEFAULT
#define RES(b) ((ID3D11Resource *)(b))

static const GUID G1 = { 0x11111111, 0x1111, 0x1111, { 1, 1, 1, 1, 1, 1, 1, 1 } };
static const GUID G2 = { 0x22222222, 0x2222, 0x2222, { 2, 2, 2, 2, 2, 2, 2, 2 } };
static const GUID G_NAME = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 } };

static ID3D11Buffer *mkb(UINT bw, UINT bind)
{
    D3D11_BUFFER_DESC d = bufh_desc(bw, DEF, bind, 0, 0, 0);
    ID3D11Buffer *b = NULL;
    if (!bufh_init()) return NULL;
    if (FAILED(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b))) return NULL;
    return b;
}

/* ---- COM ---- */
TEST(d3d11_buffers_com_initial_refcount)
{
    ID3D11Buffer *b = mkb(64, VB);
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_EQ(ID3D11Buffer_AddRef(b), 2);
    EXPECT_EQ(ID3D11Buffer_Release(b), 1);
    EXPECT_EQ(ID3D11Buffer_Release(b), 0);
}

TEST(d3d11_buffers_com_addref_release_sequence)
{
    ID3D11Buffer *b = mkb(64, VB);
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_EQ(ID3D11Buffer_AddRef(b), 2);
    EXPECT_EQ(ID3D11Buffer_AddRef(b), 3);
    EXPECT_EQ(ID3D11Buffer_AddRef(b), 4);
    EXPECT_EQ(ID3D11Buffer_Release(b), 3);
    EXPECT_EQ(ID3D11Buffer_Release(b), 2);
    EXPECT_EQ(ID3D11Buffer_Release(b), 1);
    EXPECT_EQ(ID3D11Buffer_Release(b), 0);
}

#define QI_T(nm, iid, want) \
TEST(d3d11_buffers_com_qi_##nm) { \
    ID3D11Buffer *b = mkb(64, VB); void *o = (void *)1; HRESULT hr; \
    NEED_DEV(); CHECK(b != NULL); if (!b) return; \
    hr = ID3D11Buffer_QueryInterface(b, &iid, &o); EXPECT_HR(hr, want); \
    if (SUCCEEDED(hr)) { CHECK(o != NULL); if (o) ((IUnknown *)o)->lpVtbl->Release((IUnknown *)o); } else CHECK(o == NULL); \
    SAFE_RELEASE(b); }
QI_T(iunknown, IID_IUnknown, S_OK)
QI_T(devicechild, IID_ID3D11DeviceChild, S_OK)
QI_T(resource, IID_ID3D11Resource, S_OK)
QI_T(buffer, IID_ID3D11Buffer, S_OK)
QI_T(texture1d, IID_ID3D11Texture1D, E_NOINTERFACE)
QI_T(texture2d, IID_ID3D11Texture2D, E_NOINTERFACE)
QI_T(texture3d, IID_ID3D11Texture3D, E_NOINTERFACE)
QI_T(view, IID_ID3D11View, E_NOINTERFACE)
QI_T(device, IID_ID3D11Device, E_NOINTERFACE)
QI_T(context, IID_ID3D11DeviceContext, E_NOINTERFACE)

TEST(d3d11_buffers_com_qi_null_out_guid_zero)
{
    ID3D11Buffer *b = mkb(64, VB);
    void *o = (void *)1;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_QueryInterface(b, &GUID_NULL, &o), E_NOINTERFACE);
    CHECK(o == NULL);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_com_qi_refcounts)
{
    ID3D11Buffer *b = mkb(64, VB);
    ID3D11Resource *r = NULL;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_QueryInterface(b, &IID_ID3D11Resource, (void **)&r), S_OK);
    if (r) {
        EXPECT_EQ(d3dh_refcount(b), 2);
        EXPECT_EQ(ID3D11Resource_Release(r), 1);
    }
    EXPECT_EQ(ID3D11Buffer_Release(b), 0);
}

TEST(d3d11_buffers_com_iunknown_identity)
{
    ID3D11Buffer *b = mkb(64, VB);
    IUnknown *u1 = NULL, *u2 = NULL;
    ID3D11Resource *r = NULL;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_QueryInterface(b, &IID_IUnknown, (void **)&u1), S_OK);
    EXPECT_HR(ID3D11Buffer_QueryInterface(b, &IID_ID3D11Resource, (void **)&r), S_OK);
    if (r) EXPECT_HR(ID3D11Resource_QueryInterface(r, &IID_IUnknown, (void **)&u2), S_OK);
    CHECK(u1 && u2 && u1 == u2);
    CHECK((void *)u1 == (void *)b);
    if (u1) u1->lpVtbl->Release(u1);
    if (u2) u2->lpVtbl->Release(u2);
    SAFE_RELEASE(r);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_com_qi_dxgi_note)
{
    ID3D11Buffer *b = mkb(64, VB);
    IUnknown *o = NULL;
    HRESULT hr;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    hr = ID3D11Buffer_QueryInterface(b, &IID_IDXGIObject, (void **)&o);
    t_note("buffer QI IDXGIObject: %s", bufh_hrn(hr)); if (o) { o->lpVtbl->Release(o); o = NULL; }
    hr = ID3D11Buffer_QueryInterface(b, &IID_IDXGIResource, (void **)&o);
    t_note("buffer QI IDXGIResource: %s", bufh_hrn(hr)); if (o) { o->lpVtbl->Release(o); o = NULL; }
    hr = ID3D11Buffer_QueryInterface(b, &IID_IDXGISurface, (void **)&o);
    t_note("buffer QI IDXGISurface: %s", bufh_hrn(hr)); if (o) { o->lpVtbl->Release(o); o = NULL; }
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_getdevice)
{
    ID3D11Buffer *b = mkb(64, VB);
    ID3D11Device *d = NULL;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11Buffer_GetDevice(b, &d);
    CHECK(d == g_dev);
    SAFE_RELEASE(d);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_getdevice_addrefs)
{
    ID3D11Buffer *b = mkb(64, VB);
    ID3D11Device *d = NULL;
    ULONG before, after;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    before = d3dh_refcount(g_dev);
    ID3D11Buffer_GetDevice(b, &d);
    after = d3dh_refcount(g_dev);
    EXPECT_EQ(after, before + 1);
    SAFE_RELEASE(d);
    EXPECT_EQ(d3dh_refcount(g_dev), before);
    SAFE_RELEASE(b);
}

/* ---- private data ---- */
TEST(d3d11_buffers_private_data_roundtrip)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE in[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, out[8] = { 0 };
    UINT sz = 8;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 8, in), S_OK);
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, out), S_OK);
    EXPECT_EQ(sz, 8);
    CHECK(!memcmp(in, out, 8));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_size_query)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE in[12] = { 0 }, out[4];
    UINT sz = 0;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 12, in), S_OK);
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, NULL), S_OK);
    t_note("GetPrivateData(NULL data): size=%u", sz);
    sz = 4;
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 12);
    sz = 0;
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(sz, 12);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_not_found)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE out[8];
    UINT sz = 8;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G2, &sz, out), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(sz, 0);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_exact_size_ok)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE in[5] = { 9, 8, 7, 6, 5 }, out[16];
    UINT sz = 16;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 5, in), S_OK);
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, out), S_OK);   /* larger buffer than needed */
    EXPECT_EQ(sz, 5);
    CHECK(!memcmp(in, out, 5));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_replace)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE a[4] = { 1, 1, 1, 1 }, c[2] = { 2, 3 }, out[8];
    UINT sz = 8;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 4, a), S_OK);
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 2, c), S_OK);
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, out), S_OK);
    EXPECT_EQ(sz, 2);
    CHECK(out[0] == 2 && out[1] == 3);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_two_guids)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE a[4] = { 1, 1, 1, 1 }, c[3] = { 2, 3, 4 }, out[8];
    UINT sz = 8;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 4, a), S_OK);
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G2, 3, c), S_OK);
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, out), S_OK);
    EXPECT_EQ(sz, 4);
    sz = 8;
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G2, &sz, out), S_OK);
    EXPECT_EQ(sz, 3);
    CHECK(out[0] == 2);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_remove_with_null)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE a[4] = { 1, 1, 1, 1 }, out[8];
    UINT sz = 8;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 4, a), S_OK);
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 0, NULL), S_OK);
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, out), DXGI_ERROR_NOT_FOUND);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_debug_name)
{
    ID3D11Buffer *b = mkb(64, VB);
    static const char name[] = "buffers_test_name";
    char out[64] = { 0 };
    UINT sz = sizeof out;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G_NAME, sizeof name - 1, name), S_OK);
    EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G_NAME, &sz, out), S_OK);
    EXPECT_EQ(sz, sizeof name - 1);
    CHECK(!memcmp(out, name, sizeof name - 1));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_zero_size_nonnull)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE a[1] = { 5 };
    UINT sz = 4;
    BYTE out[4];
    HRESULT hr;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    hr = ID3D11Buffer_SetPrivateData(b, &G1, 0, a);
    t_note("SetPrivateData size 0 with non-NULL data: %s", bufh_hrn(hr));
    hr = ID3D11Buffer_GetPrivateData(b, &G1, &sz, out);
    t_note("then GetPrivateData: %s size=%u", bufh_hrn(hr), sz);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_large)
{
    ID3D11Buffer *b = mkb(64, VB);
    BYTE *in = malloc(65536), *out = malloc(65536);
    UINT sz = 65536;
    NEED_DEV();
    CHECK(b != NULL);
    if (b && in && out) {
        bufh_pattern(in, 65536, 3);
        EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 65536, in), S_OK);
        EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, out), S_OK);
        EXPECT_EQ(sz, 65536);
        CHECK(!memcmp(in, out, 65536));
    }
    free(in); free(out);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_private_data_interface_addref)
{
    ID3D11Buffer *b = mkb(64, VB);
    ID3D11Buffer *obj = mkb(32, VB);
    IUnknown *got = NULL;
    UINT sz = sizeof got;
    NEED_DEV();
    CHECK(b && obj);
    if (b && obj) {
        EXPECT_EQ(d3dh_refcount(obj), 1);
        EXPECT_HR(ID3D11Buffer_SetPrivateDataInterface(b, &G1, (IUnknown *)obj), S_OK);
        EXPECT_EQ(d3dh_refcount(obj), 2);
        EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, &got), S_OK);
        EXPECT_EQ(sz, sizeof got);
        CHECK(got == (IUnknown *)obj);
        EXPECT_EQ(d3dh_refcount(obj), 3);
        if (got) got->lpVtbl->Release(got);
        /* replacing with plain data releases the interface */
        {
            BYTE x = 1;
            EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G1, 1, &x), S_OK);
            EXPECT_EQ(d3dh_refcount(obj), 1);
        }
    }
    SAFE_RELEASE(b); SAFE_RELEASE(obj);
}

TEST(d3d11_buffers_private_data_interface_released_on_destroy)
{
    ID3D11Buffer *b = mkb(64, VB);
    ID3D11Buffer *obj = mkb(32, VB);
    NEED_DEV();
    CHECK(b && obj);
    if (b && obj) {
        EXPECT_HR(ID3D11Buffer_SetPrivateDataInterface(b, &G2, (IUnknown *)obj), S_OK);
        EXPECT_EQ(d3dh_refcount(obj), 2);
        SAFE_RELEASE(b);
        EXPECT_EQ(d3dh_refcount(obj), 1);
    }
    SAFE_RELEASE(b); SAFE_RELEASE(obj);
}

TEST(d3d11_buffers_private_data_interface_remove)
{
    ID3D11Buffer *b = mkb(64, VB);
    ID3D11Buffer *obj = mkb(32, VB);
    NEED_DEV();
    CHECK(b && obj);
    if (b && obj) {
        EXPECT_HR(ID3D11Buffer_SetPrivateDataInterface(b, &G2, (IUnknown *)obj), S_OK);
        EXPECT_HR(ID3D11Buffer_SetPrivateDataInterface(b, &G2, NULL), S_OK);
        EXPECT_EQ(d3dh_refcount(obj), 1);
    }
    SAFE_RELEASE(b); SAFE_RELEASE(obj);
}

TEST(d3d11_buffers_private_data_is_per_object)
{
    ID3D11Buffer *a = mkb(64, VB), *b = mkb(64, VB);
    BYTE x = 7, out;
    UINT sz = 1;
    NEED_DEV();
    CHECK(a && b);
    if (a && b) {
        EXPECT_HR(ID3D11Buffer_SetPrivateData(a, &G1, 1, &x), S_OK);
        EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G1, &sz, &out), DXGI_ERROR_NOT_FOUND);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

/* ---- eviction priority ---- */
TEST(d3d11_buffers_eviction_default_normal)
{
    ID3D11Buffer *b = mkb(64, VB);
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_EQ(ID3D11Buffer_GetEvictionPriority(b), DXGI_RESOURCE_PRIORITY_NORMAL);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_eviction_default_all_bind_types)
{
    UINT binds[] = { VB, IB, CB, SR, SO };
    int i;
    NEED_DEV();
    for (i = 0; i < 5; i++) {
        ID3D11Buffer *b = mkb(64, binds[i]);
        CHECK(b != NULL);
        if (b) EXPECT_EQ(ID3D11Buffer_GetEvictionPriority(b), DXGI_RESOURCE_PRIORITY_NORMAL);
        SAFE_RELEASE(b);
    }
}

TEST(d3d11_buffers_eviction_set_get_note)
{
    static const UINT vals[] = { DXGI_RESOURCE_PRIORITY_MINIMUM, DXGI_RESOURCE_PRIORITY_LOW, DXGI_RESOURCE_PRIORITY_NORMAL,
                                 DXGI_RESOURCE_PRIORITY_HIGH, DXGI_RESOURCE_PRIORITY_MAXIMUM, 0x12345678, 0 };
    ID3D11Buffer *b = mkb(64, VB);
    size_t i;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        UINT got;
        ID3D11Buffer_SetEvictionPriority(b, vals[i]);
        got = ID3D11Buffer_GetEvictionPriority(b);
        t_note("SetEvictionPriority(0x%08x) -> Get 0x%08x", vals[i], got);
        EXPECT_EQ(ID3D11Buffer_GetEvictionPriority(b), got);   /* stable */
    }
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_eviction_per_object)
{
    ID3D11Buffer *a = mkb(64, VB), *b = mkb(64, VB);
    NEED_DEV();
    CHECK(a && b);
    if (a && b) {
        ID3D11Buffer_SetEvictionPriority(a, DXGI_RESOURCE_PRIORITY_MAXIMUM);
        EXPECT_EQ(ID3D11Buffer_GetEvictionPriority(b), DXGI_RESOURCE_PRIORITY_NORMAL);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

/* ---- pipeline binding roundtrips ---- */
TEST(d3d11_buffers_bind_vertex_buffer_roundtrip)
{
    ID3D11Buffer *b = mkb(256, VB), *g[2] = { 0 };
    UINT stride = 24, off = 8, gs[2] = { 0 }, go[2] = { 0 };
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_IASetVertexBuffers(g_ctx, 3, 1, &b, &stride, &off);
    ID3D11DeviceContext_IAGetVertexBuffers(g_ctx, 3, 1, g, gs, go);
    CHECK(g[0] == b);
    EXPECT_EQ(gs[0], 24);
    EXPECT_EQ(go[0], 8);
    SAFE_RELEASE(g[0]);
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_vertex_buffer_addrefs)
{
    ID3D11Buffer *b = mkb(256, VB), *g[1] = { 0 };
    UINT stride = 16, off = 0, gs[1], go[1];
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_IASetVertexBuffers(g_ctx, 0, 1, &b, &stride, &off);
    EXPECT_EQ(d3dh_refcount(b), 2);
    ID3D11DeviceContext_IAGetVertexBuffers(g_ctx, 0, 1, g, gs, go);
    EXPECT_EQ(d3dh_refcount(b), 3);
    SAFE_RELEASE(g[0]);
    ID3D11DeviceContext_IASetVertexBuffers(g_ctx, 0, 1, (ID3D11Buffer *[]){ NULL }, &stride, &off);
    EXPECT_EQ(d3dh_refcount(b), 1);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_vertex_buffer_kept_alive_by_context)
{
    ID3D11Buffer *b = mkb(256, VB), *g[1] = { 0 };
    UINT stride = 16, off = 0, gs[1], go[1];
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_IASetVertexBuffers(g_ctx, 0, 1, &b, &stride, &off);
    SAFE_RELEASE(b);
    ID3D11DeviceContext_IAGetVertexBuffers(g_ctx, 0, 1, g, gs, go);
    CHECK(g[0] != NULL);
    if (g[0]) { D3D11_BUFFER_DESC d; ID3D11Buffer_GetDesc(g[0], &d); EXPECT_EQ(d.ByteWidth, 256); }
    SAFE_RELEASE(g[0]);
    ID3D11DeviceContext_ClearState(g_ctx);
}

TEST(d3d11_buffers_bind_vertex_buffer_multiple_slots)
{
    ID3D11Buffer *b[3], *g[3] = { 0 };
    UINT st[3] = { 4, 8, 12 }, of[3] = { 0, 16, 32 }, gs[3] = { 0 }, go[3] = { 0 };
    int i;
    NEED_DEV();
    for (i = 0; i < 3; i++) b[i] = mkb(256, VB);
    CHECK(b[0] && b[1] && b[2]);
    if (b[0] && b[1] && b[2]) {
        ID3D11DeviceContext_IASetVertexBuffers(g_ctx, 1, 3, b, st, of);
        ID3D11DeviceContext_IAGetVertexBuffers(g_ctx, 1, 3, g, gs, go);
        for (i = 0; i < 3; i++) { CHECK(g[i] == b[i]); EXPECT_EQ(gs[i], st[i]); EXPECT_EQ(go[i], of[i]); SAFE_RELEASE(g[i]); }
    }
    ID3D11DeviceContext_ClearState(g_ctx);
    for (i = 0; i < 3; i++) SAFE_RELEASE(b[i]);
}

TEST(d3d11_buffers_bind_index_buffer_roundtrip)
{
    ID3D11Buffer *b = mkb(256, IB), *g = NULL;
    DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN;
    UINT off = 0;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_IASetIndexBuffer(g_ctx, b, DXGI_FORMAT_R32_UINT, 12);
    ID3D11DeviceContext_IAGetIndexBuffer(g_ctx, &g, &f, &off);
    CHECK(g == b);
    EXPECT_EQ(f, DXGI_FORMAT_R32_UINT);
    EXPECT_EQ(off, 12);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_IASetIndexBuffer(g_ctx, b, DXGI_FORMAT_R16_UINT, 0);
    ID3D11DeviceContext_IAGetIndexBuffer(g_ctx, &g, &f, &off);
    EXPECT_EQ(f, DXGI_FORMAT_R16_UINT);
    EXPECT_EQ(off, 0);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_index_buffer_unbind)
{
    ID3D11Buffer *b = mkb(256, IB), *g = (ID3D11Buffer *)(ULONG_PTR)1;
    DXGI_FORMAT f = DXGI_FORMAT_R16_UINT;
    UINT off = 99;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_IASetIndexBuffer(g_ctx, b, DXGI_FORMAT_R16_UINT, 4);
    ID3D11DeviceContext_IASetIndexBuffer(g_ctx, NULL, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11DeviceContext_IAGetIndexBuffer(g_ctx, &g, &f, &off);
    CHECK(g == NULL);
    EXPECT_EQ(f, DXGI_FORMAT_UNKNOWN);
    EXPECT_EQ(off, 0);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_constant_buffer_roundtrip)
{
    ID3D11Buffer *b = mkb(256, CB), *g[1] = { 0 };
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_VSSetConstantBuffers(g_ctx, 5, 1, &b);
    ID3D11DeviceContext_VSGetConstantBuffers(g_ctx, 5, 1, g);
    CHECK(g[0] == b);
    SAFE_RELEASE(g[0]);
    ID3D11DeviceContext_PSGetConstantBuffers(g_ctx, 5, 1, g);
    CHECK(g[0] == NULL);   /* stages are independent */
    SAFE_RELEASE(g[0]);
    ID3D11DeviceContext_PSSetConstantBuffers(g_ctx, 5, 1, &b);
    ID3D11DeviceContext_PSGetConstantBuffers(g_ctx, 5, 1, g);
    CHECK(g[0] == b);
    SAFE_RELEASE(g[0]);
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_constant_buffer_all_stages)
{
    ID3D11Buffer *b = mkb(256, CB), *g = NULL;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_GSSetConstantBuffers(g_ctx, 0, 1, &b);
    ID3D11DeviceContext_GSGetConstantBuffers(g_ctx, 0, 1, &g); CHECK(g == b); SAFE_RELEASE(g);
    ID3D11DeviceContext_HSSetConstantBuffers(g_ctx, 13, 1, &b);
    ID3D11DeviceContext_HSGetConstantBuffers(g_ctx, 13, 1, &g); CHECK(g == b); SAFE_RELEASE(g);
    ID3D11DeviceContext_DSSetConstantBuffers(g_ctx, 2, 1, &b);
    ID3D11DeviceContext_DSGetConstantBuffers(g_ctx, 2, 1, &g); CHECK(g == b); SAFE_RELEASE(g);
    ID3D11DeviceContext_CSSetConstantBuffers(g_ctx, 7, 1, &b);
    ID3D11DeviceContext_CSGetConstantBuffers(g_ctx, 7, 1, &g); CHECK(g == b); SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_constant_buffer1_offsets)
{
    ID3D11DeviceContext1 *c1 = NULL;
    ID3D11Buffer *b = mkb(1024, CB), *g = NULL;
    UINT first = 16, num = 32, gf = 0, gn = 0;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    if (FAILED(ID3D11DeviceContext_QueryInterface(g_ctx, &IID_ID3D11DeviceContext1, (void **)&c1)) || !c1) {
        SAFE_RELEASE(b); t_skip("no ID3D11DeviceContext1"); return;
    }
    ID3D11DeviceContext1_VSSetConstantBuffers1(c1, 0, 1, &b, &first, &num);
    ID3D11DeviceContext1_VSGetConstantBuffers1(c1, 0, 1, &g, &gf, &gn);
    CHECK(g == b);
    EXPECT_EQ(gf, 16);
    EXPECT_EQ(gn, 32);
    SAFE_RELEASE(g);
    ID3D11DeviceContext1_Release(c1);
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_constant_buffer1_default_whole)
{
    ID3D11DeviceContext1 *c1 = NULL;
    ID3D11Buffer *b = mkb(1024, CB), *g = NULL;
    UINT gf = 77, gn = 77;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    if (FAILED(ID3D11DeviceContext_QueryInterface(g_ctx, &IID_ID3D11DeviceContext1, (void **)&c1)) || !c1) {
        SAFE_RELEASE(b); t_skip("no ID3D11DeviceContext1"); return;
    }
    ID3D11DeviceContext_VSSetConstantBuffers(g_ctx, 0, 1, &b);
    ID3D11DeviceContext1_VSGetConstantBuffers1(c1, 0, 1, &g, &gf, &gn);
    CHECK(g == b);
    EXPECT_EQ(gf, 0);
    t_note("VSGetConstantBuffers1 after plain Set: first=%u num=%u", gf, gn);
    SAFE_RELEASE(g);
    ID3D11DeviceContext1_Release(c1);
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_so_target_roundtrip)
{
    ID3D11Buffer *b = mkb(256, SO | VB), *g[1] = { 0 };
    UINT off = 16;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_SOSetTargets(g_ctx, 1, &b, &off);
    ID3D11DeviceContext_SOGetTargets(g_ctx, 1, g);
    CHECK(g[0] == b);
    SAFE_RELEASE(g[0]);
    ID3D11DeviceContext_SOSetTargets(g_ctx, 0, NULL, NULL);
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_clearstate_releases)
{
    ID3D11Buffer *b = mkb(256, VB | IB);
    ID3D11Buffer *c = mkb(256, CB);
    UINT stride = 4, off = 0;
    NEED_DEV();
    CHECK(b && c);
    if (b && c) {
        ID3D11DeviceContext_IASetVertexBuffers(g_ctx, 0, 1, &b, &stride, &off);
        ID3D11DeviceContext_IASetIndexBuffer(g_ctx, b, DXGI_FORMAT_R16_UINT, 0);
        ID3D11DeviceContext_VSSetConstantBuffers(g_ctx, 0, 1, &c);
        EXPECT_EQ(d3dh_refcount(b), 3);
        EXPECT_EQ(d3dh_refcount(c), 2);
        ID3D11DeviceContext_ClearState(g_ctx);
        EXPECT_EQ(d3dh_refcount(b), 1);
        EXPECT_EQ(d3dh_refcount(c), 1);
    }
    SAFE_RELEASE(b); SAFE_RELEASE(c);
}

TEST(d3d11_buffers_bind_same_buffer_vb_and_ib)
{
    ID3D11Buffer *b = mkb(256, VB | IB), *g[1] = { 0 }, *gi = NULL;
    UINT stride = 4, off = 0, gs, go;
    DXGI_FORMAT f; UINT io;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    ID3D11DeviceContext_IASetVertexBuffers(g_ctx, 0, 1, &b, &stride, &off);
    ID3D11DeviceContext_IASetIndexBuffer(g_ctx, b, DXGI_FORMAT_R16_UINT, 0);
    ID3D11DeviceContext_IAGetVertexBuffers(g_ctx, 0, 1, g, &gs, &go);
    ID3D11DeviceContext_IAGetIndexBuffer(g_ctx, &gi, &f, &io);
    CHECK(g[0] == b && gi == b);
    SAFE_RELEASE(g[0]); SAFE_RELEASE(gi);
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_bind_srv_of_buffer_roundtrip)
{
    ID3D11Buffer *b = mkb(64, SR);
    ID3D11ShaderResourceView *v = NULL, *g = NULL;
    D3D11_SHADER_RESOURCE_VIEW_DESC d;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_R32_FLOAT; d.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; d.Buffer.NumElements = 16;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(g_dev, RES(b), &d, &v), S_OK);
    if (v) {
        ID3D11DeviceContext_PSSetShaderResources(g_ctx, 4, 1, &v);
        ID3D11DeviceContext_PSGetShaderResources(g_ctx, 4, 1, &g);
        CHECK(g == v);
        SAFE_RELEASE(g);
        ID3D11DeviceContext_CSSetShaderResources(g_ctx, 4, 1, &v);
        ID3D11DeviceContext_CSGetShaderResources(g_ctx, 4, 1, &g);
        CHECK(g == v);
        SAFE_RELEASE(g);
    }
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}
