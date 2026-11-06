/* level descriptors, device-level queries, buffer argument validation */
#include "d9r.h"

TEST(d3d9_device_resources_extra_leveldesc_chain)
{
    D9_OPEN(d);
    struct { UINT w, h; UINT levels; } t[] = { { 16, 4, 5 }, { 1, 1, 1 }, { 8, 8, 4 }, { 32, 1, 6 }, { 7, 5, 3 } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3DTexture9 *tex = NULL;
        HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, t[i].w, t[i].h, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &tex, NULL);
        if (FAILED(hr)) { t_note("create %ux%u: %s", t[i].w, t[i].h, d3dh_hr_name(hr)); continue; }
        EXPECT_EQ(IDirect3DTexture9_GetLevelCount(tex), t[i].levels);
        UINT w = t[i].w, h = t[i].h;
        for (UINT l = 0; l < t[i].levels; l++) {
            D3DSURFACE_DESC sd; memset(&sd, 0xcc, sizeof sd);
            EXPECT_HR(IDirect3DTexture9_GetLevelDesc(tex, l, &sd), D3D_OK);
            EXPECT_EQ(sd.Width, w); EXPECT_EQ(sd.Height, h);
            EXPECT_EQ(sd.Format, D3DFMT_A8R8G8B8);
            EXPECT_EQ(sd.Type, D3DRTYPE_SURFACE);
            EXPECT_EQ(sd.Pool, D3DPOOL_SYSTEMMEM);
            EXPECT_EQ(sd.MultiSampleType, D3DMULTISAMPLE_NONE);
            w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1;
        }
        D3DSURFACE_DESC sd;
        EXPECT_HR(IDirect3DTexture9_GetLevelDesc(tex, t[i].levels, &sd), D3DERR_INVALIDCALL);
        EXPECT_HRN(IDirect3DTexture9_GetLevelDesc(tex, 0, NULL), D9_AVHR);   /* Win10: access violation */
        IDirect3DTexture9_Release(tex);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_extra_managed_lod_returns_previous)
{
    D9_OPEN(d);
    IDirect3DTexture9 *tex = NULL;
    HRESULT hr = IDirect3DDevice9_CreateTexture(d.dev, 16, 16, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    if (FAILED(hr)) { t_skip("no managed texture: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    EXPECT_EQ(IDirect3DTexture9_GetLOD(tex), 0);
    EXPECT_EQ(IDirect3DTexture9_SetLOD(tex, 2), 0);
    EXPECT_EQ(IDirect3DTexture9_GetLOD(tex), 2);
    EXPECT_EQ(IDirect3DTexture9_SetLOD(tex, 3), 2);
    /* clamps to the last level */
    IDirect3DTexture9_SetLOD(tex, 100);
    EXPECT_EQ(IDirect3DTexture9_GetLOD(tex), IDirect3DTexture9_GetLevelCount(tex) - 1);
    IDirect3DTexture9_SetLOD(tex, 0);
    IDirect3DTexture9_Release(tex);
    D9_END(d);
}

TEST(d3d9_device_resources_extra_getdisplaymode_args)
{
    D9_OPEN(d);
    D3DDISPLAYMODE m;
    EXPECT_HRN(IDirect3DDevice9_GetDisplayMode(d.dev, 0, NULL), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(IDirect3DDevice9_GetDisplayMode(d.dev, 1, &m), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetDisplayMode(d.dev, 0, &m), D3D_OK);
    CHECK(m.Width > 0 && m.Height > 0);
    CHECK(m.Format != D3DFMT_UNKNOWN);
    D9_END(d);
}

TEST(d3d9_device_resources_extra_swapchain_index_args)
{
    D9_OPEN(d);
    IDirect3DSwapChain9 *sc = (IDirect3DSwapChain9 *)1;
    EXPECT_HR(IDirect3DDevice9_GetSwapChain(d.dev, 1, &sc), D3DERR_INVALIDCALL);
    CHECK(sc == NULL);
    EXPECT_HRN(IDirect3DDevice9_GetSwapChain(d.dev, 0, NULL), D9_AVHR);   /* Win10: access violation */
    IDirect3DSurface9 *bb = NULL;
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 1, 0, D3DBACKBUFFER_TYPE_MONO, &bb), D3DERR_INVALIDCALL);
    EXPECT_HR(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 100, D3DBACKBUFFER_TYPE_MONO, &bb), D3DERR_INVALIDCALL);
    EXPECT_HRN(IDirect3DDevice9_GetBackBuffer(d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, NULL), D9_AVHR);   /* Win10: access violation */
    D9_END(d);
}

TEST(d3d9_device_resources_extra_evict_and_mem)
{
    D9_OPEN(d);
    EXPECT_HR(IDirect3DDevice9_EvictManagedResources(d.dev), D3D_OK);
    UINT m = IDirect3DDevice9_GetAvailableTextureMem(d.dev);
    t_note("GetAvailableTextureMem = %u", m);
    CHECK(m > 0);
    EXPECT_HR(IDirect3DDevice9_EvictManagedResources(d.dev), D3D_OK);
    D9_END(d);
}

TEST(d3d9_device_resources_extra_buffer_zero_length)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = (IDirect3DVertexBuffer9 *)1;
    EXPECT_HRN(IDirect3DDevice9_CreateVertexBuffer(d.dev, 0, 0, 0, D3DPOOL_DEFAULT, &vb, NULL), D3DERR_INVALIDCALL);
    CHECK(vb == NULL);
    EXPECT_HRN(IDirect3DDevice9_CreateVertexBuffer(d.dev, 64, 0, 0, D3DPOOL_DEFAULT, NULL, NULL), D9_AVHR);   /* Win10: access violation */
    IDirect3DIndexBuffer9 *ib = (IDirect3DIndexBuffer9 *)1;
    EXPECT_HRN(IDirect3DDevice9_CreateIndexBuffer(d.dev, 0, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, NULL), D3DERR_INVALIDCALL);
    CHECK(ib == NULL);
    ib = NULL;
    EXPECT_HRN(IDirect3DDevice9_CreateIndexBuffer(d.dev, 64, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &ib, NULL), D3DERR_INVALIDCALL);
    SAFE_RELEASE(ib);
    D9_END(d);
}

