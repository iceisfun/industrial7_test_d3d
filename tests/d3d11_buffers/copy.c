/* UpdateSubresource, CopyResource, CopySubresourceRegion, ClearUnorderedAccessView on buffers. */
#include "bufh.h"

#define VB D3D11_BIND_VERTEX_BUFFER
#define IB D3D11_BIND_INDEX_BUFFER
#define CB D3D11_BIND_CONSTANT_BUFFER
#define SR D3D11_BIND_SHADER_RESOURCE
#define SO D3D11_BIND_STREAM_OUTPUT
#define UA D3D11_BIND_UNORDERED_ACCESS
#define DEF D3D11_USAGE_DEFAULT
#define IMM D3D11_USAGE_IMMUTABLE
#define DYN D3D11_USAGE_DYNAMIC
#define STG D3D11_USAGE_STAGING
#define CR D3D11_CPU_ACCESS_READ
#define CW D3D11_CPU_ACCESS_WRITE
#define RAW D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS
#define STRUCT D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
#define RES(b) ((ID3D11Resource *)(b))

static ID3D11Buffer *mk(UINT bw, D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, UINT stride, int init, unsigned seed)
{
    D3D11_BUFFER_DESC d = bufh_desc(bw, u, bind, cpu, misc, stride);
    ID3D11Buffer *b = NULL;
    if (!bufh_init()) return NULL;
    if (FAILED(bufh_create(&d, init, seed, &b))) return NULL;
    return b;
}

static D3D11_BOX box(UINT l, UINT r)
{
    D3D11_BOX b;
    b.left = l; b.right = r; b.top = 0; b.bottom = 1; b.front = 0; b.back = 1;
    return b;
}

/* Behaviours the runtime leaves to the driver are pinned only on the oracle adapter (Basic Render Driver); elsewhere they are logged. */
#define BRD_CHECK(c) do { if (bufh_is_brd(g_dev)) CHECK(c); else t_note("non-BRD adapter: %s", #c); } while (0)

