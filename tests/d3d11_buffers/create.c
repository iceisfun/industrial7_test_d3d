/* CreateBuffer validity matrix and descriptor roundtrip. */
#include "bufh.h"

#define VB D3D11_BIND_VERTEX_BUFFER
#define IB D3D11_BIND_INDEX_BUFFER
#define CB D3D11_BIND_CONSTANT_BUFFER
#define SR D3D11_BIND_SHADER_RESOURCE
#define SO D3D11_BIND_STREAM_OUTPUT
#define RT D3D11_BIND_RENDER_TARGET
#define UA D3D11_BIND_UNORDERED_ACCESS
#define DEF D3D11_USAGE_DEFAULT
#define IMM D3D11_USAGE_IMMUTABLE
#define DYN D3D11_USAGE_DYNAMIC
#define STG D3D11_USAGE_STAGING
#define CR D3D11_CPU_ACCESS_READ
#define CW D3D11_CPU_ACCESS_WRITE
#define RAW D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS
#define STRUCT D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
#define DIA D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS

enum { W_OK, W_BAD, W_NOTE, W_MAPDEF };

/* MapOnDefaultBuffers (D3D11_FEATURE_D3D11_OPTIONS1): DEFAULT buffers may carry CPU access flags, restricted to SRV/UAV binds. */
static int map_on_default(void)
{
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 o;
    memset(&o, 0, sizeof o);
    if (FAILED(ID3D11Device_CheckFeatureSupport(g_dev, D3D11_FEATURE_D3D11_OPTIONS1, &o, sizeof o))) return 0;
    return o.MapOnDefaultBuffers != 0;
}

static void run_case(UINT bw, D3D11_USAGE usage, UINT bind, UINT cpu, UINT misc, UINT stride, int want, int needs11)
{
    D3D11_BUFFER_DESC d = bufh_desc(bw, usage, bind, cpu, misc, stride), g;
    D3D11_SUBRESOURCE_DATA sd;
    unsigned char *mem;
    ID3D11Buffer *b = NULL;
    HRESULT hrv, hrc;
    int hard = want != W_NOTE;
    NEED_DEV();
    if (want == W_MAPDEF) want = map_on_default() ? W_OK : W_BAD;
    hard = want != W_NOTE;
    if (needs11 && g_fl < D3D_FEATURE_LEVEL_11_0) hard = 0;
    mem = calloc(1, bw ? bw : 1);
    memset(&sd, 0, sizeof sd);
    sd.pSysMem = mem;
    sd.SysMemPitch = sd.SysMemSlicePitch = 0;
    /* validate-only: NULL output pointer, S_FALSE means "valid" */
    hrv = ID3D11Device_CreateBuffer(g_dev, &d, usage == IMM ? &sd : NULL, NULL);
    hrc = ID3D11Device_CreateBuffer(g_dev, &d, usage == IMM ? &sd : NULL, &b);
    if (!hard) {
        t_note("bw=%u usage=%d bind=0x%x cpu=0x%x misc=0x%x stride=%u: validate=%s create=%s", bw, usage, bind, cpu, misc, stride, bufh_hrn(hrv), bufh_hrn(hrc));
    } else if (want == W_OK) {
        EXPECT_HR(hrv, S_FALSE);
        EXPECT_HR(hrc, S_OK);
        CHECK(b != NULL);
        if (b) {
            ID3D11Buffer_GetDesc(b, &g);
            EXPECT_EQ(g.ByteWidth, bw);
            EXPECT_EQ(g.Usage, usage);
            EXPECT_EQ(g.BindFlags, bind);
            EXPECT_EQ(g.CPUAccessFlags, cpu);
            EXPECT_EQ(g.MiscFlags, misc);
            if (misc & STRUCT) EXPECT_EQ(g.StructureByteStride, stride);
        }
    } else {
        EXPECT_HR(hrv, E_INVALIDARG);
        EXPECT_HR(hrc, E_INVALIDARG);
        CHECK(b == NULL);
    }
    if (!hard && SUCCEEDED(hrc) && b) {
        /* tolerated either way; still check the object */
        D3D11_BUFFER_DESC g2; ID3D11Buffer_GetDesc(b, &g2); EXPECT_EQ(g2.ByteWidth, bw);
    }
    SAFE_RELEASE(b);
    free(mem);
}

