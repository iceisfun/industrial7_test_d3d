/* d3d12_objects: texture copies with footprints, clears (RTV/DSV/UAV), discard, resolve; results verified by readback. */
#include "d12o.h"

/* ---- helpers --------------------------------------------------------------------------------- */
static UINT32 px_pattern(UINT x, UINT y, UINT z, UINT seed) { return 0x01000193u * (x + 1) ^ (y * 0x9E3779B1u + z * 0x85EBCA6Bu + seed * 0x27D4EB2Fu); }
static int row_bytes_eq(const d12_sub *s, UINT y, const void *exp, UINT n) { return !memcmp(s->data + (size_t)y * s->pitch, exp, n); }

/* ====== uploads / readbacks ============================================================================ */
TEST(d3d12_objects_tex_upload_roundtrip_rgba8) {
    NEED_DEV();
    ID3D12Resource *t = NULL;
    EXPECT_HR(d12_make_tex2d(dev, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &t), S_OK);
    if (!t) return;
    UINT32 px[64]; for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) px[y * 8 + x] = px_pattern(x, y, 0, 1);
    EXPECT_HR(tex_upload(dev, t, 0, px, 32, 8, 1), S_OK);
    d12_sub s;
    EXPECT_HR(d12_readback_sub(dev, t, 0, D3D12_RESOURCE_STATE_COPY_DEST, &s), S_OK);
    if (s.data) {
        EXPECT_EQ(s.pitch, 256); EXPECT_EQ(s.rows, 8); EXPECT_EQ(s.rowsize, 32);
        int ok = 1; for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) if (d12_sub_px32(&s, x, y, 0) != px[y * 8 + x]) ok = 0;
        CHECK(ok);
        d12_sub_free(&s);
    }
    SAFE_RELEASE(t);
}
TEST(d3d12_objects_tex_upload_roundtrip_formats) {
    NEED_DEV();
    static const struct { DXGI_FORMAT f; UINT bpp; const char *n; } t[] = {
        { DXGI_FORMAT_R8_UNORM, 1, "R8" }, { DXGI_FORMAT_R8G8_UNORM, 2, "RG8" }, { DXGI_FORMAT_R16_UNORM, 2, "R16" }, { DXGI_FORMAT_R16G16_FLOAT, 4, "RG16F" },
        { DXGI_FORMAT_R32_UINT, 4, "R32U" }, { DXGI_FORMAT_R16G16B16A16_UNORM, 8, "RGBA16" }, { DXGI_FORMAT_R32G32_FLOAT, 8, "RG32F" },
        { DXGI_FORMAT_R32G32B32A32_UINT, 16, "RGBA32U" }, { DXGI_FORMAT_R10G10B10A2_UNORM, 4, "R10G10B10A2" }, { DXGI_FORMAT_R11G11B10_FLOAT, 4, "R11G11B10" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        const UINT W = 13, H = 7;
        ID3D12Resource *tx = NULL;
        HRESULT hr = d12_make_tex2d(dev, W, H, t[i].f, 1, 1, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &tx);
        CHECKF(hr == S_OK, "%s create: %s", t[i].n, d3dh_hr_name(hr));
        if (!tx) continue;
        unsigned char *src = malloc(W * H * t[i].bpp);
        for (UINT k = 0; k < W * H * t[i].bpp; k++) src[k] = (unsigned char)((k * 37 + i) ^ (k >> 4));
        EXPECT_HR(tex_upload(dev, tx, 0, src, W * t[i].bpp, H, 1), S_OK);
        d12_sub s;
        if (SUCCEEDED(d12_readback_sub(dev, tx, 0, D3D12_RESOURCE_STATE_COPY_DEST, &s))) {
            CHECKF(s.rowsize == W * t[i].bpp && s.rows == H, "%s: rowsize %u rows %u", t[i].n, s.rowsize, s.rows);
            int ok = 1; for (UINT y = 0; y < H; y++) if (!row_bytes_eq(&s, y, src + y * W * t[i].bpp, W * t[i].bpp)) ok = 0;
            CHECKF(ok, "%s data mismatch", t[i].n);
            d12_sub_free(&s);
        }
        free(src); SAFE_RELEASE(tx);
    }
}
TEST(d3d12_objects_tex_upload_bc_formats) {
    NEED_DEV();
    static const struct { DXGI_FORMAT f; UINT blockbytes; const char *n; } t[] = {
        { DXGI_FORMAT_BC1_UNORM, 8, "BC1" }, { DXGI_FORMAT_BC2_UNORM, 16, "BC2" }, { DXGI_FORMAT_BC3_UNORM, 16, "BC3" },
        { DXGI_FORMAT_BC4_UNORM, 8, "BC4" }, { DXGI_FORMAT_BC5_UNORM, 16, "BC5" }, { DXGI_FORMAT_BC7_UNORM, 16, "BC7" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ID3D12Resource *tx = NULL;
        HRESULT hr = d12_make_tex2d(dev, 12, 8, t[i].f, 1, 1, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &tx);
        CHECKF(hr == S_OK, "%s create: %s", t[i].n, d3dh_hr_name(hr));
        if (!tx) continue;
        const UINT bx = 3, by = 2, rb = bx * t[i].blockbytes;
        unsigned char src[3 * 2 * 16];
        for (UINT k = 0; k < bx * by * t[i].blockbytes; k++) src[k] = (unsigned char)(k * 11 + i * 3 + 1);
        EXPECT_HR(tex_upload(dev, tx, 0, src, rb, by, 1), S_OK);
        d12_sub s;
        if (SUCCEEDED(d12_readback_sub(dev, tx, 0, D3D12_RESOURCE_STATE_COPY_DEST, &s))) {
            CHECKF(s.rows == by && s.rowsize == rb, "%s: rows %u rowsize %u", t[i].n, s.rows, s.rowsize);
            for (UINT y = 0; y < by; y++) CHECKF(row_bytes_eq(&s, y, src + y * rb, rb), "%s row %u", t[i].n, y);
            d12_sub_free(&s);
        }
        SAFE_RELEASE(tx);
    }
}
TEST(d3d12_objects_tex_upload_mips_and_slices) {
    NEED_DEV();
    ID3D12Resource *t = NULL;
    EXPECT_HR(d12_make_tex2d(dev, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 2, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &t), S_OK);
    if (!t) return;
    static const UINT dim[3] = { 16, 8, 4 };
    for (UINT slice = 0; slice < 2; slice++) for (UINT mip = 0; mip < 3; mip++) {
        UINT sub = mip + slice * 3, n = dim[mip];
        UINT32 *px = malloc(n * n * 4);
        for (UINT y = 0; y < n; y++) for (UINT x = 0; x < n; x++) px[y * n + x] = px_pattern(x, y, sub, 7);
        EXPECT_HR(tex_upload(dev, t, sub, px, n * 4, n, 1), S_OK);
        free(px);
    }
    for (UINT sub = 0; sub < 6; sub++) {
        UINT mip = sub % 3, n = dim[mip];
        d12_sub s;
        if (FAILED(d12_readback_sub(dev, t, sub, D3D12_RESOURCE_STATE_COPY_DEST, &s))) continue;
        int ok = 1; for (UINT y = 0; y < n; y++) for (UINT x = 0; x < n; x++) if (d12_sub_px32(&s, x, y, 0) != px_pattern(x, y, sub, 7)) ok = 0;
        CHECKF(ok, "subresource %u (mip %u slice %u)", sub, mip, sub / 3);
        EXPECT_EQ(s.rows, n);
        d12_sub_free(&s);
    }
    SAFE_RELEASE(t);
}
TEST(d3d12_objects_tex_upload_3d) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 3, 0);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    ID3D12Resource *t = NULL;
    EXPECT_HR(d12_create_committed(dev, D3D12_HEAP_TYPE_DEFAULT, &d, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &t), S_OK);
    if (!t) return;
    UINT32 px[4 * 4 * 3]; for (UINT z = 0; z < 3; z++) for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) px[(z * 4 + y) * 4 + x] = px_pattern(x, y, z, 9);
    EXPECT_HR(tex_upload(dev, t, 0, px, 16, 4, 3), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, t, 0, D3D12_RESOURCE_STATE_COPY_DEST, &s))) {
        EXPECT_EQ(s.depth, 3);
        int ok = 1; for (UINT z = 0; z < 3; z++) for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) if (d12_sub_px32(&s, x, y, z) != px[(z * 4 + y) * 4 + x]) ok = 0;
        CHECK(ok);
        d12_sub_free(&s);
    }
    SAFE_RELEASE(t);
}

