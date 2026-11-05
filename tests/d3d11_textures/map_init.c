/* Map pitches, subresource indexing, CPU access matrix, and initial data (SysMemPitch / SysMemSlicePitch). */
#include "txh.h"

/* ---- helpers -------------------------------------------------------------------------------------------------- */
static void tx_verify_all(tx_ctx *c, ID3D11Resource *res, UINT nsub, UINT seed0, const char *file, int line) {
    for (UINT s = 0; s < nsub; s++) {
        tx_img im; size_t bad = 0; HRESULT hr = tx_fetch(c, res, s, &im);
        t_check(SUCCEEDED(hr), file, line, "tx_fetch", "sub %u: %s", s, d3dh_hr_name(hr));
        if (FAILED(hr)) continue;
        t_check(tx_img_is_pat(&im, seed0 + s, &bad), file, line, "subresource content", "sub %u (%ux%ux%u) first mismatch at byte %u", s, im.w, im.h, im.d, (unsigned)bad);
        tx_img_free(&im);
    }
}

/* ---- Map pitches per format -------------------------------------------------------------------------------- */
static void tx_pitch_case(tx_ctx *c, UINT w, UINT h, DXGI_FORMAT f, const char *file, int line) {
    ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE m; HRESULT hr;
    UINT rb = tx_row_bytes(f, w), rows = tx_rows(f, h);
    hr = tx_mk2d(c, w, h, 1, 1, f, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t);
    if (FAILED(hr)) { t_note("%s:%d staging %ux%u format %u not creatable: %s", file, line, w, h, f, d3dh_hr_name(hr)); return; }
    memset(&m, 0, sizeof m);
    hr = ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ_WRITE, 0, &m);
    t_check(hr == S_OK, file, line, "Map", "got %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) {
        t_check(m.pData != NULL, file, line, "pData", "NULL");
        t_check(m.RowPitch >= rb, file, line, "RowPitch >= row bytes", "RowPitch %u, row bytes %u (%ux%u fmt %u)", m.RowPitch, rb, w, h, f);
        t_check(m.DepthPitch >= (UINT64)m.RowPitch * rows, file, line, "DepthPitch >= RowPitch*rows", "DepthPitch %u RowPitch %u rows %u", m.DepthPitch, m.RowPitch, rows);
        if (m.RowPitch != rb || m.DepthPitch != m.RowPitch * rows) t_note("%ux%u fmt %u: RowPitch %u (tight %u) DepthPitch %u (tight %u)", w, h, f, m.RowPitch, rb, m.DepthPitch, m.RowPitch * rows);
        ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    }
    ID3D11Texture2D_Release(t);
}
#define PITCH2D(nm, W, H, F) DEVTEST(d3d11_textures_pitch_##nm) { tx_pitch_case(c, W, H, F, __FILE__, __LINE__); }
PITCH2D(rgba8_1x1, 1, 1, F_RGBA8)
PITCH2D(rgba8_3x5, 3, 5, F_RGBA8)
PITCH2D(rgba8_64x64, 64, 64, F_RGBA8)
PITCH2D(rgba8_511x3, 511, 3, F_RGBA8)
PITCH2D(rgba8_512x512, 512, 512, F_RGBA8)
PITCH2D(bgra8_17x9, 17, 9, F_BGRA8)
PITCH2D(r8_1x1, 1, 1, F_R8)
PITCH2D(r8_7x2, 7, 2, F_R8)
PITCH2D(r8_255x4, 255, 4, F_R8)
PITCH2D(r8g8_9x3, 9, 3, DXGI_FORMAT_R8G8_UNORM)
PITCH2D(r16_13x3, 13, 3, DXGI_FORMAT_R16_UNORM)
PITCH2D(r16g16_5x5, 5, 5, DXGI_FORMAT_R16G16_FLOAT)
PITCH2D(r16g16b16a16_5x3, 5, 3, DXGI_FORMAT_R16G16B16A16_FLOAT)
PITCH2D(r32_7x7, 7, 7, DXGI_FORMAT_R32_FLOAT)
PITCH2D(r32g32_5x5, 5, 5, DXGI_FORMAT_R32G32_FLOAT)
PITCH2D(r32g32b32_5x2, 5, 2, DXGI_FORMAT_R32G32B32_FLOAT)
PITCH2D(r32g32b32a32_5x5, 5, 5, DXGI_FORMAT_R32G32B32A32_FLOAT)
PITCH2D(r10g10b10a2_9x9, 9, 9, DXGI_FORMAT_R10G10B10A2_UNORM)
PITCH2D(r11g11b10_9x2, 9, 2, DXGI_FORMAT_R11G11B10_FLOAT)
PITCH2D(a8_33x3, 33, 3, DXGI_FORMAT_A8_UNORM)
PITCH2D(b5g6r5_7x3, 7, 3, DXGI_FORMAT_B5G6R5_UNORM)
PITCH2D(bc1_4x4, 4, 4, DXGI_FORMAT_BC1_UNORM)
PITCH2D(bc1_8x8, 8, 8, DXGI_FORMAT_BC1_UNORM)
PITCH2D(bc1_64x32, 64, 32, DXGI_FORMAT_BC1_UNORM)
PITCH2D(bc1_12x12, 12, 12, DXGI_FORMAT_BC1_UNORM)
PITCH2D(bc1_1x1, 1, 1, DXGI_FORMAT_BC1_UNORM)
PITCH2D(bc1_6x10, 6, 10, DXGI_FORMAT_BC1_UNORM)
PITCH2D(bc2_16x16, 16, 16, DXGI_FORMAT_BC2_UNORM)
PITCH2D(bc3_16x16, 16, 16, DXGI_FORMAT_BC3_UNORM)
PITCH2D(bc3_20x4, 20, 4, DXGI_FORMAT_BC3_UNORM)
PITCH2D(bc4_16x16, 16, 16, DXGI_FORMAT_BC4_UNORM)
PITCH2D(bc5_16x16, 16, 16, DXGI_FORMAT_BC5_UNORM)
PITCH2D(bc6h_16x16, 16, 16, DXGI_FORMAT_BC6H_UF16)
PITCH2D(bc7_16x16, 16, 16, DXGI_FORMAT_BC7_UNORM)
PITCH2D(bc7_28x8, 28, 8, DXGI_FORMAT_BC7_UNORM)

