/* Input layout creation. */
#include "so_common.h"

static const char VS3[] =
    "struct VI { float4 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };\n"
    "float4 main(VI i) : SV_Position { return i.pos + float4(i.uv, 0, 0) + i.col; }\n";
static const char VS_SYSVAL[] =
    "struct VI { float4 pos : POSITION; uint vid : SV_VertexID; uint iid : SV_InstanceID; };\n"
    "float4 main(VI i) : SV_Position { return i.pos + (float)i.vid + (float)i.iid; }\n";
static const char VS_NOINPUT[] =
    "float4 main(uint v : SV_VertexID) : SV_Position { return float4((float)v, 0, 0, 1); }\n";
static const char VS_INDEXED[] =
    "float4 main(float4 a : TEXCOORD1, float4 b : TEXCOORD3, float4 c : POSITION2) : SV_Position { return a + b + c; }\n";
static const char VS_ONE[] =
    "float4 main(float4 p : POSITION) : SV_Position { return p; }\n";

static D3D11_INPUT_ELEMENT_DESC E(const char *name, UINT idx, DXGI_FORMAT fmt, UINT slot, UINT off, D3D11_INPUT_CLASSIFICATION cls, UINT rate)
{
    D3D11_INPUT_ELEMENT_DESC e;
    e.SemanticName = name; e.SemanticIndex = idx; e.Format = fmt; e.InputSlot = slot; e.AlignedByteOffset = off;
    e.InputSlotClass = cls; e.InstanceDataStepRate = rate;
    return e;
}
#define V D3D11_INPUT_PER_VERTEX_DATA
#define I D3D11_INPUT_PER_INSTANCE_DATA
#define APP D3D11_APPEND_ALIGNED_ELEMENT

