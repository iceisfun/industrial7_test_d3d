/* Buffer views: shader-resource and unordered-access view creation rules for buffers. */
#include "bufh.h"

#define VB D3D11_BIND_VERTEX_BUFFER
#define SR D3D11_BIND_SHADER_RESOURCE
#define UA D3D11_BIND_UNORDERED_ACCESS
#define DEF D3D11_USAGE_DEFAULT
#define RAW D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS
#define STRUCT D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
#define RES(b) ((ID3D11Resource *)(b))

static ID3D11Buffer *mkb(UINT bw, UINT bind, UINT misc, UINT stride)
{
    D3D11_BUFFER_DESC d = bufh_desc(bw, DEF, bind, 0, misc, stride);
    ID3D11Buffer *b = NULL;
    if (!bufh_init()) return NULL;
    if (FAILED(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b))) return NULL;
    return b;
}

static HRESULT srv(ID3D11Buffer *b, D3D11_SRV_DIMENSION dim, DXGI_FORMAT f, UINT first, UINT n, UINT flags, ID3D11ShaderResourceView **out)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC d;
    memset(&d, 0, sizeof d);
    d.Format = f; d.ViewDimension = dim;
    if (dim == D3D11_SRV_DIMENSION_BUFFEREX) { d.BufferEx.FirstElement = first; d.BufferEx.NumElements = n; d.BufferEx.Flags = flags; }
    else { d.Buffer.FirstElement = first; d.Buffer.NumElements = n; }
    return ID3D11Device_CreateShaderResourceView(g_dev, RES(b), &d, out);
}
static HRESULT uav(ID3D11Buffer *b, DXGI_FORMAT f, UINT first, UINT n, UINT flags, ID3D11UnorderedAccessView **out)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC d;
    memset(&d, 0, sizeof d);
    d.Format = f; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    d.Buffer.FirstElement = first; d.Buffer.NumElements = n; d.Buffer.Flags = flags;
    return ID3D11Device_CreateUnorderedAccessView(g_dev, RES(b), &d, out);
}

/* expected: S_OK -> created and released, E_INVALIDARG -> fails */
#define SRV_T(nm, bw, bind, misc, stride, dim, fmt, first, n, flags, want, needs11) \
TEST(d3d11_buffers_srv_##nm) { \
    ID3D11Buffer *b; ID3D11ShaderResourceView *v = NULL; HRESULT hr; \
    NEED_DEV(); if ((needs11) && g_fl < D3D_FEATURE_LEVEL_11_0) { t_skip("needs FL11"); return; } \
    b = mkb(bw, bind, misc, stride); CHECK(b != NULL); if (!b) return; \
    hr = srv(b, dim, fmt, first, n, flags, &v); EXPECT_HR(hr, want); \
    if (SUCCEEDED(hr)) CHECK(v != NULL); else CHECK(v == NULL); \
    SAFE_RELEASE(v); SAFE_RELEASE(b); }

