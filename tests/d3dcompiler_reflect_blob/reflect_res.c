/* d3dcompiler_47 D3DReflect: shader description, bound resources, signatures, feature level and requirement queries. */
#include "rb.h"

static int rb_desc(ID3D11ShaderReflection *r, D3D11_SHADER_DESC *d)
{
    HRESULT hr; memset(d, 0, sizeof *d); hr = r->lpVtbl->GetDesc(r, d);
    t_check(SUCCEEDED(hr), __FILE__, __LINE__, "shader desc", "%s", d3dh_hr_name(hr));
    return SUCCEEDED(hr);
}
static int rb_bind(ID3D11ShaderReflection *r, const char *name, D3D11_SHADER_INPUT_BIND_DESC *b)
{
    HRESULT hr; memset(b, 0, sizeof *b); hr = r->lpVtbl->GetResourceBindingDescByName(r, name, b);
    t_check(SUCCEEDED(hr), __FILE__, __LINE__, "binding desc by name", "%s: %s", name, d3dh_hr_name(hr));
    return SUCCEEDED(hr);
}

/* ---- program version per target ---------------------------------------------------------------- */
#define VER_CASE(nm, target, src, want) \
TEST(rb_desc_version_##nm) { RB_REFLECT(r, src, target); D3D11_SHADER_DESC d; if (rb_desc(r, &d)) { t_check(d.Version == (want), __FILE__, __LINE__, "Version", "%s got 0x%x want 0x%x", target, d.Version, (UINT)(want)); } RB_DONE(r); }
#define PS_SRC "float4 main() : SV_Target { return float4(1, 0, 0, 1); }\n"
#define VS_SRC "float4 main(float4 p : POSITION) : SV_Position { return p; }\n"
#define CS_SRC "RWBuffer<uint> u : register(u0);\n[numthreads(1, 1, 1)] void main(uint3 t : SV_DispatchThreadID) { u[t.x] = t.x; }\n"
#define GS_SRC "struct V { float4 p : SV_Position; };\n[maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> s) { s.Append(i[0]); s.Append(i[1]); s.Append(i[2]); s.RestartStrip(); }\n"
#define HS_SRC "struct VO { float4 p : POSITION; };\nstruct HC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };\n" \
    "HC pcf(InputPatch<VO, 3> ip) { HC o; o.e[0] = o.e[1] = o.e[2] = 1; o.i = 1; return o; }\n" \
    "[domain(\"tri\")][partitioning(\"integer\")][outputtopology(\"triangle_cw\")][outputcontrolpoints(3)][patchconstantfunc(\"pcf\")]\n" \
    "VO main(InputPatch<VO, 3> ip, uint id : SV_OutputControlPointID) { return ip[id]; }\n"
#define DS_SRC "struct VO { float4 p : POSITION; };\nstruct HC { float e[3] : SV_TessFactor; float i : SV_InsideTessFactor; };\n" \
    "[domain(\"tri\")] float4 main(HC hc, float3 loc : SV_DomainLocation, const OutputPatch<VO, 3> p) : SV_Position { return p[0].p * loc.x + p[1].p * loc.y + p[2].p * loc.z; }\n"
VER_CASE(ps_4_0, "ps_4_0", PS_SRC, 0x00000040)
VER_CASE(ps_4_1, "ps_4_1", PS_SRC, 0x00000041)
VER_CASE(ps_5_0, "ps_5_0", PS_SRC, 0x00000050)
VER_CASE(vs_4_0, "vs_4_0", VS_SRC, 0x00010040)
VER_CASE(vs_4_1, "vs_4_1", VS_SRC, 0x00010041)
VER_CASE(vs_5_0, "vs_5_0", VS_SRC, 0x00010050)
VER_CASE(gs_4_0, "gs_4_0", GS_SRC, 0x00020040)
VER_CASE(gs_5_0, "gs_5_0", GS_SRC, 0x00020050)
VER_CASE(hs_5_0, "hs_5_0", HS_SRC, 0x00030050)
VER_CASE(ds_5_0, "ds_5_0", DS_SRC, 0x00040050)
VER_CASE(cs_4_0, "cs_4_0", "RWByteAddressBuffer u : register(u0);\n[numthreads(1, 1, 1)] void main(uint3 t : SV_DispatchThreadID) { u.Store(0, t.x); }\n", 0x00050040)
VER_CASE(cs_5_0, "cs_5_0", CS_SRC, 0x00050050)

TEST(rb_desc_creator_string)
{
    RB_REFLECT(r, PS_SRC, "ps_5_0"); D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) {
        CHECK(d.Creator != NULL);
        if (d.Creator) { t_note("Creator = \"%s\"", d.Creator); if (!t_is_wine()) CHECK(!strncmp(d.Creator, "Microsoft (R) HLSL Shader Compiler", 34)); }
        t_note("Flags = 0x%x", d.Flags);
    }
    RB_DONE(r);
}
TEST(rb_desc_null_pointer_and_refcount)
{
    RB_REFLECT(r, PS_SRC, "ps_5_0");
    EXPECT_HR(r->lpVtbl->GetDesc(r, NULL), RB_E_FAIL);
    { ULONG a = r->lpVtbl->AddRef(r); EXPECT_EQ(a, 2); a = r->lpVtbl->AddRef(r); EXPECT_EQ(a, 3); a = r->lpVtbl->Release(r); EXPECT_EQ(a, 2); a = r->lpVtbl->Release(r); EXPECT_EQ(a, 1); }
    RB_DONE(r);
}
TEST(rb_desc_query_interface)
{
    RB_REFLECT(r, PS_SRC, "ps_5_0"); void *o;
    o = NULL; EXPECT_HR(r->lpVtbl->QueryInterface(r, &IID_ID3D11ShaderReflection, &o), S_OK); CHECK(o == (void *)r); if (o) r->lpVtbl->Release(r);
    o = NULL; EXPECT_HR(r->lpVtbl->QueryInterface(r, &IID_IUnknown, &o), S_OK); CHECK(o == (void *)r); if (o) r->lpVtbl->Release(r);
    o = NULL; EXPECT_HR(r->lpVtbl->QueryInterface(r, &RB_IID_ShaderReflection12, &o), S_OK); CHECK(o == (void *)r); if (o) r->lpVtbl->Release(r);
    o = (void *)1; EXPECT_HR(r->lpVtbl->QueryInterface(r, &IID_ID3D11Device, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
    o = (void *)1; EXPECT_HR(r->lpVtbl->QueryInterface(r, &IID_ID3D10Blob, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
    o = (void *)1; EXPECT_HR(r->lpVtbl->QueryInterface(r, &RB_IID_LibraryReflection, &o), RB_E_NOINTERFACE); CHECK(o == NULL);
    if (!t_is_wine()) EXPECT_HR(r->lpVtbl->QueryInterface(r, &IID_IUnknown, NULL), RB_E_FAIL);
    { ULONG rc = r->lpVtbl->AddRef(r); EXPECT_EQ(rc, 2); r->lpVtbl->Release(r); }
    RB_DONE(r);
}
TEST(rb_desc_reflect_with_d3d12_iid)
{
    RB_COMPILE(b, PS_SRC, "ps_5_0"); void *o = NULL; ID3D11ShaderReflection *r;
    EXPECT_HR(d3dh_D3DReflect()(RB_BP(b), RB_BS(b), &RB_IID_ShaderReflection12, &o), S_OK);
    r = o;
    if (r) { D3D11_SHADER_DESC d; if (rb_desc(r, &d)) EXPECT_EQ(d.Version, 0x50); r->lpVtbl->Release(r); }
    ID3D10Blob_Release(b);
}
TEST(rb_desc_reflect_null_output_pointer)
{
    RB_COMPILE(b, PS_SRC, "ps_5_0");
    RB_NOWINE("D3DReflect(NULL out)");
    HRESULT hr = d3dh_D3DReflect()(RB_BP(b), RB_BS(b), &IID_ID3D11ShaderReflection, NULL);
    EXPECT_HR(hr, RB_E_FAIL);
    ID3D10Blob_Release(b);
}
TEST(rb_desc_reflect_refcount_is_one)
{
    RB_COMPILE(b, PS_SRC, "ps_5_0"); ID3D11ShaderReflection *r = rb_reflect(b);
    if (r) { ULONG n = r->lpVtbl->AddRef(r); EXPECT_EQ(n, 2); n = r->lpVtbl->Release(r); EXPECT_EQ(n, 1); n = r->lpVtbl->Release(r); EXPECT_EQ(n, 0); }
    ID3D10Blob_Release(b);
}
TEST(rb_desc_reflector_outlives_blob)
{
    RB_COMPILE(b, "cbuffer CB { float4 keep; };\nfloat4 main() : SV_Target { return keep; }\n", "ps_5_0"); ID3D11ShaderReflection *r = rb_reflect(b);
    if (!r) { ID3D10Blob_Release(b); return; }
    ID3D10Blob_Release(b);   /* the reflector copied what it needs */
    {
        D3D11_SHADER_VARIABLE_DESC v; D3D11_SHADER_DESC d;
        ID3D11ShaderReflectionVariable *var = r->lpVtbl->GetVariableByName(r, "keep"); EXPECT_HR(var->lpVtbl->GetDesc(var, &v), S_OK); EXPECT_STR(v.Name, "keep");
        if (rb_desc(r, &d)) CHECK(d.Creator != NULL);
    }
    r->lpVtbl->Release(r);
}
TEST(rb_desc_two_reflectors_are_independent)
{
    RB_COMPILE(b, PS_SRC, "ps_5_0"); ID3D11ShaderReflection *a = rb_reflect(b), *c = rb_reflect(b);
    CHECK(a != c);
    if (a) a->lpVtbl->Release(a);
    if (c) { D3D11_SHADER_DESC d; if (rb_desc(c, &d)) EXPECT_EQ(d.Version, 0x50); c->lpVtbl->Release(c); }
    ID3D10Blob_Release(b);
}

/* ---- instruction statistics ------------------------------------------------------------------- */
TEST(rb_desc_statistics_basic_invariants)
{
    RB_REFLECT(r, "cbuffer CB { float4 a; float4 b; };\nTexture2D t : register(t0); SamplerState s : register(s0);\nfloat4 main(float2 uv : TEXCOORD0) : SV_Target { return (t.Sample(s, uv) + a) * b; }\n", "ps_5_0");
    D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) {
        CHECK(d.InstructionCount >= 3); CHECK(d.TempRegisterCount >= 1); EXPECT_EQ(d.DclCount, 2);   /* measured on Win10: the cbuffer declaration and the resource declarations are not counted */
        CHECK(d.FloatInstructionCount >= 2); EXPECT_EQ(d.TextureNormalInstructions, 1);
        EXPECT_EQ(d.TextureLoadInstructions, 0); EXPECT_EQ(d.TextureCompInstructions, 0); EXPECT_EQ(d.TextureBiasInstructions, 0); EXPECT_EQ(d.TextureGradientInstructions, 0);
        EXPECT_EQ(d.IntInstructionCount, 0); EXPECT_EQ(d.UintInstructionCount, 0);
        EXPECT_EQ(d.CutInstructionCount, 0); EXPECT_EQ(d.EmitInstructionCount, 0);
        CHECK(r->lpVtbl->GetMovInstructionCount(r) <= d.InstructionCount);
        CHECK(r->lpVtbl->GetMovcInstructionCount(r) <= d.InstructionCount);
        t_note("instr=%u temps=%u dcl=%u float=%u mov=%u", d.InstructionCount, d.TempRegisterCount, d.DclCount, d.FloatInstructionCount, r->lpVtbl->GetMovInstructionCount(r));
    }
    RB_DONE(r);
}
#define TEX_STAT(nm, expr, field) \
TEST(rb_desc_texture_counter_##nm) { \
    RB_REFLECT(r, "Texture2D t : register(t0); SamplerState s : register(s0); SamplerComparisonState c : register(s1);\nfloat4 main(float2 uv : TEXCOORD0, float bias : TEXCOORD1) : SV_Target { return " expr "; }\n", "ps_5_0"); \
    D3D11_SHADER_DESC d; \
    if (rb_desc(r, &d)) { EXPECT_EQ(d.field, 1); \
        UINT total = d.TextureNormalInstructions + d.TextureLoadInstructions + d.TextureCompInstructions + d.TextureBiasInstructions + d.TextureGradientInstructions; EXPECT_EQ(total, 1); } \
    RB_DONE(r); }
TEX_STAT(sample, "t.Sample(s, uv)", TextureNormalInstructions)
TEX_STAT(load, "t.Load(int3(uv * 8, 0))", TextureLoadInstructions)
TEX_STAT(compare, "t.SampleCmp(c, uv, bias)", TextureCompInstructions)
TEX_STAT(bias, "t.SampleBias(s, uv, bias)", TextureBiasInstructions)
TEX_STAT(gradient, "t.SampleGrad(s, uv, ddx(uv), ddy(uv))", TextureGradientInstructions)

TEST(rb_desc_int_uint_bitwise_conversion_counters)
{
    RB_REFLECT(r, "cbuffer CB { uint4 u; int4 i; float4 f; uint sh; };\nfloat4 main() : SV_Target { uint4 x = (u ^ (u << sh)) | (u >> sh); int4 y = i * i + i; return (float4)x + (float4)y + (float4)(int4)f; }\n", "ps_5_0");
    D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) {
        CHECK(d.UintInstructionCount >= 1); CHECK(d.IntInstructionCount >= 1);
        EXPECT_EQ(r->lpVtbl->GetBitwiseInstructionCount(r), 0); CHECK(r->lpVtbl->GetConversionInstructionCount(r) >= 1);   /* shifts/xor/or are counted as uint ALU, not as bitwise (measured: uint=3 int=2 bitwise=0 conv=3) */
        t_note("uint=%u int=%u bitwise=%u conv=%u", d.UintInstructionCount, d.IntInstructionCount, r->lpVtbl->GetBitwiseInstructionCount(r), r->lpVtbl->GetConversionInstructionCount(r));
    }
    RB_DONE(r);
}
TEST(rb_desc_movc_counter)
{
    RB_REFLECT(r, "cbuffer CB { float4 a; float4 b; uint c; };\nfloat4 main() : SV_Target { return c != 0 ? a : b; }\n", "ps_5_0");
    CHECK(r->lpVtbl->GetMovcInstructionCount(r) >= 1);
    RB_DONE(r);
}
TEST(rb_desc_dynamic_flow_control_counter)
{
    RB_REFLECT(r, "cbuffer CB { uint n; float4 a; };\nfloat4 main() : SV_Target { float4 s = 0; [loop] for (uint k = 0; k < n; k++) { s += a * k; } return s; }\n", "ps_5_0");
    D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) { CHECK(d.DynamicFlowControlCount >= 1); EXPECT_EQ(d.StaticFlowControlCount, 1);   /* the loop back edge counts as static flow control */ t_note("dynamic=%u static=%u", d.DynamicFlowControlCount, d.StaticFlowControlCount); }
    RB_DONE(r);
}
TEST(rb_desc_straight_line_shader_has_no_flow_control)
{
    RB_REFLECT(r, "cbuffer CB { float4 a; };\nfloat4 main() : SV_Target { return a * 2; }\n", "ps_5_0");
    D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.DynamicFlowControlCount, 0); EXPECT_EQ(d.StaticFlowControlCount, 1);   /* measured: the trailing ret is counted */ EXPECT_EQ(d.MacroInstructionCount, 0); EXPECT_EQ(d.ArrayInstructionCount, 0); EXPECT_EQ(d.TempArrayCount, 0); }
    RB_DONE(r);
}
TEST(rb_desc_instruction_count_grows_with_code)
{
    ID3DBlob *a, *b; ID3D11ShaderReflection *ra, *rb2; D3D11_SHADER_DESC da, db;
    if (!rb_compile_t("cbuffer CB { float4 v; };\nfloat4 main() : SV_Target { return v + 1; }\n", "ps_5_0", 0, &a)) return;
    if (!rb_compile_t("cbuffer CB { float4 v; };\nfloat4 main() : SV_Target { float4 x = v; x = sin(x) + cos(x * 2) + sqrt(abs(x)) + rcp(x + 3) + exp(x); return x * x + log(abs(x) + 1); }\n", "ps_5_0", 0, &b)) { ID3D10Blob_Release(a); return; }
    ra = rb_reflect(a); rb2 = rb_reflect(b);
    if (ra && rb2 && rb_desc(ra, &da) && rb_desc(rb2, &db)) { CHECK(db.InstructionCount > da.InstructionCount); CHECK(db.FloatInstructionCount > da.FloatInstructionCount); }
    if (ra) ra->lpVtbl->Release(ra);
    if (rb2) rb2->lpVtbl->Release(rb2);
    ID3D10Blob_Release(a); ID3D10Blob_Release(b);
}
TEST(rb_desc_temp_array_counters)
{
    RB_REFLECT(r, "cbuffer CB { uint i; };\nfloat4 main() : SV_Target { float4 arr[4] = { float4(1,0,0,0), float4(0,1,0,0), float4(0,0,1,0), float4(0,0,0,1) }; return arr[i & 3]; }\n", "ps_5_0");
    D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) t_note("TempArrayCount=%u ArrayInstructionCount=%u DefCount=%u", d.TempArrayCount, d.ArrayInstructionCount, d.DefCount);
    RB_DONE(r);
}

