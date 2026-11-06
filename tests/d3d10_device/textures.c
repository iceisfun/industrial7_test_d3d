/* D3D10 textures (1D/2D/3D) creation matrix + views (RTV/DSV/SRV) default descriptors and validation. */
#include "dd.h"

#define T(n) TEST(d3d10_device_tex_##n)
#define SR D3D10_BIND_SHADER_RESOURCE
#define RT D3D10_BIND_RENDER_TARGET
#define DS D3D10_BIND_DEPTH_STENCIL
#define DEF D3D10_USAGE_DEFAULT
#define IMM D3D10_USAGE_IMMUTABLE
#define DYN D3D10_USAGE_DYNAMIC
#define STG D3D10_USAGE_STAGING
#define CR D3D10_CPU_ACCESS_READ
#define CW D3D10_CPU_ACCESS_WRITE
#define RGBA DXGI_FORMAT_R8G8B8A8_UNORM
enum { W_OK, W_BAD, W_NOTE, W_LAX }; /* W_LAX: documented as invalid, but the Win10 WARP d3d10 device accepts it (probed) */

/* generous zero-filled initial data for immutable cases: RGBA8 / BC1 sized pitches, up to 16 subresources */
static void fill_sd(D3D10_SUBRESOURCE_DATA *sd, UINT n, UINT w, UINT h, UINT d, UINT mips, void *mem)
{
    for (UINT i = 0; i < n; i++) {
        UINT m = i % (mips ? mips : 1), mw = w >> m, mh = h >> m, md = d >> m;
        if (!mw) { mw = 1; } if (!mh) { mh = 1; } if (!md) { md = 1; }
        sd[i].pSysMem = mem; sd[i].SysMemPitch = mw * 4; sd[i].SysMemSlicePitch = mw * mh * 4;
        (void)md;
    }
}
static BYTE g_zero[1 << 20];

static void tex2d_case(UINT w, UINT h, UINT mips, UINT arr, DXGI_FORMAT f, UINT ms, UINT usage, UINT bind, UINT cpu, UINT misc, int want)
{
    D3D10_TEXTURE2D_DESC d = { w, h, mips, arr, f, { ms, 0 }, (D3D10_USAGE)usage, bind, cpu, misc }, g;
    D3D10_SUBRESOURCE_DATA sd[64];
    ID3D10Texture2D *t = NULL;
    HRESULT hrc, hrv;
    DD_DEV(dev);
    int lax = (want == W_LAX);
    if (lax) want = dd_warp() ? W_OK : W_BAD;
    UINT nsub = (mips ? mips : 1) * (arr ? arr : 1);
    if (nsub > 64) nsub = 64;
    fill_sd(sd, nsub, w, h, 1, mips, g_zero);
    int imm = usage == IMM;
    hrv = ID3D10Device_CreateTexture2D(dev, &d, imm ? sd : NULL, NULL);
    hrc = ID3D10Device_CreateTexture2D(dev, &d, imm ? sd : NULL, &t);
    if (want == W_NOTE) {
        t_note("%ux%u mips %u arr %u fmt %u ms %u usage %u bind 0x%x cpu 0x%x misc 0x%x: validate %s create %s", w, h, mips, arr, f, ms, usage, bind, cpu, misc, d3dh_hr_name(hrv), d3dh_hr_name(hrc));
    } else if (want == W_OK) {
        EXPECT_HR(hrv, S_FALSE);
        EXPECT_HR(hrc, S_OK);
        CHECK(t != NULL);
        if (t) {
            memset(&g, 0xcc, sizeof g);
            ID3D10Texture2D_GetDesc(t, &g);
            EXPECT_EQ(g.Width, w); EXPECT_EQ(g.Height, h); EXPECT_EQ(g.ArraySize, arr); EXPECT_EQ(g.Format, f);
            EXPECT_EQ(g.SampleDesc.Count, ms); EXPECT_EQ(g.SampleDesc.Quality, 0);
            EXPECT_EQ(g.Usage, usage); EXPECT_EQ(g.BindFlags, bind); EXPECT_EQ(g.CPUAccessFlags, cpu); EXPECT_EQ(g.MiscFlags, lax ? 0 : misc); /* an accepted-but-meaningless TEXTURECUBE flag is dropped (probed) */
            if (mips) EXPECT_EQ(g.MipLevels, mips); else CHECK(g.MipLevels >= 1);
        }
    } else {
        EXPECT_HR(hrc, E_INVALIDARG);
        EXPECT_HR(hrv, E_INVALIDARG);
        CHECK(t == NULL);
    }
    SAFE_RELEASE(t);
    DD_END(dev);
}
#define C2(nm, w, h, mips, arr, f, ms, usage, bind, cpu, misc, want) T(t2d_##nm) { tex2d_case(w, h, mips, arr, f, ms, usage, bind, cpu, misc, want); }

