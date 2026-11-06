/* vertex / index buffers */
#include "d9r.h"

#define MKVB(d, len, us, fvf, pool, pp) IDirect3DDevice9_CreateVertexBuffer((d).dev, len, us, fvf, pool, pp, NULL)
#define MKIB(d, len, us, fmt, pool, pp) IDirect3DDevice9_CreateIndexBuffer((d).dev, len, us, fmt, pool, pp, NULL)

TEST(d3d9_device_resources_vb_args)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = (IDirect3DVertexBuffer9 *)1;
    EXPECT_HR(MKVB(d, 0, 0, 0, D3DPOOL_MANAGED, &vb), D3DERR_INVALIDCALL);
    CHECK(vb == NULL);
    EXPECT_HRN(MKVB(d, 64, 0, 0, D3DPOOL_MANAGED, NULL), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(MKVB(d, 64, 0, 0, (D3DPOOL)4, &vb), D3DERR_INVALIDCALL);
    EXPECT_HR(MKVB(d, 64, 0, 0, (D3DPOOL)-1, &vb), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_vb_pool_usage_matrix)
{
    D9_OPEN(d);
    struct { DWORD usage; D3DPOOL pool; int invalid; const char *n; } t[] = {
        { 0, D3DPOOL_DEFAULT, 0, "default" }, { 0, D3DPOOL_MANAGED, 0, "managed" }, { 0, D3DPOOL_SYSTEMMEM, 0, "sysmem" },
        { D3DUSAGE_WRITEONLY, D3DPOOL_DEFAULT, 0, "wo default" }, { D3DUSAGE_WRITEONLY, D3DPOOL_MANAGED, 0, "wo managed" },
        { D3DUSAGE_DYNAMIC, D3DPOOL_DEFAULT, 0, "dynamic default" }, { D3DUSAGE_DYNAMIC, D3DPOOL_SYSTEMMEM, 0, "dynamic sysmem" },
        { D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DPOOL_DEFAULT, 0, "dyn+wo default" },
        { D3DUSAGE_DYNAMIC, D3DPOOL_MANAGED, 1, "dynamic managed" },
        { 0, D3DPOOL_SCRATCH, 1, "scratch" },
        { D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, 1, "rendertarget" },
        { D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, 1, "depthstencil" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3DVertexBuffer9 *vb = NULL;
        HRESULT hr = MKVB(d, 256, t[i].usage, 0, t[i].pool, &vb);
        if (t[i].invalid) CHECKF(hr == D3DERR_INVALIDCALL, "%s got %s", t[i].n, d3dh_hr_name(hr));
        else {
            CHECKF(SUCCEEDED(hr), "%s got %s", t[i].n, d3dh_hr_name(hr));
            if (vb) {
                D3DVERTEXBUFFER_DESC vd;
                EXPECT_HR(IDirect3DVertexBuffer9_GetDesc(vb, &vd), D3D_OK);
                EXPECT_EQ(vd.Format, D3DFMT_VERTEXDATA);
                EXPECT_EQ(vd.Type, D3DRTYPE_VERTEXBUFFER);
                EXPECT_EQ(vd.Size, 256);
                EXPECT_EQ(vd.Pool, t[i].pool);
                EXPECT_EQ(vd.Usage & ~D3DUSAGE_SOFTWAREPROCESSING, t[i].usage);
                EXPECT_EQ(vd.FVF, 0);
            }
        }
        SAFE_RELEASE(vb);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_vb_desc_fvf)
{
    D9_OPEN(d);
    DWORD fvfs[] = { D3DFVF_XYZ, D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2 };
    for (unsigned i = 0; i < 4; i++) {
        IDirect3DVertexBuffer9 *vb = NULL;
        EXPECT_HR(MKVB(d, 128, 0, fvfs[i], D3DPOOL_MANAGED, &vb), D3D_OK);
        if (vb) {
            D3DVERTEXBUFFER_DESC vd;
            IDirect3DVertexBuffer9_GetDesc(vb, &vd);
            EXPECT_EQ(vd.FVF, fvfs[i]);
            EXPECT_EQ(vd.Size, 128);
            EXPECT_HRN(IDirect3DVertexBuffer9_GetDesc(vb, NULL), D9_AVHR);   /* Win10: access violation */
            IDirect3DVertexBuffer9_Release(vb);
        }
    }
    D9_END(d);
}

TEST(d3d9_device_resources_vb_size_not_fvf_multiple)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    HRESULT hr = MKVB(d, 13, 0, D3DFVF_XYZ, D3DPOOL_MANAGED, &vb);
    EXPECT_HR(hr, D3D_OK);
    if (vb) { D3DVERTEXBUFFER_DESC vd; IDirect3DVertexBuffer9_GetDesc(vb, &vd); EXPECT_EQ(vd.Size, 13); IDirect3DVertexBuffer9_Release(vb); }
    hr = MKVB(d, 1, 0, 0, D3DPOOL_SYSTEMMEM, &vb);
    EXPECT_HR(hr, D3D_OK);
    SAFE_RELEASE(vb);
    D9_END(d);
}

TEST(d3d9_device_resources_vb_lock_args)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    if (FAILED(MKVB(d, 64, 0, 0, D3DPOOL_MANAGED, &vb))) { t_skip("no vb"); D9_END(d); return; }
    void *p = (void *)1;
    EXPECT_HRN(IDirect3DVertexBuffer9_Lock(vb, 0, 0, NULL, 0), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 65, 0, &p, 0), S_OK);   /* Win10: S_OK (the lock range is not validated) */
    CHECK(p != NULL);
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 65, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 32, 33, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 64, 1, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0xffffffffu, 2, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 2, 0xffffffffu, &p, 0), S_OK);   /* Win10: S_OK */
    /* the failed locks must not have left the buffer locked */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 64, &p, 0), D3D_OK);
    EXPECT_HR(IDirect3DVertexBuffer9_Unlock(vb), D3D_OK);
    IDirect3DVertexBuffer9_Release(vb);
    D9_END(d);
}

