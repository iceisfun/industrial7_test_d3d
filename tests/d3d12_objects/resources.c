/* d3d12_objects: heaps, committed/placed/reserved resources, footprints, allocation info, mapping. */
#include "d12o.h"

#define BAD(hr) (FAILED(hr))

/* ====== resource descriptor validity: buffers ================================================= */
static HRESULT try_buffer(ID3D12Device *dev, D3D12_HEAP_TYPE ht, D3D12_RESOURCE_DESC d, D3D12_RESOURCE_STATES st, const D3D12_CLEAR_VALUE *cv, ID3D12Resource **out) {
    return d12_create_committed(dev, ht, &d, st, cv, out);
}
TEST(d3d12_objects_res_buffer_create_heap_types) {
    NEED_DEV();
    static const D3D12_HEAP_TYPE ht[] = { D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_TYPE_READBACK };
    static const char *hn[] = { "DEFAULT", "UPLOAD", "READBACK" };
    for (unsigned i = 0; i < 3; i++) {
        ID3D12Resource *r = NULL;
        HRESULT hr = d12_make_buffer(dev, ht[i], 4096, D3D12_RESOURCE_FLAG_NONE, &r);
        CHECKF(hr == S_OK, "%s buffer: %s", hn[i], d3dh_hr_name(hr));
        if (r) {
            D3D12_RESOURCE_DESC g = d12_res_desc(r);
            EXPECT_EQ(g.Dimension, D3D12_RESOURCE_DIMENSION_BUFFER);
            EXPECT_EQ(g.Width, 4096); EXPECT_EQ(g.Height, 1); EXPECT_EQ(g.DepthOrArraySize, 1); EXPECT_EQ(g.MipLevels, 1);
            EXPECT_EQ(g.Format, DXGI_FORMAT_UNKNOWN); EXPECT_EQ(g.SampleDesc.Count, 1); EXPECT_EQ(g.SampleDesc.Quality, 0);
            EXPECT_EQ(g.Layout, D3D12_TEXTURE_LAYOUT_ROW_MAJOR); EXPECT_EQ(g.Flags, D3D12_RESOURCE_FLAG_NONE);
            EXPECT_EQ(g.Alignment, 65536);
            SAFE_RELEASE(r);
        }
    }
}
TEST(d3d12_objects_res_buffer_sizes) {
    NEED_DEV();
    static const UINT64 sz[] = { 1, 4, 255, 256, 65535, 65536, 65537, 1 << 20, 16 << 20 };
    for (unsigned i = 0; i < sizeof sz / sizeof *sz; i++) {
        ID3D12Resource *r = NULL;
        HRESULT hr = d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, sz[i], D3D12_RESOURCE_FLAG_NONE, &r);
        CHECKF(hr == S_OK, "buffer %llu: %s", (unsigned long long)sz[i], d3dh_hr_name(hr));
        if (r) { D3D12_RESOURCE_DESC g = d12_res_desc(r); EXPECT_EQ(g.Width, sz[i]); SAFE_RELEASE(r); }
    }
}
TEST(d3d12_objects_res_buffer_flags_valid) {
    NEED_DEV();
    static const D3D12_RESOURCE_FLAGS f[] = { D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER };
    for (unsigned i = 0; i < 4; i++) {
        ID3D12Resource *r = NULL;
        HRESULT hr = d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 1024, f[i], &r);
        if (f[i] & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER) { t_note("buffer flags 0x%x: %s", f[i], d3dh_hr_name(hr)); }
        else CHECKF(hr == S_OK, "flags 0x%x: %s", f[i], d3dh_hr_name(hr));
        if (r) { D3D12_RESOURCE_DESC g = d12_res_desc(r); EXPECT_EQ(g.Flags, f[i]); SAFE_RELEASE(r); }
    }
}
static const struct { const char *n; } buf_bad[] = {
    { "width 0" }, { "height 2" }, { "depth 2" }, { "mips 2" }, { "format R8" }, { "layout UNKNOWN" },
    { "layout 64KB_UNDEFINED" }, { "samples 2" }, { "flag RT" }, { "flag DS" }, { "flag DENY_SRV" }, { "dimension 0" },
    { "dimension 5" }, { "alignment 4096" }, { "flag RT|UAV" },
};
static void buf_mutate(unsigned i, D3D12_RESOURCE_DESC *d) {
    switch (i) {
    case 0: d->Width = 0; break;
    case 1: d->Height = 2; break;
    case 2: d->DepthOrArraySize = 2; break;
    case 3: d->MipLevels = 2; break;
    case 4: d->Format = DXGI_FORMAT_R8_UNORM; break;
    case 5: d->Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; break;
    case 6: d->Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE; break;
    case 7: d->SampleDesc.Count = 2; break;
    case 8: d->Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; break;
    case 9: d->Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; break;
    case 10: d->Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE; break;
    case 11: d->Dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN; break;
    case 12: d->Dimension = (D3D12_RESOURCE_DIMENSION)5; break;
    case 13: d->Alignment = 4096; break;
    case 14: d->Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; break;
    }
}
TEST(d3d12_objects_res_buffer_invalid_descs) {
    NEED_DEV();
    for (unsigned i = 0; i < sizeof buf_bad / sizeof *buf_bad; i++) {
        D3D12_RESOURCE_DESC d = d12_buf_desc(1024, D3D12_RESOURCE_FLAG_NONE);
        buf_mutate(i, &d);
        ID3D12Resource *r = (ID3D12Resource *)(ULONG_PTR)1;
        HRESULT hr = try_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, d, D3D12_RESOURCE_STATE_COMMON, NULL, &r);
        CHECKF(hr == E_INVALIDARG, "buffer with %s: %s", buf_bad[i].n, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(r); else CHECK(r == NULL);
    }
}
TEST(d3d12_objects_res_initial_state_rules) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_buf_desc(256, D3D12_RESOURCE_FLAG_NONE);
    struct { D3D12_HEAP_TYPE h; D3D12_RESOURCE_STATES s; HRESULT want; const char *n; } t[] = {
        { D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, S_OK, "upload/GENERIC_READ" },
        { D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_COMMON, E_INVALIDARG, "upload/COMMON" },
        { D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_COPY_DEST, E_INVALIDARG, "upload/COPY_DEST" },
        { D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_COPY_SOURCE, E_INVALIDARG, "upload/COPY_SOURCE" },
        { D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, S_OK, "readback/COPY_DEST" },
        { D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COMMON, E_INVALIDARG, "readback/COMMON" },
        { D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_GENERIC_READ, E_INVALIDARG, "readback/GENERIC_READ" },
        { D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_SOURCE, E_INVALIDARG, "readback/COPY_SOURCE" },
        { D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, S_OK, "default/COMMON" },
        { D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, S_OK, "default/COPY_DEST" },
        { D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_GENERIC_READ, S_OK, "default/GENERIC_READ" },
        { D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_SOURCE, S_OK, "default/COPY_SOURCE" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ID3D12Resource *r = NULL;
        HRESULT hr = try_buffer(dev, t[i].h, d, t[i].s, NULL, &r);
        CHECKF(hr == t[i].want, "%s: %s", t[i].n, d3dh_hr_name(hr));
        SAFE_RELEASE(r);
    }
}
TEST(d3d12_objects_res_buffer_clear_value_must_be_null) {
    NEED_DEV();
    D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ID3D12Resource *r = NULL;
    HRESULT hr = try_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, d12_buf_desc(256, 0), D3D12_RESOURCE_STATE_COMMON, &cv, &r);
    CHECKF(hr == E_INVALIDARG, "buffer with clear value: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_res_create_ppv_null_s_false) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_buf_desc(256, 0);
    D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, NULL);
    EXPECT_HR(hr, S_FALSE);
    d.Width = 0;
    hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, NULL);
    CHECKF(FAILED(hr), "invalid desc with NULL ppv: %s", d3dh_hr_name(hr));
}
TEST(d3d12_objects_res_create_wrong_iid) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_buf_desc(256, 0);
    D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    void *o = (void *)1;
    HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Fence, &o);
    EXPECT_HR(hr, E_NOINTERFACE);
    CHECK(o == NULL);
    ID3D12Object *ob = NULL;   /* any interface the resource supports is accepted */
    EXPECT_HR(ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Pageable, (void **)&ob), S_OK);
    SAFE_RELEASE(ob);
}
TEST(d3d12_objects_res_create_null_args) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_buf_desc(256, 0);
    D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    ID3D12Resource *r = NULL; HRESULT hr;
    hr = ID3D12Device_CreateCommittedResource(dev, NULL, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
    CHECKF(FAILED(hr), "NULL heap props: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, NULL, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
    CHECKF(FAILED(hr), "NULL desc: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
}
TEST(d3d12_objects_res_heap_properties_invalid) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_buf_desc(256, 0);
    struct { D3D12_HEAP_PROPERTIES hp; const char *n; D3D12_RESOURCE_STATES st; } t[4];
    memset(t, 0, sizeof t);
    t[0].hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT); t[0].hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; t[0].n = "DEFAULT with page property"; t[0].st = D3D12_RESOURCE_STATE_COMMON;
    t[1].hp = d12_heap_props(D3D12_HEAP_TYPE_UPLOAD); t[1].hp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; t[1].n = "UPLOAD with pool"; t[1].st = D3D12_RESOURCE_STATE_GENERIC_READ;
    t[2].hp = d12_heap_props(D3D12_HEAP_TYPE_CUSTOM); t[2].n = "CUSTOM with UNKNOWN page/pool"; t[2].st = D3D12_RESOURCE_STATE_COMMON;
    t[3].hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT); t[3].hp.CreationNodeMask = 2; t[3].hp.VisibleNodeMask = 2; t[3].n = "DEFAULT node mask 2"; t[3].st = D3D12_RESOURCE_STATE_COMMON;
    for (int i = 0; i < 4; i++) {
        ID3D12Resource *r = NULL;
        HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &t[i].hp, D3D12_HEAP_FLAG_NONE, &d, t[i].st, NULL, &IID_ID3D12Resource, (void **)&r);
        CHECKF(FAILED(hr), "%s: %s", t[i].n, d3dh_hr_name(hr));
        SAFE_RELEASE(r);
    }
}
TEST(d3d12_objects_res_custom_heap_matching_upload) {
    NEED_DEV();
    D3D12_HEAP_PROPERTIES hp = d12_custom_heap_props(dev, D3D12_HEAP_TYPE_UPLOAD);
    t_note("GetCustomHeapProperties(UPLOAD): type=%d cpu=%d pool=%d", hp.Type, hp.CPUPageProperty, hp.MemoryPoolPreference);
    CHECK(hp.CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE || hp.CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK);
    hp.Type = D3D12_HEAP_TYPE_CUSTOM;
    D3D12_RESOURCE_DESC d = d12_buf_desc(256, 0);
    ID3D12Resource *r = NULL;
    HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void **)&r);
    EXPECT_HR(hr, S_OK);
    if (r) {
        void *p = NULL; D3D12_RANGE none = { 0, 0 };
        EXPECT_HR(ID3D12Resource_Map(r, 0, &none, &p), S_OK);
        CHECK(p != NULL);
        ID3D12Resource_Unmap(r, 0, NULL);
        SAFE_RELEASE(r);
    }
}
TEST(d3d12_objects_res_custom_heap_props_all_types) {
    NEED_DEV();
    static const D3D12_HEAP_TYPE ht[] = { D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_TYPE_READBACK };
    for (unsigned i = 0; i < 3; i++) {
        D3D12_HEAP_PROPERTIES hp = d12_custom_heap_props(dev, ht[i]);
        t_note("custom props for heap type %d: type=%d cpu=%d pool=%d", ht[i], hp.Type, hp.CPUPageProperty, hp.MemoryPoolPreference);
        CHECK(hp.Type == D3D12_HEAP_TYPE_CUSTOM || hp.Type == ht[i]);
    }
}

