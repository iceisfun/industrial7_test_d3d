/* Effect state objects, resource variables, shaders/passes, pools and device state blocks. */
#include "ue.h"
#include <float.h>

#define E_INVALIDARG_ ((HRESULT)0x80070057u)
#define E_FAIL_       ((HRESULT)0x80004005u)

#define BV(fx, name) ID3D10Effect_GetVariableByName(fx, name)
#define NEED_REAL_DEVICE(dev) NEED_DEVICE(dev); if (ue_device_is_fake()) { t_skip("needs a real device"); return; }

/* ---- state-object backing stores ---- */
TEST(d3d10_utils_effects_fxo_sampler_backing_store)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10EffectSamplerVariable *sv = ID3D10EffectVariable_AsSampler(BV(fx, "smp"));
    D3D10_SAMPLER_DESC d;
    memset(&d, 0xcd, sizeof d);
    EXPECT_HR(ID3D10EffectSamplerVariable_GetBackingStore(sv, 0, &d), S_OK);
    EXPECT_EQ(d.Filter, D3D10_FILTER_MIN_MAG_MIP_LINEAR);
    EXPECT_EQ(d.AddressU, D3D10_TEXTURE_ADDRESS_WRAP);
    EXPECT_EQ(d.AddressV, D3D10_TEXTURE_ADDRESS_CLAMP);
    t_note("sampler defaults: addrW %d bias %g aniso %u cmp %d minlod %g maxlod %g", d.AddressW, d.MipLODBias, d.MaxAnisotropy, d.ComparisonFunc, d.MinLOD, d.MaxLOD);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_blend_backing_store)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10EffectBlendVariable *bv = ID3D10EffectVariable_AsBlend(BV(fx, "bs"));
    D3D10_BLEND_DESC d;
    memset(&d, 0xcd, sizeof d);
    EXPECT_HR(ID3D10EffectBlendVariable_GetBackingStore(bv, 0, &d), S_OK);
    CHECK(d.BlendEnable[0] != 0);
    EXPECT_EQ(d.SrcBlend, D3D10_BLEND_SRC_ALPHA);
    EXPECT_EQ(d.DestBlend, D3D10_BLEND_INV_SRC_ALPHA);
    t_note("blend defaults: a2c %d op %d srcA %d dstA %d opA %d mask0 %02x en1 %d", d.AlphaToCoverageEnable, d.BlendOp, d.SrcBlendAlpha, d.DestBlendAlpha, d.BlendOpAlpha, d.RenderTargetWriteMask[0], d.BlendEnable[1]);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_rasterizer_backing_store)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10EffectRasterizerVariable *rv = ID3D10EffectVariable_AsRasterizer(BV(fx, "rs"));
    D3D10_RASTERIZER_DESC d;
    memset(&d, 0xcd, sizeof d);
    EXPECT_HR(ID3D10EffectRasterizerVariable_GetBackingStore(rv, 0, &d), S_OK);
    EXPECT_EQ(d.CullMode, D3D10_CULL_NONE);
    EXPECT_EQ(d.FillMode, D3D10_FILL_WIREFRAME);
    t_note("rasterizer defaults: ccw %d bias %d clip %d scissor %d ms %d aa %d", d.FrontCounterClockwise, d.DepthBias, d.DepthClipEnable, d.ScissorEnable, d.MultisampleEnable, d.AntialiasedLineEnable);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_depth_stencil_backing_store)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10EffectDepthStencilVariable *dv = ID3D10EffectVariable_AsDepthStencil(BV(fx, "dss"));
    D3D10_DEPTH_STENCIL_DESC d;
    memset(&d, 0xcd, sizeof d);
    EXPECT_HR(ID3D10EffectDepthStencilVariable_GetBackingStore(dv, 0, &d), S_OK);
    EXPECT_EQ(d.DepthEnable, 0);
    t_note("depth-stencil defaults: write %d func %d stencil %d", d.DepthWriteMask, d.DepthFunc, d.StencilEnable);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_backing_store_index_out_of_range)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    D3D10_SAMPLER_DESC sd; D3D10_BLEND_DESC bd; D3D10_RASTERIZER_DESC rd; D3D10_DEPTH_STENCIL_DESC dd;
    HRESULT h1 = ID3D10EffectSamplerVariable_GetBackingStore(ID3D10EffectVariable_AsSampler(BV(fx, "smp")), 1, &sd);
    HRESULT h2 = ID3D10EffectBlendVariable_GetBackingStore(ID3D10EffectVariable_AsBlend(BV(fx, "bs")), 1, &bd);
    HRESULT h3 = ID3D10EffectRasterizerVariable_GetBackingStore(ID3D10EffectVariable_AsRasterizer(BV(fx, "rs")), 1, &rd);
    HRESULT h4 = ID3D10EffectDepthStencilVariable_GetBackingStore(ID3D10EffectVariable_AsDepthStencil(BV(fx, "dss")), 1, &dd);
    CHECK(FAILED(h1)); CHECK(FAILED(h2)); CHECK(FAILED(h3)); CHECK(FAILED(h4));
    t_note("backing store index 1 on a single object: %s %s %s %s", d3dh_hr_name(h1), d3dh_hr_name(h2), d3dh_hr_name(h3), d3dh_hr_name(h4));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_sampler_array_backing_store)
{
    NEED_DEVICE(dev);
    const char *src =
        "SamplerState smpArr[2] { { Filter = MIN_MAG_MIP_POINT; }, { Filter = ANISOTROPIC; MaxAnisotropy = 4; } };\n"
        "technique10 T { pass P { } }\n";
    FX_OPEN(fx, src);
    ID3D10EffectVariable *v = BV(fx, "smpArr");
    D3D10_EFFECT_TYPE_DESC td;
    EXPECT_HR(ID3D10EffectType_GetDesc(ID3D10EffectVariable_GetType(v), &td), S_OK);
    EXPECT_EQ(td.Elements, 2);
    ID3D10EffectSamplerVariable *sv = ID3D10EffectVariable_AsSampler(v);
    D3D10_SAMPLER_DESC d0, d1;
    EXPECT_HR(ID3D10EffectSamplerVariable_GetBackingStore(sv, 0, &d0), S_OK);
    EXPECT_HR(ID3D10EffectSamplerVariable_GetBackingStore(sv, 1, &d1), S_OK);
    EXPECT_EQ(d0.Filter, D3D10_FILTER_MIN_MAG_MIP_POINT);
    EXPECT_EQ(d1.Filter, D3D10_FILTER_ANISOTROPIC);
    EXPECT_EQ(d1.MaxAnisotropy, 4);
    D3D10_SAMPLER_DESC d2;
    CHECK(FAILED(ID3D10EffectSamplerVariable_GetBackingStore(sv, 2, &d2)));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_state_objects_from_device)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10BlendState *bs = NULL;
    EXPECT_HR(ID3D10EffectBlendVariable_GetBlendState(ID3D10EffectVariable_AsBlend(BV(fx, "bs")), 0, &bs), S_OK);
    CHECK(bs != NULL);
    if (bs) {
        D3D10_BLEND_DESC d;
        ID3D10BlendState_GetDesc(bs, &d);
        CHECK(d.BlendEnable[0] != 0);
        EXPECT_EQ(d.SrcBlend, D3D10_BLEND_SRC_ALPHA);
        ID3D10BlendState_Release(bs);
    }
    ID3D10RasterizerState *rs = NULL;
    EXPECT_HR(ID3D10EffectRasterizerVariable_GetRasterizerState(ID3D10EffectVariable_AsRasterizer(BV(fx, "rs")), 0, &rs), S_OK);
    if (rs) { D3D10_RASTERIZER_DESC d; ID3D10RasterizerState_GetDesc(rs, &d); EXPECT_EQ(d.CullMode, D3D10_CULL_NONE); ID3D10RasterizerState_Release(rs); } else CHECK(0);
    ID3D10SamplerState *ss = NULL;
    EXPECT_HR(ID3D10EffectSamplerVariable_GetSampler(ID3D10EffectVariable_AsSampler(BV(fx, "smp")), 0, &ss), S_OK);
    if (ss) { D3D10_SAMPLER_DESC d; ID3D10SamplerState_GetDesc(ss, &d); EXPECT_EQ(d.AddressV, D3D10_TEXTURE_ADDRESS_CLAMP); ID3D10SamplerState_Release(ss); } else CHECK(0);
    ID3D10DepthStencilState *ds = NULL;
    EXPECT_HR(ID3D10EffectDepthStencilVariable_GetDepthStencilState(ID3D10EffectVariable_AsDepthStencil(BV(fx, "dss")), 0, &ds), S_OK);
    if (ds) { D3D10_DEPTH_STENCIL_DESC d; ID3D10DepthStencilState_GetDesc(ds, &d); EXPECT_EQ(d.DepthEnable, 0); ID3D10DepthStencilState_Release(ds); } else CHECK(0);
    ID3D10Effect_Release(fx);
}