static void std3(D3D11_INPUT_ELEMENT_DESC *e)
{
    e[0] = E("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, V, 0);
    e[1] = E("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, V, 0);
    e[2] = E("COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 24, V, 0);
}
static HRESULT cil(const D3D11_INPUT_ELEMENT_DESC *e, UINT n, ID3DBlob *b, ID3D11InputLayout **out)
{
    return ID3D11Device_CreateInputLayout(so_dev, e, n, d3dh_blob_ptr(b), d3dh_blob_size(b), out);
}
/* element and slot limit: 16 for feature levels 9.x and 10.0, 32 from 10.1 on */
static UINT il_limit(void) { return so_fl >= D3D_FEATURE_LEVEL_10_1 ? 32 : 16; }
#define NEED_VS(var, src) ID3DBlob *var; SO_NEED(); var = so_compile_vs(src); if (!var) { t_skip("shader compiler unavailable"); return; }

ST(il_valid_basic)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    ID3D11InputLayout *l = NULL;
    NEED_VS(b, VS3);
    std3(e);
    EXPECT_HR(cil(e, 3, b, NULL), S_FALSE);
    EXPECT_HR(cil(e, 3, b, &l), S_OK);
    CHECK(l != NULL);
    SAFE_RELEASE(l);
}
ST(il_null_elements)
{
    ID3D11InputLayout *l = SO_SENTINEL(ID3D11InputLayout);
    NEED_VS(b, VS3);
    EXPECT_HR(cil(NULL, 3, b, NULL), E_INVALIDARG);
    EXPECT_HR(cil(NULL, 0, b, NULL), E_INVALIDARG);
    EXPECT_HR(cil(NULL, 3, b, &l), E_INVALIDARG);
}
ST(il_null_bytecode)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    SO_NEED();
    std3(e);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, NULL, 0, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, NULL, 100, NULL), E_INVALIDARG);
}
ST(il_zero_length_bytecode)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, d3dh_blob_ptr(b), 0, NULL), E_INVALIDARG);
}
ST(il_bytecode_length_must_match_container_size)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    SIZE_T n;
    NEED_VS(b, VS3);
    std3(e);
    n = d3dh_blob_size(b);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, d3dh_blob_ptr(b), n, NULL), S_FALSE);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, d3dh_blob_ptr(b), n - 1, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, d3dh_blob_ptr(b), n + 1, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, d3dh_blob_ptr(b), n / 2, NULL), E_INVALIDARG);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, d3dh_blob_ptr(b), 32, NULL), E_INVALIDARG);
}
ST(il_garbage_bytecode)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    unsigned char zeros[128];
    SO_NEED();
    std3(e);
    memset(zeros, 0, sizeof zeros);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, zeros, sizeof zeros, NULL), E_INVALIDARG);
    memset(zeros, 0x5A, sizeof zeros);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, zeros, sizeof zeros, NULL), E_INVALIDARG);
}
ST(il_corrupt_fourcc_rejected)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    unsigned char *copy;
    SIZE_T n;
    HRESULT hr;
    NEED_VS(b, VS3);
    std3(e);
    n = d3dh_blob_size(b);
    copy = (unsigned char *)malloc(n);
    CHECK(copy != NULL);
    if (!copy) return;
    memcpy(copy, d3dh_blob_ptr(b), n);
    copy[0] = 'X';
    hr = ID3D11Device_CreateInputLayout(so_dev, e, 3, copy, n, NULL);
    EXPECT_HR(hr, E_INVALIDARG);   /* Win10 */
    free(copy);
}
ST(il_bytecode_alignment_irrelevant_copy)
{
    /* a byte-for-byte copy of the container at an arbitrary heap address behaves like the original */
    D3D11_INPUT_ELEMENT_DESC e[3];
    unsigned char *copy;
    SIZE_T n;
    NEED_VS(b, VS3);
    std3(e);
    n = d3dh_blob_size(b);
    copy = (unsigned char *)malloc(n + 1);
    CHECK(copy != NULL);
    if (!copy) return;
    memcpy(copy + 1, d3dh_blob_ptr(b), n);
    EXPECT_HR(ID3D11Device_CreateInputLayout(so_dev, e, 3, copy + 1, n, NULL), S_FALSE);
    free(copy);
}
ST(il_missing_element_for_signature_rejected)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    ID3D11InputLayout *l = SO_SENTINEL(ID3D11InputLayout);
    int drop;
    NEED_VS(b, VS3);
    for (drop = 0; drop < 3; drop++) {
        D3D11_INPUT_ELEMENT_DESC f[2];
        int i, n = 0;
        std3(e);
        for (i = 0; i < 3; i++) if (i != drop) f[n++] = e[i];
        EXPECT_HR(cil(f, 2, b, NULL), E_INVALIDARG);
        l = SO_SENTINEL(ID3D11InputLayout);
        EXPECT_HR(cil(f, 2, b, &l), E_INVALIDARG);
    }
}
ST(il_zero_elements_with_required_inputs_rejected)
{
    D3D11_INPUT_ELEMENT_DESC e[1];
    NEED_VS(b, VS3);
    e[0] = E("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, V, 0);
    EXPECT_HR(cil(e, 0, b, NULL), E_INVALIDARG);
}
ST(il_extra_elements_not_in_signature_accepted)
{
    D3D11_INPUT_ELEMENT_DESC e[5];
    NEED_VS(b, VS3);
    std3(e);
    e[3] = E("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, V, 0);
    e[4] = E("TANGENT", 3, DXGI_FORMAT_R16G16_FLOAT, 2, 0, V, 0);
    EXPECT_HR(cil(e, 5, b, NULL), S_FALSE);
}
ST(il_element_order_irrelevant)
{
    D3D11_INPUT_ELEMENT_DESC e[3], f[3];
    NEED_VS(b, VS3);
    std3(e);
    f[0] = e[2]; f[1] = e[0]; f[2] = e[1];
    EXPECT_HR(cil(f, 3, b, NULL), S_FALSE);
}
ST(il_semantic_name_case_insensitive)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[0].SemanticName = "position";
    e[1].SemanticName = "TexCoord";
    e[2].SemanticName = "color";
    EXPECT_HR(cil(e, 3, b, NULL), S_FALSE);
}
ST(il_semantic_index_must_match)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[1].SemanticIndex = 1;
    EXPECT_HR(cil(e, 3, b, NULL), E_INVALIDARG);
    std3(e);
    e[0].SemanticIndex = 5;
    EXPECT_HR(cil(e, 3, b, NULL), E_INVALIDARG);
}
ST(il_semantic_indices_in_signature_matched)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS_INDEXED);
    e[0] = E("TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, V, 0);
    e[1] = E("TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, V, 0);
    e[2] = E("POSITION", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, V, 0);
    EXPECT_HR(cil(e, 3, b, NULL), S_FALSE);
    e[1].SemanticIndex = 2;
    EXPECT_HR(cil(e, 3, b, NULL), E_INVALIDARG);
}
ST(il_semantic_name_null)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[1].SemanticName = NULL;
    EXPECT_HR(cil(e, 3, b, NULL), E_INVALIDARG);
}
ST(il_semantic_name_with_trailing_digit_rejected)
{
    static const char *names[] = { "TEXCOORD0", "POSITION1", "COLOR9", "A0", "X1234" };
    unsigned i;
    NEED_VS(b, VS3);
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        D3D11_INPUT_ELEMENT_DESC e[4];
        HRESULT hr;
        std3(e);
        e[3] = E(names[i], 0, DXGI_FORMAT_R32_FLOAT, 1, 0, V, 0);   /* extra element, signature still satisfied */
        hr = cil(e, 4, b, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "trailing digit", "name %s got %s", names[i], d3dh_hr_name(hr));
    }
}
ST(il_semantic_name_trailing_digit_in_middle_accepted)
{
    D3D11_INPUT_ELEMENT_DESC e[4];
    NEED_VS(b, VS3);
    std3(e);
    e[3] = E("TEX1COORD", 0, DXGI_FORMAT_R32_FLOAT, 1, 0, V, 0);
    EXPECT_HR(cil(e, 4, b, NULL), S_FALSE);
    e[3] = E("A_B", 0, DXGI_FORMAT_R32_FLOAT, 1, 0, V, 0);
    EXPECT_HR(cil(e, 4, b, NULL), S_FALSE);
}
ST(il_duplicate_semantic_and_index_rejected)
{
    D3D11_INPUT_ELEMENT_DESC e[4];
    NEED_VS(b, VS3);
    std3(e);
    e[3] = E("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, V, 0);
    EXPECT_HR(cil(e, 4, b, NULL), S_FALSE);
    e[3] = E("COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, V, 0);
    EXPECT_HR(cil(e, 4, b, NULL), E_INVALIDARG);
    e[3] = E("color", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, V, 0);
    EXPECT_HR(cil(e, 4, b, NULL), E_INVALIDARG);
    e[3] = E("COLOR", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, V, 0);
    EXPECT_HR(cil(e, 4, b, NULL), S_FALSE);
}
ST(il_same_extra_semantic_twice_rejected_even_if_unused)
{
    D3D11_INPUT_ELEMENT_DESC e[5];
    NEED_VS(b, VS3);
    std3(e);
    e[3] = E("NORMAL", 2, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, V, 0);
    e[4] = E("Normal", 2, DXGI_FORMAT_R32G32B32_FLOAT, 2, 0, V, 0);
    EXPECT_HR(cil(e, 5, b, NULL), E_INVALIDARG);
}

/* ---- count and slots --------------------------------------------------------- */
ST(il_element_count_limit)
{
    D3D11_INPUT_ELEMENT_DESC e[40];
    static char names[40][8];
    int i;
    UINT lim;
    NEED_VS(b, VS_ONE);
    lim = il_limit();
    for (i = 0; i < 40; i++) {
        _snprintf(names[i], sizeof names[i], "E%c%c", 'A' + i / 26, 'A' + i % 26);
        e[i] = E(names[i], 0, DXGI_FORMAT_R32_FLOAT, i % lim, 0, V, 0);
    }
    e[0] = E("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, V, 0);
    EXPECT_HR(cil(e, 1, b, NULL), S_FALSE);
    EXPECT_HR(cil(e, lim - 1, b, NULL), S_FALSE);
    EXPECT_HR(cil(e, lim, b, NULL), S_FALSE);
    EXPECT_HR(cil(e, lim + 1, b, NULL), E_INVALIDARG);
    EXPECT_HR(cil(e, 40, b, NULL), E_INVALIDARG);
}
ST(il_element_count_huge)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    /* count above the maximum is rejected before the array is read */
    EXPECT_HR(cil(e, 33, b, NULL), E_INVALIDARG);
    EXPECT_HR(cil(e, 1000, b, NULL), E_INVALIDARG);
    EXPECT_HR(cil(e, 0xFFFFFFFFu, b, NULL), E_INVALIDARG);
}
ST(il_input_slot_scan)
{
    UINT slot, lim;
    NEED_VS(b, VS3);
    lim = il_limit();
    for (slot = 0; slot < 40; slot++) {
        D3D11_INPUT_ELEMENT_DESC e[3];
        HRESULT hr;
        std3(e);
        e[0].InputSlot = e[1].InputSlot = e[2].InputSlot = slot;
        hr = cil(e, 3, b, NULL);
        t_check(hr == (slot < lim ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "slot", "slot %u got %s", slot, d3dh_hr_name(hr));
    }
}
ST(il_input_slot_huge_rejected)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[1].InputSlot = 0xFFFFFFFFu;
    EXPECT_HR(cil(e, 3, b, NULL), E_INVALIDARG);
    e[1].InputSlot = 0x80000000u;
    EXPECT_HR(cil(e, 3, b, NULL), E_INVALIDARG);
}
ST(il_each_element_in_its_own_slot)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[0].InputSlot = 0; e[1].InputSlot = 7; e[2].InputSlot = il_limit() - 1;
    e[1].AlignedByteOffset = 0; e[2].AlignedByteOffset = 0;
    EXPECT_HR(cil(e, 3, b, NULL), S_FALSE);
}
ST(il_slot_class_scan)
{
    int v;
    NEED_VS(b, VS3);
    for (v = -2; v <= 4; v++) {
        D3D11_INPUT_ELEMENT_DESC e[3];
        HRESULT hr;
        std3(e);
        e[0].InputSlotClass = e[1].InputSlotClass = e[2].InputSlotClass = (D3D11_INPUT_CLASSIFICATION)v;
        e[0].InstanceDataStepRate = e[1].InstanceDataStepRate = e[2].InstanceDataStepRate = (v == 1);
        hr = cil(e, 3, b, NULL);
        t_check(hr == ((v == 0 || v == 1) ? S_FALSE : E_INVALIDARG), __FILE__, __LINE__, "class", "class %d got %s", v, d3dh_hr_name(hr));
    }
}
ST(il_per_vertex_with_step_rate_rejected)
{
    UINT r;
    NEED_VS(b, VS3);
    for (r = 1; r < 4; r++) {
        D3D11_INPUT_ELEMENT_DESC e[3];
        HRESULT hr;
        std3(e);
        e[1].InstanceDataStepRate = r;
        hr = cil(e, 3, b, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "per-vertex step", "rate %u got %s", r, d3dh_hr_name(hr));
    }
}
ST(il_per_instance_step_rates)
{
    static const UINT rates[] = { 0, 1, 2, 7, 1000, 0x7FFFFFFF, 0xFFFFFFFF };
    unsigned i;
    NEED_VS(b, VS3);
    for (i = 0; i < sizeof rates / sizeof rates[0]; i++) {
        D3D11_INPUT_ELEMENT_DESC e[3];
        HRESULT hr;
        std3(e);
        e[0].InputSlotClass = e[1].InputSlotClass = e[2].InputSlotClass = I;
        e[0].InstanceDataStepRate = e[1].InstanceDataStepRate = e[2].InstanceDataStepRate = rates[i];
        hr = cil(e, 3, b, NULL);
        t_check(hr == S_FALSE, __FILE__, __LINE__, "instance step", "rate %u got %s", rates[i], d3dh_hr_name(hr));
    }
}
ST(il_mixed_class_in_same_slot_rejected)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[2].InputSlotClass = I;
    e[2].InstanceDataStepRate = 1;
    EXPECT_HR(cil(e, 3, b, NULL), E_INVALIDARG);
}
ST(il_mixed_class_in_different_slots_accepted)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[2].InputSlot = 1;
    e[2].AlignedByteOffset = 0;
    e[2].InputSlotClass = I;
    e[2].InstanceDataStepRate = 1;
    EXPECT_HR(cil(e, 3, b, NULL), S_FALSE);
}
ST(il_different_step_rate_in_same_slot_rejected)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[0].InputSlotClass = e[1].InputSlotClass = e[2].InputSlotClass = I;
    e[0].InstanceDataStepRate = e[1].InstanceDataStepRate = 2;
    e[2].InstanceDataStepRate = 3;
    EXPECT_HR(cil(e, 3, b, NULL), E_INVALIDARG);
    e[2].InstanceDataStepRate = 2;
    EXPECT_HR(cil(e, 3, b, NULL), S_FALSE);
}
ST(il_different_step_rate_in_different_slots_accepted)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[0].InputSlotClass = e[1].InputSlotClass = e[2].InputSlotClass = I;
    e[0].InstanceDataStepRate = 1;
    e[1].InstanceDataStepRate = 2; e[1].InputSlot = 1; e[1].AlignedByteOffset = 0;
    e[2].InstanceDataStepRate = 5; e[2].InputSlot = 2; e[2].AlignedByteOffset = 0;
    EXPECT_HR(cil(e, 3, b, NULL), S_FALSE);
}

