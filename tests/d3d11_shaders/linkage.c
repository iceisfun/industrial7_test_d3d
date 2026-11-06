/* Class linkage, class instances, interface slots (dynamic shader linking). */
#include "sh_common.h"
#include <d3d11shader.h>

#define NEED_IFACE_PS(e, b) SH_ENV(e, 0xb000); SH_BLOB(b, "ps_interface")

static ID3D11PixelShader *mk_iface_ps(SHENV *e, ID3DBlob *b, ID3D11ClassLinkage *cl, HRESULT *hr_out)
{
    ID3D11PixelShader *ps = NULL;
    HRESULT hr = ID3D11Device_CreatePixelShader(e->dev, SH_BP(b), SH_BN(b), cl, &ps);
    if (hr_out) *hr_out = hr;
    return ps;
}

TEST(d3d11_shaders_linkage_create)
{
    SH_ENV(e, 0);
    ID3D11ClassLinkage *cl = (ID3D11ClassLinkage *)(ULONG_PTR)1;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    CHECK(cl != NULL && cl != (void *)(ULONG_PTR)1);
    if (cl) EXPECT_EQ(d3dh_refcount(cl), 1);
    SH_REL(cl); sh_env_free(&e);
}
/* CreateClassLinkage(NULL) is not validated by Win10 22H2 d3d11.dll: the call raises an access violation. The call is made in
 * a child process; an implementation must not report success (it may return an error or fault like Windows). */
CHILD(linkage_null_out)
{
    SHENV e; if (!sh_env(&e, 0)) return 3;
    HRESULT hr = ID3D11Device_CreateClassLinkage(e.dev, NULL);
    return SUCCEEDED(hr) ? 1 : 0;
}
TEST(d3d11_shaders_linkage_create_null_out)
{
    DWORD rc = t_run_child("linkage_null_out", NULL, 60000);
    t_note("CreateClassLinkage(NULL) child exit %#lx (Win10: 0xc0000005)", rc);
    t_check(rc == 0 || rc == 0xC0000005, __FILE__, __LINE__, "CreateClassLinkage(NULL) fails or faults", "child exit %#lx", rc);
}
TEST(d3d11_shaders_linkage_distinct_objects)
{
    SH_ENV(e, 0); ID3D11ClassLinkage *a = NULL, *b = NULL;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &a), S_OK); EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &b), S_OK);
    CHECK(a && b && a != b); SH_REL(a); SH_REL(b); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_queryinterface)
{
    SH_ENV(e, 0); ID3D11ClassLinkage *cl = NULL; void *q = NULL;
    if (FAILED(ID3D11Device_CreateClassLinkage(e.dev, &cl))) { CHECK(0); return; }
    EXPECT_HR(IUnknown_QueryInterface((IUnknown *)cl, &IID_ID3D11ClassLinkage, &q), S_OK); EXPECT_EQ(q, cl); SH_REL(q);
    EXPECT_HR(IUnknown_QueryInterface((IUnknown *)cl, &IID_ID3D11DeviceChild, &q), S_OK); EXPECT_EQ(q, cl); SH_REL(q);
    EXPECT_HR(IUnknown_QueryInterface((IUnknown *)cl, &IID_IUnknown, &q), S_OK); SH_REL(q);
    EXPECT_HR(IUnknown_QueryInterface((IUnknown *)cl, &IID_ID3D11ClassInstance, &q), E_NOINTERFACE);
    EXPECT_HR(IUnknown_QueryInterface((IUnknown *)cl, &IID_ID3D11PixelShader, &q), E_NOINTERFACE);
    SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_getdevice)
{
    SH_ENV(e, 0); ID3D11ClassLinkage *cl = NULL; ID3D11Device *d = NULL;
    if (FAILED(ID3D11Device_CreateClassLinkage(e.dev, &cl))) { CHECK(0); return; }
    ULONG before = d3dh_refcount(e.dev);
    ID3D11ClassLinkage_GetDevice(cl, &d); CHECK(d == e.dev); EXPECT_EQ(d3dh_refcount(e.dev), before + 1); SH_REL(d);
    SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_private_data)
{
    SH_ENV(e, 0); ID3D11ClassLinkage *cl = NULL;
    if (FAILED(ID3D11Device_CreateClassLinkage(e.dev, &cl))) { CHECK(0); return; }
    static const GUID g = { 0x77aa, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };
    UINT sz = 8; char buf[8];
    EXPECT_HR(ID3D11ClassLinkage_GetPrivateData(cl, &g, &sz, buf), DXGI_ERROR_NOT_FOUND);
    EXPECT_HR(ID3D11ClassLinkage_SetPrivateData(cl, &g, 4, "abc"), S_OK);
    sz = 8; EXPECT_HR(ID3D11ClassLinkage_GetPrivateData(cl, &g, &sz, buf), S_OK); EXPECT_EQ(sz, 4); EXPECT_STR(buf, "abc");
    SH_REL(cl); sh_env_free(&e);
}
/* GetClassInstance never fails on Win10 22H2: it hands out a handle for any name, even on a linkage with no shader and for
 * names that are not in any shader. Instances for distinct names are distinct objects; the type name is empty. */
