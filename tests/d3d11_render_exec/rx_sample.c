/* Texture sampling and format decoding through a 1-pixel probe render target. */
#include "rx.h"

#define A(a) RX_N(a)

typedef struct { float a[4]; float c[4]; int b[4]; int pad[4]; } pcb;

typedef struct {
    rx r;
    ID3D11Buffer *cb;
    ID3D11PixelShader *ps;
    ID3D11ShaderResourceView *srv;
    ID3D11SamplerState *smp;
} pr;

static void pr_close(pr *p)
{
    SAFE_RELEASE(p->smp); SAFE_RELEASE(p->srv); SAFE_RELEASE(p->ps); SAFE_RELEASE(p->cb);
    rx_close(&p->r);
}
static int pr_open(pr *p, const char *ps_src)
{
    memset(p, 0, sizeof *p);
    if (!rx_open(&p->r)) return 0;
    p->cb = rx_cbuf(&p->r, sizeof(pcb));
    p->ps = rx_ps(&p->r, ps_src);
    if (!p->cb || !p->ps || !rx_target(&p->r, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT)) { pr_close(p); return 0; }
    rx_set_rs(&p->r, D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, TRUE, FALSE);
    ID3D11DeviceContext_PSSetShader(p->r.ctx, p->ps, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(p->r.ctx, 0, 1, &p->cb);
    return 1;
}
static int pr_target(pr *p, UINT w, UINT h)
{
    if (!rx_target(&p->r, w, h, DXGI_FORMAT_R32G32B32A32_FLOAT)) return 0;
    return 1;
}
static void pr_bind(pr *p, ID3D11ShaderResourceView *srv, ID3D11SamplerState *smp)
{
    SAFE_RELEASE(p->srv); SAFE_RELEASE(p->smp);
    p->srv = srv; p->smp = smp;
    ID3D11DeviceContext_PSSetShaderResources(p->r.ctx, 0, 1, &p->srv);
    ID3D11DeviceContext_PSSetSamplers(p->r.ctx, 0, 1, &p->smp);
}
static int pr_run(pr *p, const pcb *cb, float out[4])
{
    d3dh_image im;
    ID3D11DeviceContext_UpdateSubresource(p->r.ctx, (ID3D11Resource *)p->cb, 0, NULL, cb, 0, 0);
    rx_full_vs(&p->r, 0);
    ID3D11DeviceContext_Draw(p->r.ctx, 4, 0);
    if (!rx_read_rt(&p->r, &im)) { out[0] = out[1] = out[2] = out[3] = -12345.0f; return 0; }
    for (int i = 0; i < 4; i++) out[i] = rx_f32(&im, 0, 0, i);
    d3dh_image_free(&im);
    return 1;
}
static pcb pc_uv(float u, float v, float w, float lod)
{
    pcb c; memset(&c, 0, sizeof c);
    c.a[0] = u; c.a[1] = v; c.a[2] = w; c.c[0] = lod;
    return c;
}
static pcb pc_i(int x, int y, int z)
{
    pcb c; memset(&c, 0, sizeof c);
    c.b[0] = x; c.b[1] = y; c.b[2] = z;
    return c;
}

static uint16_t h16(float f)
{
    uint32_t x; memcpy(&x, &f, 4);
    uint32_t s = (x >> 16) & 0x8000u; int e = (int)((x >> 23) & 255) - 127 + 15; uint32_t m = x & 0x7FFFFFu;
    if ((x & 0x7FFFFFFFu) == 0) return (uint16_t)s;
    if (e <= 0) return (uint16_t)s;
    if (e >= 31) return (uint16_t)(s | 0x7C00);
    return (uint16_t)(s | (e << 10) | (m >> 13));
}

static ID3D11SamplerState *mk_samp(rx *r, D3D11_FILTER f, D3D11_TEXTURE_ADDRESS_MODE u, D3D11_TEXTURE_ADDRESS_MODE v, D3D11_TEXTURE_ADDRESS_MODE w,
                                   float bias, D3D11_COMPARISON_FUNC cmp, const float *border, float minlod, float maxlod)
{
    D3D11_SAMPLER_DESC d; ID3D11SamplerState *s = NULL;
    memset(&d, 0, sizeof d);
    d.Filter = f; d.AddressU = u; d.AddressV = v; d.AddressW = w; d.MipLODBias = bias; d.MaxAnisotropy = 1; d.ComparisonFunc = cmp;
    if (border) memcpy(d.BorderColor, border, 16);
    d.MinLOD = minlod; d.MaxLOD = maxlod;
    HRESULT hr = ID3D11Device_CreateSamplerState(r->dev, &d, &s);
    EXPECT_HR_OK(hr);
    return SUCCEEDED(hr) ? s : NULL;
}
static ID3D11SamplerState *samp_point(rx *r, D3D11_TEXTURE_ADDRESS_MODE m, const float *border)
{
    return mk_samp(r, D3D11_FILTER_MIN_MAG_MIP_POINT, m, m, m, 0, D3D11_COMPARISON_NEVER, border, 0, D3D11_FLOAT32_MAX);
}
static ID3D11SamplerState *samp_lin(rx *r, D3D11_TEXTURE_ADDRESS_MODE m)
{
    return mk_samp(r, D3D11_FILTER_MIN_MAG_MIP_LINEAR, m, m, m, 0, D3D11_COMPARISON_NEVER, NULL, 0, D3D11_FLOAT32_MAX);
}
static ID3D11ShaderResourceView *mk_srv(rx *r, ID3D11Resource *res, const D3D11_SHADER_RESOURCE_VIEW_DESC *d)
{
    ID3D11ShaderResourceView *v = NULL;
    HRESULT hr = ID3D11Device_CreateShaderResourceView(r->dev, res, d, &v);
    EXPECT_HR_OK(hr);
    return SUCCEEDED(hr) ? v : NULL;
}

static const char PS_SL2D[] = HL(
    cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
    Texture2D<float4> t : register(t0); SamplerState s : register(s0);
    float4 main(float4 p : SV_Position) : SV_Target { return t.SampleLevel(s, a.xy, c.x); });
static const char PS_LOAD[] = HL(
    cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
    Texture2D<float4> t : register(t0);
    float4 main(float4 p : SV_Position) : SV_Target { return t.Load(int3(b.xyz)); });

/* ----- 4x4 RGBA32F texel (x,y) = (x, y, 4y+x, 1) ----- */
static ID3D11Texture2D *tex4_f32(rx *r)
{
    float d[4 * 4 * 4]; D3D11_SUBRESOURCE_DATA sd;
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { float *t = d + (y * 4 + x) * 4; t[0] = (float)x; t[1] = (float)y; t[2] = (float)(y * 4 + x); t[3] = 1; }
    sd.pSysMem = d; sd.SysMemPitch = 64; sd.SysMemSlicePitch = 0;
    return rx_tex2d(r, 4, 4, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
}

static int addr_index(D3D11_TEXTURE_ADDRESS_MODE m, int i, int n)
{
    /* returns texel index or -1 for border */
    switch (m) {
    case D3D11_TEXTURE_ADDRESS_WRAP: return ((i % n) + n) % n;
    case D3D11_TEXTURE_ADDRESS_MIRROR: { int k = ((i % (2 * n)) + 2 * n) % (2 * n); return k < n ? k : 2 * n - 1 - k; }
    case D3D11_TEXTURE_ADDRESS_CLAMP: return i < 0 ? 0 : i >= n ? n - 1 : i;
    case D3D11_TEXTURE_ADDRESS_MIRROR_ONCE: { int k = i < 0 ? -i - 1 : i; return k >= n ? n - 1 : k; }
    default: return (i < 0 || i >= n) ? -1 : i;
    }
}

TEST(d3d11_render_exec_sample_point_address_modes)
{
    pr p; if (!pr_open(&p, PS_SL2D)) return;
    ID3D11Texture2D *t = tex4_f32(&p.r);
    static const D3D11_TEXTURE_ADDRESS_MODE modes[] = { D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_MIRROR_ONCE };
    static const float border[4] = { 0.125f, 0.25f, 0.375f, 0.5f };
    if (t) {
        for (unsigned m = 0; m < A(modes); m++) {
            ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
            ID3D11SamplerState *smp = samp_point(&p.r, modes[m], border);
            if (!srv || !smp) { SAFE_RELEASE(srv); SAFE_RELEASE(smp); break; }
            pr_bind(&p, srv, smp);
            unsigned bad = 0;
            for (int ku = -9; ku <= 12; ku++) for (int kv = -6; kv <= 9; kv += 3) {
                float o[4]; pcb c = pc_uv((ku + 0.5f) / 4, (kv + 0.5f) / 4, 0, 0);
                if (!pr_run(&p, &c, o)) continue;
                int ix = addr_index(modes[m], ku, 4), iy = addr_index(modes[m], kv, 4);
                float w[4];
                if (ix < 0 || iy < 0) memcpy(w, border, 16); else { w[0] = (float)ix; w[1] = (float)iy; w[2] = (float)(iy * 4 + ix); w[3] = 1; }
                if (memcmp(o, w, 16)) { bad++; if (bad < 4) CHECKF(0, "mode %d texel (%d,%d): got %g %g %g %g want %g %g %g %g", (int)modes[m], ku, kv, o[0], o[1], o[2], o[3], w[0], w[1], w[2], w[3]); }
            }
            CHECKF(bad == 0, "address mode %d: %u mismatching probes", (int)modes[m], bad);
        }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_mixed_address_modes_per_axis)
{
    pr p; if (!pr_open(&p, PS_SL2D)) return;
    ID3D11Texture2D *t = tex4_f32(&p.r);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = mk_samp(&p.r, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, 0, D3D11_COMPARISON_NEVER, NULL, 0, D3D11_FLOAT32_MAX);
        if (srv && smp) {
            pr_bind(&p, srv, smp);
            for (int ku = -5; ku <= 8; ku++) for (int kv = -3; kv <= 6; kv += 3) {
                float o[4]; pcb c = pc_uv((ku + 0.5f) / 4, (kv + 0.5f) / 4, 0, 0);
                if (!pr_run(&p, &c, o)) continue;
                int ix = addr_index(D3D11_TEXTURE_ADDRESS_WRAP, ku, 4), iy = addr_index(D3D11_TEXTURE_ADDRESS_CLAMP, kv, 4);
                CHECKF(o[0] == ix && o[1] == iy, "(%d,%d): got texel (%g,%g) want (%d,%d)", ku, kv, o[0], o[1], ix, iy);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_linear_1d_weights_wrap_clamp)
{
    pr p; if (!pr_open(&p, PS_SL2D)) return;
    uint16_t d[2 * 4] = { h16(0), 0, 0, h16(1), h16(1), 0, 0, h16(1) };  /* texel0 R=0, texel1 R=1 */
    D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = d; sd.SysMemPitch = 16; sd.SysMemSlicePitch = 0;
    ID3D11Texture2D *t = rx_tex2d(&p.r, 2, 1, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
    static const struct { float u, clamp, wrap; } tab[] = {
        { 0.25f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, { 0.75f, 1.0f, 1.0f }, { 0.375f, 0.25f, 0.25f }, { 0.625f, 0.75f, 0.75f },
        { 0.0f, 0.0f, 0.5f }, { 1.0f, 1.0f, 0.5f }, { 0.125f, 0.0f, 0.25f }, { 0.875f, 1.0f, 0.75f }, { -0.25f, 0.0f, 1.0f }, { 1.25f, 1.0f, 0.0f },
    };
    if (t) for (int w = 0; w < 2; w++) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_lin(&p.r, w ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP);
        if (!srv || !smp) { SAFE_RELEASE(srv); SAFE_RELEASE(smp); break; }
        pr_bind(&p, srv, smp);
        for (unsigned i = 0; i < A(tab); i++) {
            float o[4]; pcb c = pc_uv(tab[i].u, 0.5f, 0, 0);
            if (!pr_run(&p, &c, o)) continue;
            float want = w ? tab[i].wrap : tab[i].clamp;
            CHECKF(rx_near(o[0], want, 0.01f), "%s u=%g: got %g want %g", w ? "wrap" : "clamp", tab[i].u, o[0], want);
        }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_bilinear_2x2_quadrants)
{
    pr p; if (!pr_open(&p, PS_SL2D)) return;
    uint16_t d[4 * 4];
    for (int i = 0; i < 4; i++) { d[i * 4] = h16((float)i); d[i * 4 + 1] = 0; d[i * 4 + 2] = 0; d[i * 4 + 3] = h16(1); }
    D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = d; sd.SysMemPitch = 16; sd.SysMemSlicePitch = 0;
    ID3D11Texture2D *t = rx_tex2d(&p.r, 2, 2, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_lin(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP);
        if (srv && smp) {
            pr_bind(&p, srv, smp);
            /* texels: (0,0)=0 (1,0)=1 (0,1)=2 (1,1)=3. value = x + 2y with fractional weights */
            for (int iu = 0; iu <= 8; iu++) for (int iv = 0; iv <= 8; iv++) {
                float u = iu / 8.0f, v = iv / 8.0f, o[4];
                float fx = u * 2 - 0.5f, fy = v * 2 - 0.5f;
                fx = fx < 0 ? 0 : fx > 1 ? 1 : fx; fy = fy < 0 ? 0 : fy > 1 ? 1 : fy;
                pcb c = pc_uv(u, v, 0, 0);
                if (!pr_run(&p, &c, o)) continue;
                CHECKF(rx_near(o[0], fx + 2 * fy, 0.02f), "uv (%g,%g): got %g want %g", u, v, o[0], fx + 2 * fy);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_linear_filters_srgb_in_linear_space)
{
    pr p; if (!pr_open(&p, PS_SL2D)) return;
    UINT sup = 0;
    uint32_t d[2] = { 0xFF000000u, 0xFFFFFFFFu };  /* black, white */
    D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = d; sd.SysMemPitch = 8; sd.SysMemSlicePitch = 0;
    ID3D11Device_CheckFormatSupport(p.r.dev, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, &sup);
    ID3D11Texture2D *t = (sup & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) ? rx_tex2d(&p.r, 2, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd) : NULL;
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_lin(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP);
        if (srv && smp) {
            float o[4]; pcb c = pc_uv(0.5f, 0.5f, 0, 0);
            pr_bind(&p, srv, smp);
            if (pr_run(&p, &c, o)) {
                CHECKF(rx_near(o[0], 0.5f, 0.01f), "midpoint of black/white sRGB texels filters to %g (linear 0.5 expected, 0.735 would mean filtering after decode is missing)", o[0]);
                CHECK(rx_near(o[3], 1.0f, 0.001f));
            }
            c = pc_uv(0.25f, 0.5f, 0, 0);
            if (pr_run(&p, &c, o)) CHECK(o[0] == 0.0f);
            c = pc_uv(0.75f, 0.5f, 0, 0);
            if (pr_run(&p, &c, o)) CHECK(o[0] == 1.0f);
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    } else if (!(sup & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)) t_skip("sRGB texture sampling unsupported");
    SAFE_RELEASE(t);
    pr_close(&p);
}

/* ------------------------------------------------------------- mips ---- */
/* 8x8 texture with 4 mips; every texel of mip m holds R = m + 1 (half precision) */
static ID3D11Texture2D *tex_mips(rx *r, UINT size, UINT levels, DXGI_FORMAT fmt)
{
    D3D11_SUBRESOURCE_DATA sd[8]; uint16_t *bufs[8]; ID3D11Texture2D *t;
    for (UINT m = 0; m < levels; m++) {
        UINT w = size >> m; if (!w) w = 1;
        bufs[m] = (uint16_t *)malloc((size_t)w * w * 8);
        for (UINT i = 0; i < w * w; i++) { bufs[m][i * 4] = h16((float)(m + 1)); bufs[m][i * 4 + 1] = 0; bufs[m][i * 4 + 2] = 0; bufs[m][i * 4 + 3] = h16(1); }
        sd[m].pSysMem = bufs[m]; sd[m].SysMemPitch = w * 8; sd[m].SysMemSlicePitch = 0;
    }
    t = rx_tex2d(r, size, size, levels, 1, fmt, 1, D3D11_BIND_SHADER_RESOURCE, 0, sd);
    for (UINT m = 0; m < levels; m++) free(bufs[m]);
    return t;
}

TEST(d3d11_render_exec_sample_explicit_lod_point_and_trilinear)
{
    pr p; if (!pr_open(&p, PS_SL2D)) return;
    ID3D11Texture2D *t = tex_mips(&p.r, 8, 4, DXGI_FORMAT_R16G16B16A16_FLOAT);
    static const struct { float lod, point, tri; int hard_point; } tab[] = {
        { 0, 1, 1, 1 }, { 1, 2, 2, 1 }, { 2, 3, 3, 1 }, { 3, 4, 4, 1 }, { 5, 4, 4, 1 }, { 100, 4, 4, 1 }, { -3, 1, 1, 1 },
        { 0.25f, 1, 1.25f, 0 }, { 0.75f, 2, 1.75f, 0 }, { 1.5f, 2, 2.5f, 0 }, { 2.25f, 3, 3.25f, 0 }, { 0.4f, 1, 1.4f, 0 }, { 2.9f, 4, 3.9f, 0 },
    };
    if (t) for (int tri = 0; tri < 2; tri++) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = tri ? samp_lin(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP) : samp_point(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP, NULL);
        if (!srv || !smp) { SAFE_RELEASE(srv); SAFE_RELEASE(smp); break; }
        pr_bind(&p, srv, smp);
        for (unsigned i = 0; i < A(tab); i++) {
            float o[4]; pcb c = pc_uv(0.5f, 0.5f, 0, tab[i].lod);
            if (!pr_run(&p, &c, o)) continue;
            if (tri) CHECKF(rx_near(o[0], tab[i].tri, 0.02f), "trilinear lod %g: got %g want %g", tab[i].lod, o[0], tab[i].tri);
            else if (tab[i].hard_point) CHECKF(o[0] == tab[i].point, "point mip filter lod %g: got %g want %g", tab[i].lod, o[0], tab[i].point);
            else if (o[0] != tab[i].point) t_note("point mip filter at lod %g gave mip value %g (nearest expected %g)", tab[i].lod, o[0], tab[i].point);
        }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_min_max_lod_clamp)
{
    pr p; if (!pr_open(&p, PS_SL2D)) return;
    ID3D11Texture2D *t = tex_mips(&p.r, 8, 4, DXGI_FORMAT_R16G16B16A16_FLOAT);
    static const struct { float minl, maxl, lod, want; } tab[] = {
        { 2, 3, 0, 3 }, { 2, 3, 1, 3 }, { 2, 3, 3, 4 }, { 0, 1, 3, 2 }, { 0, 1, 0, 1 }, { 1, 1, 0, 2 }, { 1, 1, 3, 2 }, { 3, 3, 0, 4 }, { 0, 0, 2, 1 },
    };
    if (t) for (unsigned i = 0; i < A(tab); i++) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = mk_samp(&p.r, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, 0, D3D11_COMPARISON_NEVER, NULL, tab[i].minl, tab[i].maxl);
        float o[4]; pcb c = pc_uv(0.5f, 0.5f, 0, tab[i].lod);
        if (!srv || !smp) { SAFE_RELEASE(srv); SAFE_RELEASE(smp); break; }
        pr_bind(&p, srv, smp);
        if (pr_run(&p, &c, o)) CHECKF(o[0] == tab[i].want, "MinLOD %g MaxLOD %g lod %g: got mip value %g want %g", tab[i].minl, tab[i].maxl, tab[i].lod, o[0], tab[i].want);
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

static const char PS_AUTO[] = HL(
    cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
    Texture2D<float4> t : register(t0); SamplerState s : register(s0);
    float4 main(float4 p : SV_Position) : SV_Target {
        float2 uv = p.xy * a.xy;
        return float4(t.Sample(s, uv).x, t.CalculateLevelOfDetail(s, uv), t.CalculateLevelOfDetailUnclamped(s, uv), 0);
    });

TEST(d3d11_render_exec_sample_automatic_lod_from_derivatives)
{
    pr p; if (!pr_open(&p, PS_AUTO)) return;
    ID3D11Texture2D *t = tex_mips(&p.r, 16, 5, DXGI_FORMAT_R16G16B16A16_FLOAT);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP, NULL);
        if (srv && smp) {
            pr_bind(&p, srv, smp);
            static const UINT rts[] = { 16, 8, 4, 2, 1 };
            for (unsigned i = 0; i < A(rts); i++) {
                float o[4]; pcb c; memset(&c, 0, sizeof c);
                if (!pr_target(&p, rts[i], rts[i])) break;
                c.a[0] = c.a[1] = 1.0f / rts[i];
                if (!pr_run(&p, &c, o)) continue;
                CHECKF(o[0] == (float)(i + 1), "%ux%u target onto 16x16 texture: mip value %g want %u", rts[i], rts[i], o[0], i + 1);
                CHECKF(rx_near(o[1], (float)i, 0.02f), "CalculateLevelOfDetail %g want %u", o[1], i);
                CHECKF(rx_near(o[2], (float)i, 0.02f), "CalculateLevelOfDetailUnclamped %g want %u", o[2], i);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_calculate_lod_clamped_vs_unclamped)
{
    pr p; if (!pr_open(&p, PS_AUTO)) return;
    ID3D11Texture2D *t = tex_mips(&p.r, 16, 5, DXGI_FORMAT_R16G16B16A16_FLOAT);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = mk_samp(&p.r, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, 0, D3D11_COMPARISON_NEVER, NULL, 0, 1);
        if (srv && smp) {
            float o[4]; pcb c; memset(&c, 0, sizeof c);
            pr_bind(&p, srv, smp);
            if (pr_target(&p, 2, 2)) {
                c.a[0] = c.a[1] = 0.5f;   /* 16 texels over 2 pixels: lod 3 */
                if (pr_run(&p, &c, o)) {
                    CHECKF(o[0] == 2.0f, "sampled mip value %g (MaxLOD 1 -> mip 1 -> value 2)", o[0]);
                    CHECKF(rx_near(o[1], 1.0f, 0.02f), "clamped lod %g want 1", o[1]);
                    CHECKF(rx_near(o[2], 3.0f, 0.02f), "unclamped lod %g want 3", o[2]);
                }
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

static const char PS_DIMS[] = HL(
    cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
    Texture2D<float4> t : register(t0);
    float4 main(float4 p : SV_Position) : SV_Target { uint w, h, l; t.GetDimensions(b.x, w, h, l); return float4(w, h, l, 0); });

TEST(d3d11_render_exec_getdimensions_through_srv_mip_range)
{
    pr p; if (!pr_open(&p, PS_DIMS)) return;
    ID3D11Texture2D *t = tex_mips(&p.r, 16, 5, DXGI_FORMAT_R16G16B16A16_FLOAT);
    if (t) {
        static const struct { UINT most, levels; int query; float w, h, l; } tab[] = {
            { 0, 5, 0, 16, 16, 5 }, { 0, 5, 1, 8, 8, 5 }, { 0, 5, 4, 1, 1, 5 }, { 1, 3, 0, 8, 8, 3 }, { 1, 3, 1, 4, 4, 3 }, { 2, 1, 0, 4, 4, 1 }, { 3, 2, 1, 1, 1, 2 },
        };
        for (unsigned i = 0; i < A(tab); i++) {
            D3D11_SHADER_RESOURCE_VIEW_DESC d; float o[4]; pcb c = pc_i(tab[i].query, 0, 0);
            memset(&d, 0, sizeof d);
            d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            d.Texture2D.MostDetailedMip = tab[i].most; d.Texture2D.MipLevels = tab[i].levels;
            ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, &d);
            if (!srv) break;
            pr_bind(&p, srv, NULL);
            if (pr_run(&p, &c, o))
                CHECKF(o[0] == tab[i].w && o[1] == tab[i].h && o[2] == tab[i].l, "view mip %u+%u query %d: got %gx%g levels %g want %gx%g levels %g",
                       tab[i].most, tab[i].levels, tab[i].query, o[0], o[1], o[2], tab[i].w, tab[i].h, tab[i].l);
        }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_srv_most_detailed_mip_offsets_lod0)
{
    pr p; if (!pr_open(&p, PS_SL2D)) return;
    ID3D11Texture2D *t = tex_mips(&p.r, 8, 4, DXGI_FORMAT_R16G16B16A16_FLOAT);
    if (t) {
        for (UINT most = 0; most < 4; most++) {
            D3D11_SHADER_RESOURCE_VIEW_DESC d; float o[4]; pcb c = pc_uv(0.5f, 0.5f, 0, 0);
            ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP, NULL);
            memset(&d, 0, sizeof d);
            d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            d.Texture2D.MostDetailedMip = most; d.Texture2D.MipLevels = 4 - most;
            ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, &d);
            if (!srv || !smp) { SAFE_RELEASE(srv); SAFE_RELEASE(smp); break; }
            pr_bind(&p, srv, smp);
            if (pr_run(&p, &c, o)) CHECKF(o[0] == (float)(most + 1), "view starting at mip %u: lod 0 returned %g", most, o[0]);
            c = pc_uv(0.5f, 0.5f, 0, 100.0f);
            if (pr_run(&p, &c, o)) CHECKF(o[0] == 4.0f, "view starting at mip %u: high lod returned %g", most, o[0]);
        }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

/* ------------------------------------------------------------ Load ---- */
TEST(d3d11_render_exec_load_texel_mip_and_out_of_range)
{
    pr p; if (!pr_open(&p, PS_LOAD)) return;
    ID3D11Texture2D *t = tex4_f32(&p.r);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        if (srv) {
            pr_bind(&p, srv, NULL);
            for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
                float o[4]; pcb c = pc_i(x, y, 0);
                if (!pr_run(&p, &c, o)) continue;
                CHECKF(o[0] == x && o[1] == y && o[2] == y * 4 + x && o[3] == 1, "Load(%d,%d) = %g %g %g %g", x, y, o[0], o[1], o[2], o[3]);
            }
            static const int oob[][3] = { { 4, 0, 0 }, { 0, 4, 0 }, { -1, 0, 0 }, { 0, -1, 0 }, { 100, 100, 0 }, { 0, 0, 1 }, { 0, 0, 5 } };
            for (unsigned i = 0; i < A(oob); i++) {
                float o[4]; pcb c = pc_i(oob[i][0], oob[i][1], oob[i][2]);
                if (!pr_run(&p, &c, o)) continue;
                CHECKF(o[0] == 0 && o[1] == 0 && o[2] == 0 && o[3] == 0, "out-of-range Load(%d,%d,mip %d) = %g %g %g %g, want all zero", oob[i][0], oob[i][1], oob[i][2], o[0], o[1], o[2], o[3]);
            }
        } else SAFE_RELEASE(srv);
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_load_mip_levels_distinct)
{
    pr p; if (!pr_open(&p, PS_LOAD)) return;
    ID3D11Texture2D *t = tex_mips(&p.r, 8, 4, DXGI_FORMAT_R16G16B16A16_FLOAT);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        if (srv) {
            pr_bind(&p, srv, NULL);
            for (int m = 0; m < 4; m++) {
                int sz = 8 >> m;
                float o[4]; pcb c = pc_i(sz - 1, sz - 1, m);
                if (pr_run(&p, &c, o)) CHECKF(o[0] == m + 1, "Load mip %d last texel = %g", m, o[0]);
                c = pc_i(sz, 0, m);
                if (pr_run(&p, &c, o)) CHECKF(o[0] == 0 && o[3] == 0, "Load mip %d at x=%d (just outside) = %g", m, sz, o[0]);
            }
        } else SAFE_RELEASE(srv);
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_load_and_sample_with_literal_offsets)
{
    static const char ps_load[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2D<float4> t : register(t0);
        float4 main(float4 p : SV_Position) : SV_Target { return t.Load(int3(b.xyz), int2(1, -1)); });
    static const char ps_samp[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2D<float4> t : register(t0); SamplerState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target { return t.SampleLevel(s, a.xy, 0, int2(-1, 2)); });
    pr p;
    if (!pr_open(&p, ps_load)) return;
    ID3D11Texture2D *t = tex4_f32(&p.r);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_WRAP, NULL);
        if (srv && smp) {
            float o[4]; pcb c = pc_i(1, 2, 0);
            pr_bind(&p, srv, smp);
            if (pr_run(&p, &c, o)) CHECKF(o[0] == 2 && o[1] == 1, "Load((1,2),offset(1,-1)) = (%g,%g) want (2,1)", o[0], o[1]);
            c = pc_i(3, 0, 0);
            if (pr_run(&p, &c, o)) CHECKF(o[0] == 0 && o[3] == 0, "offset load beyond the edge must be 0, got %g", o[0]);
            /* SampleLevel with offset, wrap addressing applies after the offset */
            ID3D11PixelShader *ps2 = rx_ps(&p.r, ps_samp);
            if (ps2) {
                ID3D11DeviceContext_PSSetShader(p.r.ctx, ps2, NULL, 0);
                c = pc_uv(0.125f, 0.125f, 0, 0);   /* texel (0,0) + (-1,2) = (-1,2) -> wraps to (3,2) */
                if (pr_run(&p, &c, o)) CHECKF(o[0] == 3 && o[1] == 2, "SampleLevel offset (-1,2) from texel (0,0): got (%g,%g) want (3,2)", o[0], o[1]);
                ID3D11DeviceContext_PSSetShader(p.r.ctx, p.ps, NULL, 0);
                SAFE_RELEASE(ps2);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

/* ------------------------------------------------- arrays, cube, 3D ---- */
TEST(d3d11_render_exec_sample_texture_array_slice_selection)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2DArray<float4> t : register(t0); SamplerState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target { return t.SampleLevel(s, a.xyz, 0); });
    pr p; if (!pr_open(&p, ps)) return;
    uint16_t d[4][1 * 4];
    D3D11_SUBRESOURCE_DATA sd[4];
    for (int i = 0; i < 4; i++) { d[i][0] = h16(i * 10 + 1.0f); d[i][1] = 0; d[i][2] = 0; d[i][3] = h16(1); sd[i].pSysMem = d[i]; sd[i].SysMemPitch = 8; sd[i].SysMemSlicePitch = 0; }
    ID3D11Texture2D *t = rx_tex2d(&p.r, 1, 1, 1, 4, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D11_BIND_SHADER_RESOURCE, 0, sd);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP, NULL);
        if (srv && smp) {
            static const struct { float idx; int slice; } tab[] = { { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 }, { 1.4f, 1 }, { 2.6f, 3 }, { 0.3f, 0 }, { 7, 3 }, { 100, 3 }, { -2, 0 }, { -0.4f, 0 } };
            pr_bind(&p, srv, smp);
            for (unsigned i = 0; i < A(tab); i++) {
                float o[4]; pcb c = pc_uv(0.5f, 0.5f, tab[i].idx, 0);
                if (pr_run(&p, &c, o)) CHECKF(o[0] == tab[i].slice * 10 + 1, "array index %g: got %g want slice %d", tab[i].idx, o[0], tab[i].slice);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_cube_face_selection)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        TextureCube<float4> t : register(t0); SamplerState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target { return t.SampleLevel(s, a.xyz, 0); });
    pr p; if (!pr_open(&p, ps)) return;
    uint16_t d[6][4];
    D3D11_SUBRESOURCE_DATA sd[6];
    for (int i = 0; i < 6; i++) { d[i][0] = h16(i + 1.0f); d[i][1] = 0; d[i][2] = 0; d[i][3] = h16(1); sd[i].pSysMem = d[i]; sd[i].SysMemPitch = 8; sd[i].SysMemSlicePitch = 0; }
    ID3D11Texture2D *t = rx_tex2d(&p.r, 1, 1, 1, 6, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_TEXTURECUBE, sd);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP, NULL);
        if (srv && smp) {
            static const struct { float v[3]; int face; } tab[] = {
                { { 1, 0, 0 }, 0 }, { { -1, 0, 0 }, 1 }, { { 0, 1, 0 }, 2 }, { { 0, -1, 0 }, 3 }, { { 0, 0, 1 }, 4 }, { { 0, 0, -1 }, 5 },
                { { 1, 0.9f, 0.8f }, 0 }, { { -0.3f, 0.2f, 0.1f }, 1 }, { { 0.5f, 3, -2 }, 2 }, { { 0.5f, -3, 2 }, 3 }, { { 0.1f, 0.2f, 5 }, 4 }, { { -0.1f, 0.2f, -5 }, 5 },
                { { 100, 1, 1 }, 0 }, { { -0.01f, -0.005f, 0.004f }, 1 },
            };
            pr_bind(&p, srv, smp);
            for (unsigned i = 0; i < A(tab); i++) {
                float o[4]; pcb c = pc_uv(tab[i].v[0], tab[i].v[1], tab[i].v[2], 0);
                if (pr_run(&p, &c, o)) CHECKF(o[0] == tab[i].face + 1, "direction (%g,%g,%g): face value %g want %d", tab[i].v[0], tab[i].v[1], tab[i].v[2], o[0], tab[i].face + 1);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_texture3d_voxels_and_wrap)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture3D<float4> t : register(t0); SamplerState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target { return t.SampleLevel(s, a.xyz, 0); });
    pr p; if (!pr_open(&p, ps)) return;
    float d[4 * 4 * 4 * 4];
    for (int z = 0; z < 4; z++) for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { float *v = d + ((z * 4 + y) * 4 + x) * 4; v[0] = (float)x; v[1] = (float)y; v[2] = (float)z; v[3] = 1; }
    D3D11_TEXTURE3D_DESC td; D3D11_SUBRESOURCE_DATA sd; ID3D11Texture3D *t = NULL;
    memset(&td, 0, sizeof td);
    td.Width = td.Height = td.Depth = 4; td.MipLevels = 1; td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sd.pSysMem = d; sd.SysMemPitch = 64; sd.SysMemSlicePitch = 256;
    EXPECT_HR_OK(ID3D11Device_CreateTexture3D(p.r.dev, &td, &sd, &t));
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_WRAP, NULL);
        if (srv && smp) {
            pr_bind(&p, srv, smp);
            for (int kz = -2; kz <= 5; kz++) for (int ky = -1; ky <= 4; ky += 2) for (int kx = -1; kx <= 4; kx += 3) {
                float o[4]; pcb c = pc_uv((kx + 0.5f) / 4, (ky + 0.5f) / 4, (kz + 0.5f) / 4, 0);
                if (!pr_run(&p, &c, o)) continue;
                int ix = addr_index(D3D11_TEXTURE_ADDRESS_WRAP, kx, 4), iy = addr_index(D3D11_TEXTURE_ADDRESS_WRAP, ky, 4), iz = addr_index(D3D11_TEXTURE_ADDRESS_WRAP, kz, 4);
                CHECKF(o[0] == ix && o[1] == iy && o[2] == iz, "voxel (%d,%d,%d): got (%g,%g,%g) want (%d,%d,%d)", kx, ky, kz, o[0], o[1], o[2], ix, iy, iz);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
        SAFE_RELEASE(t);
    }
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_texture1d_point_and_wrap)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture1D<float4> t : register(t0); SamplerState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target { return t.SampleLevel(s, a.x, 0); });
    pr p; if (!pr_open(&p, ps)) return;
    float d[8 * 4];
    for (int x = 0; x < 8; x++) { d[x * 4] = (float)(x * x); d[x * 4 + 1] = 0; d[x * 4 + 2] = 0; d[x * 4 + 3] = 1; }
    D3D11_TEXTURE1D_DESC td; D3D11_SUBRESOURCE_DATA sd; ID3D11Texture1D *t = NULL;
    memset(&td, 0, sizeof td);
    td.Width = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sd.pSysMem = d; sd.SysMemPitch = 128; sd.SysMemSlicePitch = 0;
    EXPECT_HR_OK(ID3D11Device_CreateTexture1D(p.r.dev, &td, &sd, &t));
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_WRAP, NULL);
        if (srv && smp) {
            pr_bind(&p, srv, smp);
            for (int k = -10; k <= 17; k++) {
                float o[4]; pcb c = pc_uv((k + 0.5f) / 8, 0, 0, 0);
                if (pr_run(&p, &c, o)) { int ix = addr_index(D3D11_TEXTURE_ADDRESS_WRAP, k, 8); CHECKF(o[0] == ix * ix, "texel %d: got %g want %d", k, o[0], ix * ix); }
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
        SAFE_RELEASE(t);
    }
    pr_close(&p);
}

/* -------------------------------------------------- format decoding ---- */
static int fmt_loadable(rx *r, DXGI_FORMAT f)
{
    UINT sup = 0;
    if (FAILED(ID3D11Device_CheckFormatSupport(r->dev, f, &sup))) return 0;
    return (sup & D3D11_FORMAT_SUPPORT_TEXTURE2D) && (sup & D3D11_FORMAT_SUPPORT_SHADER_LOAD);
}

TEST(d3d11_render_exec_format_decode_float_loads)
{
    pr p; if (!pr_open(&p, PS_LOAD)) return;
    static const struct { DXGI_FORMAT f; unsigned bpp; uint8_t v[8]; float want[4]; float tol; const char *name; } tab[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM, 4, { 255, 128, 0, 64 }, { 1, 128 / 255.f, 0, 64 / 255.f }, 1e-5f, "R8G8B8A8_UNORM" },
        { DXGI_FORMAT_R8G8B8A8_SNORM, 4, { 0x7F, 0x80, 0x81, 0x00 }, { 1, -1, -1, 0 }, 1e-6f, "R8G8B8A8_SNORM" },
        { DXGI_FORMAT_R8G8B8A8_SNORM, 4, { 0x40, 0xC0, 0x01, 0xFF }, { 64 / 127.f, -64 / 127.f, 1 / 127.f, -1 / 127.f }, 1e-5f, "R8G8B8A8_SNORM mid" },
        { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, { 255, 0, 128, 200 }, { 1, 0, 0.2158605f, 200 / 255.f }, 2e-3f, "R8G8B8A8_UNORM_SRGB" },
        { DXGI_FORMAT_B8G8R8A8_UNORM, 4, { 10, 20, 30, 40 }, { 30 / 255.f, 20 / 255.f, 10 / 255.f, 40 / 255.f }, 1e-5f, "B8G8R8A8_UNORM" },
        { DXGI_FORMAT_B8G8R8X8_UNORM, 4, { 10, 20, 30, 99 }, { 30 / 255.f, 20 / 255.f, 10 / 255.f, 1 }, 1e-5f, "B8G8R8X8_UNORM" },
        { DXGI_FORMAT_R8G8_UNORM, 2, { 255, 51 }, { 1, 0.2f, 0, 1 }, 1e-5f, "R8G8_UNORM" },
        { DXGI_FORMAT_R8_UNORM, 1, { 102 }, { 0.4f, 0, 0, 1 }, 1e-5f, "R8_UNORM" },
        { DXGI_FORMAT_A8_UNORM, 1, { 51 }, { 0, 0, 0, 0.2f }, 1e-5f, "A8_UNORM" },
        { DXGI_FORMAT_R16_UNORM, 2, { 0xFF, 0xFF }, { 1, 0, 0, 1 }, 1e-6f, "R16_UNORM" },
        { DXGI_FORMAT_R16_UNORM, 2, { 0x00, 0x80 }, { 32768 / 65535.f, 0, 0, 1 }, 1e-6f, "R16_UNORM mid" },
        { DXGI_FORMAT_R16G16_SNORM, 4, { 0x00, 0x80, 0xFF, 0x7F }, { -1, 1, 0, 1 }, 1e-6f, "R16G16_SNORM" },
        { DXGI_FORMAT_R16_FLOAT, 2, { 0x00, 0x3C }, { 1, 0, 0, 1 }, 0, "R16_FLOAT" },
        { DXGI_FORMAT_R16G16B16A16_FLOAT, 8, { 0x00, 0x40, 0x00, 0xC0, 0x00, 0x38, 0x00, 0x3C }, { 2, -2, 0.5f, 1 }, 0, "R16G16B16A16_FLOAT" },
        { DXGI_FORMAT_R32_FLOAT, 4, { 0x00, 0x00, 0x60, 0x40 }, { 3.5f, 0, 0, 1 }, 0, "R32_FLOAT" },
        { DXGI_FORMAT_R10G10B10A2_UNORM, 4, { 0xFF, 0x03, 0x00, 0xC0 }, { 1, 0, 0, 1 }, 1e-6f, "R10G10B10A2_UNORM r+a" },
        { DXGI_FORMAT_R10G10B10A2_UNORM, 4, { 0x00, 0x00, 0x00, 0x60 }, { 0, 0, 512 / 1023.f, 1.0f / 3 }, 1e-6f, "R10G10B10A2_UNORM mid" },
        { DXGI_FORMAT_R11G11B10_FLOAT, 4, { 0xC0, 0x03, 0x20, 0x70 }, { 1, 2, 0.5f, 1 }, 0, "R11G11B10_FLOAT" },
        { DXGI_FORMAT_R9G9B9E5_SHAREDEXP, 4, { 0, 0, 0, 0 }, { 1, 1, 1, 1 }, 0, "R9G9B9E5_SHAREDEXP" },
        { DXGI_FORMAT_B5G6R5_UNORM, 2, { 0x00, 0xF8 }, { 1, 0, 0, 1 }, 1e-6f, "B5G6R5_UNORM red" },
        { DXGI_FORMAT_B5G6R5_UNORM, 2, { 0xFF, 0x07 }, { 0, 1, 1, 1 }, 1e-6f, "B5G6R5_UNORM cyan" },
        { DXGI_FORMAT_B5G5R5A1_UNORM, 2, { 0xE0, 0xFF }, { 1, 1, 0, 1 }, 1e-6f, "B5G5R5A1_UNORM yellow+a" },
        { DXGI_FORMAT_B5G5R5A1_UNORM, 2, { 0x1F, 0x00 }, { 0, 0, 1, 0 }, 1e-6f, "B5G5R5A1_UNORM blue" },
        { DXGI_FORMAT_B4G4R4A4_UNORM, 2, { 0x0F, 0xF0 }, { 0, 0, 1, 1 }, 1e-6f, "B4G4R4A4_UNORM blue+a" },
    };
    /* R9G9B9E5 1.0: mantissa 256 in each channel, exponent 16 (bias 15, 9 mantissa bits /512) */
    uint32_t e5 = 256u | (256u << 9) | (256u << 18) | (16u << 27);
    for (unsigned i = 0; i < A(tab); i++) {
        uint8_t buf[16]; D3D11_SUBRESOURCE_DATA sd; ID3D11Texture2D *t;
        if (!fmt_loadable(&p.r, tab[i].f)) { t_note("%s: not loadable on this device", tab[i].name); continue; }
        memset(buf, 0, sizeof buf); memcpy(buf, tab[i].v, tab[i].bpp);
        if (tab[i].f == DXGI_FORMAT_R9G9B9E5_SHAREDEXP) memcpy(buf, &e5, 4);
        sd.pSysMem = buf; sd.SysMemPitch = 16; sd.SysMemSlicePitch = 0;
        t = rx_tex2d(&p.r, 1, 1, 1, 1, tab[i].f, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
        ID3D11ShaderResourceView *srv = t ? mk_srv(&p.r, (ID3D11Resource *)t, NULL) : NULL;
        if (srv) {
            float o[4]; pcb c = pc_i(0, 0, 0);
            pr_bind(&p, srv, NULL);
            if (pr_run(&p, &c, o)) {
                int ok = 1;
                for (int k = 0; k < 4; k++) ok &= rx_near(o[k], tab[i].want[k], tab[i].tol);
                CHECKF(ok, "%s: got %.7g %.7g %.7g %.7g want %.7g %.7g %.7g %.7g", tab[i].name, o[0], o[1], o[2], o[3], tab[i].want[0], tab[i].want[1], tab[i].want[2], tab[i].want[3]);
            }
        }
        SAFE_RELEASE(t);
    }
    pr_close(&p);
}

TEST(d3d11_render_exec_format_decode_integer_loads)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2D<uint4> t : register(t0);
        uint4 main(float4 p : SV_Position) : SV_Target { return t.Load(int3(b.xyz)); });
    static const char pss[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2D<int4> t : register(t0);
        uint4 main(float4 p : SV_Position) : SV_Target { return asuint(t.Load(int3(b.xyz))); });
    static const struct { DXGI_FORMAT f; int sgn; unsigned bpp; uint8_t v[16]; uint32_t want[4]; const char *name; } tab[] = {
        { DXGI_FORMAT_R8G8B8A8_UINT, 0, 4, { 1, 2, 200, 255 }, { 1, 2, 200, 255 }, "R8G8B8A8_UINT" },
        { DXGI_FORMAT_R8G8B8A8_SINT, 1, 4, { 0xFF, 0x7F, 0x80, 0x01 }, { 0xFFFFFFFFu, 127, 0xFFFFFF80u, 1 }, "R8G8B8A8_SINT" },
        { DXGI_FORMAT_R16G16_UINT, 0, 4, { 0xFF, 0xFF, 0x34, 0x12 }, { 65535, 0x1234, 0, 1 }, "R16G16_UINT" },
        { DXGI_FORMAT_R16_SINT, 1, 2, { 0x00, 0x80 }, { 0xFFFF8000u, 0, 0, 1 }, "R16_SINT" },
        { DXGI_FORMAT_R32_UINT, 0, 4, { 0xEF, 0xBE, 0xAD, 0xDE }, { 0xDEADBEEFu, 0, 0, 1 }, "R32_UINT" },
        { DXGI_FORMAT_R32G32_SINT, 1, 8, { 0xFE, 0xFF, 0xFF, 0xFF, 5, 0, 0, 0 }, { 0xFFFFFFFEu, 5, 0, 1 }, "R32G32_SINT" },
        { DXGI_FORMAT_R8_UINT, 0, 1, { 0xAB }, { 0xAB, 0, 0, 1 }, "R8_UINT" },
        { DXGI_FORMAT_R10G10B10A2_UINT, 0, 4, { 0xFF, 0xFF, 0xFF, 0xFF }, { 1023, 1023, 1023, 3 }, "R10G10B10A2_UINT" },
        { DXGI_FORMAT_R32G32B32A32_UINT, 0, 16, { 1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0 }, { 1, 2, 3, 4 }, "R32G32B32A32_UINT" },
    };
    pr p; if (!pr_open(&p, ps)) return;
    if (!rx_target(&p.r, 1, 1, DXGI_FORMAT_R32G32B32A32_UINT)) { pr_close(&p); return; }
    ID3D11PixelShader *pss_ = rx_ps(&p.r, pss);
    for (unsigned i = 0; i < A(tab); i++) {
        D3D11_SUBRESOURCE_DATA sd; ID3D11Texture2D *t;
        if (!fmt_loadable(&p.r, tab[i].f)) { t_note("%s: not loadable", tab[i].name); continue; }
        sd.pSysMem = tab[i].v; sd.SysMemPitch = 16; sd.SysMemSlicePitch = 0;
        t = rx_tex2d(&p.r, 1, 1, 1, 1, tab[i].f, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
        ID3D11ShaderResourceView *srv = t ? mk_srv(&p.r, (ID3D11Resource *)t, NULL) : NULL;
        if (srv && pss_) {
            float o[4]; pcb c = pc_i(0, 0, 0); uint32_t u[4];
            ID3D11DeviceContext_PSSetShader(p.r.ctx, tab[i].sgn ? pss_ : p.ps, NULL, 0);
            pr_bind(&p, srv, NULL);
            if (pr_run(&p, &c, o)) {
                memcpy(u, o, 16);
                CHECKF(!memcmp(u, tab[i].want, 16), "%s: got %08x %08x %08x %08x want %08x %08x %08x %08x", tab[i].name, u[0], u[1], u[2], u[3], tab[i].want[0], tab[i].want[1], tab[i].want[2], tab[i].want[3]);
            }
        }
        SAFE_RELEASE(t);
    }
    SAFE_RELEASE(pss_);
    pr_close(&p);
}

TEST(d3d11_render_exec_typeless_texture_views_reinterpret)
{
    pr p; if (!pr_open(&p, PS_LOAD)) return;
    static const uint8_t px[4] = { 255, 128, 64, 32 };
    D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = px; sd.SysMemPitch = 4; sd.SysMemSlicePitch = 0;
    ID3D11Texture2D *t = rx_tex2d(&p.r, 1, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
    if (t) {
        static const struct { DXGI_FORMAT f; float want[4]; float tol; } tab[] = {
            { DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 128 / 255.f, 64 / 255.f, 32 / 255.f }, 1e-5f },
            { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, { 1, 0.2158605f, 0.0513f, 32 / 255.f }, 2e-3f },
        };
        for (unsigned i = 0; i < A(tab); i++) {
            D3D11_SHADER_RESOURCE_VIEW_DESC d; float o[4]; pcb c = pc_i(0, 0, 0);
            memset(&d, 0, sizeof d); d.Format = tab[i].f; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1;
            ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, &d);
            if (!srv) continue;
            pr_bind(&p, srv, NULL);
            if (pr_run(&p, &c, o)) {
                int ok = 1; for (int k = 0; k < 4; k++) ok &= rx_near(o[k], tab[i].want[k], tab[i].tol);
                CHECKF(ok, "view format %d: got %g %g %g %g", (int)tab[i].f, o[0], o[1], o[2], o[3]);
            }
        }
        /* R8G8B8A8_UINT view of the same bytes needs the integer shader */
        static const char ps_u[] = HL(
            cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
            Texture2D<uint4> t : register(t0);
            float4 main(float4 p : SV_Position) : SV_Target { return float4(t.Load(int3(b.xyz))); });
        ID3D11PixelShader *pu = rx_ps(&p.r, ps_u);
        D3D11_SHADER_RESOURCE_VIEW_DESC d; memset(&d, 0, sizeof d);
        d.Format = DXGI_FORMAT_R8G8B8A8_UINT; d.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels = 1;
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, &d);
        if (pu && srv) {
            float o[4]; pcb c = pc_i(0, 0, 0);
            ID3D11DeviceContext_PSSetShader(p.r.ctx, pu, NULL, 0);
            pr_bind(&p, srv, NULL);
            if (pr_run(&p, &c, o)) CHECKF(o[0] == 255 && o[1] == 128 && o[2] == 64 && o[3] == 32, "UINT view: %g %g %g %g", o[0], o[1], o[2], o[3]);
        } else SAFE_RELEASE(srv);
        SAFE_RELEASE(pu);
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_bc1_bc4_block_decode)
{
    pr p; if (!pr_open(&p, PS_LOAD)) return;
    /* BC1: c0 = 0xF800 (red) c1 = 0x001F (blue), selector bits all equal to one value */
    static const struct { uint8_t blk[8]; float want[4]; float tol; const char *name; } tab[] = {
        { { 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 }, { 1, 0, 0, 1 }, 0.002f, "BC1 selector 0 = color0" },
        { { 0x00, 0xF8, 0x1F, 0x00, 0x55, 0x55, 0x55, 0x55 }, { 0, 0, 1, 1 }, 0.002f, "BC1 selector 1 = color1" },
        { { 0x1F, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF }, { 0, 0, 0, 0 }, 0, "BC1 c0<=c1, selector 3 = transparent black" },
        { { 0x00, 0xF8, 0x1F, 0x00, 0xAA, 0xAA, 0xAA, 0xAA }, { 2 / 3.f, 0, 1 / 3.f, 1 }, 0.03f, "BC1 selector 2 = 2/3 c0 + 1/3 c1" },
    };
    for (unsigned i = 0; i < A(tab); i++) {
        D3D11_SUBRESOURCE_DATA sd; ID3D11Texture2D *t;
        if (!fmt_loadable(&p.r, DXGI_FORMAT_BC1_UNORM)) { t_note("BC1 not loadable"); break; }
        sd.pSysMem = tab[i].blk; sd.SysMemPitch = 8; sd.SysMemSlicePitch = 0;
        t = rx_tex2d(&p.r, 4, 4, 1, 1, DXGI_FORMAT_BC1_UNORM, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
        ID3D11ShaderResourceView *srv = t ? mk_srv(&p.r, (ID3D11Resource *)t, NULL) : NULL;
        if (srv) {
            pr_bind(&p, srv, NULL);
            for (int k = 0; k < 16; k += 5) {
                float o[4]; pcb c = pc_i(k % 4, k / 4, 0);
                if (!pr_run(&p, &c, o)) continue;
                int ok = 1; for (int j = 0; j < 4; j++) ok &= rx_near(o[j], tab[i].want[j], tab[i].tol);
                CHECKF(ok, "%s texel %d: got %g %g %g %g", tab[i].name, k, o[0], o[1], o[2], o[3]);
            }
        }
        SAFE_RELEASE(t);
    }
    if (fmt_loadable(&p.r, DXGI_FORMAT_BC4_UNORM)) {
        /* BC4: red0 = 255, red1 = 0, selector 0 -> 1.0 ; selector 1 -> 0.0 */
        static const uint8_t b0[8] = { 255, 0, 0, 0, 0, 0, 0, 0 };
        static const uint8_t b1[8] = { 255, 0, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24 };  /* all selectors = 1 (001 repeated) */
        for (int v = 0; v < 2; v++) {
            D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = v ? b1 : b0; sd.SysMemPitch = 8; sd.SysMemSlicePitch = 0;
            ID3D11Texture2D *t = rx_tex2d(&p.r, 4, 4, 1, 1, DXGI_FORMAT_BC4_UNORM, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
            ID3D11ShaderResourceView *srv = t ? mk_srv(&p.r, (ID3D11Resource *)t, NULL) : NULL;
            if (srv) {
                float o[4]; pcb c = pc_i(2, 1, 0);
                pr_bind(&p, srv, NULL);
                if (pr_run(&p, &c, o)) CHECKF(rx_near(o[0], v ? 0.0f : 1.0f, 0.002f) && o[1] == 0 && o[2] == 0 && o[3] == 1, "BC4 variant %d: %g %g %g %g", v, o[0], o[1], o[2], o[3]);
            }
            SAFE_RELEASE(t);
        }
    }
    pr_close(&p);
}

/* -------------------------------------------------------- comparison ---- */
static int cmp_pass2(D3D11_COMPARISON_FUNC f, float ref, float tex)
{
    switch (f) {
    case D3D11_COMPARISON_NEVER: return 0;
    case D3D11_COMPARISON_LESS: return ref < tex;
    case D3D11_COMPARISON_EQUAL: return ref == tex;
    case D3D11_COMPARISON_LESS_EQUAL: return ref <= tex;
    case D3D11_COMPARISON_GREATER: return ref > tex;
    case D3D11_COMPARISON_NOT_EQUAL: return ref != tex;
    case D3D11_COMPARISON_GREATER_EQUAL: return ref >= tex;
    default: return 1;
    }
}

TEST(d3d11_render_exec_sample_cmp_all_functions)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2D<float> t : register(t0); SamplerComparisonState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target { return float4(t.SampleCmpLevelZero(s, a.xy, a.z), t.SampleCmp(s, a.xy, a.z), 0, 1); });
    pr p; if (!pr_open(&p, ps)) return;
    UINT sup = 0;
    ID3D11Device_CheckFormatSupport(p.r.dev, DXGI_FORMAT_R32_FLOAT, &sup);
    if (!(sup & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON)) { t_skip("R32_FLOAT sample comparison unsupported"); pr_close(&p); return; }
    float d[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
    D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = d; sd.SysMemPitch = 8; sd.SysMemSlicePitch = 0;
    ID3D11Texture2D *t = rx_tex2d(&p.r, 2, 2, 1, 1, DXGI_FORMAT_R32_FLOAT, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
    if (t) {
        static const float refs[] = { 0.25f, 0.5f, 0.75f, 0.0f, 1.0f };
        static const D3D11_FILTER filt[] = { D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT };
        for (unsigned fi = 0; fi < A(filt); fi++) for (int f = D3D11_COMPARISON_NEVER; f <= D3D11_COMPARISON_ALWAYS; f++) {
            ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
            ID3D11SamplerState *smp = mk_samp(&p.r, filt[fi], D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, 0, (D3D11_COMPARISON_FUNC)f, NULL, 0, D3D11_FLOAT32_MAX);
            if (!srv || !smp) { SAFE_RELEASE(srv); SAFE_RELEASE(smp); continue; }
            pr_bind(&p, srv, smp);
            for (unsigned i = 0; i < A(refs); i++) {
                float o[4]; pcb c = pc_uv(0.5f, 0.5f, refs[i], 0);
                if (!pr_run(&p, &c, o)) continue;
                float want = cmp_pass2((D3D11_COMPARISON_FUNC)f, refs[i], 0.5f) ? 1.0f : 0.0f;
                CHECKF(o[0] == want, "filter %d func %d ref %g: SampleCmpLevelZero %g want %g", (int)filt[fi], f, refs[i], o[0], want);
                CHECKF(o[1] == want, "filter %d func %d ref %g: SampleCmp %g want %g", (int)filt[fi], f, refs[i], o[1], want);
            }
        }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sample_cmp_depth_texture_view)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2D<float> t : register(t0); SamplerComparisonState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target { return float4(t.SampleCmpLevelZero(s, a.xy, a.z), 0, 0, 1); });
    pr p; if (!pr_open(&p, ps)) return;
    ID3D11Texture2D *dt = rx_tex2d(&p.r, 4, 4, 1, 1, DXGI_FORMAT_R32_TYPELESS, 1, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, 0, NULL);
    ID3D11DepthStencilView *dsv = NULL;
    if (dt) {
        D3D11_DEPTH_STENCIL_VIEW_DESC vd; D3D11_SHADER_RESOURCE_VIEW_DESC sv;
        memset(&vd, 0, sizeof vd); vd.Format = DXGI_FORMAT_D32_FLOAT; vd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        memset(&sv, 0, sizeof sv); sv.Format = DXGI_FORMAT_R32_FLOAT; sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sv.Texture2D.MipLevels = 1;
        EXPECT_HR_OK(ID3D11Device_CreateDepthStencilView(p.r.dev, (ID3D11Resource *)dt, &vd, &dsv));
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)dt, &sv);
        ID3D11SamplerState *smp = mk_samp(&p.r, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, 0, D3D11_COMPARISON_LESS_EQUAL, NULL, 0, D3D11_FLOAT32_MAX);
        if (dsv && srv && smp) {
            ID3D11RenderTargetView *rtv = p.r.rtv;
            ID3D11DeviceContext_ClearDepthStencilView(p.r.ctx, dsv, D3D11_CLEAR_DEPTH, 0.5f, 0);
            ID3D11DeviceContext_OMSetRenderTargets(p.r.ctx, 1, &rtv, NULL);
            pr_bind(&p, srv, smp);
            static const struct { float ref; float want; } tab[] = { { 0.25f, 1 }, { 0.5f, 1 }, { 0.75f, 0 }, { 0, 1 }, { 1, 0 } };
            for (unsigned i = 0; i < A(tab); i++) {
                float o[4]; pcb c = pc_uv(0.5f, 0.5f, tab[i].ref, 0);
                if (pr_run(&p, &c, o)) CHECKF(o[0] == tab[i].want, "ref %g against depth 0.5 (LESS_EQUAL): %g want %g", tab[i].ref, o[0], tab[i].want);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(dsv); SAFE_RELEASE(dt);
    pr_close(&p);
}

/* ------------------------------------------------------------ gather ---- */
static ID3D11Texture2D *tex4_idx16(rx *r)
{
    /* RGBA16F, texel (x,y): R = i, G = i+100, B = i+200, A = i+300 where i = 4y+x */
    uint16_t d[16 * 4]; D3D11_SUBRESOURCE_DATA sd;
    for (int i = 0; i < 16; i++) { d[i * 4] = h16((float)i); d[i * 4 + 1] = h16((float)i + 100); d[i * 4 + 2] = h16((float)i + 200); d[i * 4 + 3] = h16((float)i + 300); }
    sd.pSysMem = d; sd.SysMemPitch = 32; sd.SysMemSlicePitch = 0;
    return rx_tex2d(r, 4, 4, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, D3D11_BIND_SHADER_RESOURCE, 0, &sd);
}

TEST(d3d11_render_exec_gather_component_order_and_channels)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2D<float4> t : register(t0); SamplerState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target {
            if (b.x == 0) return t.GatherRed(s, a.xy);
            if (b.x == 1) return t.GatherGreen(s, a.xy);
            if (b.x == 2) return t.GatherBlue(s, a.xy);
            return t.GatherAlpha(s, a.xy);
        });
    pr p; if (!pr_open(&p, ps)) return;
    ID3D11Texture2D *t = tex4_idx16(&p.r);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_CLAMP, NULL);
        if (srv && smp) {
            pr_bind(&p, srv, smp);
            /* gather footprint around a texel corner (cx,cy): texels (cx-1,cy-1) (cx,cy-1) (cx-1,cy) (cx,cy);
               result order: x = (cx-1,cy), y = (cx,cy), z = (cx,cy-1), w = (cx-1,cy-1) */
            for (int cy = 1; cy <= 3; cy++) for (int cx = 1; cx <= 3; cx++) for (int ch = 0; ch < 4; ch++) {
                float o[4]; pcb c = pc_uv(cx / 4.0f, cy / 4.0f, 0, 0); c.b[0] = ch;
                if (!pr_run(&p, &c, o)) continue;
                float w[4] = { (float)(cy * 4 + cx - 1), (float)(cy * 4 + cx), (float)((cy - 1) * 4 + cx), (float)((cy - 1) * 4 + cx - 1) };
                for (int k = 0; k < 4; k++) w[k] += 100.0f * ch;
                CHECKF(o[0] == w[0] && o[1] == w[1] && o[2] == w[2] && o[3] == w[3], "gather ch %d at corner (%d,%d): got %g %g %g %g want %g %g %g %g", ch, cx, cy, o[0], o[1], o[2], o[3], w[0], w[1], w[2], w[3]);
            }
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_gather_offset_and_border)
{
    static const char ps[] = HL(
        cbuffer C : register(b0) { float4 a; float4 c; int4 b; };
        Texture2D<float4> t : register(t0); SamplerState s : register(s0);
        float4 main(float4 p : SV_Position) : SV_Target { return t.Gather(s, a.xy, int2(1, 1)); });
    pr p; if (!pr_open(&p, ps)) return;
    ID3D11Texture2D *t = tex4_idx16(&p.r);
    if (t) {
        ID3D11ShaderResourceView *srv = mk_srv(&p.r, (ID3D11Resource *)t, NULL);
        static const float bord[4] = { 7, 7, 7, 7 };
        ID3D11SamplerState *smp = samp_point(&p.r, D3D11_TEXTURE_ADDRESS_BORDER, bord);
        if (srv && smp) {
            float o[4]; pcb c = pc_uv(0.25f, 0.25f, 0, 0);   /* corner (1,1) shifted by (1,1) -> corner (2,2): texels 5 6 9 10 -> x=9 y=10 z=6 w=5 */
            pr_bind(&p, srv, smp);
            if (pr_run(&p, &c, o)) CHECKF(o[0] == 9 && o[1] == 10 && o[2] == 6 && o[3] == 5, "offset gather: %g %g %g %g", o[0], o[1], o[2], o[3]);
            c = pc_uv(1.0f, 1.0f, 0, 0);                      /* corner (4,4) + (1,1) -> corner (5,5): all outside -> border */
            if (pr_run(&p, &c, o)) CHECKF(o[0] == 7 && o[1] == 7 && o[2] == 7 && o[3] == 7, "border gather: %g %g %g %g", o[0], o[1], o[2], o[3]);
            c = pc_uv(0.75f, 0.75f, 0, 0);                    /* corner (3,3) + (1,1) -> corner (4,4): texel 15 is the only inside one (w), x/y/z outside */
            if (pr_run(&p, &c, o)) CHECKF(o[0] == 7 && o[1] == 7 && o[2] == 7 && o[3] == 15, "partial border gather: %g %g %g %g", o[0], o[1], o[2], o[3]);
        } else { SAFE_RELEASE(srv); SAFE_RELEASE(smp); }
    }
    SAFE_RELEASE(t);
    pr_close(&p);
}

TEST(d3d11_render_exec_sampler_state_roundtrip_via_context)
{
    rx r; if (!rx_open(&r)) return;
    ID3D11SamplerState *s[3] = { NULL, NULL, NULL }, *g[3] = { NULL, NULL, NULL };
    for (int i = 0; i < 3; i++) s[i] = mk_samp(&r, D3D11_FILTER_MIN_MAG_MIP_POINT, (D3D11_TEXTURE_ADDRESS_MODE)(1 + i), D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP, 0, D3D11_COMPARISON_NEVER, NULL, 0, 4.0f + i);
    if (s[0] && s[1] && s[2]) {
        ID3D11DeviceContext_PSSetSamplers(r.ctx, 2, 3, s);
        ID3D11DeviceContext_PSGetSamplers(r.ctx, 2, 3, g);
        for (int i = 0; i < 3; i++) { CHECK(g[i] == s[i]); SAFE_RELEASE(g[i]); }
        ID3D11DeviceContext_CSSetSamplers(r.ctx, 0, 1, &s[1]);
        ID3D11DeviceContext_CSGetSamplers(r.ctx, 0, 1, &g[0]);
        CHECK(g[0] == s[1]); SAFE_RELEASE(g[0]);
        ID3D11DeviceContext_PSGetSamplers(r.ctx, 0, 1, &g[0]);
        CHECK(g[0] == NULL);
        D3D11_SAMPLER_DESC d;
        ID3D11SamplerState_GetDesc(s[2], &d);
        CHECK(d.AddressU == D3D11_TEXTURE_ADDRESS_CLAMP + 0 || d.AddressU == (D3D11_TEXTURE_ADDRESS_MODE)3);
        CHECK(d.MaxLOD == 6.0f);
    }
    for (int i = 0; i < 3; i++) SAFE_RELEASE(s[i]);
    rx_close(&r);
}