/* ================= UpdateSubresource ================= */
static void upd_full(UINT bw, UINT bind, UINT misc, UINT stride, UINT rp, UINT dp)
{
    ID3D11Buffer *b;
    unsigned char *p = malloc(bw);
    NEED_DEV();
    if ((misc || (bind & UA)) && g_fl < D3D_FEATURE_LEVEL_11_0) { free(p); t_skip("needs FL11"); return; }
    b = mk(bw, DEF, bind, 0, misc, stride, 1, 0x01);
    CHECK(b != NULL);
    if (b) {
        bufh_pattern(p, bw, 0xd0);
        ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, NULL, p, rp, dp);
        CHECK(bufh_matches(b, p, bw));
    }
    SAFE_RELEASE(b);
    free(p);
}
#define UF(nm, bw, bind, misc, stride) \
    TEST(d3d11_buffers_update_full_##nm) { upd_full(bw, bind, misc, stride, 0, 0); } \
    TEST(d3d11_buffers_update_full_pitch_ignored_##nm) { upd_full(bw, bind, misc, stride, 0x1000, 0x2000); }
UF(vb16, 16, VB, 0, 0)
UF(vb1, 1, VB, 0, 0)
UF(vb13, 13, VB, 0, 0)
UF(vb4096, 4096, VB, 0, 0)
UF(vb100000, 100000, VB, 0, 0)
UF(ib64, 64, IB, 0, 0)
UF(cb16, 16, CB, 0, 0)
UF(cb256, 256, CB, 0, 0)
UF(srv48, 48, SR, 0, 0)
UF(so64, 64, SO, 0, 0)
UF(uav64, 64, UA, 0, 0)
UF(raw64, 64, SR | UA, RAW, 0)
UF(struct96, 96, SR | UA, STRUCT, 12)

static void upd_box(UINT bw, UINT l, UINT r, UINT bind)
{
    ID3D11Buffer *b;
    unsigned char *base = malloc(bw), *src = malloc(r - l), *want = malloc(bw);
    D3D11_BOX bx = box(l, r);
    NEED_DEV();
    b = mk(bw, DEF, bind, 0, 0, 0, 1, 0x40);
    CHECK(b != NULL);
    if (b) {
        bufh_pattern(base, bw, 0x40);
        memcpy(want, base, bw);
        bufh_pattern(src, r - l, 0xee);
        memcpy(want + l, src, r - l);
        ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, &bx, src, 0, 0);
        CHECK(bufh_matches(b, want, bw));
    }
    SAFE_RELEASE(b);
    free(base); free(src); free(want);
}
#define UB(nm, bw, l, r, bind) TEST(d3d11_buffers_update_box_##nm) { upd_box(bw, l, r, bind); }
UB(mid, 64, 4, 12, VB)
UB(first_byte, 64, 0, 1, VB)
UB(last_byte, 64, 63, 64, VB)
UB(whole, 64, 0, 64, VB)
UB(head, 64, 0, 16, VB)
UB(tail, 64, 48, 64, VB)
UB(odd_unaligned, 64, 3, 10, VB)
UB(odd_unaligned2, 67, 1, 66, VB)
UB(one_mid, 64, 31, 32, IB)
UB(srv_mid, 256, 100, 200, SR)
UB(so_mid, 256, 16, 17, SO)
UB(large, 65536, 1000, 60000, VB)
UB(uav_mid, 128, 8, 24, UA)

TEST(d3d11_buffers_update_box_front_back_top_bottom_unit)
{
    /* the y/z extent of a buffer box must be exactly [0,1) */
    ID3D11Buffer *b;
    unsigned char src[8], want[64];
    D3D11_BOX bx = box(8, 16);
    NEED_DEV();
    b = mk(64, DEF, VB, 0, 0, 0, 1, 0);
    if (!b) { CHECK(b != NULL); return; }
    bufh_pattern(want, 64, 0);
    bufh_pattern(src, 8, 0x99);
    memcpy(want + 8, src, 8);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, &bx, src, 0, 0);
    CHECK(bufh_matches(b, want, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_twice_last_wins)
{
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 0, 0);
    unsigned char a[64], c[64];
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    bufh_pattern(a, 64, 1); bufh_pattern(c, 64, 2);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, NULL, a, 0, 0);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, NULL, c, 0, 0);
    CHECK(bufh_matches(b, c, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_src_copied_immediately)
{
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 0, 0);
    unsigned char a[64], orig[64];
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    bufh_pattern(a, 64, 5); memcpy(orig, a, 64);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, NULL, a, 0, 0);
    memset(a, 0, 64);   /* the runtime has already taken a copy */
    CHECK(bufh_matches(b, orig, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_disjoint_boxes_compose)
{
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 0, 0);
    unsigned char want[64], p1[16], p2[16];
    D3D11_BOX b1 = box(0, 16), b2 = box(32, 48);
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(want, 0, 64);
    bufh_pattern(p1, 16, 0x10); bufh_pattern(p2, 16, 0x20);
    memcpy(want, p1, 16); memcpy(want + 32, p2, 16);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, &b1, p1, 0, 0);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, &b2, p2, 0, 0);
    CHECK(bufh_matches(b, want, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_immutable_unchanged)
{
    ID3D11Buffer *b = mk(64, IMM, VB, 0, 0, 0, 1, 0x30);
    unsigned char a[64];
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(a, 0xff, 64);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, NULL, a, 0, 0);
    CHECK(bufh_matches_pattern(b, 0x30));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_immutable_box_unchanged)
{
    ID3D11Buffer *b = mk(64, IMM, IB, 0, 0, 0, 1, 0x31);
    unsigned char a[16];
    D3D11_BOX bx = box(8, 24);
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(a, 0xff, 16);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, &bx, a, 0, 0);
    CHECK(bufh_matches_pattern(b, 0x31));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_dynamic_note)
{
    ID3D11Buffer *b = mk(64, DYN, VB, CW, 0, 0, 1, 0x32);
    unsigned char a[64];
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(a, 0xff, 64);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, NULL, a, 0, 0);
    /* BRD: UpdateSubresource on a DYNAMIC buffer is silently ignored */
    BRD_CHECK(bufh_matches_pattern(b, 0x32));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_staging_note)
{
    ID3D11Buffer *b = mk(64, STG, 0, CR | CW, 0, 0, 1, 0x33);
    unsigned char a[64];
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(a, 0xff, 64);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, NULL, a, 0, 0);
    BRD_CHECK(bufh_matches_pattern(b, 0x33));   /* ignored */
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_cb_box_note)
{
    /* partial constant-buffer updates through a box are not documented as supported */
    ID3D11Buffer *b = mk(64, DEF, CB, 0, 0, 0, 1, 0x34);
    unsigned char a[16], want[64];
    D3D11_BOX bx = box(16, 32);
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(a, 0xab, 16);
    bufh_pattern(want, 64, 0x34);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, &bx, a, 0, 0);
    memcpy(want + 16, a, 16);
    /* BRD: a box update of a DEFAULT constant buffer is dropped completely (buffer unchanged); only UpdateSubresource1 does partial CB updates */
    BRD_CHECK(bufh_matches_pattern(b, 0x34));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_box_out_of_range_note)
{
    /* BRD: a box that runs past the end of the buffer removes the device, so use a private one */
    ID3D11Device *dev, *sdev = g_dev;
    ID3D11DeviceContext *ctx, *sctx = g_ctx;
    ID3D11Buffer *b;
    unsigned char a[64];
    D3D11_BOX bx = box(48, 80);
    NEED_DEV();
    if (FAILED(bufh_priv_device(0, &dev, &ctx))) { t_skip("no private device"); return; }
    g_dev = dev; g_ctx = ctx;
    b = mk(64, DEF, VB, 0, 0, 0, 1, 0x35);
    memset(a, 0xcd, 64);
    ID3D11DeviceContext_UpdateSubresource(ctx, RES(b), 0, &bx, a, 0, 0);
    ID3D11DeviceContext_Flush(ctx);
    if (bufh_is_brd(dev)) EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(dev), DXGI_ERROR_DEVICE_REMOVED);
    else t_note("non-BRD: removed reason %s", bufh_hrn(ID3D11Device_GetDeviceRemovedReason(dev)));
    SAFE_RELEASE(b);
    ID3D11DeviceContext_ClearState(ctx); ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
    g_dev = sdev; g_ctx = sctx;
}