#define CASE(nm, bw, usage, bind, cpu, misc, stride, want, n11) \
    TEST(d3d11_buffers_create_##nm) { REQUIRE(d3dh_D3D11CreateDevice()); run_case(bw, usage, bind, cpu, misc, stride, want, n11); }

/* ---- valid combinations ---- */
CASE(def_vb_16, 16, DEF, VB, 0, 0, 0, W_OK, 0)
CASE(def_ib_16, 16, DEF, IB, 0, 0, 0, W_OK, 0)
CASE(def_cb_16, 16, DEF, CB, 0, 0, 0, W_OK, 0)
CASE(def_cb_256, 256, DEF, CB, 0, 0, 0, W_OK, 0)
CASE(def_cb_65536, 65536, DEF, CB, 0, 0, 0, W_OK, 0)
CASE(def_srv_16, 16, DEF, SR, 0, 0, 0, W_OK, 0)
CASE(def_so_16, 16, DEF, SO, 0, 0, 0, W_OK, 0)
CASE(def_uav_16, 16, DEF, UA, 0, 0, 0, W_OK, 1)
CASE(def_vb_bw1, 1, DEF, VB, 0, 0, 0, W_OK, 0)
CASE(def_vb_bw3, 3, DEF, VB, 0, 0, 0, W_OK, 0)
CASE(def_ib_bw7, 7, DEF, IB, 0, 0, 0, W_OK, 0)
CASE(def_srv_bw1, 1, DEF, SR, 0, 0, 0, W_OK, 0)
CASE(def_vb_1mb, 1 << 20, DEF, VB, 0, 0, 0, W_OK, 0)
CASE(def_vb_16mb, 16 << 20, DEF, VB, 0, 0, 0, W_OK, 0)
CASE(def_vb_ib, 64, DEF, VB | IB, 0, 0, 0, W_OK, 0)
CASE(def_vb_srv, 64, DEF, VB | SR, 0, 0, 0, W_OK, 0)
CASE(def_ib_srv, 64, DEF, IB | SR, 0, 0, 0, W_OK, 0)
CASE(def_vb_so, 64, DEF, VB | SO, 0, 0, 0, W_OK, 0)
CASE(def_srv_so, 64, DEF, SR | SO, 0, 0, 0, W_OK, 0)
CASE(def_srv_uav, 64, DEF, SR | UA, 0, 0, 0, W_OK, 1)
CASE(def_vb_ib_srv_so, 64, DEF, VB | IB | SR | SO, 0, 0, 0, W_OK, 0)
CASE(def_vb_ib_srv_so_uav, 64, DEF, VB | IB | SR | SO | UA, 0, 0, 0, W_BAD, 1)
CASE(def_bind0, 64, DEF, 0, 0, 0, 0, W_OK, 0)
CASE(dyn_vb, 64, DYN, VB, CW, 0, 0, W_OK, 0)
CASE(dyn_ib, 64, DYN, IB, CW, 0, 0, W_OK, 0)
CASE(dyn_cb, 64, DYN, CB, CW, 0, 0, W_OK, 0)
CASE(dyn_srv, 64, DYN, SR, CW, 0, 0, W_OK, 0)
CASE(dyn_vb_ib_srv, 64, DYN, VB | IB | SR, CW, 0, 0, W_OK, 0)
CASE(dyn_vb_bw1, 1, DYN, VB, CW, 0, 0, W_OK, 0)
CASE(dyn_cb_16, 16, DYN, CB, CW, 0, 0, W_OK, 0)
CASE(imm_vb, 64, IMM, VB, 0, 0, 0, W_OK, 0)
CASE(imm_ib, 64, IMM, IB, 0, 0, 0, W_OK, 0)
CASE(imm_cb, 64, IMM, CB, 0, 0, 0, W_OK, 0)
CASE(imm_srv, 64, IMM, SR, 0, 0, 0, W_OK, 0)
CASE(imm_vb_bw1, 1, IMM, VB, 0, 0, 0, W_OK, 0)
CASE(imm_vb_ib_srv, 64, IMM, VB | IB | SR, 0, 0, 0, W_OK, 0)
CASE(stg_read, 64, STG, 0, CR, 0, 0, W_OK, 0)
CASE(stg_write, 64, STG, 0, CW, 0, 0, W_OK, 0)
CASE(stg_readwrite, 64, STG, 0, CR | CW, 0, 0, W_OK, 0)
CASE(stg_read_bw1, 1, STG, 0, CR, 0, 0, W_OK, 0)
CASE(stg_read_bw13, 13, STG, 0, CR, 0, 0, W_OK, 0)
CASE(stg_read_16mb, 16 << 20, STG, 0, CR, 0, 0, W_OK, 0)
/* ---- ByteWidth ---- */
CASE(bw0_def_vb, 0, DEF, VB, 0, 0, 0, W_BAD, 0)
CASE(bw0_def_cb, 0, DEF, CB, 0, 0, 0, W_BAD, 0)
CASE(bw0_dyn_vb, 0, DYN, VB, CW, 0, 0, W_BAD, 0)
CASE(bw0_stg, 0, STG, 0, CR, 0, 0, W_BAD, 0)
CASE(bw0_srv, 0, DEF, SR, 0, 0, 0, W_BAD, 0)
CASE(cb_bw1, 1, DEF, CB, 0, 0, 0, W_BAD, 0)
CASE(cb_bw4, 4, DEF, CB, 0, 0, 0, W_BAD, 0)
CASE(cb_bw15, 15, DEF, CB, 0, 0, 0, W_BAD, 0)
CASE(cb_bw17, 17, DEF, CB, 0, 0, 0, W_BAD, 0)
CASE(cb_bw20, 20, DEF, CB, 0, 0, 0, W_BAD, 0)
CASE(cb_bw31, 31, DEF, CB, 0, 0, 0, W_BAD, 0)
CASE(cb_bw100, 100, DEF, CB, 0, 0, 0, W_BAD, 0)
CASE(cb_dyn_bw20, 20, DYN, CB, CW, 0, 0, W_BAD, 0)
CASE(cb_imm_bw20, 20, IMM, CB, 0, 0, 0, W_BAD, 0)
CASE(cb_bw65552, 65552, DEF, CB, 0, 0, 0, W_OK, 0)
CASE(srv_bw_not_mult4, 6, DEF, SR, 0, 0, 0, W_OK, 0)
/* ---- constant buffer exclusivity ---- */
CASE(cb_vb, 64, DEF, CB | VB, 0, 0, 0, W_BAD, 0)
CASE(cb_ib, 64, DEF, CB | IB, 0, 0, 0, W_BAD, 0)
CASE(cb_srv, 64, DEF, CB | SR, 0, 0, 0, W_BAD, 0)
CASE(cb_so, 64, DEF, CB | SO, 0, 0, 0, W_BAD, 0)
CASE(cb_uav, 64, DEF, CB | UA, 0, 0, 0, W_BAD, 0)
CASE(cb_rt, 64, DEF, CB | RT, 0, 0, 0, W_BAD, 0)
CASE(cb_vb_ib, 64, DEF, CB | VB | IB, 0, 0, 0, W_BAD, 0)
CASE(cb_dyn_srv, 64, DYN, CB | SR, CW, 0, 0, W_BAD, 0)
CASE(cb_dyn_vb, 64, DYN, CB | VB, CW, 0, 0, W_BAD, 0)
/* ---- usage rules ---- */
CASE(imm_cpu_write, 64, IMM, VB, CW, 0, 0, W_BAD, 0)
CASE(imm_cpu_read, 64, IMM, VB, CR, 0, 0, W_BAD, 0)
CASE(imm_bind0, 64, IMM, 0, 0, 0, 0, W_BAD, 0)
CASE(def_cpu_write, 64, DEF, VB, CW, 0, 0, W_BAD, 0)
CASE(def_cpu_read, 64, DEF, VB, CR, 0, 0, W_BAD, 0)
CASE(def_cpu_rw, 64, DEF, VB, CR | CW, 0, 0, W_BAD, 0)
CASE(def_srv_cpu_write, 64, DEF, SR, CW, 0, 0, W_MAPDEF, 0)
CASE(def_srv_cpu_read, 64, DEF, SR, CR, 0, 0, W_MAPDEF, 0)
CASE(def_srv_cpu_rw, 64, DEF, SR, CR | CW, 0, 0, W_MAPDEF, 0)
CASE(def_uav_cpu_write, 64, DEF, UA, CW, 0, 0, W_MAPDEF, 1)
CASE(def_uav_cpu_read, 64, DEF, UA, CR, 0, 0, W_MAPDEF, 1)
CASE(def_srv_uav_cpu_rw, 64, DEF, SR | UA, CR | CW, 0, 0, W_MAPDEF, 1)
CASE(def_srv_raw_cpu_write, 64, DEF, SR, CW, RAW, 0, W_MAPDEF, 1)
CASE(def_srv_struct_cpu_write, 64, DEF, SR, CW, STRUCT, 16, W_MAPDEF, 1)
CASE(def_srv_vb_cpu_write, 64, DEF, SR | VB, CW, 0, 0, W_BAD, 0)
CASE(def_srv_ib_cpu_write, 64, DEF, SR | IB, CW, 0, 0, W_BAD, 0)
CASE(def_srv_so_cpu_write, 64, DEF, SR | SO, CW, 0, 0, W_BAD, 0)
CASE(def_cb_cpu_write, 64, DEF, CB, CW, 0, 0, W_BAD, 0)
CASE(def_bind0_cpu_write, 64, DEF, 0, CW, 0, 0, W_BAD, 0)
CASE(def_so_uav, 64, DEF, SO | UA, 0, 0, 0, W_BAD, 1)
CASE(def_vb_so_uav, 64, DEF, VB | SO | UA, 0, 0, 0, W_BAD, 1)
CASE(def_srv_so_uav, 64, DEF, SR | SO | UA, 0, 0, 0, W_BAD, 1)
CASE(def_depth_stencil_vb, 64, DEF, D3D11_BIND_DEPTH_STENCIL | VB, 0, 0, 0, W_BAD, 0)
CASE(def_vb_ib_rt, 64, DEF, VB | IB | RT, 0, 0, 0, W_OK, 0)
CASE(def_rt_uav, 64, DEF, RT | UA, 0, 0, 0, W_OK, 1)
CASE(def_rt_so, 64, DEF, RT | SO, 0, 0, 0, W_OK, 0)
CASE(def_decoder_vb, 64, DEF, D3D11_BIND_DECODER | VB, 0, 0, 0, W_OK, 0)
CASE(def_cb_rt, 64, DEF, CB | RT, 0, 0, 0, W_BAD, 0)
CASE(def_cb_decoder, 64, DEF, CB | D3D11_BIND_DECODER, 0, 0, 0, W_BAD, 0)
CASE(dyn_cpu0, 64, DYN, VB, 0, 0, 0, W_BAD, 0)
CASE(dyn_cpu_read, 64, DYN, VB, CR, 0, 0, W_BAD, 0)
CASE(dyn_cpu_rw, 64, DYN, VB, CR | CW, 0, 0, W_BAD, 0)
CASE(dyn_so, 64, DYN, SO, CW, 0, 0, W_BAD, 0)
CASE(dyn_uav, 64, DYN, UA, CW, 0, 0, W_BAD, 0)
CASE(dyn_rt, 64, DYN, RT, CW, 0, 0, W_BAD, 0)
CASE(dyn_vb_so, 64, DYN, VB | SO, CW, 0, 0, W_BAD, 0)
CASE(stg_cpu0, 64, STG, 0, 0, 0, 0, W_BAD, 0)
CASE(stg_vb, 64, STG, VB, CR, 0, 0, W_BAD, 0)
CASE(stg_ib, 64, STG, IB, CR, 0, 0, W_BAD, 0)
CASE(stg_cb, 64, STG, CB, CR, 0, 0, W_BAD, 0)
CASE(stg_srv, 64, STG, SR, CR, 0, 0, W_BAD, 0)
CASE(stg_so, 64, STG, SO, CR, 0, 0, W_BAD, 0)
CASE(stg_uav, 64, STG, UA, CR, 0, 0, W_BAD, 0)
CASE(stg_rt, 64, STG, RT, CR, 0, 0, W_BAD, 0)
CASE(stg_structured, 64, STG, 0, CR, STRUCT, 16, W_OK, 1)
CASE(stg_raw, 64, STG, 0, CR, RAW, 0, W_BAD, 1)
/* ---- undefined values ---- */
CASE(usage_5, 64, 5, VB, 0, 0, 0, W_BAD, 0)
CASE(usage_ffff, 64, 0xffff, VB, 0, 0, 0, W_BAD, 0)
CASE(bind_undefined_bit, 64, DEF, 0x80000000u, 0, 0, 0, W_BAD, 0)
CASE(bind_undefined_bit_with_vb, 64, DEF, VB | 0x80000000u, 0, 0, 0, W_BAD, 0)
CASE(cpu_undefined_bit, 64, STG, 0, 0x40000, 0, 0, W_BAD, 0)
CASE(cpu_undefined_with_read, 64, STG, 0, CR | 0x40000, 0, 0, W_BAD, 0)
CASE(misc_undefined_bit, 64, DEF, VB, 0, 0x80000000u, 0, W_BAD, 0)
/* ---- misc flags ---- */
CASE(misc_generate_mips, 64, DEF, SR | RT, 0, D3D11_RESOURCE_MISC_GENERATE_MIPS, 0, W_BAD, 0)
CASE(misc_generate_mips_srv, 64, DEF, SR, 0, D3D11_RESOURCE_MISC_GENERATE_MIPS, 0, W_BAD, 0)
CASE(misc_texturecube, 64, DEF, SR, 0, D3D11_RESOURCE_MISC_TEXTURECUBE, 0, W_BAD, 0)
CASE(misc_gdi_compatible, 64, DEF, SR, 0, D3D11_RESOURCE_MISC_GDI_COMPATIBLE, 0, W_BAD, 0)
CASE(misc_shared, 64, DEF, SR, 0, D3D11_RESOURCE_MISC_SHARED, 0, W_OK, 0)
CASE(misc_shared_keyed, 64, DEF, SR, 0, D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX, 0, W_OK, 0)
CASE(misc_resource_clamp, 64, DEF, SR, 0, D3D11_RESOURCE_MISC_RESOURCE_CLAMP, 0, W_BAD, 0)
/* ---- raw / structured / indirect ---- */
CASE(raw_srv_4, 4, DEF, SR, 0, RAW, 0, W_OK, 1)
CASE(raw_srv_64, 64, DEF, SR, 0, RAW, 0, W_OK, 1)
CASE(raw_uav_64, 64, DEF, UA, 0, RAW, 0, W_OK, 1)
CASE(raw_srv_uav_64, 64, DEF, SR | UA, 0, RAW, 0, W_OK, 1)
CASE(raw_vb_srv, 64, DEF, VB | SR, 0, RAW, 0, W_OK, 1)
CASE(raw_imm_srv, 64, IMM, SR, 0, RAW, 0, W_OK, 1)
CASE(raw_bw6, 6, DEF, SR, 0, RAW, 0, W_OK, 1)
CASE(raw_bw1, 1, DEF, SR, 0, RAW, 0, W_OK, 1)
CASE(raw_vb_only, 64, DEF, VB, 0, RAW, 0, W_BAD, 1)
CASE(raw_bind0, 64, DEF, 0, 0, RAW, 0, W_BAD, 1)
CASE(raw_cb, 64, DEF, CB, 0, RAW, 0, W_BAD, 1)
CASE(raw_and_structured, 64, DEF, SR, 0, RAW | STRUCT, 16, W_BAD, 1)
CASE(struct_srv_4, 16, DEF, SR, 0, STRUCT, 4, W_OK, 1)
CASE(struct_srv_16, 64, DEF, SR, 0, STRUCT, 16, W_OK, 1)
CASE(struct_uav_12, 36, DEF, UA, 0, STRUCT, 12, W_OK, 1)
CASE(struct_srv_uav_8, 64, DEF, SR | UA, 0, STRUCT, 8, W_OK, 1)
CASE(struct_stride_2048, 2048, DEF, SR, 0, STRUCT, 2048, W_OK, 1)
CASE(struct_stride_2048_x4, 8192, DEF, SR | UA, 0, STRUCT, 2048, W_OK, 1)
CASE(struct_imm_srv, 64, IMM, SR, 0, STRUCT, 16, W_OK, 1)
CASE(struct_dyn_srv, 64, DYN, SR, CW, STRUCT, 16, W_OK, 1)
CASE(struct_stride0, 64, DEF, SR, 0, STRUCT, 0, W_BAD, 1)
CASE(struct_stride1, 64, DEF, SR, 0, STRUCT, 1, W_BAD, 1)
CASE(struct_stride2, 64, DEF, SR, 0, STRUCT, 2, W_BAD, 1)
CASE(struct_stride3, 63, DEF, SR, 0, STRUCT, 3, W_BAD, 1)
CASE(struct_stride6, 60, DEF, SR, 0, STRUCT, 6, W_BAD, 1)
CASE(struct_stride10, 60, DEF, SR, 0, STRUCT, 10, W_BAD, 1)
CASE(struct_bw_not_multiple, 20, DEF, SR, 0, STRUCT, 12, W_BAD, 1)
CASE(struct_bw_lt_stride, 8, DEF, SR, 0, STRUCT, 16, W_BAD, 1)
CASE(struct_bw_plus1, 65, DEF, SR, 0, STRUCT, 16, W_BAD, 1)
CASE(struct_stride_2052, 2052, DEF, SR, 0, STRUCT, 2052, W_BAD, 1)
CASE(struct_stride_4096, 4096, DEF, SR, 0, STRUCT, 4096, W_BAD, 1)
CASE(struct_vb_only, 64, DEF, VB, 0, STRUCT, 16, W_BAD, 1)
CASE(struct_bind0, 64, DEF, 0, 0, STRUCT, 16, W_OK, 1)
CASE(struct_cb, 64, DEF, CB, 0, STRUCT, 16, W_BAD, 1)
CASE(struct_so_srv, 64, DEF, SR | SO, 0, STRUCT, 16, W_BAD, 1)
CASE(stride_without_structured, 64, DEF, SR, 0, 0, 16, W_OK, 0)
CASE(stride_without_structured_vb, 64, DEF, VB, 0, 0, 4, W_OK, 0)
CASE(indirect_uav, 64, DEF, UA, 0, DIA, 0, W_OK, 1)
CASE(indirect_srv, 64, DEF, SR, 0, DIA, 0, W_OK, 1)
CASE(indirect_bind0, 16, DEF, 0, 0, DIA, 0, W_OK, 1)
CASE(indirect_imm, 16, IMM, 0, 0, DIA, 0, W_BAD, 1)
CASE(indirect_srv_uav_raw, 64, DEF, SR | UA, 0, DIA | RAW, 0, W_OK, 1)
CASE(indirect_structured, 64, DEF, SR, 0, DIA | STRUCT, 16, W_BAD, 1)
CASE(indirect_dyn, 16, DYN, SR, CW, DIA, 0, W_OK, 1)
CASE(bind_render_target, 64, DEF, RT, 0, 0, 0, W_OK, 0)
CASE(bind_render_target_srv, 64, DEF, RT | SR, 0, 0, 0, W_OK, 0)
CASE(bind_decoder, 64, DEF, D3D11_BIND_DECODER, 0, 0, 0, W_OK, 0)
CASE(bind_video_encoder, 64, DEF, D3D11_BIND_VIDEO_ENCODER, 0, 0, 0, W_OK, 0)
CASE(bind_depth_stencil, 64, DEF, D3D11_BIND_DEPTH_STENCIL, 0, 0, 0, W_BAD, 0)
CASE(bind_depth_stencil_srv, 64, DEF, D3D11_BIND_DEPTH_STENCIL | SR, 0, 0, 0, W_BAD, 0)

/* ---- API-level argument rules ---- */
TEST(d3d11_buffers_create_null_desc)
{
    ID3D11Buffer *b = (ID3D11Buffer *)(ULONG_PTR)1;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, NULL, NULL, &b), E_INVALIDARG);
    CHECK(b == NULL || b == (ID3D11Buffer *)(ULONG_PTR)1);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, NULL, NULL, NULL), E_INVALIDARG);
}

