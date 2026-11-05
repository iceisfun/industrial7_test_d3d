/* Per-stage (VS/HS/DS/GS/PS/CS) shader, constant buffer, SRV and sampler binding tests, generated per stage. */
#include "csh.h"

#define NCB D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT
#define NSRV D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT
#define NSMP D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT

static ID3D11ShaderResourceView *stage_srv(csf *f, ID3D11Texture2D **t)
{
    *t = cs_tex(f, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
    return *t ? cs_srv(f, (ID3D11Resource *)*t) : NULL;
}

#define STAGE_TESTS(XS, SHT) \
TEST(d3d11_context_state_##XS##_shader_roundtrip) { \
    OPEN(); \
    SHT *s = mk_##XS(&f); \
    if (!s) { t_skip(#XS " shader unavailable"); CLOSE(); return; } \
    ULONG base = RC(s); \
    SHT *g = NULL; \
    ID3D11DeviceContext_##XS##SetShader(f.ctx, s, NULL, 0); \
    CHECK(RC(s) > base); \
    ULONG bound = RC(s); \
    ID3D11DeviceContext_##XS##GetShader(f.ctx, &g, NULL, NULL); \
    EXPECT_EQ(g, s); \
    EXPECT_EQ(RC(s), bound + 1); \
    if (g) SHT##_Release(g); \
    UINT n = 0; g = NULL; \
    ID3D11DeviceContext_##XS##GetShader(f.ctx, &g, NULL, &n); \
    EXPECT_EQ(g, s); EXPECT_EQ(n, 0); \
    if (g) SHT##_Release(g); \
    ID3D11DeviceContext_##XS##SetShader(f.ctx, NULL, NULL, 0); \
    g = (void *)1; \
    ID3D11DeviceContext_##XS##GetShader(f.ctx, &g, NULL, NULL); \
    CHECK(g == NULL); \
    EXPECT_EQ(RC(s), base); \
    SAFE_RELEASE(s); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_shader_replace) { \
    OPEN(); \
    SHT *a = mk_##XS(&f), *b = mk_##XS(&f); \
    if (!a || !b) { t_skip(#XS " shader unavailable"); SAFE_RELEASE(a); SAFE_RELEASE(b); CLOSE(); return; } \
    ULONG ba = RC(a); \
    ID3D11DeviceContext_##XS##SetShader(f.ctx, a, NULL, 0); \
    ID3D11DeviceContext_##XS##SetShader(f.ctx, b, NULL, 0); \
    EXPECT_EQ(RC(a), ba); \
    SHT *g = NULL; \
    ID3D11DeviceContext_##XS##GetShader(f.ctx, &g, NULL, NULL); \
    EXPECT_EQ(g, b); \
    if (g) SHT##_Release(g); \
    ID3D11DeviceContext_ClearState(f.ctx); \
    SAFE_RELEASE(a); SAFE_RELEASE(b); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_cb_each_slot) { \
    OPEN(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 256); \
    if (!b) { t_skip("buf"); CLOSE(); return; } \
    for (UINT i = 0; i < NCB; i++) { \
        ID3D11Buffer *g[NCB]; memset(g, 0x55, sizeof g); \
        ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, i, 1, &b); \
        ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 0, NCB, g); \
        for (UINT k = 0; k < NCB; k++) { \
            ID3D11Buffer *want = k == i ? b : NULL; \
            CHECKF(g[k] == want, #XS " cb: set slot %u, slot %u reads %p", i, k, g[k]); \
            SAFE_RELEASE(g[k]); \
        } \
        ID3D11Buffer *n = NULL; \
        ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, i, 1, &n); \
    } \
    SAFE_RELEASE(b); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_cb_full_range) { \
    OPEN(); \
    ID3D11Buffer *b[NCB]; int ok = 1; \
    for (UINT i = 0; i < NCB; i++) { b[i] = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 16 * (i + 1)); if (!b[i]) ok = 0; } \
    if (ok) { \
        ID3D11Buffer *g[NCB]; \
        ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 0, NCB, b); \
        ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 0, NCB, g); \
        for (UINT i = 0; i < NCB; i++) { CHECKF(g[i] == b[i], "slot %u", i); SAFE_RELEASE(g[i]); } \
        /* start in the middle, shorter range */ \
        ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 5, 3, g); \
        for (UINT i = 0; i < 3; i++) { CHECKF(g[i] == b[5 + i], "mid slot %u", 5 + i); SAFE_RELEASE(g[i]); } \
        ID3D11DeviceContext_ClearState(f.ctx); \
    } else t_skip("buf"); \
    for (UINT i = 0; i < NCB; i++) SAFE_RELEASE(b[i]); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_cb_null_clears_one_slot) { \
    OPEN(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64); \
    if (!b) { t_skip("buf"); CLOSE(); return; } \
    ID3D11Buffer *set[3] = { b, b, b }, *nul[1] = { NULL }, *g[3] = { 0 }; \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 2, 3, set); \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 3, 1, nul); \
    ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 2, 3, g); \
    EXPECT_EQ(g[0], b); CHECK(g[1] == NULL); EXPECT_EQ(g[2], b); \
    SAFE_RELEASE(g[0]); SAFE_RELEASE(g[2]); \
    ID3D11DeviceContext_ClearState(f.ctx); \
    SAFE_RELEASE(b); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_cb_refcount) { \
    OPEN(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64); \
    if (!b) { t_skip("buf"); CLOSE(); return; } \
    ULONG base = RC(b); \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 0, 1, &b); \
    ULONG one = RC(b); CHECK(one > base); \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 0, 1, &b); \
    EXPECT_EQ(RC(b), one); \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 1, 1, &b); \
    EXPECT_EQ(RC(b), one + 1); \
    ID3D11Buffer *g = NULL; \
    ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 1, 1, &g); \
    EXPECT_EQ(RC(b), one + 2); SAFE_RELEASE(g); \
    ID3D11DeviceContext_ClearState(f.ctx); \
    EXPECT_EQ(RC(b), base); \
    SAFE_RELEASE(b); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_srv_each_slot) { \
    OPEN(); \
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *v = stage_srv(&f, &t); \
    if (!v) { t_skip("srv"); SAFE_RELEASE(t); CLOSE(); return; } \
    static const UINT slots[] = { 0, 1, 2, 7, 15, 16, 31, 63, 64, 100, 126, 127 }; \
    for (unsigned i = 0; i < sizeof slots / sizeof slots[0]; i++) { \
        ID3D11ShaderResourceView *g = NULL, *n = NULL; \
        ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, slots[i], 1, &v); \
        ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, slots[i], 1, &g); \
        CHECKF(g == v, #XS " srv slot %u", slots[i]); SAFE_RELEASE(g); \
        ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, slots[i], 1, &n); \
        ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, slots[i], 1, &g); \
        CHECKF(g == NULL, #XS " srv slot %u not cleared", slots[i]); \
    } \
    SAFE_RELEASE(v); SAFE_RELEASE(t); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_srv_range) { \
    OPEN(); \
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *v = stage_srv(&f, &t); \
    if (!v) { t_skip("srv"); SAFE_RELEASE(t); CLOSE(); return; } \
    ID3D11ShaderResourceView *all[NSRV], *g[NSRV]; \
    for (UINT i = 0; i < NSRV; i++) all[i] = v; \
    ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, 0, NSRV, all); \
    ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, 0, NSRV, g); \
    for (UINT i = 0; i < NSRV; i++) { CHECKF(g[i] == v, "srv slot %u", i); SAFE_RELEASE(g[i]); } \
    ID3D11DeviceContext_ClearState(f.ctx); \
    ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, 0, NSRV, g); \
    for (UINT i = 0; i < NSRV; i++) { CHECKF(g[i] == NULL, "srv slot %u after ClearState", i); SAFE_RELEASE(g[i]); } \
    SAFE_RELEASE(v); SAFE_RELEASE(t); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_srv_refcount) { \
    OPEN(); \
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *v = stage_srv(&f, &t); \
    if (!v) { t_skip("srv"); SAFE_RELEASE(t); CLOSE(); return; } \
    ULONG base = RC(v); \
    ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, 3, 1, &v); \
    ULONG one = RC(v); CHECK(one > base); \
    ID3D11ShaderResourceView *g = NULL; \
    ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, 3, 1, &g); \
    EXPECT_EQ(RC(v), one + 1); SAFE_RELEASE(g); \
    ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, 3, 0, NULL); \
    EXPECT_EQ(RC(v), one); \
    ID3D11DeviceContext_ClearState(f.ctx); \
    EXPECT_EQ(RC(v), base); \
    SAFE_RELEASE(v); SAFE_RELEASE(t); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_sampler_each_slot) { \
    OPEN(); \
    ID3D11SamplerState *s = cs_sampler(&f, D3D11_FILTER_MIN_MAG_MIP_POINT); \
    if (!s) { t_skip("sampler"); CLOSE(); return; } \
    for (UINT i = 0; i < NSMP; i++) { \
        ID3D11SamplerState *g[NSMP]; memset(g, 0x55, sizeof g); \
        ID3D11DeviceContext_##XS##SetSamplers(f.ctx, i, 1, &s); \
        ID3D11DeviceContext_##XS##GetSamplers(f.ctx, 0, NSMP, g); \
        for (UINT k = 0; k < NSMP; k++) { \
            ID3D11SamplerState *want = k == i ? s : NULL; \
            CHECKF(g[k] == want, #XS " sampler: set %u, slot %u reads %p", i, k, g[k]); \
            SAFE_RELEASE(g[k]); \
        } \
        ID3D11SamplerState *n = NULL; \
        ID3D11DeviceContext_##XS##SetSamplers(f.ctx, i, 1, &n); \
    } \
    SAFE_RELEASE(s); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_sampler_range) { \
    OPEN(); \
    ID3D11SamplerState *a = cs_sampler(&f, D3D11_FILTER_MIN_MAG_MIP_POINT), *b = cs_sampler(&f, D3D11_FILTER_MIN_MAG_MIP_LINEAR); \
    if (!a || !b) { t_skip("sampler"); SAFE_RELEASE(a); SAFE_RELEASE(b); CLOSE(); return; } \
    ID3D11SamplerState *all[NSMP], *g[NSMP]; \
    for (UINT i = 0; i < NSMP; i++) all[i] = (i & 1) ? b : a; \
    ID3D11DeviceContext_##XS##SetSamplers(f.ctx, 0, NSMP, all); \
    ID3D11DeviceContext_##XS##GetSamplers(f.ctx, 0, NSMP, g); \
    for (UINT i = 0; i < NSMP; i++) { CHECKF(g[i] == all[i], "sampler slot %u", i); SAFE_RELEASE(g[i]); } \
    ID3D11DeviceContext_ClearState(f.ctx); \
    ID3D11DeviceContext_##XS##GetSamplers(f.ctx, 0, NSMP, g); \
    for (UINT i = 0; i < NSMP; i++) { CHECKF(g[i] == NULL, "sampler slot %u after ClearState", i); SAFE_RELEASE(g[i]); } \
    SAFE_RELEASE(a); SAFE_RELEASE(b); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_zero_count_noop) { \
    OPEN(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64); \
    ID3D11SamplerState *s = cs_sampler(&f, D3D11_FILTER_MIN_MAG_MIP_POINT); \
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *v = stage_srv(&f, &t); \
    if (!b || !s || !v) { t_skip("objects"); goto out; } \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 0, 1, &b); \
    ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, 0, 1, &v); \
    ID3D11DeviceContext_##XS##SetSamplers(f.ctx, 0, 1, &s); \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 0, 0, NULL); \
    ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, 0, 0, NULL); \
    ID3D11DeviceContext_##XS##SetSamplers(f.ctx, 0, 0, NULL); \
    ID3D11Buffer *gb = NULL; ID3D11ShaderResourceView *gv = NULL; ID3D11SamplerState *gs = NULL; \
    ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 0, 1, &gb); \
    ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, 0, 1, &gv); \
    ID3D11DeviceContext_##XS##GetSamplers(f.ctx, 0, 1, &gs); \
    EXPECT_EQ(gb, b); EXPECT_EQ(gv, v); EXPECT_EQ(gs, s); \
    SAFE_RELEASE(gb); SAFE_RELEASE(gv); SAFE_RELEASE(gs); \
    ID3D11Buffer *sb = (ID3D11Buffer *)(ULONG_PTR)0x1234; ID3D11ShaderResourceView *sv = (void *)(ULONG_PTR)0x1234; ID3D11SamplerState *ss = (void *)(ULONG_PTR)0x1234; \
    ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 0, 0, &sb); \
    ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, 0, 0, &sv); \
    ID3D11DeviceContext_##XS##GetSamplers(f.ctx, 0, 0, &ss); \
    EXPECT_EQ((ULONG_PTR)sb, 0x1234); EXPECT_EQ((ULONG_PTR)sv, 0x1234); EXPECT_EQ((ULONG_PTR)ss, 0x1234); \
    ID3D11DeviceContext_ClearState(f.ctx); \