/* ====== GetDesc / flags on textures ================================================================ */
static HRESULT try_tex(ID3D12Device *dev, D3D12_RESOURCE_DESC d, D3D12_RESOURCE_STATES st, const D3D12_CLEAR_VALUE *cv, ID3D12Resource **out) {
    return d12_create_committed(dev, D3D12_HEAP_TYPE_DEFAULT, &d, st, cv, out);
}
TEST(d3d12_objects_res_tex2d_roundtrip) {
    NEED_DEV();
    struct { UINT w, h, mips, arr; DXGI_FORMAT f; UINT wantmips; } t[] = {
        { 64, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1 }, { 64, 64, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 7 }, { 100, 37, 0, 1, DXGI_FORMAT_R8_UNORM, 7 },
        { 256, 256, 3, 4, DXGI_FORMAT_R16G16B16A16_FLOAT, 3 }, { 1, 1, 0, 1, DXGI_FORMAT_R32_FLOAT, 1 }, { 1024, 4, 0, 2, DXGI_FORMAT_R8G8_UNORM, 11 },
        { 16, 16, 0, 1, DXGI_FORMAT_BC1_UNORM, 5 }, { 8, 8, 0, 1, DXGI_FORMAT_BC7_UNORM, 4 }, { 17, 33, 2, 1, DXGI_FORMAT_B8G8R8A8_UNORM, 2 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ID3D12Resource *r = NULL;
        D3D12_RESOURCE_DESC d = d12_tex2d_desc(t[i].w, t[i].h, t[i].f, t[i].mips, t[i].arr, 0);
        HRESULT hr = try_tex(dev, d, D3D12_RESOURCE_STATE_COMMON, NULL, &r);
        CHECKF(hr == S_OK, "tex %ux%u fmt %d: %s", t[i].w, t[i].h, t[i].f, d3dh_hr_name(hr));
        if (r) {
            D3D12_RESOURCE_DESC g = d12_res_desc(r);
            EXPECT_EQ(g.Dimension, D3D12_RESOURCE_DIMENSION_TEXTURE2D);
            EXPECT_EQ(g.Width, t[i].w); EXPECT_EQ(g.Height, t[i].h); EXPECT_EQ(g.DepthOrArraySize, t[i].arr);
            EXPECT_EQ(g.MipLevels, t[i].wantmips); EXPECT_EQ(g.Format, t[i].f);
            EXPECT_EQ(g.SampleDesc.Count, 1); EXPECT_EQ(g.Layout, D3D12_TEXTURE_LAYOUT_UNKNOWN);
            CHECK(g.Alignment == 0 || g.Alignment == 65536 || g.Alignment == 4096);
            if (g.Alignment != 65536) t_note("GetDesc().Alignment for tex %ux%u = %llu", t[i].w, t[i].h, (unsigned long long)g.Alignment);
            SAFE_RELEASE(r);
        }
    }
}
TEST(d3d12_objects_res_tex1d) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(64, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 3, 0);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    ID3D12Resource *r = NULL;
    HRESULT hr = try_tex(dev, d, D3D12_RESOURCE_STATE_COMMON, NULL, &r);
    EXPECT_HR(hr, S_OK);
    if (r) {
        D3D12_RESOURCE_DESC g = d12_res_desc(r);
        EXPECT_EQ(g.Dimension, D3D12_RESOURCE_DIMENSION_TEXTURE1D); EXPECT_EQ(g.Width, 64); EXPECT_EQ(g.Height, 1);
        EXPECT_EQ(g.DepthOrArraySize, 3); EXPECT_EQ(g.MipLevels, 7);
        SAFE_RELEASE(r);
    }
    d.Height = 2;
    r = NULL; hr = try_tex(dev, d, D3D12_RESOURCE_STATE_COMMON, NULL, &r);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_res_tex3d) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8, 0);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    ID3D12Resource *r = NULL;
    HRESULT hr = try_tex(dev, d, D3D12_RESOURCE_STATE_COMMON, NULL, &r);
    EXPECT_HR(hr, S_OK);
    if (r) {
        D3D12_RESOURCE_DESC g = d12_res_desc(r);
        EXPECT_EQ(g.Dimension, D3D12_RESOURCE_DIMENSION_TEXTURE3D); EXPECT_EQ(g.DepthOrArraySize, 8); EXPECT_EQ(g.MipLevels, 4);
        SAFE_RELEASE(r);
    }
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    r = NULL; hr = try_tex(dev, d, D3D12_RESOURCE_STATE_COMMON, NULL, &r);
    EXPECT_HR(hr, S_OK);
    SAFE_RELEASE(r);
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; d.Format = DXGI_FORMAT_D32_FLOAT;
    r = NULL; hr = try_tex(dev, d, D3D12_RESOURCE_STATE_COMMON, NULL, &r);
    CHECKF(FAILED(hr), "3D depth-stencil: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(r);
}
static void tex_bad(unsigned i, D3D12_RESOURCE_DESC *d, const char **name) {
    switch (i) {
    case 0: d->Width = 0; *name = "width 0"; break;
    case 1: d->Height = 0; *name = "height 0"; break;
    case 2: d->DepthOrArraySize = 0; *name = "array size 0"; break;
    case 3: d->Format = DXGI_FORMAT_UNKNOWN; *name = "format UNKNOWN"; break;
    case 4: d->Format = (DXGI_FORMAT)0x7FFF; *name = "format out of range"; break;
    case 5: d->MipLevels = 20; *name = "20 mips on 64x64"; break;
    case 6: d->SampleDesc.Count = 0; *name = "sample count 0"; break;
    case 7: d->SampleDesc.Count = 3; *name = "sample count 3"; break;
    case 8: d->Width = 16385; d->Height = 1; d->Format = DXGI_FORMAT_R8_UNORM; *name = "width 16385"; break;
    case 9: d->DepthOrArraySize = 2049; d->Width = 4; d->Height = 4; d->Format = DXGI_FORMAT_R8_UNORM; *name = "array 2049"; break;
    case 10: d->Format = DXGI_FORMAT_BC1_UNORM; d->Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; *name = "RT on BC1"; break;
    case 11: d->Format = DXGI_FORMAT_BC1_UNORM; d->Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; *name = "UAV on BC1"; break;
    case 12: d->Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; *name = "DS on RGBA8"; break;
    case 13: d->Format = DXGI_FORMAT_D32_FLOAT; d->Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; *name = "DS|RT"; break;
    case 14: d->Format = DXGI_FORMAT_D32_FLOAT; d->Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; *name = "DS|UAV"; break;
    case 15: d->Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE; *name = "DENY_SRV without DS"; break;
    case 16: d->Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE; *name = "committed 64KB swizzle layout"; break;
    case 17: d->Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; d->MipLevels = 3; *name = "row-major with mips"; break;
    case 18: d->SampleDesc.Count = 4; d->Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; *name = "MSAA with UAV"; break;
    case 19: d->SampleDesc.Count = 4; d->MipLevels = 2; d->Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; *name = "MSAA with 2 mips"; break;
    case 20: d->Dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN; *name = "dimension UNKNOWN"; break;
    case 21: d->Alignment = 3; *name = "alignment 3"; break;
    }
}
TEST(d3d12_objects_res_tex2d_invalid_descs) {
    NEED_DEV();
    for (unsigned i = 0; i < 22; i++) {
        D3D12_RESOURCE_DESC d = d12_tex2d_desc(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0); const char *nm = "?";
        tex_bad(i, &d, &nm);
        ID3D12Resource *r = (ID3D12Resource *)(ULONG_PTR)1;
        HRESULT hr = try_tex(dev, d, D3D12_RESOURCE_STATE_COMMON, NULL, &r);
        CHECKF(hr == E_INVALIDARG, "texture with %s: %s", nm, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(r);
    }
}
TEST(d3d12_objects_res_tex_upload_readback_heaps_reject_textures) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);
    ID3D12Resource *r = NULL; HRESULT hr;
    hr = d12_create_committed(dev, D3D12_HEAP_TYPE_UPLOAD, &d, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &r);
    CHECKF(hr == E_INVALIDARG, "texture in UPLOAD heap: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    hr = d12_create_committed(dev, D3D12_HEAP_TYPE_READBACK, &d, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &r);
    CHECKF(hr == E_INVALIDARG, "texture in READBACK heap: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
}
TEST(d3d12_objects_res_tex_flag_combos_valid) {
    NEED_DEV();
    struct { DXGI_FORMAT f; D3D12_RESOURCE_FLAGS fl; const char *n; int clear; } t[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, "RT rgba8", 1 },
        { DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, "RT typeless", 0 },
        { DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "RT|UAV rgba32f", 1 },
        { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "UAV rgba8", 0 },
        { DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "UAV r32f", 0 },
        { DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, "DS d32", 1 },
        { DXGI_FORMAT_D16_UNORM, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, "DS d16", 1 },
        { DXGI_FORMAT_D24_UNORM_S8_UINT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, "DS d24s8", 1 },
        { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, "DS d32s8", 1 },
        { DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, "DS r32 typeless", 0 },
        { DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE, "DS|DENY_SRV", 1 },
        { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS, "SIMULTANEOUS", 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        D3D12_RESOURCE_DESC d = d12_tex2d_desc(32, 32, t[i].f, 1, 1, t[i].fl);
        D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = t[i].f;
        if (t[i].fl & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) cv.DepthStencil.Depth = 1.0f;
        ID3D12Resource *r = NULL;
        D3D12_RESOURCE_STATES st = (t[i].fl & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) ? D3D12_RESOURCE_STATE_DEPTH_WRITE :
            (t[i].fl & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COMMON;
        HRESULT hr = try_tex(dev, d, st, t[i].clear ? &cv : NULL, &r);
        if (t[i].fl & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) { t_note("%s: %s", t[i].n, d3dh_hr_name(hr)); }
        else CHECKF(hr == S_OK, "%s: %s", t[i].n, d3dh_hr_name(hr));
        if (r) { D3D12_RESOURCE_DESC g = d12_res_desc(r); EXPECT_EQ(g.Flags, t[i].fl); SAFE_RELEASE(r); }
    }
}
TEST(d3d12_objects_res_msaa_texture) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(32, 32, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    d.SampleDesc.Count = 4;
    ID3D12Resource *r = NULL;
    HRESULT hr = try_tex(dev, d, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &r);
    EXPECT_HR(hr, S_OK);
    if (r) {
        D3D12_RESOURCE_DESC g = d12_res_desc(r);
        EXPECT_EQ(g.SampleDesc.Count, 4);
        CHECK(g.Alignment == 0 || g.Alignment == 65536 || g.Alignment == 4 * MB);
        t_note("MSAA 4x 32x32 GetDesc alignment %llu", (unsigned long long)g.Alignment);
        SAFE_RELEASE(r);
    }
}
TEST(d3d12_objects_res_clear_value_validation) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_R8G8B8A8_UNORM; cv.Color[0] = 0.25f; cv.Color[3] = 1;
    ID3D12Resource *r = NULL;
    EXPECT_HR(try_tex(dev, d, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, &r), S_OK); SAFE_RELEASE(r);
    cv.Format = DXGI_FORMAT_UNKNOWN;
    HRESULT hr = try_tex(dev, d, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, &r);
    CHECKF(FAILED(hr), "clear value format UNKNOWN: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    cv.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;   /* not castable to the resource format */
    hr = try_tex(dev, d, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, &r);
    t_note("clear value format of different bit width: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    /* depth clear value: depth must be within [0,1] */
    D3D12_RESOURCE_DESC dd = d12_tex2d_desc(16, 16, DXGI_FORMAT_D32_FLOAT, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_D32_FLOAT; cv.DepthStencil.Depth = 0.5f;
    EXPECT_HR(try_tex(dev, dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, &r), S_OK); SAFE_RELEASE(r);
    cv.DepthStencil.Depth = 2.0f;
    hr = try_tex(dev, dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, &r);
    t_note("depth clear value 2.0: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
}
TEST(d3d12_objects_res_heap_flags_restrict_resource) {
    NEED_DEV();
    D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC b = d12_buf_desc(256, 0), t = d12_tex2d_desc(16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);
    D3D12_RESOURCE_DESC rt = d12_tex2d_desc(16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    struct { const D3D12_RESOURCE_DESC *d; D3D12_HEAP_FLAGS hf; const char *n; D3D12_RESOURCE_STATES st; } c[] = {
        { &t, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS, "texture in buffers-only", D3D12_RESOURCE_STATE_COMMON },
        { &b, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES, "buffer in textures-only", D3D12_RESOURCE_STATE_COMMON },
        { &b, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, "buffer in RT/DS-only", D3D12_RESOURCE_STATE_COMMON },
        { &rt, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES, "RT texture in non-RT-only", D3D12_RESOURCE_STATE_RENDER_TARGET },
        { &t, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, "plain texture in RT/DS-only", D3D12_RESOURCE_STATE_COMMON },
    };
    for (unsigned i = 0; i < 5; i++) {
        ID3D12Resource *r = NULL;
        HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &hp, c[i].hf, c[i].d, c[i].st, NULL, &IID_ID3D12Resource, (void **)&r);
        CHECKF(FAILED(hr), "%s: %s", c[i].n, d3dh_hr_name(hr));
        SAFE_RELEASE(r);
    }
    /* positive counterparts */
    ID3D12Resource *r = NULL;
    EXPECT_HR(ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS, &b, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r), S_OK); SAFE_RELEASE(r);
    EXPECT_HR(ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES, &t, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r), S_OK); SAFE_RELEASE(r);
    EXPECT_HR(ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, &rt, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &IID_ID3D12Resource, (void **)&r), S_OK); SAFE_RELEASE(r);
}

/* ====== GetHeapProperties ======================================================================= */
TEST(d3d12_objects_res_get_heap_properties_committed) {
    NEED_DEV();
    static const D3D12_HEAP_TYPE ht[] = { D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_TYPE_READBACK };
    for (unsigned i = 0; i < 3; i++) {
        ID3D12Resource *r = NULL;
        if (FAILED(d12_make_buffer(dev, ht[i], 256, 0, &r))) continue;
        D3D12_HEAP_PROPERTIES hp; D3D12_HEAP_FLAGS hf = (D3D12_HEAP_FLAGS)0xCDCDCDCD;
        memset(&hp, 0, sizeof hp);
        HRESULT hr = ID3D12Resource_GetHeapProperties(r, &hp, &hf);
        EXPECT_HR(hr, S_OK);
        if (SUCCEEDED(hr)) {
            EXPECT_EQ(hp.Type, ht[i]);
            EXPECT_EQ(hf, D3D12_HEAP_FLAG_NONE);
            EXPECT_EQ(hp.CreationNodeMask, 1); EXPECT_EQ(hp.VisibleNodeMask, 1);
        }
        EXPECT_HR(ID3D12Resource_GetHeapProperties(r, NULL, NULL), S_OK);
        EXPECT_HR(ID3D12Resource_GetHeapProperties(r, &hp, NULL), S_OK);
        EXPECT_HR(ID3D12Resource_GetHeapProperties(r, NULL, &hf), S_OK);
        SAFE_RELEASE(r);
    }
}
TEST(d3d12_objects_res_get_heap_properties_flags_roundtrip) {
    NEED_DEV();
    D3D12_HEAP_PROPERTIES hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT); D3D12_RESOURCE_DESC d = d12_buf_desc(256, 0);
    ID3D12Resource *r = NULL;
    if (FAILED(ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r))) { t_skip("create failed"); return; }
    D3D12_HEAP_FLAGS hf = 0;
    EXPECT_HR(ID3D12Resource_GetHeapProperties(r, NULL, &hf), S_OK);
    EXPECT_EQ(hf, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    SAFE_RELEASE(r);
}

/* ====== GPU virtual addresses ===================================================================== */
TEST(d3d12_objects_res_gpu_va_buffers) {
    NEED_DEV();
    static const D3D12_HEAP_TYPE ht[] = { D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_TYPE_READBACK };
    D3D12_GPU_VIRTUAL_ADDRESS va[3]; UINT64 sz = 4096; ID3D12Resource *r[3] = { 0 };
    for (unsigned i = 0; i < 3; i++) {
        EXPECT_HR(d12_make_buffer(dev, ht[i], sz, 0, &r[i]), S_OK);
        va[i] = r[i] ? ID3D12Resource_GetGPUVirtualAddress(r[i]) : 0;
        CHECKF(va[i] != 0, "buffer %u va", i);
        if (r[i]) EXPECT_EQ(ID3D12Resource_GetGPUVirtualAddress(r[i]), va[i]);
    }
    for (unsigned i = 0; i < 3; i++) for (unsigned j = i + 1; j < 3; j++)
        CHECKF(va[i] + sz <= va[j] || va[j] + sz <= va[i], "VA ranges overlap %u/%u", i, j);
    for (unsigned i = 0; i < 3; i++) SAFE_RELEASE(r[i]);
}
TEST(d3d12_objects_res_gpu_va_texture) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    if (FAILED(d12_make_tex2d(dev, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0, D3D12_RESOURCE_STATE_COMMON, NULL, &r))) { t_skip("no texture"); return; }
    D3D12_GPU_VIRTUAL_ADDRESS va = ID3D12Resource_GetGPUVirtualAddress(r);
    t_note("GetGPUVirtualAddress of a texture: 0x%llx (documented for buffers only)", (unsigned long long)va);
    SAFE_RELEASE(r);
}

