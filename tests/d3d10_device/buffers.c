/* D3D10 buffers: CreateBuffer validity matrix, descriptor roundtrip, initial data, resource accessors, private data. */
#include "dd.h"

#define T(n) TEST(d3d10_device_buf_##n)
#define VB D3D10_BIND_VERTEX_BUFFER
#define IB D3D10_BIND_INDEX_BUFFER
#define CB D3D10_BIND_CONSTANT_BUFFER
#define SR D3D10_BIND_SHADER_RESOURCE
#define SO D3D10_BIND_STREAM_OUTPUT
#define RT D3D10_BIND_RENDER_TARGET
#define DEF D3D10_USAGE_DEFAULT
#define IMM D3D10_USAGE_IMMUTABLE
#define DYN D3D10_USAGE_DYNAMIC
#define STG D3D10_USAGE_STAGING
#define CR D3D10_CPU_ACCESS_READ
#define CW D3D10_CPU_ACCESS_WRITE
enum { W_OK, W_BAD, W_NOTE, W_LAX }; /* W_LAX: documented as invalid, but the Win10 WARP d3d10 device accepts it (probed) */

static void run_case(UINT bw, UINT usage, UINT bind, UINT cpu, UINT misc, int want)
{
    D3D10_BUFFER_DESC d = { bw, (D3D10_USAGE)usage, bind, cpu, misc }, g;
    D3D10_SUBRESOURCE_DATA sd;
    unsigned char *mem = calloc(1, bw ? bw : 1);
    ID3D10Buffer *b = NULL;
    HRESULT hrv, hrc;
    DD_DEV(dev);
    int lax = (want == W_LAX);
    if (lax) want = dd_warp() ? W_OK : W_BAD;
    sd.pSysMem = mem; sd.SysMemPitch = sd.SysMemSlicePitch = 0;
    hrv = ID3D10Device_CreateBuffer(dev, &d, usage == IMM ? &sd : NULL, NULL);
    hrc = ID3D10Device_CreateBuffer(dev, &d, usage == IMM ? &sd : NULL, &b);
    if (want == W_NOTE) {
        t_note("bw=%u usage=%u bind=0x%x cpu=0x%x misc=0x%x: validate=%s create=%s", bw, usage, bind, cpu, misc, d3dh_hr_name(hrv), d3dh_hr_name(hrc));
    } else if (want == W_OK) {
        EXPECT_HR(hrv, S_FALSE);
        EXPECT_HR(hrc, S_OK);
        CHECK(b != NULL);
        if (b) {
            memset(&g, 0xcc, sizeof g);
            ID3D10Buffer_GetDesc(b, &g);
            EXPECT_EQ(g.ByteWidth, bw); EXPECT_EQ(g.Usage, usage); EXPECT_EQ(g.BindFlags, bind);
            EXPECT_EQ(g.CPUAccessFlags, cpu); EXPECT_EQ(g.MiscFlags, lax ? 0 : misc); /* an accepted-but-meaningless TEXTURECUBE flag is dropped (probed) */
        }
    } else {
        EXPECT_HR(hrv, E_INVALIDARG);
        EXPECT_HR(hrc, E_INVALIDARG);
        CHECK(b == NULL);
    }
    SAFE_RELEASE(b);
    free(mem);
    DD_END(dev);
}
#define CASE(nm, bw, usage, bind, cpu, misc, want) T(nm) { run_case(bw, usage, bind, cpu, misc, want); }

