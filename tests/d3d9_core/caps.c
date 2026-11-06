/* IDirect3D9::GetDeviceCaps for HAL / REF / SW / NULLREF. */
#include "d9c.h"

#define N(a) (sizeof(a) / sizeof(*(a)))

static int caps_zero(const D3DCAPS9 *c) { const BYTE *p = (const BYTE *)c; for (size_t i = 0; i < sizeof *c; i++) if (p[i]) return 0; return 1; }

TEST(d3d9_core_caps_struct_size) {
    EXPECT_EQ(sizeof(D3DCAPS9), 304);
    EXPECT_EQ(offsetof(D3DCAPS9, MaxPointSize), 176);
    EXPECT_EQ(offsetof(D3DCAPS9, MaxTextureWidth), 88);
    EXPECT_EQ(offsetof(D3DCAPS9, PixelShaderVersion), 0xCC);
}
TEST(d3d9_core_caps_bad_adapter) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    UINT bad[] = { n, n + 1, 0xFFFFFFFF };
    for (unsigned i = 0; i < 3; i++) {
        D3DCAPS9 c; memset(&c, 0xA5, sizeof c);
        EXPECT_HR(IDirect3D9_GetDeviceCaps(d, bad[i], D3DDEVTYPE_HAL, &c), D9_INVALIDCALL);
        /* the buffer is not touched on this path */
        CHECK(((BYTE *)&c)[0] == 0xA5 && ((BYTE *)&c)[303] == 0xA5);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_bad_adapter_ex) {
    D9_GET_EX(d);
    UINT n = IDirect3D9Ex_GetAdapterCount(d);
    D3DCAPS9 c;
    EXPECT_HR(IDirect3D9Ex_GetDeviceCaps(d, n, D3DDEVTYPE_HAL, &c), D9_INVALIDCALL);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_caps_failure_zeroes_struct) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE tys[] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF, D3DDEVTYPE_SW, D3DDEVTYPE_NULLREF };
    for (unsigned i = 0; i < N(tys); i++) {
        D3DCAPS9 c; memset(&c, 0xA5, sizeof c);
        HRESULT hr = IDirect3D9_GetDeviceCaps(d, 0, tys[i], &c);
        /* Win10 22H2: the runtime answers HAL, REF and NULLREF itself; SW needs RegisterSoftwareDevice */
        CHECKF(hr == (tys[i] == D3DDEVTYPE_SW ? D9_NOTAVAILABLE : S_OK), "GetDeviceCaps devtype %u -> %s", tys[i], d3dh_hr_name(hr));
        if (FAILED(hr)) CHECKF(caps_zero(&c), "failed call (%s) must return zeroed caps for devtype %u", d3dh_hr_name(hr), tys[i]);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_invalid_devtypes_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DDEVTYPE bad[] = { (D3DDEVTYPE)0, (D3DDEVTYPE)5, (D3DDEVTYPE)0xFFFFFFFF };
    for (unsigned i = 0; i < N(bad); i++) {
        D3DCAPS9 c; memset(&c, 0xA5, sizeof c);
        HRESULT hr = IDirect3D9_GetDeviceCaps(d, 0, bad[i], &c);
        CHECKF(hr == D3DERR_INVALIDDEVICE, "devtype %u -> %s", bad[i], d3dh_hr_name(hr));
        CHECK(caps_zero(&c));
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_hal_identity_fields) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    EXPECT_EQ(c.DeviceType, D3DDEVTYPE_HAL);
    EXPECT_EQ(c.AdapterOrdinal, 0);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_ref_identity_fields) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_REF, &c))) { IDirect3D9_Release(d); t_skip("no REF caps"); return; }
    EXPECT_EQ(c.DeviceType, D3DDEVTYPE_REF);
    EXPECT_EQ(c.AdapterOrdinal, 0);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_adapter_ordinal_echo) {
    D9_GET(d);
    UINT n = IDirect3D9_GetAdapterCount(d);
    if (!n) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    for (UINT a = 0; a < n; a++) {
        D3DCAPS9 c; memset(&c, 0, sizeof c);
        if (SUCCEEDED(IDirect3D9_GetDeviceCaps(d, a, D3DDEVTYPE_HAL, &c))) EXPECT_EQ(c.AdapterOrdinal, a);
    }
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_hal_limits_invariants) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    CHECKF(c.MaxTextureWidth >= 2048, "MaxTextureWidth %lu", c.MaxTextureWidth);
    CHECKF(c.MaxTextureHeight >= 2048, "MaxTextureHeight %lu", c.MaxTextureHeight);
    CHECKF(c.MaxVolumeExtent >= 256, "MaxVolumeExtent %lu", c.MaxVolumeExtent);
    CHECKF(c.MaxPointSize >= 1.0f, "MaxPointSize %f (zero is mapped to 1.0)", c.MaxPointSize);
    CHECKF(c.MaxPrimitiveCount >= 0xFFFF, "MaxPrimitiveCount %lu", c.MaxPrimitiveCount);
    CHECKF(c.MaxVertexIndex >= 0xFFFF, "MaxVertexIndex 0x%lx", c.MaxVertexIndex);
    CHECKF(c.NumSimultaneousRTs >= 1, "NumSimultaneousRTs %lu", c.NumSimultaneousRTs);
    CHECKF(c.MaxStreams >= 1, "MaxStreams %lu", c.MaxStreams);
    CHECKF(c.MaxTextureBlendStages >= 1, "MaxTextureBlendStages %lu", c.MaxTextureBlendStages);
    CHECKF(c.MaxSimultaneousTextures >= 1, "MaxSimultaneousTextures %lu", c.MaxSimultaneousTextures);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_shader_version_encoding) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    if (c.VertexShaderVersion) CHECKF((c.VertexShaderVersion & 0xFFFF0000) == 0xFFFE0000, "VS 0x%lx", c.VertexShaderVersion);
    if (c.PixelShaderVersion) CHECKF((c.PixelShaderVersion & 0xFFFF0000) == 0xFFFF0000, "PS 0x%lx", c.PixelShaderVersion);
    CHECK(D3DVS_VERSION(3, 0) == 0xFFFE0300);
    CHECK(D3DPS_VERSION(3, 0) == 0xFFFF0300);
    t_note("VS version %lu.%lu PS version %lu.%lu", (c.VertexShaderVersion >> 8) & 0xFF, c.VertexShaderVersion & 0xFF, (c.PixelShaderVersion >> 8) & 0xFF, c.PixelShaderVersion & 0xFF);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_shader_versions_minor_range) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    DWORD vmaj = (c.VertexShaderVersion >> 8) & 0xFF, pmaj = (c.PixelShaderVersion >> 8) & 0xFF;
    CHECK(vmaj <= 3); CHECK(pmaj <= 3);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_shader30_implies_limits) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    if (c.PixelShaderVersion >= D3DPS_VERSION(3, 0)) {
        CHECKF(c.MaxPixelShader30InstructionSlots >= 512, "slots %lu", c.MaxPixelShader30InstructionSlots);
        CHECKF(c.PixelShader1xMaxValue >= 1.0f, "PS1xMax %f", c.PixelShader1xMaxValue);
    }
    if (c.VertexShaderVersion >= D3DVS_VERSION(2, 0)) CHECKF(c.MaxVertexShaderConst >= 256, "VS consts %lu", c.MaxVertexShaderConst);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_hal_flag_note) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
