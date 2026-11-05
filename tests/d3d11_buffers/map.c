/* Map / Unmap behaviour per usage and map type; content roundtrips. */
#include "bufh.h"

#define VB D3D11_BIND_VERTEX_BUFFER
#define IB D3D11_BIND_INDEX_BUFFER
#define CB D3D11_BIND_CONSTANT_BUFFER
#define SR D3D11_BIND_SHADER_RESOURCE
#define DEF D3D11_USAGE_DEFAULT
#define IMM D3D11_USAGE_IMMUTABLE
#define DYN D3D11_USAGE_DYNAMIC
#define STG D3D11_USAGE_STAGING
#define CR D3D11_CPU_ACCESS_READ
#define CW D3D11_CPU_ACCESS_WRITE
#define MR D3D11_MAP_READ
#define MW D3D11_MAP_WRITE
#define MRW D3D11_MAP_READ_WRITE
#define MD D3D11_MAP_WRITE_DISCARD
#define MN D3D11_MAP_WRITE_NO_OVERWRITE

enum { M_OK, M_BAD, M_NOTE };

static void map_case(D3D11_USAGE usage, UINT bind, UINT cpu, UINT bw, D3D11_MAP type, UINT flags, int want)
{
    D3D11_BUFFER_DESC d = bufh_desc(bw, usage, bind, cpu, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr;
    NEED_DEV();
    EXPECT_HR(bufh_create(&d, usage == IMM, 1, &b), S_OK);
    if (!b) return;
    memset(&m, 0xcc, sizeof m);
    hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, type, flags, &m);
    if (want == M_OK) {
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) {
            CHECK(m.pData != NULL);
            CHECK(((ULONG_PTR)m.pData & 3) == 0);
            if (((ULONG_PTR)m.pData & 15) != 0) t_note("mapped pointer %p not 16-byte aligned", m.pData);
            ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
        }
    } else if (want == M_BAD) {
        EXPECT_HR(hr, E_INVALIDARG);
        if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    } else {
        t_note("usage=%d cpu=0x%x type=%d flags=%u: %s", usage, cpu, type, flags, bufh_hrn(hr));
        if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    }
    SAFE_RELEASE(b);
}

