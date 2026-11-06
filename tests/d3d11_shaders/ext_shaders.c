/* Stream-output statistics, indirect dispatch and data-carrying class instances (dynamic shader linking), measured on Win10 22H2 WARP. */
#include "sh_common.h"
#include <d3d11shader.h>

typedef D3D11_SO_DECLARATION_ENTRY SOE;

/* ---- stream-output statistics --------------------------------------------------------------------------------------- */
typedef struct { ID3D11Buffer *vbuf, *sob; ID3D11InputLayout *il; ID3D11VertexShader *vs; ID3D11GeometryShader *so; } so_rig;
static int so_rig_make(SHENV *e, so_rig *r, UINT sob_bytes)
{
    ID3DBlob *vb = sh_blob("vs_so_src"), *gb = sh_blob("gs_so_src");
    SOE d[1] = { { 0, "SV_Position", 0, 0, 4, 0 } }; UINT stride = 16;
    D3D11_INPUT_ELEMENT_DESC el = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    float verts[12] = { 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1 };
    D3D11_BUFFER_DESC bd = { sizeof verts, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA sd = { verts, 0, 0 };
    D3D11_BUFFER_DESC od = { sob_bytes, D3D11_USAGE_DEFAULT, D3D11_BIND_STREAM_OUTPUT, 0, 0, 0 };
    memset(r, 0, sizeof *r);
    if (!vb || !gb) return 0;
    if (FAILED(ID3D11Device_CreateGeometryShaderWithStreamOutput(e->dev, SH_BP(gb), SH_BN(gb), d, 1, &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, NULL, &r->so))) { CHECK(0); return 0; }
    EXPECT_HR(ID3D11Device_CreateVertexShader(e->dev, SH_BP(vb), SH_BN(vb), NULL, &r->vs), S_OK);
    EXPECT_HR(ID3D11Device_CreateInputLayout(e->dev, &el, 1, SH_BP(vb), SH_BN(vb), &r->il), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(e->dev, &bd, &sd, &r->vbuf), S_OK);
    EXPECT_HR(ID3D11Device_CreateBuffer(e->dev, &od, NULL, &r->sob), S_OK);
    return r->vs && r->il && r->vbuf && r->sob;
}
static void so_rig_free(SHENV *e, so_rig *r) { SH_REL(r->vbuf); SH_REL(r->sob); SH_REL(r->il); SH_REL(r->vs); SH_REL(r->so); (void)e; }
static void so_rig_bind(SHENV *e, so_rig *r)
{
    UINT vstride = 16, voff = 0, soff = 0;
    ID3D11DeviceContext_IASetInputLayout(e->ctx, r->il);
    ID3D11DeviceContext_IASetVertexBuffers(e->ctx, 0, 1, &r->vbuf, &vstride, &voff);
    ID3D11DeviceContext_IASetPrimitiveTopology(e->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(e->ctx, r->vs, NULL, 0);
    ID3D11DeviceContext_GSSetShader(e->ctx, r->so, NULL, 0);
    ID3D11DeviceContext_SOSetTargets(e->ctx, 1, &r->sob, &soff);
}
static void so_rig_unbind(SHENV *e)
{
    ID3D11Buffer *none = NULL; UINT soff = 0;
    ID3D11DeviceContext_SOSetTargets(e->ctx, 1, &none, &soff);
    ID3D11DeviceContext_GSSetShader(e->ctx, NULL, NULL, 0);
}
static HRESULT so_query(SHENV *e, D3D11_QUERY q, void *out, UINT sz, so_rig *r, int draw)
{
    ID3D11Query *qq = NULL; D3D11_QUERY_DESC qd = { q, 0 }; HRESULT hr = ID3D11Device_CreateQuery(e->dev, &qd, &qq);
    if (FAILED(hr)) return hr;
    ID3D11DeviceContext_Begin(e->ctx, (ID3D11Asynchronous *)qq);
    if (draw) { so_rig_bind(e, r); ID3D11DeviceContext_Draw(e->ctx, 3, 0); so_rig_unbind(e); }
    ID3D11DeviceContext_End(e->ctx, (ID3D11Asynchronous *)qq);
    for (int i = 0; i < 2000; i++) { hr = ID3D11DeviceContext_GetData(e->ctx, (ID3D11Asynchronous *)qq, out, sz, 0); if (hr != S_FALSE) break; Sleep(1); }
    ID3D11Query_Release(qq);
    return hr;
}
TEST(d3d11_shaders_so_statistics_one_triangle)
{
    SH_ENV(e, 0xb000); so_rig r; D3D11_QUERY_DATA_SO_STATISTICS st; memset(&st, 0xcc, sizeof st);
    if (!so_rig_make(&e, &r, 3 * 16)) { sh_env_free(&e); return; }
    EXPECT_HR(so_query(&e, D3D11_QUERY_SO_STATISTICS, &st, sizeof st, &r, 1), S_OK);
    EXPECT_EQ(st.NumPrimitivesWritten, 1); EXPECT_EQ(st.PrimitivesStorageNeeded, 1);
    so_rig_free(&e, &r); sh_env_free(&e);
}
TEST(d3d11_shaders_so_statistics_overflow)
{
    SH_ENV(e, 0xb000); so_rig r; D3D11_QUERY_DATA_SO_STATISTICS st; memset(&st, 0xcc, sizeof st);
    if (!so_rig_make(&e, &r, 2 * 16)) { sh_env_free(&e); return; }   /* room for two vertices: the triangle does not fit */
    EXPECT_HR(so_query(&e, D3D11_QUERY_SO_STATISTICS, &st, sizeof st, &r, 1), S_OK);
    EXPECT_EQ(st.NumPrimitivesWritten, 0); EXPECT_EQ(st.PrimitivesStorageNeeded, 1);
    so_rig_free(&e, &r); sh_env_free(&e);
}
TEST(d3d11_shaders_so_overflow_predicate)
{
    SH_ENV(e, 0xb000); so_rig r; BOOL ov = 0xcc, ok = 0xcc;
    if (!so_rig_make(&e, &r, 3 * 16)) { sh_env_free(&e); return; }
    EXPECT_HR(so_query(&e, D3D11_QUERY_SO_OVERFLOW_PREDICATE, &ov, sizeof ov, &r, 1), S_OK);
    EXPECT_EQ(ov, FALSE);
    so_rig_free(&e, &r);
    if (!so_rig_make(&e, &r, 2 * 16)) { sh_env_free(&e); return; }
    EXPECT_HR(so_query(&e, D3D11_QUERY_SO_OVERFLOW_PREDICATE, &ok, sizeof ok, &r, 1), S_OK);
    EXPECT_EQ(ok, TRUE);
    so_rig_free(&e, &r); sh_env_free(&e);
}
TEST(d3d11_shaders_so_captured_triangle_vertices)
{
    SH_ENV(e, 0xb000); so_rig r; void *data = NULL; size_t sz = 0;
    if (!so_rig_make(&e, &r, 3 * 16)) { sh_env_free(&e); return; }
    so_rig_bind(&e, &r); ID3D11DeviceContext_Draw(e.ctx, 3, 0); so_rig_unbind(&e);
    if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, r.sob, &data, &sz))) {
        static const float want[12] = { 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1 };
        EXPECT_EQ(sz, 48);
        CHECK(sz >= 48 && !memcmp(data, want, 48));
        free(data);
    } else CHECK(0);
    so_rig_free(&e, &r); sh_env_free(&e);
}
/* A second draw appends or overwrites depending on the offset passed to SOSetTargets; offset -1 appends after the previous draw. */
TEST(d3d11_shaders_so_append_offset_minus_one)
{
    SH_ENV(e, 0xb000); so_rig r; void *data = NULL; size_t sz = 0; UINT app = 0xffffffffu;
    if (!so_rig_make(&e, &r, 6 * 16)) { sh_env_free(&e); return; }
    so_rig_bind(&e, &r); ID3D11DeviceContext_Draw(e.ctx, 3, 0);
    ID3D11DeviceContext_SOSetTargets(e.ctx, 1, &r.sob, &app);
    ID3D11DeviceContext_Draw(e.ctx, 3, 0); so_rig_unbind(&e);
    if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, r.sob, &data, &sz))) {
        static const float want[12] = { 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1 };
        CHECK(sz >= 96 && !memcmp(data, want, 48) && !memcmp((char *)data + 48, want, 48));
        free(data);
    } else CHECK(0);
    so_rig_free(&e, &r); sh_env_free(&e);
}

