/* Initial data: SysMemPitch / SysMemSlicePitch handling, mip chains, arrays, block-compressed rows, dynamic discard. */
#include "txh.h"

static void tx_verify_subs(tx_ctx *c, ID3D11Resource *res, UINT nsub, UINT seed0, const char *file, int line) {
    for (UINT s = 0; s < nsub; s++) {
        tx_img im; size_t bad = 0; HRESULT hr = tx_fetch(c, res, s, &im);
        t_check(SUCCEEDED(hr), file, line, "tx_fetch", "sub %u: %s", s, d3dh_hr_name(hr));
        if (FAILED(hr)) continue;
        t_check(tx_img_is_pat(&im, seed0 + s, &bad), file, line, "initial data content", "sub %u (%ux%ux%u rowbytes %u) first mismatch at byte %u", s, im.w, im.h, im.d, im.rowbytes, (unsigned)bad);
        tx_img_free(&im);
    }
}

static void tx_init_case2d(tx_ctx *c, UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT f, D3D11_USAGE u, UINT bind, UINT cpu, UINT misc, UINT rowpad, const char *file, int line) {
    if (tx_is_bc67(f) && c->fl < FL_11_0) { t_skip("BC6H/BC7 need feature level 11_0"); return; }
    tx_init in; ID3D11Texture2D *t = NULL; HRESULT hr; UINT n = (mips ? mips : tx_full_mips(w > h ? w : h)) * arr;
    if (!tx_init_make(&in, w, h, 1, mips, arr, f, rowpad, 0, 40)) { t_check(0, file, line, "tx_init_make", "oom"); tx_init_free(&in); return; }
    hr = tx_mk2d(c, w, h, mips, arr, f, u, bind, cpu, misc, in.sd, &t);
    t_check(hr == S_OK, file, line, "CreateTexture2D(initial data)", "%s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { tx_verify_subs(c, (ID3D11Resource *)t, n, 40, file, line); ID3D11Texture2D_Release(t); }
    tx_init_free(&in);
}
#define INIT2D(nm, W, H, M, A, F, U, B, CP, MI, PAD) DEVTEST(d3d11_textures_init2d_##nm) { tx_init_case2d(c, W, H, M, A, F, U, B, CP, MI, PAD, __FILE__, __LINE__); }
INIT2D(rgba8_tight_def, 5, 3, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, 0)
INIT2D(rgba8_tight_imm, 5, 3, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT2D(rgba8_tight_stg, 5, 3, 1, 1, F_RGBA8, U_STG, 0, CPU_R, 0, 0)
INIT2D(rgba8_tight_stg_rw, 5, 3, 1, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, 0)
INIT2D(rgba8_tight_dyn, 5, 3, 1, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, 0)
INIT2D(rgba8_pad4_def, 5, 3, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, 4)
INIT2D(rgba8_pad16_imm, 5, 3, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 16)
INIT2D(rgba8_pad60_def, 7, 9, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, 60)
INIT2D(rgba8_pad_stg, 7, 9, 1, 1, F_RGBA8, U_STG, 0, CPU_R, 0, 100)
INIT2D(rgba8_pad_dyn, 7, 9, 1, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, 12)
INIT2D(rgba8_1x1, 1, 1, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT2D(rgba8_1x1_padded, 1, 1, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, 64)
INIT2D(rgba8_wide, 1000, 2, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, 24)
INIT2D(rgba8_tall, 2, 513, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 8)
INIT2D(rgba8_chain, 13, 7, 0, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT2D(rgba8_chain_padded, 13, 7, 0, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 8)
INIT2D(rgba8_chain_def, 13, 7, 0, 1, F_RGBA8, U_DEF, B_SRV | B_RT, 0, 0, 4)
INIT2D(rgba8_chain_stg, 13, 7, 0, 1, F_RGBA8, U_STG, 0, CPU_R, 0, 4)
INIT2D(rgba8_two_mips, 16, 16, 2, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT2D(rgba8_array3, 8, 8, 1, 3, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT2D(rgba8_array3_chain, 8, 8, 0, 3, F_RGBA8, U_IMM, B_SRV, 0, 0, 12)
INIT2D(rgba8_array3_chain_def, 8, 8, 0, 3, F_RGBA8, U_DEF, B_SRV, 0, 0, 12)
INIT2D(rgba8_cube, 8, 8, 1, 6, F_RGBA8, U_IMM, B_SRV, 0, M_CUBE, 0)
INIT2D(rgba8_cube_chain, 8, 8, 0, 6, F_RGBA8, U_IMM, B_SRV, 0, M_CUBE, 0)
INIT2D(rgba8_cube_array, 4, 4, 0, 12, F_RGBA8, U_IMM, B_SRV, 0, M_CUBE, 4)
INIT2D(rgba8_typeless, 6, 6, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, U_DEF, B_SRV | B_RT, 0, 0, 0)
INIT2D(r8_w1, 1, 4, 1, 1, F_R8, U_IMM, B_SRV, 0, 0, 3)
INIT2D(r8_w2, 2, 4, 1, 1, F_R8, U_IMM, B_SRV, 0, 0, 2)
INIT2D(r8_w3, 3, 4, 1, 1, F_R8, U_IMM, B_SRV, 0, 0, 1)
INIT2D(r8_w5, 5, 4, 1, 1, F_R8, U_IMM, B_SRV, 0, 0, 0)
INIT2D(r8_w7_def, 7, 5, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, 0)
INIT2D(r8_w9_chain, 9, 9, 0, 1, F_R8, U_IMM, B_SRV, 0, 0, 5)
INIT2D(r8g8_w5, 5, 3, 1, 1, DXGI_FORMAT_R8G8_UNORM, U_IMM, B_SRV, 0, 0, 2)
INIT2D(r16_w5, 5, 3, 1, 1, DXGI_FORMAT_R16_UNORM, U_IMM, B_SRV, 0, 0, 6)
INIT2D(r16g16_w3_chain, 3, 3, 0, 1, DXGI_FORMAT_R16G16_FLOAT, U_IMM, B_SRV, 0, 0, 0)
INIT2D(r16g16b16a16_w3, 3, 3, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, U_IMM, B_SRV, 0, 0, 8)
INIT2D(r32_w3, 3, 3, 1, 1, DXGI_FORMAT_R32_FLOAT, U_IMM, B_SRV, 0, 0, 0)
INIT2D(r32g32b32_w3, 3, 3, 1, 1, DXGI_FORMAT_R32G32B32_FLOAT, U_IMM, B_SRV, 0, 0, 4)
INIT2D(r32g32b32a32_w3_chain, 3, 3, 0, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, U_IMM, B_SRV, 0, 0, 16)
INIT2D(r10g10b10a2, 5, 5, 1, 1, DXGI_FORMAT_R10G10B10A2_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bgra8_chain, 9, 5, 0, 1, F_BGRA8, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc1_4x4, 4, 4, 1, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc1_8x8, 8, 8, 1, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc1_8x8_def, 8, 8, 1, 1, DXGI_FORMAT_BC1_UNORM, U_DEF, B_SRV, 0, 0, 0)
INIT2D(bc1_8x8_stg, 8, 8, 1, 1, DXGI_FORMAT_BC1_UNORM, U_STG, 0, CPU_R, 0, 0)
INIT2D(bc1_8x8_padded, 8, 8, 1, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 24)
INIT2D(bc1_16x16_chain, 16, 16, 0, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc1_16x16_chain_padded, 16, 16, 0, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 8)
INIT2D(bc1_16x16_chain_def, 16, 16, 0, 1, DXGI_FORMAT_BC1_UNORM, U_DEF, B_SRV, 0, 0, 8)
INIT2D(bc1_12x12, 12, 12, 1, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc1_64x16, 64, 16, 1, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc1_array3_chain, 8, 8, 0, 3, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc1_srgb, 8, 8, 0, 1, DXGI_FORMAT_BC1_UNORM_SRGB, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc2_8x8_chain, 8, 8, 0, 1, DXGI_FORMAT_BC2_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc3_12x12_chain, 12, 12, 0, 1, DXGI_FORMAT_BC3_UNORM, U_IMM, B_SRV, 0, 0, 16)
INIT2D(bc3_32x8_chain_def, 32, 8, 0, 1, DXGI_FORMAT_BC3_UNORM, U_DEF, B_SRV, 0, 0, 0)
INIT2D(bc4_8x8_chain, 8, 8, 0, 1, DXGI_FORMAT_BC4_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc5_8x8_chain, 8, 8, 0, 1, DXGI_FORMAT_BC5_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc6h_8x8_chain, 8, 8, 0, 1, DXGI_FORMAT_BC6H_UF16, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc7_8x8_chain, 8, 8, 0, 1, DXGI_FORMAT_BC7_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT2D(bc7_16x16_chain_stg, 16, 16, 0, 1, DXGI_FORMAT_BC7_UNORM, U_STG, 0, CPU_R, 0, 0)
INIT2D(bc7_cube, 8, 8, 0, 6, DXGI_FORMAT_BC7_UNORM, U_IMM, B_SRV, 0, M_CUBE, 0)

static void tx_init_case1d(tx_ctx *c, UINT w, UINT mips, UINT arr, DXGI_FORMAT f, D3D11_USAGE u, UINT bind, UINT cpu, UINT pitch_garbage, const char *file, int line) {
    tx_init in; ID3D11Texture1D *t = NULL; HRESULT hr; UINT n = (mips ? mips : tx_full_mips(w)) * arr, i;
    if (!tx_init_make(&in, w, 1, 1, mips, arr, f, 0, 0, 60)) { t_check(0, file, line, "tx_init_make", "oom"); tx_init_free(&in); return; }
    for (i = 0; i < n; i++) in.sd[i].SysMemPitch = pitch_garbage;   /* meaningless for 1D */
    hr = tx_mk1d(c, w, mips, arr, f, u, bind, cpu, 0, in.sd, &t);
    t_check(hr == S_OK, file, line, "CreateTexture1D(initial data)", "%s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { tx_verify_subs(c, (ID3D11Resource *)t, n, 60, file, line); ID3D11Texture1D_Release(t); }
    tx_init_free(&in);
}
#define INIT1D(nm, W, M, A, F, U, B, CP, G) DEVTEST(d3d11_textures_init1d_##nm) { tx_init_case1d(c, W, M, A, F, U, B, CP, G, __FILE__, __LINE__); }
INIT1D(rgba8_imm, 7, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 28)
INIT1D(rgba8_imm_pitch0, 7, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0)
INIT1D(rgba8_imm_pitch_garbage, 7, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0x7000)
INIT1D(rgba8_def, 7, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0)
INIT1D(rgba8_chain, 100, 0, 1, F_RGBA8, U_IMM, B_SRV, 0, 0)
INIT1D(rgba8_array, 9, 1, 4, F_RGBA8, U_IMM, B_SRV, 0, 0)
INIT1D(rgba8_array_chain, 9, 0, 3, F_RGBA8, U_IMM, B_SRV, 0, 0)
INIT1D(rgba8_stg, 9, 1, 1, F_RGBA8, U_STG, 0, CPU_R, 0)
INIT1D(r8_chain, 33, 0, 2, F_R8, U_IMM, B_SRV, 0, 0)
INIT1D(r32g32b32a32, 5, 3, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, U_IMM, B_SRV, 0, 0)

static void tx_init_case3d(tx_ctx *c, UINT w, UINT h, UINT d, UINT mips, DXGI_FORMAT f, D3D11_USAGE u, UINT bind, UINT cpu, UINT rowpad, UINT slicepad, const char *file, int line) {
    if (tx_is_bc67(f) && c->fl < FL_11_0) { t_skip("BC6H/BC7 need feature level 11_0"); return; }
    tx_init in; ID3D11Texture3D *t = NULL; HRESULT hr; UINT m2 = w > h ? w : h, n;
    if (d > m2) m2 = d;
    n = mips ? mips : tx_full_mips(m2);
    if (!tx_init_make(&in, w, h, d, mips, 1, f, rowpad, slicepad, 70)) { t_check(0, file, line, "tx_init_make", "oom"); tx_init_free(&in); return; }
    hr = tx_mk3d(c, w, h, d, mips, f, u, bind, cpu, 0, in.sd, &t);
    t_check(hr == S_OK, file, line, "CreateTexture3D(initial data)", "%s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { tx_verify_subs(c, (ID3D11Resource *)t, n, 70, file, line); ID3D11Texture3D_Release(t); }
    tx_init_free(&in);
}
#define INIT3D(nm, W, H, D, M, F, U, B, CP, RP, SP) DEVTEST(d3d11_textures_init3d_##nm) { tx_init_case3d(c, W, H, D, M, F, U, B, CP, RP, SP, __FILE__, __LINE__); }
INIT3D(rgba8_tight, 4, 4, 4, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT3D(rgba8_tight_def, 4, 4, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, 0)
INIT3D(rgba8_tight_stg, 4, 4, 4, 1, F_RGBA8, U_STG, 0, CPU_R, 0, 0)
INIT3D(rgba8_rowpad, 5, 3, 4, 1, F_RGBA8, U_IMM, B_SRV, 0, 12, 0)
INIT3D(rgba8_slicepad, 5, 3, 4, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 40)
INIT3D(rgba8_both_pad, 5, 3, 4, 1, F_RGBA8, U_IMM, B_SRV, 0, 4, 20)
INIT3D(rgba8_both_pad_def, 5, 3, 4, 1, F_RGBA8, U_DEF, B_SRV | B_RT, 0, 4, 20)
INIT3D(rgba8_chain, 5, 3, 4, 0, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT3D(rgba8_chain_padded, 5, 3, 4, 0, F_RGBA8, U_IMM, B_SRV, 0, 8, 16)
INIT3D(rgba8_chain_16x8x4, 16, 8, 4, 0, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT3D(rgba8_flat, 8, 8, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT3D(rgba8_1x1x8, 1, 1, 8, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, 0)
INIT3D(r8_chain, 7, 5, 3, 0, F_R8, U_IMM, B_SRV, 0, 1, 3)
INIT3D(r32f_2x2x2, 2, 2, 2, 1, DXGI_FORMAT_R32_FLOAT, U_IMM, B_SRV, 0, 0, 0)
INIT3D(r16g16b16a16_3x3x3, 3, 3, 3, 0, DXGI_FORMAT_R16G16B16A16_FLOAT, U_IMM, B_SRV, 0, 0, 0)
INIT3D(bc1_8x8x2, 8, 8, 2, 1, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT3D(bc1_8x8x4_chain, 8, 8, 4, 0, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT3D(bc1_8x8x4_chain_padded, 8, 8, 4, 0, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 8, 8)
INIT3D(bc3_8x4x2, 8, 4, 2, 1, DXGI_FORMAT_BC3_UNORM, U_IMM, B_SRV, 0, 0, 0)
INIT3D(bc7_8x8x2, 8, 8, 2, 1, DXGI_FORMAT_BC7_UNORM, U_IMM, B_SRV, 0, 0, 0)

/* SysMemSlicePitch is ignored for 2D textures. */
DEVTEST(d3d11_textures_init2d_slicepitch_ignored) {
    tx_init in; ID3D11Texture2D *t = NULL; UINT i;
    CHECK(tx_init_make(&in, 6, 6, 1, 0, 2, F_RGBA8, 0, 0, 90));
    for (i = 0; i < in.n; i++) in.sd[i].SysMemSlicePitch = 0x12345678;
    EXPECT_HR(tx_mk2d(c, 6, 6, 0, 2, F_RGBA8, U_IMM, B_SRV, 0, 0, in.sd, &t), S_OK);
    if (t) { tx_verify_subs(c, (ID3D11Resource *)t, in.n, 90, __FILE__, __LINE__); ID3D11Texture2D_Release(t); }
    tx_init_free(&in);
}
DEVTEST(d3d11_textures_init2d_slicepitch_zero_vs_set) {
    tx_init in; ID3D11Texture2D *t = NULL;
    CHECK(tx_init_make(&in, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 91));
    in.sd[0].SysMemSlicePitch = 4 * 4 * 4;
    EXPECT_HR(tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, in.sd, &t), S_OK);
    if (t) { tx_verify_subs(c, (ID3D11Resource *)t, 1, 91, __FILE__, __LINE__); ID3D11Texture2D_Release(t); }
    tx_init_free(&in);
}
/* The initial-data array is indexed like subresources: mip + array * mips. */
DEVTEST(d3d11_textures_init2d_array_ordering) {
    tx_init in; ID3D11Texture2D *t = NULL; UINT s;
    CHECK(tx_init_make(&in, 4, 4, 1, 3, 2, DXGI_FORMAT_R8_UNORM, 0, 0, 120));
    EXPECT_HR(tx_mk2d(c, 4, 4, 3, 2, DXGI_FORMAT_R8_UNORM, U_IMM, B_SRV, 0, 0, in.sd, &t), S_OK);
    if (t) {
        for (s = 0; s < 6; s++) {
            tx_img im; size_t bad;
            if (FAILED(tx_fetch(c, (ID3D11Resource *)t, s, &im))) { CHECK(0); continue; }
            CHECKF(tx_img_is_pat(&im, 120 + s, &bad), "sub %u (mip %u array %u) mismatch at %u", s, s % 3, s / 3, (unsigned)bad);
            EXPECT_EQ(im.w, tx_dim(4, s % 3));
            tx_img_free(&im);
        }
        ID3D11Texture2D_Release(t);
    }
    tx_init_free(&in);
}
DEVTEST(d3d11_textures_init2d_data_copied_at_creation) {
    tx_init in; ID3D11Texture2D *t = NULL; tx_img im; size_t bad;
    CHECK(tx_init_make(&in, 8, 8, 1, 1, 1, F_RGBA8, 0, 0, 130));
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, in.sd, &t), S_OK);
    memset(in.blocks[0], 0, 8 * 8 * 4);        /* caller memory may be reused immediately */
    if (t) {
        EXPECT_HR(tx_fetch(c, (ID3D11Resource *)t, 0, &im), S_OK);
        CHECK(tx_img_is_pat(&im, 130, &bad));
        tx_img_free(&im);
        ID3D11Texture2D_Release(t);
    }
    tx_init_free(&in);
}
DEVTEST(d3d11_textures_init2d_pitch_zero_note) {
    tx_init in; ID3D11Texture2D *t = NULL; HRESULT hr;
    CHECK(tx_init_make(&in, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 92));
    in.sd[0].SysMemPitch = 0;
    hr = tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, in.sd, &t);
    EXPECT_HR(hr, E_INVALIDARG); CHECK(t == NULL);
    SAFE_RELEASE(t);
    tx_init_free(&in);
}
DEVTEST(d3d11_textures_init2d_pitch_too_small_note) {
    tx_init in; ID3D11Texture2D *t = NULL; HRESULT hr;
    CHECK(tx_init_make(&in, 4, 4, 1, 1, 1, F_RGBA8, 0, 0, 93));
    in.sd[0].SysMemPitch = 4;      /* smaller than one row of 4 RGBA8 texels */
    hr = tx_mk2d(c, 4, 4, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, in.sd, &t);
    EXPECT_HR(hr, S_OK);   /* Win10 22H2 does not validate a SysMemPitch below the row size (it reads past the row: do not rely on it) */
    SAFE_RELEASE(t);
    tx_init_free(&in);
}

/* ---- DYNAMIC textures: discard + rewrite -------------------------------------------------------------------------- */
static void tx_dynamic_case(tx_ctx *c, UINT w, UINT h, DXGI_FORMAT f, const char *file, int line) {
    if (tx_is_bc67(f) && c->fl < FL_11_0) { t_skip("BC6H/BC7 need feature level 11_0"); return; }
    ID3D11Texture2D *t = NULL; HRESULT hr; tx_img im; size_t bad = 0;
    hr = tx_mk2d(c, w, h, 1, 1, f, U_DYN, B_SRV, CPU_W, 0, NULL, &t);
    t_check(hr == S_OK, file, line, "create dynamic", "%s", d3dh_hr_name(hr)); if (FAILED(hr)) return;
    hr = tx_write_pat(c, (ID3D11Resource *)t, 0, w, h, 1, 1, f, 11, D3D11_MAP_WRITE_DISCARD);
    t_check(hr == S_OK, file, line, "Map(WRITE_DISCARD)", "%s", d3dh_hr_name(hr));
    hr = tx_fetch(c, (ID3D11Resource *)t, 0, &im);
    t_check(hr == S_OK, file, line, "fetch", "%s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { t_check(tx_img_is_pat(&im, 11, &bad), file, line, "first write", "mismatch at %u", (unsigned)bad); tx_img_free(&im); }
    hr = tx_write_pat(c, (ID3D11Resource *)t, 0, w, h, 1, 1, f, 12, D3D11_MAP_WRITE_DISCARD);
    t_check(hr == S_OK, file, line, "Map(WRITE_DISCARD) 2", "%s", d3dh_hr_name(hr));
    hr = tx_fetch(c, (ID3D11Resource *)t, 0, &im);
    if (SUCCEEDED(hr)) { t_check(tx_img_is_pat(&im, 12, &bad), file, line, "second write", "mismatch at %u", (unsigned)bad); tx_img_free(&im); }
    ID3D11Texture2D_Release(t);
}
#define DYN(nm, W, H, F) DEVTEST(d3d11_textures_dynamic_##nm) { tx_dynamic_case(c, W, H, F, __FILE__, __LINE__); }
DYN(rgba8_17x9, 17, 9, F_RGBA8)
DYN(rgba8_1x1, 1, 1, F_RGBA8)
DYN(r8_33x5, 33, 5, F_R8)
DYN(r32g32b32a32_3x3, 3, 3, DXGI_FORMAT_R32G32B32A32_FLOAT)
DYN(bc1_16x8, 16, 8, DXGI_FORMAT_BC1_UNORM)
DYN(bc7_8x8, 8, 8, DXGI_FORMAT_BC7_UNORM)
DYN(r16g16_64x64, 64, 64, DXGI_FORMAT_R16G16_UNORM)

DEVTEST(d3d11_textures_dynamic_1d_3d) {
    ID3D11Texture1D *t1 = NULL; ID3D11Texture3D *t3 = NULL; tx_img im; size_t bad;
    EXPECT_HR(tx_mk1d(c, 19, 1, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, NULL, &t1), S_OK);
    EXPECT_HR(tx_mk3d(c, 5, 3, 4, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, NULL, &t3), S_OK);
    if (t1) {
        EXPECT_HR(tx_write_pat(c, (ID3D11Resource *)t1, 0, 19, 1, 1, 1, F_RGBA8, 21, D3D11_MAP_WRITE_DISCARD), S_OK);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t1, 0, &im))) { CHECK(tx_img_is_pat(&im, 21, &bad)); tx_img_free(&im); }
        ID3D11Texture1D_Release(t1);
    }
    if (t3) {
        EXPECT_HR(tx_write_pat(c, (ID3D11Resource *)t3, 0, 5, 3, 4, 1, F_RGBA8, 22, D3D11_MAP_WRITE_DISCARD), S_OK);
        if (SUCCEEDED(tx_fetch(c, (ID3D11Resource *)t3, 0, &im))) { CHECK(tx_img_is_pat(&im, 22, &bad)); tx_img_free(&im); }
        ID3D11Texture3D_Release(t3);
    }
}
