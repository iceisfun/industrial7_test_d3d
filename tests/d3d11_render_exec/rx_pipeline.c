/* Geometry, stream output, tessellation, indirect draws, UAV-only rendering, deferred contexts, copies. */
#include "rx.h"

#define A(a) RX_N(a)

/* ------------------------------------------------------ helpers ---- */
static unsigned rowmask(rx *r)
{
    d3dh_image im; unsigned m = 0;
    if (!rx_read_rt(r, &im)) return 0xFFFFFFFFu;
    for (UINT x = 0; x < im.width && x < 32; x++) if (d3dh_image_pixel32(&im, x, 0)) m |= 1u << x;
    d3dh_image_free(&im);
    return m;
}
static void points16(float *v)
{
    for (unsigned k = 0; k < 16; k++) { v[k * 2] = (k + 0.5f) / 16 * 2 - 1; v[k * 2 + 1] = 0; }
}
static ID3D11GeometryShader *mk_so(rx *r, const char *gs_src, const D3D11_SO_DECLARATION_ENTRY *e, UINT ne, const UINT *strides, UINT ns, UINT raster)
{
    ID3DBlob *b = rx_blob(gs_src, "gs_5_0"); ID3D11GeometryShader *g = NULL;
    if (!b) return NULL;
    HRESULT hr = ID3D11Device_CreateGeometryShaderWithStreamOutput(r->dev, d3dh_blob_ptr(b), d3dh_blob_size(b), e, ne, strides, ns, raster, NULL, &g);
    EXPECT_HR_OK(hr);
    SAFE_RELEASE(b);
    return SUCCEEDED(hr) ? g : NULL;
}

static const char VS_SO[] = HL(
    struct O { float4 p : SV_Position; float4 c : COLOR; };
    O main(uint id : SV_VertexID) { O o; o.p = float4(id, id * 2, 0, 1); o.c = float4(id + 1, id * 3, 7, 1); return o; });
static const char GS_PT[] = HL(
    struct V { float4 p : SV_Position; float4 c : COLOR; };
    [maxvertexcount(1)] void main(point V i[1], inout PointStream<V> s) { s.Append(i[0]); });
static const char GS_PT2[] = HL(
    struct V { float4 p : SV_Position; float4 c : COLOR; };
    [maxvertexcount(2)] void main(point V i[1], inout PointStream<V> s) { s.Append(i[0]); V b = i[0]; b.p.x += 100; s.Append(b); });
static const char GS_TRI[] = HL(
    struct V { float4 p : SV_Position; float4 c : COLOR; };
    [maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> s) { s.Append(i[0]); s.Append(i[1]); s.Append(i[2]); });
static const char GS_STRIP4[] = HL(
    struct V { float4 p : SV_Position; float4 c : COLOR; };
    [maxvertexcount(4)] void main(point V i[1], inout TriangleStream<V> s) {
        V a = i[0]; a.p.x += 0; s.Append(a); a.p.x += 1; s.Append(a); a.p.x -= 1; a.p.y += 1; s.Append(a); a.p.x += 1; s.Append(a); });
static const D3D11_SO_DECLARATION_ENTRY SO_PC[] = { { 0, "SV_Position", 0, 0, 4, 0 }, { 0, "COLOR", 0, 0, 4, 0 } };
static const UINT SO_STRIDE32[1] = { 32 };

typedef struct { float p[4], c[4]; } sov;

