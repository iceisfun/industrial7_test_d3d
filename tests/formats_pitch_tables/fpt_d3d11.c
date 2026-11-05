/* D3D11 staging-resource pitch tables per format group, width and dimension. */
#include "fpt.h"

typedef struct { ID3D11Device *dev; ID3D11DeviceContext *ctx; D3D_FEATURE_LEVEL fl; } env11;

static ID3D11Resource *mk_stage(ID3D11Device *dev, int dim, const fpt_fmt *f, UINT w, UINT h, UINT d, UINT mips, UINT arr, HRESULT *hr)
{
    ID3D11Resource *res = NULL;
    if (dim == 1) {
        D3D11_TEXTURE1D_DESC t = {0};
        t.Width = w; t.MipLevels = mips; t.ArraySize = arr; t.Format = (DXGI_FORMAT)f->id;
        t.Usage = D3D11_USAGE_STAGING; t.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        *hr = ID3D11Device_CreateTexture1D(dev, &t, NULL, (ID3D11Texture1D **)&res);
    } else if (dim == 2) {
        D3D11_TEXTURE2D_DESC t = {0};
        t.Width = w; t.Height = h; t.MipLevels = mips; t.ArraySize = arr; t.Format = (DXGI_FORMAT)f->id; t.SampleDesc.Count = 1;
        t.Usage = D3D11_USAGE_STAGING; t.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        *hr = ID3D11Device_CreateTexture2D(dev, &t, NULL, (ID3D11Texture2D **)&res);
    } else {
        D3D11_TEXTURE3D_DESC t = {0};
        t.Width = w; t.Height = h; t.Depth = d; t.MipLevels = mips; t.Format = (DXGI_FORMAT)f->id;
        t.Usage = D3D11_USAGE_STAGING; t.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        *hr = ID3D11Device_CreateTexture3D(dev, &t, NULL, (ID3D11Texture3D **)&res);
    }
    if (FAILED(*hr)) res = NULL;
    return res;
}

static BYTE pat(UINT id, UINT z, UINT row, UINT i) { return (BYTE)(id * 31 + z * 17 + row * 7 + i * 3 + 1); }

static void fill_rows(BYTE *base, UINT rp, UINT dp, UINT rowb, UINT rows, UINT slices, UINT id)
{
    UINT z, r, i;
    for (z = 0; z < slices; z++) for (r = 0; r < rows; r++) {
        BYTE *p = base + (size_t)z * dp + (size_t)r * rp;
        (void)rp;
        for (i = 0; i < rowb; i++) p[i] = pat(id, z, r, i);
    }
}
static int cmp_rows(const BYTE *base, UINT rp, UINT dp, UINT rowb, UINT rows, UINT slices, UINT id)
{
    UINT z, r, i;
    for (z = 0; z < slices; z++) for (r = 0; r < rows; r++) {
        const BYTE *p = base + (size_t)z * dp + (size_t)r * rp;
        for (i = 0; i < rowb; i++) if (p[i] != pat(id, z, r, i)) return 0;
    }
    return 1;
}

/* Dimension-specific sizes used by the tables. */
#define TH 5
#define TD 3

static int hard_group(UINT g) { return (g >= G_B8 && g <= G_B128) || (g >= G_BC1 && g <= G_BC7); }

static const char *dimname(int d) { return d == 1 ? "tex1d" : d == 2 ? "tex2d" : "tex3d"; }

