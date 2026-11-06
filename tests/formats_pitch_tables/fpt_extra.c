/* Capability tables measured on Windows 10 (WARP): CheckFormatSupport2, multisample quality levels, D3D12 allocation sizes, footprint range rules. */
#include "fpt.h"

#define F(n) DXGI_FORMAT_##n

static D3D12_RESOURCE_DESC mk(D3D12_RESOURCE_DIMENSION dim, UINT64 w, UINT h, UINT dp, UINT mips, DXGI_FORMAT f, UINT msaa, D3D12_RESOURCE_FLAGS fl)
{
    D3D12_RESOURCE_DESC d; memset(&d, 0, sizeof d);
    d.Dimension = dim; d.Width = w; d.Height = h; d.DepthOrArraySize = (UINT16)dp; d.MipLevels = (UINT16)mips; d.Format = f; d.SampleDesc.Count = msaa; d.Flags = fl;
    return d;
}

TEST(fpt_d3d11_support2_table)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT id;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        D3D11_FEATURE_DATA_FORMAT_SUPPORT2 s; HRESULT hr; char key[80];
        if (fpt_formats[id].group == G_NONE) continue;
        memset(&s, 0, sizeof s); s.InFormat = (DXGI_FORMAT)id;
        hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_FORMAT_SUPPORT2, &s, sizeof s);
        snprintf(key, sizeof key, "d11s2 %s", fpt_formats[id].name);
        FPT_GOLDEN(key, "%s 0x%08x", d3dh_hr_name(hr), SUCCEEDED(hr) ? s.OutFormatSupport2 : 0);
    }
    /* size mismatch and undefined formats */
    { D3D11_FEATURE_DATA_FORMAT_SUPPORT2 s; memset(&s, 0, sizeof s); s.InFormat = F(R8G8B8A8_UNORM);
      EXPECT_HR(ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_FORMAT_SUPPORT2, &s, sizeof s - 1), E_INVALIDARG);
      EXPECT_HR(ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_FORMAT_SUPPORT2, &s, sizeof s + 4), E_INVALIDARG);
      s.InFormat = (DXGI_FORMAT)200;
      EXPECT_HR(ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_FORMAT_SUPPORT2, &s, sizeof s), E_FAIL); }
}

static const DXGI_FORMAT k_msaa_fmts[] = { F(R8G8B8A8_UNORM), F(R8G8B8A8_UNORM_SRGB), F(B8G8R8A8_UNORM), F(R10G10B10A2_UNORM), F(R16G16B16A16_FLOAT), F(R32G32B32A32_FLOAT),
    F(R16_UNORM), F(R8_UNORM), F(R32_FLOAT), F(R32_UINT), F(R11G11B10_FLOAT), F(D16_UNORM), F(D24_UNORM_S8_UINT), F(D32_FLOAT), F(D32_FLOAT_S8X24_UINT),
    F(R24G8_TYPELESS), F(BC1_UNORM), F(R8G8_UNORM), F(R16G16_FLOAT), F(R32G32B32_FLOAT), F(B5G6R5_UNORM), F(UNKNOWN) };
static const UINT k_msaa_counts[] = { 1, 2, 4, 8, 16, 32 };

TEST(fpt_d3d11_msaa_quality_table)
{
    ID3D11Device *dev = fpt_dev11(NULL, NULL); UINT i, c;
    if (!dev) { t_skip("no D3D11 device"); return; }
    for (i = 0; i < sizeof k_msaa_fmts / sizeof *k_msaa_fmts; i++) for (c = 0; c < sizeof k_msaa_counts / sizeof *k_msaa_counts; c++) {
        UINT q = 0xdead; HRESULT hr = ID3D11Device_CheckMultisampleQualityLevels(dev, k_msaa_fmts[i], k_msaa_counts[c], &q); char key[100];
        snprintf(key, sizeof key, "d11msaa %s x%u", fpt_formats[k_msaa_fmts[i]].name, k_msaa_counts[c]);
        FPT_GOLDEN(key, "%s %u", d3dh_hr_name(hr), q);
    }
    /* count 0, non-power-of-two, NULL out */
    { UINT q = 0xdead;
      FPT_GOLDEN("d11msaa count 0", "%s %u", d3dh_hr_name(ID3D11Device_CheckMultisampleQualityLevels(dev, F(R8G8B8A8_UNORM), 0, &q)), q);
      q = 0xdead;
      FPT_GOLDEN("d11msaa count 3", "%s %u", d3dh_hr_name(ID3D11Device_CheckMultisampleQualityLevels(dev, F(R8G8B8A8_UNORM), 3, &q)), q); }
}

