/* Capabilities of the D3D10 device: CheckFormatSupport, CheckMultisampleQualityLevels, counters, queries (creation matrix),
 * text filter size, format/resource-creation consistency. */
#include "dd.h"

#define T(n) TEST(d3d10_device_caps_##n)

#define LAST_FMT DXGI_FORMAT_B4G4R4A4_UNORM  /* 115 */

#define FS_BUFFER D3D10_FORMAT_SUPPORT_BUFFER
#define FS_VB D3D10_FORMAT_SUPPORT_IA_VERTEX_BUFFER
#define FS_IB D3D10_FORMAT_SUPPORT_IA_INDEX_BUFFER
#define FS_SO D3D10_FORMAT_SUPPORT_SO_BUFFER
#define FS_T1 D3D10_FORMAT_SUPPORT_TEXTURE1D
#define FS_T2 D3D10_FORMAT_SUPPORT_TEXTURE2D
#define FS_T3 D3D10_FORMAT_SUPPORT_TEXTURE3D
#define FS_TC D3D10_FORMAT_SUPPORT_TEXTURECUBE
#define FS_LOAD D3D10_FORMAT_SUPPORT_SHADER_LOAD
#define FS_SAMPLE D3D10_FORMAT_SUPPORT_SHADER_SAMPLE
#define FS_MIP D3D10_FORMAT_SUPPORT_MIP
#define FS_RT D3D10_FORMAT_SUPPORT_RENDER_TARGET
#define FS_BLEND D3D10_FORMAT_SUPPORT_BLENDABLE
#define FS_DS D3D10_FORMAT_SUPPORT_DEPTH_STENCIL
#define FS_DISPLAY D3D10_FORMAT_SUPPORT_DISPLAY
#define FS_MSRT D3D10_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET
#define FS_MSRES D3D10_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE

static UINT support_of(ID3D10Device *dev, DXGI_FORMAT f, HRESULT *hr_out)
{
    UINT s = 0xdeadbeef;
    HRESULT hr = ID3D10Device_CheckFormatSupport(dev, f, &s);
    if (hr_out) *hr_out = hr;
    return s;
}