/* Staging round trip + pitch invariants for one dimension and one group. */
static void run_group(UINT grp, int dim)
{
    env11 e; UINT id, wi;
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id];
        char line[400]; int n; UINT prev = 0; int created_any = 0;
        if (f->group != grp) continue;
        n = snprintf(line, sizeof line, "%s %s rowpitch by width:", dimname(dim), f->name);
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi], h = dim == 1 ? 1 : TH, d = dim == 3 ? TD : 1;
            UINT rowb = fpt_row_bytes(f, w), rows = fpt_rows(f, h);
            UINT sup = fpt_support11(e.dev, id, NULL);
            UINT bit = dim == 1 ? D3D11_FORMAT_SUPPORT_TEXTURE1D : dim == 2 ? D3D11_FORMAT_SUPPORT_TEXTURE2D : D3D11_FORMAT_SUPPORT_TEXTURE3D;
            int must = hard_group(grp) && (sup & bit) && !(fpt_is_bc(f) && (w % 4 || dim == 1));
            HRESULT hr1, hr2;
            ID3D11Resource *a, *b;
            D3D11_MAPPED_SUBRESOURCE m;
            a = mk_stage(e.dev, dim, f, w, h, d, 1, 1, &hr1);
            b = a ? mk_stage(e.dev, dim, f, w, h, d, 1, 1, &hr2) : NULL;
            if (!a || !b) {
                if (must) CHECKF(0, "%s %s w=%u creation failed hr=%s", dimname(dim), f->name, w, d3dh_hr_name(a ? hr2 : hr1));
                else if (wi == 0 || (sup & bit)) t_note("%s %s w=%u: staging creation fails (%s), support=0x%x", dimname(dim), f->name, w, d3dh_hr_name(a ? hr2 : hr1), sup);
                SAFE_RELEASE(a);
                n += snprintf(line + n, sizeof line - n, " -");
                continue;
            }
            created_any = 1;
            memset(&m, 0, sizeof m);
            hr1 = ID3D11DeviceContext_Map(e.ctx, a, 0, D3D11_MAP_READ_WRITE, 0, &m);
            EXPECT_HR(hr1, S_OK);
            if (SUCCEEDED(hr1)) {
                UINT rp = m.RowPitch, dp = m.DepthPitch;
                CHECKF(m.pData != NULL, "%s %s w=%u pData NULL", dimname(dim), f->name, w);
                if (dim != 1) CHECKF(rp >= rowb, "%s %s w=%u RowPitch %u < row bytes %u", dimname(dim), f->name, w, rp, rowb);
                else CHECKF(rp == 0 || rp >= rowb, "%s %s w=%u RowPitch %u < row bytes %u", dimname(dim), f->name, w, rp, rowb);
                if (dim == 3) CHECKF(dp >= rp * rows, "%s %s w=%u DepthPitch %u < RowPitch*rows %u", dimname(dim), f->name, w, dp, rp * rows);
                if (dim != 1 && prev) CHECKF(rp >= prev, "%s %s RowPitch not monotonic in width: %u after %u", dimname(dim), f->name, rp, prev);
                if (dim != 1) prev = rp;
                n += snprintf(line + n, sizeof line - n, " %u", rp);
                if (dim == 1) { rp = rowb; dp = rowb; } else if (dim == 2) dp = rp * rows;
                if (m.pData) fill_rows((BYTE *)m.pData, dim == 1 ? rowb : m.RowPitch, dim == 3 ? m.DepthPitch : dp, rowb, rows, d, id);
                ID3D11DeviceContext_Unmap(e.ctx, a, 0);
                ID3D11DeviceContext_CopyResource(e.ctx, b, a);
                memset(&m, 0, sizeof m);
                hr2 = ID3D11DeviceContext_Map(e.ctx, b, 0, D3D11_MAP_READ, 0, &m);
                EXPECT_HR(hr2, S_OK);
                if (SUCCEEDED(hr2)) {
                    UINT rp2 = dim == 1 ? rowb : m.RowPitch, dp2 = dim == 3 ? m.DepthPitch : rp2 * rows;
                    CHECKF(m.pData && cmp_rows((BYTE *)m.pData, rp2, dp2, rowb, rows, d, id), "%s %s w=%u: CopyResource round trip content mismatch", dimname(dim), f->name, w);
                    ID3D11DeviceContext_Unmap(e.ctx, b, 0);
                }
            }
            SAFE_RELEASE(b); SAFE_RELEASE(a);
        }
        if (created_any) t_note("%s", line);
    }
}