#define BUF D3D11_SRV_DIMENSION_BUFFER
#define BUFEX D3D11_SRV_DIMENSION_BUFFEREX
SRV_T(typed_f32_full, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 0, 16, 0, S_OK, 0)
SRV_T(typed_f32_partial, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 4, 8, 0, S_OK, 0)
SRV_T(typed_f32_last, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 15, 1, 0, S_OK, 0)
SRV_T(typed_f32_overflow, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 0, 17, 0, E_INVALIDARG, 0)
SRV_T(typed_f32_first_overflow, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 8, 9, 0, E_INVALIDARG, 0)
SRV_T(typed_f32_first_past_end, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 16, 1, 0, E_INVALIDARG, 0)
SRV_T(typed_f32_first_huge, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 0xffffffffu, 2, 0, E_INVALIDARG, 0)
SRV_T(typed_f32_n0, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 0, 0, 0, E_INVALIDARG, 0)
SRV_T(typed_rgba32_full, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 4, 0, S_OK, 0)
SRV_T(typed_rgba32_overflow, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 5, 0, E_INVALIDARG, 0)
SRV_T(typed_rgba32_first1, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 3, 0, S_OK, 0)
SRV_T(typed_rgba8_full, 64, SR, 0, 0, BUF, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, 0, S_OK, 0)
SRV_T(typed_r8_full, 64, SR, 0, 0, BUF, DXGI_FORMAT_R8_UNORM, 0, 64, 0, S_OK, 0)
SRV_T(typed_r8_overflow, 64, SR, 0, 0, BUF, DXGI_FORMAT_R8_UNORM, 0, 65, 0, E_INVALIDARG, 0)
SRV_T(typed_r16_full, 64, SR, 0, 0, BUF, DXGI_FORMAT_R16_UINT, 0, 32, 0, S_OK, 0)
SRV_T(typed_rg32_full, 64, SR, 0, 0, BUF, DXGI_FORMAT_R32G32_FLOAT, 0, 8, 0, S_OK, 0)
SRV_T(typed_no_srv_bind, 64, VB, 0, 0, BUF, DXGI_FORMAT_R32_FLOAT, 0, 16, 0, E_INVALIDARG, 0)
SRV_T(typed_unknown_format, 64, SR, 0, 0, BUF, DXGI_FORMAT_UNKNOWN, 0, 16, 0, E_INVALIDARG, 0)
SRV_T(typed_dim_mismatch_tex2d, 64, SR, 0, 0, D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R32_FLOAT, 0, 1, 0, E_INVALIDARG, 0)
SRV_T(typed_dim_unknown, 64, SR, 0, 0, D3D11_SRV_DIMENSION_UNKNOWN, DXGI_FORMAT_R32_FLOAT, 0, 1, 0, E_INVALIDARG, 0)
SRV_T(raw_ok, 64, SR, RAW, 0, BUFEX, DXGI_FORMAT_R32_TYPELESS, 0, 16, D3D11_BUFFEREX_SRV_FLAG_RAW, S_OK, 1)
SRV_T(raw_partial, 64, SR, RAW, 0, BUFEX, DXGI_FORMAT_R32_TYPELESS, 2, 8, D3D11_BUFFEREX_SRV_FLAG_RAW, S_OK, 1)
SRV_T(raw_overflow, 64, SR, RAW, 0, BUFEX, DXGI_FORMAT_R32_TYPELESS, 0, 17, D3D11_BUFFEREX_SRV_FLAG_RAW, E_INVALIDARG, 1)
SRV_T(raw_on_nonraw_buffer, 64, SR, 0, 0, BUFEX, DXGI_FORMAT_R32_TYPELESS, 0, 16, D3D11_BUFFEREX_SRV_FLAG_RAW, E_INVALIDARG, 1)
SRV_T(raw_wrong_format, 64, SR, RAW, 0, BUFEX, DXGI_FORMAT_R32_FLOAT, 0, 16, D3D11_BUFFEREX_SRV_FLAG_RAW, E_INVALIDARG, 1)
SRV_T(raw_rgba_format, 64, SR, RAW, 0, BUFEX, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_BUFFEREX_SRV_FLAG_RAW, E_INVALIDARG, 1)
SRV_T(raw_unknown_format, 64, SR, RAW, 0, BUFEX, DXGI_FORMAT_UNKNOWN, 0, 16, D3D11_BUFFEREX_SRV_FLAG_RAW, E_INVALIDARG, 1)
SRV_T(struct_ok, 96, SR, STRUCT, 12, BUF, DXGI_FORMAT_UNKNOWN, 0, 8, 0, S_OK, 1)
SRV_T(struct_partial, 96, SR, STRUCT, 12, BUF, DXGI_FORMAT_UNKNOWN, 3, 5, 0, S_OK, 1)
SRV_T(struct_overflow, 96, SR, STRUCT, 12, BUF, DXGI_FORMAT_UNKNOWN, 0, 9, 0, E_INVALIDARG, 1)
SRV_T(struct_first_overflow, 96, SR, STRUCT, 12, BUF, DXGI_FORMAT_UNKNOWN, 7, 2, 0, E_INVALIDARG, 1)
SRV_T(struct_typed_format, 96, SR, STRUCT, 12, BUF, DXGI_FORMAT_R32_FLOAT, 0, 8, 0, E_INVALIDARG, 1)
SRV_T(struct_n0, 96, SR, STRUCT, 12, BUF, DXGI_FORMAT_UNKNOWN, 0, 0, 0, E_INVALIDARG, 1)
SRV_T(struct_bufex_raw, 96, SR, STRUCT, 12, BUFEX, DXGI_FORMAT_R32_TYPELESS, 0, 24, D3D11_BUFFEREX_SRV_FLAG_RAW, E_INVALIDARG, 1)
SRV_T(unknown_format_nonstruct, 64, SR, 0, 0, BUF, DXGI_FORMAT_UNKNOWN, 0, 4, 0, E_INVALIDARG, 0)

