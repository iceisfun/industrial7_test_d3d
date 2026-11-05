/* D3D12 GetCopyableFootprints tables: row pitch (256), placement alignment (512), totals, planes. */
#include "fpt.h"

#define ROWPITCH_ALIGN 256u
#define PLACEMENT_ALIGN 512u

static D3D12_RESOURCE_DESC mkdesc(D3D12_RESOURCE_DIMENSION dim, UINT w, UINT h, UINT dp, UINT mips, DXGI_FORMAT f)
{
    D3D12_RESOURCE_DESC d; memset(&d, 0, sizeof d);
    d.Dimension = dim; d.Width = w; d.Height = h; d.DepthOrArraySize = (UINT16)dp; d.MipLevels = (UINT16)mips; d.Format = f;
    d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    return d;
}
static UINT support1(ID3D12Device *dev, UINT f, HRESULT *hr)
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT s; HRESULT h;
    memset(&s, 0, sizeof s); s.Format = (DXGI_FORMAT)f;
    h = ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s);
    if (hr) *hr = h;
    return SUCCEEDED(h) ? (UINT)s.Support1 : 0;
}
static int plane_count(ID3D12Device *dev, UINT f)
{
    D3D12_FEATURE_DATA_FORMAT_INFO i; memset(&i, 0, sizeof i); i.Format = (DXGI_FORMAT)f;
    if (FAILED(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FORMAT_INFO, &i, sizeof i))) return -1;
    return i.PlaneCount;
}
static UINT64 expect_total(UINT64 off, UINT rp, UINT rows, UINT rowb, UINT depth)
{
    return off + (UINT64)rp * rows * (depth - 1) + (UINT64)rp * (rows - 1) + rowb;
}

static const char *dn(D3D12_RESOURCE_DIMENSION d) { return d == D3D12_RESOURCE_DIMENSION_TEXTURE1D ? "tex1d" : d == D3D12_RESOURCE_DIMENSION_TEXTURE2D ? "tex2d" : "tex3d"; }