CASE(def_vb_16, 16, DEF, VB, 0, 0, W_OK)
CASE(def_ib_16, 16, DEF, IB, 0, 0, W_OK)
CASE(def_cb_16, 16, DEF, CB, 0, 0, W_OK)
CASE(def_cb_256, 256, DEF, CB, 0, 0, W_OK)
CASE(def_cb_4096, 4096 * 16, DEF, CB, 0, 0, W_OK)
CASE(def_srv_16, 16, DEF, SR, 0, 0, W_OK)
CASE(def_so_16, 16, DEF, SO, 0, 0, W_OK)
CASE(def_rt_16, 16, DEF, RT, 0, 0, W_OK)
CASE(def_vb_bw1, 1, DEF, VB, 0, 0, W_OK)
CASE(def_vb_bw3, 3, DEF, VB, 0, 0, W_OK)
CASE(def_ib_bw7, 7, DEF, IB, 0, 0, W_OK)
CASE(def_srv_bw1, 1, DEF, SR, 0, 0, W_OK)
CASE(def_vb_1mb, 1 << 20, DEF, VB, 0, 0, W_OK)
CASE(def_vb_16mb, 16 << 20, DEF, VB, 0, 0, W_OK)
CASE(def_vb_ib, 64, DEF, VB | IB, 0, 0, W_OK)
CASE(def_vb_srv, 64, DEF, VB | SR, 0, 0, W_OK)
CASE(def_ib_srv, 64, DEF, IB | SR, 0, 0, W_OK)
CASE(def_vb_so, 64, DEF, VB | SO, 0, 0, W_OK)
CASE(def_srv_so, 64, DEF, SR | SO, 0, 0, W_OK)
CASE(def_vb_ib_srv_so, 64, DEF, VB | IB | SR | SO, 0, 0, W_OK)
CASE(def_bind0, 64, DEF, 0, 0, 0, W_OK)
CASE(dyn_vb, 64, DYN, VB, CW, 0, W_OK)
CASE(dyn_ib, 64, DYN, IB, CW, 0, W_OK)
CASE(dyn_cb, 64, DYN, CB, CW, 0, W_OK)
CASE(dyn_srv, 64, DYN, SR, CW, 0, W_OK)
CASE(dyn_vb_ib_srv, 64, DYN, VB | IB | SR, CW, 0, W_OK)
CASE(dyn_vb_bw1, 1, DYN, VB, CW, 0, W_OK)
CASE(imm_vb, 64, IMM, VB, 0, 0, W_OK)
CASE(imm_ib, 64, IMM, IB, 0, 0, W_OK)
CASE(imm_cb, 64, IMM, CB, 0, 0, W_OK)
CASE(imm_srv, 64, IMM, SR, 0, 0, W_OK)
CASE(imm_vb_bw1, 1, IMM, VB, 0, 0, W_OK)
CASE(imm_vb_ib_srv, 64, IMM, VB | IB | SR, 0, 0, W_OK)
CASE(stg_read, 64, STG, 0, CR, 0, W_OK)
CASE(stg_write, 64, STG, 0, CW, 0, W_OK)
CASE(stg_readwrite, 64, STG, 0, CR | CW, 0, W_OK)
CASE(stg_read_bw1, 1, STG, 0, CR, 0, W_OK)
CASE(stg_read_bw13, 13, STG, 0, CR, 0, W_OK)
CASE(stg_read_16mb, 16 << 20, STG, 0, CR, 0, W_OK)
/* ByteWidth */
CASE(bw0_vb, 0, DEF, VB, 0, 0, W_BAD)
CASE(bw0_cb, 0, DEF, CB, 0, 0, W_BAD)
CASE(bw0_dyn, 0, DYN, VB, CW, 0, W_BAD)
CASE(bw0_stg, 0, STG, 0, CR, 0, W_BAD)
CASE(bw0_srv, 0, DEF, SR, 0, 0, W_BAD)
CASE(cb_bw1, 1, DEF, CB, 0, 0, W_BAD)
CASE(cb_bw4, 4, DEF, CB, 0, 0, W_BAD)
CASE(cb_bw15, 15, DEF, CB, 0, 0, W_BAD)
CASE(cb_bw17, 17, DEF, CB, 0, 0, W_BAD)
CASE(cb_bw20, 20, DEF, CB, 0, 0, W_BAD)
CASE(cb_bw31, 31, DEF, CB, 0, 0, W_BAD)
CASE(cb_bw100, 100, DEF, CB, 0, 0, W_BAD)
CASE(cb_dyn_bw20, 20, DYN, CB, CW, 0, W_BAD)
CASE(cb_imm_bw20, 20, IMM, CB, 0, 0, W_BAD)
CASE(srv_bw_not_mult4, 6, DEF, SR, 0, 0, W_OK)
/* constant buffer exclusivity */
CASE(cb_vb, 64, DEF, CB | VB, 0, 0, W_BAD)
CASE(cb_ib, 64, DEF, CB | IB, 0, 0, W_BAD)
CASE(cb_srv, 64, DEF, CB | SR, 0, 0, W_BAD)
CASE(cb_so, 64, DEF, CB | SO, 0, 0, W_BAD)
CASE(cb_rt, 64, DEF, CB | RT, 0, 0, W_BAD)
CASE(cb_vb_ib, 64, DEF, CB | VB | IB, 0, 0, W_BAD)
CASE(cb_dyn_srv, 64, DYN, CB | SR, CW, 0, W_BAD)
/* usage rules */
CASE(imm_cpu_write, 64, IMM, VB, CW, 0, W_BAD)
CASE(imm_cpu_read, 64, IMM, VB, CR, 0, W_BAD)
CASE(def_cpu_write, 64, DEF, VB, CW, 0, W_BAD)
CASE(def_cpu_read, 64, DEF, VB, CR, 0, W_BAD)
CASE(def_cpu_rw, 64, DEF, VB, CR | CW, 0, W_BAD)
CASE(dyn_cpu0, 64, DYN, VB, 0, 0, W_BAD)
CASE(dyn_cpu_read, 64, DYN, VB, CR, 0, W_BAD)
CASE(dyn_cpu_rw, 64, DYN, VB, CR | CW, 0, W_BAD)
CASE(dyn_so, 64, DYN, SO, CW, 0, W_BAD)
CASE(dyn_rt, 64, DYN, RT, CW, 0, W_BAD)
CASE(stg_cpu0, 64, STG, 0, 0, 0, W_BAD)
CASE(stg_vb, 64, STG, VB, CR, 0, W_BAD)
CASE(stg_ib, 64, STG, IB, CR, 0, W_BAD)
CASE(stg_cb, 64, STG, CB, CR, 0, W_BAD)
CASE(stg_srv, 64, STG, SR, CR, 0, W_BAD)
CASE(stg_so, 64, STG, SO, CR, 0, W_BAD)
CASE(stg_rt, 64, STG, RT, CR, 0, W_BAD)
/* undefined values */
CASE(usage_5, 64, 5, VB, 0, 0, W_BAD)
CASE(usage_ffff, 64, 0xffff, VB, 0, 0, W_BAD)
CASE(bind_undefined_bit, 64, DEF, 0x80000000u, 0, 0, W_BAD)
CASE(bind_undefined_with_vb, 64, DEF, VB | 0x80000000u, 0, 0, W_BAD)
CASE(cpu_undefined_bit, 64, STG, 0, 0x40000, 0, W_BAD)
CASE(cpu_undefined_with_read, 64, STG, 0, CR | 0x40000, 0, W_BAD)
CASE(misc_undefined_bit, 64, DEF, VB, 0, 0x80000000u, W_BAD)
/* misc flags */
CASE(misc_generate_mips, 64, DEF, SR | RT, 0, D3D10_RESOURCE_MISC_GENERATE_MIPS, W_BAD)
CASE(misc_texturecube, 64, DEF, SR, 0, D3D10_RESOURCE_MISC_TEXTURECUBE, W_LAX)
CASE(misc_gdi_compatible, 64, DEF, SR, 0, D3D10_RESOURCE_MISC_GDI_COMPATIBLE, W_BAD)
CASE(misc_shared, 64, DEF, SR, 0, D3D10_RESOURCE_MISC_SHARED, W_OK)
CASE(misc_shared_keyed, 64, DEF, SR, 0, D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX, W_BAD)
CASE(misc_raw_views_not_d3d10, 1024, DEF, SR, 0, 0x20, W_BAD)
CASE(bind_depth_stencil, 64, DEF, D3D10_BIND_DEPTH_STENCIL, 0, 0, W_BAD)
CASE(bind_depth_stencil_srv, 64, DEF, D3D10_BIND_DEPTH_STENCIL | SR, 0, 0, W_BAD)
CASE(bind_rt_srv, 64, DEF, RT | SR, 0, 0, W_LAX)