#define UAV_T(nm, bw, bind, misc, stride, fmt, first, n, flags, want) \
TEST(d3d11_buffers_uav_##nm) { \
    ID3D11Buffer *b; ID3D11UnorderedAccessView *v = NULL; HRESULT hr; \
    NEED_FL11(); \
    b = mkb(bw, bind, misc, stride); CHECK(b != NULL); if (!b) return; \
    hr = uav(b, fmt, first, n, flags, &v); EXPECT_HR(hr, want); \
    if (SUCCEEDED(hr)) CHECK(v != NULL); else CHECK(v == NULL); \
    SAFE_RELEASE(v); SAFE_RELEASE(b); }
UAV_T(typed_u32_full, 64, UA, 0, 0, DXGI_FORMAT_R32_UINT, 0, 16, 0, S_OK)
UAV_T(typed_u32_partial, 64, UA, 0, 0, DXGI_FORMAT_R32_UINT, 8, 8, 0, S_OK)
UAV_T(typed_f32_full, 64, UA, 0, 0, DXGI_FORMAT_R32_FLOAT, 0, 16, 0, S_OK)
UAV_T(typed_s32_full, 64, UA, 0, 0, DXGI_FORMAT_R32_SINT, 0, 16, 0, S_OK)
UAV_T(typed_overflow, 64, UA, 0, 0, DXGI_FORMAT_R32_UINT, 0, 17, 0, E_INVALIDARG)
UAV_T(typed_first_overflow, 64, UA, 0, 0, DXGI_FORMAT_R32_UINT, 10, 7, 0, E_INVALIDARG)
UAV_T(typed_n0, 64, UA, 0, 0, DXGI_FORMAT_R32_UINT, 0, 0, 0, E_INVALIDARG)
UAV_T(typed_no_uav_bind, 64, SR, 0, 0, DXGI_FORMAT_R32_UINT, 0, 16, 0, E_INVALIDARG)
UAV_T(typed_unknown_format, 64, UA, 0, 0, DXGI_FORMAT_UNKNOWN, 0, 16, 0, E_INVALIDARG)
UAV_T(raw_ok, 64, UA, RAW, 0, DXGI_FORMAT_R32_TYPELESS, 0, 16, D3D11_BUFFER_UAV_FLAG_RAW, S_OK)
UAV_T(raw_partial, 64, UA, RAW, 0, DXGI_FORMAT_R32_TYPELESS, 4, 4, D3D11_BUFFER_UAV_FLAG_RAW, S_OK)
UAV_T(raw_overflow, 64, UA, RAW, 0, DXGI_FORMAT_R32_TYPELESS, 0, 17, D3D11_BUFFER_UAV_FLAG_RAW, E_INVALIDARG)
UAV_T(raw_on_nonraw_buffer, 64, UA, 0, 0, DXGI_FORMAT_R32_TYPELESS, 0, 16, D3D11_BUFFER_UAV_FLAG_RAW, E_INVALIDARG)
UAV_T(raw_wrong_format, 64, UA, RAW, 0, DXGI_FORMAT_R32_UINT, 0, 16, D3D11_BUFFER_UAV_FLAG_RAW, E_INVALIDARG)
UAV_T(struct_ok, 96, UA, STRUCT, 12, DXGI_FORMAT_UNKNOWN, 0, 8, 0, S_OK)
UAV_T(struct_partial, 96, UA, STRUCT, 12, DXGI_FORMAT_UNKNOWN, 2, 6, 0, S_OK)
UAV_T(struct_overflow, 96, UA, STRUCT, 12, DXGI_FORMAT_UNKNOWN, 0, 9, 0, E_INVALIDARG)
UAV_T(struct_typed_format, 96, UA, STRUCT, 12, DXGI_FORMAT_R32_UINT, 0, 8, 0, E_INVALIDARG)
UAV_T(struct_append, 96, UA, STRUCT, 12, DXGI_FORMAT_UNKNOWN, 0, 8, D3D11_BUFFER_UAV_FLAG_APPEND, S_OK)
UAV_T(struct_counter, 96, UA, STRUCT, 12, DXGI_FORMAT_UNKNOWN, 0, 8, D3D11_BUFFER_UAV_FLAG_COUNTER, S_OK)
UAV_T(struct_append_counter, 96, UA, STRUCT, 12, DXGI_FORMAT_UNKNOWN, 0, 8, D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER, E_INVALIDARG)
UAV_T(typed_counter, 64, UA, 0, 0, DXGI_FORMAT_R32_UINT, 0, 16, D3D11_BUFFER_UAV_FLAG_COUNTER, E_INVALIDARG)
UAV_T(raw_counter, 64, UA, RAW, 0, DXGI_FORMAT_R32_TYPELESS, 0, 16, D3D11_BUFFER_UAV_FLAG_RAW | D3D11_BUFFER_UAV_FLAG_COUNTER, E_INVALIDARG)
UAV_T(raw_append, 64, UA, RAW, 0, DXGI_FORMAT_R32_TYPELESS, 0, 16, D3D11_BUFFER_UAV_FLAG_RAW | D3D11_BUFFER_UAV_FLAG_APPEND, E_INVALIDARG)