/* ---- DispatchIndirect -------------------------------------------------------------------------------------------------- */
static const char *k_cs_groups =
    "RWByteAddressBuffer o : register(u0);\n"
    "[numthreads(1, 1, 1)] void main(uint3 g : SV_GroupID, uint3 gc : SV_GroupThreadID) { o.Store((g.y * 2 + g.x) * 4, 1 + g.x + (g.y + 1) * 16 + g.z * 256); }\n";
static void indirect_case(UINT args_off, UINT gx, UINT gy, UINT gz, UINT expect_written)
{
    SH_ENV(e, 0xb000); ID3DBlob *b = NULL; ID3D11ComputeShader *cs = NULL; ID3D11Buffer *args = NULL, *out = NULL; ID3D11UnorderedAccessView *uav = NULL;
    UINT a[8] = { 0xdead, 0xbeef, 0xdead, gx, gy, gz, 0xdead, 0xbeef };
    if (FAILED(d3dh_compile(k_cs_groups, "main", "cs_5_0", 0, &b))) { CHECKF(0, "compile: %s", d3dh_compile_errors()); sh_env_free(&e); return; }
    EXPECT_HR(ID3D11Device_CreateComputeShader(e.dev, SH_BP(b), SH_BN(b), NULL, &cs), S_OK);
    D3D11_BUFFER_DESC ad = { sizeof a, D3D11_USAGE_DEFAULT, 0, 0, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS, 0 }; D3D11_SUBRESOURCE_DATA sd = { a, 0, 0 };
    EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &ad, &sd, &args), S_OK);
    D3D11_BUFFER_DESC od = { 64, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS, 0, D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0 };
    EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &od, NULL, &out), S_OK);
    if (out) { D3D11_UNORDERED_ACCESS_VIEW_DESC ud; memset(&ud, 0, sizeof ud); ud.Format = DXGI_FORMAT_R32_TYPELESS; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 16; ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(e.dev, (ID3D11Resource *)out, &ud, &uav), S_OK); }
    if (cs && args && out && uav) {
        UINT zero[4] = { 0, 0, 0, 0 }; void *data = NULL; size_t sz = 0;
        ID3D11DeviceContext_ClearUnorderedAccessViewUint(e.ctx, uav, zero);
        ID3D11DeviceContext_CSSetShader(e.ctx, cs, NULL, 0);
        ID3D11DeviceContext_CSSetUnorderedAccessViews(e.ctx, 0, 1, &uav, NULL);
        ID3D11DeviceContext_DispatchIndirect(e.ctx, args, args_off);
        if (SUCCEEDED(d3dh_readback_buffer11(e.dev, e.ctx, out, &data, &sz))) {
            const UINT *w = data; UINT written = 0, i;
            for (i = 0; i < 16; i++) if (w[i]) written++;
            EXPECT_EQ(written, expect_written);
            for (UINT y = 0; y < gy && y < 4; y++) for (UINT x = 0; x < gx && x < 2; x++) if (gz) {
                UINT idx = y * 2 + x; if (idx < 16) CHECKF(w[idx] == 1 + x + (y + 1) * 16 + (gz - 1) * 256 || w[idx] == 1 + x + (y + 1) * 16, "group (%u,%u) wrote %#x", x, y, w[idx]);
            }
            free(data);
        } else CHECK(0);
        ID3D11DeviceContext_ClearState(e.ctx);
    }
    SH_REL(uav); SH_REL(out); SH_REL(args); SH_REL(cs); if (b) ID3D10Blob_Release(b); sh_env_free(&e);
}
TEST(d3d11_shaders_dispatch_indirect_offset_12) { indirect_case(12, 2, 3, 1, 6); }
TEST(d3d11_shaders_dispatch_indirect_one_group) { indirect_case(12, 1, 1, 1, 1); }
TEST(d3d11_shaders_dispatch_indirect_zero_x_writes_nothing) { indirect_case(12, 0, 3, 1, 0); }
TEST(d3d11_shaders_dispatch_indirect_zero_z_writes_nothing) { indirect_case(12, 2, 2, 0, 0); }