/* ---- API-level rules ---- */
T(null_desc)
{
    DD_DEV(dev);
    ID3D10Buffer *b = (ID3D10Buffer *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, NULL, NULL, &b), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, NULL, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(validate_only_s_false)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, &d, NULL, NULL), S_FALSE);
    d.ByteWidth = 0;
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, &d, NULL, NULL), E_INVALIDARG);
    DD_END(dev);
}
T(immutable_needs_data)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, IMM, VB, 0, 0 };
    BYTE m[64] = { 0 };
    D3D10_SUBRESOURCE_DATA sd = { m, 0, 0 };
    ID3D10Buffer *b = (ID3D10Buffer *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, &d, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, &d, NULL, &b), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, &d, &sd, NULL), S_FALSE);
    DD_END(dev);
}
T(initial_data_null_sysmem)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd = { NULL, 0, 0 };
    ID3D10Buffer *b = (ID3D10Buffer *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10Device_CreateBuffer(dev, &d, &sd, &b), E_INVALIDARG);
    DD_END(dev);
}
T(initial_data_default_usage_roundtrip)
{
    DD_DEV_R(dev);
    BYTE m[64]; for (int i = 0; i < 64; i++) m[i] = (BYTE)(i * 3 + 1);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd = { m, 0, 0 };
    ID3D10Buffer *b = NULL; void *out = NULL; size_t sz = 0;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, &sd, &b));
    if (b) {
        EXPECT_HR_OK(d3dh_readback_buffer10(dev, b, &out, &sz));
        if (out) { EXPECT_EQ(sz, 64); CHECK(!memcmp(out, m, 64)); free(out); }
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(initial_data_immutable_roundtrip)
{
    DD_DEV_R(dev);
    UINT m[32]; for (int i = 0; i < 32; i++) m[i] = 0x01010101u * (UINT)i;
    D3D10_BUFFER_DESC d = { 128, IMM, SR, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd = { m, 0, 0 };
    ID3D10Buffer *b = NULL; void *out = NULL; size_t sz = 0;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, &sd, &b));
    if (b) {
        /* immutable buffers cannot be copied *from* only if the destination is invalid; copy to a staging buffer is legal */
        if (SUCCEEDED(d3dh_readback_buffer10(dev, b, &out, &sz))) { EXPECT_EQ(sz, 128); CHECK(!memcmp(out, m, 128)); free(out); }
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(sysmem_pitch_ignored)
{
    DD_DEV(dev);
    BYTE m[64] = { 0 };
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    D3D10_SUBRESOURCE_DATA sd = { m, 12345, 6789 };
    ID3D10Buffer *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, &sd, &b));
    SAFE_RELEASE(b);
    DD_END(dev);
}
T(constant_buffer_size_sweep)
{
    DD_DEV(dev);
    for (UINT i = 0; i <= 64; i++) {
        D3D10_BUFFER_DESC d = { i, DEF, CB, 0, 0 };
        ID3D10Buffer *b = NULL;
        HRESULT want = (!i || i % 16) ? E_INVALIDARG : S_OK;
        HRESULT hr = ID3D10Device_CreateBuffer(dev, &d, NULL, &b);
        t_check(hr == want, __FILE__, __LINE__, "cb size", "size %u: %s want %s", i, d3dh_hr_name(hr), d3dh_hr_name(want));
        SAFE_RELEASE(b);
    }
    DD_END(dev);
}
T(failure_clears_output)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 0, DEF, VB, 0, 0 };
    ID3D10Buffer *b = (ID3D10Buffer *)(ULONG_PTR)0xdeadbeef;
    HRESULT hr = ID3D10Device_CreateBuffer(dev, &d, NULL, &b);
    EXPECT_HR(hr, E_INVALIDARG);
    if (b) t_note("output after failed CreateBuffer: %p", b);
    DD_END(dev);
}
T(sampled_copy_of_desc)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 256, DEF, VB | IB, 0, 0 }, g;
    ID3D10Buffer *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    if (b) {
        memset(&g, 0, sizeof g);
        ID3D10Buffer_GetDesc(b, &g);
        CHECK(!memcmp(&g, &d, sizeof d));
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}