TEST(fpt_d3d12_msaa_quality_table)
{
    ID3D12Device *dev = fpt_dev12(); UINT i, c;
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (i = 0; i < sizeof k_msaa_fmts / sizeof *k_msaa_fmts; i++) for (c = 0; c < sizeof k_msaa_counts / sizeof *k_msaa_counts; c++) {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS m; HRESULT hr; char key[100];
        memset(&m, 0, sizeof m); m.Format = k_msaa_fmts[i]; m.SampleCount = k_msaa_counts[c]; m.NumQualityLevels = 0xdead;
        hr = ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &m, sizeof m);
        snprintf(key, sizeof key, "d12msaa %s x%u", fpt_formats[k_msaa_fmts[i]].name, k_msaa_counts[c]);
        FPT_GOLDEN(key, "%s %u 0x%x", d3dh_hr_name(hr), m.NumQualityLevels, m.Flags);
    }
}

TEST(fpt_d3d12_support2_and_info_table)
{
    ID3D12Device *dev = fpt_dev12(); UINT id;
    if (!dev) { t_skip("no D3D12 device"); return; }
    for (id = 0; id < FPT_NFORMATS; id++) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT s; HRESULT hr; char key[80];
        if (fpt_formats[id].group == G_NONE) continue;
        memset(&s, 0, sizeof s); s.Format = (DXGI_FORMAT)id;
        hr = ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s);
        snprintf(key, sizeof key, "d12s2 %s", fpt_formats[id].name);
        FPT_GOLDEN(key, "%s 0x%08x", d3dh_hr_name(hr), SUCCEEDED(hr) ? (UINT)s.Support2 : 0);
    }
    { D3D12_FEATURE_DATA_FORMAT_SUPPORT s; memset(&s, 0, sizeof s); s.Format = F(R8G8B8A8_UNORM);
      EXPECT_HR(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s - 1), E_INVALIDARG);
      EXPECT_HR(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof s + 1), E_INVALIDARG); }
}

/* GetResourceAllocationInfo for textures: size/alignment on the oracle adapter. */
TEST(fpt_d3d12_allocation_info_textures)
{
    ID3D12Device *dev = fpt_dev12(); UINT i;
    struct { const char *n; D3D12_RESOURCE_DESC d; } t[] = {
        { "tex2d 1x1 R8", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 1, 1, 1, 1, F(R8_UNORM), 1, 0) },
        { "tex2d 64x64 RGBA8", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 64, 64, 1, 1, F(R8G8B8A8_UNORM), 1, 0) },
        { "tex2d 256x256 RGBA8", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 256, 256, 1, 1, F(R8G8B8A8_UNORM), 1, 0) },
        { "tex2d 256x256 RGBA8 mips9", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 256, 256, 1, 9, F(R8G8B8A8_UNORM), 1, 0) },
        { "tex2d 100x100 RGBA8 array4", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 100, 100, 4, 1, F(R8G8B8A8_UNORM), 1, 0) },
        { "tex2d 512x512 BC7", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 512, 512, 1, 1, F(BC7_UNORM), 1, 0) },
        { "tex2d 64x64 RGBA8 rt", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 64, 64, 1, 1, F(R8G8B8A8_UNORM), 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) },
        { "tex2d 64x64 D32", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 64, 64, 1, 1, F(D32_FLOAT), 1, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) },
        { "tex2d 64x64 RGBA8 msaa4", mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 64, 64, 1, 1, F(R8G8B8A8_UNORM), 4, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) },
        { "tex1d 1024 R32", mk(D3D12_RESOURCE_DIMENSION_TEXTURE1D, 1024, 1, 1, 1, F(R32_FLOAT), 1, 0) },
        { "tex3d 32x32x32 RGBA8", mk(D3D12_RESOURCE_DIMENSION_TEXTURE3D, 32, 32, 32, 1, F(R8G8B8A8_UNORM), 1, 0) },
        { "buffer 1MB", mk(D3D12_RESOURCE_DIMENSION_BUFFER, 1048576, 1, 1, 1, F(UNKNOWN), 1, 0) },
    };
    if (!dev) { t_skip("no D3D12 device"); return; }
    t[11].d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        D3D12_RESOURCE_ALLOCATION_INFO info; char key[80]; memset(&info, 0xcd, sizeof info);
        dev->lpVtbl->GetResourceAllocationInfo(dev, &info, 0, 1, &t[i].d);
        snprintf(key, sizeof key, "d12alloc %s", t[i].n);
        FPT_GOLDEN(key, "%llu %llu", (unsigned long long)info.SizeInBytes, (unsigned long long)info.Alignment);
        CHECKF(info.Alignment >= 65536 && info.Alignment % 65536 == 0, "%s alignment %llu", t[i].n, (unsigned long long)info.Alignment);
        CHECKF(info.SizeInBytes % 65536 == 0 || info.SizeInBytes % info.Alignment == 0, "%s size %llu", t[i].n, (unsigned long long)info.SizeInBytes);
    }
}

