/* Shader model versus device feature level. */
#include "sh_common.h"

typedef struct { const char *shader; const char *target; int stage; int expect_ok; /* 1 ok, 0 must fail */ } flrow;

static void run_rows(D3D_FEATURE_LEVEL fl, const flrow *rows, int n)
{
    SH_ENV_EXACT(e, fl);
    CHECK(e.fl == fl);
    for (int i = 0; i < n; i++) {
        ID3DBlob *b = sh_blob_target(rows[i].shader, rows[i].target);
        if (!b) continue;
        void *sh = NULL;
        HRESULT hr = sh_create(e.dev, rows[i].stage, SH_BP(b), SH_BN(b), NULL, &sh);
        if (rows[i].expect_ok) t_check(hr == S_OK && sh, __FILE__, __LINE__, "creation succeeds", "FL 0x%x %s as %s: %s", (UINT)fl, rows[i].shader, rows[i].target, d3dh_hr_name(hr));
        else {
            t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "creation fails", "FL 0x%x %s as %s: %s", (UINT)fl, rows[i].shader, rows[i].target, d3dh_hr_name(hr));
            HRESULT hv = sh_create(e.dev, rows[i].stage, SH_BP(b), SH_BN(b), NULL, NULL);
            t_check(hv == E_INVALIDARG, __FILE__, __LINE__, "validation path fails too", "FL 0x%x %s as %s: %s", (UINT)fl, rows[i].shader, rows[i].target, d3dh_hr_name(hv));
        }
        SH_REL(sh);
    }
    sh_env_free(&e);
}

TEST(d3d11_shaders_fl10_0_sm4_stages)
{
    static const flrow r[] = {
        { "vs_pass", "vs_4_0", ST_VS, 1 }, { "ps_const", "ps_4_0", ST_PS, 1 }, { "gs_tri", "gs_4_0", ST_GS, 1 },
        { "vs_noinput", "vs_4_0", ST_VS, 1 }, { "ps_noinput", "ps_4_0", ST_PS, 1 },
        { "vs_many_outputs", "vs_4_0", ST_VS, 1 }, { "ps_mrt8", "ps_4_0", ST_PS, 1 }, { "gs_rtindex", "gs_4_0", ST_GS, 1 },
    };
    run_rows(D3D_FEATURE_LEVEL_10_0, r, sizeof r / sizeof *r);
}
TEST(d3d11_shaders_fl10_0_rejects_sm5)
{
    static const flrow r[] = {
        { "vs_pass_50", "vs_5_0", ST_VS, 0 }, { "ps_const_50", "ps_5_0", ST_PS, 0 }, { "gs_tri_50", "gs_5_0", ST_GS, 0 },
        { "vs_pass", "vs_5_0", ST_VS, 0 }, { "ps_const", "ps_5_0", ST_PS, 0 },
    };
    run_rows(D3D_FEATURE_LEVEL_10_0, r, sizeof r / sizeof *r);
}
TEST(d3d11_shaders_fl10_0_rejects_sm4_1)
{
    static const flrow r[] = {
        { "vs_pass", "vs_4_1", ST_VS, 0 }, { "ps_const", "ps_4_1", ST_PS, 0 }, { "gs_tri", "gs_4_1", ST_GS, 0 },
    };
    run_rows(D3D_FEATURE_LEVEL_10_0, r, sizeof r / sizeof *r);
}
TEST(d3d11_shaders_fl10_0_rejects_tessellation_stages)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_10_0);
    /* Compile the hull/domain shaders for 5_0 (the only profile that exists); creation must fail on a 10.0 device. */
    SH_BLOB(hs, "hs_tri_int"); SH_BLOB(ds, "ds_tri");
    void *sh = NULL;
    HRESULT hr = sh_create(e.dev, ST_HS, SH_BP(hs), SH_BN(hs), NULL, &sh);
    t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "hull on 10_0", "%s", d3dh_hr_name(hr)); SH_REL(sh);
    hr = sh_create(e.dev, ST_DS, SH_BP(ds), SH_BN(ds), NULL, &sh);
    t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "domain on 10_0", "%s", d3dh_hr_name(hr)); SH_REL(sh);
    sh_env_free(&e);
}
TEST(d3d11_shaders_fl10_0_rejects_cs_5_0)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_10_0);
    SH_BLOB(cs, "cs_basic");
    void *sh = NULL;
    HRESULT hr = sh_create(e.dev, ST_CS, SH_BP(cs), SH_BN(cs), NULL, &sh);
    t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "cs_5_0 on 10_0", "%s", d3dh_hr_name(hr));
    SH_REL(sh); sh_env_free(&e);
}

