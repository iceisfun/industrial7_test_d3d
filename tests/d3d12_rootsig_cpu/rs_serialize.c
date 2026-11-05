/* Byte-exact serialization of root signature descriptions (v1.0 and v1.1). */
#include "rs.h"
#define ALL D3D12_SHADER_VISIBILITY_ALL
#define VS  D3D12_SHADER_VISIBILITY_VERTEX
#define HS  D3D12_SHADER_VISIBILITY_HULL
#define DS  D3D12_SHADER_VISIBILITY_DOMAIN
#define GS  D3D12_SHADER_VISIBILITY_GEOMETRY
#define PS  D3D12_SHADER_VISIBILITY_PIXEL
#define SRV D3D12_DESCRIPTOR_RANGE_TYPE_SRV
#define UAV D3D12_DESCRIPTOR_RANGE_TYPE_UAV
#define CBV D3D12_DESCRIPTOR_RANGE_TYPE_CBV
#define SMP D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
#define RCBV D3D12_ROOT_PARAMETER_TYPE_CBV
#define RSRV D3D12_ROOT_PARAMETER_TYPE_SRV
#define RUAV D3D12_ROOT_PARAMETER_TYPE_UAV
#define APPEND D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
#define UNB 0xffffffffu

#define C0(name, flags, ...) \
    TEST(d3d12_rootsig_cpu_v10_##name) { RS_REQUIRE_ALL(); RS0 *b = calloc(1, sizeof *b); if (!b) return; \
        rs0_init(b); __VA_ARGS__; rs_case0(rs0_done(b, flags), #name); free(b); }
#define C1(name, flags, ...) \
    TEST(d3d12_rootsig_cpu_v11_##name) { RS_REQUIRE_ALL(); RS1 *b = calloc(1, sizeof *b); if (!b) return; \
        rs1_init(b); __VA_ARGS__; rs_case1(rs1_done(b, flags), #name); free(b); }

/* ---------------- v1.0 ------------------ */
C0(empty, 0, (void)0)
C0(empty_flag_ia, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, (void)0)
C0(empty_flag_deny_vs, D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS, (void)0)
C0(empty_flag_deny_hs, D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS, (void)0)
C0(empty_flag_deny_ds, D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS, (void)0)
C0(empty_flag_deny_gs, D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS, (void)0)
C0(empty_flag_deny_ps, D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS, (void)0)
C0(empty_flag_so, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT, (void)0)
C0(empty_flag_local, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE, (void)0)
C0(empty_flag_deny_as, D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS, (void)0)
C0(empty_flag_deny_ms, D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS, (void)0)
C0(empty_flags_all_legacy, 0x7f, (void)0)
C0(empty_flags_ia_so, 0x41, (void)0)
C0(empty_flags_deny_all_stages, 0x3e, (void)0)
C0(empty_flags_deny_as_ms, 0x300, (void)0)
C0(consts_1, 0, rs0_consts(b, 0, 0, 1, ALL))
C0(consts_4_reg3_space2, 0, rs0_consts(b, 3, 2, 4, ALL))
C0(consts_zero_values, 0, rs0_consts(b, 0, 0, 0, ALL))
C0(consts_64, 0, rs0_consts(b, 0, 0, 64, ALL))
C0(consts_vs, 0, rs0_consts(b, 1, 0, 8, VS))
C0(consts_ps, 0, rs0_consts(b, 1, 0, 8, PS))
C0(consts_big_register, 0, rs0_consts(b, 0xfffffff0u, 0, 1, ALL))
C0(consts_big_space, 0, rs0_consts(b, 0, 0xfffffff0u, 1, ALL))
C0(cbv_root, 0, rs0_root(b, RCBV, 0, 0, ALL))
C0(cbv_root_reg5_space7, 0, rs0_root(b, RCBV, 5, 7, ALL))
C0(srv_root, 0, rs0_root(b, RSRV, 2, 0, ALL))
C0(uav_root, 0, rs0_root(b, RUAV, 9, 1, ALL))
C0(cbv_srv_uav_roots, 0, rs0_root(b, RCBV, 0, 0, ALL); rs0_root(b, RSRV, 0, 0, ALL); rs0_root(b, RUAV, 0, 0, ALL))
C0(roots_distinct_visibility, 0, rs0_root(b, RCBV, 0, 0, VS); rs0_root(b, RCBV, 0, 0, HS); rs0_root(b, RCBV, 0, 0, DS);
   rs0_root(b, RCBV, 0, 0, GS); rs0_root(b, RCBV, 0, 0, PS))
C0(roots_all_visibilities_same_register, 0, rs0_root(b, RSRV, 4, 0, VS); rs0_root(b, RSRV, 4, 0, PS); rs0_root(b, RSRV, 4, 0, GS))
C0(visibility_amplification_mesh, 0, rs0_root(b, RCBV, 0, 0, D3D12_SHADER_VISIBILITY_AMPLIFICATION);
   rs0_root(b, RCBV, 0, 0, D3D12_SHADER_VISIBILITY_MESH))
C0(table_one_srv, 0, rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, APPEND))
C0(table_one_uav, 0, rs0_table(b, ALL); rs0_range(b, UAV, 1, 0, 0, APPEND))
C0(table_one_cbv, 0, rs0_table(b, ALL); rs0_range(b, CBV, 1, 0, 0, APPEND))
C0(table_one_sampler, 0, rs0_table(b, ALL); rs0_range(b, SMP, 1, 0, 0, APPEND))
C0(table_empty, 0, rs0_table(b, ALL))
C0(table_explicit_offset, 0, rs0_table(b, ALL); rs0_range(b, SRV, 4, 0, 0, 10))
C0(table_unbounded_srv, 0, rs0_table(b, ALL); rs0_range(b, SRV, UNB, 0, 0, APPEND))
C0(table_unbounded_uav_space3, 0, rs0_table(b, PS); rs0_range(b, UAV, UNB, 2, 3, 0))
C0(table_srv_uav_cbv, 0, rs0_table(b, ALL); rs0_range(b, SRV, 2, 0, 0, APPEND); rs0_range(b, UAV, 3, 0, 0, APPEND);
   rs0_range(b, CBV, 1, 0, 0, APPEND))
C0(table_two_ranges_explicit_offsets, 0, rs0_table(b, ALL); rs0_range(b, SRV, 2, 0, 0, 0); rs0_range(b, UAV, 2, 0, 0, 100))
C0(table_unbounded_last, 0, rs0_table(b, ALL); rs0_range(b, CBV, 2, 0, 0, APPEND); rs0_range(b, SRV, UNB, 0, 0, APPEND))
C0(table_same_type_adjacent_registers, 0, rs0_table(b, ALL); rs0_range(b, SRV, 4, 0, 0, APPEND); rs0_range(b, SRV, 4, 4, 0, APPEND))
C0(table_same_registers_different_spaces, 0, rs0_table(b, ALL); rs0_range(b, SRV, 4, 0, 0, APPEND); rs0_range(b, SRV, 4, 0, 1, APPEND))
C0(table_same_registers_different_types, 0, rs0_table(b, ALL); rs0_range(b, SRV, 4, 0, 0, APPEND); rs0_range(b, UAV, 4, 0, 0, APPEND);
   rs0_range(b, CBV, 4, 0, 0, APPEND))
C0(table_sampler_vs, 0, rs0_table(b, VS); rs0_range(b, SMP, 16, 0, 0, APPEND))
C0(table_sampler_ps_srv_ps, 0, rs0_table(b, PS); rs0_range(b, SRV, 8, 0, 0, APPEND); rs0_table(b, PS); rs0_range(b, SMP, 8, 0, 0, APPEND))
C0(two_tables, 0, rs0_table(b, ALL); rs0_range(b, SRV, 2, 0, 0, APPEND); rs0_table(b, ALL); rs0_range(b, UAV, 2, 0, 0, APPEND))
C0(three_tables_varying_ranges, 0, rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, APPEND);
   rs0_table(b, VS); rs0_range(b, CBV, 1, 0, 0, APPEND); rs0_range(b, CBV, 1, 1, 0, APPEND);
   rs0_table(b, PS); rs0_range(b, UAV, 1, 0, 0, APPEND); rs0_range(b, UAV, 1, 1, 0, APPEND); rs0_range(b, UAV, 1, 2, 0, APPEND))
C0(table_between_roots, 0, rs0_root(b, RCBV, 0, 0, ALL); rs0_table(b, ALL); rs0_range(b, SRV, 3, 0, 0, APPEND); rs0_consts(b, 1, 0, 2, ALL))
C0(consts_then_table_then_root, 0, rs0_consts(b, 0, 0, 16, ALL); rs0_table(b, ALL); rs0_range(b, SRV, 3, 0, 0, APPEND); rs0_root(b, RUAV, 0, 0, ALL))
C0(table_empty_then_table, 0, rs0_table(b, ALL); rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, APPEND))
C0(table_ten_ranges, 0, rs0_table(b, ALL); { int i; for (i = 0; i < 10; i++) rs0_range(b, SRV, 1, (UINT)i * 2, 0, APPEND); })
C0(sampler_default, 0, rs0_sampler(b, 0, 0, ALL))
C0(sampler_ps_space1, 0, rs0_sampler(b, 3, 1, PS))
C0(sampler_two_regs, 0, rs0_sampler(b, 0, 0, ALL); rs0_sampler(b, 1, 0, ALL))
C0(sampler_vs_ps_same_register, 0, rs0_sampler(b, 0, 0, VS); rs0_sampler(b, 0, 0, PS))
C0(sampler_all_fields, 0, { D3D12_STATIC_SAMPLER_DESC *s = rs0_sampler(b, 7, 2, GS); s->Filter = D3D12_FILTER_ANISOTROPIC;
   s->AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR; s->AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; s->AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
   s->MipLODBias = -1.5f; s->MaxAnisotropy = 16; s->ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
   s->BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; s->MinLOD = 0.25f; s->MaxLOD = 12.5f; })
C0(sampler_lod_bias_limits, 0, { rs0_sampler(b, 0, 0, ALL)->MipLODBias = -16.0f; rs0_sampler(b, 1, 0, ALL)->MipLODBias = 15.99f; })
C0(sampler_lod_infinite, 0, { rs0_sampler(b, 0, 0, ALL)->MinLOD = -3.4e38f; rs0_sampler(b, 1, 0, ALL)->MaxLOD = 1.0f / 0.0f; })
C0(sampler_with_everything, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
   rs0_consts(b, 0, 0, 4, ALL); rs0_root(b, RCBV, 1, 0, VS); rs0_table(b, PS); rs0_range(b, SRV, 2, 0, 0, APPEND); rs0_sampler(b, 0, 0, PS))
C0(samplers_16, 0, { int i; for (i = 0; i < 16; i++) rs0_sampler(b, (UINT)i, 0, ALL); })
C0(samplers_20_spaces, 0, { int i; for (i = 0; i < 20; i++) rs0_sampler(b, 0, (UINT)i, ALL); })
C0(params_8_consts, 0, { int i; for (i = 0; i < 8; i++) rs0_consts(b, (UINT)i, 0, 1, ALL); })
C0(params_16_roots, 0, { int i; for (i = 0; i < 16; i++) rs0_root(b, (D3D12_ROOT_PARAMETER_TYPE)(RCBV + (i % 3)), (UINT)i, 0, ALL); })
C0(params_32_mixed, 0, { int i; for (i = 0; i < 32; i++) { switch (i % 4) {
   case 0: rs0_consts(b, (UINT)i, 0, 2, ALL); break; case 1: rs0_root(b, RSRV, (UINT)i, 0, ALL); break;
   case 2: rs0_table(b, ALL); rs0_range(b, UAV, 1, (UINT)i, 0, APPEND); break; default: rs0_root(b, RCBV, (UINT)i, 0, ALL); } } })
C0(params_64_tables, 0, { int i; for (i = 0; i < 64; i++) { rs0_table(b, ALL); rs0_range(b, SRV, 1, (UINT)i, 0, APPEND); } })
C0(params_64_roots_and_samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT,
   { int i; for (i = 0; i < 64; i++) rs0_root(b, RCBV, (UINT)i, 0, ALL); for (i = 0; i < 4; i++) rs0_sampler(b, (UINT)i, 0, ALL); })
C0(large_table_100_ranges, 0, { int i; rs0_table(b, ALL); for (i = 0; i < 100; i++) rs0_range(b, (D3D12_DESCRIPTOR_RANGE_TYPE)0, 1, (UINT)i, 0, APPEND); })
C0(max_register_range, 0, rs0_table(b, ALL); rs0_range(b, SRV, 1, 0xfffffffeu, 0, APPEND))
C0(max_registers_last_slot, 0, rs0_table(b, ALL); rs0_range(b, SRV, 2, 0xfffffffeu, 0, APPEND))
C0(offset_large, 0, rs0_table(b, ALL); rs0_range(b, SRV, 1, 0, 0, 0xfffffff0u))
C0(unbounded_after_explicit, 0, rs0_table(b, ALL); rs0_range(b, SRV, 3, 0, 0, 7); rs0_range(b, UAV, UNB, 0, 0, 10))

/* ---------------- v1.1 ------------------ */
#define F_DV  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
#define F_DDV D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE
#define F_DSE D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
#define F_DS  D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC
#define F_BC  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS
#define R_NONE D3D12_ROOT_DESCRIPTOR_FLAG_NONE
#define R_DV   D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE
#define R_DSE  D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
#define R_DS   D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC

C1(empty, 0, (void)0)
C1(empty_flag_ia, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, (void)0)
C1(empty_flag_local, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE, (void)0)
C1(empty_flags_0x3ff_no_local, 0x37f, (void)0)
C1(consts, 0, rs1_consts(b, 2, 3, 5, PS))
C1(root_cbv_none, 0, rs1_root(b, RCBV, 0, 0, R_NONE, ALL))
C1(root_cbv_dv, 0, rs1_root(b, RCBV, 0, 0, R_DV, ALL))
C1(root_cbv_dse, 0, rs1_root(b, RCBV, 0, 0, R_DSE, ALL))
C1(root_cbv_ds, 0, rs1_root(b, RCBV, 0, 0, R_DS, ALL))
C1(root_srv_dse, 0, rs1_root(b, RSRV, 1, 0, R_DSE, VS))
C1(root_uav_dv, 0, rs1_root(b, RUAV, 2, 1, R_DV, PS))
C1(root_uav_ds, 0, rs1_root(b, RUAV, 2, 1, R_DS, ALL))
C1(table_srv_flag_none, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, 0, APPEND))
C1(table_srv_dv, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DV, APPEND))
C1(table_srv_ddv, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DDV, APPEND))
C1(table_srv_dse, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DSE, APPEND))
C1(table_srv_ds, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DS, APPEND))
C1(table_srv_bounds_checks, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_BC, APPEND))
C1(table_srv_dv_ddv, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DV | F_DDV, APPEND))
C1(table_srv_dv_dse, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_DV | F_DSE, APPEND))
C1(table_srv_bounds_ds, 0, rs1_table(b, ALL); rs1_range(b, SRV, 1, 0, 0, F_BC | F_DS, APPEND))
C1(table_sampler_dv, 0, rs1_table(b, ALL); rs1_range(b, SMP, 4, 0, 0, F_DV, APPEND))
C1(table_sampler_none, 0, rs1_table(b, ALL); rs1_range(b, SMP, 4, 0, 0, 0, APPEND))
C1(table_mixed_flags, 0, rs1_table(b, ALL); rs1_range(b, SRV, 2, 0, 0, F_DSE, APPEND); rs1_range(b, UAV, 2, 0, 0, F_DDV, APPEND);
   rs1_range(b, CBV, 2, 0, 0, F_DS, APPEND))
