/* Shader-reflection types of class-linkage (interface/class) variables: ID3D11ShaderReflectionType::GetNumInterfaces, GetInterfaceByIndex,
 * ImplementsInterface, IsOfType, GetBaseClass, GetSubType and ID3D11ShaderReflection::GetNumInterfaceSlots. */
#include "rb.h"

static const char *iface_src =
    "interface iBase { float4 Get(); };\n"
    "class cA : iBase { float4 c; float4 Get() { return c; } };\n"
    "class cB : iBase { float4 c2; float4 Get() { return c2 * 2; } };\n"
    "class cC : cA { float4 c3; float4 Get() { return c3 + c; } };\n"
    "iBase g_obj;\n"
    "iBase g_arr[2];\n"
    "cA g_a;\n"
    "cB g_b;\n"
    "cC g_c;\n"
    "float4 main() : SV_Target { return g_obj.Get() + g_arr[1].Get() + g_a.Get() + g_b.Get() + g_c.Get(); }\n";

static ID3D11ShaderReflection *iface_refl(ID3DBlob **blob) {
    *blob = NULL; ID3D11ShaderReflection *r = NULL;
    if (FAILED(d3dh_compile(iface_src, "main", "ps_5_0", 0, blob)) || !*blob) return NULL;
    __typeof__(&D3DReflect) rf = (__typeof__(&D3DReflect))d3dh_getproc("d3dcompiler_47.dll", "D3DReflect");
    if (!rf) return NULL;
    if (FAILED(rf(ID3D10Blob_GetBufferPointer(*blob), ID3D10Blob_GetBufferSize(*blob), &IID_ID3D11ShaderReflection, (void **)&r))) return NULL;
    return r;
}
static ID3D11ShaderReflectionType *var_type(ID3D11ShaderReflection *r, const char *name) {
    ID3D11ShaderReflectionVariable *v = r->lpVtbl->GetVariableByName(r, name);
    return v ? v->lpVtbl->GetType(v) : NULL;
}

#define TY(r, n) rb_it_type((r), (n))
static ID3D11ShaderReflectionType *rb_it_type(ID3D11ShaderReflection *r, const char *name) {
    ID3D11ShaderReflectionVariable *v = r->lpVtbl->GetVariableByName(r, name);
    D3D11_SHADER_VARIABLE_DESC vd; memset(&vd, 0, sizeof vd);
    if (!v || FAILED(v->lpVtbl->GetDesc(v, &vd))) return NULL;
    return v->lpVtbl->GetType(v);
}
static const char *tname(ID3D11ShaderReflectionType *t) { static char b[4][64]; static int k; D3D11_SHADER_TYPE_DESC d; memset(&d, 0, sizeof d); if (t) t->lpVtbl->GetDesc(t, &d); k = (k + 1) & 3; snprintf(b[k], 64, "%s", d.Name ? d.Name : "-"); return b[k]; }