#define MC(nm, usage, bind, cpu, type, flags, want) \
    TEST(d3d11_buffers_map_##nm) { REQUIRE(d3dh_D3D11CreateDevice()); map_case(usage, bind, cpu, 64, type, flags, want); }

/* DEFAULT and IMMUTABLE have no CPU access */
MC(def_read, DEF, VB, 0, MR, 0, M_BAD)
MC(def_write, DEF, VB, 0, MW, 0, M_BAD)
MC(def_rw, DEF, VB, 0, MRW, 0, M_BAD)
MC(def_discard, DEF, VB, 0, MD, 0, M_BAD)
MC(def_nooverwrite, DEF, VB, 0, MN, 0, M_BAD)
MC(def_cb_discard, DEF, CB, 0, MD, 0, M_BAD)
MC(def_srv_read, DEF, SR, 0, MR, 0, M_BAD)
MC(def_read_donotwait, DEF, VB, 0, MR, D3D11_MAP_FLAG_DO_NOT_WAIT, M_BAD)
MC(imm_read, IMM, VB, 0, MR, 0, M_BAD)
MC(imm_write, IMM, VB, 0, MW, 0, M_BAD)
MC(imm_rw, IMM, VB, 0, MRW, 0, M_BAD)
MC(imm_discard, IMM, VB, 0, MD, 0, M_BAD)
MC(imm_nooverwrite, IMM, IB, 0, MN, 0, M_BAD)
/* DYNAMIC (write only) */
MC(dyn_vb_discard, DYN, VB, CW, MD, 0, M_OK)
MC(dyn_vb_nooverwrite, DYN, VB, CW, MN, 0, M_OK)
MC(dyn_ib_discard, DYN, IB, CW, MD, 0, M_OK)
MC(dyn_ib_nooverwrite, DYN, IB, CW, MN, 0, M_OK)
MC(dyn_cb_discard, DYN, CB, CW, MD, 0, M_OK)
MC(dyn_cb_nooverwrite, DYN, CB, CW, MN, 0, M_NOTE)
MC(dyn_srv_discard, DYN, SR, CW, MD, 0, M_OK)
MC(dyn_srv_nooverwrite, DYN, SR, CW, MN, 0, M_NOTE)
MC(dyn_write, DYN, VB, CW, MW, 0, M_BAD)
MC(dyn_read, DYN, VB, CW, MR, 0, M_BAD)
MC(dyn_rw, DYN, VB, CW, MRW, 0, M_BAD)
MC(dyn_cb_write, DYN, CB, CW, MW, 0, M_BAD)
MC(dyn_cb_read, DYN, CB, CW, MR, 0, M_BAD)
MC(dyn_discard_donotwait, DYN, VB, CW, MD, D3D11_MAP_FLAG_DO_NOT_WAIT, M_NOTE)
MC(dyn_nooverwrite_donotwait, DYN, VB, CW, MN, D3D11_MAP_FLAG_DO_NOT_WAIT, M_NOTE)
/* STAGING read only */
MC(stg_r_read, STG, 0, CR, MR, 0, M_OK)
MC(stg_r_write, STG, 0, CR, MW, 0, M_BAD)
MC(stg_r_rw, STG, 0, CR, MRW, 0, M_BAD)
MC(stg_r_discard, STG, 0, CR, MD, 0, M_BAD)
MC(stg_r_nooverwrite, STG, 0, CR, MN, 0, M_BAD)
/* STAGING write only */
MC(stg_w_write, STG, 0, CW, MW, 0, M_OK)
MC(stg_w_read, STG, 0, CW, MR, 0, M_BAD)
MC(stg_w_rw, STG, 0, CW, MRW, 0, M_BAD)
MC(stg_w_discard, STG, 0, CW, MD, 0, M_BAD)
MC(stg_w_nooverwrite, STG, 0, CW, MN, 0, M_BAD)
/* STAGING read/write */
MC(stg_rw_read, STG, 0, CR | CW, MR, 0, M_OK)
MC(stg_rw_write, STG, 0, CR | CW, MW, 0, M_OK)
MC(stg_rw_rw, STG, 0, CR | CW, MRW, 0, M_OK)
MC(stg_rw_discard, STG, 0, CR | CW, MD, 0, M_BAD)
MC(stg_rw_nooverwrite, STG, 0, CR | CW, MN, 0, M_BAD)

/* invalid map type values */
static void bad_type(D3D11_MAP t)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR | CW, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, t, 0, &m);
    EXPECT_HR_FAILED(hr);
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
}
TEST(d3d11_buffers_map_type_0) { bad_type((D3D11_MAP)0); }
TEST(d3d11_buffers_map_type_6) { bad_type((D3D11_MAP)6); }
TEST(d3d11_buffers_map_type_100) { bad_type((D3D11_MAP)100); }
TEST(d3d11_buffers_map_type_neg) { bad_type((D3D11_MAP)-1); }

TEST(d3d11_buffers_map_unknown_flags_note)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0x2, &m);
    t_note("Map with MapFlags=0x2: %s", bufh_hrn(hr));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_subresource_1_note)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 1, MR, 0, &m);
    t_note("Map of buffer subresource 1: %s", bufh_hrn(hr));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 1);
    SAFE_RELEASE(b);
}

/* ---- contents ---- */
static void write_via_map(ID3D11Buffer *b, D3D11_MAP t, UINT off, const unsigned char *src, UINT n, HRESULT *hr_out)
{
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, t, 0, &m);
    if (hr_out) *hr_out = hr;
    if (FAILED(hr)) return;
    memcpy((unsigned char *)m.pData + off, src, n);
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
}

static void staging_write_roundtrip(UINT bw)
{
    D3D11_BUFFER_DESC sd = bufh_desc(bw, STG, 0, CW, 0, 0), dd = bufh_desc(bw, DEF, VB, 0, 0, 0);
    ID3D11Buffer *s = NULL, *g = NULL;
    unsigned char *p = malloc(bw);
    HRESULT hr;
    NEED_DEV();
    bufh_pattern(p, bw, 0x5a);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &sd, NULL, &s), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &dd, NULL, &g), S_OK);
    if (s && g) {
        write_via_map(s, MW, 0, p, bw, &hr);
        EXPECT_HR(hr, S_OK);
        ID3D11DeviceContext_CopyResource(g_ctx, (ID3D11Resource *)g, (ID3D11Resource *)s);
        CHECK(bufh_matches(g, p, bw));
    }
    SAFE_RELEASE(s); SAFE_RELEASE(g);
    free(p);
}
#define SWR(nm, bw) TEST(d3d11_buffers_map_staging_write_copy_##nm) { staging_write_roundtrip(bw); }
SWR(1, 1) SWR(3, 3) SWR(16, 16) SWR(17, 17) SWR(64, 64) SWR(255, 255) SWR(4096, 4096) SWR(70000, 70000) SWR(1mb, 1 << 20)