TEST(d3d9_device_resources_vb_lock_unlock_state)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    if (FAILED(MKVB(d, 64, 0, 0, D3DPOOL_MANAGED, &vb))) { t_skip("no vb"); D9_END(d); return; }
    void *p = NULL, *q = NULL;
    EXPECT_HR(IDirect3DVertexBuffer9_Unlock(vb), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 0, &p, 0), D3D_OK);
    CHECK(p != NULL);
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 0, &q, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 8, 8, &q, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DVertexBuffer9_Unlock(vb), D3D_OK);
    EXPECT_HR(IDirect3DVertexBuffer9_Unlock(vb), S_OK);   /* Win10: S_OK */
    IDirect3DVertexBuffer9_Release(vb);
    D9_END(d);
}

TEST(d3d9_device_resources_vb_lock_offset_pointer)
{
    D9_OPEN(d);
    D3DPOOL pools[] = { D3DPOOL_MANAGED, D3DPOOL_SYSTEMMEM };
    for (unsigned k = 0; k < 2; k++) {
        IDirect3DVertexBuffer9 *vb = NULL;
        if (FAILED(MKVB(d, 128, 0, 0, pools[k], &vb))) continue;
        char *base = NULL, *off = NULL;
        EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 0, (void **)&base, 0), D3D_OK);
        IDirect3DVertexBuffer9_Unlock(vb);
        EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 40, 16, (void **)&off, 0), D3D_OK);
        IDirect3DVertexBuffer9_Unlock(vb);
        EXPECT_EQ(off - base, 40);
        /* size 0 means "to the end" */
        EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 127, 0, (void **)&off, 0), D3D_OK);
        IDirect3DVertexBuffer9_Unlock(vb);
        EXPECT_EQ(off - base, 127);
        t_note("Lock at offset == Size with size 0: %s", d3dh_hr_name(IDirect3DVertexBuffer9_Lock(vb, 128, 0, (void **)&off, 0)));
        IDirect3DVertexBuffer9_Unlock(vb);
        IDirect3DVertexBuffer9_Release(vb);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_vb_data_roundtrip)
{
    D9_OPEN(d);
    D3DPOOL pools[] = { D3DPOOL_MANAGED, D3DPOOL_SYSTEMMEM, D3DPOOL_DEFAULT };
    for (unsigned k = 0; k < 3; k++) {
        IDirect3DVertexBuffer9 *vb = NULL;
        if (FAILED(MKVB(d, 1024, 0, 0, pools[k], &vb))) { t_note("vb pool %d unavailable", pools[k]); continue; }
        unsigned char *p = NULL;
        HRESULT hr = IDirect3DVertexBuffer9_Lock(vb, 0, 0, (void **)&p, 0);
        if (FAILED(hr)) { t_note("pool %d lock: %s", pools[k], d3dh_hr_name(hr)); IDirect3DVertexBuffer9_Release(vb); continue; }
        for (int i = 0; i < 1024; i++) p[i] = (unsigned char)(i * 7 + k);
        IDirect3DVertexBuffer9_Unlock(vb);
        hr = IDirect3DVertexBuffer9_Lock(vb, 100, 200, (void **)&p, D3DLOCK_READONLY);
        EXPECT_HR(hr, D3D_OK);
        int bad = 0;
        if (SUCCEEDED(hr)) { for (int i = 0; i < 200; i++) if (p[i] != (unsigned char)((100 + i) * 7 + k)) bad++; IDirect3DVertexBuffer9_Unlock(vb); }
        CHECKF(bad == 0, "pool %d mismatches %d", pools[k], bad);
        IDirect3DVertexBuffer9_Release(vb);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_vb_lock_flags_dynamic)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    HRESULT hr = MKVB(d, 256, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &vb);
    if (FAILED(hr)) { t_skip("no dynamic vb: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    void *p = NULL;
    DWORD flags[] = { D3DLOCK_DISCARD, D3DLOCK_NOOVERWRITE, 0, D3DLOCK_NOSYSLOCK, D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK };
    for (unsigned i = 0; i < sizeof flags / sizeof *flags; i++) {
        hr = IDirect3DVertexBuffer9_Lock(vb, 0, 0, &p, flags[i]);
        CHECKF(SUCCEEDED(hr), "flags 0x%lx got %s", flags[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirect3DVertexBuffer9_Unlock(vb);
    }
    /* partial discard and no-overwrite locks */
    hr = IDirect3DVertexBuffer9_Lock(vb, 64, 64, &p, D3DLOCK_NOOVERWRITE);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) IDirect3DVertexBuffer9_Unlock(vb);
    hr = IDirect3DVertexBuffer9_Lock(vb, 64, 64, &p, D3DLOCK_DISCARD);
    EXPECT_HR(hr, D3D_OK);
    if (SUCCEEDED(hr)) IDirect3DVertexBuffer9_Unlock(vb);
    IDirect3DVertexBuffer9_Release(vb);
    D9_END(d);
}

TEST(d3d9_device_resources_vb_lock_flags_static)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    if (FAILED(MKVB(d, 256, 0, 0, D3DPOOL_MANAGED, &vb))) { t_skip("no vb"); D9_END(d); return; }
    void *p = NULL;
    DWORD flags[] = { D3DLOCK_READONLY, D3DLOCK_DISCARD, D3DLOCK_NOOVERWRITE, D3DLOCK_NO_DIRTY_UPDATE, D3DLOCK_READONLY | D3DLOCK_NO_DIRTY_UPDATE, D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE, D3DLOCK_READONLY | D3DLOCK_DISCARD };
    for (unsigned i = 0; i < sizeof flags / sizeof *flags; i++) {
        HRESULT hr = IDirect3DVertexBuffer9_Lock(vb, 0, 0, &p, flags[i]);
        t_note("managed vb lock flags 0x%lx: %s", flags[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirect3DVertexBuffer9_Unlock(vb);
    }
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 0, &p, D3DLOCK_READONLY), D3D_OK);
    EXPECT_HR(IDirect3DVertexBuffer9_Unlock(vb), D3D_OK);
    EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 0, 0, &p, D3DLOCK_NO_DIRTY_UPDATE), D3D_OK);
    EXPECT_HR(IDirect3DVertexBuffer9_Unlock(vb), D3D_OK);
    IDirect3DVertexBuffer9_Release(vb);
    D9_END(d);
}