/* ---- bound resources ------------------------------------------------------------------------------- */
#define RES_CASE(nm, decl, use, rname, sit, dim, ret) \
TEST(rb_res_##nm) { \
    RB_REFLECT(r, decl "\nSamplerState smp : register(s0);\nfloat4 main(float2 uv : TEXCOORD0, uint i : TEXCOORD1) : SV_Target { return " use "; }\n", "ps_5_0"); \
    D3D11_SHADER_INPUT_BIND_DESC b; \
    if (rb_bind(r, rname, &b)) { EXPECT_STR(b.Name, rname); EXPECT_EQ(b.Type, sit); EXPECT_EQ(b.Dimension, dim); EXPECT_EQ(b.ReturnType, ret); EXPECT_EQ(b.BindPoint, 0); EXPECT_EQ(b.BindCount, 1); } \
    RB_DONE(r); }
RES_CASE(texture1d, "Texture1D<float4> tex : register(t0);", "tex.Load(int2(i, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE1D, D3D_RETURN_TYPE_FLOAT)
RES_CASE(texture1d_array, "Texture1DArray<float4> tex : register(t0);", "tex.Load(int3(i, 0, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE1DARRAY, D3D_RETURN_TYPE_FLOAT)
RES_CASE(texture2d, "Texture2D<float4> tex : register(t0);", "tex.Load(int3(i, 0, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE2D, D3D_RETURN_TYPE_FLOAT)
RES_CASE(texture2d_array, "Texture2DArray<float4> tex : register(t0);", "tex.Load(int4(i, 0, 0, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE2DARRAY, D3D_RETURN_TYPE_FLOAT)
RES_CASE(texture3d, "Texture3D<float4> tex : register(t0);", "tex.Load(int4(i, 0, 0, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE3D, D3D_RETURN_TYPE_FLOAT)
RES_CASE(texturecube, "TextureCube<float4> tex : register(t0);", "tex.Sample(smp, float3(uv, 1))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURECUBE, D3D_RETURN_TYPE_FLOAT)
RES_CASE(texturecube_array, "TextureCubeArray<float4> tex : register(t0);", "tex.Sample(smp, float4(uv, 1, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURECUBEARRAY, D3D_RETURN_TYPE_FLOAT)
RES_CASE(buffer_float4, "Buffer<float4> tex : register(t0);", "tex.Load(i)", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_BUFFER, D3D_RETURN_TYPE_FLOAT)
RES_CASE(buffer_uint, "Buffer<uint> tex : register(t0);", "tex.Load(i)", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_BUFFER, D3D_RETURN_TYPE_UINT)
RES_CASE(buffer_int, "Buffer<int> tex : register(t0);", "tex.Load(i)", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_BUFFER, D3D_RETURN_TYPE_SINT)
RES_CASE(texture2d_unorm, "Texture2D<unorm float4> tex : register(t0);", "tex.Load(int3(i, 0, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE2D, D3D_RETURN_TYPE_UNORM)
RES_CASE(texture2d_snorm, "Texture2D<snorm float4> tex : register(t0);", "tex.Load(int3(i, 0, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE2D, D3D_RETURN_TYPE_SNORM)
RES_CASE(texture2d_uint, "Texture2D<uint4> tex : register(t0);", "float4(tex.Load(int3(i, 0, 0)))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE2D, D3D_RETURN_TYPE_UINT)
RES_CASE(texture2d_sint, "Texture2D<int4> tex : register(t0);", "float4(tex.Load(int3(i, 0, 0)))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE2D, D3D_RETURN_TYPE_SINT)
RES_CASE(texture2d_float1, "Texture2D<float> tex : register(t0);", "tex.Load(int3(i, 0, 0))", "tex", D3D_SIT_TEXTURE, D3D_SRV_DIMENSION_TEXTURE2D, D3D_RETURN_TYPE_FLOAT)

TEST(rb_res_texture_component_flags)
{
    static const struct { const char *ty; UINT flags; } t[] = { { "float", 0 }, { "float2", D3D_SIF_TEXTURE_COMPONENT_0 }, { "float3", D3D_SIF_TEXTURE_COMPONENT_1 }, { "float4", D3D_SIF_TEXTURE_COMPONENTS } };
    for (int k = 0; k < 4; k++) {
        char src[256]; ID3DBlob *bl; ID3D11ShaderReflection *r; D3D11_SHADER_INPUT_BIND_DESC b;
        sprintf(src, "Texture2D<%s> tex : register(t0);\nfloat4 main(uint i : TEXCOORD0) : SV_Target { return tex.Load(int3(i, 0, 0)).xxxx; }\n", t[k].ty);
        if (!rb_compile_t(src, "ps_5_0", 0, &bl)) return;
        r = rb_reflect(bl);
        if (r) { if (rb_bind(r, "tex", &b)) t_check((b.uFlags & D3D_SIF_TEXTURE_COMPONENTS) == t[k].flags, __FILE__, __LINE__, "texture component flags", "%s got 0x%x want 0x%x", t[k].ty, b.uFlags & D3D_SIF_TEXTURE_COMPONENTS, t[k].flags); r->lpVtbl->Release(r); }
        ID3D10Blob_Release(bl);
    }
}
TEST(rb_res_multisample_texture)
{
    static const int counts[] = { 2, 4, 8 };
    for (int k = 0; k < 3; k++) {
        char src[320]; ID3DBlob *bl; ID3D11ShaderReflection *r; D3D11_SHADER_INPUT_BIND_DESC b;
        sprintf(src, "Texture2DMS<float4, %d> tex : register(t0);\nfloat4 main(uint i : TEXCOORD0) : SV_Target { return tex.Load(int2(i, 0), 0); }\n", counts[k]);
        if (!rb_compile_t(src, "ps_5_0", 0, &bl)) return;
        r = rb_reflect(bl);
        if (r) { if (rb_bind(r, "tex", &b)) { EXPECT_EQ(b.Type, D3D_SIT_TEXTURE); EXPECT_EQ(b.Dimension, D3D_SRV_DIMENSION_TEXTURE2DMS); t_check(b.NumSamples == (UINT)counts[k], __FILE__, __LINE__, "NumSamples", "got %u want %d", b.NumSamples, counts[k]); } r->lpVtbl->Release(r); }
        ID3D10Blob_Release(bl);
    }
}
TEST(rb_res_multisample_texture_array)
{
    RB_REFLECT(r, "Texture2DMSArray<float4, 4> tex : register(t0);\nfloat4 main(uint i : TEXCOORD0) : SV_Target { return tex.Load(int3(i, 0, 0), 1); }\n", "ps_5_0");
    D3D11_SHADER_INPUT_BIND_DESC b;
    if (rb_bind(r, "tex", &b)) { EXPECT_EQ(b.Dimension, D3D_SRV_DIMENSION_TEXTURE2DMSARRAY); EXPECT_EQ(b.NumSamples, 4); }
    RB_DONE(r);
}
TEST(rb_res_non_multisample_texture_numsamples)
{
    RB_REFLECT(r, "Texture2D tex : register(t0);\nfloat4 main(uint i : TEXCOORD0) : SV_Target { return tex.Load(int3(i, 0, 0)); }\n", "ps_5_0");
    D3D11_SHADER_INPUT_BIND_DESC b;
    if (rb_bind(r, "tex", &b)) EXPECT_EQ(b.NumSamples, 0xffffffffu);
    RB_DONE(r);
}
TEST(rb_res_structured_buffer_stride)
{
    RB_REFLECT(r, "struct P { float3 a; float b; int c; };\nStructuredBuffer<P> sb : register(t4);\nStructuredBuffer<float4> sf : register(t5);\nfloat4 main(uint i : TEXCOORD0) : SV_Target { return float4(sb[i].a, sb[i].b + sb[i].c) + sf[i]; }\n", "ps_5_0");
    D3D11_SHADER_INPUT_BIND_DESC b;
    if (rb_bind(r, "sb", &b)) { EXPECT_EQ(b.Type, D3D_SIT_STRUCTURED); EXPECT_EQ(b.Dimension, D3D_SRV_DIMENSION_BUFFER); EXPECT_EQ(b.NumSamples, 20); EXPECT_EQ(b.BindPoint, 4); }
    if (rb_bind(r, "sf", &b)) { EXPECT_EQ(b.Type, D3D_SIT_STRUCTURED); EXPECT_EQ(b.NumSamples, 16); EXPECT_EQ(b.BindPoint, 5); }
    RB_DONE(r);
}
TEST(rb_res_byteaddress_buffer)
{
    RB_REFLECT(r, "ByteAddressBuffer rb : register(t1);\nfloat4 main(uint i : TEXCOORD0) : SV_Target { return asfloat(rb.Load4(i * 16)); }\n", "ps_5_0");
    D3D11_SHADER_INPUT_BIND_DESC b;
    if (rb_bind(r, "rb", &b)) { EXPECT_EQ(b.Type, D3D_SIT_BYTEADDRESS); EXPECT_EQ(b.BindPoint, 1); t_note("ByteAddressBuffer: dim=%d ret=%d samples=%u flags=0x%x", b.Dimension, b.ReturnType, b.NumSamples, b.uFlags); }
    RB_DONE(r);
}
#define UAV_CASE(nm, decl, use, rname, sit, dim, ret) \
TEST(rb_res_uav_##nm) { \
    RB_REFLECT(r, decl "\nfloat4 main(uint i : TEXCOORD0) : SV_Target { " use " return 0; }\n", "ps_5_0"); \
    D3D11_SHADER_INPUT_BIND_DESC b; \
    if (rb_bind(r, rname, &b)) { EXPECT_EQ(b.Type, sit); EXPECT_EQ(b.Dimension, dim); EXPECT_EQ(b.ReturnType, ret); EXPECT_EQ(b.BindPoint, 1); EXPECT_EQ(b.BindCount, 1); } \
    RB_DONE(r); }
UAV_CASE(rwtexture2d, "RWTexture2D<float4> u : register(u1);", "u[uint2(i, 0)] = float4(1, 2, 3, 4);", "u", D3D_SIT_UAV_RWTYPED, D3D_SRV_DIMENSION_TEXTURE2D, D3D_RETURN_TYPE_FLOAT)
UAV_CASE(rwtexture1d, "RWTexture1D<float4> u : register(u1);", "u[i] = float4(1, 2, 3, 4);", "u", D3D_SIT_UAV_RWTYPED, D3D_SRV_DIMENSION_TEXTURE1D, D3D_RETURN_TYPE_FLOAT)
UAV_CASE(rwtexture3d, "RWTexture3D<float4> u : register(u1);", "u[uint3(i, 0, 0)] = float4(1, 2, 3, 4);", "u", D3D_SIT_UAV_RWTYPED, D3D_SRV_DIMENSION_TEXTURE3D, D3D_RETURN_TYPE_FLOAT)
UAV_CASE(rwtexture2d_array, "RWTexture2DArray<float4> u : register(u1);", "u[uint3(i, 0, 0)] = float4(1, 2, 3, 4);", "u", D3D_SIT_UAV_RWTYPED, D3D_SRV_DIMENSION_TEXTURE2DARRAY, D3D_RETURN_TYPE_FLOAT)
UAV_CASE(rwbuffer_uint, "RWBuffer<uint> u : register(u1);", "u[i] = 7;", "u", D3D_SIT_UAV_RWTYPED, D3D_SRV_DIMENSION_BUFFER, D3D_RETURN_TYPE_UINT)
UAV_CASE(rwbuffer_float, "RWBuffer<float> u : register(u1);", "u[i] = 7;", "u", D3D_SIT_UAV_RWTYPED, D3D_SRV_DIMENSION_BUFFER, D3D_RETURN_TYPE_FLOAT)
UAV_CASE(rwbuffer_sint, "RWBuffer<int> u : register(u1);", "u[i] = -7;", "u", D3D_SIT_UAV_RWTYPED, D3D_SRV_DIMENSION_BUFFER, D3D_RETURN_TYPE_SINT)
TEST(rb_res_uav_structured_variants)
{
    RB_REFLECT(r, "RWStructuredBuffer<float4> rws : register(u1);\nRWStructuredBuffer<float4> rwc : register(u2);\nAppendStructuredBuffer<float4> app : register(u3);\nConsumeStructuredBuffer<float4> con : register(u4);\nRWByteAddressBuffer rwb : register(u5);\n"
              "float4 main(uint i : TEXCOORD0) : SV_Target { rws[i] = 1; rwc[i] = 2; rwc.IncrementCounter(); app.Append(float4(1,2,3,4)); rwb.Store(i * 4, 9); return con.Consume(); }\n", "ps_5_0");
    D3D11_SHADER_INPUT_BIND_DESC b;
    if (rb_bind(r, "rws", &b)) { EXPECT_EQ(b.Type, D3D_SIT_UAV_RWSTRUCTURED); EXPECT_EQ(b.NumSamples, 16); EXPECT_EQ(b.Dimension, D3D_SRV_DIMENSION_BUFFER); }
    if (rb_bind(r, "rwc", &b)) EXPECT_EQ(b.Type, D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER);
    if (rb_bind(r, "app", &b)) EXPECT_EQ(b.Type, D3D_SIT_UAV_APPEND_STRUCTURED);
    if (rb_bind(r, "con", &b)) EXPECT_EQ(b.Type, D3D_SIT_UAV_CONSUME_STRUCTURED);
    if (rb_bind(r, "rwb", &b)) EXPECT_EQ(b.Type, D3D_SIT_UAV_RWBYTEADDRESS);
    RB_DONE(r);
}
TEST(rb_res_samplers)
{
    RB_REFLECT(r, "Texture2D tex : register(t0);\nSamplerState lin : register(s3);\nSamplerComparisonState cmp : register(s5);\nfloat4 main(float2 uv : TEXCOORD0) : SV_Target { return tex.Sample(lin, uv) + tex.SampleCmp(cmp, uv, 0.5); }\n", "ps_5_0");
    D3D11_SHADER_INPUT_BIND_DESC b; D3D11_SHADER_DESC d;
    if (rb_bind(r, "lin", &b)) { EXPECT_EQ(b.Type, D3D_SIT_SAMPLER); EXPECT_EQ(b.BindPoint, 3); EXPECT_EQ(b.BindCount, 1); EXPECT_EQ(b.Dimension, D3D_SRV_DIMENSION_UNKNOWN); EXPECT_EQ(b.ReturnType, 0); EXPECT_EQ(b.NumSamples, 0); CHECK(!(b.uFlags & D3D_SIF_COMPARISON_SAMPLER)); }
    if (rb_bind(r, "cmp", &b)) { EXPECT_EQ(b.Type, D3D_SIT_SAMPLER); EXPECT_EQ(b.BindPoint, 5); CHECK(b.uFlags & D3D_SIF_COMPARISON_SAMPLER); }
    if (rb_desc(r, &d)) EXPECT_EQ(d.BoundResources, 3);
    RB_DONE(r);
}
TEST(rb_res_register_assignment_and_count)
{
    RB_REFLECT(r, "Texture2D a : register(t7);\nTexture2D b : register(t2);\nSamplerState s : register(s1);\ncbuffer C : register(b4) { float4 c; };\n"
              "float4 main(float2 uv : TEXCOORD0) : SV_Target { return a.Sample(s, uv) + b.Sample(s, uv) + c; }\n", "ps_5_0");
    D3D11_SHADER_DESC d; D3D11_SHADER_INPUT_BIND_DESC x;
    if (rb_desc(r, &d)) EXPECT_EQ(d.BoundResources, 4);
    if (rb_bind(r, "a", &x)) EXPECT_EQ(x.BindPoint, 7);
    if (rb_bind(r, "b", &x)) EXPECT_EQ(x.BindPoint, 2);
    if (rb_bind(r, "s", &x)) EXPECT_EQ(x.BindPoint, 1);
    if (rb_bind(r, "C", &x)) { EXPECT_EQ(x.BindPoint, 4); EXPECT_EQ(x.Type, D3D_SIT_CBUFFER); }
    RB_DONE(r);
}
TEST(rb_res_by_index_matches_by_name)
{
    RB_REFLECT(r, "Texture2D a : register(t7);\nTexture2D b : register(t2);\nSamplerState s : register(s1);\ncbuffer C : register(b4) { float4 c; };\n"
              "float4 main(float2 uv : TEXCOORD0) : SV_Target { return a.Sample(s, uv) + b.Sample(s, uv) + c; }\n", "ps_5_0");
    D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) {
        for (UINT i = 0; i < d.BoundResources; i++) {
            D3D11_SHADER_INPUT_BIND_DESC x, y; memset(&x, 0, sizeof x); memset(&y, 0, sizeof y);
            EXPECT_HR(r->lpVtbl->GetResourceBindingDesc(r, i, &x), S_OK);
            if (x.Name && rb_bind(r, x.Name, &y)) {
                CHECK(!strcmp(x.Name, y.Name) && x.Type == y.Type && x.BindPoint == y.BindPoint && x.BindCount == y.BindCount && x.uFlags == y.uFlags && x.ReturnType == y.ReturnType && x.Dimension == y.Dimension && x.NumSamples == y.NumSamples);
            }
        }
    }
    RB_DONE(r);
}
TEST(rb_res_binding_errors)
{
    RB_REFLECT(r, "Texture2D a : register(t0);\nfloat4 main(uint i : TEXCOORD0) : SV_Target { return a.Load(int3(i, 0, 0)); }\n", "ps_5_0");
    D3D11_SHADER_DESC d; D3D11_SHADER_INPUT_BIND_DESC x;
    if (rb_desc(r, &d)) {
        EXPECT_HR(r->lpVtbl->GetResourceBindingDesc(r, d.BoundResources, &x), RB_E_INVALIDARG);
        EXPECT_HR(r->lpVtbl->GetResourceBindingDesc(r, 0xffffffffu, &x), RB_E_INVALIDARG);
        EXPECT_HR(r->lpVtbl->GetResourceBindingDesc(r, 0, NULL), RB_E_INVALIDARG);
    }
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "missing", &x), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "A", &x), RB_E_INVALIDARG);   /* case sensitive */
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "", &x), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, NULL, &x), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "a", NULL), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "a", &x), S_OK);
    RB_DONE(r);
}
TEST(rb_res_resource_array)
{
    RB_REFLECT(r, "Texture2D tex[4] : register(t2);\nSamplerState s : register(s0);\ncbuffer C { uint idx; };\nfloat4 main(float2 uv : TEXCOORD0) : SV_Target { return tex[0].Load(int3(uv * 8, 0)) + tex[1].Load(int3(uv * 8, 0)) + tex[2].Load(int3(uv * 8, 0)) + tex[3].Load(int3(uv * 8, 0)); }\n", "ps_5_0");
    D3D11_SHADER_INPUT_BIND_DESC b;
    { D3D11_SHADER_DESC sd; if (rb_desc(r, &sd)) for (UINT k = 0; k < sd.BoundResources; k++) { D3D11_SHADER_INPUT_BIND_DESC x; if (SUCCEEDED(r->lpVtbl->GetResourceBindingDesc(r, k, &x))) t_note("bound %u: %s type %d bp %u count %u", k, x.Name, x.Type, x.BindPoint, x.BindCount); } }
    /* Win10: a resource array indexed with literals is reported element by element (names "tex[0]".."tex[3]", count 1,
     * consecutive registers); the array name itself is not a binding */
    for (UINT k = 0; k < 4; k++) {
        char nm[16]; sprintf(nm, "tex[%u]", k);
        if (rb_bind(r, nm, &b)) { EXPECT_EQ(b.BindPoint, 2 + k); EXPECT_EQ(b.BindCount, 1); EXPECT_EQ(b.Type, D3D_SIT_TEXTURE); EXPECT_EQ(b.Dimension, D3D_SRV_DIMENSION_TEXTURE2D); }
    }
    EXPECT_HR(r->lpVtbl->GetResourceBindingDescByName(r, "tex", &b), RB_E_INVALIDARG);
    RB_DONE(r);
}
TEST(rb_res_no_resources_shader)
{
    RB_REFLECT(r, PS_SRC, "ps_5_0"); D3D11_SHADER_DESC d; D3D11_SHADER_INPUT_BIND_DESC x;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.BoundResources, 0); EXPECT_EQ(d.ConstantBuffers, 0); EXPECT_HR(r->lpVtbl->GetResourceBindingDesc(r, 0, &x), RB_E_INVALIDARG); }
    { ID3D11ShaderReflectionConstantBuffer *cb = r->lpVtbl->GetConstantBufferByIndex(r, 0); D3D11_SHADER_BUFFER_DESC bd; EXPECT_HR(cb->lpVtbl->GetDesc(cb, &bd), RB_E_FAIL); }
    RB_DONE(r);
}
TEST(rb_res_same_resource_across_target_versions)
{
    static const char *const t[] = { "ps_4_0", "ps_4_1", "ps_5_0" };
    for (int k = 0; k < 3; k++) {
        ID3DBlob *bl; ID3D11ShaderReflection *r; D3D11_SHADER_INPUT_BIND_DESC b;
        if (!rb_compile_t("Texture2D tex : register(t3);\nSamplerState s : register(s2);\nfloat4 main(float2 uv : TEXCOORD0) : SV_Target { return tex.Sample(s, uv); }\n", t[k], 0, &bl)) continue;
        r = rb_reflect(bl);
        if (r) {
            if (rb_bind(r, "tex", &b)) t_check(b.BindPoint == 3 && b.Dimension == D3D_SRV_DIMENSION_TEXTURE2D && b.Type == D3D_SIT_TEXTURE, __FILE__, __LINE__, "texture bind", "%s", t[k]);
            if (rb_bind(r, "s", &b)) t_check(b.BindPoint == 2 && b.Type == D3D_SIT_SAMPLER, __FILE__, __LINE__, "sampler bind", "%s", t[k]);
            r->lpVtbl->Release(r);
        }
        ID3D10Blob_Release(bl);
    }
}