TEST(d3d11_buffers_create_validate_only_s_false)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB, 0, 0, 0);
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL), S_FALSE);
    d.ByteWidth = 0;
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL), E_INVALIDARG);
}

TEST(d3d11_buffers_create_validate_only_immutable_needs_data)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, IMM, VB, 0, 0, 0);
    BYTE m[64] = {0};
    D3D11_SUBRESOURCE_DATA sd = { m, 0, 0 };
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, &sd, NULL), S_FALSE);
}

TEST(d3d11_buffers_create_validate_only_does_not_create)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB, 0, 0, 0);
    NEED_DEV();
    /* a validate-only call must not touch device refcount */
    ULONG a = d3dh_refcount(g_dev), b;
    ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL);
    b = d3dh_refcount(g_dev);
    EXPECT_EQ(a, b);
}

#define IMM_NODATA(nm, bind, bw) \
TEST(d3d11_buffers_create_immutable_nodata_##nm) { \
    D3D11_BUFFER_DESC d = bufh_desc(bw, IMM, bind, 0, 0, 0); ID3D11Buffer *b = (ID3D11Buffer *)(ULONG_PTR)1; \
    NEED_DEV(); b = NULL; \
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), E_INVALIDARG); CHECK(b == NULL); SAFE_RELEASE(b); }
IMM_NODATA(vb, VB, 64)
IMM_NODATA(ib, IB, 64)
IMM_NODATA(cb, CB, 64)
IMM_NODATA(srv, SR, 64)
IMM_NODATA(vb_bw1, VB, 1)

