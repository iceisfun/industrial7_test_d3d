/* groupshared memory, barriers, atomics, raw/structured/typed UAV and SRV access rules. */
#include "hse.h"

#define RUN(src_, in_, inb_, outb_, gx_, out_) ({ hse_cs c_; memset(&c_, 0, sizeof c_); c_.src = (src_); c_.in = (in_); c_.inbytes = (inb_); \
    c_.outbytes = (outb_); c_.gx = (gx_); c_.out = (out_); HSE_DISPATCH(&c_); })

/* v[0..n) must be a permutation of 0..n-1 */
static void check_perm(const uint32_t *v, int n, const char *what) {
    unsigned char *seen = calloc(n, 1);
    int bad = 0;
    for (int i = 0; i < n; i++) { if (v[i] >= (uint32_t)n || seen[v[i]]) bad++; else seen[v[i]] = 1; }
    CHECKF(bad == 0, "%s: %d of %d values duplicated or out of range", what, bad, n);
    free(seen);
}

/* ---- groupshared + barriers ------------------------------------------------------------------------------- */
TEST(hlsl_semantics_exec_mem_group_reverse_through_barrier) {
    HSE_NEED();
    uint32_t out[64];
    if (!RUN("RWByteAddressBuffer O : register(u0); groupshared uint g[64];\n"
             "[numthreads(64,1,1)] void main(uint gi : SV_GroupIndex) { g[gi] = gi * 7 + 1; GroupMemoryBarrierWithGroupSync(); O.Store(gi * 4, g[63 - gi]); }",
             NULL, 0, sizeof out, 1, out)) return;
    for (int i = 0; i < 64; i++) EXPECT_EQ(out[i], (63 - i) * 7 + 1);
}
TEST(hlsl_semantics_exec_mem_group_rotate_neighbour_all_memory_barrier) {
    HSE_NEED();
    uint32_t out[128];
    if (!RUN("RWByteAddressBuffer O : register(u0); groupshared uint g[128];\n"
             "[numthreads(128,1,1)] void main(uint gi : SV_GroupIndex) { g[gi] = gi ^ 0x55; GroupMemoryBarrierWithGroupSync(); uint v = g[(gi + 1) & 127]; AllMemoryBarrierWithGroupSync(); g[gi] = v + 1000; GroupMemoryBarrierWithGroupSync(); O.Store(gi * 4, g[(gi + 127) & 127]); }",
             NULL, 0, sizeof out, 1, out)) return;
    for (int i = 0; i < 128; i++) EXPECT_EQ(out[i], (uint32_t)((i ^ 0x55) + 1000));
}
TEST(hlsl_semantics_exec_mem_group_inclusive_scan) {
    HSE_NEED();
    uint32_t in[64], out[64];
    for (int i = 0; i < 64; i++) in[i] = (uint32_t)(i * i + 1);
    if (!RUN("ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0); groupshared uint g[2][64];\n"
             "[numthreads(64,1,1)] void main(uint gi : SV_GroupIndex) {\n"
             "  uint src = 0; g[0][gi] = I.Load(gi * 4); GroupMemoryBarrierWithGroupSync();\n"
             "  [loop] for (uint d = 1; d < 64; d *= 2) { uint v = g[src][gi]; if (gi >= d) v += g[src][gi - d]; g[1 - src][gi] = v; src = 1 - src; GroupMemoryBarrierWithGroupSync(); }\n"
             "  O.Store(gi * 4, g[src][gi]); }", in, sizeof in, sizeof out, 1, out)) return;
    uint32_t acc = 0;
    for (int i = 0; i < 64; i++) { acc += in[i]; EXPECT_EQ(out[i], acc); }
}
TEST(hlsl_semantics_exec_mem_group_tree_reduction_256) {
    HSE_NEED();
    uint32_t in[256], out[1];
    uint32_t want = 0;
    for (int i = 0; i < 256; i++) { in[i] = (uint32_t)(i * 2654435761u); want += in[i]; }
    if (!RUN("ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0); groupshared uint g[256];\n"
             "[numthreads(256,1,1)] void main(uint gi : SV_GroupIndex) {\n"
             "  g[gi] = I.Load(gi * 4); GroupMemoryBarrierWithGroupSync();\n"
             "  [loop] for (uint s = 128; s > 0; s >>= 1) { if (gi < s) g[gi] += g[gi + s]; GroupMemoryBarrierWithGroupSync(); }\n"
             "  if (gi == 0) O.Store(0, g[0]); }", in, sizeof in, 4, 1, out)) return;
    EXPECT_EQ(out[0], want);
}
TEST(hlsl_semantics_exec_mem_group_memory_is_per_group) {
    HSE_NEED();
    uint32_t out[4];
    if (!RUN("RWByteAddressBuffer O : register(u0); groupshared uint s;\n"
             "[numthreads(32,1,1)] void main(uint gi : SV_GroupIndex, uint3 gid : SV_GroupID) {\n"
             "  if (gi == 0) s = 0; GroupMemoryBarrierWithGroupSync();\n"
             "  InterlockedAdd(s, gid.x * 1000 + gi); GroupMemoryBarrierWithGroupSync();\n"
             "  if (gi == 0) O.Store(gid.x * 4, s); }", NULL, 0, sizeof out, 4, out)) return;
    for (uint32_t g = 0; g < 4; g++) EXPECT_EQ(out[g], g * 1000 * 32 + 31 * 32 / 2);
}
TEST(hlsl_semantics_exec_mem_group_atomic_add_unique_originals) {
    HSE_NEED();
    uint32_t out[257];
    if (!RUN("RWByteAddressBuffer O : register(u0); groupshared uint cnt;\n"
             "[numthreads(256,1,1)] void main(uint gi : SV_GroupIndex) {\n"
             "  if (gi == 0) cnt = 0; GroupMemoryBarrierWithGroupSync();\n"
             "  uint orig; InterlockedAdd(cnt, 1, orig); O.Store(gi * 4, orig); GroupMemoryBarrierWithGroupSync();\n"
             "  if (gi == 0) O.Store(1024, cnt); }", NULL, 0, sizeof out, 1, out)) return;
    check_perm(out, 256, "originals of InterlockedAdd");
    EXPECT_EQ(out[256], 256);
}
TEST(hlsl_semantics_exec_mem_group_atomic_ops_final_values) {
    HSE_NEED();
    uint32_t out[8] = { 0 };
    if (!RUN("RWByteAddressBuffer O : register(u0); groupshared uint gmax, gmin, gor, gand, gxor, gadd;\n"
             "[numthreads(64,1,1)] void main(uint gi : SV_GroupIndex) {\n"
             "  if (gi == 0) { gmax = 0; gmin = 0xffffffff; gor = 0; gand = 0xffffffff; gxor = 0; gadd = 0; } GroupMemoryBarrierWithGroupSync();\n"
             "  InterlockedMax(gmax, gi * 3); InterlockedMin(gmin, 1000 - gi); InterlockedOr(gor, 1u << (gi & 31)); InterlockedAnd(gand, ~(1u << (gi & 31)));\n"
             "  InterlockedXor(gxor, gi + 1); InterlockedAdd(gadd, gi); GroupMemoryBarrierWithGroupSync();\n"
             "  if (gi == 0) { O.Store(0, gmax); O.Store(4, gmin); O.Store(8, gor); O.Store(12, gand); O.Store(16, gxor); O.Store(20, gadd); } }",
             NULL, 0, sizeof out, 1, out)) return;
    EXPECT_EQ(out[0], 63 * 3); EXPECT_EQ(out[1], 1000 - 63); EXPECT_EQ(out[2], 0xffffffffu); EXPECT_EQ(out[3], 0); EXPECT_EQ(out[4], 64); EXPECT_EQ(out[5], 2016);
}
TEST(hlsl_semantics_exec_mem_group_atomic_exchange_compare) {
    HSE_NEED();
    uint32_t out[8] = { 0 };
    if (!RUN("RWByteAddressBuffer O : register(u0); groupshared uint v;\n"
             "[numthreads(1,1,1)] void main() {\n"
             "  uint o1, o2, o3, o4; v = 10;\n"
             "  InterlockedExchange(v, 20, o1); InterlockedCompareExchange(v, 20, 30, o2); InterlockedCompareExchange(v, 99, 40, o3); InterlockedCompareStore(v, 30, 50); o4 = v;\n"
             "  O.Store4(0, uint4(o1, o2, o3, o4)); O.Store(16, v); }", NULL, 0, sizeof out, 1, out)) return;
    EXPECT_EQ(out[0], 10); EXPECT_EQ(out[1], 20); EXPECT_EQ(out[2], 30); EXPECT_EQ(out[3], 50);
}