/* ---- signatures ------------------------------------------------------------------------------------- */
static int rb_in(ID3D11ShaderReflection *r, UINT i, D3D11_SIGNATURE_PARAMETER_DESC *p)
{ HRESULT hr; memset(p, 0, sizeof *p); hr = r->lpVtbl->GetInputParameterDesc(r, i, p); t_check(SUCCEEDED(hr), __FILE__, __LINE__, "input param desc", "index %u: %s", i, d3dh_hr_name(hr)); return SUCCEEDED(hr); }
static int rb_out(ID3D11ShaderReflection *r, UINT i, D3D11_SIGNATURE_PARAMETER_DESC *p)
{ HRESULT hr; memset(p, 0, sizeof *p); hr = r->lpVtbl->GetOutputParameterDesc(r, i, p); t_check(SUCCEEDED(hr), __FILE__, __LINE__, "output param desc", "index %u: %s", i, d3dh_hr_name(hr)); return SUCCEEDED(hr); }

TEST(rb_sig_vertex_shader_inputs)
{
    RB_REFLECT(r, "struct VI { float4 p : POSITION; float2 uv : TEXCOORD3; int2 ii : BLENDINDICES1; uint4 u : COLOR; };\nfloat4 main(VI i) : SV_Position { return i.p + float4(i.uv, i.ii) + i.u; }\n", "vs_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.InputParameters, 4); EXPECT_EQ(d.OutputParameters, 1); }
    if (rb_in(r, 0, &p)) { EXPECT_STR(p.SemanticName, "POSITION"); EXPECT_EQ(p.SemanticIndex, 0); EXPECT_EQ(p.Register, 0); EXPECT_EQ(p.SystemValueType, D3D_NAME_UNDEFINED); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_FLOAT32); EXPECT_EQ(p.Mask, 0xf); EXPECT_EQ(p.Stream, 0); CHECK((p.ReadWriteMask & ~p.Mask) == 0); }
    if (rb_in(r, 1, &p)) { EXPECT_STR(p.SemanticName, "TEXCOORD"); EXPECT_EQ(p.SemanticIndex, 3); EXPECT_EQ(p.Register, 1); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_FLOAT32); EXPECT_EQ(p.Mask, 0x3); }
    if (rb_in(r, 2, &p)) { EXPECT_STR(p.SemanticName, "BLENDINDICES"); EXPECT_EQ(p.SemanticIndex, 1); EXPECT_EQ(p.Register, 2); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_SINT32); EXPECT_EQ(p.Mask, 0x3); }
    if (rb_in(r, 3, &p)) { EXPECT_STR(p.SemanticName, "COLOR"); EXPECT_EQ(p.Register, 3); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_UINT32); EXPECT_EQ(p.Mask, 0xf); }
    RB_DONE(r);
}
TEST(rb_sig_input_read_mask_is_subset_of_mask)
{
    RB_REFLECT(r, "struct VI { float4 a : A; float4 b : B; float4 c : C; };\nstruct VO { float4 p : SV_Position; float4 x : X; float2 y : Y; };\nVO main(VI i) { VO o; o.p = i.a; o.x = float4(i.b.x, i.b.y, 0, 1); o.y = i.c.zw; return o; }\n", "vs_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) {
        for (UINT k = 0; k < d.InputParameters; k++) if (rb_in(r, k, &p)) { CHECK((p.ReadWriteMask & ~p.Mask) == 0); t_note("in  %s%u mask=0x%x rwmask=0x%x", p.SemanticName, p.SemanticIndex, p.Mask, p.ReadWriteMask); }
        /* for outputs the raw mask names the components that are never written, so no subset relation is promised */
        for (UINT k = 0; k < d.OutputParameters; k++) if (rb_out(r, k, &p)) t_note("out %s%u mask=0x%x rwmask=0x%x", p.SemanticName, p.SemanticIndex, p.Mask, p.ReadWriteMask);
    }
    RB_DONE(r);
}
TEST(rb_sig_output_masks_follow_declared_width)
{
    RB_REFLECT(r, "struct VO { float4 p : SV_Position; float3 a : AAA; float2 b : BBB; float c : CCC; };\nVO main(float4 i : POSITION) { VO o; o.p = i; o.a = i.xyz; o.b = i.xy; o.c = i.w; return o; }\n", "vs_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) EXPECT_EQ(d.OutputParameters, 4);
    if (rb_out(r, 0, &p)) { EXPECT_STR(p.SemanticName, "SV_Position"); EXPECT_EQ(p.SystemValueType, D3D_NAME_POSITION); EXPECT_EQ(p.Mask, 0xf); EXPECT_EQ(p.Register, 0); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_FLOAT32); }
    if (rb_out(r, 1, &p)) { EXPECT_STR(p.SemanticName, "AAA"); EXPECT_EQ(p.Mask, 0x7); EXPECT_EQ(p.Register, 1); }
    if (rb_out(r, 2, &p)) { EXPECT_STR(p.SemanticName, "BBB"); EXPECT_EQ(p.Mask, 0x3); EXPECT_EQ(p.Register, 2); }
    if (rb_out(r, 3, &p)) { EXPECT_STR(p.SemanticName, "CCC"); EXPECT_EQ(p.Mask, 0x4); EXPECT_EQ(p.Register, 2); }   /* a lone float packs into the free z lane of the register BBB uses */
    RB_DONE(r);
}
TEST(rb_sig_semantic_index_split_from_name)
{
    RB_REFLECT(r, "struct VI { float4 a : TEXCOORD0; float4 b : TEXCOORD1; float4 c : TEXCOORD12; float4 d : COLOR7; };\nfloat4 main(VI i) : SV_Position { return i.a + i.b + i.c + i.d; }\n", "vs_5_0");
    static const struct { const char *n; UINT idx; } w[] = { { "TEXCOORD", 0 }, { "TEXCOORD", 1 }, { "TEXCOORD", 12 }, { "COLOR", 7 } };
    for (UINT k = 0; k < 4; k++) { D3D11_SIGNATURE_PARAMETER_DESC p; if (rb_in(r, k, &p)) { EXPECT_STR(p.SemanticName, w[k].n); EXPECT_EQ(p.SemanticIndex, w[k].idx); EXPECT_EQ(p.Register, k); } }
    RB_DONE(r);
}
TEST(rb_sig_pointers_stay_valid_and_stable)
{
    RB_REFLECT(r, "float4 main(float4 p : POSITION, float2 t : TEXCOORD0) : SV_Position { return p + float4(t, 0, 0); }\n", "vs_5_0");
    D3D11_SIGNATURE_PARAMETER_DESC a, b;
    if (rb_in(r, 0, &a) && rb_in(r, 0, &b)) CHECK(a.SemanticName == b.SemanticName);
    RB_DONE(r);
}
TEST(rb_sig_index_errors)
{
    RB_REFLECT(r, "float4 main(float4 p : POSITION) : SV_Position { return p; }\n", "vs_5_0");
    D3D11_SIGNATURE_PARAMETER_DESC p;
    EXPECT_HR(r->lpVtbl->GetInputParameterDesc(r, 1, &p), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetInputParameterDesc(r, 0xffffffffu, &p), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetInputParameterDesc(r, 0, NULL), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetOutputParameterDesc(r, 1, &p), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetOutputParameterDesc(r, 0, NULL), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetPatchConstantParameterDesc(r, 0, &p), RB_E_INVALIDARG);
    EXPECT_HR(r->lpVtbl->GetPatchConstantParameterDesc(r, 0, NULL), RB_E_INVALIDARG);
    RB_DONE(r);
}
TEST(rb_sig_empty_signatures)
{
    RB_REFLECT(r, "RWBuffer<uint> u : register(u0);\n[numthreads(1,1,1)] void main(uint3 t : SV_DispatchThreadID) { u[0] = t.x; }\n", "cs_5_0");
    D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.InputParameters, 0); EXPECT_EQ(d.OutputParameters, 0); EXPECT_EQ(d.PatchConstantParameters, 0); }
    { D3D11_SIGNATURE_PARAMETER_DESC p; EXPECT_HR(r->lpVtbl->GetInputParameterDesc(r, 0, &p), RB_E_INVALIDARG); }
    RB_DONE(r);
}
TEST(rb_sig_pixel_shader_inputs_and_target)
{
    RB_REFLECT(r, "struct PI { float4 p : SV_Position; float2 uv : TEXCOORD0; float3 n : NORMAL; };\nfloat4 main(PI i) : SV_Target { return float4(i.uv, i.n.x, i.p.w); }\n", "ps_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.InputParameters, 3); EXPECT_EQ(d.OutputParameters, 1); }
    if (rb_in(r, 0, &p)) { EXPECT_STR(p.SemanticName, "SV_Position"); EXPECT_EQ(p.SystemValueType, D3D_NAME_POSITION); EXPECT_EQ(p.Mask, 0xf); }
    if (rb_in(r, 1, &p)) { EXPECT_STR(p.SemanticName, "TEXCOORD"); EXPECT_EQ(p.SystemValueType, D3D_NAME_UNDEFINED); EXPECT_EQ(p.Mask, 0x3); }
    if (rb_in(r, 2, &p)) { EXPECT_STR(p.SemanticName, "NORMAL"); EXPECT_EQ(p.Mask, 0x7); }
    if (rb_out(r, 0, &p)) { EXPECT_STR(p.SemanticName, "SV_Target"); EXPECT_EQ(p.SemanticIndex, 0); EXPECT_EQ(p.SystemValueType, D3D_NAME_TARGET); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_FLOAT32); EXPECT_EQ(p.Mask, 0xf); EXPECT_EQ(p.Register, 0); }
    RB_DONE(r);
}
TEST(rb_sig_multiple_render_targets)
{
    RB_REFLECT(r, "struct PO { float4 a : SV_Target0; uint2 b : SV_Target1; int c : SV_Target3; float3 d : SV_Target2; };\nPO main() { PO o; o.a = 1; o.b = 2; o.c = 3; o.d = 4; return o; }\n", "ps_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) EXPECT_EQ(d.OutputParameters, 4);
    for (UINT k = 0; k < 4; k++) if (rb_out(r, k, &p)) { EXPECT_STR(p.SemanticName, "SV_Target"); EXPECT_EQ(p.SystemValueType, D3D_NAME_TARGET); EXPECT_EQ(p.SemanticIndex, p.Register); }
    { int seen[4] = { 0 }; for (UINT k = 0; k < 4; k++) if (rb_out(r, k, &p) && p.SemanticIndex < 4) {
        seen[p.SemanticIndex] = 1;
        switch (p.SemanticIndex) { case 0: EXPECT_EQ(p.Mask, 0xf); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_FLOAT32); break; case 1: EXPECT_EQ(p.Mask, 0x3); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_UINT32); break;
                  case 2: EXPECT_EQ(p.Mask, 0x7); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_FLOAT32); break; case 3: EXPECT_EQ(p.Mask, 0x1); EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_SINT32); break; } }
      CHECK(seen[0] && seen[1] && seen[2] && seen[3]); }
    RB_DONE(r);
}
#define SV_PS_IN(nm, decl, use, semname, semidx, sv, ctype, mask) \
TEST(rb_sig_ps_input_##nm) { \
    RB_REFLECT(r, "float4 main(" decl ") : SV_Target { return " use "; }\n", "ps_5_0"); \
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p; int found = 0; \
    if (rb_desc(r, &d)) for (UINT k = 0; k < d.InputParameters; k++) if (rb_in(r, k, &p) && !strcmp(p.SemanticName, semname) && p.SemanticIndex == (semidx)) { \
        found = 1; t_check(p.SystemValueType == (sv), __FILE__, __LINE__, "SystemValueType", "%s got %d want %d", semname, p.SystemValueType, (sv)); \
        t_check(p.ComponentType == (ctype), __FILE__, __LINE__, "ComponentType", "%s got %d", semname, p.ComponentType); t_check(p.Mask == (mask), __FILE__, __LINE__, "Mask", "%s got 0x%x", semname, p.Mask); } \
    CHECK(found); RB_DONE(r); }