TEST(d3d11_buffers_create_initdata_null_sysmem_default)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB, 0, 0, 0);
    D3D11_SUBRESOURCE_DATA sd = { NULL, 0, 0 };
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, &sd, &b), E_INVALIDARG);
    CHECK(b == NULL);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_create_initdata_null_sysmem_immutable)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, IMM, VB, 0, 0, 0);
    D3D11_SUBRESOURCE_DATA sd = { NULL, 0, 0 };
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, &sd, &b), E_INVALIDARG);
    CHECK(b == NULL);
}

TEST(d3d11_buffers_create_initdata_null_sysmem_dynamic)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DYN, VB, CW, 0, 0);
    D3D11_SUBRESOURCE_DATA sd = { NULL, 0, 0 };
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, &sd, &b), E_INVALIDARG);
    CHECK(b == NULL);
}

TEST(d3d11_buffers_create_initdata_null_sysmem_staging)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, STG, 0, CR, 0, 0);
    D3D11_SUBRESOURCE_DATA sd = { NULL, 0, 0 };
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, &sd, &b), E_INVALIDARG);
    CHECK(b == NULL);
}

/* initial data contents, per usage */
static void initdata_roundtrip(D3D11_USAGE usage, UINT bind, UINT cpu, UINT bw, int via_map)
{
    D3D11_BUFFER_DESC d = bufh_desc(bw, usage, bind, cpu, 0, 0);
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    EXPECT_HR(bufh_create(&d, 1, 0x21, &b), S_OK);
    if (!b) return;
    CHECK(bufh_matches_pattern(b, 0x21));
    SAFE_RELEASE(b);
}
#define INIT_RT(nm, usage, bind, cpu, bw) TEST(d3d11_buffers_create_initdata_##nm) { initdata_roundtrip(usage, bind, cpu, bw, 0); }
INIT_RT(default_vb_64, DEF, VB, 0, 64)
INIT_RT(default_vb_1, DEF, VB, 0, 1)
INIT_RT(default_vb_13, DEF, VB, 0, 13)
INIT_RT(default_ib_4096, DEF, IB, 0, 4096)
INIT_RT(default_cb_16, DEF, CB, 0, 16)
INIT_RT(default_cb_1024, DEF, CB, 0, 1024)
INIT_RT(default_srv_777, DEF, SR, 0, 777)
INIT_RT(default_so_64, DEF, SO, 0, 64)
INIT_RT(default_big_1mb, DEF, VB, 0, 1 << 20)
INIT_RT(immutable_vb_64, IMM, VB, 0, 64)
INIT_RT(immutable_vb_1, IMM, VB, 0, 1)
INIT_RT(immutable_ib_130, IMM, IB, 0, 130)
INIT_RT(immutable_cb_256, IMM, CB, 0, 256)
INIT_RT(immutable_srv_333, IMM, SR, 0, 333)
INIT_RT(dynamic_vb_64, DYN, VB, CW, 64)
INIT_RT(dynamic_cb_64, DYN, CB, CW, 64)
INIT_RT(dynamic_srv_5, DYN, SR, CW, 5)
INIT_RT(staging_r_64, STG, 0, CR, 64)
INIT_RT(staging_rw_65, STG, 0, CR | CW, 65)
INIT_RT(staging_w_8, STG, 0, CW, 8)