/* ====== heaps ========================================================================================= */
static D3D12_HEAP_DESC hdesc(D3D12_HEAP_TYPE t, UINT64 size, UINT64 align, D3D12_HEAP_FLAGS f) {
    D3D12_HEAP_DESC d; memset(&d, 0, sizeof d);
    d.SizeInBytes = size; d.Alignment = align; d.Flags = f; d.Properties = d12_heap_props(t);
    return d;
}
TEST(d3d12_objects_heap_create_types_and_desc) {
    NEED_DEV();
    static const D3D12_HEAP_TYPE ht[] = { D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_TYPE_READBACK };
    static const UINT64 sizes[] = { 64 * KB, 128 * KB, 1 * MB, 4 * MB };
    for (unsigned i = 0; i < 3; i++) for (unsigned j = 0; j < 4; j++) {
        D3D12_HEAP_DESC d = hdesc(ht[i], sizes[j], 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
        ID3D12Heap *h = NULL;
        HRESULT hr = ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, (void **)&h);
        CHECKF(hr == S_OK, "heap type %d size %llu: %s", ht[i], (unsigned long long)sizes[j], d3dh_hr_name(hr));
        if (h) {
            D3D12_HEAP_DESC g = d12_heap_desc(h);
            EXPECT_EQ(g.SizeInBytes, sizes[j]); EXPECT_EQ(g.Properties.Type, ht[i]); EXPECT_EQ(g.Alignment, 64 * KB);
            EXPECT_EQ(g.Flags, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
            SAFE_RELEASE(h);
        }
    }
}
TEST(d3d12_objects_heap_alignment_zero_defaults) {
    NEED_DEV();
    D3D12_HEAP_DESC d = hdesc(D3D12_HEAP_TYPE_DEFAULT, 64 * KB, 0, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    ID3D12Heap *h = NULL;
    EXPECT_HR(ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, (void **)&h), S_OK);
    if (h) { D3D12_HEAP_DESC g = d12_heap_desc(h); t_note("GetDesc().Alignment after creation with 0: %llu", (unsigned long long)g.Alignment); CHECK(g.Alignment == 0 || g.Alignment == 64 * KB); SAFE_RELEASE(h); }
}
TEST(d3d12_objects_heap_invalid_sizes_alignments) {
    NEED_DEV();
    struct { UINT64 size, align; const char *n; } t[] = {
        { 0, 64 * KB, "size 0" }, { 64 * KB + 1, 64 * KB, "size not multiple of alignment" }, { 100, 64 * KB, "size 100" },
        { 64 * KB, 4096, "alignment 4096" }, { 64 * KB, 3, "alignment 3" }, { 64 * KB, 128 * KB, "alignment 128K" }, { 64 * KB, 32 * KB, "alignment 32K" },
        { 4 * MB + 64 * KB, 4 * MB, "4MB alignment with size not multiple" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        D3D12_HEAP_DESC d = hdesc(D3D12_HEAP_TYPE_DEFAULT, t[i].size, t[i].align, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
        ID3D12Heap *h = (ID3D12Heap *)(ULONG_PTR)1;
        HRESULT hr = ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, (void **)&h);
        CHECKF(hr == E_INVALIDARG, "heap %s: %s", t[i].n, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(h);
    }
}
TEST(d3d12_objects_heap_4mb_alignment) {
    NEED_DEV();
    D3D12_HEAP_DESC d = hdesc(D3D12_HEAP_TYPE_DEFAULT, 4 * MB, 4 * MB, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES);
    ID3D12Heap *h = NULL;
    HRESULT hr = ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, (void **)&h);
    t_note("4MB-aligned 4MB heap: %s", d3dh_hr_name(hr));
    if (h) { D3D12_HEAP_DESC g = d12_heap_desc(h); EXPECT_EQ(g.Alignment, 4 * MB); EXPECT_EQ(g.SizeInBytes, 4 * MB); SAFE_RELEASE(h); }
}
TEST(d3d12_objects_heap_property_validation) {
    NEED_DEV();
    struct { D3D12_HEAP_PROPERTIES hp; const char *n; } t[6];
    memset(t, 0, sizeof t);
    t[0].hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT); t[0].hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE; t[0].n = "DEFAULT + CPU page property";
    t[1].hp = d12_heap_props(D3D12_HEAP_TYPE_UPLOAD); t[1].hp.MemoryPoolPreference = D3D12_MEMORY_POOL_L1; t[1].n = "UPLOAD + memory pool";
    t[2].hp = d12_heap_props(D3D12_HEAP_TYPE_READBACK); t[2].hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; t[2].n = "READBACK + CPU page property";
    t[3].hp = d12_heap_props(D3D12_HEAP_TYPE_CUSTOM); t[3].n = "CUSTOM with UNKNOWN/UNKNOWN";
    t[4].hp = d12_heap_props(D3D12_HEAP_TYPE_CUSTOM); t[4].hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; t[4].n = "CUSTOM with UNKNOWN pool";
    t[5].hp = d12_heap_props(D3D12_HEAP_TYPE_DEFAULT); t[5].hp.CreationNodeMask = 4; t[5].hp.VisibleNodeMask = 4; t[5].n = "node mask 4";
    for (int i = 0; i < 6; i++) {
        D3D12_HEAP_DESC d = hdesc(D3D12_HEAP_TYPE_DEFAULT, 64 * KB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS); d.Properties = t[i].hp;
        ID3D12Heap *h = NULL;
        HRESULT hr = ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, (void **)&h);
        CHECKF(hr == E_INVALIDARG, "%s: %s", t[i].n, d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(h);
    }
}
TEST(d3d12_objects_heap_conflicting_flags) {
    NEED_DEV();
    static const D3D12_HEAP_FLAGS bad[] = {
        D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS | D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES,
        D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS | D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
        D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
        D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES,
    };
    for (unsigned i = 0; i < 4; i++) {
        D3D12_HEAP_DESC d = hdesc(D3D12_HEAP_TYPE_DEFAULT, 64 * KB, 64 * KB, bad[i]);
        ID3D12Heap *h = NULL;
        HRESULT hr = ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, (void **)&h);
        CHECKF(FAILED(hr), "heap flags 0x%x: %s", bad[i], d3dh_hr_name(hr));
        if (SUCCEEDED(hr)) SAFE_RELEASE(h);
    }
}
TEST(d3d12_objects_heap_tier_flags) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS o; memset(&o, 0, sizeof o);
    HRESULT hr = ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_D3D12_OPTIONS, &o, sizeof o);
    EXPECT_HR(hr, S_OK);
    if (FAILED(hr)) return;
    t_note("ResourceHeapTier=%d TiledResourcesTier=%d ResourceBindingTier=%d", o.ResourceHeapTier, o.TiledResourcesTier, o.ResourceBindingTier);
    CHECK(o.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_1 || o.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_2);
    D3D12_HEAP_DESC d = hdesc(D3D12_HEAP_TYPE_DEFAULT, 1 * MB, 64 * KB, D3D12_HEAP_FLAG_NONE);   /* mixed buffers/textures */
    ID3D12Heap *h = NULL;
    hr = ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, (void **)&h);
    if (o.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_2) EXPECT_HR(hr, S_OK);
    else CHECKF(hr == E_INVALIDARG, "tier-1 heap with FLAG_NONE: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(h);
}
TEST(d3d12_objects_heap_ppv_null_s_false) {
    NEED_DEV();
    D3D12_HEAP_DESC d = hdesc(D3D12_HEAP_TYPE_DEFAULT, 64 * KB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    EXPECT_HR(ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, NULL), S_FALSE);
    d.SizeInBytes = 0;
    HRESULT hr = ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, NULL);
    CHECKF(FAILED(hr), "invalid desc, NULL ppv: %s", d3dh_hr_name(hr));
    { HRESULT nh = ID3D12Device_CreateHeap(dev, NULL, &IID_ID3D12Heap, NULL); CHECKF(FAILED(nh), "NULL desc: %s", d3dh_hr_name(nh)); }
}
TEST(d3d12_objects_heap_iids_and_device) {
    NEED_DEV();
    D3D12_HEAP_DESC d = hdesc(D3D12_HEAP_TYPE_DEFAULT, 64 * KB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    ID3D12Heap *h = NULL; void *o;
    EXPECT_HR(ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Heap, (void **)&h), S_OK);
    if (!h) return;
    o = NULL; EXPECT_HR(ID3D12Heap_QueryInterface(h, &IID_ID3D12Pageable, &o), S_OK); if (o) ((IUnknown *)o)->lpVtbl->Release(o);
    o = (void *)1; EXPECT_HR(ID3D12Heap_QueryInterface(h, &IID_ID3D12Resource, &o), E_NOINTERFACE); CHECK(o == NULL);
    ID3D12Device *dv = NULL;
    EXPECT_HR(ID3D12Heap_GetDevice(h, &IID_ID3D12Device, (void **)&dv), S_OK);
    CHECK(dv == dev); if (dv) ID3D12Device_Release(dv);
    SAFE_RELEASE(h);
    void *oo = (void *)1;
    EXPECT_HR(ID3D12Device_CreateHeap(dev, &d, &IID_ID3D12Fence, &oo), E_NOINTERFACE);
}

