/* Table-driven checks over the committed HLSL sources (assets/shaders/d3d11sh_*.hlsl). */
#include "sh_common.h"

TEST(d3d11_shaders_table_size_and_names)
{
    CHECK(SH_NTABLE >= 60);
    for (int i = 0; i < SH_NTABLE; i++) {
        CHECKF(sh_stage_of_target(sh_table[i].target) >= 0, "entry %s has an unknown stage", sh_table[i].name);
        for (int j = i + 1; j < SH_NTABLE; j++) CHECKF(strcmp(sh_table[i].name, sh_table[j].name), "duplicate %s", sh_table[i].name);
    }
}

TEST(d3d11_shaders_table_compile_all)
{
    REQUIRE(d3dh_D3DCompile());
    int ok = 0, bad = 0;
    for (int i = 0; i < SH_NTABLE; i++) {
        ID3DBlob *b = NULL;
        HRESULT hr = d3dh_compile(sh_table[i].src, "main", sh_table[i].target, 0, &b);
        if (SUCCEEDED(hr)) {
            ok++;
            const unsigned char *p = SH_BP(b);
            CHECKF(SH_BN(b) >= 36 && !memcmp(p, "DXBC", 4), "%s: blob is not a DXBC container", sh_table[i].name);
            CHECKF(*(const DWORD *)(p + 24) == SH_BN(b), "%s: container size field %lu != blob size %llu", sh_table[i].name, *(const DWORD *)(p + 24), (unsigned long long)SH_BN(b));
            CHECKF(*(const DWORD *)(p + 20) == 1, "%s: container version", sh_table[i].name);
            ID3D10Blob_Release(b);
        } else {
            bad++;
            if (t_is_wine()) t_note("wine compiler rejects %s (%s)", sh_table[i].name, sh_table[i].target);
            else CHECKF(0, "%s (%s) failed to compile: %s %s", sh_table[i].name, sh_table[i].target, d3dh_hr_name(hr), d3dh_compile_errors());
        }
    }
    t_note("compiled %d of %d table shaders", ok, ok + bad);
}

/* Every table entry creates S_OK through its stage's creator on a device that is at or above the shader's level. */
TEST(d3d11_shaders_table_create_all)
{
    SH_ENV(e, 0);
    int made = 0;
    for (int i = 0; i < SH_NTABLE; i++) {
        const sh_ent *t = &sh_table[i];
        if ((UINT)e.fl < t->minfl) continue;
        if (!strcmp(t->name, "gs_stream")) continue; /* multi-stream GS needs the stream-output creator */
        ID3DBlob *b = sh_blob(t->name);
        if (!b) continue;
        int st = sh_stage_of_target(t->target);
        void *sh = NULL;
        HRESULT hr = sh_create(e.dev, st, SH_BP(b), SH_BN(b), NULL, &sh);
        t_check(hr == S_OK && sh, __FILE__, __LINE__, "table shader creates", "%s (%s): %s", t->name, t->target, d3dh_hr_name(hr));
        if (sh) { made++; SH_REL(sh); }
    }
    t_note("created %d table shaders at feature level 0x%x", made, (UINT)e.fl);
    CHECK(made >= 40 || t_is_wine());
    sh_env_free(&e);
}

/* The same bytecode must be rejected by the creator of every other stage. */
TEST(d3d11_shaders_table_cross_stage_matrix)
{
    SH_ENV(e, 0xb000);
    int checked = 0;
    for (int i = 0; i < SH_NTABLE; i++) {
        const sh_ent *t = &sh_table[i];
        if ((UINT)e.fl < t->minfl || !strcmp(t->name, "gs_stream")) continue;
        if (i % 3) continue; /* every third entry keeps the matrix bounded */
        ID3DBlob *b = sh_blob(t->name); if (!b) continue;
        int st = sh_stage_of_target(t->target);
        for (int o = 0; o < ST_COUNT; o++) {
            if (o == st) continue;
            HRESULT hr = sh_create(e.dev, o, SH_BP(b), SH_BN(b), NULL, NULL);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "stage mismatch", "%s in %s creator: %s", t->name, sh_stage_name[o], d3dh_hr_name(hr));
            checked++;
        }
    }
    CHECK(checked > 40);
    sh_env_free(&e);
}

/* Truncating each table shader by one byte invalidates it. */
TEST(d3d11_shaders_table_truncated_by_one)
{
    SH_ENV(e, 0);
    for (int i = 0; i < SH_NTABLE; i++) {
        const sh_ent *t = &sh_table[i];
        if ((UINT)e.fl < t->minfl || !strcmp(t->name, "gs_stream")) continue;
        ID3DBlob *b = sh_blob(t->name); if (!b) continue;
        HRESULT hr = sh_create(e.dev, sh_stage_of_target(t->target), SH_BP(b), SH_BN(b) - 1, NULL, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "truncated by one", "%s: %s", t->name, d3dh_hr_name(hr));
    }
    sh_env_free(&e);
}

/* Validation path (NULL out) agrees with the real creation path for each entry. */
TEST(d3d11_shaders_table_validate_matches_create)
{
    SH_ENV(e, 0);
    for (int i = 0; i < SH_NTABLE; i++) {
        const sh_ent *t = &sh_table[i];
        if ((UINT)e.fl < t->minfl || !strcmp(t->name, "gs_stream")) continue;
        ID3DBlob *b = sh_blob(t->name); if (!b) continue;
        int st = sh_stage_of_target(t->target);
        HRESULT hv = sh_create(e.dev, st, SH_BP(b), SH_BN(b), NULL, NULL);
        void *sh = NULL; HRESULT hc = sh_create(e.dev, st, SH_BP(b), SH_BN(b), NULL, &sh);
        t_check(hv == S_FALSE && hc == S_OK, __FILE__, __LINE__, "validate/create pair", "%s: validate %s create %s", t->name, d3dh_hr_name(hv), d3dh_hr_name(hc));
        SH_REL(sh);
    }
    sh_env_free(&e);
}

/* Binding every table shader on the immediate context and reading it back. */
TEST(d3d11_shaders_table_bind_all)
{
    SH_ENV(e, 0);
    for (int i = 0; i < SH_NTABLE; i++) {
        const sh_ent *t = &sh_table[i];
        if ((UINT)e.fl < t->minfl || !strcmp(t->name, "gs_stream")) continue;
        ID3DBlob *b = sh_blob(t->name); if (!b) continue;
        int st = sh_stage_of_target(t->target);
        void *sh = NULL, *got = NULL;
        if (FAILED(sh_create(e.dev, st, SH_BP(b), SH_BN(b), NULL, &sh))) { CHECKF(0, "create %s", t->name); continue; }
        sh_set(e.ctx, st, sh, NULL, 0);
        sh_get(e.ctx, st, &got, NULL, NULL);
        t_check(got == sh, __FILE__, __LINE__, "bound shader read back", "%s", t->name);
        SH_REL(got);
        sh_set(e.ctx, st, NULL, NULL, 0);
        SH_REL(sh);
    }
    sh_env_free(&e);
}