/* ---- table driven: required / forbidden support bits ---- */
static const struct { DXGI_FORMAT f; UINT must; UINT mustnot; const char *name; } g_fmt[] = {
    { DXGI_FORMAT_R32G32B32A32_FLOAT, FS_T1|FS_T2|FS_T3|FS_TC|FS_LOAD|FS_RT|FS_VB|FS_MIP, FS_DS, "R32G32B32A32_FLOAT" },
    { DXGI_FORMAT_R32G32B32A32_UINT,  FS_T2|FS_LOAD|FS_RT|FS_VB, FS_SAMPLE|FS_DS|FS_BLEND, "R32G32B32A32_UINT" },
    { DXGI_FORMAT_R32G32B32A32_SINT,  FS_T2|FS_LOAD|FS_RT|FS_VB, FS_SAMPLE|FS_DS|FS_BLEND, "R32G32B32A32_SINT" },
    { DXGI_FORMAT_R32G32B32_FLOAT,    FS_VB|FS_LOAD, FS_DS, "R32G32B32_FLOAT" },
    { DXGI_FORMAT_R16G16B16A16_FLOAT, FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_VB|FS_MIP|FS_DISPLAY, FS_DS, "R16G16B16A16_FLOAT" },
    { DXGI_FORMAT_R16G16B16A16_UNORM, FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_VB, FS_DS, "R16G16B16A16_UNORM" },
    { DXGI_FORMAT_R16G16B16A16_UINT,  FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R16G16B16A16_UINT" },
    { DXGI_FORMAT_R16G16B16A16_SNORM, FS_T2|FS_SAMPLE|FS_RT|FS_VB, FS_DS, "R16G16B16A16_SNORM" },
    { DXGI_FORMAT_R32G32_FLOAT,       FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS, "R32G32_FLOAT" },
    { DXGI_FORMAT_R32G32_UINT,        FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R32G32_UINT" },
    { DXGI_FORMAT_R10G10B10A2_UNORM,  FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_VB|FS_DISPLAY, FS_DS, "R10G10B10A2_UNORM" },
    { DXGI_FORMAT_R10G10B10A2_UINT,   FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R10G10B10A2_UINT" },
    { DXGI_FORMAT_R11G11B10_FLOAT,    FS_T2|FS_SAMPLE|FS_RT|FS_VB, FS_DS, "R11G11B10_FLOAT" },
    { DXGI_FORMAT_R8G8B8A8_UNORM,     FS_T1|FS_T2|FS_T3|FS_TC|FS_LOAD|FS_SAMPLE|FS_MIP|FS_RT|FS_BLEND|FS_VB|FS_DISPLAY, FS_DS, "R8G8B8A8_UNORM" },
    { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_DISPLAY, FS_DS|FS_VB, "R8G8B8A8_UNORM_SRGB" },
    { DXGI_FORMAT_R8G8B8A8_UINT,      FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R8G8B8A8_UINT" },
    { DXGI_FORMAT_R8G8B8A8_SNORM,     FS_T2|FS_SAMPLE|FS_RT|FS_VB, FS_DS, "R8G8B8A8_SNORM" },
    { DXGI_FORMAT_R8G8B8A8_SINT,      FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R8G8B8A8_SINT" },
    { DXGI_FORMAT_R16G16_FLOAT,       FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_VB, FS_DS, "R16G16_FLOAT" },
    { DXGI_FORMAT_R16G16_UNORM,       FS_T2|FS_SAMPLE|FS_RT|FS_VB, FS_DS, "R16G16_UNORM" },
    { DXGI_FORMAT_R16G16_UINT,        FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R16G16_UINT" },
    { DXGI_FORMAT_R32_FLOAT,          FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS, "R32_FLOAT" },
    { DXGI_FORMAT_R32_UINT,           FS_T2|FS_LOAD|FS_RT|FS_VB|FS_IB, FS_DS|FS_SAMPLE, "R32_UINT" },
    { DXGI_FORMAT_R32_SINT,           FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R32_SINT" },
    { DXGI_FORMAT_D32_FLOAT,          FS_T2|FS_DS, FS_RT|FS_VB|FS_IB|FS_BLEND, "D32_FLOAT" },
    { DXGI_FORMAT_D24_UNORM_S8_UINT,  FS_T2|FS_DS, FS_RT|FS_VB|FS_IB|FS_BLEND, "D24_UNORM_S8_UINT" },
    { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, FS_T2|FS_DS, FS_RT|FS_VB|FS_IB|FS_BLEND, "D32_FLOAT_S8X24_UINT" },
    { DXGI_FORMAT_R8G8_UNORM,         FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_VB, FS_DS, "R8G8_UNORM" },
    { DXGI_FORMAT_R8G8_UINT,          FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R8G8_UINT" },
    { DXGI_FORMAT_R8G8_SNORM,         FS_T2|FS_SAMPLE|FS_RT|FS_VB, FS_DS, "R8G8_SNORM" },
    { DXGI_FORMAT_R16_FLOAT,          FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_VB, FS_DS, "R16_FLOAT" },
    { DXGI_FORMAT_D16_UNORM,          FS_T2|FS_DS, FS_RT|FS_VB|FS_IB|FS_BLEND, "D16_UNORM" },
    { DXGI_FORMAT_R16_UNORM,          FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_VB, FS_DS, "R16_UNORM" },
    { DXGI_FORMAT_R16_UINT,           FS_T2|FS_LOAD|FS_RT|FS_VB|FS_IB, FS_DS|FS_SAMPLE, "R16_UINT" },
    { DXGI_FORMAT_R16_SNORM,          FS_T2|FS_SAMPLE|FS_RT|FS_VB, FS_DS, "R16_SNORM" },
    { DXGI_FORMAT_R8_UNORM,           FS_T2|FS_SAMPLE|FS_RT|FS_BLEND|FS_VB, FS_DS, "R8_UNORM" },
    { DXGI_FORMAT_R8_UINT,            FS_T2|FS_LOAD|FS_RT|FS_VB, FS_DS|FS_SAMPLE, "R8_UINT" },
    { DXGI_FORMAT_R8_SNORM,           FS_T2|FS_SAMPLE|FS_RT|FS_VB, FS_DS, "R8_SNORM" },
    { DXGI_FORMAT_A8_UNORM,           FS_T2|FS_SAMPLE|FS_RT|FS_BLEND, FS_DS, "A8_UNORM" },
    { DXGI_FORMAT_R9G9B9E5_SHAREDEXP, FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_BLEND, "R9G9B9E5_SHAREDEXP" },
    { DXGI_FORMAT_R1_UNORM,           0, FS_RT|FS_DS|FS_BLEND|FS_VB, "R1_UNORM" },
    { DXGI_FORMAT_BC1_UNORM,          FS_T2|FS_T3|FS_TC|FS_SAMPLE|FS_MIP, FS_RT|FS_DS|FS_VB|FS_IB|FS_BLEND, "BC1_UNORM" },
    { DXGI_FORMAT_BC1_UNORM_SRGB,     FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC1_UNORM_SRGB" },
    { DXGI_FORMAT_BC2_UNORM,          FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC2_UNORM" },
    { DXGI_FORMAT_BC2_UNORM_SRGB,     FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC2_UNORM_SRGB" },
    { DXGI_FORMAT_BC3_UNORM,          FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC3_UNORM" },
    { DXGI_FORMAT_BC3_UNORM_SRGB,     FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC3_UNORM_SRGB" },
    { DXGI_FORMAT_BC4_UNORM,          FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC4_UNORM" },
    { DXGI_FORMAT_BC4_SNORM,          FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC4_SNORM" },
    { DXGI_FORMAT_BC5_UNORM,          FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC5_UNORM" },
    { DXGI_FORMAT_BC5_SNORM,          FS_T2|FS_SAMPLE, FS_RT|FS_DS|FS_VB|FS_IB, "BC5_SNORM" },
};
#define NFMT ((int)(sizeof g_fmt / sizeof *g_fmt))

T(format_table_must_bits)
{
    DD_DEV(dev);
    for (int i = 0; i < NFMT; i++) {
        HRESULT hr; UINT s = support_of(dev, g_fmt[i].f, &hr);
        t_check(SUCCEEDED(hr), __FILE__, __LINE__, g_fmt[i].name, "CheckFormatSupport hr %s", d3dh_hr_name(hr));
        if (FAILED(hr)) continue;
        t_check((s & g_fmt[i].must) == g_fmt[i].must, __FILE__, __LINE__, g_fmt[i].name, "support 0x%x missing 0x%x", s, g_fmt[i].must & ~s);
    }
    DD_END(dev);
}
T(format_table_mustnot_bits)
{
    DD_DEV(dev);
    for (int i = 0; i < NFMT; i++) {
        HRESULT hr; UINT s = support_of(dev, g_fmt[i].f, &hr);
        if (FAILED(hr)) continue;
        t_check((s & g_fmt[i].mustnot) == 0, __FILE__, __LINE__, g_fmt[i].name, "support 0x%x has forbidden 0x%x", s, s & g_fmt[i].mustnot);
    }
    DD_END(dev);
}

static int fmt_idx(DXGI_FORMAT f) { for (int i = 0; i < NFMT; i++) if (g_fmt[i].f == f) return i; return -1; }
#define FSPEC(nm, F) T(format_##nm) { DD_DEV(dev); int ix = fmt_idx(F); HRESULT hr; UINT s = support_of(dev, F, &hr); \
    EXPECT_HR_OK(hr); CHECK(ix >= 0); if (ix >= 0) { CHECK((s & g_fmt[ix].must) == g_fmt[ix].must); CHECK((s & g_fmt[ix].mustnot) == 0); } DD_END(dev); }
FSPEC(r32g32b32a32_float, DXGI_FORMAT_R32G32B32A32_FLOAT) FSPEC(r32g32b32a32_uint, DXGI_FORMAT_R32G32B32A32_UINT)
FSPEC(r16g16b16a16_float, DXGI_FORMAT_R16G16B16A16_FLOAT) FSPEC(r8g8b8a8_unorm, DXGI_FORMAT_R8G8B8A8_UNORM)
FSPEC(r8g8b8a8_srgb, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) FSPEC(d32_float, DXGI_FORMAT_D32_FLOAT)
FSPEC(d24s8, DXGI_FORMAT_D24_UNORM_S8_UINT) FSPEC(d16, DXGI_FORMAT_D16_UNORM) FSPEC(bc1, DXGI_FORMAT_BC1_UNORM)
FSPEC(bc3, DXGI_FORMAT_BC3_UNORM) FSPEC(a8, DXGI_FORMAT_A8_UNORM) FSPEC(r11g11b10, DXGI_FORMAT_R11G11B10_FLOAT)
FSPEC(r10g10b10a2, DXGI_FORMAT_R10G10B10A2_UNORM) FSPEC(r9g9b9e5, DXGI_FORMAT_R9G9B9E5_SHAREDEXP)
FSPEC(r16_uint_index, DXGI_FORMAT_R16_UINT) FSPEC(r32_uint_index, DXGI_FORMAT_R32_UINT)

T(format_sweep_result_protocol)
{
    DD_DEV(dev);
    for (UINT f = 0; f <= LAST_FMT; f++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr);
        t_check(hr == S_OK || (hr == E_FAIL && s == 0), __FILE__, __LINE__, "sweep", "format %u: hr %s support 0x%x", f, d3dh_hr_name(hr), s);
    }
    DD_END(dev);
}
T(format_sweep_no_gather)
{
    DD_DEV(dev);
    for (UINT f = 0; f <= LAST_FMT; f++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr);
        if (FAILED(hr)) continue;
        t_check(!(s & D3D10_FORMAT_SUPPORT_SHADER_GATHER), __FILE__, __LINE__, "gather", "format %u has SHADER_GATHER 0x%x", f, s);
        t_check(!(s & D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON), __FILE__, __LINE__, "gather cmp", "format %u 0x%x", f, s);
    }
    DD_END(dev);
}
T(format_sweep_implications)
{
    DD_DEV(dev);
    for (UINT f = 0; f <= LAST_FMT; f++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr);
        if (FAILED(hr)) continue;
        if (s & FS_DS) t_check(s & FS_T2, __FILE__, __LINE__, "DS => TEXTURE2D", "format %u 0x%x", f, s);
        if (s & FS_RT) t_check((s & (FS_T1|FS_T2|FS_T3|FS_BUFFER|FS_VB|FS_TC)) != 0, __FILE__, __LINE__, "RT => some resource kind", "format %u 0x%x", f, s);
        if (s & FS_BLEND) t_check(s & FS_RT, __FILE__, __LINE__, "BLENDABLE => RT", "format %u 0x%x", f, s);
        if (s & FS_MSRT) t_check(s & FS_RT, __FILE__, __LINE__, "MSRT => RT", "format %u 0x%x", f, s);
        if (s & FS_DS) t_check(!(s & FS_RT), __FILE__, __LINE__, "DS excl RT", "format %u 0x%x", f, s);
        if (s & FS_DISPLAY) t_check(s & FS_RT, __FILE__, __LINE__, "DISPLAY => RT", "format %u 0x%x", f, s);
        if (s & FS_IB) t_check(f == DXGI_FORMAT_R16_UINT || f == DXGI_FORMAT_R32_UINT, __FILE__, __LINE__, "IB only R16/R32 UINT", "format %u", f);
    }
    DD_END(dev);
}
T(format_unknown)
{
    DD_DEV(dev);
    HRESULT hr; UINT s = support_of(dev, DXGI_FORMAT_UNKNOWN, &hr);
    t_note("UNKNOWN: hr %s support 0x%x", d3dh_hr_name(hr), s);
    CHECK(hr == S_OK || (hr == E_FAIL && s == 0));
    DD_END(dev);
}
T(format_out_of_range_fail)
{
    static const UINT bad[] = { 116, 130, 200, 1000, 65536, 0x7fffffff, 0xffffffffu };
    DD_DEV(dev);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)bad[i], &hr);
        t_check(hr == E_FAIL && s == 0, __FILE__, __LINE__, "bad format", "format %u: hr %s support 0x%x", bad[i], d3dh_hr_name(hr), s);
    }
    DD_END(dev);
}
T(format_all_ones_is_efail_and_zeroes_output)
{
    DD_DEV(dev);
    UINT s = 0xdeadbeef;
    EXPECT_HR(ID3D10Device_CheckFormatSupport(dev, (DXGI_FORMAT)~0u, &s), E_FAIL);
    EXPECT_EQ(s, 0);
    DD_END(dev);
}
T(format_deterministic)
{
    DD_DEV(dev);
    for (UINT f = 0; f <= LAST_FMT; f++) {
        HRESULT h1, h2; UINT a = support_of(dev, (DXGI_FORMAT)f, &h1), b = support_of(dev, (DXGI_FORMAT)f, &h2);
        t_check(h1 == h2 && a == b, __FILE__, __LINE__, "repeat", "format %u differs", f);
    }
    DD_END(dev);
}
T(format_second_device_agrees)
{
    ID3D10Device *d2 = NULL;
    DD_DEV(dev);
    if (FAILED(dd_make(0, &d2))) { DD_END(dev); t_skip("no second device"); return; }
    for (UINT f = 0; f <= LAST_FMT; f++) {
        HRESULT h1, h2; UINT a = support_of(dev, (DXGI_FORMAT)f, &h1), b = support_of(d2, (DXGI_FORMAT)f, &h2);
        t_check(h1 == h2 && a == b, __FILE__, __LINE__, "two devices", "format %u differs", f);
    }
    ID3D10Device_Release(d2);
    DD_END(dev);
}
T(format_typeless_observed)
{
    static const DXGI_FORMAT tl[] = { DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R8G8B8A8_TYPELESS,
        DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_BC1_TYPELESS, DXGI_FORMAT_B8G8R8A8_TYPELESS };
    DD_DEV(dev);
    for (unsigned i = 0; i < sizeof tl / sizeof *tl; i++) {
        HRESULT hr; UINT s = support_of(dev, tl[i], &hr);
        t_note("typeless %u: %s 0x%x", tl[i], d3dh_hr_name(hr), s);
        CHECK(!(s & FS_DS) || !(s & FS_RT));
    }
    DD_END(dev);
}
T(format_bgra_observed)
{
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B5G6R5_UNORM,
        DXGI_FORMAT_B5G5R5A1_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM };
    DD_DEV(dev);
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        HRESULT hr; UINT s = support_of(dev, f[i], &hr);
        t_note("format %u: %s support 0x%x", f[i], d3dh_hr_name(hr), s);
    }
    DD_END(dev);
}
T(format_video_observed)
{
    DD_DEV(dev);
    for (UINT f = 103; f <= LAST_FMT; f++) { HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr); t_note("format %u: %s 0x%x", f, d3dh_hr_name(hr), s); }
    DD_END(dev);
}

/* ---- consistency between CheckFormatSupport and resource creation ---- */
T(consistency_rt_texture2d_creatable)
{
    DD_DEV(dev);
    for (UINT f = 1; f <= LAST_FMT; f++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr);
        if (FAILED(hr) || !(s & FS_T2) || !(s & FS_RT)) continue;
        D3D10_TEXTURE2D_DESC d = { 16, 16, 1, 1, (DXGI_FORMAT)f, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET, 0, 0 };
        ID3D10Texture2D *t = NULL;
        hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &t);
        t_check(SUCCEEDED(hr), __FILE__, __LINE__, "RT texture", "format %u: %s", f, d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
    DD_END(dev);
}
T(consistency_ds_texture2d_creatable)
{
    DD_DEV(dev);
    for (UINT f = 1; f <= LAST_FMT; f++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr);
        if (FAILED(hr) || !(s & FS_DS)) continue;
        D3D10_TEXTURE2D_DESC d = { 16, 16, 1, 1, (DXGI_FORMAT)f, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_DEPTH_STENCIL, 0, 0 };
        ID3D10Texture2D *t = NULL;
        hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &t);
        t_check(SUCCEEDED(hr), __FILE__, __LINE__, "DS texture", "format %u: %s", f, d3dh_hr_name(hr));
        SAFE_RELEASE(t);
    }
    DD_END(dev);
}
T(consistency_sample_texture2d_srv_creatable)
{
    DD_DEV(dev);
    for (UINT f = 1; f <= LAST_FMT; f++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr);
        if (FAILED(hr) || !(s & FS_T2) || !(s & (FS_SAMPLE | FS_LOAD)) || (s & FS_DS)) continue;
        UINT dim = (f >= DXGI_FORMAT_BC1_TYPELESS && f <= DXGI_FORMAT_BC5_SNORM) ? 16 : 8;
        D3D10_TEXTURE2D_DESC d = { dim, dim, 1, 1, (DXGI_FORMAT)f, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_SHADER_RESOURCE, 0, 0 };
        ID3D10Texture2D *t = NULL; ID3D10ShaderResourceView *v = NULL;
        hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &t);
        t_check(SUCCEEDED(hr), __FILE__, __LINE__, "SRV texture", "format %u: %s", f, d3dh_hr_name(hr));
        if (t) {
            hr = ID3D10Device_CreateShaderResourceView(dev, (ID3D10Resource *)t, NULL, &v);
            t_check(SUCCEEDED(hr), __FILE__, __LINE__, "SRV", "format %u: %s", f, d3dh_hr_name(hr));
        }
        SAFE_RELEASE(v); SAFE_RELEASE(t);
    }
    DD_END(dev);
}
T(consistency_vertex_format_buffer)
{
    DD_DEV(dev);
    ID3DBlob *b = dd_blob(DD_VS_POS, "vs_4_0");
    if (b) {
        for (UINT f = 1; f <= LAST_FMT; f++) {
            HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr);
            if (FAILED(hr) || !(s & FS_VB)) continue;
            D3D10_INPUT_ELEMENT_DESC e = { "POSITION", 0, (DXGI_FORMAT)f, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 };
            ID3D10InputLayout *il = NULL;
            /* the shader reads float4: formats with fewer/different components still validate or give a type mismatch, never crash */
            hr = ID3D10Device_CreateInputLayout(dev, &e, 1, d3dh_blob_ptr(b), d3dh_blob_size(b), &il);
            if (FAILED(hr)) t_note("input layout format %u: %s", f, d3dh_hr_name(hr));
            SAFE_RELEASE(il);
        }
        ID3D10Blob_Release(b);
    }
    DD_END(dev);
}
T(consistency_non_ds_format_rejects_ds_bind)
{
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_R16_UINT };
    DD_DEV(dev);
    for (unsigned i = 0; i < 4; i++) {
        D3D10_TEXTURE2D_DESC d = { 16, 16, 1, 1, f[i], { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_DEPTH_STENCIL, 0, 0 };
        ID3D10Texture2D *t = (ID3D10Texture2D *)(ULONG_PTR)1;
        HRESULT hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &t);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "non-DS format with DS bind", "format %u: %s", f[i], d3dh_hr_name(hr));
        CHECK(t == NULL);
    }
    DD_END(dev);
}
T(consistency_compressed_rejects_rt_bind)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 16, 16, 1, 1, DXGI_FORMAT_BC1_UNORM, { 1, 0 }, D3D10_USAGE_DEFAULT, D3D10_BIND_RENDER_TARGET, 0, 0 };
    ID3D10Texture2D *t = NULL;
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t), E_INVALIDARG);
    CHECK(t == NULL);
    DD_END(dev);
}

