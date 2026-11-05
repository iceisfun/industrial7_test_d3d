/* Shader creation (vs/gs/ps 4.0 and 4.1), input layouts, stream-output geometry shaders. */
#include "dd.h"

#define T(n) TEST(d3d10_device_shader_##n)
#define BLOB(var, src, target) ID3DBlob *var = dd_blob(src, target); if (!var) { t_skip("cannot compile " target); return; }
#define BP(b) d3dh_blob_ptr(b)
#define BS(b) d3dh_blob_size(b)

/* ---- vertex shader ---- */
T(vs_create_and_release)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    ID3D10VertexShader *vs = NULL;
    ULONG base = d3dh_refcount(dev);
    EXPECT_HR_OK(ID3D10Device_CreateVertexShader(dev, BP(b), BS(b), &vs));
    CHECK(vs != NULL);
    if (vs) {
        ID3D10Device *got = NULL; ID3D10VertexShader_GetDevice(vs, &got); CHECK(got == dev); SAFE_RELEASE(got);
        EXPECT_EQ(ID3D10VertexShader_Release(vs), 0);
    }
    EXPECT_EQ(d3dh_refcount(dev), base);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(vs_null_out_is_s_false)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, BP(b), BS(b), NULL), S_FALSE);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(vs_null_bytecode)
{
    DD_DEV(dev);
    ID3D10VertexShader *vs = (void *)1;
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, NULL, 0, &vs), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, NULL, 100, &vs), E_INVALIDARG);
    DD_END(dev);
}
T(vs_zero_length)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    ID3D10VertexShader *vs = (void *)1;
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, BP(b), 0, &vs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(vs_truncated_bytecode)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    ID3D10VertexShader *vs = (void *)1;
    SIZE_T n = BS(b);
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, BP(b), n - 1, &vs), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, BP(b), n / 2, &vs), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, BP(b), 4, &vs), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, BP(b), 1, &vs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(vs_garbage_bytecode)
{
    DD_DEV(dev);
    BYTE junk[256]; for (int i = 0; i < 256; i++) junk[i] = (BYTE)(i * 7 + 3);
    ID3D10VertexShader *vs = (void *)1;
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, junk, sizeof junk, &vs), E_INVALIDARG);
    memset(junk, 0, sizeof junk);
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, junk, sizeof junk, &vs), E_INVALIDARG);
    memcpy(junk, "DXBC", 4);
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, junk, sizeof junk, &vs), E_INVALIDARG);
    DD_END(dev);
}
T(vs_corrupt_checksum)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    SIZE_T n = BS(b);
    BYTE *copy = malloc(n);
    memcpy(copy, BP(b), n);
    copy[4] ^= 0xff; /* container checksum */
    ID3D10VertexShader *vs = (void *)1;
    HRESULT hr = ID3D10Device_CreateVertexShader(dev, copy, n, &vs);
    t_note("container checksum altered: %s", d3dh_hr_name(hr));
    if (SUCCEEDED(hr)) { ID3D10VertexShader_Release(vs); }
    vs = (void *)1;
    memcpy(copy, BP(b), n);
    copy[0] = 'X';
    hr = ID3D10Device_CreateVertexShader(dev, copy, n, &vs);
    EXPECT_HR(hr, E_INVALIDARG);
    free(copy);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(vs_trailing_bytes_observed)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    SIZE_T n = BS(b);
    BYTE *copy = calloc(1, n + 64);
    memcpy(copy, BP(b), n);
    ID3D10VertexShader *vs = NULL;
    HRESULT hr = ID3D10Device_CreateVertexShader(dev, copy, n + 64, &vs);
    t_note("bytecode length larger than the container: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(vs);
    free(copy);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(vs_wrong_type_ps)
{
    DD_DEV(dev);
    BLOB(b, DD_PS_RED, "ps_4_0");
    ID3D10VertexShader *vs = (void *)1;
    EXPECT_HR(ID3D10Device_CreateVertexShader(dev, BP(b), BS(b), &vs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(ps_wrong_type_vs)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    ID3D10PixelShader *ps = (void *)1; ID3D10GeometryShader *gs = (void *)1;
    EXPECT_HR(ID3D10Device_CreatePixelShader(dev, BP(b), BS(b), &ps), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateGeometryShader(dev, BP(b), BS(b), &gs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gs_wrong_type_ps)
{
    DD_DEV(dev);
    BLOB(b, DD_PS_RED, "ps_4_0");
    ID3D10GeometryShader *gs = (void *)1;
    EXPECT_HR(ID3D10Device_CreateGeometryShader(dev, BP(b), BS(b), &gs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(ps_create)
{
    DD_DEV(dev);
    BLOB(b, DD_PS_RED, "ps_4_0");
    ID3D10PixelShader *ps = NULL;
    EXPECT_HR(ID3D10Device_CreatePixelShader(dev, BP(b), BS(b), NULL), S_FALSE);
    EXPECT_HR_OK(ID3D10Device_CreatePixelShader(dev, BP(b), BS(b), &ps));
    if (ps) { EXPECT_EQ(ID3D10PixelShader_Release(ps), 0); }
    EXPECT_HR(ID3D10Device_CreatePixelShader(dev, NULL, 0, &ps), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gs_create)
{
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    ID3D10GeometryShader *gs = NULL;
    EXPECT_HR(ID3D10Device_CreateGeometryShader(dev, BP(b), BS(b), NULL), S_FALSE);
    EXPECT_HR_OK(ID3D10Device_CreateGeometryShader(dev, BP(b), BS(b), &gs));
    if (gs) EXPECT_EQ(ID3D10GeometryShader_Release(gs), 0);
    EXPECT_HR(ID3D10Device_CreateGeometryShader(dev, NULL, 0, &gs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(shaders_same_bytecode_distinct_objects)
{
    DD_DEV(dev);
    BLOB(b, DD_PS_RED, "ps_4_0");
    ID3D10PixelShader *a = NULL, *c = NULL;
    EXPECT_HR_OK(ID3D10Device_CreatePixelShader(dev, BP(b), BS(b), &a));
    EXPECT_HR_OK(ID3D10Device_CreatePixelShader(dev, BP(b), BS(b), &c));
    if (a && c) t_note("identical bytecode yields %s objects", a == c ? "the same" : "distinct");
    SAFE_RELEASE(a); SAFE_RELEASE(c);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(shader_qi_matrix)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    ID3D10VertexShader *vs = NULL; void *p;
    EXPECT_HR_OK(ID3D10Device_CreateVertexShader(dev, BP(b), BS(b), &vs));
    if (vs) {
        static const IID *yes[] = { &IID_IUnknown, &IID_ID3D10VertexShader, &IID_ID3D10DeviceChild };
        for (unsigned i = 0; i < 3; i++) { p = NULL; HRESULT hr = ID3D10VertexShader_QueryInterface(vs, yes[i], &p); t_check(hr == S_OK, __FILE__, __LINE__, "vs QI", "%s: %s", d3dh_guid_str(yes[i]), d3dh_hr_name(hr)); if (p) IUnknown_Release((IUnknown *)p); }
        static const IID *no[] = { &IID_ID3D10PixelShader, &IID_ID3D10GeometryShader, &IID_ID3D10Resource, &IID_ID3D10Device };
        for (unsigned i = 0; i < 4; i++) { p = (void *)1; HRESULT hr = ID3D10VertexShader_QueryInterface(vs, no[i], &p); t_check(hr == E_NOINTERFACE && !p, __FILE__, __LINE__, "vs QI neg", "%s: %s", d3dh_guid_str(no[i]), d3dh_hr_name(hr)); if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p); }
        ID3D10VertexShader_Release(vs);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(shader_pipeline_set_get)
{
    DD_DEV(dev);
    BLOB(bv, DD_VS_POS, "vs_4_0");
    BLOB(bp, DD_PS_RED, "ps_4_0");
    ID3D10VertexShader *vs = NULL, *gv = NULL; ID3D10PixelShader *ps = NULL, *gp = NULL;
    EXPECT_HR_OK(ID3D10Device_CreateVertexShader(dev, BP(bv), BS(bv), &vs));
    EXPECT_HR_OK(ID3D10Device_CreatePixelShader(dev, BP(bp), BS(bp), &ps));
    if (vs && ps) {
        ULONG rv = d3dh_refcount(vs), rp = d3dh_refcount(ps);
        ID3D10Device_VSSetShader(dev, vs); ID3D10Device_PSSetShader(dev, ps);
        CHECK(d3dh_refcount(vs) > rv); CHECK(d3dh_refcount(ps) > rp);
        ID3D10Device_VSGetShader(dev, &gv); ID3D10Device_PSGetShader(dev, &gp);
        CHECK(gv == vs); CHECK(gp == ps);
        SAFE_RELEASE(gv); SAFE_RELEASE(gp);
        ID3D10Device_VSSetShader(dev, NULL); ID3D10Device_PSSetShader(dev, NULL);
        EXPECT_EQ(d3dh_refcount(vs), rv); EXPECT_EQ(d3dh_refcount(ps), rp);
        ID3D10Device_VSGetShader(dev, &gv); CHECK(gv == NULL);
    }
    SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    ID3D10Blob_Release(bv); ID3D10Blob_Release(bp);
    DD_END(dev);
}
T(shader_clear_state_unbinds)
{
    DD_DEV(dev);
    BLOB(bv, DD_VS_POS, "vs_4_0");
    ID3D10VertexShader *vs = NULL, *g = (void *)1;
    EXPECT_HR_OK(ID3D10Device_CreateVertexShader(dev, BP(bv), BS(bv), &vs));
    if (vs) {
        ID3D10Device_VSSetShader(dev, vs);
        ID3D10Device_ClearState(dev);
        ID3D10Device_VSGetShader(dev, &g); CHECK(g == NULL);
        EXPECT_EQ(d3dh_refcount(vs), 1);
        ID3D10VertexShader_Release(vs);
    }
    ID3D10Blob_Release(bv);
    DD_END(dev);
}
T(shader_model_4_1_on_10_1)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_1);
    ID3DBlob *bv = dd_blob(DD_VS_POS, "vs_4_1"), *bp = dd_blob(DD_PS_RED, "ps_4_1");
    if (!bv || !bp) { t_skip("no sm4.1 compile"); ID3D10Device1_Release(dev); return; }
    ID3D10VertexShader *vs = NULL; ID3D10PixelShader *ps = NULL;
    EXPECT_HR_OK(ID3D10Device1_CreateVertexShader(dev, BP(bv), BS(bv), &vs));
    EXPECT_HR_OK(ID3D10Device1_CreatePixelShader(dev, BP(bp), BS(bp), &ps));
    SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    ID3D10Blob_Release(bv); ID3D10Blob_Release(bp);
    ID3D10Device1_Release(dev);
}
T(shader_model_4_1_on_10_0_observed)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_10_0);
    ID3DBlob *bv = dd_blob(DD_VS_POS, "vs_4_1");
    if (!bv) { t_skip("no sm4.1 compile"); ID3D10Device1_Release(dev); return; }
    ID3D10VertexShader *vs = NULL;
    HRESULT hr = ID3D10Device1_CreateVertexShader(dev, BP(bv), BS(bv), &vs);
    t_note("vs_4_1 on a 10_0 device: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(vs);
    ID3D10Blob_Release(bv);
    ID3D10Device1_Release(dev);
}
T(shader_model_5_0_rejected_observed)
{
    DD_DEV(dev);
    ID3DBlob *bv = dd_blob(DD_VS_POS, "vs_5_0");
    if (!bv) { t_skip("no sm5.0 compile"); DD_END(dev); return; }
    ID3D10VertexShader *vs = NULL;
    HRESULT hr = ID3D10Device_CreateVertexShader(dev, BP(bv), BS(bv), &vs);
    t_note("vs_5_0 on a D3D10 device: %s", d3dh_hr_name(hr));
    CHECK(FAILED(hr));
    SAFE_RELEASE(vs);
    ID3D10Blob_Release(bv);
    DD_END(dev);
}
T(shader_level9_profile_observed)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_9_1);
    ID3DBlob *bv = dd_blob(DD_VS_POS, "vs_4_0_level_9_1"), *bp = dd_blob(DD_PS_RED, "ps_4_0_level_9_1");
    if (bv && bp) {
        ID3D10VertexShader *vs = NULL; ID3D10PixelShader *ps = NULL;
        EXPECT_HR_OK(ID3D10Device1_CreateVertexShader(dev, BP(bv), BS(bv), &vs));
        EXPECT_HR_OK(ID3D10Device1_CreatePixelShader(dev, BP(bp), BS(bp), &ps));
        SAFE_RELEASE(vs); SAFE_RELEASE(ps);
    }
    if (bv) ID3D10Blob_Release(bv);
    if (bp) ID3D10Blob_Release(bp);
    ID3D10Device1_Release(dev);
}
T(shader_level9_gs_unsupported)
{
    DD_DEV1(dev, D3D10_FEATURE_LEVEL_9_1);
    ID3DBlob *bg = dd_blob(DD_GS_PASS, "gs_4_0");
    if (bg) {
        ID3D10GeometryShader *gs = (void *)1;
        HRESULT hr = ID3D10Device1_CreateGeometryShader(dev, BP(bg), BS(bg), &gs);
        t_note("gs_4_0 on a 9_1 device: %s", d3dh_hr_name(hr));
        CHECK(FAILED(hr));
        ID3D10Blob_Release(bg);
    }
    ID3D10Device1_Release(dev);
}

/* ---- input layouts ---- */
static ID3D10InputLayout *mk_il(ID3D10Device *dev, const D3D10_INPUT_ELEMENT_DESC *e, UINT n, ID3DBlob *b, HRESULT *hr_out)
{
    ID3D10InputLayout *il = NULL;
    HRESULT hr = ID3D10Device_CreateInputLayout(dev, e, n, BP(b), BS(b), &il);
    if (hr_out) *hr_out = hr;
    return il;
}
#define EL(name, idx, fmt, slot, off, cls, rate) { name, idx, fmt, slot, off, cls, rate }
T(il_create_single)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
    EXPECT_HR_OK(hr);
    if (il) { ID3D10Device *g = NULL; ID3D10InputLayout_GetDevice(il, &g); CHECK(g == dev); SAFE_RELEASE(g); EXPECT_EQ(ID3D10InputLayout_Release(il), 0); }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_null_out_is_s_false)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
    EXPECT_HR(ID3D10Device_CreateInputLayout(dev, e, 1, BP(b), BS(b), NULL), S_FALSE);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_null_elements)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    ID3D10InputLayout *il = (void *)1;
    EXPECT_HR(ID3D10Device_CreateInputLayout(dev, NULL, 1, BP(b), BS(b), &il), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateInputLayout(dev, NULL, 0, BP(b), BS(b), &il), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_zero_count)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
    ID3D10InputLayout *il = (void *)1;
    EXPECT_HR(ID3D10Device_CreateInputLayout(dev, e, 0, BP(b), BS(b), &il), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_null_or_bad_bytecode)
{
    DD_DEV(dev);
    D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
    ID3D10InputLayout *il = (void *)1; BYTE junk[64] = { 0 };
    EXPECT_HR(ID3D10Device_CreateInputLayout(dev, e, 1, NULL, 0, &il), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateInputLayout(dev, e, 1, junk, sizeof junk, &il), E_INVALIDARG);
    DD_END(dev);
}
T(il_missing_semantic)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS_COL, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
    EXPECT_HR(hr, E_INVALIDARG); CHECK(il == NULL);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_wrong_semantic_name)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = { EL("NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_wrong_semantic_index)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
    EXPECT_HR(hr, E_INVALIDARG);
    SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_semantic_name_case_insensitive)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = { EL("position", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
    EXPECT_HR_OK(hr);
    SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_extra_elements_allowed)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = {
        EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0),
        EL("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0),
        EL("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D10_INPUT_PER_VERTEX_DATA, 0),
    };
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 3, b, &hr);
    EXPECT_HR_OK(hr);
    SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_fewer_components_than_shader)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16_SNORM };
    for (unsigned i = 0; i < 6; i++) {
        D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, f[i], 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
        HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
        t_check(hr == S_OK, __FILE__, __LINE__, "format vs float4 input", "format %u: %s", f[i], d3dh_hr_name(hr));
        SAFE_RELEASE(il);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_non_ia_formats_rejected)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24G8_TYPELESS, 200, 0xffffffffu };
    for (unsigned i = 0; i < 7; i++) {
        D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, f[i], 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
        HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "bad IA format", "format %u: %s", f[i], d3dh_hr_name(hr));
        SAFE_RELEASE(il);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_int_format_vs_float_shader_observed)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    static const DXGI_FORMAT f[] = { DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_SINT, DXGI_FORMAT_R8G8B8A8_UINT };
    for (unsigned i = 0; i < 3; i++) {
        D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, f[i], 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
        HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
        t_note("integer format %u for a float input: %s", f[i], d3dh_hr_name(hr));
        SAFE_RELEASE(il);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_slot_range)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    for (UINT s = 0; s <= 17; s++) {
        D3D10_INPUT_ELEMENT_DESC e[] = { EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, s, 0, D3D10_INPUT_PER_VERTEX_DATA, 0) };
        HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 1, b, &hr);
        HRESULT want = s < 16 ? S_OK : E_INVALIDARG;
        t_check(hr == want, __FILE__, __LINE__, "slot", "slot %u: %s", s, d3dh_hr_name(hr));
        SAFE_RELEASE(il);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_element_count_limit)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[40];
    for (int i = 0; i < 40; i++) {
        D3D10_INPUT_ELEMENT_DESC x = EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0);
        if (i) { x.SemanticName = "TEXCOORD"; x.SemanticIndex = (UINT)(i - 1); x.AlignedByteOffset = D3D10_APPEND_ALIGNED_ELEMENT; }
        e[i] = x;
    }
    HRESULT hr; ID3D10InputLayout *il;
    il = mk_il(dev, e, 16, b, &hr); EXPECT_HR_OK(hr); SAFE_RELEASE(il);
    il = mk_il(dev, e, 17, b, &hr); t_note("17 elements: %s", d3dh_hr_name(hr)); SAFE_RELEASE(il);
    il = mk_il(dev, e, 40, b, &hr); t_note("40 elements: %s", d3dh_hr_name(hr)); CHECK(FAILED(hr)); SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_instance_data_step_rate)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e = EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_INSTANCE_DATA, 1);
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, &e, 1, b, &hr);
    EXPECT_HR_OK(hr); SAFE_RELEASE(il);
    e.InstanceDataStepRate = 7;
    il = mk_il(dev, &e, 1, b, &hr); EXPECT_HR_OK(hr); SAFE_RELEASE(il);
    e.InstanceDataStepRate = 0;
    il = mk_il(dev, &e, 1, b, &hr); t_note("per-instance with step rate 0: %s", d3dh_hr_name(hr)); SAFE_RELEASE(il);
    e.InputSlotClass = D3D10_INPUT_PER_VERTEX_DATA; e.InstanceDataStepRate = 1;
    il = mk_il(dev, &e, 1, b, &hr); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(il);
    e.InputSlotClass = (D3D10_INPUT_CLASSIFICATION)5; e.InstanceDataStepRate = 0;
    il = mk_il(dev, &e, 1, b, &hr); EXPECT_HR(hr, E_INVALIDARG); SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_append_aligned_offsets)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS_COL, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = {
        EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0),
        EL("COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0),
    };
    EXPECT_EQ(D3D10_APPEND_ALIGNED_ELEMENT, 0xffffffffu);
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 2, b, &hr);
    EXPECT_HR_OK(hr); SAFE_RELEASE(il);
    e[0].AlignedByteOffset = 0; e[1].AlignedByteOffset = 16;
    il = mk_il(dev, e, 2, b, &hr); EXPECT_HR_OK(hr); SAFE_RELEASE(il);
    DD_END(dev);
}
T(il_unaligned_offsets_observed)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    static const UINT offs[] = { 1, 2, 3, 5, 15, 2048, 4096 };
    for (unsigned i = 0; i < 7; i++) {
        D3D10_INPUT_ELEMENT_DESC e = EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offs[i], D3D10_INPUT_PER_VERTEX_DATA, 0);
        HRESULT hr; ID3D10InputLayout *il = mk_il(dev, &e, 1, b, &hr);
        t_note("AlignedByteOffset %u: %s", offs[i], d3dh_hr_name(hr));
        SAFE_RELEASE(il);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_overlapping_elements_observed)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS_COL, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = {
        EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0),
        EL("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D10_INPUT_PER_VERTEX_DATA, 0),
    };
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 2, b, &hr);
    t_note("overlapping elements: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_duplicate_semantic_observed)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e[] = {
        EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0),
        EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D10_INPUT_PER_VERTEX_DATA, 0),
    };
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, e, 2, b, &hr);
    t_note("duplicate semantic: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_semantic_null_name)
{
    /* a NULL semantic name pointer is a caller bug that D3D validates */
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e = EL(NULL, 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0);
    ID3D10InputLayout *il = (void *)1;
    HRESULT hr = ID3D10Device_CreateInputLayout(dev, &e, 1, BP(b), BS(b), &il);
    t_note("NULL semantic name: %s", d3dh_hr_name(hr));
    CHECK(FAILED(hr));
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_vertexid_only_shader)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_ID, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e = EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0);
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, &e, 1, b, &hr);
    t_note("layout with an unused element for a SV_VertexID-only shader: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(il);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_set_get_and_clear)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e = EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0);
    HRESULT hr; ID3D10InputLayout *il = mk_il(dev, &e, 1, b, &hr), *g = NULL;
    EXPECT_HR_OK(hr);
    if (il) {
        ULONG rc = d3dh_refcount(il);
        ID3D10Device_IASetInputLayout(dev, il);
        CHECK(d3dh_refcount(il) > rc);
        ID3D10Device_IAGetInputLayout(dev, &g); CHECK(g == il); SAFE_RELEASE(g);
        ID3D10Device_ClearState(dev);
        EXPECT_EQ(d3dh_refcount(il), rc);
        ID3D10Device_IAGetInputLayout(dev, &g); CHECK(g == NULL);
        ID3D10InputLayout_Release(il);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(il_uses_input_signature_of_any_vs_compatible)
{
    /* a layout created against one shader is accepted for a second shader with an identical input signature */
    DD_DEV(dev);
    BLOB(b1, DD_VS_POS, "vs_4_0");
    BLOB(b2, "float4 main(float4 q : POSITION) : SV_Position { return q * 2; }\n", "vs_4_0");
    D3D10_INPUT_ELEMENT_DESC e = EL("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0);
    HRESULT hr1, hr2; ID3D10InputLayout *a = mk_il(dev, &e, 1, b1, &hr1), *b = mk_il(dev, &e, 1, b2, &hr2);
    EXPECT_HR_OK(hr1); EXPECT_HR_OK(hr2);
    SAFE_RELEASE(a); SAFE_RELEASE(b);
    ID3D10Blob_Release(b1); ID3D10Blob_Release(b2);
    DD_END(dev);
}

/* ---- stream output ---- */
T(gso_valid_position)
{
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    D3D10_SO_DECLARATION_ENTRY d[] = { { "SV_Position", 0, 0, 4, 0 } };
    ID3D10GeometryShader *gs = NULL;
    HRESULT hr = ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, 16, &gs);
    EXPECT_HR_OK(hr);
    if (gs) EXPECT_EQ(ID3D10GeometryShader_Release(gs), 0);
    EXPECT_HR(ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, 16, NULL), S_FALSE);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_partial_components)
{
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    static const struct { BYTE start, count; UINT stride; } c[] = { { 0, 1, 4 }, { 0, 2, 8 }, { 0, 3, 12 }, { 1, 3, 12 }, { 2, 2, 8 }, { 3, 1, 4 } };
    for (unsigned i = 0; i < 6; i++) {
        D3D10_SO_DECLARATION_ENTRY d[] = { { "SV_Position", 0, c[i].start, c[i].count, 0 } };
        ID3D10GeometryShader *gs = NULL;
        HRESULT hr = ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, c[i].stride, &gs);
        t_check(hr == S_OK, __FILE__, __LINE__, "so components", "start %u count %u: %s", c[i].start, c[i].count, d3dh_hr_name(hr));
        SAFE_RELEASE(gs);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_invalid_component_ranges)
{
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    static const struct { BYTE start, count; } c[] = { { 0, 5 }, { 1, 4 }, { 3, 2 }, { 4, 1 }, { 255, 1 } };
    for (unsigned i = 0; i < 5; i++) {
        D3D10_SO_DECLARATION_ENTRY d[] = { { "SV_Position", 0, c[i].start, c[i].count, 0 } };
        ID3D10GeometryShader *gs = (void *)1;
        HRESULT hr = ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, 16, &gs);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "bad so range", "start %u count %u: %s", c[i].start, c[i].count, d3dh_hr_name(hr));
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_unknown_semantic)
{
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    D3D10_SO_DECLARATION_ENTRY d[] = { { "NORMAL", 0, 0, 4, 0 } };
    ID3D10GeometryShader *gs = (void *)1;
    EXPECT_HR(ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, 16, &gs), E_INVALIDARG);
    D3D10_SO_DECLARATION_ENTRY e[] = { { "SV_Position", 3, 0, 4, 0 } };
    EXPECT_HR(ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), e, 1, 16, &gs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_output_slot_range)
{
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    for (UINT s = 0; s <= 5; s++) {
        D3D10_SO_DECLARATION_ENTRY d[] = { { "SV_Position", 0, 0, 4, (BYTE)s } };
        ID3D10GeometryShader *gs = NULL;
        HRESULT hr = ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, 16, &gs);
        t_note("output slot %u: %s", s, d3dh_hr_name(hr));
        if (s == 0) CHECK(SUCCEEDED(hr));
        if (s >= 4) CHECK(FAILED(hr));
        SAFE_RELEASE(gs);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_null_decl_with_entries)
{
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    ID3D10GeometryShader *gs = (void *)1;
    EXPECT_HR(ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), NULL, 1, 16, &gs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_bad_bytecode)
{
    DD_DEV(dev);
    D3D10_SO_DECLARATION_ENTRY d[] = { { "SV_Position", 0, 0, 4, 0 } };
    ID3D10GeometryShader *gs = (void *)1; BYTE junk[64] = { 0 };
    EXPECT_HR(ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, NULL, 0, d, 1, 16, &gs), E_INVALIDARG);
    EXPECT_HR(ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, junk, sizeof junk, d, 1, 16, &gs), E_INVALIDARG);
    DD_END(dev);
}
T(gso_ps_bytecode_rejected)
{
    DD_DEV(dev);
    BLOB(b, DD_PS_RED, "ps_4_0");
    D3D10_SO_DECLARATION_ENTRY d[] = { { "SV_Position", 0, 0, 4, 0 } };
    ID3D10GeometryShader *gs = (void *)1;
    EXPECT_HR(ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, 16, &gs), E_INVALIDARG);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_vertex_shader_bytecode_observed)
{
    DD_DEV(dev);
    BLOB(b, DD_VS_POS, "vs_4_0");
    D3D10_SO_DECLARATION_ENTRY d[] = { { "SV_Position", 0, 0, 4, 0 } };
    ID3D10GeometryShader *gs = NULL;
    HRESULT hr = ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, 16, &gs);
    t_note("stream output from vertex shader bytecode: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(gs);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_stride_observed)
{
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    D3D10_SO_DECLARATION_ENTRY d[] = { { "SV_Position", 0, 0, 4, 0 } };
    static const UINT strides[] = { 0, 4, 12, 16, 20, 32, 2048, 4096 };
    for (unsigned i = 0; i < 8; i++) {
        ID3D10GeometryShader *gs = NULL;
        HRESULT hr = ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 1, strides[i], &gs);
        t_note("stride %u for a 16 byte vertex: %s", strides[i], d3dh_hr_name(hr));
        SAFE_RELEASE(gs);
    }
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_hole_entry_observed)
{
    /* an entry with an empty semantic name skips data in the stream */
    DD_DEV(dev);
    BLOB(b, DD_GS_PASS, "gs_4_0");
    D3D10_SO_DECLARATION_ENTRY d[] = { { NULL, 0, 0, 2, 0 }, { "SV_Position", 0, 0, 4, 0 } };
    ID3D10GeometryShader *gs = NULL;
    HRESULT hr = ID3D10Device_CreateGeometryShaderWithStreamOutput(dev, BP(b), BS(b), d, 2, 24, &gs);
    t_note("hole entry: %s", d3dh_hr_name(hr));
    SAFE_RELEASE(gs);
    ID3D10Blob_Release(b);
    DD_END(dev);
}
T(gso_set_get_targets)
{
    DD_DEV(dev);
    D3D10_BUFFER_DESC bd = { 1024, D3D10_USAGE_DEFAULT, D3D10_BIND_STREAM_OUTPUT, 0, 0 }; ID3D10Buffer *b = NULL, *g[4] = { 0 }; UINT off = 32, goff[4] = { 0 };
    EXPECT_HR_OK(ID3D10Device_CreateBuffer(dev, &bd, NULL, &b));
    if (b) {
        ID3D10Device_SOSetTargets(dev, 1, &b, &off);
        ID3D10Device_SOGetTargets(dev, 4, g, goff);
        CHECK(g[0] == b); CHECK(!g[1]); EXPECT_EQ(goff[0], 32);
        SAFE_RELEASE(g[0]);
        ID3D10Device_SOSetTargets(dev, 0, NULL, NULL);
        ID3D10Device_SOGetTargets(dev, 4, g, goff);
        CHECK(!g[0]);
        ID3D10Buffer_Release(b);
    }
    DD_END(dev);
}
