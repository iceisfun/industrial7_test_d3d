/* Compute shader execution: thread ids, shared memory, atomics, buffers, counters, indirect dispatch, scalar ALU goldens. */
#include "rx.h"
#include <limits.h>

#define A(a) RX_N(a)

/* --------------------------------------------------------- helpers ---- */
static ID3D11Buffer *raw_buf(rx *r, UINT bytes, const void *init, UINT extra_misc)
{
    return rx_buffer(r, bytes, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS | extra_misc, 0, D3D11_USAGE_DEFAULT, 0, init);
}
static ID3D11UnorderedAccessView *uav_raw(rx *r, ID3D11Buffer *b, UINT bytes)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC d; ID3D11UnorderedAccessView *v = NULL;
    memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_R32_TYPELESS; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; d.Buffer.NumElements = bytes / 4; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r->dev, (ID3D11Resource *)b, &d, &v));
    return v;
}
static ID3D11UnorderedAccessView *uav_buf(rx *r, ID3D11Buffer *b, DXGI_FORMAT fmt, UINT n, UINT flags)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC d; ID3D11UnorderedAccessView *v = NULL;
    memset(&d, 0, sizeof d);
    d.Format = fmt; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; d.Buffer.NumElements = n; d.Buffer.Flags = flags;
    EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r->dev, (ID3D11Resource *)b, &d, &v));
    return v;
}
static ID3D11ShaderResourceView *srv_raw(rx *r, ID3D11Buffer *b, UINT bytes)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC d; ID3D11ShaderResourceView *v = NULL;
    memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_R32_TYPELESS; d.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX; d.BufferEx.NumElements = bytes / 4; d.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    EXPECT_HR_OK(ID3D11Device_CreateShaderResourceView(r->dev, (ID3D11Resource *)b, &d, &v));
    return v;
}
static ID3D11ShaderResourceView *srv_struct(rx *r, ID3D11Buffer *b, UINT n)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC d; ID3D11ShaderResourceView *v = NULL;
    memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_UNKNOWN; d.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; d.Buffer.NumElements = n;
    EXPECT_HR_OK(ID3D11Device_CreateShaderResourceView(r->dev, (ID3D11Resource *)b, &d, &v));
    return v;
}
static void cs_bind_uavs(rx *r, UINT n, ID3D11UnorderedAccessView *const *u, const UINT *counts)
{
    ID3D11DeviceContext_CSSetUnorderedAccessViews(r->ctx, 0, n, u, counts);
}
static uint32_t *rb(rx *r, ID3D11Buffer *b, size_t *sz)
{
    void *d = NULL; size_t n = 0;
    HRESULT hr = d3dh_readback_buffer11(r->dev, r->ctx, b, &d, &n);
    EXPECT_HR_OK(hr);
    if (sz) *sz = n;
    return SUCCEEDED(hr) ? (uint32_t *)d : NULL;
}
static void fill32(uint32_t *p, size_t n, uint32_t v) { for (size_t i = 0; i < n; i++) p[i] = v; }

/* ---------------------------------------------------- thread ids ---- */
static const char CS_IDS[] = HL(
    RWByteAddressBuffer o : register(u0);
    [numthreads(TX, TY, TZ)]
    void main(uint3 gid : SV_GroupID, uint3 gt : SV_GroupThreadID, uint3 dt : SV_DispatchThreadID, uint gi : SV_GroupIndex) {
        uint lin = (dt.z * (GY * TY) + dt.y) * (GX * TX) + dt.x;
        o.Store4(lin * 16, uint4(gid.x | (gid.y << 10) | (gid.z << 20), gt.x | (gt.y << 10) | (gt.z << 20), gi, lin + 1));
    });

static ID3D11ComputeShader *cs_macro(rx *r, const char *src, const D3D_SHADER_MACRO *defs)
{
    ID3DBlob *b = rx_blob_ex(defs, src, "cs_5_0", 0); ID3D11ComputeShader *s = NULL;
    if (!b) return NULL;
    EXPECT_HR_OK(ID3D11Device_CreateComputeShader(r->dev, d3dh_blob_ptr(b), d3dh_blob_size(b), NULL, &s));
    SAFE_RELEASE(b);
    return s;
}

TEST(d3d11_render_exec_cs_thread_ids_all_configurations)
{
    rx r; if (!rx_open(&r)) return;
    static const int cfg[][6] = {
        { 4, 2, 3, 2, 3, 2 }, { 1, 1, 1, 5, 4, 3 }, { 64, 1, 1, 7, 1, 1 }, { 1, 1, 64, 2, 2, 1 }, { 8, 8, 4, 2, 1, 2 },
        { 1024, 1, 1, 2, 1, 1 }, { 32, 32, 1, 1, 2, 1 }, { 16, 16, 4, 1, 1, 2 }, { 3, 5, 7, 3, 2, 2 }, { 1, 1024, 1, 1, 1, 2 }, { 2, 3, 1, 1, 1, 1 },
    };
    for (unsigned c = 0; c < A(cfg); c++) {
        char tx[16], ty[16], tz[16], gx[16], gy[16], gz[16];
        sprintf(tx, "%d", cfg[c][0]); sprintf(ty, "%d", cfg[c][1]); sprintf(tz, "%d", cfg[c][2]);
        sprintf(gx, "%d", cfg[c][3]); sprintf(gy, "%d", cfg[c][4]); sprintf(gz, "%d", cfg[c][5]);
        D3D_SHADER_MACRO defs[] = { { "TX", tx }, { "TY", ty }, { "TZ", tz }, { "GX", gx }, { "GY", gy }, { "GZ", gz }, { NULL, NULL } };
        int W = cfg[c][0] * cfg[c][3], H = cfg[c][1] * cfg[c][4], D = cfg[c][2] * cfg[c][5];
        UINT total = (UINT)(W * H * D), bytes = total * 16;
        ID3D11ComputeShader *cs = cs_macro(&r, CS_IDS, defs);
        uint32_t *init = (uint32_t *)malloc(bytes); fill32(init, total * 4, 0);
        ID3D11Buffer *buf = raw_buf(&r, bytes, init, 0);
        ID3D11UnorderedAccessView *u = buf ? uav_raw(&r, buf, bytes) : NULL;
        if (cs && u) {
            ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
            cs_bind_uavs(&r, 1, &u, NULL);
            ID3D11DeviceContext_Dispatch(r.ctx, cfg[c][3], cfg[c][4], cfg[c][5]);
            uint32_t *o = rb(&r, buf, NULL);
            if (o) {
                unsigned bad = 0;
                for (int z = 0; z < D; z++) for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                    unsigned lin = (z * H + y) * W + x;
                    int gxi = x / cfg[c][0], gyi = y / cfg[c][1], gzi = z / cfg[c][2];
                    int tx_ = x % cfg[c][0], ty_ = y % cfg[c][1], tz_ = z % cfg[c][2];
                    uint32_t w0 = gxi | (gyi << 10) | (gzi << 20), w1 = tx_ | (ty_ << 10) | (tz_ << 20);
                    uint32_t w2 = (tz_ * cfg[c][1] + ty_) * cfg[c][0] + tx_;
                    if (o[lin * 4] != w0 || o[lin * 4 + 1] != w1 || o[lin * 4 + 2] != w2 || o[lin * 4 + 3] != lin + 1) {
                        if (!bad) CHECKF(0, "config %u thread (%d,%d,%d): got %08x %08x %u %u want %08x %08x %u %u", c, x, y, z, o[lin * 4], o[lin * 4 + 1], o[lin * 4 + 2], o[lin * 4 + 3], w0, w1, w2, lin + 1);
                        bad++;
                    }
                }
                CHECKF(bad == 0, "config %u (numthreads %d,%d,%d dispatch %d,%d,%d): %u of %u threads wrong", c, cfg[c][0], cfg[c][1], cfg[c][2], cfg[c][3], cfg[c][4], cfg[c][5], bad, total);
                free(o);
            }
        }
        SAFE_RELEASE(u); SAFE_RELEASE(buf); SAFE_RELEASE(cs); free(init);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_dispatch_zero_dimensions_do_nothing)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(RWByteAddressBuffer o : register(u0); [numthreads(4, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { o.Store(d.x * 4, 7); });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    uint32_t init[16]; fill32(init, 16, 0xFFFFFFFFu);
    ID3D11Buffer *buf = raw_buf(&r, 64, init, 0);
    ID3D11UnorderedAccessView *u = buf ? uav_raw(&r, buf, 64) : NULL;
    if (cs && u) {
        static const UINT dims[][3] = { { 0, 1, 1 }, { 1, 0, 1 }, { 1, 1, 0 }, { 0, 0, 0 }, { 0, 5, 5 } };
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        for (unsigned i = 0; i < A(dims); i++) ID3D11DeviceContext_Dispatch(r.ctx, dims[i][0], dims[i][1], dims[i][2]);
        uint32_t *o = rb(&r, buf, NULL);
        if (o) { for (int i = 0; i < 16; i++) CHECKF(o[i] == 0xFFFFFFFFu, "dword %d written by an empty dispatch: %08x", i, o[i]); free(o); }
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        o = rb(&r, buf, NULL);
        if (o) { CHECK(o[0] == 7 && o[3] == 7 && o[4] == 0xFFFFFFFFu); free(o); }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(buf); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_max_group_count_65535)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWByteAddressBuffer o : register(u0);
        [numthreads(1, 1, 1)] void main(uint3 g : SV_GroupID) { uint old; o.InterlockedAdd(0, 1, old); o.InterlockedMax(4, g.x); o.InterlockedMax(8, g.y); o.InterlockedMax(12, g.z); });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    uint32_t init[4] = { 0, 0, 0, 0 };
    ID3D11Buffer *buf = raw_buf(&r, 16, init, 0);
    ID3D11UnorderedAccessView *u = buf ? uav_raw(&r, buf, 16) : NULL;
    if (cs && u) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 65535, 1, 1);
        uint32_t *o = rb(&r, buf, NULL);
        if (o) { CHECKF(o[0] == 65535 && o[1] == 65534 && o[2] == 0 && o[3] == 0, "count %u max gid %u %u %u", o[0], o[1], o[2], o[3]); free(o); }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(buf); SAFE_RELEASE(cs);
    rx_close(&r);
}