C2(def_srv, 16, 16, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(def_rt, 16, 16, 1, 1, RGBA, 1, DEF, RT, 0, 0, W_OK)
C2(def_rt_srv, 16, 16, 1, 1, RGBA, 1, DEF, RT | SR, 0, 0, W_OK)
C2(def_ds_d24s8, 16, 16, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, 1, DEF, DS, 0, 0, W_OK)
C2(def_ds_d32, 16, 16, 1, 1, DXGI_FORMAT_D32_FLOAT, 1, DEF, DS, 0, 0, W_OK)
C2(def_ds_d16, 16, 16, 1, 1, DXGI_FORMAT_D16_UNORM, 1, DEF, DS, 0, 0, W_OK)
C2(def_ds_srv_typeless, 16, 16, 1, 1, DXGI_FORMAT_R24G8_TYPELESS, 1, DEF, DS | SR, 0, 0, W_OK)
C2(def_ds_srv_typeless32, 16, 16, 1, 1, DXGI_FORMAT_R32_TYPELESS, 1, DEF, DS | SR, 0, 0, W_OK)
C2(def_ds_srv_typeless16, 16, 16, 1, 1, DXGI_FORMAT_R16_TYPELESS, 1, DEF, DS | SR, 0, 0, W_OK)
C2(def_typeless_rt, 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, 1, DEF, RT | SR, 0, 0, W_OK)
C2(def_srv_float, 16, 16, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, DEF, SR, 0, 0, W_OK)
C2(def_bind0, 16, 16, 1, 1, RGBA, 1, DEF, 0, 0, 0, W_NOTE)
C2(dyn_srv, 16, 16, 1, 1, RGBA, 1, DYN, SR, CW, 0, W_OK)
C2(stg_read, 16, 16, 1, 1, RGBA, 1, STG, 0, CR, 0, W_OK)
C2(stg_write, 16, 16, 1, 1, RGBA, 1, STG, 0, CW, 0, W_OK)
C2(stg_rw, 16, 16, 1, 1, RGBA, 1, STG, 0, CR | CW, 0, W_OK)
C2(imm_srv, 16, 16, 1, 1, RGBA, 1, IMM, SR, 0, 0, W_OK)
C2(imm_srv_mips, 16, 16, 5, 1, RGBA, 1, IMM, SR, 0, 0, W_OK)
C2(imm_srv_array, 8, 8, 1, 3, RGBA, 1, IMM, SR, 0, 0, W_OK)
C2(size_1x1, 1, 1, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(size_1x1_auto_mips, 1, 1, 0, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(size_8192_x1, 8192, 1, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(size_1_x8192, 1, 8192, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(size_8193_x1, 8193, 1, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_LAX)
C2(size_1_x8193, 1, 8193, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_LAX)
C2(size_non_pow2, 100, 60, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(auto_mips_non_pow2, 100, 60, 0, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(mips_7_ok_100x60, 100, 60, 7, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(mips_8_bad_100x60, 100, 60, 8, 1, RGBA, 1, DEF, SR, 0, 0, W_BAD)
C2(mips_5_ok_16, 16, 16, 5, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(mips_6_bad_16, 16, 16, 6, 1, RGBA, 1, DEF, SR, 0, 0, W_BAD)
C2(mips_2_bad_1x1, 1, 1, 2, 1, RGBA, 1, DEF, SR, 0, 0, W_BAD)
C2(mips_3_ok_4x2, 4, 2, 3, 1, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(mips_4_bad_4x2, 4, 2, 4, 1, RGBA, 1, DEF, SR, 0, 0, W_BAD)
C2(width0, 0, 16, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_BAD)
C2(height0, 16, 0, 1, 1, RGBA, 1, DEF, SR, 0, 0, W_BAD)
C2(array0, 16, 16, 1, 0, RGBA, 1, DEF, SR, 0, 0, W_BAD)
C2(array_512, 1, 1, 1, 512, RGBA, 1, DEF, SR, 0, 0, W_OK)
C2(array_513, 1, 1, 1, 513, RGBA, 1, DEF, SR, 0, 0, W_BAD)
C2(array_2_rt, 16, 16, 1, 2, RGBA, 1, DEF, RT | SR, 0, 0, W_OK)
C2(array_ds, 16, 16, 1, 3, DXGI_FORMAT_D32_FLOAT, 1, DEF, DS, 0, 0, W_OK)
C2(format_unknown, 16, 16, 1, 1, DXGI_FORMAT_UNKNOWN, 1, DEF, SR, 0, 0, W_BAD)
C2(format_out_of_range, 16, 16, 1, 1, 200, 1, DEF, SR, 0, 0, W_BAD)
C2(format_ffff, 16, 16, 1, 1, 0xffff, 1, DEF, SR, 0, 0, W_BAD)
C2(ds_format_with_rt, 16, 16, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, 1, DEF, DS | RT, 0, 0, W_BAD)
C2(ds_on_color_format, 16, 16, 1, 1, RGBA, 1, DEF, DS, 0, 0, W_BAD)
C2(ds_format_srv_without_typeless, 16, 16, 1, 1, DXGI_FORMAT_D32_FLOAT, 1, DEF, DS | SR, 0, 0, W_NOTE)
C2(bc1_srv, 8, 8, 1, 1, DXGI_FORMAT_BC1_UNORM, 1, DEF, SR, 0, 0, W_OK)
C2(bc1_rt, 8, 8, 1, 1, DXGI_FORMAT_BC1_UNORM, 1, DEF, RT, 0, 0, W_BAD)
C2(bc1_ds, 8, 8, 1, 1, DXGI_FORMAT_BC1_UNORM, 1, DEF, DS, 0, 0, W_BAD)
C2(bc3_srv_mips, 16, 16, 5, 1, DXGI_FORMAT_BC3_UNORM, 1, DEF, SR, 0, 0, W_OK)
C2(bc1_non_multiple_of_4, 6, 6, 1, 1, DXGI_FORMAT_BC1_UNORM, 1, DEF, SR, 0, 0, W_NOTE)
C2(bc1_2x2, 2, 2, 1, 1, DXGI_FORMAT_BC1_UNORM, 1, DEF, SR, 0, 0, W_NOTE)
C2(bc1_staging, 8, 8, 1, 1, DXGI_FORMAT_BC1_UNORM, 1, STG, 0, CR, 0, W_OK)
/* usage rules */
C2(usage_99, 16, 16, 1, 1, RGBA, 1, 99, SR, 0, 0, W_BAD)
C2(imm_cpu_write, 16, 16, 1, 1, RGBA, 1, IMM, SR, CW, 0, W_BAD)
C2(imm_cpu_read, 16, 16, 1, 1, RGBA, 1, IMM, SR, CR, 0, W_BAD)
C2(def_cpu_write, 16, 16, 1, 1, RGBA, 1, DEF, SR, CW, 0, W_LAX)
C2(def_cpu_read, 16, 16, 1, 1, RGBA, 1, DEF, SR, CR, 0, W_LAX)
C2(dyn_cpu0, 16, 16, 1, 1, RGBA, 1, DYN, SR, 0, 0, W_BAD)
C2(dyn_cpu_read, 16, 16, 1, 1, RGBA, 1, DYN, SR, CR, 0, W_BAD)
C2(dyn_rt, 16, 16, 1, 1, RGBA, 1, DYN, RT, CW, 0, W_BAD)
C2(dyn_ds, 16, 16, 1, 1, DXGI_FORMAT_D32_FLOAT, 1, DYN, DS, CW, 0, W_BAD)
C2(dyn_mips, 16, 16, 5, 1, RGBA, 1, DYN, SR, CW, 0, W_NOTE)
C2(dyn_array, 16, 16, 1, 2, RGBA, 1, DYN, SR, CW, 0, W_NOTE)
C2(stg_bind_srv, 16, 16, 1, 1, RGBA, 1, STG, SR, CR, 0, W_BAD)
C2(stg_bind_rt, 16, 16, 1, 1, RGBA, 1, STG, RT, CR, 0, W_BAD)
C2(stg_cpu0, 16, 16, 1, 1, RGBA, 1, STG, 0, 0, 0, W_BAD)
C2(usage_5, 16, 16, 1, 1, RGBA, 1, 5, SR, 0, 0, W_BAD)
C2(bind_undefined_bit, 16, 16, 1, 1, RGBA, 1, DEF, 0x80000000u, 0, 0, W_BAD)
C2(bind_vertex_buffer, 16, 16, 1, 1, RGBA, 1, DEF, D3D10_BIND_VERTEX_BUFFER, 0, 0, W_BAD)
C2(bind_constant_buffer, 16, 16, 1, 1, RGBA, 1, DEF, D3D10_BIND_CONSTANT_BUFFER, 0, 0, W_BAD)
C2(bind_index_buffer, 16, 16, 1, 1, RGBA, 1, DEF, D3D10_BIND_INDEX_BUFFER, 0, 0, W_BAD)
C2(bind_stream_output, 16, 16, 1, 1, RGBA, 1, DEF, D3D10_BIND_STREAM_OUTPUT, 0, 0, W_BAD)
C2(cpu_undefined_bit, 16, 16, 1, 1, RGBA, 1, STG, 0, 0x40000 | CR, 0, W_BAD)
C2(misc_undefined_bit, 16, 16, 1, 1, RGBA, 1, DEF, SR, 0, 0x80000000u, W_BAD)
/* misc flags */
C2(cube_ok, 8, 8, 1, 6, RGBA, 1, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_OK)
C2(cube_mips, 8, 8, 4, 6, RGBA, 1, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_OK)
C2(cube_rt_srv_genmips, 8, 8, 0, 6, RGBA, 1, DEF, SR | RT, 0, D3D10_RESOURCE_MISC_TEXTURECUBE | D3D10_RESOURCE_MISC_GENERATE_MIPS, W_OK)
C2(cube_array1, 8, 8, 1, 1, RGBA, 1, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_BAD)
C2(cube_array5, 8, 8, 1, 5, RGBA, 1, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_BAD)
C2(cube_array7, 8, 8, 1, 7, RGBA, 1, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_BAD)
C2(cube_non_square, 8, 4, 1, 6, RGBA, 1, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_BAD)
C2(cube_ms, 8, 8, 1, 6, RGBA, 4, DEF, RT, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_NOTE)
C2(cube_stg, 8, 8, 1, 6, RGBA, 1, STG, 0, CR, D3D10_RESOURCE_MISC_TEXTURECUBE, W_NOTE)
C2(genmips_ok, 16, 16, 0, 1, RGBA, 1, DEF, RT | SR, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS, W_OK)
C2(genmips_srv_only, 16, 16, 0, 1, RGBA, 1, DEF, SR, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS, W_BAD)
C2(genmips_rt_only, 16, 16, 0, 1, RGBA, 1, DEF, RT, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS, W_BAD)
C2(genmips_imm, 16, 16, 1, 1, RGBA, 1, IMM, SR, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS, W_BAD)
C2(genmips_stg, 16, 16, 1, 1, RGBA, 1, STG, 0, CR, D3D10_RESOURCE_MISC_GENERATE_MIPS, W_BAD)
C2(shared, 16, 16, 1, 1, RGBA, 1, DEF, RT | SR, 0, D3D10_RESOURCE_MISC_SHARED, W_NOTE)
C2(gdi_compatible, 16, 16, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, 1, DEF, RT, 0, D3D10_RESOURCE_MISC_GDI_COMPATIBLE, W_NOTE)
C2(texture_cube_raw_flag, 16, 16, 1, 1, RGBA, 1, DEF, SR, 0, 0x20, W_NOTE)
/* multisampling */
C2(ms2_rt, 16, 16, 1, 1, RGBA, 2, DEF, RT, 0, 0, W_NOTE)
C2(ms4_rt, 16, 16, 1, 1, RGBA, 4, DEF, RT, 0, 0, W_NOTE)
C2(ms4_rt_srv, 16, 16, 1, 1, RGBA, 4, DEF, RT | SR, 0, 0, W_NOTE)
C2(ms4_ds, 16, 16, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, 4, DEF, DS, 0, 0, W_NOTE)
C2(ms4_mips_bad, 16, 16, 5, 1, RGBA, 4, DEF, RT, 0, 0, W_BAD)
C2(ms4_auto_mips_bad, 16, 16, 0, 1, RGBA, 4, DEF, RT, 0, 0, W_BAD)
C2(ms4_dynamic_bad, 16, 16, 1, 1, RGBA, 4, DYN, SR, CW, 0, W_LAX)
C2(ms4_staging_bad, 16, 16, 1, 1, RGBA, 4, STG, 0, CR, 0, W_BAD)
C2(ms4_immutable_bad, 16, 16, 1, 1, RGBA, 4, IMM, SR, 0, 0, W_BAD)
C2(ms_count0, 16, 16, 1, 1, RGBA, 0, DEF, RT, 0, 0, W_NOTE)
C2(ms_count_3, 16, 16, 1, 1, RGBA, 3, DEF, RT, 0, 0, W_NOTE)
C2(ms_count_33, 16, 16, 1, 1, RGBA, 33, DEF, RT, 0, 0, W_NOTE)
C2(ms_count_big, 16, 16, 1, 1, RGBA, 0xffffffffu, DEF, RT, 0, 0, W_NOTE)

T(t2d_null_desc)
{
    DD_DEV(dev);
    ID3D10Texture2D *t = (ID3D10Texture2D *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, NULL, NULL, &t), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, NULL, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(t2d_immutable_without_data)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 8, 8, 1, 1, RGBA, { 1, 0 }, IMM, SR, 0, 0 };
    ID3D10Texture2D *t = (ID3D10Texture2D *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &d, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(t2d_initial_data_null_sysmem)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 8, 8, 1, 1, RGBA, { 1, 0 }, DEF, SR, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd = { NULL, 32, 0 };
    ID3D10Texture2D *t = (ID3D10Texture2D *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &d, &sd, &t), E_INVALIDARG);
    DD_END(dev);
}
T(t2d_initial_data_ms_bad)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 8, 8, 1, 1, RGBA, { 4, 0 }, DEF, RT, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd = { g_zero, 32, 0 };
    ID3D10Texture2D *t = (ID3D10Texture2D *)(ULONG_PTR)1;
    HRESULT hr = ID3D10Device_CreateTexture2D(dev, &d, &sd, &t);
    EXPECT_HR(hr, E_INVALIDARG); /* Win10: probed */
    t_note("MS texture with initial data: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && t != (void *)1) ID3D10Texture2D_Release(t);
    CHECK(FAILED(hr));
    DD_END(dev);
}
T(t2d_auto_mip_count_resolved)
{
    static const struct { UINT w, h, mips; } c[] = { { 1, 1, 1 }, { 2, 2, 2 }, { 16, 16, 5 }, { 256, 64, 9 }, { 100, 60, 7 }, { 8, 1, 4 }, { 1, 8, 4 }, { 33, 17, 6 }, { 1024, 1024, 11 } };
    DD_DEV(dev);
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        D3D10_TEXTURE2D_DESC d = { c[i].w, c[i].h, 0, 1, RGBA, { 1, 0 }, DEF, SR, 0, 0 }, g; ID3D10Texture2D *t = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t));
        if (t) {
            ID3D10Texture2D_GetDesc(t, &g);
            t_check(g.MipLevels == c[i].mips, __FILE__, __LINE__, "auto mips", "%ux%u: got %u want %u", c[i].w, c[i].h, g.MipLevels, c[i].mips);
            ID3D10Texture2D_Release(t);
        }
    }
    DD_END(dev);
}
T(t2d_full_mip_chain_maximum_is_creatable)
{
    DD_DEV(dev);
    for (UINT s = 1; s <= 512; s <<= 1) {
        UINT mips = 1; for (UINT v = s; v > 1; v >>= 1) mips++;
        D3D10_TEXTURE2D_DESC d = { s, s, mips, 1, RGBA, { 1, 0 }, DEF, SR, 0, 0 }, e; ID3D10Texture2D *t = NULL, *t2 = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t));
        e = d; e.MipLevels = mips + 1;
        EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &e, NULL, &t2), E_INVALIDARG);
        SAFE_RELEASE(t); SAFE_RELEASE(t2);
    }
    DD_END(dev);
}
T(t2d_get_type_and_device)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 8, 8, 1, 1, RGBA, { 1, 0 }, DEF, SR, 0, 0 };
    ID3D10Texture2D *t = NULL; D3D10_RESOURCE_DIMENSION dim = 0; ID3D10Device *got = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t));
    if (t) {
        ID3D10Texture2D_GetType(t, &dim); EXPECT_EQ(dim, D3D10_RESOURCE_DIMENSION_TEXTURE2D);
        ID3D10Texture2D_GetDevice(t, &got); CHECK(got == dev); SAFE_RELEASE(got);
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(t2d_qi_matrix)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 8, 8, 1, 1, RGBA, { 1, 0 }, DEF, RT | SR, 0, 0 };
    ID3D10Texture2D *t = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t));
    if (t) {
        IUnknown *u = (IUnknown *)t; void *p;
        static const IID *yes[] = { &IID_IUnknown, &IID_ID3D10Texture2D, &IID_ID3D10Resource, &IID_ID3D10DeviceChild, &IID_IDXGIObject, &IID_IDXGIDeviceSubObject, &IID_IDXGIResource, &IID_IDXGISurface };
        for (unsigned i = 0; i < sizeof yes / sizeof *yes; i++) {
            p = NULL; HRESULT hr = IUnknown_QueryInterface(u, yes[i], &p);
            t_check(hr == S_OK, __FILE__, __LINE__, "tex2d QI", "%s: %s", d3dh_guid_str(yes[i]), d3dh_hr_name(hr));
            if (p) IUnknown_Release((IUnknown *)p);
        }
        static const IID *no[] = { &IID_ID3D10Buffer, &IID_ID3D10Texture1D, &IID_ID3D10Texture3D, &IID_ID3D10Device };
        for (unsigned i = 0; i < sizeof no / sizeof *no; i++) {
            p = (void *)1; HRESULT hr = IUnknown_QueryInterface(u, no[i], &p);
            t_check(hr == E_NOINTERFACE && !p, __FILE__, __LINE__, "tex2d QI neg", "%s: %s", d3dh_guid_str(no[i]), d3dh_hr_name(hr));
            if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
T(t2d_dxgi_surface_desc)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 24, 12, 1, 1, RGBA, { 1, 0 }, DEF, RT, 0, 0 };
    ID3D10Texture2D *t = NULL; IDXGISurface *sf = NULL; DXGI_SURFACE_DESC sd;
    EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t));
    if (t && SUCCEEDED(ID3D10Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&sf))) {
        EXPECT_HR_OK(IDXGISurface_GetDesc(sf, &sd));
        EXPECT_EQ(sd.Width, 24); EXPECT_EQ(sd.Height, 12); EXPECT_EQ(sd.Format, RGBA); EXPECT_EQ(sd.SampleDesc.Count, 1);
        IDXGISurface_Release(sf);
    }
    SAFE_RELEASE(t);
    DD_END(dev);
}
T(t2d_dxgi_resource_usage_observed)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 8, 8, 1, 1, RGBA, { 1, 0 }, DEF, RT | SR, 0, 0 };
    ID3D10Texture2D *t = NULL; IDXGIResource *r = NULL; DXGI_USAGE u = 0; UINT pr = 0;
    EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t));
    if (t && SUCCEEDED(ID3D10Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r))) {
        HRESULT hr = IDXGIResource_GetUsage(r, &u);
        t_note("IDXGIResource::GetUsage %s 0x%x", d3dh_hr_name(hr), u);
        EXPECT_HR_OK(IDXGIResource_GetEvictionPriority(r, &pr));
        EXPECT_EQ(pr, DXGI_RESOURCE_PRIORITY_NORMAL);
        IDXGIResource_Release(r);
    }
    SAFE_RELEASE(t);
    DD_END(dev);
}
T(t2d_eviction_priority_dxgi_shared)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC d = { 8, 8, 1, 1, RGBA, { 1, 0 }, DEF, SR, 0, 0 };
    ID3D10Texture2D *t = NULL; IDXGIResource *r = NULL; UINT pr = 0;
    EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t));
    if (t && SUCCEEDED(ID3D10Texture2D_QueryInterface(t, &IID_IDXGIResource, (void **)&r))) {
        ID3D10Texture2D_SetEvictionPriority(t, DXGI_RESOURCE_PRIORITY_HIGH);
        EXPECT_HR_OK(IDXGIResource_GetEvictionPriority(r, &pr));
        EXPECT_EQ(pr, DXGI_RESOURCE_PRIORITY_HIGH);
        EXPECT_HR_OK(IDXGIResource_SetEvictionPriority(r, DXGI_RESOURCE_PRIORITY_LOW));
        EXPECT_EQ(ID3D10Texture2D_GetEvictionPriority(t), DXGI_RESOURCE_PRIORITY_LOW);
        IDXGIResource_Release(r);
    }
    SAFE_RELEASE(t);
    DD_END(dev);
}