/* ====== texture -> texture ============================================================================== */
static ID3D12Resource *make_filled_tex(ID3D12Device *dev, UINT w, UINT h, UINT seed) {
    ID3D12Resource *t = NULL;
    if (FAILED(d12_make_tex2d(dev, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &t))) return NULL;
    UINT32 *px = malloc(w * h * 4);
    for (UINT y = 0; y < h; y++) for (UINT x = 0; x < w; x++) px[y * w + x] = px_pattern(x, y, 0, seed);
    HRESULT hr = tex_upload(dev, t, 0, px, w * 4, h, 1);
    free(px);
    if (FAILED(hr)) { ID3D12Resource_Release(t); return NULL; }
    return t;
}
TEST(d3d12_objects_copy_texture_region_full_and_box) {
    NEED_DEV();
    ID3D12Resource *src = make_filled_tex(dev, 8, 8, 3), *dst = make_filled_tex(dev, 8, 8, 99);
    d12ctx c = {0};
    if (!src || !dst || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(src); SAFE_RELEASE(dst); return; }
    D3D12_TEXTURE_COPY_LOCATION dl, sl; memset(&dl, 0, sizeof dl); memset(&sl, 0, sizeof sl);
    dl.pResource = dst; dl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    sl.pResource = src; sl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    d12_transition(c.cl, src, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_BOX box = { 2, 1, 0, 6, 4, 1 };   /* 4x3 region */
    ID3D12GraphicsCommandList_CopyTextureRegion(c.cl, &dl, 3, 4, 0, &sl, &box);   /* to (3,4) */
    d12_transition(c.cl, src, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, dst, 0, D3D12_RESOURCE_STATE_COPY_DEST, &s))) {
        int ok = 1;
        for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
            UINT32 want;
            if (x >= 3 && x < 7 && y >= 4 && y < 7) want = px_pattern(x - 3 + 2, y - 4 + 1, 0, 3); else want = px_pattern(x, y, 0, 99);
            if (d12_sub_px32(&s, x, y, 0) != want) { ok = 0; }
        }
        CHECK(ok);
        d12_sub_free(&s);
    }
    d12_ctx_free(&c); SAFE_RELEASE(src); SAFE_RELEASE(dst);
}
TEST(d3d12_objects_copy_resource_texture) {
    NEED_DEV();
    ID3D12Resource *src = make_filled_tex(dev, 16, 8, 5), *dst = NULL;
    d12_make_tex2d(dev, 16, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &dst);
    d12ctx c = {0};
    if (!src || !dst || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(src); SAFE_RELEASE(dst); return; }
    d12_transition(c.cl, src, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    ID3D12GraphicsCommandList_CopyResource(c.cl, dst, src);
    d12_transition(c.cl, src, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, dst, 0, D3D12_RESOURCE_STATE_COPY_DEST, &s))) {
        int ok = 1; for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 16; x++) if (d12_sub_px32(&s, x, y, 0) != px_pattern(x, y, 0, 5)) ok = 0;
        CHECK(ok);
        d12_sub_free(&s);
    }
    d12_ctx_free(&c); SAFE_RELEASE(src); SAFE_RELEASE(dst);
}
TEST(d3d12_objects_copy_resource_mipped_array) {
    NEED_DEV();
    ID3D12Resource *src = NULL, *dst = NULL;
    d12_make_tex2d(dev, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &src);
    d12_make_tex2d(dev, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &dst);
    d12ctx c = {0};
    if (!src || !dst || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(src); SAFE_RELEASE(dst); return; }
    static const UINT dim[2] = { 8, 4 };
    for (UINT sub = 0; sub < 4; sub++) {
        UINT n = dim[sub % 2]; UINT32 px[64];
        for (UINT y = 0; y < n; y++) for (UINT x = 0; x < n; x++) px[y * n + x] = px_pattern(x, y, sub, 21);
        EXPECT_HR(tex_upload(dev, src, sub, px, n * 4, n, 1), S_OK);
    }
    d12_transition(c.cl, src, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    ID3D12GraphicsCommandList_CopyResource(c.cl, dst, src);
    EXPECT_HR(d12_run(&c), S_OK);
    for (UINT sub = 0; sub < 4; sub++) {
        UINT n = dim[sub % 2]; d12_sub s;
        if (FAILED(d12_readback_sub(dev, dst, sub, D3D12_RESOURCE_STATE_COPY_DEST, &s))) continue;
        int ok = 1; for (UINT y = 0; y < n; y++) for (UINT x = 0; x < n; x++) if (d12_sub_px32(&s, x, y, 0) != px_pattern(x, y, sub, 21)) ok = 0;
        CHECKF(ok, "sub %u", sub);
        d12_sub_free(&s);
    }
    d12_ctx_free(&c); SAFE_RELEASE(src); SAFE_RELEASE(dst);
}
TEST(d3d12_objects_copy_texture_to_buffer_footprint_offset) {
    NEED_DEV();
    ID3D12Resource *src = make_filled_tex(dev, 8, 4, 13), *rb = NULL;
    d12ctx c = {0};
    if (!src || FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, 8192, 0, &rb)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(src); SAFE_RELEASE(rb); return; }
    D3D12_TEXTURE_COPY_LOCATION dl, sl; memset(&dl, 0, sizeof dl); memset(&sl, 0, sizeof sl);
    dl.pResource = rb; dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dl.PlacedFootprint.Offset = 1024;   /* multiple of 512 */
    dl.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM; dl.PlacedFootprint.Footprint.Width = 8; dl.PlacedFootprint.Footprint.Height = 4;
    dl.PlacedFootprint.Footprint.Depth = 1; dl.PlacedFootprint.Footprint.RowPitch = 512;   /* a larger pitch than the minimum 256 is legal */
    sl.pResource = src; sl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    d12_transition(c.cl, src, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    ID3D12GraphicsCommandList_CopyTextureRegion(c.cl, &dl, 0, 0, 0, &sl, NULL);
    EXPECT_HR(d12_run(&c), S_OK);
    unsigned char *p = NULL; D3D12_RANGE rr = { 0, 8192 };
    EXPECT_HR(ID3D12Resource_Map(rb, 0, &rr, (void **)&p), S_OK);
    if (p) {
        int ok = 1;
        for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 8; x++) { UINT32 v; memcpy(&v, p + 1024 + y * 512 + x * 4, 4); if (v != px_pattern(x, y, 0, 13)) ok = 0; }
        CHECK(ok);
        int z = 1; for (UINT i = 0; i < 1024; i++) if (p[i]) z = 0;
        CHECK(z);   /* nothing written before the footprint offset */
        D3D12_RANGE wr = { 0, 0 }; ID3D12Resource_Unmap(rb, 0, &wr);
    }
    d12_ctx_free(&c); SAFE_RELEASE(src); SAFE_RELEASE(rb);
}
TEST(d3d12_objects_copy_buffer_to_texture_subbox) {
    NEED_DEV();
    ID3D12Resource *t = NULL, *up = NULL; d12ctx c = {0};
    if (FAILED(d12_make_tex2d(dev, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &t)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(t); return; }
    UINT32 src[4 * 256 / 4]; memset(src, 0, sizeof src);
    for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) src[y * 64 + x] = px_pattern(x, y, 0, 55);
    EXPECT_HR(d12_make_upload(dev, src, sizeof src, 1024, &up), S_OK);
    if (up) {
        D3D12_TEXTURE_COPY_LOCATION dl, sl; memset(&dl, 0, sizeof dl); memset(&sl, 0, sizeof sl);
        dl.pResource = t; dl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        sl.pResource = up; sl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        sl.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sl.PlacedFootprint.Footprint.Width = 4; sl.PlacedFootprint.Footprint.Height = 4;
        sl.PlacedFootprint.Footprint.Depth = 1; sl.PlacedFootprint.Footprint.RowPitch = 256;
        D3D12_BOX box = { 0, 0, 0, 4, 4, 1 };
        ID3D12GraphicsCommandList_CopyTextureRegion(c.cl, &dl, 2, 3, 0, &sl, &box);
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s;
        if (SUCCEEDED(d12_readback_sub(dev, t, 0, D3D12_RESOURCE_STATE_COPY_DEST, &s))) {
            int ok = 1;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
                if (x >= 2 && x < 6 && y >= 3 && y < 7) { if (d12_sub_px32(&s, x, y, 0) != px_pattern(x - 2, y - 3, 0, 55)) ok = 0; }
                else if (d12_sub_px32(&s, x, y, 0) != 0) t_note("texel outside the copied box is 0x%08x", d12_sub_px32(&s, x, y, 0));
            }
            CHECK(ok);
            d12_sub_free(&s);
        }
    }
    d12_ctx_free(&c); SAFE_RELEASE(t); SAFE_RELEASE(up);
}
TEST(d3d12_objects_copy_same_buffer_disjoint) {
    NEED_DEV();
    d12ctx c = {0}; ID3D12Resource *up = NULL, *b = NULL;
    unsigned char src[256]; for (int i = 0; i < 256; i++) src[i] = (unsigned char)(i + 1);
    if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    EXPECT_HR(d12_make_upload(dev, src, 256, 256, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 512, 0, &b), S_OK);
    if (up && b) {
        d12_transition(c.cl, b, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, b, 0, up, 0, 256);
        d12_transition(c.cl, b, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        EXPECT_HR(d12_run(&c), S_OK);
        EXPECT_HR(d12_rerecord(&c), S_OK);
        /* copy a sub-range of the first buffer into the middle of a second one */
        ID3D12Resource *b2 = NULL;
        EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 512, 0, &b2), S_OK);
        if (b2) {
            ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, b2, 100, b, 10, 50);
            d12_transition(c.cl, b2, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
            EXPECT_HR(d12_run(&c), S_OK);
            unsigned char *rb = NULL; size_t sz = 0;
            EXPECT_HR(d3dh_readback_buffer12(dev, b2, D3D12_RESOURCE_STATE_COPY_SOURCE, (void **)&rb, &sz), S_OK);
            if (rb) {
                CHECK(!memcmp(rb + 100, src + 10, 50));
                int z = 1; for (int i = 0; i < 100; i++) if (rb[i]) z = 0; for (int i = 150; i < 512; i++) if (rb[i]) z = 0;
                CHECK(z);
                free(rb);
            }
            SAFE_RELEASE(b2);
        }
    }
    SAFE_RELEASE(up); SAFE_RELEASE(b); d12_ctx_free(&c);
}