TEST(d3d11_buffers_map_staging_rw_persist)
{
    D3D11_BUFFER_DESC d = bufh_desc(128, STG, 0, CR | CW, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char p[128];
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr;
    NEED_DEV();
    bufh_pattern(p, 128, 1);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    write_via_map(b, MW, 0, p, 128, &hr);
    EXPECT_HR(hr, S_OK);
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0, &m), S_OK);
    if (SUCCEEDED(hr)) {
        CHECK(!memcmp(m.pData, p, 128));
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    }
    /* READ_WRITE sees existing data and a modification persists */
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MRW, 0, &m), S_OK);
    if (m.pData) {
        CHECK(!memcmp(m.pData, p, 128));
        ((unsigned char *)m.pData)[5] ^= 0xff;
        p[5] ^= 0xff;
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    }
    CHECK(bufh_matches(b, p, 128));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_staging_initdata_readable)
{
    D3D11_BUFFER_DESC d = bufh_desc(100, STG, 0, CR, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    unsigned char p[100];
    NEED_DEV();
    bufh_pattern(p, 100, 0x44);
    EXPECT_HR(bufh_create(&d, 1, 0x44, &b), S_OK);
    if (!b) return;
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0, &m), S_OK);
    if (m.pData) { CHECK(!memcmp(m.pData, p, 100)); ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0); }
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_staging_last_byte)
{
    UINT sizes[] = { 1, 2, 13, 4097 };
    int i;
    NEED_DEV();
    for (i = 0; i < 4; i++) {
        D3D11_BUFFER_DESC d = bufh_desc(sizes[i], STG, 0, CR | CW, 0, 0);
        ID3D11Buffer *b = NULL;
        D3D11_MAPPED_SUBRESOURCE m;
        EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
        if (!b) continue;
        if (SUCCEEDED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MW, 0, &m))) {
            ((unsigned char *)m.pData)[sizes[i] - 1] = 0x99;
            ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
        }
        if (SUCCEEDED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0, &m))) {
            EXPECT_EQ(((unsigned char *)m.pData)[sizes[i] - 1], 0x99);
            ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
        }
        SAFE_RELEASE(b);
    }
}