static void run_group(UINT grp, D3D12_RESOURCE_DIMENSION dim)
{
    ID3D12Device *dev = fpt_dev12(); UINT id, wi;
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id]; UINT bit; HRESULT hr; UINT sup; char line[400]; int n, any = 0;
        if (f->group != grp || (id >= 19 && id <= 22) || id == 46 || id == 47) continue;
        bit = dim == D3D12_RESOURCE_DIMENSION_TEXTURE1D ? D3D12_FORMAT_SUPPORT1_TEXTURE1D : dim == D3D12_RESOURCE_DIMENSION_TEXTURE2D ? D3D12_FORMAT_SUPPORT1_TEXTURE2D : D3D12_FORMAT_SUPPORT1_TEXTURE3D;
        sup = support1(dev, id, &hr);
        if (FAILED(hr) || !(sup & bit)) { t_note("%s %s not supported by D3D12 for this dimension (hr=%s support1=0x%x)", dn(dim), f->name, d3dh_hr_name(hr), sup); continue; }
        n = snprintf(line, sizeof line, "%s %s rowSize/rowPitch/total by width:", dn(dim), f->name);
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi], h = dim == D3D12_RESOURCE_DIMENSION_TEXTURE1D ? 1 : 5, d = dim == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 3 : 1;
            UINT rowb, rows, rp;
            D3D12_RESOURCE_DESC desc = mkdesc(dim, w, h, d, 1, (DXGI_FORMAT)id);
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay; UINT nrows = 0; UINT64 rsz = 0, total = 0;
            if ((w & 1) && !(f->group >= G_B8 && f->group <= G_B128) && !fpt_is_bc(f)) continue;
            rowb = fpt_row_bytes(f, w); rows = fpt_rows(f, h); rp = fpt_align(rowb, ROWPITCH_ALIGN);
            memset(&lay, 0xcd, sizeof lay);
            ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, &lay, &nrows, &rsz, &total);
            CHECKF(rsz == rowb, "%s %s w=%u RowSizeInBytes %llu want %u", dn(dim), f->name, w, (unsigned long long)rsz, rowb);
            CHECKF(nrows == rows, "%s %s w=%u h=%u NumRows %u want %u", dn(dim), f->name, w, h, nrows, rows);
            CHECKF(lay.Offset == 0, "%s %s w=%u Offset %llu", dn(dim), f->name, w, (unsigned long long)lay.Offset);
            CHECKF(lay.Footprint.RowPitch == rp, "%s %s w=%u RowPitch %u want %u", dn(dim), f->name, w, lay.Footprint.RowPitch, rp);
            CHECKF(lay.Footprint.RowPitch % ROWPITCH_ALIGN == 0, "%s %s w=%u RowPitch %u not 256-aligned", dn(dim), f->name, w, lay.Footprint.RowPitch);
            CHECKF(lay.Footprint.Depth == d, "%s %s w=%u footprint Depth %u want %u", dn(dim), f->name, w, lay.Footprint.Depth, d);
            if (f->group >= G_B8 && f->group <= G_B128) {
                CHECKF(lay.Footprint.Width == w && lay.Footprint.Height == h, "%s %s footprint %ux%u want %ux%u", dn(dim), f->name, lay.Footprint.Width, lay.Footprint.Height, w, h);
            } else if (fpt_is_bc(f)) {
                CHECKF(lay.Footprint.Width >= w && lay.Footprint.Height >= h, "%s %s footprint %ux%u smaller than %ux%u", dn(dim), f->name, lay.Footprint.Width, lay.Footprint.Height, w, h);
                if (lay.Footprint.Width != fpt_align(w, 4) || lay.Footprint.Height != fpt_align(h, 4)) t_note("%s %s w=%u: footprint size %ux%u (not rounded to 4)", dn(dim), f->name, w, lay.Footprint.Width, lay.Footprint.Height);
            } else if (lay.Footprint.Width != w || lay.Footprint.Height != h) t_note("%s %s footprint size %ux%u for %ux%u", dn(dim), f->name, lay.Footprint.Width, lay.Footprint.Height, w, h);
            CHECKF(total == expect_total(0, rp, rows, rowb, d), "%s %s w=%u TotalBytes %llu want %llu", dn(dim), f->name, w, (unsigned long long)total, (unsigned long long)expect_total(0, rp, rows, rowb, d));
            if (wi == 0 && lay.Footprint.Format != (DXGI_FORMAT)id) t_note("%s %s footprint format reported as %u", dn(dim), f->name, lay.Footprint.Format);
            n += snprintf(line + n, sizeof line - n, " %u/%u/%llu", rowb, lay.Footprint.RowPitch, (unsigned long long)total);
            any = 1;
        }
        if (any) t_note("%s", line);
    }
}
#define GT(nm, grp, dim) TEST(fpt_d3d12_footprint_##nm) { run_group(grp, dim); }
#define T2 D3D12_RESOURCE_DIMENSION_TEXTURE2D
#define T1 D3D12_RESOURCE_DIMENSION_TEXTURE1D
#define T3 D3D12_RESOURCE_DIMENSION_TEXTURE3D
GT(tex2d_b8, G_B8, T2) GT(tex2d_b16, G_B16, T2) GT(tex2d_b32, G_B32, T2) GT(tex2d_b64, G_B64, T2) GT(tex2d_b96, G_B96, T2) GT(tex2d_b128, G_B128, T2)
GT(tex2d_bc1, G_BC1, T2) GT(tex2d_bc2, G_BC2, T2) GT(tex2d_bc3, G_BC3, T2) GT(tex2d_bc4, G_BC4, T2) GT(tex2d_bc5, G_BC5, T2) GT(tex2d_bc6h, G_BC6, T2) GT(tex2d_bc7, G_BC7, T2)
GT(tex2d_packed422, G_422, T2) GT(tex2d_r1, G_R1, T2) GT(tex2d_video_packed, G_VPACK, T2) GT(tex2d_palettized, G_PAL, T2)
GT(tex1d_b8, G_B8, T1) GT(tex1d_b16, G_B16, T1) GT(tex1d_b32, G_B32, T1) GT(tex1d_b64, G_B64, T1) GT(tex1d_b128, G_B128, T1)
GT(tex3d_b8, G_B8, T3) GT(tex3d_b16, G_B16, T3) GT(tex3d_b32, G_B32, T3) GT(tex3d_b64, G_B64, T3) GT(tex3d_b128, G_B128, T3)
GT(tex3d_bc1, G_BC1, T3) GT(tex3d_bc3, G_BC3, T3) GT(tex3d_bc7, G_BC7, T3)