/* ---- multisample quality levels ---- */
static HRESULT msq(ID3D10Device *dev, DXGI_FORMAT f, UINT n, UINT *q) { return ID3D10Device_CheckMultisampleQualityLevels(dev, f, n, q); }

T(msaa_count1_one_level)
{
    DD_DEV(dev);
    UINT q = 0xdeadbeef;
    HRESULT hr = msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 1, &q);
    EXPECT_HR_OK(hr);
    EXPECT_EQ(q, 1);
    DD_END(dev);
}
T(msaa_count0_fails)
{
    DD_DEV(dev);
    UINT q = 0xdeadbeef;
    HRESULT hr = msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &q);
    EXPECT_HR(hr, E_FAIL);
    EXPECT_EQ(q, 0);
    DD_END(dev);
}
T(msaa_count2_supported_rgba8)
{
    DD_DEV(dev);
    UINT q = 0xdeadbeef;
    EXPECT_HR_OK(msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 2, &q));
    t_note("R8G8B8A8_UNORM 2x levels: %u", q);
    DD_END(dev);
}
T(msaa_count4_rgba8_nonzero)
{
    DD_DEV_R(dev);
    UINT q = 0xdeadbeef;
    EXPECT_HR_OK(msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 4, &q));
    CHECK(q > 0);
    DD_END(dev);
}
T(msaa_count15_none)
{
    DD_DEV(dev);
    UINT q = 0xdeadbeef;
    EXPECT_HR_OK(msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 15, &q));
    EXPECT_EQ(q, 0);
    DD_END(dev);
}
T(msaa_count32_succeeds)
{
    DD_DEV(dev);
    UINT q = 0xdeadbeef;
    EXPECT_HR_OK(msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 32, &q));
    EXPECT_EQ(q, 0);
    DD_END(dev);
}
#define MSBIG(nm, n) T(msaa_count##nm##_fails) { DD_DEV(dev); UINT q = 0xdeadbeef; \
    EXPECT_HR(msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, n, &q), E_FAIL); EXPECT_EQ(q, 0); DD_END(dev); }
