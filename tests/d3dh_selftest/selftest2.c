/* More d3dh helper checks, pinned to what the helpers produce on Windows 10 22H2 (oracle: Basic Render Driver / WARP only). */
#include "d3dh.h"
#include "shaders.h"
#include "meshes.h"
#include "textures.h"

TEST(d3dh_selftest_have_dll) {
    CHECK(d3dh_have_dll("kernel32.dll"));
    CHECK(d3dh_have_dll("d3d11.dll"));
    CHECK(d3dh_have_dll("dxgi.dll"));
    CHECK(!d3dh_have_dll("no_such_library_i7.dll"));
    CHECK(!d3dh_have_dll(""));
}
TEST(d3dh_selftest_format_bytes_table) {
    static const struct { DXGI_FORMAT f; UINT b; } t[] = {
        { DXGI_FORMAT_R32G32B32A32_FLOAT, 16 }, { DXGI_FORMAT_R32G32B32_FLOAT, 12 }, { DXGI_FORMAT_R16G16B16A16_UNORM, 8 }, { DXGI_FORMAT_R32G32_FLOAT, 8 },
        { DXGI_FORMAT_R8G8B8A8_UNORM, 4 }, { DXGI_FORMAT_B8G8R8A8_UNORM, 4 }, { DXGI_FORMAT_R10G10B10A2_UNORM, 4 }, { DXGI_FORMAT_R32_FLOAT, 4 },
        { DXGI_FORMAT_D24_UNORM_S8_UINT, 4 }, { DXGI_FORMAT_R16_UNORM, 2 }, { DXGI_FORMAT_R8G8_UNORM, 2 }, { DXGI_FORMAT_R8_UNORM, 1 },
        { DXGI_FORMAT_A8_UNORM, 1 }, { DXGI_FORMAT_BC1_UNORM, 0 }, { DXGI_FORMAT_BC7_UNORM, 0 }, { DXGI_FORMAT_UNKNOWN, 0 }, { DXGI_FORMAT_NV12, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) CHECKF(d3dh_format_bytes(t[i].f) == t[i].b, "format %d: got %u want %u", t[i].f, d3dh_format_bytes(t[i].f), t[i].b);
}
TEST(d3dh_selftest_image_pixel_and_hash) {
    unsigned char px[2 * 2 * 4] = { 1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16 };
    d3dh_image im = { 2, 2, 8, DXGI_FORMAT_R8G8B8A8_UNORM, px, sizeof px };
    EXPECT_EQ(d3dh_image_pixel32(&im, 0, 0), 0x04030201u);
    EXPECT_EQ(d3dh_image_pixel32(&im, 1, 0), 0x08070605u);
    EXPECT_EQ(d3dh_image_pixel32(&im, 0, 1), 0x0c0b0a09u);
    EXPECT_EQ(d3dh_image_pixel32(&im, 1, 1), 0x100f0e0du);
    EXPECT_EQ(d3dh_image_hash(&im), d3dh_fnv1a(px, sizeof px));
    {   /* padded pitch: the hash covers only the tightly packed rows */
        unsigned char pad[24];
        d3dh_image ip = { 2, 2, 12, DXGI_FORMAT_R8G8B8A8_UNORM, pad, sizeof pad };
        memset(pad, 0xee, sizeof pad);
        memcpy(pad, px, 8); memcpy(pad + 12, px + 8, 8);
        EXPECT_EQ(d3dh_image_hash(&ip), d3dh_fnv1a(px, sizeof px));
        EXPECT_EQ(d3dh_image_pixel32(&ip, 1, 1), 0x100f0e0du);
    }
}
TEST(d3dh_selftest_device11_default_is_warp_fl11_0) {
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0; D3D_DRIVER_TYPE dt = 0;
    HRESULT hr = d3dh_make_device11(&dev, &ctx, &fl, &dt, 0);
    if (FAILED(hr)) { t_skip("no D3D11 device: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(dt, D3D_DRIVER_TYPE_WARP);        /* the helper tries WARP first */
    EXPECT_EQ(fl, D3D_FEATURE_LEVEL_11_0);      /* a NULL feature-level list tops out at 11_0 even on a 11_1 WARP */
    SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_device11_fl_matrix) {
    static const struct { D3D_FEATURE_LEVEL ask; HRESULT hr; D3D_FEATURE_LEVEL got; } t[] = {
        { D3D_FEATURE_LEVEL_11_1, S_OK, D3D_FEATURE_LEVEL_11_1 }, { D3D_FEATURE_LEVEL_11_0, S_OK, D3D_FEATURE_LEVEL_11_0 },
        { D3D_FEATURE_LEVEL_10_1, S_OK, D3D_FEATURE_LEVEL_10_1 }, { D3D_FEATURE_LEVEL_10_0, S_OK, D3D_FEATURE_LEVEL_10_0 },
        { D3D_FEATURE_LEVEL_9_3, S_OK, D3D_FEATURE_LEVEL_9_3 }, { D3D_FEATURE_LEVEL_9_2, S_OK, D3D_FEATURE_LEVEL_9_2 },
        { D3D_FEATURE_LEVEL_9_1, S_OK, D3D_FEATURE_LEVEL_9_1 },
        { D3D_FEATURE_LEVEL_12_0, S_OK, D3D_FEATURE_LEVEL_12_0 }, { D3D_FEATURE_LEVEL_12_1, S_OK, D3D_FEATURE_LEVEL_12_1 },   /* WARP accepts 12_x through d3d11 */
    };
    if (!d3dh_D3D11CreateDevice()) { t_skip("no d3d11"); return; }
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0;
        HRESULT hr = d3dh_make_device11_fl(&dev, &ctx, &t[i].ask, 1, &fl, NULL, 0);
        if (hr == t[i].hr && SUCCEEDED(hr)) CHECKF(fl == t[i].got, "ask 0x%x got 0x%x", t[i].ask, fl);
        CHECKF(hr == t[i].hr, "feature level 0x%x: got %s want %s", t[i].ask, d3dh_hr_name(hr), d3dh_hr_name(t[i].hr));
        SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    }
    {   /* a list is walked in order: 12_1 is skipped as unsupported only when the list is not led by it alone */
        static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0;
        EXPECT_HR(d3dh_make_device11_fl(&dev, &ctx, l, 2, &fl, NULL, 0), S_OK);
        EXPECT_EQ(fl, D3D_FEATURE_LEVEL_11_1);
        SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    }
    {   /* a single 12_x request is honoured (above) but inside a longer list the 12_x entry is passed over: 11_0 is what comes out */
        static const D3D_FEATURE_LEVEL l[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_11_0 };
        ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0;
        EXPECT_HR(d3dh_make_device11_fl(&dev, &ctx, l, 2, &fl, NULL, 0), S_OK);
        EXPECT_EQ(fl, D3D_FEATURE_LEVEL_11_0);
        SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    }
    {   /* an empty-intersection list: 9_x only requested above what 9_x gives is still fine, but an unknown level value is invalid */
        static const D3D_FEATURE_LEVEL l[] = { (D3D_FEATURE_LEVEL)0x1234 };
        ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = 0;
        EXPECT_HR(d3dh_make_device11_fl(&dev, &ctx, l, 1, &fl, NULL, 0), E_INVALIDARG);
        SAFE_RELEASE(ctx); SAFE_RELEASE(dev);
    }
}
TEST(d3dh_selftest_device10_default_is_warp) {
    ID3D10Device *dev = NULL; D3D10_DRIVER_TYPE dt = (D3D10_DRIVER_TYPE)99;
    HRESULT hr = d3dh_make_device10(&dev, &dt, 0);
    if (FAILED(hr)) { t_skip("no D3D10 device: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(dt, D3D10_DRIVER_TYPE_WARP);
    SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_device10_1_level) {
    ID3D10Device1 *dev = NULL; D3D10_DRIVER_TYPE dt = (D3D10_DRIVER_TYPE)99; D3D10_FEATURE_LEVEL1 fl = (D3D10_FEATURE_LEVEL1)0;
    HRESULT hr = d3dh_make_device10_1(&dev, &fl, &dt, 0);
    if (FAILED(hr)) { t_skip("no D3D10.1 device: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(dt, D3D10_DRIVER_TYPE_WARP);
    EXPECT_EQ(fl, D3D10_FEATURE_LEVEL_10_1);
    EXPECT_EQ(ID3D10Device1_GetFeatureLevel(dev), D3D10_FEATURE_LEVEL_10_1);
    SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_device9_classes) {
    IDirect3D9 *d3d = NULL; IDirect3DDevice9 *dev = NULL; HWND hw = NULL; D3DDEVTYPE ty = (D3DDEVTYPE)0;
    HRESULT hr = d3dh_make_device9(&d3d, &dev, NULL, &hw, &ty);
    if (FAILED(hr)) { t_skip("no D3D9 device: %s", d3dh_hr_name(hr)); SAFE_RELEASE(d3d); d3dh_destroy_window(hw); return; }
    EXPECT_EQ(ty, D3DDEVTYPE_NULLREF);          /* NULLREF is tried first */
    {
        D3DDEVICE_CREATION_PARAMETERS cp;
        EXPECT_HR(IDirect3DDevice9_GetCreationParameters(dev, &cp), S_OK);
        EXPECT_EQ(cp.DeviceType, D3DDEVTYPE_NULLREF);
        EXPECT_EQ(cp.AdapterOrdinal, 0);
        CHECK((cp.BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0);
    }
    SAFE_RELEASE(dev); SAFE_RELEASE(d3d); d3dh_destroy_window(hw);
}
TEST(d3dh_selftest_device9ex_classes) {
    IDirect3D9Ex *d3d = NULL; IDirect3DDevice9Ex *dev = NULL; HWND hw = NULL; D3DDEVTYPE ty = (D3DDEVTYPE)0;
    HRESULT hr = d3dh_make_device9ex(&d3d, &dev, NULL, &hw, &ty);
    if (FAILED(hr)) { t_skip("no D3D9Ex device: %s", d3dh_hr_name(hr)); SAFE_RELEASE(d3d); d3dh_destroy_window(hw); return; }
    EXPECT_EQ(ty, D3DDEVTYPE_NULLREF);
    {
        IDirect3DDevice9 *d9 = NULL;
        EXPECT_HR(IDirect3DDevice9Ex_QueryInterface(dev, &IID_IDirect3DDevice9, (void **)&d9), S_OK);
        SAFE_RELEASE(d9);
    }
    SAFE_RELEASE(dev); SAFE_RELEASE(d3d); d3dh_destroy_window(hw);
}
TEST(d3dh_selftest_device12_is_warp) {
    ID3D12Device *dev = NULL; int warp = -1;
    HRESULT hr = d3dh_make_device12(&dev, D3D_FEATURE_LEVEL_11_0, &warp);
    if (FAILED(hr)) { t_skip("no D3D12 device: %s", d3dh_hr_name(hr)); return; }
    EXPECT_EQ(warp, 1);                         /* EnumWarpAdapter is tried first */
    SAFE_RELEASE(dev);
    EXPECT_HR(d3dh_make_device12(&dev, D3D_FEATURE_LEVEL_12_2, &warp), E_INVALIDARG);   /* no 12_2 anywhere on Win10 22H2 here */
    SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_factory2) {
    IDXGIFactory2 *f = NULL;
    HRESULT hr = d3dh_make_factory2(&f);
    if (FAILED(hr)) { t_skip("no IDXGIFactory2: %s", d3dh_hr_name(hr)); return; }
    EXPECT_HR(hr, S_OK);
    {
        IDXGIAdapter *a = NULL;
        EXPECT_HR(IDXGIFactory2_EnumAdapters(f, 0, &a), S_OK);
        SAFE_RELEASE(a);
        EXPECT_HR(IDXGIFactory2_EnumAdapters(f, 100, &a), DXGI_ERROR_NOT_FOUND);
    }
    SAFE_RELEASE(f);
}
TEST(d3dh_selftest_compile_ex_errors_and_defines) {
    ID3DBlob *b = NULL;
    static const char src[] = "float4 main() : SV_Target { return float4(V, 0, 0, 1); }";
    D3D_SHADER_MACRO defs[] = { { "V", "0.5" }, { NULL, NULL } };
    HRESULT hr;
    if (!d3dh_D3DCompile()) { t_skip("no d3dcompiler"); return; }
    hr = d3dh_compile_ex(src, "inline.hlsl", defs, "main", "ps_4_0", 0, 0, &b);
    EXPECT_HR(hr, S_OK);
    CHECK(b != NULL && d3dh_blob_size(b) > 32 && d3dh_blob_ptr(b) != NULL);
    EXPECT_EQ(((const uint32_t *)d3dh_blob_ptr(b))[0], 0x43425844u);   /* 'DXBC' */
    SAFE_RELEASE(b);
    hr = d3dh_compile_ex(src, "inline.hlsl", NULL, "main", "ps_4_0", 0, 0, &b);   /* V undefined */
    EXPECT_HR(hr, E_FAIL);
    CHECK(b == NULL);
    CHECK(strstr(d3dh_compile_errors(), "undeclared identifier") != NULL);
    CHECK(strstr(d3dh_compile_errors(), "inline.hlsl") != NULL);
    hr = d3dh_compile_ex(src, "inline.hlsl", defs, "no_such_entry", "ps_4_0", 0, 0, &b);
    CHECK(FAILED(hr));
    hr = d3dh_compile_ex(src, "inline.hlsl", defs, "main", "xx_9_9", 0, 0, &b);
    CHECK(FAILED(hr));
    hr = d3dh_compile_ex(src, "inline.hlsl", defs, "main", "ps_4_0", 0, 0, &b);   /* a success clears the stored error text */
    EXPECT_HR(hr, S_OK);
    EXPECT_EQ(d3dh_compile_errors()[0], 0);
    SAFE_RELEASE(b);
}
TEST(d3dh_selftest_readback_buffer10) {
    ID3D10Device *dev = NULL; D3D10_DRIVER_TYPE dt;
    D3D10_BUFFER_DESC bd; D3D10_SUBRESOURCE_DATA sd; ID3D10Buffer *b = NULL;
    HRESULT hr = d3dh_make_device10(&dev, &dt, 0);
    if (FAILED(hr)) { t_skip("no D3D10 device: %s", d3dh_hr_name(hr)); return; }
    memset(&bd, 0, sizeof bd); bd.ByteWidth = sizeof mesh_quad_indices; bd.BindFlags = D3D10_BIND_INDEX_BUFFER;
    sd.pSysMem = mesh_quad_indices; sd.SysMemPitch = sd.SysMemSlicePitch = 0;
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, &bd, &sd, &b), S_OK);
    if (b) {
        void *p = NULL; size_t n = 0;
        EXPECT_HR(d3dh_readback_buffer10(dev, b, &p, &n), S_OK);
        EXPECT_EQ(n, sizeof mesh_quad_indices);
        if (p) { CHECK(!memcmp(p, mesh_quad_indices, n)); free(p); }
        SAFE_RELEASE(b);
    }
    SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_readback_texture2d10) {
    ID3D10Device *dev = NULL; D3D10_DRIVER_TYPE dt;
    D3D10_TEXTURE2D_DESC td; D3D10_SUBRESOURCE_DATA sd; ID3D10Texture2D *t = NULL;
    HRESULT hr = d3dh_make_device10(&dev, &dt, 0);
    if (FAILED(hr)) { t_skip("no D3D10 device: %s", d3dh_hr_name(hr)); return; }
    memset(&td, 0, sizeof td); td.Width = 8; td.Height = 8; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D10_USAGE_DEFAULT;
    sd.pSysMem = tex_checker8; sd.SysMemPitch = 8 * 4; sd.SysMemSlicePitch = 0;
    EXPECT_HR(ID3D10Device_CreateTexture2D(dev, &td, &sd, &t), S_OK);
    if (t) {
        d3dh_image im;
        EXPECT_HR(d3dh_readback_texture2d10(dev, t, &im), S_OK);
        EXPECT_EQ(im.width, 8); EXPECT_EQ(im.height, 8); EXPECT_EQ(im.format, DXGI_FORMAT_R8G8B8A8_UNORM);
        EXPECT_EQ(d3dh_image_hash(&im), d3dh_fnv1a(tex_checker8, sizeof tex_checker8));
        d3dh_image_free(&im);
        SAFE_RELEASE(t);
    }
    SAFE_RELEASE(dev);
}
TEST(d3dh_selftest_d12_helpers) {
    ID3D12Device *dev = NULL; int warp = 0;
    HRESULT hr = d3dh_make_device12(&dev, D3D_FEATURE_LEVEL_11_0, &warp);
    D3D12_HEAP_PROPERTIES hp; D3D12_RESOURCE_DESC rd; ID3D12Resource *up = NULL; ID3D12CommandQueue *q = NULL; D3D12_COMMAND_QUEUE_DESC qd;
    ID3D12DescriptorHeap *dh = NULL; D3D12_DESCRIPTOR_HEAP_DESC dd;
    if (FAILED(hr)) { t_skip("no D3D12 device: %s", d3dh_hr_name(hr)); return; }
    memset(&hp, 0, sizeof hp); hp.Type = D3D12_HEAP_TYPE_UPLOAD; hp.CreationNodeMask = hp.VisibleNodeMask = 1;
    memset(&rd, 0, sizeof rd); rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width = 64; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    EXPECT_HR(ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void **)&up), S_OK);
    if (up) {
        D3D12_RESOURCE_DESC g = d3dh_d12_resource_desc(up);
        unsigned char *m = NULL; D3D12_RANGE none = { 0, 0 }; unsigned i;
        ID3D12Resource *def = NULL;
        EXPECT_EQ(g.Dimension, D3D12_RESOURCE_DIMENSION_BUFFER); EXPECT_EQ(g.Width, 64); EXPECT_EQ(g.Height, 1);
        if (SUCCEEDED(ID3D12Resource_Map(up, 0, &none, (void **)&m)) && m) {
            for (i = 0; i < 64; i++) m[i] = (unsigned char)(i * 3 + 1);
            ID3D12Resource_Unmap(up, 0, NULL);
        }
        hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        EXPECT_HR(ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void **)&def), S_OK);
        if (def) {
            ID3D12CommandQueue *cq = NULL; ID3D12CommandAllocator *al = NULL; ID3D12GraphicsCommandList *cl = NULL; D3D12_COMMAND_QUEUE_DESC cqd;
            memset(&cqd, 0, sizeof cqd); cqd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            if (SUCCEEDED(ID3D12Device_CreateCommandQueue(dev, &cqd, &IID_ID3D12CommandQueue, (void **)&cq)) &&
                SUCCEEDED(ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&al)) &&
                SUCCEEDED(ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, al, NULL, &IID_ID3D12GraphicsCommandList, (void **)&cl))) {
                void *back = NULL; size_t n = 0;
                ID3D12GraphicsCommandList_CopyBufferRegion(cl, def, 0, up, 0, 64);
                EXPECT_HR(ID3D12GraphicsCommandList_Close(cl), S_OK);
                ID3D12CommandQueue_ExecuteCommandLists(cq, 1, (ID3D12CommandList *const *)&cl);
                EXPECT_HR(d3dh_d12_flush(dev, cq), S_OK);
                EXPECT_HR(d3dh_readback_buffer12(dev, def, D3D12_RESOURCE_STATE_COPY_DEST, &back, &n), S_OK);
                EXPECT_EQ(n, 64);
                if (back) { for (i = 0; i < 64; i++) if (((unsigned char *)back)[i] != (unsigned char)(i * 3 + 1)) break; EXPECT_EQ(i, 64); free(back); }
            } else CHECK(0);
            SAFE_RELEASE(cl); SAFE_RELEASE(al); SAFE_RELEASE(cq); SAFE_RELEASE(def);
        }
        SAFE_RELEASE(up);
    }
    memset(&qd, 0, sizeof qd); qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    EXPECT_HR(ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void **)&q), S_OK);
    if (q) { EXPECT_HR(d3dh_d12_flush(dev, q), S_OK); SAFE_RELEASE(q); }   /* flushing an idle queue returns at once */
    memset(&dd, 0, sizeof dd); dd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; dd.NumDescriptors = 4; dd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    EXPECT_HR(ID3D12Device_CreateDescriptorHeap(dev, &dd, &IID_ID3D12DescriptorHeap, (void **)&dh), S_OK);
    if (dh) {
        D3D12_CPU_DESCRIPTOR_HANDLE c = d3dh_d12_heap_cpu_start(dh);
        D3D12_GPU_DESCRIPTOR_HANDLE g = d3dh_d12_heap_gpu_start(dh);
        CHECK(c.ptr != 0);
        CHECK(g.ptr != 0);                      /* shader-visible heap: a GPU handle exists */
        SAFE_RELEASE(dh);
    }
    SAFE_RELEASE(dev);
}