/* Height rounding for block compression and tall textures. */
TEST(fpt_d3d12_footprint_bc_height_rounding)
{
    ID3D12Device *dev = fpt_dev12(); static const UINT hs[] = { 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17 }; UINT i;
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (i = 0; i < sizeof hs / sizeof *hs; i++) {
        D3D12_RESOURCE_DESC desc = mkdesc(T2, 16, hs[i], 1, 1, DXGI_FORMAT_BC1_UNORM);
        UINT nrows = 0; UINT64 rsz = 0, total = 0;
        ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, NULL, &nrows, &rsz, &total);
        CHECKF(nrows == fpt_ceil_div(hs[i], 4), "BC1 height %u NumRows %u", hs[i], nrows);
        CHECKF(rsz == 32, "BC1 width 16 RowSizeInBytes %llu", (unsigned long long)rsz);
        CHECKF(total == 256ull * (nrows - 1) + 32, "BC1 height %u total %llu", hs[i], (unsigned long long)total);
    }
}
TEST(fpt_d3d12_footprint_optional_outputs)
{
    ID3D12Device *dev = fpt_dev12(); D3D12_RESOURCE_DESC desc; D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay; UINT nr; UINT64 rs, t1, t2, t3;
    if (!dev) { t_skip("no D3D12 device"); return; }
    desc = mkdesc(T2, 33, 17, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    t1 = t2 = t3 = 0xdeadbeef;
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, &lay, &nr, &rs, &t1);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, NULL, NULL, NULL, &t2);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, NULL, NULL, NULL, NULL);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, &lay, NULL, NULL, NULL);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, NULL, &nr, NULL, NULL);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, NULL, NULL, &rs, &t3);
    CHECK(t1 == t2 && t2 == t3);
    CHECKF(t1 == 256ull * 16 + 132, "total %llu", (unsigned long long)t1);
}
TEST(fpt_d3d12_footprint_base_offset)
{
    ID3D12Device *dev = fpt_dev12(); D3D12_RESOURCE_DESC desc; D3D12_PLACED_SUBRESOURCE_FOOTPRINT l0, l1; UINT nr; UINT64 rs, t0 = 0, t1 = 0;
    if (!dev) { t_skip("no D3D12 device"); return; }
    desc = mkdesc(T2, 40, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 0, &l0, &nr, &rs, &t0);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 4096, &l1, &nr, &rs, &t1);
    CHECKF(l1.Offset == 4096, "Offset with BaseOffset 4096 = %llu", (unsigned long long)l1.Offset);
    CHECKF(l1.Footprint.RowPitch == l0.Footprint.RowPitch, "pitch changed with base offset");
    CHECKF(t1 == t0, "TotalBytes with base offset %llu vs %llu (does not include BaseOffset)", (unsigned long long)t1, (unsigned long long)t0);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 1, 1000, &l1, &nr, &rs, &t1);
    t_note("BaseOffset 1000 (unaligned): reported Offset=%llu total=%llu", (unsigned long long)l1.Offset, (unsigned long long)t1);
}

/* Mip chain x array: offsets aligned to 512, strictly increasing, dimension of each level. */
typedef struct { DXGI_FORMAT f; } mf;
static void mip_array(D3D12_RESOURCE_DIMENSION dim, DXGI_FORMAT fmt, UINT w, UINT h, UINT dp, UINT mips, UINT arr)
{
    ID3D12Device *dev = fpt_dev12(); const fpt_fmt *f = &fpt_formats[fmt]; D3D12_RESOURCE_DESC desc;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay[64]; UINT nr[64]; UINT64 rs[64], total = 0; UINT n, i; UINT64 prev_end = 0; HRESULT hr; UINT sup;
    if (!dev) { t_skip("no D3D12 device"); return; }
    sup = support1(dev, fmt, &hr);
    if (!(sup & (dim == T3 ? D3D12_FORMAT_SUPPORT1_TEXTURE3D : D3D12_FORMAT_SUPPORT1_TEXTURE2D))) { t_skip("format %s unsupported", f->name); return; }
    desc = mkdesc(dim, w, h, dim == T3 ? dp : arr, mips, fmt);
    n = mips * (dim == T3 ? 1 : arr);
    memset(lay, 0, sizeof lay);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, n, 0, lay, nr, rs, &total);
    for (i = 0; i < n; i++) {
        UINT m = i % mips, mw = fpt_mip(w, m), mh = fpt_mip(h, m), md = dim == T3 ? fpt_mip(dp, m) : 1;
        UINT rowb = fpt_row_bytes(f, mw), rows = fpt_rows(f, mh), rp = fpt_align(rowb, ROWPITCH_ALIGN);
        CHECKF(rs[i] == rowb && nr[i] == rows, "sub %u (mip %u): row size %llu/%u rows %u/%u", i, m, (unsigned long long)rs[i], rowb, nr[i], rows);
        CHECKF(lay[i].Footprint.RowPitch == rp, "sub %u RowPitch %u want %u", i, lay[i].Footprint.RowPitch, rp);
        CHECKF(lay[i].Footprint.Depth == md, "sub %u Depth %u want %u", i, lay[i].Footprint.Depth, md);
        CHECKF(lay[i].Offset % PLACEMENT_ALIGN == 0, "sub %u Offset %llu not 512-aligned", i, (unsigned long long)lay[i].Offset);
        if (i == 0) CHECK(lay[i].Offset == 0); else CHECKF(lay[i].Offset >= prev_end, "sub %u Offset %llu overlaps previous end %llu", i, (unsigned long long)lay[i].Offset, (unsigned long long)prev_end);
        prev_end = expect_total(lay[i].Offset, rp, rows, rowb, md);
    }
    CHECKF(total == prev_end, "total %llu want end of last subresource %llu", (unsigned long long)total, (unsigned long long)prev_end);
}
#define MA(nm, dim, fmt, w, h, dp, mips, arr) TEST(fpt_d3d12_mipchain_##nm) { mip_array(dim, DXGI_FORMAT_##fmt, w, h, dp, mips, arr); }
MA(r8_2d, T2, R8_UNORM, 33, 17, 1, 6, 3)         MA(rgba8_2d, T2, R8G8B8A8_UNORM, 64, 64, 1, 7, 2)
MA(rgba32f_2d, T2, R32G32B32A32_FLOAT, 100, 20, 1, 7, 2) MA(r16g16_2d, T2, R16G16_UNORM, 5, 3, 1, 3, 4)
MA(b5g6r5_2d, T2, B5G6R5_UNORM, 31, 31, 1, 5, 2)  MA(bc1_2d, T2, BC1_UNORM, 33, 17, 1, 6, 3)
MA(bc3_2d, T2, BC3_UNORM, 64, 16, 1, 7, 2)        MA(bc7_2d, T2, BC7_UNORM, 17, 31, 1, 5, 2)
MA(bc4_2d, T2, BC4_UNORM, 7, 7, 1, 3, 2)          MA(bc6h_2d, T2, BC6H_UF16, 8, 8, 1, 4, 1)
MA(rgba8_3d, T3, R8G8B8A8_UNORM, 16, 8, 8, 5, 1)  MA(r8_3d, T3, R8_UNORM, 5, 7, 11, 4, 1)
MA(bc1_3d, T3, BC1_UNORM, 16, 16, 16, 5, 1)       MA(rgba16f_3d, T3, R16G16B16A16_FLOAT, 9, 9, 9, 4, 1)