/* ---- resource variables ---- */
static ID3D10Texture2D *mk_tex(ID3D10Device *dev, DXGI_FORMAT fmt, UINT bind)
{
    D3D10_TEXTURE2D_DESC d;
    ID3D10Texture2D *t = NULL;
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 4; d.MipLevels = 1; d.ArraySize = 1; d.Format = fmt; d.SampleDesc.Count = 1; d.Usage = D3D10_USAGE_DEFAULT; d.BindFlags = bind;
    if (FAILED(ID3D10Device_CreateTexture2D(dev, &d, NULL, &t))) return NULL;
    return t;
}
TEST(d3d10_utils_effects_fxo_srv_set_get)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10Texture2D *t = mk_tex(dev, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_BIND_SHADER_RESOURCE);
    ID3D10ShaderResourceView *srv = NULL;
    if (!t || FAILED(ID3D10Device_CreateShaderResourceView(dev, (ID3D10Resource *)t, NULL, &srv))) { t_skip("cannot create SRV"); SAFE_RELEASE(t); ID3D10Effect_Release(fx); return; }
    ID3D10EffectShaderResourceVariable *rv = ID3D10EffectVariable_AsShaderResource(BV(fx, "tex"));
    ID3D10ShaderResourceView *got = (ID3D10ShaderResourceView *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10EffectShaderResourceVariable_GetResource(rv, &got), S_OK);
    EXPECT_EQ(got, NULL);
    ID3D10ShaderResourceView_AddRef(srv);
    ULONG base = ID3D10ShaderResourceView_Release(srv);
    EXPECT_HR(ID3D10EffectShaderResourceVariable_SetResource(rv, srv), S_OK);
    ID3D10ShaderResourceView_AddRef(srv);
    ULONG held = ID3D10ShaderResourceView_Release(srv);
    CHECK(held > base);
    EXPECT_HR(ID3D10EffectShaderResourceVariable_GetResource(rv, &got), S_OK);
    EXPECT_EQ(got, srv);
    SAFE_RELEASE(got);
    EXPECT_HR(ID3D10EffectShaderResourceVariable_SetResource(rv, NULL), S_OK);
    got = (ID3D10ShaderResourceView *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10EffectShaderResourceVariable_GetResource(rv, &got), S_OK);
    EXPECT_EQ(got, NULL);
    ID3D10ShaderResourceView_AddRef(srv);
    EXPECT_EQ(ID3D10ShaderResourceView_Release(srv), base);
    ID3D10ShaderResourceView_Release(srv);
    ID3D10Texture2D_Release(t);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_srv_unset_is_null)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    static const char *const names[] = { "tex", "texarr", "tex3" };
    for (int i = 0; i < 3; i++) {
        ID3D10ShaderResourceView *got = (ID3D10ShaderResourceView *)(ULONG_PTR)1;
        EXPECT_HR(ID3D10EffectShaderResourceVariable_GetResource(ID3D10EffectVariable_AsShaderResource(BV(fx, names[i])), &got), S_OK);
        t_check(got == NULL, __FILE__, __LINE__, "unset resource is NULL", "%s", names[i]);
        if (got != (ID3D10ShaderResourceView *)(ULONG_PTR)1) SAFE_RELEASE(got);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_srv_set_null_is_ok)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10EffectShaderResourceVariable *rv = ID3D10EffectVariable_AsShaderResource(BV(fx, "tex"));
    EXPECT_HR(ID3D10EffectShaderResourceVariable_SetResource(rv, NULL), S_OK);
    EXPECT_HR(ID3D10EffectShaderResourceVariable_SetResource(rv, NULL), S_OK);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_srv_array_set_get)
{
    NEED_REAL_DEVICE(dev);
    const char *src = "Texture2D texs[3];\ntechnique10 T { pass P { } }\n";
    FX_OPEN(fx, src);
    ID3D10Texture2D *t = mk_tex(dev, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_BIND_SHADER_RESOURCE);
    ID3D10ShaderResourceView *srv = NULL;
    if (!t || FAILED(ID3D10Device_CreateShaderResourceView(dev, (ID3D10Resource *)t, NULL, &srv))) { t_skip("cannot create SRV"); SAFE_RELEASE(t); ID3D10Effect_Release(fx); return; }
    ID3D10EffectShaderResourceVariable *rv = ID3D10EffectVariable_AsShaderResource(BV(fx, "texs"));
    ID3D10ShaderResourceView *in[2] = { srv, NULL }, *out[3] = { (void *)1, (void *)1, (void *)1 };
    EXPECT_HR(ID3D10EffectShaderResourceVariable_SetResourceArray(rv, in, 1, 2), S_OK);
    EXPECT_HR(ID3D10EffectShaderResourceVariable_GetResourceArray(rv, out, 0, 3), S_OK);
    EXPECT_EQ(out[0], NULL);
    EXPECT_EQ(out[1], srv);
    EXPECT_EQ(out[2], NULL);
    SAFE_RELEASE(out[1]);
    { ID3D10ShaderResourceView *nulls[2] = { NULL, NULL }; ID3D10EffectShaderResourceVariable_SetResourceArray(rv, nulls, 1, 2); }
    ID3D10ShaderResourceView_Release(srv);
    ID3D10Texture2D_Release(t);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_rtv_set_get)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10Texture2D *t = mk_tex(dev, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_BIND_RENDER_TARGET);
    ID3D10RenderTargetView *rtv = NULL;
    if (!t || FAILED(ID3D10Device_CreateRenderTargetView(dev, (ID3D10Resource *)t, NULL, &rtv))) { t_skip("cannot create RTV"); SAFE_RELEASE(t); ID3D10Effect_Release(fx); return; }
    ID3D10EffectRenderTargetViewVariable *v = ID3D10EffectVariable_AsRenderTargetView(BV(fx, "rtv"));
    ID3D10RenderTargetView *got = (ID3D10RenderTargetView *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10EffectRenderTargetViewVariable_GetRenderTarget(v, &got), S_OK);
    EXPECT_EQ(got, NULL);
    EXPECT_HR(ID3D10EffectRenderTargetViewVariable_SetRenderTarget(v, rtv), S_OK);
    EXPECT_HR(ID3D10EffectRenderTargetViewVariable_GetRenderTarget(v, &got), S_OK);
    EXPECT_EQ(got, rtv);
    SAFE_RELEASE(got);
    EXPECT_HR(ID3D10EffectRenderTargetViewVariable_SetRenderTarget(v, NULL), S_OK);
    ID3D10RenderTargetView_Release(rtv);
    ID3D10Texture2D_Release(t);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_dsv_set_get)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10Texture2D *t = mk_tex(dev, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D10_BIND_DEPTH_STENCIL);
    ID3D10DepthStencilView *dsv = NULL;
    if (!t || FAILED(ID3D10Device_CreateDepthStencilView(dev, (ID3D10Resource *)t, NULL, &dsv))) { t_skip("cannot create DSV"); SAFE_RELEASE(t); ID3D10Effect_Release(fx); return; }
    ID3D10EffectDepthStencilViewVariable *v = ID3D10EffectVariable_AsDepthStencilView(BV(fx, "dsv"));
    ID3D10DepthStencilView *got = (ID3D10DepthStencilView *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10EffectDepthStencilViewVariable_GetDepthStencil(v, &got), S_OK);
    EXPECT_EQ(got, NULL);
    EXPECT_HR(ID3D10EffectDepthStencilViewVariable_SetDepthStencil(v, dsv), S_OK);
    EXPECT_HR(ID3D10EffectDepthStencilViewVariable_GetDepthStencil(v, &got), S_OK);
    EXPECT_EQ(got, dsv);
    SAFE_RELEASE(got);
    EXPECT_HR(ID3D10EffectDepthStencilViewVariable_SetDepthStencil(v, NULL), S_OK);
    ID3D10DepthStencilView_Release(dsv);
    ID3D10Texture2D_Release(t);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_constant_buffer_override)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectConstantBuffer *cb = ID3D10Effect_GetConstantBufferByName(fx, "cb");
    ID3D10Buffer *got = (ID3D10Buffer *)(ULONG_PTR)1;
    HRESULT hr = ID3D10EffectConstantBuffer_GetConstantBuffer(cb, &got);
    EXPECT_HR(hr, S_OK);
    CHECK(got != NULL);
    if (got) {
        D3D10_BUFFER_DESC d;
        ID3D10Buffer_GetDesc(got, &d);
        EXPECT_EQ(d.BindFlags, D3D10_BIND_CONSTANT_BUFFER);
        EXPECT_EQ(d.ByteWidth, 176);
        ID3D10Buffer_Release(got);
    }
    ID3D10Effect_Release(fx);
}