static void cs4x_follows_option(D3D_FEATURE_LEVEL fl, const char *name, const char *target)
{
    SH_ENV_EXACT(e, fl);
    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS opt = { 0xdead };
    HRESULT hq = ID3D11Device_CheckFeatureSupport(e.dev, D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &opt, sizeof opt);
    EXPECT_HR(hq, S_OK);
    SH_BLOB(cs, name);
    (void)target;
    void *sh = NULL;
    HRESULT hr = sh_create(e.dev, ST_CS, SH_BP(cs), SH_BN(cs), NULL, &sh);
    /* WARP reports ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x = TRUE on 10_x, and cs_4_x then creates */
    EXPECT_EQ(opt.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x, 1);
    if (opt.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x) EXPECT_HR(hr, S_OK);
    else { EXPECT_HR(hr, E_INVALIDARG); CHECK(sh == NULL); }
    SH_REL(sh); sh_env_free(&e);
}
TEST(d3d11_shaders_fl10_0_cs_4_0_follows_hardware_option) { cs4x_follows_option(D3D_FEATURE_LEVEL_10_0, "cs_40", "cs_4_0"); }
TEST(d3d11_shaders_fl10_1_cs_4_0_follows_hardware_option) { cs4x_follows_option(D3D_FEATURE_LEVEL_10_1, "cs_40", "cs_4_0"); }
TEST(d3d11_shaders_fl10_1_cs_4_1_follows_hardware_option) { cs4x_follows_option(D3D_FEATURE_LEVEL_10_1, "cs_41", "cs_4_1"); }
TEST(d3d11_shaders_fl10_0_cs_768_threads_follows_hardware_option) { cs4x_follows_option(D3D_FEATURE_LEVEL_10_0, "cs_768", "cs_4_0"); }

TEST(d3d11_shaders_fl10_0_cs_4_1_rejected)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_10_0);
    SH_BLOB(cs, "cs_41");
    void *sh = NULL;
    HRESULT hr = sh_create(e.dev, ST_CS, SH_BP(cs), SH_BN(cs), NULL, &sh);
    t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "cs_4_1 on 10_0", "%s", d3dh_hr_name(hr));
    SH_REL(sh); sh_env_free(&e);
}

TEST(d3d11_shaders_fl10_1_sm4_1_stages)
{
    static const flrow r[] = {
        { "vs_pass_41", "vs_4_1", ST_VS, 1 }, { "ps_sampleidx", "ps_4_1", ST_PS, 1 }, { "gs_41", "gs_4_1", ST_GS, 1 },
        { "ps_gather", "ps_4_1", ST_PS, 1 }, { "vs_pass", "vs_4_0", ST_VS, 1 }, { "ps_const", "ps_4_1", ST_PS, 1 },
    };
    run_rows(D3D_FEATURE_LEVEL_10_1, r, sizeof r / sizeof *r);
}
TEST(d3d11_shaders_fl10_1_rejects_sm5)
{
    static const flrow r[] = {
        { "vs_pass_50", "vs_5_0", ST_VS, 0 }, { "ps_const_50", "ps_5_0", ST_PS, 0 }, { "gs_tri_50", "gs_5_0", ST_GS, 0 },
        { "ps_bufinfo", "ps_5_0", ST_PS, 0 },
    };
    run_rows(D3D_FEATURE_LEVEL_10_1, r, sizeof r / sizeof *r);
}
TEST(d3d11_shaders_fl10_1_rejects_tessellation_and_cs_5_0)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_10_1);
    const char *names[] = { "hs_tri_int", "ds_tri", "cs_basic" };
    int stages[] = { ST_HS, ST_DS, ST_CS };
    for (int i = 0; i < 3; i++) {
        ID3DBlob *b = sh_blob(names[i]); if (!b) continue;
        void *sh = NULL; HRESULT hr = sh_create(e.dev, stages[i], SH_BP(b), SH_BN(b), NULL, &sh);
        t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "5_0 only stage rejected on 10_1", "%s: %s", names[i], d3dh_hr_name(hr)); SH_REL(sh);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_fl11_0_accepts_everything)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_11_0);
    static const flrow r[] = {
        { "vs_pass", "vs_4_0", ST_VS, 1 }, { "vs_pass_41", "vs_4_1", ST_VS, 1 }, { "vs_pass_50", "vs_5_0", ST_VS, 1 },
        { "ps_const", "ps_4_0", ST_PS, 1 }, { "ps_const", "ps_4_1", ST_PS, 1 }, { "ps_const_50", "ps_5_0", ST_PS, 1 },
        { "gs_tri", "gs_4_0", ST_GS, 1 }, { "gs_41", "gs_4_1", ST_GS, 1 }, { "gs_tri_50", "gs_5_0", ST_GS, 1 },
        { "hs_tri_int", "hs_5_0", ST_HS, 1 }, { "ds_tri", "ds_5_0", ST_DS, 1 },
        { "cs_basic", "cs_5_0", ST_CS, 1 }, { "cs_40", "cs_4_0", ST_CS, 1 }, { "cs_41", "cs_4_1", ST_CS, 1 },
    };
    sh_env_free(&e);
    run_rows(D3D_FEATURE_LEVEL_11_0, r, sizeof r / sizeof *r);
}
TEST(d3d11_shaders_fl11_0_cs_5_0_numthreads_limits)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_11_0);
    SH_BLOB(a, "cs_1024"); SH_BLOB(b, "cs_3d");
    void *sh = NULL;
    EXPECT_HR(sh_create(e.dev, ST_CS, SH_BP(a), SH_BN(a), NULL, &sh), S_OK); SH_REL(sh);
    EXPECT_HR(sh_create(e.dev, ST_CS, SH_BP(b), SH_BN(b), NULL, &sh), S_OK); SH_REL(sh);
    sh_env_free(&e);
}

