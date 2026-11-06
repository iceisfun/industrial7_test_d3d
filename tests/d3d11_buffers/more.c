/* Buffer entry points that had no coverage: CopySubresourceRegion1 flags, Discard*, ClearView, CopyStructureCount, shared handles,
 * private-data interfaces, deferred-context buffer operations, per-stage constant-buffer offsets, debug-layer messages. */
#include "bufh.h"
#include <dxgi1_2.h>

#define RES(b) ((ID3D11Resource *)(b))
#define DEF D3D11_USAGE_DEFAULT
#define DYN D3D11_USAGE_DYNAMIC
#define STG D3D11_USAGE_STAGING
#define BRD_CHECK(c) do { if (bufh_is_brd(g_dev)) CHECK(c); else t_note("non-BRD adapter: %s", #c); } while (0)

static ID3D11Buffer *mk(UINT bw, D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, UINT stride, int init, unsigned seed)
{
    D3D11_BUFFER_DESC d = bufh_desc(bw, u, bind, cpu, misc, stride);
    ID3D11Buffer *b = NULL;
    if (!bufh_init()) return NULL;
    if (FAILED(bufh_create(&d, init, seed, &b))) return NULL;
    return b;
}
static ID3D11DeviceContext1 *ctx1(void)
{
    ID3D11DeviceContext1 *c = NULL;
    if (!bufh_init()) return NULL;
    if (FAILED(ID3D11DeviceContext_QueryInterface(g_ctx, &IID_ID3D11DeviceContext1, (void **)&c))) return NULL;
    return c;
}
static D3D11_BOX box(UINT l, UINT r)
{
    D3D11_BOX b;
    b.left = l; b.right = r; b.top = 0; b.bottom = 1; b.front = 0; b.back = 1;
    return b;
}