/* FirstSubresource selects the right level. */
TEST(fpt_d3d12_first_subresource_selects_level)
{
    ID3D12Device *dev = fpt_dev12(); D3D12_RESOURCE_DESC desc; D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay[3]; UINT nr[3]; UINT64 rs[3], tot; UINT first;
    if (!dev) { t_skip("no D3D12 device"); return; }
    desc = mkdesc(T2, 64, 32, 3, 4, DXGI_FORMAT_R8G8B8A8_UNORM); /* 3 slices x 4 mips */
    for (first = 0; first < 12; first += 5) {
        UINT m = first % 4, a = first / 4, k;
        UINT n = first + 3 <= 12 ? 3 : 12 - first;
        ID3D12Device_GetCopyableFootprints(&*dev, &desc, first, n, 0, lay, nr, rs, &tot);
        (void)a;
        for (k = 0; k < n; k++) {
            UINT mm = (first + k) % 4;
            CHECKF(rs[k] == 4 * fpt_mip(64, mm) && nr[k] == fpt_mip(32, mm), "first=%u k=%u row %llu rows %u", first, k, (unsigned long long)rs[k], nr[k]);
        }
        CHECKF(lay[0].Offset == 0, "first=%u first layout offset %llu", first, (unsigned long long)lay[0].Offset);
        (void)m;
    }
}

/* Formats with planes. */
typedef struct { DXGI_FORMAT f; int planes; } pc_row;
TEST(fpt_d3d12_format_info_plane_count)
{
    ID3D12Device *dev = fpt_dev12(); UINT i;
    static const pc_row t[] = {
        { DXGI_FORMAT_NV12, 2 }, { DXGI_FORMAT_P010, 2 }, { DXGI_FORMAT_P016, 2 },
        { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 2 }, { DXGI_FORMAT_D24_UNORM_S8_UINT, 2 },
        { DXGI_FORMAT_R32G8X24_TYPELESS, 2 }, { DXGI_FORMAT_R24G8_TYPELESS, 2 },
        { DXGI_FORMAT_YUY2, 1 }, { DXGI_FORMAT_AYUV, 1 }, { DXGI_FORMAT_Y410, 1 }, { DXGI_FORMAT_Y210, 1 },
        { DXGI_FORMAT_R8G8B8A8_UNORM, 1 }, { DXGI_FORMAT_BC1_UNORM, 1 }, { DXGI_FORMAT_BC7_UNORM, 1 }, { DXGI_FORMAT_R8_UNORM, 1 },
        { DXGI_FORMAT_D32_FLOAT, 1 }, { DXGI_FORMAT_D16_UNORM, 1 }, { DXGI_FORMAT_R32_TYPELESS, 1 }, { DXGI_FORMAT_R16_TYPELESS, 1 },
        { DXGI_FORMAT_R32G32B32A32_FLOAT, 1 },
    };
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        int pc = plane_count(dev, t[i].f);
        if (pc < 0) { t_note("FORMAT_INFO failed for %u", t[i].f); continue; }
        CHECKF(pc == t[i].planes, "%s PlaneCount %d want %d", fpt_formats[t[i].f].name, pc, t[i].planes);
    }
}
TEST(fpt_d3d12_format_info_all_formats_note)
{
    ID3D12Device *dev = fpt_dev12(); UINT id;
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (id = 1; id < FPT_NFORMATS; id++) {
        HRESULT hr; UINT s; int pc;
        if (fpt_formats[id].group == G_NONE) continue;
        s = support1(dev, id, &hr); pc = plane_count(dev, id);
        t_note("d3d12 %-30s planes=%d hr=%s support1=0x%08x", fpt_formats[id].name, pc, d3dh_hr_name(hr), s);
    }
}
TEST(fpt_d3d12_format_info_undefined_ids_fail)
{
    ID3D12Device *dev = fpt_dev12(); static const UINT ids[] = { 116, 125, 129, 133, 0x7fffffff }; UINT i;
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (i = 0; i < sizeof ids / sizeof *ids; i++) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT s; HRESULT hr; memset(&s, 0, sizeof s); s.Format = (DXGI_FORMAT)ids[i];
        hr = ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s);
        CHECKF(FAILED(hr) || s.Support1 == 0, "undefined format %u reported support 0x%x", ids[i], s.Support1);
    }
}