/* ====== placed resources ================================================================================ */
TEST(d3d12_objects_placed_buffers_offsets) {
    NEED_DEV();
    D3D12_HEAP_DESC hd = hdesc(D3D12_HEAP_TYPE_DEFAULT, 256 * KB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    ID3D12Heap *h = NULL;
    if (FAILED(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h))) { t_skip("no heap"); return; }
    D3D12_RESOURCE_DESC d = d12_buf_desc(64 * KB, 0);
    struct { UINT64 off; UINT64 size; int ok; const char *n; } t[] = {
        { 0, 64 * KB, 1, "offset 0" }, { 64 * KB, 64 * KB, 1, "offset 64K" }, { 192 * KB, 64 * KB, 1, "last slot" },
        { 0, 256 * KB, 1, "whole heap" }, { 128 * KB, 128 * KB, 1, "upper half" },
        { 32 * KB, 64 * KB, 0, "offset 32K not multiple of 64K" }, { 1, 64 * KB, 0, "offset 1" }, { 192 * KB, 128 * KB, 0, "exceeds heap end" },
        { 0, 256 * KB + 1, 0, "bigger than heap" }, { 256 * KB, 64 * KB, 0, "offset at heap end" }, { 320 * KB, 64 * KB, 0, "offset past heap" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        d.Width = t[i].size;
        ID3D12Resource *r = NULL;
        HRESULT hr = ID3D12Device_CreatePlacedResource(dev, h, t[i].off, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
        if (t[i].ok) CHECKF(hr == S_OK, "placed %s: %s", t[i].n, d3dh_hr_name(hr));
        else CHECKF(hr == E_INVALIDARG, "placed %s: %s", t[i].n, d3dh_hr_name(hr));
        SAFE_RELEASE(r);
    }
    SAFE_RELEASE(h);
}
TEST(d3d12_objects_placed_gpu_va_offsets) {
    NEED_DEV();
    D3D12_HEAP_DESC hd = hdesc(D3D12_HEAP_TYPE_DEFAULT, 256 * KB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    ID3D12Heap *h = NULL;
    if (FAILED(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h))) { t_skip("no heap"); return; }
    D3D12_RESOURCE_DESC d = d12_buf_desc(64 * KB, 0);
    ID3D12Resource *a = NULL, *b = NULL, *c = NULL;
    EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&a), S_OK);
    EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 64 * KB, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&b), S_OK);
    EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&c), S_OK);   /* aliases a */
    if (a && b && c) {
        D3D12_GPU_VIRTUAL_ADDRESS va = ID3D12Resource_GetGPUVirtualAddress(a), vb = ID3D12Resource_GetGPUVirtualAddress(b), vc = ID3D12Resource_GetGPUVirtualAddress(c);
        CHECK(va != 0);
        EXPECT_EQ(vb, va + 64 * KB);
        EXPECT_EQ(vc, va);
    }
    SAFE_RELEASE(a); SAFE_RELEASE(b); SAFE_RELEASE(c); SAFE_RELEASE(h);
}
TEST(d3d12_objects_placed_wrong_heap_kind) {
    NEED_DEV();
    D3D12_HEAP_DESC hd = hdesc(D3D12_HEAP_TYPE_DEFAULT, 1 * MB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    ID3D12Heap *h = NULL;
    if (FAILED(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h))) { t_skip("no heap"); return; }
    D3D12_RESOURCE_DESC t = d12_tex2d_desc(16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);
    ID3D12Resource *r = NULL;
    HRESULT hr = ID3D12Device_CreatePlacedResource(dev, h, 0, &t, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
    CHECKF(FAILED(hr), "texture in a buffers-only heap: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    hr = ID3D12Device_CreatePlacedResource(dev, NULL, 0, &t, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
    CHECKF(FAILED(hr), "NULL heap: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    SAFE_RELEASE(h);
}
TEST(d3d12_objects_placed_initial_state_rules) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_buf_desc(64 * KB, 0);
    struct { D3D12_HEAP_TYPE t; D3D12_RESOURCE_STATES s; HRESULT want; const char *n; } c[] = {
        { D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, S_OK, "upload/GENERIC_READ" },
        { D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_COMMON, E_INVALIDARG, "upload/COMMON" },
        { D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, S_OK, "readback/COPY_DEST" },
        { D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COMMON, E_INVALIDARG, "readback/COMMON" },
        { D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, S_OK, "default/COMMON" },
    };
    for (unsigned i = 0; i < 5; i++) {
        D3D12_HEAP_DESC hd = hdesc(c[i].t, 64 * KB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
        ID3D12Heap *h = NULL; ID3D12Resource *r = NULL;
        if (FAILED(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h))) continue;
        HRESULT hr = ID3D12Device_CreatePlacedResource(dev, h, 0, &d, c[i].s, NULL, &IID_ID3D12Resource, (void **)&r);
        CHECKF(hr == c[i].want, "%s: %s", c[i].n, d3dh_hr_name(hr));
        SAFE_RELEASE(r); SAFE_RELEASE(h);
    }
}
TEST(d3d12_objects_placed_textures_use_allocation_info) {
    NEED_DEV();
    D3D12_RESOURCE_DESC t = d12_tex2d_desc(128, 128, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);
    D3D12_RESOURCE_DESC rt = d12_tex2d_desc(128, 128, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_RESOURCE_ALLOCATION_INFO it = d12_alloc_info(dev, &t, 1), irt = d12_alloc_info(dev, &rt, 1);
    CHECK(it.SizeInBytes >= 128 * 128 * 4 && it.SizeInBytes != ~0ull);
    CHECK(irt.SizeInBytes >= 128 * 128 * 4 && irt.SizeInBytes != ~0ull);
    UINT64 size = ALIGN_UP(it.SizeInBytes > irt.SizeInBytes ? it.SizeInBytes : irt.SizeInBytes, 64 * KB);
    D3D12_HEAP_DESC hd = hdesc(D3D12_HEAP_TYPE_DEFAULT, size, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);
    ID3D12Heap *h = NULL; ID3D12Resource *r = NULL;
    EXPECT_HR(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h), S_OK);
    if (h) {
        EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &t, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r), S_OK);
        SAFE_RELEASE(r);
        HRESULT hr = ID3D12Device_CreatePlacedResource(dev, h, 0, &rt, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &IID_ID3D12Resource, (void **)&r);
        CHECKF(FAILED(hr), "RT texture in a non-RT/DS heap: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
        SAFE_RELEASE(h);
    }
    hd.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
    EXPECT_HR(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h), S_OK);
    if (h) {
        D3D12_CLEAR_VALUE cv; memset(&cv, 0, sizeof cv); cv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &rt, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, &IID_ID3D12Resource, (void **)&r), S_OK);
        if (r) {
            D3D12_HEAP_PROPERTIES hp; D3D12_HEAP_FLAGS hf;
            HRESULT hr = ID3D12Resource_GetHeapProperties(r, &hp, &hf);
            t_note("GetHeapProperties on a placed resource: %s type=%d flags=0x%x", d3dh_hr_name(hr), hp.Type, hf);
        }
        SAFE_RELEASE(r);
        SAFE_RELEASE(h);
    }
}
TEST(d3d12_objects_placed_ppv_null_s_false) {
    NEED_DEV();
    D3D12_HEAP_DESC hd = hdesc(D3D12_HEAP_TYPE_DEFAULT, 64 * KB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    ID3D12Heap *h = NULL;
    if (FAILED(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h))) { t_skip("no heap"); return; }
    D3D12_RESOURCE_DESC d = d12_buf_desc(64 * KB, 0);
    EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, NULL), S_FALSE);
    SAFE_RELEASE(h);
}
TEST(d3d12_objects_placed_heap_lifetime) {
    NEED_DEV();
    D3D12_HEAP_DESC hd = hdesc(D3D12_HEAP_TYPE_UPLOAD, 64 * KB, 64 * KB, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
    ID3D12Heap *h = NULL; ID3D12Resource *r = NULL;
    if (FAILED(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h))) { t_skip("no heap"); return; }
    D3D12_RESOURCE_DESC d = d12_buf_desc(4096, 0);
    EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &d, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void **)&r), S_OK);
    SAFE_RELEASE(h);   /* the placed resource keeps working after the app released its heap reference */
    if (r) {
        void *p = NULL; D3D12_RANGE none = { 0, 0 };
        EXPECT_HR(ID3D12Resource_Map(r, 0, &none, &p), S_OK);
        if (p) { memset(p, 0x5A, 4096); CHECK(((unsigned char *)p)[100] == 0x5A); }
        ID3D12Resource_Unmap(r, 0, NULL);
        SAFE_RELEASE(r);
    }
}