TEST(d3d11_buffers_srv_validate_only)
{
    ID3D11Buffer *b;
    D3D11_SHADER_RESOURCE_VIEW_DESC d;
    NEED_DEV();
    b = mkb(64, SR, 0, 0);
    CHECK(b != NULL);
    if (!b) return;
    memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_R32_FLOAT; d.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; d.Buffer.NumElements = 16;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(g_dev, RES(b), &d, NULL), S_FALSE);
    d.Buffer.NumElements = 17;
    EXPECT_HR(ID3D11Device_CreateShaderResourceView(g_dev, RES(b), &d, NULL), E_INVALIDARG);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_uav_validate_only)
{
    ID3D11Buffer *b;
    D3D11_UNORDERED_ACCESS_VIEW_DESC d;
    NEED_FL11();
    b = mkb(64, UA, 0, 0);
    CHECK(b != NULL);
    if (!b) return;
    memset(&d, 0, sizeof d);
    d.Format = DXGI_FORMAT_R32_UINT; d.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; d.Buffer.NumElements = 16;
    EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(g_dev, RES(b), &d, NULL), S_FALSE);
    d.Buffer.NumElements = 17;
    EXPECT_HR(ID3D11Device_CreateUnorderedAccessView(g_dev, RES(b), &d, NULL), E_INVALIDARG);
    SAFE_RELEASE(b);
}