static ID3D11Buffer *so_buf(rx *r, UINT bytes, UINT extra_bind)
{
    return rx_buffer(r, bytes, D3D11_BIND_STREAM_OUTPUT | extra_bind, 0, 0, D3D11_USAGE_DEFAULT, 0, NULL);
}
/* stream-output buffer pre-filled with a byte pattern (via CopyResource: SO buffers are not created with initial data) */
static ID3D11Buffer *so_buf_pat(rx *r, UINT bytes, UINT extra_bind, uint8_t pat)
{
    ID3D11Buffer *b = so_buf(r, bytes, extra_bind), *src = NULL;
    uint8_t *tmp = (uint8_t *)malloc(bytes);
    memset(tmp, pat, bytes);
    src = rx_buffer(r, bytes, D3D11_BIND_SHADER_RESOURCE, 0, 0, D3D11_USAGE_DEFAULT, 0, tmp);
    free(tmp);
    if (b && src) ID3D11DeviceContext_CopyResource(r->ctx, (ID3D11Resource *)b, (ID3D11Resource *)src);
    SAFE_RELEASE(src);
    return b;
}
static void so_begin(rx *r, ID3D11VertexShader *vs, ID3D11GeometryShader *gs, D3D11_PRIMITIVE_TOPOLOGY topo)
{
    ID3D11DeviceContext_OMSetRenderTargets(r->ctx, 0, NULL, NULL);
    ID3D11DeviceContext_IASetInputLayout(r->ctx, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(r->ctx, topo);
    ID3D11DeviceContext_VSSetShader(r->ctx, vs, NULL, 0);
    ID3D11DeviceContext_GSSetShader(r->ctx, gs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(r->ctx, NULL, NULL, 0);
}
static void so_unbind(rx *r)
{
    ID3D11Buffer *nb[1] = { NULL }; UINT z[1] = { 0 };
    ID3D11DeviceContext_SOSetTargets(r->ctx, 1, nb, z);
}

/* --------------------------------------------------- stream output ---- */
TEST(d3d11_render_exec_so_capture_points_exact)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs = rx_vs(&r, VS_SO);
    ID3D11GeometryShader *gs = mk_so(&r, GS_PT, SO_PC, 2, SO_STRIDE32, 1, D3D11_SO_NO_RASTERIZED_STREAM);
    ID3D11Buffer *buf = so_buf(&r, 32 * 8, 0);
    if (vs && gs && buf) {
        UINT off = 0;
        so_begin(&r, vs, gs, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &buf, &off);
        ID3D11DeviceContext_Draw(r.ctx, 5, 0);
        so_unbind(&r);
        sov *o = NULL; size_t sz = 0;
        if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, buf, (void **)&o, &sz))) {
            for (int i = 0; i < 5; i++) {
                CHECKF(o[i].p[0] == i && o[i].p[1] == i * 2 && o[i].p[2] == 0 && o[i].p[3] == 1, "record %d position %g %g %g %g", i, o[i].p[0], o[i].p[1], o[i].p[2], o[i].p[3]);
                CHECKF(o[i].c[0] == i + 1 && o[i].c[1] == i * 3 && o[i].c[2] == 7 && o[i].c[3] == 1, "record %d colour %g %g %g %g", i, o[i].c[0], o[i].c[1], o[i].c[2], o[i].c[3]);
            }
            free(o);
        }
    }
    SAFE_RELEASE(buf); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_so_partial_component_declaration_and_gaps)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs = rx_vs(&r, VS_SO);
    /* SV_Position.xy to slot 0 (stride 8), COLOR.zw (StartComponent 2, 2 components) to slot 1 (stride 8) */
    static const D3D11_SO_DECLARATION_ENTRY e[] = { { 0, "SV_Position", 0, 0, 2, 0 }, { 0, "COLOR", 0, 2, 2, 1 } };
    static const UINT strides[2] = { 8, 8 };
    ID3D11GeometryShader *gs = mk_so(&r, GS_PT, e, 2, strides, 2, D3D11_SO_NO_RASTERIZED_STREAM);
    ID3D11Buffer *b0 = so_buf(&r, 8 * 6, 0), *b1 = so_buf(&r, 8 * 6, 0);
    if (vs && gs && b0 && b1) {
        ID3D11Buffer *bs[2] = { b0, b1 }; UINT off[2] = { 0, 0 };
        so_begin(&r, vs, gs, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_SOSetTargets(r.ctx, 2, bs, off);
        ID3D11DeviceContext_Draw(r.ctx, 6, 0);
        ID3D11Buffer *nb[2] = { NULL, NULL };
        ID3D11DeviceContext_SOSetTargets(r.ctx, 2, nb, off);
        float *p0 = NULL, *p1 = NULL; size_t s0, s1;
        if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, b0, (void **)&p0, &s0)) && SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, b1, (void **)&p1, &s1))) {
            for (int i = 0; i < 6; i++) {
                CHECKF(p0[i * 2] == i && p0[i * 2 + 1] == i * 2, "slot 0 record %d: %g %g", i, p0[i * 2], p0[i * 2 + 1]);
                CHECKF(p1[i * 2] == 7 && p1[i * 2 + 1] == 1, "slot 1 record %d: %g %g", i, p1[i * 2], p1[i * 2 + 1]);
            }
        }
        free(p0); free(p1);
    }
    SAFE_RELEASE(b0); SAFE_RELEASE(b1); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_so_triangle_strip_decomposed_to_list)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs = rx_vs(&r, VS_SO);
    ID3D11GeometryShader *gs = mk_so(&r, GS_STRIP4, SO_PC, 2, SO_STRIDE32, 1, D3D11_SO_NO_RASTERIZED_STREAM);
    ID3D11Buffer *buf = so_buf(&r, 32 * 12, 0);
    ID3D11Query *q = rx_make_query(&r, D3D11_QUERY_SO_STATISTICS);
    if (vs && gs && buf && q) {
        UINT off = 0; D3D11_QUERY_DATA_SO_STATISTICS st; memset(&st, 0xCD, sizeof st);
        so_begin(&r, vs, gs, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &buf, &off);
        ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)q);
        ID3D11DeviceContext_Draw(r.ctx, 1, 0);
        ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)q);
        so_unbind(&r);
        EXPECT_HR(rx_query(&r, q, &st, sizeof st), S_OK);
        CHECKF(st.NumPrimitivesWritten == 2, "primitives written %llu want 2 (a 4-vertex strip is two triangles)", (unsigned long long)st.NumPrimitivesWritten);
        CHECKF(st.PrimitivesStorageNeeded == 2, "storage needed %llu", (unsigned long long)st.PrimitivesStorageNeeded);
        sov *o = NULL; size_t sz;
        if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, buf, (void **)&o, &sz))) {
            /* vertices: v0 (0,0) v1 (1,0) v2 (0,1) v3 (1,1) from the point at (0,0); lists: (v0 v1 v2) and (v2 v1 v3) */
            static const float xy[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
            for (int i = 0; i < 6; i++) CHECKF(o[i].p[0] == xy[i][0] && o[i].p[1] == xy[i][1], "list vertex %d: %g %g want %g %g", i, o[i].p[0], o[i].p[1], xy[i][0], xy[i][1]);
            free(o);
        }
    }
    SAFE_RELEASE(q); SAFE_RELEASE(buf); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_so_gs_amplification_counts)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs = rx_vs(&r, VS_SO);
    ID3D11GeometryShader *gs = mk_so(&r, GS_PT2, SO_PC, 2, SO_STRIDE32, 1, D3D11_SO_NO_RASTERIZED_STREAM);
    ID3D11Buffer *buf = so_buf(&r, 32 * 16, 0);
    ID3D11Query *q = rx_make_query(&r, D3D11_QUERY_SO_STATISTICS), *ps = rx_make_query(&r, D3D11_QUERY_PIPELINE_STATISTICS);
    if (vs && gs && buf && q && ps) {
        UINT off = 0; D3D11_QUERY_DATA_SO_STATISTICS st; D3D11_QUERY_DATA_PIPELINE_STATISTICS pst;
        so_begin(&r, vs, gs, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &buf, &off);
        ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)q);
        ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)ps);
        ID3D11DeviceContext_Draw(r.ctx, 7, 0);
        ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)q);
        ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)ps);
        so_unbind(&r);
        EXPECT_HR(rx_query(&r, q, &st, sizeof st), S_OK);
        CHECKF(st.NumPrimitivesWritten == 14 && st.PrimitivesStorageNeeded == 14, "written %llu needed %llu want 14/14", (unsigned long long)st.NumPrimitivesWritten, (unsigned long long)st.PrimitivesStorageNeeded);
        EXPECT_HR(rx_query(&r, ps, &pst, sizeof pst), S_OK);
        CHECKF(pst.IAVertices == 7 && pst.IAPrimitives == 7, "IA %llu/%llu", (unsigned long long)pst.IAVertices, (unsigned long long)pst.IAPrimitives);
        CHECKF(pst.GSInvocations == 7, "GSInvocations %llu", (unsigned long long)pst.GSInvocations);
        CHECKF(pst.GSPrimitives == 14, "GSPrimitives %llu", (unsigned long long)pst.GSPrimitives);
        sov *o = NULL; size_t sz;
        if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, buf, (void **)&o, &sz))) {
            for (int i = 0; i < 7; i++) CHECKF(o[2 * i].p[0] == i && o[2 * i + 1].p[0] == i + 100, "pair %d: %g %g", i, o[2 * i].p[0], o[2 * i + 1].p[0]);
            free(o);
        }
    }
    SAFE_RELEASE(q); SAFE_RELEASE(ps); SAFE_RELEASE(buf); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_so_append_offsets_and_overwrite)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs = rx_vs(&r, VS_SO);
    ID3D11GeometryShader *gs = mk_so(&r, GS_PT, SO_PC, 2, SO_STRIDE32, 1, D3D11_SO_NO_RASTERIZED_STREAM);
    ID3D11Buffer *buf = so_buf_pat(&r, 32 * 10, 0, 0x42);
    if (vs && gs && buf) {
        UINT off = 0, app = 0xFFFFFFFFu;
        so_begin(&r, vs, gs, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &buf, &off);
        ID3D11DeviceContext_Draw(r.ctx, 3, 0);                    /* records 0,1,2 hold vertex ids 0,1,2 */
        ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &buf, &app);   /* -1: append after the previous end */
        ID3D11DeviceContext_Draw(r.ctx, 2, 0);                    /* records 3,4 hold ids 0,1 */
        off = 32;
        ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &buf, &off);   /* explicit offset: record 1 */
        ID3D11DeviceContext_Draw(r.ctx, 1, 0);                    /* record 1 now holds id 0 */
        so_unbind(&r);
        sov *o = NULL; size_t sz;
        if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, buf, (void **)&o, &sz))) {
            static const float want[5] = { 0, 0, 2, 0, 1 };
            for (int i = 0; i < 5; i++) CHECKF(o[i].p[0] == want[i] && o[i].p[3] == 1, "record %d x=%g want %g", i, o[i].p[0], want[i]);
            for (int i = 5; i < 10; i++) { uint32_t w; memcpy(&w, &o[i].p[0], 4); CHECKF(w == 0x42424242u, "record %d beyond the appended range was modified", i); }
            free(o);
        }
    }
    SAFE_RELEASE(buf); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_so_overflow_stops_whole_primitives)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs = rx_vs(&r, VS_SO);
    ID3D11GeometryShader *gs = mk_so(&r, GS_PT, SO_PC, 2, SO_STRIDE32, 1, D3D11_SO_NO_RASTERIZED_STREAM);
    ID3D11Buffer *buf = so_buf_pat(&r, 4 * sizeof(sov), 0, 0xAB);
    ID3D11Buffer *small = NULL;
    ID3D11Query *q = rx_make_query(&r, D3D11_QUERY_SO_STATISTICS);
    if (vs && gs && buf && q) {
        /* the bound range is [offset, buffer end): start at record 2 of 4 so only 2 records fit */
        UINT off = 64; D3D11_QUERY_DATA_SO_STATISTICS st; memset(&st, 0, sizeof st);
        so_begin(&r, vs, gs, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &buf, &off);
        ID3D11DeviceContext_Begin(r.ctx, (ID3D11Asynchronous *)q);
        ID3D11DeviceContext_Draw(r.ctx, 5, 0);
        ID3D11DeviceContext_End(r.ctx, (ID3D11Asynchronous *)q);
        so_unbind(&r);
        EXPECT_HR(rx_query(&r, q, &st, sizeof st), S_OK);
        CHECKF(st.NumPrimitivesWritten == 2, "written %llu want 2", (unsigned long long)st.NumPrimitivesWritten);
        CHECKF(st.PrimitivesStorageNeeded == 5, "storage needed %llu want 5", (unsigned long long)st.PrimitivesStorageNeeded);
        sov *o = NULL; size_t sz;
        if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, buf, (void **)&o, &sz))) {
            const uint8_t *raw = (const uint8_t *)o; unsigned untouched = 0;
            for (int i = 0; i < 64; i++) if (raw[i] == 0xAB) untouched++;
            CHECKF(untouched == 64, "bytes before the SO offset were modified (%u of 64 intact)", untouched);
            CHECK(o[2].p[0] == 0 && o[3].p[0] == 1);
            free(o);
        }
    }
    SAFE_RELEASE(small); SAFE_RELEASE(q); SAFE_RELEASE(buf); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_so_get_targets_roundtrip)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11Buffer *b = so_buf(&r, 256, 0), *got[2] = { NULL, NULL };
    if (b) {
        UINT off = 64;
        ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &b, &off);
        ID3D11DeviceContext_SOGetTargets(r.ctx, 2, got);
        CHECK(got[0] == b && got[1] == NULL);
        SAFE_RELEASE(got[0]);
        so_unbind(&r);
        ID3D11DeviceContext_SOGetTargets(r.ctx, 1, got);
        CHECK(got[0] == NULL);
    }
    SAFE_RELEASE(b);
    rx_close(&r);
}

static const char VS_NDC[] = HL(
    struct O { float4 p : SV_Position; float4 c : COLOR; };
    O main(uint id : SV_VertexID) { O o; o.p = float4((id + 0.5) / 16 * 2 - 1, 0, 0, 1); o.c = float4(1, 1, 1, 1); return o; });
static const char VS_FROM_SO[] = HL(float4 main(float4 p : POSITION, float4 c : COLOR) : SV_Position { return p; });

