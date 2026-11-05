/* Exercises the shared d3dh helpers and the committed assets. Anything needing a device skips when none can be created. */
#include "d3dh.h"
#include "shaders.h"
#include "meshes.h"
#include "textures.h"

TEST(d3dh_selftest_hr_names) {
    EXPECT_STR(d3dh_hr_name(S_OK), "S_OK");
    EXPECT_STR(d3dh_hr_name(DXGI_ERROR_DEVICE_REMOVED), "DXGI_ERROR_DEVICE_REMOVED");
    EXPECT_STR(d3dh_hr_name(D3DERR_INVALIDCALL), "D3DERR_INVALIDCALL");
    EXPECT_STR(d3dh_hr_name(DDERR_SURFACELOST), "DDERR_SURFACELOST");
    EXPECT_STR(d3dh_hr_name(E_INVALIDARG), "E_INVALIDARG");
    EXPECT_STR(d3dh_hr_name((HRESULT)0x80123456), "0x80123456");
    EXPECT_HR(E_FAIL, E_FAIL);
    EXPECT_HR_OK(S_FALSE);
    EXPECT_HR_FAILED(E_NOTIMPL);
}
TEST(d3dh_selftest_guid) {
    const char *s = d3dh_guid_str(&IID_IUnknown);
    CHECK(strstr(s, "{00000000-0000-0000-C000-000000000046}") == s);
    CHECK(strstr(s, "IID_IUnknown") != NULL);
    CHECK(strstr(d3dh_guid_str(NULL), "null") != NULL);
}
TEST(d3dh_selftest_hash_rng) {
    EXPECT_EQ(d3dh_crc32("123456789", 9), 0xCBF43926u);
    EXPECT_EQ(d3dh_fnv1a("a", 1), 0xE40C292Cu);
    EXPECT_EQ(d3dh_fnv1a("", 0), 2166136261u);
    d3dh_rng a, b; d3dh_rng_seed(&a, 42); d3dh_rng_seed(&b, 42);
    for (int i = 0; i < 100; i++) EXPECT_EQ(d3dh_rng_u32(&a), d3dh_rng_u32(&b));
    d3dh_rng_seed(&a, 1);
    for (int i = 0; i < 100; i++) { float f = d3dh_rng_float(&a); CHECK(f >= 0.0f && f < 1.0f); }
}
TEST(d3dh_selftest_window) {
    HWND w = d3dh_create_hidden_window();
    if (!w) { t_skip("cannot create a window here (err %lu)", GetLastError()); return; }
    CHECK(!IsWindowVisible(w));
    HWND w2 = d3dh_create_hidden_window_sized(32, 16);   /* class registered once: second creation works */
    CHECK(w2 != NULL);
    d3dh_destroy_window(w2); d3dh_destroy_window(w);
    CHECK(!IsWindow(w));
}
TEST(d3dh_selftest_loaders) {
    t_note("d3d11=%d d3d12=%d d3d9=%d d3d10=%d d3d10_1=%d dxgi=%d compiler=%d ddraw=%d",
           d3dh_D3D11CreateDevice() != NULL, d3dh_D3D12CreateDevice() != NULL, d3dh_Direct3DCreate9() != NULL,
           d3dh_D3D10CreateDevice() != NULL, d3dh_D3D10CreateDevice1() != NULL, d3dh_CreateDXGIFactory1() != NULL,
           d3dh_D3DCompile() != NULL, d3dh_DirectDrawCreate() != NULL);
    CHECK(d3dh_getproc("kernel32.dll", "GetTickCount") == d3dh_getproc("kernel32.dll", "GetTickCount"));
    CHECK(d3dh_getproc("no_such_dll_xyz.dll", "Foo") == NULL);
}
TEST(d3dh_selftest_assets_meshes) {
    struct { const float (*p)[3], (*n)[3]; const uint16_t *i; int nv, ni; const char *name; } m[] = {
        { mesh_cube_positions, mesh_cube_normals, mesh_cube_indices, MESH_CUBE_NVERTS, MESH_CUBE_NINDICES, "cube" },
        { mesh_quad_positions, mesh_quad_normals, mesh_quad_indices, MESH_QUAD_NVERTS, MESH_QUAD_NINDICES, "quad" },
        { mesh_tetra_positions, mesh_tetra_normals, mesh_tetra_indices, MESH_TETRA_NVERTS, MESH_TETRA_NINDICES, "tetra" },
    };
    for (int k = 0; k < 3; k++) {
        CHECK(m[k].ni % 3 == 0);
        for (int t = 0; t < m[k].ni; t += 3) {
            const float *a = m[k].p[m[k].i[t]], *b = m[k].p[m[k].i[t + 1]], *c = m[k].p[m[k].i[t + 2]];
            CHECK(m[k].i[t] < m[k].nv && m[k].i[t + 1] < m[k].nv && m[k].i[t + 2] < m[k].nv);
            float e1[3] = { b[0]-a[0], b[1]-a[1], b[2]-a[2] }, e2[3] = { c[0]-a[0], c[1]-a[1], c[2]-a[2] };
            float n[3] = { e1[1]*e2[2]-e1[2]*e2[1], e1[2]*e2[0]-e1[0]*e2[2], e1[0]*e2[1]-e1[1]*e2[0] };
            const float *vn = m[k].n[m[k].i[t]];
            CHECKF(n[0]*vn[0] + n[1]*vn[1] + n[2]*vn[2] > 0, "%s tri %d winding disagrees with its normal", m[k].name, t / 3);
            if (k != 1) CHECKF(a[0]*vn[0] + a[1]*vn[1] + a[2]*vn[2] > 0, "%s tri %d normal not outward", m[k].name, t / 3);
        }
    }
}
TEST(d3dh_selftest_assets_textures) {
    EXPECT_EQ(sizeof tex_checker8, 8 * 8 * 4);
    EXPECT_EQ(sizeof tex_gradient16, 16 * 16 * 4);
    EXPECT_EQ(sizeof tex_bc1_4x4, TEX_BC1_4X4_BYTES);
    EXPECT_EQ(sizeof tex_bc1_8x8, TEX_BC1_8X8_BYTES);
    EXPECT_EQ(tex_checker8[0], 0xffffffffu);
    EXPECT_EQ(tex_ramp1d[31] & 0xffffff, 0xffffffu);
}
TEST(d3dh_selftest_shaders_compile) {
    static const struct { const char *name, *src, *target; } sh[] = {
        { "fullscreen_vs", SH_FULLSCREEN_VS, "vs_5_0" }, { "flat_ps", SH_FLAT_PS, "ps_5_0" }, { "flat_cb_ps", SH_FLAT_CB_PS, "ps_5_0" },
        { "textured_ps", SH_TEXTURED_PS, "ps_5_0" }, { "vs_transform", SH_VS_TRANSFORM, "vs_5_0" },
        { "passthrough_gs", SH_PASSTHROUGH_GS, "gs_5_0" }, { "cs_thread_ids", SH_CS_THREAD_IDS, "cs_5_0" },
        { "cs_ids_full", SH_CS_IDS_FULL, "cs_5_0" }, { "cs_structured", SH_CS_STRUCTURED, "cs_5_0" },
        { "tess_vs", SH_TESS_VS, "vs_5_0" }, { "tess_hs", SH_TESS_HS, "hs_5_0" }, { "tess_ds", SH_TESS_DS, "ds_5_0" },
        { "sm5_math", SH_SM5_MATH, "cs_5_0" }, { "sm5_bits", SH_SM5_BITS, "cs_5_0" }, { "sm5_atomics", SH_SM5_ATOMICS, "cs_5_0" },
        { "sm5_sample", SH_SM5_SAMPLE, "ps_5_0" }, { "sm5_derivs", SH_SM5_DERIVS, "ps_5_0" }, { "sm5_interp", SH_SM5_INTERP, "ps_5_0" },
    };
    if (!d3dh_D3DCompile()) { t_skip("d3dcompiler_47.dll/D3DCompile not available"); return; }
    for (size_t i = 0; i < sizeof sh / sizeof sh[0]; i++) {
        ID3DBlob *b = NULL;
        HRESULT hr = d3dh_compile(sh[i].src, "main", sh[i].target, 0, &b);
        if (t_is_wine() && FAILED(hr)) t_note("wine compiler rejected %s: %s %.200s", sh[i].name, d3dh_hr_name(hr), d3dh_compile_errors());
        else { CHECKF(SUCCEEDED(hr), "%s: %s %.300s", sh[i].name, d3dh_hr_name(hr), d3dh_compile_errors()); }
        if (SUCCEEDED(hr)) { CHECK(d3dh_blob_size(b) > 16); CHECK(memcmp(d3dh_blob_ptr(b), "DXBC", 4) == 0); }
        SAFE_RELEASE(b);
    }
    /* a deliberately bad source must fail and produce error text */
    ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile("float4 main() : SV_Target { return undefined_symbol; }", "main", "ps_5_0", 0, &b);
    CHECK(FAILED(hr)); CHECK(b == NULL); CHECK(d3dh_compile_errors()[0] != 0);
}
TEST(d3dh_selftest_device11) {
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0; D3D_DRIVER_TYPE dt = 0;
    HRESULT hr = d3dh_make_device11(&dev, &ctx, &fl, &dt, 0);
    if (FAILED(hr)) { t_skip("no D3D11 device: %s", d3dh_hr_name(hr)); return; }
    t_note("D3D11 driver type %d feature level 0x%x", dt, fl);
    /* texture readback roundtrip */
    D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
    d.Width = 8; d.Height = 8; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA sd = { tex_checker8, 8 * 4, 0 };
    ID3D11Texture2D *t = NULL;
    EXPECT_HR_OK(hr = ID3D11Device_CreateTexture2D(dev, &d, &sd, &t));
    if (SUCCEEDED(hr)) {
        d3dh_image im;
        EXPECT_HR_OK(hr = d3dh_readback_texture2d11(dev, ctx, t, 0, &im));
        if (SUCCEEDED(hr)) { EXPECT_EQ(im.width, 8); EXPECT_EQ(d3dh_image_hash(&im), d3dh_fnv1a(tex_checker8, sizeof tex_checker8)); d3dh_image_free(&im); }
        EXPECT_EQ(d3dh_refcount(t), 1);
        EXPECT_EQ(D3DH_RELEASE_RC(t), 0);
    }
    /* buffer readback roundtrip */
    D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd); bd.ByteWidth = sizeof mesh_quad_indices; bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA bs = { mesh_quad_indices, 0, 0 };
    ID3D11Buffer *b = NULL;
    EXPECT_HR_OK(hr = ID3D11Device_CreateBuffer(dev, &bd, &bs, &b));
    if (SUCCEEDED(hr)) {
        void *p = NULL; size_t n = 0;
        EXPECT_HR_OK(hr = d3dh_readback_buffer11(dev, ctx, b, &p, &n));
        if (SUCCEEDED(hr)) { EXPECT_EQ(n, sizeof mesh_quad_indices); CHECK(!memcmp(p, mesh_quad_indices, n)); free(p); }
        SAFE_RELEASE(b);
    }
    CHECK(b == NULL);
    SAFE_RELEASE(ctx); EXPECT_EQ(D3DH_RELEASE_RC(dev), 0);
}
TEST(d3dh_selftest_device10) {
    ID3D10Device *dev = NULL; D3D10_DRIVER_TYPE dt;
    HRESULT hr = d3dh_make_device10(&dev, &dt, 0);
    if (FAILED(hr)) { t_skip("no D3D10 device: %s", d3dh_hr_name(hr)); return; }
    t_note("D3D10 driver type %d", dt);
    SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_device10_1) {
    ID3D10Device1 *dev = NULL; D3D10_DRIVER_TYPE dt; D3D10_FEATURE_LEVEL1 fl;
    HRESULT hr = d3dh_make_device10_1(&dev, &fl, &dt, 0);
    if (FAILED(hr)) { t_skip("no D3D10.1 device: %s", d3dh_hr_name(hr)); return; }
    t_note("D3D10.1 driver type %d level 0x%x", dt, fl);
    SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_device12) {
    ID3D12Device *dev = NULL; int warp = 0;
    HRESULT hr = d3dh_make_device12(&dev, D3D_FEATURE_LEVEL_11_0, &warp);
    if (FAILED(hr)) { t_skip("no D3D12 device: %s", d3dh_hr_name(hr)); return; }
    t_note("D3D12 warp=%d", warp);
    SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_device9) {
    IDirect3D9 *d3d = NULL; IDirect3DDevice9 *dev = NULL; HWND hw = NULL; D3DDEVTYPE ty;
    HRESULT hr = d3dh_make_device9(&d3d, &dev, NULL, &hw, &ty);
    if (FAILED(hr)) { t_skip("no D3D9 device: %s", d3dh_hr_name(hr)); SAFE_RELEASE(d3d); d3dh_destroy_window(hw); return; }
    t_note("D3D9 device type %d", ty);
    SAFE_RELEASE(dev); SAFE_RELEASE(d3d); d3dh_destroy_window(hw);
}
TEST(d3dh_selftest_factory) {
    IDXGIFactory1 *f = NULL;
    HRESULT hr = d3dh_make_factory1(&f);
    if (FAILED(hr)) { t_skip("no DXGI factory: %s", d3dh_hr_name(hr)); return; }
    IDXGIFactory2 *f2 = NULL;
    hr = D3DH_QI(f, IID_IDXGIFactory2, &f2);
    t_note("IDXGIFactory2 QI: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(f2); SAFE_RELEASE(f);
}