C1(table_unbounded, 0, rs1_table(b, ALL); rs1_range(b, SRV, UNB, 0, 0, F_DV, APPEND))
C1(table_explicit_offsets, 0, rs1_table(b, ALL); rs1_range(b, SRV, 2, 0, 0, 0, 5); rs1_range(b, UAV, 2, 0, 0, 0, 50))
C1(three_params_with_sampler, D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS,
   rs1_consts(b, 0, 0, 4, ALL); rs1_root(b, RSRV, 1, 0, R_DSE, VS); rs1_table(b, VS); rs1_range(b, SRV, 1, 2, 0, F_DSE, APPEND); rs1_sampler(b, 0, 0, VS))
C1(roots_same_register_distinct_visibility, 0, rs1_root(b, RCBV, 0, 0, R_NONE, VS); rs1_root(b, RCBV, 0, 0, R_DS, PS))
C1(params_64_mixed, 0, { int i; for (i = 0; i < 64; i++) { switch (i % 3) {
   case 0: rs1_consts(b, (UINT)i, 0, 1, ALL); break; case 1: rs1_root(b, RSRV, (UINT)i, 0, R_DSE, ALL); break;
   default: rs1_table(b, ALL); rs1_range(b, UAV, 1, (UINT)i, 0, F_DV, APPEND); } } })