TEST(d3d11_render_exec_draw_auto_replays_stream_output)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11VertexShader *vs0 = rx_vs(&r, VS_NDC);
    static const D3D11_INPUT_ELEMENT_DESC el[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs1 = rx_vs_il(&r, VS_FROM_SO, el, 2, &il);
    ID3D11GeometryShader *gs = mk_so(&r, GS_PT, SO_PC, 2, SO_STRIDE32, 1, D3D11_SO_NO_RASTERIZED_STREAM);
    ID3D11Buffer *buf = so_buf(&r, 32 * 16, D3D11_BIND_VERTEX_BUFFER);
    if (vs0 && vs1 && il && gs && buf && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        static const unsigned counts[] = { 5, 1, 16 };
        for (unsigned i = 0; i < A(counts); i++) {
            UINT off = 0, stride = 32, voff = 0;
            ID3D11RenderTargetView *rtv = r.rtv;
            ID3D11GeometryShader *nogs = NULL;
            rx_clear(&r, 0, 0, 0, 0);
            so_begin(&r, vs0, gs, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
            ID3D11DeviceContext_SOSetTargets(r.ctx, 1, &buf, &off);
            ID3D11DeviceContext_Draw(r.ctx, counts[i], 0);
            so_unbind(&r);
            /* second pass: the SO buffer is the vertex buffer, vertex count comes from the stream-output counter */
            ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 1, &rtv, NULL);
            ID3D11DeviceContext_GSSetShader(r.ctx, nogs, NULL, 0);
            ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
            ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &buf, &stride, &voff);
            ID3D11DeviceContext_VSSetShader(r.ctx, vs1, NULL, 0);
            rx_set_color(&r, RX_WHITE);
            ID3D11DeviceContext_DrawAuto(r.ctx);
            unsigned m = rowmask(&r), want = counts[i] >= 16 ? 0xFFFFu : (1u << counts[i]) - 1;
            CHECKF(m == want, "DrawAuto after %u captured points: mask %04x want %04x", counts[i], m, want);
            ID3D11Buffer *nb = NULL; UINT z = 0;
            ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &nb, &z, &z);
        }
    }
    SAFE_RELEASE(buf); SAFE_RELEASE(gs); SAFE_RELEASE(il); SAFE_RELEASE(vs1); SAFE_RELEASE(vs0);
    rx_close(&r);
}

/* --------------------------------------------------- geometry shader ---- */
static const char VS_PTID[] = HL(
    struct O { float4 p : SV_Position; };
    O main(uint id : SV_VertexID) { O o; o.p = float4(id < 4 ? -0.5 + 0.5 * id : 0, 0, 0, 1); return o; });