#define GT(nm, grp, dim) TEST(fpt_d3d11_##nm) { run_group(grp, dim); }
GT(tex2d_b8, G_B8, 2)     GT(tex2d_b16, G_B16, 2)   GT(tex2d_b32, G_B32, 2)   GT(tex2d_b64, G_B64, 2)
GT(tex2d_b96, G_B96, 2)   GT(tex2d_b128, G_B128, 2)
GT(tex2d_bc1, G_BC1, 2)   GT(tex2d_bc2, G_BC2, 2)   GT(tex2d_bc3, G_BC3, 2)   GT(tex2d_bc4, G_BC4, 2)
GT(tex2d_bc5, G_BC5, 2)   GT(tex2d_bc6h, G_BC6, 2)  GT(tex2d_bc7, G_BC7, 2)
GT(tex2d_packed422, G_422, 2) GT(tex2d_r1, G_R1, 2) GT(tex2d_video_packed, G_VPACK, 2) GT(tex2d_palettized, G_PAL, 2)
GT(tex1d_b8, G_B8, 1)     GT(tex1d_b16, G_B16, 1)   GT(tex1d_b32, G_B32, 1)   GT(tex1d_b64, G_B64, 1)
GT(tex1d_b96, G_B96, 1)   GT(tex1d_b128, G_B128, 1) GT(tex1d_bc_try, G_BC1, 1) GT(tex1d_bc7_try, G_BC7, 1)
GT(tex3d_b8, G_B8, 3)     GT(tex3d_b16, G_B16, 3)   GT(tex3d_b32, G_B32, 3)   GT(tex3d_b64, G_B64, 3)
GT(tex3d_b96, G_B96, 3)   GT(tex3d_b128, G_B128, 3)
GT(tex3d_bc1, G_BC1, 3)   GT(tex3d_bc2, G_BC2, 3)   GT(tex3d_bc3, G_BC3, 3)   GT(tex3d_bc4, G_BC4, 3)
GT(tex3d_bc5, G_BC5, 3)   GT(tex3d_bc6h, G_BC6, 3)  GT(tex3d_bc7, G_BC7, 3)
GT(tex3d_packed422, G_422, 3)

/* Planar video formats: observe the luma row pitch; no content round trip (plane layout is implementation defined). */
TEST(fpt_d3d11_tex2d_planar_video)
{
    env11 e; UINT id, wi;
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id];
        char line[300]; int n; UINT sizes[2] = { 2, 8 }, si;
        if (f->group != G_PLANAR) continue;
        n = snprintf(line, sizeof line, "%s luma RowPitch by (even) width:", f->name);
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi];
            HRESULT hr; ID3D11Resource *a; D3D11_MAPPED_SUBRESOURCE m = {0};
            if (w & 1) continue;
            for (si = 0; si < 2; si++) {
                a = mk_stage(e.dev, 2, f, w, sizes[si] * 4, 1, 1, 1, &hr);
                if (!a) { if (si == 0) n += snprintf(line + n, sizeof line - n, " (w=%u: %s)", w, d3dh_hr_name(hr)); continue; }
                if (si == 0 && SUCCEEDED(ID3D11DeviceContext_Map(e.ctx, a, 0, D3D11_MAP_READ, 0, &m))) {
                    CHECKF(m.RowPitch >= w * f->bpb, "%s w=%u luma RowPitch %u < %u", f->name, w, m.RowPitch, w * f->bpb);
                    n += snprintf(line + n, sizeof line - n, " %u", m.RowPitch);
                    ID3D11DeviceContext_Unmap(e.ctx, a, 0);
                }
                SAFE_RELEASE(a);
            }
        }
        t_note("%s", line);
    }
}

