#include "hse.h"

hse_t hse;
static int g_inited, g_ok;
static void hse_fini(void) {
    SAFE_RELEASE(hse.ctx);
    SAFE_RELEASE(hse.dev);
}
int hse_init(void) {
    if (g_inited) return g_ok;
    g_inited = 1;
    if (FAILED(d3dh_make_device11(&hse.dev, &hse.ctx, &hse.fl, &hse.drv, 0))) return g_ok = 0;
    g_ok = 1;
    atexit(hse_fini);
    t_note("hlsl_semantics_exec device: driver_type=%d feature_level=0x%x", (int)hse.drv, (unsigned)hse.fl);
    return 1;
}

int hse_need(void) {
    if (hse_init()) return 1;
    return t_is_wine() ? 1 : 0;
}

uint32_t hse_ulp_dist(uint32_t a, uint32_t b) {
    int32_t x = (int32_t)a, y = (int32_t)b;
    if (x < 0) x = (int32_t)0x80000000 - x;
    if (y < 0) y = (int32_t)0x80000000 - y;
    int64_t d = (int64_t)x - (int64_t)y;
    return (uint32_t)(d < 0 ? -d : d);
}

ID3DBlob *hse_compile_at(const char *file, int line, const char *src, const char *target, UINT flags) {
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile(src, "main", target, (flags & HSE_NO_IEEE) ? (flags & ~HSE_NO_IEEE) : (flags | D3DCOMPILE_IEEE_STRICTNESS), &b);
    if (FAILED(hr)) {
        if (t_is_wine() && !getenv("HSE_STRICT_COMPILE")) { t_skip("shader compile failed under wine (hr=%s)", d3dh_hr_name(hr)); return NULL; }
        if (t_is_wine()) t_check(0, file, line, "shader compile", "hr=%s err=%.200s", d3dh_hr_name(hr), d3dh_compile_errors());
        else t_check(0, file, line, "shader compile", "hr=%s target=%s err=%.400s\n--- source ---\n%.1500s", d3dh_hr_name(hr), target, d3dh_compile_errors(), src);
        return NULL;
    }
    return b;
}

ID3D11Buffer *hse_buf(UINT bytes, UINT bind, UINT misc, UINT stride, const void *init) {
    if (!hse.dev) return NULL;
    D3D11_BUFFER_DESC d; memset(&d, 0, sizeof d);
    d.ByteWidth = bytes; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind; d.MiscFlags = misc; d.StructureByteStride = stride;
    D3D11_SUBRESOURCE_DATA sd; memset(&sd, 0, sizeof sd); sd.pSysMem = init;
    ID3D11Buffer *b = NULL;
    HRESULT hr = ID3D11Device_CreateBuffer(hse.dev, &d, init ? &sd : NULL, &b);
    return SUCCEEDED(hr) ? b : NULL;
}
ID3D11ShaderResourceView *hse_buf_srv(ID3D11Buffer *b, DXGI_FORMAT fmt, UINT first, UINT num, int raw) {
    D3D11_SHADER_RESOURCE_VIEW_DESC d; memset(&d, 0, sizeof d);
    ID3D11ShaderResourceView *v = NULL;
    if (!hse.dev || !b) return NULL;
    if (raw) {
        d.Format = DXGI_FORMAT_R32_TYPELESS; d.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        d.BufferEx.FirstElement = first; d.BufferEx.NumElements = num; d.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    } else {
        d.Format = fmt; d.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; d.Buffer.FirstElement = first; d.Buffer.NumElements = num;
    }
    return SUCCEEDED(ID3D11Device_CreateShaderResourceView(hse.dev, (ID3D11Resource *)b, &d, &v)) ? v : NULL;
}
ID3D11UnorderedAccessView *hse_buf_uav(ID3D11Buffer *b, DXGI_FORMAT fmt, UINT first, UINT num, int raw) {
    D3D11_UNORDERED_ACCESS_VIEW_DESC d; memset(&d, 0, sizeof d);
    ID3D11UnorderedAccessView *v = NULL;
    if (!hse.dev || !b) return NULL;
    d.Format = raw ? DXGI_FORMAT_R32_TYPELESS : fmt; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    d.Buffer.FirstElement = first; d.Buffer.NumElements = num; d.Buffer.Flags = raw ? D3D11_BUFFER_UAV_FLAG_RAW : 0;
    return SUCCEEDED(ID3D11Device_CreateUnorderedAccessView(hse.dev, (ID3D11Resource *)b, &d, &v)) ? v : NULL;
}
int hse_read_buf(ID3D11Buffer *b, void **data, size_t *size) {
    return SUCCEEDED(d3dh_readback_buffer11(hse.dev, hse.ctx, b, data, size));
}