/* ---- CopySubresourceRegion1 / UpdateSubresource1 on buffers ---------------------------------------------------------------------- */
TEST(d3d11_buffers_more_copy1_flags_zero_copies)
{
    ID3D11DeviceContext1 *c = ctx1();
    ID3D11Buffer *s = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x11), *d = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x22);
    D3D11_BOX bx = box(8, 24);
    unsigned char want[64], sp[64];
    NEED_DEV();
    if (!c) { t_skip("no ID3D11DeviceContext1"); SAFE_RELEASE(s); SAFE_RELEASE(d); return; }
    CHECK(s && d);
    if (s && d) {
        bufh_pattern(want, 64, 0x22); bufh_pattern(sp, 64, 0x11);
        memcpy(want + 32, sp + 8, 16);
        ID3D11DeviceContext1_CopySubresourceRegion1(c, RES(d), 0, 32, 0, 0, RES(s), 0, &bx, 0);
        CHECK(bufh_matches(d, want, 64));
    }
    SAFE_RELEASE(s); SAFE_RELEASE(d); ID3D11DeviceContext1_Release(c);
}
TEST(d3d11_buffers_more_copy1_discard_flag)
{
    ID3D11DeviceContext1 *c = ctx1();
    ID3D11Buffer *s = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x33), *d = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x44);
    D3D11_BOX bx = box(0, 64);
    NEED_DEV();
    if (!c) { t_skip("no ID3D11DeviceContext1"); SAFE_RELEASE(s); SAFE_RELEASE(d); return; }
    CHECK(s && d);
    if (s && d) {
        /* COPY_DISCARD is a hint: replacing the whole destination keeps full semantics */
        ID3D11DeviceContext1_CopySubresourceRegion1(c, RES(d), 0, 0, 0, 0, RES(s), 0, &bx, D3D11_COPY_DISCARD);
        CHECK(bufh_matches_pattern(d, 0x33));
        ID3D11DeviceContext1_CopySubresourceRegion1(c, RES(d), 0, 0, 0, 0, RES(s), 0, &bx, D3D11_COPY_NO_OVERWRITE);
        CHECK(bufh_matches_pattern(d, 0x33));
    }
    SAFE_RELEASE(s); SAFE_RELEASE(d); ID3D11DeviceContext1_Release(c);
}
TEST(d3d11_buffers_more_update1_flags)
{
    ID3D11DeviceContext1 *c = ctx1();
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x55);
    unsigned char a[64];
    NEED_DEV();
    if (!c) { t_skip("no ID3D11DeviceContext1"); SAFE_RELEASE(b); return; }
    CHECK(b != NULL);
    if (b) {
        memset(a, 0xe7, 64);
        ID3D11DeviceContext1_UpdateSubresource1(c, RES(b), 0, NULL, a, 0, 0, D3D11_COPY_DISCARD);
        CHECK(bufh_matches(b, a, 64));
        memset(a, 0x5a, 64);
        ID3D11DeviceContext1_UpdateSubresource1(c, RES(b), 0, NULL, a, 0, 0, 0);
        CHECK(bufh_matches(b, a, 64));
        memset(a, 0x3c, 64);
        ID3D11DeviceContext1_UpdateSubresource1(c, RES(b), 0, NULL, a, 0, 0, D3D11_COPY_NO_OVERWRITE);
        CHECK(bufh_matches(b, a, 64));
    }
    SAFE_RELEASE(b); ID3D11DeviceContext1_Release(c);
}
/* UpdateSubresource1 with a box on a default constant buffer is the supported partial update (ConstantBufferPartialUpdate) */
TEST(d3d11_buffers_more_update1_cb_partial)
{
    ID3D11DeviceContext1 *c = ctx1();
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0, 1, 0x66);
    unsigned char a[16], want[64];
    D3D11_BOX bx = box(16, 32);
    NEED_DEV();
    if (!c) { t_skip("no ID3D11DeviceContext1"); SAFE_RELEASE(b); return; }
    CHECK(b != NULL);
    if (b) {
        memset(a, 0xab, 16);
        bufh_pattern(want, 64, 0x66);
        memcpy(want + 16, a, 16);
        ID3D11DeviceContext1_UpdateSubresource1(c, RES(b), 0, &bx, a, 0, 0, 0);
        /* DIVERGENCE: D3D11_FEATURE_DATA_D3D11_OPTIONS.ConstantBufferPartialUpdate is TRUE on the Basic Render Driver, yet the box update of a
         * DEFAULT constant buffer is dropped entirely (the buffer keeps its previous contents), exactly like the classic UpdateSubresource. */
        {
            D3D11_FEATURE_DATA_D3D11_OPTIONS o;
            memset(&o, 0, sizeof o);
            ID3D11Device_CheckFeatureSupport(g_dev, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof o);
            BRD_CHECK(o.ConstantBufferPartialUpdate == TRUE);
        }
        BRD_CHECK(bufh_matches_pattern(b, 0x66));
    }
    SAFE_RELEASE(b); ID3D11DeviceContext1_Release(c);
}