out: \
    SAFE_RELEASE(b); SAFE_RELEASE(s); SAFE_RELEASE(v); SAFE_RELEASE(t); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_clearstate_resets_stage) { \
    OPEN(); \
    SHT *sh = mk_##XS(&f); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64); \
    ID3D11SamplerState *s = cs_sampler(&f, D3D11_FILTER_MIN_MAG_MIP_POINT); \
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *v = stage_srv(&f, &t); \
    if (!sh || !b || !s || !v) { t_skip("objects"); goto out; } \
    ID3D11DeviceContext_##XS##SetShader(f.ctx, sh, NULL, 0); \
    ID3D11Buffer *bb[NCB]; ID3D11ShaderResourceView *vv[NSRV]; ID3D11SamplerState *ss[NSMP]; \
    for (UINT i = 0; i < NCB; i++) bb[i] = b; \
    for (UINT i = 0; i < NSRV; i++) vv[i] = v; \
    for (UINT i = 0; i < NSMP; i++) ss[i] = s; \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, 0, NCB, bb); \
    ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, 0, NSRV, vv); \
    ID3D11DeviceContext_##XS##SetSamplers(f.ctx, 0, NSMP, ss); \
    ID3D11DeviceContext_ClearState(f.ctx); \
    SHT *gsh = (void *)1; \
    ID3D11DeviceContext_##XS##GetShader(f.ctx, &gsh, NULL, NULL); \
    CHECK(gsh == NULL); \
    ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 0, NCB, bb); \
    ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, 0, NSRV, vv); \
    ID3D11DeviceContext_##XS##GetSamplers(f.ctx, 0, NSMP, ss); \
    for (UINT i = 0; i < NCB; i++) CHECKF(bb[i] == NULL, "cb %u", i); \
    for (UINT i = 0; i < NSRV; i++) CHECKF(vv[i] == NULL, "srv %u", i); \
    for (UINT i = 0; i < NSMP; i++) CHECKF(ss[i] == NULL, "sampler %u", i); \