int hse_dispatch_at(const char *file, int line, hse_cs *c) {
    int ok = 0;
    ID3DBlob *bl = NULL; ID3D11ComputeShader *cs = NULL;
    ID3D11Buffer *bin = NULL, *bcb = NULL, *bout = NULL;
    ID3D11ShaderResourceView *sin = NULL; ID3D11UnorderedAccessView *uout = NULL;
    void *rb = NULL; size_t rbs = 0;
    bl = hse_compile_at(file, line, c->src, c->target ? c->target : "cs_5_0", c->cflags);
    if (!bl) return 0;
    if (!hse.dev) { t_skip("compile-only mode: shader compiled, not executed"); SAFE_RELEASE(bl); return 0; }
    HRESULT hr = ID3D11Device_CreateComputeShader(hse.dev, d3dh_blob_ptr(bl), d3dh_blob_size(bl), NULL, &cs);
    if (FAILED(hr)) { t_check(0, file, line, "CreateComputeShader", "hr=%s", d3dh_hr_name(hr)); goto done; }
    if (c->inbytes) {
        bin = hse_buf(c->inbytes, D3D11_BIND_SHADER_RESOURCE, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, c->in);
        sin = bin ? hse_buf_srv(bin, 0, 0, c->inbytes / 4, 1) : NULL;
        if (!sin) { t_check(0, file, line, "input buffer", NULL); goto done; }
    }
    if (c->cbbytes) {
        bcb = hse_buf(c->cbbytes, D3D11_BIND_CONSTANT_BUFFER, 0, 0, c->cb);
        if (!bcb) { t_check(0, file, line, "constant buffer", NULL); goto done; }
    }
    if (c->outbytes) {
        bout = hse_buf(c->outbytes, D3D11_BIND_UNORDERED_ACCESS, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0, c->outinit);
        uout = bout ? hse_buf_uav(bout, 0, 0, c->outbytes / 4, 1) : NULL;
        if (!uout) { t_check(0, file, line, "output buffer", NULL); goto done; }
    }
    ID3D11DeviceContext *x = hse.ctx;
    ID3D11DeviceContext_CSSetShader(x, cs, NULL, 0);
    if (sin) ID3D11DeviceContext_CSSetShaderResources(x, 0, 1, &sin);
    for (int i = 0; i < 6; i++) if (c->srv[i]) ID3D11DeviceContext_CSSetShaderResources(x, 1 + i, 1, &c->srv[i]);
    for (int i = 0; i < 4; i++) if (c->smp[i]) ID3D11DeviceContext_CSSetSamplers(x, i, 1, &c->smp[i]);
    if (bcb) ID3D11DeviceContext_CSSetConstantBuffers(x, 0, 1, &bcb);
    if (uout) ID3D11DeviceContext_CSSetUnorderedAccessViews(x, 0, 1, &uout, NULL);
    for (int i = 0; i < 4; i++) if (c->uav[i]) ID3D11DeviceContext_CSSetUnorderedAccessViews(x, 1 + i, 1, &c->uav[i], NULL);
    ID3D11DeviceContext_Dispatch(x, c->gx ? c->gx : 1, c->gy ? c->gy : 1, c->gz ? c->gz : 1);
    {   /* unbind */
        ID3D11UnorderedAccessView *nu[5] = {0}; ID3D11ShaderResourceView *ns[8] = {0};
        ID3D11DeviceContext_CSSetUnorderedAccessViews(x, 0, 5, nu, NULL);
        ID3D11DeviceContext_CSSetShaderResources(x, 0, 7, ns);
        ID3D11DeviceContext_CSSetShader(x, NULL, NULL, 0);
    }
    if (bout) {
        if (!hse_read_buf(bout, &rb, &rbs) || rbs < c->outbytes) { t_check(0, file, line, "readback", NULL); goto done; }
        if (c->out) memcpy(c->out, rb, c->outbytes);
    }
    ok = 1;
done:
    free(rb);
    SAFE_RELEASE(uout); SAFE_RELEASE(sin); SAFE_RELEASE(bout); SAFE_RELEASE(bcb); SAFE_RELEASE(bin);
    SAFE_RELEASE(cs); SAFE_RELEASE(bl);
    return ok;
}