/* ====== ClearRenderTargetView ============================================================================= */
static HRESULT make_rt(ID3D12Device *dev, DXGI_FORMAT f, UINT w, UINT h, UINT mips, UINT arr, ID3D12Resource **out) {
    return d12_make_tex2d(dev, w, h, f, mips, arr, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, out);
}
/* Clear a fresh w x h render target of format f with color, return readback of sub 0. */
static int clear_and_read(ID3D12Device *dev, DXGI_FORMAT f, const float *color, d12_sub *s) {
    ID3D12Resource *rt = NULL; ID3D12DescriptorHeap *h = NULL; d12ctx c = {0}; int ok = 0;
    if (SUCCEEDED(make_rt(dev, f, 4, 4, 1, 1, &rt)) && SUCCEEDED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, 0, &h)) && SUCCEEDED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = d3dh_d12_heap_cpu_start(h);
        ID3D12Device_CreateRenderTargetView(dev, rt, NULL, cpu);
        ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, cpu, color, 0, NULL);
        if (SUCCEEDED(d12_run(&c)) && SUCCEEDED(d12_readback_sub(dev, rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, s))) ok = 1;
        d12_ctx_free(&c);
    } else d12_ctx_free(&c);
    SAFE_RELEASE(h); SAFE_RELEASE(rt);
    return ok;
}
TEST(d3d12_objects_clear_rtv_rgba8_exact) {
    NEED_DEV();
    static const struct { float c[4]; UINT32 want; const char *n; } t[] = {
        { { 1, 0, 0, 1 }, 0xFF0000FF, "red" }, { { 0, 1, 0, 1 }, 0xFF00FF00, "green" }, { { 0, 0, 1, 1 }, 0xFFFF0000, "blue" },
        { { 0, 0, 0, 0 }, 0x00000000, "transparent" }, { { 1, 1, 1, 1 }, 0xFFFFFFFF, "white" }, { { 0, 0, 0, 1 }, 0xFF000000, "black" },
        { { 2, -1, 5, 3 }, 0xFFFF00FF, "out-of-range values are clamped for UNORM" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        d12_sub s;
        if (!clear_and_read(dev, DXGI_FORMAT_R8G8B8A8_UNORM, t[i].c, &s)) { t_skip("setup failed"); return; }
        CHECKF(d12_sub_count32(&s, 0, 0, 4, 4, t[i].want) == 16, "%s: first pixel 0x%08x want 0x%08x", t[i].n, d12_sub_px32(&s, 0, 0, 0), t[i].want);
        d12_sub_free(&s);
    }
}
TEST(d3d12_objects_clear_rtv_rgba8_midvalues) {
    NEED_DEV();
    float c[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    d12_sub s;
    if (!clear_and_read(dev, DXGI_FORMAT_R8G8B8A8_UNORM, c, &s)) { t_skip("setup failed"); return; }
    UINT32 v = d12_sub_px32(&s, 1, 1, 0);
    int r = v & 0xFF, g = (v >> 8) & 0xFF, b = (v >> 16) & 0xFF, a = v >> 24;
    CHECKF(abs(r - 64) <= 1 && abs(g - 128) <= 1 && abs(b - 191) <= 1 && a == 255, "pixel 0x%08x", v);
    EXPECT_EQ(d12_sub_count32(&s, 0, 0, 4, 4, v), 16);
    d12_sub_free(&s);
}
TEST(d3d12_objects_clear_rtv_bgra_swizzle) {
    NEED_DEV();
    float c[4] = { 1, 0, 0, 1 };
    d12_sub s;
    if (!clear_and_read(dev, DXGI_FORMAT_B8G8R8A8_UNORM, c, &s)) { t_skip("BGRA8 render target unsupported"); return; }
    EXPECT_EQ(d12_sub_px32(&s, 2, 2, 0), 0xFFFF0000);   /* memory order B,G,R,A */
    d12_sub_free(&s);
}
TEST(d3d12_objects_clear_rtv_srgb) {
    NEED_DEV();
    float c[4] = { 0.5f, 0.0f, 1.0f, 0.5f };
    d12_sub s;
    if (!clear_and_read(dev, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, c, &s)) { t_skip("sRGB render target unsupported"); return; }
    UINT32 v = d12_sub_px32(&s, 0, 0, 0);
    int r = v & 0xFF, g = (v >> 8) & 0xFF, b = (v >> 16) & 0xFF, a = v >> 24;
    CHECKF(abs(r - 188) <= 1 && g == 0 && b == 255, "rgb %d %d %d (sRGB encoding of 0.5 is 188)", r, g, b);
    CHECKF(abs(a - 128) <= 1, "alpha stays linear: %d", a);
    d12_sub_free(&s);
}
TEST(d3d12_objects_clear_rtv_float_formats) {
    NEED_DEV();
    float c[4] = { 1.5f, -2.25f, 0.5f, 1e10f };
    d12_sub s;
    if (clear_and_read(dev, DXGI_FORMAT_R32G32B32A32_FLOAT, c, &s)) {
        float *f = (float *)(s.data + 3 * s.pitch + 3 * 16);
        CHECKF(f[0] == 1.5f && f[1] == -2.25f && f[2] == 0.5f && f[3] == 1e10f, "got %g %g %g %g", f[0], f[1], f[2], f[3]);
        d12_sub_free(&s);
    } else t_skip("R32G32B32A32_FLOAT RT unsupported");
    float h[4] = { 1.0f, 0.5f, 0.25f, 2.0f };
    if (clear_and_read(dev, DXGI_FORMAT_R16G16B16A16_FLOAT, h, &s)) {
        unsigned short *u = (unsigned short *)(s.data);
        CHECKF(u[0] == 0x3C00 && u[1] == 0x3800 && u[2] == 0x3400 && u[3] == 0x4000, "half floats %04x %04x %04x %04x", u[0], u[1], u[2], u[3]);
        d12_sub_free(&s);
    }
    float r[4] = { 0.5f, 1, 1, 1 };
    if (clear_and_read(dev, DXGI_FORMAT_R32_FLOAT, r, &s)) { float *f = (float *)s.data; EXPECT_EQ(*(UINT32 *)f, 0x3F000000); d12_sub_free(&s); }
}
TEST(d3d12_objects_clear_rtv_narrow_formats) {
    NEED_DEV();
    float c[4] = { 1, 0.5f, 0, 1 };
    d12_sub s;
    if (clear_and_read(dev, DXGI_FORMAT_R8_UNORM, c, &s)) { EXPECT_EQ(s.data[0], 255); EXPECT_EQ(s.data[s.pitch + 3], 255); d12_sub_free(&s); }
    if (clear_and_read(dev, DXGI_FORMAT_R8G8_UNORM, c, &s)) { CHECK(s.data[0] == 255 && abs(s.data[1] - 128) <= 1); d12_sub_free(&s); }
    if (clear_and_read(dev, DXGI_FORMAT_R16_UNORM, c, &s)) { unsigned short v; memcpy(&v, s.data, 2); EXPECT_EQ(v, 65535); d12_sub_free(&s); }
    float c2[4] = { 1, 0, 0, 0 };
    if (clear_and_read(dev, DXGI_FORMAT_R10G10B10A2_UNORM, c2, &s)) { EXPECT_EQ(d12_sub_px32(&s, 0, 0, 0), 0x3FF); d12_sub_free(&s); }
}
TEST(d3d12_objects_clear_rtv_integer_formats) {
    NEED_DEV();
    /* ClearRenderTargetView takes floats; for UINT formats the values are converted */
    float c[4] = { 7, 3, 200, 9 };
    d12_sub s;
    if (clear_and_read(dev, DXGI_FORMAT_R8G8B8A8_UINT, c, &s)) {
        UINT32 got = d12_sub_px32(&s, 1, 1, 0);
        t_note("float clear values 7,3,200,9 on R8G8B8A8_UINT -> 0x%08x (0x09C80307 if converted by value)", got);
        d12_sub_free(&s);
    } else t_skip("R8G8B8A8_UINT RT unsupported");
    if (clear_and_read(dev, DXGI_FORMAT_R32_UINT, c, &s)) { t_note("R32_UINT cleared with 7.0f -> %u", d12_sub_px32(&s, 0, 0, 0)); d12_sub_free(&s); }
}
TEST(d3d12_objects_clear_rtv_rects) {
    NEED_DEV();
    d12draw dr; d12ctx c = {0};
    if (FAILED(d12_draw_init(dev, &dr)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); d12_draw_free(&dr); d12_ctx_free(&c); return; }
    float black[4] = { 0, 0, 0, 1 }, green[4] = { 0, 1, 0, 1 };
    ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, dr.rtv, black, 0, NULL);
    D3D12_RECT rc[2] = { { 1, 1, 4, 3 }, { 5, 5, 8, 8 } };   /* 3x2 and 3x3 */
    ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, dr.rtv, green, 2, rc);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) {
        EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_GREEN), 15);
        EXPECT_EQ(d12_sub_count32(&s, 1, 1, 4, 3, PX_GREEN), 6);
        EXPECT_EQ(d12_sub_count32(&s, 5, 5, 8, 8, PX_GREEN), 9);
        EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_BLACK), 49);
        d12_sub_free(&s);
    }
    d12_ctx_free(&c); d12_draw_free(&dr);
}
TEST(d3d12_objects_clear_rtv_mip_and_slice_views) {
    NEED_DEV();
    ID3D12Resource *rt = NULL; ID3D12DescriptorHeap *h = NULL; d12ctx c = {0};
    if (FAILED(make_rt(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 8, 8, 2, 2, &rt)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, 0, &h)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(rt); SAFE_RELEASE(h); d12_ctx_free(&c); return; }
    UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_RENDER_TARGET_VIEW_DESC rv; memset(&rv, 0, sizeof rv);
    rv.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rv.Texture2DArray.MipSlice = 1; rv.Texture2DArray.FirstArraySlice = 1; rv.Texture2DArray.ArraySize = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE h0 = d12_dh_cpu(h, 0, inc), h1 = d12_dh_cpu(h, 1, inc);
    ID3D12Device_CreateRenderTargetView(dev, rt, &rv, h0);
    rv.Texture2DArray.MipSlice = 0; rv.Texture2DArray.FirstArraySlice = 0;
    ID3D12Device_CreateRenderTargetView(dev, rt, &rv, h1);
    float zero[4] = { 0, 0, 0, 0 }, red[4] = { 1, 0, 0, 1 }, blue[4] = { 0, 0, 1, 1 };
    for (UINT s = 0; s < 4; s++) { (void)zero; }
    ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, h0, red, 0, NULL);    /* mip 1 of slice 1 = subresource 3 */
    ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, h1, blue, 0, NULL);   /* mip 0 of slice 0 = subresource 0 */
    EXPECT_HR(d12_run(&c), S_OK);
    static const UINT32 want[4] = { PX_BLUE, PX_ZERO, PX_ZERO, PX_RED };
    for (UINT sub = 0; sub < 4; sub++) {
        d12_sub s;
        if (FAILED(d12_readback_sub(dev, rt, sub, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) continue;
        UINT n = (sub % 2) ? 4 : 8;
        UINT cnt = d12_sub_count32(&s, 0, 0, n, n, want[sub]);
        if (want[sub] != PX_ZERO) CHECKF(cnt == n * n, "subresource %u: first pixel 0x%08x want 0x%08x", sub, d12_sub_px32(&s, 0, 0, 0), want[sub]);
        else if (cnt != n * n) t_note("subresource %u was not cleared and is not zero-initialised (0x%08x)", sub, d12_sub_px32(&s, 0, 0, 0));
        d12_sub_free(&s);
    }
    d12_ctx_free(&c); SAFE_RELEASE(rt); SAFE_RELEASE(h);
}

/* ====== depth/stencil ===================================================================================== */
static HRESULT make_ds(ID3D12Device *dev, DXGI_FORMAT f, UINT w, UINT h, ID3D12Resource **out) {
    D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = f; cv.DepthStencil.Depth = 1.0f;
    return d12_make_tex2d(dev, w, h, f, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, out);
}
TEST(d3d12_objects_clear_dsv_depth32) {
    NEED_DEV();
    ID3D12Resource *ds = NULL; ID3D12DescriptorHeap *h = NULL; d12ctx c = {0};
    if (FAILED(make_ds(dev, DXGI_FORMAT_D32_FLOAT, 8, 8, &ds)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, 0, &h)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(ds); SAFE_RELEASE(h); d12_ctx_free(&c); return; }
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = d3dh_d12_heap_cpu_start(h);
    ID3D12Device_CreateDepthStencilView(dev, ds, NULL, cpu);
    ID3D12GraphicsCommandList_ClearDepthStencilView(c.cl, cpu, D3D12_CLEAR_FLAG_DEPTH, 0.25f, 0, 0, NULL);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, ds, 0, D3D12_RESOURCE_STATE_DEPTH_WRITE, &s))) {
        int ok = 1; for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) { float f; memcpy(&f, s.data + y * s.pitch + x * 4, 4); if (f != 0.25f) ok = 0; }
        CHECK(ok);
        d12_sub_free(&s);
    }
    /* partial clear with a rectangle */
    EXPECT_HR(d12_rerecord(&c), S_OK);
    D3D12_RECT rc = { 2, 2, 6, 4 };
    ID3D12GraphicsCommandList_ClearDepthStencilView(c.cl, cpu, D3D12_CLEAR_FLAG_DEPTH, 0.75f, 0, 1, &rc);
    EXPECT_HR(d12_run(&c), S_OK);
    if (SUCCEEDED(d12_readback_sub(dev, ds, 0, D3D12_RESOURCE_STATE_DEPTH_WRITE, &s))) {
        int in = 0, out = 0;
        for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) { float f; memcpy(&f, s.data + y * s.pitch + x * 4, 4); if (f == 0.75f) in++; else if (f == 0.25f) out++; }
        EXPECT_EQ(in, 8); EXPECT_EQ(out, 56);
        d12_sub_free(&s);
    }
    d12_ctx_free(&c); SAFE_RELEASE(ds); SAFE_RELEASE(h);
}
TEST(d3d12_objects_clear_dsv_depth16) {
    NEED_DEV();
    ID3D12Resource *ds = NULL; ID3D12DescriptorHeap *h = NULL; d12ctx c = {0};
    if (FAILED(make_ds(dev, DXGI_FORMAT_D16_UNORM, 8, 8, &ds)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, 0, &h)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(ds); SAFE_RELEASE(h); d12_ctx_free(&c); return; }
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = d3dh_d12_heap_cpu_start(h);
    ID3D12Device_CreateDepthStencilView(dev, ds, NULL, cpu);
    ID3D12GraphicsCommandList_ClearDepthStencilView(c.cl, cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, ds, 0, D3D12_RESOURCE_STATE_DEPTH_WRITE, &s))) {
        unsigned short v; memcpy(&v, s.data, 2); EXPECT_EQ(v, 0xFFFF);
        d12_sub_free(&s);
    }
    d12_ctx_free(&c); SAFE_RELEASE(ds); SAFE_RELEASE(h);
}
TEST(d3d12_objects_clear_dsv_stencil_plane) {
    NEED_DEV();
    ID3D12Resource *ds = NULL; ID3D12DescriptorHeap *h = NULL; d12ctx c = {0};
    if (FAILED(make_ds(dev, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 8, 8, &ds)) || FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, 0, &h)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); SAFE_RELEASE(ds); SAFE_RELEASE(h); d12_ctx_free(&c); return; }
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = d3dh_d12_heap_cpu_start(h);
    ID3D12Device_CreateDepthStencilView(dev, ds, NULL, cpu);
    ID3D12GraphicsCommandList_ClearDepthStencilView(c.cl, cpu, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.5f, 0x5A, 0, NULL);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, ds, 0, D3D12_RESOURCE_STATE_DEPTH_WRITE, &s))) {
        float f; memcpy(&f, s.data, 4); EXPECT_EQ(*(UINT32 *)&f, 0x3F000000);
        d12_sub_free(&s);
    }
    if (SUCCEEDED(d12_readback_sub(dev, ds, 1, D3D12_RESOURCE_STATE_DEPTH_WRITE, &s))) {
        EXPECT_EQ(s.data[0], 0x5A); EXPECT_EQ(s.data[s.pitch * 7 + 7], 0x5A);
        d12_sub_free(&s);
    }
    /* clear stencil only: depth keeps its value */
    EXPECT_HR(d12_rerecord(&c), S_OK);
    ID3D12GraphicsCommandList_ClearDepthStencilView(c.cl, cpu, D3D12_CLEAR_FLAG_STENCIL, 0.9f, 0x11, 0, NULL);
    EXPECT_HR(d12_run(&c), S_OK);
    if (SUCCEEDED(d12_readback_sub(dev, ds, 0, D3D12_RESOURCE_STATE_DEPTH_WRITE, &s))) { float f; memcpy(&f, s.data, 4); EXPECT_EQ(*(UINT32 *)&f, 0x3F000000); d12_sub_free(&s); }
    if (SUCCEEDED(d12_readback_sub(dev, ds, 1, D3D12_RESOURCE_STATE_DEPTH_WRITE, &s))) { EXPECT_EQ(s.data[0], 0x11); d12_sub_free(&s); }
    d12_ctx_free(&c); SAFE_RELEASE(ds); SAFE_RELEASE(h);
}

