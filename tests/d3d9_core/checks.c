/* IDirect3D9 CheckDevice* family: argument validation (exact HRESULTs) and format/usage matrices. */
#include "d9c.h"

#define HAL  D3DDEVTYPE_HAL
#define REF  D3DDEVTYPE_REF
#define SW   D3DDEVTYPE_SW
#define NREF D3DDEVTYPE_NULLREF

static const D3DFORMAT k_dispfmts[] = { D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A2R10G10B10 };
static const D3DFORMAT k_bbfmts[] = { D3DFMT_UNKNOWN, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_A2R10G10B10, D3DFMT_A16B16G16R16F };
static const D3DFORMAT k_notbb[] = { D3DFMT_DXT1, D3DFMT_D16, D3DFMT_D24S8, D3DFMT_L8, D3DFMT_V8U8, D3DFMT_INDEX16, D3DFMT_VERTEXDATA, D3DFMT_R16F };
#define N(a) (sizeof(a) / sizeof(*(a)))
static const D3DDEVTYPE k_baddev[] = { (D3DDEVTYPE)0, (D3DDEVTYPE)5, (D3DDEVTYPE)6, (D3DDEVTYPE)100, (D3DDEVTYPE)0xFFFFFFFF };

/* ================= CheckDeviceType ================= */
TEST(d3d9_core_cdt_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    UINT bad[] = { n, n + 1, 0xFFFFFFFF };
    for (unsigned i = 0; i < 3; i++) {
        EXPECT_HR(IDirect3D9_CheckDeviceType(d, bad[i], HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D9_INVALIDCALL);
        EXPECT_HR(IDirect3D9_CheckDeviceType(d, bad[i], HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, FALSE), D9_INVALIDCALL);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_bad_devtype) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    /* Only HAL, REF and SW are accepted; NULLREF is a CreateDevice-only type and is rejected here. */
    for (unsigned i = 0; i < N(k_baddev); i++)
        EXPECT_HR(IDirect3D9_CheckDeviceType(d, 0, k_baddev[i], D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceType(d, 0, NREF, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceType(d, 0, NREF, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, FALSE), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_devtype_checked_after_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    EXPECT_HR(IDirect3D9_CheckDeviceType(d, n, (D3DDEVTYPE)99, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_unknown_adapter_format) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (int w = 0; w < 2; w++)
        for (unsigned b = 0; b < N(k_bbfmts); b++)
            EXPECT_HR(IDirect3D9_CheckDeviceType(d, 0, HAL, D3DFMT_UNKNOWN, k_bbfmts[b], w), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_unknown_backbuffer_fullscreen) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned a = 0; a < N(k_dispfmts); a++) {
        EXPECT_HR(IDirect3D9_CheckDeviceType(d, 0, HAL, k_dispfmts[a], D3DFMT_UNKNOWN, FALSE), D9_INVALIDCALL);
        EXPECT_HR(IDirect3D9_CheckDeviceType(d, 0, REF, k_dispfmts[a], D3DFMT_UNKNOWN, FALSE), D9_INVALIDCALL);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_unknown_backbuffer_windowed_equals_adapter_format) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE tys[] = { HAL, REF, SW };
    for (unsigned t = 0; t < 3; t++)
        for (unsigned a = 0; a < N(k_dispfmts); a++) {
            HRESULT h1 = IDirect3D9_CheckDeviceType(d, 0, tys[t], k_dispfmts[a], D3DFMT_UNKNOWN, TRUE);
            HRESULT h2 = IDirect3D9_CheckDeviceType(d, 0, tys[t], k_dispfmts[a], k_dispfmts[a], TRUE);
            CHECKF(h1 == h2, "dev %u fmt %u: unknown-bb %s same-bb %s", tys[t], k_dispfmts[a], d3dh_hr_name(h1), d3dh_hr_name(h2));
        }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_nondisplay_adapter_format_notavailable) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned i = 0; i < N(k_notbb); i++)
        for (int w = 0; w < 2; w++)
            CHECKF(IDirect3D9_CheckDeviceType(d, 0, HAL, k_notbb[i], D3DFMT_X8R8G8B8, w) == D9_NOTAVAILABLE, "adapter fmt 0x%x windowed %d", k_notbb[i], w);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_nonbackbuffer_format_notavailable) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned i = 0; i < N(k_notbb); i++)
        for (int w = 0; w < 2; w++) {
            HRESULT hr = IDirect3D9_CheckDeviceType(d, 0, HAL, D3DFMT_X8R8G8B8, k_notbb[i], w);
            CHECKF(hr == D9_NOTAVAILABLE, "bb fmt 0x%x windowed %d -> %s", k_notbb[i], w, d3dh_hr_name(hr));
        }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_result_space) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE tys[] = { HAL, REF, SW };
    for (unsigned t = 0; t < 3; t++)
        for (unsigned a = 0; a < N(k_dispfmts); a++)
            for (unsigned b = 0; b < N(k_bbfmts); b++)
                for (int w = 0; w < 2; w++) {
                    if (k_bbfmts[b] == D3DFMT_UNKNOWN && !w) continue;
                    HRESULT hr = IDirect3D9_CheckDeviceType(d, 0, tys[t], k_dispfmts[a], k_bbfmts[b], w);
                    if (tys[t] == HAL) CHECKF(hr == S_OK || hr == D9_NOTAVAILABLE, "dev %u adapter %u bb %u w %d -> %s", tys[t], k_dispfmts[a], k_bbfmts[b], w, d3dh_hr_name(hr));
                }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_matrix_hal_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (int w = 0; w < 2; w++)
        for (unsigned a = 0; a < N(k_dispfmts); a++) {
            char line[256]; int p = 0;
            for (unsigned b = 0; b < N(k_bbfmts); b++) {
                HRESULT hr = IDirect3D9_CheckDeviceType(d, 0, HAL, k_dispfmts[a], k_bbfmts[b], w);
                p += snprintf(line + p, sizeof line - p, " %u:%c", k_bbfmts[b], hr == S_OK ? 'Y' : (hr == D9_NOTAVAILABLE ? 'n' : '?'));
            }
            t_note("HAL windowed=%d adapter fmt %u backbuffers%s", w, k_dispfmts[a], line);
        }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_matrix_ref_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    HRESULT hr = IDirect3D9_CheckDeviceType(d, 0, REF, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE);
    t_note("REF X8R8G8B8/X8R8G8B8 windowed: %s (REF rasteriser ships only with the SDK)", d3dh_hr_name(hr));
    hr = IDirect3D9_CheckDeviceType(d, 0, SW, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE);
    t_note("SW X8R8G8B8/X8R8G8B8 windowed (no software device registered): %s", d3dh_hr_name(hr));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_hal_x8r8g8b8_windowed_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    HRESULT hr = IDirect3D9_CheckDeviceType(d, 0, HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE);
    t_note("HAL X8R8G8B8/X8R8G8B8 windowed -> %s", d3dh_hr_name(hr));
    if (hr == S_OK) {
        /* A HAL that accepts X8R8G8B8 as adapter format accepts the alpha variant as windowed back buffer too, or says NOTAVAILABLE. */
        hr = IDirect3D9_CheckDeviceType(d, 0, HAL, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, TRUE);
        CHECKF(hr == S_OK || hr == D9_NOTAVAILABLE, "A8R8G8B8 bb -> %s", d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdt_plain_vs_ex) {
    D9_GET(a); D9_GET_EX(b);
    if (!IDirect3D9_GetAdapterCount(a)) { IDirect3D9_Release(a); IDirect3D9Ex_Release(b); t_skip("no adapters"); return; }
    HRESULT h1 = IDirect3D9_CheckDeviceType(a, 0, HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE);
    HRESULT h2 = IDirect3D9Ex_CheckDeviceType(b, 0, HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE);
    CHECKF(h1 == h2, "plain %s ex %s", d3dh_hr_name(h1), d3dh_hr_name(h2));
    IDirect3D9_Release(a); IDirect3D9Ex_Release(b);
}
TEST(d3d9_core_cdt_ex_accepts_extended_display_formats_note) {
    D9_GET_EX(d);
    if (!IDirect3D9Ex_GetAdapterCount(d)) { IDirect3D9Ex_Release(d); t_skip("no adapters"); return; }
    /* Ex objects additionally treat A16B16G16R16F (0x71) and A2B10G10R10_XR_BIAS (0x77) as display formats. */
    HRESULT h1 = IDirect3D9Ex_CheckDeviceType(d, 0, HAL, (D3DFORMAT)0x71, (D3DFORMAT)0x71, TRUE);
    HRESULT h2 = IDirect3D9Ex_CheckDeviceType(d, 0, HAL, (D3DFORMAT)0x77, (D3DFORMAT)0x77, TRUE);
    t_note("Ex CheckDeviceType A16B16G16R16F: %s, A2B10G10R10_XR_BIAS: %s", d3dh_hr_name(h1), d3dh_hr_name(h2));
    D9_GET(p);
    h1 = IDirect3D9_CheckDeviceType(p, 0, HAL, (D3DFORMAT)0x71, (D3DFORMAT)0x71, TRUE);
    CHECKF(h1 == D9_NOTAVAILABLE, "plain object with FP16 display format -> %s", d3dh_hr_name(h1));
    IDirect3D9_Release(p);
    IDirect3D9Ex_Release(d);
}

/* ================= CheckDeviceFormat ================= */
static D3DDEVTYPE hal_or_skip(IDirect3D9 *d) { return d9_have_devtype(d, HAL) ? HAL : (D3DDEVTYPE)0; }

TEST(d3d9_core_cdf_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    UINT bad[] = { n, n + 1, 0xFFFFFFFF };
    for (unsigned i = 0; i < 3; i++) {
        if (n == 0 && bad[i] == 0) continue;
        EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, bad[i], HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    }
    if (n) EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, n, HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_bad_devtype) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE bad[] = { (D3DDEVTYPE)0, (D3DDEVTYPE)5, (D3DDEVTYPE)6, (D3DDEVTYPE)0xFFFFFFFF };
    for (unsigned i = 0; i < N(bad); i++)
        EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, bad[i], D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_unknown_formats) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_UNKNOWN, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_UNKNOWN, 0, D3DRTYPE_TEXTURE, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, REF, D3DFMT_UNKNOWN, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_nullref_allows_unknown_adapter_format) {
    /* D3DDEVTYPE_NULLREF is the one type for which AdapterFormat may be UNKNOWN (past argument validation). */
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, NREF, D3DFMT_UNKNOWN, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8);
    t_note("NULLREF with UNKNOWN adapter format -> %s", d3dh_hr_name(hr));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_nondisplay_adapter_format_notavailable) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (unsigned i = 0; i < N(k_notbb); i++) {
        HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, HAL, k_notbb[i], 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8);
        CHECKF(hr == D9_NOTAVAILABLE, "adapter fmt 0x%x -> %s", k_notbb[i], d3dh_hr_name(hr));
        hr = IDirect3D9_CheckDeviceFormat(d, 0, REF, k_notbb[i], 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8);
        CHECKF(hr == D9_NOTAVAILABLE, "REF adapter fmt 0x%x -> %s", k_notbb[i], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_invalid_resource_types) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    /* VERTEXBUFFER/INDEXBUFFER and out-of-range values are not formattable resource types. */
    D3DRESOURCETYPE bad[] = { (D3DRESOURCETYPE)0, D3DRTYPE_VERTEXBUFFER, D3DRTYPE_INDEXBUFFER, (D3DRESOURCETYPE)8, (D3DRESOURCETYPE)100, (D3DRESOURCETYPE)0x7FFFFFFF };
    for (unsigned i = 0; i < N(bad); i++)
        CHECKF(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_X8R8G8B8, 0, bad[i], D3DFMT_A8R8G8B8) == D9_INVALIDCALL, "rtype %u", bad[i]);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_invalid_usage_bits) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    DWORD bad[] = { 0x4, 0x400000, 0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x40000000, 0x80000000u, 0xFFFFFFFFu, 0x4 | D3DUSAGE_RENDERTARGET };
    for (unsigned i = 0; i < N(bad); i++)
        CHECKF(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_X8R8G8B8, bad[i], D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8) == D9_INVALIDCALL, "usage 0x%x", bad[i]);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_volume_rt_ds_usage_invalid) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DRESOURCETYPE vols[] = { D3DRTYPE_VOLUME, D3DRTYPE_VOLUMETEXTURE };
    DWORD us[] = { D3DUSAGE_RENDERTARGET, D3DUSAGE_DEPTHSTENCIL, D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL };
    for (unsigned r = 0; r < 2; r++) for (unsigned u = 0; u < 3; u++)
        CHECKF(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_X8R8G8B8, us[u], vols[r], D3DFMT_A8R8G8B8) == D9_INVALIDCALL, "rtype %u usage 0x%x", vols[r], us[u]);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_volume_autogenmipmap_invalid) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_VOLUME, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_VOLUMETEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_query_vertextexture_needs_texture_type) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_VOLUME, D3DFMT_A32B32G32R32F), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_depthstencil_usage_needs_z_format) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DFORMAT nz[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_DXT1, D3DFMT_L8, D3DFMT_V8U8, D3DFMT_A16B16G16R16F };
    D3DRESOURCETYPE rt[] = { D3DRTYPE_TEXTURE, D3DRTYPE_CUBETEXTURE };
    for (unsigned r = 0; r < 2; r++) for (unsigned i = 0; i < N(nz); i++)
        CHECKF(IDirect3D9_CheckDeviceFormat(d, 0, HAL, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, rt[r], nz[i]) == D9_INVALIDCALL, "rtype %u fmt 0x%x", rt[r], nz[i]);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_dmap_needs_texture_type) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, D3DUSAGE_DMAP, D3DRTYPE_CUBETEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, D3DUSAGE_DMAP, D3DRTYPE_VOLUMETEXTURE, D3DFMT_A8R8G8B8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_result_space_matrix) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    static const D3DFORMAT fm[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A1R5G5B5, D3DFMT_A4R4G4B4, D3DFMT_A2R10G10B10, D3DFMT_A16B16G16R16, D3DFMT_A16B16G16R16F,
        D3DFMT_A32B32G32R32F, D3DFMT_R16F, D3DFMT_R32F, D3DFMT_G16R16F, D3DFMT_G32R32F, D3DFMT_L8, D3DFMT_A8, D3DFMT_V8U8, D3DFMT_Q8W8V8U8, D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5, D3DFMT_D16, D3DFMT_D24S8, D3DFMT_D24X8, D3DFMT_D32, D3DFMT_D16_LOCKABLE, D3DFMT_D32F_LOCKABLE, D3DFMT_UYVY, D3DFMT_YUY2, D3DFMT_A8B8G8R8 };
    static const DWORD us[] = { 0, D3DUSAGE_RENDERTARGET, D3DUSAGE_DEPTHSTENCIL, D3DUSAGE_DYNAMIC, D3DUSAGE_AUTOGENMIPMAP, D3DUSAGE_QUERY_FILTER, D3DUSAGE_QUERY_LEGACYBUMPMAP, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DUSAGE_QUERY_SRGBREAD, D3DUSAGE_QUERY_SRGBWRITE, D3DUSAGE_QUERY_VERTEXTEXTURE, D3DUSAGE_QUERY_WRAPANDMIP };
    static const D3DRESOURCETYPE rts[] = { D3DRTYPE_SURFACE, D3DRTYPE_TEXTURE, D3DRTYPE_VOLUMETEXTURE, D3DRTYPE_CUBETEXTURE };
    for (unsigned r = 0; r < N(rts); r++) {
        for (unsigned f = 0; f < N(fm); f++) {
            char line[200]; int p = 0;
            for (unsigned u = 0; u < N(us); u++) {
                HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, us[u], rts[r], fm[f]);
                CHECKF(hr == S_OK || hr == D9_NOTAVAILABLE || hr == D9_NOAUTOGEN || hr == D9_INVALIDCALL,
                       "rtype %u fmt 0x%x usage 0x%x -> %s", rts[r], fm[f], us[u], d3dh_hr_name(hr));
                if (hr == D9_NOAUTOGEN) CHECK(us[u] & D3DUSAGE_AUTOGENMIPMAP);
                p += snprintf(line + p, sizeof line - p, "%c", hr == S_OK ? 'Y' : hr == D9_NOAUTOGEN ? 'a' : hr == D9_NOTAVAILABLE ? 'n' : 'x');
            }
            t_note("CDF rtype=%u fmt=0x%x usages[0,RT,DS,DYN,AUTOGEN,FILTER,BUMP,POSTPS,SRGBR,SRGBW,VTX,WRAP]=%s", rts[r], fm[f], line);
        }
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_basic_hal_requirements) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t || t_is_wine()) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    /* Formats every D3D9 HAL on Windows 10 (WDDM 1.x+) exposes. */
    CHECK(IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8) == S_OK);
    CHECK(IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_X8R8G8B8) == S_OK);
    CHECK(IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8) == S_OK);
    CHECK(IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, D3DFMT_X8R8G8B8) == S_OK);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_surface_zero_usage_offscreen_note) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    D3DFORMAT f[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_DXT1, D3DFMT_D24S8, D3DFMT_L8 };
    for (unsigned i = 0; i < N(f); i++)
        t_note("offscreen plain surface fmt 0x%x: %s", f[i], d3dh_hr_name(IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE, f[i])));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_depth_formats_note) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    D3DFORMAT z[] = { D3DFMT_D16, D3DFMT_D15S1, D3DFMT_D24X8, D3DFMT_D24S8, D3DFMT_D24X4S4, D3DFMT_D32, D3DFMT_D16_LOCKABLE, D3DFMT_D32F_LOCKABLE, D3DFMT_D24FS8, D3DFMT_D32_LOCKABLE, D3DFMT_S8_LOCKABLE };
    for (unsigned i = 0; i < N(z); i++) {
        HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, z[i]);
        CHECKF(hr == S_OK || hr == D9_NOTAVAILABLE, "Z fmt %u -> %s", z[i], d3dh_hr_name(hr));
        t_note("depth/stencil format %u: %s", z[i], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_adapter_format_independent_of_back_buffer_note) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    for (unsigned a = 0; a < N(k_dispfmts); a++)
        t_note("adapter fmt %u: A8R8G8B8 RT texture -> %s", k_dispfmts[a], d3dh_hr_name(IDirect3D9_CheckDeviceFormat(d, 0, t, k_dispfmts[a], D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8)));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_autogenmipmap_noautogen_code_note) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    HRESULT hr = IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8);
    CHECKF(hr == S_OK || hr == D9_NOAUTOGEN || hr == D9_NOTAVAILABLE, "-> %s", d3dh_hr_name(hr));
    t_note("A8R8G8B8 AUTOGENMIPMAP texture: %s (D3DOK_NOAUTOGEN is a success code: %d)", d3dh_hr_name(hr), SUCCEEDED(D9_NOAUTOGEN));
    CHECK(SUCCEEDED(D9_NOAUTOGEN));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdf_plain_vs_ex) {
    D9_GET(a); D9_GET_EX(b);
    if (!IDirect3D9_GetAdapterCount(a)) { IDirect3D9_Release(a); IDirect3D9Ex_Release(b); t_skip("no adapters"); return; }
    D3DFORMAT f[] = { D3DFMT_A8R8G8B8, D3DFMT_DXT1, D3DFMT_D24S8, D3DFMT_A16B16G16R16F };
    for (unsigned i = 0; i < N(f); i++) {
        HRESULT h1 = IDirect3D9_CheckDeviceFormat(a, 0, HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, f[i]);
        HRESULT h2 = IDirect3D9Ex_CheckDeviceFormat(b, 0, HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, f[i]);
        CHECKF(h1 == h2, "fmt 0x%x plain %s ex %s", f[i], d3dh_hr_name(h1), d3dh_hr_name(h2));
    }
    IDirect3D9_Release(a); IDirect3D9Ex_Release(b);
}