TEST(rb_iface_types_class_linkage_reflection) {
    ID3DBlob *b; ID3D11ShaderReflection *r = iface_refl(&b);
    if (!r) { t_skip("no compiler / reflection (%s)", d3dh_compile_errors()); return; }
    D3D11_SHADER_DESC sd; EXPECT_HR(r->lpVtbl->GetDesc(r, &sd), S_OK);
    EXPECT_EQ(sd.ConstantBuffers, 2);
    EXPECT_EQ(r->lpVtbl->GetNumInterfaceSlots(r), 3);                       /* g_obj plus g_arr[2] */
    /* cbuffer 0 is $Globals (the class instances), cbuffer 1 is $ThisPointer of type D3D_CT_INTERFACE_POINTERS (the interface variables) */
    ID3D11ShaderReflectionConstantBuffer *g = r->lpVtbl->GetConstantBufferByIndex(r, 0), *tp = r->lpVtbl->GetConstantBufferByIndex(r, 1);
    D3D11_SHADER_BUFFER_DESC gd, td; memset(&gd, 0, sizeof gd); memset(&td, 0, sizeof td);
    EXPECT_HR(g->lpVtbl->GetDesc(g, &gd), S_OK); EXPECT_HR(tp->lpVtbl->GetDesc(tp, &td), S_OK);
    EXPECT_STR(gd.Name, "$Globals"); EXPECT_EQ(gd.Type, D3D_CT_CBUFFER); EXPECT_EQ(gd.Variables, 3); EXPECT_EQ(gd.Size, 64);
    EXPECT_STR(td.Name, "$ThisPointer"); EXPECT_EQ(td.Type, D3D_CT_INTERFACE_POINTERS); EXPECT_EQ(td.Variables, 2);
    ID3D11ShaderReflectionType *ta = TY(r, "g_a"), *tb = TY(r, "g_b"), *tc = TY(r, "g_c"), *po = TY(r, "g_obj"), *pa = TY(r, "g_arr");
    CHECK(ta && tb && tc && po && pa);
    if (!(ta && tb && tc && po && pa)) { r->lpVtbl->Release(r); ID3D10Blob_Release(b); return; }
    D3D11_SHADER_TYPE_DESC d;
    EXPECT_HR(ta->lpVtbl->GetDesc(ta, &d), S_OK); EXPECT_STR(d.Name, "cA"); EXPECT_EQ(d.Class, D3D_SVC_STRUCT); EXPECT_EQ(d.Members, 1); EXPECT_EQ(d.Columns, 4);
    EXPECT_HR(tc->lpVtbl->GetDesc(tc, &d), S_OK); EXPECT_STR(d.Name, "cC"); EXPECT_EQ(d.Members, 2); EXPECT_EQ(d.Columns, 8);
    EXPECT_HR(po->lpVtbl->GetDesc(po, &d), S_OK); EXPECT_STR(d.Name, "iBase"); EXPECT_EQ(d.Class, D3D_SVC_INTERFACE_POINTER); EXPECT_EQ(d.Type, D3D_SVT_INTERFACE_POINTER); EXPECT_EQ(d.Elements, 0);
    EXPECT_HR(pa->lpVtbl->GetDesc(pa, &d), S_OK); EXPECT_EQ(d.Elements, 2);
    /* classes list the interfaces they implement directly; the derived class lists none but still implements them through its base */
    EXPECT_EQ(ta->lpVtbl->GetNumInterfaces(ta), 1); EXPECT_EQ(tb->lpVtbl->GetNumInterfaces(tb), 1); EXPECT_EQ(tc->lpVtbl->GetNumInterfaces(tc), 0); EXPECT_EQ(po->lpVtbl->GetNumInterfaces(po), 0);
    ID3D11ShaderReflectionType *ia = ta->lpVtbl->GetInterfaceByIndex(ta, 0), *ib = tb->lpVtbl->GetInterfaceByIndex(tb, 0), *bc = tc->lpVtbl->GetBaseClass(tc);
    CHECK(ia && ib && bc);
    if (ia && ib && bc) {
        EXPECT_STR(tname(ia), "iBase"); EXPECT_STR(tname(bc), "cA");
        EXPECT_HR(ia->lpVtbl->IsEqual(ia, ib), S_OK);                            /* both classes report the same interface type object */
        EXPECT_HR(ta->lpVtbl->ImplementsInterface(ta, ia), S_OK); EXPECT_HR(tb->lpVtbl->ImplementsInterface(tb, ia), S_OK);
        EXPECT_HR(tc->lpVtbl->ImplementsInterface(tc, ia), S_OK);               /* inherited from cA */
        EXPECT_HR(ia->lpVtbl->ImplementsInterface(ia, ia), S_FALSE);            /* an interface does not implement itself */
        EXPECT_HR(ta->lpVtbl->ImplementsInterface(ta, ta), S_FALSE); EXPECT_HR(ta->lpVtbl->ImplementsInterface(ta, tb), S_FALSE);
        EXPECT_HR(tc->lpVtbl->IsOfType(tc, bc), S_OK);                           /* derived IsOfType base */
        EXPECT_HR(bc->lpVtbl->IsOfType(bc, tc), S_FALSE);
        EXPECT_HR(tc->lpVtbl->IsOfType(tc, tc), S_OK); EXPECT_HR(ta->lpVtbl->IsOfType(ta, ta), S_OK);
        EXPECT_HR(ta->lpVtbl->IsOfType(ta, tb), S_FALSE); EXPECT_HR(ta->lpVtbl->IsOfType(ta, ia), S_FALSE);
        EXPECT_HR(tc->lpVtbl->IsEqual(tc, bc), S_FALSE);
        EXPECT_HR(po->lpVtbl->IsEqual(po, ia), S_FALSE);                         /* the interface-pointer variable type differs from the interface type */
        EXPECT_HR(po->lpVtbl->IsEqual(po, po), S_OK); EXPECT_HR(po->lpVtbl->IsEqual(po, pa), S_FALSE);
        EXPECT_HR(po->lpVtbl->IsOfType(po, po), S_OK);
    }
    EXPECT_EQ(ta->lpVtbl->GetBaseClass(ta) == NULL, 1);
    /* the base class appears as the first member "$super" */
    EXPECT_STR(tc->lpVtbl->GetMemberTypeName(tc, 0), "$super"); EXPECT_STR(tc->lpVtbl->GetMemberTypeName(tc, 1), "c3");
    r->lpVtbl->Release(r); ID3D10Blob_Release(b);
}