/* Exact values for the simplest layouts are driver dependent; a tight 512-wide RGBA8 is a note. */
DEVTEST(d3d11_textures_pitch_rgba8_512_tightness_note) {
    ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(tx_mk2d(c, 512, 512, 1, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ_WRITE, 0, &m), S_OK);
    t_note("512x512 RGBA8 staging: RowPitch %u DepthPitch %u (tight %u / %u)", m.RowPitch, m.DepthPitch, 512 * 4, 512 * 512 * 4);
    CHECK(m.RowPitch >= 2048);
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11Texture2D_Release(t);
}

static void tx_pitch_chain(tx_ctx *c, UINT w, UINT h, DXGI_FORMAT f, UINT arr, const char *file, int line) {
    ID3D11Texture2D *t = NULL; HRESULT hr; UINT mips = tx_full_mips(w > h ? w : h), s;
    hr = tx_mk2d(c, w, h, mips, arr, f, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t);
    if (FAILED(hr)) { t_note("%s:%d chain %ux%u fmt %u: %s", file, line, w, h, f, d3dh_hr_name(hr)); return; }
    for (s = 0; s < mips * arr; s++) {
        D3D11_MAPPED_SUBRESOURCE m; UINT mip = s % mips, rb = tx_row_bytes(f, tx_dim(w, mip)), rows = tx_rows(f, tx_dim(h, mip));
        memset(&m, 0, sizeof m);
        hr = ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, s, D3D11_MAP_READ_WRITE, 0, &m);
        t_check(hr == S_OK, file, line, "Map", "sub %u: %s", s, d3dh_hr_name(hr));
        if (FAILED(hr)) continue;
        t_check(m.pData != NULL && m.RowPitch >= rb && m.DepthPitch >= (UINT64)m.RowPitch * rows, file, line, "pitches", "sub %u (mip %u): RowPitch %u (min %u) DepthPitch %u (rows %u)", s, mip, m.RowPitch, rb, m.DepthPitch, rows);
        ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, s);
    }
    ID3D11Texture2D_Release(t);
}
#define PCHAIN(nm, W, H, F, A) DEVTEST(d3d11_textures_pitch_chain_##nm) { tx_pitch_chain(c, W, H, F, A, __FILE__, __LINE__); }
PCHAIN(rgba8_64, 64, 64, F_RGBA8, 1)
PCHAIN(rgba8_37x11_arr3, 37, 11, F_RGBA8, 3)
PCHAIN(r8_100x3, 100, 3, F_R8, 1)
PCHAIN(r32g32b32a32_16, 16, 16, DXGI_FORMAT_R32G32B32A32_FLOAT, 1)
PCHAIN(bc1_64, 64, 64, DXGI_FORMAT_BC1_UNORM, 1)
PCHAIN(bc3_32_arr2, 32, 32, DXGI_FORMAT_BC3_UNORM, 2)
PCHAIN(bc7_16, 16, 16, DXGI_FORMAT_BC7_UNORM, 1)