TEST(d3d11_buffers_map_dynamic_discard_roundtrip)
{
    D3D11_BUFFER_DESC d = bufh_desc(256, DYN, VB, CW, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char p[256];
    HRESULT hr;
    NEED_DEV();
    bufh_pattern(p, 256, 0x10);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    write_via_map(b, MD, 0, p, 256, &hr);
    EXPECT_HR(hr, S_OK);
    CHECK(bufh_matches(b, p, 256));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_dynamic_cb_discard_roundtrip)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DYN, CB, CW, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char p[64];
    HRESULT hr;
    NEED_DEV();
    bufh_pattern(p, 64, 0x80);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    write_via_map(b, MD, 0, p, 64, &hr);
    EXPECT_HR(hr, S_OK);
    CHECK(bufh_matches(b, p, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_dynamic_nooverwrite_append)
{
    D3D11_BUFFER_DESC d = bufh_desc(256, DYN, VB, CW, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char p[256];
    HRESULT hr;
    NEED_DEV();
    bufh_pattern(p, 256, 0x33);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    write_via_map(b, MD, 0, p, 128, &hr);
    EXPECT_HR(hr, S_OK);
    write_via_map(b, MN, 128, p + 128, 128, &hr);
    EXPECT_HR(hr, S_OK);
    CHECK(bufh_matches(b, p, 256));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_dynamic_nooverwrite_preserves)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DYN, IB, CW, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char p[64];
    int i;
    HRESULT hr;
    NEED_DEV();
    bufh_pattern(p, 64, 0x71);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    write_via_map(b, MD, 0, p, 64, &hr);
    for (i = 0; i < 16; i++) {
        unsigned char v = (unsigned char)(0xa0 + i);
        write_via_map(b, MN, (UINT)i * 4, &v, 1, &hr);
        EXPECT_HR(hr, S_OK);
        p[i * 4] = v;
    }
    CHECK(bufh_matches(b, p, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_dynamic_discard_loop)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DYN, CB, CW, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char p[64];
    int i, ok = 0;
    HRESULT hr;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    for (i = 0; i < 100; i++) {
        bufh_pattern(p, 64, (unsigned)i);
        write_via_map(b, MD, 0, p, 64, &hr);
        ok += hr == S_OK;
    }
    EXPECT_EQ(ok, 100);
    CHECK(bufh_matches(b, p, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_dynamic_discard_then_draw_copy)
{
    /* the data the CPU wrote must be what a GPU-side copy sees */
    D3D11_BUFFER_DESC d = bufh_desc(128, DYN, SR, CW, 0, 0), dd = bufh_desc(128, DEF, SR, 0, 0, 0);
    ID3D11Buffer *b = NULL, *g = NULL;
    unsigned char p[128];
    HRESULT hr;
    NEED_DEV();
    bufh_pattern(p, 128, 0x12);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &dd, NULL, &g), S_OK);
    if (b && g) {
        write_via_map(b, MD, 0, p, 128, &hr);
        ID3D11DeviceContext_CopyResource(g_ctx, (ID3D11Resource *)g, (ID3D11Resource *)b);
        CHECK(bufh_matches(g, p, 128));
    }
    SAFE_RELEASE(b); SAFE_RELEASE(g);
}

TEST(d3d11_buffers_map_unmap_without_map)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR, 0, 0);
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);   /* silently ignored */
    CHECK(1);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_unmap_twice)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0, &m), S_OK);
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0, &m), S_OK);
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_double_map_note)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m, m2;
    HRESULT hr;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0, &m), S_OK);
    hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0, &m2);
    t_note("second Map without Unmap: %s", bufh_hrn(hr));
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_two_buffers_simultaneously)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR | CW, 0, 0);
    ID3D11Buffer *a = NULL, *b = NULL;
    D3D11_MAPPED_SUBRESOURCE ma, mb;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!a || !b) { SAFE_RELEASE(a); SAFE_RELEASE(b); return; }
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)a, 0, MW, 0, &ma), S_OK);
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MW, 0, &mb), S_OK);
    CHECK(ma.pData && mb.pData && ma.pData != mb.pData);
    memset(ma.pData, 0x11, 64);
    memset(mb.pData, 0x22, 64);
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)a, 0);
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    {
        unsigned char e1[64], e2[64];
        memset(e1, 0x11, 64); memset(e2, 0x22, 64);
        CHECK(bufh_matches(a, e1, 64));
        CHECK(bufh_matches(b, e2, 64));
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_pitches_note)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    memset(&m, 0xcc, sizeof m);
    if (SUCCEEDED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, 0, &m))) {
        t_note("buffer Map RowPitch=%u DepthPitch=%u pData=%p", m.RowPitch, m.DepthPitch, m.pData);
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    }
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_alignment_many)
{
    UINT sizes[] = { 1, 3, 16, 17, 100, 4096, 65536 };
    int i;
    NEED_DEV();
    for (i = 0; i < 7; i++) {
        D3D11_BUFFER_DESC d = bufh_desc(sizes[i], STG, 0, CR | CW, 0, 0);
        ID3D11Buffer *b = NULL;
        D3D11_MAPPED_SUBRESOURCE m;
        EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
        if (!b) continue;
        if (SUCCEEDED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MRW, 0, &m))) {
            CHECK(((ULONG_PTR)m.pData & 3) == 0);
            if ((ULONG_PTR)m.pData & 15) t_note("size %u: pData %p not 16-aligned", sizes[i], m.pData);
            ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
        }
        SAFE_RELEASE(b);
    }
}

/* ---- DO_NOT_WAIT ---- */
TEST(d3d11_buffers_map_donotwait_idle_staging)
{
    D3D11_BUFFER_DESC d = bufh_desc(256, STG, 0, CR, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr;
    NEED_DEV();
    EXPECT_HR(bufh_create(&d, 1, 5, &b), S_OK);
    if (!b) return;
    ID3D11DeviceContext_Flush(g_ctx);
    hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, MR, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
    CHECKF(hr == S_OK || hr == DXGI_ERROR_WAS_STILL_DRAWING, "got %s", bufh_hrn(hr));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_map_donotwait_after_copy)
{
    D3D11_BUFFER_DESC sd = bufh_desc(4 << 20, STG, 0, CR, 0, 0), gd = bufh_desc(4 << 20, DEF, SR, 0, 0, 0);
    ID3D11Buffer *s = NULL, *g = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr;
    int tries;
    NEED_DEV();
    EXPECT_HR(bufh_create(&gd, 1, 0x66, &g), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &sd, NULL, &s), S_OK);
    if (!g || !s) { SAFE_RELEASE(g); SAFE_RELEASE(s); return; }
    ID3D11DeviceContext_CopyResource(g_ctx, (ID3D11Resource *)s, (ID3D11Resource *)g);
    hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)s, 0, MR, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
    CHECKF(hr == S_OK || hr == DXGI_ERROR_WAS_STILL_DRAWING, "got %s", bufh_hrn(hr));
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
        /* the caller polls until the GPU is done; bounded */
        ID3D11DeviceContext_Flush(g_ctx);
        for (tries = 0; tries < 2000 && hr == DXGI_ERROR_WAS_STILL_DRAWING; tries++) {
            Sleep(2);
            hr = ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)s, 0, MR, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
        }
        EXPECT_HR(hr, S_OK);
    }
    if (SUCCEEDED(hr)) {
        unsigned char w[64];
        bufh_pattern(w, 64, 0x66);
        CHECK(!memcmp(m.pData, w, 64));
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)s, 0);
    }
    SAFE_RELEASE(s); SAFE_RELEASE(g);
}