SV_PS_IN(sv_position, "float4 p : SV_Position", "p", "SV_Position", 0, D3D_NAME_POSITION, D3D_REGISTER_COMPONENT_FLOAT32, 0xf)
SV_PS_IN(sv_isfrontface, "bool f : SV_IsFrontFace", "f ? 1 : 0", "SV_IsFrontFace", 0, D3D_NAME_IS_FRONT_FACE, D3D_REGISTER_COMPONENT_UINT32, 0x1)
SV_PS_IN(sv_primitiveid, "uint i : SV_PrimitiveID", "i", "SV_PrimitiveID", 0, D3D_NAME_PRIMITIVE_ID, D3D_REGISTER_COMPONENT_UINT32, 0x1)
SV_PS_IN(sv_sampleindex, "uint i : SV_SampleIndex", "i", "SV_SampleIndex", 0, D3D_NAME_SAMPLE_INDEX, D3D_REGISTER_COMPONENT_UINT32, 0x1)
SV_PS_IN(sv_rtarrayindex, "uint i : SV_RenderTargetArrayIndex", "i", "SV_RenderTargetArrayIndex", 0, D3D_NAME_RENDER_TARGET_ARRAY_INDEX, D3D_REGISTER_COMPONENT_UINT32, 0x1)
SV_PS_IN(sv_viewportindex, "uint i : SV_ViewportArrayIndex", "i", "SV_ViewportArrayIndex", 0, D3D_NAME_VIEWPORT_ARRAY_INDEX, D3D_REGISTER_COMPONENT_UINT32, 0x1)
SV_PS_IN(sv_clipdistance, "float c : SV_ClipDistance0", "c", "SV_ClipDistance", 0, D3D_NAME_CLIP_DISTANCE, D3D_REGISTER_COMPONENT_FLOAT32, 0x1)
SV_PS_IN(sv_culldistance, "float c : SV_CullDistance0", "c", "SV_CullDistance", 0, D3D_NAME_CULL_DISTANCE, D3D_REGISTER_COMPONENT_FLOAT32, 0x1)
SV_PS_IN(user_texcoord_is_undefined, "float4 t : TEXCOORD2", "t", "TEXCOORD", 2, D3D_NAME_UNDEFINED, D3D_REGISTER_COMPONENT_FLOAT32, 0xf)