DEVTEST(d3d11_textures_pitch_3d_basic) {
    ID3D11Texture3D *t = NULL; D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(tx_mk3d(c, 5, 3, 4, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ_WRITE, 0, &m), S_OK);
    CHECK(m.pData != NULL); CHECK(m.RowPitch >= 20); CHECK(m.DepthPitch >= m.RowPitch * 3);
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11Texture3D_Release(t);
}
DEVTEST(d3d11_textures_pitch_3d_bc1) {
    ID3D11Texture3D *t = NULL; D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(tx_mk3d(c, 8, 8, 2, 1, DXGI_FORMAT_BC1_UNORM, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ_WRITE, 0, &m), S_OK);
    CHECK(m.pData != NULL); CHECK(m.RowPitch >= 16); CHECK(m.DepthPitch >= m.RowPitch * 2);
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11Texture3D_Release(t);
}
DEVTEST(d3d11_textures_pitch_3d_chain) {
    ID3D11Texture3D *t = NULL; UINT s;
    EXPECT_HR(tx_mk3d(c, 16, 8, 4, 0, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return;
    for (s = 0; s < 5; s++) {
        D3D11_MAPPED_SUBRESOURCE m; UINT rb = tx_dim(16, s) * 4, rows = tx_dim(8, s);
        HRESULT hr = ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, s, D3D11_MAP_READ_WRITE, 0, &m);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) { CHECKF(m.RowPitch >= rb && m.DepthPitch >= m.RowPitch * rows, "mip %u: RowPitch %u DepthPitch %u", s, m.RowPitch, m.DepthPitch); ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, s); }
    }
    { D3D11_MAPPED_SUBRESOURCE m = {0}; EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 5, D3D11_MAP_READ, 0, &m), E_INVALIDARG); }
    ID3D11Texture3D_Release(t);
}
DEVTEST(d3d11_textures_pitch_1d_map) {
    ID3D11Texture1D *t = NULL; D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(tx_mk1d(c, 33, 1, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return;
    memset(&m, 0, sizeof m);
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ_WRITE, 0, &m), S_OK);
    CHECK(m.pData != NULL);
    t_note("1D staging map: RowPitch %u DepthPitch %u", m.RowPitch, m.DepthPitch);
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11Texture1D_Release(t);
}

/* ---- Map error handling -------------------------------------------------------------------------------------------- */
#define MAP_OOR(nm, MIPS, ARR) DEVTEST(d3d11_textures_map_out_of_range_##nm) { \
    ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE m; UINT n = MIPS * ARR; \
    EXPECT_HR(tx_mk2d(c, 16, 16, MIPS, ARR, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return; \
    m.pData = (void *)(ULONG_PTR)0xDEADBEEF; m.RowPitch = 0xABAB; m.DepthPitch = 0xCDCD; \
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, n, D3D11_MAP_READ, 0, &m), E_INVALIDARG); \
    EXPECT_EQ(m.pData, NULL); EXPECT_EQ(m.RowPitch, 0xABAB); EXPECT_EQ(m.DepthPitch, 0xCDCD); \
    memset(&m, 0, sizeof m); EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, n - 1, D3D11_MAP_READ, 0, &m), S_OK); \
    if (m.pData) ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, n - 1); \
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0xFFFFFFFFu, D3D11_MAP_READ, 0, &m), E_INVALIDARG); \
    ID3D11Texture2D_Release(t); }
