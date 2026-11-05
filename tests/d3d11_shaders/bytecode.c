/* DXBC container details: checksum, optional chunks, stripping, reassembly. */
#include "sh_common.h"

/* ---- DXBC container checksum (MD5 variant used by the shader compiler) ---- */
static void md5_block(uint32_t st[4], const uint8_t *p)
{
    static uint32_t K[64]; static int init;
    static const int S[64] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                               4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
    if (!init) { for (int i = 0; i < 64; i++) K[i] = (uint32_t)(4294967296.0 * fabs(sin((double)(i + 1)))); init = 1; }
    uint32_t M[16]; memcpy(M, p, 64);
    uint32_t a = st[0], b = st[1], c = st[2], d = st[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f; int g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | ~d); g = (7 * i) & 15; }
        uint32_t t = d; d = c; c = b;
        uint32_t x = a + f + K[i] + M[g];
        b = b + ((x << S[i]) | (x >> (32 - S[i]))); a = t;
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
}
static void dxbc_checksum(const uint8_t *data, size_t total, uint32_t out[4])
{
    const uint8_t *p = data + 20; size_t size = total - 20;
    uint32_t st[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    size_t full = size / 64; uint8_t blk[64];
    for (size_t i = 0; i < full; i++) md5_block(st, p + i * 64);
    size_t left = size & 63; p += full * 64;
    uint32_t bits = (uint32_t)size * 8, bits_or = ((uint32_t)size * 2) | 1;
    if (left >= 56) {
        memset(blk, 0, 64); memcpy(blk, p, left); blk[left] = 0x80; md5_block(st, blk);
        memset(blk, 0, 64); memcpy(blk, &bits, 4); memcpy(blk + 60, &bits_or, 4); md5_block(st, blk);
    } else {
        memset(blk, 0, 64); memcpy(blk, &bits, 4); memcpy(blk + 4, p, left); blk[4 + left] = 0x80; memcpy(blk + 60, &bits_or, 4); md5_block(st, blk);
    }
    memcpy(out, st, 16);
}
static void dxbc_rehash(uint8_t *data, size_t total) { uint32_t h[4]; dxbc_checksum(data, total, h); memcpy(data + 4, h, 16); }

typedef HRESULT (WINAPI *SetBlobPart_t)(LPCVOID, SIZE_T, D3D_BLOB_PART, UINT, LPCVOID, SIZE_T, ID3DBlob **);

static const char *const hash_names[] = { "vs_pass", "ps_const", "ps_texture", "vs_matrix", "vs_many_outputs", "cs_basic", "gs_tri", "ps_mrt8", "vs_loop", "cs_40", "ps_ddx", "vs_struct_in" };

TEST(d3d11_shaders_bytecode_compiler_checksum_matches_algorithm)
{
    int n = 0;
    for (size_t i = 0; i < sizeof hash_names / sizeof *hash_names; i++) {
        ID3DBlob *b = sh_blob(hash_names[i]); if (!b) continue;
        uint32_t h[4]; dxbc_checksum(SH_BP(b), SH_BN(b), h);
        t_check(!memcmp(h, (const uint8_t *)SH_BP(b) + 4, 16), __FILE__, __LINE__, "stored checksum equals recomputed", "%s", hash_names[i]); n++;
    }
    CHECK(n > 0 || t_is_wine());
}
TEST(d3d11_shaders_bytecode_rehash_after_edit_is_accepted)
{
    /* Appending nothing: re-computing the checksum of an unmodified container must leave it valid. */
    SH_ENV(e, 0);
    for (int st = 0; st < ST_COUNT; st++) {
        const char *nm = (const char *[]){ "vs_pass", "ps_const", "gs_tri", "hs_tri_int", "ds_tri", "cs_basic" }[st];
        ID3DBlob *b = sh_blob(nm); if (!b) continue;
        if (st >= ST_HS && st != ST_CS && (UINT)e.fl < 0xb000) continue;
        if (st == ST_CS && (UINT)e.fl < 0xb000) continue;
        unsigned char *p = sh_dup(b, 0); dxbc_rehash(p, SH_BN(b));
        void *sh = NULL; HRESULT hr = sh_create(e.dev, st, p, SH_BN(b), NULL, &sh);
        t_check(hr == S_OK, __FILE__, __LINE__, "rehashed container valid", "%s: %s", nm, d3dh_hr_name(hr)); SH_REL(sh); free(p);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_stale_checksum_after_payload_edit_observed)
{
    SH_ENV(e, 0); SH_BLOB(b, "ps_const");
    ID3DBlob *pb = NULL; SetBlobPart_t sbp = (SetBlobPart_t)d3dh_getproc("d3dcompiler_47.dll", "D3DSetBlobPart");
    REQUIRE(sbp);
    HRESULT hr = sbp(SH_BP(b), SH_BN(b), D3D_BLOB_PRIVATE_DATA, 0, "payload", 8, &pb);
    if (FAILED(hr)) { t_skip("D3DSetBlobPart: %s", d3dh_hr_name(hr)); return; }
    void *sh = NULL;
    EXPECT_HR(sh_create(e.dev, ST_PS, SH_BP(pb), SH_BN(pb), NULL, &sh), S_OK); SH_REL(sh); /* private-data chunk is legal */
    unsigned char *p = sh_dup(pb, 0);
    /* flip one byte inside the private data chunk (last 8 bytes of the container) without fixing the checksum */
    p[SH_BN(pb) - 2] ^= 0x55;
    hr = sh_create(e.dev, ST_PS, p, SH_BN(pb), NULL, &sh);
    t_note("container whose private-data payload no longer matches its checksum -> %s", d3dh_hr_name(hr)); SH_REL(sh);
    dxbc_rehash(p, SH_BN(pb));
    EXPECT_HR(sh_create(e.dev, ST_PS, p, SH_BN(pb), NULL, &sh), S_OK); SH_REL(sh);
    free(p); ID3D10Blob_Release(pb); sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_zero_checksum_observed)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); unsigned char *p = sh_dup(b, 0); void *sh = NULL;
    memset(p + 4, 0, 16);
    HRESULT hr = sh_create(e.dev, ST_VS, p, SH_BN(b), NULL, &sh);
    t_note("container with an all-zero checksum -> %s", d3dh_hr_name(hr)); CHECK(hr == S_OK || hr == E_INVALIDARG); SH_REL(sh);
    free(p); sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_skip_validation_flag_observed)
{
    SH_ENV(e, 0); ID3DBlob *b = NULL;
    HRESULT hr = d3dh_compile(sh_find("vs_pass")->src, "main", "vs_4_0", D3DCOMPILE_SKIP_VALIDATION, &b);
    if (FAILED(hr)) { t_skip("compile: %s", d3dh_hr_name(hr)); return; }
    void *sh = NULL; hr = sh_create(e.dev, ST_VS, SH_BP(b), SH_BN(b), NULL, &sh);
    t_note("unvalidated container (D3DCOMPILE_SKIP_VALIDATION) -> %s", d3dh_hr_name(hr)); SH_REL(sh);
    ID3D10Blob_Release(b); sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_debug_compile_flags_accepted)
{
    SH_ENV(e, 0);
    static const UINT flags[] = { D3DCOMPILE_DEBUG, D3DCOMPILE_SKIP_OPTIMIZATION, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                  D3DCOMPILE_ENABLE_STRICTNESS, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, D3DCOMPILE_AVOID_FLOW_CONTROL };
    for (size_t i = 0; i < sizeof flags / sizeof *flags; i++) {
        ID3DBlob *b = NULL; HRESULT hr = d3dh_compile(sh_find("vs_matrix")->src, "main", "vs_4_0", flags[i], &b);
        if (FAILED(hr)) { if (!t_is_wine()) CHECKF(0, "compile flags 0x%x: %s", flags[i], d3dh_hr_name(hr)); continue; }
        void *sh = NULL; hr = sh_create(e.dev, ST_VS, SH_BP(b), SH_BN(b), NULL, &sh);
        t_check(hr == S_OK, __FILE__, __LINE__, "bytecode from compile flags creates", "flags 0x%x: %s", flags[i], d3dh_hr_name(hr)); SH_REL(sh);
        ID3D10Blob_Release(b);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_optimization_levels_accepted)
{
    SH_ENV(e, 0);
    static const UINT flags[] = { D3DCOMPILE_OPTIMIZATION_LEVEL0, D3DCOMPILE_OPTIMIZATION_LEVEL1, D3DCOMPILE_OPTIMIZATION_LEVEL2, D3DCOMPILE_OPTIMIZATION_LEVEL3 };
    for (int i = 0; i < 4; i++) {
        ID3DBlob *b = NULL; HRESULT hr = d3dh_compile(sh_find("ps_loop")->src, "main", "ps_4_0", flags[i], &b);
        if (FAILED(hr)) { if (!t_is_wine()) CHECKF(0, "O%d: %s", i, d3dh_hr_name(hr)); continue; }
        void *sh = NULL; hr = sh_create(e.dev, ST_PS, SH_BP(b), SH_BN(b), NULL, &sh);
        t_check(hr == S_OK, __FILE__, __LINE__, "optimisation level creates", "O%d: %s", i, d3dh_hr_name(hr)); SH_REL(sh); ID3D10Blob_Release(b);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_stripped_containers_accepted)
{
    REQUIRE(d3dh_D3DStripShader());
    SH_ENV(e, 0xb000);
    static const char *const names[ST_COUNT] = { "vs_matrix", "ps_texture", "gs_so_src", "hs_tri_patchdata", "ds_quad", "cs_shared" };
    static const UINT sf[] = { D3DCOMPILER_STRIP_REFLECTION_DATA, D3DCOMPILER_STRIP_DEBUG_INFO, D3DCOMPILER_STRIP_TEST_BLOBS, D3DCOMPILER_STRIP_PRIVATE_DATA,
                               D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_TEST_BLOBS | D3DCOMPILER_STRIP_PRIVATE_DATA };
    for (int st = 0; st < ST_COUNT; st++) {
        const sh_ent *t = sh_find(names[st]); ID3DBlob *src = NULL;
        HRESULT hr = d3dh_compile(t->src, "main", t->target, D3DCOMPILE_DEBUG, &src);
        if (FAILED(hr)) { if (!t_is_wine()) CHECKF(0, "debug compile %s: %s", names[st], d3dh_hr_name(hr)); continue; }
        for (size_t f = 0; f < sizeof sf / sizeof *sf; f++) {
            ID3DBlob *out = NULL; hr = d3dh_D3DStripShader()(SH_BP(src), SH_BN(src), sf[f], &out);
            if (FAILED(hr)) { t_note("D3DStripShader(%s, 0x%x) -> %s", names[st], sf[f], d3dh_hr_name(hr)); continue; }
            void *sh = NULL; hr = sh_create(e.dev, st, SH_BP(out), SH_BN(out), NULL, &sh);
            t_check(hr == S_OK, __FILE__, __LINE__, "stripped container creates", "%s strip 0x%x: %s", names[st], sf[f], d3dh_hr_name(hr)); SH_REL(sh);
            ID3D10Blob_Release(out);
        }
        ID3D10Blob_Release(src);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_debug_info_containers_accepted_per_stage)
{
    SH_ENV(e, 0xb000);
    static const char *const names[ST_COUNT] = { "vs_matrix", "ps_texture", "gs_so_src", "hs_tri_patchdata", "ds_quad", "cs_shared" };
    for (int st = 0; st < ST_COUNT; st++) {
        const sh_ent *t = sh_find(names[st]); ID3DBlob *src = NULL;
        HRESULT hr = d3dh_compile(t->src, "main", t->target, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, &src);
        if (FAILED(hr)) { if (!t_is_wine()) CHECKF(0, "debug compile %s: %s", names[st], d3dh_hr_name(hr)); continue; }
        void *sh = NULL; hr = sh_create(e.dev, st, SH_BP(src), SH_BN(src), NULL, &sh);
        t_check(hr == S_OK, __FILE__, __LINE__, "debug container creates", "%s: %s", names[st], d3dh_hr_name(hr)); SH_REL(sh);
        ID3D10Blob_Release(src);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_private_data_chunk_accepted)
{
    SetBlobPart_t sbp = (SetBlobPart_t)d3dh_getproc("d3dcompiler_47.dll", "D3DSetBlobPart");
    REQUIRE(sbp);
    SH_ENV(e, 0xb000);
    static const char *const names[ST_COUNT] = { "vs_pass", "ps_const", "gs_tri", "hs_tri_int", "ds_tri", "cs_basic" };
    for (int st = 0; st < ST_COUNT; st++) {
        ID3DBlob *b = sh_blob(names[st]), *out = NULL; if (!b) continue;
        HRESULT hr = sbp(SH_BP(b), SH_BN(b), D3D_BLOB_PRIVATE_DATA, 0, "0123456789abcdef", 16, &out);
        if (FAILED(hr)) { t_note("D3DSetBlobPart -> %s", d3dh_hr_name(hr)); continue; }
        void *sh = NULL; hr = sh_create(e.dev, st, SH_BP(out), SH_BN(out), NULL, &sh);
        t_check(hr == S_OK, __FILE__, __LINE__, "container with private data creates", "%s: %s", names[st], d3dh_hr_name(hr)); SH_REL(sh); ID3D10Blob_Release(out);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_chunk_directory_has_expected_chunks)
{
    int n = 0;
    for (size_t i = 0; i < sizeof hash_names / sizeof *hash_names; i++) {
        ID3DBlob *b = sh_blob(hash_names[i]); if (!b) continue;
        const uint8_t *p = SH_BP(b); DWORD cnt = *(const DWORD *)(p + 28); int has_code = 0, has_isgn = 0;
        for (DWORD c = 0; c < cnt && c < 32; c++) {
            DWORD off = *(const DWORD *)(p + 32 + 4 * c);
            if (off + 8 > SH_BN(b)) { CHECKF(0, "%s chunk %lu offset out of range", hash_names[i], c); continue; }
            if (!memcmp(p + off, "SHDR", 4) || !memcmp(p + off, "SHEX", 4)) has_code = 1;
            if (!memcmp(p + off, "ISGN", 4) || !memcmp(p + off, "ISG1", 4)) has_isgn = 1;
        }
        CHECKF(has_code, "%s has a shader code chunk", hash_names[i]); (void)has_isgn; n++;
    }
    CHECK(n > 0 || t_is_wine());
}
TEST(d3d11_shaders_bytecode_version_token_matches_target)
{
    static const struct { const char *name; DWORD tok; } t[] = {
        { "vs_pass", 0x00010040 }, { "vs_pass_41", 0x00010041 }, { "vs_pass_50", 0x00010050 }, { "ps_const", 0x00000040 }, { "ps_const_50", 0x00000050 },
        { "gs_tri", 0x00020040 }, { "gs_tri_50", 0x00020050 }, { "cs_basic", 0x00050050 }, { "cs_40", 0x00050040 }, { "cs_41", 0x00050041 },
        { "hs_tri_int", 0x00030050 }, { "ds_tri", 0x00040050 },
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        ID3DBlob *b = sh_blob(t[i].name); if (!b) continue;
        const uint8_t *p = SH_BP(b); DWORD cnt = *(const DWORD *)(p + 28); int found = 0;
        for (DWORD c = 0; c < cnt && c < 32; c++) {
            DWORD off = *(const DWORD *)(p + 32 + 4 * c);
            if (!memcmp(p + off, "SHDR", 4) || !memcmp(p + off, "SHEX", 4)) { found = 1; DWORD v = *(const DWORD *)(p + off + 8); t_check(v == t[i].tok, __FILE__, __LINE__, "version token", "%s: 0x%08lx want 0x%08lx", t[i].name, v, t[i].tok); }
        }
        CHECKF(found, "%s code chunk present", t[i].name);
    }
}
TEST(d3d11_shaders_bytecode_version_token_edit_changes_stage)
{
    /* Rewriting the version token of a rehashed vs_4_0 container into a ps_4_0 token must flip which creator accepts it
     * (or reject it: the body is not a valid pixel shader). The vertex creator must no longer accept it. */
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); unsigned char *p = sh_dup(b, 0); void *sh = NULL;
    DWORD cnt = *(DWORD *)(p + 28);
    for (DWORD c = 0; c < cnt; c++) { DWORD off = *(DWORD *)(p + 32 + 4 * c);
        if (!memcmp(p + off, "SHDR", 4) || !memcmp(p + off, "SHEX", 4)) *(DWORD *)(p + off + 8) = 0x00000040; }
    dxbc_rehash(p, SH_BN(b));
    HRESULT hr = sh_create(e.dev, ST_VS, p, SH_BN(b), NULL, &sh);
    t_check(FAILED(hr) && !sh, __FILE__, __LINE__, "vertex creator rejects a retagged container", "%s", d3dh_hr_name(hr)); SH_REL(sh);
    hr = sh_create(e.dev, ST_PS, p, SH_BN(b), NULL, &sh);
    t_note("retagged vs body as ps_4_0 in the pixel creator -> %s", d3dh_hr_name(hr)); SH_REL(sh);
    free(p); sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_unsupported_version_token_rejected)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass");
    static const DWORD toks[] = { 0x00010060, 0x00010030, 0x000100f0, 0x00010000, 0x0001ffff };
    for (int i = 0; i < 5; i++) {
        unsigned char *p = sh_dup(b, 0); void *sh = NULL; DWORD cnt = *(DWORD *)(p + 28);
        for (DWORD c = 0; c < cnt; c++) { DWORD off = *(DWORD *)(p + 32 + 4 * c);
            if (!memcmp(p + off, "SHDR", 4) || !memcmp(p + off, "SHEX", 4)) *(DWORD *)(p + off + 8) = toks[i]; }
        dxbc_rehash(p, SH_BN(b));
        HRESULT hr = sh_create(e.dev, ST_VS, p, SH_BN(b), NULL, &sh);
        t_check(FAILED(hr) && !sh, __FILE__, __LINE__, "unknown shader model rejected", "token 0x%08lx: %s", toks[i], d3dh_hr_name(hr)); SH_REL(sh); free(p);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_hs_ds_cs_blobs_on_wrong_creators_with_rehash)
{
    SH_ENV(e, 0xb000);
    static const char *const names[ST_COUNT] = { "vs_pass", "ps_const", "gs_tri", "hs_tri_int", "ds_tri", "cs_basic" };
    for (int st = 0; st < ST_COUNT; st++) {
        ID3DBlob *b = sh_blob(names[st]); if (!b) continue;
        unsigned char *p = sh_dup(b, 0); dxbc_rehash(p, SH_BN(b));
        for (int o = 0; o < ST_COUNT; o++) { if (o == st) continue; void *sh = NULL;
            HRESULT hr = sh_create(e.dev, o, p, SH_BN(b), NULL, &sh);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "valid hash does not rescue the wrong stage", "%s in %s creator: %s", names[st], sh_stage_name[o], d3dh_hr_name(hr)); SH_REL(sh); }
        free(p);
    }
    sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_trailing_garbage_observed)
{
    SH_ENV(e, 0); SH_BLOB(b, "ps_const"); unsigned char *p = sh_dup(b, 64); void *sh = NULL;
    memset(p + SH_BN(b), 0xcd, 64);
    HRESULT hr = sh_create(e.dev, ST_PS, p, SH_BN(b) + 64, NULL, &sh);
    t_note("BytecodeLength 64 bytes larger than the container's total-size field -> %s", d3dh_hr_name(hr)); SH_REL(sh);
    free(p); sh_env_free(&e);
}
TEST(d3d11_shaders_bytecode_unaligned_pointer_accepted)
{
    SH_ENV(e, 0); SH_BLOB(b, "ps_const"); unsigned char *raw = malloc(SH_BN(b) + 8); void *sh = NULL;
    memcpy(raw + 1, SH_BP(b), SH_BN(b));
    HRESULT hr = sh_create(e.dev, ST_PS, raw + 1, SH_BN(b), NULL, &sh);
    t_note("bytecode at an odd address -> %s", d3dh_hr_name(hr)); CHECK(hr == S_OK); SH_REL(sh); free(raw); sh_env_free(&e);
}