TEST(d3d11_render_exec_gs_point_to_quad_amplification)
{
    rx r; if (!rx_open(&r)) return;
    static const char vs_src[] = HL(
        float4 main(uint id : SV_VertexID) : SV_Position {
            float2 px = id == 0 ? float2(4, 4) : id == 1 ? float2(10, 5) : float2(7, 12);
            return float4(px.x / 8 - 1, 1 - px.y / 8, 0, 1); });
    static const char gs_src[] = HL(
        struct V { float4 p : SV_Position; };
        [maxvertexcount(4)] void main(point V i[1], inout TriangleStream<V> s) {
            float2 c = i[0].p.xy; V v;
            v.p = float4(c.x - 0.25, c.y + 0.25, 0, 1); s.Append(v);
            v.p = float4(c.x + 0.25, c.y + 0.25, 0, 1); s.Append(v);
            v.p = float4(c.x - 0.25, c.y - 0.25, 0, 1); s.Append(v);
            v.p = float4(c.x + 0.25, c.y - 0.25, 0, 1); s.Append(v);
        });
    ID3D11VertexShader *vs = rx_vs(&r, vs_src); ID3D11GeometryShader *gs = rx_gs(&r, gs_src);
    if (vs && gs && rx_target(&r, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, TRUE, FALSE);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_GSSetShader(r.ctx, gs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        ID3D11DeviceContext_Draw(r.ctx, 3, 0);
        if (rx_read_rt(&r, &im)) {
            unsigned bad = 0;
            static const int c[3][2] = { { 4, 4 }, { 10, 5 }, { 7, 12 } };
            for (UINT y = 0; y < 16; y++) for (UINT x = 0; x < 16; x++) {
                int in = 0;
                for (int k = 0; k < 3; k++) if ((int)x >= c[k][0] - 2 && (int)x < c[k][0] + 2 && (int)y >= c[k][1] - 2 && (int)y < c[k][1] + 2) in = 1;
                if ((d3dh_image_pixel32(&im, x, y) != 0) != in) bad++;
            }
            CHECKF(bad == 0, "%u pixels differ from three 4x4 squares", bad);
            CHECKF(rx_count32(&im, 0xFFFFFFFFu) == 48, "covered %u want 48", rx_count32(&im, 0xFFFFFFFFu));
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_gs_restart_strip_separates_triangles)
{
    rx r; if (!rx_open(&r)) return;
    static const char vs_src[] = HL(float4 main(uint id : SV_VertexID) : SV_Position { return float4(0, 0, 0, 1); });
    /* two right triangles in an 8x8 target; hypotenuses at x+y=4.5 avoid pixel-centre ties */
    static const char gs_src[] = HL(
        struct V { float4 p : SV_Position; };
        float4 px(float x, float y) { return float4(x / 4 - 1, 1 - y / 4, 0, 1); }
        [maxvertexcount(6)] void main(point V i[1], inout TriangleStream<V> s) {
            V v;
            v.p = px(0, 0); s.Append(v); v.p = px(4.5, 0); s.Append(v); v.p = px(0, 4.5); s.Append(v);
            s.RestartStrip();
            v.p = px(4, 4); s.Append(v); v.p = px(8.5, 4); s.Append(v); v.p = px(4, 8.5); s.Append(v);
        });
    ID3D11VertexShader *vs = rx_vs(&r, vs_src); ID3D11GeometryShader *gs = rx_gs(&r, gs_src);
    if (vs && gs && rx_target(&r, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, TRUE, FALSE);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_GSSetShader(r.ctx, gs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        ID3D11DeviceContext_Draw(r.ctx, 1, 0);
        if (rx_read_rt(&r, &im)) {
            unsigned a = 0, b = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) if (d3dh_image_pixel32(&im, x, y)) { if (x < 4 && y < 4) a++; else b++; }
            CHECKF(a == 10 && b == 10, "triangle pixel counts %u / %u, want 10 / 10 (a connecting triangle would add pixels)", a, b);
            CHECK(d3dh_image_pixel32(&im, 7, 0) == 0 && d3dh_image_pixel32(&im, 0, 7) == 0);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_gs_conditional_emit_and_primitive_id)
{
    rx r; if (!rx_open(&r)) return;
    static const char vs_src[] = HL(float4 main(uint id : SV_VertexID) : SV_Position { return float4((id + 0.5) / 16 * 2 - 1, 0, 0, 1); });
    static const char gs_src[] = HL(
        struct GI { float4 p : SV_Position; };
        struct GO { float4 p : SV_Position; nointerpolation uint pid : PID; };
        [maxvertexcount(1)] void main(point GI i[1], uint pid : SV_PrimitiveID, inout PointStream<GO> s) {
            if (pid & 1) return;
            GO v; v.p = i[0].p; v.pid = pid; s.Append(v);
        });
    static const char ps_src[] = HL(
        float4 main(float4 p : SV_Position, nointerpolation uint pid : PID) : SV_Target { return float4(pid * 20 / 255.0, 0, 0, 1); });
    ID3D11VertexShader *vs = rx_vs(&r, vs_src); ID3D11GeometryShader *gs = rx_gs(&r, gs_src); ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (vs && gs && ps && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_GSSetShader(r.ctx, gs, NULL, 0);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 8, 0);
        if (rx_read_rt(&r, &im)) {
            for (UINT x = 0; x < 16; x++) {
                uint32_t p = d3dh_image_pixel32(&im, x, 0);
                if (x < 8 && (x & 1) == 0) CHECKF((p & 255) == x * 20 && (p >> 24) == 255, "pixel %u: %08x want red %u", x, p, x * 20);
                else CHECKF(p == 0, "pixel %u: %08x want untouched", x, p);
            }
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(ps); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_gs_instancing_attribute)
{
    rx r; if (!rx_open(&r)) return;
    static const char vs_src[] = HL(float4 main(uint id : SV_VertexID) : SV_Position { return float4((id + 0.5) / 16 * 2 - 1, 0, 0, 1); });
    static const char gs_src[] = HL(
        struct V { float4 p : SV_Position; };
        [instance(3)] [maxvertexcount(1)] void main(point V i[1], uint inst : SV_GSInstanceID, inout PointStream<V> s) {
            V v = i[0]; v.p.y = 1 - 2 * (inst + 0.5) / 4; s.Append(v);
        });
    ID3D11VertexShader *vs = rx_vs(&r, vs_src); ID3D11GeometryShader *gs = rx_gs(&r, gs_src);
    if (vs && gs && rx_target(&r, 16, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_GSSetShader(r.ctx, gs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        ID3D11DeviceContext_Draw(r.ctx, 5, 0);
        if (rx_read_rt(&r, &im)) {
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 16; x++) {
                int want = y < 3 && x < 5;
                CHECKF((d3dh_image_pixel32(&im, x, y) != 0) == want, "pixel %u,%u: %08x", x, y, d3dh_image_pixel32(&im, x, y));
            }
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_gs_render_target_array_index)
{
    rx r; if (!rx_open(&r)) return;
    static const char vs_src[] = HL(float4 main(uint id : SV_VertexID) : SV_Position { return float4(0, 0, 0, 1); });
    static const char gs_src[] = HL(
        struct O { float4 p : SV_Position; uint rt : SV_RenderTargetArrayIndex; float4 c : COLOR; };
        [maxvertexcount(3)] void main(point float4 i[1] : SV_Position, uint pid : SV_PrimitiveID, inout TriangleStream<O> s) {
            O o; o.rt = 3 - pid; o.c = float4((pid + 1) * 60 / 255.0, 0, 0, 1);
            o.p = float4(-1, 1, 0, 1); s.Append(o); o.p = float4(3, 1, 0, 1); s.Append(o); o.p = float4(-1, -3, 0, 1); s.Append(o);
        });
    static const char ps_src[] = HL(float4 main(float4 p : SV_Position, float4 c : COLOR) : SV_Target { return c; });
    ID3D11VertexShader *vs = rx_vs(&r, vs_src); ID3D11GeometryShader *gs = rx_gs(&r, gs_src); ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    ID3D11Texture2D *ta = rx_tex2d(&r, 8, 8, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_RENDER_TARGET, 0, NULL);
    ID3D11RenderTargetView *rtv = NULL;
    if (ta) {
        D3D11_RENDER_TARGET_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY; d.Texture2DArray.ArraySize = 4;
        EXPECT_HR_OK(ID3D11Device_CreateRenderTargetView(r.dev, (ID3D11Resource *)ta, &d, &rtv));
    }
    if (vs && gs && ps && rtv) {
        ID3D11DeviceContext_ClearRenderTargetView(r.ctx, rtv, RX_ZERO);
        ID3D11DeviceContext_OMSetRenderTargets(r.ctx, 1, &rtv, NULL);
        r.w = r.h = 8;
        rx_viewport(&r, 0, 0, 8, 8, 0, 1);
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, TRUE, FALSE);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_GSSetShader(r.ctx, gs, NULL, 0);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);        /* point k goes to slice 3-k with red (k+1)*60 */
        for (UINT s = 0; s < 4; s++) {
            d3dh_image im;
            if (!rx_read(&r, ta, s, &im)) continue;
            rx_expect_all32(&im, RGBA8((4 - s) * 60, 0, 0, 255));
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(rtv); SAFE_RELEASE(ta); SAFE_RELEASE(ps); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_gs_viewport_array_index)
{
    rx r; if (!rx_open(&r)) return;
    static const char vs_src[] = HL(float4 main(uint id : SV_VertexID) : SV_Position { return float4(0, 0, 0, 1); });
    static const char gs_src[] = HL(
        struct O { float4 p : SV_Position; uint vp : SV_ViewportArrayIndex; float4 c : COLOR; };
        [instance(2)] [maxvertexcount(3)] void main(point float4 i[1] : SV_Position, uint inst : SV_GSInstanceID, inout TriangleStream<O> s) {
            O o; o.vp = inst; o.c = inst ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1);
            o.p = float4(-1, 1, 0, 1); s.Append(o); o.p = float4(3, 1, 0, 1); s.Append(o); o.p = float4(-1, -3, 0, 1); s.Append(o);
        });
    static const char ps_src[] = HL(float4 main(float4 p : SV_Position, float4 c : COLOR) : SV_Target { return c; });
    ID3D11VertexShader *vs = rx_vs(&r, vs_src); ID3D11GeometryShader *gs = rx_gs(&r, gs_src); ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    if (vs && gs && ps && rx_target(&r, 16, 8, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        D3D11_VIEWPORT vp[2] = { { 0, 0, 8, 8, 0, 1 }, { 8, 0, 8, 8, 0, 1 } };
        ID3D11DeviceContext_RSSetViewports(r.ctx, 2, vp);
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, TRUE, FALSE);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, NULL);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        ID3D11DeviceContext_GSSetShader(r.ctx, gs, NULL, 0);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 1, 0);
        if (rx_read_rt(&r, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 16; x++) {
                uint32_t want = x < 8 ? RGBA8(255, 0, 0, 255) : RGBA8(0, 255, 0, 255);
                if (d3dh_image_pixel32(&im, x, y) != want) bad++;
            }
            CHECKF(bad == 0, "%u pixels not in the viewport selected by SV_ViewportArrayIndex", bad);
            d3dh_image_free(&im);
        }
        UINT n = 0; D3D11_VIEWPORT got[2];
        ID3D11DeviceContext_RSGetViewports(r.ctx, &n, NULL);
        CHECKF(n == 2, "RSGetViewports count %u", n);
        n = 2; ID3D11DeviceContext_RSGetViewports(r.ctx, &n, got);
        CHECK(got[1].TopLeftX == 8 && got[1].Width == 8 && got[0].Width == 8);
    }
    SAFE_RELEASE(ps); SAFE_RELEASE(gs); SAFE_RELEASE(vs);
    rx_close(&r);
}

/* ----------------------------------------------------- tessellation ---- */
static const char VS_PATCH[] = HL(
    struct O { float3 p : POSITION; };
    O main(uint id : SV_VertexID) { O o; o.p = float3(id & 1, (id >> 1) & 1, 0); return o; });
static const char HS_QUAD[] = HL(
    struct HP { float3 p : POSITION; };
    struct PCF { float e[4] : SV_TessFactor; float i[2] : SV_InsideTessFactor; };
    cbuffer T : register(b0) { float4 fac; };
    PCF pcf(InputPatch<HP, 4> ip) { PCF o; o.e[0] = fac.x; o.e[1] = fac.x; o.e[2] = fac.x; o.e[3] = fac.x; o.i[0] = fac.x; o.i[1] = fac.x; return o; }
    [domain("quad")] [partitioning(PART)] [outputtopology("triangle_cw")] [outputcontrolpoints(4)] [patchconstantfunc("pcf")]
    HP main(InputPatch<HP, 4> ip, uint i : SV_OutputControlPointID) { return ip[i]; });
static const char DS_QUAD[] = HL(
    struct HP { float3 p : POSITION; };
    struct PCF { float e[4] : SV_TessFactor; float i[2] : SV_InsideTessFactor; };
    [domain("quad")] float4 main(PCF f, float2 uv : SV_DomainLocation, const OutputPatch<HP, 4> op) : SV_Position {
        float3 p = lerp(lerp(op[0].p, op[1].p, uv.x), lerp(op[2].p, op[3].p, uv.x), uv.y);
        return float4(p.xy * 2 - 1, 0, 1); });
static const char HS_TRI[] = HL(
    struct HP { float3 p : POSITION; };
    struct PCF { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };
    cbuffer T : register(b0) { float4 fac; };
    PCF pcf(InputPatch<HP, 3> ip) { PCF o; o.e[0] = fac.x; o.e[1] = fac.x; o.e[2] = fac.x; o.i = fac.x; return o; }
    [domain("tri")] [partitioning(PART)] [outputtopology("triangle_cw")] [outputcontrolpoints(3)] [patchconstantfunc("pcf")]
    HP main(InputPatch<HP, 3> ip, uint i : SV_OutputControlPointID) { return ip[i]; });
static const char DS_TRI[] = HL(
    struct HP { float3 p : POSITION; };
    struct PCF { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };
    [domain("tri")] float4 main(PCF f, float3 b : SV_DomainLocation, const OutputPatch<HP, 3> op) : SV_Position {
        float3 p = op[0].p * b.x + op[1].p * b.y + op[2].p * b.z;
        return float4(p.xy * 2 - 1, 0, 1); });
static const char GS_TRI_P[] = HL(
    struct V { float4 p : SV_Position; };
    [maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> s) { s.Append(i[0]); s.Append(i[1]); s.Append(i[2]); });
static const D3D11_SO_DECLARATION_ENTRY SO_P[] = { { 0, "SV_Position", 0, 0, 4, 0 } };
static const UINT SO_STRIDE16[1] = { 16 };

/* run one patch through VS/HS/DS and a stream-out GS; returns triangles written (or -1) and storage needed */
static int tess_count(rx *r, const char *hs_src, const char *ds_src, const char *partition, float factor, D3D11_PRIMITIVE_TOPOLOGY topo, int ncp,
                      unsigned long long *needed, unsigned long long *hs_inv, unsigned long long *ds_inv, float **verts, size_t *vsize)
{
    char part[48];
    sprintf(part, "\"%s\"", partition);
    D3D_SHADER_MACRO defs[] = { { "PART", part }, { NULL, NULL } };
    ID3DBlob *hb = rx_blob_ex(defs, hs_src, "hs_5_0", 0);
    ID3D11HullShader *hs = NULL; ID3D11DomainShader *ds = ds_src ? rx_ds(r, ds_src) : NULL;
    ID3D11VertexShader *vs = rx_vs(r, VS_PATCH);
    ID3D11GeometryShader *gs = mk_so(r, GS_TRI_P, SO_P, 1, SO_STRIDE16, 1, D3D11_SO_NO_RASTERIZED_STREAM);
    ID3D11Buffer *cb = rx_cbuf(r, 16), *sob = so_buf(r, 16 * 3 * 9000, 0);
    ID3D11Query *q = rx_make_query(r, D3D11_QUERY_SO_STATISTICS), *pq = rx_make_query(r, D3D11_QUERY_PIPELINE_STATISTICS);
    int result = -1;
    if (hb) EXPECT_HR_OK(ID3D11Device_CreateHullShader(r->dev, d3dh_blob_ptr(hb), d3dh_blob_size(hb), NULL, &hs));
    (void)ncp;
    if (hs && ds && vs && gs && cb && sob && q && pq) {
        float f[4] = { factor, factor, factor, factor }; UINT off = 0;
        D3D11_QUERY_DATA_SO_STATISTICS st; D3D11_QUERY_DATA_PIPELINE_STATISTICS ps;
        ID3D11DeviceContext_UpdateSubresource(r->ctx, (ID3D11Resource *)cb, 0, NULL, f, 0, 0);
        so_begin(r, vs, gs, topo);
        ID3D11DeviceContext_HSSetShader(r->ctx, hs, NULL, 0);
        ID3D11DeviceContext_DSSetShader(r->ctx, ds, NULL, 0);
        ID3D11DeviceContext_HSSetConstantBuffers(r->ctx, 0, 1, &cb);
        ID3D11DeviceContext_SOSetTargets(r->ctx, 1, &sob, &off);
        ID3D11DeviceContext_Begin(r->ctx, (ID3D11Asynchronous *)q);
        ID3D11DeviceContext_Begin(r->ctx, (ID3D11Asynchronous *)pq);
        ID3D11DeviceContext_Draw(r->ctx, ncp, 0);
        ID3D11DeviceContext_End(r->ctx, (ID3D11Asynchronous *)q);
        ID3D11DeviceContext_End(r->ctx, (ID3D11Asynchronous *)pq);
        so_unbind(r);
        ID3D11DeviceContext_HSSetShader(r->ctx, NULL, NULL, 0);
        ID3D11DeviceContext_DSSetShader(r->ctx, NULL, NULL, 0);
        if (rx_query(r, q, &st, sizeof st) == S_OK && rx_query(r, pq, &ps, sizeof ps) == S_OK) {
            result = (int)st.NumPrimitivesWritten;
            if (needed) *needed = st.PrimitivesStorageNeeded;
            if (hs_inv) *hs_inv = ps.HSInvocations;
            if (ds_inv) *ds_inv = ps.DSInvocations;
            if (verts) { *verts = NULL; d3dh_readback_buffer11(r->dev, r->ctx, sob, (void **)verts, vsize); }
        }
    }
    SAFE_RELEASE(q); SAFE_RELEASE(pq); SAFE_RELEASE(sob); SAFE_RELEASE(cb); SAFE_RELEASE(gs); SAFE_RELEASE(vs); SAFE_RELEASE(ds); SAFE_RELEASE(hs); SAFE_RELEASE(hb);
    return result;
}

TEST(d3d11_render_exec_tess_quad_integer_triangle_counts)
{
    rx r; if (!rx_open(&r)) return;
    static const int n[] = { 1, 2, 3, 4, 5, 8, 16 };
    for (unsigned i = 0; i < A(n); i++) {
        unsigned long long need = 0, hsi = 0, dsi = 0;
        int got = tess_count(&r, HS_QUAD, DS_QUAD, "integer", (float)n[i], D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 4, &need, &hsi, &dsi, NULL, NULL);
        if (got < 0) break;
        CHECKF(got == 2 * n[i] * n[i], "integer quad factor %d: %d triangles, want %d", n[i], got, 2 * n[i] * n[i]);
        CHECKF(need == (unsigned long long)got, "storage needed %llu vs written %d", need, got);
        t_note("quad factor %d: HSInvocations %llu DSInvocations %llu", n[i], hsi, dsi);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_tess_quad_pow2_and_fractional_exact_factors)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { const char *part; float f; int tris; int hard; } tab[] = {
        { "pow2", 1, 2, 1 }, { "pow2", 2, 8, 1 }, { "pow2", 4, 32, 1 }, { "pow2", 8, 128, 1 }, { "pow2", 3, 32, 1 }, { "pow2", 5, 128, 1 },
        { "fractional_odd", 1, 2, 1 }, { "fractional_odd", 3, 18, 1 }, { "fractional_odd", 5, 50, 1 }, { "fractional_even", 2, 8, 1 }, { "fractional_even", 4, 32, 1 },
        { "fractional_odd", 2, 18, 0 }, { "fractional_even", 3, 32, 0 },
    };
    for (unsigned i = 0; i < A(tab); i++) {
        unsigned long long need = 0;
        int got = tess_count(&r, HS_QUAD, DS_QUAD, tab[i].part, tab[i].f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 4, &need, NULL, NULL, NULL, NULL);
        if (got < 0) break;
        if (tab[i].hard) CHECKF(got == tab[i].tris, "%s factor %g: %d triangles want %d", tab[i].part, tab[i].f, got, tab[i].tris);
        else if (got != tab[i].tris) t_note("%s factor %g gave %d triangles (expected about %d)", tab[i].part, tab[i].f, got, tab[i].tris);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_tess_quad_max_factor_64)
{
    rx r; if (!rx_open(&r)) return;
    unsigned long long need = 0;
    int got = tess_count(&r, HS_QUAD, DS_QUAD, "integer", 64.0f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 4, &need, NULL, NULL, NULL, NULL);
    if (got >= 0) {
        CHECKF(need == 2 * 64 * 64, "factor 64: %llu triangles generated, want %d", need, 2 * 64 * 64);
        CHECKF(got == 2 * 64 * 64 || got == 8192, "factor 64: %d written", got);
    }
    /* factors above 64 are clamped to 64 */
    got = tess_count(&r, HS_QUAD, DS_QUAD, "integer", 200.0f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 4, &need, NULL, NULL, NULL, NULL);
    if (got >= 0) CHECKF(need == 2 * 64 * 64, "factor 200 clamps to 64: %llu triangles", need);
    rx_close(&r);
}

TEST(d3d11_render_exec_tess_quad_vertices_cover_unit_square_exactly)
{
    rx r; if (!rx_open(&r)) return;
    unsigned long long need = 0; float *v = NULL; size_t sz = 0;
    int got = tess_count(&r, HS_QUAD, DS_QUAD, "integer", 4.0f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 4, &need, NULL, NULL, &v, &sz);
    if (got == 32 && v) {
        /* every output vertex lies on the 5x5 lattice of the [-1,1] square (domain points k/4) and all 25 lattice points are used */
        unsigned seen[25] = { 0 }, bad = 0;
        for (int i = 0; i < 32 * 3; i++) {
            float x = v[i * 4], y = v[i * 4 + 1];
            float gx = (x + 1) * 2, gy = (y + 1) * 2;   /* lattice coordinates 0..4 */
            int ix = (int)floorf(gx + 0.5f), iy = (int)floorf(gy + 0.5f);
            if (fabsf(gx - ix) > 1e-4f || fabsf(gy - iy) > 1e-4f || ix < 0 || ix > 4 || iy < 0 || iy > 4) { bad++; continue; }
            seen[iy * 5 + ix]++;
        }
        unsigned used = 0; for (int i = 0; i < 25; i++) used += seen[i] != 0;
        CHECKF(bad == 0, "%u vertices off the domain lattice", bad);
        CHECKF(used == 25, "%u of 25 lattice points used", used);
        /* all triangles share one winding */
        unsigned pos = 0, neg = 0;
        for (int tr = 0; tr < 32; tr++) {
            const float *a = v + (tr * 3) * 4, *b = v + (tr * 3 + 1) * 4, *c = v + (tr * 3 + 2) * 4;
            float area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
            if (area > 1e-6f) pos++; else if (area < -1e-6f) neg++;
        }
        CHECKF(pos + neg == 32 && (pos == 0 || neg == 0), "winding is inconsistent: %u counter-clockwise, %u clockwise", pos, neg);
        t_note("triangle_cw output appears as %s in y-up NDC when the domain v axis maps to +y", pos ? "counter-clockwise" : "clockwise");
    } else if (got >= 0) CHECKF(0, "expected 32 triangles, got %d", got);
    free(v);
    rx_close(&r);
}

TEST(d3d11_render_exec_tess_tri_domain_factor_one_and_counts_observed)
{
    rx r; if (!rx_open(&r)) return;
    static const struct { float f; const char *part; } tab[] = { { 1, "integer" }, { 2, "integer" }, { 3, "integer" }, { 4, "integer" }, { 3, "fractional_odd" }, { 2, "pow2" } };
    for (unsigned i = 0; i < A(tab); i++) {
        unsigned long long need = 0;
        int got = tess_count(&r, HS_TRI, DS_TRI, tab[i].part, tab[i].f, D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST, 3, &need, NULL, NULL, NULL, NULL);
        if (got < 0) break;
        if (i == 0) CHECKF(got == 1, "triangle domain with all factors 1 produces %d triangles, want 1", got);
        else CHECKF(got >= 1, "triangle domain factor %g produced %d triangles", tab[i].f, got);
        t_note("tri domain %s factor %g -> %d triangles", tab[i].part, tab[i].f, got);
    }
    rx_close(&r);
}

TEST(d3d11_render_exec_tess_patch_count_scales_output)
{
    rx r; if (!rx_open(&r)) return;
    /* 8 control points = two 4-point patches: twice the triangles of one patch */
    unsigned long long need = 0, hs1 = 0, hs2 = 0;
    int one = tess_count(&r, HS_QUAD, DS_QUAD, "integer", 3.0f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 4, &need, &hs1, NULL, NULL, NULL);
    int two = tess_count(&r, HS_QUAD, DS_QUAD, "integer", 3.0f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 8, &need, &hs2, NULL, NULL, NULL);
    if (one > 0 && two > 0) {
        CHECKF(two == 2 * one, "two patches: %d triangles vs one patch %d", two, one);
        CHECKF(hs2 == 2 * hs1, "HS invocations %llu vs %llu", hs2, hs1);
    }
    /* a trailing partial patch (6 control points) emits only the complete patch */
    int six = tess_count(&r, HS_QUAD, DS_QUAD, "integer", 3.0f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 6, &need, NULL, NULL, NULL, NULL);
    if (six > 0 && one > 0) CHECKF(six == one, "6 control points: %d triangles, want %d (incomplete patch dropped)", six, one);
    rx_close(&r);
}

TEST(d3d11_render_exec_tess_zero_factor_culls_patch)
{
    rx r; if (!rx_open(&r)) return;
    unsigned long long need = 99;
    int got = tess_count(&r, HS_QUAD, DS_QUAD, "integer", 0.0f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 4, &need, NULL, NULL, NULL, NULL);
    if (got >= 0) CHECKF(got == 0 && need == 0, "factor 0 must cull the patch: %d written, %llu needed", got, need);
    got = tess_count(&r, HS_QUAD, DS_QUAD, "integer", -3.0f, D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST, 4, &need, NULL, NULL, NULL, NULL);
    if (got >= 0) CHECKF(got == 0, "negative factor must cull the patch: %d written", got);
    rx_close(&r);
}

/* ------------------------------------------------- indirect draws ---- */
TEST(d3d11_render_exec_draw_instanced_indirect_args_and_offsets)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "OFFSET", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 } };
    static const char vs_src[] = HL(float4 main(float2 p : POSITION, float2 o : OFFSET) : SV_Position { return float4(p + o * float2(0.25, -0.5), 0, 1); });
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, vs_src, el, 2, &il);
    float v[8];   /* four vertices along the first row of an 8x4 target */
    for (int k = 0; k < 4; k++) { v[k * 2] = (k + 0.5f) / 8 * 2 - 1; v[k * 2 + 1] = 0.75f; }
    static const float inst[8] = { 0, 0, 0, 1, 0, 2, 0, 3 };   /* instance i moves down by i rows */
    uint32_t args[4 + 8 + 4] = { 0xDEAD, 0xBEEF, 0xF00D, 0xCAFE,   3, 2, 1, 1,   4, 4, 0, 0,   2, 1, 2, 3 };
    ID3D11Buffer *vb0 = rx_buffer(&r, sizeof v, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, v);
    ID3D11Buffer *vb1 = rx_buffer(&r, sizeof inst, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, inst);
    ID3D11Buffer *ab = rx_buffer(&r, sizeof args, 0, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS, 0, D3D11_USAGE_DEFAULT, 0, args);
    if (vs && il && vb0 && vb1 && ab && rx_target(&r, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        ID3D11Buffer *vbs[2] = { vb0, vb1 }; UINT st[2] = { 8, 8 }, off[2] = { 0, 0 };
        /* args at byte 16: vertexCount 3, instanceCount 2, start vertex 1, start instance 1 -> vertices 1..3 on rows 1 and 2
           args at byte 32: vertexCount 4, instanceCount 4, 0, 0 -> 4 x 4 grid
           args at byte 48: vertexCount 2, instanceCount 1, start vertex 2, start instance 3 -> vertices 2,3 on row 3 */
        static const struct { UINT off; unsigned want[4]; } tab[] = {
            { 16, { 0, 0x0E, 0x0E, 0 } }, { 32, { 0x0F, 0x0F, 0x0F, 0x0F } }, { 48, { 0, 0, 0, 0x0C } },
        };
        ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 2, vbs, st, off);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        for (unsigned i = 0; i < A(tab); i++) {
            d3dh_image im; unsigned bad = 0;
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_DrawInstancedIndirect(r.ctx, ab, tab[i].off);
            if (!rx_read_rt(&r, &im)) break;
            for (UINT y = 0; y < 4; y++) {
                unsigned m = 0;
                for (UINT x = 0; x < 8; x++) if (d3dh_image_pixel32(&im, x, y)) m |= 1u << x;
                if (m != tab[i].want[y]) bad++;
            }
            CHECKF(bad == 0, "DrawInstancedIndirect at byte %u: %u rows differ from the predicted masks", tab[i].off, bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(ab); SAFE_RELEASE(vb0); SAFE_RELEASE(vb1); SAFE_RELEASE(il); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_draw_indirect_vertex_selection)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el0[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    static const char vs_src[] = HL(float4 main(float2 p : POSITION) : SV_Position { return float4(p, 0, 1); });
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, vs_src, el0, 1, &il);
    float v[32]; points16(v);
    uint32_t args[] = { 5, 1, 0, 0,    4, 1, 6, 0,    16, 1, 0, 0,    0, 1, 0, 0,    3, 1, 13, 0 };
    ID3D11Buffer *vb = rx_buffer(&r, sizeof v, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, v);
    ID3D11Buffer *ab = rx_buffer(&r, sizeof args, 0, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS, 0, D3D11_USAGE_DEFAULT, 0, args);
    if (vs && il && vb && ab && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        UINT st = 8, off = 0;
        static const unsigned want[5] = { 0x001F, 0x03C0, 0xFFFF, 0, 0xE000 };
        ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &vb, &st, &off);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        for (unsigned i = 0; i < 5; i++) {
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_DrawInstancedIndirect(r.ctx, ab, i * 16);
            unsigned m = rowmask(&r);
            CHECKF(m == want[i], "args record %u: mask %04x want %04x", i, m, want[i]);
        }
    }
    SAFE_RELEASE(ab); SAFE_RELEASE(vb); SAFE_RELEASE(il); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_draw_indexed_instanced_indirect_args)
{
    rx r; if (!rx_open(&r)) return;
    static const D3D11_INPUT_ELEMENT_DESC el0[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    static const char vs_src[] = HL(float4 main(float2 p : POSITION) : SV_Position { return float4(p, 0, 1); });
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, vs_src, el0, 1, &il);
    float v[32]; points16(v);
    static const uint16_t idx[8] = { 1, 3, 5, 7, 9, 11, 13, 15 };
    /* IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation */
    int32_t args[] = { 3, 1, 0, 0, 0,    2, 1, 4, 0, 0,    3, 1, 1, -1, 0,    4, 1, 0, 2, 7,    0, 1, 0, 0, 0 };
    ID3D11Buffer *vb = rx_buffer(&r, sizeof v, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, v);
    ID3D11Buffer *ib = rx_buffer(&r, sizeof idx, D3D11_BIND_INDEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, idx);
    ID3D11Buffer *ab = rx_buffer(&r, sizeof args, 0, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS, 0, D3D11_USAGE_DEFAULT, 0, args);
    if (vs && il && vb && ib && ab && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        UINT st = 8, off = 0;
        /* 1,3,5 | 9,11 | (3,5,7)-1 = 2,4,6 | (1,3,5,7)+2 = 3,5,7,9 | none */
        unsigned want[5];
        want[0] = (1u << 1) | (1u << 3) | (1u << 5);
        want[1] = (1u << 9) | (1u << 11);
        want[2] = (1u << 2) | (1u << 4) | (1u << 6);
        want[3] = (1u << 3) | (1u << 5) | (1u << 7) | (1u << 9);
        want[4] = 0;
        ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &vb, &st, &off);
        ID3D11DeviceContext_IASetIndexBuffer(r.ctx, ib, DXGI_FORMAT_R16_UINT, 0);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        for (unsigned i = 0; i < 5; i++) {
            rx_clear(&r, 0, 0, 0, 0);
            ID3D11DeviceContext_DrawIndexedInstancedIndirect(r.ctx, ab, i * 20);
            unsigned m = rowmask(&r);
            CHECKF(m == want[i], "args record %u: mask %04x want %04x", i, m, want[i]);
        }
    }
    SAFE_RELEASE(ab); SAFE_RELEASE(ib); SAFE_RELEASE(vb); SAFE_RELEASE(il); SAFE_RELEASE(vs);
    rx_close(&r);
}

TEST(d3d11_render_exec_draw_indirect_args_from_compute)
{
    rx r; if (!rx_open(&r)) return;
    static const char cs_src[] = HL(
        RWByteAddressBuffer a : register(u0);
        [numthreads(1, 1, 1)] void main() { a.Store4(0, uint4(6, 1, 3, 0)); });
    static const D3D11_INPUT_ELEMENT_DESC el0[] = { { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    static const char vs_src[] = HL(float4 main(float2 p : POSITION) : SV_Position { return float4(p, 0, 1); });
    ID3D11ComputeShader *cs = rx_cs(&r, cs_src);
    ID3D11InputLayout *il = NULL;
    ID3D11VertexShader *vs = rx_vs_il(&r, vs_src, el0, 1, &il);
    float v[32]; points16(v);
    ID3D11Buffer *vb = rx_buffer(&r, sizeof v, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DEFAULT, 0, v);
    ID3D11Buffer *ab = rx_buffer(&r, 16, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, D3D11_USAGE_DEFAULT, 0, NULL);
    ID3D11UnorderedAccessView *u = NULL;
    if (ab) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_TYPELESS; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; d.Buffer.NumElements = 4; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r.dev, (ID3D11Resource *)ab, &d, &u));
    }
    if (cs && vs && il && vb && u && rx_target(&r, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        UINT st = 8, off = 0; ID3D11UnorderedAccessView *none = NULL;
        ID3D11DeviceContext_CSSetShader(r.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetUnorderedAccessViews(r.ctx, 0, 1, &u, NULL);
        ID3D11DeviceContext_Dispatch(r.ctx, 1, 1, 1);
        ID3D11DeviceContext_CSSetUnorderedAccessViews(r.ctx, 0, 1, &none, NULL);
        ID3D11DeviceContext_IASetInputLayout(r.ctx, il);
        ID3D11DeviceContext_IASetPrimitiveTopology(r.ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ID3D11DeviceContext_IASetVertexBuffers(r.ctx, 0, 1, &vb, &st, &off);
        ID3D11DeviceContext_VSSetShader(r.ctx, vs, NULL, 0);
        rx_set_color(&r, RX_WHITE);
        ID3D11DeviceContext_DrawInstancedIndirect(r.ctx, ab, 0);
        unsigned m = rowmask(&r);
        CHECKF(m == 0x1F8, "GPU-written draw args (6 vertices from 3): mask %04x want 01f8", m);
    }
    SAFE_RELEASE(u); SAFE_RELEASE(ab); SAFE_RELEASE(vb); SAFE_RELEASE(il); SAFE_RELEASE(vs); SAFE_RELEASE(cs);
    rx_close(&r);
}

/* ------------------------------------------------ UAV-only rendering ---- */
TEST(d3d11_render_exec_pixel_shader_uav_only_rendering)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        RWTexture2D<uint> u : register(u0);
        void main(float4 p : SV_Position) { uint old; InterlockedAdd(u[uint2(p.xy)], 1, old); });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    ID3D11Texture2D *t = rx_tex2d(&r, 8, 8, 1, 1, DXGI_FORMAT_R32_UINT, 1, D3D11_BIND_UNORDERED_ACCESS, 0, NULL);
    ID3D11UnorderedAccessView *u = NULL;
    if (t) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_UINT; d.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r.dev, (ID3D11Resource *)t, &d, &u));
    }
    if (ps && u) {
        static const UINT zero[4] = { 0, 0, 0, 0 };
        d3dh_image im;
        r.w = r.h = 8;
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(r.ctx, u, zero);
        ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(r.ctx, 0, NULL, NULL, 0, 1, &u, NULL);
        rx_viewport(&r, 0, 0, 8, 8, 0, 1);
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, TRUE, FALSE);
        rx_full_vs(&r, 0);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);
        if (rx_read(&r, t, 0, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) if (d3dh_image_pixel32(&im, x, y) != 1) bad++;
            CHECKF(bad == 0, "%u pixels not shaded exactly once (the shared diagonal edge must not double-shade)", bad);
            d3dh_image_free(&im);
        }
        /* partial rectangle, second pass accumulates */
        rx_rect_vs(&r, 2, 2, 6, 5, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);
        if (rx_read(&r, t, 0, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
                uint32_t want = 1 + (x >= 2 && x < 6 && y >= 2 && y < 5);
                if (d3dh_image_pixel32(&im, x, y) != want) bad++;
            }
            CHECKF(bad == 0, "%u pixels wrong after the second pass", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(u); SAFE_RELEASE(t); SAFE_RELEASE(ps);
    rx_close(&r);
}

TEST(d3d11_render_exec_pixel_shader_uav_with_render_target)
{
    rx r; if (!rx_open(&r)) return;
    static const char ps_src[] = HL(
        RWByteAddressBuffer o : register(u1);
        float4 main(float4 p : SV_Position) : SV_Target { uint old; o.InterlockedAdd(0, 1, old); o.InterlockedAdd(4, (uint)p.x, old); return float4(1, 0, 0, 1); });
    ID3D11PixelShader *ps = rx_ps(&r, ps_src);
    uint32_t zero[2] = { 0, 0 };
    ID3D11Buffer *b = rx_buffer(&r, 8, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, D3D11_USAGE_DEFAULT, 0, zero);
    ID3D11UnorderedAccessView *u = NULL;
    if (b) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R32_TYPELESS; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; d.Buffer.NumElements = 2; d.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        EXPECT_HR_OK(ID3D11Device_CreateUnorderedAccessView(r.dev, (ID3D11Resource *)b, &d, &u));
    }
    if (ps && u && rx_target(&r, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        d3dh_image im;
        ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(r.ctx, 1, &r.rtv, NULL, 1, 1, &u, NULL);
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, TRUE, FALSE);
        rx_full_vs(&r, 0);
        ID3D11DeviceContext_PSSetShader(r.ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(r.ctx, 4, 0);
        if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, RGBA8(255, 0, 0, 255)); d3dh_image_free(&im); }
        uint32_t *o = NULL; size_t sz;
        if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, b, (void **)&o, &sz))) {
            CHECKF(o[0] == 32, "invocations that wrote the UAV: %u want 32", o[0]);
            CHECKF(o[1] == 4 * (0 + 1 + 2 + 3 + 4 + 5 + 6 + 7), "sum of pixel x: %u want 112", o[1]);
            free(o);
        }
        /* binding UAVs through OMSet... replaces the render targets: reading them back */
        ID3D11RenderTargetView *got = NULL; ID3D11UnorderedAccessView *gu = NULL;
        ID3D11DeviceContext_OMGetRenderTargetsAndUnorderedAccessViews(r.ctx, 1, &got, NULL, 1, 1, &gu);
        CHECK(got == r.rtv && gu == u);
        SAFE_RELEASE(got); SAFE_RELEASE(gu);
    }
    SAFE_RELEASE(u); SAFE_RELEASE(b); SAFE_RELEASE(ps);
    rx_close(&r);
}