#define SV_VS_IN(nm, decl, use, semname, sv, ctype, mask) \
TEST(rb_sig_vs_input_##nm) { \
    RB_REFLECT(r, "float4 main(" decl ") : SV_Position { return " use "; }\n", "vs_5_0"); \
    D3D11_SIGNATURE_PARAMETER_DESC p; \
    if (rb_in(r, 0, &p)) { EXPECT_STR(p.SemanticName, semname); EXPECT_EQ(p.SystemValueType, sv); EXPECT_EQ(p.ComponentType, ctype); EXPECT_EQ(p.Mask, mask); } \
    RB_DONE(r); }
SV_VS_IN(sv_vertexid, "uint v : SV_VertexID", "float4(v, 0, 0, 1)", "SV_VertexID", D3D_NAME_VERTEX_ID, D3D_REGISTER_COMPONENT_UINT32, 0x1)
SV_VS_IN(sv_instanceid, "uint v : SV_InstanceID", "float4(v, 0, 0, 1)", "SV_InstanceID", D3D_NAME_INSTANCE_ID, D3D_REGISTER_COMPONENT_UINT32, 0x1)

#define SV_PS_OUT(nm, rettype, semdecl, body, semname, sv, ctype, mask) \
TEST(rb_sig_ps_output_##nm) { \
    RB_REFLECT(r, rettype " main(linear noperspective centroid float4 p : SV_Position) : " semdecl " { " body " }\n", "ps_5_0"); \
    D3D11_SIGNATURE_PARAMETER_DESC p; \
    if (rb_out(r, 0, &p)) { EXPECT_STR(p.SemanticName, semname); EXPECT_EQ(p.SystemValueType, sv); EXPECT_EQ(p.ComponentType, ctype); EXPECT_EQ(p.Mask, mask); } \
    RB_DONE(r); }