TEST(d3d11_buffers_update_box_empty_note)
{
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 1, 0x36);
    unsigned char a[64];
    D3D11_BOX bx = box(16, 16);
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(a, 0xcd, 64);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, &bx, a, 0, 0);
    BRD_CHECK(bufh_matches_pattern(b, 0x36));   /* unchanged */
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_box_inverted_note)
{
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 1, 0x37);
    unsigned char a[64];
    D3D11_BOX bx = box(32, 16);
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(a, 0xcd, 64);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 0, &bx, a, 0, 0);
    BRD_CHECK(bufh_matches_pattern(b, 0x37));   /* unchanged, device not removed */
    EXPECT_HR(ID3D11Device_GetDeviceRemovedReason(g_dev), S_OK);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update_subresource_index_note)
{
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 1, 0x38);
    unsigned char a[64];
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    memset(a, 0xcd, 64);
    ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(b), 1, NULL, a, 0, 0);
    BRD_CHECK(bufh_matches_pattern(b, 0x38));   /* unchanged */
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update1_discard)
{
    ID3D11DeviceContext1 *c1 = NULL;
    ID3D11Buffer *b = mk(64, DEF, CB, 0, 0, 0, 1, 0x50);
    unsigned char a[64];
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    if (FAILED(ID3D11DeviceContext_QueryInterface(g_ctx, &IID_ID3D11DeviceContext1, (void **)&c1)) || !c1) {
        SAFE_RELEASE(b); t_skip("no ID3D11DeviceContext1"); return;
    }
    bufh_pattern(a, 64, 0x51);
    ID3D11DeviceContext1_UpdateSubresource1(c1, RES(b), 0, NULL, a, 0, 0, D3D11_COPY_DISCARD);
    CHECK(bufh_matches(b, a, 64));
    bufh_pattern(a, 64, 0x52);
    ID3D11DeviceContext1_UpdateSubresource1(c1, RES(b), 0, NULL, a, 0, 0, 0);
    CHECK(bufh_matches(b, a, 64));
    bufh_pattern(a, 64, 0x53);
    ID3D11DeviceContext1_UpdateSubresource1(c1, RES(b), 0, NULL, a, 0, 0, D3D11_COPY_NO_OVERWRITE);
    CHECK(bufh_matches(b, a, 64));
    ID3D11DeviceContext1_Release(c1);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_update1_box_flags0)
{
    ID3D11DeviceContext1 *c1 = NULL;
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 1, 0x60);
    unsigned char a[16], want[64];
    D3D11_BOX bx = box(20, 36);
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    if (FAILED(ID3D11DeviceContext_QueryInterface(g_ctx, &IID_ID3D11DeviceContext1, (void **)&c1)) || !c1) {
        SAFE_RELEASE(b); t_skip("no ID3D11DeviceContext1"); return;
    }
    bufh_pattern(want, 64, 0x60);
    bufh_pattern(a, 16, 0x61);
    memcpy(want + 20, a, 16);
    ID3D11DeviceContext1_UpdateSubresource1(c1, RES(b), 0, &bx, a, 0, 0, 0);
    CHECK(bufh_matches(b, want, 64));
    ID3D11DeviceContext1_Release(c1);
    SAFE_RELEASE(b);
}

/* ================= CopyResource ================= */
static void copyres(UINT bw, D3D11_USAGE su, UINT sbind, UINT scpu, D3D11_USAGE du, UINT dbind, UINT dcpu)
{
    ID3D11Buffer *s, *d;
    NEED_DEV();
    s = mk(bw, su, sbind, scpu, 0, 0, 1, 0x70);
    d = mk(bw, du, dbind, dcpu, 0, 0, 0, 0);
    CHECK(s && d);
    if (s && d) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(d), RES(s));
        CHECK(bufh_matches_pattern(d, 0x70));
        CHECK(bufh_matches_pattern(s, 0x70));   /* source untouched */
    }
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}
#define CR_(nm, bw, su, sb, sc, du, db, dc) TEST(d3d11_buffers_copyres_##nm) { copyres(bw, su, sb, sc, du, db, dc); }
CR_(def_to_def_64, 64, DEF, VB, 0, DEF, VB, 0)
CR_(def_to_def_1, 1, DEF, VB, 0, DEF, VB, 0)
CR_(def_to_def_13, 13, DEF, VB, 0, DEF, VB, 0)
CR_(def_to_def_1mb, 1 << 20, DEF, VB, 0, DEF, VB, 0)
CR_(def_to_def_bindflags_differ, 256, DEF, VB, 0, DEF, SR | SO, 0)
CR_(def_to_cb, 256, DEF, VB, 0, DEF, CB, 0)
CR_(cb_to_vb, 256, DEF, CB, 0, DEF, VB, 0)
CR_(def_to_stg, 100, DEF, SR, 0, STG, 0, CR)
CR_(stg_to_def, 100, STG, 0, CW | CR, DEF, VB, 0)
CR_(stg_to_stg, 100, STG, 0, CR | CW, STG, 0, CR)
CR_(dyn_to_stg, 64, DYN, VB, CW, STG, 0, CR)
CR_(dyn_to_def, 64, DYN, SR, CW, DEF, SR, 0)
CR_(imm_to_def, 64, IMM, VB, 0, DEF, VB, 0)
CR_(imm_to_stg, 64, IMM, IB, 0, STG, 0, CR)
CR_(imm_cb_to_def_cb, 64, IMM, CB, 0, DEF, CB, 0)
CR_(def_to_def_so, 64, DEF, SR, 0, DEF, SO, 0)
CR_(def_to_def_uav, 64, DEF, SR, 0, DEF, UA, 0)