/* ---- shaders, passes ---- */
TEST(d3d10_utils_effects_fxo_pass_shader_descs)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByName(fx, "TSh");
    D3D10_TECHNIQUE_DESC td;
    EXPECT_HR(ID3D10EffectTechnique_GetDesc(t, &td), S_OK);
    EXPECT_EQ(td.Passes, 1);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(t, 0);
    D3D10_PASS_SHADER_DESC vs, gs, ps;
    EXPECT_HR(ID3D10EffectPass_GetVertexShaderDesc(p, &vs), S_OK);
    EXPECT_HR(ID3D10EffectPass_GetGeometryShaderDesc(p, &gs), S_OK);
    EXPECT_HR(ID3D10EffectPass_GetPixelShaderDesc(p, &ps), S_OK);
    CHECK(vs.pShaderVariable && ID3D10EffectShaderVariable_IsValid(vs.pShaderVariable));
    CHECK(ps.pShaderVariable && ID3D10EffectShaderVariable_IsValid(ps.pShaderVariable));
    CHECK(vs.pShaderVariable != ps.pShaderVariable);
    D3D10_EFFECT_SHADER_DESC d;
    memset(&d, 0, sizeof d);
    if (vs.pShaderVariable && SUCCEEDED(ID3D10EffectShaderVariable_GetShaderDesc(vs.pShaderVariable, vs.ShaderIndex, &d))) {
        CHECK(d.IsInline != 0);
        CHECK(d.pBytecode != NULL && d.BytecodeLength > 32);
        if (d.pBytecode) EXPECT_EQ(memcmp(d.pBytecode, "DXBC", 4), 0);
        EXPECT_EQ(d.NumInputSignatureEntries, 2);
        EXPECT_EQ(d.NumOutputSignatureEntries, 1);
        CHECK(d.pInputSignature != NULL);
    } else CHECK(0);
    memset(&d, 0, sizeof d);
    if (ps.pShaderVariable && SUCCEEDED(ID3D10EffectShaderVariable_GetShaderDesc(ps.pShaderVariable, ps.ShaderIndex, &d))) {
        CHECK(d.IsInline != 0);
        EXPECT_EQ(d.NumInputSignatureEntries, 1);
        EXPECT_EQ(d.NumOutputSignatureEntries, 1);
    } else CHECK(0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_geometry_shader_null)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    D3D10_PASS_SHADER_DESC gs;
    HRESULT hr = ID3D10EffectPass_GetGeometryShaderDesc(p, &gs);
    EXPECT_HR(hr, S_OK);
    if (SUCCEEDED(hr) && gs.pShaderVariable) {
        D3D10_EFFECT_SHADER_DESC d;
        hr = ID3D10EffectShaderVariable_GetShaderDesc(gs.pShaderVariable, gs.ShaderIndex, &d);
        t_note("GetShaderDesc of a NULL geometry shader: %s (len %u)", d3dh_hr_name(hr), SUCCEEDED(hr) ? d.BytecodeLength : 0);
    }
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_shader_signature_elements)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    D3D10_PASS_SHADER_DESC vs;
    EXPECT_HR(ID3D10EffectPass_GetVertexShaderDesc(p, &vs), S_OK);
    D3D10_SIGNATURE_PARAMETER_DESC e;
    EXPECT_HR(ID3D10EffectShaderVariable_GetInputSignatureElementDesc(vs.pShaderVariable, vs.ShaderIndex, 0, &e), S_OK);
    EXPECT_STR(e.SemanticName, "POSITION");
    EXPECT_EQ(e.SemanticIndex, 0);
    EXPECT_EQ(e.Register, 0);
    EXPECT_EQ(e.Mask, 0xf);
    EXPECT_HR(ID3D10EffectShaderVariable_GetInputSignatureElementDesc(vs.pShaderVariable, vs.ShaderIndex, 1, &e), S_OK);
    EXPECT_STR(e.SemanticName, "TEXCOORD");
    EXPECT_EQ(e.Mask, 3);
    CHECK(FAILED(ID3D10EffectShaderVariable_GetInputSignatureElementDesc(vs.pShaderVariable, vs.ShaderIndex, 2, &e)));
    EXPECT_HR(ID3D10EffectShaderVariable_GetOutputSignatureElementDesc(vs.pShaderVariable, vs.ShaderIndex, 0, &e), S_OK);
    EXPECT_STR(e.SemanticName, "SV_Position");
    CHECK(FAILED(ID3D10EffectShaderVariable_GetOutputSignatureElementDesc(vs.pShaderVariable, vs.ShaderIndex, 1, &e)));
    CHECK(FAILED(ID3D10EffectShaderVariable_GetInputSignatureElementDesc(vs.pShaderVariable, vs.ShaderIndex, 0, NULL)));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_desc_input_signature)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    D3D10_PASS_DESC d;
    memset(&d, 0, sizeof d);
    EXPECT_HR(ID3D10EffectPass_GetDesc(p, &d), S_OK);
    EXPECT_STR(d.Name, "P0");
    CHECK(d.pIAInputSignature != NULL);
    CHECK(d.IAInputSignatureSize > 32);
    if (d.pIAInputSignature && d.IAInputSignatureSize > 32) {
        EXPECT_EQ(memcmp(d.pIAInputSignature, "DXBC", 4), 0);
        EXPECT_EQ(*(const DWORD *)(d.pIAInputSignature + 24), d.IAInputSignatureSize);
        CHECK(ue_dxbc_chunk(d.pIAInputSignature, d.IAInputSignatureSize, FOURCC('I','S','G','N'), NULL) != NULL);
    }
    t_note("pass desc: stencil ref %u sample mask %08x blend factor %g %g %g %g", d.StencilRef, d.SampleMask, d.BlendFactor[0], d.BlendFactor[1], d.BlendFactor[2], d.BlendFactor[3]);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_without_vs_has_no_input_signature)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "T0"), 0);
    D3D10_PASS_DESC d;
    memset(&d, 0xcd, sizeof d);
    EXPECT_HR(ID3D10EffectPass_GetDesc(p, &d), S_OK);
    EXPECT_EQ(d.pIAInputSignature, NULL);
    EXPECT_EQ(d.IAInputSignatureSize, 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_input_signature_creates_layout)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    D3D10_PASS_DESC d;
    EXPECT_HR(ID3D10EffectPass_GetDesc(p, &d), S_OK);
    static const D3D10_INPUT_ELEMENT_DESC el[2] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D10InputLayout *il = NULL;
    EXPECT_HR(ID3D10Device_CreateInputLayout(dev, el, 2, d.pIAInputSignature, d.IAInputSignatureSize, &il), S_OK);
    SAFE_RELEASE(il);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_shader_objects_from_device)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    D3D10_PASS_SHADER_DESC vs, ps;
    ID3D10EffectPass_GetVertexShaderDesc(p, &vs);
    ID3D10EffectPass_GetPixelShaderDesc(p, &ps);
    ID3D10VertexShader *v = NULL;
    ID3D10PixelShader *s = NULL;
    EXPECT_HR(ID3D10EffectShaderVariable_GetVertexShader(vs.pShaderVariable, vs.ShaderIndex, &v), S_OK);
    EXPECT_HR(ID3D10EffectShaderVariable_GetPixelShader(ps.pShaderVariable, ps.ShaderIndex, &s), S_OK);
    CHECK(v != NULL); CHECK(s != NULL);
    SAFE_RELEASE(v); SAFE_RELEASE(s);
    /* asking the pixel shader variable for a vertex shader is a type mismatch */
    HRESULT hr = ID3D10EffectShaderVariable_GetVertexShader(ps.pShaderVariable, ps.ShaderIndex, &v);
    CHECK(FAILED(hr));
    SAFE_RELEASE(v);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_apply_binds_shaders)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    ID3D10VertexShader *vs = (void *)1; ID3D10PixelShader *ps = (void *)1; ID3D10GeometryShader *gs = (void *)1;
    ID3D10Device_VSSetShader(dev, NULL); ID3D10Device_PSSetShader(dev, NULL); ID3D10Device_GSSetShader(dev, NULL);
    EXPECT_HR(ID3D10EffectPass_Apply(p, 0), S_OK);
    ID3D10Device_VSGetShader(dev, &vs); ID3D10Device_PSGetShader(dev, &ps); ID3D10Device_GSGetShader(dev, &gs);
    CHECK(vs != NULL); CHECK(ps != NULL); EXPECT_EQ(gs, NULL);
    SAFE_RELEASE(vs); SAFE_RELEASE(ps); SAFE_RELEASE(gs);
    ID3D10Device_VSSetShader(dev, NULL); ID3D10Device_PSSetShader(dev, NULL);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_apply_binds_constant_buffer)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    ID3D10Buffer *cb[1] = { (void *)1 };
    { ID3D10Buffer *nullcb[1] = { NULL }; ID3D10Device_VSSetConstantBuffers(dev, 0, 1, nullcb); }
    EXPECT_HR(ID3D10EffectPass_Apply(p, 0), S_OK);
    ID3D10Device_VSGetConstantBuffers(dev, 0, 1, cb);
    CHECK(cb[0] != NULL);
    if (cb[0]) {
        D3D10_BUFFER_DESC d;
        ID3D10Buffer_GetDesc(cb[0], &d);
        EXPECT_EQ(d.ByteWidth, 16);
        D3D10_BUFFER_DESC sd = d;
        sd.Usage = D3D10_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
        ID3D10Buffer *st = NULL;
        if (SUCCEEDED(ID3D10Device_CreateBuffer(dev, &sd, NULL, &st))) {
            void *m = NULL;
            ID3D10Device_CopyResource(dev, (ID3D10Resource *)st, (ID3D10Resource *)cb[0]);
            if (SUCCEEDED(ID3D10Buffer_Map(st, D3D10_MAP_READ, 0, &m)) && m) {
                const float *f = m;
                EXPECT_EQ(f[0] == 1 && f[1] == 2 && f[2] == 3 && f[3] == 4, 1);
                ID3D10Buffer_Unmap(st);
            } else t_note("could not map the staging copy");
            ID3D10Buffer_Release(st);
        }
        ID3D10Buffer_Release(cb[0]);
    }
    { ID3D10Buffer *nullcb[1] = { NULL }; ID3D10Device_VSSetConstantBuffers(dev, 0, 1, nullcb); }
    ID3D10Device_VSSetShader(dev, NULL); ID3D10Device_PSSetShader(dev, NULL);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_constant_buffer_updated_on_apply)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    float nv[4] = { 10, 20, 30, 40 };
    EXPECT_HR(ID3D10EffectVectorVariable_SetFloatVector(ID3D10EffectVariable_AsVector(BV(fx, "gcol")), nv), S_OK);
    EXPECT_HR(ID3D10EffectPass_Apply(p, 0), S_OK);
    ID3D10Buffer *cb[1] = { NULL };
    ID3D10Device_VSGetConstantBuffers(dev, 0, 1, cb);
    if (cb[0]) {
        D3D10_BUFFER_DESC d, sd;
        ID3D10Buffer_GetDesc(cb[0], &d);
        sd = d; sd.Usage = D3D10_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
        ID3D10Buffer *st = NULL;
        if (SUCCEEDED(ID3D10Device_CreateBuffer(dev, &sd, NULL, &st))) {
            void *m = NULL;
            ID3D10Device_CopyResource(dev, (ID3D10Resource *)st, (ID3D10Resource *)cb[0]);
            if (SUCCEEDED(ID3D10Buffer_Map(st, D3D10_MAP_READ, 0, &m)) && m) {
                const float *f = m;
                EXPECT_EQ(f[0] == 10 && f[1] == 20 && f[2] == 30 && f[3] == 40, 1);
                ID3D10Buffer_Unmap(st);
            }
            ID3D10Buffer_Release(st);
        }
        ID3D10Buffer_Release(cb[0]);
    } else CHECK(0);
    { ID3D10Buffer *nullcb[1] = { NULL }; ID3D10Device_VSSetConstantBuffers(dev, 0, 1, nullcb); }
    ID3D10Device_VSSetShader(dev, NULL); ID3D10Device_PSSetShader(dev, NULL);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_apply_sets_render_state)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_OBJECTS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TObj"), 0);
    { FLOAT one[4] = { 1, 1, 1, 1 }; ID3D10Device_OMSetBlendState(dev, NULL, one, 0xffffffff); }
    EXPECT_HR(ID3D10EffectPass_Apply(p, 0), S_OK);
    ID3D10BlendState *bs = NULL;
    FLOAT bf[4] = { -1, -1, -1, -1 };
    UINT mask = 0;
    ID3D10Device_OMGetBlendState(dev, &bs, bf, &mask);
    CHECK(bs != NULL);
    EXPECT_EQ(mask, 0xffffffffu);
    EXPECT_EQ(bf[0] == 0 && bf[1] == 0 && bf[2] == 0 && bf[3] == 0, 1);
    if (bs) { D3D10_BLEND_DESC d; ID3D10BlendState_GetDesc(bs, &d); EXPECT_EQ(d.SrcBlend, D3D10_BLEND_SRC_ALPHA); ID3D10BlendState_Release(bs); }
    ID3D10RasterizerState *rs = NULL;
    ID3D10Device_RSGetState(dev, &rs);
    CHECK(rs != NULL);
    if (rs) { D3D10_RASTERIZER_DESC d; ID3D10RasterizerState_GetDesc(rs, &d); EXPECT_EQ(d.CullMode, D3D10_CULL_NONE); ID3D10RasterizerState_Release(rs); }
    ID3D10DepthStencilState *ds = NULL;
    UINT sref = 99;
    ID3D10Device_OMGetDepthStencilState(dev, &ds, &sref);
    CHECK(ds != NULL);
    EXPECT_EQ(sref, 0);
    SAFE_RELEASE(ds);
    { FLOAT one[4] = { 1, 1, 1, 1 }; ID3D10Device_OMSetBlendState(dev, NULL, one, 0xffffffff); }
    ID3D10Device_RSSetState(dev, NULL);
    ID3D10Device_OMSetDepthStencilState(dev, NULL, 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_apply_empty_pass)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "T0"), 0);
    EXPECT_HR(ID3D10EffectPass_Apply(p, 0), S_OK);
    EXPECT_HR(ID3D10EffectPass_Apply(p, 0), S_OK);
    ID3D10EffectPass *p1 = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "T0"), 1);
    EXPECT_HR(ID3D10EffectPass_Apply(p1, 0), S_OK);
    HRESULT hr = ID3D10EffectPass_Apply(p, 0x10);
    t_note("Apply with a non-zero flags argument: %s", d3dh_hr_name(hr));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_pass_state_block_mask)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByName(fx, "TSh");
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(t, 0);
    D3D10_STATE_BLOCK_MASK m, mt;
    memset(&m, 0, sizeof m); memset(&mt, 0, sizeof mt);
    EXPECT_HR(ID3D10EffectPass_ComputeStateBlockMask(p, &m), S_OK);
    EXPECT_HR(ID3D10EffectTechnique_ComputeStateBlockMask(t, &mt), S_OK);
    CHECK(UIN("d3d10.dll", D3D10StateBlockMaskGetSetting)(&m, D3D10_DST_VS, 0) != 0);
    CHECK(UIN("d3d10.dll", D3D10StateBlockMaskGetSetting)(&m, D3D10_DST_PS, 0) != 0);
    CHECK(UIN("d3d10.dll", D3D10StateBlockMaskGetSetting)(&mt, D3D10_DST_VS, 0) != 0);
    CHECK(UIN("d3d10.dll", D3D10StateBlockMaskGetSetting)(&mt, D3D10_DST_PS, 0) != 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_empty_pass_state_block_mask)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "T0"), 0);
    D3D10_STATE_BLOCK_MASK m;
    memset(&m, 0xcd, sizeof m);
    EXPECT_HR(ID3D10EffectPass_ComputeStateBlockMask(p, &m), S_OK);
    int pop = 0;
    for (unsigned i = 0; i < sizeof m; i++) for (int b = 0; b < 8; b++) pop += (((unsigned char *)&m)[i] >> b) & 1;
    t_note("state block mask bits set by an empty pass: %d", pop);
    t_note("empty pass: VS bit %d PS bit %d", UIN("d3d10.dll", D3D10StateBlockMaskGetSetting)(&m, D3D10_DST_VS, 0) != 0, UIN("d3d10.dll", D3D10StateBlockMaskGetSetting)(&m, D3D10_DST_PS, 0) != 0);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_shader_variable_global_is_shader)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_SHADERS);
    ID3D10EffectPass *p = ID3D10EffectTechnique_GetPassByIndex(ID3D10Effect_GetTechniqueByName(fx, "TSh"), 0);
    D3D10_PASS_SHADER_DESC vs;
    ID3D10EffectPass_GetVertexShaderDesc(p, &vs);
    ID3D10EffectVariable *as_var = (ID3D10EffectVariable *)vs.pShaderVariable;
    ID3D10EffectShaderVariable *back = ID3D10EffectVariable_AsShader(as_var);
    CHECK(back && ID3D10EffectShaderVariable_IsValid(back));
    CHECK(!ID3D10EffectScalarVariable_IsValid(ID3D10EffectVariable_AsScalar(as_var)));
    ID3D10Effect_Release(fx);
}