/* ================= CheckDeviceMultiSampleType ================= */
TEST(d3d9_core_cdms_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    DWORD q = 77;
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d, n, HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, &q), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d, 0xFFFFFFFF, HAL, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_4_SAMPLES, &q), D9_INVALIDCALL);
    EXPECT_EQ(q, 77);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdms_bad_devtype) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE bad[] = { (D3DDEVTYPE)0, (D3DDEVTYPE)5, (D3DDEVTYPE)6, (D3DDEVTYPE)0xFFFFFFFF };
    for (unsigned i = 0; i < N(bad); i++) {
        DWORD q = 77;
        EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d, 0, bad[i], D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, &q), D9_INVALIDCALL);
        EXPECT_EQ(q, 77);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdms_unknown_format) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    DWORD q = 77;
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d, 0, HAL, D3DFMT_UNKNOWN, TRUE, D3DMULTISAMPLE_NONE, &q), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d, 0, HAL, D3DFMT_UNKNOWN, FALSE, D3DMULTISAMPLE_4_SAMPLES, NULL), D9_INVALIDCALL);
    EXPECT_EQ(q, 77);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdms_none_always_ok_with_one_level) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    D3DFORMAT f[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_D24S8, D3DFMT_D16 };
    for (unsigned i = 0; i < N(f); i++) for (int w = 0; w < 2; w++) {
        DWORD q = 77;
        HRESULT hr = IDirect3D9_CheckDeviceMultiSampleType(d, 0, t, f[i], w, D3DMULTISAMPLE_NONE, &q);
        CHECKF(hr == S_OK, "fmt %u w %d -> %s", f[i], w, d3dh_hr_name(hr));
        CHECKF(q == 1, "fmt %u w %d quality %lu", f[i], w, q);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdms_null_quality_pointer) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    EXPECT_HR(IDirect3D9_CheckDeviceMultiSampleType(d, 0, t, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, NULL), S_OK);
    HRESULT hr = IDirect3D9_CheckDeviceMultiSampleType(d, 0, t, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_4_SAMPLES, NULL);
    CHECKF(hr == S_OK || hr == D9_NOTAVAILABLE, "4x null quality -> %s", d3dh_hr_name(hr));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdms_invalid_sample_type) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    UINT bad[] = { 17, 18, 32, 100, 0xFFFFFFFFu };
    for (unsigned i = 0; i < N(bad); i++) {
        DWORD q = 77;
        HRESULT hr = IDirect3D9_CheckDeviceMultiSampleType(d, 0, t, D3DFMT_X8R8G8B8, TRUE, (D3DMULTISAMPLE_TYPE)bad[i], &q);
        CHECKF(hr == D9_INVALIDCALL, "type %u -> %s", bad[i], d3dh_hr_name(hr));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdms_sample_matrix_note) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    D3DFORMAT f[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A2R10G10B10, D3DFMT_A16B16G16R16F, D3DFMT_D16, D3DFMT_D24S8, D3DFMT_D32 };
    for (unsigned i = 0; i < N(f); i++) for (int w = 0; w < 2; w++) {
        char line[200]; int p = 0;
        for (UINT s = 1; s <= 16; s++) {
            DWORD q = 0;
            HRESULT hr = IDirect3D9_CheckDeviceMultiSampleType(d, 0, t, f[i], w, (D3DMULTISAMPLE_TYPE)s, &q);
            CHECKF(hr == S_OK || hr == D9_NOTAVAILABLE, "fmt %u w %d samples %u -> %s", f[i], w, s, d3dh_hr_name(hr));
            if (hr == S_OK) { CHECKF(q >= 1, "fmt %u samples %u quality %lu", f[i], s, q); p += snprintf(line + p, sizeof line - p, " %u:%lu", s, q); }
        }
        t_note("MS fmt %u windowed=%d supported(samples:quality)%s", f[i], w, line);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdms_nullref_accepted_devtype) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    DWORD q = 77;
    HRESULT hr = IDirect3D9_CheckDeviceMultiSampleType(d, 0, NREF, D3DFMT_X8R8G8B8, TRUE, D3DMULTISAMPLE_NONE, &q);
    t_note("NULLREF multisample NONE -> %s quality %lu", d3dh_hr_name(hr), q);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdms_plain_vs_ex) {
    D9_GET(a); D9_GET_EX(b);
    if (!IDirect3D9_GetAdapterCount(a)) { IDirect3D9_Release(a); IDirect3D9Ex_Release(b); t_skip("no adapters"); return; }
    for (UINT s = 0; s <= 8; s++) {
        DWORD q1 = 0, q2 = 0;
        HRESULT h1 = IDirect3D9_CheckDeviceMultiSampleType(a, 0, HAL, D3DFMT_X8R8G8B8, TRUE, (D3DMULTISAMPLE_TYPE)s, &q1);
        HRESULT h2 = IDirect3D9Ex_CheckDeviceMultiSampleType(b, 0, HAL, D3DFMT_X8R8G8B8, TRUE, (D3DMULTISAMPLE_TYPE)s, &q2);
        CHECKF(h1 == h2 && q1 == q2, "samples %u: %s/%lu vs %s/%lu", s, d3dh_hr_name(h1), q1, d3dh_hr_name(h2), q2);
    }
    IDirect3D9_Release(a); IDirect3D9Ex_Release(b);
}