MSBIG(33, 33) MSBIG(64, 64) MSBIG(100, 100) MSBIG(max, 0xffffffffu)
T(msaa_format_unknown)
{
    DD_DEV(dev);
    UINT q = 0xdeadbeef;
    HRESULT hr = msq(dev, DXGI_FORMAT_UNKNOWN, 2, &q);
    CHECK(SUCCEEDED(hr));
    EXPECT_EQ(q, 0);
    DD_END(dev);
}
T(msaa_format_out_of_range)
{
    DD_DEV(dev);
    UINT q = 0xdeadbeef;
    EXPECT_HR(msq(dev, (DXGI_FORMAT)65536, 2, &q), E_INVALIDARG);
    EXPECT_EQ(q, 0xdeadbeef);
    q = 0xdeadbeef;
    EXPECT_HR(msq(dev, (DXGI_FORMAT)0xffffffffu, 2, &q), E_INVALIDARG);
    EXPECT_EQ(q, 0xdeadbeef);
    DD_END(dev);
}
T(msaa_null_output)
{
    DD_DEV(dev);
    EXPECT_HR(msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 0, NULL), E_INVALIDARG);
    EXPECT_HR(msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 1, NULL), E_INVALIDARG);
    EXPECT_HR(msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, 2, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(msaa_compressed_zero_levels)
{
    DD_DEV(dev);
    UINT q = 0xdeadbeef;
    EXPECT_HR_OK(msq(dev, DXGI_FORMAT_BC3_UNORM, 2, &q));
    EXPECT_EQ(q, 0);
    q = 0xdeadbeef;
    EXPECT_HR_OK(msq(dev, DXGI_FORMAT_BC1_UNORM, 4, &q));
    EXPECT_EQ(q, 0);
    DD_END(dev);
}
T(msaa_count1_every_supported_rt_format)
{
    DD_DEV(dev);
    for (UINT f = 1; f <= LAST_FMT; f++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr), q = 0xdeadbeef;
        if (FAILED(hr) || !(s & (FS_RT | FS_DS))) continue;
        HRESULT h2 = msq(dev, (DXGI_FORMAT)f, 1, &q);
        t_check(SUCCEEDED(h2) && q == 1, __FILE__, __LINE__, "count 1", "format %u: %s levels %u", f, d3dh_hr_name(h2), q);
    }
    DD_END(dev);
}
T(msaa_consistent_with_format_support)
{
    DD_DEV(dev);
    for (UINT f = 1; f <= LAST_FMT; f++) {
        HRESULT hr; UINT s = support_of(dev, (DXGI_FORMAT)f, &hr), q = 0;
        if (FAILED(hr)) continue;
        if (SUCCEEDED(msq(dev, (DXGI_FORMAT)f, 4, &q)) && q > 0)
            t_check(s & (FS_MSRT | FS_RT | FS_DS | FS_MSRES), __FILE__, __LINE__, "4x ms", "format %u reports %u levels but support 0x%x", f, q, s);
    }
    DD_END(dev);
}
T(msaa_levels_match_texture_creation)
{
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT };
    static const UINT counts[] = { 2, 4, 8 };
    DD_DEV_R(dev);
    for (unsigned i = 0; i < 5; i++) for (unsigned c = 0; c < 3; c++) {
        UINT q = 0;
        HRESULT hr = msq(dev, f[i], counts[c], &q);
        if (FAILED(hr)) continue;
        UINT bind = (f[i] == DXGI_FORMAT_D24_UNORM_S8_UINT || f[i] == DXGI_FORMAT_D32_FLOAT) ? D3D10_BIND_DEPTH_STENCIL : D3D10_BIND_RENDER_TARGET;
        D3D10_TEXTURE2D_DESC d = { 16, 16, 1, 1, f[i], { counts[c], 0 }, D3D10_USAGE_DEFAULT, bind, 0, 0 };
        ID3D10Texture2D *t = NULL;
        hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &t);
        if (q > 0) t_check(SUCCEEDED(hr), __FILE__, __LINE__, "ms tex", "format %u x%u q0: %s", f[i], counts[c], d3dh_hr_name(hr));
        else t_check(FAILED(hr), __FILE__, __LINE__, "ms tex unsupported", "format %u x%u unexpectedly created", f[i], counts[c]);
        SAFE_RELEASE(t);
        if (q > 0) {
            ID3D10Texture2D *t2 = NULL;
            d.SampleDesc.Quality = q;
            hr = ID3D10Device_CreateTexture2D(dev, &d, NULL, &t2);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "quality == levels", "format %u x%u: %s", f[i], counts[c], d3dh_hr_name(hr));
            SAFE_RELEASE(t2);
        }
    }
    DD_END(dev);
}
T(msaa_standard_pattern_quality_observed)
{
    DD_DEV(dev);
    UINT q = 0;
    for (UINT n = 1; n <= 16; n++) {
        HRESULT hr = msq(dev, DXGI_FORMAT_R8G8B8A8_UNORM, n, &q);
        t_note("RGBA8 %ux: %s levels %u", n, d3dh_hr_name(hr), q);
    }
    DD_END(dev);
}
T(msaa_depth_formats_observed)
{
    DD_DEV(dev);
    UINT q = 0;
    for (UINT f = 1; f <= LAST_FMT; f++) {
        HRESULT hr = msq(dev, (DXGI_FORMAT)f, 4, &q);
        if (SUCCEEDED(hr) && q) t_note("format %u 4x levels %u", f, q);
    }
    DD_END(dev);
}