/* ---- offsets and formats ------------------------------------------------------ */
ST(il_append_aligned_everywhere)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    NEED_VS(b, VS3);
    std3(e);
    e[0].AlignedByteOffset = e[1].AlignedByteOffset = e[2].AlignedByteOffset = APP;
    EXPECT_HR(cil(e, 3, b, NULL), S_FALSE);
}
ST(il_aligned_offsets_accepted)
{
    UINT off;
    NEED_VS(b, VS3);
    for (off = 0; off <= 1024; off += 4) {
        D3D11_INPUT_ELEMENT_DESC e[3];
        HRESULT hr;
        std3(e);
        e[0].AlignedByteOffset = off;
        e[1].AlignedByteOffset = off + 16;
        e[2].AlignedByteOffset = off + 24;
        hr = cil(e, 3, b, NULL);
        t_check(hr == S_FALSE, __FILE__, __LINE__, "offset", "offset %u got %s", off, d3dh_hr_name(hr));
    }
}
ST(il_unaligned_offset_32bit_components_rejected)
{
    static const DXGI_FORMAT fmts[] = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,
                                        DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32B32_SINT };
    unsigned i;
    UINT off;
    NEED_VS(b, VS_ONE);
    for (i = 0; i < sizeof fmts / sizeof fmts[0]; i++)
        for (off = 1; off < 16; off++) {
            D3D11_INPUT_ELEMENT_DESC e[1];
            HRESULT hr;
            e[0] = E("POSITION", 0, fmts[i], 0, off, V, 0);
            hr = cil(e, 1, b, NULL);
            t_check(hr == ((off % 4) ? E_INVALIDARG : S_FALSE), __FILE__, __LINE__, "offset", "format %d offset %u got %s", fmts[i], off, d3dh_hr_name(hr));
        }
}
ST(il_small_component_format_offsets_observation)
{
    UINT off;
    NEED_VS(b, VS_ONE);
    for (off = 1; off < 4; off++) {
        D3D11_INPUT_ELEMENT_DESC e[1];
        HRESULT h8, h16;
        e[0] = E("POSITION", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, off, V, 0);
        h8 = cil(e, 1, b, NULL);
        e[0] = E("POSITION", 0, DXGI_FORMAT_R16G16_FLOAT, 0, off, V, 0);
        h16 = cil(e, 1, b, NULL);
        /* Win10: a byte offset that is not a multiple of the component size is rejected for both formats */
        t_check(h8 == E_INVALIDARG, __FILE__, __LINE__, "R8G8B8A8_UNORM at unaligned offset", "off %u got %s", off, d3dh_hr_name(h8));
        t_check(h16 == E_INVALIDARG, __FILE__, __LINE__, "R16G16_FLOAT at unaligned offset", "off %u got %s", off, d3dh_hr_name(h16));
    }
}
ST(il_core_vertex_formats_accepted)
{
    static const DXGI_FORMAT fmts[] = {
        DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_SINT,
        DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_SINT,
        DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R16G16B16A16_SINT,
        DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_SINT,
        DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UINT, DXGI_FORMAT_R11G11B10_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_SINT,
        DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16_SINT,
        DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_SINT,
        DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R8G8_SINT,
        DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_SNORM, DXGI_FORMAT_R16_SINT,
        DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8_SNORM, DXGI_FORMAT_R8_SINT,
    };
    unsigned i;
    NEED_VS(b, VS_ONE);
    for (i = 0; i < sizeof fmts / sizeof fmts[0]; i++) {
        D3D11_INPUT_ELEMENT_DESC e[1];
        HRESULT hr;
        e[0] = E("POSITION", 0, fmts[i], 0, 0, V, 0);
        hr = cil(e, 1, b, NULL);
        t_check(hr == S_FALSE, __FILE__, __LINE__, "format", "format %d got %s", fmts[i], d3dh_hr_name(hr));
    }
}
ST(il_unusable_vertex_formats_rejected)
{
    static const DXGI_FORMAT fmts[] = {
        DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D16_UNORM, (DXGI_FORMAT)200, (DXGI_FORMAT)0x7FFFFFFF, (DXGI_FORMAT)0xFFFFFFFF,
    };
    unsigned i;
    NEED_VS(b, VS_ONE);
    for (i = 0; i < sizeof fmts / sizeof fmts[0]; i++) {
        D3D11_INPUT_ELEMENT_DESC e[1];
        HRESULT hr;
        e[0] = E("POSITION", 0, fmts[i], 0, 0, V, 0);
        hr = cil(e, 1, b, NULL);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "format", "format %u got %s", (unsigned)fmts[i], d3dh_hr_name(hr));
    }
}
ST(il_format_is_independent_of_shader_input_type)
{
    /* the runtime matches names and indices only; type compatibility is not validated at creation */
    D3D11_INPUT_ELEMENT_DESC e[1];
    HRESULT hr;
    NEED_VS(b, VS_ONE);
    e[0] = E("POSITION", 0, DXGI_FORMAT_R32_UINT, 0, 0, V, 0);
    hr = cil(e, 1, b, NULL);
    EXPECT_HR(hr, S_FALSE);   /* Win10: the element type is not checked against the shader input type */
    e[0] = E("POSITION", 0, DXGI_FORMAT_R32_FLOAT, 0, 0, V, 0);
    EXPECT_HR(cil(e, 1, b, NULL), S_FALSE);
    e[0] = E("POSITION", 0, DXGI_FORMAT_R8_UNORM, 0, 0, V, 0);
    EXPECT_HR(cil(e, 1, b, NULL), S_FALSE);
}