/* ================= CheckDepthStencilMatch ================= */
TEST(d3d9_core_cdsm_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d, n, HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_D24S8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d, 0xFFFFFFFF, HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_D24S8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdsm_bad_devtype) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE bad[] = { (D3DDEVTYPE)0, NREF, (D3DDEVTYPE)5, (D3DDEVTYPE)0xFFFFFFFF };
    for (unsigned i = 0; i < N(bad); i++)
        EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d, 0, bad[i], D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_D24S8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdsm_unknown_formats) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d, 0, HAL, D3DFMT_UNKNOWN, D3DFMT_X8R8G8B8, D3DFMT_D24S8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d, 0, HAL, D3DFMT_X8R8G8B8, D3DFMT_UNKNOWN, D3DFMT_D24S8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d, 0, HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDepthStencilMatch(d, 0, HAL, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdsm_matrix_note) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    D3DFORMAT rt[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_A2R10G10B10, D3DFMT_A16B16G16R16F };
    D3DFORMAT ds[] = { D3DFMT_D16, D3DFMT_D24X8, D3DFMT_D24S8, D3DFMT_D32, D3DFMT_D16_LOCKABLE, D3DFMT_D32F_LOCKABLE, D3DFMT_D24FS8 };
    for (unsigned r = 0; r < N(rt); r++) {
        char line[200]; int p = 0;
        for (unsigned z = 0; z < N(ds); z++) {
            HRESULT hr = IDirect3D9_CheckDepthStencilMatch(d, 0, t, D3DFMT_X8R8G8B8, rt[r], ds[z]);
            p += snprintf(line + p, sizeof line - p, " %u:%c", ds[z], hr == S_OK ? 'Y' : hr == D9_NOTAVAILABLE ? 'n' : hr == D9_INVALIDCALL ? 'i' : '?');
        }
        t_note("DSMatch adapter X8R8G8B8 rt=%u ds%s", rt[r], line);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdsm_non_z_ds_format_fails) {
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    HRESULT hr = IDirect3D9_CheckDepthStencilMatch(d, 0, t, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8);
    t_note("color format used as depth format -> %s", d3dh_hr_name(hr));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdsm_consistent_with_cdf) {
    /* A match can only succeed if both formats were individually usable for their roles. */
    D9_GET(d);
    D3DDEVTYPE t = hal_or_skip(d);
    if (!t) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    D3DFORMAT ds[] = { D3DFMT_D16, D3DFMT_D24X8, D3DFMT_D24S8, D3DFMT_D32 };
    for (unsigned z = 0; z < N(ds); z++) {
        HRESULT m = IDirect3D9_CheckDepthStencilMatch(d, 0, t, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, ds[z]);
        if (m == S_OK) {
            HRESULT f = IDirect3D9_CheckDeviceFormat(d, 0, t, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, ds[z]);
            if (f != S_OK) t_note("ds %u match OK but CheckDeviceFormat -> %s", ds[z], d3dh_hr_name(f));
        }
    }
    IDirect3D9_Release(d);
}