TEST(d3d11_buffers_copyres_overwrites_dest)
{
    ID3D11Buffer *s = mk(64, DEF, VB, 0, 0, 0, 1, 0x80), *d = mk(64, DEF, VB, 0, 0, 0, 1, 0x90);
    NEED_DEV();
    if (s && d) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(d), RES(s));
        CHECK(bufh_matches_pattern(d, 0x80));
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

TEST(d3d11_buffers_copyres_chain)
{
    ID3D11Buffer *a = mk(64, DEF, VB, 0, 0, 0, 1, 0xa0), *b = mk(64, DEF, VB, 0, 0, 0, 0, 0), *c = mk(64, STG, 0, CR, 0, 0, 0, 0);
    NEED_DEV();
    if (a && b && c) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(b), RES(a));
        ID3D11DeviceContext_CopyResource(g_ctx, RES(c), RES(b));
        CHECK(bufh_matches_pattern(c, 0xa0));
    } else CHECK(0);
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c);
}

TEST(d3d11_buffers_copyres_after_update_ordering)
{
    ID3D11Buffer *a = mk(64, DEF, VB, 0, 0, 0, 0, 0), *b = mk(64, DEF, VB, 0, 0, 0, 0, 0);
    unsigned char p[64], q[64];
    NEED_DEV();
    if (a && b) {
        bufh_pattern(p, 64, 1); bufh_pattern(q, 64, 2);
        ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(a), 0, NULL, p, 0, 0);
        ID3D11DeviceContext_CopyResource(g_ctx, RES(b), RES(a));
        ID3D11DeviceContext_UpdateSubresource(g_ctx, RES(a), 0, NULL, q, 0, 0);   /* must not affect b */
        CHECK(bufh_matches(b, p, 64));
        CHECK(bufh_matches(a, q, 64));
    } else CHECK(0);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_copyres_structured)
{
    ID3D11Buffer *a, *b;
    NEED_FL11();
    a = mk(96, DEF, SR | UA, 0, STRUCT, 12, 1, 0xb0);
    b = mk(96, DEF, SR | UA, 0, STRUCT, 12, 0, 0);
    if (a && b) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(b), RES(a));
        CHECK(bufh_matches_pattern(b, 0xb0));
    } else CHECK(0);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_copyres_raw)
{
    ID3D11Buffer *a, *b;
    NEED_FL11();
    a = mk(64, DEF, SR | UA, 0, RAW, 0, 1, 0xb1);
    b = mk(64, DEF, SR | UA, 0, RAW, 0, 0, 0);
    if (a && b) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(b), RES(a));
        CHECK(bufh_matches_pattern(b, 0xb1));
    } else CHECK(0);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_copyres_size_mismatch_note)
{
    ID3D11Buffer *s = mk(64, DEF, VB, 0, 0, 0, 1, 0xc0), *d = mk(128, DEF, VB, 0, 0, 0, 1, 0xc1);
    NEED_DEV();
    if (s && d) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(d), RES(s));
        BRD_CHECK(bufh_matches_pattern(d, 0xc1));   /* mismatched sizes: copy dropped, destination unchanged */
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

TEST(d3d11_buffers_copyres_size_mismatch_smaller_dst_note)
{
    ID3D11Buffer *s = mk(128, DEF, VB, 0, 0, 0, 1, 0xc2), *d = mk(64, DEF, VB, 0, 0, 0, 1, 0xc3);
    NEED_DEV();
    if (s && d) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(d), RES(s));
        BRD_CHECK(bufh_matches_pattern(d, 0xc3));   /* dropped */
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

TEST(d3d11_buffers_copyres_to_immutable_note)
{
    ID3D11Buffer *s = mk(64, DEF, VB, 0, 0, 0, 1, 0xc4), *d = mk(64, IMM, VB, 0, 0, 0, 1, 0xc5);
    NEED_DEV();
    if (s && d) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(d), RES(s));
        CHECK(bufh_matches_pattern(d, 0xc5));
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

