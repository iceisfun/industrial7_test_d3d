/* Texture1D and Texture3D creation validity matrices plus argument-validation tests. */
#include "txh.h"

#define OKAY S_OK
#define BAD E_INVALIDARG

static void tx_c1d(tx_ctx *c, const char *file, int line, UINT w, UINT mips, UINT arr, DXGI_FORMAT f, D3D11_USAGE u,
                   UINT bind, UINT cpu, UINT misc, HRESULT want, UINT minfl) {
    D3D11_TEXTURE1D_DESC d, g; tx_init in; D3D11_SUBRESOURCE_DATA *sd = NULL; ID3D11Texture1D *t = NULL; HRESULT hr, hr2, w2;
    int hard = c->fl >= (D3D_FEATURE_LEVEL)minfl;
    memset(&in, 0, sizeof in); memset(&d, 0, sizeof d);
    d.Width = w; d.MipLevels = mips; d.ArraySize = arr; d.Format = f; d.Usage = u; d.BindFlags = bind; d.CPUAccessFlags = cpu; d.MiscFlags = misc;
    if (u == D3D11_USAGE_IMMUTABLE && w && w <= 16384 && arr && arr <= 64 && tx_init_make(&in, w, 1, 1, mips <= 16 ? mips : 1, arr, f, 0, 0, 3)) sd = in.sd;
    hr = ID3D11Device_CreateTexture1D(c->dev, &d, sd, &t);
    t_check(!hard || hr == want, file, line, "CreateTexture1D", "got %s want %s (feature level %#x)", d3dh_hr_name(hr), d3dh_hr_name(want), c->fl);
    if (!hard && hr != want) t_note("%s:%d below feature level %#x: got %s want %s", file, line, minfl, d3dh_hr_name(hr), d3dh_hr_name(want));
    if (SUCCEEDED(hr) && t) {
        UINT em = mips ? mips : tx_full_mips(w);
        ID3D11Texture1D_GetDesc(t, &g);
        t_check(g.Width == w && g.MipLevels == em && g.ArraySize == arr && g.Format == f, file, line, "GetDesc 1D", "w %u/%u mips %u/%u arr %u/%u fmt %u/%u", g.Width, w, g.MipLevels, em, g.ArraySize, arr, g.Format, f);
        t_check(g.Usage == u && g.BindFlags == bind && g.CPUAccessFlags == cpu && g.MiscFlags == misc, file, line, "GetDesc 1D flags", "usage %u/%u bind %#x/%#x cpu %#x/%#x misc %#x/%#x", g.Usage, u, g.BindFlags, bind, g.CPUAccessFlags, cpu, g.MiscFlags, misc);
        ID3D11Texture1D_Release(t);
    }
    hr2 = ID3D11Device_CreateTexture1D(c->dev, &d, sd, NULL);
    w2 = want == S_OK ? S_FALSE : want;
    t_check(!hard || hr2 == w2, file, line, "CreateTexture1D(validate only)", "got %s want %s", d3dh_hr_name(hr2), d3dh_hr_name(w2));
    tx_init_free(&in);
}