/* ---- UAV atomics ------------------------------------------------------------------------------------------------- */
TEST(hlsl_semantics_exec_mem_uav_atomic_add_counter_across_groups) {
    HSE_NEED();
    uint32_t out[1 + 256];
    if (!RUN("RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) { uint orig; O.InterlockedAdd(0, 1, orig); O.Store(4 + id.x * 4, orig); }",
             NULL, 0, sizeof out, 4, out)) return;
    EXPECT_EQ(out[0], 256);
    check_perm(out + 1, 256, "originals");
}
TEST(hlsl_semantics_exec_mem_uav_atomic_add_without_original) {
    HSE_NEED();
    uint32_t init[2] = { 100, 0 }, out[2];
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0);\n[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) { O.InterlockedAdd(0, id.x + 1); O.InterlockedXor(4, 1u << (id.x & 31)); }";
    c.outinit = init; c.outbytes = 8; c.gx = 2; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    EXPECT_EQ(out[0], 100 + 128 * 129 / 2);
    EXPECT_EQ(out[1], 0);   /* every bit toggled four times */
}
TEST(hlsl_semantics_exec_mem_uav_atomic_sequence_single_thread) {
    HSE_NEED();
    uint32_t init[24] = { 0xF0F0 }, out[24];
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(1,1,1)] void main() {\n"
            "  uint o[10];\n"
            "  O.InterlockedAnd(0, 0x0FF0, o[0]);  O.InterlockedOr(0, 0x000F, o[1]);  O.InterlockedXor(0, 0xFF00, o[2]);\n"
            "  O.InterlockedMin(0, 0x1234, o[3]);  O.InterlockedMax(0, 0x5000, o[4]);  O.InterlockedExchange(0, 0x77, o[5]);\n"
            "  O.InterlockedCompareExchange(0, 0x77, 0x88, o[6]);  O.InterlockedCompareExchange(0, 0x99, 0xAA, o[7]);\n"
            "  O.InterlockedAdd(0, 0x10, o[8]);  O.InterlockedCompareStore(0, 0x98, 0x10);\n"
            "  uint fin = O.Load(0);\n"
            "  [unroll] for (uint i = 0; i < 9; i++) O.Store(16 + i * 4, o[i]);\n"
            "  O.Store(8, fin); }";
    c.outinit = init; c.outbytes = sizeof init; c.gx = 1; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    static const uint32_t want[9] = { 0xF0F0, 0x00F0, 0x00FF, 0xFFFF, 0x1234, 0x5000, 0x77, 0x88, 0x88 };
    for (int i = 0; i < 9; i++) CHECKF(out[4 + i] == want[i], "original %d got %x want %x", i, out[4 + i], want[i]);
    EXPECT_EQ(out[2], 0x10);   /* 0x88 + 0x10 = 0x98, then CompareStore(0x98 -> 0x10) */
}
TEST(hlsl_semantics_exec_mem_uav_atomic_unsigned_min_max) {
    HSE_NEED();
    uint32_t init[4] = { 0x80000000u, 0x80000000u, 5, 5 }, out[8];
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(1,1,1)] void main() { uint a, b, c, d; O.InterlockedMin(0, 1, a); O.InterlockedMax(4, 1, b); O.InterlockedMax(8, 0xffffffff, c); O.InterlockedMin(12, 0xffffffff, d);\n"
            "  O.Store4(16, uint4(a, b, c, d)); }";
    c.outinit = init; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    EXPECT_EQ(out[0], 1); EXPECT_EQ(out[1], 0x80000000u); EXPECT_EQ(out[2], 0xffffffffu); EXPECT_EQ(out[3], 5);
    EXPECT_EQ(out[4], 0x80000000u); EXPECT_EQ(out[5], 0x80000000u); EXPECT_EQ(out[6], 5); EXPECT_EQ(out[7], 5);
}
TEST(hlsl_semantics_exec_mem_uav_atomic_wraparound) {
    HSE_NEED();
    uint32_t init[2] = { 0xffffffffu, 5 }, out[4];
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { uint a, b; O.InterlockedAdd(0, 1, a); O.InterlockedAdd(4, 0xfffffffd, b); O.Store(8, a); O.Store(12, b); }";
    c.outinit = init; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[1], 2); EXPECT_EQ(out[2], 0xffffffffu); EXPECT_EQ(out[3], 5);
}
TEST(hlsl_semantics_exec_mem_uav_atomic_signed_min_max_structured) {
    HSE_NEED();
    int32_t sinit[2] = { -5, INT32_MIN };
    ID3D11Buffer *sb = hse_buf(sizeof sinit, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, sinit);
    ID3D11UnorderedAccessView *su = sb ? hse_buf_uav(sb, DXGI_FORMAT_UNKNOWN, 0, 2, 0) : NULL;
    uint32_t out[8] = { 0 };
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0); RWStructuredBuffer<int> S : register(u1);\n"
            "[numthreads(1,1,1)] void main() { int o1, o2, o3, o4, o5; InterlockedMin(S[0], -10, o1); InterlockedMax(S[0], -3, o2); InterlockedMax(S[0], -20, o3); InterlockedAdd(S[0], 10, o4);\n"
            "  InterlockedMin(S[1], 1, o5); O.Store4(0, uint4(o1, o2, o3, o4)); O.Store(16, o5); }";
    c.uav[0] = su; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    int ok = HSE_DISPATCH(&c);
    if (ok) {
        EXPECT_EQ(out[0], (uint32_t)-5); EXPECT_EQ(out[1], (uint32_t)-10); EXPECT_EQ(out[2], (uint32_t)-3); EXPECT_EQ(out[3], (uint32_t)-3);
        EXPECT_EQ(out[4], 0x80000000u);   /* signed: INT_MIN is already smaller than 1 */
        void *d = NULL; size_t n = 0;
        if (hse_read_buf(sb, &d, &n) && n >= 8) { EXPECT_EQ(((int32_t *)d)[0], 7); EXPECT_EQ(((int32_t *)d)[1], INT32_MIN); }
        free(d);
    }
    SAFE_RELEASE(su); SAFE_RELEASE(sb);
}
TEST(hlsl_semantics_exec_mem_texture_uav_atomic_add) {
    HSE_NEED();
    HSE_RUN();
    ID3D11Texture2D *t = hse_tex2d(4, 4, 1, 1, DXGI_FORMAT_R32_UINT, D3D11_BIND_UNORDERED_ACCESS, NULL);
    if (!t) { t_skip("R32_UINT UAV texture unavailable"); return; }
    ID3D11UnorderedAccessView *u = NULL;
    ID3D11Device_CreateUnorderedAccessView(hse.dev, (ID3D11Resource *)t, NULL, &u);
    uint32_t out[64];
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0); RWTexture2D<uint> T : register(u1);\n"
            "[numthreads(64,1,1)] void main(uint gi : SV_GroupIndex) { uint orig; InterlockedAdd(T[uint2(1, 2)], 3, orig); O.Store(gi * 4, orig); }";
    c.uav[0] = u; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (u && HSE_DISPATCH(&c)) {
        for (int i = 0; i < 64; i++) out[i] /= 3;
        check_perm(out, 64, "texture atomic originals / 3");
        d3dh_image im = { 0 };
        if (SUCCEEDED(d3dh_readback_texture2d11(hse.dev, hse.ctx, t, 0, &im))) {
            EXPECT_EQ(d3dh_image_pixel32(&im, 1, 2), 192); EXPECT_EQ(d3dh_image_pixel32(&im, 0, 0), 0);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(t);
}

/* ---- byte-address (raw) buffers ------------------------------------------------------------------------------------ */
static void fill_words(uint32_t *w, int n) { for (int i = 0; i < n; i++) w[i] = (uint32_t)(i + 1) * 0x01010101u ^ 0x10000000u; }
TEST(hlsl_semantics_exec_mem_raw_load_widths) {
    HSE_NEED();
    uint32_t in[16], out[12 * 10];
    fill_words(in, 16);
    if (!RUN("ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(12,1,1)] void main(uint gi : SV_GroupIndex) {\n"
             "  uint o = gi * 4; uint a = I.Load(o); uint2 b = I.Load2(o); uint3 c = I.Load3(o); uint4 d = I.Load4(o);\n"
             "  O.Store(gi * 40, a); O.Store2(gi * 40 + 4, b); O.Store3(gi * 40 + 12, uint3(c)); O.Store4(gi * 40 + 24, d); }", in, sizeof in, sizeof out, 1, out)) return;
    for (int t = 0; t < 12; t++) {
        uint32_t *o = out + t * 10;
        EXPECT_EQ(o[0], in[t]);
        for (int k = 0; k < 2; k++) EXPECT_EQ(o[1 + k], in[t + k]);
        for (int k = 0; k < 3; k++) EXPECT_EQ(o[3 + k], in[t + k]);
        for (int k = 0; k < 4 && 6 + k < 10; k++) EXPECT_EQ(o[6 + k], in[t + k]);
    }
}
TEST(hlsl_semantics_exec_mem_raw_store_widths_roundtrip) {
    HSE_NEED();
    uint32_t out[32];
    if (!RUN("RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(1,1,1)] void main() { O.Store(0, 1); O.Store2(4, uint2(2, 3)); O.Store3(12, uint3(4, 5, 6)); O.Store4(24, uint4(7, 8, 9, 10));\n"
             "  O.Store(64, asuint(1.5)); O.Store2(68, uint2(asuint(2.5), 0x80000000)); O.Store4(80, asuint(float4(1, 2, 3, 4))); }", NULL, 0, sizeof out, 1, out)) return;
    for (int i = 0; i < 10; i++) EXPECT_EQ(out[i], (uint32_t)(i + 1));
    EXPECT_EQ(out[16], FB(1.5f)); EXPECT_EQ(out[17], FB(2.5f)); EXPECT_EQ(out[18], F_NZERO);
    for (int i = 0; i < 4; i++) EXPECT_EQ(out[20 + i], FB((float)(i + 1)));
}
TEST(hlsl_semantics_exec_mem_raw_load_out_of_bounds_returns_zero) {
    HSE_NEED();
    uint32_t in[16], out[16];
    fill_words(in, 16);
    if (!RUN("ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(1,1,1)] void main() { uint4 a = I.Load4(64); O.Store4(0, a); O.Store(16, I.Load(68)); O.Store(20, I.Load(4096)); O.Store2(24, I.Load2(128)); O.Store3(32, I.Load3(1024));\n"
             "  O.Store(44, I.Load(60)); }", in, sizeof in, sizeof out, 1, out)) return;
    for (int i = 0; i < 4; i++) EXPECT_EQ(out[i], 0);
    EXPECT_EQ(out[4], 0); EXPECT_EQ(out[5], 0); EXPECT_EQ(out[6], 0); EXPECT_EQ(out[7], 0);
    for (int i = 8; i < 11; i++) EXPECT_EQ(out[i], 0);
    EXPECT_EQ(out[11], in[15]);   /* the last in-bounds word is still readable */
}
TEST(hlsl_semantics_exec_mem_raw_load_partially_out_of_bounds_observed) {
    HSE_NEED();
    uint32_t in[16], out[8];
    fill_words(in, 16);
    if (!RUN("ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(1,1,1)] void main() { O.Store4(0, I.Load4(56)); O.Store3(16, I.Load3(60)); }", in, sizeof in, sizeof out, 1, out)) return;
    t_note("Load4(56) on a 64-byte raw buffer: %08x %08x %08x %08x (in-bounds words %08x %08x)", out[0], out[1], out[2], out[3], in[14], in[15]);
    t_note("Load3(60): %08x %08x %08x (in-bounds word %08x)", out[4], out[5], out[6], in[15]);
}
TEST(hlsl_semantics_exec_mem_raw_load_unaligned_offset_observed) {
    HSE_NEED();
    uint32_t in[16], out[4];
    fill_words(in, 16);
    if (!RUN("ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(1,1,1)] void main() { O.Store4(0, uint4(I.Load(6), I.Load(5), I.Load(7), I.Load(9))); }", in, sizeof in, sizeof out, 1, out)) return;
    t_note("misaligned raw loads at 6,5,7,9 return %08x %08x %08x %08x (words %08x %08x %08x)", out[0], out[1], out[2], out[3], in[1], in[2], in[3]);
}
TEST(hlsl_semantics_exec_mem_raw_store_out_of_bounds_ignored) {
    HSE_NEED();
    uint32_t init[16], out[16];
    for (int i = 0; i < 16; i++) init[i] = 0xA0A00000u + i;
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWByteAddressBuffer O : register(u0);\n[numthreads(1,1,1)] void main() { O.Store(64, 1); O.Store4(128, uint4(2, 3, 4, 5)); O.Store(100000, 6); O.Store(60, 0x1234); }";
    c.outinit = init; c.outbytes = sizeof init; c.gx = 1; c.out = out;
    if (!HSE_DISPATCH(&c)) return;
    for (int i = 0; i < 15; i++) EXPECT_EQ(out[i], init[i]);
    EXPECT_EQ(out[15], 0x1234);
}
TEST(hlsl_semantics_exec_mem_raw_getdimensions_bytes) {
    HSE_NEED();
    uint32_t in[24], out[4];
    fill_words(in, 24);
    if (!RUN("ByteAddressBuffer I : register(t0); RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(1,1,1)] void main() { uint a, b; I.GetDimensions(a); O.GetDimensions(b); O.Store2(0, uint2(a, b)); }", in, sizeof in, 16, 1, out)) return;
    EXPECT_EQ(out[0], 96); EXPECT_EQ(out[1], 16);
}
TEST(hlsl_semantics_exec_mem_raw_srv_view_range_bounds) {
    HSE_NEED();
    uint32_t in[16], out[8];
    fill_words(in, 16);
    ID3D11Buffer *b = hse_buf(sizeof in, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, in);
    ID3D11ShaderResourceView *v = b ? hse_buf_srv(b, 0, 4, 8, 1) : NULL;
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "ByteAddressBuffer V : register(t1); RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(1,1,1)] void main() { uint n; V.GetDimensions(n); O.Store4(0, uint4(n, V.Load(0), V.Load(28), V.Load(32))); O.Store(16, V.Load(36)); }";
    c.srv[0] = v; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (HSE_DISPATCH(&c)) {
        EXPECT_EQ(out[0], 32); EXPECT_EQ(out[1], in[4]); EXPECT_EQ(out[2], in[11]);
        EXPECT_EQ(out[3], 0);   /* just past the view, but inside the buffer */
        EXPECT_EQ(out[4], 0);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

/* ---- structured buffers ---------------------------------------------------------------------------------------------- */
static int run_structured(const char *src, const void *data, UINT bytes, UINT stride, UINT first, UINT num, void *out, UINT outbytes) {
    ID3D11Buffer *b = hse_buf(bytes, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, stride, data);
    ID3D11ShaderResourceView *v = b ? hse_buf_srv(b, DXGI_FORMAT_UNKNOWN, first, num, 0) : NULL;
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = src; c.srv[0] = v; c.outbytes = outbytes; c.gx = 1; c.out = out;
    int ok = HSE_DISPATCH(&c);
    SAFE_RELEASE(v); SAFE_RELEASE(b);
    return ok;
}
TEST(hlsl_semantics_exec_mem_structured_member_offsets) {
    HSE_NEED();
    struct S { float p[3]; uint32_t id; } d[8];
    for (int i = 0; i < 8; i++) { d[i].p[0] = (float)i; d[i].p[1] = (float)i * 2; d[i].p[2] = (float)i * 3; d[i].id = 100 + i; }
    uint32_t out[8 * 4 + 4];
    if (!run_structured("struct S { float3 p; uint id; }; StructuredBuffer<S> B : register(t1); RWByteAddressBuffer O : register(u0);\n"
                        "[numthreads(8,1,1)] void main(uint gi : SV_GroupIndex) { S s = B[gi]; O.Store4(gi * 16, uint4(asuint(s.p.x), asuint(s.p.y), asuint(s.p.z), s.id));\n"
                        "  if (gi == 0) { uint n, st; B.GetDimensions(n, st); O.Store2(128, uint2(n, st)); } }", d, sizeof d, 16, 0, 8, out, sizeof out)) return;
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(out[i * 4], FB((float)i)); EXPECT_EQ(out[i * 4 + 1], FB((float)i * 2)); EXPECT_EQ(out[i * 4 + 2], FB((float)i * 3)); EXPECT_EQ(out[i * 4 + 3], 100 + i);
    }
    EXPECT_EQ(out[32], 8); EXPECT_EQ(out[33], 16);
}
TEST(hlsl_semantics_exec_mem_structured_stride_not_multiple_of_16) {
    HSE_NEED();
    uint32_t d[6 * 5], out[6 * 5 + 2];
    for (int i = 0; i < 30; i++) d[i] = 1000 + i;
    if (!run_structured("struct T { uint a[5]; }; StructuredBuffer<T> B : register(t1); RWByteAddressBuffer O : register(u0);\n"
                        "[numthreads(6,1,1)] void main(uint gi : SV_GroupIndex) { [unroll] for (uint k = 0; k < 5; k++) O.Store((gi * 5 + k) * 4, B[gi].a[k]);\n"
                        "  if (gi == 0) { uint n, st; B.GetDimensions(n, st); O.Store2(120, uint2(n, st)); } }", d, sizeof d, 20, 0, 6, out, sizeof out)) return;
    for (int i = 0; i < 30; i++) EXPECT_EQ(out[i], d[i]);
    EXPECT_EQ(out[30], 6); EXPECT_EQ(out[31], 20);
}
TEST(hlsl_semantics_exec_mem_structured_float3_packed_stride_12) {
    HSE_NEED();
    float d[5 * 3], out[5 * 3 + 2];
    for (int i = 0; i < 15; i++) d[i] = (float)(i + 1) * 0.5f;
    if (!run_structured("StructuredBuffer<float3> B : register(t1); RWByteAddressBuffer O : register(u0);\n"
                        "[numthreads(5,1,1)] void main(uint gi : SV_GroupIndex) { float3 v = B[gi]; O.Store3(gi * 12, asuint(v)); if (gi == 0) { uint n, st; B.GetDimensions(n, st); O.Store2(60, uint2(n, st)); } }",
                        d, sizeof d, 12, 0, 5, out, sizeof out)) return;
    for (int i = 0; i < 15; i++) EXPECT_EQ(hse_fb(out[i]), hse_fb(d[i]));
    EXPECT_EQ(((uint32_t *)out)[15], 5); EXPECT_EQ(((uint32_t *)out)[16], 12);
}
TEST(hlsl_semantics_exec_mem_structured_mixed_member_types) {
    HSE_NEED();
    struct M { int32_t i; uint32_t u; float f; uint32_t pad; } d[4];
    for (int i = 0; i < 4; i++) { d[i].i = -i - 1; d[i].u = 0xfffffff0u + i; d[i].f = -(float)i - 0.25f; d[i].pad = 0; }
    uint32_t out[16];
    if (!run_structured("struct M { int i; uint u; float f; uint pad; }; StructuredBuffer<M> B : register(t1); RWByteAddressBuffer O : register(u0);\n"
                        "[numthreads(4,1,1)] void main(uint gi : SV_GroupIndex) { M m = B[gi]; O.Store4(gi * 16, uint4(asuint(m.i + 1), m.u + 1, asuint(m.f * 2.0), 7)); }",
                        d, sizeof d, 16, 0, 4, out, sizeof out)) return;
    for (int i = 0; i < 4; i++) { EXPECT_EQ(out[i * 4], (uint32_t)(d[i].i + 1)); EXPECT_EQ(out[i * 4 + 1], d[i].u + 1); EXPECT_EQ(out[i * 4 + 2], FB(d[i].f * 2.0f)); EXPECT_EQ(out[i * 4 + 3], 7); }
}
TEST(hlsl_semantics_exec_mem_structured_out_of_bounds_returns_zero) {
    HSE_NEED();
    uint32_t d[4 * 4], out[8];
    for (int i = 0; i < 16; i++) d[i] = 77 + i;
    if (!run_structured("StructuredBuffer<uint4> B : register(t1); RWByteAddressBuffer O : register(u0);\n"
                        "[numthreads(1,1,1)] void main() { O.Store4(0, B[4]); O.Store4(16, B[1000]); }", d, sizeof d, 16, 0, 4, out, sizeof out)) return;
    for (int i = 0; i < 8; i++) EXPECT_EQ(out[i], 0);
}
TEST(hlsl_semantics_exec_mem_structured_view_range) {
    HSE_NEED();
    uint32_t d[8], out[8];
    for (int i = 0; i < 8; i++) d[i] = 500 + i;
    if (!run_structured("StructuredBuffer<uint> B : register(t1); RWByteAddressBuffer O : register(u0);\n"
                        "[numthreads(1,1,1)] void main() { uint n, st; B.GetDimensions(n, st); O.Store4(0, uint4(n, st, B[0], B[2])); O.Store2(16, uint2(B[3], B[7])); }", d, sizeof d, 4, 2, 3, out, sizeof out)) return;
    EXPECT_EQ(out[0], 3); EXPECT_EQ(out[1], 4); EXPECT_EQ(out[2], 502); EXPECT_EQ(out[3], 504); EXPECT_EQ(out[4], 0); EXPECT_EQ(out[5], 0);
}
TEST(hlsl_semantics_exec_mem_structured_rw_store_layout) {
    HSE_NEED();
    ID3D11Buffer *b = hse_buf(5 * 24, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 24, NULL);
    ID3D11UnorderedAccessView *u = b ? hse_buf_uav(b, DXGI_FORMAT_UNKNOWN, 0, 5, 0) : NULL;
    hse_cs c; memset(&c, 0, sizeof c);
    uint32_t dummy[4];
    c.src = "struct R { uint a; float2 b; uint c[3]; }; RWStructuredBuffer<R> B : register(u1); RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(5,1,1)] void main(uint gi : SV_GroupIndex) { R r; r.a = gi; r.b = float2(gi, -(float)gi); r.c[0] = gi * 10; r.c[1] = gi * 100; r.c[2] = gi * 1000; B[gi] = r; }";
    c.uav[0] = u; c.outbytes = sizeof dummy; c.gx = 1; c.out = dummy;
    if (u && HSE_DISPATCH(&c)) {
        uint32_t *w = NULL; size_t n = 0;
        if (hse_read_buf(b, (void **)&w, &n) && n >= 120) {
            for (uint32_t i = 0; i < 5; i++) {
                uint32_t *e = w + i * 6;
                EXPECT_EQ(e[0], i); EXPECT_EQ(e[1], FB((float)i)); EXPECT_EQ(e[2], FB(-(float)i)); EXPECT_EQ(e[3], i * 10); EXPECT_EQ(e[4], i * 100); EXPECT_EQ(e[5], i * 1000);
            }
        }
        free(w);
    }
    SAFE_RELEASE(u); SAFE_RELEASE(b);
}
TEST(hlsl_semantics_exec_mem_structured_append_buffer_counts) {
    HSE_NEED();
    HSE_RUN();
    ID3D11Buffer *b = hse_buf(64 * 4, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, NULL);
    ID3D11Buffer *cnt = NULL;
    ID3D11UnorderedAccessView *u = NULL;
    if (b) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_UNKNOWN; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; d.Buffer.NumElements = 64; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
        ID3D11Device_CreateUnorderedAccessView(hse.dev, (ID3D11Resource *)b, &d, &u);
    }
    if (!u) { t_skip("append UAV unavailable"); SAFE_RELEASE(b); SAFE_RELEASE(cnt); return; }
    {
        D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
        bd.ByteWidth = 16; bd.Usage = D3D11_USAGE_DEFAULT;
        ID3D11Device_CreateBuffer(hse.dev, &bd, NULL, &cnt);
    }
    ID3DBlob *bl = HSE_COMPILE("AppendStructuredBuffer<uint> A : register(u0);\n[numthreads(64,1,1)] void main(uint gi : SV_GroupIndex) { A.Append(gi * 3 + 1); }", "cs_5_0", 0);
    ID3D11ComputeShader *cs = NULL;
    if (bl && SUCCEEDED(ID3D11Device_CreateComputeShader(hse.dev, d3dh_blob_ptr(bl), d3dh_blob_size(bl), NULL, &cs))) {
        UINT zero = 0;
        ID3D11DeviceContext_CSSetShader(hse.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetUnorderedAccessViews(hse.ctx, 0, 1, &u, &zero);
        ID3D11DeviceContext_Dispatch(hse.ctx, 1, 1, 1);
        ID3D11UnorderedAccessView *nu = NULL;
        ID3D11DeviceContext_CSSetUnorderedAccessViews(hse.ctx, 0, 1, &nu, NULL);
        ID3D11DeviceContext_CopyStructureCount(hse.ctx, cnt, 0, u);
        uint32_t *cw = NULL; size_t n = 0;
        if (hse_read_buf(cnt, (void **)&cw, &n)) EXPECT_EQ(cw[0], 64);
        free(cw);
        uint32_t *w = NULL;
        if (hse_read_buf(b, (void **)&w, &n) && n >= 256) {
            uint32_t seen = 0; int bad = 0;
            unsigned char hit[64] = { 0 };
            for (int i = 0; i < 64; i++) { uint32_t v = w[i]; if (v < 1 || (v - 1) % 3 || (v - 1) / 3 >= 64 || hit[(v - 1) / 3]) bad++; else hit[(v - 1) / 3] = 1; seen++; }
            CHECKF(bad == 0, "append produced %d duplicate or foreign values", bad);
        }
        free(w);
    }
    SAFE_RELEASE(cs); SAFE_RELEASE(bl); SAFE_RELEASE(u); SAFE_RELEASE(cnt); SAFE_RELEASE(b);
}
TEST(hlsl_semantics_exec_mem_structured_counter_increment_unique) {
    HSE_NEED();
    HSE_RUN();
    ID3D11Buffer *b = hse_buf(128 * 4, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, NULL);
    ID3D11UnorderedAccessView *u = NULL;
    if (b) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_UNKNOWN; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; d.Buffer.NumElements = 128; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
        ID3D11Device_CreateUnorderedAccessView(hse.dev, (ID3D11Resource *)b, &d, &u);
    }
    if (!u) { t_skip("counter UAV unavailable"); SAFE_RELEASE(b); return; }
    ID3DBlob *bl = HSE_COMPILE("RWStructuredBuffer<uint> A : register(u0);\n[numthreads(64,1,1)] void main(uint gi : SV_GroupIndex) { uint i = A.IncrementCounter(); A[i] = gi + 1000; }", "cs_5_0", 0);
    ID3D11ComputeShader *cs = NULL;
    if (bl && SUCCEEDED(ID3D11Device_CreateComputeShader(hse.dev, d3dh_blob_ptr(bl), d3dh_blob_size(bl), NULL, &cs))) {
        UINT start = 10;
        ID3D11DeviceContext_CSSetShader(hse.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetUnorderedAccessViews(hse.ctx, 0, 1, &u, &start);
        ID3D11DeviceContext_Dispatch(hse.ctx, 1, 1, 1);
        ID3D11UnorderedAccessView *nu = NULL;
        ID3D11DeviceContext_CSSetUnorderedAccessViews(hse.ctx, 0, 1, &nu, NULL);
        uint32_t *w = NULL; size_t n = 0;
        if (hse_read_buf(b, (void **)&w, &n) && n >= 512) {
            int stored = 0;
            for (int i = 0; i < 128; i++) { if (i >= 10 && i < 74) { CHECKF(w[i] >= 1000 && w[i] < 1064, "slot %d = %u", i, w[i]); stored += w[i] >= 1000; } else CHECKF(w[i] == 0, "slot %d outside the counter range was written (%u)", i, w[i]); }
            EXPECT_EQ(stored, 64);
        }
        free(w);
    }
    SAFE_RELEASE(cs); SAFE_RELEASE(bl); SAFE_RELEASE(u); SAFE_RELEASE(b);
}

/* ---- typed UAV access ------------------------------------------------------------------------------------------------------ */
TEST(hlsl_semantics_exec_mem_typed_uav_load_r32_formats_supported) {
    HSE_NEED();
    HSE_RUN();
    if (hse.fl < D3D_FEATURE_LEVEL_11_0) { t_skip("needs feature level 11_0"); return; }
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32_FLOAT };
    for (int i = 0; i < 3; i++) {
        D3D11_FEATURE_DATA_FORMAT_SUPPORT2 s; memset(&s, 0, sizeof s); s.InFormat = f[i];
        HRESULT hr = ID3D11Device_CheckFeatureSupport(hse.dev, D3D11_FEATURE_FORMAT_SUPPORT2, &s, sizeof s);
        EXPECT_HR_OK(hr);
        CHECKF(s.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD, "format %d lacks typed UAV load (support2=%x)", f[i], s.OutFormatSupport2);
        CHECKF(s.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE, "format %d lacks typed UAV store", f[i]);
        if (f[i] != DXGI_FORMAT_R32_FLOAT)
            CHECKF(s.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD, "format %d lacks atomic add (support2=%x)", f[i], s.OutFormatSupport2);
    }
}
TEST(hlsl_semantics_exec_mem_typed_uav_load_other_formats_observed) {
    HSE_NEED();
    HSE_RUN();
    static const struct { DXGI_FORMAT f; const char *n; } f[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM" }, { DXGI_FORMAT_R8G8B8A8_UINT, "R8G8B8A8_UINT" }, { DXGI_FORMAT_R16G16B16A16_FLOAT, "R16G16B16A16_FLOAT" },
        { DXGI_FORMAT_R32G32B32A32_FLOAT, "R32G32B32A32_FLOAT" }, { DXGI_FORMAT_R32G32B32A32_UINT, "R32G32B32A32_UINT" }, { DXGI_FORMAT_R16_FLOAT, "R16_FLOAT" },
        { DXGI_FORMAT_R8_UNORM, "R8_UNORM" }, { DXGI_FORMAT_R16_UINT, "R16_UINT" }, { DXGI_FORMAT_R32G32_FLOAT, "R32G32_FLOAT" }, { DXGI_FORMAT_B8G8R8A8_UNORM, "B8G8R8A8_UNORM" } };
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++) {
        D3D11_FEATURE_DATA_FORMAT_SUPPORT2 s; memset(&s, 0, sizeof s); s.InFormat = f[i].f;
        if (FAILED(ID3D11Device_CheckFeatureSupport(hse.dev, D3D11_FEATURE_FORMAT_SUPPORT2, &s, sizeof s))) continue;
        t_note("%s: typed UAV load=%d store=%d", f[i].n, !!(s.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD), !!(s.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE));
    }
}
static ID3D11UnorderedAccessView *tex_uav(ID3D11Texture2D *t) { ID3D11UnorderedAccessView *u = NULL; if (t) ID3D11Device_CreateUnorderedAccessView(hse.dev, (ID3D11Resource *)t, NULL, &u); return u; }
static void typed_texture_roundtrip(DXGI_FORMAT fmt, const char *hlslType, const char *conv_in, const char *conv_out, const uint32_t *in_bits, const uint32_t *want_bits) {
    HSE_RUN();
    uint32_t px_in[16];
    for (int i = 0; i < 16; i++) px_in[i] = in_bits[i];
    D3D11_SUBRESOURCE_DATA sd = { px_in, 16, 0 };
    ID3D11Texture2D *t = hse_tex2d(4, 4, 1, 1, fmt, D3D11_BIND_UNORDERED_ACCESS, &sd);
    ID3D11Texture2D *t2 = hse_tex2d(4, 4, 1, 1, fmt, D3D11_BIND_UNORDERED_ACCESS, NULL);
    ID3D11UnorderedAccessView *u = tex_uav(t), *u2 = tex_uav(t2);
    if (!u || !u2) { t_skip("UAV on format %d unavailable", fmt); SAFE_RELEASE(u); SAFE_RELEASE(u2); SAFE_RELEASE(t); SAFE_RELEASE(t2); return; }
    char src[1024];
    snprintf(src, sizeof src, "RWTexture2D<%s> A : register(u1); RWTexture2D<%s> B : register(u2); RWByteAddressBuffer O : register(u0);\n"
             "[numthreads(4,4,1)] void main(uint3 id : SV_DispatchThreadID) { %s v = A[id.xy]; B[id.xy] = %s; O.Store(id.y * 16 + id.x * 4, %s); }",
             hlslType, hlslType, hlslType, conv_in, conv_out);
    uint32_t out[16];
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = src; c.uav[0] = u; c.uav[1] = u2; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (HSE_DISPATCH(&c)) {
        for (int i = 0; i < 16; i++) CHECKF(out[i] == want_bits[i], "texel %d loaded %08x want %08x", i, out[i], want_bits[i]);
        d3dh_image im = { 0 };
        if (SUCCEEDED(d3dh_readback_texture2d11(hse.dev, hse.ctx, t2, 0, &im))) {
            for (int i = 0; i < 16; i++) CHECKF(d3dh_image_pixel32(&im, i % 4, i / 4) == in_bits[i], "stored texel %d = %08x want %08x", i, d3dh_image_pixel32(&im, i % 4, i / 4), in_bits[i]);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(u2); SAFE_RELEASE(t); SAFE_RELEASE(t2);
}
TEST(hlsl_semantics_exec_mem_typed_uav_roundtrip_r32_uint) {
    HSE_NEED();
    uint32_t v[16]; for (int i = 0; i < 16; i++) v[i] = 0xdead0000u + i * 0x111;
    typed_texture_roundtrip(DXGI_FORMAT_R32_UINT, "uint", "v", "v", v, v);
}
TEST(hlsl_semantics_exec_mem_typed_uav_roundtrip_r32_sint) {
    HSE_NEED();
    uint32_t v[16]; for (int i = 0; i < 16; i++) v[i] = (uint32_t)(-100000 * (i - 8));
    typed_texture_roundtrip(DXGI_FORMAT_R32_SINT, "int", "v", "asuint(v)", v, v);
}
TEST(hlsl_semantics_exec_mem_typed_uav_roundtrip_r32_float) {
    HSE_NEED();
    uint32_t v[16], w[16]; for (int i = 0; i < 16; i++) v[i] = w[i] = FB((float)i * -0.375f);
    typed_texture_roundtrip(DXGI_FORMAT_R32_FLOAT, "float", "v", "asuint(v)", v, w);
}
TEST(hlsl_semantics_exec_mem_typed_uav_buffer_load_store) {
    HSE_NEED();
    HSE_RUN();
    uint32_t init[16], out[17];
    for (int i = 0; i < 16; i++) init[i] = 9000 + i;
    ID3D11Buffer *b = hse_buf(sizeof init, D3D11_BIND_UNORDERED_ACCESS, 0, 0, init);
    ID3D11UnorderedAccessView *u = b ? hse_buf_uav(b, DXGI_FORMAT_R32_UINT, 0, 16, 0) : NULL;
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWBuffer<uint> B : register(u1); RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(16,1,1)] void main(uint gi : SV_GroupIndex) { uint v = B[gi]; B[gi] = v * 2 + 1; O.Store(gi * 4, v); uint n; B.GetDimensions(n); if (gi == 0) O.Store(64, n); }";
    c.uav[0] = u; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (u && HSE_DISPATCH(&c)) {
        for (int i = 0; i < 16; i++) EXPECT_EQ(out[i], 9000 + i);
        EXPECT_EQ(out[16], 16);
        uint32_t *w = NULL; size_t n = 0;
        if (hse_read_buf(b, (void **)&w, &n) && n >= 64) for (int i = 0; i < 16; i++) EXPECT_EQ(w[i], (9000 + i) * 2 + 1);
        free(w);
    }
    SAFE_RELEASE(u); SAFE_RELEASE(b);
}
TEST(hlsl_semantics_exec_mem_typed_uav_out_of_bounds_load_store) {
    HSE_NEED();
    HSE_RUN();
    uint32_t px_in[16], out[8];
    for (int i = 0; i < 16; i++) px_in[i] = 40 + i;
    D3D11_SUBRESOURCE_DATA sd = { px_in, 16, 0 };
    ID3D11Texture2D *t = hse_tex2d(4, 4, 1, 1, DXGI_FORMAT_R32_UINT, D3D11_BIND_UNORDERED_ACCESS, &sd);
    ID3D11UnorderedAccessView *u = tex_uav(t);
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWTexture2D<uint> T : register(u1); RWByteAddressBuffer O : register(u0);\n"
            "[numthreads(1,1,1)] void main() { O.Store4(0, uint4(T[uint2(4, 0)], T[uint2(0, 4)], T[uint2(100, 100)], T[uint2(3, 3)])); T[uint2(4, 1)] = 1; T[uint2(1, 4)] = 2; T[uint2(0, 0)] = 7; }";
    c.uav[0] = u; c.outbytes = sizeof out; c.gx = 1; c.out = out;
    if (u && HSE_DISPATCH(&c)) {
        EXPECT_EQ(out[0], 0); EXPECT_EQ(out[1], 0); EXPECT_EQ(out[2], 0); EXPECT_EQ(out[3], 55);
        d3dh_image im = { 0 };
        if (SUCCEEDED(d3dh_readback_texture2d11(hse.dev, hse.ctx, t, 0, &im))) {
            EXPECT_EQ(d3dh_image_pixel32(&im, 0, 0), 7);
            for (int i = 1; i < 16; i++) EXPECT_EQ(d3dh_image_pixel32(&im, i % 4, i / 4), px_in[i]);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(t);
}
TEST(hlsl_semantics_exec_mem_typed_uav_store_unorm_rounding) {
    HSE_NEED();
    HSE_RUN();
    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 s; memset(&s, 0, sizeof s); s.InFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    ID3D11Device_CheckFeatureSupport(hse.dev, D3D11_FEATURE_FORMAT_SUPPORT2, &s, sizeof s);
    if (!(s.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE)) { t_skip("R8G8B8A8_UNORM typed UAV store unsupported"); return; }
    ID3D11Texture2D *t = hse_tex2d(4, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_UNORDERED_ACCESS, NULL);
    ID3D11UnorderedAccessView *u = tex_uav(t);
    uint32_t dummy[4];
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = "RWTexture2D<unorm float4> T : register(u1);\n"
            "[numthreads(1,1,1)] void main() { T[uint2(0, 0)] = float4(1.0, 0.0, 0.5, 1.0); T[uint2(1, 0)] = float4(2.0, -1.0, 0.25, 0.75); T[uint2(2, 0)] = float4(0.0, 1.0 / 255.0, 254.0 / 255.0, 0.0); T[uint2(3, 0)] = float4(1.0 / 510.0, 3.0 / 510.0, 0.999, 0.001); }";
    c.uav[0] = u; c.outbytes = sizeof dummy; c.gx = 1; c.out = dummy;
    (void)dummy;
    if (u && HSE_DISPATCH(&c)) {
        d3dh_image im = { 0 };
        if (SUCCEEDED(d3dh_readback_texture2d11(hse.dev, hse.ctx, t, 0, &im))) {
            EXPECT_EQ(d3dh_image_pixel32(&im, 0, 0), 0xff8000ffu);       /* 0.5 -> 127.5 -> 128 (round to nearest even) */
            EXPECT_EQ(d3dh_image_pixel32(&im, 1, 0), 0xbf4000ffu);   /* r=255 (clamped) g=0 (clamped) b=64 a=191 */
            EXPECT_EQ(d3dh_image_pixel32(&im, 2, 0), 0x00fe0100u);
            t_note("texel 3 (1/510, 3/510, .999, .001) stored as %08x", d3dh_image_pixel32(&im, 3, 0));
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(t);
}