MAP_OOR(single, 1, 1)
MAP_OOR(mips3, 3, 1)
MAP_OOR(arr2, 1, 2)
MAP_OOR(mips3_arr2, 3, 2)
MAP_OOR(mips5_arr4, 5, 4)

DEVTEST(d3d11_textures_map_out_of_range_3d_1d) {
    ID3D11Texture3D *t3 = NULL; ID3D11Texture1D *t1 = NULL; D3D11_MAPPED_SUBRESOURCE m = {0};
    EXPECT_HR(tx_mk3d(c, 8, 8, 8, 1, F_RGBA8, U_STG, 0, CPU_R, 0, NULL, &t3), S_OK);
    EXPECT_HR(tx_mk1d(c, 8, 2, 2, F_RGBA8, U_STG, 0, CPU_R, 0, NULL, &t1), S_OK);
    if (t3) { EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t3, 1, D3D11_MAP_READ, 0, &m), E_INVALIDARG); ID3D11Texture3D_Release(t3); }
    if (t1) { EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t1, 4, D3D11_MAP_READ, 0, &m), E_INVALIDARG); ID3D11Texture1D_Release(t1); }
}

static void tx_access(tx_ctx *c, ID3D11Resource *r, int dynamic, int cpu_r, int cpu_w, const char *file, int line) {
    D3D11_MAPPED_SUBRESOURCE m; HRESULT hr, want;
    memset(&m, 0, sizeof m);
    want = cpu_r ? S_OK : E_INVALIDARG;
    hr = ID3D11DeviceContext_Map(c->ctx, r, 0, D3D11_MAP_READ, 0, &m);
    t_check(hr == want, file, line, "Map(READ)", "got %s want %s", d3dh_hr_name(hr), d3dh_hr_name(want));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(c->ctx, r, 0);
    want = (!dynamic && cpu_w) ? S_OK : E_INVALIDARG;
    hr = ID3D11DeviceContext_Map(c->ctx, r, 0, D3D11_MAP_WRITE, 0, &m);
    t_check(hr == want, file, line, "Map(WRITE)", "got %s want %s", d3dh_hr_name(hr), d3dh_hr_name(want));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(c->ctx, r, 0);
    want = (cpu_r && cpu_w) ? S_OK : E_INVALIDARG;
    hr = ID3D11DeviceContext_Map(c->ctx, r, 0, D3D11_MAP_READ_WRITE, 0, &m);
    t_check(hr == want, file, line, "Map(READ_WRITE)", "got %s want %s", d3dh_hr_name(hr), d3dh_hr_name(want));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(c->ctx, r, 0);
    want = dynamic ? S_OK : E_INVALIDARG;
    hr = ID3D11DeviceContext_Map(c->ctx, r, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    t_check(hr == want, file, line, "Map(WRITE_DISCARD)", "got %s want %s", d3dh_hr_name(hr), d3dh_hr_name(want));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(c->ctx, r, 0);
    hr = ID3D11DeviceContext_Map(c->ctx, r, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &m);
    t_check(hr == E_INVALIDARG, file, line, "Map(WRITE_NO_OVERWRITE) on texture", "got %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(c->ctx, r, 0);
}
/* kind: 1=1D 2=2D 3=3D */
static void tx_access_case(tx_ctx *c, int kind, D3D11_USAGE u, UINT bind, UINT cpu, const char *file, int line) {
    ID3D11Resource *r = NULL; HRESULT hr = E_FAIL; tx_init in; D3D11_SUBRESOURCE_DATA *sd = NULL;
    memset(&in, 0, sizeof in);
    if (u == D3D11_USAGE_IMMUTABLE) { tx_init_make(&in, 8, kind >= 2 ? 8 : 1, kind == 3 ? 4 : 1, 1, 1, F_RGBA8, 0, 0, 1); sd = in.sd; }
    if (kind == 1) hr = tx_mk1d(c, 8, 1, 1, F_RGBA8, u, bind, cpu, 0, sd, (ID3D11Texture1D **)&r);
    if (kind == 2) hr = tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, u, bind, cpu, 0, sd, (ID3D11Texture2D **)&r);
    if (kind == 3) hr = tx_mk3d(c, 8, 8, 4, 1, F_RGBA8, u, bind, cpu, 0, sd, (ID3D11Texture3D **)&r);
    t_check(hr == S_OK, file, line, "create", "%s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { tx_access(c, r, u == D3D11_USAGE_DYNAMIC, !!(cpu & CPU_R), !!(cpu & CPU_W), file, line); ID3D11Resource_Release(r); }
    tx_init_free(&in);
}
#define ACCESS(nm, KIND, U, B, CPU) DEVTEST(d3d11_textures_access_##nm) { tx_access_case(c, KIND, U, B, CPU, __FILE__, __LINE__); }
ACCESS(2d_staging_r, 2, U_STG, 0, CPU_R)
ACCESS(2d_staging_w, 2, U_STG, 0, CPU_W)
ACCESS(2d_staging_rw, 2, U_STG, 0, CPU_R | CPU_W)
ACCESS(2d_dynamic, 2, U_DYN, B_SRV, CPU_W)
ACCESS(2d_default, 2, U_DEF, B_SRV, 0)
ACCESS(2d_default_rt, 2, U_DEF, B_RT, 0)
ACCESS(2d_immutable, 2, U_IMM, B_SRV, 0)
ACCESS(1d_staging_r, 1, U_STG, 0, CPU_R)
ACCESS(1d_staging_w, 1, U_STG, 0, CPU_W)
ACCESS(1d_staging_rw, 1, U_STG, 0, CPU_R | CPU_W)
ACCESS(1d_dynamic, 1, U_DYN, B_SRV, CPU_W)
ACCESS(1d_default, 1, U_DEF, B_SRV, 0)
ACCESS(1d_immutable, 1, U_IMM, B_SRV, 0)
ACCESS(3d_staging_r, 3, U_STG, 0, CPU_R)
ACCESS(3d_staging_w, 3, U_STG, 0, CPU_W)
ACCESS(3d_staging_rw, 3, U_STG, 0, CPU_R | CPU_W)
ACCESS(3d_dynamic, 3, U_DYN, B_SRV, CPU_W)
ACCESS(3d_default, 3, U_DEF, B_SRV, 0)
ACCESS(3d_immutable, 3, U_IMM, B_SRV, 0)

DEVTEST(d3d11_textures_map_invalid_type_note) {
    ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE m;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return;
    t_note("Map type 0 -> %s", d3dh_hr_name(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, (D3D11_MAP)0, 0, &m)));
    t_note("Map type 6 -> %s", d3dh_hr_name(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, (D3D11_MAP)6, 0, &m)));
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_map_double_map_note) {
    ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE m, m2; HRESULT hr;
    EXPECT_HR(tx_mk2d(c, 8, 8, 2, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &m), S_OK);
    hr = ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &m2);
    t_note("second Map of the same subresource -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    hr = ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 1, D3D11_MAP_READ, 0, &m2);
    t_note("Map of another subresource while one is mapped -> %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 1);
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_map_staging_roundtrip_dword) {
    ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE m; DWORD v;
    EXPECT_HR(tx_mk2d(c, 32, 32, 1, 1, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &t), S_OK); if (!t) return;
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_WRITE, 0, &m), S_OK);
    *(DWORD *)m.pData = 0xDEADBEEF;
    ((DWORD *)((char *)m.pData + 31 * m.RowPitch))[31] = 0x01020304;
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    EXPECT_HR(ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 0, D3D11_MAP_READ, 0, &m), S_OK);
    v = *(DWORD *)m.pData; EXPECT_EQ(v, 0xDEADBEEF);
    v = ((DWORD *)((char *)m.pData + 31 * m.RowPitch))[31]; EXPECT_EQ(v, 0x01020304);
    ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 0);
    ID3D11Texture2D_Release(t);
}