static void planar_case(DXGI_FORMAT fmt, UINT w, UINT h, UINT lumab, UINT chromab, int hard)
{
    ID3D12Device *dev = fpt_dev12(); D3D12_RESOURCE_DESC desc; D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay[2]; UINT nr[2]; UINT64 rs[2], tot = 0;
    UINT rp0, rp1, crows = (h + 1) / 2;
    if (!dev) return;
    desc = mkdesc(T2, w, h, 1, 1, fmt);
    memset(lay, 0, sizeof lay);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 2, 0, lay, nr, rs, &tot);
    rp0 = fpt_align(w * lumab, ROWPITCH_ALIGN); rp1 = fpt_align((w / 2) * chromab, ROWPITCH_ALIGN);
    if (!hard) { t_note("%s %ux%u: rows %u/%u rowsize %llu/%llu pitch %u/%u off1 %llu total %llu", fpt_formats[fmt].name, w, h, nr[0], nr[1], (unsigned long long)rs[0], (unsigned long long)rs[1], lay[0].Footprint.RowPitch, lay[1].Footprint.RowPitch, (unsigned long long)lay[1].Offset, (unsigned long long)tot); return; }
    CHECKF(nr[0] == h && nr[1] == crows, "%s %ux%u rows %u/%u want %u/%u", fpt_formats[fmt].name, w, h, nr[0], nr[1], h, crows);
    CHECKF(rs[0] == (UINT64)w * lumab && rs[1] == (UINT64)(w / 2) * chromab, "%s %ux%u row sizes %llu/%llu", fpt_formats[fmt].name, w, h, (unsigned long long)rs[0], (unsigned long long)rs[1]);
    CHECKF(lay[0].Footprint.RowPitch == rp0 && lay[1].Footprint.RowPitch == rp1, "%s %ux%u pitches %u/%u want %u/%u", fpt_formats[fmt].name, w, h, lay[0].Footprint.RowPitch, lay[1].Footprint.RowPitch, rp0, rp1);
    CHECK(lay[0].Offset == 0);
    CHECKF(lay[1].Offset % PLACEMENT_ALIGN == 0 && lay[1].Offset >= (UINT64)rp0 * (h - 1) + rs[0], "%s %ux%u plane1 offset %llu", fpt_formats[fmt].name, w, h, (unsigned long long)lay[1].Offset);
    CHECKF(tot == lay[1].Offset + (UINT64)rp1 * (crows - 1) + rs[1], "%s %ux%u total %llu", fpt_formats[fmt].name, w, h, (unsigned long long)tot);
}
static void planar_table(DXGI_FORMAT fmt, UINT lumab, UINT chromab)
{
    ID3D12Device *dev = fpt_dev12(); static const UINT ws[] = { 2, 4, 8, 16, 64 }, hs[] = { 2, 4, 16, 64 }; UINT i, j;
    if (!dev) { t_skip("no D3D12 device"); return; }
    if (plane_count(dev, fmt) != 2 || !(support1(dev, fmt, NULL) & D3D12_FORMAT_SUPPORT1_TEXTURE2D)) { t_skip("%s not a supported planar texture format", fpt_formats[fmt].name); return; }
    for (i = 0; i < 5; i++) for (j = 0; j < 4; j++) planar_case(fmt, ws[i], hs[j], lumab, chromab, 1);
    { static const UINT oddw[] = { 1, 3, 5, 7, 15, 17, 31 }; for (i = 0; i < 7; i++) planar_case(fmt, oddw[i], 5, lumab, chromab, 0); }
}
TEST(fpt_d3d12_footprint_nv12) { planar_table(DXGI_FORMAT_NV12, 1, 2); }
TEST(fpt_d3d12_footprint_p010) { planar_table(DXGI_FORMAT_P010, 2, 4); }
TEST(fpt_d3d12_footprint_p016) { planar_table(DXGI_FORMAT_P016, 2, 4); }