/* -------------------------------------------------- deferred contexts ---- */
TEST(d3d11_render_exec_deferred_context_records_and_replays)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR_OK(ID3D11Device_CreateDeferredContext(r.dev, 0, &dc));
    if (dc && rx_target(&r, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        ID3D11DeviceContext *imm = r.ctx; d3dh_image im;
        D3D11_BLEND_DESC bd; ID3D11BlendState *bs = NULL;
        rx_blend_default(&bd);
        bd.RenderTarget[0].BlendEnable = TRUE; bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        EXPECT_HR_OK(ID3D11Device_CreateBlendState(r.dev, &bd, &bs));
        rx_set_rs(&r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
        r.ctx = dc;
        ID3D11DeviceContext_OMSetRenderTargets(dc, 1, &r.rtv, NULL);
        rx_viewport(&r, 0, 0, 8, 8, 0, 1);
        ID3D11DeviceContext_RSSetState(dc, r.rs);
        ID3D11DeviceContext_OMSetBlendState(dc, bs, RX_ZERO, 0xFFFFFFFFu);
        ID3D11DeviceContext_ClearRenderTargetView(dc, r.rtv, RX_ZERO);
        { static const float one[4] = { 10 / 255.f, 20 / 255.f, 30 / 255.f, 40 / 255.f }; rx_full(&r, 0, one); }
        r.ctx = imm;
        EXPECT_HR_OK(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl));
        if (cl) {
            /* nothing has executed yet */
            rx_clear(&r, 1, 1, 1, 1);
            if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, 0xFFFFFFFFu); d3dh_image_free(&im); }
            ID3D11DeviceContext_ExecuteCommandList(imm, cl, TRUE);
            if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, RGBA8(10, 20, 30, 40)); d3dh_image_free(&im); }
            /* command lists can be executed repeatedly; the clear inside restarts from zero each time */
            ID3D11DeviceContext_ExecuteCommandList(imm, cl, TRUE);
            if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, RGBA8(10, 20, 30, 40)); d3dh_image_free(&im); }
        }
        SAFE_RELEASE(bs);
    }
    SAFE_RELEASE(cl); SAFE_RELEASE(dc);
    rx_close(&r);
}