TEST(d3d11_buffers_copyres_to_dynamic_note)
{
    ID3D11Buffer *s = mk(64, DEF, VB, 0, 0, 0, 1, 0xc6), *d = mk(64, DYN, VB, CW, 0, 0, 1, 0xc7);
    NEED_DEV();
    if (s && d) {
        ID3D11DeviceContext_CopyResource(g_ctx, RES(d), RES(s));
        BRD_CHECK(bufh_matches_pattern(d, 0xc6));   /* the copy is applied */
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

/* ================= CopySubresourceRegion ================= */
static void csr(UINT sbw, UINT dbw, UINT dx, const D3D11_BOX *bx, UINT n, UINT soff)
{
    ID3D11Buffer *s = mk(sbw, DEF, VB, 0, 0, 0, 1, 0xd0), *d = mk(dbw, DEF, VB, 0, 0, 0, 1, 0xe0);
    unsigned char *want, *sp;
    NEED_DEV();
    CHECK(s && d);
    if (s && d) {
        want = malloc(dbw); sp = malloc(sbw);
        bufh_pattern(want, dbw, 0xe0);
        bufh_pattern(sp, sbw, 0xd0);
        memcpy(want + dx, sp + soff, n);
        ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(d), 0, dx, 0, 0, RES(s), 0, bx);
        CHECK(bufh_matches(d, want, dbw));
        CHECK(bufh_matches(s, sp, sbw));
        free(want); free(sp);
    }
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}
#define CSR(nm, sbw, dbw, dx, l, r) TEST(d3d11_buffers_copyregion_##nm) { D3D11_BOX b_ = box(l, r); csr(sbw, dbw, dx, &b_, (r) - (l), l); }
CSR(mid_same_offset, 64, 64, 8, 8, 24)
CSR(to_start, 64, 64, 0, 16, 48)
CSR(to_end, 64, 64, 48, 0, 16)
CSR(one_byte, 64, 64, 33, 5, 6)
CSR(one_byte_first, 64, 64, 0, 0, 1)
CSR(one_byte_last, 64, 64, 63, 63, 64)
CSR(odd_offsets, 64, 64, 1, 3, 10)
CSR(odd_offsets_2, 67, 71, 5, 7, 66)
CSR(into_larger, 32, 128, 64, 0, 32)
CSR(from_larger, 128, 32, 0, 100, 128)
CSR(from_larger_2, 4096, 64, 10, 4000, 4054)
CSR(full_box, 64, 64, 0, 0, 64)
CSR(large, 100000, 200000, 12345, 500, 70000)
CSR(dst_end_exact, 64, 64, 60, 0, 4)

TEST(d3d11_buffers_copyregion_null_box_whole_source)
{
    csr(32, 64, 0, NULL, 32, 0);
}
TEST(d3d11_buffers_copyregion_null_box_offset)
{
    csr(32, 64, 16, NULL, 32, 0);
}
TEST(d3d11_buffers_copyregion_null_box_offset_tail)
{
    csr(16, 64, 48, NULL, 16, 0);
}
TEST(d3d11_buffers_copyregion_null_box_1byte)
{
    csr(1, 64, 63, NULL, 1, 0);
}
TEST(d3d11_buffers_copyregion_null_box_equal_size)
{
    csr(64, 64, 0, NULL, 64, 0);
}

static void csr_usage(D3D11_USAGE su, UINT sb, UINT sc, D3D11_USAGE du, UINT db, UINT dc)
{
    ID3D11Buffer *s = mk(64, su, sb, sc, 0, 0, 1, 0xd1), *d = mk(64, du, db, dc, 0, 0, 1, 0xe1);
    unsigned char want[64], sp[64];
    D3D11_BOX bx = box(16, 48);
    NEED_DEV();
    CHECK(s && d);
    if (s && d) {
        bufh_pattern(want, 64, 0xe1); bufh_pattern(sp, 64, 0xd1);
        memcpy(want + 8, sp + 16, 32);
        ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(d), 0, 8, 0, 0, RES(s), 0, &bx);
        CHECK(bufh_matches(d, want, 64));
    }
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}
TEST(d3d11_buffers_copyregion_def_to_stg) { csr_usage(DEF, VB, 0, STG, 0, CR); }
TEST(d3d11_buffers_copyregion_stg_to_def) { csr_usage(STG, 0, CR | CW, DEF, VB, 0); }
TEST(d3d11_buffers_copyregion_stg_to_stg) { csr_usage(STG, 0, CR, STG, 0, CR | CW); }
TEST(d3d11_buffers_copyregion_imm_to_def) { csr_usage(IMM, VB, 0, DEF, IB, 0); }
TEST(d3d11_buffers_copyregion_imm_to_stg) { csr_usage(IMM, VB, 0, STG, 0, CR); }
TEST(d3d11_buffers_copyregion_dyn_to_def) { csr_usage(DYN, VB, CW, DEF, VB, 0); }
TEST(d3d11_buffers_copyregion_dyn_to_stg) { csr_usage(DYN, SR, CW, STG, 0, CR); }
TEST(d3d11_buffers_copyregion_def_to_cb) { csr_usage(DEF, VB, 0, DEF, CB, 0); }