SV_PS_OUT(depth, "float", "SV_Depth", "return p.z;", "SV_Depth", D3D_NAME_DEPTH, D3D_REGISTER_COMPONENT_FLOAT32, 0x1)
SV_PS_OUT(depth_ge, "float", "SV_DepthGreaterEqual", "return p.z;", "SV_DepthGreaterEqual", D3D_NAME_DEPTH_GREATER_EQUAL, D3D_REGISTER_COMPONENT_FLOAT32, 0x1)
SV_PS_OUT(depth_le, "float", "SV_DepthLessEqual", "return p.z;", "SV_DepthLessEqual", D3D_NAME_DEPTH_LESS_EQUAL, D3D_REGISTER_COMPONENT_FLOAT32, 0x1)
SV_PS_OUT(coverage, "uint", "SV_Coverage", "return 3;", "SV_Coverage", D3D_NAME_COVERAGE, D3D_REGISTER_COMPONENT_UINT32, 0x1)
SV_PS_OUT(target_float3, "float3", "SV_Target", "return p.xyz;", "SV_Target", D3D_NAME_TARGET, D3D_REGISTER_COMPONENT_FLOAT32, 0x7)
SV_PS_OUT(target_uint4, "uint4", "SV_Target", "return (uint4)p;", "SV_Target", D3D_NAME_TARGET, D3D_REGISTER_COMPONENT_UINT32, 0xf)
SV_PS_OUT(target_int2, "int2", "SV_Target", "return (int2)p.xy;", "SV_Target", D3D_NAME_TARGET, D3D_REGISTER_COMPONENT_SINT32, 0x3)
SV_PS_OUT(target_float1, "float", "SV_Target", "return p.x;", "SV_Target", D3D_NAME_TARGET, D3D_REGISTER_COMPONENT_FLOAT32, 0x1)