TEST(d3d11_render_exec_execute_command_list_restore_context_state)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR_OK(ID3D11Device_CreateDeferredContext(r.dev, 0, &dc));
    if (dc && rx_target(&r, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        ID3D11RenderTargetView *got = NULL;
        ID3D11DeviceContext_ClearRenderTargetView(dc, r.rtv, RX_RED);
        EXPECT_HR_OK(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl));
        if (cl) {
            ID3D11DeviceContext_ExecuteCommandList(r.ctx, cl, TRUE);
            ID3D11DeviceContext_OMGetRenderTargets(r.ctx, 1, &got, NULL);
            CHECKF(got == r.rtv, "RestoreContextState TRUE must keep the immediate context bindings");
            SAFE_RELEASE(got);
            ID3D11DeviceContext_ExecuteCommandList(r.ctx, cl, FALSE);
            ID3D11DeviceContext_OMGetRenderTargets(r.ctx, 1, &got, NULL);
            CHECKF(got == NULL, "RestoreContextState FALSE must reset the immediate context to the default state");
            SAFE_RELEASE(got);
            d3dh_image im;
            if (rx_read_rt(&r, &im)) { rx_expect_all32(&im, RGBA8(255, 0, 0, 255)); d3dh_image_free(&im); }
        }
    }
    SAFE_RELEASE(cl); SAFE_RELEASE(dc);
    rx_close(&r);
}