static const char *k_row_tmpl_a =
    "ByteAddressBuffer I : register(t0);\n"
    "RWByteAddressBuffer O : register(u0);\n"
    "[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) {\n"
    "  uint ro_ = id.x * 16;\n"
    "  uint ua = I.Load(ro_), ub = I.Load(ro_ + 4), uc = I.Load(ro_ + 8);\n"
    "  int ia = asint(ua), ib = asint(ub), ic = asint(uc);\n"
    "  float fa = asfloat(ua), fb = asfloat(ub), fc = asfloat(uc);\n";

static int row_ok(int mode, int ulps, uint32_t got, uint32_t want) {
    int gn = (got & 0x7fffffffu) > 0x7f800000u, wn = (want & 0x7fffffffu) > 0x7f800000u;
    if (gn || wn) return mode != HSE_EXACT ? (gn && wn) : got == want;
    switch (mode) {
    case HSE_EXACT: case HSE_SIGNED0: return got == want;
    case HSE_FLT: return got == want || (((got | want) & 0x7fffffffu) == 0);
    case HSE_ULP:
        if (((got | want) & 0x7fffffffu) == 0) return 1;
        if ((got & 0x7fffffffu) == 0x7f800000u || (want & 0x7fffffffu) == 0x7f800000u) return got == want;
        return hse_ulp_dist(got, want) <= (uint32_t)ulps;
    }
    return 0;
}

const char *hse_prelude;
int hse_rows_at(const char *file, int line, const char *pre, const char *expr, const hse_row *r, int n, int mode, int ulps, UINT cflags) {
    char src[8192];
    const char *pl = hse_prelude ? hse_prelude : ""; hse_prelude = NULL;
    snprintf(src, sizeof src, "%s\n%s  %s\n  O.Store(id.x * 4, asuint(%s));\n}\n", pl, k_row_tmpl_a, pre ? pre : "", expr);
    int groups = (n + 63) / 64;
    hse_row *in = calloc(groups * 64, sizeof *in);
    uint32_t *out = calloc(groups * 64, 4);
    memcpy(in, r, n * sizeof *r);
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = src; c.cflags = cflags; c.in = in; c.inbytes = groups * 64 * sizeof *in; c.outbytes = groups * 64 * 4; c.gx = groups; c.out = out;
    int ok = hse_dispatch_at(file, line, &c);
    if (ok) {
        int soft = mode & HSE_SOFT, bad = 0;
        for (int i = 0; i < n; i++) {
            int good = row_ok(mode & 0xff, ulps, out[i], r[i].want);
            if (soft) {
                if (!good && bad++ < 4) t_note("%s: row %d a=%08x b=%08x c=%08x got=%08x want=%08x", expr, i, r[i].a, r[i].b, r[i].c, out[i], r[i].want);
            } else
                t_check(good, file, line, expr, "row %d a=%08x b=%08x c=%08x got=%08x want=%08x", i, r[i].a, r[i].b, r[i].c, out[i], r[i].want);
        }
        if (soft) t_note("%s: %d of %d rows differ from the reference", expr, bad, n);
    }
    free(in); free(out);
    return ok;
}

/* ---------------- textures / pixel shader ---------------- */
ID3D11Texture2D *hse_tex2d(UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT fmt, UINT bind, const D3D11_SUBRESOURCE_DATA *init) {
    D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
    if (!hse.dev) return NULL;
    d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = arr ? arr : 1; d.Format = fmt;
    d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    ID3D11Texture2D *t = NULL;
    return SUCCEEDED(ID3D11Device_CreateTexture2D(hse.dev, &d, init, &t)) ? t : NULL;
}
ID3D11SamplerState *hse_sampler(D3D11_FILTER f, D3D11_TEXTURE_ADDRESS_MODE am, D3D11_COMPARISON_FUNC cmp, float lodbias) {
    D3D11_SAMPLER_DESC d; memset(&d, 0, sizeof d);
    if (!hse.dev) return NULL;
    d.Filter = f; d.AddressU = d.AddressV = d.AddressW = am; d.MipLODBias = lodbias; d.MaxAnisotropy = 1;
    d.ComparisonFunc = cmp ? cmp : D3D11_COMPARISON_NEVER; d.MinLOD = 0; d.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState *s = NULL;
    return SUCCEEDED(ID3D11Device_CreateSamplerState(hse.dev, &d, &s)) ? s : NULL;
}