/* ====== UAV clears ======================================================================================== */
typedef struct { ID3D12DescriptorHeap *vis, *cpu; D3D12_GPU_DESCRIPTOR_HANDLE gpu; D3D12_CPU_DESCRIPTOR_HANDLE cpuh; } uavpair;
static HRESULT uav_pair(ID3D12Device *dev, ID3D12Resource *r, const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc, uavpair *u) {
    memset(u, 0, sizeof *u);
    HRESULT hr = d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, 1, &u->vis);
    if (FAILED(hr)) return hr;
    hr = d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, 0, &u->cpu);
    if (FAILED(hr)) return hr;
    ID3D12Device_CreateUnorderedAccessView(dev, r, NULL, desc, d3dh_d12_heap_cpu_start(u->vis));
    u->cpuh = d3dh_d12_heap_cpu_start(u->cpu);
    ID3D12Device_CreateUnorderedAccessView(dev, r, NULL, desc, u->cpuh);
    u->gpu = d3dh_d12_heap_gpu_start(u->vis);
    return S_OK;
}
static void uav_free(uavpair *u) { SAFE_RELEASE(u->vis); SAFE_RELEASE(u->cpu); }
TEST(d3d12_objects_clear_uav_uint_buffer) {
    NEED_DEV();
    ID3D12Resource *b = NULL; uavpair u; d12ctx c = {0};
    if (FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 64 * 4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &b))) { t_skip("no buffer"); return; }
    D3D12_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
    ud.Format = DXGI_FORMAT_R32_UINT; ud.ViewDimension = D3D12_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 64;
    if (FAILED(uav_pair(dev, b, &ud, &u)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); uav_free(&u); SAFE_RELEASE(b); d12_ctx_free(&c); return; }
    d12_transition(c.cl, b, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ID3D12DescriptorHeap *hh = u.vis;
    ID3D12GraphicsCommandList_SetDescriptorHeaps(c.cl, 1, &hh);
    UINT v[4] = { 0xDEADBEEF, 1, 2, 3 };
    ID3D12GraphicsCommandList_ClearUnorderedAccessViewUint(c.cl, u.gpu, u.cpuh, b, v, 0, NULL);
    d12_uav_barrier(c.cl, b);
    UINT v2[4] = { 0x12345678, 0, 0, 0 };
    D3D12_RECT rc = { 8, 0, 16, 1 };   /* elements 8..15 */
    ID3D12GraphicsCommandList_ClearUnorderedAccessViewUint(c.cl, u.gpu, u.cpuh, b, v2, 1, &rc);
    EXPECT_HR(d12_run(&c), S_OK);
    UINT *rb = NULL; size_t sz = 0;
    EXPECT_HR(d3dh_readback_buffer12(dev, b, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
    if (rb) {
        int ok = 1; for (int i = 0; i < 64; i++) if ((i < 8 || i >= 16) && rb[i] != 0xDEADBEEF) ok = 0;
        CHECK(ok);   /* outside the rectangle the first clear survives */
        int in = 0; for (int i = 8; i < 16; i++) if (rb[i] == 0x12345678) in++;
        CHECKF(in == 8, "elements 8..15 cleared by the rectangle: %d of 8", in);
        free(rb);
    }
    d12_ctx_free(&c); uav_free(&u); SAFE_RELEASE(b);
}
TEST(d3d12_objects_clear_uav_raw_buffer) {
    NEED_DEV();
    ID3D12Resource *b = NULL; uavpair u; d12ctx c = {0};
    if (FAILED(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &b))) { t_skip("no buffer"); return; }
    D3D12_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
    ud.Format = DXGI_FORMAT_R32_TYPELESS; ud.ViewDimension = D3D12_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 64; ud.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    if (FAILED(uav_pair(dev, b, &ud, &u)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); uav_free(&u); SAFE_RELEASE(b); d12_ctx_free(&c); return; }
    d12_transition(c.cl, b, 0, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ID3D12DescriptorHeap *hh = u.vis;
    ID3D12GraphicsCommandList_SetDescriptorHeaps(c.cl, 1, &hh);
    UINT v[4] = { 0xCAFEF00D, 0, 0, 0 };
    ID3D12GraphicsCommandList_ClearUnorderedAccessViewUint(c.cl, u.gpu, u.cpuh, b, v, 0, NULL);
    EXPECT_HR(d12_run(&c), S_OK);
    UINT *rb = NULL; size_t sz = 0;
    EXPECT_HR(d3dh_readback_buffer12(dev, b, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, (void **)&rb, &sz), S_OK);
    if (rb) { int ok = 1; for (int i = 0; i < 64; i++) if (rb[i] != 0xCAFEF00D) ok = 0; CHECK(ok); free(rb); }
    d12_ctx_free(&c); uav_free(&u); SAFE_RELEASE(b);
}
TEST(d3d12_objects_clear_uav_texture_uint_and_float) {
    NEED_DEV();
    static const struct { DXGI_FORMAT f; int isfloat; } t[] = { { DXGI_FORMAT_R8G8B8A8_UINT, 0 }, { DXGI_FORMAT_R32_UINT, 0 }, { DXGI_FORMAT_R32_FLOAT, 1 }, { DXGI_FORMAT_R8G8B8A8_UNORM, 1 } };
    for (unsigned i = 0; i < 4; i++) {
        ID3D12Resource *tx = NULL; uavpair u; d12ctx c = {0};
        if (FAILED(d12_make_tex2d(dev, 8, 8, t[i].f, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL, &tx))) continue;
        D3D12_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
        ud.Format = t[i].f; ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        if (FAILED(uav_pair(dev, tx, &ud, &u)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { uav_free(&u); SAFE_RELEASE(tx); d12_ctx_free(&c); continue; }
        ID3D12DescriptorHeap *hh = u.vis;
        ID3D12GraphicsCommandList_SetDescriptorHeaps(c.cl, 1, &hh);
        if (t[i].isfloat) { float fv[4] = { 0.0f, 1.0f, 0.0f, 1.0f }; if (t[i].f == DXGI_FORMAT_R32_FLOAT) fv[0] = 3.5f;
            ID3D12GraphicsCommandList_ClearUnorderedAccessViewFloat(c.cl, u.gpu, u.cpuh, tx, fv, 0, NULL); }
        else { UINT uv[4] = { 1, 2, 3, 4 }; if (t[i].f == DXGI_FORMAT_R32_UINT) uv[0] = 0xABCD1234;
            ID3D12GraphicsCommandList_ClearUnorderedAccessViewUint(c.cl, u.gpu, u.cpuh, tx, uv, 0, NULL); }
        EXPECT_HR(d12_run(&c), S_OK);
        d12_sub s;
        if (SUCCEEDED(d12_readback_sub(dev, tx, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &s))) {
            UINT32 v = d12_sub_px32(&s, 5, 6, 0);
            switch (i) {
            case 0: EXPECT_EQ(v, 0x04030201); break;
            case 1: EXPECT_EQ(v, 0xABCD1234); break;
            case 2: EXPECT_EQ(v, 0x40600000); break;   /* 3.5f */
            case 3: EXPECT_EQ(v, 0xFF00FF00); break;
            }
            d12_sub_free(&s);
        }
        d12_ctx_free(&c); uav_free(&u); SAFE_RELEASE(tx);
    }
}

/* ====== discard ======================================================================================== */
TEST(d3d12_objects_discard_resource) {
    NEED_DEV();
    d12draw dr; d12ctx c = {0};
    if (FAILED(d12_draw_init(dev, &dr)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); d12_draw_free(&dr); d12_ctx_free(&c); return; }
    d12_clear_rt(c.cl, &dr, 1, 0, 0, 1);
    ID3D12GraphicsCommandList_DiscardResource(c.cl, dr.rt, NULL);
    d12_clear_rt(c.cl, &dr, 0, 1, 0, 1);   /* content after the discard is defined again by this clear */
    D3D12_RECT rc = { 0, 0, 4, 4 }; D3D12_DISCARD_REGION reg; memset(&reg, 0, sizeof reg); reg.NumRects = 1; reg.pRects = &rc; reg.FirstSubresource = 0; reg.NumSubresources = 1;
    ID3D12GraphicsCommandList_DiscardResource(c.cl, dr.rt, &reg);
    d12_clear_rt(c.cl, &dr, 0, 0, 1, 1);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) { EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_BLUE), 64); d12_sub_free(&s); }
    d12_ctx_free(&c); d12_draw_free(&dr);
}

/* ====== MSAA resolve ==================================================================================== */
TEST(d3d12_objects_resolve_subresource_msaa4) {
    NEED_DEV();
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ql; memset(&ql, 0, sizeof ql);
    ql.Format = DXGI_FORMAT_R8G8B8A8_UNORM; ql.SampleCount = 4;
    if (FAILED(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ql, sizeof ql)) || ql.NumQualityLevels == 0) { t_skip("4x MSAA RGBA8 unsupported"); return; }
    D3D12_RESOURCE_DESC md = d12_tex2d_desc(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    md.SampleDesc.Count = 4;
    ID3D12Resource *ms = NULL, *dst = NULL; ID3D12DescriptorHeap *h = NULL; d12ctx c = {0};
    if (FAILED(d12_create_committed(dev, D3D12_HEAP_TYPE_DEFAULT, &md, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &ms)) ||
        FAILED(d12_make_tex2d(dev, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0, D3D12_RESOURCE_STATE_RESOLVE_DEST, NULL, &dst)) ||
        FAILED(d12_make_dheap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, 0, &h)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) {
        t_skip("setup failed"); SAFE_RELEASE(ms); SAFE_RELEASE(dst); SAFE_RELEASE(h); d12_ctx_free(&c); return; }
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = d3dh_d12_heap_cpu_start(h);
    ID3D12Device_CreateRenderTargetView(dev, ms, NULL, cpu);
    float col[4] = { 1, 0, 0, 1 };
    ID3D12GraphicsCommandList_ClearRenderTargetView(c.cl, cpu, col, 0, NULL);
    d12_transition(c.cl, ms, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    ID3D12GraphicsCommandList_ResolveSubresource(c.cl, dst, 0, ms, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, dst, 0, D3D12_RESOURCE_STATE_RESOLVE_DEST, &s))) { EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_RED), 64); d12_sub_free(&s); }
    d12_ctx_free(&c); SAFE_RELEASE(ms); SAFE_RELEASE(dst); SAFE_RELEASE(h);
}