/* ---- subresource indexing: index = mip + array * mips (+ plane * mips * arrays) --------------------------------------- */
static void tx_index_case2d(tx_ctx *c, UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT f, const char *file, int line) {
    ID3D11Texture2D *stg = NULL, *def = NULL; HRESULT hr; UINT n = mips * arr, s;
    hr = tx_mk2d(c, w, h, mips, arr, f, U_STG, 0, CPU_R | CPU_W, 0, NULL, &stg);
    if (FAILED(hr)) { t_note("%s:%d staging create: %s", file, line, d3dh_hr_name(hr)); return; }
    hr = tx_mk2d(c, w, h, mips, arr, f, U_DEF, B_SRV, 0, 0, NULL, &def);
    t_check(hr == S_OK, file, line, "create default", "%s", d3dh_hr_name(hr));
    if (FAILED(hr)) { ID3D11Texture2D_Release(stg); return; }
    for (s = 0; s < n; s++) {
        hr = tx_write_pat(c, (ID3D11Resource *)stg, s, w, h, 1, mips, f, 100 + s, D3D11_MAP_WRITE);
        t_check(hr == S_OK, file, line, "write subresource", "sub %u: %s", s, d3dh_hr_name(hr));
    }
    ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource *)def, (ID3D11Resource *)stg);
    tx_verify_all(c, (ID3D11Resource *)def, n, 100, file, line);
    tx_verify_all(c, (ID3D11Resource *)stg, n, 100, file, line);
    ID3D11Texture2D_Release(def); ID3D11Texture2D_Release(stg);
}
#define IDX2D(nm, W, H, M, A, F) DEVTEST(d3d11_textures_index_2d_##nm) { tx_index_case2d(c, W, H, M, A, F, __FILE__, __LINE__); }
IDX2D(rgba8_m1_a1, 8, 8, 1, 1, F_RGBA8)
IDX2D(rgba8_m4_a1, 8, 8, 4, 1, F_RGBA8)
IDX2D(rgba8_m1_a3, 8, 8, 1, 3, F_RGBA8)
IDX2D(rgba8_m4_a3, 8, 8, 4, 3, F_RGBA8)
IDX2D(rgba8_m3_a5, 5, 7, 3, 5, F_RGBA8)
IDX2D(rgba8_m6_a2_32, 32, 32, 6, 2, F_RGBA8)
IDX2D(r8_m5_a4, 16, 16, 5, 4, F_R8)
IDX2D(r8_odd_m4_a2, 13, 9, 4, 2, F_R8)
IDX2D(r16g16_m3_a3, 7, 4, 3, 3, DXGI_FORMAT_R16G16_UNORM)
IDX2D(r32g32b32a32_m3_a2, 4, 4, 3, 2, DXGI_FORMAT_R32G32B32A32_FLOAT)
IDX2D(bc1_m5_a1, 16, 16, 5, 1, DXGI_FORMAT_BC1_UNORM)
IDX2D(bc1_m3_a4, 16, 16, 3, 4, DXGI_FORMAT_BC1_UNORM)
IDX2D(bc3_m5_a2, 16, 16, 5, 2, DXGI_FORMAT_BC3_UNORM)
IDX2D(bc7_m3_a2, 8, 8, 3, 2, DXGI_FORMAT_BC7_UNORM)
IDX2D(bc4_m4_a2, 8, 8, 4, 2, DXGI_FORMAT_BC4_UNORM)
IDX2D(1x1_m1_a6, 1, 1, 1, 6, F_RGBA8)
IDX2D(16x1_m5_a2, 16, 1, 5, 2, F_RGBA8)