/* Odd dimensions of 4:2:0 video: observe only. */
TEST(fpt_d3d11_tex2d_video_odd_sizes)
{
    env11 e; static const UINT fmts[] = { DXGI_FORMAT_NV12, DXGI_FORMAT_P010, DXGI_FORMAT_YUY2, DXGI_FORMAT_Y210, DXGI_FORMAT_AYUV, DXGI_FORMAT_420_OPAQUE };
    UINT i, j; static const UINT dims[][2] = { {1,1}, {3,3}, {4,3}, {3,4}, {5,7}, {16,17}, {17,16} };
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof fmts / sizeof *fmts; i++) {
        char line[300]; int n; const fpt_fmt *f = &fpt_formats[fmts[i]];
        n = snprintf(line, sizeof line, "%s odd-size creation (hr per WxH):", f->name);
        for (j = 0; j < sizeof dims / sizeof *dims; j++) {
            HRESULT hr; ID3D11Resource *a = mk_stage(e.dev, 2, f, dims[j][0], dims[j][1], 1, 1, 1, &hr);
            n += snprintf(line + n, sizeof line - n, " %ux%u=%s", dims[j][0], dims[j][1], a ? "ok" : d3dh_hr_name(hr));
            SAFE_RELEASE(a);
        }
        t_note("%s", line);
    }
}

/* UpdateSubresource consumes tightly packed source rows when SrcRowPitch is the unpadded row size. */
static void run_update(UINT grp, int dim)
{
    env11 e; UINT id, wi;
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id];
        if (f->group != grp) continue;
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi], h = TH, d = dim == 3 ? TD : 1;
            UINT rowb = fpt_row_bytes(f, w), rows = fpt_rows(f, h), z, r, i;
            BYTE *src; HRESULT hr; ID3D11Resource *dst = NULL, *st;
            D3D11_MAPPED_SUBRESOURCE m = {0};
            if (fpt_is_bc(f) && w % 4) continue;
            if (dim == 2) {
                D3D11_TEXTURE2D_DESC t = {0};
                t.Width = w; t.Height = h; t.MipLevels = 1; t.ArraySize = 1; t.Format = (DXGI_FORMAT)id; t.SampleDesc.Count = 1; t.Usage = D3D11_USAGE_DEFAULT;
                hr = ID3D11Device_CreateTexture2D(e.dev, &t, NULL, (ID3D11Texture2D **)&dst);
            } else {
                D3D11_TEXTURE3D_DESC t = {0};
                t.Width = w; t.Height = h; t.Depth = d; t.MipLevels = 1; t.Format = (DXGI_FORMAT)id; t.Usage = D3D11_USAGE_DEFAULT;
                hr = ID3D11Device_CreateTexture3D(e.dev, &t, NULL, (ID3D11Texture3D **)&dst);
            }
            if (FAILED(hr)) { if (wi == 0) t_note("%s %s default texture creation: %s", dimname(dim), f->name, d3dh_hr_name(hr)); continue; }
            st = mk_stage(e.dev, dim, f, w, h, d, 1, 1, &hr);
            if (!st) { SAFE_RELEASE(dst); continue; }
            src = (BYTE *)malloc((size_t)rowb * rows * d);
            for (z = 0; z < d; z++) for (r = 0; r < rows; r++) for (i = 0; i < rowb; i++) src[((size_t)z * rows + r) * rowb + i] = pat(id, z, r, i);
            ID3D11DeviceContext_UpdateSubresource(e.ctx, dst, 0, NULL, src, rowb, rowb * rows);
            ID3D11DeviceContext_CopyResource(e.ctx, st, dst);
            hr = ID3D11DeviceContext_Map(e.ctx, st, 0, D3D11_MAP_READ, 0, &m);
            EXPECT_HR(hr, S_OK);
            if (SUCCEEDED(hr)) {
                CHECKF(cmp_rows((BYTE *)m.pData, m.RowPitch, dim == 3 ? m.DepthPitch : m.RowPitch * rows, rowb, rows, d, id),
                       "%s %s w=%u: UpdateSubresource->CopyResource content mismatch", dimname(dim), f->name, w);
                ID3D11DeviceContext_Unmap(e.ctx, st, 0);
            }
            free(src); SAFE_RELEASE(st); SAFE_RELEASE(dst);
        }
    }
}
#define UT(nm, grp, dim) TEST(fpt_d3d11_update_##nm) { run_update(grp, dim); }
UT(tex2d_b8, G_B8, 2) UT(tex2d_b16, G_B16, 2) UT(tex2d_b32, G_B32, 2) UT(tex2d_b64, G_B64, 2) UT(tex2d_b128, G_B128, 2)
UT(tex2d_bc1, G_BC1, 2) UT(tex2d_bc2, G_BC2, 2) UT(tex2d_bc3, G_BC3, 2) UT(tex2d_bc4, G_BC4, 2) UT(tex2d_bc5, G_BC5, 2)
UT(tex2d_bc6h, G_BC6, 2) UT(tex2d_bc7, G_BC7, 2)
UT(tex3d_b8, G_B8, 3) UT(tex3d_b32, G_B32, 3) UT(tex3d_b128, G_B128, 3) UT(tex3d_bc1, G_BC1, 3) UT(tex3d_bc7, G_BC7, 3)