/* ---- 1D ---- */
static void tex1d_case(UINT w, UINT mips, UINT arr, DXGI_FORMAT f, UINT usage, UINT bind, UINT cpu, UINT misc, int want)
{
    D3D10_TEXTURE1D_DESC d = { w, mips, arr, f, (D3D10_USAGE)usage, bind, cpu, misc }, g;
    D3D10_SUBRESOURCE_DATA sd[64];
    ID3D10Texture1D *t = NULL; HRESULT hrc, hrv;
    DD_DEV(dev);
    int lax = (want == W_LAX);
    if (lax) want = dd_warp() ? W_OK : W_BAD;
    UINT nsub = (mips ? mips : 1) * (arr ? arr : 1); if (nsub > 64) nsub = 64;
    fill_sd(sd, nsub, w, 1, 1, mips, g_zero);
    hrv = ID3D10Device_CreateTexture1D(dev, &d, usage == IMM ? sd : NULL, NULL);
    hrc = ID3D10Device_CreateTexture1D(dev, &d, usage == IMM ? sd : NULL, &t);
    if (want == W_NOTE) t_note("1D w %u mips %u arr %u fmt %u usage %u bind 0x%x: %s", w, mips, arr, f, usage, bind, d3dh_hr_name(hrc));
    else if (want == W_OK) {
        EXPECT_HR(hrv, S_FALSE); EXPECT_HR(hrc, S_OK);
        if (t) {
            ID3D10Texture1D_GetDesc(t, &g);
            EXPECT_EQ(g.Width, w); EXPECT_EQ(g.ArraySize, arr); EXPECT_EQ(g.Format, f); EXPECT_EQ(g.Usage, usage);
            EXPECT_EQ(g.BindFlags, bind); EXPECT_EQ(g.CPUAccessFlags, cpu); EXPECT_EQ(g.MiscFlags, lax ? 0 : misc); /* an accepted-but-meaningless TEXTURECUBE flag is dropped (probed) */
            if (mips) EXPECT_EQ(g.MipLevels, mips);
        }
    } else { EXPECT_HR(hrc, E_INVALIDARG); EXPECT_HR(hrv, E_INVALIDARG); CHECK(t == NULL); }
    SAFE_RELEASE(t);
    DD_END(dev);
}
#define C1(nm, w, mips, arr, f, usage, bind, cpu, misc, want) T(t1d_##nm) { tex1d_case(w, mips, arr, f, usage, bind, cpu, misc, want); }
C1(def_srv, 16, 1, 1, RGBA, DEF, SR, 0, 0, W_OK)
C1(def_rt, 16, 1, 1, RGBA, DEF, RT, 0, 0, W_OK)
C1(def_rt_srv_genmips, 16, 0, 1, RGBA, DEF, RT | SR, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS, W_OK)
C1(def_ds, 16, 1, 1, DXGI_FORMAT_D32_FLOAT, DEF, DS, 0, 0, W_NOTE)
C1(w1, 1, 1, 1, RGBA, DEF, SR, 0, 0, W_OK)
C1(w8192, 8192, 1, 1, RGBA, DEF, SR, 0, 0, W_OK)
C1(w8193, 8193, 1, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C1(w0, 0, 1, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C1(array0, 16, 1, 0, RGBA, DEF, SR, 0, 0, W_BAD)
C1(array512, 2, 1, 512, RGBA, DEF, SR, 0, 0, W_OK)
C1(array513, 2, 1, 513, RGBA, DEF, SR, 0, 0, W_BAD)
C1(mips_5_of_16, 16, 5, 1, RGBA, DEF, SR, 0, 0, W_OK)
C1(mips_6_of_16, 16, 6, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C1(mips_auto, 16, 0, 1, RGBA, DEF, SR, 0, 0, W_OK)
C1(dyn_srv, 16, 1, 1, RGBA, DYN, SR, CW, 0, W_OK)
C1(stg_cr, 16, 1, 1, RGBA, STG, 0, CR, 0, W_OK)
C1(imm_srv, 16, 1, 1, RGBA, IMM, SR, 0, 0, W_OK)
C1(imm_cpu, 16, 1, 1, RGBA, IMM, SR, CR, 0, W_BAD)
C1(def_cpu_write, 16, 1, 1, RGBA, DEF, SR, CW, 0, W_LAX)
C1(unknown_format, 16, 1, 1, DXGI_FORMAT_UNKNOWN, DEF, SR, 0, 0, W_BAD)
C1(cube_misc, 16, 1, 6, RGBA, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_LAX)
C1(bc1, 16, 1, 1, DXGI_FORMAT_BC1_UNORM, DEF, SR, 0, 0, W_NOTE)
C1(bind_vb, 16, 1, 1, RGBA, DEF, D3D10_BIND_VERTEX_BUFFER, 0, 0, W_BAD)
T(t1d_null_desc)
{
    DD_DEV(dev);
    EXPECT_HR(ID3D10Device_CreateTexture1D(dev, NULL, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(t1d_auto_mips_resolved)
{
    DD_DEV(dev);
    D3D10_TEXTURE1D_DESC d = { 256, 0, 1, RGBA, DEF, SR, 0, 0 }, g; ID3D10Texture1D *t = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateTexture1D(dev, &d, NULL, &t));
    if (t) { ID3D10Texture1D_GetDesc(t, &g); EXPECT_EQ(g.MipLevels, 9); D3D10_RESOURCE_DIMENSION dim; ID3D10Texture1D_GetType(t, &dim); EXPECT_EQ(dim, D3D10_RESOURCE_DIMENSION_TEXTURE1D); ID3D10Texture1D_Release(t); }
    DD_END(dev);
}
T(t1d_qi_not_surface)
{
    DD_DEV(dev);
    D3D10_TEXTURE1D_DESC d = { 16, 1, 1, RGBA, DEF, SR, 0, 0 }; ID3D10Texture1D *t = NULL; void *p = (void *)1;
    EXPECT_HR_OK(ID3D10Device_CreateTexture1D(dev, &d, NULL, &t));
    if (t) {
        EXPECT_HR(ID3D10Texture1D_QueryInterface(t, &IID_ID3D10Texture2D, &p), E_NOINTERFACE);
        p = NULL;
        EXPECT_HR_OK(ID3D10Texture1D_QueryInterface(t, &IID_ID3D10Resource, &p));
        if (p) IUnknown_Release((IUnknown *)p);
        ID3D10Texture1D_Release(t);
    }
    DD_END(dev);
}

/* ---- 3D ---- */
static void tex3d_case(UINT w, UINT h, UINT dp, UINT mips, DXGI_FORMAT f, UINT usage, UINT bind, UINT cpu, UINT misc, int want)
{
    D3D10_TEXTURE3D_DESC d = { w, h, dp, mips, f, (D3D10_USAGE)usage, bind, cpu, misc }, g;
    D3D10_SUBRESOURCE_DATA sd[16];
    ID3D10Texture3D *t = NULL; HRESULT hrc, hrv;
    DD_DEV(dev);
    int lax = (want == W_LAX);
    if (lax) want = dd_warp() ? W_OK : W_BAD;
    UINT n = mips ? mips : 1; if (n > 16) n = 16;
    fill_sd(sd, n, w, h, dp, mips, g_zero);
    hrv = ID3D10Device_CreateTexture3D(dev, &d, usage == IMM ? sd : NULL, NULL);
    hrc = ID3D10Device_CreateTexture3D(dev, &d, usage == IMM ? sd : NULL, &t);
    if (want == W_NOTE) t_note("3D %ux%ux%u mips %u fmt %u usage %u bind 0x%x: %s", w, h, dp, mips, f, usage, bind, d3dh_hr_name(hrc));
    else if (want == W_OK) {
        EXPECT_HR(hrv, S_FALSE); EXPECT_HR(hrc, S_OK);
        if (t) {
            ID3D10Texture3D_GetDesc(t, &g);
            EXPECT_EQ(g.Width, w); EXPECT_EQ(g.Height, h); EXPECT_EQ(g.Depth, dp); EXPECT_EQ(g.Format, f); EXPECT_EQ(g.Usage, usage);
            EXPECT_EQ(g.BindFlags, bind); EXPECT_EQ(g.CPUAccessFlags, cpu); EXPECT_EQ(g.MiscFlags, lax ? 0 : misc); /* an accepted-but-meaningless TEXTURECUBE flag is dropped (probed) */
            if (mips) EXPECT_EQ(g.MipLevels, mips);
        }
    } else { EXPECT_HR(hrc, E_INVALIDARG); EXPECT_HR(hrv, E_INVALIDARG); CHECK(t == NULL); }
    SAFE_RELEASE(t);
    DD_END(dev);
}
#define C3(nm, w, h, dp, mips, f, usage, bind, cpu, misc, want) T(t3d_##nm) { tex3d_case(w, h, dp, mips, f, usage, bind, cpu, misc, want); }
C3(def_srv, 8, 8, 8, 1, RGBA, DEF, SR, 0, 0, W_OK)
C3(def_rt, 8, 8, 8, 1, RGBA, DEF, RT, 0, 0, W_OK)
C3(def_rt_srv_genmips, 8, 8, 8, 0, RGBA, DEF, RT | SR, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS, W_OK)
C3(mips_4_ok, 8, 8, 8, 4, RGBA, DEF, SR, 0, 0, W_OK)
C3(mips_5_bad, 8, 8, 8, 5, RGBA, DEF, SR, 0, 0, W_BAD)
C3(mips_depth_limited, 8, 8, 2, 4, RGBA, DEF, SR, 0, 0, W_OK)
C3(non_cubic, 16, 4, 2, 1, RGBA, DEF, SR, 0, 0, W_OK)
C3(w2048, 2048, 1, 1, 1, RGBA, DEF, SR, 0, 0, W_OK)
C3(w2049, 2049, 1, 1, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C3(h2049, 1, 2049, 1, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C3(d2049, 1, 1, 2049, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C3(d2048, 1, 1, 2048, 1, RGBA, DEF, SR, 0, 0, W_OK)
C3(w0, 0, 8, 8, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C3(h0, 8, 0, 8, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C3(d0, 8, 8, 0, 1, RGBA, DEF, SR, 0, 0, W_BAD)
C3(ds_bind, 8, 8, 8, 1, DXGI_FORMAT_D32_FLOAT, DEF, DS, 0, 0, W_BAD)
C3(unknown_format, 8, 8, 8, 1, DXGI_FORMAT_UNKNOWN, DEF, SR, 0, 0, W_BAD)
C3(dyn_srv, 8, 8, 8, 1, RGBA, DYN, SR, CW, 0, W_OK)
C3(stg_cr, 8, 8, 8, 1, RGBA, STG, 0, CR, 0, W_OK)
C3(imm_srv, 8, 8, 8, 1, RGBA, IMM, SR, 0, 0, W_OK)
C3(imm_cpu, 8, 8, 8, 1, RGBA, IMM, SR, CW, 0, W_BAD)
C3(def_cpu, 8, 8, 8, 1, RGBA, DEF, SR, CR, 0, W_LAX)
C3(cube_misc, 8, 8, 8, 1, RGBA, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_LAX)
C3(bc1, 8, 8, 4, 1, DXGI_FORMAT_BC1_UNORM, DEF, SR, 0, 0, W_NOTE)
C3(bc1_rt, 8, 8, 4, 1, DXGI_FORMAT_BC1_UNORM, DEF, RT, 0, 0, W_BAD)
T(t3d_null_desc)
{
    DD_DEV(dev);
    EXPECT_HR(ID3D10Device_CreateTexture3D(dev, NULL, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(t3d_auto_mips_resolved)
{
    static const struct { UINT w, h, d, mips; } c[] = { { 8, 8, 8, 4 }, { 16, 4, 2, 5 }, { 1, 1, 1, 1 }, { 4, 4, 32, 6 }, { 2, 2, 2, 2 } };
    DD_DEV(dev);
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        D3D10_TEXTURE3D_DESC d = { c[i].w, c[i].h, c[i].d, 0, RGBA, DEF, SR, 0, 0 }, g; ID3D10Texture3D *t = NULL;
        EXPECT_HR_OK(ID3D10Device_CreateTexture3D(dev, &d, NULL, &t));
        if (t) { ID3D10Texture3D_GetDesc(t, &g); t_check(g.MipLevels == c[i].mips, __FILE__, __LINE__, "3d auto mips", "%ux%ux%u got %u want %u", c[i].w, c[i].h, c[i].d, g.MipLevels, c[i].mips); ID3D10Texture3D_Release(t); }
    }
    DD_END(dev);
}
T(t3d_type)
{
    DD_DEV(dev);
    D3D10_TEXTURE3D_DESC d = { 4, 4, 4, 1, RGBA, DEF, SR, 0, 0 }; ID3D10Texture3D *t = NULL; D3D10_RESOURCE_DIMENSION dim = 0;
    EXPECT_HR_OK(ID3D10Device_CreateTexture3D(dev, &d, NULL, &t));
    if (t) { ID3D10Texture3D_GetType(t, &dim); EXPECT_EQ(dim, D3D10_RESOURCE_DIMENSION_TEXTURE3D); ID3D10Texture3D_Release(t); }
    DD_END(dev);
}