/* ---- Discard* -------------------------------------------------------------------------------------------------------------------- */
TEST(d3d11_buffers_more_discard_resource)
{
    ID3D11DeviceContext1 *c = ctx1();
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x77), *g = NULL;
    NEED_DEV();
    if (!c) { t_skip("no ID3D11DeviceContext1"); SAFE_RELEASE(b); return; }
    CHECK(b != NULL);
    if (b) {
        ID3D11DeviceContext1_DiscardResource(c, RES(b));
        ID3D11DeviceContext_Flush(g_ctx);
        EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(g_dev), S_OK);
        /* the buffer is still usable and writable after a discard */
        {
            unsigned char a[64];
            memset(a, 0x99, 64);
            ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, NULL, a, 0, 0);
            CHECK(bufh_matches(b, a, 64));
        }
    }
    g = NULL; SAFE_RELEASE(g);
    SAFE_RELEASE(b); ID3D11DeviceContext1_Release(c);
}
TEST(d3d11_buffers_more_discard_view)
{
    ID3D11DeviceContext1 *c = ctx1();
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 0, 0, 0, 0, 0);
    ID3D11UnorderedAccessView *u = NULL;
    ID3D11ShaderResourceView *sv = NULL;
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud;
    D3D11_SHADER_RESOURCE_VIEW_DESC sd;
    NEED_FL11();
    if (!c) { t_skip("no ID3D11DeviceContext1"); SAFE_RELEASE(b); return; }
    CHECK(b != NULL);
    if (b) {
        memset(&ud, 0, sizeof ud); ud.Format = DXGI_FORMAT_R32_UINT; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 16;
        memset(&sd, 0, sizeof sd); sd.Format = DXGI_FORMAT_R32_UINT; sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; sd.Buffer.NumElements = 16;
        EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(g_dev, RES(b), &ud, &u), S_OK);
        EXPECT_HR(ID3D11Device_CreateShaderResourceView(g_dev, RES(b), &sd, &sv), S_OK);
        if (u) ID3D11DeviceContext1_DiscardView(c, (ID3D11View *)u);
        if (sv) ID3D11DeviceContext1_DiscardView(c, (ID3D11View *)sv);
        if (u) ID3D11DeviceContext1_DiscardView1(c, (ID3D11View *)u, NULL, 0);
        ID3D11DeviceContext_Flush(g_ctx);
        EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(g_dev), S_OK);
    }
    SAFE_RELEASE(u); SAFE_RELEASE(sv); SAFE_RELEASE(b); ID3D11DeviceContext1_Release(c);
}
TEST(d3d11_buffers_more_clear_view_uav)
{
    ID3D11DeviceContext1 *c = ctx1();
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 0, 0, 0, 1, 0x12);
    ID3D11UnorderedAccessView *u = NULL;
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud;
    const FLOAT color[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    unsigned char *got;
    UINT sz = 0;
    NEED_FL11();
    if (!c) { t_skip("no ID3D11DeviceContext1"); SAFE_RELEASE(b); return; }
    CHECK(b != NULL);
    if (b) {
        memset(&ud, 0, sizeof ud); ud.Format = DXGI_FORMAT_R32_FLOAT; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 16;
        EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(g_dev, RES(b), &ud, &u), S_OK);
        if (u) {
            ID3D11DeviceContext1_ClearView(c, (ID3D11View *)u, color, NULL, 0);
            got = bufh_read(b, &sz);
            if (got) {   /* the whole view becomes 1.0f */
                UINT i, bad = 0;
                for (i = 0; i < 16; i++) if (((float *)got)[i] != 1.0f) bad++;
                BRD_CHECK(bad == 0);
                free(got);
            }
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(b); ID3D11DeviceContext1_Release(c);
}

/* ---- CopyStructureCount + append/consume counters ------------------------------------------------------------------------------- */
TEST(d3d11_buffers_more_copy_structure_count)
{
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_UNORDERED_ACCESS, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16, 0, 0);
    ID3D11Buffer *dst = mk(16, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x01);
    ID3D11UnorderedAccessView *u = NULL;
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud;
    UINT init = 7;
    unsigned char *got;
    UINT sz = 0;
    NEED_FL11();
    CHECK(b && dst);
    if (b && dst) {
        memset(&ud, 0, sizeof ud); ud.Format = DXGI_FORMAT_UNKNOWN; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        ud.Buffer.NumElements = 4; ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
        EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(g_dev, RES(b), &ud, &u), S_OK);
        if (u) {
            ID3D11DeviceContext_CSSetUnorderedAccessViews(g_ctx, 0, 1, &u, &init);
            ID3D11DeviceContext_CopyStructureCount(g_ctx, dst, 4, u);
            got = bufh_read(dst, &sz);
            if (got) {
                EXPECT_EQ(*(UINT *)(got + 4), 7);                       /* the initial count was written at offset 4 */
                { unsigned char pat[16]; bufh_pattern(pat, 16, 0x01); CHECK(!memcmp(got, pat, 4) && !memcmp(got + 8, pat + 8, 8)); }   /* only dword 1 written */
                free(got);
            }
            ID3D11DeviceContext_CSSetUnorderedAccessViews(g_ctx, 0, 1, (ID3D11UnorderedAccessView *[]) { NULL }, NULL);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(b); SAFE_RELEASE(dst);
}

/* ---- legacy shared handles on buffers --------------------------------------------------------------------------------------------- */
TEST(d3d11_buffers_more_shared_handle)
{
    ID3D11Buffer *b = mk(256, DEF, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED, 0, 1, 0x41), *o = NULL;
    IDXGIResource *r = NULL;
    HANDLE h = NULL;
    HRESULT hr;
    NEED_DEV();
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(ID3D11Buffer_QueryInterface(b, &IID_IDXGIResource, (void **)&r), S_OK);
    if (r) {
        hr = IDXGIResource_GetSharedHandle(r, &h);
        EXPECT_HR(hr, S_OK);
        CHECK(h != NULL);
        if (SUCCEEDED(hr) && h) {
            hr = ID3D11Device_OpenSharedResource(g_dev, h, &IID_ID3D11Buffer, (void **)&o);
            EXPECT_HR(hr, S_OK);
            if (o) {
                D3D11_BUFFER_DESC d;
                ID3D11Buffer_GetDesc(o, &d);
                EXPECT_EQ(d.ByteWidth, 256);
                CHECK(bufh_matches_pattern(o, 0x41));
                SAFE_RELEASE(o);
            }
        }
        IDXGIResource_Release(r);
    }
    SAFE_RELEASE(b);
}

/* ---- private data interface ------------------------------------------------------------------------------------------------------- */
static const GUID G_IFACE = { 0x4a9f0001, 0x1001, 0x4c01, { 0x80, 0x01, 0x10, 0x20, 0x30, 0x40, 0x50, 0x71 } };
TEST(d3d11_buffers_more_private_data_interface_roundtrip)
{
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 0, 0);
    ID3D11Buffer *held = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 0, 0);   /* any COM object serves as the stored interface */
    IUnknown *out = NULL;
    UINT sz = sizeof(IUnknown *);
    NEED_DEV();
    CHECK(b && held);
    if (b && held) {
        ULONG before = d3dh_refcount(held), mid;
        EXPECT_HR(ID3D11Buffer_SetPrivateDataInterface(b, &G_IFACE, (IUnknown *)held), S_OK);
        mid = d3dh_refcount(held);
        EXPECT_EQ(mid, before + 1);                                     /* the store takes a reference */
        EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G_IFACE, &sz, &out), S_OK);
        EXPECT_EQ(sz, sizeof(IUnknown *));
        CHECK(out == (IUnknown *)held);
        EXPECT_EQ(d3dh_refcount(held), before + 2);                     /* GetPrivateData AddRefs the returned interface */
        if (out) IUnknown_Release(out);
        EXPECT_HR(ID3D11Buffer_SetPrivateDataInterface(b, &G_IFACE, NULL), S_OK);   /* NULL removes the entry and its reference */
        EXPECT_EQ(d3dh_refcount(held), before);
        /* the entry itself survives with a NULL interface: it is found, holds one pointer, and that pointer is NULL */
        sz = sizeof(out); out = (IUnknown *)(ULONG_PTR)1;
        EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G_IFACE, &sz, &out), S_OK);
        EXPECT_EQ(sz, sizeof(IUnknown *));
        CHECK(out == NULL);
    }
    SAFE_RELEASE(b); SAFE_RELEASE(held);
}
TEST(d3d11_buffers_more_private_data_interface_released_with_buffer)
{
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 0, 0), *held = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 0, 0);
    NEED_DEV();
    CHECK(b && held);
    if (b && held) {
        ULONG before = d3dh_refcount(held);
        EXPECT_HR(ID3D11Buffer_SetPrivateDataInterface(b, &G_IFACE, (IUnknown *)held), S_OK);
        EXPECT_EQ(d3dh_refcount(held), before + 1);
        SAFE_RELEASE(b);                                                 /* destroying the buffer drops the stored reference */
        EXPECT_EQ(d3dh_refcount(held), before);
    }
    SAFE_RELEASE(b); SAFE_RELEASE(held);
}
TEST(d3d11_buffers_more_private_data_replace_interface_with_data)
{
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 0, 0), *held = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 0, 0);
    DWORD v = 0x1234, got = 0;
    UINT sz = 4;
    NEED_DEV();
    CHECK(b && held);
    if (b && held) {
        ULONG before = d3dh_refcount(held);
        EXPECT_HR(ID3D11Buffer_SetPrivateDataInterface(b, &G_IFACE, (IUnknown *)held), S_OK);
        EXPECT_HR(ID3D11Buffer_SetPrivateData(b, &G_IFACE, 4, &v), S_OK);   /* same GUID: the interface entry is replaced by plain data */
        EXPECT_EQ(d3dh_refcount(held), before);
        EXPECT_HR(ID3D11Buffer_GetPrivateData(b, &G_IFACE, &sz, &got), S_OK);
        EXPECT_EQ(got, 0x1234);
    }
    SAFE_RELEASE(b); SAFE_RELEASE(held);
}