/* ---- system-value inputs ---------------------------------------------------------- */
ST(il_vertexid_instanceid_need_no_element)
{
    D3D11_INPUT_ELEMENT_DESC e[1];
    NEED_VS(b, VS_SYSVAL);
    e[0] = E("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, V, 0);
    EXPECT_HR(cil(e, 1, b, NULL), S_FALSE);
}
ST(il_vertexid_only_shader_with_unrelated_element)
{
    D3D11_INPUT_ELEMENT_DESC e[1];
    NEED_VS(b, VS_NOINPUT);
    e[0] = E("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, V, 0);
    EXPECT_HR(cil(e, 1, b, NULL), S_FALSE);
}
ST(il_vertexid_only_shader_with_zero_elements_observation)
{
    D3D11_INPUT_ELEMENT_DESC e[1];
    HRESULT hr;
    NEED_VS(b, VS_NOINPUT);
    e[0] = E("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, V, 0);
    hr = cil(e, 0, b, NULL);
    EXPECT_HR(hr, S_FALSE);   /* Win10: a shader that needs no vertex buffers accepts an empty element array */
}

/* ---- objects ------------------------------------------------------------------------ */
ST(il_create_twice_distinct_or_same_observation)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    ID3D11InputLayout *a = NULL, *b2 = NULL;
    NEED_VS(b, VS3);
    std3(e);
    EXPECT_HR(cil(e, 3, b, &a), S_OK);
    EXPECT_HR(cil(e, 3, b, &b2), S_OK);
    CHECK(a != NULL && b2 != NULL);
    CHECK(a != b2);   /* Win10: input layouts are not de-duplicated */
    SAFE_RELEASE(a); SAFE_RELEASE(b2);
}
ST(il_getdevice_returns_creating_device)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    ID3D11InputLayout *l = NULL;
    ID3D11Device *d = NULL;
    ULONG rc;
    NEED_VS(b, VS3);
    std3(e);
    EXPECT_HR(cil(e, 3, b, &l), S_OK);
    if (!l) return;
    rc = d3dh_refcount(so_dev);
    ID3D11InputLayout_GetDevice(l, &d);
    EXPECT_EQ(d, so_dev);
    EXPECT_EQ(d3dh_refcount(so_dev), rc + 1);
    SAFE_RELEASE(d);
    SAFE_RELEASE(l);
}
ST(il_qi_devicechild_and_unknown)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    ID3D11InputLayout *l = NULL;
    ID3D11DeviceChild *c = NULL;
    IUnknown *u = NULL;
    ID3D11BlendState *bad = NULL;
    NEED_VS(b, VS3);
    std3(e);
    EXPECT_HR(cil(e, 3, b, &l), S_OK);
    if (!l) return;
    EXPECT_HR(ID3D11InputLayout_QueryInterface(l, &IID_ID3D11DeviceChild, (void **)&c), S_OK);
    EXPECT_HR(ID3D11InputLayout_QueryInterface(l, &IID_IUnknown, (void **)&u), S_OK);
    EXPECT_HR(ID3D11InputLayout_QueryInterface(l, &IID_ID3D11BlendState, (void **)&bad), E_NOINTERFACE);
    CHECK(bad == NULL);
    SAFE_RELEASE(c); SAFE_RELEASE(u); SAFE_RELEASE(l);
}
ST(il_survives_shader_blob_release)
{
    /* the layout copies what it needs: freeing the bytecode afterwards must not matter */
    D3D11_INPUT_ELEMENT_DESC e[3];
    ID3D11InputLayout *l = NULL;
    ID3DBlob *tmp = NULL;
    SO_NEED();
    std3(e);
    if (FAILED(d3dh_compile(VS3, "main", "vs_5_0", 0, &tmp))) { t_skip("shader compiler unavailable"); return; }
    EXPECT_HR(cil(e, 3, tmp, &l), S_OK);
    ID3D10Blob_Release(tmp);
    if (l) {
        ID3D11DeviceContext_IASetInputLayout(so_ctx, l);
        ID3D11DeviceContext_IASetInputLayout(so_ctx, NULL);
        SAFE_RELEASE(l);
    }
}
ST(il_set_and_get_on_context)
{
    D3D11_INPUT_ELEMENT_DESC e[3];
    ID3D11InputLayout *l = NULL, *g = NULL;
    ULONG rc;
    NEED_VS(b, VS3);
    std3(e);
    EXPECT_HR(cil(e, 3, b, &l), S_OK);
    if (!l) return;
    ID3D11DeviceContext_IASetInputLayout(so_ctx, l);
    rc = d3dh_refcount(l);
    ID3D11DeviceContext_IAGetInputLayout(so_ctx, &g);
    EXPECT_EQ(g, l);
    EXPECT_EQ(d3dh_refcount(l), rc + 1);
    SAFE_RELEASE(g);
    ID3D11DeviceContext_IASetInputLayout(so_ctx, NULL);
    ID3D11DeviceContext_IAGetInputLayout(so_ctx, &g);
    CHECK(g == NULL);
    SAFE_RELEASE(l);
}