/* ====== allocation info ================================================================================== */
TEST(d3d12_objects_allocinfo_buffers) {
    NEED_DEV();
    static const struct { UINT64 w, size; } t[] = { { 1, 65536 }, { 100, 65536 }, { 65536, 65536 }, { 65537, 131072 }, { 1 << 20, 1 << 20 }, { (1 << 20) + 1, (1 << 20) + 65536 } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        D3D12_RESOURCE_DESC d = d12_buf_desc(t[i].w, 0);
        D3D12_RESOURCE_ALLOCATION_INFO ai = d12_alloc_info(dev, &d, 1);
        CHECKF(ai.Alignment == 65536 && ai.SizeInBytes == t[i].size, "buffer %llu: size %llu align %llu", (unsigned long long)t[i].w, (unsigned long long)ai.SizeInBytes, (unsigned long long)ai.Alignment);
    }
}
TEST(d3d12_objects_allocinfo_multiple_descs) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d[3] = { d12_buf_desc(100, 0), d12_buf_desc(65536, 0), d12_buf_desc(65537, 0) };
    D3D12_RESOURCE_ALLOCATION_INFO ai = d12_alloc_info(dev, d, 3);
    EXPECT_EQ(ai.Alignment, 65536);
    EXPECT_EQ(ai.SizeInBytes, 65536 + 65536 + 131072);
    ai = d12_alloc_info(dev, d, 2);
    EXPECT_EQ(ai.SizeInBytes, 131072);
}
TEST(d3d12_objects_allocinfo_textures) {
    NEED_DEV();
    struct { UINT w, h; DXGI_FORMAT f; UINT mips; UINT arr; } t[] = {
        { 256, 256, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1 }, { 256, 256, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1 }, { 1024, 1024, DXGI_FORMAT_BC1_UNORM, 0, 1 },
        { 512, 512, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1 }, { 64, 64, DXGI_FORMAT_R8_UNORM, 0, 6 }, { 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        D3D12_RESOURCE_DESC d = d12_tex2d_desc(t[i].w, t[i].h, t[i].f, t[i].mips, t[i].arr, 0);
        D3D12_RESOURCE_ALLOCATION_INFO ai = d12_alloc_info(dev, &d, 1);
        UINT64 minbytes = (UINT64)t[i].w * t[i].h * t[i].arr * (t[i].f == DXGI_FORMAT_BC1_UNORM ? 1 : d3dh_format_bytes(t[i].f)) / (t[i].f == DXGI_FORMAT_BC1_UNORM ? 2 : 1);
        CHECKF(ai.Alignment == 65536, "tex %u: alignment %llu", i, (unsigned long long)ai.Alignment);
        CHECKF(ai.SizeInBytes >= minbytes && ai.SizeInBytes != ~0ull, "tex %u: size %llu < %llu", i, (unsigned long long)ai.SizeInBytes, (unsigned long long)minbytes);
        CHECKF(ai.SizeInBytes % ai.Alignment == 0, "tex %u: size %llu not a multiple of alignment", i, (unsigned long long)ai.SizeInBytes);
        t_note("tex %ux%u fmt %d mips %u arr %u: size %llu align %llu", t[i].w, t[i].h, t[i].f, t[i].mips, t[i].arr, (unsigned long long)ai.SizeInBytes, (unsigned long long)ai.Alignment);
    }
}
TEST(d3d12_objects_allocinfo_small_texture_alignment) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);
    D3D12_RESOURCE_ALLOCATION_INFO a0 = d12_alloc_info(dev, &d, 1);
    EXPECT_EQ(a0.Alignment, 65536);
    d.Alignment = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;
    D3D12_RESOURCE_ALLOCATION_INFO a1 = d12_alloc_info(dev, &d, 1);
    t_note("16x16 RGBA8 with Alignment=4096: size %llu align %llu", (unsigned long long)a1.SizeInBytes, (unsigned long long)a1.Alignment);
    CHECK(a1.Alignment == 4096 || a1.Alignment == 65536);
    CHECK(a1.SizeInBytes >= 16 * 16 * 4);
}
TEST(d3d12_objects_allocinfo_msaa_alignment) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(256, 256, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    d.SampleDesc.Count = 4;
    D3D12_RESOURCE_ALLOCATION_INFO a0 = d12_alloc_info(dev, &d, 1);
    t_note("MSAA4 256x256 Alignment=0: size %llu align %llu", (unsigned long long)a0.SizeInBytes, (unsigned long long)a0.Alignment);
    CHECK(a0.Alignment == 65536 || a0.Alignment == 4 * MB);
    CHECK(a0.SizeInBytes >= 256 * 256 * 4 * 4);
    d.Alignment = 4 * MB;
    D3D12_RESOURCE_ALLOCATION_INFO a1 = d12_alloc_info(dev, &d, 1);
    EXPECT_EQ(a1.Alignment, 4 * MB);
    CHECK(a1.SizeInBytes >= 256 * 256 * 4 * 4);
    CHECK(a1.SizeInBytes % (4 * MB) == 0);
}
TEST(d3d12_objects_allocinfo_invalid_desc) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_buf_desc(0, 0);
    D3D12_RESOURCE_ALLOCATION_INFO ai = d12_alloc_info(dev, &d, 1);
    t_note("allocation info of an invalid desc: size %llx align %llx", (unsigned long long)ai.SizeInBytes, (unsigned long long)ai.Alignment);
    CHECK(ai.SizeInBytes == ~0ull || ai.SizeInBytes == 0 || ai.SizeInBytes == 65536);
}
TEST(d3d12_objects_allocinfo_matches_committed_requirements) {
    NEED_DEV();
    /* a placed resource at offset 0 of a heap of exactly the reported size must be creatable */
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(200, 100, DXGI_FORMAT_R16G16_FLOAT, 0, 1, 0);
    D3D12_RESOURCE_ALLOCATION_INFO ai = d12_alloc_info(dev, &d, 1);
    if (ai.SizeInBytes == ~0ull || (ai.Alignment != 65536 && ai.Alignment != 4 * MB)) { t_skip("unexpected allocation info"); return; }
    D3D12_HEAP_DESC hd = hdesc(D3D12_HEAP_TYPE_DEFAULT, ai.SizeInBytes, ai.Alignment, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);
    ID3D12Heap *h = NULL; ID3D12Resource *r = NULL;
    EXPECT_HR(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h), S_OK);
    if (h) {
        EXPECT_HR(ID3D12Device_CreatePlacedResource(dev, h, 0, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r), S_OK);
        SAFE_RELEASE(r);
        hd.SizeInBytes = ai.SizeInBytes - 64 * KB;
        SAFE_RELEASE(h);
        if (hd.SizeInBytes >= 64 * KB) {
            EXPECT_HR(ID3D12Device_CreateHeap(dev, &hd, &IID_ID3D12Heap, (void **)&h), S_OK);
            if (h) {
                HRESULT hr = ID3D12Device_CreatePlacedResource(dev, h, 0, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
                CHECKF(FAILED(hr), "placed resource in a heap smaller than the reported size: %s", d3dh_hr_name(hr));
                SAFE_RELEASE(r); SAFE_RELEASE(h);
            }
        }
    }
}