/* ---- pools ---- */
typedef HRESULT (WINAPI *create_fx_t)(void *, SIZE_T, UINT, ID3D10Device *, ID3D10EffectPool *, ID3D10Effect **);
typedef HRESULT (WINAPI *create_pool_t)(void *, SIZE_T, UINT, ID3D10Device *, ID3D10EffectPool **);
static int make_pool(ID3D10Device *dev, ID3D10EffectPool **pool)
{
    ID3D10Blob *b = NULL;
    create_pool_t f = (create_pool_t)t_proc_in("d3d10.dll", "D3D10CreateEffectPoolFromMemory");
    if (!f) { t_skip("missing export"); return 0; }
    HRESULT hr = ue_compile_fx(UE_FX_POOL, 0, 0, &b);
    if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "pool compile", "%s", d3dh_hr_name(hr)); return 0; }
    hr = f((void *)ue_bp(b), ue_bs(b), 0, dev, pool);
    UE_REL(b);
    if (FAILED(hr) && t_is_wine()) { t_skip("pool unsupported by the emulator"); return 0; }
    t_check(SUCCEEDED(hr) && *pool, __FILE__, __LINE__, "pool created", "%s", d3dh_hr_name(hr));
    return SUCCEEDED(hr) && *pool;
}
static int make_child(ID3D10Device *dev, ID3D10EffectPool *pool, ID3D10Effect **fx)
{
    ID3D10Blob *b = NULL;
    create_fx_t f = (create_fx_t)t_proc_in("d3d10.dll", "D3D10CreateEffectFromMemory");
    HRESULT hr = ue_compile_fx(UE_FX_CHILD, 0, D3D10_EFFECT_COMPILE_CHILD_EFFECT, &b);
    if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "child compile", "%s", d3dh_hr_name(hr)); return 0; }
    hr = f((void *)ue_bp(b), ue_bs(b), D3D10_EFFECT_COMPILE_CHILD_EFFECT, dev, pool, fx);
    UE_REL(b);
    t_check(SUCCEEDED(hr) && *fx, __FILE__, __LINE__, "child created", "%s", d3dh_hr_name(hr));
    return SUCCEEDED(hr) && *fx;
}
TEST(d3d10_utils_effects_fxp_pool_basics)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *pe = ID3D10EffectPool_AsEffect(pool);
    CHECK(pe != NULL);
    if (pe) {
        CHECK(ID3D10Effect_IsPool(pe) != 0);
        CHECK(ID3D10Effect_IsValid(pe) != 0);
        EXPECT_EQ(ID3D10EffectPool_AsEffect(pool), pe);
        D3D10_EFFECT_DESC d;
        EXPECT_HR(ID3D10Effect_GetDesc(pe, &d), S_OK);
        EXPECT_EQ(d.IsChildEffect, 0);
        EXPECT_EQ(d.GlobalVariables, 3);
        EXPECT_EQ(d.Techniques, 1);
        EXPECT_EQ(d.SharedGlobalVariables, 0);
    }
    EQV(ID3D10EffectPool_Release(pool), 0);
}
TEST(d3d10_utils_effects_fxp_pool_refcount)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    EQV(ID3D10EffectPool_AddRef(pool), 2);
    EQV(ID3D10EffectPool_Release(pool), 1);
    EQV(ID3D10EffectPool_Release(pool), 0);
}
TEST(d3d10_utils_effects_fxp_pool_variables)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *pe = ID3D10EffectPool_AsEffect(pool);
    float f = 0;
    EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(BV(pe, "shared_f")), &f), S_OK);
    EXPECT_EQ(f == 2.0f, 1);
    CHECK(ID3D10EffectShaderResourceVariable_IsValid(ID3D10EffectVariable_AsShaderResource(BV(pe, "shared_tex"))));
    CHECK(ID3D10EffectConstantBuffer_IsValid(ID3D10Effect_GetConstantBufferByName(pe, "shared_cb")));
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_child_desc)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *fx = NULL;
    if (make_child(dev, pool, &fx)) {
        D3D10_EFFECT_DESC d;
        EXPECT_HR(ID3D10Effect_GetDesc(fx, &d), S_OK);
        EXPECT_EQ(d.IsChildEffect, 1);
        EXPECT_EQ(d.SharedGlobalVariables, 3);
        EXPECT_EQ(d.GlobalVariables, 2);
        EXPECT_EQ(d.Techniques, 1);
        CHECK(d.SharedConstantBuffers >= 1);
        EXPECT_EQ(ID3D10Effect_IsPool(fx), 0);
        ID3D10Effect_Release(fx);
    }
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_child_shares_pool_memory)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *fx = NULL;
    if (make_child(dev, pool, &fx)) {
        ID3D10Effect *pe = ID3D10EffectPool_AsEffect(pool);
        float f = 0;
        ID3D10EffectScalarVariable *cs = ID3D10EffectVariable_AsScalar(BV(fx, "shared_f"));
        CHECK(ID3D10EffectScalarVariable_IsValid(cs));
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(cs, &f), S_OK);
        EXPECT_EQ(f == 2.0f, 1);
        EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(cs, 5.5f), S_OK);
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(BV(pe, "shared_f")), &f), S_OK);
        EXPECT_EQ(f == 5.5f, 1);
        EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(ID3D10EffectVariable_AsScalar(BV(pe, "shared_f")), -1.0f), S_OK);
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(cs, &f), S_OK);
        EXPECT_EQ(f == -1.0f, 1);
        ID3D10Effect_Release(fx);
    }
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_child_variable_enumeration)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *fx = NULL;
    if (make_child(dev, pool, &fx)) {
        /* local variables come first, then the pool's variables */
        static const char *const order[5] = { "own_vec", "own_f", "shared_vec", "shared_f", "shared_tex" };
        for (UINT i = 0; i < 5; i++) {
            ID3D10EffectVariable *v = ID3D10Effect_GetVariableByIndex(fx, i);
            D3D10_EFFECT_VARIABLE_DESC d;
            if (!v || !ID3D10EffectVariable_IsValid(v) || FAILED(ID3D10EffectVariable_GetDesc(v, &d))) { t_check(0, __FILE__, __LINE__, "variable index", "%u", i); continue; }
            t_check(d.Name && !strcmp(d.Name, order[i]), __FILE__, __LINE__, "order", "%u: %s want %s", i, d.Name ? d.Name : "(null)", order[i]);
            if (i >= 2) CHECK(d.Flags & D3D10_EFFECT_VARIABLE_POOLED);
            else CHECK(!(d.Flags & D3D10_EFFECT_VARIABLE_POOLED));
        }
        ID3D10EffectVariable *bad = ID3D10Effect_GetVariableByIndex(fx, 5);
        CHECK(bad && !ID3D10EffectVariable_IsValid(bad));
        float f;
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(BV(fx, "own_f")), &f), S_OK);
        EXPECT_EQ(f == 7.0f, 1);
        ID3D10Effect_Release(fx);
    }
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_child_constant_buffers)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *fx = NULL;
    if (make_child(dev, pool, &fx)) {
        ID3D10EffectConstantBuffer *own = ID3D10Effect_GetConstantBufferByName(fx, "own");
        ID3D10EffectConstantBuffer *sh = ID3D10Effect_GetConstantBufferByName(fx, "shared_cb");
        CHECK(own && ID3D10EffectConstantBuffer_IsValid(own));
        CHECK(sh && ID3D10EffectConstantBuffer_IsValid(sh));
        ID3D10Effect *pe = ID3D10EffectPool_AsEffect(pool);
        EXPECT_EQ(sh, ID3D10Effect_GetConstantBufferByName(pe, "shared_cb"));
        ID3D10EffectConstantBuffer *miss = ID3D10Effect_GetConstantBufferByName(fx, "nosuch");
        CHECK(miss && !ID3D10EffectConstantBuffer_IsValid(miss));
        ID3D10Effect_Release(fx);
    }
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_child_semantic_and_technique_lookup)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *fx = NULL;
    if (make_child(dev, pool, &fx)) {
        /* techniques are not inherited from the pool */
        ID3D10EffectTechnique *t = ID3D10Effect_GetTechniqueByName(fx, "TPool");
        CHECK(t && !ID3D10EffectTechnique_IsValid(t));
        t = ID3D10Effect_GetTechniqueByName(fx, "TChild");
        CHECK(t && ID3D10EffectTechnique_IsValid(t));
        ID3D10Effect_Release(fx);
    }
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_two_children_share_values)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *a = NULL, *b = NULL;
    if (make_child(dev, pool, &a) && make_child(dev, pool, &b)) {
        float f = 0;
        EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(ID3D10EffectVariable_AsScalar(BV(a, "shared_f")), 33.0f), S_OK);
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(BV(b, "shared_f")), &f), S_OK);
        EXPECT_EQ(f == 33.0f, 1);
        EXPECT_HR(ID3D10EffectScalarVariable_SetFloat(ID3D10EffectVariable_AsScalar(BV(a, "own_f")), 1.0f), S_OK);
        EXPECT_HR(ID3D10EffectScalarVariable_GetFloat(ID3D10EffectVariable_AsScalar(BV(b, "own_f")), &f), S_OK);
        EXPECT_EQ(f == 7.0f, 1);
    }
    if (a) ID3D10Effect_Release(a);
    if (b) ID3D10Effect_Release(b);
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_pool_qi)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    void *o = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10EffectPool_QueryInterface(pool, &IID_IDXGIObject, &o), E_NOINTERFACE);
    EXPECT_EQ(o, NULL);
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_pool_qi_null_out)
{ if (t_is_wine()) { t_skip("Wine faults on this call; Windows behaviour is authoritative"); return; }
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    EXPECT_HR(ID3D10EffectPool_QueryInterface(pool, &IID_IUnknown, NULL), E_INVALIDARG_);
    ID3D10EffectPool_Release(pool);
}
TEST(d3d10_utils_effects_fxp_optimize_pool_is_noop_ok)
{
    NEED_DEVICE(dev);
    ID3D10EffectPool *pool = NULL;
    if (!make_pool(dev, &pool)) return;
    ID3D10Effect *pe = ID3D10EffectPool_AsEffect(pool);
    HRESULT hr = ID3D10Effect_Optimize(pe);
    t_note("Optimize on a pool effect: %s, IsOptimized=%d", d3dh_hr_name(hr), ID3D10Effect_IsOptimized(pe));
    CHECK(SUCCEEDED(hr));
    ID3D10EffectPool_Release(pool);
}