DEVTEST(d3d11_textures_index_cube_faces) {
    /* a cube is a 6-slice array: face order +X -X +Y -Y +Z -Z maps to array slices 0..5 */
    ID3D11Texture2D *stg = NULL, *def = NULL; UINT n = 3 * 6, s;
    EXPECT_HR(tx_mk2d(c, 4, 4, 3, 6, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &stg), S_OK);
    EXPECT_HR(tx_mk2d(c, 4, 4, 3, 6, F_RGBA8, U_DEF, B_SRV, 0, M_CUBE, NULL, &def), S_OK);
    if (!stg || !def) { SAFE_RELEASE(stg); SAFE_RELEASE(def); return; }
    for (s = 0; s < n; s++) EXPECT_HR(tx_write_pat(c, (ID3D11Resource *)stg, s, 4, 4, 1, 3, F_RGBA8, 200 + s, D3D11_MAP_WRITE), S_OK);
    ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource *)def, (ID3D11Resource *)stg);
    tx_verify_all(c, (ID3D11Resource *)def, n, 200, __FILE__, __LINE__);
    ID3D11Texture2D_Release(def); ID3D11Texture2D_Release(stg);
}
DEVTEST(d3d11_textures_index_cube_array) {
    ID3D11Texture2D *stg = NULL, *def = NULL; UINT n = 2 * 12, s;
    EXPECT_HR(tx_mk2d(c, 2, 2, 2, 12, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &stg), S_OK);
    EXPECT_HR(tx_mk2d(c, 2, 2, 2, 12, F_RGBA8, U_DEF, B_SRV, 0, M_CUBE, NULL, &def), S_OK);
    if (!stg || !def) { SAFE_RELEASE(stg); SAFE_RELEASE(def); return; }
    for (s = 0; s < n; s++) EXPECT_HR(tx_write_pat(c, (ID3D11Resource *)stg, s, 2, 2, 1, 2, F_RGBA8, 300 + s, D3D11_MAP_WRITE), S_OK);
    ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource *)def, (ID3D11Resource *)stg);
    tx_verify_all(c, (ID3D11Resource *)def, n, 300, __FILE__, __LINE__);
    ID3D11Texture2D_Release(def); ID3D11Texture2D_Release(stg);
}
static void tx_index_case1d(tx_ctx *c, UINT w, UINT mips, UINT arr, DXGI_FORMAT f, const char *file, int line) {
    ID3D11Texture1D *stg = NULL, *def = NULL; UINT n = mips * arr, s; HRESULT hr;
    hr = tx_mk1d(c, w, mips, arr, f, U_STG, 0, CPU_R | CPU_W, 0, NULL, &stg);
    t_check(hr == S_OK, file, line, "create staging 1D", "%s", d3dh_hr_name(hr)); if (FAILED(hr)) return;
    hr = tx_mk1d(c, w, mips, arr, f, U_DEF, B_SRV, 0, 0, NULL, &def);
    t_check(hr == S_OK, file, line, "create default 1D", "%s", d3dh_hr_name(hr)); if (FAILED(hr)) { ID3D11Texture1D_Release(stg); return; }
    for (s = 0; s < n; s++) { hr = tx_write_pat(c, (ID3D11Resource *)stg, s, w, 1, 1, mips, f, 400 + s, D3D11_MAP_WRITE); t_check(hr == S_OK, file, line, "write 1D", "sub %u: %s", s, d3dh_hr_name(hr)); }
    ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource *)def, (ID3D11Resource *)stg);
    tx_verify_all(c, (ID3D11Resource *)def, n, 400, file, line);
    ID3D11Texture1D_Release(def); ID3D11Texture1D_Release(stg);
}
#define IDX1D(nm, W, M, A, F) DEVTEST(d3d11_textures_index_1d_##nm) { tx_index_case1d(c, W, M, A, F, __FILE__, __LINE__); }
IDX1D(m1_a1, 16, 1, 1, F_RGBA8)
IDX1D(m5_a1, 16, 5, 1, F_RGBA8)
IDX1D(m1_a4, 16, 1, 4, F_RGBA8)
IDX1D(m4_a3, 13, 4, 3, F_RGBA8)
IDX1D(r8_m3_a2, 7, 3, 2, F_R8)
IDX1D(r32f_m2_a2, 5, 2, 2, DXGI_FORMAT_R32_FLOAT)