/* ====== copyable footprints ================================================================================= */
typedef struct { DXGI_FORMAT f; UINT bw, bh, bytes; const char *n; } fmtinfo;
static const fmtinfo g_fmts[] = {
    { DXGI_FORMAT_R8_UNORM, 1, 1, 1, "R8" }, { DXGI_FORMAT_R8G8_UNORM, 1, 1, 2, "R8G8" }, { DXGI_FORMAT_R16_FLOAT, 1, 1, 2, "R16F" },
    { DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 4, "RGBA8" }, { DXGI_FORMAT_B8G8R8A8_UNORM, 1, 1, 4, "BGRA8" }, { DXGI_FORMAT_R10G10B10A2_UNORM, 1, 1, 4, "R10G10B10A2" },
    { DXGI_FORMAT_R11G11B10_FLOAT, 1, 1, 4, "R11G11B10" }, { DXGI_FORMAT_R16G16B16A16_FLOAT, 1, 1, 8, "RGBA16F" }, { DXGI_FORMAT_R32_FLOAT, 1, 1, 4, "R32F" },
    { DXGI_FORMAT_R32G32_FLOAT, 1, 1, 8, "RG32F" }, { DXGI_FORMAT_R32G32B32_FLOAT, 1, 1, 12, "RGB32F" }, { DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1, 16, "RGBA32F" },
    { DXGI_FORMAT_D32_FLOAT, 1, 1, 4, "D32" }, { DXGI_FORMAT_D16_UNORM, 1, 1, 2, "D16" }, { DXGI_FORMAT_A8_UNORM, 1, 1, 1, "A8" },
    { DXGI_FORMAT_BC1_UNORM, 4, 4, 8, "BC1" }, { DXGI_FORMAT_BC2_UNORM, 4, 4, 16, "BC2" }, { DXGI_FORMAT_BC3_UNORM, 4, 4, 16, "BC3" },
    { DXGI_FORMAT_BC4_UNORM, 4, 4, 8, "BC4" }, { DXGI_FORMAT_BC5_UNORM, 4, 4, 16, "BC5" }, { DXGI_FORMAT_BC7_UNORM, 4, 4, 16, "BC7" },
};
static UINT mdim(UINT v, UINT m) { v >>= m; return v ? v : 1; }
static UINT chain_len(UINT w, UINT h, UINT d) { UINT m = 1, mx = w > h ? w : h; if (d > mx) mx = d; while (mx > 1) { mx >>= 1; m++; } return m; }

/* Compare GetCopyableFootprints output for every subresource against the documented layout rules:
 * rows are padded to 256 bytes, each subresource starts on a 512-byte boundary, total ends at the last row's data. */