/* ====== rendering state interplay =========================================================================== */
TEST(d3d12_objects_draw_fullscreen_triangle) {
    NEED_DEV();
    d12draw dr; d12ctx c = {0};
    if (FAILED(d12_draw_init(dev, &dr)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); d12_draw_free(&dr); d12_ctx_free(&c); return; }
    d12_draw_setup(c.cl, &dr);
    d12_clear_rt(c.cl, &dr, 0, 0, 1, 1);
    ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 0, 0);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) { EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_RED), 64); d12_sub_free(&s); }
    d12_ctx_free(&c); d12_draw_free(&dr);
}
TEST(d3d12_objects_draw_scissor_and_viewport) {
    NEED_DEV();
    d12draw dr; d12ctx c = {0};
    if (FAILED(d12_draw_init(dev, &dr)) || FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("setup failed"); d12_draw_free(&dr); d12_ctx_free(&c); return; }
    d12_draw_setup(c.cl, &dr);
    d12_clear_rt(c.cl, &dr, 0, 0, 0, 1);
    D3D12_RECT sc = { 2, 3, 6, 5 };   /* needs a scissor-enabled rasterizer: scissor is always on in D3D12 */
    ID3D12GraphicsCommandList_RSSetScissorRects(c.cl, 1, &sc);
    ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 0, 0);
    EXPECT_HR(d12_run(&c), S_OK);
    d12_sub s;
    if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) {
        EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_RED), 8);
        EXPECT_EQ(d12_sub_count32(&s, 2, 3, 6, 5, PX_RED), 8);
        d12_sub_free(&s);
    }
    EXPECT_HR(d12_rerecord(&c), S_OK);
    d12_draw_setup(c.cl, &dr);
    d12_clear_rt(c.cl, &dr, 0, 0, 0, 1);
    D3D12_VIEWPORT vp = { 0, 0, 4, 4, 0, 1 };   /* the triangle covers the viewport only, which is the top-left 4x4 quad */
    ID3D12GraphicsCommandList_RSSetViewports(c.cl, 1, &vp);
    ID3D12GraphicsCommandList_DrawInstanced(c.cl, 3, 1, 0, 0);
    EXPECT_HR(d12_run(&c), S_OK);
    if (SUCCEEDED(d12_readback_sub(dev, dr.rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, &s))) {
        EXPECT_EQ(d12_sub_count32(&s, 0, 0, 4, 4, PX_RED), 16);
        EXPECT_EQ(d12_sub_count32(&s, 0, 0, 8, 8, PX_RED), 16);
        d12_sub_free(&s);
    }
    d12_ctx_free(&c); d12_draw_free(&dr);
}