static void tx_c3d(tx_ctx *c, const char *file, int line, UINT w, UINT h, UINT dp, UINT mips, DXGI_FORMAT f, D3D11_USAGE u,
                   UINT bind, UINT cpu, UINT misc, HRESULT want, UINT minfl) {
    D3D11_TEXTURE3D_DESC d, g; tx_init in; D3D11_SUBRESOURCE_DATA *sd = NULL; ID3D11Texture3D *t = NULL; HRESULT hr, hr2, w2;
    int hard = c->fl >= (D3D_FEATURE_LEVEL)minfl;
    memset(&in, 0, sizeof in); memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.Depth = dp; d.MipLevels = mips; d.Format = f; d.Usage = u; d.BindFlags = bind; d.CPUAccessFlags = cpu; d.MiscFlags = misc;
    if (u == D3D11_USAGE_IMMUTABLE && w && h && dp && w <= 256 && h <= 256 && dp <= 256 && tx_init_make(&in, w, h, dp, mips <= 16 ? mips : 1, 1, f, 0, 0, 5)) sd = in.sd;
    hr = ID3D11Device_CreateTexture3D(c->dev, &d, sd, &t);
    t_check(!hard || hr == want, file, line, "CreateTexture3D", "got %s want %s (feature level %#x)", d3dh_hr_name(hr), d3dh_hr_name(want), c->fl);
    if (!hard && hr != want) t_note("%s:%d below feature level %#x: got %s want %s", file, line, minfl, d3dh_hr_name(hr), d3dh_hr_name(want));
    if (SUCCEEDED(hr) && t) {
        UINT m2 = w > h ? w : h, em; if (dp > m2) m2 = dp; em = mips ? mips : tx_full_mips(m2);
        ID3D11Texture3D_GetDesc(t, &g);
        t_check(g.Width == w && g.Height == h && g.Depth == dp, file, line, "GetDesc 3D size", "got %ux%ux%u", g.Width, g.Height, g.Depth);
        t_check(g.MipLevels == em && g.Format == f, file, line, "GetDesc 3D mips/format", "mips %u/%u fmt %u/%u", g.MipLevels, em, g.Format, f);
        t_check(g.Usage == u && g.BindFlags == bind && g.CPUAccessFlags == cpu && g.MiscFlags == misc, file, line, "GetDesc 3D flags", "usage %u/%u bind %#x/%#x cpu %#x/%#x misc %#x/%#x", g.Usage, u, g.BindFlags, bind, g.CPUAccessFlags, cpu, g.MiscFlags, misc);
        ID3D11Texture3D_Release(t);
    }
    hr2 = ID3D11Device_CreateTexture3D(c->dev, &d, sd, NULL);
    w2 = want == S_OK ? S_FALSE : want;
    t_check(!hard || hr2 == w2, file, line, "CreateTexture3D(validate only)", "got %s want %s", d3dh_hr_name(hr2), d3dh_hr_name(w2));
    tx_init_free(&in);
}