/* ---- deferred contexts ------------------------------------------------------------------------------------------------------------ */
TEST(d3d11_buffers_more_deferred_update_and_copy)
{
    ID3D11DeviceContext *dc = NULL;
    ID3D11CommandList *cl = NULL;
    ID3D11Buffer *b = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x10), *c2 = mk(64, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0, 1, 0x20);
    unsigned char a[64];
    NEED_DEV();
    CHECK(b && c2);
    if (b && c2 && SUCCEEDED(ID3D11Device_CreateDeferredContext(g_dev, 0, &dc)) && dc) {
        memset(a, 0xd3, 64);
        ID3D11DeviceContext_UpdateSubresource(dc, RES(b), 0, NULL, a, 0, 0);
        ID3D11DeviceContext_CopyResource(dc, RES(c2), RES(b));
        CHECK(bufh_matches_pattern(b, 0x10));              /* nothing executed yet */
        EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
        CHECK(bufh_matches_pattern(b, 0x10));
        if (cl) {
            ID3D11DeviceContext_ExecuteCommandList(g_ctx, cl, FALSE);
            CHECK(bufh_matches(b, a, 64));
            CHECK(bufh_matches(c2, a, 64));
        }
    }
    SAFE_RELEASE(cl); SAFE_RELEASE(dc); SAFE_RELEASE(b); SAFE_RELEASE(c2);
}
TEST(d3d11_buffers_more_deferred_map_rules)
{
    ID3D11DeviceContext *dc = NULL;
    ID3D11Buffer *dyn = mk(64, DYN, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0, 0, 0);
    ID3D11Buffer *stg = mk(64, STG, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0, 0, 0);
    D3D11_MAPPED_SUBRESOURCE m;
    NEED_DEV();
    CHECK(dyn && stg);
    if (dyn && stg && SUCCEEDED(ID3D11Device_CreateDeferredContext(g_dev, 0, &dc)) && dc) {
        HRESULT hr;
        hr = ID3D11DeviceContext_Map(dc, RES(dyn), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
        EXPECT_HR(hr, S_OK);                                   /* deferred contexts only support DISCARD / NO_OVERWRITE maps */
        if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(dc, RES(dyn), 0);
        hr = ID3D11DeviceContext_Map(dc, RES(stg), 0, D3D11_MAP_READ, 0, &m);
        EXPECT_HR(hr, E_INVALIDARG);                           /* no CPU reads in a deferred context */
        if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(dc, RES(stg), 0);
        hr = ID3D11DeviceContext_Map(dc, RES(stg), 0, D3D11_MAP_WRITE, 0, &m);
        EXPECT_HR(hr, E_INVALIDARG);
        if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(dc, RES(stg), 0);
    }
    SAFE_RELEASE(dc); SAFE_RELEASE(dyn); SAFE_RELEASE(stg);
}

/* ---- constant-buffer offsets on every stage -------------------------------------------------------------------------------------- */
typedef void (STDMETHODCALLTYPE *setcb1_fn)(ID3D11DeviceContext1 *, UINT, UINT, ID3D11Buffer *const *, const UINT *, const UINT *);
typedef void (STDMETHODCALLTYPE *getcb1_fn)(ID3D11DeviceContext1 *, UINT, UINT, ID3D11Buffer **, UINT *, UINT *);
TEST(d3d11_buffers_more_cb1_all_stages_roundtrip)
{
    ID3D11DeviceContext1 *c = ctx1();
    ID3D11Buffer *b = mk(4096, DEF, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0, 0, 0);
    D3D11_FEATURE_DATA_D3D11_OPTIONS o;
    unsigned st;
    NEED_DEV();
    memset(&o, 0, sizeof o);
    ID3D11Device_CheckFeatureSupport(g_dev, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof o);
    if (!c) { t_skip("no ID3D11DeviceContext1"); SAFE_RELEASE(b); return; }
    if (!o.ConstantBufferOffsetting) { t_skip("ConstantBufferOffsetting unsupported"); SAFE_RELEASE(b); ID3D11DeviceContext1_Release(c); return; }
    CHECK(b != NULL);
    for (st = 0; b && st < 6; st++) {
        UINT first = 16, n = 32, gf = 99, gn = 99;
        ID3D11Buffer *g = NULL;
        switch (st) {
        case 0: ID3D11DeviceContext1_VSSetConstantBuffers1(c, 2, 1, &b, &first, &n); ID3D11DeviceContext1_VSGetConstantBuffers1(c, 2, 1, &g, &gf, &gn); break;
        case 1: ID3D11DeviceContext1_PSSetConstantBuffers1(c, 2, 1, &b, &first, &n); ID3D11DeviceContext1_PSGetConstantBuffers1(c, 2, 1, &g, &gf, &gn); break;
        case 2: ID3D11DeviceContext1_GSSetConstantBuffers1(c, 2, 1, &b, &first, &n); ID3D11DeviceContext1_GSGetConstantBuffers1(c, 2, 1, &g, &gf, &gn); break;
        case 3: ID3D11DeviceContext1_HSSetConstantBuffers1(c, 2, 1, &b, &first, &n); ID3D11DeviceContext1_HSGetConstantBuffers1(c, 2, 1, &g, &gf, &gn); break;
        case 4: ID3D11DeviceContext1_DSSetConstantBuffers1(c, 2, 1, &b, &first, &n); ID3D11DeviceContext1_DSGetConstantBuffers1(c, 2, 1, &g, &gf, &gn); break;
        default: ID3D11DeviceContext1_CSSetConstantBuffers1(c, 2, 1, &b, &first, &n); ID3D11DeviceContext1_CSGetConstantBuffers1(c, 2, 1, &g, &gf, &gn); break;
        }
        CHECKF(g == b, "stage %u: bound buffer not returned", st);
        EXPECT_EQ(gf, 16);
        EXPECT_EQ(gn, 32);
        SAFE_RELEASE(g);
    }
    ID3D11DeviceContext_ClearState(g_ctx);
    SAFE_RELEASE(b); ID3D11DeviceContext1_Release(c);
}

/* ---- debug layer messages for buffers --------------------------------------------------------------------------------------------- */
TEST(d3d11_buffers_more_debug_layer_create_buffer_message)
{
    ID3D11Device *dev;
    ID3D11DeviceContext *ctx;
    ID3D11InfoQueue *iq = NULL;
    D3D11_BUFFER_DESC bad = bufh_desc(0, DEF, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0);
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    if (FAILED(bufh_priv_device(D3D11_CREATE_DEVICE_DEBUG, &dev, &ctx))) { t_skip("debug layer unavailable"); return; }
    if (SUCCEEDED(ID3D11Device_QueryInterface(dev, &IID_ID3D11InfoQueue, (void **)&iq))) {
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(iq), 0);
        EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bad, NULL, &b), E_INVALIDARG);   /* ByteWidth 0 */
        EXPECT_EQ(ID3D11InfoQueue_GetNumStoredMessages(iq), 2);   /* the validation error plus the runtime's "returning E_INVALIDARG" follow-up */
        if (ID3D11InfoQueue_GetNumStoredMessages(iq) >= 1) {
            SIZE_T len = 0;
            D3D11_MESSAGE *m;
            ID3D11InfoQueue_GetMessage(iq, 0, NULL, &len);
            m = malloc(len);
            if (m && SUCCEEDED(ID3D11InfoQueue_GetMessage(iq, 0, m, &len))) {
                EXPECT_EQ(m->Severity, D3D11_MESSAGE_SEVERITY_ERROR);
                EXPECT_EQ(m->Category, D3D11_MESSAGE_CATEGORY_STATE_CREATION);
                EXPECT_EQ(m->ID, D3D11_MESSAGE_ID_CREATEBUFFER_INVALIDDIMENSIONS);
                t_note("message: %.100s", m->pDescription);
            }
            free(m);
        }
        ID3D11InfoQueue_Release(iq);
    }
    SAFE_RELEASE(b);
    ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
}