static void tx_index_case3d(tx_ctx *c, UINT w, UINT h, UINT d, UINT mips, DXGI_FORMAT f, const char *file, int line) {
    ID3D11Texture3D *stg = NULL, *def = NULL; UINT s; HRESULT hr;
    hr = tx_mk3d(c, w, h, d, mips, f, U_STG, 0, CPU_R | CPU_W, 0, NULL, &stg);
    t_check(hr == S_OK, file, line, "create staging 3D", "%s", d3dh_hr_name(hr)); if (FAILED(hr)) return;
    hr = tx_mk3d(c, w, h, d, mips, f, U_DEF, B_SRV, 0, 0, NULL, &def);
    t_check(hr == S_OK, file, line, "create default 3D", "%s", d3dh_hr_name(hr)); if (FAILED(hr)) { ID3D11Texture3D_Release(stg); return; }
    for (s = 0; s < mips; s++) { hr = tx_write_pat(c, (ID3D11Resource *)stg, s, w, h, d, mips, f, 500 + s, D3D11_MAP_WRITE); t_check(hr == S_OK, file, line, "write 3D", "sub %u: %s", s, d3dh_hr_name(hr)); }
    ID3D11DeviceContext_CopyResource(c->ctx, (ID3D11Resource *)def, (ID3D11Resource *)stg);
    tx_verify_all(c, (ID3D11Resource *)def, mips, 500, file, line);
    ID3D11Texture3D_Release(def); ID3D11Texture3D_Release(stg);
}
#define IDX3D(nm, W, H, D, M, F) DEVTEST(d3d11_textures_index_3d_##nm) { tx_index_case3d(c, W, H, D, M, F, __FILE__, __LINE__); }
IDX3D(rgba8_4x4x4_m1, 4, 4, 4, 1, F_RGBA8)
IDX3D(rgba8_4x4x4_m3, 4, 4, 4, 3, F_RGBA8)
IDX3D(rgba8_8x4x2_m4, 8, 4, 2, 4, F_RGBA8)
IDX3D(rgba8_5x3x7_m3, 5, 3, 7, 3, F_RGBA8)
IDX3D(r8_16x2x8_m5, 16, 2, 8, 5, F_R8)
IDX3D(bc1_8x8x4_m4, 8, 8, 4, 4, DXGI_FORMAT_BC1_UNORM)
IDX3D(bc3_8x4x2_m4, 8, 4, 2, 4, DXGI_FORMAT_BC3_UNORM)
IDX3D(r32f_2x2x2_m2, 2, 2, 2, 2, DXGI_FORMAT_R32_FLOAT)