/* ---- class instances carrying data from a constant buffer ------------------------------------------------------------------- */
static const char *k_ps_cbclass =
    "interface iColor { float4 Get(); };\n"
    "class cConst : iColor { float4 c; float4 Get() { return c; } };\n"
    "iColor g_color;\n"
    "cConst g_a;\n"
    "cConst g_b;\n"
    "float4 main(float4 p : SV_Position) : SV_Target { return g_color.Get(); }\n";
TEST(d3d11_shaders_linkage_data_instances_reflection)
{
    REQUIRE(d3dh_D3DReflect());
    ID3DBlob *b = NULL; ID3D11ShaderReflection *r = NULL;
    if (FAILED(d3dh_compile(k_ps_cbclass, "main", "ps_5_0", 0, &b))) { CHECKF(0, "compile: %s", d3dh_compile_errors()); return; }
    if (SUCCEEDED(d3dh_D3DReflect()(SH_BP(b), SH_BN(b), &IID_ID3D11ShaderReflection, (void **)&r))) {
        EXPECT_EQ(r->lpVtbl->GetNumInterfaceSlots(r), 1);
        D3D11_SHADER_DESC d; if (SUCCEEDED(r->lpVtbl->GetDesc(r, &d))) { EXPECT_EQ(d.ConstantBuffers, 2); }   /* $Globals plus the compiler's $ThisPointer buffer */
        r->lpVtbl->Release(r);
    } else CHECK(0);
    ID3D10Blob_Release(b);
}
static void cb_instance_case(UINT inst_ix)
{
    SH_ENV(e, 0xb000); ID3DBlob *b = NULL, *vbb = sh_blob("vs_noinput"); ID3D11ClassLinkage *cl = NULL; ID3D11PixelShader *ps = NULL; ID3D11VertexShader *vs = NULL;
    ID3D11ClassInstance *inst = NULL; ID3D11Buffer *cb = NULL; ID3D11Texture2D *tex = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11RasterizerState *rs = NULL;
    static const char *const names[2] = { "g_a", "g_b" };
    float cbdata[8] = { 1, 0, 0, 1,  0, 0, 1, 1 };    /* g_a.c = red at offset 0, g_b.c = blue at offset 16 */
    if (!vbb || FAILED(d3dh_compile(k_ps_cbclass, "main", "ps_5_0", 0, &b))) { sh_env_free(&e); return; }
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(b), SH_BN(b), cl, &ps), S_OK);
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vbb), SH_BN(vbb), NULL, &vs), S_OK);
    EXPECT_HR(ID3D11ClassLinkage_GetClassInstance(cl, names[inst_ix], 0, &inst), S_OK);
    if (inst) {
        D3D11_CLASS_INSTANCE_DESC d; memset(&d, 0xcc, sizeof d); ID3D11ClassInstance_GetDesc(inst, &d);
        char nm[32] = ""; SIZE_T len = sizeof nm; ID3D11ClassInstance_GetInstanceName(inst, nm, &len); EXPECT_STR(nm, names[inst_ix]);
        len = sizeof nm; ID3D11ClassInstance_GetTypeName(inst, nm, &len);
        EXPECT_EQ(d.Created, FALSE); EXPECT_EQ(d.InstanceId, inst_ix);   /* declaration order of the shader's instances */
        EXPECT_EQ(d.InstanceIndex, 0); EXPECT_STR(nm, "");
    }
    D3D11_BUFFER_DESC bd = { 64, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 }; D3D11_SUBRESOURCE_DATA sd = { cbdata, 0, 0 };
    float cbfull[16]; memset(cbfull, 0, sizeof cbfull); memcpy(cbfull, cbdata, sizeof cbdata); sd.pSysMem = cbfull;
    EXPECT_HR(ID3D11Device_CreateBuffer(e.dev, &bd, &sd, &cb), S_OK);
    D3D11_TEXTURE2D_DESC td = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
    EXPECT_HR(ID3D11Device_CreateTexture2D(e.dev, &td, NULL, &tex), S_OK);
    if (tex) EXPECT_HR(ID3D11Device_CreateRenderTargetView(e.dev, (ID3D11Resource *)tex, NULL, &rtv), S_OK);
    D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd); rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    EXPECT_HR(ID3D11Device_CreateRasterizerState(e.dev, &rd, &rs), S_OK);
    if (ps && vs && inst && cb && rtv && rs) {
        D3D11_VIEWPORT vp = { 0, 0, 4, 4, 0, 1 }; d3dh_image im;
        ID3D11DeviceContext_OMSetRenderTargets(e.ctx, 1, &rtv, NULL); ID3D11DeviceContext_RSSetViewports(e.ctx, 1, &vp); ID3D11DeviceContext_RSSetState(e.ctx, rs);
        ID3D11DeviceContext_IASetPrimitiveTopology(e.ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0); ID3D11DeviceContext_PSSetConstantBuffers(e.ctx, 0, 1, &cb);
        ID3D11DeviceContext_PSSetShader(e.ctx, ps, &inst, 1);
        ID3D11DeviceContext_Draw(e.ctx, 3, 0);
        if (SUCCEEDED(d3dh_readback_texture2d11(e.dev, e.ctx, tex, 0, &im))) {
            /* unlike the empty classes of ps_interface, an instance of a class with a data member is usable straight from GetClassInstance:
             * its colour comes from the constant buffer (g_a = red at offset 0, g_b = blue at offset 16) */
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++)
                t_check(d3dh_image_pixel32(&im, x, y) == (inst_ix ? 0xffff0000u : 0xff0000ffu), __FILE__, __LINE__, "data instance colour", "%s pixel(%u,%u)=%08x", names[inst_ix], x, y, d3dh_image_pixel32(&im, x, y));
            d3dh_image_free(&im);
        }
        ID3D11DeviceContext_ClearState(e.ctx);
    }
    SH_REL(rs); SH_REL(rtv); SH_REL(tex); SH_REL(cb); SH_REL(inst); SH_REL(vs); SH_REL(ps); SH_REL(cl); if (b) ID3D10Blob_Release(b); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_data_instance_g_a_draws) { cb_instance_case(0); }