TEST(d3d11_buffers_srv_getdesc_roundtrip)
{
    ID3D11Buffer *b;
    ID3D11ShaderResourceView *v = NULL;
    D3D11_SHADER_RESOURCE_VIEW_DESC g;
    NEED_DEV();
    b = mkb(64, SR, 0, 0);
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(srv(b, BUF, DXGI_FORMAT_R32_FLOAT, 3, 7, 0, &v), S_OK);
    if (v) {
        memset(&g, 0, sizeof g);
        ID3D11ShaderResourceView_GetDesc(v, &g);
        EXPECT_EQ(g.Format, DXGI_FORMAT_R32_FLOAT);
        EXPECT_EQ(g.Buffer.FirstElement, 3);
        EXPECT_EQ(g.Buffer.NumElements, 7);
        t_note("SRV ViewDimension reported as %d (BUFFER=%d BUFFEREX=%d)", g.ViewDimension, BUF, BUFEX);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_srv_getresource)
{
    ID3D11Buffer *b;
    ID3D11ShaderResourceView *v = NULL;
    ID3D11Resource *r = NULL;
    NEED_DEV();
    b = mkb(64, SR, 0, 0);
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(srv(b, BUF, DXGI_FORMAT_R32_FLOAT, 0, 16, 0, &v), S_OK);
    if (v) {
        ID3D11View_GetResource((ID3D11View *)v, &r);
        CHECK(r == RES(b));
        SAFE_RELEASE(r);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_srv_outlives_buffer_release)
{
    ID3D11Buffer *b;
    ID3D11ShaderResourceView *v = NULL;
    ID3D11Resource *r = NULL;
    NEED_DEV();
    b = mkb(64, SR, 0, 0);
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(srv(b, BUF, DXGI_FORMAT_R32_FLOAT, 0, 16, 0, &v), S_OK);
    t_note("buffer Release with a live view returned %lu", (unsigned long)D3DH_RELEASE_RC(b));
    b = NULL;
    if (v) {
        ID3D11View_GetResource((ID3D11View *)v, &r);
        CHECK(r != NULL);
        if (r) {
            D3D11_RESOURCE_DIMENSION dim = 99;
            ID3D11Resource_GetType(r, &dim);
            EXPECT_EQ(dim, D3D11_RESOURCE_DIMENSION_BUFFER);
            SAFE_RELEASE(r);
        }
    }
    SAFE_RELEASE(v);
}

TEST(d3d11_buffers_uav_getdesc_roundtrip)
{
    ID3D11Buffer *b;
    ID3D11UnorderedAccessView *v = NULL;
    D3D11_UNORDERED_ACCESS_VIEW_DESC g;
    NEED_FL11();
    b = mkb(96, UA, STRUCT, 12);
    CHECK(b != NULL);
    if (!b) return;
    EXPECT_HR(uav(b, DXGI_FORMAT_UNKNOWN, 1, 4, D3D11_BUFFER_UAV_FLAG_COUNTER, &v), S_OK);
    if (v) {
        memset(&g, 0, sizeof g);
        ID3D11UnorderedAccessView_GetDesc(v, &g);
        EXPECT_EQ(g.ViewDimension, D3D11_UAV_DIMENSION_BUFFER);
        EXPECT_EQ(g.Format, DXGI_FORMAT_UNKNOWN);
        EXPECT_EQ(g.Buffer.FirstElement, 1);
        EXPECT_EQ(g.Buffer.NumElements, 4);
        EXPECT_EQ(g.Buffer.Flags, D3D11_BUFFER_UAV_FLAG_COUNTER);
    }
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_srv_null_desc_note)
{
    ID3D11Buffer *b;
    ID3D11ShaderResourceView *v = NULL;
    HRESULT hr;
    NEED_DEV();
    b = mkb(64, SR, 0, 0);
    CHECK(b != NULL);
    if (!b) return;
    hr = ID3D11Device_CreateShaderResourceView(g_dev, RES(b), NULL, &v);
    t_note("SRV with NULL desc on a plain buffer: %s", bufh_hrn(hr));
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_srv_null_desc_structured_note)
{
    ID3D11Buffer *b;
    ID3D11ShaderResourceView *v = NULL;
    HRESULT hr;
    NEED_FL11();
    b = mkb(96, SR, STRUCT, 12);
    CHECK(b != NULL);
    if (!b) return;
    hr = ID3D11Device_CreateShaderResourceView(g_dev, RES(b), NULL, &v);
    t_note("SRV with NULL desc on a structured buffer: %s", bufh_hrn(hr));
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}

TEST(d3d11_buffers_srv_staging_buffer)
{
    D3D11_BUFFER_DESC d = bufh_desc(64, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0, 0);
    ID3D11Buffer *b = NULL;
    ID3D11ShaderResourceView *v = NULL;
    NEED_DEV();
    EXPECT_HR(ID3D11Device_CreateBuffer(g_dev, &d, NULL, &b), S_OK);
    if (!b) return;
    EXPECT_HR(srv(b, BUF, DXGI_FORMAT_R32_FLOAT, 0, 16, 0, &v), E_INVALIDARG);
    SAFE_RELEASE(v); SAFE_RELEASE(b);
}