TEST(rb_sig_vs_output_clip_distance)
{
    RB_REFLECT(r, "struct VO { float4 p : SV_Position; float c0 : SV_ClipDistance0; float c1 : SV_ClipDistance1; };\nVO main(float4 i : POSITION) { VO o; o.p = i; o.c0 = i.x; o.c1 = i.y; return o; }\n", "vs_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p; int clips = 0;
    if (rb_desc(r, &d)) for (UINT k = 0; k < d.OutputParameters; k++) if (rb_out(r, k, &p) && p.SystemValueType == D3D_NAME_CLIP_DISTANCE) { clips++; EXPECT_STR(p.SemanticName, "SV_ClipDistance"); }
    EXPECT_EQ(clips, 2);
    RB_DONE(r);
}
TEST(rb_sig_interpolation_semantics_are_plain_user_names)
{
    RB_REFLECT(r, "struct PI { float4 p : SV_Position; nointerpolation float4 a : AAA; noperspective float2 b : BBB; centroid float2 c : CCC; };\nfloat4 main(PI i) : SV_Target { return i.a + float4(i.b, i.c); }\n", "ps_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) EXPECT_EQ(d.InputParameters, 4);
    if (rb_in(r, 1, &p)) { EXPECT_STR(p.SemanticName, "AAA"); EXPECT_EQ(p.SystemValueType, D3D_NAME_UNDEFINED); }
    if (rb_in(r, 3, &p)) { EXPECT_STR(p.SemanticName, "CCC"); EXPECT_EQ(p.Mask, 0x3); }
    RB_DONE(r);
}
TEST(rb_sig_gs_stream_field)
{
    RB_REFLECT(r, "struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };\n[maxvertexcount(2)] void main(point V i[1], inout PointStream<V> s) { s.Append(i[0]); s.Append(i[0]); }\n", "gs_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.InputParameters, 2); EXPECT_EQ(d.OutputParameters, 2); }
    if (rb_in(r, 0, &p)) EXPECT_EQ(p.Stream, 0);
    if (rb_out(r, 0, &p)) EXPECT_EQ(p.Stream, 0);
    RB_DONE(r);
}

/* ---- geometry / tessellation / compute description fields ---------------------------------------- */
#define GS_CASE(nm, ins, prim, outty, topo, maxv, body) \
TEST(rb_gs_##nm) { \
    RB_REFLECT(r, "struct V { float4 p : SV_Position; };\n[maxvertexcount(" #maxv ")] void main(" ins ", inout " outty "<V> s) { " body " }\n", "gs_5_0"); \
    D3D11_SHADER_DESC d; \
    if (rb_desc(r, &d)) { EXPECT_EQ(d.GSOutputTopology, topo); EXPECT_EQ(d.GSMaxOutputVertexCount, maxv); EXPECT_EQ(d.InputPrimitive, prim); } \
    EXPECT_EQ(r->lpVtbl->GetGSInputPrimitive(r), prim); \
    RB_DONE(r); }
GS_CASE(point_to_pointlist, "point V i[1]", D3D_PRIMITIVE_POINT, "PointStream", D3D_PRIMITIVE_TOPOLOGY_POINTLIST, 1, "s.Append(i[0]);")
GS_CASE(line_to_linestrip, "line V i[2]", D3D_PRIMITIVE_LINE, "LineStream", D3D_PRIMITIVE_TOPOLOGY_LINESTRIP, 2, "s.Append(i[0]); s.Append(i[1]);")
GS_CASE(triangle_to_trianglestrip, "triangle V i[3]", D3D_PRIMITIVE_TRIANGLE, "TriangleStream", D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 3, "s.Append(i[0]); s.Append(i[1]); s.Append(i[2]);")
GS_CASE(lineadj_input, "lineadj V i[4]", D3D_PRIMITIVE_LINE_ADJ, "PointStream", D3D_PRIMITIVE_TOPOLOGY_POINTLIST, 4, "s.Append(i[1]);")
GS_CASE(triangleadj_input, "triangleadj V i[6]", D3D_PRIMITIVE_TRIANGLE_ADJ, "LineStream", D3D_PRIMITIVE_TOPOLOGY_LINESTRIP, 6, "s.Append(i[0]); s.Append(i[2]);")
TEST(rb_gs_emit_and_cut_counters)
{
    RB_REFLECT(r, GS_SRC, "gs_5_0"); D3D11_SHADER_DESC d;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.EmitInstructionCount, 3); EXPECT_EQ(d.CutInstructionCount, 1); }
    RB_DONE(r);
}
TEST(rb_gs_instance_count)
{
    RB_REFLECT(r, "struct V { float4 p : SV_Position; };\n[instance(4)][maxvertexcount(1)] void main(point V i[1], uint id : SV_GSInstanceID, inout PointStream<V> s) { V o = i[0]; o.p.x += id; s.Append(o); }\n", "gs_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p; int found = 0;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.cGSInstanceCount, 4); for (UINT k = 0; k < d.InputParameters; k++) if (rb_in(r, k, &p) && (t_note("gs input %u: %s%u sv=%d mask=0x%x", k, p.SemanticName, p.SemanticIndex, p.SystemValueType, p.Mask), !strcmp(p.SemanticName, "SV_GSInstanceID"))) { found = 1; EXPECT_EQ(p.ComponentType, D3D_REGISTER_COMPONENT_UINT32); } }
    CHECK(!found);   /* measured: SV_GSInstanceID is not listed in the input signature (cGSInstanceCount carries it) */
    RB_DONE(r);
}
TEST(rb_gs_input_primitive_on_other_stages)
{
    RB_REFLECT(r, VS_SRC, "vs_5_0");
    EXPECT_EQ(r->lpVtbl->GetGSInputPrimitive(r), D3D_PRIMITIVE_UNDEFINED);
    { D3D11_SHADER_DESC d; if (rb_desc(r, &d)) { EXPECT_EQ(d.GSMaxOutputVertexCount, 0); EXPECT_EQ(d.cGSInstanceCount, 0); } }
    RB_DONE(r);
}
TEST(rb_hs_description_fields)
{
    RB_REFLECT(r, HS_SRC, "hs_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) {
        EXPECT_EQ(d.HSOutputPrimitive, D3D11_TESSELLATOR_OUTPUT_TRIANGLE_CW); EXPECT_EQ(d.HSPartitioning, D3D11_TESSELLATOR_PARTITIONING_INTEGER); EXPECT_EQ(d.TessellatorDomain, D3D11_TESSELLATOR_DOMAIN_TRI);
        EXPECT_EQ(d.cControlPoints, 3); EXPECT_EQ(d.PatchConstantParameters, 4); EXPECT_EQ(d.InputParameters >= 1, 1);
        for (UINT k = 0; k < d.PatchConstantParameters; k++) { memset(&p, 0, sizeof p); EXPECT_HR(r->lpVtbl->GetPatchConstantParameterDesc(r, k, &p), S_OK); t_note("pc %u: %s%u sv=%d reg=%u mask=0x%x", k, p.SemanticName, p.SemanticIndex, p.SystemValueType, p.Register, p.Mask); }
    }
    RB_DONE(r);
}
TEST(rb_hs_patch_constant_system_values)
{
    RB_REFLECT(r, HS_SRC, "hs_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p; int tess = 0, inside = 0;
    if (rb_desc(r, &d)) for (UINT k = 0; k < d.PatchConstantParameters; k++) { memset(&p, 0, sizeof p); if (SUCCEEDED(r->lpVtbl->GetPatchConstantParameterDesc(r, k, &p))) { tess += p.SystemValueType == D3D_NAME_FINAL_TRI_EDGE_TESSFACTOR; inside += p.SystemValueType == D3D_NAME_FINAL_TRI_INSIDE_TESSFACTOR; } }
    EXPECT_EQ(tess, 3); EXPECT_EQ(inside, 1);
    RB_DONE(r);
}
TEST(rb_hs_other_partitioning_and_domain)
{
    static const struct { const char *dom, *part, *topo; D3D11_TESSELLATOR_DOMAIN d; D3D11_TESSELLATOR_PARTITIONING p; D3D11_TESSELLATOR_OUTPUT_PRIMITIVE o; int nfac; } t[] = {
        { "quad", "fractional_even", "triangle_ccw", D3D11_TESSELLATOR_DOMAIN_QUAD, D3D11_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN, D3D11_TESSELLATOR_OUTPUT_TRIANGLE_CCW, 6 },
        { "isoline", "pow2", "line", D3D11_TESSELLATOR_DOMAIN_ISOLINE, D3D11_TESSELLATOR_PARTITIONING_POW2, D3D11_TESSELLATOR_OUTPUT_LINE, 2 },
        { "tri", "fractional_odd", "point", D3D11_TESSELLATOR_DOMAIN_TRI, D3D11_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD, D3D11_TESSELLATOR_OUTPUT_POINT, 4 },
    };
    for (int k = 0; k < 3; k++) {
        char src[1400]; ID3DBlob *bl; ID3D11ShaderReflection *r; D3D11_SHADER_DESC d;
        const char *edges = k == 0 ? "float e[4] : SV_TessFactor; float i[2] : SV_InsideTessFactor;" : k == 1 ? "float e[2] : SV_TessFactor;" : "float e[3] : SV_TessFactor; float i : SV_InsideTessFactor;";
        const char *init = k == 0 ? "o.e[0]=o.e[1]=o.e[2]=o.e[3]=1; o.i[0]=o.i[1]=1;" : k == 1 ? "o.e[0]=o.e[1]=1;" : "o.e[0]=o.e[1]=o.e[2]=1; o.i=1;";
        sprintf(src, "struct VO { float4 p : POSITION; };\nstruct HC { %s };\nHC pcf(InputPatch<VO, 4> ip) { HC o; %s return o; }\n"
                "[domain(\"%s\")][partitioning(\"%s\")][outputtopology(\"%s\")][outputcontrolpoints(4)][patchconstantfunc(\"pcf\")]\nVO main(InputPatch<VO, 4> ip, uint id : SV_OutputControlPointID) { return ip[id]; }\n", edges, init, t[k].dom, t[k].part, t[k].topo);
        if (!rb_compile_t(src, "hs_5_0", 0, &bl)) return;
        r = rb_reflect(bl);
        if (r) {
            if (rb_desc(r, &d)) { t_check(d.TessellatorDomain == t[k].d && d.HSPartitioning == t[k].p && d.HSOutputPrimitive == t[k].o, __FILE__, __LINE__, "hs fields", "case %d: dom %d part %d prim %d", k, d.TessellatorDomain, d.HSPartitioning, d.HSOutputPrimitive); t_check(d.cControlPoints == 4, __FILE__, __LINE__, "control points", "case %d: %u", k, d.cControlPoints); t_check(d.PatchConstantParameters == (UINT)t[k].nfac, __FILE__, __LINE__, "patch constants", "case %d: %u want %d", k, d.PatchConstantParameters, t[k].nfac); }
            r->lpVtbl->Release(r);
        }
        ID3D10Blob_Release(bl);
    }
}
TEST(rb_ds_description_fields)
{
    RB_REFLECT(r, DS_SRC, "ds_5_0");
    D3D11_SHADER_DESC d; D3D11_SIGNATURE_PARAMETER_DESC p;
    if (rb_desc(r, &d)) { EXPECT_EQ(d.TessellatorDomain, D3D11_TESSELLATOR_DOMAIN_TRI); EXPECT_EQ(d.cControlPoints, 3); EXPECT_EQ(d.PatchConstantParameters, 4); }
    { int loc = 0; if (rb_desc(r, &d)) for (UINT k = 0; k < d.InputParameters; k++) if (rb_in(r, k, &p) && p.SystemValueType == D3D_NAME_UNDEFINED && !strcmp(p.SemanticName, "SV_DomainLocation")) loc++; t_note("SV_DomainLocation as input element: %d", loc); }
    RB_DONE(r);
}
TEST(rb_cs_thread_group_size)
{
    static const UINT sz[][3] = { { 1, 1, 1 }, { 8, 4, 2 }, { 64, 1, 1 }, { 16, 16, 1 }, { 4, 4, 4 }, { 1024, 1, 1 }, { 32, 32, 1 }, { 1, 1, 64 }, { 1, 1024, 1 } };
    for (size_t k = 0; k < sizeof sz / sizeof *sz; k++) {
        char src[256]; ID3DBlob *bl; ID3D11ShaderReflection *r; UINT x = 7, y = 7, z = 7, total;
        sprintf(src, "RWBuffer<uint> u : register(u0);\n[numthreads(%u, %u, %u)] void main(uint3 t : SV_DispatchThreadID) { u[t.x] = t.y + t.z; }\n", sz[k][0], sz[k][1], sz[k][2]);
        if (!rb_compile_t(src, "cs_5_0", 0, &bl)) return;
        r = rb_reflect(bl);
        if (r) {
            total = r->lpVtbl->GetThreadGroupSize(r, &x, &y, &z);
            t_check(x == sz[k][0] && y == sz[k][1] && z == sz[k][2], __FILE__, __LINE__, "group size", "got %u %u %u want %u %u %u", x, y, z, sz[k][0], sz[k][1], sz[k][2]);
            t_check(total == sz[k][0] * sz[k][1] * sz[k][2], __FILE__, __LINE__, "total threads", "got %u", total);
            r->lpVtbl->Release(r);
        }
        ID3D10Blob_Release(bl);
    }
}
TEST(rb_cs_thread_group_size_null_outputs)
{
    RB_REFLECT(r, "RWBuffer<uint> u : register(u0);\n[numthreads(8, 2, 2)] void main(uint3 t : SV_DispatchThreadID) { u[t.x] = 1; }\n", "cs_5_0");
    UINT x = 0, y = 0, z = 0;
    EXPECT_EQ(r->lpVtbl->GetThreadGroupSize(r, NULL, NULL, NULL), 32);
    EXPECT_EQ(r->lpVtbl->GetThreadGroupSize(r, &x, NULL, NULL), 32); EXPECT_EQ(x, 8);
    EXPECT_EQ(r->lpVtbl->GetThreadGroupSize(r, NULL, &y, NULL), 32); EXPECT_EQ(y, 2);
    EXPECT_EQ(r->lpVtbl->GetThreadGroupSize(r, NULL, NULL, &z), 32); EXPECT_EQ(z, 2);
    RB_DONE(r);
}
TEST(rb_cs_thread_group_size_on_non_compute)
{
    RB_REFLECT(r, PS_SRC, "ps_5_0"); UINT x = 9, y = 9, z = 9;
    EXPECT_EQ(r->lpVtbl->GetThreadGroupSize(r, &x, &y, &z), 0); EXPECT_EQ(x, 0); EXPECT_EQ(y, 0); EXPECT_EQ(z, 0);
    RB_DONE(r);
}
TEST(rb_cs_cs_4_0_group_size)
{
    RB_REFLECT(r, "RWByteAddressBuffer u : register(u0);\n[numthreads(4, 2, 1)] void main(uint3 t : SV_DispatchThreadID) { u.Store(t.x * 4, t.y); }\n", "cs_4_0");
    UINT x = 0, y = 0, z = 0; EXPECT_EQ(r->lpVtbl->GetThreadGroupSize(r, &x, &y, &z), 8); EXPECT_EQ(x, 4); EXPECT_EQ(y, 2); EXPECT_EQ(z, 1);
    RB_DONE(r);
}

/* ---- feature level / requirements ------------------------------------------------------------------ */
#define FL_CASE(nm, target, src, want) \
TEST(rb_fl_min_level_##nm) { RB_REFLECT(r, src, target); D3D_FEATURE_LEVEL fl = (D3D_FEATURE_LEVEL)0x1234; HRESULT hr = r->lpVtbl->GetMinFeatureLevel(r, &fl); \
    t_check(hr == S_OK && fl == (want), __FILE__, __LINE__, "min level", "%s: hr=%s level=0x%x want 0x%x", target, d3dh_hr_name(hr), fl, (UINT)(want)); RB_DONE(r); }
FL_CASE(ps_4_0, "ps_4_0", PS_SRC, D3D_FEATURE_LEVEL_10_0)
FL_CASE(vs_4_0, "vs_4_0", VS_SRC, D3D_FEATURE_LEVEL_10_0)
FL_CASE(gs_4_0, "gs_4_0", GS_SRC, D3D_FEATURE_LEVEL_10_0)
FL_CASE(ps_4_1, "ps_4_1", PS_SRC, D3D_FEATURE_LEVEL_10_1)
FL_CASE(vs_4_1, "vs_4_1", VS_SRC, D3D_FEATURE_LEVEL_10_1)
FL_CASE(ps_5_0, "ps_5_0", PS_SRC, D3D_FEATURE_LEVEL_11_0)
FL_CASE(vs_5_0, "vs_5_0", VS_SRC, D3D_FEATURE_LEVEL_11_0)
FL_CASE(cs_5_0, "cs_5_0", CS_SRC, D3D_FEATURE_LEVEL_11_0)
FL_CASE(hs_5_0, "hs_5_0", HS_SRC, D3D_FEATURE_LEVEL_11_0)
FL_CASE(ds_5_0, "ds_5_0", DS_SRC, D3D_FEATURE_LEVEL_11_0)
TEST(rb_fl_null_pointer)
{
    RB_REFLECT(r, PS_SRC, "ps_5_0");
    EXPECT_HR(r->lpVtbl->GetMinFeatureLevel(r, NULL), RB_E_INVALIDARG);
    RB_DONE(r);
}
TEST(rb_fl_level9_targets)
{
    static const char *const t[] = { "ps_4_0_level_9_1", "ps_4_0_level_9_3", "vs_4_0_level_9_1", "vs_4_0_level_9_3" };
    for (int k = 0; k < 4; k++) {
        ID3DBlob *bl; ID3D11ShaderReflection *r; D3D_FEATURE_LEVEL fl = (D3D_FEATURE_LEVEL)0; HRESULT hr; D3D11_SHADER_DESC d;
        const char *src = k < 2 ? PS_SRC : VS_SRC;
        if (!rb_compile_t(src, t[k], 0, &bl)) continue;
        r = rb_reflect(bl);
        if (r) { hr = r->lpVtbl->GetMinFeatureLevel(r, &fl); t_note("%s: min level %s 0x%x", t[k], d3dh_hr_name(hr), fl); if (rb_desc(r, &d)) t_note("%s: Version=0x%x", t[k], d.Version); r->lpVtbl->Release(r); }
        ID3D10Blob_Release(bl);
    }
}
TEST(rb_req_plain_shaders_require_nothing)
{
    static const char *const t[] = { "ps_4_0", "ps_5_0" };
    for (int k = 0; k < 2; k++) {
        ID3DBlob *bl; ID3D11ShaderReflection *r;
        if (!rb_compile_t(PS_SRC, t[k], 0, &bl)) return;
        r = rb_reflect(bl);
        if (r) { UINT64 f = r->lpVtbl->GetRequiresFlags(r); t_check(f == 0, __FILE__, __LINE__, "requires flags", "%s: 0x%llx", t[k], (unsigned long long)f); r->lpVtbl->Release(r); }
        ID3D10Blob_Release(bl);
    }
}
TEST(rb_req_double_precision)
{
    RB_REFLECT(r, "cbuffer CB { double a; double b; };\nfloat4 main() : SV_Target { double c = a * b + a; return (float)c; }\n", "ps_5_0");
    UINT64 f = r->lpVtbl->GetRequiresFlags(r);
    CHECK(f & D3D_SHADER_REQUIRES_DOUBLES);
    RB_DONE(r);
}
TEST(rb_req_early_depth_stencil)
{
    RB_REFLECT(r, "RWBuffer<uint> u : register(u1);\n[earlydepthstencil] float4 main(float4 p : SV_Position) : SV_Target { u[0] = 1; return p; }\n", "ps_5_0");
    UINT64 f = r->lpVtbl->GetRequiresFlags(r);
    CHECK(f & D3D_SHADER_REQUIRES_EARLY_DEPTH_STENCIL);
    CHECK(!(f & D3D_SHADER_REQUIRES_DOUBLES));
    RB_DONE(r);
}
TEST(rb_req_uav_in_vertex_shader)
{
    ID3DBlob *bl = NULL; ID3D11ShaderReflection *r; HRESULT hr;
    if (!d3dh_D3DCompile()) { t_skip("no compiler"); return; }
    hr = d3dh_compile("RWBuffer<uint> u : register(u1);\nfloat4 main(float4 p : POSITION) : SV_Position { u[0] = 1; return p; }\n", "main", "vs_5_0", 0, &bl);
    if (FAILED(hr)) { t_note("vs_5_0 with a UAV does not compile: %s", d3dh_hr_name(hr)); return; }
    r = rb_reflect(bl);
    if (r) {
        UINT64 f = r->lpVtbl->GetRequiresFlags(r); D3D_FEATURE_LEVEL fl = 0;
        CHECK(f & D3D_SHADER_REQUIRES_UAVS_AT_EVERY_STAGE);
        EXPECT_HR(r->lpVtbl->GetMinFeatureLevel(r, &fl), S_OK); CHECK((int)fl >= (int)D3D_FEATURE_LEVEL_11_0);
        r->lpVtbl->Release(r);
    }
    ID3D10Blob_Release(bl);
}
TEST(rb_req_minimum_precision_flag_is_noted)
{
    RB_REFLECT(r, "cbuffer CB { min16float4 a; };\nfloat4 main() : SV_Target { return a * 2; }\n", "ps_5_0");
    t_note("min16float shader RequiresFlags = 0x%llx", (unsigned long long)r->lpVtbl->GetRequiresFlags(r));
    { D3D11_SIGNATURE_PARAMETER_DESC p; if (rb_out(r, 0, &p)) t_note("output MinPrecision = %d", p.MinPrecision); }
    RB_DONE(r);
}
TEST(rb_req_sample_frequency_shader)
{
    RB_REFLECT(r, "float4 main(uint s : SV_SampleIndex) : SV_Target { return float4(s, 0, 0, 1); }\n", "ps_5_0");
    CHECK(r->lpVtbl->IsSampleFrequencyShader(r));
    RB_DONE(r);
}
TEST(rb_req_not_sample_frequency_shader)
{
    RB_REFLECT(r, PS_SRC, "ps_5_0");
    CHECK(!r->lpVtbl->IsSampleFrequencyShader(r));
    RB_DONE(r);
}
TEST(rb_req_sample_modifier_makes_sample_frequency)
{
    RB_REFLECT(r, "float4 main(sample float2 uv : TEXCOORD0) : SV_Target { return float4(uv, 0, 1); }\n", "ps_5_0");
    t_note("IsSampleFrequencyShader with 'sample' interpolation = %d", r->lpVtbl->IsSampleFrequencyShader(r));
    RB_DONE(r);
}
TEST(rb_req_interface_slots_none)
{
    RB_REFLECT(r, "cbuffer CB { float4 a; };\nfloat4 main() : SV_Target { return a; }\n", "ps_5_0");
    EXPECT_EQ(r->lpVtbl->GetNumInterfaceSlots(r), 0);
    RB_DONE(r);
}
TEST(rb_req_interface_slots_with_class_linkage)
{
    RB_REFLECT(r, "interface IBase { float4 Get(); };\nclass CA : IBase { float4 c; float4 Get() { return c; } };\nclass CB2 : IBase { float4 c; float4 Get() { return c * 2; } };\n"
              "IBase g_iface;\nfloat4 main() : SV_Target { return g_iface.Get(); }\n", "ps_5_0");
    UINT n = r->lpVtbl->GetNumInterfaceSlots(r);
    EXPECT_EQ(n, 1);
    { D3D11_SHADER_VARIABLE_DESC v; if (rb_var_desc(r, "g_iface", &v)) CHECK(v.uFlags & D3D_SVF_INTERFACE_POINTER); }
    RB_DONE(r);
}