TEST(d3d11_shaders_linkage_get_unknown_instance_succeeds)
{
    SH_ENV(e, 0); ID3D11ClassLinkage *cl = NULL; ID3D11ClassInstance *ci = NULL, *cj = NULL;
    if (FAILED(ID3D11Device_CreateClassLinkage(e.dev, &cl))) { CHECK(0); return; }
    EXPECT_HR(ID3D11ClassLinkage_GetClassInstance(cl, "noSuchInstance", 0, &ci), S_OK);
    EXPECT_HR(ID3D11ClassLinkage_GetClassInstance(cl, "otherName", 0, &cj), S_OK);
    CHECK(ci != NULL && cj != NULL && ci != cj);
    if (ci) {
        char nm[64] = "?"; SIZE_T len = sizeof nm;
        ID3D11ClassInstance_GetInstanceName(ci, nm, &len); EXPECT_STR(nm, "noSuchInstance"); EXPECT_EQ(len, 15);
        len = sizeof nm; ID3D11ClassInstance_GetTypeName(ci, nm, &len); EXPECT_STR(nm, ""); EXPECT_EQ(len, 1);
    }
    SH_REL(ci); SH_REL(cj); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_create_instance_before_shader_observed)
{
    SH_ENV(e, 0); ID3D11ClassLinkage *cl = NULL; ID3D11ClassInstance *ci = NULL;
    if (FAILED(ID3D11Device_CreateClassLinkage(e.dev, &cl))) { CHECK(0); return; }
    HRESULT hr = ID3D11ClassLinkage_CreateClassInstance(cl, "cRed", 0, 0, 0, 0, &ci);
    EXPECT_HR(hr, S_OK); CHECK(ci != NULL); SH_REL(ci); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_shader_with_interfaces_create)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr);
    EXPECT_HR(hr, S_OK); CHECK(ps != NULL);
    SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_shader_with_interfaces_null_linkage_observed)
{
    NEED_IFACE_PS(e, b); HRESULT hr;
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, NULL, &hr);
    /* a shader with interface slots needs a class linkage: E_INVALIDARG without one, on the validation path too */
    EXPECT_HR(hr, E_INVALIDARG); CHECK(ps == NULL);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(b), SH_BN(b), NULL, NULL), E_INVALIDARG);
    SH_REL(ps); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_null_out_validates)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    EXPECT_HR(ID3D11Device_CreatePixelShader(e.dev, SH_BP(b), SH_BN(b), cl, NULL), S_FALSE);
    SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_get_named_instances)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); if (!ps) { CHECK(0); return; }
    const char *names[] = { "g_red", "g_blue", "g_color" };
    for (int i = 0; i < 3; i++) {
        ID3D11ClassInstance *ci = NULL; hr = ID3D11ClassLinkage_GetClassInstance(cl, names[i], 0, &ci);
        t_note("GetClassInstance(%s,0) -> %s", names[i], d3dh_hr_name(hr));
        if (i < 2) { EXPECT_HR(hr, S_OK); CHECK(ci != NULL); }
        SH_REL(ci);
    }
    SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_get_instance_desc)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); if (!ps) { CHECK(0); return; }
    ID3D11ClassInstance *ci = NULL;
    EXPECT_HR(ID3D11ClassLinkage_GetClassInstance(cl, "g_red", 0, &ci), S_OK);
    if (ci) {
        D3D11_CLASS_INSTANCE_DESC d; memset(&d, 0xcc, sizeof d);
        ID3D11ClassInstance_GetDesc(ci, &d);
        CHECK(d.Created == FALSE);
        t_note("g_red desc: InstanceId %u InstanceIndex %u TypeId %u CB %u CBoff %u tex %u smp %u", d.InstanceId, d.InstanceIndex, d.TypeId, d.ConstantBuffer, d.BaseConstantBufferOffset, d.BaseTexture, d.BaseSampler);
        char nm[64] = ""; SIZE_T len = sizeof nm;
        /* an instance obtained by name carries only that name (Win10 22H2): the type name is the empty string */
        ID3D11ClassInstance_GetTypeName(ci, nm, &len); EXPECT_STR(nm, ""); EXPECT_EQ(len, 1);
        len = sizeof nm; nm[0] = 0; ID3D11ClassInstance_GetInstanceName(ci, nm, &len); EXPECT_STR(nm, "g_red"); EXPECT_EQ(len, 6);
        SIZE_T need = 0; ID3D11ClassInstance_GetTypeName(ci, NULL, &need); EXPECT_EQ(need, 1);
        need = 0; ID3D11ClassInstance_GetInstanceName(ci, NULL, &need); EXPECT_EQ(need, 6);
        len = 3; nm[0] = 'x'; ID3D11ClassInstance_GetInstanceName(ci, nm, &len); t_note("truncated GetInstanceName(len 3) -> \"%s\" len %llu", nm, (unsigned long long)len);
        ID3D11ClassLinkage *l2 = NULL; ID3D11ClassInstance_GetClassLinkage(ci, &l2); CHECK(l2 == cl); SH_REL(l2);
        ID3D11Device *dv = NULL; ID3D11ClassInstance_GetDevice(ci, &dv); CHECK(dv == e.dev); SH_REL(dv);
    }
    SH_REL(ci); SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_create_instance_desc_created_flag)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); if (!ps) { CHECK(0); return; }
    ID3D11ClassInstance *ci = NULL;
    hr = ID3D11ClassLinkage_CreateClassInstance(cl, "cBlue", 0, 0, 0, 0, &ci);
    EXPECT_HR(hr, S_OK);
    if (ci) {
        D3D11_CLASS_INSTANCE_DESC d; memset(&d, 0, sizeof d); ID3D11ClassInstance_GetDesc(ci, &d);
        CHECK(d.Created == TRUE);
        char nm[64] = ""; SIZE_T len = sizeof nm; ID3D11ClassInstance_GetTypeName(ci, nm, &len); EXPECT_STR(nm, "cBlue");
        EXPECT_EQ(d3dh_refcount(ci), 1);
    }
    SH_REL(ci); SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_create_instance_unknown_type_observed)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); if (!ps) { CHECK(0); return; }
    ID3D11ClassInstance *ci = NULL;
    hr = ID3D11ClassLinkage_CreateClassInstance(cl, "cNoSuchClass", 0, 0, 0, 0, &ci);
    EXPECT_HR(hr, S_OK); CHECK(ci != NULL);   /* the type is not checked against the shader */
    SH_REL(ci); SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_instance_queryinterface)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr; void *q = NULL;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); if (!ps) { CHECK(0); return; }
    ID3D11ClassInstance *ci = NULL;
    if (SUCCEEDED(ID3D11ClassLinkage_GetClassInstance(cl, "g_red", 0, &ci)) && ci) {
        EXPECT_HR(IUnknown_QueryInterface((IUnknown *)ci, &IID_ID3D11ClassInstance, &q), S_OK); SH_REL(q);
        EXPECT_HR(IUnknown_QueryInterface((IUnknown *)ci, &IID_ID3D11DeviceChild, &q), S_OK); SH_REL(q);
        EXPECT_HR(IUnknown_QueryInterface((IUnknown *)ci, &IID_ID3D11ClassLinkage, &q), E_NOINTERFACE);
    }
    SH_REL(ci); SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_instance_keeps_linkage_alive)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); if (!ps) { CHECK(0); return; }
    ID3D11ClassInstance *ci = NULL;
    if (SUCCEEDED(ID3D11ClassLinkage_GetClassInstance(cl, "g_blue", 0, &ci)) && ci) {
        ULONG rc = d3dh_refcount(cl);
        ID3D11ClassLinkage *l2 = NULL; ID3D11ClassInstance_GetClassLinkage(ci, &l2); CHECK(l2 == cl);
        EXPECT_EQ(d3dh_refcount(cl), rc + 1); SH_REL(l2);
        SH_REL(cl); /* app reference gone; instance and shader still hold it */
        char nm[32] = ""; SIZE_T len = sizeof nm; ID3D11ClassInstance_GetInstanceName(ci, nm, &len); EXPECT_STR(nm, "g_blue");
    }
    SH_REL(ci); SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_set_get_with_instances)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); if (!ps) { CHECK(0); return; }
    ID3D11ClassInstance *red = NULL, *phantom = NULL;
    /* An instance fetched by name (GetClassInstance) is not usable for binding on Win10 22H2: PSSetShader with it leaves NO
     * pixel shader bound at all (PSGetShader returns NULL and zero instances). */
    EXPECT_HR(ID3D11ClassLinkage_GetClassInstance(cl, "g_red", 0, &phantom), S_OK);
    if (phantom) {
        ID3D11PixelShader *gp = (ID3D11PixelShader *)(ULONG_PTR)1; ID3D11ClassInstance *got[4] = { 0 }; UINT n = 4;
        ID3D11DeviceContext_PSSetShader(e.ctx, ps, &phantom, 1);
        ID3D11DeviceContext_PSGetShader(e.ctx, &gp, got, &n);
        CHECK(gp == NULL); EXPECT_EQ(n, 0); CHECK(got[0] == NULL);
        SH_REL(gp); for (UINT i = 0; i < 4; i++) SH_REL(got[i]);
        ID3D11DeviceContext_PSSetShader(e.ctx, NULL, NULL, 0);
    }
    EXPECT_HR(ID3D11ClassLinkage_CreateClassInstance(cl, "cRed", 0, 0, 0, 0, &red), S_OK);
    if (red) {
        ID3D11DeviceContext_PSSetShader(e.ctx, ps, &red, 1);
        ID3D11PixelShader *gp = NULL; ID3D11ClassInstance *got[4] = { 0 }; UINT n = 4;
        ID3D11DeviceContext_PSGetShader(e.ctx, &gp, got, &n);
        CHECK(gp == ps); EXPECT_EQ(n, 1);
        CHECK(got[0] == red);
        for (UINT i = 0; i < 4; i++) SH_REL(got[i]);
        SH_REL(gp);
        ID3D11DeviceContext_PSSetShader(e.ctx, NULL, NULL, 0);
    }
    SH_REL(red); SH_REL(phantom); SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_get_instance_count_query)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); if (!ps) { CHECK(0); return; }
    ID3D11ClassInstance *blue = NULL;
    if (SUCCEEDED(ID3D11ClassLinkage_CreateClassInstance(cl, "cBlue", 0, 0, 0, 0, &blue)) && blue) {
        ID3D11DeviceContext_PSSetShader(e.ctx, ps, &blue, 1);
        ID3D11PixelShader *gp = NULL; UINT n = 0;
        ID3D11DeviceContext_PSGetShader(e.ctx, &gp, NULL, &n);
        EXPECT_EQ(n, 1); /* NULL array: the count of bound instances is still reported */
        CHECK(gp == ps); SH_REL(gp);
        ID3D11DeviceContext_PSSetShader(e.ctx, NULL, NULL, 0);
    }
    SH_REL(blue); SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_two_shaders_share_linkage)
{
    NEED_IFACE_PS(e, b); ID3D11ClassLinkage *cl = NULL; HRESULT h1, h2;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *p1 = mk_iface_ps(&e, b, cl, &h1), *p2 = mk_iface_ps(&e, b, cl, &h2);
    EXPECT_HR(h1, S_OK); EXPECT_HR(h2, S_OK); CHECK(p1 != p2);
    ID3D11ClassInstance *a = NULL, *c = NULL;
    HRESULT ha = ID3D11ClassLinkage_GetClassInstance(cl, "g_red", 0, &a), hc = ID3D11ClassLinkage_GetClassInstance(cl, "g_red", 0, &c);
    EXPECT_HR(ha, S_OK); EXPECT_HR(hc, S_OK);
    t_note("GetClassInstance twice returns %s object", a == c ? "the same" : "different");
    SH_REL(a); SH_REL(c); SH_REL(p1); SH_REL(p2); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_vertex_shader_with_linkage_and_no_interfaces)
{
    SH_ENV(e, 0); SH_BLOB(b, "vs_pass"); ID3D11ClassLinkage *cl = NULL; ID3D11VertexShader *vs = NULL; ID3D11ClassInstance *ci = NULL;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(b), SH_BN(b), cl, &vs), S_OK);
    /* GetClassInstance hands out a handle regardless of the shader (see get_unknown_instance_succeeds) */
    HRESULT hr = ID3D11ClassLinkage_GetClassInstance(cl, "g_red", 0, &ci);
    t_check(hr == S_OK && ci, __FILE__, __LINE__, "instance handle from an interface-less shader's linkage", "%s", d3dh_hr_name(hr));
    SH_REL(ci); SH_REL(vs); SH_REL(cl); sh_env_free(&e);
}
TEST(d3d11_shaders_linkage_reflection_interface_slots)
{
    REQUIRE(d3dh_D3DReflect());
    SH_BLOB(b, "ps_interface");
    ID3D11ShaderReflection *r = NULL;
    HRESULT hr = d3dh_D3DReflect()(SH_BP(b), SH_BN(b), &IID_ID3D11ShaderReflection, (void **)&r);
    if (FAILED(hr)) { t_skip("D3DReflect: %s", d3dh_hr_name(hr)); return; }
    UINT slots = r->lpVtbl->GetNumInterfaceSlots(r);
    EXPECT_EQ(slots, 1);
    D3D11_SHADER_DESC d; if (SUCCEEDED(r->lpVtbl->GetDesc(r, &d))) t_note("shader desc: interface slots %u instr %u", slots, d.InstructionCount);
    r->lpVtbl->Release(r);
}
TEST(d3d11_shaders_linkage_dynamic_dispatch_red_blue)
{
    NEED_IFACE_PS(e, b); SH_BLOB(vb, "vs_noinput");
    ID3D11ClassLinkage *cl = NULL; HRESULT hr;
    EXPECT_HR(ID3D11Device_CreateClassLinkage(e.dev, &cl), S_OK);
    ID3D11PixelShader *ps = mk_iface_ps(&e, b, cl, &hr); ID3D11VertexShader *vs = NULL;
    EXPECT_HR(ID3D11Device_CreateVertexShader(e.dev, SH_BP(vb), SH_BN(vb), NULL, &vs), S_OK);
    ID3D11ClassInstance *inst[2] = { NULL, NULL };
    ID3D11ClassLinkage_CreateClassInstance(cl, "cRed", 0, 0, 0, 0, &inst[0]); ID3D11ClassLinkage_CreateClassInstance(cl, "cBlue", 0, 0, 0, 0, &inst[1]);
    D3D11_TEXTURE2D_DESC td = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
    ID3D11Texture2D *tex = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11RasterizerState *rs = NULL;
    EXPECT_HR(ID3D11Device_CreateTexture2D(e.dev, &td, NULL, &tex), S_OK);
    if (!ps || !vs || !inst[0] || !inst[1] || !tex) { CHECK(0); return; }
    EXPECT_HR(ID3D11Device_CreateRenderTargetView(e.dev, (ID3D11Resource *)tex, NULL, &rtv), S_OK);
    D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd); rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    EXPECT_HR(ID3D11Device_CreateRasterizerState(e.dev, &rd, &rs), S_OK);
    D3D11_VIEWPORT vp = { 0, 0, 4, 4, 0, 1 };
    ID3D11DeviceContext_OMSetRenderTargets(e.ctx, 1, &rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(e.ctx, 1, &vp); ID3D11DeviceContext_RSSetState(e.ctx, rs);
    ID3D11DeviceContext_IASetPrimitiveTopology(e.ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(e.ctx, vs, NULL, 0);
    static const UINT32 want[2] = { 0xff0000ffu, 0xffff0000u }; /* R8G8B8A8: red, blue */
    for (int k = 0; k < 2; k++) {
        ID3D11DeviceContext_PSSetShader(e.ctx, ps, &inst[k], 1);
        ID3D11DeviceContext_Draw(e.ctx, 3, 0);
        d3dh_image im;
        if (SUCCEEDED(d3dh_readback_texture2d11(e.dev, e.ctx, tex, 0, &im))) {
            for (UINT y = 0; y < 4; y++) for (UINT x = 0; x < 4; x++)
                t_check(d3dh_image_pixel32(&im, x, y) == want[k], __FILE__, __LINE__, "dynamic linking colour", "instance %d pixel(%u,%u)=0x%08x want 0x%08x", k, x, y, d3dh_image_pixel32(&im, x, y), want[k]);
            d3dh_image_free(&im);
        } else CHECK(0);
    }
    ID3D11DeviceContext_PSSetShader(e.ctx, NULL, NULL, 0);
    SH_REL(rs); SH_REL(rtv); SH_REL(tex); SH_REL(inst[0]); SH_REL(inst[1]); SH_REL(vs); SH_REL(ps); SH_REL(cl); sh_env_free(&e);
}