/* -------------------------------------------------- shared memory ---- */
TEST(d3d11_render_exec_cs_groupshared_reverse_and_atomic_sum)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWByteAddressBuffer o : register(u0);
        groupshared uint sh[64]; groupshared uint tot;
        [numthreads(64, 1, 1)]
        void main(uint3 gid : SV_GroupID, uint gi : SV_GroupIndex) {
            sh[gi] = gi * 3 + gid.x;
            GroupMemoryBarrierWithGroupSync();
            uint v = sh[63 - gi];
            o.Store((gid.x * 64 + gi) * 4, v);
            if (gi == 0) tot = 0;
            GroupMemoryBarrierWithGroupSync();
            InterlockedAdd(tot, gi + 1);
            GroupMemoryBarrierWithGroupSync();
            if (gi == 0) o.Store(4096 + gid.x * 4, tot);
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    ID3D11Buffer *buf = raw_buf(&r, 8192, NULL, 0);
    ID3D11UnorderedAccessView *u = buf ? uav_raw(&r, buf, 8192) : NULL;
    if (cs && u) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 9, 1, 1);
        uint32_t *o = rb(&r, buf, NULL);
        if (o) {
            unsigned bad = 0;
            for (unsigned g = 0; g < 9; g++) {
                for (unsigned i = 0; i < 64; i++) if (o[g * 64 + i] != (63 - i) * 3 + g) bad++;
                CHECKF(o[1024 + g] == 2080, "group %u: atomic sum %u want 2080", g, o[1024 + g]);
            }
            CHECKF(bad == 0, "%u reversed shared-memory values wrong", bad);
            free(o);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(buf); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_groupshared_visible_across_barrier_multiple_phases)
{
    rx r; if (!rx_open(&r)) return;
    /* parallel prefix sum (Hillis-Steele) over 128 elements in shared memory: exercises repeated barriers */
    static const char src[] = HL(
        RWByteAddressBuffer o : register(u0);
        groupshared uint a[128];
        [numthreads(128, 1, 1)]
        void main(uint gi : SV_GroupIndex) {
            a[gi] = gi + 1;
            GroupMemoryBarrierWithGroupSync();
            for (uint off = 1; off < 128; off <<= 1) {
                uint v = a[gi];
                if (gi >= off) v += a[gi - off];
                GroupMemoryBarrierWithGroupSync();
                a[gi] = v;
                GroupMemoryBarrierWithGroupSync();
            }
            o.Store(gi * 4, a[gi]);
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    ID3D11Buffer *buf = raw_buf(&r, 512, NULL, 0);
    ID3D11UnorderedAccessView *u = buf ? uav_raw(&r, buf, 512) : NULL;
    if (cs && u) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        uint32_t *o = rb(&r, buf, NULL);
        if (o) { unsigned bad = 0; for (unsigned i = 0; i < 128; i++) if (o[i] != (i + 1) * (i + 2) / 2) bad++; CHECKF(bad == 0, "%u prefix sums wrong", bad); free(o); }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(buf); SAFE_RELEASE(cs);
    rx_close(&r);
}

/* -------------------------------------------------------- atomics ---- */
TEST(d3d11_render_exec_cs_atomic_operations_suite)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWByteAddressBuffer o : register(u0);
        RWStructuredBuffer<int> s : register(u1);
        [numthreads(64, 1, 1)]
        void main(uint3 dt : SV_DispatchThreadID) {
            uint i = dt.x, orig, ow, xo;
            o.InterlockedAdd(0, 1, orig);
            o.InterlockedAdd(64 + orig * 4, 1);
            o.InterlockedOr(4, 1u << (i & 31));
            o.InterlockedAnd(8, ~(1u << (i & 31)));
            o.InterlockedXor(12, i * 2654435761u);
            o.InterlockedMax(16, i);
            o.InterlockedMin(20, i + 5);
            o.InterlockedCompareExchange(32, 0, i + 1, ow);
            if (ow == 0) o.InterlockedAdd(36, 1);
            o.InterlockedExchange(40, i + 1, xo);
            o.InterlockedAdd(44, xo);
            o.InterlockedAdd(48, i);
            InterlockedMax(s[0], (int)i - 100);
            InterlockedMin(s[1], (int)i - 100);
            InterlockedAdd(s[2], (int)i - 100);
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    enum { N = 256 };
    uint32_t init[16 + N]; fill32(init, 16 + N, 0);
    init[2] = 0xFFFFFFFFu; init[5] = 0xFFFFFFFFu;
    int32_t sinit[4] = { INT_MIN, INT_MAX, 0, 0 };
    ID3D11Buffer *buf = raw_buf(&r, sizeof init, init, 0);
    ID3D11Buffer *sbuf = rx_buffer(&r, sizeof sinit, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, D3D11_USAGE_DEFAULT, 0, sinit);
    ID3D11UnorderedAccessView *u[2];
    u[0] = buf ? uav_raw(&r, buf, sizeof init) : NULL;
    u[1] = sbuf ? uav_buf(&r, sbuf, DXGI_FORMAT_UNKNOWN, 4, 0) : NULL;
    if (cs && u[0] && u[1]) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 2, u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, N / 64, 1, 1);
        uint32_t *o = rb(&r, buf, NULL);
        int32_t *so = (int32_t *)rb(&r, sbuf, NULL);
        if (o && so) {
            uint32_t xr = 0; unsigned bad = 0;
            for (uint32_t i = 0; i < N; i++) xr ^= i * 2654435761u;
            CHECKF(o[0] == N, "InterlockedAdd counter %u", o[0]);
            CHECKF(o[1] == 0xFFFFFFFFu, "InterlockedOr %08x", o[1]);
            CHECKF(o[2] == 0, "InterlockedAnd %08x", o[2]);
            CHECKF(o[3] == xr, "InterlockedXor %08x want %08x", o[3], xr);
            CHECKF(o[4] == N - 1, "InterlockedMax %u", o[4]);
            CHECKF(o[5] == 5, "InterlockedMin %u", o[5]);
            CHECKF(o[9] == 1, "CompareExchange winners %u", o[9]);
            CHECKF(o[8] >= 1 && o[8] <= N, "CompareExchange stored value %u", o[8]);
            CHECKF(o[10] + o[11] == N * (N + 1) / 2, "InterlockedExchange conservation: final %u + sum of originals %u != %u", o[10], o[11], N * (N + 1) / 2);
            CHECKF(o[12] == N * (N - 1) / 2, "InterlockedAdd sum %u", o[12]);
            for (unsigned i = 0; i < N; i++) if (o[16 + i] != 1) bad++;
            CHECKF(bad == 0, "%u of %d returned original values were not unique", bad, N);
            CHECKF(so[0] == N - 1 - 100, "signed InterlockedMax %d", so[0]);
            CHECKF(so[1] == -100, "signed InterlockedMin %d", so[1]);
            CHECKF(so[2] == (N * (N - 1) / 2) - 100 * N, "signed InterlockedAdd %d", so[2]);
        }
        free(o); free(so);
    }
    SAFE_RELEASE(u[0]); SAFE_RELEASE(u[1]); SAFE_RELEASE(buf); SAFE_RELEASE(sbuf); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_interlocked_compare_store_and_original_values)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWByteAddressBuffer o : register(u0);
        [numthreads(1, 1, 1)]
        void main() {
            uint a, b, c, d, e, f;
            o.InterlockedCompareExchange(0, 5, 9, a);    // 5 -> 9, original 5
            o.InterlockedCompareExchange(0, 5, 1, b);    // mismatch, original 9
            o.InterlockedCompareStore(4, 3, 8);          // 3 -> 8
            o.InterlockedCompareStore(4, 3, 2);          // mismatch, stays 8
            o.InterlockedExchange(8, 77, c);             // original 11
            o.InterlockedAdd(12, 5, d);                  // original 100
            o.InterlockedOr(16, 0xF0, e);                // original 0x0F
            o.InterlockedXor(20, 0xFF, f);               // original 0xAA
            o.Store(32, a); o.Store(36, b); o.Store(40, c); o.Store(44, d); o.Store(48, e); o.Store(52, f);
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    uint32_t init[16] = { 5, 3, 11, 100, 0x0F, 0xAA, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    ID3D11Buffer *buf = raw_buf(&r, 64, init, 0);
    ID3D11UnorderedAccessView *u = buf ? uav_raw(&r, buf, 64) : NULL;
    if (cs && u) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        uint32_t *o = rb(&r, buf, NULL);
        if (o) {
            static const uint32_t want[16] = { 9, 8, 77, 105, 0xFF, 0x55, 0, 0, 5, 9, 11, 100, 0x0F, 0xAA, 0, 0 };
            for (int i = 0; i < 14; i++) CHECKF(o[i] == want[i], "dword %d = %08x want %08x", i, o[i], want[i]);
            free(o);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(buf); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_groupshared_atomics_min_max_or)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWByteAddressBuffer o : register(u0);
        groupshared uint mx; groupshared uint mn; groupshared uint bits; groupshared uint ow;
        [numthreads(96, 1, 1)]
        void main(uint3 gid : SV_GroupID, uint gi : SV_GroupIndex) {
            if (gi == 0) { mx = 0; mn = 0xFFFFFFFF; bits = 0; ow = 0; }
            GroupMemoryBarrierWithGroupSync();
            uint orig;
            InterlockedMax(mx, gi * 7 + gid.x);
            InterlockedMin(mn, gi * 7 + gid.x + 3);
            InterlockedOr(bits, 1u << (gi % 32));
            InterlockedExchange(ow, gi + 1, orig);
            GroupMemoryBarrierWithGroupSync();
            if (gi == 0) { o.Store(gid.x * 16, mx); o.Store(gid.x * 16 + 4, mn); o.Store(gid.x * 16 + 8, bits); o.Store(gid.x * 16 + 12, ow); }
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    ID3D11Buffer *buf = raw_buf(&r, 64, NULL, 0);
    ID3D11UnorderedAccessView *u = buf ? uav_raw(&r, buf, 64) : NULL;
    if (cs && u) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 4, 1, 1);
        uint32_t *o = rb(&r, buf, NULL);
        if (o) {
            for (unsigned g = 0; g < 4; g++) {
                CHECKF(o[g * 4] == 95 * 7 + g, "group %u max %u", g, o[g * 4]);
                CHECKF(o[g * 4 + 1] == g + 3, "group %u min %u", g, o[g * 4 + 1]);
                CHECKF(o[g * 4 + 2] == 0xFFFFFFFFu, "group %u or %08x", g, o[g * 4 + 2]);
                CHECKF(o[g * 4 + 3] >= 1 && o[g * 4 + 3] <= 96, "group %u exchange final %u", g, o[g * 4 + 3]);
            }
            free(o);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(buf); SAFE_RELEASE(cs);
    rx_close(&r);
}

/* --------------------------------------------------------- buffers ---- */
TEST(d3d11_render_exec_cs_structured_scale_bias_with_bounds_guard)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        struct E { float4 v; uint id; float3 pad; };
        StructuredBuffer<E> src : register(t0);
        RWStructuredBuffer<E> dst : register(u0);
        cbuffer P : register(b0) { float scale; float bias; uint count; uint pad; };
        [numthreads(16, 1, 1)]
        void main(uint3 d : SV_DispatchThreadID) {
            if (d.x >= count) return;
            E e = src[d.x];
            e.v = e.v * scale + bias;
            e.id = d.x;
            dst[d.x] = e;
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    enum { N = 48, COUNT = 37 };
    float in[N * 8], out_init[N * 8];
    for (int i = 0; i < N; i++) {
        float *e = in + i * 8;
        e[0] = (float)i; e[1] = i + 0.25f; e[2] = -(float)i; e[3] = 1; ((uint32_t *)e)[4] = 0xABCD0000u + i; e[5] = 7; e[6] = 8; e[7] = 9;
    }
    for (int i = 0; i < N * 8; i++) ((uint32_t *)out_init)[i] = 0xCAFEF00Du;
    ID3D11Buffer *ib = rx_buffer(&r, sizeof in, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 32, D3D11_USAGE_DEFAULT, 0, in);
    ID3D11Buffer *ob = rx_buffer(&r, sizeof out_init, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 32, D3D11_USAGE_DEFAULT, 0, out_init);
    ID3D11Buffer *cb = rx_cbuf(&r, 16);
    ID3D11ShaderResourceView *sv = ib ? srv_struct(&r, ib, N) : NULL;
    ID3D11UnorderedAccessView *uv = ob ? uav_buf(&r, ob, DXGI_FORMAT_UNKNOWN, N, 0) : NULL;
    if (cs && sv && uv && cb) {
        struct { float s, b; uint32_t c, p; } pc = { 2.0f, 0.5f, COUNT, 0 };
        ID3D11DeviceContext_UpdateSubresource(r.ctx, (ID3D11Resource *)cb, 0, NULL, &pc, 0, 0);
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetConstantBuffers(r.ctx, 0, 1, &cb);
        ID3D11DeviceContext_CSSetShaderResources(r.ctx, 0, 1, &sv);
        cs_bind_uavs(&r, 1, &uv, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, (N + 15) / 16, 1, 1);
        float *o = (float *)rb(&r, ob, NULL);
        if (o) {
            unsigned bad = 0;
            for (int i = 0; i < N; i++) {
                const float *e = o + i * 8; const uint32_t *u = (const uint32_t *)e;
                if (i < COUNT) {
                    if (e[0] != i * 2.0f + 0.5f || e[1] != (i + 0.25f) * 2.0f + 0.5f || e[2] != -(float)i * 2.0f + 0.5f || e[3] != 2.5f || u[4] != (uint32_t)i || e[5] != 7 || e[6] != 8 || e[7] != 9) bad++;
                } else for (int k = 0; k < 8; k++) if (u[k] != 0xCAFEF00Du) { bad++; break; }
            }
            CHECKF(bad == 0, "%d structured elements wrong", bad);
            free(o);
        }
    }
    SAFE_RELEASE(sv); SAFE_RELEASE(uv); SAFE_RELEASE(ib); SAFE_RELEASE(ob); SAFE_RELEASE(cb); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_structured_stride_12_and_20)
{
    rx r; if (!rx_open(&r)) return;
    static const char s12[] = HL(RWStructuredBuffer<float3> b : register(u0); [numthreads(8, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { b[d.x] = float3(d.x, d.x * 2, d.x * 3); });
    static const char s20[] = HL(
        struct S { uint a; float b; uint2 c; uint d; };
        RWStructuredBuffer<S> b : register(u0);
        [numthreads(8, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { S s; s.a = d.x; s.b = d.x + 0.5; s.c = uint2(d.x * 10, d.x * 100); s.d = 0xF0000000u | d.x; b[d.x] = s; });
    ID3D11ComputeShader *c12 = rx_cs(&r, s12), *c20 = rx_cs(&r, s20);
    ID3D11Buffer *b12 = rx_buffer(&r, 12 * 16, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 12, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11Buffer *b20 = rx_buffer(&r, 20 * 16, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 20, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11UnorderedAccessView *u12 = b12 ? uav_buf(&r, b12, DXGI_FORMAT_UNKNOWN, 16, 0) : NULL, *u20 = b20 ? uav_buf(&r, b20, DXGI_FORMAT_UNKNOWN, 16, 0) : NULL;
    if (c12 && c20 && u12 && u20) {
        ID3D11DeviceContext_CSSetShader(r.ctx, c12, NULL, 0);
        cs_bind_uavs(&r, 1, &u12, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 2, 1, 1);
        float *o = (float *)rb(&r, b12, NULL);
        if (o) { unsigned bad = 0; for (int i = 0; i < 16; i++) if (o[i * 3] != i || o[i * 3 + 1] != i * 2 || o[i * 3 + 2] != i * 3) bad++; CHECKF(bad == 0, "stride-12 elements wrong: %u", bad); free(o); }
        ID3D11DeviceContext_CSSetShader(r.ctx, c20, NULL, 0);
        cs_bind_uavs(&r, 1, &u20, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 2, 1, 1);
        uint32_t *w = rb(&r, b20, NULL);
        if (w) {
            unsigned bad = 0;
            for (int i = 0; i < 16; i++) {
                float f; memcpy(&f, &w[i * 5 + 1], 4);
                if (w[i * 5] != (uint32_t)i || f != i + 0.5f || w[i * 5 + 2] != (uint32_t)i * 10 || w[i * 5 + 3] != (uint32_t)i * 100 || w[i * 5 + 4] != (0xF0000000u | i)) bad++;
            }
            CHECKF(bad == 0, "stride-20 elements wrong: %u", bad);
            free(w);
        }
    }
    SAFE_RELEASE(u12); SAFE_RELEASE(u20); SAFE_RELEASE(b12); SAFE_RELEASE(b20); SAFE_RELEASE(c12); SAFE_RELEASE(c20);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_raw_buffer_load_store_widths)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        ByteAddressBuffer inb : register(t0);
        RWByteAddressBuffer o : register(u0);
        [numthreads(1, 1, 1)]
        void main() {
            o.Store(0, inb.Load(4));
            o.Store2(8, inb.Load2(8) ^ uint2(0xFFFFFFFF, 0));
            o.Store3(16, inb.Load3(16) + uint3(1, 2, 3));
            o.Store4(32, inb.Load4(32).wzyx);
            o.Store4(48, uint4(inb.Load(0), inb.Load(60), inb.Load(124), inb.Load(1020 - 4)));
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    uint32_t in[256]; for (int i = 0; i < 256; i++) in[i] = 0x01010101u * (i & 255) + i;
    ID3D11Buffer *ib = raw_buf(&r, sizeof in, in, 0);
    ID3D11Buffer *ob = raw_buf(&r, 1024, NULL, 0);
    ID3D11ShaderResourceView *sv = ib ? srv_raw(&r, ib, sizeof in) : NULL;
    ID3D11UnorderedAccessView *uv = ob ? uav_raw(&r, ob, 1024) : NULL;
    if (cs && sv && uv) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetShaderResources(r.ctx, 0, 1, &sv);
        cs_bind_uavs(&r, 1, &uv, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        uint32_t *o = rb(&r, ob, NULL);
        if (o) {
            CHECKF(o[0] == in[1], "Load/Store %08x", o[0]);
            CHECKF(o[2] == (in[2] ^ 0xFFFFFFFFu) && o[3] == in[3], "Load2/Store2 %08x %08x", o[2], o[3]);
            CHECKF(o[4] == in[4] + 1 && o[5] == in[5] + 2 && o[6] == in[6] + 3, "Load3/Store3 %08x %08x %08x", o[4], o[5], o[6]);
            CHECKF(o[8] == in[11] && o[9] == in[10] && o[10] == in[9] && o[11] == in[8], "Load4 swizzle");
            CHECKF(o[12] == in[0] && o[13] == in[15] && o[14] == in[31] && o[15] == in[255 - 1 + 0] , "scattered Loads %08x %08x %08x %08x", o[12], o[13], o[14], o[15]);
            free(o);
        }
    }
    SAFE_RELEASE(sv); SAFE_RELEASE(uv); SAFE_RELEASE(ib); SAFE_RELEASE(ob); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_typed_buffer_uav_formats)
{
    rx r; if (!rx_open(&r)) return;
    static const char su[] = HL(RWBuffer<uint> b : register(u0); [numthreads(8, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { b[d.x] = d.x * 0x01000193u + 7; });
    static const char sf[] = HL(RWBuffer<float> b : register(u0); [numthreads(8, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { b[d.x] = d.x * 0.5 - 1.0; });
    static const char s4[] = HL(RWBuffer<uint4> b : register(u0); [numthreads(8, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { b[d.x] = uint4(d.x, d.x + 1, d.x + 2, d.x + 3); });
    static const char si[] = HL(RWBuffer<int> b : register(u0); [numthreads(8, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { b[d.x] = (int)d.x * -3; });
    static const struct { const char *src; DXGI_FORMAT f; UINT elem; } tab[] = {
        { su, DXGI_FORMAT_R32_UINT, 4 }, { sf, DXGI_FORMAT_R32_FLOAT, 4 }, { s4, DXGI_FORMAT_R32G32B32A32_UINT, 16 }, { si, DXGI_FORMAT_R32_SINT, 4 },
    };
    for (unsigned t = 0; t < A(tab); t++) {
        UINT sup = 0;
        ID3D11Device_CheckFormatSupport(r.dev, tab[t].f, &sup);
        if (!(sup & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW)) { t_note("typed UAV format %d unsupported", (int)tab[t].f); continue; }
        ID3D11ComputeShader *cs = rx_cs(&r, tab[t].src);
        ID3D11Buffer *b = rx_buffer(&r, tab[t].elem * 16, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, 0, 0, D3D11_USAGE_DEFAULT, 0, NULL);
        ID3D11UnorderedAccessView *u = b ? uav_buf(&r, b, tab[t].f, 16, 0) : NULL;
        if (cs && u) {
            ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
            cs_bind_uavs(&r, 1, &u, NULL);
            ID3D11DeviceContext_Dispatch(r.ctx, 2, 1, 1);
            uint32_t *o = rb(&r, b, NULL);
            if (o) {
                unsigned bad = 0;
                for (int i = 0; i < 16; i++) {
                    if (t == 0 && o[i] != (uint32_t)i * 0x01000193u + 7) bad++;
                    if (t == 1) { float f; memcpy(&f, &o[i], 4); if (f != i * 0.5f - 1.0f) bad++; }
                    if (t == 2 && (o[i * 4] != (uint32_t)i || o[i * 4 + 1] != (uint32_t)i + 1 || o[i * 4 + 2] != (uint32_t)i + 2 || o[i * 4 + 3] != (uint32_t)i + 3)) bad++;
                    if (t == 3 && (int32_t)o[i] != i * -3) bad++;
                }
                CHECKF(bad == 0, "typed UAV format %d: %u wrong elements", (int)tab[t].f, bad);
                free(o);
            }
        }
        SAFE_RELEASE(u); SAFE_RELEASE(b); SAFE_RELEASE(cs);
    }
    rx_close(&r);
}

/* --------------------------------------------------- texture UAVs ---- */
TEST(d3d11_render_exec_cs_texture2d_uav_write_exact)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWTexture2D<uint> t : register(u0);
        [numthreads(4, 4, 1)] void main(uint3 d : SV_DispatchThreadID) { t[d.xy] = d.x * 100 + d.y; });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    ID3D11Texture2D *tx = rx_tex2d(&r, 12, 8, 1, 1, DXGI_FORMAT_R32_UINT, 1, D3D11_BIND_UNORDERED_ACCESS, 0, NULL);
    ID3D11UnorderedAccessView *u = NULL;
    if (tx) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_UINT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r.dev, (ID3D11Resource *)tx, &d, &u));
    }
    if (cs && u) {
        d3dh_image im;
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 3, 2, 1);
        if (rx_read(&r, tx, 0, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 12; x++) if (d3dh_image_pixel32(&im, x, y) != x * 100 + y) bad++;
            CHECKF(bad == 0, "%u texels wrong", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(tx); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_texture2d_array_and_mip_uav_slices)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWTexture2DArray<float4> t : register(u0);
        [numthreads(4, 4, 1)] void main(uint3 d : SV_DispatchThreadID) { t[d] = float4(d.x, d.y, d.z, 1); });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    UINT sup = 0;
    ID3D11Device_CheckFormatSupport(r.dev, DXGI_FORMAT_R32G32B32A32_FLOAT, &sup);
    ID3D11Texture2D *tx = (sup & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) ? rx_tex2d(&r, 4, 4, 2, 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_BIND_UNORDERED_ACCESS, 0, NULL) : NULL;
    ID3D11UnorderedAccessView *u = NULL;
    if (!tx) t_note("typed UAV on R32G32B32A32_FLOAT unsupported");
    if (tx) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
        d.Texture2DArray.MipSlice = 0; d.Texture2DArray.FirstArraySlice = 0; d.Texture2DArray.ArraySize = 3;
        EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r.dev, (ID3D11Resource *)tx, &d, &u));
    }
    if (cs && u) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 3);
        for (UINT s = 0; s < 3; s++) {
            d3dh_image im;
            if (!rx_read(&r, tx, s * 2, &im)) continue;      /* mip 0 of slice s: subresource = mip + slice * mips */
            unsigned bad = 0;
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) if (rx_f32(&im, x, y, 0) != x || rx_f32(&im, x, y, 1) != y || rx_f32(&im, x, y, 2) != s || rx_f32(&im, x, y, 3) != 1) bad++;
            CHECKF(bad == 0, "slice %u: %u texels wrong", s, bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(tx); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_texture3d_uav_write_exact)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWTexture3D<uint> t : register(u0);
        [numthreads(2, 2, 2)] void main(uint3 d : SV_DispatchThreadID) { t[d] = d.x + d.y * 10 + d.z * 100; });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    D3D11_TEXTURE3D_DESC td; ID3D11Texture3D *tx = NULL, *st = NULL; ID3D11UnorderedAccessView *u = NULL;
    memset(&td, 0, sizeof td);
    td.Width = td.Height = td.Depth = 4; td.MipLevels = 1; td.Format = DXGI_FORMAT_R32_UINT; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    EXPECT_HR_OK(ID3D11Device_CreateTexture3D(r.dev, &td, NULL, &tx));
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    EXPECT_HR_OK(ID3D11Device_CreateTexture3D(r.dev, &td, NULL, &st));
    if (tx) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_UINT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D; d.Texture3D.WSize = 4;
        EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r.dev, (ID3D11Resource *)tx, &d, &u));
    }
    if (cs && u && st) {
        D3D11_MAPPED_SUBRESOURCE m;
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 2, 2, 2);
        ID3D11DeviceContext_CopyResource(r.ctx, (ID3D11Resource *)st, (ID3D11Resource *)tx);
        if (SUCCEEDED(ID3D11DeviceContext_Map(r.ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m))) {
            unsigned bad = 0;
            for (int z = 0; z < 4; z++) for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
                uint32_t v = *(const uint32_t *)((const char *)m.pData + (size_t)z * m.DepthPitch + (size_t)y * m.RowPitch + x * 4);
                if (v != (uint32_t)(x + y * 10 + z * 100)) bad++;
            }
            CHECKF(bad == 0, "%u voxels wrong", bad);
            ID3D11DeviceContext_Unmap(r.ctx, (ID3D11Resource *)st, 0);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(tx); SAFE_RELEASE(st); SAFE_RELEASE(cs);
    rx_close(&r);
}

/* ------------------------------------------------ append / counters ---- */
TEST(d3d11_render_exec_cs_append_buffer_counts_and_initial_count)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        AppendStructuredBuffer<uint> a : register(u0);
        [numthreads(32, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { a.Append(d.x + 1000); });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    uint32_t init[512]; fill32(init, 512, 0xEEEEEEEEu);
    ID3D11Buffer *b = rx_buffer(&r, sizeof init, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, D3D11_USAGE_DEFAULT, 0, init);
    ID3D11Buffer *cnt = rx_buffer(&r, 16, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11UnorderedAccessView *u = b ? uav_buf(&r, b, DXGI_FORMAT_UNKNOWN, 512, D3D11_BUFFER_UAV_FLAG_APPEND) : NULL;
    if (cs && u && cnt) {
        UINT c0 = 10, keep = 0xFFFFFFFFu;
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, &c0);
        ID3D11DeviceContext_Dispatch(r.ctx, 4, 1, 1);          /* 128 appends starting at slot 10 */
        ID3D11DeviceContext_CopyStructureCount(r.ctx, cnt, 0, u);
        cs_bind_uavs(&r, 1, &u, &keep);                         /* -1 keeps the running counter */
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);          /* 32 more */
        ID3D11DeviceContext_CopyStructureCount(r.ctx, cnt, 4, u);
        uint32_t *c = rb(&r, cnt, NULL), *o = rb(&r, b, NULL);
        if (c && o) {
            CHECKF(c[0] == 138, "counter after first dispatch %u want 138", c[0]);
            CHECKF(c[1] == 170, "counter after second dispatch (initial count -1) %u want 170", c[1]);
            unsigned seen[32 * 4] = { 0 }, bad = 0, sentinel_bad = 0;
            for (int i = 0; i < 10; i++) if (o[i] != 0xEEEEEEEEu) sentinel_bad++;
            for (int i = 170; i < 512; i++) if (o[i] != 0xEEEEEEEEu) sentinel_bad++;
            for (int i = 10; i < 170; i++) { if (o[i] < 1000 || o[i] >= 1128) bad++; else seen[o[i] - 1000]++; }
            for (int i = 0; i < 128; i++) { unsigned want = i < 32 ? 2 : 1; if (seen[i] != want) bad++; }
            CHECKF(sentinel_bad == 0, "%u elements outside the appended range were modified", sentinel_bad);
            CHECKF(bad == 0, "%u appended values missing or duplicated", bad);
        }
        free(c); free(o);
    }
    SAFE_RELEASE(u); SAFE_RELEASE(b); SAFE_RELEASE(cnt); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_consume_buffer_drains_counter)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        ConsumeStructuredBuffer<uint> c : register(u0);
        RWByteAddressBuffer o : register(u1);
        [numthreads(50, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { uint v = c.Consume(); o.InterlockedAdd(0, v); o.InterlockedAdd(4, 1); });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    uint32_t data[100]; for (int i = 0; i < 100; i++) data[i] = i;
    uint32_t zero[4] = { 0, 0, 0, 0 };
    ID3D11Buffer *b = rx_buffer(&r, sizeof data, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, D3D11_USAGE_DEFAULT, 0, data);
    ID3D11Buffer *ob = raw_buf(&r, 16, zero, 0);
    ID3D11Buffer *cnt = rx_buffer(&r, 4, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11UnorderedAccessView *u[2];
    u[0] = b ? uav_buf(&r, b, DXGI_FORMAT_UNKNOWN, 100, D3D11_BUFFER_UAV_FLAG_APPEND) : NULL;
    u[1] = ob ? uav_raw(&r, ob, 16) : NULL;
    if (cs && u[0] && u[1] && cnt) {
        UINT counts[2] = { 100, 0xFFFFFFFFu };
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 2, u, counts);
        ID3D11DeviceContext_Dispatch(r.ctx, 2, 1, 1);
        ID3D11DeviceContext_CopyStructureCount(r.ctx, cnt, 0, u[0]);
        uint32_t *c = rb(&r, cnt, NULL), *o = rb(&r, ob, NULL);
        if (c && o) {
            CHECKF(c[0] == 0, "counter after consuming everything: %u", c[0]);
            CHECKF(o[0] == 4950, "sum of consumed values %u want 4950", o[0]);
            CHECKF(o[1] == 100, "consume count %u", o[1]);
        }
        free(c); free(o);
    }
    SAFE_RELEASE(u[0]); SAFE_RELEASE(u[1]); SAFE_RELEASE(b); SAFE_RELEASE(ob); SAFE_RELEASE(cnt); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_uav_counter_increment_decrement)
{
    rx r; if (!rx_open(&r)) return;
    static const char sinc[] = HL(
        RWStructuredBuffer<uint> b : register(u0);
        [numthreads(64, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { uint k = b.IncrementCounter(); b[k] = d.x + 1; });
    static const char sdec[] = HL(
        RWStructuredBuffer<uint> b : register(u0);
        RWByteAddressBuffer o : register(u1);
        [numthreads(64, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { uint k = b.DecrementCounter(); o.InterlockedAdd(0, k); o.InterlockedOr(4, 1u << (k & 31)); });
    ID3D11ComputeShader *ci = rx_cs(&r, sinc), *cd = rx_cs(&r, sdec);
    uint32_t zero[256] = { 0 };
    ID3D11Buffer *b = rx_buffer(&r, 256, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, D3D11_USAGE_DEFAULT, 0, zero);
    ID3D11Buffer *ob = raw_buf(&r, 16, zero, 0);
    ID3D11Buffer *cnt = rx_buffer(&r, 8, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11UnorderedAccessView *u[2];
    u[0] = b ? uav_buf(&r, b, DXGI_FORMAT_UNKNOWN, 64, D3D11_BUFFER_UAV_FLAG_COUNTER) : NULL;
    u[1] = ob ? uav_raw(&r, ob, 16) : NULL;
    if (ci && cd && u[0] && u[1] && cnt) {
        UINT counts[2] = { 0, 0xFFFFFFFFu };
        ID3D11DeviceContext_CSSetShader(r.ctx, ci, NULL, 0);
        cs_bind_uavs(&r, 1, u, counts);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        ID3D11DeviceContext_CopyStructureCount(r.ctx, cnt, 0, u[0]);
        uint32_t *c = rb(&r, cnt, NULL), *o = rb(&r, b, NULL);
        if (c && o) {
            unsigned seen = 0; uint64_t mask = 0;
            CHECKF(c[0] == 64, "counter after 64 increments: %u", c[0]);
            for (int i = 0; i < 64; i++) { if (o[i] >= 1 && o[i] <= 64) { mask |= 1ull << (o[i] - 1); seen++; } }
            CHECKF(seen == 64 && mask == ~0ull, "increment slots are not a permutation (seen %u)", seen);
        }
        free(c); free(o);
        ID3D11DeviceContext_CSSetShader(r.ctx, cd, NULL, 0);
        counts[0] = 0xFFFFFFFFu;
        cs_bind_uavs(&r, 2, u, counts);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        ID3D11DeviceContext_CopyStructureCount(r.ctx, cnt, 4, u[0]);
        c = rb(&r, cnt, NULL); o = rb(&r, ob, NULL);
        if (c && o) {
            CHECKF(c[1] == 0, "counter after 64 decrements: %u", c[1]);
            CHECKF(o[0] == 63 * 64 / 2, "sum of DecrementCounter results %u want %u (values 63..0)", o[0], 63 * 64 / 2);
            CHECKF(o[1] == 0xFFFFFFFFu, "DecrementCounter results do not cover 0..31 (mask %08x)", o[1]);
        }
        free(c); free(o);
    }
    SAFE_RELEASE(u[0]); SAFE_RELEASE(u[1]); SAFE_RELEASE(b); SAFE_RELEASE(ob); SAFE_RELEASE(cnt); SAFE_RELEASE(ci); SAFE_RELEASE(cd);
    rx_close(&r);
}

TEST(d3d11_render_exec_copy_structure_count_offsets_and_zero)
{
    rx r; if (!rx_open(&r)) return;
    uint32_t zero[64] = { 0 };
    ID3D11Buffer *b = rx_buffer(&r, 256, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, D3D11_USAGE_DEFAULT, 0, zero);
    uint32_t pre[8]; fill32(pre, 8, 0x55555555u);
    ID3D11Buffer *cnt = rx_buffer(&r, 32, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, D3D11_USAGE_DEFAULT, 0, pre);
    ID3D11UnorderedAccessView *u = b ? uav_buf(&r, b, DXGI_FORMAT_UNKNOWN, 64, D3D11_BUFFER_UAV_FLAG_APPEND) : NULL;
    static const char src[] = HL(RWByteAddressBuffer o : register(u1); [numthreads(1, 1, 1)] void main() { o.Store(0, 1); });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    ID3D11UnorderedAccessView *dummy = cnt ? uav_raw(&r, cnt, 32) : NULL;
    if (u && cs && dummy) {
        static const UINT counts[] = { 0, 7, 63, 64, 1 };
        ID3D11UnorderedAccessView *uu[2] = { u, dummy };
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        for (unsigned i = 0; i < A(counts); i++) {
            UINT ic[2]; ic[0] = counts[i]; ic[1] = 0xFFFFFFFFu;
            cs_bind_uavs(&r, 2, uu, ic);                  /* binding sets the counter to the initial count; no dispatch needed */
            ID3D11DeviceContext_CopyStructureCount(r.ctx, cnt, (i % 5) * 4, u);
        }
        uint32_t *c = rb(&r, cnt, NULL);
        if (c) {
            for (unsigned i = 0; i < 5; i++) CHECKF(c[i] == counts[i], "slot %u: copied count %u want %u", i, c[i], counts[i]);
            CHECKF(c[5] == 0x55555555u && c[6] == 0x55555555u, "bytes beyond the copied counts changed");
            free(c);
        }
    }
    SAFE_RELEASE(dummy); SAFE_RELEASE(u); SAFE_RELEASE(b); SAFE_RELEASE(cnt); SAFE_RELEASE(cs);
    rx_close(&r);
}

/* ---------------------------------------------------- clear UAVs ---- */
TEST(d3d11_render_exec_clear_uav_uint_and_float)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11Buffer *b = rx_buffer(&r, 64, D3D11_BIND_UNORDERED_ACCESS, 0, 0, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11Buffer *b4 = rx_buffer(&r, 256, D3D11_BIND_UNORDERED_ACCESS, 0, 0, D3D11_USAGE_DEFAULT, 0, NULL);
    UINT sup4 = 0;
    ID3D11Device_CheckFormatSupport(r.dev, DXGI_FORMAT_R32G32B32A32_UINT, &sup4);
    if (!(sup4 & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW)) { t_skip("typed UAV on R32G32B32A32_UINT unsupported"); SAFE_RELEASE(b); SAFE_RELEASE(b4); rx_close(&r); return; }
    ID3D11Buffer *bf = rx_buffer(&r, 64, D3D11_BIND_UNORDERED_ACCESS, 0, 0, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11UnorderedAccessView *u = b ? uav_buf(&r, b, DXGI_FORMAT_R32_UINT, 16, 0) : NULL;
    ID3D11UnorderedAccessView *u4 = b4 ? uav_buf(&r, b4, DXGI_FORMAT_R32G32B32A32_UINT, 16, 0) : NULL;
    ID3D11UnorderedAccessView *uf = bf ? uav_buf(&r, bf, DXGI_FORMAT_R32_FLOAT, 16, 0) : NULL;
    if (u && u4 && uf) {
        static const UINT v[4] = { 0xDEADBEEFu, 2, 3, 4 };
        static const float fv[4] = { 2.5f, 0, 0, 0 };
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(r.ctx, u, v);
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(r.ctx, u4, v);
        ID3D11DeviceContext_ClearUnorderedAccessViewFloat(r.ctx, uf, fv);
        uint32_t *o = rb(&r, b, NULL), *o4 = rb(&r, b4, NULL), *of = rb(&r, bf, NULL);
        if (o) { for (int i = 0; i < 16; i++) CHECKF(o[i] == 0xDEADBEEFu, "R32_UINT element %d = %08x", i, o[i]); }
        if (o4) { for (int i = 0; i < 16; i++) CHECKF(o4[i * 4] == v[0] && o4[i * 4 + 1] == 2 && o4[i * 4 + 2] == 3 && o4[i * 4 + 3] == 4, "R32G32B32A32_UINT element %d", i); }
        if (of) { for (int i = 0; i < 16; i++) { float f; memcpy(&f, &of[i], 4); CHECKF(f == 2.5f, "R32_FLOAT element %d = %g", i, f); } }
        free(o); free(o4); free(of);
    }
    SAFE_RELEASE(u); SAFE_RELEASE(u4); SAFE_RELEASE(uf); SAFE_RELEASE(b); SAFE_RELEASE(b4); SAFE_RELEASE(bf);
    rx_close(&r);
}

TEST(d3d11_render_exec_clear_uav_texture_rgba8_and_uint)
{
    rx r; if (!rx_open(&r)) return;
    UINT sup32 = 0;
    ID3D11Device_CheckFormatSupport(r.dev, DXGI_FORMAT_R32G32B32A32_UINT, &sup32);
    ID3D11Texture2D *t32 = (sup32 & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) ? rx_tex2d(&r, 4, 4, 1, 1, DXGI_FORMAT_R32G32B32A32_UINT, 1, D3D11_BIND_UNORDERED_ACCESS, 0, NULL) : NULL;
    ID3D11Texture2D *t8 = rx_tex2d(&r, 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_UNORDERED_ACCESS, 0, NULL);
    UINT sup = 0;
    ID3D11Device_CheckFormatSupport(r.dev, DXGI_FORMAT_R8G8B8A8_UNORM, &sup);
    D3D11_UNORDERED_ACCESS_VIEW_DESC d; ID3D11UnorderedAccessView *u32 = NULL, *u8 = NULL;
    memset(&d, 0, sizeof d); d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    if (t32) { d.Format = DXGI_FORMAT_R32G32B32A32_UINT; EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r.dev, (ID3D11Resource *)t32, &d, &u32)); }
    if (t8 && (sup & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW)) { d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r.dev, (ID3D11Resource *)t8, &d, &u8)); }
    if (u32) {
        static const UINT v[4] = { 1, 0xFFFFFFFFu, 3, 0x80000000u };
        d3dh_image im;
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(r.ctx, u32, v);
        if (rx_read(&r, t32, 0, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) if (rx_u32(&im, x, y, 0) != 1 || rx_u32(&im, x, y, 1) != 0xFFFFFFFFu || rx_u32(&im, x, y, 2) != 3 || rx_u32(&im, x, y, 3) != 0x80000000u) bad++;
            CHECKF(bad == 0, "%u texels wrong", bad);
            d3dh_image_free(&im);
        }
    }
    if (u8) {
        static const float v[4] = { 1, 0, 64 / 255.f, 1 };
        d3dh_image im;
        ID3D11DeviceContext_ClearUnorderedAccessViewFloat(r.ctx, u8, v);
        if (rx_read(&r, t8, 0, &im)) { rx_expect_all32(&im, RGBA8(255, 0, 64, 255)); d3dh_image_free(&im); }
    } else t_note("typed UAV on R8G8B8A8_UNORM unsupported here");
    SAFE_RELEASE(u32); SAFE_RELEASE(u8); SAFE_RELEASE(t32); SAFE_RELEASE(t8);
    rx_close(&r);
}

/* ---------------------------------------------------- indirect ---- */
TEST(d3d11_render_exec_cs_dispatch_indirect_cpu_args_and_offsets)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        RWByteAddressBuffer o : register(u0);
        [numthreads(4, 1, 1)] void main(uint3 g : SV_GroupID, uint3 d : SV_DispatchThreadID) { uint old; o.InterlockedAdd(0, 1, old); o.InterlockedMax(4, g.x); o.InterlockedMax(8, g.y); o.InterlockedMax(12, g.z); });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    uint32_t args[12] = { 99, 99, 99, 3, 2, 2, 5, 1, 1, 0, 4, 4 };
    ID3D11Buffer *ab = rx_buffer(&r, sizeof args, 0, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS, 0, D3D11_USAGE_DEFAULT, 0, args);
    uint32_t zero[4] = { 0, 0, 0, 0 };
    ID3D11Buffer *ob = raw_buf(&r, 16, zero, 0);
    ID3D11UnorderedAccessView *u = ob ? uav_raw(&r, ob, 16) : NULL;
    if (cs && u && ab) {
        static const struct { UINT off; UINT threads, mx, my, mz; } tab[] = { { 12, 3 * 2 * 2 * 4, 2, 1, 1 }, { 24, 5 * 4, 4, 0, 0 }, { 36, 0, 0, 0, 0 } };
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        cs_bind_uavs(&r, 1, &u, NULL);
        for (unsigned i = 0; i < A(tab); i++) {
            ID3D11DeviceContext_UpdateSubresource(r.ctx, (ID3D11Resource *)ob, 0, NULL, zero, 0, 0);
            ID3D11DeviceContext_DispatchIndirect(r.ctx, ab, tab[i].off);
            uint32_t *o = rb(&r, ob, NULL);
            if (o) {
                CHECKF(o[0] == tab[i].threads && o[1] == tab[i].mx && o[2] == tab[i].my && o[3] == tab[i].mz, "args at byte %u: threads %u max gid (%u,%u,%u), want %u (%u,%u,%u)", tab[i].off, o[0], o[1], o[2], o[3], tab[i].threads, tab[i].mx, tab[i].my, tab[i].mz);
                free(o);
            }
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(ob); SAFE_RELEASE(ab); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_dispatch_indirect_gpu_generated_args)
{
    rx r; if (!rx_open(&r)) return;
    static const char gen[] = HL(
        RWByteAddressBuffer args : register(u0);
        [numthreads(1, 1, 1)] void main() { args.Store3(0, uint3(6, 1, 2)); });
    static const char use[] = HL(
        RWByteAddressBuffer o : register(u1);
        [numthreads(8, 1, 1)] void main(uint3 d : SV_DispatchThreadID) { uint old; o.InterlockedAdd(0, 1, old); });
    ID3D11ComputeShader *cg = rx_cs(&r, gen), *cu = rx_cs(&r, use);
    uint32_t zero[4] = { 0 };
    ID3D11Buffer *ab = rx_buffer(&r, 16, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, D3D11_USAGE_DEFAULT, 0, zero);
    ID3D11Buffer *ob = raw_buf(&r, 16, zero, 0);
    ID3D11UnorderedAccessView *ua = ab ? uav_raw(&r, ab, 16) : NULL, *uo = ob ? uav_raw(&r, ob, 16) : NULL;
    if (cg && cu && ua && uo) {
        ID3D11UnorderedAccessView *both[2] = { ua, uo };
        ID3D11DeviceContext_CSSetShader(r.ctx, cg, NULL, 0);
        cs_bind_uavs(&r, 2, both, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        {   /* the argument buffer must not stay bound as a UAV while it feeds DispatchIndirect */
            ID3D11UnorderedAccessView *only_out[2] = { NULL, uo };
            cs_bind_uavs(&r, 2, only_out, NULL);
        }
        ID3D11DeviceContext_CSSetShader(r.ctx, cu, NULL, 0);
        ID3D11DeviceContext_DispatchIndirect(r.ctx, ab, 0);
        uint32_t *o = rb(&r, ob, NULL);
        if (o) { CHECKF(o[0] == 6 * 1 * 2 * 8, "threads run by the GPU-written dispatch: %u want 96", o[0]); free(o); }
    }
    SAFE_RELEASE(ua); SAFE_RELEASE(uo); SAFE_RELEASE(ab); SAFE_RELEASE(ob); SAFE_RELEASE(cg); SAFE_RELEASE(cu);
    rx_close(&r);
}

/* ---------------------------------------------------- ALU goldens ---- */
static uint32_t ref_rev(uint32_t x) { uint32_t r = 0; for (int i = 0; i < 32; i++) if (x & (1u << i)) r |= 1u << (31 - i); return r; }
static uint32_t ref_pop(uint32_t x) { uint32_t n = 0; while (x) { n += x & 1; x >>= 1; } return n; }

TEST(d3d11_render_exec_cs_integer_bit_operations_golden)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        StructuredBuffer<uint2> inb : register(t0);
        RWStructuredBuffer<uint4> outb : register(u0);
        [numthreads(1, 1, 1)]
        void main(uint3 d : SV_DispatchThreadID) {
            uint x = inb[d.x].x, y = inb[d.x].y;
            outb[d.x * 2] = uint4(countbits(x), reversebits(x), firstbitlow(x), firstbithigh(x));
            outb[d.x * 2 + 1] = uint4(x << y, x >> y, (uint)(((int)x) >> y), x / (y | 1) + x % (y | 1));
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    static const uint32_t xs[] = { 0, 1, 2, 0x80000000u, 0xFFFFFFFFu, 0x00F00F00u, 0xDEADBEEFu, 0x12345678u, 255, 65536, 0x7FFFFFFFu, 0x80000001u };
    static const uint32_t ys[] = { 0, 1, 5, 31, 32, 33, 63, 100 };
    enum { NX = 12, NY = 8, N = NX * NY };
    uint32_t in[N * 2];
    for (int i = 0; i < NX; i++) for (int j = 0; j < NY; j++) { in[(i * NY + j) * 2] = xs[i]; in[(i * NY + j) * 2 + 1] = ys[j]; }
    ID3D11Buffer *ib = rx_buffer(&r, sizeof in, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 8, D3D11_USAGE_DEFAULT, 0, in);
    ID3D11Buffer *ob = rx_buffer(&r, N * 32, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11ShaderResourceView *sv = ib ? srv_struct(&r, ib, N) : NULL;
    ID3D11UnorderedAccessView *uv = ob ? uav_buf(&r, ob, DXGI_FORMAT_UNKNOWN, N * 2, 0) : NULL;
    if (cs && sv && uv) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetShaderResources(r.ctx, 0, 1, &sv);
        cs_bind_uavs(&r, 1, &uv, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, N, 1, 1);
        uint32_t *o = rb(&r, ob, NULL);
        if (o) {
            unsigned bad = 0;
            for (int k = 0; k < N; k++) {
                uint32_t x = in[k * 2], y = in[k * 2 + 1], s = y & 31;
                uint32_t low = x ? (uint32_t)__builtin_ctz(x) : 0xFFFFFFFFu, high = x ? 31u - (uint32_t)__builtin_clz(x) : 0xFFFFFFFFu;
                uint32_t want[8] = { ref_pop(x), ref_rev(x), low, high, x << s, x >> s, (uint32_t)((int32_t)x >> s), x / (y | 1) + x % (y | 1) };
                for (int j = 0; j < 8; j++) if (o[k * 8 + j] != want[j]) {
                    if (bad < 6) CHECKF(0, "x=%08x y=%u output %d: got %08x want %08x", x, y, j, o[k * 8 + j], want[j]);
                    bad++;
                }
            }
            CHECKF(bad == 0, "%u of %d values wrong", bad, N * 8);
            free(o);
        }
    }
    SAFE_RELEASE(sv); SAFE_RELEASE(uv); SAFE_RELEASE(ib); SAFE_RELEASE(ob); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_signed_integer_arithmetic_golden)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        StructuredBuffer<int2> inb : register(t0);
        RWStructuredBuffer<int4> outb : register(u0);
        [numthreads(1, 1, 1)]
        void main(uint3 d : SV_DispatchThreadID) {
            int a = inb[d.x].x, b = inb[d.x].y;
            int nz = b == 0 ? 1 : b;
            outb[d.x * 2] = int4(a * b, a / nz, a % nz, a + b);
            outb[d.x * 2 + 1] = int4(a - b, min(a, b), max(a, b), abs(a));
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    static const int32_t as[] = { 0, 1, -1, 7, -7, 100000, -100000, 2147483647, -2147483647 };
    static const int32_t bs[] = { 1, -1, 3, -3, 10, 65536, -65536, 2147483647, 0 };
    enum { NA = 9, NB = 9, N = NA * NB };
    int32_t in[N * 2];
    for (int i = 0; i < NA; i++) for (int j = 0; j < NB; j++) { in[(i * NB + j) * 2] = as[i]; in[(i * NB + j) * 2 + 1] = bs[j]; }
    ID3D11Buffer *ib = rx_buffer(&r, sizeof in, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 8, D3D11_USAGE_DEFAULT, 0, in);
    ID3D11Buffer *ob = rx_buffer(&r, N * 32, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11ShaderResourceView *sv = ib ? srv_struct(&r, ib, N) : NULL;
    ID3D11UnorderedAccessView *uv = ob ? uav_buf(&r, ob, DXGI_FORMAT_UNKNOWN, N * 2, 0) : NULL;
    if (cs && sv && uv) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetShaderResources(r.ctx, 0, 1, &sv);
        cs_bind_uavs(&r, 1, &uv, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, N, 1, 1);
        int32_t *o = (int32_t *)rb(&r, ob, NULL);
        if (o) {
            unsigned bad = 0;
            for (int k = 0; k < N; k++) {
                int32_t a = in[k * 2], b = in[k * 2 + 1], nz = b == 0 ? 1 : b;
                int32_t want[8] = { (int32_t)((uint32_t)a * (uint32_t)b), a / nz, a % nz, (int32_t)((uint32_t)a + (uint32_t)b), (int32_t)((uint32_t)a - (uint32_t)b), a < b ? a : b, a > b ? a : b, a < 0 ? -a : a };
                for (int j = 0; j < 8; j++) if (o[k * 8 + j] != want[j]) {
                    if (bad < 6) CHECKF(0, "a=%d b=%d output %d: got %d want %d", a, b, j, o[k * 8 + j], want[j]);
                    bad++;
                }
            }
            CHECKF(bad == 0, "%u of %d values wrong", bad, N * 8);
            free(o);
        }
    }
    SAFE_RELEASE(sv); SAFE_RELEASE(uv); SAFE_RELEASE(ib); SAFE_RELEASE(ob); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_float_rounding_and_conversion_golden)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        StructuredBuffer<float> inb : register(t0);
        RWStructuredBuffer<uint4> outb : register(u0);
        [numthreads(1, 1, 1)]
        void main(uint3 d : SV_DispatchThreadID) {
            float x = inb[d.x];
            outb[d.x * 3] = uint4(asuint(floor(x)), asuint(ceil(x)), asuint(round(x)), asuint(trunc(x)));
            outb[d.x * 3 + 1] = uint4(asuint((int)x), (uint)x, asuint(saturate(x)), asuint(sign(x)));
            outb[d.x * 3 + 2] = uint4(isnan(x), isinf(x), isfinite(x), asuint(abs(x)));
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    union { float f; uint32_t u; } nanv, pinf, ninf;
    nanv.u = 0x7FC00000u; pinf.u = 0x7F800000u; ninf.u = 0xFF800000u;
    const float xs[] = { -2.5f, -1.5f, -0.5f, 0.5f, 1.5f, 2.5f, 3.5f, -3.7f, 3.7f, 0.0f, 1.0f, -1.0f, 0.99999994f, 1e10f, -1e10f, 2147483520.0f, 4294967040.0f, nanv.f, pinf.f, ninf.f, 100.25f, -100.75f };
    enum { N = 22 };
    ID3D11Buffer *ib = rx_buffer(&r, sizeof xs, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, D3D11_USAGE_DEFAULT, 0, xs);
    ID3D11Buffer *ob = rx_buffer(&r, N * 48, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 16, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11ShaderResourceView *sv = ib ? srv_struct(&r, ib, N) : NULL;
    ID3D11UnorderedAccessView *uv = ob ? uav_buf(&r, ob, DXGI_FORMAT_UNKNOWN, N * 3, 0) : NULL;
    if (cs && sv && uv) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetShaderResources(r.ctx, 0, 1, &sv);
        cs_bind_uavs(&r, 1, &uv, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, N, 1, 1);
        uint32_t *o = rb(&r, ob, NULL);
        if (o) {
            unsigned bad = 0;
            for (int k = 0; k < N; k++) {
                float x = xs[k]; int isn = x != x, isi = (x == pinf.f || x == ninf.f);
                float fl = floorf(x), ce = ceilf(x), ro = rintf(x), tr = truncf(x), sat = isn ? 0.0f : x < 0 ? 0.0f : x > 1 ? 1.0f : x;
                int32_t si = isn ? 0 : x >= 2147483648.0f ? INT_MAX : x <= -2147483648.0f ? INT_MIN : (int32_t)x;
                uint32_t su = isn ? 0 : x <= 0 ? 0 : x >= 4294967296.0f ? 0xFFFFFFFFu : (uint32_t)x;
                int32_t sg = x > 0 ? 1 : x < 0 ? -1 : 0;
                uint32_t want[12]; float wf[4] = { fl, ce, ro, tr };
                for (int j = 0; j < 4; j++) memcpy(&want[j], &wf[j], 4);
                want[4] = (uint32_t)si; want[5] = su; memcpy(&want[6], &sat, 4); want[7] = (uint32_t)sg;
                want[8] = isn; want[9] = isi; want[10] = !isn && !isi; { float a = fabsf(x); memcpy(&want[11], &a, 4); }
                for (int j = 0; j < 12; j++) {
                    uint32_t g = o[k * 12 + j], w = want[j];
                    int same = g == w;
                    if (j < 4 || j == 6 || j == 11) { float gf, wf2; memcpy(&gf, &g, 4); memcpy(&wf2, &w, 4); same = gf == wf2 || ((gf != gf) && (wf2 != wf2)); }
                    if (j >= 8 && j <= 10) same = (g != 0) == (w != 0);
                    if (j == 7 && isn) same = 1;                      /* sign(NaN) is not specified here */
                    if (!same) {
                        if (bad < 8) CHECKF(0, "x=%g output %d: got %08x want %08x", x, j, g, w);
                        bad++;
                    }
                }
            }
            CHECKF(bad == 0, "%u float results differ from the golden values", bad);
            free(o);
        }
    }
    SAFE_RELEASE(sv); SAFE_RELEASE(uv); SAFE_RELEASE(ib); SAFE_RELEASE(ob); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_half_float_conversion_golden)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        StructuredBuffer<uint> inb : register(t0);
        RWStructuredBuffer<uint2> outb : register(u0);
        [numthreads(1, 1, 1)]
        void main(uint3 d : SV_DispatchThreadID) {
            uint v = inb[d.x];
            outb[d.x] = uint2(asuint(f16tof32(v)), f32tof16(asfloat(v)));
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    /* f16tof32 inputs are half bit patterns (low 16 bits); f32tof16 inputs are float bit patterns */
    static const struct { uint32_t in; uint32_t f32_of_half; uint32_t half_of_f32; } tab[] = {
        { 0x3C00, 0x3F800000u, 0 },
        { 0x0000, 0x00000000u, 0 }, { 0x8000, 0x80000000u, 0 }, { 0xBC00, 0xBF800000u, 0 }, { 0x4000, 0x40000000u, 0 }, { 0x3800, 0x3F000000u, 0 },
        { 0x7BFF, 0x477FE000u, 0 }, { 0x0400, 0x38800000u, 0 }, { 0x7C00, 0x7F800000u, 0 }, { 0xFC00, 0xFF800000u, 0 },
    };
    enum { N = 10 };
    uint32_t in[N + 6];
    for (int i = 0; i < N; i++) in[i] = tab[i].in;
    /* float -> half inputs */
    static const uint32_t fin[6] = { 0x3F800000u, 0xBF800000u, 0x40490FDBu /* pi */, 0x477FE000u, 0x47800000u /* 65536 -> inf or max */, 0x33800000u /* 2^-24 smallest half denormal */ };
    for (int i = 0; i < 6; i++) in[N + i] = fin[i];
    ID3D11Buffer *ib = rx_buffer(&r, sizeof in, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 4, D3D11_USAGE_DEFAULT, 0, in);
    ID3D11Buffer *ob = rx_buffer(&r, (N + 6) * 8, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 8, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11ShaderResourceView *sv = ib ? srv_struct(&r, ib, N + 6) : NULL;
    ID3D11UnorderedAccessView *uv = ob ? uav_buf(&r, ob, DXGI_FORMAT_UNKNOWN, N + 6, 0) : NULL;
    if (cs && sv && uv) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetShaderResources(r.ctx, 0, 1, &sv);
        cs_bind_uavs(&r, 1, &uv, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, N + 6, 1, 1);
        uint32_t *o = rb(&r, ob, NULL);
        if (o) {
            for (int i = 0; i < N; i++) CHECKF(o[i * 2] == tab[i].f32_of_half, "f16tof32(0x%04x) = %08x want %08x", tab[i].in, o[i * 2], tab[i].f32_of_half);
            static const uint32_t wantf[6] = { 0x3C00, 0xBC00, 0x4248, 0x7BFF, 0x7C00, 0x0001 };
            for (int i = 0; i < 6; i++) {
                uint32_t g = o[(N + i) * 2 + 1];
                if (i == 4) { if (g != 0x7C00 && g != 0x7BFF) CHECKF(0, "f32tof16(65536) = %04x", g); if (g != 0x7C00) t_note("f32tof16(65536) = %04x (not infinity)", g); }
                else if (i == 5) { if (g != 1) t_note("f32tof16(2^-24) = %04x (denormal handling)", g); }
                else CHECKF(g == wantf[i], "f32tof16(%08x) = %04x want %04x", fin[i], g, wantf[i]);
            }
            free(o);
        }
    }
    SAFE_RELEASE(sv); SAFE_RELEASE(uv); SAFE_RELEASE(ib); SAFE_RELEASE(ob); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_reads_srv_texture_and_sampler)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        Texture2D<float4> t : register(t0); SamplerState s : register(s0);
        RWByteAddressBuffer o : register(u0);
        [numthreads(4, 4, 1)]
        void main(uint3 d : SV_DispatchThreadID) {
            float4 a = t.Load(int3(d.xy, 0));
            float4 b = t.SampleLevel(s, (d.xy + 0.5) / 4.0, 0);
            o.Store3((d.y * 4 + d.x) * 16, uint3(asuint(a.x), asuint(b.x), asuint(a.y + b.y)));
        });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    float tx[4 * 4 * 4];
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { float *t = tx + (y * 4 + x) * 4; t[0] = (float)(x + 10 * y); t[1] = (float)y; t[2] = t[3] = 0; }
    D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = tx; sd.SysMemPitch = 64; sd.SysMemSlicePitch = 0;
    ID3D11Texture2D *t = rx_tex2d(&r, 4, 4, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
    D3D11_SAMPLER_DESC smd; ID3D11SamplerState *smp = NULL; ID3D11ShaderResourceView *sv = NULL;
    memset(&smd, 0, sizeof smd);
    smd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; smd.AddressU = smd.AddressV = smd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; smd.MaxAnisotropy = 1; smd.ComparisonFunc = D3D11_COMPARISON_NEVER; smd.MaxLOD = D3D11_FLOAT32_MAX;
    EXPECT_HR_OK(ID3D11Device_CreateSamplerState(r.dev, &smd, &smp));
    if (t) EXPECT_HR_OK(ID3D11Device_CreateShaderResourceView(r.dev, (ID3D11Resource *)t, NULL, &sv));
    ID3D11Buffer *ob = raw_buf(&r, 256, NULL, 0);
    ID3D11UnorderedAccessView *uv = ob ? uav_raw(&r, ob, 256) : NULL;
    if (cs && sv && smp && uv) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetShaderResources(r.ctx, 0, 1, &sv);
        ID3D11DeviceContext_CSSetSamplers(r.ctx, 0, 1, &smp);
        cs_bind_uavs(&r, 1, &uv, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        float *o = (float *)rb(&r, ob, NULL);
        if (o) {
            unsigned bad = 0;
            for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
                const float *e = o + (y * 4 + x) * 4;
                if (e[0] != x + 10 * y || e[1] != x + 10 * y || e[2] != 2.0f * y) bad++;
            }
            CHECKF(bad == 0, "%u texels differ between Load, SampleLevel and the reference", bad);
            free(o);
        }
    }
    SAFE_RELEASE(sv); SAFE_RELEASE(smp); SAFE_RELEASE(t); SAFE_RELEASE(uv); SAFE_RELEASE(ob); SAFE_RELEASE(cs);
    rx_close(&r);
}

TEST(d3d11_render_exec_cs_constant_buffer_dynamic_update_between_dispatches)
{
    rx r; if (!rx_open(&r)) return;
    static const char src[] = HL(
        cbuffer K : register(b0) { uint4 k; };
        RWByteAddressBuffer o : register(u0);
        [numthreads(1, 1, 1)] void main(uint3 g : SV_GroupID) { o.Store(g.x * 4, k.x * 100 + k.y * 10 + k.z + k.w); });
    ID3D11ComputeShader *cs = rx_cs(&r, src);
    ID3D11Buffer *cb = rx_buffer(&r, 16, D3D11_BIND_CONSTANT_BUFFER, 0, 0, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL);
    ID3D11Buffer *ob = raw_buf(&r, 64, NULL, 0);
    ID3D11UnorderedAccessView *uv = ob ? uav_raw(&r, ob, 64) : NULL;
    if (cs && cb && uv) {
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetConstantBuffers(r.ctx, 0, 1, &cb);
        cs_bind_uavs(&r, 1, &uv, NULL);
        for (UINT i = 0; i < 4; i++) {
            D3D11_MAPPED_SUBRESOURCE m;
            if (FAILED(ID3D11DeviceContext_Map(r.ctx, (ID3D11Resource *)cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) { CHECKF(0, "map discard failed"); break; }
            ((UINT *)m.pData)[0] = i + 1; ((UINT *)m.pData)[1] = i; ((UINT *)m.pData)[2] = 3; ((UINT *)m.pData)[3] = 1;
            ID3D11DeviceContext_Unmap(r.ctx, (ID3D11Resource *)cb, 0);
            ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
            /* each dispatch writes slot 0; copy it out before the next update */
            uint32_t *o = rb(&r, ob, NULL);
            if (o) { CHECKF(o[0] == (i + 1) * 100 + i * 10 + 4, "update %u: %u", i, o[0]); free(o); }
        }
    }
    SAFE_RELEASE(uv); SAFE_RELEASE(ob); SAFE_RELEASE(cb); SAFE_RELEASE(cs);
    rx_close(&r);
}