/* Depth-stencil: plane 0 holds depth, plane 1 stencil (one byte per texel). */
static void ds_planes(DXGI_FORMAT fmt, UINT depthb)
{
    ID3D12Device *dev = fpt_dev12(); UINT wi;
    if (!dev) { t_skip("no D3D12 device"); return; }
    if (plane_count(dev, fmt) != 2) { t_skip("%s: no 2 planes", fpt_formats[fmt].name); return; }
    for (wi = 0; wi < FPT_NW; wi++) {
        UINT w = fpt_widths[wi], h = 5, rp0 = fpt_align(w * depthb, 256), rp1 = fpt_align(w, 256);
        D3D12_RESOURCE_DESC desc = mkdesc(T2, w, h, 1, 1, fmt); D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay[2]; UINT nr[2]; UINT64 rs[2], tot = 0;
        memset(lay, 0, sizeof lay);
        ID3D12Device_GetCopyableFootprints(dev, &desc, 0, 2, 0, lay, nr, rs, &tot);
        CHECKF(nr[0] == h && nr[1] == h, "%s w=%u rows %u/%u", fpt_formats[fmt].name, w, nr[0], nr[1]);
        CHECKF(rs[0] == (UINT64)w * depthb && rs[1] == w, "%s w=%u row sizes %llu/%llu", fpt_formats[fmt].name, w, (unsigned long long)rs[0], (unsigned long long)rs[1]);
        CHECKF(lay[0].Footprint.RowPitch == rp0 && lay[1].Footprint.RowPitch == rp1, "%s w=%u pitches %u/%u want %u/%u", fpt_formats[fmt].name, w, lay[0].Footprint.RowPitch, lay[1].Footprint.RowPitch, rp0, rp1);
        CHECKF(lay[1].Offset % 512 == 0 && lay[1].Offset >= (UINT64)rp0 * (h - 1) + rs[0], "%s w=%u plane1 offset %llu", fpt_formats[fmt].name, w, (unsigned long long)lay[1].Offset);
        CHECKF(tot == lay[1].Offset + (UINT64)rp1 * (h - 1) + rs[1], "%s w=%u total %llu", fpt_formats[fmt].name, w, (unsigned long long)tot);
    }
}
TEST(fpt_d3d12_footprint_d24s8_planes) { ds_planes(DXGI_FORMAT_D24_UNORM_S8_UINT, 4); }
TEST(fpt_d3d12_footprint_d32s8_planes) { ds_planes(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 4); }
TEST(fpt_d3d12_footprint_r24g8_typeless_planes) { ds_planes(DXGI_FORMAT_R24G8_TYPELESS, 4); }
TEST(fpt_d3d12_footprint_r32g8x24_typeless_planes) { ds_planes(DXGI_FORMAT_R32G8X24_TYPELESS, 4); }

/* D3D12CalcSubresource arithmetic against the order GetCopyableFootprints uses (mip fastest, then array, then plane). */
TEST(fpt_d3d12_calc_subresource_matches_footprint_order)
{
    ID3D12Device *dev = fpt_dev12(); enum { M = 4, A = 3, P = 2, N = M * A * P };
    D3D12_RESOURCE_DESC desc; D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay[N]; UINT nr[N]; UINT64 rs[N], tot; UINT m, a, p;
    if (!dev) { t_skip("no D3D12 device"); return; }
    if (plane_count(dev, DXGI_FORMAT_D24_UNORM_S8_UINT) != 2) { t_skip("D24S8 not two-plane"); return; }
    desc = mkdesc(T2, 64, 32, A, M, DXGI_FORMAT_D24_UNORM_S8_UINT);
    ID3D12Device_GetCopyableFootprints(dev, &desc, 0, N, 0, lay, nr, rs, &tot);
    for (p = 0; p < P; p++) for (a = 0; a < A; a++) for (m = 0; m < M; m++) {
        UINT idx = fpt_calc_sub12(m, a, p, M, A);
        UINT want_rs = (p ? 1 : 4) * fpt_mip(64, m);
        CHECKF(idx < N && rs[idx] == want_rs && nr[idx] == fpt_mip(32, m), "plane %u slice %u mip %u -> index %u: rowsize %llu want %u rows %u", p, a, m, idx, (unsigned long long)rs[idx], want_rs, nr[idx]);
    }
    CHECK(fpt_calc_sub12(0, 0, 0, M, A) == 0 && fpt_calc_sub12(M - 1, A - 1, P - 1, M, A) == N - 1);
    CHECK(fpt_calc_sub12(3, 2, 1, 4, 3) == 3 + 2 * 4 + 1 * 12);
}
TEST(fpt_d3d12_calc_subresource_arithmetic)
{
    UINT m, a, seen[3 * 7 * 2] = {0}; UINT p;
    for (p = 0; p < 2; p++) for (a = 0; a < 3; a++) for (m = 0; m < 7; m++) {
        UINT i = fpt_calc_sub12(m, a, p, 7, 3);
        CHECK(i == m + a * 7 + p * 21);
        CHECK(i < 42 && seen[i] == 0); seen[i]++;
    }
    CHECK(fpt_calc_sub(0, 0, 1) == 0 && fpt_calc_sub(0, 5, 1) == 5 && fpt_calc_sub(3, 2, 4) == 11 && fpt_calc_sub(14, 2047, 15) == 14 + 2047 * 15);
}