TEST(d3d11_buffers_create_initdata_pitches_ignored)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB, 0, 0, 0);
    unsigned char src[64];
    D3D11_SUBRESOURCE_DATA sd;
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    bufh_pattern(src, 64, 9);
    sd.pSysMem = src; sd.SysMemPitch = 0x7777; sd.SysMemSlicePitch = 0x1234567;
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, &sd, &b), S_OK);
    if (!b) return;
    CHECK(bufh_matches(b, src, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_create_initdata_copied_at_creation)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB, 0, 0, 0);
    unsigned char src[64], orig[64];
    D3D11_SUBRESOURCE_DATA sd = { src, 0, 0 };
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    bufh_pattern(src, 64, 3); memcpy(orig, src, 64);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, &sd, &b), S_OK);
    memset(src, 0xee, 64);   /* caller memory is not referenced after creation */
    if (!b) return;
    CHECK(bufh_matches(b, orig, 64));
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_create_immutable_initdata_copied)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, IMM, VB, 0, 0, 0);
    unsigned char src[64], orig[64];
    D3D11_SUBRESOURCE_DATA sd = { src, 0, 0 };
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    bufh_pattern(src, 64, 77); memcpy(orig, src, 64);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, &sd, &b), S_OK);
    memset(src, 0, 64);
    if (!b) return;
    CHECK(bufh_matches(b, orig, 64));
    SAFE_RELEASE(b);
}