TEST(d3d9_device_resources_vb_type_device_qi)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    if (FAILED(MKVB(d, 64, 0, 0, D3DPOOL_MANAGED, &vb))) { t_skip("no vb"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DVertexBuffer9_GetType(vb), D3DRTYPE_VERTEXBUFFER);
    IDirect3DDevice9 *dd = NULL;
    EXPECT_HR(IDirect3DVertexBuffer9_GetDevice(vb, &dd), D3D_OK);
    CHECK(dd == d.dev);
    SAFE_RELEASE(dd);
    void *p = NULL;
    EXPECT_HR(IDirect3DVertexBuffer9_QueryInterface(vb, &IID_IDirect3DResource9, &p), S_OK);
    if (p) IDirect3DResource9_Release((IDirect3DResource9 *)p);
    p = (void *)1;
    EXPECT_HR(IDirect3DVertexBuffer9_QueryInterface(vb, &IID_IDirect3DIndexBuffer9, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_HR(IDirect3DVertexBuffer9_QueryInterface(vb, &IID_IDirect3DBaseTexture9, &p), E_NOINTERFACE);
    IDirect3DVertexBuffer9_Release(vb);
    D9_END(d);
}

TEST(d3d9_device_resources_vb_priority)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    if (FAILED(MKVB(d, 64, 0, 0, D3DPOOL_MANAGED, &vb))) { t_skip("no vb"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DVertexBuffer9_GetPriority(vb), 0);
    EXPECT_EQ(IDirect3DVertexBuffer9_SetPriority(vb, 11), 0);
    EXPECT_EQ(IDirect3DVertexBuffer9_SetPriority(vb, 12), 11);
    EXPECT_EQ(IDirect3DVertexBuffer9_GetPriority(vb), 12);
    IDirect3DVertexBuffer9_PreLoad(vb);
    IDirect3DVertexBuffer9_Release(vb);
    if (SUCCEEDED(MKVB(d, 64, 0, 0, D3DPOOL_SYSTEMMEM, &vb))) {
        EXPECT_EQ(IDirect3DVertexBuffer9_SetPriority(vb, 11), 0);
        EXPECT_EQ(IDirect3DVertexBuffer9_SetPriority(vb, 12), 0);
        IDirect3DVertexBuffer9_Release(vb);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_vb_set_stream_source)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL, *got = NULL;
    if (FAILED(MKVB(d, 256, 0, 0, D3DPOOL_MANAGED, &vb))) { t_skip("no vb"); D9_END(d); return; }
    UINT off = 99, stride = 99;
    EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, 0, &got, &off, &stride), D3D_OK);
    CHECK(got == NULL);
    EXPECT_EQ(off, 0);
    EXPECT_EQ(stride, 0);
    ULONG rc0 = d3dh_refcount(vb);
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 16, 12), D3D_OK);
    EXPECT_EQ(d3dh_refcount(vb), rc0);   /* Win10: binding to a stream does not change the public reference count */
    EXPECT_HR(IDirect3DDevice9_GetStreamSource(d.dev, 0, &got, &off, &stride), D3D_OK);
    CHECK(got == vb);
    EXPECT_EQ(off, 16);
    EXPECT_EQ(stride, 12);
    SAFE_RELEASE(got);
    D3DCAPS9 c; IDirect3DDevice9_GetDeviceCaps(d.dev, &c);
    /* Win10 retail d3d9 accepts stream number == MaxStreams (S_OK) and then corrupts its own heap (0xc0000374), so the out-of-range stream numbers are NOT exercised */
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 0, vb, 0, c.MaxStreamStride + 1), S_OK);   /* Win10: stride above MaxStreamStride is accepted */
    EXPECT_HRN(IDirect3DDevice9_GetStreamSource(d.dev, 0, NULL, &off, &stride), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(IDirect3DDevice9_SetStreamSource(d.dev, 0, NULL, 0, 0), D3D_OK);
    CHECK(d3dh_refcount(vb) == rc0);
    IDirect3DVertexBuffer9_Release(vb);
    D9_END(d);
}