static void check_footprints(ID3D12Device *dev, const D3D12_RESOURCE_DESC *d, const fmtinfo *fi, UINT64 base, const char *label) {
    UINT mips = d->MipLevels ? d->MipLevels : chain_len((UINT)d->Width, d->Height, d->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? d->DepthOrArraySize : 1);
    UINT arr = d->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : d->DepthOrArraySize;
    UINT n = mips * arr;
    if (n > 64) { t_note("%s: too many subresources", label); return; }
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp[64]; UINT rows[64]; UINT64 rsz[64], total = 0xCDCDCDCD;
    memset(fp, 0xCD, sizeof fp);
    ID3D12Device_GetCopyableFootprints(dev, d, 0, n, base, fp, rows, rsz, &total);
    UINT64 off = base, last_end = 0;
    for (UINT i = 0; i < n; i++) {
        UINT m = i % mips;
        UINT w = mdim((UINT)d->Width, m), h = mdim(d->Height, m);
        UINT dp = d->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? mdim(d->DepthOrArraySize, m) : 1;
        UINT wb = (w + fi->bw - 1) / fi->bw, hb = (h + fi->bh - 1) / fi->bh;
        UINT64 rowsize = (UINT64)wb * fi->bytes, pitch = ALIGN_UP(rowsize, 256);
        UINT64 size = (UINT64)(dp - 1) * hb * pitch + (UINT64)(hb - 1) * pitch + rowsize;
        CHECKF(fp[i].Offset == off, "%s sub %u: offset %llu want %llu", label, i, (unsigned long long)fp[i].Offset, (unsigned long long)off);
        CHECKF(fp[i].Footprint.Format == d->Format, "%s sub %u: format %d", label, i, fp[i].Footprint.Format);
        CHECKF(fp[i].Footprint.RowPitch == pitch, "%s sub %u: pitch %u want %llu", label, i, fp[i].Footprint.RowPitch, (unsigned long long)pitch);
        CHECKF(fp[i].Footprint.Depth == dp, "%s sub %u: depth %u want %u", label, i, fp[i].Footprint.Depth, dp);
        if (fi->bw == 1) CHECKF(fp[i].Footprint.Width == w && fp[i].Footprint.Height == h, "%s sub %u: size %ux%u want %ux%u", label, i, fp[i].Footprint.Width, fp[i].Footprint.Height, w, h);
        else CHECKF(fp[i].Footprint.Width >= w && fp[i].Footprint.Width < w + fi->bw, "%s sub %u: block width %u for %u", label, i, fp[i].Footprint.Width, w);
        CHECKF(rows[i] == hb, "%s sub %u: rows %u want %u", label, i, rows[i], hb);
        CHECKF(rsz[i] == rowsize, "%s sub %u: rowsize %llu want %llu", label, i, (unsigned long long)rsz[i], (unsigned long long)rowsize);
        last_end = off + size;
        off = ALIGN_UP(off + size, 512);
    }
    CHECKF(total == last_end - base, "%s: total %llu want %llu", label, (unsigned long long)total, (unsigned long long)(last_end - base));
}
TEST(d3d12_objects_footprint_formats_2d) {
    NEED_DEV();
    static const UINT dims[][2] = { { 64, 64 }, { 17, 5 }, { 1, 1 }, { 256, 1 }, { 33, 65 }, { 100, 100 } };
    for (unsigned f = 0; f < sizeof g_fmts / sizeof *g_fmts; f++) for (unsigned s = 0; s < 6; s++) {
        D3D12_RESOURCE_DESC d = d12_tex2d_desc(dims[s][0], dims[s][1], g_fmts[f].f, 1, 1, 0);
        char lb[64]; wsprintfA(lb, "%s %ux%u", g_fmts[f].n, dims[s][0], dims[s][1]);
        check_footprints(dev, &d, &g_fmts[f], 0, lb);
    }
}
TEST(d3d12_objects_footprint_mip_chains) {
    NEED_DEV();
    static const unsigned pick[] = { 0, 3, 7, 11, 15, 17, 19 };
    for (unsigned k = 0; k < sizeof pick / sizeof *pick; k++) {
        const fmtinfo *fi = &g_fmts[pick[k]];
        D3D12_RESOURCE_DESC d = d12_tex2d_desc(70, 45, fi->f, 0, 1, 0);
        char lb[64]; wsprintfA(lb, "%s chain 70x45", fi->n);
        check_footprints(dev, &d, fi, 0, lb);
        d = d12_tex2d_desc(256, 256, fi->f, 0, 1, 0);
        wsprintfA(lb, "%s chain 256x256", fi->n);
        check_footprints(dev, &d, fi, 0, lb);
    }
}
TEST(d3d12_objects_footprint_array_slices) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(32, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 4, 0);
    check_footprints(dev, &d, &g_fmts[3], 0, "RGBA8 32x16 3 mips x 4");
    d = d12_tex2d_desc(20, 20, DXGI_FORMAT_BC1_UNORM, 0, 3, 0);
    check_footprints(dev, &d, &g_fmts[15], 0, "BC1 20x20 chain x 3");
}
TEST(d3d12_objects_footprint_base_offset) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 4, 1, 0);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT a[4], b[4]; UINT rows[4]; UINT64 rs[4], ta = 0, tb = 0;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 4, 0, a, rows, rs, &ta);
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 4, 8192, b, rows, rs, &tb);
    for (int i = 0; i < 4; i++) CHECKF(b[i].Offset == a[i].Offset + 8192, "sub %d: %llu vs %llu", i, (unsigned long long)b[i].Offset, (unsigned long long)a[i].Offset);
    CHECK(ta > 0);
    if (tb != ta) t_note("total with BaseOffset 8192: %llu (without: %llu)", (unsigned long long)tb, (unsigned long long)ta);
}
TEST(d3d12_objects_footprint_exact_values_rgba8_256) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(256, 256, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1, 0);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp[9]; UINT rows[9]; UINT64 rs[9], total = 0;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 9, 0, fp, rows, rs, &total);
    static const UINT64 off[9] = { 0, 262144, 327680, 344064, 352256, 356352, 358400, 359424, 359936 };
    static const UINT pitch[9] = { 1024, 512, 256, 256, 256, 256, 256, 256, 256 };
    static const UINT64 rsz[9] = { 1024, 512, 256, 128, 64, 32, 16, 8, 4 };
    static const UINT nrows[9] = { 256, 128, 64, 32, 16, 8, 4, 2, 1 };
    for (int i = 0; i < 9; i++) {
        CHECKF(fp[i].Offset == off[i], "mip %d offset %llu want %llu", i, (unsigned long long)fp[i].Offset, (unsigned long long)off[i]);
        CHECKF(fp[i].Footprint.RowPitch == pitch[i], "mip %d pitch %u", i, fp[i].Footprint.RowPitch);
        CHECKF(rs[i] == rsz[i], "mip %d row size %llu", i, (unsigned long long)rs[i]);
        CHECKF(rows[i] == nrows[i], "mip %d rows %u", i, rows[i]);
    }
    EXPECT_EQ(total, 359936 + 4);
}
TEST(d3d12_objects_footprint_exact_values_misc) {
    NEED_DEV();
    UINT64 total; UINT rows; UINT64 rs; D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(17, 5, DXGI_FORMAT_R8_UNORM, 1, 1, 0);
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, &fp, &rows, &rs, &total);
    EXPECT_EQ(fp.Footprint.RowPitch, 256); EXPECT_EQ(rows, 5); EXPECT_EQ(rs, 17); EXPECT_EQ(total, 4 * 256 + 17); EXPECT_EQ(fp.Offset, 0);
    d = d12_tex2d_desc(16, 16, DXGI_FORMAT_BC1_UNORM, 1, 1, 0);
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, &fp, &rows, &rs, &total);
    EXPECT_EQ(fp.Footprint.RowPitch, 256); EXPECT_EQ(rows, 4); EXPECT_EQ(rs, 32); EXPECT_EQ(total, 3 * 256 + 32);
    d = d12_tex2d_desc(64, 64, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1, 0);
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, &fp, &rows, &rs, &total);
    EXPECT_EQ(fp.Footprint.RowPitch, 1024); EXPECT_EQ(rows, 64); EXPECT_EQ(rs, 1024); EXPECT_EQ(total, 65536);
    d = d12_tex2d_desc(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, &fp, &rows, &rs, &total);
    EXPECT_EQ(fp.Footprint.RowPitch, 256); EXPECT_EQ(rows, 1); EXPECT_EQ(rs, 4); EXPECT_EQ(total, 4);
    d = d12_tex2d_desc(8, 8, DXGI_FORMAT_R32G32B32_FLOAT, 1, 1, 0);
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, &fp, &rows, &rs, &total);
    EXPECT_EQ(rs, 96); EXPECT_EQ(fp.Footprint.RowPitch, 256); EXPECT_EQ(total, 7 * 256 + 96);
}
TEST(d3d12_objects_footprint_3d) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 4, 0);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    UINT64 total; UINT rows; UINT64 rs; D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, &fp, &rows, &rs, &total);
    EXPECT_EQ(fp.Footprint.Depth, 4); EXPECT_EQ(fp.Footprint.RowPitch, 256); EXPECT_EQ(rows, 8); EXPECT_EQ(rs, 32);
    EXPECT_EQ(total, 3 * 8 * 256 + 7 * 256 + 32);
    d = d12_tex2d_desc(16, 8, DXGI_FORMAT_R8_UNORM, 0, 8, 0);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    check_footprints(dev, &d, &g_fmts[0], 0, "R8 3D 16x8x8 chain");
    d = d12_tex2d_desc(8, 8, DXGI_FORMAT_BC1_UNORM, 0, 4, 0);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    check_footprints(dev, &d, &g_fmts[15], 0, "BC1 3D 8x8x4 chain");
}
TEST(d3d12_objects_footprint_1d) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(100, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 2, 0);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    check_footprints(dev, &d, &g_fmts[3], 0, "RGBA8 1D 100 chain x2");
}
TEST(d3d12_objects_footprint_buffer) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_buf_desc(1000, 0);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT rows = 0; UINT64 rs = 0, total = 0;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, &fp, &rows, &rs, &total);
    EXPECT_EQ(rows, 1); EXPECT_EQ(rs, 1000); EXPECT_EQ(total, 1000); EXPECT_EQ(fp.Offset, 0);
    EXPECT_EQ(fp.Footprint.Format, DXGI_FORMAT_UNKNOWN); EXPECT_EQ(fp.Footprint.Width, 1000); EXPECT_EQ(fp.Footprint.Height, 1); EXPECT_EQ(fp.Footprint.Depth, 1);
    t_note("buffer footprint RowPitch = %u", fp.Footprint.RowPitch);
}
TEST(d3d12_objects_footprint_optional_outputs) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(32, 32, DXGI_FORMAT_R8G8B8A8_UNORM, 2, 1, 0);
    UINT64 total = 0;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 2, 0, NULL, NULL, NULL, &total);
    CHECK(total > 0 && total != ~0ull);
    UINT rows[2]; UINT64 rs[2];
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 2, 0, NULL, rows, rs, NULL);
    EXPECT_EQ(rows[0], 32); EXPECT_EQ(rows[1], 16); EXPECT_EQ(rs[0], 128); EXPECT_EQ(rs[1], 64);
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 0, 0, NULL, NULL, NULL, &total);
    EXPECT_EQ(total, 0);
}
TEST(d3d12_objects_footprint_first_subresource) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 4, 1, 0);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT all[4], one; UINT rows[4], r1; UINT64 rs[4], rs1, t4, t1;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 4, 0, all, rows, rs, &t4);
    for (UINT s = 0; s < 4; s++) {
        ID3D12Device_GetCopyableFootprints(dev, &d, s, 1, 0, &one, &r1, &rs1, &t1);
        CHECKF(one.Offset == 0, "sub %u alone starts at offset %llu", s, (unsigned long long)one.Offset);
        CHECKF(one.Footprint.RowPitch == all[s].Footprint.RowPitch && r1 == rows[s] && rs1 == rs[s], "sub %u layout differs when queried alone", s);
        CHECKF(one.Footprint.Width == all[s].Footprint.Width && one.Footprint.Height == all[s].Footprint.Height, "sub %u extent", s);
    }
    ID3D12Device_GetCopyableFootprints(dev, &d, 2, 2, 0, all, rows, rs, &t1);
    EXPECT_EQ(all[0].Offset, 0);
    EXPECT_EQ(all[1].Offset, 4096);   /* 16x16 RGBA8: 15 * 256 + 64 bytes, rounded up to 512 */
}
TEST(d3d12_objects_footprint_invalid_desc) {
    NEED_DEV();
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);
    UINT64 total = 0xABCDEF;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, NULL, NULL, NULL, &total);
    EXPECT_EQ(total, ~0ull);
    d = d12_tex2d_desc(8, 8, DXGI_FORMAT_UNKNOWN, 1, 1, 0);
    total = 0xABCDEF;
    ID3D12Device_GetCopyableFootprints(dev, &d, 0, 1, 0, NULL, NULL, NULL, &total);
    EXPECT_EQ(total, ~0ull);
}
TEST(d3d12_objects_footprint_matches_real_resource) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    if (FAILED(d12_make_tex2d(dev, 40, 24, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 2, 0, D3D12_RESOURCE_STATE_COMMON, NULL, &r))) { t_skip("no texture"); return; }
    D3D12_RESOURCE_DESC d = d12_res_desc(r);
    EXPECT_EQ(d.MipLevels, 6);
    check_footprints(dev, &d, &g_fmts[3], 0, "resource desc RGBA8 40x24");
    SAFE_RELEASE(r);
}