/* Mip chain and array subresource order: pitch of each (mip, slice) derives from that mip's width. */
static void run_mips(UINT grp)
{
    env11 e; UINT id;
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id];
        enum { W = 64, H = 32, MIPS = 7, ARR = 3 };
        UINT a, m; HRESULT hr; ID3D11Resource *res; UINT first[MIPS] = {0}; UINT sup;
        if (f->group != grp) continue;
        sup = fpt_support11(e.dev, id, NULL);
        res = mk_stage(e.dev, 2, f, W, H, 1, MIPS, ARR, &hr);
        if (!res) { if (hard_group(grp) && (sup & D3D11_FORMAT_SUPPORT_TEXTURE2D)) CHECKF(0, "%s mip/array staging creation %s", f->name, d3dh_hr_name(hr)); continue; }
        for (a = 0; a < ARR; a++) {
            UINT prev = 0xffffffffu;
            for (m = 0; m < MIPS; m++) {
                D3D11_MAPPED_SUBRESOURCE ms = {0};
                UINT sub = fpt_calc_sub(m, a, MIPS);
                hr = ID3D11DeviceContext_Map(e.ctx, res, sub, D3D11_MAP_READ, 0, &ms);
                EXPECT_HR(hr, S_OK);
                if (FAILED(hr)) continue;
                CHECKF(ms.RowPitch >= fpt_row_bytes(f, fpt_mip(W, m)), "%s a=%u m=%u RowPitch %u < %u", f->name, a, m, ms.RowPitch, fpt_row_bytes(f, fpt_mip(W, m)));
                CHECKF(ms.RowPitch <= prev, "%s a=%u RowPitch grows with mip level (%u after %u)", f->name, a, ms.RowPitch, prev);
                prev = ms.RowPitch;
                if (a == 0) first[m] = ms.RowPitch; else CHECKF(ms.RowPitch == first[m], "%s m=%u slice %u pitch %u differs from slice 0 pitch %u", f->name, m, a, ms.RowPitch, first[m]);
                ID3D11DeviceContext_Unmap(e.ctx, res, sub);
            }
        }
        SAFE_RELEASE(res);
    }
}
#define MT(nm, grp) TEST(fpt_d3d11_mipchain_##nm) { run_mips(grp); }
MT(b8, G_B8) MT(b16, G_B16) MT(b32, G_B32) MT(b64, G_B64) MT(b96, G_B96) MT(b128, G_B128)
MT(bc1, G_BC1) MT(bc2, G_BC2) MT(bc3, G_BC3) MT(bc4, G_BC4) MT(bc5, G_BC5) MT(bc6h, G_BC6) MT(bc7, G_BC7)