/* Level 9: compile with the *_level_9_x targets. */
TEST(d3d11_shaders_fl9_1_level9_shaders)
{
    static const flrow r[] = {
        { "vs_pass", "vs_4_0_level_9_1", ST_VS, 1 }, { "ps_noinput", "ps_4_0_level_9_1", ST_PS, 1 },
    };
    run_rows(D3D_FEATURE_LEVEL_9_1, r, sizeof r / sizeof *r);
}
TEST(d3d11_shaders_fl9_3_level9_shaders)
{
    static const flrow r[] = {
        { "vs_pass", "vs_4_0_level_9_3", ST_VS, 1 }, { "ps_noinput", "ps_4_0_level_9_3", ST_PS, 1 }, { "vs_pass", "vs_4_0_level_9_1", ST_VS, 1 },
        { "ps_noinput", "ps_4_0_level_9_1", ST_PS, 1 },
    };
    run_rows(D3D_FEATURE_LEVEL_9_3, r, sizeof r / sizeof *r);
}
TEST(d3d11_shaders_fl9_x_rejects_gs_hs_ds_cs)
{
    D3D_FEATURE_LEVEL lv[] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_3 };
    for (int l = 0; l < 3; l++) {
        SHENV e; if (!sh_env_exact(&e, lv[l])) return;
        const char *names[] = { "gs_tri", "hs_tri_int", "ds_tri", "cs_basic" };
        int stages[] = { ST_GS, ST_HS, ST_DS, ST_CS };
        for (int i = 0; i < 4; i++) {
            ID3DBlob *b = sh_blob(names[i]); if (!b) continue;
            void *sh = NULL; HRESULT hr = sh_create(e.dev, stages[i], SH_BP(b), SH_BN(b), NULL, &sh);
            t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "stage unavailable at level 9", "FL 0x%x %s: %s", (UINT)lv[l], names[i], d3dh_hr_name(hr)); SH_REL(sh);
        }
        sh_env_free(&e);
    }
}
TEST(d3d11_shaders_fl9_x_sm4_0_bytecode_observed)
{
    D3D_FEATURE_LEVEL lv[] = { D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_9_3 };
    for (int l = 0; l < 2; l++) {
        SHENV e; if (!sh_env_exact(&e, lv[l])) return;
        SH_BLOB(v, "vs_pass"); SH_BLOB(p, "ps_noinput");
        void *sh = NULL;
        /* plain vs_4_0 / ps_4_0 bytecode (no level-9 profile) is rejected on a 9_x device */
        HRESULT hr = sh_create(e.dev, ST_VS, SH_BP(v), SH_BN(v), NULL, &sh); t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "plain vs_4_0 at 9_x", "FL 0x%x: %s", (UINT)lv[l], d3dh_hr_name(hr)); SH_REL(sh);
        hr = sh_create(e.dev, ST_PS, SH_BP(p), SH_BN(p), NULL, &sh); t_check(hr == E_INVALIDARG && !sh, __FILE__, __LINE__, "plain ps_4_0 at 9_x", "FL 0x%x: %s", (UINT)lv[l], d3dh_hr_name(hr)); SH_REL(sh);
        sh_env_free(&e);
    }
}
TEST(d3d11_shaders_fl9_1_level9_3_pixel_shader_observed)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_9_1);
    SH_BLOB(p, "ps_noinput");
    ID3DBlob *p93 = sh_blob_target("ps_noinput", "ps_4_0_level_9_3"); if (!p93) return;
    void *sh = NULL;
    HRESULT hr = sh_create(e.dev, ST_PS, SH_BP(p93), SH_BN(p93), NULL, &sh);
    /* a level-9_3 profile is accepted by a 9_1 device (the runtime validates the encoding, not the sub-level) */
    EXPECT_HR(hr, S_OK); SH_REL(sh);
    sh_env_free(&e);
}