/* Writing one subresource must not disturb its neighbours. */
DEVTEST(d3d11_textures_index_isolation) {
    ID3D11Texture2D *stg = NULL; UINT s, n = 4 * 3;
    EXPECT_HR(tx_mk2d(c, 8, 8, 4, 3, F_RGBA8, U_STG, 0, CPU_R | CPU_W, 0, NULL, &stg), S_OK); if (!stg) return;
    for (s = 0; s < n; s++) EXPECT_HR(tx_write_pat(c, (ID3D11Resource *)stg, s, 8, 8, 1, 4, F_RGBA8, 10 + s, D3D11_MAP_WRITE), S_OK);
    EXPECT_HR(tx_write_pat(c, (ID3D11Resource *)stg, 6, 8, 8, 1, 4, F_RGBA8, 999, D3D11_MAP_WRITE), S_OK);
    for (s = 0; s < n; s++) {
        tx_img im; size_t bad;
        if (FAILED(tx_fetch(c, (ID3D11Resource *)stg, s, &im))) { CHECK(0); continue; }
        CHECKF(tx_img_is_pat(&im, s == 6 ? 999 : 10 + s, &bad), "sub %u corrupted at %u", s, (unsigned)bad);
        tx_img_free(&im);
    }
    ID3D11Texture2D_Release(stg);
}