static void zero_init(D3D11_USAGE usage, UINT bind, UINT cpu, UINT bw, int hard)
{
    D3D11_BUFFER_DESC d = bufh_desc(bw, usage, bind, cpu, 0, 0);
    ID3D11Buffer *b = NULL;
    unsigned char *z; UINT i, nz = 0, sz = 0;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    if (cpu & CR || usage == DEF) {
        z = bufh_read(b, &sz);
        CHECK(z != NULL);
        for (i = 0; z && i < sz; i++) nz += z[i] != 0;
        if (hard) EXPECT_EQ(nz, 0); else if (nz) t_note("%u non-zero bytes in uninitialised buffer", nz);
        free(z);
    }
    SAFE_RELEASE(b);
}
TEST(d3d11_buffers_create_noinit_default_zero) { zero_init(DEF, VB, 0, 256, 1); }
TEST(d3d11_buffers_create_noinit_default_cb_zero) { zero_init(DEF, CB, 0, 256, 1); }
TEST(d3d11_buffers_create_noinit_default_big_zero) { zero_init(DEF, SR, 0, 1 << 20, 1); }
TEST(d3d11_buffers_create_noinit_staging_zero) { zero_init(STG, 0, CR, 256, 1); }
TEST(d3d11_buffers_create_noinit_staging_odd_zero) { zero_init(STG, 0, CR, 37, 1); }

/* ---- GetDesc / GetType / identity ---- */
TEST(d3d11_buffers_getdesc_roundtrip_table)
{
    static const struct { UINT bw; D3D11_USAGE u; UINT bind, cpu, misc, stride; } t[] = {
        { 16, DEF, VB, 0, 0, 0 }, { 17, DEF, IB, 0, 0, 0 }, { 32, DEF, CB, 0, 0, 0 }, { 5, DEF, SR, 0, 0, 0 },
        { 64, DEF, VB | IB | SR | SO, 0, 0, 0 }, { 64, DYN, VB, CW, 0, 0 }, { 64, IMM, IB, 0, 0, 0 },
        { 64, STG, 0, CR, 0, 0 }, { 64, STG, 0, CR | CW, 0, 0 }, { 1, STG, 0, CW, 0, 0 }, { 4096, DEF, SR, 0, 0, 0 },
    };
    size_t i;
    NEED_DEV();
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        D3D11_BUFFER_DESC d = bufh_desc(t[i].bw, t[i].u, t[i].bind, t[i].cpu, t[i].misc, t[i].stride), g;
        ID3D11Buffer *b = NULL;
        HRESULT hr = bufh_create(&d, t[i].u == IMM, 5, &b);
        EXPECT_HR(hr, S_OK);
        if (!b) continue;
        memset(&g, 0xcc, sizeof g);
        ID3D11Buffer_GetDesc(b, &g);
        EXPECT_EQ(g.ByteWidth, d.ByteWidth);
        EXPECT_EQ(g.Usage, d.Usage);
        EXPECT_EQ(g.BindFlags, d.BindFlags);
        EXPECT_EQ(g.CPUAccessFlags, d.CPUAccessFlags);
        EXPECT_EQ(g.MiscFlags, d.MiscFlags);
        EXPECT_EQ(g.StructureByteStride, d.StructureByteStride);
        SAFE_RELEASE(b);
    }
}

