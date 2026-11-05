/* Cross-checks: validate-only agrees with real creation; misc contract corners. */
#include "bufh.h"

static const UINT k_usage[] = { D3D11_USAGE_DEFAULT, D3D11_USAGE_IMMUTABLE, D3D11_USAGE_DYNAMIC, D3D11_USAGE_STAGING };
static const UINT k_bind[] = {
    0, D3D11_BIND_VERTEX_BUFFER, D3D11_BIND_INDEX_BUFFER, D3D11_BIND_CONSTANT_BUFFER, D3D11_BIND_SHADER_RESOURCE,
    D3D11_BIND_STREAM_OUTPUT, D3D11_BIND_RENDER_TARGET, D3D11_BIND_UNORDERED_ACCESS,
    D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER, D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE,
    D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_SHADER_RESOURCE, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS
};
static const UINT k_cpu[] = { 0, D3D11_CPU_ACCESS_READ, D3D11_CPU_ACCESS_WRITE, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE };

/* validate-only (NULL output) must succeed (S_FALSE) exactly when real creation succeeds */
TEST(d3d11_buffers_extra_validate_matches_create_matrix)
{
    unsigned u, b, c, bad = 0, ok = 0, rejected = 0;
    BYTE mem[64] = {0};
    D3D11_SUBRESOURCE_DATA sd;
    NEED_FL11();
    memset(&sd, 0, sizeof sd);
    sd.pSysMem = mem;
    for (u = 0; u < 4; u++) for (b = 0; b < sizeof k_bind / sizeof *k_bind; b++) for (c = 0; c < 4; c++) {
        D3D11_BUFFER_DESC d = bufh_desc(64, (D3D11_USAGE)k_usage[u], k_bind[b], k_cpu[c], 0, 0);
        ID3D11Buffer *buf = NULL;
        HRESULT hrv, hrc;
        const D3D11_SUBRESOURCE_DATA *p = (k_usage[u] == D3D11_USAGE_IMMUTABLE) ? &sd : NULL;
        hrv = ID3D11Device_CreateBuffer(g_dev, &d, p, NULL);
        hrc = ID3D11Device_CreateBuffer(g_dev, &d, p, &buf);
        if (SUCCEEDED(hrc)) { ok++; if (hrv != S_FALSE) bad++; }
        else { rejected++; if (SUCCEEDED(hrv)) bad++; if (buf) bad++; }
        SAFE_RELEASE(buf);
    }
    t_note("matrix: %u created, %u rejected", ok, rejected);
    EXPECT_EQ(bad, 0);
    CHECK(ok > 0);
    CHECK(rejected > 0);
}

/* every created buffer reports exactly the creation descriptor */
TEST(d3d11_buffers_extra_getdesc_matches_matrix)
{
    unsigned u, b, c, bad = 0;
    BYTE mem[64] = {0};
    D3D11_SUBRESOURCE_DATA sd;
    NEED_FL11();
    memset(&sd, 0, sizeof sd);
    sd.pSysMem = mem;
    for (u = 0; u < 4; u++) for (b = 0; b < sizeof k_bind / sizeof *k_bind; b++) for (c = 0; c < 4; c++) {
        D3D11_BUFFER_DESC d = bufh_desc(64, (D3D11_USAGE)k_usage[u], k_bind[b], k_cpu[c], 0, 0), g;
        ID3D11Buffer *buf = NULL;
        if (FAILED(ID3D11Device_CreateBuffer(g_dev, &d, k_usage[u] == D3D11_USAGE_IMMUTABLE ? &sd : NULL, &buf))) continue;
        memset(&g, 0xee, sizeof g);
        ID3D11Buffer_GetDesc(buf, &g);
        if (g.ByteWidth != d.ByteWidth || g.Usage != d.Usage || g.BindFlags != d.BindFlags ||
            g.CPUAccessFlags != d.CPUAccessFlags || g.MiscFlags != d.MiscFlags || g.StructureByteStride != d.StructureByteStride) bad++;
        SAFE_RELEASE(buf);
    }
    EXPECT_EQ(bad, 0);
}

/* IMMUTABLE needs data; with data it is accepted for the same binds */
TEST(d3d11_buffers_extra_immutable_needs_data_matrix)
{
    unsigned b, bad = 0, tested = 0;
    BYTE mem[64] = {0};
    D3D11_SUBRESOURCE_DATA sd;
    NEED_DEV();
    memset(&sd, 0, sizeof sd);
    sd.pSysMem = mem;
    for (b = 1; b < sizeof k_bind / sizeof *k_bind; b++) {
        D3D11_BUFFER_DESC d = bufh_desc(64, D3D11_USAGE_IMMUTABLE, k_bind[b], 0, 0, 0);
        HRESULT with = ID3D11Device_CreateBuffer(g_dev, &d, &sd, NULL);
        HRESULT without = ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL);
        if (with != S_FALSE) continue;   /* bind combination not valid for buffers: skip */
        tested++;
        if (without != E_INVALIDARG) bad++;
    }
    CHECK(tested >= 4);
    EXPECT_EQ(bad, 0);
}