TEST(d3d9_device_resources_extra_buffer_desc_roundtrip)
{
    D9_OPEN(d);
    IDirect3DIndexBuffer9 *ib = NULL;
    HRESULT hr = IDirect3DDevice9_CreateIndexBuffer(d.dev, 96, D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ib, NULL);
    if (SUCCEEDED(hr)) {
        D3DINDEXBUFFER_DESC id;
        EXPECT_HR(IDirect3DIndexBuffer9_GetDesc(ib, &id), D3D_OK);
        EXPECT_EQ(id.Format, D3DFMT_INDEX32);
        EXPECT_EQ(id.Type, D3DRTYPE_INDEXBUFFER);
        EXPECT_EQ(id.Usage, D3DUSAGE_WRITEONLY);
        EXPECT_EQ(id.Pool, D3DPOOL_DEFAULT);
        EXPECT_EQ(id.Size, 96);
        EXPECT_HRN(IDirect3DIndexBuffer9_GetDesc(ib, NULL), D9_AVHR);   /* Win10: access violation */
        IDirect3DIndexBuffer9_Release(ib);
    } else t_note("INDEX32 buffer: %s", d3dh_hr_name(hr));
    IDirect3DVertexBuffer9 *vb = NULL;
    hr = IDirect3DDevice9_CreateVertexBuffer(d.dev, 100, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DPOOL_DEFAULT, &vb, NULL);
    if (SUCCEEDED(hr)) {
        D3DVERTEXBUFFER_DESC vd;
        EXPECT_HR(IDirect3DVertexBuffer9_GetDesc(vb, &vd), D3D_OK);
        EXPECT_EQ(vd.Size, 100);
        EXPECT_EQ(vd.FVF, D3DFVF_XYZ | D3DFVF_DIFFUSE);
        EXPECT_EQ(vd.Type, D3DRTYPE_VERTEXBUFFER);
        EXPECT_EQ(vd.Pool, D3DPOOL_DEFAULT);
        CHECK(vd.Usage & D3DUSAGE_DYNAMIC);
        EXPECT_HRN(IDirect3DVertexBuffer9_GetDesc(vb, NULL), D9_AVHR);   /* Win10: access violation */
        IDirect3DVertexBuffer9_Release(vb);
    } else t_note("dynamic VB: %s", d3dh_hr_name(hr));
    D9_END(d);
}

TEST(d3d9_device_resources_extra_buffer_lock_range)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    if (FAILED(IDirect3DDevice9_CreateVertexBuffer(d.dev, 128, 0, 0, D3DPOOL_MANAGED, &vb, NULL))) { t_skip("no managed VB"); D9_END(d); return; }
    void *p = (void *)1;
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 129, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 200, 4, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HRN(IDirect3DVertexBuffer9_Lock(vb, 0, 128, NULL, 0), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 124, 4, &p, 0), D3D_OK);
    EXPECT_HR(IDirect3DVertexBuffer9_Unlock(vb), D3D_OK);
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 0, &p, 0), D3D_OK);   /* size 0 = whole buffer */
    memset(p, 0xab, 128);
    EXPECT_HR(IDirect3DVertexBuffer9_Unlock(vb), D3D_OK);
    IDirect3DVertexBuffer9_Release(vb);
    D9_END(d);
}

TEST(d3d9_device_resources_extra_creation_params_types)
{
    D9_OPEN(d);
    D3DDEVICE_CREATION_PARAMETERS cp; memset(&cp, 0, sizeof cp);
    EXPECT_HR(IDirect3DDevice9_GetCreationParameters(d.dev, &cp), D3D_OK);
    EXPECT_EQ(cp.AdapterOrdinal, 0);
    EXPECT_EQ(cp.DeviceType, d.type);
    CHECK(cp.hFocusWindow == d.hwnd);
    CHECK(cp.BehaviorFlags & (D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MIXED_VERTEXPROCESSING));
    EXPECT_HRN(IDirect3DDevice9_GetCreationParameters(d.dev, NULL), D9_AVHR);   /* Win10: access violation */
    D9_END(d);
}

TEST(d3d9_device_resources_extra_pool_priority_default)
{
    D9_OPEN(d);
    IDirect3DTexture9 *t = NULL;
    if (FAILED(IDirect3DDevice9_CreateTexture(d.dev, 8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL))) { t_skip("no managed tex"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DTexture9_GetPriority(t), 0);
    EXPECT_EQ(IDirect3DTexture9_SetPriority(t, 5), 0);
    EXPECT_EQ(IDirect3DTexture9_SetPriority(t, 9), 5);
    EXPECT_EQ(IDirect3DTexture9_GetPriority(t), 9);
    IDirect3DTexture9_Release(t);
    D9_END(d);
}