TEST(d3d11_buffers_getdesc_structured_stride)
{
    D3D11_BUFFER_DESC d = bufh_desc(96, DEF, SR | UA, 0, STRUCT, 24), g;
    ID3D11Buffer *b = NULL;
    NEED_FL11();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    ID3D11Buffer_GetDesc(b, &g);
    EXPECT_EQ(g.StructureByteStride, 24);
    EXPECT_EQ(g.MiscFlags, STRUCT);
    EXPECT_EQ(g.ByteWidth, 96);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_getdesc_stride_nonstructured_note)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB, 0, 0, 16), g;
    ID3D11Buffer *b = NULL;
    HRESULT hr;
    NEED_DEV();
    hr = ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b);
    EXPECT_HR(hr, S_OK);   /* the stride is ignored for non-structured buffers... */
    if (b) { ID3D11Buffer_GetDesc(b, &g); EXPECT_EQ(g.StructureByteStride, 0); SAFE_RELEASE(b); }   /* ...and reads back as 0 */
}

TEST(d3d11_buffers_gettype_buffer)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB, 0, 0, 0);
    ID3D11Buffer *b = NULL;
    D3D11_RESOURCE_DIMENSION dim = (D3D11_RESOURCE_DIMENSION)99;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    ID3D11Buffer_GetType(b, &dim);
    EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_BUFFER);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_gettype_all_usages)
{
    static const struct { D3D11_USAGE u; UINT bind, cpu; } t[] = { { DEF, CB, 0 }, { DYN, VB, CW }, { STG, 0, CR }, { IMM, IB, 0 } };
    size_t i;
    NEED_DEV();
    for (i = 0; i < 4; i++) {
        D3D11_BUFFER_DESC d = bufh_desc(64, t[i].u, t[i].bind, t[i].cpu, 0, 0);
        ID3D11Buffer *b = NULL;
        D3D11_RESOURCE_DIMENSION dim = (D3D11_RESOURCE_DIMENSION)99;
        EXPECT_HR(bufh_create(&d, t[i].u == IMM, 1, &b), S_OK);
        if (!b) continue;
        ID3D11Buffer_GetType(b, &dim);
        EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_BUFFER);
        SAFE_RELEASE(b);
    }
}

TEST(d3d11_buffers_distinct_objects)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB, 0, 0, 0);
    ID3D11Buffer *a = NULL, *b = NULL;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &a), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    CHECK(a && b && a != b);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_desc_input_not_modified)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, DEF, VB | SR, 0, 0, 0), copy;
    ID3D11Buffer *b = NULL;
    NEED_DEV();
    copy = d;
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    CHECK(!memcmp(&d, &copy, sizeof d));
    SAFE_RELEASE(b);
}

/* ---- many sizes ---- */
TEST(d3d11_buffers_create_vb_sizes_roundtrip)
{
    static const UINT sz[] = { 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1023, 1024, 1025, 4095, 4096, 4097, 65535, 65536, 65537 };
    size_t i;
    NEED_DEV();
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        D3D11_BUFFER_DESC d = bufh_desc(sz[i], DEF, VB, 0, 0, 0), g;
        ID3D11Buffer *b = NULL;
        EXPECT_HR(bufh_create(&d, 1, (unsigned)i, &b), S_OK);
        if (!b) continue;
        ID3D11Buffer_GetDesc(b, &g);
        EXPECT_EQ(g.ByteWidth, sz[i]);
        CHECK(bufh_matches_pattern(b, (unsigned)i));
        SAFE_RELEASE(b);
    }
}

TEST(d3d11_buffers_create_cb_sizes_multiple_of_16)
{
    UINT n;
    NEED_DEV();
    for (n = 16; n <= 4096; n += 16) {
        D3D11_BUFFER_DESC d = bufh_desc(n, DEF, CB, 0, 0, 0);
        HRESULT hr = ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL);
        if (hr != S_FALSE) { EXPECT_HR(hr, S_FALSE); break; }
    }
}

TEST(d3d11_buffers_create_cb_sizes_not_multiple_of_16)
{
    UINT n, bad = 0;
    NEED_DEV();
    for (n = 1; n <= 512; n++) {
        D3D11_BUFFER_DESC d = bufh_desc(n, DEF, CB, 0, 0, 0);
        HRESULT hr = ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL);
        HRESULT want = (n % 16) ? E_INVALIDARG : S_FALSE;
        if (hr != want) { bad++; if (bad < 5) t_note("cb size %u: %s", n, bufh_hrn(hr)); }
    }
    EXPECT_EQ(bad, 0);
}

TEST(d3d11_buffers_create_struct_stride_sweep)
{
    UINT s, bad = 0;
    NEED_FL11();
    for (s = 1; s <= 64; s++) {
        D3D11_BUFFER_DESC d = bufh_desc(s * 4, DEF, SR, 0, STRUCT, s);
        HRESULT hr = ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL);
        HRESULT want = (s % 4) ? E_INVALIDARG : S_FALSE;
        if (hr != want) { bad++; if (bad < 5) t_note("stride %u: %s", s, bufh_hrn(hr)); }
    }
    EXPECT_EQ(bad, 0);
}

TEST(d3d11_buffers_create_struct_bw_multiple_sweep)
{
    UINT bw, bad = 0;
    NEED_FL11();
    for (bw = 1; bw <= 200; bw++) {
        D3D11_BUFFER_DESC d = bufh_desc(bw, DEF, SR, 0, STRUCT, 20);
        HRESULT hr = ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL);
        HRESULT want = (bw % 20) ? E_INVALIDARG : S_FALSE;
        if (hr != want) { bad++; if (bad < 5) t_note("bw %u: %s", bw, bufh_hrn(hr)); }
    }
    EXPECT_EQ(bad, 0);
}