/* Stage x level matrix for the plain simple shaders. */
TEST(d3d11_shaders_level_matrix_sm_vs_fl)
{
    static const struct { D3D_FEATURE_LEVEL fl; const char *target; int ok; } m[] = {
        { D3D_FEATURE_LEVEL_10_0, "vs_4_0", 1 }, { D3D_FEATURE_LEVEL_10_0, "vs_4_1", 0 }, { D3D_FEATURE_LEVEL_10_0, "vs_5_0", 0 },
        { D3D_FEATURE_LEVEL_10_1, "vs_4_0", 1 }, { D3D_FEATURE_LEVEL_10_1, "vs_4_1", 1 }, { D3D_FEATURE_LEVEL_10_1, "vs_5_0", 0 },
        { D3D_FEATURE_LEVEL_11_0, "vs_4_0", 1 }, { D3D_FEATURE_LEVEL_11_0, "vs_4_1", 1 }, { D3D_FEATURE_LEVEL_11_0, "vs_5_0", 1 },
    };
    for (int i = 0; i < 9; i++) {
        SHENV e; if (!sh_env_exact(&e, m[i].fl)) return;
        ID3DBlob *b = sh_blob_target("vs_pass", m[i].target); if (!b) { sh_env_free(&e); return; }
        void *sh = NULL; HRESULT hr = sh_create(e.dev, ST_VS, SH_BP(b), SH_BN(b), NULL, &sh);
        if (m[i].ok) t_check(hr == S_OK, __FILE__, __LINE__, "vs matrix ok", "FL 0x%x %s: %s", (UINT)m[i].fl, m[i].target, d3dh_hr_name(hr));
        else t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "vs matrix fail", "FL 0x%x %s: %s", (UINT)m[i].fl, m[i].target, d3dh_hr_name(hr));
        SH_REL(sh); sh_env_free(&e);
    }
}
TEST(d3d11_shaders_level_matrix_sm_ps_fl)
{
    static const struct { D3D_FEATURE_LEVEL fl; const char *target; int ok; } m[] = {
        { D3D_FEATURE_LEVEL_10_0, "ps_4_0", 1 }, { D3D_FEATURE_LEVEL_10_0, "ps_4_1", 0 }, { D3D_FEATURE_LEVEL_10_0, "ps_5_0", 0 },
        { D3D_FEATURE_LEVEL_10_1, "ps_4_0", 1 }, { D3D_FEATURE_LEVEL_10_1, "ps_4_1", 1 }, { D3D_FEATURE_LEVEL_10_1, "ps_5_0", 0 },
        { D3D_FEATURE_LEVEL_11_0, "ps_4_0", 1 }, { D3D_FEATURE_LEVEL_11_0, "ps_4_1", 1 }, { D3D_FEATURE_LEVEL_11_0, "ps_5_0", 1 },
    };
    for (int i = 0; i < 9; i++) {
        SHENV e; if (!sh_env_exact(&e, m[i].fl)) return;
        ID3DBlob *b = sh_blob_target("ps_const", m[i].target); if (!b) { sh_env_free(&e); return; }
        void *sh = NULL; HRESULT hr = sh_create(e.dev, ST_PS, SH_BP(b), SH_BN(b), NULL, &sh);
        if (m[i].ok) t_check(hr == S_OK, __FILE__, __LINE__, "ps matrix ok", "FL 0x%x %s: %s", (UINT)m[i].fl, m[i].target, d3dh_hr_name(hr));
        else t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "ps matrix fail", "FL 0x%x %s: %s", (UINT)m[i].fl, m[i].target, d3dh_hr_name(hr));
        SH_REL(sh); sh_env_free(&e);
    }
}
TEST(d3d11_shaders_level_matrix_sm_gs_fl)
{
    static const struct { D3D_FEATURE_LEVEL fl; const char *target; int ok; } m[] = {
        { D3D_FEATURE_LEVEL_10_0, "gs_4_0", 1 }, { D3D_FEATURE_LEVEL_10_0, "gs_4_1", 0 }, { D3D_FEATURE_LEVEL_10_0, "gs_5_0", 0 },
        { D3D_FEATURE_LEVEL_10_1, "gs_4_0", 1 }, { D3D_FEATURE_LEVEL_10_1, "gs_4_1", 1 }, { D3D_FEATURE_LEVEL_10_1, "gs_5_0", 0 },
        { D3D_FEATURE_LEVEL_11_0, "gs_4_0", 1 }, { D3D_FEATURE_LEVEL_11_0, "gs_4_1", 1 }, { D3D_FEATURE_LEVEL_11_0, "gs_5_0", 1 },
    };
    for (int i = 0; i < 9; i++) {
        SHENV e; if (!sh_env_exact(&e, m[i].fl)) return;
        ID3DBlob *b = sh_blob_target("gs_41", m[i].target); if (!b) { sh_env_free(&e); return; }
        void *sh = NULL; HRESULT hr = sh_create(e.dev, ST_GS, SH_BP(b), SH_BN(b), NULL, &sh);
        if (m[i].ok) t_check(hr == S_OK, __FILE__, __LINE__, "gs matrix ok", "FL 0x%x %s: %s", (UINT)m[i].fl, m[i].target, d3dh_hr_name(hr));
        else t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "gs matrix fail", "FL 0x%x %s: %s", (UINT)m[i].fl, m[i].target, d3dh_hr_name(hr));
        SH_REL(sh); sh_env_free(&e);
    }
}