/* ---- device state blocks ---- */
typedef HRESULT (WINAPI *csb_t)(ID3D10Device *, D3D10_STATE_BLOCK_MASK *, ID3D10StateBlock **);
#define CSB(dll) ((csb_t)t_proc_in(dll, "D3D10CreateStateBlock"))
TEST(d3d10_utils_effects_sb_create_null_args)
{
    REQUIRE(CSB("d3d10.dll"));
    NOWINE("emulated state blocks are stubs");
    NEED_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    memset(&m, 0, sizeof m);
    ID3D10StateBlock *sb = (ID3D10StateBlock *)(ULONG_PTR)1;
    EXPECT_HR(CSB("d3d10.dll")(NULL, &m, &sb), E_INVALIDARG_);
    EXPECT_HR(CSB("d3d10.dll")(dev, &m, NULL), E_INVALIDARG_);
    EXPECT_HR(CSB("d3d10.dll")(NULL, &m, NULL), E_INVALIDARG_);
    EXPECT_HR(CSB("d3d10_1.dll")(NULL, &m, &sb), E_INVALIDARG_);
}
TEST(d3d10_utils_effects_sb_create_and_release)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskDisableAll)(&m);
    UIN("d3d10.dll", D3D10StateBlockMaskEnableCapture)(&m, D3D10_DST_IA_PRIMITIVE_TOPOLOGY, 0, 1);
    ID3D10StateBlock *sb = NULL;
    EXPECT_HR(CSB("d3d10.dll")(dev, &m, &sb), S_OK);
    CHECK(sb != NULL);
    if (sb) {
        EQV(ID3D10StateBlock_AddRef(sb), 2);
        EQV(ID3D10StateBlock_Release(sb), 1);
        ID3D10Device *d = NULL;
        EXPECT_HR(ID3D10StateBlock_GetDevice(sb, &d), S_OK);
        EXPECT_EQ(d, dev);
        SAFE_RELEASE(d);
        EXPECT_HR(ID3D10StateBlock_GetDevice(sb, NULL), E_INVALIDARG_);
        EQV(ID3D10StateBlock_Release(sb), 0);
    }
}
TEST(d3d10_utils_effects_sb_qi)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskDisableAll)(&m);
    UIN("d3d10.dll", D3D10StateBlockMaskEnableCapture)(&m, D3D10_DST_PS, 0, 1);
    ID3D10StateBlock *sb = NULL;
    if (FAILED(CSB("d3d10.dll")(dev, &m, &sb)) || !sb) { CHECK(0); return; }
    void *o = (void *)(ULONG_PTR)1;
    EXPECT_HR(ID3D10StateBlock_QueryInterface(sb, &IID_ID3D10Device, &o), E_NOINTERFACE);
    EXPECT_EQ(o, NULL);
    EXPECT_HR(ID3D10StateBlock_QueryInterface(sb, &IID_IUnknown, NULL), E_INVALIDARG_);
    o = NULL;
    HRESULT hr = ID3D10StateBlock_QueryInterface(sb, &IID_ID3D10StateBlock, &o);
    t_note("QI(IID_ID3D10StateBlock): %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && o) ID3D10StateBlock_Release((ID3D10StateBlock *)o);
    o = NULL;
    hr = ID3D10StateBlock_QueryInterface(sb, &IID_IUnknown, &o);
    t_note("QI(IID_IUnknown) on a state block: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && o) ID3D10StateBlock_Release((ID3D10StateBlock *)o);
    ID3D10StateBlock_Release(sb);
}
TEST(d3d10_utils_effects_sb_capture_apply_topology)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskDisableAll)(&m);
    EXPECT_HR(UIN("d3d10.dll", D3D10StateBlockMaskEnableCapture)(&m, D3D10_DST_IA_PRIMITIVE_TOPOLOGY, 0, 1), S_OK);
    ID3D10StateBlock *sb = NULL;
    EXPECT_HR(CSB("d3d10.dll")(dev, &m, &sb), S_OK);
    if (!sb) return;
    D3D10_PRIMITIVE_TOPOLOGY topo = D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    EXPECT_HR(ID3D10StateBlock_Capture(sb), S_OK);
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D10Device_IAGetPrimitiveTopology(dev, &topo);
    EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
    EXPECT_HR(ID3D10StateBlock_Apply(sb), S_OK);
    ID3D10Device_IAGetPrimitiveTopology(dev, &topo);
    EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED);
    ID3D10StateBlock_Release(sb);
}
TEST(d3d10_utils_effects_sb_uncaptured_state_untouched)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskDisableAll)(&m);
    UIN("d3d10.dll", D3D10StateBlockMaskEnableCapture)(&m, D3D10_DST_IA_PRIMITIVE_TOPOLOGY, 0, 1);
    ID3D10StateBlock *sb = NULL;
    if (FAILED(CSB("d3d10.dll")(dev, &m, &sb)) || !sb) { CHECK(0); return; }
    D3D10_VIEWPORT vp0 = { 0, 0, 33, 44, 0.0f, 1.0f }, vpg;
    UINT n = 1;
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    ID3D10Device_RSSetViewports(dev, 1, &vp0);
    EXPECT_HR(ID3D10StateBlock_Capture(sb), S_OK);
    D3D10_VIEWPORT vp1 = { 0, 0, 55, 66, 0.0f, 1.0f };
    ID3D10Device_RSSetViewports(dev, 1, &vp1);
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
    EXPECT_HR(ID3D10StateBlock_Apply(sb), S_OK);
    memset(&vpg, 0, sizeof vpg);
    ID3D10Device_RSGetViewports(dev, &n, &vpg);
    EXPECT_EQ(vpg.Width, 55);
    EXPECT_EQ(vpg.Height, 66);
    D3D10_PRIMITIVE_TOPOLOGY topo;
    ID3D10Device_IAGetPrimitiveTopology(dev, &topo);
    EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED);
    ID3D10Device_RSSetViewports(dev, 0, NULL);
    ID3D10StateBlock_Release(sb);
}
TEST(d3d10_utils_effects_sb_viewport_capture)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskDisableAll)(&m);
    UIN("d3d10.dll", D3D10StateBlockMaskEnableCapture)(&m, D3D10_DST_RS_VIEWPORTS, 0, 1);
    ID3D10StateBlock *sb = NULL;
    if (FAILED(CSB("d3d10.dll")(dev, &m, &sb)) || !sb) { CHECK(0); return; }
    D3D10_VIEWPORT a = { 1, 2, 100, 50, 0.0f, 1.0f }, b = { 0, 0, 7, 7, 0.0f, 1.0f }, g;
    UINT n = 1;
    ID3D10Device_RSSetViewports(dev, 1, &a);
    EXPECT_HR(ID3D10StateBlock_Capture(sb), S_OK);
    ID3D10Device_RSSetViewports(dev, 1, &b);
    EXPECT_HR(ID3D10StateBlock_Apply(sb), S_OK);
    memset(&g, 0, sizeof g);
    ID3D10Device_RSGetViewports(dev, &n, &g);
    EXPECT_EQ(g.TopLeftX, 1); EXPECT_EQ(g.TopLeftY, 2); EXPECT_EQ(g.Width, 100); EXPECT_EQ(g.Height, 50);
    ID3D10Device_RSSetViewports(dev, 0, NULL);
    ID3D10StateBlock_Release(sb);
}
TEST(d3d10_utils_effects_sb_release_all_device_objects)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskEnableAll)(&m);
    ID3D10StateBlock *sb = NULL;
    EXPECT_HR(CSB("d3d10.dll")(dev, &m, &sb), S_OK);
    if (!sb) return;
    EXPECT_HR(ID3D10StateBlock_Capture(sb), S_OK);
    EXPECT_HR(ID3D10StateBlock_ReleaseAllDeviceObjects(sb), S_OK);
    EXPECT_HR(ID3D10StateBlock_Apply(sb), S_OK);
    ID3D10StateBlock_Release(sb);
}
TEST(d3d10_utils_effects_sb_full_mask_capture_apply)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskEnableAll)(&m);
    ID3D10StateBlock *sb = NULL;
    EXPECT_HR(CSB("d3d10.dll")(dev, &m, &sb), S_OK);
    if (!sb) return;
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    EXPECT_HR(ID3D10StateBlock_Capture(sb), S_OK);
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
    EXPECT_HR(ID3D10StateBlock_Apply(sb), S_OK);
    D3D10_PRIMITIVE_TOPOLOGY topo;
    ID3D10Device_IAGetPrimitiveTopology(dev, &topo);
    EXPECT_EQ(topo, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D10Device_IASetPrimitiveTopology(dev, D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED);
    ID3D10StateBlock_Release(sb);
}
TEST(d3d10_utils_effects_sb_empty_mask)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskDisableAll)(&m);
    ID3D10StateBlock *sb = NULL;
    HRESULT hr = CSB("d3d10.dll")(dev, &m, &sb);
    t_note("CreateStateBlock with an empty mask: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr) && sb) {
        EXPECT_HR(ID3D10StateBlock_Capture(sb), S_OK);
        EXPECT_HR(ID3D10StateBlock_Apply(sb), S_OK);
        ID3D10StateBlock_Release(sb);
    }
}
TEST(d3d10_utils_effects_sb_holds_device_reference)
{
    REQUIRE(CSB("d3d10.dll"));
    NEED_REAL_DEVICE(dev);
    D3D10_STATE_BLOCK_MASK m;
    UIN("d3d10.dll", D3D10StateBlockMaskDisableAll)(&m);
    UIN("d3d10.dll", D3D10StateBlockMaskEnableCapture)(&m, D3D10_DST_PS, 0, 1);
    ID3D10Device_AddRef(dev);
    ULONG before = ID3D10Device_Release(dev);
    ID3D10StateBlock *sb = NULL;
    if (FAILED(CSB("d3d10.dll")(dev, &m, &sb)) || !sb) { CHECK(0); return; }
    ID3D10Device_AddRef(dev);
    ULONG during = ID3D10Device_Release(dev);
    CHECK(during >= before);
    t_note("device refcount %u -> %u with a state block", (unsigned)before, (unsigned)during);
    ID3D10StateBlock_Release(sb);
    ID3D10Device_AddRef(dev);
    ULONG after = ID3D10Device_Release(dev);
    EXPECT_EQ(after, before);
}