/* ================= CheckDeviceFormatConversion ================= */
TEST(d3d9_core_cdfc_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, n, HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0xFFFFFFFF, HAL, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdfc_unknown_formats) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0, HAL, D3DFMT_UNKNOWN, D3DFMT_X8R8G8B8), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0, HAL, D3DFMT_X8R8G8B8, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0, HAL, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0, REF, D3DFMT_UNKNOWN, D3DFMT_X8R8G8B8), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdfc_unknown_checked_after_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, n, HAL, D3DFMT_UNKNOWN, D3DFMT_UNKNOWN), D9_INVALIDCALL);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdfc_matrix_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DFORMAT f[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A2R10G10B10, D3DFMT_DXT1, D3DFMT_D24S8 };
    for (unsigned s = 0; s < N(f); s++) {
        char line[200]; int p = 0;
        for (unsigned t = 0; t < N(f); t++) {
            HRESULT hr = IDirect3D9_CheckDeviceFormatConversion(d, 0, HAL, f[s], f[t]);
            CHECKF(hr == S_OK || hr == D9_NOTAVAILABLE || hr == D9_INVALIDCALL, "src %u dst %u -> %s", f[s], f[t], d3dh_hr_name(hr));
            p += snprintf(line + p, sizeof line - p, " %u:%c", f[t], hr == S_OK ? 'Y' : hr == D9_NOTAVAILABLE ? 'n' : 'i');
        }
        t_note("FormatConversion src=%u dst%s", f[s], line);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdfc_cached_result_stable) {
    /* Results are cached per (adapter, devtype, src, dst); repeated calls must agree. */
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    HRESULT first = IDirect3D9_CheckDeviceFormatConversion(d, 0, HAL, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8);
    for (int i = 0; i < 5; i++) EXPECT_HR(IDirect3D9_CheckDeviceFormatConversion(d, 0, HAL, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8), first);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_cdfc_plain_vs_ex) {
    D9_GET(a); D9_GET_EX(b);
    if (!IDirect3D9_GetAdapterCount(a)) { IDirect3D9_Release(a); IDirect3D9Ex_Release(b); t_skip("no adapters"); return; }
    HRESULT h1 = IDirect3D9_CheckDeviceFormatConversion(a, 0, HAL, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8);
    HRESULT h2 = IDirect3D9Ex_CheckDeviceFormatConversion(b, 0, HAL, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8);
    CHECKF(h1 == h2, "plain %s ex %s", d3dh_hr_name(h1), d3dh_hr_name(h2));
    IDirect3D9_Release(a); IDirect3D9Ex_Release(b);
}