TEST(d3d11_render_exec_deferred_context_map_discard_dynamic_buffer)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11DeviceContext *dc = NULL; ID3D11CommandList *cl = NULL;
    EXPECT_HR_OK(ID3D11Device_CreateDeferredContext(r.dev, 0, &dc));
    ID3D11Buffer *dyn = rx_buffer(&r, 64, D3D11_BIND_VERTEX_BUFFER, 0, 0, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL);
    ID3D11Buffer *dst = rx_buffer(&r, 64, D3D11_BIND_SHADER_RESOURCE, 0, 0, D3D11_USAGE_DEFAULT, 0, NULL);
    if (dc && dyn && dst) {
        D3D11_MAPPED_SUBRESOURCE m;
        HRESULT hr = ID3D11DeviceContext_Map(dc, (ID3D11Resource *)dyn, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
        EXPECT_HR_OK(hr);
        if (SUCCEEDED(hr)) {
            for (int i = 0; i < 16; i++) ((uint32_t *)m.pData)[i] = 0x1000u + i;
            ID3D11DeviceContext_Unmap(dc, (ID3D11Resource *)dyn, 0);
            ID3D11DeviceContext_CopyResource(dc, (ID3D11Resource *)dst, (ID3D11Resource *)dyn);
            EXPECT_HR_OK(ID3D11DeviceContext_FinishCommandList(dc, FALSE, &cl));
            if (cl) {
                uint32_t *o = NULL; size_t sz;
                ID3D11DeviceContext_ExecuteCommandList(r.ctx, cl, FALSE);
                if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, dst, (void **)&o, &sz))) {
                    unsigned bad = 0; for (int i = 0; i < 16; i++) if (o[i] != 0x1000u + i) bad++;
                    CHECKF(bad == 0, "%u dwords differ after executing the recorded discard-map + copy", bad);
                    free(o);
                }
            }
        }
        /* NO_OVERWRITE map is rejected on a non-dynamic-index/vertex buffer type only; WRITE_NO_OVERWRITE on dynamic VB is valid on deferred contexts */
        hr = ID3D11DeviceContext_Map(dc, (ID3D11Resource *)dyn, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &m);
        EXPECT_HR_OK(hr);
        if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(dc, (ID3D11Resource *)dyn, 0);
        /* READ is not allowed on a deferred context */
        hr = ID3D11DeviceContext_Map(dc, (ID3D11Resource *)dst, 0, D3D11_MAP_READ, 0, &m);
        EXPECT_HR_FAILED(hr);
    }
    SAFE_RELEASE(cl); SAFE_RELEASE(dyn); SAFE_RELEASE(dst); SAFE_RELEASE(dc);
    rx_close(&r);
}