C1(samplers_8, 0, { int i; for (i = 0; i < 8; i++) rs1_sampler(b, (UINT)i, 0, ALL); })
C1(sampler_fields, 0, { D3D12_STATIC_SAMPLER_DESC *s = rs1_sampler(b, 1, 1, PS); s->Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
   s->ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; s->MipLODBias = 2.0f; s->MaxAnisotropy = 8; s->BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK; })

/* ---------- table driven: sampler enums ------------ */
static void ser_one_sampler(const D3D12_STATIC_SAMPLER_DESC *tmpl, const char *tag) {
    RS0 *b = calloc(1, sizeof *b); if (!b) return;
    rs0_init(b); *rs0_sampler(b, 0, 0, ALL) = *tmpl;
    rs_case0(rs0_done(b, 0), tag); free(b);
}
static void ser_one_sampler1(const D3D12_STATIC_SAMPLER_DESC *tmpl, const char *tag) {
    RS1 *b = calloc(1, sizeof *b); if (!b) return;
    rs1_init(b); *rs1_sampler(b, 0, 0, ALL) = *tmpl;
    rs_case1(rs1_done(b, 0), tag); free(b);
}
static const UINT k_filters[] = { 0x00,0x01,0x04,0x05,0x10,0x11,0x14,0x15,0x55,0x80,0x81,0x84,0x85,0x90,0x91,0x94,0x95,0xd5 };
static const UINT k_minmax_filters[] = { 0x100,0x101,0x104,0x105,0x110,0x111,0x114,0x115,0x155,0x180,0x181,0x184,0x185,0x190,0x191,0x194,0x195,0x1d5 };