TEST(d3d11_buffers_map_blocking_after_copy)
{
    D3D11_BUFFER_DESC sd = bufh_desc(4 << 20, STG, 0, CR, 0, 0), gd = bufh_desc(4 << 20, DEF, SR, 0, 0, 0);
    ID3D11Buffer *s = NULL, *g = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    NEED_DEV();
    EXPECT_HR(bufh_create(&gd, 1, 0x77, &g), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &sd, NULL, &s), S_OK);
    if (!g || !s) { SAFE_RELEASE(g); SAFE_RELEASE(s); return; }
    ID3D11DeviceContext_CopyResource(g_ctx, (ID3D11Resource *)s, (ID3D11Resource *)g);
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)s, 0, MR, 0, &m), S_OK);
    if (m.pData) {
        unsigned char w[4096];
        bufh_pattern(w, 4096, 0x77);
        CHECK(!memcmp(m.pData, w, 4096));
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)s, 0);
    }
    SAFE_RELEASE(s); SAFE_RELEASE(g);
}

/* ---- deferred contexts ---- */
static ID3D11DeviceContext *mk_deferred(void)
{
    ID3D11DeviceContext *dc = NULL;
    if (FAILED(ID3D11Device_CreateDeferredContext(g_dev, 0, &dc))) return NULL;
    return dc;
}

TEST(d3d11_buffers_map_deferred_dynamic_discard)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DYN, VB, CW, 0, 0);
    ID3D11Buffer *b = NULL;
    ID3D11DeviceContext *dc;
    D3D11_MAPPED_SUBRESOURCE m;
    NEED_DEV();
    dc = mk_deferred();
    if (!dc) { t_skip("no deferred context"); return; }
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (b) {
        EXPECT_HR(ID3D11DeviceContext_Map(dc, (ID3D11Resource *)b, 0, MD, 0, &m), S_OK);
        if (m.pData) { memset(m.pData, 0x5c, 64); ID3D11DeviceContext_Unmap(dc, (ID3D11Resource *)b, 0); }
        {
            ID3D11CommandList *cl = NULL;
            EXPECT_HR(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl), S_OK);
            if (cl) {
                unsigned char w[64];
                ID3D11DeviceContext_ExecuteCommandList(g_ctx, cl, FALSE);
                memset(w, 0x5c, 64);
                CHECK(bufh_matches(b, w, 64));
                ID3D11CommandList_Release(cl);
            }
        }
    }
    SAFE_RELEASE(b);
    ID3D11DeviceContext_Release(dc);
}

static void deferred_bad(D3D11_USAGE u, UINT bind, UINT cpu, D3D11_MAP t)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, u, bind, cpu, 0, 0);
    ID3D11Buffer *b = NULL;
    ID3D11DeviceContext *dc;
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr;
    NEED_DEV();
    dc = mk_deferred();
    if (!dc) { t_skip("no deferred context"); return; }
    EXPECT_HR(bufh_create(&d, u == IMM, 1, &b), S_OK);
    if (b) {
        hr = ID3D11DeviceContext_Map(dc, (ID3D11Resource *)b, 0, t, 0, &m);
        EXPECT_HR_FAILED(hr);
        if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(dc, (ID3D11Resource *)b, 0);
    }
    SAFE_RELEASE(b);
    ID3D11DeviceContext_Release(dc);
}
TEST(d3d11_buffers_map_deferred_staging_read) { deferred_bad(STG, 0, CR, MR); }
TEST(d3d11_buffers_map_deferred_staging_write) { deferred_bad(STG, 0, CW, MW); }
TEST(d3d11_buffers_map_deferred_dynamic_write) { deferred_bad(DYN, VB, CW, MW); }
TEST(d3d11_buffers_map_deferred_default_discard) { deferred_bad(DEF, VB, 0, MD); }
TEST(d3d11_buffers_map_deferred_immutable_discard) { deferred_bad(IMM, VB, 0, MD); }