/* ------------------------------------------------------ copies ---- */
TEST(d3d11_render_exec_update_subresource_box_and_pitch)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11Texture2D *t = rx_tex2d(&r, 8, 6, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_SHADER_RESOURCE, 0, NULL);
    if (t) {
        uint32_t src[3 * 4];   /* 3 columns x 2 rows, deliberately padded rows of 4 texels */
        D3D11_BOX box = { 2, 1, 0, 5, 3, 1 };
        d3dh_image im;
        uint32_t zero[8 * 6]; memset(zero, 0, sizeof zero);
        ID3D11DeviceContext_UpdateSubresource(r.ctx, (ID3D11Resource *)t, 0, NULL, zero, 8 * 4, 0);
        for (int y = 0; y < 2; y++) for (int x = 0; x < 4; x++) src[y * 4 + x] = 0xA0000000u | (y << 8) | x;
        ID3D11DeviceContext_UpdateSubresource(r.ctx, (ID3D11Resource *)t, 0, &box, src, 16, 0);
        if (rx_read(&r, t, 0, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 6; y++) for (UINT x = 0; x < 8; x++) {
                int in = x >= 2 && x < 5 && y >= 1 && y < 3;
                uint32_t want = in ? (0xA0000000u | ((y - 1) << 8) | (x - 2)) : 0u;
                if (d3dh_image_pixel32(&im, x, y) != want) bad++;
            }
            CHECKF(bad == 0, "%u texels wrong after a boxed UpdateSubresource", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(t);
    rx_close(&r);
}

TEST(d3d11_render_exec_copy_subresource_region_texture_and_buffer)
{
    rx r; if (!rx_open(&r)) return;
    uint32_t src[8 * 8], dstinit[8 * 8];
    for (int i = 0; i < 64; i++) { src[i] = 0x01000000u + i; dstinit[i] = 0xEE000000u; }
    D3D11_SUBRESOURCE_DATA s1 = { src, 32, 0 }, s2 = { dstinit, 32, 0 };
    ID3D11Texture2D *a = rx_tex2d(&r, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_SHADER_RESOURCE, 0, &s1);
    ID3D11Texture2D *b = rx_tex2d(&r, 8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_SHADER_RESOURCE, 0, &s2);
    if (a && b) {
        D3D11_BOX box = { 1, 2, 0, 4, 5, 1 }; d3dh_image im;
        ID3D11DeviceContext_CopySubresourceRegion(r.ctx, (ID3D11Resource *)b, 0, 4, 3, 0, (ID3D11Resource *)a, 0, &box);
        if (rx_read(&r, b, 0, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 8; y++) for (UINT x = 0; x < 8; x++) {
                int in = x >= 4 && x < 7 && y >= 3 && y < 6;
                uint32_t want = in ? src[(y - 3 + 2) * 8 + (x - 4 + 1)] : 0xEE000000u;
                if (d3dh_image_pixel32(&im, x, y) != want) bad++;
            }
            CHECKF(bad == 0, "%u texels wrong after CopySubresourceRegion", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    uint32_t bs[16], bd[16];
    for (int i = 0; i < 16; i++) { bs[i] = i + 1; bd[i] = 0xFFFFFFFFu; }
    ID3D11Buffer *x = rx_buffer(&r, 64, D3D11_BIND_SHADER_RESOURCE, 0, 0, D3D11_USAGE_DEFAULT, 0, bs), *y = rx_buffer(&r, 64, D3D11_BIND_SHADER_RESOURCE, 0, 0, D3D11_USAGE_DEFAULT, 0, bd);
    if (x && y) {
        D3D11_BOX box = { 8, 0, 0, 24, 1, 1 };   /* bytes 8..24 */
        uint32_t *o = NULL; size_t sz;
        ID3D11DeviceContext_CopySubresourceRegion(r.ctx, (ID3D11Resource *)y, 0, 32, 0, 0, (ID3D11Resource *)x, 0, &box);
        if (SUCCEEDED(d3dh_readback_buffer11(r.dev, r.ctx, y, (void **)&o, &sz))) {
            for (int i = 0; i < 16; i++) {
                uint32_t want = (i >= 8 && i < 12) ? (uint32_t)(i - 8 + 3) : 0xFFFFFFFFu;
                CHECKF(o[i] == want, "buffer dword %d = %08x want %08x", i, o[i], want);
            }
            free(o);
        }
    }
    SAFE_RELEASE(x); SAFE_RELEASE(y);
    rx_close(&r);
}

TEST(d3d11_render_exec_copy_resource_between_typeless_and_typed)
{
    rx r; if (!rx_open(&r)) return;
    uint32_t src[16]; for (int i = 0; i < 16; i++) src[i] = 0x80402010u + i;
    D3D11_SUBRESOURCE_DATA sd = { src, 16, 0 };
    ID3D11Texture2D *a = rx_tex2d(&r, 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
    ID3D11Texture2D *b = rx_tex2d(&r, 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, 1, D3D11_BIND_SHADER_RESOURCE, 0, NULL);
    ID3D11Texture2D *c = rx_tex2d(&r, 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UINT, 1, D3D11_BIND_SHADER_RESOURCE, 0, NULL);
    if (a && b && c) {
        d3dh_image im;
        ID3D11DeviceContext_CopyResource(r.ctx, (ID3D11Resource *)b, (ID3D11Resource *)a);
        ID3D11DeviceContext_CopyResource(r.ctx, (ID3D11Resource *)c, (ID3D11Resource *)b);
        if (rx_read(&r, c, 0, &im)) {
            unsigned bad = 0;
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++) if (d3dh_image_pixel32(&im, x, y) != src[y * 4 + x]) bad++;
            CHECKF(bad == 0, "%u texels differ after UNORM -> TYPELESS -> UINT copies", bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c);
    rx_close(&r);
}

TEST(d3d11_render_exec_generate_mips_box_filter_values)
{
    rx r; if (!rx_open(&r)) return;
    UINT sup = 0;
    ID3D11Device_CheckFormatSupport(r.dev, DXGI_FORMAT_R8G8B8A8_UNORM, &sup);
    if (!(sup & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN)) { t_skip("R8G8B8A8_UNORM does not support mip autogeneration"); rx_close(&r); return; }
    uint32_t base[8 * 8];
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) base[y * 8 + x] = ((x ^ y) & 1) ? RGBA8(200, 100, 0, 200) : 0u;   /* checkerboard: every 2x2 block averages to half */
    D3D11_SUBRESOURCE_DATA sd[1] = { { base, 32, 0 } };
    ID3D11Texture2D *t = rx_tex2d(&r, 8, 8, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_RESOURCE_MISC_GENERATE_MIPS, NULL);
    ID3D11ShaderResourceView *srv = NULL;
    if (t) {
        ID3D11DeviceContext_UpdateSubresource(r.ctx, (ID3D11Resource *)t, 0, NULL, sd[0].pSysMem, 32, 0);
        EXPECT_HR_OK(ID3D11Device_CreateShaderResourceView(r.dev, (ID3D11Resource *)t, NULL, &srv));
    }
    if (srv) {
        ID3D11DeviceContext_GenerateMips(r.ctx, srv);
        for (UINT m = 1; m < 4; m++) {
            d3dh_image im;
            if (!rx_read(&r, t, m, &im)) continue;
            unsigned bad = 0;
            for (UINT y = 0; y < im.height; y++) for (UINT x = 0; x < im.width; x++) if (!rx_close32(d3dh_image_pixel32(&im, x, y), RGBA8(100, 50, 0, 100), 2)) bad++;
            CHECKF(im.width == (8u >> m) && bad == 0, "mip %u (%ux%u): %u texels not the 2x2 average (100,50,0,100)", m, im.width, im.height, bad);
            d3dh_image_free(&im);
        }
    }
    SAFE_RELEASE(srv); SAFE_RELEASE(t);
    rx_close(&r);
}