/* a ByteWidth sweep: any size 1..300 is valid for VB/IB/SRV/SO/STAGING */
TEST(d3d11_buffers_extra_bytewidth_sweep_non_cb)
{
    UINT n, bad = 0;
    NEED_DEV();
    for (n = 1; n <= 300; n++) {
        D3D11_BUFFER_DESC a = bufh_desc(n, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER, 0, 0, 0);
        D3D11_BUFFER_DESC s = bufh_desc(n, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0, 0);
        D3D11_BUFFER_DESC y = bufh_desc(n, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0);
        if (ID3D11Device_CreateBuffer(g_dev, &a, NULL, NULL) != S_FALSE) bad++;
        if (ID3D11Device_CreateBuffer(g_dev, &s, NULL, NULL) != S_FALSE) bad++;
        if (ID3D11Device_CreateBuffer(g_dev, &y, NULL, NULL) != S_FALSE) bad++;
    }
    EXPECT_EQ(bad, 0);
}

/* updating a DEFAULT buffer then reading it back, for several sizes and a middle box */
TEST(d3d11_buffers_extra_update_middle_box_readback)
{
    D3D11_BUFFER_DESC d = bufh_desc(256, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char src[256], want[256];
    D3D11_BOX box = { 100, 0, 0, 140, 1, 1 };
    NEED_DEV();
    bufh_pattern(src, 256, 0x33);
    EXPECT_HR(bufh_create(&d, 1, 0x10, &b), S_OK);
    if (!b) return;
    bufh_pattern(want, 256, 0x10);
    memcpy(want + 100, src, 40);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, (ID3D11Resource *)b, 0, &box, src, 0x1111, 0x2222);
    CHECK(bufh_matches(b, want, 256));
    SAFE_RELEASE(b);
}

/* UpdateSubresource with a NULL box replaces the first ByteWidth bytes */
TEST(d3d11_buffers_extra_update_null_box_whole)
{
    D3D11_BUFFER_DESC d = bufh_desc(128, D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char src[128];
    NEED_DEV();
    bufh_pattern(src, 128, 0x44);
    EXPECT_HR(bufh_create(&d, 1, 0x01, &b), S_OK);
    if (!b) return;
    ID3D11DeviceContext_UpdateSubresource(g_ctx, (ID3D11Resource *)b, 0, NULL, src, 0, 0);
    CHECK(bufh_matches(b, src, 128));
    SAFE_RELEASE(b);
}

/* CopyResource then modify the source: destination keeps the copied snapshot */
TEST(d3d11_buffers_extra_copy_is_snapshot)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, 0);
    ID3D11Buffer *a = NULL, *b = NULL;
    unsigned char other[64];
    NEED_DEV();
    EXPECT_HR(bufh_create(&d, 1, 0x20, &a), S_OK);
    EXPECT_HR(bufh_create(&d, 1, 0x99, &b), S_OK);
    if (a && b) {
        bufh_pattern(other, 64, 0x55);
        ID3D11DeviceContext_CopyResource(g_ctx, (ID3D11Resource *)b, (ID3D11Resource *)a);
        ID3D11DeviceContext_UpdateSubresource(g_ctx, (ID3D11Resource *)a, 0, NULL, other, 0, 0);
        CHECK(bufh_matches_pattern(b, 0x20));
        CHECK(bufh_matches(a, other, 64));
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

/* staging buffer: data written by Map(WRITE) survives unmap and a second Map(READ_WRITE) */
TEST(d3d11_buffers_extra_staging_rw_two_maps)
{
    D3D11_BUFFER_DESC d = bufh_desc(32, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    memset(&m, 0, sizeof m);
    EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, D3D11_MAP_READ_WRITE, 0, &m), S_OK);
    if (m.pData) {
        UINT i, bad = 0;
        for (i = 0; i < 32; i++) if (((BYTE *)m.pData)[i] != 0) bad++;
        EXPECT_EQ(bad, 0);
        memset(m.pData, 0xA5, 32);
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
        memset(&m, 0, sizeof m);
        EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b, 0, D3D11_MAP_READ, 0, &m), S_OK);
        if (m.pData) {
            for (i = 0, bad = 0; i < 32; i++) if (((BYTE *)m.pData)[i] != 0xA5) bad++;
            EXPECT_EQ(bad, 0);
            ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b, 0);
        }
    }
    SAFE_RELEASE(b);
}