#define C1D(nm, W, M, A, F, U, B, CP, MI, WANT, MINFL) \
    DEVTEST(d3d11_textures_c1d_##nm) { tx_c1d(c, __FILE__, __LINE__, W, M, A, F, U, B, CP, MI, WANT, MINFL); }
#define C3D(nm, W, H, DP, M, F, U, B, CP, MI, WANT, MINFL) \
    DEVTEST(d3d11_textures_c3d_##nm) { tx_c3d(c, __FILE__, __LINE__, W, H, DP, M, F, U, B, CP, MI, WANT, MINFL); }

C1D(w0, 0, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(w1, 1, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(w512, 512, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(w513, 513, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(w8192, 8192, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(w16384, 16384, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, OKAY, FL_11_0)
C1D(w16385, 16385, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(w_huge, 0xFFFFFFFFu, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(mips0_512, 512, 0, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(mips10_512, 512, 10, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(mips11_512, 512, 11, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(mips2_w1, 1, 2, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(mips0_w1, 1, 0, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(mips0_w1000, 1000, 0, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(mips10_w1000, 1000, 10, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(mips11_w1000, 1000, 11, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(mips15_16384, 16384, 15, 1, F_R8, U_DEF, B_SRV, 0, 0, OKAY, FL_11_0)
C1D(mips16_16384, 16384, 16, 1, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(mips_huge, 64, 0xFFFFFFFFu, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(arr0, 64, 1, 0, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(arr1, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(arr2, 64, 1, 2, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(arr3, 64, 1, 3, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(arr_mips, 64, 0, 5, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(arr512, 8, 1, 512, F_R8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(arr2048, 8, 1, 2048, F_R8, U_DEF, B_SRV, 0, 0, OKAY, FL_11_0)
C1D(arr2049, 8, 1, 2049, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(arr_huge, 8, 1, 0xFFFFFFFFu, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(def_cpuw, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, CPU_W, 0, BAD, 0)
C1D(def_cpur, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, CPU_R, 0, BAD, 0)
C1D(def_nobind, 64, 1, 1, F_RGBA8, U_DEF, 0, 0, 0, OKAY, 0)
C1D(dyn_srv_w, 64, 1, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, OKAY, 0)
C1D(dyn_srv_nocpu, 64, 1, 1, F_RGBA8, U_DYN, B_SRV, 0, 0, BAD, 0)
C1D(dyn_rt, 64, 1, 1, F_RGBA8, U_DYN, B_RT, CPU_W, 0, BAD, 0)
C1D(stg_r, 64, 1, 1, F_RGBA8, U_STG, 0, CPU_R, 0, OKAY, 0)
C1D(stg_rw, 64, 1, 1, F_RGBA8, U_STG, 0, CPU_R|CPU_W, 0, OKAY, 0)
C1D(stg_srv, 64, 1, 1, F_RGBA8, U_STG, B_SRV, CPU_R, 0, BAD, 0)
C1D(stg_nocpu, 64, 1, 1, F_RGBA8, U_STG, 0, 0, 0, BAD, 0)
C1D(imm_srv, 64, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, OKAY, 0)
C1D(imm_chain, 64, 0, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, OKAY, 0)
C1D(imm_array, 64, 1, 3, F_RGBA8, U_IMM, B_SRV, 0, 0, OKAY, 0)
C1D(imm_rt, 64, 1, 1, F_RGBA8, U_IMM, B_RT, 0, 0, BAD, 0)
C1D(imm_nobind, 64, 1, 1, F_RGBA8, U_IMM, 0, 0, 0, BAD, 0)
C1D(imm_cpuw, 64, 1, 1, F_RGBA8, U_IMM, B_SRV, CPU_W, 0, BAD, 0)
C1D(usage_5, 64, 1, 1, F_RGBA8, (D3D11_USAGE)5, B_SRV, 0, 0, BAD, 0)
C1D(bind_vb, 64, 1, 1, F_RGBA8, U_DEF, B_VB, 0, 0, BAD, 0)
C1D(bind_ib, 64, 1, 1, F_RGBA8, U_DEF, B_IB, 0, 0, BAD, 0)
C1D(bind_cb, 64, 1, 1, F_RGBA8, U_DEF, B_CB, 0, 0, BAD, 0)
C1D(bind_so, 64, 1, 1, F_RGBA8, U_DEF, B_SO, 0, 0, BAD, 0)
C1D(bind_undef, 64, 1, 1, F_RGBA8, U_DEF, 0x80000000u, 0, 0, BAD, 0)
C1D(bind_rt, 64, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, OKAY, 0)
C1D(bind_srv_rt, 64, 1, 1, F_RGBA8, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C1D(bind_ds_d24s8, 64, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, U_DEF, B_DS, 0, 0, OKAY, 0)
C1D(bind_ds_d32, 64, 1, 1, DXGI_FORMAT_D32_FLOAT, U_DEF, B_DS, 0, 0, OKAY, 0)
C1D(bind_ds_rgba8, 64, 1, 1, F_RGBA8, U_DEF, B_DS, 0, 0, BAD, 0)
C1D(bind_ds_srv_d32, 64, 1, 1, DXGI_FORMAT_D32_FLOAT, U_DEF, B_DS|B_SRV, 0, 0, BAD, 0)
C1D(bind_uav_r32u, 64, 1, 1, DXGI_FORMAT_R32_UINT, U_DEF, B_UAV, 0, 0, OKAY, FL_11_0)
C1D(bind_uav_srv_r32f, 64, 1, 1, DXGI_FORMAT_R32_FLOAT, U_DEF, B_UAV|B_SRV, 0, 0, OKAY, FL_11_0)
C1D(fmt_unknown, 64, 1, 1, DXGI_FORMAT_UNKNOWN, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(fmt_bad, 64, 1, 1, (DXGI_FORMAT)0xFFFF, U_DEF, B_SRV, 0, 0, BAD, 0)
C1D(fmt_r32g32b32a32_typeless, 64, 1, 1, DXGI_FORMAT_R32G32B32A32_TYPELESS, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(fmt_r32g32b32_float, 64, 1, 1, DXGI_FORMAT_R32G32B32_FLOAT, U_DEF, B_SRV, 0, 0, OKAY, 0)
C1D(fmt_r16g16b16a16f_rt, 64, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, U_DEF, B_RT|B_SRV, 0, 0, OKAY, 0)
C1D(fmt_r8_rt, 64, 1, 1, F_R8, U_DEF, B_RT|B_SRV, 0, 0, OKAY, 0)
C1D(fmt_a8_rt, 64, 1, 1, DXGI_FORMAT_A8_UNORM, U_DEF, B_RT|B_SRV, 0, 0, OKAY, 0)
C1D(fmt_bgra8_rt, 64, 1, 1, F_BGRA8, U_DEF, B_RT|B_SRV, 0, 0, OKAY, 0)
C1D(fmt_srgb_rt, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, U_DEF, B_RT|B_SRV, 0, 0, OKAY, 0)
C1D(fmt_r10g10b10a2_rt, 64, 1, 1, DXGI_FORMAT_R10G10B10A2_UNORM, U_DEF, B_RT|B_SRV, 0, 0, OKAY, 0)
C1D(fmt_r9g9b9e5_rt, 64, 1, 1, DXGI_FORMAT_R9G9B9E5_SHAREDEXP, U_DEF, B_RT, 0, 0, BAD, 0)
C1D(fmt_depth_rt, 64, 1, 1, DXGI_FORMAT_D32_FLOAT, U_DEF, B_RT, 0, 0, BAD, 0)
C1D(gen_rt_srv, 64, 0, 1, F_RGBA8, U_DEF, B_RT|B_SRV, 0, M_GEN, OKAY, 0)
C1D(gen_srv_only, 64, 0, 1, F_RGBA8, U_DEF, B_SRV, 0, M_GEN, BAD, 0)
C1D(gen_rt_only, 64, 0, 1, F_RGBA8, U_DEF, B_RT, 0, M_GEN, BAD, 0)
C1D(cube_misc, 64, 1, 6, F_RGBA8, U_DEF, B_SRV, 0, M_CUBE, BAD, 0)
C1D(misc_undef, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0x40000000u, BAD, 0)
C1D(misc_structured, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, BAD, 0)
C1D(misc_shared_keyed, 64, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, M_SHR|M_KEYED, BAD, 0)
C3D(w0, 0, 4, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(h0, 4, 0, 4, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(d0, 4, 4, 0, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(all0, 0, 0, 0, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(1x1x1, 1, 1, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(w2048, 2048, 1, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(h2048, 1, 2048, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(d2048, 1, 1, 2048, 1, F_R8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(w2049, 2049, 1, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(h2049, 1, 2049, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(d2049, 1, 1, 2049, 1, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(w16384, 16384, 1, 1, 1, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(whd_huge, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 1, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(64cube_rt, 64, 64, 64, 1, F_RGBA8, U_DEF, B_RT, 0, 0, OKAY, 0)
C3D(64cube_mips0, 64, 64, 64, 0, F_RGBA8, U_DEF, B_RT, 0, 0, OKAY, 0)
C3D(64cube_mips7, 64, 64, 64, 7, F_RGBA8, U_DEF, B_RT, 0, 0, OKAY, 0)
C3D(64cube_mips8, 64, 64, 64, 8, F_RGBA8, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(16x8x4_mips5, 16, 8, 4, 5, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(16x8x4_mips6, 16, 8, 4, 6, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(1x1x4_mips3, 1, 1, 4, 3, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(1x1x4_mips4, 1, 1, 4, 4, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(2048x1x1_mips12, 2048, 1, 1, 12, F_R8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(2048x1x1_mips13, 2048, 1, 1, 13, F_R8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(5x3x2_mips0, 5, 3, 2, 0, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(5x3x2_mips3, 5, 3, 2, 3, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(5x3x2_mips4, 5, 3, 2, 4, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(mips_huge, 8, 8, 8, 0xFFFFFFFFu, F_RGBA8, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(bind_vb, 8, 8, 8, 1, F_RGBA8, U_DEF, B_VB, 0, 0, BAD, 0)
C3D(bind_ib, 8, 8, 8, 1, F_RGBA8, U_DEF, B_IB, 0, 0, BAD, 0)
C3D(bind_cb, 8, 8, 8, 1, F_RGBA8, U_DEF, B_CB, 0, 0, BAD, 0)
C3D(bind_so, 8, 8, 8, 1, F_RGBA8, U_DEF, B_SO, 0, 0, BAD, 0)
C3D(bind_srv, 8, 8, 8, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bind_srv_rt, 8, 8, 8, 1, F_RGBA8, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(bind_ds_rgba8, 8, 8, 8, 1, F_RGBA8, U_DEF, B_DS, 0, 0, BAD, 0)
C3D(bind_ds_d32, 8, 8, 8, 1, DXGI_FORMAT_D32_FLOAT, U_DEF, B_DS, 0, 0, BAD, 0)
C3D(bind_ds_d24s8, 8, 8, 8, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, U_DEF, B_DS, 0, 0, BAD, 0)
C3D(bind_uav_r32u, 8, 8, 8, 1, DXGI_FORMAT_R32_UINT, U_DEF, B_UAV, 0, 0, OKAY, FL_11_0)
C3D(bind_undef, 8, 8, 8, 1, F_RGBA8, U_DEF, 0x80000000u, 0, 0, BAD, 0)
C3D(def_cpuw, 8, 8, 8, 1, F_RGBA8, U_DEF, B_SRV, CPU_W, 0, BAD, 0)
C3D(def_nobind, 8, 8, 8, 1, F_RGBA8, U_DEF, 0, 0, 0, OKAY, 0)
C3D(dyn_srv_w, 8, 8, 8, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, OKAY, 0)
C3D(dyn_rt, 8, 8, 8, 1, F_RGBA8, U_DYN, B_RT, CPU_W, 0, BAD, 0)
C3D(dyn_nocpu, 8, 8, 8, 1, F_RGBA8, U_DYN, B_SRV, 0, 0, BAD, 0)
C3D(stg_r, 8, 8, 8, 1, F_RGBA8, U_STG, 0, CPU_R, 0, OKAY, 0)
C3D(stg_rw_mips, 8, 8, 8, 0, F_RGBA8, U_STG, 0, CPU_R|CPU_W, 0, OKAY, 0)
C3D(stg_srv, 8, 8, 8, 1, F_RGBA8, U_STG, B_SRV, CPU_R, 0, BAD, 0)
C3D(stg_nocpu, 8, 8, 8, 1, F_RGBA8, U_STG, 0, 0, 0, BAD, 0)
C3D(imm_srv, 8, 8, 8, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, OKAY, 0)
C3D(imm_chain, 8, 8, 8, 0, F_RGBA8, U_IMM, B_SRV, 0, 0, OKAY, 0)
C3D(imm_bc1, 8, 8, 8, 0, DXGI_FORMAT_BC1_UNORM, U_IMM, B_SRV, 0, 0, OKAY, 0)
C3D(imm_rt, 8, 8, 8, 1, F_RGBA8, U_IMM, B_RT, 0, 0, BAD, 0)
C3D(imm_nobind, 8, 8, 8, 1, F_RGBA8, U_IMM, 0, 0, 0, BAD, 0)
C3D(imm_cpuw, 8, 8, 8, 1, F_RGBA8, U_IMM, B_SRV, CPU_W, 0, BAD, 0)
C3D(usage_5, 8, 8, 8, 1, F_RGBA8, (D3D11_USAGE)5, B_SRV, 0, 0, BAD, 0)
C3D(fmt_unknown, 8, 8, 8, 1, DXGI_FORMAT_UNKNOWN, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(fmt_bad, 8, 8, 8, 1, (DXGI_FORMAT)0xFFFF, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(srv_r32g32b32a32_typeless, 8, 8, 8, 1, DXGI_FORMAT_R32G32B32A32_TYPELESS, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(srv_r16g16b16a16_typeless, 8, 8, 8, 1, DXGI_FORMAT_R16G16B16A16_TYPELESS, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(srv_r10g10b10a2_typeless, 8, 8, 8, 1, DXGI_FORMAT_R10G10B10A2_TYPELESS, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(srv_r8g8b8a8_typeless, 8, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(srv_r32_typeless, 8, 8, 8, 1, DXGI_FORMAT_R32_TYPELESS, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(srv_r16_typeless, 8, 8, 8, 1, DXGI_FORMAT_R16_TYPELESS, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(srv_r8_typeless, 8, 8, 8, 1, DXGI_FORMAT_R8_TYPELESS, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(rt_r32g32b32a32_float, 8, 8, 8, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r16g16b16a16_float, 8, 8, 8, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r16g16b16a16_unorm, 8, 8, 8, 1, DXGI_FORMAT_R16G16B16A16_UNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r10g10b10a2_unorm, 8, 8, 8, 1, DXGI_FORMAT_R10G10B10A2_UNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r11g11b10_float, 8, 8, 8, 1, DXGI_FORMAT_R11G11B10_FLOAT, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r8g8b8a8_unorm, 8, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r8g8b8a8_unorm_srgb, 8, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r8g8b8a8_uint, 8, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_UINT, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r8g8b8a8_snorm, 8, 8, 8, 1, DXGI_FORMAT_R8G8B8A8_SNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r16g16_float, 8, 8, 8, 1, DXGI_FORMAT_R16G16_FLOAT, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r32_float, 8, 8, 8, 1, DXGI_FORMAT_R32_FLOAT, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r32_uint, 8, 8, 8, 1, DXGI_FORMAT_R32_UINT, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r8g8_unorm, 8, 8, 8, 1, DXGI_FORMAT_R8G8_UNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r16_unorm, 8, 8, 8, 1, DXGI_FORMAT_R16_UNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_r8_unorm, 8, 8, 8, 1, DXGI_FORMAT_R8_UNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_a8_unorm, 8, 8, 8, 1, DXGI_FORMAT_A8_UNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(rt_b8g8r8a8_unorm, 8, 8, 8, 1, DXGI_FORMAT_B8G8R8A8_UNORM, U_DEF, B_SRV|B_RT, 0, 0, OKAY, 0)
C3D(fmt_r9g9b9e5_srv, 8, 8, 8, 1, DXGI_FORMAT_R9G9B9E5_SHAREDEXP, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(fmt_r9g9b9e5_rt, 8, 8, 8, 1, DXGI_FORMAT_R9G9B9E5_SHAREDEXP, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(fmt_d24s8_rt, 8, 8, 8, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(fmt_d32_rt, 8, 8, 8, 1, DXGI_FORMAT_D32_FLOAT, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(fmt_d32_srv, 8, 8, 8, 1, DXGI_FORMAT_D32_FLOAT, U_DEF, B_SRV, 0, 0, BAD, 0)
C3D(bc_srv_bc1_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC1_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_chain_bc1_unorm, 16, 16, 4, 0, DXGI_FORMAT_BC1_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_rt_bc1_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC1_UNORM, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(bc_srv_bc2_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC2_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_chain_bc2_unorm, 16, 16, 4, 0, DXGI_FORMAT_BC2_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_rt_bc2_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC2_UNORM, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(bc_srv_bc3_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC3_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_chain_bc3_unorm, 16, 16, 4, 0, DXGI_FORMAT_BC3_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_rt_bc3_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC3_UNORM, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(bc_srv_bc4_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC4_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_chain_bc4_unorm, 16, 16, 4, 0, DXGI_FORMAT_BC4_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_rt_bc4_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC4_UNORM, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(bc_srv_bc5_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC5_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_chain_bc5_unorm, 16, 16, 4, 0, DXGI_FORMAT_BC5_UNORM, U_DEF, B_SRV, 0, 0, OKAY, 0)
C3D(bc_rt_bc5_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC5_UNORM, U_DEF, B_RT, 0, 0, BAD, 0)
C3D(bc_srv_bc6h_uf16, 8, 8, 4, 1, DXGI_FORMAT_BC6H_UF16, U_DEF, B_SRV, 0, 0, OKAY, FL_11_0)
C3D(bc_srv_bc7_unorm, 8, 8, 4, 1, DXGI_FORMAT_BC7_UNORM, U_DEF, B_SRV, 0, 0, OKAY, FL_11_0)
C3D(gen_rt_srv, 8, 8, 8, 0, F_RGBA8, U_DEF, B_RT|B_SRV, 0, M_GEN, OKAY, 0)
C3D(gen_srv_only, 8, 8, 8, 0, F_RGBA8, U_DEF, B_SRV, 0, M_GEN, BAD, 0)
C3D(gen_rt_only, 8, 8, 8, 0, F_RGBA8, U_DEF, B_RT, 0, M_GEN, BAD, 0)
C3D(cube_misc, 8, 8, 8, 1, F_RGBA8, U_DEF, B_SRV, 0, M_CUBE, BAD, 0)
C3D(misc_undef, 8, 8, 8, 1, F_RGBA8, U_DEF, B_SRV, 0, 0x40000000u, BAD, 0)
C3D(misc_shared_keyed, 8, 8, 8, 1, F_RGBA8, U_DEF, B_SRV, 0, M_SHR|M_KEYED, BAD, 0)

/* ---- argument validation shared by all three creators ------------------------------------------------- */
DEVTEST(d3d11_textures_null_desc_1d) {
    ID3D11Texture1D *t = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture1D(c->dev, NULL, NULL, &t), E_INVALIDARG);
    CHECK(t == NULL);
}
DEVTEST(d3d11_textures_null_desc_2d) {
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture2D(c->dev, NULL, NULL, &t), E_INVALIDARG);
    CHECK(t == NULL);
}
DEVTEST(d3d11_textures_null_desc_3d) {
    ID3D11Texture3D *t = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture3D(c->dev, NULL, NULL, &t), E_INVALIDARG);
    CHECK(t == NULL);
}
DEVTEST(d3d11_textures_null_desc_null_out) {
    EXPECT_HR(ID3D11Device_CreateTexture2D(c->dev, NULL, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device_CreateTexture1D(c->dev, NULL, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device_CreateTexture3D(c->dev, NULL, NULL, NULL), E_INVALIDARG);
}
/* Initial data whose pSysMem is NULL is rejected for every usage when a description is otherwise valid. */
DEVTEST(d3d11_textures_init_null_sysmem_1d) {
    D3D11_SUBRESOURCE_DATA sd = {0}; ID3D11Texture1D *t = NULL;
    EXPECT_HR(tx_mk1d(c, 512, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, &sd, &t), E_INVALIDARG);
    CHECK(t == NULL); SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_init_null_sysmem_2d) {
    D3D11_SUBRESOURCE_DATA sd = {0}; ID3D11Texture2D *t = NULL;
    EXPECT_HR(tx_mk2d(c, 512, 512, 1, 1, F_RGBA8, U_DEF, B_RT, 0, 0, &sd, &t), E_INVALIDARG);
    CHECK(t == NULL); SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_init_null_sysmem_3d) {
    D3D11_SUBRESOURCE_DATA sd = {0}; ID3D11Texture3D *t = NULL;
    EXPECT_HR(tx_mk3d(c, 64, 64, 64, 1, F_RGBA8, U_DEF, B_RT, 0, 0, &sd, &t), E_INVALIDARG);
    CHECK(t == NULL); SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_init_null_sysmem_immutable) {
    D3D11_SUBRESOURCE_DATA sd = {0}; ID3D11Texture2D *t = NULL;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, &sd, &t), E_INVALIDARG);
    SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_immutable_needs_data_1d) {
    ID3D11Texture1D *t = NULL;
    EXPECT_HR(tx_mk1d(c, 16, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, NULL, &t), E_INVALIDARG);
    SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_immutable_needs_data_2d) {
    ID3D11Texture2D *t = NULL;
    EXPECT_HR(tx_mk2d(c, 16, 16, 1, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, NULL, &t), E_INVALIDARG);
    SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_immutable_needs_data_3d) {
    ID3D11Texture3D *t = NULL;
    EXPECT_HR(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_IMM, B_SRV, 0, 0, NULL, &t), E_INVALIDARG);
    SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_immutable_needs_data_validate_only) {
    D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
    d.Width = d.Height = 8; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 1; d.Usage = U_IMM; d.BindFlags = B_SRV;
    EXPECT_HR(ID3D11Device_CreateTexture2D(c->dev, &d, NULL, NULL), E_INVALIDARG);
}
DEVTEST(d3d11_textures_msaa_rejects_initial_data) {
    tx_init in; ID3D11Texture2D *t = NULL; D3D11_TEXTURE2D_DESC d; UINT q = 0; HRESULT hr;
    hr = ID3D11Device_CheckMultisampleQualityLevels(c->dev, F_RGBA8, 4, &q);
    if (FAILED(hr) || !q) { t_skip("4x MSAA unsupported"); return; }
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 8; d.MipLevels = d.ArraySize = 1; d.Format = F_RGBA8; d.SampleDesc.Count = 4; d.Usage = U_DEF; d.BindFlags = B_RT;
    CHECK(tx_init_make(&in, 8, 8, 1, 1, 1, F_RGBA8, 0, 0, 1));
    EXPECT_HR(ID3D11Device_CreateTexture2D(c->dev, &d, in.sd, &t), E_INVALIDARG);
    SAFE_RELEASE(t);
    tx_init_free(&in);
}
DEVTEST(d3d11_textures_refcount_lifecycle) {
    ID3D11Texture2D *t = NULL; ULONG r;
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (!t) return;
    r = ID3D11Texture2D_AddRef(t); EXPECT_EQ(r, 2);
    r = ID3D11Texture2D_Release(t); EXPECT_EQ(r, 1);
    r = ID3D11Texture2D_Release(t); EXPECT_EQ(r, 0);
}
DEVTEST(d3d11_textures_device_refcount_grows) {
    ID3D11Texture2D *t = NULL; ULONG before, during, after;
    before = d3dh_refcount(c->dev);
    EXPECT_HR(tx_mk2d(c, 8, 8, 1, 1, F_RGBA8, U_DEF, B_SRV, 0, 0, NULL, &t), S_OK);
    if (!t) return;
    during = d3dh_refcount(c->dev);
    CHECKF(during >= before + 1, "device refcount %lu -> %lu", before, during);
    ID3D11Texture2D_Release(t);
    after = d3dh_refcount(c->dev);
    CHECKF(after <= during, "device refcount did not drop: %lu -> %lu", during, after);
}
/* Observations that depend on runtime/driver version. */
DEVTEST(d3d11_textures_note_cube_array_sizes) {
    static const UINT sizes[] = {6, 7, 8, 11, 12, 13, 18};
    for (UINT i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        ID3D11Texture2D *t = NULL; HRESULT hr = tx_mk2d(c, 16, 16, 1, sizes[i], F_RGBA8, U_DEF, B_SRV, 0, M_CUBE, NULL, &t);
        t_note("TEXTURECUBE ArraySize %u -> %s", sizes[i], d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
}
DEVTEST(d3d11_textures_note_dynamic_arrays_and_mips) {
    ID3D11Texture2D *t = NULL; HRESULT hr;
    hr = tx_mk2d(c, 16, 16, 1, 2, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, NULL, &t); t_note("dynamic array 2 -> %s", d3dh_hr_name(hr)); SAFE_RELEASE(t);
    hr = tx_mk2d(c, 16, 16, 3, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, NULL, &t); t_note("dynamic 3 mips -> %s", d3dh_hr_name(hr)); SAFE_RELEASE(t);
    hr = tx_mk2d(c, 16, 16, 0, 1, F_RGBA8, U_DYN, B_SRV, CPU_W, 0, NULL, &t); t_note("dynamic full chain -> %s", d3dh_hr_name(hr)); SAFE_RELEASE(t);
}
DEVTEST(d3d11_textures_note_bc_nonmultiple_of_4) {
    static const UINT sz[][2] = {{1,1},{2,2},{3,3},{5,5},{6,10},{7,4}};
    for (UINT i = 0; i < sizeof sz / sizeof *sz; i++) {
        ID3D11Texture2D *t = NULL; HRESULT hr = tx_mk2d(c, sz[i][0], sz[i][1], 1, 1, DXGI_FORMAT_BC1_UNORM, U_DEF, B_SRV, 0, 0, NULL, &t);
        t_note("BC1 %ux%u -> %s", sz[i][0], sz[i][1], d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
}
DEVTEST(d3d11_textures_note_default_nobind_1d_3d) {
    ID3D11Texture1D *t1 = NULL; ID3D11Texture3D *t3 = NULL;
    t_note("1D default bind 0 -> %s", d3dh_hr_name(tx_mk1d(c, 8, 1, 1, F_RGBA8, U_DEF, 0, 0, 0, NULL, &t1))); SAFE_RELEASE(t1);
    t_note("3D default bind 0 -> %s", d3dh_hr_name(tx_mk3d(c, 4, 4, 4, 1, F_RGBA8, U_DEF, 0, 0, 0, NULL, &t3))); SAFE_RELEASE(t3);
}