/* A shader bound on a context obtained at a level keeps working across Set/Get. */
TEST(d3d11_shaders_fl10_0_bind_and_readback)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_10_0);
    SH_BLOB(v, "vs_pass"); SH_BLOB(p, "ps_const");
    ID3D11VertexShader *vs = NULL, *g = NULL; ID3D11PixelShader *ps = NULL, *gp = NULL;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(v), SH_BN(v), NULL, &vs), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(p), SH_BN(p), NULL, &ps), S_OK);
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0); ID3D11DeviceContext_PSSetShader(e.ctx, ps, NULL, 0);
    ID3D11DeviceContext_VSGetShader(e.ctx, &g, NULL, NULL); ID3D11DeviceContext_PSGetShader(e.ctx, &gp, NULL, NULL);
    CHECK(g == vs && gp == ps);
    SH_REL(g); SH_REL(gp); SH_REL(vs); SH_REL(ps); sh_env_free(&e);
}

/* Stream-output creation is a 10.0 feature, but not at level 9. */
TEST(d3d11_shaders_fl9_x_rejects_stream_output)
{
    SHENV e; if (!sh_env_exact(&e, D3D_FEATURE_LEVEL_9_3)) return;
    SH_BLOB(b, "gs_so_src");
    D3D11_SO_DECLARATION_ENTRY d = { 0, "SV_Position", 0, 0, 4, 0 }; UINT stride = 16; ID3D11GeometryShader *gs = NULL;
    HRESULT hr = ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), &d, 1, &stride, 1, 0, NULL, &gs);
    t_check(hr == E_INVALIDARG && !gs, __FILE__, __LINE__, "SO at level 9", "%s", d3dh_hr_name(hr)); SH_REL(gs);
    sh_env_free(&e);
}
TEST(d3d11_shaders_fl10_0_stream_output_ok)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_10_0);
    SH_BLOB(b, "gs_so_src");
    D3D11_SO_DECLARATION_ENTRY d = { 0, "SV_Position", 0, 0, 4, 0 }; UINT stride = 16; ID3D11GeometryShader *gs = NULL;
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), &d, 1, &stride, 1, 0, NULL, &gs), S_OK);
    SH_REL(gs);
    /* the 'no rasterized stream' value is a feature-level 11 feature: rejected on a 10_0 device (Win10 22H2) */
    gs = NULL;
    EXPECT_HR(ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), &d, 1, &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, NULL, &gs), E_INVALIDARG);
    CHECK(gs == NULL); SH_REL(gs);
    sh_env_free(&e);
}
TEST(d3d11_shaders_fl10_x_multi_stream_rejected)
{
    SH_ENV_EXACT(e, D3D_FEATURE_LEVEL_10_1);
    SH_BLOB(b, "gs_so_src");
    D3D11_SO_DECLARATION_ENTRY d = { 1, "SV_Position", 0, 0, 4, 0 }; UINT stride = 16; ID3D11GeometryShader *gs = NULL;
    HRESULT hr = ID3D11Device_CreateGeometryShaderWithStreamOutput(e.dev, SH_BP(b), SH_BN(b), &d, 1, &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, NULL, &gs);
    t_check(hr == E_INVALIDARG && !gs, __FILE__, __LINE__, "stream 1 on 10_1", "%s", d3dh_hr_name(hr)); SH_REL(gs);
    sh_env_free(&e);
}