/* Limits: sizes above the D3D12 maxima must be rejected by resource creation. */
static HRESULT create_tex(ID3D12Device *dev, D3D12_RESOURCE_DIMENSION dim, UINT w, UINT h, UINT dp, UINT mips, DXGI_FORMAT f, ID3D12Resource **out)
{
    D3D12_HEAP_PROPERTIES hp; D3D12_RESOURCE_DESC d = mkdesc(dim, w, h, dp, mips, f);
    memset(&hp, 0, sizeof hp); hp.Type = D3D12_HEAP_TYPE_DEFAULT; hp.CreationNodeMask = 1; hp.VisibleNodeMask = 1;
    *out = NULL;
    return ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)out);
}
TEST(fpt_d3d12_limit_texture_dimensions)
{
    ID3D12Device *dev = fpt_dev12(); ID3D12Resource *r; HRESULT hr;
    if (!dev) { t_skip("no D3D12 device"); return; }
    hr = create_tex(dev, T1, D3D12_REQ_TEXTURE1D_U_DIMENSION, 1, 1, 1, DXGI_FORMAT_R8_UNORM, &r); EXPECT_HR(hr, S_OK); SAFE_RELEASE(r);
    hr = create_tex(dev, T1, D3D12_REQ_TEXTURE1D_U_DIMENSION + 1, 1, 1, 1, DXGI_FORMAT_R8_UNORM, &r); CHECKF(FAILED(hr), "1D over limit: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    hr = create_tex(dev, T2, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION, 1, 1, 1, DXGI_FORMAT_R8_UNORM, &r); EXPECT_HR(hr, S_OK); SAFE_RELEASE(r);
    hr = create_tex(dev, T2, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION, 1, 1, DXGI_FORMAT_R8_UNORM, &r); EXPECT_HR(hr, S_OK); SAFE_RELEASE(r);
    hr = create_tex(dev, T2, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1, 1, 1, 1, DXGI_FORMAT_R8_UNORM, &r); CHECKF(FAILED(hr), "2D width over limit: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    hr = create_tex(dev, T2, 1, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1, 1, 1, DXGI_FORMAT_R8_UNORM, &r); CHECKF(FAILED(hr), "2D height over limit: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    hr = create_tex(dev, T3, D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION, 1, 1, 1, DXGI_FORMAT_R8_UNORM, &r); EXPECT_HR(hr, S_OK); SAFE_RELEASE(r);
    hr = create_tex(dev, T3, D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION + 1, 1, 1, 1, DXGI_FORMAT_R8_UNORM, &r); CHECKF(FAILED(hr), "3D width over limit: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    hr = create_tex(dev, T3, 1, 1, D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION + 1, 1, DXGI_FORMAT_R8_UNORM, &r); CHECKF(FAILED(hr), "3D depth over limit: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
}
TEST(fpt_d3d12_limit_texture_array_size)
{
    ID3D12Device *dev = fpt_dev12(); ID3D12Resource *r; HRESULT hr;
    if (!dev) { t_skip("no D3D12 device"); return; }
    hr = create_tex(dev, T2, 1, 1, D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION, 1, DXGI_FORMAT_R8_UNORM, &r); EXPECT_HR(hr, S_OK); SAFE_RELEASE(r);
    hr = create_tex(dev, T2, 1, 1, D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION + 1, 1, DXGI_FORMAT_R8_UNORM, &r); CHECKF(FAILED(hr), "array over limit: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    hr = create_tex(dev, T1, 1, 1, D3D12_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION, 1, DXGI_FORMAT_R8_UNORM, &r); EXPECT_HR(hr, S_OK); SAFE_RELEASE(r);
    hr = create_tex(dev, T1, 1, 1, D3D12_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION + 1, 1, DXGI_FORMAT_R8_UNORM, &r); CHECKF(FAILED(hr), "1D array over limit: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
}
TEST(fpt_d3d12_limit_mip_levels)
{
    ID3D12Device *dev = fpt_dev12(); ID3D12Resource *r; HRESULT hr; UINT m;
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (m = 1; m <= D3D12_REQ_MIP_LEVELS; m++) {
        hr = create_tex(dev, T2, 1u << (m - 1), 1, 1, m, DXGI_FORMAT_R8_UNORM, &r); CHECKF(hr == S_OK, "width %u mips %u: %s", 1u << (m - 1), m, d3dh_hr_name(hr)); SAFE_RELEASE(r);
        hr = create_tex(dev, T2, 1u << (m - 1), 1, 1, m + 1, DXGI_FORMAT_R8_UNORM, &r); CHECKF(FAILED(hr), "width %u mips %u accepted", 1u << (m - 1), m + 1); SAFE_RELEASE(r);
    }
}
TEST(fpt_d3d12_resource_allocation_buffer_64k)
{
    ID3D12Device *dev = fpt_dev12(); static const UINT64 sz[] = { 1, 4, 255, 256, 4096, 65535, 65536, 65537, 131072 }; UINT i;
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (i = 0; i < sizeof sz / sizeof *sz; i++) {
        D3D12_RESOURCE_DESC d; D3D12_RESOURCE_ALLOCATION_INFO info; memset(&d, 0, sizeof d);
        d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; d.Width = sz[i]; d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1; d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        memset(&info, 0, sizeof info);
        dev->lpVtbl->GetResourceAllocationInfo(dev, &info, 0, 1, &d);
        CHECKF(info.Alignment == 65536, "buffer %llu alignment %llu", (unsigned long long)sz[i], (unsigned long long)info.Alignment);
        CHECKF(info.SizeInBytes == fpt_align((UINT)sz[i], 65536), "buffer %llu size %llu", (unsigned long long)sz[i], (unsigned long long)info.SizeInBytes);
    }
}
TEST(fpt_d3d12_resource_allocation_texture_covers_footprint)
{
    ID3D12Device *dev = fpt_dev12(); UINT i; static const DXGI_FORMAT fm[] = { DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC7_UNORM };
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (i = 0; i < sizeof fm / sizeof *fm; i++) {
        D3D12_RESOURCE_DESC d = mkdesc(T2, 64, 64, 1, 1, fm[i]); D3D12_RESOURCE_ALLOCATION_INFO info; UINT64 total = 0;
        memset(&info, 0, sizeof info);
        dev->lpVtbl->GetResourceAllocationInfo(dev, &info, 0, 1, &d);
        ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, NULL, NULL, NULL, &total);
        CHECKF(info.SizeInBytes >= fpt_row_bytes(&fpt_formats[fm[i]], 64) * fpt_rows(&fpt_formats[fm[i]], 64), "%s allocation %llu smaller than raw data", fpt_formats[fm[i]].name, (unsigned long long)info.SizeInBytes);
        CHECKF(info.Alignment == 65536 || info.Alignment == 4096, "%s alignment %llu", fpt_formats[fm[i]].name, (unsigned long long)info.Alignment);
        CHECKF(info.SizeInBytes % info.Alignment == 0, "%s size %llu not multiple of alignment %llu", fpt_formats[fm[i]].name, (unsigned long long)info.SizeInBytes, (unsigned long long)info.Alignment);
        t_note("%s 64x64: alloc size %llu alignment %llu, footprint total %llu", fpt_formats[fm[i]].name, (unsigned long long)info.SizeInBytes, (unsigned long long)info.Alignment, (unsigned long long)total);
    }
}
TEST(fpt_d3d12_constants)
{
    CHECK(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 256 && D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT == 512);
    CHECK(D3D12_REQ_TEXTURE1D_U_DIMENSION == 16384 && D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION == 16384 && D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION == 2048);
    CHECK(D3D12_REQ_TEXTURECUBE_DIMENSION == 16384 && D3D12_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION == 2048 && D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION == 2048);
    CHECK(D3D12_REQ_MIP_LEVELS == 15 && D3D12_REQ_SUBRESOURCES == 30720 && D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT == 4096);
    CHECK(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT == 65536 && D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT == 4096 && D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT == 4194304);
    CHECK(D3D12_REQ_MAXANISOTROPY == 16 && D3D12_REQ_BUFFER_RESOURCE_TEXEL_COUNT_2_TO_EXP == 27);
    CHECK(D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT == 16 && D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT == 128 && D3D12_UAV_SLOT_COUNT == 64);
    CHECK(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT == 8 && D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE == 16 && D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT == 32);
}