TEST(d3d11_shaders_linkage_data_instance_g_b_draws) { cb_instance_case(1); }

/* ---- creation matrix: every table shader on every feature level ------------------------------------------------------------------ */
/* A shader is creatable when the device level reaches the shader's own minimum and is at least 10_0 (plain SM4/5 bytecode is rejected
 * at 9_x; level-9 profiles are a separate encoding). Everything else is E_INVALIDARG, on the validation path too. */
static void matrix_for(D3D_FEATURE_LEVEL fl)
{
    SH_ENV_EXACT(e, fl);
    int ok = 0, rej = 0;
    for (int i = 0; i < SH_NTABLE; i++) {
        const sh_ent *t = &sh_table[i];
        if (!strcmp(t->name, "gs_stream")) continue;                    /* needs the stream-output creator */
        ID3DBlob *b = sh_blob(t->name); if (!b) continue;
        int st = sh_stage_of_target(t->target);
        ID3D11ClassLinkage *cl = NULL; void *sh = NULL; HRESULT hv, hr;
        if (sh_needs_linkage(t)) ID3D11Device_CreateClassLinkage(e.dev, &cl);
        int want_ok = (UINT)fl >= t->minfl && (UINT)fl >= 0xa000;
        /* WARP quirk: compute shaders written for cs_5_0 at minfl 0xb000 are the only ones above 10_x */
        hv = sh_create(e.dev, st, SH_BP(b), SH_BN(b), cl, NULL);
        hr = sh_create(e.dev, st, SH_BP(b), SH_BN(b), cl, &sh);
        t_check(hr == (want_ok ? S_OK : E_INVALIDARG) && hv == (want_ok ? S_FALSE : E_INVALIDARG) && !!sh == want_ok, __FILE__, __LINE__, "shader x feature level",
                "%s (%s min %#x) at FL %#x: create %s validate %s", t->name, t->target, t->minfl, (UINT)fl, d3dh_hr_name(hr), d3dh_hr_name(hv));
        if (want_ok) ok++; else rej++;
        SH_REL(sh); SH_REL(cl);
    }
    CHECK(ok > 0 || (UINT)fl < 0xa000);
    sh_env_free(&e);
}
TEST(d3d11_shaders_matrix_fl9_1) { matrix_for(D3D_FEATURE_LEVEL_9_1); }
TEST(d3d11_shaders_matrix_fl9_3) { matrix_for(D3D_FEATURE_LEVEL_9_3); }
TEST(d3d11_shaders_matrix_fl10_0) { matrix_for(D3D_FEATURE_LEVEL_10_0); }
TEST(d3d11_shaders_matrix_fl10_1) { matrix_for(D3D_FEATURE_LEVEL_10_1); }
TEST(d3d11_shaders_matrix_fl11_0) { matrix_for(D3D_FEATURE_LEVEL_11_0); }
TEST(d3d11_shaders_matrix_fl11_1) { matrix_for(D3D_FEATURE_LEVEL_11_1); }