TEST(d3d11_buffers_create_many_buffers_released)
{
    D3D11_BUFFER_DESC d = bufh_desc(256, DEF, VB, 0, 0, 0);
    ID3D11Buffer *b[200];
    int i;
    ULONG rc0, rc1;
    NEED_DEV();
    rc0 = d3dh_refcount(g_dev);
    for (i = 0; i < 200; i++) { b[i] = NULL; EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b[i]), S_OK); }
    for (i = 0; i < 200; i++) SAFE_RELEASE(b[i]);
    rc1 = d3dh_refcount(g_dev);
    EXPECT_EQ(rc0, rc1);
}

TEST(d3d11_buffers_create_large_validate_only)
{
    /* resources up to 128 MB are always allowed (D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM) */
    D3D11_BUFFER_DESC d = bufh_desc(128u << 20, DEF, SR, 0, 0, 0);
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL), S_FALSE);
}

/* ByteWidth 0xffffffff: the validate-only call applies no size ceiling (S_FALSE) while a real allocation fails with E_OUTOFMEMORY. On the Basic
 * Render Driver either call leaves the device in a state that is removed as soon as it does real work (a readback reports
 * DXGI_ERROR_DEVICE_REMOVED). It runs on a private device for that reason. */
TEST(d3d11_buffers_create_bw_ffffffff_oom_private_device)
{
    D3D11_BUFFER_DESC d = bufh_desc(0xffffffffu, DEF, SR, 0, 0, 0), sm = bufh_desc(64, DEF, VB, 0, 0, 0);
    ID3D11Device *dev;
    ID3D11DeviceContext *ctx;
    ID3D11Buffer *b = (ID3D11Buffer *)(ULONG_PTR)1, *c = NULL;
    HRESULT hr, hr2, rr;
    NEED_DEV();
    if (FAILED(bufh_priv_device(0, &dev, &ctx))) { t_skip("no private device"); return; }
    EXPECT_HR(ID3D11Device_CreateBuffer(dev, &d, NULL, NULL), S_FALSE);
    hr = ID3D11Device_CreateBuffer(dev, &d, NULL, &b);
    EXPECT_HR(hr, E_OUTOFMEMORY);
    CHECK(b == NULL);
    {
        ID3D11Device *sdev = g_dev;
        ID3D11DeviceContext *sctx = g_ctx;
        unsigned char *rb;
        g_dev = dev; g_ctx = ctx;
        hr2 = bufh_create(&sm, 1, 0x77, &c);
        rb = hr2 == S_OK ? bufh_read(c, NULL) : NULL;
        ID3D11DeviceContext_Flush(ctx);
        rr = ID3D11Device_GetDeviceRemovedReason(dev);
        t_note("after the OOM: small CreateBuffer %s, readback %s, removed reason %s", bufh_hrn(hr2), rb ? "ok" : "failed", bufh_hrn(rr));
        EXPECT_HR(hr2, S_OK);
        if (bufh_is_brd(dev)) { CHECK(rb == NULL); EXPECT_HR(rr, DXGI_ERROR_DEVICE_REMOVED); }
        free(rb);
        g_dev = sdev; g_ctx = sctx;
    }
    SAFE_RELEASE(c);
    ID3D11DeviceContext_ClearState(ctx); ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
}

/* Whole-matrix pins (WARP/BRD == the default hardware adapter on the reference VM): every subset of the bind flags for a DEFAULT buffer. */
static int default_bind_valid(UINT b)
{
    if (b & D3D11_BIND_CONSTANT_BUFFER) return b == D3D11_BIND_CONSTANT_BUFFER;
    if ((b & SO) && (b & UA)) return 0;
    return 1;
}
TEST(d3d11_buffers_create_bind_matrix_default)
{
    static const UINT bits[] = { VB, IB, CB, SR, SO, RT, UA, D3D11_BIND_DECODER };
    UINT m, bad = 0;
    NEED_FL11();
    for (m = 0; m < 256; m++) {
        UINT bind = 0, k;
        D3D11_BUFFER_DESC d;
        HRESULT hr;
        for (k = 0; k < 8; k++) if (m & (1u << k)) bind |= bits[k];
        d = bufh_desc(64, DEF, bind, 0, 0, 0);
        hr = ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL);
        if (hr != (default_bind_valid(bind) ? S_FALSE : E_INVALIDARG)) { bad++; t_check(0, __FILE__, __LINE__, "bind matrix", "bind=0x%x got %s", bind, bufh_hrn(hr)); }
    }
    CHECK(bad == 0);
}
/* DYNAMIC + CPU write: only VB/IB/SRV (any mix) or a lone CB; nothing else. */
TEST(d3d11_buffers_create_bind_matrix_dynamic)
{
    static const UINT bits[] = { VB, IB, CB, SR, SO, RT, UA };
    UINT m, bad = 0;
    NEED_FL11();
    for (m = 0; m < 128; m++) {
        UINT bind = 0, k;
        D3D11_BUFFER_DESC d;
        HRESULT hr;
        int valid;
        for (k = 0; k < 7; k++) if (m & (1u << k)) bind |= bits[k];
        valid = bind && !(bind & (SO | RT | UA)) && (!(bind & CB) || bind == CB);
        d = bufh_desc(64, DYN, bind, CW, 0, 0);
        hr = ID3D11Device_CreateBuffer(g_dev, &d, NULL, NULL);
        if (hr != (valid ? S_FALSE : E_INVALIDARG)) { bad++; t_check(0, __FILE__, __LINE__, "dyn bind matrix", "bind=0x%x got %s", bind, bufh_hrn(hr)); }
    }
    CHECK(bad == 0);
}