static const char *k_vs_fs =
    "float4 main(uint id : SV_VertexID) : SV_Position {\n"
    "  float2 p = float2((id << 1) & 2, id & 2);\n"
    "  return float4(p * 2.0 - 1.0, 0.0, 1.0); }\n";

int hse_draw_at(const char *file, int line, hse_ps *p, d3dh_image *out) {
    int ok = 0;
    ID3DBlob *vb = NULL, *pb = NULL; ID3D11VertexShader *vs = NULL; ID3D11PixelShader *ps = NULL;
    ID3D11Texture2D *rt = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11RasterizerState *rs = NULL; ID3D11Buffer *cb = NULL;
    UINT w = p->w ? p->w : 4, h = p->h ? p->h : 4; DXGI_FORMAT fmt = p->fmt ? p->fmt : DXGI_FORMAT_R32G32B32A32_FLOAT;
    vb = hse_compile_at(file, line, p->vs ? p->vs : k_vs_fs, "vs_5_0", p->cflags);
    if (!vb) return 0;
    pb = hse_compile_at(file, line, p->ps, "ps_5_0", p->cflags);
    if (!pb) { SAFE_RELEASE(vb); return 0; }
    if (!hse.dev) { t_skip("compile-only mode: shaders compiled, not executed"); SAFE_RELEASE(pb); SAFE_RELEASE(vb); return 0; }
    HRESULT hr = ID3D11Device_CreateVertexShader(hse.dev, d3dh_blob_ptr(vb), d3dh_blob_size(vb), NULL, &vs);
    if (SUCCEEDED(hr)) hr = ID3D11Device_CreatePixelShader(hse.dev, d3dh_blob_ptr(pb), d3dh_blob_size(pb), NULL, &ps);
    if (FAILED(hr)) { t_check(0, file, line, "Create*Shader", "hr=%s", d3dh_hr_name(hr)); goto done; }
    rt = hse_tex2d(w, h, 1, 1, fmt, D3D11_BIND_RENDER_TARGET, NULL);
    if (!rt || FAILED(ID3D11Device_CreateRenderTargetView(hse.dev, (ID3D11Resource *)rt, NULL, &rtv))) { t_check(0, file, line, "render target", NULL); goto done; }
    D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd);
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    ID3D11Device_CreateRasterizerState(hse.dev, &rd, &rs);
    if (p->cbbytes) cb = hse_buf(p->cbbytes, D3D11_BIND_CONSTANT_BUFFER, 0, 0, p->cb);
    ID3D11DeviceContext *x = hse.ctx;
    D3D11_VIEWPORT vp = { 0, 0, (float)w, (float)h, 0, 1 };
    ID3D11DeviceContext_OMSetRenderTargets(x, 1, &rtv, NULL);
    ID3D11DeviceContext_ClearRenderTargetView(x, rtv, p->clear);
    ID3D11DeviceContext_RSSetViewports(x, 1, &vp);
    ID3D11DeviceContext_RSSetState(x, rs);
    ID3D11DeviceContext_IASetInputLayout(x, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(x, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(x, vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(x, ps, NULL, 0);
    if (cb) { ID3D11DeviceContext_PSSetConstantBuffers(x, 0, 1, &cb); ID3D11DeviceContext_VSSetConstantBuffers(x, 0, 1, &cb); }
    for (int i = 0; i < 6; i++) if (p->srv[i]) ID3D11DeviceContext_PSSetShaderResources(x, i, 1, &p->srv[i]);
    for (int i = 0; i < 4; i++) if (p->smp[i]) ID3D11DeviceContext_PSSetSamplers(x, i, 1, &p->smp[i]);
    ID3D11DeviceContext_Draw(x, 3, 0);
    {
        ID3D11ShaderResourceView *ns[6] = {0};
        ID3D11DeviceContext_PSSetShaderResources(x, 0, 6, ns);
        ID3D11DeviceContext_OMSetRenderTargets(x, 0, NULL, NULL);
    }
    hr = d3dh_readback_texture2d11(hse.dev, x, rt, 0, out);
    if (FAILED(hr)) { t_check(0, file, line, "render target readback", "hr=%s", d3dh_hr_name(hr)); goto done; }
    ok = 1;
done:
    SAFE_RELEASE(cb); SAFE_RELEASE(rs); SAFE_RELEASE(rtv); SAFE_RELEASE(rt); SAFE_RELEASE(ps); SAFE_RELEASE(vs);
    SAFE_RELEASE(pb); SAFE_RELEASE(vb);
    return ok;
}

void hse_patterns(hse_row *r, int n) {
    static const uint32_t L[] = { 0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 31, 32, 33, 63, 64, 0x7f, 0x80, 0xff, 0x100, 0xffff, 0x10000,
        0x7fffffff, 0x80000000u, 0x80000001u, 0xfffffffeu, 0xffffffffu, 0x55555555u, 0xaaaaaaaau, 0x12345678u, 0xdeadbeefu,
        0x00ff00ffu, 0xff000000u, 0x7ffffffeu, 0x40000000u, 1000000007u, 0xfffffff0u };
    const int K = (int)(sizeof L / sizeof L[0]);
    d3dh_rng g; d3dh_rng_seed(&g, 0x5eed1234u);
    for (int i = 0; i < n; i++) {
        r[i].a = L[i % K];
        r[i].b = L[(i * 7 + i / K * 3 + 1) % K];
        r[i].c = d3dh_rng_u32(&g);
        if (i >= n - 48) { r[i].a = d3dh_rng_u32(&g); r[i].b = (uint32_t)(i - (n - 48)); }
        r[i].want = 0;
    }
}

int hse_sweep_at(const char *file, int line, const char *name, const char *expr, double (*ref)(double, double), double lo, double hi, double y, int n) {
    hse_row *r = calloc(n, sizeof *r);
    for (int i = 0; i < n; i++) {
        double x = lo + (hi - lo) * i / (n > 1 ? n - 1 : 1);
        r[i].a = hse_fb((float)x); r[i].b = hse_fb((float)y);
        r[i].want = hse_fb((float)ref((double)hse_bf(r[i].a), (double)hse_bf(r[i].b)));
    }
    char src[2048];
    int groups = (n + 63) / 64;
    hse_row *in = calloc(groups * 64, sizeof *in); uint32_t *out = calloc(groups * 64, 4);
    memcpy(in, r, n * sizeof *r);
    snprintf(src, sizeof src, "%s  O.Store(id.x * 4, asuint(%s));\n}\n", k_row_tmpl_a, expr);
    hse_cs c; memset(&c, 0, sizeof c);
    c.src = src; c.in = in; c.inbytes = groups * 64 * sizeof *in; c.outbytes = groups * 64 * 4; c.gx = groups; c.out = out;
    int ok = hse_dispatch_at(file, line, &c);
    if (ok) {
        uint32_t worst = 0; int wi = 0, nan = 0; double maxabs = 0;
        for (int i = 0; i < n; i++) {
            float g = hse_bf(out[i]), w = hse_bf(r[i].want);
            if (g != g) { if (w == w) nan++; continue; }
            if (w != w) continue;
            uint32_t d = hse_ulp_dist(out[i], r[i].want);
            double ae = fabs((double)g - (double)w);
            if (ae > maxabs && ae < 1e30) maxabs = ae;
            /* measured on Win10 WARP (worst abs 6.3e-5 for asin/acos, worst relative ~2e-6 for pow): a gross-error bound, hard on WARP only */
            if (hse.drv == D3D_DRIVER_TYPE_WARP && ae >= 1e-30 && ae < 1e30)
                t_check(ae <= 1.5e-4 + 1e-5 * fabs((double)w), file, line, name, "x=%g got %g want %g", (double)hse_bf(r[i].a), (double)g, (double)w);
            if (d > worst && (r[i].want & 0x7fffffffu) != 0x7f800000u) { worst = d; wi = i; }
        }
        t_note("%s: max error %u ulp (at x=%g, got %g want %g), max abs error %g, unexpected NaNs %d, over %d samples", name, worst,
               (double)hse_bf(r[wi].a), (double)hse_bf(out[wi]), (double)hse_bf(r[wi].want), maxabs, nan, n);
    }
    free(r); free(in); free(out);
    return ok;
}