/* ====== Map / Unmap ============================================================================================= */
TEST(d3d12_objects_map_upload_basic) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, 4096, 0, &r), S_OK);
    if (!r) return;
    void *p1 = NULL, *p2 = NULL; D3D12_RANGE none = { 0, 0 };
    EXPECT_HR(ID3D12Resource_Map(r, 0, &none, &p1), S_OK);
    CHECK(p1 != NULL);
    EXPECT_HR(ID3D12Resource_Map(r, 0, NULL, &p2), S_OK);   /* nested map */
    EXPECT_EQ(p1, p2);
    if (p1) { memset(p1, 0xA5, 4096); }
    ID3D12Resource_Unmap(r, 0, NULL);
    ID3D12Resource_Unmap(r, 0, NULL);
    void *p3 = NULL; D3D12_RANGE rr = { 0, 4096 };
    EXPECT_HR(ID3D12Resource_Map(r, 0, &rr, &p3), S_OK);
    if (p3) { int ok = 1; for (int i = 0; i < 4096; i++) if (((unsigned char *)p3)[i] != 0xA5) ok = 0; CHECK(ok); }
    ID3D12Resource_Unmap(r, 0, &none);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_map_null_ppdata) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, 256, 0, &r), S_OK);
    if (!r) return;
    EXPECT_HR(ID3D12Resource_Map(r, 0, NULL, NULL), S_OK);   /* documented: a NULL ppData is valid */
    ID3D12Resource_Unmap(r, 0, NULL);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_map_readback_buffer) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, 1024, 0, &r), S_OK);
    if (!r) return;
    void *p = NULL; D3D12_RANGE rr = { 0, 1024 };
    EXPECT_HR(ID3D12Resource_Map(r, 0, &rr, &p), S_OK);
    CHECK(p != NULL);
    if (p) { int z = 1; for (int i = 0; i < 1024; i++) if (((unsigned char *)p)[i]) z = 0; CHECK(z); }
    D3D12_RANGE wr = { 0, 0 };
    ID3D12Resource_Unmap(r, 0, &wr);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_map_default_heap_rejected) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, 0, &r), S_OK);
    if (!r) return;
    void *p = (void *)1;
    HRESULT hr = ID3D12Resource_Map(r, 0, NULL, &p);
    CHECKF(FAILED(hr), "Map on a DEFAULT-heap buffer: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D12Resource_Unmap(r, 0, NULL);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_map_bad_subresource) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, 256, 0, &r), S_OK);
    if (!r) return;
    void *p = (void *)1;
    HRESULT hr = ID3D12Resource_Map(r, 1, NULL, &p);
    CHECKF(FAILED(hr), "Map subresource 1 of a buffer: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D12Resource_Unmap(r, 1, NULL);
    hr = ID3D12Resource_Map(r, 0xFFFFFFFF, NULL, &p);
    CHECKF(FAILED(hr), "Map subresource -1: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D12Resource_Unmap(r, 0xFFFFFFFF, NULL);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_map_read_range_note) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, 256, 0, &r), S_OK);
    if (!r) return;
    void *p = NULL; D3D12_RANGE bad = { 100, 50 };
    HRESULT hr = ID3D12Resource_Map(r, 0, &bad, &p);
    t_note("Map with ReadRange End < Begin: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D12Resource_Unmap(r, 0, NULL);
    D3D12_RANGE big = { 0, 4096 };
    hr = ID3D12Resource_Map(r, 0, &big, &p);
    t_note("Map with ReadRange past the buffer: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) ID3D12Resource_Unmap(r, 0, NULL);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_map_pointer_not_offset_by_range) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, 4096, 0, &r), S_OK);
    if (!r) return;
    void *p0 = NULL, *p1 = NULL; D3D12_RANGE a = { 0, 0 }, b = { 512, 1024 };
    EXPECT_HR(ID3D12Resource_Map(r, 0, &a, &p0), S_OK);
    EXPECT_HR(ID3D12Resource_Map(r, 0, &b, &p1), S_OK);
    EXPECT_EQ(p0, p1);   /* documented: the pointer is never offset by pReadRange */
    ID3D12Resource_Unmap(r, 0, NULL); ID3D12Resource_Unmap(r, 0, NULL);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_map_persistent_while_gpu_copies) {
    NEED_DEV();
    d12ctx c; if (FAILED(d12_ctx_init(&c, dev, D3D12_COMMAND_LIST_TYPE_DIRECT))) { t_skip("no ctx"); d12_ctx_free(&c); return; }
    ID3D12Resource *up = NULL, *rb = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_UPLOAD, 1024, 0, &up), S_OK);
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_READBACK, 1024, 0, &rb), S_OK);
    if (up && rb) {
        unsigned char *pu = NULL, *pr = NULL; D3D12_RANGE none = { 0, 0 };
        EXPECT_HR(ID3D12Resource_Map(up, 0, &none, (void **)&pu), S_OK);   /* persistent maps */
        EXPECT_HR(ID3D12Resource_Map(rb, 0, NULL, (void **)&pr), S_OK);
        if (pu && pr) {
            for (int round = 0; round < 3; round++) {
                for (int i = 0; i < 1024; i++) pu[i] = (unsigned char)(i * 3 + round);
                ID3D12GraphicsCommandList_CopyBufferRegion(c.cl, rb, 0, up, 0, 1024);
                EXPECT_HR(d12_run(&c), S_OK);
                int ok = 1; for (int i = 0; i < 1024; i++) if (pr[i] != (unsigned char)(i * 3 + round)) ok = 0;
                CHECKF(ok, "round %d readback differs", round);
                EXPECT_HR(d12_rerecord(&c), S_OK);
            }
        }
        ID3D12Resource_Unmap(up, 0, NULL); ID3D12Resource_Unmap(rb, 0, NULL);
    }
    SAFE_RELEASE(up); SAFE_RELEASE(rb); d12_ctx_free(&c);
}
TEST(d3d12_objects_write_read_subresource_custom_heap) {
    NEED_DEV();
    D3D12_HEAP_PROPERTIES hp; memset(&hp, 0, sizeof hp);
    hp.Type = D3D12_HEAP_TYPE_CUSTOM; hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; hp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    hp.CreationNodeMask = hp.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);
    ID3D12Resource *r = NULL;
    HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
    if (FAILED(hr) || !r) { t_note("texture in a CUSTOM WRITE_BACK/L0 heap: %s", d3dh_hr_name(hr)); t_skip("custom heap texture unsupported (non-UMA)"); return; }
    UINT32 src[16], dst[16];
    for (int i = 0; i < 16; i++) src[i] = 0x01010101u * (i + 1);
    EXPECT_HR(ID3D12Resource_Map(r, 0, NULL, NULL), S_OK);
    hr = ID3D12Resource_WriteToSubresource(r, 0, NULL, src, 16, 64);
    EXPECT_HR(hr, S_OK);
    memset(dst, 0, sizeof dst);
    hr = ID3D12Resource_ReadFromSubresource(r, dst, 16, 64, 0, NULL);
    EXPECT_HR(hr, S_OK);
    CHECK(!memcmp(src, dst, sizeof src));
    /* partial box */
    D3D12_BOX box = { 1, 1, 0, 3, 3, 1 }; UINT32 patch[4] = { 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD };
    EXPECT_HR(ID3D12Resource_WriteToSubresource(r, 0, &box, patch, 8, 16), S_OK);
    memset(dst, 0, sizeof dst);
    EXPECT_HR(ID3D12Resource_ReadFromSubresource(r, dst, 16, 64, 0, NULL), S_OK);
    EXPECT_EQ(dst[1 * 4 + 1], 0xAAAAAAAA); EXPECT_EQ(dst[1 * 4 + 2], 0xBBBBBBBB);
    EXPECT_EQ(dst[2 * 4 + 1], 0xCCCCCCCC); EXPECT_EQ(dst[2 * 4 + 2], 0xDDDDDDDD);
    EXPECT_EQ(dst[0], src[0]); EXPECT_EQ(dst[3], src[3]); EXPECT_EQ(dst[15], src[15]); EXPECT_EQ(dst[1 * 4 + 3], src[1 * 4 + 3]);
    UINT32 sub[4]; memset(sub, 0, sizeof sub);
    EXPECT_HR(ID3D12Resource_ReadFromSubresource(r, sub, 8, 16, 0, &box), S_OK);
    CHECK(!memcmp(sub, patch, sizeof sub));
    ID3D12Resource_Unmap(r, 0, NULL);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_write_subresource_invalid) {
    NEED_DEV();
    ID3D12Resource *r = NULL;
    EXPECT_HR(d12_make_buffer(dev, D3D12_HEAP_TYPE_DEFAULT, 256, 0, &r), S_OK);
    if (!r) return;
    UINT32 px[4] = { 0 };
    HRESULT hr = ID3D12Resource_WriteToSubresource(r, 0, NULL, px, 16, 16);
    CHECKF(FAILED(hr), "WriteToSubresource on a DEFAULT-heap buffer: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(r);
}

/* ====== reserved resources ========================================================================================== */
TEST(d3d12_objects_reserved_buffer) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS o; memset(&o, 0, sizeof o);
    if (FAILED(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_D3D12_OPTIONS, &o, sizeof o)) || o.TiledResourcesTier == D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED) { t_skip("tiled resources not supported"); return; }
    D3D12_RESOURCE_DESC d = d12_buf_desc(1 * MB, 0);
    ID3D12Resource *r = NULL;
    EXPECT_HR(ID3D12Device_CreateReservedResource(dev, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r), S_OK);
    if (!r) return;
    CHECK(ID3D12Resource_GetGPUVirtualAddress(r) != 0);
    D3D12_HEAP_PROPERTIES hp; D3D12_HEAP_FLAGS hf;
    EXPECT_HR(ID3D12Resource_GetHeapProperties(r, &hp, &hf), E_INVALIDARG);
    UINT ntiles = 0;
    { D3D12_PACKED_MIP_INFO pm0; D3D12_TILE_SHAPE ts0; UINT ns0 = 0; D3D12_SUBRESOURCE_TILING st0; ID3D12Device_GetResourceTiling(dev, r, &ntiles, &pm0, &ts0, &ns0, 0, &st0); }
    EXPECT_EQ(ntiles, 16);
    SAFE_RELEASE(r);
}
TEST(d3d12_objects_reserved_texture_tile_shapes) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS o; memset(&o, 0, sizeof o);
    if (FAILED(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_D3D12_OPTIONS, &o, sizeof o)) || o.TiledResourcesTier == D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED) { t_skip("tiled resources not supported"); return; }
    static const struct { DXGI_FORMAT f; UINT w, h; } t[] = {
        { DXGI_FORMAT_R8_UNORM, 256, 256 }, { DXGI_FORMAT_R8G8B8A8_UNORM, 128, 128 }, { DXGI_FORMAT_R16G16B16A16_FLOAT, 128, 64 }, { DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64 },
    };
    for (unsigned i = 0; i < 4; i++) {
        D3D12_RESOURCE_DESC d = d12_tex2d_desc(512, 512, t[i].f, 1, 1, 0);
        d.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
        ID3D12Resource *r = NULL;
        HRESULT hr = ID3D12Device_CreateReservedResource(dev, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
        CHECKF(hr == S_OK, "reserved tex fmt %d: %s", t[i].f, d3dh_hr_name(hr));
        if (!r) continue;
        UINT ntiles = 0, nsub = 1; D3D12_PACKED_MIP_INFO pm; D3D12_TILE_SHAPE ts; D3D12_SUBRESOURCE_TILING st;
        memset(&pm, 0, sizeof pm); memset(&ts, 0, sizeof ts); memset(&st, 0, sizeof st);
        ID3D12Device_GetResourceTiling(dev, r, &ntiles, &pm, &ts, &nsub, 0, &st);
        CHECKF(ts.WidthInTexels == t[i].w && ts.HeightInTexels == t[i].h && ts.DepthInTexels == 1, "fmt %d tile shape %ux%ux%u", t[i].f, ts.WidthInTexels, ts.HeightInTexels, ts.DepthInTexels);
        CHECKF(ntiles == (512 / t[i].w) * (512 / t[i].h), "fmt %d tiles %u", t[i].f, ntiles);
        CHECKF(st.WidthInTiles == 512 / t[i].w && st.HeightInTiles == 512 / t[i].h && st.DepthInTiles == 1, "fmt %d sub0 tiling %ux%u", t[i].f, st.WidthInTiles, st.HeightInTiles);
        EXPECT_EQ(st.StartTileIndexInOverallResource, 0);
        SAFE_RELEASE(r);
    }
}
TEST(d3d12_objects_reserved_rejects_committed_only_features) {
    NEED_DEV();
    D3D12_FEATURE_DATA_D3D12_OPTIONS o; memset(&o, 0, sizeof o);
    if (FAILED(ID3D12Device_CheckFeatureSupport(dev, D3D12_FEATURE_D3D12_OPTIONS, &o, sizeof o)) || o.TiledResourcesTier == D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED) { t_skip("tiled resources not supported"); return; }
    D3D12_RESOURCE_DESC d = d12_tex2d_desc(256, 256, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 0);   /* UNKNOWN layout is a valid reserved layout? record */
    ID3D12Resource *r = NULL;
    HRESULT hr = ID3D12Device_CreateReservedResource(dev, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
    t_note("reserved texture with layout UNKNOWN: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
    d = d12_buf_desc(0, 0);
    hr = ID3D12Device_CreateReservedResource(dev, &d, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (void **)&r);
    CHECKF(hr == E_INVALIDARG, "reserved buffer width 0: %s", d3dh_hr_name(hr)); SAFE_RELEASE(r);
}