out: \
    SAFE_RELEASE(sh); SAFE_RELEASE(b); SAFE_RELEASE(s); SAFE_RELEASE(v); SAFE_RELEASE(t); \
    CLOSE(); \
} \
TEST(d3d11_context_state_##XS##_slot_limit_oob_set) { \
    OPEN(); \
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64); \
    ID3D11SamplerState *s = cs_sampler(&f, D3D11_FILTER_MIN_MAG_MIP_POINT); \
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *v = stage_srv(&f, &t); \
    if (!b || !s || !v) { t_skip("objects"); goto out; } \
    ID3D11Buffer *two[2] = { b, b }; ID3D11SamplerState *twos[2] = { s, s }; ID3D11ShaderResourceView *twov[2] = { v, v }; \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, NCB, 1, &b); \
    ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, NSRV, 1, &v); \
    ID3D11DeviceContext_##XS##SetSamplers(f.ctx, NSMP, 1, &s); \
    { ID3D11Buffer *g[NCB]; ID3D11ShaderResourceView *gv[NSRV]; ID3D11SamplerState *gs[NSMP]; \
      ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, 0, NCB, g); \
      ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, 0, NSRV, gv); \
      ID3D11DeviceContext_##XS##GetSamplers(f.ctx, 0, NSMP, gs); \
      for (UINT i = 0; i < NCB; i++) { CHECKF(g[i] == NULL, "slot %u bound by out-of-range cb set", i); SAFE_RELEASE(g[i]); } \
      for (UINT i = 0; i < NSRV; i++) { CHECKF(gv[i] == NULL, "slot %u bound by out-of-range srv set", i); SAFE_RELEASE(gv[i]); } \
      for (UINT i = 0; i < NSMP; i++) { CHECKF(gs[i] == NULL, "slot %u bound by out-of-range sampler set", i); SAFE_RELEASE(gs[i]); } } \
    /* ranges that straddle the limit: only observed */ \
    ID3D11DeviceContext_##XS##SetConstantBuffers(f.ctx, NCB - 1, 2, two); \
    ID3D11DeviceContext_##XS##SetSamplers(f.ctx, NSMP - 1, 2, twos); \
    ID3D11DeviceContext_##XS##SetShaderResources(f.ctx, NSRV - 1, 2, twov); \
    { ID3D11Buffer *g = NULL; ID3D11SamplerState *gs = NULL; ID3D11ShaderResourceView *gv = NULL; \
      ID3D11DeviceContext_##XS##GetConstantBuffers(f.ctx, NCB - 1, 1, &g); \
      ID3D11DeviceContext_##XS##GetSamplers(f.ctx, NSMP - 1, 1, &gs); \
      ID3D11DeviceContext_##XS##GetShaderResources(f.ctx, NSRV - 1, 1, &gv); \
      t_note(#XS " straddling Set: last cb=%p last sampler=%p last srv=%p", g, gs, gv); \
      SAFE_RELEASE(g); SAFE_RELEASE(gs); SAFE_RELEASE(gv); } \
    ID3D11DeviceContext_ClearState(f.ctx); \
out: \
    SAFE_RELEASE(b); SAFE_RELEASE(s); SAFE_RELEASE(v); SAFE_RELEASE(t); \
    CLOSE(); \
}

