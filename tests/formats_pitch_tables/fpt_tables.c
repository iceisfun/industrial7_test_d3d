/* Device-independent tables: header constants an implementation must agree with, subresource arithmetic,
 * and the format layout table used by the other files. */
#include "fpt.h"

TEST(fpt_tables_format_ids_match_enum)
{
    UINT id;
#define X(i, nm, g, b, w, h) CHECKF((UINT)DXGI_FORMAT_##nm == i, "enum value of " #nm " is %u, table says " #i, (UINT)DXGI_FORMAT_##nm);
    FPT_TABLE(X)
#undef X
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id];
        if (id >= 116 && id <= 129) CHECKF(f->group == G_NONE, "id %u should be unassigned", id);
        else CHECKF(f->id == id && f->name, "table entry %u missing", id);
    }
}
TEST(fpt_tables_block_rounding_arithmetic)
{
    const fpt_fmt *bc1 = &fpt_formats[DXGI_FORMAT_BC1_UNORM], *bc7 = &fpt_formats[DXGI_FORMAT_BC7_UNORM], *r8 = &fpt_formats[DXGI_FORMAT_R8_UNORM];
    static const UINT want_bc1[FPT_NW] = { 8, 8, 8, 8, 16, 16, 16, 32, 32, 40, 64, 128 };
    UINT i;
    for (i = 0; i < FPT_NW; i++) {
        CHECKF(fpt_row_bytes(bc1, fpt_widths[i]) == want_bc1[i], "BC1 width %u row bytes %u", fpt_widths[i], fpt_row_bytes(bc1, fpt_widths[i]));
        CHECKF(fpt_row_bytes(bc7, fpt_widths[i]) == want_bc1[i] * 2, "BC7 width %u", fpt_widths[i]);
        CHECK(fpt_row_bytes(r8, fpt_widths[i]) == fpt_widths[i]);
    }
    CHECK(fpt_rows(bc1, 1) == 1 && fpt_rows(bc1, 4) == 1 && fpt_rows(bc1, 5) == 2 && fpt_rows(bc1, 17) == 5);
    CHECK(fpt_row_bytes(&fpt_formats[DXGI_FORMAT_R1_UNORM], 1) == 1 && fpt_row_bytes(&fpt_formats[DXGI_FORMAT_R1_UNORM], 8) == 1 && fpt_row_bytes(&fpt_formats[DXGI_FORMAT_R1_UNORM], 9) == 2);
    CHECK(fpt_row_bytes(&fpt_formats[DXGI_FORMAT_R8G8_B8G8_UNORM], 1) == 4 && fpt_row_bytes(&fpt_formats[DXGI_FORMAT_R8G8_B8G8_UNORM], 3) == 8);
    CHECK(fpt_row_bytes(&fpt_formats[DXGI_FORMAT_YUY2], 5) == 12 && fpt_row_bytes(&fpt_formats[DXGI_FORMAT_Y210], 2) == 8);
}
TEST(fpt_tables_bytes_per_pixel_agree_with_helper)
{
    UINT id;
    for (id = 1; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id]; UINT hb;
        if (f->group < G_B8 || f->group > G_B128) continue;
        hb = d3dh_format_bytes((DXGI_FORMAT)id);
        if (hb) CHECKF(hb == f->bpb, "%s: d3dh_format_bytes %u vs table %u", f->name, hb, f->bpb);
    }
}
TEST(fpt_tables_d3d11_calc_subresource)
{
    /* D3D11CalcSubresource(mip, array, mipLevels) = mip + array * mipLevels */
    UINT a, m, n = 0;
    for (a = 0; a < 5; a++) for (m = 0; m < 6; m++) { CHECK(fpt_calc_sub(m, a, 6) == n); n++; }
    CHECK(fpt_calc_sub(0, 0, 1) == 0 && fpt_calc_sub(0, 7, 1) == 7 && fpt_calc_sub(14, 0, 15) == 14 && fpt_calc_sub(0, 1, 15) == 15);
}
TEST(fpt_tables_d3d11_req_constants)
{
    CHECK(D3D11_REQ_TEXTURE1D_U_DIMENSION == 16384 && D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION == 16384 && D3D11_REQ_TEXTURECUBE_DIMENSION == 16384);
    CHECK(D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION == 2048 && D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION == 2048 && D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION == 2048);
    CHECK(D3D11_REQ_MIP_LEVELS == 15 && D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT == 4096 && D3D11_REQ_MAXANISOTROPY == 16);
    CHECK(D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM == 128 && D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_C_TERM == 2048);
    CHECK(D3D11_REQ_BUFFER_RESOURCE_TEXEL_COUNT_2_TO_EXP == 27 && D3D11_REQ_FILTERING_HW_ADDRESSABLE_RESOURCE_DIMENSION == 16384);
    CHECK(D3D11_REQ_SAMPLER_OBJECT_COUNT_PER_DEVICE == 4096 && D3D11_REQ_BLEND_OBJECT_COUNT_PER_DEVICE == 4096 && D3D11_REQ_RASTERIZER_OBJECT_COUNT_PER_DEVICE == 4096);
    CHECK(D3D11_REQ_DEPTH_STENCIL_OBJECT_COUNT_PER_DEVICE == 4096 && D3D11_REQ_RESOURCE_VIEW_COUNT_PER_DEVICE_2_TO_EXP == 20);
}
TEST(fpt_tables_d3d11_slot_constants)
{
    CHECK(D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT == 16 && D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT == 128 && D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT == 14);
    CHECK(D3D11_1_UAV_SLOT_COUNT == 64 && D3D11_PS_CS_UAV_REGISTER_COUNT == 8 && D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT == 32 && D3D11_SO_BUFFER_SLOT_COUNT == 4);
    CHECK(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT == 8 && D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE == 16 && D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT == 32);
    CHECK(D3D11_STANDARD_VERTEX_ELEMENT_COUNT == 32 && D3D11_FLOAT32_MAX > 3.0e38f);
}
TEST(fpt_tables_d3d12_d3d11_limit_parity)
{
    CHECK(D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION == D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION && D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION == D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION);
    CHECK(D3D12_REQ_MIP_LEVELS == D3D11_REQ_MIP_LEVELS && D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION == D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION);
    CHECK(D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT == D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT && D3D12_UAV_SLOT_COUNT == D3D11_1_UAV_SLOT_COUNT);
}
TEST(fpt_tables_d3d10_req_constants)
{
    CHECK(D3D10_REQ_TEXTURE1D_U_DIMENSION == 8192 && D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION == 8192 && D3D10_REQ_TEXTURECUBE_DIMENSION == 8192);
    CHECK(D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION == 2048 && D3D10_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION == 512 && D3D10_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION == 512);
    CHECK(D3D10_REQ_MIP_LEVELS == 14 && D3D10_REQ_CONSTANT_BUFFER_ELEMENT_COUNT == 4096 && D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT == 16);
    CHECK(D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT == 128 && D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT == 8 && D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT == 32);
}
TEST(fpt_tables_dxgi_format_enum_values)
{
    CHECK(DXGI_FORMAT_R8G8B8A8_UNORM == 28 && DXGI_FORMAT_BC1_UNORM == 71 && DXGI_FORMAT_BC7_UNORM_SRGB == 99 && DXGI_FORMAT_AYUV == 100);
    CHECK(DXGI_FORMAT_NV12 == 103 && DXGI_FORMAT_P010 == 104 && DXGI_FORMAT_YUY2 == 107 && DXGI_FORMAT_B4G4R4A4_UNORM == 115 && DXGI_FORMAT_P208 == 130 && DXGI_FORMAT_V408 == 132);
    CHECK(DXGI_FORMAT_D32_FLOAT == 40 && DXGI_FORMAT_D24_UNORM_S8_UINT == 45 && DXGI_FORMAT_D16_UNORM == 55 && DXGI_FORMAT_D32_FLOAT_S8X24_UINT == 20);
}