TEST(d3d11_buffers_copyregion_same_buffer_disjoint)
{
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 1, 0xf0);
    unsigned char want[64];
    D3D11_BOX bx = box(0, 16);
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    bufh_pattern(want, 64, 0xf0);
    memcpy(want + 32, want, 16);
    ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(b), 0, 32, 0, 0, RES(b), 0, &bx);
    CHECK(bufh_matches(b, want, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_copyregion_same_buffer_overlap_note)
{
    ID3D11Buffer *b = mk(64, DEF, VB, 0, 0, 0, 1, 0xf1);
    unsigned char orig[64], exp[64];
    D3D11_BOX bx = box(0, 32);
    UINT sz = 0;
    unsigned char *got;
    NEED_DEV();
    if (!b) { CHECK(b != NULL); return; }
    bufh_pattern(orig, 64, 0xf1);
    memcpy(exp, orig, 64);
    memmove(exp + 16, orig, 32);
    ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(b), 0, 16, 0, 0, RES(b), 0, &bx);
    got = bufh_read(b, &sz);
    BRD_CHECK(got && !memcmp(got, exp, 64));   /* memmove semantics (documented as undefined) */
    free(got);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_copyregion_does_not_touch_outside)
{
    ID3D11Buffer *s = mk(256, DEF, VB, 0, 0, 0, 1, 0x12), *d = mk(256, DEF, VB, 0, 0, 0, 1, 0x34);
    unsigned char want[256], sp[256];
    D3D11_BOX bx = box(100, 101);
    NEED_DEV();
    if (s && d) {
        bufh_pattern(want, 256, 0x34); bufh_pattern(sp, 256, 0x12);
        want[200] = sp[100];
        ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(d), 0, 200, 0, 0, RES(s), 0, &bx);
        CHECK(bufh_matches(d, want, 256));
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

TEST(d3d11_buffers_copyregion_sequence_tiles)
{
    /* assemble a buffer from 8-byte pieces of another one in reverse order */
    ID3D11Buffer *s = mk(64, DEF, VB, 0, 0, 0, 1, 0x5e), *d = mk(64, DEF, VB, 0, 0, 0, 0, 0);
    unsigned char want[64], sp[64];
    int i;
    NEED_DEV();
    if (s && d) {
        bufh_pattern(sp, 64, 0x5e);
        for (i = 0; i < 8; i++) {
            D3D11_BOX bx = box(i * 8, i * 8 + 8);
            ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(d), 0, (7 - i) * 8, 0, 0, RES(s), 0, &bx);
            memcpy(want + (7 - i) * 8, sp + i * 8, 8);
        }
        CHECK(bufh_matches(d, want, 64));
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

/* Invalid CopySubresourceRegion arguments on a buffer. The retail runtime does not validate them; what happens is up to the driver.
 * On the oracle (Basic Render Driver / WARP) an out-of-range or malformed box removes the device (DXGI_ERROR_DEVICE_REMOVED) while an
 * empty box or a nonzero subresource index is ignored. Each case therefore runs on its own private device so it cannot poison the others. */
enum { CSR_IGNORED, CSR_REMOVED };
static void csr_bad(const char *what, UINT dx, UINT dy, UINT dz, UINT dsub, UINT ssub, const D3D11_BOX *bx, int brd_expect)
{
    ID3D11Device *dev, *sdev = g_dev;
    ID3D11DeviceContext *ctx, *sctx = g_ctx;
    ID3D11Buffer *s = NULL, *d = NULL;
    D3D11_BUFFER_DESC bd = bufh_desc(64, DEF, VB, 0, 0, 0);
    HRESULT rr;
    int brd, pass;
    NEED_DEV();
    for (pass = 0; pass < 3; pass++) {
        if (FAILED(bufh_priv_device(0, &dev, &ctx))) { t_skip("cannot create a private device"); return; }
        g_dev = dev; g_ctx = ctx;
        brd = bufh_is_brd(dev);
        CHECK(SUCCEEDED(bufh_create(&bd, 1, 0x01, &s)) && SUCCEEDED(bufh_create(&bd, 1, 0x02, &d)));
        ID3D11DeviceContext_CopySubresourceRegion(ctx, RES(d), dsub, dx, dy, dz, RES(s), ssub, bx);
        ID3D11DeviceContext_Flush(ctx);
        rr = ID3D11Device_GetDeviceRemovedReason(dev);
        if (brd) {
            if (brd_expect == CSR_REMOVED) {
                EXPECT_HR(rr, DXGI_ERROR_DEVICE_REMOVED);
                {   /* a removed device refuses every further creation with the same code */
                    ID3D11Buffer *e = NULL;
                    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &bd, NULL, &e), DXGI_ERROR_DEVICE_REMOVED);
                    SAFE_RELEASE(e);
                }
            } else {
                EXPECT_HR(rr, S_OK);
                CHECK(bufh_matches_pattern(d, 0x02));
                CHECK(bufh_matches_pattern(s, 0x01));
            }
        } else {
            t_note("%s: non-BRD adapter, removed reason %s (driver-defined)", what, bufh_hrn(rr));
        }
        SAFE_RELEASE(s); SAFE_RELEASE(d);
        ID3D11DeviceContext_ClearState(ctx);
        ID3D11DeviceContext_Release(ctx);
        ID3D11Device_Release(dev);
        g_dev = sdev; g_ctx = sctx;
    }
}
TEST(d3d11_buffers_copyregion_src_box_exceeds_note) { D3D11_BOX b = box(32, 96); csr_bad("source box beyond buffer", 0, 0, 0, 0, 0, &b, CSR_REMOVED); }
TEST(d3d11_buffers_copyregion_dst_overflow_note) { D3D11_BOX b = box(0, 32); csr_bad("destination range beyond buffer", 48, 0, 0, 0, 0, &b, CSR_REMOVED); }
TEST(d3d11_buffers_copyregion_dstx_beyond_note) { D3D11_BOX b = box(0, 4); csr_bad("DstX beyond buffer", 64, 0, 0, 0, 0, &b, CSR_REMOVED); }
TEST(d3d11_buffers_copyregion_dsty_nonzero_note) { D3D11_BOX b = box(0, 16); csr_bad("DstY=1", 0, 1, 0, 0, 0, &b, CSR_REMOVED); }
TEST(d3d11_buffers_copyregion_dstz_nonzero_note) { D3D11_BOX b = box(0, 16); csr_bad("DstZ=1", 0, 0, 1, 0, 0, &b, CSR_REMOVED); }
TEST(d3d11_buffers_copyregion_empty_box_note) { D3D11_BOX b = box(16, 16); csr_bad("empty box", 0, 0, 0, 0, 0, &b, CSR_IGNORED); }
TEST(d3d11_buffers_copyregion_inverted_box_note) { D3D11_BOX b = box(32, 16); csr_bad("inverted box", 0, 0, 0, 0, 0, &b, CSR_REMOVED); }
TEST(d3d11_buffers_copyregion_bottom2_note) { D3D11_BOX b = box(0, 16); b.bottom = 2; csr_bad("box.bottom=2", 0, 0, 0, 0, 0, &b, CSR_REMOVED); }
TEST(d3d11_buffers_copyregion_back2_note) { D3D11_BOX b = box(0, 16); b.back = 2; csr_bad("box.back=2", 0, 0, 0, 0, 0, &b, CSR_REMOVED); }
TEST(d3d11_buffers_copyregion_dst_subres1_note) { D3D11_BOX b = box(0, 16); csr_bad("dst subresource 1", 0, 0, 0, 1, 0, &b, CSR_IGNORED); }
TEST(d3d11_buffers_copyregion_src_subres1_note) { D3D11_BOX b = box(0, 16); csr_bad("src subresource 1", 0, 0, 0, 0, 1, &b, CSR_IGNORED); }

