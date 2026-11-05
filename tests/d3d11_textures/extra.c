/* Additional mip-chain length, subresource-index and GetDesc normalisation cases. */
#include "txh.h"

static void tx_full_chain_3d(tx_ctx *c, UINT w, UINT h, UINT d, const char *file, int line) {
    ID3D11Texture3D *t = NULL; D3D11_TEXTURE3D_DESC g; UINT m = w > h ? w : h, em;
    HRESULT hr = tx_mk3d(c, w, h, d, 0, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t);
    t_check(hr == S_OK, file, line, "create 3D full chain", "%s", d3dh_hr_name(hr));
    if (!t) return;
    if (d > m) m = d;
    em = tx_full_mips(m);
    ID3D11Texture3D_GetDesc(t, &g);
    t_check(g.MipLevels == em, file, line, "3D full chain length", "%ux%ux%u: got %u want %u", w, h, d, g.MipLevels, em);
    ID3D11Texture3D_Release(t);
}
#define F3D(nm, W, H, D) DEVTEST(d3d11_textures_x3d_chain_##nm) { tx_full_chain_3d(c, W, H, D, __FILE__, __LINE__); }
F3D(deep, 2, 2, 64)
F3D(wide, 64, 2, 2)
F3D(tall, 2, 64, 2)
F3D(odd, 7, 5, 3)
F3D(unit, 1, 1, 1)
F3D(flat, 33, 17, 1)

static void tx_too_many_mips(tx_ctx *c, UINT w, UINT h, UINT mips, const char *file, int line) {
    ID3D11Texture2D *t = NULL;
    HRESULT hr = tx_mk2d(c, w, h, mips, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t);
    t_note("%s:%d %ux%u with %u mip levels (full chain %u): %s", file, line, w, h, mips, tx_full_mips(w > h ? w : h), d3dh_hr_name(hr));
    SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_x2d_mip_count_beyond_chain_note) {
    tx_too_many_mips(c, 16, 16, 6, __FILE__, __LINE__);
    tx_too_many_mips(c, 16, 16, 20, __FILE__, __LINE__);
    tx_too_many_mips(c, 1, 1, 2, __FILE__, __LINE__);
}
DEVTEST(d3d11_textures_x2d_exact_chain_accepted) {
    ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC g;
    EXPECT_HR(tx_mk2d(c, 16, 16, 5, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (!t) return;
    ID3D11Texture2D_GetDesc(t, &g);
    EXPECT_EQ(g.MipLevels, 5);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_x2d_getdesc_zero_mips_normalised_for_array) {
    ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC g;
    EXPECT_HR(tx_mk2d(c, 40, 24, 0, 3, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (!t) return;
    ID3D11Texture2D_GetDesc(t, &g);
    EXPECT_EQ(g.MipLevels, 6);
    EXPECT_EQ(g.ArraySize, 3);
    ID3D11Texture2D_Release(t);
}
DEVTEST(d3d11_textures_x2d_subresource_count_via_staging_map) {
    /* Array 3 x 2 mips: subresource 5 is valid, 6 is out of range (Map fails). */
    ID3D11Texture2D *t = NULL; D3D11_MAPPED_SUBRESOURCE m; HRESULT hr;
    EXPECT_HR(tx_mk2d(c, 8, 8, 2, 3, F_RGBA8, U_STG, 0, CPU_R, 0, NULL, &t), S_OK);
    if (!t) return;
    hr = ID3D11DeviceContext_Map(c->ctx, (ID3D11Resource *)t, 5, D3D11_MAP_READ, 0, &m);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        CHECKF(m.RowPitch >= 4 * 4, "row pitch %u for 4-texel row", m.RowPitch);
        ID3D11DeviceContext_Unmap(c->ctx, (ID3D11Resource *)t, 5);
    }
    t_note("Map of subresource 6 (out of range) is not exercised: runtime-layer behaviour");
    ID3D11Texture2D_Release(t);
}