/* ---- resource accessors ---- */
T(get_type)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    ID3D10Buffer *b = NULL; D3D10_RESOURCE_DIMENSION dim = 0xff;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    if (b) { ID3D10Buffer_GetType(b, &dim); EXPECT_EQ(dim, D3D10_RESOURCE_DIMENSION_BUFFER); ID3D10Buffer_Release(b); }
    EXPECT_EQ(D3D10_RESOURCE_DIMENSION_UNKNOWN, 0);
    EXPECT_EQ(D3D10_RESOURCE_DIMENSION_BUFFER, 1);
    EXPECT_EQ(D3D10_RESOURCE_DIMENSION_TEXTURE1D, 2);
    EXPECT_EQ(D3D10_RESOURCE_DIMENSION_TEXTURE2D, 3);
    EXPECT_EQ(D3D10_RESOURCE_DIMENSION_TEXTURE3D, 4);
    DD_END(dev);
}
T(get_device)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    ID3D10Buffer *b = NULL; ID3D10Device *got = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    if (b) {
        ULONG rc = d3dh_refcount(dev);
        ID3D10Buffer_GetDevice(b, &got);
        CHECK(got == dev);
        EXPECT_EQ(d3dh_refcount(dev), rc + 1);
        SAFE_RELEASE(got);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(eviction_priority_default)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    ID3D10Buffer *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    if (b) {
        UINT p = ID3D10Buffer_GetEvictionPriority(b);
        t_note("default eviction priority 0x%x", p);
        EXPECT_EQ(p, DXGI_RESOURCE_PRIORITY_NORMAL);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(eviction_priority_roundtrip)
{
    static const UINT pr[] = { DXGI_RESOURCE_PRIORITY_MINIMUM, DXGI_RESOURCE_PRIORITY_LOW, DXGI_RESOURCE_PRIORITY_NORMAL,
                               DXGI_RESOURCE_PRIORITY_HIGH, DXGI_RESOURCE_PRIORITY_MAXIMUM };
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    ID3D10Buffer *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    if (b) {
        for (int i = 0; i < 5; i++) {
            ID3D10Buffer_SetEvictionPriority(b, pr[i]);
            UINT g = ID3D10Buffer_GetEvictionPriority(b);
            t_check(g == pr[i], __FILE__, __LINE__, "priority", "set 0x%x got 0x%x", pr[i], g);
        }
        ID3D10Buffer_SetEvictionPriority(b, 0x12345678);
        t_note("Set(0x12345678) reads back 0x%x", ID3D10Buffer_GetEvictionPriority(b));
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(qi_resource_interfaces)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    ID3D10Buffer *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    if (b) {
        IUnknown *u = (IUnknown *)b; void *p = NULL;
        static const IID *yes[] = { &IID_IUnknown, &IID_ID3D10Buffer, &IID_ID3D10Resource, &IID_ID3D10DeviceChild, &IID_IDXGIObject };
        for (unsigned i = 0; i < sizeof yes / sizeof *yes; i++) {
            /* quirk (probed): a Win10 d3d10 buffer also answers IDXGISurface/IDXGISurface1/IDXGIResource/IDXGIDeviceSubObject */
        static const IID *quirk[] = { &IID_IDXGISurface, &IID_IDXGISurface1, &IID_IDXGIResource, &IID_IDXGIDeviceSubObject };
        for (unsigned i = 0; i < sizeof quirk / sizeof *quirk; i++) {
            p = NULL; HRESULT hr = IUnknown_QueryInterface(u, quirk[i], &p);
            t_check(hr == S_OK, __FILE__, __LINE__, "buffer QI quirk", "%s: %s", d3dh_guid_str(quirk[i]), d3dh_hr_name(hr));
            if (p) IUnknown_Release((IUnknown *)p);
        }
        p = NULL; HRESULT hr = IUnknown_QueryInterface(u, yes[i], &p);
            t_check(hr == S_OK, __FILE__, __LINE__, "buffer QI", "%s: %s", d3dh_guid_str(yes[i]), d3dh_hr_name(hr));
            if (p) IUnknown_Release((IUnknown *)p);
        }
        static const IID *no[] = { &IID_ID3D10Texture2D, &IID_ID3D10Texture1D, &IID_ID3D10Texture3D, &IID_ID3D10Device };
        for (unsigned i = 0; i < sizeof no / sizeof *no; i++) {
            p = (void *)1; HRESULT hr = IUnknown_QueryInterface(u, no[i], &p);
            t_check(hr == E_NOINTERFACE && !p, __FILE__, __LINE__, "buffer QI neg", "%s: %s", d3dh_guid_str(no[i]), d3dh_hr_name(hr));
            if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
        }
        p = NULL;
        HRESULT hr = IUnknown_QueryInterface(u, &IID_ID3D11Buffer, &p);
        EXPECT_HR(hr, S_OK); /* Win10: probed */
        t_note("buffer QI ID3D11Buffer: %s", d3dh_hr_name(hr));
        if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(buffer_keeps_device_alive_ordering)
{
    ID3D10Device *dev = NULL;
    REQUIRE(DD_C10());
    if (FAILED(dd_make(0, &dev))) { t_skip("no device"); return; }
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 };
    ID3D10Buffer *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    ID3D10Device_Release(dev); /* the buffer holds a device reference */
    if (b) {
        ID3D10Device *got = NULL;
        ID3D10Buffer_GetDevice(b, &got);
        CHECK(got != NULL);
        SAFE_RELEASE(got);
        EXPECT_EQ(ID3D10Buffer_Release(b), 0);
    }
}

/* ---- private data (same semantics as D3D11) ---- */
static const GUID PD_G1 = {0xfdb37466, 0x428f, 0x4edf, {0xa3, 0x7f, 0x9b, 0x1d, 0xf4, 0x88, 0xc5, 0xfc}};
static const GUID PD_G2 = {0x2e5afac2, 0x87b5, 0x4c10, {0x9b, 0x4b, 0x89, 0xd7, 0xd1, 0x12, 0xe7, 0x2b}};

T(pd_clear_codes_device)
{
    DD_DEV(dev);
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G1, 0, NULL), S_FALSE);
    EXPECT_HR(ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, NULL), S_OK);
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G1, ~0u, NULL), S_OK);
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G1, ~0u, NULL), S_FALSE);
    DD_END(dev);
}
T(pd_null_interface_stored_as_pointer)
{
    DD_DEV(dev);
    IUnknown *ptr = (IUnknown *)(ULONG_PTR)0xdeadbeef; UINT size = sizeof(ptr) * 2;
    EXPECT_HR(ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, NULL), S_OK);
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G1, &size, &ptr), S_OK);
    CHECK(ptr == NULL);
    EXPECT_EQ(size, sizeof(IUnknown *));
    DD_END(dev);
}
T(pd_dxgi_device_shares_store)
{
    DD_DEV(dev);
    IDXGIDevice *dx = NULL; IUnknown *ptr = (IUnknown *)(ULONG_PTR)0xdeadbeef; UINT size = sizeof(ptr) * 2;
    EXPECT_HR(ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, NULL), S_OK);
    EXPECT_HR_OK(ID3D10Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dx));
    if (dx) {
        EXPECT_HR(IDXGIDevice_GetPrivateData(dx, &PD_G1, &size, &ptr), S_OK);
        CHECK(ptr == NULL);
        EXPECT_EQ(size, sizeof(IUnknown *));
        IDXGIDevice_Release(dx);
    }
    DD_END(dev);
}
T(pd_interface_refcounting)
{
    ID3D10Device *obj = NULL; UINT size;
    DD_DEV(dev);
    if (FAILED(dd_make(0, &obj))) { DD_END(dev); t_skip("second device"); return; }
    ULONG rc = d3dh_refcount(obj), expected = rc + 1;
    EXPECT_HR(ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, (IUnknown *)obj), S_OK);
    EXPECT_EQ(d3dh_refcount(obj), expected);
    EXPECT_HR(ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, (IUnknown *)obj), S_OK);
    EXPECT_EQ(d3dh_refcount(obj), expected);
    EXPECT_HR(ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, NULL), S_OK);
    expected--;
    EXPECT_EQ(d3dh_refcount(obj), expected);
    /* interface -> plain data replaces and releases */
    EXPECT_HR(ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, (IUnknown *)obj), S_OK);
    static const DWORD data[] = { 1, 2, 3, 4 };
    size = sizeof data;
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G1, size, data), S_OK);
    EXPECT_EQ(d3dh_refcount(obj), expected);
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G1, 42, NULL), S_OK);
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G1, 42, NULL), S_FALSE);
    /* GetPrivateData on an interface entry AddRefs */
    EXPECT_HR(ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, (IUnknown *)obj), S_OK);
    IUnknown *ptr = NULL;
    size = 2 * sizeof(ptr);
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G1, &size, &ptr), S_OK);
    EXPECT_EQ(size, sizeof(IUnknown *));
    CHECK(ptr == (IUnknown *)obj);
    EXPECT_EQ(d3dh_refcount(obj), expected + 2);
    IUnknown_Release(ptr);
    ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, NULL);
    ID3D10Device_Release(obj);
    DD_END(dev);
}
T(pd_get_size_protocol)
{
    ID3D10Device *obj = NULL;
    DD_DEV(dev);
    if (FAILED(dd_make(0, &obj))) { DD_END(dev); t_skip("second device"); return; }
    ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, (IUnknown *)obj);
    UINT size = 1;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G1, &size, NULL), S_OK);
    EXPECT_EQ(size, sizeof(void *));
    size = 2 * sizeof(void *);
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G1, &size, NULL), S_OK);
    EXPECT_EQ(size, sizeof(void *));
    IUnknown *ptr = (IUnknown *)(ULONG_PTR)0xdeadbeef;
    size = 1;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G1, &size, &ptr), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(size, sizeof(void *));
    CHECK(ptr == (IUnknown *)(ULONG_PTR)0xdeadbeef);
    ID3D10Device_SetPrivateDataInterface(dev, &PD_G1, NULL);
    ID3D10Device_Release(obj);
    DD_END(dev);
}
T(pd_get_not_found)
{
    DD_DEV(dev);
    IUnknown *ptr = (IUnknown *)(ULONG_PTR)0xdeadbeef; UINT size = 0xdeadbabe;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G2, &size, &ptr), DXGI_ERROR_NOT_FOUND);
    EXPECT_EQ(size, 0);
    CHECK(ptr == (IUnknown *)(ULONG_PTR)0xdeadbeef);
    DD_END(dev);
}
T(pd_get_null_size_pointer)
{
    DD_DEV(dev);
    IUnknown *ptr = (IUnknown *)(ULONG_PTR)0xdeadbeef;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G2, NULL, NULL), E_INVALIDARG);
    ID3D10Device_SetPrivateData(dev, &PD_G1, 4, &ptr);
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G1, NULL, &ptr), E_INVALIDARG);
    CHECK(ptr == (IUnknown *)(ULONG_PTR)0xdeadbeef);
    ID3D10Device_SetPrivateData(dev, &PD_G1, 0, NULL);
    DD_END(dev);
}
T(pd_plain_data_roundtrip)
{
    DD_DEV(dev);
    DWORD in[5] = { 0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555 }, out[8]; UINT size;
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G2, sizeof in, in), S_OK);
    memset(out, 0xaa, sizeof out); size = sizeof out;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G2, &size, out), S_OK);
    EXPECT_EQ(size, sizeof in);
    CHECK(!memcmp(in, out, sizeof in));
    CHECK(out[5] == 0xaaaaaaaa);
    size = 3;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G2, &size, out), DXGI_ERROR_MORE_DATA);
    EXPECT_EQ(size, sizeof in);
    size = sizeof in;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G2, &size, out), S_OK);
    ID3D10Device_SetPrivateData(dev, &PD_G2, 0, NULL);
    DD_END(dev);
}
T(pd_overwrite_keeps_latest)
{
    DD_DEV(dev);
    DWORD a = 0xaaaa, b[2] = { 1, 2 }, out[2]; UINT size = sizeof out;
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G2, sizeof a, &a), S_OK);
    EXPECT_HR(ID3D10Device_SetPrivateData(dev, &PD_G2, sizeof b, b), S_OK);
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G2, &size, out), S_OK);
    EXPECT_EQ(size, sizeof b);
    CHECK(!memcmp(out, b, sizeof b));
    ID3D10Device_SetPrivateData(dev, &PD_G2, 0, NULL);
    DD_END(dev);
}
T(pd_two_guids_independent)
{
    DD_DEV(dev);
    DWORD a = 1, b = 2, o; UINT s = sizeof o;
    ID3D10Device_SetPrivateData(dev, &PD_G1, sizeof a, &a);
    ID3D10Device_SetPrivateData(dev, &PD_G2, sizeof b, &b);
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G1, &s, &o), S_OK); EXPECT_EQ(o, 1);
    s = sizeof o;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G2, &s, &o), S_OK); EXPECT_EQ(o, 2);
    ID3D10Device_SetPrivateData(dev, &PD_G1, 0, NULL);
    s = sizeof o;
    EXPECT_HR(ID3D10Device_GetPrivateData(dev, &PD_G2, &s, &o), S_OK);
    ID3D10Device_SetPrivateData(dev, &PD_G2, 0, NULL);
    DD_END(dev);
}
T(pd_buffer_clear_codes)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 }; ID3D10Buffer *b = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    if (b) {
        DWORD v = 7, o = 0; UINT s = sizeof o;
        EXPECT_HR(ID3D10Buffer_SetPrivateData(b, &PD_G1, 0, NULL), S_FALSE);
        EXPECT_HR(ID3D10Buffer_SetPrivateData(b, &PD_G1, sizeof v, &v), S_OK);
        EXPECT_HR(ID3D10Buffer_GetPrivateData(b, &PD_G1, &s, &o), S_OK);
        EXPECT_EQ(o, 7);
        EXPECT_HR(ID3D10Buffer_SetPrivateData(b, &PD_G1, sizeof v, NULL), S_OK);
        EXPECT_HR(ID3D10Buffer_SetPrivateData(b, &PD_G1, sizeof v, NULL), S_FALSE);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
T(pd_destroy_releases_interface)
{
    ID3D10Device *obj = NULL;
    DD_DEV(dev);
    if (FAILED(dd_make(0, &obj))) { DD_END(dev); t_skip("second device"); return; }
    D3D10_BUFFER_DESC d = { 64, DEF, VB, 0, 0 }; ID3D10Buffer *b = NULL;
    ULONG rc = d3dh_refcount(obj);
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &d, NULL, &b));
    if (b) {
        EXPECT_HR(ID3D10Buffer_SetPrivateDataInterface(b, &PD_G1, (IUnknown *)obj), S_OK);
        EXPECT_EQ(d3dh_refcount(obj), rc + 1);
        ID3D10Buffer_Release(b);
        EXPECT_EQ(d3dh_refcount(obj), rc);
    }
    ID3D10Device_Release(obj);
    DD_END(dev);
}
T(pd_texture_shares_with_dxgi_surface)
{
    DD_DEV(dev);
    D3D10_TEXTURE2D_DESC td = { 16, 16, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, DEF, RT, 0, 0 };
    ID3D10Texture2D *t = NULL; IDXGISurface *sf = NULL; DWORD v = 0x1234, o = 0; UINT s = sizeof o;
    EXPECT_HR_OK(ID3D10Device_CreateTexture2D(dev, &td, NULL, &t));
    if (t) {
        EXPECT_HR_OK(ID3D10Texture2D_QueryInterface(t, &IID_IDXGISurface, (void **)&sf));
        EXPECT_HR(ID3D10Texture2D_SetPrivateData(t, &PD_G1, sizeof v, &v), S_OK);
        if (sf) {
            EXPECT_HR(IDXGISurface_GetPrivateData(sf, &PD_G1, &s, &o), S_OK);
            EXPECT_EQ(o, 0x1234);
            IDXGISurface_Release(sf);
        }
        ID3D10Texture2D_Release(t);
    }
    DD_END(dev);
}