/* FirstSubresource / NumSubresources / BaseOffset edge cases of GetCopyableFootprints. */
TEST(fpt_d3d12_footprint_subresource_ranges)
{
    ID3D12Device *dev = fpt_dev12(); D3D12_RESOURCE_DESC d = mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 64, 32, 3, 4, F(R8G8B8A8_UNORM), 1, 0);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT lay[16]; UINT nr[16]; UINT64 rs[16], tot;
    if (!dev) { t_skip("no D3D12 device"); return; }
    /* 12 subresources exist (4 mips x 3 slices): a middle range keeps the numbering and starts at BaseOffset */
    memset(lay, 0, sizeof lay);
    ID3D12Device_GetCopyableFootprints(dev, &d, 5, 3, 0, lay, nr, rs, &tot);
    CHECKF(lay[0].Offset == 0 && nr[0] == 16 && rs[0] == 128, "sub 5 (slice 1 mip 1): off %llu rows %u rowsize %llu", (unsigned long long)lay[0].Offset, nr[0], (unsigned long long)rs[0]);
    CHECKF(nr[1] == 8 && rs[1] == 64 && nr[2] == 4 && rs[2] == 32, "subs 6,7: rows %u/%u rowsize %llu/%llu", nr[1], nr[2], (unsigned long long)rs[1], (unsigned long long)rs[2]);
    CHECKF(lay[0].Footprint.Width == 32 && lay[0].Footprint.Height == 16 && lay[1].Footprint.Width == 16 && lay[2].Footprint.Width == 8, "sub 5..7 footprint sizes %u,%u,%u", lay[0].Footprint.Width, lay[1].Footprint.Width, lay[2].Footprint.Width);
    /* the whole range */
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 12, 1024, lay, nr, rs, &tot);
    CHECKF(lay[0].Offset == 1024, "offset of sub 0 with BaseOffset 1024: %llu", (unsigned long long)lay[0].Offset);
    CHECKF(lay[11].Offset > lay[0].Offset && lay[11].Offset % 512 == 0, "offset of sub 11: %llu", (unsigned long long)lay[11].Offset);
    /* past the end: Win10 answers with all-ones in the outputs it writes */
    { UINT64 t2 = 5; memset(lay, 0xcd, sizeof lay); memset(nr, 0xcd, sizeof nr); memset(rs, 0xcd, sizeof rs);
      ID3D12Device_GetCopyableFootprints(dev, &d, 10, 5, 0, lay, nr, rs, &t2);
      FPT_GOLDEN("d12fp range 10+5 of 12", "tot %llx nr0 %x rs0 %llx pitch0 %x", (unsigned long long)t2, nr[0], (unsigned long long)rs[0], lay[0].Footprint.RowPitch);
      t2 = 5; memset(lay, 0xcd, sizeof lay); memset(nr, 0xcd, sizeof nr); memset(rs, 0xcd, sizeof rs);
      ID3D12Device_GetCopyableFootprints(dev, &d, 12, 1, 0, lay, nr, rs, &t2);
      FPT_GOLDEN("d12fp range 12+1 of 12", "tot %llx nr0 %x rs0 %llx pitch0 %x", (unsigned long long)t2, nr[0], (unsigned long long)rs[0], lay[0].Footprint.RowPitch);
      t2 = 5; memset(lay, 0xcd, sizeof lay);
      ID3D12Device_GetCopyableFootprints(dev, &d, 0, 0, 0, lay, nr, rs, &t2);
      FPT_GOLDEN("d12fp range 0+0", "tot %llx", (unsigned long long)t2); }
    /* MSAA and buffer descriptions */
    { D3D12_RESOURCE_DESC ms = mk(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 64, 64, 1, 1, F(R8G8B8A8_UNORM), 4, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET); UINT64 t2 = 5; UINT n0 = 5; UINT64 r0 = 5;
      ID3D12Device_GetCopyableFootprints(dev, &ms, 0, 1, 0, lay, &n0, &r0, &t2);
      FPT_GOLDEN("d12fp msaa4 64x64", "tot %llx nr %x rs %llx", (unsigned long long)t2, n0, (unsigned long long)r0);
      { D3D12_RESOURCE_DESC b = mk(D3D12_RESOURCE_DIMENSION_BUFFER, 1000, 1, 1, 1, F(UNKNOWN), 1, 0); b.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; t2 = 5; n0 = 5; r0 = 5;
        ID3D12Device_GetCopyableFootprints(dev, &b, 0, 1, 0, lay, &n0, &r0, &t2);
        FPT_GOLDEN("d12fp buffer 1000", "tot %llu nr %x rs %llu fmt %u w %u h %u d %u pitch %u", (unsigned long long)t2, n0, (unsigned long long)r0, lay[0].Footprint.Format, lay[0].Footprint.Width, lay[0].Footprint.Height, lay[0].Footprint.Depth, lay[0].Footprint.RowPitch); } }
}