STAGE_TESTS(VS, ID3D11VertexShader)
STAGE_TESTS(PS, ID3D11PixelShader)
STAGE_TESTS(GS, ID3D11GeometryShader)
STAGE_TESTS(HS, ID3D11HullShader)
STAGE_TESTS(DS, ID3D11DomainShader)
STAGE_TESTS(CS, ID3D11ComputeShader)

/* ---- cross-stage independence ---------------------------------------------- */
TEST(d3d11_context_state_stage_independence_cb) {
    OPEN();
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_CONSTANT_BUFFER, 64);
    if (!b) { t_skip("buf"); CLOSE(); return; }
    ID3D11DeviceContext_VSSetConstantBuffers(f.ctx, 4, 1, &b);
    ID3D11Buffer *g[6] = { 0 };
    ID3D11DeviceContext_PSGetConstantBuffers(f.ctx, 4, 1, &g[0]);
    ID3D11DeviceContext_GSGetConstantBuffers(f.ctx, 4, 1, &g[1]);
    ID3D11DeviceContext_HSGetConstantBuffers(f.ctx, 4, 1, &g[2]);
    ID3D11DeviceContext_DSGetConstantBuffers(f.ctx, 4, 1, &g[3]);
    ID3D11DeviceContext_CSGetConstantBuffers(f.ctx, 4, 1, &g[4]);
    ID3D11DeviceContext_VSGetConstantBuffers(f.ctx, 4, 1, &g[5]);
    for (int i = 0; i < 5; i++) CHECKF(g[i] == NULL, "stage %d leaked VS binding", i);
    EXPECT_EQ(g[5], b);
    for (int i = 0; i < 6; i++) SAFE_RELEASE(g[i]);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_stage_independence_srv) {
    OPEN();
    ID3D11Texture2D *t = NULL; ID3D11ShaderResourceView *v = stage_srv(&f, &t);
    if (!v) { t_skip("srv"); SAFE_RELEASE(t); CLOSE(); return; }
    ID3D11DeviceContext_PSSetShaderResources(f.ctx, 9, 1, &v);
    ID3D11ShaderResourceView *g[6] = { 0 };
    ID3D11DeviceContext_VSGetShaderResources(f.ctx, 9, 1, &g[0]);
    ID3D11DeviceContext_GSGetShaderResources(f.ctx, 9, 1, &g[1]);
    ID3D11DeviceContext_HSGetShaderResources(f.ctx, 9, 1, &g[2]);
    ID3D11DeviceContext_DSGetShaderResources(f.ctx, 9, 1, &g[3]);
    ID3D11DeviceContext_CSGetShaderResources(f.ctx, 9, 1, &g[4]);
    ID3D11DeviceContext_PSGetShaderResources(f.ctx, 9, 1, &g[5]);
    for (int i = 0; i < 5; i++) CHECKF(g[i] == NULL, "stage %d leaked PS binding", i);
    EXPECT_EQ(g[5], v);
    for (int i = 0; i < 6; i++) SAFE_RELEASE(g[i]);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(v); SAFE_RELEASE(t);
    CLOSE();
}
TEST(d3d11_context_state_stage_independence_shaders) {
    OPEN();
    ID3D11VertexShader *vs = mk_VS(&f);
    ID3D11PixelShader *ps = mk_PS(&f);
    ID3D11ComputeShader *cs = mk_CS(&f);
    if (!vs || !ps || !cs) { t_skip("shaders"); goto out; }
    ID3D11DeviceContext_VSSetShader(f.ctx, vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(f.ctx, ps, NULL, 0);
    ID3D11DeviceContext_CSSetShader(f.ctx, cs, NULL, 0);
    ID3D11GeometryShader *gs = (void *)1; ID3D11HullShader *hs = (void *)1; ID3D11DomainShader *ds = (void *)1;
    ID3D11DeviceContext_GSGetShader(f.ctx, &gs, NULL, NULL);
    ID3D11DeviceContext_HSGetShader(f.ctx, &hs, NULL, NULL);
    ID3D11DeviceContext_DSGetShader(f.ctx, &ds, NULL, NULL);
    CHECK(gs == NULL); CHECK(hs == NULL); CHECK(ds == NULL);
    ID3D11DeviceContext_VSSetShader(f.ctx, NULL, NULL, 0);
    ID3D11PixelShader *gp = NULL; ID3D11ComputeShader *gc = NULL;
    ID3D11DeviceContext_PSGetShader(f.ctx, &gp, NULL, NULL);
    ID3D11DeviceContext_CSGetShader(f.ctx, &gc, NULL, NULL);
    EXPECT_EQ(gp, ps); EXPECT_EQ(gc, cs);
    SAFE_RELEASE(gp); SAFE_RELEASE(gc);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(vs); SAFE_RELEASE(ps); SAFE_RELEASE(cs);
    CLOSE();
}

/* ---- compute UAVs ------------------------------------------------------------ */
static ID3D11UnorderedAccessView *mk_buf_uav(csf *f, ID3D11Buffer **bo)
{
    *bo = cs_buf(f, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, 256);
    if (!*bo) return NULL;
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
    ud.Format = DXGI_FORMAT_R32_UINT; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 64;
    ID3D11UnorderedAccessView *u = NULL;
    HRESULT hr = ID3D11Device_CreateUnorderedAccessView(f->dev, (ID3D11Resource *)*bo, &ud, &u);
    if (FAILED(hr)) { t_note("CreateUAV: %s", d3dh_hr_name(hr)); SAFE_RELEASE(*bo); return NULL; }
    return u;
}
TEST(d3d11_context_state_cs_uav_each_slot) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Buffer *b; ID3D11UnorderedAccessView *u = mk_buf_uav(&f, &b);
    if (!u) { t_skip("uav"); CLOSE(); return; }
    for (UINT i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++) {
        ID3D11UnorderedAccessView *g[D3D11_PS_CS_UAV_REGISTER_COUNT]; UINT init = 0xffffffffu;
        memset(g, 0x55, sizeof g);
        ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, i, 1, &u, &init);
        ID3D11DeviceContext_CSGetUnorderedAccessViews(f.ctx, 0, D3D11_PS_CS_UAV_REGISTER_COUNT, g);
        for (UINT k = 0; k < D3D11_PS_CS_UAV_REGISTER_COUNT; k++) {
            ID3D11UnorderedAccessView *want = k == i ? u : NULL;
            CHECKF(g[k] == want, "set %u slot %u: %p", i, k, g[k]);
            SAFE_RELEASE(g[k]);
        }
        ID3D11UnorderedAccessView *n = NULL;
        ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, i, 1, &n, &init);
    }
    SAFE_RELEASE(u); SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_cs_uav_full_range_and_clear) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Buffer *b; ID3D11UnorderedAccessView *u = mk_buf_uav(&f, &b);
    if (!u) { t_skip("uav"); CLOSE(); return; }
    ID3D11UnorderedAccessView *all[8], *g[8]; UINT init[8];
    for (int i = 0; i < 8; i++) { all[i] = u; init[i] = 0xffffffffu; }
    ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, 0, 8, all, init);
    ID3D11DeviceContext_CSGetUnorderedAccessViews(f.ctx, 0, 8, g);
    for (int i = 0; i < 8; i++) { CHECKF(g[i] == u, "slot %d", i); SAFE_RELEASE(g[i]); }
    ID3D11DeviceContext_ClearState(f.ctx);
    ID3D11DeviceContext_CSGetUnorderedAccessViews(f.ctx, 0, 8, g);
    for (int i = 0; i < 8; i++) { CHECKF(g[i] == NULL, "slot %d after ClearState", i); SAFE_RELEASE(g[i]); }
    SAFE_RELEASE(u); SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_cs_uav_refcount) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Buffer *b; ID3D11UnorderedAccessView *u = mk_buf_uav(&f, &b);
    if (!u) { t_skip("uav"); CLOSE(); return; }
    ULONG base = RC(u); UINT init = 0xffffffffu;
    ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, 0, 1, &u, &init);
    CHECK(RC(u) > base);
    ID3D11DeviceContext_ClearState(f.ctx);
    EXPECT_EQ(RC(u), base);
    SAFE_RELEASE(u); SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_cs_uav_zero_count) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Buffer *b; ID3D11UnorderedAccessView *u = mk_buf_uav(&f, &b);
    if (!u) { t_skip("uav"); CLOSE(); return; }
    UINT init = 0xffffffffu;
    ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, 0, 1, &u, &init);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, 0, 0, NULL, NULL);
    ID3D11UnorderedAccessView *g = NULL;
    ID3D11DeviceContext_CSGetUnorderedAccessViews(f.ctx, 0, 1, &g);
    EXPECT_EQ(g, u); SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(u); SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_cs_uav_unbinds_cs_srv) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Buffer *b; ID3D11UnorderedAccessView *u = mk_buf_uav(&f, &b);
    ID3D11ShaderResourceView *srv = NULL;
    if (u) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd; memset(&sd, 0, sizeof sd);
        sd.Format = DXGI_FORMAT_R32_UINT; sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; sd.Buffer.NumElements = 64;
        ID3D11Device_CreateShaderResourceView(f.dev, (ID3D11Resource *)b, &sd, &srv);
    }
    if (!u || !srv) { t_skip("views"); goto out; }
    ID3D11DeviceContext_CSSetShaderResources(f.ctx, 0, 1, &srv);
    UINT init = 0xffffffffu;
    ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, 0, 1, &u, &init);
    ID3D11ShaderResourceView *g = (void *)1;
    ID3D11DeviceContext_CSGetShaderResources(f.ctx, 0, 1, &g);
    CHECKF(g == NULL, "SRV of a buffer bound as UAV must be unbound (got %p)", g);
    SAFE_RELEASE(g);
    /* and the other way round */
    ID3D11DeviceContext_CSSetShaderResources(f.ctx, 0, 1, &srv);
    ID3D11UnorderedAccessView *gu = NULL;
    ID3D11DeviceContext_CSGetUnorderedAccessViews(f.ctx, 0, 1, &gu);
    ID3D11ShaderResourceView *g2 = NULL;
    ID3D11DeviceContext_CSGetShaderResources(f.ctx, 0, 1, &g2);
    t_note("after SRV set on bound UAV resource: UAV=%p SRV=%p", gu, g2);
    CHECKF(!(gu && g2), "resource bound as both UAV and SRV in CS");
    SAFE_RELEASE(gu); SAFE_RELEASE(g2);
    ID3D11DeviceContext_ClearState(f.ctx);