/* ---- index buffers ---- */
TEST(d3d9_device_resources_ib_args)
{
    D9_OPEN(d);
    IDirect3DIndexBuffer9 *ib = (IDirect3DIndexBuffer9 *)1;
    EXPECT_HR(MKIB(d, 0, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib), D3DERR_INVALIDCALL);
    CHECK(ib == NULL);
    EXPECT_HRN(MKIB(d, 32, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, NULL), D9_AVHR);   /* Win10: access violation */
    D3DFORMAT bad[] = { D3DFMT_UNKNOWN, D3DFMT_A8R8G8B8, D3DFMT_VERTEXDATA, D3DFMT_R16F, D3DFMT_D16 };
    for (unsigned i = 0; i < 5; i++) {
        HRESULT hr = MKIB(d, 32, 0, bad[i], D3DPOOL_MANAGED, &ib);
        CHECKF(FAILED(hr), "fmt %d got %s", bad[i], d3dh_hr_name(hr));
        SAFE_RELEASE(ib);
    }
    EXPECT_HR(MKIB(d, 32, 0, D3DFMT_INDEX16, (D3DPOOL)5, &ib), D3DERR_INVALIDCALL);
    D9_END(d);
}

TEST(d3d9_device_resources_ib_pool_usage_matrix)
{
    D9_OPEN(d);
    struct { DWORD usage; D3DPOOL pool; D3DFORMAT f; int invalid; const char *n; } t[] = {
        { 0, D3DPOOL_DEFAULT, D3DFMT_INDEX16, 0, "default16" }, { 0, D3DPOOL_MANAGED, D3DFMT_INDEX16, 0, "managed16" },
        { 0, D3DPOOL_SYSTEMMEM, D3DFMT_INDEX16, 0, "sysmem16" }, { D3DUSAGE_WRITEONLY, D3DPOOL_MANAGED, D3DFMT_INDEX16, 0, "wo managed16" },
        { D3DUSAGE_DYNAMIC, D3DPOOL_DEFAULT, D3DFMT_INDEX16, 0, "dyn default16" }, { D3DUSAGE_DYNAMIC, D3DPOOL_MANAGED, D3DFMT_INDEX16, 1, "dyn managed16" },
        { 0, D3DPOOL_SCRATCH, D3DFMT_INDEX16, 1, "scratch16" }, { D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, D3DFMT_INDEX16, 1, "rt16" },
        { 0, D3DPOOL_MANAGED, D3DFMT_INDEX32, 0, "managed32" },
    };
    D3DCAPS9 c; IDirect3DDevice9_GetDeviceCaps(d.dev, &c);
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        IDirect3DIndexBuffer9 *ib = NULL;
        HRESULT hr = MKIB(d, 64, t[i].usage, t[i].f, t[i].pool, &ib);
        if (t[i].invalid) CHECKF(hr == D3DERR_INVALIDCALL, "%s got %s", t[i].n, d3dh_hr_name(hr));
        else if (t[i].f == D3DFMT_INDEX32 && c.MaxVertexIndex <= 0xffff) t_note("%s with MaxVertexIndex 0x%lx: %s", t[i].n, c.MaxVertexIndex, d3dh_hr_name(hr));
        else {
            CHECKF(SUCCEEDED(hr), "%s got %s", t[i].n, d3dh_hr_name(hr));
            if (ib) {
                D3DINDEXBUFFER_DESC id;
                EXPECT_HR(IDirect3DIndexBuffer9_GetDesc(ib, &id), D3D_OK);
                EXPECT_EQ(id.Format, t[i].f);
                EXPECT_EQ(id.Type, D3DRTYPE_INDEXBUFFER);
                EXPECT_EQ(id.Size, 64);
                EXPECT_EQ(id.Pool, t[i].pool);
                EXPECT_EQ(id.Usage & ~D3DUSAGE_SOFTWAREPROCESSING, t[i].usage);
                EXPECT_HRN(IDirect3DIndexBuffer9_GetDesc(ib, NULL), D9_AVHR);   /* Win10: access violation */
            }
        }
        SAFE_RELEASE(ib);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_ib_lock_args)
{
    D9_OPEN(d);
    IDirect3DIndexBuffer9 *ib = NULL;
    if (FAILED(MKIB(d, 64, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib))) { t_skip("no ib"); D9_END(d); return; }
    void *p = (void *)1;
    EXPECT_HRN(IDirect3DIndexBuffer9_Lock(ib, 0, 0, NULL, 0), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(IDirect3DIndexBuffer9_Lock(ib, 65, 0, &p, 0), S_OK);   /* Win10: S_OK (the lock range is not validated) */
    CHECK(p != NULL);
    EXPECT_HR(IDirect3DIndexBuffer9_Lock(ib, 0, 65, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DIndexBuffer9_Lock(ib, 60, 5, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DIndexBuffer9_Unlock(ib), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DIndexBuffer9_Lock(ib, 2, 6, &p, 0), D3D_OK);
    EXPECT_HR(IDirect3DIndexBuffer9_Lock(ib, 0, 0, &p, 0), S_OK);   /* Win10: S_OK */
    EXPECT_HR(IDirect3DIndexBuffer9_Unlock(ib), D3D_OK);
    EXPECT_HR(IDirect3DIndexBuffer9_Unlock(ib), S_OK);   /* Win10: S_OK */
    IDirect3DIndexBuffer9_Release(ib);
    D9_END(d);
}

TEST(d3d9_device_resources_ib_data_roundtrip)
{
    D9_OPEN(d);
    D3DFORMAT fm[] = { D3DFMT_INDEX16, D3DFMT_INDEX32 };
    for (unsigned k = 0; k < 2; k++) {
        IDirect3DIndexBuffer9 *ib = NULL;
        if (FAILED(MKIB(d, 96, 0, fm[k], D3DPOOL_MANAGED, &ib))) { t_note("ib fmt %d unavailable", fm[k]); continue; }
        unsigned char *p = NULL;
        EXPECT_HR(IDirect3DIndexBuffer9_Lock(ib, 0, 0, (void **)&p, 0), D3D_OK);
        for (int i = 0; i < 96; i++) p[i] = (unsigned char)(i ^ 0x5a);
        IDirect3DIndexBuffer9_Unlock(ib);
        EXPECT_HR(IDirect3DIndexBuffer9_Lock(ib, 32, 32, (void **)&p, D3DLOCK_READONLY), D3D_OK);
        int bad = 0;
        for (int i = 0; i < 32; i++) if (p[i] != (unsigned char)((32 + i) ^ 0x5a)) bad++;
        CHECK(bad == 0);
        IDirect3DIndexBuffer9_Unlock(ib);
        IDirect3DIndexBuffer9_Release(ib);
    }
    D9_END(d);
}

TEST(d3d9_device_resources_ib_dynamic_flags)
{
    D9_OPEN(d);
    IDirect3DIndexBuffer9 *ib = NULL;
    HRESULT hr = MKIB(d, 128, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib);
    if (FAILED(hr)) { t_skip("no dynamic ib: %s", d3dh_hr_name(hr)); D9_END(d); return; }
    void *p;
    DWORD flags[] = { D3DLOCK_DISCARD, D3DLOCK_NOOVERWRITE, 0 };
    for (unsigned i = 0; i < 3; i++) {
        hr = IDirect3DIndexBuffer9_Lock(ib, 0, 0, &p, flags[i]);
        CHECKF(SUCCEEDED(hr), "flags 0x%lx got %s", flags[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) IDirect3DIndexBuffer9_Unlock(ib);
    }
    IDirect3DIndexBuffer9_Release(ib);
    D9_END(d);
}

TEST(d3d9_device_resources_ib_set_indices)
{
    D9_OPEN(d);
    IDirect3DIndexBuffer9 *ib = NULL, *got = (IDirect3DIndexBuffer9 *)1;
    if (FAILED(MKIB(d, 64, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib))) { t_skip("no ib"); D9_END(d); return; }
    EXPECT_HR(IDirect3DDevice9_GetIndices(d.dev, &got), D3D_OK);
    CHECK(got == NULL);
    ULONG rc0 = d3dh_refcount(ib);
    EXPECT_HR(IDirect3DDevice9_SetIndices(d.dev, ib), D3D_OK);
    EXPECT_EQ(d3dh_refcount(ib), rc0);   /* Win10: binding does not change the public reference count */
    EXPECT_HR(IDirect3DDevice9_GetIndices(d.dev, &got), D3D_OK);
    CHECK(got == ib);
    SAFE_RELEASE(got);
    EXPECT_HRN(IDirect3DDevice9_GetIndices(d.dev, NULL), D9_AVHR);   /* Win10: access violation */
    EXPECT_HR(IDirect3DDevice9_SetIndices(d.dev, NULL), D3D_OK);
    CHECK(d3dh_refcount(ib) == rc0);
    IDirect3DIndexBuffer9_Release(ib);
    D9_END(d);
}

TEST(d3d9_device_resources_ib_type_priority)
{
    D9_OPEN(d);
    IDirect3DIndexBuffer9 *ib = NULL;
    if (FAILED(MKIB(d, 64, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib))) { t_skip("no ib"); D9_END(d); return; }
    EXPECT_EQ(IDirect3DIndexBuffer9_GetType(ib), D3DRTYPE_INDEXBUFFER);
    EXPECT_EQ(IDirect3DIndexBuffer9_SetPriority(ib, 4), 0);
    EXPECT_EQ(IDirect3DIndexBuffer9_GetPriority(ib), 4);
    IDirect3DIndexBuffer9_PreLoad(ib);
    void *p = (void *)1;
    EXPECT_HR(IDirect3DIndexBuffer9_QueryInterface(ib, &IID_IDirect3DVertexBuffer9, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    IDirect3DDevice9 *dd = NULL;
    EXPECT_HR(IDirect3DIndexBuffer9_GetDevice(ib, &dd), D3D_OK);
    CHECK(dd == d.dev);
    SAFE_RELEASE(dd);
    IDirect3DIndexBuffer9_Release(ib);
    D9_END(d);
}

TEST(d3d9_device_resources_buffers_large)
{
    D9_OPEN(d);
    IDirect3DVertexBuffer9 *vb = NULL;
    HRESULT hr = MKVB(d, 8 * 1024 * 1024, 0, 0, D3DPOOL_MANAGED, &vb);
    EXPECT_HR(hr, D3D_OK);
    if (vb) {
        unsigned char *p = NULL;
        EXPECT_HR(IDirect3DVertexBuffer9_Lock(vb, 8 * 1024 * 1024 - 4, 4, (void **)&p, 0), D3D_OK);
        if (p) { p[0] = 1; p[3] = 2; }
        IDirect3DVertexBuffer9_Unlock(vb);
        IDirect3DVertexBuffer9_Release(vb);
    }
    D9_END(d);
}