TEST(d3d11_buffers_copyregion_dest_immutable_unchanged)
{
    ID3D11Buffer *s = mk(64, DEF, VB, 0, 0, 0, 1, 0x01), *d = mk(64, IMM, VB, 0, 0, 0, 1, 0x02);
    D3D11_BOX bx = box(0, 16);
    NEED_DEV();
    if (s && d) {
        ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(d), 0, 0, 0, 0, RES(s), 0, &bx);
        CHECK(bufh_matches_pattern(d, 0x02));
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

TEST(d3d11_buffers_copyregion_staging_write_target)
{
    ID3D11Buffer *s = mk(64, DEF, VB, 0, 0, 0, 1, 0x61), *d = mk(64, STG, 0, CR, 0, 0, 0, 0);
    D3D11_BOX bx = box(8, 40);
    D3D11_MAPPED_SUBRESOURCE m;
    NEED_DEV();
    if (s && d) {
        unsigned char sp[64];
        bufh_pattern(sp, 64, 0x61);
        ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(d), 0, 16, 0, 0, RES(s), 0, &bx);
        EXPECT_HR(ID3D11DeviceContext_Map(g_ctx, RES(d), 0, D3D11_MAP_READ, 0, &m), S_OK);
        if (m.pData) {
            CHECK(!memcmp((unsigned char *)m.pData + 16, sp + 8, 32));
            ID3D11DeviceContext_Unmap(g_ctx, RES(d), 0);
        }
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

TEST(d3d11_buffers_copyregion_structured_elements)
{
    ID3D11Buffer *s, *d;
    D3D11_BOX bx = box(24, 48);
    unsigned char want[96], sp[96];
    NEED_FL11();
    s = mk(96, DEF, SR, 0, STRUCT, 12, 1, 0x71);
    d = mk(96, DEF, SR, 0, STRUCT, 12, 1, 0x72);
    if (s && d) {
        bufh_pattern(want, 96, 0x72); bufh_pattern(sp, 96, 0x71);
        memcpy(want, sp + 24, 24);
        ID3D11DeviceContext_CopySubresourceRegion(g_ctx, RES(d), 0, 0, 0, 0, RES(s), 0, &bx);
        CHECK(bufh_matches(d, want, 96));
    } else CHECK(0);
    SAFE_RELEASE(s); SAFE_RELEASE(d);
}

/* ================= ClearUnorderedAccessView ================= */
static ID3D11UnorderedAccessView *mkuav(ID3D11Buffer *b, DXGI_FORMAT f, UINT first, UINT n, UINT flags)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC d;
    ID3D11UnorderedAccessView *v = NULL;
    memset(&d, 0, sizeof d);
    d.Format = f; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    d.Buffer.FirstElement = first; d.Buffer.NumElements = n; d.Buffer.Flags = flags;
    if (FAILED(ID3D11Device_CreateUnorderedAccessView(g_dev, RES(b), &d, &v))) return NULL;
    return v;
}

TEST(d3d11_buffers_clear_uav_raw_uint)
{
    ID3D11Buffer *b;
    ID3D11UnorderedAccessView *v;
    const UINT vals[4] = { 0xdeadbeef, 0x11111111, 0x22222222, 0x33333333 };
    UINT sz = 0;
    unsigned char *got;
    NEED_FL11();
    b = mk(64, DEF, SR | UA, 0, RAW, 0, 1, 0x10);
    v = b ? mkuav(b, DXGI_FORMAT_R32_TYPELESS, 0, 16, D3D11_BUFFER_UAV_FLAG_RAW) : NULL;
    CHECK(b && v);
    if (v) {
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(g_ctx, v, vals);
        got = bufh_read(b, &sz);
        if (got) {
            UINT i, same = 0;
            CHECK(*(UINT *)got == 0xdeadbeef);
            for (i = 0; i < 16; i++) same += ((UINT *)got)[i] == ((UINT *)got)[0];
            /* Win10: the raw clear replicates Values[0] over the whole buffer */
            EXPECT_EQ(same, 16);
            free(got);
        }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_clear_uav_typed_r32_uint)
{
    ID3D11Buffer *b;
    ID3D11UnorderedAccessView *v;
    const UINT vals[4] = { 0x01020304, 9, 9, 9 };
    UINT sz = 0, i, bad = 0;
    UINT *got;
    NEED_FL11();
    b = mk(64, DEF, SR | UA, 0, 0, 0, 1, 0x10);
    v = b ? mkuav(b, DXGI_FORMAT_R32_UINT, 0, 16, 0) : NULL;
    CHECK(b && v);
    if (v) {
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(g_ctx, v, vals);
        got = (UINT *)bufh_read(b, &sz);
        CHECK(got != NULL);
        for (i = 0; got && i < 16; i++) bad += got[i] != 0x01020304;
        EXPECT_EQ(bad, 0);
        free(got);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_clear_uav_typed_r32_uint_subrange)
{
    ID3D11Buffer *b;
    ID3D11UnorderedAccessView *v;
    const UINT vals[4] = { 0xaabbccdd, 0, 0, 0 };
    UINT sz = 0, i, bad = 0;
    UINT *got;
    UINT base[16];
    NEED_FL11();
    b = mk(64, DEF, SR | UA, 0, 0, 0, 1, 0x20);
    v = b ? mkuav(b, DXGI_FORMAT_R32_UINT, 4, 4, 0) : NULL;
    CHECK(b && v);
    if (v) {
        bufh_pattern((unsigned char *)base, 64, 0x20);
        for (i = 4; i < 8; i++) base[i] = 0xaabbccdd;
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(g_ctx, v, vals);
        got = (UINT *)bufh_read(b, &sz);
        CHECK(got != NULL);
        for (i = 0; got && i < 16; i++) bad += got[i] != base[i];
        EXPECT_EQ(bad, 0);
        free(got);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_clear_uav_typed_r32_float)
{
    ID3D11Buffer *b;
    ID3D11UnorderedAccessView *v;
    const FLOAT vals[4] = { 1.5f, 0, 0, 0 };
    UINT sz = 0, i, bad = 0;
    float *got;
    NEED_FL11();
    b = mk(64, DEF, SR | UA, 0, 0, 0, 1, 0x10);
    v = b ? mkuav(b, DXGI_FORMAT_R32_FLOAT, 0, 16, 0) : NULL;
    CHECK(b && v);
    if (v) {
        ID3D11DeviceContext_ClearUnorderedAccessViewFloat(g_ctx, v, vals);
        got = (float *)bufh_read(b, &sz);
        CHECK(got != NULL);
        for (i = 0; got && i < 16; i++) bad += got[i] != 1.5f;
        EXPECT_EQ(bad, 0);
        free(got);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_clear_uav_typed_r32_sint)
{
    ID3D11Buffer *b;
    ID3D11UnorderedAccessView *v;
    const UINT vals[4] = { (UINT)-5, 0, 0, 0 };
    UINT sz = 0, i, bad = 0;
    int *got;
    NEED_FL11();
    b = mk(32, DEF, SR | UA, 0, 0, 0, 1, 0x10);
    v = b ? mkuav(b, DXGI_FORMAT_R32_SINT, 0, 8, 0) : NULL;
    CHECK(b && v);
    if (v) {
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(g_ctx, v, vals);
        got = (int *)bufh_read(b, &sz);
        CHECK(got != NULL);
        for (i = 0; got && i < 8; i++) bad += got[i] != -5;
        EXPECT_EQ(bad, 0);
        free(got);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_clear_uav_raw_twice)
{
    ID3D11Buffer *b;
    ID3D11UnorderedAccessView *v;
    const UINT a[4] = { 0x11223344, 0, 0, 0 }, c[4] = { 0x55667788, 0, 0, 0 };
    UINT sz = 0;
    UINT *got;
    NEED_FL11();
    b = mk(16, DEF, UA, 0, RAW, 0, 0, 0);
    v = b ? mkuav(b, DXGI_FORMAT_R32_TYPELESS, 0, 4, D3D11_BUFFER_UAV_FLAG_RAW) : NULL;
    CHECK(b && v);
    if (v) {
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(g_ctx, v, a);
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(g_ctx, v, c);
        got = (UINT *)bufh_read(b, &sz);
        if (got) { EXPECT_EQ(got[0], 0x55667788); free(got); }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_clear_uav_structured_note)
{
    ID3D11Buffer *b;
    ID3D11UnorderedAccessView *v;
    const UINT vals[4] = { 0x01010101, 0x02020202, 0x03030303, 0x04040404 };
    UINT sz = 0;
    UINT *got;
    NEED_FL11();
    b = mk(48, DEF, SR | UA, 0, STRUCT, 12, 0, 0);
    v = b ? mkuav(b, DXGI_FORMAT_UNKNOWN, 0, 4, 0) : NULL;
    CHECK(b && v);
    if (v) {
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(g_ctx, v, vals);
        got = (UINT *)bufh_read(b, &sz);
        if (got) {   /* Win10: structured clear replicates Values[0] into every dword of the buffer */
            UINT i;
            for (i = 0; i < 12; i++) EXPECT_EQ(got[i], 0x01010101);
            free(got);
        }
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}