/* Per-feature-level format support, for the requirement-level formats. */
TEST(fpt_d3d11_support_by_feature_level_table)
{
    static const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1 };
    static const UINT keyfmts[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC6H_UF16, DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT,
        DXGI_FORMAT_NV12, DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_R11G11B10_FLOAT };
    UINT i, k;
    if (!d3dh_D3D11CreateDevice()) { t_skip("no d3d11"); return; }
    for (i = 0; i < sizeof fls / sizeof *fls; i++) {
        ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL got = 0; HRESULT hr;
        hr = d3dh_make_device11_fl(&dev, &ctx, &fls[i], 1, &got, NULL, 0);
        if (FAILED(hr)) { t_note("feature level 0x%x: device creation %s", fls[i], d3dh_hr_name(hr)); continue; }
        CHECKF(got == fls[i], "requested FL 0x%x, device FL 0x%x", fls[i], got);
        CHECK(ID3D11Device_GetFeatureLevel(dev) == got);
        for (k = 0; k < sizeof keyfmts / sizeof *keyfmts; k++) {
            HRESULT h2; UINT s = fpt_support11(dev, keyfmts[k], &h2);
            t_note("FL 0x%x %-24s hr=%s bits=0x%08x", got, fpt_formats[keyfmts[k]].name, d3dh_hr_name(h2), s);
            if (keyfmts[k] == DXGI_FORMAT_R8G8B8A8_UNORM) CHECKF((s & (D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_BLENDABLE)) == (D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_BLENDABLE), "FL 0x%x RGBA8 bits 0x%x", got, s);
            if (keyfmts[k] == DXGI_FORMAT_BC1_UNORM) CHECKF((s & D3D11_FORMAT_SUPPORT_TEXTURE2D) && (s & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE), "FL 0x%x BC1 bits 0x%x", got, s);
            if (keyfmts[k] == DXGI_FORMAT_D32_FLOAT) CHECKF(s & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, "FL 0x%x D32 bits 0x%x", got, s);
        }
        ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
    }
}