/* ---- constant buffer objects ---- */
TEST(d3d10_utils_effects_fxo_cb_set_get_buffer)
{
    NEED_REAL_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectConstantBuffer *cb = ID3D10Effect_GetConstantBufferByName(fx, "cb");
    D3D10_BUFFER_DESC bd;
    memset(&bd, 0, sizeof bd);
    bd.ByteWidth = 176; bd.Usage = D3D10_USAGE_DEFAULT; bd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
    ID3D10Buffer *mine = NULL, *orig = NULL, *got = NULL;
    if (FAILED(ID3D10Device_CreateBuffer(dev, &bd, NULL, &mine))) { t_skip("cannot create buffer"); ID3D10Effect_Release(fx); return; }
    EXPECT_HR(ID3D10EffectConstantBuffer_GetConstantBuffer(cb, &orig), S_OK);
    EXPECT_HR(ID3D10EffectConstantBuffer_SetConstantBuffer(cb, mine), S_OK);
    EXPECT_HR(ID3D10EffectConstantBuffer_GetConstantBuffer(cb, &got), S_OK);
    EXPECT_EQ(got, mine);
    SAFE_RELEASE(got);
    EXPECT_HR(ID3D10EffectConstantBuffer_SetConstantBuffer(cb, orig), S_OK);
    EXPECT_HR(ID3D10EffectConstantBuffer_GetConstantBuffer(cb, &got), S_OK);
    EXPECT_EQ(got, orig);
    SAFE_RELEASE(got);
    SAFE_RELEASE(orig);
    ID3D10Buffer_Release(mine);
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_cb_variable_methods_on_cbuffer_object)
{
    NEED_DEVICE(dev);
    FX_OPEN(fx, UE_FX_BASIC);
    ID3D10EffectConstantBuffer *cb = ID3D10Effect_GetConstantBufferByName(fx, "cb");
    ID3D10EffectVariable *as_var = (ID3D10EffectVariable *)cb;
    ID3D10EffectConstantBuffer *back = ID3D10EffectVariable_AsConstantBuffer(as_var);
    CHECK(back && ID3D10EffectConstantBuffer_IsValid(back));
    CHECK(!ID3D10EffectScalarVariable_IsValid(ID3D10EffectVariable_AsScalar(as_var)));
    CHECK(!ID3D10EffectVectorVariable_IsValid(ID3D10EffectVariable_AsVector(as_var)));
    CHECK(!ID3D10EffectMatrixVariable_IsValid(ID3D10EffectVariable_AsMatrix(as_var)));
    CHECK(!ID3D10EffectShaderResourceVariable_IsValid(ID3D10EffectVariable_AsShaderResource(as_var)));
    ID3D10Effect_Release(fx);
}
TEST(d3d10_utils_effects_fxo_tbuffer_texture_buffer)
{
    NEED_DEVICE(dev);
    const char *src = "tbuffer tb { float4 tdata[4]; };\ntechnique10 T { pass P { } }\n";
    FX_OPEN(fx, src);
    ID3D10EffectConstantBuffer *cb = ID3D10Effect_GetConstantBufferByName(fx, "tb");
    CHECK(cb && ID3D10EffectConstantBuffer_IsValid(cb));
    D3D10_EFFECT_TYPE_DESC td;
    if (cb) {
        EXPECT_HR(ID3D10EffectType_GetDesc(ID3D10EffectVariable_GetType((ID3D10EffectVariable *)cb), &td), S_OK);
        t_note("tbuffer type: name %s class %d", td.TypeName ? td.TypeName : "(null)", td.Class);
    }
    ID3D10Effect_Release(fx);
}