/* ---- counters ---- */
T(counter_info_basic)
{
    D3D10_COUNTER_INFO info;
    DD_DEV(dev);
    memset(&info, 0xcc, sizeof info);
    ID3D10Device_CheckCounterInfo(dev, &info);
    t_note("counter info: last=0x%x simultaneous=%u parallel=%u", info.LastDeviceDependentCounter, info.NumSimultaneousCounters, info.NumDetectableParallelUnits);
    CHECK(info.NumSimultaneousCounters >= 0);
    CHECK(info.LastDeviceDependentCounter == 0 || info.LastDeviceDependentCounter >= D3D10_COUNTER_DEVICE_DEPENDENT_0);
    DD_END(dev);
}
T(counter_info_stable)
{
    D3D10_COUNTER_INFO a, b;
    DD_DEV(dev);
    ID3D10Device_CheckCounterInfo(dev, &a);
    ID3D10Device_CheckCounterInfo(dev, &b);
    EXPECT_EQ(a.LastDeviceDependentCounter, b.LastDeviceDependentCounter);
    EXPECT_EQ(a.NumSimultaneousCounters, b.NumSimultaneousCounters);
    EXPECT_EQ(a.NumDetectableParallelUnits, b.NumDetectableParallelUnits);
    DD_END(dev);
}
T(counter_no_device_dependent_when_last_zero)
{
    D3D10_COUNTER_INFO info; D3D10_COUNTER_DESC desc; D3D10_COUNTER_TYPE ty; UINT act = 0, nl = 0, ul = 0, dl = 0;
    DD_DEV(dev);
    ID3D10Device_CheckCounterInfo(dev, &info);
    if (info.LastDeviceDependentCounter != 0) { DD_END(dev); t_skip("device exposes device dependent counters"); return; }
    desc.Counter = D3D10_COUNTER_DEVICE_DEPENDENT_0; desc.MiscFlags = 0;
    HRESULT hr = ID3D10Device_CheckCounter(dev, &desc, &ty, &act, NULL, &nl, NULL, &ul, NULL, &dl);
    CHECK(FAILED(hr));
    t_note("CheckCounter(DEVICE_DEPENDENT_0) = %s", d3dh_hr_name(hr));
    ID3D10Counter *c = (ID3D10Counter *)(ULONG_PTR)1;
    hr = ID3D10Device_CreateCounter(dev, &desc, &c);
    CHECK(FAILED(hr));
    CHECK(c == NULL);
    DD_END(dev);
}
T(counter_check_standard_observed)
{
    DD_DEV(dev);
    for (int c = D3D10_COUNTER_GPU_IDLE; c <= D3D10_COUNTER_TEXTURE_CACHE_HIT_RATE; c++) {
        D3D10_COUNTER_DESC d = { (D3D10_COUNTER)c, 0 }; D3D10_COUNTER_TYPE ty = 0; UINT act = 0, nl = 0, ul = 0, dl = 0;
        HRESULT hr = ID3D10Device_CheckCounter(dev, &d, &ty, &act, NULL, &nl, NULL, &ul, NULL, &dl);
        if (SUCCEEDED(hr)) {
            t_note("counter %d supported: type %d active %u name %u units %u desc %u", c, ty, act, nl, ul, dl);
            CHECK(nl > 0 && ul > 0 && dl > 0);
        } else {
            t_check(hr == DXGI_ERROR_UNSUPPORTED || hr == E_INVALIDARG || hr == E_FAIL, __FILE__, __LINE__, "unsupported counter", "counter %d: %s", c, d3dh_hr_name(hr));
        }
    }
    DD_END(dev);
}
T(counter_create_unsupported_fails_cleanly)
{
    DD_DEV(dev);
    for (int c = D3D10_COUNTER_GPU_IDLE; c <= D3D10_COUNTER_TEXTURE_CACHE_HIT_RATE; c++) {
        D3D10_COUNTER_DESC d = { (D3D10_COUNTER)c, 0 }; D3D10_COUNTER_TYPE ty = 0; UINT act = 0, nl = 0, ul = 0, dl = 0;
        HRESULT chk = ID3D10Device_CheckCounter(dev, &d, &ty, &act, NULL, &nl, NULL, &ul, NULL, &dl);
        ID3D10Counter *cnt = (ID3D10Counter *)(ULONG_PTR)1;
        HRESULT hr = ID3D10Device_CreateCounter(dev, &d, &cnt);
        if (FAILED(chk)) { t_check(FAILED(hr) && cnt == NULL, __FILE__, __LINE__, "unsupported => create fails", "counter %d: %s", c, d3dh_hr_name(hr)); }
        else if (SUCCEEDED(hr)) {
            D3D10_COUNTER_DESC g; memset(&g, 0xcc, sizeof g);
            ID3D10Counter_GetDesc(cnt, &g);
            EXPECT_EQ(g.Counter, c);
            EXPECT_EQ(ID3D10Counter_GetDataSize(cnt) > 0, 1);
        }
        SAFE_RELEASE(cnt);
    }
    DD_END(dev);
}
T(counter_create_null_desc)
{
    DD_DEV(dev);
    ID3D10Counter *c = (ID3D10Counter *)(ULONG_PTR)1;
    HRESULT hr = ID3D10Device_CreateCounter(dev, NULL, &c);
    EXPECT_HR(hr, E_INVALIDARG);
    DD_END(dev);
}
T(counter_create_invalid_value)
{
    DD_DEV(dev);
    D3D10_COUNTER_DESC d = { (D3D10_COUNTER)0x3fffffff, 0 };
    ID3D10Counter *c = (ID3D10Counter *)(ULONG_PTR)1;
    HRESULT hr = ID3D10Device_CreateCounter(dev, &d, &c);
    CHECK(FAILED(hr));
    CHECK(c == NULL);
    d.Counter = (D3D10_COUNTER)0xffffffffu;
    c = (ID3D10Counter *)(ULONG_PTR)1;
    hr = ID3D10Device_CreateCounter(dev, &d, &c);
    CHECK(FAILED(hr));
    CHECK(c == NULL);
    DD_END(dev);
}