TEST(fpt_tables_block_bytes_per_group)
{
    UINT id;
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id];
        if (!fpt_is_bc(f)) continue;
        CHECKF(f->bw == 4 && f->bh == 4, "%s block %ux%u", f->name, f->bw, f->bh);
        if (f->group == G_BC1 || f->group == G_BC4) CHECKF(f->bpb == 8, "%s block bytes %u", f->name, f->bpb);
        else CHECKF(f->bpb == 16, "%s block bytes %u", f->name, f->bpb);
    }
}
TEST(fpt_tables_bc_row_bytes_all_groups)
{
    UINT id, wi;
    for (id = 0; id < FPT_NFORMATS; id++) {
        const fpt_fmt *f = &fpt_formats[id];
        if (!fpt_is_bc(f)) continue;
        for (wi = 0; wi < FPT_NW; wi++) {
            UINT w = fpt_widths[wi], blocks = (w + 3) / 4;
            CHECKF(fpt_row_bytes(f, w) == blocks * f->bpb, "%s w=%u", f->name, w);
            CHECKF(fpt_row_bytes(f, w) % f->bpb == 0, "%s w=%u not a multiple of block size", f->name, w);
        }
    }
}
TEST(fpt_tables_row_pitch_alignment_arithmetic)
{
    static const struct { UINT in, want; } t[] = { {0,0}, {1,256}, {255,256}, {256,256}, {257,512}, {511,512}, {512,512}, {1000,1024}, {4096,4096} };
    UINT i;
    for (i = 0; i < sizeof t / sizeof *t; i++) CHECKF(fpt_align(t[i].in, 256) == t[i].want, "align256(%u)=%u", t[i].in, fpt_align(t[i].in, 256));
    CHECK(fpt_align(1, 512) == 512 && fpt_align(513, 512) == 1024);
}
TEST(fpt_tables_mip_dimension_arithmetic)
{
    UINT v, m;
    CHECK(fpt_mip(1, 0) == 1 && fpt_mip(1, 5) == 1 && fpt_mip(16384, 14) == 1 && fpt_mip(5, 1) == 2 && fpt_mip(7, 2) == 1 && fpt_mip(100, 3) == 12);
    for (v = 1; v <= 64; v++) for (m = 0; m < 8; m++) CHECK(fpt_mip(v, m) >= 1 && fpt_mip(v, m) <= v);
}
TEST(fpt_tables_planar_and_packed_entries)
{
    CHECK(fpt_formats[DXGI_FORMAT_NV12].group == G_PLANAR && fpt_formats[DXGI_FORMAT_P010].bpb == 2 && fpt_formats[DXGI_FORMAT_NV12].bpb == 1);
    CHECK(fpt_formats[DXGI_FORMAT_YUY2].bw == 2 && fpt_formats[DXGI_FORMAT_YUY2].bpb == 4 && fpt_formats[DXGI_FORMAT_Y210].bpb == 8);
    CHECK(fpt_formats[DXGI_FORMAT_AYUV].bpb == 4 && fpt_formats[DXGI_FORMAT_R1_UNORM].bw == 8);
}
TEST(fpt_tables_d3d12_subresource_count_limits)
{
    CHECK(D3D12_REQ_SUBRESOURCES == 30720);
    CHECK(D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION * D3D12_REQ_MIP_LEVELS == D3D12_REQ_SUBRESOURCES);
    CHECK(D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION * D3D11_REQ_MIP_LEVELS == 30720);
}