TEST(d3d12_rootsig_cpu_ser_sampler_filters) {
    RS_REQUIRE_ALL(); size_t i;
    for (i = 0; i < sizeof k_filters / sizeof *k_filters; i++) {
        D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
        s = *rs0_sampler(t, 0, 0, ALL); free(t);
        s.Filter = (D3D12_FILTER)k_filters[i]; s.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
        s.MaxAnisotropy = (k_filters[i] == 0x55 || k_filters[i] == 0xd5) ? 16 : 1;
        ser_one_sampler(&s, "filter"); ser_one_sampler1(&s, "filter1");
    }
}
TEST(d3d12_rootsig_cpu_ser_sampler_minmax_filters) {
    RS_REQUIRE_ALL(); size_t i;
    for (i = 0; i < sizeof k_minmax_filters / sizeof *k_minmax_filters; i++) {
        D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
        s = *rs0_sampler(t, 0, 0, ALL); free(t);
        s.Filter = (D3D12_FILTER)k_minmax_filters[i]; s.MaxAnisotropy = 4;
        ser_one_sampler(&s, "minmax filter"); ser_one_sampler1(&s, "minmax filter1");
    }
}
TEST(d3d12_rootsig_cpu_ser_sampler_address_modes) {
    RS_REQUIRE_ALL(); UINT u, v, w;
    for (u = 1; u <= 5; u++) for (v = 1; v <= 5; v++) for (w = 1; w <= 5; w += 2) {
        D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
        s = *rs0_sampler(t, 0, 0, ALL); free(t);
        s.AddressU = u; s.AddressV = v; s.AddressW = w; ser_one_sampler(&s, "address");
    }
}
TEST(d3d12_rootsig_cpu_ser_sampler_border_colors) {
    RS_REQUIRE_ALL(); UINT c;
    for (c = 0; c <= 2; c++) {
        D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
        s = *rs0_sampler(t, 0, 0, ALL); free(t);
        s.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER; s.BorderColor = c; ser_one_sampler(&s, "border");
    }
}
TEST(d3d12_rootsig_cpu_ser_sampler_comparison_funcs) {
    RS_REQUIRE_ALL(); UINT f;
    for (f = 1; f <= 8; f++) {
        D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
        s = *rs0_sampler(t, 0, 0, ALL); free(t);
        s.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT; s.ComparisonFunc = f; ser_one_sampler(&s, "cmp");
    }
}
TEST(d3d12_rootsig_cpu_ser_sampler_anisotropy_values) {
    RS_REQUIRE_ALL(); UINT a;
    for (a = 0; a <= 16; a++) {
        D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
        s = *rs0_sampler(t, 0, 0, ALL); free(t);
        s.Filter = D3D12_FILTER_ANISOTROPIC; s.MaxAnisotropy = a; ser_one_sampler(&s, "aniso");
    }
}
TEST(d3d12_rootsig_cpu_ser_sampler_visibilities) {
    RS_REQUIRE_ALL(); UINT v;
    for (v = 0; v <= 7; v++) {
        D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
        s = *rs0_sampler(t, 0, 0, ALL); free(t);
        s.ShaderVisibility = v; ser_one_sampler(&s, "vis");
    }
}
TEST(d3d12_rootsig_cpu_ser_sampler_lod_bias_sweep) {
    RS_REQUIRE_ALL(); int i;
    for (i = -16; i <= 15; i++) {
        D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
        s = *rs0_sampler(t, 0, 0, ALL); free(t);
        s.MipLODBias = (float)i * 0.99f; ser_one_sampler(&s, "bias");
    }
}
TEST(d3d12_rootsig_cpu_ser_sampler_border_not_validated_when_not_border_mode) {
    RS_REQUIRE_ALL();
    /* BorderColor is stored verbatim even when no address mode uses it and even for out of range values:
       the serializer does not validate it (verifier source: no check on this field). */
    D3D12_STATIC_SAMPLER_DESC s; RS0 *t = calloc(1, sizeof *t); if (!t) return;
    s = *rs0_sampler(t, 0, 0, ALL); free(t);
    s.BorderColor = (D3D12_STATIC_BORDER_COLOR)7;
    ID3DBlob *bl = NULL, *e = NULL; RS0 *b = calloc(1, sizeof *b); if (!b) return; rs0_init(b); *rs0_sampler(b, 0, 0, ALL) = s;
    HRESULT hr = rs_ser0(rs0_done(b, 0), &bl, &e);
    EXPECT_HR(hr, S_OK);
    if (bl) { RSBUF m; rs_model0(&b->d, &m); rs_blob_matches(bl, &m, "border7"); }
    SAFE_RELEASE(bl); SAFE_RELEASE(e); free(b);
}