out:
    SAFE_RELEASE(srv); SAFE_RELEASE(u); SAFE_RELEASE(b);
    CLOSE();
}
TEST(d3d11_context_state_cs_uav_initial_counts_accepted) {
    OPEN();
    NEED_FL(D3D_FEATURE_LEVEL_11_0);
    ID3D11Buffer *b = cs_buf(&f, D3D11_BIND_UNORDERED_ACCESS, 256);
    ID3D11UnorderedAccessView *u = NULL;
    if (b) {
        /* append/consume style structured buffers need MISC flag; use a raw-free typed view and just verify Set/Get with explicit counts */
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud);
        ud.Format = DXGI_FORMAT_R32_UINT; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 64;
        ID3D11Device_CreateUnorderedAccessView(f.dev, (ID3D11Resource *)b, &ud, &u);
    }
    if (!u) { t_skip("uav"); SAFE_RELEASE(b); CLOSE(); return; }
    UINT counts[] = { 0, 5, 0xffffffffu };
    for (int i = 0; i < 3; i++) {
        ID3D11UnorderedAccessView *g = NULL;
        ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, 0, 1, &u, &counts[i]);
        ID3D11DeviceContext_CSGetUnorderedAccessViews(f.ctx, 0, 1, &g);
        CHECKF(g == u, "initial count %u", counts[i]); SAFE_RELEASE(g);
    }
    ID3D11DeviceContext_CSSetUnorderedAccessViews(f.ctx, 0, 1, &u, NULL);
    ID3D11UnorderedAccessView *g = NULL;
    ID3D11DeviceContext_CSGetUnorderedAccessViews(f.ctx, 0, 1, &g);
    t_note("CSSetUnorderedAccessViews with NULL initial counts: slot=%p", g);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_ClearState(f.ctx);
    SAFE_RELEASE(u); SAFE_RELEASE(b);
    CLOSE();
}