/* ---- query / predicate creation ---- */
static const struct { D3D10_QUERY q; BOOL pred; UINT size; const char *name; } g_q[] = {
    { D3D10_QUERY_EVENT, FALSE, sizeof(BOOL), "EVENT" },
    { D3D10_QUERY_OCCLUSION, FALSE, sizeof(UINT64), "OCCLUSION" },
    { D3D10_QUERY_TIMESTAMP, FALSE, sizeof(UINT64), "TIMESTAMP" },
    { D3D10_QUERY_TIMESTAMP_DISJOINT, FALSE, sizeof(D3D10_QUERY_DATA_TIMESTAMP_DISJOINT), "TIMESTAMP_DISJOINT" },
    { D3D10_QUERY_PIPELINE_STATISTICS, FALSE, sizeof(D3D10_QUERY_DATA_PIPELINE_STATISTICS), "PIPELINE_STATISTICS" },
    { D3D10_QUERY_OCCLUSION_PREDICATE, TRUE, sizeof(BOOL), "OCCLUSION_PREDICATE" },
    { D3D10_QUERY_SO_STATISTICS, FALSE, sizeof(D3D10_QUERY_DATA_SO_STATISTICS), "SO_STATISTICS" },
    { D3D10_QUERY_SO_OVERFLOW_PREDICATE, TRUE, sizeof(BOOL), "SO_OVERFLOW_PREDICATE" },
};
T(query_enum_values)
{
    for (int i = 0; i < 8; i++) { EXPECT_EQ(g_q[i].q, i); }
    EXPECT_EQ(sizeof(D3D10_QUERY_DATA_TIMESTAMP_DISJOINT), 16);
    EXPECT_EQ(sizeof(D3D10_QUERY_DATA_PIPELINE_STATISTICS), 64);
    EXPECT_EQ(sizeof(D3D10_QUERY_DATA_SO_STATISTICS), 16);
}
T(query_create_null_desc)
{
    DD_DEV(dev);
    ID3D10Query *q = (ID3D10Query *)(ULONG_PTR)1; ID3D10Predicate *p = (ID3D10Predicate *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Device_CreateQuery(dev, NULL, &q), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreatePredicate(dev, NULL, &p), E_INVALIDARG);
    DD_END(dev);
}
T(query_create_each_type)
{
    DD_DEV(dev);
    for (int i = 0; i < 8; i++) {
        D3D10_QUERY_DESC d = { g_q[i].q, 0 }; ID3D10Query *q = NULL;
        HRESULT hr = ID3D10Device_CreateQuery(dev, &d, &q);
        if (hr != S_OK) { t_check(0, __FILE__, __LINE__, g_q[i].name, "CreateQuery %s", d3dh_hr_name(hr)); continue; }
        CHECK(q != NULL);
        SAFE_RELEASE(q);
    }
    DD_END(dev);
}
T(query_create_null_out_returns_false)
{
    DD_DEV(dev);
    for (int i = 0; i < 8; i++) {
        D3D10_QUERY_DESC d = { g_q[i].q, 0 };
        HRESULT hr = ID3D10Device_CreateQuery(dev, &d, NULL);
        t_check(hr == S_FALSE, __FILE__, __LINE__, g_q[i].name, "CreateQuery(NULL out) %s", d3dh_hr_name(hr));
    }
    DD_END(dev);
}
T(query_predicate_interface_only_on_predicates)
{
    DD_DEV(dev);
    for (int i = 0; i < 8; i++) {
        D3D10_QUERY_DESC d = { g_q[i].q, 0 }; ID3D10Query *q = NULL; ID3D10Predicate *p = NULL;
        if (ID3D10Device_CreateQuery(dev, &d, &q) != S_OK) continue;
        HRESULT hr = ID3D10Query_QueryInterface(q, &IID_ID3D10Predicate, (void **)&p);
        if (g_q[i].pred) t_check(hr == S_OK, __FILE__, __LINE__, g_q[i].name, "QI predicate %s", d3dh_hr_name(hr));
        else t_check(hr == E_NOINTERFACE, __FILE__, __LINE__, g_q[i].name, "QI predicate %s", d3dh_hr_name(hr));
        SAFE_RELEASE(p); ID3D10Query_Release(q);
    }
    DD_END(dev);
}
T(query_create_predicate_matrix)
{
    DD_DEV(dev);
    for (int i = 0; i < 8; i++) {
        D3D10_QUERY_DESC d = { g_q[i].q, 0 }; ID3D10Predicate *p = (ID3D10Predicate *)(ULONG_PTR)1;
        HRESULT want = g_q[i].pred ? S_FALSE : E_INVALIDARG;
        HRESULT hr = ID3D10Device_CreatePredicate(dev, &d, NULL);
        t_check(hr == want, __FILE__, __LINE__, g_q[i].name, "CreatePredicate(NULL out) %s", d3dh_hr_name(hr));
        want = g_q[i].pred ? S_OK : E_INVALIDARG;
        hr = ID3D10Device_CreatePredicate(dev, &d, &p);
        t_check(hr == want, __FILE__, __LINE__, g_q[i].name, "CreatePredicate %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) ID3D10Predicate_Release(p);
    }
    DD_END(dev);
}
T(query_data_size_and_desc)
{
    DD_DEV(dev);
    for (int i = 0; i < 8; i++) {
        D3D10_QUERY_DESC d = { g_q[i].q, 0 }, g; ID3D10Query *q = NULL;
        if (ID3D10Device_CreateQuery(dev, &d, &q) != S_OK) continue;
        UINT sz = ID3D10Query_GetDataSize(q);
        t_check(sz == g_q[i].size, __FILE__, __LINE__, g_q[i].name, "GetDataSize %u want %u", sz, g_q[i].size);
        memset(&g, 0xcc, sizeof g);
        ID3D10Query_GetDesc(q, &g);
        t_check(g.Query == g_q[i].q && g.MiscFlags == 0, __FILE__, __LINE__, g_q[i].name, "GetDesc %d/%u", g.Query, g.MiscFlags);
        ID3D10Query_Release(q);
    }
    DD_END(dev);
}
T(query_invalid_type)
{
    static const UINT bad[] = { 8, 9, 100, 0x7fffffff, 0xffffffffu };
    DD_DEV(dev);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        D3D10_QUERY_DESC d = { (D3D10_QUERY)bad[i], 0 }; ID3D10Query *q = (ID3D10Query *)(ULONG_PTR)1;
        HRESULT hr = ID3D10Device_CreateQuery(dev, &d, &q);
        t_check(hr == E_INVALIDARG && q == NULL, __FILE__, __LINE__, "bad query type", "type %u: %s", bad[i], d3dh_hr_name(hr));
    }
    DD_END(dev);
}
T(query_predicate_hint_flag)
{
    DD_DEV(dev);
    for (int i = 0; i < 8; i++) {
        D3D10_QUERY_DESC d = { g_q[i].q, D3D10_QUERY_MISC_PREDICATEHINT }; ID3D10Query *q = NULL;
        HRESULT hr = ID3D10Device_CreateQuery(dev, &d, &q);
        t_note("%s + PREDICATEHINT: %s", g_q[i].name, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) { D3D10_QUERY_DESC g; ID3D10Query_GetDesc(q, &g); EXPECT_EQ(g.MiscFlags, D3D10_QUERY_MISC_PREDICATEHINT); ID3D10Query_Release(q); }
    }
    DD_END(dev);
}
T(query_invalid_misc_flags)
{
    DD_DEV(dev);
    D3D10_QUERY_DESC d = { D3D10_QUERY_EVENT, 0x80 }; ID3D10Query *q = (ID3D10Query *)(ULONG_PTR)1;
    HRESULT hr = ID3D10Device_CreateQuery(dev, &d, &q);
    t_note("EVENT + misc 0x80: %s", d3dh_hr_name(hr));
    if (FAILED(hr)) CHECK(q == NULL); else ID3D10Query_Release(q);
    DD_END(dev);
}
T(query_device_backpointer)
{
    DD_DEV(dev);
    D3D10_QUERY_DESC d = { D3D10_QUERY_EVENT, 0 }; ID3D10Query *q = NULL; ID3D10Device *got = NULL;
    EXPECT_HR(ID3D10Device_CreateQuery(dev, &d, &q), S_OK);
    if (q) {
        ULONG before = d3dh_refcount(dev);
        ID3D10Query_GetDevice(q, &got);
        CHECK(got == dev);
        EXPECT_EQ(d3dh_refcount(dev), before + 1);
        SAFE_RELEASE(got);
        EXPECT_EQ(d3dh_refcount(dev), before);
        ID3D10Query_Release(q);
    }
    DD_END(dev);
}
T(query_holds_device_reference)
{
    DD_DEV(dev);
    D3D10_QUERY_DESC d = { D3D10_QUERY_OCCLUSION, 0 }; ID3D10Query *q = NULL;
    ULONG base = d3dh_refcount(dev);
    EXPECT_HR(ID3D10Device_CreateQuery(dev, &d, &q), S_OK);
    CHECK(d3dh_refcount(dev) >= base);
    t_note("device refcount with a query: %lu (base %lu)", d3dh_refcount(dev), base);
    SAFE_RELEASE(q);
    EXPECT_EQ(d3dh_refcount(dev), base);
    DD_END(dev);
}
T(query_d3d11_identity_observed)
{
    DD_DEV(dev);
    D3D10_QUERY_DESC d = { D3D10_QUERY_OCCLUSION_PREDICATE, 0 }; ID3D10Predicate *p = NULL; ID3D11Predicate *p11 = NULL;
    EXPECT_HR(ID3D10Device_CreatePredicate(dev, &d, &p), S_OK);
    if (p) {
        HRESULT hr = ID3D10Predicate_QueryInterface(p, &IID_ID3D11Predicate, (void **)&p11);
        t_note("QI ID3D11Predicate: %s", d3dh_hr_name(hr));
        SAFE_RELEASE(p11); ID3D10Predicate_Release(p);
    }
    DD_END(dev);
}