/* MipLevels == 0 expands to the full chain; BC mips below 4x4 still occupy one block. */
TEST(fpt_d3d11_miplevels_zero_full_chain)
{
    env11 e; UINT i; static const UINT sz[][2] = { {1,1}, {2,1}, {4,4}, {5,3}, {16,16}, {64,8}, {100,100}, {1024,1}, {255,256} };
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        D3D11_TEXTURE2D_DESC t = {0}, g = {0}; ID3D11Texture2D *tex = NULL; UINT mx = sz[i][0] > sz[i][1] ? sz[i][0] : sz[i][1], want = 1;
        while (mx > 1) { mx >>= 1; want++; }
        t.Width = sz[i][0]; t.Height = sz[i][1]; t.MipLevels = 0; t.ArraySize = 1; t.Format = DXGI_FORMAT_R8G8B8A8_UNORM; t.SampleDesc.Count = 1;
        t.Usage = D3D11_USAGE_DEFAULT; t.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; t.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        EXPECT_HR(ID3D11Device_CreateTexture2D(e.dev, &t, NULL, &tex), S_OK);
        if (!tex) continue;
        ID3D11Texture2D_GetDesc(tex, &g);
        CHECKF(g.MipLevels == want, "%ux%u MipLevels %u want %u", sz[i][0], sz[i][1], g.MipLevels, want);
        CHECK(g.Width == sz[i][0] && g.Height == sz[i][1] && g.ArraySize == 1);
        SAFE_RELEASE(tex);
    }
}
TEST(fpt_d3d11_miplevels_zero_full_chain_3d)
{
    env11 e; UINT i; static const UINT sz[][3] = { {1,1,1}, {4,4,4}, {8,2,1}, {3,5,17}, {16,16,16}, {2,2,64} };
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        D3D11_TEXTURE3D_DESC t = {0}, g = {0}; ID3D11Texture3D *tex = NULL; UINT mx = sz[i][0], want = 1;
        if (sz[i][1] > mx) mx = sz[i][1];
        if (sz[i][2] > mx) mx = sz[i][2];
        while (mx > 1) { mx >>= 1; want++; }
        t.Width = sz[i][0]; t.Height = sz[i][1]; t.Depth = sz[i][2]; t.MipLevels = 0; t.Format = DXGI_FORMAT_R8G8B8A8_UNORM; t.Usage = D3D11_USAGE_DEFAULT;
        t.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        EXPECT_HR(ID3D11Device_CreateTexture3D(e.dev, &t, NULL, &tex), S_OK);
        if (!tex) continue;
        ID3D11Texture3D_GetDesc(tex, &g);
        CHECKF(g.MipLevels == want, "%ux%ux%u MipLevels %u want %u", sz[i][0], sz[i][1], sz[i][2], g.MipLevels, want);
        SAFE_RELEASE(tex);
    }
}

/* Texture3D mip pitches: each mip's DepthPitch covers its own rows. */
TEST(fpt_d3d11_tex3d_mip_depth_pitch)
{
    env11 e; static const UINT fmts[] = { DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC7_UNORM };
    UINT i, m;
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof fmts / sizeof *fmts; i++) {
        const fpt_fmt *f = &fpt_formats[fmts[i]]; HRESULT hr; ID3D11Resource *res = mk_stage(e.dev, 3, f, 16, 16, 8, 5, 1, &hr);
        if (!res) { t_note("%s 3D mip staging: %s", f->name, d3dh_hr_name(hr)); continue; }
        for (m = 0; m < 5; m++) {
            D3D11_MAPPED_SUBRESOURCE ms = {0};
            UINT w = fpt_mip(16, m), h = fpt_mip(16, m), rows = fpt_rows(f, h);
            hr = ID3D11DeviceContext_Map(e.ctx, res, m, D3D11_MAP_READ, 0, &ms);
            EXPECT_HR(hr, S_OK);
            if (FAILED(hr)) continue;
            CHECKF(ms.RowPitch >= fpt_row_bytes(f, w), "%s m=%u RowPitch %u", f->name, m, ms.RowPitch);
            CHECKF(ms.DepthPitch >= ms.RowPitch * rows, "%s m=%u DepthPitch %u < %u", f->name, m, ms.DepthPitch, ms.RowPitch * rows);
            ID3D11DeviceContext_Unmap(e.ctx, res, m);
        }
        SAFE_RELEASE(res);
    }
}