#define FL(f, m) t_note("HAL " #f " has " #m ": %d", !!(c.f & (m)))
    FL(Caps2, 0x00080000); FL(DevCaps, D3DDEVCAPS_DRAWPRIMTLVERTEX); FL(DevCaps, D3DDEVCAPS_HWRASTERIZATION);
    FL(PresentationIntervals, D3DPRESENT_INTERVAL_IMMEDIATE); FL(PresentationIntervals, D3DPRESENT_INTERVAL_ONE);
    FL(SrcBlendCaps, D3DPBLENDCAPS_SRCALPHA); FL(DestBlendCaps, D3DPBLENDCAPS_INVSRCALPHA);
    t_note("HAL TextureCaps 0x%lx ZCmpCaps 0x%lx StencilCaps 0x%lx", c.TextureCaps, c.ZCmpCaps, c.StencilCaps);
#undef FL
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_repeatable) {
    D9_GET(d);
    D3DCAPS9 a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &a))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_repeatable_ref) {
    D9_GET(d);
    D3DCAPS9 a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_REF, &a))) { IDirect3D9_Release(d); t_skip("no REF caps"); return; }
    IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_REF, &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_two_objects_agree) {
    D9_GET(a); D9_GET(b);
    D3DCAPS9 x, y; memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
    if (!IDirect3D9_GetAdapterCount(a) || FAILED(IDirect3D9_GetDeviceCaps(a, 0, D3DDEVTYPE_HAL, &x))) { IDirect3D9_Release(a); IDirect3D9_Release(b); t_skip("no HAL caps"); return; }
    IDirect3D9_GetDeviceCaps(b, 0, D3DDEVTYPE_HAL, &y);
    CHECK(!memcmp(&x, &y, sizeof x));
    IDirect3D9_Release(a); IDirect3D9_Release(b);
}
TEST(d3d9_core_caps_plain_vs_ex_note) {
    D9_GET(a); D9_GET_EX(b);
    D3DCAPS9 x, y; memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
    if (!IDirect3D9_GetAdapterCount(a) || FAILED(IDirect3D9_GetDeviceCaps(a, 0, D3DDEVTYPE_HAL, &x))) { IDirect3D9_Release(a); IDirect3D9Ex_Release(b); t_skip("no HAL caps"); return; }
    HRESULT hr = IDirect3D9Ex_GetDeviceCaps(b, 0, D3DDEVTYPE_HAL, &y);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr)) {
        int same = !memcmp(&x, &y, sizeof x);
        t_note("HAL caps identical on plain and Ex objects: %d", same);
        EXPECT_EQ(x.MaxTextureWidth, y.MaxTextureWidth);
        EXPECT_EQ(x.VertexShaderVersion, y.VertexShaderVersion);
        EXPECT_EQ(x.PixelShaderVersion, y.PixelShaderVersion);
    }
    IDirect3D9_Release(a); IDirect3D9Ex_Release(b);
}
TEST(d3d9_core_caps_ex_flags_note) {
    D9_GET_EX(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9Ex_GetAdapterCount(d) || FAILED(IDirect3D9Ex_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9Ex_Release(d); t_skip("no HAL caps"); return; }
    t_note("Caps2 0x%lx (D3DCAPS2_CANSHARERESOURCE=%d), DevCaps2 0x%lx, Caps3 0x%lx", c.Caps2, !!(c.Caps2 & 0x00001000), c.DevCaps2, c.Caps3);
    IDirect3D9Ex_Release(d);
}
TEST(d3d9_core_caps_full_table_note) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
#define F(x) t_note("HAL caps %-34s = 0x%08lx (%lu)", #x, (unsigned long)c.x, (unsigned long)c.x)
    F(Caps); F(Caps2); F(Caps3); F(PresentationIntervals); F(CursorCaps); F(DevCaps); F(PrimitiveMiscCaps); F(RasterCaps);
    F(ZCmpCaps); F(SrcBlendCaps); F(DestBlendCaps); F(AlphaCmpCaps); F(ShadeCaps); F(TextureCaps); F(TextureFilterCaps);
    F(CubeTextureFilterCaps); F(VolumeTextureFilterCaps); F(TextureAddressCaps); F(VolumeTextureAddressCaps); F(LineCaps);
    F(MaxTextureWidth); F(MaxTextureHeight); F(MaxVolumeExtent); F(MaxTextureRepeat); F(MaxTextureAspectRatio); F(MaxAnisotropy);
    F(StencilCaps); F(FVFCaps); F(TextureOpCaps); F(MaxTextureBlendStages); F(MaxSimultaneousTextures); F(VertexProcessingCaps);
    F(MaxActiveLights); F(MaxUserClipPlanes); F(MaxVertexBlendMatrices); F(MaxVertexBlendMatrixIndex); F(MaxPrimitiveCount);
    F(MaxVertexIndex); F(MaxStreams); F(MaxStreamStride); F(VertexShaderVersion); F(MaxVertexShaderConst); F(PixelShaderVersion);
    F(DevCaps2); F(MasterAdapterOrdinal); F(AdapterOrdinalInGroup); F(NumberOfAdaptersInGroup); F(DeclTypes); F(NumSimultaneousRTs);
    F(StretchRectFilterCaps); F(VS20Caps.Caps); F(VS20Caps.DynamicFlowControlDepth); F(VS20Caps.NumTemps); F(VS20Caps.StaticFlowControlDepth);
    F(PS20Caps.Caps); F(PS20Caps.DynamicFlowControlDepth); F(PS20Caps.NumTemps); F(PS20Caps.StaticFlowControlDepth); F(PS20Caps.NumInstructionSlots);
    F(VertexTextureFilterCaps); F(MaxVShaderInstructionsExecuted); F(MaxPShaderInstructionsExecuted); F(MaxVertexShader30InstructionSlots); F(MaxPixelShader30InstructionSlots);
#undef F
    t_note("HAL caps MaxVertexW=%f GuardBand=[%f %f %f %f] ExtentsAdjust=%f MaxPointSize=%f PS1xMaxValue=%f", c.MaxVertexW, c.GuardBandLeft, c.GuardBandTop, c.GuardBandRight, c.GuardBandBottom, c.ExtentsAdjust, c.MaxPointSize, c.PixelShader1xMaxValue);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_ref_full_table_note) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_REF, &c))) { IDirect3D9_Release(d); t_skip("no REF caps"); return; }
    t_note("REF caps: tex %lux%lu VS %lx PS %lx RTs %lu streams %lu", c.MaxTextureWidth, c.MaxTextureHeight, c.VertexShaderVersion, c.PixelShaderVersion, c.NumSimultaneousRTs, c.MaxStreams);
    CHECK(c.MaxPointSize >= 1.0f);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_sw_without_registration) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DCAPS9 c; memset(&c, 0xA5, sizeof c);
    HRESULT hr = IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_SW, &c);
    EXPECT_HR(hr, D9_NOTAVAILABLE);
    CHECK(caps_zero(&c));
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_nullref_note) {
    D9_GET(d);
    if (!IDirect3D9_GetAdapterCount(d)) { IDirect3D9_Release(d); t_skip("no adapters"); return; }
    D3DCAPS9 c; memset(&c, 0xA5, sizeof c);
    HRESULT hr = IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_NULLREF, &c);
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(c.DeviceType, D3DDEVTYPE_NULLREF); CHECK(c.MaxPointSize >= 1.0f);
    EXPECT_EQ(c.MaxTextureWidth, 8192);                       /* Win10 22H2 runtime-defined NULLREF limits */
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_msaa_consistent_with_checks) {
    /* If NumSimultaneousRTs says N, the caps are an upper bound that must itself be sane. */
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    CHECKF(c.NumSimultaneousRTs <= 4, "NumSimultaneousRTs %lu (D3D9 limit is 4)", c.NumSimultaneousRTs);
    CHECKF(c.MaxStreams <= 16, "MaxStreams %lu", c.MaxStreams);
    CHECKF(c.MaxUserClipPlanes <= 32, "clip planes %lu (D3DMAXUSERCLIPPLANES is 32)", c.MaxUserClipPlanes);
    CHECKF(c.MaxAnisotropy <= 16, "MaxAnisotropy %lu", c.MaxAnisotropy);
    IDirect3D9_Release(d);
}
TEST(d3d9_core_caps_vertex_texture_caps_consistent) {
    D9_GET(d);
    D3DCAPS9 c; memset(&c, 0, sizeof c);
    if (!IDirect3D9_GetAdapterCount(d) || FAILED(IDirect3D9_GetDeviceCaps(d, 0, D3DDEVTYPE_HAL, &c))) { IDirect3D9_Release(d); t_skip("no HAL caps"); return; }
    /* DX9 hardware reporting a vs_3_0 vertex shader must also report ps_3_0 or ps_2_x. */
    if (c.VertexShaderVersion >= D3DVS_VERSION(3, 0)) CHECK(c.PixelShaderVersion >= D3DPS_VERSION(2, 0));
    IDirect3D9_Release(d);
}