/* ---- predication state and text filter ---- */
T(predication_default_unset)
{
    DD_DEV(dev);
    ID3D10Predicate *p = (ID3D10Predicate *)(ULONG_PTR)1; BOOL val = 0xcc;
    ID3D10Device_GetPredication(dev, &p, &val);
    CHECK(p == NULL);
    EXPECT_EQ(val, FALSE);
    DD_END(dev);
}
T(predication_set_get_roundtrip)
{
    DD_DEV(dev);
    D3D10_QUERY_DESC d = { D3D10_QUERY_OCCLUSION_PREDICATE, 0 }; ID3D10Predicate *p = NULL, *g = NULL; BOOL val = 0;
    EXPECT_HR(ID3D10Device_CreatePredicate(dev, &d, &p), S_OK);
    if (p) {
        ULONG rc = d3dh_refcount(p);
        ID3D10Device_SetPredication(dev, p, TRUE);
        CHECK(d3dh_refcount(p) > rc);
        ID3D10Device_GetPredication(dev, &g, &val);
        CHECK(g == p);
        EXPECT_EQ(val, TRUE);
        SAFE_RELEASE(g);
        ID3D10Device_SetPredication(dev, NULL, FALSE);
        EXPECT_EQ(d3dh_refcount(p), rc);
        ID3D10Device_GetPredication(dev, &g, &val);
        CHECK(g == NULL);
        EXPECT_EQ(val, FALSE);
        ID3D10Predicate_Release(p);
    }
    DD_END(dev);
}
T(predication_value_normalised_observed)
{
    DD_DEV(dev);
    D3D10_QUERY_DESC d = { D3D10_QUERY_OCCLUSION_PREDICATE, 0 }; ID3D10Predicate *p = NULL, *g = NULL; BOOL val = 0;
    if (SUCCEEDED(ID3D10Device_CreatePredicate(dev, &d, &p))) {
        ID3D10Device_SetPredication(dev, p, 7);
        ID3D10Device_GetPredication(dev, &g, &val);
        t_note("SetPredication(.., 7) reads back %d", val);
        CHECK(val != 0);
        SAFE_RELEASE(g);
        ID3D10Device_SetPredication(dev, NULL, FALSE);
        ID3D10Predicate_Release(p);
    }
    DD_END(dev);
}
T(predication_cleared_by_clearstate)
{
    DD_DEV(dev);
    D3D10_QUERY_DESC d = { D3D10_QUERY_OCCLUSION_PREDICATE, 0 }; ID3D10Predicate *p = NULL, *g = (ID3D10Predicate *)(ULONG_PTR)1; BOOL val = 0xcc;
    if (SUCCEEDED(ID3D10Device_CreatePredicate(dev, &d, &p))) {
        ID3D10Device_SetPredication(dev, p, TRUE);
        ID3D10Device_ClearState(dev);
        ID3D10Device_GetPredication(dev, &g, &val);
        CHECK(g == NULL);
        EXPECT_EQ(val, FALSE);
        ID3D10Predicate_Release(p);
    }
    DD_END(dev);
}
T(text_filter_default_observed)
{
    DD_DEV(dev);
    UINT w = 0xdeadbeef, h = 0xdeadbeef;
    ID3D10Device_GetTextFilterSize(dev, &w, &h);
    t_note("default text filter size %u x %u", w, h);
    DD_END(dev);
}
T(text_filter_roundtrip_observed)
{
    DD_DEV(dev);
    UINT w = 0, h = 0;
    ID3D10Device_SetTextFilterSize(dev, 7, 9);
    ID3D10Device_GetTextFilterSize(dev, &w, &h);
    t_note("after Set(7,9): %u x %u", w, h);
    CHECK((w == 7 && h == 9) || (w == 0 && h == 0));
    ID3D10Device_SetTextFilterSize(dev, 0, 0);
    DD_END(dev);
}
T(text_filter_null_outputs)
{
    DD_DEV(dev);
    UINT v = 0;
    ID3D10Device_GetTextFilterSize(dev, NULL, NULL);
    ID3D10Device_GetTextFilterSize(dev, &v, NULL);
    ID3D10Device_GetTextFilterSize(dev, NULL, &v);
    DD_END(dev);
}
T(text_filter_zero_keeps_working)
{
    DD_DEV(dev);
    UINT w, h;
    ID3D10Device_SetTextFilterSize(dev, 0, 0);
    ID3D10Device_GetTextFilterSize(dev, &w, &h);
    EXPECT_EQ(w, 0); EXPECT_EQ(h, 0);
    DD_END(dev);
}
T(text_filter_cleared_by_clearstate_observed)
{
    DD_DEV(dev);
    UINT w = 0, h = 0;
    ID3D10Device_SetTextFilterSize(dev, 3, 3);
    ID3D10Device_ClearState(dev);
    ID3D10Device_GetTextFilterSize(dev, &w, &h);
    t_note("after ClearState: %u x %u", w, h);
    ID3D10Device_SetTextFilterSize(dev, 0, 0);
    DD_END(dev);
}
T(getter_default_state_all_null)
{
    DD_DEV(dev);
    ID3D10Buffer *b[8] = {0}; ID3D10ShaderResourceView *s[4] = {0}; ID3D10SamplerState *sm[4] = {0};
    ID3D10VertexShader *vs = (void *)1; ID3D10PixelShader *ps = (void *)1; ID3D10GeometryShader *gs = (void *)1;
    ID3D10Device_VSGetShader(dev, &vs); ID3D10Device_PSGetShader(dev, &ps); ID3D10Device_GSGetShader(dev, &gs);
    CHECK(!vs); CHECK(!ps); CHECK(!gs);
    ID3D10Device_VSGetConstantBuffers(dev, 0, 8, b);
    for (int i = 0; i < 8; i++) CHECK(!b[i]);
    ID3D10Device_PSGetShaderResources(dev, 0, 4, s);
    for (int i = 0; i < 4; i++) CHECK(!s[i]);
    ID3D10Device_PSGetSamplers(dev, 0, 4, sm);
    for (int i = 0; i < 4; i++) CHECK(!sm[i]);
    DD_END(dev);
}