/* Staging buffers: any ByteWidth; mapped pointer valid, data survives CopyResource. */
TEST(fpt_d3d11_staging_buffer_sizes)
{
    env11 e; static const UINT sz[] = { 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 64, 65, 255, 4096 }; UINT i, k;
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        D3D11_BUFFER_DESC bd = {0}, g = {0}; ID3D11Buffer *a = NULL, *b = NULL; D3D11_MAPPED_SUBRESOURCE m = {0};
        bd.ByteWidth = sz[i]; bd.Usage = D3D11_USAGE_STAGING; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &bd, NULL, &a), S_OK);
        EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &bd, NULL, &b), S_OK);
        if (a && b) {
            ID3D11Buffer_GetDesc(a, &g);
            CHECKF(g.ByteWidth == sz[i], "ByteWidth %u", g.ByteWidth);
            EXPECT_HR(ID3D11DeviceContext_Map(e.ctx, (ID3D11Resource *)a, 0, D3D11_MAP_WRITE, 0, &m), S_OK);
            if (m.pData) { for (k = 0; k < sz[i]; k++) ((BYTE *)m.pData)[k] = (BYTE)(k * 5 + 1); ID3D11DeviceContext_Unmap(e.ctx, (ID3D11Resource *)a, 0); }
            ID3D11DeviceContext_CopyResource(e.ctx, (ID3D11Resource *)b, (ID3D11Resource *)a);
            memset(&m, 0, sizeof m);
            EXPECT_HR(ID3D11DeviceContext_Map(e.ctx, (ID3D11Resource *)b, 0, D3D11_MAP_READ, 0, &m), S_OK);
            if (m.pData) {
                int ok = 1; for (k = 0; k < sz[i]; k++) if (((BYTE *)m.pData)[k] != (BYTE)(k * 5 + 1)) ok = 0;
                CHECKF(ok, "buffer ByteWidth %u content mismatch", sz[i]);
                ID3D11DeviceContext_Unmap(e.ctx, (ID3D11Resource *)b, 0);
            }
        }
        SAFE_RELEASE(a); SAFE_RELEASE(b);
    }
}

TEST(fpt_d3d11_constant_buffer_bytewidth_multiple_of_16)
{
    env11 e; static const UINT sz[] = { 16, 32, 48, 256, 4096, 65536 }; static const UINT bad[] = { 1, 4, 12, 15, 17, 20, 100, 65535 }; UINT i;
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        D3D11_BUFFER_DESC bd = {0}; ID3D11Buffer *b = NULL;
        bd.ByteWidth = sz[i]; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &bd, NULL, &b), S_OK);
        SAFE_RELEASE(b);
    }
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3D11_BUFFER_DESC bd = {0}; ID3D11Buffer *b = (ID3D11Buffer *)1;
        bd.ByteWidth = bad[i]; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &bd, NULL, &b), E_INVALIDARG);
        b = NULL;
    }
}
TEST(fpt_d3d11_constant_buffer_over_4096_elements)
{
    env11 e; D3D11_BUFFER_DESC bd = {0}; ID3D11Buffer *b = NULL; HRESULT hr;
    e.dev = fpt_dev11(&e.ctx, &e.fl);
    if (!e.dev) { t_skip("no D3D11 device"); return; }
    bd.ByteWidth = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16 + 16; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = ID3D11Device_CreateBuffer(e.dev, &bd, NULL, &b);
    t_note("constant buffer of 4097 float4 (FL 0x%x): %s", e.fl, d3dh_hr_name(hr));
    SAFE_RELEASE(b);
}
